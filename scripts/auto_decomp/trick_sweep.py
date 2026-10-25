#!/usr/bin/env python3
"""Mechanical trick sweep over near-miss bodies (before spending AI time).

For each pending INCLUDE_ASM function with a saved bodies/<u>/<t>/<n>.best.c
scoring >= --min-score with <= --max-diff differing words, greedily hill-climb
over a fixed catalogue of source transformations (WORKER_LESSONS.md and the
decomp-match-fixes memory):

  decl order (adjacent swaps, reverse), local type flips (int/unsigned/
  long long, char/short signedness), volatile local, swap two local names,
  adjacent statement swap, commutative operand swap, (int) cast on an addend,
  `x = (a) op b` <-> `x = a; x op= b`, `x |= K` through a temp,
  if-return <-> ternary, if/else swap (negated), for -> while / do-while,
  early return -> single exit goto, `!x` <-> `x == 0`,
  dup_tail_store (store after an if/else chain copied into every branch, and
  the reverse), fallthrough_label (`goto L; L: return K;`, return before a
  fallen-into label, drop goto-to-next-line, `goto end` for equal constant
  returns: branch-likely annul bit), div_hoist (adjacent array stores of / %
  values: temps / swapped order, trap_if scheduling), regalloc_tie (only when
  the diff is register-only: the body is compiled with -dl, the .lreg allocno
  table gives priorities floor_log2(refs)*refs/live; near-tied globals are
  mapped to C variables and statements are moved 1-4 lines across their
  live ranges).

Each round evaluates the single transforms of the current best with
try_fn.try_function (in-process, no try budget) family by family (family = the
label prefix: type, stmtswap, volatile, ...) in LEARNED order: families with
the best historical improvement rate first (ties: fewer compiles per attempt),
families that never helped last. The first batch that improves wins the round
(the rest of the round is skipped); repeat until no gain or --budget compiles.
Per-family attempts / improvements / matches / compiles live in
build/auto_decomp/trick_stats.json (seeded from the SWEEP lines of loop.log on
first use, updated as it runs; --stats prints them). After a sweep the sha1 of
the final best.c is recorded in build/auto_decomp/sweep_marks.json so
permute_queue knows the body was swept. Improvements go to
.best.c/.best.json ("by": "sweep"); a match is integrated via integrate.py.
Functions are leased as owner "sweep" while worked on.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import fcntl
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import claims  # noqa: E402
import state  # noqa: E402
from build_queue import build_queue  # noqa: E402

ASM_RE = re.compile(r"\b(__asm__|__asm|asm)\b")   # hard rule: never touch inline asm
BODIES = ROOT / "build/auto_decomp/bodies"
KEYWORDS = {"return", "goto", "break", "continue", "if", "else", "for", "while", "do",
            "switch", "case", "default", "sizeof", "typedef"}

# ---------------------------------------------------------------- text helpers


def match_brace(t: str, i: int) -> int:
    """Index of the bracket closing the one at t[i] ('{' or '(')."""
    op = t[i]
    cl = {"{": "}", "(": ")", "[": "]"}[op]
    d = 0
    j = i
    while j < len(t):
        c = t[j]
        if c in "\"'":
            q = c
            j += 1
            while j < len(t) and t[j] != q:
                j += 2 if t[j] == "\\" else 1
        elif t.startswith("/*", j):
            j = t.find("*/", j) + 1 or len(t)
        elif t.startswith("//", j):
            j = t.find("\n", j)
            if j < 0:
                return -1
        elif c == op:
            d += 1
        elif c == cl:
            d -= 1
            if d == 0:
                return j
        j += 1
    return -1


def find_function(text: str, name: str) -> tuple[int, int, int] | None:
    """(header start, body '{' index, closing '}' index) of NAME's definition."""
    best = None
    for m in re.finditer(r"\b" + re.escape(name) + r"\s*\(", text):
        p = match_brace(text, m.end() - 1)
        if p < 0:
            continue
        rest = text[p + 1:]
        mm = re.match(r"\s*\{", rest)
        if not mm:
            continue
        b = p + 1 + mm.end() - 1
        e = match_brace(text, b)
        if e < 0:
            continue
        ls = text.rfind("\n", 0, m.start()) + 1
        best = (ls, b, e)
    return best


def split_top(s: str, sep: str = ",") -> list[str]:
    out, d, cur = [], 0, ""
    for c in s:
        if c in "([{":
            d += 1
        elif c in ")]}":
            d -= 1
        if c == sep and d == 0:
            out.append(cur)
            cur = ""
        else:
            cur += c
    out.append(cur)
    return out


DECL_RE = re.compile(
    r"^(?P<ind>\s*)(?P<type>(?:(?:unsigned|signed|const|volatile|register|static)\s+)*"
    r"(?:(?:struct|union|enum)\s+\w+|\w+)(?:\s+(?:int|char|short|long|long\s+long))?)"
    r"(?P<rest>(?:\s+|\s*\*)\**\s*\w+.*);\s*$")


def parse_decl(line: str):
    m = DECL_RE.match(line)
    if not m:
        return None
    ty = m.group("type")
    first = ty.split()[0]
    if first in KEYWORDS:
        return None
    rest = m.group("rest")
    if re.match(r"\s*\**\s*\w+\s*(\(|->|\.|\+\+|--|[-+*/%&|^]?=[^=]?)", rest) and \
            not re.match(r"\s*\**\s*\w+\s*(\[|=[^=]|,|$)", rest):
        return None
    # "x = 1;" parses as type=x rest=" = 1"? no: rest needs an identifier first.
    decls = [d.strip() for d in split_top(rest)]
    if not all(re.match(r"\**\s*\w+\s*(\[[^\]]*\])*\s*(=.*)?$", d, re.S) for d in decls):
        return None
    return m.group("ind"), ty, decls


def decl_name(d: str) -> str:
    return re.match(r"\**\s*(\w+)", d).group(1)


def body_lines(fn: str, b: int):
    """Lines of the function body (between the braces) plus offset."""
    return fn[b + 1:]


# ---------------------------------------------------------------- transforms
# Each transform gets the function text (header .. closing brace) and yields
# (label, new function text).

def _split_fn(fn: str):
    b = fn.index("{")
    if fn.find("(", 0, b) >= 0:              # `union { ... } *args` in the parameter list
        o = fn.index("(")
        c = match_brace(fn, o)
        m = re.compile(r"\s*\{").match(fn, c + 1) if c > 0 else None
        if m:
            b = m.end() - 1
    e = fn.rindex("}")
    return fn[:b + 1], fn[b + 1:e], fn[e:]


def _decl_block(inner: str):
    """(decl items [(type, declarator)], index of first non-decl line, lines)."""
    lines = inner.split("\n")
    items = []
    i = 0
    # skip leading blank line after '{'
    while i < len(lines):
        ln = lines[i]
        if not ln.strip():
            i += 1
            continue
        p = parse_decl(ln)
        if not p:
            break
        _, ty, decls = p
        for d in decls:
            items.append((ty, d))
        i += 1
    return items, i, lines


def _render(items, first, lines, ind="    "):
    out = [""] + [f"{ind}{ty} {d};" for ty, d in items] + lines[first:]
    return "\n".join(out)


def t_decls(fn):
    head, inner, tail = _split_fn(fn)
    items, first, lines = _decl_block(inner)
    if not items:
        return
    names = [decl_name(d) for _, d in items]

    def dep(a, b):  # b's initializer mentions a's name
        _, d = items[b]
        return "=" in d and re.search(r"\b" + names[a] + r"\b", d.split("=", 1)[1])

    for k in range(len(items) - 1):
        if dep(k, k + 1):
            continue
        it = items[:]
        it[k], it[k + 1] = it[k + 1], it[k]
        yield f"declswap:{names[k]}<->{names[k+1]}", head + _render(it, first, lines) + tail
    if len(items) > 2 and not any("=" in d for _, d in items):
        yield "declreverse", head + _render(items[::-1], first, lines) + tail
    # type flips / volatile
    FLIPS = {
        "int": ["unsigned int", "long long", "short"], "s32": ["unsigned int", "long long"],
        "signed int": ["unsigned int"], "long": ["int", "unsigned long"],
        "unsigned int": ["int", "unsigned long long"], "unsigned": ["int", "unsigned long long"],
        "u32": ["int", "unsigned long long"],
        "u64": ["unsigned int", "int"], "unsigned long long": ["unsigned int", "int"],
        "long long": ["int", "unsigned int"], "s64": ["int"], "unsigned long": ["unsigned int", "long"],
        "char": ["unsigned char", "int"], "signed char": ["unsigned char", "int"],
        "unsigned char": ["signed char", "int"], "u8": ["signed char", "int"], "s8": ["unsigned char", "int"],
        "short": ["unsigned short", "int"], "s16": ["unsigned short", "int"],
        "unsigned short": ["short", "int"], "u16": ["short", "int"],
        "float": [],
    }
    for k, (ty, d) in enumerate(items):
        base = re.sub(r"\b(register|const)\s+", "", ty).strip()
        ptr = d.lstrip().startswith("*")
        arr = "[" in d.split("=")[0]
        if not ptr and not arr:
            for nt in FLIPS.get(base, []):
                it = items[:]
                it[k] = (nt, d)
                yield f"type:{names[k]}:{base}->{nt}", head + _render(it, first, lines) + tail
        if ptr and base in ("char", "unsigned char", "u8", "s8", "short", "unsigned short", "s16", "u16"):
            nt = {"char": "unsigned char", "u8": "signed char", "s8": "unsigned char",
                  "unsigned char": "char", "short": "unsigned short", "s16": "unsigned short",
                  "unsigned short": "short", "u16": "short"}[base]
            it = items[:]
            it[k] = (nt, d)
            yield f"ptype:{names[k]}:{base}->{nt}", head + _render(it, first, lines) + tail
        if "volatile" not in ty and not arr:
            it = items[:]
            if ptr:
                it[k] = (ty, re.sub(r"^(\**)", r"\1 volatile ", d.lstrip(), count=1))
            else:
                it[k] = ("volatile " + ty, d)
            yield f"volatile:{names[k]}", head + _render(it, first, lines) + tail


def t_rename_swap(fn):
    head, inner, tail = _split_fn(fn)
    items, _, _ = _decl_block(inner)
    sc = [(ty, decl_name(d)) for ty, d in items if not d.lstrip().startswith("*") and "[" not in d]
    for a in range(len(sc)):
        for b in range(a + 1, len(sc)):
            if sc[a][0] != sc[b][0]:
                continue
            na, nb = sc[a][1], sc[b][1]
            new = re.sub(r"\b(%s|%s)\b" % (na, nb), lambda m: nb if m.group(1) == na else na, inner)
            yield f"rename:{na}<->{nb}", head + new + tail


STMT_SKIP = re.compile(r"^\s*(return|break|continue|goto|case|default|if|else|for|while|do|switch)\b|^\s*\w+\s*:(?!:)")


def _simple_stmt(ln: str) -> bool:
    s = ln.strip()
    return (s.endswith(";") and "{" not in s and "}" not in s and not STMT_SKIP.match(ln)
            and parse_decl(ln) is None and s.count("(") == s.count(")") and s.count(";") == 1)


def _writes(s: str) -> set[str]:
    w = set()
    m = re.match(r"\s*\**\s*\(?\s*(\w+)", s)
    if m and re.search(r"(?<![=!<>])=(?!=)|\+\+|--", s):
        w.add(m.group(1))
    for mm in re.finditer(r"(\w+)\s*(\+\+|--)|(\+\+|--)\s*(\w+)", s):
        w.add(mm.group(1) or mm.group(4))
    return w


def t_stmt_swap(fn):
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    for i in range(len(lines) - 1):
        a, b = lines[i], lines[i + 1]
        if not (_simple_stmt(a) and _simple_stmt(b)):
            continue
        if len(a) - len(a.lstrip()) != len(b) - len(b.lstrip()):
            continue
        wa, wb = _writes(a), _writes(b)
        ida = set(re.findall(r"\b\w+\b", a))
        idb = set(re.findall(r"\b\w+\b", b))
        if wa & idb or wb & ida:
            continue
        if "(" in a and "(" in b and re.search(r"\w\s*\(", a) and re.search(r"\w\s*\(", b):
            continue
        nl = lines[:]
        nl[i], nl[i + 1] = b, a
        yield f"stmtswap:L{i}", head + "\n".join(nl) + tail


OPND = r"(?:\(\s*(?:unsigned\s+)?\w+\s*\**\s*\)\s*)?(?:0x[0-9a-fA-F]+|\d+|\w+)(?:(?:->|\.)\w+|\[[^\[\]]+\])*"
BIN = re.compile(r"(?P<a>" + OPND + r")\s*(?P<op>==|!=|\+|\*|&|\||\^)\s*(?P<b>" + OPND + r")(?=\s*[;),\]])")


def _leftok(s: str, i: int) -> bool:
    j = i - 1
    while j >= 0 and s[j] in " \t":
        j -= 1
    if j < 0:
        return False
    if s[j] in "(,[?:{;":
        return True
    if s[j] == "=" and (j == 0 or s[j - 1] not in "=!<>"):
        return True
    return s[max(0, j - 5):j + 1].endswith("return")


def t_commute(fn):
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in BIN.finditer(inner):
        if not _leftok(inner, m.start()):
            continue
        if m.group("a") in KEYWORDS or m.group("b") in KEYWORDS:
            continue
        a, op, b = m.group("a"), m.group("op"), m.group("b")
        new = inner[:m.start()] + f"{b} {op} {a}" + inner[m.end():]
        yield f"commute:{a}{op}{b}", head + new + tail
        if op == "+":
            for x, lab in ((a, "a"), (b, "b")):
                if re.fullmatch(r"\d+|0x[0-9a-fA-F]+", x) or x.startswith("("):
                    continue
                na, nb = (f"(int){a}", b) if lab == "a" else (a, f"(int){b}")
                yield f"intcast:{x}", head + inner[:m.start()] + f"{na} + {nb}" + inner[m.end():] + tail
        n += 1
        if n > 40:
            break


ASSIGN_OP = re.compile(r"^(?P<ind>\s*)(?P<x>[\w.>\-\[\]]+) = (?P<a>\(.+\)|[\w.>\-\[\]]+) (?P<op>%|/|<<|>>|&|\||\^|\+|-|\*) (?P<b>[^;?]+);\s*$")
COMPOUND = re.compile(r"^(?P<ind>\s*)(?P<x>[\w.>\-\[\]]+) (?P<op>%|/|<<|>>|&|\||\^|\+|-|\*)= (?P<b>[^;]+);\s*$")


def t_split(fn):
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    for i, ln in enumerate(lines):
        m = ASSIGN_OP.match(ln)
        if m and m.group("x") not in m.group("b"):
            a = m.group("a")
            if a.startswith("(") and match_brace(a, 0) == len(a) - 1:
                a = a[1:-1]
            nl = lines[:]
            nl[i] = f"{m['ind']}{m['x']} = {a};\n{m['ind']}{m['x']} {m['op']}= {m['b']};"
            yield f"split:L{i}", head + "\n".join(nl) + tail
        m = COMPOUND.match(ln)
        if m and i > 0:
            p = re.match(r"^\s*" + re.escape(m["x"]) + r" = ([^;]+);\s*$", lines[i - 1])
            if p:
                nl = lines[:]
                nl[i - 1] = f"{m['ind']}{m['x']} = ({p.group(1)}) {m['op']} {m['b']};"
                del nl[i]
                yield f"join:L{i}", head + "\n".join(nl) + tail
        if m and m["op"] in "|&^+":
            nl = lines[:]
            x = m["x"]
            nl[i] = (f"{m['ind']}{{ __typeof__({x}) _st = {x} {m['op']} {m['b']}; {x} = _st; }}")
            yield f"temp:L{i}", head + "\n".join(nl) + tail


def t_ternary(fn):
    head, inner, tail = _split_fn(fn)
    pat = re.compile(r"(?P<ind>[ \t]*)if \((?P<c>.+)\) \{?\s*return (?P<a>[^;]+);\s*\}?\s*\n\s*return (?P<b>[^;]+);")
    for m in pat.finditer(inner):
        new = inner[:m.start()] + f"{m['ind']}return ({m['c']}) ? ({m['a']}) : ({m['b']});" + inner[m.end():]
        yield "ternary", head + new + tail
    pat2 = re.compile(r"(?P<ind>[ \t]*)return (?P<c>[^?;]+)\?(?P<a>[^:;?]+):(?P<b>[^;?]+);")
    for m in pat2.finditer(inner):
        ind = m["ind"]
        new = inner[:m.start()] + (f"{ind}if ({m['c'].strip()}) {{\n{ind}    return {m['a'].strip()};\n{ind}}}\n"
                                   f"{ind}return {m['b'].strip()};") + inner[m.end():]
        yield "unternary", head + new + tail


def t_ifelse(fn):
    head, inner, tail = _split_fn(fn)
    for m in re.finditer(r"\bif\s*\(", inner):
        if inner[max(0, m.start() - 5):m.start()].rstrip().endswith("else"):
            continue
        cp = match_brace(inner, m.end() - 1)
        mm = re.match(r"\s*\{", inner[cp + 1:])
        if cp < 0 or not mm:
            continue
        b1 = cp + 1 + mm.end() - 1
        e1 = match_brace(inner, b1)
        me = re.match(r"\s*else\s*\{", inner[e1 + 1:])
        if e1 < 0 or not me:
            continue
        b2 = e1 + 1 + me.end() - 1
        e2 = match_brace(inner, b2)
        if e2 < 0:
            continue
        cond = inner[m.end():cp]
        A, B = inner[b1 + 1:e1], inner[b2 + 1:e2]
        new = inner[:m.start()] + f"if (!({cond})) {{{B}}} else {{{A}}}" + inner[e2 + 1:]
        yield f"ifswap:{cond[:20]}", head + new + tail
        c = cond.strip()
        mn = re.fullmatch(r"!\s*\((.*)\)", c, re.S)
        inv = (mn.group(1) if mn and match_brace(c, c.index("(")) == len(c) - 1 else None) or _invert_simple(c)
        if inv:                           # ACT_updateSequence: `x & K` / `!(x & K)` swap, no double negation
            new = inner[:m.start()] + f"if ({inv.strip()}) {{{B}}} else {{{A}}}" + inner[e2 + 1:]
            yield f"ifswap:clean:{cond[:20]}", head + new + tail


def t_loops(fn):
    head, inner, tail = _split_fn(fn)
    for m in re.finditer(r"\bfor\s*\(", inner):
        cp = match_brace(inner, m.end() - 1)
        parts = split_top(inner[m.end():cp], ";")
        mm = re.match(r"\s*\{", inner[cp + 1:])
        if len(parts) != 3 or not mm:
            continue
        b = cp + 1 + mm.end() - 1
        e = match_brace(inner, b)
        body = inner[b + 1:e]
        if re.search(r"\bcontinue\b", body):
            continue
        ini, cond, step = (p.strip() for p in parts)
        ls = inner.rfind("\n", 0, m.start()) + 1
        ind = inner[ls:m.start()]
        init = f"{ini};\n{ind}" if ini else ""
        stepl = f"    {step};\n{ind}" if step else ""
        c = cond or "1"
        w = f"{init}while ({c}) {{{body.rstrip()}\n{ind}{stepl}}}"
        yield "for->while", head + inner[:m.start()] + w + inner[e + 1:] + tail
        dw = f"{init}if ({c}) {{\n{ind}    do {{{body.rstrip()}\n{ind}    {stepl}}} while ({c});\n{ind}}}"
        yield "for->dowhile", head + inner[:m.start()] + dw + inner[e + 1:] + tail
        dw2 = f"{init}do {{{body.rstrip()}\n{ind}{stepl}}} while ({c});"
        yield "for->dowhile-nocheck", head + inner[:m.start()] + dw2 + inner[e + 1:] + tail
        gl = (f"{init}goto _sw_test;\n{ind}do {{{body.rstrip()}\n{ind}{stepl}_sw_test:;\n{ind}}} while ({c});")
        yield "for->goto-test", head + inner[:m.start()] + gl + inner[e + 1:] + tail
    for m in re.finditer(r"\bwhile\s*\(", inner):
        if re.search(r"\}\s*$", inner[:m.start()]):
            continue   # do-while tail
        cp = match_brace(inner, m.end() - 1)
        cond = inner[m.end():cp]
        mm = re.match(r"\s*\{", inner[cp + 1:])
        if not mm:
            continue
        b = cp + 1 + mm.end() - 1
        e = match_brace(inner, b)
        body = inner[b + 1:e]
        yield "while->for", head + inner[:m.start()] + f"for (; {cond};) {{{body}}}" + inner[e + 1:] + tail
        if not re.search(r"\bcontinue\b", body):
            yield "while->dowhile", head + inner[:m.start()] + \
                f"if ({cond}) do {{{body}}} while ({cond});" + inner[e + 1:] + tail


def t_single_exit(fn):
    head, inner, tail = _split_fn(fn)
    hdr = head[:head.index("{")]
    name_m = re.search(r"(\w+)\s*\(", hdr)
    rtype = hdr[:name_m.start()].strip()
    rtype = re.sub(r"\b(static|inline)\b", "", rtype).strip()
    rets = list(re.finditer(r"\breturn\b\s*([^;]*);", inner))
    if len(rets) < 2 and not (rets and inner[rets[-1].end():].strip()):
        return
    void = rtype == "void" or rtype == ""
    if void:
        new = re.sub(r"\breturn\s*;", "goto _sw_exit;", inner)
        yield "single-exit", head + new.rstrip() + "\n_sw_exit:;\n" + tail
    else:
        new = re.sub(r"\breturn\b\s*([^;]+);", r"{ _sw_rv = \1; goto _sw_exit; }", inner)
        yield "single-exit", head + f"\n    {rtype} _sw_rv;" + new.rstrip() + "\n_sw_exit:\n    return _sw_rv;\n" + tail
        # switch whose cases each `return`: `ret = E; break;` + one return after it
        # (CheckCornerDist: kept the float result in $f20 + a trailing mov.s)
        ms = re.search(r"\bswitch\s*\(", inner)
        if ms and not re.search(r"\b(for|while|do)\b", inner):
            ob = inner.find("{", match_brace(inner, ms.end() - 1))
            cb = match_brace(inner, ob) if ob > 0 else -1
            if cb > 0:
                after = inner[cb + 1:].strip()
                ma = re.fullmatch(r"return\s+([^;]+);", after)
                if (ma or not after) and "return" in inner[ob:cb]:
                    sw = re.sub(r"\breturn\b\s*([^;]+);", r"{ _sw_rv = \1; break; }", inner[ob:cb + 1])
                    ls = inner.rfind("\n", 0, ms.start()) + 1
                    pre = inner[:ls] + (f"    _sw_rv = {ma.group(1)};\n" if ma else "")
                    yield "single-exit", (head + f"\n    {rtype} _sw_rv;" + pre + inner[ls:ob] + sw
                                          + "\n    return _sw_rv;\n" + tail)
        yield from _single_exit_ifelse(fn)


def _single_exit_ifelse(fn):
    """keyCmdGet (o456): `if (C) { S; return K; } REST return v;` (v a local,
    the function's last statement) -> `if (C) { S; v = K; } else { REST }
    return v;` (the assignment where the return was, or first in the block)."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    nz = [k for k, ln in enumerate(lines) if ln.strip()]
    if not nz:
        return
    last = nz[-1]
    mr = re.match(r"^(\s*)return\s+([A-Za-z_]\w*)\s*;\s*$", lines[last])
    if not mr or mr.group(2) not in locs:
        return
    v, d = mr.group(2), dep[last]
    n = 0
    for i in range(first, last):
        m = re.match(r"^(\s*)if\s*\(.*\{\s*$", lines[i])
        if not m or dep[i] != d:
            continue
        o = lines[i].index("(")
        if match_brace(lines[i], o) < 0:
            continue
        end = next((j for j in range(i + 1, last) if dep[j] == d + 1 and lines[j].strip() == "}"
                    and dep[j + 1] == d), None)
        if end is None or end + 1 >= last or lines[end + 1].strip().startswith("else"):
            continue
        body = [k for k in range(i + 1, end) if lines[k].strip()]
        if not body:
            continue
        rr = re.match(r"^(\s*)return\s+([^;]+);\s*$", lines[body[-1]])
        if not rr or dep[body[-1]] != d + 1:
            continue
        K = rr.group(2).strip()
        ind = m.group(1)
        rest = ["    " + x if x.strip() else x for x in lines[end + 1:last]]
        for form in ("at", "first"):
            th = lines[i + 1:body[-1]] + lines[body[-1] + 1:end]
            asg = f"{rr.group(1)}{v} = {K};"
            th = th + [asg] if form == "at" else [asg] + th
            nl = lines[:i + 1] + th + [f"{ind}}} else {{"] + rest + [f"{ind}}}"] + lines[last:]
            yield f"single_exit_ifelse:{form}:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 6:
            return


def t_not(fn):
    head, inner, tail = _split_fn(fn)
    for m in re.finditer(r"\(\s*!\s*(" + OPND + r")\s*\)", inner):
        yield "not->eq0", head + inner[:m.start()] + f"({m.group(1)} == 0)" + inner[m.end():] + tail
    for m in re.finditer(r"\(\s*(" + OPND + r")\s*==\s*0\s*\)", inner):
        yield "eq0->not", head + inner[:m.start()] + f"(!{m.group(1)})" + inner[m.end():] + tail
    for m in re.finditer(r"\(\s*(" + OPND + r")\s*!=\s*0\s*\)", inner):
        yield "ne0->bare", head + inner[:m.start()] + f"({m.group(1)})" + inner[m.end():] + tail


# ---- if/else chains (dup_tail_store) --------------------------------------

def _if_chains(inner: str):
    """Braced if / else if / else chains: (start, end, [(open, close)], has_else)."""
    for m in re.finditer(r"\bif\s*\(", inner):
        if re.search(r"\belse\s*$", inner[:m.start()]):
            continue                      # an `else if` inside a chain
        cp = match_brace(inner, m.end() - 1)
        if cp < 0:
            continue
        mm = re.match(r"\s*\{", inner[cp + 1:])
        if not mm:
            continue
        o = cp + 1 + mm.end() - 1
        c = match_brace(inner, o)
        if c < 0:
            continue
        brs, has_else, end = [(o, c)], False, c
        while True:
            me = re.match(r"\s*else\s+if\s*\(", inner[end + 1:])
            if me:
                p = end + 1 + me.end() - 1
                cp = match_brace(inner, p)
                mm = re.match(r"\s*\{", inner[cp + 1:]) if cp > 0 else None
                if not mm:
                    brs = None
                    break
                o = cp + 1 + mm.end() - 1
                c = match_brace(inner, o)
                if c < 0:
                    brs = None
                    break
                brs.append((o, c))
                end = c
                continue
            me = re.match(r"\s*else\s*\{", inner[end + 1:])
            if me:
                o = end + 1 + me.end() - 1
                c = match_brace(inner, o)
                if c < 0:
                    brs = None
                    break
                brs.append((o, c))
                has_else, end = True, c
            break
        if brs:
            ls = inner.rfind("\n", 0, m.start()) + 1
            yield ls, end, brs, has_else, inner[ls:m.start()]


def _branch_tail(inner: str, o: int, c: int):
    """(start, end, text) of the last statement line of branch (o, c), or None."""
    body = inner[o + 1:c].rstrip()
    if not body.strip():
        return None
    ls = body.rfind("\n") + 1
    ln = body[ls:]
    if not _simple_stmt(ln):
        return None
    return o + 1 + ls, o + 1 + len(body), ln.strip()


def t_dup_tail_store(fn):
    """A store right after an if/else chain -> the end of every branch (gcc
    cross-jumps the identical tails after scheduling, so the store becomes the
    first insn at the join label), and the reverse: identical branch-tail
    stores hoisted after the chain."""
    head, inner, tail = _split_fn(fn)
    for ls, end, brs, has_else, ind in _if_chains(inner):
        after = inner[end + 1:]
        m = re.match(r"[ \t]*\n([ \t]*)([^\n]*)\n", after)
        if m and _simple_stmt(m.group(2)) and "=" in m.group(2):
            st = m.group(2).strip()
            # rebuild with insertions (positions are in `inner`, insert back-to-front)
            out = inner[:end + 1]
            for o, c in reversed(brs):
                bi = inner.rfind("\n", 0, c) + 1
                pre = inner[bi:c]
                bind = pre if not pre.strip() else ind
                out = out[:c].rstrip() + f"\n{bind}    {st}\n{bind}" + out[c:]
            if not has_else:
                out += f" else {{\n{ind}    {st}\n{ind}}}"
            rest = after[m.end() - 1:]
            yield f"dup_tail_store:{st[:24]}", head + out + rest + tail
        if has_else and len(brs) >= 2:
            tails = [_branch_tail(inner, o, c) for o, c in brs]
            if all(tails) and len({t[2] for t in tails}) == 1 and "=" in tails[0][2]:
                st = tails[0][2]
                out, shift = inner, 0
                for ts, te, _ in reversed(tails):
                    cut = out.rfind("\n", 0, ts)
                    out = out[:cut] + out[te:]
                    shift += te - cut
                endpos = end - shift
                out = out[:endpos + 1] + f"\n{ind}{st}" + out[endpos + 1:]
                yield f"dup_tail_store:rev:{st[:24]}", head + out + tail


# ---- branch-likely / fall-through label ------------------------------------

def t_fallthrough_label(fn, ctx=None):
    """Reorg's annul (branch-likely) choice depends on a label / jump at the
    fall-through: add `goto L; L:` before a constant return, drop a goto that
    only jumps to the next line, or return before a label that is fallen into,
    or route an inner `return K;` through a label at the function's end
    (`goto-tail`: MenuFaceEpidGet beqz -> beqzl). When the diff shows a
    branch-likely bit flip the same candidates run as family `fallthrough_bl`
    (diff-gated, own stats/prior)."""
    if (ctx or {}).get("fallthrough_bl"):
        for lab, nf in _fallthrough_cands(fn):
            yield "fallthrough_bl:" + lab.split(":", 1)[1], nf
        return
    yield from _fallthrough_cands(fn)


def _fallthrough_cands(fn):
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in re.finditer(r"\breturn\s+(-?(?:0x[0-9a-fA-F]+|\d+)|\w+)\s*;", inner):
        n += 1
        if n > 12:
            break
        new = inner[:m.start()] + f"{{ goto _sw_ft{n}; _sw_ft{n}: {m.group(0)} }}" + inner[m.end():]
        yield f"fallthrough_label:goto-return{n}", head + new + tail
        p = re.search(r"(\n[ \t]*)(\w+):[ \t]*\n[ \t]*$", inner[:m.start()])
        if p and p.group(2) not in ("default",):
            new = inner[:p.start()] + f"{p.group(1)}{m.group(0)}" + inner[p.start():]
            yield f"fallthrough_label:return-before-{p.group(2)}", head + new + tail
    for m in re.finditer(r"\bgoto\s+(\w+)\s*;\s*\n(\s*)\1\s*:", inner):
        new = inner[:m.start()] + inner[inner.index("\n", m.start()) + 1:]
        yield f"fallthrough_label:drop-goto-{m.group(1)}", head + new + tail
    # value-return through a variable: `ret = K; goto end;`
    rets = list(re.finditer(r"\breturn\s+(-?(?:0x[0-9a-fA-F]+|\d+))\s*;", inner))
    if len(rets) >= 2:
        hdr = head[:head.index("{")]
        nm = re.search(r"(\w+)\s*\(", hdr)
        rtype = re.sub(r"\b(static|inline)\b", "", hdr[:nm.start()]).strip() if nm else ""
        if rtype and rtype != "void":
            last = rets[-1]
            new = inner[:last.start()] + "_sw_end:\n    " + inner[last.start():]
            for r in reversed(rets[:-1]):
                if r.group(1) == last.group(1):
                    new = new[:r.start()] + "goto _sw_end;" + new[r.end():]
            yield "fallthrough_label:goto-end", head + new + tail
    # an inner `return K;` (not the final statement) -> `goto L;` + `L: return K;` at the end
    body_end = inner.rstrip()
    n = 0
    for m in re.finditer(r"\breturn(\s+[^;]+?)?\s*;", inner):
        if not inner[m.end():].strip() or (m.group(1) and "(" in m.group(1)):
            continue
        n += 1
        if n > 8:
            break
        val = (m.group(1) or "").strip()
        new = inner[:m.start()] + f"goto _sw_tl{n};" + inner[m.end():len(body_end)]
        yield f"fallthrough_label:goto-tail{n}", head + new + f"\n_sw_tl{n}:\n    return{' ' + val if val else ''};\n" + tail
    yield from _flag_else_return(head, inner, tail)


def _flag_else_return(head, inner, tail):
    """A result flag set after a loop, `if (C) V = K;` with `return V;` at the
    end -> `if (!(C)) { V = V0; } else { return K; }` (V0 = V's constant
    initializer). Also an outer `if (x != 0) { rest }` wrapper -> `if (x == 0)
    return V0;` (MenuCharHpCheck: bnel -> bne)."""
    fin = re.search(r"\breturn\s+(\w+)\s*;\s*$", inner.rstrip())
    if not fin:
        return
    V = fin.group(1)
    init = re.search(r"\b" + V + r"\s*=\s*(-?(?:0x[0-9a-fA-F]+|\d+))\s*;", inner)
    V0 = init.group(1) if init else None
    n = 0
    pat = re.compile(r"\bif\s*\(")
    for m in pat.finditer(inner):
        cp = match_brace(inner, m.end() - 1)
        if cp < 0:
            continue
        C = inner[m.end():cp]
        rest = inner[cp + 1:]
        mb = (re.match(r"\s*\{\s*" + V + r"\s*=\s*([^;{}]+);\s*\}", rest)
              or re.match(r"\s*" + V + r"\s*=\s*([^;{}]+);", rest))
        if not mb or re.match(r"\s*else\b", rest[mb.end():]):
            continue
        K = mb.group(1).strip()
        if K == V0:
            continue
        n += 1
        if n > 6:
            break
        keep = f"{V} = {V0}; " if V0 is not None else ""
        for tag, neg in (("not", f"!({C.strip()})"), ("inv", _invert_simple(C))):
            if not neg:
                continue
            rep = f"if ({neg}) {{ {keep}}} else {{ return {K}; }}"
            yield (f"fallthrough_label:flag-else-return{n}:{tag}",
                   head + inner[:m.start()] + rep + rest[mb.end():] + tail)


# ---- trap_if scheduling (div_hoist) ----------------------------------------

_STORE = re.compile(r"^(?P<ind>[ \t]*)(?P<lhs>[\w.>\-]+\s*\[[^\]]+\])\s*=\s*(?P<rhs>[^;]+);\s*$")


def t_div_hoist(fn):
    """Two adjacent array stores whose values divide (/ %): sched2 order follows
    the trap_if (div-by-zero) anti-deps on the earlier store. Hoist the later
    value into a temp before the earlier store, both into temps (in order), or
    swap the stores keeping the earlier value in a temp."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    for i in range(len(lines) - 1):
        a, b = _STORE.match(lines[i]), _STORE.match(lines[i + 1])
        if not a or not b or a["ind"] != b["ind"]:
            continue
        if not re.search(r"[/%]", a["rhs"] + b["rhs"]):
            continue
        base = lambda x: re.match(r"[\w]+", x).group(0)  # noqa: E731
        if base(a["lhs"]) in b["rhs"] or base(b["lhs"]) in a["rhs"]:
            continue
        ind, A, B = a["ind"], a["rhs"].strip(), b["rhs"].strip()
        var = [
            ("later", [f"{ind}{{", f"{ind}    __typeof__({B}) _dh1 = {B};",
                       f"{ind}    {a['lhs']} = {A};", f"{ind}    {b['lhs']} = _dh1;", f"{ind}}}"]),
            ("both", [f"{ind}{{", f"{ind}    __typeof__({A}) _dh0 = {A};", f"{ind}    __typeof__({B}) _dh1 = {B};",
                      f"{ind}    {a['lhs']} = _dh0;", f"{ind}    {b['lhs']} = _dh1;", f"{ind}}}"]),
            ("swap", [f"{ind}{{", f"{ind}    __typeof__({A}) _dh0 = {A};",
                      f"{ind}    {b['lhs']} = {B};", f"{ind}    {a['lhs']} = _dh0;", f"{ind}}}"]),
            ("swap-plain", [f"{ind}{b['lhs']} = {B};", f"{ind}{a['lhs']} = {A};"]),
        ]
        for lab, rep in var:
            nl = lines[:i] + rep + lines[i + 2:]
            yield f"div_hoist:{lab}:L{i}", head + "\n".join(nl) + tail


# ---- register-allocation near-tie (regalloc_tie) ---------------------------

def _uses(s: str) -> set[str]:
    return set(re.findall(r"\b[A-Za-z_]\w*\b", s))


def t_regalloc_tie(fn, ctx=None):
    """Statement moves that lengthen / shorten the live range of a value in a
    near-tied global-alloc priority pair (gcc: floor_log2(refs)*refs/live).
    Only generated when ctx["ties"] names the tied variables (see
    regalloc_ties); moves a simple statement 1-4 lines up/down across
    independent simple statements when it or a crossed line mentions one."""
    tied = set((ctx or {}).get("tie_vars") or [])
    if not tied:
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    calls = lambda s: bool(re.search(r"\w\s*\(", s))  # noqa: E731
    for i, s in enumerate(lines):
        if not _simple_stmt(s):
            continue
        ind = len(s) - len(s.lstrip())
        ws, us = _writes(s), _uses(s)
        for d in (-4, -3, -2, -1, 1, 2, 3, 4):
            j = i + d
            if j < 0 or j >= len(lines):
                continue
            rng = lines[j:i] if d < 0 else lines[i + 1:j + 1]
            ok = True
            for x in rng:
                if not _simple_stmt(x) or len(x) - len(x.lstrip()) != ind:
                    ok = False
                    break
                if ws & _uses(x) or _writes(x) & us or (calls(s) and calls(x)):
                    ok = False
                    break
            if not ok:
                continue
            hit = (us | set().union(*(_uses(x) for x in rng))) & tied
            if not hit:
                continue
            nl = lines[:]
            del nl[i]
            nl.insert(j, s)
            yield f"regalloc_tie:{sorted(hit)[0]}:L{i}->L{j}", head + "\n".join(nl) + tail


_LREG = re.compile(r"^Register (\d+) used (\d+) times across (\d+) insns(.*)$", re.M)


def _noaslr() -> list:
    """setarch -R prefix for direct cc1 runs (tools/tu/noaslr.py)."""
    import try_fn
    return try_fn.NOASLR


def _cc1_cmd(unit: str, tu: str):
    """(ee-gcc path, cc1 flags) of the TU's compile edge."""
    import try_fn
    ud, g, rule, e = try_fn.compile_edge(unit, tu)
    sc = dict(g)
    sc.update({a: try_fn.expand(b, g) for a, b in e["vars"].items()})
    sc.update({"in": "IN.c", "out": "OUT.o", "overlay": "OV"})
    cmd = try_fn.expand(rule["command"], sc)
    m = re.search(r"cc_tu\.sh\s+\S+\s+\S+\s+(\S+)\s+\S+\s+(-G\d+)", cmd)
    if m:
        return f"{m.group(1)}/ee-gcc", ["-B" + m.group(1) + "/", "-nostdinc", "-fno-builtin", "-O2", m.group(2)]
    m = re.search(r"(\S+/ee-gcc)\s+(-B\S+)\s+-nostdinc\s+-fno-builtin\s+-S\s+(.*?)\s+\S+\.i\s+-o", cmd)
    if m:
        return m.group(1), [m.group(2), "-nostdinc", "-fno-builtin"] + m.group(3).split()
    raise ValueError("no cc1 step in compile command")


def _floor_log2(n: int) -> int:
    return max(n, 1).bit_length() - 1


def regalloc_ties(unit: str, tu: str, name: str, text: str, tol: float = 0.15) -> dict:
    """Compile TEXT (the body) with -dl, read NAME's .lreg allocno table and
    return {"regs": [...], "ties": [[a, b, pa, pb]...], "tie_vars": [...]}.
    Pseudos of user variables are mapped to params + scalar locals in
    declaration order (gcc numbers them that way); global (multi-block)
    pseudos with priorities within TOL are near-ties."""
    out: dict = {"regs": [], "ties": [], "tie_vars": []}
    dd = compile_dumps(unit, tu, name, text, ["-dl"])
    sec = dd["dumps"].get("lreg")
    if sec is None:
        out["error"] = dd.get("error") or "no .lreg dump"
        return out
    regs = []
    for g in _LREG.finditer(sec):
        n, refs, live, rest = int(g[1]), int(g[2]), int(g[3]), g[4]
        regs.append({"reg": n, "refs": refs, "live": live, "user": "user var" in rest,
                     "global": "in block" not in rest,
                     "prio": round(_floor_log2(refs) * refs / max(live, 1), 4)})
    # pseudo -> variable name (params + scalar locals, declaration order)
    loc = find_function(text, name)
    names: list[str] = []
    if loc:
        s, b, e = loc
        fn = text[s:e + 1]
        hdr = fn[:fn.index("{")]
        pm = re.search(r"\((.*)\)\s*$", hdr, re.S)
        for prm in split_top(pm.group(1)) if pm else []:
            mm = re.search(r"(\w+)\s*(\[[^\]]*\])?\s*$", prm.strip())
            if mm and prm.strip() != "void":
                names.append(mm.group(1))
        _, inner, _ = _split_fn(fn)
        items, _, _ = _decl_block(inner)
        for ty, dcl in items:
            nm = decl_name(dcl)
            if "[" in dcl.split("=")[0] or re.search(r"(?:[=(,!?:]|\breturn)\s*&\s*" + nm + r"\b(?!\s*(?:\[|\.|->))", inner):
                continue
            if re.match(r"(struct|union)\b", ty) and not dcl.lstrip().startswith("*"):
                continue
            names.append(nm)
    users = [x for x in regs if x["user"]]
    mapped = len(users) == len(names)
    for x, nm in zip(users, names):
        x["var"] = nm
    out["mapped"] = mapped
    out["regs"] = regs
    gl = sorted((x for x in regs if x["global"] and x["prio"] > 0 and x["live"] >= 8), key=lambda x: -x["prio"])
    for a, b in zip(gl, gl[1:]):
        if a["prio"] - b["prio"] <= tol * a["prio"]:
            out["ties"].append([a["reg"], b["reg"], a["prio"], b["prio"]])
            for x in (a, b):
                if x.get("var"):
                    out["tie_vars"].append(x["var"])
    out["tie_vars"] = sorted(set(out["tie_vars"]))
    if not out["tie_vars"] and out["ties"]:
        out["tie_vars"] = names           # unmapped tie: let every variable move
    return out

# ---- compiler dumps (shared by regalloc_tie, sched_weight, de_ifcvt) --------

def compile_dumps(unit: str, tu: str, name: str, text: str, dflags, extra=()) -> dict:
    """Compile TEXT (a body for NAME) through try_fn (keep=True), recompile the
    kept candidate .i with the TU's cc1 flags plus DFLAGS (e.g. ["-dl"],
    ["-dS", "-dR", "-fsched-verbose=6"]) and EXTRA, and return
    {"dumps": {suffix: NAME's section}, "error": ...}; suffix is the dump
    extension without the pass number ("lreg", "sched", "sched2", "ce2"...)."""
    import shutil
    import try_fn
    fd, p = tempfile.mkstemp(prefix=f"dump_{name}_", suffix=".c")
    os.write(fd, text.encode())
    os.close(fd)
    out: dict = {"dumps": {}}
    try:
        r = try_fn.try_function(unit, tu, name, Path(p), keep=True, diff_limit=0)
        scratch = Path(r.get("scratch", ""))
        try:
            ii = list((scratch / "cand").glob("*.i"))
            if not ii:
                out["error"] = "no .i"
                return out
            gcc, flags = _cc1_cmd(unit, tu)
            with tempfile.TemporaryDirectory(prefix="dump_") as td:
                shutil.copyfile(ii[0], Path(td) / "x.i")
                subprocess.run([*_noaslr(), gcc, *flags, "-S", *dflags, *extra, "x.i", "-o", "x.s"], cwd=td,
                               capture_output=True, text=True, timeout=120)
                for f in Path(td).glob("x.i.*"):
                    suf = re.sub(r"^\d+\.", "", f.name[len("x.i."):])
                    sec = _fn_section(f.read_text(errors="replace"), name)
                    if sec is not None:
                        out["dumps"][suf] = sec
        finally:
            if scratch.name:
                shutil.rmtree(scratch, ignore_errors=True)
    finally:
        os.unlink(p)
    if not out["dumps"]:
        out["error"] = "no dump for function"
    return out


def _fn_section(d: str, name: str) -> str | None:
    m = re.search(r"^;; Function " + re.escape(name) + r"\s*$", d, re.M)
    if not m:
        return None
    nx = re.search(r"^;; Function ", d[m.end():], re.M)
    return d[m.end():m.end() + nx.start()] if nx else d[m.end():]


# ---- diff shapes (shared with permute_queue triage) -------------------------

BRANCH_RE = re.compile(r"^(b|bal|beq|bne|beqz|bnez|bgez|bgtz|blez|bltz|bgezal|bltzal|bc1t|bc1f)l?$")
INVERSE = {frozenset(p) for p in (("beq", "bne"), ("beqz", "bnez"), ("bgez", "bltz"), ("blez", "bgtz"),
                                   ("bc1t", "bc1f"))}
STORE_RE = re.compile(r"^(sb|sh|sw|sd|sq|swc1|sdc1|swl|swr|sdl|sdr)$")


def diff_rows(rows) -> list[tuple[str, str, str]]:
    """try_fn side_by_side rows -> [(target insn, mark, candidate insn)]
    (addresses stripped); [] when the diff is truncated or absent."""
    if not rows or "   CANDIDATE" not in rows[0]:
        return []
    w = rows[0].index("   CANDIDATE")
    side = lambda x: x.split(":", 1)[1].strip() if ":" in x else ""  # noqa: E731
    out = []
    for r in rows[1:]:
        if r.startswith("... ("):
            return []
        out.append((side(r[:w]), r[w + 1:w + 2], side(r[w + 3:])))
    return out


def _mn(insn: str) -> str:
    return insn.split()[0] if insn.split() else ""


def _ops_nt(insn: str) -> str:
    """operands without the branch target and relocation tag."""
    insn = re.sub(r"\s*\[.*$", "", insn)
    p = insn.split(None, 1)
    ops = p[1] if len(p) > 1 else ""
    return ",".join(o.strip() for o in ops.split(",") if not re.fullmatch(r"[0-9a-f]+", o.strip()))


def cmp_mismatch(rows) -> list[dict]:
    """Aligned rows where the branch condition itself differs (not a simple
    inversion pair, not just the likely bit) on the same operands, or an
    slti/sltiu immediate differing by one: the C comparison is wrong."""
    out = []
    for a, m, b in diff_rows(rows):
        if m != "|" or not a or not b:
            continue
        ma, mb = _mn(a), _mn(b)
        if BRANCH_RE.match(ma) and BRANCH_RE.match(mb):
            fa = ma[:-1] if ma.endswith("l") and ma != "bal" else ma
            fb = mb[:-1] if mb.endswith("l") and mb != "bal" else mb
            if fa != fb and frozenset((fa, fb)) not in INVERSE and _ops_nt(a) == _ops_nt(b):
                out.append({"target": fa, "cand": fb, "ops": _ops_nt(a), "zero": fa[-1] == "z" or fa in
                            ("bgez", "bgtz", "blez", "bltz")})
        elif ma == mb and ma in ("slti", "sltiu"):
            ia, ib = a.split(",")[-1].strip(), b.split(",")[-1].strip()
            try:
                if abs(int(ia, 0) - int(ib, 0)) == 1 and a.rsplit(",", 1)[0] == b.rsplit(",", 1)[0]:
                    out.append({"target": f"{ma} {ia}", "cand": f"{mb} {ib}", "ops": a, "zero": False,
                                "imm": [int(ia, 0), int(ib, 0)]})
            except ValueError:
                pass
    return out


def _reloc_only(a: str, b: str) -> bool:
    """A relocation tag on at least one side and otherwise only the immediate
    differs (a .rodata string / jump table the data carve places, a .lit4
    literal, an absolute address splat left unsymbolized)."""
    if "[" not in a and "[" not in b:      # one side may be a plain absolute (try_fn abs_reloc)
        return False
    norm = lambda x: re.sub(r"-?0x[0-9a-f]+|-?\b\d+\b", "N", re.sub(r"\s*\[.*$", "", x))  # noqa: E731
    return norm(a) == norm(b)


def moved_insns(rows) -> list[str] | None:
    """Insns that only moved (same multiset on both sides of the differing
    rows, nothing else differs); None when the diff is not a pure move."""
    tl: dict = {}
    cl: dict = {}
    n = 0
    for a, m, b in diff_rows(rows):
        if m == " " or (m == "|" and _reloc_only(a, b)):
            continue
        n += 1
        for x, bag in ((a, tl), (b, cl)):
            if x:
                bag[x] = bag.get(x, 0) + 1
    if not n or tl != cl:
        return None
    # a swap shows as one '<' + one '>' (or a '|' pair): every moved insn once per side
    return sorted(tl)


def diff_window(rows, pad: int = 2) -> list[str]:
    """Target insns of the differing rows plus PAD rows of context."""
    dr = diff_rows(rows)
    idx = [i for i, (_, m, _) in enumerate(dr) if m != " "]
    if not idx:
        return []
    lo, hi = max(0, idx[0] - pad), min(len(dr), idx[-1] + pad + 1)
    return [x for a, _, b in dr[lo:hi] for x in (a, b) if x]


def diff_shape(rows) -> dict:
    """Mechanism tags for a try_fn diff (see WORKER_LESSONS.md):
      cmp_fix      branch condition differs (bgez vs blez, not an inversion)
      sched_weight one store moved, nothing else (sched1 register-weight tie)
      de_ifcvt     a move next to movz/movn (early if-conversion: sched2 luid tie)
      loop_live    only address/constant inits reordered (loop-preheader tie)
      const_fix    same insn, different immediate (wrong constant copied from a sibling)
      zext_form    target keeps andi 0xffff + sltiu that our compare lost
      field_alias  pure reorder moving a non-sp sw/sh/sb + a same-width global load
      volatile_mask only an epilogue ld sN/ra moved among a run of stores
      loop_form    target `move; b(n)ez; andi` loop bottom where we have `andi; b(n)ez`
      split_local  target scratch v*/a*/t* where ours has s* (one local reused across regions)
      hoist_load   target plain branch + load in its slot where ours is branch-likely
      sched_barrier a store moved (around a loop / helper: do{}while(0) barrier, fresh temp)
      param_widen  an a-reg param extended (sll/sra 16|24, andi) on one side only
      const_rev    same-reg stores at different offsets (a constant store run misordered)
      addu_swap    addu with rs/rt swapped
      sign_flip    slti<->sltiu / slt<->sltu
      float_cmp_neg a differing c.cond.fmt / bc1t / bc1f (a >= b vs !(a < b))
      unsized_extern a symbol gp-relative on one side, lui/%lo on the other
      aligned_local ldl/ldr/sdl/sdr (lwl/...) on our side only
      store_sort   >= 2 differing stores through one base register or to distinct globals (store order)
      block_barrier pure move of a load (sched1 hoisted it above a store: dead float store-flag barrier)
      load_width_cast same-row loads of another width/sign (lhu vs lbu: cast the dereference)
      fallthrough_bl a branch whose likely bit alone differs (beqz vs beqzl: label/goto at the fall-through)
      global_field_view more `lui %hi(S)` on our side (raw-offset global accesses -> typed view)
      global_recast more `lui %hi(S)` in the target (a later loop re-loads the global: direct casts, no alias)
      const_local  `li rX,K` on both sides in other registers (a repeated literal -> one local)
      const_cmp    target li+slt(u) where ours has slti(u) (compare constant in a local; family const_local)
      cvt_unfold   target cvt.s.w of an li/lui constant ours folded into a float literal
      zero_store_type a differing row next to a non-sp `sw zero` (float vs int typed zero store)
      store_width_cast an offset stored with another width only (m2c `*(int *)` on a short field)
      ptr_stride   aligned addiu adding K*r bytes where the target adds K (m2c `p += 4` on an int *)
      bit_test_hoist target `andi` of the old value before an in-place shift; ours shifts first
      no_tailcall  target `jal S` where ours sibcalls `j S` (do { S(..); } while (0))
      builtin_math ours `jal fabsf|sqrtf|..` where the target has more abs.s/sqrt.s (__builtin_*)
      shift_index  target `sll A; sra B` (A > B >= .., A >= 16) ours lacks ((e << 16) >> (16 - n))
      fadd_order   a float add/mul moved or with other operands (`x += K` -> `x = K + x`; family addu_swap)
      float_zero_store more target `mtc1 zero,$fN` (0.0f kept in an FPR + swc1; ours `sw zero`)
      early_float_const a differing `mtc1 at,` on both sides (a float constant built elsewhere)
      volatile_global target `nop` where ours fills the delay slot with a global's load
      union_store  a non-sp sw/sh moved among lwc1 rows (int store vs float loads: TBAA order)
      byte_offset_index same addiu / load / store with only the immediate differing (address split)"""
    tags: dict = {}
    cp = const_pairs(rows)
    if cp:
        tags["const_fix"] = cp
    zx = zext_shape(rows)
    if zx:
        tags["zext_form"] = zx
    for f, g in (("field_alias", field_alias_shape), ("volatile_mask", volatile_mask_shape),
                 ("loop_form", loop_form_shape), ("split_local", split_local_shape),
                 ("hoist_load", hoist_load_shape), ("sched_barrier", sched_barrier_shape),
                 ("param_widen", sext_shape), ("const_rev", const_store_shape),
                 ("addu_swap", addu_swap_shape), ("sign_flip", sign_shape),
                 ("float_cmp_neg", fcmp_shape), ("unsized_extern", gp_shape),
                 ("aligned_local", unaligned_shape), ("store_sort", store_order_shape),
                 ("block_barrier", block_barrier_shape), ("load_width_cast", load_width_shape)):
        v = g(rows)
        if v:
            tags[f] = v
    cm = cmp_mismatch(rows)
    if cm:
        tags["cmp_fix"] = cm
    bl = branch_likely_shape(rows)
    if bl:
        tags["fallthrough_bl"] = bl
    hx = hi_extra_shape(rows)
    if hx:
        tags["global_field_view"] = hx
    hm = hi_missing_shape(rows)
    if hm:
        tags["global_recast"] = hm
    kr = const_reg_shape(rows)
    if kr:
        tags["const_local"] = kr
    for f, g in (("cvt_unfold", cvt_const_shape), ("zero_store_type", zero_store_shape),
                 ("const_cmp", const_cmp_shape), ("store_width_cast", store_width_shape),
                 ("ptr_stride", ptr_stride_shape), ("bit_test_hoist", bit_test_shape),
                 ("no_tailcall", tailcall_shape), ("builtin_math", builtin_math_shape),
                 ("shift_index", shift_pair_shape), ("fadd_order", fadd_order_shape),
                 ("float_zero_store", fzero_shape), ("early_float_const", early_fconst_shape),
                 ("volatile_global", vol_delay_shape), ("union_store", union_store_shape),
                 ("byte_offset_index", addr_split_shape)):
        v = g(rows)
        if v:
            tags[f] = v
    mv = moved_insns(rows)
    if mv:
        win = diff_window(rows)
        if any(_mn(x) in ("movz", "movn") for x in win):
            tags["de_ifcvt"] = mv
        elif len(mv) == 1 and STORE_RE.match(_mn(mv[0])):
            tags["sched_weight"] = mv
        elif all(_mn(x) in ("addiu", "addu", "move", "li", "lui", "ori", "daddu", "sll") for x in mv):
            tags["loop_live"] = mv
    return tags


def text_shape(text: str, name: str, unit: str, rows, tu: str = "") -> list[str]:
    """Text-gated mechanism tags: void_ret (int falling off its end and the
    target's last block writes no v0, or the reverse) and lit_inline (an
    extern of a .lit4/.lit8 data symbol)."""
    tags = []
    ctx = {"rows": rows, "unit": unit, "tu": tu, "patches": {}}
    for f, t in (("void_ret", t_void_ret), ("lit_inline", t_lit_inline)):
        try:
            if next((1 for lab, _ in t(text, name, dict(ctx)) if family(lab) == f), None):
                tags.append(f)
        except Exception:  # noqa: BLE001
            pass
    loc = find_function(text, name)
    if loc:
        fn = text[loc[0]:loc[2] + 1]
        try:
            if next(iter(t_ret_local(fn)), None):
                tags.append("ret_local")
            if next(iter(t_copy_inline(text, name)), None):
                tags.append("copy_inline")
            for f, t in (("field_reread", t_field_reread), ("ptr_index", t_ptr_index),
                         ("bit_split", t_bit_split), ("early_return", t_early_return)):
                if next(iter(t(fn)), None):
                    tags.append(f)
        except Exception:  # noqa: BLE001
            pass
    return tags


# ---- cmp_fix: the C comparison is wrong -------------------------------------

_REL = re.compile(r"(?<![<>=!\-])(<=|>=|==|!=|<|>)(?![<>=])")
_RELOPS = ["<", "<=", ">", ">=", "==", "!="]


def _cond_spans(inner: str):
    """(start, end) of if / while / for-condition / ternary-condition text."""
    for m in re.finditer(r"\b(if|while)\s*\(", inner):
        cp = match_brace(inner, m.end() - 1)
        if cp > 0:
            yield m.end(), cp
    for m in re.finditer(r"\bfor\s*\(", inner):
        cp = match_brace(inner, m.end() - 1)
        if cp > 0:
            parts = inner[m.end():cp].split(";")
            if len(parts) == 3:
                s = m.end() + len(parts[0]) + 1
                yield s, s + len(parts[1])
    for m in re.finditer(r"[=(,]\s*\(?([^;?{}=]*?)\)?\s*\?", inner):
        yield m.start(1), m.end(1)


def _operand_end(s: str, i: int) -> int:
    """end of the operand starting at i (balanced, stops at && || ) ; , ?)."""
    depth = 0
    j = i
    while j < len(s):
        c = s[j]
        if c in "([":
            depth += 1
        elif c in ")]":
            if depth == 0:
                break
            depth -= 1
        elif depth == 0 and (c in ";,?:" or s.startswith("&&", j) or s.startswith("||", j)):
            break
        j += 1
    return j


# zero-compare branch -> condition class (a branch and its inverse test the same C relation)
_ZBR = {"bgez": "ge0", "bltz": "ge0", "blez": "gt0", "bgtz": "gt0", "beqz": "eq0", "bnez": "eq0"}


def _zclass(op: str, k: int) -> str | None:
    """class of `x OP k` as a compare with zero: x >= 1 is x > 0 (blez/bgtz)."""
    norm = {(">=", 1): ">", ("<", 1): "<=", ("<=", -1): "<", (">", -1): ">="}.get((op, k))
    if norm:
        op, k = norm, 0
    if k != 0:
        return None
    return {">=": "ge0", "<": "ge0", ">": "gt0", "<=": "gt0", "==": "eq0", "!=": "eq0"}[op]


def t_cmp_fix(fn, ctx=None):
    """Only when the diff shows a wrong branch condition (ctx["cmp"]): every
    comparison in a condition gets every relational operator and constant
    k-1 / k / k+1; bare truth tests get `> 0`, `< 0`, ...; if/else arms and
    ternary arms are swapped (condition kept). Zero-compare branches
    (bgez/blez/...) put comparisons against -1/0/1 first."""
    cm = (ctx or {}).get("cmp")
    if not cm:
        return
    zero = any(c.get("zero") for c in cm)
    cand_cls = {_ZBR.get(c["cand"]) for c in cm} - {None}
    tgt_cls = {_ZBR.get(c["target"]) for c in cm} - {None}
    imms = {x for c in cm for x in c.get("imm", [])}
    head, inner, tail = _split_fn(fn)
    out = []           # (priority, label, text)
    seen = set()
    for s, e in sorted(set(_cond_spans(inner))):
        cond = inner[s:e]
        for m in _REL.finditer(cond):
            op = m.group(1)
            a0 = m.start()
            b0 = m.end()
            while b0 < len(cond) and cond[b0] == " ":
                b0 += 1
            b1 = _operand_end(cond, b0)
            rhs = cond[b0:b1].rstrip()
            k = None
            mk = re.fullmatch(r"\(?\s*(-?(?:0x[0-9a-fA-F]+|\d+))[uUlL]*\s*\)?", rhs)
            if mk:
                k = int(mk.group(1), 0)
            consts = [k - 1, k, k + 1] if k is not None else [None]
            for nop in _RELOPS:
                for kk in consts:
                    if nop == op and kk == k:
                        continue
                    nr = rhs if kk is None else (hex(kk) if rhs.lstrip("(").strip().lower().startswith("0x") and kk >= 0
                                                 else str(kk))
                    ncond = cond[:a0].rstrip() + f" {nop} " + nr + cond[b0 + len(rhs):]
                    if ncond in seen:
                        continue
                    seen.add(ncond)
                    pr = 3
                    if k is not None and (zero and k in (-1, 0, 1) or k in imms or (kk is not None and kk in imms)):
                        pr = 2
                    if k is not None and kk is not None and _zclass(op, k) in cand_cls:
                        pr = 0 if _zclass(nop, kk) in tgt_cls else 1
                    out.append((pr, f"cmp_fix:{op}{rhs[:8]}->{nop}{nr[:8]}",
                                head + inner[:s] + ncond + inner[e:] + tail))
        # bare truth tests: if (x) / if (!x)
        mb = re.fullmatch(r"\s*(!?)\s*([A-Za-z_][\w.\->\[\]]*)\s*", cond)
        if mb:
            v = mb.group(2)
            for nop in ("<", "<=", ">", ">="):
                for kk in (0, 1, -1):
                    ncond = f"{v} {nop} {kk}"
                    out.append((1 if "eq0" in cand_cls else 2, f"cmp_fix:bare{mb.group(1)}{v[:8]}->{nop}{kk}",
                                head + inner[:s] + ncond + inner[e:] + tail))
    # swap then/else arms (condition unchanged)
    for m in re.finditer(r"\bif\s*\(", inner):
        cp = match_brace(inner, m.end() - 1)
        mm = re.match(r"\s*\{", inner[cp + 1:]) if cp > 0 else None
        if not mm:
            continue
        b1 = cp + mm.end()
        e1 = match_brace(inner, b1)
        me = re.match(r"\s*else\s*\{", inner[e1 + 1:]) if e1 > 0 else None
        if not me:
            continue
        b2 = e1 + me.end()
        e2 = match_brace(inner, b2)
        if e2 < 0:
            continue
        new = inner[:b1 + 1] + inner[b2 + 1:e2] + inner[e1:b2 + 1] + inner[b1 + 1:e1] + inner[e2:]
        out.append((2, f"cmp_fix:armswap:{inner[m.end():cp][:16]}", head + new + tail))
    for m in re.finditer(r"\?([^:;?]+):([^;?]+);", inner):
        new = inner[:m.start()] + f"? {m.group(2).strip()} : {m.group(1).strip()};" + inner[m.end():]
        out.append((2, "cmp_fix:ternswap", head + new + tail))
    out.sort(key=lambda x: x[0])
    for _, lab, t in out[:160]:
        yield lab, t


# ---- sched_weight: sched1 register-weight tie --------------------------------

def _scalar_locals(fn: str) -> list[tuple[str, str]]:
    head, inner, _ = _split_fn(fn)
    items, _, _ = _decl_block(inner)
    out = [(ty, decl_name(d)) for ty, d in items
           if "[" not in d.split("=")[0] and not (re.match(r"(struct|union)\b", ty) and not d.lstrip().startswith("*"))]
    pm = re.search(r"\((.*)\)\s*\{?\s*$", head, re.S)
    for prm in split_top(pm.group(1)) if pm else []:
        mm = re.match(r"(.*?)(\w+)\s*$", prm.strip())
        if mm and prm.strip() != "void":
            out.append((mm.group(1).strip(), mm.group(2)))
    return out


def _is_store(s: str) -> bool:
    m = re.match(r"\s*([^=;]+?)\s*(?<![=!<>+\-*/|&^%])=(?!=)", s)
    return bool(m and re.search(r"->|\[|\.|^\s*\*", m.group(1)))


def t_sched_weight(fn, ctx=None):
    """Single store moved by sched1's register-weight tie-break (the insn
    holding a variable's LAST use wins equal priority): change where the
    stored variables die. Moves statements using them (1-4 lines), splits
    chained stores `a = b = v;`, re-reads the stored lvalue instead of the
    variable in later statements, routes one store through a temp copy, and
    flips the variable's int/unsigned/float type."""
    if not (ctx or {}).get("sched_weight"):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    locs = dict((n, t) for t, n in _scalar_locals(fn))
    stores = [(i, s) for i, s in enumerate(lines) if _simple_stmt(s) and _is_store(s)]
    vars_ = set()
    for _, s in stores:
        rhs = s.split("=")[-1]
        vars_ |= _uses(rhs) & set(locs)
    if not vars_:
        return
    # 1. moves (shared with regalloc_tie)
    for lab, t in t_regalloc_tie(fn, {"tie_vars": sorted(vars_)}):
        yield "sched_weight:mv" + lab[len("regalloc_tie"):], t
    for i, s in stores:
        ind = s[:len(s) - len(s.lstrip())]
        parts = [p.strip() for p in s.strip().rstrip(";").split("=")]
        if any(p == "" for p in parts):
            continue
        v = parts[-1]
        if v not in vars_:
            continue
        # 2. split chained stores
        if len(parts) > 2:
            lhs = parts[:-1]
            for order, tagn in ((lhs[::-1], "rev"), (lhs, "fwd")):
                body = [f"{ind}{x} = {v};" for x in order]
                yield f"sched_weight:split{tagn}:L{i}", head + "\n".join(lines[:i] + body + lines[i + 1:]) + tail
                chain = [f"{ind}{order[0]} = {v};"] + [f"{ind}{x} = {order[0]};" for x in order[1:]]
                yield f"sched_weight:chain{tagn}:L{i}", head + "\n".join(lines[:i] + chain + lines[i + 1:]) + tail
        # 3. re-read the stored lvalue in later uses
        lv = parts[-2]
        for j in range(i + 1, min(len(lines), i + 12)):
            sj = lines[j]
            if not _simple_stmt(sj) or v not in _uses(sj.split("=")[-1]) or v in _writes(sj):
                continue
            pre, _, rhs = sj.rpartition("=")
            nr = re.sub(r"\b" + re.escape(v) + r"\b", lv, rhs)
            nl = lines[:]
            nl[j] = pre + "=" + nr
            yield f"sched_weight:reread:L{j}", head + "\n".join(nl) + tail
        # 4. temp copy for this store
        ty = locs.get(v, "int")
        if "*" not in ty and len(parts) == 2:
            nl = lines[:]
            nl[i] = f"{ind}{{ {ty} t_ = {v}; {parts[0]} = t_; }}"
            yield f"sched_weight:temp:L{i}", head + "\n".join(nl) + tail
    # 5. type flips of the stored variable
    for v in sorted(vars_):
        ty = locs.get(v, "")
        for nt in ("int", "unsigned int", "float", "long"):
            if ty and nt != ty and "*" not in ty and not ty.startswith(("struct", "union")):
                new = re.sub(r"(^|\n)(\s*)" + re.escape(ty) + r"(\s+)(" + re.escape(v) + r")\b",
                             lambda m: f"{m.group(1)}{m.group(2)}{nt}{m.group(3)}{m.group(4)}", inner, count=1)
                if new != inner:
                    yield f"sched_weight:type:{v}:{nt}", head + new + tail


def sched_ties(unit: str, tu: str, name: str, text: str, mem_only: bool = True) -> dict:
    """sched1 diagnosis: compile with -dS -dR -fsched-verbose=6, walk each
    block's dependency table (insn -> prio) and ready lists, and list the
    decisions where the chosen insn beat another ready insn of EQUAL
    priority (gcc then compares register weight: sets minus REG_DEAD deaths,
    then luid). Each tie carries both insns' REG_DEAD regs and patterns.
    Also counts conditional moves (set reg (if_then_else)) in .sched
    (pre-reload if-conversion) and .sched2 (cmov_sched1 / cmov_sched2): equal
    counts with a movz/movn in the diff = converted too early (de_ifcvt)."""
    d = compile_dumps(unit, tu, name, text, ["-dS", "-dR", "-fsched-verbose=6"])
    out: dict = {"ties": [], "error": d.get("error")}
    s1 = d["dumps"].get("sched", "")
    s2 = d["dumps"].get("sched2", "")
    cm = lambda x: len(re.findall(r"\(set(?::\w+)? \(reg[^)]*\)\s+\(if_then_else", x))  # noqa: E731
    out["cmov_sched1"] = cm(s1)
    out["cmov_sched2"] = cm(s2)
    # insn uid -> (pattern head, REG_DEAD regs) from the RTL listing
    rtl = {}
    for m in re.finditer(r"^\((?:insn|jump_insn|call_insn) (\d+) .*?(?=^\((?:insn|jump_insn|call_insn|note|code_label|barrier) |\Z)",
                         s1, re.M | re.S):
        body = m.group(0)
        dead = re.findall(r"REG_DEAD \(reg(?:/v)?:\w+ (\d+)", body)
        pat = " ".join(" ".join(body.split("\n")[:2]).split()[4:])[:100]
        rtl[int(m.group(1))] = {"dead": dead, "pat": pat}
    prio: dict = {}
    for ln in s1.splitlines():
        m = re.match(r";;\s+(\d+)\s+-?\d+\s+\d+\s+\d+\s+(\d+)\s+\d+\s", ln)
        if m:
            prio[int(m.group(1))] = int(m.group(2))
    ready = None
    for ln in s1.splitlines():
        m = re.match(r";;\s+Ready list \(t =\s*\d+\):\s*(.*)$", ln)
        if m:
            ready = [int(x.split("/")[0]) for x in m.group(1).split() if x.split("/")[0].isdigit()]
            continue
        m = re.search(r"--> scheduling insn <<<(\d+)>>>", ln)
        if m and ready:
            ch = int(m.group(1))
            for o in ready:
                pats = (rtl.get(ch, {}).get("pat", ""), rtl.get(o, {}).get("pat", ""))
                if mem_only and not any("(mem" in x for x in pats):
                    continue
                if o != ch and prio.get(o) is not None and prio.get(o) == prio.get(ch):
                    out["ties"].append({"chosen": ch, "other": o, "prio": prio[ch],
                                        "chosen_dead": rtl.get(ch, {}).get("dead"),
                                        "other_dead": rtl.get(o, {}).get("dead"),
                                        "chosen_pat": rtl.get(ch, {}).get("pat"),
                                        "other_pat": rtl.get(o, {}).get("pat")})
            ready = None
    return out


# ---- de_ifcvt: pre-reload if-conversion too early ---------------------------

def t_de_ifcvt(fn, ctx=None):
    """A min/max/clamp became a movz/movn before reload (in .sched) so the
    sched2 luid tie-break inherits sched1's order; the original stayed a branch
    until post-reload ifcvt. Rewrites `x = c ? a : b;` and `if (c) x = a;`
    into forms pre-reload ifcvt does not take: if/else statements, default
    then conditional overwrite, temp copy in the arm, two-statement arm,
    goto/label, unsigned compare; and the reverse (if-assign -> ternary)."""
    if not (ctx or {}).get("de_ifcvt"):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    locs = dict((n, t) for t, n in _scalar_locals(fn))
    tern = re.compile(r"^(\s*)([\w.\->\[\]]+)\s*=\s*\(?(.+?)\)?\s*\?\s*([^:;?]+?)\s*:\s*([^;?]+?)\s*;\s*$")
    for i, s in enumerate(lines):
        m = tern.match(s)
        if not m:
            continue
        ind, x, c, a, b = m.groups()
        ty = locs.get(x, "int")
        nc = f"!({c})"
        vs = {
            "ifelse": [f"{ind}if ({c}) {{", f"{ind}    {x} = {a};", f"{ind}}} else {{", f"{ind}    {x} = {b};", f"{ind}}}"],
            "dflt_b": [f"{ind}{x} = {b};", f"{ind}if ({c}) {{", f"{ind}    {x} = {a};", f"{ind}}}"],
            "dflt_a": [f"{ind}{x} = {a};", f"{ind}if ({nc}) {{", f"{ind}    {x} = {b};", f"{ind}}}"],
            "temp": [f"{ind}{x} = {b};", f"{ind}if ({c}) {{", f"{ind}    {ty} t_ = {a};", f"{ind}    {x} = t_;", f"{ind}}}"],
            "goto": [f"{ind}{x} = {a};", f"{ind}if ({c}) goto ifc_{i};", f"{ind}{x} = {b};", f"ifc_{i}:;"],
            "goto2": [f"{ind}if ({c}) {{", f"{ind}    {x} = {a};", f"{ind}    goto ifc_{i};", f"{ind}}}",
                      f"{ind}{x} = {b};", f"ifc_{i}:;"],
        }
        mu = re.match(r"\s*(\w+)\s*(<=|>=|<|>)\s*(.+)$", c)
        if mu:
            vs["unsigned"] = [f"{ind}{x} = ((unsigned int){mu.group(1)} {mu.group(2)} {mu.group(3)}) ? {a} : {b};"]
        for lab, rep in vs.items():
            yield f"de_ifcvt:{lab}:L{i}", head + "\n".join(lines[:i] + rep + lines[i + 1:]) + tail
    # if (c) x = a;   (single statement, no else)
    ifa = re.compile(r"^(\s*)if\s*\((.+)\)\s*\{?\s*([\w.\->\[\]]+)\s*=\s*([^;=]+);\s*\}?\s*$")
    for i, s in enumerate(lines):
        m = ifa.match(s)
        nxt = lines[i + 1].strip() if i + 1 < len(lines) else ""
        if not m or nxt.startswith("else"):
            continue
        ind, c, x, a = m.groups()
        ty = locs.get(x, "int")
        vs = {
            "tern": [f"{ind}{x} = ({c}) ? {a} : {x};"],
            "temp": [f"{ind}if ({c}) {{", f"{ind}    {ty} t_ = {a};", f"{ind}    {x} = t_;", f"{ind}}}"],
            "goto": [f"{ind}if (!({c})) goto ifc_{i};", f"{ind}{x} = {a};", f"ifc_{i}:;"],
            "else": [f"{ind}if ({c}) {{", f"{ind}    {x} = {a};", f"{ind}}} else {{", f"{ind}    {x} = {x};", f"{ind}}}"],
        }
        mu = re.match(r"\s*(\w+)\s*(<=|>=|<|>)\s*(.+)$", c)
        if mu:
            vs["unsigned"] = [f"{ind}if ((unsigned int){mu.group(1)} {mu.group(2)} {mu.group(3)}) {x} = {a};"]
        for lab, rep in vs.items():
            yield f"de_ifcvt:if{lab}:L{i}", head + "\n".join(lines[:i] + rep + lines[i + 1:]) + tail


# ---- loop_live: loop-preheader sched tie ------------------------------------

def _is_num(s: str) -> bool:
    return bool(re.fullmatch(r"-?(0x[0-9a-fA-F]+|\d+)[uUlL]*", s.strip()))


def t_loop_live(fn, ctx=None):
    """Preheader inits (strength-reduced addresses + countdown) reordered:
    sched1's tie-break follows what is live after the loop. Varies that:
    limit copied into a fresh variable, another lock-step counter as the exit
    test, `i = n;` after the loop (counter dead / limit live), counter reset
    after the loop, limit re-read after the loop."""
    if not (ctx or {}).get("loop_live"):
        return
    head, inner, tail = _split_fn(fn)
    for m in re.finditer(r"\bfor\s*\(", inner):
        cp = match_brace(inner, m.end() - 1)
        if cp < 0:
            continue
        parts = inner[m.end():cp].split(";")
        if len(parts) != 3:
            continue
        init, cond, step = parts
        mc = re.fullmatch(r"\s*(\w+)\s*(<|<=|!=)\s*(\w+)\s*", cond)
        if not mc:
            continue
        i, op, n = mc.groups()
        mb = re.match(r"\s*\{", inner[cp + 1:])
        if not mb:
            continue
        b = cp + mb.end()
        e = match_brace(inner, b)
        if e < 0:
            continue
        body = inner[b + 1:e]
        ls = inner.rfind("\n", 0, m.start()) + 1
        ind = inner[ls:m.start()] if not inner[ls:m.start()].strip() else "    "
        after = inner[e + 1:]
        pre = inner[:m.start()]
        loop = inner[m.start():e + 1]
        # limit copy
        if not _is_num(n):
            nl = loop.replace(cond, f" {i} {op} lim_", 1)
            yield f"loop_live:limcopy:{n}", head + pre + f"{{ int lim_ = {n};\n{ind}{nl}\n{ind}}}" + after + tail
        # lock-step counter as exit test
        for k in re.findall(r"\b(\w+)\s*\+\+\s*;", body):
            if k != i and re.search(r"\b" + k + r"\s*=\s*0\s*;", pre[-200:]):
                nl = loop.replace(cond, f" {k} {op} {n}", 1)
                yield f"loop_live:exitvar:{k}", head + pre + nl + after + tail
                yield f"loop_live:sync:{k}", head + pre + loop + f"\n{ind}{i} = {k};" + after + tail
        # liveness after the loop
        if not _is_num(n):
            yield f"loop_live:i=n:{i}", head + pre + loop + f"\n{ind}{i} = {n};" + after + tail
            yield f"loop_live:n=i:{n}", head + pre + loop + f"\n{ind}{n} = {i};" + after + tail
        yield f"loop_live:reset:{i}", head + pre + loop + f"\n{ind}{i} = 0;" + after + tail
        # countdown: separate remaining-count variable
        if not _is_num(n) and re.fullmatch(r"\s*" + i + r"\s*\+\+\s*", step):
            nl = loop.replace(cond, f" cnt_ != 0", 1).replace(step, f" {step.strip()}, cnt_--", 1)
            yield f"loop_live:countdown:{i}", head + pre + f"{{ int cnt_ = {n} - {i};\n{ind}{nl}\n{ind}}}" + after + tail


# ---- third-round mechanisms: const_fix, void_ret, lit_inline, zext_form ------
# TEXT transforms: (text, name, ctx) -> (label, whole new text). ctx carries
# "rows" (the try_fn diff), "unit", "tu" and a "patches" dict the void_ret
# family fills with source patches (forward declarations) keyed by the sha1 of
# the candidate text.

_IMM_MN = {"lui", "ori", "addiu", "daddiu", "li", "andi", "xori", "slti", "sltiu"}
_MEM_MN = re.compile(r"^(lb|lbu|lh|lhu|lw|lwu|ld|lq|lwc1|ldc1|sb|sh|sw|sd|sq|swc1|sdc1)$")
_LIT_RE = re.compile(r"(?<![\w.])(0[xX][0-9a-fA-F]+|\d+)([uUlL]*)(?![\w.])")


def _insn_ops(insn: str):
    """(mnemonic, [operands], has_reloc)"""
    rel = "[" in insn
    insn = re.sub(r"\s*\[.*$", "", insn).strip()
    p = insn.split(None, 1)
    if not p:
        return "", [], rel
    return p[0], [o.strip() for o in (p[1].split(",") if len(p) > 1 else [])], rel


def _ival(x: str):
    try:
        return int(x, 0)
    except ValueError:
        return None


def _full_const(col: list[str], i: int):
    """Value built by the lui at col[i] (+ the addiu/ori consuming it within 8
    insns), or by the addiu/ori at col[i] fed by a lui up to 8 insns back."""
    mn, ops, rel = _insn_ops(col[i])
    if rel:
        return None
    if mn == "lui" and len(ops) == 2:
        hi = _ival(ops[1])
        if hi is None:
            return None
        for j in range(i + 1, min(len(col), i + 9)):
            m2, o2, r2 = _insn_ops(col[j])
            if m2 in ("addiu", "ori") and len(o2) == 3 and o2[1] == ops[0] and not r2:
                lo = _ival(o2[2])
                if lo is None:
                    return None
                return ((hi << 16) + lo if m2 == "addiu" else (hi << 16) | lo) & 0xFFFFFFFF
            if o2 and o2[0] == ops[0]:
                break
        return (hi << 16) & 0xFFFFFFFF
    if mn in ("addiu", "ori") and len(ops) == 3:
        lo = _ival(ops[2])
        for j in range(i - 1, max(-1, i - 9), -1):
            m2, o2, r2 = _insn_ops(col[j])
            if m2 == "lui" and o2 and o2[0] == ops[1] and not r2:
                hi = _ival(o2[1])
                if hi is None or lo is None:
                    return None
                return ((hi << 16) + lo if mn == "addiu" else (hi << 16) | lo) & 0xFFFFFFFF
            if o2 and o2[0] == ops[1]:
                break
    return None


def const_pairs(rows) -> list[tuple[int, int]]:
    """(candidate value, target value) for aligned rows that differ only in
    an immediate (lui/ori/addiu/li/andi/slti(u)) or a load/store offset;
    lui+ori / lui+addiu pairs give the full 32-bit constant."""
    dr = diff_rows(rows)
    ta = [a for a, _, _ in dr]
    cb = [b for _, _, b in dr]
    out: list[tuple[int, int]] = []
    for i, (a, m, b) in enumerate(dr):
        if m != "|" or not a or not b:
            continue
        # only locally aligned rows: an insertion / deletion nearby means the
        # pairing is a scheduling artefact, not a wrong constant
        if any(x[1] in "<>" for x in dr[max(0, i - 4):i + 5]):
            continue
        ma, oa, ra = _insn_ops(a)
        mb, ob, rb = _insn_ops(b)
        if ma != mb or ra or rb or len(oa) != len(ob) or not oa:
            continue
        if ma in _IMM_MN and oa[:-1] == ob[:-1]:
            va, vb = _ival(oa[-1]), _ival(ob[-1])
            if va is None or vb is None or va == vb:
                continue
            fa, fb = _full_const(ta, i), _full_const(cb, i)
            if fa is not None and fb is not None and fa != fb:
                out.append((fb, fa))
            if ma == "lui":
                out.append(((vb << 16) & 0xFFFFFFFF, (va << 16) & 0xFFFFFFFF))
            else:
                out.append((vb, va))
                out.append((-vb, -va))
        elif _MEM_MN.match(ma) and oa[0] == ob[0] and len(oa) == 2:
            pa = re.fullmatch(r"(-?\d+)\((\w+)\)", oa[1])
            pb = re.fullmatch(r"(-?\d+)\((\w+)\)", ob[1])
            if pa and pb and pa.group(2) == pb.group(2) and pa.group(1) != pb.group(1):
                out.append((int(pb.group(1)), int(pa.group(1))))
    seen, res = set(), []
    for p in out:
        if p not in seen and p[0] != p[1]:
            seen.add(p)
            res.append(p)
    return res


def _fmt_like(orig: str, v: int, suf: str) -> str:
    """V written like ORIG (hex / decimal, digit case) with suffix SUF."""
    if not orig.lower().startswith("0x"):
        return str(v) + suf
    h = format(abs(v), "X" if any(c.isupper() for c in orig[2:]) else "x")
    return ("-" if v < 0 else "") + orig[:2] + h + suf


def t_const_fix(text, name, ctx=None):
    """Body copied from a sibling kept the sibling's constant (subPrintRibbon:
    0x20654000 vs 0x20A64000): every C literal equal to the candidate's value
    of a differing immediate is replaced by the target's value (each
    occurrence, then all of them)."""
    pairs = (ctx or {}).get("const")
    if not pairs:
        return
    loc = find_function(text, name)
    lo, hi = (0, len(text)) if not loc else (0, loc[2] + 1)
    lits = [(m.start(1), m.end(2), m.group(1), m.group(2)) for m in _LIT_RE.finditer(text, lo, hi)]
    n = 0
    for cv, tv in pairs:
        hits = []
        for s, e, lit, suf in lits:
            v = int(lit, 0)
            if v == cv:
                nv = tv
            elif v == cv & 0xFFFFFFFF:           # unsigned spelling of a negative value
                nv = tv & 0xFFFFFFFF
            else:
                continue
            hits.append((s, e, _fmt_like(lit, nv, suf)))
        for s, e, rep in hits:
            n += 1
            yield f"const_fix:{text[s:e]}->{rep}", text[:s] + rep + text[e:]
        if len(hits) > 1:
            t2 = text
            for s, e, rep in sorted(hits, reverse=True):
                t2 = t2[:s] + rep + t2[e:]
            yield f"const_fix:all{hex(cv)}->{hex(tv)}", t2
        if n > 60:
            return


# void_ret ------------------------------------------------------------------

_V0W = re.compile(r"^(?!s[bhwdq]\b|swc1|sdc1|b|j|jr|jal|nop|mt|ctc)\w+\s+v0\b")


def _last_block_sets_v0(col: list[str]) -> bool | None:
    """Is v0 live out of the final block (written after the last call /
    branch and not read again before `jr ra` + delay slot)? A function that
    only uses v0 as a scratch register returns no value. None when there is
    no jr ra."""
    idx = [i for i, x in enumerate(col) if re.match(r"jr\s+ra\b", x)]
    if not idx:
        return None
    j = idx[-1]
    blk = []
    for i in range(j - 1, -1, -1):
        mn = _mn(col[i])
        if BRANCH_RE.match(mn) or mn in ("j", "jal", "jalr", "jr"):
            break
        blk.append(col[i])
    blk = blk[::-1] + ([col[j + 1]] if j + 1 < len(col) else [])
    live = False
    for x in blk:
        mn, ops, _ = _insn_ops(x)
        if any(re.search(r"\bv0\b", o) for o in (ops[1:] if not STORE_RE.match(mn) else ops)):
            live = False                   # read: the write was a scratch use
        if _V0W.match(x):
            live = True
    return live


def _fn_rettype(fn: str, name: str) -> str | None:
    hdr = fn[:fn.index("{")]
    m = re.search(r"\b" + re.escape(name) + r"\s*\(", hdr)
    return hdr[:m.start()].strip() if m else None


def _decl_patches(unit: str, tu: str, name: str, old_t: str, new_t: str) -> list | None:
    """Forward declarations of NAME with return type OLD_T in the TU and the
    src/<unit> headers it includes: [(relpath, old line, new line)];
    None when one lives where it cannot be evaluated (include/)."""
    src = ROOT / "src" / unit / f"{tu}.c"
    try:
        tu_text = src.read_text()
    except OSError:
        return []
    files = [src]
    for inc in re.findall(r'^\s*#\s*include\s+"([^"]+)"', tu_text, re.M):
        p = (src.parent / inc).resolve()
        if p.is_file():
            files.append(p)
        elif (ROOT / "include" / inc).is_file() and name in (ROOT / "include" / inc).read_text(errors="replace"):
            return None
    out = []
    dre = re.compile(r"^([^\n;{}()#=]*?)\b" + re.escape(name) + r"\s*\(([^;{}]*)\)\s*;", re.M)
    for f in files:
        for m in dre.finditer(f.read_text(errors="replace")):
            rt = m.group(1).strip()
            words = rt.split()
            if not words or "return" in words:
                continue
            core = " ".join(w for w in words if w not in ("static", "extern", "inline"))
            if core.replace(" ", "") != old_t.replace(" ", "").replace("static", "").replace("extern", ""):
                continue
            old = m.group(0)
            new = old.replace(rt, rt.replace(core, new_t) if core in rt else rt, 1)
            out.append((str(f.relative_to(ROOT)), old, new))
    return out


def t_void_ret(text, name, ctx=None):
    """An int function falling off its end (no `return <expr>`) gets a
    clobber/use of v0 that reorders the last block in sched2
    (_SetVersion3Extent): flip it to void when the target's last block writes
    no v0. Reverse: a void function whose target sets v0 at the end becomes
    int returning the last call / assigned value. Conflicting forward
    declarations are flipped too (evaluated through try_fn overrides, applied
    with verified_edit before integrating)."""
    ctx = ctx or {}
    rows = ctx.get("rows")
    yield from _callee_void(text, name, ctx)          # specSyoukanProc (o477): callee prototypes
    loc = find_function(text, name)
    if not rows or not loc:
        return
    s, b, e = loc
    fn = text[s:e + 1]
    rt = _fn_rettype(fn, name)
    if rt is None:
        return
    dr = diff_rows(rows)
    if not any(m != " " for _, m, _ in dr):
        return
    tv0 = _last_block_sets_v0([a for a, _, _ in dr if a])
    cv0 = _last_block_sets_v0([c for _, _, c in dr if c])
    core = " ".join(w for w in rt.split() if w not in ("static", "extern", "inline"))
    _, inner, _ = _split_fn(fn)
    outs = []
    if core in ("int", "s32", "unsigned int", "u32", "short", "char", "long") and \
            not re.search(r"\breturn\s+[^;\s]", inner) and tv0 is False:
        nf = fn[:fn.index(rt)] + rt.replace(core, "void") + fn[fn.index(rt) + len(rt):]
        outs.append(("void_ret:int->void", nf, core, "void"))
    elif core == "void" and tv0 and not cv0:
        nrt = rt.replace("void", "int")
        base = fn[:fn.index(rt)] + nrt + fn[fn.index(rt) + len(rt):]
        bi = base.rindex("}")
        lines = base[:bi].rstrip().splitlines()
        last = lines[-1] if lines else ""
        m1 = re.match(r"(\s*)([A-Za-z_]\w*)\s*=[^=].*;\s*$", last)
        m2 = re.match(r"(\s*)([A-Za-z_]\w*\s*\(.*\)\s*;)\s*$", last)
        if m1:
            outs.append(("void_ret:void->int:ret-" + m1.group(2), base[:bi].rstrip() + f"\n{m1.group(1)}return {m1.group(2)};\n}}", "void", "int"))
        if m2 and not m2.group(2).startswith(("if", "while", "for", "switch", "return")):
            nb = "\n".join(lines[:-1] + [f"{m2.group(1)}return {m2.group(2)}"])
            outs.append(("void_ret:void->int:retcall", nb + "\n}", "void", "int"))
    # eMessageMain / MenuInfoWindow / MenuSelectWindow: a void function ending in
    # a call is tail-called (`j f`) where the target does `jal f` + epilogue
    # because the original is int falling off its end; flip to int, no return.
    if core == "void" and not outs:
        cand = [c for _, _, c in dr if c]
        targ = [a for a, _, _ in dr if a]
        if any(re.search(r"\bj\s+(?!\$|ra\b)[A-Za-z_]", c) for c in cand[-4:]) and \
                any(re.search(r"\bjal\b", a) for a in targ[-8:]):
            nrt = rt.replace("void", "int")
            outs.append(("void_ret:void->int:no-tailcall",
                         fn[:fn.index(rt)] + nrt + fn[fn.index(rt) + len(rt):], "void", "int"))
    pat = ctx.setdefault("patches", {})
    for lab, nf, ot, nt in outs:
        new = text[:s] + nf + text[e + 1:]
        # forward declarations inside the body text itself
        new = re.sub(r"^([^\n;{}()#=]*?)\b" + re.escape(ot) + r"(\s+\**\s*" + re.escape(name) + r"\s*\([^;{}]*\)\s*;)",
                     lambda m: m.group(1) + nt + m.group(2), new, flags=re.M)
        pp = _decl_patches(ctx.get("unit", ""), ctx.get("tu", ""), name, ot, nt) if ctx.get("unit") else []
        if pp is None:
            continue
        if pp:
            pat[body_sha(new)] = pp
        yield lab, new


# lit_inline ----------------------------------------------------------------

def lit_literal(info: dict, as_double: bool = False) -> str | None:
    """Shortest decimal literal that rounds exactly to the data word."""
    import struct
    if info["kind"] == "float":
        raw = info["bits"].to_bytes(4, "little")
        v = struct.unpack("<f", raw)[0]
        if v != v or v in (float("inf"), float("-inf")):
            return None
        for p in range(1, 10):
            s = f"{v:.{p}g}"
            if struct.pack("<f", float(s)) == raw:
                break
        suf = "f"
    else:
        raw = info["bits"].to_bytes(8, "little")
        v = struct.unpack("<d", raw)[0]
        if v != v or v in (float("inf"), float("-inf")):
            return None
        for p in range(1, 18):
            s = f"{v:.{p}g}"
            if struct.pack("<d", float(s)) == raw:
                break
        suf = ""
    if "e" not in s and "." not in s:
        s += ".0"
    elif "e" in s and "." not in s.split("e")[0]:
        s = s.replace("e", ".0e")
    s += suf
    return f"({s})" if s.startswith("-") else s


def _lit_inline_fwd(text, name, ctx=None):
    """`extern float D_xxx` for a value living in .lit4/.lit8 (a symbol of a
    *.lit4.s / *.lit8.s data file) -> the float/double literal (checkTalk):
    an extern load is a pseudo SET (+1 register weight) and changes sched1's
    order; a const_double does not. Matches show up as try_fn
    match_after_link (lit_literal) -> integrate --skip-try."""
    ctx = ctx or {}
    unit = ctx.get("unit")
    loc = find_function(text, name)
    if not unit or not loc:
        return
    import try_fn
    syms = try_fn.lit_symbols(unit)
    if not syms:
        return
    s, b, e = loc
    fn = text[s:e + 1]
    used = [x for x in dict.fromkeys(re.findall(r"\bD_[0-9A-Fa-f]{8}\b", fn)) if x in syms]
    if not used:
        return
    try:
        tu_text = (ROOT / "src" / unit / f"{ctx.get('tu')}.c").read_text(errors="replace")
    except OSError:
        tu_text = ""
    reps = {}
    for x in used:
        m = re.search(r"^\s*extern\s+(?:const\s+)?(float|double|f32|f64)\s+" + x + r"\s*;", text + "\n" + tu_text, re.M)
        if not m:
            continue
        if re.search(r"&\s*" + x + r"\b|\b" + x + r"\s*(=[^=]|\+\+|--|\[)", fn):
            continue
        lit = lit_literal(syms[x])
        if lit:
            reps[x] = lit

    def apply(keys):
        nf = fn
        for x in keys:
            nf = re.sub(r"\b" + x + r"\b", reps[x], nf)
        new = text[:s] + nf + text[e + 1:]
        for x in keys:
            rest = re.sub(r"^\s*extern\s+(?:const\s+)?(?:float|double|f32|f64)\s+" + x + r"\s*;", "", new, flags=re.M)
            if not re.search(r"\b" + x + r"\b", rest):
                new = re.sub(r"^\s*extern\s+(?:const\s+)?(?:float|double|f32|f64)\s+" + x + r"\s*;[ \t]*\n", "", new, flags=re.M)
        return new
    if not reps:
        return
    yield "lit_inline:all", apply(list(reps))
    if len(reps) > 1:
        for x in reps:
            yield f"lit_inline:{x}", apply([x])


_FLIT = re.compile(r"(?<![\w.])(?P<n>(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?)(?P<f>[fF]?)(?![\w.])")


def lit_extern_forms(text, name, ctx):
    """Fold13 (o545 DrawZeldaShadowSub: its best body with float literals 0.9222,
    the same body reading the target's `.lit4` symbols through `extern float
    D_x;` 0.9444): the reverse of lit_inline. A float/double literal of the
    function equal to a .lit4/.lit8 symbol the target .s references (and the
    body does not) -> that symbol, `extern float|double D_x;` added at the
    top of the body file; one symbol at a time, then all."""
    import struct
    import try_fn
    unit = ctx.get("unit")
    loc = find_function(text, name)
    if not unit or not loc:
        return
    syms = try_fn.lit_symbols(unit)
    want = [x for x in sorted(_asm_syms(unit, ctx.get("tu"), name)) if x in syms]
    s, b, e = loc
    fn = text[s:e + 1]
    want = [x for x in want if not re.search(r"\b" + x + r"\b", text)]
    if not want:
        return
    vals = {}
    for x in want:
        info = syms[x]
        if info.get("bits") is None:
            continue
        if info["kind"] == "float":
            vals[x] = ("float", struct.unpack("<f", info["bits"].to_bytes(4, "little"))[0])
        else:
            vals[x] = ("double", struct.unpack("<d", info["bits"].to_bytes(8, "little"))[0])
    hb = fn.index("{")
    reps = {}
    for m in _FLIT.finditer(fn, hb):
        if "." not in m.group("n") and "e" not in m.group("n").lower():
            continue
        isf = bool(m.group("f"))
        a = m.start()
        pre = fn[:a].rstrip()
        neg = pre.endswith("-") and re.search(r"(?:[=(,?:]|return)\s*-$", pre) is not None
        v = float(m.group("n"))
        hit = None
        for sg, st in (((-1, pre.rfind("-")),) if neg else ()) + ((1, a),):
            for x, (k, tv) in vals.items():
                if (k == "float") != isf:
                    continue
                vv = sg * v
                if (struct.pack("<f", vv) == struct.pack("<f", tv)) if k == "float" else vv == tv:
                    hit = (x, st, k)
                    break
            if hit:
                break
        if hit:
            reps.setdefault(hit[0], []).append((hit[1], m.end(), hit[2]))
    if not reps:
        return

    def apply(keys):
        spans = sorted(((st, en, x) for x in keys for st, en, _ in reps[x]), reverse=True)
        nf = fn
        for st, en, x in spans:
            nf = nf[:st] + x + nf[en:]
        decl = "".join(f"extern {reps[x][0][2]} {x};\n" for x in keys)
        return decl + text[:s] + nf + text[e + 1:]
    yield "lit_inline:extern:all", apply(list(reps))
    if len(reps) > 1:
        for x in reps:
            yield f"lit_inline:extern:{x}", apply([x])


def t_lit_inline(text, name, ctx=None):
    """Both directions (fold13): .lit4/.lit8 externs -> literals (_lit_inline_fwd,
    checkTalk), and literals -> the target's .lit symbols (lit_extern_forms,
    o545 DrawZeldaShadowSub 0.9222 -> 0.9444)."""
    yield from _lit_inline_fwd(text, name, ctx)
    if ctx and ctx.get("unit"):
        yield from lit_extern_forms(text, name, ctx)


# zext_form -----------------------------------------------------------------

def zext_shape(rows) -> list[dict]:
    """Target `andi x,x,0xffff` followed by `sltiu y,x,N` inside the differing
    region (we compare without the zero-extend: slti / no andi)."""
    dr = diff_rows(rows)
    out = []
    for i, (a, m, b) in enumerate(dr):
        ma, oa, _ = _insn_ops(a)
        if ma != "andi" or len(oa) != 3 or _ival(oa[2]) != 0xFFFF:
            continue
        near = dr[max(0, i - 3):i + 4]
        if all(mm == " " for _, mm, _ in near):
            continue
        for j in range(i + 1, min(len(dr), i + 4)):
            mj, oj, _ = _insn_ops(dr[j][0])
            if mj == "sltiu" and len(oj) == 3 and oj[1] == oa[0]:
                c = None
                for k in range(i - 1, max(-1, i - 4), -1):
                    mk, ok, _ = _insn_ops(dr[k][0])
                    if mk == "addiu" and ok and ok[0] == oa[1]:
                        c = _ival(ok[2])
                        break
                out.append({"N": _ival(oj[2]), "C": c})
                break
    return out


def _operand_start(s: str, i: int) -> int:
    """start of the operand ending just before i (balanced, stops at ( && || ! ,)."""
    depth = 0
    j = i - 1
    while j >= 0:
        c = s[j]
        if c in ")]":
            depth += 1
        elif c in "([":
            if depth == 0:
                break
            depth -= 1
        elif depth == 0 and (c in ",!?:" or s[max(0, j - 1):j + 1] in ("&&", "||")):
            break
        j -= 1
    return j + 1


_ZFORMS = [
    ("stmtexpr", "({{ unsigned short _z = ({E}); _z; }})"),
    ("andmask", "(({E}) & 0xFFFF)"),
    ("uandmask", "(unsigned int)(({E}) & 0xFFFF)"),
    ("intcast", "(int)(unsigned short)({E})"),
    ("ucast", "(unsigned short)({E})"),
    ("union", "({{ union {{ short s; unsigned short u; }} _z; _z.s = ({E}); _z.u; }})"),
    ("vol", "({{ volatile unsigned short _z = ({E}); _z; }})"),
    ("twice", "({{ unsigned short _z = ({E}); _z = _z; _z; }})"),
    ("bitf", "({{ struct {{ unsigned int v : 16; }} _z; _z.v = ({E}); _z.v; }})"),
]


def t_zext_form(text, name, ctx=None):
    """Target keeps `addiu; andi 0xffff; sltiu` where every simple C form
    loses the zero-extend or the unsigned compare (InitMapTrap): rewrite the
    compared value into forms that stop combine from eliding the andi (temp,
    union, bitfield, masked twice). Pre-screened with snippet.py: only forms
    whose standalone asm keeps andi+sltu are compiled for real."""
    ctx = ctx or {}
    zs = ctx.get("zext")
    loc = find_function(text, name)
    if not zs or not loc:
        return
    s0, b0, e0 = loc
    fn = text[s0:e0 + 1]
    head, inner, tail = _split_fn(fn)
    Ns = {z["N"] for z in zs if z.get("N") is not None}
    Cs = {z["C"] for z in zs if z.get("C") is not None}
    ok_forms = None
    try:
        import snippet
        ok_forms = []
        c = abs(next(iter(Cs))) if Cs else 1
        n = next(iter(Ns)) if Ns else 8
        for lab, f in _ZFORMS:
            code = ("extern void g(void); void f(short x){ if (" + f.format(E=f"x - {c}") + f" < {n}) g(); }}")
            try:
                ins = snippet.compile_snippet(ctx["unit"], ctx["tu"], code)
                if snippet.has_seq(ins, [r"^andi .*0xffff", r"^sltu"]):
                    ok_forms.append(lab)
            except Exception:  # noqa: BLE001
                ok_forms.append(lab)
    except Exception:  # noqa: BLE001
        ok_forms = None
    out = []
    for s, e in sorted(set(_cond_spans(inner))):
        cond = inner[s:e]
        for m in _REL.finditer(cond):
            op = m.group(1)
            b1 = m.end()
            while b1 < len(cond) and cond[b1] == " ":
                b1 += 1
            b2 = _operand_end(cond, b1)
            rk = _ival(re.sub(r"[uUlL()\s]", "", cond[b1:b2]))
            if rk is None or not ({rk, rk + 1} & Ns or not Ns):
                continue
            a0 = _operand_start(cond, m.start())
            lhs = cond[a0:m.start()].strip()
            if not lhs:
                continue
            E = re.sub(r"^\(\s*(?:unsigned\s+short|u16|short|s16|signed\s+short)\s*\)\s*", "", lhs)
            for lab, f in _ZFORMS:
                if ok_forms is not None and lab not in ok_forms:
                    continue
                ncond = cond[:a0] + (" " if a0 and cond[a0 - 1] not in "( " else "") + f.format(E=E) + " " + cond[m.start():]
                out.append((f"zext_form:{lab}", head + inner[:s] + ncond + inner[e:] + tail))
    for lab, nf in out[:60]:
        yield lab, text[:s0] + nf + text[e0 + 1:]


# ---- fourth round: field_alias / volatile_mask / loop_form -----------------

_LOAD_W = {"sw": "lw", "sh": "lh", "sb": "lb", "sd": "ld"}


def field_alias_shape(rows) -> list[str]:
    """Pure reorder inside the diff that moves a sw/sh/sb through a non-sp
    base while the function also loads a global (relocated load) of the same
    width (cmdCamLock: int field stores aliasing `int camMode`)."""
    mv = moved_insns(rows)
    if not mv:
        return []
    st = []
    for x in mv:
        m, ops, _ = _insn_ops(x)
        if m in ("sw", "sh", "sb") and ops and not re.search(r"\((sp|fp)\)", x):
            st.append(m)
    if not st:
        return []
    tcol = [a for a, _, _ in diff_rows(rows) if a]
    out = []
    for m in dict.fromkeys(st):
        ld = _LOAD_W[m]
        if any(re.match(rf"^{ld}u?\s", a) and "[" in a for a in tcol):
            out.append(m)
    return out


def volatile_mask_shape(rows) -> list[str]:
    """Only an epilogue restore (ld/lw sN|ra|fp off sp) moved, inside a run of
    stores (xglRenderClearEnvInit: volatile stores chained by output deps let
    the independent `ld s1` jump between them)."""
    mv = moved_insns(rows)
    if not mv or len(mv) != 1 or not re.match(r"^(ld|lw|lq)\s+(s\d|ra|fp),-?\d+\(sp\)", mv[0]):
        return []
    win = diff_window(rows, pad=3)
    if sum(1 for x in win if STORE_RE.match(_mn(x)) and "(sp)" not in x) < 2:
        return []
    return mv


def loop_form_shape(rows) -> list[dict]:
    """Target loop bottom `move rX,rY; b(n)ez rX; andi rX,rX,mask` (the exit
    test duplicated by jump.c after a while loop is rotated) where we have
    `andi; b(n)ez` (UmlTest)."""
    dr = diff_rows(rows)
    out = []
    for i in range(len(dr) - 2):
        a0, a1, a2 = dr[i][0], dr[i + 1][0], dr[i + 2][0]
        m0, o0, _ = _insn_ops(a0)
        m1, o1, _ = _insn_ops(a1)
        m2, o2, _ = _insn_ops(a2)
        if (m0 == "move" and m1 in ("bnez", "beqz", "bnezl", "beqzl") and m2 == "andi" and len(o0) == 2
                and o1 and o1[0] == o0[0] and len(o2) == 3 and o2[0] == o2[1] == o0[0]
                and any(m != " " for _, m, _ in dr[i:i + 3])):
            near = [c for _, _, c in dr[max(0, i - 1):i + 3] if c]
            if any(_mn(c) == "andi" for c in near):
                out.append({"reg": o0[0], "mask": o2[2]})
    return out


def _stmt_lhs(ln: str):
    """(indent, lhs, rhs) of a plain `lhs = rhs;` statement, else None."""
    m = re.match(r"^(\s*)([^=;{}]+?)\s*(?<![=!<>+\-*/|&^%])=(?!=)\s*(.+?);\s*$", ln)
    if not m or not _simple_stmt(ln):
        return None
    return m.group(1), m.group(2).strip(), m.group(3).strip()


def t_field_alias(fn, ctx=None):
    """Struct field stores scheduled too early because an int store may alias
    a global int loaded right after (sched2 memory dep store -> global load ->
    call): store the field through a type out of int's alias set, one field
    at a time then in pairs. Two spellings: a cast lvalue `*(void **)&lhs`
    and a one-field local view struct (keeps MEM_IN_STRUCT_P)."""
    fa = (ctx or {}).get("field_alias")
    if not fa or "sw" not in fa:
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    idx = []
    for i, ln in enumerate(lines):
        p = _stmt_lhs(ln)
        if p and re.search(r"->|\.\s*[A-Za-z_]", p[1]) and not p[1].startswith("*") \
                and not re.search(r"\b(float|double)\b|\d\.\d|\d+\.\d*f\b", p[2]):
            idx.append(i)
    forms = [("cast", "{i}*(void **)&({l}) = (void *)({r});"),
             ("view", "{i}((struct {{ void *v; }} *)&({l}))->v = (void *)({r});")]
    sets = [[i] for i in idx] + [[a, b] for k, a in enumerate(idx) for b in idx[k + 1:]][:40]
    if len(idx) > 2:
        sets.append(idx)
    for fl, f in forms:
        for s in sets:
            nl = list(lines)
            for i in s:
                ind, l, r = _stmt_lhs(nl[i])
                nl[i] = f.format(i=ind, l=l, r=r)
            yield f"field_alias:{fl}:" + "+".join(str(i) for i in s), head + "\n".join(nl) + tail


def _ptr_base_type(fn: str, var: str) -> str | None:
    m = re.search(r"(?:^|[;{(,\n])\s*((?:const\s+|volatile\s+|struct\s+|unsigned\s+|signed\s+)*\w+)\s*"
                  r"((?:volatile\s+)?)\*\s*(?:volatile\s+)?" + re.escape(var) + r"\s*[;,=)]", fn)
    if not m:
        return None
    t = re.sub(r"\bvolatile\s+", "", m.group(1)).strip()
    return t


def t_volatile_mask(fn, ctx=None):
    """Per-store volatile mask over the trailing run of k <= 8 stores: sched2's
    rank_for_schedule prefers an insn independent of the last one scheduled
    over one with an output/anti dep, so which stores are volatile decides
    where the epilogue `ld sN` lands. Masks with two consecutive volatile
    stores are pruned; each kept mask is also tried with one adjacent swap."""
    if not (ctx or {}).get("volatile_mask"):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    j = len(lines) - 1
    while j >= 0 and (not lines[j].strip() or re.match(r"\s*return\s*;", lines[j])):
        j -= 1
    run = []
    while j >= 0 and len(run) < 8:
        p = _stmt_lhs(lines[j])
        if not p:
            break
        m = re.match(r"^(\w+)\s*(->|\.)\s*(.+)$", p[1])
        if not m:
            break
        run.insert(0, (j, p, m))
        j -= 1
    if len(run) < 2:
        return
    def form(p, m, vol):
        ind, l, r = p
        b, op, rest = m.groups()
        if op == "->":
            t = _ptr_base_type(fn, b)
            ty = t if t else f"__typeof__(*{b})"
            return f"{ind}(({'volatile ' if vol else ''}{ty} *){b})->{rest} = {r};"
        return f"{ind}(({'volatile ' if vol else ''}__typeof__({b}) *)&{b})->{rest} = {r};"
    k = len(run)
    masks = [mk for mk in range(1 << k) if not (mk & (mk >> 1))]
    outs = []
    for mk in masks:
        nl = list(lines)
        for q, (i, p, m) in enumerate(run):
            nl[i] = form(p, m, bool(mk >> q & 1))
        outs.append((f"volatile_mask:{mk:0{k}b}", nl))
    for lab, nl in outs:
        yield lab, head + "\n".join(nl) + tail
    for lab, nl in outs:
        for q in range(k - 1):
            a, b = run[q][0], run[q + 1][0]
            sl = list(nl)
            sl[a], sl[b] = sl[b], sl[a]
            yield f"{lab}:swap{q}", head + "\n".join(sl) + tail


def _paren_after(s: str, i: int):
    """(inner, end) of the balanced (...) starting at or after s[i]."""
    o = s.index("(", i)
    c = match_brace(s, o)
    return s[o + 1:c], c


def t_loop_form(fn, ctx=None):
    """`goto L; do { B1 L: B2 } while (C);` -> rotated while forms so jump.c
    duplicates the exit test at the loop bottom (UmlTest: target keeps
    `move; bnez; andi` where the goto-entered do-while folds the zero-extend):
    `B2; while (C) {B1 B2}`, `for (B2; C; B2) {B1}`, `while ((B2, C)) {B1}`,
    and the while form with an extra label / goto inside the body (stops
    loop.c invariant hoisting). `for(;;){ B2 if (!(C)) break; B1 }` likewise."""
    if not (ctx or {}).get("loop_form"):
        return
    head, inner, tail = _split_fn(fn)
    for m in re.finditer(r"\bgoto\s+(\w+)\s*;\s*do\s*\{", inner):
        lab = m.group(1)
        ob = m.end() - 1
        cb = match_brace(inner, ob)
        if cb < 0:
            continue
        wm = re.match(r"\s*while\s*", inner[cb + 1:])
        if not wm:
            continue
        cond, ce = _paren_after(inner, cb + 1 + wm.end())
        sm = re.match(r"\s*;", inner[ce + 1:])
        if not sm:
            continue
        end = ce + 1 + sm.end()
        body = inner[ob + 1:cb]
        lm = None
        for mm in re.finditer(r"(?m)^\s*" + re.escape(lab) + r"\s*:\s*;?", body):
            pre = body[:mm.start()]
            if pre.count("{") == pre.count("}"):
                lm = mm
        if not lm:
            continue
        b1 = body[:lm.start()].rstrip()
        b2 = body[lm.end():].strip()
        b2s = [x.strip() for x in b2.split(";") if x.strip()]
        if any("{" in x or "}" in x or re.match(r"(if|for|while|return|goto|break|continue)\b", x) for x in b2s):
            continue
        b2txt = "".join(x + "; " for x in b2s)
        comma = ", ".join(b2s)
        pre_s, post_s = inner[:m.start()], inner[end:]
        ind = re.search(r"[ \t]*$", pre_s).group(0)
        vs = [("while", f"{b2txt}while ({cond}) {{{b1}\n{ind}  {b2txt}\n{ind}}}"),
              ("while-label", f"{b2txt}while ({cond}) {{\n{ind}  {lab}_top: ;{b1}\n{ind}  {b2txt}\n{ind}}}"),
              ("while-goto", f"{b2txt}while ({cond}) {{{b1}\n{ind}  goto {lab}_next;\n{ind}  {lab}_next:\n{ind}  {b2txt}\n{ind}}}"),
              ("while-midlabel", f"{b2txt}while ({cond}) {{{b1}\n{ind}  {lab}: ;\n{ind}  {b2txt}\n{ind}}}"),
              ("for", f"for ({comma}; {cond}; {comma}) {{{b1}\n{ind}}}"),
              ("goto-for1", f"goto {lab}; for (;;) {{{b1}\n{ind}  {lab}: ; {b2txt}if (!({cond})) break;\n{ind}}}"),
              ("if-do", f"{b2txt}if ({cond}) do {{{b1}\n{ind}  {b2txt}\n{ind}}} while ({cond});"),
              ("for1", f"for (;;) {{ {b2txt}if (!({cond})) break;{b1}\n{ind}}}")]
        if b2s:
            vs.append(("while-comma", f"while (({comma}, ({cond}))) {{{b1}\n{ind}}}"))
            vs.append(("for-nostep", f"for ({comma}; {cond};) {{{b1}\n{ind}  {b2txt}\n{ind}}}"))
        else:
            vs.append(("for-empty", f"for (; {cond};) {{{b1}\n{ind}}}"))
        for vl, v in vs:
            yield f"loop_form:{vl}", head + pre_s + v + post_s + tail
    # for(;;) { B2 if (!(C)) break; B1 }
    for m in re.finditer(r"\b(?:for\s*\(\s*;\s*;\s*\)|while\s*\(\s*1\s*\))\s*\{", inner):
        ob = m.end() - 1
        cb = match_brace(inner, ob)
        if cb < 0:
            continue
        body = inner[ob + 1:cb]
        bm = re.search(r"\bif\s*\(\s*!\s*", body)
        if not bm:
            continue
        try:
            nc, ne = _paren_after(body, bm.start() + 2)
        except ValueError:
            continue
        cond = re.sub(r"^\s*!\s*", "", nc).strip()
        rest = body[ne + 1:]
        rm = re.match(r"\s*(?:\{\s*break\s*;\s*\}|break\s*;)", rest)
        pre = body[:bm.start()]
        if not rm or pre.count("{") != pre.count("}") or re.search(r"\b(if|for|while|do)\b", pre):
            continue
        b2 = pre.strip()
        b1 = rest[rm.end():].rstrip()
        yield "loop_form:for1->while", head + inner[:m.start()] + \
            f"{b2} while ({cond}) {{{b1}\n {b2}\n}}" + inner[cb + 1:] + tail


# ---- fifth round: split_local / hoist_load / sched_barrier -------------------

_SREG = re.compile(r"^(s[0-8]|fp)$")
_TREG = re.compile(r"^(v[01]|a[0-3]|t\d)$")
_LOAD_MN = re.compile(r"^(lb|lbu|lh|lhu|lw|lwu|ld|lq|lwc1|ldc1)$")


def split_local_shape(rows) -> list[dict]:
    """Aligned rows (same opcode) where the target uses a scratch v*/a*/t* reg
    and ours a callee-saved s* in the same operand: one variable assigned in
    two disjoint regions, live across calls in one of them, got an s-reg
    everywhere (_HissatuPassTime)."""
    out = []
    for a, m, b in diff_rows(rows):
        if m != "|" or not a or not b:
            continue
        ma, oa, _ = _insn_ops(a)
        mb, ob, _ = _insn_ops(b)
        if ma != mb or not oa or len(oa) != len(ob):
            continue
        for x, y in zip(oa, ob):
            x = re.sub(r"^-?\w*\((\w+)\)$", r"\1", x)
            y = re.sub(r"^-?\w*\((\w+)\)$", r"\1", y)
            if _TREG.match(x) and _SREG.match(y):
                out.append({"target": x, "cand": y, "insn": a})
                break
    return out


def hoist_load_shape(rows) -> list[dict]:
    """Target plain branch whose delay slot holds a load where ours has the
    branch-likely form: reorg never fills a non-annulled slot with a may-trap
    load from the fall-through, so the source loaded it BEFORE the branch."""
    dr = diff_rows(rows)
    out = []
    for i, (a, m, b) in enumerate(dr):
        if m == " " or not a or not b:
            continue
        ma, mb = _mn(a), _mn(b)
        slot = next((x for x, _, _ in dr[i + 1:] if x), "")
        if BRANCH_RE.match(ma) and not ma.endswith("l") and mb == ma + "l" and _LOAD_MN.match(_mn(slot)):
            out.append({"branch": a, "slot": slot})
    return out


def sched_barrier_shape(rows) -> list[str]:
    """A store moved (pure move, or a store moved next to a load that only
    differs in its register): sched1 order around a loop / helper; LOOP_BEG/END
    notes from a `do { } while (0);` are scheduling barriers."""
    dr = diff_rows(rows)
    mv = moved_insns(rows)
    if mv and any(STORE_RE.match(_mn(x)) for x in mv):
        return [x for x in mv if STORE_RE.match(_mn(x))]
    ins = [x for a, m, b in dr if m in "<>" for x in (a, b) if x and STORE_RE.match(_mn(x))]
    lds = [a for a, m, b in dr if m == "|" and a and b and _mn(a) == _mn(b) and _LOAD_MN.match(_mn(a))]
    if ins and lds:
        return ins
    return []


def _depths(lines: list[str]) -> list[int]:
    """Brace depth at the start of each line (strings/comments ignored roughly)."""
    d, out = 0, []
    for ln in lines:
        out.append(d)
        s = re.sub(r'"(\\.|[^"\\])*"|\'(\\.|[^\'\\])*\'|/\*.*?\*/|//.*$', "", ln)
        d += s.count("{") - s.count("}")
    return out


def _region(lines, dep, a, case: bool) -> int:
    """End (exclusive) of the region starting at line A: the enclosing block,
    or (CASE) the enclosing switch case."""
    d = dep[a]
    for j in range(a + 1, len(lines)):
        if dep[j] < d or (dep[j] == d and lines[j].lstrip().startswith("}")):
            return j
        if case and dep[j] == d and re.match(r"\s*(case\b.*:|default\s*:)", lines[j]):
            return j
    return len(lines)


def _rename_region(fn: str, var: str, new: str, ty: str, a: int, e: int) -> str | None:
    """Rename VAR to NEW on body lines [a, e) and declare NEW after VAR."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    pat = re.compile(r"(?<![\w.>])" + re.escape(var) + r"\b")
    hit = False
    for i in range(a, e):
        nl = pat.sub(new, lines[i])
        hit |= nl != lines[i]
        lines[i] = nl
    if not hit:
        return None
    for i, ln in enumerate(lines):
        p = parse_decl(ln)
        if p and any(decl_name(x) == var for x in p[2]):
            stars = next(re.match(r"(\**)", x.strip()).group(1) for x in p[2] if decl_name(x) == var)
            lines.insert(i + 1, f"{p[0]}{p[1]} {stars}{new};")
            return head + "\n".join(lines) + tail
    return None


def _assign_sites(lines: list[str], var: str) -> list[int]:
    r = re.compile(r"(?:^|[;({]\s*|for\s*\(\s*)" + re.escape(var) + r"\s*=(?!=)")
    return [i for i, ln in enumerate(lines) if r.search(ln.strip()) and parse_decl(ln) is None]


def _locals_block(inner: str):
    """({name: type} of the leading declarations, index of the first statement
    line); tolerates `__attribute__((...))` declarations, unlike _decl_block."""
    lines = inner.split("\n")
    locs: dict = {}
    i = 0
    while i < len(lines):
        ln = re.sub(r"\s*__attribute__\s*\(\(.*\)\)", "", lines[i])
        if not ln.strip():
            i += 1
            continue
        p = parse_decl(ln)
        if not p:
            break
        for d in p[2]:
            if "[" not in d.split("=")[0]:
                locs[decl_name(d)] = p[1]
        i += 1
    return locs, i


def t_split_local(fn, ctx=None):
    """Give one region of a local assigned in disjoint regions (switch cases,
    sibling blocks) its own fresh local, so the region where it is not live
    across a call can use a scratch register again. Diff-gated (split_local
    shape), and fold13 (o542 resolveStaticField: per-case locals for global
    class pointers stopped gcc merging the case tails, 0.87 -> match):
    ungated for a local whose every assignment copies a global (`c = classInt;`
    in one case, `c = classFloat;` in another)."""
    gated = bool((ctx or {}).get("split_local"))
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    names = set(locs) | {p for _, _, p in _params(fn)}
    n = 0
    for var, ty in locs.items():
        sites = [s for s in _assign_sites(lines, var) if s >= first]
        if len(sites) < 2:
            continue
        if not gated:
            rhs = [re.match(r"^\s*" + re.escape(var) + r"\s*=\s*(?:\([^()]*\)\s*)?&?\s*([A-Za-z_]\w*)\s*;\s*$", lines[s])
                   for s in sites]
            if not all(rhs) or any(m.group(1) in names for m in rhs):
                continue
        for case in (True, False):
            regs = [(s, _region(lines, dep, s, case)) for s in sites]
            for s, e in regs:
                if not any(e2 <= s or e <= s2 for s2, e2 in regs if s2 != s):
                    continue           # not disjoint from every other site
                if any(s < s2 < e for s2, _ in regs):
                    continue           # region holds another assignment: rename from the first one
                nf = _rename_region(fn, var, f"{var}_{s}", ty, s, e)
                if nf:
                    n += 1
                    yield f"split_local:{var}@{s}{':case' if case else ''}", nf
                if n >= 40:
                    return
_LVAL = re.compile(r"(?:\*\s*)?(?:\(\s*[\w\s*]+\)\s*)?\w+(?:\s*(?:->|\.)\s*\w+|\s*\[[^\[\]]+\])+")


def t_hoist_load(fn, ctx=None):
    """`if (a && p->f)` -> `t = p->f; if (a && t)` (the later && operand's or a
    nested if's load hoisted above the condition), so reorg can put it in the
    delay slot of a plain branch instead of annulling a branch-likely."""
    if not (ctx or {}).get("hoist_load"):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    _, first = _locals_block(inner)
    ind0 = re.match(r"\s*", lines[first - 1] if first else "    ").group(0) or "    "
    n = 0
    for i, ln in enumerate(lines):
        m = re.match(r"^(\s*)if\s*\(", ln)
        if not m or i < first:
            continue
        try:
            cond, ce = _paren_after(ln, m.end() - 1)
        except ValueError:
            continue
        ind = m.group(1)
        exprs = []
        ops = [x.strip() for x in re.split(r"&&|\|\|", cond)]
        exprs += [(k, x) for k, x in enumerate(ops) if k > 0]
        # nested if: first statement of the then-block
        if ln.rstrip().endswith("{") and i + 1 < len(lines):
            mm = re.match(r"^\s*if\s*\(", lines[i + 1])
            if mm:
                try:
                    c2, _ = _paren_after(lines[i + 1], mm.end() - 1)
                    exprs += [(-1, x.strip()) for x in re.split(r"&&|\|\|", c2)]
                except ValueError:
                    pass
        end = _region(lines, dep, i + 1, False) if ln.rstrip().endswith("{") else i + 1
        for k, op in exprs:
            for lm in _LVAL.finditer(op):
                lv = lm.group(0).strip()
                if "(" in lv and not lv.startswith(("*", "(")):
                    continue
                if re.search(r"\w\s*\(", lv):
                    continue
                tmp = f"hl{n}"
                for scope in ("cond", "body"):
                    nl = list(lines)
                    stop = i + 1 if scope == "cond" else min(end + 1, len(nl))
                    hit = False
                    for j in range(i, stop):
                        r = nl[j].replace(lv, tmp)
                        hit |= r != nl[j]
                        nl[j] = r
                    if not hit:
                        continue
                    if scope == "body" and stop == i + 1:
                        continue
                    nl.insert(i, f"{ind}{tmp} = {lv};")
                    nl.insert(first, f"{ind0}__typeof__({lv}) {tmp};")
                    yield f"hoist_load:{k}:{scope}:{lv[:24]}", head + "\n".join(nl) + tail
                n += 1
                if n >= 30:
                    return


def t_sched_barrier(fn, ctx=None):
    """sched1 barrier tricks around a moved store in a loop: wrap a store in
    its own `do { } while (0);` (NOTE_INSN_LOOP_BEG/END), move a load to be
    the first statement after the preceding call, and give a loaded local a
    fresh block-local temp (global pseudo vs local-alloc)."""
    if not (ctx or {}).get("sched_barrier"):
        return
    if not re.search(r"\b(for|while|do)\b", fn):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    stores = [i for i in range(first, len(lines)) if _simple_stmt(lines[i]) and _is_store(lines[i])]
    n = 0
    # (1) wrap one store
    for i in stores:
        ind = re.match(r"\s*", lines[i]).group(0)
        nl = list(lines)
        nl[i] = f"{ind}do {{\n{ind}    {lines[i].strip()}\n{ind}}} while (0);"
        yield f"sched_barrier:wrap:{i}", head + "\n".join(nl) + tail
        n += 1
    # (2) local load right after the preceding call statement
    loads = []
    for i in range(first, len(lines)):
        p = _stmt_lhs(lines[i])
        if p and p[1] in locs and re.search(r"->|\[|\*", p[2]) and not re.search(r"\w\s*\(", p[2].replace("(float *)", "")):
            loads.append(i)
    for i in loads:
        j = i - 1
        while j >= first and _simple_stmt(lines[j]) and not re.search(r"\w\s*\([^)]*\)\s*;", lines[j]) \
                and dep[j] == dep[i]:
            j -= 1
        if j < i - 1 and j >= first and dep[j] == dep[i] and re.search(r"\w\s*\(.*\)\s*;", lines[j]):
            nl = list(lines)
            ln = nl.pop(i)
            nl.insert(j + 1, ln)
            yield f"sched_barrier:loadfirst:{i}", head + "\n".join(nl) + tail
    # (3) fresh block-local temp for a loaded local, alone and + each wrap
    for i in loads:
        var = _stmt_lhs(lines[i])[1]
        e = _region(lines, dep, i, False)
        others = [k for k in range(first, len(lines)) if (k < i or k >= e)
                  and re.search(r"(?<![\w.>])" + re.escape(var) + r"\b", lines[k])]
        if not others:
            continue
        nf = _rename_region(fn, var, f"{var}_t", locs[var], i, e)
        if not nf:
            continue
        yield f"sched_barrier:fresh:{var}@{i}", nf
        h2, in2, t2 = _split_fn(nf)
        l2 = in2.split("\n")
        for k in range(len(l2)):
            if _simple_stmt(l2[k]) and _is_store(l2[k]) and re.search(r"\b" + re.escape(var) + r"_t\b", l2[k]):
                ind = re.match(r"\s*", l2[k]).group(0)
                m2 = list(l2)
                m2[k] = f"{ind}do {{\n{ind}    {l2[k].strip()}\n{ind}}} while (0);"
                yield f"sched_barrier:fresh+wrap:{var}@{i}:{k}", h2 + "\n".join(m2) + t2


# ---- sixth round (nmlModelDirectSend, JNT_getStaticVal, svAddImage) ---------
# param_widen  candidate sign/zero-extends an a-register param the target uses raw
#              (s16 param -> sll/sra 16): widen that param to int (reverse too)
# copy_inline  repeated d[i] = s[i] groups outlined into a static inline helper
# const_rev    a run of stores of one constant reordered (s[2], s[1], s[0])
# addu_swap    addu rs/rt swapped: commute one `+` (any operands, (int) cast form)
# sign_flip    slti<->sltiu / slt<->sltu: flip the compared variable's signedness
# ret_local    `ret` local only assigned and returned -> `return expr;` per site

_NARROW = {"short": 16, "s16": 16, "signed short": 16, "unsigned short": 16, "u16": 16,
           "char": 24, "s8": 24, "signed char": 24, "unsigned char": 24, "u8": 24}


def _areg_ext(insn: str):
    """(aN index, bits, kind) when INSN sign/zero-extends an a-register:
    `sll x,aN,16|24` / `sra x,aN,16|24` / `andi x,aN,0xffff|0xff`."""
    mn, ops, _ = _insn_ops(insn)
    if len(ops) != 3:
        return None
    m = re.fullmatch(r"a([0-3])", ops[1])
    if not m:
        return None
    v = _ival(ops[2])
    if mn in ("sll", "sra", "dsll32", "dsra32") and v in (16, 24):
        return int(m.group(1)), v, "s"
    if mn == "andi" and v in (0xFFFF, 0xFF):
        return int(m.group(1)), 16 if v == 0xFFFF else 24, "u"
    return None


def sext_shape(rows) -> list[dict]:
    """Extends of an a-register param present on one side only."""
    dr = diff_rows(rows)
    t = {}
    c = {}
    for a, m, b in dr:
        for x, bag in ((a, t), (b, c)):
            e = _areg_ext(x) if x else None
            if e:
                bag[e] = bag.get(e, 0) + 1
    out = []
    for e in set(t) | set(c):
        if c.get(e, 0) > t.get(e, 0):
            out.append({"a": e[0], "bits": e[1], "kind": e[2], "dir": "widen"})
        elif t.get(e, 0) > c.get(e, 0):
            out.append({"a": e[0], "bits": e[1], "kind": e[2], "dir": "narrow"})
    return out


def _params(fn: str):
    """[(start, end, text)] of the parameter declarations (offsets into fn)."""
    b = _split_fn(fn)[0]
    o = b.index("(")
    c = match_brace(b, o)
    if c < 0:
        return []
    out = []
    i = o + 1
    for p in split_top(b[o + 1:c]):
        j = b.index(p, i) if p else i
        out.append((j, j + len(p), p))
        i = j + len(p) + 1
    return out


def _param_type(p: str):
    m = re.match(r"^\s*((?:const\s+|volatile\s+|register\s+)*)([\w\s]+?)\s+(\w+)\s*$", p)
    return (m.group(1), m.group(2).strip(), m.group(3)) if m else None


def _proto_patches(unit: str, tu: str, name: str, k: int, old_t: str, new_t: str) -> list | None:
    """Forward declarations of NAME whose K-th parameter type is OLD_T in the
    TU / src/<unit> headers: [(relpath, old, new)]; None when one lives in include/."""
    src = ROOT / "src" / unit / f"{tu}.c"
    try:
        tu_text = src.read_text()
    except OSError:
        return []
    files = [src]
    for inc in re.findall(r'^\s*#\s*include\s+"([^"]+)"', tu_text, re.M):
        p = (src.parent / inc).resolve()
        if p.is_file():
            files.append(p)
        elif (ROOT / "include" / inc).is_file() and re.search(r"\b" + re.escape(name) + r"\s*\(",
                                                              (ROOT / "include" / inc).read_text(errors="replace")):
            return None
    out = []
    dre = re.compile(r"^[^\n;{}()#=]*?\b" + re.escape(name) + r"\s*\(([^;{}]*)\)\s*;", re.M)
    for f in files:
        for m in dre.finditer(f.read_text(errors="replace")):
            ps = split_top(m.group(1))
            if k >= len(ps):
                continue
            q = ps[k].strip()
            core = re.sub(r"\s+\w+$", "", q) if _param_type(q) else q
            if core.replace(" ", "") != old_t.replace(" ", ""):
                continue
            ps2 = list(ps)
            ps2[k] = " " + q.replace(core, new_t, 1) if k else q.replace(core, new_t, 1)
            old = m.group(0)
            new = old[:m.start(1) - m.start()] + ",".join(ps2) + old[m.end(1) - m.start():]
            out.append((str(f.relative_to(ROOT)), old, new))
    return out


def t_param_widen(text, name, ctx=None):
    """A param declared s16/char gets `sll 16; sra 16` (or andi) the target
    lacks: widen it to int. Reverse: the target extends an a-reg we use raw ->
    the matching narrow type. Conflicting prototypes are patched like void_ret
    (nmlModelDirectSend: `s16 attr` -> `int attr`)."""
    ctx = ctx or {}
    sh = ctx.get("param_widen") or []
    loc = find_function(text, name)
    if not loc:
        return
    if not sh:
        # fold15: try_fn's narrow_arg_calls alone still gates the call casts
        yield from _call_widen(text, name, ctx)
        return
    s, b, e = loc
    fn = text[s:e + 1]
    ps = _params(fn)
    pat = ctx.setdefault("patches", {})
    done = set()
    for x in sh:
        ks = [x["a"]] if x["a"] < len(ps) else []
        ks += [k for k in range(len(ps)) if k not in ks]     # float params shift a-regs: try all
        for k in ks:
            st, en, p = ps[k]
            pt = _param_type(p)
            if not pt or "*" in p:
                continue
            q, ty, nm = pt
            if x["dir"] == "widen":
                if ty not in _NARROW:
                    continue
                news = ["int", "unsigned int"] if x["kind"] == "u" else ["int"]
            else:
                if ty not in ("int", "s32", "u32", "unsigned int", "unsigned", "long"):
                    continue
                news = ({16: ["short", "s16"], 24: ["signed char", "char"]} if x["kind"] == "s"
                        else {16: ["unsigned short"], 24: ["unsigned char"]})[x["bits"]]
            for nt in news:
                if (k, nt) in done:
                    continue
                done.add((k, nt))
                nf = fn[:st] + p.replace(ty, nt, 1) + fn[en:]
                new = text[:s] + nf + text[e + 1:]
                pp = _proto_patches(ctx.get("unit", ""), ctx.get("tu", ""), name, k, ty, nt) if ctx.get("unit") else []
                if pp is None:
                    continue
                if pp:
                    pat[body_sha(new)] = pp
                yield f"param_widen:{nm}:{ty}->{nt}", new
    # fold10 (o497 cmdAtkset): the extend is at a call argument, not a param
    yield from _call_widen(text, name, ctx)


_COPY = re.compile(r"^(\s*)(.+?)\[(\d+)\]\s*=\s*(.+?)\[(\d+)\];\s*$")


def _copy_groups(lines):
    """Runs of 2-4 `D[i] = S[i];` with i = 0..n-1 (same D, S): [(start, n, D, S)]."""
    out = []
    i = 0
    while i < len(lines):
        m = _COPY.match(lines[i])
        if not m or m.group(3) != "0" or m.group(5) != "0":
            i += 1
            continue
        d, sx = m.group(2).strip(), m.group(4).strip()
        n = 1
        while i + n < len(lines) and n < 4:
            mm = _COPY.match(lines[i + n])
            if not (mm and mm.group(2).strip() == d and mm.group(4).strip() == sx
                    and mm.group(3) == str(n) and mm.group(5) == str(n)):
                break
            n += 1
        if n >= 2:
            out.append((i, n, d, sx))
        i += max(n, 1)
    return out


def _elem_type(text: str, fn: str, expr: str) -> str:
    """Element type for a copy helper: float when the expression mentions a
    float array/pointer cast, else int."""
    if "float" in expr:
        return "float"
    base = re.match(r"[\w]+", expr.lstrip("(*&"))
    if base:
        m = re.search(r"\b(float|int|short|char|unsigned\s+\w+|s32|u32|s16|u16)\s*\*?\s*" + re.escape(base.group(0))
                      + r"\b", fn)
        if m:
            return m.group(1)
    if "->" in expr or "." in expr:
        return "float"
    return "int"


def t_copy_inline(text, name, ctx=None):
    """Two or more identical-shape copy groups (`d[0]=s[0]; d[1]=s[1]; ...`)
    outlined into `static inline void CopyN(T *d, T *s)` (JNT_getStaticVal:
    inline float[3] copies gave an off-by-one register allocation)."""
    loc = find_function(text, name)
    if not loc:
        return
    s, b, e = loc
    fn = text[s:e + 1]
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    gs = _copy_groups(lines)
    by_n: dict = {}
    for g in gs:
        by_n.setdefault(g[1], []).append(g)
    for n, grp in by_n.items():
        if len(grp) < 2:
            continue
        ty = _elem_type(text, fn, grp[0][2] + " " + grp[0][3])
        hn = f"SwCopy{n}_{name}"
        helper = (f"static inline void {hn}({ty} *d, {ty} *s) {{\n"
                  + "".join(f"    d[{i}] = s[{i}];\n" for i in range(n)) + "}\n\n")
        for mode in ("all", "noaddr"):
            nl = list(lines)
            for i0, nn, d, sx in sorted(grp, reverse=True):
                ind = _COPY.match(lines[i0]).group(1)
                da = d if d.startswith("(") or re.fullmatch(r"\w+", d) and mode == "noaddr" else f"&({d})[0]" \
                    if mode == "all" else d
                sa = sx if sx.startswith("(") or re.fullmatch(r"\w+", sx) and mode == "noaddr" else f"&({sx})[0]" \
                    if mode == "all" else sx
                nl[i0:i0 + nn] = [f"{ind}{hn}({da}, {sa});"]
            yield f"copy_inline:{n}x{len(grp)}:{mode}", text[:s] + helper + head + "\n".join(nl) + tail + text[e + 1:]


_CSTORE = re.compile(r"^(\s*)([^=;]+?)\s*=\s*(-?[\d.]+[fF]?|0x[0-9a-fA-F]+)\s*;\s*$")


def t_const_rev(fn, ctx=None):
    """Runs of >= 2 adjacent stores of the same constant: reversed and every
    permutation (<= 4 stores). gcc's CSE of the shared constant reorders them
    (JNT_getStaticVal: s[0],s[1],s[2] = 1.0f came out last/first/second;
    writing s[2],s[1],s[0] gave the target order)."""
    if not (ctx or {}).get("const_rev"):
        return
    import itertools
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    i = 0
    n = 0
    while i < len(lines):
        m = _CSTORE.match(lines[i])
        if not m:
            i += 1
            continue
        j = i + 1
        while j < len(lines) and (mm := _CSTORE.match(lines[j])) and mm.group(3) == m.group(3) \
                and mm.group(1) == m.group(1) and j - i < 6:
            j += 1
        if j - i >= 2:
            run = lines[i:j]
            perms = [run[::-1]] + ([list(p) for p in itertools.permutations(run)] if len(run) <= 4 else [])
            seen = {tuple(run)}
            for p in perms:
                if tuple(p) in seen:
                    continue
                seen.add(tuple(p))
                nl = lines[:i] + list(p) + lines[j:]
                yield f"const_rev:L{i}x{j - i}:{n}", head + "\n".join(nl) + tail
                n += 1
                if n > 60:
                    return
        i = j


def const_store_shape(rows) -> list[str]:
    """A differing / moved store whose source register is stored >= 2 times
    in the diff window: possibly a run of stores of one constant in the
    wrong order (JNT_getStaticVal: one moved `swc1 $f2,8(t0)`)."""
    out = []
    win = diff_window(rows, pad=4)
    for a, m, b in diff_rows(rows):
        if m == " ":
            continue
        for x in (a, b):
            if not x:
                continue
            mn, ops, _ = _insn_ops(x)
            if STORE_RE.match(mn) and ops and ops[0] != "zero" and \
                    sum(1 for y in win if _insn_ops(y)[0] == mn and _insn_ops(y)[1][:1] == [ops[0]]) >= 2:
                out.append(x)
                break
    return out


_STORE_BASE = re.compile(r"(-?\w+)\((\w+)\)\s*$")


def store_order_shape(rows) -> list[str]:
    """>= 2 differing stores through one non-sp base register at different
    offsets (SEQ_moveUnitSPL: target 16,24,20 where ours is 16,20,24): adjacent
    field stores written in the wrong order."""
    by: dict = {}
    for a, m, b in diff_rows(rows):
        if m == " ":
            continue
        for x in (a, b):
            if not x:
                continue
            mn, ops, _ = _insn_ops(x)
            if not STORE_RE.match(mn) or len(ops) < 2:
                continue
            rs = re.search(r"\[(?:LO16|GPREL16) ([\w.$]+)\]", x)
            if rs:                          # distinct globals (thinkInit): one group
                by.setdefault("<glob>", {}).setdefault(rs.group(1), x)
                continue
            mm = _STORE_BASE.match(ops[-1])
            if not mm or mm.group(2) == "sp":
                continue
            by.setdefault(mm.group(2), {}).setdefault(mm.group(1), x)
    out = []
    for base, offs in by.items():
        if len(offs) >= 2:
            out.extend(offs.values())
    return out


_SYM_ADDRS: dict = {}


def _sym_addrs(unit: str) -> dict:
    """{symbol: address} from config/symbols/<unit>.txt (cached)."""
    if unit not in _SYM_ADDRS:
        d: dict = {}
        try:
            for ln in (ROOT / "config/symbols" / f"{unit}.txt").read_text().splitlines():
                m = re.match(r"\s*([A-Za-z_.$][\w.$]*)\s*=\s*(0[xX][0-9a-fA-F]+)\s*;", ln)
                if m:
                    d[m.group(1)] = int(m.group(2), 16)
        except OSError:
            pass
        _SYM_ADDRS[unit] = d
    return _SYM_ADDRS[unit]


_FSTORE = re.compile(r"^(\s*)(\(?\s*\*?\s*\(?([A-Za-z_]\w*)[^=;]*?)\s*=(?!=)\s*([^;]*);\s*$")


def _store_slot_order(run) -> list[int] | None:
    """Indexes of a store run sorted by their literal slot (`a[3]`, `->f[2]`),
    None when the stores do not differ by one literal subscript."""
    ks = []
    for r in run:
        m = re.search(r"\[\s*(0[xX][0-9a-fA-F]+|\d+)\s*\]\s*$", re.sub(r"\s+", " ", r[2]).strip())
        if not m:
            return None
        ks.append(int(m.group(1), 0))
    if len(set(ks)) != len(ks):
        return None
    return sorted(range(len(run)), key=lambda x: ks[x])


def t_store_sort(fn, ctx=None):
    """Runs of 2..6 adjacent stores to fields / elements of one base
    (`u->pos.x = v[0]; u->pos.z = v[2]; u->pos.y = v[1];`), reordered: every
    permutation of runs <= 4 (all orders of a 4-run: 23), the reversal and
    adjacent swaps of longer ones. stmtswap never swaps them (both statements
    "write" the base). SEQ_moveUnitSPL / SEQ_rotateUnitSPL matched with the
    stores in ascending field order although the target emits 0x20,0x28,0x24.
    Runs of plain global stores (`camMode = 0; persMode = 3;`, thinkInit
    o452) group too: first their address order from config/symbols, and runs
    of 5-8 also get rotations and every single-store move (thinkInit: address
    order 0.80 -> 0.94, one move -> match).
    Diff-gated: >= 2 differing stores through one base register, or to
    distinct %lo / %gp_rel symbols. Fold14: runs of 2..4 stores through
    different bases too (_store_sort_mixed, target-first-store-last prior;
    also gated on a single moved store: sched_barrier / sched_weight)."""
    c_ = ctx or {}
    if not (c_.get("store_sort") or c_.get("sched_barrier") or c_.get("sched_weight")):
        return
    import itertools
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    nm_ = 0
    for i0, k0, nl0 in _store_sort_mixed(lines):     # fold14: mixed-base runs (o560)
        yield f"store_sort:mix:L{i0}x{k0}:{nm_}", head + "\n".join(nl0) + tail
        nm_ += 1
        if nm_ >= 30:
            break
    if not c_.get("store_sort"):
        return
    locals_ = {nm for _, nm in _scalar_locals(fn)} | set(_locals_block(inner)[0])
    addr = _sym_addrs((ctx or {}).get("unit") or "")

    def store(ln):
        m = _FSTORE.match(ln)
        if not m or not _simple_stmt(ln):
            return None
        lhs, rhs = m.group(2).strip(), m.group(4)
        glob = bool(re.fullmatch(r"[A-Za-z_]\w*", lhs)) and lhs not in locals_
        if not (glob or re.search(r"->|\.|\[", lhs)) or re.search(r"\+\+|--|(?<![=!<>])=(?!=)", rhs):
            return None
        if re.search(r"\b(?!sizeof\b)[A-Za-z_]\w*\s*\(", rhs.replace("(float)", "").replace("(int)", "")):
            return None                       # a call: order is observable
        return m.group(1), ("<glob>" if glob else m.group(3)), lhs, rhs

    n = 0
    i = 0
    while i < len(lines):
        a = store(lines[i])
        if not a:
            i += 1
            continue
        run = [a]
        j = i + 1
        while j < len(lines) and j - i < 8:
            b = store(lines[j])
            if not b or b[0] != a[0] or b[1] != a[1]:
                break
            run.append(b)
            j += 1
        lhss = [re.sub(r"\s+", "", r[2]) for r in run]
        ok = len(run) >= 2 and len(set(lhss)) == len(lhss)
        if ok:
            rhss = [re.sub(r"\s+", "", r[3]) for r in run]
            ok = not any(x in y for x in lhss for y in rhss) and \
                not any(x != y and y.startswith(x) for x in lhss for y in lhss)
        if not ok:
            i = max(i + 1, j - 1) if len(run) >= 2 else i + 1
            continue
        blk = lines[i:j]
        perms = []
        if run[0][1] == "<glob>" and addr:
            # declaration / address order (thinkInit: target emitted the m2c
            # order, the source wrote the stores in the globals' address order)
            ks = [addr.get(r[2], 1 << 40) for r in run]
            perms.append([blk[x] for x in sorted(range(len(blk)), key=lambda x: (ks[x], x))])
        if len(blk) == 4:
            # fold11 prior: last store first (3,0,1,2) matched two 4-slot clearing
            # blocks by hand (o435 ACT_updateSequence, o514 MAP_updateUnitPartsSequence)
            perms.append([blk[3]] + blk[:3])
            sk = _store_slot_order(run)
            if sk:
                srt = [blk[x] for x in sk]
                perms += [[srt[3]] + srt[:3], srt]
        if len(blk) <= 4:
            perms += [list(p) for p in itertools.permutations(blk)]
        else:
            perms += [blk[::-1]] + [blk[:k] + [blk[k + 1], blk[k]] + blk[k + 2:] for k in range(len(blk) - 1)]
            perms += [blk[k:] + blk[:k] for k in range(1, len(blk))]          # rotations
            for a in range(len(blk)):                                          # one store moved
                for b in range(len(blk)):
                    if a != b:
                        q = blk[:a] + blk[a + 1:]
                        perms.append(q[:b] + [blk[a]] + q[b:])
        seen = {tuple(blk)}
        for p in perms:
            if tuple(p) in seen:
                continue
            seen.add(tuple(p))
            yield f"store_sort:L{i}x{len(blk)}:{n}", head + "\n".join(lines[:i] + p + lines[j:]) + tail
            n += 1
            if n > 90:
                return
        i = j


def addu_swap_shape(rows) -> list[str]:
    out = []
    for a, m, b in diff_rows(rows):
        if m != "|" or not a or not b:
            continue
        ma, oa, _ = _insn_ops(a)
        mb, ob, _ = _insn_ops(b)
        if ma == mb and ma in ("addu", "daddu") and len(oa) == 3 == len(ob) and oa[0] == ob[0] \
                and oa[1] == ob[2] and oa[2] == ob[1] and oa[1] != oa[2]:
            out.append(a)
    return out


_FADD = {"add.s", "add.d", "mul.s", "mul.d"}


def fadd_order_shape(rows) -> list[str]:
    """Fold13 (o547): a float add/mul in another place or with other operands
    (`x += K` vs `x = K + x` decides the add.s operand order and schedule)."""
    out = []
    for a, m, b in diff_rows(rows):
        if m == " " or (not a and not b):
            continue
        ma, oa, _ = _insn_ops(a) if a else ("", [], None)
        mb, ob, _ = _insn_ops(b) if b else ("", [], None)
        if (ma in _FADD or mb in _FADD) and (ma != mb or oa != ob):
            out.append(a or b)
    return out


def _plus_sites(inner: str):
    """(a_start, a_end, b_start, b_end) of top-level binary `+` with balanced operands."""
    for m in re.finditer(r"(?<![+\s(,=])\s*\+\s*(?![+=])|(?<=[)\]\w])\s*\+\s*(?![+=])", inner):
        i = m.start()
        if inner[i - 1:i] == "+" or inner[m.end():m.end() + 1] == "+":
            continue
        # left operand: walk back over a balanced primary
        j = i
        while j > 0 and inner[j - 1] == " ":
            j -= 1
        k = j
        if k > 0 and inner[k - 1] in ")]":
            depth = 0
            while k > 0:
                c = inner[k - 1]
                if c in ")]":
                    depth += 1
                elif c in "([":
                    depth -= 1
                k -= 1
                if depth == 0:
                    break
            while k > 0 and re.match(r"[\w.>\-]", inner[k - 1]) and not (inner[k - 1] == "-" and inner[k:k + 1] != ">"):
                k -= 1
        else:
            while k > 0 and re.match(r"[\w.\->]", inner[k - 1]):
                if inner[k - 1] == "-" and inner[k:k + 1] != ">":
                    break
                if inner[k - 1] == ">" and inner[k - 2:k - 1] != "-":
                    break
                k -= 1
        # a leading cast `(int)x`
        mc = re.search(r"\(\s*(?:unsigned\s+)?\w+\s*\**\s*\)\s*$", inner[:k])
        if mc:
            k = mc.start()
        if k == j or not _leftok(inner, k):
            continue
        b0 = m.end()
        b1 = _operand_end(inner, b0)
        rhs = inner[b0:b1]
        if not rhs.strip() or re.search(r"(?<![-+])[+\-*/%<>&|^](?![>])", re.sub(r"\([^()]*\)|\[[^\[\]]*\]|->", "", rhs)):
            continue          # only `A + B` with B a single (balanced) operand
        yield k, j, b0, b0 + len(rhs.rstrip())


_COMPOUND_ADD = re.compile(r"^(?P<ind>\s*)(?P<x>[\w.>\-\[\]()&*]+?)\s*(?P<op>[+*])=\s*(?P<e>[^;]+);\s*$")


def t_addu_swap(fn, ctx=None):
    """addu rs/rt swapped (JNT_getStaticVal: `(idx << 5) + (int)tbl` vs
    `tbl + (idx << 5)`): commute one `+` at a time, plus the (int)-cast forms
    for pointer + int. Fold13 (o547 MAP_updateUnitSpecialSymbol 0.9375 ->
    0.9563, add.s operands / schedule; also gated on `fadd_order`, a float
    add/mul differing in place or operands): compound `x += E;` / `x *= E;`
    -> `x = E + x;` / `x = x + E;` (one site at a time, then all), first."""
    ctx = ctx or {}
    if not ctx.get("addu_swap") and not ctx.get("fadd_order"):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    sites = [(i, m) for i, ln in enumerate(lines) for m in [_COMPOUND_ADD.match(ln)] if m]
    for i, m in sites[:12]:
        x, e, op = m.group("x"), m.group("e").strip(), m.group("op")
        ee = e if re.fullmatch(r"[\w.>\-\[\]]+", e) else f"({e})"
        for form, lab in ((f"{x} = {ee} {op} {x};", "first"), (f"{x} = {x} {op} {ee};", "last")):
            nl = list(lines)
            nl[i] = m.group("ind") + form
            yield f"addu_swap:compound:{lab}:{i}", head + "\n".join(nl) + tail
    if len(sites) > 1:
        nl = list(lines)
        for i, m in sites:
            e = m.group("e").strip()
            ee = e if re.fullmatch(r"[\w.>\-\[\]]+", e) else f"({e})"
            nl[i] = m.group("ind") + f"{m.group('x')} = {ee} {m.group('op')} {m.group('x')};"
        yield "addu_swap:compound:first:all", head + "\n".join(nl) + tail
    if not ctx.get("addu_swap"):
        return
    yield from addu_wrap_forms(fn)
    n = 0
    for a0, a1, b0, b1 in _plus_sites(inner):
        A, B = inner[a0:a1].strip(), inner[b0:b1].strip()
        if A in KEYWORDS or B in KEYWORDS or not A or not B:
            continue
        forms = [(f"{B} + {A}", "swap")]
        for X, Y, lab in ((A, B, "a"), (B, A, "b")):
            if not X.startswith("(") and not re.fullmatch(r"\d+|0x[0-9a-fA-F]+", X):
                forms.append((f"{Y} + (int){X}", f"swapcast{lab}"))
                forms.append((f"(int){X} + {Y}", f"cast{lab}"))
        mc = re.fullmatch(r"\(\s*int\s*\)\s*(.+)", A) or re.fullmatch(r"\(\s*int\s*\)\s*(.+)", B)
        if mc:                         # drop an existing (int) cast
            other = B if mc.string == A else A
            forms.append((f"{mc.group(1)} + {other}", "uncast"))
        for f, lab in forms:
            yield f"addu_swap:{lab}:{A[:12]}+{B[:12]}", head + inner[:a0] + f + inner[b1:] + tail
        n += 1
        if n > 30:
            return


def sign_shape(rows) -> list[dict]:
    out = []
    pairs = {("slti", "sltiu"), ("slt", "sltu")}
    for a, m, b in diff_rows(rows):
        if m != "|" or not a or not b:
            continue
        ma, mb = _mn(a), _mn(b)
        if (ma, mb) in pairs or (mb, ma) in pairs:
            out.append({"target": ma, "cand": mb, "want": "unsigned" if ma.endswith("u") else "signed"})
    return out


_SIGNED = {"int": "unsigned int", "s32": "u32", "short": "unsigned short", "s16": "u16", "char": "unsigned char",
           "signed char": "unsigned char", "s8": "u8", "long": "unsigned long", "long long": "unsigned long long"}
_UNSIGNED = {"unsigned int": "int", "unsigned": "int", "u32": "s32", "unsigned short": "short", "u16": "s16",
             "unsigned char": "signed char", "u8": "s8", "unsigned long": "long", "u64": "s64"}


def t_sign_flip(fn, ctx=None):
    """slti<->sltiu / slt<->sltu: flip the signedness of a variable used in a
    comparison (declaration: local or param), or cast the comparison operand."""
    sg = (ctx or {}).get("sign_flip")
    if not sg:
        return
    want = sg[0]["want"]
    tbl = _SIGNED if want == "unsigned" else _UNSIGNED
    head, inner, tail = _split_fn(fn)
    vars_ = set()
    for s0, e0 in _cond_spans(inner):
        for m in _REL.finditer(inner[s0:e0]):
            if m.group(1) in ("==", "!="):
                continue
            vars_ |= set(re.findall(r"\b([A-Za-z_]\w*)\b", inner[s0:e0]))
    for m in re.finditer(r"=\s*([^;]*?)\s*(<|>|<=|>=)\s*([^;]*);", inner):   # x = a < b;
        vars_ |= set(re.findall(r"\b([A-Za-z_]\w*)\b", m.group(0)))
    vars_ -= KEYWORDS
    n = 0
    # locals
    items, first, lines = _decl_block(inner)
    for k, (ty, d) in enumerate(items):
        nm = decl_name(d)
        base = re.sub(r"\b(register|const|volatile)\s+", "", ty).strip()
        if nm in vars_ and base in tbl and not d.lstrip().startswith("*") and "[" not in d:
            it = items[:]
            it[k] = (ty.replace(base, tbl[base]), d)
            yield f"sign_flip:local:{nm}->{tbl[base]}", head + _render(it, first, lines) + tail
            n += 1
    # params
    for st, en, p in _params(fn):
        pt = _param_type(p)
        if pt and pt[2] in vars_ and pt[1] in tbl and "*" not in p:
            yield f"sign_flip:param:{pt[2]}->{tbl[pt[1]]}", fn[:st] + p.replace(pt[1], tbl[pt[1]], 1) + fn[en:]
    # cast one operand of each relational compare
    cast = "(unsigned int)" if want == "unsigned" else "(int)"
    for s0, e0 in sorted(set(_cond_spans(inner))):
        cond = inner[s0:e0]
        for m in _REL.finditer(cond):
            if m.group(1) in ("==", "!="):
                continue
            b0 = m.end()
            while b0 < len(cond) and cond[b0] == " ":
                b0 += 1
            b1 = _operand_end(cond, b0)
            nc = cond[:b0] + cast + "(" + cond[b0:b1].rstrip() + ")" + cond[b0 + len(cond[b0:b1].rstrip()):]
            yield f"sign_flip:castr:{cond[:12]}", head + inner[:s0] + nc + inner[e0:] + tail
            a0 = _operand_start(cond, m.start())
            lhs = cond[a0:m.start()].rstrip()
            if lhs.strip():
                nc = cond[:a0] + cast + "(" + lhs.strip() + ")" + cond[a0 + len(lhs):]
                yield f"sign_flip:castl:{cond[:12]}", head + inner[:s0] + nc + inner[e0:] + tail
            n += 1
            if n > 30:
                return


def _skip_else(lines, j, dep):
    """Index after an `else` (block or single statement) starting at line j."""
    ln = lines[j].strip()
    if ln.endswith("{") or (j + 1 < len(lines) and lines[j + 1].strip() == "{"):
        k = j if ln.endswith("{") else j + 1
        d0 = dep[k]
        k += 1
        while k < len(lines) and dep[k] > d0:
            k += 1
        return k + 1
    if ln.startswith("else") and ln.rstrip().endswith(";"):
        return j + 1
    if ln == "else" or re.match(r"else\s+if\b", ln):
        return None                       # else-if chain: not handled
    return None


def _flows_to_return(lines, dep, i, end):
    """Does control after line i reach line END (the final return) with
    nothing executed in between (only closing braces / skipped else arms)?"""
    j = i + 1
    while j < end:
        s = lines[j].strip()
        if not s or s == "}" or s == "};" or s.startswith("//") or s.startswith("/*"):
            j += 1
            continue
        if s.startswith("} else") or s.startswith("else"):
            if s.startswith("}"):
                s2 = s[1:].strip()
                if s2.endswith("{"):
                    d0 = dep[j]
                    k = j + 1
                    while k < len(lines) and dep[k] > d0 - 1 and not lines[k].strip().startswith("}"):
                        k += 1
                    # k: closing brace of the else block at the same depth
                    j = k + 1
                    continue
                return False
            k = _skip_else(lines, j, dep)
            if k is None:
                return False
            j = k
            continue
        return False
    return True


def t_ret_local(fn, ctx=None):
    """A `ret` local only assigned (`ret = E;`) and read by a final `return
    ret;` -> `return E;` at each tail site (svAddImage 0.51 -> 0.93: the local
    landed in t1 instead of v0). A non-tail constant assignment at the top
    level (`ret = -1;` before an if) becomes the final `return -1;`."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    ri = max((i for i, ln in enumerate(lines) if ln.strip()), default=-1)
    if ri < 0:
        return
    m = re.match(r"^(\s*)return\s+([A-Za-z_]\w*)\s*;\s*$", lines[ri])
    if not m or m.group(2) not in locs:
        return
    v = m.group(2)
    vre = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
    sites, other = [], []
    for i in range(first, ri):
        if not vre.search(lines[i]):
            continue
        mm = re.match(r"^(\s*)" + re.escape(v) + r"\s*=\s*([^=].*);\s*$", lines[i])
        if mm and not vre.search(mm.group(2)):
            sites.append((i, mm.group(1), mm.group(2).strip()))
        else:
            other.append(i)
    if other or not sites:
        return
    tails = [x for x in sites if _flows_to_return(lines, dep, x[0], ri)]
    nont = [x for x in sites if x not in tails]
    def drop_decl(nl):
        out = []
        for k, ln in enumerate(nl):
            p = parse_decl(ln) if k < first else None
            if p and any(decl_name(d) == v for d in p[2]):
                rest = [d for d in p[2] if decl_name(d) != v]
                if rest:
                    out.append(f"{p[0]}{p[1]} {', '.join(rest)};")
                continue
            out.append(ln)
        return out
    if not nont:
        nl = list(lines)
        for i, ind, ex in tails:
            nl[i] = f"{ind}return {ex};"
        nl[ri] = ""                     # unreachable now
        yield "ret_local:all", head + "\n".join(drop_decl(nl)) + tail
        return
    if len(nont) == 1 and dep[nont[0][0]] == dep[ri] and all(x[0] > nont[0][0] for x in tails) \
            and re.fullmatch(r"-?(?:0x[0-9a-fA-F]+|\d+)[uUlL]*|NULL|0", nont[0][2]):
        i0, ind0, ex0 = nont[0]
        nl = list(lines)
        for i, ind, ex in tails:
            nl[i] = f"{ind}return {ex};"
        nl[ri] = f"{m.group(1)}return {ex0};"
        nl[i0] = None
        yield "ret_local:const-tail", head + "\n".join(drop_decl([x for x in nl if x is not None])) + tail
        # keep the early constant assignment (only the tails return directly)
        nl2 = list(lines)
        for i, ind, ex in tails:
            nl2[i] = f"{ind}return {ex};"
        yield "ret_local:tails", head + "\n".join(nl2) + tail


# ---- seventh round (2026-10-06 worker matches, WORKER_LESSONS.md) -----------
# float_cmp_neg  c.lt.s/c.le.s or bc1t/bc1f differ: `a >= b` <-> `!(a < b)` (HitCheckUwamonoAt)
# field_reread   local cached from a field (`v = p->f;`): re-read the field at a use / all
#                uses / drop the cache (sefCnvDeathEffectNo, ACT_loadMotion)
# ptr_index      `*p++` in a counted loop -> `p[i]`; trailing `p++` -> `p = &E[i];` at the
#                loop top (nmlModelSetPartsVisible, CrossCheckActor)
# unsized_extern target lui/%hi where ours is gp-relative: body-level `extern T X[N]` /
#                `static T X[N]` / `extern T X` -> `extern T X[]` (PrintThumbnail, ACT_loadMotion)
# aligned_local  ours ldl/ldr/sdl/sdr (lwl/lwr/swl/swr) the target lacks: aligned(8)/(16)
#                on a local aggregate (UwamonoBgmFunc)
# bit_split      `x = (E & K) == 0;` / `return (E & K) != 0;` -> mask in its own statement
#                first (objDispCur, scenarioSrcEndChk)
# bool_ifelse    `x = a == b;` / `return a < b;` -> if/else setting 1/0 (scenarioBatExecDst)
# early_return   `if (C) { rest } return R;` -> `if (!C) return R; rest` (scenarioSrcEndChk)

_FCMP_RE = re.compile(r"^c\.\w+\.(s|d)$")
_NEG_REL = {"<": ">=", "<=": ">", ">": "<=", ">=": "<"}
_SWP_REL = {"<": ">", "<=": ">=", ">": "<", ">=": "<="}


def fcmp_shape(rows) -> list[str]:
    """Differing rows holding a c.cond.fmt compare or a bc1t/bc1f(l)."""
    out = []
    for a, m, b in diff_rows(rows):
        if m == " ":
            continue
        for x in (a, b):
            mn = _mn(x) if x else ""
            if _FCMP_RE.match(mn) or mn in ("bc1t", "bc1f", "bc1tl", "bc1fl"):
                out.append(x)
                break
    return out


def _rel_spans(inner: str):
    """Condition spans plus the right-hand sides of `x = ...;` / `return ...;`."""
    seen = set()
    for s0, e0 in _cond_spans(inner):
        if (s0, e0) not in seen:
            seen.add((s0, e0))
            yield s0, e0
    for m in re.finditer(r"(?:\breturn\s+|[^=!<>]=\s*)([^;{}]*?(?:<|>)[^;{}]*);", inner):
        if (m.start(1), m.end(1)) not in seen:
            seen.add((m.start(1), m.end(1)))
            yield m.start(1), m.end(1)


def t_float_cmp_neg(fn, ctx=None):
    """Float compare written negated: `a >= b` -> `!(a < b)` (c.lt.s + bc1f/bc1t
    instead of c.le.s; HitCheckUwamonoAt), the reverse, and the operand swap
    `b <= a`. Diff-gated on a differing c.cond.fmt / bc1t / bc1f."""
    if not (ctx or {}).get("float_cmp_neg"):
        return
    head, inner, tail = _split_fn(fn)
    n = 0
    for s0, e0 in sorted(_rel_spans(inner)):
        cond = inner[s0:e0]
        for m in _REL.finditer(cond):
            op = m.group(1)
            if op not in _NEG_REL:
                continue
            a0 = _operand_start(cond, m.start())
            b1 = _operand_end(cond, m.end())
            L, R = cond[a0:m.start()].strip(), cond[m.end():b1].strip()
            if not L or not R:
                continue
            lead = cond[a0:m.start()][:len(cond[a0:m.start()]) - len(cond[a0:m.start()].lstrip())]
            trail = cond[m.end():b1][len(cond[m.end():b1].rstrip()):]
            b1 -= len(trail)
            wrapped = cond[:a0].rstrip().endswith("!(") and cond[b1:].lstrip().startswith(")")
            if wrapped:
                p0 = cond[:a0].rstrip()
                p0 = p0[:-2]
                p1 = b1 + (len(cond[b1:]) - len(cond[b1:].lstrip())) + 1
                nc = p0 + f"{L} {_NEG_REL[op]} {R}" + cond[p1:]
                yield f"float_cmp_neg:unneg:{L[:10]}{op}{R[:10]}", head + inner[:s0] + nc + inner[e0:] + tail
            if not wrapped:
                nc = cond[:a0] + lead + f"!({L} {_NEG_REL[op]} {R})" + cond[b1:]
                yield f"float_cmp_neg:neg:{L[:10]}{op}{R[:10]}", head + inner[:s0] + nc + inner[e0:] + tail
            nc = cond[:a0] + lead + f"{R} {_SWP_REL[op]} {L}" + cond[b1:]
            yield f"float_cmp_neg:swap:{L[:10]}{op}{R[:10]}", head + inner[:s0] + nc + inner[e0:] + tail
            nc = cond[:a0] + lead + f"!({R} {_SWP_REL[_NEG_REL[op]]} {L})" + cond[b1:]
            yield f"float_cmp_neg:negswap:{L[:10]}{op}{R[:10]}", head + inner[:s0] + nc + inner[e0:] + tail
            n += 1
            if n > 24:
                return


_FIELD = r"[A-Za-z_]\w*(?:(?:->|\.)\w+|\[[^\[\]]+\])+"


def _assigns(ln: str, x: str) -> bool:
    """Does LN assign / increment the lvalue text X (or take its address)?"""
    e = re.escape(x)
    return bool(re.search(r"(?<![\w.>])" + e + r"\s*(?:[-+*/%&|^]|<<|>>)?=(?!=)", ln)
                or re.search(r"(?<![\w.>])" + e + r"\s*(?:\+\+|--)|(?:\+\+|--)\s*" + e + r"\b", ln)
                or re.search(r"&\s*" + e + r"\b(?!\s*->)", ln))   # fold12: &p->f does not write p


def t_field_reread(fn):
    """`v = p->f; ... use(v)` -> re-read `p->f` at one use, at every use, or
    drop the cached copy (sefCnvDeathEffectNo 0.79 -> 0.97: compare against
    the re-read field; ACT_loadMotion: drop a temp so the field is re-read)."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    n = 0
    for i in range(first, len(lines)):
        m = re.match(r"^(\s*)([A-Za-z_]\w*)\s*=\s*(" + _FIELD + r")\s*;\s*$", lines[i])
        if not m or m.group(2) not in locs:
            continue
        v, E = m.group(2), m.group(3)
        base = re.match(r"\w+", E).group(0)
        idx = re.findall(r"\[([^\[\]]+)\]", E)
        watch = [v, E, base] + [w for x in idx for w in re.findall(r"[A-Za-z_]\w*", x)]
        if base == v or v in watch[3:]:
            continue
        vre = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
        uses = []
        end = len(lines)
        for j in range(i + 1, len(lines)):
            if dep[j] < dep[i] or (dep[j] == dep[i] and lines[j].lstrip().startswith("}")):
                end = j
                break
            if any(_assigns(lines[j], w) for w in watch):
                if vre.search(lines[j]) and not _assigns(lines[j], v):
                    pass                       # `x = v` style read before a write of E's parts
                else:
                    end = j
                    break
            for mm in vre.finditer(lines[j]):
                uses.append((j, mm.start(), mm.end()))
            if any(_assigns(lines[j], w) for w in watch):
                end = j + 1
                break
        if not uses:
            continue
        rep = E if re.fullmatch(r"\w+(?:(?:->|\.)\w+|\[[^\[\]]+\])+", E) else f"({E})"

        def sub(sel):
            nl = list(lines)
            for j, a, b in sorted(sel, reverse=True):
                nl[j] = nl[j][:a] + rep + nl[j][b:]
            return nl
        for k, u in enumerate(uses[:6]):
            yield f"field_reread:use{k}:{v}", head + "\n".join(sub([u])) + tail
            n += 1
        if len(uses) > 1:
            yield f"field_reread:all:{v}", head + "\n".join(sub(uses)) + tail
        rest = [j for j in range(first, len(lines)) if j != i and vre.search(lines[j])]
        if set(rest) <= {u[0] for u in uses} and all(len(vre.findall(lines[j])) ==
                                                     sum(1 for u in uses if u[0] == j) for j in rest):
            nl = sub(uses)
            del nl[i]
            yield f"field_reread:drop:{v}", head + "\n".join(nl) + tail
        n += 1
        if n > 30:
            break
    yield from _field_reread_addr(fn)


def _field_reread_addr(fn):
    """`p = &arr[idx];` ... `p->f` -> `arr[idx].f` at one use / every use
    (xglCullingMapSet: re-reading through the full index expression let
    loop-invariant hoisting give the target's move)."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    n = 0
    for i in range(first, len(lines)):
        m = re.match(r"^(\s*)([A-Za-z_]\w*)\s*=\s*&\s*(" + _FIELD + r")\s*;\s*$", lines[i])
        if not m or m.group(2) not in locs:
            continue
        v, E = m.group(2), m.group(3)
        base = re.match(r"\w+", E).group(0)
        idx = [w for x in re.findall(r"\[([^\[\]]+)\]", E) for w in re.findall(r"[A-Za-z_]\w*", x)]
        if base == v or v in idx:
            continue
        watch = [v, base] + idx
        ure = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\s*->")
        uses = []
        for j in range(i + 1, len(lines)):
            if dep[j] < dep[i] or any(_assigns(lines[j], w) for w in watch):
                break
            uses += [(j, mm.start(), mm.end()) for mm in ure.finditer(lines[j])]
        if not uses:
            continue

        def sub(sel):
            nl = list(lines)
            for j, a, b in sorted(sel, reverse=True):
                nl[j] = nl[j][:a] + E + "." + nl[j][b:]
            return nl
        for k, u in enumerate(uses[:4]):
            yield f"field_reread:addr{k}:{v}", head + "\n".join(sub([u])) + tail
        if len(uses) > 1:
            yield f"field_reread:addr-all:{v}", head + "\n".join(sub(uses)) + tail
        n += 1
        if n > 6:
            return


def _loop_body(lines, dep, i):
    """(first body line, end line exclusive, braced) of the loop header at line i."""
    if lines[i].rstrip().endswith("{"):
        o = i
    elif i + 1 < len(lines) and lines[i + 1].strip() == "{":
        o = i + 1
    else:
        return i + 1, i + 2, False
    for j in range(o + 1, len(lines)):
        if dep[j] == dep[o] + 1 and lines[j].lstrip().startswith("}"):
            return o + 1, j, True
    return None


def t_ptr_index(fn):
    """Pointer walks in a counted loop -> indexing by the counter:
    `*p++` -> `p[i]` (nmlModelSetPartsVisible: offs[i] stopped the reuse of a
    preloaded value) and a trailing `p++;` (or `, p++` in the for header) ->
    `p = &E[i];` at the loop top (CrossCheckActor)."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    hre = re.compile(r"^(\s*)for\s*\(\s*(\w+)\s*=\s*0\s*;([^;]*);\s*(\2\s*\+\+|\+\+\s*\2|\2\s*\+=\s*1)"
                     r"(\s*,\s*(\w+)\s*\+\+)?\s*\)\s*\{?\s*$")
    for i, ln in enumerate(lines):
        m = hre.match(ln)
        if not m:
            continue
        I = m.group(2)
        lb = _loop_body(lines, dep, i)
        if not lb:
            continue
        b0, end, braced = lb
        after = "\n".join(lines[end + (1 if braced else 0):])
        body = lines[b0:end]

        def init_of(P):
            for k in range(i - 1, -1, -1):
                mm = re.match(r"^\s*" + re.escape(P) + r"\s*=\s*([^=;][^;]*);\s*$", lines[k])
                if mm and dep[k] <= dep[i]:
                    return mm.group(1).strip()
                if re.search(r"(?<![\w.>])" + re.escape(P) + r"\b", lines[k]) and parse_decl(lines[k]) is None:
                    return None
            return None
        # form A: *P++ -> P[I]
        for P in sorted(set(re.findall(r"\*\s*(\w+)\s*\+\+", "\n".join(body)))):
            pre = re.compile(r"(?<![\w.>])" + re.escape(P) + r"\b")
            occ = sum(len(pre.findall(x)) for x in body)
            walks = sum(len(re.findall(r"\*\s*" + re.escape(P) + r"\s*\+\+", x)) for x in body)
            if walks != 1 or occ != 1 or pre.search(after) or I == P:
                continue
            nl = list(lines)
            for k in range(b0, end):
                nl[k] = re.sub(r"\*\s*" + re.escape(P) + r"\s*\+\+", f"{P}[{I}]", nl[k])
            yield f"ptr_index:deref:{P}", head + "\n".join(nl) + tail
        # form B: trailing `P++;` / header `, P++` -> `P = &E[I];` at the top
        cands = []
        if m.group(6):
            cands.append((m.group(6), "hdr"))
        if braced:
            last = max((k for k in range(b0, end) if lines[k].strip()), default=None)
            if last is not None:
                mm = re.match(r"^\s*(?:(\w+)\s*\+\+|\+\+\s*(\w+)|(\w+)\s*\+=\s*1)\s*;\s*$", lines[last])
                if mm:
                    cands.append((mm.group(1) or mm.group(2) or mm.group(3), last))
        for P, where in cands:
            E = init_of(P)
            pre = re.compile(r"(?<![\w.>])" + re.escape(P) + r"\b")
            if not E or not braced or pre.search(after) or P == I:
                continue
            if any(_assigns(lines[k], P) for k in range(b0, end) if k != where):
                continue
            mE = re.fullmatch(r"&\s*(.+)\[\s*0\s*\]", E)
            tgt = f"&{mE.group(1)}[{I}]" if mE else (f"&{E}[{I}]" if re.fullmatch(r"[\w.>\-]+", E)
                                                      else f"&({E})[{I}]")
            nl = list(lines)
            ind = re.match(r"^(\s*)", next((x for x in body if x.strip()), "    ")).group(1)
            if where == "hdr":
                nl[i] = nl[i].replace(m.group(5), "", 1)
            else:
                nl[where] = None
            at = b0
            while at < end and (not lines[at].strip() or parse_decl(lines[at])):
                at += 1                       # after the block's own declarations (C89)
            nl.insert(at, f"{ind}{P} = {tgt};")
            nl = [x for x in nl if x is not None]
            yield f"ptr_index:addr:{P}", head + "\n".join(nl) + tail


def gp_shape(rows) -> list[dict]:
    """Symbols addressed gp-relative (GPREL16, reloc type 7) on one side and
    through lui/%hi + %lo (5/6) on the other."""
    t: dict = {}
    c: dict = {}
    for a, m, b in diff_rows(rows):
        if m == " ":
            continue
        for x, bag in ((a, t), (b, c)):
            mm = re.search(r"\[(\d+) ([A-Za-z_.$][\w.$]*)", x or "")
            if mm:
                bag.setdefault(mm.group(2), set()).add(int(mm.group(1)))
    out = []
    for s in sorted(set(t) | set(c)):
        if 7 in c.get(s, ()) and t.get(s, set()) & {5, 6}:
            out.append({"sym": s, "dir": "unsized"})
        elif 7 in t.get(s, ()) and c.get(s, set()) & {5, 6}:
            out.append({"sym": s, "dir": "sized"})
    return out


def t_unsized_extern(text, name, ctx=None):
    """Target reaches a global through lui/%lo where ours is gp-relative: gcc
    uses $gp only for a known small size, so declare it unsized `extern T X[];`
    (PrintThumbnail: `static int X[N]` -> `extern int X[]`; ACT_loadMotion:
    `extern int GameLoopState[]`). A scalar `extern T X;` becomes `X[]` with
    the function's uses rewritten to `X[0]`. Only body-level declarations."""
    sh = [x for x in (ctx or {}).get("unsized_extern") or [] if x["dir"] == "unsized"]
    if not sh:
        return
    for x in sh:
        S = x["sym"]
        dre = re.compile(r"^(?P<q>(?:static|extern)\s+)?(?P<ty>(?:(?:const|volatile|unsigned|signed|struct|union)\s+)*"
                         r"\w+(?:\s+(?:int|char|short|long))?\s*\**)\s*" + re.escape(S)
                         + r"\s*(?P<arr>\[[^\]]*\])?\s*;[ \t]*$", re.M)
        md = dre.search(text)
        if not md:
            continue
        ty = md.group("ty").strip()
        new_decl = f"extern {ty} {S}[];"
        nt = text[:md.start()] + new_decl + text[md.end():]
        if md.group("arr") is None:
            loc = find_function(nt, name)
            if not loc:
                continue
            s, b, e = loc
            fn = nt[s:e + 1]
            fn2 = re.sub(r"(?<![\w.>])" + re.escape(S) + r"\b(?!\s*\[)", f"{S}[0]", fn)
            nt = nt[:s] + fn2 + nt[e + 1:]
        if nt != text:
            yield f"unsized_extern:{S}", nt


_UNALIGNED = {"ldl", "ldr", "sdl", "sdr", "lwl", "lwr", "swl", "swr"}
_SCALAR_T = {"int", "char", "short", "long", "float", "double", "unsigned", "signed", "s8", "u8", "s16", "u16",
             "s32", "u32", "s64", "u64", "f32", "f64", "u128", "void"}


def unaligned_shape(rows) -> list[str]:
    """Unaligned ldl/ldr/sdl/sdr (lwl/...) on our side only."""
    t = sum(1 for a, m, b in diff_rows(rows) if a and _mn(a) in _UNALIGNED)
    cs = [b for a, m, b in diff_rows(rows) if b and _mn(b) in _UNALIGNED]
    return cs if len(cs) > t else []


def t_aligned_local(fn, ctx=None):
    """ldl/ldr/sdl/sdr pairs where the target has ld/sd: the local aggregate
    is only 4-aligned; `__attribute__((aligned(8)))` (or 16) on its
    declaration (UwamonoBgmFunc: aligned(8) on a local struct)."""
    if not (ctx or {}).get("aligned_local"):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    locs, first = _locals_block(inner)
    for i in range(first):
        ln = lines[i]
        if "__attribute__" in ln or not ln.strip():
            continue
        p = parse_decl(ln)
        if not p or len(p[2]) != 1 or "=" in p[2][0]:
            continue
        ty, d = p[1], p[2][0].strip()
        words = set(re.sub(r"\b(register|const|volatile|static)\b", "", ty).split())
        if d.startswith("*") or (words <= _SCALAR_T and "[" not in d):
            continue
        for al in (8, 16):
            nl = list(lines)
            nl[i] = re.sub(r";\s*$", f" __attribute__((aligned({al})));", ln)
            yield f"aligned_local:{decl_name(d)}:{al}", head + "\n".join(nl) + tail


def t_bit_split(fn):
    """`x = (E & K) == 0;` / `return (E & K) != 0;` / `x = !(E & K);` -> the
    masked value in its own statement first (objDispCur: `flags = x & 0x40;
    mode = flags == 0;` avoided an sra/xori/andi fold; scenarioSrcEndChk:
    `ret = x; ret &= K; return ret != 0;` gave andi/sltu)."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    pat = re.compile(r"^(?P<ind>\s*)(?:(?P<x>[\w.>\-\[\]]+)\s*=|(?P<ret>return))\s*"
                     r"(?:\(\s*\(\s*(?P<e1>[^;]+?)\s*&\s*(?P<k1>0x[0-9a-fA-F]+|\d+)\s*\)\s*(?P<op>==|!=)\s*0\s*\)"
                     r"|\(\s*(?P<e2>[^;]+?)\s*&\s*(?P<k2>0x[0-9a-fA-F]+|\d+)\s*\)\s*(?P<op2>==|!=)\s*0"
                     r"|!\s*\(\s*(?P<e3>[^;]+?)\s*&\s*(?P<k3>0x[0-9a-fA-F]+|\d+)\s*\))\s*;\s*$")
    for i, ln in enumerate(lines):
        m = pat.match(ln)
        if not m:
            continue
        E = m.group("e1") or m.group("e2") or m.group("e3")
        K = m.group("k1") or m.group("k2") or m.group("k3")
        op = m.group("op") or m.group("op2") or "=="
        if E.count("(") != E.count(")"):
            continue
        ind = m.group("ind")
        lhs = "return " if m.group("ret") else f"{m.group('x')} = "
        forms = [("temp", f"{ind}{{ int _bs = {E} & {K}; {lhs}_bs {op} 0; }}"),
                 ("utemp", f"{ind}{{ unsigned int _bs = {E} & {K}; {lhs}_bs {op} 0; }}"),
                 ("andeq", f"{ind}{{ int _bs = {E}; _bs &= {K}; {lhs}_bs {op} 0; }}")]
        x = m.group("x")
        if x and re.fullmatch(r"\w+", x) and not re.search(r"\b" + re.escape(x) + r"\b", E):
            forms.append(("self", f"{ind}{x} = {E} & {K};\n{ind}{x} = {x} {op} 0;"))
            forms.append(("selfeq", f"{ind}{x} = {E};\n{ind}{x} &= {K};\n{ind}{x} = {x} {op} 0;"))
        for lab, rep in forms:
            nl = list(lines)
            nl[i] = rep
            yield f"bit_split:{lab}:L{i}", head + "\n".join(nl) + tail


def _top_level_rel(s: str) -> bool:
    """S is a single comparison / negation at depth 0 (no ?:, no assignment)."""
    s = s.strip()
    while s.startswith("(") and match_brace(s, 0) == len(s) - 1:
        s = s[1:-1].strip()
    if "?" in s or re.search(r"[^=!<>]=[^=]", s):
        return False
    flat = s
    while True:
        f2 = re.sub(r"\([^()]*\)|\[[^\[\]]*\]", "_", flat)
        if f2 == flat:
            break
        flat = f2
    return bool(_REL.search(flat)) or flat.startswith("!")


def t_bool_ifelse(fn):
    """`x = (a == b);` / `return a < b;` -> if/else setting 1/0, or a default
    then a conditional overwrite (scenarioBatExecDst: if/else 1/0 instead of
    `(x == y)`). Also dmgNumPut (ov01/menu): a bool added to a counter
    `v += (x == 0);` / `v += !x;` / `v = v + (a == b);` (and `-`) ->
    `if (x == 0) v += 1;` (a branch/movz instead of sltu+addu)."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    for i, ln in enumerate(lines):
        ma = re.match(r"^(\s*)([\w.>\-\[\]]+)\s*(?:([-+])=\s*|=\s*\2\s*([-+])\s*)([^;]+);\s*$", ln)
        if ma and parse_decl(ln) is None and _top_level_rel(ma.group(5)):
            ind, v, op = ma.group(1), ma.group(2), ma.group(3) or ma.group(4)
            C = ma.group(5).strip()
            while C.startswith("(") and match_brace(C, 0) == len(C) - 1:
                C = C[1:-1].strip()
            if not re.search(r"(?<![\w.>])" + re.escape(v) + r"\b", C):
                forms = [("acc", f"{ind}if ({C}) {{\n{ind}    {v} {op}= 1;\n{ind}}}"),
                         ("acc_inc", f"{ind}if ({C}) {{\n{ind}    {v}{op}{op};\n{ind}}}")]
                if C.startswith("!") and re.fullmatch(r"![\w.>\-\[\]]+", C):
                    forms.append(("acc_eq0", f"{ind}if ({C[1:]} == 0) {{\n{ind}    {v} {op}= 1;\n{ind}}}"))
                for lab, rep in forms:
                    nl = list(lines)
                    nl[i] = rep
                    yield f"bool_ifelse:{lab}:L{i}", head + "\n".join(nl) + tail
            continue
        m = re.match(r"^(\s*)(?:([\w.>\-\[\]]+)\s*=(?!=)|(return)\s)\s*([^;]+);\s*$", ln)
        if not m or parse_decl(ln) or not _top_level_rel(m.group(4)):
            continue
        ind, x, C = m.group(1), m.group(2), m.group(4).strip()
        if m.group(3):
            forms = [("ret", f"{ind}if ({C}) {{\n{ind}    return 1;\n{ind}}}\n{ind}return 0;")]
        else:
            forms = [("ifelse", f"{ind}if ({C}) {{\n{ind}    {x} = 1;\n{ind}}} else {{\n{ind}    {x} = 0;\n{ind}}}"),
                     ("def0", f"{ind}{x} = 0;\n{ind}if ({C}) {{\n{ind}    {x} = 1;\n{ind}}}")]
            if not re.search(r"(?<![\w.>])" + re.escape(x) + r"\b", C):
                forms.append(("def1", f"{ind}{x} = 1;\n{ind}if (!({C})) {{\n{ind}    {x} = 0;\n{ind}}}"))
            else:
                forms = forms[:1]
        for lab, rep in forms:
            nl = list(lines)
            nl[i] = rep
            yield f"bool_ifelse:{lab}:L{i}", head + "\n".join(nl) + tail


def _invert_simple(C: str) -> str | None:
    """`a OP b` (one comparison, no && / ||) with the operator inverted."""
    s = C.strip()
    if "&&" in s or "||" in s or "?" in s:
        return None
    ms = [m for m in _REL.finditer(s)]
    if len(ms) != 1 or s.count("(") != s.count(")"):
        return None
    m = ms[0]
    inv = {"==": "!=", "!=": "==", "<": ">=", ">=": "<", ">": "<=", "<=": ">"}[m.group(1)]
    return s[:m.start()] + inv + s[m.end():]


_INCDEC = re.compile(r"^(?P<ind>\s*)(?:(?P<x>[\w.>\-\[\]]+)\s*(?P<op>\+\+|--)|(?P<op2>\+\+|--)\s*(?P<x2>[\w.>\-\[\]]+)|"
                     r"(?P<x3>[\w.>\-\[\]]+)\s*(?P<op3>[-+])=\s*(?P<b>[^;]+))\s*;\s*$")
_TERN_ASSIGN = re.compile(r"^(?P<ind>\s*)(?P<x>[\w.>\-\[\]]+)\s*=\s*(?P<e>[^;=]*\?[^;]*:[^;]*);\s*$")


def t_via_temp(fn):
    """A global / field read-modify-write or ternary store routed through a
    block-local copy (MAP_serach, o411): `g--;` -> `{ T t = g; t--; g = t; }`
    (same for ++, += k, -= k) and `g = c ? a : b;` -> `{ T t = c ? a : b; g = t; }`.
    The copy is its own pseudo with its own lifetime, which moves the load /
    store against neighbouring code. Locals are left alone."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    locs = {n for _, n in _scalar_locals(fn)}
    n = 0
    for i, ln in enumerate(lines):
        m = _INCDEC.match(ln)
        rep = None
        if m:
            x = m["x"] or m["x2"] or m["x3"]
            if x in locs:
                continue
            step = (f"_vt{m['op'] or m['op2']};" if (m["op"] or m["op2"]) else f"_vt {m['op3']}= {m['b'].strip()};")
            rep = f"{m['ind']}{{ __typeof__({x}) _vt = {x}; {step} {x} = _vt; }}"
        else:
            m = _TERN_ASSIGN.match(ln)
            if m and m["x"] not in locs:
                rep = f"{m['ind']}{{ __typeof__({m['x']}) _vt = {m['e'].strip()}; {m['x']} = _vt; }}"
        if rep is None:
            continue
        nl = lines[:]
        nl[i] = rep
        yield f"via_temp:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 12:
            return


def t_early_return(fn):
    """`if (C) { rest } return R;` (the if wraps the rest of the function) ->
    `if (!(C)) return R; rest return R;` (scenarioSrcEndChk: the wrapped
    `if (a || b) {...}` became an early return)."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    nz = [k for k, ln in enumerate(lines) if ln.strip()]
    if not nz:
        return
    lastl = nz[-1]
    mret = re.match(r"^\s*return(?:\s+([^;]+))?\s*;\s*$", lines[lastl])
    is_void = re.match(r"^\s*(?:static\s+)?(?:inline\s+)?void\s", head) is not None
    for i, ln in enumerate(lines):
        if dep[i] != 0:
            continue
        m = re.match(r"^(\s*)if\s*\(", ln)
        if not m:
            continue
        cp = match_brace(ln, ln.index("(", m.end() - 1))
        if cp < 0 or ln[cp + 1:].strip() not in ("{", ""):
            continue
        C = ln[ln.index("(", m.end() - 1) + 1:cp]
        lb = _loop_body(lines, dep, i)
        if not lb or not lb[2]:
            continue
        b0, close, _ = lb
        if lines[close].strip() != "}":
            continue                          # `} else ...`
        rest = [k for k in nz if k > close]
        if rest and not (rest == [lastl] and mret):
            continue
        if not rest and not is_void:
            continue
        R = mret.group(1) if rest and mret and mret.group(1) else None
        ind = m.group(1)
        ret = f"return {R};" if R else "return;"
        body = [x[4:] if x.startswith(ind + "    ") else x for x in lines[b0:close]]
        negs = [f"!({C.strip()})"]
        inv = _invert_simple(C)
        if inv:
            negs.append(inv)
        for k, N in enumerate(negs):
            nl = lines[:i] + [f"{ind}if ({N}) {{", f"{ind}    {ret}", f"{ind}}}"] + body
            if R or (rest and mret):
                nl.append(f"{ind}{ret}")
            nl += [""]
            yield f"early_return:{'inv' if k else 'not'}:L{i}", head + "\n".join(nl) + tail


# ---- ninth round (fold3: InitShopSymbol / InitEvsSymbol / InitRetSymbol, counterProc)
# block_barrier    loads hoisted above a store in a pure-move diff: a dead float
#                  store-flag `{ float _sb = p != 0; if (_sb) {} }` keeps a
#                  conditional jump alive through sched1 (deleted only by the
#                  post-reload jump pass) = a sched1 block boundary; inserted at
#                  each statement boundary, alone and with the two statements
#                  around it swapped
# load_width_cast  a load whose width/sign differs (lhu vs lbu, lh vs lhu, lw vs
#                  lbu ...): cast the dereference to the target's width/sign

BLOCK_BARRIER = "{{ float _sb = {p} != 0; if (_sb) {{}} }}"


def block_barrier_shape(rows) -> list[str]:
    """Pure-move diff that moves at least one load (the target loads later,
    after a store sched1 hoisted them above)."""
    mv = moved_insns(rows)
    if mv and any(_LOAD_MN.match(_mn(x)) for x in mv):
        return [x for x in mv if _LOAD_MN.match(_mn(x))]
    return []


def _decl_end(inner: str) -> int:
    """Index of the first statement line of a body: skips blank lines, local
    declarations and `extern` / `static` declarations (which _locals_block
    stops at)."""
    lines = inner.split("\n")
    i = 0
    while i < len(lines):
        ln = lines[i]
        if ln.strip() and not parse_decl(ln) and not re.match(r"\s*(extern|static)\b[^(]*;\s*$", ln) \
                and not re.match(r"\s*(extern|static)\b.*\)\s*;\s*$", ln):
            break
        i += 1
    return i


def _ptr_vars(fn: str) -> list[str]:
    """Pointer params, then pointer locals (names)."""
    out = []
    for _, _, p in _params(fn):
        m = re.match(r"^[^()\[\]]*\*\s*(\w+)\s*$", p.strip())
        if m:
            out.append(m.group(1))
    _, inner, _ = _split_fn(fn)
    first = _decl_end(inner)
    for ln in inner.split("\n")[:first]:
        p = parse_decl(ln)
        for d in (p[2] if p else []):
            d0 = d.split("=")[0].strip()
            if d0.startswith("*") and "[" not in d0 and "(" not in d0:
                v = decl_name(d0)
                if v not in out:
                    out.append(v)
    return out


def t_block_barrier(fn, ctx=None):
    """Insert BLOCK_BARRIER (a sched1-only block boundary) between two
    statements of one block; also with the two statements swapped around it.
    InitShopSymbol: barrier right after the `update` store; InitRetSymbol /
    InitEvsSymbol: `update` store, barrier, then the `count` store that came
    before it (C order U,B,C,F)."""
    if not (ctx or {}).get("block_barrier"):
        return
    ps = _ptr_vars(fn)
    if not ps:
        return
    p = ps[0]
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    first = _decl_end(inner)
    def decl_follows(i):      # a declaration later in the same block: no statement may precede it
        for j in range(i, len(lines)):
            if dep[j] < dep[i]:
                return False
            if dep[j] == dep[i] and (parse_decl(lines[j]) or re.match(r"\s*(volatile|const|register)\b", lines[j])):
                return True
        return False
    st = [i for i in range(first, len(lines))
          if lines[i].strip() and (_simple_stmt(lines[i]) or re.match(r"\s*(if|for|while|do|switch)\b", lines[i]))
          and not decl_follows(i)]
    for i in st:
        ind = re.match(r"\s*", lines[i]).group(0)
        bar = ind + BLOCK_BARRIER.format(p=p)
        nl = list(lines)
        nl.insert(i, bar)
        yield f"block_barrier:at:{i}", head + "\n".join(nl) + tail
    for a, b in zip(st, st[1:]):
        if b != a + 1 or dep[a] != dep[b] or not (_simple_stmt(lines[a]) and _simple_stmt(lines[b])):
            continue
        ind = re.match(r"\s*", lines[a]).group(0)
        bar = ind + BLOCK_BARRIER.format(p=p)
        nl = list(lines)
        nl[a:b + 1] = [lines[b], bar, lines[a]]
        yield f"block_barrier:swap:{a}", head + "\n".join(nl) + tail


_WIDTH_T = {"lb": ["signed char"], "lbu": ["unsigned char"], "lh": ["short"], "lhu": ["unsigned short"],
            "lw": ["int", "unsigned int"], "lwu": ["unsigned int"], "ld": ["long long"]}


def load_width_shape(rows) -> list[dict]:
    """Rows where both sides load (same row) with another width or sign."""
    out = []
    for a, m, b in diff_rows(rows):
        if m != "|" or not a or not b:
            continue
        ma, mb = _mn(a), _mn(b)
        if ma != mb and ma in _WIDTH_T and mb in _WIDTH_T:
            out.append({"target": ma, "ours": mb})
    return out


_DEREF_RE = re.compile(r"(?<![\w)\]])\*\s*(\(\s*[\w\s]+\*\s*\)\s*)?(?=[\w(])")
_FIELD_RE = re.compile(r"(?<![\w.>&*])(\w+(?:\s*(?:->|\.)\s*\w+|\s*\[[^\[\]]+\])+)(?!\s*(?:\(|->|\.|\[))")


def t_load_width_cast(fn, ctx=None):
    """Cast a dereference to the target's load width/sign: `*E` / `*(U *)E`
    -> `*(T *)E`, a field/element read `X->f` -> `*(T *)&X->f`, or the
    value `(T)X->f`. counterProc: `*(unsigned char *)dataTecGet(x)` for lbu
    where the declared type gave lhu. Diff-gated (load_width_shape)."""
    shp = (ctx or {}).get("load_width_cast")
    if not shp:
        return
    tys = []
    for d in shp:
        for t in _WIDTH_T.get(d["target"], []):
            if t not in tys:
                tys.append(t)
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    _, first = _locals_block(inner)
    n = 0
    for i in range(first, len(lines)):
        ln = lines[i]
        if parse_decl(ln):
            continue
        # read part of the line: right of a plain `=`, or the whole line for if/while/return/calls
        m = re.match(r"^(\s*[^=;(]*?(?<![=!<>+\-*/%&|^])=(?!=))(.*)$", ln)
        lo = len(m.group(1)) if m and not re.match(r"\s*(if|while|for|return)\b", ln) else 0
        sites = []
        for mm in _DEREF_RE.finditer(ln, lo):
            sites.append(("deref", mm.start(), mm.end(), mm.group(1)))
        for mm in _FIELD_RE.finditer(ln, lo):
            sites.append(("field", mm.start(1), mm.end(1), mm.group(1)))
        for kind, s0, s1, g in sites:
            for t in tys:
                if kind == "deref":
                    if g and re.sub(r"\s+", " ", g.strip("() *")).strip() == t:
                        continue
                    new = ln[:s0] + f"*({t} *)" + ln[s1:]
                    forms = [("deref", new)]
                else:
                    forms = [("addr", ln[:s0] + f"*({t} *)&{g}" + ln[s1:]),
                             ("val", ln[:s0] + f"({t}){g}" + ln[s1:])]
                for lab, new in forms:
                    nl = list(lines)
                    nl[i] = new
                    yield f"load_width_cast:{lab}:{t.replace(' ', '_')}:L{i}", head + "\n".join(nl) + tail
                    n += 1
                    if n >= 60:
                        return
    # packed-field split (o420 FontTexChange 0.68 -> 0.97): the target loads the
    # whole word (lw) where ours narrowed `p->xy >> 16` / `p->xy & 0xFFFF` to
    # lhu: read the field into a local first, then shift / mask the local
    wid = {"lb": 1, "lbu": 1, "lh": 2, "lhu": 2, "lw": 4, "lwu": 4, "ld": 8}
    if not any(wid.get(d["target"], 0) > wid.get(d["ours"], 9) for d in shp):
        return
    first = _decl_end(inner)
    pk = re.compile(r"(?<![\w.>&*])(\w+(?:\s*(?:->|\.)\s*\w+|\s*\[[^\[\]]+\])+)(?=\s*(?:>>|&)\s*(?:0x[0-9a-fA-F]+|\d+))")
    seen = set()
    for i in range(first, len(lines)):
        if not _simple_stmt(lines[i]):
            continue
        for mm in pk.finditer(lines[i]):
            e = mm.group(1)
            if e in seen:
                continue
            seen.add(e)
            nl = list(lines)
            ind = re.match(r"\s*", nl[i]).group(0)
            pat = re.compile(r"(?<![\w.>&*])" + re.escape(e) + r"(?![\w\[])")
            root = re.match(r"\w+", e).group(0)
            dep = _depths(nl)
            j = i
            while j < len(nl) and _simple_stmt(nl[j]) and dep[j] == dep[i]:
                lhs = _stmt_lhs(nl[j])
                nl[j] = pat.sub("_pk", nl[j])
                if root in _writes(nl[j]) or (lhs and pat.search(lhs[1] if len(lhs) > 1 else "")):
                    break
                j += 1
            n1 = list(nl)
            n1.insert(i, f"{ind}_pk = {e};")
            n1.insert(first, "    unsigned int _pk;")
            yield f"load_width_cast:split:L{i}", head + "\n".join(n1) + tail
            # each `_pk >> K` / `_pk & M` in its own local right after the load
            # (FontTexChange: lo = xy & 0xFFFF; hi = xy >> 16)
            parts = []
            for k in range(i, j + 1 if j < len(nl) else j):
                for m2 in re.finditer(r"\b_pk\s*(>>|&)\s*(0x[0-9a-fA-F]+|\d+)", nl[k]):
                    if m2.group(0) not in parts:
                        parts.append(m2.group(0))
            if not parts:
                continue
            n2 = list(nl)
            for k in range(i, min(j + 1, len(n2))):
                for q, ex in enumerate(parts):
                    n2[k] = n2[k].replace(ex, f"_pk{q}")
            for q, ex in reversed(list(enumerate(parts))):
                n2.insert(i, f"{ind}_pk{q} = {ex};")
            n2.insert(i, f"{ind}_pk = {e};")
            n2.insert(first, "    unsigned int _pk" + "".join(f", _pk{q}" for q in range(len(parts))) + ";")
            yield f"load_width_cast:split_parts:L{i}", head + "\n".join(n2) + tail


# ---- fold4 (2026-10-07 AI matches o422-o436) --------------------------------

def branch_likely_shape(rows) -> list[dict]:
    """Aligned rows where only the branch-likely bit differs (beqz vs beqzl,
    same operands): reorg's annul choice (MenuFaceEpidGet)."""
    out = []
    for a, m, b in diff_rows(rows):
        if m != "|" or not a or not b:
            continue
        ma, mb = _mn(a), _mn(b)
        if (ma != mb and BRANCH_RE.match(ma) and BRANCH_RE.match(mb) and "bal" not in (ma, mb)
                and ma.rstrip("l") == mb.rstrip("l") and _ops_nt(a) == _ops_nt(b)):
            out.append({"target": ma, "cand": mb})
    return out


_HI_RE = re.compile(r"\[HI16 ([\w.$]+)\]")


def hi_extra_shape(rows) -> list[str]:
    """Symbols with more `lui %hi(S)` on our side than in the target: the %hi
    base is re-materialized per access instead of kept in one register
    (Check_Undu: raw-offset stores into a global)."""
    dr = diff_rows(rows)
    t: dict = {}
    c: dict = {}
    for a, _, b in dr:
        for x, bag in ((a, t), (b, c)):
            if x and _mn(x) == "lui":
                for s in _HI_RE.findall(x):
                    bag[s] = bag.get(s, 0) + 1
    return sorted(s for s in c if c[s] > t.get(s, 0))


_SCALAR_SIZE = {"char": 1, "signed char": 1, "unsigned char": 1, "u8": 1, "s8": 1,
                "short": 2, "signed short": 2, "unsigned short": 2, "u16": 2, "s16": 2,
                "int": 4, "signed int": 4, "unsigned int": 4, "unsigned": 4, "u32": 4, "s32": 4,
                "long": 4, "unsigned long": 4, "float": 4, "f32": 4,
                "long long": 8, "unsigned long long": 8, "u64": 8, "s64": 8, "double": 8, "f64": 8}
# `*(T *)((char *)BASE + K)` (m2c's raw-offset access); BASE = `&S`, `(&S)`, `S`, `(S)`
_RAW_OFF = re.compile(r"\*\s*\(\s*(?P<T>(?:const\s+|volatile\s+)*[A-Za-z_][\w ]*?(?:\s*\*)*)\s*\*\s*\)\s*"
                      r"\(\s*\(\s*(?:unsigned\s+char|signed\s+char|char|u8|s8)\s*\*\s*\)\s*"
                      r"(?P<B>\(\s*&?\s*[A-Za-z_]\w*\s*\)|&\s*[A-Za-z_]\w*|[A-Za-z_]\w*)\s*\+\s*"
                      r"(?P<K>0x[0-9a-fA-F]+|\d+)\s*\)")


def _raw_sites(fn: str):
    """[(start, end, T, base expr, base identifier, K)] raw-offset accesses in FN."""
    out = []
    for m in _RAW_OFF.finditer(fn):
        B = m.group("B")
        ident = re.search(r"[A-Za-z_]\w*", B).group(0)
        T = re.sub(r"\s+", " ", m.group("T")).strip()
        out.append((m.start(), m.end(), T, B, ident, int(m.group("K"), 0)))
    return out


def _view_ok(T: str, K: int) -> bool:
    """The field lands at exactly K in `struct { char _p[K]; T v; }`."""
    core = re.sub(r"\b(const|volatile)\b", "", T).strip()
    al = 4 if core.endswith("*") else _SCALAR_SIZE.get(core)
    if al is None:
        return K % 16 == 0
    return K % al == 0


def _view_union(tag: str, fields: dict) -> str:
    parts = []
    for K, T in sorted(fields.items()):
        pad = f"char _p[{K}]; " if K else ""
        parts.append(f"struct {{ {pad}{T} v; }} o_{K:x};")
    return f"union {tag} {{ " + " ".join(parts) + " };\n"


def _field_view(text: str, name: str, idents, fam: str, cap: int = 4):
    """Rewrite raw-offset accesses through IDENTS as typed field views:
    `*(T *)((char *)&S + K)` -> `((union _gfv_S *)&S)->o_K.v` (one union of
    per-offset structs: every field at its exact offset), or one struct type
    per offset (`each`)."""
    loc = find_function(text, name)
    if not loc:
        return
    s, b, e = loc
    fn = text[s:e + 1]
    sites = [x for x in _raw_sites(fn) if x[4] in idents]
    n = 0
    for ident in sorted({x[4] for x in sites}):
        mine = [x for x in sites if x[4] == ident]
        if not all(_view_ok(x[2], x[5]) for x in mine):
            continue
        fields: dict = {}
        if any(fields.setdefault(x[5], x[2]) != x[2] for x in mine):
            continue                      # two types at one offset: leave it
        tag = f"_{fam[:1]}fv_{ident}"
        if tag in text:
            continue
        nf = fn
        for st, en, T, B, _, K in sorted(mine, reverse=True):
            nf = nf[:st] + f"((union {tag} *){B})->o_{K:x}.v" + nf[en:]
        yield f"{fam}:{ident}", text[:s] + _view_union(tag, fields) + nf + text[e + 1:]
        nf = fn
        defs = ""
        for K, T in sorted(fields.items()):
            pad = f"char _p[{K}]; " if K else ""
            defs += f"struct {tag}_{K:x} {{ {pad}{T} v; }};\n"
        for st, en, T, B, _, K in sorted(mine, reverse=True):
            nf = nf[:st] + f"((struct {tag}_{K:x} *){B})->v" + nf[en:]
        yield f"{fam}:each:{ident}", text[:s] + defs + nf + text[e + 1:]
        n += 1
        if n >= cap:
            return


def t_global_field_view(text, name, ctx=None):
    """Check_Undu (0.78 -> 1.0): raw-offset stores into a global
    `*(long long *)((char *)&G + K) = v;` re-materialize `lui %hi(G)` per store;
    a typed struct view `((View *)&G)->fK` keeps the %hi base in one register.
    Diff-gated: a symbol with more lui %hi on our side."""
    syms = (ctx or {}).get("global_field_view")
    if not syms:
        return
    yield from _field_view(text, name, set(syms), "global_field_view")


def t_ptr_field_view(text, name, ctx=None):
    """The same typed view for raw-offset accesses through a pointer
    local/param (ACT_updateSequence: typed fields instead of raw u32* offsets
    avoided a reload after stores, struct vs scalar alias rules). Text-gated:
    >= 2 raw-offset accesses through one base."""
    loc = find_function(text, name)
    if not loc:
        return
    fn = text[loc[0]:loc[2] + 1]
    cnt: dict = {}
    for x in _raw_sites(fn):
        if "&" not in x[3]:
            cnt[x[4]] = cnt.get(x[4], 0) + 1
    gsyms = set((ctx or {}).get("global_field_view") or [])
    yield from _field_view(text, name, {k for k, v in cnt.items() if v >= 2 and k not in gsyms},
                           "ptr_field_view", cap=3)
    # fold10 (o509 InitDrill, o494 cmdStatGet): the TU's real struct fields
    # (-gstabs layouts) instead of a union view; one raw site is enough
    yield from _tu_field_views(text, name, ctx, "ptr_field_view")


_LOOP_HDR = re.compile(r"^\s*(for\s*\(|while\s*\(|do\b)")


def t_reload_base(fn):
    """`n = w->node;` ... loop ... loop: re-assign the base pointer local right
    before a later loop (`n = w->node;` again) so gcc does not CSE the value
    across loops (subTreeLineDraw_type_3)."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    n = 0
    for i in range(first, len(lines)):
        m = re.match(r"^(\s*)([A-Za-z_]\w*)\s*=\s*(" + _FIELD + r")\s*;\s*$", lines[i])
        if not m or m.group(2) not in locs:
            continue
        v, E = m.group(2), m.group(3)
        base = re.match(r"\w+", E).group(0)
        if base == v or "(" in E:
            continue
        vre = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
        sites = []
        for j in range(i + 1, len(lines)):
            if dep[j] < dep[i]:
                break
            if _assigns(lines[j], v) or _assigns(lines[j], base):
                break
            if dep[j] == dep[i] and _LOOP_HDR.match(lines[j]) and \
                    any(vre.search(x) for x in lines[j:]) and not lines[j - 1].strip() == f"{v} = {E};":
                sites.append(j)
        if not sites:
            continue
        ind = m.group(1)
        sel = [[j] for j in sites[:4]] + ([sites] if len(sites) > 1 else [])
        for ss in sel:
            nl = list(lines)
            for j in sorted(ss, reverse=True):
                nl.insert(j, f"{ind}{v} = {E};")
            yield f"reload_base:{v}:L{'+'.join(map(str, ss))}", head + "\n".join(nl) + tail
            n += 1
            if n > 12:
                return


_CASE_K = r"(?:-?(?:0x[0-9a-fA-F]+|\d+)[uUlL]*|[A-Z_][A-Z0-9_]*)"


def _eq_chain(cond: str):
    """`V == K` / `K == V` / `V == K1 || V == K2 ...` -> (V, [K...]) else None."""
    out_v, ks = None, []
    s = cond.strip()
    while s.startswith("(") and match_brace(s, 0) == len(s) - 1:
        s = s[1:-1].strip()
    for p in s.split("||"):
        p = p.strip()
        while p.startswith("(") and match_brace(p, 0) == len(p) - 1:
            p = p[1:-1].strip()
        m = re.fullmatch(r"(" + _FIELD + r"|[A-Za-z_]\w*)\s*==\s*(" + _CASE_K + r")", p) or \
            re.fullmatch(r"(" + _CASE_K + r")\s*==\s*(" + _FIELD + r"|[A-Za-z_]\w*)", p)
        if not m:
            return None
        V, K = (m.group(1), m.group(2)) if not re.fullmatch(_CASE_K, m.group(1)) else (m.group(2), m.group(1))
        if out_v is not None and V.replace(" ", "") != out_v.replace(" ", ""):
            return None
        out_v = V
        ks.append(K)
    return (out_v, ks) if out_v else None


def _free_jump(body: str) -> bool:
    """A `break` / `continue` not enclosed by a loop / switch inside BODY."""
    s = body
    while True:
        m = re.search(r"\b(switch|for|while|do)\b", s)
        if not m:
            break
        o = s.find("{", m.end())
        c = match_brace(s, o) if o >= 0 else -1
        if c < 0:
            s = s[:m.start()] + s[m.end():]
            continue
        s = s[:m.start()] + s[c + 1:]
    return bool(re.search(r"\b(break|continue)\b", s))


def t_if_to_switch(fn):
    """if / else-if chain comparing one variable against constants -> switch
    (tskFileEx: the outer `switch (state)` gave the target's branch shape)."""
    head, inner, tail = _split_fn(fn)
    for ls, end, brs, has_else, ind in _if_chains(inner):
        n_cond = len(brs) - (1 if has_else else 0)
        if n_cond < 2 and not (n_cond == 1 and has_else):
            continue
        conds = []
        prev = ls
        for k in range(n_cond):
            o = brs[k][0]
            mm = re.search(r"\bif\s*\(", inner[prev:o])
            if not mm:
                conds = None
                break
            cp = match_brace(inner, prev + mm.end() - 1)
            conds.append(inner[prev + mm.end():cp])
            prev = brs[k][1] + 1
        if not conds:
            continue
        ec = [_eq_chain(c) for c in conds]
        if any(x is None for x in ec) or len({x[0].replace(" ", "") for x in ec}) != 1:
            continue
        allk = [k for x in ec for k in x[1]]
        if len(set(allk)) != len(allk):
            continue
        bodies = [inner[o + 1:c] for o, c in brs]
        if any(_free_jump(bd) for bd in bodies):
            continue
        V = ec[0][0]
        arms = list(zip([x[1] for x in ec], bodies[:n_cond]))

        def render(arms, dflt):
            s = f"switch ({V}) {{\n"
            for ks, bd in arms:
                s += "".join(f"{ind}case {k}:\n" for k in ks) + f"{ind}    {{{bd}}}\n{ind}    break;\n"
            if dflt is not None:
                s += f"{ind}default:\n{ind}    {{{dflt}}}\n{ind}    break;\n"
            return s + f"{ind}}}"
        dflt = bodies[-1] if has_else else None
        yield f"if_to_switch:{V[:16]}", head + inner[:ls] + ind + render(arms, dflt) + inner[end + 1:] + tail
        srt = sorted(arms, key=lambda a: _ival(a[0][0]) if _ival(a[0][0]) is not None else 1 << 40)
        if srt != arms:
            yield f"if_to_switch:sorted:{V[:16]}", head + inner[:ls] + ind + render(srt, dflt) + inner[end + 1:] + tail


_RP_LHS = re.compile(r"^(?P<ind>\s*)(?P<lhs>[A-Za-z_]\w*(?:\s*(?:->|\.)\s*\w+|\s*\[[^\[\]]+\])+)"
                     r"\s*(?P<op>(?:[-+*/%&|^]|<<|>>)?=)(?!=)\s*(?P<rhs>[^;]*);\s*$")


def _rp_split(lhs: str):
    """`PTR->f` / `ELEM.f` -> (pointer expression, field) when PTR / ELEM is not
    a plain identifier (that one is already a local)."""
    lhs = re.sub(r"\s+", "", lhs)
    d = 0
    cut = None
    for i, ch in enumerate(lhs):
        if ch == "[":
            d += 1
        elif ch == "]":
            d -= 1
        elif d == 0 and (lhs.startswith("->", i) or ch == "."):
            cut = i
    if cut is None:
        return None
    pre = lhs[:cut]
    sep = "->" if lhs.startswith("->", cut) else "."
    fld = lhs[cut + len(sep):]
    if re.fullmatch(r"[A-Za-z_]\w*", pre):
        return None
    return (pre if sep == "->" else f"&{pre}"), fld


def t_reuse_ptr_local(fn):
    """Runs of stores through different pointer expressions (`a->e->x = 0;
    b->f->x = 0;`, `p[4].x = ..; p[5].x = ..;`) -> one pointer local reassigned
    per run (`e = a->e; e->x = 0; e = b->f; e->x = 0;`; DrillPowerOffFunc,
    sprt_put), or a block-scoped pointer per run."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    _, first = _locals_block(inner)
    runs = []          # [ptr, [line idx], [field]]
    for i in range(first, len(lines)):
        m = _RP_LHS.match(lines[i])
        sp = _rp_split(m.group("lhs")) if m else None
        if not sp or "++" in lines[i] or "--" in lines[i] or "(" in sp[0]:
            continue
        if runs and runs[-1][0] == sp[0] and runs[-1][1][-1] == i - 1:
            runs[-1][1].append(i)
        else:
            runs.append([sp[0], [i]])
    if len({r[0] for r in runs}) < 2:
        return
    runs = runs[:10]

    def rewrite(nl, r, var):
        for i in r[1]:
            m = _RP_LHS.match(lines[i])
            _, fld = _rp_split(m.group("lhs"))
            nl[i] = f"{m.group('ind')}{var}->{fld} {m.group('op')} {m.group('rhs')};"
    # one reused local (typeof the first run's pointer)
    nl = list(lines)
    for r in reversed(runs):
        rewrite(nl, r, "_rp")
        i0 = r[1][0]
        ind = _RP_LHS.match(lines[i0]).group("ind")
        nl.insert(i0, f"{ind}_rp = (__typeof__(_rp))({r[0]});" if r is not runs[0] else f"{ind}_rp = {r[0]};")
    nl.insert(first, f"    __typeof__({runs[0][0]}) _rp;")
    yield "reuse_ptr_local:one", head + "\n".join(nl) + tail
    # block-scoped pointer per run
    nl = list(lines)
    for r in reversed(runs):
        rewrite(nl, r, "_rp")
        i0, i1 = r[1][0], r[1][-1]
        ind = _RP_LHS.match(lines[i0]).group("ind")
        nl[i1] = nl[i1] + f"\n{ind}}}"
        nl[i0] = f"{ind}{{ __typeof__({r[0]}) _rp = {r[0]};\n" + nl[i0]
    yield "reuse_ptr_local:block", head + "\n".join(nl) + tail
    # one reused local per same-shape cluster (`&p[4]`, `&p[5]`: sprt_put's
    # per-qword bases), and only the multi-statement runs
    groups: dict = {}
    for r in runs:
        groups.setdefault(("shape", re.sub(r"\[[^\[\]]*\]", "[]", r[0])), []).append(r)
        f0 = re.match(r"\w+", _rp_split(_RP_LHS.match(lines[r[1][0]]).group("lhs"))[1]).group(0)
        groups.setdefault(("field", f0), []).append(r)     # same field written: likely one type
    sel, seen_g = [], set()
    for (kind, key), g in groups.items():
        ids = tuple(r[1][0] for r in g)
        if 2 <= len(g) < len(runs) and ids not in seen_g:
            seen_g.add(ids)
            sel.append((f"{kind}:{key[:12]}", g))
    big = [r for r in runs if len(r[1]) > 1]
    if 2 <= len(big) < len(runs):
        sel.append(("runs", big))
    for lab, grp in sel[:5]:
        nl = list(lines)
        for r in reversed(grp):
            rewrite(nl, r, "_rp")
            i0 = r[1][0]
            ind = _RP_LHS.match(lines[i0]).group("ind")
            nl.insert(i0, f"{ind}_rp = (__typeof__(_rp))({r[0]});" if r is not grp[0] else f"{ind}_rp = {r[0]};")
        nl.insert(first, f"    __typeof__({grp[0][0]}) _rp;")
        yield f"reuse_ptr_local:{lab}", head + "\n".join(nl) + tail


_SIDEFX = re.compile(r"\(|\+\+|--|=")


def _countdown_init(inner: str, at: int, C: str):
    """`C = N;` right before position AT (an `if (N != 0) {` / `if (N > 0) {` /
    `if (N) {` wrapper line may sit between them or around both):
    ((init line start, end), N, (wrapper line start, its '{') or None)."""
    lines_before = inner[:at]
    ls = [m.start() for m in re.finditer(r"(?m)^", lines_before)]
    ls = ls[-4:]
    init = wrap = None
    for k in range(len(ls) - 2, -1, -1):
        seg = inner[ls[k]:ls[k + 1]]
        mi = re.fullmatch(r"[ \t]*" + re.escape(C) + r"\s*=\s*([^;=][^;]*);\s*\n", seg)
        mw = re.fullmatch(r"[ \t]*if\s*\((.*)\)\s*\{\s*\n", seg)
        if mi and not init:
            init = ((ls[k], ls[k + 1]), mi.group(1).strip())
        elif mw and not wrap:
            wrap = (ls[k], ls[k] + seg.rindex("{"), mw.group(1))
        elif seg.strip():
            break
        if init and wrap:
            break
    if not init:
        # fold12 (o530 m2c): `c = 8;` further up among plain statements that
        # do not mention c (m2c interleaves the counter init with stores)
        allb = [m.start() for m in re.finditer(r"(?m)^", lines_before)]
        cre = re.compile(r"(?<![\w.>])" + re.escape(C) + r"\b")
        for k in range(len(allb) - 2, max(-1, len(allb) - 16), -1):
            seg = inner[allb[k]:allb[k + 1]]
            mi = re.fullmatch(r"[ \t]*" + re.escape(C) + r"\s*=\s*([^;=][^;]*);\s*\n", seg)
            if mi:
                return (allb[k], allb[k + 1]), mi.group(1).strip(), None
            st = seg.strip()
            if st and (cre.search(seg) or not st.endswith(";") or "{" in st or "}" in st
                       or re.match(r"(case|default|return|break|goto|continue)\b", st)):
                return None
        return None
    if wrap:
        c = re.sub(r"\s+", "", wrap[2])
        n = re.sub(r"\s+", "", init[1])
        if c not in (n, f"{n}!=0", f"{n}>0", f"({n})!=0", f"({n})>0", f"{C}!=0", f"{C}>0", C):
            wrap = None
    return init[0], init[1], (wrap[0], wrap[1]) if wrap else None


def t_count_up_loop(fn):
    """Countdown loop whose counter is not used in the body -> `for (i = 0;
    i < n; i++)` (RSRC_dispose: loop.c reverses the biv itself and emits the
    init after the hoisted invariants; ACT_initSequence). Handles
    `c = n; do {B} while (--c != 0);` (also `--c`, `--c > 0`, `--c >= 0` = n+1
    passes, or a trailing `c--;` + `while (c != 0)`), an `if (n != 0) {..}`
    wrapper around it, `while (c--) {B}` and `for (c = n; c != 0; c--) {B}`.
    Fold12 (o530 MenuTecMenuMain): m2c's `c = 1; ...; do { c -= 1; B } while
    (c >= 0);` - the decrement anywhere at the body's top level and the init
    up to 15 plain statements earlier; then ptr_index turns the walked
    pointers into `&w->win[i]` (pre-fix 0.524 -> 0.572 -> match)."""
    head, inner, tail = _split_fn(fn)

    def mk(C, N, extra, body, ind):
        lim = f"{C} <= {N}" if extra else f"{C} < {N}"
        if extra and _ival(N) is not None:
            lim = f"{C} < {_ival(N) + 1}"
        return f"for ({C} = 0; {lim}; {C}++) {{{body}}}"

    def unused(C, body, after):
        cre = re.compile(r"(?<![\w.>])" + re.escape(C) + r"\b")
        if cre.search(body):
            return False
        m = cre.search(after)
        return not m or bool(re.match(r"\s*=(?!=)", after[m.end():]))

    out = []
    # do { B } while (COND);
    for m in re.finditer(r"\bdo\s*\{", inner):
        ob = m.end() - 1
        cb = match_brace(inner, ob)
        wm = re.match(r"\s*while\s*\(", inner[cb + 1:]) if cb > 0 else None
        if not wm:
            continue
        cp = match_brace(inner, cb + 1 + wm.end() - 1)
        sm = re.match(r"\s*;", inner[cp + 1:])
        if cp < 0 or not sm:
            continue
        end = cp + 1 + sm.end()
        cond = inner[cb + 1 + wm.end():cp].strip()
        body = inner[ob + 1:cb]
        C, extra = None, False
        mc = re.fullmatch(r"--\s*(\w+)\s*(?:(!=|>)\s*0)?|--\s*(\w+)\s*>=\s*0|\(\s*--\s*(\w+)\s*\)\s*!=\s*0", cond)
        if mc:
            C = mc.group(1) or mc.group(3) or mc.group(4)
            extra = bool(mc.group(3))
        else:
            mc = re.fullmatch(r"(\w+)\s*(?:(!=|>)\s*0)?|(\w+)\s*>=\s*0", cond)
            lm = re.search(r"\n?[ \t]*(?:(\w+)\s*--|--\s*(\w+)|(\w+)\s*-=\s*1)\s*;\s*$", body)
            if mc and not lm:
                # fold12 (o530 m2c form): `do { c -= 1; ...; p += K; } while (c >= 0);`
                # - the decrement anywhere at the body's top level
                cv = mc.group(1) or mc.group(3)
                for lm2 in re.finditer(r"\n[ \t]*(?:(" + re.escape(cv) + r")\s*--|--\s*(" + re.escape(cv)
                                       + r")|(" + re.escape(cv) + r")\s*-=\s*1)\s*;[ \t]*(?=\n)", body):
                    pre_b = body[:lm2.start()]
                    if pre_b.count("{") == pre_b.count("}"):
                        lm = lm2
                        break
            if mc and lm:
                C = mc.group(1) or mc.group(3)
                if C != (lm.group(1) or lm.group(2) or lm.group(3)):
                    continue
                extra = bool(mc.group(3))
                body = body[:lm.start()] + (body[lm.end():] if lm.end() < len(body.rstrip()) else "\n")
        if not C:
            continue
        r = _countdown_init(inner, m.start(), C)
        if not r or not unused(C, body, inner[end:]):
            continue
        (i0, i1), N, wrap = r
        if _SIDEFX.search(N):
            continue
        lst = inner.rfind("\n", 0, m.start()) + 1
        ind = inner[lst:m.start()] if not inner[lst:m.start()].strip() else ""
        ni = inner[:m.start()] + mk(C, N, extra, body, ind) + inner[end:]
        ni = ni[:i0] + ni[i1:]                       # drop the init (it precedes the loop)
        out.append(("do", ni))
        if wrap and not extra:
            ws, ob2 = wrap
            cb2 = match_brace(inner, ob2)
            if cb2 > 0 and not inner[end:cb2].strip() and not inner[ob2 + 1:m.start()].replace(inner[i0:i1], "").strip():
                wind = re.match(r"[ \t]*", inner[ws:]).group(0)
                lo = min(ws, i0)
                mid = inner[i1:ws] if i0 < ws else ""
                out.append(("do-unwrap", inner[:lo] + mid + wind + mk(C, N, False, body, wind) + inner[cb2 + 1:]))
    # while (C--) { B } / while (C-- != 0) / while (C-- > 0)
    for m in re.finditer(r"\bwhile\s*\(\s*(\w+)\s*--\s*(?:(?:!=|>)\s*0\s*)?\)\s*\{", inner):
        C = m.group(1)
        ob = m.end() - 1
        cb = match_brace(inner, ob)
        if cb < 0 or re.search(r"\}\s*$", inner[:m.start()]):
            continue
        body = inner[ob + 1:cb]
        pre = inner[:m.start()]
        im = re.search(r"(?:^|\n)([ \t]*)" + re.escape(C) + r"\s*=\s*([^;=][^;]*);\s*\n[ \t]*$", pre)
        if not im or _SIDEFX.search(im.group(2)) or not unused(C, body, inner[cb + 1:]):
            continue
        start = im.start() + (1 if pre[im.start()] == "\n" else 0)
        out.append(("while", inner[:start] + im.group(1) + mk(C, im.group(2).strip(), False, body, im.group(1))
                    + inner[cb + 1:]))
    # for (C = N; C != 0 / C > 0; C--) { B }
    for m in re.finditer(r"\bfor\s*\(\s*(\w+)\s*=\s*([^;=][^;]*);\s*\1\s*(?:!=|>)\s*0\s*;\s*(?:\1\s*--|--\s*\1|\1\s*-=\s*1)\s*\)\s*\{",
                         inner):
        C, N = m.group(1), m.group(2).strip()
        ob = m.end() - 1
        cb = match_brace(inner, ob)
        if cb < 0 or _SIDEFX.search(N) or not unused(C, inner[ob + 1:cb], inner[cb + 1:]):
            continue
        out.append(("for", inner[:m.start()] + mk(C, N, False, inner[ob + 1:cb], "") + inner[cb + 1:]))
    for lab, ni in out:
        yield f"count_up_loop:{lab}", head + ni + tail


# ---- fold5 families (2026-10-07 AI matches o437-o447) -----------------------

def hi_missing_shape(rows) -> list[str]:
    """Symbols with more `lui %hi(S)` in the target than on our side: the
    target re-materializes the global's address before a later loop where ours
    reuses (GCSE) the first one (sefReleaseID)."""
    t: dict = {}
    c: dict = {}
    for a, _, b in diff_rows(rows):
        for x, bag in ((a, t), (b, c)):
            if x and _mn(x) == "lui":
                for s in _HI_RE.findall(x):
                    bag[s] = bag.get(s, 0) + 1
    return sorted(s for s in t if t[s] > c.get(s, 0))


_CALL_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\(")


def _pure_expr(e: str) -> bool:
    """No call (casts / sizeof allowed), no assignment, no ++/--."""
    if re.search(r"(?<![=!<>])=(?!=)|\+\+|--", e):
        return False
    return not any(m.group(1) not in ("sizeof", "__typeof__") for m in _CALL_RE.finditer(e))


def t_global_recast(fn, ctx=None):
    """sefReleaseID (o442): `e = (T *)G; for (..) e->f[i] ..` -> the uses in a
    later loop become direct casts `((T *)G)->f[i]`, so the loop re-loads the
    global's address (`lui %hi(G)`) like the target instead of reusing the
    first one (GCSE). Diff-gated: a symbol with more `lui %hi` in the target."""
    syms = (ctx or {}).get("global_recast")
    if not syms:
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    n = 0
    for i in range(first, len(lines)):
        m = re.match(r"^(\s*)([A-Za-z_]\w*)\s*=\s*([^;]+);\s*$", lines[i])
        if not m or m.group(2) not in locs:
            continue
        v, rhs = m.group(2), m.group(3).strip()
        if not any(re.search(r"(?<![\w.>])" + re.escape(s) + r"\b", rhs) for s in syms) or not _pure_expr(rhs):
            continue
        rep = rhs if re.fullmatch(r"\(.*\)", rhs) and match_brace(rhs, 0) == len(rhs) - 1 else f"({rhs})"
        vre = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b(?!\s*(?:[-+*/%&|^]|<<|>>)?=(?!=))")
        end = len(lines)
        for j in range(i + 1, len(lines)):
            if _assigns(lines[j], v):
                end = j
                break
        uses = [j for j in range(i + 1, end) if vre.search(lines[j])]
        if not uses:
            continue
        loops = []
        for j in range(i + 1, end):
            if _LOOP_HDR.match(lines[j]):
                lb = _loop_body(lines, dep, j)
                if lb and any(j <= u < lb[1] + (1 if lb[2] else 0) for u in uses):
                    loops.append((j, lb[1] + (1 if lb[2] else 0)))
        loops = [x for x in loops if not any(o[0] < x[0] < o[1] for o in loops)]   # outermost only
        spans = [(f"loop{k}", [x]) for k, x in enumerate(loops)]
        spans += [(f"from{k}", [(x[0], end)]) for k, x in enumerate(loops) if k]
        spans.append(("all", [(i + 1, end)]))
        for lab, rngs in spans:
            nl = list(lines)
            hit = False
            for a, e in rngs:
                for j in range(a, e):
                    if j in uses:
                        nl[j] = vre.sub(lambda _m: rep, nl[j])
                        hit = True
            if not hit:
                continue
            yield f"global_recast:{v}:{lab}", head + "\n".join(nl) + tail
            if not any(vre.search(x) for x in nl[i + 1:end]) and not any(
                    vre.search(x) for x in nl[end:]):
                dl = nl[:i] + nl[i + 1:]
                yield f"global_recast:{v}:{lab}:drop", head + "\n".join(dl) + tail
            n += 1
            if n > 12:
                return


_IF_RET1 = re.compile(r"^(?P<ind>\s*)if\s*\((?P<c>.*)\)\s*(?:return\b(?P<r>[^;]*);|\{\s*return\b(?P<r2>[^;]*);\s*\})\s*$")
_IF_OPEN = re.compile(r"^(?P<ind>\s*)if\s*\((?P<c>.*)\)\s*\{\s*$")


def _if_return(lines, i):
    """(cond, return expr, end line exclusive) of `if (C) return K;` (one line
    or `if (C) {` / `return K;` / `}`) at line i, else None."""
    ln = lines[i]
    m = _IF_RET1.match(ln)
    if m and ln.strip().startswith("if"):
        o = ln.index("(")
        c = match_brace(ln, o)
        if c < 0 or ln[o + 1:c] != m.group("c"):
            return None
        return m.group("c").strip(), (m.group("r") if m.group("r") is not None else m.group("r2")).strip(), i + 1
    m = _IF_OPEN.match(ln)
    if m and i + 2 < len(lines):
        o = ln.index("(")
        c = match_brace(ln, o)
        if c < 0 or ln[o + 1:c] != m.group("c"):
            return None
        r = re.match(r"^\s*return\b([^;]*);\s*$", lines[i + 1])
        if r and lines[i + 2].strip() == "}" and not (i + 3 < len(lines) and lines[i + 3].strip().startswith("else")):
            return m.group("c").strip(), r.group(1).strip(), i + 3
    return None


def t_merge_early_returns(fn):
    """sefCreateEffect2 (o443): `if (a) return K; s1; s2; if (b) return K;` ->
    `if (a || (s1, s2, b)) return K;` (two separate early returns gave an extra
    `move v0,zero`). Up to 4 plain expression statements in between."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    n = 0
    for i in range(len(lines)):
        a = _if_return(lines, i)
        if not a:
            continue
        ca, ka, j = a
        stmts = []
        while j < len(lines) and len(stmts) <= 4:
            if not lines[j].strip():
                j += 1
                continue
            if dep[j] != dep[i]:
                break
            b = _if_return(lines, j)
            if b:
                cb, kb, e = b
                if kb == ka:
                    ind = re.match(r"\s*", lines[i]).group(0)
                    cond = f"({ca}) || ({', '.join(stmts + [cb])})"
                    ret = f"return {ka};" if ka else "return;"
                    new = [f"{ind}if ({cond}) {{", f"{ind}    {ret}", f"{ind}}}"]
                    yield (f"merge_early_returns:L{i}+{len(stmts)}",
                           head + "\n".join(lines[:i] + new + lines[e:]) + tail)
                    n += 1
                break
            if not _simple_stmt(lines[j]) or re.match(r"^\s*(return|goto|break|continue)\b", lines[j]):
                break
            stmts.append(lines[j].strip()[:-1].strip())
            j += 1
        if n > 8:
            return


_BF_FIELD = re.compile(r"^(?P<ind>\s*)(?P<T>(?:unsigned\s+int|signed\s+int|unsigned\s+long|unsigned|int|long|"
                       r"u32|s32))\s+(?P<f>[A-Za-z_]\w*)\s*;(?P<rest>.*)$")
_STRUCT_DEF = re.compile(r"\b(?:typedef\s+)?struct\s*\w*\s*\{")


def mem_rows_shape(rows) -> list[str]:
    """Differing loads/stores through a non-sp base (reload/store order around
    stores): gate of bitfield32_alias."""
    out = []
    for a, m, b in diff_rows(rows):
        if m == " ":
            continue
        for x in (a, b):
            if x and _MEM_MN.match(_mn(x)) and "(sp)" not in x and not (m == "|" and _reloc_only(a, b)):
                out.append(x)
    return out[:8]


def t_bitfield32_alias(text, name, ctx=None):
    """RSRC_loadFileSub (o437, 0.35 -> 0.96): a full-width bitfield
    `u32 f : 32;` makes the field store alias everything (alias set 0), so a
    later global/field reload is not hoisted above it. Fields of struct types
    defined in the body that the function stores to: one at a time, then all.
    Diff-gated on differing non-sp loads/stores."""
    if not mem_rows_shape((ctx or {}).get("rows") or []):
        return
    loc = find_function(text, name)
    if not loc:
        return
    fn = text[loc[0]:loc[2] + 1]
    pre = text[:loc[0]]
    sites = []
    for m in _STRUCT_DEF.finditer(pre):
        o = pre.index("{", m.start())
        c = match_brace(pre, o)
        if c < 0:
            continue
        off = o + 1
        for ln in pre[o + 1:c].split("\n"):
            fm = _BF_FIELD.match(ln)
            f = re.escape(fm.group("f")) if fm else ""
            if fm and re.search(r"(?:->|\.)\s*" + f + r"\s*(?:[-+*/%&|^]|<<|>>)?=(?!=)", fn) \
                    and not re.search(r"&\s*\(?\s*[\w.\->\[\]]*(?:->|\.)\s*" + f + r"\b", text):   # no &bitfield
                sites.append((off, off + len(ln), fm))
            off += len(ln) + 1
    if not sites:
        return

    def apply(sel):
        t = text
        for st, en, fm in sorted(sel, key=lambda x: -x[0]):
            t = t[:st] + f"{fm.group('ind')}{fm.group('T')} {fm.group('f')} : 32;{fm.group('rest')}" + t[en:]
        return t
    for s in sites[:8]:
        yield f"bitfield32_alias:{s[2].group('f')}", apply([s])
    if len(sites) > 1:
        yield "bitfield32_alias:all", apply(sites)


def const_reg_shape(rows) -> list[int]:
    """`li rX,K` in the differing rows on both sides with other registers: a
    repeated constant kept in another s-reg (sefInitScheduler)."""
    t: dict = {}
    c: dict = {}
    for a, m, b in diff_rows(rows):
        if m == " ":
            continue
        for x, bag in ((a, t), (b, c)):
            if x and _mn(x) == "li":
                ops = [o.strip() for o in x.split(None, 1)[1].split(",")]
                try:
                    bag.setdefault(int(ops[1], 0), set()).add(ops[0])
                except (ValueError, IndexError):
                    pass
    return sorted(k for k in t if k in c and k != 0 and t[k] != c[k])


_LIT_STORE = re.compile(r"^(?P<ind>\s*)(?P<lhs>[^=;]+?)\s*=\s*(?P<k>-?(?:0[xX][0-9a-fA-F]+|\d+))[uUlL]*\s*;\s*$")


def t_const_local(fn, ctx=None):
    """sefInitScheduler (o438): a literal stored several times (`p[1] = -1;`
    x3) -> one local `int m = K;` used by every store; the declaration
    position (and the init: first statement / before the first store / in the
    declaration) decides the s-reg. Diff-gated on a `li rX,K` on both sides in
    other registers. Compare operands (menuBatObjFree o470, gated on
    `const_cmp`: target li+slt where ours has slti): `if (4 < ++g)` ->
    `_k = 4; if (_k < ++g)` and the equivalent operator / constant forms.
    Ungated `share` forms (dmgNumPut): a literal both compared and divided by
    -> one local (_const_share_forms)."""
    if (ctx or {}).get("const_cmp"):
        yield from _const_cmp_forms(fn, ctx["const_cmp"])
    yield from _const_share_forms(fn)
    ks = (ctx or {}).get("const_local")
    if not ks:
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    items, first, _ = _decl_block(inner)
    decl_ln = [i for i in range(first) if lines[i].strip()]
    ind = re.match(r"\s*", lines[decl_ln[0]]).group(0) if decl_ln else "    "
    by_k: dict = {}
    for i in range(first, len(lines)):
        m = _LIT_STORE.match(lines[i])
        if m and parse_decl(lines[i]) is None:
            v = int(m.group("k"), 0)
            by_k.setdefault(v, []).append(i)
    want = set()
    for k in ks:
        want |= {k, k & 0xffff, k & 0xff, k & 0xffffffff}
    nm = next(x for x in ("_k", "_k1", "_k2", "_kc") if not re.search(r"\b" + x + r"\b", fn))
    n = 0
    for v, sites in sorted(by_k.items(), key=lambda x: -len(x[1])):
        if len(sites) < 2 or not ({v, v & 0xffff, v & 0xffffffff} & want):
            continue
        lit = lines[sites[0]].split("=")[-1].strip().rstrip(";").strip()
        body = list(lines)
        for i in sites:
            body[i] = re.sub(r"=\s*" + re.escape(lit) + r"\s*;\s*$", f"= {nm};", body[i])
        poss = sorted(set(decl_ln + [decl_ln[-1] + 1 if decl_ln else first]))
        for p in poss:
            for form in ("first", "before", "init"):
                nl = list(body)
                if form == "init":
                    nl.insert(p, f"{ind}int {nm} = {lit};")
                else:
                    at = first if form == "first" else sites[0]
                    if form == "before" and at == first:
                        continue
                    sind = re.match(r"\s*", lines[at]).group(0) if at < len(lines) else ind
                    # statement first (indices after p shift by one once the decl is in)
                    nl.insert(at, f"{sind}{nm} = {lit};")
                    nl.insert(p, f"{ind}int {nm};")
                yield f"const_local:{lit}:d{poss.index(p)}:{form}", head + "\n".join(nl) + tail
                n += 1
        if n > 40:
            return


_SHARE_CMP = r"(?<![<>\-=!])(?:[<>]=?|[!=]=)(?![<>=])"
_SHARE_LIT = r"(?<![\w.])(?P<v>-?(?:0[xX][0-9a-fA-F]+|\d+))[uUlL]*(?![\w.])"


def _const_share_forms(fn):
    """dmgNumPut (ov01/menu): one literal used both as a loop bound / compare
    operand and as a divisor (`while (d <= 1000)` .. `num / 1000`) -> one
    local `int _k = K;` used at those sites (gcc keeps it in a register for
    both instead of li + the div-by-constant expansion). Ungated: only fires
    when the same value appears in both roles."""
    head, inner, tail = _split_fn(fn)
    items, first, lines = _decl_block(inner)
    lit = _SHARE_LIT
    cmp_v, div_v = {}, {}
    for i in range(first, len(lines)):
        ln = lines[i]
        if re.match(r"\s*case\b", ln):
            continue
        for m in re.finditer(_SHARE_CMP + r"\s*" + lit, ln):
            cmp_v.setdefault(int(m.group("v"), 0), []).append(i)
        for m in re.finditer(lit + r"\s*" + _SHARE_CMP, ln):
            cmp_v.setdefault(int(m.group("v"), 0), []).append(i)
        for m in re.finditer(r"[/%]\s*" + lit, ln):
            div_v.setdefault(int(m.group("v"), 0), []).append(i)
    ks = [k for k in cmp_v if k in div_v and abs(k) >= 2]
    if not ks:
        return
    nm = next(x for x in ("_k", "_k1", "_k2", "_kc") if not re.search(r"\b" + x + r"\b", fn))
    decl_ln = [i for i in range(first) if lines[i].strip()]
    ind = re.match(r"\s*", lines[decl_ln[0]]).group(0) if decl_ln else "    "
    sind = re.match(r"\s*", lines[first]).group(0) if first < len(lines) else ind
    for k in ks[:4]:
        def sub(ln, role):
            def rep(m):
                if int(m.group("v"), 0) != k:
                    return m.group(0)
                return m.group(0)[:m.start("v") - m.start()] + nm + m.group(0)[m.end("v") - m.start():]
            if role in ("cmp", "both"):
                ln = re.sub(_SHARE_CMP + r"\s*" + _SHARE_LIT, rep, ln)
                ln = re.sub(_SHARE_LIT + r"(?=\s*" + _SHARE_CMP + ")", rep, ln)
            if role in ("div", "both"):
                ln = re.sub(r"[/%]\s*" + _SHARE_LIT, rep, ln)
            return ln
        for role in ("both", "div", "cmp"):
            body = list(lines)
            for i in sorted(set(cmp_v[k] + div_v[k])):
                if not re.match(r"\s*case\b", body[i]):
                    body[i] = sub(body[i], role)
            if body == lines:
                continue
            at = decl_ln[-1] + 1 if decl_ln else first
            for form in ("init", "first"):
                nl = list(body)
                if form == "init":
                    nl.insert(at, f"{ind}int {nm} = {k};")
                else:
                    nl.insert(first, f"{sind}{nm} = {k};")
                    nl.insert(at, f"{ind}int {nm};")
                yield f"const_local:share:{k}:{role}:{form}", head + "\n".join(nl) + tail


_IDX_SITE =re.compile(r"(\(\(\s*(?:const\s+|volatile\s+)?[A-Za-z_][\w ]*?\s*\*+\s*\)\s*[A-Za-z_][\w.>\-]*\)|"
                       r"(?<![\w.>\]])[A-Za-z_]\w*)\s*\[")


def _wrap_index_sites(inner: str):
    """{text of `E[I]`: E} for E an identifier or `((T *)x)` cast."""
    out: dict = {}
    for m in _IDX_SITE.finditer(inner):
        E = m.group(1)
        if E in KEYWORDS or re.fullmatch(r"(?:unsigned|signed|char|short|int|long|float|double|void|u8|s8|u16|"
                                         r"s16|u32|s32|u64|s64|f32|f64)", E):
            continue
        o = m.end() - 1
        c = match_brace(inner, o)
        if c < 0:
            continue
        a = inner.rfind("\n", 0, m.start()) + 1
        b = inner.find("\n", m.start())
        if parse_decl(inner[a:b if b >= 0 else len(inner)]):
            continue
        out.setdefault(inner[m.start():c + 1], (E, inner[o + 1:c]))
    return out


def addu_wrap_forms(fn):
    """curMove2Pos (o444): addu base/index order fixed only by indexing an
    array that is a struct member at offset 0: `((T **)p)[i]` / `p[i]` ->
    `((struct { __typeof__((E)[0]) a[1]; } *)(E))->a[i]` (every occurrence of
    one site text)."""
    head, inner, tail = _split_fn(fn)
    n = 0
    for site, (E, I) in _wrap_index_sites(inner).items():
        if re.fullmatch(r"\s*-?(?:0[xX][0-9a-fA-F]+|\d+)\s*", I):
            continue                      # constant index: no addu to reorder
        new = f"((struct {{ __typeof__(({E})[0]) a[1]; }} *)({E}))->a[{I}]"
        yield f"addu_swap:wrap:{site[:20]}", head + inner.replace(site, new) + tail
        n += 1
        if n >= 12:
            return


def interblock_hint(unit: str, tu: str, name: str, text: str, cur_r: dict) -> dict | None:
    """Diagnostic only (svAddImage): recompile the candidate with
    -fno-sched-interblock. If that gets closer to the target, sched1 moved
    insns across blocks (interblock hoist) where the original source had a
    block boundary / different insns in that region."""
    import try_fn
    fd, p = tempfile.mkstemp(prefix=f"ib_{name}_", suffix=".c")
    os.write(fd, text.encode())
    os.close(fd)
    try:
        r = try_fn.try_function(unit, tu, name, Path(p), diff_limit=0, cand_cflags="-fno-sched-interblock")
    finally:
        os.unlink(p)
    if not r.get("ok"):
        return {"error": (r.get("error") or r.get("stage") or "")[-200:]}
    better = (r.get("score") or 0) > (cur_r.get("score") or 0)
    out = {"score_nointerblock": r.get("score"), "score": cur_r.get("score"),
           "diff_nointerblock": r.get("differing_words"), "better": better}
    if better:
        out["hint"] = ("interblock sched1 hoist: with -fno-sched-interblock the body scores "
                       f"{r.get('score')} vs {cur_r.get('score')} -- the target did not hoist across that "
                       "block boundary: add a block/insns in that region (split an if, move a statement "
                       "into the branch, an extra label/goto, a fresh block-local temp)")
    return out


def gcse_hint(unit: str, tu: str, name: str, text: str, score) -> dict | None:
    """try_fn.gcse_diag on body TEXT (fold12 diagnostic; gates gcse_split)."""
    import try_fn
    fd, p = tempfile.mkstemp(prefix=f"gc_{name}_", suffix=".c")
    os.write(fd, text.encode())
    os.close(fd)
    try:
        return try_fn.gcse_diag(unit, tu, name, Path(p), score)
    finally:
        os.unlink(p)


# ---- fold6 families (2026-10-07 AI matches o448-o460) -----------------------

_NUM = r"(?:0[xX][0-9a-fA-F]+|\d+)"
_STEP_RE = re.compile(r"^(?P<ind>\s*)(?:(?P<a>[A-Za-z_]\w*)\s*\+=\s*(?P<k>" + _NUM + r")|(?P<b>[A-Za-z_]\w*)\s*\+\+"
                      r"|\+\+\s*(?P<c>[A-Za-z_]\w*)|(?P<d>[A-Za-z_]\w*)\s*=\s*(?P=d)\s*\+\s*(?P<k2>" + _NUM + r"))"
                      r"\s*;\s*$")


def _step_of(ln: str):
    """(var, K) of a `v += K;` / `v++;` / `++v;` / `v = v + K;` statement line."""
    m = _STEP_RE.match(ln)
    if not m:
        return None
    v = m["a"] or m["b"] or m["c"] or m["d"]
    k = m["k"] or m["k2"] or "1"
    return v, int(k, 0)


def _hdr_step(hdr: str):
    """(init, cond, step) of a `for (...)` header line, else None."""
    m = re.match(r"^\s*for\s*\(", hdr)
    if not m:
        return None
    c = match_brace(hdr, m.end() - 1)
    if c < 0:
        return None
    parts = split_top(hdr[m.end():c], ";")
    return [p.strip() for p in parts] if len(parts) == 3 else None


def _init_before(lines, dep, i, v, stop=0):
    """(line, rhs) of the last plain `v = rhs;` before line i at depth <=
    dep[i] with no other write of v in between, else None."""
    for k in range(i - 1, stop - 1, -1):
        mm = re.match(r"^\s*" + re.escape(v) + r"\s*=\s*([^=;][^;]*);\s*$", lines[k])
        if mm and dep[k] <= dep[i]:
            return k, mm.group(1).strip()
        if _assigns(lines[k], v) or parse_decl(lines[k]) is not None and re.search(
                r"\b" + re.escape(v) + r"\s*=", lines[k]):
            return None
    return None


def _subst_var(ln: str, v: str, expr: str) -> str:
    """Replace the identifier v in ln by expr, parenthesized unless the
    neighbours make it unambiguous (`f(v)`, `v + 2`, `= v;`)."""
    pat = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
    out, last = [], 0
    for m in pat.finditer(ln):
        pre = ln[:m.start()].rstrip()
        post = ln[m.end():].lstrip()
        bare = pre[-1:] in ("(", ",", "=") and post[:1] in (")", ",", ";", "+", "-") \
            and not post.startswith(("++", "--", "->", "-="))
        out.append(ln[last:m.start()] + (expr if bare else f"({expr})"))
        last = m.end()
    return "".join(out) + ln[last:]


def t_derive_from_counter(fn):
    """cmdMemdump (o453), cmdUnittbl (o455): a separately stepped address
    `p = B; loop { use(p); p += K; c++; }` -> the address derived from the
    counter, `use(B + c * K)` (loop.c then builds the target's giv itself; the
    reverse of strength reduction / ptr_index). The counter is the `for` step
    variable (p stepped unconditionally at the loop body's top level) or a
    `c++` in the same block as `p += K`; its initial value must be a literal.
    Forms: B substituted (init dropped) or `p` kept as the fixed base; the
    offset of a use after the step as `c * K + K` or `(c + 1) * K`."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    n = 0
    for i in range(first, len(lines)):
        if not _LOOP_HDR.match(lines[i]):
            continue
        lb = _loop_body(lines, dep, i)
        if not lb or not lb[2]:
            continue
        b0, end = lb[0], lb[1]
        body = lines[b0:end]
        if any(re.search(r"\b(continue|goto)\b", x) for x in body):
            continue
        hs = _hdr_step(lines[i])
        hdr_ctr = None
        if hs:
            st = _step_of(hs[2] + ";") if hs[2] else None
            if st and st[1] == 1:
                hdr_ctr = st[0]
        steps = [(k,) + _step_of(lines[k]) for k in range(b0, end) if _step_of(lines[k])]
        for s, P, K in steps:
            if P not in locs or P == hdr_ctr or re.search(r"(?<![\w.>])" + re.escape(P) + r"\b", lines[i]):
                continue
            if any(_assigns(lines[k], P) for k in range(b0, end) if k != s):
                continue
            # counters stepping in lock-step with P
            ctrs = []
            if hdr_ctr and dep[s] == dep[b0]:
                ctrs.append((hdr_ctr, None))
            for k, C, KC in steps:
                if k == s or C == P or KC != 1 or dep[k] != dep[s]:
                    continue
                lo, hi = min(k, s), max(k, s)
                if any(dep[x] < dep[s] for x in range(lo, hi + 1)):
                    continue                              # not the same block
                if any(_assigns(lines[x], C) for x in range(b0, end) if x != k):
                    continue
                ctrs.append((C, k))
            if not ctrs:
                continue
            ini = _init_before(lines, dep, i, P, first)
            if not ini:
                continue
            pk, B = ini
            # P dead after the loop (until a plain reassignment)
            after_ok = True
            pre = re.compile(r"(?<![\w.>])" + re.escape(P) + r"\b")
            for x in lines[end + 1:]:
                if pre.search(x):
                    after_ok = bool(re.match(r"^\s*" + re.escape(P) + r"\s*=(?!=)", x)) and \
                        not pre.search(x.split("=", 1)[1])
                    break
            if not after_ok:
                continue
            uses = [k for k in range(b0, end) if k != s and pre.search(lines[k])]
            if not uses:
                continue
            for C, ck in ctrs:
                c0 = None
                if ck is None and hs and hs[0]:
                    mm = re.fullmatch(re.escape(C) + r"\s*=\s*(-?" + _NUM + r")", hs[0])
                    c0 = int(mm.group(1), 0) if mm else None
                if c0 is None:
                    ci = _init_before(lines, dep, i, C, first)
                    if ci and re.fullmatch(r"-?" + _NUM, ci[1]):
                        c0 = int(ci[1], 0)
                if c0 is None:
                    continue
                Bsafe = re.fullmatch(r"[A-Za-z_]\w*", B) and not any(_assigns(x, B) for x in body) \
                    and not any(_assigns(lines[x], B) for x in range(pk + 1, i))
                bases = ([("sub", B)] if Bsafe else []) + [("keep", P)]
                for bl, base in bases:
                    for form in ("add", "mul"):
                        nl = list(lines)
                        anyd = False
                        for u in uses:
                            d = (1 if u > s else 0) - (1 if ck is not None and u > ck else 0) - c0
                            anyd |= d != 0
                            ce = C if d == 0 else (f"({C} + {d})" if d > 0 else f"({C} - {-d})")
                            if K == 1:
                                off = C if d == 0 else (f"{C} + {d}" if d > 0 else f"{C} - {-d}")
                            elif form == "mul" or d == 0:
                                off = f"{ce} * {K}"
                            else:
                                off = f"{C} * {K} + {d * K}" if d > 0 else f"{C} * {K} - {-d * K}"
                            nl[u] = _subst_var(nl[u], P, f"{base} + {off}")
                        if form == "mul" and not anyd:
                            continue
                        nl[s] = None
                        if bl == "sub":
                            nl[pk] = None
                            for x in range(first):
                                if re.fullmatch(r"\s*[\w ]+?\s+\**\s*" + re.escape(P) + r"\s*;\s*", lines[x] or ""):
                                    nl[x] = None
                        out = [x for x in nl if x is not None]
                        yield f"derive_from_counter:{P}:{C}:{bl}:{form}", head + "\n".join(out) + tail
                        n += 1
                        if n >= 16:
                            return


def t_global_reread(fn):
    """cmdCamCoordGet (o454): `m = g; if (m >= 0) f(&v, m);` -> read the
    global at each use, `if (g >= 0) f(&v, g);` (the inverse of via_temp;
    field_reread does the same for `p->f` / `a[i]` sources). One use, every
    use, and every use with the copy dropped; uses stop at a write of the
    global or the local."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    names = set(locs) | {n for _, n in _scalar_locals(fn)}
    n = 0
    for i in range(first, len(lines)):
        m = re.match(r"^(\s*)([A-Za-z_]\w*)\s*=\s*(?:\(\s*[\w ]+\**\s*\)\s*)?([A-Za-z_]\w*)\s*;\s*$", lines[i])
        if not m or m.group(2) not in locs:
            continue
        v, G = m.group(2), m.group(3)
        if G in names or G in KEYWORDS or not re.search(r"[a-z]", G) or G == v:
            continue
        E = lines[i].split("=", 1)[1].strip().rstrip(";").strip()
        if any(lb and lb[0] <= i < lb[1] and any(_assigns(lines[x], G) for x in range(lb[0], lb[1]) if x != i)
               for lb in (_loop_body(lines, dep, h) for h in range(first, i) if _LOOP_HDR.match(lines[h]))):
            continue
        rep = G if E == G else f"({E})"
        vre = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
        uses = []
        for j in range(i + 1, len(lines)):
            if _LOOP_HDR.match(lines[j]):
                lb = _loop_body(lines, dep, j)
                if not lb or any(_assigns(lines[x], v) or _assigns(lines[x], G)
                                 for x in range(j, min(lb[1] + 1, len(lines)))):
                    break                  # written inside a later loop: uses there see the new value
            if _assigns(lines[j], v) or _assigns(lines[j], G):
                if not _assigns(lines[j], v):
                    uses += [(j, mm.start(), mm.end()) for mm in vre.finditer(lines[j])]
                break
            uses += [(j, mm.start(), mm.end()) for mm in vre.finditer(lines[j])]
        if not uses:
            continue

        def sub(sel):
            nl = list(lines)
            for j, a, b in sorted(sel, reverse=True):
                nl[j] = nl[j][:a] + rep + nl[j][b:]
            return nl
        for k, u in enumerate(uses[:6]):
            yield f"global_reread:use{k}:{v}", head + "\n".join(sub([u])) + tail
        if len(uses) > 1:
            yield f"global_reread:all:{v}", head + "\n".join(sub(uses)) + tail
        rest = [j for j in range(first, len(lines)) if j != i and vre.search(lines[j])]
        if all(sum(1 for u in uses if u[0] == j) == len(vre.findall(lines[j])) for j in rest):
            nl = sub(uses)
            del nl[i]
            yield f"global_reread:drop:{v}", head + "\n".join(nl) + tail
        n += 1
        if n > 8:
            return


def t_dup_tail_block(fn):
    """battleAfter (o450): the statement run after an if/else chain (k = 2..6
    plain statements, a trailing `return` included) copied to the end of every
    branch (an `else` added when there is none); when the run ends in a return
    and the chain ends in `else`, also the flattened form `if (c) { A; T }
    B; T` (the else body falls through). gcc cross-jumps the copies back
    together after scheduling, which moves the join label (bnel + lui in the
    delay slot). The reverse: an identical k-statement tail of every branch
    hoisted after the chain. dup_tail_store is the k = 1 store case."""
    head, inner, tail = _split_fn(fn)
    for lab, ni in _dup_tail_switch(inner):           # svAnalyzeChunk (o475): the switch form
        yield lab, head + ni + tail
    n = 0
    for ls, end, brs, has_else, ind in _if_chains(inner):
        L = inner.count("\n", 0, ls)
        after = inner[end + 1:]
        m0 = re.match(r"[ \t]*\n", after)
        if m0:
            run, pos = [], m0.end()
            while len(run) < 6:
                mm = re.match(r"([ \t]*)([^\n]*)\n", after[pos:])
                if not mm or mm.group(1) != ind:
                    break
                st = mm.group(2).strip()
                is_ret = bool(re.match(r"return\b[^;]*;$", st))
                if not (is_ret or _simple_stmt(mm.group(0).rstrip("\n"))) or not st.endswith(";"):
                    break
                run.append((st, pos, pos + mm.end()))
                pos += mm.end()
                if is_ret:
                    break
            for k in range(2, len(run) + 1):
                sts = [r[0] for r in run[:k]]
                blk = "".join(f"\n{ind}    {s}" for s in sts)
                out = inner[:end + 1]
                for o, c in reversed(brs):
                    out = out[:c].rstrip() + blk + f"\n{ind}" + out[c:]
                if not has_else:
                    out += f" else {{{blk}\n{ind}}}"
                rest = after[run[k - 1][2]:]
                yield f"dup_tail_block:L{L}:{k}", head + out + "\n" + rest + tail
                n += 1
                if sts[-1].startswith("return") and has_else:
                    o, c = brs[-1]
                    eb = inner.rfind("else", 0, o)
                    pre = inner[:eb]
                    for o2, c2 in reversed(brs[:-1]):
                        pre = pre[:c2].rstrip() + blk + f"\n{ind}" + pre[c2:]
                    els = inner[o + 1:c].rstrip()
                    flat = pre.rstrip() + "\n" + "\n".join(x[4:] if x.startswith(ind + "    ") else x
                                                         for x in (els + blk).split("\n")[1:])
                    yield f"dup_tail_block:L{L}:{k}:flat", head + flat + "\n" + rest + tail
        if has_else and len(brs) >= 2:
            bodies = [inner[o + 1:c].rstrip().split("\n") for o, c in brs]
            for k in range(2, 7):
                tails = [[x.strip() for x in b[-k:]] for b in bodies]
                if any(len(b) <= k for b in bodies) or len({tuple(t) for t in tails}) != 1:
                    break
                if not all(_simple_stmt(x) or re.match(r"return\b", x) for x in tails[0]):
                    break
                out = inner
                for o, c in reversed(brs):
                    b = inner[o + 1:c].rstrip().split("\n")
                    out = out[:o + 1] + "\n".join(b[:-k]) + f"\n{ind}" + out[c:]
                # the chain end moved: re-find it from the chain start
                e2 = out.find("{", ls)
                last = e2
                for _ in brs:
                    c2 = match_brace(out, last)
                    nxt = out.find("{", c2 + 1)
                    if _ != brs[-1]:
                        last = nxt
                endpos = c2
                hoist = "".join(f"\n{ind}{x}" for x in tails[0])
                yield f"dup_tail_block:L{L}:rev{k}", head + out[:endpos + 1] + hoist + out[endpos + 1:] + tail
                n += 1
        if n > 12:
            return


def _dead_from(lines, dep, i, v, first):
    """Is local v dead at line i (its next occurrence is a plain assignment
    not reading it, or none) and not used in a loop enclosing line i?"""
    pre = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
    for j in range(first, i):
        if _LOOP_HDR.match(lines[j]):
            lb = _loop_body(lines, dep, j)
            if lb and lb[0] <= i < lb[1] and any(pre.search(lines[x]) for x in range(j, lb[1] + 1)):
                return False
    for j in range(i, len(lines)):
        if pre.search(lines[j]):
            m = re.match(r"^\s*" + re.escape(v) + r"\s*=(?!=)(.*)$", lines[j])
            return bool(m and not pre.search(m.group(1)))
    return True


def t_merge_local(fn):
    """The complement of split_local. merge (spWepDispOn, o449): a local B
    whose uses all lie in a region where another local A of the same type is
    dead -> B renamed to A (one pseudo, so the two values share a register).
    reuse (battleExecNorm): `if (G != 0)` on a global / field -> `L = (T)G;
    if (L != 0)` through an existing local L that is dead there (a fresh temp
    stayed in v0; the reused local's pseudo got v1 like the target)."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    scal = {v: ty for v, ty in locs.items() if not re.match(r"(struct|union)\b", ty)}
    # declared pointer-ness per name (the type in locs drops the `*`)
    stars = {}
    for x in lines[:first]:
        p = parse_decl(x)
        if p:
            for d in p[2]:
                stars[decl_name(d)] = re.match(r"\s*(\**)", d).group(1)
    n = 0
    occ = {v: [j for j in range(first, len(lines))
               if re.search(r"(?<![\w.>])" + re.escape(v) + r"\b", lines[j])] for v in scal}
    for B, bty in scal.items():
        ob = occ[B]
        if not ob or not re.match(r"^\s*" + re.escape(B) + r"\s*=(?!=)", lines[ob[0]]) or \
                re.search(r"&\s*" + re.escape(B) + r"\b", inner):
            continue
        b1, b2 = ob[0], ob[-1]
        for A, aty in scal.items():
            if A == B or aty != bty or stars.get(A) != stars.get(B) or not occ[A]:
                continue
            inside = [j for j in occ[A] if b1 <= j <= b2]
            self_def = inside == [b2] and b2 != b1 and \
                re.match(r"^\s*" + re.escape(A) + r"\s*=(?!=)", lines[b2]) is not None
            if (inside and not self_def) or not _dead_from(lines, dep, b1, A, first):
                continue
            if re.search(r"&\s*" + re.escape(A) + r"\b", inner):
                continue
            pat = re.compile(r"(?<![\w.>])" + re.escape(B) + r"\b")
            nl = list(lines)
            for j in ob:
                nl[j] = pat.sub(A, nl[j])
            # drop B from its declaration
            for x in range(first):
                p = parse_decl(nl[x])
                if p and any(decl_name(d) == B for d in p[2]):
                    rest = [d for d in p[2] if decl_name(d) != B]
                    nl[x] = None if not rest else f"{p[0]}{p[1]} {', '.join(rest)};"
                    break
            yield f"merge_local:{'self:' if self_def else ''}{B}->{A}", head + "\n".join(x for x in nl if x is not None) + tail
            n += 1
            if n >= 12:
                return
    cre = re.compile(r"^(?P<ind>\s*)(?P<kw>if|else if)\s*\(\s*(?P<x>[A-Za-z_]\w*(?:(?:->|\.)\w+|\[[^\[\]]+\])*)"
                     r"\s*(?P<op>(?:==|!=|<=|>=|<|>)\s*[^()&|?]+?)?\s*\)(?P<rest>.*)$")
    for i in range(first, len(lines)):
        m = cre.match(lines[i])
        if not m or m["kw"] != "if":
            continue
        X = m["x"]
        base = re.match(r"\w+", X).group(0)
        if base in KEYWORDS or (X == base and (base in locs or not re.search(r"[a-z]", base))):
            continue
        if X == base and base in {nm for _, nm in _scalar_locals(fn)}:
            continue                               # a param
        op = m["op"] or "!= 0"
        k = 0
        for L, ty in scal.items():
            if L == base or not occ[L] or not _dead_from(lines, dep, i, L, first) or \
                    re.search(r"&\s*" + re.escape(L) + r"\b", inner):
                continue
            ind = m["ind"]
            T = ty + (" " + stars[L] if stars.get(L) else "")
            nl = list(lines)
            nl[i] = f"{ind}{L} = ({T}){X};\n{ind}if ({L} {op.strip()}){m['rest']}"
            yield f"merge_local:reuse:{L}:L{i}", head + "\n".join(nl) + tail
            n += 1
            k += 1
            if k >= 4 or n >= 24:
                break
        if n >= 24:
            return


_SEV_T = r"(?:unsigned\s+|signed\s+)?(?:int|short|char|long|float|double|u8|s8|u16|s16|u32|s32|f32|f64)"


def t_struct_elem_view(text, name, ctx=None):
    """menuOpenBat (o459): a flat global array written with constant indexes,
    some through casts (`D[0] = 0; ((float *)D)[8] = x;` on `extern int D[];`)
    -> one element type: an array of small structs `struct { T f0, f1; } D[]`
    with `D[k / g].f<k % g>` (g = 2, 4), or the flat array retyped to the cast
    type (g = 1). Mixed int / float access put the stores in two alias sets
    and sched2 interleaved them differently (0.9706 -> match with
    `{float a, b;}` and `D[i].a`). The declaration must be in the body text
    and the array used by no other function there; indexes must be literals;
    an access whose type changes must be a store of 0."""
    loc = find_function(text, name)
    if not loc:
        return
    fn = text[loc[0]:loc[2] + 1]
    pre = text[:loc[0]]
    n = 0
    for dm in re.finditer(r"^(?P<ind>[ \t]*)(?P<sc>(?:extern|static)\s+)?(?P<T>" + _SEV_T + r")\s+(?P<D>[A-Za-z_]\w*)"
                          r"\s*\[\s*(?P<N>\d*|0[xX][0-9a-fA-F]+)\s*\]\s*;[ \t]*$", pre, re.M):
        D, T = dm["D"], re.sub(r"\s+", " ", dm["T"])
        others = text[:dm.start()] + text[dm.end():loc[0]] + text[loc[2] + 1:]
        if re.search(r"(?<![\w.>])" + re.escape(D) + r"\b", others):
            continue
        acc = []        # (start, end, k, type or None, is_zero_store)
        for m in re.finditer(r"\(\s*\(\s*(?P<T2>" + _SEV_T + r")\s*\*\s*\)\s*" + re.escape(D) +
                             r"\s*\)\s*\[\s*(?P<k>\d+|0[xX][0-9a-fA-F]+)\s*\]", fn):
            z = bool(re.match(r"\s*=\s*(?:0|0\.0*[fF]?|\(\w+\)\s*0)\s*;", fn[m.end():]))
            acc.append((m.start(), m.end(), int(m["k"], 0), re.sub(r"\s+", " ", m["T2"]), z))
        for m in re.finditer(r"(?<![\w.>(])" + re.escape(D) + r"\s*\[", fn):
            o = m.end() - 1
            c = match_brace(fn, o)
            idx = fn[o + 1:c].strip() if c > 0 else ""
            if not re.fullmatch(r"\d+|0[xX][0-9a-fA-F]+", idx):
                acc = None
                break
            z = bool(re.match(r"\s*=\s*(?:0|0\.0*[fF]?|\(\w+\)\s*0)\s*;", fn[c + 1:]))
            acc.append((m.start(), c + 1, int(idx, 0), None, z))
        if not acc or len(acc) < 2:
            continue
        acc.sort()
        if any(a[1] > b[0] for a, b in zip(acc, acc[1:])):
            continue
        ets = [T] + sorted({a[3] for a in acc if a[3] and a[3] != T})
        N = dm["N"]
        for et in ets:
            if any((a[3] or T) != et and not a[4] for a in acc):
                continue
            for g in (1, 2, 4):
                if g == 1 and et == T and all(a[3] in (None, T) for a in acc):
                    continue
                if g == 1:
                    decl = f"{dm['ind']}{dm['sc'] or ''}{et} {D}[{N}];"
                else:
                    nn = (str(int(N, 0) // g) if N and int(N, 0) % g == 0 else "")
                    flds = " ".join(f"{et} f{j};" for j in range(g))
                    decl = f"{dm['ind']}{dm['sc'] or ''}struct {{ {flds} }} {D}[{nn}];"
                nf = fn
                for s, e, k, _, _ in reversed(acc):
                    nf = nf[:s] + (f"{D}[{k}]" if g == 1 else f"{D}[{k // g}].f{k % g}") + nf[e:]
                yield (f"struct_elem_view:{D}:{et.replace(' ', '_')}:g{g}",
                       text[:dm.start()] + decl + text[dm.end():loc[0]] + nf + text[loc[2] + 1:])
                n += 1
        if n >= 12:
            return


# ---- fold7 (2026-10-07, o461-o473): sink_past_call, mul_expand, inline_temp,
# loop_guard (ungated text), cvt_unfold, zero_store_type (diff-gated),
# const_local compare operands (diff-gated `const_cmp`) ------------------------

_CALL_IN = re.compile(r"(?<![\w.>])([A-Za-z_]\w*)\s*\(")


def _calls_in(s: str) -> list[tuple[int, int]]:
    """[(start, end)] of the outermost call expressions `f(...)` in S."""
    out: list = []
    for m in _CALL_IN.finditer(s):
        if m.group(1) in KEYWORDS or m.group(1) in ("__typeof__", "typeof", "__attribute__"):
            continue
        if out and m.start() < out[-1][1]:
            continue
        c = match_brace(s, m.end() - 1)
        if c > 0:
            out.append((m.start(), c + 1))
    return out


def _last_decl(lines: list[str], first: int) -> int:
    """Index after the last declaration line before FIRST (where a new local goes)."""
    idx = [i for i in range(first) if lines[i].strip() and parse_decl(re.sub(r"\s*__attribute__\s*\(\(.*\)\)", "", lines[i]))]
    return idx[-1] + 1 if idx else first


def _fresh(fn: str, base: str) -> str:
    for x in [base] + [f"{base}{k}" for k in range(1, 9)]:
        if not re.search(r"\b" + re.escape(x) + r"\b", fn):
            return x
    return base + "_x"


def _decl_ind(lines: list[str], first: int) -> str:
    for i in range(first):
        if lines[i].strip():
            return re.match(r"\s*", lines[i]).group(0)
    return "    "


_CONST_ASSIGN = re.compile(r"^(?P<ind>\s*)(?P<v>[A-Za-z_]\w*)\s*=\s*(?P<k>-?(?:0[xX][0-9a-fA-F]+|\d+)[uUlL]*|"
                           r"-?\d+\.\d*(?:[eE][-+]?\d+)?[fF]?)\s*;\s*$")


def _next_stmt(lines, i):
    j = i + 1
    while j < len(lines) and not lines[j].strip():
        j += 1
    return j


_ADDR_ASSIGN = re.compile(r"^(?P<ind>\s*)(?P<v>[A-Za-z_]\w*)\s*=\s*(?P<k>(?:\(\s*[\w\s]+\**\s*\)\s*)?"
                          r"\(?\s*&\s*[A-Za-z_]\w*\s*\)?)\s*;\s*$")


def t_sink_past_call(fn):
    """menuOpenItm / menuOpenEth (o464, o466): a constant default assignment
    right before a call, `dis = 0; if (f(x) == 0) { dis = 1; ... }` -> the call
    result saved first, `ok = f(x); dis = 0; if (ok == 0)`, so the constant is
    not live across the call (an extra s-reg + bigger frame otherwise). Also
    moves `v = K;` past the following plain statements that contain a call.
    Fold13 (o548 MenuGameDataPop 0.768 -> 0.808, late_global_ptr): the same
    for a global's address `gs = (T *)&G;`, also from a declaration initializer
    `T *gs = &G;` (split, then sunk past the first call statements)."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    locs, first = _all_locals(inner)
    # declaration initializers `T *v = &G;` -> `T *v;` + `v = &G;` as first statement
    for i in range(first):
        dm = re.match(r"^(?P<ind>\s*)(?P<t>[^=;,]*?[\s*])(?P<v>[A-Za-z_]\w*)\s*=\s*(?P<k>[^;,]+);\s*$", lines[i])
        if dm and dm["v"] in locs and _ADDR_ASSIGN.match(f"{dm['v']} = {dm['k'].strip()};"):
            nl = list(lines)
            nl[i] = f"{dm['ind']}{dm['t'].rstrip()}{' ' if not dm['t'].rstrip().endswith('*') else ''}{dm['v']};"
            nl.insert(first, f"{dm['ind']}{dm['v']} = {dm['k'].strip()};")
            yield from _sink_past_call_lines(fn, head, nl, tail, "decl:")
    yield from _sink_past_call_lines(fn, head, lines, tail, "")


def _all_locals(inner):
    """({name: type} of the leading declarations, first statement line); unlike
    _locals_block it steps over `extern` / `static` lines among them."""
    lines = inner.split("\n")
    first = _decl_end(inner)
    locs = {}
    for ln in lines[:first]:
        if re.match(r"\s*(extern|static|typedef)\b", ln):
            continue
        p = parse_decl(re.sub(r"\s*__attribute__\s*\(\(.*\)\)", "", ln))
        for d in (p[2] if p else []):
            if "[" not in d.split("=")[0]:
                locs[decl_name(d)] = p[1]
    return locs, first


def _sink_past_call_lines(fn, head, lines, tail, pre):
    inner = "\n".join(lines)
    dep = _depths(lines)
    locs, first = _all_locals(inner)
    n = 0
    for i in range(first, len(lines)):
        m = _CONST_ASSIGN.match(lines[i]) or _ADDR_ASSIGN.match(lines[i])
        if not m or m["v"] not in locs:
            continue
        v = m["v"]
        vre = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
        j = _next_stmt(lines, i)
        # (a) past 1-3 following plain statements with a call
        k, moved = j, 0
        while k < len(lines) and moved < 3 and dep[k] == dep[i] and _simple_stmt(lines[k]) \
                and not vre.search(lines[k]):
            moved += 1
            if _calls_in(lines[k]):
                nl = list(lines)
                del nl[i]
                nl.insert(k, lines[i])
                yield f"sink_past_call:{pre}{v}:past{moved}", head + "\n".join(nl) + tail
                n += 1
            k = _next_stmt(lines, k)
        # (b) the call in the next `if` condition into a local first
        if j < len(lines) and dep[j] == dep[i] and _CONST_ASSIGN.match(lines[i]):
            hm = re.match(r"^(\s*)if\s*\(", lines[j])
            if hm:
                try:
                    cond, ce = _paren_after(lines[j], hm.end() - 1)
                except ValueError:
                    continue
                if vre.search(cond):
                    continue
                cs = _calls_in(cond)
                if not cs:
                    continue
                a, b = cs[0]
                call = cond[a:b]
                r = _fresh(fn, "_sr")
                o = lines[j].index("(", hm.end() - 1)
                nl = list(lines)
                nl[j] = lines[j][:o + 1] + cond[:a] + r + cond[b:] + lines[j][ce:]
                nl.insert(i, f"{m['ind']}{r} = {call};")
                at = _last_decl(nl, first)
                nl.insert(at, f"{_decl_ind(lines, first)}__typeof__({call}) {r};")
                yield f"sink_past_call:{pre}{v}:ifcall", head + "\n".join(nl) + tail
                n += 1
        if n >= 10:
            return
_MUL = re.compile(r"(?P<e>(?<![\w.>\]])[A-Za-z_]\w*|\([^()]*\))\s*\*\s*(?P<k>0[xX][0-9a-fA-F]+|\d+)(?![\w.])")


def _mul_steps(t: str, e: str, k: int) -> str | None:
    """`t = e << a; t += e; t <<= b;` for k = (2^a +- 1) << b."""
    b = 0
    while k % 2 == 0:
        k //= 2
        b += 1
    for a in range(1, 16):
        if k in ((1 << a) + 1, (1 << a) - 1):
            op = "+=" if k == (1 << a) + 1 else "-="
            s = f"{t} = {e} << {a}; {t} {op} {e};"
            return s + (f" {t} <<= {b};" if b else "")
    return None


def t_mul_expand(fn):
    """menuCritObjDraw (o463): `f(.., (n - 1) * 24 + 0xB1, ..)` -> the product
    in a temp statement before the call / at the top of the block (the match
    was `t = m << 1; t += m; t <<= 3;`; `t = m * 24;` there gives the same
    code), plus the explicit shift/add steps for an existing `v = e * K;`."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    n = 0
    for i in range(first, len(lines)):
        ln = lines[i]
        if parse_decl(ln) or ASM_RE.search(ln):
            continue
        for mm in _MUL.finditer(ln):
            k = int(mm["k"], 0)
            if k < 3 or k & (k - 1) == 0:
                continue
            pre = ln[:mm.start()].rstrip()
            if pre and pre[-1] not in "(,=+-?:" and not pre.endswith("return"):
                continue
            e = mm["e"]
            ids = set(re.findall(r"\b[A-Za-z_]\w*\b", e)) - KEYWORDS
            if any(re.search(r"float|double|f32|f64", locs.get(x, "")) for x in ids):
                continue
            ind = re.match(r"\s*", ln).group(0)
            am = re.match(r"^\s*([A-Za-z_]\w*)\s*=\s*" + re.escape(mm.group(0)) + r"\s*;\s*$", ln)
            if am and am.group(1) in locs and am.group(1) not in ids:
                st = _mul_steps(am.group(1), e.strip("()") if re.fullmatch(r"\(\w+\)", e) else e, k)
                if st and re.fullmatch(r"\(?\w+\)?", e):
                    nl = list(lines)
                    nl[i] = ind + st
                    yield f"mul_expand:steps:{am.group(1)}", head + "\n".join(nl) + tail
                    n += 1
                continue
            if not _simple_stmt(ln) and not re.match(r"\s*return\b", ln):
                continue
            t = _fresh(fn, "_mul")
            # insertion points: right before, and the first statement of the block
            pts = [i]
            for p in range(i - 1, first - 1, -1):
                if not lines[p].strip():
                    continue
                if dep[p] != dep[i]:
                    break
                if any(_assigns(lines[p], x) for x in ids) or not _simple_stmt(lines[p]):
                    break
                pts.append(p)
            pts = sorted({pts[0], pts[-1]})
            new_ln = ln[:mm.start()] + t + ln[mm.end():]
            ee = e[1:-1] if re.fullmatch(r"\(\s*\w+\s*\)", e) else e
            for p in pts:
                for form in ("mul", "steps"):
                    if form == "mul":
                        st = f"{t} = {e} * {mm['k']};"
                    else:
                        if not re.fullmatch(r"\w+", ee):
                            continue
                        st = _mul_steps(t, ee, k)
                        if not st:
                            continue
                    nl = list(lines)
                    nl[i] = new_ln
                    nl.insert(p, re.match(r"\s*", lines[p]).group(0) + st)
                    nl.insert(_last_decl(nl, first), f"{_decl_ind(lines, first)}int {t};")
                    yield f"mul_expand:{form}:L{i}{'' if p == i else ':top'}", head + "\n".join(nl) + tail
                    n += 1
            if n >= 12:
                return


_PURE_RHS = re.compile(r"^[\w\s+\-*<>&|^()~]+$")


def t_inline_temp(fn):
    """menuCritObjDraw (o463): a temp `y4 = y + 4;` used several times ->
    `y + 4` written at each use, the temp dropped (the inverse of hoisting a
    common subexpression; global_reread / field_reread do plain copies of
    globals / fields). Operands must be locals/params not written before the
    last use."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    names = set(locs) | {nm for _, nm in _scalar_locals(fn)}
    n = 0
    for i in range(first, len(lines)):
        m = re.match(r"^(\s*)([A-Za-z_]\w*)\s*=\s*([^;=]+);\s*$", lines[i])
        if not m or m.group(2) not in locs or parse_decl(lines[i]):
            continue
        v, rhs = m.group(2), m.group(3).strip()
        if not _PURE_RHS.match(rhs) or not re.search(r"[-+*<>&|^~]", rhs) or "++" in rhs or "--" in rhs:
            continue
        if re.search(r"\b[A-Za-z_]\w*\s*\(", rhs):
            continue
        ids = set(re.findall(r"\b[A-Za-z_]\w*\b", rhs))
        if v in ids or not ids or not ids <= names:
            continue
        vre = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
        uses, ok, end = [], True, len(lines)
        for j in range(i + 1, len(lines)):
            if dep[j] < dep[i] or (dep[j] == dep[i] and lines[j].lstrip().startswith("}")):
                end = j
                break
            if _LOOP_HDR.match(lines[j]):
                lb = _loop_body(lines, dep, j)
                if not lb or any(any(_assigns(lines[x], y) for y in ids | {v}) for x in range(j, min(lb[1] + 1, len(lines)))):
                    end = j
                    break
            if _assigns(lines[j], v):
                end = j
                break
            hit = [(j, q.start(), q.end()) for q in vre.finditer(lines[j])]
            uses += hit
            if any(_assigns(lines[j], y) for y in ids):
                end = j + 1
                break
        if not uses:
            continue
        if any(vre.search(lines[x]) and not _assigns(lines[x], v) for x in range(end, len(lines))):
            # a later read of v beyond the region (another branch / loop): keep away
            nxt = next((x for x in range(end, len(lines)) if vre.search(lines[x])), None)
            if nxt is None or not re.match(r"^\s*" + re.escape(v) + r"\s*=(?!=)", lines[nxt]) or \
                    vre.search(lines[nxt].split("=", 1)[1]):
                continue
        nl = list(lines)
        for j, a, b in sorted(uses, reverse=True):
            s = nl[j]
            bare = re.search(r"[(,=]\s*$", s[:a]) and re.match(r"\s*[,);]", s[b:])
            nl[j] = s[:a] + (rhs if bare else f"({rhs})") + s[b:]
        del nl[i]
        if not any(vre.search(x) for x in nl[first - 0:] if not parse_decl(x)):
            for x in range(first):
                p = parse_decl(nl[x])
                if p and any(decl_name(d) == v for d in p[2]):
                    rest = [d for d in p[2] if decl_name(d) != v]
                    nl[x] = None if not rest else f"{p[0]}{p[1]} {', '.join(rest)};"
                    break
        yield f"inline_temp:{v}", head + "\n".join(x for x in nl if x is not None) + tail
        n += 1
        if n >= 8:
            return


def _reindent(block: list[str], add: str) -> list[str]:
    return [add + x if x.strip() else x for x in block]


def t_loop_guard(fn):
    """Loop guard forms (o473 JNT_setModelMatrix, o461 menuFontPut).
    while -> `if (C) { do {B} while (C); next; }`: the guarded do-while, with
    the statement after the loop sunk into the guard (JNT: the reload of `hdr`
    after the first loop) or not; each loop alone and all loops at once (one
    at a time scored lower than the pre-fix body, both together matched).
    The reverse: `if (C) do {B; p = E;} while (*p);` -> `while (*E) { p = E; B }`
    (menuFontPut: the pointer recomputed at the top of a plain while), plain
    `while (C2) {B}`, with/without the `p = E0;` before the guard.
    (t_loops' while->dowhile is retired and never sank the next statement.)"""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    hdr = re.compile(r"^(?P<ind>\s*)while\s*\((?P<c>.*)\)\s*\{\s*$")

    def whiles(ls):
        dp = _depths(ls)
        out = []
        for i, ln in enumerate(ls):
            m = hdr.match(ln)
            if not m:
                continue
            try:
                c, ce = _paren_after(ln, ln.index("while"))
            except ValueError:
                continue
            if ln[ce + 1:].strip() != "{":
                continue
            lb = _loop_body(ls, dp, i)
            if lb and lb[2]:
                out.append((i, lb[1], c, m["ind"]))
        return out, dp

    def conv(ls, w, absorb):
        i, e, c, ind = w
        body = ls[i + 1:e]
        k = _next_stmt(ls, e)
        take = absorb and k < len(ls) and _simple_stmt(ls[k]) and re.match(r"\s*", ls[k]).group(0) == ind
        if absorb and not take:
            return None
        new = [f"{ind}if ({c}) {{", f"{ind}    do {{"] + _reindent(body, "    ") + [f"{ind}    }} while ({c});"]
        if take:
            return ls[:i] + new + ["    " + ls[k], f"{ind}}}"] + ls[e + 1:k] + ls[k + 1:]
        return ls[:i] + new + [f"{ind}}}"] + ls[e + 1:]

    ws, _ = whiles(lines)
    n = 0
    for w in ws[:6]:
        for ab in (False, True):
            nl = conv(lines, w, ab)
            if nl:
                yield f"loop_guard:ifdo{'+next' if ab else ''}:L{w[0]}", head + "\n".join(nl) + tail
                n += 1
    if len(ws) > 1:
        for ab in (False, True):
            cur = list(lines)
            idx = len(ws)
            okc = False
            for q in range(idx - 1, -1, -1):
                cw, _ = whiles(cur)
                if q >= len(cw):
                    continue
                nl = conv(cur, cw[q], ab) or (conv(cur, cw[q], False) if ab else None)
                if nl:
                    cur, okc = nl, True
            if okc:
                yield f"loop_guard:all{'+next' if ab else ''}", head + "\n".join(cur) + tail
    # reverse: guarded do-while -> while
    dep = _depths(lines)
    for i, ln in enumerate(lines):
        m = re.match(r"^(?P<ind>\s*)if\s*\(", ln)
        if not m:
            continue
        try:
            C, ce = _paren_after(ln, m.end() - 1)
        except ValueError:
            continue
        rest = ln[ce + 1:].strip()
        if rest == "do {":
            ds, braced = i, False
        elif rest == "{" and i + 1 < len(lines) and lines[i + 1].strip() == "do {":
            ds, braced = i + 1, True
        else:
            continue
        de = next((j for j in range(ds + 1, len(lines)) if dep[j] == dep[ds] + 1
                   and re.match(r"^\s*\}\s*while\s*\(", lines[j])), None)
        if de is None:
            continue
        try:
            C2, c2e = _paren_after(lines[de], lines[de].index("while"))
        except ValueError:
            continue
        if lines[de][c2e + 1:].strip() != ";":
            continue
        endl = de
        if braced:
            endl = _next_stmt(lines, de)
            if endl >= len(lines) or lines[endl].strip() != "}":
                continue
        body = lines[ds + 1:de]
        ind = m["ind"]
        # body indentation relative to the new while
        bi = min((len(re.match(r"\s*", x).group(0)) for x in body if x.strip()), default=len(ind) + 4)
        body = [x[bi - len(ind) - 4:] if x.strip() and bi - len(ind) - 4 > 0 else x for x in body]
        pre, post = lines[:i], lines[endl + 1:]
        yield f"loop_guard:while:L{i}", head + "\n".join(pre + [f"{ind}while ({C2}) {{"] + body + [f"{ind}}}"] + post) + tail
        n += 1
        lastj = max((j for j, x in enumerate(body) if x.strip()), default=None)
        lm = re.match(r"^\s*([A-Za-z_]\w*)\s*=\s*([^;=]+);\s*$", body[lastj]) if lastj is not None else None
        if lm and re.search(r"(?<![\w.>])" + re.escape(lm.group(1)) + r"\b", C2):
            v, E = lm.group(1), lm.group(2).strip()
            c2 = re.sub(r"(?<![\w.>])" + re.escape(v) + r"\b", f"({E})", C2)
            c2 = re.sub(r"\*\s*\(\s*&\s*([^()]+?)\s*\)", r"\1", c2)
            c2 = re.sub(r"\(\s*\(([^()]*)\)\s*\)", r"(\1)", c2)
            sind = re.match(r"\s*", body[lastj]).group(0)
            nb = [f"{sind}{v} = {E};"] + body[:lastj] + body[lastj + 1:]
            p = i - 1
            while p >= 0 and not lines[p].strip():
                p -= 1
            drops = [False]
            if p >= 0 and re.match(r"^\s*" + re.escape(v) + r"\s*=(?!=)", lines[p]):
                drops.append(True)
            for dr in drops:
                pr = pre[:p] + pre[p + 1:] if dr else pre
                yield (f"loop_guard:while-rot{'-drop' if dr else ''}:L{i}",
                       head + "\n".join(pr + [f"{ind}while ({c2}) {{"] + nb + [f"{ind}}}"] + post) + tail)
                n += 1
        if n >= 16:
            return


def _col_const_cvts(col: list[str]) -> list[int]:
    """Constants converted at run time: `mtc1 rX,$fN` + `cvt.s.w $fN,$fN` with
    rX set by li / lui(+ori/addiu) earlier in COL."""
    out = []
    for i, x in enumerate(col):
        mn, ops, _ = _insn_ops(x)
        if mn != "mtc1" or len(ops) != 2:
            continue
        if not any(_insn_ops(y)[0] == "cvt.s.w" and _insn_ops(y)[1][:2] == [ops[1], ops[1]] for y in col[i + 1:i + 4]):
            continue
        r = ops[0]
        for j in range(i - 1, max(-1, i - 24), -1):
            m2, o2, rel = _insn_ops(col[j])
            if not o2 or o2[0] != r or m2.startswith("s") or m2.startswith("b") or m2 == "jal":
                continue
            if m2 == "li" and len(o2) == 2 and _ival(o2[1]) is not None:
                out.append(_ival(o2[1]) & 0xFFFFFFFF)
            elif m2 in ("lui", "ori", "addiu") and not rel:
                v = _full_const(col, j)
                if v is not None:
                    out.append(v)
            break
    return out


def cvt_const_shape(rows) -> list[int]:
    """Target converts a constant at run time (cvt.s.w of an li/lui value)
    where ours does not (a folded float literal; menuSelLine o465)."""
    dr = diff_rows(rows)
    if not any(m != " " for _, m, _ in dr):
        return []
    t = _col_const_cvts([a for a, _, _ in dr if a])
    c = _col_const_cvts([b for _, _, b in dr if b])
    return sorted({k for k in t if t.count(k) > c.count(k)})


def _f32(x: float) -> float:
    import struct
    return struct.unpack("<f", struct.pack("<f", x))[0]


def t_cvt_unfold(fn, ctx=None):
    """menuSelLine (o465): target `li/lui+ori; mtc1; cvt.s.w` of a constant
    where ours loads a folded float literal: `float z = 16777212.0f;` ->
    `int zi = 0xFFFFFC;` at the top and `z = zi;` at a later statement
    boundary (after the next if/else: the branch between def and conversion
    stops const folding). Diff-gated on the constant cvt."""
    ks = (ctx or {}).get("cvt_unfold")
    if not ks:
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    locs, first = _locals_block(inner)
    fl = {v for v, ty in locs.items() if re.search(r"\b(float|f32)\b", ty)}
    want = {}
    for k in ks:
        sk = k - (1 << 32) if k & 0x80000000 else k
        want[_f32(float(sk))] = sk
    n = 0
    for i in range(len(lines)):
        ln = lines[i]
        m = re.match(r"^(?P<ind>\s*)(?P<decl>(?:float|f32)\s+)?(?P<v>[A-Za-z_]\w*)\s*=\s*(?:\(\s*(?:float|f32)\s*\)\s*)?"
                     r"(?P<lit>-?(?:\d+\.?\d*(?:[eE][-+]?\d+)?|0[xX][0-9a-fA-F]+))[fFuUlL]*\s*;\s*$", ln)
        if not m or m["v"] not in fl:
            continue
        try:
            val = _f32(float(int(m["lit"], 0)) if re.match(r"-?0[xX]", m["lit"]) else float(m["lit"]))
        except ValueError:
            continue
        if val not in want:
            continue
        K = want[val]
        v = m["v"]
        vre = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
        zi = _fresh(fn, v + "i")
        kt = hex(K) if K > 9 else str(K)
        base = list(lines)
        if m["decl"]:
            base[i] = f"{m['ind']}{m['decl'].strip()} {v};"
        else:
            base[i] = None
        # statement boundaries at the def's depth up to v's first use
        d0 = dep[i]
        pts, prev = [], ";"
        for j in range(first if m["decl"] else i + 1, len(lines)):
            s = lines[j].strip()
            if not s or j == i:
                continue
            if dep[j] < d0:
                break
            at_stmt = dep[j] == d0 and not s.startswith(("}", "else", "{")) and prev.endswith((";", "}", "{"))
            if vre.search(lines[j]):
                if at_stmt:
                    pts.append(j)
                break
            if at_stmt:
                pts.append(j)
            prev = s
        for form in ("init", "assign"):
            for p in pts[:8]:
                nl = list(base)
                ind = re.match(r"\s*", lines[p]).group(0)
                nl[p] = f"{ind}{v} = {zi};\n{lines[p]}" if nl[p] is not None else f"{ind}{v} = {zi};"
                if form == "assign":
                    if m["decl"]:            # no statements among the declarations (gcc 2.96)
                        q = pts[0]
                        qi = re.match(r"\s*", lines[q]).group(0)
                        nl[q] = f"{qi}{zi} = {kt};\n{nl[q]}"
                    else:
                        nl[i] = f"{m['ind']}{zi} = {kt};"
                    dl = f"{_decl_ind(lines, first)}int {zi};"
                else:
                    dl = f"{_decl_ind(lines, first)}int {zi} = {kt};"
                nl.insert(_last_decl(lines, first), dl)
                yield f"cvt_unfold:{v}:{form}:p{pts.index(p)}", head + "\n".join(x for x in nl if x is not None) + tail
                n += 1
        if n >= 20:
            return


_ZST = re.compile(r"^s[bhw]\s+zero,(-?\w+)\((\w+)\)")


def zero_store_shape(rows) -> list[str]:
    """A differing row within 3 rows of a non-sp `sw/sh/sb zero` store
    (menuStockDead o462: the store's alias set moves the scheduling)."""
    dr = diff_rows(rows)
    out = set()
    for i, (a, m, b) in enumerate(dr):
        if m == " " or (m == "|" and _reloc_only(a, b)):
            continue
        for a2, _, b2 in dr[max(0, i - 3):i + 4]:
            for x in (a2, b2):
                zm = _ZST.match(x or "")
                if zm and zm.group(2) != "sp":
                    out.add(f"{zm.group(1)}({zm.group(2)})")
    return sorted(out)


def t_zero_store_type(fn, ctx=None):
    """menuStockDead (o462): an int-typed store of 0 to what is a float
    (`*(int *)&pos->z = 0;`) -> a float store `pos->z = 0.0f;` /
    `*(float *)E = 0.0f;` (other alias set -> other scheduling), and the
    reverse. Diff-gated on a differing row next to a `sw zero` store."""
    if not (ctx or {}).get("zero_store_type"):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    _, first = _locals_block(inner)
    n = 0
    zre = re.compile(r"^(?P<ind>\s*)(?P<lhs>[^=;]+?)\s*=\s*(?P<z>0|0x0|0\.0[fF]?|0\.[fF]?)\s*;\s*$")
    for i in range(first, len(lines)):
        m = zre.match(lines[i])
        if not m or parse_decl(lines[i]):
            continue
        lhs, ind, isf = m["lhs"].strip(), m["ind"], "." in m["z"]
        outs = []
        cm = re.match(r"^\*\s*\(\s*(?P<T>(?:unsigned\s+|signed\s+)?(?:int|long|u32|s32|unsigned|float|f32))\s*\*\s*\)\s*(?P<e>.+)$", lhs)
        if cm:
            E = cm["e"].strip()
            if re.search(r"float|f32", cm["T"]):
                outs.append(("int", f"*(int *){E} = 0;"))
            else:
                outs.append(("float", f"*(float *){E} = 0.0f;"))
                em = re.match(r"^&\s*(.+)$", E)
                if em:
                    X = em.group(1).strip()
                    if X.startswith("(") and match_brace(X, 0) == len(X) - 1:
                        X = X[1:-1]
                    outs.append(("plain", f"{X} = 0.0f;"))
                    outs.append(("plain0", f"{X} = 0;"))
        elif re.search(r"->|\.|\[", lhs) and not re.match(r"^\*", lhs):
            if isf:
                outs.append(("int", f"*(int *)&{lhs} = 0;"))
            else:
                outs.append(("float", f"*(float *)&{lhs} = 0.0f;"))
        for lab, s in outs:
            nl = list(lines)
            nl[i] = ind + s
            yield f"zero_store_type:{lab}:L{i}", head + "\n".join(nl) + tail
            n += 1
        if n >= 12:
            return


def const_cmp_shape(rows) -> list[int]:
    """Target `li rX,K` + `slt(u)` reading rX where ours compares with an
    immediate (slti/sltiu): the compare constant was held in a local
    (menuBatObjFree o470: `t = 4; if (t < ++g)`)."""
    dr = diff_rows(rows)
    if not any(m != " " and _mn(b or "") in ("slti", "sltiu") for _, m, b in dr):
        return []
    ks = set()
    for i, (a, m, _) in enumerate(dr):
        if m == " " or _mn(a or "") != "li":
            continue
        _, ops, _ = _insn_ops(a)
        if len(ops) != 2 or _ival(ops[1]) is None:
            continue
        for a2, _, _ in dr[i + 1:i + 8]:
            m2, o2, _ = _insn_ops(a2 or "")
            if m2 in ("slt", "sltu") and ops[0] in o2[1:]:
                ks.add(_ival(ops[1]))
                break
            if o2 and o2[0] == ops[0]:
                break
    return sorted(ks)


_MIRROR = {"<": ">", ">": "<", "<=": ">=", ">=": "<="}


def _const_cmp_forms(fn, ks):
    """`if (4 < ++g)` / `if (++g > 4)` / `>= 5` -> `_k = 4; if (_k < ++g)` and
    the other equivalent operand orders; only constants in KS."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    _, first = _locals_block(inner)
    want = set()
    for k in ks:
        want |= {k, k - (1 << 32) if k & 0x80000000 else k}
    nm = _fresh(fn, "_k")
    lit = r"-?(?:0[xX][0-9a-fA-F]+|\d+)[uUlL]*"
    n = 0
    for i in range(first, len(lines)):
        hm = re.match(r"^(\s*)(?:else\s+)?if\s*\(", lines[i])
        if not hm:
            continue
        try:
            cond, ce = _paren_after(lines[i], hm.end() - 1)
        except ValueError:
            continue
        rels = [r for r in _REL.finditer(cond) if r.group(1) in _MIRROR]
        if len(rels) != 1 or re.search(r"&&|\|\||\?", cond):
            continue
        r = rels[0]
        a, op, b = cond[:r.start()].strip(), r.group(1), cond[r.end():].strip()
        if re.fullmatch(lit, a):
            X, op, L = b, _MIRROR[op], int(re.sub(r"[uUlL]+$", "", a), 0)
        elif re.fullmatch(lit, b):
            X, L = a, int(re.sub(r"[uUlL]+$", "", b), 0)
        else:
            continue
        if op == ">":
            cls, c = "gt", L
        elif op == ">=":
            cls, c = "gt", L - 1
        elif op == "<":
            cls, c = "lt", L
        else:
            cls, c = "lt", L + 1
        if cls == "gt":
            forms = [(c, f"{nm} < {X}"), (c, f"{X} > {nm}"), (c + 1, f"{X} >= {nm}"), (c + 1, f"{nm} <= {X}")]
        else:
            forms = [(c, f"{X} < {nm}"), (c, f"{nm} > {X}"), (c - 1, f"{X} <= {nm}"), (c - 1, f"{nm} >= {X}")]
        o = lines[i].index("(", hm.end() - 1)
        ind = hm.group(1)
        if re.match(r"\s*else\b", lines[i]):
            continue                       # the `_k = K;` statement needs a plain if
        for K, cnd in forms:
            if K not in want:
                continue
            nl = list(lines)
            nl[i] = f"{ind}{nm} = {K};\n" + lines[i][:o + 1] + cnd + lines[i][ce:]
            nl.insert(_last_decl(lines, first), f"{_decl_ind(lines, first)}int {nm};")
            yield f"const_local:cmp:{K}:{cnd.replace(' ', '')}", head + "\n".join(nl) + tail
            n += 1
        if n >= 12:
            return


# ---- fold8 (2026-10-07, o474-o482): each reproduced its hand match on a rebuilt pre-fix body
# return_to_goto_out  `return [E];` inside a loop -> `goto out;` with `out:` right
#                     before the function's final `return E;` (or at its end for a
#                     plain `return;`): gcc keeps the loop's bottom test + entry
#                     jump instead of duplicating the exit test (svAnalyzeChunk)
# dup_tail_block:sw   (in t_dup_tail_block) statements after a switch copied into
#                     every case that leaves it (svAnalyzeChunk)
# dup_head_stmt       statements right before an if/else chain copied to the start
#                     of every branch, and the reverse (ACT_updateMotionCore)
# ternary_to_ifchain  `x = a ? b : (c ? d : e);` -> if / else-if chain and back;
#                     `do { } while (0);` wrapper dropped / a bare block wrapped
#                     (FileSaveDataPush)
# rel_mirror          `a <= b` -> `b >= a` (diff-gated: a moved load or a
#                     differing slt / branch row; actStockPick)
# float_vararg        explicit `fptodp(x)` / `(double)x` / m2c `(s32) fptodp(x)`
#                     temps in a call with a string-literal format -> `x` passed
#                     directly (InitMapKoware)
# callee_void         (in t_void_ret, own family: not SEMANTIC) a body-level `int f(..);` prototype whose calls
#                     never use the value -> void (and back), gated on a v0/v1 swap
#                     after a call (specSyoukanProc)

_LOOP_KW = re.compile(r"^\s*(?:\}\s*)?(for|while|do|switch)\b")


def _label_name(fn: str, base: str = "out") -> str:
    for x in [base, "_" + base] + [f"{base}{k}" for k in range(1, 9)]:
        if not re.search(r"\b" + re.escape(x) + r"\b", fn):
            return x
    return base + "_x"


def t_return_to_goto_out(fn):
    """svAnalyzeChunk (o475): `default: return;` inside the while loop ->
    `goto out;` with the label after the loop (here: before the function's
    final return / at its end, which is the same point when the loop is last).
    The jump is a plain goto, so gcc's loop copy-header keeps the exit test at
    the bottom (entry jump + one test) instead of duplicating it at the top.
    All returns of one value in one loop, then in every loop."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    nz = [k for k, ln in enumerate(lines) if ln.strip()]
    if not nz:
        return
    last = nz[-1]
    mlast = re.match(r"^(\s*)return(?:\s+([^;]+?))?\s*;\s*$", lines[last])
    if mlast and dep[last] != 0:
        mlast = None
    lab = _label_name(fn)
    loops = []
    for i, ln in enumerate(lines):
        if re.match(r"^\s*(for|while)\s*\(.*\)\s*\{\s*$", ln) or re.match(r"^\s*do\s*\{\s*$", ln):
            lb = _loop_body(lines, dep, i)
            if lb and lb[2]:
                loops.append((i, lb[0], lb[1]))
    rre = re.compile(r"\breturn(?:\s+([^;]+?))?\s*;")
    seen = set()
    per_val: dict = {}
    for i, b0, b1 in loops:
        hits: dict = {}
        for j in range(b0, b1):
            if mlast and j == last:
                continue
            for m in rre.finditer(lines[j]):
                hits.setdefault((m.group(1) or "").strip(), []).append(j)
        for v, js in hits.items():
            per_val.setdefault(v, set()).update(js)
            yield from _goto_out_emit(head, lines, tail, js, v, mlast, last, lab, f"L{i}", seen)
    for v, js in per_val.items():
        yield from _goto_out_emit(head, lines, tail, sorted(js), v, mlast, last, lab, "all", seen)


def _goto_out_emit(head, lines, tail, js, v, mlast, last, lab, tag, seen):
    if v:
        if not mlast or (mlast.group(2) or "").strip() != v:
            return
    nl = list(lines)
    rre = re.compile(r"\breturn(?:\s+([^;]+?))?\s*;")
    for j in js:
        nl[j] = rre.sub(lambda m: f"goto {lab};" if (m.group(1) or "").strip() == v else m.group(0), nl[j])
    if mlast and (mlast.group(2) or "").strip() == v:
        ind = mlast.group(1)
        nl[last] = f"{lab}:\n{nl[last]}"
    elif not v:
        ind = "    "
        k = len(nl) - 1
        while k > 0 and not nl[k].strip():
            k -= 1
        nl.insert(k + 1, f"{lab}:\n{ind};")
    else:
        return
    key = tuple(js)
    if key in seen:
        return
    seen.add(key)
    yield f"return_to_goto_out:{tag}:{v[:12] or 'void'}", head + "\n".join(nl) + tail


def _switch_exits(lines, i, close):
    """Lines in (i, close) holding a `break;` that leaves the switch at line i
    (not inside a nested loop / switch), or None when one is not alone on its
    line or a loop body is unbraced."""
    out = []
    stack = []
    for j in range(i + 1, close):
        s = re.sub(r'"(\\.|[^"\\])*"|\'(\\.|[^\'\\])*\'', "", lines[j])
        for _ in range(s.count("}")):
            if stack:
                stack.pop()
        if re.match(r"^\s*(?:\}\s*)?(for|while|switch)\s*\(", s) and not s.rstrip().endswith(("{", ";")):
            return None
        opens = s.count("{")
        loopish = bool(_LOOP_KW.match(s)) and not re.match(r"^\s*\}\s*while\b", s)
        for _ in range(opens):
            stack.append(loopish)
        if re.search(r"\bbreak\s*;", s) and not any(stack):
            if s.strip() != "break;":
                return None
            out.append(j)
    return out


def _dup_tail_switch(inner):
    """(label, new inner) pairs: statements after a switch copied before each
    `break;` leaving it (and appended to a last case that falls out; a
    `default:` added when there is none), the run removed after the switch.
    Reverse: an identical statement before every switch-leaving break of a
    switch with a default and no fall-out -> hoisted after the switch."""
    lines = inner.split("\n")
    dep = _depths(lines)
    for i, ln in enumerate(lines):
        m = re.match(r"^(\s*)switch\s*\(.*\)\s*\{\s*$", ln)
        if not m:
            continue
        ind = m.group(1)
        d = dep[i]
        close = next((j for j in range(i + 1, len(lines)) if dep[j] == d + 1 and lines[j].strip().startswith("}")), None)
        if close is None or lines[close].strip() != "}":
            continue
        exits = _switch_exits(lines, i, close)
        if exits is None:
            continue
        has_def = any(dep[j] == d + 1 and re.match(r"^\s*default\s*:", lines[j]) for j in range(i + 1, close))
        lastst = next((j for j in range(close - 1, i, -1) if lines[j].strip()), None)
        if lastst is None:
            continue
        falls = not re.match(r"^\s*(break|continue|return|goto)\b", lines[lastst]) or dep[lastst] != d + 1
        run = []
        j = close + 1
        while j < len(lines) and len(run) < 3:
            s = lines[j]
            if not s.strip():
                j += 1
                continue
            if not s.startswith(ind) or s[len(ind):len(ind) + 1].isspace() or not (_simple_stmt(s) or re.match(r"^\s*return\b[^;]*;\s*$", s)):
                break
            run.append(j)
            if s.strip().startswith("return"):
                break
            j += 1
        for k in range(1, len(run) + 1):
            sts = [lines[x].strip() for x in run[:k]]
            nl = list(lines)
            for x in run[:k]:
                nl[x] = None
            for b in exits:
                bind = re.match(r"\s*", lines[b]).group(0)
                nl[b] = "\n".join(bind + s for s in sts) + "\n" + lines[b]
            cind = ind + "    "
            add = []
            if falls:
                add += [cind + "    " + s for s in sts] + [cind + "    break;"]
            if not has_def:
                add += [cind + "default:"] + [cind + "    " + s for s in sts] + [cind + "    break;"]
            if add:
                nl[close] = "\n".join(add) + "\n" + lines[close]
            yield f"dup_tail_block:sw:L{i}:{k}", "\n".join(x for x in nl if x is not None)
        # reverse
        if has_def and not falls and exits:
            prev = []
            for b in exits:
                p = b - 1
                while p > i and not lines[p].strip():
                    p -= 1
                prev.append(p)
            sts = {lines[p].strip() for p in prev}
            if len(sts) == 1 and all(_simple_stmt(lines[p]) for p in prev):
                st = sts.pop()
                ends_ok = all(re.match(r"^\s*(break|continue|return|goto)\b", lines[q]) for q in [lastst])
                if ends_ok:
                    nl = list(lines)
                    for p in prev:
                        nl[p] = None
                    nl[close] = lines[close] + "\n" + ind + st
                    yield f"dup_tail_block:sw:L{i}:rev", "\n".join(x for x in nl if x is not None)


_PURE_COND_CALL = re.compile(r"(?<![\w.>])(?!(?:sizeof|if|while|for|switch|return)\b)([A-Za-z_]\w*)\s*\(")


def _cond_pure(c: str) -> bool:
    return not (_PURE_COND_CALL.search(c) or re.search(r"\+\+|--|(?<![=!<>])=(?!=)", c))


def t_dup_head_stmt(fn):
    """ACT_updateMotionCore (o481): `q = E; if (c) {A} else {B}` ->
    `if (c) { q = E; A } else { q = E; B }` (gcc merges the copies into the
    branch delay slot). k = 1..3 statements right before the chain; the
    conditions must be pure and not read what they write. Reverse: an
    identical first statement of every branch hoisted before the chain."""
    head, inner, tail = _split_fn(fn)
    n = 0
    for ls, end, brs, has_else, ind in _if_chains(inner):
        conds = []
        p = ls
        ok = True
        for o, c in brs:
            seg = inner[p:o]
            mc = re.search(r"\bif\s*\(", seg)
            if mc:
                cp = match_brace(inner, p + mc.end() - 1)
                conds.append(inner[p + mc.end():cp])
            p = c + 1
        if not all(_cond_pure(c) for c in conds):
            continue
        L = inner.count("\n", 0, ls)
        # statements before the chain
        before = inner[:ls].rstrip("\n").split("\n")
        run = []
        for ln in reversed(before):
            if not ln.strip():
                break
            if not ln.startswith(ind) or ln[len(ind):len(ind) + 1].isspace() or not _simple_stmt(ln) or len(run) >= 3:
                break
            run.insert(0, ln.strip())
        for k in range(1, len(run) + 1):
            sts = run[-k:]
            ws = set().union(*[_writes(s) for s in sts])
            if not ws or any(re.search(r"(?<![\w.>])" + re.escape(w) + r"\b", c) for w in ws for c in conds):
                break
            # plain local targets only, no calls: nothing the conditions read can change
            if not all(re.match(r"^[A-Za-z_]\w*\s*=(?!=)", s) and _cond_pure(s.split("=", 1)[1]) for s in sts):
                break
            blk = "".join(f"\n{ind}    {s}" for s in sts)
            out = inner[:end + 1]
            for o, c in reversed(brs):
                out = out[:o + 1] + blk + out[o + 1:]
            if not has_else:
                out += f" else {{{blk}\n{ind}}}"
            pre = "\n".join(before[:len(before) - k])
            body = (pre + "\n" if pre else "") + out[ls:] + inner[end + 1:]
            yield f"dup_head_stmt:L{L}:{k}", head + inner[:0] + body + tail
            n += 1
        if has_else and len(brs) >= 2:
            firsts = []
            for o, c in brs:
                bl = [x for x in inner[o + 1:c].split("\n") if x.strip()]
                firsts.append(bl[0].strip() if bl else None)
            if firsts[0] and len(set(firsts)) == 1 and _simple_stmt(firsts[0]):
                st = firsts[0]
                ws = _writes(st)
                if ws and not any(re.search(r"(?<![\w.>])" + re.escape(w) + r"\b", c) for w in ws for c in conds):
                    out = inner
                    for o, c in reversed(brs):
                        seg = out[o + 1:c]
                        k0 = seg.index(st)
                        e0 = seg.find("\n", k0)
                        ls0 = seg.rfind("\n", 0, k0)
                        seg = seg[:ls0 + 1 if ls0 >= 0 else 0] + (seg[e0 + 1:] if e0 >= 0 else "")
                        out = out[:o + 1] + ("\n" if not seg.startswith("\n") else "") + seg + out[c:]
                    yield f"dup_head_stmt:L{L}:rev", head + out[:ls] + f"{ind}{st}\n" + out[ls:] + tail
                    n += 1
        if n > 12:
            return


def _strip_parens(e: str) -> str:
    e = e.strip()
    while e.startswith("(") and match_brace(e, 0) == len(e) - 1:
        e = e[1:-1].strip()
    return e


def _split_ternary(e: str):
    """`c ? a : b` at top level -> (c, a, b) or None."""
    depth, q = 0, -1
    nest = 0
    for i, ch in enumerate(e):
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        elif depth == 0 and ch == "?":
            if q < 0:
                q = i
            else:
                nest += 1
        elif depth == 0 and ch == ":" and q >= 0:
            if nest:
                nest -= 1
            else:
                return e[:q].strip(), e[q + 1:i].strip(), e[i + 1:].strip()
    return None


def _tern_chain(lhs: str, e: str, ind: str, depth: int = 0) -> list[str] | None:
    if depth > 4:
        return None
    t = _split_ternary(_strip_parens(e))
    if not t:
        return [f"{ind}{lhs} = {_strip_parens(e)};"]
    c, a, b = t
    out = [f"{ind}if ({_strip_parens(c)}) {{"]
    ta = _tern_chain(lhs, a, ind + "    ", depth + 1)
    if ta is None:
        return None
    out += ta
    bb = _strip_parens(b)
    if _split_ternary(bb):
        tb = _tern_chain(lhs, bb, ind, depth + 1)
        if tb is None:
            return None
        out.append(f"{ind}}} else " + tb[0].lstrip())
        out += tb[1:]
    else:
        out += [f"{ind}}} else {{", f"{ind}    {lhs} = {bb};", f"{ind}}}"]
    return out


def t_ternary_to_ifchain(fn):
    """FileSaveDataPush (o478): `rank = s >= 0x12D ? 3 : (s >= 0x65 ? 2 : 1);`
    -> `if (s >= 0x12D) { rank = 3; } else if (s >= 0x65) { rank = 2; } else
    { rank = 1; }`; reverse: such a chain -> nested ternary. Also a
    `do { ... } while (0);` wrapper -> a bare block (prologue save order) and
    a bare block -> do-while(0)."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        m = re.match(r"^(\s*)([A-Za-z_][\w.\->\[\]]*|\*\s*\(.*?\)\s*[\w.\->]+)\s*=(?!=)\s*(.*\?.*:.*);\s*$", ln)
        if not m or '"' in ln or parse_decl(ln) is not None:
            continue
        ch = _tern_chain(m.group(2), m.group(3), m.group(1))
        if not ch or len(ch) < 4:
            continue
        nl = lines[:i] + ch + lines[i + 1:]
        yield f"ternary_to_ifchain:L{i}", head + "\n".join(nl) + tail
        n += 1
    # reverse: if / else-if / else chain, each branch one `x = v;`
    for ls, end, brs, has_else, ind in _if_chains(inner):
        if not has_else or len(brs) < 2:
            continue
        vals, conds, lhs, p = [], [], None, ls
        ok = True
        for o, c in brs:
            seg = inner[p:o]
            mc = re.search(r"\bif\s*\(", seg)
            if mc:
                cp = match_brace(inner, p + mc.end() - 1)
                conds.append(inner[p + mc.end():cp].strip())
            st = [x.strip() for x in inner[o + 1:c].split("\n") if x.strip()]
            mm = re.match(r"^([A-Za-z_][\w.\->\[\]]*)\s*=(?!=)\s*([^;]+);$", st[0]) if len(st) == 1 else None
            if not mm or (lhs is not None and mm.group(1) != lhs):
                ok = False
                break
            lhs = mm.group(1)
            vals.append(mm.group(2).strip())
            p = c + 1
        if not ok or len(conds) != len(brs) - 1:
            continue
        e = vals[-1]
        for c, v in reversed(list(zip(conds, vals))):
            e = f"{c} ? {v} : {e}" if e == vals[-1] else f"{c} ? {v} : ({e})"
        L = inner.count("\n", 0, ls)
        yield f"ternary_to_ifchain:L{L}:rev", head + inner[:ls] + f"{ind}{lhs} = {e};" + inner[end + 1:] + tail
        n += 1
    dep = _depths(lines)
    for i, ln in enumerate(lines):
        if re.match(r"^\s*do\s*\{\s*$", ln):
            lb = _loop_body(lines, dep, i)
            if not lb or not lb[2] or not re.match(r"^\s*\}\s*while\s*\(\s*0\s*\)\s*;\s*$", lines[lb[1]]):
                continue
            if any(re.search(r"\b(break|continue)\s*;", lines[j]) for j in range(lb[0], lb[1])):
                continue
            nl = list(lines)
            nl[i] = re.sub(r"do\s*\{", "{", ln)
            nl[lb[1]] = re.sub(r"\}\s*while\s*\(\s*0\s*\)\s*;", "}", lines[lb[1]])
            yield f"ternary_to_ifchain:dowhile0-drop:L{i}", head + "\n".join(nl) + tail
        elif ln.strip() == "{" and i > 0 and (lines[i - 1].rstrip().endswith((";", "}", "{")) or not lines[i - 1].strip()) \
                and not re.match(r"^\s*(if|else|for|while|do|switch)\b.*\)\s*$", lines[i - 1]):
            close = next((j for j in range(i + 1, len(lines)) if dep[j] == dep[i] + 1 and lines[j].strip().startswith("}")), None)
            if close is None or lines[close].strip() != "}":
                continue
            if any(re.search(r"\b(break|continue)\s*;", lines[j]) for j in range(i + 1, close)):
                continue
            nl = list(lines)
            nl[i] = ln.replace("{", "do {")
            nl[close] = lines[close].replace("}", "} while (0);")
            yield f"ternary_to_ifchain:dowhile0-add:L{i}", head + "\n".join(nl) + tail


_MIRROR_REL = {"<": ">", ">": "<", "<=": ">=", ">=": "<="}


def rel_mirror_shape(rows) -> list[str]:
    """A pure move including a load, or a differing slt / sltu / c.cond row:
    the operand order of a comparison decides which load comes first."""
    mv = moved_insns(rows)
    if mv and any(_LOAD_MN.match(_mn(x)) for x in mv):
        return mv
    out = []
    for a, m, b in diff_rows(rows):
        if m == "|" and a and b and _mn(a) == _mn(b) and re.match(r"^(slt|sltu|c\.\w+\.[sd])$", _mn(a)):
            out.append(a)
    return out


def _top_has(s: str, chars: str) -> bool:
    d = 0
    s = s.replace("->", "..")
    for ch in s:
        if ch in "([":
            d += 1
        elif ch in ")]":
            d -= 1
        elif d == 0 and ch in chars:
            return True
    return False


def t_rel_mirror(fn, ctx=None):
    """actStockPick (o476): `c >= max` -> `max <= c` (operands swapped,
    operator mirrored): the left operand is expanded first, which orders the
    two loads. Diff-gated (rel_mirror_shape); every relational comparison in
    a condition, one at a time."""
    rows = (ctx or {}).get("rows")
    if not rows or not rel_mirror_shape(rows):
        return
    head, inner, tail = _split_fn(fn)
    n = 0
    seen = set()
    for s, e in sorted(set(_cond_spans(inner))):
        cond = inner[s:e]
        for m in _REL.finditer(cond):
            op = m.group(1)
            if op not in _MIRROR_REL:
                continue
            a0 = _operand_start(cond, m.start())
            b1 = _operand_end(cond, m.end())
            A, B = cond[a0:m.start()], cond[m.end():b1]
            if not A.strip() or not B.strip() or _top_has(A, "&|^=<>?") or _top_has(B, "&|^=<>?"):
                continue
            if cond[:a0].rstrip().endswith(("!", "~", "-", "*", "&")) and not cond[:a0].rstrip().endswith(("&&",)):
                continue
            lw = A[:len(A) - len(A.lstrip())]
            new = cond[:a0] + lw + B.strip() + f" {_MIRROR_REL[op]} " + A.strip() + B[len(B.rstrip()):] + cond[b1:]
            key = (s, m.start())
            if key in seen:
                continue
            seen.add(key)
            yield f"rel_mirror:{A.strip()[:10]}{op}{B.strip()[:10]}", head + inner[:s] + new + inner[e:] + tail
            n += 1
            if n >= 16:
                return


_FMT_CALL = re.compile(r"(?<![\w.>])([A-Za-z_]\w*)\s*\(")


def _call_args(s: str, o: int):
    """(close index, [(start, end) of each top-level arg]) of the call whose '(' is at o."""
    c = match_brace(s, o)
    if c < 0:
        return None
    out, d, st, i = [], 0, o + 1, o + 1
    while i < c:
        ch = s[i]
        if ch in "\"'":
            q = ch
            i += 1
            while i < c and s[i] != q:
                i += 2 if s[i] == "\\" else 1
        elif ch in "([{":
            d += 1
        elif ch in ")]}":
            d -= 1
        elif ch == "," and d == 0:
            out.append((st, i))
            st = i + 1
        i += 1
    out.append((st, c))
    return c, out


_DPCONV = re.compile(r"^\s*(?:\(\s*(?:bitwise\s+)?(?:s32|int|u32|unsigned\s+int|f64|double)\s*\)\s*)?"
                     r"(?:fptodp\s*\((?P<a>.*)\)|\(\s*double\s*\)\s*(?P<b>.*))\s*$", re.S)


def t_float_vararg(fn):
    """InitMapKoware (o474): printf("..%f..", fptodp(x)) / (double)x / m2c's
    `t = (s32) fptodp(x); printf(.., t)` -> pass the float x directly (the
    default promotion emits the conversion call itself, in another order).
    Only calls whose arguments include a string literal (a format)."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    # m2c temps used once in a format call
    temps = {}
    for i, ln in enumerate(lines):
        m = re.match(r"^\s*([A-Za-z_]\w*)\s*=\s*(.*);\s*$", ln)
        if m and parse_decl(ln) is None:
            md = _DPCONV.match(m.group(2))
            if md and (md.group("a") or md.group("b")):
                temps[m.group(1)] = (i, (md.group("a") or md.group("b")).strip())
    sub = []
    for m in _FMT_CALL.finditer(inner):
        if m.group(1) in KEYWORDS or m.group(1) in ("sizeof", "fptodp"):
            continue
        r = _call_args(inner, m.end() - 1)
        if not r:
            continue
        c, args = r
        if not any(inner[a:b].strip().startswith('"') for a, b in args):
            continue
        for a, b in args:
            x = inner[a:b]
            md = _DPCONV.match(x)
            if md and (md.group("a") or md.group("b")):
                v = (md.group("a") or md.group("b")).strip()
                sub.append((a, b, x[:len(x) - len(x.lstrip())] + v, None))
            elif x.strip() in temps:
                sub.append((a, b, x[:len(x) - len(x.lstrip())] + temps[x.strip()][1], x.strip()))
    if not sub:
        return
    sub = sorted(set(sub), key=lambda t: -t[0])
    new = inner
    for a, b, v, _ in sub:
        new = new[:a] + v + new[b:]
    dropped = {t for *_, t in sub if t}
    if dropped:
        nl = new.split("\n")
        for t in dropped:
            i, _ = temps[t]
            if len(re.findall(r"(?<![\w.>])" + re.escape(t) + r"\b", new)) == 2:   # its decl-free def + the one use
                nl[i] = None
        new = "\n".join(x for x in nl if x is not None)
    yield "float_vararg:direct", head + new + tail
    # only the explicit conversions (temps kept)
    if dropped and any(t is None for *_, t in sub):
        new2 = inner
        for a, b, v, t in sub:
            if t is None:
                new2 = new2[:a] + v + new2[b:]
        yield "float_vararg:direct-noTemp", head + new2 + tail


def vswap_shape(rows) -> list[str]:
    """Rows equal up to a v0 <-> v1 swap, after a call in the target."""
    out = []
    seen_call = False
    for a, m, b in diff_rows(rows):
        if a and _mn(a) in ("jal", "jalr"):
            seen_call = True
        if m == "|" and a and b and seen_call:
            sw = re.sub(r"\bv[01]\b", lambda x: "v1" if x.group(0) == "v0" else "v0", a)
            if sw == b:
                out.append(a)
    return out


def _callee_void(text, name, ctx):
    """callee_void (specSyoukanProc, o477; xglRenderMove 0.891 -> 0.913): a prototype `[extern] int f(..);`
    in the body file whose calls in NAME are all plain statements (value
    unused) -> `void f(..);`, and the reverse; gated on a v0/v1 swap after a
    call (an int callee's dead v0 set changes local-alloc's choice)."""
    rows = (ctx or {}).get("rows")
    if not rows or not vswap_shape(rows):
        return
    loc = find_function(text, name)
    if not loc:
        return
    fn = text[loc[0]:loc[2] + 1]
    pre = text[:loc[0]]
    pro = re.compile(r"^(?P<lead>[ \t]*(?:extern\s+)?)(?P<t>int|s32|unsigned\s+int|u32|void)(?P<rest>\s+(?P<n>[A-Za-z_]\w*)\s*\([^;{}]*\)\s*;)", re.M)
    for m in pro.finditer(pre):
        f = m.group("n")
        if f == name:
            continue
        calls = list(re.finditer(r"(?<![\w.>])" + re.escape(f) + r"\s*\(", fn))
        if not calls:
            continue
        stmt_only = True
        for c in calls:
            pre_c = fn[:c.start()].rstrip()
            cp = match_brace(fn, c.end() - 1)
            if not (pre_c.endswith((";", "{", "}", ":")) or re.search(r"\belse$", pre_c)) or cp < 0 \
                    or not fn[cp + 1:].lstrip().startswith(";"):
                stmt_only = False
                break
        nt = "void" if m.group("t") != "void" else "int"
        if nt == "void" and not stmt_only:
            continue
        new = text[:m.start()] + m.group("lead") + nt + m.group("rest") + text[m.end():]
        yield f"callee_void:{f}:{m.group('t').replace(' ', '')}->{nt}", new


# ---- fold9 (2026-10-07, o483-o495) ------------------------------------------

def _f9_cols(rows):
    """(target, candidate) insn columns without addresses, or None."""
    import try_fn                                          # noqa: PLC0415
    c = try_fn.cols_from_rows(rows)
    if not c:
        return None
    strip = lambda s: s.split(":", 1)[1].strip() if ":" in s else s.strip()  # noqa: E731
    return [strip(x) for x in c[0]], [strip(x) for x in c[1]]


def store_width_shape(rows) -> list[dict]:
    """Non-frame offsets stored with another width only (ours sw, target sh):
    try_fn.store_width_mismatch (also a suspect-seed tell)."""
    import try_fn                                          # noqa: PLC0415
    c = _f9_cols(rows)
    if not c:
        return []
    return [{"ours": mn, "off": off, "target": t} for mn, off, t in try_fn.store_width_mismatch(*c)]


def ptr_stride_shape(rows) -> list[dict]:
    """Aligned `addiu rX,rS,K` with another addend, our K a multiple of the
    target's or the reverse (m2c `p += 4` on an `int *` adds 16 bytes)."""
    import try_fn                                          # noqa: PLC0415
    c = _f9_cols(rows)
    if not c:
        return []
    out = []
    for reg, vb, va in try_fn.addend_mismatch(*c):
        if va and vb and (vb % va == 0 or va % vb == 0):
            out.append({"reg": reg, "ours": vb, "target": va})
    return out


_BT_SHIFT = re.compile(r"^(srl|sra|sll|dsrl|dsra|dsll)$")


def bit_test_shape(rows) -> list[dict]:
    """Target `andi rT,rS,K` followed within 6 insns by an in-place shift of rS
    (`srl rS,rS,n`) where ours shifts first: the C tests the bit of the old
    value after the update (o488 FCV_getAttrAndValue: `b = flags & 4;
    flags >>= 3; if (b)`)."""
    c = _f9_cols(rows)
    if not c:
        return []

    def pairs(col, first_andi):
        s = set()
        for i, x in enumerate(col):
            mn, o, rel = _insn_ops(x)
            if rel or len(o) != 3:
                continue
            want = (mn == "andi") if first_andi else bool(_BT_SHIFT.match(mn) and o[0] == o[1])
            if not want:
                continue
            for y in col[i + 1:i + 7]:
                m2, o2, r2 = _insn_ops(y)
                if r2 or len(o2) != 3:
                    continue
                if first_andi and _BT_SHIFT.match(m2) and o2[0] == o2[1] == o[1]:
                    s.add((o[2], m2, o2[2]))
                    break
                if not first_andi and m2 == "andi" and o2[1] == o[0]:
                    s.add((o2[2], mn, o[2]))
                    break
        return s
    t_first = pairs(c[0], True)
    c_first = pairs(c[1], True)
    return [{"k": k, "shift": sh, "n": n} for k, sh, n in sorted(t_first - c_first)]


def _ptr_decls(fn: str) -> dict:
    """{name: pointee-ful type text} of pointer params and locals (`T *p`)."""
    out = {}
    for _, _, p in _params(fn):
        m = re.match(r"^\s*((?:const\s+|volatile\s+)*[A-Za-z_][\w\s]*?)\s*(\*+)\s*(\w+)\s*$", p)
        if m:
            out[m.group(3)] = f"{m.group(1).strip()} {m.group(2)}"
    _, inner, _ = _split_fn(fn)
    for ln in inner.split("\n"):
        p = parse_decl(ln)
        if not p:
            continue
        for d in p[2]:
            m = re.match(r"^\s*(\*+)\s*(\w+)\s*(?:=.*)?$", d)
            if m and "(" not in d.split("=")[0]:
                out[m.group(2)] = f"{p[1]} {m.group(1)}"
    return out


def t_ret_param(text, name, ctx=None):
    """o487 ACT_createChr: `void f(..)` whose target never reuses v0 after its
    last call while ours does (the value stays live in v0 = it is returned)
    -> `T *f(..)` with `return p;` for a pointer param / a pointer local
    assigned from a call (every `return;` too). Reverse of void_ret's
    int->void; forward declarations flipped like void_ret (TU via patches)."""
    ctx = ctx or {}
    rows = ctx.get("rows")
    loc = find_function(text, name)
    if not rows or not loc:
        return
    s, b, e = loc
    fn = text[s:e + 1]
    rt = _fn_rettype(fn, name)
    if rt is None or " ".join(w for w in rt.split() if w not in ("static", "extern", "inline")) != "void":
        return
    dr = diff_rows(rows)
    if not any(m != " " for _, m, _ in dr):
        return

    def v0w(col):
        last = max([i for i, x in enumerate(col) if _mn(x) in ("jal", "jalr")] or [-1])
        return [(i, x) for i, x in enumerate(col) if i > last + 1 and _V0W.match(x)]
    tcol = [a for a, _, _ in dr if a]
    tw, cw = v0w(tcol), v0w([c for _, _, c in dr if c])
    # the target leaves v0 alone after its last call (ACT_createChr: the call
    # result stays there), or only copies the value in at the very end
    # (`move v0,a0` of a returned param)
    if not len(tw) < len(cw):
        return
    if tw and not (len(tw) == 1 and _mn(tw[0][1]) == "move" and tw[0][0] >= len(tcol) - 4):
        return
    _, inner, _ = _split_fn(fn)
    ptrs = _ptr_decls(fn)
    pnames = {re.search(r"(\w+)\s*$", p).group(1) for _, _, p in _params(fn) if re.search(r"(\w+)\s*$", p)}
    cands = []
    for v, ty in ptrs.items():
        from_call = re.search(r"(?<![\w.>])" + re.escape(v) + r"\s*=\s*(?:\([^()]*\)\s*)?[A-Za-z_]\w*\s*\(", inner)
        if v in pnames or from_call:
            cands.append((v, ty))
    pat = ctx.setdefault("patches", {})
    for v, ty in cands[:4]:
        nf = fn[:fn.index(rt)] + rt.replace("void", ty, 1) + fn[fn.index(rt) + len(rt):]
        nf = re.sub(r"\*\s+(" + re.escape(name) + r"\s*\()", r"*\1", nf, count=1)
        h2, in2, t2 = _split_fn(nf)
        in2 = re.sub(r"\breturn\s*;", f"return {v};", in2)
        body = in2.rstrip()
        if not re.search(r"\breturn\b[^;]*;$", body):
            body += f"\n    return {v};"
        nf = h2 + body + "\n" + t2
        new = text[:s] + nf + text[e + 1:]
        new = re.sub(r"^([^\n;{}()#=]*?)\bvoid(\s+" + re.escape(name) + r"\s*\([^;{}]*\)\s*;)",
                     lambda m: m.group(1) + ty + m.group(2), new, flags=re.M)
        pp = _decl_patches(ctx.get("unit", ""), ctx.get("tu", ""), name, "void", ty) if ctx.get("unit") else []
        if pp is None:
            continue
        if pp:
            pat[body_sha(new)] = pp
        yield f"ret_param:{v}", new


def _switch_groups(body: str):
    """Case groups of a switch body: (lead lines, [(label lines, stmt lines)])
    or None when a label shares its line with a statement / code precedes
    the first label."""
    lines = body.split("\n")
    dep = _depths(lines)
    lead, groups, cur = [], [], None
    for j, ln in enumerate(lines):
        if dep[j] == 0 and re.match(r"^\s*(case\b|default\b)", ln):
            if not re.match(r"^\s*(?:case\s+[^:]+|default\s*):\s*$", ln) or "?" in ln:
                return None
            if cur is None or any(x.strip() for x in cur[1]):
                cur = ([ln], [])
                groups.append(cur)
            else:
                cur[0].append(ln)
        elif cur is None:
            if ln.strip():
                return None
            lead.append(ln)
        else:
            cur[1].append(ln)
    return lead, groups


_JUMP_END = re.compile(r"^\s*(?:break\s*;|continue\s*;|return\b[^;]*;|goto\s+\w+\s*;)\s*$")


def _case_perms(n: int):
    import itertools
    if n <= 4:
        ps = [p for p in itertools.permutations(range(n)) if list(p) != list(range(n))]
    else:
        ps = [tuple(range(k, n)) + tuple(range(k)) for k in range(1, n)]
        for k in range(n - 1):
            q = list(range(n))
            q[k], q[k + 1] = q[k + 1], q[k]
            ps.append(tuple(q))
        ps.append(tuple(reversed(range(n))))
    return ps[:24]


def _switch_orders(fn: str, only_at: int | None = None, tag: str = ""):
    """Every case-group order of each switch (2-6 groups, no fall-through)."""
    for m in re.finditer(r"\bswitch\s*\(", fn):
        if only_at is not None and m.start() < only_at:
            continue
        o = m.end() - 1
        c = match_brace(fn, o)
        if c < 0:
            continue
        mm = re.compile(r"\s*\{").match(fn, c + 1)
        if not mm:
            continue
        bo = mm.end() - 1
        bc = match_brace(fn, bo)
        if bc < 0:
            continue
        sg = _switch_groups(fn[bo + 1:bc])
        if not sg:
            continue
        lead, groups = sg
        if not 2 <= len(groups) <= 6:
            continue
        gs = []
        ok = True
        for k, (labs, st) in enumerate(groups):
            st = list(st)
            while st and not st[-1].strip():
                st.pop()
            if not st or not _JUMP_END.match(st[-1]):
                if k != len(groups) - 1:
                    ok = False             # fall-through into the next group
                    break
                ind = re.match(r"\s*", next((x for x in st if x.strip()), labs[0] + "    ")).group(0)
                st.append(f"{ind}break;")
            gs.append(labs + st)
        if not ok:
            continue
        trail = fn[bo + 1:bc].split("\n")
        end_ind = trail[-1] if not trail[-1].strip() else ""
        for p in _case_perms(len(gs)):
            nb = "\n".join(lead + [x for i in p for x in gs[i]]) + "\n" + end_ind
            yield f"switch_case_order:{tag}L{fn.count(chr(10), 0, m.start())}:{''.join(map(str, p))}", \
                fn[:bo + 1] + nb + fn[bc:]
        if only_at is not None:
            return


def t_switch_case_order(fn):
    """o485 MenuItemListMake00_1: the order of the case blocks of a switch
    changes gcc's compare tree / block layout; every order of 2-4 groups
    (rotations / adjacent swaps / reverse for 5-6), groups that end in
    break / return / continue / goto only; plus if-chain -> switch
    (if_to_switch) in every order."""
    n = 0
    for lab, nf in _switch_orders(fn):
        yield lab, nf
        n += 1
        if n >= 48:
            return
    for lab, nf in t_if_to_switch(fn):
        at = len(os.path.commonprefix([fn, nf]))
        at = nf.rfind("switch", 0, at + 7)
        for lab2, nf2 in _switch_orders(nf, only_at=max(at, 0), tag="ifsw:"):
            yield lab2, nf2
            n += 1
            if n >= 96:
                return


def t_bit_test_hoist(fn, ctx=None):
    """o488 FCV_getAttrAndValue: target tests a bit of the OLD value then
    shifts in place (`andi; srl rS,rS,3`) where the seed shifts first and
    tests after: `x >>= n; if (x & K)` -> `_bt = x & K; x >>= n; if (_bt)`
    (gated on bit_test_shape; the gate's K is tried too)."""
    shp = (ctx or {}).get("bit_test_hoist")
    if not shp:
        return
    ks = [d["k"] for d in shp]
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    first = _decl_end(inner)
    sh = re.compile(r"^(?P<ind>\s*)(?P<x>[A-Za-z_]\w*)\s*(?:(?:>>|<<)=\s*(?P<n1>\w+)|=\s*(?:\([\w\s]+\)\s*)?\(?\s*(?P=x)\s*(?:>>|<<)\s*(?P<n2>\w+)\s*\)?)\s*;\s*$")
    n = 0
    for i, ln in enumerate(lines):
        m = sh.match(ln)
        if not m:
            continue
        x = m.group("x")
        j = i + 1
        while j < len(lines) and not lines[j].strip():
            j += 1
        if j >= len(lines):
            continue
        tm = re.search(r"\(\s*" + re.escape(x) + r"\s*&\s*(0x[0-9a-fA-F]+|\d+)\s*\)|(?<![\w.>])" + re.escape(x)
                       + r"\s*&\s*(0x[0-9a-fA-F]+|\d+)", lines[j])
        if not tm or not re.match(r"^\s*(?:\}\s*else\s+)?if\s*\(", lines[j]):
            continue
        K = tm.group(1) or tm.group(2)
        for kk in [K] + [k for k in ks if _ival(k) != _ival(K)]:
            nl = list(lines)
            nl[j] = nl[j][:tm.start()] + ("(_bt)" if tm.group(1) else "_bt") + nl[j][tm.end():]
            nl.insert(i, f"{m.group('ind')}_bt = {x} & {kk};")
            nl.insert(first, "    unsigned int _bt;")
            yield f"bit_test_hoist:L{i}:{kk}", head + "\n".join(nl) + tail
            n += 1
            if n >= 12:
                return


_SW_TYPES = {1: ["unsigned char", "signed char"], 2: ["short", "unsigned short"], 4: ["int"],
             8: ["long long"]}
_SW_WIDTH = {"sb": 1, "sh": 2, "sw": 4, "sd": 8}
_INTLIKE = {"char": 1, "signed char": 1, "unsigned char": 1, "u8": 1, "s8": 1,
            "short": 2, "unsigned short": 2, "signed short": 2, "u16": 2, "s16": 2,
            "int": 4, "unsigned int": 4, "unsigned": 4, "signed int": 4, "u32": 4, "s32": 4, "long": 4,
            "unsigned long": 4, "long long": 8, "unsigned long long": 8, "u64": 8, "s64": 8}
_CAST_DEREF = re.compile(r"\*\s*\(\s*\(?\s*(?P<T>(?:const\s+|volatile\s+)*[A-Za-z_][\w ]*?)\s*\*\s*\)")


def t_store_width_cast(fn, ctx=None):
    """ov01/cmd seeds (o494 cmdStatGet, o495 cmdMsg): m2c's `*(int *)((char *)p
    + 4) = ..` for a short field gives `sw` where the target has `sh`. A
    store-width mismatch at one offset (store_width_shape) retypes the casts
    of that width: every cast whose text names the offset (reads too: the
    field is narrower), each assignment alone, and a field store `X->f = ..`
    -> `*(T *)&X->f = ..`."""
    shp = (ctx or {}).get("store_width_cast")
    if not shp:
        return
    head, inner, tail = _split_fn(fn)
    seen = set()
    n = 0
    for d in shp:
        ow = _SW_WIDTH.get(d["ours"])
        tws = [_SW_WIDTH[t] for t in d["target"] if t in _SW_WIDTH]
        if not ow or not tws:
            continue
        off = d["off"]
        offre = re.compile(r"(?<![\w.])(?:0[xX]0*%x|%d)(?![\w.])" % (off, off)) if off else None
        for tw in tws:
            for T in _SW_TYPES[tw]:
                # (a) every cast of our width whose expression names the offset
                sites = []
                for m in _CAST_DEREF.finditer(inner):
                    ty = re.sub(r"\s+", " ", m.group("T").replace("const", "").replace("volatile", "")).strip()
                    if _INTLIKE.get(ty) != ow:
                        continue
                    k = m.end()
                    while k < len(inner) and inner[k] in " \t":
                        k += 1
                    if k < len(inner) and inner[k] == "(":
                        ce = match_brace(inner, k)
                        seg = inner[k:ce + 1] if ce > 0 else inner[k:k + 60]
                    else:
                        seg = re.match(r"[\w.>\-\[\]]*", inner[k:]).group(0)
                    named = bool(offre.search(seg)) if offre else not re.search(r"[+-]\s*(?:0[xX])?[0-9a-fA-F]+\s*\)*$", seg)
                    sites.append((m.start("T"), m.end("T"), named))
                for lab, pick in (("named", [s for s in sites if s[2]]), ("allw", sites)):
                    if not pick:
                        continue
                    ni = inner
                    for a, b2, _ in sorted(pick, reverse=True):
                        ni = ni[:a] + T + ni[b2:]
                    if ni not in seen:
                        seen.add(ni)
                        yield f"store_width_cast:{lab}:{T.replace(' ', '_')}@{off}", head + ni + tail
                        n += 1
                # (b) field stores
                lines = inner.split("\n")
                for i, ln in enumerate(lines):
                    m = re.match(r"^(\s*)(" + _FIELD + r")\s*(=(?!=)|[+\-|&^]=)(.*)$", ln)
                    if not m or ln.strip().startswith(("if", "while", "for", "return")):
                        continue
                    nl = list(lines)
                    nl[i] = f"{m.group(1)}*({T} *)&{m.group(2)} {m.group(3)}{m.group(4)}"
                    ni = "\n".join(nl)
                    if ni not in seen:
                        seen.add(ni)
                        yield f"store_width_cast:field:{T.replace(' ', '_')}:L{i}", head + ni + tail
                        n += 1
                    if n >= 40:
                        return


def t_ptr_stride(fn, ctx=None):
    """m2c byte strides on typed pointers (MenuSortChange `var_3 += 4` on an
    `s32 *`, MAP_updateUnitPartsSequence, nmlModelDirectXtxSub, o488's
    `var_16 += 0x10` on an `f32 *`): an aligned addiu adds K*r bytes where
    the target adds K (ptr_stride_shape) -> `p += L` / `p -= L` /
    `p = p + L` with L scaled by target/ours (`p++` when it becomes 1);
    each site alone, then all."""
    shp = (ctx or {}).get("ptr_stride")
    if not shp:
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    st = re.compile(r"^(?P<ind>\s*)(?P<x>[A-Za-z_]\w*)\s*(?:(?P<op>[+-])=\s*(?P<l>0[xX][0-9a-fA-F]+|\d+)"
                    r"|=\s*(?P=x)\s*(?P<op2>[+-])\s*(?P<l2>0[xX][0-9a-fA-F]+|\d+))\s*;\s*$")
    ptrs = set(_ptr_decls(fn))
    # `void (**cb)(T *)`: a pointer to function pointers steps like any pointer
    ptrs |= set(re.findall(r"\(\s*\*\s*\*+\s*(\w+)\s*\)\s*\(", inner))
    for d in shp:
        vb, va = d["ours"], d["target"]
        hits = []
        for i, ln in enumerate(lines):
            m = st.match(ln)
            if not m or m.group("x") not in ptrs:
                continue
            L = _ival(m.group("l") or m.group("l2"))
            if not L or (L * va) % vb:
                continue
            L2 = L * va // vb
            if L2 == L or L2 == 0:
                continue
            op = m.group("op") or m.group("op2")
            rep = f"{m.group('ind')}{m.group('x')}{'++' if op == '+' else '--'};" if L2 == 1 else \
                f"{m.group('ind')}{m.group('x')} {op}= {L2};"
            hits.append((i, rep))
        for i, rep in hits:
            nl = list(lines)
            nl[i] = rep
            yield f"ptr_stride:L{i}:{vb}->{va}", head + "\n".join(nl) + tail
        if len(hits) > 1:
            nl = list(lines)
            for i, rep in hits:
                nl[i] = rep
            yield f"ptr_stride:all:{vb}->{va}", head + "\n".join(nl) + tail



# ---- fold10 (2026-10-07, o492-o509) -----------------------------------------

_STABS_CACHE: dict = {}
_STABS_STR = re.compile(r'^\s*\.stabs\s+"((?:[^"\\]|\\.)*)",\s*(\d+)', re.M)


class _StabsParser:
    """Just enough of gcc 2.96's -gstabs type grammar for struct layouts:
    ranges, pointers, structs/unions (+ bitfields), arrays, enums, forward
    references (`xsTag:`), functions, const/volatile."""

    def __init__(self):
        self.types: dict = {}       # number -> node
        self.names: dict = {}       # number -> typedef / basic type name
        self.tags: dict = {}        # struct/union tag -> number
        self.anon = 0

    def parse_def(self, s: str):
        """One `.stabs` string `NAME:<letter>TYPE`: returns (name, letter, type id)."""
        m = re.match(r"([^:]*):([A-Za-z]?)", s)
        if not m:
            return None
        self.s, self.i = s, m.end()
        name, letter = m.group(1), m.group(2)
        try:
            tid = self.ptype()
        except (IndexError, ValueError):
            return None
        if letter == "t" and isinstance(tid, int) and name:
            self.names.setdefault(tid, name)
        if letter == "T" and name:
            self.tags[name] = tid
        return name, letter, tid

    def num(self) -> int:
        m = re.compile(r"-?\d+").match(self.s, self.i)
        self.i = m.end()
        return int(m.group(0))

    def ptype(self):
        c = self.s[self.i]
        if c.isdigit():
            n = self.num()
            if self.i < len(self.s) and self.s[self.i] == "=":
                self.i += 1
                if self.s[self.i].isdigit():
                    self.types[n] = ("ref", self.ptype())     # `160=161=xsDrill:`
                else:
                    self.types[n] = self.pdesc()
            return n
        self.anon -= 1
        a = self.anon
        self.types[a] = self.pdesc()
        return a

    def pdesc(self):
        c = self.s[self.i]
        self.i += 1
        if c == "r":
            ref = self.ptype()
            self.i += 1                     # ;
            lo = self.num()
            self.i += 1
            hi = self.num()
            self.i += 1
            return ("range", ref, lo, hi)
        if c == "*":
            return ("ptr", self.ptype())
        if c in "su":
            size = self.num()
            fields = []
            while self.s[self.i] != ";":
                j = self.s.index(":", self.i)
                fname = self.s[self.i:j]
                self.i = j + 1
                ft = self.ptype()
                self.i += 1                 # ,
                off = self.num()
                self.i += 1
                bits = self.num()
                self.i += 1                 # ;
                fields.append((fname, ft, off, bits))
            self.i += 1
            return ("struct" if c == "s" else "union", size, fields)
        if c == "a":
            idx = self.ptype()
            el = self.ptype()
            ix = self.types.get(idx)
            lo, hi = (ix[2], ix[3]) if ix and ix[0] == "range" else (0, -1)
            return ("array", lo, hi, el)
        if c == "e":
            while self.s[self.i] != ";":
                self.i = self.s.index(",", self.i) + 1
            self.i += 1
            return ("enum",)
        if c == "x":
            k = self.s[self.i]
            j = self.s.index(":", self.i)
            tag = self.s[self.i + 1:j]
            self.i = j + 1
            return ("xref", k, tag)
        if c == "f":
            return ("func", self.ptype())
        if c in "kB":
            return ("ref", self.ptype())
        raise ValueError(f"stabs desc {c!r}")

    # -- queries --------------------------------------------------------------
    def resolve(self, t, depth: int = 0):
        """Follow plain references / typedef chains / forward tags."""
        while depth < 40:
            depth += 1
            n = self.types.get(t)
            if isinstance(t, int) and n is None:
                return t, None
            if n is None:
                return t, None
            if isinstance(n, int):
                t = n
                continue
            if n[0] == "ref":
                t = n[1]
                continue
            if n[0] == "xref":
                t2 = self.tags.get(n[2])
                if t2 is None or t2 == t:
                    return t, n
                t = t2
                continue
            return t, n
        return t, None

    def scalar(self, t):
        """(size, kind) of a scalar type: kind 'f' float, 's' signed, 'u'
        unsigned, 'p' pointer; None for aggregates."""
        t, n = self.resolve(t)
        if n is None:
            return None
        if n[0] == "ptr":
            return 4, "p"
        if n[0] == "enum":
            return 4, "s"
        if n[0] != "range":
            return None
        _, _, lo, hi = n
        if hi == 0 and lo > 0:
            return lo, "f"
        nm = self.names.get(t, "")
        if hi in (127, 255) or "char" in nm:
            return 1, "s" if lo < 0 else "u"
        if hi in (32767, 65535):
            return 2, "s" if lo < 0 else "u"
        if hi in (2147483647, 4294967295):
            return 4, "s" if lo < 0 else "u"
        return 8, "s" if lo < 0 else "u"

    def size(self, t) -> int | None:
        sc = self.scalar(t)
        if sc:
            return sc[0]
        t, n = self.resolve(t)
        if n is None:
            return None
        if n[0] in ("struct", "union"):
            return n[1]
        if n[0] == "array":
            es = self.size(n[3])
            return None if es is None or n[2] < n[1] else es * (n[2] - n[1] + 1)
        return None

    def leaves(self, t, K: int, width: int, path: str = "", depth: int = 0):
        """[(path, (size, kind))] scalar members exactly at byte offset K of width WIDTH."""
        if depth > 8:
            return []
        sc = self.scalar(t)
        if sc:
            return [(path, sc)] if K == 0 and sc[0] == width else []
        t, n = self.resolve(t)
        if n is None:
            return []
        out = []
        if n[0] in ("struct", "union"):
            for fname, ft, off, bits in n[2]:
                if off % 8 or not fname:
                    continue
                fs = self.size(ft)
                if fs is None or bits != fs * 8:
                    continue                # bitfield
                o = off // 8
                if o <= K < o + fs:
                    out += self.leaves(ft, K - o, width, f"{path}.{fname}", depth + 1)
        elif n[0] == "array":
            es = self.size(n[3])
            if es:
                k = K // es
                if n[1] <= k <= n[2]:
                    out += self.leaves(n[3], K - k * es, width, f"{path}[{k}]", depth + 1)
        return out


def stabs_info(unit: str, tu: str, name: str, text: str) -> dict | None:
    """{"p": _StabsParser, "vars": {local/param name: type id}, "globals": {...}}
    from the TU compiled with -gstabs (the body for NAME in place); cached per
    body text. None when the compile fails."""
    import shutil
    import try_fn
    key = (unit, tu, name, hashlib.sha1(text.encode()).hexdigest())
    if key in _STABS_CACHE:
        return _STABS_CACHE[key]
    res = None
    fd, p = tempfile.mkstemp(prefix=f"stabs_{name}_", suffix=".c")
    os.write(fd, text.encode())
    os.close(fd)
    try:
        r = try_fn.try_function(unit, tu, name, Path(p), keep=True, diff_limit=0)
        scratch = Path(r.get("scratch", ""))
        try:
            ii = list((scratch / "cand").glob("*.i")) if scratch.name else []
            if ii:
                gcc, flags = _cc1_cmd(unit, tu)
                with tempfile.TemporaryDirectory(prefix="stabs_") as td:
                    shutil.copyfile(ii[0], Path(td) / "x.i")
                    subprocess.run([*_noaslr(), gcc, *flags, "-gstabs", "-S", "x.i", "-o", "x.s"], cwd=td,
                                   capture_output=True, text=True, timeout=120)
                    xs = Path(td) / "x.s"
                    if xs.is_file():
                        res = _parse_stabs(xs.read_text(errors="replace"), name)
        finally:
            if scratch.name:
                shutil.rmtree(scratch, ignore_errors=True)
    except Exception:  # noqa: BLE001
        res = None
    finally:
        os.unlink(p)
    if len(_STABS_CACHE) > 64:
        _STABS_CACHE.clear()
    _STABS_CACHE[key] = res
    return res


def _parse_stabs(asm: str, name: str) -> dict:
    sp = _StabsParser()
    vars_: dict = {}
    globs: dict = {}
    infn = False
    buf = ""
    for m in _STABS_STR.finditer(asm):
        s = m.group(1).replace('\\"', '"')
        if s.endswith("\\\\"):              # continued in the next .stabs
            buf += s[:-2]
            continue
        s, buf = buf + s, ""
        d = sp.parse_def(s)
        if not d:
            continue
        nm, letter, tid = d
        if letter in ("F", "f"):
            infn = nm == name
            continue
        if letter == "G":
            globs[nm] = tid
        elif letter == "S" and not infn:
            globs.setdefault(nm, tid)
        elif infn and letter in ("", "P", "p", "r", "V") and nm:
            vars_.setdefault(nm, tid)
    return {"p": sp, "vars": vars_, "globals": globs}


def _t_kind(T: str):
    """(size, kind) of a cast type text: kind f / s / u / p."""
    core = re.sub(r"\b(const|volatile)\b", "", T).strip()
    if core.endswith("*"):
        return 4, "p"
    sz = _SCALAR_SIZE.get(core)
    if sz is None:
        return None
    if core in ("float", "f32", "double", "f64"):
        return sz, "f"
    return sz, "u" if core.startswith("unsigned") or core.startswith("u") else "s"


def _tu_field_sites(fn: str, info: dict):
    """[(start, end, replacement, ident)] raw `*(T *)((char *)B + K)` accesses
    whose base is a typed struct pointer / struct variable in the TU's stabs
    and whose offset K holds exactly one scalar member of T's width."""
    sp = info["p"]
    out = []
    for st, en, T, B, ident, K in _raw_sites(fn):
        tk = _t_kind(T)
        if not tk:
            continue
        amp = "&" in B
        tid = info["vars"].get(ident, info["globals"].get(ident))
        if tid is None:
            continue
        _, n = sp.resolve(tid)
        if n is None:
            continue
        if amp:
            if n[0] not in ("struct", "union"):
                continue
            agg, pre = tid, ident
        else:
            if n[0] != "ptr":
                continue
            agg, pre = n[1], ident + "->"
        lv = sp.leaves(agg, K, tk[0])
        if len(lv) != 1:
            continue
        path, (sz, kind) = lv[0]
        if (kind == "f") != (tk[1] == "f"):
            continue
        acc = pre + path[1:] if not amp else pre + path
        if fn[:st].rstrip().endswith("&"):
            continue                                   # address taken: the type matters
        rest = fn[en:].lstrip()
        store = bool(re.match(r"(?:[-+*/%&|^]|<<|>>)?=(?!=)|\+\+|--", rest))
        if not store and (kind != tk[1] or tk[1] == "p"):
            acc = f"({T})({acc})" if tk[1] != "p" or kind != "p" else f"({T}){acc}"
        out.append((st, en, acc, ident))
    return out


def _tu_field_views(text: str, name: str, ctx, fam: str):
    """o509 InitDrill (11 tries): the TU's own struct fields instead of raw
    `*(T *)((char *)p + K)` casts (gcc 2.96 MEM_IN_STRUCT_P / alias sets move
    the scheduling). Field paths from the TU compiled with -gstabs."""
    ctx = ctx or {}
    loc = find_function(text, name)
    if not loc or not ctx.get("unit") or not _raw_sites(text[loc[0]:loc[2] + 1]):
        return
    s, b, e = loc
    fn = text[s:e + 1]
    info = stabs_info(ctx["unit"], ctx.get("tu", ""), name, text)
    sites = _tu_field_sites(fn, info) if info else []
    if sites:
        idents = sorted({x[3] for x in sites})
        groups = [(f"tu:{i}", [x for x in sites if x[3] == i]) for i in idents[:4]]
        if len(idents) > 1:
            groups.append(("tu:all", sites))
        for lab, grp in groups:
            nf = fn
            for st, en, acc, _ in sorted(grp, reverse=True):
                nf = nf[:st] + acc + nf[en:]
            yield f"{fam}:{lab}", text[:s] + nf + text[e + 1:]
    # m2c's `void *arg0` (o494 cmdStatGet, o499 cmdCamShake: every ov01/cmd
    # seed): retype it like the TU's matched functions type that parameter
    # (`ThinkProcess *proc`), then the same field rewrite
    raw_ids = {x[4] for x in _raw_sites(fn) if "&" not in x[3]}
    for k, (pst, pen, p) in enumerate(_params(fn)):
        pm = re.fullmatch(r"\s*(?:const\s+)?void\s*\*\s*(\w+)\s*", p)
        if not pm or pm.group(1) not in raw_ids:
            continue
        for T in _sibling_param_types(ctx["unit"], ctx.get("tu", ""), k)[:2]:
            fn2 = fn[:pst] + f"{T} *{pm.group(1)}" + fn[pen:]
            text2 = text[:s] + fn2 + text[e + 1:]
            info2 = stabs_info(ctx["unit"], ctx.get("tu", ""), name, text2)
            sites2 = [x for x in (_tu_field_sites(fn2, info2) if info2 else []) if x[3] == pm.group(1)]
            if not sites2:
                continue
            nf = fn2
            for st, en, acc, _ in sorted(sites2, reverse=True):
                nf = nf[:st] + acc + nf[en:]
            yield f"{fam}:tu:retype:{T.replace(' ', '_')}", text[:s] + nf + text[e + 1:]


_SIB_PT: dict = {}


def _sibling_param_types(unit: str, tu: str, k: int) -> list[str]:
    """Pointee types of the K-th parameter of the TU's C function definitions,
    most frequent first (>= 2 uses, struct-like names only)."""
    key = (unit, tu)
    if key not in _SIB_PT:
        cnt: dict = {}
        try:
            src = (ROOT / "src" / unit / f"{tu}.c").read_text(errors="replace")
        except OSError:
            src = ""
        for m in re.finditer(r"^[A-Za-z_][\w \t*]*?\b\w+\s*\(([^;{}()]*)\)\s*\{", src, re.M):
            for i, p in enumerate(split_top(m.group(1))):
                pm = re.fullmatch(r"\s*(?:const\s+)?((?:struct\s+)?[A-Za-z_]\w*)\s*\*\s*\w+\s*", p)
                if pm and pm.group(1) not in _SCALAR_SIZE and pm.group(1) not in ("void", "char", "signed", "unsigned"):
                    cnt.setdefault(i, {}).setdefault(pm.group(1), 0)
                    cnt[i][pm.group(1)] += 1
        _SIB_PT[key] = {i: [t for t, n in sorted(d.items(), key=lambda x: -x[1]) if n >= 2] for i, d in cnt.items()}
    return _SIB_PT[key].get(k, [])


_RELOC_SYM = re.compile(r"\[26 ([\w.$]+)\]")


def tailcall_shape(rows) -> list[str]:
    """Callees the target reaches with `jal S` more often, where ours
    tail-jumps `j S` more often (gcc's sibcall: o507 checkTalkPoint)."""
    from collections import Counter
    dr = diff_rows(rows)
    cnt = {"t": Counter(), "c": Counter()}
    for a, _, b in dr:
        for side, x in (("t", a), ("c", b)):
            m = _RELOC_SYM.search(x) if x else None
            if m and _mn(x) in ("jal", "j"):
                cnt[side][(_mn(x), m.group(1))] += 1
    t, c = cnt["t"], cnt["c"]
    syms = {S for (_, S) in t} | {S for (_, S) in c}
    return sorted(S for S in syms if t[("jal", S)] > c[("jal", S)] and c[("j", S)] > t[("j", S)])


def t_no_tailcall(fn, ctx=None):
    """o507 checkTalkPoint (0.70 -> 0.9775, matched once callee_void ran):
    the target calls `jal f` + epilogue where ours sibcalls `j f`; a
    `do { f(..); } while (0);` around the call keeps gcc 2.96 from turning it
    into a tail jump. Each call statement of a tailcall_shape callee alone
    (plus its following `return;`), then all."""
    syms = (ctx or {}).get("no_tailcall")
    if not syms:
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    sites = []
    for S in syms:
        pat = re.compile(r"^(?P<ind>\s*)(?P<st>(?:return\s+)?(?:\([\w\s*]+\)\s*)?" + re.escape(S) + r"\s*\(.*\)\s*;)\s*$")
        for i, ln in enumerate(lines):
            m = pat.match(ln)
            if m and ln.count("(") == ln.count(")"):
                sites.append((i, m.group("ind"), m.group("st")))
    for i, ind, st in sites:
        nl = list(lines)
        nl[i] = f"{ind}do {{ {st} }} while (0);"
        yield f"no_tailcall:L{i}", head + "\n".join(nl) + tail
        j = _next_stmt(lines, i)
        if j < len(lines) and re.match(r"^\s*return\s*;\s*$", lines[j]):
            nl = list(lines)
            nl[i] = f"{ind}do {{ {st} return; }} while (0);"
            nl[j] = None
            yield f"no_tailcall:ret:L{i}", head + "\n".join(x for x in nl if x is not None) + tail
    if len(sites) > 1:
        nl = list(lines)
        for i, ind, st in sites:
            nl[i] = f"{ind}do {{ {st} }} while (0);"
        yield "no_tailcall:all", head + "\n".join(nl) + tail


_MATH_OP = {"fabsf": "abs.s", "fabs": "abs.d", "sqrtf": "sqrt.s", "sqrt": "sqrt.d"}


def builtin_math_shape(rows) -> list[str]:
    """Math calls ours makes (`jal fabsf`) where the target has more of the
    matching FPU op (abs.s / sqrt.s): o508 Check_InsideFan."""
    dr = diff_rows(rows)
    calls = set()
    cnt = {"t": {}, "c": {}}
    for a, _, b in dr:
        for side, x in (("t", a), ("c", b)):
            if not x:
                continue
            mn = _mn(x)
            cnt[side][mn] = cnt[side].get(mn, 0) + 1
            m = _RELOC_SYM.search(x)
            if side == "c" and m and mn in ("jal", "j") and m.group(1) in _MATH_OP:
                calls.add(m.group(1))
    return sorted(f for f in calls if cnt["t"].get(_MATH_OP[f], 0) > cnt["c"].get(_MATH_OP[f], 0))


def t_builtin_math(fn, ctx=None):
    """o508 Check_InsideFan (0.73 -> match): under -fno-builtin a plain
    `fabsf(x)` is a call (undeclared even: implicit int -> fptodp/cvt);
    `__builtin_fabsf` / `__builtin_sqrtf` give abs.s / sqrt.s. Gated on
    builtin_math_shape (ours `jal fabsf`, the target the FPU op)."""
    fs = (ctx or {}).get("builtin_math")
    if not fs:
        return
    head, inner, tail = _split_fn(fn)
    hit = 0
    for f in fs:
        ni = re.sub(r"(?<![\w.>])" + re.escape(f) + r"(\s*\()", r"__builtin_" + f + r"\1", inner)
        if ni != inner:
            hit += 1
            yield f"builtin_math:{f}", head + ni + tail
    if hit > 1:
        ni = re.sub(r"(?<![\w.>])(" + "|".join(map(re.escape, fs)) + r")(\s*\()", r"__builtin_\1\2", inner)
        yield "builtin_math:all", head + ni + tail


_SET_LAST = re.compile(r"^(?P<ind>\s*)(?P<x>[A-Za-z_]\w*(?:\s*(?:->|\.)\s*\w+|\s*\[[^\[\]]+\])*)\s*=(?!=)\s*(?P<e>[^;]+);\s*$")


def t_shared_case_label(fn):
    """o505 cmdRmath1 (14 tries): switch cases that each end `X = e; break;`
    for the same X -> `_t = e; goto _set;` with one shared `_set: X = _t;
    break;` inside a case (single conversion point: per-case int, one
    sign-extend join). Label in the last such case first, then each other."""
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in re.finditer(r"\bswitch\s*\(", inner):
        c = match_brace(inner, m.end() - 1)
        if c < 0:
            continue
        mm = re.compile(r"\s*\{").match(inner, c + 1)
        if not mm:
            continue
        bo = mm.end() - 1
        bc = match_brace(inner, bo)
        if bc < 0:
            continue
        body = inner[bo + 1:bc]
        sg = _switch_groups(body)
        if not sg:
            continue
        lead, groups = sg
        by_x: dict = {}
        for gi, (labs, st) in enumerate(groups):
            st2 = [x for x in st if x.strip()]
            if len(st2) < 2 or not re.match(r"^\s*break\s*;\s*$", st2[-1]):
                continue
            sm = _SET_LAST.match(st2[-2])
            if not sm:
                continue
            x = re.sub(r"\s+", "", sm.group("x"))
            by_x.setdefault(x, []).append(gi)
        for x, gis in by_x.items():
            if len(gis) < 2:
                continue
            t = _fresh(fn, "_t")
            lab = _label_name(fn, "set")
            for li in [gis[-1]] + gis[:-1][::-1]:
                ng = []
                for gi, (labs, st) in enumerate(groups):
                    st = list(st)
                    if gi in gis:
                        idx = [k for k, y in enumerate(st) if y.strip()]
                        si, bi = idx[-2], idx[-1]
                        sm = _SET_LAST.match(st[si])
                        st[si] = f"{sm.group('ind')}{t} = {sm.group('e').strip()};"
                        if gi == li:
                            bind = re.match(r"\s*", st[bi]).group(0)
                            st[bi:bi + 1] = [f"{lab}:", f"{bind}{sm.group('x')} = {t};", st[bi]]
                        else:
                            st[bi] = re.sub(r"break\s*;", f"goto {lab};", st[bi])
                    ng.append((labs, st))
                nb = "\n".join(lead + [y for labs, st in ng for y in labs + st])
                ni = inner[:bo + 1] + nb + inner[bc:]
                lines = ni.split("\n")
                first = _decl_end(ni)
                at = _last_decl(lines, first)
                lines.insert(at, f"{_decl_ind(lines, first)}int {t};")
                yield f"shared_case_label:{x}:g{li}", head + "\n".join(lines) + tail
                n += 1
                if n >= 8:
                    return


_SHIFT_PAIR = re.compile(r"^(sll|dsll)$")


def shift_pair_shape(rows) -> list[list[int]]:
    """Target `sll r,x,A` + `sra r,r,B` (A > B, A >= 16: a short/char index
    scaled by 2^(A-B)) that ours lacks: o497 cmdAtkset `(idx << 17) >> 15`."""
    c = _f9_cols(rows)
    if not c:
        return []

    def pairs(col):
        out = {}
        for i, x in enumerate(col):
            mn, o, rel = _insn_ops(x)
            if mn != "sll" or rel or len(o) != 3:
                continue
            A = _ival(o[2])
            for y in col[i + 1:i + 4]:
                m2, o2, r2 = _insn_ops(y)
                if m2 == "sra" and len(o2) == 3 and o2[1] == o[0]:
                    B = _ival(o2[2])
                    if A is not None and B is not None and A > B and A >= 16:
                        out[(A, B)] = out.get((A, B), 0) + 1
                    break
        return out
    t, o = pairs(c[0]), pairs(c[1])
    return [[A, B] for (A, B), k in sorted(t.items()) if k > o.get((A, B), 0)]


_NARROW_CAST = r"\(\s*(?P<T>short|s16|signed\s+short|char|s8|signed\s+char)\s*\)"


def t_shift_index(fn, ctx=None):
    """o497 cmdAtkset: a sign-extended scaled index the target computes as
    `sll A; sra B` -> the explicit shift form `((e << 16) >> (16 - n))` for
    `(short)(e) * 2^n` / `(short)(e) << n` (24 for char), and the shape's own
    `(v << A) >> B` for `(short)(v * 2^m) * 2^n` style operands. Gated on
    shift_pair_shape."""
    shp = (ctx or {}).get("shift_index")
    if not shp:
        return
    head, inner, tail = _split_fn(fn)
    pat = re.compile(_NARROW_CAST + r"\s*(?P<E>\((?:[^()]|\([^()]*\))*\)|[A-Za-z_]\w*)\s*(?P<op>\*|<<)\s*"
                     r"(?P<K>0[xX][0-9a-fA-F]+|\d+)(?![\w.])")
    n = 0
    for m in pat.finditer(inner):
        k = _ival(m.group("K"))
        if not k:
            continue
        sh = k if m.group("op") == "<<" else (k.bit_length() - 1 if k & (k - 1) == 0 else None)
        if sh is None:
            continue
        w = 16 if m.group("T").replace("signed", "").strip() in ("short", "s16") else 24
        E = m.group("E")
        e = E[1:-1] if E.startswith("(") else E
        forms = [f"((({e}) << {w}) >> {w - sh})"]
        em = re.fullmatch(r"\s*([A-Za-z_]\w*)\s*(?:\*\s*(\d+)|<<\s*(\d+))\s*", e)
        if em:
            inner_sh = int(em.group(3)) if em.group(3) else (int(em.group(2)).bit_length() - 1
                                                              if int(em.group(2)) & (int(em.group(2)) - 1) == 0 else None)
            if inner_sh is not None:
                forms.append(f"(({em.group(1)} << {w + inner_sh}) >> {w - sh})")
        elif re.fullmatch(r"[A-Za-z_]\w*", e):
            forms.append(f"(({e} << {w}) >> {w - sh})")
        seen = set()
        for f in forms:
            if f in seen:
                continue
            seen.add(f)
            yield f"shift_index:{m.start()}:{len(seen)}", head + inner[:m.start()] + f + inner[m.end():] + tail
            n += 1
        if n >= 8:
            return


def t_param_scale_reassign(fn):
    """drawVector2 (main/db_light_write): a parameter/local only ever used
    scaled by a power of two (`&col[arg2 * 2]`, `p[i << 1]`) where gcc folds
    the scale and the element size into one `sll 3` while the target has two
    separate shifts -> reassign it first (`arg2 = arg2 * 2;`) and index with
    the plain variable. Ungated: fires only when every use of the variable
    after the declarations is the same scaled form."""
    head, inner, tail = _split_fn(fn)
    items, first, lines = _decl_block(inner)
    hdr_params = re.findall(r"([A-Za-z_]\w*)\s*(?:,|\)\s*\{)", head)
    names = [n for n in hdr_params if n not in KEYWORDS and n != "void"] + \
        [decl_name(d) for ty, d in items if "[" not in d and "*" not in d.split("=")[0]]
    rest = "\n".join(lines[first:])
    n = 0
    for v in dict.fromkeys(names):
        ev = re.escape(v)
        pat = re.compile(r"(?<![\w.>])" + ev + r"\s*(?P<op>\*|<<)\s*(?P<K>0[xX][0-9a-fA-F]+|\d+)(?![\w.])")
        uses = list(re.finditer(r"(?<![\w.>])" + ev + r"\b", rest))
        sc = list(pat.finditer(rest))
        if not sc or len(sc) != len(uses):
            continue
        if any(rest[:m.start()].rstrip()[-1:] in ("/", "%", "*", ")", "<", ">") for m in sc):
            continue                      # `x / v * 2`, `(short)v * 2`: not `(v * 2)`
        ks = {(m.group("op"), _ival(m.group("K"))) for m in sc}
        if len(ks) != 1:
            continue
        op, k = next(iter(ks))
        if not k or (op == "*" and (k < 2 or k & (k - 1))):
            continue
        if any(_assigns(ln, v) for ln in lines[first:]):
            continue
        new_rest = pat.sub(v, rest)
        sind = re.match(r"\s*", lines[first]).group(0) if first < len(lines) else "    "
        stmt_forms = [f"{v} = {v} {op} {m_k};" for m_k in [sc[0].group("K")]]
        stmt_forms.append(f"{v} {op}= {sc[0].group('K')};")
        nl_rest = new_rest.split("\n")
        fu = next((j for j, ln in enumerate(nl_rest) if re.search(r"(?<![\w.>])" + ev + r"\b", ln)), 0)
        for si, st in enumerate(stmt_forms):
            for pos in sorted({0, fu}):
                body = list(nl_rest)
                pind = re.match(r"\s*", body[pos]).group(0) if pos < len(body) else sind
                body.insert(pos, f"{pind}{st}")
                yield f"param_scale_reassign:{v}:{si}:{'top' if pos == 0 else 'use'}", \
                    head + "\n".join(lines[:first] + body) + tail
                n += 1
        if n >= 8:
            return


_PROTOS: dict = {}


def _proto_of(text: str, f: str) -> str | None:
    """`RET f(params);` from the body file, else build/auto_decomp/protos.json."""
    m = re.search(r"^[ \t]*(?:extern\s+)?(?P<r>[A-Za-z_][\w \t*]*?)\s*\b" + re.escape(f)
                  + r"\s*\((?P<p>[^;{}()]*)\)\s*;", text, re.M)
    if m:
        return f"{m.group('r')} {f}({m.group('p')});"
    if not _PROTOS:
        try:
            _PROTOS.update(json.loads((ROOT / "build/auto_decomp/protos.json").read_text()))
        except (OSError, ValueError):
            _PROTOS["?"] = ""
    return _PROTOS.get(f)


def _call_widen(text: str, name: str, ctx):
    """o497 cmdAtkset: `thinkAdrGet(short)` sign-extends its argument at every
    call (`sll a0,16; sra a0,16`) where the target passes the int raw ->
    call through `((int (*)(int))thinkAdrGet)(x)`. Callees with a
    short/char parameter (body prototype or protos.json); gated on
    param_widen's `widen` shape (an a-reg extend only on our side)."""
    sh = (ctx or {}).get("param_widen") or []
    narrow = list((ctx or {}).get("narrow_calls") or [])
    if not narrow and not any(x.get("dir") == "widen" for x in sh):
        return
    loc = find_function(text, name)
    if not loc:
        return
    s, b, e = loc
    fn = text[s:e + 1]
    head, inner, tail = _split_fn(fn)
    n = 0
    called = {m.group(1) for m in _CALL_IN.finditer(inner)} - KEYWORDS - {name}
    # fold15: callees try_fn saw extended at more call sites than the target first
    for f in [x for x in narrow if x in called] + sorted(called - set(narrow)):
        pr = _proto_of(text, f)
        pm = re.match(r"^\s*(?:extern\s+)?(?P<r>.*?)\s*\b" + re.escape(f) + r"\s*\((?P<p>.*)\)\s*;\s*$", pr or "")
        if not pm:
            continue
        ps = [p.strip() for p in split_top(pm.group("p"))] if pm.group("p").strip() not in ("", "void") else []
        wide, hit = [], False
        for p in ps:
            pt = _param_type(p)
            core = pt[1] if pt else p
            core = re.sub(r"\s+", " ", core).strip()
            if core in _NARROW and "*" not in p:
                wide.append("int")
                hit = True
            else:
                wide.append(re.sub(r"\s+\w+$", "", p) if pt else p)
        if not hit:
            continue
        cast = f"(({pm.group('r').strip()} (*)({', '.join(wide)})){f})"
        pre = cast[:-len(f) - 1]
        sites = [m for m in re.finditer(r"(?<![\w.>])" + re.escape(f) + r"(?=\s*\()", inner)
                 if not inner[:m.start()].endswith(pre)]
        if not sites:
            continue
        ni = inner
        for m in reversed(sites):
            ni = ni[:m.start()] + cast + ni[m.end():]
        yield f"param_widen:call:{f}", text[:s] + head + ni + tail + text[e + 1:]
        n += 1
        if len(sites) > 1:
            # fold15 (o574 cmdAtktblSearch: the 6th thinkAdrGet call needed it): every site, up to 8
            for k, m in enumerate(sites[:8]):
                yield f"param_widen:call:{f}:{k}", text[:s] + head + inner[:m.start()] + cast \
                    + inner[m.end():] + tail + text[e + 1:]
        if n >= 4:
            return


# ---- fold11 (o510-o523) ------------------------------------------------------

_SIDE_EFFECT = re.compile(r"[A-Za-z_]\w*\s*\(|\+\+|--|(?<![=!<>])=(?!=)")


def _aggregate_lines(inner: str) -> set[int]:
    """Line indexes of INNER inside a struct/union/enum body or a brace
    initializer (member declarations, not locals)."""
    out, stack, line, last = set(), [], 0, 0
    for i, c in enumerate(inner):
        if c == "\n":
            line += 1
            if any(stack):
                out.add(line)
        elif c == "{":
            pre = inner[last:i]
            stack.append(bool(re.search(r"\b(struct|union|enum)\b|=\s*$", pre)) or any(stack))
            last = i + 1
        elif c == "}":
            if stack:
                stack.pop()
            last = i + 1
        elif c == ";":
            last = i + 1
    return out


def dead_locals(fn: str) -> list[tuple[int, str, str]]:
    """[(line index in the body, declarator, name)] of locals declared at the
    start of a block (function or inner) whose name never appears anywhere
    else in the function, initializer absent or free of calls / assignments /
    ++ --. A name declared twice (shadowing) is left alone."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    agg = _aggregate_lines(inner)
    out = []
    prev = "{"
    for i, ln in enumerate(lines):
        st = ln.strip()
        if not st:
            continue
        p = parse_decl(re.sub(r"\s*__attribute__\s*\(\(.*\)\)", "", ln))
        at_top = prev.endswith("{") or prev == "decl"
        if p and at_top and i not in agg and not re.match(r"\s*(return|goto|case|else)\b", ln):
            for d in p[2]:
                nm = decl_name(d)
                init = d.split("=", 1)[1] if "=" in d else ""
                if init and _SIDE_EFFECT.search(init):
                    continue
                if len(re.findall(r"(?<![\w.>])" + re.escape(nm) + r"\b", fn)) == 1:
                    out.append((i, d, nm))
            prev = "decl"
        else:
            prev = st
    return out


def _drop_decls(fn: str, drop: list[tuple[int, str, str]]) -> str:
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    by: dict = {}
    for i, d, nm in drop:
        by.setdefault(i, set()).add(nm)
    for i in sorted(by, reverse=True):
        ln = lines[i]
        p = parse_decl(re.sub(r"\s*__attribute__\s*\(\(.*\)\)", "", ln))
        if not p:
            continue
        ind, ty, decls = p
        keep = [d for d in decls if decl_name(d) not in by[i]]
        if not keep:
            del lines[i]
        elif "__attribute__" not in ln:
            lines[i] = f"{ind}{ty} {', '.join(keep)};"
    return head + "\n".join(lines) + tail


def t_drop_dead_locals(fn):
    """o516 subTreeLineDraw_type_2: declared-but-never-used locals (permuter /
    m2c leftovers: `TLV t; TLV u, v;` aggregates still get stack slots in gcc
    2.96, frame 96 -> 48) removed: all at once first, then one at a time.
    Also the sweep's cleanup pre-pass (sweep_one)."""
    dl = dead_locals(fn)
    if not dl:
        return
    yield "drop_dead_locals:all", _drop_decls(fn, dl)
    if len(dl) > 1:
        for k, x in enumerate(dl[:6]):
            yield f"drop_dead_locals:{x[2]}", _drop_decls(fn, [x])


_W64_CAST = re.compile(r"^\(\s*(u64|s64|unsigned\s+long\s+long|long\s+long|unsigned\s+long\s+long\s+int)\s*\)\s*")


def _split_or(e: str) -> list[str]:
    """Top-level `|` operands of E (not `||`)."""
    out, d, cur, i = [], 0, "", 0
    while i < len(e):
        c = e[i]
        if c in "([{":
            d += 1
        elif c in ")]}":
            d -= 1
        if c == "|" and d == 0 and e[i + 1:i + 2] != "|" and e[i - 1:i] != "|":
            out.append(cur)
            cur = ""
        else:
            cur += c
        i += 1
    out.append(cur)
    return [x.strip() for x in out]


def _strip_parens(e: str) -> str:
    e = e.strip()
    while e.startswith("(") and match_brace(e, 0) == len(e) - 1:
        e = e[1:-1].strip()
    return e


def _int_lit(e: str):
    m = re.fullmatch(r"(0[xX][0-9a-fA-F]+|\d+)[uUlL]*", _strip_parens(e))
    return int(m.group(1), 0) if m else None


def t_widen_before_or(fn):
    """o523 endBackTexDraw: `*(u64 *)p = (u64)(0x24020000 | (w << 5)) |
    0x2000000640000000;` - the shift and the low constant ORed in 32 bits,
    then widened, then the high constant (gcc keeps `sll; or; dsll32`-free
    code). From a 64-bit OR of shift terms and constants (64-bit casts on the
    terms or a constant > 32 bits): the low-half OR in 32 bits inside one
    `(u64)` cast, high half outside; also the whole constant outside, and
    terms-first order. Ungated (needs a constant > 0xFFFFFFFF or a 64-bit
    cast term next to a `<<`)."""
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in re.finditer(r"(?<![=!<>+\-*/%&|^])=(?!=)\s*(?P<e>[^;]+);", inner):
        e = m.group("e")
        ops = _split_or(_strip_parens(e))
        if len(ops) < 2:
            continue
        consts, terms, wide = 0, [], False
        cast = None
        for o in ops:
            k = _int_lit(o)
            if k is not None:
                consts |= k
                continue
            oo = _strip_parens(o)
            sub = _split_or(oo) if not _W64_CAST.match(oo) else [oo]
            if len(sub) > 1:              # nested (u64)(a | b) group: flatten
                for x in sub:
                    kx = _int_lit(x)
                    if kx is not None:
                        consts |= kx
                    else:
                        cm = _W64_CAST.match(_strip_parens(x))
                        terms.append(_strip_parens(x)[cm.end():] if cm else _strip_parens(x))
                        wide |= bool(cm)
                        cast = cast or (cm.group(1) if cm else None)
                continue
            cm = _W64_CAST.match(oo)
            if cm:
                wide = True
                cast = cast or cm.group(1)
                oo = oo[cm.end():]
                ci = _strip_parens(oo)
                k2 = _int_lit(ci)
                if k2 is not None:
                    consts |= k2
                    continue
                oo = ci
            terms.append(oo)
        if not terms or not any("<<" in t for t in terms) or not (wide or consts > 0xFFFFFFFF):
            continue
        cast = cast or ("u64" if re.search(r"\bu64\b", fn) else "unsigned long long")
        tj = " | ".join(f"({t})" if not re.fullmatch(r"[\w.\->\[\]]+", t) else t for t in terms)
        forms = []
        # split points: the whole low word, then below each set bit of it (o523
        # kept bit 30 with the high constant: 0x24020000 | .. , 0x2000000640000000)
        los = []
        for b in [32] + [b for b in range(31, 15, -1) if consts >> b & 1]:
            lo = consts & ((1 << b) - 1)
            if lo not in los:
                los.append(lo)
        for k, lo in enumerate(los):
            hi = consts & ~lo
            inner32 = f"{lo:#x} | {tj}" if lo else tj
            forms.append(f"({cast})({inner32}) | {hi:#x}" if hi else f"({cast})({inner32})")
            if k == 0 and lo:
                forms.append(f"({cast})({tj} | {lo:#x}) | {hi:#x}" if hi else f"({cast})({tj} | {lo:#x})")
        if consts:
            forms.append(f"({cast})({tj}) | {consts:#x}")
        seen = set()
        for f in forms:
            if not f or f in seen or re.sub(r"\s+", "", f) == re.sub(r"\s+", "", e):
                continue
            seen.add(f)
            yield (f"widen_before_or:{m.start()}:{len(seen)}",
                   head + inner[:m.start("e")] + f + inner[m.end("e"):] + tail)
            n += 1
        if n >= 9:
            return


_INC_STMT = re.compile(r"^\s*(?:(?P<a>[A-Za-z_]\w*)\s*(?:\+\+|--|[+-]=\s*[^;]+)|(?:\+\+|--)\s*(?P<b>[A-Za-z_]\w*))\s*;\s*$")


def t_ptr_lifetime(fn):
    """o515 endPrintDirectRibbon: where a loop steps its pointer decides how
    long the old value lives (copy propagation into an earlier loop, s0/s1
    swap): a `p++;` / `p += k;` statement moved to every other position of its
    block among the neighbouring plain statements that do not mention `p`
    (multi-step moves stmtswap cannot reach in one round). Pointer params /
    locals only; ungated, <= 6 per function."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    ptrs = set(_ptr_vars(fn))

    def plain(ln):
        st = ln.strip()
        return st.endswith(";") and "{" not in st and "}" not in st and not re.match(
            r"(return|break|continue|goto|case|default|while|if|for|do|else|switch)\b", st) \
            and not st.endswith(":") and not parse_decl(ln)
    for i, ln in enumerate(lines):
        m = _INC_STMT.match(ln)
        if not m:
            continue
        v = m.group("a") or m.group("b")
        if v not in ptrs:
            continue
        ind = re.match(r"\s*", ln).group(0)
        mention = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
        lo = i
        while lo - 1 >= 0 and plain(lines[lo - 1]) and re.match(r"\s*", lines[lo - 1]).group(0) == ind \
                and not mention.search(lines[lo - 1]):
            lo -= 1
        hi = i
        while hi + 1 < len(lines) and plain(lines[hi + 1]) and re.match(r"\s*", lines[hi + 1]).group(0) == ind \
                and not mention.search(lines[hi + 1]):
            hi += 1
        rest = lines[:i] + lines[i + 1:]
        for pos in list(range(lo, i - 1)) + list(range(i + 2, hi + 1)):
            at = pos if pos < i else pos           # insert index in REST
            nl = rest[:at] + [ln] + rest[at:]
            yield f"ptr_lifetime:{v}:L{i}->{pos}", head + "\n".join(nl) + tail
            n += 1
            if n >= 6:
                return


# ---- fold12 families (o524-o533) ---------------------------------------------

_LIS_HDR = re.compile(r"^(?P<ind>\s*)for\s*\(\s*(?P<i>[A-Za-z_]\w*)\s*=\s*(?P<a>-?\d+)\s*;\s*(?P=i)\s*"
                      r"(?P<op><=|<|!=)\s*(?P<b>[^;]+?)\s*;\s*(?:(?P=i)\s*\+\+|\+\+\s*(?P=i)|(?P=i)\s*\+=\s*1)\s*\)"
                      r"\s*(?P<br>\{)?\s*$")


def _lis_off(v: int) -> str:
    return "" if v == 0 else (f" + {v}" if v > 0 else f" - {-v}")


def _lis_shift(ln: str, I: str, d: int) -> str:
    """LN with every use of the counter I rewritten for the new counter I' = I + d
    (I = I' - d): `I + K` -> `I + (K - d)`, a bare I -> `I - d`."""
    pat = re.compile(r"(?<![\w.>])" + re.escape(I) + r"\b(?!\s*(?:\+\+|--|\(|->|\.|\[))"
                     r"(?P<off>\s*(?P<sg>[+-])\s*(?P<k>0[xX][0-9a-fA-F]+|\d+)\b(?!\s*[*/%\[(.]|\s*<<|\s*>>))?")
    out, last = [], 0
    for m in pat.finditer(ln):
        pre = ln[:m.start()].rstrip()
        k = 0
        absorb = m.group("off") and not re.search(r"(?:[*/%~\-]|<<|>>)$", pre)
        if absorb:
            k = int(m.group("k"), 0) * (1 if m.group("sg") == "+" else -1)
            end = m.end()
        else:
            end = m.end() - len(m.group("off") or "")
        r = k - d
        e = I + _lis_off(r)
        post = ln[end:].lstrip()
        if r != 0 and not (pre[-1:] in ("[", "(", ",", "=") and post[:1] in ("]", ")", ",", ";")):
            e = f"({e})"
        out.append(ln[last:m.start()] + e)
        last = end
    return "".join(out) + ln[last:]


def t_loop_index_shift(fn):
    """o527 MenuItemLineMain: the counter's base decides which addresses gcc
    strength-reduces into givs: `for (i = 1; i < 5; i++) { f(&a[i], &t[i - 1]); }`
    matched where `for (i = 0; i < 4; i++) { f(&a[i + 1], &t[i]); }` did not.
    For a `for (I = A; I <op> B; I++)` loop whose body indexes with `I + K` /
    `I - K` (literal K) and does not write I or use it after the loop: the
    counter shifted by each such K (bounds A + K, B + K; every use rewritten).
    Ungated text pattern, <= 2 shifts per loop, <= 6 per function."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    n = 0
    for i, ln in enumerate(lines):
        m = _LIS_HDR.match(ln)
        if not m:
            continue
        I = m.group("i")
        lb = _loop_body(lines, dep, i)
        if not lb:
            continue
        b0, end, braced = lb
        if not braced and not m.group("br") and b0 < len(lines):
            end = b0 + 1
        body = lines[b0:end]
        if any(_assigns(x, I) for x in body):
            continue
        after = "\n".join(lines[end:])
        nxt = re.search(r"(?<![\w.>])" + re.escape(I) + r"\b(?P<rest>[^;]*)", after)
        if nxt and not re.match(r"\s*=(?!=)", nxt.group("rest")):
            continue                      # I's final value is used after the loop
        offs = []
        for x in body:
            for mm in re.finditer(r"(?<![\w.>])" + re.escape(I) + r"\s*([+-])\s*(0[xX][0-9a-fA-F]+|\d+)\b"
                                  r"(?!\s*[*/%\[(.])", x):
                pre = x[:mm.start()].rstrip()
                if re.search(r"(?:[*/%~\-]|<<|>>)$", pre):
                    continue
                v = int(mm.group(2), 0) * (1 if mm.group(1) == "+" else -1)
                if v and v not in offs and abs(v) <= 16:
                    offs.append(v)
        if not offs:
            continue
        b = m.group("b").strip()
        for d in offs[:2]:
            a2 = int(m.group("a")) + d
            b2 = str(int(b, 0) + d) if re.fullmatch(r"-?(?:0[xX][0-9a-fA-F]+|\d+)", b) else f"{b}{_lis_off(d)}"
            hdr = ln[:m.start("a")] + str(a2) + ln[m.end("a"):m.start("b")] + b2 + ln[m.end("b"):]
            nl = list(lines)
            nl[i] = hdr
            for k in range(b0, end):
                nl[k] = _lis_shift(lines[k], I, d)
            yield f"loop_index_shift:{I}:L{i}:{d:+d}", head + "\n".join(nl) + tail
            n += 1
            if n >= 6:
                return


_ISA_STEP = re.compile(r"^(?P<ind>\s*)(?:(?P<v>[A-Za-z_]\w*)\s*\+=\s*(?P<e>[^;]+?)|(?P<v2>[A-Za-z_]\w*)\s*=\s*(?P=v2)\s*\+"
                       r"\s*(?P<e2>[^;]+?)|(?P<v3>[A-Za-z_]\w*)\s*\+\+|\+\+\s*(?P<v4>[A-Za-z_]\w*))\s*;\s*$")


def _in_loop(lines, dep, i) -> bool:
    d = dep[i]
    for k in range(i - 1, -1, -1):
        if dep[k] < d:
            if re.match(r"^\s*(?:\}\s*)?(for|while|do)\b", lines[k]) or (
                    lines[k].strip() == "{" and k and re.match(r"^\s*(for|while|do)\b", lines[k - 1])):
                return True
            d = dep[k]
    return False


def t_inline_sum_arg(fn):
    """o529 ACT_loadResource: `sid += 0x10000; s = f(-2, sid); g(s, sid);` ->
    `s = f(-2, sid + 0x10000); g(s, sid + 0x10000);` (gcc keeps sid and the sum
    in other registers: s0/s1 swap fixed). A local's `v += E;` / `v = v + E;` /
    `v++;` whose value is only read by the following statements of its block
    (dead after them, not in a loop, E's names unchanged) -> the sum written at
    each use, the step dropped; and the reverse (`v + K` used >= 2 times by
    consecutive statements, v dead after -> `v += K;` before them). Ungated."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    n = 0
    for i, ln in enumerate(lines):
        m = _ISA_STEP.match(ln)
        if not m:
            continue
        v = m.group("v") or m.group("v2") or m.group("v3") or m.group("v4")
        E = (m.group("e") or m.group("e2") or "1").strip()
        if not re.fullmatch(r"[\w\s+\-*<>|&^()]+", E) or _CALL_RE.search(E) or "++" in E or "--" in E:
            continue
        if _in_loop(lines, dep, i):
            continue
        mention = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
        names = set(re.findall(r"[A-Za-z_]\w*", E)) - {v}
        uses, j, ok = [], i + 1, True
        while j < len(lines) and dep[j] >= dep[i]:
            if dep[j] == dep[i] and lines[j].lstrip().startswith("}"):
                break
            x = lines[j]
            if mention.search(x):
                if re.match(r"^\s*" + re.escape(v) + r"\s*=(?!=)", x) and not mention.search(x.split("=", 1)[1]):
                    break                 # plain reassignment: dead from here
                if _assigns(x, v) or re.search(r"\b(if|while|for|switch|return)\b", x) and dep[j] == dep[i] \
                        and not x.rstrip().endswith(";"):
                    ok = False
                    break
                uses.append(j)
            if any(_assigns(x, nm) for nm in names):
                ok = False
                break
            j += 1
        if not ok or not uses:
            continue
        rest = "\n".join(lines[j:])
        nx = mention.search(rest)
        if nx and not re.match(r"\s*=(?!=)", rest[nx.end():]):
            continue
        if parse_decl(ln) is not None:
            continue
        sumx = f"{v} + {E}" if re.fullmatch(r"[\w.]+", E) else f"{v} + ({E})"
        nl = list(lines)
        for k in uses:
            nl[k] = _subst_var(lines[k], v, sumx)
        del nl[i]
        yield f"inline_sum_arg:{v}:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 4:
            return
    # reverse: `f(v + K); g(v + K);` -> `v += K; f(v); g(v);`
    for i, ln in enumerate(lines):
        for mm in re.finditer(r"(?<![\w.>*/\-])([A-Za-z_]\w*)\s*\+\s*(0[xX][0-9a-fA-F]+|\d+)\b(?!\s*[*/%\[(.])", ln):
            v, K = mm.group(1), mm.group(2)
            if v in KEYWORDS or not ln.rstrip().endswith(";") or parse_decl(ln) is not None:
                continue
            pat = re.compile(r"(?<![\w.>*/\-])" + re.escape(v) + r"\s*\+\s*" + re.escape(K) + r"\b(?!\s*[*/%\[(.])")
            mention = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
            if _in_loop(lines, dep, i) or i and pat.search(lines[i - 1]):
                continue
            j, uses = i, []
            while j < len(lines) and dep[j] == dep[i] and lines[j].rstrip().endswith(";") and pat.search(lines[j]) \
                    and len(mention.findall(lines[j])) == len(pat.findall(lines[j])) and not _assigns(lines[j], v):
                uses.append(j)
                j += 1
            if len(uses) + sum(len(pat.findall(lines[k])) - 1 for k in uses) < 2:
                continue
            rest = "\n".join(lines[j:])
            nx = mention.search(rest)
            if nx and not re.match(r"\s*=(?!=)", rest[nx.end():]):
                continue
            ind = re.match(r"\s*", ln).group(0)
            nl = list(lines)
            for k in uses:
                nl[k] = pat.sub(v, lines[k])
            nl.insert(i, f"{ind}{v} += {K};")
            yield f"inline_sum_arg:rev:{v}:L{i}", head + "\n".join(nl) + tail
            n += 1
            if n >= 6:
                return
            break


_INL_DEF = re.compile(r"(?m)^[ \t]*(?:static\s+inline|inline\s+static|static\s+__inline__|__inline__\s+static|"
                      r"static\s+__inline|__inline\s+static)\b[^;{]*?\b([A-Za-z_]\w*)\s*\(")


def _helper_parts(text: str, h: str):
    """(start, end, return type text, [(type, name)], body inner text) of the
    static inline helper H defined in TEXT, else None."""
    loc = find_function(text, h)
    if not loc:
        return None
    s, b, e = loc
    hdr = text[s:b]
    m = re.search(r"\b" + re.escape(h) + r"\s*\(", hdr)
    if not m:
        return None
    rt = re.sub(r"\b(static|inline|__inline__|__inline)\b", "", hdr[:m.start()]).strip()
    c = match_brace(hdr, m.end() - 1)
    plist = hdr[m.end():c].strip()
    ps = []
    if plist and plist != "void":
        for p in split_top(plist):
            p = p.strip()
            mm = re.match(r"^(.*?)([A-Za-z_]\w*)\s*$", p)
            if not mm or not mm.group(1).strip() or "(" in p:
                return None
            ps.append((mm.group(1).strip(), mm.group(2)))
    return s, e + 1, rt, ps, text[b + 1:e]


def t_flatten_inline_helpers(text, name, ctx=None):
    """o531 UnduDataGetHeader: seed-defined `static inline` helpers called as
    `return H(args);` from NAME, flattened into the caller (gcc 2.96's inliner
    keeps the parameter copies and the helper's block structure: other
    registers / branch layout; 0.67 -> 0.75 by hand). Each site becomes
    `{ T p = arg; ... { helper body } }` (the helper's own `return`s return
    from NAME, which is what `return H(..)` did); a parameter whose argument is
    the same name is used directly, one clashing with a caller name is renamed.
    The helper definition is dropped when nothing else calls it. All helpers,
    then one at a time. Text-gated (a static inline helper in the body file)."""
    loc = find_function(text, name)
    if not loc:
        return
    helpers = [h for h in dict.fromkeys(m.group(1) for m in _INL_DEF.finditer(text)) if h != name]
    if not helpers:
        return
    fn_text = text[loc[0]:loc[2] + 1]
    helpers = [h for h in helpers if re.search(r"\breturn\s+" + re.escape(h) + r"\s*\(", fn_text)]
    if not helpers:
        return

    def flatten(t: str, hs) -> str | None:
        for h in hs:
            hp = _helper_parts(t, h)
            if not hp:
                return None
            hs_, he_, rt, ps, hbody = hp
            l2 = find_function(t, name)
            if not l2:
                return None
            fs, fb, fe = l2
            fn = t[fs:fe + 1]
            out, last = [], 0
            for m in re.finditer(r"(?m)^(?P<ind>[ \t]*)return\s+" + re.escape(h) + r"\s*\(", fn):
                o = m.end() - 1
                c = match_brace(fn, o)
                sm = re.match(r"\s*;", fn[c + 1:]) if c > 0 else None
                if not sm:
                    return None
                args = [a.strip() for a in split_top(fn[o + 1:c])] if fn[o + 1:c].strip() else []
                if len(args) != len(ps):
                    return None
                ind = m.group("ind")
                body = hbody
                decls = []
                for (pt, pn), a in zip(ps, args):
                    if a == pn:
                        continue
                    nn = pn
                    if re.search(r"(?<![\w.>])" + re.escape(pn) + r"\b", fn):
                        nn = _fresh(t, f"{pn}_")
                        body = re.sub(r"(?<![\w.>])" + re.escape(pn) + r"\b", nn, body)
                    star = pt.endswith("*")
                    decls.append(f"{ind}    {pt}{'' if star else ' '}{nn} = {a};")
                blk = [f"{ind}{{"] + decls + [f"{ind}    {{"]
                blk += [(ind + "    " + x if x.strip() else x) for x in body.strip("\n").split("\n")]
                blk += [f"{ind}    }}", f"{ind}}}"]
                out.append(fn[last:m.start()] + "\n".join(blk))
                last = c + 1 + sm.end()
            if not out:
                return None
            fn2 = "".join(out) + fn[last:]
            t = t[:fs] + fn2 + t[fe + 1:]
            rest = t[:hs_] + t[he_:].lstrip("\n")
            if not re.search(r"(?<![\w.>])" + re.escape(h) + r"\b", rest):
                t = rest                  # nothing else calls it: drop the definition
        return t
    seen = set()
    for lab, hs in [("all", helpers)] + ([(h, [h]) for h in helpers] if len(helpers) > 1 else []):
        try:
            nt = flatten(text, hs)
        except (ValueError, IndexError):
            nt = None
        if nt and nt != text and nt not in seen:
            seen.add(nt)
            yield f"flatten_inline_helpers:{lab}", nt


def t_gcse_split(fn, ctx=None):
    """o526 MenuTecExMain / o532 MenuTecListMain: -fno-gcse scores far closer
    (0.49 -> 0.92, 0.51 -> 0.86): GCSE/PRE hoisted a shared `p + K` / `p->f`
    into a saved register across blocks where the original recomputes it.
    Taking the pointer's address once - `(*(_ap = &p))->f` / `(*(_ap = &p))[K]` at one read,
    `T **_ap;` declared - makes p an ADDRESSOF pseudo and stops the hoist
    (o526 matched with exactly this at the 4th `ex->` access). Gated on the
    sweep's gcse diagnostic (ctx["gcse"]); each of the first accesses of each
    pointer local / param with >= 3 accesses, <= 16 candidates."""
    if not ctx or not ctx.get("gcse"):
        return
    head, inner, tail = _split_fn(fn)
    first = _decl_end(inner)
    lines = inner.split("\n")
    ap = _fresh(fn, "_ap")
    n = 0
    for P in _ptr_vars(fn):
        pat = re.compile(r"(?<![\w.>])" + re.escape(P) + r"\b(?!\s*(?:=(?!=)|\+\+|--|[-+*/|&^]=))")
        sites = [m for m in pat.finditer(inner) if inner.rfind("\n", 0, m.start()) >= len("\n".join(lines[:first]))]
        if len(sites) < 3:
            continue
        for m in sites[:10]:
            ni = inner[:m.start()] + f"(*({ap} = &{P}))" + inner[m.end():]
            nl = ni.split("\n")
            ind = _decl_ind(lines, first) if first < len(lines) else "    "
            nl.insert(first, f"{ind}__typeof__({P}) *{ap};")
            yield f"gcse_split:addr:{P}:{n}", head + "\n".join(nl) + tail
            n += 1
            if n >= 16:
                return


def shadow_locals(fn: str) -> list[tuple[int, str, str]]:
    """[(line index in the body, declarator, name)] of local declarations
    (any block) whose name is one of the function's parameters (draft_fix's
    `int arg0;` over a parameter whose type failed to parse; the body then
    reads an uninitialized local - o529 seed, menuResultItm)."""
    params = set()
    for _, _, p in _params(fn):
        m = re.search(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)", p) if "(" in p else None
        ids = re.findall(r"[A-Za-z_]\w*", re.sub(r"\[[^\]]*\]", "", p))
        if m:
            params.add(m.group(1))
        elif len(ids) >= 2 and ids[-1] not in ("void", "int", "char", "short", "long", "float", "double",
                                               "unsigned", "signed"):
            params.add(ids[-1])
    params.discard("")
    if not params:
        return []
    head, inner, tail = _split_fn(fn)
    out = []
    agg = _aggregate_lines(inner)
    for i, ln in enumerate(inner.split("\n")):
        if i in agg:
            continue
        p = parse_decl(re.sub(r"\s*__attribute__\s*\(\(.*\)\)", "", ln))
        if not p or re.match(r"\s*(extern|static|typedef)\b", ln):
            continue
        for d in p[2]:
            nm = decl_name(d)
            if nm in params and "=" not in d:
                out.append((i, d, nm))
    return out


def t_unshadow_param(fn):
    """A local declared with a parameter's name (draft_fix / permuter
    leftovers: `s32 menuResultItm(s32 *arg0) { int arg0; ... *(int *)arg0 = ..`
    writes through an uninitialized local; try_fn's `shadowed param` suspect
    tell): the declaration dropped, so the body uses the parameter. Ungated;
    also part of the sweep's cleanup pre-pass."""
    sl = shadow_locals(fn)
    if sl:
        yield "unshadow_param:all", _drop_decls(fn, sl)




# ---- fold13 families (o534-o549) ---------------------------------------------

_INF_HDR = re.compile(r"^\s*(?:while\s*\(\s*1\s*\)|for\s*\(\s*;\s*;\s*\)|do)\s*\{?\s*$")
_BRK_HDR = re.compile(r"^\s*(?:\}\s*)?(?:for|while|do|switch)\b")


def _own_breaks(lines, dep, b0, end):
    """Line indexes in [b0, end) holding a `break;` that leaves the loop whose
    body is [b0, end) (not one of a nested loop / switch)."""
    nested = []
    for j in range(b0, end):
        if _BRK_HDR.match(lines[j]) and not re.match(r"^\s*\}\s*while\b", lines[j]):
            lb = _loop_body(lines, dep, j)
            if lb and lb[2]:
                nested.append((lb[0], lb[1] + 1))
            else:
                nested.append((j + 1, j + 2))
    out = []
    for j in range(b0, end):
        if re.search(r"(?<!\w)break\s*;", lines[j]) and not any(a <= j < e for a, e in nested):
            out.append(j)
    return out


def t_sink_into_break(fn):
    """o537 camEventExec: a search loop `while (1) { t = f(i); if (t == 0) return 1;
    if (t == key) break; i += 4; } s = g(i + 2);` matched once the read after the
    loop moved into the found-arm: `if (t == key) { s = g(i + 2); break; }`
    (sched1 / reg-alloc see the read inside the loop). For an infinite loop
    (`while (1)`, `for (;;)`, `do .. while (1)`) left only by ONE `break;`:
    the first 1-3 plain statements after the loop sunk before that break,
    and the reverse (plain statements right before the break hoisted after
    the loop). Ungated text pattern."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    n = 0
    for i, ln in enumerate(lines):
        if not _INF_HDR.match(ln):
            continue
        is_do = re.match(r"^\s*do\b", ln) is not None
        lb = _loop_body(lines, dep, i)
        if not lb or not lb[2]:
            continue
        b0, end, _ = lb
        after = end + 1
        if is_do:
            if not re.match(r"^\s*\}\s*while\s*\(\s*1\s*\)\s*;\s*$", lines[end]):
                continue
        elif lines[end].strip() != "}":
            continue
        brks = _own_breaks(lines, dep, b0, end)
        if len(brks) != 1:
            continue
        k = brks[0]
        ind_loop = re.match(r"\s*", ln).group(0)
        # the break arm: `if (C) break;` on one line, or `break;` closing an if block
        one = re.match(r"^(?P<ind>\s*)if\s*\((?P<c>.*)\)\s*break\s*;\s*$", lines[k])
        if one:
            arm_ind = one.group("ind") + "    "
            pre_stmts = []
        elif lines[k].strip() == "break;" and k + 1 < len(lines) and lines[k + 1].strip() == "}":
            arm_ind = re.match(r"\s*", lines[k]).group(0)
            pre_stmts = []
            j = k - 1
            while j >= b0 and _simple_stmt(lines[j]) and dep[j] == dep[k] and len(pre_stmts) < 3:
                pre_stmts.insert(0, j)
                j -= 1
        else:
            continue
        # statements right after the loop
        post = []
        j = after
        while j < len(lines) and len(post) < 3 and _simple_stmt(lines[j]) and dep[j] == dep[i]:
            post.append(j)
            j += 1
        for cnt in range(1, len(post) + 1):
            mv = [lines[x].strip() for x in post[:cnt]]
            nl = list(lines)
            if one:
                rep = [f"{one.group('ind')}if ({one.group('c')}) {{"] + [arm_ind + s for s in mv] + \
                      [arm_ind + "break;", one.group("ind") + "}"]
            else:
                rep = [arm_ind + s for s in mv] + [lines[k]]
            del nl[post[0]:post[0] + cnt]
            nl[k:k + 1] = rep
            yield f"sink_into_break:sink:{i}:{cnt}", head + "\n".join(nl) + tail
            n += 1
        # reverse: plain statements before the break hoisted after the loop
        for cnt in range(1, len(pre_stmts) + 1):
            mv_idx = pre_stmts[-cnt:]
            mv = [lines[x].strip() for x in mv_idx]
            nl = list(lines)
            ins = [ind_loop + s for s in mv]
            nl[after:after] = ins
            for x in reversed(mv_idx):
                del nl[x]
            yield f"sink_into_break:hoist:{i}:{cnt}", head + "\n".join(nl) + tail
            n += 1
        if n >= 12:
            return



_PADD = re.compile(r"(?<![\w.>])(?P<p>[A-Za-z_]\w*)\s*\+=\s*(?P<k>0[xX][0-9a-fA-F]+|\d+)\s*;")
_SW_CAST = re.compile(r"\bswitch\s*\(\s*(?:\(\s*(?:u32|unsigned(?:\s+int)?|s32|int)\s*\)\s*)?"
                      r"\(\s*(?P<T>s16|short|u16|unsigned\s+short|s8|char|signed\s+char|u8|unsigned\s+char)\s*\)\s*"
                      r"(?P<x>[A-Za-z_]\w*)\s*\)")


def t_split_base_ptr(fn):
    """o540 cmdUnitparaSub: `p = f(..); switch (c) { case 0: p += 2; .. }`
    (0.78) matched as `base = f(..); switch (c) { case 0: p = base + 2; .. }`:
    a pointer local initialized once and otherwise only advanced by literal
    `p += K;` statements -> a separate base local (`__typeof__(p) _b`) and
    `p = _b + K;` (the `p = _b` copy kept and dropped). Also (`swcopy`): a
    switch on a narrowing cast of a parameter / local, `switch ((short)c)`,
    -> `short _c = c;` as the first declaration (and each other `(short)c` of
    the function replaced by it): o540's `short c = category; switch (c)`
    (0.53 -> 0.78 alone). Ungated text pattern."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    first = _decl_end(inner)
    dep = _depths(lines)
    # --- split base pointer
    for P in _ptr_vars(fn):
        sites = [i for i, ln in enumerate(lines) if any(m.group("p") == P for m in _PADD.finditer(ln))]
        if not sites:
            continue
        wr = re.compile(r"(?<![\w.>])" + re.escape(P) + r"\s*(?:=(?!=)|\+\+|--|[-*/|&^]=)")
        inits = [i for i, ln in enumerate(lines) if wr.search(ln) and i not in sites]
        decl_init = None
        for i in range(first):
            p = parse_decl(lines[i])
            if p and any(decl_name(d) == P and "=" in d for d in p[2]):
                decl_init = i
        if decl_init is not None:
            inits = [i for i in inits if i != decl_init]
            if inits:
                continue
            m = re.match(r"^(?P<ind>\s*)(?P<ty>.*?)\b" + re.escape(P) + r"\s*=\s*(?P<e>[^;]+);\s*$",
                         lines[decl_init].replace("*" + P, P).replace("* " + P, P))
            if not m or "," in lines[decl_init]:
                continue
            init_line, e = decl_init, m.group("e").strip()
        else:
            if len(inits) != 1:
                continue
            m = re.match(r"^(?P<ind>\s*)" + re.escape(P) + r"\s*=\s*(?P<e>[^;]+);\s*$", lines[inits[0]])
            if not m or inits[0] > min(sites):
                continue
            init_line, e = inits[0], m.group("e").strip()
        if any(_in_loop(lines, dep, s) for s in sites):
            continue
        B = _fresh(fn, "_b")
        ind = _decl_ind(lines, first)
        for keep in (False, True):
            nl = list(lines)
            for s in sites:
                nl[s] = _PADD.sub(lambda mm: (f"{P} = {B};" if int(mm.group("k"), 0) == 0 else
                                              f"{P} = {B} + {mm.group('k')};") if mm.group("p") == P else mm.group(0),
                                  nl[s])
            if decl_init is not None:
                dl = nl[decl_init]
                nl[decl_init] = re.sub(r"(?<![\w.>])" + re.escape(P) + r"\s*=\s*[^;]+;", P + ";", dl, count=1)
                nl.insert(decl_init + 1, f"{ind}__typeof__({P}) {B} = {e};")
                if keep:          # `p = _b;` as the first statement
                    nl.insert(_decl_end("\n".join(nl)), f"{ind}{P} = {B};")
            else:
                mi = re.match(r"\s*", nl[init_line]).group(0)
                nl[init_line] = f"{mi}{B} = {e};" + (f" {P} = {B};" if keep else "")
                nl.insert(first, f"{ind}__typeof__({P}) {B};")
            yield f"split_base_ptr:base:{P}" + (":keep" if keep else ""), head + "\n".join(nl) + tail
    # --- switch on a narrowing cast -> local copy
    for m in _SW_CAST.finditer(inner):
        T, X = m.group("T"), m.group("x")
        C = _fresh(fn, "_c")
        cast = re.compile(r"\(\s*" + re.escape(T).replace(r"\ ", r"\s+") + r"\s*\)\s*" + re.escape(X) + r"\b")
        for scope in ("switch", "all"):
            ni = inner[:m.start()] + f"switch ({C})" + inner[m.end():]
            if scope == "all":
                ni = cast.sub(C, ni)
            nl = ni.split("\n")
            fe = _decl_end(ni)
            nl.insert(1 if not nl[0].strip() else 0, f"{_decl_ind(lines, first)}{T} {C} = {X};")
            yield f"split_base_ptr:swcopy:{X}:{scope}", head + "\n".join(nl) + tail



_LSTRUCT_FIELD = re.compile(r"^(?P<ind>\s*)(?P<T>(?:const\s+)?(?:struct\s+)?[A-Za-z_]\w*)\s*\*\s*(?P<f>[A-Za-z_]\w*)\s*;\s*$")


def t_ptr_field_as_int(fn):
    """o539 cmdThinkCamEvent: a body-local record `struct { int type; int arg;
    ObjectTask *unit; ObjectTask *target; } ev;` filled field by field and
    passed by address (0.68) matched with the pointer fields typed `int`
    (`ev.unit = (int)pThinkUnit;`): an int store shares the alias set of the
    neighbouring int fields / the struct copy around it, so sched does not
    hoist it. For each pointer field of a struct defined inside the function:
    retyped `int`, stores `X.f = E;` -> `X.f = (int)(E);`, reads -> `((T *)X.f)`;
    one field at a time, then all. Ungated text pattern."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    fields = []          # (line, T, f, struct var names)
    for i, ln in enumerate(lines):
        if not re.match(r"^\s*(?:typedef\s+)?struct\s*\w*\s*\{\s*$", ln):
            continue
        d0 = dep[i]
        close = next((j for j in range(i + 1, len(lines)) if dep[j] == d0 + 1 and lines[j].lstrip().startswith("}")), None)
        if close is None:
            continue
        m = re.match(r"^\s*\}\s*(?P<v>[^;]*);", lines[close])
        if not m:
            continue
        names = [decl_name(v.strip().lstrip("*")) for v in m.group("v").split(",") if v.strip()]
        if re.match(r"^\s*typedef\b", ln) and names:
            tn = names[0]
            names = []
            for ln2 in lines[close + 1:]:
                p = parse_decl(ln2)
                if p and re.search(r"\b" + re.escape(tn) + r"\b", p[1]):
                    names += [decl_name(d) for d in p[2]]
        if not names:
            continue
        for j in range(i + 1, close):
            fm = _LSTRUCT_FIELD.match(lines[j])
            if fm and dep[j] == d0 + 1 and fm.group("T") not in ("void", "char", "const char"):
                fields.append((j, fm, names))
    if not fields:
        return

    def apply(nl, j, fm, names):
        T, f = fm.group("T"), fm.group("f")
        nl[j] = f"{fm.group('ind')}int {f};"
        acc = r"(?P<a>(?<![\w.>])(?:" + "|".join(map(re.escape, names)) + r")\s*(?:\.|->)\s*" + re.escape(f) + r"\b)"
        st = re.compile(r"^(?P<ind>\s*)" + acc + r"\s*=(?!=)\s*(?P<e>[^;]+);\s*$")
        rd = re.compile(acc)
        hit = False
        for k in range(len(nl)):
            if k == j:
                continue
            m = st.match(nl[k])
            if m:
                e = m.group("e").strip()
                if not re.fullmatch(r"-?\d+", e):
                    e = f"(int){e}" if re.fullmatch(r"[\w.>\-\[\]&]+", e) else f"(int)({e})"
                pre = rd.sub(lambda mm: f"(({T} *){mm.group('a')})", m.group("e"))
                nl[k] = f"{m.group('ind')}{m.group('a')} = {e if pre == m.group('e') else '(int)(' + pre.strip() + ')'};"
                hit = True
            elif rd.search(nl[k]):
                nl[k] = rd.sub(lambda mm: f"(({T} *){mm.group('a')})", nl[k])
                hit = True
        return hit

    seen = set()
    for j, fm, names in fields:
        nl = list(lines)
        if apply(nl, j, fm, names):
            t = head + "\n".join(nl) + tail
            if t not in seen:
                seen.add(t)
                yield f"ptr_field_as_int:{fm.group('f')}", t
    if len(fields) > 1:
        nl = list(lines)
        if any([apply(nl, j, fm, names) for j, fm, names in fields]):
            t = head + "\n".join(nl) + tail
            if t not in seen:
                yield "ptr_field_as_int:all", t



_CASE_LIT = re.compile(r"^\s*case\s+(?P<k>-?(?:0[xX][0-9a-fA-F]+|\d+))\s*:")


def t_explicit_default_case(fn):
    """o535 xglFontReloadTexture: `case 0: case 2: case 3: case 4: env = A; break;
    default: env = B;` (0.80) matched with an explicit `case 1:` beside
    `default:` - the listed values decide gcc's compare tree / table range.
    For each switch with a `default:`: `case K:` added right before it for
    each value K in the gaps of the listed literals and max+1 (<= 6), all gaps
    at once, and the reverse (a `case K:` label adjacent to `default:` dropped).
    Ungated text pattern."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    n = 0
    for i, ln in enumerate(lines):
        if not re.match(r"^\s*switch\s*\(.*\)\s*\{\s*$", ln):
            continue
        d = dep[i] + 1
        close = next((j for j in range(i + 1, len(lines)) if dep[j] == d and lines[j].lstrip().startswith("}")), None)
        if close is None:
            continue
        vals, dflt = [], None
        for j in range(i + 1, close):
            if dep[j] != d:
                continue
            for part in re.split(r"(?<=:)\s+(?=case\b|default\b)", lines[j].strip()):
                m = _CASE_LIT.match(part)
                if m:
                    vals.append((int(m.group("k"), 0), j))
                elif re.match(r"^default\s*:", part):
                    dflt = j
        if dflt is None or not vals or not re.match(r"^\s*default\s*:\s*$", lines[dflt]):
            continue
        ks = sorted({v for v, _ in vals})
        if ks[-1] - ks[0] > 64:
            continue
        gaps = [k for k in range(max(ks[0], 0), ks[-1] + 2) if k not in ks][:6]
        ind = re.match(r"\s*", lines[dflt]).group(0)
        for k in gaps:
            nl = list(lines)
            nl.insert(dflt, f"{ind}case {k}:")
            yield f"explicit_default_case:add:{i}:{k}", head + "\n".join(nl) + tail
            n += 1
        if len(gaps) > 1:
            nl = list(lines)
            nl[dflt:dflt] = [f"{ind}case {k}:" for k in gaps]
            yield f"explicit_default_case:add:{i}:all", head + "\n".join(nl) + tail
        # reverse: bare case labels right before / after default
        adj = []
        j = dflt - 1
        while j > i and re.match(r"^\s*case\s+[^:]+:\s*$", lines[j]):
            adj.append(j)
            j -= 1
        j = dflt + 1
        while j < close and re.match(r"^\s*case\s+[^:]+:\s*$", lines[j]):
            adj.append(j)
            j += 1
        for j in adj:
            nl = list(lines)
            del nl[j]
            yield f"explicit_default_case:drop:{i}:{j}", head + "\n".join(nl) + tail
        if n >= 14:
            return



def _asm_syms(unit: str, tu: str, name: str) -> set:
    """Data symbols the target .s of NAME references (%hi/%lo/%gp_rel, `.extern`)."""
    for asm in (ROOT / "build" / unit / "asm" / unit / "nonmatchings" / tu / f"{name}.s",
                ROOT / "build" / unit / "asm" / "nonmatchings" / unit / tu / f"{name}.s"):
        if asm.is_file():
            s = asm.read_text(errors="replace")
            return set(re.findall(r"%(?:hi|lo|gp_rel)\(([A-Za-z_][\w.$]*)\)", s)) | \
                set(re.findall(r"^\s*\.extern\s+([A-Za-z_][\w.$]*)", s, re.M))
    return set()


def _fn_name(fn: str) -> str | None:
    m = re.search(r"([A-Za-z_]\w*)\s*\(", fn[:fn.index("{")])
    return m.group(1) if m else None


def _blocks(lines, dep, lo, hi):
    """[(open line, first, end)] of brace blocks and switch cases in [lo, hi)."""
    out = []
    for i in range(lo, hi):
        s = lines[i].rstrip()
        if s.endswith("{") and not re.match(r"^\s*(?:switch|typedef|struct|union|enum)\b", s) \
                and "=" not in s.split("(")[0]:
            for j in range(i + 1, hi):
                if dep[j] == dep[i] + 1 and lines[j].lstrip().startswith("}"):
                    out.append((i, i + 1, j))
                    break
        elif re.match(r"^\s*(case\b[^:]*|default\s*):\s*$", s):
            e = _region(lines, dep, i + 1, True) if i + 1 < hi else i + 1
            if e > i + 1:
                out.append((i, i + 1, e))
    return out


def t_global_local_copy(fn, ctx=None):
    """o535 xglFontReloadTexture / o541 dbheader_load: a global read in several
    statements of one block cached in a block-local copy,
    `if (type == 1) { unsigned char *fs; f(..); fs = FS; g(*(char **)fs ..); .. }`
    (o535 0.9946 -> match with the per-block copies) and a global string
    passed to two calls through `const char *name = D_x;` (o541 0.74 -> 0.93).
    Globals are the data symbols of the target .s (%hi/%lo/%gp_rel), not
    assigned in the function, read on >= 2 lines. Per block that reads it, and
    function-wide: `__typeof__((G) + 0) _g = G;` declared at the block top, or
    declared there and assigned right before the first statement using it
    (`&G` / `G.f` -> a pointer copy `_g = &G`, `_g->f`). Context: ctx unit/tu."""
    if not ctx or not ctx.get("unit"):
        return
    name = _fn_name(fn)
    if not name:
        return
    syms = _asm_syms(ctx["unit"], ctx["tu"], name)
    if not syms:
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    first = _decl_end(inner)
    locs = set(_locals_block(inner)[0]) | {p for _, _, p in _params(fn)}
    n = 0
    for G in sorted(syms):
        if G in locs or not re.search(r"(?<![\w.>])" + re.escape(G) + r"\b", inner):
            continue
        use = re.compile(r"(?<![\w.>])" + re.escape(G) + r"\b(?!\s*\()")
        if re.search(r"(?<![\w.>])" + re.escape(G) + r"\s*(?:=(?!=)|\+\+|--|[-+*/|&^]=)", inner) or \
                re.search(r"(?:\+\+|--)\s*" + re.escape(G) + r"\b", inner):
            continue
        dotted = re.search(r"(?<![\w.>])" + re.escape(G) + r"\s*\.", inner) is not None
        V = _fresh(fn, "_g")
        regions = [(None, first, len(lines))] + [b for b in _blocks(lines, dep, first, len(lines))]
        total = sum(1 for j in range(first, len(lines)) if use.search(lines[j]))
        if total < 2:
            continue
        for opn, a, e in regions:
            ul = [j for j in range(a, e) if use.search(lines[j]) and not parse_decl(lines[j])]
            if not ul or (opn is None and len(ul) < 2):
                continue
            # declarations of a nested block go after its own leading decls
            sub = "\n".join(lines[a:e])
            da = a + _decl_end(sub) if opn is not None else first
            ind = re.match(r"\s*", lines[ul[0]]).group(0)
            if dotted:
                decl_t, init = f"__typeof__({G}) *", f"&{G}"

                def rep(s):
                    s = re.sub(r"&\s*" + re.escape(G) + r"\b(?!\s*[.\[])", V, s)
                    s = re.sub(r"(?<![\w.>])" + re.escape(G) + r"\s*\.", V + "->", s)
                    return use.sub(f"(*{V})", s)
            else:
                decl_t, init = f"__typeof__(({G}) + 0) ", G

                def rep(s):
                    return use.sub(V, s)
            for form in ("decl", "late"):
                nl = list(lines)
                for j in range(a, e):
                    if j >= da or opn is None:
                        nl[j] = rep(nl[j]) if not parse_decl(nl[j]) else nl[j]
                if form == "decl":
                    nl.insert(da, f"{ind}{decl_t}{V} = {init};")
                else:
                    nl.insert(ul[0], f"{ind}{V} = {init};")
                    nl.insert(da, f"{ind}{decl_t}{V};")
                yield f"global_local_copy:{G}:{'fn' if opn is None else opn}:{form}", head + "\n".join(nl) + tail
                n += 1
            if n >= 10:
                return



def t_local_array_scope(fn):
    """o538 MenuShopListChange00: `unsigned char rows[18] = {..};` declared at
    the top (0.59) vs in a nested block opened AFTER the if/else that precedes
    its first use: `if (..) {..} else {..} { unsigned char rows[18] = {..}; ..rest.. }`
    (0.976; gcc emits the initializer copy where the declaration's scope
    starts). For each initialized local array declared in the function's
    leading declarations: moved into the one block that holds all its uses,
    or into a new block wrapping the rest of the function from the top-level
    statement that first uses it / from the statement right after the
    compound statement before that. Reverse: an initialized array in a
    nested block / bare block hoisted to the function's declarations.
    Ungated text pattern."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    first = _decl_end(inner)
    if first >= len(lines):
        return
    d0 = dep[first]
    ind0 = _decl_ind(lines, first)
    n = 0
    seen = set()

    def emit(lab, nl):
        t = head + "\n".join(nl) + tail
        if t not in seen and t != fn:
            seen.add(t)
            return lab, t
        return None

    for i in range(first):
        ln = lines[i]
        m = re.match(r"^\s*(?!extern|static|typedef)[^=;]*?\b(?P<v>[A-Za-z_]\w*)\s*\[[^\]]*\]\s*=\s*\{.*\}\s*;\s*$", ln)
        if not m:
            continue
        v = m.group("v")
        use = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
        ul = [j for j in range(first, len(lines)) if use.search(lines[j])]
        if not ul:
            continue
        decl = ln.strip()
        # (a) every use inside one block
        best = None
        for b in _blocks(lines, dep, first, len(lines)):
            opn, a, e = b
            if lines[opn].rstrip().endswith("{") and all(a <= j < e for j in ul):
                if best is None or e - a < best[2] - best[1]:
                    best = b
        if best:
            opn, a, e = best
            sub = "\n".join(lines[a:e])
            da = a + _decl_end(sub)
            bind = re.match(r"\s*", lines[ul[0]]).group(0)
            nl = list(lines)
            nl.insert(da, bind + decl)
            del nl[i]
            r = emit(f"local_array_scope:{v}:block{opn}", nl)
            if r:
                yield r
                n += 1
        # (b) a new block wrapping the rest of the function
        top = [j for j in range(first, len(lines)) if dep[j] == d0 and lines[j].strip()]
        u0 = ul[0]
        s0 = max([j for j in top if j <= u0], default=None)
        starts = []
        if s0 is not None:
            starts.append(s0)
            # statement right after the compound statement that precedes s0
            prev = [j for j in top if j < s0]
            k = s0
            for j in reversed(prev):
                if lines[j].strip().startswith("}") or lines[j].rstrip().endswith("}"):
                    break
                if dep[j] == d0 and not _simple_stmt(lines[j]):
                    break
                k = j
            if k != s0:
                starts.append(k)
        end = len(lines)
        while end > 0 and not lines[end - 1].strip():
            end -= 1
        for s in starts:
            if any(re.match(r"^\s*(?:case\b|default\s*:|[A-Za-z_]\w*\s*:(?!:))", lines[j]) and dep[j] == d0
                   for j in range(s, end)):
                continue
            nl = list(lines)
            nl.insert(end, ind0 + "}")
            nl[s:s] = [ind0 + "{", ind0 + "    " + decl]
            del nl[i]
            r = emit(f"local_array_scope:{v}:wrap{s}", nl)
            if r:
                yield r
                n += 1
        if n >= 8:
            return
    # reverse: initialized arrays declared in nested blocks -> function top
    for j in range(first, len(lines)):
        m = re.match(r"^\s*(?!extern|static|typedef)[^=;]*?\b(?P<v>[A-Za-z_]\w*)\s*\[[^\]]*\]\s*=\s*\{.*\}\s*;\s*$", lines[j])
        if m and dep[j] > d0 and parse_decl(lines[j]) is not None:
            v = m.group("v")
            if re.search(r"(?<![\w.>])" + re.escape(v) + r"\b", "\n".join(lines[:j])):
                continue
            nl = list(lines)
            dl = nl.pop(j)
            nl.insert(first, ind0 + dl.strip())
            r = emit(f"local_array_scope:{v}:hoist", nl)
            if r:
                yield r



def fzero_shape(rows) -> list[str]:
    """Target `mtc1 zero,$fN` (a float 0.0 kept in an FPR, stored with swc1)
    more often than ours (gcc folded our 0.0f stores into `sw zero`)."""
    a = [x for x, _, _ in diff_rows(rows) if x and re.match(r"mtc1\s+zero,\$f\d+", x)]
    b = [y for _, _, y in diff_rows(rows) if y and re.match(r"mtc1\s+zero,\$f\d+", y)]
    return a if len(a) > len(b) else []


_FZERO_LIT = re.compile(r"^(?P<ind>\s*)(?P<lhs>[^=;]+?)\s*=\s*(?P<k>0?\.0*[fF]?|0\.[0]*[fF]?|0[fF])\s*;\s*$")


def t_float_zero_store(fn, ctx=None):
    """o544 ACT_initMotion: the target loads 0.0 once (`mtc1 zero,$f20`) and
    stores it with swc1; ours folds `float zero = 0.0f;` into `sw zero`.
    A `double` holding the zero is not folded into the float stores (the
    conversion keeps an FPR pseudo): `double zero = 0.0;` reproduces the
    mtc1 + swc1 shape. Gated on more target `mtc1 zero,$fN` rows (diff tag
    `float_zero_store`): each float local only ever assigned 0 -> double, and the
    float-zero literal stores (`X = 0.0f;`) routed through one
    `double _z = 0.0;` (all, then each)."""
    if not (ctx or {}).get("float_zero_store"):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    first = _decl_end(inner)
    for i in range(first):
        p = parse_decl(lines[i])
        if not p or not re.fullmatch(r"\s*(?:float|f32)\s*", p[1]) or len(p[2]) != 1:
            continue
        v = decl_name(p[2][0])
        if "[" in p[2][0] or "*" in p[2][0]:
            continue
        sites = [j for j in range(first, len(lines)) if re.match(r"^\s*" + re.escape(v) + r"\s*=(?!=)", lines[j])]
        init = p[2][0].split("=", 1)[1].strip() if "=" in p[2][0] else None
        vals = [re.match(r"^\s*" + re.escape(v) + r"\s*=\s*([^;]+);", lines[j]) for j in sites]
        allv = ([init] if init else []) + [m.group(1).strip() if m else "?" for m in vals]
        if not allv or not all(re.fullmatch(r"0?\.0*[fF]?|0\.0*[fF]?|0[fF]?", x or "?") for x in allv):
            continue
        if re.search(r"&\s*" + re.escape(v) + r"\b|\b" + re.escape(v) + r"\s*(?:\+\+|--|[-+*/]=)", inner):
            continue
        nl = list(lines)
        nl[i] = re.sub(r"\b(?:float|f32)\b", "double", lines[i], count=1).replace("0.0f", "0.0")
        for j in sites:
            nl[j] = re.sub(r"=\s*[^;]+;", "= 0.0;", nl[j], count=1)
        yield f"float_zero_store:double:{v}", head + "\n".join(nl) + tail
    zs = [j for j in range(first, len(lines)) if (m := _FZERO_LIT.match(lines[j])) and "." in m.group("k") + "." and
          (m.group("k").endswith(("f", "F")) or "." in m.group("k")) and not parse_decl(lines[j])]
    zs = [j for j in zs if not re.match(r"^\s*[A-Za-z_]\w*\s*=", lines[j]) or
          re.match(r"^\s*([A-Za-z_]\w*)", lines[j]).group(1) not in _all_locals(inner)[0]]
    if not zs:
        return
    Z = _fresh(fn, "_z")
    ind = _decl_ind(lines, first)
    sel = [("all", zs)] + ([(str(j), [j]) for j in zs[:6]] if len(zs) > 1 else [])
    for lab, js in sel:
        nl = list(lines)
        for j in js:
            nl[j] = _FZERO_LIT.sub(lambda m: f"{m.group('ind')}{m.group('lhs')} = {Z};", nl[j])
        nl.insert(first, f"{ind}double {Z} = 0.0;")
        yield f"float_zero_store:lit:{lab}", head + "\n".join(nl) + tail








# ---- fold14 (o550-o569) ------------------------------------------------------

def implicit_hint(unit: str, tu: str, name: str, text: str) -> dict | None:
    """{"implicit": [callees NAME uses with no prototype in scope]} from one
    try_fn run with the -Wimplicit-function-declaration check (fold14, o564
    SEQ_moveUnitChr: xglSin/xglCos undeclared -> int returns + double args).
    Gates implicit_decl_proto."""
    import try_fn
    fd, p = tempfile.mkstemp(prefix=f"im_{name}_", suffix=".c")
    os.write(fd, text.encode())
    os.close(fd)
    try:
        r = try_fn.try_function(unit, tu, name, Path(p), diff_limit=0, uninit=True)
    finally:
        os.unlink(p)
    return {"implicit": list(r.get("implicit") or [])}


def t_implicit_decl_proto(text, name, ctx=None):
    """o564 SEQ_moveUnitChr (0.9363 -> match): callees called with no
    prototype in scope get gcc's implicit `int f()` (int return read from v0
    + cvt, float args promoted to double). Inserts protos.json's prototype
    (proto_index.py) before NAME: all of them, then each callee whose real
    prototype has a float/double/narrow return, float params or a void
    return alone. Gated on ctx["implicit"] (sweep pre-step implicit_hint)."""
    imp = (ctx or {}).get("implicit") or []
    if not imp:
        return
    import try_fn
    loc = find_function(text, name)
    if not loc:
        return
    s = text.rfind("\n", 0, loc[0]) + 1
    pro = []
    for f in imp:
        p = try_fn.real_proto(f)
        if p and not re.search(r"(?<![\w.>])" + re.escape(f) + r"\s*\([^;{}]*\)\s*;", text[:s]):
            pro.append((f, p.strip().rstrip(";") + ";"))
    if not pro:
        return
    yield "implicit_decl_proto:all", text[:s] + "\n".join(p for _, p in pro) + "\n" + text[s:]
    if len(pro) > 1:
        k = 0
        for f, p in pro:
            if try_fn.implicit_semantic(f) or re.match(r"^\s*(?:extern\s+)?void\s", p):
                yield f"implicit_decl_proto:{f}", text[:s] + p + "\n" + text[s:]
                k += 1
                if k >= 6:
                    return


def t_callee_ret_cast(text, name, ctx=None):
    """o552 Check_Straight_ID / o555 / o556: m2c read a callee's result as
    `M2C_ERROR(/* Read from unset register $f0 */)` (or $v0) because the
    callee is declared void; draft_fix's stub made it 0 (a wrong seed).
    draft_fix.m2c_ret_fix rewrites it to `fret_g = ((float (*)(..))g)(..)` /
    an inline cast call; a second form also adds protos.json prototypes for
    the rewritten callees the body text does not declare (a cast needs a
    declared name). Text-gated (the body still has such an M2C_ERROR)."""
    if "unset register" not in text:
        return
    import draft_fix
    import try_fn
    nt, notes = draft_fix.m2c_ret_fix(text, try_fn.real_proto)
    if not notes or nt == text:
        return
    yield "callee_ret_cast:cast", nt
    fs = list(dict.fromkeys(re.findall(r"callee_ret_cast (\w+)", " ".join(notes))))
    loc = find_function(nt, name)
    if not loc:
        return
    s = nt.rfind("\n", 0, loc[0]) + 1
    add = []
    for f in fs:
        p = try_fn.real_proto(f)
        if p and not re.search(r"(?<![\w.>])" + re.escape(f) + r"\s*\([^;{}]*\)\s*;", nt[:s]):
            add.append(p.strip().rstrip(";") + ";")
    if add:
        yield "callee_ret_cast:proto", nt[:s] + "\n".join(add) + "\n" + nt[s:]


def early_fconst_shape(rows) -> list[str]:
    """A float constant built (`lui at,K; mtc1 at,$fN`) at another place: a
    differing `mtc1 at,` row on both sides (o550 InitCfSystem)."""
    t = [a for a, m, b in diff_rows(rows) if m != " " and a and re.match(r"^mtc1\s+at,", a)]
    c = [b for a, m, b in diff_rows(rows) if m != " " and b and re.match(r"^mtc1\s+at,", b)]
    return t if t and c else []


_FLIT_STMT = re.compile(r"(?<![\w.])(?P<n>-?(?:\d+\.\d*|\.\d+)(?:[eE][-+]?\d+)?)[fF]?(?![\w.])")


def t_early_float_const(fn, ctx=None):
    """o550 InitCfSystem (0.9531 -> 0.9766, the AI's 0.68 -> 0.945 step): a
    non-zero float literal used by one statement -> a local `float _fk;`
    assigned K 1..4 statements earlier, right after the previous call, or at
    the block start (before an unrelated
    load), the use reading `_fk`: the lui/mtc1 moves up. Gated on a
    differing `mtc1 at,` on both sides."""
    if not (ctx or {}).get("early_float_const"):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    first = _decl_end(inner)
    ind0 = _decl_ind(lines, first) if first < len(lines) else "    "
    v = _fresh(fn, "_fk")
    n = 0
    for i in range(first, len(lines)):
        ln = lines[i]
        if parse_decl(ln) or not _simple_stmt(ln) or '"' in ln:
            continue
        ms = [m for m in _FLIT_STMT.finditer(ln) if float(m.group("n")) != 0.0]
        if len(ms) != 1:
            continue
        m = ms[0]
        lit = ln[m.start():m.end()]
        if lit.startswith("-") and ln[:m.start()].rstrip()[-1:] not in ("=", "(", ",", "?", ":"):
            continue
        if not lit.lower().endswith("f"):
            lit = lit + "f"
        use = ln[:m.start()] + v + ln[m.end():]
        pos, k, after_call = [], i, None
        while len(pos) < 30:
            k -= 1
            while k >= first and not lines[k].strip():
                k -= 1
            if k < first or dep[k] != dep[i]:
                break
            s = lines[k].strip()
            if s.endswith("{") or s.startswith("}") or re.match(r"^\w+\s*:", s) or parse_decl(lines[k]):
                break
            if after_call is None and _calls_in(s):
                after_call = pos[-1] if pos else None
                if after_call is None:
                    break
            pos.append(k)
        picks = list(dict.fromkeys(pos[:4] + ([after_call] if after_call is not None else []) + pos[-1:]))
        for k in picks:
            ind = re.match(r"\s*", lines[k]).group(0)
            nl = lines[:k] + [f"{ind}{v} = {lit};"] + lines[k:i] + [use] + lines[i + 1:]
            nl.insert(first, f"{ind0}float {v};")
            yield f"early_float_const:L{i}:b{pos.index(k) + 1}", head + "\n".join(nl) + tail
            n += 1
            if n >= 24:
                return


def t_int_ptr_param(text, name, ctx=None):
    """o553 GetBSplineLoop (0.9417 -> match): `T *pts` param indexed as
    `&pts[E]` / `pts + E` / `pts[E].f` -> `int pts` with every use written
    `(T *)((E) * sizeof(T) + pts)`: the int is the second addu operand, as in
    the target (addu_swap swaps one site; this needs all of them and the
    param type). Gated on addu_swap's diff shape."""
    if not (ctx or {}).get("addu_swap"):
        return
    loc = find_function(text, name)
    if not loc:
        return
    fn = text[loc[0]:loc[2] + 1]
    head, inner, tail = _split_fn(fn)
    po = head.index("(")
    pc = match_brace(head, po)
    if pc < 0:
        return
    params = split_top(head[po + 1:pc])
    k = 0
    for j, p in enumerate(params):
        m = re.match(r"^\s*(?P<T>(?:const\s+)?(?:struct\s+)?[A-Za-z_]\w*)\s*\*\s*(?P<v>[A-Za-z_]\w*)\s*$", p)
        if not m or m.group("T").split()[-1] in ("void", "char", "s8", "u8") or "unsigned char" in p:
            continue
        T, v = m.group("T"), m.group("v")
        cast = lambda e: f"(({T} *)(({e}) * sizeof({T}) + {v}))"  # noqa: E731
        body = inner
        out, i, ok = [], 0, True
        pat = re.compile(r"(?<![\w.>])" + re.escape(v) + r"\b")
        for mm in pat.finditer(body):
            if mm.start() < i:
                continue
            a, b = mm.start(), mm.end()
            pre = body[:a].rstrip()
            rest = body[b:]
            if pre.endswith("&") and rest.lstrip().startswith("["):     # &v[E]
                o = b + rest.index("[")
                c = match_brace(body, o)
                if c < 0:
                    ok = False
                    break
                amp = body.rindex("&", 0, a)
                out.append(body[i:amp] + cast(body[o + 1:c]))
                i = c + 1
            elif rest.lstrip().startswith("["):                           # v[E] -> (*(T *)..)
                o = b + rest.index("[")
                c = match_brace(body, o)
                if c < 0:
                    ok = False
                    break
                out.append(body[i:a] + "(*" + cast(body[o + 1:c]) + ")")
                i = c + 1
            elif re.match(r"\s*\+\s*", rest) and not re.match(r"\s*\+\s*\+", rest):   # v + E
                mp = re.match(r"\s*\+\s*", rest)
                e0 = b + mp.end()
                e1 = _operand_end(body, e0)
                if e1 <= e0:
                    ok = False
                    break
                out.append(body[i:a] + cast(body[e0:e1]))
                i = e1
            elif re.match(r"\s*(=(?!=)|\+\+|--|[-+]=)", rest) or pre.endswith(("++", "--")):
                ok = False
                break
            else:
                out.append(body[i:a] + f"(({T} *){v})")
                i = b
        if not ok:
            continue
        out.append(body[i:])
        ni = "".join(out)
        if ni == inner:
            continue
        np = list(params)
        np[j] = re.sub(r"^(\s*).*$", lambda q: q.group(1) + f"int {v}", p)
        nh = head[:po + 1] + ",".join(np) + head[pc:]
        yield f"int_ptr_param:{v}", text[:loc[0]] + nh + ni + tail + text[loc[2] + 1:]
        k += 1
        if k >= 3:
            return


def vol_delay_shape(rows) -> list[str]:
    """Globals whose load fills a branch delay slot where the target has a
    `nop` (and loads it elsewhere): o555 Get_EnemyIDPos' loop-end slot."""
    out = []
    tl = [a for a, _, _ in diff_rows(rows) if a]
    for a, m, b in diff_rows(rows):
        if m == "|" and a and b and a.strip() == "nop" and _LOAD_MN.match(_mn(b)):
            r = re.search(r"\[(?:LO16|GPREL16) ([\w.$]+)\]", b)
            if r and any(r.group(1) in x and _LOAD_MN.match(_mn(x)) for x in tl):
                out.append(r.group(1))
    return list(dict.fromkeys(out))


def t_volatile_global(text, name, ctx=None):
    """o555 Get_EnemyIDPos (0.9668 -> match): `extern volatile float G;` when
    the target leaves the loop-end delay slot a nop where ours fills it with
    G's load (volatile loads are not moved into slots). The body file's
    `extern T G;` gets `volatile`; with no declaration in the body, the reads
    in NAME become `(*(volatile T *)&G)` (T from the TU / body text).
    Gated on vol_delay_shape."""
    gs = (ctx or {}).get("volatile_global") or []
    loc = find_function(text, name)
    if not gs or not loc:
        return
    for g in gs[:3]:
        dm = re.search(r"^(?P<a>[ \t]*extern\s+)(?P<T>(?!volatile)[^;()]*?)\s+(?P<g>" + re.escape(g)
                       + r")\s*(?P<arr>\[[^\]]*\])?\s*;", text[:loc[0]], re.M)
        if dm:
            yield (f"volatile_global:{g}:decl",
                   text[:dm.start("T")] + "volatile " + text[dm.start("T"):])
            continue
        T = None
        try:
            import try_fn
            src = (ROOT / "src" / ctx["unit"] / f"{ctx['tu']}.c").read_text(errors="replace")
            mt = re.search(r"\bextern\s+(?P<T>(?!volatile)[A-Za-z_][\w ]*?)\s+" + re.escape(g) + r"\s*;", src)
            T = mt.group("T") if mt else None
        except (OSError, KeyError):
            T = None
        if not T:
            continue
        fn = text[loc[0]:loc[2] + 1]
        head, inner, tail = _split_fn(fn)
        ni = re.sub(r"(?<![\w.>&])" + re.escape(g) + r"\b(?!\s*(?:=(?!=)|\[))", f"(*(volatile {T} *)&{g})", inner)
        if ni != inner:
            yield f"volatile_global:{g}:cast", text[:loc[0]] + head + ni + tail + text[loc[2] + 1:]


def t_goto_loop(fn):
    """o561 CalcPosContainer: `do { B } while (C);` -> `L: B; if (C) goto L;`
    (the goto loop is not a loop note: loop.c's invariant hoist (a constant
    rematerialized every pass in the target) no longer runs); and the reverse
    for m2c's `L: B; if (C) { goto L; }` (then count_up_loop / loop_form can
    act: o566 ACT_updateNPC's m2c seed). Ungated, few candidates."""
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in re.finditer(r"(?m)^(?P<ind>[ \t]*)do\s*\{", inner):
        b = inner.index("{", m.start())
        e = match_brace(inner, b)
        if e < 0:
            continue
        w = re.match(r"\s*while\s*\(", inner[e + 1:])
        if not w:
            continue
        co = e + 1 + w.end() - 1
        cc = match_brace(inner, co)
        sc = re.match(r"\s*;", inner[cc + 1:]) if cc > 0 else None
        body = inner[b + 1:e]
        if not sc or re.search(r"\b(break|continue)\b", body) or inner[co + 1:cc].strip() == "0":
            continue
        lab = _label_name(fn, "_loop")
        ind = m.group("ind")
        fl = next((x for x in body.split("\n") if x.strip()), "")
        if parse_decl(fl):                 # leading declarations: keep them in a block
            rep = f"{lab}: {{\n{body.strip(chr(10))}\n{ind}}}\n{ind}if ({inner[co + 1:cc]}) goto {lab};"
        else:
            rep = f"{lab}:\n{body.strip(chr(10))}\n{ind}if ({inner[co + 1:cc]}) goto {lab};"
        yield f"goto_loop:do{n}", head + inner[:m.start()] + rep + inner[cc + 1 + sc.end():] + tail
        n += 1
    for m in re.finditer(r"(?m)^[ \t]*(?P<L>[A-Za-z_]\w*):[ \t]*;?[ \t]*$", inner):
        L = m.group("L")
        if L in ("default",):
            continue
        refs = re.findall(r"\bgoto\s+" + re.escape(L) + r"\s*;", inner)
        if len(refs) != 1:
            continue
        g = re.compile(r"(?m)^(?P<ind>[ \t]*)if\s*\((?P<c>.*)\)\s*(?:\{\s*goto\s+" + re.escape(L)
                       + r"\s*;\s*\}|goto\s+" + re.escape(L) + r"\s*;)[ \t]*$")
        gm = g.search(inner, m.end())
        if not gm:
            continue
        body = inner[m.end():gm.start()]
        if re.search(r"\b(goto|break|continue|case|default)\b|^\s*\w+\s*:", body, re.M):
            continue
        dep = 0
        bad = False
        for ch in body:
            dep += ch == "{"
            dep -= ch == "}"
            if dep < 0:
                bad = True
                break
        if bad or dep:
            continue
        ind = gm.group("ind")
        bl = "\n".join(("    " + x if x.strip() else x) for x in body.strip("\n").split("\n"))
        rep = f"{ind}do {{\n{bl}\n{ind}}} while ({gm.group('c')});"
        yield f"goto_loop:to_do:{L}", head + inner[:m.start()] + rep + inner[gm.end():] + tail
        n += 1
        if n >= 8:
            return


def t_ret_to_if_return(fn):
    """o562 checkClass (movn -> bnezl): `return C ? X : Y;` whose fall-through
    reaches the function's final `return Y;` -> `if (C) return X;` (the
    inner return dropped), plus `if (!(C)) return Y; return X;`. unternary
    already makes `if (C) return X; return Y;`."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    dep = _depths(lines)
    ri = max((i for i, ln in enumerate(lines) if ln.strip()), default=-1)
    if ri < 0:
        return
    fr = re.match(r"^\s*return\s+(?P<e>[^;]+);\s*$", lines[ri])
    for i, ln in enumerate(lines):
        if i == ri:
            continue
        m = re.match(r"^(?P<ind>\s*)return\s+(?P<e>[^;]*\?[^;]*);\s*$", ln)
        if not m:
            continue
        t = _split_ternary(_strip_parens(m.group("e")))
        if not t:
            continue
        c, x, y = (_strip_parens(q) for q in t)
        ind = m.group("ind")
        if fr and re.sub(r"\s+", "", _strip_parens(fr.group("e"))) == re.sub(r"\s+", "", y) \
                and _flows_to_return(lines, dep, i, ri):
            nl = lines[:i] + [f"{ind}if ({c}) {{", f"{ind}    return {x};", f"{ind}}}"] + lines[i + 1:]
            yield f"ret_to_if_return:fall:L{i}", head + "\n".join(nl) + tail
        nl = lines[:i] + [f"{ind}if (!({c})) {{", f"{ind}    return {y};", f"{ind}}}", f"{ind}return {x};"] + lines[i + 1:]
        yield f"ret_to_if_return:neg:L{i}", head + "\n".join(nl) + tail


def union_store_shape(rows) -> list[str]:
    """A non-sp sw/sh moved among float loads (lwc1 rows in the differing
    window): o569 TWIN_draw2."""
    win = [x for a, m, b in diff_rows(rows) if m != " " for x in (a, b) if x]
    if not any(_mn(x) == "lwc1" for x in win):
        return []
    return [x for x in win if _mn(x) in ("sw", "sh") and not re.search(r"\((sp|fp)\)", x)][:4]


_RAW_STORE = re.compile(r"^(?P<ind>\s*)\*\s*(?P<lv>[^=;]+?)\s*=(?!=)\s*(?P<r>[^;]+);\s*$")
_INT_CAST = re.compile(r"^\(\s*(?:int|unsigned\s+int|s32|u32)\s*\*\s*\)\s*")


def _raw_int_store(ln: str):
    """`*(int *)A = V;` / `*((int *)(A)) = V;` -> (indent, A, V) or None."""
    m = _RAW_STORE.match(ln)
    if not m:
        return None
    lv = m.group("lv").strip()
    while lv.startswith("(") and match_brace(lv, 0) == len(lv) - 1:
        lv = lv[1:-1].strip()
    c = _INT_CAST.match(lv)
    if not c:
        return None
    a = lv[c.end():].strip()
    if not a or (a[0] == "(" and match_brace(a, 0) != len(a) - 1) or re.search(r"[=,;]", a):
        return None
    return m.group("ind"), a, m.group("r").strip()


def t_union_store(fn, ctx=None):
    """o569 TWIN_draw2 (0.7704 -> 0.9898): an int store through a raw cast
    `*((int *)(E)) = V;` -> `((union { int i; float f; } *)(E))->i = V;`:
    the union's alias set conflicts with float loads, so sched keeps the
    store before them (TBAA ordering, like o568's typed short field). One
    store at a time, <= 12. Gated on union_store_shape."""
    if not (ctx or {}).get("union_store"):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        p = _raw_int_store(ln)
        if not p:
            continue
        ind, a, r = p
        while a.startswith("(") and match_brace(a, 0) == len(a) - 1:
            a = a[1:-1].strip()
        nl = list(lines)
        nl[i] = f"{ind}((union {{ int i; float f; }} *)({a}))->i = {r};"
        yield f"union_store:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 12:
            return


_MIX_LHS = re.compile(r"^(?P<ind>\s*)(?P<lhs>[^=;]+?)\s*=(?!=)\s*(?P<rhs>[^;]+);\s*$")
_TYPE_WORDS = {"signed", "unsigned", "char", "short", "int", "long", "float", "double", "void", "const",
               "volatile", "struct", "union", "s8", "u8", "s16", "u16", "s32", "u32", "s64", "u64", "f32"}


def _mix_store(ln: str):
    m = _MIX_LHS.match(ln)
    if not m or not _simple_stmt(ln) or parse_decl(ln):
        return None
    lhs, rhs = m.group("lhs").strip(), m.group("rhs")
    if lhs.endswith(("+", "-", "*", "/", "|", "&", "^", "<<", ">>", "%", "!", "<", ">")):
        return None
    if not (re.search(r"->|\.\s*[A-Za-z_]|\[", lhs) or lhs.startswith("*")):
        return None
    if re.search(r"\+\+|--|(?<![=!<>])=(?!=)", rhs) or re.search(r"\b(?!sizeof\b)[A-Za-z_]\w*\s*\(", re.sub(
            r"\(\s*(?:unsigned\s+|signed\s+)?\w+\s*\**\s*\)", "", rhs)):
        return None
    ids = [w for w in re.findall(r"[A-Za-z_]\w*", lhs) if w not in _TYPE_WORDS]
    return m.group("ind"), (ids[0] if ids else ""), re.sub(r"\s+", "", lhs), re.sub(r"\s+", "", rhs)


def _store_sort_mixed(lines):
    """fold14 (o560 DrillStandbyFunc): runs of 2..4 adjacent stores through
    DIFFERENT bases (`drill->phase = 0; save->hit = 0; *(s8 *)(save + 0x32)
    = 0;`), which the same-base runs never group. Prior first: the target's
    first store written LAST (sched1 favours the store where a pointer reg
    dies: last_use_store_first), then every other order."""
    import itertools
    i = 0
    while i < len(lines):
        a = _mix_store(lines[i])
        if not a:
            i += 1
            continue
        j = i + 1
        run = [a]
        while j < len(lines) and j - i < 4:
            b = _mix_store(lines[j])
            if not b or b[0] != a[0]:
                break
            run.append(b)
            j += 1
        lh = [r[2] for r in run]
        ok = len(run) >= 2 and len({r[1] for r in run}) >= 2 and len(set(lh)) == len(lh) \
            and not any(x in r[3] for x in lh for r in run)
        if not ok:
            i += 1
            continue
        blk = lines[i:j]
        perms = [blk[1:] + blk[:1], blk[-1:] + blk[:-1]] + [list(p) for p in itertools.permutations(blk)]
        seen = {tuple(blk)}
        for p in perms:
            if tuple(p) not in seen:
                seen.add(tuple(p))
                yield i, len(blk), lines[:i] + p + lines[j:]
        i = j


_ALIAS_PTR = re.compile(r"^\s*(?:(?:const|volatile|unsigned|signed|struct|union)\s+)*\w+\s*\*\s*([A-Za-z_]\w*)\s*=\s*&\s*([A-Za-z_][\w.\[\]>-]*)\s*;\s*$")


def t_alias_deref(fn):
    """A local pointer to a variable (`T *p = &x;`, never reassigned) read
    through `*p` / `p[0]`: read `x` directly instead (_InitBattle, 2026-10-12:
    the loop bound `*pNumCams` -> `numCams` matched from 0.9906). Also the
    reverse is left to other families. One variant per alias pointer."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    for i, ln in enumerate(lines):
        m = _ALIAS_PTR.match(ln)
        if not m:
            continue
        p, x = m.group(1), m.group(2)
        rest = "\n".join(lines[i + 1:])
        if re.search(r"(?<![\w*])" + re.escape(p) + r"\s*(?:[-+*/|&^]?=(?!=)|\+\+|--)", rest) \
                or re.search(r"(?:\+\+|--)\s*" + re.escape(p) + r"\b", rest):
            continue                               # p itself is reassigned / stepped
        new = re.sub(r"\*\s*" + re.escape(p) + r"\b(?!\s*\[)|\b" + re.escape(p) + r"\s*\[\s*0\s*\]", x, rest)
        if new != rest:
            yield f"alias_deref:{p}", head + "\n".join(lines[:i + 1] + [new]) + tail



_OR_IF = re.compile(r"^(?P<ind>\s*)if\s*\((?P<c>.+)\)\s*(?P<s>[^{};][^;]*;)\s*$")


def _split_top_or(c: str) -> list[str] | None:
    parts, depth, cur, i = [], 0, "", 0
    while i < len(c):
        ch = c[i]
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if depth == 0 and c.startswith("||", i):
            parts.append(cur.strip()); cur = ""; i += 2; continue
        if depth == 0 and c.startswith("&&", i):
            return None                            # mixed && / || at top level: leave it
        cur += ch
        i += 1
    parts.append(cur.strip())
    return parts if len(parts) > 1 else None


def t_or_split(fn):
    """Check_Wall (2026-10-13, 0.69 -> 0.97 -> match): `if (a || b || c) S;`
    -> `if (a) S; else if (b) S; else if (c) S;` (S repeated per arm). The
    separate arms change which values gcc keeps in callee-saved registers.
    Single-statement ifs only; conditions with calls / ++ / -- / assignments
    are left alone (re-evaluation order must not change)."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        m = _OR_IF.match(ln)
        if not m or (i + 1 < len(lines) and lines[i + 1].strip().startswith("else")):
            continue
        parts = _split_top_or(m["c"])
        if not parts or any(re.search(r"\w\s*\(|\+\+|--|(?<![=!<>])=(?!=)", p) for p in parts):
            continue
        st = m["s"].strip()
        arms = [f"{m['ind']}if ({parts[0]}) {st}"] + [f"{m['ind']}else if ({p}) {st}" for p in parts[1:]]
        nl = lines[:i] + arms + lines[i + 1:]
        yield f"or_split:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 8:
            return


def t_loop_wrap(fn):
    """nmlModelRenderDrop / command_enenpcse (2026-10-13): wrap a whole
    for/while loop in `do { ... } while (0);` -- the extra block boundary
    gives the 8-byte alignment nop before the loop head the target has (and
    changes block layout). One variant per loop."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        if not re.match(r"^\s*(for|while)\s*\(", ln) or ln.rstrip().endswith(";"):
            continue
        depth, j, opened = 0, i, False
        while j < len(lines):
            depth += lines[j].count("{") - lines[j].count("}")
            opened = opened or "{" in lines[j]
            if opened and depth <= 0:
                break
            if not opened and j > i and lines[j].rstrip().endswith(";"):
                break                              # single-statement body
            j += 1
        if j >= len(lines):
            continue
        ind = re.match(r"^\s*", ln).group(0)
        nl = lines[:i] + [f"{ind}do {{"] + lines[i:j + 1] + [f"{ind}}} while (0);"] + lines[j + 1:]
        yield f"loop_wrap:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 6:
            return



def t_field_via_ptr(fn):
    """dataEnemyMdlChk (2026-10-13, 0.967 -> 0.9918): a field of a pointer
    parameter read more than once -> later reads go through a separate
    pointer local (`__typeof__(P->f) *_fv = &P->f;` ... `*_fv`), which the
    early CSE passes do not match, so the value can come from a register
    copy (reload_cse) the way the target has it. Variants: every use but the
    first, and every use."""
    head, inner, tail = _split_fn(fn)
    m = re.match(r"[^(]*\((?P<a>[^)]*)\)", head.replace("\n", " "))
    if not m:
        return
    params = [re.findall(r"\*\s*([A-Za-z_]\w*)\s*$", a.strip()) for a in m["a"].split(",")]
    params = [x[0] for x in params if x]
    n = 0
    for P in params:
        for f in sorted(set(re.findall(r"\b" + re.escape(P) + r"->([A-Za-z_]\w*)\b(?!\s*\()", inner))):
            if re.search(r"(?<![\w>.])" + re.escape(P) + r"\s*(?:[-+*/|&^]?=(?!=)|\+\+|--)", inner):
                continue                           # P itself reassigned: &P->f would go stale
            pat = re.compile(r"\b" + re.escape(P) + r"->" + re.escape(f) + r"\b(?!\s*\()")
            uses = list(pat.finditer(inner))
            if len(uses) < 2:
                continue
            decl = f"\n    __typeof__({P}->{f}) *_fv = &{P}->{f};"
            for skip_first in (True, False):
                k = 0
                def rep(mm):
                    nonlocal k
                    k += 1
                    return mm.group(0) if (skip_first and k == 1) else "(*_fv)"
                new = pat.sub(rep, inner)
                yield f"field_via_ptr:{P}.{f}{'' if skip_first else ':all'}", head + decl + new + tail
                n += 1
                if n >= 10:
                    return



def t_static_table_extern(text, name, ctx=None):
    """menuKeyGuideHelp (2026-10-13): the body defines a table the original
    keeps as a GLOBAL (`static T X[N] = {...};` / `static T X[N];` where X is
    a known symbol) -> `extern T X[];` so gcc addresses it like the target
    (the data stays in the carve). One table at a time, then all."""
    import try_fn as _tf
    try:
        unit = (ctx or {}).get("unit") or ""
        syms = _tf.symbol_addresses(unit) if unit else {}
    except Exception:
        syms = {}
    dre = re.compile(r"^[ \t]*static\s+((?:(?:const|volatile|unsigned|signed|struct|union)\s+)*\w+[\s*]*?)"
                     r"\b([A-Za-z_]\w*)\s*(\[[^\]]*\])+\s*(=\s*\{.*?\})?\s*;[ \t]*\n", re.M | re.S)
    hits = [m for m in dre.finditer(text) if not syms or m.group(2) in syms]
    if not hits:
        return
    def conv(t, ms):
        for m in sorted(ms, key=lambda m: -m.start()):
            t = t[:m.start()] + f"extern {m.group(1).strip()} {m.group(2)}[];\n" + t[m.end():]
        return t
    for m in hits:
        yield f"static_table_extern:{m.group(2)}", conv(text, [m])
    if len(hits) > 1:
        yield "static_table_extern:all", conv(text, hits)



_GSTORE = re.compile(r"^(?P<ind>\s*)(?P<g>[A-Za-z_]\w*(?:\.\w+|\[[^\]]+\])*)\s*=(?!=)\s*(?P<e>[^;]+);\s*$")


def t_global_store_ptr(fn):
    """menuMapItemDispSub (2026-10-13): two adjacent stores to globals /
    global fields `A = a; B = b;` whose lui/store order differs from the
    target -> pointer locals declared in one order and stored in the other:
    `{ __typeof__(A) *pa = &A; __typeof__(B) *pb = &B; *pb = b; *pa = a; }`
    (and declared B first). Only when a and b do not read A or B."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    locs = {n for _, n in _scalar_locals(fn)}
    n = 0
    for i in range(len(lines) - 1):
        m1, m2 = _GSTORE.match(lines[i]), _GSTORE.match(lines[i + 1])
        if not (m1 and m2) or m1["ind"] != m2["ind"]:
            continue
        A, B = m1["g"], m2["g"]
        base = lambda x: re.match(r"[A-Za-z_]\w*", x).group(0)
        if base(A) in locs or base(B) in locs or A == B:
            continue
        if re.search(r"\b" + re.escape(base(A)) + r"\b", m2["e"]) or re.search(r"\b" + re.escape(base(B)) + r"\b", m1["e"]):
            continue
        ind = m1["ind"]
        for order in ("ab", "ba"):
            d = (f"__typeof__({A}) *_pa = &{A}; __typeof__({B}) *_pb = &{B};" if order == "ab"
                 else f"__typeof__({B}) *_pb = &{B}; __typeof__({A}) *_pa = &{A};")
            blk = f"{ind}{{ {d} *_pb = {m2['e'].strip()}; *_pa = {m1['e'].strip()}; }}"
            nl = lines[:i] + [blk] + lines[i + 2:]
            yield f"global_store_ptr:L{i}:{order}", head + "\n".join(nl) + tail
            n += 1
        if n >= 10:
            return


_ELIF_SAME = re.compile(r"^(?P<ind>\s*)(?:else\s+)?if\s*\((?P<c>.+)\)\s*(?P<s>[^{};][^;]*;)\s*$")


def t_or_merge(fn):
    """Inverse of or_split (menuMapItemDispSub used `||` chains where the
    target merged the tests): `if (a) S; else if (b) S; ...` with the same
    single statement S in every arm -> `if (a || b || ...) S;`."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    i = 0
    while i < len(lines):
        m = _ELIF_SAME.match(lines[i])
        if not m or lines[i].lstrip().startswith("else"):
            i += 1
            continue
        conds, st, j = [m["c"]], m["s"].strip(), i + 1
        while j < len(lines):
            mm = _ELIF_SAME.match(lines[j])
            if not (mm and lines[j].lstrip().startswith("else") and mm["s"].strip() == st):
                break
            conds.append(mm["c"]); j += 1
        if len(conds) > 1 and not (j < len(lines) and lines[j].lstrip().startswith("else")):
            merged = f"{m['ind']}if ({' || '.join('(' + c + ')' for c in conds)}) {st}"
            yield f"or_merge:L{i}", head + "\n".join(lines[:i] + [merged] + lines[j:]) + tail
        i = j



_BITNE = re.compile(r"\(\s*\(?\s*(?P<e>[A-Za-z_][\w.\[\]>-]*(?:\s*>>\s*\d+)?)\s*&\s*(?P<k>0x[0-9A-Fa-f]+|\d+)\s*\)?\s*(?:!=\s*0)?\s*\)")


def t_bit_helper(text, name, ctx=None):
    """tgtFindFirst (2026-10-13): bit tests used as VALUES (`ok = (f & 4) != 0;`,
    returned / stored / passed) routed through `static inline int _bitt(unsigned
    v, unsigned m) { return (v & m) != 0; }` keep gcc's `andi; sltu` instead of
    a shift-and-mask. One site at a time, then every site in NAME."""
    from_fn = find_function(text, name)
    if not from_fn:
        return
    s0, b, e = from_fn
    fn = text[s0:e + 1]
    sites = [m for m in re.finditer(r"(?<![\w(!&|])(?P<all>\(\s*(?P<e>[A-Za-z_][\w.\[\]>-]*)\s*&\s*(?P<k>0x[0-9A-Fa-f]+|\d+)\s*\)\s*!=\s*0)", fn)
             if not re.match(r"\s*\)?\s*\)\s*\{|\s*\)\s*$", fn[m.end():m.end() + 4])]
    if not sites:
        return
    helper = "static inline int _bitt(unsigned int v, unsigned int m) { return (v & m) != 0; }\n"
    def build(ms):
        f2 = fn
        for m in sorted(ms, key=lambda m: -m.start()):
            f2 = f2[:m.start("all")] + f"_bitt({m['e']}, {m['k']})" + f2[m.end("all"):]
        return text[:s0] + helper + f2 + text[e + 1:]
    for i, m in enumerate(sites[:6]):
        yield f"bit_helper:{i}", build([m])
    if len(sites) > 1:
        yield "bit_helper:all", build(sites)



_RODATA_STR_CACHE: dict = {}


def _rodata_blobs(unit: str, tu: str) -> list[tuple[str, bytes]]:
    """(symbol, bytes) for every dlabel in the TU's rodata/data asm."""
    key = (unit, tu)
    if key in _RODATA_STR_CACHE:
        return _RODATA_STR_CACHE[key]
    base = (ROOT / "build" / unit / "asm" / ("main/data" if unit == "main" else f"data/{unit}"))
    out = []
    for suf in ("rodata", "data"):
        f = base / f"{tu}.{suf}.s"
        if not f.is_file():
            continue
        cur, buf = None, bytearray()
        for ln in f.read_text(errors="replace").splitlines():
            m = re.match(r"\s*dlabel\s+(\w+)", ln)
            if m:
                if cur:
                    out.append((cur, bytes(buf)))
                cur, buf = m.group(1), bytearray()
                continue
            if cur is None:
                continue
            m = re.search(r"\.word\s+(0x[0-9A-Fa-f]+|-?\d+)", ln)
            if m:
                buf += (int(m.group(1), 0) & 0xFFFFFFFF).to_bytes(4, "little"); continue
            m = re.search(r"\.(?:byte)\s+(.+)", ln)
            if m:
                for v in m.group(1).split(","):
                    buf.append(int(v.strip(), 0) & 0xFF)
                continue
            m = re.search(r'\.(asciz|ascii)\s+"((?:[^"\\]|\\.)*)"', ln)
            if m:
                buf += bytes(m.group(2), "latin-1").decode("unicode_escape").encode("latin-1")
                if m.group(1) == "asciz":
                    buf.append(0)
            if re.match(r"\s*enddlabel", ln):
                out.append((cur, bytes(buf))); cur, buf = None, bytearray()
        if cur:
            out.append((cur, bytes(buf)))
    _RODATA_STR_CACHE[key] = out
    return out


def _c_str_bytes(lit: str) -> bytes | None:
    try:
        return bytes(lit, "latin-1").decode("unicode_escape").encode("latin-1") + b"\0"
    except (UnicodeError, ValueError):
        return None


def t_string_symbol(text, name, ctx=None):
    """InitUwamonoSys (2026-10-13): string literals in NAME whose bytes are a
    known rodata symbol of this TU -> `extern const char D_xxx[];` + the
    symbol (a fresh literal lands in a new .rodata slot the carve can't
    place; the original references the existing object). One at a time,
    then all."""
    unit, tu = (ctx or {}).get("unit"), (ctx or {}).get("tu")
    loc = find_function(text, name)
    if not (unit and tu and loc):
        return
    blobs = _rodata_blobs(unit, tu)
    if not blobs:
        return
    s0, b, e = loc
    fn = text[s0:e + 1]
    hits = []
    for m in re.finditer(r'"((?:[^"\\\n]|\\.)*)"', fn):
        raw = _c_str_bytes(m.group(1))
        if not raw:
            continue
        sym = next((sy for sy, bb in blobs if bb[:len(raw)] == raw and len(bb) - len(raw) < 8), None)
        if sym:
            hits.append((m, sym))
    if not hits:
        return
    def build(hs):
        f2 = fn
        for m, sym in sorted(hs, key=lambda h: -h[0].start()):
            f2 = f2[:m.start()] + sym + f2[m.end():]
        decls = "".join(f"extern const char {sym}[];\n" for sym in dict.fromkeys(sy for _, sy in hs)
                        if not re.search(r"\b" + sym + r"\b", text[:s0]))
        return text[:s0] + decls + f2 + text[e + 1:]
    for i, h in enumerate(hits[:6]):
        yield f"string_symbol:{h[1]}", build([h])
    if len(hits) > 1:
        yield "string_symbol:all", build(hits)


_ELEM = {"char": (1, "b"), "signed char": (1, "b"), "unsigned char": (1, "B"), "u8": (1, "B"), "s8": (1, "b"),
         "short": (2, "h"), "signed short": (2, "h"), "unsigned short": (2, "H"), "u16": (2, "H"), "s16": (2, "h"),
         "int": (4, "i"), "signed int": (4, "i"), "unsigned int": (4, "I"), "u32": (4, "I"), "s32": (4, "i"),
         "float": (4, "f"), "f32": (4, "f")}


def t_init_local_table(text, name, ctx=None):
    """menuNumPut / menuEventTimerDispSub (2026-10-19): an initialized LOCAL
    array `T a[N] = {c0, ..};` that the target fills by copying a data symbol
    (lw/ldl from D_xxx, a .rodata blob with the same bytes in this TU) ->
    `T a[N];` + `*(struct{T v[N];} *)a = *(struct{T v[N];} *)D_xxx;` after the
    declarations and `extern T D_xxx[N];` above the function. gcc otherwise
    emits a private .rodata copy (relocs differ, +words). One table, then all."""
    import struct as _st
    unit, tu = (ctx or {}).get("unit"), (ctx or {}).get("tu")
    loc = find_function(text, name)
    if not (unit and tu and loc):
        return
    blobs = _rodata_blobs(unit, tu)
    if not blobs:
        return
    s0, b, e = loc
    fn = text[s0:e + 1]
    rx = re.compile(r"^([ \t]*)((?:const\s+)?((?:signed |unsigned )?(?:char|short|int|float)|u8|s8|u16|s16|u32|s32|f32))"
                    r"\s+(\w+)\s*\[\s*(\d+)\s*\]\s*=\s*\{([^{};]*)\}\s*;[ \t]*\n", re.M)
    hits = []
    for m in rx.finditer(fn):
        base = m.group(3).strip()
        if base not in _ELEM:
            continue
        size, code = _ELEM[base]
        n = int(m.group(5))
        vals = [v.strip() for v in m.group(6).split(",") if v.strip()]
        if not vals or len(vals) > n:
            continue
        try:
            nums = [float(v.rstrip("fF")) if code == "f" else int(v, 0) for v in vals] + [0] * (n - len(vals))
            fmt = "<" + code * n
            if code in "bhi":
                nums = [int(x) for x in nums]
            elif code in "BHI":
                nums = [int(x) & ((1 << (8 * size)) - 1) for x in nums]
            raw = _st.pack(fmt, *nums)
        except (ValueError, _st.error):
            continue
        sym = next((sy for sy, bb in blobs if bb[:len(raw)] == raw and len(bb) - len(raw) < 16), None)
        if sym:
            hits.append((m, sym, m.group(2).replace("const", "").strip(), m.group(4), n))
    if not hits:
        return
    def build(hs):
        f2 = fn
        copies = []
        for m, sym, ty, nm, n in sorted(hs, key=lambda h: -h[0].start()):
            f2 = f2[:m.start()] + f"{m.group(1)}{ty} {nm}[{n}];\n" + f2[m.end():]
        for m, sym, ty, nm, n in hs:
            copies.append(f"    *(struct {{ {ty} v[{n}]; }} *){nm} = *(struct {{ {ty} v[{n}]; }} *){sym};\n")
        hh, inner, tail = _split_fn(f2)
        items, first, lines = _decl_block(inner)
        ls = inner.split("\n")
        # insert after the last leading declaration line
        k = 0
        while k < len(ls) and (not ls[k].strip() or re.match(r"\s*(?:const\s+|volatile\s+|register\s+|unsigned\s+|signed\s+|struct\s+\w+\s*\*?|\w+\s*\*?)+\s*\**\w+(\s*\[[^\]]*\])*\s*;\s*$", ls[k]) or ls[k].strip() == "{"):
            k += 1
        ls[k:k] = [c.rstrip("\n") for c in copies]
        f3 = hh + "\n".join(ls) + tail
        decls = "".join(f"extern {ty} {sym}[{n}];\n" for m, sym, ty, nm, n in hs
                        if not re.search(r"\b" + sym + r"\b", text[:s0]))
        return text[:s0] + decls + f3 + text[e + 1:]
    for i, h in enumerate(hits[:4]):
        yield f"init_local_table:{h[3]}", build([h])
    if len(hits) > 1:
        yield "init_local_table:all", build(hits)


def t_drop_volatile(fn):
    """Check_MoveSquare (2026-10-13): a spilled local made `volatile` to
    force a stack slot is wrong (gcc spills it anyway when out of
    registers); old bodies use volatile locals / casts as order hacks.
    Drop `volatile` from one local declaration or cast at a time, then all."""
    head, inner, tail = _split_fn(fn)
    sites = list(re.finditer(r"\bvolatile\s+", inner))
    if not sites:
        return
    for i, m in enumerate(sites[:8]):
        yield f"drop_volatile:{i}", head + inner[:m.start()] + inner[m.end():] + tail
    if len(sites) > 1:
        yield "drop_volatile:all", head + re.sub(r"\bvolatile\s+", "", inner) + tail


TEXT_TRANSFORMS = [t_const_fix, t_void_ret, t_lit_inline, t_zext_form, t_param_widen, t_copy_inline,
                   t_unsized_extern, t_global_field_view, t_ptr_field_view, t_bitfield32_alias,
                   t_struct_elem_view, t_ret_param, t_flatten_inline_helpers,
                   t_implicit_decl_proto, t_callee_ret_cast, t_int_ptr_param,
                   t_volatile_global, t_static_table_extern, t_bit_helper, t_string_symbol, t_init_local_table]   # (text, name, ctx) -> whole text


# ---------------------------------------------------------------- fold (AI fixes: menuTgtLineDisp ..)

_EIC_ASG = re.compile(r"^\s*([A-Za-z_]\w*)\s*=(?!=)\s*([^;]+);\s*$")


def _eic_block(inner: str, p: int):
    """Parse `if (c) S` [`else if (c) S` ..] [`else S`] at P, every arm only
    plain `v = e;` assignments (braced or one statement). -> (end, [(cond |
    None, [(v, e)])]) or None."""
    arms = []
    while True:
        m = re.compile(r"if\s*\(").match(inner, p)
        if not m:
            return None
        cp = match_brace(inner, m.end() - 1)
        if cp < 0:
            return None
        cond = inner[m.end():cp].strip()
        q = cp + 1
        mb = re.compile(r"\s*\{").match(inner, q)
        if mb:
            c = match_brace(inner, mb.end() - 1)
            if c < 0:
                return None
            body = [x.strip() + ";" for x in inner[mb.end():c].split(";") if x.strip()]
            end = c + 1
        else:
            ms = re.compile(r"\s*([^;{}\n]+;)").match(inner, q)
            if not ms:
                return None
            body, end = [ms.group(1)], ms.end()
        asg = [_EIC_ASG.match(x) for x in body]
        if not asg or not all(asg) or any(x.count("(") != x.count(")") for x in body):
            return None
        arms.append((cond, [(a.group(1), a.group(2).strip()) for a in asg]))
        me = re.compile(r"\s*else\b\s*").match(inner, end)
        if not me:
            return end, arms
        p = me.end()
        if not inner.startswith("if", p) or re.match(r"\w", inner[p + 2:p + 3] or " "):
            mb = re.compile(r"\{").match(inner, p)
            if mb:
                c = match_brace(inner, mb.end() - 1)
                if c < 0:
                    return None
                body = [x.strip() + ";" for x in inner[mb.end():c].split(";") if x.strip()]
                end = c + 1
            else:
                ms = re.compile(r"([^;{}\n]+;)").match(inner, p)
                if not ms:
                    return None
                body, end = [ms.group(1)], ms.end()
            asg = [_EIC_ASG.match(x) for x in body]
            if not asg or not all(asg):
                return None
            arms.append((None, [(a.group(1), a.group(2).strip()) for a in asg]))
            return end, arms


def t_else_if_chain(fn):
    """menuTgtLineDisp (ov01/menu): defaults then conditional overwrites
    (`x = D; y = E; if (a) x = A; else if (b) y = B;`) or consecutive
    `if (a) x = A; if (b) x = B;` setting the same variables -> one
    `if / else if / else` chain where every arm assigns every variable
    (defaults in the final else). The default + overwrite form was
    if-converted to movz; the full chain kept the branches. Consecutive
    single ifs: source order and the last-wins order (`if (b) .. else if (a)`)."""
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in re.finditer(r"(?m)^([ \t]*)if\s*\(", inner):
        ind = m.group(1)
        st = m.start() + len(ind)
        if re.search(r"\belse\s*$", inner[:st]):
            continue
        r = _eic_block(inner, st)
        if not r:
            continue
        end, arms = r
        ifs = [arms]
        while True:                                    # consecutive ifs at the same indent
            mm = re.compile(r"[ \t]*\n" + re.escape(ind) + r"(?=if\s*\()").match(inner, end)
            if not mm:
                break
            r2 = _eic_block(inner, mm.end())
            if not r2:
                break
            end, a2 = r2
            ifs.append(a2)
        vs = list(dict.fromkeys(v for arm in ifs for _, asg in arm for v, _ in asg))
        # default assignments to chained variables right before the first if
        pre = inner[:m.start()].split("\n")
        k = len(pre) - 1                                # pre[-1] == "" (line start)
        dflt = []
        while k - 1 >= 0:
            ln = pre[k - 1]
            a = _EIC_ASG.match(ln)
            if not a or not ln.startswith(ind) or ln[len(ind):len(ind) + 1].isspace() \
                    or parse_decl(ln) or re.search(r"\w\s*\(", a.group(2)) \
                    or a.group(1) not in vs or a.group(1) in [v for v, _ in dflt] \
                    or any(re.search(r"\b" + re.escape(v) + r"\b", a.group(2)) for v in vs):
                break
            dflt.insert(0, (a.group(1), a.group(2).strip()))
            k -= 1
        keep = []
        vs = [v for v, _ in dflt] + [v for v in vs if v not in dict(dflt)]   # default order first
        if any(len(arm) > 1 for arm in ifs[1:]) or (len(ifs) > 1 and len(ifs[0]) > 1):
            continue                                   # consecutive: single-arm ifs only
        if any(c is None for arm in ifs for c, _ in arm) and dflt:
            continue                                   # already has an else
        if not dflt and len(ifs) < 2:
            continue
        if any(re.search(r"\b" + re.escape(v) + r"\b", c or "") for arm in ifs for c, _ in arm
               for v in vs):
            continue                                   # a condition reads a chained variable
        if not dflt and any(v not in {x for x, _ in arm[0][1]} for arm in ifs for v in vs):
            continue                                   # no default: arms must set the same vars
        dmap = dict(dflt)
        flat = [a for arm in ifs for a in arm]
        orders = [("src", flat)]
        if len(ifs) > 1:
            orders.append(("last", flat[::-1]))
        for lab, arms_ in orders:
            out = []
            for j, (c, asg) in enumerate(arms_):
                full = dict(dmap)
                full.update(asg)
                kw = "if" if j == 0 else "} else if"
                out.append(f"{ind}{kw} ({c}) {{")
                out += [f"{ind}    {v} = {full[v]};" for v in vs if v in full]
            if dflt:
                out.append(f"{ind}}} else {{")
                out += [f"{ind}    {v} = {dmap[v]};" for v in vs if v in dmap]
            out.append(f"{ind}}}")
            s0 = len("\n".join(pre[:k])) + (1 if k else 0)
            ni = inner[:s0] + "".join(x + "\n" for x in keep) + "\n".join(out) + inner[end:]
            yield f"else_if_chain:{lab}:L{inner.count(chr(10), 0, st)}", head + ni + tail
            n += 1
            if n >= 8:
                return


def t_load_alias_cast(fn, ctx=None):
    """unitJobTypeAtk (ov01/unit_cmd, 0.9664 -> 0.9916): a field / element
    read scheduled too early -> read it through a cast lvalue of another
    alias set, `x = cmd[1]` -> `x = (__typeof__(x))*(ObjectTask **)&cmd[1]`
    (struct pointer types the function already casts to, then void *).
    The load-side analogue of field_alias. Diff-gated on a moved load
    (block_barrier / field_alias / sched_weight / store_sort / de_ifcvt)."""
    c_ = ctx or {}
    if not any(c_.get(k) for k in ("block_barrier", "field_alias", "sched_weight", "store_sort",
                                   "de_ifcvt", "sched_barrier", "load_width_cast")) \
            and not (c_.get("rows") and block_barrier_shape(c_["rows"])):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    tys = list(dict.fromkeys(re.findall(r"\(\s*([A-Z]\w*)\s*\*\s*\)", fn)))[:2] + ["void"]
    rhs_re = re.compile(r"^(?:\(\s*\w+\s*\)\s*)?(?P<e>[A-Za-z_]\w*(?:\s*(?:->\s*\w+|\.\s*\w+|\[[^\[\]=]+\]))+)$")
    n = 0
    for i, ln in enumerate(lines):
        p = _stmt_lhs(ln)
        if not p or parse_decl(ln):
            continue
        ind, lhs, rhs = p
        mr = rhs_re.match(rhs)
        if not mr or re.search(r"\b(float|double)\b", rhs):
            continue
        E = mr.group("e")
        for T in tys:
            nl = list(lines)
            nl[i] = f"{ind}{lhs} = (__typeof__({lhs}))*({T} **)&{E};"
            yield f"load_alias_cast:{T}:L{i}", head + "\n".join(nl) + tail
            n += 1
        if n >= 24:
            return


_ZRUN = re.compile(r"^(?P<ind>\s*)\*\s*\(\s*(?P<T>int|s32|u32|unsigned\s+int|unsigned|long|float)\s*\*\s*\)\s*"
                   r"\(\s*\(\s*(?:unsigned\s+)?char\s*\*\s*\)\s*\(?\s*(?P<b>[A-Za-z_]\w*)\s*\)?\s*\+\s*"
                   r"(?P<o>0[xX][0-9a-fA-F]+|\d+)\s*\)\s*=\s*(?P<z>0|0\.0[fF]?)\s*;\s*$")
_ZRUN_F = re.compile(r"^(?P<ind>\s*)(?P<b>[A-Za-z_]\w*)\s*->\s*(?P<f>\w+)\s*=\s*0\s*;\s*$")


def t_int_view_zero_run(fn, ctx=None):
    """SetHideObject (main/init_uwamono_sys, 0.77 -> 0.84): a run of >= 2
    constant-zero stores to adjacent words (`*(int *)((char *)b + 0x50) = 0;`
    +4, +8 / adjacent `b->f = 0;` fields) -> one block-local view
    `{ int *z = (int *)((char *)b + 0x50); z[0] = 0; z[1] = 0; .. }` (one
    base register, other scheduling). Gated on a differing row next to a
    `sw zero` (zero_store_type) or a moved store (store_sort / sched_weight)."""
    c_ = ctx or {}
    if not any(c_.get(k) for k in ("zero_store_type", "store_sort", "sched_weight", "const_rev")):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    z = _fresh(fn, "z")
    n = 0
    i = 0
    while i < len(lines):
        m = _ZRUN.match(lines[i])
        if m:
            run = [m]
            while i + len(run) < len(lines):
                m2 = _ZRUN.match(lines[i + len(run)])
                if not (m2 and m2["b"] == m["b"] and m2["ind"] == m["ind"]
                        and int(m2["o"], 0) == int(run[-1]["o"], 0) + 4):
                    break
                run.append(m2)
            if len(run) >= 2:
                ind = m["ind"]
                for ty in ("int", "float") if "." in m["z"] else ("int",):
                    zv = "0.0f" if ty == "float" else "0"
                    blk = [f"{ind}{{", f"{ind}    {ty} *{z} = ({ty} *)((char *){m['b']} + {m['o']});"] + \
                        [f"{ind}    {z}[{k}] = {zv};" for k in range(len(run))] + [f"{ind}}}"]
                    nl = lines[:i] + blk + lines[i + len(run):]
                    yield f"int_view_zero_run:raw:{ty}:L{i}", head + "\n".join(nl) + tail
                    n += 1
            i += len(run)
            continue
        m = _ZRUN_F.match(lines[i])
        if m:
            run = [m]
            while i + len(run) < len(lines):
                m2 = _ZRUN_F.match(lines[i + len(run)])
                if not (m2 and m2["b"] == m["b"] and m2["ind"] == m["ind"]):
                    break
                run.append(m2)
            if len(run) >= 2:
                ind = m["ind"]
                blk = [f"{ind}{{", f"{ind}    int *{z} = (int *)&{m['b']}->{m['f']};"] + \
                    [f"{ind}    {z}[{k}] = 0;" for k in range(len(run))] + [f"{ind}}}"]
                nl = lines[:i] + blk + lines[i + len(run):]
                yield f"int_view_zero_run:fld:L{i}", head + "\n".join(nl) + tail
                n += 1
            i += len(run)
            continue
        if n >= 8:
            return
        i += 1



# ---------------------------------------------------------------- fold (AI fixes: CC_Kara_DisposeEvent ..)

def addr_split_shape(rows) -> list[str]:
    """Same base address split another way: `addiu a1,s1,426; lh v0,0(a1)`
    in the target vs `addiu a1,s1,400; lh v0,26(a1)` in ours (rows with the
    same mnemonic and registers, only an immediate / offset differs, on an
    addiu or a load / store). Target insns."""
    out = []
    imm = re.compile(r"-?(?:0x[0-9a-fA-F]+|\d+)")
    for a, m, b in diff_rows(rows):
        if m != "|" or not a or not b or "[" in a or "[" in b:
            continue
        mn = _mn(a)
        if mn != _mn(b) or not (mn == "addiu" or _LOAD_MN.match(mn) or STORE_RE.match(mn)):
            continue
        if imm.sub("#", a) == imm.sub("#", b) and a != b:
            out.append(a)
    return out


_BOI_ACC = re.compile(r"(?<![\w.>\]&])(?P<b>[A-Za-z_]\w*)(?P<p>(?:\s*(?:->|\.)\s*[A-Za-z_]\w*|\s*\[[^\[\]]+\])+)"
                      r"(?!\s*(?:\(|\[|->|\.|\+\+|--))")
_BOI_TOK = re.compile(r"\s*(->|\.)\s*([A-Za-z_]\w*)|\s*\[([^\[\]]+)\]")


def t_byte_offset_index(fn, ctx=None):
    """CC_Kara_DisposeEvent (ov10/battle_area_check_enemy, 0.9615 -> match):
    an indexed struct access `b->arr[i].f` / `b[i].f` in a loop -> the byte
    offset form `*(T *)((char *)b + OFF + i * STRIDE)` (gcc folds the whole
    constant offset into one addiu and loads at 0 where the typed form split
    it, `addiu 400; lh 26(a1)` vs `addiu 426; lh 0(a1)`). OFF / STRIDE are
    the layout constants spelled as offsetof / sizeof on the access's own
    types, so no layout is needed. All sites of one access shape, then each
    alone; OFF first and stride first. Gated on addr_split_shape (same insn,
    only an addiu immediate / load offset differs)."""
    if not (ctx or {}).get("byte_offset_index"):
        return
    head, inner, tail = _split_fn(fn)
    if not re.search(r"\b(for|while|do)\b", inner):
        return
    sites = []
    for m in _BOI_ACC.finditer(inner):
        B, P = m.group("b"), m.group("p")
        if B in ("sizeof", "return", "case", "else") or re.search(r"&\s*$", inner[:m.start()]):
            continue
        toks = [(t.group(1), t.group(2), t.group(3)) for t in _BOI_TOK.finditer(P)]
        var = [k for k, t in enumerate(toks)
               if t[2] is not None and not re.fullmatch(r"\s*(0[xX][0-9a-fA-F]+|\d+)\s*", t[2])]
        if len(var) != 1 or len(toks) < 2:
            continue
        k = var[0]
        I = toks[k][2].strip()
        if re.search(r"\+\+|--|[^=!<>]=(?!=)|\w\s*\(", I):
            continue

        def path(ts_, zero_at):
            return "".join(("[0]" if j == zero_at else f"[{ix.strip()}]") if ix is not None else f"{op}{f}"
                           for j, (op, f, ix) in enumerate(ts_))
        if toks[0][0] == ".":      # B is a struct object: offsets from &B
            Z, Pb = f"((__typeof__({B}) *)0)", f"(char *)&{B}"
            zpath = "->" + toks[0][1] + path(toks[1:], k - 1)
        else:                      # B->... / B[..]...: B is the pointer
            Z, Pb = f"((__typeof__(*{B}) *)0)", f"(char *){B}"
            zpath = path(toks, k)
        E = B + path(toks, -1)
        pre = B + path(toks[:k], -1)
        off = f"(int)&{Z}{zpath}"
        stride = f"sizeof({pre}[0])"
        Iw = I if re.fullmatch(r"[A-Za-z_]\w*|\d+", I) else f"({I})"
        ty = f"__typeof__({E})"
        sites.append((m.start(), m.end(), E,
                      (f"*({ty} *)({Pb} + {off} + {Iw} * {stride})",
                       f"*({ty} *)({Pb} + {Iw} * {stride} + {off})")))
    if not sites:
        return
    norm = lambda e: re.sub(r"\s+", "", e)  # noqa: E731

    def apply(sel, v):
        out, last = [], 0
        for s0, s1, E, reps in sorted(sel):
            out.append(inner[last:s0] + reps[v])
            last = s1
        return head + "".join(out) + inner[last:] + tail
    shapes = list(dict.fromkeys(re.sub(r"\[[^\[\]]*\]", "[]", norm(x[2])) for x in sites))
    n = 0
    for v in (0, 1):
        for sh in shapes[:4]:
            sel = [x for x in sites if re.sub(r"\[[^\[\]]*\]", "[]", norm(x[2])) == sh]
            yield f"byte_offset_index:all{v}:{sh[:24]}", apply(sel, v)
            n += 1
        if len(shapes) > 1:
            yield f"byte_offset_index:every{v}", apply(sites, v)
    for x in sites[:6]:
        yield f"byte_offset_index:one:{x[0]}", apply([x], 0)


def _sw_groups(body: str):
    """Top-level `case X:` / `default:` groups of a switch body ->
    [(values | None for default, statements text)] or None when a group
    falls through into the next one or breaks from somewhere else."""
    lab = re.compile(r"(?:case\s+([^:;{}]+?)|default)\s*:(?!:)")
    marks, d, j = [], 0, 0
    while j < len(body):
        c = body[j]
        if c in "\"'":
            q = c
            j += 1
            while j < len(body) and body[j] != q:
                j += 2 if body[j] == "\\" else 1
        elif c in "{(":
            d += 1
        elif c in "})":
            d -= 1
        elif d == 0 and (j == 0 or not (body[j - 1].isalnum() or body[j - 1] == "_")):
            m = lab.match(body, j)
            if m:
                marks.append((j, m.end(), m.group(1)))
                j = m.end()
                continue
        j += 1
    if not marks or body[:marks[0][0]].strip():
        return None
    groups, vals, has_dflt = [], [], False
    for k, (s, e, v) in enumerate(marks):
        stm = body[e:marks[k + 1][0] if k + 1 < len(marks) else len(body)]
        if v is None:
            has_dflt = True
        vals.append(v)
        if not stm.strip():
            if k + 1 == len(marks):
                groups.append((vals, ""))
            continue
        st = stm.rstrip()
        mb = re.search(r"\bbreak\s*;\s*$", st)
        if mb:
            st = st[:mb.start()].rstrip()
        elif k + 1 < len(marks) and not re.search(r"\b(return\b[^;]*|goto\s+\w+|continue)\s*;\s*$", st):
            return None
        if re.search(r"\bbreak\b", re.sub(r"\b(for|while|switch|do)\b.*", "", st, flags=re.S)):
            return None
        groups.append((vals, st))
        vals = []
    if None in [v for g in groups for v in g[0]] and any(len(g[0]) > 1 and None in g[0] for g in groups):
        return None
    return groups, has_dflt


def t_switch_to_ifchain(fn, ctx=None):
    """menuMapItemDispSub (ov01/menu, opcode 0.69 -> 0.90): the reverse of
    if_to_switch for a target without a jump table: `switch (e) { case a:
    case b: S; break; ... default: D; }` -> `if (e == a || e == b) { S }
    else if .. else { D }` (gcc's switch expansion orders and shares the
    compares in its own way; the if chain gives source-order compares).
    Groups that fall through are not converted; e must be side-effect free.
    Gated off when the target uses a jump table (a `jtbl` relocation)."""
    rows = (ctx or {}).get("rows") or []
    if any("jtbl" in a for a, _, _ in diff_rows(rows)):
        return
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in re.finditer(r"(?m)^(?P<ind>[ \t]*)switch\s*\(", inner):
        o = inner.index("(", m.start())
        c = match_brace(inner, o)
        if c < 0:
            continue
        E = inner[o + 1:c].strip()
        if re.search(r"\+\+|--|[^=!<>]=(?!=)|\w\s*\(", re.sub(r"^\((?:\w+\s*)+\**\)", "", E)):
            continue
        mb = re.compile(r"\s*\{").match(inner, c + 1)
        if not mb:
            continue
        bo = mb.end() - 1
        bc = match_brace(inner, bo)
        if bc < 0:
            continue
        r = _sw_groups(inner[bo + 1:bc])
        if not r:
            continue
        groups, has_dflt = r
        ind = m.group("ind")
        Ew = E if re.fullmatch(r"[\w.>\-\[\]]+", E) else f"({E})"
        conds = [g for g in groups if None not in g[0]]
        dfl = [g for g in groups if None in g[0]]
        if not conds:
            continue
        parts = []
        for k, (vals, st) in enumerate(conds):
            cnd = " || ".join(f"{Ew} == {v.strip()}" for v in vals)
            parts.append(("if" if k == 0 else " else if") + f" ({cnd}) {{" + st + "\n" + ind + "}")
        if dfl and dfl[0][1].strip():
            parts.append(" else {" + dfl[0][1] + "\n" + ind + "}")
        new = "".join(parts)
        yield f"switch_to_ifchain:{n}", head + inner[:m.start() + len(ind)] + new + inner[bc + 1:] + tail
        n += 1
        if n >= 4:
            return


_L2E_DEF = re.compile(r"^(?P<q>static\s+)?(?P<ty>(?:(?:const|volatile|unsigned|signed|struct|union)\s+)*"
                      r"[A-Za-z_]\w*(?:\s*\*+\s*|\s+)(?:const\s+)?)(?P<n>[A-Za-z_]\w*)\s*(?P<arr>(?:\[[^\]]*\])*)"
                      r"\s*(?P<init>=)?", re.M)


def t_local_def_to_extern(text, name, ctx=None):
    """scenarioBatExecPhase25 / specHensinProc (ov01/battle_init: matched once
    the data carve owned the table) and nmlModelCalcSpecularClip
    (`static const char D_004DC470[16] = "_face";` -> `extern const char
    D_004DC470[];`): a file-scope definition in the body file of a symbol the
    asm data already defines (config/symbols/<unit>.txt, or a D_<addr> name)
    becomes an `extern` declaration (sized as written, then unsized), so the
    function references the symbol and not a private .rodata/.data copy.
    Gated on such a symbol being used by NAME."""
    loc = find_function(text, name)
    if not loc:
        return
    pre = text[:loc[0]]
    fn = text[loc[0]:loc[2] + 1]
    syms = _sym_addrs((ctx or {}).get("unit") or "") if (ctx or {}).get("unit") else {}
    n = 0
    for m in _L2E_DEF.finditer(pre):
        S = m.group("n")
        if m.group("ty").split()[0] in ("extern", "typedef", "return") or "extern" in m.group("ty"):
            continue
        if not (m.group("init") or m.group("arr") or m.group("q")):
            continue
        if S not in syms and not re.fullmatch(r"D_[0-9A-Fa-f]{8}", S):
            continue
        if not re.search(r"\b" + re.escape(S) + r"\b", fn):
            continue
        if m.group("init"):
            p = m.end()
            q = re.compile(r"\s*\{").match(pre, p)
            if q:
                p = match_brace(pre, q.end() - 1)
                if p < 0:
                    continue
                p += 1
            end = pre.find(";", p)
        else:
            end = pre.find(";", m.end())
        if end < 0 or (not m.group("init") and pre[m.end():end].strip()):
            continue
        ty = re.sub(r"\s+", " ", m.group("ty")).strip()
        sep = "" if ty.endswith("*") else " "
        arr = m.group("arr")
        vs = [arr] if arr else [""]
        if arr:
            vs.append(re.sub(r"^\[[^\]]*\]", "[]", arr))
        for v in dict.fromkeys(vs):
            decl = f"extern {ty}{sep}{S}{v};"
            yield f"local_def_to_extern:{S}:{'sized' if v == arr else 'unsized'}", \
                text[:m.start()] + decl + text[end + 1:]
            n += 1
        if n >= 6:
            return


TEXT_TRANSFORMS.append(t_local_def_to_extern)


# ---- fold (2026-10-11): ret_narrow, switch_u32, struct_word_copy, store_volatile ----

_NARROW_RT = {"s16", "u16", "short", "unsigned short", "signed short", "short int", "char", "signed char",
              "unsigned char", "u8", "s8"}


def ret_narrow_shape(rows) -> bool:
    """The only diff: our extra sll/sra/srl 16|24 or andi 0xff|0xffff rows just
    before (or in the delay slot of) a `jr ra` -- the narrow return type's
    extension the target leaves to its callers."""
    dr = diff_rows(rows)
    diffs = [i for i, (a, m, c) in enumerate(dr) if m != " "]
    if not diffs:
        return False
    jr = [i for i, (a, m, c) in enumerate(dr) if c and re.match(r"jr\s+ra\b", c)]
    for i in diffs:
        a, m, c = dr[i]
        if m != ">" or a:
            return False
        mn, ops, _ = _insn_ops(c)
        ext = (mn in ("sll", "sra", "srl") and ops and ops[-1] in ("16", "24")) or \
              (mn == "andi" and ops and _ival(ops[-1]) in (0xff, 0xffff))
        if not ext or not any(-1 <= j - i <= 4 for j in jr):
            return False
    return True


def t_ret_narrow(text, name, ctx=None):
    """s16/u16/char function whose only diff is the return-value extension
    (svLoadImageList): return int instead, and keep every caller's bytes by
    flipping the TU prototype to int and rewriting the TU's call sites to
    `((OLD (*)(ARGS))name)(..)`. The TU edits go to ctx["patches"] (written as
    <name>.patch.json when kept)."""
    ctx = ctx or {}
    if not ctx.get("unit") or not ret_narrow_shape(ctx.get("rows")):
        return
    loc = find_function(text, name)
    if not loc:
        return
    s, b, e = loc
    fn = text[s:e + 1]
    rt = _fn_rettype(fn, name)
    core = " ".join(w for w in (rt or "").split() if w not in ("static", "extern", "inline"))
    if core not in _NARROW_RT:
        return
    po = fn.index("(", fn.index(name))
    args = fn[po + 1:match_brace(fn, po)].strip() or "void"
    nf = fn[:fn.index(rt)] + rt.replace(core, "int") + fn[fn.index(rt) + len(rt):]
    new = text[:s] + nf + text[e + 1:]
    new = re.sub(r"^([^\n;{}()#=]*?)\b" + re.escape(core) + r"(\s+" + re.escape(name) + r"\s*\([^;{}]*\)\s*;)",
                 lambda m: m.group(1) + "int" + m.group(2), new, flags=re.M)
    pp = _decl_patches(ctx["unit"], ctx.get("tu", ""), name, core, "int")
    rel = f"src/{ctx['unit']}/{ctx.get('tu', '')}.c"
    if pp is None or any(r != rel for r, _, _ in pp):
        return                                # a header prototype: other TUs' callers would change
    try:
        tu_text = (ROOT / rel).read_text()
    except OSError:
        return
    edits = []                                # (start, end, replacement)
    for _, old, nw in pp:
        i = tu_text.find(old)
        if i >= 0:
            edits.append((i, i + len(old), nw))
    for m in re.finditer(r"(?<![\w.>])" + re.escape(name) + r"(\s*)\(", tu_text):
        if any(a <= m.start() < z for a, z, _ in edits):
            continue
        ls = tu_text.rfind("\n", 0, m.start()) + 1
        if re.match(r"\s*(INCLUDE_ASM|INCLUDE_RODATA|#)", tu_text[ls:m.start()]) or \
                re.match(r"[^\n;{}()=]*\b" + re.escape(name) + r"\s*\([^;{}]*\)\s*[;{]", tu_text[ls:]) and \
                not re.search(r"[=(,]|\breturn\b", tu_text[ls:m.start()]):
            continue                          # a declaration / definition line, not a call
        edits.append((m.start(), m.start() + len(name), f"(({core} (*)({args})){name})"))
    pat = ctx.setdefault("patches", {})
    if edits:
        edits.sort()
        a0, z0 = edits[0][0], max(z for _, z, _ in edits)
        old = tu_text[a0:z0]
        if tu_text.count(old) != 1:
            return
        out, k = [], a0
        for a, z, r in edits:
            out.append(tu_text[k:a] + r)
            k = z
        out.append(tu_text[k:z0])
        pat[body_sha(new)] = [(rel, old, "".join(out))]
    yield f"ret_narrow:{core.replace(' ', '_')}->int", new


def t_switch_u32(fn):
    """`switch (e)` -> `switch ((unsigned int)(e))`: the selector's bounds check
    as one sltiu (no signed compare / extension before the jump table)."""
    for m in re.finditer(r"\bswitch\s*\(", fn):
        o = m.end() - 1
        c = match_brace(fn, o)
        sel = fn[o + 1:c].strip()
        if c < 0 or re.match(r"\(\s*(unsigned|u32)\b", sel):
            continue
        yield f"switch_u32:{sel[:20]}", fn[:o + 1] + f"(unsigned int)({sel})" + fn[c:]


_LV = r"[A-Za-z_]\w*(?:\s*(?:->|\.)\s*\w+|\s*\[[^\[\];]+\])*"


def t_struct_word_copy(fn, ctx=None):
    """A struct copy `a = b;` compiled to lwl/lwr/swl/swr where the target has
    lw/sw: copy it as an int-word aggregate of the same size (compile error
    unless sizeof is a multiple of 4, so it never changes what is copied).
    Gated on our-only unaligned loads (aligned_local shape)."""
    if not (ctx or {}).get("aligned_local"):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        m = re.fullmatch(r"(\s*)(\*?\s*" + _LV + r")\s*=\s*(\*?\s*" + _LV + r")\s*;\s*", ln)
        if not m or parse_decl(ln):
            continue
        ind, d, sr = m.groups()
        w = f"sizeof({d}) / 4 * (sizeof({d}) % 4 == 0 && sizeof({d}) == sizeof({sr}) ? 1 : -1)"
        nl = list(lines)
        nl[i] = (f"{ind}{{ typedef struct {{ int w[{w}]; }} W_; "
                 f"*(W_ *)&({d}) = *(W_ *)&({sr}); }}")
        yield f"struct_word_copy:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 12:
            return


def t_store_volatile(fn, ctx=None):
    """One store in another order than the target: make that store volatile
    (`*(volatile __typeof__(x) *)&(x) = e;`), pinning it against sched1 / the
    other memory ops without changing what is stored. Gated on a single moved
    store / store-order shape."""
    if not (ctx or {}).get("sched_weight") and not (ctx or {}).get("store_sort") \
            and not (ctx or {}).get("sched_barrier"):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        m = re.fullmatch(r"(\s*)(" + _LV + r"|\*\s*\([^;=]+\)\s*\w+)\s*=(?!=)\s*([^;]+);\s*", ln)
        if not m or parse_decl(ln) or not re.search(r"->|\.|\[|\*", m.group(2)):
            continue
        ind, lv, rhs = m.groups()
        nl = list(lines)
        nl[i] = f"{ind}*(volatile __typeof__({lv}) *)&({lv}) = {rhs};"
        yield f"store_volatile:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 20:
            return


def t_alias_store(fn, ctx=None):
    """Store order wrong because a nearby load is (not) hoisted across an int
    store: write that store through a type out of int's alias set. Covers the
    lvalues field_alias skips: `*p = x`, `p[i] = x`, plain globals
    (unitCmdItemSub `((void **)dst)[k]`, MenuShopModelMain one-field struct
    view on `*out`, xglDmaMFIFOSetup `void **` lvalue). Gated on a store-order
    shape; float stores are left alone."""
    c_ = ctx or {}
    if not any(c_.get(k) for k in ("field_alias", "store_sort", "sched_weight", "sched_barrier",
                                   "union_store", "zero_store_type", "hoist_load")):
        return
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    idx = []
    for i, ln in enumerate(lines):
        p = _stmt_lhs(ln)
        if not p or parse_decl(ln):
            continue
        l, r = p[1], p[2]
        if re.search(r"\b(float|double)\b|\d\.\d|\d+\.\d*f\b", r) or "volatile" in l:
            continue
        if not (l.startswith("*") or re.search(r"\[[^\]]+\]\s*$", l) or re.fullmatch(r"[A-Za-z_]\w*", l)):
            continue
        idx.append(i)
    forms = [("vpp", "{i}*(void **)&({l}) = (void *)({r});"),
             ("view", "{i}((struct {{ void *v; }} *)&({l}))->v = (void *)({r});"),
             ("iview", "{i}((struct {{ int v; }} *)&({l}))->v = ({r});")]
    sets = [[i] for i in idx][:16] + [[a, b] for k, a in enumerate(idx) for b in idx[k + 1:]][:24]
    for fl, f in forms:
        for s in sets:
            nl = list(lines)
            for i in s:
                ind, l, r = _stmt_lhs(nl[i])
                nl[i] = f.format(i=ind, l=l, r=r)
            yield f"alias_store:{fl}:" + "+".join(str(i) for i in s), head + "\n".join(nl) + tail


_SOFTDP = ("dpadd", "dpsub", "dpmul", "dpdiv", "litodp", "dptoli", "fptodp", "dptofp", "dpcmp",
           "dpneg", "dpgt", "dpge", "dplt", "dple", "dpeq", "dpne", "utodp", "dptoul")


def t_softdouble_proto(text, name, ctx=None):
    """Soft-double helper call chain missing the delay-slot nops / with float
    moves where the target passes register pairs: type the helpers' prototypes
    (and the function's float locals) as double (CheckXDamageCommand,
    0.66 -> match)."""
    loc = find_function(text, name)
    if not loc:
        return
    s, b, e = loc
    fn = text[s:e + 1]
    used = [h for h in _SOFTDP if re.search(r"\b" + h + r"\s*\(", fn)]
    if not used:
        return
    def proto_fix(t):
        for h in used:
            t = re.sub(r"(^|\n)([^\n;{}]*\b)float(\s*\**\s*" + h + r"\s*\([^;{]*\)\s*;)",
                       lambda m: m.group(1) + m.group(2) + "double" + m.group(3), t)
            t = re.sub(r"(\b" + h + r"\s*\()([^;{)]*)(\)\s*;)",
                       lambda m: m.group(1) + re.sub(r"\bfloat\b", "double", m.group(2)) + m.group(3), t)
        return t
    t1 = proto_fix(text)
    if t1 != text:
        yield "softdouble_proto:protos", t1
    loc1 = find_function(t1, name)
    if loc1:
        s1, b1, e1 = loc1
        f1 = t1[s1:e1 + 1]
        hd, inn, tl = _split_fn(f1)
        inn2 = re.sub(r"(^|\n)(\s*)float(\s+[A-Za-z_])", r"\1\2double\3", inn)
        if inn2 != inn:
            yield "softdouble_proto:locals", t1[:s1] + hd + inn2 + tl + t1[e1 + 1:]
    # no prototype in scope: declare the helpers as double at the top of the body
    hd, inn, tl = _split_fn(fn)
    decl = "".join(f"    extern double {h}();\n" for h in used if h not in ("dptoli", "dptoul"))
    decl += "".join(f"    extern int {h}();\n" for h in used if h in ("dptoli", "dptoul"))
    yield "softdouble_proto:blockdecl", text[:s] + hd + decl + inn + tl + text[e + 1:]


def _callee_float_unsafe(text: str, f: str) -> bool:
    """True when calling f without a prototype could change behaviour: it (or
    any parameter) is float/double (unprototyped calls promote float args to
    double and an int-returning cast reads v0 instead of $f0), it is a
    soft-float helper, or its declaration can't be found."""
    if f in _SOFTDP:
        return True
    decls = re.findall(r"(?:^|[;}\n])([^;{}()\n]*\b" + re.escape(f) + r"\s*\([^;{)]*\))\s*[;{]", text)
    if not decls:
        return True
    return any(re.search(r"\b(float|double|f32|f64)\b|\.\.\.", d) for d in decls)


def t_unproto_call(text, name, ctx=None):
    """Callee prototype narrows/widens arguments or forbids the extra/missing
    argument the target passes through: call through an unprototyped function
    pointer cast (scenarioBatExecPhase20/40, scenarioBatExecPhase10;
    _time_disp). One call site at a time. Only for callees whose declaration
    is all-integer/pointer (float args/returns would change behaviour), with a
    cast return type taken from the declaration."""
    loc = find_function(text, name)
    if not loc:
        return
    s, b, e = loc
    fn = text[s:e + 1]
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in re.finditer(r"(?<![\w.>])([A-Za-z_]\w*)\s*\(", inner):
        f = m.group(1)
        if f in ("if", "while", "for", "switch", "return", "sizeof", "__typeof__") \
                or f.startswith("__builtin") or f == name or _callee_float_unsafe(text, f):
            continue
        dm = re.search(r"(?:^|[;}\n])\s*(?:extern\s+|static\s+)*([^;{}()\n]*?)\b" + re.escape(f)
                       + r"\s*\([^;{)]*\)\s*[;{]", text)
        rt = re.sub(r"\b(extern|static|inline|const)\b", "", dm.group(1)).strip() if dm else "int"
        if not rt or not re.fullmatch(r"[\w\s\*]+", rt):
            continue
        new = inner[:m.start()] + f"(({rt} (*)()){f})(" + inner[m.end():]
        yield f"unproto_call:{f}:{rt.replace(' ', '')}:{m.start()}", text[:s] + head + new + tail + text[e + 1:]
        n += 1
        if n >= 24:
            return


def t_narrow_const_local(fn, ctx=None):
    """Negative small constant stored repeatedly in a loop / run: hoist it into
    a narrow local before the loop so gcc emits `li -128` (MenuShopLine:
    `signed char` local for -0x80, 0.9778 -> match). Only negative values, so
    the narrowed local holds the same value."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    items, first, _ = _decl_block(inner)
    decl_ln = [i for i in range(first) if lines[i].strip()]
    ind = re.match(r"\s*", lines[decl_ln[0]]).group(0) if decl_ln else "    "
    by_lit: dict = {}
    for i in range(first, len(lines)):
        p = _stmt_lhs(lines[i])
        if not p or parse_decl(lines[i]):
            continue
        mm = re.fullmatch(r"-\s*(0x[0-9A-Fa-f]+|\d+)", p[2].strip("() "))
        if mm:
            by_lit.setdefault(p[2].strip(), []).append(i)
    nm = next(x for x in ("_nk", "_nk1", "_nk2") if not re.search(r"\b" + x + r"\b", fn))
    n = 0
    for lit, sites in by_lit.items():
        v = -int(re.sub(r"[()\s-]", "", lit), 0)
        tys = ["signed char", "short", "int"] if v >= -128 else ["short", "int"] if v >= -32768 else ["int"]
        loop_at = next((j for j in range(sites[0], first - 1, -1)
                        if re.match(r"\s*(for|while|do)\b", lines[j])), None)
        spots = [sites[0]] + ([loop_at] if loop_at is not None else [])
        for ty in tys:
            for at in spots:
                nl = list(lines)
                for i in sites:
                    nl[i] = re.sub(re.escape(lit) + r"\s*;\s*$", f"{nm};", nl[i])
                sind = re.match(r"\s*", lines[at]).group(0)
                nl.insert(at, f"{sind}{nm} = {lit};")
                nl.insert(decl_ln[-1] + 1 if decl_ln else first, f"{ind}{ty} {nm};")
                yield f"narrow_const_local:{lit}:{ty.replace(' ', '')}:L{at}", head + "\n".join(nl) + tail
                n += 1
        if n >= 24:
            return


TEXT_TRANSFORMS.append(t_softdouble_proto)
TEXT_TRANSFORMS.append(t_unproto_call)

def t_bit_test_cast(fn, ctx=None):
    """MenuModelControl (lit_inline -> 0.884 -> match): the target keeps an extra
    `andi x,x,0xffff` on a bit test: split `if (E & K)` into a local and test it
    through an unsigned short cast (`int _bt = E & K; if ((unsigned short)_bt)`),
    plus the inline cast form. Same truth value (K fits in 16 bits)."""
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in re.finditer(r"(?m)^(\s*)if \(([^()]*?\w[^()]*?)\s*&\s*(0x[0-9A-Fa-f]{1,4}|\d{1,5})\)", inner):
        k = int(m.group(3), 0)
        if k == 0 or k > 0xFFFF:
            continue
        ind, e, ks = m.group(1), m.group(2).strip(), m.group(3)
        nm = next(x for x in ("_bt", "_bt1", "_bt2") if not re.search(r"\b" + x + r"\b", fn))
        a, z = m.start(), m.end()
        inline = inner[:a] + f"{ind}if ((unsigned short)(({e}) & {ks}))" + inner[z:]
        yield f"bit_test_cast:inline:{a}", head + inline + tail
        split = inner[:a] + f"{ind}{{ int {nm} = ({e}) & {ks}; if ((unsigned short){nm})" + inner[z:]
        # close the extra block after the if statement: find the statement end
        rest = split[a:]
        mm = re.search(r"\)\s*\{", rest[len(ind):])
        if mm:
            # braced if: match the closing brace of the if body (and an else chain is left inside)
            depth, i0 = 0, a + len(ind) + mm.end() - 1
            for j in range(i0, len(split)):
                if split[j] == "{": depth += 1
                elif split[j] == "}":
                    depth -= 1
                    if depth == 0:
                        k2 = j + 1
                        em = re.match(r"\s*else\b", split[k2:])
                        if em:
                            break
                        yield f"bit_test_cast:local:{a}", head + split[:k2] + " }" + split[k2:] + tail
                        break
        n += 1
        if n >= 8:
            return


def t_memcpy_packed(fn, ctx=None):
    """Small constant-size `__builtin_memcpy(d, s, N)` where the target copies
    with unaligned lwl/lwr + swl/swr into an aligned local: copy it through a
    packed struct of N bytes instead (menuBatObjDraw, GPT g1: 4-byte colour
    init `memcpy(col, D_00A46730, 4)` -> packed `{ int }` copy, 0.9942 -> match).
    Same bytes copied; only the access shape changes."""
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in re.finditer(r"__builtin_memcpy\s*\(\s*([^,;]+?)\s*,\s*([^,;]+?)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\)", inner):
        d, sr, k = m.group(1), m.group(2), int(m.group(3), 0)
        if not 2 <= k <= 16:
            continue
        shapes = []
        if k % 4 == 0:
            shapes.append(("int", f"int w[{k // 4}]"))
        if k % 8 == 0:
            shapes.append(("long", f"long w[{k // 8}]"))
        if k % 2 == 0:
            shapes.append(("short", f"short w[{k // 2}]"))
        shapes.append(("char", f"unsigned char w[{k}]"))
        for tag, fld in shapes:
            for packed in (True, False):
                if not packed and tag == "char":
                    continue
                attr = "__attribute__((packed)) " if packed else ""
                rep = (f"({{ typedef struct {attr}{{ {fld}; }} Mc_; "
                       f"*(Mc_ *)({d}) = *(Mc_ *)({sr}); }})")
                yield (f"memcpy_packed:{tag}{'p' if packed else ''}:{m.start()}",
                       head + inner[:m.start()] + rep + inner[m.end():] + tail)
                n += 1
        if n >= 16:
            return


def t_field_volatile(text, name, ctx=None):
    """Stores the target keeps in source order / unmerged: make the stored
    field itself volatile in a struct the body defines (MenuEtherStatusMain,
    GPT g4: `volatile short offsetX;` + `MenuEtherStRecord *volatile dispWork;`
    0.993 -> match; casting the stores volatile instead did not work). Tries
    each stored field alone, then pairs. Volatile never changes the values."""
    loc = find_function(text, name)
    if not loc:
        return
    s, b, e = loc
    fn = text[s:e + 1]
    stored = set(re.findall(r"(?:\.|->)\s*([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*(?:[-+*/&|^]|<<|>>)?=(?!=)", fn))
    pre = text[:s]
    sites = []  # (start, end, new_decl, field)
    for sm in re.finditer(r"\bstruct\b[^{;()]*\{", pre):
        depth, j = 1, sm.end()
        while j < len(pre) and depth:
            depth += {"{": 1, "}": -1}.get(pre[j], 0)
            j += 1
        for fm in re.finditer(r"(?m)^([ \t]*)([A-Za-z_][\w \t]*?)(\s*\*+\s*|\s+)([A-Za-z_]\w*)(\s*(?:\[[^\]]*\])*)\s*;", pre[sm.end():j - 1]):
            ind, ty, star, fld, arr = fm.groups()
            if fld not in stored or "volatile" in fm.group(0) or ty.strip() in ("struct", "union", "typedef"):
                continue
            new = (f"{ind}{ty}{star.rstrip()}volatile {fld}{arr};" if "*" in star
                   else f"{ind}volatile {ty}{star}{fld}{arr};")
            a = sm.end() + fm.start()
            sites.append((a, a + len(fm.group(0)), new, fld))
    if not sites:
        return
    # pairs: fields stored within 3 lines of each other in the function
    order = []
    for ln_no, ln in enumerate(fn.split("\n")):
        for f in re.findall(r"(?:\.|->)\s*([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*(?:[-+*/&|^]|<<|>>)?=(?!=)", ln):
            order.append((ln_no, f))
    near: dict = {}
    for (a, f) in order:
        for (b, g) in order:
            if f != g and 0 < b - a <= 3:
                k = frozenset((f, g))
                near[k] = min(near.get(k, 9), b - a)
    by_f: dict = {}
    for x in sites:
        by_f.setdefault(x[3], x)
    sites = list(by_f.values())
    pairs = [(near[frozenset((x[3], y[3]))], [x, y]) for i, x in enumerate(sites) for y in sites[i + 1:]
             if frozenset((x[3], y[3])) in near]
    combos = [[x] for x in sites] + [c for _, c in sorted(pairs, key=lambda q: q[0])]
    for n, combo in enumerate(combos[:48]):
        out = text
        for a, z, new, _ in sorted(combo, reverse=True):
            out = out[:a] + new + out[z:]
        yield "field_volatile:" + "+".join(c[3] for c in combo), out


def t_store_pair_swap(fn, ctx=None):
    """Two stores of the SAME value inside one run of plain stores, emitted in
    the other order (Unit_CreateUwamono, GPT g6: `*(short *)(sub + 0x2) = -1;`
    and `*(short *)(sub + 0x30) = -1;` ten lines apart -> swap them, 0.9858 ->
    match). store_sort misses these: cast-deref lvalues and runs > 8. Same
    value + only plain stores between -> final memory is unchanged. Pairs whose
    lvalue mentions an offset from the store_sort diff go first."""
    c_ = ctx or {}
    if not c_.get("store_sort"):
        return
    offs = set()
    for x in c_.get("store_sort") or []:
        m = re.search(r",\s*(-?\d+)\(", str(x))
        if m:
            offs.add(int(m.group(1)))
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    pat = re.compile(r"(\s*)([^=;{}]+?)\s*=(?!=)\s*([^;{}]+);\s*")

    def st(ln):
        m = pat.fullmatch(ln)
        if not m or parse_decl(ln) or re.search(r"\b(?!sizeof\b)[A-Za-z_]\w*\s*\(\s*[^*)]", m.group(3)) \
                or re.search(r"\+\+|--", ln) or re.search(r"[<>!+\-*/&|^%]$", m.group(2).strip()):
            return None
        return re.sub(r"\s+", "", m.group(2)), re.sub(r"\s+", "", m.group(3))

    def hit(lhs):
        nums = {int(v, 0) for v in re.findall(r"\b(0x[0-9A-Fa-f]+|\d+)\b", lhs)}
        return bool(nums & offs)

    cands = []
    i = 0
    while i < len(lines):
        if not st(lines[i]):
            i += 1
            continue
        j = i
        while j < len(lines) and j - i < 24 and st(lines[j]):
            j += 1
        run = [(k, st(lines[k])) for k in range(i, j)]
        lhss = {r[0] for _, r in run}
        for a in range(len(run)):
            for b in range(a + 1, len(run)):
                (ka, (la, ra)), (kb, (lb, rb)) = run[a], run[b]
                if ra != rb or la == lb or any(l in ra for l in lhss):
                    continue
                cands.append((-(hit(la) + hit(lb)), kb - ka, ka, kb))
        i = j
    for n, (_, _, ka, kb) in enumerate(sorted(cands)[:24]):
        nl = list(lines)
        ia = re.match(r"\s*", nl[ka]).group(0)
        ib = re.match(r"\s*", nl[kb]).group(0)
        nl[ka], nl[kb] = ib + lines[kb].strip(), ia + lines[ka].strip()
        yield f"store_pair_swap:L{ka}-L{kb}", head + "\n".join(nl) + tail


def t_addr_assign(fn, ctx=None):
    """Assign a scalar local through its address: `x = E;` ->
    `*(xp_ = &x) = E;` with a `T *xp_;` local (nmlModelLexDataCheck, GPT g7:
    `if (model->blockCount > 0) *(rp = &ret) = 0;` 0.9009 -> match; the
    address-taken return local gives the target's conditional move / branch
    shape). Conditional (`if (c) x = E;`) and lone arm assignments first,
    then plain ones. Same value stored to the same variable."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    locs = dict((n, t) for t, n in _scalar_locals(fn))
    if not locs:
        return
    pn = next(x for x in ("_ap", "_ap1", "_ap2", "_ap3") if not re.search(r"\b" + x + r"\b", fn))
    items, first, _ = _decl_block(inner)
    decl_ln = [i for i in range(first) if lines[i].strip()]
    ind = re.match(r"\s*", lines[decl_ln[0]]).group(0) if decl_ln else "    "
    cands = []
    for i, ln in enumerate(lines):
        if i < first:
            continue
        m = re.fullmatch(r"(\s*(?:if\s*\(.*\)\s*|else\s+)?)([A-Za-z_]\w*)\s*=(?!=)\s*([^;{}]+);\s*", ln)
        if not m or m.group(2) not in locs or parse_decl(ln):
            continue
        cond = bool(m.group(1).strip())
        prev = lines[i - 1].strip() if i else ""
        arm = prev.endswith(")") and prev.startswith(("if", "else")) or prev in ("{", "else")
        cands.append((0 if cond else 1 if arm else 2, i, m))
    for n, (_, i, m) in enumerate(sorted(cands, key=lambda q: q[:2])[:16]):
        v = m.group(2)
        nl = list(lines)
        nl[i] = f"{m.group(1)}*({pn} = &{v}) = {m.group(3).strip()};"
        nl.insert(decl_ln[-1] + 1 if decl_ln else first, f"{ind}{locs[v]} *{pn};")
        yield f"addr_assign:{v}:L{i}", head + "\n".join(nl) + tail


def t_union_field_store(fn, ctx=None):
    """A field / element store written through a union view:
    `db->unk18 = 0;` -> `((union { __typeof__(db->unk18) i; float f; } *)&(db->unk18))->i = 0;`
    (SCRIPT_init, GPT g10: 0.9811 -> match; the union's alias set changes
    sched order like union_store, which only takes raw-cast stores and is
    diff-gated). __typeof__ keeps the stored type, so the value is identical.
    One store at a time, <= 16, ungated."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        m = re.fullmatch(r"(\s*)([A-Za-z_]\w*(?:\s*(?:->|\.)\s*[A-Za-z_]\w*|\s*\[[^\]=;]+\])+)\s*=(?!=)\s*([^;{}]+);\s*", ln)
        if not m or parse_decl(ln) or re.search(r"\+\+|--", ln):
            continue
        ind, lv, rv = m.groups()
        nl = list(lines)
        nl[i] = f"{ind}((union {{ __typeof__({lv}) i; float f; }} *)&({lv}))->i = {rv.strip()};"
        yield f"union_field_store:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 16:
            return


def _elem_size(t: str) -> int | None:
    t = re.sub(r"\b(const|volatile)\b", "", t).strip()
    if t.endswith("**"):
        return 4
    base = t[:-1].strip() if t.endswith("*") else None
    if base is None:
        return None
    sizes = {"char": 1, "unsigned char": 1, "signed char": 1, "u8": 1, "s8": 1,
             "short": 2, "unsigned short": 2, "u16": 2, "s16": 2,
             "int": 4, "unsigned int": 4, "unsigned": 4, "u32": 4, "s32": 4, "long": 4,
             "float": 4, "long long": 8, "u64": 8, "s64": 8, "double": 8}
    return sizes.get(" ".join(base.split()))


def _match_brace(t: str, i: int) -> int:
    d = 0
    for j in range(i, len(t)):
        if t[j] == "{":
            d += 1
        elif t[j] == "}":
            d -= 1
            if d == 0:
                return j
    return -1


def t_counter_to_field(fn, ctx=None):
    """A local counter copied into a field/global after its loop -> count in
    the field itself: `n = 0; while (*src != -1) { src++; n++; } W.cnt = n;`
    -> `W.cnt = 0; while (*src != -1) { src++; W.cnt++; }` (UmnMailFolderSet,
    Opus o8 2026-10-18: gcc's load_mems promotes the field to a register and
    stores it once after the loop, giving the target's pointer copy and
    register choice; opcode 1.0). Only when the local is used solely between
    its `= 0` and the final copy, and that copy is to an lvalue with `.`/`->`/
    a global name. <= 6."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        m = re.fullmatch(r"(\s*)([A-Za-z_]\w*)\s*=\s*0\s*;\s*", ln)
        if not m:
            continue
        v = m.group(2)
        vre = r"(?<![\w.>])" + re.escape(v) + r"\b"
        for j in range(i + 1, min(len(lines), i + 40)):
            mm = re.fullmatch(r"(\s*)(" + _LV + r")\s*=\s*" + re.escape(v) + r"\s*;\s*", lines[j])
            if not mm or not re.search(r"\.|->|\[", mm.group(2)):
                continue
            lv = mm.group(2).strip()
            if re.search(vre, lv):
                break
            # v must not be used after j
            if any(re.search(vre, l) for l in lines[j + 1:]):
                break
            mid = lines[i + 1:j]
            if not any(re.search(vre, l) for l in mid):
                break
            nl = list(lines)
            nl[i] = f"{m.group(1)}{lv} = 0;"
            for k in range(i + 1, j):
                nl[k] = re.sub(vre, f"({lv})", nl[k])
            nl[j] = ""
            yield f"counter_to_field:L{i}", head + "\n".join(nl) + tail
            n += 1
            break
        if n >= 6:
            return


def t_break_to_goto(fn, ctx=None):
    """`break;` out of a loop -> `goto <label>;` with the label right after
    the loop (HddInstallCB, Opus o5 2026-10-18, 0.946 -> 0.971 on the way to a
    match: an exit `break` made gcc rotate the infinite loop at expansion and
    move the trailing block to the loop top; a goto to a label after the loop
    does not). Breaks of nested loops / switches are left alone. One loop at
    a time, <= 6."""
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in re.finditer(r"\b(for|while)\s*\(|\bdo\s*\{", inner):
        if m.group(0).startswith("do"):
            ob = inner.index("{", m.start())
        else:
            # skip the (...) header
            d, j = 0, m.end() - 1
            while j < len(inner):
                if inner[j] == "(":
                    d += 1
                elif inner[j] == ")":
                    d -= 1
                    if d == 0:
                        break
                j += 1
            k = j + 1
            while k < len(inner) and inner[k] in " \t\n":
                k += 1
            if k >= len(inner) or inner[k] != "{":
                continue
            # a `while (...)` that closes a do-while is not a loop head
            before = inner[:m.start()].rstrip()
            if m.group(1) == "while" and before.endswith("}"):
                continue
            ob = k
        cb = _match_brace(inner, ob)
        if cb < 0:
            continue
        body = inner[ob + 1:cb]
        # mask nested loop/switch bodies
        mask = list(body)
        for mm in re.finditer(r"\b(for|while|switch)\s*\(|\bdo\s*\{", body):
            o = body.find("{", mm.end() - 1)
            if o < 0:
                continue
            c = _match_brace(body, o)
            if c < 0:
                continue
            for q in range(o, c + 1):
                mask[q] = " "
        masked = "".join(mask)
        brks = [b.start() for b in re.finditer(r"\bbreak\s*;", masked)]
        if not brks:
            continue
        lab = f"_bk{n}"
        nb = body
        for b in reversed(brks):
            e = nb.index(";", b) + 1
            nb = nb[:b] + f"goto {lab};" + nb[e:]
        end = cb + 1
        if m.group(0).startswith("do"):
            sc = inner.find(";", cb)
            if sc < 0:
                continue
            end = sc + 1
        newinner = inner[:ob + 1] + nb + inner[cb:end] + f"\n{lab}: ;" + inner[end:]
        yield f"break_to_goto:{m.start()}", head + newinner + tail
        n += 1
        if n >= 6:
            return


def t_byte_index(text, name, ctx=None):
    """A scaled byte offset cast to an element pointer -> a typed index:
    `(const char **)(sztmp_6 + mode * 4)` -> `&((const char **)(sztmp_6))[mode]`
    (TitleHddInstall, Opus o4 2026-10-18: the typed index reuses the switch's
    own `mode*4` pseudo, a3 -> a0, then matched). Only when the scale equals
    the element size of the cast type AND the base is byte-typed (a char/u8
    pointer cast, or an identifier declared char/u8 pointer/array in the body
    file), so the address is unchanged. One site at a time, then all; <= 8."""
    loc = find_function(text, name)
    if not loc:
        return
    s0, e0 = loc[0], loc[2] + 1
    inner = text[s0:e0]
    pat = re.compile(r"\(\s*((?:const\s+|volatile\s+)?[A-Za-z_][\w ]*?\s*\*+)\s*\)\s*\(\s*"
                     r"(\(?[A-Za-z_][\w.\->\[\]()&* ]*?\)?)\s*\+\s*\(?\s*([A-Za-z_]\w*)\s*\*\s*(\d+)\s*\)?\s*\)")
    bytety = r"(?:const\s+)?(?:unsigned\s+char|signed\s+char|char|u8|s8)"
    def byte_base(b: str) -> bool:
        b = b.strip()
        if re.match(r"\(\s*" + bytety + r"\s*\*\s*\)", b):
            return True
        m = re.fullmatch(r"\(?\s*([A-Za-z_]\w*)\s*\)?", b)
        if not m:
            return False
        v = re.escape(m.group(1))
        return bool(re.search(r"\b" + bytety + r"\s*(?:\*\s*)?" + v + r"\s*(?:\[|;|,|=|\))", text))
    sites = [m for m in pat.finditer(inner) if _elem_size(m.group(1)) == int(m.group(4)) and byte_base(m.group(2))]
    if not sites:
        return
    def apply(ms):
        out, last = [], 0
        for m in ms:
            out.append(inner[last:m.start()])
            out.append(f"&(({m.group(1)})({m.group(2)}))[{m.group(3)}]")
            last = m.end()
        out.append(inner[last:])
        return text[:s0] + "".join(out) + text[e0:]
    for m in sites[:8]:
        yield f"byte_index:{m.start()}", apply([m])
    if len(sites) > 1:
        yield "byte_index:all", apply(sites)


def t_callsite_int_cast(text, name, ctx=None):
    """A statement call to a void function whose prototype is visible in the
    body/TU text -> called through an int-returning cast with the same
    parameter list: `MoveSlide(a, b, f);` ->
    `((int (*)(short *, short *, float))MoveSlide)(a, b, f);` (tskUmnMailFolder,
    Opus o19 2026-10-18: fixed the last register swap; the call's v0 def
    changes allocation like callee_int_ret, but works when the prototype lives
    in the TU). Prototyped params only (no float promotion change). One callee
    at a time, <= 8."""
    loc = find_function(text, name)
    if not loc:
        return
    s0, e0 = loc[0], loc[2] + 1
    fn = text[s0:e0]
    tu = ""
    try:
        if ctx and ctx.get("unit") and ctx.get("tu"):
            tu = (ROOT / "src" / ctx["unit"] / f"{ctx['tu']}.c").read_text(errors="replace")
    except OSError:
        tu = ""
    protos = {}
    for src in (text[:s0], tu):
        for m in re.finditer(r"^[ \t]*(?:extern[ \t]+|static[ \t]+)?void[ \t]+([A-Za-z_]\w*)[ \t]*\(([^;{}()]*)\)[ \t]*;", src, re.M):
            params = m.group(2).strip()
            if params and params != "void" and "..." not in params:
                protos.setdefault(m.group(1), params)
    n = 0
    for callee, params in protos.items():
        if callee == name:
            continue
        pat = re.compile(r"(^|[;{}]\s*)(" + re.escape(callee) + r")(\s*\()", re.M)
        if not pat.search(fn):
            continue
        # strip param names: keep types
        types = []
        for prm in params.split(","):
            prm = prm.strip()
            mm = re.fullmatch(r"(.*?[\s\*])([A-Za-z_]\w*)\s*", prm)
            types.append(mm.group(1).strip() if mm and not re.fullmatch(r"(unsigned|signed|int|char|short|long|float|double|void)", mm.group(2)) else prm)
        cast = f"((int (*)({', '.join(types)})){callee})"
        nf = pat.sub(lambda m: m.group(1) + cast + m.group(3), fn)
        if nf != fn:
            yield f"callsite_int_cast:{callee}", text[:s0] + nf + text[e0:]
            n += 1
            if n >= 8:
                return


def t_offset_field_view(fn, ctx=None):
    """`*(T *)((char *)&G + K)` -> a struct field view
    `((struct { char _pad[K]; T v; } *)&G)->v` (tskUmnMailInfo, Opus o18
    2026-10-18: the raw offset folded to %hi/%lo(G+K) and cost a saved register
    across the function; the field view keeps G's base register). Same
    address and type. One site at a time, then all; <= 8."""
    head, inner, tail = _split_fn(fn)
    pat = re.compile(r"\*\s*\(\s*([A-Za-z_][\w ]*?\s*\**)\s*\*\s*\)\s*\(\s*\(\s*(?:unsigned\s+)?char\s*\*\s*\)\s*&\s*([A-Za-z_]\w*)\s*\+\s*(0x[0-9A-Fa-f]+|\d+)\s*\)")
    sites = list(pat.finditer(inner))
    if not sites:
        return
    def rep(m):
        t, g, k = m.group(1).strip(), m.group(2), m.group(3)
        return f"(((struct {{ char _pad[{k}]; {t} v; }} *)&{g})->v)"
    def apply(ms):
        out, last = [], 0
        for m in ms:
            out.append(inner[last:m.start()]); out.append(rep(m)); last = m.end()
        out.append(inner[last:])
        return head + "".join(out) + tail
    for m in sites[:8]:
        yield f"offset_field_view:{m.start()}", apply([m])
    if len(sites) > 1:
        yield "offset_field_view:all", apply(sites)


def t_signed_zero_test(fn, ctx=None):
    """An unsigned byte read used only as a zero test -> read as signed char
    (`if (*(u8 *)(w + 3))` -> `if (*(signed char *)(w + 3))`, also `!x`,
    `x == 0`, `x != 0`): the target's `lb` instead of `lbu` (tskUmnMailDisp,
    UmnDataBase, Opus 2026-10-18). Zero-ness is the same. <= 8 sites."""
    head, inner, tail = _split_fn(fn)
    rd = r"\*\s*\(\s*(?:unsigned\s+char|u8)\s*\*\s*\)\s*(\((?:[^()]|\([^()]*\))*\)|[A-Za-z_][\w.\->]*)"
    pats = [re.compile(r"(\b(?:if|while)\s*\(\s*!?\s*)" + rd + r"(\s*\))"),
            re.compile(r"(\b(?:if|while)\s*\(\s*)" + rd + r"(\s*[!=]=\s*0\s*\))")]
    sites = []
    for p in pats:
        sites += list(p.finditer(inner))
    sites.sort(key=lambda m: m.start())
    if not sites:
        return
    def apply(ms):
        out, last = [], 0
        for m in ms:
            out.append(inner[last:m.start()])
            out.append(f"{m.group(1)}*(signed char *){m.group(2)}{m.group(3)}")
            last = m.end()
        out.append(inner[last:])
        return head + "".join(out) + tail
    for m in sites[:8]:
        yield f"signed_zero_test:{m.start()}", apply([m])
    if len(sites) > 1:
        yield "signed_zero_test:all", apply(sites)


def t_local_reread(fn, ctx=None):
    """A local initialised once from a field and never reassigned -> the
    field read at each use, cast to the local's type so truncation is kept
    (tskUmnMailDisp / tyaUmlDispType3Sub0, Opus 2026-10-18: re-reading `p->f`
    lets GCSE make the target's shared `lhu; move` copy + per-use sign
    extends, where a short local gets widened). Only when no call, store to
    that field name, or reassignment of the base pointer lies before the last
    use. <= 6."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        m = re.fullmatch(r"(\s*)((?:[A-Za-z_][\w ]*?\s+\**))?([A-Za-z_]\w*)\s*=\s*(\(?[A-Za-z_]\w*(?:\s*->\s*|\.)[A-Za-z_]\w*(?:\[[^\]]*\])?\)?)\s*;\s*", ln)
        if not m:
            continue
        ind, decl, v, rhs = m.groups()
        if re.search(r"(?<![\w.>])" + re.escape(v) + r"\b", rhs) or (decl and decl.strip() in ("return", "else", "case", "goto")):
            continue
        base = re.match(r"\(?([A-Za-z_]\w*)", rhs).group(1)
        fld = re.search(r"(?:->|\.)\s*([A-Za-z_]\w*)", rhs).group(1)
        vre = r"(?<![\w.>])" + re.escape(v) + r"\b"
        uses = [j for j in range(i + 1, len(lines)) if re.search(vre, lines[j])]
        if not uses:
            continue
        span = lines[i + 1:uses[-1] + 1]
        bad = False
        for idx, l in enumerate(span):
            if re.search(vre + r"\s*(=[^=]|\+\+|--|\+=|-=|\|=|&=)", l) or re.search(r"&\s*" + re.escape(v) + r"\b", l):
                bad = True; break
            if idx < len(span) - 1 and re.search(r"\b[A-Za-z_]\w*\s*\(", re.sub(r"\b(if|while|for|switch|return|sizeof)\s*\(", "", l)):
                bad = True; break
            if re.search(r"(?:->|\.)\s*" + re.escape(fld) + r"\b[^;=]*=[^=]", l) or re.search(r"(?<![\w.>])" + re.escape(base) + r"\s*(=[^=]|\+\+|--|\+=|-=)", l):
                bad = True; break
        if bad:
            continue
        cast = decl.strip() if decl else f"__typeof__({v})"
        nl = list(lines)
        nl[i] = "" if not decl else ""
        if not decl:
            nl[i] = ""
        for j in uses:
            nl[j] = re.sub(vre, f"(({cast})({rhs}))", nl[j])
        if decl:
            # keep nothing: the local is gone entirely (its only def was this line)
            pass
        yield f"local_reread:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 6:
            return


def t_elem_reread(fn, ctx=None):
    """A local initialised once from an array element (`s32 a0v = ap[0];`,
    m2c's cached copy) and never reassigned -> `ap[0]` read at each use
    (TodomeHyouka, Opus 2026-10-18, 0.9848 -> 0.9939: the original re-reads
    the stack array at every test; the cached local let gcc CSE it into a
    register, and earlier attempts fought that with volatile hacks). The
    array-element twin of local_reread. Only when no call, store to the
    array, or write to the index lies before the last use. <= 6."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        m = re.fullmatch(r"(\s*)((?:[A-Za-z_][\w ]*?\s+\**))?([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\s*\[\s*([A-Za-z_]\w*|0x[0-9A-Fa-f]+|\d+)\s*\]\s*;\s*", ln)
        if not m:
            continue
        ind, decl, v, base, idx = m.groups()
        if v in (base, idx) or (decl and decl.strip() in ("return", "else", "case", "goto")):
            continue
        rhs = f"{base}[{idx}]"
        vre = r"(?<![\w.>])" + re.escape(v) + r"\b"
        uses = [j for j in range(i + 1, len(lines)) if re.search(vre, lines[j])]
        if not uses:
            continue
        span = lines[i + 1:uses[-1] + 1]
        bad = False
        for k, l in enumerate(span):
            if re.search(vre + r"\s*(=[^=]|\+\+|--|\+=|-=|\|=|&=)", l) or re.search(r"&\s*" + re.escape(v) + r"\b", l):
                bad = True; break
            if k < len(span) - 1 and re.search(r"\b[A-Za-z_]\w*\s*\(", re.sub(r"\b(if|while|for|switch|return|sizeof)\s*\(", "", l)):
                bad = True; break
            if re.search(r"(?<![\w.>])" + re.escape(base) + r"\s*(\[[^\]]*\]\s*)?(=[^=]|\+\+|--|\+=|-=|\|=|&=)", l):
                bad = True; break
            if not re.fullmatch(r"0x[0-9A-Fa-f]+|\d+", idx) and re.search(r"(?<![\w.>])" + re.escape(idx) + r"\s*(=[^=]|\+\+|--|\+=|-=)", l):
                bad = True; break
        if bad:
            continue
        cast = decl.strip() if decl else f"__typeof__({v})"
        nl = list(lines)
        nl[i] = ""
        for j in uses:
            nl[j] = re.sub(vre, f"(({cast})({rhs}))", nl[j])
        yield f"elem_reread:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 6:
            return


def t_if_and_collapse(fn, ctx=None):
    """`if (A) { if (B) { X } }` (no else on either, nothing else in the outer
    block) -> `if ((A) && (B)) { X }` (TodomeHyouka, 2026-10-18: m2c nests
    each && operand; the flat condition changed the block layout). <= 8."""
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in re.finditer(r"\bif\s*\(", inner):
        po = m.end() - 1
        pc = match_brace(inner, po)
        if pc <= 0:
            continue
        ob = re.compile(r"\s*\{").match(inner, pc + 1)
        if not ob:
            continue
        oo = ob.end() - 1
        oc = _match_brace(inner, oo)
        if oc is None or oc < 0:
            continue
        if re.match(r"\s*else\b", inner[oc + 1:]):
            continue
        body = inner[oo + 1:oc]
        im = re.match(r"\s*if\s*\(", body)
        if not im:
            continue
        ipo = oo + 1 + im.end() - 1
        ipc = match_brace(inner, ipo)
        if ipc <= 0:
            continue
        ib = re.compile(r"\s*\{").match(inner, ipc + 1)
        if not ib:
            continue
        ioo = ib.end() - 1
        ioc = _match_brace(inner, ioo)
        if ioc is None or ioc < 0 or inner[ioc + 1:oc].strip():
            continue
        A, B = inner[po + 1:pc].strip(), inner[ipo + 1:ipc].strip()
        new = inner[:m.start()] + f"if (({A}) && ({B}))" + inner[ipc + 1:ioc + 1] + inner[oc + 1:]
        yield f"if_and_collapse:@{m.start()}", head + new + tail
        n += 1
        if n >= 8:
            return


_VOL_RD = re.compile(r"\*\s*\(\s*\(?\s*volatile\s+([A-Za-z_][\w ]*?)\s*\*\s*\)\s*\(?\s*&\s*([A-Za-z_]\w*(?:\s*(?:->|\.)\s*\w+|\s*\[[^\[\]]+\])*)\s*\)?\s*\)?")


def t_volatile_to_ptr(fn, ctx=None):
    """`*(volatile T *)&X` -> `*(__osp = &X)` through a one-shot pointer local
    (TodomeHyouka match, Opus 2026-10-18): GCSE/PRE treats `*p` as a different
    expression from X, so it stops sharing/hoisting X across blocks like
    volatile does, but combine folds it back into a plain load that reorg CAN
    put in a delay slot (volatile loads never go there). Also removes a
    non-semantic volatile. Variants: every occurrence through one shared
    pointer, and each occurrence alone. <= 8."""
    head, inner, tail = _split_fn(fn)
    occ = []
    for m in _VOL_RD.finditer(inner):
        t = m.group(0)
        if t.count("(") != t.count(")"):
            continue
        occ.append(m)
    if not occ:
        return
    def build(sel, tag):
        ty = sel[0].group(1).strip()
        out, last = [], 0
        for m in sel:
            out.append(inner[last:m.start()])
            out.append(f"(*({tag} = &{m.group(2)}))")
            last = m.end()
        out.append(inner[last:])
        body = "".join(out)
        return head + f"\n    {ty} *{tag};" + body + tail
    n = 0
    tys = {m.group(1).strip() for m in occ}
    if len(tys) == 1 and len(occ) > 1:
        yield "volatile_to_ptr:all", build(occ, "__osp")
        n += 1
    for k, m in enumerate(occ):
        yield f"volatile_to_ptr:{k}", build([m], "__osp")
        n += 1
        if n >= 8:
            return


_CHAIN_LV = r"[A-Za-z_][\w.\->\[\]\s*()]*?"


def t_assign_chain(fn, ctx=None):
    """Assignment-chain order (CardMainInit match, Opus 2026-10-18): gcc
    stores a chain `a = (b = (c = v))` innermost-first, so the chain's order
    fixes the store order (and which store lands in a jal delay slot).
    Rewrites: an existing chain reversed; two/three adjacent `X = E;` lines
    with the same simple E merged into a chain in both orders. <= 10."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    one = re.compile(r"^(\s*)(" + _CHAIN_LV + r")\s*=(?!=)\s*([^;=]+);\s*$")
    for i, ln in enumerate(lines):
        # existing chain: L1 = (L2 = (... = E));  or  L1 = L2 = E;
        flat = ln.replace("(", " ( ").replace(")", " ) ")
        if ln.count("=") - 2 * ln.count("==") >= 2 and ";" in ln and not re.search(r"\b(if|for|while|return)\b", ln):
            body = ln.strip().rstrip(";").strip()
            parts = [x.strip() for x in re.split(r"(?<![=!<>+\-*/&|^])=(?!=)", body)]
            parts = [re.sub(r"^\((.*)\)$", r"\1", p.strip()).strip() for p in parts]
            parts = [p.lstrip("(").rstrip(")").strip() if p.count("(") != p.count(")") else p for p in parts]
            if len(parts) >= 3 and all(re.fullmatch(_CHAIN_LV, p) for p in parts[:-1]):
                lv, e = parts[:-1], parts[-1]
                rev = lv[::-1]
                ind = re.match(r"\s*", ln).group(0)
                chain = e
                for x in reversed(rev):
                    chain = f"{x} = ({chain})" if chain is not e else f"{x} = {chain}"
                nl = list(lines); nl[i] = ind + chain + ";"
                yield f"assign_chain:rev@{i}", head + "\n".join(nl) + tail
                nl2 = list(lines); nl2[i] = "\n".join(f"{ind}{x} = {e};" for x in reversed(lv))
                yield f"assign_chain:split@{i}", head + "\n".join(nl2) + tail
                n += 2
                if n >= 10:
                    return
                continue
        m = one.match(ln)
        if not m:
            continue
        ind, a, e = m.group(1), m.group(2).strip(), m.group(3).strip()
        if not re.fullmatch(r"[\w.\->\[\]\s*()+\-]+", e) or "(" in e and re.search(r"\w\s*\(", e):
            continue
        grp = [a]
        j = i + 1
        while j < len(lines) and len(grp) < 3:
            m2 = one.match(lines[j])
            if not m2 or m2.group(3).strip() != e or m2.group(2).strip() in grp:
                break
            grp.append(m2.group(2).strip()); j += 1
        if len(grp) < 2 or any(re.search(r"(?<![\w.>])" + re.escape(g) + r"\b", e) for g in grp):
            continue
        for order, tag in ((grp, "fwd"), (grp[::-1], "rev")):
            chain = e
            for x in reversed(order[1:]):
                chain = f"({x} = {chain})"
            nl = list(lines)
            nl[i] = f"{ind}{order[0]} = {chain};"
            for k in range(i + 1, j):
                nl[k] = ""
            yield f"assign_chain:merge{tag}@{i}", head + "\n".join(nl) + tail
            n += 1
        if n >= 10:
            return


def t_goto_inline(fn, ctx=None):
    """`goto L;` -> a copy of the code at `L:` up to its terminating
    `break;` / `return ...;` / `goto X;` (CardPlayDispHand match, Opus
    2026-10-18: `case 4: t = 10 - timer; goto calc; default: t = timer;
    calc: ...` written out in both cases; gcc cross-jumps the copies back
    together so the asm shape stays, but reload/regs follow the target).
    The label stays for its other entries. Region <= 25 lines, brace-balanced,
    no labels/declarations inside. <= 6."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    labels = {}
    for i, ln in enumerate(lines):
        m = re.match(r"^\s*([A-Za-z_]\w*)\s*:(?!:)\s*(.*)$", ln)
        if m and m.group(1) not in ("default", "case") and not ln.strip().startswith("case "):
            labels[m.group(1)] = i
    for i, ln in enumerate(lines):
        m = re.match(r"^(\s*)goto\s+([A-Za-z_]\w*)\s*;\s*$", ln)
        if not m or m.group(2) not in labels:
            continue
        L = labels[m.group(2)]
        if L <= i:
            continue  # only forward gotos (backward ones are loops)
        rest = re.match(r"^\s*[A-Za-z_]\w*\s*:\s*(.*)$", lines[L]).group(1)
        body = ([rest] if rest.strip() else [])
        depth, end = 0, None
        for j in range(L + 1, min(L + 26, len(lines))):
            t = lines[j]
            if re.match(r"^\s*(case\b|default\s*:|[A-Za-z_]\w*\s*:(?!:))", t) and depth == 0:
                break
            depth += t.count("{") - t.count("}")
            if depth < 0:
                break
            if parse_decl(t):
                body = None
                break
            body.append(t)
            if depth == 0 and re.match(r"^\s*(break|return\b[^;]*|goto\s+\w+)\s*;\s*$", t):
                end = j
                break
        if not body or end is None:
            continue
        ind = m.group(1)
        copy = [ind + x.strip() if x.strip() else x for x in body]
        nl = lines[:i] + copy + lines[i + 1:]
        yield f"goto_inline:{m.group(2)}@{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 6:
            return


def t_select_helper(text, name, ctx=None):
    """`x = A; if (c) { x = B; }` -> `x = __selN(params...)` via a static
    inline helper `if (!(c)) return A; return B;` (and the `if (c) return B;
    return A;` order). CC_Kara_CommRequest match (Opus 2026-10-18): every
    direct form of the side select (if/else, ternary, switch...) gave
    movz/movn; the inlined helper's early return keeps the target's branch.
    `A` may be the declaration's initialiser (`T *x = A;`), with only
    declarations / blank lines between it and the if. Only when c/A/B use
    nothing but the function's parameters. <= 4."""
    fs = -1
    for mm in re.finditer(r"(?<![\w.>])" + re.escape(name) + r"\s*\(", text):
        b = text.find("{", mm.end())
        if b > 0 and ";" not in text[mm.end():b]:
            fs = mm.start()
            break
    if fs < 0:
        return
    ls = text.rfind("\n", 0, fs) + 1
    b = text.find("{", fs)
    sig = text[ls:b]
    pm = re.search(r"\(([^()]*)\)\s*$", sig.strip())
    if not pm:
        return
    ptypes = {}
    for prm in pm.group(1).split(","):
        mm = re.match(r"^\s*(.*?[\s*])([A-Za-z_]\w*)\s*$", prm)
        if mm:
            ptypes[mm.group(2)] = mm.group(1).strip()
    e = match_brace(text, b)
    if e < 0:
        return
    body = text[b:e + 1]
    lines = body.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        im = re.match(r"^(\s*)if\s*\((.+)\)\s*(\{)?\s*(?:([A-Za-z_]\w*)\s*=\s*([^;=]+);\s*(\})?)?\s*$", ln)
        if not im:
            continue
        ind, C = im.group(1), im.group(2).strip()
        if im.group(4):
            x, B, last = im.group(4), im.group(5).strip(), i
            if im.group(3) and not im.group(6):
                if i + 1 < len(lines) and lines[i + 1].strip() == "}":
                    last = i + 1
                else:
                    continue
        else:
            k = i + 1
            if not im.group(3):
                if k < len(lines) and lines[k].strip() == "{":
                    k += 1
                else:
                    continue
            if k + 1 >= len(lines):
                continue
            am = re.match(r"^\s*([A-Za-z_]\w*)\s*=\s*([^;=]+);\s*$", lines[k])
            if not am or lines[k + 1].strip() != "}":
                continue
            x, B, last = am.group(1), am.group(2).strip(), k + 1
        # the previous definition of x
        A, di, decl_init = None, None, False
        for q in range(i - 1, -1, -1):
            t = lines[q]
            if not t.strip() or t.strip() == "{":
                continue
            dm = re.match(r"^(\s*)([A-Za-z_][\w\s]*?\**)\s*\b" + re.escape(x) + r"\s*=\s*([^;=]+);\s*$", t)
            am = re.match(r"^\s*" + re.escape(x) + r"\s*=\s*([^;=]+);\s*$", t)
            if dm and dm.group(2).strip() not in ("return", "else"):
                A, di, decl_init, rty = dm.group(3).strip(), q, True, dm.group(2).strip()
                break
            if am:
                A, di = am.group(1).strip(), q
                break
            if parse_decl(t):
                continue
            break
        if A is None:
            continue
        if not decl_init:
            dm = re.search(r"^\s*([A-Za-z_][\w\s]*?\**)\s*\b" + re.escape(x) + r"\s*(?:=[^;]*)?;", body, re.M)
            if not dm:
                continue
            rty = dm.group(1).strip()
        ids = set(re.findall(r"(?<![\w.>])([A-Za-z_]\w*)\b", " ".join((A, B, C)))) - {"sizeof"}
        if not ids or not ids <= set(ptypes) or x in ids:
            continue
        args = sorted(ids, key=lambda a: list(ptypes).index(a))
        plist = ", ".join(f"{ptypes[a]} {a}" for a in args)
        for k2, hb in enumerate((f"if (!({C})) return {A}; return {B};", f"if ({C}) return {B}; return {A};")):
            hn = f"__sel{n}_{k2}"
            helper = f"static inline {rty} {hn}({plist}) {{ {hb} }}\n"
            nl = list(lines)
            if decl_init:
                nl[di] = re.sub(r"\s*=\s*[^;=]+;\s*$", ";", nl[di])
            else:
                nl[di] = ""
            nl[i] = f"{ind}{x} = {hn}({', '.join(args)});"
            for q in range(i + 1, last + 1):
                nl[q] = ""
            yield f"select_helper:{x}:{k2}", text[:ls] + helper + text[ls:b] + "\n".join(nl) + text[e + 1:]
        n += 1
        if n >= 2:
            return

def t_macro_comma(text, name, ctx=None):
    """`#define M(a) do { s1; s2; } while (0)` -> `#define M(a) (s1, s2)`
    (CaedGameWinMode match, Opus 2026-10-18: the loop notes of do-while(0)
    macros block sched1 from hoisting constants to the case start; a comma
    expression has none). Only macros whose body is plain expression
    statements (no decls / if / loops / braces inside). All macros at once,
    then each one. <= 6."""
    pat = re.compile(r"^(#define\s+\w+\s*\([^)]*\)\s*)do\s*\{(.*?)\}\s*while\s*\(\s*0\s*\)\s*$", re.M | re.S)
    hits = []
    for m in pat.finditer(text):
        body = m.group(2).replace("\\\n", " ")
        stmts = [x.strip() for x in body.split(";") if x.strip()]
        if not stmts or any(re.match(r"(if|for|while|do|switch|return|break|continue)\b", x) or "{" in x or "}" in x or parse_decl(x + ";") for x in stmts):
            continue
        hits.append((m, f"{m.group(1)}({', '.join('(' + x + ')' for x in stmts)})"))
    if not hits:
        return
    def apply(sel):
        out, last = [], 0
        for m, rep in sel:
            out.append(text[last:m.start()]); out.append(rep); last = m.end()
        out.append(text[last:])
        return "".join(out)
    if len(hits) > 1:
        yield "macro_comma:all", apply(hits)
    for k, h in enumerate(hits[:5]):
        yield f"macro_comma:{k}", apply([h])


def t_for_to_do(fn, ctx=None):
    """`for (i = K; i < N; i++) { B }` (K, N integer literals, K < N, so the
    body runs at least once) -> `i = K; do { B i++; } while (i < N);`
    (CC_Kara_SetOpe / CGPEndFaseSub, 2026-10-19: the for form had its first
    iteration peeled -- `lh CONST(sN)` then `b` into the loop middle -- the
    do-while form did not). Bodies with `continue` are skipped. <= 6."""
    head, inner, tail = _split_fn(fn)
    n = 0
    pat = re.compile(r"\bfor\s*\(\s*([A-Za-z_]\w*)\s*=\s*(-?\d+)\s*;\s*\1\s*(<|<=|!=)\s*(-?\d+)\s*;\s*(?:\1\s*\+\+|\+\+\s*\1|\1\s*\+=\s*1)\s*\)\s*\{")
    for m in pat.finditer(inner):
        v, k, op, lim = m.group(1), int(m.group(2)), m.group(3), int(m.group(4))
        if not (k < lim if op in ("<", "!=") else k <= lim):
            continue
        ob = m.end() - 1
        cb = _match_brace(inner, ob)
        if cb is None or cb < 0:
            continue
        body = inner[ob + 1:cb]
        if re.search(r"\bcontinue\b", body):
            continue
        new = f"{v} = {k}; do {{{body}    {v}++;\n}} while ({v} {op} {lim});"
        yield f"for_to_do:{v}@{m.start()}", head + inner[:m.start()] + new + inner[cb + 1:] + tail
        n += 1
        if n >= 6:
            return


def t_do_to_forbreak(fn, ctx=None):
    """`do { B } while (C);` -> `for (;;) { B if (!(C)) break; }`
    (eMessageNextGyouMaxGet, 2026-10-19: this loop shape alone moved the
    prologue `sd ra` after `li s1` as in the target). Bodies with `continue`
    are skipped (continue would skip the test). <= 6."""
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in re.finditer(r"\bdo\s*\{", inner):
        ob = m.end() - 1
        cb = _match_brace(inner, ob)
        if cb is None or cb < 0:
            continue
        wm = re.compile(r"\s*while\s*\(").match(inner, cb + 1)
        if not wm:
            continue
        po = wm.end() - 1
        pc = match_brace(inner, po)
        if pc <= 0:
            continue
        sm = re.compile(r"\s*;").match(inner, pc + 1)
        if not sm:
            continue
        cond = inner[po + 1:pc].strip()
        body = inner[ob + 1:cb]
        if cond in ("0", "1") or re.search(r"\bcontinue\b", body):
            continue
        new = f"for (;;) {{{body}    if (!({cond})) break;\n}}"
        yield f"do_to_forbreak@{m.start()}", head + inner[:m.start()] + new + inner[sm.end():] + tail
        n += 1
        if n >= 6:
            return


def t_s16_dec(fn, ctx=None):
    """`F--;` / `F -= 1;` / `F = F - 1;` on a field lvalue -> `F = (s16)F - 1;`
    (and `(s8)`): CGPBattleFaseProc / CGPDrawFaseProc / CGPSetFaseProc
    (2026-10-18/19): on a u16 field `phaseTimer - 1` compiles to
    `li at,0xffff; addu`, `(s16)phaseTimer - 1` to `addiu -1`, which also
    lets the phase stores cross-jump into one shared store. The truncating
    store makes both forms identical in value. <= 8."""
    head, inner, tail = _split_fn(fn)
    lv = r"([A-Za-z_]\w*(?:\s*(?:->|\.)\s*\w+|\s*\[[^\[\]]+\])+)"
    pats = [re.compile(r"(?<![\w.>])" + lv + r"\s*--\s*;"), re.compile(r"--\s*" + lv + r"\s*;"),
            re.compile(r"(?<![\w.>])" + lv + r"\s*-=\s*1\s*;")]
    n = 0
    for pat in pats:
        for m in pat.finditer(inner):
            F = m.group(1).strip()
            for cast in ("s16", "s8"):
                rep = f"{F} = ({cast}){F} - 1;"
                yield f"s16_dec:{cast}@{m.start()}", head + inner[:m.start()] + rep + inner[m.end():] + tail
                n += 1
                if n >= 8:
                    return


_ASM_WORDS: dict = {}


def _asm_word_pool(unit: str) -> set:
    """Every 32-bit hex value written in the unit's asm (lui/ori `0x3F80D1B8`
    halves of li.s constants, .word data): the overlay float pool."""
    if unit not in _ASM_WORDS:
        vals = set()
        root = Path(__file__).resolve().parents[2] / "build" / unit / "asm"
        for f in root.rglob("*.s"):
            try:
                for h in re.findall(r"0x([0-9A-Fa-f]{8})\b", f.read_text(errors="replace")):
                    vals.add(int(h, 16))
            except OSError:
                pass
        _ASM_WORDS[unit] = vals
    return _ASM_WORDS[unit]


def t_float_ulp(fn, ctx=None):
    """Float literal one ULP up / down (CardHelpProc, 2026-10-19: the target's
    constants were 1.0064001f / 1.4824001f; `1.0064f` rounds one ULP off).
    Each `D.Df` literal -> its float32 neighbours, printed with 9 significant
    digits. <= 10."""
    import struct
    # Only values the unit's constant pool really holds: a neighbour that no
    # .lit4 symbol has would just defeat constant sharing and change what the
    # code computes (JNT_getVal 1.00000012f for 1.0f, 2026-10-19).
    pool = None
    if ctx and ctx.get("unit"):
        try:
            from try_fn import lit_symbols
            pool = {v["bits"] for v in lit_symbols(ctx["unit"]).values() if v.get("kind") == "float"}
        except Exception:
            pool = None
        pool = (pool or set()) | _asm_word_pool(ctx["unit"])   # overlays: li.s lui/ori immediates
    if pool is None:
        return
    head, inner, tail = _split_fn(fn)
    n = 0
    for m in re.finditer(r"(?<![\w.])(\d+\.\d*|\.\d+)([eE][-+]?\d+)?[fF]\b", inner):
        val = float(m.group(1) + (m.group(2) or ""))
        if val == 0.0:
            continue
        bits = struct.unpack("<I", struct.pack("<f", val))[0]
        for d in (1, -1):
            nb = (bits + d) & 0xFFFFFFFF
            if nb not in pool:
                continue
            nv = struct.unpack("<f", struct.pack("<I", nb))[0]
            lit = f"{nv:.9g}"
            if "." not in lit and "e" not in lit:
                lit += ".0"
            yield f"float_ulp:{d:+d}@{m.start()}", head + inner[:m.start()] + lit + "f" + inner[m.end():] + tail
            n += 1
            if n >= 10:
                return


def t_fabsf(fn, ctx=None):
    """Hand-written float absolute value -> __builtin_fabsf (nmlModelAskNonLinear
    Camera, Haiku V4 2026-10-19: the target's `abs.s` only comes from fabsf;
    a negate-if-negative gives neg.s + branches). Forms:
    `(x < 0.0f) ? -x : x`, `(x >= 0.0f) ? x : -x`, `if (x < 0.0f) x = -x;`. <= 6."""
    head, inner, tail = _split_fn(fn)
    v = r"([A-Za-z_][\w.\->\[\]]*)"
    z = r"0(?:\.0*)?f?"
    pats = [
        (re.compile(r"\(?\s*" + v + r"\s*<\s*" + z + r"\s*\)?\s*\?\s*-\s*\(?\s*\1\s*\)?\s*:\s*\1\b"), lambda m: f"__builtin_fabsf({m.group(1)})"),
        (re.compile(r"\(?\s*" + v + r"\s*>=?\s*" + z + r"\s*\)?\s*\?\s*\1\s*:\s*-\s*\(?\s*\1\b\s*\)?"), lambda m: f"__builtin_fabsf({m.group(1)})"),
        (re.compile(r"\bif\s*\(\s*" + v + r"\s*<\s*" + z + r"\s*\)\s*\{?\s*\1\s*=\s*-\s*\1\s*;\s*\}?"), lambda m: f"{m.group(1)} = __builtin_fabsf({m.group(1)});"),
    ]
    n = 0
    for pat, rep in pats:
        for m in pat.finditer(inner):
            yield f"fabsf@{m.start()}", head + inner[:m.start()] + rep(m) + inner[m.end():] + tail
            n += 1
            if n >= 6:
                return


def _fn_params(head: str) -> list[str]:
    m = re.search(r"\(([^()]*)\)\s*\{?\s*$", head.strip())
    if not m or m.group(1).strip() in ("", "void"):
        return []
    out = []
    for prm in m.group(1).split(","):
        mm = re.search(r"([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*$", prm.strip())
        if mm:
            out.append(mm.group(1))
    return out


def t_param_mask_inplace(fn, ctx=None):
    """A local holding a masked/narrowed copy of a parameter -> mask the
    parameter in place: `s32 s = slot & 0xFFFF;` (uses of s) ->
    `slot &= 0xFFFF;` (uses of slot) (CC_Kara_KabeMove, Opus o23 2026-10-18,
    0.85 -> 0.9945: gives the target's `move s2,a2; andi s2,s2,0xffff` and
    fixed a param/register swap through the whole function). Only when the
    parameter is not used after the copy except through the local. <= 6."""
    head, inner, tail = _split_fn(fn)
    params = _fn_params(head)
    if not params:
        return
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        m = re.fullmatch(r"(\s*)(?:[A-Za-z_][\w ]*?\s+)?([A-Za-z_]\w*)\s*=\s*\(?\s*([A-Za-z_]\w*)\s*&\s*(0x[0-9A-Fa-f]+|\d+)\s*\)?\s*;\s*", ln)
        if not m:
            continue
        ind, v, prm, mask = m.groups()
        if prm not in params or v == prm or v in params:
            continue
        pre = r"(?<![\w.>])"
        # the local's declared type must hold the masked value unchanged
        dm = re.search(r"^[ \t]*((?:unsigned\s+|signed\s+)?(?:char|short|int|long)|[us](?:8|16|32)|u_?(?:char|short|int))\s+(?:[^;=(){}]*,\s*)?" + re.escape(v) + r"\s*[;,=\[]", inner, re.M)
        if dm:
            ty = dm.group(1)
            width = 8 if re.search(r"char|8", ty) else 16 if re.search(r"short|16", ty) else 32
            unsigned = ty.startswith(("unsigned", "u"))
            mv = int(mask, 0)
            if width < 32 and mv >= (1 << width if unsigned else 1 << (width - 1)):
                continue
        if any(re.search(pre + re.escape(prm) + r"\b", l) for l in lines[i + 1:]):
            continue
        if any(re.search(pre + re.escape(v) + r"\s*(=[^=]|\+\+|--|\+=|-=|&=|\|=)", l) for l in lines[i + 1:]):
            continue
        nl = list(lines)
        nl[i] = f"{ind}{prm} &= {mask};"
        for j in range(i + 1, len(nl)):
            nl[j] = re.sub(pre + re.escape(v) + r"\b", prm, nl[j])
        yield f"param_mask_inplace:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 6:
            return


def t_loop_counter_reuse(fn, ctx=None):
    """A later loop's own counter -> reuse an earlier loop's counter variable
    when their lifetimes don't overlap (CC_Kara_SensiMove, Opus o28
    2026-10-18: the target keeps the third loop's counter in the same stack
    slot as `k`; `for (k = 0; ...)` instead of `i` matched). Pairs of `for`
    counters A (earlier) / B (later) where A is not used after its last use
    before B's loop and B is not used outside its own loop span. <= 6."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    loops = []
    for i, ln in enumerate(lines):
        m = re.search(r"\bfor\s*\(\s*([A-Za-z_]\w*)\s*=", ln)
        if m:
            loops.append((i, m.group(1)))
    if len(loops) < 2:
        return
    pre = r"(?<![\w.>])"
    def uses(v):
        return [j for j, l in enumerate(lines) if re.search(pre + re.escape(v) + r"\b", l)]
    n = 0
    seen = set()
    for ai, a in loops:
        for bi, b in loops:
            if bi <= ai or a == b or (a, b) in seen:
                continue
            seen.add((a, b))
            ua, ub = uses(a), uses(b)
            ua_decl = [j for j in ua if parse_decl(lines[j])]
            ub_decl = [j for j in ub if parse_decl(lines[j])]
            ua_body = [j for j in ua if j not in ua_decl]
            ub_body = [j for j in ub if j not in ub_decl]
            if not ua_body or not ub_body:
                continue
            # A's uses all before B's first use, and A not used again after B starts
            if max(ua_body) >= min(ub_body):
                continue
            nl = list(lines)
            for j in ub_body:
                nl[j] = re.sub(pre + re.escape(b) + r"\b", a, nl[j])
            yield f"loop_counter_reuse:{b}->{a}", head + "\n".join(nl) + tail
            n += 1
            if n >= 6:
                return


def t_ti_copy(text, name, ctx=None):
    """A 16-byte copy -> one TImode assignment, which gcc 2.96 on the R5900
    emits as `lq`/`sq` (CardMainInit, Haiku h72 2026-10-18: `typedef int Q16
    __attribute__((mode(TI))); *(Q16 *)&pos = *(Q16 *)&campos;` gave the
    target's lq/sq where memcpy / aligned u32[4] structs gave ld/sd pairs).
    Rewrites `__builtin_memcpy(d, s, 16)` / `memcpy(d, s, 16)` statements in
    NAME; only valid when both sides are 16-byte aligned (lq/sq trap
    otherwise), so this is offered, not assumed: the score decides. <= 6."""
    loc = find_function(text, name)
    if not loc:
        return
    s0, e0 = loc[0], loc[2] + 1
    fn = text[s0:e0]
    pat = re.compile(r"(?:__builtin_)?memcpy\s*\(\s*([^,;]+?)\s*,\s*([^,;]+?)\s*,\s*(?:16|0x10)\s*\)\s*;")
    sites = list(pat.finditer(fn))
    if not sites:
        return
    td = "typedef int __ti_copy_t __attribute__((mode(TI)));\n"
    pre = text[:s0] if "__ti_copy_t" in text[:s0] else text[:s0] + td
    n = 0
    for m in sites[:6]:
        nf = fn[:m.start()] + f"*(__ti_copy_t *)({m.group(1)}) = *(__ti_copy_t *)({m.group(2)});" + fn[m.end():]
        yield f"ti_copy:{m.start()}", pre + nf + text[e0:]
        n += 1
    if len(sites) > 1:
        nf = pat.sub(lambda m: f"*(__ti_copy_t *)({m.group(1)}) = *(__ti_copy_t *)({m.group(2)});", fn)
        yield "ti_copy:all", pre + nf + text[e0:]


def t_callee_int_ret(text, name, ctx=None):
    """A `void` callee prototype in the body declared as returning `int`
    instead (menuBpBarPut, Opus o2 2026-10-18, 0.9841 -> match: `extern int
    grOpenF4(void *); ...` with the result ignored; the call's v0 def changes
    sched2's ordering of the neighbouring stores). One prototype at a time,
    then all; only body-file prototypes of functions NAME calls."""
    loc = find_function(text, name)
    if not loc:
        return
    pre, fn = text[:loc[0]], text[loc[0]:loc[2] + 1]
    pat = re.compile(r"^([ \t]*(?:extern[ \t]+)?)void([ \t]+)([A-Za-z_]\w*)([ \t]*\([^;{}]*\)[ \t]*;)", re.M)
    hits = [m for m in pat.finditer(pre) if re.search(r"\b" + re.escape(m.group(3)) + r"\s*\(", fn)]
    if not hits:
        return
    def apply(ms):
        out, last = [], 0
        for m in ms:
            out.append(pre[last:m.start()])
            out.append(f"{m.group(1)}int{m.group(2)}{m.group(3)}{m.group(4)}")
            last = m.end()
        out.append(pre[last:])
        return "".join(out) + text[loc[0]:]
    for m in hits[:8]:
        yield f"callee_int_ret:{m.group(3)}", apply([m])
    if len(hits) > 1:
        yield "callee_int_ret:all", apply(hits)


def t_elem_ptr_inline(fn, ctx=None):
    """An element pointer local used only for field stores/loads ->
    the array element written out in each statement:
    `e = &p->mounts[n]; e->type = t; e->pos = x;` ->
    `p->mounts[n].type = t; p->mounts[n].pos = x;` (RgEquipAddMount, Opus o1
    2026-10-18, 0.84 -> 0.985 -> match; same shape as the matched
    XrgParticleDriverDispReq: gcc makes one address copy per statement).
    Only when the pointer is never reassigned, every use is `e->`, and no
    variable of the element expression is assigned in between. <= 6."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        m = re.fullmatch(r"(\s*)(?:([A-Za-z_][\w\s]*\*+)\s*)?([A-Za-z_]\w*)\s*=\s*&\s*\(?([^;]*\[[^;]*\][^;]*?)\)?\s*;\s*", ln)
        if not m:
            continue
        ind, decl, var, expr = m.groups()
        if "(" in expr and ")" not in expr:
            continue
        rest = lines[i + 1:]
        uses = [j for j, l in enumerate(rest) if re.search(r"\b" + re.escape(var) + r"\b", l)]
        if not uses:
            continue
        idvars = set(re.findall(r"[A-Za-z_]\w*", expr))
        span = rest[:uses[-1] + 1]
        bad = False
        for l in span:
            if re.search(r"\b" + re.escape(var) + r"\s*(=[^=]|\+\+|--|\+=|-=)", l) or re.search(r"&\s*" + re.escape(var) + r"\b", l):
                bad = True
                break
            for v in idvars:
                if re.search(r"(?<![\w.>])" + re.escape(v) + r"\s*(=[^=]|\+\+|--|\+=|-=)", l) or re.search(r"(\+\+|--)\s*" + re.escape(v) + r"\b", l):
                    bad = True
                    break
            if bad:
                break
            for mm in re.finditer(r"\b" + re.escape(var) + r"\b", l):
                if not l[mm.end():].lstrip().startswith("->"):
                    bad = True
                    break
            if bad:
                break
        if bad:
            continue
        nl = list(lines)
        nl[i] = ""
        for j in range(uses[-1] + 1):
            nl[i + 1 + j] = re.sub(r"\b" + re.escape(var) + r"\s*->", f"({expr}).", nl[i + 1 + j])
        if not decl:
            pass
        yield f"elem_ptr_inline:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 6:
            return


def t_load_volatile(fn, ctx=None):
    """A field / element load read through a volatile view:
    `wx = task->work;` -> `wx = *(volatile __typeof__(task->work) *)&(task->work);`
    (menuStockStart, Haiku h15: 0.9839 -> match together with moving the
    statement after the float converts; a volatile load is not hoisted above
    neighbouring memory ops by sched1). Same value is read. One load at a
    time, <= 12, ungated; the hill-climb pairs it with stmt-swap moves."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        m = re.fullmatch(r"(\s*(?:[A-Za-z_][\w\s\*]*?\s+\**)?[A-Za-z_]\w*\s*=(?!=)\s*)(?:\(([^()]+)\)\s*)?(" + _LV + r")\s*;\s*", ln)
        if not m or not re.search(r"->|\.|\[", m.group(3)) or "volatile" in ln:
            continue
        pre, cast, lv = m.groups()
        c = f"({cast}) " if cast else ""
        nl = list(lines)
        nl[i] = f"{pre}{c}*(volatile __typeof__({lv}) *)&({lv});"
        yield f"load_volatile:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 12:
            return


def t_cast_store_view(fn, ctx=None):
    """An int-array element store of a cast pointer, written through a
    pointer-typed view of the array: `S->a[n] = (int)u;` ->
    `((__typeof__(u) *)(S->a))[n] = u;` (actStockMake, Haiku h11: 0.9593 ->
    match; the pointer-typed store gets a different alias set, so the
    following `u->work` load is scheduled before it). Pointers and int are
    both 4 bytes on the R5900, so the stored bits are identical. One store at
    a time, <= 12, ungated."""
    head, inner, tail = _split_fn(fn)
    lines = inner.split("\n")
    n = 0
    for i, ln in enumerate(lines):
        m = re.fullmatch(r"(\s*)(.+?)\[([^\[\]=;]+)\]\s*=(?!=)\s*\((?:int|s32|u32|unsigned int|unsigned|long)\)\s*([^;{}]+);\s*", ln)
        if not m or parse_decl(ln):
            continue
        ind, arr, idx, rv = m.groups()
        rv = rv.strip()
        if re.fullmatch(r"-?(?:0x[0-9A-Fa-f]+|\d+)[uUlL]*", rv):
            continue
        nl = list(lines)
        nl[i] = f"{ind}((__typeof__({rv}) *)({arr}))[{idx}] = {rv};"
        yield f"cast_store_view:L{i}", head + "\n".join(nl) + tail
        n += 1
        if n >= 12:
            return


def t_global_struct_view(text, name, ctx=None):
    """`extern int G;` only ever read narrowed (`(short)G`, `(unsigned short)G`,
    `(char)G`...): declare it as a struct whose first field is the narrow
    type and read `G.value` (sefAllocScheduler, GPT g16: `extern struct {
    unsigned short value, pad; } _nowEvent;` 0.963 -> match; the struct
    access changes the load's alias set / scheduling; volatile casts did
    not). Little-endian, so `.value` is the same low bytes the cast kept.
    Only when every use in the body text is such a cast."""
    loc = find_function(text, name)
    if not loc:
        return
    narrow = {"short": ("short", "short"), "unsigned short": ("unsigned short", "unsigned short"),
              "s16": ("short", "short"), "u16": ("unsigned short", "unsigned short"),
              "char": ("char", "char pad[3]"), "signed char": ("signed char", "char pad[3]"),
              "unsigned char": ("unsigned char", "char pad[3]"), "u8": ("unsigned char", "char pad[3]"),
              "s8": ("signed char", "char pad[3]")}
    n = 0
    for dm in re.finditer(r"^([ \t]*)extern\s+(?:unsigned\s+)?int\s+([A-Za-z_]\w*)\s*;[ \t]*$", text, re.M):
        g = dm.group(2)
        rest = text[:dm.start()] + text[dm.end():]
        uses = list(re.finditer(r"\b" + re.escape(g) + r"\b", rest))
        casts = list(re.finditer(r"\(\s*(" + "|".join(sorted(map(re.escape, narrow), key=len, reverse=True))
                                 + r")\s*\)\s*" + re.escape(g) + r"\b(?!\s*[\[(.]|\s*->)", rest))
        if not uses or len(casts) != len(uses):
            continue
        tys = {re.sub(r"\s+", " ", c.group(1)) for c in casts}
        if len(tys) != 1:
            continue
        vt, pad = narrow[tys.pop()]
        padf = pad if pad.startswith("char") else f"{pad} pad"
        for vol in ("", "volatile "):
            new = re.sub(r"\(\s*(?:" + "|".join(sorted(map(re.escape, narrow), key=len, reverse=True))
                         + r")\s*\)\s*" + re.escape(g) + r"\b", f"{g}.value", rest)
            decl = f"{dm.group(1)}extern {vol}struct {{ {vt} value; {padf}; }} {g};"
            out = new[:dm.start()] + decl + new[dm.start():]
            yield f"global_struct_view:{g}:{vol.strip() or 'plain'}", out
            n += 1
        if n >= 8:
            return


TEXT_TRANSFORMS.append(t_field_volatile)
TEXT_TRANSFORMS.append(t_callee_int_ret)
TEXT_TRANSFORMS.append(t_byte_index)
TEXT_TRANSFORMS.append(t_callsite_int_cast)
TEXT_TRANSFORMS.append(t_ti_copy)
TEXT_TRANSFORMS.append(t_global_struct_view)
TEXT_TRANSFORMS.append(t_select_helper)
TEXT_TRANSFORMS.append(t_macro_comma)

TRANSFORMS = [t_decls, t_rename_swap, t_stmt_swap, t_commute, t_split, t_ternary,
              t_ifelse, t_loops, t_single_exit, t_not, t_dup_tail_store,
              t_div_hoist, t_ret_local, t_field_reread, t_ptr_index, t_bit_split, t_bool_ifelse,
              t_early_return, t_via_temp, t_reload_base, t_if_to_switch, t_reuse_ptr_local,
              t_count_up_loop, t_merge_early_returns, t_derive_from_counter,
              t_global_reread, t_dup_tail_block, t_merge_local,
              t_sink_past_call, t_mul_expand, t_inline_temp, t_loop_guard,
              t_return_to_goto_out, t_dup_head_stmt, t_ternary_to_ifchain, t_float_vararg,
              t_switch_case_order, t_shared_case_label, t_drop_dead_locals, t_widen_before_or,
              t_ptr_lifetime, t_loop_index_shift, t_inline_sum_arg, t_unshadow_param,
              t_sink_into_break, t_split_base_ptr, t_ptr_field_as_int, t_explicit_default_case,
              t_local_array_scope, t_goto_loop, t_ret_to_if_return, t_param_scale_reassign,
              t_else_if_chain, t_switch_u32, t_narrow_const_local, t_bit_test_cast, t_memcpy_packed, t_addr_assign, t_union_field_store, t_cast_store_view, t_load_volatile, t_elem_ptr_inline, t_break_to_goto, t_counter_to_field, t_offset_field_view, t_signed_zero_test, t_local_reread, t_elem_reread, t_if_and_collapse, t_volatile_to_ptr, t_assign_chain, t_goto_inline, t_for_to_do, t_do_to_forbreak, t_s16_dec, t_float_ulp, t_fabsf, t_param_mask_inplace, t_loop_counter_reuse, t_alias_deref, t_or_split, t_loop_wrap, t_field_via_ptr, t_global_store_ptr, t_or_merge, t_drop_volatile]
CTX_TRANSFORMS = [t_fallthrough_label, t_regalloc_tie, t_cmp_fix, t_sched_weight, t_de_ifcvt, t_loop_live,
                  t_field_alias, t_volatile_mask, t_loop_form,
                  t_split_local, t_hoist_load, t_sched_barrier,
                  t_const_rev, t_addu_swap, t_sign_flip, t_float_cmp_neg, t_aligned_local,
                  t_store_sort, t_block_barrier, t_load_width_cast, t_global_recast, t_const_local,
                  t_cvt_unfold, t_zero_store_type, t_rel_mirror,
                  t_bit_test_hoist, t_store_width_cast, t_ptr_stride,
                  t_no_tailcall, t_builtin_math, t_shift_index, t_gcse_split,
                  t_global_local_copy, t_float_zero_store, t_early_float_const,
                  t_union_store, t_load_alias_cast, t_int_view_zero_run,
                  t_byte_offset_index, t_switch_to_ifchain, t_struct_word_copy,
                  t_store_volatile, t_alias_store, t_store_pair_swap]  # need ctx (diff shape / dumps)
TEXT_TRANSFORMS.append(t_ret_narrow)


def candidates(text: str, name: str, ctx: dict | None = None):
    loc = find_function(text, name)
    if not loc:
        return
    s, b, e = loc
    fn = text[s:e + 1]
    for t in TRANSFORMS + CTX_TRANSFORMS:
        try:
            for lab, nf in (t(fn, ctx) if t in CTX_TRANSFORMS else t(fn)):
                if nf != fn:
                    yield lab, text[:s] + nf + text[e + 1:]
        except (ValueError, AttributeError, IndexError, re.error):
            continue
    for t in TEXT_TRANSFORMS:
        try:
            for lab, nt in t(text, name, ctx):
                if nt != text:
                    yield lab, nt
        except (ValueError, AttributeError, IndexError, KeyError, re.error):
            continue

# ---------------------------------------------------------------- learned order

STATS = ROOT / "build/auto_decomp/trick_stats.json"
MARKS = ROOT / "build/auto_decomp/sweep_marks.json"
FAMILIES = ["declswap", "declreverse", "type", "ptype", "volatile", "rename", "stmtswap", "commute",
            "intcast", "split", "join", "temp", "ternary", "unternary", "ifswap", "for->while",
            "for->dowhile", "for->dowhile-nocheck", "for->goto-test", "while->for", "while->dowhile",
            "single-exit", "not->eq0", "eq0->not", "ne0->bare", "dup_tail_store", "fallthrough_label",
            "div_hoist", "regalloc_tie", "cmp_fix", "sched_weight", "de_ifcvt", "loop_live",
            "const_fix", "void_ret", "lit_inline", "zext_form", "field_alias", "volatile_mask",
            "loop_form", "split_local", "hoist_load", "sched_barrier",
            "param_widen", "copy_inline", "const_rev", "addu_swap", "sign_flip", "ret_local",
            "float_cmp_neg", "field_reread", "ptr_index", "unsized_extern", "aligned_local",
            "bit_split", "bool_ifelse", "early_return", "store_sort", "via_temp", "alias_deref", "or_split", "loop_wrap", "field_via_ptr", "static_table_extern", "global_store_ptr", "or_merge", "bit_helper", "string_symbol", "init_local_table", "drop_volatile",
            "block_barrier", "load_width_cast",
            "fallthrough_bl", "global_field_view", "ptr_field_view", "reload_base", "if_to_switch",
            "reuse_ptr_local", "count_up_loop",
            "global_recast", "merge_early_returns", "bitfield32_alias", "const_local",
            "derive_from_counter", "global_reread", "dup_tail_block", "merge_local", "struct_elem_view",
            "single_exit_ifelse",
            "sink_past_call", "mul_expand", "inline_temp", "loop_guard", "cvt_unfold", "zero_store_type",
            "return_to_goto_out", "dup_head_stmt", "ternary_to_ifchain", "rel_mirror", "float_vararg", "callee_void",
            "ret_param", "switch_case_order", "bit_test_hoist", "store_width_cast", "ptr_stride",
            "no_tailcall", "builtin_math", "shared_case_label", "shift_index",
            "drop_dead_locals", "widen_before_or", "ptr_lifetime",
            "loop_index_shift", "inline_sum_arg", "flatten_inline_helpers", "gcse_split", "unshadow_param",
            "sink_into_break", "split_base_ptr", "ptr_field_as_int", "explicit_default_case",
            "global_local_copy", "local_array_scope", "float_zero_store",
            "implicit_decl_proto", "callee_ret_cast", "early_float_const", "int_ptr_param",
            "volatile_global", "goto_loop", "ret_to_if_return", "union_store", "param_scale_reassign",
            "else_if_chain", "load_alias_cast", "int_view_zero_run",
            "byte_offset_index", "switch_to_ifchain", "local_def_to_extern",
            "ret_narrow", "switch_u32", "struct_word_copy", "store_volatile",
            "alias_store", "softdouble_proto", "unproto_call", "narrow_const_local", "bit_test_cast", "memcpy_packed", "field_volatile", "store_pair_swap", "addr_assign", "union_field_store", "global_struct_view", "cast_store_view", "load_volatile", "elem_ptr_inline", "callee_int_ret", "byte_index", "break_to_goto", "counter_to_field", "callsite_int_cast", "offset_field_view", "signed_zero_test", "local_reread", "elem_reread", "if_and_collapse", "volatile_to_ptr", "assign_chain", "goto_inline", "for_to_do", "do_to_forbreak", "s16_dec", "float_ulp", "fabsf", "select_helper", "macro_comma", "param_mask_inplace", "loop_counter_reuse", "ti_copy"]


# Proven by hand before they were automated (DrawRadarInfo matched with both):
# start them ahead of the catalogue; their real record takes over as it grows.
PRIORS = {
    "dup_tail_store": {"attempts": 10, "improvements": 1, "matches": 1, "compiles": 0},
    "regalloc_tie": {"attempts": 10, "improvements": 1, "matches": 1, "compiles": 0},
    # fileLoad (0.9891 -> match): only generated when the diff shows a wrong
    # branch condition, so a strong prior costs nothing elsewhere
    "cmp_fix": {"attempts": 4, "improvements": 2, "matches": 2, "compiles": 0},
    # mechanisms found by AI workers, not yet proven: explore early (ctx-gated)
    "sched_weight": {"attempts": 10, "improvements": 1, "matches": 0, "compiles": 0},
    "de_ifcvt": {"attempts": 10, "improvements": 1, "matches": 0, "compiles": 0},
    "loop_live": {"attempts": 20, "improvements": 1, "matches": 0, "compiles": 0},
    # third round (WORKER_LESSONS.md): proven by hand, ctx/text-gated -> strong priors
    "const_fix": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},    # subPrintRibbon
    "void_ret": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},     # _SetVersion3Extent
    "lit_inline": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},   # checkTalk
    "zext_form": {"attempts": 30, "improvements": 1, "matches": 0, "compiles": 0},   # speculative
    # fourth round: field_alias proven by hand (cmdCamLock); the other two mechanism-confirmed
    "field_alias": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},
    "volatile_mask": {"attempts": 20, "improvements": 1, "matches": 0, "compiles": 0},  # xglRenderClearEnvInit
    "loop_form": {"attempts": 20, "improvements": 1, "matches": 0, "compiles": 0},      # UmlTest
    # fifth round: split_local + hoist_load proven by hand (_HissatuPassTime);
    # sched_barrier mechanism confirmed (setCFAnglePers tail), not yet a match
    "split_local": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},
    "hoist_load": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},
    "sched_barrier": {"attempts": 10, "improvements": 1, "matches": 0, "compiles": 0},
    # sixth round: proven by hand (nmlModelDirectSend, JNT_getStaticVal, svAddImage)
    "param_widen": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},
    "copy_inline": {"attempts": 4, "improvements": 2, "matches": 1, "compiles": 0},
    "const_rev": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},
    "addu_swap": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},
    "sign_flip": {"attempts": 10, "improvements": 2, "matches": 0, "compiles": 0},
    "ret_local": {"attempts": 3, "improvements": 2, "matches": 1, "compiles": 0},
    # seventh round (2026-10-06 AI matches): diff-gated ones get strong priors,
    # ungated text patterns a moderate one (they fire on many bodies)
    "float_cmp_neg": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},   # HitCheckUwamonoAt
    "unsized_extern": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},  # PrintThumbnail
    "aligned_local": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},   # UwamonoBgmFunc
    "field_reread": {"attempts": 6, "improvements": 2, "matches": 1, "compiles": 0},    # sefCnvDeathEffectNo
    "ptr_index": {"attempts": 6, "improvements": 2, "matches": 1, "compiles": 0},       # CrossCheckActor
    "bit_split": {"attempts": 6, "improvements": 2, "matches": 1, "compiles": 0},       # objDispCur
    "bool_ifelse": {"attempts": 8, "improvements": 1, "matches": 1, "compiles": 0},     # scenarioBatExecDst
    "early_return": {"attempts": 8, "improvements": 1, "matches": 1, "compiles": 0},    # scenarioSrcEndChk
    # eighth round: diff-gated, proven by hand twice (SEQ_moveUnitSPL, SEQ_rotateUnitSPL)
    "store_sort": {"attempts": 2, "improvements": 2, "matches": 2, "compiles": 0},
    "string_symbol": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},  # InitUwamonoSys
    "drop_volatile": {"attempts": 3, "improvements": 1, "matches": 1, "compiles": 0},  # Check_MoveSquare
    "bit_helper": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},  # tgtFindFirst
    "global_store_ptr": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},  # menuMapItemDispSub
    "or_merge": {"attempts": 3, "improvements": 1, "matches": 0, "compiles": 0},
    "static_table_extern": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},  # menuKeyGuideHelp
    "field_via_ptr": {"attempts": 3, "improvements": 1, "matches": 0, "compiles": 0},  # dataEnemyMdlChk
    "or_split": {"attempts": 3, "improvements": 1, "matches": 1, "compiles": 0},   # Check_Wall
    "loop_wrap": {"attempts": 4, "improvements": 2, "matches": 2, "compiles": 0},  # nmlModelRenderDrop, command_enenpcse
    "alias_deref": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},  # _InitBattle
    "via_temp": {"attempts": 10, "improvements": 1, "matches": 0, "compiles": 0},  # MAP_serach (ungated)
    # ninth round (fold3): diff-gated, proven by hand (InitShopSymbol/InitRetSymbol/InitEvsSymbol; counterProc)
    "block_barrier": {"attempts": 3, "improvements": 3, "matches": 3, "compiles": 0},
    "load_width_cast": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},
    # tenth round (fold4, o422-o436): diff-gated ones proven by hand get strong
    # priors (MenuFaceEpidGet, Check_Undu); ungated text patterns a moderate one
    "fallthrough_bl": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},
    "global_field_view": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},
    "ptr_field_view": {"attempts": 8, "improvements": 1, "matches": 1, "compiles": 0},      # ACT_updateSequence
    "reload_base": {"attempts": 8, "improvements": 1, "matches": 1, "compiles": 0},         # subTreeLineDraw_type_3
    "if_to_switch": {"attempts": 8, "improvements": 1, "matches": 1, "compiles": 0},        # tskFileEx
    "reuse_ptr_local": {"attempts": 6, "improvements": 2, "matches": 1, "compiles": 0},     # DrillPowerOffFunc, sprt_put
    "count_up_loop": {"attempts": 6, "improvements": 2, "matches": 1, "compiles": 0},       # RSRC_dispose
    # fold5 (2026-10-07): each reproduces its hand match on the pre-fix body
    "global_recast": {"attempts": 4, "improvements": 2, "matches": 1, "compiles": 0},       # sefReleaseID
    "merge_early_returns": {"attempts": 6, "improvements": 2, "matches": 1, "compiles": 0}, # sefCreateEffect2
    "bitfield32_alias": {"attempts": 6, "improvements": 3, "matches": 1, "compiles": 0},    # RSRC_loadFileSub, MMvPlay2
    "const_local": {"attempts": 4, "improvements": 2, "matches": 1, "compiles": 0},         # sefInitScheduler
    # fold6 (2026-10-07): each reproduced its hand match on a rebuilt pre-fix body
    "derive_from_counter": {"attempts": 4, "improvements": 2, "matches": 2, "compiles": 0}, # cmdMemdump, cmdUnittbl
    "global_reread": {"attempts": 6, "improvements": 1, "matches": 1, "compiles": 0},       # cmdCamCoordGet
    "dup_tail_block": {"attempts": 8, "improvements": 1, "matches": 0, "compiles": 0},      # battleAfter
    "merge_local": {"attempts": 6, "improvements": 2, "matches": 2, "compiles": 0},         # spWepDispOn, battleExecNorm
    "struct_elem_view": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},    # menuOpenBat
    "single_exit_ifelse": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},  # keyCmdGet
    # fold7 (2026-10-07, o461-o473): each reproduced its hand match on a rebuilt pre-fix body
    "sink_past_call": {"attempts": 4, "improvements": 2, "matches": 2, "compiles": 0},      # menuOpenItm, menuOpenEth
    "mul_expand": {"attempts": 6, "improvements": 1, "matches": 1, "compiles": 0},          # menuCritObjDraw
    "inline_temp": {"attempts": 6, "improvements": 1, "matches": 1, "compiles": 0},         # menuCritObjDraw
    "loop_guard": {"attempts": 6, "improvements": 2, "matches": 2, "compiles": 0},          # JNT_setModelMatrix, menuFontPut
    "cvt_unfold": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},          # menuSelLine
    "zero_store_type": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},     # menuStockDead
    # fold8 (2026-10-07, o474-o482): each reproduced its hand match on a rebuilt pre-fix body
    "return_to_goto_out": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},  # svAnalyzeChunk
    "dup_head_stmt": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},       # ACT_updateMotionCore
    "ternary_to_ifchain": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},  # FileSaveDataPush
    "rel_mirror": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},          # actStockPick
    "float_vararg": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},        # InitMapKoware
    # fold10: callee_void matched checkTalkPoint in the sweep (0.9775 -> 1.0) and o499 cmdCamShake by hand
    "callee_void": {"attempts": 4, "improvements": 2, "matches": 1, "compiles": 0},         # xglRenderMove, checkTalkPoint
    # fold9 (o483-o495): each reproduced its hand match on a rebuilt pre-fix body
    "ret_param": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},           # ACT_createChr
    "switch_case_order": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},   # MenuItemListMake00_1
    "bit_test_hoist": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},      # FCV_getAttrAndValue
    "store_width_cast": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},    # ov01/cmd seeds
    "ptr_stride": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},          # MenuSortChange
    # fold10 (o492-o509): each reproduced its hand match on a rebuilt pre-fix body
    "no_tailcall": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},         # checkTalkPoint
    "builtin_math": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},        # Check_InsideFan
    "shared_case_label": {"attempts": 6, "improvements": 1, "matches": 1, "compiles": 0},   # cmdRmath1 (ungated)
    "shift_index": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},         # cmdAtkset
    # fold11 (o510-o523): each reproduced its hand fix on a rebuilt pre-fix body
    "drop_dead_locals": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},    # subTreeLineDraw_type_2
    "widen_before_or": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},     # endBackTexDraw
    "ptr_lifetime": {"attempts": 6, "improvements": 1, "matches": 1, "compiles": 0},        # endPrintDirectRibbon (ungated)
    # fold12 (o524-o533): each reproduced its hand fix on a rebuilt pre-fix body
    "loop_index_shift": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},    # MenuItemLineMain (ungated)
    "inline_sum_arg": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},      # ACT_loadResource (ungated)
    "flatten_inline_helpers": {"attempts": 4, "improvements": 1, "matches": 0, "compiles": 0},  # UnduDataGetHeader
    "gcse_split": {"attempts": 2, "improvements": 2, "matches": 1, "compiles": 0},          # MenuTecExMain, MenuTecListMain
    "unshadow_param": {"attempts": 4, "improvements": 1, "matches": 0, "compiles": 0},      # semantic fix (also pre-pass)
    # fold13 (o534-o549): each reproduced its hand fix on a rebuilt pre-fix body in /tmp/fold13
    "sink_into_break": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},     # camEventExec (ungated)
    "split_base_ptr": {"attempts": 4, "improvements": 2, "matches": 1, "compiles": 0},      # cmdUnitparaSub (ungated)
    "ptr_field_as_int": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},    # cmdThinkCamEvent (ungated)
    "explicit_default_case": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},  # xglFontReloadTexture
    "global_local_copy": {"attempts": 6, "improvements": 1, "matches": 1, "compiles": 0},   # xglFontReloadTexture
    "local_array_scope": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},   # MenuShopListChange00
    "float_zero_store": {"attempts": 10, "improvements": 0, "matches": 0, "compiles": 0},   # ACT_initMotion shape only
    # fold14 (o550-o569): each reproduced its hand fix on a rebuilt pre-fix body in /tmp/fold14
    "implicit_decl_proto": {"attempts": 2, "improvements": 2, "matches": 2, "compiles": 0},  # SEQ_moveUnitChr; 4 live bodies
    "callee_ret_cast": {"attempts": 4, "improvements": 1, "matches": 0, "compiles": 0},     # Check_Straight_ID seed
    "early_float_const": {"attempts": 4, "improvements": 1, "matches": 0, "compiles": 0},   # InitCfSystem
    "int_ptr_param": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},       # GetBSplineLoop
    "volatile_global": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},     # Get_EnemyIDPos
    "goto_loop": {"attempts": 8, "improvements": 1, "matches": 0, "compiles": 0},           # CalcPosContainer (ungated)
    "ret_to_if_return": {"attempts": 6, "improvements": 1, "matches": 1, "compiles": 0},    # checkClass (ungated)
    "union_store": {"attempts": 4, "improvements": 1, "matches": 0, "compiles": 0},         # TWIN_draw2
    "param_scale_reassign": {"attempts": 4, "improvements": 1, "matches": 1, "compiles": 0},  # drawVector2 (ungated)
    # AI-worker fold (/tmp/tsx2: hand fix undone, transform re-applied)
    "else_if_chain": {"attempts": 2, "improvements": 2, "matches": 2, "compiles": 0},      # menuTgtLineDisp (ungated)
    "load_alias_cast": {"attempts": 2, "improvements": 1, "matches": 0, "compiles": 0},    # unitJobTypeAtk .9664->.9916
    "int_view_zero_run": {"attempts": 2, "improvements": 1, "matches": 0, "compiles": 0},  # SetHideObject .781->.838
    # AI-worker fold 2 (/tmp/tsx3: hand fix undone, transform re-applied)
    "byte_offset_index": {"attempts": 2, "improvements": 2, "matches": 2, "compiles": 0},  # CC_Kara_DisposeEvent
    "switch_to_ifchain": {"attempts": 4, "improvements": 1, "matches": 0, "compiles": 0},  # menuMapItemDispSub .804->.839
    "local_def_to_extern": {"attempts": 2, "improvements": 1, "matches": 1, "compiles": 0},  # link-level; also pre-pass
}


def family(label: str) -> str:
    return label.split(":", 1)[0]


def _seed_stats() -> dict:
    """Seed from loop.log: every finished sweep counts as one attempt of every
    family (each round tried all of them); each applied step is an improvement;
    a matched sweep credits the family of its last step."""
    st = {f: {"attempts": 0, "improvements": 0, "matches": 0, "compiles": 0} for f in FAMILIES}
    try:
        lines = state.LOG_PATH.read_text(errors="replace").splitlines()
    except OSError:
        lines = []
    done = 0
    step = re.compile(r" SWEEP \S+ (\S+) -> [\d.]+ ")
    for ln in lines:
        if " SWEEP " not in ln:
            continue
        m = re.search(r" SWEEP \S+ done .* matched=(\w+) compiles=(\d+) ?(.*)$", ln)
        if m:
            done += 1
            steps = m.group(3).split()
            if m.group(1) == "True" and steps:
                st.setdefault(family(steps[-1].rsplit("=", 1)[0]),
                              {"attempts": 0, "improvements": 0, "matches": 0, "compiles": 0})["matches"] += 1
            continue
        m = step.search(ln)
        if m:
            f = st.setdefault(family(m.group(1)), {"attempts": 0, "improvements": 0, "matches": 0, "compiles": 0})
            f["improvements"] += 1
    for f in st.values():
        f["attempts"] = max(done, f["improvements"])
    for f, v in PRIORS.items():
        st[f] = dict(v)
    return st


def _locked_json(path: Path, update):
    """Read-modify-write PATH under an exclusive lock; update(obj) -> obj."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path.with_suffix(".lock"), "w") as lk:
        fcntl.flock(lk, fcntl.LOCK_EX)
        try:
            obj = json.loads(path.read_text())
        except (OSError, ValueError):
            obj = None
        obj = update(obj)
        if obj is not None:
            tmp = path.with_suffix(".tmp")
            tmp.write_text(json.dumps(obj, indent=1, sort_keys=True) + "\n")
            os.replace(tmp, path)
        return obj


def load_stats() -> dict:
    def up(o):
        if not (isinstance(o, dict) and o):
            return _seed_stats()
        for f, v in PRIORS.items():          # families added after the file was seeded
            o.setdefault(f, dict(v))
        return o
    return _locked_json(STATS, up)


def add_stats(delta: dict) -> None:
    def up(o):
        o = o if isinstance(o, dict) and o else _seed_stats()
        for f, d in delta.items():
            e = o.setdefault(f, {"attempts": 0, "improvements": 0, "matches": 0, "compiles": 0})
            for k, v in d.items():
                e[k] = e.get(k, 0) + v
        return o
    _locked_json(STATS, up)


def family_order(stats: dict, fams) -> list[str]:
    """Best improvement rate first; never-tried families get a small explore
    rate (ahead of never-helped ones); ties: fewer compiles per attempt."""
    def k(f):
        e = stats.get(f) or {}
        a = e.get("attempts", 0)
        rate = (e.get("improvements", 0) + 2 * e.get("matches", 0)) / a if a else 0.005
        cost = e.get("compiles", 0) / a if a else 0
        return (-rate, cost, f)
    return sorted(fams, key=k)


# A family with this many attempts and not one improvement is retired: it is
# not generated in normal sweeps (--families still runs it). Through
# 2026-10-06: declswap, rename, temp, ptype, single-exit, the loop and not/eq0
# rewrites, ternary/unternary, join -- ~1.3k of 24.5k compiles, 0 gains.
RETIRE_ATTEMPTS = 80


def retired(stats: dict, f: str) -> bool:
    e = stats.get(f) or {}
    return e.get("attempts", 0) >= RETIRE_ATTEMPTS and not e.get("improvements") and not e.get("matches")


def body_sha(text: str) -> str:
    return hashlib.sha1(text.encode()).hexdigest()


# Families a sweep without a "fams" record (older marks, by=="sweep", loop.log
# "done" lines) is known to have covered.
LEGACY_FAMILIES = FAMILIES[:FAMILIES.index("dup_tail_store")]


def mark_swept(k: str, text: str) -> None:
    def up(o):
        o = o if isinstance(o, dict) else {}
        o[k] = {"sha1": body_sha(text), "t": int(time.time()), "fams": FAMILIES}
        return o
    _locked_json(MARKS, up)


def swept_families(k: str, best_c: Path) -> set[str]:
    """Families trick_sweep already ran (to a local optimum) on BEST_C's text."""
    try:
        m = json.loads(MARKS.read_text()).get(k) or {}
        if m.get("sha1") == body_sha(best_c.read_text()):
            return set(m.get("fams") or LEGACY_FAMILIES)
    except (OSError, ValueError):
        pass
    return set(LEGACY_FAMILIES) if _legacy_swept(k, best_c) else set()


def is_swept(k: str, best_c: Path) -> bool:
    """True when trick_sweep already ran every current family on BEST_C's text."""
    return not (set(FAMILIES) - swept_families(k, best_c))


def _legacy_swept(k: str, best_c: Path) -> bool:
    try:
        text = best_c.read_text()
        j = json.loads(best_c.with_name(best_c.stem.replace(".best", "") + ".best.json").read_text())
    except (OSError, ValueError):
        j = {}
        text = None
    if text is None:
        return False
    if j.get("by") == "sweep":
        return True
    try:
        m = json.loads(MARKS.read_text()).get(k) or {}
    except (OSError, ValueError):
        m = {}
    if m.get("sha1") == body_sha(text):
        return True
    # sweeps from before the marks file: a "done" log line newer than best.c
    try:
        mt = best_c.stat().st_mtime
        for ln in state.LOG_PATH.read_text(errors="replace").splitlines():
            if f" SWEEP {k} done " in ln:
                ts = time.mktime(time.strptime(ln.split()[0], "%Y-%m-%dT%H:%M:%SZ")) - time.timezone
                if ts + 1 >= mt:
                    return True
    except (OSError, ValueError):
        pass
    return False

# ---------------------------------------------------------------- evaluation


def _init():
    try:
        os.nice(10)
    except OSError:
        pass


def _overrides(unit: str, patch) -> dict | None:
    """[(relpath, old, new)] -> try_fn overrides {file name: patched text}
    (files under src/<unit>/ only)."""
    if not patch:
        return None
    ov: dict = {}
    for rel, old, new in patch:
        p = ROOT / rel
        if p.parent != ROOT / "src" / unit:
            return None
        t = ov.get(p.name) or p.read_text()
        if old not in t:
            return None
        ov[p.name] = t.replace(old, new, 1)
    return ov


def apply_patch(unit: str, tu: str, patch) -> tuple[bool, str]:
    """Apply forward-declaration patches with verified_edit (byte-identical gate)."""
    by: dict = {}
    for rel, old, new in patch:
        by.setdefault(rel, []).append((old, new))
    for rel, reps in by.items():
        cmd = [sys.executable, "-B", str(HERE / "verified_edit.py"), "--unit", unit, "--tu", tu]
        if rel != f"src/{unit}/{tu}.c":
            cmd += ["--file", rel]
        for o, n in reps:
            cmd += ["--replace", o, n]
        p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        if p.returncode != 0:
            return False, (p.stdout + p.stderr)[-800:]
    return True, ""


def evaluate(args):
    unit, tu, name, text = args[:4]
    patch = args[4] if len(args) > 4 else None
    import try_fn
    fd, p = tempfile.mkstemp(prefix=f"sweep_{name}_", suffix=".c")
    os.write(fd, text.encode())
    os.close(fd)
    try:
        ov = _overrides(unit, patch)
        if patch and ov is None:
            return {"ok": False, "error": "patch not applicable"}
        r = try_fn.try_function(unit, tu, name, Path(p), diff_limit=100000, overrides=ov)
    except Exception as ex:  # noqa: BLE001
        return {"ok": False, "error": str(ex)[:300]}
    finally:
        os.unlink(p)
    return {k: r.get(k) for k in ("ok", "match", "bytes_match", "link_check", "match_after_link", "score",
                                  "differing_words", "target_insns", "candidate_insns",
                                  "opcode_score", "match_pending_jtbl", "stage", "diff", "gate")}


def register_only(r) -> bool:
    """Same length, (near-)identical opcode sequence: the diff is registers."""
    return bool(r and r.get("ok") and not is_match(r) and r.get("opcode_score") is not None
                and r["opcode_score"] >= 1.0 and r.get("target_insns") == r.get("candidate_insns"))


def rank(r):
    # a match (incl. match_after_link: lit/false-lo16 relocs only) beats any score:
    # it often has the same pre-link score as the non-matching body it replaces
    if not r or not r.get("ok"):
        return (0, -1, 0)
    return (1 if is_match(r) else 0, r["score"], -(r.get("differing_words") or 0))


def is_match(r):
    return bool(r and r.get("ok") and (r.get("match") or r.get("bytes_match") or r.get("link_check")
                                       or r.get("match_after_link")))


def gate_ok(r, cur_r) -> bool:
    """Correctness gate for a climb step: a match, a clean gate, or the same
    gate failures as the current body (never introduce a new difference in
    calls / store counts / data symbols vs the target)."""
    return is_match(r) or not r.get("gate") or r.get("gate") == (cur_r or {}).get("gate")


def integrate(unit, tu, name, body: Path, skip_try: bool = False) -> tuple[bool, str]:
    p = subprocess.run([sys.executable, str(HERE / "integrate.py"), "--unit", unit, "--tu", tu,
                        "--name", name, "--body", str(body)] + (["--skip-try"] if skip_try else []),
                       cwd=ROOT, capture_output=True, text=True)
    out = p.stdout + p.stderr
    return ('"matched": true' in out or '"already_matched": true' in out), out[-2000:]


# Families that change what the code means (a comparison, a constant, the return
# type): a higher score on a near-miss is no evidence the edit is right, so their
# edits are only ever kept when they make the function match.
SEMANTIC = {"cmp_fix", "const_fix", "void_ret"}


def sweep_one(c: dict, pool, budget: int, only_new: bool = True, only_fams=None) -> dict:
    unit, tu, name = c["unit"], c["tu"], c["name"]
    k = c["key"]
    best_c = BODIES / unit / tu / f"{name}.best.c"
    text = best_c.read_text()
    if ASM_RE.search(text):
        return {"key": k, "error": "inline asm body skipped", "matched": False}
    base = pool.submit(evaluate, (unit, tu, name, text)).result()
    used = 1
    res = {"key": k, "start": base.get("score"), "applied": [], "matched": False}
    if not base.get("ok"):
        res["error"] = "baseline does not compile"
        if not only_fams:
            mark_swept(k, text)      # nothing to sweep until the text changes (--watch would retry)
        return res
    cur, cur_r = text, base
    cur_patch: list = []
    if not is_match(base) and (not only_fams or "drop_dead_locals" in only_fams or "unshadow_param" in only_fams):
        # fold11 cleanup pre-pass: never-used locals out first (o516: unused
        # aggregates keep stack slots); kept when not worse. Fold12: also locals
        # that shadow a parameter (draft_fix `int arg0;`: the body read garbage)
        try:
            loc = find_function(text, name)
            dl = dead_locals(text[loc[0]:loc[2] + 1]) if loc else []
            dl += [x for x in (shadow_locals(text[loc[0]:loc[2] + 1]) if loc else []) if x not in dl]
            if dl:
                nt = text[:loc[0]] + _drop_decls(text[loc[0]:loc[2] + 1], dl) + text[loc[2] + 1:]
                r0 = pool.submit(evaluate, (unit, tu, name, nt)).result()
                used += 1
                if r0.get("ok") and rank(r0) >= rank(base):
                    cur, cur_r = nt, r0
                    res["applied"].append(f"drop_dead_locals:pre={r0['score']}")
                    if rank(r0) > rank(base):
                        add_stats({"drop_dead_locals": {"attempts": 1, "improvements": 1,
                                                        "matches": int(is_match(r0)), "compiles": 1}})
        except Exception as ex:  # noqa: BLE001
            res["predrop_error"] = str(ex)[:200]
    if not is_match(base) and (not only_fams or "local_def_to_extern" in only_fams):
        # fold (scenarioBatExecPhase25, nmlModelCalcSpecularClip): a body-file
        # definition of an asm data symbol -> extern. try_fn scores both alike
        # (the difference is at link), so kept when not worse.
        try:
            for lab, nt in t_local_def_to_extern(cur, name, {"unit": unit, "tu": tu}):
                r0 = pool.submit(evaluate, (unit, tu, name, nt)).result()
                used += 1
                if r0.get("ok") and rank(r0) >= rank(cur_r):
                    cur, cur_r = nt, r0
                    res["applied"].append(f"{lab}:pre={r0['score']}")
                    break
        except Exception as ex:  # noqa: BLE001
            res["predef_error"] = str(ex)[:200]
    if not is_match(base) and (base.get("opcode_score") or 0) >= 0.8:
        try:                               # diagnostic only: interblock sched1 hoisting
            ib = pool.submit(interblock_hint, unit, tu, name, text, base).result()
            used += 1
            if ib and ib.get("better"):
                res["interblock"] = ib
                state.log(f"SWEEP {k} hint: {ib['hint'][:160]}")
        except Exception as ex:  # noqa: BLE001
            res["interblock_error"] = str(ex)[:200]
    gcse_on = False
    if not is_match(base) and (not only_fams or "gcse_split" in only_fams):
        try:                               # fold12: -fno-gcse closer? -> gcse_split candidates
            gd = pool.submit(gcse_hint, unit, tu, name, cur, cur_r.get("score")).result()
            used += 1
            if gd and gd.get("better"):
                gcse_on = True
                res["gcse"] = {"score_nogcse": gd["score_nogcse"], "score": gd["score"]}
                state.log(f"SWEEP {k} hint: -fno-gcse {gd['score']} -> {gd['score_nogcse']}")
        except Exception as ex:  # noqa: BLE001
            res["gcse_error"] = str(ex)[:200]
    seen = {hashlib.sha1(text.encode()).hexdigest()}
    # families already run to a local optimum on this exact text: only the
    # new ones get a first round (all of them again after any improvement)
    done_fams = swept_families(k, best_c) if only_new and not only_fams else set()
    implicit: list = []
    if not is_match(base) and "implicit_decl_proto" not in done_fams \
            and (not only_fams or "implicit_decl_proto" in only_fams):
        try:                               # fold14: callees used with no prototype in scope
            implicit = (pool.submit(implicit_hint, unit, tu, name, cur).result() or {}).get("implicit") or []
            used += 1
            if implicit:
                res["implicit"] = implicit[:8]
        except Exception as ex:  # noqa: BLE001
            res["implicit_error"] = str(ex)[:200]
    batch_min = max(2, getattr(pool, "_max_workers", 3))
    while used < budget and not is_match(cur_r):
        by_fam: dict[str, list] = {}
        skip_fams = done_fams if cur == text else set()
        ctx = {"rows": cur_r.get("diff") or [], "unit": unit, "tu": tu, "patches": {}, "gcse": gcse_on,
               "implicit": implicit, "narrow_calls": cur_r.get("narrow_arg_calls") or []}
        shape = diff_shape(cur_r.get("diff") or [])
        for f in ("cmp_fix", "sched_weight", "de_ifcvt", "loop_live", "const_fix", "zext_form",
                  "field_alias", "volatile_mask", "loop_form", "split_local", "hoist_load",
                  "sched_barrier", "param_widen", "const_rev", "addu_swap", "sign_flip",
                  "float_cmp_neg", "unsized_extern", "aligned_local", "store_sort",
                  "fallthrough_bl", "global_field_view", "global_recast", "const_local",
                  "const_cmp", "cvt_unfold", "zero_store_type", "store_width_cast", "ptr_stride",
                  "bit_test_hoist", "no_tailcall", "builtin_math", "shift_index",
                  "early_float_const", "volatile_global", "union_store",
                  "block_barrier", "load_width_cast"):
            ff = "const_local" if f == "const_cmp" else f
            if f in shape and ff not in skip_fams and (not only_fams or ff in only_fams):
                ctx[{"cmp_fix": "cmp", "const_fix": "const", "zext_form": "zext"}.get(f, f)] = shape[f]
                res.setdefault("shape", {})[f] = shape[f][:3]
        if ("sched_weight" in ctx or "de_ifcvt" in ctx) and "sched_diag" not in res:
            try:                           # informational: equal-priority sched1 decisions
                sd = pool.submit(sched_ties, unit, tu, name, cur).result()
                used += 1
                res["sched_diag"] = {"ties": sd["ties"][:6], "cmov_sched1": sd.get("cmov_sched1"),
                                     "cmov_sched2": sd.get("cmov_sched2")}
            except Exception as ex:  # noqa: BLE001
                res["sched_diag"] = {"error": str(ex)[:200]}
        if register_only(cur_r) and "regalloc_tie" not in skip_fams and (not only_fams or "regalloc_tie" in only_fams):
            try:                           # s-reg rotation: diagnose the global-alloc tie
                ra = pool.submit(regalloc_ties, unit, tu, name, cur).result()
                ctx.update(ra)
                used += 1
                if ra.get("ties"):
                    res.setdefault("regalloc", []).append(
                        {"ties": ra["ties"][:4], "vars": ra["tie_vars"], "mapped": ra.get("mapped")})
            except Exception as ex:  # noqa: BLE001
                res["regalloc_error"] = str(ex)[:200]
        st_now = load_stats()
        for lab, nt in candidates(cur, name, ctx):
            h = hashlib.sha1(nt.encode()).hexdigest()
            if h in seen:
                continue
            seen.add(h)
            if family(lab) in skip_fams or (only_fams and family(lab) not in only_fams):
                continue
            if not only_fams and retired(st_now, family(lab)):
                continue
            by_fam.setdefault(family(lab), []).append((lab, nt, cur_patch + ctx["patches"].get(body_sha(nt), [])))
        if not by_fam:
            break
        order = family_order(load_stats(), by_fam)
        delta: dict = {}
        won = None
        i = 0
        while i < len(order) and used < budget:
            fams, cands = [], []          # fill a batch to keep the pool busy
            while i < len(order) and len(cands) < batch_min:
                fams.append(order[i])
                cands += [(order[i], lab, nt, pt) for lab, nt, pt in by_fam[order[i]]]
                i += 1
            cands = cands[:budget - used]
            rs = list(pool.map(evaluate, [(unit, tu, name, nt, pt) for _, _, nt, pt in cands]))
            used += len(cands)
            res["cc_fail"] = res.get("cc_fail", 0) + sum(1 for r in rs if not r.get("ok"))
            for f in fams:
                d = delta.setdefault(f, {"attempts": 1, "improvements": 0, "matches": 0, "compiles": 0})
                d["compiles"] += sum(1 for c in cands if c[0] == f)
            ok_j = [j for j in range(len(cands)) if (cands[j][0] not in SEMANTIC or is_match(rs[j]))
                    and gate_ok(rs[j], cur_r)]
            bi = max(ok_j, key=lambda j: rank(rs[j])) if ok_j else None
            if bi is not None and rank(rs[bi]) > rank(cur_r):
                won = (cands[bi], rs[bi])
                break
        if won:
            (f, lab, nt, pt), r = won
            delta[f]["improvements"] += 1
            if is_match(r):
                delta[f]["matches"] += 1
        add_stats(delta)
        if not won:
            break
        cur, cur_r, cur_patch = nt, r, pt
        res["applied"].append(f"{lab}={r['score']}")
        state.log(f"SWEEP {k} {lab} -> {r['score']} ({r.get('differing_words')}w)")
    res.update(compiles=used, end=cur_r.get("score"), diff=cur_r.get("differing_words"))
    if ASM_RE.search(cur):
        res["error"] = "inline asm (refused)"
        return res
    sus_new: list = []
    if rank(cur_r) > rank(base) and not is_match(cur_r):
        # fold11: suspect-aware (try_fn.SUSPECT_MARGIN): a sweep result the uninit /
        # semantic tells flag must beat a clean best by the margin
        import try_fn
        fd, tp = tempfile.mkstemp(prefix=f"sweep_{name}_", suffix=".c")
        os.write(fd, cur.encode())
        os.close(fd)
        try:
            ok_b, sus_new = try_fn.prefer_over_best(unit, tu, name, Path(tp), cur_r.get("score"), cur_r,
                                                    best_c.with_name(f"{name}.best.json"))
        except Exception as ex:  # noqa: BLE001
            ok_b, sus_new = True, []
            res["suspect_error"] = str(ex)[:200]
        finally:
            os.unlink(tp)
        if not ok_b:
            res["suspect_refused"] = sus_new[:2]
            state.log(f"SWEEP {k} not saved: suspect {sus_new[:1]}")
            cur, cur_r = text, base
    # 2026-10-14: only refuse when the result fails the gate WORSE than its start
    # (a base that already misses a store count re-found the same 39 gains every
    # pass and could never save them, so looping the sweep never stacked).
    if rank(cur_r) > rank(base) and not is_match(cur_r) and cur_r.get("gate") \
            and not (base.get("gate") and set(cur_r["gate"]) <= set(base["gate"])):
        # correctness gate (try_fn.semantic_gate): calls / stores / data symbols differ from the target
        res["gate_refused"] = cur_r["gate"][:3]
        state.log(f"SWEEP {k} not saved: gate {cur_r['gate'][:2]}")
        cur, cur_r = text, base
    if rank(cur_r) > rank(base):
        shutil.copyfile(best_c, best_c.with_name(f"{name}.best.prev.c"))   # undo point
        best_c.write_text(cur)
        rec = {"score": cur_r["score"], "differing_words": cur_r.get("differing_words"),
               "target_insns": cur_r.get("target_insns"), "by": "sweep",
               "transforms": res["applied"], "suspect_v": 12}
        if sus_new:
            rec["suspect"] = sus_new[:4]
        if cur_patch:
            rec["src_patch"] = cur_patch       # forward declarations the body needs (void_ret)
            if any(family(x) == "ret_narrow" for x in res["applied"]):
                # TU prototype + cast-call sites: try_fn / integrate pick it up as <name>.patch.json
                best_c.with_name(f"{name}.patch.json").write_text(json.dumps([list(x) for x in cur_patch]) + "\n")
        best_c.with_name(f"{name}.best.json").write_text(json.dumps(rec) + "\n")
    if not only_fams:
        # local optimum, or out of budget: either way this exact text is done.
        # (Budget-capped sweeps used to stay unmarked and were re-run from
        # scratch: 1431 of 1768 sweeps restarted from an unchanged score,
        # 21k of 31k compiles, no match.)
        mark_swept(k, cur if rank(cur_r) > rank(base) else text)
    if is_match(cur_r) and cur_patch:
        ok, out = apply_patch(unit, tu, cur_patch)
        res["proto_flip"] = {"patch": cur_patch, "ok": ok}
        state.log(f"SWEEP {k} proto flip ok={ok} {cur_patch}")
        if not ok:
            res["proto_flip"]["tail"] = out
            cur_r = dict(cur_r, match=False, bytes_match=False, link_check=False, match_after_link=False)
    if is_match(cur_r):
        ok, out = integrate(unit, tu, name, best_c,
                            skip_try=not cur_r.get("match") and (not cur_r.get("bytes_match")
                                                                 or bool(cur_r.get("match_after_link"))))
        res["matched"] = ok
        if not ok:
            res["integrate_tail"] = out[-600:]
    state.log(f"SWEEP {k} done {res['start']} -> {res['end']} matched={res['matched']} "
              f"compiles={used} {' '.join(res['applied'])}")
    return res


def pick(min_score, max_diff, limit, only=None, all_pending=False):
    cands = build_queue(ROOT)["candidates"]
    out = []
    for c in cands:
        k = state.key(c["unit"], c["tu"], c["name"])
        if only and k != only:
            continue
        if c.get("nested") and not only:
            continue      # GCC nested function: unmatchable without its parent (build_queue.nested_reason)
        if c.get("asm_blocked") and not only:
            continue      # VU0/COP2/sync/system op: inline asm only (build_queue.asm_blocked_reason)
        if c.get("ice") and not only:
            continue      # build_queue.KNOWN_ICE: the matching C crashes cc1 in the full TU
        bc = BODIES / c["unit"] / c["tu"] / f"{c['name']}.best.c"
        bj = bc.with_name(f"{c['name']}.best.json")
        if not bc.is_file() or not bj.is_file():
            continue
        try:
            j = json.loads(bj.read_text())
        except ValueError:
            continue
        sc = float(j.get("score") or 0)
        if not only and not all_pending and (sc < min_score or sc >= 1):
            continue
        diff = j.get("differing_words")
        if diff is None:
            diff = round((1 - sc) * (c.get("size") or 0) / 4)
        if not only and not all_pending and diff > max_diff:
            continue
        if ASM_RE.search(bc.read_text(errors="replace")):
            continue
        if not only and not all_pending and is_swept(k, bc):
            continue   # every current family already swept to a local optimum
        out.append({"key": k, "unit": c["unit"], "tu": c["tu"], "name": c["name"],
                    "score": sc, "diff": diff})
    out.sort(key=lambda r: (r["diff"], -r["score"]))
    return out[:limit] if limit else out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--key")
    ap.add_argument("--min-score", type=float, default=0.85)
    ap.add_argument("--max-diff", type=int, default=25)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--budget", type=int, default=300)
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--list", action="store_true", help="only print the pool")
    ap.add_argument("--families", help="comma list: only run these families (no sweep mark is written)")
    ap.add_argument("--all-pending", action="store_true",
                    help="every pending body regardless of score / diff / sweep marks (e.g. --families cmp_fix)")
    ap.add_argument("--stats", action="store_true", help="print per-family stats in try order")
    ap.add_argument("--watch", type=float, default=0,
                    help="re-pick every N minutes and sweep bodies that are new or changed since their "
                         "last sweep (sweep_marks.json); runs until killed")
    ap.add_argument("--interblock", action="store_true",
                    help="with --key: only print the -fno-sched-interblock diagnostic for the best body")
    a = ap.parse_args()
    if a.interblock and a.key:
        u, t, n = a.key.split("/")
        txt = (BODIES / u / t / f"{n}.best.c").read_text()
        r0 = evaluate((u, t, n, txt))
        print(json.dumps(interblock_hint(u, t, n, txt, r0), indent=1))
        return 0
    if a.stats:
        st = load_stats()
        for f in family_order(st, st):
            print(f"{f:22} {st[f]}")
        return 0
    only_fams = set(a.families.split(",")) if a.families else None
    if a.list:
        for c in pick(a.min_score, a.max_diff, a.limit, a.key, a.all_pending):
            print(c["key"], c["score"], c["diff"])
        return 0
    if a.watch and (a.all_pending or a.families or a.key):
        ap.error("--watch sweeps unswept bodies only: no --all-pending / --families / --key")
    while True:
        todo = pick(a.min_score, a.max_diff, a.limit, a.key, a.all_pending)
        if todo or not a.watch:
            _sweep_batch(a, todo, only_fams)
        if not a.watch:
            return 0
        time.sleep(a.watch * 60)


def _sweep_batch(a, todo, only_fams) -> None:
    results = []
    with ProcessPoolExecutor(max_workers=min(a.jobs, 3), initializer=_init) as pool:
        for c in todo:
            with claims.locked():
                if c["key"] in claims.active():
                    print(f"skip leased {c['key']}", flush=True)
                    continue
                claims.take(c["key"], "sweep")
            try:
                r = sweep_one(c, pool, a.budget, only_fams=only_fams)
            except Exception as ex:  # noqa: BLE001
                r = {"key": c["key"], "error": repr(ex)[:300]}
            finally:
                claims.release(c["key"])
            results.append(r)
            print(json.dumps(r), flush=True)
    m = [r["key"] for r in results if r.get("matched")]
    imp = [r for r in results if r.get("end") and r.get("start") and r["end"] > r["start"]]
    print(json.dumps({"swept": len(results), "matched": m,
                      "improved": [(r["key"], r["start"], r["end"], r["applied"]) for r in imp]}, indent=1),
          flush=True)


if __name__ == "__main__":
    raise SystemExit(main())

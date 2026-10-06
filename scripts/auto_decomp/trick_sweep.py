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

def t_fallthrough_label(fn):
    """Reorg's annul (branch-likely) choice depends on a label / jump at the
    fall-through: add `goto L; L:` before a constant return, drop a goto that
    only jumps to the next line, or return before a label that is fallen into."""
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
    import shutil
    import try_fn
    fd, p = tempfile.mkstemp(prefix=f"ra_{name}_", suffix=".c")
    os.write(fd, text.encode())
    os.close(fd)
    out: dict = {"regs": [], "ties": [], "tie_vars": []}
    try:
        r = try_fn.try_function(unit, tu, name, Path(p), keep=True, diff_limit=0)
        scratch = Path(r.get("scratch", ""))
        try:
            ii = [x for x in (scratch / "cand").glob("*.i")]
            if not ii:
                out["error"] = "no .i"
                return out
            gcc, flags = _cc1_cmd(unit, tu)
            with tempfile.TemporaryDirectory(prefix="ra_") as td:
                shutil.copyfile(ii[0], Path(td) / "x.i")
                subprocess.run([gcc, *flags, "-S", "-dl", "x.i", "-o", "x.s"], cwd=td,
                               capture_output=True, text=True, timeout=60)
                dumps = list(Path(td).glob("x.i.*lreg"))
                if not dumps:
                    out["error"] = "no .lreg dump"
                    return out
                d = dumps[0].read_text(errors="replace")
        finally:
            shutil.rmtree(scratch, ignore_errors=True)
    finally:
        os.unlink(p)
    m = re.search(r"^;; Function " + re.escape(name) + r"\s*$", d, re.M)
    if not m:
        out["error"] = "function not in dump"
        return out
    nx = re.search(r"^;; Function ", d[m.end():], re.M)
    sec = d[m.end():m.end() + nx.start()] if nx else d[m.end():]
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

TRANSFORMS = [t_decls, t_rename_swap, t_stmt_swap, t_commute, t_split, t_ternary,
              t_ifelse, t_loops, t_single_exit, t_not, t_dup_tail_store, t_fallthrough_label,
              t_div_hoist]
CTX_TRANSFORMS = [t_regalloc_tie]      # need analysis context (regalloc_ties)


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

# ---------------------------------------------------------------- learned order

STATS = ROOT / "build/auto_decomp/trick_stats.json"
MARKS = ROOT / "build/auto_decomp/sweep_marks.json"
FAMILIES = ["declswap", "declreverse", "type", "ptype", "volatile", "rename", "stmtswap", "commute",
            "intcast", "split", "join", "temp", "ternary", "unternary", "ifswap", "for->while",
            "for->dowhile", "for->dowhile-nocheck", "for->goto-test", "while->for", "while->dowhile",
            "single-exit", "not->eq0", "eq0->not", "ne0->bare", "dup_tail_store", "fallthrough_label",
            "div_hoist", "regalloc_tie"]


# Proven by hand before they were automated (DrawRadarInfo matched with both):
# start them ahead of the catalogue; their real record takes over as it grows.
PRIORS = {
    "dup_tail_store": {"attempts": 10, "improvements": 1, "matches": 1, "compiles": 0},
    "regalloc_tie": {"attempts": 10, "improvements": 1, "matches": 1, "compiles": 0},
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


def evaluate(args):
    unit, tu, name, text = args
    import try_fn
    fd, p = tempfile.mkstemp(prefix=f"sweep_{name}_", suffix=".c")
    os.write(fd, text.encode())
    os.close(fd)
    try:
        r = try_fn.try_function(unit, tu, name, Path(p), diff_limit=0)
    except Exception as ex:  # noqa: BLE001
        return {"ok": False, "error": str(ex)[:300]}
    finally:
        os.unlink(p)
    return {k: r.get(k) for k in ("ok", "match", "bytes_match", "link_check", "match_after_link", "score",
                                  "differing_words", "target_insns", "candidate_insns",
                                  "opcode_score", "stage")}


def register_only(r) -> bool:
    """Same length, (near-)identical opcode sequence: the diff is registers."""
    return bool(r and r.get("ok") and not is_match(r) and r.get("opcode_score") is not None
                and r["opcode_score"] >= 1.0 and r.get("target_insns") == r.get("candidate_insns"))


def rank(r):
    if not r or not r.get("ok"):
        return (-1, 0)
    return (r["score"], -(r.get("differing_words") or 0))


def is_match(r):
    return bool(r and r.get("ok") and (r.get("match") or r.get("bytes_match") or r.get("link_check")
                                       or r.get("match_after_link")))


def integrate(unit, tu, name, body: Path, skip_try: bool = False) -> tuple[bool, str]:
    p = subprocess.run([sys.executable, str(HERE / "integrate.py"), "--unit", unit, "--tu", tu,
                        "--name", name, "--body", str(body)] + (["--skip-try"] if skip_try else []),
                       cwd=ROOT, capture_output=True, text=True)
    out = p.stdout + p.stderr
    return '"matched": true' in out, out[-2000:]


def sweep_one(c: dict, pool, budget: int, only_new: bool = True) -> dict:
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
        return res
    cur, cur_r = text, base
    seen = {hashlib.sha1(text.encode()).hexdigest()}
    # families already run to a local optimum on this exact text: only the
    # new ones get a first round (all of them again after any improvement)
    done_fams = swept_families(k, best_c) if only_new else set()
    batch_min = max(2, getattr(pool, "_max_workers", 3))
    while used < budget and not is_match(cur_r):
        by_fam: dict[str, list] = {}
        skip_fams = done_fams if cur == text else set()
        ctx = None
        if register_only(cur_r) and "regalloc_tie" not in skip_fams:        # s-reg rotation: diagnose the global-alloc tie
            try:
                ctx = pool.submit(regalloc_ties, unit, tu, name, cur).result()
                used += 1
                if ctx.get("ties"):
                    res.setdefault("regalloc", []).append(
                        {"ties": ctx["ties"][:4], "vars": ctx["tie_vars"], "mapped": ctx.get("mapped")})
            except Exception as ex:  # noqa: BLE001
                ctx = None
                res["regalloc_error"] = str(ex)[:200]
        for lab, nt in candidates(cur, name, ctx):
            h = hashlib.sha1(nt.encode()).hexdigest()
            if h in seen:
                continue
            seen.add(h)
            if family(lab) in skip_fams:
                continue
            by_fam.setdefault(family(lab), []).append((lab, nt))
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
                cands += [(order[i], lab, nt) for lab, nt in by_fam[order[i]]]
                i += 1
            cands = cands[:budget - used]
            rs = list(pool.map(evaluate, [(unit, tu, name, nt) for _, _, nt in cands]))
            used += len(cands)
            res["cc_fail"] = res.get("cc_fail", 0) + sum(1 for r in rs if not r.get("ok"))
            for f in fams:
                d = delta.setdefault(f, {"attempts": 1, "improvements": 0, "matches": 0, "compiles": 0})
                d["compiles"] += sum(1 for c in cands if c[0] == f)
            bi = max(range(len(cands)), key=lambda j: rank(rs[j]))
            if rank(rs[bi]) > rank(cur_r):
                won = (cands[bi], rs[bi])
                break
        if won:
            (f, lab, nt), r = won
            delta[f]["improvements"] += 1
            if is_match(r):
                delta[f]["matches"] += 1
        add_stats(delta)
        if not won:
            break
        cur, cur_r = nt, r
        res["applied"].append(f"{lab}={r['score']}")
        state.log(f"SWEEP {k} {lab} -> {r['score']} ({r.get('differing_words')}w)")
    res.update(compiles=used, end=cur_r.get("score"), diff=cur_r.get("differing_words"))
    if ASM_RE.search(cur):
        res["error"] = "inline asm (refused)"
        return res
    if rank(cur_r) > rank(base):
        best_c.write_text(cur)
        rec = {"score": cur_r["score"], "differing_words": cur_r.get("differing_words"),
               "target_insns": cur_r.get("target_insns"), "by": "sweep",
               "transforms": res["applied"]}
        best_c.with_name(f"{name}.best.json").write_text(json.dumps(rec) + "\n")
    if used < budget or is_match(cur_r):   # ran to a local optimum, not out of budget
        mark_swept(k, cur if rank(cur_r) > rank(base) else text)
    if is_match(cur_r):
        ok, out = integrate(unit, tu, name, best_c,
                            skip_try=not cur_r.get("match") and not cur_r.get("bytes_match"))
        res["matched"] = ok
        if not ok:
            res["integrate_tail"] = out[-600:]
    state.log(f"SWEEP {k} done {res['start']} -> {res['end']} matched={res['matched']} "
              f"compiles={used} {' '.join(res['applied'])}")
    return res


def pick(min_score, max_diff, limit, only=None):
    cands = build_queue(ROOT)["candidates"]
    out = []
    for c in cands:
        k = state.key(c["unit"], c["tu"], c["name"])
        if only and k != only:
            continue
        bc = BODIES / c["unit"] / c["tu"] / f"{c['name']}.best.c"
        bj = bc.with_name(f"{c['name']}.best.json")
        if not bc.is_file() or not bj.is_file():
            continue
        try:
            j = json.loads(bj.read_text())
        except ValueError:
            continue
        sc = float(j.get("score") or 0)
        if not only and (sc < min_score or sc >= 1):
            continue
        diff = j.get("differing_words")
        if diff is None:
            diff = round((1 - sc) * (c.get("size") or 0) / 4)
        if not only and diff > max_diff:
            continue
        if ASM_RE.search(bc.read_text(errors="replace")):
            continue
        if not only and is_swept(k, bc):
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
    ap.add_argument("--stats", action="store_true", help="print per-family stats in try order")
    a = ap.parse_args()
    if a.stats:
        st = load_stats()
        for f in family_order(st, st):
            print(f"{f:22} {st[f]}")
        return 0
    todo = pick(a.min_score, a.max_diff, a.limit, a.key)
    if a.list:
        for c in todo:
            print(c["key"], c["score"], c["diff"])
        return 0
    results = []
    with ProcessPoolExecutor(max_workers=min(a.jobs, 3), initializer=_init) as pool:
        for c in todo:
            with claims.locked():
                if c["key"] in claims.active():
                    print(f"skip leased {c['key']}", flush=True)
                    continue
                claims.take(c["key"], "sweep")
            try:
                r = sweep_one(c, pool, a.budget)
            except Exception as ex:  # noqa: BLE001
                r = {"key": c["key"], "error": repr(ex)[:300]}
            finally:
                claims.release(c["key"])
            results.append(r)
            print(json.dumps(r), flush=True)
    m = [r["key"] for r in results if r.get("matched")]
    imp = [r for r in results if r.get("end") and r.get("start") and r["end"] > r["start"]]
    print(json.dumps({"swept": len(results), "matched": m,
                      "improved": [(r["key"], r["start"], r["end"], r["applied"]) for r in imp]}, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

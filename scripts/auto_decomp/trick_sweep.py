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
  early return -> single exit goto, `!x` <-> `x == 0`.

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


TRANSFORMS = [t_decls, t_rename_swap, t_stmt_swap, t_commute, t_split, t_ternary,
              t_ifelse, t_loops, t_single_exit, t_not]


def candidates(text: str, name: str):
    loc = find_function(text, name)
    if not loc:
        return
    s, b, e = loc
    fn = text[s:e + 1]
    for t in TRANSFORMS:
        try:
            for lab, nf in t(fn):
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
            "single-exit", "not->eq0", "eq0->not", "ne0->bare"]


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
    return _locked_json(STATS, lambda o: o if isinstance(o, dict) and o else _seed_stats())


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


def mark_swept(k: str, text: str) -> None:
    def up(o):
        o = o if isinstance(o, dict) else {}
        o[k] = {"sha1": body_sha(text), "t": int(time.time())}
        return o
    _locked_json(MARKS, up)


def is_swept(k: str, best_c: Path) -> bool:
    """True when trick_sweep already ran on BEST_C's current text."""
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
    return {k: r.get(k) for k in ("ok", "match", "bytes_match", "link_check", "score",
                                  "differing_words", "target_insns", "stage")}


def rank(r):
    if not r or not r.get("ok"):
        return (-1, 0)
    return (r["score"], -(r.get("differing_words") or 0))


def is_match(r):
    return bool(r and r.get("ok") and (r.get("match") or r.get("bytes_match") or r.get("link_check")))


def integrate(unit, tu, name, body: Path) -> tuple[bool, str]:
    p = subprocess.run([sys.executable, str(HERE / "integrate.py"), "--unit", unit, "--tu", tu,
                        "--name", name, "--body", str(body)], cwd=ROOT, capture_output=True, text=True)
    out = p.stdout + p.stderr
    return '"matched": true' in out, out[-2000:]


def sweep_one(c: dict, pool, budget: int) -> dict:
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
    batch_min = max(2, getattr(pool, "_max_workers", 3))
    while used < budget and not is_match(cur_r):
        by_fam: dict[str, list] = {}
        for lab, nt in candidates(cur, name):
            h = hashlib.sha1(nt.encode()).hexdigest()
            if h in seen:
                continue
            seen.add(h)
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
        ok, out = integrate(unit, tu, name, best_c)
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
        if j.get("by") == "sweep" and not only:
            continue   # already swept to a local optimum
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

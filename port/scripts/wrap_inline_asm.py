#!/usr/bin/env python3
"""Rewrite inline asm in the game sources into the include/port/ps2_asm.h macros.

One-shot, idempotent source migration (run again after importing new code):

  __asm__ __volatile__ (...)  / __asm__ volatile (...)  ->  PS2_ASMV(...)
  __asm__ (...)  asm (...)  (statement, not a symbol label)  ->  PS2_ASM(...)
  register T x asm("$f21")                                ->  register T x PS2_REG("$f21")
  file-scope __asm__(".equ ...");                         ->  wrapped in #ifndef PORT_HOST

Symbol labels (`extern T x[] __asm__("sym")`, `#define SYM(n) __asm__(#n)`)
are left alone: host gcc accepts them.  Each touched file gets
`#include "port/ps2_asm.h"` as its first line.  The PS2 expansion of every
macro is the original construct, so ee-gcc output is unchanged (checked by
`ninja gate`).

Usage: python3 port/scripts/wrap_inline_asm.py [--check]
  --check  only report what would change; exit 1 if anything would.
"""
import glob
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
INCLUDE = '#include "port/ps2_asm.h"\n'
HEAD = re.compile(r"\b(?:__asm__|asm|__asm)\s*(?P<vol>(?:__volatile__|volatile|__volatile)\s*)?\(")
REG = re.compile(r"(\bregister\b[^;=(){}]*?)\b(?:asm|__asm__)(\s*\(\s*\"[^\"]*\"\s*\))")


def files():
    out = []
    for unit in ("main", "ov01", "ov02", "ov10", "ov11", "ov12"):
        out += glob.glob(f"src/{unit}/**/*.[ch]", recursive=True)
    out += glob.glob("include/**/*.h", recursive=True)
    skip = {"include/include_asm.h", "include/port/ps2_asm.h"}
    return sorted(f for f in out if f not in skip)


def blank(t):
    """Comments and string contents -> spaces (offsets preserved)."""
    def sub(m):
        s = m.group(0)
        if s.startswith('"') or s.startswith("'"):
            return s[0] + re.sub(r"[^\n]", " ", s[1:-1]) + s[-1]
        return re.sub(r"[^\n]", " ", s)
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', sub, t, flags=re.S)


def is_label(b, start):
    line = b[b.rfind("\n", 0, start) + 1:start]
    if line.lstrip().startswith("#"):
        # `#define SYM(name) __asm__(#name)` label macro
        return re.match(r"\s*\(\s*#\s*\w+\s*\)", b[b.index("(", start):]) is not None
    j = start - 1
    while j >= 0 and b[j] in " \t\n\\":
        j -= 1
    if j < 0:
        return False
    # labels follow a declarator: `name`, `name[]`, `f(args)`
    if b[j] in "]_" or b[j].isalnum():
        return True
    if b[j] == ")":
        # `f(void) __asm__("x")` declarator vs `if (c) __asm__(...)`: look for
        # a control keyword in front of the matching '('
        depth, k = 0, j
        while k >= 0:
            if b[k] == ")":
                depth += 1
            elif b[k] == "(":
                depth -= 1
                if depth == 0:
                    break
            k -= 1
        return not re.search(r"\b(if|while|for|switch)\s*$", b[:k])
    return False


def close_paren(b, i):
    depth = 1
    while depth:
        if b[i] == "(":
            depth += 1
        elif b[i] == ")":
            depth -= 1
        i += 1
    return i


def rewrite(t):
    b = blank(t)
    edits = []  # (start, end, replacement)
    for m in HEAD.finditer(b):
        a = m.start()
        line_start = b.rfind("\n", 0, a) + 1
        if REG.match(b, line_start) and re.search(r"\bregister\b", b[line_start:a]):
            continue  # register binding, below
        if not m.group("vol") and is_label(b, a):
            continue
        end = close_paren(b, m.end())
        toplevel = (b[line_start:a].strip() == "" and _depth(b, a) == 0
                    and not b[:line_start].rstrip("\n").endswith("\\"))
        if toplevel:
            semi = re.match(r"\s*;[ \t]*\n?", b[end:])
            stop = end + (semi.end() if semi else 0)
            body = t[a:stop]
            if not body.endswith("\n"):
                body += "\n"
            edits.append((line_start, stop, "#ifndef PORT_HOST\n" + t[line_start:a] + body + "#endif\n"))
            continue
        edits.append((a, m.end(), "PS2_ASMV(" if m.group("vol") else "PS2_ASM("))
    for m in REG.finditer(b):
        edits.append((m.end(1), m.start(2), "PS2_REG"))
    if not edits:
        return t, 0
    edits.sort()
    out, last = [], 0
    for a, e, r in edits:
        assert a >= last, (a, last)
        out += [t[last:a], r]
        last = e
    out.append(t[last:])
    t2 = "".join(out)
    if INCLUDE not in t2:
        t2 = INCLUDE + t2
    return t2, len(edits)


def _depth(b, pos):
    """Brace depth at pos (0 == file scope)."""
    return b.count("{", 0, pos) - b.count("}", 0, pos)


def main():
    os.chdir(ROOT)
    check = "--check" in sys.argv
    total, changed = 0, 0
    for f in files():
        t = open(f, encoding="latin-1").read()
        t2, n = rewrite(t)
        if t2 != t:
            changed += 1
            total += n
            if not check:
                open(f, "w", encoding="latin-1").write(t2)
            print(f"{f}: {n}")
    print(f"{changed} files, {total} rewrites" + (" (check only)" if check else ""))
    sys.exit(1 if check and changed else 0)


if __name__ == "__main__":
    main()

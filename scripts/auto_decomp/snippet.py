#!/usr/bin/env python3
"""Fast pre-screen: compile a tiny standalone C snippet with a TU's ee-gcc
and cc1 flags (no TU, no includes, no assembler) in milliseconds and return
the asm of one function, so expression variants can be filtered by their asm
shape before spending full try_fn compiles.

    python3 -B scripts/auto_decomp/snippet.py --unit main --tu game \\
        --code 'extern void g(void); void f(short x){ if ((unsigned short)(x - 5) < 9) g(); }'

Library: compile_snippet(unit, tu, code, fn="f") -> [insn, ...] (mnemonic +
operands, labels / directives dropped; cached on (unit, tu, code)), and
has_seq(insns, [regex, ...]) -> True when the regexes match insns in order
(gaps allowed). Used by trick_sweep's zext_form family (and available to
cmp_fix / const_fix style generators).
"""
from __future__ import annotations

import argparse
import functools
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))


@functools.lru_cache(maxsize=64)
def _flags(unit: str, tu: str):
    import trick_sweep
    return trick_sweep._cc1_cmd(unit, tu)


@functools.lru_cache(maxsize=4096)
def _compile(unit: str, tu: str, code: str, fn: str) -> tuple[str, ...]:
    gcc, flags = _flags(unit, tu)
    with tempfile.TemporaryDirectory(prefix="snip_") as td:
        (Path(td) / "x.c").write_text(code + "\n")
        p = subprocess.run([gcc, *flags, "-S", "x.c", "-o", "x.s"], cwd=td, capture_output=True,
                           text=True, timeout=60)
        if p.returncode != 0:
            raise ValueError(p.stderr[-800:])
        s = (Path(td) / "x.s").read_text()
    out, on = [], False
    for ln in s.splitlines():
        t = ln.strip()
        if re.match(re.escape(fn) + r":$", t):
            on = True
            continue
        if not on:
            continue
        if t.startswith(".end") and fn in t:
            break
        if not t or t.endswith(":") or t.startswith((".", "#")):
            continue
        out.append(re.sub(r"\s+", " ", t.replace("$", "")))
    return tuple(out)


def compile_snippet(unit: str, tu: str, code: str, fn: str = "f") -> list[str]:
    return list(_compile(unit, tu, code, fn))


def has_seq(insns, pats) -> bool:
    i = 0
    for x in insns:
        if i < len(pats) and re.search(pats[i], x):
            i += 1
    return i == len(pats)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", required=True)
    ap.add_argument("--tu", required=True)
    ap.add_argument("--code", required=True, help="C text, or @file")
    ap.add_argument("--fn", default="f")
    a = ap.parse_args()
    code = Path(a.code[1:]).read_text() if a.code.startswith("@") else a.code
    try:
        print("\n".join(compile_snippet(a.unit, a.tu, code, a.fn)))
    except ValueError as ex:
        print(json.dumps({"ok": False, "error": str(ex)}))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

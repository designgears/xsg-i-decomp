#!/usr/bin/env python3
"""Resolve absolute addresses to the symbol that contains them.

    python3 -B scripts/auto_decomp/addr2sym.py 0x490DC0 [0x490DBC ...] [--unit ov01]

Searches config/symbols/<unit>.txt (default: every unit) for `NAME = 0xADDR;
// size:0xN` entries and prints `ADDR -> NAME+0xOFF (size 0xN, file)`. Use it
when the target loads a bare address (`lui/addiu 0x490DC0`, an undefined
D_xxx at link): write the C against the containing symbol (e.g. PadData+0x30
-> a field/index of PadData) instead of inventing a new symbol.
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENTRY = re.compile(r"^\s*([A-Za-z_][\w.]*)\s*=\s*0x([0-9A-Fa-f]+)\s*;(?:.*size:\s*0x([0-9A-Fa-f]+))?")


def load(units: list[str] | None) -> list[tuple[int, int, str, str]]:
    out = []
    for f in sorted((ROOT / "config/symbols").glob("*.txt")):
        if units and f.stem not in units:
            continue
        for line in f.read_text(errors="replace").splitlines():
            m = ENTRY.match(line)
            if m:
                out.append((int(m.group(2), 16), int(m.group(3) or "0", 16), m.group(1), f.name))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("addr", nargs="+")
    ap.add_argument("--unit", action="append")
    a = ap.parse_args()
    syms = load(a.unit)
    for s in a.addr:
        x = int(s, 16)
        hits = sorted({(addr, size, name, f) for addr, size, name, f in syms
                       if addr <= x < addr + max(size, 1)}, key=lambda h: x - h[0])
        if not hits:   # nearest symbol below, size unknown
            below = [h for h in syms if h[0] <= x]
            hits = sorted(below, key=lambda h: x - h[0])[:1]
        if not hits:
            print(f"0x{x:X} -> (no symbol)")
            continue
        addr, size, name, f = hits[0]
        off = x - addr
        print(f"0x{x:X} -> {name}{'+0x%X' % off if off else ''}"
              f"  (at 0x{addr:X}, size {'0x%X' % size if size else '?'}, {f})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

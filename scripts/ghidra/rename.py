#!/usr/bin/env python3
"""Rewrite Ghidra's placeholder names (func_0x.., DAT_.., PTR_.., LAB_..) in
build/ghidra/<unit>/*.c with the names config/symbols/<unit>.txt gives that
address (an overlay's file also names MAIN's symbols, which its image lacks).

    python3 -B scripts/ghidra/rename.py <export dir>      (build/ghidra/<unit>[.new])
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SYM = re.compile(r"^\s*([A-Za-z_.$][\w.$]*)\s*=\s*0x([0-9A-Fa-f]+);")
REF = re.compile(r"\b(?:func_0x|FUN_|DAT_|PTR_|LAB_|_DAT_)([0-9a-fA-F]{8})\b")


def main() -> int:
    out = Path(sys.argv[1]).resolve()
    unit = out.name.split(".")[0]
    names = {}
    for line in (ROOT / f"config/symbols/{unit}.txt").read_text().splitlines():
        m = SYM.match(line)
        if m:
            names.setdefault(int(m.group(2), 16), m.group(1))
    n = 0
    for f in out.glob("*.c"):
        t = f.read_text()
        new = REF.sub(lambda m: names.get(int(m.group(1), 16), m.group(0)), t)
        if new != t:
            f.write_text(new)
            n += 1
    print(f"rename {unit}: {n} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())

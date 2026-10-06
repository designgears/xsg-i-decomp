#!/usr/bin/env python3
"""Copy a fresh Ghidra export into the agents' seeds.

    python3 -B scripts/ghidra/reseed.py

For every function with a scratch body (build/auto_decomp/bodies/<unit>/<tu>/
<name>.c or .m2c.c), refresh <name>.ghidra.c from build/ghidra/<unit>/<name>.c
when it differs. m2c_seed.py only copies it once, at seeding time, so without
this the seeds keep the decompile from before the types they now benefit from.
refresh.sh runs it after each export.
"""
from __future__ import annotations

import filecmp
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    bodies, ghidra = ROOT / "build/auto_decomp/bodies", ROOT / "build/ghidra"
    seen = updated = 0
    for tu_dir in sorted(p for p in bodies.glob("*/*") if p.is_dir()):
        unit = tu_dir.parent.name
        names = {p.name.split(".")[0] for p in tu_dir.glob("*.c")}
        for name in sorted(names):
            src = ghidra / unit / f"{name}.c"
            if not src.is_file():
                continue
            seen += 1
            dst = tu_dir / f"{name}.ghidra.c"
            if not dst.is_file() or not filecmp.cmp(src, dst, shallow=False):
                shutil.copyfile(src, dst)
                updated += 1
    print(f"reseed: {updated} of {seen} .ghidra.c seeds updated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

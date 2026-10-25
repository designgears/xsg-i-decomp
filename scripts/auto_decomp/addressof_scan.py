#!/usr/bin/env python3
"""List C functions whose code depends on memory layout (gcc 2.96 ADDRESSOF bug).

A scalar local whose address is taken (`f(&table)`) becomes an ADDRESSOF mem in
gcc 2.96, and that MEM's alias set is never initialized: it is whatever the
obstack held before (0x2020202000000000 = leftover spaces, 0, ...).  The value
decides whether later stores kill the local's reload, so the function's code
changes when *anything earlier in the TU* changes (one extra prototype was
enough).  2026-10-07: curEnProcInit flipped on 6 of 8 one-line perturbations and
broke every ov01/battle_init integrate (`ov01.ld:94 cannot move location counter
backwards`: battle_init .text 8 bytes longer, pinned by the next data pin).

Fix of a listed function (byte-identical, apply with verified_edit.py): make the
local a one-element array, `T table[1]; f(table); ... table[0][i]` (its mem
gets a real, initialized alias set; stable under 8/8 perturbations).  curEnProcInit,
calcA2Chk, calcStatGetNorm, loadScriptCD were fixed so.

    python3 -B scripts/auto_decomp/addressof_scan.py [--all]

Recompiles every built TU's .i with -dr (ee-gcc 2.96, -O2) and prints
`TU FUNCTION alias-sets` for functions with an ADDRESSOF mem whose alias set is
garbage (outside 0..0xFFFF; small positive values are real alias sets, e.g. the
array form's).  --all: also the ones at 0 (uninitialized but zero this time:
they can flip the other way, more rarely).
"""
from __future__ import annotations

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GCC = ROOT / ".work/toolchains/ee-gcc2.96-realconv-lp7-fsv3-v1/bin"
sys.path.insert(0, str(ROOT / "tools/tu"))
from noaslr import NOASLR  # noqa: E402
MEM = re.compile(r"\(mem[/\w]*:\w+ \(addressof[^\n]*?\) (-?\d+)\)")


def scan(ipath: str) -> list[tuple[str, str, list[str]]]:
    with tempfile.TemporaryDirectory(prefix="aof_") as td:
        shutil.copyfile(ipath, Path(td) / "x.i")
        subprocess.run([*NOASLR, str(GCC / "ee-gcc"), f"-B{GCC}/", "-nostdinc", "-fno-builtin", "-O2", "-G0",
                        "-S", "-dr", "x.i", "-o", "x.s"], cwd=td, capture_output=True, timeout=600)
        out = []
        for f in Path(td).glob("x.i.*rtl"):
            parts = re.split(r"^;; Function (\S+)\s*$", f.read_text(errors="replace"), flags=re.M)
            for i in range(1, len(parts), 2):
                al = sorted(set(MEM.findall(parts[i + 1])))
                if al:
                    out.append((ipath.split("/build/", 2)[-1], parts[i], al))
        return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--all", action="store_true", help="also functions whose ADDRESSOF alias set is 0")
    ap.add_argument("-j", type=int, default=4)
    a = ap.parse_args()
    iis = sorted(p for p in glob.glob(str(ROOT / "build/*/build/**/*.i"), recursive=True)
                 if "/try/" not in p and ".raw." not in p)
    with ThreadPoolExecutor(a.j) as ex:
        for res in ex.map(scan, iis):
            for tu, fn, al in res:
                garbage = [x for x in al if not 0 <= int(x) <= 0xFFFF]
                if garbage or (a.all and "0" in al):
                    print(tu, fn, " ".join(al), "GARBAGE" if garbage else "zero")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

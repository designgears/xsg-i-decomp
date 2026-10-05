#!/usr/bin/env python3
"""Apply exact-text replacements to a TU, keep them only if the unit stays identical.

    python3 -B scripts/auto_decomp/verified_edit.py --unit ov12 --tu rg_robot \\
        --replace 'static void _InitDamageStatus(int a, float b);' \\
                  'static void _InitDamageStatus(float b, int a);' \\
        --replace '_InitDamageStatus(s, x, 1.0f)' '_InitDamageStatus(s, 1.0f, x)'

For refactors of ALREADY-MATCHED code that a new function needs (a parameter
order or type fixed in a prototype together with its existing call sites, a
field renamed, a typedef completed). Each OLD must occur exactly once (use
--all to replace every occurrence). Runs under the global integrate lock,
rebuilds the unit and reverts unless it is still whole-file identical — so it
can never change the produced bytes, only the source's shape.
Prints JSON; exit 0 when applied.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from state import locked, log  # noqa: E402
from verify_unit import rebuild  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", required=True)
    ap.add_argument("--tu", required=True)
    ap.add_argument("--replace", nargs=2, action="append", required=True, metavar=("OLD", "NEW"))
    ap.add_argument("--all", action="store_true", help="replace every occurrence of each OLD")
    ap.add_argument("--file", help="edit this src/ or include/ file (e.g. the TU's header) instead of the TU;"
                                   " verified with the full `ninja gate`, since other units may include it")
    a = ap.parse_args()
    src = ROOT / "src" / a.unit / f"{a.tu}.c"
    if a.file:
        src = (ROOT / a.file).resolve()
        if not (src.is_file() and src.is_relative_to(ROOT) and src.relative_to(ROOT).parts[0] in ("src", "include")):
            ap.error("--file must be an existing file under src/ or include/")
    report: dict = {"tu": str(src.relative_to(ROOT)), "replacements": len(a.replace)}
    with locked():
        original = src.read_text()
        text = original
        for old, new in a.replace:
            n = text.count(old)
            if n == 0 or (n > 1 and not a.all):
                report.update(ok=False, error=f"{old!r} occurs {n} times (need exactly 1, or --all)")
                print(json.dumps(report, indent=1))
                return 1
            text = text.replace(old, new)
        tmp = src.with_name(f".{src.name}.{os.getpid()}.tmp")
        tmp.write_text(text)
        os.replace(tmp, src)
        r = rebuild(ROOT, a.unit)
        ok = bool(r.get("identical")) and (a.unit != "main" or r.get("mapcheck_ok", True))
        if ok and a.file:
            g = subprocess.run(["ninja", "gate"], cwd=ROOT, capture_output=True, text=True)
            ok = g.returncode == 0
            r["ninja_tail"] = (g.stdout + g.stderr)[-1500:]
        if not ok:
            tmp.write_text(original)
            os.replace(tmp, src)
            rebuild(ROOT, a.unit)
            if a.file:
                subprocess.run(["ninja", "gate"], cwd=ROOT, capture_output=True)
            report.update(ok=False, error="unit not identical after the edit (reverted)",
                          ninja_tail=r.get("ninja_tail", "")[-1500:])
            print(json.dumps(report, indent=1))
            return 1
    log(f"VEDIT {a.unit}/{a.tu} {len(a.replace)} replacement(s)")
    report["ok"] = True
    print(json.dumps(report, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

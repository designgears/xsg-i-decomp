#!/usr/bin/env python3
"""Drain tiny INCLUDE_ASM funcs recognizable by pattern_decompile.py."""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from next import eligible  # noqa: E402
from state import key, log  # noqa: E402
import claims  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("-n", type=int, default=20)
    ap.add_argument("--max-size", type=int, default=64)
    ap.add_argument("--unit", action="append")
    a = ap.parse_args()
    units = set(a.unit) if a.unit else None
    cands = eligible(a.root, a.n, a.max_size, units, claim_owner="patterns", ttl_s=15 * 60)
    matched = failed = skipped = 0
    for c in cands:
        body = a.root / "build/auto_decomp/bodies" / c["unit"] / c["tu"] / f"{c['name']}.c"
        asm = a.root / c["asm"]
        proc = subprocess.run(
            [sys.executable, "-B", str(HERE / "pattern_decompile.py"),
             "--asm", str(asm), "--name", c["name"], "--out", str(body)],
            capture_output=True, text=True)
        if proc.returncode != 0:
            skipped += 1
            claims.release(key(c["unit"], c["tu"], c["name"]))
            continue
        integ = subprocess.run(
            [sys.executable, "-B", str(HERE / "integrate.py"),
             "--root", str(a.root), "--unit", c["unit"], "--tu", c["tu"],
             "--name", c["name"], "--body", str(body)],
            capture_output=True, text=True)
        try:
            report = json.loads(integ.stdout)
        except json.JSONDecodeError:
            report = {"ok": False, "error": integ.stdout[-500:], "stderr": integ.stderr[-500:]}
        if report.get("ok") and report.get("matched"):
            matched += 1
            print(f"MATCH {c['unit']}/{c['tu']}/{c['name']} size={c['size']}")
        else:
            failed += 1
            print(f"FAIL  {c['unit']}/{c['tu']}/{c['name']}: {report.get('error') or report.get('verify',{}).get('diff_range_count')}")
            log(f"pattern fail {c['unit']}/{c['tu']}/{c['name']}: {json.dumps(report)[:500]}")
    print(json.dumps({"matched": matched, "failed": failed, "skipped_unrecognized": skipped,
                      "considered": len(cands)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

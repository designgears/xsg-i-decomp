#!/usr/bin/env python3
"""Rebuild one unit and report whole-file identity (the matching gate)."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OVERLAYS = ("ov01", "ov02", "ov10", "ov11", "ov12")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def original_path(root: Path, unit: str) -> Path:
    originals = json.loads((root / "config/originals.json").read_text())
    name = originals["units"][unit]["file"]
    local = root / "config/toolchain-local.json"
    game = None
    if local.is_file():
        game = json.loads(local.read_text()).get("game_dir")
    game = Path(game) if game else root / "game/extracted-iso"
    return Path(game) / name


def rebuild(root: Path, unit: str, jobs: int = 4) -> dict:
    env = os.environ.copy()
    # Prefer scripts/env.sh settings if already exported.
    unit_dir = root / "build" / unit
    if unit == "main":
        targets = ["build/main.compare.json", "build/main.mapcheck.json"]
    else:
        targets = [f"build/{unit}.compare.json"]
    cmd = ["ninja", "-C", str(unit_dir), "-j", str(jobs), *targets]
    proc = subprocess.run(cmd, cwd=str(root), env=env, capture_output=True, text=True)
    report = {
        "unit": unit,
        "ninja_rc": proc.returncode,
        "ninja_tail": (proc.stdout + proc.stderr)[-4000:],
    }
    if proc.returncode != 0:
        report["identical"] = False
        report["error"] = "ninja failed"
        return report
    if unit == "main":
        compare_path = unit_dir / "build/main.compare.json"
        mapcheck_path = unit_dir / "build/main.mapcheck.json"
        compare = json.loads(compare_path.read_text())
        mapcheck = json.loads(mapcheck_path.read_text())
        report["identical"] = bool(compare.get("whole_file_identical"))
        report["mapcheck_ok"] = bool(mapcheck.get("ok"))
        report["diff_range_count"] = compare.get("diff_range_count")
        report["diffs"] = compare.get("diffs", [])[:8]
        report["mapcheck_problems"] = (mapcheck.get("problems") or [])[:5]
    else:
        compare_path = unit_dir / f"build/{unit}.compare.json"
        compare = json.loads(compare_path.read_text())
        report["identical"] = bool(compare.get("whole_file_identical"))
        report["diff_range_count"] = compare.get("diff_range_count")
        report["diffs"] = compare.get("diffs", [])[:8]
    # Hash cross-check against originals
    built = unit_dir / "build" / (f"{unit}.bin" if unit != "main" else "main.bin")
    if unit == "main":
        built = unit_dir / "build/main.bin"
    else:
        built = unit_dir / f"build/{unit}.bin"
    if built.is_file():
        report["built_sha256"] = sha256(built)
        report["original_sha256"] = sha256(original_path(root, unit))
        report["sha_match"] = report["built_sha256"] == report["original_sha256"]
    return report


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("unit", choices=("main",) + OVERLAYS)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--json-out", type=Path)
    a = ap.parse_args()
    report = rebuild(a.root, a.unit)
    text = json.dumps(report, indent=1) + "\n"
    if a.json_out:
        a.json_out.write_text(text)
    sys.stdout.write(text)
    ok = report.get("identical") and report.get("ninja_rc") == 0
    if a.unit == "main":
        ok = ok and report.get("mapcheck_ok", True)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())

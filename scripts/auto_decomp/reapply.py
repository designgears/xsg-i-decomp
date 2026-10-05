#!/usr/bin/env python3
"""Re-integrate functions matched on another branch into the current tree.

    python3 -B scripts/auto_decomp/reapply.py --branch local-matches [--list build/reapply.json]

For each {unit, tu, name} in the list (functions that are INCLUDE_ASM here but C
on --branch), integrate.py is run with, in order, (1) the saved body file
build/auto_decomp/bodies/<unit>/<tu>/<name>.c and (2) the definition extracted
from the branch's TU (with the declarations it uses). integrate verifies every
one byte-for-byte and restores the TU on failure, so a body that needs a type
the current tree lacks just fails and is reported. Results go to
build/reapply.results.json; MATCH/FAIL lines also go to loop.log via integrate.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import tu_edit  # noqa: E402


def integrate(unit: str, tu: str, name: str, body: Path) -> dict:
    p = subprocess.run([sys.executable, "-B", str(ROOT / "scripts/auto_decomp/integrate.py"),
                        "--unit", unit, "--tu", tu, "--name", name, "--body", str(body)],
                       cwd=ROOT, capture_output=True, text=True)
    try:
        return json.loads(p.stdout[p.stdout.index("{"):])
    except ValueError:
        return {"ok": False, "error": (p.stderr or p.stdout)[-400:]}


def from_branch(branch: str, unit: str, tu: str, name: str) -> str | None:
    r = subprocess.run(["git", "show", f"{branch}:src/{unit}/{tu}.c"], cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        return None
    try:
        return tu_edit.extract_function(r.stdout, name, declarations=True)
    except Exception:
        return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--branch", required=True)
    ap.add_argument("--list", type=Path, default=ROOT / "build/reapply.json")
    a = ap.parse_args()
    todo = json.loads(a.list.read_text())
    out_path = ROOT / "build/reapply.results.json"
    results = json.loads(out_path.read_text()) if out_path.is_file() else {}
    scratch = ROOT / "build/auto_decomp/reapply"
    scratch.mkdir(parents=True, exist_ok=True)
    for i, f in enumerate(todo, 1):
        key = f"{f['unit']}/{f['tu']}/{f['name']}"
        if results.get(key, {}).get("matched"):
            continue
        t0 = time.time()
        attempts = []
        body = ROOT / f"build/auto_decomp/bodies/{f['unit']}/{f['tu']}/{f['name']}.c"
        if body.is_file():
            attempts.append(("body", body))
        text = from_branch(a.branch, f["unit"], f["tu"], f["name"])
        if text:
            alt = scratch / f"{f['unit']}__{f['tu']}__{f['name']}.c"
            alt.write_text(text)
            attempts.append(("branch", alt))
        res = {"matched": False, "tried": []}
        for label, path in attempts:
            r = integrate(f["unit"], f["tu"], f["name"], path)
            res["tried"].append({"from": label, "matched": bool(r.get("matched")),
                                 "error": r.get("error") or r.get("refused") or r.get("stage")})
            if r.get("matched"):
                res.update(matched=True, source=label)
                break
        results[key] = res
        out_path.write_text(json.dumps(results, indent=1))
        print(f"[{i}/{len(todo)}] {key}: {'MATCH via ' + res['source'] if res['matched'] else 'FAIL'} "
              f"({time.time() - t0:.0f}s)", flush=True)
    done = sum(1 for r in results.values() if r.get("matched"))
    print(json.dumps({"matched": done, "failed": len(results) - done}))
    return 0


if __name__ == "__main__":
    sys.exit(main())

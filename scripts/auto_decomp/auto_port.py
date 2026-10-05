#!/usr/bin/env python3
"""Mechanically port bodies matched on another branch whose re-integration failed
to compile because the current TU already declares their types/externs.

    python3 -B scripts/auto_decomp/auto_port.py --branch local-matches [--list build/reapply.retry.json]

For each function still INCLUDE_ASM and not leased by someone else: take the saved
body (or the branch TU's definition when the body is only an m2c draft), drop the
body's leading declarations whose names the TU already declares, hoist those names
above the function (hoist.py, verified byte-identical), and run integrate.py.
Whatever does not match is left for an agent. Results: build/auto_port.results.json.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import claims  # noqa: E402
import tu_edit  # noqa: E402

INC = r'INCLUDE_ASM\(\s*"[^"]*"\s*,\s*{}\s*\)'


def run(*args) -> dict:
    p = subprocess.run([sys.executable, "-B", *map(str, args)], cwd=ROOT, capture_output=True, text=True)
    out = p.stdout
    try:
        return json.loads(out[out.index("{"):])
    except ValueError:
        return {"ok": False, "error": (p.stderr or out)[-300:]}


def names_of(decl: str) -> set[str]:
    try:
        return {k for _kind, k in tu_edit.decl_keys(decl)}
    except Exception:
        return set()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--branch", required=True)
    ap.add_argument("--list", type=Path, default=ROOT / "build/reapply.retry.json")
    a = ap.parse_args()
    keys = json.loads(a.list.read_text())
    out_path = ROOT / "build/auto_port.results.json"
    results = json.loads(out_path.read_text()) if out_path.is_file() else {}
    scratch = ROOT / "build/auto_decomp/reapply"
    scratch.mkdir(parents=True, exist_ok=True)
    for key in keys:
        unit, tu, name = key.split("/", 2)
        src = ROOT / "src" / unit / f"{tu}.c"
        if not re.search(INC.format(re.escape(name)), src.read_text(errors="replace")):
            continue                                     # already C (an agent or an earlier run)
        lease = claims.active().get(key)
        if lease and lease.get("owner") not in ("auto", "port"):
            continue
        body = ROOT / f"build/auto_decomp/bodies/{unit}/{tu}/{name}.c"
        # The branch's integrated definition is the truth (a body file can be a later,
        # unmatched edit); fall back to the body file's leading declarations.
        r = subprocess.run(["git", "show", f"{a.branch}:src/{unit}/{tu}.c"], cwd=ROOT, capture_output=True, text=True)
        try:
            defn_branch = tu_edit.extract_function(r.stdout, name)
        except Exception:
            defn_branch = None
        text = body.read_text() if body.is_file() else ""
        if text.lstrip().startswith("/* m2c draft"):
            text = ""
        if defn_branch:
            blead, _bdef = tu_edit.split_leading_declarations(text) if text else ([], "")
            text = "\n".join(blead + [defn_branch])
        if not text:
            results[key] = {"matched": False, "why": "no body"}
            continue
        lead, defn = tu_edit.split_leading_declarations(text)
        tu_names = set()
        for item in tu_edit.scan(src.read_text(errors="replace")):
            if item.kind == "decl":
                tu_names |= names_of(item.text(src.read_text(errors="replace")))
        keep, hoist = [], set()
        for d in lead:
            n = names_of(d)
            if n and n & tu_names:
                hoist |= n & tu_names
            else:
                keep.append(d)
        if hoist:
            run(ROOT / "scripts/auto_decomp/hoist.py", "--unit", unit, "--tu", tu, "--before", name,
                *[x for h in sorted(hoist) for x in ("--name", h)])
        port = scratch / f"{unit}__{tu}__{name}.port.c"
        port.write_text("\n".join(keep + [defn]) + "\n")
        for _round in range(3):
            r = run(ROOT / "scripts/auto_decomp/integrate.py", "--unit", unit, "--tu", tu, "--name", name, "--body", port)
            ok = bool(r.get("matched"))
            err = (r.get("try") or {}).get("error") or r.get("error") or r.get("refused")
            if ok or not err:
                break
            # names the compiler has not seen yet but the TU declares further down: hoist them
            missing = set(re.findall(r"`(\w+)' undeclared|parse error before `?\"?(\w+)", str(err)))
            missing = {m for pair in missing for m in pair if m} & tu_names
            missing -= hoist
            if not missing:
                break
            hoist |= missing
            run(ROOT / "scripts/auto_decomp/hoist.py", "--unit", unit, "--tu", tu, "--before", name,
                *[x for h in sorted(missing) for x in ("--name", h)])
        results[key] = {"matched": ok, "dropped": sorted(hoist), "error": None if ok else str(err)[:300]}
        if ok:
            body.write_text(port.read_text())
        out_path.write_text(json.dumps(results, indent=1))
        print(f"{key}: {'MATCH' if ok else 'FAIL'} (dropped {len(hoist)})", flush=True)
    m = sum(1 for v in results.values() if v.get("matched"))
    print(json.dumps({"matched": m, "failed": len(results) - m}))
    return 0


if __name__ == "__main__":
    sys.exit(main())

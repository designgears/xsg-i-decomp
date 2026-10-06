#!/usr/bin/env python3
"""Permuter stage: drain near-miss failures through permute_fn.py.

    python3 -B scripts/auto_decomp/permute_queue.py [--min-score 0.5] [--minutes 10]
        [--parallel 1] [-j N] [--rounds 2] [--once]

Picks functions that are still INCLUDE_ASM, have a saved body and a best score
(state.json failure record or bodies/*.best.json) >= --min-score, best first,
skipping diffs no permutation can fix (VU0/sync asm, inline asm, cc1 ICE); leases each (owner
"permuter") so agents leave it alone, permutes it, integrates on a match.
Each function gets at most --rounds permuter runs. Without --once it keeps
polling for new near-misses every 2 minutes.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import claims  # noqa: E402
from build_queue import build_queue  # noqa: E402
from state import key, load_state, locked, log, save_state  # noqa: E402


# Diffs no source permutation can fix: VU0 / COP2 and sync / break (inline asm
# only), and cc1 crashing in the full TU.
UNPERMUTABLE_ASM = re.compile(r"\b(lqc2|sqc2|qmtc2|qmfc2|vcallms|sync|v[a-z]+\.[xyzw]+)\b|\bbreak\s+0\s*$", re.M)


def unpermutable(c: dict, body: Path, f: dict) -> str | None:
    try:
        if UNPERMUTABLE_ASM.search((ROOT / c["asm"]).read_text()):
            return "vu0/sync/break asm"
    except OSError:
        pass
    if "__asm__" in body.read_text(errors="replace"):
        return "inline asm in body"
    if "Internal compiler error" in json.dumps(f):
        return "cc1 ICE"
    return None


def best_json(body: Path) -> float:
    try:
        return float(json.loads(body.with_name(body.stem + ".best.json").read_text()).get("score") or 0)
    except (OSError, ValueError):
        return 0.0


def pick(min_score: float, rounds: int, limit: int) -> list[dict]:
    """Pending functions with a saved body, best score first. The score is the
    higher of the failure record (state.json) and the agents' <name>.best.json,
    so near-misses that never went through integrate --give-up count too."""
    cands = build_queue(ROOT)["candidates"]
    state = load_state()
    out = []
    with claims.locked():
        taken = claims.active()
        ranked = []
        for c in cands:
            k = key(c["unit"], c["tu"], c["name"])
            if k in taken:
                continue
            f = state["failed"].get(k) or {}
            if f.get("permute_runs", 0) >= rounds:
                continue
            body = ROOT / "build/auto_decomp/bodies" / c["unit"] / c["tu"] / f"{c['name']}.c"
            if body.with_name(f"{c['name']}.best.c").is_file():
                body = body.with_name(f"{c['name']}.best.c")
            if not body.is_file():
                continue
            sc = max(f.get("best_score") or 0, best_json(ROOT / "build/auto_decomp/bodies" / c["unit"]
                                                         / c["tu"] / f"{c['name']}.c"))
            if sc < min_score or sc >= 1:
                continue
            if unpermutable(c, body, f):
                continue
            ranked.append((sc, {"key": k, "unit": c["unit"], "tu": c["tu"], "name": c["name"],
                                "body": body, "score": sc}))
        ranked.sort(key=lambda x: -x[0])
        out = [r for _, r in ranked[:limit]]
    return out


def run_one(c: dict, minutes: float, jobs: int) -> dict:
    with claims.locked():
        if c["key"] in claims.active():
            return {"key": c["key"], "skipped": "claimed"}
        claims.take(c["key"], "permuter", int(minutes * 60) + 600)
    try:
        p = subprocess.run([sys.executable, "-B", str(HERE / "permute_fn.py"), "--unit", c["unit"],
                            "--tu", c["tu"], "--name", c["name"], "--body", str(c["body"]),
                            "--minutes", str(minutes), "-j", str(jobs)],
                           capture_output=True, text=True, cwd=ROOT)
        try:
            r = json.loads(p.stdout)
        except json.JSONDecodeError:
            r = {"ok": False, "stage": "crash", "error": (p.stdout + p.stderr)[-600:]}
    finally:
        claims.release(c["key"])
    with locked():
        s = load_state()
        if not r.get("ok"):                       # count runs for .best.json-only picks too
            f = s["failed"].setdefault(c["key"], {"attempts": 0})
            f["permute_runs"] = f.get("permute_runs", 0) + 1
            f["permute_last"] = {"stage": r.get("stage"), "best": r.get("best_permuter_score"),
                                 "error": (r.get("error") or "")[-300:]}
            save_state(s)
    log(f"PERMUTE {c['key']} ok={r.get('ok')} stage={r.get('stage')} best={r.get('best_permuter_score')}")
    return {"key": c["key"], "ok": r.get("ok"), "stage": r.get("stage"),
            "best": r.get("best_permuter_score"), "error": (r.get("error") or "")[-200:]}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--min-score", type=float, default=0.9)
    ap.add_argument("--minutes", type=float, default=10)
    ap.add_argument("--parallel", type=int, default=1)
    ap.add_argument("-j", type=int, default=max(1, ((os.cpu_count() or 4) - 2)))
    ap.add_argument("--rounds", type=int, default=2)
    ap.add_argument("--once", action="store_true")
    a = ap.parse_args()
    while True:
        batch = pick(a.min_score, a.rounds, a.parallel)
        if not batch:
            if a.once:
                break
            time.sleep(120)
            continue
        with ThreadPoolExecutor(a.parallel) as ex:
            for r in ex.map(lambda c: run_one(c, a.minutes, a.j), batch):
                print(json.dumps(r), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

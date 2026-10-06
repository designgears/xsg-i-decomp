#!/usr/bin/env python3
"""Permuter stage: drain near-miss failures through permute_fn.py.

    python3 -B scripts/auto_decomp/permute_queue.py [--min-score 0.5] [--minutes 10]
        [--parallel 3] [-j 4] [--rounds 2] [--once]

Picks failed functions (state.json) that are still INCLUDE_ASM, have a saved
body and a best try score >= --min-score, best first; leases each (owner
"permuter") so agents leave it alone, permutes it, integrates on a match.
Each function gets at most --rounds permuter runs. Without --once it keeps
polling for new near-misses every 2 minutes.
"""
from __future__ import annotations

import argparse
import json
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


def pick(min_score: float, rounds: int, limit: int) -> list[dict]:
    pending = {key(c["unit"], c["tu"], c["name"]) for c in build_queue(ROOT)["candidates"]}
    state = load_state()
    out = []
    with claims.locked():
        taken = claims.active()
        sc = lambda f: max(f.get("best_score") or 0, f.get("best_opcode_score") or 0)  # noqa: E731
        for k, f in sorted(state["failed"].items(), key=lambda kv: -sc(kv[1])):
            if k not in pending or k in taken or f.get("blocked"):
                continue
            if sc(f) < min_score or f.get("permute_runs", 0) >= rounds:
                continue
            u, t, n = k.split("/")
            body = ROOT / "build/auto_decomp/bodies" / u / t / f"{n}.c"
            if body.with_name(f"{n}.best.c").is_file():
                body = body.with_name(f"{n}.best.c")
            if not body.is_file():
                continue
            out.append({"key": k, "unit": u, "tu": t, "name": n, "body": body,
                        "score": f.get("best_score")})
            if len(out) >= limit:
                break
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
        f = s["failed"].get(c["key"])
        if f is not None and not r.get("ok"):
            f["permute_runs"] = f.get("permute_runs", 0) + 1
            f["permute_last"] = {"stage": r.get("stage"), "best": r.get("best_permuter_score"),
                                 "error": (r.get("error") or "")[-300:]}
            save_state(s)
    log(f"PERMUTE {c['key']} ok={r.get('ok')} stage={r.get('stage')} best={r.get('best_permuter_score')}")
    return {"key": c["key"], "ok": r.get("ok"), "stage": r.get("stage"),
            "best": r.get("best_permuter_score"), "error": (r.get("error") or "")[-200:]}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--min-score", type=float, default=0.5)
    ap.add_argument("--minutes", type=float, default=10)
    ap.add_argument("--parallel", type=int, default=3)
    ap.add_argument("-j", type=int, default=4)
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

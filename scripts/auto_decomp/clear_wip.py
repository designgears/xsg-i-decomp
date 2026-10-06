#!/usr/bin/env python3
"""Reset the loop's work in progress without losing near-misses.

    python3 -B scripts/auto_decomp/clear_wip.py [--dry-run]

Releases every lease, deletes try counters and permuter work, deletes the
scratch body files of functions that are still INCLUDE_ASM, and clears their
failure records -- but keeps:
  - each function's best body (<name>.best.c / .best.json): a retry starts
    from the closest earlier attempt instead of from scratch;
  - `blocked` markers (compiler/toolchain decisions), which stay in state.json.
Run it under the global lock only when no agent or permuter is running.
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import claims  # noqa: E402
from build_queue import build_queue  # noqa: E402
from state import STATE_DIR, key, load_state, locked, log, save_state  # noqa: E402

ROOT = HERE.parents[1]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    pending = {key(c["unit"], c["tu"], c["name"]) for c in build_queue(ROOT)["candidates"]}

    leases = list((STATE_DIR / "claims").glob("*.json"))
    tries = list((STATE_DIR / "tries").glob("*"))
    bodies = []
    for p in (STATE_DIR / "bodies").glob("*/*/*"):
        unit, tu = p.parent.parent.name, p.parent.name
        name = p.name.split(".")[0]
        if ".best." in p.name or key(unit, tu, name) not in pending:
            continue                                  # keep best bodies; matched ones are history
        bodies.append(p)
    permute = STATE_DIR / "permute"

    with locked():
        state = load_state()
        failed = state.get("failed") or {}
        keep = {k: {"attempts": 0, "blocked": f["blocked"]} for k, f in failed.items() if f.get("blocked")}
        cleared = len(failed) - len(keep)
        summary = (f"CLEAR WIP: released {len(leases)} leases, {len(tries)} try counters, permute work, "
                   f"{len(bodies)} unmatched body files, {cleared} failed records "
                   f"(kept {len(keep)} blocked markers and all .best bodies)")
        if a.dry_run:
            print(summary)
            return 0
        with claims.locked():
            for p in leases:
                p.unlink(missing_ok=True)
        for p in tries + bodies:
            p.unlink(missing_ok=True)
        shutil.rmtree(permute, ignore_errors=True)
        state["failed"] = keep
        save_state(state)
    log(summary)
    print(summary)
    return 0


if __name__ == "__main__":
    sys.exit(main())

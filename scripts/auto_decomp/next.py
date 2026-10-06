#!/usr/bin/env python3
"""Emit the next N eligible INCLUDE_ASM candidates from the queue.

The source is the truth: a function is a candidate while its TU still has it
as INCLUDE_ASM. A key recorded as matched but back to INCLUDE_ASM (a lost
edit) is dropped from state.matched and offered again.

With --claim the returned candidates are leased (claims.py) so a concurrent
batch never gets them; integrate.py releases the lease. --per-tu spreads a
batch across translation units.
"""
from __future__ import annotations

import argparse
import difflib
import fcntl
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import claims  # noqa: E402
import m2c_seed  # noqa: E402
from build_queue import build_queue  # noqa: E402
from state import QUEUE_PATH, key, load_state, locked, log, save_state  # noqa: E402

MAX_ATTEMPTS = 5


def refresh_queue(root: Path) -> dict:
    q = build_queue(root)
    QUEUE_PATH.parent.mkdir(parents=True, exist_ok=True)
    tmp = QUEUE_PATH.with_suffix(".tmp")
    tmp.write_text(json.dumps(q, indent=1) + "\n")
    tmp.replace(QUEUE_PATH)
    return q


def reconcile(q: dict) -> list[str]:
    """Drop matched keys whose function is INCLUDE_ASM again. Returns them."""
    pending = {key(c["unit"], c["tu"], c["name"]) for c in q["candidates"]}
    with locked():
        state = load_state()
        lost = [k for k in state["matched"] if k in pending]
        if lost:
            state["matched"] = [k for k in state["matched"] if k not in pending]
            state["matched_count"] = len(state["matched"])
            state.setdefault("lost", {})
            for k in lost:
                state["lost"][k] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
                log(f"LOST {k}: recorded matched but INCLUDE_ASM in source; requeued")
            save_state(state)
    return lost


_MNEM = re.compile(r"^\s*/\*[^*]*\*/\s+([a-z0-9.]+)")


def _mnemonics(path: Path) -> list[str]:
    try:
        return [m.group(1) for m in map(_MNEM.match, path.read_text().splitlines()) if m]
    except OSError:
        return []


_TRAPPING = {"add", "addi", "sub", "dadd", "daddi", "dsub"}


def genuine_asm(root: Path, c: dict) -> bool:
    """Hand-written asm no C compiler emits: splat tags trapping arithmetic
    (add/addi/sub/dadd...) as handwritten — gcc only emits the *u forms. These
    stay INCLUDE_ASM; single privileged insns (syscall, ei/di, mtc0) do not
    count, they are fine as one-instruction inline asm."""
    try:
        text = (root / c["asm"]).read_text(errors="replace")
    except OSError:
        return False
    if "Handwritten function" not in text:
        return False
    for line in text.splitlines():
        if "handwritten instruction" in line:
            m = _MNEM.match(line)
            if m and m.group(1) in _TRAPPING:
                return True
    return False


def sibling(root: Path, c: dict, pending: set) -> dict | None:
    """Closest already-matched function in the same TU by opcode sequence —
    near-identical asm usually means a copy-paste sibling whose C transfers."""
    asm = root / c["asm"]
    mine = _mnemonics(asm)
    if len(mine) < 4:
        return None
    best = None
    for other in asm.parent.glob("*.s"):
        n = other.stem
        if n == c["name"] or key(c["unit"], c["tu"], n) in pending:
            continue
        theirs = _mnemonics(other)
        if not theirs or abs(len(theirs) - len(mine)) > max(4, len(mine) // 4):
            continue
        r = difflib.SequenceMatcher(None, mine, theirs, autojunk=False).ratio()
        if r >= 0.85 and (best is None or r > best["similarity"]):
            best = {"name": n, "similarity": round(r, 3)}
    return best


def load_fn_index(root: Path) -> dict:
    """fn_index.py's instruction index (built on first use, ~1 s), with exact-copy
    groups by shape. {} when it cannot be built: siblings are an aid, not a gate."""
    path = root / "build/auto_decomp/fn_index.json"
    try:
        if not path.is_file():
            subprocess.run([str(root / ".venv/bin/python"), "-B", str(root / "scripts/auto_decomp/fn_index.py")],
                           cwd=root, capture_output=True, timeout=300)
        idx = json.loads(path.read_text())
    except Exception:
        return {}
    shapes: dict[str, list[dict]] = {}
    for v in idx.values():
        shapes.setdefault(v["shape"], []).append(v)
    return {"fns": idx, "shapes": shapes}


def twin_in_flight(fx: dict, c: dict, pending: set, busy: set) -> str | None:
    """An exact copy of c (same shape, other TU/unit) that is leased right now:
    leave c for later, when that copy's matched C becomes its sibling."""
    me = fx.get("fns", {}).get(f"{c['unit']}/{c['name']}")
    if not me:
        return None
    for v in fx["shapes"].get(me["shape"], ()):
        k = key(v["unit"], v["tu"], v["name"])
        if v is not me and k in pending and k in busy:
            return k
    return None


def cross_sibling(fx: dict, c: dict, pending: set, threshold: float = 0.9) -> dict | None:
    """Closest already-matched function in ANOTHER TU or unit (copies across
    overlays and TUs): fn_index.py has every function's words from the originals."""
    me = fx.get("fns", {}).get(f"{c['unit']}/{c['name']}")
    if not me:
        return None
    mine = me["mn"].split()
    n = len(mine)
    if n < 4:
        return None
    sm = difflib.SequenceMatcher(None, autojunk=False)
    sm.set_seq2(mine)
    best = None
    for v in fx["fns"].values():
        if (v["unit"], v["tu"]) == (c["unit"], c["tu"]) or key(v["unit"], v["tu"], v["name"]) in pending:
            continue
        if not v["c"] or abs(v["mn"].count(" ") + 1 - n) > max(4, n // 4):
            continue
        sm.set_seq1(v["mn"].split())
        if sm.real_quick_ratio() < threshold or sm.quick_ratio() < threshold:
            continue
        r = sm.ratio()
        if r >= threshold and (best is None or r > best["similarity"]):
            best = {"name": v["name"], "similarity": round(r, 3), "unit": v["unit"], "tu": v["tu"],
                    "source": f"src/{v['unit']}/{v['tu']}.c"}
    return best


def name_sibling(fx: dict, c: dict, pending: set, min_sim: float = 0.5) -> dict | None:
    """An already-matched function in the same TU whose name shares a long prefix
    (nmlModelSetGlobalPointLight -> ...LightCol, scMISSILEScript -> scMISSILE3Script):
    families like these often transfer their C even when the instruction
    similarity is below the sibling threshold."""
    me = fx.get("fns", {}).get(f"{c['unit']}/{c['name']}")
    name = c["name"]
    if not me:
        return None
    best, best_len = None, 0
    for v in fx.get("fns", {}).values():
        if (v["unit"], v["tu"]) != (c["unit"], c["tu"]) or v["name"] == name or not v["c"]:
            continue
        if key(v["unit"], v["tu"], v["name"]) in pending:
            continue
        n = len(os.path.commonprefix([name.lower(), v["name"].lower()]))
        if n < max(6, int(0.6 * min(len(name), len(v["name"])))) or n <= best_len:
            continue
        sim = difflib.SequenceMatcher(None, me["mn"].split(), v["mn"].split(), autojunk=False).ratio()
        if sim < min_sim:
            continue
        best_len = n
        best = {"name": v["name"], "similarity": round(sim, 3), "by": "name"}
    return best


def refresh_protos(root: Path, max_age_s: int = 3600) -> None:
    """Rebuild the cross-TU prototype index (proto_index.py, ~30 s) and the
    instruction index (fn_index.py, ~1 s) when stale, so m2c drafts see the
    prototypes of recently matched callees."""
    out = root / "build/auto_decomp/protos.json"
    try:
        if out.is_file() and time.time() - out.stat().st_mtime < max_age_s:
            return
        with (root / "build/auto_decomp/protos.lock").open("a") as fh:
            try:
                fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                return                       # another claimant is rebuilding it
            for script in ("proto_index.py", "fn_index.py"):
                subprocess.run([str(root / ".venv/bin/python"), "-B",
                                str(root / "scripts/auto_decomp" / script)],
                               cwd=root, capture_output=True, timeout=600)
    except Exception:                        # a draft aid, never a blocker
        pass


def refresh_ghidra(root: Path, max_age_s: int = 86400) -> None:
    """Start scripts/ghidra/refresh.sh detached (~2-3 min, it locks itself) when
    the Ghidra export is over a day old, so its types follow the matched C."""
    stamp = root / "build/ghidra/.stamp"
    try:
        if stamp.is_file() and time.time() - stamp.stat().st_mtime < max_age_s:
            return
        stamp.parent.mkdir(parents=True, exist_ok=True)
        stamp.touch()                        # claim the slot; refresh.sh re-touches on success
        subprocess.Popen([str(root / "scripts/ghidra/refresh.sh")], cwd=root, start_new_session=True,
                         stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception:
        pass


def eligible(root: Path, limit: int, max_size: int | None, units: set[str] | None,
             per_tu: int | None = None, claim_owner: str | None = None,
             ttl_s: int = claims.DEFAULT_TTL_S, min_size: int = 0,
             keys: set[str] | None = None, include_blocked: bool = False,
             claim_model: str | None = None, near_miss: float | None = None,
             untouched: bool = False) -> list[dict]:
    """keys: only these candidates, ignoring size limits, attempt caps and back-off
    (a deliberate retry); blocked and genuine-asm ones are still skipped.
    near_miss: only candidates whose best score (failure record or the agents'
    bodies/<name>.best.json) is >= near_miss, best first, ignoring size limits,
    attempt caps and back-off. untouched: only candidates nobody has tried yet
    (no failure record, try counter or saved best)."""
    q = refresh_queue(root)
    reconcile(q)
    state = load_state()
    failed = state.get("failed") or {}
    now = time.time()
    out, per = [], {}
    pending = {key(c["unit"], c["tu"], c["name"]) for c in q["candidates"]}
    fx = load_fn_index(root)
    def best_of(c: dict) -> float:
        f = failed.get(key(c["unit"], c["tu"], c["name"])) or {}
        try:
            b = json.loads((root / "build/auto_decomp/bodies" / c["unit"] / c["tu"]
                            / f"{c['name']}.best.json").read_text()).get("score") or 0
        except (OSError, ValueError):
            b = 0
        return max(f.get("best_score") or 0, b)

    cands = q["candidates"]
    if near_miss is not None:
        scored = [(best_of(c), c) for c in cands]
        cands = [c for sc, c in sorted(scored, key=lambda x: -x[0]) if near_miss <= sc < 1]
    if untouched:
        tries = root / "build/auto_decomp/tries"
        bodies = root / "build/auto_decomp/bodies"
        cands = [c for c in cands if key(c["unit"], c["tu"], c["name"]) not in failed
                 and not (tries / f"{c['unit']}__{c['tu']}__{c['name']}").exists()
                 and not (bodies / c["unit"] / c["tu"] / f"{c['name']}.best.json").exists()]
    free = keys is not None or near_miss is not None   # no size/attempt/back-off limits
    with claims.locked():
        taken = claims.active(now)
        busy = set(taken)
        # callee-first: candidates blocked by a same-TU asm callee above them
        # (callgraph.py) are deferred to a second pass, used only when nothing
        # else is eligible; --include-blocked / --key lift the deferral.
        lift = include_blocked or free
        for deferred_pass in (False, True):
            for c in cands:
                if bool(c.get("blocked_by")) != deferred_pass and not lift:
                    continue
                if units and c["unit"] not in units:
                    continue
                if keys is not None and key(c["unit"], c["tu"], c["name"]) not in keys:
                    continue
                if not free and (c["size"] < min_size or (max_size is not None and c["size"] > max_size)):
                    continue
                k = key(c["unit"], c["tu"], c["name"])
                if k in taken or not c.get("asm"):
                    continue
                f = failed.get(k)
                if not free and f and f.get("attempts", 0) >= MAX_ATTEMPTS:
                    continue
                if f and f.get("blocked"):           # needs a toolchain/compiler decision
                    continue
                if not free and f and f.get("next_eligible") and f["next_eligible"] > now:
                    continue
                if genuine_asm(root, c):
                    continue
                if twin_in_flight(fx, c, pending, busy):
                    continue
                tk = (c["unit"], c["tu"])
                if per_tu and per.get(tk, 0) >= per_tu:
                    continue
                per[tk] = per.get(tk, 0) + 1
                c = dict(c, key=k)
                if c.get("blocked_by"):
                    c["blocked_by_note"] = ("same-TU callee(s) above are still INCLUDE_ASM: expect a "
                                            "delay-slot/branch-likely plateau until they are C")
                c["body"] = f"build/auto_decomp/bodies/{c['unit']}/{c['tu']}/{c['name']}.c"
                (ROOT / c["body"]).parent.mkdir(parents=True, exist_ok=True)
                if f:
                    c["previous"] = {x: f.get(x) for x in ("attempts", "last_error", "best_score", "hint") if f.get(x) is not None}
                sib = sibling(root, c, pending) or cross_sibling(fx, c, pending) or name_sibling(fx, c, pending)
                if sib:
                    c["sibling"] = sib
                if claim_owner:
                    claims.take(k, claim_owner, ttl_s, claim_model)
                    (root / "build/auto_decomp/tries" / f"{c['unit']}__{c['tu']}__{c['name']}").unlink(missing_ok=True)
                out.append(c)
                busy.add(k)
                if len(out) >= limit:
                    break
            if out or lift:
                break
    if claim_owner and out:
        refresh_protos(root)
        refresh_ghidra(root)
        for c in out:        # m2c first draft; never overwrites an earlier attempt's body
            try:
                r = m2c_seed.seed(c["unit"], c["tu"], c["name"], (root / c["asm"]).resolve(),
                                  (root / c["body"]).resolve(), write_body=True)
            except Exception as e:            # a draft is a convenience, never a blocker
                r = {"ok": False, "error": f"{type(e).__name__}: {e}"}
            if r.get("ghidra"):
                c["ghidra"] = r["ghidra"]
            if r.get("ok"):
                c["m2c"] = r["draft"]
                c["m2c_seeded_body"] = bool(r.get("seeded_body"))
                if r.get("partial"):
                    c["m2c_partial"] = True
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("-n", type=int, default=5)
    ap.add_argument("--max-size", type=int, default=64)
    ap.add_argument("--min-size", type=int, default=0)
    ap.add_argument("--unit", action="append", default=None)
    ap.add_argument("--per-tu", type=int, default=1, help="max candidates per TU (0 = no limit)")
    ap.add_argument("--key", action="append", default=None, metavar="UNIT/TU/NAME",
                    help="only these candidates, bypassing size limits and back-off (repeatable)")
    ap.add_argument("--include-blocked", action="store_true",
                    help="do not defer candidates whose same-TU callee above them is still INCLUDE_ASM")
    ap.add_argument("--claim", metavar="OWNER", help="lease the returned candidates to OWNER")
    ap.add_argument("--model", help="model the leasing worker runs on (stored in the claim, shown by status.py)")
    ap.add_argument("--ttl-min", type=int, default=claims.DEFAULT_TTL_S // 60)
    ap.add_argument("--near-miss", type=float, metavar="SCORE",
                    help="only functions already scored >= SCORE (failure record or .best.json), best first; "
                         "ignores size limits, attempt caps and back-off")
    ap.add_argument("--untouched", action="store_true",
                    help="only functions nobody has tried (no failure record, try counter or .best.json)")
    a = ap.parse_args()
    units = set(a.unit) if a.unit else None
    items = eligible(a.root, a.n, a.max_size, units, a.per_tu or None, a.claim,
                     a.ttl_min * 60, a.min_size, set(a.key) if a.key else None, a.include_blocked,
                     a.model, a.near_miss, a.untouched)
    q = json.loads(QUEUE_PATH.read_text())
    print(json.dumps({"count": len(items), "queue_total": q["total"],
                      "buckets": q["by_size_bucket"], "candidates": items}, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

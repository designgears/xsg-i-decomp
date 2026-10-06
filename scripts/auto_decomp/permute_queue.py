#!/usr/bin/env python3
"""Permuter stage: drain near-miss failures through permute_fn.py.

    python3 -B scripts/auto_decomp/permute_queue.py [--min-score 0.85] [--max-diff 25]
        [--minutes 5] [--parallel 4] [-j 3] [--rounds 1] [--once]

Picks functions that are still INCLUDE_ASM, have a saved body and a best score
(state.json failure record or bodies/*.best.json) >= --min-score AND at most
--max-diff differing instruction words, fewest differences first. The score is
a ratio, so it alone over-rates big functions: 0.85 of a 2000-insn function is
~300 wrong words, far past what permuting closes, while 0.85 of 40 insns is 6.
The word count comes from .best.json (try_fn records it) or is estimated as
(1 - score) * size / 4. Past runs: ~24% matched from 0.8-0.9, ~10% below 0.8.
skipping diffs no permutation can fix (VU0/sync asm, inline asm, cc1 ICE); leases each (owner
"permuter") so agents leave it alone, permutes it, integrates on a match.
Each function gets at most --rounds permuter runs. Without --once it keeps
polling for new near-misses every 2 minutes.

Before the permuter (cheapest first):
  * triage: one try_fn.try_function on the best body classifies the remaining
    diff (cached in build/auto_decomp/triage.json, keyed by the body's mtime):
      match       already matches, or only false %lo relocations /
                  absolute-vs-symbol slots differ (try_fn match_after_link /
                  link_check) -> integrate.py (--skip-try for the latter)
      reloc-only  bytes match, only relocation names differ -> integrate.py
                  (the data carve often passes it), never permuted
      callee      has same-TU INCLUDE_ASM callees above it (callgraph
                  blocked_by) and the diff is only branch-likely / delay-slot
                  moves -> skipped; retry_best.py retries it once the callee
                  matches
      assembler   only delay-slot moves of la/li macro halves (lui/addiu/ori)
                  and the TU has no -mswap-fix in config/tu/as-flags.json ->
                  skipped, listed by --triage-report (as-flags is never edited)
      permute     anything else; a diff that is only a branch-likely bit
                  flip (bnez vs bnezl) is tagged "branch_likely": the sweep's
                  fallthrough_label family covers it and the permuter runs
                  with --profile branch_likely (label/goto/condition weights).
                  Mechanism tags ("mech", from trick_sweep.diff_shape):
                  cmp_fix (branch condition differs, not an inversion: the C
                  comparison is wrong), sched_weight (one store moved: sched1
                  register-weight tie), de_ifcvt (move next to movz/movn:
                  early if-conversion), loop_live (address/constant inits
                  reordered: loop-preheader tie); the sweep runs the matching
                  ctx family first-class
  * sweep first: a body trick_sweep has not run on (.best.json "by"=="sweep"
    or a sweep_marks.json sha1 / loop.log "done" line) is swept inline under
    the same lease; the permuter only runs if that did not match.
  * order: fewest prior permuter rounds first, then fewest differing words.

    permute_queue.py --triage-report [--json]   classify the pool, print counts
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
import callgraph  # noqa: E402
import claims  # noqa: E402
import trick_sweep  # noqa: E402
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


def best_json(body: Path) -> dict:
    try:
        return json.loads(body.with_name(body.stem + ".best.json").read_text())
    except (OSError, ValueError):
        return {}


TRIAGE_VERSION = 4      # bump when classification rules change (invalidates the cache)
TRIAGE = ROOT / "build/auto_decomp/triage.json"
AS_FLAGS = ROOT / "config/tu/as-flags.json"
BRANCH = re.compile(r"^(b|bal|j|jr|jal|jalr|beq|bne|beqz|bnez|bgez|bgtz|blez|bltz|bgezal|bltzal|bc1t|bc1f)l?$")
MACRO_HALF = re.compile(r"^(lui|addiu|ori|li)$")
_CG: dict | None = None


def _cg() -> dict:
    global _CG
    if _CG is None:
        try:
            _CG = callgraph.build(ROOT)
        except Exception:  # noqa: BLE001
            _CG = {}
    return _CG


def _norm(insn: str) -> tuple[str, str]:
    """(mnemonic with branch-likely folded, normalized text without branch target)."""
    insn = insn.strip()
    rel = re.search(r"\[(\w+) ", insn)
    insn = re.sub(r"\s*\[.*$", "", insn)
    parts = insn.split(None, 1)
    if not parts:
        return "", ""
    mn, ops = parts[0], (parts[1] if len(parts) > 1 else "")
    if BRANCH.match(mn):
        base = mn[:-1] if mn.endswith("l") and mn not in ("bal", "jal", "jalr") else mn
        ops = ",".join(o for o in ops.split(",") if not re.fullmatch(r"[0-9a-f]+", o.strip()))
        return base, f"{base} {ops}"
    return mn, f"{mn} {ops}" + (f" [{rel.group(1)}]" if rel else "")


def classify_diff(rows: list[str]) -> tuple[str, list[str]]:
    """'delay' when every differing row is a branch / nop / branch-likely flip
    or an instruction that merely moved by a few rows next to a branch;
    'macro' when additionally all moved insns are la/li halves; else 'other'."""
    parsed = []
    if not rows or "   CANDIDATE" not in rows[0]:
        return "other", []
    w = rows[0].index("   CANDIDATE")          # side_by_side: f"{l:<w} {mark} {r}"
    side = lambda x: x.split(":", 1)[1] if ":" in x else ""  # noqa: E731
    for r in rows[1:]:
        if r.startswith("... ("):
            return "other", []
        parsed.append((side(r[:w]), r[w + 1:w + 2], side(r[w + 3:])))
    if not parsed:
        return "other", []
    tl, cl = {}, {}   # normalized text -> count (differing rows only)
    moved_near = True
    diff_idx = [i for i, p in enumerate(parsed) if p[1] != " "]
    if not diff_idx:
        return "other", []
    for i in diff_idx:
        a, _, b = parsed[i]
        for x, bag in ((a, tl), (b, cl)):
            if not x.strip():
                continue
            mn, t = _norm(x)
            if mn == "nop" or BRANCH.match(mn):
                continue
            bag[t] = bag.get(t, 0) + 1
            near = any(BRANCH.match(_norm(parsed[j][0])[0] or "x") or BRANCH.match(_norm(parsed[j][2])[0] or "x")
                       for j in range(max(0, i - 2), min(len(parsed), i + 3)))
            moved_near &= near
    if tl != cl:
        return "other", []
    if not moved_near:
        return "reorder", sorted(tl)   # pure scheduling move: permuter territory
    moved = sorted(tl)
    # la = lui + addiu %lo (LO16 reloc), li = lui + ori / li: plain addiu is not a macro half
    if moved and all(t.startswith(("lui ", "li ")) or (MACRO_HALF.match(t.split()[0]) and "[LO16]" in t)
                     for t in moved):
        return "macro", moved
    return "delay", moved


def branch_likely_only(rows: list[str]) -> bool:
    """Every differing row is the same branch with only the likely bit flipped
    (bnez vs bnezl, same operands): reorg's annul choice, decided by the
    label / jump layout at the fall-through."""
    if not rows or "   CANDIDATE" not in rows[0]:
        return False
    w = rows[0].index("   CANDIDATE")
    side = lambda x: x.split(":", 1)[1].strip() if ":" in x else ""  # noqa: E731
    n = 0
    for r in rows[1:]:
        if r.startswith("... ("):
            return False
        if r[w + 1:w + 2] == " ":
            continue
        a, b = side(r[:w]), side(r[w + 3:])
        if not a or not b:
            return False
        ma, mb = a.split()[0], b.split()[0]
        if not (BRANCH.match(ma) and BRANCH.match(mb)) or ma == mb or ma.rstrip("l") != mb.rstrip("l"):
            return False
        if _norm(a)[1] != _norm(b)[1]:
            return False
        n += 1
    return n > 0


def triage(c: dict, body: Path) -> dict:
    """Cached classification of C's remaining diff (see module doc)."""
    k = c["key"]
    mt = body.stat().st_mtime
    try:
        cache = json.loads(TRIAGE.read_text())
    except (OSError, ValueError):
        cache = {}
    e = cache.get(k)
    if e and e.get("mtime") == mt and e.get("body") == str(body) and e.get("v") == TRIAGE_VERSION:
        return e
    import try_fn
    try:
        r = try_fn.try_function(c["unit"], c["tu"], c["name"], body, diff_limit=100000)
    except Exception as ex:  # noqa: BLE001
        r = {"ok": False, "error": str(ex)[:200]}
    e = {"v": TRIAGE_VERSION, "mtime": mt, "body": str(body), "score": r.get("score"), "words": r.get("differing_words")}
    blocked = (_cg().get(k) or {}).get("blocked_by") or []
    try:
        flags = json.loads(AS_FLAGS.read_text())
    except (OSError, ValueError):
        flags = {}
    if not r.get("ok"):
        e["cat"] = "permute"
        e["why"] = f"try failed: {r.get('stage')}"
    elif r.get("match") or try_fn.is_link_match(r):
        e["cat"] = "match"
        if not r.get("match"):
            e["link"] = True             # integrate with --skip-try (whole-file gate decides)
            e["why"] = ("false %lo reloc " + ",".join(f["sym"] for f in r.get("false_lo16") or [])
                        if r.get("match_after_link") else "absolute-vs-symbol slots")
    elif r.get("bytes_match"):
        e["cat"] = "reloc-only"
        e["why"] = json.dumps(r.get("reloc_symbol_diffs", [])[:3])
    else:
        kind, moved = classify_diff(r.get("diff") or [])
        e["moved"] = moved
        if branch_likely_only(r.get("diff") or []):
            e["tag"] = "branch_likely"   # trick_sweep fallthrough_label; permuter label/goto weights
        shape = trick_sweep.diff_shape(r.get("diff") or [])
        if shape:                        # mechanism tags (trick_sweep ctx families, WORKER_LESSONS.md)
            e["mech"] = sorted(shape)
            e.setdefault("tag", sorted(shape)[0])
        e["blocked_by"] = blocked
        if kind in ("delay", "macro") and blocked:
            e["cat"], e["why"] = "callee", "blocked_by " + ",".join(b.rsplit("/", 1)[1] for b in blocked)
        elif kind == "macro" and "-mswap-fix" not in flags.get(f"{c['unit']}/{c['tu']}", []):
            e["cat"], e["why"] = "assembler", "-mswap-fix candidate for " + f"{c['unit']}/{c['tu']}"
        else:
            e["cat"] = "permute"
            e["why"] = kind + "".join(" " + x for x in (["branch_likely"] if e.get("tag") == "branch_likely" else [])
                                      + e.get("mech", []))
    with locked():   # state lock serializes cache writers
        try:
            cache = json.loads(TRIAGE.read_text())
        except (OSError, ValueError):
            cache = {}
        cache[k] = e
        TRIAGE.parent.mkdir(parents=True, exist_ok=True)
        tmp = TRIAGE.with_suffix(".tmp")
        tmp.write_text(json.dumps(cache, indent=1, sort_keys=True) + "\n")
        os.replace(tmp, TRIAGE)
    return e


def pool(min_score: float, rounds: int, max_diff: int | None) -> list[dict]:
    """Unclaimed near-miss candidates passing the score / diff / permutability gates."""
    cands = build_queue(ROOT)["candidates"]
    state = load_state()
    with claims.locked():
        taken = claims.active()
    out = []
    for c in cands:
        k = key(c["unit"], c["tu"], c["name"])
        if k in taken:
            continue
        f = state["failed"].get(k) or {}
        runs = f.get("permute_runs", 0)
        if runs >= rounds:
            continue
        body = ROOT / "build/auto_decomp/bodies" / c["unit"] / c["tu"] / f"{c['name']}.c"
        if body.with_name(f"{c['name']}.best.c").is_file():
            body = body.with_name(f"{c['name']}.best.c")
        if not body.is_file():
            continue
        bj = best_json(ROOT / "build/auto_decomp/bodies" / c["unit"] / c["tu"] / f"{c['name']}.c")
        sc = max(f.get("best_score") or 0, float(bj.get("score") or 0))
        if sc < min_score or sc >= 1:
            continue
        diff = bj.get("differing_words") if bj.get("score") == sc else None
        if diff is None:
            diff = round((1 - sc) * (c.get("size") or 0) / 4)
        if max_diff is not None and diff > max_diff:
            continue
        if unpermutable(c, body, f):
            continue
        out.append({"key": k, "unit": c["unit"], "tu": c["tu"], "name": c["name"], "body": body,
                    "score": sc, "diff": diff, "runs": runs,
                    "integrate_tried": f.get("triage_integrate") == body.stat().st_mtime})
    return out


def pick(min_score: float, rounds: int, limit: int, max_diff: int | None = None) -> list[dict]:
    """Up to LIMIT candidates: triaged (callee / assembler skipped, reloc-only and
    match go to integrate once), fewest prior rounds first, then fewest words."""
    ranked = sorted(pool(min_score, rounds, max_diff), key=lambda r: (r["runs"], r["diff"], -r["score"]))
    out = []
    for r in ranked:
        t = triage(r, r["body"])
        r["cat"] = t["cat"]
        r["link"] = bool(t.get("link"))
        r["tag"] = t.get("tag")
        r["mech"] = t.get("mech") or []
        if t["cat"] in ("callee", "assembler"):
            continue
        if t["cat"] in ("match", "reloc-only") and r["integrate_tried"]:
            continue
        out.append(r)
        if len(out) >= limit:
            break
    return out


def triage_report(min_score: float, rounds: int, max_diff: int | None, as_json: bool) -> int:
    rs = pool(min_score, 10 ** 6, max_diff)
    by: dict[str, list] = {}
    for r in sorted(rs, key=lambda r: (r["runs"], r["diff"], -r["score"])):
        t = triage(r, r["body"])
        by.setdefault(t["cat"], []).append({"key": r["key"], "score": r["score"], "diff": r["diff"],
                                            "runs": r["runs"], "why": t.get("why"),
                                            "moved": t.get("moved"), "mech": t.get("mech")})
    if as_json:
        print(json.dumps(by, indent=1))
        return 0
    print(f"pool {len(rs)}: " + ", ".join(f"{k} {len(v)}" for k, v in sorted(by.items())))
    mech: dict[str, int] = {}
    for v in by.values():
        for e in v:
            for m in e.get("mech") or []:
                mech[m] = mech.get(m, 0) + 1
    if mech:
        print("mechanisms: " + ", ".join(f"{k} {n}" for k, n in sorted(mech.items())))
    for cat in ("match", "reloc-only", "assembler", "callee", "permute"):
        if cat not in by:
            continue
        print(f"\n== {cat} ({len(by[cat])})")
        for e in by[cat]:
            print(f"  {e['score']:.4f} {e['diff']:3}w r{e['runs']} {e['key']}  {e['why'] or ''}"
                  + (f"  moved={e['moved']}" if cat == "assembler" else ""))
    return 0


def _integrate(c: dict) -> dict:
    p = subprocess.run([sys.executable, "-B", str(HERE / "integrate.py"), "--unit", c["unit"], "--tu", c["tu"],
                        "--name", c["name"], "--body", str(c["body"])] + (["--skip-try"] if c.get("link") else []),
                       capture_output=True, text=True, cwd=ROOT)
    ok = '"matched": true' in p.stdout + p.stderr
    with locked():
        s = load_state()
        if not ok:
            s["failed"].setdefault(c["key"], {"attempts": 0})["triage_integrate"] = c["body"].stat().st_mtime
            save_state(s)
    log(f"PERMUTE {c['key']} triage={c['cat']} integrate ok={ok}")
    return {"key": c["key"], "ok": ok, "stage": f"triage-{c['cat']}-integrate",
            "error": "" if ok else (p.stdout + p.stderr)[-200:]}


def _sweep(c: dict, jobs: int) -> dict | None:
    """trick_sweep on the current best body if it was never swept (lease held)."""
    best_c = ROOT / "build/auto_decomp/bodies" / c["unit"] / c["tu"] / f"{c['name']}.best.c"
    if not best_c.is_file() or not best_c.with_name(f"{c['name']}.best.json").is_file():
        return None
    if trick_sweep.is_swept(c["key"], best_c):
        return None
    from concurrent.futures import ProcessPoolExecutor
    with ProcessPoolExecutor(max_workers=max(1, min(jobs, 3)), initializer=trick_sweep._init) as pp:
        r = trick_sweep.sweep_one(c, pp, 300)
    log(f"PERMUTE {c['key']} pre-sweep {r.get('start')} -> {r.get('end')} matched={r.get('matched')}")
    return r


def run_one(c: dict, minutes: float, jobs: int) -> dict:
    with claims.locked():
        if c["key"] in claims.active():
            return {"key": c["key"], "skipped": "claimed"}
        claims.take(c["key"], "permuter", int(minutes * 60) + 1800)
    try:
        if c.get("cat") in ("match", "reloc-only"):
            return _integrate(c)
        sw = _sweep(c, jobs)
        if sw and sw.get("matched"):
            return {"key": c["key"], "ok": True, "stage": "sweep", "best": sw.get("end")}
        p = subprocess.run([sys.executable, "-B", str(HERE / "permute_fn.py"), "--unit", c["unit"],
                            "--tu", c["tu"], "--name", c["name"], "--body", str(c["body"]),
                            "--minutes", str(minutes), "-j", str(jobs)]
                           + (["--profile", c["tag"]] if c.get("tag") == "branch_likely" else []),
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
    ap.add_argument("--min-score", type=float, default=0.85, help="minimum best score (ratio, 0-1)")
    ap.add_argument("--max-diff", type=int, default=25,
                    help="maximum differing instruction words (0 = no limit)")
    ap.add_argument("--minutes", type=float, default=5)
    ap.add_argument("--parallel", type=int, default=4)
    ap.add_argument("-j", type=int, default=3, help="permuter threads per job")
    ap.add_argument("--rounds", type=int, default=1)
    ap.add_argument("--once", action="store_true")
    ap.add_argument("--triage-report", action="store_true",
                    help="classify the near-miss pool (all rounds), print counts and lists, exit")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()
    if a.triage_report:
        return triage_report(a.min_score, a.rounds, a.max_diff or None, a.json)
    while True:
        batch = pick(a.min_score, a.rounds, a.parallel, a.max_diff or None)
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

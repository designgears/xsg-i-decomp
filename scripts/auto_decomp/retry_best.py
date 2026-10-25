#!/usr/bin/env python3
"""Retry saved near-miss bodies (bodies/<unit>/<tu>/<name>.best.c) unchanged.

A try report with "match_after_link" (false %lo relocation, see try_fn) or
"link_check" counts as a match and is integrated with --skip-try.

A body that plateaued may match once something else changed (a same-TU callee
became C, a prototype/type changed, a hoist). No AI involved: each target is
re-scored with try_fn.try_function (object-level, lock-free); a match is handed
to integrate.py (sequential, AUTO_RETRY=0 so integrate doesn't fan out), and
each new match recursively retries its own TU + callers (bounded depth).

  retry_best.py --after unit/tu/name     # same TU pending fns + asm callers of name
  retry_best.py --tu unit/tu
  retry_best.py --all [--min-score 0.9]
  [--dry-run] [--max-depth 3]
  retry_best.py --all --suspect-scan [--apply]   # fold15

--suspect-scan (fold15, o587 CheckGameSymbol / o590 MenuSystemMenuMain): for
each selected .best.c the semantic tells (try_fn.prev_suspect, cached in
.best.json) flag, re-score every other saved body of the function
(<name>.c, .vN.c, .vN.best.c, .best.prev.c, .perm_cand.c; not .m2c / .ghidra
/ .sweepbad) with the uninit check; the best-scoring one that compiles and is
NOT suspect is reported and, with --apply, replaces .best.c whatever its
score (the old one is kept as <name>.best.suspect.c). Leased keys skipped.
"""
from __future__ import annotations

import argparse
import json
import re
import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import callgraph  # noqa: E402
import claims  # noqa: E402
import try_fn  # noqa: E402
from state import ROOT, STATE_DIR, load_state, log  # noqa: E402

BODIES = STATE_DIR / "bodies"


def best_path(k: str) -> Path:
    u, t, n = k.split("/")
    return BODIES / u / t / f"{n}.best.c"


def best_score(k: str) -> float:
    try:
        return float(json.loads(best_path(k).with_suffix(".json").read_text()).get("score", -1))
    except (OSError, ValueError, TypeError):
        return -1.0


def pending() -> dict[str, dict]:
    """Live INCLUDE_ASM functions (src/ scan). src/ is the truth: a key that
    state.json lists as matched but is INCLUDE_ASM again (lost / reverted
    integration) is pending too (next.py reconcile requeues those)."""
    return dict(callgraph.build(ROOT))


def callers_of(fns: dict, k: str) -> list[str]:
    name = k.split("/")[2]
    out = []
    for c, v in fns.items():
        asm = callgraph._asm_path(ROOT, v["unit"], f"asm/{v['unit']}/nonmatchings/{v['tu']}", v["name"])
        try:
            if name in callgraph.JAL.findall(asm.read_text(errors="replace")):
                out.append(c)
        except OSError:
            pass
    return out


def select(fns: dict, a, after: str | None) -> list[str]:
    if after:
        u, t, _ = after.split("/")
        ks = [k for k, v in fns.items() if v["unit"] == u and v["tu"] == t]
        ks += [c for c in callers_of(fns, after) if c not in ks]
    elif a.tu:
        u, t = a.tu.split("/")
        ks = [k for k, v in fns.items() if v["unit"] == u and v["tu"] == t]
    else:
        ks = list(fns)
    from build_queue import KNOWN_ICE      # fold13: a match integrate can never build (cc1 ICE)
    ks = [k for k in ks if k != after and best_path(k).is_file() and k not in KNOWN_ICE]
    if a.min_score is not None:
        ks = [k for k in ks if best_score(k) >= a.min_score]
    return sorted(ks, key=lambda k: -best_score(k))


def run_integrate(k: str, best: Path, skip_try: bool = False) -> dict:
    u, t, n = k.split("/")
    body = best.with_name(f"{n}.retry.c")
    shutil.copyfile(best, body)
    env = dict(os.environ, AUTO_RETRY="0")
    p = subprocess.run([sys.executable, "-B", str(Path(__file__).with_name("integrate.py")),
                        "--unit", u, "--tu", t, "--name", n, "--body", str(body)]
                       + (["--skip-try"] if skip_try else []),
                       cwd=ROOT, env=env, capture_output=True, text=True)
    try:
        return json.loads(p.stdout)
    except ValueError:
        return {"ok": False, "error": (p.stdout + p.stderr)[-2000:]}


def process(fns: dict, keys: list[str], a, depth: int, seen: set, results: list) -> None:
    for k in keys:
        if k in seen:
            continue
        seen.add(k)
        if k in claims.active():
            results.append({"key": k, "skipped": "leased"})
            continue
        if k not in fns:
            results.append({"key": k, "skipped": "not pending"})
            continue
        u, t, n = k.split("/")
        best, prev = best_path(k), best_score(k)
        if re.search(r"\b(__asm__|asm)\b", best.read_text(errors="replace")):
            results.append({"key": k, "skipped": "inline asm (banned)"})
            continue
        try:
            rep = try_fn.try_function(u, t, n, best, diff_limit=0)
        except Exception as e:  # noqa: BLE001
            results.append({"key": k, "error": str(e)[:300]})
            log(f"RETRY {k} error={str(e)[:120]}")
            continue
        link = try_fn.is_link_match(rep)       # false %lo / absolute-vs-symbol only
        score, match = rep.get("score"), bool(rep.get("match")) or link
        r = {"key": k, "prev": prev, "score": score, "match": match}
        if link:
            r["match_after_link"] = True
            r["false_lo16"] = rep.get("false_lo16") or []
        if not match and score is not None and score > prev and not a.dry_run:
            try:                               # same text, new score: keep by / suspect / src_patch
                rec = json.loads(best.with_suffix(".json").read_text())
            except (OSError, ValueError):
                rec = {}
            rec["score"] = score
            for f in ("differing_words", "target_insns"):
                if rep.get(f) is not None:
                    rec[f] = rep[f]
            best.with_suffix(".json").write_text(json.dumps(rec) + "\n")
            r["improved"] = True
        if match and not a.dry_run:
            ir = run_integrate(k, best, skip_try=link)
            r["integrated"] = bool(ir.get("matched"))
            if not ir.get("matched"):
                r["integrate_error"] = str(ir.get("error") or ir.get("refused") or "not green")[:300]
        log(f"RETRY {k} match={match} score={score} prev={prev}"
            + (f" integrated={r.get('integrated')}" if match and not a.dry_run else ""))
        results.append(r)
        if r.get("integrated"):
            fns.pop(k, None)
            if depth < a.max_depth:
                fns.update({x: y for x, y in pending().items() if x not in fns})
                process(fns, select(fns, a, k), a, depth + 1, seen, results)


SCAN_SKIP = re.compile(r"^\.best\.c$|\.(m2c|ghidra|sweepbad|lit|retry|suspect)\.c$")


def suspect_scan(fns: dict, keys: list[str], apply: bool) -> list[dict]:
    out = []
    for k in keys:
        if k not in fns:
            continue
        if k in claims.active():
            out.append({"key": k, "skipped": "leased"})
            continue
        u, t, n = k.split("/")
        best = best_path(k)
        side = best.with_suffix(".json")
        try:
            psus, psem = try_fn._prev_checks(u, t, n, side)   # 2026-10-19: + semantics gate
            psus = psus + ["semantics: " + d for d in psem]
        except Exception as e:  # noqa: BLE001
            out.append({"key": k, "error": str(e)[:200]})
            continue
        if not psus:
            continue
        r = {"key": k, "score": best_score(k), "suspect": psus[:2], "replacement": None}
        cands = [p for p in best.parent.glob(f"{n}.*c")
                 if p.name.startswith(n + ".") and p.suffix == ".c" and not SCAN_SKIP.search(p.name[len(n):])]
        cands += [p for p in (best.with_name(f"{n}.best.prev.c"), best.with_name(f"{n}.c")) if p.is_file()]
        seen, top = set(), None
        bt = best.read_text(errors="replace")
        for c in cands:
            if c in seen or re.search(r"\b(__asm__|asm)\b", c.read_text(errors="replace")):
                continue
            seen.add(c)
            if c.read_text(errors="replace") == bt:
                continue
            try:
                rep = try_fn.try_function(u, t, n, c, diff_limit=100000, uninit=True)
            except Exception:  # noqa: BLE001
                continue
            if not rep.get("ok") or rep.get("score") is None or rep.get("suspect") or try_fn.sem_failures(rep):
                continue
            if top is None or rep["score"] > top[1]["score"]:
                top = (c, rep)
        if top:
            r["replacement"] = {"body": top[0].name, "score": top[1]["score"]}
            if apply:
                shutil.copyfile(best, best.with_name(f"{n}.best.suspect.c"))
                shutil.copyfile(top[0], best)
                side.write_text(json.dumps(try_fn.best_record(
                    top[1]["score"], top[1], [], by="suspect_scan", replaced=r["score"],
                    replaced_suspect=psus[:2], source=top[0].name)) + "\n")
                r["applied"] = True
                log(f"SUSPECT_SCAN {k} {r['score']} -> {top[1]['score']} ({top[0].name})")
        out.append(r)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--after", metavar="UNIT/TU/NAME")
    g.add_argument("--tu", metavar="UNIT/TU")
    g.add_argument("--all", action="store_true")
    ap.add_argument("--min-score", type=float)
    ap.add_argument("--max-depth", type=int, default=3)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--suspect-scan", action="store_true",
                    help="fold15: replace suspect .best.c bodies by the best clean saved body (report only "
                         "unless --apply)")
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()
    fns = pending()
    if a.suspect_scan:
        rs = suspect_scan(fns, select(fns, a, a.after), a.apply)
        flagged = [r for r in rs if "suspect" in r]
        print(json.dumps({"flagged": len(flagged),
                          "replaceable": sum(1 for r in flagged if r.get("replacement")),
                          "applied": sum(1 for r in flagged if r.get("applied")),
                          "results": rs}, indent=1))
        return 0
    results: list = []
    process(fns, select(fns, a, a.after), a, 0, set(), results)
    summary = {"tried": sum("score" in r for r in results),
               "matches": [r["key"] for r in results if r.get("match")],
               "integrated": [r["key"] for r in results if r.get("integrated")],
               "improved": [r["key"] for r in results if r.get("improved")],
               "results": results}
    print(json.dumps(summary, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

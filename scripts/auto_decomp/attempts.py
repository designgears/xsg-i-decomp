#!/usr/bin/env python3
"""Per-function attempt diary: what was already tried, so nobody repeats it.

One JSON line per attempt in build/auto_decomp/attempts/<unit>__<tu>__<name>.jsonl
(persistent; append-only, flock'd, safe for parallel workers). try_fn.py appends
one record per CLI run automatically (score, differing words, body hash, the
--note text, owner, leftover diff signature); seeded records come from
loopstate/ai_fixes.md and the memory file decomp-match-fixes.md.

    attempts.py show KEY [--all]       compact summary: best so far, distinct notes
                                       with their scores, leftover diff signature,
                                       imported history notes
    attempts.py log KEY --note TEXT [--score S] [--owner W]
                                       add a manual note (a hypothesis judged
                                       without a try_fn run, or a conclusion)
    attempts.py seed                   (re)import the history notes (idempotent)
    attempts.py list [--top N]         most-attempted keys

KEY: unit/tu/name, unit__tu__name, unit/name or a bare function name (looked up in
build/auto_decomp/fn_index.json).

Approach of gt4-decomp tools/attempts.py (knowledge/attempts.jsonl), adapted.
"""
from __future__ import annotations

import argparse
import fcntl
import hashlib
import json
import os
import re
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DIR = ROOT / "build/auto_decomp/attempts"
FN_INDEX = ROOT / "build/auto_decomp/fn_index.json"
AI_FIXES = ROOT / "build/auto_decomp/loopstate/ai_fixes.md"
MEMORY = Path.home() / ".claude/projects/-home-bobby-Projects-decomp/memory/decomp-match-fixes.md"
CLAIMS = ROOT / "build/auto_decomp/claims"


# ------------------------------------------------------------------ keys ----

_IDX: dict | None = None


def _index() -> dict:
    global _IDX
    if _IDX is None:
        try:
            _IDX = json.loads(FN_INDEX.read_text())
        except (OSError, ValueError):
            _IDX = {}
    return _IDX


def resolve(key: str) -> tuple[str, str, str]:
    """(unit, tu, name) of KEY; raises KeyError when it cannot be resolved."""
    k = key.strip().strip("`")
    if k.endswith(".jsonl"):
        k = Path(k).name[:-6]
    parts = k.split("__") if "__" in k and "/" not in k else k.split("/")
    if len(parts) == 3:
        return parts[0], parts[1], parts[2]
    idx = _index()
    if len(parts) == 2:
        e = idx.get(f"{parts[0]}/{parts[1]}")
        if e:
            return e["unit"], e["tu"], e["name"]
    if len(parts) == 1:
        hits = [e for e in idx.values() if e.get("name") == parts[0]]
        if len(hits) == 1:
            return hits[0]["unit"], hits[0]["tu"], hits[0]["name"]
        if len(hits) > 1:
            raise KeyError(f"{key}: ambiguous ({', '.join(h['unit'] + '/' + h['tu'] for h in hits[:6])})")
    raise KeyError(f"{key}: unknown function (use unit/tu/name)")


def path_of(unit: str, tu: str, name: str) -> Path:
    return DIR / f"{unit}__{tu}__{name}.jsonl"


def load(unit: str, tu: str, name: str) -> list[dict]:
    p = path_of(unit, tu, name)
    out = []
    try:
        for line in p.read_text(errors="replace").splitlines():
            line = line.strip()
            if line:
                try:
                    out.append(json.loads(line))
                except ValueError:
                    pass
    except OSError:
        pass
    return out


def append(unit: str, tu: str, name: str, rec: dict) -> None:
    DIR.mkdir(parents=True, exist_ok=True)
    line = json.dumps(rec, ensure_ascii=False, separators=(",", ":")) + "\n"
    with open(path_of(unit, tu, name), "a", encoding="utf-8") as f:
        fcntl.flock(f, fcntl.LOCK_EX)
        try:
            f.write(line)
        finally:
            fcntl.flock(f, fcntl.LOCK_UN)


# ------------------------------------------------------------- try hook ----

def _strip_off(s: str) -> str:
    s = s.strip()
    return s.split(":", 1)[1].strip() if re.match(r"^[0-9a-f]+:", s) else s


def diff_signature(rows: list[str] | None, limit: int = 6) -> str:
    """Compact leftover-diff signature from try_fn's side-by-side rows:
    `T>C` per differing row ('-T' target only, '+C' candidate only, '~X' moved)."""
    if not rows:
        return ""
    head = rows[0]
    w = len(head) - len("   CANDIDATE") if head.endswith("CANDIDATE") else None
    out = []
    for r in rows[1:]:
        if w is None or len(r) <= w + 1:
            continue
        mark = r[w + 1]
        if mark not in "|<>":
            continue
        lt, rt = _strip_off(r[:w]), _strip_off(r[w + 3:])
        out.append(f"{lt}>{rt}" if mark == "|" else (f"-{lt}" if mark == "<" else f"+{rt}"))
    # a text both removed and added = an instruction in another place: `~text`
    adds = {x[1:] for x in out if x.startswith("+")}
    dels = {x[1:] for x in out if x.startswith("-")}
    moved = adds & dels
    res, seen = [], set()
    for x in out:
        if x[:1] in "+-" and x[1:] in moved:
            if x[1:] not in seen:
                seen.add(x[1:])
                res.append("~" + x[1:])
            continue
        res.append(x)
    out = res
    n = len(out)
    s = "; ".join(out[:limit])
    return s + (f"; ...(+{n - limit})" if n > limit else "")


def _owner(arg: str | None, unit: str, tu: str, name: str) -> str:
    if arg:
        return arg
    for v in ("TRY_FN_OWNER", "DECOMP_OWNER", "WORKER_NAME"):
        if os.environ.get(v):
            return os.environ[v]
    try:
        c = json.loads((CLAIMS / f"{unit}__{tu}__{name}.json").read_text())
        if c.get("owner"):
            return str(c["owner"])
    except (OSError, ValueError):
        pass
    return "?"


def record_try(unit: str, tu: str, name: str, report: dict, body: Path, note: str | None = None,
               owner: str | None = None) -> dict:
    """Append one try_fn run (called from try_fn.main; never raises into it)."""
    try:
        bh = hashlib.sha1(Path(body).read_bytes()).hexdigest()[:10]
    except OSError:
        bh = ""
    rec = {"ts": time.strftime("%Y-%m-%dT%H:%M:%S"), "kind": "try",
           "owner": _owner(owner, unit, tu, name), "score": report.get("score"),
           "words": report.get("differing_words"), "body": bh}
    if note:
        rec["note"] = " ".join(str(note).split())[:300]
    if report.get("match"):
        rec["match"] = True
    elif report.get("match_after_link") or report.get("link_check") or report.get("match_pending_jtbl"):
        rec["link_match"] = True
    if not report.get("ok"):
        rec["stage"] = report.get("stage")
    sem = report.get("semantics")
    if isinstance(sem, dict) and not sem.get("ok", True):
        rec["sem_fail"] = "; ".join(str(x) for x in (sem.get("diffs") or []))[:160] or "semantics differ"
    if report.get("suspect"):
        rec["suspect"] = len(report["suspect"])
    sig = diff_signature(report.get("diff"))
    if sig:
        rec["sig"] = sig[:400]
    append(unit, tu, name, rec)
    return rec


# ----------------------------------------------------------------- show ----

def _fmt_score(s) -> str:
    return f"{s:.4f}" if isinstance(s, (int, float)) else "  -   "


def _norm_note(n: str) -> str:
    return re.sub(r"\W+", " ", n.lower()).strip()


def show(unit: str, tu: str, name: str, all_notes: bool = False) -> str:
    recs = load(unit, tu, name)
    key = f"{unit}/{tu}/{name}"
    if not recs:
        return f"{key}: no attempts recorded yet (pass --note '<what changed>' to every try_fn run)"
    tries = [r for r in recs if r.get("kind") == "try"]
    seeds = [r for r in recs if r.get("kind") == "seed"]
    manual = [r for r in recs if r.get("kind") == "note"]
    scored = [r for r in tries if isinstance(r.get("score"), (int, float)) and not r.get("sem_fail")]
    lines = []
    owners = sorted({r.get("owner", "?") for r in tries})
    head = f"{key}: {len(tries)} try_fn runs, {len({r.get('body') for r in tries})} distinct bodies"
    if owners:
        head += f", owners {','.join(owners[:6])}"
    lines.append(head)
    if any(r.get("match") for r in tries):
        lines.append("  MATCHED at least once: " + next(r["ts"] for r in tries if r.get("match")))
    if scored:
        best = max(scored, key=lambda r: (r["score"], -(r.get("words") or 0)))
        lines.append(f"  best {_fmt_score(best['score'])} ({best.get('words')} words) {best['ts']} "
                     f"{best.get('owner', '?')} body {best.get('body')}"
                     + (f" note: {best['note']}" if best.get("note") else ""))
        if best.get("sig"):
            lines.append(f"  leftover diff @best: {best['sig'][:300]}")
    bad = [r for r in tries if r.get("sem_fail")]
    if bad:
        lines.append(f"  {len(bad)} runs failed the semantics gate (not counted as best)")
    # distinct notes
    groups: dict[str, dict] = {}
    for r in tries + manual:
        n = r.get("note")
        if not n:
            continue
        g = groups.setdefault(_norm_note(n), {"note": n, "best": None, "n": 0, "words": None, "sem": 0})
        g["n"] += 1
        s = r.get("score")
        if r.get("sem_fail"):
            g["sem"] += 1
        elif isinstance(s, (int, float)) and (g["best"] is None or s > g["best"]):
            g["best"], g["words"] = s, r.get("words")
    if groups:
        lines.append(f"  tried ({len(groups)} distinct notes; score / words / runs):")
        gs = sorted(groups.values(), key=lambda g: -(g["best"] if g["best"] is not None else -1))
        lim = None if all_notes else 25
        for g in gs[:lim]:
            extra = f" x{g['n']}" if g["n"] > 1 else ""
            sem = " SEMFAIL" if g["sem"] else ""
            lines.append(f"    {_fmt_score(g['best'])} {str(g['words'] if g['words'] is not None else '-'):>4}"
                         f"{extra}{sem}  {g['note'][:160]}")
        if lim and len(gs) > lim:
            lines.append(f"    ... {len(gs) - lim} more (--all)")
    nn = [r for r in tries if not r.get("note")]
    if nn:
        sc = [r["score"] for r in nn if isinstance(r.get("score"), (int, float))]
        rng = f", scores {min(sc):.4f}-{max(sc):.4f}" if sc else ""
        lines.append(f"  {len(nn)} runs without --note{rng}")
    if tries and tries[-1].get("sig") and (not scored or tries[-1] is not max(scored, key=lambda r: r["score"])):
        lines.append(f"  leftover diff @latest ({_fmt_score(tries[-1].get('score'))}): {tries[-1]['sig'][:300]}")
    if seeds:
        lines.append(f"  history ({len(seeds)} imported notes):")
        for r in seeds[-(None if all_notes else 6):]:
            lines.append(f"    [{r.get('source', 'seed')}] {r.get('note', '')[:400 if all_notes else 260]}")
    return "\n".join(lines)


# ----------------------------------------------------------------- seed ----

KEY3 = re.compile(r"\b(main|ov\d\d)/([A-Za-z0-9_]+)/([A-Za-z_][A-Za-z0-9_]*)\b")


def _seed_ai_fixes() -> list[tuple[tuple, str, str]]:
    out = []
    try:
        text = AI_FIXES.read_text(errors="replace")
    except OSError:
        return out
    for ln in text.splitlines():
        m = re.match(r"^##\s+(V\d+)\s+([A-Za-z_]\w*)\b[:\s]*(.*)$", ln)
        if not m:
            continue
        tag, fn, rest = m.groups()
        try:
            k = resolve(fn)
        except KeyError:
            continue
        out.append((k, f"ai_fixes {tag}", " ".join(rest.split())[:600]))
    return out


def _seed_memory() -> list[tuple[tuple, str, str]]:
    out = []
    try:
        text = MEMORY.read_text(errors="replace")
    except OSError:
        return out
    idx = _index()
    for i, para in enumerate(re.split(r"\n\s*\n|\n(?=\d+\.\s|\*\*|- |## )", text)):
        para = " ".join(para.split())
        if not para or para.startswith("---"):
            continue
        seen = set()
        for m in KEY3.finditer(para):
            k = m.groups()
            if k in seen:
                continue
            seen.add(k)
            # only real functions (avoid file paths like main/foo/bar.c)
            if (idx.get(f"{k[0]}/{k[2]}") or {}).get("tu") != k[1]:
                srcf = ROOT / "src" / k[0] / f"{k[1]}.c"
                try:
                    if k[2] not in srcf.read_text(errors="replace"):
                        continue
                except OSError:
                    continue
            # a window around the mention keeps long multi-function paragraphs short
            s = max(0, m.start() - 120)
            out.append((k, "memory decomp-match-fixes", ("..." if s else "") + para[s:s + 600]))
    return out


def seed() -> int:
    items = _seed_ai_fixes() + _seed_memory()
    by: dict[tuple, list] = {}
    for k, src, note in items:
        by.setdefault(k, []).append((src, note))
    DIR.mkdir(parents=True, exist_ok=True)
    n = 0
    # drop earlier seed records everywhere, keep everything else
    for p in DIR.glob("*.jsonl"):
        k = tuple(p.name[:-6].split("__"))
        recs = [r for r in load(*k) if r.get("kind") != "seed"] if len(k) == 3 else []
        if len(k) == 3 and k not in by:
            _rewrite(p, recs)
    for k, lst in by.items():
        p = path_of(*k)
        recs = [r for r in load(*k) if r.get("kind") != "seed"]
        seeds, seen = [], set()
        for src, note in lst:
            if note in seen:
                continue
            seen.add(note)
            seeds.append({"ts": "seed", "kind": "seed", "source": src, "note": note})
        _rewrite(p, seeds + recs)
        n += len(seeds)
    print(f"seeded {n} notes for {len(by)} functions into {DIR}")
    return 0


def _rewrite(p: Path, recs: list[dict]) -> None:
    with open(p, "a+", encoding="utf-8") as f:
        fcntl.flock(f, fcntl.LOCK_EX)
        try:
            f.seek(0)
            cur = [json.loads(x) for x in f.read().splitlines() if x.strip()]
            # keep records appended since load() (a concurrent try_fn run)
            extra = [r for r in cur if r.get("kind") != "seed" and r not in recs]
            f.seek(0)
            f.truncate()
            for r in recs + extra:
                f.write(json.dumps(r, ensure_ascii=False, separators=(",", ":")) + "\n")
        finally:
            fcntl.flock(f, fcntl.LOCK_UN)


# ----------------------------------------------------------------- main ----

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("show")
    s.add_argument("key")
    s.add_argument("--all", action="store_true")
    s = sub.add_parser("log")
    s.add_argument("key")
    s.add_argument("--note", required=True)
    s.add_argument("--score", type=float)
    s.add_argument("--owner")
    sub.add_parser("seed")
    s = sub.add_parser("list")
    s.add_argument("--top", type=int, default=30)
    a = ap.parse_args()
    if a.cmd == "seed":
        return seed()
    if a.cmd == "list":
        rows = []
        for p in DIR.glob("*.jsonl"):
            k = p.name[:-6].split("__")
            if len(k) != 3:
                continue
            recs = load(*k)
            t = [r for r in recs if r.get("kind") == "try"]
            sc = [r["score"] for r in t if isinstance(r.get("score"), (int, float))]
            rows.append((len(t), max(sc) if sc else None, len(recs) - len(t), "/".join(k)))
        rows.sort(key=lambda r: (-r[0], -(r[1] or 0)))
        for n, b, sn, k in rows[:a.top]:
            print(f"{n:4d} runs  best {_fmt_score(b)}  {sn:3d} notes  {k}")
        return 0
    try:
        k = resolve(a.key)
    except KeyError as e:
        print(str(e), file=sys.stderr)
        return 2
    if a.cmd == "show":
        print(show(*k, all_notes=a.all))
        return 0
    rec = {"ts": time.strftime("%Y-%m-%dT%H:%M:%S"), "kind": "note",
           "owner": _owner(a.owner, *k), "note": " ".join(a.note.split())[:300]}
    if a.score is not None:
        rec["score"] = a.score
    append(*k, rec)
    print(f"logged for {'/'.join(k)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

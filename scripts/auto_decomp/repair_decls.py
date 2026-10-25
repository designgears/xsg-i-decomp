#!/usr/bin/env python3
"""Repair saved near-miss bodies that stopped compiling because the TU's
declarations changed under them (2026-10-18: TitleHddInstall, RES_loadFileSub,
sefCreateReactionEffect all failed with `conflicting types for X` after other
functions in their TU were integrated with different prototypes).

A body that does not compile is invisible to trick_sweep and the permuter.
For every pending INCLUDE_ASM function with a bodies/<u>/<t>/<n>.best.c this
scores the body with try_fn; when the candidate compile fails with
decl_collisions (try_fn.decl_collisions), each colliding file-scope function
prototype in the body is tried, in order:
  1. dropped (the TU's own declaration is used),
  2. made unprototyped with the body's return type (`extern int X();`, which
     coexists with any later prototype and passes pointer/int args the same).
The first combination that compiles is written back as .best.c (the old file
is kept as .best.predecl.c) with a .best.json carrying the new score.

    python3 -B scripts/auto_decomp/repair_decls.py [--key U/T/N] [--dry-run]
"""
from __future__ import annotations

import argparse
import itertools
import json
import re
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import try_fn  # noqa: E402

ROOT = HERE.parent.parent
BODIES = ROOT / "build" / "auto_decomp" / "bodies"


def pending_keys() -> list[str]:
    out = []
    for best in sorted(BODIES.glob("*/*/*.best.c")):
        name = best.name[: -len(".best.c")]
        if "." in name:  # variant files like X.v3.best.c
            continue
        tu, unit = best.parent.name, best.parent.parent.name
        src = ROOT / "src" / unit / f"{tu}.c"
        try:
            text = src.read_text(errors="replace")
        except OSError:
            continue
        if re.search(r"INCLUDE_ASM\([^)]*[\", ]" + re.escape(name) + r"\)", text):
            out.append(f"{unit}/{tu}/{name}")
    return out


def proto_lines(text: str, sym: str, fn_name: str) -> list[int]:
    """Indices of single-line file-scope prototypes / extern data declarations
    of SYM (not the function itself)."""
    out = []
    pat = re.compile(r"^\s*(?:extern\s+)?[A-Za-z_][\w\s\*]*?\b" + re.escape(sym) + r"\s*\([^;{}]*\)\s*;\s*$")
    dpat = re.compile(r"^\s*extern\s+[^;(){}=]*\b" + re.escape(sym) + r"\b\s*(?:\[[^\]]*\])*\s*;\s*$")
    for i, ln in enumerate(text.split("\n")):
        if sym != fn_name and (pat.match(ln) or dpat.match(ln)):
            out.append(i)
    return out


def self_fix(text: str, name: str, tu_decl: str) -> str | None:
    """The body's own definition clashes with the TU's declaration of NAME:
    align `static` with the TU (the usual drift: a forward `static` prototype
    added by another integration)."""
    lines = text.split("\n")
    hd = next((i for i, ln in enumerate(lines) if re.search(r"\b" + re.escape(name) + r"\s*\(", ln)
               and not ln.rstrip().endswith(";")), None)
    if hd is None:
        return None
    tu_static = bool(re.match(r"\s*static\b", tu_decl))
    ln = lines[hd]
    has_static = bool(re.match(r"\s*static\b", ln))
    if tu_static and not has_static:
        lines[hd] = "static " + ln.lstrip()
    elif has_static and not tu_static:
        lines[hd] = re.sub(r"^\s*static\s+", "", ln)
    else:
        return None
    return "\n".join(lines)


def unproto(line: str, sym: str) -> str:
    head = line[: line.index(sym)].strip()
    head = re.sub(r"^extern\s+", "", head)
    return f"extern {head} {sym}();".replace("  ", " ")


def data_view(line: str, sym: str) -> str | None:
    """`extern T SYM[...];` -> `((T *)&SYM)`; `extern T SYM;` -> `(*(T *)&SYM)`:
    keep the body's view of a data symbol whose TU type changed."""
    m = re.match(r"^\s*extern\s+(.*?)\b" + re.escape(sym) + r"\b\s*((?:\[[^\]]*\])*)\s*;", line)
    if not m:
        return None
    t = m.group(1).strip()
    return f"(({t} *)&{sym})" if m.group(2) else f"(*({t} *)&{sym})"


def score(unit: str, tu: str, name: str, text: str) -> dict:
    with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False, dir="/tmp") as f:
        f.write(text)
        p = Path(f.name)
    try:
        return try_fn.try_function(unit, tu, name, p, diff_limit=0)
    finally:
        p.unlink(missing_ok=True)


def repair(key: str, dry: bool) -> dict | None:
    unit, tu, name = key.split("/")
    best = BODIES / unit / tu / f"{name}.best.c"
    text = best.read_text(errors="replace")
    orig = text
    rep = score(unit, tu, name, text)
    if rep.get("ok") or not rep.get("decl_collisions"):
        return None
    syms = list(dict.fromkeys(c["name"] for c in rep["decl_collisions"]))
    if name in syms:
        tu_decl = next(c["tu_decl"] for c in rep["decl_collisions"] if c["name"] == name)
        fixed = self_fix(text, name, tu_decl)
        if fixed is not None:
            r = score(unit, tu, name, fixed)
            if r.get("ok"):
                text_new = fixed
                if not dry:
                    (best.with_name(f"{name}.best.predecl.c")).write_text(orig)
                    best.write_text(text_new)
                    best.with_name(f"{name}.best.json").write_text(json.dumps(
                        {"score": r.get("score"), "differing_words": r.get("differing_words"),
                         "target_insns": r.get("target_insns"), "by": "repair_decls"}))
                return {"key": key, "fixed": True, "how": {name: "static-align"},
                        "score": r.get("score"), "match": r.get("match")}
            if r.get("decl_collisions"):
                text, rep = fixed, r
                syms = list(dict.fromkeys(c["name"] for c in rep["decl_collisions"]))
    sites = {s: proto_lines(text, s, name) for s in syms}
    sites = {s: v for s, v in sites.items() if v}
    if not sites:
        return {"key": key, "fixed": False, "why": f"collisions {syms} are not single-line prototypes"}
    lines = text.split("\n")
    tudecl = {c["name"]: c.get("tu_decl", "") for c in rep["decl_collisions"]}
    choices = []
    for s, idxs in sites.items():
        is_data = "(" not in lines[idxs[0]]
        choices.append([(s, idxs, "drop"), (s, idxs, "cast"), (s, idxs, "tucast")] if is_data
                       else [(s, idxs, "drop"), (s, idxs, "unproto")])
    for combo in itertools.product(*choices):
        nl = list(lines)
        casts = {}
        for s, idxs, how in combo:
            for i in idxs:
                if how in ("cast", "tucast"):
                    casts[s] = data_view(lines[i], s)
                if how == "unproto":
                    nl[i] = unproto(lines[i], s)
                elif how == "tucast" and tudecl.get(s, "").endswith(";") and "(" not in tudecl.get(s, ""):
                    td = tudecl[s]
                    nl[i] = td if td.startswith("extern") else "extern " + re.sub(r"\s*=.*;$", ";", td)
                else:
                    nl[i] = ""
        cand = "\n".join(nl)
        for s, view in casts.items():
            if view:
                hd = cand.find("\n" + name + "(") if ("\n" + name + "(") in cand else 0
                pre, post = cand[:hd], cand[hd:]
                cand = pre + re.sub(r"(?<![\w.>])" + re.escape(s) + r"\b", view, post)
        r = score(unit, tu, name, cand)
        if r.get("ok"):
            res = {"key": key, "fixed": True, "how": {s: how for s, _, how in combo},
                   "score": r.get("score"), "match": r.get("match"),
                   "differing_words": r.get("differing_words")}
            if not dry:
                (best.with_name(f"{name}.best.predecl.c")).write_text(orig)
                best.write_text(cand)
                best.with_name(f"{name}.best.json").write_text(json.dumps(
                    {"score": r.get("score"), "differing_words": r.get("differing_words"),
                     "target_insns": r.get("target_insns"), "by": "repair_decls"}))
            return res
    return {"key": key, "fixed": False, "why": "no drop/unproto combination compiles", "syms": syms}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--key")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    keys = [a.key] if a.key else pending_keys()
    results = []
    for k in keys:
        try:
            r = repair(k, a.dry_run)
        except Exception as e:  # noqa: BLE001
            r = {"key": k, "fixed": False, "why": f"error: {e}"[:300]}
        if r:
            print(json.dumps(r), flush=True)
            results.append(r)
    print(json.dumps({"checked": len(keys), "broken": len(results),
                      "fixed": sum(1 for r in results if r.get("fixed"))}))
    return 0


if __name__ == "__main__":
    sys.exit(main())

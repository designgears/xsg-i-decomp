#!/usr/bin/env python3
"""Match pending INCLUDE_ASM functions by cloning the C of matched twins (no AI).

    python3 -B scripts/auto_decomp/clone_twins.py [--min-sim 0.9] [--dry-run] [--key U/T/N] [--limit N]

For each pending, unleased function: find already-matched twins in fn_index.json
(instruction-identical shape first, then mnemonic similarity >= --min-sim, any
TU/unit), take the twin's C definition, rename it, map the symbols the two
functions reference differently (aligned %hi/%lo/%gp_rel/jal operands of the two
splat .s files), and try it in tiers:

  bare       the renamed/mapped definition alone
  decls      + the twin TU's declarations the body needs (closure; data
               definitions become externs), placed by tu_edit.to_c only when the
               target TU lacks them
  includes   + the twin TU's #include lines

A candidate is written to build/auto_decomp/bodies/<u>/<t>/<n>.twin.c and tested
with try_fn.try_function. A match (or link_check) is copied to <n>.c and
integrated via integrate.py; a non-match better than <n>.best.json is saved as
<n>.best.c/.best.json. Each function is leased (owner clone_twins) while worked.
"""
from __future__ import annotations

import argparse
import difflib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "tools"))
import claims  # noqa: E402
import try_fn  # noqa: E402
import tu_edit  # noqa: E402
from build_queue import build_queue  # noqa: E402
from state import key, log  # noqa: E402

BODIES = ROOT / "build/auto_decomp/bodies"
OWNER = "clone_twins"
INSN = re.compile(r"^\s*/\*\s*[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s*\*/\s+(\S+)\s*(.*)$")
RELOC = re.compile(r"%(?:hi|lo|gp_rel)\(\s*([A-Za-z_.$][\w.$]*)\s*(?:\+\s*(0x[0-9A-Fa-f]+|\d+))?\s*\)")
JUMP = {"jal", "j"}
IDENT = re.compile(r"\b[A-Za-z_]\w*\b")
GLABEL = re.compile(r"^\s*glabel\s+([A-Za-z_]\w*)", re.M)


# ---------------------------------------------------------------- asm / symbols

_asm_paths: dict[str, dict[str, Path]] = {}


def asm_path(unit: str, tu: str, name: str) -> Path | None:
    if unit not in _asm_paths:
        m: dict[str, Path] = {}
        base = ROOT / "build" / unit / "asm"
        for dp, _dn, fns in os.walk(base):
            if "/try/" in dp:
                continue
            for f in fns:
                if f.endswith(".s"):
                    p = Path(dp) / f
                    m[f"{p.parent.name}/{p.stem}"] = p
        _asm_paths[unit] = m
    return _asm_paths[unit].get(f"{tu}/{name}")


def asm_insns(path: Path) -> list[tuple[str, list[tuple[str, int]]]]:
    """(mnemonic, [(symbol, addend)]) per instruction."""
    out = []
    for line in path.read_text(errors="replace").splitlines():
        m = INSN.match(line)
        if not m:
            continue
        mn, ops = m.group(1), m.group(2)
        syms = [(s, int(a, 0) if a else 0) for s, a in RELOC.findall(ops)]
        if mn in JUMP:
            t = ops.strip().split(",")[-1].strip()
            if re.fullmatch(r"[A-Za-z_]\w*", t) and not t.startswith(".L"):
                syms.append((t, 0))
        out.append((mn, syms))
    return out


def symbol_map(twin_s: Path, mine_s: Path) -> tuple[dict[str, str], list[str]]:
    a, b = asm_insns(twin_s), asm_insns(mine_s)
    sm = difflib.SequenceMatcher(None, [x[0] for x in a], [x[0] for x in b], autojunk=False)
    pairs: dict[str, set] = {}
    notes = []
    for blk in sm.get_matching_blocks():
        for i in range(blk.size):
            sa, sb = a[blk.a + i][1], b[blk.b + i][1]
            for (na, oa), (nb, ob) in zip(sa, sb):
                if na.startswith("jtbl_") or nb.startswith("jtbl_"):
                    continue
                if na == nb and oa == ob:
                    pairs.setdefault(na, set()).add((nb, 0))
                    continue
                pairs.setdefault(na, set()).add((nb, ob - oa))
    mapping = {}
    for na, targets in pairs.items():
        if len(targets) != 1:
            notes.append(f"conflict {na}->{sorted(targets)}")
            continue
        (nb, delta), = targets
        if delta:
            notes.append(f"addend {na}->{nb}{delta:+#x}")
            continue
        if nb != na:
            mapping[na] = nb
    return mapping, notes


def glabel_name(path: Path | None, fallback: str) -> str:
    if path:
        m = GLABEL.search(path.read_text(errors="replace"))
        if m:
            return m.group(1)
    return tu_edit.name_stem(fallback)


def subst(text: str, mapping: dict[str, str]) -> str:
    if not mapping:
        return text
    rx = re.compile(r"\b(" + "|".join(map(re.escape, sorted(mapping, key=len, reverse=True))) + r")\b")
    return rx.sub(lambda m: mapping[m.group(1)], text)


# ---------------------------------------------------------------- declarations

def _as_extern(text: str) -> str | None:
    """A data definition as an extern declaration (None when not a plain object)."""
    lex = tu_edit.lexical(text).strip()
    if lex.startswith(("typedef", "extern")) or "(" in lex.split("=")[0] and "(*" not in lex:
        return text
    if "{" in lex.split("=")[0]:
        return None
    t = re.sub(r"^\s*static\s+", "", text.strip())
    depth = 0
    for i, c in enumerate(t):
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "=" and depth == 0:
            t = t[:i].rstrip() + ";"
            break
    return "extern " + t if not t.startswith("extern") else t


def needed_decls(twin_text: str, twin_name: str, body: str, target_text: str,
                 mapping: dict[str, str], with_includes: bool) -> list[str]:
    items = tu_edit.scan(twin_text)
    have = tu_edit.declared_names(target_text) if target_text else set()
    cands = []
    for it in items:
        txt = it.text(twin_text)
        if it.kind == "pp":
            if re.match(r"#\s*include\b", txt):
                if with_includes:
                    cands.append(({("include", txt)}, txt, True))
                continue
            if not re.match(r"#\s*define\b", txt):
                continue
        elif it.kind == "func":
            if it.name == twin_name or not tu_edit._is_inline_def(twin_text, it):
                continue
        elif it.kind != "decl":
            continue
        txt = subst(txt, mapping)
        keys = tu_edit.decl_keys(txt) if it.kind != "func" else {("ordinary", subst(it.name, mapping))}
        cands.append((keys, txt, False))
    used = set(IDENT.findall(tu_edit.lexical(body)))
    chosen: list[int] = []
    changed = True
    while changed:
        changed = False
        for i, (keys, txt, forced) in enumerate(cands):
            if i in chosen:
                continue
            names = {n for _k, n in keys}
            if forced or names & used:
                if not forced and names and names <= have:
                    continue
                chosen.append(i)
                used |= set(IDENT.findall(tu_edit.lexical(txt)))
                changed = True
    out = []
    for i in sorted(chosen):
        keys, txt, _ = cands[i]
        if not txt.lstrip().startswith("#") and ("include", txt) not in keys:
            txt = _as_extern(txt)
            if txt is None:
                continue
        if txt.strip() not in out:
            out.append(txt.strip())
    return out


# ---------------------------------------------------------------- twins

def load_index() -> dict:
    subprocess.run([sys.executable, "-B", str(HERE / "fn_index.py")], cwd=ROOT,
                   capture_output=True, timeout=600)
    return json.loads((ROOT / "build/auto_decomp/fn_index.json").read_text())


def find_twins(idx: dict, c: dict, pending: set, min_sim: float, top: int = 3) -> list[dict]:
    me = idx.get(f"{c['unit']}/{c['name']}")
    if not me:
        return []
    mine = me["mn"].split()
    n = len(mine)
    exact, fuzzy = [], []
    sm = difflib.SequenceMatcher(None, autojunk=False)
    sm.set_seq2(mine)
    for v in idx.values():
        if v is me or not v["c"] or key(v["unit"], v["tu"], v["name"]) in pending:
            continue
        if v["shape"] == me["shape"]:
            exact.append({**v, "similarity": 1.0, "exact": True})
            continue
        if n < 4 or abs(v["mn"].count(" ") + 1 - n) > max(4, n // 4):
            continue
        sm.set_seq1(v["mn"].split())
        if sm.real_quick_ratio() < min_sim or sm.quick_ratio() < min_sim:
            continue
        r = sm.ratio()
        if r >= min_sim:
            fuzzy.append({**v, "similarity": round(r, 4), "exact": False})
    same = lambda v: (v["unit"], v["tu"]) == (c["unit"], c["tu"])  # noqa: E731
    exact.sort(key=lambda v: (not same(v), v["unit"] != c["unit"]))
    fuzzy.sort(key=lambda v: (-v["similarity"], not same(v)))
    return (exact + fuzzy)[:top]


# ---------------------------------------------------------------- per function

def best_score(stem: Path) -> float:
    try:
        return json.loads(stem.with_name(stem.name + ".best.json").read_text()).get("score", -1)
    except (OSError, ValueError):
        return -1


def attempt(c: dict, twins: list[dict], dry: bool) -> dict:
    u, t, n = c["unit"], c["tu"], c["name"]
    k = key(u, t, n)
    mine_s = ROOT / c["asm"]
    target_src = ROOT / "src" / u / f"{t}.c"
    target_text = target_src.read_text(errors="replace")
    my_cname = glabel_name(mine_s if mine_s.is_file() else None, n)
    bdir = BODIES / u / t
    stem = bdir / n
    res = {"key": k, "twins": [], "matched": False}
    best = None
    for tw in twins:
        tsrc = ROOT / "src" / tw["unit"] / f"{tw['tu']}.c"
        info = {"twin": f"{tw['unit']}/{tw['tu']}/{tw['name']}", "sim": tw["similarity"]}
        res["twins"].append(info)
        try:
            ttext = tsrc.read_text(errors="replace")
            definition = tu_edit.extract_function(ttext, tw["name"])
        except Exception as e:  # noqa: BLE001
            info["fail"] = f"extract: {str(e)[:120]}"
            continue
        if re.search(r"\b(?:__asm__|asm)\b", definition):
            info["fail"] = "twin_has_asm"
            continue
        tw_s = asm_path(tw["unit"], tw["tu"], tw["name"])
        mapping, notes = ({}, ["no_twin_asm"]) if tw_s is None or not mine_s.is_file() \
            else symbol_map(tw_s, mine_s)
        twin_c = tw["name"]
        mapping = {a: b for a, b in mapping.items() if a != twin_c}
        mapping[twin_c] = my_cname
        info["map"] = {a: b for a, b in mapping.items() if a != twin_c}
        if notes:
            info["notes"] = notes[:6]
        body = subst(definition, mapping)
        tiers = [("bare", [])]
        try:
            tiers.append(("decls", needed_decls(ttext, twin_c, body, target_text, mapping, False)))
            tiers.append(("includes", needed_decls(ttext, twin_c, body, target_text, mapping, True)))
        except Exception as e:  # noqa: BLE001
            info["decl_error"] = str(e)[:120]
        seen = set()
        for tier, decls in tiers:
            cand = ("\n".join(decls) + "\n\n" if decls else "") + body.strip() + "\n"
            if cand in seen:
                continue
            seen.add(cand)
            if re.search(r"\b(?:__asm__|__asm|asm)\b", tu_edit.lexical(cand)):
                info.setdefault("skipped_tiers", []).append(f"{tier}:inline_asm")
                continue                      # hard rule: never test/save/integrate inline asm
            if dry:
                info.setdefault("tiers", []).append(tier)
                continue
            bdir.mkdir(parents=True, exist_ok=True)
            twin_path = stem.with_name(n + ".twin.c")
            twin_path.write_text(cand)
            try:
                r = try_fn.try_function(u, t, n, twin_path, diff_limit=0)
            except try_fn.TryError as e:
                r = {"ok": False, "match": False, "stage": "setup", "error": str(e)}
            r.pop("diff", None)
            row = {"tier": tier, "ok": r.get("ok"), "match": r.get("match"),
                   "link_check": r.get("link_check"), "score": r.get("score"),
                   "differing_words": r.get("differing_words"), "stage": r.get("stage")}
            if not r.get("ok"):
                row["error"] = (r.get("error") or "")[-300:]
            info.setdefault("tries", []).append(row)
            if r.get("ok") and (best is None or (r.get("score") or 0) > best[0].get("score", 0)):
                best = (r, cand)
            if r.get("match") or r.get("link_check"):
                res["candidate_match"] = True
                return finish(res, c, cand, r, stem)
            if not r.get("ok") and r.get("stage") == "compile_cand":
                continue                      # next tier may add the missing declarations
            if r.get("ok"):
                break                         # compiled: more decls will not change code
    if best:
        r, cand = best
        res["best_score"] = r.get("score")
        side = stem.with_name(n + ".best.json")
        better, sus = False, []
        if (r.get("score") or 0) > best_score(stem) - try_fn.SUSPECT_MARGIN:
            import tempfile
            fd, tmp = tempfile.mkstemp(prefix=f"clone_{n}_", suffix=".c")
            os.close(fd)
            try:                       # fold11: suspect-aware (SUSPECT_MARGIN)
                Path(tmp).write_text(cand)
                better, sus = try_fn.prefer_over_best(u, t, n, Path(tmp), r.get("score"), r, side)
            except (try_fn.TryError, OSError):
                better = False
            finally:
                os.unlink(tmp)
        if better:
            stem.with_name(n + ".best.c").write_text(cand)
            side.write_text(json.dumps(try_fn.best_record(r.get("score"), r, sus, by=OWNER)) + "\n")
            res["improved"] = True
    return res


def finish(res: dict, c: dict, cand: str, r: dict, stem: Path) -> dict:
    u, t, n = c["unit"], c["tu"], c["name"]
    body = stem.with_name(n + ".c")
    body.write_text(cand)
    p = subprocess.run([sys.executable, "-B", str(HERE / "integrate.py"), "--unit", u, "--tu", t,
                        "--name", n, "--body", str(body)], cwd=ROOT, capture_output=True, text=True)
    try:
        rep = json.loads(p.stdout)
    except ValueError:
        rep = {"ok": False, "error": (p.stdout + p.stderr)[-500:]}
    res["integrate"] = {x: rep.get(x) for x in ("ok", "matched", "error", "refused", "reverted")}
    res["matched"] = bool(rep.get("matched"))
    if not res["matched"]:
        res["integrate_try"] = rep.get("try")
    return res


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--min-sim", type=float, default=0.9)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--key", help="only this unit/tu/name")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--json-out", type=Path)
    a = ap.parse_args()

    idx = load_index()
    q = build_queue(ROOT)
    cands = q["candidates"]
    pending = {key(c["unit"], c["tu"], c["name"]) for c in cands}
    if a.key:
        cands = [c for c in cands if key(c["unit"], c["tu"], c["name"]) == a.key]
    summary = {"pending": len(cands), "with_twins": 0, "matched": [], "improved": [],
               "failures": {}, "results": []}
    done = 0
    for c in cands:
        k = key(c["unit"], c["tu"], c["name"])
        twins = find_twins(idx, c, pending, a.min_sim)
        if not twins:
            continue
        summary["with_twins"] += 1
        if a.limit and done >= a.limit:
            break
        done += 1
        if not a.dry_run:
            with claims.locked():
                if k in claims.active():
                    summary["failures"].setdefault("leased", []).append(k)
                    continue
                claims.take(k, OWNER, ttl_s=30 * 60)
        try:
            res = attempt(c, twins, a.dry_run)
        except Exception as e:  # noqa: BLE001
            res = {"key": k, "matched": False, "error": f"{type(e).__name__}: {e}"[:300]}
        finally:
            if not a.dry_run:
                claims.release(k)
        summary["results"].append(res)
        if res.get("matched"):
            summary["matched"].append(k)
            reason = "matched"
        elif res.get("candidate_match"):
            reason = "integrate_refused"
        elif "error" in res:
            reason = "exception"
        elif a.dry_run:
            reason = "dry_run"
        else:
            tries = [x for tw in res["twins"] for x in tw.get("tries", [])]
            fails = [tw.get("fail") for tw in res["twins"] if tw.get("fail")]
            if any(x.get("ok") for x in tries):
                reason = "code_differs"
            elif tries:
                reason = "compile_error"
            else:
                reason = (fails[0].split(":")[0] if fails else "no_try")
        if res.get("improved"):
            summary["improved"].append(k)
        if reason != "matched":
            summary["failures"].setdefault(reason, []).append(k)
        if not a.dry_run:
            log(f"CLONE {k} {reason} twins={[tw['twin'] for tw in res.get('twins', [])]} "
                f"best={res.get('best_score')}")
        print(json.dumps({"key": k, "reason": reason, "matched": res.get("matched"),
                          "best": res.get("best_score"),
                          "twins": [tw["twin"] + f"@{tw['sim']}" for tw in res.get("twins", [])]}),
              flush=True)
    out = {"pending": summary["pending"], "with_twins": summary["with_twins"],
           "matched": summary["matched"], "improved": summary["improved"],
           "failures": {r: len(v) for r, v in summary["failures"].items()}}
    if a.json_out:
        a.json_out.parent.mkdir(parents=True, exist_ok=True)
        a.json_out.write_text(json.dumps(summary, indent=1) + "\n")
    print(json.dumps(out, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

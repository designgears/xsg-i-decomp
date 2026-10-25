#!/usr/bin/env python3
"""Re-check saved 1.0 bodies whose function is still INCLUDE_ASM, and integrate them.

    python3 -B scripts/auto_decomp/revalidate_best.py [--dry-run] [--key u/t/n]
    python3 -B scripts/auto_decomp/revalidate_best.py --gate-scan [--min-score 0.85]

For every bodies/<u>/<t>/<n>.best.json with score 1.0 whose function is still
`INCLUDE_ASM(..., n)` in src/<u>/<t>.c (bodies with inline asm skipped, leased
keys skipped): try_fn on a copy of .best.c. A match (or link_check /
match_after_link) is copied to <n>.c and handed to integrate.py.

Headers move on after a body was saved (o-series: `sys->nodeX` became
`sys->node.x`), so a body that no longer compiles is ported automatically:
  (a) "structure has no member named `X'"  -> RENAMES below (grow it), then a
      generic trailing-axis split `->fooX` -> `->foo.x`;
  (b) "`T' undeclared" for a type used only in casts -> its typedef/struct
      definition, found elsewhere in include/ or src/, is emitted at the top of
      the body (cast views stay casts: switching a cast to the real field
      changed scheduling in FileLoadTest). No definition found -> skipped.

--gate-scan: run try_fn.semantic_gate on every pending .best.c (score in
[--min-score, 1)) and write "suspect_semantics": true (+ "gate") into its
.best.json when it fails (removed when it passes).
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import claims  # noqa: E402
import try_fn  # noqa: E402

BODIES = ROOT / "build/auto_decomp/bodies"

# member renames after header refactors: regex on `->member` / `.member` -> replacement
RENAMES = {
    r"->nodeX\b": "->node.x", r"->nodeY\b": "->node.y", r"->nodeZ\b": "->node.z",
    r"->centerX\b": "->center.x", r"->centerY\b": "->center.y", r"->centerZ\b": "->center.z",
    r"\bPadData\.button_state\b": "PadData.buttons.button_state",
}
ASM_RE = re.compile(r"__asm__|\basm\s*\(|asm\s+volatile")
NO_MEMBER = re.compile(r"has no member named [`'\"‘]?(\w+)")
UNDECL = re.compile(r"[`'\"‘](\w+)['\"’] undeclared")


def pending(unit: str, tu: str, name: str) -> bool:
    src = ROOT / "src" / unit / f"{tu}.c"
    return src.is_file() and re.search(r"INCLUDE_ASM\([^;]*?,\s*" + re.escape(name) + r"\s*\)",
                                       src.read_text(errors="replace")) is not None


def candidates(only: str | None = None, min_score: float = 1.0, below_one: bool = False):
    for j in sorted(BODIES.glob("*/*/*.best.json")):
        name = j.name[:-len(".best.json")]
        if "." in name:
            continue
        unit, tu = j.parts[-3], j.parts[-2]
        if only and only != f"{unit}/{tu}/{name}":
            continue
        try:
            sc = float(json.loads(j.read_text()).get("score") or 0)
        except (OSError, ValueError):
            continue
        if sc < min_score or (below_one and sc >= 1):
            continue
        if j.with_name(name + ".best.c").is_file() and pending(unit, tu, name):
            yield unit, tu, name, j


def rename_port(text: str, missing: set[str]) -> str:
    for pat, rep in RENAMES.items():
        text = re.sub(pat, rep, text)
    for m in missing:                       # generic: fooX -> foo.x
        mm = re.fullmatch(r"(\w*[a-z0-9])([XYZW])", m)
        if mm:
            text = re.sub(r"(->|\.)" + m + r"\b", r"\g<1>" + mm.group(1) + "." + mm.group(2).lower(), text)
    return text


def _struct_body(t: str, start: int) -> tuple[int, int] | None:
    i = t.index("{", start)
    depth = 0
    for k in range(i, len(t)):
        depth += {"{": 1, "}": -1}.get(t[k], 0)
        if depth == 0:
            return start, k + 1
    return None


def find_type_def(tname: str) -> str | None:
    """Complete definition of type TNAME from include/ or src/: `typedef struct
    {...} T;`, `struct T {...};`, or `typedef struct S T;` + `struct S {...};`."""
    files = [f for d in ("include", "src") for f in (ROOT / d).rglob("*.[ch]")]
    alias = None
    for f in files:
        t = f.read_text(errors="replace")
        if tname not in t:
            continue
        for m in re.finditer(r"typedef\s+(?:struct|union)\s*\w*\s*\{|(?:struct|union)\s+" + re.escape(tname)
                             + r"\s*\{", t):
            se = _struct_body(t, m.start())
            tail = se and re.match(r"\s*(\w*)\s*;", t[se[1]:])
            if tail and (tail.group(1) == tname or (not tail.group(1) and not m.group(0).startswith("typedef"))):
                d = t[se[0]:se[1] + tail.end()]
                return d if m.group(0).startswith("typedef") else \
                    f"{d}\ntypedef {m.group(0).split()[0]} {tname} {tname};"
        m = re.search(r"typedef\s+(struct|union)\s+(\w+)\s+" + re.escape(tname) + r"\s*;", t)
        if m and not alias:
            alias = m
    if alias:
        body = find_type_def(alias.group(2)) if alias.group(2) != tname else None
        if body:
            return body + "\n" + alias.group(0)
    return None


def type_port(text: str, undeclared: set[str]) -> tuple[str | None, str]:
    adds = []
    for tn in sorted(undeclared):
        if not re.search(r"\(\s*(?:const\s+)?" + re.escape(tn) + r"\s*\*+\s*\)", text):
            return None, f"`{tn}` undeclared and not a cast type"
        d = find_type_def(tn)
        if not d:
            return None, f"cast type `{tn}`: no definition found (offset inference not done)"
        adds.append(d)
    return "\n".join(adds) + "\n\n" + text, ""


def try_text(unit, tu, name, text) -> dict:
    with tempfile.NamedTemporaryFile("w", suffix=".c", prefix=f"reval_{name}_", delete=False) as f:
        f.write(text)
    try:
        return try_fn.try_function(unit, tu, name, Path(f.name))
    except try_fn.TryError as e:
        return {"ok": False, "stage": "setup", "error": str(e)}
    finally:
        Path(f.name).unlink()


def first_error(err: str) -> str:
    hits = [m.group(1) for m in re.finditer(r"\.c:\d+: (.*)", err) if "In function" not in m.group(1)]
    for h in hits:
        if re.search(r"no member|undeclared|Internal compiler", h):
            return h[:160]
    if hits:
        return hits[0][:160]
    return (err.strip().splitlines() or [""])[-1][:160]


def is_ok(r: dict) -> bool:
    return bool(r.get("match") or try_fn.is_link_match(r))


def integrate(unit, tu, name, text, r) -> tuple[bool, str]:
    body = BODIES / unit / tu / f"{name}.c"
    body.write_text(text)
    cmd = [sys.executable, "-B", str(HERE / "integrate.py"), "--unit", unit, "--tu", tu, "--name", name,
           "--body", str(body)]
    if not r.get("match"):
        cmd.append("--skip-try")
    patch = try_fn.auto_patch_path(unit, tu, name)
    if patch.is_file():
        cmd += ["--src-patch", str(patch)]
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    try:
        j = json.loads(p.stdout)
    except ValueError:
        return False, (p.stdout + p.stderr)[-160:]
    why = j.get("error") or j.get("refused") or (j.get("failed") or {}).get("last_error") or ""
    if not why and (j.get("verify") or {}).get("identical") is False:
        why = f"whole-file not identical ({j['verify'].get('diff_range_count')} diff ranges), reverted"
    if j.get("skipped"):
        why = "in backoff: " + str(why)
    return bool(j.get("matched")), str(why)[:160]


def revalidate(unit, tu, name, dry: bool) -> dict:
    best = BODIES / unit / tu / f"{name}.best.c"
    text = best.read_text(errors="replace")
    row = {"key": f"{unit}/{tu}/{name}"}
    if ASM_RE.search(text):
        return {**row, "result": "skipped-asm"}
    r = try_text(unit, tu, name, text)
    ported = False
    for _ in range(3):                      # port until it compiles (rename, then types)
        if r.get("ok") or not str(r.get("stage", "")).startswith("compile_cand"):
            break
        err = r.get("error") or ""
        miss, und = set(NO_MEMBER.findall(err)), set(UNDECL.findall(err))
        nt, why = text, ""
        if miss:
            nt = rename_port(text, miss)
        elif und:
            nt, why = type_port(text, und)
        if not nt or nt == text:
            return {**row, "result": "still-failing", "error": why or first_error(err)}
        text, ported = nt, True
        r = try_text(unit, tu, name, text)
    if not r.get("ok"):
        return {**row, "result": "still-failing",
                "error": f"{r.get('stage')}: " + first_error(r.get("error") or "")}
    if not is_ok(r):
        return {**row, "result": "still-failing", "error": f"score {r.get('score')} "
                f"({r.get('differing_words')} words differ)" + (" after port" if ported else "")}
    if ported:
        best.with_name(f"{name}.best.ported.c").write_text(text)
    if dry:
        return {**row, "result": ("ported+" if ported else "") + "would-integrate"}
    ok, tail = integrate(unit, tu, name, text, r)
    if not ok:
        return {**row, "result": "still-failing", "error": "integrate: " + tail}
    return {**row, "result": ("ported+" if ported else "") + "integrated"}


def gate_scan(min_score: float, only: str | None) -> None:
    n = bad = 0
    for unit, tu, name, j in candidates(only, min_score, below_one=True):
        if ASM_RE.search((BODIES / unit / tu / f"{name}.best.c").read_text(errors="replace")):
            continue
        try:
            r = try_fn.try_function(unit, tu, name, BODIES / unit / tu / f"{name}.best.c")
        except try_fn.TryError:
            continue
        if not r.get("ok") or is_ok(r):
            continue
        n += 1
        d = json.loads(j.read_text())
        if r.get("gate"):
            bad += 1
            d.update(suspect_semantics=True, gate=r["gate"][:3])
            print(f"SUSPECT {unit}/{tu}/{name} {d.get('score')} by={d.get('by')} {r['gate'][:2]}", flush=True)
        else:
            d.pop("suspect_semantics", None)
            d.pop("gate", None)
        j.write_text(json.dumps(d) + "\n")
    print(json.dumps({"scanned": n, "suspect_semantics": bad}))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--key")
    ap.add_argument("--dry-run", action="store_true", help="try (and port) only, no integrate")
    ap.add_argument("--gate-scan", action="store_true")
    ap.add_argument("--min-score", type=float, default=0.85, help="--gate-scan: lowest score scanned")
    a = ap.parse_args()
    if a.gate_scan:
        gate_scan(a.min_score, a.key)
        return 0
    rows = []
    for unit, tu, name, _ in list(candidates(a.key)):
        k = f"{unit}/{tu}/{name}"
        if k in claims.active():
            rows.append({"key": k, "result": "skipped-leased"})
            continue
        row = revalidate(unit, tu, name, a.dry_run)
        rows.append(row)
        print(json.dumps(row), flush=True)
    order = ["integrated", "ported+integrated", "would-integrate", "ported+would-integrate",
             "still-failing", "skipped-asm", "skipped-leased"]
    print(f"\n{'result':24} {'function':52} error")
    for row in sorted(rows, key=lambda x: order.index(x["result"]) if x["result"] in order else 99):
        print(f"{row['result']:24} {row['key']:52} {row.get('error', '')}")
    print(json.dumps({res: sum(1 for x in rows if x["result"] == res) for res in order}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Worker brief for one leased function, with the context precomputed.

    python3 -B scripts/auto_decomp/brief.py --unit main --tu file --name MenuFile --owner o302 > brief.txt

Everything a worker otherwise spends its first minutes digging up: the current
best score and instruction diff (try_fn, lock-free, does not use the worker's
try budget), earlier workers' <name>.notes.md, and the closest already-matched
sibling's C. Plus the workflow, the rules, WORKER_LESSONS.md and the WRAP UP
protocol (the time cap is enforced from outside: a worker that is told about a
time limit quits early).
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import try_fn  # noqa: E402

DIFF_LINES = 60


def changed_only(diff, context: int = 2, limit: int = DIFF_LINES) -> str:
    """try_fn's side-by-side diff reduced to the differing rows (marked | < >)
    plus a little context: the whole listing is mostly identical lines."""
    if not diff:
        return ""
    rows = diff if isinstance(diff, list) else diff.splitlines()
    mark = re.compile(r"\s[|<>](\s|$)")
    hit = [i for i, l in enumerate(rows) if i and mark.search(l)]
    keep = sorted({j for i in hit for j in range(max(1, i - context), min(len(rows), i + context + 1))})
    out, prev = [rows[0]], None
    for j in keep:
        if prev is not None and j != prev + 1:
            out.append("  ...")
        out.append(rows[j])
        prev = j
    if len(out) > limit:
        out = out[:limit] + [f"  ... ({len(hit)} differing rows in total)"]
    return "\n".join(out)


def current(unit: str, tu: str, name: str, bdir: Path) -> str:
    best = bdir / f"{name}.best.c"
    body = best if best.is_file() else bdir / f"{name}.c"
    if not body.is_file():
        return "No body yet: start from the .m2c.c / .ghidra.c seeds."
    try:
        r = try_fn.try_function(unit, tu, name, body, diff_limit=2000)
    except Exception as e:                       # noqa: BLE001 - context only
        return f"(could not score {body.name}: {e})"
    if r.get("stage"):
        err = (r.get("error") or "").strip().splitlines()
        return (f"{body.name} does NOT compile yet (stage {r['stage']}). First errors:\n"
                + "\n".join(err[:15]))
    head = (f"{body.name}: score {r.get('score')}, opcode_score {r.get('opcode_score')}, "
            f"{r.get('differing_words')} differing words of {r.get('target_insns')} "
            f"(candidate {r.get('candidate_insns')} insns)")
    diff = changed_only(r.get("diff"))
    note = r.get("note")
    return head + (f"\nNote: {note}" if note else "") + (f"\nDiff (target | ours):\n{diff}" if diff else "")


def sibling_c(unit: str, tu: str, name: str) -> str:
    import next as nx                            # noqa: PLC0415 - heavy, only here
    q = json.loads((ROOT / "build/auto_decomp/queue.json").read_text())
    pending = {nx.key(c["unit"], c["tu"], c["name"]) for c in q["candidates"]}
    c = next((c for c in q["candidates"] if (c["unit"], c["tu"], c["name"]) == (unit, tu, name)), None)
    if c is None:
        return ""
    sib = nx.sibling(ROOT, c, pending)
    if not sib:
        return ""
    p = subprocess.run([sys.executable, "-B", str(ROOT / "tools/tu_edit.py"), "extract",
                        str(ROOT / "src" / unit / f"{tu}.c"), sib["name"], "--from-source"],
                       capture_output=True, text=True, cwd=ROOT)
    if p.returncode != 0 or not p.stdout.strip():
        return ""
    return (f"CLOSEST MATCHED SIBLING in this TU: {sib['name']} (opcode similarity {sib['similarity']}). "
            f"Its C usually transfers almost verbatim:\n{p.stdout.strip()}")


def brief(unit: str, tu: str, name: str, owner: str) -> str:
    R = ROOT
    bdir = R / "build/auto_decomp/bodies" / unit / tu
    asm = next((R / f"build/{unit}").glob(f"asm/**/{tu}/{name}.s"), None)
    body = bdir / f"{name}.c"
    notes = bdir / f"{name}.notes.md"
    parts = [f"""You are decompiling ONE function of the Xenosaga Episode I PS2 decomp (MIPS R5900, gcc 2.96 EE cc1) into C that compiles to BYTE-IDENTICAL machine code. Everything you need is below; do not explore the repo to learn the workflow.

TARGET: unit={unit}  tu={tu}  name={name}  (already leased to owner "{owner}"; do NOT run next.py or claim anything else)
Repo root: {R}   (git branch decomp-work; never touch git, never switch branches)

YOUR TOOLS: if you have a bash/shell tool, use it directly; otherwise use your subagent tool with: "cd {R} && source scripts/env.sh && <command>; print the full output verbatim".

FILES:
- Target assembly (the ground truth): {asm}
- Your body file (the ONLY file you edit): {body} — just this function's C definition (plus any local helper decls it needs).
- Seeds next to it: {name}.best.c (best earlier attempt — START FROM THIS if it exists), {name}.m2c.c (m2c draft), {name}.ghidra.c (Ghidra decompile with recovered struct layouts).
- Context: {R}/src/{unit}/{tu}.c is the translation unit: types, externs, globals, and already-matched sibling functions — imitate their style/types. Headers: {R}/include/ and {R}/src/{unit}/*.h.
- Tricks that worked and plateaus to stop on: {R}/scripts/auto_decomp/WORKER_LESSONS.md (read it).

CORE LOOP:
1. Edit {body} (copy {name}.best.c in first if the body is missing or worse).
2. Test: python3 -B scripts/auto_decomp/try_fn.py --unit {unit} --tu {tu} --name {name} --body {body}
   JSON: "match" (true = done), "score" 0..1, "differing_words", "diff" (target vs ours). READ THE DIFF, make one targeted edit per run. Compile errors are printed too.
3. When try_fn prints "match": true: python3 -B scripts/auto_decomp/integrate.py --unit {unit} --tu {tu} --name {name} --body {body}
   It must print "matched": true. That is the only thing that edits src/ — never hand-edit src/.
Helpers: scripts/auto_decomp/hoist.py, scripts/auto_decomp/verified_edit.py (--help).

RULES: 60 try_fn runs per function, one deliberate diff-driven edit per run, no scripted/random searches (a permuter does that separately). Never delete/move/rename files, never run rm/mv/ln/git. NO inline asm of any kind. Edit only {body}. Don't ask questions. Never claim a match unless try_fn printed "match": true and integrate printed "matched": true (quote them).
TIME: there is NO time limit for you to track — never stop because of time, never claim a time limit was reached. Keep going until you match or use the 60 runs, or until you receive a WRAP UP message. On WRAP UP: finish the step you are on, make sure {body} holds your best version, then stop and report. Keep {body} at your best version after every improvement.

FINAL MESSAGE: matched yes/no, best score, the integrate output (if any), the remaining diff in one line, and the variants you tried that did NOT help (so the next worker skips them)."""]
    if notes.is_file():
        parts.append(f"EARLIER WORKERS' NOTES ({notes.name}) — read before anything else:\n{notes.read_text().strip()}")
    up = R / "build/upstream_ref/src" / unit / f"{tu}.c"
    if up.is_file() and up.read_bytes() != (R / "src" / unit / f"{tu}.c").read_bytes():
        parts.append(f"UPSTREAM REFERENCE (read-only, never copy wholesale): {up} is upstream's copy of this TU. "
                     "It has recovered typed data (struct layouts, tables, statics) we may lack: grep it for the "
                     "globals your function touches and borrow their types into your body file.")
    parts.append("CURRENT STATE (precomputed, did not use your budget):\n" + current(unit, tu, name, bdir))
    sib = sibling_c(unit, tu, name)
    if sib:
        parts.append(sib)
    return "\n\n".join(parts) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", required=True)
    ap.add_argument("--tu", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--owner", required=True)
    a = ap.parse_args()
    sys.stdout.write(brief(a.unit, a.tu, a.name, a.owner))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

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
        r = try_fn.try_function(unit, tu, name, body, diff_limit=2000, uninit=True)
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
    sus = r.get("suspect") or []
    warn = ""
    if sus:
        # fold9: o483 set_clip, o485, o491, o492, ov01/cmd seeds scored 0.79-0.95 with wrong logic
        warn = ("\nWARNING: the seed may be semantically WRONG (" + "; ".join(sus[:3]) + "). "
                "Several earlier seeds scored 0.8-0.95 while computing the wrong thing. Before tuning, "
                "check those spots against the asm (field types / widths, `p += K` on a typed pointer is "
                "an element step, constants, missing branches); consider rewriting from the asm.")
    imp = r.get("implicit") or []
    if imp:                                       # fold14 (o564 SEQ_moveUnitChr)
        pr = [try_fn.real_proto(f) or f"{f}(?)" for f in imp[:6]]
        warn += ("\nNOTE: called with NO prototype in scope (gcc assumes `int f()`: int return, float args "
                 "promoted to double): " + ", ".join(imp[:6]) + ". Declare them before the function: "
                 + " ".join(p.strip() for p in pr))
    nc = r.get("narrow_arg_calls") or []
    if nc:                                        # fold15 (o536/o574/o575/o577 thinkAdrGet)
        warn += ("\nNOTE: ours sign/zero-extends an argument of " + ", ".join(nc[:4]) + " that the target "
                 "passes raw (the prototype narrows a short/char param): call those sites through an int "
                 "cast, e.g. `((int (*)(int))" + nc[0] + ")(x)` (or unprototyped `((int (*)())" + nc[0]
                 + ")(x)`), per call site; trick_sweep's param_widen:call tries this too.")
    gd = None
    if not r.get("match") and not r.get("match_pending_jtbl"):
        gd = try_fn.gcse_diag(unit, tu, name, body, r.get("score"))   # fold12 (o526, o532)
    if gd and gd.get("better"):
        warn += "\nDIAGNOSTIC: " + gd["hint"]
    sd = None
    if not r.get("match") and not r.get("match_pending_jtbl"):
        sd = try_fn.sched_diag(unit, tu, name, body, r.get("score"))   # fold13 (o537)
    if sd and sd.get("better"):
        warn += "\nDIAGNOSTIC: " + sd["hint"]
    return (head + warn + (f"\nNote: {note}" if note else "")
            + (f"\nDiff (target | ours):\n{diff}" if diff else ""))


def _extract_c(unit: str, tu: str, fn: str) -> str:
    p = subprocess.run([sys.executable, "-B", str(ROOT / "tools/tu_edit.py"), "extract",
                        str(ROOT / "src" / unit / f"{tu}.c"), fn, "--from-source"],
                       capture_output=True, text=True, cwd=ROOT)
    return p.stdout.strip() if p.returncode == 0 else ""


def sibling_c(unit: str, tu: str, name: str) -> str:
    import next as nx                            # noqa: PLC0415 - heavy, only here
    q = json.loads((ROOT / "build/auto_decomp/queue.json").read_text())
    pending = {nx.key(c["unit"], c["tu"], c["name"]) for c in q["candidates"]}
    c = next((c for c in q["candidates"] if (c["unit"], c["tu"], c["name"]) == (unit, tu, name)), None)
    if c is None:
        return ""
    out = []
    sib = nx.sibling(ROOT, c, pending)
    if sib:
        txt = _extract_c(unit, tu, sib["name"])
        if txt:
            out.append(f"CLOSEST MATCHED SIBLING in this TU: {sib['name']} (opcode similarity {sib['similarity']}). "
                       f"Its C usually transfers almost verbatim:\n{txt}")
    # fold10 (o493-o504: ~10 ov01/cmd functions matched in 1-2 tries right
    # after a neighbour): the most similar same-TU functions matched recently
    recent = nx.recent_matches(ROOT)
    rec = recent.get((unit, tu)) or []
    sims = []
    mine = nx._mnemonics(ROOT / c["asm"]) if c.get("asm") else []
    if mine and rec:
        import difflib                           # noqa: PLC0415
        for n in sorted({n for n, _ in rec} - {name}):
            theirs = nx._mnemonics((ROOT / c["asm"]).parent / f"{n}.s")
            if theirs:
                sims.append((round(difflib.SequenceMatcher(None, mine, theirs, autojunk=False).ratio(), 3), n))
    sims = [x for x in sorted(sims, reverse=True) if x[0] >= 0.5][:2]
    if sims:
        lines = [f"RECENTLY MATCHED SIBLINGS in this TU (last {nx.HOT_HOURS:g} h; copy their style, types and "
                 "helper calls first - right after a neighbour matched, siblings in this TU went in 1-2 tries): "
                 + ", ".join(f"{n} (opcode similarity {r})" for r, n in sims)]
        top = sims[0][1]
        if not sib or top != sib["name"]:
            txt = _extract_c(unit, tu, top)
            if txt:
                lines.append(f"{top}:\n{txt}")
        out.append("\n".join(lines))
    return "\n\n".join(out)


_INLINE_DEF = re.compile(r"^static\s+(?:__)?inline(?:__)?\b([^;{}()]*?)\b([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*\{", re.M)


def inline_helpers(unit: str, tu: str, limit: int = 12) -> str:
    """`static inline` helpers defined in the TU and the matched functions that
    call them (o490 SGsSetZTestEnv / SGsRestoreEnv: SGsAddReg_i like its
    siblings instead of raw scratchpad writes matched in one try)."""
    try:
        text = (ROOT / "src" / unit / f"{tu}.c").read_text(errors="replace")
    except OSError:
        return ""
    defs = [(m.group(2), f"static inline {m.group(1).strip()} {m.group(2)}({m.group(3).strip()})".replace("  ", " "),
             m.start()) for m in _INLINE_DEF.finditer(text)]
    if not defs:
        return ""
    heads = [(m.start(), m.group(1)) for m in re.finditer(
        r"^(?!static\s+(?:__)?inline)[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{", text, re.M)]
    out = []
    for nm, sig, at in defs[:limit]:
        users = []
        for i, (pos, fn) in enumerate(heads):
            end = heads[i + 1][0] if i + 1 < len(heads) else len(text)
            if fn != nm and re.search(r"\b" + re.escape(nm) + r"\s*\(", text[pos:end]) and fn not in users:
                users.append(fn)
        out.append(f"- {sig}" + (f"   used by: {', '.join(users[:4])}" if users else ""))
    return ("INLINE HELPERS already defined in this TU (matched siblings call them; when your function does the "
            "same stores / packet writes, call the helper instead of open-coding them - o490 SGsSetZTestEnv "
            "matched in one try with SGsAddReg_i):\n" + "\n".join(out))


_W = {"lb": ("s8", 1), "lbu": ("u8", 1), "sb": ("u8", 1), "lh": ("s16", 2), "lhu": ("u16", 2), "sh": ("s16", 2),
      "lw": ("s32", 4), "sw": ("s32", 4), "lwc1": ("f32", 4), "swc1": ("f32", 4), "ld": ("u64", 8), "sd": ("u64", 8)}


def _ival(e: str) -> int | None:
    e = e.strip()
    if not re.fullmatch(r"[0-9a-fA-FxX+\-* ()]+", e):
        return None
    try:
        return int(eval(e, {"__builtins__": {}}))   # noqa: S307 - digits/operators only
    except Exception:  # noqa: BLE001
        return None


def unmodeled_hint(unit: str, tu: str, name: str, asm: Path | None) -> str:
    """Fold13 (o549 Intermission): the target reads/writes SYM + K through a
    base register built by `addiu rB, rX, %lo(SYM)` while K lies inside an
    `unmodeled_XX[N]` filler of SYM's struct: point the worker at a typed
    field carved via <name>.patch.json (with the exact OLD/NEW text) instead
    of a raw `*(T *)((char *)&SYM + K)` cast."""
    if not asm or not asm.is_file():
        return ""
    base: dict = {}
    hits: dict = {}
    for ln in asm.read_text(errors="replace").splitlines():
        m = re.search(r"\*/\s+(\S+)\s+(.*)$", ln)
        if not m:
            continue
        mn, ops = m.group(1), [x.strip() for x in m.group(2).split(",")]
        if mn == "addiu" and len(ops) == 3 and (s := re.fullmatch(r"%lo\(([A-Za-z_]\w*)\)", ops[2])):
            base[ops[0]] = s.group(1)
            continue
        mm = re.fullmatch(r"(-?(?:0x[0-9A-Fa-f]+|\d+))\((\$\w+)\)", ops[-1]) if len(ops) == 2 else None
        if mn in _W and mm and mm.group(2) in base:
            off = int(mm.group(1), 0)
            if off > 0:
                hits.setdefault((base[mm.group(2)], off), mn)
        elif ops and ops[0] in base and mn not in ("sb", "sh", "sw", "sd", "sq", "swc1", "sdc1", "swl", "swr") \
                and not mn.startswith(("b", "j")):
            del base[ops[0]]                     # base register overwritten
    if not hits:
        return ""
    srcs = sorted((ROOT / "src" / unit).glob("*.h")) + [ROOT / "src" / unit / f"{tu}.c"]
    texts = {p: p.read_text(errors="replace") for p in srcs if p.is_file()}
    groups: dict = {}     # (file, filler text) -> {"sym", "ty", "f", "n", "start", "size", "ind", "offs": {off: mn}}
    for (sym, off), mn in sorted(hits.items()):
        ty = None
        for t in texts.values():
            d = re.search(r"^\s*extern\s+(?:struct\s+)?(\w+)\s+" + re.escape(sym) + r"\s*;", t, re.M) or \
                re.search(r"^\s*(?:static\s+)?(?:struct\s+)?(\w+)\s+" + re.escape(sym) + r"\s*(?:=|;)", t, re.M)
            if d:
                ty = d.group(1)
                break
        if not ty:
            continue
        for p, t in texts.items():
            sd = re.search(r"typedef\s+struct\s*\w*\s*\{(?P<b>(?:[^{}]|\{[^{}]*\})*)\}\s*" + re.escape(ty) + r"\s*;", t) or \
                re.search(r"struct\s+" + re.escape(ty) + r"\s*\{(?P<b>(?:[^{}]|\{[^{}]*\})*)\}\s*;", t)
            if not sd:
                continue
            for fm in re.finditer(r"^(?P<ind>[ \t]*)(?:u8|unsigned\s+char|char|s8)\s+(?P<f>unmodeled_(?P<o>[0-9A-Fa-f]+))"
                                  r"\s*\[(?P<n>[^\]]+)\]\s*;[^\n]*\n", sd.group("b"), re.M):
                start, n = int(fm.group("o"), 16), _ival(fm.group("n"))
                w = _W[mn][1]
                if n is None or not (start <= off and off + w <= start + n) or t.count(fm.group(0)) != 1:
                    continue
                g = groups.setdefault((p, fm.group(0)), {"sym": sym, "ty": ty, "f": fm.group("f"), "n": fm.group("n"),
                                                         "start": start, "size": n, "ind": fm.group("ind"), "offs": {}})
                g["offs"].setdefault(off, mn)
                break
    out = []
    for (p, old), g in groups.items():
        ind, cur, end = g["ind"], g["start"], g["start"] + g["size"]
        new, desc = "", []
        for off in sorted(g["offs"]):
            T, w = _W[g["offs"][off]]
            if off < cur:                        # overlaps the previous field (another width): skip
                continue
            if off > cur:
                new += f"{ind}u8 unmodeled_{cur:x}[0x{off - cur:x}];\n"
            new += f"{ind}{T} field_{off:x};\n"
            desc.append(f"+0x{off:x} {g['offs'][off]}")
            cur = off + w
        if cur < end:
            new += f"{ind}u8 unmodeled_{cur:x}[0x{end:x} - 0x{cur:x}];\n"
        rel = p.relative_to(ROOT).as_posix()
        out.append(f"- {g['sym']} ({', '.join(desc)}) inside `{g['f']}[{g['n']}]` of {g['ty']} in {rel}. "
                   f"Suggested {name}.patch.json entry: " + json.dumps([rel, old, new]))
    if not out:
        return ""
    return ("UNMODELED FIELDS (fold13, o549 Intermission matched only once the field was real): the target "
            "accesses these offsets through a base register (`addiu rB, %lo(SYM)` then `l*/s* K(rB)`). Model "
            "them as typed struct fields via the patch file (rename `field_K` as you like; integrate applies it "
            "with verified_edit.py) instead of raw `*(T *)((char *)&SYM + K)` casts, which fold K into the "
            "address and use the wrong width when guessed:\n" + "\n".join(out[:6]))


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
- Optional prototype fix: {bdir}/{name}.patch.json (the only other file you may create). If the TU itself must change (a stale forward declaration / prototype: void vs int return, wrong arg count or types, plus its existing callers), write JSON [["src/{unit}/{tu}.c", "OLD text", "NEW text"], ...] there (files in src/{unit}/ only, each OLD exactly once). try_fn.py and integrate.py pick it up automatically: try_fn compiles against the patched TU without writing it, integrate applies it with verified_edit.py (kept only if every already-matched function stays byte-identical). Never edit src/ yourself.
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

RULES: 60 try_fn runs per function, one deliberate diff-driven edit per run, no scripted/random searches (a permuter does that separately). Never delete/move/rename files, never run rm/mv/ln/git. NO inline asm of any kind. Edit only {body} (and {name}.patch.json, see FILES). Don't ask questions. Never claim a match unless try_fn printed "match": true and integrate printed "matched": true (quote them).
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
    try:
        um = unmodeled_hint(unit, tu, name, asm)
    except Exception:  # noqa: BLE001 - context only
        um = ""
    if um:
        parts.append(um)
    sib = sibling_c(unit, tu, name)
    if sib:
        parts.append(sib)
    ih = inline_helpers(unit, tu)
    if ih:
        parts.append(ih)
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

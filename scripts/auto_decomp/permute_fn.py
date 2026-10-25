#!/usr/bin/env python3
"""Run decomp-permuter on a near-miss body, integrate the result if it matches.

    python3 -B scripts/auto_decomp/permute_fn.py --unit main --tu sc_get \\
        --name scWAITCNTScript --body build/auto_decomp/bodies/main/sc_get/scWAITCNTScript.c \\
        [--minutes 3] [-j N] [--stall 1] [--no-gain 2.5] [--extend 2] [--no-integrate]

Builds build/auto_decomp/permute/<unit>__<tu>__<name>/ for the permuter:
  base.c     the candidate TU preprocessed, other functions reduced to
             prototypes, INCLUDE_ASM blocks dropped
  target.o   the TU with NAME as INCLUDE_ASM (original bytes)
  compile.sh the TU's own cc1 + as steps run directly on the preprocessed
             source (checked against try_fn.py --compile-file at setup;
             falls back to it if the objects differ)
  settings.toml  objdump restricted to NAME, randomization weights tuned for
             this compiler's plateaus (register rotations, scheduling order)
The permuter mutates only NAME, in segments: when a segment improves on the
base score and then stalls for --stall minutes, the best output becomes the
new base (a fresh walk from there). A segment that has not beaten its base
after --no-gain minutes ends the run (measured: 112 of 202 unmatched 5-minute
runs never beat their base, while every match showed its first gain within
~125 s); a run still improving at the deadline gets --extend minutes once.
Every improvement that try_fn also scores higher is saved as <name>.best.c,
so partial wins survive. On a score-0
candidate the function is extracted, re-checked with try_fn and handed to
integrate.py.
Prints a JSON report. Exit 0 = matched (and integrated unless --no-integrate).
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "tools"))
import try_fn  # noqa: E402
import tu_edit  # noqa: E402

PERMUTER = ROOT / ".work/decomp-permuter/permuter.py"
PY = ROOT / ".venv/bin/python"
WORK = ROOT / "build/auto_decomp/permute"
JOBS = max(1, (os.cpu_count() or 4) - 2)

# Default gcc weights plus more of the passes that move register allocation and
# scheduling, the usual last diffs here (see WORKER_LESSONS.md).
WEIGHTS = {
    "perm_reorder_stmts": 30, "perm_reorder_decls": 40, "perm_temp_for_expr": 100,
    "perm_split_assignment": 15, "perm_commutative": 25, "perm_refer_to_var": 25,
    "perm_randomize_internal_type": 15, "perm_struct_ref": 30, "perm_ins_block": 25,
    "perm_compound_assignment": 10, "perm_duplicate_assignment": 10,
}

# --profile: extra weights for a diff class tagged by permute_queue triage.
# branch_likely: a lone b<cond> vs b<cond>l flip whose annul choice reorg makes
# from the label/jump layout at the fall-through -> label/goto/condition edits.
PROFILES = {
    "branch_likely": {"perm_ins_block": 120, "perm_condition": 100, "perm_var_cond_block": 60,
                      "perm_empty_stmt": 40, "perm_reorder_stmts": 40},
    # trick_sweep diff-shape tags (permute_queue triage "mech"), see WORKER_LESSONS.md
    "addu_swap": {"perm_commutative": 150, "perm_cast_simple": 60},                    # JNT_getStaticVal
    "sign_flip": {"perm_randomize_internal_type": 100, "perm_randomize_function_type": 60,
                  "perm_cast_simple": 80, "perm_inequalities": 40},
    "param_widen": {"perm_randomize_function_type": 150, "perm_cast_simple": 40},      # nmlModelDirectSend
    "const_rev": {"perm_reorder_stmts": 120, "perm_chain_assignment": 40},              # JNT_getStaticVal
    "ret_local": {"perm_remove_var": 80, "perm_refer_to_var": 40},                      # svAddImage
    # seventh round (2026-10-06)
    "float_cmp_neg": {"perm_condition": 120, "perm_inequalities": 120, "perm_commutative": 40},  # HitCheckUwamonoAt
    "field_reread": {"perm_refer_to_var": 100, "perm_remove_var": 80, "perm_expand_expr": 60},  # sefCnvDeathEffectNo
    "bit_split": {"perm_temp_for_expr": 150, "perm_split_assignment": 80,
                  "perm_compound_assignment": 60},                                               # objDispCur
    "early_return": {"perm_condition": 80, "perm_ins_block": 80, "perm_empty_stmt": 30},       # scenarioSrcEndChk
    # ninth round (fold3): InitSaveSymbol matched by the permuter with a dead
    # `if ((v = p && p) && p) {}` (a sched1-only block boundary) + statement order
    "block_barrier": {"perm_ins_block": 100, "perm_temp_for_expr": 80, "perm_reorder_stmts": 80,
                      "perm_condition": 40},
    "load_width_cast": {"perm_cast_simple": 150, "perm_randomize_internal_type": 60},          # counterProc
    # AI-worker fold 2: struct ref <-> pointer arithmetic (CC_Kara_DisposeEvent byte-offset index)
    "byte_offset_index": {"perm_struct_ref": 150, "perm_expand_expr": 60, "perm_cast_simple": 40},
    # 2026-10-13 Opus matches: whole-loop/statement block (loop_wrap: nmlModelRenderDrop,
    # command_enenpcse), || -> else-if arms (Check_Wall), one variable reused across
    # blocks / fresh pointer reads (HitCheckBox*, dataEnemyMdlChk), spill order = decl order
    "loop_wrap": {"perm_ins_block": 150, "perm_empty_stmt": 40, "perm_reorder_stmts": 40},
    "or_split": {"perm_condition": 150, "perm_ins_block": 60},
    "var_reuse": {"perm_refer_to_var": 120, "perm_remove_var": 80, "perm_temp_for_expr": 80},
    "decl_order": {"perm_reorder_decls": 200},
    # g21 GamePushSaveDataUser: reversing independent pointer initializers fixed
    # one scheduled load but exchanged pointer registers; vary statement and
    # declaration order together with variable references to search both effects.
    "sched_order": {"perm_reorder_stmts": 150, "perm_reorder_decls": 100,
                    "perm_refer_to_var": 100, "perm_temp_for_expr": 80},
    # 2026-10-18 Opus o20 build_mipmap (ov02's last function): the final register
    # allocation came from where each constant's load first appears -> constants
    # pulled into locals (`c = 0x2000;`) and only those assignment lines moved
    # (310-iteration local search matched). Same search here: hoist constants
    # into temps, then reorder statements/declarations.
    "const_place": {"perm_temp_for_expr": 200, "perm_reorder_stmts": 200,
                    "perm_reorder_decls": 80, "perm_expand_expr": 40},
}


def c_name_of(body_text: str, name: str) -> str:
    m = re.match(r"(.+)_[0-9A-Fa-f]{8}$", name)
    if m and re.search(r"\b" + re.escape(m.group(1)) + r"\s*\(", body_text) \
            and not re.search(r"\b" + re.escape(name) + r"\s*\(", body_text):
        return m.group(1)
    return name


def strip_tu(text: str, keep: str) -> str:
    """Drop top-level asm, reduce every other function definition to a prototype."""
    out = []
    for it in tu_edit.scan(text):
        t = it.text(text)
        if it.kind == "decl" and t.lstrip().startswith(("__asm__", "asm")):
            continue
        if it.kind == "func" and it.name != keep and not tu_edit._is_inline_def(text, it):
            head = t[:t.index("{")].rstrip()
            out.append(head + ";\n")
            continue
        out.append(t if t.endswith("\n") else t + "\n")
    return "".join(out)


def find_i(d: Path) -> Path:
    for p in list(d.glob("*.o.i")) + list(d.glob("*.i")):
        return p
    raise FileNotFoundError(f"no preprocessed .i in {d}")


def fast_compile_sh(unit: str, tu: str) -> str | None:
    """compile.sh running the TU's cc1 and as steps directly (no cpp: base.c is
    already preprocessed), about 5x faster than going through try_fn.py."""
    unit_dir, g, rule, edge = try_fn.compile_edge(unit, tu)
    scope = dict(g)
    scope.update({k: try_fn.expand(v, g) for k, v in edge["vars"].items()})
    scope.update({"overlay": "OVL", "in": "IN.c", "out": "OUT.o"})
    cmd = try_fn.expand(rule["command"], scope)
    m = re.search(r"cc_tu\.sh IN\.c OUT\.o (\S+) (\S+) (-G\d+)", cmd)
    if m:                                         # overlays: tools/tu/cc_tu.sh
        ccdir, as_, gflag = m.groups()
        af = re.search(r"XENO_TU_ASFLAGS='([^']*)'", cmd)
        asflags = af.group(1) if af else ""
    else:                                         # main: inline ninja rule
        cc = re.search(r"(\S+)/ee-gcc ", cmd)
        asm = re.search(r"(\S+/ee-as) -EL -m5900 -mabi=eabi (-G\d+)(.*?) -I \. ", cmd)
        if not cc or not asm:
            return None
        ccdir, as_, gflag, asflags = cc.group(1), asm.group(1), asm.group(2), asm.group(3).strip()
    # cc1 sees the build's own .i / .s names (relative, inside the temp dir):
    # their lengths shift cc1's heap (try_fn.same_len_link); ASLR off as in the build.
    out = edge["outs"][0]
    stem = out[:-2] if m else out                  # cc_tu.sh: ${OUT%.o}.i ; ninja rule: $out.i
    i_rel, s_rel = stem + ".i", stem + ".s"
    if not re.fullmatch(r"[\w./+-]+", i_rel) or i_rel.startswith(("/", "..")):
        i_rel, s_rel = "a.i", "a.s"
    noaslr = " ".join(try_fn.NOASLR)
    return ("#!/bin/bash\n"
            "t=$(mktemp -d /dev/shm/permc.XXXXXX) || exit 1\n"
            "trap 'rm -rf \"$t\"' EXIT\n"
            'o=$(realpath -m "$3") &&\n'
            f'mkdir -p "$t/$(dirname "{i_rel}")" && cp "$1" "$t/{i_rel}" && cd "$t" &&\n'
            f'{noaslr} "{ccdir}/ee-gcc" -B"{ccdir}/" -nostdinc -fno-builtin -S -O2 {gflag} "{i_rel}" -o "{s_rel}" &&\n'
            f'"{as_}" -EL -m5900 -mabi=eabi {gflag} {asflags} -o "$o" "{s_rel}" > "$t/m" 2>&1 &&\n'
            '! test -s "$t/m"\n')


def text_of(obj: Path, cname: str) -> str:
    p = subprocess.run([str(try_fn.OBJDUMP), "-drz", f"--disassemble={cname}", str(obj)],
                       capture_output=True, text=True)
    return "\n".join(l for l in p.stdout.splitlines() if l.startswith(" ") or l.startswith("\t"))


def setup(unit: str, tu: str, name: str, body: Path, profile: str | None = None) -> tuple[Path, str]:
    t = try_fn.try_function(unit, tu, name, body, keep=True)
    scratch = Path(t["scratch"])
    try:
        if t.get("stage"):
            raise try_fn.TryError(f"candidate does not compile ({t.get('stage')}): "
                                  f"{(t.get('error') or '')[-800:]}")
        cname = c_name_of(body.read_text(), name)
        d = WORK / f"{unit}__{tu}__{name}"
        shutil.rmtree(d, ignore_errors=True)
        d.mkdir(parents=True)
        (d / "base.c").write_text(strip_tu(find_i(scratch / "cand").read_text(), cname))
        shutil.copyfile(next((scratch / "base").glob("*.o")), d / "target.o")
        slow = ("#!/bin/bash\n"
                f'exec "{sys.executable}" -B "{HERE / "try_fn.py"}" --compile-file "$1" '
                f'--unit {unit} --tu {tu} -o "$3"\n')
        fast = fast_compile_sh(unit, tu)
        if fast:                                  # use it only if it reproduces try_fn's object
            for n, text in (("slow", slow), ("fast", fast)):
                (d / f"compile.{n}.sh").write_text(text)
                (d / f"compile.{n}.sh").chmod(0o755)
                subprocess.run([str(d / f"compile.{n}.sh"), str(d / "base.c"), "-o", str(d / f"{n}.o")],
                               capture_output=True)
            ok = all((d / f"{n}.o").is_file() for n in ("slow", "fast")) \
                and text_of(d / "slow.o", cname) == text_of(d / "fast.o", cname)
            if not ok:
                fast = None
        (d / "compile.sh").write_text(fast or slow)
        (d / "compile.sh").chmod(0o755)
        (d / "settings.toml").write_text(
            f'func_name = "{cname}"\ncompiler_type = "gcc"\n'
            f'objdump_command = "{try_fn.OBJDUMP} -drz --disassemble={cname}"\n'
            "\n[weight_overrides]\n" + "".join(f"{k} = {v}\n" for k, v in
                                               {**WEIGHTS, **PROFILES.get(profile or "", {})}.items()))
        return d, cname
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def best_output(d: Path) -> tuple[int | None, Path | None]:
    best, path = None, None
    for o in d.glob("output-*"):
        m = re.match(r"output-(\d+)-", o.name)
        if not m or not (o / "source.c").is_file():
            continue
        sc = int(m.group(1))
        if best is None or sc < best:
            best, path = sc, o / "source.c"
    return best, path


def base_score(d: Path) -> int | None:
    m = re.findall(r"base score = (\d+)", (d / "permuter.log").read_text(errors="replace"))
    return int(m[0]) if m else None


def run_segment(d: Path, deadline: float, stall: float, jobs: int, on_better,
                no_gain: float | None = None) -> dict:
    """One permuter walk from base.c until score 0, the deadline, --stall
    minutes without a new best after it beat the base, or --no-gain minutes
    without beating the base at all. on_better(score, path) sees every new best
    output and returns True to stop (try_fn matched)."""
    log = (d / "permuter.log").open("w")
    proc = subprocess.Popen([str(PY), str(PERMUTER), str(d), "-j", str(jobs), "--stop-on-zero",
                             "--best-only", "--quiet"],
                            stdout=log, stderr=subprocess.STDOUT, cwd=d,
                            start_new_session=True)
    t0 = time.time()
    best, last_gain, why = None, t0, "deadline"
    while proc.poll() is None and time.time() < deadline:
        time.sleep(5)
        sc, path = best_output(d)
        if sc is not None and (best is None or sc < best):
            best, last_gain = sc, time.time()
            if on_better(sc, path):
                best, why = 0, "match"
                break
        base = base_score(d)
        if sc == 0:
            why = "match"
            break
        if base is not None and best is not None and best < base and time.time() - last_gain > stall * 60:
            why = "stall"
            break
        if no_gain and time.time() - t0 > no_gain * 60 and (best is None or base is None or best >= base):
            why = "no_gain"                       # never beat its base: a plateau, rotate
            break
    if proc.poll() is None:
        os.killpg(proc.pid, signal.SIGINT)
        try:
            proc.wait(20)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
    log.close()
    sc, path = best_output(d)
    return {"base": base_score(d), "best": 0 if best == 0 else sc, "path": path, "rc": proc.returncode,
            "why": why, "secs": round(time.time() - t0), "last_gain": round(last_gain - t0)}


def run_permuter(d: Path, minutes: float, jobs: int, stall: float, on_better,
                 no_gain: float | None = None, extend: float = 0) -> dict:
    """Segments: a segment that beat its base and stalled restarts from its best
    output (outputs are kept under rounds/<n>/). A run still improving when the
    budget ends (its last segment beat its base and either just rebased or found
    a new best within --stall minutes) gets --extend more minutes, once."""
    start = time.time()
    deadline = start + minutes * 60
    extended = False
    segs, best, best_path = [], None, None
    while True:
        if time.time() >= deadline - 10:
            last = segs[-1] if segs else None
            if extend and not extended and last and last["best"] is not None and last["base"] is not None \
                    and last["best"] < last["base"] \
                    and (last["why"] == "stall" or last["secs"] - last["last_gain"] < stall * 60):
                deadline, extended = deadline + extend * 60, True
            else:
                break
        r = run_segment(d, deadline, stall, jobs, on_better, no_gain)
        segs.append({k: r[k] for k in ("base", "best", "why", "secs", "last_gain")})
        if r["best"] is not None and (best is None or r["best"] < best):
            best = r["best"]
            keep = d / "rounds" / str(len(segs))
            keep.mkdir(parents=True, exist_ok=True)
            best_path = keep / "source.c"
            shutil.copyfile(r["path"], best_path)
        if best == 0 or r["best"] is None or r["base"] is None or r["best"] >= r["base"]:
            break                                 # matched, or no progress this segment
        shutil.copyfile(best_path, d / "base.c")  # rebase on the improvement
        for o in d.glob("output-*"):
            shutil.rmtree(o, ignore_errors=True)
    tail = (d / "permuter.log").read_text(errors="replace")[-1500:]
    return {"best_permuter_score": best, "best_source": str(best_path) if best_path else None,
            "segments": segs, "secs": round(time.time() - start), "extended": extended, "log_tail": tail}


def extract(source: Path, cname: str) -> str:
    p = tu_edit_cli("extract", str(source), cname, "--from-source")
    if p.returncode != 0:
        raise try_fn.TryError(f"extract failed: {(p.stdout + p.stderr)[-800:]}")
    return p.stdout


def tu_edit_cli(*a):
    return subprocess.run([sys.executable, "-B", str(ROOT / "tools/tu_edit.py"), *a],
                          capture_output=True, text=True, cwd=ROOT)


def leading_decls(body_text: str) -> str:
    """The declarations an agent put before the definition (kept with the result)."""
    try:
        lead, _ = tu_edit.split_leading_declarations(body_text)
    except Exception:
        return ""
    return "\n".join(lead) + ("\n\n" if lead else "")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", required=True)
    ap.add_argument("--tu", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--body", type=Path, required=True)
    ap.add_argument("--minutes", type=float, default=3)
    ap.add_argument("-j", type=int, default=JOBS)
    ap.add_argument("--stall", type=float, default=1,
                    help="minutes without a new best (after beating the base) before rebasing")
    ap.add_argument("--no-gain", type=float, default=2.5,
                    help="minutes a segment may run without beating its base before the run stops (0 = off)")
    ap.add_argument("--extend", type=float, default=2,
                    help="extra minutes, once, when the run is still improving at the deadline (0 = off)")
    ap.add_argument("--no-integrate", action="store_true")
    ap.add_argument("--profile", choices=sorted(PROFILES), help="extra weights for a triage tag")
    a = ap.parse_args()
    key = f"{a.unit}/{a.tu}/{a.name}"
    report: dict = {"key": key}
    body = a.body.resolve()
    try:
        d, cname = setup(a.unit, a.tu, a.name, body, a.profile)
    except (try_fn.TryError, FileNotFoundError, ValueError) as e:
        report.update(ok=False, stage="setup", error=str(e))
        print(json.dumps(report, indent=1))
        return 2
    report["dir"] = str(d)
    lead = leading_decls(body.read_text())
    best_side = body.with_name(f"{a.name}.best.json")

    def on_better(sc: int, path: Path) -> bool:
        """Keep a permuter improvement as .best.c when try_fn agrees it is better.
        try_fn can match before the permuter's own score reaches 0 (its objdump
        text also differs on symbol names and stack noise): stop there."""
        try:
            cand = body.with_name(f"{a.name}.perm_cand.c")
            cand.write_text(lead + extract(path, cname))
            t = try_fn.try_function(a.unit, a.tu, a.name, cand, uninit=True)
            # fold11: a suspect improvement (e.g. o511's uninitialized local) must beat
            # a clean best by SUSPECT_MARGIN; a clean one may replace a suspect best below it
            better, sus = try_fn.prefer_over_best(a.unit, a.tu, a.name, cand, t.get("score"), t, best_side)
        except (try_fn.TryError, OSError):
            return False
        if better and not t.get("match") and not try_fn.is_link_match(t) and t.get("gate"):
            # correctness gate: other callees / store counts / data symbols than the target
            better = False
            report.setdefault("gate_refused", []).append({"score": t.get("score"), "why": t["gate"][:3]})
            print(f"permute_fn: not saving {t.get('score')}: gate {t['gate'][:2]}", file=sys.stderr, flush=True)
        if better:
            shutil.copyfile(cand, body.with_name(f"{a.name}.best.c"))
            best_side.write_text(json.dumps(try_fn.best_record(t["score"], t, sus, by="permuter")) + "\n")
            report.setdefault("saved_best", []).append(t["score"])
        if t.get("match") or try_fn.is_link_match(t):   # incl. match_pending_jtbl / link_check
            report["matched_source"] = str(cand)
            return True
        return False

    report.update(run_permuter(d, a.minutes, a.j, a.stall, on_better, a.no_gain or None, a.extend))
    if report.get("matched_source"):
        report["best_permuter_score"] = 0
    if report["best_permuter_score"] != 0:
        report.update(ok=False, stage="permuter")
        print(json.dumps(report, indent=1))
        return 1
    out_body = body.with_name(body.stem + ".permuted.c")
    if report.get("matched_source"):
        shutil.copyfile(report["matched_source"], out_body)
    else:
        out_body.write_text(lead + extract(Path(report["best_source"]), cname))
    t = try_fn.try_function(a.unit, a.tu, a.name, out_body)
    report["try"] = {k: t.get(k) for k in ("match", "score", "stage", "error", "link_check",
                                            "match_after_link", "match_pending_jtbl")}
    link = not t.get("match") and try_fn.is_link_match(t)
    if not t.get("match") and not link:
        report.update(ok=False, stage="recheck")
        print(json.dumps(report, indent=1))
        return 1
    shutil.copyfile(out_body, body)
    if a.no_integrate:
        report.update(ok=True, integrated=False)
    else:
        p = subprocess.run([sys.executable, "-B", str(HERE / "integrate.py"), "--unit", a.unit,
                            "--tu", a.tu, "--name", a.name, "--body", str(body)] + (["--skip-try"] if link else []),
                           capture_output=True, text=True, cwd=ROOT)
        try:
            r = json.loads(p.stdout)
        except json.JSONDecodeError:
            r = {"ok": False, "error": (p.stdout + p.stderr)[-800:]}
        report.update(ok=bool(r.get("matched") or r.get("already_matched")), integrated=True,
                      integrate={k: r.get(k) for k in ("ok", "matched", "error", "refused")})
    report.pop("log_tail", None) if report.get("ok") else None
    print(json.dumps(report, indent=1))
    return 0 if report.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())

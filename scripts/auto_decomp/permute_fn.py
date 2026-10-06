#!/usr/bin/env python3
"""Run decomp-permuter on a near-miss body, integrate the result if it matches.

    python3 -B scripts/auto_decomp/permute_fn.py --unit main --tu sc_get \\
        --name scWAITCNTScript --body build/auto_decomp/bodies/main/sc_get/scWAITCNTScript.c \\
        [--minutes 5] [-j N] [--stall 3] [--no-integrate]

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
new base (a fresh walk from there). Every improvement that try_fn also scores
higher is saved as <name>.best.c, so partial wins survive. On a score-0
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
    "perm_reorder_stmts": 30, "perm_reorder_decls": 25, "perm_temp_for_expr": 100,
    "perm_split_assignment": 15, "perm_commutative": 15, "perm_refer_to_var": 15,
    "perm_randomize_internal_type": 15, "perm_struct_ref": 15, "perm_ins_block": 15,
    "perm_compound_assignment": 10, "perm_duplicate_assignment": 10,
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
    return ("#!/bin/bash\n"
            "t=$(mktemp -d /dev/shm/permc.XXXXXX) || exit 1\n"
            "trap 'rm -rf \"$t\"' EXIT\n"
            'cp "$1" "$t/a.i" &&\n'
            f'"{ccdir}/ee-gcc" -B"{ccdir}/" -nostdinc -fno-builtin -S -O2 {gflag} "$t/a.i" -o "$t/a.s" &&\n'
            f'"{as_}" -EL -m5900 -mabi=eabi {gflag} {asflags} -o "$3" "$t/a.s" > "$t/m" 2>&1 &&\n'
            '! test -s "$t/m"\n')


def text_of(obj: Path, cname: str) -> str:
    p = subprocess.run([str(try_fn.OBJDUMP), "-drz", f"--disassemble={cname}", str(obj)],
                       capture_output=True, text=True)
    return "\n".join(l for l in p.stdout.splitlines() if l.startswith(" ") or l.startswith("\t"))


def setup(unit: str, tu: str, name: str, body: Path) -> tuple[Path, str]:
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
            "\n[weight_overrides]\n" + "".join(f"{k} = {v}\n" for k, v in WEIGHTS.items()))
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


def run_segment(d: Path, deadline: float, stall: float, jobs: int, on_better) -> dict:
    """One permuter walk from base.c until score 0, the deadline, or --stall
    minutes without a new best after it beat the base. on_better(score, path)
    sees every new best output and returns True to stop (try_fn matched)."""
    log = (d / "permuter.log").open("w")
    proc = subprocess.Popen([str(PY), str(PERMUTER), str(d), "-j", str(jobs), "--stop-on-zero",
                             "--best-only", "--quiet"],
                            stdout=log, stderr=subprocess.STDOUT, cwd=d,
                            start_new_session=True)
    best, last_gain = None, time.time()
    while proc.poll() is None and time.time() < deadline:
        time.sleep(5)
        sc, path = best_output(d)
        if sc is not None and (best is None or sc < best):
            best, last_gain = sc, time.time()
            if on_better(sc, path):
                best = 0
                break
        base = base_score(d)
        if sc == 0 or (base is not None and best is not None and best < base
                       and time.time() - last_gain > stall * 60):
            break
    if proc.poll() is None:
        os.killpg(proc.pid, signal.SIGINT)
        try:
            proc.wait(20)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
    log.close()
    sc, path = best_output(d)
    return {"base": base_score(d), "best": 0 if best == 0 else sc, "path": path, "rc": proc.returncode}


def run_permuter(d: Path, minutes: float, jobs: int, stall: float, on_better) -> dict:
    """Segments: a segment that beat its base and stalled restarts from its best
    output (outputs are kept under rounds/<n>/)."""
    deadline = time.time() + minutes * 60
    segs, best, best_path = [], None, None
    while time.time() < deadline - 10:
        r = run_segment(d, deadline, stall, jobs, on_better)
        segs.append({k: r[k] for k in ("base", "best")})
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
            "segments": segs, "log_tail": tail}


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
    ap.add_argument("--minutes", type=float, default=5)
    ap.add_argument("-j", type=int, default=JOBS)
    ap.add_argument("--stall", type=float, default=3,
                    help="minutes without a new best (after beating the base) before rebasing")
    ap.add_argument("--no-integrate", action="store_true")
    a = ap.parse_args()
    key = f"{a.unit}/{a.tu}/{a.name}"
    report: dict = {"key": key}
    body = a.body.resolve()
    try:
        d, cname = setup(a.unit, a.tu, a.name, body)
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
            prev = json.loads(best_side.read_text()).get("score", -1)
        except (OSError, ValueError):
            prev = -1
        try:
            cand = body.with_name(f"{a.name}.perm_cand.c")
            cand.write_text(lead + extract(path, cname))
            t = try_fn.try_function(a.unit, a.tu, a.name, cand)
        except (try_fn.TryError, OSError):
            return False
        if t.get("score") is not None and t["score"] > prev:
            shutil.copyfile(cand, body.with_name(f"{a.name}.best.c"))
            best_side.write_text(json.dumps({"score": t["score"], "differing_words": t.get("differing_words"),
                                             "target_insns": t.get("target_insns"), "by": "permuter"}) + "\n")
            report.setdefault("saved_best", []).append(t["score"])
        if t.get("match"):
            report["matched_source"] = str(cand)
            return True
        return False

    report.update(run_permuter(d, a.minutes, a.j, a.stall, on_better))
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
    report["try"] = {k: t.get(k) for k in ("match", "score", "stage", "error")}
    if not t.get("match"):
        report.update(ok=False, stage="recheck")
        print(json.dumps(report, indent=1))
        return 1
    shutil.copyfile(out_body, body)
    if a.no_integrate:
        report.update(ok=True, integrated=False)
    else:
        p = subprocess.run([sys.executable, "-B", str(HERE / "integrate.py"), "--unit", a.unit,
                            "--tu", a.tu, "--name", a.name, "--body", str(body)],
                           capture_output=True, text=True, cwd=ROOT)
        try:
            r = json.loads(p.stdout)
        except json.JSONDecodeError:
            r = {"ok": False, "error": (p.stdout + p.stderr)[-800:]}
        report.update(ok=bool(r.get("matched")), integrated=True,
                      integrate={k: r.get(k) for k in ("ok", "matched", "error", "refused")})
    report.pop("log_tail", None) if report.get("ok") else None
    print(json.dumps(report, indent=1))
    return 0 if report.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())

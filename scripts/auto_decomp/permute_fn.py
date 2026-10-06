#!/usr/bin/env python3
"""Run decomp-permuter on a near-miss body, integrate the result if it matches.

    python3 -B scripts/auto_decomp/permute_fn.py --unit main --tu sc_get \\
        --name scWAITCNTScript --body build/auto_decomp/bodies/main/sc_get/scWAITCNTScript.c \\
        [--minutes 5] [-j 4] [--no-integrate]

Builds build/auto_decomp/permute/<unit>__<tu>__<name>/ for the permuter:
  base.c     the candidate TU preprocessed, other functions reduced to
             prototypes, INCLUDE_ASM blocks dropped
  target.o   the TU with NAME as INCLUDE_ASM (original bytes)
  compile.sh try_fn.py --compile-file with the TU's exact compile command
  settings.toml  objdump restricted to NAME
The permuter mutates only NAME. On a score-0 candidate the function is
extracted, re-checked with try_fn and handed to integrate.py.
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
        (d / "compile.sh").write_text(
            "#!/bin/bash\n"
            f'exec "{sys.executable}" -B "{HERE / "try_fn.py"}" --compile-file "$1" '
            f'--unit {unit} --tu {tu} -o "$3"\n')
        (d / "compile.sh").chmod(0o755)
        (d / "settings.toml").write_text(
            f'func_name = "{cname}"\ncompiler_type = "gcc"\n'
            f'objdump_command = "{try_fn.OBJDUMP} -drz --disassemble={cname}"\n')
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


def run_permuter(d: Path, minutes: float, jobs: int) -> dict:
    log = (d / "permuter.log").open("w")
    proc = subprocess.Popen([str(PY), str(PERMUTER), str(d), "-j", str(jobs), "--stop-on-zero"],
                            stdout=log, stderr=subprocess.STDOUT, cwd=d,
                            start_new_session=True)
    deadline = time.time() + minutes * 60
    while proc.poll() is None and time.time() < deadline:
        time.sleep(5)
        sc, _ = best_output(d)
        if sc == 0:
            break
    if proc.poll() is None:
        os.killpg(proc.pid, signal.SIGINT)
        try:
            proc.wait(20)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
    log.close()
    sc, path = best_output(d)
    tail = (d / "permuter.log").read_text(errors="replace")[-1500:]
    return {"best_permuter_score": sc, "best_source": str(path) if path else None,
            "rc": proc.returncode, "log_tail": tail}


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
    ap.add_argument("-j", type=int, default=4)
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
    report.update(run_permuter(d, a.minutes, a.j))
    if report["best_permuter_score"] != 0:
        report.update(ok=False, stage="permuter")
        print(json.dumps(report, indent=1))
        return 1
    fn = extract(Path(report["best_source"]), cname)
    out_body = body.with_name(body.stem + ".permuted.c")
    out_body.write_text(leading_decls(body.read_text()) + fn)
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

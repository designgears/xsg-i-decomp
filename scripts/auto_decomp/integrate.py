#!/usr/bin/env python3
"""Integrate one C function into a TU, verify the unit still matches, or revert.

    python3 -B scripts/auto_decomp/integrate.py \\
        --unit main --tu ether_tree --name EtherTreeObjectWorkGet \\
        --body /tmp/fn.c

Safe to call from many agents at once:

1. try_fn.py first (lock-free): the function must match at object level, else
   the body is refused without touching src/ (recorded as a failed attempt with
   the try score).
2. Under the global integrate lock (build/auto_decomp/integrate.lock): check the
   unit is green before the edit, apply, rebuild, keep if whole-file identical
   else restore the exact pre-edit text and rebuild.
3. state.json is read and written only under that lock.

On success the source change is kept and state.json is updated.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from state import key, load_state, log, save_state  # noqa: E402
from state import locked as global_lock  # noqa: E402
from verify_unit import rebuild  # noqa: E402
import claims  # noqa: E402
import try_fn  # noqa: E402

BACKOFF_S = 30 * 60

# Inline asm that carries control flow or assembler directives is the original
# function pasted back, not a decompilation; integrate refuses it unless
# --allow-asm-body is given (a reviewed exception, e.g. a hand-written memcpy).
# Also refused: `.set noreorder` (hand-scheduling the assembler), asm that is
# only nop padding, empty `asm volatile("" ::: "memory")` scheduling barriers, and `register T x __asm__("$n")` register pins -- they force
# the target's bytes instead of finding the C that produces them.
ASM_BLOCK = re.compile(r"\b(?:__asm__|asm)\s*(?:__volatile__|volatile)?\s*\((.*?)\)\s*;", re.S)
ASM_FORBIDDEN = re.compile(
    r"(\.end\b|\.ent\b|\.section\b|\.mdebug|\.set\s+noreorder\b|\bjr\s+\$31|\bjr\s+\$ra|\bjalr?\b|"
    r"\bb(?:eq|ne|gez|gtz|lez|ltz|eqz|nez)l?\b|\bb\s|\bj\s)")


def run(cmd, cwd=None) -> subprocess.CompletedProcess:
    env = dict(os.environ)
    if not env.get("XENO_TOOLCHAIN_ALLOW"):
        # same accepted list as scripts/env.sh; without it ninja_main.py exits 1
        env["XENO_TOOLCHAIN_ALLOW"] = "ee-as-la29-vsqrt,ee-as-2.9-plain,ps2dev-binutils,dvp-as-3eb45ea"
    return subprocess.run(cmd, cwd=cwd or ROOT, capture_output=True, text=True, env=env)


def tu_path(root: Path, unit: str, tu: str) -> Path:
    return root / "src" / unit / f"{tu}.c"


def unit_dir(root: Path, unit: str) -> Path:
    return root / "build" / unit


def asm_body_problems(text: str) -> list[str]:
    out = []
    for m in ASM_BLOCK.finditer(text):
        # Only the string literals (template + constraints) are assembler text;
        # C operands like `"r"(j)` must not read as a `j` jump.
        tmpl = "\n".join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1)))
        for hit in ASM_FORBIDDEN.findall(tmpl):
            out.append(hit.strip())
        if re.fullmatch(r"\s*\$\w+\s*", tmpl):
            out.append(f"register pin {tmpl.strip()}")
        elif re.fullmatch(r"(?:\s*nop\s*(?:;|\\n|\\t)*)+", tmpl):
            out.append("nop padding")
        elif re.match(r'\s*""', m.group(1)) and re.search(r'"memory"', m.group(1)):
            out.append("empty asm barrier")       # `asm volatile("" ::: "memory")`: a scheduling fence
    return sorted(set(out))


def is_handwritten(root: Path, unit: str, tu: str, name: str) -> bool:
    """splat marks functions the original programmers wrote in assembly; for those,
    inline asm (branches included) is the faithful representation, not a shortcut."""
    for asm in (root / "build" / unit / "asm" / unit / "nonmatchings" / tu / f"{name}.s",
                root / "build" / unit / "asm" / "nonmatchings" / unit / tu / f"{name}.s"):
        if asm.is_file():
            return "Handwritten function" in asm.read_text(errors="replace")
    return False


def maybe_derive_carve(root: Path, unit: str, tu: str) -> dict | None:
    """If the C object grew local data, derive a carve and re-apply ninja scripts."""
    sys.path.insert(0, str(root / "tools/tu"))
    from elfinfo import Elf  # type: ignore

    if unit == "main":
        obj = root / "build/main/build/c" / f"{tu}.o"
        tu_id = None
        man = json.loads((root / "build/main/tu-manifest.json").read_text())
        for t in man["tus"]:
            if t["name"] == tu:
                tu_id = t["id"]
                break
        source = root / "src/main" / f"{tu}.c"
    else:
        obj = root / f"build/{unit}/build/scaffold/src/{unit}/{tu}.o"
        if not obj.is_file():
            obj = root / f"build/{unit}/build/src/{unit}/{tu}.o"
        man = json.loads((root / f"build/{unit}/compile-manifest.json").read_text())
        tu_id = next((t["id"] for t in man["units"][unit]["tus"] if t["name"] == tu), None)
        source = root / "src" / unit / f"{tu}.c"
    if not obj.is_file() or tu_id is None:
        return None
    elf = Elf(obj.read_bytes())
    secs = {s.name: s.size for s in elf.sections}
    if not any(secs.get(s, 0) for s in (".rodata", ".lit4", ".sdata")):
        return None
    cmd = [sys.executable, "-B", str(root / "tools/tu/data_carve.py"), "derive",
           "--root", str(root), "--unit", unit, "--unit-dir", str(unit_dir(root, unit)),
           "--tu", tu_id, "--source", str(source), "--obj", str(obj), "--write"]
    proc = run(cmd)
    result = {"derive_rc": proc.returncode, "stdout_tail": (proc.stdout or "")[-2000:]}
    if proc.returncode != 0:
        return result
    _refresh_scripts(root, unit)
    result["scripts_refreshed"] = True
    return result


def _refresh_scripts(root: Path, unit: str) -> None:
    """Re-apply the unit's linker scripts so data-carve changes take effect."""
    if unit == "main":
        p = run([sys.executable, "-B", str(root / "tools/tu/ninja_main.py"),
             "--root", str(root), "--unit-dir", str(unit_dir(root, unit))])
    else:
        p = run([sys.executable, "-B", str(root / "tools/tu/ninja_ovl.py"),
             str(unit_dir(root, unit)), unit,
             "--manifest", str(unit_dir(root, unit) / "compile-manifest.json"),
             "--root", str(root)])
    if p.returncode != 0:
        print(f"integrate: linker-script refresh failed (rc={p.returncode}): "
              f"{(p.stderr or p.stdout)[-800:]}", file=sys.stderr)


def _green(verify: dict, unit: str) -> bool:
    return bool(verify.get("identical")) and (unit != "main" or verify.get("mapcheck_ok", True))


MIN_TRIES_BEFORE_GIVEUP = 40


def integrate(root: Path, unit: str, tu: str, name: str, body: Path,
              max_attempts_before_skip: int = 5, skip_try: bool = False,
              allow_asm_body: bool = False, give_up: bool = False) -> dict:
    src = tu_path(root, unit, tu)
    if not src.is_file():
        return {"ok": False, "error": f"missing source {src}"}
    if not body.is_file():
        return {"ok": False, "error": f"missing body {body}"}
    k = key(unit, tu, name)
    report: dict = {"key": k, "unit": unit, "tu": tu, "name": name}

    if not allow_asm_body and not is_handwritten(root, unit, tu, name):
        problems = asm_body_problems(body.read_text(errors="replace"))
        if problems:
            report.update(ok=False, refused="asm_body", asm_problems=problems,
                          error="inline asm contains control flow / directives "
                                f"{problems}: write C, not the original assembly")
            return report

    # Object-level precheck: lock-free, does not touch src/.
    if not skip_try:
        try:
            t = try_fn.try_function(unit, tu, name, body.resolve(), diff_limit=60)
        except try_fn.TryError as e:
            t = {"ok": False, "match": False, "stage": "setup", "error": str(e)}
        report["try"] = {x: t.get(x) for x in ("ok", "match", "stage", "score", "opcode_score",
                                               "differing_words", "new_data_sections", "error",
                                               "link_check", "match_after_link", "false_lo16")}
        if not t.get("match") and (t.get("link_check") or t.get("match_after_link")) and not give_up:
            report["link_check"] = True        # absolute-vs-symbol only: let the gate decide
        elif not t.get("match"):
            report["try_diff"] = t.get("diff")
            if give_up:
                # Try hard: no give-up until the agent has spent most of its budget.
                need = int(os.environ.get("MIN_TRIES_BEFORE_GIVEUP", MIN_TRIES_BEFORE_GIVEUP))
                cnt = try_fn.TRIES_DIR / f"{unit}__{tu}__{name}"
                used = int(cnt.read_text()) if cnt.is_file() else 0
                if used < need:
                    report.pop("try_diff", None)
                    return {**report, "ok": False, "refused": "give_up_too_early",
                            "error": f"only {used}/{need} try_fn runs used: keep iterating "
                                     "(fix the first TARGET|CANDIDATE divergence, try the pitfalls "
                                     "list) before --give-up"}
            # A precheck refusal never touched src/: keep the best score for the
            # next agent; only an explicit --give-up burns an attempt + backoff.
            with global_lock():
                state = load_state()
                if give_up:
                    side = body.with_name(body.stem + ".best.json")
                    try:
                        best = json.loads(side.read_text()).get("score", -1)
                    except (OSError, ValueError):
                        best = -1
                    if best > (t.get("score") or 0):      # a worse last edit: restore the best body
                        shutil.copyfile(body.with_name(body.stem + ".best.c"), body)
                        t["score"] = best
                        report["try"]["score"] = best
                        report["restored_best"] = True
                    _keep_best(state["failed"].setdefault(k, {"attempts": 0}), t.get("score"), body)
                    _record_fail(state, k, f"gave up: {t.get('stage') or 'mismatch'}", report,
                                 score=t.get("score"))
                    if t.get("opcode_score") is not None:
                        state["failed"][k]["best_opcode_score"] = max(
                            t["opcode_score"], state["failed"][k].get("best_opcode_score") or 0)
                        save_state(state)
                    log(f"GIVEUP {k} score={t.get('score')}")
                    return {**report, "ok": False, "reverted": False, "gave_up": True}
                entry = state["failed"].setdefault(k, {"attempts": 0})
                _keep_best(entry, t.get("score"), body)
                if t.get("score") is not None:
                    entry["best_score"] = max(t["score"], entry.get("best_score") or 0)
                entry["last_error"] = f"try: {t.get('stage') or 'mismatch'}"
                if report.get("try_diff"):
                    entry["last_try_diff"] = report["try_diff"][:40]
                save_state(state)
            return {**report, "ok": False, "reverted": False}

    with global_lock():
        state = load_state()
        if k in state["matched"] and "INCLUDE_ASM" not in _block_text(root, unit, tu, name):
            return {"ok": True, "already_matched": True, "key": k}
        failed = state["failed"].get(k) or {}
        if failed.get("attempts", 0) >= max_attempts_before_skip:
            return {"ok": False, "skipped": True, "key": k, "failed": failed}

        before = rebuild(root, unit)
        if not _green(before, unit):
            # Someone left the unit red; refuse rather than blame this body.
            log(f"ABORT {k}: unit {unit} not green before integrate")
            return {**report, "ok": False, "error": f"unit {unit} not green before integrate",
                    "verify_before": before}

        original = src.read_text()
        # Edit via a temp file + rename so lock-free readers (try_fn) never see a
        # half-written TU.
        staged = src.with_name(f".{src.name}.{os.getpid()}.tmp")
        cmd = [sys.executable, "-B", str(root / "tools/tu_edit.py"), "to-c",
               str(src), name, "--body", str(body), "-o", str(staged), "--json",
               "--unit-dir", str(unit_dir(root, unit))]
        proc = run(cmd)
        report["tu_edit_rc"] = proc.returncode
        report["tu_edit"] = (proc.stdout or proc.stderr)[-2000:]
        if proc.returncode != 0 or not staged.is_file():
            staged.unlink(missing_ok=True)
            _record_fail(state, k, "tu_edit failed", report)
            return {**report, "ok": False, "reverted": True}
        os.replace(staged, src)

        verify = rebuild(root, unit)
        report["verify"] = verify
        carves = root / "config/tu/data-carves.json"
        carves_before = carves.read_bytes() if carves.is_file() else None
        if not verify.get("identical"):
            carve = maybe_derive_carve(root, unit, tu)
            report["carve"] = carve
            if carve and carve.get("scripts_refreshed"):
                verify = rebuild(root, unit)
                report["verify_after_carve"] = verify

        if _green(verify, unit):
            if k not in state["matched"]:
                state["matched"].append(k)
            state["matched_count"] = len(state["matched"])
            state["attempts"] = state.get("attempts", 0) + 1
            state["failed"].pop(k, None)
            state["last_success"] = {"key": k, "at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
            save_state(state)
            log(f"MATCH {k}")
            if os.environ.get("AUTO_RETRY") != "0":   # free re-tries of unblocked near-misses
                subprocess.Popen([sys.executable, "-B", str(Path(__file__).with_name("retry_best.py")),
                                  "--after", k], cwd=root, start_new_session=True,
                                 stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                 stderr=subprocess.DEVNULL)
            return {**report, "ok": True, "matched": True}

        _atomic_write(src, original)
        # A derive above may have rewritten the data-carve manifest: a failed
        # attempt must not leave it behind (it turned main red on 2026-10-02).
        now = carves.read_bytes() if carves.is_file() else None
        if now != carves_before:
            if carves_before is None:
                carves.unlink(missing_ok=True)
            else:
                carves.write_bytes(carves_before)
            _refresh_scripts(root, unit)
            report["carves_restored"] = True
        restored = rebuild(root, unit)
        report["restored_green"] = _green(restored, unit)
        _record_fail(state, k, "unit no longer identical after integrate", report)
        log(f"FAIL {k}: not identical (restored_green={report['restored_green']})")
        return {**report, "ok": False, "reverted": True}


def _atomic_write(path: Path, text: str) -> None:
    tmp = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    tmp.write_text(text)
    os.replace(tmp, path)


def _block_text(root: Path, unit: str, tu: str, name: str) -> str:
    p = run([sys.executable, "-B", str(root / "tools/tu_edit.py"), "extract",
             str(tu_path(root, unit, tu)), name])
    return p.stdout if p.returncode == 0 else "INCLUDE_ASM"


def _keep_best(entry: dict, score, body: Path | None) -> None:
    """Remember the best-scoring body next to the working one (<name>.best.c)."""
    if score is None or body is None or not body.is_file():
        return
    if score > (entry.get("best_score") or 0) or not body.with_name(body.stem + ".best.c").is_file():
        if score >= (entry.get("best_score") or 0):
            shutil.copyfile(body, body.with_name(body.stem + ".best.c"))


def _record_fail(state: dict, k: str, reason: str, report: dict, score=None) -> None:
    """Caller holds the global lock."""
    entry = state["failed"].setdefault(k, {"attempts": 0})
    entry["attempts"] += 1
    entry["last_error"] = reason
    entry["last_at"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    entry["next_eligible"] = time.time() + BACKOFF_S * entry["attempts"]
    if score is not None:
        entry["best_score"] = max(score, entry.get("best_score") or 0)
    entry["last_diffs"] = (report.get("verify") or report.get("verify_after_carve") or {}).get("diffs")
    if report.get("try_diff"):
        entry["last_try_diff"] = report["try_diff"][:40]
    state["attempts"] = state.get("attempts", 0) + 1
    save_state(state)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--unit", required=True)
    ap.add_argument("--tu", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--body", type=Path, required=True)
    ap.add_argument("--skip-try", action="store_true", help="skip the object-level precheck")
    ap.add_argument("--allow-asm-body", action="store_true",
                    help="accept inline asm with control flow/directives (reviewed exception)")
    ap.add_argument("--keep-claim", action="store_true", help="do not release the claim afterwards")
    ap.add_argument("--give-up", action="store_true",
                    help="no match found: record the attempt (backoff) with this best body")
    ap.add_argument("--json-out", type=Path)
    a = ap.parse_args()
    report = None
    try:
        report = integrate(a.root, a.unit, a.tu, a.name, a.body,
                           skip_try=a.skip_try, allow_asm_body=a.allow_asm_body,
                           give_up=a.give_up)
    except BaseException:
        report = None
        raise
    finally:
        # A precheck refusal (body not matching yet, src/ untouched) keeps the
        # lease: the agent is still iterating. Matches, give-ups and real
        # attempts free it.
        refused_early = (report is not None and not report.get("ok") and "try" in report
                         and not report.get("gave_up") and "verify" not in report)
        if not a.keep_claim and not refused_early:
            claims.release(key(a.unit, a.tu, a.name))
    text = json.dumps(report, indent=1) + "\n"
    if a.json_out:
        a.json_out.parent.mkdir(parents=True, exist_ok=True)
        a.json_out.write_text(text)
    sys.stdout.write(text)
    return 0 if report.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())

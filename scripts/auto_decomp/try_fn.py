#!/usr/bin/env python3
"""Lock-free, function-level match check for one candidate C body.

    python3 -B scripts/auto_decomp/try_fn.py \\
        --unit main --tu menu_tec --name MenuTecNextSpeedPointGet --body fn.c

Builds two objects of the TU in a private scratch directory under
build/<unit>/try/ with the exact compile command of the unit's build.ninja:

  base: the TU with NAME as INCLUDE_ASM (the original bytes)
  cand: the TU with NAME replaced by --body

and compares NAME's .text bytes and relocations between them. Nothing under
src/ or the unit's real build outputs is touched, so any number of agents can
run this at once. Iterate here until "match": true, then call integrate.py
once for the whole-file gate.

Prints a JSON report; with --diff (default on) also prints a side-by-side
instruction diff to stderr. Exit 0 = match, 1 = mismatch, 2 = error.
"""
from __future__ import annotations

import argparse
import difflib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tu"))
from elfinfo import Elf  # noqa: E402

TOOLCHAIN = Path(os.environ.get("XENO_TOOLCHAIN_DIR", ROOT / ".work/toolchains"))
OBJDUMP = TOOLCHAIN / "ee-binutils/bin/mips64r5900el-ps2-elf-objdump"


class TryError(Exception):
    pass


# ---------------------------------------------------------------- ninja ----

def _logical_lines(text: str):
    buf = ""
    for line in text.splitlines():
        if line.endswith("$") and not line.endswith("$$"):
            buf += line[:-1]
            continue
        yield buf + line
        buf = ""
    if buf:
        yield buf


def _split_unescaped(s: str, sep: str) -> list[str]:
    out, cur, i = [], "", 0
    while i < len(s):
        c = s[i]
        if c == "$" and i + 1 < len(s):
            cur += s[i:i + 2]
            i += 2
            continue
        if c == sep:
            out.append(cur)
            cur = ""
        else:
            cur += c
        i += 1
    out.append(cur)
    return out


def _unescape_path(p: str) -> str:
    return p.replace("$ ", " ").replace("$:", ":").replace("$$", "$")


def parse_ninja(path: Path):
    """(globals, rules, edges); edge = {outs, rule, ins, vars}."""
    globals_, rules, edges = {}, {}, []
    cur = None
    for line in _logical_lines(path.read_text()):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if line[0] in " \t":
            if cur is not None and "=" in line:
                k, v = line.strip().split("=", 1)
                cur[k.strip()] = v.strip()
            continue
        if line.startswith("rule "):
            cur = rules.setdefault(line[5:].strip(), {})
        elif line.startswith("build "):
            head, tail = _split_unescaped(line[6:], ":")[0], ":".join(_split_unescaped(line[6:], ":")[1:])
            parts = tail.split()
            rule, rest = parts[0], " ".join(parts[1:])
            explicit = rest.split("|")[0].split()
            cur = {}
            edges.append({"outs": [_unescape_path(o) for o in head.split()],
                          "rule": rule,
                          "ins": [_unescape_path(i) for i in explicit],
                          "vars": cur})
        elif "=" in line:
            k, v = line.split("=", 1)
            globals_[k.strip()] = v.strip()
            cur = None
        else:
            cur = None
    return globals_, rules, edges


_VAR = re.compile(r"\$(\{([A-Za-z0-9_.-]+)\}|([A-Za-z0-9_-]+)|\$|:| |\n)")


def expand(value: str, scope: dict) -> str:
    def sub(m):
        tok = m.group(1)
        if tok in ("$", ":", " "):
            return tok
        name = m.group(2) or m.group(3)
        return scope.get(name, "")
    return _VAR.sub(sub, value)


def compile_edge(unit: str, tu: str):
    unit_dir = ROOT / "build" / unit
    g, rules, edges = parse_ninja(unit_dir / "build.ninja")
    want = (ROOT / "src" / unit / f"{tu}.c").resolve()
    for e in edges:
        if e["rule"] in rules and len(e["ins"]) == 1 and (unit_dir / e["ins"][0]).resolve() == want:
            return unit_dir, g, rules[e["rule"]], e
    raise TryError(f"no compile edge for src/{unit}/{tu}.c in {unit_dir / 'build.ninja'}")


def run_compile(unit_dir: Path, g: dict, rule: dict, edge: dict, src: Path, out_rel: str,
                overlay_rel: str) -> tuple[bool, str]:
    scope = dict(g)
    scope.update({k: expand(v, g) for k, v in edge["vars"].items()})
    if "overlay" in scope:
        scope["overlay"] = overlay_rel   # main: cinc.py writes the filtered asm here
    scope["in"] = str(src)
    scope["out"] = out_rel
    cmd = expand(rule["command"], scope)
    proc = subprocess.run(["/bin/sh", "-c", cmd], cwd=unit_dir, capture_output=True, text=True)
    ok = proc.returncode == 0 and (unit_dir / out_rel).is_file()
    return ok, (proc.stdout + proc.stderr)


# ------------------------------------------------------------------ elf ----

def _relocs(elf: Elf, text_idx: int):
    out = []
    for s in elf.sections:
        if s.type not in (9, 4) or s.info != text_idx:   # SHT_REL / SHT_RELA
            continue
        ent = 8 if s.type == 9 else 12
        syms = {sym.index: sym for sym in elf.symbols}
        for i in range(s.size // ent):
            off, info = struct.unpack_from("<II", elf.data, s.offset + i * ent)
            sym = syms.get(info >> 8)
            name = sym.name if sym else ""
            if sym and not name and sym.shndx < len(elf.sections):
                name = elf.sections[sym.shndx].name
            out.append((off, info & 0xFF, name))
    return out


def function_slice(obj: Path, name: str) -> dict:
    elf = Elf(obj.read_bytes())
    cands = [s for s in elf.symbols if s.name == name and 0 < s.shndx < len(elf.sections)]
    if not cands:
        alt = re.compile(re.escape(name) + r"_[0-9A-Fa-f]{8}$")
        cands = [s for s in elf.symbols if alt.match(s.name) and 0 < s.shndx < len(elf.sections)]
    if not cands:
        m = re.match(r"(.+)_[0-9A-Fa-f]{8}$", name)
        if m:
            cands = [s for s in elf.symbols if s.name == m.group(1) and 0 < s.shndx < len(elf.sections)]
    if not cands:
        raise TryError(f"symbol {name} not in {obj.name}")
    sym = cands[0]
    sec = elf.sections[sym.shndx]
    start = sym.value
    if sym.size:
        end = start + sym.size
    else:
        nexts = sorted(s.value for s in elf.symbols
                       if s.shndx == sym.shndx and s.value > start and s.name
                       and not s.name.startswith((".L", "$L", "$")) and s.type in (0, 2))
        end = nexts[0] if nexts else sec.size
    data = elf.data[sec.offset + start:sec.offset + end]
    while len(data) >= 4 and data[-4:] == b"\0\0\0\0":   # alignment padding
        data = data[:-4]
    end = start + len(data)
    relocs = [(o - start, t, n) for o, t, n in _relocs(elf, sym.shndx) if start <= o < end]
    data_secs = {s.name: s.size for s in elf.sections
                 if s.name in (".rodata", ".lit4", ".lit8", ".sdata", ".data") and s.size}
    return {"start": start, "end": end, "bytes": data, "relocs": sorted(relocs),
            "section": sec.name, "data_sections": data_secs}


def disasm(obj: Path, sl: dict) -> list[str]:
    if not OBJDUMP.is_file():
        return [f"{i * 4:4x}: {sl['bytes'][i * 4:i * 4 + 4][::-1].hex()}"
                for i in range(len(sl["bytes"]) // 4)]
    proc = subprocess.run([str(OBJDUMP), "-d", "-r", "-j", sl["section"],
                           f"--start-address={sl['start']}", f"--stop-address={sl['end']}",
                           str(obj)], capture_output=True, text=True)
    lines, by_off = [], {}
    ins = re.compile(r"^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(.*)$")
    rel = re.compile(r"^\s*([0-9a-f]+):\s+(R_\S+)\s+(\S+)")
    for raw in proc.stdout.splitlines():
        m = ins.match(raw)
        if m:
            off = int(m.group(1), 16) - sl["start"]
            text = re.sub(r"\s+", " ", m.group(3).split("<")[0]).strip()
            by_off[off] = len(lines)
            lines.append(f"{off:4x}: {text}")
            continue
        m = rel.match(raw)
        if m:
            off = int(m.group(1), 16) - sl["start"]
            if off in by_off:
                lines[by_off[off]] += f"  [{m.group(2).replace('R_MIPS_', '')} {m.group(3)}]"
    return lines


def side_by_side(a: list[str], b: list[str], limit: int) -> list[str]:
    strip = lambda s: s.split(":", 1)[1].strip() if ":" in s else s  # noqa: E731
    sm = difflib.SequenceMatcher(None, [strip(x) for x in a], [strip(x) for x in b], autojunk=False)
    rows = []
    w = max([len(x) for x in a] + [20])
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        n = max(i2 - i1, j2 - j1)
        for k in range(n):
            l = a[i1 + k] if i1 + k < i2 else ""
            r = b[j1 + k] if j1 + k < j2 else ""
            mark = " " if op == "equal" else ("|" if l and r else ("<" if l else ">"))
            rows.append(f"{l:<{w}} {mark} {r}")
    if len(rows) > limit:
        rows = rows[:limit] + [f"... ({len(rows) - limit} more rows)"]
    return ["TARGET".ljust(w) + "   CANDIDATE"] + rows


# ----------------------------------------------------------------- main ----

def tu_edit(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, "-B", str(ROOT / "tools/tu_edit.py"), *args],
                          capture_output=True, text=True, cwd=ROOT)


def make_src_dir(scratch: Path, unit: str, tu: str, text: str) -> Path:
    """A src/<unit> look-alike: symlinks to every sibling, own copy of the TU,
    so quoted includes resolve exactly as for the real file."""
    real = ROOT / "src" / unit
    d = scratch / "src"
    d.mkdir(parents=True, exist_ok=True)
    for p in real.iterdir():
        if p.name != f"{tu}.c":
            (d / p.name).symlink_to(p.resolve())
    (d / f"{tu}.c").write_text(text)
    return d / f"{tu}.c"


def try_function(unit: str, tu: str, name: str, body: Path, keep: bool = False,
                 diff_limit: int = 200) -> dict:
    src = ROOT / "src" / unit / f"{tu}.c"
    if not src.is_file():
        raise TryError(f"missing {src}")
    unit_dir, g, rule, edge = compile_edge(unit, tu)
    scratch_rel = Path("try") / f"{tu}.{name}.{os.getpid()}"
    scratch = unit_dir / scratch_rel
    shutil.rmtree(scratch, ignore_errors=True)
    scratch.mkdir(parents=True)
    report: dict = {"key": f"{unit}/{tu}/{name}", "scratch": str(scratch)}
    try:
        cur = src.read_text()
        # base: NAME as INCLUDE_ASM (revert it if the current TU already has C)
        lst = tu_edit("list", str(src), "--json", "--unit-dir", str(unit_dir))
        base_text = cur
        try:
            blocks = json.loads(lst.stdout)
            blocks = blocks.get("functions", blocks) if isinstance(blocks, dict) else blocks
            state = next((b.get("state") for b in blocks
                          if isinstance(b, dict) and name in (b.get("name"), b.get("key"))), None)
        except (json.JSONDecodeError, AttributeError):
            state = None
        if state == "c":
            p = tu_edit("to-asm", str(src), name, "--unit-dir", str(unit_dir))
            if p.returncode != 0:
                raise TryError(f"tu_edit to-asm failed: {(p.stdout + p.stderr)[-1500:]}")
            base_text = p.stdout
        p = tu_edit("to-c", str(src), name, "--body", str(body), "--unit-dir", str(unit_dir))
        if p.returncode != 0:
            report.update(ok=False, match=False, stage="tu_edit",
                          error=(p.stdout + p.stderr)[-3000:])
            return report
        cand_text = p.stdout

        results = {}
        for tag, text in (("base", base_text), ("cand", cand_text)):
            tdir = scratch / tag
            src_copy = make_src_dir(tdir, unit, tu, text)
            out_rel = str(scratch_rel / tag / f"{tu}.o")
            ok, log = run_compile(unit_dir, g, rule, edge, src_copy, out_rel,
                                  str(scratch_rel / tag / "cinc"))
            if not ok:
                report.update(ok=False, match=False, stage=f"compile_{tag}", error=log[-3000:])
                return report
            results[tag] = unit_dir / out_rel

        base = function_slice(results["base"], name)
        cand = function_slice(results["cand"], name)
        bytes_match = base["bytes"] == cand["bytes"]
        relocs_match = [(o, t) for o, t, _ in base["relocs"]] == [(o, t) for o, t, _ in cand["relocs"]]
        sym_diffs = [{"off": hex(a[0]), "target": a[2], "candidate": b[2]}
                     for a, b in zip(base["relocs"], cand["relocs"])
                     if a[:2] == b[:2] and a[2] != b[2]]
        words = lambda b: [b[i:i + 4] for i in range(0, len(b), 4)]  # noqa: E731
        wa, wb = words(base["bytes"]), words(cand["bytes"])
        n = len(wa)
        sm = difflib.SequenceMatcher(None, wa, wb, autojunk=False)
        diff_words = max(n, len(wb)) - sum(b.size for b in sm.get_matching_blocks())
        # Words that differ only because the candidate carries a HI16/LO16
        # relocation where the target has an absolute address (scaffold asm of
        # an overlay addressing a main symbol by number): only the linked image
        # can tell, so flag them for integrate's whole-file gate.
        cand_rel_offs = {o for o, t, _ in cand["relocs"] if t in (5, 6)}   # R_MIPS_HI16/LO16
        base_rel_offs = {o for o, _, _ in base["relocs"]}
        link_check = False
        if not bytes_match and len(wa) == len(wb):
            diff_offs = [i * 4 for i in range(len(wa)) if wa[i] != wb[i]]
            def same_op(i):
                a = int.from_bytes(wa[i], "little"); b = int.from_bytes(wb[i], "little")
                return (a >> 16) == (b >> 16)          # opcode + rs + rt equal; imm differs
            # Also: both sides relocate the slot but against different symbols
            # (an address alias like D_00942228 vs asPacketSource+0x10) — the
            # REL addend in the immediate differs, the linked address may not.
            alias_offs = {int(d["off"], 16) for d in sym_diffs}
            link_check = bool(diff_offs) and all(
                ((o in cand_rel_offs and o not in base_rel_offs) or o in alias_offs) and same_op(o // 4)
                for o in diff_offs)
        report.update(
            ok=True,
            match=bytes_match and relocs_match,
            link_check=link_check,
            bytes_match=bytes_match,
            relocs_match=relocs_match,
            reloc_symbol_diffs=sym_diffs[:20],
            target_insns=n,
            candidate_insns=len(wb),
            differing_words=diff_words,
            score=round(sm.ratio(), 4),
            new_data_sections={k: v - base["data_sections"].get(k, 0)
                               for k, v in cand["data_sections"].items()
                               if v != base["data_sections"].get(k, 0)},
        )
        if not report["match"]:
            da, db = disasm(results["base"], base), disasm(results["cand"], cand)
            # Opcode-only similarity: a register-allocation miss scores high here
            # while the strict word score drops to ~0 (every word differs).
            op = lambda ls: [l.split(":", 1)[1].split()[0] if ":" in l and l.split(":", 1)[1].split() else ""
                             for l in ls]  # noqa: E731
            report["opcode_score"] = round(difflib.SequenceMatcher(None, op(da), op(db), autojunk=False).ratio(), 4)
            report["diff"] = side_by_side(da, db, diff_limit)
        if sym_diffs:
            report["note"] = ("bytes/relocation types match but some relocations name different "
                              "symbols; integrate's whole-file gate decides (usually fine when "
                              "the names alias the same address)")
        return report
    finally:
        if not keep:
            shutil.rmtree(scratch, ignore_errors=True)


def compile_file(unit: str, tu: str, c_path: Path, out: Path) -> tuple[bool, str]:
    """Compile an arbitrary (possibly preprocessed) C file with TU's exact
    compile command into `out` (used as the permuter's compile.sh)."""
    unit_dir, g, rule, edge = compile_edge(unit, tu)
    scratch_rel = Path("try") / f"cf.{tu}.{os.getpid()}"
    scratch = unit_dir / scratch_rel
    shutil.rmtree(scratch, ignore_errors=True)
    scratch.mkdir(parents=True)
    try:
        src = scratch / f"{tu}.c"
        shutil.copyfile(c_path, src)
        out_rel = str(scratch_rel / f"{tu}.o")
        ok, log = run_compile(unit_dir, g, rule, edge, src, out_rel, str(scratch_rel / "cinc"))
        if ok:
            shutil.copyfile(unit_dir / out_rel, out)
        return ok, log
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


TRY_BUDGET = 60


def _track_best(body: Path, score) -> None:
    """Keep the best-scoring body of a function as <name>.best.c (+ .best.json
    with its score) so a later worse edit can't lose it; integrate --give-up
    restores it."""
    if score is None or not body.is_file() or "auto_decomp/bodies" not in str(body):
        return
    stem = body.stem[:-5] if body.stem.endswith(".best") else body.stem
    if stem != body.stem:
        return
    side = body.with_name(stem + ".best.json")
    try:
        prev = json.loads(side.read_text()).get("score", -1)
    except (OSError, ValueError):
        prev = -1
    if score > prev:
        shutil.copyfile(body, body.with_name(stem + ".best.c"))
        side.write_text(json.dumps({"score": score}) + "\n")
TRIES_DIR = Path(__file__).resolve().parents[2] / "build/auto_decomp/tries"


def main() -> int:
    if len(sys.argv) > 1 and sys.argv[1] == "--compile-file":
        cp = argparse.ArgumentParser()
        cp.add_argument("--compile-file", type=Path, required=True)
        cp.add_argument("--unit", required=True)
        cp.add_argument("--tu", required=True)
        cp.add_argument("-o", type=Path, required=True)
        c = cp.parse_args()
        ok, log = compile_file(c.unit, c.tu, c.compile_file.resolve(), c.o.resolve())
        if not ok:
            sys.stderr.write(log[-3000:])
        return 0 if ok else 1
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", required=True)
    ap.add_argument("--tu", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--body", type=Path, required=True)
    ap.add_argument("--keep", action="store_true", help="keep the scratch directory")
    ap.add_argument("--diff-limit", type=int, default=200)
    ap.add_argument("--no-diff", action="store_true", help="omit the diff from stderr")
    ap.add_argument("--json-out", type=Path)
    a = ap.parse_args()
    # Per-lease budget: agents get TRY_BUDGET runs per function (next.py --claim
    # resets the counter). Brute-force sweeps belong to the permuter stage.
    budget = int(os.environ.get("TRY_FN_BUDGET", TRY_BUDGET))
    cnt = TRIES_DIR / f"{a.unit}__{a.tu}__{a.name}"
    n = int(cnt.read_text()) if cnt.is_file() else 0
    if budget > 0 and n >= budget:
        print(json.dumps({"ok": False, "match": False, "stage": "budget",
                          "error": f"try budget exhausted ({n}/{budget} runs for this lease): "
                                   "run integrate.py --give-up with your best body"}, indent=1))
        return 3
    TRIES_DIR.mkdir(parents=True, exist_ok=True)
    cnt.write_text(str(n + 1))
    try:
        report = try_function(a.unit, a.tu, a.name, a.body.resolve(), a.keep, a.diff_limit)
    except TryError as e:
        report = {"ok": False, "match": False, "stage": "setup", "error": str(e)}
    _track_best(a.body.resolve(), report.get("score"))
    diff = report.pop("diff", None)
    if diff and not a.no_diff:
        sys.stderr.write("\n".join(diff) + "\n")
    if diff:
        report["diff_rows"] = len(diff) - 1
    text = json.dumps(report, indent=1) + "\n"
    if a.json_out:
        a.json_out.parent.mkdir(parents=True, exist_ok=True)
        a.json_out.write_text(json.dumps({**report, "diff": diff}, indent=1) + "\n")
    sys.stdout.write(text)
    if not report.get("ok"):
        if report.get("stage") == "setup":
            return 2
        return 1
    return 0 if report["match"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

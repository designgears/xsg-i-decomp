#!/usr/bin/env python3
"""First draft of a function's C from its splat asm with m2c.

    python3 -B scripts/auto_decomp/m2c_seed.py --unit main --tu sdv --name sdvKillAlter [--body PATH]

m2c (MIPS -> C, target mipsee-gcc-c) translates build/<unit>/asm/.../<name>.s,
using the TU's preprocessed source (the .i the build already writes) as
context so globals, structs and prototypes come out typed. The draft is
written to <body>.m2c.c; with --seed it also becomes the body when no body
exists yet (an earlier attempt's body is never overwritten). The draft is a
starting point: it is not expected to compile or match as is.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import try_fn  # noqa: E402

M2C = ROOT / ".venv/bin/m2c"
TARGET = "mipsee-gcc-c"


def context_file(unit: str, tu: str) -> Path | None:
    """The preprocessed TU from the last build, if any."""
    try:
        unit_dir, _g, _rule, edge = try_fn.compile_edge(unit, tu)
    except try_fn.TryError:
        return None
    out = unit_dir / edge["outs"][0]
    for p in (Path(str(out) + ".i"), out.with_suffix(".i")):
        if p.is_file():
            return p
    return None


_TOPLEVEL_ASM = re.compile(r"^\s*__asm__\s*\(.*?\);\s*$", re.M | re.S)
_ATTR = re.compile(r"__attribute__\s*\(\((?:[^()]|\((?:[^()]|\([^()]*\))*\))*\)\)")


def strip_bodies(text: str) -> str:
    """Replace every top-level function body with `;` (a prototype): bodies hold
    inline asm with operand lists the m2c context parser (pycparser) rejects,
    and m2c only needs the declarations."""
    out, i, n, depth = [], 0, len(text), 0
    last = ""          # last significant char seen at depth 0
    while i < n:
        ch = text[i]
        if ch in "\"'":                             # skip string / char literals
            j = i + 1
            while j < n and text[j] != ch:
                j += 2 if text[j] == "\\" else 1
            if depth == 0:
                out.append(text[i:j + 1])
            i = j + 1
            continue
        if depth == 0 and ch == "{" and last == ")":  # function definition body
            d, j = 1, i + 1
            while j < n and d:
                c = text[j]
                if c in "\"'":
                    k = j + 1
                    while k < n and text[k] != c:
                        k += 2 if text[k] == "\\" else 1
                    j = k + 1
                    continue
                d += (c == "{") - (c == "}")
                j += 1
            out.append(";")
            i, last = j, ";"
            continue
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
        out.append(ch)
        if not ch.isspace():
            last = ch
        i += 1
    return "".join(out)


def sanitize(text: str) -> str:
    """Declarations only, without the GCC-isms pycparser rejects."""
    text = "\n".join(l for l in text.splitlines() if not l.startswith("#"))
    text = _TOPLEVEL_ASM.sub("", text)
    text = _ATTR.sub("", text)
    text = re.sub(r"\b__inline__\b|\b__inline\b|\binline\b", "", text)
    text = re.sub(r"\b__extension__\b", "", text)
    return strip_bodies(text)


_JTBL_WORD = re.compile(r"\.word 0x([0-9A-Fa-f]{8})")
DATA_KINDS = ("rodata", "lit4", "lit8", "sdata", "data")


def data_files(unit: str, tu: str, scratch: Path, labels: set[str]) -> list[Path]:
    """The TU's splat data files (strings, float literals, jump tables) so m2c
    can inline literals and resolve switch jump tables. Jump-table entries are
    raw addresses in splat output; m2c needs them as the `.L<addr>` labels.
    Only addresses that are labels of the function are rewritten: splat's
    table bounds can include trailing padding/data words."""
    out = []
    for kind in DATA_KINDS:
        for f in (ROOT / "build" / unit / "asm").rglob(f"{tu}.{kind}.s"):
            if "nonmatchings" in f.parts or f.stat().st_size > 512 * 1024:
                continue
            lines, in_jtbl = [], False
            for l in f.read_text(errors="replace").splitlines():
                if l.startswith("dlabel jtbl_"):
                    in_jtbl = True
                elif l.startswith("enddlabel"):
                    in_jtbl = False
                elif in_jtbl:
                    l = _JTBL_WORD.sub(lambda m: ".word .L" + m.group(1).upper()
                                       if ".L" + m.group(1).upper() in labels else m.group(0), l)
                lines.append(l)
            dst = scratch / f"{unit}__{f.name}.{os.getpid()}"
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_text("\n".join(lines) + "\n")
            out.append(dst)
    return out


def run_m2c(asm: Path, ctx: Path | None, extra: list[Path] = (), timeout: int = 60) -> subprocess.CompletedProcess:
    cmd = [str(M2C), "-t", TARGET]
    if ctx:
        cmd += ["--context", str(ctx)]
    cmd += [str(asm), *map(str, extra)]
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)


def _failed(out: str) -> bool:
    return "Decompilation failure in function" in out or "Internal error in function" in out


PROTOS = ROOT / "build/auto_decomp/protos.json"      # proto_index.py
_CALL = re.compile(r"\b(?:jal|j)\s+([A-Za-z_]\w*)\s*$", re.M)


def callee_protos(asm_text: str, ctx_text: str) -> str:
    """Prototypes (from proto_index.py) of the callees the TU does not declare."""
    try:
        protos = json.loads(PROTOS.read_text())
    except (OSError, ValueError):
        return ""
    out = []
    for name in sorted(set(_CALL.findall(asm_text))):
        if name in protos and not re.search(rf"\b{re.escape(name)}\s*\(", ctx_text):
            out.append(protos[name])
    return "\n".join(out)


def draft(unit: str, tu: str, asm: Path, scratch: Path) -> dict:
    """Return {ok, c, context: decls|none, data: bool, error}. Tries the richest
    input first and falls back when m2c rejects it."""
    ctx_opts: list[tuple[str, Path | None]] = []
    ctx = context_file(unit, tu)
    if ctx:
        clean = scratch / f"{unit}__{tu}.ctx.{os.getpid()}.c"
        clean.parent.mkdir(parents=True, exist_ok=True)
        decls = sanitize(ctx.read_text(errors="replace"))
        clean.write_text(decls)
        extra = callee_protos(asm.read_text(errors="replace"), decls)
        if extra:
            richer = scratch / f"{unit}__{tu}.ctxp.{os.getpid()}.c"
            richer.write_text(decls + "\n/* callees declared in other TUs (proto_index.py) */\n" + extra + "\n")
            ctx_opts.append(("decls+protos", richer))
        ctx_opts.append(("decls", clean))
    ctx_opts.append(("none", None))
    labels = set(re.findall(r"\.L[0-9A-F]{8}", asm.read_text(errors="replace")))
    data = data_files(unit, tu, scratch, labels)
    err, partial = "", None
    for label, c in ctx_opts:
        for extra in ([data, []] if data else [[]]):
            try:
                p = run_m2c(asm, c, extra)
            except subprocess.TimeoutExpired:
                err = f"m2c timeout ({label} context)"
                continue
            res = {"ok": True, "c": p.stdout, "context": label, "data": bool(extra)}
            if p.returncode == 0 and p.stdout.strip() and not _failed(p.stdout):
                return res
            if p.stdout.strip() and partial is None:
                partial = res              # m2c's failure comment still shows the asm flow
            tail = (p.stderr or p.stdout).strip().splitlines()
            err = f"{label}: " + next((t for t in reversed(tail) if t.strip() and t.strip() != "*/"), "m2c failed")
    if partial:
        return {**partial, "partial": True, "error": err}
    return {"ok": False, "c": "", "context": None, "error": err}


_ZERO_INIT = re.compile(r"(=\s*\{)(\s*0,)+\s*0?,?\s*(\};)")


def collapse_zero_inits(text: str) -> str:
    """m2c writes data definitions one element per line; an all-zero array
    (e.g. a 0x9800-byte table) becomes tens of thousands of `0,` lines that an
    agent then carries into the TU. `= {0}` produces the same bytes."""
    return _ZERO_INIT.sub(r"\1 0 \3", text)


def seed(unit: str, tu: str, name: str, asm: Path, body: Path, write_body: bool) -> dict:
    with tempfile.TemporaryDirectory(prefix="m2c_") as td:
        d = draft(unit, tu, asm, Path(td))
    side = body.with_name(body.stem + ".m2c.c")
    res = {"ok": d["ok"], "context": d.get("context"), "data": d.get("data"),
           "partial": d.get("partial", False), "draft": str(side.relative_to(ROOT))}
    gh = ROOT / "build/ghidra" / unit / f"{name}.c"      # scripts/ghidra/export.sh
    if gh.is_file():
        gside = body.with_name(body.stem + ".ghidra.c")
        gside.parent.mkdir(parents=True, exist_ok=True)
        gside.write_text(gh.read_text())
        res["ghidra"] = str(gside.relative_to(ROOT))
    if not d["ok"]:
        res["error"] = d.get("error")
        return res
    side.parent.mkdir(parents=True, exist_ok=True)
    head = (f"/* m2c draft ({TARGET}, context={d['context']}{', PARTIAL' if d.get('partial') else ''}): "
            "rework into matching C */\n")
    d["c"] = collapse_zero_inits(d["c"])
    side.write_text(head + d["c"])
    if write_body and not body.exists():
        body.write_text(head + d["c"])
        res["seeded_body"] = True
        try:   # make the fresh draft compile (draft_fix.py; bounded, never fatal)
            import draft_fix
            fx = draft_fix.run(unit, tu, name, body, write=True, timeout=60)
            res["draft_fix"] = {k: fx.get(k) for k in ("ok", "errors", "initial_errors", "score", "skipped")}
        except Exception as e:  # noqa: BLE001
            res["draft_fix"] = {"error": str(e)[-200:]}
    return res


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", required=True)
    ap.add_argument("--tu", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--asm", type=Path, help="default: located via the queue")
    ap.add_argument("--body", type=Path)
    ap.add_argument("--seed", action="store_true", help="also write the draft as the body if none exists")
    a = ap.parse_args()
    body = a.body or ROOT / f"build/auto_decomp/bodies/{a.unit}/{a.tu}/{a.name}.c"
    asm = a.asm
    if asm is None:
        hits = list((ROOT / "build" / a.unit / "asm").rglob(f"nonmatchings/**/{a.tu}/{a.name}.s"))
        if not hits:
            print(json.dumps({"ok": False, "error": "asm not found"}))
            return 1
        asm = hits[0]
    res = seed(a.unit, a.tu, a.name, (ROOT / asm).resolve(), (ROOT / body).resolve(), a.seed)
    print(json.dumps(res, indent=1))
    if res["ok"] and not a.seed:
        sys.stdout.write((ROOT / res["draft"]).read_text())
    return 0 if res["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

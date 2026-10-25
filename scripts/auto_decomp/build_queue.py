#!/usr/bin/env python3
"""Build / refresh the INCLUDE_ASM candidate queue (smallest first; callees that
unblock same-TU callers get a size boost, see callgraph.py)."""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
INC = re.compile(r'^\s*INCLUDE_ASM\(\s*"([^"]+)"\s*,\s*([^)\s]+)\s*\)\s*;', re.M)
ADDR = re.compile(r'/\*\s*([0-9A-Fa-f]+)\s+[0-9A-Fa-f]{8}')


def function_size(asm: Path) -> int:
    if not asm.is_file():
        return 10**9
    text = asm.read_text(errors="replace")
    addrs = ADDR.findall(text)
    if len(addrs) >= 2:
        return int(addrs[-1], 16) - int(addrs[0], 16) + 4
    if len(addrs) == 1:
        return 4
    return max(4, text.count("\n") * 4)


GLABEL = re.compile(r"^\s*glabel\s+(\S+?)(?:,|\s|$)", re.M)
INSN = re.compile(r"^\s*/\*[^*]*\*/\s+([a-z0-9.]+)\s*([^\n]*)$", re.M)
_STORE = re.compile(r"^(s[bhwd]|sw[lr]|sd[lr]|sq|swc1|sdc1|sqc2)$")
_FLOW = re.compile(r"^(b[a-z]*|j|jal|jr|jalr)$")


# Instructions gcc 2.96 never emits from plain C: VU0 macro mode / COP2 moves
# (lqc2, sqc2, qmfc2, qmtc2, cfc2, ctc2, bc2*, v* ops), sync, and system ops
# (syscall, eret, ei, di, mtc0, mfc0, cache, `break 0`). Only inline asm can
# produce them and inline asm is banned, so such functions are skipped
# (o414 xglRenderDrawFlipPk: sync; o417 CalcNearCrossPointCircle: lqc2/vsub).
# `break 0, 7` (gcc's divide-by-zero trap) is NOT in the list.
_BLOCKED_MN = re.compile(r"^(?:lqc2|sqc2|qmfc2|qmtc2|cfc2|ctc2|bc2[a-z]*|v[a-z0-9]+|sync|syscall|eret|ei|di|"
                         r"mtc0|mfc0|cache)(?:\.[a-z]+)?$")


_QMEM = re.compile(r"^\$(\w+),\s*(-?0x[0-9A-Fa-f]+|-?\d+)\(\$(\w+)\)")


def _addiu_base(ins: list, i: int, reg: str) -> bool:
    """Was REG last written (within 6 insns before ins[i]) by `addiu REG, rS, K` with K != 0
    or K = %lo(sym)? gcc folds such an offset into the lq/sq itself (`lq $2,%lo(sym)($3)`);
    an asm "r" operand cannot. (The splat .s writes a global's low half as %lo(sym), not a
    number: the round-7 screen missed o480 xglCameraTravelInit on exactly that.)"""
    for mn, ops in reversed(ins[max(0, i - 6):i]):
        o = [x.strip() for x in ops.split("#")[0].split(",")]
        if not o or o[0] != f"${reg}" or re.match(r"^(s[bhwdq]|sw[lr]|sd[lr]|swc1|sdc1|b|j)", mn):
            continue
        if mn != "addiu" or len(o) != 3:
            return False
        if o[2].startswith("%lo("):           # global base `addiu rX,rX,%lo(sym)` (o480 xglCameraTravelInit)
            return True
        try:
            return int(o[2], 0) != 0
        except ValueError:
            return False
    return False


def _built_base(ins: list, i: int, reg: str) -> bool:
    """Was REG last written (within 6 insns before ins[i]) by an address build,
    `addu REG, rS, rT` or a nonzero / %lo `addiu` (fold11)?"""
    for mn, ops in reversed(ins[max(0, i - 6):i]):
        o = [x.strip() for x in ops.split("#")[0].split(",")]
        if not o or o[0] != f"${reg}" or re.match(r"^(s[bhwdq]|sw[lr]|sd[lr]|swc1|sdc1|b|j)", mn):
            continue
        if mn == "addu" and len(o) == 3:
            return True
        return _addiu_base(ins, i, reg)
    return False


def qword_asm_runs(text: str) -> int:
    """Fold11 (o512 JNT_setMDLMatrix, case 5): runs of >= 2 adjacent `lq rX,K(rA);
    sq rX,K(rB)` pairs (same offsets on both sides) all through ONE scratch
    register, both bases built by `addu` / a folded-looking `addiu` right
    before the run: an unrolled inline-asm block copy (4 pairs via $v0 there).
    gcc's own TImode block copies interleave several registers (sched1 runs
    before allocation). Returns the number of such runs."""
    ins = [(mn, ops.strip()) for mn, ops in INSN.findall(text)]
    n, i = 0, 0
    while i < len(ins) - 1:
        run, j, key = 0, i, None
        while j + 1 < len(ins) and ins[j][0] == "lq" and ins[j + 1][0] == "sq":
            a, b = _QMEM.match(ins[j][1]), _QMEM.match(ins[j + 1][1])
            if not a or not b or a.group(1) != b.group(1) or a.group(3) == b.group(3) \
                    or int(a.group(2), 0) != int(b.group(2), 0):
                break
            k = (a.group(1), a.group(3), b.group(3))
            if key is not None and k != key:
                break
            key = k
            run += 1
            j += 2
        if run >= 2 and key[0] not in (key[1], key[2]) \
                and _built_base(ins, i, key[1]) and _built_base(ins, i, key[2]):
            n += 1
        i = j if run else i + 1
    return n


def qword_asm_copies(text: str) -> int:
    """Adjacent `lq rX,0(rA)` / `sq rX,0(rB)` pairs that only the inline-asm
    qword-copy macro produces (sceVu0CopyVector-style `lq $2/$8,0(%1); sq ..,0(%0)`;
    o440 sefPushEffect, o472 ACT_DrawShadow): the copy register is the macro's
    fixed $8 (t0), or both bases were just built by a nonzero `addiu` that gcc
    would have folded into the offsets. Pure-C TImode / volatile quadword copies
    (xglLightIntensity*, JNT_getRoot*, TCAMERA_mpackGetInterest, look_get_place,
    xglMatrixReverse, GameDispTag) have neither, so they are not counted."""
    ins = [(mn, ops.strip()) for mn, ops in INSN.findall(text)]
    n = 0
    for i in range(len(ins) - 1):
        if ins[i][0] != "lq" or ins[i + 1][0] != "sq":
            continue
        a, b = _QMEM.match(ins[i][1]), _QMEM.match(ins[i + 1][1])
        if not a or not b or a.group(1) != b.group(1) or a.group(3) == b.group(3):
            continue
        if int(a.group(2), 0) or int(b.group(2), 0):
            continue
        if a.group(1) in ("8", "t0") or (_addiu_base(ins, i, a.group(3)) and _addiu_base(ins, i, b.group(3))):
            n += 1
    return n


# Fold13: functions whose matching C crashes cc1 in the full TU (try_fn matches the
# function alone, integrate cannot build it). Tagged `ice` in queue.json; next.py,
# permute_queue, trick_sweep and retry_best skip them (`next.py --key` lists why).
KNOWN_ICE = {
    "ov11/mini_g/PoBonusCardAnime": ("cc1 ICE in reload_cse_simplify_operands on the 16-byte struct copy "
                                     "from CardPos[i] (o543; try_fn score 1.0, integrate fails)"),
}


def asm_blocked_reason(text: str) -> str | None:
    """Why NAME's asm cannot come from C (VU0/COP2, sync, system ops, the
    inline-asm qword-copy macro), or None."""
    hits = []
    for mn, ops in INSN.findall(text):
        if _BLOCKED_MN.match(mn) or (mn == "break" and re.match(r"0\s*(?:/\*|$)", ops.strip() or "0")):
            if mn not in hits:
                hits.append(mn)
    if not hits:
        # 128-bit zero store straight from $zero: ee-gcc always materialises the
        # TImode zero first (por rX,zero,zero; sq rX), so only inline asm gives
        # `sq $0` (o-2026-10-13: Camera_resetFog__I, CONSTRUCT_MODELSYSTEM).
        z = sum(1 for mn, ops in INSN.findall(text)
                if mn == "sq" and re.match(r"\s*\$(?:0|zero)\s*,", ops))
        if z:
            return f"asm has {z} `sq $0` 128-bit zero store(s) (plain C emits por+sq: inline asm only)"
        q = qword_asm_copies(text)
        if q:
            return (f"asm has {q} inline-asm qword copy pair(s) (adjacent lq/sq at offset 0 through precomputed "
                    f"bases / fixed $t0: sceVu0CopyVector-style macro, inline asm only)")
        q = qword_asm_runs(text)
        if q:
            return (f"asm has {q} inline-asm qword copy run(s) (>= 2 adjacent lq/sq pairs through one scratch "
                    f"register, addu/addiu-built bases: unrolled asm block copy, inline asm only; o512)")
        return None
    return f"asm has {', '.join(hits[:6])} (VU0/COP2/sync/system op: inline asm only)"


def nested_reason(text: str, name: str) -> str | None:
    """Why NAME looks like a GCC nested function (unmatchable standalone: it
    needs its parent decompiled with the nested definition inside), or None.
    The original symbol is `parent.N` (splat names it parent_N), or the entry
    code reads the static chain $v0 ($2) before anything writes it (o409
    ListMake.3)."""
    m = GLABEL.search(text)
    if m and "." in m.group(1) and m.group(1).replace(".", "_") == name:
        return f"GCC nested function (original symbol {m.group(1)})"
    for mn, ops in INSN.findall(text):
        regs = re.findall(r"\$(\w+)", ops.split("#")[0])
        if _FLOW.match(mn):
            if "2" in regs or "v0" in regs:
                break                  # a branch on $v0 at entry: leave it to the agents
            break
        if not regs:
            continue
        srcs = regs if _STORE.match(mn) else regs[1:]
        if "2" in srcs or "v0" in srcs:
            return "GCC nested function (entry reads the static chain $v0 before writing it)"
        if not _STORE.match(mn) and regs[0] in ("2", "v0"):
            return None                # $v0 written first: an ordinary function
    return None


def annotate_callgraph(root: Path, candidates: list[dict]) -> None:
    """Callee-first scheduling (callgraph.py): blocked_by = same-TU INCLUDE_ASM
    callees above the function (it cannot match until they are C); unblocks =
    functions transitively waiting on it. A function that unblocks others sorts
    as if up to 4x smaller (1 + min(unblocks, 3)), so leaves come first within a
    TU without starving big functions. Best effort: no annotation on failure."""
    try:
        import callgraph
        fns = callgraph.build(root)
    except Exception:
        return
    for c in candidates:
        v = fns.get(f"{c['unit']}/{c['tu']}/{c['name']}")
        if v:
            if v["blocked_by"]:
                c["blocked_by"] = v["blocked_by"]
            if v["unblocks_all"]:
                c["unblocks"] = v["unblocks_all"]
                c["depth"] = v["depth"]


def build_queue(root: Path) -> dict:
    candidates = []
    for cpath in sorted((root / "src").rglob("*.c")):
        unit = cpath.parts[cpath.parts.index("src") + 1]
        text = cpath.read_text(errors="replace")
        for m in INC.finditer(text):
            folder, name = m.group(1), m.group(2)
            asm = root / "build" / unit / folder / f"{name}.s"
            if not asm.is_file():
                alt = root / "build" / unit / f"{folder}/{name}.s"
                asm = alt if alt.is_file() else asm
            c = {
                "unit": unit,
                "tu": cpath.stem,
                "source": str(cpath.relative_to(root)),
                "folder": folder,
                "name": name,
                "asm": str(asm.relative_to(root)) if asm.is_file() else None,
                "size": function_size(asm),
            }
            if asm.is_file():
                atext = asm.read_text(errors="replace")
                why = nested_reason(atext, name)
                if why:
                    c["nested"] = why
                why = asm_blocked_reason(atext)
                if why:
                    c["asm_blocked"] = why
            ice = KNOWN_ICE.get(f"{unit}/{cpath.stem}/{name}")
            if ice:
                c["ice"] = ice
            candidates.append(c)
    annotate_callgraph(root, candidates)
    candidates.sort(key=lambda c: (c["size"] // (1 + min(len(c.get("unblocks", ())), 3)),
                                   c["size"], c["unit"], c["tu"], c["name"]))
    buckets = {
        "le_32": sum(1 for c in candidates if c["size"] <= 32),
        "le_64": sum(1 for c in candidates if c["size"] <= 64),
        "le_128": sum(1 for c in candidates if c["size"] <= 128),
        "le_256": sum(1 for c in candidates if c["size"] <= 256),
        "gt_256": sum(1 for c in candidates if c["size"] > 256),
    }
    return {"schema": "auto-decomp-queue/1", "total": len(candidates),
            "by_size_bucket": buckets, "candidates": candidates}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("-o", type=Path, default=None)
    a = ap.parse_args()
    q = build_queue(a.root)
    out = a.o or (a.root / "build/auto_decomp/queue.json")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(q, indent=1) + "\n")
    print(json.dumps({"total": q["total"], "buckets": q["by_size_bucket"], "out": str(out)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

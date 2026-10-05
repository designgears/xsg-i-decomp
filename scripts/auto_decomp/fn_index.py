#!/usr/bin/env python3
"""Instruction index of every function in the original images, for cross-TU
sibling lookup (next.py) and duplicate reports.

    .venv/bin/python -B scripts/auto_decomp/fn_index.py            # -> build/auto_decomp/fn_index.json
    .venv/bin/python -B scripts/auto_decomp/fn_index.py --report   # duplicate summary

Matched functions no longer have splat asm, so the words come straight from
the original ELFs (config/symbols/<unit>.txt gives address and size). Each
function is reduced to its mnemonic sequence and a `shape` hash: mnemonics plus
registers, every immediate/symbol masked, so a copy of a function in another
TU or overlay hashes the same even though its addresses differ.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

import rabbitizer

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/auto_decomp/fn_index.json"
IMAGES = {"main": "SLUS_204.69", "ov01": "OV01.OVL", "ov02": "OV02.OVL",
          "ov10": "OV10.OVL", "ov11": "OV11.OVL", "ov12": "OV12.OVL"}
SYM = re.compile(r"^\s*([A-Za-z_.$][\w.$]*)\s*=\s*0x([0-9A-Fa-f]+);.*type:func.*size:0x([0-9A-Fa-f]+)")
DEF = re.compile(r"^[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{", re.M)
ASM = re.compile(r"INCLUDE_ASM\(\s*\"[^\"]*\"\s*,\s*([A-Za-z_]\w*)\s*\)")
MASK = re.compile(r"%\w+\([^)]*\)|0x[0-9A-Fa-f]+|-?\b\d+\b|\b(?!\$)[A-Za-z_.][\w.]*\b")


def game_dir() -> Path:
    try:
        return Path(json.loads((ROOT / "config/toolchain-local.json").read_text())["game_dir"])
    except (OSError, KeyError, ValueError):
        return ROOT / "game/extracted-iso"


def segments(elf: bytes) -> list[tuple[int, int, int]]:
    """(vaddr, file offset, file size) of each PT_LOAD."""
    phoff, = struct.unpack_from("<I", elf, 0x1C)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x2A)
    out = []
    for i in range(phnum):
        ptype, off, vaddr, _pa, filesz = struct.unpack_from("<IIIII", elf, phoff + i * phentsize)
        if ptype == 1:
            out.append((vaddr, off, filesz))
    return out


def read(elf: bytes, segs, addr: int, size: int) -> bytes | None:
    for vaddr, off, filesz in segs:
        if vaddr <= addr and addr + size <= vaddr + filesz:
            return elf[off + addr - vaddr: off + addr - vaddr + size]
    return None


def tu_map(unit: str) -> tuple[dict[str, str], set[str]]:
    """name -> TU for every function a TU defines in C or includes as asm; and the C-defined set."""
    where, in_c = {}, set()
    for src in (ROOT / "src" / unit).glob("*.c"):
        text = src.read_text(errors="replace")
        for n in ASM.findall(text):
            where.setdefault(n, src.stem)
        for n in DEF.findall(text):
            where[n] = src.stem
            in_c.add(n)
    return where, in_c


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()
    if args.report:
        return report()
    gd = game_dir()
    index = {}
    for unit, image in IMAGES.items():
        elf = (gd / image).read_bytes()
        segs = segments(elf)
        where, in_c = tu_map(unit)
        for line in (ROOT / f"config/symbols/{unit}.txt").read_text().splitlines():
            m = SYM.match(line)
            if not m:
                continue
            name, addr, size = m.group(1), int(m.group(2), 16), int(m.group(3), 16)
            if name not in where or size < 8:        # SDK / out of scope, or trivial
                continue
            data = read(elf, segs, addr, size)
            if data is None:
                continue
            mn, shape = [], []
            for i in range(0, size - size % 4, 4):
                ins = rabbitizer.Instruction(struct.unpack_from("<I", data, i)[0], vram=addr + i,
                                             category=rabbitizer.InstrCategory.R5900)
                mn.append(ins.getOpcodeName())
                shape.append(MASK.sub("#", ins.disassemble()))
            index[f"{unit}/{name}"] = {
                "unit": unit, "tu": where[name], "name": name, "c": name in in_c,
                "mn": " ".join(mn),
                "shape": hashlib.sha1("\n".join(shape).encode()).hexdigest()[:16],
            }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(index))
    print(json.dumps({"functions": len(index), "in_c": sum(v["c"] for v in index.values()),
                      "out": str(OUT.relative_to(ROOT))}))
    return 0


def report() -> int:
    index = json.loads(OUT.read_text())
    groups = collections.defaultdict(list)
    for k, v in index.items():
        groups[v["shape"]].append(v)
    free = twins = 0
    for g in groups.values():
        todo = [v for v in g if not v["c"]]
        if not todo or len(g) < 2:
            continue
        if any(v["c"] for v in g):
            free += len(todo)                         # a matched copy exists
        else:
            twins += len(todo)                        # solve one, the rest follow
    print(json.dumps({"unmatched_with_matched_copy": free, "unmatched_in_unmatched_dup_groups": twins}))
    return 0


if __name__ == "__main__":
    sys.exit(main())

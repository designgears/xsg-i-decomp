#!/usr/bin/env python3
"""Heuristic decompiler for tiny R5900 leaf patterns (getters / bump allocators / etc.).

Writes a C definition to --out when it recognizes the body. Exit 0 on success,
1 when the pattern is unknown (caller should fall back to an LLM / m2c agent).
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

INS = re.compile(
    r'/\*\s*[0-9A-Fa-f]+\s+[0-9A-Fa-f]{8}\s+([0-9A-Fa-f]{8})\s*\*/\s*(\S+)\s*(.*)$'
)


def parse_asm(text: str) -> list[tuple[str, str, str]]:
    """Return [(raw_word_hex, mnemonic, operands), ...] ignoring pseudo/meta."""
    out = []
    for line in text.splitlines():
        m = INS.search(line)
        if not m:
            continue
        out.append((m.group(1), m.group(2), m.group(3).strip()))
    return out


def try_bump_allocator(insns: list[tuple[str, str, str]], name: str) -> str | None:
    """lw v0, SYM; addiu v1, v0, IMM; jr ra; sw v1, SYM  -> return old, advance."""
    if len(insns) != 4:
        return None
    a, b, c, d = insns
    if a[1] != "lw" or b[1] != "addiu" or c[1] != "jr" or d[1] not in ("sw", "sw"):
        return None
    # lw $2, SYM
    lw_ops = [x.strip() for x in a[2].split(",")]
    sw_ops = [x.strip() for x in d[2].split(",")]
    add_ops = [x.strip() for x in b[2].split(",")]
    if len(lw_ops) < 2 or len(sw_ops) < 2 or len(add_ops) < 3:
        return None
    sym = lw_ops[1]
    if sw_ops[1] != sym:
        return None
    if add_ops[1] != lw_ops[0]:
        return None
    imm = add_ops[2]
    if not re.fullmatch(r'-?0x[0-9A-Fa-f]+|-?\d+', imm):
        return None
    # Prefer opaque pointer type matching surrounding TU style.
    return (
        f"void *{name}(void)\n"
        "{\n"
        f"    void *p = {sym};\n"
        "\n"
        f"    {sym} = (void *)((unsigned char *)p + {imm});\n"
        "    return p;\n"
        "}\n"
    )


def try_simple_lw_return(insns: list[tuple[str, str, str]], name: str) -> str | None:
    """lw v0, SYM; jr ra; nop  OR  lw v0, OFF(reg); jr ra; nop"""
    if len(insns) < 2:
        return None
    # strip trailing nops after jr
    body = []
    for w, m, o in insns:
        body.append((w, m, o))
        if m == "jr":
            break
    if len(body) < 2 or body[-1][1] != "jr":
        return None
    useful = [x for x in body[:-1] if x[1] != "nop"]
    if len(useful) == 1 and useful[0][1] == "lw":
        ops = [x.strip() for x in useful[0][2].split(",")]
        if len(ops) >= 2 and ops[0] in ("$2", "v0"):
            rhs = ops[1]
            if "(" not in rhs:
                return f"void *{name}(void)\n{{\n    return {rhs};\n}}\n"
    return None


def try_lq_sq_copy(insns: list[tuple[str, str, str]], name: str) -> str | None:
    """lq $2,0($5); sq $2,0($4); jr; nop -> 16-byte copy a1->a0."""
    useful = [(m, o) for _, m, o in insns if m != "nop"]
    if len(useful) < 3:
        return None
    if useful[0][0] != "lq" or useful[1][0] != "sq" or useful[2][0] != "jr":
        return None
    return (
        f"void {name}(void *dst, const void *src)\n"
        "{\n"
        "    __asm__ __volatile__(\n"
        "        \"lq $2, 0(%1)\\n\"\n"
        "        \"sq $2, 0(%0)\\n\"\n"
        "        :\n"
        "        : \"r\"(dst), \"r\"(src)\n"
        "        : \"$2\", \"memory\");\n"
        "}\n"
    )


def try_indexed_lw(insns: list[tuple[str, str, str]], name: str) -> str | None:
    """sll a0,2; lui %hi(SYM); addu; lw %lo(SYM); jr -> SYM[index]."""
    useful = [(m, o) for _, m, o in insns if m != "nop"]
    if len(useful) < 5:
        return None
    mnems = [m for m, _ in useful]
    if mnems[:5] != ["sll", "lui", "addu", "lw", "jr"]:
        return None
    m = re.search(r"%hi\(([^)]+)\)", useful[1][1])
    if not m:
        return None
    sym = m.group(1)
    return (
        f"void *{name}(int index)\n"
        "{\n"
        f"    return ((void **){sym})[index];\n"
        "}\n"
    )


def try_li_return(insns: list[tuple[str, str, str]], name: str) -> str | None:
    """addiu/ori/lui patterns returning a constant — keep narrow for now."""
    return None


def try_gs_tag_entry(insns: list[tuple[str, str, str]], name: str) -> str | None:
    """nmlPacketAddGs*: append (data, tag) into g_aGsTag[g_nGsEntry++].

    Canonical 12-insn leaf:
      lw $5, g_nGsEntry
      lui/addiu $3, g_aGsTag
      addiu $7, $0, TAG
      sll $2, $5, 4
      addiu $5, $5, 1
      addu $6, $2, $3
      daddu $2, $6, $0
      sd $4, 0x10($2)
      sd $7, 0x18($6)
      jr $31
      sw $5, g_nGsEntry
    """
    useful = [(m, o) for _, m, o in insns if m != "nop"]
    if len(useful) < 11:
        return None
    mnems = [m for m, _ in useful]
    # Allow optional trailing noise after jr/sw
    if mnems[0] != "lw" or "lui" not in mnems[:3] or "sll" not in mnems[:6]:
        return None
    if "sd" not in mnems or mnems.count("sd") < 2:
        return None
    if "jr" not in mnems:
        return None
    # entry counter symbol
    lw_ops = [x.strip() for x in useful[0][1].split(",")]
    if len(lw_ops) < 2 or lw_ops[0] not in ("$5", "a1"):
        return None
    entry_sym = lw_ops[1]
    # tag immediate from addiu $7, $0, IMM
    tag = None
    for m, o in useful:
        if m != "addiu":
            continue
        ops = [x.strip() for x in o.split(",")]
        if len(ops) >= 3 and ops[0] in ("$7", "a3") and ops[1] in ("$0", "zero", "$0"):
            tag = ops[2]
            break
    if tag is None:
        return None
    # base symbol from %hi/%lo
    base = None
    for m, o in useful:
        if m == "lui":
            hm = re.search(r"%hi\(([^)]+)\)", o)
            if hm:
                base = hm.group(1)
                break
    if not base:
        return None
    # Confirm sd of $4 at 0x10 and tag reg at 0x18
    sd_offs = []
    for m, o in useful:
        if m != "sd":
            continue
        # sd $r, OFF($b)
        mm = re.match(r"\$(\d+|a0|a3|t\d+),\s*(-?0x[0-9A-Fa-f]+|-?\d+)\(", o)
        if mm:
            sd_offs.append(mm.group(2).lower())
    if "0x10" not in sd_offs or "0x18" not in sd_offs:
        return None
    return (
        f"void {name}(unsigned long data)\n"
        "{\n"
        f"    int n = {entry_sym};\n"
        f"    unsigned long *p = (unsigned long *)((char *){base} + (n << 4));\n"
        "\n"
        f"    {entry_sym} = n + 1;\n"
        "    p[2] = data;\n"
        f"    p[3] = {tag};\n"
        "}\n"
    )


def try_gs_tag_entry_sw(insns: list[tuple[str, str, str]], name: str) -> str | None:
    """Variant that stores data as two sw (lo/hi) then sd tag — Pixeltest/Texture."""
    useful = [(m, o) for _, m, o in insns if m != "nop"]
    if len(useful) < 10:
        return None
    mnems = [m for m, _ in useful]
    if mnems[0] != "lw" or "sll" not in mnems[:6] or "sd" not in mnems:
        return None
    if mnems.count("sw") < 2:
        return None
    lw_ops = [x.strip() for x in useful[0][1].split(",")]
    if len(lw_ops) < 2:
        return None
    entry_sym = lw_ops[1]
    base = None
    for m, o in useful:
        if m == "lui":
            hm = re.search(r"%hi\(([^)]+)\)", o)
            if hm and "GsTag" in hm.group(1):
                base = hm.group(1)
                break
    if not base:
        for m, o in useful:
            if m == "lui":
                hm = re.search(r"%hi\(([^)]+)\)", o)
                if hm:
                    base = hm.group(1)
                    break
    if not base:
        return None
    tag = None
    for m, o in useful:
        if m != "addiu":
            continue
        ops = [x.strip() for x in o.split(",")]
        if len(ops) >= 3 and ops[1] in ("$0", "zero") and ops[0] in (
            "$7", "$8", "a3", "t0"
        ):
            tag = ops[2]
            break
    if tag is None:
        return None
    # Detect whether $4 (arg) is stored into the tag slot, or zeros (Texture).
    # Ignore `sw $4, g_nGsEntry` (no parenthesis — counter writeback).
    stores_arg = any(
        m == "sw" and o.strip().startswith("$4,") and "(" in o for m, o in useful
    )
    if stores_arg:
        data_stmt = "    *(unsigned int *)&p[2] = (unsigned int)data;\n    *((unsigned int *)&p[2] + 1) = 0;\n"
        sig = f"void {name}(unsigned int data)\n"
    else:
        data_stmt = "    p[2] = 0;\n"
        sig = f"void {name}(void)\n"
    return (
        f"{sig}"
        "{\n"
        f"    int n = {entry_sym};\n"
        f"    unsigned long *p = (unsigned long *)((char *){base} + (n << 4));\n"
        "\n"
        f"    {entry_sym} = n + 1;\n"
        f"{data_stmt}"
        f"    p[3] = {tag};\n"
        "}\n"
    )


def decompile(asm_text: str, name: str) -> str | None:
    insns = parse_asm(asm_text)
    for fn in (try_bump_allocator, try_simple_lw_return, try_lq_sq_copy,
               try_indexed_lw, try_gs_tag_entry, try_gs_tag_entry_sw,
               try_li_return):
        got = fn(insns, name)
        if got:
            return got
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--asm", type=Path, required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()
    text = a.asm.read_text(errors="replace")
    body = decompile(text, a.name)
    if not body:
        print("unrecognized pattern", file=sys.stderr)
        return 1
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(body)
    print(body)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Diff one data section between the objdiff report's target (original) and base (C) objects.
Usage (checkout root, after tools/dataown/report.sh): secdiff.py UNIT/STEM .data [max_lines]
Shows symbols on both sides, size mismatch, and every differing 4-byte word / relocation (offset, target, base)."""
import struct, sys
sys.path.insert(0, 'tools/tu'); from elfinfo import Elf
name, sec = sys.argv[1], sys.argv[2]; lim = int(sys.argv[3]) if len(sys.argv) > 3 else 40
d = f'build/objdiff/{name}'
def load(p):
    e = Elf(open(p, 'rb').read()); s = e.section(sec); data = e.section_bytes(s)
    syms = sorted((x.value, x.size, x.name) for x in e.symbols if x.shndx == s.index and x.type != 3 and not x.name.endswith('.NON_MATCHING'))
    rel = {}
    for r in e.sections:
        if r.type == 9 and r.info == s.index:
            for at in range(r.offset, r.offset + r.size, 8):
                off, info = struct.unpack_from('<II', e.data, at); sy = e.symbols[info >> 8]
                rel[off] = (info & 255, sy.name if sy.type != 3 else e.sections[sy.shndx].name)
    return data, syms, rel
T, ts, tr = load(f'{d}/target.o'); B, bs, br = load(f'{d}/base.nodebug.o')
print(f'size target 0x{len(T):x} base 0x{len(B):x}')
print('target syms:', [(hex(a), hex(b), n) for a, b, n in ts][:30])
print('base   syms:', [(hex(a), hex(b), n) for a, b, n in bs][:30])
n = 0
for off in range(0, max(len(T), len(B)), 4):
    tw, bw = T[off:off+4], B[off:off+4]
    if tw != bw or tr.get(off) != br.get(off):
        print(f'  +0x{off:x}: target {tw.hex()} {tr.get(off)}  base {bw.hex()} {br.get(off)}'); n += 1
        if n >= lim: break
print(n, 'differing words shown')

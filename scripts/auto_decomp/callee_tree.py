#!/usr/bin/env python3
"""Print the tree of still-INCLUDE_ASM callees (jal/j targets in the target asm) under a
function, down to leaves. Decompile bottom-up: a caller often only matches once its
callees are C defined earlier in the TU (CC_Kara_MoveFase_Type4 bnez/bnezl, 2026-10-18).
    python3 -B scripts/auto_decomp/callee_tree.py NAME"""
import re, sys, glob, os
ROOT='/home/bobby/Projects/decomp/xsg-i-decomp'
# map name -> (unit, tu) for INCLUDE_ASM functions
asm_fns = {}
for f in glob.glob(f'{ROOT}/src/*/*.c'):
    u = f.split('/')[-2]; t = os.path.basename(f)[:-2]
    for m in re.finditer(r'^\s*INCLUDE_ASM\([^,]+,\s*(\w+)\);', open(f, errors='replace').read(), re.M):
        asm_fns[m.group(1)] = (u, t)
def asm_path(name):
    u, t = asm_fns[name]
    for p in glob.glob(f'{ROOT}/build/{u}/asm/**/{name}.s', recursive=True):
        return p
def callees(name):
    p = asm_path(name)
    if not p: return []
    out = []
    for m in re.finditer(r'\b(?:jal|j)\s+([A-Za-z_]\w*)', open(p).read()):
        c = m.group(1)
        if c != name and not c.startswith('.L') and c not in out:
            out.append(c)
    return out
def tree(name, depth=0, seen=None):
    seen = seen if seen is not None else set()
    seen.add(name)
    cs = [c for c in callees(name) if c in asm_fns]
    u, t = asm_fns[name]
    print('  ' * depth + f'{u}/{t}/{name}' + ('' if cs else '   <- leaf (no INCLUDE_ASM callees)'))
    for c in cs:
        if c not in seen:
            tree(c, depth + 1, seen)
if sys.argv[1] not in asm_fns:
    print(f'{sys.argv[1]}: already C (not INCLUDE_ASM)')
else:
    tree(sys.argv[1])

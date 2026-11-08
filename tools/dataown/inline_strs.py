#!/usr/bin/env python3
"""Replace extern string labels of a TU with the string literals they were in the original.
  inline_strs.py UNIT/STEM            (checkout root; MAIN units)
Only labels that (a) are anonymous in the original (no symtab entry), (b) are a single
.asciz item in the TU's splat data, and (c) are declared `extern const char D_x[];` in the C."""
import re, sys, subprocess
from pathlib import Path
unit, stem = sys.argv[1].split('/')
src = Path(f'src/{unit}/{stem}.c'); text = src.read_text(encoding='latin-1')
files = sorted(Path(f'build/{unit}/asm/main/data').glob(f'{stem}.*.s')) if unit == 'main' else \
        sorted(Path(f'build/{unit}/asm/data/{unit}').glob(f'{stem}.*.s'))
strs = {}
for f in files:
    cur, lines = None, []
    for l in f.read_text(encoding='latin-1').splitlines():
        m = re.match(r'dlabel (\S+)', l)
        if m: cur, lines = m.group(1), []; continue
        if l.startswith('enddlabel') and cur:
            asc = [re.search(r'\*/ \.asciz (".*")\s*$', x) for x in lines]
            other = [x for x in lines if re.search(r'\*/ \.(word|byte|short|ascii |float|double|space)', x)]
            asc = [mm.group(1) for mm in asc if mm]
            if asc and not other and asc[0] != '""' and all(a == '""' for a in asc[1:]):
                strs[cur] = asc[0]
            elif not asc and lines:
                # splat read the string as words (`.word sym + off`): decode the raw bytes
                raw = b''
                for x in lines:
                    hx = re.search(r'/\* [0-9A-F]+ [0-9A-F]{8} ([0-9A-F]+) \*/', x)
                    if hx: raw += bytes.fromhex(hx.group(1))
                z = raw.find(b'\0')
                if z >= 0 and raw and not any(raw[z:]) and all(32 <= c < 127 for c in raw[:z]):
                    body = raw[:z].decode().replace('\\', '\\\\').replace('"', '\\"')
                    strs[cur] = '"' + body + '"'
            cur = None; continue
        if cur: lines.append(l)
nm = subprocess.run(['.work/toolchains/ee-binutils/bin/mips64r5900el-ps2-elf-nm', 'build/main/orig/SLUS_204.69'],
                    capture_output=True, text=True).stdout if unit == 'main' else ''
named = set(x.split()[-1] for x in nm.splitlines())
done = 0
for lab, lit in strs.items():
    decl = re.compile(rf'^extern const char {re.escape(lab)}\[\];\n', re.M)
    if lab in named or not decl.search(text): continue
    text = decl.sub('', text, count=1)
    text = re.sub(rf'\b{re.escape(lab)}\b', lambda _m: lit, text)
    done += 1
src.write_text(text, encoding='latin-1')
print(f'{sys.argv[1]}: {done} labels inlined of {len(strs)} string items')

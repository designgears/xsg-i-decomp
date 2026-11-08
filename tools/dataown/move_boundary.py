#!/usr/bin/env python3
"""Move the boundary between two adjacent MAIN TUs in one data section.

  move_boundary.py SECTION PREV_TU NEXT_TU NEW_VA      (run from the checkout root)

Updates config/split/main.yaml (the NEXT_TU subsegment's file offset) and
config/objects/main.objects.json (section order pieces and both TUs' section
records: start/end/size/pieces/bounds), preserving the manifest's formatting.
Afterwards: configure.py, re-derive both TUs' carves (tu_data.py carve), ninja gate.
"""
import json
import re
import sys
from pathlib import Path

sec, prev, nxt, new = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4], 16)
h8 = lambda v: f'0x{v:08X}'
root = Path.cwd()

if '/' in prev:          # overlay: ovl/tu names; config/objects/<ovl>.layout.json + config/split/<ovl>.yaml
    unit = prev.split('/')[0]
    lp = root / f'config/objects/{unit}.layout.json'
    raw = lp.read_text(); lay = json.loads(raw)
    fam = lay['families'][sec]
    i = next(k for k, e in enumerate(fam) if e[0] == prev)
    assert fam[i + 1][0] == nxt, f'{prev} and {nxt} are not adjacent in {sec}'
    old = int(fam[i + 1][1], 16)
    assert int(fam[i][1], 16) < new < int(fam[i + 1][2], 16), 'new boundary outside the two pieces'
    fam[i][2] = fam[i + 1][1] = f'0x{new:08x}'
    indent = 2 if raw.startswith('{\n  "') else 1
    lp.write_text(json.dumps(lay, indent=indent, ensure_ascii=False) + '\n')
    yp = root / f'config/split/{unit}.yaml'; text = yp.read_text(); kind = sec.lstrip('.')
    pat = re.compile(rf'^(\s*- \[)0x([0-9A-Fa-f]+)(, {re.escape(kind)}, {re.escape(nxt)}\])$', re.M)
    hits = pat.findall(text); assert len(hits) == 1, f'{len(hits)} splat subsegments for {kind} {nxt}'
    off = int(hits[0][1], 16); newoff = off + (new - old)
    yp.write_text(pat.sub(lambda mm: f'{mm.group(1)}0x{newoff:X}{mm.group(3)}', text))
    print(f'{sec}: {prev} | {nxt} boundary {h8(old)} -> {h8(new)} (splat 0x{off:X} -> 0x{newoff:X})')
    sys.exit(0)

mp = root / 'config/objects/main.objects.json'
m = json.loads(mp.read_text())
order = m['sections'][sec]['order']
idx = {q['tu']: i for i, q in enumerate(order) if q.get('tu') in (prev, nxt)}
assert set(idx) == {prev, nxt} and idx[nxt] == idx[prev] + 1, f'{prev} and {nxt} are not adjacent in {sec}'
a, b = order[idx[prev]], order[idx[nxt]]
old = int(b['start'], 16)
assert int(a['start'], 16) < new < int(b['end'], 16), 'new boundary outside the two pieces'
a['end'] = b['start'] = h8(new)

for t in m['tus']:
    rec = t.get('sections', {}).get(sec)
    if not rec or t.get('name') not in (prev, nxt):
        continue
    if t['name'] == prev:
        rec['end'] = h8(new)
        if rec.get('pieces'):
            rec['pieces'][-1]['end'] = h8(new)
        if rec.get('bounds'):
            rec['bounds'][1] = h8(new)
    else:
        rec['start'] = h8(new)
        if rec.get('pieces'):
            rec['pieces'][0]['start'] = h8(new)
        if rec.get('anchored'):
            rec['anchored'][0] = h8(min(new, int(rec['anchored'][0], 16)))
        rec['rule'] = 'moved boundary (data ownership by reference)'
    rec['size'] = int(rec['end'], 16) - int(rec['start'], 16)
mp.write_text(json.dumps(m, indent=2, ensure_ascii=False) + '\n')

# splat: file offset = va - (section va - section file offset); derive delta from the old subsegment
yp = root / 'config/split/main.yaml'
text = yp.read_text()
kind = sec.lstrip('.')
pat = re.compile(rf'^(\s*- \[)0x([0-9A-Fa-f]+)(, {re.escape(kind)}, {re.escape(nxt)}\])$', re.M)
hits = pat.findall(text)
assert len(hits) == 1, f'{len(hits)} splat subsegments for {kind} {nxt}'
off = int(hits[0][1], 16)
newoff = off + (new - old)
text = pat.sub(lambda mm: f'{mm.group(1)}0x{newoff:X}{mm.group(3)}', text)
yp.write_text(text)
print(f'{sec}: {prev} | {nxt} boundary {h8(old)} -> {h8(new)} (splat 0x{off:X} -> 0x{newoff:X})')

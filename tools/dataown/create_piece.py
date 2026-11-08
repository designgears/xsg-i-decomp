#!/usr/bin/env python3
"""MAIN or overlay (ovl/tu names): give TU NEW a data section piece carved from the tail of TU PREV's piece.

  create_piece.py SECTION PREV NEW START_VA       (checkout root)

PREV must be immediately followed (in that section's order) by a TU whose ordinal is
greater than NEW's ordinal, and NEW must have no piece in SECTION yet. The range
[START_VA, PREV's end) becomes NEW's piece. Updates config/split/main.yaml (a new
subsegment line after PREV's) and config/objects/main.objects.json (section order and
NEW's per-TU section record), preserving formatting.
"""
import copy
import json
import re
import sys
from pathlib import Path

sec, prev, new, start = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4], 16)
h8 = lambda v: f'0x{v:08X}'

if '/' in prev:          # overlay: ovl/tu names; config/objects/<ovl>.layout.json families + config/split/<ovl>.yaml
    unit = prev.split('/')[0]
    lp = Path(f'config/objects/{unit}.layout.json')
    raw = lp.read_text(); lay = json.loads(raw)
    fam = lay['families'][sec]
    text_order = list(lay['text'])                       # TU ordinal = position in .text order
    assert not any(e[0] == new for e in fam), f'{new} already has a {sec} piece'
    i = next(k for k, e in enumerate(fam) if e[0] == prev)
    nxt = fam[i + 1][0] if i + 1 < len(fam) else None
    assert text_order.index(prev) < text_order.index(new) and (nxt is None or text_order.index(new) < text_order.index(nxt)), 'ordinal order'
    end = int(fam[i][2], 16)
    assert int(fam[i][1], 16) < start < end
    fam[i][2] = f'0x{start:08x}'
    fam.insert(i + 1, [new, f'0x{start:08x}', f'0x{end:08x}'])
    indent = 2 if raw.startswith('{\n  "') else 1
    lp.write_text(json.dumps(lay, indent=indent, ensure_ascii=False) + '\n')
    yp = Path(f'config/split/{unit}.yaml'); text = yp.read_text(); kind = sec.lstrip('.')
    pat = re.compile(rf'^(\s*- \[)0x([0-9A-Fa-f]+)(, {re.escape(kind)}, {re.escape(prev)}\])$', re.M)
    hits = list(pat.finditer(text)); assert len(hits) == 1, f'{len(hits)} splat subsegments for {kind} {prev}'
    mm = hits[0]
    delta = int(fam[i][1], 16) - int(mm.group(2), 16)    # va - file offset
    line = f'{mm.group(1)}0x{start - delta:X}, {kind}, {new}]'
    yp.write_text(text[:mm.end()] + '\n' + line + text[mm.end():])
    print(f'{sec}: {new} gets [{h8(start)}, {h8(end)}) from {prev}; splat 0x{start - delta:X}')
    sys.exit(0)
mp = Path('config/objects/main.objects.json')
m = json.loads(mp.read_text())
tus = {t.get('name'): t for t in m['tus']}
order = m['sections'][sec]['order']
i = next(k for k, p in enumerate(order) if p['tu'] == prev)
assert not any(p['tu'] == new for p in order), f'{new} already has a {sec} piece'
nxt = order[i + 1] if i + 1 < len(order) else None
# SDK library objects are linked after every game object, whatever their ordinal
after_ok = (nxt is None or tus[new]['ordinal'] < tus[nxt['tu']]['ordinal']
            or (tus[nxt['tu']].get('category') == 'sdk' and tus[new].get('category') != 'sdk'))
assert tus[prev]['ordinal'] < tus[new]['ordinal'] and after_ok, 'ordinal order'
p = order[i]
end = int(p['end'], 16)
assert int(p['start'], 16) < start < end
piece = dict(c_split=False, end=p['end'], ordinal=tus[new]['ordinal'], piece=new, start=h8(start), tu=new)
p['end'] = h8(start)
order.insert(i + 1, piece)
prec = tus[prev]['sections'][sec]
prec['end'] = h8(start)
if prec.get('pieces'):
    prec['pieces'][-1]['end'] = h8(start)
if prec.get('bounds'):
    prec['bounds'][1] = h8(start)
prec['size'] = start - int(prec['start'], 16)
rec = copy.deepcopy(prec)
rec.update(start=h8(start), end=h8(end), size=end - start, rule='created piece (data ownership by reference)',
           pieces=[dict(c_split=False, end=h8(end), name=new, start=h8(start))],
           anchored=[h8(start), h8(start)], bounds=[h8(start), h8(end)])
tus[new].setdefault('sections', {})[sec] = rec
tus[new].setdefault('owners', {})[sec] = 'asm'
mp.write_text(json.dumps(m, indent=2, ensure_ascii=False) + '\n')

yp = Path('config/split/main.yaml')
text = yp.read_text()
kind = sec.lstrip('.')
pat = re.compile(rf'^(\s*- \[)0x([0-9A-Fa-f]+)(, {re.escape(kind)}, {re.escape(prev)}\])$', re.M)
mm = pat.search(text)
assert mm, f'no {kind} subsegment for {prev}'
delta = int(p['start'], 16) - int(mm.group(2), 16)          # va - file offset
line = f'{mm.group(1)}0x{start - delta:X}, {kind}, {new}]'
text = text[:mm.end()] + '\n' + line + text[mm.end():]
yp.write_text(text)
print(f'{sec}: {new} gets [{h8(start)}, {h8(end)}) from {prev}; splat 0x{start - delta:X}')

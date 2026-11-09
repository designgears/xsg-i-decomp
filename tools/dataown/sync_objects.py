#!/usr/bin/env python3
"""Make config/objects/main.objects.json piece boundaries follow config/split/main.yaml (data sections)."""
import json, re
y = open('config/split/main.yaml').read()
p = 'config/objects/main.objects.json'; m = json.load(open(p))
h8 = lambda v: f'0x{v:08X}'
tus = {t.get('name'): t for t in m['tus']}
n = 0
for sec in ('.data', '.rodata', '.sdata', '.lit4'):
    order = m['sections'][sec]['order']
    for i, q in enumerate(order):
        mm = re.search(rf'\[0x([0-9A-F]+), {sec[1:]}, {re.escape(q["piece"])}\]', y)
        if not mm or i == 0:
            continue
        new = int(mm.group(1), 16) + 0x1FF000
        if new == int(q['start'], 16):
            continue
        prev = order[i - 1]
        prev['end'] = q['start'] = h8(new)
        for name, side in ((prev['tu'], 'end'), (q['tu'], 'start')):
            rec = tus.get(name, {}).get('sections', {}).get(sec)
            if not rec: continue
            rec[side] = h8(new)
            if rec.get('pieces'):
                rec['pieces'][-1 if side == 'end' else 0][side] = h8(new)
            rec['size'] = int(rec['end'], 16) - int(rec['start'], 16)
        print(f'{sec} {prev["tu"]}|{q["tu"]} -> {h8(new)}'); n += 1
open(p, 'w').write(json.dumps(m, indent=2, ensure_ascii=False) + '\n')
print(n, 'boundaries synced')

#!/usr/bin/env python3
"""Hand a TU's .bss/.sbss to its C object (or back to asm).

  own_bss.py UNIT/STEM SECTION [asm]        (checkout root)

MAIN: sets `owners[SECTION]` of the TU in config/objects/main.objects.json; the C link
then takes SECTION from the C object and mapcheck verifies every original symbol is
defined by the C object at its original address.
Overlays: switches the splat subsegment `{ type: bss, ... name: <ovl>/<stem> }` to
`.bss` (taken from the C object) in config/split/<ovl>.yaml.
Then: configure.py, ninja gate (or tools/dataown/report.sh).
"""
import json
import re
import sys
from pathlib import Path

unit, stem = sys.argv[1].split('/')
sec = sys.argv[2]
to = 'asm' if len(sys.argv) > 3 and sys.argv[3] == 'asm' else 'c'
assert sec in ('.bss', '.sbss')
root = Path.cwd()
if unit == 'main':
    p = root / 'config/objects/main.objects.json'
    m = json.loads(p.read_text())
    t = next(t for t in m['tus'] if t.get('name') == stem)
    assert sec in t['owners'], f'{stem} has no {sec}'
    t['owners'][sec] = to
    p.write_text(json.dumps(m, indent=2, ensure_ascii=False) + '\n')
else:
    p = root / f'config/split/{unit}.yaml'
    text = p.read_text()
    kind = sec.lstrip('.')
    src, dst = (kind, '.' + kind) if to == 'c' else ('.' + kind, kind)
    pat = re.compile(rf'(\{{ type: ){re.escape(src)}(, vram: 0x[0-9A-Fa-f]+, name: {unit}/{re.escape(stem)} \}})')
    text, n = pat.subn(lambda mm: mm.group(1) + dst + mm.group(2), text)
    assert n == 1, f'{n} {src} subsegments for {unit}/{stem}'
    p.write_text(text)
print(f'{unit}/{stem} {sec} -> {to}')

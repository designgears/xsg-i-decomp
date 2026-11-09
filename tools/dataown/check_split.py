#!/usr/bin/env python3
"""MAIN: every data subsegment start in config/split/main.yaml equals the TU's piece start in main.objects.json."""
import json, re, sys
y = open('config/split/main.yaml').read()
m = json.load(open('config/objects/main.objects.json'))
bad = 0
for sec in ('.data', '.rodata', '.sdata', '.lit4'):
    for p in m['sections'][sec]['order']:
        mm = re.search(rf'\[0x([0-9A-F]+), {sec[1:]}, {re.escape(p["piece"])}\]', y)
        if not mm:
            continue
        ya = int(mm.group(1), 16) + 0x1FF000
        if ya != int(p['start'], 16):
            print(f'MISMATCH {sec} {p["piece"]}: yaml {ya:#x} objects {p["start"]}'); bad += 1
print('split consistent' if not bad else f'{bad} mismatches'); sys.exit(1 if bad else 0)

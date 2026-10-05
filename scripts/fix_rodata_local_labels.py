#!/usr/bin/env python3
"""Rewrite splat jumptable `.word .L00XXXXXX` to absolute `.word 0x00XXXXXX`.

Legacy ee-as / modern GAS strips `.L*` locals even when `.globl`'d, so
`migrate_rodata_to_functions: False` jumptables in separate `*.rodata.s`
files leave undefined references at link.  The absolute VA form is
byte-identical in the image (the comment already records the VA) and
assembles cleanly.

Run after splat / before ninja:

  python3 -B scripts/fix_rodata_local_labels.py
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAT = re.compile(r'^(\s*\.word\s+)\.L([0-9A-Fa-f]{8})\b(.*)$')


def fix_file(path: Path) -> int:
    text = path.read_text(errors='replace')
    out, n = [], 0
    for line in text.splitlines(keepends=True):
        m = PAT.match(line.rstrip('\n'))
        if m:
            nl = '\n' if line.endswith('\n') else ''
            out.append(f'{m.group(1)}0x{m.group(2)}{m.group(3)}{nl}')
            n += 1
        else:
            out.append(line)
    if n:
        path.write_text(''.join(out))
    return n


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root', type=Path, default=ROOT)
    a = ap.parse_args()
    total, files = 0, 0
    for path in (a.root / 'build').glob('*/asm/**/*.rodata.s'):
        n = fix_file(path)
        if n:
            files += 1
            total += n
    print(f'rewrote {total} .word .L* site(s) across {files} file(s)')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

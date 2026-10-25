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
# Splat lines are often `/* VMA VA bytes */ .word .L00XXXXXX`.
PAT = re.compile(
    r'^(?P<prefix>\s*(?:/\*[^*]*\*/\s*)?\.word\s+)\.L(?P<va>[0-9A-Fa-f]{8})\b(?P<suffix>.*)$'
)


def fix_file(path: Path) -> int:
    text = path.read_text(errors='replace')
    out, n = [], 0
    for line in text.splitlines(keepends=True):
        m = PAT.match(line.rstrip('\n'))
        if m:
            nl = '\n' if line.endswith('\n') else ''
            out.append(f'{m.group("prefix")}0x{m.group("va")}{m.group("suffix")}{nl}')
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
    patterns = (
        '*/asm/**/*.rodata.s',
        '*/carve/**/*.rodata.s',  # data_carve split pieces also keep splat .L jumptables
    )
    seen: set[Path] = set()
    for pat in patterns:
        for path in (a.root / 'build').glob(pat):
            if path in seen:
                continue
            seen.add(path)
            n = fix_file(path)
            if n:
                files += 1
                total += n
    print(f'rewrote {total} .word .L* site(s) across {files} file(s)')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

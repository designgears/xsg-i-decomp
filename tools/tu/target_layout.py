"""Lay out a TARGET literal pool in the order the verified build links it.

A recovered C function emits its float/double literals into the compiler's own
anonymous `.lit4`/`.lit8`; the build then cuts that section into the declared
C-owned runs (config/tu/data-carves.json, tools/tu/data_carve.py) and pins
each run at its original address.  objdiff compares a literal load by section
and address, so a target that keeps splat's address order (`D_004D8690` at
target `.lit4+0x84`) never meets the compiler's `[.lit4]+0x8`, even though the
linked binary is byte-identical.

This pass rebuilds only the report-local target data object of one section:

* run offsets inside the compiled section are found by placing each declared
  run's ORIGINAL bytes (from the byte-identical linked ELF) in order, first
  fit; every compiled byte must be covered by a run or be zero alignment fill;
* the original data blocks (same labels, same bytes, from splat's data file)
  of each run are emitted at those offsets, the rest follow in address order;
* the assembled result must carry each run's original bytes at its offset and
  every run label at offset + (address - run start).

No compiler bytes, symbols or relocations are copied into the target; the
compiled section only decides where the declared runs sit.
"""
import json
from pathlib import Path
import re

from tu import data_carve
from tu.elfinfo import Elf


def va_bytes(elf, va, size):
    for s in elf.sections:
        if s.type != 8 and s.flags & 2 and s.addr <= va and va + size <= s.addr + s.size:
            return elf.data[s.offset + va - s.addr:s.offset + va - s.addr + size]
    return None


EQUAL = re.compile(r'\+0x([0-9a-fA-F]+)\.\.\+0x([0-9a-fA-F]+) of the C object equals the original '
                   r'0x([0-9a-fA-F]+)\.\.0x([0-9a-fA-F]+)')


def carved_spans(record, section, compiled, linked):
    """[(offset, address, bytes)] the build's split record links, or None.

    `data_carve.py split` writes `<obj>.carved.o.json`: each piece's offset in
    the compiled section and the original address it is pinned at, plus
    left-out spans whose compiled bytes equal an original (scaffold) item that
    the C references resolve to. Every span must hold the original bytes."""
    if record is None or not Path(record).is_file():
        return None
    entry = json.loads(Path(record).read_text()).get('sections', {}).get(section)
    if not entry:
        return None
    spans = [(int(p['offset'], 16), int(p['va'], 16), int(p['length'], 16)) for p in entry['pieces']]
    for note in entry.get('notes', []):
        m = EQUAL.search(note)
        if m:
            lo, hi, va, va_end = (int(x, 16) for x in m.groups())
            if hi - lo != va_end - va:
                return None
            spans.append((lo, va, hi - lo))
    placed = []
    for at, va, n in sorted(spans):
        want = va_bytes(linked, va, n)
        if not want or compiled[at:at + n] != want or (placed and at < placed[-1][0] + len(placed[-1][2])):
            return None
        placed.append((at, va, want))
    return placed


def run_offsets(compiled, runs, linked):
    """[(offset, start, bytes)] of each run in the compiled section, or None."""
    placed, cursor = [], 0
    for k, run in enumerate(runs):
        g0, g1 = (int(x, 16) for x in run['generated'])
        want = va_bytes(linked, g0, g1 - g0)
        if not want:
            return None
        at = compiled.find(want, cursor)
        while at >= 0 and at % 4:
            at = compiled.find(want, at + 1)
        if at < 0 or any(compiled[cursor:at]):
            return None
        placed.append((at, g0, want))
        cursor = at + len(want)
    if any(compiled[cursor:]):
        return None
    return placed


def reorder(sources, destination, runs, compiled, linked, carved=None, section=None):
    """Write one compiler-ordered copy of the data files `sources` (one TU
    section, ascending addresses); return proof or None."""
    placed = (carved_spans(Path(str(carved) + '.json'), section, compiled, linked) if carved else None) \
        or run_offsets(compiled, runs, linked)
    if not placed:
        return None
    header, blocks = None, []
    for source in sources:
        head, items = data_carve.parse_data_file(source)
        header = head if header is None else header
        blocks.extend(items)
    if not blocks or any(a['start'] >= b['start'] for a, b in zip(blocks, blocks[1:])):
        return None
    ends = [b['start'] for b in blocks[1:]] + [None]
    used, out, offset, labels = set(), list(header), 0, {}
    for at, g0, want in placed:
        g1 = g0 + len(want)
        chosen = [k for k, b in enumerate(blocks) if g0 <= b['start'] < g1]
        if not chosen or blocks[chosen[0]]['start'] != g0:
            return None
        last_end = ends[chosen[-1]] or g1     # last item of the file: verified below
        if last_end < g1 or at < offset:
            return None
        if at > offset:
            out.append(f'    .space 0x{at - offset:X}')
        for k in chosen:
            used.add(k)
            out.extend(line for line in blocks[k]['lines'] if not line.strip().startswith('.align'))
            for name in blocks[k]['labels']:
                labels[name] = at + blocks[k]['start'] - g0
        offset = at + (last_end - g0)
    for k, b in enumerate(blocks):
        if k not in used:
            out.extend(b['lines'])
    destination.write_text('\n'.join(out) + '\n')
    return dict(placed=[(at, len(want), want) for at, _, want in placed], labels=labels)


def verify(obj, section, proof, original_size):
    """Runs hold their original bytes at the compiled offsets, labels follow
    them, and the section keeps exactly the original size."""
    elf = Elf(Path(obj).read_bytes())
    secs = [s for s in elf.sections if s.name == section]
    if len(secs) != 1 or secs[0].size != original_size:
        return False
    data = elf.section_bytes(secs[0])
    if any(data[at:at + n] != want for at, n, want in proof['placed']):
        return False
    values = {s.name: s.value for s in elf.symbols if s.shndx == secs[0].index and s.name}
    return all(values.get(name) == value for name, value in proof['labels'].items())

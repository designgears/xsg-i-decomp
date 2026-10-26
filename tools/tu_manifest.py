#!/usr/bin/env python3
"""Total provisional recovery-TU analysis, never a matcher authorization.

prepare freezes legacy map evidence; generate reconstructs the canonical manifest;
check independently rebuilds it from pinned ELFs and compares the derived map.
All outputs are explicit; no original ELF, policy, queue, or source is written.
"""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

UNITS = ('main', 'ov01', 'ov02', 'ov10', 'ov11', 'ov12', 'iop_ssd', 'iop_rssd')
REVISION = 'slus-20469-412d448de315'
ENTRY = re.compile(r'^        ((?:main|ov\d\d|iop_ssd|iop_rssd):0x[0-9a-f]{8}) (.+)$')


def require(test, message):
    if not test:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, f'duplicate JSON key: {key}')
            result[key] = value
        return result
    return json.loads(path.read_text(), object_pairs_hook=unique)


def encoded(value):
    return (json.dumps(value, indent=2, sort_keys=True, ensure_ascii=True) + '\n').encode()


def write(path, data):
    require(not path.exists() and not path.is_symlink(), f'output must be fresh: {path}')
    require(not any(p.is_symlink() for p in path.parents), 'symlink output parent')
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as stream:
        stream.write(data)


def output_paths(root, destinations):
    resolved = []
    for path, filename in destinations:
        require(path is not None and path.name == filename, f'expected output filename: {filename}')
        require(not any(path.resolve().is_relative_to(root.resolve() / name)
                        for name in ('game', 'src', 'include', 'annotations', '.work/toolchains', '.work/envs')),
                'protected output tree')
        require(not path.exists() and not path.is_symlink(), f'output must be fresh: {path}')
        require(not any(p.is_symlink() for p in path.parents), 'symlink output parent')
        resolved.append(path.resolve())
    require(len(resolved) == len(set(resolved)), 'output collision')


def parse_map(text):
    entries = {}
    domain = group = None
    for line in text.splitlines():
        if not line or line.startswith('#'):
            continue
        match = ENTRY.fullmatch(line)
        if match:
            key, names = match.groups()
            require(key not in entries, f'duplicate legacy identity: {key}')
            require(domain is not None and group is not None, 'entry without category')
            entries[key] = dict(domain=domain, group=group, names=names.split(' | '))
        elif line.startswith('    ') and not line.startswith('        '):
            group = line.strip()
        elif not line.startswith(' ') and line.endswith('/'):
            domain, group = line[:-1], None
        else:
            raise ValueError(f'unrecognized legacy map line: {line}')
    return entries


def debug_records(data, section):
    """Decode little-endian ECOFF HDRR/FDR/SYMR with absolute file offsets.

    Only exact stProc/stStaticProc name+address agreement is ownership evidence.
    Header strings and neighboring procedures alone never assign ELF functions.
    """
    start, size = section['file_offset'], section['size']
    end = start + size

    def span(offset, length):
        require(start <= offset <= end and 0 <= length <= end - offset,
                'mdebug table outside pinned section')
        return data[offset:offset + length]

    header = struct.unpack('<HH23I', span(start, 96))
    require(header[0] == 0x7009, 'unsupported mdebug magic')
    h = header[2:]
    span(h[18], h[17] * 72)
    span(h[8], h[7] * 12)
    strings = span(h[14], h[13])
    records = []
    for index in range(h[17]):
        position = h[18] + index * 72
        fd = struct.unpack('<10I2H7I', span(position, 72))
        require(fd[4] + fd[5] <= h[7] and fd[2] + fd[3] <= h[13],
                'mdebug FDR table bounds')
        local = strings[fd[2]:fd[2] + fd[3]]

        def name_at(offset):
            require(0 <= offset < len(local), 'mdebug string offset')
            stop = local.find(b'\0', offset)
            require(stop >= offset, 'unterminated mdebug string')
            return local[offset:stop].decode('ascii')

        source = name_at(fd[1])
        symbols = []
        for j in range(fd[5]):
            pos = h[8] + (fd[4] + j) * 12
            iss, value, bits = struct.unpack('<III', span(pos, 12))
            kind, storage = bits & 63, (bits >> 6) & 31
            if kind in (1, 2, 6, 14):
                symbols.append(dict(name=name_at(iss), address=value, kind=kind,
                                    storage=storage, file_offset=pos, raw_bits=bits))
        records.append(dict(index=index, source=source, fdr_offset=position,
                            language_code=(fd[16] & 31), symbols=symbols))
    return dict(magic=header[0], version=header[1], file_count=h[17],
                local_symbol_count=h[7], procedure_count=h[5], records=records)


def prepare(root):
    text = (root / '.work/function-map.txt').read_text()
    records = parse_map(text)
    paths = ['.work/attempts/function-map-effects-audio-06/manifest.json',
             '.work/attempts/function-map-effects-audio-06/evidence.json',
             '.work/attempts/function-map-annotations-05/manifest.json',
             '.work/attempts/function-map-annotations-05/evidence.json']
    pins = {p: sha((root / p).read_bytes()) for p in paths}
    outcomes = load(root / paths[-1])['outcomes']
    return dict(schema_version=1, original_text=text, original_sha256=sha(text.encode()),
                entries=records, provenance_pins=pins, annotation_outcomes=outcomes)


def collect(root):
    # Existing strict EE reader is deliberately not used for the IRX originals.
    sys.path.insert(0, str(root))
    from tools.inventory import ELF, build_inventory
    from tools.iop_inventory import verify_iop
    from tools.verify_project import verify_project
    verify_project(root)
    targets = load(root / 'config/targets.json')
    iop = verify_iop(root)
    pins = targets['units'] | load(root / 'config/iop-targets.json')['units']
    require(set(pins) == set(UNITS), 'required eight-unit set')
    inventory = build_inventory(root / 'game/extracted-iso', root / 'annotations')
    functions = {}
    binaries, sections, symbols = {}, {}, {}
    for unit in UNITS:
        binary = (root / 'game/extracted-iso' / pins[unit]['path']).read_bytes()
        require(sha(binary) == pins[unit]['sha256'], f'binary pin: {unit}')
        binaries[unit] = binary
        if unit.startswith('iop_'):
            record = iop['units'][unit]
            sections[unit] = [dict(s, va=f"0x{s['module_offset']:08x}") for s in record['sections']]
            symbols[unit] = [dict(s, va=f"0x{s['value']:08x}", type=s['info'] & 15)
                             for s in record['symbols']]
            # Include every defined FUNC, including zero-sized symbols if present.
            for s in symbols[unit]:
                if s['type'] != 2 or not 0 < s['section_index'] < len(sections[unit]):
                    continue
                sec = sections[unit][s['section_index']]
                delta = int(s['va'], 16) - int(sec['va'], 16)
                require(sec['flags'] & 4 and 0 <= delta <= sec['size'] and
                        delta + s['size'] <= sec['size'], 'IRX FUNC extent')
                key = unit + ':' + s['va']
                f = functions.setdefault(key, dict(unit=unit, va=s['va'], size=s['size'],
                    section=sec['name'], section_index=sec['index'], file_unit=unit,
                    file_offset=sec['file_offset'] + delta, names=[], symbols=[]))
                require(f['size'] == s['size'], 'conflicting IRX alias size')
                f['names'].append(s['name'])
                f['symbols'].append(s)
                f['sha256'] = sha(binary[f['file_offset']:f['file_offset'] + f['size']])
        else:
            elf = ELF(binary, unit)
            sections[unit], symbols[unit] = elf.sections, elf.symbols
    functions.update(inventory['functions'])
    return pins, functions, binaries, sections, symbols, inventory, iop


def build(root, legacy):
    require(sha(legacy['original_text'].encode()) == legacy['original_sha256'], 'legacy text pin')
    require(parse_map(legacy['original_text']) == legacy['entries'], 'legacy entry preservation')
    for path, digest in legacy['provenance_pins'].items():
        require(sha((root / path).read_bytes()) == digest, f'legacy provenance pin: {path}')
    pins, funcs, binaries, sections, symbols, inventory, iop = collect(root)
    old = legacy['entries']
    internal = {o['canonical_entry']: o for o in legacy['annotation_outcomes']
                if o['outcome'] == 'annotation-labelled-internal-entry'}
    require(set(old) == set(funcs) | set(internal), 'missing/extra legacy or ELF identity')
    input_paths = ['config/targets.json', 'config/iop-targets.json', 'config/iop-tools.lock.json',
                   'config/split-tools.lock.json', 'config/scaling-policy.json',
                   'tools/inventory.py', 'tools/iop_inventory.py', 'tools/verify_project.py',
                   'tools/verify_target.py', iop['reader']['path'],
                   '.work/envs/split-tools/lib/python3.14/site-packages/spimdisasm/mips/sections/MipsSectionText.py']
    input_paths += sorted(p.relative_to(root).as_posix() for p in (root / 'annotations').rglob('*.csv'))
    inputs = {p: sha((root / p).read_bytes()) for p in input_paths}
    tus, ownership, audit = {}, {}, {}

    def group(identifier, unit, confidence, evidence, source=None):
        record = dict(unit=unit, historical_confidence=confidence, evidence=evidence,
                      source=source, language='unresolved', matcher_ready=False,
                      matcher_blockers=['Separate packet, semantics, language, ABI/toolchain and exact comparison review required'],
                      functions=[], code_spans=[], data_ownership=[], references=[])
        if identifier in tus:
            require(tus[identifier]['unit'] == unit, 'cross-unit TU merge')
        else:
            tus[identifier] = record
        return identifier

    # Current accepted/project source boundaries have precedence over heuristics.
    for path in sorted((root / 'config/units').glob('*.json')):
        packet = load(path)
        unit, source = packet['unit'], packet['source']['path']
        source_bytes = (root / source).read_bytes()
        require(sha(source_bytes) == packet['source']['sha256'], f'source packet pin: {source}')
        rel = path.relative_to(root).as_posix()
        inputs[rel], inputs[source] = sha(path.read_bytes()), sha(source_bytes)
        identifier = unit + '/' + Path(source).stem
        # Preserve established queue TU spelling where it differs from the source basename.
        if source == 'src/script/cf_time.c':
            identifier = 'main/script-cf-time'
        elif source == 'src/menu/load_state.c':
            identifier = 'main/menu-load-state'
        elif source == 'src/resource/load_mode.c':
            identifier = 'main/srs-load-mode'
        g = group(identifier, unit, 'existing-project-TU-not-original-language-proof',
                  [dict(kind='current-unit-packet', path=rel, sha256=inputs[rel])], source)
        tus[g]['recovered_language'] = 'c' if source.endswith('.c') else 'unresolved'
        tus[g]['references'] = packet.get('references', {})
        tus[g]['generated_data_contract'] = packet.get('generated_data', [])
        for f in packet['functions']:
            key = unit + ':' + f['va']
            require(key in funcs and key not in ownership, 'packet function missing/duplicate')
            require(f['name'] in funcs[key]['names'] and f['size'] == funcs[key]['size'], 'packet extent/name')
            ownership[key] = g

    for unit in UNITS:
        debug_sections = [s for s in sections[unit] if any(t in s['name'].lower()
                          for t in ('debug', 'stab', 'dwarf'))]
        audit[unit] = dict(sections=sections[unit],
            debug_sections=debug_sections,
            file_symbols=[s for s in symbols[unit] if s['type'] == 4],
            section_symbols=[s for s in symbols[unit] if s['type'] == 3],
            language_evidence='No language inferred from plain names, domain or native Java/JNI prefixes',
            mdebug=None)
        for sec in debug_sections:
            if sec['name'] != '.mdebug' or not sec['size']:
                continue
            decoded = debug_records(binaries[unit], sec)
            audit[unit]['mdebug'] = decoded
            candidates = defaultdict(list)
            for fd in decoded['records']:
                for s in fd['symbols']:
                    key = f"{unit}:0x{s['address']:08x}"
                    if s['kind'] in (6, 14) and s['storage'] == 1 and key in funcs and s['name'] in funcs[key]['names']:
                        candidates[key].append((fd, s))
            for key, matches in candidates.items():
                require(len({fd['index'] for fd, s in matches}) == 1, 'ambiguous debug file ownership')
                fd, s = matches[0]
                g = group(f"{unit}/debug-file-{fd['index']:03d}", unit,
                    'attested-debug-file-association-not-complete-original-TU',
                    [dict(kind='ECOFF-FDR-exact-procedure-name-address', source=fd['source'],
                          fdr_offset=fd['fdr_offset'], section_sha256=sec['sha256'],
                          language_code=fd['language_code'])])
                require(key not in ownership, 'debug/current source ownership conflict')
                ownership[key] = g
            # Associate data only when both ECOFF and ELF attest the same named
            # address. A source filename or proximity is not enough.
            for fd in decoded['records']:
                g = f"{unit}/debug-file-{fd['index']:03d}"
                for ds in fd['symbols']:
                    if ds['kind'] not in (1, 2):
                        continue
                    matches = [s for s in symbols[unit] if s['type'] == 1 and s['size'] > 0
                               and s['name'] == ds['name'] and int(s['va'], 16) == ds['address']
                               and 0 < s['section_index'] < len(sections[unit])]
                    for s in matches:
                        data_sec = sections[unit][s['section_index']]
                        if not data_sec['flags'] & 2 or data_sec['flags'] & 4:
                            continue
                        delta = ds['address'] - int(data_sec['va'], 16)
                        require(0 <= delta and delta + s['size'] <= data_sec['size'], 'debug data extent')
                        group(g, unit, 'attested-debug-file-association-not-complete-original-TU',
                              [dict(kind='ECOFF-FDR-exact-data-name-address', source=fd['source'],
                                    fdr_offset=fd['fdr_offset'], section_sha256=sec['sha256'])])
                        item = dict(name=s['name'], address=s['va'], size=s['size'],
                            section_index=s['section_index'], symbol_id=s['id'],
                            debug_symbol_offset=ds['file_offset'],
                            evidence='Exact ECOFF data symbol and ELF OBJECT agreement; not generated-data matching',
                            sha256=None if data_sec['type'] == 8 else sha(binaries[unit][
                                data_sec['file_offset'] + delta:data_sec['file_offset'] + delta + s['size']]))
                        if item not in tus[g]['data_ownership']:
                            tus[g]['data_ownership'].append(item)

    # Remaining regions use bounded recovery batches, not claimed historical TUs.
    # Alignment after a zero-filled inter-FUNC gap is a weak compiler-layout hint.
    # Ignore category and prefix changes so one recovery TU can cross domain views.
    for unit in UNITS:
        remaining = sorted((k for k, f in funcs.items() if f['unit'] == unit and k not in ownership),
                           key=lambda k: int(funcs[k]['va'], 16))
        current, previous, first, count = None, None, 0, 0
        for key in remaining:
            f = funcs[key]
            address = int(f['va'], 16)
            hint = None
            if previous:
                p = funcs[previous]
                end = int(p['va'], 16) + p['size']
                same = p['section_index'] == f['section_index'] and p['file_unit'] == f['file_unit']
                gap = address - end
                if same and 0 < gap <= 64 and address % 16 == 0:
                    payload = binaries[unit][p['file_offset'] + p['size']:f['file_offset']]
                    if len(payload) == gap and not any(payload):
                        hint = dict(kind='zero-gap-16-byte-alignment-heuristic', start=end, size=gap,
                                    sha256=sha(payload), historical_boundary_proven=False)
            split = current is None or count >= 64 or address - first >= 16384
            if previous:
                p = funcs[previous]
                split |= p['section_index'] != f['section_index']
                # Do not bridge already-owned intervals, including existing project TUs.
                split |= any(x in ownership for x, q in funcs.items() if q['unit'] == unit
                             and int(p['va'], 16) < int(q['va'], 16) < address)
            if hint and count >= 8:
                split = True
            if split:
                evidence = [dict(kind='fallback-recovery-boundary', max_functions=64,
                                 target_max_address_span=16384, minimum_before_hint=8,
                                 ownership_proof=False)]
                if hint:
                    evidence.append(hint)
                current = group(f'{unit}/recovery-{address:08x}', unit, 'provisional-unresolved', evidence)
                first, count = address, 0
            ownership[key] = current
            count += 1
            previous = key

    entries = []
    for key, f in sorted(funcs.items()):
        unit = f['unit']
        g = ownership[key]
        require(tus[g]['unit'] == unit, 'cross-unit ownership')
        require(set(f['names']) <= set(old[key]['names']), f'legacy lost ELF alias: {key}')
        entry = dict(key=key, revision=REVISION, binary_sha256=pins[unit]['sha256'], unit=unit,
                     address=f['va'], address_kind='module_relative' if unit.startswith('iop_') else 'va',
                     tu=g, kind='elf-function', size=f['size'], code_sha256=f['sha256'],
                     original_names=sorted(set(f['names'])), view=old[key],
                     symbol_records=f['symbols'], file_unit=f['file_unit'], file_offset=f['file_offset'],
                     section_index=f['section_index'])
        entries.append(entry)
        tus[g]['functions'].append(key)
        tus[g]['code_spans'].append(dict(function=key, address=f['va'], size=f['size'],
                                       sha256=f['sha256'], file_offset=f['file_offset']))
    for key, outcome in sorted(internal.items()):
        owner = outcome['owner']
        require(owner in ownership and key not in funcs, 'invalid internal owner')
        unit, address = key.split(':')
        f = funcs[owner]
        require(unit == f['unit'] and int(f['va'], 16) < int(address, 16) < int(f['va'], 16) + f['size'],
                'internal entry outside owner')
        entries.append(dict(key=key, revision=REVISION, binary_sha256=pins[unit]['sha256'],
            unit=unit, address=address, address_kind='va', kind='internal-entry',
            owner=owner, tu=ownership[owner], view=old[key], standalone_credit=False,
            provenance=outcome))
    # Subtract only demonstrated OBJECT ownership; all residual data remains unresolved.
    unresolved = {u: [] for u in UNITS}
    for unit in UNITS:
        for sec in sections[unit]:
            if not sec['flags'] & 2 or sec['flags'] & 4 or not sec['size']:
                continue
            start = int(sec['va'], 16)
            stop = start + sec['size']
            owned = sorted((int(d['address'], 16), int(d['address'], 16) + d['size'], g)
                           for g, t in tus.items() if t['unit'] == unit
                           for d in t['data_ownership'] if d['section_index'] == sec['index'])
            cursor = start
            for left, right, g in owned:
                require(cursor <= left, f'overlapping debug data ownership: {unit}:{left:x}')
                if cursor < left:
                    unresolved[unit].append(dict(section_index=sec['index'], section=sec['name'],
                        address=f'0x{cursor:08x}', size=left-cursor, reason='unresolved source ownership'))
                cursor = right
            if cursor < stop:
                unresolved[unit].append(dict(section_index=sec['index'], section=sec['name'],
                    address=f'0x{cursor:08x}', size=stop-cursor, reason='unresolved source ownership'))
    unresolved_exec = {u: [] for u in UNITS}
    for unit in UNITS:
        for sec in sections[unit]:
            if not sec['flags'] & 2 or not sec['flags'] & 4 or not sec['size']:
                continue
            cursor = int(sec['va'], 16)
            stop = cursor + sec['size']
            # Use this binary's raw symbol extents, not canonical file_unit:
            # main's embedded OV02 is a separately pinned section alias.
            bodies = sorted((int(s['va'], 16), int(s['va'], 16) + s['size'])
                            for s in symbols[unit] if s['type'] == 2 and s['size']
                            and s['section_index'] == sec['index'])
            for left, right in bodies + [(stop, stop)]:
                require(int(sec['va'], 16) <= left <= right <= stop, 'raw function section extent')
                if cursor < left:
                    offset = sec['file_offset'] + cursor - int(sec['va'], 16)
                    unresolved_exec[unit].append(dict(section_index=sec['index'], section=sec['name'],
                        address=f'0x{cursor:08x}', size=left-cursor, file_offset=offset,
                        sha256=sha(binaries[unit][offset:offset + left-cursor]),
                        reason='Unowned code, inline data or padding; executable section flags do not prove code'))
                cursor = max(cursor, right)
    result = dict(schema_version=1, revision=REVISION, analysis_only=True,
        identity_rule='revision + binary_sha256 + unit + address; embedded EE aliases explicit only',
        coverage_scope='All defined ELF STT_FUNC canonical identities, not undiscovered/stripped code or OBJECT import veneers',
        legacy_sha256=sha(encoded(legacy)), inputs=inputs, units={u: dict(path=pins[u]['path'],
             sha256=pins[u]['sha256'], address_kind='module_relative' if u.startswith('iop_') else 'va') for u in UNITS},
        entries=sorted(entries, key=lambda e: e['key']), tus=tus, metadata_audit=audit,
        unresolved_data=unresolved, unresolved_executable_spans=unresolved_exec,
        iop_relocations={u: r['relocations'] for u, r in iop['units'].items()},
        section_aliases=inventory['section_aliases'], function_aliases=inventory['function_aliases'],
        excluded_elf_symbols={u: r['excluded_functions'] for u, r in inventory['units'].items()},
        boundary_method='Exact existing packet, then exact ECOFF file/procedure association, then bounded provisional text regions; no category or prefix ownership inference',
        split_tool_use='Existing pinned splat/spimdisasm conceptual alignment heuristic only; no quickstart or IRX conversion. No rodata pairing asserted without ownership evidence.',
        non_claims=['No new matching credit', 'No checkpoint reset', 'No matcher readiness',
                    'No original complete TU/language recovery', 'No guarantee against future technical blockers'])
    result['counts'] = dict(elf_functions=len(funcs), internal_entries=len(internal),
        map_entries=len(entries), tus=len(tus), by_unit=dict(Counter(f['unit'] for f in funcs.values())),
        by_confidence=dict(Counter(tus[ownership[k]]['historical_confidence'] for k in funcs)))
    validate(result, legacy)
    return result


def validate(manifest, legacy):
    require(set(manifest['units']) == set(UNITS), 'unit coverage')
    seen, assigned = set(), []
    by_key = {e['key']: e for e in manifest['entries']}
    for entry in manifest['entries']:
        key, unit = entry['key'], entry['unit']
        require(key not in seen, 'duplicate identity')
        seen.add(key)
        require(key == unit + ':' + entry['address'], 'identity/unit mismatch')
        require(entry['revision'] == REVISION and entry['binary_sha256'] == manifest['units'][unit]['sha256'], 'identity pin')
        require(entry['address_kind'] == manifest['units'][unit]['address_kind'], 'address kind')
        require(entry['view'] == legacy['entries'][key], 'legacy aliases/category lost')
        require(entry['tu'] in manifest['tus'] and manifest['tus'][entry['tu']]['unit'] == unit, 'cross-unit TU')
        if entry['kind'] == 'internal-entry':
            owner = by_key[entry['owner']]
            require(owner['kind'] == 'elf-function' and owner['tu'] == entry['tu'] and owner['unit'] == unit,
                    'internal entry ownership')
            require(entry['standalone_credit'] is False, 'internal standalone credit')
        else:
            require(entry['kind'] == 'elf-function', 'entry kind')
    for name, tu in manifest['tus'].items():
        require((tu['functions'] or tu['data_ownership']) and tu['matcher_ready'] is False, 'empty TU or false readiness')
        for key in tu['functions']:
            require(key in by_key and by_key[key]['kind'] == 'elf-function' and by_key[key]['tu'] == name,
                    'TU membership mismatch')
            assigned.append(key)
    expected = {k for k, e in by_key.items() if e['kind'] == 'elf-function'}
    require(len(assigned) == len(set(assigned)) and set(assigned) == expected, 'missing/duplicate function assignment')
    require(seen == set(legacy['entries']), 'total legacy coverage')


def render(manifest):
    lines = [f'# Function Map: {manifest["revision"]}',
             '# Generated from config/tu-manifest.json by tools/tu_manifest.py.',
             '# Manifest SHA256: ' + sha(encoded(manifest)),
             '# Domain/group is the preserved category view, NOT TU ownership.',
             '# TU metadata precedes each entry. One TU may cross categories.',
             '# Provisional analysis only; no matching readiness/credit or checkpoint reset.',
             '# EE addresses are VAs; IOP addresses are module-relative offsets, including zero.',
             '# Original symbols and annotation aliases retain separate provenance in manifest.',
             '# Counts: ' + json.dumps(manifest['counts'], sort_keys=True)]
    for unit, record in sorted(manifest['units'].items()):
        lines.append(f'# {unit} {record["sha256"]}')
    views = defaultdict(list)
    for e in manifest['entries']:
        views[(e['view']['domain'], e['view']['group'])].append(e)
    domain = None
    for (d, group), entries in sorted(views.items()):
        if d != domain:
            lines.extend(['', d + '/'])
            domain = d
        lines.append('    ' + group)
        for e in sorted(entries, key=lambda e: e['key']):
            tu = manifest['tus'][e['tu']]
            suffix = ' owner=' + e['owner'] + ' standalone_credit=false' if e['kind'] == 'internal-entry' else ''
            lines.append(f'# tu={e["tu"]} confidence={tu["historical_confidence"]} kind={e["kind"]}{suffix}')
            lines.append('        ' + e['key'] + ' ' + ' | '.join(e['view']['names']))
    return ('\n'.join(lines) + '\n').encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['prepare', 'generate', 'check', 'summary'])
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--legacy', type=Path, required=True)
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--map', type=Path)
    args = parser.parse_args()
    try:
        if args.command == 'summary':
            saved = load(args.manifest)
            print(json.dumps(dict(counts=saved['counts'], metadata={u: dict(
                debug_sections=[(s['name'], s['size']) for s in a['debug_sections']],
                file_symbols=len(a['file_symbols']), section_symbols=len(a['section_symbols']),
                debug_files=[] if a['mdebug'] is None else [(f['source'], f['language_code']) for f in a['mdebug']['records']])
                for u, a in saved['metadata_audit'].items()},
                group_sizes=dict(Counter(len(t['functions']) for t in saved['tus'].values())),
                tu_kinds=dict(Counter(t['historical_confidence'] for t in saved['tus'].values())),
                data_objects=sum(len(t['data_ownership']) for t in saved['tus'].values()),
                unresolved_executable_bytes={u: sum(s['size'] for s in spans) for u, spans in saved['unresolved_executable_spans'].items()},
                unresolved_data_bytes={u: sum(s['size'] for s in spans) for u, spans in saved['unresolved_data'].items()}), indent=2))
        elif args.command == 'prepare':
            output_paths(args.root, [(args.legacy, 'tu-map-legacy.json')])
            write(args.legacy, encoded(prepare(args.root)))
        else:
            require(args.manifest is not None and args.map is not None, '--manifest and --map required')
            if args.command == 'generate':
                output_paths(args.root, [(args.manifest, 'tu-manifest.json'), (args.map, 'function-map.txt')])
            legacy = load(args.legacy)
            result = build(args.root, legacy)
            if args.command == 'generate':
                write(args.manifest, encoded(result))
                write(args.map, render(result))
            else:
                saved = load(args.manifest)
                validate(saved, legacy)
                require(args.manifest.read_bytes() == encoded(result), 'manifest differs from fresh ELF reconstruction')
                require(args.map.read_bytes() == render(saved), 'map differs from canonical manifest')
                require(sha((args.root / '.work/function-map.txt').read_bytes()) in
                        (legacy['original_sha256'], sha(render(saved))), 'baseline map changed; rebase required')
            print(json.dumps(dict(ok=True, counts=result['counts']), sort_keys=True))
        return 0
    except (OSError, ValueError, KeyError, TypeError, struct.error) as exc:
        print(json.dumps(dict(ok=False, error=str(exc))))
        return 1


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3
"""Per-TU data ownership helper.  Run from the checkout root after `source scripts/env.sh`.

  tu_data.py status UNIT/STEM      list the TU's data items and which are C-owned
  tu_data.py carve  UNIT/STEM      build the C object, derive + write carve runs, show status
"""
import json, re, subprocess, sys, glob
from pathlib import Path
ROOT = Path.cwd()
PY = str(ROOT / '.venv/bin/python')

def tu_info(name):
    unit, stem = name.split('/')
    if unit == 'main':
        m = json.load(open(ROOT / 'config/objects/main.objects.json'))
        t = next(t for t in m['tus'] if t.get('name') == stem)
        return unit, stem, t['id'], 'build/main', f'build/main/build/c/{stem}.o', sorted(glob.glob(f'build/main/asm/main/data/{stem}.*.s') + glob.glob(f'build/main/asm/main/data/{stem}__*.s'))
    m = json.load(open(ROOT / f'build/{unit}/compile-manifest.json'))
    t = next(t for t in m['units'][unit]['tus'] if t['name'] == stem)
    obj = f'build/{unit}/build/src/{unit}/{stem}.o'
    nin = (ROOT / f'build/{unit}/build.ninja').read_text()
    if f'build build/scaffold/src/{unit}/{stem}.o:' in nin:
        obj = f'build/{unit}/build/scaffold/src/{unit}/{stem}.o'
    return unit, stem, t['id'], f'build/{unit}', obj, sorted(glob.glob(f'build/{unit}/asm/data/{unit}/{stem}.*.s') + glob.glob(f'build/{unit}/asm/data/{unit}/{stem}__*.s'))

def items(files):
    out = []
    for f in files:
        sec = None
        for l in open(f):
            m = re.match(r'\.section (\.\w+)', l)
            if m: sec = m.group(1)
            m = re.match(r'dlabel (\S+)', l)
            if m: out.append([sec, m.group(1), 0, 0, Path(f).name])
            m = re.search(r'/\* \S+ ([0-9A-F]{8}) [0-9A-F]* \*/ \.(word|short|byte|asciz|ascii|space|float|double)', l)
            if m and out and out[-1][3] == 0: out[-1][3] = int(m.group(1), 16)
            m2 = re.search(r'\*/ \.(word|short|byte|float|double|space) (.*)', l)
            if m2 and out:
                k, v = m2.groups(); out[-1][2] += {'word':4,'float':4,'short':2,'byte':1,'double':8}.get(k) or (int(v.split(',')[0], 0) if k=='space' else 0)
            m3 = re.search(r'\*/ \.(asciz|ascii) "', l)
            if m3 and out: out[-1][2] += -1  # marker: string item, size unknown from line
    return out

def status(name):
    unit, stem, tu, udir, obj, files = tu_info(name)
    reg = json.load(open(ROOT / 'config/tu/data-carves.json'))['tus'].get(tu, {})
    owned = {s for runs in reg.values() for r in runs for s in r.get('c_owned_symbols', [])}
    print(f'{name}  tu={tu}  obj={obj}')
    left = 0
    for sec, lab, size, va, f in items(files):
        if sec in ('.bss', '.sbss'): continue
        mark = 'C  ' if lab in owned else 'ASM'
        if lab not in owned: left += 1
        print(f'  {mark} {sec:8} {lab:40} va=0x{va:08X} ~{size if size>=0 else "str"} bytes  ({f})')
    print(f'  -> {left} initialized data item(s) still asm-owned')
    return left

def carve(name):
    unit, stem, tu, udir, obj, files = tu_info(name)
    subprocess.run([PY, '-B', 'configure.py', '--quiet'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    rel = obj[len(udir) + 1:]
    r = subprocess.run(['ninja', '-C', udir, rel], capture_output=True, text=True)
    if r.returncode: print(r.stdout[-3000:], r.stderr[-2000:]); sys.exit(1)
    src = f'src/{unit}/{stem}.c'
    r = subprocess.run([PY, '-B', 'tools/tu/data_carve.py', 'derive', '--root', '.', '--unit', unit, '--unit-dir', udir,
                        '--tu', tu, '--source', src, '--obj', obj, '--write', '--json-out', f'/tmp/carve-{unit}-{stem}.json'],
                       capture_output=True, text=True)
    g = json.load(open(f'/tmp/carve-{unit}-{stem}.json'))
    print('written:', g.get('written'), ' problems:', g.get('problems'))
    for sec, info in (g.get('sections') or {}).items():
        b = info.get('bytes', {})
        if b and not b.get('equal', True): print(f'  BYTES DIFFER in {sec}: {b.get("differing_words")[:8]}')
    subprocess.run([PY, '-B', 'configure.py', '--quiet'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    status(name)

if __name__ == '__main__':
    {'status': status, 'carve': carve}[sys.argv[1]](sys.argv[2])

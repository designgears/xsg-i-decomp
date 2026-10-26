#!/usr/bin/env python3
"""Audit callee declarations against the target's call sites.

For every INCLUDE_ASM function still in a TU, each `jal callee` in its target
asm is checked against the prototype the TU's own compile sees (cc1 -aux-info
on the build's preprocessed .i):

  RET_USED_VOID   callee declared void, but the caller reads $v0/$f0 after the call
  RET_FLOAT       callee declared int-returning, caller reads $f0 and not $v0
  EXTRA_ARGS      caller loads more int/float argument registers than the
                  prototype has parameters (register written, not read again
                  before the jal, so it is live into the call)
  IMPLICIT        no declaration visible (implicit int, double promotion)
  OLDSTYLE        K&R / unprototyped declaration

Findings are evidence, not proof: each fix still goes through the gate.

usage: proto_audit.py [--unit U] [--tu T] [--json out.json]
"""
import argparse, json, os, re, subprocess, sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CC1 = None
OUTDIR = os.path.join(ROOT, 'build/auto_decomp/loopstate/protoaudit')

STORES = {'sb', 'sh', 'sw', 'sd', 'sq', 'swc1', 'sdc1', 'swl', 'swr', 'sdl', 'sdr', 'sqc2'}
NODEST = STORES | {'beq', 'bne', 'beql', 'bnel', 'bnez', 'beqz', 'bnezl', 'beqzl', 'bgez', 'bgtz',
                   'blez', 'bltz', 'bgezl', 'bgtzl', 'blezl', 'bltzl', 'jr', 'j', 'b', 'bc1t',
                   'bc1f', 'bc1tl', 'bc1fl', 'c.eq.s', 'c.lt.s', 'c.le.s', 'c.olt.s', 'c.ole.s',
                   'teq', 'tne', 'break', 'syscall', 'sync', 'nop', 'mthi', 'mtlo', 'cache',
                   'ctc1', 'ctc2', 'qmtc2', 'mtsa', 'pref', 'jal'}
SRC_FIRST_DEST_SECOND = {'mtc1', 'dmtc1', 'qmtc2', 'ctc1'}
TWO_OP_HILO = {'mult', 'multu', 'div', 'divu', 'mult1', 'multu1', 'div1', 'divu1', 'madd', 'maddu'}
END = {'jr', 'j', 'b', 'jalr', 'eret'}
INT_ARGS = ['$4', '$5', '$6', '$7', '$8', '$9', '$10', '$11']
FLT_ARGS = ['$f%d' % i for i in range(12, 20)]
INS = re.compile(r'^\s*/\*[^*]*\*/\s+([a-z0-9.]+)\s*(.*?)\s*$')
REG = re.compile(r'\$(?:f?\d+|[a-z]{2}\d?)')


def parse_s(path):
    out = []
    for line in open(path, errors='replace'):
        m = INS.match(line)
        if m:
            op, rest = m.group(1), m.group(2)
            ops = [o.strip() for o in rest.split(',')] if rest else []
            out.append(('I', op, ops))
        elif re.match(r'^\s*(\.L[0-9A-Fa-f_]+|glabel|jlabel|alabel)', line) or re.match(r'^\.?L\w*:', line.strip()):
            out.append(('L', line.strip(), []))
    return out


def dest_src(op, ops):
    regs = [REG.findall(o) for o in ops]
    if op in SRC_FIRST_DEST_SECOND:
        return (regs[1][0] if len(regs) > 1 and regs[1] else None), [r for r in regs[0]]
    if op in NODEST or (op in TWO_OP_HILO and len(ops) == 2):
        return None, [r for rs in regs for r in rs]
    if op == 'jalr':
        return '$31', [r for rs in regs for r in rs]
    if not regs or not regs[0]:
        return None, [r for rs in regs for r in rs]
    return regs[0][0], [r for rs in regs[1:] for r in rs]


def call_sites(ins):
    """yield (callee, set_int_args, set_flt_args, reads_v0, reads_f0)"""
    n = len(ins)
    for i, (k, op, ops) in enumerate(ins):
        if k != 'I' or op != 'jal' or not ops:
            continue
        callee = ops[0]
        # argument registers live into the call: delay slot + backward window
        written, readafter = {}, set()
        win = []
        def ctl(x):
            return x[0] == 'I' and (x[1] in END or x[1] in ('jal', 'jalr') or (x[1].startswith('b') and x[1] != 'break'))
        j = i - 1
        while j >= 0 and ins[j][0] == 'I' and not ctl(ins[j]) and not (j > 0 and ctl(ins[j - 1])):
            win.insert(0, ins[j])
            j -= 1
        if i + 1 < n and ins[i + 1][0] == 'I':
            win.append(ins[i + 1])
        # walk window forward: a reg is an arg if its last write is not followed by a read
        live = {}
        for (_, o, a) in win:
            d, s = dest_src(o, a)
            for r in s:
                if r in live:
                    live[r] = 'read'
            if d:
                live[d] = 'w'
        ia = [r for r in INT_ARGS if live.get(r) == 'w']
        fa = [r for r in FLT_ARGS if live.get(r) == 'w']
        # return value use: scan forward from after the delay slot
        rv = rf = False
        j = i + 2
        while j < n:
            k2, o, a = ins[j]
            if k2 == 'L':
                break
            d, s = dest_src(o, a)
            if '$2' in s:
                rv = True
                break
            if '$f0' in s:
                rf = True
                break
            if d in ('$2', '$f0'):
                break
            if o == 'jal':
                # the next call's delay slot may read $2
                if j + 1 < n and ins[j + 1][0] == 'I':
                    d2, s2 = dest_src(ins[j + 1][1], ins[j + 1][2])
                    rv = '$2' in s2
                    rf = '$f0' in s2
                break
            if o in END or (o.startswith('b') and o != 'break'):
                if j + 1 < n and ins[j + 1][0] == 'I':
                    d2, s2 = dest_src(ins[j + 1][1], ins[j + 1][2])
                    rv = rv or '$2' in s2
                    rf = rf or '$f0' in s2
                break
            j += 1
        yield callee, ia, fa, rv, rf


def split_args(s):
    s = s.strip()
    if s in ('void', ''):
        return []
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch in '([':
            depth += 1
        elif ch in ')]':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip()); cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


AUX = re.compile(r'^/\* (\S+):(\d+):(\w+) \*/ (.*?)\s*;')


def protos(ifile, tag):
    aux = os.path.join(OUTDIR, tag + '.aux')
    subprocess.run([CC1, '-quiet', '-O2', '-G0', ifile, '-aux-info', aux, '-o', '/dev/null'],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
    P = {}
    if not os.path.exists(aux):
        return P
    for line in open(aux, errors='replace'):
        m = AUX.match(line)
        if not m:
            continue
        where, kind, decl = m.group(1) + ':' + m.group(2), m.group(3), m.group(4)
        decl = re.sub(r'/\*.*?\*/', '', decl).strip()
        # find the outermost "name (args)" at the end
        depth, k = 0, len(decl) - 1
        if not decl.endswith(')'):
            continue
        while k >= 0:
            if decl[k] == ')':
                depth += 1
            elif decl[k] == '(':
                depth -= 1
                if depth == 0:
                    break
            k -= 1
        args = decl[k + 1:-1]
        head = decl[:k].strip()
        mm = re.search(r'(\w+)\s*$', head)
        if not mm or '(' in head:
            continue
        name = mm.group(1)
        ret = head[:mm.start()].replace('extern', '').replace('static', '').replace('inline', '').strip()
        al = split_args(args)
        va = '...' in al
        al = [a for a in al if a != '...']
        nf = sum(1 for a in al if re.match(r'^(const\s+)?(float|double)\b', a) and '*' not in a)
        P[name] = dict(ret=ret, args=al, nint=len(al) - nf, nflt=nf, va=va,
                       oldstyle=(kind[0] == 'O' or '???' in args or args.strip() == ''), where=where)
    return P


def tus():
    for unit in ['main', 'ov01', 'ov02', 'ov10', 'ov11', 'ov12']:
        srcdir = os.path.join(ROOT, 'src', unit)
        if not os.path.isdir(srcdir):
            continue
        for fn in sorted(os.listdir(srcdir)):
            if fn.endswith('.c'):
                yield unit, fn[:-2]


def ifile_for(unit, tu):
    c = [os.path.join(ROOT, 'build', unit, 'build/c', tu + '.o.i'),
         os.path.join(ROOT, 'build', unit, 'build/scaffold/src', unit, tu + '.i')]
    for p in c:
        if os.path.exists(p):
            return p
    return None


def asm_for(unit, tu, name):
    for p in [f'build/{unit}/asm/{unit}/nonmatchings/{tu}/{name}.s',
              f'build/{unit}/asm/nonmatchings/{unit}/{tu}/{name}.s',
              f'build/{unit}/asm/nonmatchings/{tu}/{name}.s']:
        if os.path.exists(os.path.join(ROOT, p)):
            return os.path.join(ROOT, p)
    return None


def main():
    global CC1
    ap = argparse.ArgumentParser()
    ap.add_argument('--unit'); ap.add_argument('--tu'); ap.add_argument('--json')
    a = ap.parse_args()
    ident = json.load(open(os.path.join(ROOT, 'config/toolchain-identity.json')))
    tc = [d for d in os.listdir(os.path.join(ROOT, '.work/toolchains')) if 'fsv5-usecount' in d]
    CC1 = os.path.join(ROOT, '.work/toolchains', sorted(tc)[-1], 'bin/cc1')
    os.makedirs(OUTDIR, exist_ok=True)
    findings = []
    for unit, tu in tus():
        if a.unit and unit != a.unit or a.tu and tu != a.tu:
            continue
        src = open(os.path.join(ROOT, 'src', unit, tu + '.c'), errors='replace').read()
        names = re.findall(r'INCLUDE_ASM\([^)]*?,\s*(\w+)\s*\)', src)
        if not names:
            continue
        ifile = ifile_for(unit, tu)
        if not ifile:
            continue
        P = protos(ifile, f'{unit}__{tu}')
        agg = defaultdict(lambda: defaultdict(list))
        for name in names:
            s = asm_for(unit, tu, name)
            if not s:
                continue
            for callee, ia, fa, rv, rf in call_sites(parse_s(s)):
                if not re.match(r'^[A-Za-z_]\w*$', callee) or callee.startswith('func_') or callee.startswith('D_'):
                    continue
                p = P.get(callee)
                site = name
                if p is None:
                    agg[callee]['IMPLICIT'].append(site)
                    continue
                if p['oldstyle']:
                    agg[callee]['OLDSTYLE'].append(site)
                if p['ret'] == 'void' and (rv or rf):
                    agg[callee]['RET_USED_VOID'].append(site)
                if p['ret'] not in ('void', 'float', 'double') and rf and not rv and '*' not in p['ret']:
                    agg[callee]['RET_FLOAT'].append(site)
                if not p['va'] and not p['oldstyle']:
                    if ia and INT_ARGS.index(ia[-1]) + 1 > p['nint']:
                        agg[callee]['EXTRA_ARGS'].append(f"{site}:int{INT_ARGS.index(ia[-1]) + 1}>{p['nint']}")
                    if fa and FLT_ARGS.index(fa[-1]) + 1 > p['nflt']:
                        agg[callee]['EXTRA_ARGS'].append(f"{site}:flt{FLT_ARGS.index(fa[-1]) + 1}>{p['nflt']}")
        for callee, kinds in agg.items():
            p = P.get(callee)
            for kind, sites in kinds.items():
                findings.append(dict(unit=unit, tu=tu, callee=callee, kind=kind, n=len(sites),
                                     sites=sorted(set(sites))[:6],
                                     decl=(f"{p['ret']} {callee}({', '.join(p['args'])})" if p else None),
                                     where=(p['where'] if p else None)))
    order = {'RET_USED_VOID': 0, 'EXTRA_ARGS': 1, 'RET_FLOAT': 2, 'IMPLICIT': 3, 'OLDSTYLE': 4}
    findings.sort(key=lambda f: (order[f['kind']], -f['n'], f['unit'], f['tu']))
    if a.json:
        json.dump(findings, open(a.json, 'w'), indent=1)
    for f in findings:
        print(f"{f['kind']:14} {f['unit']}/{f['tu']:24} {f['callee']:32} x{f['n']:<3} {f['decl'] or ''}  [{', '.join(f['sites'][:3])}]")


if __name__ == '__main__':
    main()

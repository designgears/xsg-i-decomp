#!/usr/bin/env python3
"""check_call_types.py: cross-TU prototype consistency for the i386 host port.

On the EE (MIPS EABI as used by ee-gcc) float arguments travel in FPRs and
integer/pointer arguments in GPRs, each class numbered independently, and a
float result comes back in $f0 while an int comes back in $v0.  So the decomp
could carry per-TU declarations such as `f(float, int *)` vs `f(int *, float)`,
or `int g()` for a function that returns float, and still match.  On i386
cdecl every argument is a stack slot in order and a float result is in st(0),
so those declarations corrupt arguments or results silently.

How it works: every C TU the host builds (src/main, src/ovXX) is run through
the host compiler with -fsyntax-only -aux-info, which prints every function
declaration and definition gcc saw, with file:line and whether it is a
prototype.  Implicit declarations are taken from gcc's warnings.  Every type
string is classified by a second compile of a probe TU that includes the
source and emits __builtin_classify_type/sizeof per type, so typedefs resolve
exactly as gcc sees them.  Each parameter becomes one of
  F float  D double  I int (<=4 bytes)  L 8-byte int  P pointer  S<n> struct
Q16 (a PS2 TImode quadword: a scalar in one 128-bit GPR on the EE, a ps2_u128
struct on the host), and every declaration of a global name is compared against its definition
(or, without a definition, against the most common declaration).

Reported (file:line: message), most severe first:
  RET     return class differs (float vs int vs void/8-byte...)
  STRUCT  one view passes a struct of STRUCT_REF_MIN+ bytes by value in a slot
          where another view has a pointer/int/other struct: the EE EABI
          passes such aggregates by invisible reference (one GPR holding the
          address), so the PS2 code can mix `T s` and `T *` freely, while
          i386 cdecl copies the whole struct onto the stack
  ARGS    parameter class sequence differs (float position, count, width)
  NOPROTO definition has float or 8-byte int params but a TU calls it through
          an unprototyped or implicit declaration (float -> double promotion,
          an int argument where 8 bytes are read)
With --structs, also list (informational, BYVAL) every definition taking a
struct of STRUCT_REF_MIN+ bytes by value: any caller outside C's view (asm,
function-pointer tables, casts through `()`) hands it a pointer on the EE.
Static functions are scoped per TU.  Exit 1 when a finding is not listed in
port/scripts/check_call_types_allow.txt (`name  reason` per line).

usage: check_call_types.py [-j N] [--all] [--structs]
"""
import collections
import concurrent.futures
import glob
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
UNITS = ['main', 'ov01', 'ov02', 'ov10', 'ov11', 'ov12']
CFLAGS = ['-m32', '-std=gnu89', '-fsigned-char', '-fno-common', '-fgnu89-inline', '-fno-builtin',
          '-DPORT_HOST', '-I' + ROOT + '/include', '-I' + ROOT,
          '-include', ROOT + '/port/include/ps2_host_compat.h']
AUX_RE = re.compile(r'^/\* (.*?):(\d+):([NO])([CF]) \*/ (.*?)(?:;|\s*/\* \((.*?)\).*\*/)?\s*$')
IMPL_RE = re.compile(r"^(.*?):(\d+):\d+: (?:warning|error): implicit declaration of function '(\w+)'")


def sources():
    out = []
    for u in UNITS:
        for f in sorted(glob.glob(os.path.join(ROOT, 'src', u, '**', '*.c'), recursive=True)):
            if '/src/main/nml_packet_add/' in f:
                continue
            out.append((u, f))
    return out


def split_top(s):
    out, depth, cur = [], 0, ''
    for c in s:
        if c in '([':
            depth += 1
        elif c in ')]':
            depth -= 1
        if c == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += c
    if cur.strip():
        out.append(cur.strip())
    return out


def parse_decl(text):
    """'extern float f (int, float *)' -> (storage, ret, name, params|None)"""
    text = text.strip().rstrip(";").strip()
    if text.endswith('{}') or text.endswith('{ }'):
        text = text[:text.rfind('{')].strip()
    # the declarator's parameter list is the last top-level (...) group
    if not text.endswith(')'):
        return None
    depth = 0
    for i in range(len(text) - 1, -1, -1):
        if text[i] == ')':
            depth += 1
        elif text[i] == '(':
            depth -= 1
            if depth == 0:
                break
    head, plist = text[:i].strip(), text[i + 1:-1].strip()
    m = re.match(r'^(.*?)(\w+)$', head)
    if not m:
        return None  # function returning function pointer etc.
    pre, name = m.group(1).strip(), m.group(2)
    words = pre.split()
    storage = 'static' if 'static' in words else 'extern'
    ret = ' '.join(w for w in words if w not in ('extern', 'static', 'inline', '__inline__', '__inline'))
    if plist.startswith('/* ???') or plist == '':
        params = None
    elif plist == 'void':
        params = []
    else:
        params = split_top(plist)
    return storage, ret or 'int', name, params


def run_tu(arg):
    unit, src = arg
    with tempfile.TemporaryDirectory() as td:
        aux = os.path.join(td, 'a.X')
        p = subprocess.run(['cc'] + CFLAGS + ['-fsyntax-only', '-aux-info', aux, src],
                           capture_output=True, text=True, errors='replace')
        decls, impl = [], []
        for line in p.stderr.splitlines():
            m = IMPL_RE.match(line)
            if m:
                impl.append((m.group(1), int(m.group(2)), m.group(3)))
        try:
            lines = open(aux, errors='replace').read().splitlines()
        except FileNotFoundError:
            return unit, src, [], impl, {}, p.stderr[-400:]
        for line in lines:
            m = AUX_RE.match(line)
            if not m or m.group(1).startswith('/usr/'):
                continue
            d = parse_decl(m.group(5))
            if not d:
                continue
            if m.group(4) == 'F' and d[3]:
                # definitions carry parameter names; the trailing comment lists them
                names = [x.strip() for x in (m.group(6) or '').split(',') if x.strip()]
                ps = list(d[3])
                for k, nm in enumerate(names):
                    if k < len(ps) and ps[k] != '...':
                        ps[k] = re.sub(r'\b%s\b(?!.*\b%s\b)' % (nm, nm), '', ps[k], count=1).strip()
                d = (d[0], d[1], d[2], ps)
            decls.append(dict(file=m.group(1), line=int(m.group(2)), proto=m.group(3) == 'N',
                              defn=m.group(4) == 'F', storage=d[0], ret=d[1], name=d[2],
                              params=d[3], unit=unit, tu=src))
        # classify every type string with a probe compile
        types = set()
        for d in decls:
            types.add(d['ret'])
            for t in d['params'] or []:
                if t != '...':
                    types.add(t)
        types = sorted(types)
        probe = os.path.join(td, 'probe.c')
        cls = {'void': 'V'}
        todo = [i for i, t in enumerate(types) if t != 'void']
        for _attempt in range(4):
            lines = {}
            with open(probe, 'w') as f:
                f.write('#include "%s"\n' % src)
                # host stand-in for the EE's TImode quadword (a scalar there)
                f.write('#ifndef PS2_U128_DEFINED\ntypedef struct { char __c[3]; } ps2_u128;\n#endif\n')
                n = 5
                for i in todo:
                    f.write('static void __xq_%d(%s __x) { static char __xs_%d[__builtin_classify_type(__x)'
                            ' * 4096 + sizeof(__x) + 0x100000 * __builtin_types_compatible_p(__typeof__(__x),'
                            ' ps2_u128)]; __asm__ volatile("" :: "r"(__xs_%d)); }'
                            ' void *__xu_%d = (void *)__xq_%d;\n' % (i, types[i], i, i, i, i))
                    lines[n] = i
                    n += 1
            p3 = subprocess.run(['cc'] + CFLAGS + ['-fno-pic', '-w', '-S', '-o', '-', probe],
                                capture_output=True, text=True, errors='replace',
                                cwd=os.path.dirname(src))
            if p3.returncode == 0:
                for m in re.finditer(r'\.local\s+__xs_(\d+)\.\d+\n\s*\.comm\s+__xs_\d+\.\d+,(\d+)', p3.stdout):
                    i, v = int(m.group(1)), int(m.group(2))
                    cls[types[i]] = 'Q16' if v >= 0x100000 else classify(v // 4096, v % 4096)
                break
            badl = {int(m.group(1)) for m in re.finditer(r'probe\.c:(\d+):\d+: error', p3.stderr)}
            drop = {lines[l] for l in badl if l in lines}
            if not drop:
                break
            todo = [i for i in todo if i not in drop]
        for t in types:
            cls.setdefault(t, '?')
        return unit, src, decls, impl, cls, ''


def classify(tc, size):
    # gcc type classes: 1 integer, 2 char, 3 enumeral, 4 boolean, 5 pointer,
    # 6 reference, 8 real, 12 record, 13 union
    if tc == 8:
        return 'F' if size == 4 else 'D'
    if tc in (1, 2, 3, 4):
        return 'L' if size == 8 else ('I' if size == 4 else 'i%d' % size)
    if tc == 5:
        return 'P'
    if tc in (12, 13):
        return 'S%d' % size
    return '?%d/%d' % (tc, size)


def sig(d, cls):
    ps = None if d['params'] is None else tuple('...' if t == '...' else cls.get(t, '?') for t in d['params'])
    return cls.get(d['ret'], '?'), ps


# EE gcc 2.96 EABI (mips.c function_arg_pass_by_reference): aggregates larger
# than UNITS_PER_WORD (8 on the EE) go by invisible reference.
STRUCT_REF_MIN = 9


def big_struct(c):
    return c.startswith('S') and c[1:].isdigit() and int(c[1:]) >= STRUCT_REF_MIN


def struct_slots(a, b):
    """slot indices where one view has a by-reference-on-EE struct and the
    other view something else"""
    out = []
    for k in range(min(len(a), len(b))):
        if '...' in (a[k], b[k]):
            break
        x, y = norm(a[k]), norm(b[k])
        if x != y and (big_struct(x) or big_struct(y)):
            out.append(k)
    return out


def norm(c):
    """i386 slot class.  Narrow ints are promoted to a 4-byte slot at prototyped
    calls, and pointers and ints share a 4-byte slot, so I, i1, i2, P and (for
    returns) V all compare equal: they pass through %eax / one stack slot the same
    way.  What breaks is F/D (stack slot vs st(0), 4 vs 8 bytes), L (8 bytes) and
    structs."""
    if c.startswith('i') or c in ('P', 'V'):
        return 'I'
    if c == 'Q16':
        return 'S16'  # on the host a quadword is a 16-byte struct like any other
    return c


def args_differ(a, b):
    """a call through one view and a callee with the other disagree on some slot
    both use (extra trailing args are ignored by cdecl, missing ones read garbage
    exactly as on the EE)"""
    n = min(len(a), len(b))
    if '...' in a[:n] or '...' in b[:n]:
        n = min(n, (a + ('...',)).index('...'), (b + ('...',)).index('...'))
    return a[:n] != b[:n]


def main():
    jobs = os.cpu_count() or 8
    if '-j' in sys.argv:
        jobs = int(sys.argv[sys.argv.index('-j') + 1])
    show_all = '--all' in sys.argv
    allow = {}
    ap = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'check_call_types_allow.txt')
    if os.path.exists(ap):
        for line in open(ap):
            line = line.split('#')[0].strip()
            if line:
                k, _, r = line.partition(' ')
                allow[k] = r.strip()
    entries = []  # (key, decl, ret, params)
    implicit = []
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        for unit, src, decls, impl, cls, err in ex.map(run_tu, sources()):
            if err:
                print('%s: aux-info failed: %s' % (src, err), file=sys.stderr)
            for d in decls:
                r, ps = sig(d, cls)
                key = d['name'] if d['storage'] == 'extern' else '%s@%s' % (d['name'], src)
                entries.append((key, d, r, ps))
            for f, l, n in impl:
                implicit.append((n, f, l, src))
    rel = lambda p: os.path.relpath(p, ROOT) if os.path.isabs(p) else p
    by = collections.defaultdict(list)
    for e in entries:
        by[e[0]].append(e)
    findings = []
    seen = set()
    for key, es in sorted(by.items()):
        defs = [e for e in es if e[1]['defn']]
        if defs:
            ref = defs[0]
        else:
            protos = [e for e in es if e[3] is not None]
            if not protos:
                continue
            cnt = collections.Counter((e[2], e[3]) for e in protos)
            best = cnt.most_common(1)[0][0]
            ref = next(e for e in protos if (e[2], e[3]) == best)
        rr, rp = ref[2], ref[3]
        name = key.split('@')[0]
        for e in es:
            if e is ref:
                continue
            d = e[1]
            where = '%s:%d' % (rel(d['file']), d['line'])
            if (where, name) in seen:
                continue
            what = '%s:%d (%s)' % (rel(ref[1]['file']), ref[1]['line'],
                                   'definition' if ref[1]['defn'] else 'majority declaration')
            if norm(e[2]) != norm(rr) and '?' not in e[2] + rr:
                findings.append((0, name, where, 'RET %s returns %s here, %s at %s' % (name, e[2], rr, what)))
                seen.add((where, name))
            if e[3] is not None and rp is not None:
                # a double and an 8-byte int occupy the same two stack slots
                a = tuple(norm(x).replace('D', 'L') for x in e[3])
                b = tuple(norm(x).replace('D', 'L') for x in rp)
                ss = struct_slots(e[3], rp)
                if ss and not any('?' in x for x in e[3] + rp):
                    findings.append((0, name, where, 'STRUCT %s(%s) here, (%s) at %s [slot %s]' % (
                        name, ','.join(e[3]), ','.join(rp), what, ','.join(str(k + 1) for k in ss))))
                    seen.add((where, name))
                elif args_differ(a, b) and not any('?' in x for x in a + b):
                    findings.append((1, name, where, 'ARGS %s(%s) here, (%s) at %s' % (
                        name, ','.join(e[3]), ','.join(rp), what)))
                    seen.add((where, name))
            if e[3] is None and rp is not None and any(x in ('F', 'L') for x in rp):
                findings.append((2, name, where, 'NOPROTO %s(%s) declared without a prototype here' % (
                    name, ','.join(rp))))
                seen.add((where, name))
    # implicit declarations: return int, args default-promoted
    for n, f, l, src in implicit:
        es = by.get(n)
        if not es:
            continue
        defs = [e for e in es if e[1]['defn']] or [e for e in es if e[3] is not None]
        if not defs:
            continue
        ref = defs[0]
        where = '%s:%d' % (rel(f), l)
        what = '%s:%d' % (rel(ref[1]['file']), ref[1]['line'])
        if norm(ref[2]) not in ('I', 'V', 'P'):
            findings.append((0, n, where, 'RET %s implicitly declared (int) here, returns %s at %s' % (n, ref[2], what)))
        if ref[3] and any(x in ('F', 'L') for x in ref[3]):
            findings.append((2, n, where, 'NOPROTO %s(%s) implicitly declared here' % (n, ','.join(ref[3]))))
    findings.sort()
    if '--structs' in sys.argv:
        seen_def = set()
        for key, es in sorted(by.items()):
            for e in es:
                if not e[1]['defn'] or e[3] is None:
                    continue
                ss = [k for k, x in enumerate(e[3]) if big_struct(x)]
                where = '%s:%d' % (rel(e[1]['file']), e[1]['line'])
                if ss and (where, key) not in seen_def:
                    seen_def.add((where, key))
                    print('%s: BYVAL %s(%s) struct by value in slot %s' % (
                        where, key.split('@')[0], ','.join(e[3]), ','.join(str(k + 1) for k in ss)))
    bad = 0
    for sev, name, where, msg in findings:
        allowed = name in allow
        if allowed and not show_all:
            continue
        bad += not allowed
        print('%s: %s%s' % (where, msg, '  [allowed: %s]' % allow[name] if allowed else ''))
    print('%d findings (%d not allowed)' % (len(findings), bad), file=sys.stderr)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())

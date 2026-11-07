#!/usr/bin/env python3
"""check_missing_args.py: find calls that pass fewer arguments than the callee takes.

The decomp matches the PS2 code, and the PS2 code sometimes calls a function
with fewer arguments than its definition takes: the callee then reads
whatever $a0..$a3 still hold from earlier code ("leftover registers").  The C
says this with a `(void)`/`()` extern, a `((T (*)())f)(...)` cast, or an
`__asm__("name")` alias whose prototype is shorter than the real function.
On i386 the missing arguments are stack slots above the pushed ones, so the
callee reads unrelated caller stack instead.  Each finding needs the PS2 asm
at the call site to tell what the register held (often a value the callee
ignores on that path, sometimes a real argument).

How it works: every C TU the host builds is parsed by clang
(-Xclang -ast-dump=json, same defines as the host build).  From each AST it
takes
  * every function definition: name, parameter count, variadic, static;
  * every call whose callee is (after parentheses, implicit and explicit
    casts, & and *) a reference to a named function, with the argument count;
  * every `__asm__("label")` on a function declaration (an alias): calls to
    the declared name are checked against the definition of `label`.
A call is reported when it passes fewer arguments than the definition of the
function it reaches has parameters (variadic: fewer than the fixed ones).
Static functions are matched per TU.

Exit 1 when a finding is not listed in port/scripts/check_missing_args_allow.txt
(`file:line name  reason` or `name  reason` per line; the name form allows
every call to that function).

usage: check_missing_args.py [-j N] [--all]
"""
import collections
import concurrent.futures
import glob
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
UNITS = ['main', 'ov01', 'ov02', 'ov10', 'ov11', 'ov12']
CFLAGS = ['-m32', '-std=gnu89', '-fsigned-char', '-fno-common', '-fno-builtin',
          '-DPORT_HOST', '-I' + ROOT + '/include', '-I' + ROOT,
          '-include', ROOT + '/port/include/ps2_host_compat.h',
          '-w', '-Wno-error', '-ferror-limit=0']
ASM_RE = re.compile(r'\b(\w+)\s*\([^;{}]*\)\s*__asm__\s*\(\s*"(\w+)"\s*\)')


def sources():
    out = []
    for u in UNITS:
        for f in sorted(glob.glob(os.path.join(ROOT, 'src', u, '**', '*.c'), recursive=True)):
            if '/src/main/nml_packet_add/' in f:
                continue
            out.append((u, f))
    return out


class Loc:
    """clang's JSON dumper omits file/line when unchanged since the last
    location it printed, so locations are tracked in dump order."""

    def __init__(self):
        self.file, self.line = None, None

    def feed(self, loc):
        if not loc:
            return
        if 'expansionLoc' in loc:
            self.feed(loc.get('spellingLoc'))
            loc = loc['expansionLoc']
        if 'file' in loc:
            self.file = loc['file']
        if 'line' in loc:
            self.line = loc['line']


def strip_callee(n):
    cast = False
    while True:
        k = n.get('kind')
        if k in ('ImplicitCastExpr', 'ParenExpr'):
            pass
        elif k == 'CStyleCastExpr':
            cast = True
        elif k == 'UnaryOperator' and n.get('opcode') in ('&', '*'):
            pass
        else:
            return n, cast
        inner = n.get('inner') or []
        if not inner:
            return n, cast
        n = inner[0]


def run_tu(arg):
    unit, src = arg
    p = subprocess.run(['clang'] + CFLAGS + ['-fsyntax-only', '-Xclang', '-ast-dump=json', src],
                       capture_output=True, text=True, errors='replace', cwd=os.path.dirname(src))
    if not p.stdout.strip():
        return src, [], [], {}, p.stderr[-400:]
    ast = json.loads(p.stdout)
    loc = Loc()
    defs, calls = [], []

    def walk(n, fn):
        loc.feed(n.get('loc'))
        r = n.get('range')
        if r:
            loc.feed(r.get('begin'))
            begin = (loc.file, loc.line)
            loc.feed(r.get('end'))
        else:
            begin = (loc.file, loc.line)
        k = n.get('kind')
        inner = n.get('inner') or []
        if k == 'FunctionDecl':
            fn = n.get('name')
            if any(c.get('kind') == 'CompoundStmt' for c in inner):
                qt = n.get('type', {}).get('qualType', '')
                defs.append(dict(name=fn, nparams=sum(c.get('kind') == 'ParmVarDecl' for c in inner),
                                 variadic='...' in qt, static=n.get('storageClass') == 'static',
                                 file=begin[0], line=begin[1]))
        for c in inner:
            walk(c, fn)
        if k == 'CallExpr' and inner:
            callee, cast = strip_callee(inner[0])
            ref = callee.get('referencedDecl') if callee.get('kind') == 'DeclRefExpr' else None
            if ref and ref.get('kind') == 'FunctionDecl':
                calls.append(dict(name=ref['name'], nargs=len(inner) - 1, cast=cast,
                                  type=ref.get('type', {}).get('qualType', ''),
                                  file=begin[0], line=begin[1], caller=fn))

    walk(ast, None)
    aliases = {}
    try:
        text = open(src, errors='replace').read()
        for m in ASM_RE.finditer(text):
            if m.group(1) != m.group(2):
                aliases[m.group(1)] = m.group(2)
    except OSError:
        pass
    return src, defs, calls, aliases, ''


def main():
    jobs = os.cpu_count() or 8
    if '-j' in sys.argv:
        jobs = int(sys.argv[sys.argv.index('-j') + 1])
    show_all = '--all' in sys.argv
    allow = {}
    ap = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'check_missing_args_allow.txt')
    if os.path.exists(ap):
        for line in open(ap):
            line = line.split('#')[0].strip()
            if line:
                k, _, r = line.partition(' ')
                allow[k] = r.strip()
    rel = lambda p: os.path.relpath(p, ROOT) if p and os.path.isabs(p) else (p or '?')
    gdefs = collections.defaultdict(list)
    results = []
    with concurrent.futures.ProcessPoolExecutor(jobs) as ex:
        for src, defs, calls, aliases, err in ex.map(run_tu, sources()):
            if err:
                print('%s: clang failed: %s' % (rel(src), err), file=sys.stderr)
            for d in defs:
                if not d['static']:
                    gdefs[d['name']].append(d)
            results.append((src, defs, calls, aliases))
    findings = []
    seen = set()
    for src, defs, calls, aliases in results:
        sdefs = {d['name']: d for d in defs if d['static']}
        for c in calls:
            target = aliases.get(c['name'], c['name'])
            d = sdefs.get(target) or (gdefs[target][0] if gdefs.get(target) else None)
            if not d:
                continue
            need = d['nparams']
            if c['nargs'] >= need:
                continue
            where = '%s:%s' % (rel(c['file']), c['line'])
            if (where, c['name']) in seen:
                continue
            seen.add((where, c['name']))
            how = []
            if target != c['name']:
                how.append('alias %s -> %s' % (c['name'], target))
            if c['cast']:
                how.append('cast')
            how.append('declared %s' % c['type'])
            findings.append((where, c['name'], '%s: %s passes %d of %d%s args to %s (%s:%s), in %s [%s]' % (
                where, c['name'], c['nargs'], need, '+' if d['variadic'] else '', target,
                rel(d['file']), d['line'], c['caller'], '; '.join(how))))
    findings.sort()
    bad = 0
    for where, name, msg in findings:
        key = '%s:%s' % (where, name) if '%s:%s' % (where, name) in allow else name
        allowed = key in allow
        if allowed and not show_all:
            continue
        bad += not allowed
        print(msg + ('  [allowed: %s]' % allow[key] if allowed else ''))
    print('%d findings (%d not allowed)' % (len(findings), bad), file=sys.stderr)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())

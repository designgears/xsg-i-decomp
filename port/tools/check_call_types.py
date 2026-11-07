#!/usr/bin/env python3
"""Stage 11: find calls whose declaration disagrees with the callee's definition.

On the PS2 a function returning `short`/`char` leaves its result sign- or
zero-extended to the full register (MIPS ABI), so a caller that declares it
`int` (no prototype, an `__asm__` alias, a stale prototype) reads the right
value.  On i386 only %ax / %al is defined; the upper bits are garbage.

This uses GCC's LTO type checking: every game file is recompiled with -flto
(same flags as the build), then `main` plus each overlay is LTO-linked with
-Wlto-type-mismatch.  The report keeps only function return-type conflicts
where at least one side is narrower than int, or the signedness differs.

  check_call_types.py <cmake-build-dir> [--all]   (--all: print every mismatch)
"""
import os, re, shlex, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

bdir = sys.argv[1]
show_all = "--all" in sys.argv
cmds = subprocess.run(["ninja", "-C", bdir, "-t", "commands", "xsg_game"], capture_output=True,
                      text=True, check=True).stdout.splitlines()
tmp = tempfile.mkdtemp(prefix="xsg_cct_")
units = {}
jobs = []
for c in cmds:
    if " -c " not in c or "/src/" not in c:
        continue
    a = shlex.split(c)
    src = a[a.index("-c") + 1]
    unit = src.split("/src/")[1].split("/")[0]
    out = os.path.join(tmp, src.split("/src/")[1].replace("/", "_") + ".o")
    a[a.index("-o") + 1] = out
    a = [x for x in a if x not in ("-MD",)]
    i = a.index("-MF"); del a[i:i + 2]
    i = a.index("-MT"); del a[i:i + 2]
    a[1:1] = ["-flto", "-w"]
    units.setdefault(unit, []).append(out)
    jobs.append((a, bdir))

def run(j):
    return subprocess.run(j[0], cwd=j[1], capture_output=True, text=True).returncode
with ThreadPoolExecutor(os.cpu_count()) as ex:
    bad = [r for r in ex.map(run, jobs) if r]
if bad:
    print("warning: %d compiles failed" % len(bad), file=sys.stderr)

WIDTH = [("char", 1), ("u8", 1), ("s8", 1), ("_Bool", 1), ("short", 2), ("u16", 2), ("s16", 2)]

def width(t):
    if "*" in t or "(" in t:
        return 4
    for k, w in WIDTH:
        if re.search(r"\b%s\b" % k, t):
            return w
    return 0 if t == "void" else 4

def signed(t):
    return not ("unsigned" in t or re.search(r"\bu(8|16)\b", t))

env = dict(os.environ, LANG="C", LC_ALL="C")
seen = set()
for ov in [u for u in units if u != "main"] or ["main"]:
    objs = units["main"] + (units[ov] if ov != "main" else [])
    r = subprocess.run(["gcc", "-m32", "-flto", "-flto-partition=none", "-r", "-nostdlib", "-o",
                        os.path.join(tmp, ov + ".lto.o"), "-Wlto-type-mismatch"] + objs,
                       capture_output=True, text=True, cwd=bdir, env=env)
    blocks, cur = [], None
    for l in r.stderr.splitlines():
        if "warning: type of" in l:
            cur = [l]; blocks.append(cur)
        elif cur is not None and "note:" in l:
            cur.append(l)
    for b in blocks:
        t = "\n".join(b)
        if "return value type mismatch" not in t:
            continue
        name = re.search(r"type of '(\w+)'", b[0]).group(1)
        decl = b[0].split(": warning")[0].split("/src/")[-1]
        defn = next((x.split(": note")[0].split("/src/")[-1] for x in b if "previously declared" in x), "?")
        if (name, decl) in seen:
            continue
        seen.add((name, decl))
        ty = re.search(r"type '([^']*)' should match type '([^']*)'", t)
        if not ty:          # pointer/struct return types: not an extension issue
            if show_all:
                print("%-32s decl %s  def %s  (pointer types)" % (name, decl, defn))
            continue
        dt, ct = ty.group(1), ty.group(2)      # definition's type, caller's type
        wd, wc = width(dt), width(ct)
        if 0 < wd < wc:
            kind = "NARROW: callee returns %s, caller reads %s" % (dt, ct)
        elif wd == wc and wd < 4 and signed(dt) != signed(ct):
            kind = "SIGN: callee returns %s, caller reads %s" % (dt, ct)
        elif wd == 0 and wc:
            kind = "VOID: callee is void, caller reads %s (see scan_void_results.py)" % ct
        else:
            kind = "ok: callee returns %s, caller reads %s" % (dt, ct)
            if not show_all:
                continue
        print("%-32s decl %s  def %s  %s" % (name, decl, defn, kind))
print("objects in", tmp, file=sys.stderr)

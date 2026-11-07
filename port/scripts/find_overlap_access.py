#!/usr/bin/env python3
"""Stage 11: find code that reaches one PS2 data symbol through another.

On the PS2 many objects are carved into several symbols (FS, D_00881188 =
FS+0x08, D_0088119C = FS+0x1c ...).  When one C file names both A and a
symbol B that sits inside the part of A the code touches, GCC assumes the two
declarations never overlap and caches or reorders loads and stores between
`A[k]` and `B` (the xglFontFlushCore hang, STAGE10_PERF.md 1a).

Input is the compiled host objects, so the offsets are what GCC really
emitted after -O2 folding: every R_386_32 relocation in code against a data
symbol carries the constant offset of that use in place (`FS+0x1c`).  Each
use is classified from the instruction that holds it:

  mem   memory operand  (`mov 0x1c(%eax),..`, `movl $1,FS+0x1c`) - an access
  cmp   immediate in cmp/sub/test                 - loop bound, end pointer
  addr  immediate elsewhere (`mov $FS+0x1c,%eax`, push) - address taken
  hi    `mov $A+0x8000,%eax; xor %ax,%ax` - translated MIPS %hi(A), ignored

The PS2 layout comes from build/<unit>/symbol_addrs.txt (+ main's for the
overlays) and the splat data labels.  A file is reported when it names both
A and B, B's PS2 address is A+gap, and the file uses A at an offset >= gap:

  OVERLAP  a `mem` use of A at offset >= gap (GCC sees A[k] and B as
           different objects although they are the same bytes);
  review   (-v) only `addr` uses reach B: a pointer derived from A walks
           into B; harmless unless it is dereferenced at those offsets in
           the same function as an access to B;
  `cmp` uses (A + sizeof(A) as a loop end) are not reported.

The host fix is the FS one: `#define B (*(T *)((char *)A + gap))` under
PORT_HOST, so B is no longer a separate object in that file, or route the
access through XSG_VIEW.  Pairs reviewed as harmless go in
find_overlap_access_allow.txt as `<file.c> <A> <B>  # reason`.

  find_overlap_access.py BUILD_DIR [--root REPO] [-v]
      -> one line per pair; exit 1 if an OVERLAP is not allowed
"""
import argparse
import os
import re
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audit_data_relocs as AR  # noqa: E402

UNITS = ["main", "ov01", "ov02", "ov10", "ov11", "ov12"]
INSN_RE = re.compile(r"^\s*([0-9a-f]+):\s+(.*)$")
RELOC_RE = re.compile(r"^\s*([0-9a-f]+):\s+R_386_32\s+(\S+)$")


def elf_addends(path):
    """-> ({(section, offset): addend} for R_386_32 in code, {name}) of an
    i386 REL object."""
    d = open(path, "rb").read()
    if d[:4] != b"\x7fELF" or d[4] != 1:
        return {}, set()
    shoff, = struct.unpack_from("<I", d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x2E)
    secs = [struct.unpack_from("<IIIIIIIIII", d, shoff + i * shentsize) for i in range(shnum)]
    shstr = secs[shstrndx][4]

    def cstr(base, o):
        return d[base + o:d.index(b"\0", base + o)].decode("latin-1")

    out = {}
    for s in secs:
        if s[1] != 9:  # SHT_REL
            continue
        tgt = secs[s[7]]
        if not tgt[2] & 0x4:  # SHF_EXECINSTR
            continue
        tname = cstr(shstr, tgt[0])
        for k in range(s[5] // 8):
            off, info = struct.unpack_from("<II", d, s[4] + k * 8)
            if info & 0xFF == 1:  # R_386_32
                out[(tname, off)] = struct.unpack_from("<i", d, tgt[4] + off)[0]
    return out


def uses(obj):
    """-> {symbol: [(addend, kind, function)]} for absolute uses in code."""
    add = elf_addends(obj)
    r = subprocess.run(["objdump", "-dr", "--no-show-raw-insn", obj],
                       capture_output=True, text=True)
    res, sec, func, insn, last = {}, None, None, "", None
    for ln in r.stdout.splitlines():
        if ln.startswith("Disassembly of section "):
            sec = ln[len("Disassembly of section "):].rstrip(":")
            continue
        m = re.match(r"[0-9a-f]+ <(.+)>:", ln)
        if m:
            func = m.group(1)
            continue
        m = RELOC_RE.match(ln)
        if m:
            off, sym = int(m.group(1), 16), m.group(2)
            a = add.get((sec, off))
            if a is None or sym.startswith("."):
                continue
            body = insn.split("#")[0]
            op = body.split()[0] if body.split() else ""
            if "$0x%x" % (a & 0xFFFFFFFF) in body or ("$0x0" in body and a == 0):
                # the relocated field is an immediate (address of A+a)
                if not ("0x%x" % a in body.replace("$0x%x" % a, "")) or op.startswith("mov"):
                    kind = "cmp" if op.startswith(("cmp", "sub", "test", "lea")) else "addr"
                else:
                    kind = "mem"
                if op.startswith("mov") and re.search(r"\$0x[0-9a-f]+,0x[0-9a-f]+$", body):
                    # movl $imm, A+a: which field is relocated is ambiguous
                    kind = "mem"
            else:
                kind = "mem"
            res.setdefault(sym, []).append((a, kind, func))
            rm = re.search(r",%e(\w\w)$", body.strip())
            last = (sym, len(res[sym]) - 1, rm.group(1), 3) if kind == "addr" and rm else None
            continue
        m = INSN_RE.match(ln)
        if m:
            insn = m.group(2)
            # `mov $A+0x8000,%eax; xor %ax,%ax` is the translated MIPS %hi(A)
            # (lui): the address of A rounded, not a pointer past it
            if last:
                sym0, k0, reg, left = last
                if re.match(r"xor\s+%%%s,%%%s\s*$" % (reg, reg), insn):
                    a0, _kind, f0 = res[sym0][k0]
                    res[sym0][k0] = (a0, "hi", f0)
                    last = None
                elif left > 1 and reg not in insn:
                    last = (sym0, k0, reg, left - 1)
                else:
                    last = None
    return res


DIE_RE = re.compile(r"^(0x[0-9a-f]+):(\s+)(DW_TAG_\w+)")
ATTR_RE = re.compile(r"^\s+(DW_AT_\w+)\s+\((.*)\)$")


def decl_sizes(obj):
    """name -> declared byte size of every global variable in the object's
    DWARF (None for incomplete types such as `extern char FS[]`)."""
    r = subprocess.run(["llvm-dwarfdump", "--debug-info", obj], capture_output=True, text=True)
    dies, order, cur, last_array = {}, [], None, None
    for ln in r.stdout.splitlines():
        m = DIE_RE.match(ln)
        if m:
            cur = {"tag": m.group(3), "subs": []}
            dies[m.group(1)] = cur
            order.append(cur)
            if cur["tag"] == "DW_TAG_array_type":
                last_array = cur
            elif cur["tag"] == "DW_TAG_subrange_type" and last_array is not None:
                last_array["subs"].append(cur)
            continue
        m = ATTR_RE.match(ln)
        if m and cur is not None:
            cur[m.group(1)] = m.group(2)
    memo = {}

    def ref(v):
        m = re.match(r"(0x[0-9a-f]+)", v or "")
        return dies.get(m.group(1)) if m else None

    def size(d, depth=0):
        if d is None or depth > 40:
            return None
        if id(d) in memo:
            return memo[id(d)]
        res = None
        if "DW_AT_byte_size" in d:
            res = int(d["DW_AT_byte_size"], 0)
        elif d["tag"] == "DW_TAG_pointer_type":
            res = 4
        elif d["tag"] in ("DW_TAG_typedef", "DW_TAG_const_type", "DW_TAG_volatile_type"):
            res = size(ref(d.get("DW_AT_type")), depth + 1)
        elif d["tag"] == "DW_TAG_array_type":
            n = 1
            for s in d["subs"]:
                if "DW_AT_count" in s:
                    n *= int(s["DW_AT_count"], 0)
                elif "DW_AT_upper_bound" in s:
                    n *= int(s["DW_AT_upper_bound"], 0) + 1
                else:
                    n = None
                    break
            e = size(ref(d.get("DW_AT_type")), depth + 1)
            res = n * e if n is not None and e is not None and d["subs"] else None
        memo[id(d)] = res
        return res

    out = {}
    for d in order:
        if d["tag"] == "DW_TAG_variable" and "DW_AT_name" in d and "DW_AT_type" in d:
            name = d.get("DW_AT_linkage_name", d["DW_AT_name"]).strip('"')
            s = size(ref(d["DW_AT_type"]))
            if s is not None:
                out[name] = max(out.get(name) or 0, s)
            else:
                out.setdefault(name, None)
    return out


def game_objects(build):
    out = []
    ninja = os.path.join(build, "build.ninja")
    if os.path.exists(ninja):
        with open(ninja) as f:
            for line in f:
                m = re.match(r"build (\S+\.c\.o): C_COMPILER\S* (\S+/src/(\w+)/\S+\.c)", line)
                if m and m.group(3) in UNITS:
                    out.append((os.path.join(build, m.group(1)), m.group(2), m.group(3)))
        return out
    # Makefile generators: objects live at CMakeFiles/<tgt>.dir/<abs source path>.o
    for dirpath, _dirs, files in os.walk(os.path.join(build, "CMakeFiles")):
        for fn in files:
            if not fn.endswith(".c.o"):
                continue
            obj = os.path.join(dirpath, fn)
            m = re.search(r"\.dir(/.*?/src/(\w+)/.+\.c)\.o$", obj)
            if m and m.group(2) in UNITS and os.path.exists(m.group(1)):
                out.append((obj, m.group(1), m.group(2)))
    return out


def ps2_layout(root):
    lay = {}
    for u in UNITS:
        by_addr, by_name = AR.load_symbols(root, u)
        names = dict(by_name)
        for a, ents in by_addr.items():
            for n, _k, _s in ents:
                names.setdefault(n, a)
        lay[u] = names
    return lay


def check(build, root, verbose=False, allow=()):
    lay = ps2_layout(root)
    rows = []
    for obj, src, unit in game_objects(build):
        if not os.path.exists(obj):
            continue
        U = uses(obj)
        L = lay[unit]
        data = sorted((L[n], n) for n in U if n in L)
        sizes = None
        for i, (aa, an) in enumerate(data):
            top = max(a for a, _k, _f in U[an])
            for ba, bn in data[i + 1:]:
                gap = ba - aa
                if gap <= 0:
                    continue
                if gap > top:
                    # constant offsets stop short of B; does the C type of A
                    # still cover B (A[i] with a variable index)?
                    if gap >= 0x40000:
                        break
                    if sizes is None:
                        sizes = decl_sizes(obj)
                    if (sizes.get(an) or 0) > gap:
                        key = (os.path.basename(src), an, bn)
                        rows.append(("allowed" if key in allow else "EXTENT",
                                     os.path.relpath(src, root), an, bn, gap,
                                     [(sizes[an], "sizeof", "decl")], []))
                    continue
                far = [(a, k, f) for a, k, f in U[an] if a >= gap]
                kinds = {k for _a, k, _f in far}
                if "mem" in kinds:
                    tag = "OVERLAP"
                elif "addr" in kinds:
                    tag = "review"
                else:
                    continue
                key = (os.path.basename(src), an, bn)
                if key in allow:
                    tag = "allowed"
                ex = sorted({(a, k, f) for a, k, f in far if k == ("mem" if tag != "review" else "addr")})
                # functions that use both the far part of A and B itself:
                # only there can GCC reorder or forward between the two
                shared = sorted({f for _a, _k, f in ex} & {f for _a, _k, f in U[bn]})
                rows.append((tag, os.path.relpath(src, root), an, bn, gap, ex, shared))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build")
    ap.add_argument("--root", default=os.path.abspath(os.path.join(os.path.dirname(__file__), "../..")))
    ap.add_argument("--allow", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                    "find_overlap_access_allow.txt"))
    ap.add_argument("-v", action="store_true", help="also list `review` pairs")
    a = ap.parse_args()
    allow = set()
    if os.path.exists(a.allow):
        for line in open(a.allow):
            line = line.split("#")[0].strip()
            if line:
                allow.add(tuple(line.split()[:3]))
    bad = 0
    for tag, src, an, bn, gap, ex, shared in check(a.build, a.root, a.v, allow):
        if tag in ("OVERLAP", "EXTENT"):
            bad += 1
        elif not a.v:
            continue
        print("%-7s %s: %s reaches %s (= %s+0x%x) via %s" % (
            tag, src, an, bn, an, gap,
            ", ".join("%s+0x%x[%s]" % (f, o, k) for o, k, f in ex[:4]) + (" ..." if len(ex) > 4 else "")
            + ("; B also used in %s" % ", ".join(shared) if shared else "")))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()

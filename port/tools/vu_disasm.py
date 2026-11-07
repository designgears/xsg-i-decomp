#!/usr/bin/env python3
"""VU micro-mode disassembler for the xsg port.

The decode tables follow port/runtime/vu1.cpp (the interpreter), so whatever
this prints is what the interpreter executes.

  vu_disasm.py --blob [--set N]          microcode set N (0..6) of the .vudata blob
  vu_disasm.py --micro FILE              a raw 16 KB micro memory (vu1_micro.bin, a
                                         capture's MICR record extracted)
  --entries 0x28,0x160,...               label these addresses and print the static
                                         reach (pairs reachable without leaving the
                                         program) of each
  --sets                                 list the MPG sets in the blob and exit
"""
import argparse
import struct
import sys

BLOB = "build/main/assets/main/blobs/vudata_rest.databin.bin"
DEST = ["", "w", "z", "zw", "y", "yw", "yz", "yzw", "x", "xw", "xz", "xzw", "xy", "xyw", "xyz", "xyzw"]
BC = "xyzw"


def blob_sets(path):
    d = open(path, "rb").read()
    sets = []
    off = 0
    while off + 16 <= len(d):
        qwc = struct.unpack_from("<I", d, off)[0] & 0xFFFF
        end = off + 16 + qwc * 16
        micro = bytearray(16384)
        uploads = []
        p = off + 8
        while p + 4 <= end:
            w = struct.unpack_from("<I", d, p)[0]
            if (w >> 24) & 0x7F == 0x4A:
                num = ((w >> 16) & 0xFF) or 256
                a = (w & 0xFFFF) * 8
                micro[a:a + num * 8] = d[p + 4:p + 4 + num * 8]
                uploads.append((a, num))
                p += 4 + num * 8
            else:
                p += 4
        if not uploads:
            break
        size = max(a + n * 8 for a, n in uploads)
        sets.append((off, bytes(micro), size, uploads))
        off = end
    return sets


def upper(w):
    """-> (text, writes) ; writes is the destination register name or None."""
    dest = DEST[(w >> 21) & 15]
    ft, fs, fd = (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31
    op = w & 0x3F
    flags = ("[I]" if w >> 31 & 1 else "") + ("[E]" if w >> 30 & 1 else "") + \
            ("[M]" if w >> 29 & 1 else "") + ("[D]" if w >> 28 & 1 else "") + ("[T]" if w >> 27 & 1 else "")
    if op < 0x3C:
        if op < 0x1C:
            names = ["add", "sub", "madd", "msub", "max", "mini", "mul"]
            return f"{names[op >> 2]}{BC[op & 3]}.{dest} vf{fd}, vf{fs}, vf{ft}{BC[op & 3]}", flags
        t = {0x1C: ("mulq", "q"), 0x1D: ("maxi", "i"), 0x1E: ("muli", "i"), 0x1F: ("minii", "i"),
             0x20: ("addq", "q"), 0x21: ("maddq", "q"), 0x22: ("addi", "i"), 0x23: ("maddi", "i"),
             0x24: ("subq", "q"), 0x25: ("msubq", "q"), 0x26: ("subi", "i"), 0x27: ("msubi", "i")}
        if op in t:
            n, s = t[op]
            return f"{n}.{dest} vf{fd}, vf{fs}, {s}", flags
        v = {0x28: "add", 0x29: "madd", 0x2A: "mul", 0x2B: "max", 0x2C: "sub", 0x2D: "msub",
             0x2E: "opmsub", 0x2F: "mini"}
        if op in v:
            return f"{v[op]}.{dest} vf{fd}, vf{fs}, vf{ft}", flags
        return f"upper? {w:08x}", flags
    sp = (w & 3) | ((w >> 4) & 0x7C)
    if sp < 0x10 or 0x18 <= sp < 0x1C:
        names = ["adda", "suba", "madda", "msuba", "?", "?", "mula"]
        return f"{names[sp >> 2]}{BC[sp & 3]}.{dest} acc, vf{fs}, vf{ft}{BC[sp & 3]}", flags
    a = {0x1C: "mulaq", 0x1E: "mulai", 0x20: "addaq", 0x21: "maddaq", 0x22: "addai", 0x23: "maddai",
         0x24: "subaq", 0x25: "msubaq", 0x26: "subai", 0x27: "msubai"}
    if sp in a:
        return f"{a[sp]}.{dest} acc, vf{fs}, {a[sp][-1]}", flags
    a2 = {0x28: "adda", 0x29: "madda", 0x2A: "mula", 0x2C: "suba", 0x2D: "msuba"}
    if sp in a2:
        return f"{a2[sp]}.{dest} acc, vf{fs}, vf{ft}", flags
    if 0x10 <= sp <= 0x13:
        return f"itof{[0, 4, 12, 15][sp & 3]}.{dest} vf{ft}, vf{fs}", flags
    if 0x14 <= sp <= 0x17:
        return f"ftoi{[0, 4, 12, 15][sp & 3]}.{dest} vf{ft}, vf{fs}", flags
    if sp == 0x1D:
        return f"abs.{dest} vf{ft}, vf{fs}", flags
    if sp == 0x1F:
        return f"clipw.xyz vf{fs}, vf{ft}w", flags
    if sp == 0x2E:
        return f"opmula.{dest} acc, vf{fs}, vf{ft}", flags
    if sp == 0x2F:
        return "nop", flags
    return f"upper-special? {w:08x}", flags


def imm11(w):
    v = w & 0x7FF
    return v - 0x800 if v & 0x400 else v


def lower(w, pc):
    """-> (text, kind, target) kind: None | 'b' (cond) | 'B' (uncond) | 'bal' | 'jr' | 'jalr'"""
    op7 = w >> 25
    dest = DEST[(w >> 21) & 15]
    ft, fs, fd = (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31
    it, is_, id_ = ft & 15, fs & 15, fd & 15
    tgt = (pc + 8 + imm11(w) * 8) & 0x3FF8
    if op7 == 0x00:
        return f"lq.{dest} vf{ft}, {imm11(w)}(vi{is_})", None, None
    if op7 == 0x01:
        return f"sq.{dest} vf{fs}, {imm11(w)}(vi{it})", None, None
    if op7 == 0x04:
        return f"ilw.{dest} vi{it}, {imm11(w)}(vi{is_})", None, None
    if op7 == 0x05:
        return f"isw.{dest} vi{it}, {imm11(w)}(vi{is_})", None, None
    if op7 in (0x08, 0x09):
        imm = ((w >> 10) & 0x7800) | (w & 0x7FF)
        return f"{'iaddiu' if op7 == 8 else 'isubiu'} vi{it}, vi{is_}, 0x{imm:x}", None, None
    if 0x10 <= op7 <= 0x13:
        return f"{['fceq', 'fcset', 'fcand', 'fcor'][op7 - 0x10]} vi1, 0x{w & 0xFFFFFF:06x}", None, None
    if 0x14 <= op7 <= 0x17:
        imm = ((w >> 10) & 0x800) | (w & 0x7FF)
        return f"{['fseq', 'fsset', 'fsand', 'fsor'][op7 - 0x14]} vi{it}, 0x{imm:x}", None, None
    if op7 in (0x18, 0x1A, 0x1B):
        return f"{ {0x18: 'fmeq', 0x1A: 'fmand', 0x1B: 'fmor'}[op7]} vi{it}, vi{is_}", None, None
    if op7 == 0x1C:
        return f"fcget vi{it}", None, None
    if op7 == 0x20:
        return f"b 0x{tgt:04x}", "B", tgt
    if op7 == 0x21:
        return f"bal vi{it}, 0x{tgt:04x}", "bal", tgt
    if op7 == 0x24:
        return f"jr vi{is_}", "jr", None
    if op7 == 0x25:
        return f"jalr vi{it}, vi{is_}", "jalr", None
    if op7 in (0x28, 0x29):
        return f"{'ibeq' if op7 == 0x28 else 'ibne'} vi{it}, vi{is_}, 0x{tgt:04x}", "b", tgt
    if 0x2C <= op7 <= 0x2F:
        return f"{['ibltz', 'ibgtz', 'iblez', 'ibgez'][op7 - 0x2C]} vi{is_}, 0x{tgt:04x}", "b", tgt
    if op7 != 0x40:
        return f"lower? {w:08x}", None, None
    sub = w & 0x3F
    if sub < 0x3C:
        m = {0x30: "iadd", 0x31: "isub", 0x34: "iand", 0x35: "ior"}
        if sub in m:
            return f"{m[sub]} vi{id_}, vi{is_}, vi{it}", None, None
        if sub == 0x32:
            v = fd - 32 if fd & 16 else fd
            return f"iaddi vi{it}, vi{is_}, {v}", None, None
        return f"lower40? {w:08x}", None, None
    sp = (w & 3) | ((w >> 4) & 0x7C)
    fsf, ftf = BC[(w >> 21) & 3], BC[(w >> 23) & 3]
    t = {
        0x30: f"move.{dest} vf{ft}, vf{fs}", 0x31: f"mr32.{dest} vf{ft}, vf{fs}",
        0x34: f"lqi.{dest} vf{ft}, (vi{is_}++)", 0x35: f"sqi.{dest} vf{fs}, (vi{it}++)",
        0x36: f"lqd.{dest} vf{ft}, (--vi{is_})", 0x37: f"sqd.{dest} vf{fs}, (--vi{it})",
        0x38: f"div q, vf{fs}{fsf}, vf{ft}{ftf}", 0x39: f"sqrt q, vf{ft}{ftf}",
        0x3A: f"rsqrt q, vf{fs}{fsf}, vf{ft}{ftf}", 0x3B: "waitq",
        0x3C: f"mtir vi{it}, vf{fs}{fsf}", 0x3D: f"mfir.{dest} vf{ft}, vi{is_}",
        0x3E: f"ilwr.{dest} vi{it}, (vi{is_})", 0x3F: f"iswr.{dest} vi{it}, (vi{is_})",
        0x40: f"rnext.{dest} vf{ft}, r", 0x41: f"rget.{dest} vf{ft}, r",
        0x42: f"rinit r, vf{fs}{fsf}", 0x43: f"rxor r, vf{fs}{fsf}",
        0x64: f"mfp.{dest} vf{ft}, p", 0x68: f"xtop vi{it}", 0x69: f"xitop vi{it}",
        0x6C: f"xgkick vi{is_}",
        0x70: f"esadd p, vf{fs}", 0x71: f"ersadd p, vf{fs}", 0x72: f"eleng p, vf{fs}",
        0x73: f"erleng p, vf{fs}", 0x74: f"eatanxy p, vf{fs}", 0x75: f"eatanxz p, vf{fs}",
        0x76: f"esum p, vf{fs}", 0x78: f"esqrt p, vf{fs}{fsf}", 0x79: f"ersqrt p, vf{fs}{fsf}",
        0x7A: f"ercpr p, vf{fs}{fsf}", 0x7B: "waitp", 0x7C: f"esin p, vf{fs}{fsf}",
        0x7D: f"eatan p, vf{fs}{fsf}", 0x7E: f"eexp p, vf{fs}{fsf}",
    }
    if sp == 0x30 and w == 0x8000033C:
        return "nop", None, None
    return t.get(sp, f"lower-special? {w:08x}"), None, None


def decode(micro, pc):
    lo, hi = struct.unpack_from("<II", micro, pc)
    u, fl = upper(hi)
    if hi >> 31 & 1:
        f = struct.unpack("<f", struct.pack("<I", lo))[0]
        return u, fl, f"loi {f:g} (0x{lo:08x})", None, None, hi
    l, k, t = lower(lo, pc)
    return u, fl, l, k, t, hi


def reach(micro, entry, limit):
    """Static flow from entry: follow fallthrough and branch targets until an E bit.
    JR/JALR targets are not known statically: JR ends the path, JALR falls through."""
    seen = set()
    work = [entry]
    calls = set()
    while work:
        pc = work.pop()
        while pc < limit and pc not in seen:
            seen.add(pc)
            _, _, _, k, t, hi = decode(micro, pc)
            if hi >> 30 & 1:      # E: the next pair (delay) runs, then stop
                seen.add(pc + 8)
                break
            if k in ("b", "bal"):
                work.append(t)
            if k == "B":
                seen.add(pc + 8)
                pc = t
                continue
            if k == "jr":
                seen.add(pc + 8)
                break
            if k == "jalr":
                calls.add(pc)
            pc += 8
    return seen, calls


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--blob", nargs="?", const=BLOB)
    ap.add_argument("--set", type=int, default=0)
    ap.add_argument("--micro")
    ap.add_argument("--entries", default="")
    ap.add_argument("--sets", action="store_true")
    ap.add_argument("--start", type=lambda x: int(x, 0), default=0)
    ap.add_argument("--end", type=lambda x: int(x, 0))
    a = ap.parse_args()
    if a.micro:
        micro = open(a.micro, "rb").read()
        size = a.end or len(micro)
    else:
        sets = blob_sets(a.blob or BLOB)
        if a.sets:
            for i, (off, m, size, ups) in enumerate(sets):
                print(f"set {i}: blob offset 0x{off:x}, size 0x{size:x} ({size // 8} pairs), "
                      f"uploads {', '.join(f'0x{x:x}x{n}' for x, n in ups)}")
            return
        _, micro, size, _ = sets[a.set]
    end = a.end or size
    entries = [int(x, 0) for x in a.entries.split(",") if x]
    labels = {e: f"entry_{e:04x}" for e in entries}
    for e in entries:
        r, calls = reach(micro, e, size)
        print(f"; entry 0x{e:04x}: {len(r)} pairs statically reachable, "
              f"{len(calls)} jalr sites", file=sys.stderr)
    for pc in range(a.start, end, 8):
        if pc in labels:
            print(f"\n{labels[pc]}:")
        u, fl, l, _, _, _ = decode(micro, pc)
        print(f"  {pc:04x}: {u:<40s} {l:<36s} {fl}")


if __name__ == "__main__":
    main()

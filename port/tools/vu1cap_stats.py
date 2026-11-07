#!/usr/bin/env python3
"""Summarise [debug] vu1_capture files (format: port/runtime/vu1_capture.h).

  vu1cap_stats.py FILE...            per entry pc: runs, MSCNT share, pairs, kicks, GIF prims
  --micro-out PREFIX                 also write each distinct MICR record to PREFIX_N.bin
  --sets                             name each MICR by the .vudata microcode set it equals
"""
import argparse
import collections
import struct
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
import vu_disasm  # noqa: E402

RUN_HDR = 10 * 4
REGS = 32 * 16 + 16 + 16 * 4 + 4 * 4 + 4 * 4
PRIMS = ["point", "line", "lstrip", "tri", "tstrip", "tfan", "sprite", "?"]


def records(path):
    with open(path, "rb") as f:
        while True:
            h = f.read(8)
            if len(h) < 8:
                return
            tag, n = struct.unpack("<II", h)
            yield tag, f.read(n)


def gif_summary(pk):
    """-> list of (prim or None, flg, nloop, nreg, regs) per tag."""
    out = []
    q = 0
    while q * 16 < len(pk):
        lo, hi = struct.unpack_from("<QQ", pk, q * 16)
        nloop = lo & 0x7FFF
        pre = lo >> 46 & 1
        prim = lo >> 47 & 0x7FF
        flg = lo >> 58 & 3
        nreg = lo >> 60 & 15 or 16
        regs = [(hi >> (4 * i)) & 15 for i in range(nreg)]
        out.append((prim if pre else None, flg, nloop, nreg, regs))
        body = nloop * nreg if flg == 0 else (nloop * nreg + 1) // 2 if flg == 1 else nloop
        q += 1 + body
    return out


def upk_fmt(code):
    cmd = code >> 24 & 0x7F
    vn, vl = cmd >> 2 & 3, cmd & 3
    name = ["S", "V2", "V3", "V4"][vn] + "-" + ["32", "16", "8", "5"][vl]
    if code >> 14 & 1:
        name += " usn"
    if cmd & 0x10:
        name += " mask"
    return name


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--micro-out")
    ap.add_argument("--sets", action="store_true")
    a = ap.parse_args()
    sets = vu_disasm.blob_sets(vu_disasm.BLOB) if a.sets else []
    st = collections.defaultdict(lambda: collections.Counter())
    formats = collections.defaultdict(collections.Counter)
    micros = []
    upk = collections.Counter()
    pending = []
    for path in a.files:
        mid_base = len(micros)
        for tag, body in records(path):
            if tag == 0x5243494D:
                if body not in micros:
                    micros.append(body)
                cur = micros.index(body)
                continue
            if tag == 0x204B5055:   # 'UPK '
                pending = [struct.unpack_from("<II", body, i) for i in range(0, len(body), 8)]
                continue
            frame, seq, pc, top, itop, kind, pairs, nkick, micro_id, end_pc = struct.unpack_from("<10I", body)
            key = (cur, pc)
            if kind:
                key = (cur, pc, struct.unpack_from("<I", body, RUN_HDR + 32 * 16 + 16 + 15 * 4)[0] * 8)
            for code, cyc in pending:
                if code >> 15 & 1 == 0:
                    continue   # absolute address: constants, counted separately below
                upk[(key, upk_fmt(code), (code & 0x3ff), cyc & 0xffff)] += 1
            pending = []
            c = st[key]
            c["runs"] += 1
            c["mscnt"] += kind
            c["pairs"] += pairs
            c["kicks"] += nkick
            p = RUN_HDR + REGS + 16384
            for _ in range(nkick):
                q, nqw = struct.unpack_from("<II", body, p)
                pk = body[p + 8:p + 8 + nqw * 16]
                p += 8 + nqw * 16
                for prim, flg, nloop, nreg, regs in gif_summary(pk):
                    if prim is not None:
                        c[PRIMS[prim & 7]] += 1
                        c["verts"] += nloop
                    formats[key][(prim if prim is None else prim & 0x7ff, flg, nreg, tuple(regs))] += 1
        del mid_base
    for i, m in enumerate(micros):
        name = ""
        for si, (_, sm, size, _) in enumerate(sets):
            if sm[:size] == m[:size]:
                name = f" = .vudata set {si} (0x{size:x} bytes)"
        print(f"micro {i}{name}")
        if a.micro_out:
            open(f"{a.micro_out}_{i}.bin", "wb").write(m)
    print(f"{'micro':>5} {'pc':>6} {'runs':>7} {'mscnt':>6} {'pairs/run':>9} {'kicks':>7} prims")
    for key, c in sorted(st.items()):
        mid, pc = key[0], key[1]
        if len(key) == 3:
            pc_txt = f"0x{pc:04x}>0x{key[2]:04x}"
        else:
            pc_txt = f"0x{pc:04x}"
        prims = " ".join(f"{k} {c[k]}" for k in PRIMS + ["verts"] if c[k])
        print(f"{mid:>5} {pc_txt:>13} {c['runs']:>7} {c['mscnt']:>6} {c['pairs'] / c['runs']:>9.1f} {c['kicks']:>7} {prims}")
    print("\nTOP-relative UNPACKs before MSCNT batches (key, format, qw offset, cl|wl<<8): count")
    for (key, fmt, off, cyc), n in sorted(upk.items()):
        if n >= 20:
            print(f"  m{key[0]} {'>'.join(hex(k) for k in key[1:])}: {fmt:<12} +{off:<4} cl/wl 0x{cyc:04x}: {n}")
    print("\nGIF tag formats per entry (prim|None, flg, nreg, regs): count")
    for key in sorted(formats):
        for fmt, n in formats[key].most_common(6):
            prim, flg, nreg, regs = fmt
            ptxt = "-" if prim is None else f"0x{prim:03x}"
            print(f"  m{key[0]} {'>'.join(hex(k) for k in key[1:])}: prim {ptxt} flg {flg} nreg {nreg} regs {[hex(r) for r in regs]}: {n}")


if __name__ == "__main__":
    main()

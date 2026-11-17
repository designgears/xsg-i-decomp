#!/usr/bin/env python3
"""Check that the skeleton build's data, once filled from the disc, equals the
full-data build's (gen_host_data.py without --skeleton) object by object.

Fills NEW's data offline the way datafill.c does (xsg_fill_tabs / xsg_fill_files
read from the executable, bytes from <game>/extracted-iso), then compares every
sized data object both executables define.  A word that differs passes when
both are pointers to the same symbol + offset (the two links lay out
differently).  Linux (ELF) executables only.

  verify_data_fill.py OLD_xsg NEW_xsg [--game game/extracted-iso]
"""
import argparse
import bisect
import collections
import os
import struct
import sys


class Elf:
    def __init__(self, path):
        d = self.d = open(path, "rb").read()
        shoff, = struct.unpack_from("<I", d, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x2E)
        raw = [struct.unpack_from("<10I", d, shoff + i * shentsize) for i in range(shnum)]
        stro = raw[shstrndx][4]
        self.secs = []
        for s in raw:
            self.secs.append(dict(name=self.cstr(stro + s[0]), type=s[1], flags=s[2], addr=s[3],
                                  off=s[4], size=s[5], link=s[6], entsize=s[9]))
        self.mem = {}
        for s in self.secs:
            if s["flags"] & 2 and s["addr"]:
                body = bytearray(d[s["off"]:s["off"] + s["size"]]) if s["type"] != 8 else bytearray(s["size"])
                self.mem[s["addr"]] = (s, body)
        self.starts = sorted(self.mem)
        self.syms = []
        for s in self.secs:
            if s["type"] != 2:
                continue
            strt = self.secs[s["link"]]["off"]
            for i in range(s["size"] // 16):
                n, v, sz, info, _, ndx = struct.unpack_from("<IIIBBH", d, s["off"] + i * 16)
                if 0 < ndx < len(self.secs) and v:
                    self.syms.append((self.cstr(strt + n), v, sz, info & 15, self.secs[ndx]["name"]))
        self.by_name = collections.defaultdict(list)
        for sym in self.syms:
            self.by_name[sym[0]].append(sym)
        iv = sorted((v, sz, n) for n, v, sz, t, sec in self.syms if t in (1, 2))
        self.iv_addr = [v for v, _, _ in iv]
        self.iv = iv

    def cstr(self, o):
        return self.d[o:self.d.index(b"\0", o)].decode("latin-1")

    def span(self, addr, n):
        i = bisect.bisect_right(self.starts, addr) - 1
        if i < 0:
            return None
        s, body = self.mem[self.starts[i]]
        o = addr - s["addr"]
        if o + n > len(body):
            return None
        return body, o

    def read(self, addr, n):
        sp = self.span(addr, n)
        return bytes(sp[0][sp[1]:sp[1] + n]) if sp else None

    def write(self, addr, data):
        body, o = self.span(addr, len(data))
        body[o:o + len(data)] = data

    def u32(self, addr):
        return struct.unpack("<I", self.read(addr, 4))[0]

    def symbolize(self, addr):
        i = bisect.bisect_right(self.iv_addr, addr) - 1
        while i >= 0:
            v, sz, n = self.iv[i]
            if addr < v + max(sz, 1) or addr == v + sz:
                return n, addr - v
            if v + sz < addr - 0x100000:
                break
            i -= 1
        return None


def fill(elf, game):
    files = []
    a = elf.by_name["xsg_fill_files"][0][1]
    while True:
        name, size, _ = struct.unpack("<III", elf.read(a, 12))
        if not name:
            break
        files.append(open(os.path.join(game, read_cstr(elf, name)), "rb").read())
        a += 12
    t = elf.by_name["xsg_fill_tabs"][0][1]
    runs = nbytes = 0
    while True:
        fi, tab = struct.unpack("<iI", elf.read(t, 8))
        if not tab:
            break
        e = tab
        while True:
            at, off, n, hn = struct.unpack("<IIII", elf.read(e, 16))
            if not at:
                break
            if n != hn:
                sys.exit("run at %#x: %d bytes, laid out %d" % (at, n, hn))
            elf.write(at, files[fi][off:off + n])
            runs += 1
            nbytes += n
            e += 16
        t += 8
    print("filled %d runs, %d bytes" % (runs, nbytes))


def read_cstr(elf, addr):
    out = b""
    while True:
        c = elf.read(addr, 1)
        if c == b"\0":
            return out.decode()
        out += c
        addr += 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("old")
    ap.add_argument("new")
    ap.add_argument("--game", default="game/extracted-iso")
    a = ap.parse_args()
    old, new = Elf(a.old), Elf(a.new)
    fill(new, a.game)
    data_secs = lambda s: s in (".data", ".bss", "xsgr_main") or s.startswith(("xsgd_", "xsgr_", "xsgb_"))
    runtime = {n for n, *_ in old.syms if n in ("ovl",)}  # the runtime's own tables
    checked = bad = ptr = 0
    for name, olds in old.by_name.items():
        news = new.by_name.get(name, [])
        for _, ov, osz, ot, osec in olds:
            if ot != 1 or not osz or not data_secs(osec) or name in runtime:
                continue
            cand = [x for x in news if x[4] == osec and x[2] == osz]
            if len(cand) != 1 or sum(1 for x in olds if x[4] == osec) != 1:
                continue
            nv = cand[0][1]
            ob, nb = old.read(ov, osz), new.read(nv, osz)
            if ob is None or nb is None:
                continue
            checked += 1
            if ob == nb:
                continue
            ok = True
            for i in range(osz):
                if ob[i] == nb[i] or (i & 3 and ob[i & ~3:(i & ~3) + 4] != nb[i & ~3:(i & ~3) + 4]):
                    continue
                w = i & ~3  # the word holding the byte (objects are word-aligned)
                if w + 4 > osz:
                    ok = False
                    break
                ow, nw = struct.unpack_from("<I", ob, w)[0], struct.unpack_from("<I", nb, w)[0]
                so, sn = old.symbolize(ow), new.symbolize(nw)
                if so and so == sn:
                    ptr += 1
                    continue
                ok = False
                print("MISMATCH %s (%s) +%#x: old %08x %s new %08x %s" % (name, osec, w, ow, so, nw, sn))
                break
            if not ok:
                bad += 1
    print("%d objects compared, %d mismatched, %d pointer words equivalent" % (checked, bad, ptr))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Behavioural (differential execution) gate for candidate bodies (2026-10-19).

semgate.py compares static facts; it cannot see a wrong branch condition, a
loop pointer that is never advanced, a load from 0x10 instead of 0x14, a wrong
base object or a missing store. This module runs the TARGET function (the
INCLUDE_ASM bytes of try_fn's base object) and the CANDIDATE (try_fn's compiled
object) in a small R5900 interpreter from the same pseudo-random initial state
and compares what they do:

  * every byte written to memory outside the stack frame (keyed by real address
    or, for TU-private sections, by symbol+offset; float stores compared with a
    relative tolerance; stack-pointer values stored are equal tokens),
  * the sequence of outgoing calls: callee + argument registers (from the
    callee's protos.json prototype: GPR args a0-a3/t0-t3, float args f12..;
    without a prototype a0-a3 where both sides hold a defined value),
  * the return value (width from the prototype / the body's definition).

Initial state per seed: argument registers and memory get values from a mix of
0 / small ints / heap-like pointers / floats / random words, hashed from
(seed, address) so both sides see the same world. Memory that is part of the
game image (SLUS + the unit's overlay, game/extracted-iso) reads the real
bytes; globals map to their real addresses (config/symbols, undefined_syms),
TU-private sections (.rodata/.lit4/.data/.bss of each object) to fixed fake
addresses with their own bytes and R_MIPS_32 relocations applied. Callees are
stubs: the call is logged, v0/f0 get a value hashed from (seed, callee,
occurrence), caller-saved registers are poisoned (side-specific values that are
never compared). Caller-saved registers that are not arguments start poisoned
too, the frame below $sp starts zeroed and its writes are ignored.

Result: {"ok": True | False | None, "detail", "seeds", "cov", "ms"}; None
("unknown") for unsupported instructions (VU0 / most MMI / COP0), unresolvable
relocations, jumps out of the function or a target that hits the step cap.
A candidate that keeps running far past the target's step count on the same
seed (a loop that never advances) is a definite difference.

    python3 behgate.py UNIT NAME BASE.o CAND.o [--body BODY.c] [-v]
"""
from __future__ import annotations

import bisect
import json
import math
import os
import re
import struct
import sys
import time
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT / "tools" / "tu") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools" / "tu"))
from elfinfo import Elf  # noqa: E402

VERSION = 1
M64 = (1 << 64) - 1
M32 = 0xFFFFFFFF
GP = 0x004DFB70
SP0 = 0x7E000000
STACK_LO, STACK_HI = SP0 - 0x40000, SP0 + 0x400      # writes here are frame scratch (ignored)
RA_MAGIC = 0x0FFFFFF0
TEXT_FAKE = 0x2F000000
SEC_FAKE = {".rodata": 0x30000000, ".data": 0x31000000, ".sdata": 0x32000000, ".sbss": 0x33000000,
            ".bss": 0x34000000, ".lit4": 0x35000000, ".lit8": 0x36000000}
POISON_G = 0xBAD00000
POISON_F = 0x7F3A0000
FTOL = 1e-4
LOW_ALIAS = 0x10000
OUT_BYTES = 256
STRICT_UNPROTO = bool(os.environ.get('BEH_STRICT'))
DEFAULT_SEEDS = 1000
TARGET_CAP = 5000
CAND_CAP = 40000
TRACE: list = [int(os.environ["BEH_TRACE"])] if os.environ.get("BEH_TRACE") else []


def TRACE_REGS(m) -> str:
    names = ("zr at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 s0 s1 s2 s3 s4 s5 s6 s7 t8 t9 k0 k1 gp sp s8 ra").split()
    g = " ".join(f"{names[i]}={m.R[i] & M32:x}" for i in range(1, 32) if i not in (26, 27, 28)
                 and (m.R[i] & 0xFFF00000) != POISON_G)
    f = " ".join(f"f{i}={b2f(m.F[i]):g}" for i in range(32) if (m.F[i] & 0xFFFF0000) != POISON_F
                 and i not in range(20, 32))
    return g + " | " + f

R_32, R_26, R_HI16, R_LO16, R_GPREL16, R_LITERAL = 2, 4, 5, 6, 7, 8


class Unknown(Exception):
    pass


class Stop(Exception):
    pass


def sx32(v: int) -> int:
    """sign-extend the low 32 bits to a 64-bit register value (unsigned repr)."""
    v &= M32
    return v | 0xFFFFFFFF00000000 if v & 0x80000000 else v


def s32(v: int) -> int:
    v &= M32
    return v - (1 << 32) if v & 0x80000000 else v


def s64(v: int) -> int:
    v &= M64
    return v - (1 << 64) if v >> 63 else v


def simm(w: int) -> int:
    i = w & 0xFFFF
    return i - 0x10000 if i & 0x8000 else i


def mix(*xs) -> int:
    h = 0x9E3779B97F4A7C15
    for x in xs:
        h = (h ^ (x & M64)) * 0xBF58476D1CE4E5B9 & M64
        h ^= h >> 31
        h = h * 0x94D049BB133111EB & M64
        h ^= h >> 29
    return h


_FS, _IS = struct.Struct("<f"), struct.Struct("<I")
FLT_MAX_BITS = 0x7F7FFFFF


def b2f(b: int) -> float:
    b &= M32
    e = (b >> 23) & 0xFF
    if e == 0:
        return -0.0 if b >> 31 else 0.0                  # EE: denormals are zero
    if e == 0xFF:                                        # EE: no inf / NaN, just big numbers
        v = math.ldexp(1.0 + (b & 0x7FFFFF) / 8388608.0, 128)
        return -v if b >> 31 else v
    return _FS.unpack(_IS.pack(b))[0]


def f2b(x: float) -> int:
    if x != x:
        return FLT_MAX_BITS
    ax = abs(x)
    if ax >= 3.4028235677973366e38:
        return FLT_MAX_BITS | (0x80000000 if x < 0 else 0)
    if ax < 1.1754943508222875e-38:
        return 0x80000000 if math.copysign(1.0, x) < 0 else 0
    try:
        return _IS.unpack(_FS.pack(x))[0]
    except OverflowError:
        return FLT_MAX_BITS | (0x80000000 if x < 0 else 0)


def rvalue(h: int, dic: tuple = (), dshare: int = 15) -> int:
    """32-bit pseudo-random value from hash H: mostly 0 / small / pointer / float;
    with DIC (the target's own constants) 15% are one of them or a neighbour."""
    sel, r = h % 100, h >> 8
    if dic and sel >= 100 - dshare:
        return dict_value(dic, r)
    sel = sel * 100 // (100 - dshare) if dic else sel
    if sel < 25:
        return 0
    if sel < 45:
        return 1 if r % 3 == 0 else 1 + (r >> 2) % 8
    if sel < 58:
        return 0x01000000 + (r & 0xFFFF) * 0x1000      # 4 KB apart: no accidental aliasing
    if sel < 72:
        return f2b(((r % 20001) - 10000) / 100.0)
    if sel < 85:
        return r % 96
    if sel < 90:
        return (-(1 + r % 8)) & M32
    return r & M32


def rret(h: int, dic: tuple = (), kind: str = "") -> int:
    """stub return value: callees mostly return flags / counts / pointers (KIND:
    the callee's prototype return kind, 'p' pointer, 'g..' integer)."""
    sel = h % 100
    if sel < 25:
        return 0
    if kind == "p":
        r = h // 100
        return 0x01000000 + (r & 0xFFFF) * 0x1000 if sel < 85 else rvalue(r, dic, 25)
    if sel < 45:
        return 1
    if kind.startswith("g") and sel < 85:
        r = h // 100
        return dict_value(dic, r) if dic and sel >= 70 else (2 + r % 14 if sel < 60 else (-(1 + r % 4)) & M32)
    return rvalue(h // 100, dic, 25)


def dict_value(dic: tuple, r: int) -> int:
    kind, v = dic[r % len(dic)]
    r //= len(dic)
    if kind == "f":
        x = b2f(v)
        return f2b((x, x + 0.5, x - 0.5, x * 1.001 + 0.001)[r % 4])
    return (v + (0, 1, -1)[r % 3]) & M32


def rfloat(h: int, dic: tuple = ()) -> int:
    r = h >> 8
    if dic and h % 100 >= 85:
        fl = tuple(x for x in dic if x[0] == "f")
        if fl:
            return dict_value(fl, r)
    return 0 if h % 10 == 0 else f2b(((r % 20001) - 10000) / 100.0)


# ------------------------------------------------------------- images ----

_IMG: dict = {}


def game_dir() -> Path:
    env = os.environ.get("XENO_GAME_DIR")
    if env:
        return Path(env)
    try:
        g = json.loads((ROOT / "config/toolchain-local.json").read_text()).get("game_dir")
        if g:
            return Path(g)
    except (OSError, ValueError):
        pass
    return ROOT / "game/extracted-iso"


def rom_image(unit: str) -> list:
    """[(lo, hi, bytes)] of the game image the unit runs in (overlay first)."""
    if unit in _IMG:
        return _IMG[unit]
    segs = []
    try:
        units = json.loads((ROOT / "config/originals.json").read_text())["units"]
        names = ([unit] if unit != "main" else []) + ["main"]
        for u in names:
            f = units.get(u, {}).get("file")
            if not f:
                continue
            e = Elf((game_dir() / f).read_bytes())
            for p in e.phdrs:
                if p.type == 1 and p.filesz:
                    segs.append((p.vaddr, p.vaddr + p.filesz, e.data[p.offset:p.offset + p.filesz]))
    except (OSError, ValueError, KeyError):
        segs = []
    _IMG[unit] = segs
    return segs


# ------------------------------------------------------------- objects ----

class Obj:
    """One relocatable object: section fake bases, symbols, relocations."""

    def __init__(self, data: bytes, unit: str, sym_address):
        self.elf = e = Elf(data)
        self.unit = unit
        self._sa = sym_address
        self.base: dict = {}
        k = 0
        for s in e.sections:
            if s.flags & 2 and s.size:                       # SHF_ALLOC
                if s.flags & 4:                              # SHF_EXECINSTR
                    self.base[s.index] = TEXT_FAKE + 0x100000 * k
                    k += 1
                else:
                    self.base[s.index] = SEC_FAKE.get(s.name, 0x37000000 + 0x100000 * s.index)
        self.syms = e.symbols
        self.rels: dict = {}                                 # target section idx -> [(off, type, symidx)]
        for s in e.sections:
            if s.type == 9:
                lst = self.rels.setdefault(s.info, [])
                for i in range(s.size // 8):
                    off, info = struct.unpack_from("<II", e.data, s.offset + i * 8)
                    lst.append((off, info & 0xFF, info >> 8))
            elif s.type == 4:
                raise Unknown("RELA relocations")
        self.symcache: dict = {}
        self.gp0 = 0
        for s in e.sections:
            if s.name == ".reginfo" and s.size >= 24:
                self.gp0 = struct.unpack_from("<I", e.data, s.offset + 20)[0]

    def symname(self, idx: int) -> str:
        s = self.syms[idx]
        if s.name:
            return s.name
        if 0 < s.shndx < len(self.elf.sections):
            return self.elf.sections[s.shndx].name
        return ""

    def symval(self, idx: int) -> int:
        if idx in self.symcache:
            return self.symcache[idx]
        s = self.syms[idx]
        v = None
        if s.type == 3 or not s.name:                         # section symbol
            v = self.base.get(s.shndx)
        else:
            a = self._sa(self.unit, s.name) if self._sa else None
            if a is None:
                m = re.fullmatch(r"(?:D|jtbl|func|L|lbl|jlabel)_([0-9A-Fa-f]{8})", s.name)
                a = int(m.group(1), 16) if m else None
            if a is not None:
                v = a
            elif 0 < s.shndx < 0xFF00 and s.shndx in self.base:
                v = self.base[s.shndx] + s.value
            elif s.shndx == 0:
                v = 0x40000000 + (zlib.crc32(s.name.encode()) & 0xFFFFF) * 16
        if v is None:
            raise Unknown(f"unresolvable symbol {self.symname(idx)}")
        self.symcache[idx] = v
        return v

    def images(self) -> list:
        """[(lo, hi, bytes)] of the private data sections (R_MIPS_32 applied)."""
        out = []
        for s in self.elf.sections:
            if s.index not in self.base or s.flags & 4 or s.type == 8:
                continue
            b = bytearray(self.elf.data[s.offset:s.offset + s.size])
            for off, t, si in self.rels.get(s.index, []):
                if t == R_32 and off + 4 <= len(b):
                    try:
                        v = self.symval(si)
                    except Unknown:
                        continue
                    a = struct.unpack_from("<I", b, off)[0]
                    struct.pack_into("<I", b, off, (v + a) & M32)
            lo = self.base[s.index]
            out.append((lo, lo + len(b), bytes(b)))
        return out

    def locate(self, addr: int):
        """(symbol name, offset) of a private-section address, else None."""
        for idx, lo in self.base.items():
            sec = self.elf.sections[idx]
            if lo <= addr < lo + sec.size:
                best = None
                types = (2,) if sec.flags & 4 else (0, 1)
                for s in self.syms:
                    if s.shndx == idx and s.name and s.type in types and not s.name.startswith(("$", ".")) \
                            and s.value <= addr - lo and (best is None or s.value > best.value):
                        best = s
                if best is not None and (best.size == 0 or addr - lo < best.value + best.size):
                    return (best.name, addr - lo - best.value)
                return (sec.name, addr - lo)
        return None

    def func(self, name: str):
        """(section idx, start, end) of function NAME."""
        e = self.elf
        c = [s for s in self.syms if s.name == name and 0 < s.shndx < len(e.sections)]
        if not c:
            alt = re.compile(re.escape(name) + r"_[0-9A-Fa-f]{8}$")
            c = [s for s in self.syms if alt.match(s.name) and 0 < s.shndx < len(e.sections)]
        if not c:
            m = re.match(r"(.+)_[0-9A-Fa-f]{8}$", name)
            if m:
                c = [s for s in self.syms if s.name == m.group(1) and 0 < s.shndx < len(e.sections)]
        if not c:
            raise Unknown(f"symbol {name} not in object")
        s = c[0]
        sec = e.sections[s.shndx]
        start = s.value
        if s.size:
            end = start + s.size
        else:
            nx = sorted(x.value for x in self.syms if x.shndx == s.shndx and x.value > start and x.name
                        and not x.name.startswith((".L", "$L", "$")) and x.type in (0, 2))
            end = nx[0] if nx else sec.size
        data = e.data[sec.offset + start:sec.offset + end]
        while len(data) >= 4 and data[-4:] == b"\0\0\0\0":
            data = data[:-4]
        return s.shndx, start, start + len(data)

    def text_names(self, idx: int) -> dict:
        out: dict = {}
        for t in (2, 0):                                   # FUNC symbols win over labels
            for s in self.syms:
                if s.shndx == idx and s.name and not s.name.startswith((".", "$")) and s.type == t \
                        and s.name != "gcc2_compiled.":
                    out.setdefault(s.value, s.name)
        return out


# ------------------------------------------------------------- prototypes ----

_INT8 = re.compile(r"\b(char|u8|s8|u_char|uchar|bool|u_int8_t|int8_t)\b")
_INT16 = re.compile(r"\b(short|u16|s16|u_short|ushort|int16_t|u_int16_t)\b")
_INT64 = re.compile(r"\b(long\s+long|u64|s64|u_long|long|int64_t|u_int64_t|u_long64|long64)\b")
_INT128 = re.compile(r"\b(u_long128|long128|u128|s128|__int128)\b")


def _split_params(a: str) -> list:
    out, depth, cur = [], 0, ""
    for ch in a:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return [x.strip() for x in out]


def parse_proto(proto: str | None, name: str):
    """(ret kind, [param kinds]) or None. kinds: 'v' void, 'g8','g16','g32','g64' GPR
    values, 'p' pointer, 'f' float, '?' unknown (struct / variadic tail)."""
    if not proto:
        return None
    m = re.match(r"^\s*(?:extern\s+|static\s+|inline\s+)*(.*?)\b" + re.escape(name) + r"\s*\((.*)\)\s*[;{]?\s*$",
                 proto.strip(), re.S)
    if not m:
        return None
    ret, args = m.group(1).strip(), m.group(2).strip()
    def kind(t: str) -> str:
        t = re.sub(r"\b(const|volatile|register|signed|unsigned|static)\b", " ", t)
        if "(" in t or "*" in t or "[" in t:
            return "p"
        if re.search(r"\bvoid\b", t):
            return "v"
        if re.search(r"\bfloat\b", t):
            return "f"
        if re.search(r"\bdouble\b", t):
            return "g64"
        if re.search(r"\b(struct|union)\b", t):
            return "?"
        if _INT128.search(t):
            return "?"
        if _INT64.search(t):
            return "g64"
        if _INT16.search(t):
            return "g16"
        if _INT8.search(t):
            return "g8"
        return "g32"
    rk = kind(ret) if ret else "g32"
    ps = []
    if args not in ("", "void"):
        for p in _split_params(args):
            if p == "...":
                ps.append("...")
            else:
                ps.append(kind(p))
    return rk, ps


def body_proto(body_text: str, name: str) -> str | None:
    m = re.search(r"^([^\n;{}()]*?\b" + re.escape(name) + r"\s*\([^;{]*\))\s*\{", body_text, re.M)
    return m.group(1) if m else None


# ------------------------------------------------------------- decoder ----
# own R5900 opcode decoder (try_fn runs under the system python: no rabbitizer)

_SPECIAL = {0: "sll", 2: "srl", 3: "sra", 4: "sllv", 6: "srlv", 7: "srav", 8: "jr", 9: "jalr", 10: "movz",
            11: "movn", 12: "syscall", 13: "break", 15: "sync", 16: "mfhi", 17: "mthi", 18: "mflo", 19: "mtlo",
            20: "dsllv", 22: "dsrlv", 23: "dsrav", 24: "mult", 25: "multu", 26: "div", 27: "divu", 32: "add",
            33: "addu", 34: "sub", 35: "subu", 36: "and", 37: "or", 38: "xor", 39: "nor", 40: "mfsa",
            41: "mtsa", 42: "slt", 43: "sltu", 44: "dadd", 45: "daddu", 46: "dsub", 47: "dsubu", 48: "tge",
            49: "tgeu", 50: "tlt", 51: "tltu", 52: "teq", 54: "tne", 56: "dsll", 58: "dsrl", 59: "dsra",
            60: "dsll32", 62: "dsrl32", 63: "dsra32"}
_REGIMM = {0: "bltz", 1: "bgez", 2: "bltzl", 3: "bgezl", 8: "tgei", 9: "tgeiu", 10: "tlti", 11: "tltiu",
           12: "teqi", 14: "tnei", 16: "bltzal", 17: "bgezal", 18: "bltzall", 19: "bgezall", 24: "mtsab",
           25: "mtsah"}
_OPC = {2: "j", 3: "jal", 4: "beq", 5: "bne", 6: "blez", 7: "bgtz", 8: "addi", 9: "addiu", 10: "slti",
        11: "sltiu", 12: "andi", 13: "ori", 14: "xori", 15: "lui", 20: "beql", 21: "bnel", 22: "blezl",
        23: "bgtzl", 24: "daddi", 25: "daddiu", 26: "ldl", 27: "ldr", 30: "lq", 31: "sq", 32: "lb", 33: "lh",
        34: "lwl", 35: "lw", 36: "lbu", 37: "lhu", 38: "lwr", 39: "lwu", 40: "sb", 41: "sh", 42: "swl",
        43: "sw", 44: "sdl", 45: "sdr", 46: "swr", 47: "cache", 49: "lwc1", 51: "pref", 54: "lqc2", 55: "ld",
        57: "swc1", 62: "sqc2", 63: "sd"}
_COP1S = {0: "add.s", 1: "sub.s", 2: "mul.s", 3: "div.s", 4: "sqrt.s", 5: "abs.s", 6: "mov.s", 7: "neg.s",
          22: "rsqrt.s", 24: "adda.s", 25: "suba.s", 26: "mula.s", 28: "madd.s", 29: "msub.s", 30: "madda.s",
          31: "msuba.s", 36: "cvt.w.s", 40: "max.s", 41: "min.s", 48: "c.f.s", 50: "c.eq.s", 52: "c.lt.s",
          54: "c.le.s"}
_MMI = {0: "madd", 1: "maddu", 16: "mfhi1", 17: "mthi1", 18: "mflo1", 19: "mtlo1", 24: "mult1",
        25: "multu1", 26: "div1", 27: "divu1", 32: "madd1", 33: "maddu1"}
_MMISUB = {(8, 18): "pextlw", (40, 18): "pextuw", (9, 14): "pcpyld", (9, 18): "pand", (9, 19): "pxor",
           (41, 14): "pcpyud", (41, 18): "por", (41, 19): "pnor"}


def decode(w: int) -> str:
    """mnemonic of W (the subset step() implements; anything else -> '?<class>')."""
    o = w >> 26
    if o == 0:
        return _SPECIAL.get(w & 63, "?special")
    if o == 1:
        return _REGIMM.get((w >> 16) & 31, "?regimm")
    if o == 17:
        rs = (w >> 21) & 31
        if rs == 0:
            return "mfc1"
        if rs == 2:
            return "cfc1"
        if rs == 4:
            return "mtc1"
        if rs == 6:
            return "ctc1"
        if rs == 8:
            return ("bc1f", "bc1t", "bc1fl", "bc1tl")[(w >> 16) & 3]
        if rs == 16:
            return _COP1S.get(w & 63, "?cop1")
        if rs == 20 and w & 63 == 32:
            return "cvt.s.w"
        return "?cop1"
    if o == 28:
        f = w & 63
        if f in _MMI:
            return _MMI[f]
        if f in (8, 9, 40, 41):
            return _MMISUB.get((f, (w >> 6) & 31), "?mmi")
        return "?mmi"
    if o in (16, 18):
        return "?cop0" if o == 16 else "?cop2"
    return _OPC.get(o, "?op")


# ------------------------------------------------------------- program ----

class Program:
    """Pre-decoded function: per word (op name, word, reloc info)."""

    def __init__(self, obj: Obj, name: str, real_start: int | None):
        self.obj = obj
        sidx, start, end = obj.func(name)
        self.sidx, self.start, self.end = sidx, start, end
        self.real_start = real_start
        sec = obj.elf.sections[sidx]
        raw = obj.elf.data[sec.offset + start:sec.offset + end]
        self.words = [struct.unpack_from("<I", raw, i)[0] for i in range(0, len(raw), 4)]
        self.ops = [decode(w) for w in self.words]
        self.names = obj.text_names(sidx)
        self.textbase = obj.base.get(sidx, TEXT_FAKE)
        # relocation info per word index
        self.rel: dict = {}
        rels = obj.rels.get(sidx, [])
        sec_words = lambda o: struct.unpack_from("<I", obj.elf.data, sec.offset + o)[0]  # noqa: E731
        for k, (off, t, si) in enumerate(rels):
            if not (start <= off < end):
                continue
            i = (off - start) // 4
            w = sec_words(off)
            if t == R_26:
                s = obj.syms[si]
                field = (w & 0x3FFFFFF) << 2
                if s.type == 3 or not s.name:
                    tgt = field if s.shndx == sidx else None
                elif s.shndx == sidx:
                    tgt = s.value + field
                else:
                    tgt = None
                if tgt is not None and start <= tgt < end and self.ops[i] == "j":
                    self.rel[i] = ("jlocal", (tgt - start) // 4)
                elif tgt is not None:
                    self.rel[i] = ("call", self.names.get(tgt, f"<local+{tgt:x}>"))
                else:
                    self.rel[i] = ("call", obj.symname(si))
            elif t == R_HI16:
                alo = 0
                for off2, t2, si2 in rels[k + 1:]:
                    if t2 == R_LO16 and si2 == si:
                        alo = simm(sec_words(off2))
                        break
                v = obj.symval(si) + ((w & 0xFFFF) << 16) + alo
                self.rel[i] = ("imm", ((v + 0x8000) >> 16) & 0xFFFF)
            elif t == R_LO16:
                v = obj.symval(si) + simm(w)
                self.rel[i] = ("imm", v & 0xFFFF)
            elif t in (R_GPREL16, R_LITERAL):
                sy = obj.syms[si]
                local = sy.bind == 0 or sy.type == 3 or not sy.name   # ABI: GP0 applies to locals
                v = obj.symval(si) + simm(w) + (obj.gp0 if local else 0)
                self.rel[i] = ("abs", v & M32)
            else:
                raise Unknown(f"relocation type {t} at +{off - start:#x}")

    def constants(self, images: list) -> tuple:
        """the function's own integer immediates / float literals (seed dictionary)."""
        ints, fls = set(), set()
        for i, (w, op) in enumerate(zip(self.words, self.ops)):
            rel = self.rel.get(i)
            if op in ("andi", "ori", "xori", "slti", "sltiu", "addiu", "daddiu") and not rel:
                v = w & 0xFFFF if op in ("andi", "ori", "xori") else simm(w)
                if (w >> 21) & 31 == 0 or op in ("slti", "sltiu", "xori", "andi", "ori"):
                    ints.add(v & M32)
                elif op in ("addiu", "daddiu") and -0x1000 < v < 0:
                    ints.add(-v)                         # x - K: x == K is interesting
            elif op == "lui" and not rel:
                ints.add((w & 0xFFFF) << 16)
                fls.add((w & 0xFFFF) << 16)
            elif op == "lwc1" and rel and rel[0] in ("abs", "imm"):
                pass
            if op == "lwc1" and rel and rel[0] == "abs":
                a = rel[1]
                for lo, hi, data in images:
                    if lo <= a and a + 4 <= hi:
                        fls.add(struct.unpack_from("<I", data, a - lo)[0])
                        break
        ints -= {0, 1, M32}
        out = tuple(sorted(("i", v) for v in ints)) + tuple(sorted(("f", v) for v in fls if v & 0x7F800000))
        return out[:64]

    def jr_index(self, addr: int):
        addr &= M32
        lo = self.textbase + self.start
        if lo <= addr < self.textbase + self.end and not (addr - lo) & 3:
            return (addr - lo) // 4
        if self.real_start is not None and self.real_start <= addr < self.real_start + (self.end - self.start) \
                and not (addr - self.real_start) & 3:
            return (addr - self.real_start) // 4
        return None


# ------------------------------------------------------------- machine ----

ARGG = (4, 5, 6, 7, 8, 9, 10, 11)                      # a0-a3, t0-t3 (EABI)
CALLER_G = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25)
CALLEE_G = (16, 17, 18, 19, 20, 21, 22, 23, 30)


class Machine:
    def __init__(self, prog: Program, side: int, seed: int, images: list, cap: int, protos, dic: tuple = ()):
        self.p, self.side, self.seed, self.cap = prog, side, seed, cap
        self.dic = dic
        self.protos = protos
        self.R = [0] * 32
        self.RH = [0] * 32
        self.F = [0] * 32
        self.hi = self.lo = self.hi1 = self.lo1 = 0
        self.acc = 0.0
        self.fcc = False
        self.W: dict = {}                  # byte address -> byte
        self.WW: set = set()               # word indices with written bytes
        self.ST: dict = {}                 # store start -> (size, value, kind)
        self.OWN: dict = {}                # byte address -> store start
        self.IC: dict = {}                 # word index -> initial word
        self.imgs = images
        self.img_lo = [x[0] for x in images]
        self.calls: list = []
        self.ncall: dict = {}
        self.cov: set = set()
        self.steps = 0
        self.deadline = time.time() + 1e9
        self.uninit = False
        R, F = self.R, self.F
        for r in range(32):
            R[r] = sx32(POISON_G | (side << 12) | (r << 4))
        for r in ARGG + CALLEE_G:
            R[r] = sx32(rvalue(mix(seed, 1, r), self.dic, 35))
        R[0] = 0
        R[28] = GP
        R[29] = SP0
        R[31] = RA_MAGIC
        R[26] = R[27] = 0
        for f in range(32):
            F[f] = POISON_F | (side << 8) | f
        for f in list(range(12, 20)) + list(range(20, 32)):
            F[f] = rfloat(mix(seed, 2, f), self.dic)

    # ---- memory
    def initword(self, wi: int) -> int:
        v = self.IC.get(wi)
        if v is not None:
            return v
        a = wi << 2
        if STACK_LO <= a < SP0:
            v = 0
            self.uninit = True             # a frame slot read before any write
        else:
            v = None
            for lo, hi, data in self.imgs:
                if lo <= a and a + 4 <= hi:
                    v = struct.unpack_from("<I", data, a - lo)[0]
                    break
                if lo <= a < hi or lo < a + 4 <= hi:
                    v = 0
                    for k in range(4):
                        if lo <= a + k < hi:
                            v |= data[a + k - lo] << (8 * k)
                    break
            if v is None:
                v = rvalue(mix(self.seed, 3, wi), self.dic)
        self.IC[wi] = v
        return v

    def rd8(self, a: int) -> int:
        v = self.W.get(a)
        if v is None:
            return (self.initword(a >> 2) >> (8 * (a & 3))) & 0xFF
        return v

    def rd(self, a: int, n: int) -> int:
        a &= M32
        wi = a >> 2
        if wi not in self.WW and (a + n - 1) >> 2 == wi:
            v = self.initword(wi)
            return v if n == 4 else (v >> (8 * (a & 3))) & ((1 << (8 * n)) - 1)
        v = 0
        for k in range(n):
            v |= self.rd8((a + k) & M32) << (8 * k)
        return v

    def wr(self, a: int, n: int, v: int, kind: str = "") -> None:
        a &= M32
        W, OWN = self.W, self.OWN
        for k in range(n):
            b = (a + k) & M32
            W[b] = (v >> (8 * k)) & 0xFF
            OWN[b] = a
            self.WW.add(b >> 2)
        if kind == "" and n == 4 and STACK_LO <= v < STACK_HI:
            kind = "sp"
        self.ST[a] = (n, v, kind)

    # ---- canonical values (call args / returns / stored pointers)
    def cstr(self, a: int):
        a &= M32
        if not any(lo <= a < hi for lo, hi, _d in self.imgs):
            return None                              # random memory is never a string
        s = bytearray()
        for k in range(200):
            c = self.rd8(a + k)
            if c == 0:
                break
            if not (32 <= c < 127 or c in (9, 10, 13)) and c < 0x80:
                return None
            s.append(c)
        else:
            return None
        return bytes(s).decode("latin-1") if len(s) >= 2 else None

    def canon(self, v: int, kind: str = "g32"):
        if kind == "f":
            if (v & 0xFFFF0000) == POISON_F:
                return "?"
            return ("f", b2f(v))
        if (v & 0xFFF00000) == POISON_G and (kind != "g64" or (v >> 32) in (0, M32)):
            return "?"
        if kind == "g8":
            return v & 0xFF
        if kind == "g16":
            return v & 0xFFFF
        if kind == "g64":
            return v & M64
        x = v & M32
        if STACK_LO <= x < STACK_HI:
            return "SP"
        if kind == "p" or kind == "g32":
            loc = self.p.obj.locate(x)
            if loc:
                st = self.cstr(x)
                return ("str", st) if st is not None else ("sym",) + loc
            st = self.cstr(x) if x >= 0x100000 else None
            if st is not None:
                return ("str", st)
        return x

    # ---- calls
    def do_call(self, callee) -> None:
        R, F = self.R, self.F
        spec = self.protos(callee) if isinstance(callee, str) else None
        args = []
        if spec is not None:
            gi = fi = 0
            for k in spec[1]:
                if k == "...":
                    break
                if k == "f":
                    if fi < 8:
                        args.append(self.canon(F[12 + fi], "f"))
                    fi += 1
                elif k == "?":
                    args.append("?")
                    gi += 1
                else:
                    if gi < 8:
                        args.append(self.canon(R[ARGG[gi]], k))
                    gi += 1
        else:
            args = [self.canon(R[r]) for r in (4, 5, 6, 7)]
        self.calls.append((callee, tuple(args), spec is None))
        key = callee if isinstance(callee, str) else repr(callee)
        occ = self.ncall.get(key, 0)
        self.ncall[key] = occ + 1
        h = mix(self.seed, 4, zlib.crc32(key.encode()), occ)
        # out-parameters in the frame: the callee fills OUT_BYTES at each stack
        # pointer argument with values that depend only on (callee, occurrence,
        # argument, offset), so both sides see the same "result" whatever their layout
        nreg = 4 if spec is None else min(8, sum(1 for k in spec[1] if k not in ("f", "...")))
        for j in range(nreg):
            pv = R[ARGG[j]] & M32
            if STACK_LO <= pv < SP0:
                for o in range(0, min(OUT_BYTES, SP0 - pv), 4):
                    q = pv + o
                    if o >= 16 and any(q + k in self.W for k in range(4)):
                        continue                 # a live local next to the buffer: keep it
                    self.wr(q, 4, rvalue(mix(h, 6, j, o), self.dic))
        for r in CALLER_G:
            R[r] = sx32(POISON_G | (self.side << 12) | (r << 4) | 1)
        for f in range(0, 20):
            F[f] = POISON_F | (self.side << 8) | f | 0x80
        R[2] = sx32(rret(h, self.dic, spec[0] if spec else ""))
        R[3] = 0
        F[0] = rfloat(mix(h, 5), self.dic)
        self.hi = self.lo = self.hi1 = self.lo1 = 0

    # ---- run
    def run(self) -> str:
        p = self.p
        n = len(p.words)
        pc = 0
        try:
            while True:
                if pc < 0 or pc >= n:
                    raise Unknown(f"pc out of function (+{pc * 4:#x})")
                self.steps += 1
                if self.steps > self.cap:
                    return "cap"
                if TRACE and TRACE[0] == self.side:
                    print(f"{self.side} {pc * 4:5x} {self.p.ops[pc]:8s} {TRACE_REGS(self)}", file=sys.stderr)
                if not self.steps & 4095 and time.time() > self.deadline:
                    raise Unknown("time budget")
                r = self.step(pc)
                if r is None:
                    pc += 1
                    continue
                kind, val, likely = r
                if kind == "br":                         # conditional branch
                    self.cov.add((pc, val is not None))
                    if val is None and likely:
                        pc += 2
                        continue
                    self.delay(pc + 1)
                    pc = val if val is not None else pc + 2
                elif kind == "j":
                    self.delay(pc + 1)
                    pc = val
                elif kind == "call":
                    self.delay(pc + 1)
                    self.do_call(val)
                    if likely:                           # tail call (j)
                        return "ret"
                    pc += 2
                elif kind == "jr":
                    self.delay(pc + 1)
                    if val == RA_MAGIC:
                        return "ret"
                    i = p.jr_index(val)
                    if i is None:
                        raise Unknown(f"jr to {val:#x}")
                    pc = i
                elif kind == "stop":
                    return val
        except Stop as e:
            return str(e)

    def delay(self, i: int) -> None:
        self.steps += 1
        if i >= len(self.p.words):
            return                                   # the trailing nop stripped as padding
        if self.step(i) is not None:
            raise Unknown("control transfer in a delay slot")

    def step(self, i: int):  # noqa: C901 - one big decoder
        p = self.p
        w = p.words[i]
        op = p.ops[i]
        R, F = self.R, self.F
        rs, rt, rd, sa = (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31
        rel = p.rel.get(i)
        imm = simm(w)
        if rel and rel[0] == "imm":
            imm = rel[1] - 0x10000 if rel[1] & 0x8000 else rel[1]

        def setr(d, v):
            if d:
                R[d] = v & M64
                self.RH[d] = 0

        def ea():
            if rel and rel[0] == "abs":
                return rel[1]
            return (R[rs] + imm) & M32

        # ---------- control transfer
        if op in BRANCHES:
            cond, likely = BRANCHES[op]
            a, b = s64(R[rs]), s64(R[rt])
            if cond == "eq":
                t = a == b
            elif cond == "ne":
                t = a != b
            elif cond == "lez":
                t = a <= 0
            elif cond == "gtz":
                t = a > 0
            elif cond == "ltz":
                t = a < 0
            elif cond == "gez":
                t = a >= 0
            elif cond == "c1t":
                t = self.fcc
            elif cond == "c1f":
                t = not self.fcc
            else:
                raise Unknown(op)
            tgt = i + 1 + imm
            if t and not (0 <= tgt < len(p.words)):
                raise Unknown(f"branch out of function ({op})")
            return ("br", tgt if t else None, likely)
        if op == "j":
            if rel and rel[0] == "jlocal":
                return ("j", rel[1], False)
            if rel and rel[0] == "call":
                return ("call", rel[1], True)
            raise Unknown("j without relocation")
        if op == "jal":
            if rel and rel[0] == "call":
                return ("call", rel[1], False)
            raise Unknown("jal without relocation")
        if op == "jr":
            return ("jr", R[rs] & M32, False)
        if op == "jalr":
            tgt = R[rs] & M32
            if rd != 31:
                raise Unknown("jalr rd != ra")
            return ("call", ("<indirect>", self.canon(tgt, "p")), False)
        if op in ("bltzal", "bgezal", "bltzall", "bgezall"):
            raise Unknown(op)
        if op == "break":
            return ("stop", "break", False)
        if op == "syscall":
            self.calls.append((("syscall", s64(R[3])), (), True))
            setr(2, 0)
            return None
        if op in TRAPS:
            a, b = s64(R[rs]), s64(R[rt]) if op in ("teq", "tne", "tge", "tgeu", "tlt", "tltu") else imm
            c = TRAPS[op]
            if c == "eq" and a == b or c == "ne" and a != b or c == "ge" and a >= b or c == "lt" and a < b \
                    or c == "geu" and (a & M64) >= (b & M64) or c == "ltu" and (a & M64) < (b & M64):
                return ("stop", "trap", False)
            return None

        # ---------- ALU
        if op in ALU3:
            a, b = R[rs], R[rt]
            setr(rd, ALU3[op](a, b))
            return None
        if op in ALUI:
            if op == "lui":
                setr(rt, sx32(((rel[1] if rel and rel[0] == "imm" else w & 0xFFFF)) << 16))
                return None
            if op in ("andi", "ori", "xori"):
                u = (rel[1] if rel and rel[0] == "imm" else w) & 0xFFFF
                setr(rt, ALUI[op](R[rs], u))
                return None
            if rel and rel[0] == "abs":                      # addiu rt, gp, %gp_rel(sym)
                setr(rt, sx32(rel[1]))
                return None
            setr(rt, ALUI[op](R[rs], imm))
            return None
        if op in SHIFTS:
            setr(rd, SHIFTS[op](R[rt], sa, R[rs]))
            return None
        if op in ("movn", "movz"):
            if (R[rt] != 0) == (op == "movn"):
                setr(rd, R[rs])
                self.RH[rd] = self.RH[rs] if rd else 0
            return None
        if op in ("mult", "multu", "mult1", "multu1", "madd", "maddu", "madd1", "maddu1"):
            uns = op.rstrip("1").endswith("u")
            if uns:
                a, b = R[rs] & M32, R[rt] & M32
            else:
                a, b = s32(R[rs]), s32(R[rt])
            pr = a * b
            if op.startswith("madd"):
                one = op.endswith("1")
                cur = ((self.hi1 if one else self.hi) & M32) << 32 | ((self.lo1 if one else self.lo) & M32)
                pr += cur if uns else (cur - (1 << 64) if cur >> 63 else cur)
            lo, hi = sx32(pr), sx32(pr >> 32)
            if op.endswith("1"):
                self.lo1, self.hi1 = lo, hi
            else:
                self.lo, self.hi = lo, hi
            setr(rd, lo)
            return None
        if op in ("div", "divu", "div1", "divu1"):
            if "u" in op:
                a, b = R[rs] & M32, R[rt] & M32
            else:
                a, b = s32(R[rs]), s32(R[rt])
            if b == 0:
                q, r_ = (-1 if a >= 0 else 1), a
            else:
                q = abs(a) // abs(b) * (1 if (a < 0) == (b < 0) else -1)
                r_ = a - q * b
            if op.endswith("1"):
                self.lo1, self.hi1 = sx32(q), sx32(r_)
            else:
                self.lo, self.hi = sx32(q), sx32(r_)
            return None
        if op in ("mfhi", "mflo", "mfhi1", "mflo1"):
            setr(rd, getattr(self, op[2:]))
            return None
        if op in ("mthi", "mtlo", "mthi1", "mtlo1"):
            setattr(self, op[2:], R[rs])
            return None
        if op in ("nop", "sync", "sync.l", "sync.p", "ssnop", "pref", "cache", "ei", "di", "ehb"):
            return None

        # ---------- memory
        if op in LOADS:
            n, sign = LOADS[op]
            a = ea()
            v = self.rd(a, n)
            if sign and v >> (8 * n - 1):
                v -= 1 << (8 * n)
            setr(rt, v)
            return None
        if op in STORES:
            n = STORES[op]
            self.wr(ea(), n, R[rt] & ((1 << (8 * n)) - 1))
            return None
        if op == "lq":
            a = (R[rs] + imm) & M32 & ~15 if not (rel and rel[0] == "abs") else rel[1] & ~15
            lo, hi = self.rd(a, 8), self.rd(a + 8, 8)
            if rt:
                R[rt], self.RH[rt] = lo, hi
            return None
        if op == "sq":
            a = (R[rs] + imm) & M32 & ~15 if not (rel and rel[0] == "abs") else rel[1] & ~15
            self.wr(a, 8, R[rt])
            self.wr(a + 8, 8, self.RH[rt] if rt else 0)
            return None
        if op in ("lwl", "lwr", "ldl", "ldr"):
            a = ea()
            nb = 4 if op[1] == "w" else 8
            k = a & (nb - 1)
            cur = R[rt]
            if op in ("lwl", "ldl"):
                for j in range(k + 1):
                    byte = self.rd8((a - k + j) & M32)
                    pos = nb - 1 - k + j
                    cur = (cur & ~(0xFF << (8 * pos))) | (byte << (8 * pos))
            else:
                for j in range(nb - k):
                    byte = self.rd8((a + j) & M32)
                    cur = (cur & ~(0xFF << (8 * j))) | (byte << (8 * j))
            if nb == 4:
                cur = sx32(cur)
            setr(rt, cur)
            return None
        if op in ("swl", "swr", "sdl", "sdr"):
            a = ea()
            nb = 4 if op[1] == "w" else 8
            k = a & (nb - 1)
            v = R[rt]
            if op in ("swl", "sdl"):
                for j in range(k + 1):
                    pos = nb - 1 - k + j
                    self.wr((a - k + j) & M32, 1, (v >> (8 * pos)) & 0xFF)
            else:
                for j in range(nb - k):
                    self.wr((a + j) & M32, 1, (v >> (8 * j)) & 0xFF)
            return None

        # ---------- COP1
        if op == "lwc1":
            F[rt] = self.rd(ea(), 4)
            return None
        if op == "swc1":
            self.wr(ea(), 4, F[rt], "f")
            return None
        fs, ft, fd = rd, rt, sa
        if op == "mtc1":
            F[fs] = R[rt] & M32
            return None
        if op == "mfc1":
            setr(rt, sx32(F[fs]))
            return None
        if op == "ctc1":
            return None
        if op == "cfc1":
            setr(rt, 0)
            return None
        if op in FOPS:
            a, b = b2f(F[fs]), b2f(F[ft])
            if op == "add.s":
                F[fd] = f2b(a + b)
            elif op == "sub.s":
                F[fd] = f2b(a - b)
            elif op == "mul.s":
                F[fd] = f2b(a * b)
            elif op == "div.s":
                F[fd] = f2b(a / b) if b != 0 else (FLT_MAX_BITS | ((F[fs] ^ F[ft]) & 0x80000000))
            elif op == "abs.s":
                F[fd] = F[fs] & 0x7FFFFFFF
            elif op == "neg.s":
                F[fd] = F[fs] ^ 0x80000000
            elif op == "mov.s":
                F[fd] = F[fs]
            elif op == "sqrt.s":
                F[fd] = f2b(math.sqrt(abs(b)))
            elif op == "rsqrt.s":
                s_ = math.sqrt(abs(b))
                F[fd] = f2b(a / s_) if s_ else FLT_MAX_BITS
            elif op == "max.s":
                F[fd] = F[fs] if a >= b else F[ft]
            elif op == "min.s":
                F[fd] = F[fs] if a <= b else F[ft]
            elif op == "adda.s":
                self.acc = b2f(f2b(a + b))
            elif op == "suba.s":
                self.acc = b2f(f2b(a - b))
            elif op == "mula.s":
                self.acc = b2f(f2b(a * b))
            elif op == "madda.s":
                self.acc = b2f(f2b(self.acc + b2f(f2b(a * b))))
            elif op == "msuba.s":
                self.acc = b2f(f2b(self.acc - b2f(f2b(a * b))))
            elif op == "madd.s":
                F[fd] = f2b(self.acc + b2f(f2b(a * b)))
            elif op == "msub.s":
                F[fd] = f2b(self.acc - b2f(f2b(a * b)))
            elif op == "cvt.s.w":
                F[fd] = f2b(float(s32(F[fs])))
            elif op == "cvt.w.s":
                if a != a:
                    F[fd] = 0x7FFFFFFF
                else:
                    t = int(a)
                    F[fd] = (max(-0x80000000, min(0x7FFFFFFF, t))) & M32
            elif op == "c.f.s":
                self.fcc = False
            elif op == "c.eq.s":
                self.fcc = a == b
            elif op == "c.lt.s":
                self.fcc = a < b
            elif op == "c.le.s":
                self.fcc = a <= b
            return None

        # ---------- MMI (128-bit moves / packs only)
        RH = self.RH
        if op == "por":
            if rd:
                R[rd], RH[rd] = R[rs] | R[rt], RH[rs] | RH[rt]
            return None
        if op == "pnor":
            if rd:
                R[rd], RH[rd] = ~(R[rs] | R[rt]) & M64, ~(RH[rs] | RH[rt]) & M64
            return None
        if op == "pand":
            if rd:
                R[rd], RH[rd] = R[rs] & R[rt], RH[rs] & RH[rt]
            return None
        if op == "pxor":
            if rd:
                R[rd], RH[rd] = R[rs] ^ R[rt], RH[rs] ^ RH[rt]
            return None
        if op == "pcpyld":
            if rd:
                R[rd], RH[rd] = R[rt], R[rs]
            return None
        if op == "pcpyud":
            if rd:
                R[rd], RH[rd] = RH[rs], RH[rt]
            return None
        if op in ("pextlw", "pextuw"):
            if rd:
                a = R[rs] if op == "pextlw" else RH[rs]
                b = R[rt] if op == "pextlw" else RH[rt]
                R[rd] = (b & M32) | (a & M32) << 32
                RH[rd] = (b >> 32) | (a >> 32) << 32
            return None
        raise Unknown(f"unsupported {op}")


def _u(v):
    return v & M64


ALU3 = {
    "addu": lambda a, b: sx32(a + b), "add": lambda a, b: sx32(a + b),
    "subu": lambda a, b: sx32(a - b), "sub": lambda a, b: sx32(a - b),
    "daddu": lambda a, b: _u(a + b), "dadd": lambda a, b: _u(a + b),
    "dsubu": lambda a, b: _u(a - b), "dsub": lambda a, b: _u(a - b),
    "and": lambda a, b: a & b, "or": lambda a, b: a | b, "xor": lambda a, b: a ^ b,
    "nor": lambda a, b: ~(a | b) & M64,
    "slt": lambda a, b: int(s64(a) < s64(b)), "sltu": lambda a, b: int(a < b),
}
ALUI = {
    "addiu": lambda a, k: sx32(a + k), "addi": lambda a, k: sx32(a + k),
    "daddiu": lambda a, k: _u(a + k), "daddi": lambda a, k: _u(a + k),
    "slti": lambda a, k: int(s64(a) < k), "sltiu": lambda a, k: int(a < (k & M64)),
    "andi": lambda a, k: a & k, "ori": lambda a, k: a | k, "xori": lambda a, k: a ^ k,
    "lui": None,
}
SHIFTS = {
    "sll": lambda t, s, v: sx32(t << s), "srl": lambda t, s, v: sx32((t & M32) >> s),
    "sra": lambda t, s, v: sx32(s32(t) >> s),
    "sllv": lambda t, s, v: sx32(t << (v & 31)), "srlv": lambda t, s, v: sx32((t & M32) >> (v & 31)),
    "srav": lambda t, s, v: sx32(s32(t) >> (v & 31)),
    "dsll": lambda t, s, v: _u(t << s), "dsrl": lambda t, s, v: t >> s, "dsra": lambda t, s, v: _u(s64(t) >> s),
    "dsll32": lambda t, s, v: _u(t << (s + 32)), "dsrl32": lambda t, s, v: t >> (s + 32),
    "dsra32": lambda t, s, v: _u(s64(t) >> (s + 32)),
    "dsllv": lambda t, s, v: _u(t << (v & 63)), "dsrlv": lambda t, s, v: t >> (v & 63),
    "dsrav": lambda t, s, v: _u(s64(t) >> (v & 63)),
}
BRANCHES = {
    "beq": ("eq", False), "bne": ("ne", False), "blez": ("lez", False), "bgtz": ("gtz", False),
    "bltz": ("ltz", False), "bgez": ("gez", False), "beql": ("eq", True), "bnel": ("ne", True),
    "blezl": ("lez", True), "bgtzl": ("gtz", True), "bltzl": ("ltz", True), "bgezl": ("gez", True),
    "bc1t": ("c1t", False), "bc1f": ("c1f", False), "bc1tl": ("c1t", True), "bc1fl": ("c1f", True),
}
TRAPS = {"teq": "eq", "tne": "ne", "tge": "ge", "tgeu": "geu", "tlt": "lt", "tltu": "ltu",
         "teqi": "eq", "tnei": "ne", "tgei": "ge", "tgeiu": "geu", "tlti": "lt", "tltiu": "ltu"}
LOADS = {"lb": (1, True), "lbu": (1, False), "lh": (2, True), "lhu": (2, False), "lw": (4, True),
         "lwu": (4, False), "ld": (8, False)}
STORES = {"sb": 1, "sh": 2, "sw": 4, "sd": 8}
FOPS = {"add.s", "sub.s", "mul.s", "div.s", "abs.s", "neg.s", "mov.s", "sqrt.s", "rsqrt.s", "max.s", "min.s",
        "adda.s", "suba.s", "mula.s", "madda.s", "msuba.s", "madd.s", "msub.s", "cvt.s.w", "cvt.w.s",
        "c.f.s", "c.eq.s", "c.lt.s", "c.le.s"}


# ------------------------------------------------------------- compare ----

def _feq(a, b) -> bool:
    if a == b:
        return True
    return abs(a - b) <= FTOL * max(1.0, abs(a), abs(b))


def _veq(a, b) -> bool:
    if a == "?" or b == "?":
        return True
    if isinstance(a, tuple) and isinstance(b, tuple) and a[:1] == b[:1] == ("f",):
        return _feq(a[1], b[1])
    return a == b


def _fmt(v) -> str:
    if isinstance(v, int):
        return hex(v) if abs(v) > 9 else str(v)
    if isinstance(v, tuple):
        if v[0] == "f":
            return f"{v[1]:g}f"
        if v[0] == "str":
            return repr(v[1][:24])
        if v[0] == "sym":
            return f"&{v[1]}+{v[2]:#x}"
    return str(v)


def _call_str(c) -> str:
    callee, args, _ = c
    nm = callee if isinstance(callee, str) else "/".join(map(_fmt, callee))
    return f"{nm}({', '.join(_fmt(a) for a in args)})"


def _wkey(m: Machine, a: int):
    loc = m.p.obj.locate(a)
    return ("sym",) + loc if loc else a


def _wname(m: Machine, k, rsym: dict) -> str:
    if isinstance(k, tuple):
        return f"{k[1]}+{k[2]:#x}"
    i = bisect.bisect_right(rsym[0], k) - 1
    if i >= 0 and k - rsym[0][i] < 0x10000:
        return f"{rsym[1][i]}+{k - rsym[0][i]:#x}"
    return hex(k)


def writes(m: Machine) -> dict:
    """{key: (byte, store start, store size, kind)} outside the frame."""
    out = {}
    for a, b in m.W.items():
        if STACK_LO <= a < STACK_HI:
            continue
        st = m.OWN[a]
        n, v, kind = m.ST[st]
        out[_wkey(m, a)] = (b, st, n, v, kind)
    return out


def _addr_of(m: Machine, k):
    """address of write key K in M's address space (None: symbol unknown there)."""
    if isinstance(k, int):
        return k
    o = m.p.obj
    for s in o.syms:
        if s.name == k[1] and s.shndx in o.base:
            return o.base[s.shndx] + s.value + k[2]
    for idx, lo in o.base.items():
        if o.elf.sections[idx].name == k[1]:
            return lo + k[2]
    return None


def _init_byte(m: Machine, k):
    a = _addr_of(m, k)
    return None if a is None else (m.initword(a >> 2) >> (8 * (a & 3))) & 0xFF


def compare_runs(t: Machine, c: Machine, ret_kind, rsym, prefix: bool = False) -> str | None:
    """Definite difference description, or None. PREFIX: both runs were cut off at
    the step cap, compare only the calls both made."""
    # call log
    for k in range(min(len(t.calls), len(c.calls)) if prefix else max(len(t.calls), len(c.calls))):
        a = t.calls[k] if k < len(t.calls) else None
        b = c.calls[k] if k < len(c.calls) else None
        if a is None or b is None or a[0] != b[0]:
            return (f"call #{k + 1}: target {_call_str(a) if a else 'no call'}, "
                    f"ours {_call_str(b) if b else 'no call'}")
        for j, (x, y) in enumerate(zip(a[1], b[1])):
            if not _veq(x, y):
                if a[2] and "?" in (x, y):          # unknown prototype: a register the
                    continue                        # other side left undefined
                if a[2] and not STRICT_UNPROTO:
                    continue                        # unknown prototype: count / leftovers unknown
                return (f"call #{k + 1} {a[0]} arg {j + 1}: target {_fmt(x)}, ours {_fmt(y)} "
                        f"[{_call_str(a)} vs {_call_str(b)}]")
    if prefix:
        return None
    # return value
    if ret_kind and ret_kind not in ("v", "?"):
        if ret_kind == "f":
            x, y = t.canon(t.F[0], "f"), c.canon(c.F[0], "f")
        else:
            x, y = t.canon(t.R[2], ret_kind), c.canon(c.R[2], ret_kind)
        if not _veq(x, y):
            return f"return value: target {_fmt(x)}, ours {_fmt(y)}"
    # memory
    wt, wc = writes(t), writes(c)
    bad = []
    for k in set(wt) | set(wc):
        x, y = wt.get(k), wc.get(k)
        if x is not None and y is not None and x[0] == y[0]:
            continue
        if x is None and y[0] == _init_byte(t, k) or y is None and x[0] == _init_byte(c, k):
            continue                                 # the other side stored the value already there
        if x is not None and y is not None:
            if x[4] == y[4] == "f" and x[2] == y[2] == 4 and _feq(b2f(x[3]), b2f(y[3])):
                continue
            if x[4] == y[4] == "sp":
                continue
        bad.append((k, x, y))
    if bad:
        bad.sort(key=lambda z: (isinstance(z[0], tuple), z[0] if isinstance(z[0], int) else str(z[0])))
        k, x, y = bad[0]
        def side(z):
            if z is None:
                return "not written"
            n, v, kind = z[2], z[3], z[4]
            return f"{_fmt(b2f(v)) + 'f' if kind == 'f' else hex(v)} ({n}-byte store)"
        return (f"memory: {len(bad)} byte(s) differ, first {_wname(t, k, rsym)}: target {side(x)}, "
                f"ours {side(y)}")
    return None


_RSYM: dict = {}


def _rsym(unit: str, sym_address_table) -> tuple:
    if unit not in _RSYM:
        items = sorted((a, n) for n, a in (sym_address_table(unit) if sym_address_table else {}).items())
        _RSYM[unit] = ([a for a, _ in items], [n for _, n in items])
    return _RSYM[unit]


def check(unit: str, name: str, base_obj: bytes, cand_obj: bytes, sym_address=None, real_proto=None,
          body_text: str = "", seeds: int = DEFAULT_SEEDS, budget: float = 0.6, symbol_table=None) -> dict:
    """{"ok": True/False/None, "detail", "seeds", "cov", "ms", "v"}."""
    t0 = time.time()
    out = {"ok": None, "v": VERSION}
    try:
        bo, co = Obj(base_obj, unit, sym_address), Obj(cand_obj, unit, sym_address)
        real = sym_address(unit, name) if sym_address else None
        bp, cp = Program(bo, name, real), Program(co, name, None)
        imgs = rom_image(unit)
        bimg, cimg = bo.images() + imgs, co.images() + imgs
    except Unknown as e:
        out["detail"] = f"unknown: {e}"
        out["ms"] = int((time.time() - t0) * 1000)
        return out
    except Exception as e:  # noqa: BLE001
        out["detail"] = f"unknown: setup {type(e).__name__}: {e}"[:200]
        out["ms"] = int((time.time() - t0) * 1000)
        return out
    pc: dict = {}

    def protos(f):
        if f not in pc:
            pc[f] = parse_proto(real_proto(f) if real_proto else None, f)
        return pc[f]
    own = parse_proto((real_proto(name) if real_proto else None) or body_proto(body_text, name), name)
    ret_kind = own[0] if own else None
    dic = bp.constants(bimg)
    rsym = _rsym(unit, symbol_table)
    done, cov, why = 0, set(), []
    for seed in range(1, seeds + 1):
        if time.time() - t0 > budget and done:
            break
        tm = Machine(bp, 0, seed, bimg, TARGET_CAP, protos, dic)
        tm.deadline = t0 + budget * 1.5
        try:
            ts = tm.run()
        except Unknown as e:
            why.append(f"target: {e}")
            break
        if ts == "cap":
            # cut off (a loop over a random count): the calls both sides made before
            # the cap must still agree, in order
            why.append("target step cap")
            cm = Machine(cp, 1, seed, cimg, TARGET_CAP + TARGET_CAP // 2, protos, dic)
            cm.deadline = t0 + budget * 1.5
            try:
                cm.run()
            except Unknown:
                continue
            if len(tm.calls) >= 2 and len(cm.calls) >= 2 and not (tm.uninit or cm.uninit) \
                    and not any(a < LOW_ALIAS for a in tm.W) and not any(a < LOW_ALIAS for a in cm.W):
                d = compare_runs(tm, cm, None, rsym, prefix=True)
                if d:
                    out.update(ok=False, detail=f"seed {seed} (cut off at the step cap): {d}"[:400])
                    break
            continue
        cm = Machine(cp, 1, seed, cimg, min(max(4 * tm.steps + 2000, 8000), CAND_CAP), protos, dic)
        cm.deadline = t0 + budget * 1.5
        try:
            cs = cm.run()
        except Unknown as e:
            why.append(f"ours: {e}")
            break
        cov |= tm.cov
        d = None
        low = any(a < LOW_ALIAS for a in tm.W) or any(a < LOW_ALIAS for a in cm.W)
        uninit = tm.uninit or cm.uninit
        if cs == "cap":
            d = compare_runs(tm, cm, None, rsym) if len(cm.calls) >= len(tm.calls) else None
            if not d or d.startswith("memory") or d.startswith("return"):
                d = f"ours still running after {cm.steps} steps; target returned after {tm.steps}"
            else:
                d = d + f" (ours still running after {cm.steps} steps)"
        elif cs != ts:
            d = f"target ends with {ts}, ours with {cs}"
        else:
            d = compare_runs(tm, cm, ret_kind, rsym)
        done += 1
        if d and (low or uninit):
            # a write through a NULL / small-int pointer (random pointers may alias
            # there) or a read of a frame slot nobody wrote (uninitialised local, or
            # an out-parameter bigger than the stub fills): not conclusive
            why.append(f"seed {seed} inconclusive ({'writes near address 0' if low else 'uninitialised frame read'})")
            done -= 1
            continue
        if d:
            out.update(ok=False, detail=f"seed {seed}: {d}"[:400])
            break
    else:
        pass
    if out["ok"] is None and done:
        out["ok"] = True
        out["detail"] = f"{done} seed(s) identical" + (f" ({why[0]})" if why else "")
    elif out["ok"] is None:
        out["detail"] = "unknown: " + (why[0] if why else "no seed ran")
    out["seeds"] = done
    nbr = sum(1 for op in bp.ops if op in BRANCHES)
    out["cov"] = f"{len(cov)}/{2 * nbr}" if nbr else "-"
    out["ms"] = int((time.time() - t0) * 1000)
    return out


def main() -> int:
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("unit")
    ap.add_argument("name")
    ap.add_argument("base", type=Path)
    ap.add_argument("cand", type=Path)
    ap.add_argument("--body", type=Path)
    ap.add_argument("--seeds", type=int, default=DEFAULT_SEEDS)
    a = ap.parse_args()
    sys.path.insert(0, str(Path(__file__).parent))
    import try_fn
    r = check(a.unit, a.name, a.base.read_bytes(), a.cand.read_bytes(), try_fn.sym_address, try_fn.real_proto,
              a.body.read_text(errors="replace") if a.body else "", seeds=a.seeds, budget=60,
              symbol_table=try_fn.symbol_addresses)
    print(json.dumps(r))
    return 0


if __name__ == "__main__":
    sys.exit(main())

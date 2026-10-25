#!/usr/bin/env python3
"""Semantics gate for saved best bodies (2026-10-19).

A near-miss body can score higher than a faithful one while computing
something else (drawVector4: a call argument 0 where the target passes
0x70000400; menuStockObjDraw: -0x7F where the target has -0x80; monsGainSet:
a byte load moved from the jal delay slot to after the call). Every later
worker / permuter run then polishes the wrong logic.

check() compares facts that register allocation and instruction scheduling
never change, extracted from the objdump rows of the target (base) and the
candidate (try_fn.disasm output, "off: insn  [RELOC sym]"), with constant
propagation over the CFG (label = meet of its incoming edges, branch-likely
delay slots annulled on the fall-through path). BLOCKING (report "diffs"):

  constant     integer values one side materializes and the other never has
               (li, lui+ori/addiu combined, raw lui operands, andi/ori/xori
               immediates, absolute scratchpad addresses). Tolerated: 0/1/-1,
               0xff/0xffff masks, values folded from other constants or used
               as pointer offsets ("weak"), a symbol address vs the same
               plain immediate, x<K vs x<=K-1, lui-sized OR/sum splits.
  float        float literals (.lit4 words, TU .rodata words, lui+mtc1) on one
               side only (negated values tolerated: -x < .5 == -.5 < x)
  value addend `addiu rd,rs,K` whose result is used as data (stored, compared,
               multiplied, masked) with a K the other side never has
               (menuStockObjDraw -0x7F vs -0x80); tolerated: sums of two
               addends, K = K' mod 256, &sym+K
  conversion   cvt.* kinds one side only (int vs float local)
  calls        a callee one side never calls (counts only noted: cross-jumping)
  call arg     a0-a3 holding a known constant / global address right before a
               call (set in the call's block, not read before it) with values
               the other side never passes to that callee (drawVector4), or a
               target arg the candidate leaves holding the previous call's
               leftovers (EtherTreeMain)
  load order   a load (non-frame, non-constant) the target does right before
               call X (same block, the jal delay slot included) and ours only
               right after X, or the reverse (monsGainSet)
  operand order subu/sub.s/div with the operand origins swapped

NOTES only (too noisy on faithful bodies): memory offset sets, data symbols,
shift amounts, address addends, compare-only constants, call counts/order.
Measured 2026-10-19 (build/auto_decomp/loopstate/semgate/NOTES.txt): 0 of 94
audited-faithful bodies flagged; 3/3 confirmed wrong bodies flagged.
"""
from __future__ import annotations

import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VERSION = 1             # .best.json "sem_v": bump when check() changes what it flags
_RO: dict = {}

LOADS = {"lb", "lbu", "lh", "lhu", "lw", "lwu", "ld", "lq", "lwc1", "ldc1", "lwl", "lwr", "ldl", "ldr", "lqc2"}
STORES = {"sb", "sh", "sw", "sd", "sq", "swc1", "sdc1", "swl", "swr", "sdl", "sdr", "sqc2"}
# unaligned / partial pairs are folded onto the aligned access they implement
_PAIR = {"lwl": ("lw", -3), "lwr": ("lw", 0), "ldl": ("ld", -7), "ldr": ("ld", 0),
         "swl": ("sw", -3), "swr": ("sw", 0), "sdl": ("sd", -7), "sdr": ("sd", 0)}
FRAME = {"sp", "s8", "fp"}
NODEST = re.compile(r"^(sb|sh|sw|sd|sq|swc1|sdc1|swl|swr|sdl|sdr|sqc2|beq|bne|beql|bnel|beqz|bnez|beqzl|bnezl|"
                    r"bgez|bgtz|blez|bltz|bgezl|bgtzl|blezl|bltzl|bgezal|bltzal|b|j|jr|jal|jalr|bal|bc1t|bc1f|"
                    r"bc1tl|bc1fl|mtc1|ctc1|dmtc1|mthi|mtlo|mthi1|mtlo1|teq|tne|tge|tlt|break|syscall|sync|nop|"
                    r"c\..*|mult|multu|div|divu|mult1|multu1|div1|divu1|madd|maddu|madd1|maddu1|cache|"
                    r"qmtc2|ctc2|vcallms|vcallmsr|lwc1|swc1|lqc2|sqc2|mtsab|mtsah)$")
BRANCH = re.compile(r"^(b|beq|bne|beql|bnel|beqz|bnez|beqzl|bnezl|bgez|bgtz|blez|bltz|bgezl|bgtzl|blezl|bltzl|"
                    r"bc1t|bc1f|bc1tl|bc1fl|bgezal|bltzal)$")
SHIFT = {"sll", "srl", "sra", "dsll", "dsrl", "dsra", "dsll32", "dsrl32", "dsra32"}
CLOBBER = {"v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9", "at",
           "ra"}
ARGS = ("a0", "a1", "a2", "a3")
# masks / shift amounts that only implement C integer conversions
EXT_MASKS = {0xFF, 0xFFFF, 0x7FFF, 0x7F}
EXT_SHIFTS = {16, 24, 32, 48, 56}
SCRATCH = (0x70000000, 0x70004000)
FOLD = {"addu": lambda x, y: x + y, "daddu": lambda x, y: x + y, "subu": lambda x, y: x - y,
        "and": lambda x, y: x & y, "or": lambda x, y: x | y, "xor": lambda x, y: x ^ y,
        "nor": lambda x, y: ~(x | y), "sllv": lambda x, y: x << (y & 31), "srlv": lambda x, y: x >> (y & 31)}


def s32(v: int) -> int:
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v & 0x80000000 else v


def parse(row: str):
    """(off, mnemonic, operands, (reltype, sym) | None)."""
    off, _, rest = row.partition(":")
    rel = None
    m = re.search(r"\[(\S+) ([^\]]+)\]", rest)
    if m:
        rel = (m.group(1), m.group(2))
        rest = rest[:m.start()]
    p = rest.strip().split(None, 1)
    mn = p[0] if p else ""
    ops = [o.strip() for o in p[1].split(",")] if len(p) > 1 else []
    try:
        o = int(off.strip(), 16)
    except ValueError:
        o = None
    return o, mn, ops, rel


def _int(x: str):
    try:
        return int(x, 0)
    except (ValueError, TypeError):
        return None


class Ctx:
    """Unit-level lookups: symbol addresses, literal / rodata words."""

    def __init__(self, unit: str, tu: str, sym_address=None, lit_symbols=None, proto=None):
        self.unit, self.tu = unit, tu
        self._sa, self._ls, self._pr = sym_address, lit_symbols, proto
        self._ro = None
        self._na: dict = {}

    def nargs(self, f: str):
        """GPR argument count of F from its known prototype (None: unknown / variadic)."""
        if f not in self._na:
            self._na[f] = self._nargs(f)
        return self._na[f]

    def _nargs(self, f: str):
        pr = self._pr(f) if self._pr else None
        m = re.match(r"^\s*(?:extern\s+)?.*?\b" + re.escape(f) + r"\s*\((.*)\)\s*;?\s*$", pr or "")
        if not m:
            return None
        a = m.group(1).strip()
        if a in ("", "void"):
            return 0
        if "..." in a or "(" in a:
            return None
        return sum(1 for x in a.split(",") if "*" in x or not re.search(r"\b(float|double)\b", x))

    def addr(self, name: str):
        if self._sa:
            return self._sa(self.unit, name)
        m = re.fullmatch(r"D_([0-9A-Fa-f]{1,8})", name)
        return int(m.group(1), 16) if m else None

    def lit(self, name: str):
        """bits of a .lit4/.lit8 data symbol or a word of the TU's .rodata, else None."""
        if self._ls:
            d = self._ls(self.unit).get(name)
            if d:
                return ("d" if d["kind"] == "double" else "f", d["bits"])
        if self._ro is None:
            self._ro = _RO.setdefault((self.unit, self.tu), {})
            for f in [] if self._ro else (ROOT / "build" / self.unit / "asm").rglob(f"{self.tu}.rodata.s"):
                cur = None
                for ln in f.read_text(errors="replace").splitlines():
                    m = re.match(r"\s*dlabel\s+(\w+)", ln)
                    if m:
                        cur = m.group(1)
                        continue
                    m = re.search(r"/\*\s*\S+\s+\S+\s+([0-9A-Fa-f]{8})\s*\*/\s*\.(float|word)", ln)
                    if m and cur:
                        self._ro[cur] = ("f", int.from_bytes(bytes.fromhex(m.group(1)), "little"))
                        cur = None
        return self._ro.get(name)


_REG = re.compile(r"[a-z][a-z0-9]?|s8|zero")
_MEMOP = re.compile(r"-?\w+\((\w+)\)")


def regs_rw(mn: str, ops: list[str]) -> tuple[set, str | None]:
    """(registers read, register written) of one insn (GPRs only)."""
    rd = set()
    for o in ops:
        m = _MEMOP.fullmatch(o)
        if m:
            rd.add(m.group(1))
    plain = [o for o in ops if _REG.fullmatch(o)]
    if NODEST.match(mn) or mn in LOADS and mn in ("lwc1", "lqc2"):
        rd |= set(plain)
        if mn in ("mtc1", "dmtc1", "ctc1", "qmtc2", "ctc2"):
            rd = {ops[0]} if ops else set()
        return rd - {"zero"}, None
    if mn in ("mfc1", "cfc1", "dmfc1", "qmfc2", "cfc2"):
        return set(), ops[0] if ops else None
    w = ops[0] if ops and plain and ops[0] == plain[0] else None
    rd |= set(plain[1:] if w else plain)
    return rd - {"zero"}, w


def meet(states: list) -> dict:
    if not states:
        return {}
    out = dict(states[0])
    for s in states[1:]:
        out = {r: v for r, v in out.items() if s.get(r) == v}
    return out


def facts(rows: list[str], start: int, names: dict, data: dict, ctx: Ctx) -> dict:
    """NAMES: section offset -> function name (same-TU `jal` targets); DATA:
    section name -> bytes of this object (.lit4/.lit8 literals)."""
    P = [parse(r) for r in rows]
    RW = [regs_rw(mn, ops) for _o, mn, ops, _r in P]
    targets = set()
    for off, mn, ops, rel in P:
        if (BRANCH.match(mn) or (mn == "j" and rel and rel[1].startswith("."))) and ops:
            t = _int(ops[-1] if ops[-1].startswith("0x") else "0x" + ops[-1])
            if t is not None:
                targets.add(t - start)
    # forward use classification of `addiu rd,rs,K`: address (memory base / pointer
    # add) or value
    def value_use(i, rd):
        """True when the first read of RD after row I is a plain-value use (stored
        as data, compared, multiplied, masked, moved to the FPU): RD is a value,
        not a pointer (address addends fold into memory offsets / move around)."""
        for j in range(i + 1, min(len(P), i + 40)):
            mn, ops = P[j][1], P[j][2]
            rr, w = RW[j]
            if rd in rr:
                if mn in STORES:
                    return bool(ops) and ops[0] == rd and not re.search(r"\(" + rd + r"\)", ops[-1])
                return mn in ("slt", "sltu", "slti", "sltiu", "mult", "multu", "div", "divu", "and", "or",
                              "xor", "nor", "andi", "ori", "xori", "sll", "srl", "sra", "sllv", "srlv", "srav",
                              "mtc1", "movn", "movz")  # (not beq/bne: loop-end pointer compares)
            if w == rd:
                return False
        return False
    def run(edges: dict) -> dict:
        def addr_use(i, rd):
            """True when the first read of RD after row I is a pointer add or a memory base."""
            for j in range(i + 1, min(len(P), i + 40)):
                mn, ops = P[j][1], P[j][2]
                rr, w = RW[j]
                if rd in rr:
                    return mn in ("addu", "daddu") or any(o.endswith("(" + rd + ")") for o in ops)
                if w == rd:
                    return False
            return False

        F = {"consts": set(), "floats": set(), "float_unk": 0, "shifts": set(), "calls": [], "args": [],
             "loads": Counter(), "stores": Counter(), "globals": set(), "sect_refs": 0, "addends": set(), "vaddends": set(), "gaddrs": set(), "cvts": Counter(), "lo16s": set(), "folded": set(), "weak": set(), "cmpk": set(), "argdefs": [],
             "anchors": {}, "n": len(rows)}
        st: dict = {}            # reg -> ("k", value) | ("hi", sym, hi_addend)
        unread: dict = {}        # reg -> True while its last definition has not been read
        ncall = Counter()
        pending = None           # (callee, occurrence) whose delay slot is the current row
        last_call = None         # call occurrence ending the current basic block's prefix
        blk_loads: list = []     # load keys since the last label / branch / call (anchor "before")
        likely_dest = None
        garbage: set = set()     # caller-saved regs clobbered by the last call, not written since

        def kconst(v, weak=False):
            """a materialized constant; WEAK = derived (folded / used as a pointer
            offset): it explains the other side's values but is not required there"""
            v = s32(v)
            if v not in (0, 1, -1) and (v & 0xFFFFFFFF) not in EXT_MASKS:
                F["weak" if weak else "consts"].add(v)

        def anchor(k, kind, call):
            F["anchors"].setdefault(k, set()).add((kind, call))

        def mem_key(mn, ops, rel):
            m = re.fullmatch(r"(-?\w+)\((\w+)\)", ops[1]) if len(ops) == 2 else None
            if not m:
                return None
            imm, base = _int(m.group(1)), m.group(2)
            if imm is None:
                return None
            op, adj = _PAIR.get(mn, (mn, 0))
            if rel:
                s = rel[1]
                if s.startswith("."):
                    return (op, "sect", s)
                a = ctx.addr(s)
                hi = st.get(base)
                hadd = hi[2] if hi and hi[0] == "hi" and hi[1] == s else 0
                if a is None:
                    return (op, "sym", s, hadd + imm + adj)
                return (op, "abs", s32(a + hadd + imm + adj))
            if base in FRAME:
                return None
            b = st.get(base)
            if b and b[0] == "k":
                return (op, "abs", s32(b[1] + imm + adj))
            if base == "gp":
                return (op, "gp", imm + adj)
            return (op, "off", imm + adj)

        org: dict = {r: ("arg", r) for r in ARGS + ("$f12", "$f14")}   # symbolic origin of a register
        F["subs"] = Counter()
        for i, (off, mn, ops, rel) in enumerate(P):
            fall = not (i >= 2 and (P[i - 2][1] in ("b", "jr") or P[i - 2][1] == "j"))
            if (off is not None and off in targets) or not fall:
                ins = list(edges.get(off, {}).values()) + ([st] if fall else [])
                st = meet(ins)
                unread, last_call, blk_loads, org = {}, None, [], {}
                garbage = set()
            rr, w = RW[i]
            if mn not in ("ori", "addiu", "daddiu", "mtc1", "move"):
                for r in rr:
                    x = st.get(r)
                    if x and x[2:] == ("lui",) and not any(o.endswith("(" + r + ")") for o in ops):
                        kconst(x[1])           # a lui value used as is (or / and / compare / store)
            if mn.startswith(("cvt.", "trunc.", "round.", "floor.", "ceil.")):
                if not (len(ops) == 2 and st.get(ops[1], ("?",))[0] == "k"):   # (float)0 etc. is a constant
                    F["cvts"][mn] += 1
            # ---- operand order of non-commutative ops (swapped subtraction / division)
            if mn in ("subu", "dsubu", "sub.s", "div.s", "div", "divu") and len(ops) == 3:
                a_, b_ = (ops[1], ops[2])
                if "zero" not in (a_, b_) and a_ != b_:
                    oa, ob = org.get(a_), org.get(b_)
                    if oa and ob and oa != ob:
                        F["subs"][(mn, oa, ob)] += 1
            wf = ops[0] if ops and ops[0].startswith("$f") and not NODEST.match(mn) else None
            if mn in ("mtc1", "lwc1") and len(ops) == 2:
                wf = ops[1] if mn == "mtc1" else ops[0]
            nw = w or wf
            if nw:
                o = None
                if mn in LOADS:
                    kk = mem_key(mn, ops, rel)
                    o = ("ld", kk) if kk else None
                    if rel and ctx.lit(rel[1]) is not None:
                        o = ("lit", ctx.lit(rel[1]))
                    elif rel and rel[1] in (".lit4", ".lit8"):
                        o = None
                elif mn in ("move", "mov.s") and len(ops) == 2:
                    o = org.get(ops[1]) if ops[1] != "zero" else ("k", 0)
                elif mn == "li" and len(ops) == 2:
                    o = ("k", _int(ops[1]))
                org.pop(nw, None)
                if o:
                    org[nw] = o
            for r in rr:
                unread.pop(r, None)
            if rel and rel[0] in ("HI16", "LO16", "GPREL16", "LITERAL"):
                s = rel[1]
                if s.startswith("."):
                    if s not in (".lit4", ".lit8"):
                        F["sect_refs"] += 1
                elif not s.startswith(("$", "jtbl_")):
                    a = ctx.addr(s)
                    if a is not None:
                        F["lo16s"].add(s32(((a & 0xFFFF) ^ 0x8000) - 0x8000))
                    lit = ctx.lit(s) if mn in ("lwc1", "lw", "ld") else None
                    if lit is None and not (a is not None and SCRATCH[0] <= a < SCRATCH[1]):
                        F["globals"].add(a if a is not None else s)
            is_call = mn in ("jal", "jalr") or (mn == "j" and rel and rel[0] == "26" and not rel[1].startswith("."))
            if is_call:
                if mn == "jalr":
                    callee = "<indirect>"
                elif rel and rel[1].startswith("."):
                    t = _int("0x" + ops[0]) if ops else None
                    callee = names.get(t, "<local>") if t is not None else "<local>"
                else:
                    callee = rel[1] if rel else "?"
                F["calls"].append(callee)
                pending = (callee, ncall[callee])
                ncall[callee] += 1
                continue
            # ---- memory
            if mn in LOADS or mn in STORES:
                k = mem_key(mn, ops, rel)
                if mn in LOADS:
                    if k and k[1] not in ("sect",):
                        F["loads"][k] += 1
                        if rel and ctx.lit(rel[1]) is not None or rel and rel[1].startswith((".lit", ".rodata")):
                            pass                       # a constant: free to move across calls
                        else:
                            blk_loads.append(k)
                            if last_call is not None:
                                anchor(k, "after", last_call)
                    if mn in ("lwc1", "lw", "ld") and rel:
                        s = rel[1]
                        if s in (".lit4", ".lit8"):
                            m = re.fullmatch(r"(-?\w+)\(gp\)", ops[1]) if len(ops) == 2 else None
                            imm = _int(m.group(1)) if m else None
                            blob = data.get(s)
                            wd = 8 if s == ".lit8" or mn == "ld" else 4
                            if imm is not None and blob and 0 <= imm and imm + wd <= len(blob):
                                F["floats"].add(("d" if wd == 8 else "f", int.from_bytes(blob[imm:imm + wd], "little")))
                            else:
                                F["float_unk"] += 1
                        elif not s.startswith(".") and rel[0] in ("GPREL16", "LO16"):
                            lv = ctx.lit(s)
                            if lv is not None and (mn == "lwc1" or lv[0] == "d"):
                                F["floats"].add(lv)
                        elif s.startswith(".") and mn == "lwc1":
                            F["float_unk"] += 1
                    if w:
                        st.pop(w, None)
                elif k and k[1] not in ("sect",):
                    F["stores"][k] += 1
            # ---- constants / register state
            elif mn == "lui" and ops:
                if rel and rel[0] == "HI16":
                    st[ops[0]] = ("hi", rel[1], (_int(ops[1]) or 0) << 16)
                else:
                    st[ops[0]] = ("k", s32((_int(ops[1]) or 0) << 16), "lui")
            elif mn == "li" and len(ops) == 2:
                v = _int(ops[1])
                if v is not None:
                    kconst(v, weak=addr_use(i, ops[0]))
                    st[ops[0]] = ("k", s32(v))
                else:
                    st.pop(ops[0], None)
            elif mn == "move" and len(ops) == 2:
                if ops[1] == "zero":
                    st[ops[0]] = ("k", 0)
                elif ops[1] in st:
                    st[ops[0]] = st[ops[1]]
                else:
                    st.pop(ops[0], None)
            elif mn in ("addiu", "daddiu", "ori", "andi", "xori", "slti", "sltiu") and len(ops) == 3:
                v = _int(ops[2])
                src = st.get(ops[1])
                if rel:
                    st.pop(ops[0], None)
                    if rel[0] == "LO16" and not rel[1].startswith(".") and mn in ("addiu", "daddiu"):
                        # %lo(S) pairs with a %hi(S) (maybe on another path): S + addend
                        a = ctx.addr(rel[1])
                        hadd = src[2] if src and src[0] == "hi" and src[1] == rel[1] else 0
                        if a is not None and SCRATCH[0] <= a < SCRATCH[1]:
                            val = s32(a + hadd + (v or 0))
                            kconst(val)
                            st[ops[0]] = ("k", val)
                        elif a is not None:
                            st[ops[0]] = ("g", s32(a + hadd + (v or 0)))
                            F["gaddrs"].add(s32(a + hadd + (v or 0)))
                elif v is None:
                    st.pop(ops[0], None)
                elif ops[1] == "zero" and mn in ("addiu", "daddiu", "ori"):
                    kconst(v, weak=addr_use(i, ops[0]))
                    st[ops[0]] = ("k", s32(v))
                elif src and src[0] == "k" and mn in ("addiu", "daddiu", "ori", "andi", "xori"):
                    val = s32(FOLD[{"addiu": "addu", "daddiu": "addu"}.get(mn, mn[:-1])](src[1] & 0xFFFFFFFF, v & 0xFFFF
                                                                                   if mn != "addiu" and mn != "daddiu"
                                                                                   else v & 0xFFFFFFFF))
                    F["folded"].add(s32(src[1]))
                    kconst(val, weak=src[2:] != ("lui",) or mn in ("andi", "xori"))
                    st[ops[0]] = ("k", val)
                else:
                    if ops[1] not in FRAME and ops[1] != "gp":
                        if mn in ("addiu", "daddiu"):
                            if value_use(i, ops[0]):
                                F["vaddends"].add(s32(v))
                            else:
                                F["addends"].add(s32(v))
                        elif mn in ("slti", "sltiu"):
                            if abs(v) > 1:
                                F["cmpk"].add(s32(v))
                        elif not (mn == "andi" and v in EXT_MASKS):
                            kconst(v)
                    st.pop(ops[0], None)
            elif mn in SHIFT and len(ops) == 3:
                v = _int(ops[2])
                if v is not None:
                    if mn.endswith("32"):
                        v += 32
                    if v not in EXT_SHIFTS:
                        F["shifts"].add((mn.replace("32", "").lstrip("d"), v))
                st.pop(ops[0], None)
            elif mn == "mtc1" and len(ops) == 2:
                st.pop(ops[1], None)
                if ops[0] == "zero":
                    F["floats"].add(("f", 0))
                    st[ops[1]] = ("k", 0)
                else:
                    src = st.get(ops[0])
                    if src and src[0] == "k":
                        F["floats"].add(("f", src[1] & 0xFFFFFFFF))
                        F["consts"].discard(src[1])
                        st[ops[1]] = ("k", src[1])
            elif mn in FOLD and len(ops) == 3 and w and all(
                    o == "zero" or st.get(o, ("?",))[0] == "k" for o in ops[1:]):
                # both inputs known: gcc left a constant expression unfolded (an
                # unsymbolized address rounded up, ...): compare the result, not the parts
                x, y = [0 if o == "zero" else st[o][1] for o in ops[1:]]
                val = FOLD[mn](x & 0xFFFFFFFF, y & 0xFFFFFFFF)
                F["folded"].update({s32(x), s32(y)})
                kconst(val, weak=True)
                st[w] = ("k", s32(val))
            elif w:
                st.pop(w, None)
            else:
                wf_ = ops[0] if ops and ops[0].startswith("$f") and not NODEST.match(mn) else None
                if wf_:
                    st.pop(wf_, None)
            prev_jump = i > 0 and (BRANCH.match(P[i - 1][1]) or P[i - 1][1] == "jr" or P[i - 1][1] == "j" and not (
                P[i - 1][3] and P[i - 1][3][0] == "26" and not P[i - 1][3][1].startswith(".")))
            if w:
                garbage.discard(w)
            if w and not mn.startswith(("b", "j")) and not prev_jump:
                unread[w] = True
            if BRANCH.match(mn):
                unread = {}
            # the delay slot ran: record the state on the branch edge
            if i > 0 and (BRANCH.match(P[i - 1][1]) or P[i - 1][1] == "j" and P[i - 1][2] and not (
                    P[i - 1][3] and P[i - 1][3][0] == "26" and not P[i - 1][3][1].startswith("."))):
                t = _int(P[i - 1][2][-1] if P[i - 1][2][-1].startswith("0x") else "0x" + P[i - 1][2][-1])
                if t is not None:
                    edges.setdefault(t - start, {})[i] = dict(st)
            # branch-likely: the delay slot is annulled on the fall-through path
            if likely_dest is not None:
                for r in likely_dest:
                    st.pop(r, None)
                likely_dest = None
            if BRANCH.match(mn) and mn.endswith("l") and mn not in ("bal",) and i + 1 < len(P):
                _, wl = RW[i + 1]
                likely_dest = {wl} if wl else set()
            if BRANCH.match(mn) or mn in ("j", "jr"):
                last_call = None
            # ---- after a call's delay slot: record args / anchors, clobber
            if pending is not None:
                na = ctx.nargs(pending[0]) if pending[0][0] != "<" else None
                F["args"].append((pending[0], {r: st[r][:2] for n_, r in enumerate(ARGS)
                                               if r in st and st[r][0] in ("k", "g") and unread.get(r)
                                               and (na is None or n_ < na)}))
                # (no prototype filter: protos.json can be wrong, the target's own
                # per-call setup is the evidence)
                F["argdefs"].append((pending[0], {r for r in ARGS if unread.get(r)},
                                     {r for r in ARGS if r in garbage}))
                for k in blk_loads:
                    anchor(k, "before", pending)
                st = {r: v for r, v in st.items() if r not in CLOBBER}
                garbage = set(CLOBBER) - {"ra"}
                org = {r: v for r, v in org.items() if r not in CLOBBER and not r.startswith("$f")}
                org["v0"] = org["$f0"] = ("ret",) + pending
                unread = {}
                last_call, blk_loads, pending = pending, [], None
            elif BRANCH.match(mn) or mn in ("j", "jr"):
                blk_loads = []
        return F

    # constant propagation over the CFG: a label starts with the meet of the
    # states on its incoming edges (iterated: back edges arrive one pass later)
    edges: dict = {}
    prev = None
    for _ in range(5):
        F = run(edges)
        sig = {t: sorted((s, sorted(v.items(), key=str)) for s, v in d.items()) for t, d in edges.items()}
        if sig == prev:
            break
        prev = sig
    return F


def _mk(k):
    if k[1] == "abs":
        return f"{k[0]} @{k[2] & 0xFFFFFFFF:#x}"
    return f"{k[0]} {k[1]}{'+' + str(k[2]) if len(k) > 2 else ''}"


def _org(o):
    if o[0] == "ld":
        return "load " + _mk(o[1])
    if o[0] == "ret":
        return f"result of {o[1]} #{o[2] + 1}"
    if o[0] == "arg":
        return f"param {o[1]}"
    if o[0] == "lit":
        return "const " + _fl(o[1])
    return f"{o[0]} {_fmt(o[1]) if len(o) > 1 else ''}"


def _fmt(v):
    return hex(v) if isinstance(v, int) and abs(v) > 9 else str(v)


def _fl(x):
    import struct
    k, b = x
    try:
        return repr(struct.unpack("<f", b.to_bytes(4, "little"))[0]) if k == "f" else \
            repr(struct.unpack("<d", b.to_bytes(8, "little"))[0])
    except (OverflowError, ValueError):
        return hex(b)


def compare(T: dict, C: dict) -> tuple[list[str], list[str]]:
    """(blocking diffs, notes)."""
    block, notes = [], []
    # constants
    # (an absolute address may be a plain immediate on one side, a symbol on the other;
    # a scratchpad pointer may be a materialized constant on one side and a base +
    # offset on the other)
    def explained(c, S, O):
        ov = O["consts"] | O["weak"]
        if c in ov or c in O["gaddrs"] or c in O["cmpk"]:
            return True
        if c - 1 in O["cmpk"] or c + 1 in O["cmpk"]:
            return True                    # x < 129 vs x <= 128 (slti K vs li K-1; slt)
        # one side ORs / adds two constants the other has as one (0x100008 | 0x400000);
        # only for lui-sized values (small ones would explain anything)
        big = lambda x: abs(x) >= 0x10000  # noqa: E731
        if big(c) and any((x | y) == c or s32(x + y) == c for x in ov for y in ov if x != y and big(x | y)):
            return True
        sv = S["consts"] | S["weak"]
        if big(c) and any((c | y) in ov or s32(c + y) in ov for y in sv if y != c):
            return True
        if SCRATCH[0] <= (c & 0xFFFFFFFF) < SCRATCH[1]:
            r = (c & 0xFFFFFFFF) - SCRATCH[0]
            offs = {k[2] for k in list(O["loads"]) + list(O["stores"]) if k[1] in ("off", "abs")}
            return r in O["addends"] | O["vaddends"] | O["consts"] or r in offs or c in offs
        return False
    a = sorted(c for c in T["consts"] if not explained(c, T, C) and c not in T["folded"])
    b = sorted(c for c in C["consts"] if not explained(c, C, T) and c not in C["folded"])
    ca = sorted(c for c in T["cmpk"] - C["cmpk"] if c not in C["consts"] | C["weak"])
    cb = sorted(c for c in C["cmpk"] - T["cmpk"] if c not in T["consts"] | T["weak"])
    if ca or cb:                           # switch-tree pivots / < vs <= forms: a hint only
        notes.append(f"compare constants: target-only {ca[:4]}, ours-only {cb[:4]}")
    if a or b:
        block.append("constant: target-only " + str([_fmt(x) for x in a[:6]]) +
                     ", ours-only " + str([_fmt(x) for x in b[:6]]))
    # floats
    neg = lambda fs: {(k, b ^ (0x80000000 if k == "f" else 1 << 63)) for k, b in fs}  # noqa: E731
    fa = sorted(T["floats"] - C["floats"] - neg(C["floats"]))      # -x < 0.5 == -0.5 < x
    fb = sorted(C["floats"] - T["floats"] - neg(T["floats"]))
    if fb and not T["float_unk"]:
        block.append("float: ours-only constants " + str([_fl(x) for x in fb[:4]]) + " (not in the target)")
    if fa and not C["float_unk"]:
        block.append("float: target constants " + str([_fl(x) for x in fa[:4]]) + " missing from ours")
    # int <-> float conversions (an int local where the target keeps a float, or the reverse)
    a, b = sorted(set(T["cvts"]) - set(C["cvts"])), sorted(set(C["cvts"]) - set(T["cvts"]))
    if a or b:
        block.append(f"conversion: target-only {a}, ours-only {b} (int vs float type of a value?)")
    # calls
    # (only a callee one side never calls blocks: gcc cross-jumping merges identical
    # call tails, so per-callee counts can differ in a faithful body)
    tc, cc = Counter(T["calls"]), Counter(C["calls"])
    if set(tc) != set(cc):
        d = {k: (tc[k], cc[k]) for k in set(tc) ^ set(cc)}
        block.append("calls: " + ", ".join(f"{k} target {x}x ours {y}x" for k, (x, y) in sorted(d.items())[:4]))
    elif tc != cc:
        d = {k: (tc[k], cc[k]) for k in set(tc) | set(cc) if tc[k] != cc[k]}
        notes.append("call counts: " + ", ".join(f"{k} target {x}x ours {y}x" for k, (x, y) in sorted(d.items())[:4]))
    elif T["calls"] != C["calls"]:
        notes.append("call order differs")
    # call args: per callee and argument register, the known constant / address
    # values (sets: the n-th linear call is layout dependent). Flag a value one side
    # passes that the other never does, when the other passes a different known one.
    def argsets(side):
        d: dict = {}
        for f, args in side["args"]:
            for r, v in args.items():
                d.setdefault((f, r), set()).add(v)
        return d
    sh = lambda x: _fmt(x[1]) if x[0] == "k" else f"&{x[1] & 0xFFFFFFFF:#x}"  # noqa: E731
    ta, ca = argsets(T), argsets(C)
    for k in sorted(set(ta) & set(ca), key=str):
        a, b = ta[k] - ca[k], ca[k] - ta[k]
        if a and b:
            block.append(f"call arg: {k[0]} {k[1]}: target passes {sorted(map(sh, a))[:3]}, ours "
                         f"{sorted(map(sh, b))[:3]}")
            break
    # a missing argument: every target call to f sets aK right before it, ours
    # calls f with aK still holding garbage from the previous call (EtherTreeMain:
    # a delay-slot `move a0,zero` dropped)
    if not any("call arg" in x for x in block):
        tdef: dict = {}
        for f, d, _g in T["argdefs"]:
            tdef.setdefault(f, []).append(d)
        for f, _d, g in C["argdefs"]:
            ds = tdef.get(f)
            miss = sorted(r for r in g if ds and all(r in d for d in ds))
            if miss:
                block.append(f"call arg: ours calls {f} without setting {miss[0]} (it still holds the previous "
                             f"call's leftovers); the target sets it before every call")
                break
    # memory
    for kind, key in (("load", "loads"), ("store", "stores")):
        a = sorted(set(T[key]) - set(C[key]), key=str)
        b = sorted(set(C[key]) - set(T[key]), key=str)
        a = [x for x in a if x[1] not in ("sect", "sym")]
        b = [x for x in b if x[1] not in ("sect", "sym")]
        if a or b:
            notes.append(f"{kind}s: target-only {a[:4]}, ours-only {b[:4]}")
    # globals
    td, cd = T["globals"], C["globals"]
    if td != cd:
        a = sorted(map(str, td - cd))
        b = sorted(map(str, cd - td))
        notes.append(f"data syms: target-only {a[:4]}, ours-only {b[:4]}")
    # a load moved across a call: the target reads K right before call X (same
    # basic block, the jal delay slot included) and ours only right after it
    # (or the reverse)
    for k, ta in T["anchors"].items():
        ca = C["anchors"].get(k, set())
        for kind, call in sorted(ta, key=str):
            other = "after" if kind == "before" else "before"
            if (other, call) in ca and (kind, call) not in ca and (other, call) not in ta:
                block.append(f"load order: the target reads {_mk(k)} {kind} call #{call[1] + 1} to {call[0]}, "
                             f"ours only {other} it")
                break
        else:
            continue
        break
    # swapped operands of a subtraction / division
    for (op, x, y), n in T["subs"].items():
        if C["subs"].get((op, y, x)) and not C["subs"].get((op, x, y)) and not T["subs"].get((op, y, x)):
            block.append(f"operand order: the target computes {op} {_org(x)}, {_org(y)}; ours {_org(y)}, {_org(x)}")
            break
    # shifts / addends
    a, b = T["shifts"] - C["shifts"], C["shifts"] - T["shifts"]
    if a or b:
        notes.append(f"shifts: target-only {sorted(a)[:4]}, ours-only {sorted(b)[:4]}")
    a, b = T["addends"] - C["addends"], C["addends"] - T["addends"]
    if a or b:
        notes.append(f"addr addends: target-only {sorted(a)[:4]}, ours-only {sorted(b)[:4]}")
    tv, cv = T.get("vaddends", set()), C.get("vaddends", set())
    allt = tv | T["addends"] | T["consts"] | {-x for x in T["consts"]} | T["lo16s"]
    allc = cv | C["addends"] | C["consts"] | {-x for x in C["consts"]} | C["lo16s"]
    # ((x + 127) & -128) + 2048 == (x + 2175) & -128 (MenuBackModelSet): a sum of two
    # addends of the other side explains a one-sided one
    def sums(O):
        v = [x for x in O["addends"] | O.get("vaddends", set()) if abs(x) > 1]
        return {x + y for x in v for y in v if x != y}
    def narrow(x, O):                      # x - 1 vs x + 255 before an sb (UmnTopMenu)
        return any((x - y) % 256 == 0 and abs(x - y) < 0x10000 for y in O)
    gs = T["gaddrs"] | C["gaddrs"]

    def symoff(x):                         # &sym + x on either side (a pointer, not a value)
        return any(s32(g + x) in gs for g in gs)
    a = sorted(x for x in tv - allc if abs(x) > 1 and x not in sums(C) and not narrow(x, cv) and not symoff(x))
    b = sorted(x for x in cv - allt if abs(x) > 1 and x not in sums(T) and not narrow(x, tv) and not symoff(x))
    if a or b:
        block.append(f"value addend: target-only {a[:4]}, ours-only {b[:4]} (added to a value that is stored / "
                     "compared / used as data)")
    return block, notes


def check(ctx: Ctx, da: list[str], db: list[str], bstart: int, cstart: int, bnames: dict, cnames: dict,
          bdata: dict, cdata: dict) -> dict:
    """{"ok", "diffs"[, "notes"]} for the target rows DA vs the candidate rows DB."""
    T = facts(da, bstart, bnames, bdata, ctx)
    C = facts(db, cstart, cnames, cdata, ctx)
    block, notes = compare(T, C)
    out = {"ok": not block, "diffs": block[:6], "v": VERSION}
    if notes:
        out["notes"] = notes[:4]
    return out

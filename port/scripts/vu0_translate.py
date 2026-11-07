#!/usr/bin/env python3
"""Translate the game's stubbed inline asm (PS2_ASMV / PS2_ASM) into host C.

Each asm statement is parsed (template + operands) and, if every instruction
in it is understood, rewritten as

    #ifndef PORT_HOST
        PS2_ASMV(...original, untouched...);
    #else
        { ...calls into port/runtime/vu0.h (VU0 model + EE register file)... }
    #endif

so the PS2 build sees exactly the original tokens.  A statement inside a
`#define` gets the whole define duplicated under the same #ifndef.  Already
translated sites (preceded by `#ifndef PORT_HOST`) are skipped, so the script
is idempotent.

Usage:
  vu0_translate.py [files...]        translate (default: every file with sites)
  vu0_translate.py --report [files]  list untranslated sites and the reason
  vu0_translate.py --dry [files]     show what would be translated, change nothing
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(__file__))
from wrap_inline_asm import blank, files  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SITE = re.compile(r"\bPS2_ASMV?\s*\(")


class Unsupported(Exception):
    pass


# ---------------------------------------------------------------- parsing

def c_string_concat(text):
    """Concatenate adjacent C string literals; error on anything else."""
    out = []
    pos = 0
    text = text.strip()
    while pos < len(text):
        m = re.compile(r'\s*"((?:\\.|[^"\\])*)"\s*').match(text, pos)
        if not m:
            raise Unsupported("template is not plain string literals: %r" % text[pos:pos + 40])
        out.append(m.group(1))
        pos = m.end()
    s = "".join(out)
    s = s.replace("\\n", "\n").replace("\\t", "\t").replace('\\"', '"').replace("\\\\", "\\")
    return s


def split_top(text, sep):
    b = blank(text)
    parts, depth, last = [], 0, 0
    for i, ch in enumerate(b):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == sep and depth == 0:
            parts.append(text[last:i])
            last = i + 1
    parts.append(text[last:])
    return parts


def parse_operands(sec):
    ops = []
    if not sec.strip():
        return ops
    for p in split_top(sec, ","):
        p = p.strip()
        if not p:
            continue
        m = re.match(r'(?:\[\w+\]\s*)?"([^"]*)"\s*\((.*)\)\s*$', p, re.S)
        if not m:
            raise Unsupported("operand %r" % p)
        ops.append((m.group(1), m.group(2).strip()))
    return ops


# ---------------------------------------------------------------- translation

LANE = {"x": 0, "y": 1, "z": 2, "w": 3}


def dest_mask(s):
    if not s:
        raise Unsupported("missing dest")
    m = 0
    for c in s:
        m |= 8 >> LANE[c]
    return m


class Block:
    def __init__(self, site_id, tmpl, outs, ins):
        self.id = site_id
        self.tmpl = tmpl
        self.ops = []          # (kind, constraint, expr)  kind: r f m
        for c, e in outs:
            self.ops.append(("out", c, e))
        for c, e in ins:
            self.ops.append(("in", c, e))
        self.lines = []
        self.labels = {}
        self.noreorder = False
        self.symbols = []
        self.pins = {}         # C variable -> "xsg_ee_gpr[N]" / "xsg_ee_fpr[N]" (PS2_REG)

    # operand classification
    def opkind(self, n):
        if n >= len(self.ops):
            raise Unsupported("operand %%%d out of range" % n)
        d, c, e = self.ops[n]
        cc = c.lstrip("=+&")
        if cc.isdigit():
            return self.opkind(int(cc))
        if "f" in cc:
            return "f"
        if "m" in cc:
            return "m"
        if "r" in cc or "d" in cc:
            return "r"
        raise Unsupported("constraint %r" % c)

    def opslot(self, n):
        d, c, e = self.ops[n]
        cc = c.lstrip("=+&")
        if cc.isdigit():
            return int(cc)
        return n

    # register references
    def gpr(self, tok, write=False):
        tok = tok.strip()
        m = re.fullmatch(r"%(\d+)", tok)
        if m:
            n = int(m.group(1))
            if self.opkind(n) != "r":
                raise Unsupported("%s is not a GPR operand" % tok)
            pin = self.pins.get(self.ops[self.opslot(n)][2])
            return pin or "r_%d" % self.opslot(n)
        names = {"zero": 0, "at": 1, "v0": 2, "v1": 3, "a0": 4, "a1": 5, "a2": 6, "a3": 7,
                 "t0": 8, "t1": 9, "t2": 10, "t3": 11, "t4": 12, "t5": 13, "t6": 14, "t7": 15,
                 "s0": 16, "s1": 17, "s2": 18, "s3": 19, "s4": 20, "s5": 21, "s6": 22, "s7": 23,
                 "t8": 24, "t9": 25, "gp": 28, "sp": 29, "fp": 30, "s8": 30, "ra": 31}
        m = re.fullmatch(r"\$(\d+|[a-z]\w*)", tok)
        if not m:
            raise Unsupported("GPR %r" % tok)
        r = m.group(1)
        n = int(r) if r.isdigit() else names.get(r, -1)
        if not 0 <= n < 32:
            raise Unsupported("GPR %r" % tok)
        if n == 0:
            return "xsg_ee_sink" if write else "xsg_ee_zero"
        if n in (28, 29):
            raise Unsupported("uses $gp/$sp")
        return "xsg_ee_gpr[%d]" % n

    def fpr(self, tok):
        tok = tok.strip()
        m = re.fullmatch(r"%(\d+)", tok)
        if m:
            n = int(m.group(1))
            if self.opkind(n) != "f":
                raise Unsupported("%s is not an FPR operand" % tok)
            pin = self.pins.get(self.ops[self.opslot(n)][2])
            return pin or "f_%d" % self.opslot(n)
        m = re.fullmatch(r"\$f(\d+)", tok)
        if not m:
            raise Unsupported("FPR %r" % tok)
        return "xsg_ee_fpr[%s]" % m.group(1)

    @staticmethod
    def vf(tok):
        m = re.fullmatch(r"\$?vf(\d+)([xyzw]*)", tok.strip())
        if not m:
            raise Unsupported("VF %r" % tok)
        return int(m.group(1)), m.group(2)

    @staticmethod
    def vi(tok):
        m = re.fullmatch(r"\$?vi(\d+)", tok.strip())
        if not m:
            raise Unsupported("VI %r" % tok)
        return int(m.group(1))

    def vffield(self, tok):
        n, f = self.vf(tok)
        if len(f) != 1:
            raise Unsupported("needs one field: %r" % tok)
        return n, LANE[f]

    def sym(self, name):
        if not re.fullmatch(r"[A-Za-z_]\w*", name):
            raise Unsupported("symbol %r" % name)
        if name not in self.symbols:
            self.symbols.append(name)
        return "(unsigned int)(unsigned long)xsg_sym_%s" % name

    def imm(self, tok):
        tok = tok.strip()
        m = re.fullmatch(r"%(hi|lo)\((\w+)\)", tok)
        if m:
            a = self.sym(m.group(2))
            if m.group(1) == "hi":
                return "((%s + 0x8000u) >> 16)" % a
            return "(int)(short)(%s & 0xffffu)" % a
        m = re.fullmatch(r"%(\d+)", tok)
        if m:
            n = int(m.group(1))
            d, c, e = self.ops[n]
            if "i" not in c and "n" not in c:
                raise Unsupported("immediate operand %r" % c)
            return "(%s)" % e
        try:
            return str(int(tok, 0))
        except ValueError:
            raise Unsupported("immediate %r" % tok)

    def ea(self, tok):
        """Memory operand -> C `void *` expression."""
        tok = tok.strip()
        m = re.fullmatch(r"%(\d+)", tok)
        if m:
            n = int(m.group(1))
            if self.opkind(n) != "m":
                raise Unsupported("bare %s as address" % tok)
            return "m_%d" % self.opslot(n)
        m = re.fullmatch(r"(.*?)\((\$\w+|%\d+)\)", tok)
        if m:
            off = m.group(1).strip() or "0"
            base = self.gpr(m.group(2))
            return "XSG_EA(%s, %s)" % (base, self.imm(off))
        m = re.fullmatch(r"([A-Za-z_]\w*)(?:\s*\+\s*(\w+))?", tok)
        if m:
            self.sym(m.group(1))
            off = m.group(2) or "0"
            return "(xsg_sym_%s + %s)" % (m.group(1), int(off, 0))
        raise Unsupported("address %r" % tok)

    # ------------------------------------------------------------ main
    def translate(self):
        lines = self.tmpl.replace("%%", "%")
        stmts = []
        for raw in re.split(r"[\n;]", lines):
            raw = raw.split("#")[0].strip()
            while True:
                m = re.match(r"(\w+):\s*(.*)$", raw)
                if not m or re.match(r"\w+\.\w", raw):
                    break
                stmts.append(("label", m.group(1)))
                raw = m.group(2)
            if raw:
                stmts.append(("ins", raw))
        # label resolution: unique names for each definition
        defs = []
        for i, (k, v) in enumerate(stmts):
            if k == "label":
                defs.append((i, v))
        names = {i: "xsg_L%s_%d" % (self.id, j) for j, (i, v) in enumerate(defs)}
        self.used_labels = set()

        def resolve(idx, ref):
            m = re.fullmatch(r"(\d+)([fb])", ref)
            if m:
                lab, d = m.groups()
                cands = [i for i, v in defs if v == lab and (i > idx if d == "f" else i < idx)]
                if not cands:
                    raise Unsupported("label %s" % ref)
                i = cands[0] if d == "f" else cands[-1]
            else:
                cands = [i for i, v in defs if v == ref]
                if not cands:
                    raise Unsupported("label %s" % ref)
                i = cands[0]
            self.used_labels.add(names[i])
            return names[i]

        out = []
        i = 0
        while i < len(stmts):
            k, v = stmts[i]
            if k == "label":
                out.append("%s:;" % names[i])
                i += 1
                continue
            res = self.ins(v, i, resolve)
            if isinstance(res, tuple):          # branch: (cond, target)
                cond, target = res
                if self.noreorder:
                    # delay slot executes before the jump
                    j = i + 1
                    while j < len(stmts) and stmts[j][0] == "label":
                        raise Unsupported("label in delay slot")
                    delay = self.ins(stmts[j][1], j, resolve) if j < len(stmts) else []
                    if isinstance(delay, tuple):
                        raise Unsupported("branch in delay slot")
                    out.append("{ int xsg_c_ = %s;" % cond)
                    out += ["  " + d for d in delay]
                    out.append("  if (xsg_c_) goto %s; }" % target)
                    i = j + 1
                    continue
                out.append("if (%s) goto %s;" % (cond, target))
            else:
                out += res
            i += 1
        return out

    def ins(self, text, idx, resolve):
        m = re.match(r"([A-Za-z_.][\w.]*)\s*(.*)$", text)
        if not m:
            raise Unsupported("instruction %r" % text)
        mn, args = m.group(1), m.group(2)
        a = [x.strip() for x in split_top(args, ",")] if args.strip() else []
        mnl = mn.lower()

        if mnl == ".set":
            if a and a[0] == "noreorder":
                self.noreorder = True
            elif a and a[0] == "reorder":
                self.noreorder = False
            elif a and a[0] == "pop":
                self.noreorder = False
            return []
        if mnl in (".align", "nop", "vnop", "sync", "sync.l", "sync.p", "ei", "di", "ssnop", "break"):
            return []
        if mnl == "vwaitq":
            return ["vu0_waitq();"]
        if mnl.startswith("v") and not mnl.startswith("vcallms"):
            return self.vu(mnl, a)
        return self.ee(mnl, a, idx, resolve)

    # ------------------------------------------------------------ EE
    def ee(self, mn, a, idx, resolve):
        G = self.gpr
        W = lambda t: self.gpr(t, write=True)
        F = self.fpr
        mn0 = re.sub(r"\.(n?i)$", "", mn) if mn.startswith(("qmtc2", "qmfc2", "ctc2", "cfc2")) else mn
        if mn0 == "lqc2":
            return ["vu0_lqc2(%d, %s);" % (self.vf(a[0])[0], self.ea(a[1]))]
        if mn0 == "sqc2":
            return ["vu0_sqc2(%d, %s);" % (self.vf(a[0])[0], self.ea(a[1]))]
        if mn0 == "qmtc2":
            return ["vu0_qmtc2(%d, &%s);" % (self.vf(a[1])[0], G(a[0]))]
        if mn0 == "qmfc2":
            return ["vu0_qmfc2(&%s, %d);" % (W(a[0]), self.vf(a[1])[0])]
        if mn0 == "ctc2":
            return ["vu0_ctc2(%d, &%s);" % (self.vi(a[1]), G(a[0]))]
        if mn0 == "cfc2":
            return ["vu0_cfc2(&%s, %d);" % (W(a[0]), self.vi(a[1]))]
        if mn == "vcallms":
            return ["vu0_callms(%s);" % self.imm(a[0])]
        if mn == "vcallmsr":
            if self.vi(a[0]) != 27:
                raise Unsupported("vcallmsr not $vi27")
            return ["vu0_callmsr();"]
        set32 = lambda d, e: ["xsg_ee_set32(&%s, %s);" % (W(d), e)]
        if mn in ("addi", "addiu"):
            return set32(a[0], "%s.w[0] + (unsigned int)(%s)" % (G(a[1]), self.imm(a[2])))
        if mn == "daddiu" or mn == "daddi":
            return ["%s.sd[0] = %s.sd[0] + (long long)(%s);" % (W(a[0]), G(a[1]), self.imm(a[2]))]
        if mn in ("add", "addu"):
            return set32(a[0], "%s.w[0] + %s.w[0]" % (G(a[1]), G(a[2])))
        if mn in ("sub", "subu"):
            return set32(a[0], "%s.w[0] - %s.w[0]" % (G(a[1]), G(a[2])))
        if mn in ("daddu", "dadd"):
            return ["%s.d[0] = %s.d[0] + %s.d[0];" % (W(a[0]), G(a[1]), G(a[2]))]
        if mn in ("dsubu", "dsub"):
            return ["%s.d[0] = %s.d[0] - %s.d[0];" % (W(a[0]), G(a[1]), G(a[2]))]
        if mn == "move":
            return ["%s.d[0] = %s.d[0];" % (W(a[0]), G(a[1]))]
        if mn == "li":
            return ["%s.sd[0] = (long long)(int)(%s);" % (W(a[0]), self.imm(a[1]))]
        if mn == "lui":
            return set32(a[0], "(unsigned int)(%s) << 16" % self.imm(a[1]))
        if mn in ("andi", "ori", "xori"):
            op = {"andi": "&", "ori": "|", "xori": "^"}[mn]
            return ["%s.d[0] = %s.d[0] %s (unsigned long long)(unsigned short)(%s);" % (W(a[0]), G(a[1]), op, self.imm(a[2]))]
        if mn in ("and", "or", "xor"):
            op = {"and": "&", "or": "|", "xor": "^"}[mn]
            return ["%s.d[0] = %s.d[0] %s %s.d[0];" % (W(a[0]), G(a[1]), op, G(a[2]))]
        if mn == "nor":
            return ["%s.d[0] = ~(%s.d[0] | %s.d[0]);" % (W(a[0]), G(a[1]), G(a[2]))]
        if mn in ("slti", "sltiu"):
            if mn == "slti":
                return ["%s.d[0] = %s.sd[0] < (long long)(%s);" % (W(a[0]), G(a[1]), self.imm(a[2]))]
            return ["%s.d[0] = %s.d[0] < (unsigned long long)(long long)(%s);" % (W(a[0]), G(a[1]), self.imm(a[2]))]
        if mn in ("slt", "sltu"):
            f = "sd" if mn == "slt" else "d"
            return ["%s.d[0] = %s.%s[0] < %s.%s[0];" % (W(a[0]), G(a[1]), f, G(a[2]), f)]
        if mn in ("sll", "srl", "sra"):
            e = {"sll": "%s.w[0] << %s", "srl": "%s.w[0] >> %s", "sra": "(unsigned int)(%s.sw[0] >> %s)"}[mn]
            return set32(a[0], e % (G(a[1]), self.imm(a[2])))
        if mn in ("dsll", "dsrl", "dsra", "dsll32", "dsrl32", "dsra32"):
            sh = "(%s)" % self.imm(a[2]) + (" + 32" if mn.endswith("32") else "")
            if mn.startswith("dsra"):
                return ["%s.sd[0] = %s.sd[0] >> (%s);" % (W(a[0]), G(a[1]), sh)]
            op = "<<" if mn.startswith("dsll") else ">>"
            return ["%s.d[0] = %s.d[0] %s (%s);" % (W(a[0]), G(a[1]), op, sh)]
        if mn in ("movz", "movn"):
            c = "==" if mn == "movz" else "!="
            return ["if (%s.d[0] %s 0) %s.d[0] = %s.d[0];" % (G(a[2]), c, W(a[0]), G(a[1]))]
        if mn in ("pcpyld", "pcpyud", "pextlw", "pextuw", "pxor", "por", "pand", "pnor", "paddw", "psubw",
                  "pcgth", "pextlh", "pmaxw", "pminw", "ppach", "ppacb", "pextlb", "paddh", "psubh"):
            return ["xsg_mmi_%s(&%s, &%s, &%s);" % (mn, W(a[0]), G(a[1]), G(a[2]))]
        if mn in ("pexew", "pcpyh"):
            return ["xsg_mmi_%s(&%s, &xsg_ee_zero, &%s);" % (mn, W(a[0]), G(a[1]))]
        loads = {"lb": ("signed char", "sd"), "lbu": ("unsigned char", "d"), "lh": ("short", "sd"),
                 "lhu": ("unsigned short", "d"), "lw": ("int", "sd"), "lwu": ("unsigned int", "d"),
                 "ld": ("unsigned long long", "d")}
        if mn in loads:
            t, f = loads[mn]
            return ["{ %s xsg_v_; __builtin_memcpy(&xsg_v_, %s, sizeof xsg_v_); %s.%s[0] = xsg_v_; }"
                    % (t, self.ea(a[1]), W(a[0]), f)]
        if mn in ("ldl", "ldr"):
            # used as the unaligned ldl 7(x)/ldr 0(x) pair: each loads the whole doubleword
            ea = self.ea(a[1])
            if mn == "ldl":
                ea = "((char *)%s - 7)" % ea
            return ["__builtin_memcpy(&%s.d[0], %s, 8);" % (W(a[0]), ea)]
        stores = {"sb": 1, "sh": 2, "sw": 4, "sd": 8, "sq": 16}
        if mn in stores:
            return ["__builtin_memcpy(%s, &%s, %d);" % (self.ea(a[1]), G(a[0]), stores[mn])]
        if mn == "lq":
            return ["__builtin_memcpy(&%s, %s, 16);" % (W(a[0]), self.ea(a[1]))]
        if mn == "lwc1":
            return ["__builtin_memcpy(&%s, %s, 4);" % (F(a[0]), self.ea(a[1]))]
        if mn == "swc1":
            return ["__builtin_memcpy(%s, &%s, 4);" % (self.ea(a[1]), F(a[0]))]
        if mn == "mfc1":
            return set32(a[0], "%s.u" % F(a[1]))
        if mn == "mtc1":
            return ["%s.u = %s.w[0];" % (F(a[1]), G(a[0]))]
        if mn == "li.s":
            return ["%s.f = %sf;" % (F(a[0]), a[1])]
        if mn in ("add.s", "sub.s", "mul.s", "div.s"):
            op = {"add.s": "+", "sub.s": "-", "mul.s": "*", "div.s": "/"}[mn]
            return ["%s.f = %s.f %s %s.f;" % (F(a[0]), F(a[1]), op, F(a[2]))]
        if mn in ("mov.s", "neg.s", "abs.s", "sqrt.s"):
            e = {"mov.s": "%s.f", "neg.s": "-%s.f", "abs.s": "__builtin_fabsf(%s.f)", "sqrt.s": "__builtin_sqrtf(%s.f)"}[mn]
            return ["%s.f = %s;" % (F(a[0]), e % F(a[1]))]
        if mn == "cvt.w.s":
            return ["%s.i = (int)%s.f;" % (F(a[0]), F(a[1]))]
        if mn == "cvt.s.w":
            return ["%s.f = (float)%s.i;" % (F(a[0]), F(a[1]))]
        if mn in ("c.lt.s", "c.le.s", "c.eq.s"):
            op = {"c.lt.s": "<", "c.le.s": "<=", "c.eq.s": "=="}[mn]
            return ["xsg_ee_fcc = %s.f %s %s.f;" % (F(a[0]), op, F(a[1]))]
        # branches
        if mn in ("b", "j"):
            return ("1", resolve(idx, a[0]))
        if mn in ("beq", "bne"):
            op = "==" if mn == "beq" else "!="
            return ("%s.d[0] %s %s.d[0]" % (G(a[0]), op, G(a[1])), resolve(idx, a[2]))
        if mn in ("beqz", "bnez"):
            op = "==" if mn == "beqz" else "!="
            return ("%s.d[0] %s 0" % (G(a[0]), op), resolve(idx, a[1]))
        if mn in ("bgez", "bltz", "blez", "bgtz"):
            op = {"bgez": ">=", "bltz": "<", "blez": "<=", "bgtz": ">"}[mn]
            return ("%s.sd[0] %s 0" % (G(a[0]), op), resolve(idx, a[1]))
        if mn in ("bc1t", "bc1f"):
            return ("xsg_ee_fcc" if mn == "bc1t" else "!xsg_ee_fcc", resolve(idx, a[0]))
        raise Unsupported("EE op %s" % mn)

    # ------------------------------------------------------------ VU0 macro
    def vu(self, mn, a):
        m = re.fullmatch(r"v(madd|msub|mini|max|mul|add|sub)(a?)([xyzwqi]?)\.([xyzw]+)", mn)
        if m:
            op, acc, bc, dest = m.groups()
            if acc and op in ("max", "mini"):
                raise Unsupported(mn)
            dm = dest_mask(dest)
            if acc:
                if not a[0].upper().startswith("ACC"):
                    raise Unsupported("acc dest %r" % a[0])
                fd = "VU0_ACC"
            else:
                fd = str(self.vf(a[0])[0])
            fs = self.vf(a[1])[0]
            if bc == "q":
                ft, b = 0, "VU0_BQ"
            elif bc == "i":
                ft, b = 0, "VU0_BI"
            elif bc:
                ft, b = self.vf(a[2])[0], str(LANE[bc])
            else:
                ft, b = self.vf(a[2])[0], "VU0_VEC"
            return ["vu0_arith(VU0_%s, %d, %s, %d, %d, %s);" % (op.upper(), dm, fd, fs, ft, b)]
        m = re.fullmatch(r"v(move|mr32|abs)\.([xyzw]+)", mn)
        if m:
            return ["vu0_%s(%d, %d, %d);" % (m.group(1), dest_mask(m.group(2)), self.vf(a[0])[0], self.vf(a[1])[0])]
        m = re.fullmatch(r"v(itof|ftoi)(0|4|12|15)\.([xyzw]+)", mn)
        if m:
            return ["vu0_%s(%s, %d, %d, %d);" % (m.group(1), m.group(2), dest_mask(m.group(3)),
                                                  self.vf(a[0])[0], self.vf(a[1])[0])]
        m = re.fullmatch(r"vclipw\.xyz", mn)
        if m:
            fs = self.vf(a[0])[0]
            ft, f = self.vf(a[1])
            return ["vu0_clip(%d, %d);" % (fs, ft)]
        if mn in ("vopmula.xyz", "vopmsub.xyz"):
            if mn == "vopmula.xyz":
                return ["vu0_opmula(14, %d, %d);" % (self.vf(a[1])[0], self.vf(a[2])[0])]
            return ["vu0_opmsub(14, %d, %d, %d);" % (self.vf(a[0])[0], self.vf(a[1])[0], self.vf(a[2])[0])]
        if mn == "vdiv":
            fs, fsf = self.vffield(a[1])
            ft, ftf = self.vffield(a[2])
            return ["vu0_div(%d, %d, %d, %d);" % (fs, fsf, ft, ftf)]
        if mn == "vsqrt":
            ft, ftf = self.vffield(a[1])
            return ["vu0_sqrt(%d, %d);" % (ft, ftf)]
        if mn == "vrsqrt":
            fs, fsf = self.vffield(a[1])
            ft, ftf = self.vffield(a[2])
            return ["vu0_rsqrt(%d, %d, %d, %d);" % (fs, fsf, ft, ftf)]
        m = re.fullmatch(r"vmfir\.([xyzw]+)", mn)
        if m:
            return ["vu0_mfir(%d, %d, %d);" % (dest_mask(m.group(1)), self.vf(a[0])[0], self.vi(a[1]))]
        if mn == "vmtir":
            fs, fsf = self.vffield(a[1])
            return ["vu0_mtir(%d, %d, %d);" % (self.vi(a[0]), fs, fsf)]
        if mn in ("viadd", "visub", "viand", "vior"):
            return ["vu0_%s(%d, %d, %d);" % (mn[1:], self.vi(a[0]), self.vi(a[1]), self.vi(a[2]))]
        if mn == "viaddi":
            return ["vu0_iaddi(%d, %d, %s);" % (self.vi(a[0]), self.vi(a[1]), self.imm(a[2]))]
        m = re.fullmatch(r"v(lqi|sqi|lqd|sqd)\.([xyzw]+)", mn)
        if m:
            r = re.fullmatch(r"\(\s*(?:--)?\s*(\$?vi\d+)\s*(?:\+\+)?\s*\)", a[1])
            if not r:
                raise Unsupported("%s operand %r" % (mn, a[1]))
            return ["vu0_%s(%d, %d, %d);" % (m.group(1), dest_mask(m.group(2)), self.vf(a[0])[0], self.vi(r.group(1)))]
        m = re.fullmatch(r"v(ilwr|iswr)\.([xyzw])", mn)
        if m:
            r = re.fullmatch(r"\(\s*(\$?vi\d+)\s*\)", a[1])
            if not r:
                raise Unsupported("%s operand" % mn)
            return ["vu0_%s(%d, %d, %d);" % (m.group(1), dest_mask(m.group(2)), self.vi(a[0]), self.vi(r.group(1)))]
        if mn in ("vrinit", "vrxor"):
            fs, fsf = self.vffield(a[1])
            return ["vu0_%s(%d, %d);" % (mn[1:], fs, fsf)]
        m = re.fullmatch(r"v(rget|rnext)\.([xyzw]+)", mn)
        if m:
            return ["vu0_%s(%d, %d);" % (m.group(1), dest_mask(m.group(2)), self.vf(a[0])[0])]
        raise Unsupported("VU op %s" % mn)

    # ------------------------------------------------------------ emit
    def emit(self, indent):
        body = self.translate()
        pre, post, stm_pins = [], [], []
        for n, (d, c, e) in enumerate(self.ops):
            cc = c.lstrip("=+&")
            if cc.isdigit():
                k = self.opkind(int(cc))
                tgt = int(cc)
                if k == "r":
                    pre.append("r_%d = XSG_IN(%s);" % (tgt, e))
                elif k == "f":
                    pre.append("f_%d = XSG_FIN(%s);" % (tgt, e))
                continue
            k = self.opkind(n)
            isout, inout = d == "out", "+" in c
            pin = self.pins.get(e)
            if pin and k in ("r", "f"):
                # the variable lives in that hard register: so does the operand
                if not isout or inout:
                    stm_pins.append("%s = %s(%s);" % (pin, "XSG_IN" if k == "r" else "XSG_FIN", e))
                if isout:
                    post.append("XSG_OUT(%s, %s);" % (e, pin) if k == "r" else "%s = %s.f;" % (e, pin))
                continue
            if k == "r":
                pre.insert(0, "XsgGpr r_%d = %s;" % (n, "XSG_IN(%s)" % e if (not isout or inout) else "{{0, 0}}"))
                if isout:
                    post.append("XSG_OUT(%s, r_%d);" % (e, n))
            elif k == "f":
                pre.insert(0, "XsgFpr f_%d = %s;" % (n, "XSG_FIN(%s)" % e if (not isout or inout) else "{0}"))
                if isout:
                    post.append("%s = f_%d.f;" % (e, n))
            else:
                pre.insert(0, "void *m_%d = (void *)&(%s);" % (n, e))
        # declarations must come before statements (gnu89 is lenient, but keep it tidy)
        decls = [p for p in pre if p.startswith(("XsgGpr", "XsgFpr", "void *"))]
        # symbols named in the template, bound by assembler name (works for
        # file-scope statics and for data the C does not declare)
        decls = ['extern char xsg_sym_%s[] __asm__("%s");' % (n, n) for n in self.symbols] + decls
        stm = stm_pins + [p for p in pre if p not in decls]
        lines = decls + stm + body + ["xsg_vu0_blocks++;"] + post
        out = [indent + "{"]
        out += [indent + "    " + l for l in lines]
        out.append(indent + "}")
        return out


# ---------------------------------------------------------------- files

def find_sites(text):
    b = blank(text)
    sites = []
    for m in SITE.finditer(b):
        start = m.start()
        depth = 0
        for j in range(m.end() - 1, len(b)):
            if b[j] == "(":
                depth += 1
            elif b[j] == ")":
                depth -= 1
                if depth == 0:
                    break
        end = j + 1
        sites.append((start, end, text[m.end():j]))
    return sites


def define_span(text, pos):
    """If pos lies inside a #define (with continuations), return its line span."""
    ls = text.rfind("\n", 0, pos) + 1
    # walk back over continued lines
    while ls > 0:
        prev_end = ls - 1
        prev_start = text.rfind("\n", 0, prev_end) + 1
        if text[prev_start:prev_end].rstrip().endswith("\\"):
            ls = prev_start
        else:
            break
    if not text[ls:].lstrip().startswith("#"):
        return None
    if not re.match(r"\s*#\s*define\b", text[ls:]):
        return None
    le = ls
    while True:
        e = text.find("\n", le)
        if e < 0:
            e = len(text)
        if text[le:e].rstrip().endswith("\\"):
            le = e + 1
            continue
        return ls, e


def find_pins(text, pos):
    """`register T x PS2_REG("$N")` declared in the function containing pos."""
    fstart = text.rfind("\n{", 0, pos)
    region = text[fstart if fstart >= 0 else 0:pos]
    pins = {}
    for m in re.finditer(r'\bregister\b[^;{}]*?\b(\w+)\s*PS2_REG\s*\(\s*"\$(f?)(\d+)"\s*\)', region):
        pins[m.group(1)] = "xsg_ee_%s[%s]" % ("fpr" if m.group(2) else "gpr", m.group(3))
    return pins


def translate_site(body, site_id, pins=None):
    body = body.replace("\\\n", "\n")
    parts = split_top(body, ":")
    tmpl = c_string_concat(parts[0])
    outs = parse_operands(parts[1]) if len(parts) > 1 else []
    ins = parse_operands(parts[2]) if len(parts) > 2 else []
    blk = Block(site_id, tmpl, outs, ins)
    blk.pins = pins or {}
    return blk


def already(text, start):
    ls = text.rfind("\n", 0, start) + 1
    # Is there an `#ifndef PORT_HOST` opening the region this site is in?
    before = text[:ls].rstrip("\n").split("\n")
    k = len(before) - 1
    depth = 0
    while k >= 0:
        l = before[k].strip()
        if l.startswith("#endif"):
            depth += 1
        elif l.startswith("#if"):
            if depth == 0:
                return l.startswith("#ifndef PORT_HOST")
            depth -= 1
        k -= 1
        if len(before) - k > 400:
            break
    return False


def process(path, mode):
    text = open(path).read()
    sites = find_sites(text)
    edits = []          # (start, end, replacement)
    report = []
    done_defs = set()
    stem = re.sub(r"\W", "_", os.path.splitext(os.path.basename(path))[0])
    n_ok = 0
    for k, (start, end, body) in enumerate(sites):
        line = text.count("\n", 0, start) + 1
        if already(text, start):
            continue
        dspan = define_span(text, start)
        if dspan and dspan in done_defs:
            continue
        try:
            if dspan:
                # all sites in the define must translate
                ds, de = dspan
                dtext = text[ds:de]
                new = dtext
                sub_sites = find_sites(dtext)
                for j, (s2, e2, b2) in reversed(list(enumerate(sub_sites))):
                    blk = translate_site(b2, "%s_%d_%d" % (stem, line, j))
                    code = blk.emit("")
                    if blk.used_labels:
                        raise Unsupported("labels inside a #define")
                    rep = "({ " + " ".join(l.strip() for l in code) + " })"
                    e3 = e2
                    new = new[:s2] + rep + new[e3:]
                # re-wrap with continuations
                new_lines = new.split("\n")
                new = " \\\n".join(l.rstrip().rstrip("\\").rstrip() for l in new_lines)
                rep = "#ifndef PORT_HOST\n" + dtext + "\n#else\n" + new + "\n#endif"
                edits.append((ds, de, rep))
                done_defs.add(dspan)
                n_ok += len(sub_sites)
                continue
            blk = translate_site(body, "%s_%d" % (stem, line), find_pins(text, start))
            ls = text.rfind("\n", 0, start) + 1
            indent = re.match(r"[ \t]*", text[ls:]).group(0)
            # swallow the trailing ';'
            e = end
            m = re.match(r"\s*;", text[e:])
            if not m:
                raise Unsupported("asm not a statement")
            e += m.end()
            code = blk.emit(indent)
            prefix = text[ls:start]
            orig = text[start:e]
            head = ""
            if prefix.strip():
                head = "\n"
                orig = indent + orig
            rep = (head + "#ifndef PORT_HOST\n" + ("" if prefix.strip() else indent) + orig.lstrip(" \t") +
                   "\n#else\n" + "\n".join(code) + "\n#endif")
            if not prefix.strip():
                edits.append((ls, e, rep))
            else:
                edits.append((start, e, rep))
            n_ok += 1
            # the rest of the line after ';' must move to a new line
        except Unsupported as ex:
            report.append((path, line, str(ex)))
    if mode == "write" and edits:
        for s, e, rep in sorted(edits, reverse=True):
            tail = text[e:]
            if tail and not tail.startswith("\n"):
                rep += "\n"
            text = text[:s] + rep + text[e:]
        open(path, "w").write(text)
    return n_ok, report


def main():
    args = sys.argv[1:]
    mode = "write"
    if args and args[0] in ("--report", "--dry"):
        mode = args.pop(0)[2:]
    os.chdir(ROOT)
    targets = args or [f for f in files() if f.endswith(".c") or f.endswith(".h")]
    total_ok, all_rep = 0, []
    for f in targets:
        ok, rep = process(f, mode)
        total_ok += ok
        all_rep += rep
    left = {}
    for p, l, r in all_rep:
        left.setdefault(p, []).append((l, r))
    print("%s: %d sites translatable, %d untranslated" % (mode, total_ok, len(all_rep)))
    if mode != "write" or all_rep:
        for p in sorted(left):
            print("%s: %d" % (p, len(left[p])))
            for l, r in left[p]:
                print("    %d: %s" % (l, r))


if __name__ == "__main__":
    main()

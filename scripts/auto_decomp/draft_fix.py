#!/usr/bin/env python3
"""Make a raw m2c draft body COMPILE inside its TU (it need not match).

    python3 -B scripts/auto_decomp/draft_fix.py --unit main --tu file --name MenuFile \
        [--body PATH] [--write] [--max-iter 30] [--quiet]

Error-driven: the body is spliced into the TU (tools/tu_edit.py to-c) and
compiled with the TU's exact command; gcc's error lines are mapped back to
body lines and fixed deterministically:

  * missing s8/u8/s16/u16/s32/u32/s64/u64/f32/f64 typedefs are prepended;
  * m2c `?` placeholder types become int, `(bitwise T)` casts become `(T)`,
    M2C_ERROR()/M2C_BREAK()/M2C_SYNC() get stub macros; fold14: a callee
    result m2c could not see (`M2C_ERROR(/* Read from unset register $f0 */)`
    right after `g(..);` or in `(g(..), M2C_ERROR(..))`) becomes a
    `fret_g = ((float (*)(..))g)(..)` temp / inline cast call (o552);
  * draft declarations the TU already has (redefinition / conflicting types /
    static-follows-non-static) are commented out;
  * implicitly declared callees get their prototype from protos.json
    (proto_index.py), undeclared globals an extern sized from
    config/symbols/<unit>.txt, undeclared m2c locals an `int` local;
  * `p->unkXX` / `s.unk_XX` / `unmodeled_XX` on a struct lacking the member
    become `*(T *)((char *)p + 0xXX)`, T from the asm's access width at that
    offset (default int);
  * calls with too many / too few arguments are trimmed (m2c's
    `/* extra? */` args first) or padded with 0 against the prototype.
  * functions used as values (fn-pointer tables) get a prototype, not a data
    extern (type:func in the symbol file); a table entry naming an undeclared
    function never gets the whole table dropped;
  * a global we could only size-guess (`extern int/char[N] v`) is retyped when
    it is used with `->`/`.`: the draft's own dropped declaration is restored
    (after imported T__df types), else the TU's later `extern T v;` becomes a
    block-scope extern (gcc 2.96 accepts that against later file-scope ones);
    our prelude externs that clash with a later TU declaration move into the
    using functions the same way;
  * undeclared stack temps used as `spXX[i]` are declared as int arrays sized
    by the gap to the next m2c stack slot (default 16 words); other scalar
    locals indexed with [] become `((T *)&v)[i]`;
  * `called object is not a function` on `tbl[i](...)` / `p->f(...)` ->
    `((int (*)())tbl[i])(...)`;
  * gcc error lines are mapped to the body line with identical text (comments
    ignored) when the diff mapping drifts (big multi-item drafts);
  * past --max-iter rounds it keeps going (up to 4x) while the error count
    still falls.

Real statements are never deleted or commented out. Output goes to
<body>.fixed.c; --write also replaces the body when the current body does
not compile (never a compiling body, never .best.c). When the result
compiles it is scored with try_fn.try_function and .best.c/.best.json are
updated if it beats the existing best. Uses no try budget and no locks.
"""
from __future__ import annotations

import argparse
import difflib
import json
import os
import re
import shutil
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import try_fn  # noqa: E402

PROTOS = ROOT / "build/auto_decomp/protos.json"
TAG = "/* draft_fix */"

BASIC_TYPES = {
    "s8": "signed char", "u8": "unsigned char", "s16": "signed short", "u16": "unsigned short",
    "s32": "signed int", "u32": "unsigned int", "s64": "signed long long",
    "u64": "unsigned long long", "f32": "float", "f64": "double",
}
M2C_MACROS = {
    "M2C_ERROR": "#define M2C_ERROR(args...) 0",
    "M2C_BREAK": "#define M2C_BREAK(args...)",
    "M2C_SYNC": "#define M2C_SYNC(args...)",
    "M2C_TRAP_IF": "#define M2C_TRAP_IF(args...)",
}
HAS_ASM = re.compile(r"\b(?:__asm__|__asm|asm)\b")
UNK_MEMBER = re.compile(r"^(?:unk_?|unmodeled_|field_0x|unk0x|pad_?|half_?|byte_?|word_?|(unkneg))([0-9A-Fa-f]+)$")


# ---------------------------------------------------------------- compile --

def compile_body(unit: str, tu: str, name: str, text: str) -> tuple[bool, str, list[str] | None]:
    """(ok, log, candidate TU lines). Same splice + command as try_fn's cand side."""
    src = ROOT / "src" / unit / f"{tu}.c"
    unit_dir, g, rule, edge = try_fn.compile_edge(unit, tu)
    scratch_rel = Path("try") / f"df.{tu}.{name}.{os.getpid()}"
    scratch = unit_dir / scratch_rel
    shutil.rmtree(scratch, ignore_errors=True)
    scratch.mkdir(parents=True)
    try:
        body = scratch / "body.c"
        body.write_text(text)
        p = try_fn.tu_edit("to-c", str(src), name, "--body", str(body), "--unit-dir", str(unit_dir))
        if p.returncode != 0:
            return False, "tu_edit: " + (p.stdout + p.stderr)[-3000:], None
        cand = p.stdout
        src_copy = try_fn.make_src_dir(scratch / "c", unit, tu, cand)
        ok, log = try_fn.run_compile(unit_dir, g, rule, edge, src_copy,
                                     str(scratch_rel / "c" / f"{tu}.o"), str(scratch_rel / "c" / "cinc"))
        return ok, log, cand.splitlines()
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


_ERR = re.compile(r"^(?P<file>[^:\s]+\.c):(?P<line>\d+): (?P<msg>.*)$")


def map_lines(body: list[str], cand: list[str]) -> dict[int, int]:
    """candidate line index -> body line index."""
    norm = lambda ls: [l.strip() for l in ls]  # noqa: E731
    b, c = norm(body), norm(cand)
    # anchor: a distinctive body line that occurs once in the candidate
    anchor = None
    for i, l in enumerate(b):
        if len(l) > 12 and c.count(l) == 1 and b.count(l) == 1:
            anchor = c.index(l) - i
            break
    lo, hi = 0, len(c)
    if anchor is not None:
        lo, hi = max(0, anchor - 200), min(len(c), anchor + len(b) + 200)
    sm = difflib.SequenceMatcher(None, b, c[lo:hi], autojunk=False)
    m = {}
    for blk in sm.get_matching_blocks():
        for k in range(blk.size):
            m[lo + blk.b + k] = blk.a + k
    # lines in gaps (edited by tu_edit) map to the nearest preceding mapped line
    if m:
        first, last = min(m), max(m)
        cur = None
        for ci in range(first, last + 1):
            if ci in m:
                cur = m[ci]
            elif cur is not None:
                m[ci] = cur
    return m


def parse_errors(log: str, body: list[str], cand: list[str] | None) -> tuple[list, list]:
    """([(body_idx|None, msg)], [warnings same form]); only *.c lines of the candidate."""
    if cand is None:
        return [(None, log.strip()[-500:])], []
    m = map_lines(body, cand)
    key = lambda l: " ".join(re.sub(r"/\*.*?\*/", " ", l).split())  # noqa: E731
    where: dict[str, list[int]] = {}
    for i, l in enumerate(body):
        if len(key(l)) > 3:
            where.setdefault(key(l), []).append(i)
    errs, warns = [], []
    for l in log.splitlines():
        e = _ERR.match(l.strip())
        if not e or not e["file"].endswith(".c"):
            continue
        ci = int(e["line"]) - 1
        idx = m.get(ci)
        # the diff mapping can drift on big drafts (tu_edit moves/drops parts):
        # prefer the body line with the identical text, nearest the diff guess
        hits = where.get(key(cand[ci])) if 0 <= ci < len(cand) else None
        if hits and idx is not None and idx not in hits:
            idx = min(hits, key=lambda h: abs(h - idx))
        msg = e["msg"]
        (warns if msg.startswith("warning:") else errs).append((idx, msg))
    if not errs and "error" in log.lower() and not log.strip().endswith(".o"):
        tail = [l for l in log.splitlines() if "rror" in l]
        if tail:
            errs.append((None, tail[-1][-300:]))
    return errs, warns


# ------------------------------------------------------------- knowledge --

class Ctx:
    def __init__(self, unit: str, tu: str, name: str):
        self.unit, self.tu, self.name = unit, tu, name
        self.decls = "\n".join(l for l in self._decls().splitlines() if not l.lstrip().startswith("#"))
        try:
            self.protos = json.loads(PROTOS.read_text())
        except (OSError, ValueError):
            self.protos = {}
        self.funcs: set[str] = set()
        self.sizes = self._sizes()
        self.asm = self._asm()
        self.tried: set = set()          # one-shot fixes, persistent across rounds
        try:
            self.tu_src = (ROOT / "src" / unit / f"{tu}.c").read_text(errors="replace")
        except OSError:
            self.tu_src = ""

    def _decls(self) -> str:
        try:
            import m2c_seed
            ctx = m2c_seed.context_file(self.unit, self.tu)
            if ctx:
                return ctx.read_text(errors="replace")
        except Exception:  # noqa: BLE001
            pass
        return (ROOT / "src" / self.unit / f"{self.tu}.c").read_text(errors="replace")

    def _sizes(self) -> dict[str, int]:
        out = {}
        for u in dict.fromkeys(["main", self.unit]):
            f = ROOT / "config/symbols" / f"{u}.txt"
            if not f.is_file():
                continue
            for l in f.read_text(errors="replace").splitlines():
                m = re.match(r"\s*(\w+)\s*=\s*0x[0-9A-Fa-f]+;", l)
                if not m:
                    continue
                if re.search(r"\btype:func\b", l):
                    self.funcs.add(m.group(1))
                    continue
                sz = re.search(r"\bsize:(0x[0-9A-Fa-f]+|\d+)", l)
                if sz:
                    out[m.group(1)] = int(sz.group(1), 0)
        return out

    def _asm(self) -> str:
        hits = list((ROOT / "build" / self.unit / "asm").rglob(f"nonmatchings/**/{self.tu}/{self.name}.s"))
        return hits[0].read_text(errors="replace") if hits else ""

    def has_typedef(self, t: str) -> bool:
        return bool(re.search(rf"typedef[^;{{}}]*\b{t}\s*;", self.decls)
                    or re.search(rf"#\s*define\s+{t}\b", self.decls))

    def width_type(self, off: int) -> str:
        ops = set(re.findall(rf"\b([ls](?:bu?|hu?|wu?|d|wc1))\s+\$\w+,\s*(?:0x)?{off:X}\(\$\w+\)|"
                             rf"\b([ls](?:bu?|hu?|wu?|d|wc1))\s+\$\w+,\s*{off}\(\$\w+\)",
                             self.asm, re.I))
        ops = {(a or b).lower() for a, b in ops} if ops else set()
        if off >= 0 and not ops:
            ops = {o.lower() for o in re.findall(rf"\b([ls](?:bu?|hu?|wu?|d|wc1))\s+\$\w+,\s*0x{off:X}\(", self.asm, re.I)}
        table = {"lb": "signed char", "lbu": "unsigned char", "sb": "unsigned char",
                 "lh": "short", "lhu": "unsigned short", "sh": "unsigned short",
                 "lw": "int", "sw": "int", "lwu": "unsigned int",
                 "lwc1": "float", "swc1": "float", "ld": "long long", "sd": "long long"}
        ts = {table[o] for o in ops if o in table}
        if len(ts) == 1:
            return ts.pop()
        loads = {table[o] for o in ops if o.startswith("l") and o in table}
        if len(loads) == 1 and len({t.split()[-1] for t in ts}) == 1:
            return loads.pop()
        return "int"

    def param_count(self, f: str) -> int | None:
        for src in (self.decls, self.protos.get(f, "")):
            for m in re.finditer(rf"\b{re.escape(f)}\s*\(([^;{{}}()]*(?:\([^()]*\)[^;{{}}()]*)*)\)\s*[;{{]", src):
                params = m.group(1).strip()
                if params in ("", "void"):
                    if params == "void":
                        return 0
                    continue           # unprototyped: any count is fine
                if "..." in params:
                    return None
                return len(split_args(params))
        return None


# --------------------------------------------------------------- helpers --

def split_args(s: str) -> list[str]:
    out, depth, cur, i = [], 0, "", 0
    while i < len(s):
        ch = s[i]
        if ch in "\"'":
            j = i + 1
            while j < len(s) and s[j] != ch:
                j += 2 if s[j] == "\\" else 1
            cur += s[i:j + 1]
            i = j + 1
            continue
        if s.startswith("/*", i):
            j = s.find("*/", i)
            j = len(s) if j < 0 else j + 2
            cur += s[i:j]
            i = j
            continue
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
        i += 1
    if cur.strip() or out:
        out.append(cur)
    return out


def call_span(line: str, f: str, start: int = 0) -> tuple[int, int] | None:
    """(index of '(' , index of matching ')') of the first call of f at/after start."""
    m = re.compile(rf"\b{re.escape(f)}\s*\(").search(line, start)
    if not m:
        return None
    o = m.end() - 1
    depth = 0
    for j in range(o, len(line)):
        if line[j] == "(":
            depth += 1
        elif line[j] == ")":
            depth -= 1
            if depth == 0:
                return o, j
    return None


def base_start(line: str, end: int) -> int:
    """Start index of the postfix expression ending just before `end`."""
    i = end
    while i > 0 and line[i - 1] == " ":
        i -= 1
    while i > 0:
        ch = line[i - 1]
        if ch == '"':
            j = i - 2
            while j >= 0 and not (line[j] == '"' and (j == 0 or line[j - 1] != "\\")):
                j -= 1
            return max(j, 0)
        if ch in ")]":
            close, open_, depth = ch, "(" if ch == ")" else "[", 0
            j = i - 1
            while j >= 0:
                if line[j] == close:
                    depth += 1
                elif line[j] == open_:
                    depth -= 1
                    if depth == 0:
                        break
                j -= 1
            i = j
            if ch == ")":
                # function call: identifier directly before '('
                k = i
                while k > 0 and (line[k - 1].isalnum() or line[k - 1] == "_"):
                    k -= 1
                if k < i:
                    i = k
                else:
                    return i          # parenthesised expression / cast group
        elif ch.isalnum() or ch == "_":
            while i > 0 and (line[i - 1].isalnum() or line[i - 1] == "_"):
                i -= 1
        else:
            return i
        # continue through member access chains
        if line[max(0, i - 2):i] == "->":
            i -= 2
        elif i > 0 and line[i - 1] == "." and not (i > 1 and line[i - 2].isdigit()):
            i -= 1
        elif i > 0 and line[i - 1] == "[":
            return i
        else:
            return i
    return i


def rewrite_member(line: str, member: str, ctx: Ctx) -> str:
    m = UNK_MEMBER.match(member)
    if not m:
        return line
    off = int(m.group(2), 16) * (-1 if m.group(1) else 1)
    t = ctx.width_type(off)
    sign = "-" if off < 0 else "+"
    pat = re.compile(rf"(->|\.)\s*{re.escape(member)}\b")
    for _ in range(20):
        mm = pat.search(line)
        if not mm:
            break
        s = base_start(line, mm.start())
        base = line[s:mm.start()].strip()
        if not base:
            break
        addr = base if mm.group(1) == "->" else f"&{base}"
        if mm.end() < len(line) and line[mm.end():].lstrip().startswith("["):
            new = f"(({t} *)((char *)({addr}) {sign} 0x{abs(off):X}))"
        else:
            new = f"(*({t} *)((char *)({addr}) {sign} 0x{abs(off):X}))"
        line = line[:s] + new + line[mm.end():]
    return line


def preprocess(lines: list[str], ctx: Ctx) -> list[str]:
    heads = [i for i, l in enumerate(lines) if re.match(r"^[A-Za-z_][^;=]*?\b(\w+)\s*\([^;]*\)\s*\{?\s*$", l)
             and not l.startswith("typedef")]
    if len(heads) == 1 and not re.search(rf"\b{re.escape(ctx.name)}\s*\(", lines[heads[0]]):
        lines = list(lines)
        lines[heads[0]] = re.sub(r"\b(\w+)(\s*\([^()]*(?:\([^()]*\)[^()]*)*\)\s*\{?\s*)$",
                                 lambda m: ctx.name + m.group(2), lines[heads[0]], count=1)
    out = []
    for l in lines:
        if not l.lstrip().startswith(("/*", "*")) or "?" in l.split("*/")[-1]:
            # m2c unknown-type placeholder `?` (not the ternary operator)
            l = re.sub(r"^(\s*(?:extern\s+|static\s+)?)\?(?=\s|\*)", r"\1int", l)
            l = re.sub(r"([(,]\s*)\?(?=\s*[*,)])", r"\1int", l)
            l = re.sub(r"\(\s*bitwise\s+", "(", l)
            # m2c sized unknowns ?8/?16/?32/?64 and 128-bit (lq/sq) types
            l = re.sub(r"(?<![\w?])\?(8|16|32|64)\b",
                       lambda m: {"8": "char", "16": "short", "32": "int", "64": "long long"}[m.group(1)], l)
            l = re.sub(r"\b[su]128\b", "long long", l)
            # negative member offsets: p->unk-4
            l = re.sub(r"(->|\.)\s*unk-([0-9A-Fa-f]+)\b", r"\1unkneg\2", l)
            # m2c synthesized struct types named by size
            l = re.sub(r"\bstruct\s+_struct_\w+?_0x([0-9A-Fa-f]+)\b",
                       lambda m: {1: "char", 2: "short", 4: "int", 8: "long long"}.get(
                           int(m.group(1), 16), m.group(0)), l)
        out.append(l)
    # fold14: callee results m2c lost (M2C_ERROR "Read from unset register $f0/$v0")
    t2, notes = m2c_ret_fix("\n".join(out), ctx.protos.get)
    if notes:
        out = t2.split("\n")
    return out




# ---- fold14 (o552 Check_Straight_ID, o555, o556): callee result m2c lost ------

_M2C_RET = re.compile(r"M2C_ERROR\(\s*/\*\s*Read from unset register \$(?P<r>f0|v0)\s*\*/\s*\)")
_NOT_CALL = {"if", "while", "for", "switch", "return", "sizeof", "M2C_ERROR", "do", "else", "int", "float", "void",
             "char", "short", "long", "unsigned", "signed", "double"}


def _ret_cast(f: str, reg: str, proto: str | None) -> str:
    """F called for its $f0 (float) / $v0 (int) result through a
    function-pointer cast `((float (*)(void *, int))F)` (still a direct jal;
    params from PROTO, pointers as void *: same code). Always cast: the TU
    may declare F void even where protos.json has the real type (UnduCheck)."""
    want = "float" if reg == "f0" else "int"
    params = ""
    if proto:
        m = re.match(r"^\s*(?:extern\s+)?(?P<r>.*?)\b" + re.escape(f) + r"\s*\((?P<a>.*)\)\s*;?\s*$", proto)
        if m:
            a = m.group("a").strip()
            if a and a != "void" and "(" not in a:
                ps = []
                for x in a.split(","):
                    x = x.strip()
                    if x == "...":
                        ps = None
                        break
                    ps.append("void *" if "*" in x or "[" in x else x)
                params = ", ".join(ps) if ps is not None else ""
            elif a == "void":
                params = "void"
    return f"(({want} (*)({params})){f})"


def _rmatch_paren(t: str, j: int) -> int:
    """index of the '(' matching the ')' at J, or -1."""
    d = 0
    for i in range(j, -1, -1):
        if t[i] == ")":
            d += 1
        elif t[i] == "(":
            d -= 1
            if d == 0:
                return i
    return -1


def m2c_ret_fix(text: str, proto_of) -> tuple[str, list[str]]:
    """m2c reads a callee's result as `M2C_ERROR(/* Read from unset register
    $f0 */)` (or $v0) when the callee is declared void / the call was
    dropped into a comma expression: draft_fix's stub turns it into 0 and the
    seed silently compares a constant (o552 Check_Straight_ID, o555
    Get_EnemyIDPos, o556 Check_Wall: all rewritten by hand). Rewrites
      `g(a), (M2C_ERROR($v0) != -1)`  ->  `(CAST(g)(a) != -1)`
      `g(a);` + next line reading M2C_ERROR($f0)  ->  `ret_g = CAST(g)(a);`
        and every same-register M2C_ERROR up to the next call -> `ret_g`
    with CAST from _ret_cast (o552's `((float (*)(...))UnduCheck)(...)`).
    PROTO_OF(f) -> prototype text or None. Returns (text, notes)."""
    notes: list[str] = []
    for _ in range(40):                          # (A) comma expressions
        hit = False
        for m in _M2C_RET.finditer(text):
            k = m.start() - 1
            while k >= 0 and text[k] in " \t(":
                k -= 1
            if k < 0 or text[k] != ",":
                continue
            c = k - 1
            while c >= 0 and text[c] in " \t":
                c -= 1
            if c < 0 or text[c] != ")":
                continue
            o = _rmatch_paren(text, c)
            mi = re.search(r"([A-Za-z_]\w*)\s*$", text[:o]) if o > 0 else None
            if not mi or mi.group(1) in _NOT_CALL:
                continue
            f = mi.group(1)
            call = _ret_cast(f, m.group("r"), proto_of(f)) + text[o:c + 1]
            text = text[:mi.start(1)] + text[k + 1:m.start()].lstrip() + call + text[m.end():]
            notes.append(f"callee_ret_cast {f} (comma ${m.group('r')})")
            hit = True
            break
        if not hit:
            break
    lines = text.split("\n")
    call_stmt = re.compile(r"^(?P<ind>\s*)(?P<f>[A-Za-z_]\w*)\s*\((?P<a>.*)\)\s*;\s*$")
    any_call = re.compile(r"(?<![\w.>])([A-Za-z_]\w*)\s*\(")
    decls: list[tuple[int, str]] = []
    mine: dict = {}
    i = 0
    while i < len(lines):
        m = call_stmt.match(lines[i])
        if not m or m.group("f") in _NOT_CALL or not _balanced(m.group("a")):
            i += 1
            continue
        j, e = i + 1, None
        while j < len(lines) and j <= i + 4:     # o555: `var_2 = 0 < 8;` in between
            if lines[j].strip():
                e = _M2C_RET.search(lines[j])
                if e or not re.match(r"^\s*[\w.>\[\]\-]+\s*=[^=;]*;\s*$", lines[j]) or any_call.search(lines[j]):
                    break
            j += 1
        if not e or any(x.group(1) not in _NOT_CALL for x in any_call.finditer(lines[j][:e.start()])):
            i += 1
            continue
        reg, f = e.group("r"), m.group("f")
        head = next((k for k in range(i, -1, -1) if re.match(r"^[A-Za-z_][^;]*\)\s*\{?\s*$", lines[k])), None)
        if head is None:
            i += 1
            continue
        v = mine.get((head, f, reg))
        if v is None:
            v = f"ret_{f}" if reg == "v0" else f"fret_{f}"
            n = 2
            while re.search(rf"\b{v}\b", "\n".join(lines)):
                v = f"ret_{f}_{n}"
                n += 1
            mine[(head, f, reg)] = v
            decls.append((head, f"{'float' if reg == 'f0' else 'int'} {v};"))
        args = m.group("a")
        pa = _proto_arity(proto_of(f), f)
        if pa is not None and "/* extra? */" in args:
            sp = _top_split(args)
            while len(sp) > pa and "/* extra? */" in sp[-1]:
                sp.pop()
            args = ", ".join(x.strip() for x in sp)
        lines[i] = f"{m.group('ind')}{v} = {_ret_cast(f, reg, proto_of(f))}({args});"
        for q in range(j, len(lines)):
            if q > j and any(x.group(1) not in _NOT_CALL for x in any_call.finditer(_M2C_RET.sub("", lines[q]))):
                break
            lines[q] = re.sub(r"M2C_ERROR\(\s*/\*\s*Read from unset register \$" + reg + r"\s*\*/\s*\)", v, lines[q])
        notes.append(f"callee_ret_cast {f} (stmt ${reg} -> {v})")
        i = j + 1
    for head, d in sorted(decls, reverse=True):
        at = head + 1 if lines[head].rstrip().endswith("{") else head + 2
        lines.insert(at, "    " + d)
    return "\n".join(lines), notes


def _balanced(s: str) -> bool:
    d = 0
    for ch in s:
        d += ch == "("
        d -= ch == ")"
        if d < 0:
            return False
    return d == 0


def _top_split(s: str) -> list[str]:
    out, d, cur = [], 0, ""
    for ch in s:
        if ch in "([{":
            d += 1
        elif ch in ")]}":
            d -= 1
        if ch == "," and d == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    out.append(cur)
    return out


def _proto_arity(proto: str | None, f: str) -> int | None:
    m = re.search(re.escape(f) + r"\s*\((?P<a>.*)\)\s*;?\s*$", proto or "")
    if not m or "..." in m.group("a") or "(" in m.group("a"):
        return None
    a = m.group("a").strip()
    return 0 if a in ("", "void") else len(_top_split(a))


def prelude_end(lines: list[str]) -> int:
    """Index after the leading comment / draft_fix prelude lines."""
    i = 0
    while i < len(lines) and (lines[i].startswith("/* m2c draft") or TAG in lines[i]
                              or lines[i].startswith(("typedef ", "#define M2C_", "#define NULL"))):
        i += 1
    return i


def insert_prelude(lines: list[str], new: str) -> None:
    if any(l.strip() == new.strip() or l.strip() == f"{new} {TAG}".strip() for l in lines):
        return
    lines.insert(prelude_end(lines), f"{new} {TAG}")


def func_open(lines: list[str], idx: int) -> int | None:
    """Line index of the `{` opening the function containing line idx."""
    for i in range(min(idx, len(lines) - 1), -1, -1):
        if re.match(r"^[A-Za-z_].*\)\s*\{\s*$", lines[i]) or (lines[i].strip() == "{" and i and
                                                               re.match(r"^[A-Za-z_].*\)\s*$", lines[i - 1])):
            return i
    return None


def comment_out(line: str) -> str:
    return f"/* {line.replace('*/', '* /')} */ {TAG}"


def comment_item(lines: list[str], idx: int) -> None:
    """Comment out the top-level declaration starting at idx (may span lines)."""
    depth, k = 0, idx
    while k < len(lines):
        code = re.sub(r"/\*.*?\*/|\"(?:\\.|[^\"])*\"|'(?:\\.|[^'])*'", "", lines[k])
        depth += code.count("{") - code.count("}")
        if depth <= 0 and code.rstrip().endswith(";"):
            break
        k += 1
    for j in range(idx, min(k, len(lines) - 1) + 1):
        lines[j] = comment_out(lines[j])


def is_toplevel_decl(lines: list[str], idx: int) -> bool:
    l = lines[idx]
    return bool(l) and not l[0].isspace() and l.rstrip().endswith(";") and "{" not in l


# ---------------------------------------------------------------- fixing --

def fix_irregular_case(lines: list[str], idx: int) -> bool:
    """m2c 'irregular' switches put `case X: /* switch N */` labels outside switch
    N's braces, where they land in an enclosing switch (duplicate case value).
    Turn the label into a goto target and dispatch to it from switch N."""
    m = re.match(r"^(\s*)(case\s+([^:]+)|default)\s*:\s*/\*\s*switch\s+(\d+)\b.*\*/\s*$", lines[idx])
    if not m:
        return False
    n = m.group(4)
    head = next((k for k, l in enumerate(lines) if re.search(rf"\bswitch\s*\(.*\{{\s*/\*\s*switch\s+{n}\b", l)), None)
    if head is None:
        return False
    depth, end = 0, None
    for k in range(head, len(lines)):
        code = re.sub(r"/\*.*?\*/|\"(?:\\.|[^\"])*\"|'(?:\\.|[^'])*'", "", lines[k])
        depth += code.count("{") - code.count("}")
        if depth == 0:
            end = k
            break
    if end is None or head < idx <= end:
        return False
    val = m.group(3).strip() if m.group(3) else None
    lab = f"m2c_sw{n}_" + (re.sub(r"\W", "_", val) if val else "default")
    lines[idx] = f"{m.group(1)}{lab}: {TAG}"
    disp = f"case {val}" if val else "default"
    lines.insert(head + 1, f"{m.group(1)}{disp}: goto {lab}; {TAG}")
    return True


def used_as_type(t: str, lines: list[str]) -> bool:
    if re.match(r"^(temp|var|saved_reg|sp|arg|phi|unksp|D_|func_|jtbl_)", t):
        return False
    text = "\n".join(l for l in lines if TAG not in l)
    return bool(re.search(rf"^\s*(?:static\s+|extern\s+|const\s+)*{t}\s*\*+\s*\w+\s*[;,)\[=]|"
                          rf"\(\s*{t}\s*\*+\s*\)|[(,]\s*{t}\s*\*+\s*\w+\s*[,)]|^\s*{t}\s+\w+\s*;", text, re.M))


def fix_type_name(t: str, lines: list[str], ctx: Ctx, pending: list[str], notes: list[str]) -> bool:
    """A type the TU defines only after this function (or not at all)."""
    imported = import_type(t, ctx)
    if imported:
        alias = f"{t}__df"
        for i, l in enumerate(lines):
            if not (TAG in l and l.lstrip().startswith("/*")):
                lines[i] = re.sub(rf"\b{t}\b", alias, l)
        pending.append(imported)
        notes.append(f"type {t} copied from {ctx.type_src} as {alias}")
        return True
    if re.search(rf"\b{t}\b", ctx.decls):
        changed = False
        for i, l in enumerate(lines):
            if TAG in l and l.lstrip().startswith("/*"):
                continue
            n = re.sub(rf"\b{t}(\s*\*)", r"void\1", l)
            if n != l:
                lines[i], changed = n, True
        if changed:
            notes.append(f"type {t} (defined later in TU) -> void *")
        return changed
    pending.append(f"typedef int {t};")
    notes.append(f"unknown type {t} -> int")
    return True


def import_type(t: str, ctx: Ctx) -> str | None:
    """`typedef struct {...} T;` from the TU (defined after the function) or any
    other source/header, renamed T__df, as one prelude line."""
    cands = [ROOT / "src" / ctx.unit / f"{ctx.tu}.c"]
    try:
        import subprocess
        out = subprocess.run(["grep", "-rlE", rf"}}\s*{t}\s*;", str(ROOT / "include"), str(ROOT / "src")],
                             capture_output=True, text=True, timeout=20).stdout.split()
        cands += [Path(x) for x in out if x.endswith((".h", ".c"))]
    except Exception:  # noqa: BLE001
        pass
    for f in cands:
        try:
            txt = f.read_text(errors="replace")
        except OSError:
            continue
        m = re.search(rf"}}\s*{t}\s*;", txt)
        if not m:
            continue
        depth, j = 0, m.start()
        while j >= 0:
            depth += (txt[j] == "}") - (txt[j] == "{")
            if depth == 0:
                break
            j -= 1
        k = txt.rfind("typedef", 0, j)
        if k < 0 or ";" in re.sub(r"/\*.*?\*/", "", txt[k:j], flags=re.S):
            continue
        d = txt[k:m.end()]
        d = re.sub(r"/\*.*?\*/|//[^\n]*", "", d, flags=re.S)
        d = re.sub(r"\b(struct|union)\s+\w+\s*\{", r"\1 {", d, count=1)   # drop the tag
        d = re.sub(rf"\b{t}\s*;\s*$", f"{t}__df;", d)
        ctx.type_src = str(f.relative_to(ROOT))
        return " ".join(d.split())
    return None


def first_func(lines: list[str]) -> int:
    for i, l in enumerate(lines):
        if re.match(r"^[A-Za-z_][^;=]*\)\s*\{?\s*$", l) and "(" in l and not l.startswith("typedef"):
            return i
    return len(lines)


def match_head(line: str, ctx: Ctx) -> str | None:
    """Re-type the definition head `RET NAME(T a, T b) {` from the TU's prototype."""
    m = re.search(rf"(?:^|[;}}\n])\s*((?:extern\s+|static\s+)?[^;{{}}()]*?)\b{re.escape(ctx.name)}\s*\(([^;{{}}]*?)\)\s*;",
                  ctx.decls)
    h = re.match(rf"^(.*?)\b{re.escape(ctx.name)}\s*\((.*)\)(\s*\{{?\s*)$", line)
    if not m or not h:
        return None
    ret = re.sub(r"\bextern\s+", "", m.group(1)).strip()
    pt = [a.strip() for a in split_args(m.group(2))] if m.group(2).strip() not in ("", "void") else []
    names = [re.findall(r"\w+", a)[-1] for a in split_args(h.group(2))
             if a.strip() not in ("", "void") and re.findall(r"\w+", a)]
    if len(pt) != len(names):
        return None
    params = []
    for t, n in zip(pt, names):
        t = re.sub(r"\b\w+\s*$", "", t).strip() if len(re.findall(r"\w+", t)) > 1 and not t.endswith("*") \
            and t.split()[-1] not in ("int", "char", "short", "long", "float", "double", "unsigned", "signed") else t
        params.append(f"{t} {n}".replace("* ", "*") if "*" in t else f"{t} {n}")
    return f"{ret} {ctx.name}({', '.join(params) or 'void'}){h.group(3)}"


def cast_derefs(line: str, lines: list[str], idx: int) -> str:
    """`*x` where x is not a pointer -> `*(int *)x` (also `*f(...)`)."""
    def is_ptr(name: str) -> bool:
        return any(re.search(rf"\*\s*{name}\s*[;\[,)=]|\b{name}\s*\[", l) for l in lines[:idx]
                   if re.match(r"^\s*[\w ]+\*", l))
    def sub(m):
        name = m.group(2)
        if is_ptr(name):
            return m.group(0)
        return f"{m.group(1)}*(int *){name}"
    new = re.sub(r"((?:^|[^\w)\]\s])\s*)\*\s*([A-Za-z_]\w*)\b(?!\s*\))", sub, line)
    new = re.sub(r"(\((?:const\s+|unsigned\s+|signed\s+|struct\s+)*[A-Za-z_]\w*(?:\s+\w+)?\s*\**\)\s*)\*\s*([A-Za-z_]\w*)\b(?!\s*\()",
                 sub, new)
    if new == line:   # deref of a non-pointer expression in parentheses
        new = re.sub(r"((?:^|[^\w)\]\s]|\((?:const\s+|unsigned\s+|signed\s+)*[A-Za-z_]\w*(?:\s+\w+)?\s*\**\))\s*)\*\s*\((?!\s*(?:const\s+|unsigned\s+|signed\s+)*[A-Za-z_]\w*\s*\*+\s*\))",
                     r"\1*(int *)(", line, count=1)
    return new


def item_head(lines: list[str], idx: int) -> int:
    """First line of the top-level item containing idx (before the first function)."""
    if idx < first_func(lines):
        while idx > 0 and (lines[idx][:1].isspace() or lines[idx].startswith("}")):
            idx -= 1
    return idx


def func_end(lines: list[str], fo: int) -> int:
    depth = 0
    for k in range(fo, len(lines)):
        code = re.sub(r"/\*.*?\*/|\"(?:\\.|[^\"])*\"|'(?:\\.|[^'])*'", "", lines[k])
        depth += code.count("{") - code.count("}")
        if depth == 0 and "{" in "".join(lines[fo:k + 1]):
            return k
    return len(lines) - 1


def stack_words(lines: list[str], fo: int, v: str) -> int:
    """Element count for stack temp `spXX` used as an array: gap to the next
    stack slot m2c named in this function, in words (default 16, a matrix)."""
    m = re.match(r"^sp([0-9A-Fa-f]*)$", v)
    if not m:
        return 16
    off = int(m.group(1) or "0", 16)
    text = "\n".join(lines[fo:func_end(lines, fo) + 1])
    offs = {int(o, 16) for o in re.findall(r"\bsp([0-9A-Fa-f]+)\b", text)}
    nxt = [o for o in offs if o > off]
    if not nxt:
        return 16
    return max(1, min(64, (min(nxt) - off) // 4))


def scalar_local(lines: list[str], fo: int, idx: int, v: str) -> tuple[int, str] | None:
    """(line, type) of a plain non-pointer, non-array local declaration of v."""
    for k in range(fo + 1, min(idx, len(lines))):
        l = lines[k]
        if l.lstrip().startswith("/*"):
            continue
        m = re.match(rf"^\s+((?:unsigned\s+|signed\s+|const\s+|volatile\s+)*[A-Za-z_]\w*(?:\s+(?:int|char|long))?)\s+{re.escape(v)}\s*;", l)
        if m and m.group(1).split()[-1] not in ("return", "goto", "case"):
            return k, m.group(1)
    return None


def later_tu_decl(ctx: Ctx, v: str) -> str | None:
    """`extern T v;` as the TU itself declares v (after this function)."""
    m = re.search(rf"^[ \t]*extern\s+((?:const\s+|volatile\s+|unsigned\s+|signed\s+|struct\s+|union\s+)*[A-Za-z_]\w*)"
                  rf"\s*(\**)\s*\b{re.escape(v)}\s*((?:\[[^\]]*\])*)\s*;", ctx.tu_src, re.M)
    if not m:
        return None
    return f"extern {m.group(1)} {m.group(2)}{v}{m.group(3)};"


def restore_draft_decl(lines: list[str], v: str) -> bool:
    """Un-comment a one-line draft declaration of v dropped earlier (typically
    for a type that has since been imported as T__df)."""
    aliases = set(re.findall(r"\b(\w+)__df\b", "\n".join(lines)))
    for i, l in enumerate(lines):
        m = re.match(r"^/\* (.*) \*/ " + re.escape(TAG) + r"$", l)
        if not m or TAG in m.group(1):
            continue
        inner = m.group(1).replace("* /", "*/")
        if not re.search(rf"\b{re.escape(v)}\b", inner) or not inner.rstrip().endswith(";") \
                or "{" in inner or inner[:1].isspace() or "(" in inner:
            continue
        for t in aliases:
            inner = re.sub(rf"\b{t}\b(?!__df)", f"{t}__df", inner)
        del lines[i]                      # re-insert after the prelude (imported types)
        lines.insert(prelude_end(lines), inner)
        return True
    return False


def retype_global(lines: list[str], v: str, idx: int | None, ctx: Ctx, notes: list[str]) -> bool:
    """Give global v a real type instead of our size-guessed extern: the
    draft's own (dropped) declaration, else the TU's later declaration as a
    block-scope extern. One shot per symbol and kind."""
    ours = [k for k, l in enumerate(lines) if TAG in l and re.match(rf"^\s*extern\s+(?:int|char|short|long long)\s+{re.escape(v)}\b", l)]
    if ("restore", v) not in ctx.tried:
        ctx.tried.add(("restore", v))
        n = len(lines)
        for k in ours:
            lines[k] = comment_out(lines[k])
        if restore_draft_decl(lines, v):
            notes.append(f"restored draft decl of {v}")
            return True
        for k in ours:
            lines[k] = lines[k][3:].split(" */ " + TAG)[0].replace("* /", "*/")
        assert len(lines) == n
    d = later_tu_decl(ctx, v)
    if d and idx is not None and ("bs", v) not in ctx.tried:
        fo = func_open(lines, idx)
        if fo is not None and fo < idx:
            ctx.tried.add(("bs", v))
            for k in ours:
                lines[k] = comment_out(lines[k])
            lines.insert(fo + 1, f"    {d} {TAG}")
            notes.append(f"block-scope {d}")
            return True
    return False


def apply_fixes(lines: list[str], errs: list, warns: list, ctx: Ctx, notes: list[str]) -> bool:
    changed = False
    done_lines = set()
    pending: list[str] = []      # prelude lines, inserted last (keeps error indices valid)
    try:
        return _apply(lines, errs, warns, ctx, notes, pending, done_lines)
    finally:
        for p in pending:
            insert_prelude(lines, p)


def _apply(lines, errs, warns, ctx, notes, pending, done_lines) -> bool:
    changed = False

    def note(s):
        notes.append(s)

    # missing basic typedefs / m2c macros
    text = "\n".join(lines)
    for t, real in BASIC_TYPES.items():
        if re.search(rf"\b{t}\b", text) and not ctx.has_typedef(t) and \
                not re.search(rf"typedef[^;]*\b{t}\s*;", text):
            pending.append(f"typedef {real} {t};")
            note(f"typedef {t}")
            changed = True
    text = "\n".join(lines)
    for mname, d in M2C_MACROS.items():
        if re.search(rf"\b{mname}\s*\(", text) and d not in text:
            pending.append(d)
            changed = True

    # top-level items with an undeclared name in them: fix the name, keep the item
    protect = {item_head(lines, i) for i, mm in errs
               if i is not None and i < len(lines) and "undeclared here" in mm}

    for idx, msg in errs + warns:
        mt = re.search(r"parse error before [\"`](\w+)[\"']", msg)
        if idx is None and mt:
            idx = next((i for i, l in enumerate(lines) if "__df;" in l and re.search(rf"\b{mt.group(1)}\b", l)), None)
        m = re.search(r"`(\w+)' undeclared", msg)
        if m:
            v = m.group(1)
            if v in BASIC_TYPES:
                continue
            if used_as_type(v, lines):
                if fix_type_name(v, lines, ctx, pending, notes):
                    return True
                continue
            if v == "NULL":
                pending.append("#define NULL 0")
                changed = True
                continue
            if re.match(r"^(temp|var|saved_reg|sp|arg|phi|unksp)\w*$", v) and v not in ctx.sizes:
                if idx is not None and ("u", v) not in done_lines:
                    fo = func_open(lines, idx)
                    if fo is not None and any(re.search(rf"^\s+[A-Za-z_][\w ]*[\s*]+{v}\s*(;|\[|=|,)", l)
                                              for l in lines[fo + 1:fo + 400] if TAG not in l):
                        continue          # declared already; the error is a cascade
                    hdr = "" if fo is None else (lines[fo] if "(" in lines[fo] else (lines[fo - 1] if fo else ""))
                    if hdr and re.search(rf"\(.*\b{v}\b.*\)", hdr):
                        # fold12 (o529 ACT_loadResource): a parameter, undeclared only
                        # because its type failed to parse (s32 before the typedef):
                        # a local `int arg1;` would shadow it (the body reads garbage)
                        continue
                    if fo is not None and f"    int {v}; {TAG}" not in lines[fo + 1:fo + 200]:
                        body_txt = "\n".join(lines[fo:func_end(lines, fo) + 1])
                        if re.search(rf"\b{v}\s*\[", body_txt) and not re.search(
                                rf"(?<![&\w])\b{v}\b(?!\s*\[)", body_txt):
                            lines.insert(fo + 1, f"    int {v}[{stack_words(lines, fo, v)}]; {TAG}")
                        else:
                            lines.insert(fo + 1, f"    int {v}; {TAG}")
                        done_lines.add(("u", v))
                        note(f"local {v}")
                        return True       # indices shifted: recompile
                continue
            if v in ctx.funcs or (v in ctx.protos and v not in ctx.sizes):
                if ("p", v) not in done_lines:
                    pending.append(ctx.protos.get(v) or f"int {v}();")
                    done_lines.add(("p", v))
                    note(f"proto {v} (function used as a value)")
                    changed = True
                continue
            if retype_global(lines, v, idx, ctx, notes):
                return True
            size = ctx.sizes.get(v)
            decl = {1: f"extern char {v};", 2: f"extern short {v};", 4: f"extern int {v};",
                    8: f"extern long long {v};"}.get(size, f"extern char {v}[{size}];" if size else f"extern int {v};")
            if ("g", v) not in done_lines:
                pending.append(decl)
                done_lines.add(("g", v))
                note(f"global {decl}")
                changed = True
            continue
        m = re.search(r"implicit declaration of function `(\w+)'", msg)
        if m:
            f = m.group(1)
            if f in ctx.protos and ("p", f) not in done_lines:
                pending.append(ctx.protos[f])
                done_lines.add(("p", f))
                note(f"proto {f}")
                changed = True
            continue
        mc = re.search(r"conflicting types for `(\w+)'|`(\w+)' redeclared as different kind of symbol|"
                       r"previous declaration of `(\w+)'", msg)
        if mc and (idx is None or idx >= len(lines) or TAG in lines[idx]):
            sym = mc.group(1) or mc.group(2) or mc.group(3)
            k = next((k for k, l in enumerate(lines) if TAG in l and l.startswith("extern ")
                      and re.search(rf"\b{sym}\b", l)), None)
            if k is not None and ("mv", sym) not in ctx.tried:
                # our file-scope extern clashes with a later TU declaration:
                # a block-scope extern in each function using it does not
                ctx.tried.add(("mv", sym))
                decl = lines[k].replace(TAG, "").strip()
                lines[k] = comment_out(lines[k])
                ff = first_func(lines)
                opens = []
                for j in range(ff, len(lines)):
                    if TAG not in lines[j] and re.search(rf"\b{sym}\b", lines[j]):
                        fo = func_open(lines, j)
                        if fo is not None and fo not in opens:
                            opens.append(fo)
                for fo in sorted(opens, reverse=True):
                    lines.insert(fo + 1, f"    {decl} {TAG}")
                note(f"block-scope {decl}")
                return True
        if msg.startswith("warning:") or idx is None or idx >= len(lines):
            continue
        line = lines[idx]
        # errors on our own prelude lines: downgrade / drop
        if TAG in line and line.startswith("    int ") and ("conflicting" in msg or "redeclar" in msg):
            lines[idx] = comment_out(line.strip())
            return True
        mt = re.search(r"parse error before [\"`](\w+)[\"']", msg)
        if TAG in line and "__df;" in line and mt and ("t", mt.group(1)) not in done_lines:
            done_lines.add(("t", mt.group(1)))
            sub = import_type(mt.group(1), ctx)
            if sub:
                t = mt.group(1)
                for i, l in enumerate(lines):
                    if not (TAG in l and l.lstrip().startswith("/*")):
                        lines[i] = re.sub(rf"\b{t}\b", f"{t}__df", l)
                lines.insert(idx, f"{sub} {TAG}")
                note(f"type {t} copied as {t}__df")
                return True
        me = re.match(r"^\s+extern\s+(?:struct\s+|union\s+)?([A-Za-z_]\w*)\s*\**\s*\w+\s*(?:\[[^\]]*\])*\s*;", line)
        if TAG in line and me and ("parse error" in msg or "syntax error" in msg) \
                and ("ty", me.group(1)) not in ctx.tried:
            # our block-scope extern names a type the TU defines only later
            ctx.tried.add(("ty", me.group(1)))
            if fix_type_name(me.group(1), lines, ctx, pending, notes):
                return True
        if TAG in line and not line.lstrip().startswith("/*") and not line.lstrip().startswith("int "):
            mm = re.search(r"\b(\w+)\s*\(", line)
            if mm and "extern int" not in line and "#define" not in line and "typedef" not in line:
                lines[idx] = f"extern int {mm.group(1)}(); {TAG}"
            else:
                lines[idx] = comment_out(line)
            note(f"dropped prelude: {line.strip()[:60]}")
            return True
        m = re.search(r"(?:redefinition|redeclaration|conflicting types) (?:of|for) `(\w+)'|"
                      r"static declaration for `(\w+)' follows|"
                      r"previous (?:declaration|definition) of `(\w+)'", msg)
        if m and idx < first_func(lines) and (line[:1].isspace() or line.startswith("}")):
            k = idx
            while k > 0 and (lines[k][:1].isspace() or lines[k].startswith("}")):
                k -= 1
            idx, line = k, lines[k]
        if m and is_toplevel_decl(lines, idx) and TAG not in line:
            comment_item(lines, idx)
            note(f"dup decl {m.group(1) or m.group(2) or m.group(3)}")
            changed = True
            continue
        if m and TAG not in line and re.search(r"=\s*[^=]", line) and not line[0].isspace():
            # duplicate global with initialiser (may not end in ';' on one line)
            comment_item(lines, idx)
            return True
        if m and m.group(1) == ctx.name and TAG not in line and not line[0].isspace() and "(" in line:
            new = match_head(line, ctx)
            if new and new != line:
                lines[idx] = new
                note("function head from TU prototype")
                changed = True
            continue
        if msg.startswith("parse error") and idx < first_func(lines) and TAG not in line \
                and not line[0].isspace() and not line.startswith(("/*", "#")) and idx not in protect:
            comment_item(lines, idx)       # draft data/extern decl with an unknown type
            note(f"dropped draft decl: {line.strip()[:50]}")
            return True
        m = re.search(r"invalid operands to binary (\S+)", msg)
        if m:
            op = re.escape(m.group(1))
            new = re.sub(rf"(?<![\w)])([A-Za-z_]\w*)\s*{op}\s*([A-Za-z_]\w*)\b(?!\s*[(\[])",
                         rf"(int)\1 {m.group(1)} (int)\2", line, count=1)
            if new != line:
                lines[idx] = new
                note("int-cast binary operands")
                changed = True
            continue
        m = re.search(r"parse error before [\"`](\w+)[\"']", msg)
        if m and m.group(1) not in BASIC_TYPES and used_as_type(m.group(1), lines) \
                and re.search(rf"\b{m.group(1)}\b", ctx.decls):
            if fix_type_name(m.group(1), lines, ctx, pending, notes):
                return True
        if m and m.group(1) not in BASIC_TYPES and not re.search(rf"\b{m.group(1)}\b", ctx.decls) \
                and not re.match(r"^(D_|func_|jtbl_|sp|temp_|var_|arg)", m.group(1)) \
                and not re.search(rf"\b{m.group(1)}\s*[;=\[]", "\n".join(lines)) \
                and re.search(rf"\b{m.group(1)}\s*\*|\(\s*{m.group(1)}\s*\)|^\s*{m.group(1)}\s+\w", "\n".join(lines), re.M) \
                and m.group(1) != ctx.name and m.group(1)[0].isupper():
            pending.append(f"typedef int {m.group(1)};")
            note(f"unknown type {m.group(1)} -> int")
            changed = True
            continue
        if idx < first_func(lines) and (line[:1].isspace() or line.startswith("}")):
            k = idx
            while k > 0 and (lines[k][:1].isspace() or lines[k].startswith("}")):
                k -= 1
            idx, line = k, lines[k]
        if idx < first_func(lines) and TAG not in line and line and not line[0].isspace() \
                and idx not in protect \
                and not line.startswith(("/*", "#")) and ("error" in msg or "not constant" in msg or "parse" in msg
                                                          or "initializ" in msg or "storage size" in msg):
            comment_item(lines, idx)       # draft data definition the TU cannot take as is
            note(f"dropped draft decl: {line.strip()[:50]}")
            return True
        if "subscripted value is neither array nor pointer" in msg:
            fo = func_open(lines, idx)
            if fo is None:
                continue
            for v in dict.fromkeys(re.findall(r"(?<![\w.>])([A-Za-z_]\w*)\s*\[", line)):
                sl = scalar_local(lines, fo, idx, v)
                if sl is None:
                    continue
                k, t = sl
                body_txt = "\n".join(lines[fo:func_end(lines, fo) + 1])
                if TAG in lines[k] and not re.search(rf"(?<![&\w])\b{v}\b(?!\s*\[)", body_txt):
                    lines[k] = f"    {t} {v}[{stack_words(lines, fo, v)}]; {TAG}"
                    note(f"stack {v} -> array")
                else:
                    lines[idx] = line = re.sub(rf"(?<![\w.>])\b{v}\s*\[", f"(({t} *)&{v})[", line)
                    note(f"scalar {v} indexed via cast")
                changed = True
            continue
        if "called object is not a function" in msg:
            new = re.sub(r"(?<![\w)\]])([A-Za-z_]\w*(?:\s*(?:->|\.)\s*\w+|\s*\[[^\[\]]*\])+)\s*\(",
                         r"((int (*)())\1)(", line, count=1)
            if new != line:
                lines[idx] = new
                note("call through function-pointer cast")
                changed = True
            continue
        if "invalid type argument of `unary *'" in msg:
            new = cast_derefs(line, lines, idx)
            if new != line:
                lines[idx] = new
                note("cast deref")
                changed = True
            continue
        m = re.search(r"(?:has no member named|request for member) `(\w+)'", msg)
        bases = re.findall(r"(?<![\w.>])([A-Za-z_]\w*)\s*->", line) if "`->'" in msg else \
            (re.findall(rf"(?<![\w.>])([A-Za-z_]\w*)\s*\.\s*{m.group(1)}\b", line) if m and "request for" in msg else [])
        for v in bases:
            # a global we declared from its size only: give it its real type
            if any(TAG in l and re.match(rf"^\s*extern\s+(?:int|char|short|long long)\s+{re.escape(v)}\b", l) for l in lines) \
                    and retype_global(lines, v, idx, ctx, notes):
                return True
        if m:
            new = rewrite_member(line, m.group(1), ctx)
            if new != line:
                lines[idx] = new
                note(f"member {m.group(1)}")
                changed = True
            continue
        if "invalid type argument of `->'" in msg or "dereferencing pointer to incomplete type" in msg:
            for mem in re.findall(r"->\s*(\w+)", line):
                new = rewrite_member(line, mem, ctx)
                if new != line:
                    lines[idx] = line = new
                    changed = True
            continue
        m = re.search(r"too (many|few) arguments to function(?: `(\w+)')?", msg)
        if m:
            names = [m.group(2)] if m.group(2) else re.findall(r"\b([A-Za-z_]\w*)\s*\(", line)
            for f in names:
                if f in ("if", "while", "switch", "for", "return", "sizeof"):
                    continue
                hdr = next((k for k, l in enumerate(lines) if k < idx and l and not l[0].isspace()
                            and TAG not in l and re.search(rf"\b{re.escape(f)}\s*\([^()]*(\([^()]*\)[^()]*)*\)\s*;", l)), None)
                if hdr is not None and not re.search(rf"\b{re.escape(f)}\s*\(", ctx.decls):
                    # the draft's own guessed prototype: make it unprototyped
                    lines[hdr] = re.sub(rf"(\b{re.escape(f)})\s*\(.*\)\s*;", r"\1();", lines[hdr], count=1) + f" {TAG}"
                    note(f"unprototyped {f}")
                    changed = True
                    break
                want = ctx.param_count(f)
                sp = call_span(line, f)
                if sp is None:
                    continue
                args = split_args(line[sp[0] + 1:sp[1]])
                if m.group(1) == "many":
                    kept = [a for a in args if "/* extra? */" not in a]
                    if want is not None and len(kept) > want:
                        kept = kept[:want]
                    if want is None and len(kept) == len(args):
                        continue
                else:
                    if want is None or len(args) >= want:
                        continue
                    kept = args + [" 0"] * (want - len(args))
                    if not args or not "".join(args).strip():
                        kept = ["0"] + [" 0"] * (want - 1)
                if kept != args:
                    lines[idx] = line = line[:sp[0] + 1] + ",".join(kept).strip() + line[sp[1]:]
                    note(f"argc {f} {len(args)}->{len(kept)}")
                    changed = True
                    if m.group(2):
                        break
            continue
        if ("dup" in msg and "case value" in msg) or "case label not within a switch" in msg:
            if fix_irregular_case(lines, idx):
                note("irregular switch case -> goto")
                return True
            continue
        m = re.search(r"parse error before `(\w+)'", msg)
        if m and m.group(1) in BASIC_TYPES:
            pending.append(f"typedef {BASIC_TYPES[m.group(1)]} {m.group(1)};")
            changed = True
            continue
    return changed


def fix(unit: str, tu: str, name: str, text: str, max_iter: int = 30, deadline: float | None = None,
        log=lambda s: None) -> dict:
    ctx = Ctx(unit, tu, name)
    lines = preprocess(text.splitlines(), ctx)
    notes: list[str] = []
    errs0 = None
    res = {"ok": False}
    hist: list[int] = []
    it = -1
    while True:
        it += 1
        if it >= max_iter:
            # past the cap: keep going (up to 4x) only while the error count still falls
            if it >= 4 * max_iter or len(hist) < 6 or min(hist[-5:]) >= min(hist[:-5]):
                break
        ok, clog, cand = compile_body(unit, tu, name, "\n".join(lines) + "\n")
        errs, warns = parse_errors(clog, lines, cand) if not ok else ([], [])
        if errs0 is None:
            errs0 = len(errs)
        hist.append(len(errs))
        log(f"iter {it}: ok={ok} errors={len(errs)}")
        res = {"ok": ok, "iterations": it + 1, "errors": len(errs), "initial_errors": errs0,
               "remaining": [(None if i is None else i + 1, m) for i, m in errs[:15]]}
        if ok:
            # still worth injecting prototypes for implicitly declared callees
            pw = [w for w in warns if "implicit declaration" in w[1]]
            if pw and it + 1 < 4 * max_iter:
                trial = list(lines)
                if apply_fixes(trial, [], pw, ctx, notes):
                    ok2, _, _ = compile_body(unit, tu, name, "\n".join(trial) + "\n")
                    if ok2:
                        lines = trial
            break
        if deadline and time.time() > deadline:
            res["timeout"] = True
            break
        if cand is None or not apply_fixes(lines, errs, warns, ctx, notes):
            break
    res["text"] = "\n".join(lines) + "\n"
    assert not HAS_ASM.search(res["text"]), "draft_fix must never emit inline asm"
    res["fixes"] = notes
    return res


# ------------------------------------------------------------------ main --

def track_best(body: Path, text: str, report: dict) -> bool:
    score = report.get("score")
    if score is None or ".best" in body.name:
        return False
    stem = body.stem
    side = body.with_name(stem + ".best.json")
    try:
        prev = json.loads(side.read_text()).get("score", -1)
    except (OSError, ValueError):
        prev = -1
    if score <= prev - try_fn.SUSPECT_MARGIN:
        return False
    sus = list(report.get("suspect") or [])
    k3 = (report.get("key") or "").split("/")
    fixed = body.with_name(stem + ".fixed.c")      # the file run() scored
    if len(k3) == 3 and fixed.is_file() and fixed.read_text() == text:
        try:                     # fold11: suspect-aware pick (uninit check, SUSPECT_MARGIN)
            ok, sus = try_fn.prefer_over_best(*k3, fixed, score, report, side)
        except (try_fn.TryError, OSError):
            ok = score > prev
    else:
        ok = score > prev
    if not ok:
        return False
    body.with_name(stem + ".best.c").write_text(text)
    side.write_text(json.dumps(try_fn.best_record(score, report, sus)) + "\n")
    return True


def run(unit: str, tu: str, name: str, body: Path, write: bool = False, max_iter: int = 30,
        timeout: float | None = None, verbose: bool = False) -> dict:
    deadline = time.time() + timeout if timeout else None
    text = body.read_text()
    out = {"key": f"{unit}/{tu}/{name}", "body": str(body)}
    if HAS_ASM.search(text):
        out.update(ok=False, skipped="draft contains inline asm (not allowed)")
        return out
    ok0, _, _ = compile_body(unit, tu, name, text)
    out["body_compiled"] = ok0
    r = fix(unit, tu, name, text, max_iter, deadline,
            log=(lambda s: print(s, file=sys.stderr)) if verbose else (lambda s: None))
    fixed = body.with_name(body.stem + ".fixed.c")
    fixed.write_text(r["text"])
    out.update({k: v for k, v in r.items() if k != "text"}, fixed=str(fixed))
    if r["ok"]:
        try:
            rep = try_fn.try_function(unit, tu, name, fixed)
            out["score"] = rep.get("score")
            out["match"] = rep.get("match")
            if rep.get("score") is not None:
                out["best_updated"] = track_best(body, r["text"], rep)
        except Exception as e:  # noqa: BLE001
            out["score_error"] = str(e)[-300:]
    if write and not ok0 and (r["ok"] or r["errors"] < (r["initial_errors"] or 0)):
        if body.read_text() == text:      # nobody edited it meanwhile
            body.write_text(r["text"])
            out["body_written"] = True
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", required=True)
    ap.add_argument("--tu", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--body", type=Path)
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--max-iter", type=int, default=30)
    ap.add_argument("--timeout", type=float, default=None, help="wall-clock seconds")
    ap.add_argument("-v", "--verbose", action="store_true")
    a = ap.parse_args()
    body = (ROOT / (a.body or f"build/auto_decomp/bodies/{a.unit}/{a.tu}/{a.name}.c")).resolve()
    if not body.is_file():
        print(json.dumps({"ok": False, "error": f"missing {body}"}))
        return 1
    try:
        res = run(a.unit, a.tu, a.name, body, a.write, a.max_iter, a.timeout, a.verbose)
    except try_fn.TryError as e:
        print(json.dumps({"ok": False, "error": str(e)}))
        return 1
    print(json.dumps(res, indent=1))
    return 0 if res["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())

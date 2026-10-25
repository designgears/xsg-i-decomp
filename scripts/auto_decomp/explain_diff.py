#!/usr/bin/env python3
"""Explain WHY a near-miss body differs from the target: scheduler, register
allocator and delay-slot decisions, read from cc1's own dumps.

    python3 -B scripts/auto_decomp/explain_diff.py --unit main --tu endou_test \\
        --name ShopTest --body build/auto_decomp/bodies/main/endou_test/ShopTest.c

Compiles BODY exactly as try_fn.py does (same scratch setup, same cc1, same
file-name lengths), then recompiles the candidate .i with
`-dp -dN -dS -dR -dl -dg -dd -fsched-verbose=5` (and -g for source lines when
that leaves the code unchanged), maps every differing instruction to its RTL
insn uid (assembler listing of the -dp output) and prints, per differing
region, a few lines:

  ORDER  two instructions in the other order: for sched1 and sched2, whether
         they were ready together and which rank_for_schedule key decided
         (1 priority, 2 register weight [sched1 only], 3 interblock, 4 class
         vs the last-issued insn, 5 forward dependents, 6 luid = source
         order), or which dependency kept one waiting; plus a hint.
  REG    same instruction, other register: the pseudo that got ours, its
         allocator priority floor_log2(refs)*refs/live, the pseudo(s)
         holding the target's register over its life, and the refs / live
         length change that would flip the choice; source line of each def.
  SLOT   a differing delay slot: which insn reorg put there, from where
         (before the branch / branch target / fall-through), annulled or not.
  SELECT movn vs movz: which input the select's result is tied to.

Ports the approaches of rac3 tools/sched_explain.py --why and DDS
tools/ee_gcc_retail_regs.py / ee_gcc_delay_slots.py to our cc1 (no gdb).
Nothing under src/ is written; the scratch dir is removed unless --keep.
"""
from __future__ import annotations

import argparse
import difflib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import try_fn as tf  # noqa: E402

ROOT = tf.ROOT
ABI = ["zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
       "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra"]
CALL_CLOBBERED = set(range(1, 16)) | {24, 25, 31}
BRANCH = re.compile(r"^(b\w*|j|jal|jalr|jr)$")
DUMP_FLAGS = ["-dp", "-dN", "-dS", "-dR", "-dl", "-dg", "-dd", "-fsched-verbose=5"]


def regnum(tok: str):
    t = tok.strip().lstrip("$")
    if t in ABI:
        return ABI.index(t)
    if t == "fp":
        return 30
    m = re.fullmatch(r"f(\d+)", t)
    if m:
        return 32 + int(m.group(1))
    return None


def regname(n: int) -> str:
    return ABI[n] if 0 <= n < 32 else (f"f{n - 32}" if n < 64 else f"hard{n}")


def floor_log2(n: int) -> int:
    return n.bit_length() - 1 if n > 0 else -1


def priority(refs: int, live: int) -> int:
    """global.c / local-alloc.c allocation priority (x10000, size 1)."""
    if not live:
        return 0
    return max(floor_log2(refs), 0) * refs * 10000 // live


def text_of(line: str) -> str:
    """disasm row -> instruction text without offset and relocation tag."""
    s = line.split(":", 1)[1] if re.match(r"^\s*[0-9a-f]+:", line) else line
    return s.split("  [")[0].strip()


def mnem(s: str) -> str:
    return s.split()[0] if s.split() else ""


def operands(s: str) -> list[str]:
    p = s.split(None, 1)
    return [x.strip() for x in re.split(r"[,()]", p[1]) if x.strip()] if len(p) > 1 else []


# ------------------------------------------------------------ compiling ----

def same_len_rel(path: str) -> str:
    """A relative path as long as PATH, same basename (cc1 heap layout)."""
    if not path.startswith("/"):
        return path
    base = os.path.basename(path)
    n = len(path) - len(base) - 3
    return ("t/" + "x" * max(n, 1) + "/" + base) if n >= 1 else base


def dump_compile(unit: str, tu: str, cand_dir: Path, dd: Path, with_g: bool) -> tuple[Path, dict, str]:
    """cc1 the candidate .i with the dump flags in DD: (.s path, {pass: dump path}, log)."""
    ii = [p for p in cand_dir.glob("*.i")]
    ss = [p for p in cand_dir.glob("*.s")]
    if not ii or not ss:
        raise tf.TryError(f"no candidate .i/.s in {cand_dir}")
    m = re.search(r'^\s*\.file\s+1\s+"([^"]+)"', ss[0].read_text(errors="replace"), re.M)
    rel = same_len_rel(m.group(1)) if m else ii[0].name
    shutil.rmtree(dd, ignore_errors=True)
    (dd / rel).parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ii[0], dd / rel)
    gcc, flags = tf.cc1_cmd(unit, tu)
    cmd = ["env", "-i", "PATH=/usr/bin:/bin", "LC_ALL=C", "setarch", "-R", gcc, *flags, "-S", rel,
           "-o", "x.s", *DUMP_FLAGS] + (["-g"] if with_g else [])
    p = subprocess.run(cmd, cwd=dd, capture_output=True, text=True, timeout=120)
    if p.returncode != 0 or not (dd / "x.s").is_file():
        raise tf.TryError("dump compile failed: " + (p.stdout + p.stderr)[-800:])
    dumps = {}
    for suf, key in ((".regmove", "regmove"), (".sched", "sched1"), (".lreg", "lreg"), (".greg", "greg"),
                     (".sched2", "sched2"), (".dbr", "dbr")):
        hit = [q for q in dd.rglob(f"*{suf}") if re.search(r"\.\d+" + re.escape(suf) + "$", q.name)]
        if hit:
            dumps[key] = hit[0]
    return dd / "x.s", dumps, p.stderr


def listing(unit: str, tu: str, scratch: Path, s_path: Path) -> list[str]:
    """Assemble S_PATH like the build does, return the -al listing lines."""
    unit_dir, g, rule, edge = tf.compile_edge(unit, tu)
    v = {k: tf.expand(x, g) for k, x in edge["vars"].items()}
    asm = v.get("eeas") or v.get("cas")
    if not asm:
        m = re.search(r"(\S+/ee-as)\b", rule["command"])
        asm = m.group(1) if m else None
    if not asm:
        raise tf.TryError("no assembler in the compile edge")
    cinc = scratch / "cand" / "cinc"
    cwd = cinc if cinc.is_dir() and any(cinc.iterdir()) else unit_dir
    gflag = v.get("gflag") or "-G8"
    lst = s_path.parent / "x.lst"
    cmd = [asm, "-EL", "-m5900", "-mabi=eabi", gflag, *v.get("asx", "").split(), "-I", ".",
           "-I", str(unit_dir), "-I", str(unit_dir / "include"), "-I", str(ROOT),
           f"-al={lst}", "-o", str(s_path.parent / "x.o"), str(s_path)]
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=120)
    if p.returncode != 0 or not lst.is_file():
        raise tf.TryError("listing assembly failed: " + (p.stdout + p.stderr)[-600:])
    return lst.read_text(errors="replace").splitlines()


def function_words(lst: list[str], name: str) -> list[tuple[bytes, str, int | None]]:
    """[(word bytes, .s text, uid)] of NAME's code from an as listing (words
    emitted into other sections, e.g. a switch table in .rdata, are skipped)."""
    out = []
    inside = False
    in_text, prev_text = True, True
    row = re.compile(r"^\s*\d+\s([0-9a-f]{4,8})\s([0-9A-F]+)\s*(.*)$")
    cont = re.compile(r"^\s*\d+\s{3,}([0-9A-F]{8,})\s*$")
    direc = re.compile(r"^\s*\d+\s+(?:[0-9a-f]{4,8}\s+[0-9A-F]*\s*)?\s*\.(text|rdata|data|sdata|sbss|bss|"
                       r"section|previous)\b\s*(\S*)")
    last_cont_ok = False
    for ln in lst:
        m = row.match(ln)
        if not inside:
            if re.match(r"^\s*\d+\s+(?:[0-9a-f]{4,8}\s+)?" + re.escape(name) + r":", ln):
                inside = True
            continue
        if re.search(r"^\s*\d+\s+(?:[0-9a-f]{4,8}\s+[0-9A-F]*\s*)?\s*\.end\s+" + re.escape(name) + r"\b", ln):
            break
        d = direc.match(ln)
        if d:
            k, arg = d.group(1), d.group(2)
            now = (k == "text" or (k == "section" and arg.startswith(".text"))) if k != "previous" else prev_text
            prev_text, in_text = in_text, now
        if m:
            last_cont_ok = in_text
            if not in_text:
                continue
            hx = m.group(2)
            text = m.group(3).strip()
            u = re.search(r"#\s*(\d+)\s+[\w+/*]+\s*\[length", text)
            uid = int(u.group(1)) if u else None
            clean = re.sub(r"\s*#.*$", "", text).strip()
            for k in range(0, len(hx) // 8):
                w = bytes.fromhex(hx[k * 8:k * 8 + 8])
                out.append((w, clean if k == 0 else f"({clean} +{k})", uid))
            continue
        c = cont.match(ln)
        if c and out and last_cont_ok:
            hx = c.group(1)
            for k in range(0, len(hx) // 8):
                out.append((bytes.fromhex(hx[k * 8:k * 8 + 8]), f"({out[-1][1].strip('()')} +)", out[-1][2]))
    return out


# ----------------------------------------------------------- RTL dumps ----

def fn_dump(path: Path | None, name: str) -> str:
    if not path:
        return ""
    t = path.read_text(errors="replace")
    i = t.find(f";; Function {name}\n")
    if i < 0:
        return ""
    j = t.find("\n;; Function ", i + 10)
    return t[i:j if j > 0 else None]


INSN_HEAD = re.compile(r"^\((insn|call_insn|jump_insn)(?:/\w+)?(?::\w+)? (\d+) (\d+) (\d+) ", re.M)


def rtl_insns(text: str) -> dict[int, str]:
    out = {}
    heads = list(re.finditer(r"^\((?:insn|call_insn|jump_insn|note|code_label|barrier)\S* (\d+) ", text, re.M))
    for k, m in enumerate(heads):
        end = heads[k + 1].start() if k + 1 < len(heads) else len(text)
        chunk = text[m.start():end]
        if chunk.startswith(("(insn", "(call_insn", "(jump_insn")):
            out[int(m.group(1))] = chunk
    return out


def chain(text: str) -> list[tuple[int, str]]:
    """[(uid, kind)] for insns and line notes in chain order (kind 'line:N' for notes)."""
    out = []
    for m in re.finditer(r"^\((insn|call_insn|jump_insn|note)\S* (\d+) \d+ \d+ (?:\(\"[^\"]*\"\) (\d+)|)", text, re.M):
        if m.group(1) == "note":
            if m.group(3):
                out.append((int(m.group(2)), "line:" + m.group(3)))
        else:
            out.append((int(m.group(2)), m.group(1)))
    return out


def reg_weight(rtl: str) -> int:
    """haifa find_insn_reg_weight: +1 per SET/CLOBBER of a register, -1 per REG_DEAD/REG_UNUSED."""
    body = re.sub(r"^\(\w+(?:/\w+)?(?::\w+)? \d+ \d+ \d+ ", "", rtl.strip())
    w = 0
    if re.match(r"\((?:set|clobber) \((?:reg|subreg)", body):
        w += 1
    elif body.startswith("(parallel"):
        w += len(re.findall(r"\((?:set|clobber) \((?:reg|subreg)", body.split("(expr_list")[0]))
    w -= len(re.findall(r"expr_list:REG_(?:DEAD|UNUSED)", rtl))
    return w


def dep_kinds(rtl: str) -> dict[int, str]:
    """{pred uid: data|anti|output} from LOG_LINKS."""
    out = {}
    m = re.search(r"\(insn_list.*", rtl, re.S)
    if not m:
        return out
    for d in re.finditer(r"insn_list(?::REG_DEP_(\w+))? (\d+)", rtl.split("(expr_list")[0]):
        out[int(d.group(2))] = (d.group(1) or "data").lower()
    return out


def parse_sched(text: str) -> list[dict]:
    blocks = []
    rgn_of = {}
    for m in re.finditer(r";;\trgn (\d+) nr_blocks (\d+):\n;;\tbb/block:\s+([\d/ ]+)", text):
        for pair in m.group(3).split():
            if "/" in pair:
                rgn_of[int(pair.split("/")[1])] = int(m.group(2))
    for m in re.finditer(r"(?ms)^;;   -- basic block (\d+) from (\d+) to (\d+) -- (before|after) reload\n(.*?)"
                         r"(?=^;;   -- basic block |\Z)", text):
        bnum, body = int(m.group(1)), m.group(5)
        table, order = {}, []
        for t in re.finditer(r"(?m)^;;\s+(\d+)\s+(-?\d+)\s+(\d+)\s+(\d+)\s+(-?\d+)\s+(\d+)\s+(-?\d+) -\s*(-?\d+)"
                             r"\s+(\S*)\t: ([\d ]*)$", body):
            uid = int(t.group(1))
            table[uid] = dict(bb=int(t.group(3)), dep=int(t.group(4)), prio=int(t.group(5)),
                              cost=int(t.group(6)), deps=[int(x) for x in t.group(10).split()])
            order.append(uid)
        cycles, cur = [], None
        for line in body.split("\n"):
            r = re.match(r";;\tReady list \(t =\s*(\d+)\):\s*(.*)$", line)
            if r:
                cur = dict(t=int(r.group(1)), ready=[int(x.split("/")[0]) for x in r.group(2).split()], issued=[])
                cycles.append(cur)
                continue
            s = re.match(r";;\t\t--> scheduling insn <<<(\d+)>>>", line)
            if s and cur is not None:
                cur["issued"].append(int(s.group(1)))
        blocks.append(dict(block=bnum, table=table, order=order, cycles=cycles,
                           multi=rgn_of.get(bnum, 1) > 1))
    return blocks


# --------------------------------------------------------------- order ----

KEYS = ("priority", "reg weight", "class vs last insn", "dependents", "luid (source order)")
CLASS = {3: "indep", 2: "anti/out", 1: "data>1"}


def rank_keys(uid, blk, weights, kinds, last, use_weight):
    tb = blk["table"]
    if last is None:
        cls = None
    else:
        k = kinds.get(uid, {}).get(last)
        if k is None:
            cls = 3
        elif k == "data":
            cls = 3 if tb.get(last, {}).get("cost", 1) == 1 else 1
        else:
            cls = 2
    return dict(prio=tb[uid]["prio"], weight=weights.get(uid, 0) if use_weight else 0, cls=cls,
                deps=len(tb[uid]["deps"]), luid=blk["order"].index(uid))


def deciding(a, b, ka, kb, use_weight):
    """(key name, winner, va, vb) of the first key separating A and B."""
    steps = [("priority", "prio", 1)] + ([("reg weight", "weight", -1)] if use_weight else []) + \
        [("class vs last insn", "cls", 1), ("dependents", "deps", 1), ("luid (source order)", "luid", -1)]
    for nm, k, sign in steps:
        va, vb = ka[k], kb[k]
        if va is None or vb is None or va == vb:
            continue
        return nm, (a if (va - vb) * sign > 0 else b), va, vb
    return "tie", None, None, None


class Ctx:
    def __init__(self, name, dumps, uid_text, src_lines):
        self.name = name
        self.sched = {p: parse_sched(fn_dump(dumps.get(p), name)) for p in ("sched1", "sched2")}
        self.rtl = {p: rtl_insns(fn_dump(dumps.get(p), name)) for p in ("regmove", "sched1", "sched2", "lreg")}
        self.weights = {u: reg_weight(r) for u, r in self.rtl["regmove"].items()}
        self.kinds = {p: {u: dep_kinds(r) for u, r in self.rtl[p].items()} for p in ("sched1", "sched2")}
        self.uid_text = uid_text
        self.lreg = fn_dump(dumps.get("lreg"), name)
        self.greg = fn_dump(dumps.get("greg"), name)
        self.dbr = fn_dump(dumps.get("dbr"), name)
        self.src_lines = src_lines
        # line of each uid (needs -g line notes)
        self.line_of = {}
        cur = None
        for u, k in chain(self.lreg):
            if k.startswith("line:"):
                cur = int(k[5:])
            elif cur is not None:
                self.line_of[u] = cur
        self._alloc()

    def t(self, uid):
        return self.uid_text.get(uid) or f"insn {uid}"

    def src(self, uid):
        ln = self.line_of.get(uid)
        if ln is None or not self.src_lines or ln > len(self.src_lines):
            return ""
        return f"L{ln}: `{' '.join(self.src_lines[ln - 1].split())[:70]}`"

    # ---- order
    def order(self, x: int, y: int) -> list[str]:
        """Ours issues X before Y; the target has Y first."""
        out = []
        sched1_luid = False
        for p in ("sched1", "sched2"):
            use_w = p == "sched1"
            both = [b for b in self.sched[p] if x in b["table"] and y in b["table"]]
            # interblock regions print the whole region's table in every block: take the
            # block that actually issued them
            blk = next((b for b in both if any(x in c["issued"] or y in c["issued"] for c in b["cycles"])),
                       both[0] if both else None)
            if blk is None:
                bx = next((b["block"] for b in self.sched[p] if x in b["table"]), None)
                by = next((b["block"] for b in self.sched[p] if y in b["table"]), None)
                if bx is not None and by is not None:
                    out.append(f"{p}: different blocks (bb{bx} / bb{by}): block layout, not a tie")
                else:
                    out.append(f"{p}: {'one insn not present' if bx is not None or by is not None else 'not present'}"
                               " (created later: split / reload / reorg)")
                continue
            tb = blk["table"]
            if y in tb[x]["deps"] or x in tb[y]["deps"]:
                first, second = (x, y) if y in tb[x]["deps"] else (y, x)
                kind = self.kinds[p].get(second, {}).get(first, "data")
                out.append(f"{p}: {second} depends on {first} ({kind} dependence) - order forced")
                continue
            last = None
            done = False
            for cyc in blk["cycles"]:
                r = cyc["ready"]
                if x in r and y in r:
                    win = x if r.index(x) > r.index(y) else y
                    kx = rank_keys(x, blk, self.weights, self.kinds[p], last, use_w)
                    ky = rank_keys(y, blk, self.weights, self.kinds[p], last, use_w)
                    nm, kw, va, vb = deciding(x, y, kx, ky, use_w)
                    keys = (f"prio {kx['prio']}/{ky['prio']}" + (f", weight {kx['weight']}/{ky['weight']}" if use_w else "")
                            + f", class {CLASS.get(kx['cls'], '-')}/{CLASS.get(ky['cls'], '-')}"
                            + f", dependents {kx['deps']}/{ky['deps']}, luid {kx['luid']}/{ky['luid']}")
                    ok = "" if kw in (None, win) else " (keys disagree with the dump: interblock/target hook?)"
                    if blk["multi"] and tb[x]["bb"] != tb[y]["bb"]:
                        nm, ok = "interblock (in-block insn preferred over a moved one)", ""
                    out.append(f"{p}: ready together at t={cyc['t']}, {win} won by {nm}{ok}  [{x}/{y}: {keys}]")
                    out.append("  -> " + self.hint(p, nm, win, x if win == y else y, kx if win == x else ky,
                                                   ky if win == x else kx, last))
                    if p == "sched2" and nm.startswith("luid"):
                        sched1_luid = True
                    done = True
                    break
                if cyc["issued"]:
                    last = cyc["issued"][-1]
            if not done:
                tx = next((c["t"] for c in blk["cycles"] if x in c["issued"]), None)
                ty0 = next((c["t"] for c in blk["cycles"] if y in c["ready"]), None)
                preds = [u for u, e in tb.items() if y in e["deps"]]
                issued_at = {u: c["t"] for c in blk["cycles"] for u in c["issued"]}
                pl = max(preds, key=lambda u: issued_at.get(u, -1)) if preds else None
                why = (f"; {y} waits for {pl} `{self.t(pl)}` (issued t={issued_at.get(pl)})" if pl is not None else "")
                out.append(f"{p}: never ready together: {x} issued t={tx}, {y} ready t={ty0}{why}")
        if sched1_luid:
            out.append("  (sched2's luid = sched1's output order: the sched1 line above is the real decision)")
        return out

    def hint(self, p, key, win, lose, kw, kl, last):
        w, l = f"{win} `{self.t(win)}`", f"{lose} `{self.t(lose)}`"
        if key == "priority":
            return (f"{w} has the longer path to the block end (prio {kw['prio']} vs {kl['prio']}): make {l}'s result "
                    f"feed a longer chain, or {w}'s a shorter one (use its value later / in fewer steps)")
        if key == "reg weight":
            return (f"a register dies at {w} (weight {kw['weight']} vs {kl['weight']}): make that value's LAST use "
                    f"later (after {lose}'s statement), or make a value die at {l} (its last use there)")
        if key.startswith("class"):
            return (f"relation to the last-issued insn {last} `{self.t(last)}`: {lose} has a "
                    f"{CLASS.get(kl['cls'], '?')} dependence on it, {win} {CLASS.get(kw['cls'], '?')}: change what is "
                    "scheduled just before them")
        if key == "dependents":
            return (f"{w} has more insns depending on it ({kw['deps']} vs {kl['deps']}): give {l}'s value more "
                    f"direct users, or {win}'s fewer")
        if key.startswith("luid"):
            if p == "sched1":
                sl, sw = self.src(lose), self.src(win)
                loop = " (made by the loop optimizer: it sits at the end of the preheader)" if re.search(
                    r"`(do|for|while)\b", sl) else ""
                return (f"pure source-order tie (all other keys equal): {lose} must come first in the insn stream - "
                        f"compute its value earlier{(' [' + sl + ']') if sl else ''}{loop}, or {win}'s later"
                        f"{(' [' + sw + ']') if sw else ''}; or break the tie on weight: {self.weight_hint(win, lose)}")
            return "tie broken by the insn order sched1 produced (see sched1)"
        return "all keys equal: insn order decided"

    def weight_hint(self, win, lose) -> str:
        """Register-weight lever for a sched1 tie (BT3: the insn where a value dies goes first)."""
        def dead(u):
            r = self.rtl["regmove"].get(u, "")
            return [int(x) for x in re.findall(r"REG_(?:DEAD|UNUSED) \(reg(?:/\w+)*:\w+ (\d+)\)", r)]
        def used(u):
            r = self.rtl["regmove"].get(u, "").split("(expr_list")[0]
            body = re.sub(r"^\(\w+(?:/\w+)?(?::\w+)? \d+ \d+ \d+ \(set \([^()]*\)", "", r)
            return sorted({int(x) for x in re.findall(r"\(reg(?:/\w+)*:\w+ (\d+)\)", body)})
        dw, ul = dead(win), [x for x in used(lose) if x not in dead(lose)]
        parts = []
        if dw:
            parts.append(f"r{dw[0]} dies at {win}; if it stayed live after it (used again later) {win}'s weight "
                         f"rises and {lose} goes first")
        if ul:
            parts.append(f"if r{ul[0]} (read by {lose}) had its LAST use at {lose}, {lose}'s weight drops and it "
                         "goes first")
        return "; ".join(parts) or "a value whose last use is the other insn"

    # ---- allocation
    def _alloc(self):
        self.info = {}
        for m in re.finditer(r"^Register (\d+) used (\d+) times across (\d+) insns(.*)$", self.lreg, re.M):
            calls = re.search(r"crosses (\d+) call", m.group(4))
            blk = re.search(r" in block (\d+)", m.group(4))
            self.info[int(m.group(1))] = dict(refs=int(m.group(2)), live=int(m.group(3)),
                                              calls=int(calls.group(1)) if calls else 0,
                                              block=int(blk.group(1)) if blk else None,
                                              user="user var" in m.group(4))
        blkd = self.greg[self.greg.find("Register dispositions"):].split("\n\n")[0]
        self.disp = {int(a): int(b) for a, b in re.findall(r"(\d+) in (\d+)", blkd)}
        self.conf = {int(a): {int(x) for x in b.split()}
                     for a, b in re.findall(r"^;; (\d+) conflicts: ([\d ]*)$", self.greg, re.M)}
        self.prefs = {int(a): {int(x) for x in b.split()}
                      for a, b in re.findall(r"^;; (\d+) preferences: ([\d ]*)$", self.greg, re.M)}
        m = re.search(r"^;; \d+ regs to allocate: ([\d ]*)$", self.greg, re.M)
        self.gorder = {int(r): k for k, r in enumerate(m.group(1).split())} if m else {}
        self.first_pseudo = min(self.info) if self.info else 1 << 30
        lins = rtl_insns(self.lreg)
        self.insn_regs, self.defs, self.span = {}, {}, {}
        for pos, (u, chunk) in enumerate(lins.items()):
            body = chunk.split("(expr_list")[0]
            regs = {int(x) for x in re.findall(r"\(reg(?:/\w+)*:\w+ (\d+)\)", body) if int(x) >= self.first_pseudo}
            self.insn_regs[u] = regs
            for r in regs:
                a, b = self.span.get(r, (pos, pos))
                self.span[r] = (min(a, pos), max(b, pos))
            d = re.match(r"\(insn\S* \d+ \d+ \d+ \(set \(reg(?:/\w+)*:\w+ (\d+)\)", chunk)
            if d and int(d.group(1)) not in self.defs:
                self.defs[int(d.group(1))] = u
        self.lins = lins

    def pdesc(self, r):
        d = self.info.get(r, {})
        where = f"local bb{d['block']}" if d.get("block") is not None else f"global #{self.gorder.get(r, '?')}"
        du = self.defs.get(r)
        src = self.src(du) if du else ""
        dtxt = f" def {du} `{self.t(du)}`" if du else ""
        return (f"r{r}{' (user var)' if d.get('user') else ''} pri {priority(d.get('refs', 0), d.get('live', 0))} "
                f"refs {d.get('refs')} live {d.get('live')} calls {d.get('calls')} {where}{dtxt} {src}").rstrip()

    def holders(self, r, hard):
        """Pseudos allocated to HARD whose life overlaps R's (for a block-local R only the
        locals of its block: globals are allocated after local-alloc and cannot block it)."""
        loc = self.info.get(r, {}).get("block")
        if r in self.conf and loc is None:
            cands = [z for z in self.conf[r] if z != r and z >= self.first_pseudo and self.disp.get(z) == hard]
        else:
            a, b = self.span.get(r, (0, -1))
            cands = [z for z, (c, d) in self.span.items()
                     if z != r and self.disp.get(z) == hard and c <= b and d >= a
                     and (loc is None or self.info.get(z, {}).get("block") == loc)]
        return sorted(cands, key=lambda z: -self.pri(z))

    def reg(self, uid, hx, hy) -> list[str]:
        regs = self.insn_regs.get(uid)
        if regs is None:
            return [f"insn {uid} not in the allocator dumps (made by reload/split/reorg)"]
        owners = sorted(r for r in regs if self.disp.get(r) == hx)
        if not owners:
            hard = re.search(r"\(reg(?:/\w+)*:\w+ %d\)" % hx, self.lins.get(uid, ""))
            return [f"{regname(hx)} is a fixed hard register in the RTL of insn {uid}" if hard else
                    f"no pseudo of insn {uid} got {regname(hx)} (reload chose it)"]
        out = []
        for r in owners:
            d = self.info.get(r, {})
            out.append(f"ours: {self.pdesc(r)} -> {regname(hx)}; target uses {regname(hy)}")
            if hy in CALL_CLOBBERED and d.get("calls"):
                out.append(f"  {regname(hy)} is call-clobbered and r{r} crosses {d['calls']} call(s): in the target "
                           "this value does not live across a call (compute it after the call / use it before)")
            if hy in CALL_CLOBBERED and d.get("calls"):
                continue
            hl = self.holders(r, hy)
            pr = self.pri(r)
            rloc = d.get("block") is not None
            for z in hl[:1]:
                pz = self.pri(z)
                zloc = self.info.get(z, {}).get("block") is not None
                more = f" (+{len(hl) - 1} more)" if len(hl) > 1 else ""
                out.append(f"  {regname(hy)} is held by {self.pdesc(z)}{more}")
                if zloc and not rloc:
                    out.append(f"  -> r{z} is block-local: local-alloc gives it {regname(hy)} before global-alloc "
                               f"runs, so r{r} cannot have {regname(hy)} while r{z} is live; in the target their lives "
                               f"do not overlap (shorter r{r} life, or r{z}'s value computed/used elsewhere)")
                elif pz >= pr:
                    out.append(f"  -> r{z} is allocated first (pri {pz} >= {pr}); {self.flip(r, z)}")
                elif ORDER_POS.get(hx, 99) < ORDER_POS.get(hy, 99):
                    out.append(f"  -> r{r} is allocated first (pri {pr} > {pz}) and takes {regname(hx)}, the first free "
                               f"register; for the target's choice r{z} (or another value) must take {regname(hx)} "
                               f"first: {self.flip(z, r)}")
                else:
                    out.append(f"  -> r{r} outranks r{z} ({pr} > {pz}) but {regname(hy)} was not usable for it: a "
                               f"preference (r{r} prefs {sorted(regname(x) for x in self.prefs.get(r, []))}) or a "
                               f"copy to/from {regname(hx)} pulled it there")
            if not hl:
                busy = self.hard_busy(r, hy)
                if busy:
                    out.append(f"  {regname(hy)} is used as a hard register during r{r}'s life (insn {busy[0]} "
                               f"`{self.t(busy[0])}`: call args / return value): in the target that use is outside this "
                               "value's life (set up the call later / compute the value after it)")
                elif ORDER_POS.get(hy, 99) > ORDER_POS.get(hx, 99):
                    out.append(f"  {regname(hy)} was free, but it comes after {regname(hx)} in allocation order "
                               "(v0 v1 a0-a3 t0-t7 s0-s7): in the target the earlier registers were busy during this "
                               "value's life -> the target value lives LONGER (computed earlier, e.g. before a compare "
                               "or call setup, or used later) and overlaps values that sit in "
                               f"{regname(hx)}..; or a copy ties it to {regname(hy)}")
                else:
                    pf = sorted(self.prefs.get(r, []))
                    out.append(f"  {regname(hy)} was free and earlier in allocation order: ours was pulled to "
                               f"{regname(hx)} by a preference/tie"
                               + (f" (r{r} prefers {','.join(regname(x) for x in pf)})" if pf else
                                  " (a copy to/from it, or local-alloc tying it to an input/output that dies)"))
        return out

    def pri(self, r):
        d = self.info.get(r, {})
        return priority(d.get("refs", 0), d.get("live", 0))

    def flip(self, a, b) -> str:
        """How pseudo A could outrank B (refs / live length)."""
        da, db = self.info.get(a, {}), self.info.get(b, {})
        ra, la, rb, lb = da.get("refs", 0), da.get("live", 0), db.get("refs", 0), db.get("live", 0)
        pa, pb = self.pri(a), self.pri(b)
        if pa > pb:
            return f"r{a} already outranks r{b} ({pa} > {pb}): order/preference decided"
        lga = max(floor_log2(ra), 0)
        if not lga:
            return f"r{a} has refs {ra}: priority 0 until it has >= 2 uses (use the value once more)"
        need_la = lga * ra * 10000 // (pb + 1)
        lgb = max(floor_log2(rb), 0)
        need_lb = (lgb * rb * 10000 // pa + 1) if pa else None
        need_ra = next((k for k in range(ra + 1, 200) if la and priority(k, la) > pb), None)
        first = (f"shorten r{a}'s life to <= {need_la} insns (now {la})" if need_la >= 2 else
                 f"r{a}'s life cannot get short enough")
        refs = f" or raise its uses to >= {need_ra} (now {ra})" if need_ra else ""
        return (first + refs + (f", or lengthen r{b}'s life to >= {need_lb} (now {lb})" if need_lb
                                else f", or reduce r{b}'s uses"))

    def hard_busy(self, r, hard) -> list[int]:
        a, b = self.span.get(r, (0, -1))
        out = []
        for pos, (u, ch) in enumerate(self.lins.items()):
            if a < pos < b and re.search(r"\(reg(?:/\w+)*:\w+ %d\)" % hard, ch.split("(expr_list")[0]):
                out.append(u)
        return out


ORDER_POS = {r: k for k, r in enumerate([2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                                          22, 23, 24, 25, 1, 30, 31])}


# ---------------------------------------------------------- delay slots ----

def balanced(text: str, start: int) -> tuple[str, int]:
    depth = 0
    q = False
    for i in range(start, len(text)):
        ch = text[i]
        if q:
            if ch == '"':
                q = False
            continue
        if ch == '"':
            q = True
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return text[start:i + 1], i + 1
    return text[start:], len(text)


def dbr_sequences(text: str) -> dict[int, dict]:
    """{slot uid: {branch, branch_kind, annulled, from_target}} from the .dbr dump."""
    out = {}
    hdr = re.compile(r"\((call_insn|jump_insn|insn)((?:/\w)*)(?::\w+)? (\d+) ")
    for m in re.finditer(r"^\(insn\S* \d+ \d+ \d+ \(sequence\[", text, re.M):
        form, _ = balanced(text, m.start())
        k = form.find("(sequence[") + len("(sequence[")
        nodes = []
        while True:
            h = hdr.search(form, k)
            if not h:
                break
            node, k = balanced(form, h.start())
            nodes.append((h.group(1), set(h.group(2).replace("/", "")), int(h.group(3)), node))
        if len(nodes) < 2:
            continue
        bk, bf, bu, _ = nodes[0]
        for kind, fl, u, node in nodes[1:]:
            out[u] = dict(branch=bu, branch_kind=bk, annulled="u" in bf, from_target="s" in fl)
    return out


# ---------------------------------------------------------------- main ----

def _jmask(w: bytes) -> int:
    """Word with j/jal targets masked (other functions of the TU may move under -g)."""
    v = int.from_bytes(w, "little")
    return v & 0xFC000000 if (v >> 26) in (2, 3) else v


def full_disasm(obj: Path, sl: dict) -> list[str]:
    """One text per word (objdump folds zero runs into `...`: those are nops)."""
    n = len(sl["bytes"]) // 4
    out = ["nop"] * n
    for x in tf.disasm(obj, sl):
        m = re.match(r"^\s*([0-9a-f]+):", x)
        if m and int(m.group(1), 16) // 4 < n:
            out[int(m.group(1), 16) // 4] = text_of(x)
    return out

def analyse(unit, tu, name, body: Path, keep=False, with_g=True, max_regions=8) -> tuple[list[str], dict]:
    rep = tf.try_function(unit, tu, name, body, keep=True, diff_limit=100000)
    scratch = Path(rep["scratch"])
    lines: list[str] = []
    try:
        if not rep.get("ok"):
            return [f"compile failed ({rep.get('stage')}): {str(rep.get('error'))[:400]}"], rep
        if rep.get("match"):
            return ["match: nothing to explain"], rep
        unit_dir = tf.compile_edge(unit, tu)[0]
        base_o = next((scratch / "base").glob("*.o"))
        cand_o = next((scratch / "cand").glob("*.o"))
        bsl, csl = tf.function_slice(base_o, name), tf.function_slice(cand_o, name)
        T, C = full_disasm(base_o, bsl), full_disasm(cand_o, csl)
        cw = [csl["bytes"][i:i + 4] for i in range(0, len(csl["bytes"]), 4)]
        words, used_g, dumps = None, False, {}
        for g in ([True, False] if with_g else [False]):
            s_path, dumps, _ = dump_compile(unit, tu, scratch / "cand", scratch / "dd", g)
            fw = function_words(listing(unit, tu, scratch, s_path), name)
            if [_jmask(w) for w, _, _ in fw] == [_jmask(w) for w in cw]:
                words, used_g = fw, g
                break
        note = ""
        if words is None:
            note = "WARNING: the dump compile does not reproduce the candidate's code; uids may be off"
            words = fw
        uid_at = [w[2] for w in words] + [None] * max(0, len(C) - len(words))
        uid_text = {}
        for i, (_, s, u) in enumerate(words):
            if u is not None and u not in uid_text and i < len(C):
                uid_text[u] = C[i]
        src_lines = None
        srcf = scratch / "cand" / "src" / f"{tu}.c"
        if used_g and srcf.is_file():
            src_lines = srcf.read_text(errors="replace").split("\n")
        ctx = Ctx(name, dumps, uid_text, src_lines)
        seq = dbr_sequences(ctx.dbr)
        maxuid2 = max(ctx.rtl["sched2"]) if ctx.rtl["sched2"] else 0

        sm = difflib.SequenceMatcher(None, T, C, autojunk=False)
        t2c, pairs, t_only, c_only = {}, [], [], []
        for op, i1, i2, j1, j2 in sm.get_opcodes():
            if op == "equal":
                for k in range(i2 - i1):
                    t2c[i1 + k] = j1 + k
            elif op == "replace" and i2 - i1 == j2 - j1:
                pairs += [(i1 + k, j1 + k) for k in range(i2 - i1)]
            else:
                t_only += list(range(i1, i2))
                c_only += list(range(j1, j2))
        regions = []
        # moved instructions (same text on both sides, different place)
        used_c = set()
        for ti in t_only:
            cj = next((j for j in sorted(c_only, key=lambda j: abs(j - ti)) if C[j] == T[ti] and j not in used_c
                       and abs(j - ti) <= 24), None)
            if cj is None:
                continue
            used_c.add(cj)
            prev = max((k for k in t2c if k < ti), default=None)
            cpos = t2c[prev] + 1 if prev is not None else 0
            if cpos > cj:      # ours too early: it jumped over C[cj+1:cpos]
                jumped = [j for j in range(cj + 1, cpos) if j not in used_c and j not in c_only]
                regions.append(("order", cj, [(uid_at[cj], uid_at[j]) for j in jumped[:2]], ti))
            else:              # ours too late: target had it before C[cpos:cj]
                jumped = [j for j in range(cpos, cj) if j not in used_c and j not in c_only]
                regions.append(("order", cj, [(uid_at[j], uid_at[cj]) for j in jumped[-2:]], ti))
        for ti, cj in pairs:
            if T[ti] == C[cj]:
                continue
            regions.append(("pair", cj, None, ti))
        rest_t = [ti for ti in t_only if not any(r[3] == ti for r in regions)]
        rest_c = [j for j in c_only if j not in used_c]
        regions.sort(key=lambda r: r[1])

        lines.append(f"{unit}/{tu}/{name}: score {rep.get('score')}, {rep.get('differing_words')} differing words"
                     f"{'' if used_g else ' (no -g: source lines unavailable)'}")
        if note:
            lines.append(note)
        if rest_t or rest_c:
            lines.append(f"SHAPE: {len(rest_c)} insn(s) only in ours "
                         f"({'; '.join(f'`{C[j]}` ({uid_at[j]})' for j in rest_c[:3])}), {len(rest_t)} only in target "
                         f"({'; '.join(f'`{T[i]}`' for i in rest_t[:3])}): a code-shape difference - fix it first, "
                         "the register/order notes below may be its knock-on effects")
        # movn/movz regions: the v0/v1 and preload-operand diffs around them are its consequence
        sel_at = [cj for kind, cj, _, ti in regions if kind == "pair" and {mnem(T[ti]), mnem(C[cj])} == {"movn", "movz"}]
        reg_seen: dict = {}
        blocks: list[tuple[int, list[str]]] = []
        for kind, cj, extra, ti in regions:
            off = f"+0x{cj * 4:x}"
            if kind == "order":
                for x, y in extra or []:
                    if x is None or y is None or (x, y) in reg_seen:
                        continue
                    reg_seen[(x, y)] = []
                    blocks.append((cj, [f"ORDER {off}: ours `{ctx.t(x)}` ({x}) before `{ctx.t(y)}` ({y}); "
                                        "target the reverse"] + ["  " + q for q in ctx.order(x, y)]))
                if not extra:
                    blocks.append((cj, [f"ORDER {off}: `{C[cj]}` moved (target +0x{ti * 4:x}); "
                                        "no insn between to compare"]))
                continue
            t, c = T[ti], C[cj]
            u = uid_at[cj] if cj < len(uid_at) else None
            mt, mc = mnem(t), mnem(c)
            ot, oc = operands(t), operands(c)
            # only a branch / jal target moved (a length change elsewhere): not a decision
            if mt == mc and BRANCH.match(mt) and ot[:-1] == oc[:-1] and re.fullmatch(r"[0-9a-f]+", ot[-1] if ot else ""):
                continue
            if {mt, mc} == {"movn", "movz"}:
                blocks.append((cj, [f"SELECT {off}: ours `{c}`, target `{t}`"] +
                               ["  " + q for q in select_explain(ctx, u, c, t)]))
                continue
            if any(-4 <= cj - sj <= 4 for sj in sel_at):
                continue
            in_slot = (cj > 0 and BRANCH.match(mnem(C[cj - 1]))) or (ti > 0 and BRANCH.match(mnem(T[ti - 1])))
            if mt == mc and len(ot) == len(oc):
                diffs = [(x, y) for x, y in zip(oc, ot) if x != y]
                rd = [(regnum(x), regnum(y)) for x, y in diffs]
                if diffs and all(h is not None and k is not None for h, k in rd):
                    for hx, hy in rd:
                        if u is None:
                            continue
                        ex = ctx.reg(u, hx, hy)
                        key = (ex[0], hy)
                        if key in reg_seen:
                            reg_seen[key].append(off)
                            continue
                        reg_seen[key] = []
                        blocks.append((cj, [f"REG {off}: ours `{c}` ({u}), target `{t}`", key] +
                                       ["  " + q for q in ex]))
                    continue
            if in_slot:
                blocks.append((cj, [f"SLOT {off}: ours `{c}` ({u}), target `{t}` in the delay slot of "
                                    f"`{C[cj - 1]}` / target `{T[ti - 1]}`"] +
                               ["  " + q for q in slot_explain(seq, u, maxuid2, ctx, C, T, cj, ti)]))
            else:
                blocks.append((cj, [f"DIFF {off}: ours `{c}` ({u}), target `{t}` (not a register/order/slot pattern)"]))
        for k, (cj, bl) in enumerate(blocks):
            if k >= max_regions:
                lines.append(f"... {len(blocks) - k} more regions (--max)")
                break
            if len(bl) > 1 and isinstance(bl[1], tuple):
                more = reg_seen.get(bl[1]) or []
                bl = [bl[0] + (f" (same value also at {', '.join(more[:4])})" if more else "")] + bl[2:]
            lines += bl
        if not regions and not rest_t and not rest_c:
            lines.append("no instruction-level difference (relocations / symbols only): see try_fn's notes")
        rep["explain_dumps"] = str(scratch / "dd") if keep else None
        return lines, rep
    finally:
        if not keep:
            shutil.rmtree(scratch, ignore_errors=True)


def slot_desc(seq, u, maxuid2, ctx) -> str:
    s = seq.get(u)
    if not s:
        return "not a filled slot in ours"
    if u > maxuid2:
        src = "copied from the branch TARGET" if s["from_target"] else "copied from the fall-through"
    else:
        src = "moved from before the branch"
    return (f"reorg filled it for {s['branch_kind']} {s['branch']} `{ctx.t(s['branch'])}` - {src}"
            + (", branch-likely (annulled)" if s["annulled"] else ""))


def slot_explain(seq, u, maxuid2, ctx, C, T, cj, ti) -> list[str]:
    out = [slot_desc(seq, u, maxuid2, ctx) if u is not None else "ours has an assembler nop there"]
    bc, bt = mnem(C[cj - 1]), mnem(T[ti - 1])
    if bc != bt and bc.rstrip("l") == bt.rstrip("l"):
        out.append("-> branch-likely mismatch: gcc predicts `==` 40%, `!=` 60%, other 50%; >=50% fills from the "
                   "target (`b..l`). Invert the condition (swap the arms), or define/declare the callee earlier "
                   "(a missing earlier definition in the file flips this)")
    tt = T[ti]
    where = [j for j in range(max(0, cj - 12), min(len(C), cj + 12)) if C[j] == tt and j != cj]
    if mnem(tt) == "nop":
        out.append("-> target left this slot empty: in the original no insn was movable (the candidate "
                   "insn depended on / fed the branch, or sat in another block)")
    elif where:
        j = where[0]
        out.append(f"-> target's slot insn `{tt}` is at ours +0x{j * 4:x} ({'before' if j < cj else 'after'} the "
                   "branch): reorg could not take it; check whether the branch reads its result or an insn between "
                   "them uses/sets its registers")
    return out


def select_explain(ctx, u, c, t) -> list[str]:
    out = []
    rtl = ctx.lins.get(u, "") if u is not None else ""
    if not rtl:
        return ["select insn not in the allocator dumps"]
    m = re.search(r"\(set(?::\w+)? \(reg(?:/\w+)*:\w+ (\d+)\)\s+\(if_then_else:\w+ \((\w+) .*?\)\s+"
                  r"\(reg(?:/\w+)*:\w+ (\d+)\)\s+\(reg(?:/\w+)*:\w+ (\d+)\)", rtl, re.S)
    if not m:
        return [f"select insn {u}: {(' '.join(rtl.split()))[:160]}"]
    dst, cond, a, b = (int(m.group(1)), m.group(2), int(m.group(3)), int(m.group(4)))
    if dst in (a, b):
        # `x = P; if (c) x = Q;` if-converted: the RTL already fixes which value is preloaded
        q = b if a == dst else a
        pre = None
        for uu, ch in ctx.lins.items():
            mm = re.match(r"\(insn\S* \d+ \d+ \d+ \(set \(reg(?:/\w+)*:\w+ %d\)\s+(.*)" % dst, ch, re.S)
            if mm and uu != u:
                pre = (uu, mm.group(1))
        src_r = re.match(r"\(reg(?:/\w+)*:\w+ (\d+)\)", pre[1]) if pre else None
        p_txt = (f"r{src_r.group(1)}" if src_r else "a value") + (f" (insn {pre[0]} `{ctx.t(pre[0])}`)" if pre else "")
        out.append(f"insn {u}: r{dst} = {p_txt}; if ({cond} cond) r{dst} = r{q}  -> movn/movz is decided BEFORE "
                   "register allocation (if-conversion keeps the preload as the select's else-input)")
        if src_r:
            out.append("  preload " + ctx.pdesc(int(src_r.group(1))))
        out.append("  moved-in " + ctx.pdesc(q))
        out.append(f"-> target preloads the OTHER value (`{t}` moves in what ours preloads): write it as "
                   f"`x = <r{q}'s value>; if (<inverse cond>) x = <preload value>;` - the statement assigned first "
                   "is the preload. The comparison's form also decides it (ov01 stat1Proc: `(dmg <= -1) ? neg : dmg` gives movn, "
                   "`(dmg < 0) ? neg : dmg` gives movz): try `< 0` / `<= -1` / `>= 0`, swapped arms, and the if-form")
        return out
    dies = {int(x) for x in re.findall(r"REG_DEAD \(reg(?:/\w+)*:\w+ (\d+)\)", rtl)}
    tied = a if ctx.disp.get(a) == ctx.disp.get(dst) else (b if ctx.disp.get(b) == ctx.disp.get(dst) else None)
    out.append(f"insn {u}: r{dst} = ({cond} ? r{a} : r{b}); result in "
               f"{regname(ctx.disp.get(dst, -1)) if dst in ctx.disp else '?'}; tied input r{tied} "
               f"(r{a} {'dies' if a in dies else 'lives on'}, r{b} {'dies' if b in dies else 'lives on'})")
    for r in (dst, a, b):
        out.append("  " + ctx.pdesc(r))
    other = b if tied == a else a
    out.append(f"-> movn/movz is chosen by which input shares the result's register (the other is moved in when the "
               f"condition holds). Target ties the other input: make r{other}'s value the one that is preloaded "
               f"and dies at the select (its last use), and keep r{tied}'s value live past it / compute it later")
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", required=True)
    ap.add_argument("--tu", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--body", type=Path, required=True)
    ap.add_argument("--max", type=int, default=8, help="differing regions to explain (default 8)")
    ap.add_argument("--keep", action="store_true", help="keep scratch + dumps (path printed)")
    ap.add_argument("--no-g", action="store_true", help="skip the -g compile (no source lines)")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()
    try:
        lines, rep = analyse(a.unit, a.tu, a.name, a.body.resolve(), a.keep, not a.no_g, a.max)
    except tf.TryError as e:
        print(f"error: {e}", file=sys.stderr)
        return 2
    if a.json:
        print(json.dumps({"lines": lines, "score": rep.get("score"), "dumps": rep.get("explain_dumps")}, indent=1))
    else:
        print("\n".join(lines))
        if a.keep:
            print(f"dumps: {rep.get('explain_dumps')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

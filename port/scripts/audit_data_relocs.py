#!/usr/bin/env python3
"""Stage 11: rank symbolised data words that are probably not pointers.

splat writes a data word as `.word Sym` / `.word Sym + off` whenever its value
falls in the RAM range.  On the PS2 that is harmless (the linker puts back the
same value), but gen_host_data.py turns it into a host relocation, so a word
that was really an integer (u16 pair, GIF register value, string tail, half of
a u64) becomes a host address.  asVramBase, Head_9/TestPrim (CLAMP_1) and the
string tails (STAGE7/8/10) were three such cases.

For every symbolised word in the data that gen_host_data.py converts, this
script takes the original value (the bytes splat copied from SLUS_204.69 /
OV*.OVL into the line comment), works out what the host would do with it and
scores how likely it is to be a non-pointer:

  * not relocated on the host (symbol is an absolute --defsym, host value ==
    PS2 value), already emitted raw (NO_RELOC_LABELS, string tail) -> skipped;
  * GIF context: the word is the low/high data half of an A+D quadword whose
    register id is a GS register (CLAMP, TEX0, FRAME, ...), or sits in a
    GIFtag;                                                           +6
  * the target address is not the start of any known symbol (only reachable
    as `sym + off`), off not a multiple of 4                          +3 / +2
  * the target symbol is code and off != 0                            +3
  * the value looks like two u16 halves (low half 0, or both < 0x400)  +1
  * neighbours inside the same label are small ints/floats            +2
    neighbours are pointers too (pointer table)                       -3
  * target is the start of a sized data/func symbol and off == 0       -4
  * the containing label is used in C as an integer/u64 table         +2

  audit_data_relocs.py [--root R] [--host port/build-wt/xsg] [--min 3] [--all]
Output: one line per word, highest score first:
  score  unit  vram  label+off  raw-value  ref  reasons
"""
import argparse
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_host_data as G  # noqa: E402

LINE_RE = re.compile(r"/\*\s*([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]{8})\s*\*/\s*\.word\s+(.+)$")
REF_RE = re.compile(r"^([A-Za-z_][\w.]*)\s*(?:([+-])\s*(0x[0-9A-Fa-f]+|\d+))?$")
SYM_RE = re.compile(r"^\s*([\w.]+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;\s*(?://(.*))?")
# GS registers that carry packed integers, never addresses (GS manual 7.1).
GS_REGS = {
    0x00: "PRIM", 0x01: "RGBAQ", 0x02: "ST", 0x03: "UV", 0x04: "XYZF2", 0x05: "XYZ2",
    0x06: "TEX0_1", 0x07: "TEX0_2", 0x08: "CLAMP_1", 0x09: "CLAMP_2", 0x0A: "FOG",
    0x0C: "XYZF3", 0x0D: "XYZ3", 0x14: "TEX1_1", 0x15: "TEX1_2", 0x16: "TEX2_1",
    0x17: "TEX2_2", 0x18: "XYOFFSET_1", 0x19: "XYOFFSET_2", 0x1A: "PRMODECONT",
    0x1B: "PRMODE", 0x1C: "TEXCLUT", 0x22: "SCANMSK", 0x34: "MIPTBP1_1",
    0x35: "MIPTBP1_2", 0x36: "MIPTBP2_1", 0x37: "MIPTBP2_2", 0x3B: "TEXA",
    0x3D: "FOGCOL", 0x3F: "TEXFLUSH", 0x40: "SCISSOR_1", 0x41: "SCISSOR_2",
    0x42: "ALPHA_1", 0x43: "ALPHA_2", 0x44: "DIMX", 0x45: "DTHE", 0x46: "COLCLAMP",
    0x47: "TEST_1", 0x48: "TEST_2", 0x49: "PABE", 0x4A: "FBA_1", 0x4B: "FBA_2",
    0x4C: "FRAME_1", 0x4D: "FRAME_2", 0x4E: "ZBUF_1", 0x4F: "ZBUF_2",
    0x50: "BITBLTBUF", 0x51: "TRXPOS", 0x52: "TRXREG", 0x53: "TRXDIR",
    0x54: "HWREG", 0x60: "SIGNAL", 0x61: "FINISH", 0x62: "LABEL",
}


def load_symbols(root, unit):
    """addr -> [(name, kind, size)] and name -> addr for one unit (+ main's)."""
    by_addr, by_name = {}, {}
    units = [unit] if unit == "main" else [unit, "main"]
    for u in units:
        d = os.path.join(root, "build", u)
        for fn, auto in (("symbol_addrs.txt", False), ("undefined_syms_auto.txt", True)):
            p = os.path.join(d, fn)
            if not os.path.exists(p):
                continue
            for line in open(p):
                m = SYM_RE.match(line)
                if not m:
                    continue
                name, addr, meta = m.group(1), int(m.group(2), 16), m.group(3) or ""
                if name in by_name:
                    continue
                km = re.search(r"type:(\w+)", meta)
                sm = re.search(r"size:(0x[0-9A-Fa-f]+|\d+)", meta)
                kind = "auto" if auto else (km.group(1) if km else "data")
                size = int(sm.group(1), 0) if sm else 0
                by_name[name] = addr
                by_addr.setdefault(addr, []).append((name, kind, size))
    # every data label splat emitted (strings, tables) is a real object start
    for u in units:
        for dp, _, fs in os.walk(os.path.join(root, "build", u, "asm")):
            if "/data" not in dp:
                continue
            for f in fs:
                if not f.endswith(".s"):
                    continue
                pend = None
                for line in open(os.path.join(dp, f), encoding="latin-1"):
                    s = line.strip()
                    if s.startswith("dlabel"):
                        pend = s.split(None, 1)[1].split(",")[0].strip()
                        continue
                    if pend:
                        m = re.match(r"/\*\s*[0-9A-Fa-f]+\s+([0-9A-Fa-f]+)", s)
                        if m:
                            by_addr.setdefault(int(m.group(1), 16), []).append((pend, "dlabel", 0))
                            pend = None
    return by_addr, by_name


def host_symbols(host):
    """name -> set of nm types in the host binary."""
    out = {}
    if not host or not os.path.exists(host):
        return None
    r = subprocess.run(["nm", host], capture_output=True, text=True)
    for line in r.stdout.splitlines():
        p = line.split()
        if len(p) == 3:
            out.setdefault(p[2], set()).add((p[1], int(p[0], 16)))
    return out


def c_int_labels(root):
    """Labels the C code declares as integer/u64/u16 arrays (not pointers)."""
    ints = set()
    decl = re.compile(r"\b(?:extern\s+)?(?:const\s+)?(?:unsigned\s+)?"
                      r"(u64|u32|u16|u8|s64|s32|s16|s8|long\s+long|int|short|char|float|"
                      r"ps2_u64|ps2_u128|u_long|u_int|u_short|u_char|sceGifPacked|"
                      r"sceGsTex0|sceGsClamp|sceGsTex1)\s+([A-Za-z_]\w*)\s*\[")
    for dp, _, fs in os.walk(os.path.join(root, "src")):
        for f in fs:
            if f.endswith((".c", ".h")):
                try:
                    t = open(os.path.join(dp, f), encoding="latin-1").read()
                except OSError:
                    continue
                for m in decl.finditer(t):
                    ints.add(m.group(2))
    return ints


def is_floatish(v):
    e = (v >> 23) & 0xFF
    return v != 0 and 0x70 <= e <= 0x8A


def is_plain(v):
    return v < 0x10000 or v >= 0xFFFF0000 or is_floatish(v)


def scan_file(src):
    """-> list of labels: (name, [(vram, value, ref_or_None)])"""
    labels, cur = [], None
    raw_mode = set()
    for line in open(src, encoding="latin-1"):
        s = line.strip()
        if s.startswith("dlabel"):
            name = s.split(None, 1)[1].split(",")[0].strip()
            cur = (name, [])
            labels.append(cur)
            continue
        m = LINE_RE.search(line)
        if not m:
            continue
        if cur is None:
            cur = ("<anon>", [])
            labels.append(cur)
        vram = int(m.group(2), 16)
        b = bytes.fromhex(m.group(3))
        val = int.from_bytes(b, "little")
        arg = re.sub(r"/\*.*?\*/", "", m.group(4)).strip()
        ref = arg if re.match(r"[A-Za-z_]", arg) and not arg.startswith(".L") else None
        cur[1].append((vram, val, ref, b))
    return labels


def split_ref(ref):
    rm = REF_RE.match(ref)
    if not rm:
        return None, 0
    sym, sign, off = rm.group(1), rm.group(2), rm.group(3)
    return sym, (int(off, 0) * (-1 if sign == "-" else 1)) if off else 0


def is_hit(sym, off, val, by_addr, code_names):
    """A real pointer: its value is the start of a symbol that is a real
    object (sized entry in symbol_addrs) or that code refers to by name."""
    for name, kind, size in by_addr.get(val, []):
        if kind != "auto" or name in code_names:  # dlabel/data/func
            return True
    return False


_STARTS = {}


def interior(sym, off, val, by_addr):
    """val lies inside the real (non-auto) object `sym` starts."""
    key = id(by_addr)
    if key not in _STARTS:
        _STARTS[key] = sorted(a for a, ents in by_addr.items() if any(k != "auto" for _, k, _ in ents))
    starts = _STARTS[key]
    base = (val - off) & 0xFFFFFFFF
    ents = [e for e in by_addr.get(base, []) if e[1] not in ("auto", "func")]
    if not ents or off <= 0:
        return False
    size = max(e[2] for e in ents)
    if not size:
        import bisect
        j = bisect.bisect_right(starts, base)
        size = (starts[j] - base) if j < len(starts) else 0
    return off < size


def word_hit(w, by_addr, code_names):
    sym, off = split_ref(w[2])
    return sym is not None and is_hit(sym, off, w[1], by_addr, code_names)


def code_names_of(root):
    """Identifiers used by C (src/) or by function asm (nonmatchings)."""
    names = set()
    ident = re.compile(r"\b[A-Za-z_]\w*\b")
    for top in [os.path.join(root, "src")] + [os.path.join(root, "build", u, "asm") for u in G.UNITS]:
        for dp, _, fs in os.walk(top):
            if "/data" in dp:
                continue
            for f in fs:
                if f.endswith((".c", ".h", ".s", ".inc")):
                    try:
                        names.update(ident.findall(open(os.path.join(dp, f), encoding="latin-1").read()))
                    except OSError:
                        pass
    return names


def score_word(i, words, label, by_addr, cints, code_names):
    vram, val, ref, b = words[i]
    sym, off = split_ref(ref)
    if sym is None:
        return None
    target = (val - off) & 0xFFFFFFFF
    score, why = 0, []
    # GIF context (quadword layout as on the PS2)
    wmap = {w[0]: w for w in words}
    q = vram & ~15
    slot = (vram >> 2) & 3
    w2, w3 = wmap.get(q + 8), wmap.get(q + 12)
    if slot in (0, 1) and w2 and w3 and w2[2] is None and w3[1] == 0 and w2[1] in GS_REGS and w2[1] != 0:
        score += 6
        why.append("A+D %s data %s" % (GS_REGS[w2[1]], "lo" if slot == 0 else "hi"))
    elif slot == 1:
        w0 = wmap.get(q)
        if w0 and w0[2] is None and (w0[1] & 0x7FFF) and (w0[1] & 0xFFFF7FFF) < 0x8000 \
                and w2 and w2[1] in (0x0E,) and (w3 is None or w3[1] == 0):
            score += 6
            why.append("GIFtag high word")
    if is_hit(sym, off, val, by_addr, code_names):
        score -= 4
        why.append("value is the start of a symbol the code uses")
    else:
        score += 2
        why.append("no code-used symbol at value")
        if off and val % 4 == 0 and interior(sym, off, val, by_addr):
            score -= 3
            why.append("aligned interior pointer %s+0x%x" % (sym, off))
        elif off:
            score += 1 if off % 4 == 0 else 2
            why.append("only reachable as %s+0x%x" % (sym, off))
        syms_at = [x for x in by_addr.get(target, []) if x[1] != "auto"]
        if off and syms_at and syms_at[0][1] == "func":
            score += 2
            why.append("offset into code")
    if (val & 0xFFFF) == 0 or ((val >> 16) < 0x400 and (val & 0xFFFF) < 0x400):
        score += 1
        why.append("u16-pair shape")
    if val & 3:
        score += 2
        why.append("unaligned value")
    nb = [w for j, w in enumerate(words) if j != i and abs(j - i) <= 4]
    if nb:
        hits = sum(1 for w in nb if w[2] and word_hit(w, by_addr, code_names))
        plain = sum(1 for w in nb if (not w[2] and w[1] and is_plain(w[1])) or
                    (w[2] and not word_hit(w, by_addr, code_names)))
        if hits >= len(nb) * 0.5:
            score -= 3
            why.append("pointer-table neighbours")
        elif plain >= len(nb) * 0.5:
            score += 2
            why.append("int/float/miss neighbours")
    if label in cints:
        score += 2
        why.append("C declares %s as int array" % label)
    return score, why, sym, off


def audit(root, host, units=G.UNITS):
    hs = host_symbols(host)
    cints = c_int_labels(root)
    code_names = code_names_of(root)
    rows = []
    for unit in units:
        unit_dir = os.path.join(root, "build", unit)
        ld = os.path.join(unit_dir, G.LD_SCRIPT.get(unit, unit + ".elf.modern.ld"))
        srcs = G.ninja_sources(unit_dir)
        by_addr, _ = load_symbols(root, unit)
        seen = set()
        for obj, _sec in G.ld_objects(ld, stop_at_section="ov02" if unit == "main" else None):
            if obj in seen or obj not in srcs:
                continue
            seen.add(obj)
            for label, words in scan_file(os.path.join(unit_dir, srcs[obj])):
                prev = None
                for i, (vram, val, ref, b) in enumerate(words):
                    pb, prev = prev, b
                    if not ref:
                        continue
                    status = "reloc"
                    if label in G.NO_RELOC_LABELS or G.string_tail(b, pb, i == 0):
                        status = "raw"
                    r = score_word(i, words, label, by_addr, cints, code_names)
                    if r is None:
                        continue
                    score, why, sym, off = r
                    if hs is not None:
                        ent = hs.get(sym)
                        if ent and all(t in "aA" and (v + off) & 0xFFFFFFFF == val for t, v in ent):
                            status = "abs"
                    rows.append((score, unit, vram, label, vram - words[0][0], val, ref, status, why))
    rows.sort(key=lambda r: (-r[0], r[1], r[2]))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.abspath(os.path.join(os.path.dirname(__file__), "../..")))
    ap.add_argument("--host", default=None, help="host xsg binary (to drop absolute defsyms)")
    ap.add_argument("--min", type=int, default=3)
    ap.add_argument("--all", action="store_true", help="also list raw/absolute words")
    a = ap.parse_args()
    host = a.host or os.path.join(a.root, "port/build/xsg")
    rows = audit(a.root, host)
    n = {"reloc": 0, "raw": 0, "abs": 0}
    shown = 0
    for score, unit, vram, label, loff, val, ref, status, why in rows:
        n[status] += 1
        if (status != "reloc" and not a.all) or score < a.min:
            continue
        shown += 1
        print("%3d %-4s %08X %s+0x%x %08X %-32s %s %s" % (
            score, unit, vram, label, loff, val, ref, status, "; ".join(why)))
    print("# symbolised words: %d relocated on host, %d emitted raw, %d absolute; "
          "%d relocated with score >= %d" % (n["reloc"], n["raw"], n["abs"], shown, a.min),
          file=sys.stderr)


if __name__ == "__main__":
    main()

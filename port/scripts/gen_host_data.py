#!/usr/bin/env python3
"""Convert the PS2 build's data-only MIPS assembly into i386 GNU as input.

The PS2 link (build/<unit>/<unit>.ld) places, per output section, a mix of
  * compiled C objects        build/scaffold/src/...      (host: compiled from src/)
  * splat data pieces         build/asm/.../X.{data,rodata,bss,...}.o
  * data carves               build/carve/<unit>/X.rodata.o
  * blobs/assets              ELF header etc. (skipped; VU microcode kept)
This script walks each unit's linker script in order, picks every non-C data
object, maps it back to its .s source and rewrites it for `as --32`:

  .word -> .long (i386 .word is 16-bit), .align n -> .balign 2^n (MIPS gas
  .align is a power of two), splat macros (dlabel/enddlabel/nonmatching) are
  expanded inline, `.L` jump-table targets that point into MIPS text become 0,
  .incbin paths are made absolute.

Sections: main keeps .data/.rodata/.bss.  Overlay data goes to xsgd_<unit>
(data), xsgr_<unit> (rodata) and xsgb_<unit> (bss) so the runtime can find each
overlay's bounds through __start_/__stop_ symbols and re-initialise it.

Output: <out>/<unit>/*.s and <out>/data_sources.cmake.  Reads only build/
(generated from the user's own disc by configure.py); writes nothing there.

  port/scripts/gen_host_data.py [--root REPO] [--out port/build/data]
"""
import argparse
import os
import re
import sys

UNITS = ["main", "ov01", "ov02", "ov10", "ov11", "ov12"]
# Linker scripts the PS2 build really links with (see each unit's build.ninja).
LD_SCRIPT = {"main": "main.elf.ld"}
# Objects whose data the host does not want: C objects (compiled from src/),
# SDK/libc text, the ELF/crt blobs, and main's embedded copy of ov02.
SKIP_PREFIXES = ("build/c/", "build/expected/", "build/hasm/", "build/scaffold/",
                 "build/assets/", "build/ov02/")
SKIP_BLOBS = {"reginfo", "ctors", "dtors", "eh_frame", "elf_header"}

REF_RE = re.compile(r"(build/[^\s(]+\.o)\(([^)]+)\)")
COMMENT_RE = re.compile(r"/\*.*?\*/")
SEC_RE = re.compile(r'^\s*\.section\s+([.\w]+)')
# splat line comment: /* rom vram BYTES */ (BYTES as stored, big-endian hex)
RAW_RE = re.compile(r"/\*\s*[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})\s*\*/")
# Integer tables whose words splat symbolised because they happen to look like
# RAM addresses (asVramBase: u16 pairs such as 0x0000,0x008C read as
# D_008C0000).  Harmless on the PS2, but the host would relocate them to its own
# address of that symbol (xglRenderSetReso then took ZBP 0x130 from the low
# half of the host pointer and the depth buffer landed inside the 448-line
# frame buffers).  Inside these labels every word is emitted as its raw bytes.
NO_RELOC_LABELS = {"asVramBase"}
STR_CHARS = set(b"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_.-+%:/\\ ")


def string_tail(raw, prev_raw, at_label):
    """True if a symbolised .word is really the NUL-terminated tail of a
    string: splat turns bytes like "006\0" (0x00363030) into `Sym + off`
    whenever they happen to land inside RAM, which is harmless on the PS2
    (same bytes) but relocates on the host.  Real pointers into main's
    .rodata/.sdata (0x4Cxxxx/0x4Dxxxx, i.e. 3rd byte 'L'/'M') are kept."""
    s = raw.split(b"\0")[0]
    if len(s) < 2 or any(c not in STR_CHARS for c in s) or raw[len(s):] != b"\0" * (4 - len(s)):
        return False
    if raw[2] in (0x4C, 0x4D):
        return False
    return at_label or (prev_raw is not None and all(0x20 <= c < 0x7F for c in prev_raw))


def ld_objects(ld_path, stop_at_section=None):
    """Ordered (object, input-section) data refs from a splat linker script."""
    out = []
    with open(ld_path) as f:
        for line in f:
            if stop_at_section and re.match(r"\s*\.?%s\s" % re.escape(stop_at_section), line):
                break
            code = COMMENT_RE.sub("", line)
            for obj, sec in REF_RE.findall(code):
                if sec == ".text" or obj.startswith(SKIP_PREFIXES):
                    continue
                if "/blobs/" in obj and os.path.basename(obj)[:-2] in SKIP_BLOBS:
                    continue
                out.append((obj, sec))
    return out


def host_section(unit, sec):
    kind = {".data": "d", ".sdata": "d", ".rodata": "r", ".lit4": "r",
            ".bss": "b", ".sbss": "b", ".vudata": "r"}[sec]
    if unit == "main":
        return {"d": ".data", "r": ".rodata", "b": ".bss"}[kind]
    return "xsg%s_%s" % (kind, unit)


def section_directive(name):
    if name in (".data", ".rodata", ".bss"):
        return "\t.section %s" % name
    flags = {"d": '"aw",@progbits', "r": '"a",@progbits', "b": '"aw",@nobits'}[name[3]]
    return "\t.section %s,%s" % (name, flags)


def convert(src, unit, unit_dir):
    # linker/common.bss + scommon.sbss hold the PS2 link's COMMON symbols
    # (ee-gcc -fcommon tentative definitions).  The host compiles with
    # -fno-common, so the C objects define them; keep these as weak fallbacks.
    weak = "/linker/" in src
    text = open(src, encoding="latin-1").read()
    defined = set(re.findall(r"^\s*(\.L\w+):", text, re.M))
    out = ["# generated by port/scripts/gen_host_data.py from %s" % os.path.relpath(src)]
    stats = {"zeroed_L": 0, "strings": 0}
    pending_align = False
    prev_raw = None
    at_label = False
    no_reloc = False
    for raw in text.splitlines():
        if pending_align:
            # First addressed line after a .section: start this piece at the
            # same address modulo 16 as on the PS2, so 16-byte quadword data
            # keeps its alignment (host gcc uses movaps for ps2_u128).
            m = re.match(r"\s*/\*\s*([0-9A-Fa-f]+)(?:\s+([0-9A-Fa-f]+))?", raw)
            if m:
                vram = int(m.group(2) or m.group(1), 16)
                # insert right after the .section line, ahead of any label
                # that precedes the first addressed line
                out.insert(pending_align, "\t.balign 16\n\t.skip %d" % (vram % 16))
                pending_align = False
        rm = RAW_RE.search(raw)
        cur_raw = bytes.fromhex(rm.group(1)) if rm else None
        line = COMMENT_RE.sub("", raw).strip()
        if not line or line.startswith("#"):
            continue
        was_label, at_label = at_label, line.startswith("dlabel")
        if line.startswith("nonmatching"):
            at_label = was_label
        last_raw, prev_raw = prev_raw, cur_raw
        m = SEC_RE.match(line)
        if m:
            out.append(section_directive(host_section(unit, m.group(1))))
            pending_align = len(out)
            continue
        if line.startswith(".include"):
            continue
        parts = line.split(None, 1)
        op, arg = parts[0], (parts[1] if len(parts) > 1 else "")
        if op == "nonmatching":
            continue
        if op == "dlabel":
            name = arg.split(",")[0].strip()
            vis = arg.split(",")[1].strip() if "," in arg else "global"
            if weak and vis == "global":
                vis = "weak"
            out += ["\t.%s %s" % (vis, name), "\t.type %s,@object" % name, "%s:" % name]
            no_reloc = name in NO_RELOC_LABELS
        elif op == "enddlabel":
            name = arg.strip()
            out.append("\t.size %s, . - %s" % (name, name))
            no_reloc = False
        elif op == ".size":
            continue  # enddlabel already sized it
        elif op == ".align":
            out.append("\t.balign %d" % (1 << int(arg, 0)))
        elif op == ".word" and cur_raw is not None and no_reloc:
            out.append("\t.balign 4\n\t.byte %s" % ", ".join("0x%02x" % c for c in cur_raw))
        elif op == ".word" and cur_raw is not None and re.match(r"[A-Za-z_]", arg) \
                and not arg.startswith(".L") and string_tail(cur_raw, last_raw, was_label):
            stats["strings"] += 1
            out.append("\t.balign 4\n\t.byte %s" % ", ".join("0x%02x" % c for c in cur_raw))
        elif op == ".word":
            vals = []
            for v in arg.split(","):
                v = v.strip()
                if v.startswith(".L") and v.split()[0] not in defined:
                    stats["zeroed_L"] += 1
                    v = "0"
                vals.append(v)
            out.append("\t.balign 4\n\t.long %s" % ", ".join(vals))
        elif op == ".short":
            out.append("\t.balign 2\n\t.short %s" % arg)
        elif op == ".float":
            out.append("\t.balign 4\n\t.float %s" % arg)
        elif op in (".byte", ".ascii", ".asciz", ".space"):
            out.append("\t%s %s" % (op, arg))
        elif op == ".incbin":
            path = arg.strip().strip('"')
            out.append('\t.incbin "%s"' % os.path.abspath(os.path.join(unit_dir, path)))
        elif line.endswith(":"):
            out.append(line)
        else:
            raise SystemExit("%s: unhandled line: %r" % (src, raw))
    return "\n".join(out) + "\n", stats


def ninja_sources(unit_dir):
    """object -> assembly source, from `build X.o: as Y.s` edges."""
    srcs = {}
    with open(os.path.join(unit_dir, "build.ninja")) as f:
        for line in f:
            m = re.match(r"build (\S+\.o): as (\S+\.s)", line)
            if m:
                srcs[m.group(1)] = m.group(2)
    return srcs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.abspath(os.path.join(os.path.dirname(__file__), "../..")))
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    out_root = a.out or os.path.join(a.root, "port/build/data")
    srcs = []
    total = {"files": 0, "zeroed_L": 0, "strings": 0}
    for unit in UNITS:
        unit_dir = os.path.join(a.root, "build", unit)
        ld = os.path.join(unit_dir, LD_SCRIPT.get(unit, unit + ".elf.modern.ld"))
        asm_src = ninja_sources(unit_dir)
        if not os.path.exists(ld):
            sys.exit("missing %s: run the PS2 configure/build first" % ld)
        # main.ld also embeds a copy of ov02 at 0xA00000; the ov02 unit owns it.
        refs = ld_objects(ld, stop_at_section="ov02" if unit == "main" else None)
        seen = set()
        os.makedirs(os.path.join(out_root, unit), exist_ok=True)
        for obj, sec in refs:
            if obj in seen:
                continue
            seen.add(obj)
            if obj not in asm_src:
                print("gen_host_data: %s: %s has no `as` source, skipped" % (unit, obj))
                continue
            rel = asm_src[obj]
            src = os.path.join(unit_dir, rel)
            body, st = convert(src, unit, unit_dir)
            dst = os.path.join(out_root, unit, rel.replace("/", "__"))
            old = open(dst).read() if os.path.exists(dst) else None
            if old != body:
                with open(dst, "w") as f:
                    f.write(body)
            srcs.append(dst)
            total["files"] += 1
            total["zeroed_L"] += st["zeroed_L"]
            total["strings"] += st["strings"]
    with open(os.path.join(out_root, "data_sources.cmake"), "w") as f:
        f.write("set(XSG_DATA_SOURCES\n%s)\n" % "".join("  %s\n" % s for s in srcs))
    print("gen_host_data: %(files)d files, %(zeroed_L)d text-label refs zeroed, "
          "%(strings)d string words unsymbolised" % total)


if __name__ == "__main__":
    main()

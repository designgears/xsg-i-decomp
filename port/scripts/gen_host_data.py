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
# Labels whose words are all integers although splat symbolised some of them
# (their value happens to fall in the RAM range).  Harmless on the PS2 (the
# linker writes back the same value), but the host would relocate them to its
# own address of that symbol.  Inside these labels every word is emitted as
# its raw bytes.  Each Stage 11 entry was found by audit_data_relocs.py: no
# symbolised word in the label is the start of a real object, and the C type
# of the label is an integer/struct/char array (STAGE11_DATA.md).
NO_RELOC_LABELS = {
    # asVramBase: u16 pairs such as 0x0000,0x008C read as D_008C0000.
    # xglRenderSetReso took ZBP 0x130 from the low half of the host pointer
    # and the depth buffer landed inside the 448-line frame buffers (Stage 8).
    "asVramBase": "u16 FBP/ZBP pairs (xgl_render)",
    # Head_9 (act_3, the drop-shadow begin packet) and TestPrim_0_004A8D00
    # (xgl_packet) carry CLAMP_1 0x6FC_007FC00A, whose low word splat read as
    # D_007FC00A: relocated, REGION_CLAMP got MINU 708 > MAXU 511 and the
    # shadow pass sampled the wrong texels (Stage 10).
    "Head_9": "GIF A+D CLAMP_1 0x6FC_007FC00A (act_3)",
    "TestPrim_0_004A8D00": "GIF A+D CLAMP_1 0x6FC_007FC00A (xgl_packet)",
    # --- Stage 11: GIF packets (A+D register data words) -------------------
    "TestPrim_0_00362790":   'u64 packet, A+D CLAMP_1 low word 0x007FC005 (map_1)',
    "Head_13":               'packet, A+D CLAMP_1 low word 0x007FC005 (game_defocus)',
    "TestEnv_0_00369BA0":    'u32[28] env packet, CLAMP_1 0x003FC000 (game_radar)',
    "TestEnv_1_0036DDB0":    'sprite env packet, CLAMP_1 0x003FDFF0 (karakama_test)',
    "TestEnv_49":            'env packet, CLAMP_1 0x003FFFF0 (endou_test)',
    "TestEnv_56":            'env packet, CLAMP_1 0x003FDFF0 (endou_test)',
    "TestEnv_59":            'env packet, CLAMP_1 0x003FDFF0 (endou_test)',
    "TestEnv_60":            'env packet, CLAMP_2 0x007FDFF0 (endou_test)',
    "sPoint_0":              'int[] GIF packet, RGBAQ 0x005151EE (init_uwamono_sys)',
    # --- Stage 11: integer / struct tables and copied MIPS code -----------
    "others_uv":             'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "others2_uv":            'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "ether_uv":              'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "icom_uv":               'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "pad_uv":                'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "pad2_uv":               'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "face00_uv":             'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "face01_uv":             'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "face02_uv":             'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "face03_uv":             'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "face04_uv":             'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "face_agws_uv":          'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "seg_uv":                'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "btl_msg_uv":            'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "btl_msg_uv2":           'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "umn_uv":                'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "thumnail_uv":           'u16 u,v,w,h rectangle table, unsigned char[] (end_print)',
    "_srsChrEfTbl2":         'SrsChrEf2Entry {short chrNo, min, max}[] (srs)',
    "TrapKind_t_0":          'TrapKindEntry.flags 0x00400000/0x00800000 (init_map_trap)',
    "line_34":               'EndouPrintPoint[6] {short x; u16 y; int depth; u8 color[4]}, RGBA 0x00806060',
    "d_42":                  'EndouPrintPoint[7], RGBA 0x00804040 (endou_test)',
    "D_004C92E8":            'const unsigned char[28] range table, u16 pairs (menu_tec)',
    "BmpHeader":             'u8[0x36] BMP file header, bfOffBits 0x36 (tya)',
    "headerALL":             'u8[0x26F] JPEG header and Huffman tables (xgl_jpeg)',
    "list":                  '{int id; char name[12]} table, name tail bytes (hair_test)',
    "two_over_pi_004D6220":  'libm e_rem_pio2 int32 2/pi digit table',
    "osdsrc":                'MIPS instructions copied into the kernel (InitExecPS2)',
    "srcfile":               'MIPS instructions copied into the kernel (InitExecPS2)',
    "tlbsrc":                'MIPS instructions, TLB miss handler (set_tlb_handler)',
    "kernelTLB":             'TLB entries PageMask/EntryHi/EntryLo (set_tlb_handler)',
    "defaultTLB":            'TLB entries (set_tlb_handler)',
    "extendTLB":             'TLB entries (set_tlb_handler)',
    "eenull":                'MIPS instructions of the EE null thread (glue_2)',
    # --- Stage 11: strings whose NUL-terminated tail looks like a RAM address;
    # their control codes / SJIS bytes are outside string_tail()'s charset.
    "D_004C21C8":            '"\\x0b\\x0c\\x80\\0\\0OFF" (db_light_write)',
    "D_004C21D8":            '"\\x0b\\x0c\\x80\\0\\0DIRECTIONAL" (db_light_write)',
    "D_004C2200":            '"\\x0b\\x0c\\x80\\0\\0POINT" (db_light_write)',
    "D_004CBC58":            '"\\x0b\\x0c\\x80\\0\\0CollisionHit!!" (colli_test)',
    "D_004C2BE0":            'SJIS text with 0x19/0x0d control codes (endou_test)',
    "D_004C6E70":            '"\\x0c\\x80\\x80\\x80R. Arm\\n..." body-part names (menu_para_pt_rate_get)',
    "D_004C8690":            '"List\\x03\\x9cE.Pts" column header (menu_ether)',
    "D_004C86A0":            'column header string (menu_ether)',
    "D_004C90F0":            '"List\\x03P\\x03>Eqp" column header (menu_tec)',
    "D_004CA818":            '"\\r\\0<SJIS>%4d" (pause_menu_page_uwamono)',
    "D_004CA8E8":            '"\\r\\0<SJIS> %6dG" gold counter (pause_menu_page_uwamono)',
    "D_004DA580":            '"\\x0bVECTOR" (db_light_write)',
    "D_004DA670":            '"\\x0bCW" shop heading (endou_test)',
    "D_004DA678":            'shop heading (endou_test)',
    "D_004DA680":            'shop heading (endou_test)',
    "D_004DA688":            '"\\x0bAW" shop heading (endou_test)',
    "D_004DA690":            'shop heading (endou_test)',
    "D_004DA698":            'shop heading (endou_test)',
    "D_004DA6A0":            'shop heading (endou_test)',
    "D_004DA6A8":            'shop heading (endou_test)',
    "D_004DA9D8":            '"\\x01Hp" battle window label (e_battle_win_open)',
    "D_004DAE00":            '"\\x01Weight" (menu_para_pt_rate_get)',
    "D_004DAF98":            '"\\x01Weight" (menu_agws_para_set)',
    "D_004DB188":            '"\\x01Target" (menu_shop)',
    "D_004DB1A8":            '"\\x01Result" (menu_shop)',
    "D_004DB2D8":            '"\\x01Wt" (menu_ether)',
    "D_004DB310":            '"\\x0b... %2d" debug printf format (menu_ether)',
    "D_004DB318":            '"\\x0b... %2d" debug printf format (menu_ether)',
    "D_004DB320":            '"\\x0b... %2d" debug printf format (menu_ether)',
    "D_004DB328":            '"\\x0b... %2d" debug printf format (menu_ether)',
    "D_004DB330":            '"\\x0b... %2d" debug printf format (menu_ether)',
    "D_004DB338":            '"\\x0b... %2d" debug printf format (menu_ether)',
    "D_004DB340":            '"\\x0b... %2d" debug printf format (menu_ether)',
    "D_004DB4F8":            '"\\x0b... %2d" debug printf format (menu_ether)',
    "D_004DB500":            '"\\x0b... %2d" debug printf format (menu_ether)',
    "D_004DB508":            '"\\x0b... %2d" debug printf format (menu_ether)',
    "D_004DB510":            '"\\x0b... %2d" debug printf format (menu_ether)',
    "D_004DB5B0":            '"\\x0b... %2d" debug printf format (menu_ether)',
    "D_004DB578":            '" P@0\\0p`" colour/position bytes (menu_system)',
    "D_004DB5A8":            '"\\x0bsystem" (menu_system)',
    "D_004DBBB8":            '"???" placeholder (sef_print)',
    "D_004DC2C8":            '"\\x0b%s" (xgl_menu)',
    "text_17":               'EUC/SJIS text with control codes (endou_test, char text_17[])',
}
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

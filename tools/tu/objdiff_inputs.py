"""Prepare ELF object pairs for objdiff, without calculating report scores.

Targets contain original assembly scaffolds and original TU data. Bases are fresh C
compilations with SKIP_ASM: neither fallback nor standalone assembly earns C
credit. IOP originals have no base. All binary inputs remain local.
"""
from concurrent.futures import ThreadPoolExecutor
import json
import re
from pathlib import Path
import struct
import subprocess

import coverage_report as cov
from tu import verify_report as verification
from tu.elfinfo import Elf
from tu.noaslr import NOASLR
from tu import data_carve, target_layout, target_symbolize

LITERAL_SECTIONS = ('.lit4', '.lit8')
from tu.toolchain import Toolchain


def run(argv, cwd):
    subprocess.run(['timeout', '-k', '5', '30', *map(str, argv)], cwd=cwd,
                   check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def compile_source(root, unit_dir, source, output, ccdir, assembler, flags, skip_asm,
                   as_flags=(), keep_locals=False):
    common = [ccdir / 'ee-gcc', f'-B{ccdir}/', '-nostdinc', '-fno-builtin']
    preprocessed, assembly = Path(str(output) + '.i'), Path(str(output) + '.s')
    run([*common, '-E', *flags, *(['-DSKIP_ASM'] if skip_asm else []), '-Iinclude', '-I.',
         f'-I{root / "include"}', f'-I{source.parent}', source, '-o', preprocessed], unit_dir)
    run([*NOASLR, *common, '-S', *flags, preprocessed, '-o', assembly], unit_dir)
    gflag = next(flag for flag in flags if flag.startswith('-G'))
    # as_flags: the TU's extra ee-as options (config/tu/as-flags.json), as the
    # published build passes them (tools/tu/ninja_main.py, ninja_ovl.py).
    # keep_locals (-L): keep the compiler's local labels ($LC string/float
    # literals) as symbols, so objdiff measures anonymous data byte for byte
    # instead of seeing a section with no symbols. Bytes are unchanged.
    run([assembler, '-EL', '-m5900', '-mabi=eabi', gflag, *as_flags,
         *(['-L'] if keep_locals else []), '-I.',
         '-Iinclude', '-o', output, assembly], unit_dir)
    return assembly


def build_target(unit_dir, stem, unit):
    candidates = ([f'build/expected/{stem}.o', f'build/hasm/{stem}.o']
                  if unit == 'main' else [f'build/src/{unit}/{stem}.o',
                                         f'build/scaffold/src/{unit}/{stem}.o'])
    path = next((unit_dir / p for p in candidates if (unit_dir / p).is_file()), None)
    verification.require(path is not None, f'{unit}/{stem}: missing verified build object')
    return path


def functions(path):
    return {s.name: s.size for s in Elf(path.read_bytes()).symbols
            if s.type == 2 and s.shndx not in (0, 0xfff1) and s.size}


def strip_debug(prefix, source, destination, cwd, iop_reference=False):
    # Old GCC emits ECOFF debug records that objdiff's MIPS reader cannot parse
    # reliably. Remove debug records only; verify every allocated section and
    # function identity survives unchanged. No instruction/data normalization.
    before = Elf(source.read_bytes())
    # IRX stores module-relative data addresses/relocations. Make its data
    # sections section-relative for objdiff, keeping text addresses and every
    # FUNC identity. Target-only IOP references have no base and earn zero.
    options = (['--remove-section=.rel.text', '--remove-section=.rel.data', '--strip-all',
                *[f'--change-section-address={s.name}=0' for s in before.sections
                  if s.flags & 2 and not s.flags & 4],
                *[f'--keep-symbol={name}' for name in functions(source)]] if iop_reference else [])
    run([str(prefix) + 'objcopy', '--strip-debug', *options, source, destination], cwd)
    after = Elf(destination.read_bytes())
    allocated = lambda elf: {s.name: (s.type, s.flags,
                             0 if iop_reference and not s.flags & 4 else s.addr,
                             s.size, elf.section_bytes(s))
                             for s in elf.sections if s.flags & 2}
    verification.require(allocated(before) == allocated(after) and
                         functions(source) == functions(destination),
                         f'{source}: debug stripping changed report inputs')
    identity = lambda elf: {(s.name, s.value, s.size, s.bind, s.type)
                           for s in elf.symbols if s.type == 2 and s.size}
    verification.require(identity(before) == identity(after), f'{source}: FUNC identity changed')


def pair_verified_names(prefix, target, base_elf, verified, cwd):
    """Give a verified function's target symbol the C object's name and binding.

    A scaffold (INCLUDE_ASM) build of an overlay TU names a file-local function
    by its unique splat label (`makeHermiteCoord_00A36F28`, GLOBAL), while the
    C object defines it as `static makeHermiteCoord` (LOCAL). objdiff pairs
    functions by name, so it never compares them. verify_report has already
    proven the linked bytes identical; this only renames the report-local copy
    so the pair is measured. Only same-offset, same-size pairs are renamed.
    """
    base = {s.name: s for s in base_elf.symbols
            if s.type == 2 and s.shndx not in (0, 0xfff1) and s.size}
    elf = Elf(Path(target).read_bytes())
    have = {s.name: s for s in elf.symbols
            if s.type == 2 and s.shndx not in (0, 0xfff1) and s.size}
    args = []
    for function in verified:
        old = function['name']
        new = function.get('c_symbol', function['original_symbol'])
        if old == new or old not in have or new in have or new not in base:
            continue
        t, b = have[old], base[new]
        if (t.value, t.size) != (b.value, b.size):
            continue
        args += [f'--redefine-sym={old}={new}']
        if b.bind == 0 and t.bind != 0:
            args += [f'--localize-symbol={new}']
    if args:
        # objcopy applies --redefine-sym before --localize-symbol.
        run([str(prefix) + 'objcopy', *args, target], cwd)


def section_relative_data_relocs(target, base_elf):
    """Write the target's data pointers to its own unnamed items the way cc1 does.

    A pointer in the original data to an anonymous item (a string literal) is
    an R_MIPS_32 against a splat label (`D_00A0DC18`, addend 0) in the target,
    and against the section symbol with the item's offset stored in the word in
    the compiled object. Both resolve to the same address. Only relocations
    against a label the C object does not define are rewritten, to the
    section symbol with the label's offset added to the stored addend.
    """
    elf = Elf(Path(target).read_bytes())
    data = bytearray(elf.data)
    named = {s.name for s in base_elf.symbols if s.shndx}
    # offsets where the C object itself writes a section-relative pointer
    base_section_relative = {}
    for r in base_elf.sections:
        if r.type == 9 and r.info < len(base_elf.sections):
            for at in range(r.offset, r.offset + r.size, 8):
                off, inf = struct.unpack_from('<II', base_elf.data, at)
                if inf & 255 == 2 and base_elf.symbols[inf >> 8].type == 3:
                    base_section_relative.setdefault(base_elf.sections[r.info].name, set()).add(off)
    section_symbol = {s.shndx: s.index for s in elf.symbols if s.type == 3}
    for rel in elf.sections:
        if rel.type != 9:
            continue
        applies = elf.sections[rel.info]
        if not applies.flags & 2 or applies.flags & 4:
            continue
        for at in range(rel.offset, rel.offset + rel.size, 8):
            offset, info = struct.unpack_from('<II', data, at)
            symbol = elf.symbols[info >> 8]
            if (info & 255 != 2 or symbol.type == 3
                    or (symbol.name in named
                        and offset not in base_section_relative.get(applies.name, ()))
                    or symbol.shndx not in section_symbol):
                continue
            word = struct.unpack_from('<I', data, applies.offset + offset)[0]
            struct.pack_into('<I', data, applies.offset + offset,
                             (word + symbol.value) & 0xffffffff)
            struct.pack_into('<I', data, at + 4, (section_symbol[symbol.shndx] << 8) | 2)
    Path(target).write_bytes(bytes(data))


def size_bss_symbols(path, real_names=None, base_elf=None):
    """Size every BSS/SBSS label up to the next label (or the section end), on both sides.

    splat's `dlabel` + `.space` and cc1's `.lcomm` both leave uninitialised
    objects sizeless, and objdiff's BSS diff compares (offset, size) of sized
    symbols only: with none on either side every BSS section measured 100%
    whatever the C defined. Sized by the same rule on both objects, a BSS
    section matches only when the C defines objects at the original offsets.
    splat's `.NON_MATCHING` markers are left as they are (objdiff ignores them).
    """
    elf = Elf(Path(path).read_bytes())
    data = bytearray(elf.data)
    symtab = next(x for x in elf.sections if x.type == 2)
    for section in elf.sections:
        if section.type != 8 or not section.flags & 2:
            continue
        labels = sorted((y for y in elf.symbols if y.shndx == section.index and y.type != 3
                         and y.name and not y.name.endswith('.NON_MATCHING')),
                        key=lambda y: y.value)
        if base_elf is not None:
            bsec = [x for x in base_elf.sections if x.name == section.name]
            starts_c = {y.value for y in base_elf.symbols
                        if bsec and y.shndx == bsec[0].index and y.type != 3 and y.name}
            inner = [y for y in labels if y.value not in starts_c
                     and re.fullmatch(r'D_[0-9A-F]{8}', y.name)] if starts_c else []
            for y in inner:
                struct.pack_into('<I', data, symtab.offset + 16 * y.index + 8, 0)
            labels = [y for y in labels if y not in inner]
        if real_names is not None:
            # splat labels the code's references into the middle of an object
            # (`D_<addr>`); the original symbol table names the objects
            inner = [y for y in labels if y.name not in real_names
                     and re.fullmatch(r'D_[0-9A-F]{8}', y.name)]
            for y in inner:
                struct.pack_into('<I', data, symtab.offset + 16 * y.index + 8, 0)
            labels = [y for y in labels if y not in inner]
        starts = sorted({y.value for y in labels})
        for y in labels:
            nxt = next((v for v in starts if v > y.value), None)
            # the last object runs to the section end, which differs by the
            # link fill (up to the next object's 16-byte alignment): round it
            # to 16 bytes on both sides
            size = nxt - y.value if nxt is not None else (section.size - y.value + 15) & ~15
            struct.pack_into('<I', data, symtab.offset + 16 * y.index + 8, size)
            if y.type == 0:
                at = symtab.offset + 16 * y.index + 12
                data[at] = (data[at] & 0xf0) | 1          # STT_OBJECT
    Path(path).write_bytes(bytes(data))


def anchor_data_sections(prefix, base, cwd):
    """A local symbol at offset 0 of every data section of the compiled object.

    objdiff compares a data section only up to the end of its last symbol and
    from its first. cc1's literal pools (`.lit4`/`.lit8` from li.s/li.d) and
    leading anonymous items carry no symbol, so their bytes were never compared
    and the section could not be measured. An anchor at 0 (objdiff sizes it up
    to the next symbol) makes every byte count; it adds no credit by itself.
    """
    elf = Elf(Path(base).read_bytes())
    args = []
    for section in elf.sections:
        if section.type != 1 or not section.size or not section.flags & 2 or section.flags & 4:
            continue
        if any(s.shndx == section.index and s.type != 3 and s.value == 0 for s in elf.symbols):
            continue
        args.append(f'--add-symbol=$anchor{section.name}={section.name}:0,local,object')
    if args:
        run([str(prefix) + 'objcopy', *args, base], cwd)


def fold_constant_labels(target):
    """Undo splat's reading of data constants as addresses (`.word D_8000 + 0x1`).

    Where a data word's value is not an address of the loaded images, splat
    still emits it as `D_<hex> + addend` against an undefined absolute symbol
    (undefined_syms_auto.txt: `D_8000 = 0x8000`). The original holds the plain
    constant and the linked image is identical; the report's target carries an
    R_MIPS_32 the compiled C correctly lacks. Such a relocation is resolved into
    the word and removed. Only undefined `D_<hex>` symbols outside the EE image
    range [0x00100000, 0x02000000) are folded.
    """
    elf = Elf(Path(target).read_bytes())
    data = bytearray(elf.data)
    for rel in elf.sections:
        if rel.type != 9:
            continue
        applies = elf.sections[rel.info]
        if not applies.flags & 2 or applies.flags & 4:
            continue
        kept = []
        for at in range(rel.offset, rel.offset + rel.size, 8):
            offset, info = struct.unpack_from('<II', data, at)
            symbol = elf.symbols[info >> 8]
            match = re.fullmatch(r'D_([0-9A-F]+)', symbol.name)
            value = int(match.group(1), 16) if match else None
            if (info & 255 == 2 and symbol.shndx == 0 and value is not None
                    and not 0x00100000 <= value < 0x02000000):
                word = struct.unpack_from('<I', data, applies.offset + offset)[0]
                struct.pack_into('<I', data, applies.offset + offset, (word + value) & 0xffffffff)
            else:
                kept.append((offset, info))
        if len(kept) * 8 != rel.size:
            for k, entry in enumerate(kept):
                struct.pack_into('<II', data, rel.offset + 8 * k, *entry)
            struct.pack_into('<I', data, elf.shoff + rel.index * elf.shentsize + 20, len(kept) * 8)
    Path(target).write_bytes(bytes(data))


def resolve_external_data_relocs(target, base_elf, addresses):
    """Express the target's data words the way the compiled object does, where both link the same.

    Two splat artifacts, both decided word by word against the C object:
    * a pointer to a static function of this TU is an R_MIPS_32 against its
      name, undefined in the data object, while cc1 writes `.text` + offset:
      retarget it to the target's own `.text` at that function;
    * a constant splat read as `symbol + offset` (`.word _imageMapper +
      0x490C0` for 0x003FC000) where the C object holds a plain word with no
      relocation: fold it only when the linked address plus addend equals that
      word exactly.
    """
    elf = Elf(Path(target).read_bytes())
    data = bytearray(elf.data)
    base_sections = {x.name: x for x in base_elf.sections}
    base_relocs = {}
    for r in base_elf.sections:
        if r.type == 9 and r.info < len(base_elf.sections):
            name = base_elf.sections[r.info].name
            base_relocs.setdefault(name, set()).update(
                struct.unpack_from('<I', base_elf.data, at)[0]
                for at in range(r.offset, r.offset + r.size, 8))
    local_functions = {y.name: y for y in elf.symbols if y.type == 2 and y.shndx}
    section_symbol = {y.shndx: y.index for y in elf.symbols if y.type == 3}
    for rel in elf.sections:
        if rel.type != 9:
            continue
        applies = elf.sections[rel.info]
        if not applies.flags & 2 or applies.flags & 4 or applies.name not in base_sections:
            continue
        bsec = base_sections[applies.name]
        bdata = base_elf.section_bytes(bsec)
        kept = []
        for at in range(rel.offset, rel.offset + rel.size, 8):
            offset, info = struct.unpack_from('<II', data, at)
            symbol = elf.symbols[info >> 8]
            if info & 255 == 2 and symbol.shndx == 0:
                function = local_functions.get(symbol.name)
                if function is not None and function.shndx in section_symbol:
                    word = struct.unpack_from('<I', data, applies.offset + offset)[0]
                    struct.pack_into('<I', data, applies.offset + offset,
                                     (word + function.value) & 0xffffffff)
                    kept.append((offset, (section_symbol[function.shndx] << 8) | 2))
                    continue
                hexname = re.fullmatch(r'D_([0-9A-F]{1,8})', symbol.name)
                address = addresses.get(symbol.name, int(hexname.group(1), 16) if hexname else None)
                if (address is not None and offset + 4 <= len(bdata)
                        and offset not in base_relocs.get(applies.name, ())):
                    word = struct.unpack_from('<I', data, applies.offset + offset)[0]
                    value = (address + word) & 0xffffffff
                    if value == struct.unpack_from('<I', bdata, offset)[0]:
                        struct.pack_into('<I', data, applies.offset + offset, value)
                        continue
            kept.append((offset, info))
        if len(kept) * 8 != rel.size or kept:
            for k, entry in enumerate(kept):
                struct.pack_into('<II', data, rel.offset + 8 * k, *entry)
            struct.pack_into('<I', data, elf.shoff + rel.index * elf.shentsize + 20, len(kept) * 8)
    Path(target).write_bytes(bytes(data))


def trim_leading_fill(target, base_elf):
    """Drop the link fill splat puts at the start of a target data section.

    When the previous object's section ends short of this object's alignment
    (a 16-aligned first item), splat gives the fill to this TU as an
    unreferenced pad item; the compiled object starts at the aligned item.
    Fewer than 16 zero bytes, holding no relocation and only unreferenced
    `D_<addr>` pad labels, are removed: the section's bytes, symbols and
    relocations move down by that amount.
    """
    elf = Elf(Path(target).read_bytes())
    data = bytearray(elf.data)
    base = {x.name: x for x in base_elf.sections}
    symtab = next(x for x in elf.sections if x.type == 2)
    referenced = {struct.unpack_from('<I', elf.data, at + 4)[0] >> 8
                  for r in elf.sections if r.type == 9
                  for at in range(r.offset, r.offset + r.size, 8)}
    changed = False
    for section in elf.sections:
        if section.type != 1 or section.name not in base or not section.flags & 2 \
                or section.flags & 4 or section.size <= base[section.name].size:
            continue
        labels = [y for y in elf.symbols if y.shndx == section.index and y.type != 3
                  and not y.name.endswith('.NON_MATCHING')]
        fill = [y for y in labels if y.value == 0]
        rest = [y.value for y in labels if y.value > 0]
        if not fill or not rest:
            continue
        n = min(rest)
        if not 0 < n < 16 or any(elf.data[section.offset:section.offset + n]) or any(
                not re.fullmatch(r'D_[0-9A-F]{8}', y.name) or y.index in referenced for y in fill):
            continue
        rels = [r for r in elf.sections if r.type == 9 and r.info == section.index]
        if any(struct.unpack_from('<I', elf.data, at)[0] < n
               for r in rels for at in range(r.offset, r.offset + r.size, 8)):
            continue
        data[section.offset:section.offset + section.size - n] = \
            elf.data[section.offset + n:section.offset + section.size]
        struct.pack_into('<I', data, elf.shoff + section.index * elf.shentsize + 20,
                         section.size - n)
        for y in elf.symbols:
            if y.shndx == section.index and y.type != 3:
                at = symtab.offset + 16 * y.index
                if y.value < n:
                    struct.pack_into('<II', data, at + 4, 0, 0)
                else:
                    struct.pack_into('<I', data, at + 4, y.value - n)
        for r in rels:
            for at in range(r.offset, r.offset + r.size, 8):
                struct.pack_into('<I', data, at, struct.unpack_from('<I', elf.data, at)[0] - n)
        changed = True
    if changed:
        Path(target).write_bytes(bytes(data))


def trim_link_padding(target, base_elf):
    """End a target data section's last symbol where the C object's section ends.

    splat gives a data item the zero fill the original linker inserted to align
    the next object's section, so the target section is a few bytes longer than
    the compiled one. Only zero bytes past the compiled section's size, fewer
    than 16 and with no relocation, are dropped: the covering symbol and the
    section end where the compiled section ends. objdiff still compares every
    byte both objects contain.
    """
    elf = Elf(Path(target).read_bytes())
    data = bytearray(elf.data)
    base = {s.name: s for s in base_elf.sections}
    symtab = next(s for s in elf.sections if s.type == 2)
    for section in elf.sections:
        if section.type != 1 or section.name not in base or not section.flags & 2 \
                or section.flags & 4:
            continue
        size = base[section.name].size
        if not 0 < section.size - size < 16 or any(
                elf.data[section.offset + size:section.offset + section.size]):
            continue
        referenced = {struct.unpack_from('<I', elf.data, at + 4)[0] >> 8
                      for r in elf.sections if r.type == 9
                      for at in range(r.offset, r.offset + r.size, 8)}
        fill_labels = [y for y in elf.symbols if y.shndx == section.index and y.type != 3
                       and y.value >= size and not y.name.endswith('.NON_MATCHING')]
        # Only fill: every label in the dropped bytes is an unreferenced splat
        # pad label (`D_<addr>`); a named or referenced item there is data the
        # C object lacks, and stays measured.
        if any(not re.fullmatch(r'D_[0-9A-F]{8}', y.name) or y.index in referenced
               for y in fill_labels):
            continue
        if any(r.type == 9 and r.info == section.index and any(
                struct.unpack_from('<I', elf.data, at)[0] >= size
                for at in range(r.offset, r.offset + r.size, 8)) for r in elf.sections):
            continue
        for symbol in elf.symbols:
            # clamp to the new end; a label of the fill itself ends empty
            if symbol.shndx == section.index and symbol.value + symbol.size > size:
                struct.pack_into('<I', data, symtab.offset + 16 * symbol.index + 8,
                                 max(0, size - symbol.value))
        struct.pack_into('<I', data, elf.shoff + section.index * elf.shentsize + 20, size)
    Path(target).write_bytes(bytes(data))


def preserve_gp0(source, target):
    """Restore the text object's .reginfo ri_gp_value after `ld -r`.

    `ld -r` writes a fresh gp (0x7ff0) into .reginfo but leaves every implicit
    GPREL16/LITERAL addend encoded for the assembler's gp0, so objdiff would
    display a spurious +0x7ff0 on each gp-relative operand. Only that metadata
    word is restored, and only when ld -r kept the text bytes unchanged.
    """
    before, after = Elf(source.read_bytes()), Elf(target.read_bytes())
    verification.require(before.section_bytes(before.section('.text')) ==
                         after.section_bytes(after.section('.text')),
                         f'{target}: ld -r changed text bytes')
    old = [s for s in before.sections if s.name == '.reginfo']
    new = [s for s in after.sections if s.name == '.reginfo']
    if len(old) != 1 or len(new) != 1 or old[0].size != 24 or new[0].size != 24:
        return
    data = bytearray(after.data)
    data[new[0].offset + 20:new[0].offset + 24] = before.data[old[0].offset + 20:old[0].offset + 24]
    target.write_bytes(bytes(data))


def mark_compiler_literals(target, labels):
    """GPREL16 -> R_MIPS_LITERAL for gp loads of compiler-generated literals.

    gas records a load from the compiler's own literal pool (`li.s`/`li.d`) as
    R_MIPS_LITERAL; it is resolved exactly like GPREL16 (S + A - GP), only
    marking that the datum is a literal-pool entry. Applied only to lwc1/ldc1
    off $gp whose symbol is a label of a declared C-generated literal run in
    .lit4/.lit8 (config/tu/data-carves.json, laid out by target_layout). Data
    the C names (`extern const float D_...`) keeps GPREL16, as in cc1 output.
    """
    elf = Elf(target.read_bytes())
    data = bytearray(elf.data)
    text = elf.section('.text')
    text_bytes = elf.section_bytes(text)
    for rel in elf.sections:
        if rel.type != 9 or rel.info != text.index:
            continue
        for at in range(rel.offset, rel.offset + rel.size, 8):
            offset, info = struct.unpack_from('<II', data, at)
            if info & 255 != 7:
                continue
            symbol = elf.symbols[info >> 8]
            section = elf.sections[symbol.shndx].name if 0 < symbol.shndx < len(elf.sections) else ''
            word = int.from_bytes(text_bytes[offset:offset + 4], 'little')
            if (symbol.name in labels.get(section, ()) and word >> 26 in (0x31, 0x35)
                    and (word >> 21) & 31 == 28):
                struct.pack_into('<I', data, at + 4, (info & ~255) | 8)
    target.write_bytes(bytes(data))


def prepare(root, evidence):
    build, project = root / 'build', root / 'build/objdiff'
    project.mkdir(exist_ok=True)
    objects, targets, _, _, _ = cov.tracked_scope(root)
    tc = Toolchain(root)
    prefix = Path(tc.dir('ps2dev-binutils')) / 'mips64r5900el-ps2-elf-'
    main = verification.read(build / 'main/tu-manifest.json')
    main_tus = {t['name']: t for t in main['tus']}
    overlays = {unit: verification.read(build / unit / 'compile-manifest.json')
                for unit in cov.OVERLAY_UNITS}
    expected = {}
    main_elf = build / 'main/build/carve/main.rom.elf'
    registry = data_carve.load_registry(root)
    original_names = {y.name for y in Elf((build / 'main/orig/SLUS_204.69').read_bytes()).symbols}
    linked = {unit: Elf((build / 'main/build/carve/main.rom.elf' if unit == 'main' else
                         build / unit / f'build/{unit}.rom.elf').read_bytes())
              for unit in ('main', *cov.OVERLAY_UNITS)}
    def symbol_addresses(*elves):
        out = {}
        for elf in elves:
            for sym in elf.symbols:
                if sym.shndx and sym.type != 3 and sym.name:
                    out.setdefault(sym.name, sym.value)
        return out
    addresses = {unit: symbol_addresses(linked[unit], *([] if unit == 'main' else [linked['main']]))
                 for unit in linked}
    symtabs = {unit: target_symbolize.SymbolTable(
        [main_elf] + ([] if unit == 'main' else [build / unit / f'build/{unit}.rom.elf']))
        for unit in ('main', *cov.OVERLAY_UNITS)}

    tu_as_flags = verification.read(root / 'config/tu/as-flags.json')

    def prepare_object(obj):
        unit, stem = obj['unit'], Path(obj['source']).stem
        as_flags = tu_as_flags.get(f'{unit}/{stem}', [])
        unit_dir, name = build / unit, f'{unit}/{stem}'
        destination = project / name
        destination.mkdir(parents=True, exist_ok=True)
        checked = evidence['objects'][obj['id']]
        target_text = destination / 'text.o'
        if unit == 'main':
            tu = main_tus[stem]
            contract = tu.get('contract', {})
            ccdir = Path(contract.get('cc_dir', '.'))
            assembler = Path(contract.get('assembler_path', '.'))
            flags = contract.get('flags', [])
            data_sources = [unit_dir / (piece.get('file') or
                            f'asm/main/data/{piece["piece"]}.{section[1:]}.s')
                            for section, entry in main['sections'].items()
                            for piece in entry['order'] if piece.get('tu') == stem]
            if obj['functions']:
                target_text = build_target(unit_dir, stem, unit)
            else:
                run([str(prefix) + 'as', '-EL', '-march=r5900', '-mabi=eabi', '-mgp64',
                     '-o', target_text, '/dev/null'], unit_dir)
        else:
            tus = overlays[unit]['units'][unit]['tus']
            tu = next(t for t in tus if t['name'] == stem)
            ccdir = Path(tu['compiler']['dir'])
            assembler = Path(tu['assembler']['path'])
            flags = tu['flags']
            data_sources = sorted((unit_dir / f'asm/data/{unit}').glob(stem + '.*.s'))
            if tu['kind'] == 'c':
                compile_source(root, unit_dir, unit_dir / f'scaffold/src/{unit}/{stem}.c',
                               target_text, ccdir, assembler, flags, skip_asm=False,
                               as_flags=as_flags)
            else:
                target_text = build_target(unit_dir, stem, unit)
        unit_config = dict(name=name, target_path=str(destination / 'target.o'), metadata=dict(
            source_path=obj['source'], progress_categories=[unit], complete=checked['complete']))
        allowed = {f.get('c_symbol', f['original_symbol']): f['size'] for f in checked['functions']}
        base_elf = None
        if allowed:
            base = destination / 'base.o'
            assembly = compile_source(root, unit_dir, root / obj['source'], base,
                                      ccdir, assembler, flags, skip_asm=True,
                                      as_flags=as_flags, keep_locals=True)
            emitted = verification.compiler_functions(assembly.read_text())
            actual = functions(base)
            # Removing inline scaffold blocks can change scheduling/alignment.
            # Native objdiff measures that difference; the unmodified published
            # build remains the authority for exact linked sizes and bytes.
            verification.require(set(actual) == set(allowed) and set(actual) <= emitted,
                                 f'{name}: SKIP_ASM emission differs from verified C scope')
            stripped_base = destination / 'base.nodebug.o'
            strip_debug(prefix, base, stripped_base, unit_dir)
            anchor_data_sections(prefix, stripped_base, unit_dir)
            size_bss_symbols(stripped_base)
            unit_config['base_path'] = str(stripped_base)
            base_elf = Elf(stripped_base.read_bytes())
        runs = dict(registry['tus'].get(tu.get('id'), {})) if base_elf is not None else {}
        if unit == 'main':
            # Import-era C-owned pieces (config/split/main.data-splits.json).
            for section, entry in main['sections'].items():
                split = [dict(generated=[p['start'], p['end']]) for p in entry['order']
                         if p.get('tu') == stem and p.get('c_split')]
                if split and not runs.get(section):
                    runs[section] = split
        carved = next((c for c in ([unit_dir / f'build/c/{stem}.carved.o'] if unit == 'main'
                                   else [unit_dir / f'build/src/{unit}/{stem}.carved.o',
                                         unit_dir / f'build/scaffold/src/{unit}/{stem}.carved.o'])
                       if c.is_file()), None)
        assemble = lambda source, output: run(
            [str(prefix) + 'as', '-EL', '-march=r5900', '-mabi=eabi', '-mgp64', '-G0',
             '-no-pad-sections', '-mno-fix-r5900', '-I.', '-Iinclude', '-o', output, source], unit_dir)
        data, literal_labels, merged = [], {}, set()
        for section in LITERAL_SECTIONS:
            group = [d for d in data_sources if d.name.endswith(section + '.s')]
            compiled = [x for x in base_elf.sections if x.name == section] if base_elf else []
            if not group or not runs.get(section) or len(compiled) != 1:
                continue
            ordered = destination / (group[0].stem + '.compiler-order.s')
            output = destination / (group[0].stem + '.o')
            proof = target_layout.reorder(group, ordered, runs[section],
                                          base_elf.section_bytes(compiled[0]), linked[unit],
                                          carved, section)
            if proof is None:
                continue
            original_size = 0
            for source in group:
                assemble(source, destination / (source.stem + '.orig.o'))
                original = Elf((destination / (source.stem + '.orig.o')).read_bytes())
                original_size += sum(x.size for x in original.sections if x.name == section)
            assemble(ordered, output)
            if target_layout.verify(output, section, proof, original_size):
                literal_labels[section] = set(proof['labels'])
                merged.update(group)
                data.append(output)
        for source in data_sources:
            if source in merged:
                continue
            output = destination / (source.stem + '.o')
            assemble(source, output)
            data.append(output)
        if obj['functions']:
            # Report-local copy: recover splat's unpaired %hi/%lo operands
            # from the original disassembly + linked ELF addresses.
            target_text, _ = target_symbolize.symbolize(
                target_text, unit_dir, destination / 'text.sym.o', assembler, flags,
                symtabs[unit])
        target = destination / 'target.o'
        stripped = destination / 'text.nodebug.o'
        strip_debug(prefix, target_text, stripped, unit_dir)
        run([str(prefix) + 'ld', '-EL', '-m', 'elf32lr5900', '-r',
             '-o', target, stripped, *data], unit_dir)
        preserve_gp0(stripped, target)
        if unit == 'main':
            size_bss_symbols(target, original_names)
        else:
            # overlays have no original symbol table: a splat `D_<addr>` BSS
            # label where the C object starts no object lies inside the C's
            # object before it (whose size is then compared as one)
            size_bss_symbols(target, None, base_elf)
        if base_elf is not None:
            pair_verified_names(prefix, target, base_elf, checked['functions'], unit_dir)
            trim_leading_fill(target, base_elf)
            trim_link_padding(target, base_elf)
            section_relative_data_relocs(target, base_elf)
            fold_constant_labels(target)
            resolve_external_data_relocs(target, base_elf, addresses[unit])
        if literal_labels:
            mark_compiler_literals(target, literal_labels)
        target_functions = functions(target)
        verification.require(len(target_functions) == len(obj['functions']) and
                             sum(target_functions.values()) == sum(f[2] for f in obj['functions']),
                             f'{name}: target function scope differs from tracked TU')
        return unit_config, dict(functions=target_functions, allowed={**allowed, **{f['original_symbol']: f['size'] for f in checked['functions']}},
                                 complete=checked['complete'])

    with ThreadPoolExecutor(max_workers=4) as pool:
        prepared = list(pool.map(prepare_object, objects))
    units = []
    for unit, checked in prepared:
        verification.require(unit['name'] not in expected, 'duplicate TU name')
        units.append(unit)
        expected[unit['name']] = checked
    # IOP: per-FDR pairs of an original-instruction reference and the fresh
    # compiler object from the verified complete SSD/RSSD build.
    from tu import iop_objdiff
    iop = evidence.get('iop') or {}
    verification.require(iop.get('build_report'),
                         'verified IOP build missing; run verify_report --finish')
    report_path = root / iop['build_report']
    verification.require(verification.digest(report_path) == iop.get('build_report_sha256'),
                         'verified IOP build report changed')
    iop_units, iop_expected, iop_proof = iop_objdiff.prepare(root, project, report_path)
    verification.require(not set(iop_expected).intersection(expected), 'duplicate IOP object name')
    units.extend(iop_units)
    expected.update(iop_expected)
    (project / 'iop-proof.json').write_text(json.dumps(iop_proof, indent=2) + '\n')
    config = dict(build_target=False, build_base=False, units=units,
                  progress_categories=[dict(id=u, name=targets.get(u, u)) for u in cov.UNITS])
    (project / 'objdiff.json').write_text(json.dumps(config, indent=1) + '\n')
    return project, expected


def validate(report, expected):
    """Reject scope drift and scaffold credit; leave native JSON unchanged."""
    units = {u['name']: u for u in report['units']}
    verification.require(set(units) == set(expected), 'native report dropped or added units')
    for name, checked in expected.items():
        unit = units[name]
        actual = {f['name']: int(f['size']) for f in unit.get('functions', [])}
        verification.require(actual == checked['functions'], f'{name}: native function scope drift')
        for function in unit.get('functions', []):
            verification.require(not function.get('fuzzy_match_percent', 0) or
                                 function['name'] in checked['allowed'],
                                 f'{name}/{function["name"]}: scaffolding received matching credit')
        # Keep objdiff's native BSS measure. It describes uninitialized storage
        # matching, never recovered C declarations or source/link completeness.
        if not checked['allowed']:
            verification.require(not any(int(s.get('size', 0)) and s['name'] not in ('.bss', '.sbss')
                                         and s.get('fuzzy_match_percent') == 100
                                         for s in unit.get('sections', [])),
                                 f'{name}: initialized scaffold data received matching credit')
        verification.require(bool(unit.get('metadata', {}).get('complete', False)) == checked['complete'],
                             f'{name}: unexpected complete metadata')

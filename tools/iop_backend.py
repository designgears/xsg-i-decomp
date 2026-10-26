#!/usr/bin/env python3
"""Strict, read-only ELF and ordinary-C checks shared by the IOP runner.

Addresses are module-relative throughout. Nothing rewrites, masks or normalizes
object bytes, relocation records, instructions, or the original IRX headers.
"""
import hashlib
import json
from pathlib import Path
import re
import struct

REVISION = "slus-20469-412d448de315"
PROFILE = "iop-gcc2_8_1"
# The pinned 1998 i386 assembler relaxes macro loads with an overlapping
# memcpy (gas md_convert_frag: dst = src - 4).  Modern glibc's SSE2+ memcpy
# variants copy such ranges in an alignment-dependent order, so whether a
# relaxed `lui` survives depended on host heap layout.  Restricting glibc to
# its baseline ia32 memcpy restores the forward copy the tool was built for.
LEGACY_HOST_TUNABLES = ("glibc.cpu.hwcaps=-SSE2,-SSSE3,-SSE4_1,-SSE4_2,-AVX,-AVX2,"
                        "-AVX512F,-ERMS,-Fast_Unaligned_Load,-Fast_Unaligned_Copy")


def legacy_host_env(base):
    return dict(base, LC_ALL="C", LANG="C", GLIBC_TUNABLES=LEGACY_HOST_TUNABLES)
BINDINGS = {"LOCAL": 0, "GLOBAL": 1, "WEAK": 2}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(value):
    return hashlib.sha256(value if isinstance(value, bytes) else Path(value).read_bytes()).hexdigest()


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, "duplicate JSON key: " + key)
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=unique)


def relative(value):
    require(isinstance(value, str) and re.fullmatch(r"[A-Za-z0-9_./-]+", value)
            and not value.startswith(("/", "-"))
            and all(p not in ("", ".", "..") for p in value.split("/")), "unsafe relative path")
    return Path(value)


def address(value):
    require(isinstance(value, str) and re.fullmatch(r"0x[0-9a-f]{8}", value), "invalid module-relative offset")
    return int(value, 16)


def identifier(value):
    require(isinstance(value, str) and re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", value), "unsafe original symbol")
    return value


def mask_comments(text):
    return re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*',
                  lambda m: " " if m[0].startswith(("/*", "//")) else m[0], text, flags=re.S)


def ordinary_c(text, staged=None, headers=()):
    """C only; check both authored input and the actual preprocessed stream."""
    require("\x00" not in text and "??" not in text and "%:" not in text,
            "non-ordinary C encoding or alternative preprocessing tokens")
    text = mask_comments(re.sub(r"\\\r?\n", "", text))
    require(text.isascii(), "non-ASCII executable C tokens")
    tokens = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', " ", text)
    # GNU C accepts '$' in identifiers. Such an authored identifier could
    # otherwise imitate GAS's private $L... labels and disguise C storage as a
    # compiler-generated table. Apply this to authored headers and the actual
    # preprocessed stream; dollars inside comments/literals remain ordinary data.
    require("$" not in tokens,"GNU dollar identifiers are not ordinary IOP C")
    require(not re.search(r"\b(?:register|asm|__asm|__asm__|__attribute|__attribute__|_Pragma|"
                          r"__extension__|typeof|__typeof__|__label__|__builtin_[A-Za-z_0-9]+)\b", tokens),
            "register bindings, ASM, attributes, pragmas or unrelated GNU extensions are not ordinary IOP C")
    require(not re.search(r"\bgoto\s*\*|\(\s*\{", tokens), "computed goto or statement expression is not ordinary IOP C")
    for line in text.splitlines():
        if not line.lstrip().startswith("#"):
            continue
        if staged is None:
            require(re.fullmatch(r'\s*#\s*\d+\s+"[^"\n]+"(?:\s+\d+)*\s*', line),
                    "unexpected preprocessed directive")
            continue
        include = re.fullmatch(r'\s*#\s*include\s*["<]([A-Za-z0-9_./-]+)[">]\s*', line)
        if include:
            name = relative(include[1])
            stage = Path(staged)
            require(any(p.as_posix() in headers for p in (stage.parent/name, Path("include")/name, name)),
                    "include is not a declared staged header")
        else:
            require(re.match(r"\s*#\s*(?:define|undef|if|ifdef|ifndef|elif|else|endif)\b", line)
                    and "##" not in line, "unsupported preprocessor directive")


class ELF:
    """Bounded ELF32LE MIPS reader for ET_REL, ET_EXEC and original SCE IRX."""
    def __init__(self, data):
        self.data = bytes(data)
        require(len(data) >= 52 and data[:7] == b"\x7fELF\x01\x01\x01", "expected ELF32 little endian")
        h = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
        kind, machine, version, entry, phoff, shoff, flags, ehsize, phsize, phnum, shsize, shnum, shstr = h
        require(kind in (1, 2, 0xff80) and machine == 8 and version == 1 and ehsize == 52,
                "unsupported IOP ELF identity")
        require(shsize == 40 and 0 < shnum <= 4096 and shstr < shnum, "invalid IOP section table")
        require((not phnum or phsize == 32) and phnum <= 128, "invalid IOP program headers")
        self.header = dict(type=kind, machine=machine, version=version, entry=entry, flags=flags,
                           program_header_count=phnum, section_count=shnum)
        self.program_headers = [struct.unpack("<8I", self.span(phoff+i*phsize, phsize)) for i in range(phnum)]
        raw = [struct.unpack("<10I", self.span(shoff+i*40, 40)) for i in range(shnum)]
        names = self.span(raw[shstr][4], raw[shstr][5])
        self.sections = []
        self.symbols = []
        self.relocations = []
        for i, row in enumerate(raw):
            name, typ, attrs, start, offset, size, link, info, alignment, entsize = row
            if typ != 8:
                self.span(offset, size)
            self.sections.append(dict(index=i, name=self.string(names,name), type=typ, flags=attrs,
                address=start, va=f"0x{start:08x}", module_offset=start, file_offset=offset,
                offset=offset, size=size, link=link, info=info, alignment=alignment, entry_size=entsize))
        tables = {}
        for section in self.sections:
            if section["type"] != 2:
                continue
            require(section["entry_size"] == 16 and section["size"] % 16 == 0
                    and section["link"] < shnum, "invalid IOP symbol table")
            strings_section = self.sections[section["link"]]
            require(strings_section["type"] == 3, "invalid IOP symbol strings")
            strings = self.section_bytes(strings_section)
            table = []
            for index, pos in enumerate(range(section["offset"], section["offset"]+section["size"], 16)):
                name,value,size,info,other,owner = struct.unpack_from("<IIIBBH", data, pos)
                symbol = dict(index=index, name=self.string(strings,name), value=value, va=f"0x{value:08x}",
                              size=size, info=info, type=info&15, binding=info>>4, other=other,
                              section_index=owner)
                table.append(symbol)
                self.symbols.append(symbol)
            tables[section["index"]] = table
        for section in self.sections:
            if section["type"] not in (4,9):
                continue
            stride = 12 if section["type"] == 4 else 8
            table = tables.get(section["link"])
            require(table is not None and section["entry_size"] == stride and section["size"] % stride == 0
                    and section["info"] < shnum, "invalid IOP relocation table")
            target = self.sections[section["info"]]
            for pos in range(section["offset"],section["offset"]+section["size"],stride):
                offset, info = struct.unpack_from("<II",data,pos)
                symbol_index, typ = info>>8, info&255
                require(symbol_index < len(table), "invalid IOP relocation symbol")
                module_offset = offset + (target["address"] if kind == 1 else 0)
                local_offset = offset if kind == 1 else offset-target["address"]
                require(0 <= local_offset <= target["size"]-4, "relocation outside target section")
                word = int.from_bytes(self.span(target["offset"]+local_offset,4),"little")
                self.relocations.append(dict(section=section["name"], target_section=target["name"],
                    offset=offset, module_offset=module_offset, info=info, type=typ, symbol_index=symbol_index,
                    symbol=table[symbol_index]["name"], symbol_value=table[symbol_index]["value"],
                    word=word, explicit_addend=struct.unpack_from("<i",data,pos+8)[0] if stride == 12 else None))

    def span(self, offset, size):
        require(type(offset) is int and type(size) is int and 0 <= offset <= len(self.data)
                and 0 <= size <= len(self.data)-offset, "ELF extent outside file")
        return self.data[offset:offset+size]

    @staticmethod
    def string(table,index):
        require(0 <= index < len(table), "ELF string outside table")
        end = table.find(b"\0",index)
        require(end >= index,"unterminated ELF string")
        return table[index:end].decode("ascii")

    def section_bytes(self, section):
        return None if section["type"] == 8 else self.span(section["offset"],section["size"])

    def at(self, start, size):
        matches = [s for s in self.sections if s["flags"]&2 and s["address"] <= start
                   and start+size <= s["address"]+s["size"]]
        require(len(matches) == 1,"span lacks a unique allocated section")
        section = matches[0]
        return section, (None if section["type"] == 8 else
                         self.span(section["offset"]+start-section["address"],size))

    def function_bytes(self, symbol):
        require(0 < symbol["section_index"] < len(self.sections),"function has no emitted section")
        section = self.sections[symbol["section_index"]]
        start = symbol["value"] if self.header["type"] == 1 else symbol["value"]-section["address"]
        require(section["flags"]&4 and 0 <= start and start+symbol["size"] <= section["size"],
                "function extent outside executable section")
        return self.span(section["offset"]+start,symbol["size"])


def difference(expected,actual,start):
    offset = next((i for i,(a,b) in enumerate(zip(expected,actual)) if a!=b),
                  min(len(expected),len(actual)) if expected!=actual else None)
    return dict(equal=expected==actual, expected_size=len(expected),actual_size=len(actual),
        expected_sha256=digest(expected),actual_sha256=digest(actual),first_difference=None if offset is None else
        dict(offset=offset,va=f"0x{start+offset:08x}",instruction_offset=offset//4*4,
             expected=expected[offset:offset+4].hex(),actual=actual[offset:offset+4].hex()))


def emission_audit(obj, assembly, functions, original_symbols, generated_data):
    require(obj.header["type"] == 1 and obj.header["flags"]&0xf0000000 == 0
            and not obj.header["flags"]&0x20, "candidate object is not MIPS-I ELF32")
    require(not re.search(r"^\s*#\s*APP\b|\.incbin\b",assembly,re.M),"compiler output contains authored assembly")
    names = {f["name"] for f in functions}
    emitted = [s for s in obj.symbols if s["type"]==2 and s["section_index"]!=0]
    extras = [s["name"] for s in emitted if s["name"] not in names]
    allowed_data = {s["section"] for s in generated_data}
    extra_sections = [s["name"] for s in obj.sections if s["size"] and s["flags"]&2 and
                      not s["flags"]&4 and s["name"] not in allowed_data and
                      not (s["name"]==".reginfo" and s["type"]==0x70000006 and s["size"]==24)]
    # COMMON allocations do not occupy a nonempty ET_REL .bss section. They
    # still allocate storage and cannot silently disappear from a composed
    # module. Source-defined named data must retain the original identity;
    # compiler-created local constant/jump-table labels remain uncredited.
    data_objects=[]
    for symbol in obj.symbols:
        if symbol["type"]!=1 or symbol["section_index"]==0:
            continue
        common=symbol["section_index"]==0xfff2
        internal=(symbol["binding"]==0 and bool(re.fullmatch(r"(?:\$L|\.L)[A-Za-z_0-9.]*",symbol["name"])))
        section=(obj.sections[symbol["section_index"]] if 0<symbol["section_index"]<len(obj.sections) else None)
        section_name=(".rodata" if section and section["name"]==".rdata" else section["name"] if section else None)
        maps=[m for m in generated_data if m["section"] in (".bss",".sbss")] if common else [
            m for m in generated_data if m["section"]==section_name]
        originals=[s for s in original_symbols if s["name"]==symbol["name"] and s["type"]=="OBJECT"
                   and BINDINGS.get(s["binding"])==symbol["binding"] and s["other"]==symbol["other"]]
        original=originals[0] if len(originals)==1 else None
        if common:
            mapped=bool(original and any(address(m["va"])<=original["value"] and
                        original["value"]+symbol["size"]<=address(m["va"])+m["size"] for m in maps))
            size_equal=bool(original and (not original["size"] or original["size"]==symbol["size"]))
            valid=mapped and size_equal
            if not maps:
                extra_sections.append("COMMON:"+symbol["name"])
        else:
            mapped=bool(section and maps and any(address(m["va"])+symbol["value"]-section["address"]
                       ==original["value"] for m in maps)) if original else False
            size_equal=bool(original and (not original["size"] or not symbol["size"] or
                                          original["size"]==symbol["size"]))
            valid=bool(maps and (internal or mapped and size_equal))
        data_objects.append(dict(name=symbol["name"],object_symbol=symbol,original_symbol=original,
                                 common=common,compiler_generated=internal,verified=valid,
                                 recovered_data=False,linked_identity_required=not internal))
    checks = []
    for function in functions:
        name = function["name"]
        candidates = [s for s in emitted if s["name"]==name]
        originals = [s for s in original_symbols if s["name"]==name and s["type"]=="FUNC"
                     and s["value"]==address(function["va"])]
        symbol = candidates[0] if len(candidates)==1 else None
        original = originals[0] if len(originals)==1 else None
        assembler_identity = bool(re.search(r"^\s*\.ent\s+"+re.escape(name)+r"\s*$",assembly,re.M)
                                  and re.search(r"^"+re.escape(name)+r":\s*$",assembly,re.M)
                                  and re.search(r"^\s*\.end\s+"+re.escape(name)+r"\s*$",assembly,re.M))
        binding = bool(symbol and original and symbol["binding"]==BINDINGS[original["binding"]]
                       and symbol["other"]==original["other"])
        checks.append(dict(name=name, compiler_emitted=bool(symbol and assembler_identity),
                           object_symbol=symbol, original_symbol=original, binding_equal=binding,
                           size_equal=bool(symbol and symbol["size"]==function["size"]),
                           verified=bool(symbol and assembler_identity and binding and symbol["size"]==function["size"])))
    return dict(verified=all(c["verified"] for c in checks) and all(d["verified"] for d in data_objects)
                and not extras and not extra_sections,
                functions=checks,data_objects=data_objects,extra_functions=extras,extra_sections=extra_sections)


def scoped_relocations(elf,spans):
    """Raw IRX REL records plus their unmodified in-place addend words."""
    return [r for r in elf.relocations if any(section==r["target_section"] and start<=r["module_offset"]<start+size
                                            for section,start,size in spans)]

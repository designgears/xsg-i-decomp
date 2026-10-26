#!/usr/bin/env python3
"""Verify required IOP reference pins and inventory module-relative symbols.

verify_iop(root) returns deterministic JSON-compatible evidence, not a C match.
describe_irx(data, unit, reader_output) is the bounded readelf-output decoder.
CLI --output requires a fresh JSON file below ROOT/build; without it (or with
--verify-only), print only a verification summary. No input is ever repinned.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EE_SHA = "f38aeb224e2d96ababcdce03c4aa4461ce6da22de8feffb86425ee332501a7e3"
UNIT_PATHS = {"iop_ssd": "IOP/SSD.IRX", "iop_rssd": "IOP/RSSD.IRX"}
ROLES = {"iop_ssd": "sound_engine", "iop_rssd": "rpc_and_ee_stream_adapter"}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, f"duplicate JSON key: {key}")
            result[key] = value
        return result
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique)


def equal(actual, expected, context):
    require(json.dumps(actual, sort_keys=True, allow_nan=False) ==
            json.dumps(expected, sort_keys=True, allow_nan=False), f"{context}: pin mismatch")


def describe_irx(data, unit, reader_output):
    """Decode only ELF32LE MIPS-I SCE IRX; retain raw section and REL pins.

    GNU readelf decodes sections and symbols. Raw header/table checks fence its
    textual records against the input, including NULL and NOBITS distinctions.
    REL offsets are module-relative, not offsets within .data or runtime VAs.
    """
    require(unit in UNIT_PATHS, "unknown IOP unit")
    require(len(data) >= 52 and data[:7] == b"\x7fELF\x01\x01\x01", "expected ELF32LE")
    header_names = ("type", "machine", "version", "entry", "program_offset",
                    "section_offset", "flags", "header_size", "program_entry_size",
                    "program_count", "section_entry_size", "section_count", "string_section")
    header = dict(zip(header_names, struct.unpack_from("<HHIIIIIHHHHHH", data, 16)))
    for field, value in dict(type=0xff80, machine=8, version=1, entry=0,
                             flags=1, header_size=52, section_entry_size=40).items():
        equal(header[field], value, f"IRX header {field}")
    count = header["section_count"]
    require(0 < count < 0xff00 and header["string_section"] < count, "unsupported section count")

    def extent(offset, size):
        require(0 <= offset <= len(data) and 0 <= size <= len(data) - offset,
                "truncated file extent")
        return data[offset:offset + size]

    extent(header["section_offset"], count * 40)
    require(header["program_entry_size"] == 32 or header["program_count"] == 0,
            "invalid program header size")
    program_bytes = extent(header["program_offset"], header["program_count"] * 32)
    for pos in range(0, len(program_bytes), 32):
        segment = struct.unpack_from("<8I", program_bytes, pos)
        extent(segment[1], segment[4])
    raw_sections = [struct.unpack_from("<10I", data, header["section_offset"] + i * 40)
                    for i in range(count)]
    names_header = raw_sections[header["string_section"]]
    require(names_header[1] == 3, "invalid section name table")
    names = extent(names_header[4], names_header[5])

    def string_at(table, offset):
        require(offset < len(table) and b"\0" in table[offset:], "invalid string offset")
        return table[offset:table.index(b"\0", offset)].decode("ascii")

    section_pattern = re.compile(
        r"^\s*\[\s*(\d+)\]\s+(.*?)\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)"
        r"\s+([0-9a-f]+)\s+([0-9a-f]+)\s*([A-Za-z]*)\s+(\d+)\s+(\d+)\s+(\d+)\s*$")
    sections = []
    for line in reader_output.splitlines():
        match = section_pattern.match(line)
        if not match:
            continue
        index, name, kind, address, offset, size, entry_size, flags, link, info, alignment = match.groups()
        index = int(index)
        require(index == len(sections) and index < count, "duplicate or unordered section")
        raw = raw_sections[index]
        equal([int(address, 16), int(offset, 16), int(size, 16), int(entry_size, 16),
               int(link), int(info), int(alignment)],
              [raw[3], raw[4], raw[5], raw[9], raw[6], raw[7], raw[8]], "readelf section")
        equal(name, string_at(names, raw[0]), "section name")
        require(raw[3] + raw[5] <= 0x100000000, "section address overflow")
        payload = None if raw[1] in (0, 8) else extent(raw[4], raw[5])
        sections.append(dict(index=index, name=name, type=raw[1], flags=raw[2],
                             module_offset=raw[3], file_offset=raw[4], size=raw[5],
                             link=raw[6], info=raw[7], alignment=raw[8], entry_size=raw[9],
                             sha256=None if payload is None else digest(payload)))
    require(len(sections) == count, "incomplete readelf sections")
    by_name = {s["name"]: s for s in sections}
    require(len(by_name) == count, "duplicate section name")
    require({".iopmod", ".text", ".data", ".bss", ".mdebug", ".symtab"} <= by_name.keys(),
            "missing required IRX sections")
    module = by_name[".iopmod"]
    require(module["type"] == 0x70000080 and module["size"] >= 27, "invalid .iopmod")
    module_bytes = extent(module["file_offset"], module["size"])
    module_info = dict(zip(("module_info", "entry", "gp", "text_size", "data_size", "bss_size"),
                          struct.unpack_from("<6I", module_bytes)))
    module_info.update(version_raw=struct.unpack_from("<H", module_bytes, 24)[0],
                       name=string_at(module_bytes, 26))
    require(by_name[".mdebug"]["size"] > 0, "missing nonempty debug evidence")
    symtab = by_name[".symtab"]
    require(symtab["type"] == 2 and symtab["entry_size"] == 16 and
            symtab["size"] % 16 == 0 and symtab["link"] < count, "invalid symbol table")
    strings_section = sections[symtab["link"]]
    require(strings_section["type"] == 3, "invalid symbol strings")
    strings = extent(strings_section["file_offset"], strings_section["size"])
    symbols, functions = [], {}
    symbol_pattern = re.compile(
        r"^\s*(\d+):\s+([0-9a-f]{8})\s+(\d+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s*(.*?)\s*$")
    for line in reader_output.splitlines():
        match = symbol_pattern.match(line)
        if not match:
            continue
        index, value, size, kind, binding, visibility, section_text, name = match.groups()
        index, value, size = int(index), int(value, 16), int(size)
        require(index == len(symbols) and index < symtab["size"] // 16, "invalid symbol sequence")
        raw = struct.unpack_from("<IIIBBH", data, symtab["file_offset"] + index * 16)
        # Section symbols have a readelf-synthesized name when st_name is zero.
        original_name = string_at(strings, raw[0])
        expected_name = sections[raw[5]]["name"] if raw[3] & 15 == 3 and raw[0] == 0 and raw[5] < count else original_name
        equal([value, size, name], [raw[1], raw[2], expected_name], "readelf symbol")
        require((kind == "FUNC") == (raw[3] & 15 == 2) and
                (kind == "OBJECT") == (raw[3] & 15 == 1), "symbol type mismatch")
        symbol = dict(id=f"{unit}:sym:{index}", index=index, name=name, original_name=original_name,
                      value=value, size=size, type=kind, info=raw[3], other=raw[4],
                      section_index=raw[5], binding=binding, visibility=visibility)
        symbols.append(symbol)
        if kind != "FUNC" or not size:
            continue
        require(0 < raw[5] < count, "FUNC has no defined section")
        section = sections[raw[5]]
        delta = value - section["module_offset"]
        require(section["type"] == 1 and section["flags"] & 4 and
                0 <= delta and delta + size <= section["size"], "FUNC outside executable section")
        key = f"{unit}:0x{value:08x}"
        function = functions.setdefault(key, dict(key=key, address_kind="module_relative", module_offset=value, size=size,
            section_index=raw[5], aliases=[], symbol_ids=[],
            sha256=digest(extent(section["file_offset"] + delta, size))))
        equal([function["size"], function["section_index"]], [size, raw[5]], "FUNC alias extent")
        function["symbol_ids"].append(symbol["id"])
        if name not in function["aliases"]:
            function["aliases"].append(name)
    require(len(symbols) == symtab["size"] // 16, "incomplete readelf symbols")
    relocations = []
    for section in sections:
        if section["type"] != 9:
            require(section["type"] != 4, "RELA unsupported for this IRX scope")
            continue
        require(section["entry_size"] == 8 and section["size"] % 8 == 0 and
                section["link"] == symtab["index"] and 0 < section["info"] < count,
                "invalid REL section")
        target = sections[section["info"]]
        records = []
        for offset, info in struct.iter_unpack("<II", extent(section["file_offset"], section["size"])):
            require(info >> 8 < len(symbols) and target["module_offset"] <= offset and
                    offset + 4 <= target["module_offset"] + target["size"], "invalid REL record")
            records.append(dict(module_offset=offset, info=info, type=info & 255,
                                symbol_id=f"{unit}:sym:{info >> 8}"))
        relocations.append(dict(section=section["name"], target_section=target["name"],
                                count=len(records), sha256=section["sha256"], records=records))
    functions = sorted(functions.values(), key=lambda f: f["module_offset"])
    end, union_bytes = 0, 0
    for function in functions:
        start, stop = function["module_offset"], function["module_offset"] + function["size"]
        union_bytes += max(0, stop - max(end, start))
        end = max(end, stop)
    return dict(path=UNIT_PATHS[unit], size=len(data), sha256=digest(data), header=header,
                program_headers_sha256=digest(program_bytes), sections=sections, module=module_info,
                address_kind="module_relative", load_base=None, functions=functions, symbols=symbols,
                relocations=relocations, function_count=len(functions), function_code_bytes=union_bytes,
                accepted_c_functions=0, accepted_c_code_bytes=0, accepted_c_data_bytes=0,
                matching_ready=False, toolchain_ready=False, unit_verified=False,
                required_for_decomp_completion=True, role=ROLES[unit])


def pins(record):
    """Manifest retains section/relocation pins; full inventories retain all records."""
    return {key: ([{k: v for k, v in section.items() if k != "records"} for section in value]
                  if key == "relocations" else value)
            for key, value in record.items() if key not in ("symbols", "functions")}


def verify_iop(root=ROOT):
    root = Path(root).resolve()
    manifest = load_json(root / "config/iop-targets.json")
    require(set(manifest) == {"schema_version", "revision", "ee_manifest_sha256", "units"}, "manifest keys")
    equal(manifest["schema_version"], 1, "schema")
    equal(manifest["revision"], "slus-20469-412d448de315", "revision")
    equal(manifest["ee_manifest_sha256"], EE_SHA, "EE manifest pin")
    equal(digest((root / "config/targets.json").read_bytes()), EE_SHA, "EE manifest")
    require(set(manifest["units"]) == set(UNIT_PATHS), "both IOP units required")
    lock = load_json(root / "config/iop-tools.lock.json")
    require(lock["role"] == "generic_elf_reader_not_iop_compiler", "wrong reader role")
    reader = root / lock["path"]
    require(not Path(lock["path"]).is_absolute() and ".." not in Path(lock["path"]).parts and
            reader.resolve().is_relative_to(root / ".work/toolchains"), "reader path escapes toolchains")
    equal(digest(reader.read_bytes()), lock["sha256"], "readelf")
    env = dict(os.environ, LC_ALL="C", LANG="C")
    def run(*args):
        result = subprocess.run([str(reader), *args], capture_output=True, text=True,
                                encoding="ascii", env=env, timeout=30, check=True)
        require(not result.stderr.strip(), "readelf reported diagnostics")
        return result.stdout
    equal(run("--version").splitlines()[0], lock["version"], "readelf version")
    inputs = {}
    game = root / "game/extracted-iso"
    for unit, filename in UNIT_PATHS.items():
        equal(manifest["units"][unit]["path"], filename, f"{unit} path")
        path = game / filename
        require(path.resolve().is_relative_to(game) and
                all(not p.is_symlink() for p in (path, *path.parents)), "unsafe IOP input path")
        data = path.read_bytes()
        equal(digest(data), manifest["units"][unit]["sha256"], f"{unit} original")
        equal(len(data), manifest["units"][unit]["size"], f"{unit} size")
        inputs[unit] = (path, data)
    units = {}
    for unit, (path, data) in inputs.items():
        record = describe_irx(data, unit, run("-hSW", "-sW", str(path)))
        require(path.read_bytes() == data, "input changed during readelf")
        equal(pins(record), manifest["units"][unit], unit)
        units[unit] = record
    return dict(ok=True, schema_version=1, revision=manifest["revision"], reader=lock,
                manifest_sha256=digest((root / "config/iop-targets.json").read_bytes()), units=units)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    output = parser.add_mutually_exclusive_group()
    output.add_argument("--output", type=Path)
    output.add_argument("--verify-only", action="store_true")
    args = parser.parse_args(argv)
    try:
        destination = None
        if args.output:
            root = args.root.resolve()
            destination = args.output if args.output.is_absolute() else root / args.output
            require(destination.suffix == ".json" and not destination.exists() and
                    not destination.is_symlink(), "output must be a fresh JSON file")
            require(destination.resolve().is_relative_to(root / "build") and
                    all(not p.is_symlink() for p in (destination, *destination.parents)),
                    "output must stay under nonsymlink ROOT/build")
        result = verify_iop(args.root)
        if destination:
            destination.parent.mkdir(parents=True, exist_ok=True)
            with destination.open("x", encoding="utf-8") as stream:
                stream.write(json.dumps(result, sort_keys=True, indent=2) + "\n")
        print(json.dumps(dict(ok=True, units={unit: {key: record[key] for key in
              ("function_count", "function_code_bytes", "accepted_c_code_bytes", "matching_ready")}
              for unit, record in result["units"].items()}), sort_keys=True))
        return 0
    except (OSError, ValueError, KeyError, TypeError, struct.error, subprocess.SubprocessError) as exc:
        print(json.dumps(dict(ok=False, error=str(exc)), sort_keys=True))
        return 1


if __name__ == "__main__":
    sys.exit(main())

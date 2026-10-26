#!/usr/bin/env python3
"""Prepare verified SSD/RSSD research inputs; never grant matching credit.

status prints readiness and loader-table evidence. bundle retains a complete
private inventory and preparation manifest. function prints one module-relative
function's debug associations, relocations, calls and pinned MIPS-I disassembly.
No original, published source, compiler contract or queue entry is modified.
"""

import argparse
from collections import Counter, defaultdict
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.iop_inventory import UNIT_PATHS, digest, load_json, require, verify_iop
from tools.tu_manifest import debug_records

RELOCATIONS = {0: "R_MIPS_NONE", 2: "R_MIPS_32", 4: "R_MIPS_26",
               5: "R_MIPS_HI16", 6: "R_MIPS_LO16", 7: "R_MIPS_GPREL16"}
FORMAT_SOURCE = "https://github.com/ps2dev/ps2sdk/blob/master/iop/kernel/include/irx.h"
SEMANTIC_WORK = [
    dict(id="protocol_review", action="Review RPC 0x900 / callback 0x901 packet layouts, sizes, ownership and streaming callbacks across EE/RSSD/SSD before publishing shared declarations."),
    dict(id="source_review", action="Every candidate still needs readable semantics, evidenced declarations, a fresh complete IRX gate and independent review."),
]


def backend_status(root):
    """Report the current executable contract, separately from immutable intake."""
    from tools.dispatch_status import status
    units = {}
    for unit in UNIT_PATHS:
        current = status(root, unit=unit, language="c", stage="match")
        ready = bool(current.get("ready") and current.get("exact_backend_ready")
                     and current.get("exact_backend") == "iop-c")
        units[unit] = dict(ready=ready, backend=current.get("exact_backend"),
                           errors=current.get("errors", []))
    return dict(matching_ready=all(item["ready"] for item in units.values()), units=units)


def publication_status(root, inventory):
    """Expose published IOP coverage without treating it as reviewed acceptance."""
    path = root / "config/iop-publication.json"
    if not path.is_file():
        return dict(state="unpublished", functions=0,
                    units={unit: dict(published=0, total=len(module["functions"]))
                           for unit, module in inventory["units"].items()},
                    independent_review="not_performed", tu_audit="not_applicable_to_iop",
                    accepted_c_functions=0)
    ledger = load_json(path)
    require(ledger.get("schema") == "iop-publication/1" and
            ledger.get("revision") == inventory["revision"],
            "unsupported or stale IOP publication ledger")
    units = {}
    for unit, module in inventory["units"].items():
        entry = ledger.get("units", {}).get(unit, {})
        require(entry.get("original_irx_sha256") == module["sha256"],
                "IOP publication ledger targets a different original: " + unit)
        total = len(module["functions"])
        published = int(entry.get("published_functions", 0))
        require(0 <= published <= total, "invalid IOP published function count: " + unit)
        units[unit] = dict(published=published, total=total,
                           raw_irx_identical=entry.get("raw_irx_identical") is True,
                           compiler_emission_identity=entry.get("compiler_emission_identity"))
    return dict(state=ledger.get("state", "unknown"),
                functions=sum(item["published"] for item in units.values()),
                total_functions=sum(item["total"] for item in units.values()),
                units=units, independent_review=ledger.get("independent_review", "not_performed"),
                tu_audit=ledger.get("tu_audit", "not_applicable_to_iop"),
                accepted_c_functions=int(ledger.get("accepted_c_functions", 0)),
                data_credit=int(ledger.get("data_credit", 0)),
                queue_closure=ledger.get("queue_closure"))


def private_destination(root, path):
    """A fresh nonsymlink location in generated private output only."""
    path = path if path.is_absolute() else root / path
    require(".." not in path.parts, "output traversal refused")
    require(not path.exists() and not path.is_symlink(), "output must be fresh")
    require(all(not p.is_symlink() for p in (path, *path.parents)), "output symlink refused")
    require(any(path.resolve().is_relative_to(root / base) and
                path.resolve() != root / base for base in ("build", ".work")),
            "output must be below ROOT/build or ROOT/.work")
    return path


def encode(value):
    return json.dumps(value, indent=2, sort_keys=True) + "\n"


def module_bytes(root, module):
    data = (root / "game/extracted-iso" / module["path"]).read_bytes()
    require(digest(data) == module["sha256"], "original changed after verification")
    return data


def section_bytes(data, section):
    return data[section["file_offset"]:section["file_offset"] + section["size"]]


def loader_tables(module, data):
    """Bounded decoding of the observed import/export table shapes.

    A zero export pointer with R_MIPS_32 is a relocated pointer to offset zero,
    not the terminator. Import veneers are metadata, never extra functions.
    This is structural evidence, not a loader simulation or ABI certification.
    """
    symbols = defaultdict(list)
    for symbol in module["symbols"]:
        if symbol["original_name"] and 0 < symbol["section_index"] < len(module["sections"]):
            symbols[symbol["section_index"], symbol["value"]].append(symbol["original_name"])
    rels = defaultdict(list)
    for section in module["relocations"]:
        for relocation in section["records"]:
            rels[section["target_section"], relocation["module_offset"]].append(relocation)
    tables = []
    occupied = set()
    for section in module["sections"]:
        if section["type"] != 1 or not section["flags"] & 4:
            continue
        payload = section_bytes(data, section)
        for offset in range(0, len(payload) - 19, 4):
            magic, next_table, version, mode = struct.unpack_from("<IIHH", payload, offset)
            if magic not in (0x41e00000, 0x41c00000):
                continue
            name_bytes = payload[offset + 12:offset + 20]
            raw_name = name_bytes.split(b"\0", 1)[0]
            require(next_table == 0 and mode == 0 and version != 0 and raw_name and
                    all(32 <= byte < 127 for byte in raw_name) and
                    name_bytes == raw_name.ljust(8, b"\0"), "unexpected loader table header")
            kind = "import" if magic == 0x41e00000 else "export"
            entries, cursor = [], offset + 20
            while True:
                stride = 8 if kind == "import" else 4
                require(cursor + stride <= len(payload), "unterminated loader table")
                address = section["module_offset"] + cursor
                value = struct.unpack_from("<I", payload, cursor)[0]
                at = rels.get((section["name"], address), [])
                if kind == "import":
                    instruction = struct.unpack_from("<I", payload, cursor + 4)[0]
                    if value == instruction == 0:
                        cursor += stride
                        break
                    require(value == 0x03e00008 and instruction >> 16 == 0x2400,
                            "unexpected import veneer encoding")
                    entries.append(dict(ordinal=instruction & 65535,
                                        module_offset=address,
                                        original_names=sorted(set(symbols[section["index"], address]))))
                else:
                    if value == 0 and not at:
                        cursor += stride
                        break
                    require(len(at) == 1 and at[0]["type"] == 2 and at[0]["info"] >> 8 == 0,
                            "unexpected export pointer relocation")
                    require(value % 4 == 0 and section["module_offset"] <= value <
                            section["module_offset"] + section["size"], "export outside executable section")
                    entries.append(dict(ordinal=len(entries), module_offset=value,
                                        slot_module_offset=address,
                                        relocation=at[0],
                                        original_names=sorted(set(symbols[section["index"], value]))))
                cursor += stride
            positions = {(section["index"], p) for p in range(offset, cursor, 4)}
            require(not occupied.intersection(positions), "overlapping loader tables")
            occupied.update(positions)
            tables.append(dict(kind=kind, library=raw_name.decode("ascii"), version_raw=version,
                               version=f"{version >> 8}.{version & 255}", mode=mode,
                               module_offset=section["module_offset"] + offset,
                               file_offset=section["file_offset"] + offset, size=cursor - offset,
                               sha256=digest(payload[offset:cursor]), entries=entries))
    require(any(t["kind"] == "import" for t in tables), "no import tables found")
    return tables


def debug_associations(module, data):
    section = next(s for s in module["sections"] if s["name"] == ".mdebug")
    debug = debug_records(data, section)
    associations = defaultdict(list)
    for record in debug["records"]:
        procedures = [s for s in record["symbols"] if s["kind"] in (6, 14)]
        for function in module["functions"]:
            matches = [p for p in procedures if p["address"] == function["module_offset"] and
                       p["name"] in function["aliases"]]
            if matches:
                associations[function["key"]].append(dict(
                    source=record["source"], fdr_index=record["index"],
                    fdr_file_offset=record["fdr_offset"], language_code=record["language_code"],
                    symbols=matches,
                    evidence="ECOFF procedure name/address agrees with original ELF FUNC; not complete TU or type recovery"))
    return debug, associations


def prepare_module(root, unit, module):
    data = module_bytes(root, module)
    tables = loader_tables(module, data)
    debug, associations = debug_associations(module, data)
    by_address = defaultdict(list)
    for symbol in module["symbols"]:
        if symbol["original_name"] and 0 < symbol["section_index"] < len(module["sections"]):
            by_address[symbol["value"]].append(symbol["original_name"])
    imports = {entry["module_offset"]: dict(entry, library=table["library"],
               version_raw=table["version_raw"]) for table in tables if table["kind"] == "import"
               for entry in table["entries"]}
    relocations = [dict(r, relocation_section=s["section"], target_section=s["target_section"],
                        type_name=RELOCATIONS.get(r["type"], f"UNKNOWN_{r['type']}"))
                   for s in module["relocations"] for r in s["records"]]
    functions = []
    calls_to = defaultdict(list)
    for function in module["functions"]:
        section = module["sections"][function["section_index"]]
        start, size = function["module_offset"], function["size"]
        file_offset = section["file_offset"] + start - section["module_offset"]
        require(size % 4 == start % 4 == 0, "unaligned MIPS function")
        calls = []
        for index, (word,) in enumerate(struct.iter_unpack("<I", data[file_offset:file_offset + size])):
            pc = start + index * 4
            if word >> 26 not in (2, 3):
                continue
            target = ((pc + 4) & 0xf0000000) | ((word & 0x03ffffff) << 2)
            # Internal branches are already in the owned function. A direct jump
            # outside it is only a possible tail call; semantics need review.
            if word >> 26 == 2 and start <= target < start + size:
                continue
            call = dict(module_offset=pc, target_module_offset=target,
                        kind="jal" if word >> 26 == 3 else "possible_tail_jump",
                        target_names=sorted(set(by_address[target])))
            if target in imports:
                call["import"] = imports[target]
            calls.append(call)
            calls_to[target].append(dict(function=function["key"], module_offset=pc,
                                         kind=call["kind"]))
        functions.append(dict(function, revision="slus-20469-412d448de315", unit=unit,
                              binary_sha256=module["sha256"], file_offset=file_offset,
                              original_symbols=[s for s in module["symbols"] if s["id"] in function["symbol_ids"]],
                              debug_associations=associations[function["key"]],
                              relocations=[r for r in relocations if r["target_section"] == section["name"]
                                           and start <= r["module_offset"] < start + size],
                              direct_transfers=calls))
    for function in functions:
        function["direct_callers"] = calls_to[function["module_offset"]]
    markers = Counter(s["original_name"] for s in module["symbols"]
                      if s["original_name"] in ("gcc2_compiled.", "__gnu_compiled_c"))
    return dict(unit=unit, path=module["path"], binary_sha256=module["sha256"],
                address_kind="module_relative", load_base=None, isa="MIPS-I", endianness="little",
                compiler_evidence=dict(markers=dict(markers), minor_version_proven=False,
                                       selected_contract=None),
                module=module["module"], sections=module["sections"], debug=debug,
                loader_tables=tables, relocation_types=dict(Counter(r["type_name"] for r in relocations)),
                functions=functions, matching_ready=False, accepted_c_functions=0,
                unmapped_debug_functions=[f["key"] for f in functions if not f["debug_associations"]])


def preparation(root):
    inventory = verify_iop(root)
    backend = backend_status(root)
    publication = publication_status(root, inventory)
    units = {unit: prepare_module(root, unit, module) for unit, module in inventory["units"].items()}
    for unit, module in units.items():
        module["matching_ready"] = backend["units"][unit]["ready"]
    exports = {(table["library"], table["version_raw"]): (unit, table)
               for unit, module in units.items() for table in module["loader_tables"]
               if table["kind"] == "export"}
    bridges = []
    for unit, module in units.items():
        for table in module["loader_tables"]:
            if table["kind"] != "import":
                continue
            target = exports.get((table["library"], table["version_raw"]))
            if target is None:
                continue
            target_unit, exported = target
            for entry in table["entries"]:
                require(entry["ordinal"] < len(exported["entries"]), "import ordinal outside project export table")
                destination = exported["entries"][entry["ordinal"]]
                bridges.append(dict(library=table["library"], version_raw=table["version_raw"],
                                    ordinal=entry["ordinal"], importer_unit=unit, importer=entry,
                                    exporter_unit=target_unit, exporter=destination,
                                    evidence="Original import ordinal and exact library version map to original export slot; no shared prototype inferred"))
    return inventory, dict(schema="iop-preparation/1", revision=inventory["revision"],
                           manifest_sha256=inventory["manifest_sha256"], analysis_only=True,
                           matching_ready=backend["matching_ready"], backend=backend,
                           accepted_c_functions=publication["accepted_c_functions"],
                           publication=publication, semantic_work=SEMANTIC_WORK,
                           blockers=[dict(id=unit, errors=item["errors"])
                                     for unit, item in backend["units"].items() if not item["ready"]],
                           loader_format_reference=FORMAT_SOURCE, units=units, module_bridges=bridges)


def summary(prepared):
    return dict(schema=prepared["schema"], ok=True, matching_ready=prepared["matching_ready"],
                backend=prepared["backend"], semantic_work=prepared["semantic_work"],
                accepted_c_functions=prepared["accepted_c_functions"],
                published_coverage=prepared["publication"], blockers=prepared["blockers"],
                module_bridges=len(prepared["module_bridges"]), units={
                    unit: dict(functions=len(module["functions"]),
                               code_bytes=sum(f["size"] for f in module["functions"]),
                               debug_files=module["debug"]["file_count"],
                               functions_without_debug=len(module["unmapped_debug_functions"]),
                               compiler_evidence=module["compiler_evidence"],
                               relocation_types=module["relocation_types"],
                               loader_tables=[dict(kind=t["kind"], library=t["library"],
                                                   version=t["version"], entries=len(t["entries"]))
                                              for t in module["loader_tables"]])
                    for unit, module in prepared["units"].items()})


def disassemble(root, unit, function):
    lock = root / "config/iop-toolchains.json"
    config = load_json(lock if lock.is_file() else root / "config/toolchains.json")
    path = config["tools"]["objdump"]
    tool = root / path
    require(not Path(path).is_absolute() and ".." not in Path(path).parts and
            tool.resolve().is_relative_to(root / ".work/toolchains"), "unsafe objdump path")
    pins = [p["sha256"] for p in config["pins"] if p["path"] == path]
    require(len(pins) == 1 and digest(tool.read_bytes()) == pins[0], "objdump pin mismatch")
    original = root / "game/extracted-iso" / UNIT_PATHS[unit]
    command = [str(tool), "-dr", "-m", "mips:3000", "--section=.text",
               f"--start-address={function['module_offset']}",
               f"--stop-address={function['module_offset'] + function['size']}", str(original)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=30,
                            env=dict(os.environ, LC_ALL="C", LANG="C"), check=True)
    require(not result.stderr.strip(), "objdump reported diagnostics")
    require(digest(original.read_bytes()) == function["binary_sha256"], "original changed during disassembly")
    return dict(command=command, tool_sha256=pins[0], stdout=result.stdout)


class IopContextIndex:
    """Verified module-relative input for the dispatcher and legacy worker.

    This adapter never invokes the EE ELF reader or infers an EE ABI. The
    shared assembly renderer below only formats already decoded MIPS-I words.
    """

    def __init__(self, unit, root=ROOT):
        require(unit in UNIT_PATHS, "unknown IOP context unit")
        self.unit, self.root = unit, Path(root).resolve()
        verified = verify_iop(self.root)
        self.original = verified["units"][unit]
        self.data = module_bytes(self.root, self.original)
        self.module = prepare_module(self.root, unit, self.original)
        self.functions = {f["module_offset"]: f for f in self.module["functions"]}
        self._symbols = defaultdict(list)
        for symbol in self.original["symbols"]:
            if symbol["original_name"] and 0 < symbol["section_index"] < len(self.original["sections"]):
                self._symbols[symbol["value"]].append(symbol)

    def function_name(self, address):
        require(address in self.functions, "context address is not an original FUNC")
        return self.functions[address]["aliases"][0]

    def reference_name(self, address):
        choices = [s for s in self._symbols.get(address, ())
                   if re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", s["original_name"])
                   and s["original_name"] not in ("gcc2_compiled", "__gnu_compiled_c")]
        choices.sort(key=lambda s: (s["type"] != "FUNC", s["binding"] != "GLOBAL",
                                    not s["size"], s["index"]))
        return choices[0]["original_name"] if choices else None

    def section_of(self, address):
        sections = [s for s in self.original["sections"] if s["flags"] & 2 and
                    s["module_offset"] <= address < s["module_offset"] + s["size"]]
        if len(sections) != 1:
            return None
        section = dict(sections[0], va=f"0x{sections[0]['module_offset']:08x}")
        if section["type"] == 8:
            section["file_offset"] = None
        return section


def context_directory_names(index, addresses):
    counts = Counter(index.function_name(address) for address in addresses)
    result = {}
    for address in addresses:
        name = index.function_name(address)
        safe = re.sub(r"[^A-Za-z0-9_.-]", "_", name) or f"func_{address:08x}"
        result[address] = safe if counts[name] == 1 else f"{safe}-{address:08x}"
    return result


def write_function_context(index, address, directory=None):
    """The four context artifacts consumed by worker init, m2c and dispatch."""
    from tools.function_context import (ContextError, build_assembly, find_switch_tables,
                                        parse_listing, validate_m2c_assembly)
    require(address in index.functions, "IOP context is not a nonempty original FUNC")
    function = index.functions[address]
    name = index.function_name(address)
    listing = disassemble(index.root, index.unit, function)
    # Numeric registers and explicit nops preserve o32 register identities and
    # every load/branch delay slot in the m2c input, including offset zero.
    command = list(listing["command"])
    command[1:1] = ["-z", "-M", "no-aliases,reg-names=numeric"]
    command[command.index("-dr")] = "-d"
    decoded = subprocess.run(command, capture_output=True, text=True, timeout=30,
                             env=dict(os.environ, LC_ALL="C", LANG="C"), check=True)
    require(not decoded.stderr.strip(), "IOP context disassembler reported diagnostics")
    require(digest(module_bytes(index.root, index.original)) == index.original["sha256"],
            "IOP original changed during context generation")
    records = parse_listing(decoded.stdout)
    start = function["file_offset"]
    body = index.data[start:start + function["size"]]
    words = [entry[0] for entry in struct.iter_unpack("<I", body)]
    require(len(records) == len(words), "MIPS-I context disassembly is incomplete")
    tables = find_switch_tables(index, address, words)
    assembly_name = name if re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", name) else f"func_{address:08X}"
    assembly = build_assembly(index, address, function["size"], assembly_name, records, words, tables)
    # Generic m2c recognizes o32 register roles through these canonical names.
    # Keep the numeric decoder evidence above, but render its source input with
    # aliases so jr $ra is recognized as a return rather than an indirect jump.
    gprs = ("zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3",
            "t4", "t5", "t6", "t7", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
            "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra")
    assembly = re.sub(r"\$(\d+)\b", lambda match: "$" + gprs[int(match[1])], assembly)
    try:
        validate_m2c_assembly(assembly)
        m2c = dict(valid=True, reason=None, isa="mips1", abi="o32")
    except (ValueError, ContextError) as exc:
        m2c = dict(valid=False, reason=str(exc), isa="mips1", abi="o32")
    branches = sum(word >> 26 in (1, 4, 5, 6, 7) for word in words)
    calls = function["direct_transfers"]
    indirect = sum(word >> 26 == 0 and word & 63 == 9 for word in words)
    gp_count = sum((word >> 21) & 31 == 28 for word in words if word >> 26 != 0)
    frames = [-(word & 65535) + 65536 for word in words if word >> 26 == 9
              and (word >> 21) & 31 == (word >> 16) & 31 == 29 and word & 0x8000]
    cls = ("trivial" if len(words) <= 8 and not calls and not branches else
           "easy" if len(words) <= 32 and len(calls) <= 2 else
           "medium" if len(words) <= 128 else "hard" if len(words) <= 512 else "huge")
    context = dict(
        schema_version=1, generator="tools/iop_prepare.py", analysis_only=True,
        identity=dict(unit=index.unit, name=name, va=f"0x{address:08x}",
                      address_kind="module_relative", size=function["size"],
                      file_offset=start, code_sha256=function["sha256"],
                      binary=dict(path="game/extracted-iso/" + index.original["path"],
                                  sha256=index.original["sha256"]),
                      elf_names=function["aliases"], section=".text",
                      section_index=function["section_index"]),
        **{key: function[key] for key in ("relocations", "direct_transfers", "debug_associations")},
        callers=function["direct_callers"],
        callees=[dict(name=entry["target_names"][0] if entry["target_names"] else
                      f"func_{entry['target_module_offset']:08X}",
                      va=f"0x{entry['target_module_offset']:08x}", kind=entry["kind"])
                 for entry in calls],
        gp_usage=dict(uses_gp=bool(gp_count), count=gp_count,
                      gp=f"0x{index.original['module']['gp']:08x}", gp_source="original .iopmod"),
        cop2=dict(present=any(word >> 26 in (18, 50, 58) for word in words),
                  count=sum(word >> 26 in (18, 50, 58) for word in words), instructions=[]),
        fpu=dict(present=any(word >> 26 in (17, 49, 57) for word in words),
                 count=sum(word >> 26 in (17, 49, 57) for word in words), instructions=[]),
        stack=dict(frame_size=max(frames, default=0), callee_saved_regs=[]),
        indirect_calls=indirect, instruction_count=len(words), branch_count=branches,
        leaf=not calls and not indirect, tail_call=any(c["kind"] == "possible_tail_jump" for c in calls),
        data_refs=[], float_constants=[],
        switch_table=dict(present=bool(tables), tables=tables), m2c_input=m2c,
        tractability=dict(cls=cls, score=len(words) + 3 * branches + 6 * len(calls),
                          suggested_route="c", suggested_profile_hints=["-G0"],
                          reasons=["IOP MIPS-I/o32 analysis; source and exactness require the IOP runner"]),
        limitations=["Context is analysis, never source or acceptance proof.",
                     "Raw IRX relocations and original module identity remain authoritative.",
                     "Indirect targets, types and load-delay semantics require worker analysis."])
    if directory is not None:
        directory = Path(directory)
        directory.mkdir(parents=True, exist_ok=True)
        payloads = {"function.s": assembly.encode("ascii"), "original.bin": body,
                    "original.asm.log": listing["stdout"].encode("utf-8")}
        for filename, payload in payloads.items():
            with (directory / filename).open("xb") as stream:
                stream.write(payload)
        context["artifacts"] = {filename: digest(payload) for filename, payload in payloads.items()}
        with (directory / "context.json").open("x", encoding="utf-8") as stream:
            stream.write(encode(context))
    return context


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    sub = parser.add_subparsers(dest="command", required=True)
    status = sub.add_parser("status", help="verify inputs and report research/matching readiness")
    status.add_argument("--require-matching-ready", action="store_true")
    bundle = sub.add_parser("bundle", help="write fresh private research inputs for both modules")
    bundle.add_argument("--out", type=Path, required=True)
    function = sub.add_parser("function", help="one original function, including offset zero")
    function.add_argument("--unit", choices=tuple(UNIT_PATHS), required=True)
    function.add_argument("--function", required=True, help="original name or module-relative offset")
    function.add_argument("--disassemble", action="store_true")
    args = parser.parse_args(argv)
    try:
        root = args.root.resolve()
        destination = private_destination(root, args.out) if args.command == "bundle" else None
        inventory, prepared = preparation(root)
        if args.command == "bundle":
            destination.mkdir(parents=True, exist_ok=False)
            files = {"inventory.json": encode(inventory), "preparation.json": encode(prepared),
                     "summary.json": encode(summary(prepared)),
                     "README.md": "# SSD / RSSD preparation evidence\n\n"
                     "Verified original identities, ELF/ECOFF records, relocations and loader tables.\n"
                     "This bundle grants no matching or recovery credit. See summary.json for blockers.\n"
                     "Use tools/iop_prepare.py function --unit UNIT --function NAME --disassemble\n"
                     "for a bounded original function context. See docs/iop-preparation.md.\n"}
            for name, content in files.items():
                with (destination / name).open("x", encoding="utf-8") as stream:
                    stream.write(content)
            print(encode(dict(ok=True, out=str(destination), files=list(files),
                              matching_ready=prepared["matching_ready"], accepted_c_functions=0)), end="")
        elif args.command == "function":
            module = prepared["units"][args.unit]
            try:
                address = int(args.function, 0)
            except ValueError:
                address = None
            matches = [f for f in module["functions"] if args.function in f["aliases"] or
                       address is not None and f["module_offset"] == address]
            require(len(matches) == 1, "function selector must identify exactly one original FUNC")
            result = dict(matches[0], analysis_only=True, matching_ready=module["matching_ready"])
            if args.disassemble:
                result["disassembly"] = disassemble(root, args.unit, matches[0])
            print(encode(result), end="")
        else:
            print(encode(summary(prepared)), end="")
            if args.require_matching_ready and not prepared["matching_ready"]:
                return 1
        return 0
    except (OSError, ValueError, KeyError, TypeError, struct.error, subprocess.SubprocessError) as exc:
        print(encode(dict(ok=False, error=str(exc), matching_ready=False)), end="")
        return 1


if __name__ == "__main__":
    sys.exit(main())

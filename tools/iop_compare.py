#!/usr/bin/env python3
"""Compare a candidate SSD/RSSD IRX with its immutable complete original.

This is an identity comparison, not a matching backend or source acceptance gate.
Exit 0 requires a separate, structurally valid file with every byte identical.
Relocations, metadata, debug bytes, padding and symbol order are never normalized.
"""

import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import iop_inventory as inventory

MAX_CANDIDATE_BYTES = 4 * 1024 * 1024
DIAGNOSTIC_LIMIT = 20


def require(condition, message):
    if not condition:
        raise ValueError(message)


def plain_path(path):
    """Require an absolute path whose existing components are not symlinks."""
    path = Path(os.path.abspath(path))
    require(all(not part.is_symlink() for part in (path, *path.parents)),
            "symlink paths are not allowed")
    return path


def read_candidate(path):
    require(path.is_file(), "candidate must be a regular file")
    size = path.stat().st_size
    require(0 < size <= MAX_CANDIDATE_BYTES,
            f"candidate size must be between 1 and {MAX_CANDIDATE_BYTES} bytes")
    with path.open("rb") as stream:
        data = stream.read(MAX_CANDIDATE_BYTES + 1)
    require(len(data) == size, "candidate changed while reading")
    return data


def output_path(value, root):
    if value is None:
        return None
    path = plain_path(value if value.is_absolute() else root / value)
    require(path.suffix == ".json" and not path.exists(),
            "output must be a fresh JSON file")
    private_roots = (root / ".work", root / "build", Path("/tmp"))
    require(any(path.is_relative_to(directory) for directory in private_roots),
            "output must stay under ROOT/.work, ROOT/build or /tmp")
    require(path.parent.is_dir(), "output parent must already exist")
    return path


def fields_changed(original, candidate):
    return {key: {"original": original.get(key), "candidate": candidate.get(key)}
            for key in sorted(original.keys() | candidate.keys())
            if original.get(key) != candidate.get(key)}


def sequence_changes(original, candidate):
    """Keep record order and report a bounded sample without matching by name."""
    count = 0
    sample = []
    for index in range(max(len(original), len(candidate))):
        before = original[index] if index < len(original) else None
        after = candidate[index] if index < len(candidate) else None
        if before == after:
            continue
        count += 1
        if len(sample) < DIAGNOSTIC_LIMIT:
            sample.append(dict(index=index, original=before, candidate=after))
    return dict(original_count=len(original), candidate_count=len(candidate),
                changed_records=count, sample=sample, sample_limit=DIAGNOSTIC_LIMIT)


def raw_changes(original, candidate):
    count = 0
    sample = []
    common = min(len(original), len(candidate))
    for offset in range(common):
        if original[offset] != candidate[offset]:
            count += 1
            if len(sample) < DIAGNOSTIC_LIMIT:
                sample.append(dict(file_offset=offset, original=original[offset],
                                   candidate=candidate[offset]))
    count += abs(len(original) - len(candidate))
    if len(original) != len(candidate) and len(sample) < DIAGNOSTIC_LIMIT:
        sample.append(dict(file_offset=common, original_tail_size=len(original) - common,
                           candidate_tail_size=len(candidate) - common))
    return dict(different_byte_count=count, sample=sample, sample_limit=DIAGNOSTIC_LIMIT)


def header_bytes(data):
    """Raw ELF32 fields for diagnostics, including an invalid candidate header."""
    fields = ("type", "machine", "version", "entry", "program_offset",
              "section_offset", "flags", "header_size", "program_entry_size",
              "program_count", "section_entry_size", "section_count", "string_section")
    result = dict(ident_hex=data[:16].hex())
    if len(data) >= 52:
        result.update(zip(fields, struct.unpack_from("<HHIIIIIHHHHHH", data, 16)))
    return result


def compare(unit, candidate, root=ROOT):
    root = Path(root).resolve()
    candidate = plain_path(candidate if Path(candidate).is_absolute() else root / candidate)
    require(unit in inventory.UNIT_PATHS, "unknown IOP unit")
    references = inventory.verify_iop(root)
    expected = references["units"][unit]
    original_path = root / "game/extracted-iso" / expected["path"]
    for reference in references["units"].values():
        path = root / "game/extracted-iso" / reference["path"]
        require(not candidate.samefile(path), "candidate must be separate from both originals")
    data = read_candidate(candidate)
    original = original_path.read_bytes()
    require(inventory.digest(original) == expected["sha256"],
            "original changed after verification")
    candidate_hash = inventory.digest(data)
    equal_bytes = data == original
    result = dict(schema_version=1, kind="iop_complete_file_identity_comparison",
                  ok=False, unit=unit, revision=references["revision"], valid_irx=False,
                  exact_bytes=False, identity_comparison_only=True,
                  source_acceptance=False, recovery_credit=False,
                  compiler_emission_proven=False, matching_backend_ready=False,
                  comparison_rule="complete raw bytes and SHA-256; no normalization",
                  original=dict(path=str(original_path), size=len(original),
                                sha256=expected["sha256"]),
                  candidate=dict(path=str(candidate), size=len(data), sha256=candidate_hash),
                  reader=dict(path=references["reader"]["path"],
                              sha256=references["reader"]["sha256"]),
                  diagnostics=dict(raw_bytes=raw_changes(original, data),
                                   header=fields_changed(header_bytes(original), header_bytes(data))))
    reader = root / references["reader"]["path"]
    try:
        decoded = subprocess.run([str(reader), "-hSW", "-sW", str(candidate)],
                                 capture_output=True, text=True, encoding="ascii",
                                 env=dict(os.environ, LC_ALL="C", LANG="C"),
                                 timeout=30, check=True)
        require(not decoded.stderr.strip(), "readelf reported candidate diagnostics")
        record = inventory.describe_irx(data, unit, decoded.stdout)
        require(read_candidate(candidate) == data, "candidate changed during readelf")
        # The unit argument cannot relabel RSSD bytes as SSD or vice versa.
        require(record["module"]["name"] == expected["module"]["name"],
                "candidate module name does not match the requested unit")
    except (OSError, ValueError, KeyError, TypeError, UnicodeError, struct.error,
            subprocess.SubprocessError) as exc:
        result["error"] = str(exc)
        return result
    result["valid_irx"] = True
    before_header = dict(expected["header"], ident_hex=original[:16].hex(),
                         program_headers_sha256=expected["program_headers_sha256"])
    after_header = dict(record["header"], ident_hex=data[:16].hex(),
                        program_headers_sha256=record["program_headers_sha256"])
    result["diagnostics"].update(
        header=fields_changed(before_header, after_header),
        module=fields_changed(expected["module"], record["module"]),
        sections=sequence_changes(expected["sections"], record["sections"]),
        symbols=sequence_changes(expected["symbols"], record["symbols"]),
        relocations=sequence_changes(
            [dict(section=section["section"], index=index, **relocation)
             for section in expected["relocations"]
             for index, relocation in enumerate(section["records"])],
            [dict(section=section["section"], index=index, **relocation)
             for section in record["relocations"]
             for index, relocation in enumerate(section["records"])])
    )
    result["exact_bytes"] = equal_bytes and candidate_hash == expected["sha256"]
    result["ok"] = result["exact_bytes"]
    if not result["ok"]:
        result["error"] = "candidate differs from the immutable complete original"
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--unit", choices=tuple(inventory.UNIT_PATHS), required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        root = args.root.resolve()
        destination = output_path(args.output, root)
        result = compare(args.unit, args.candidate, root)
        if destination:
            with destination.open("x", encoding="utf-8") as stream:
                stream.write(json.dumps(result, sort_keys=True, indent=2) + "\n")
        print(json.dumps(result, sort_keys=True))
        return 0 if result["ok"] else 1
    except (OSError, ValueError, KeyError, TypeError, UnicodeError, struct.error,
            subprocess.SubprocessError) as exc:
        print(json.dumps(dict(ok=False, schema_version=1,
                              kind="iop_complete_file_identity_comparison",
                              unit=args.unit, valid_irx=False, exact_bytes=False,
                              identity_comparison_only=True, source_acceptance=False,
                              recovery_credit=False, compiler_emission_proven=False,
                              matching_backend_ready=False, error=str(exc)), sort_keys=True))
        return 1


if __name__ == "__main__":
    sys.exit(main())

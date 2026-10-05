#!/usr/bin/env python3
"""Synthesize the private-shaped manifests a public clone needs.

OpokXeno does not publish config/tu-build.json (per-TU ledger) or
config/tu/data-carves.json (C-owned data runs).  After a successful
./configure.py (which writes build/*/tu-manifest.json and
build/*/compile-manifest.json), this script:

  1. Fills config/tu-build.json `tus` from those manifests.
  2. Seeds config/tu/data-carves.json from config/split/main.data-splits.json.
  3. Optionally (--derive) runs tools/tu/data_carve.py derive --write for every
     C object that still emits a local .rodata/.lit4/.sdata.
  4. Optionally (--tail-from-mapcheck PATH) records text.code_end from a prior
     main.mapcheck.json so 4-byte tail pads are restored on link.

After a full splat, also run scripts/fix_rodata_local_labels.py
(GAS drops .L* jumptable locals).

Then re-run the ninja generators (not a full splat) and rebuild:

  for u in ov01 ov02 ov10 ov11 ov12; do
    python3 -B tools/tu/ninja_ovl.py build/$u $u --manifest build/$u/compile-manifest.json --root .
  done
  python3 -B tools/tu/ninja_main.py --root . --unit-dir build/main
  ninja && ninja gate
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OVERLAYS = ("ov01", "ov02", "ov10", "ov11", "ov12")
OD_CANDIDATES = [
    ROOT / ".work/toolchains/ee-binutils/bin/mips64r5900el-ps2-elf-objdump",
]


def objdump() -> Path:
    for p in OD_CANDIDATES:
        if p.is_file():
            return p
    raise SystemExit("mips64r5900el-ps2-elf-objdump not found under .work/toolchains")


def section_size(od: Path, obj: Path, sec: str) -> int:
    out = subprocess.check_output([str(od), "-h", str(obj)], text=True, errors="replace")
    for line in out.splitlines():
        parts = line.split()
        if sec in parts:
            i = parts.index(sec)
            return int(parts[i + 1], 16)
    return 0


def synthesize_tu_build(root: Path) -> int:
    path = root / "config/tu-build.json"
    stub = json.loads(path.read_text()) if path.is_file() else {
        "schema": "tu-build/1",
        "units": json.loads((root / "config/originals.json").read_text()).get("units", {}),
    }
    tus = []
    main = json.loads((root / "build/main/tu-manifest.json").read_text())
    for t in main["tus"]:
        text = t["text"]
        entry = {
            "id": t["id"],
            "unit": "main",
            "name": t["name"],
            "path": t.get("path") or f"src/main/{t['name']}.c",
            "text": {
                "start": text.get("start") or text["splat_range"][0],
                "end": text.get("end") or text["splat_range"][1],
            },
        }
        if text.get("code_end"):
            entry["text"]["code_end"] = text["code_end"]
        if t.get("owners"):
            entry["data_ownership"] = {
                sec: {"owner": own} for sec, own in t["owners"].items() if sec != ".text"
            }
        tus.append(entry)
    for unit in OVERLAYS:
        man = json.loads((root / f"build/{unit}/compile-manifest.json").read_text())
        for t in man["units"][unit]["tus"]:
            tus.append({
                "id": t["id"],
                "unit": unit,
                "name": t["name"],
                "path": t.get("source") or f"scaffold/src/{unit}/{t['name']}.c",
                "text": {"start": t["text"][0], "end": t["text"][1]},
            })
    # Preserve any code_end already recorded
    old = {t["id"]: t for t in stub.get("tus") or []}
    for t in tus:
        prev = old.get(t["id"])
        if prev and (prev.get("text") or {}).get("code_end"):
            t["text"]["code_end"] = prev["text"]["code_end"]
            t["text"].setdefault("start", prev["text"].get("start"))
            t["text"].setdefault("end", prev["text"].get("end"))
    # elf_gate Target resolves unit file as root/record["file"]; make it absolute
    # under game_dir (originals/1 does this rewrite; tu-build/1 does not).
    import os
    game = os.environ.get("XENO_GAME_DIR")
    if not game:
        local = root / "config/toolchain-local.json"
        if local.is_file():
            game = json.loads(local.read_text()).get("game_dir")
    game = Path(game).expanduser() if game else root / "game/extracted-iso"
    units = stub.setdefault("units", {})
    for name, rec in list(units.items()):
        f = Path(rec.get("file") or name)
        if not f.is_absolute():
            rec = dict(rec)
            rec["file"] = str(game / f.name)
            units[name] = rec
    stub["schema"] = "tu-build/1"
    stub["tus"] = tus
    stub["note"] = (
        "Public-clone synthesized tus from build/*/tu-manifest.json and "
        "build/*/compile-manifest.json. C-owned runs live in config/tu/data-carves.json."
    )
    path.write_text(json.dumps(stub, indent=1) + "\n")
    return len(tus)


def seed_data_carves(root: Path) -> int:
    splits = json.loads((root / "config/split/main.data-splits.json").read_text())
    tb = json.loads((root / "config/tu-build.json").read_text())
    by_ord = {int(t["id"].split("tu")[1]): t for t in tb["tus"] if t["unit"] == "main"}
    detail = splits.get("detail") or {}
    name_detail = {}
    for key, info in detail.items():
        name, sec = key.split(":")
        name_detail.setdefault(name, {})["." + sec.lstrip(".")] = info
    reg_path = root / "config/tu/data-carves.json"
    reg = json.loads(reg_path.read_text()) if reg_path.is_file() else {
        "schema": "tu-data-carves/1", "tus": {}
    }
    reg["schema"] = "tu-data-carves/1"
    reg.setdefault("tus", {})
    n = 0
    for ord_s, secs in splits["tus"].items():
        tu = by_ord[int(ord_s)]
        tu_id, name = tu["id"], tu["name"]
        entry = reg["tus"].setdefault(tu_id, {})
        for sec, ranges in secs.items():
            if entry.get(sec):
                continue
            info = (name_detail.get(name) or {}).get(sec) or {}
            runs = []
            for lo, hi in ranges:
                run = {
                    "range": [lo, hi],
                    "generated": [lo, hi],
                    "generated_by": [],
                    "c_owned_symbols": info.get("c_owned") or [],
                    "also_referenced_by_c": info.get("also_referenced_by_c") or [],
                    "basis": {"kind": "import-era", "source": "config/split/main.data-splits.json"},
                }
                gens = info.get("generated") or []
                if gens:
                    g = gens[0]
                    run["generated"] = [g[0], g[1]]
                    if len(g) > 2:
                        run["generated_by"] = [g[2]]
                runs.append(run)
            entry[sec] = runs
            n += 1
    reg_path.parent.mkdir(parents=True, exist_ok=True)
    reg_path.write_text(json.dumps(reg, indent=1) + "\n")
    return n


def derive_leftovers(root: Path) -> list[str]:
    od = objdump()
    done = []
    # overlays
    for unit in OVERLAYS:
        man = json.loads((root / f"build/{unit}/compile-manifest.json").read_text())
        for t in man["units"][unit]["tus"]:
            if t.get("kind") != "c":
                continue
            name = t["name"]
            src = root / (t.get("source") or f"src/{unit}/{name}.c")
            obj = root / f"build/{unit}/build/scaffold/src/{unit}/{name}.o"
            if not obj.is_file():
                obj = root / f"build/{unit}/build/src/{unit}/{name}.o"
            if not src.is_file() or not obj.is_file():
                continue
            if section_size(od, obj, ".rodata") == 0:
                continue
            cmd = [sys.executable, "-B", str(root / "tools/tu/data_carve.py"), "derive",
                   "--root", str(root), "--unit", unit, "--unit-dir", str(root / f"build/{unit}"),
                   "--tu", t["id"], "--source", str(src), "--obj", str(obj), "--write"]
            print("+", " ".join(cmd))
            subprocess.check_call(cmd)
            done.append(t["id"])
    # main
    main = json.loads((root / "build/main/tu-manifest.json").read_text())
    for t in main["tus"]:
        if t.get("mode") != "c":
            continue
        name = t["name"]
        src = root / f"src/main/{name}.c"
        obj = root / f"build/main/build/c/{name}.o"
        if not src.is_file() or not obj.is_file():
            continue
        if all(section_size(od, obj, s) == 0 for s in (".rodata", ".lit4", ".sdata")):
            continue
        cmd = [sys.executable, "-B", str(root / "tools/tu/data_carve.py"), "derive",
               "--root", str(root), "--unit", "main", "--unit-dir", str(root / "build/main"),
               "--tu", t["id"], "--source", str(src), "--obj", str(obj), "--write"]
        print("+", " ".join(cmd))
        rc = subprocess.call(cmd)
        if rc == 0:
            done.append(t["id"])
        else:
            print(f"warn: derive failed for {t['id']} (rc={rc})", file=sys.stderr)
    return done


def apply_tail_from_mapcheck(root: Path, mapcheck: Path) -> int:
    mc = json.loads(mapcheck.read_text())
    tb = json.loads((root / "config/tu-build.json").read_text())
    n = 0
    for t in tb["tus"]:
        if t["unit"] != "main":
            continue
        info = (mc.get("c_tus") or {}).get(t["name"])
        if not info or not info.get(".text"):
            continue
        gap = info[".text"].get("gap") or 0
        if gap != 4:
            continue
        end = int(info[".text"]["expected_end"], 16)
        start = int(info[".text"]["expected_start"], 16)
        if end % 8 != 0:
            continue
        t["text"] = dict(t.get("text") or {})
        t["text"]["start"] = f"0x{start:08x}"
        t["text"]["end"] = f"0x{end:08x}"
        t["text"]["code_end"] = f"0x{end - gap:08x}"
        n += 1
    (root / "config/tu-build.json").write_text(json.dumps(tb, indent=1) + "\n")
    return n


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--derive", action="store_true", help="derive data-carves for C objs with local data")
    ap.add_argument("--tail-from-mapcheck", type=Path, help="main.mapcheck.json from a prior C link")
    a = ap.parse_args()
    root = a.root.resolve()
    n = synthesize_tu_build(root)
    print(f"tu-build.json: {n} tus")
    s = seed_data_carves(root)
    print(f"data-carves.json: seeded {s} import-era section(s)")
    if a.derive:
        done = derive_leftovers(root)
        print(f"derived {len(done)} tu(s): {done}")
    if a.tail_from_mapcheck:
        t = apply_tail_from_mapcheck(root, a.tail_from_mapcheck)
        print(f"tail code_end recorded for {t} tu(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

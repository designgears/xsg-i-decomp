#!/usr/bin/env python3
"""Integrate a C function that defines GNU nested functions.

The parent's nested helpers are separate INCLUDE_ASM slots in our TUs (splat
splits `name.N` into `name_N`), so the parent can only be integrated together
with removing those slots - one edit integrate.py cannot express. This does the
joint edit under the integrate lock, rebuilds, and keeps it only if the unit is
whole-file identical AND mapcheck is clean (mapcheck accepts the renumbered
nested symbols listed in config/gnu-nested-functions.json); otherwise it
restores the exact previous source and rebuilds.

    integrate_nested.py --unit main --tu menu_tec --name MenuTecListMain2 --body B.c

Run try_fn first with --src-patch removing the helper slots: the parent must be
opcode-exact with only symbol/reloc differences.
"""
import argparse
import fcntl
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import integrate  # noqa: E402
import verify_unit  # noqa: E402

LOCK = ROOT / "build/auto_decomp/integrate.lock"


def helper_slots(unit, tu, name):
    reg = json.loads((ROOT / "config/gnu-nested-functions.json").read_text())
    if unit == "main":
        man = json.loads((ROOT / "build/main/tu-manifest.json").read_text())
        tu_id = next((t["id"] for t in man["tus"] if t["name"] == tu), None)
    else:
        man = json.loads((ROOT / f"build/{unit}/compile-manifest.json").read_text())
        tu_id = next((t["id"] for t in man["units"][unit]["tus"] if t["name"] == tu), None)
    tus = reg.get("tus", {})
    entry = tus.get(tu_id) or tus.get((tu_id or "").split("_")[0]) or {}
    spec = entry.get("functions", {}).get(name)
    if not spec:
        raise SystemExit(f"{unit}/{tu}/{name}: no nested-function group in config/gnu-nested-functions.json")
    return [n["original_name"].replace(".", "_") for n in spec["nested_functions"]]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unit", required=True)
    ap.add_argument("--tu", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--body", required=True)
    ap.add_argument("--src-patch", help="json [[path, old, new], ...] applied with the joint edit "
                    "(helper-slot removals in it are harmless; they are removed anyway)")
    a = ap.parse_args()
    src = ROOT / "src" / a.unit / f"{a.tu}.c"
    body = Path(a.body).read_text().rstrip()
    with open(LOCK, "w") as lk:
        fcntl.flock(lk, fcntl.LOCK_EX)
        before = verify_unit.rebuild(ROOT, a.unit)
        if not (before.get("identical") and before.get("mapcheck_ok")):
            print(json.dumps(dict(ok=False, error="unit not green before edit")))
            return 1
        orig = src.read_text()
        text = orig
        removed = []
        asm_dir = f"asm/{a.unit}/nonmatchings/{a.tu}" if a.unit == "main" else f"asm/nonmatchings/{a.unit}/{a.tu}"
        for slot in helper_slots(a.unit, a.tu, a.name):
            line = f'INCLUDE_ASM("{asm_dir}", {slot});\n'
            if line in text:
                text = text.replace(line, "", 1)
                removed.append(slot)
        parent = f'INCLUDE_ASM("{asm_dir}", {a.name});'
        if parent not in text:
            print(json.dumps(dict(ok=False, error="parent INCLUDE_ASM not found")))
            return 1
        text = text.replace(parent, body, 1)
        extra = {}  # other files touched by --src-patch: path -> original text
        if a.src_patch:
            for path, old, new in json.loads(Path(a.src_patch).read_text()):
                fp = ROOT / path
                if fp == src:
                    if old in text:
                        text = text.replace(old, new, 1)
                else:
                    cur = extra.get(fp, fp.read_text()) if fp in extra else fp.read_text()
                    extra.setdefault(fp, fp.read_text())
                    if old not in cur:
                        print(json.dumps(dict(ok=False, error=f"patch text not found in {path}")))
                        return 1
                    fp.write_text(cur.replace(old, new, 1))
        src.write_text(text)
        r = verify_unit.rebuild(ROOT, a.unit)
        if not (r.get("identical") and r.get("mapcheck_ok")):
            integrate.maybe_derive_carve(ROOT, a.unit, a.tu)
            r = verify_unit.rebuild(ROOT, a.unit)
        ok = bool(r.get("identical") and r.get("mapcheck_ok"))
        if not ok:
            src.write_text(orig)
            for fp, t0 in extra.items():
                fp.write_text(t0)
            back = verify_unit.rebuild(ROOT, a.unit)
            print(json.dumps(dict(ok=False, matched=False, removed=removed,
                                  mapcheck_problems=r.get("mapcheck_problems"),
                                  restored_identical=back.get("identical"),
                                  tail=(r.get("ninja_tail") or "")[-600:])))
            return 1
        st_path = ROOT / "build/auto_decomp/state.json"
        st = json.loads(st_path.read_text())
        for k in [f"{a.unit}/{a.tu}/{a.name}"] + [f"{a.unit}/{a.tu}/{s}" for s in removed]:
            if k not in st["matched"]:
                st["matched"].append(k)
        st["matched_count"] = len(st["matched"])
        st_path.write_text(json.dumps(st, indent=1))
        print(json.dumps(dict(ok=True, matched=True, removed=removed, mapcheck_ok=True, identical=True)))
        return 0


if __name__ == "__main__":
    sys.exit(main())

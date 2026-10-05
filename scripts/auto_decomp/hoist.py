#!/usr/bin/env python3
"""Move TU declarations above a function's slot (typedefs/structs/externs/prototypes
the body needs but the TU only declares further down), or rewrite one (--replace).

    python3 -B scripts/auto_decomp/hoist.py --unit ov11 --tu mini_g \
        --replace PoGetBase --text "static short PoGetBase(int cardValue);"

--replace swaps the first file-scope declaration (not a definition) of NAME for
TEXT, e.g. a prototype whose parameter types are wrong. Callers compiled
against it must still produce identical bytes, so the same verify/revert applies.

    python3 -B scripts/auto_decomp/hoist.py --unit main --tu game_defocus \\
        --before DefocusMainType09Final --name DefocusLayer [--name OtherType ...]

Each NAME is a typedef, struct/union/enum tag, variable or function the TU
declares at file scope after the slot of --before. The declaration (with its
attached comment) moves to just above that slot. Pure declaration moves do not
change code, so the unit must stay whole-file identical: the edit runs under
the global integrate lock, rebuilds the unit, and reverts if anything differs.
Prints JSON; exit 0 when moved (or already above).
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "tools"))
import tu_edit  # noqa: E402
from state import locked, log  # noqa: E402
from verify_unit import rebuild  # noqa: E402


def _declares(item_text: str, name: str) -> bool:
    keys = tu_edit.decl_keys(item_text)
    return any(k == name for _, k in keys)


def _attached_start(text: str, items, k: int) -> int:
    start = items[k].start
    j = k - 1
    while j >= 0 and items[j].kind == "comment" and not text[items[j].end:start].strip() \
            and text[items[j].end:start].count("\n") <= 1:
        start = items[j].start
        j -= 1
    while start > 0 and text[start - 1] in " \t":
        start -= 1
    return start


def hoist_text(text: str, before: str, names: list[str]) -> tuple[str, list[str]]:
    moved = []
    for name in names:
        tu = tu_edit.TU(text)
        b = tu.by_name.get(before)
        if b is None:
            raise tu_edit.EditError(f"no function block {before!r}")
        items = tu_edit.scan(text)
        hits = [k for k, it in enumerate(items)
                if it.kind == "decl" and _declares(it.text(text), name)]
        if not hits:
            raise tu_edit.EditError(f"no file-scope declaration of {name!r}")
        if any(items[k].end <= b.start for k in hits):
            continue                              # already visible at the slot
        k = hits[0]
        s, e = _attached_start(text, items, k), items[k].end
        while e < len(text) and text[e] == "\n":
            e += 1
        chunk = text[s:items[k].end].rstrip() + "\n\n"
        text = text[:s] + text[e:]
        tu = tu_edit.TU(text)
        at = tu.by_name[before].start
        text = text[:at] + chunk + text[at:]
        moved.append(name)
    return text, moved


def replace_text(text: str, name: str, new: str) -> tuple[str, list[str]]:
    items = tu_edit.scan(text)
    hits = [it for it in items if it.kind == "decl" and _declares(it.text(text), name)]
    if not hits:
        raise tu_edit.EditError(f"no file-scope declaration of {name!r}")
    it = hits[0]
    return text[:it.start] + new.strip() + text[it.end:], [name]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", required=True)
    ap.add_argument("--tu", required=True)
    ap.add_argument("--before", help="function whose slot the decls must precede")
    ap.add_argument("--name", action="append")
    ap.add_argument("--replace", help="rewrite the declaration of this name")
    ap.add_argument("--text", help="replacement declaration text (with --replace)")
    a = ap.parse_args()
    src = ROOT / "src" / a.unit / f"{a.tu}.c"
    if a.replace:
        if not a.text:
            ap.error("--replace needs --text")
    elif not (a.before and a.name):
        ap.error("--before and --name are required (or use --replace NAME --text DECL)")
    report = {"tu": str(src.relative_to(ROOT)), "before": a.before, "names": a.name,
              "replace": a.replace}
    with locked():
        original = src.read_text()
        try:
            new, moved = (replace_text(original, a.replace, a.text) if a.replace
                          else hoist_text(original, a.before, a.name))
        except (tu_edit.EditError, tu_edit.ParseError) as e:
            report.update(ok=False, error=str(e))
            print(json.dumps(report, indent=1))
            return 1
        report["moved"] = moved
        if not moved:
            report.update(ok=True, note="already declared above the slot")
            print(json.dumps(report, indent=1))
            return 0
        tmp = src.with_name(f".{src.name}.{os.getpid()}.tmp")
        tmp.write_text(new)
        os.replace(tmp, src)
        r = rebuild(ROOT, a.unit)
        ok = bool(r.get("identical")) and (a.unit != "main" or r.get("mapcheck_ok", True))
        if not ok:
            tmp.write_text(original)
            os.replace(tmp, src)
            rebuild(ROOT, a.unit)
            report.update(ok=False, error="unit not identical after the move (reverted)",
                          ninja_tail=r.get("ninja_tail", "")[-1500:])
            print(json.dumps(report, indent=1))
            return 1
    log(f"HOIST {a.unit}/{a.tu} {moved} " + (f"replaced: {a.text}" if a.replace else f"above {a.before}"))
    report.update(ok=True)
    print(json.dumps(report, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

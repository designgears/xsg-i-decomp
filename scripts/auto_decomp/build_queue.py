#!/usr/bin/env python3
"""Build / refresh the INCLUDE_ASM candidate queue (smallest first; callees that
unblock same-TU callers get a size boost, see callgraph.py)."""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
INC = re.compile(r'^\s*INCLUDE_ASM\(\s*"([^"]+)"\s*,\s*([^)\s]+)\s*\)\s*;', re.M)
ADDR = re.compile(r'/\*\s*([0-9A-Fa-f]+)\s+[0-9A-Fa-f]{8}')


def function_size(asm: Path) -> int:
    if not asm.is_file():
        return 10**9
    text = asm.read_text(errors="replace")
    addrs = ADDR.findall(text)
    if len(addrs) >= 2:
        return int(addrs[-1], 16) - int(addrs[0], 16) + 4
    if len(addrs) == 1:
        return 4
    return max(4, text.count("\n") * 4)


def annotate_callgraph(root: Path, candidates: list[dict]) -> None:
    """Callee-first scheduling (callgraph.py): blocked_by = same-TU INCLUDE_ASM
    callees above the function (it cannot match until they are C); unblocks =
    functions transitively waiting on it. A function that unblocks others sorts
    as if up to 4x smaller (1 + min(unblocks, 3)), so leaves come first within a
    TU without starving big functions. Best effort: no annotation on failure."""
    try:
        import callgraph
        fns = callgraph.build(root)
    except Exception:
        return
    for c in candidates:
        v = fns.get(f"{c['unit']}/{c['tu']}/{c['name']}")
        if v:
            if v["blocked_by"]:
                c["blocked_by"] = v["blocked_by"]
            if v["unblocks_all"]:
                c["unblocks"] = v["unblocks_all"]
                c["depth"] = v["depth"]


def build_queue(root: Path) -> dict:
    candidates = []
    for cpath in sorted((root / "src").rglob("*.c")):
        unit = cpath.parts[cpath.parts.index("src") + 1]
        text = cpath.read_text(errors="replace")
        for m in INC.finditer(text):
            folder, name = m.group(1), m.group(2)
            asm = root / "build" / unit / folder / f"{name}.s"
            if not asm.is_file():
                alt = root / "build" / unit / f"{folder}/{name}.s"
                asm = alt if alt.is_file() else asm
            candidates.append({
                "unit": unit,
                "tu": cpath.stem,
                "source": str(cpath.relative_to(root)),
                "folder": folder,
                "name": name,
                "asm": str(asm.relative_to(root)) if asm.is_file() else None,
                "size": function_size(asm),
            })
    annotate_callgraph(root, candidates)
    candidates.sort(key=lambda c: (c["size"] // (1 + min(len(c.get("unblocks", ())), 3)),
                                   c["size"], c["unit"], c["tu"], c["name"]))
    buckets = {
        "le_32": sum(1 for c in candidates if c["size"] <= 32),
        "le_64": sum(1 for c in candidates if c["size"] <= 64),
        "le_128": sum(1 for c in candidates if c["size"] <= 128),
        "le_256": sum(1 for c in candidates if c["size"] <= 256),
        "gt_256": sum(1 for c in candidates if c["size"] > 256),
    }
    return {"schema": "auto-decomp-queue/1", "total": len(candidates),
            "by_size_bucket": buckets, "candidates": candidates}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("-o", type=Path, default=None)
    a = ap.parse_args()
    q = build_queue(a.root)
    out = a.o or (a.root / "build/auto_decomp/queue.json")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(q, indent=1) + "\n")
    print(json.dumps({"total": q["total"], "buckets": q["by_size_bucket"], "out": str(out)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

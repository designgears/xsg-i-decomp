#!/usr/bin/env python3
"""Same-TU call graph of the INCLUDE_ASM functions, for callee-first scheduling.

gcc 2.96 here only marks a call nothrow when the callee is defined as C EARLIER
IN THE SAME TU; otherwise reorg fills delay slots differently (beqzl/bnel vs
plain branches). So a caller whose same-TU callee ABOVE it is still INCLUDE_ASM
cannot match yet: it is "blocked" and plateaus at a one-word delay-slot diff.
A callee BELOW its caller was invisible in the original too: never blocks
(listed as "below" in the report, needs no scheduling).

Edges come from `jal <sym>` in build/<unit>/asm/.../nonmatchings/<tu>/<fn>.s
(jalr, other-TU and extern targets ignored). Blocking edges always point to an
earlier file position, so the blocking graph is acyclic; depth/topo order still
guard against cycles.

    python3 -B scripts/auto_decomp/callgraph.py --report [--json]
"""
from __future__ import annotations

import argparse
import glob
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
INC = re.compile(r'^\s*INCLUDE_ASM\(\s*"([^"]+)"\s*,\s*([A-Za-z_]\w*)\s*\)', re.M)
DEF = re.compile(r"^[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{", re.M)
JAL = re.compile(r"^\s*/\*[^*]*\*/\s+jal\s+([A-Za-z_.$][\w.$]*)\s*$", re.M)
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)


def _asm_path(root: Path, unit: str, folder: str, name: str) -> Path:
    return root / "build" / unit / folder / f"{name}.s"


def _tu(root: Path, cpath: Path) -> dict | None:
    unit = cpath.parts[cpath.parts.index("src") + 1]
    raw = cpath.read_text(errors="replace")
    text = COMMENT.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), raw)   # keep offsets
    pos: dict[str, int] = {}
    asm: dict[str, Path] = {}
    for m in INC.finditer(text):
        pos.setdefault(m.group(2), m.start())
        asm[m.group(2)] = _asm_path(root, unit, m.group(1), m.group(2))
    if not asm:
        return None
    for m in DEF.finditer(text):
        if m.group(1) not in asm:
            pos.setdefault(m.group(1), m.start())
    calls: dict[str, list[str]] = {}
    for n, p in asm.items():
        try:
            targets = JAL.findall(p.read_text(errors="replace"))
        except OSError:
            targets = []
        seen = []
        for t in targets:
            if t != n and t in pos and t not in seen:
                seen.append(t)
        calls[n] = seen
    return {"unit": unit, "tu": cpath.stem, "source": str(cpath.relative_to(root)),
            "pos": pos, "asm": set(asm), "calls": calls}


def build(root: Path = ROOT) -> dict:
    """{ "unit/tu/name": info } for every INCLUDE_ASM function. info: blocked_by
    (asm callees above it), below (asm callees below it), unblocks (direct
    blocked callers), unblocks_all (transitive), depth (0 = no asm callee above)."""
    fns: dict[str, dict] = {}
    for cpath in sorted((root / "src").rglob("*.c")):
        t = _tu(root, cpath)
        if not t:
            continue
        pre = f"{t['unit']}/{t['tu']}/"
        for n in t["asm"]:
            above, below = [], []
            for c in t["calls"][n]:
                if c not in t["asm"]:
                    continue
                (above if t["pos"][c] < t["pos"][n] else below).append(pre + c)
            fns[pre + n] = {"unit": t["unit"], "tu": t["tu"], "name": n, "line_pos": t["pos"][n],
                            "blocked_by": above, "below": below, "unblocks": []}
    for k, v in fns.items():
        for c in v["blocked_by"]:
            fns[c]["unblocks"].append(k)
    for k, v in fns.items():                         # transitive closure over callers
        seen, stack = set(), list(v["unblocks"])
        while stack:
            x = stack.pop()
            if x not in seen:
                seen.add(x)
                stack.extend(fns[x]["unblocks"])
        seen.discard(k)
        v["unblocks_all"] = sorted(seen)
    memo: dict[str, int] = {}

    def depth(k: str, onstack: set) -> int:
        if k in memo:
            return memo[k]
        if k in onstack:                             # cycle: break it
            return 0
        onstack.add(k)
        d = 1 + max((depth(c, onstack) for c in fns[k]["blocked_by"]), default=-1)
        onstack.discard(k)
        memo[k] = d
        return d

    for k in fns:
        fns[k]["depth"] = depth(k, set())
    return fns


def topo_order(fns: dict, unit: str | None = None, tu: str | None = None) -> list[str]:
    """Leaf-first order: depth, then most transitive unblocks, then file position."""
    ks = [k for k, v in fns.items() if (unit is None or v["unit"] == unit) and (tu is None or v["tu"] == tu)]
    return sorted(ks, key=lambda k: (fns[k]["depth"], -len(fns[k]["unblocks_all"]),
                                     fns[k]["unit"], fns[k]["tu"], fns[k]["line_pos"]))


def _near_misses(root: Path) -> dict[str, float]:
    """key -> best known score (loop.log GIVEUP scores and bodies/*.best.json)."""
    best: dict[str, float] = {}
    log = root / "build/auto_decomp/loop.log"
    if log.is_file():
        for m in re.finditer(r"GIVEUP (\S+) score=([0-9.]+)", log.read_text(errors="replace")):
            best[m.group(1)] = max(best.get(m.group(1), 0.0), float(m.group(2)))
    for p in glob.glob(str(root / "build/auto_decomp/bodies/*/*/*.best.json")):
        pp = Path(p)
        try:
            s = float(json.loads(pp.read_text()).get("score") or 0)
        except Exception:
            continue
        k = f"{pp.parent.parent.name}/{pp.parent.name}/{pp.name[:-len('.best.json')]}"
        best[k] = max(best.get(k, 0.0), s)
    return best


def report(root: Path, as_json: bool, top: int) -> int:
    fns = build(root)
    blocked = {k: v for k, v in fns.items() if v["blocked_by"]}
    ranked = sorted((k for k, v in fns.items() if v["unblocks_all"]),
                    key=lambda k: (-len(fns[k]["unblocks_all"]), -len(fns[k]["unblocks"]), k))
    per_tu: dict[str, list[int]] = {}
    for k, v in fns.items():
        r = per_tu.setdefault(f"{v['unit']}/{v['tu']}", [0, 0, 0])
        r[0] += 1
        r[1] += bool(v["blocked_by"])
        r[2] += bool(v["unblocks"])
    scores = _near_misses(root)
    near = sorted(((k, scores[k]) for k in blocked if scores.get(k, 0) >= 0.85 and scores[k] < 1.0),
                  key=lambda x: -x[1])
    below = sorted((k, v["below"]) for k, v in fns.items() if v["below"])
    if as_json:
        print(json.dumps({"asm_functions": len(fns), "blocked": len(blocked),
                          "top_unblockers": [{"key": k, "unblocks_all": len(fns[k]["unblocks_all"]),
                                              "unblocks": fns[k]["unblocks"]} for k in ranked[:top]],
                          "near_miss_blocked": [{"key": k, "score": s, "blocked_by": fns[k]["blocked_by"]}
                                                for k, s in near],
                          "callee_below": len(below), "per_tu": per_tu}, indent=1))
        return 0
    print(f"INCLUDE_ASM functions: {len(fns)}; blocked by an asm same-TU callee above them: {len(blocked)}; "
          f"callees that block someone: {len(ranked)}; max depth: {max((v['depth'] for v in fns.values()), default=0)}")
    print(f"\nTop {top} callees by functions unblocked (transitive / direct):")
    for k in ranked[:top]:
        print(f"  {len(fns[k]['unblocks_all']):3d} / {len(fns[k]['unblocks']):2d}  {k}  "
              f"(depth {fns[k]['depth']})")
    print(f"\nGiven-up / near-miss (0.85 <= score < 1) functions blocked by an asm callee: {len(near)}")
    for k, s in near:
        print(f"  {s:.4f}  {k}  <- {', '.join(c.rsplit('/', 1)[1] for c in fns[k]['blocked_by'])}")
    print(f"\nAsm callee BELOW its asm caller (no scheduling needed): {len(below)} callers")
    for k, b in below[:top]:
        print(f"  {k} -> {', '.join(c.rsplit('/', 1)[1] for c in b)}")
    print("\nPer TU (asm / blocked / blocking), TUs with any blocked function:")
    for t, (n, b, u) in sorted(per_tu.items(), key=lambda x: (-x[1][1], x[0])):
        if b:
            print(f"  {n:4d} {b:4d} {u:4d}  {t}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--top", type=int, default=15)
    ap.add_argument("--key", metavar="UNIT/TU/NAME", help="show one function's entry")
    ap.add_argument("--order", metavar="UNIT/TU", help="leaf-first order of one TU's asm functions")
    a = ap.parse_args()
    if a.key or a.order:
        fns = build(a.root)
        if a.key:
            print(json.dumps(fns.get(a.key), indent=1))
        if a.order:
            u, t = a.order.split("/", 1)
            for k in topo_order(fns, u, t):
                print(fns[k]["depth"], len(fns[k]["unblocks_all"]), k, fns[k]["blocked_by"])
        return 0
    return report(a.root, a.json, a.top)


if __name__ == "__main__":
    sys.exit(main())

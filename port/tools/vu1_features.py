#!/usr/bin/env python3
"""Static feature scan of the VU1 programs of one .vudata microcode set.

  vu1_features.py --set N --entries 0x5b0,0x8e0,...

A program is the code from its entry to the next entry (the MSCAL prologue
plus the vertex loop it returns to after the shared batch header).  Prints
per program: size, subroutine calls (jalr targets), and which building blocks
it uses -- bone loop, lighting, facing test, specular power loop, second
GIF tag (two-pass), EFU ops, XGKICKs.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import vu_disasm as D  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--set", type=int, default=0)
    ap.add_argument("--entries", required=True)
    a = ap.parse_args()
    _, micro, size, _ = D.blob_sets(D.BLOB)[a.set]
    entries = sorted(int(x, 0) for x in a.entries.split(","))
    for i, e in enumerate(entries):
        end = entries[i + 1] if i + 1 < len(entries) else size
        text = []
        calls = []
        last_imm = None
        for pc in range(e, end, 8):
            u, _, l, k, _, _ = D.decode(micro, pc)
            text.append(u + " | " + l)
            if l.startswith("iaddiu vi14, vi0, 0x"):
                last_imm = int(l.split("0x")[1], 16) * 8
            if k == "jalr" and last_imm is not None:
                calls.append(last_imm)
        t = "\n".join(text)
        feats = []
        if "mtir vi1, vf3x" in t and "mr32.xyzw vf5, vf5" in t or "mr32.xyzw vf4, vf4" in t:
            feats.append("skin(4 bones)")
        if "vf17, vf16x" in t or "vf17, vf11x" in t or "acc, vf17" in t:
            feats.append("light")
        if "1012(vi0)" in t:
            feats.append("2nd GIF tag (two-pass)")
        if "mul.xyz vf5, vf5, vf5" in t:
            feats.append("specular power loop")
        if "fsand" in t:
            feats.append("facing(fsand)")
        if 0x440 in calls:
            feats.append("tri-facing 0x440")
        if "eexp" in t or "esin" in t or "eatan" in t:
            feats.append("EFU trig/exp")
        if "erleng" in t:
            feats.append("normalize")
        if "1007(vi0)" in t:
            feats.append("fog(1007)")
        if "998(vi0)" in t:
            feats.append("fog word(998)")
        if "itof" in t:
            feats.append("itof")
        print(f"0x{e:04x}-0x{end:04x} {len(text):4d} pairs  calls {[hex(c) for c in calls]}  "
              f"xgkick {t.count('xgkick')}  {', '.join(feats)}")


if __name__ == "__main__":
    main()

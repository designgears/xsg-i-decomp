#!/usr/bin/env python3
"""Summarise a [debug] prof sample file (port/runtime/prof.c).

  prof_report.py port/build/xsg prof.txt [-n 40]

Prints self time (innermost frame) and inclusive time (function anywhere in
the sampled stack) per function, symbolised with `nm -n` of the binary.
"""
import argparse
import bisect
import collections
import subprocess


def load_syms(binary):
    addrs, names = [], []
    for line in subprocess.run(["nm", "-n", "-C", binary], capture_output=True,
                               text=True).stdout.splitlines():
        p = line.split(None, 2)
        if len(p) == 3 and p[1] in "TtWw":
            addrs.append(int(p[0], 16))
            names.append(p[2])
    return addrs, names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binary")
    ap.add_argument("samples")
    ap.add_argument("-n", type=int, default=40)
    a = ap.parse_args()
    addrs, names = load_syms(a.binary)

    def sym(x):
        i = bisect.bisect_right(addrs, x) - 1
        return names[i] if i >= 0 and x - addrs[i] < 0x100000 else "?%x" % x

    self_c, incl = collections.Counter(), collections.Counter()
    total = 0
    for line in open(a.samples):
        fr = [int(t, 16) for t in line.split()]
        if not fr:
            continue
        total += 1
        stack = [sym(fr[0])] + [sym(x - 1) for x in fr[1:]]
        self_c[stack[0]] += 1
        for s in set(stack):
            incl[s] += 1
    print("%d samples" % total)
    print("\n-- self --")
    for s, c in self_c.most_common(a.n):
        print("%6.2f%%  %s" % (100.0 * c / total, s))
    print("\n-- inclusive --")
    for s, c in incl.most_common(a.n):
        print("%6.2f%%  %s" % (100.0 * c / total, s))


if __name__ == "__main__":
    main()

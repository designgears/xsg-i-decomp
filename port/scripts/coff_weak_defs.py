#!/usr/bin/env python3
"""Weak data definitions for COFF (Windows build, port/docs/WINDOWS.md).

The ELF build marks generated data labels `.weak` so that a definition the C
makes itself wins.  In COFF a weak definition is a weak external whose
auxiliary record points at its default symbol by index, and `ld -r` does not
renumber that index: after the partial link the symbol resolves to whatever
now sits at the old index.  So the Windows build decides at build time: a
label the C objects define becomes a local label (references then bind to
the C definition, as the weak one would have lost), any other becomes a plain
global.

  coff_weak_defs.py NM IN.s OUT.s OBJ...     (names compared verbatim)
"""
import re
import subprocess
import sys


def defined_names(nm, objs):
    out = set()
    if not objs:
        return out
    for line in subprocess.run([nm] + objs, capture_output=True, text=True, check=True).stdout.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in "TDBRC":
            out.add(parts[2])
    return out


def rewrite(text, c_defined):
    weak = set(re.findall(r"^\s*\.weak\s+(\S+)\s*$", text, re.M))
    local = {n: ".Lxsg_cdef_%d" % i for i, n in enumerate(sorted(weak & c_defined))}
    out = []
    for line in text.split("\n"):
        m = re.match(r"^(\s*)\.weak\s+(\S+)\s*$", line)
        if m:
            if m.group(2) not in local:
                out.append("%s.globl %s" % (m.group(1), m.group(2)))
            continue
        m = re.match(r"^(\s*)(\S+):\s*$", line)
        if m and m.group(2) in local:
            out.append("%s%s:" % (m.group(1), local[m.group(2)]))
            continue
        out.append(line)
    return "\n".join(out)


if __name__ == "__main__":
    nm, src, dst = sys.argv[1:4]
    text = open(src, encoding="latin-1").read()
    open(dst, "w", encoding="latin-1").write(rewrite(text, defined_names(nm, sys.argv[4:])))

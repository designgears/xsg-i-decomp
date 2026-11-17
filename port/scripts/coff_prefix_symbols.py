#!/usr/bin/env python3
"""objcopy --prefix-symbols for COFF objects (Windows build, port/docs/WINDOWS.md).

GNU objcopy's --prefix-symbols also renames COFF section symbols (.text,
.bss...), which relocations against static data refer to; the result links
to wrong addresses.  This prefixes every other symbol through --redefine-syms.

  coff_prefix_symbols.py NM OBJCOPY PREFIX IN OUT
"""
import subprocess
import sys

nm, objcopy, prefix, src, dst = sys.argv[1:6]
names = set()
for line in subprocess.run([nm, src], capture_output=True, text=True, check=True).stdout.splitlines():
    parts = line.split()
    if parts and not parts[-1].startswith((".", "@")):
        names.add(parts[-1])
lst = dst + ".map"
with open(lst, "w") as f:
    f.write("".join("%s %s%s\n" % (n, prefix, n) for n in sorted(names)))
subprocess.run([objcopy, "--redefine-syms=" + lst, src, dst], check=True)

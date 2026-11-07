#!/usr/bin/env python3
"""Stage 10: find game functions defined `void` whose result some caller uses.

On the PS2 such a caller reads whatever the callee left in v0 (typically a
tail-called function's result). On the host %eax is whatever the compiler left
there, which changes with -O level and -fstack-protector. Prints candidates:

  scan_void_results.py [src-dir]
"""
import glob
import os
import re
import sys

root = sys.argv[1] if len(sys.argv) > 1 else "src"
files = sorted(glob.glob(os.path.join(root, "**/*.[ch]"), recursive=True))
text = {f: open(f, errors="replace").read() for f in files}

defn = re.compile(r"^void\s+(\w+)\s*\([^;{]*\)\s*\{", re.M)
voids = {}
for f, s in text.items():
    if f.endswith(".c"):
        for m in defn.finditer(s):
            voids.setdefault(m.group(1), []).append(f)

for name, defs in sorted(voids.items()):
    hits = []
    decl = re.compile(r"^[^\n;(){}]*?\b(?!void\b)(\w+)\s*\**\s*\b%s\s*\(" % re.escape(name), re.M)
    use = re.compile(r"(=|return|\()\s*(\([^()]*\)\s*)?%s\s*\(" % re.escape(name))
    for f, s in text.items():
        if name not in s:
            continue
        for m in decl.finditer(s):
            line = m.group(0)
            if re.match(r"\s*(void|static void|extern void)\b", line) or line.strip().startswith(("#", "/*", "*")):
                continue
            if any(k in line for k in ("return", "=", "if", "while")):
                continue
            hits.append("%s: decl `%s`" % (f, line.strip()))
        for m in use.finditer(s):
            ln = s.count("\n", 0, m.start()) + 1
            hits.append("%s:%d: use `%s`" % (f, ln, s[m.start():m.end() + 20].split("\n")[0]))
    if hits:
        print("%s (void in %s)" % (name, ", ".join(defs)))
        for h in hits[:6]:
            print("    " + h)

#!/usr/bin/env python3
"""Stage 10: list game C files that give one symbol two C names.

    extern FontStateViewStorage FS_view __asm__("FS");   /* + extern char FS[]; */

GCC treats distinct declarations as distinct objects, even with
-fno-strict-aliasing, so at -O1 and above it reorders or forwards loads and
stores between FS and FS_view (e.g. xglFontFlushCore spun forever on a queue
record it had not seen written). Files that declare an *object* with an
`__asm__("name")` label are built at -O0 by port/CMakeLists.txt; everything else
is optimised. Function aliases (`int f(void) __asm__("g")`) are harmless and
are not counted.

  find_alias_views.py <src-root> -> one path per line (CMake list on stdout with --cmake)
"""
import os
import re
import sys

# `__asm__("sym")` closing a declaration whose declarator is not a function
# declarator, i.e. the text between the last top-level ')' ... is not `(...)`.
LABEL = re.compile(r'__asm__\s*\(\s*"([A-Za-z_.$][\w.$]*)"\s*\)')


def object_labels(text):
    out = []
    for m in LABEL.finditer(text):
        # walk back to the start of this declaration
        start = max(text.rfind(";", 0, m.start()), text.rfind("{", 0, m.start()),
                    text.rfind("}", 0, m.start())) + 1
        decl = text[start:m.start()].strip()
        if not decl or decl.startswith("#") or "asm" in decl.split()[0]:
            continue
        # a function declarator ends with a parameter list; a function pointer
        # object `(*p)(...)` is an object, though
        if decl.endswith(")") and not re.search(r"\(\s*\*\s*\w+\s*\)\s*\(.*\)$", decl, re.S):
            continue
        if re.search(r"\bxsg_sym_\w+\s*\[\s*\]$", decl):
            continue  # address-only handle (`extern char xsg_sym_X[]`), never accessed
        out.append(m.group(1))
    return out


def link_aliases(repo):
    """name -> base symbol for the PS2 link's symbolic aliases (`X = Y + 8;`),
    which link_xsg.py turns into --defsym: two C names for one storage."""
    al = {}
    for d, _, files in os.walk(os.path.join(repo, "build")):
        for f in files:
            if f in ("undefined_syms_auto.txt", "aliases.ld", "externals.ld"):
                for line in open(os.path.join(d, f), errors="replace"):
                    line = re.sub(r"/\*.*?\*/", "", line).strip()
                    m = re.match(r'(?:PROVIDE\()?"?([\w.]+)"?\s*=\s*([A-Za-z_][\w.]*)', line)
                    if m and m.group(1) != m.group(2):
                        al[m.group(1)] = m.group(2)
    return al


def main():
    root = sys.argv[1]
    al = link_aliases(os.path.dirname(os.path.abspath(root)))
    words = re.compile(r"\b[A-Za-z_]\w*\b")
    hits = []
    for d, _, files in os.walk(root):
        for f in files:
            if f.endswith(".c"):
                p = os.path.join(d, f)
                with open(p, errors="replace") as fh:
                    t = fh.read()
                if object_labels(t):
                    hits.append(p)
                    continue
                w = set(words.findall(t))
                if any(n in w and b in w for n, b in al.items()):
                    hits.append(p)
    hits.sort()
    if "--cmake" in sys.argv:
        sys.stdout.write(";".join(hits))
    else:
        print("\n".join(hits))


if __name__ == "__main__":
    main()

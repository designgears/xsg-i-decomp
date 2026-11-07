#!/usr/bin/env python3
"""Stage 10: fail if a game C file gives one symbol two C names on the host.

    extern FontStateViewStorage FS_view __asm__("FS");   /* + extern char FS[]; */

GCC treats distinct declarations as distinct objects, even with
-fno-strict-aliasing, so at -O1 and above it reorders or forwards loads and
stores between FS and FS_view (xglFontFlushCore spun forever on a queue record
it had not seen written; STAGE10_PERF.md cause 4).  The PORT_HOST side of every
such view renames the declaration to `xsg_view_<name>` and accesses it only
through XSG_VIEW() (port/include/ps2_host_compat.h), a laundered pointer that
GCC must assume aliases every global.

The check looks at the host side of each file (PS2-only `#else` branches of
`#ifdef PORT_HOST` and `#ifndef PORT_HOST` blocks are dropped) and reports
  * an `extern` object declared with an `__asm__("sym")` label that is not an
    `xsg_view_*` / `xsg_sym_*` handle;
  * a non-extern object with an asm label whose symbol has another C name in
    the same file;
  * an `xsg_view_*` name used anywhere but its declaration and XSG_VIEW();
  * a PS2 link alias (`UmnDataBaseStateData = SaveData + 66140`, from
    build/*/aliases.ld etc.) used as an identifier next to its base symbol.
Function aliases (`int f(void) __asm__("g")`) name code, not storage, and are
not counted: GCC never caches anything across them.

  find_alias_views.py <src-root>   -> violations on stdout, exit 1 if any
"""
import os
import re
import sys

LABEL = re.compile(r'__asm__\s*\(\s*"([^"]+)"\s*\)')
WORD = re.compile(r"\b[A-Za-z_]\w*\b")


def strip_comments(t):
    t = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), t, flags=re.S)
    t = re.sub(r"//[^\n]*", "", t)
    return re.sub(r'"(?:\\.|[^"\\\n])*"', lambda m: m.group(0) if m.start() and
                  t[max(0, m.start() - 12):m.start()].rstrip().endswith("(") else '""', t)


def host_side(text):
    """Drop the PS2 branch of PORT_HOST conditionals (line based)."""
    out, stack = [], []  # stack entries: (kind, keep_current)
    for line in text.split("\n"):
        s = line.strip()
        m = re.match(r"#\s*(ifdef|ifndef|if|else|elif|endif)\b(.*)", s)
        keep = all(k for _, k in stack)
        if m:
            d, rest = m.group(1), m.group(2).strip()
            if d in ("ifdef", "ifndef", "if"):
                if d == "ifdef" and rest == "PORT_HOST":
                    stack.append(("host", True))
                elif d == "ifndef" and rest == "PORT_HOST":
                    stack.append(("ps2", False))
                else:
                    stack.append(("other", True))
            elif d == "else" and stack:
                kind, k = stack[-1]
                if kind in ("host", "ps2"):
                    stack[-1] = (kind, not k)
            elif d == "endif" and stack:
                stack.pop()
            out.append("")
            continue
        if s.startswith("#"):
            out.append(line if keep else "")
            continue
        out.append(line if keep else "")
    return "\n".join(out)


def decl_start(text, end):
    """Start of the declaration ending at `end`, stepping over `struct {...}` bodies."""
    i = end
    while True:
        j = max(text.rfind(";", 0, i), text.rfind("{", 0, i), text.rfind("}", 0, i))
        if j < 0 or text[j] != "}":
            return j + 1
        # a '}' followed by a declarator: is it a type body?  skip to its '{'
        depth, k = 0, j
        while k >= 0:
            if text[k] == "}":
                depth += 1
            elif text[k] == "{":
                depth -= 1
                if depth == 0:
                    break
            k -= 1
        head = text[max(text.rfind(";", 0, k), text.rfind("}", 0, k), text.rfind("{", 0, k)) + 1:k]
        if re.search(r"\b(struct|union|enum)\b[\w\s]*$", head):
            i = k
            continue
        return j + 1


def labelled_decls(text):
    """(c_name, symbol, is_extern, is_function, lineno) for each asm-labelled declaration."""
    out = []
    # blank directive lines (same length) so a `#define` never joins a declaration
    text = re.sub(r"^[ \t]*#[^\n]*", lambda m: " " * len(m.group(0)), text, flags=re.M)
    for m in LABEL.finditer(text):
        start = decl_start(text, m.start())
        decl = text[start:m.start()].strip()
        if not decl or "asm" in decl.split()[0] or decl.startswith("#"):
            continue
        tail = text[m.end():m.end() + 40].lstrip()
        if not (tail.startswith(";") or tail.startswith("=") or tail.startswith(",")):
            continue  # an asm statement operand, not a declaration
        d = decl
        while True:
            b = re.search(r"\[[^\[\]]*\]\s*$", d)
            if not b:
                break
            d = d[:b.start()].rstrip()
        fptr = re.search(r"\(\s*\*\s*(\w+)\s*\)\s*\(.*\)$", d, re.S)
        if fptr:
            name, func = fptr.group(1), False
        elif d.endswith(")"):
            fn = re.search(r"(\w+)\s*\(", d)
            name, func = (fn.group(1) if fn else "?"), True
            # rightmost `name (` that opens the parameter list
            depth, i = 0, len(d) - 1
            while i >= 0:
                if d[i] == ")":
                    depth += 1
                elif d[i] == "(":
                    depth -= 1
                    if depth == 0:
                        w = re.search(r"(\w+)\s*$", d[:i])
                        name = w.group(1) if w else name
                        break
                i -= 1
        else:
            w = re.search(r"(\w+)$", d)
            if not w:
                continue
            name, func = w.group(1), False
        out.append((name, m.group(1), decl.split()[0] == "extern" or " extern " in decl,
                    func, text.count("\n", 0, m.start()) + 1))
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


def host_defines(roots):
    """Names #defined on the host side of any header (link aliases rewritten there)."""
    out = set()
    for r in roots:
        for d, _, files in os.walk(r):
            for f in files:
                if f.endswith(".h"):
                    t = host_side(strip_comments(open(os.path.join(d, f), errors="replace").read()))
                    out |= set(re.findall(r"^\s*#\s*define\s+(\w+)", t, re.M))
    return out


def check_file(path, al, hdr_defined=frozenset()):
    raw = open(path, errors="replace").read()
    t = host_side(strip_comments(raw))
    bad = []
    decls = labelled_decls(t)
    names_by_sym = {}
    for name, sym, ext, func, ln in decls:
        if not func:
            names_by_sym.setdefault(sym, set()).add(name)
    words = WORD.findall(t)
    wset = set(words)
    defined = set(re.findall(r"^\s*#\s*define\s+(\w+)", t, re.M)) | hdr_defined
    for name, sym, ext, func, ln in decls:
        if func or name.startswith("xsg_sym_") or name.startswith("xsg_view_"):
            continue
        if ext:
            bad.append("%s:%d: %s is a second C name for %s" % (path, ln, name, sym))
        else:
            others = names_by_sym[sym] - {name}
            if sym in wset:
                others.add(sym)
            if others:
                bad.append("%s:%d: %s defines %s, also named %s" % (path, ln, name, sym,
                                                                     ", ".join(sorted(others))))
    # xsg_view_X only in its declaration(s) and XSG_VIEW(xsg_view_X)
    for v in sorted({w for w in wset if w.startswith("xsg_view_")}):
        total = words.count(v)
        ndecl = sum(1 for n, *_ in decls if n == v)
        nview = len(re.findall(r"XSG_VIEW\(\s*%s\s*\)" % v, t))
        if total != ndecl + nview:
            bad.append("%s: %s is accessed outside XSG_VIEW()" % (path, v))
    for n, b in al.items():
        if n in wset and b in wset and n not in defined:
            bad.append("%s: link alias %s = %s used next to its base" % (path, n, b))
    return bad


def main():
    root = sys.argv[1]
    al = link_aliases(os.path.dirname(os.path.abspath(root)))
    repo = os.path.dirname(os.path.abspath(root))
    hdr = host_defines([root, os.path.join(repo, "include")])
    bad = []
    for d, _, files in os.walk(root):
        for f in sorted(files):
            if f.endswith(".c") and "/nml_packet_add/" not in os.path.join(d, f) + "/":
                bad += check_file(os.path.join(d, f), al, hdr)
    bad.sort()
    print("\n".join(bad))
    if bad:
        print("find_alias_views: %d unguarded alias view(s); see the header of this script"
              % len(bad), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

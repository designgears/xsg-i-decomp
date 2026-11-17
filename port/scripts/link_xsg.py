#!/usr/bin/env python3
"""Link the host executable port/build/xsg.

Per unit (main and each overlay):
  1. assemble its converted data (gen_host_data.py output) with `as --32`;
  2. `ld -r` its C objects + data into one relocatable <unit>.r.o;
  3. globalize file-static functions/data that the unit's data tables
     reference by name (scERRORScript, _BallVsBall...; LOCAL in the original
     symbol table, `static` in the C) when exactly one definition exists;
  4. overlays: add a table {PS2 address, host symbol} for every function in
     config/symbols/<unit>.txt that the unit defines (xsg_ovtab_<unit>), and
     localize every global that main or another overlay also defines, so each
     overlay binds to its own copy as it did when it alone sat at 0xA00000.
Then:
  5. link with the runtime; for what is still undefined, generate
     - `--defsym` from the PS2 alias/undefined-symbol scripts (numeric
       PS2 addresses, `X = Y + off` aliases),
     - trampolines `func_<addr>` -> xsg_ps2_func(addr) for overlay addresses,
     - logging stubs (functions) / zero blocks (data) for the rest,
       listed in autostubs.txt;
  6. relink.

  link_xsg.py --build port/build --out port/build/xsg --runtime a.o b.o --libs=-lm

Windows (--target pe, port/docs/WINDOWS.md): the same steps with the MinGW
binutils (--tool-prefix i686-w64-mingw32-) on COFF objects.  C symbols carry a
leading underscore there, so names read from nm lose it and names written to
symbol lists, --defsym and generated assembly get it back (sym()); the data
assembly's ELF-only directives are rewritten (coff_source()).  The output is
a DLL (xsg.dll, linked at 0x40000000) that the stub xsg.exe loads.
"""
import argparse
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

UNITS = ["main", "ov01", "ov02", "ov10", "ov11", "ov12"]
OVERLAYS = UNITS[1:]
OV_IDS = {"ov01": 1, "ov02": 2, "ov10": 10, "ov11": 11, "ov12": 12}

# Target object format (set in main()): "elf" (Linux) or "pe" (Windows/MinGW).
PE = False
TOOL = {"as": "as", "ld": "ld", "nm": "nm", "objcopy": "objcopy"}


def sym(name):
    """C name -> object-file symbol name (i386 COFF prefixes an underscore)."""
    return "_" + name if PE else name


COFF_SEC = re.compile(r'^(\s*\.section\s+[\w.$]+)\s*,\s*"aw"\s*,\s*@(progbits|nobits)\s*$')
COFF_SYMLINE = re.compile(r'^(\s*\.(?:long|short|global|globl|weak)\s+)(.*)$')
COFF_IDENT = re.compile(r'(?<![\w.$])([A-Za-z_][\w$]*(?:\.[\w$]+)*)')


def coff_source(text):
    """gen_host_data.py's ELF-flavoured i386 assembly -> COFF: no .type/.size,
    section flags in COFF form, every symbol name with its leading underscore
    (.L locals and numbers untouched; strings are never on the rewritten lines)."""
    out = []
    for line in text.split("\n"):
        st = line.strip()
        if st.startswith(".type") or st.startswith(".size"):
            continue
        m = COFF_SEC.match(line)
        if m:
            out.append('%s,"%s"' % (m.group(1), "bw" if m.group(2) == "nobits" else "dw"))
            continue
        m = COFF_SYMLINE.match(line)
        if m:
            out.append(m.group(1) + COFF_IDENT.sub(lambda k: "_" + k.group(1), m.group(2)))
            continue
        if st.endswith(":") and not st.startswith(".") and '"' not in st:
            out.append(line.replace(st[:-1], "_" + st[:-1], 1))
            continue
        out.append(line)
    return "\n".join(out)


def run(cmd, check=True):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if check and r.returncode:
        sys.stderr.write(" ".join(cmd)[:2000] + "\n" + r.stdout + r.stderr)
        sys.exit(1)
    return r


def asm_labels(root):
    """Names the C gives through `__asm__("name")` (the alias views).  GCC emits
    them verbatim, so on COFF they lack the underscore every other C name has."""
    names = set()
    rx = re.compile(r'__asm__\s*\(\s*"([A-Za-z_][\w.$]*)"\s*\)')
    for top in ("src", "include"):
        for dp, _, fs in os.walk(os.path.join(root, top)):
            for f in fs:
                if f.endswith((".c", ".h")):
                    names.update(rx.findall(open(os.path.join(dp, f), encoding="latin-1").read()))
    return names


def coff_fix_asm_labels(obj, labels, work):
    """Give the verbatim __asm__ names in obj their COFF underscore."""
    raw = set()
    for line in run([TOOL["nm"], obj]).stdout.splitlines():
        parts = line.split()
        if not parts:
            continue
        n = parts[-1]
        # every C-level name is _-prefixed on COFF, so a bare identifier is an
        # __asm__ name; _-led ones are told apart through the sources' list
        if n in labels or re.fullmatch(r"[A-Za-z][\w$]*", n):
            raw.add(n)
    if raw:
        lst = obj + ".asmlabels"
        open(lst, "w").write("".join("%s _%s\n" % (n, n) for n in sorted(raw)))
        run([TOOL["objcopy"], "--redefine-syms=" + lst, obj])


def nm(obj):
    """-> (global_defined, local_counts, undefined), by C name"""
    g, loc, u = set(), {}, set()
    for line in run([TOOL["nm"], obj]).stdout.splitlines():
        parts = line.split()
        if PE and parts:
            if not parts[-1].startswith("_"):
                continue   # section/file symbols (.text, .data$x, @feat.00), not C names
            parts[-1] = parts[-1][1:]
        if len(parts) == 2 and parts[0] in ("U", "w"):
            u.add(parts[1])
        elif len(parts) == 3:
            t, name = parts[1], parts[2]
            if t in "TDBRVW" or t == "A":
                g.add(name)
            elif t in "tdbr":
                loc[name] = loc.get(name, 0) + 1
    return g, loc, u


def stale(dst, srcs):
    if not os.path.exists(dst):
        return True
    t = os.path.getmtime(dst)
    return any(os.path.getmtime(s) > t for s in srcs)


def read_symbol_txt(path):
    out = []
    for line in open(path):
        m = re.match(r"\s*(\S+)\s*=\s*0x([0-9A-Fa-f]+);.*type:func", line)
        if m:
            out.append((int(m.group(2), 16), m.group(1)))
    return out


def read_data_symbols(path):
    """(addr, size, name) of every sized non-function symbol, by address"""
    out = []
    for line in open(path):
        m = re.match(r"\s*(\S+)\s*=\s*0x([0-9A-Fa-f]+);(.*)", line)
        if not m or "type:func" in m.group(3):
            continue
        z = re.search(r"size:0x([0-9A-Fa-f]+)", m.group(3))
        if z:
            out.append((int(m.group(2), 16), int(z.group(1), 16), m.group(1)))
    return sorted(out)


def containing_symbol(table, defined, addr):
    """the host-defined symbol whose PS2 extent holds addr, as (name, offset)"""
    best = None
    for a, n, name in table:
        if a > addr:
            break
        if addr < a + n and name in defined:
            best = (name, addr - a)
    return best


def read_alias_scripts(syms_dir):
    """name -> expression from the PS2 link's alias/undefined-symbol scripts
    (copied to port/data/syms/<unit> by gen_host_data.py --skeleton)."""
    al = {}
    for u in UNITS:
        d = os.path.join(syms_dir, u)
        for f in ("undefined_syms_auto.txt", "undefined_funcs_auto.txt", "aliases.ld",
                  "externals.ld"):
            p = os.path.join(d, f)
            if not os.path.exists(p):
                continue
            for line in open(p):
                line = re.sub(r"/\*.*?\*/", "", line).strip()
                m = re.match(r'(?:PROVIDE\()?"?([\w.]+)"?\s*=\s*([^;)]+)\)?;', line)
                if m:
                    al.setdefault(m.group(1), (m.group(2).strip(), u))
    return al


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--cc", default="cc")
    ap.add_argument("--runtime", nargs="*", default=[])
    ap.add_argument("--libs", action="append", default=[])
    ap.add_argument("--target", choices=["elf", "pe"], default="elf")
    ap.add_argument("--data", default=None, help="converted data, <dir>/<unit>/*.s (default <build>/data)")
    ap.add_argument("--syms", default=None, help="symbol scripts, <dir>/<unit>/ (default port/data/syms)")
    ap.add_argument("--tool-prefix", default="", help="binutils prefix, e.g. i686-w64-mingw32-")
    ap.add_argument("--image-base", default="0x40000000", help="pe: the DLL's (fixed) base address")
    a = ap.parse_args()
    global PE
    PE = a.target == "pe"
    for t in TOOL:
        TOOL[t] = a.tool_prefix + t
    obj_ext = (".o", ".obj") if PE else (".o",)
    b = os.path.abspath(a.build)
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
    data_dir = os.path.abspath(a.data) if a.data else os.path.join(b, "data")
    syms_dir = os.path.abspath(a.syms) if a.syms else os.path.join(root, "port", "data", "syms")
    work = os.path.join(b, "link")
    os.makedirs(work, exist_ok=True)

    c_objs = {}
    for u in UNITS:
        objs = []
        for dp, _, fs in os.walk(os.path.join(b, "CMakeFiles", "xsg_%s.dir" % u)):
            objs += [os.path.join(dp, f) for f in fs if f.endswith(obj_ext)]
        c_objs[u] = sorted(objs)

    # 1. data
    jobs = []
    data_objs = {u: [] for u in UNITS}
    for u in UNITS:
        d = os.path.join(data_dir, u)
        c_defined = None
        if PE:   # the C's own definitions decide the weak labels (coff_weak_defs.py)
            sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
            import coff_weak_defs
            c_defined = {n[1:] for n in coff_weak_defs.defined_names(TOOL["nm"], c_objs[u]) if n.startswith("_")}
        for f in sorted(os.listdir(d)):
            if f.endswith(".s"):
                src = os.path.join(d, f)
                obj = os.path.join(work, u, f[:-2] + ".o")
                data_objs[u].append(obj)
                if PE:
                    os.makedirs(os.path.dirname(obj), exist_ok=True)
                    coff = obj[:-2] + ".coff.s"
                    body = coff_source(coff_weak_defs.rewrite(open(src, encoding="latin-1").read(), c_defined))
                    if not os.path.exists(coff) or open(coff).read() != body:
                        open(coff, "w").write(body)
                    src = coff
                if stale(obj, [src]):
                    os.makedirs(os.path.dirname(obj), exist_ok=True)
                    jobs.append([TOOL["as"], "--32", "-o", obj, src])
    with ThreadPoolExecutor(os.cpu_count()) as ex:
        list(ex.map(run, jobs))

    # 2./3. partial link + globalize referenced statics
    unit_obj = {}
    gl = {}
    report = []
    labels = asm_labels(root) if PE else set()
    for u in UNITS:
        p = os.path.join(work, u + ".r.o")
        run([TOOL["ld"]] + ([] if PE else ["-m", "elf_i386"]) + ["-r", "-o", p] + c_objs[u] + data_objs[u])
        if PE:
            coff_fix_asm_labels(p, labels, work)
        g, loc, und = nm(p)
        want = set(und)
        tab = []
        if True:  # main too: the title returns main entry points (0x01245C58)
            for addr, name in read_symbol_txt(os.path.join(root, "config/symbols/%s.txt" % u)):
                if name in g or loc.get(name) == 1:
                    tab.append((addr, name))
                    want.add(name)
        glob = sorted(n for n in want if loc.get(n) == 1 and n not in g)
        dup = sorted(n for n in und if loc.get(n, 0) > 1)
        report += ["%s globalized %s" % (u, n) for n in glob]
        report += ["%s AMBIGUOUS static %s (%d copies)" % (u, n, loc[n]) for n in dup]
        if u == "main":
            rename = ["--redefine-sym", "%s=%s" % (sym("main"), sym("xsg_game_main"))]
        else:
            # The overlay's compiled-C statics get their own sections too, so
            # the runtime can restore them on activation as the PS2 did by
            # reloading the overlay image (__start_/__stop_xsgcd_<ov>, ...).
            rename = ["--rename-section", ".data=xsgcd_%s,alloc,load,data,contents" % u,
                      "--rename-section", ".bss=xsgcb_%s,alloc" % u]
        q = os.path.join(work, u + ".g.o")
        args = ["objcopy"] + rename
        if glob:
            lst = os.path.join(work, u + ".globalize")
            open(lst, "w").write("\n".join(sym(n) for n in glob) + "\n")
            args.append("--globalize-symbols=" + lst)
        run([TOOL["objcopy"]] + args[1:] + [p, q])
        if tab:
            s = os.path.join(work, u + "_ovtab.s")
            with open(s, "w") as fh:
                fh.write("\t.section %s\n\t.globl %s\n\t.balign 4\n%s:\n" % (
                    '.rdata,"dr"' if PE else ".rodata", sym("xsg_ovtab_" + u), sym("xsg_ovtab_" + u)))
                for addr, name in sorted(tab):
                    # (PS2 main -> the host's main(), as an ELF link resolves it; on
                    # Windows that is xsg_host_main, called from the DLL's entry)
                    if PE and name == "main":
                        name = "xsg_host_main"
                    fh.write("\t.long 0x%08x, %s\n" % (addr, sym(name)))
                fh.write("\t.long 0, 0\n")
            so = s[:-2] + ".o"
            run([TOOL["as"], "--32", "-o", so, s])
            q2 = os.path.join(work, u + ".t.o")
            run([TOOL["ld"]] + ([] if PE else ["-m", "elf_i386"]) + ["-r", "-o", q2, q, so])
            q = q2
        unit_obj[u] = q
        gl[u] = nm(q)[0]

    # 4. localize overlay clashes
    clash_log = []
    final = {"main": unit_obj["main"]}
    for u in OVERLAYS:
        others = set(gl["main"])
        for v in OVERLAYS:
            if v != u:
                others |= gl[v]
        keep = {"xsg_ovtab_" + u}
        clash = sorted((gl[u] & others) - keep)
        clash_log += ["%s %s%s" % (u, s, " (main)" if s in gl["main"] else "") for s in clash]
        f = os.path.join(work, u + ".scoped.o")
        lst = os.path.join(work, u + ".localize")
        open(lst, "w").write("\n".join(sym(n) for n in clash) + "\n")
        run([TOOL["objcopy"], "--localize-symbols=" + lst, unit_obj[u], f])
        final[u] = f
    open(os.path.join(b, "overlay_clashes.txt"), "w").write("\n".join(clash_log) + "\n")
    open(os.path.join(b, "globalized.txt"), "w").write("\n".join(report) + "\n")

    # 5./6. link, resolve leftovers, relink
    objs = a.runtime + [final[u] for u in UNITS]

    def link(extra):
        if PE:
            # xsg.dll: fixed base, no relocations (absolute --defsym addresses must
            # never be rebased), only xsg_win32_start exported (dllexport).
            cmd = ["-m32", "-shared", "-o", a.out] + objs + extra + \
                  ["-Wl,--image-base," + a.image_base, "-Wl,--disable-dynamicbase",
                   "-Wl,--disable-reloc-section", "-Wl,--out-implib," + a.out + ".a",
                   "-Wl,--defsym,%s=%s" % (sym("__executable_start"), sym("__ImageBase")),
                   "-Wl,-Map," + a.out + ".map"] + a.libs
        else:
            cmd = ["-m32", "-no-pie", "-o", a.out] + objs + extra + \
                  ["-Wl,-Map," + a.out + ".map"] + a.libs
        rsp = os.path.join(work, "link.rsp")
        open(rsp, "w").write("\n".join(cmd))
        return run([a.cc, "@" + rsp], check=False)

    r = link([])
    undef = {}
    for m in re.finditer(r"\((\.?[\w.]+)\+0x[0-9a-f]+\): undefined reference to `([^']+)'|"
                         r":\(\.?([\w.]+)\+0x[0-9a-f]+\): undefined reference to `([^']+)'",
                         r.stderr):
        sec, name = (m.group(1), m.group(2)) if m.group(2) else (m.group(3), m.group(4))
        undef.setdefault(name, set()).add(sec)
    for m in re.finditer(r"more undefined references to `([^']+)' follow", r.stderr):
        undef.setdefault(m.group(1), set()).add("text")
    for m in re.finditer(r"in function `[^']*':\n[^\n]*undefined reference to `([^']+)'", r.stderr):
        undef.setdefault(m.group(1), set()).add("text")
    if r.returncode and not undef:
        sys.stderr.write(r.stderr[-4000:])
        sys.exit(1)

    aliases = read_alias_scripts(syms_dir)
    OVL_EXTERNALS = {n for n, (e, u) in aliases.items() if u == "main" and n.startswith("data")}
    defsyms, tramp, fstubs, dstubs, aliased = [], [], [], [], []
    main_data = read_data_symbols(os.path.join(root, "config/symbols/main.txt"))
    all_defined = set().union(*gl.values())
    for name in sorted(undef):
        expr = aliases.get(name, (None, None))[0]
        is_text = any("text" in s for s in undef[name])
        addr = None
        if expr and re.fullmatch(r"0x[0-9A-Fa-f]+", expr):
            addr = int(expr, 16)
        elif re.fullmatch(r"func_(?:00)?([0-9A-Fa-f]{6})", name):
            addr = int(re.fullmatch(r"func_(?:00)?([0-9A-Fa-f]{6})", name).group(1), 16)
        in_ovl = addr is not None and 0xA00000 <= addr < 0xC00000
        if in_ovl and (name.startswith("func_") or name in OVL_EXTERNALS):
            tramp.append((name, addr))
        elif addr is not None and 0x338600 <= addr < 0xA00000 and \
                containing_symbol(main_data, gl["main"], addr):
            # a bare PS2 address inside an object the host defines (ov01's
            # D_426F48 is batDatBuf+8): alias it to the host object, so reads
            # and writes through either name hit the same storage
            host, off = containing_symbol(main_data, gl["main"], addr)
            defsyms.append("-Wl,--defsym,%s=%s+%d" % (sym(name), sym(host), off))
            aliased.append("%s = %s+0x%x" % (name, host, off))
        elif addr is not None:
            # a bare PS2 address the host has no object for: it points into
            # the mapped PS2 RAM image (or is a constant, e.g. D_2A007F8 =
            # the title screen's "overlay 2, entry 0xA007F8" return code)
            defsyms.append("-Wl,--defsym,%s=0x%x" % (sym(name), addr))
        elif False:
            defsyms.append("-Wl,--defsym,%s=0x%x" % (name, addr))
        elif expr and not re.fullmatch(r"0x[0-9A-Fa-f]+", expr):
            base = re.match(r"([\w.]+)", expr).group(1)
            if base in all_defined:
                defsyms.append("-Wl,--defsym,%s=%s" % (sym(name), re.sub(
                    r"^([A-Za-z_][\w.]*)", lambda k: sym(k.group(1)), expr.replace(" ", ""))))
            else:
                (fstubs if is_text else dstubs).append(name)
        else:
            (fstubs if is_text else dstubs).append(name)

    st = os.path.join(work, "autostubs.c")
    with open(st, "w") as fh:
        fh.write('/* generated by link_xsg.py: symbols nothing else defines */\n')
        fh.write('extern void xsg_autostub_hit(const char *name, unsigned *count, void *ra);\n')
        fh.write('extern void *xsg_ps2_func(unsigned addr);\n')
        for n in fstubs:
            fh.write('static unsigned c_%s;\nint %s(void) { xsg_autostub_hit("%s", &c_%s, '
                     '__builtin_return_address(0)); return 0; }\n' % (n, n, n, n))
        for n in dstubs:
            fh.write('unsigned char %s[4096] __attribute__((aligned(16)));\n' % n)
    tr = os.path.join(work, "trampolines.s")
    with open(tr, "w") as fh:
        fh.write("# generated by link_xsg.py: PS2 overlay address -> active overlay\n\t.text\n")
        for n, addr in tramp:
            t = "" if PE else "\t.type %s,@function\n" % n
            fh.write("\t.globl %s\n%s%s:\n\tpushl $0x%x\n\tcall %s\n"
                     "\taddl $4, %%esp\n\tjmp *%%eax\n" % (sym(n), t, sym(n), addr, sym("xsg_ps2_func")))
    so = os.path.join(work, "autostubs.o")
    run([a.cc, "-m32", "-c", "-w", "-fno-pic", "-o", so, st])
    to = os.path.join(work, "trampolines.o")
    run([TOOL["as"], "--32", "-o", to, tr])
    with open(os.path.join(b, "autostubs.txt"), "w") as fh:
        fh.write("# defsym\n" + "\n".join(defsyms) + "\n# trampolines\n" +
                 "\n".join("%s 0x%x" % t for t in tramp) + "\n# function stubs\n" +
                 "\n".join(fstubs) + "\n# data stubs\n" + "\n".join(dstubs) + "\n")
    open(os.path.join(b, "address_aliases.txt"), "w").write("\n".join(aliased) + "\n")
    r = link([so, to] + defsyms)
    open(os.path.join(b, "link.log"), "w").write(r.stdout + r.stderr)
    if r.returncode:
        sys.stderr.write(r.stderr[-6000:])
        sys.exit(r.returncode)
    print("link_xsg: %s: %d overlay symbols scoped, %d defsym, %d trampolines, "
          "%d function stubs, %d data stubs" % (a.out, len(clash_log), len(defsyms), len(tramp),
                                                len(fstubs), len(dstubs)))


if __name__ == "__main__":
    main()

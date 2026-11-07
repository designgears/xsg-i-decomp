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


def run(cmd, check=True):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if check and r.returncode:
        sys.stderr.write(" ".join(cmd)[:2000] + "\n" + r.stdout + r.stderr)
        sys.exit(1)
    return r


def nm(obj):
    """-> (global_defined, local_counts, undefined)"""
    g, loc, u = set(), {}, set()
    for line in run(["nm", obj]).stdout.splitlines():
        parts = line.split()
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


def read_alias_scripts(root):
    """name -> expression from the PS2 link's alias/undefined-symbol scripts."""
    al = {}
    for u in UNITS:
        d = os.path.join(root, "build", u)
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
    ap.add_argument("--libs", nargs="*", default=[])
    a = ap.parse_args()
    b = os.path.abspath(a.build)
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
    data_dir = os.path.join(b, "data")
    work = os.path.join(b, "link")
    os.makedirs(work, exist_ok=True)

    # 1. data
    jobs = []
    data_objs = {u: [] for u in UNITS}
    for u in UNITS:
        d = os.path.join(data_dir, u)
        for f in sorted(os.listdir(d)):
            if f.endswith(".s"):
                src = os.path.join(d, f)
                obj = os.path.join(work, u, f[:-2] + ".o")
                data_objs[u].append(obj)
                if stale(obj, [src]):
                    os.makedirs(os.path.dirname(obj), exist_ok=True)
                    jobs.append(["as", "--32", "-o", obj, src])
    with ThreadPoolExecutor(os.cpu_count()) as ex:
        list(ex.map(run, jobs))

    c_objs = {}
    for u in UNITS:
        objs = []
        for dp, _, fs in os.walk(os.path.join(b, "CMakeFiles", "xsg_%s.dir" % u)):
            objs += [os.path.join(dp, f) for f in fs if f.endswith(".o")]
        c_objs[u] = sorted(objs)

    # 2./3. partial link + globalize referenced statics
    unit_obj = {}
    gl = {}
    report = []
    for u in UNITS:
        p = os.path.join(work, u + ".r.o")
        run(["ld", "-m", "elf_i386", "-r", "-o", p] + c_objs[u] + data_objs[u])
        g, loc, und = nm(p)
        want = set(und)
        tab = []
        if u in OVERLAYS:
            for addr, name in read_symbol_txt(os.path.join(root, "config/symbols/%s.txt" % u)):
                if name in g or loc.get(name) == 1:
                    tab.append((addr, name))
                    want.add(name)
        glob = sorted(n for n in want if loc.get(n) == 1 and n not in g)
        dup = sorted(n for n in und if loc.get(n, 0) > 1)
        report += ["%s globalized %s" % (u, n) for n in glob]
        report += ["%s AMBIGUOUS static %s (%d copies)" % (u, n, loc[n]) for n in dup]
        if u == "main":
            rename = ["--redefine-sym", "main=xsg_game_main"]
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
            open(lst, "w").write("\n".join(glob) + "\n")
            args.append("--globalize-symbols=" + lst)
        run(args + [p, q])
        if tab:
            s = os.path.join(work, u + "_ovtab.s")
            with open(s, "w") as fh:
                fh.write("\t.section .rodata\n\t.globl xsg_ovtab_%s\n\t.balign 4\nxsg_ovtab_%s:\n" % (u, u))
                for addr, name in sorted(tab):
                    fh.write("\t.long 0x%08x, %s\n" % (addr, name))
                fh.write("\t.long 0, 0\n")
            so = s[:-2] + ".o"
            run(["as", "--32", "-o", so, s])
            q2 = os.path.join(work, u + ".t.o")
            run(["ld", "-m", "elf_i386", "-r", "-o", q2, q, so])
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
        open(lst, "w").write("\n".join(clash) + "\n")
        run(["objcopy", "--localize-symbols=" + lst, unit_obj[u], f])
        final[u] = f
    open(os.path.join(b, "overlay_clashes.txt"), "w").write("\n".join(clash_log) + "\n")
    open(os.path.join(b, "globalized.txt"), "w").write("\n".join(report) + "\n")

    # 5./6. link, resolve leftovers, relink
    objs = a.runtime + [final[u] for u in UNITS]

    def link(extra):
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

    aliases = read_alias_scripts(root)
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
            sym, off = containing_symbol(main_data, gl["main"], addr)
            defsyms.append("-Wl,--defsym,%s=%s+%d" % (name, sym, off))
            aliased.append("%s = %s+0x%x" % (name, sym, off))
        elif addr is not None:
            # a bare PS2 address the host has no object for: it points into
            # the mapped PS2 RAM image (or is a constant, e.g. D_2A007F8 =
            # the title screen's "overlay 2, entry 0xA007F8" return code)
            defsyms.append("-Wl,--defsym,%s=0x%x" % (name, addr))
        elif False:
            defsyms.append("-Wl,--defsym,%s=0x%x" % (name, addr))
        elif expr and not re.fullmatch(r"0x[0-9A-Fa-f]+", expr):
            base = re.match(r"([\w.]+)", expr).group(1)
            if base in all_defined:
                defsyms.append("-Wl,--defsym,%s=%s" % (name, expr.replace(" ", "")))
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
            fh.write("\t.globl %s\n\t.type %s,@function\n%s:\n\tpushl $0x%x\n\tcall xsg_ps2_func\n"
                     "\taddl $4, %%esp\n\tjmp *%%eax\n" % (n, n, n, addr))
    so = os.path.join(work, "autostubs.o")
    run([a.cc, "-m32", "-c", "-w", "-fno-pic", "-o", so, st])
    to = os.path.join(work, "trampolines.o")
    run(["as", "--32", "-o", to, tr])
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

#!/usr/bin/env python3
"""Stage 1 host-compile survey for the Xenosaga I PC port.

Compiles every game C TU (src/main, src/ov01/02/10/11/12; IOP src/audio is out
of scope) with the host gcc and classifies the diagnostics.  Read-only with
respect to the PS2 build: nothing under src/, include/, config/ or build/ is
written.  Objects go to /dev/null.

Passes:
  m32      gcc -m32, as-is (inline asm reaches the x86 assembler)
  m32stub  gcc -m32, inline asm stubbed out by macros and mode(TI) -> DI,
           to expose the C-level blockers hidden behind the first errors
  m64      gcc (x86-64, LP64), as-is, for pointer-size issues
  m64stub  x86-64 with asm stubbed

Also writes a static inventory (inline asm by instruction kind, SDK symbols,
hardware addresses).  Results: port/build/survey/*.json and a text summary.

Usage: python3 port/scripts/host_compile_survey.py [-j N] [--passes m32,m64,...]
"""
import argparse
import collections
import concurrent.futures as cf
import glob
import json
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "port", "build", "survey")
GCC = os.environ.get("HOST_CC", "/usr/bin/gcc")

# Same include contract as the PS2 per-TU build (build/<unit>/build.ninja:
# `-Iinclude -I.` from the unit dir; build/<unit>/include == include/,
# build/<unit>/src -> ../../src), so from the repo root: -Iinclude -I.
BASE = [
    "-std=gnu89", "-c", "-o", "/dev/null", "-fno-builtin", "-fno-common",
    "-fdiagnostics-plain-output", "-fno-diagnostics-show-caret",
    "-fpermissive", "-fno-pic", "-fno-stack-protector",
    "-Wno-error=implicit-function-declaration", "-Wno-error=implicit-int",
    "-Wno-error=int-conversion", "-Wno-error=incompatible-pointer-types",
    "-Wno-error=return-mismatch", "-Wno-error=declaration-missing-parameter-type",
    "-Wint-to-pointer-cast", "-Wpointer-to-int-cast",
    "-include", "port/include/ps2_host_compat.h",
    "-Iinclude", "-I.",
]
# Stub passes compile a mirror of src/ + include/ (port/build/survey/stubtree)
# in which every inline-asm statement and `register ... asm("$x")` binding is
# removed textually (macros cannot do it: `__asm__ __volatile__ (` defeats a
# function-like macro), and mode(TI) is mapped to DI.
STUB = ["-DTI=DI"]
STUBTREE = os.path.join(OUT, "stubtree")
OBJDIR = os.path.join(OUT, "obj")
STUBBED = {"m32stub", "m64stub"}
PASSES = {
    "m32": ["-m32"],
    "m32stub": ["-m32"] + STUB,
    "m64": [],
    "m64stub": STUB,
}

DIAG = re.compile(r"^(?P<file>[^:\n]+):(?P<line>\d+):(?:\d+:)? (?P<kind>error|warning|fatal error): (?P<msg>.*)$")
ASDIAG = re.compile(r"^(?P<file>[^:\n]+):(?P<line>\d+): Error: (?P<msg>.*)$")

# (category, regex on message) -- first match wins
CATS = [
    ("inline_asm", r"invalid register name|impossible constraint|asm operand|unknown register name|"
                   r"inconsistent operand constraints|expected .* before .volatile.|`volatile'|"
                   r"^ASM: "),
    ("int128_TI", r"unable to emulate|TImode|__int128"),
    ("missing_header", r"No such file or directory"),
    ("implicit_function_decl", r"implicit declaration of function"),
    ("implicit_int", r"type defaults to .int.|return type defaults to"),
    ("ptr_size_cast", r"cast (to|from) pointer (from|to) integer of different size"),
    ("int_conversion", r"makes (pointer from integer|integer from pointer)|\[-Wint-conversion\]"),
    ("incompatible_pointer", r"incompatible pointer type|\[-Wincompatible-pointer-types\]"),
    ("conflicting_decl", r"conflicting types|redefinition of|redeclared|previous (declaration|definition)|"
                         r"static declaration of .* follows non-static|"
                         r"conflicting type qualifiers"),
    ("return_mismatch", r"\[-Wreturn-mismatch\]|return. with (a|no) value"),
    ("builtin_mismatch", r"built-in function|\[-Wbuiltin-declaration-mismatch\]"),
    ("cast_as_lvalue", r"lvalue required as (increment|decrement|left) operand"),
    ("array_size_assert", r"size of array .* is negative|static assertion"),
    ("layout_size", r"size of .* is too large|width of .* exceeds"),
]
CATS = [(n, re.compile(r)) for n, r in CATS]
# Diagnostics we count even as warnings (portability-relevant)
WARN_CATS = {"implicit_function_decl", "implicit_int", "ptr_size_cast", "int_conversion",
             "incompatible_pointer", "conflicting_decl", "return_mismatch", "builtin_mismatch"}


def classify(msg):
    for name, rx in CATS:
        if rx.search(msg):
            return name
    return "other"


def tus():
    files = []
    for unit in ("main", "ov01", "ov02", "ov10", "ov11", "ov12"):
        files += glob.glob(f"src/{unit}/**/*.c", recursive=True)
    return sorted(files)


def compile_one(args):
    path, pname = args
    cmd = [GCC] + PASSES[pname] + BASE + [path]
    if pname == "m32stub":  # keep real objects for the external-symbol scan
        obj = os.path.join(OBJDIR, path.replace("/", "__") + ".o")
        cmd[cmd.index("/dev/null")] = obj
    if pname in STUBBED:
        cmd[cmd.index("port/include/ps2_host_compat.h")] = os.path.join(ROOT, "port/include/ps2_host_compat.h")
    try:
        p = subprocess.run(cmd, cwd=STUBTREE if pname in STUBBED else ROOT, capture_output=True, text=True, timeout=120)
        text, rc = p.stderr, p.returncode
    except subprocess.TimeoutExpired:
        return path, {"rc": -1, "diags": [["error", "other", "timeout"]]}
    diags = []
    for line in text.splitlines():
        m = DIAG.match(line)
        if m:
            kind = "error" if "error" in m["kind"] else "warning"
            cat = classify(m["msg"])
            if kind == "error" or cat in WARN_CATS:
                diags.append([kind, cat, m["msg"][:200], m["file"]])
            continue
        m = ASDIAG.match(line)
        if m:  # x86 assembler rejecting MIPS/VU inline asm
            diags.append(["error", "inline_asm", "ASM: " + m["msg"][:200], path])
    return path, {"rc": rc, "diags": diags}


# ---------------------------------------------------------------- static scans

ASM_HEAD = re.compile(r"\b(?:__asm__|asm)\s*(?:(?:__volatile__|volatile|__volatile)\s*)?\(")


def is_asm_label(t, start):
    """`extern T x[] __asm__("sym");` -- a symbol alias, not an asm statement."""
    j = start - 1
    while j >= 0 and t[j] in " \t\n":
        j -= 1
    if j < 0:
        return False
    line = t[t.rfind("\n", 0, start) + 1:start]
    if line.lstrip().startswith("#"):
        # `#define SYM(name) __asm__(#name)`: label macro (stringized operand)
        return re.match(r"\s*\(\s*#\s*\w+\s*\)", t[t.index("(", start):]) is not None
    return t[j] == "]" or t[j].isalnum() or t[j] == "_" or t[j] == ")"


def asm_blocks(t, labels=False):
    """Yield (start, end, body) of each asm statement, balanced-paren aware."""
    pos = 0
    while True:
        m = ASM_HEAD.search(t, pos)
        if not m:
            return
        # skip `register x asm("$2")` bindings (handled separately)
        line_start = t.rfind("\n", 0, m.start()) + 1
        if re.search(r"\bregister\b[^;]*$", t[line_start:m.start()]):
            pos = m.end()
            continue
        if is_asm_label(t, m.start()) != labels:
            pos = m.end()
            continue
        i, depth, instr = m.end(), 1, False
        while i < len(t) and depth:
            ch = t[i]
            if instr:
                if ch == "\\":
                    i += 1
                elif ch == '"':
                    instr = False
            elif ch == '"':
                instr = True
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
            i += 1
        yield m.start(), i, t[m.end():i - 1]
        pos = i
REG_ASM = re.compile(r"\bregister\b[^;=]*\b(?:asm|__asm__)\s*\(\s*\"([^\"]*)\"")


def strip_comments(t):
    return re.sub(r"/\*.*?\*/|//[^\n]*", lambda m: re.sub(r"[^\n]", " ", m.group(0)), t, flags=re.S)


def asm_kind(mn):
    mn = mn.lower()
    base = mn.split(".")[0]
    if base in ("lqc2", "sqc2", "qmfc2", "qmtc2", "cfc2", "ctc2", "vcallms", "vcallmsr") or \
            base.startswith("v"):
        return "VU0 macro (COP2)"
    if base in ("lq", "sq"):
        return "128-bit GPR load/store (lq/sq)"
    if base.startswith("p") or base in ("qfsrv", "mfsa", "mtsa", "mfhi1", "mflo1", "mthi1", "mtlo1",
                                         "mult1", "multu1", "div1", "divu1", "madd1", "maddu1"):
        return "MMI (128-bit SIMD)"
    if base in ("sync", "sync.l", "sync.p", "ei", "di", "eret", "syscall", "cache", "mfc0", "mtc0", "bc0f", "bc0t"):
        return "system (sync/cop0/cache)"
    if base in ("", "nop"):
        return "empty / compiler barrier"
    return "other MIPS (scalar/FPU)"


def stub_text(t):
    t = strip_comments(t)
    out, last = [], 0
    for a, b, _ in asm_blocks(t):
        toplevel = a == 0 or t[a - 1] == "\n"   # file-scope asm (ELF .equ/.size tricks)
        out += [t[last:a], "" if toplevel else "((void)0)", "\n" * t[a:b].count("\n")]
        last = b
    t = "".join(out) + t[last:]
    t = re.sub(r"(\bregister\b[^;=]*?)\b(?:asm|__asm__)\s*\(\s*\"[^\"]*\"\s*\)", r"\1", t)
    return t


def build_stubtree():
    for top in ("src", "include"):
        for dp, _, fns in os.walk(os.path.join(ROOT, top)):
            for fn in fns:
                if not fn.endswith((".c", ".h")):
                    continue
                src = os.path.join(dp, fn)
                dst = os.path.join(STUBTREE, os.path.relpath(src, ROOT))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                t = open(src, errors="replace").read()
                if fn != "include_asm.h":
                    t = stub_text(t)
                open(dst, "w").write(t)


def scan_inline_asm(files):
    kinds, mnem, per_file = collections.Counter(), collections.Counter(), collections.Counter()
    blocks = regasm = 0
    regasm_files = collections.Counter()
    toplevel = collections.Counter()
    for f in files:
        t = strip_comments(open(os.path.join(ROOT, f), errors="replace").read())
        for m in REG_ASM.finditer(t):
            regasm += 1
            regasm_files[f] += 1
        for a, _, body in asm_blocks(t):
            if a == 0 or t[a - 1] == "\n":
                toplevel[f] += 1
            # template = leading string literals before the first ':'
            tmpl = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', body.split(":")[0]))
            blocks += 1
            per_file[f] += 1
            insns = [s.strip() for s in re.split(r"\\n|\\t|;", tmpl) if s.strip()]
            if not insns:
                kinds["empty / compiler barrier"] += 1
                mnem["<empty>"] += 1
            for ins in insns:
                mn = ins.split()[0]
                if mn.endswith(":") or mn.startswith("."):
                    continue
                mnem[mn.lower()] += 1
                kinds[asm_kind(mn)] += 1
    labels = collections.Counter()
    for f in files:
        t = strip_comments(open(os.path.join(ROOT, f), errors="replace").read())
        for _ in asm_blocks(t, labels=True):
            labels[f] += 1
    return {"blocks": blocks, "asm_symbol_labels": sum(labels.values()), "asm_label_files": len(labels),
            "toplevel_asm": sum(toplevel.values()), "toplevel_asm_files": len(toplevel), "files": len(per_file), "register_asm_vars": regasm,
            "register_asm_files": dict(regasm_files.most_common()),
            "by_kind": dict(kinds.most_common()), "by_mnemonic": dict(mnem.most_common()),
            "top_files": dict(per_file.most_common(20))}


HW = [
    ("EE hw regs 0x1000xxxx (timers/DMAC/VIF/GIF/IPU/INTC)", r"0x1000[0-9a-f]{4}\b"),
    ("EE hw regs 0x1001xxxx..0x1100xxxx (VU mem / other)", r"0x(?:100[1-9a-f]|110[0-9a-f])[0-9a-f]{4}\b"),
    ("GS privileged regs 0x1200xxxx", r"0x1200[0-9a-f]{4}\b"),
    ("Scratchpad 0x70000000..0x70003fff", r"0x7000[0-3][0-9a-f]{3}\b"),
    ("Uncached 0x2xxxxxxx constants/ORs", r"0x2[0-9a-f]{7}\b"),
    ("Uncached-accel 0x3xxxxxxx constants/ORs", r"0x3[0-9a-f]{7}\b"),
    ("KSEG0/1 0x8xxxxxxx/0xAxxxxxxx", r"0x[8a][0-9a-f]{7}\b"),
]


def scan_hw(files):
    res = {}
    for name, rx in HW:
        r = re.compile(rx, re.I)
        tot, perf = 0, collections.Counter()
        for f in files:
            t = strip_comments(open(os.path.join(ROOT, f), errors="replace").read())
            n = len(r.findall(t))
            if n:
                perf[f] = n
                tot += n
        res[name] = {"count": tot, "files": len(perf), "top": dict(perf.most_common(5))}
    # explicit OR/AND masks with the uncached segments
    rx = re.compile(r"[|&]\s*\(?\s*(?:\(\s*\w+\s*\))?\s*0x[23]0000000|0x[23]0000000\s*\|", re.I)
    perf = collections.Counter()
    for f in files:
        t = strip_comments(open(os.path.join(ROOT, f), errors="replace").read())
        n = len(rx.findall(t))
        if n:
            perf[f] = n
    res["Explicit |/& 0x20000000 or 0x30000000"] = {"count": sum(perf.values()), "files": len(perf),
                                                    "top": dict(perf.most_common(5))}
    return res


SDK_PREFIX = [
    ("libgraph (sceGs*)", r"sceGs"), ("libdma (sceDma*)", r"sceDma"), ("libvu0 (sceVu0*)", r"sceVu0"),
    ("libpad (scePad*)", r"scePad"), ("libcdvd (sceCd*)", r"sceCd"), ("libmc (sceMc*)", r"sceMc"),
    ("libsif/sifrpc (sceSif*)", r"sceSif"), ("libipu/libmpeg (sceIpu*/sceMpeg*)", r"sce(?:Ipu|Mpeg)"),
    ("libdev/libpkt (sceDev*/sceVif*/sceGif*)", r"sce(?:Dev|Vif|Gif)"),
    ("libhdd/libpfs (sceHdd*/scePfs*)", r"sce(?:Hdd|Pfs|Dev9)"), ("libscf (sceScf*)", r"sceScf"),
    ("sce file I/O (sceOpen/sceRead/...)", r"sce(?:Open|Close|Read|Write|Lseek|Ioctl|Dopen|Dread|Dclose|Mkdir|Remove|Getstat|Format|Sync|Mount|Umount|Devctl|Rename|Chstat|Fs)"),
    ("other sce*", r"sce"),
    ("kernel syscalls (Thread/Sema/Intc/Dma handlers...)", r"(?:Create|Delete|Start|Exit|Terminate|Sleep|Wakeup|iWakeup|Suspend|Resume|Change|Rotate|Release|Refer|iRefer|Get|Set)(?:Thread|Sema|Alarm|ThreadPriority|ThreadId)\w*|(?:Signal|iSignal|Wait|Poll|iPoll)Sema|(?:Add|Remove|Enable|Disable)(?:Intc|Dmac)\w*|i?(?:Enable|Disable)(?:Intc|Dmac)|FlushCache|iFlushCache|SyncDCache|iSyncDCache|InvalidDCache|iInvalidDCache|SetAlarm|iSetAlarm|ReleaseAlarm|iReleaseAlarm|ExitThread|SifSetDma|isceSif\w*|DIntr|EIntr|GsPutIMR|GsGetIMR|SetGsCrt|_ExitThread|LoadExecPS2|ExecPS2|Exit|GetThreadId|SetupThread|InitMainThread|SetupHeap|EndOfHeap|ChangeThreadPriority|ReferThreadStatus|SetVSyncFlag|SetSyscall|iSetEventFlag|scePrintf"),
]


def scan_sdk(files):
    """External (undefined-in-game-C) callees grouped by library.

    Game-defined functions are collected from all C files; anything called but
    not defined there and not a libc name is an external platform symbol.
    """
    defs, calls = set(), collections.Counter()
    fdef = re.compile(r"^[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{", re.M)
    call = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
    texts = {}
    for f in files:
        t = strip_comments(open(os.path.join(ROOT, f), errors="replace").read())
        t = re.sub(r'"(?:[^"\\\n]|\\.)*"', '""', t)
        texts[f] = t
        defs |= set(fdef.findall(t))
    kw = {"if", "while", "for", "switch", "return", "sizeof", "__asm__", "asm", "__attribute__",
          "__volatile__", "volatile", "mode", "aligned", "section", "__builtin_expect", "defined",
          "__typeof__", "typeof", "do", "else", "case", "packed", "unused"}
    for f, t in texts.items():
        for m in call.finditer(t):
            n = m.group(1)
            if n in kw or n in defs:
                continue
            calls[n] += 1
    groups = collections.OrderedDict((g, {}) for g, _ in SDK_PREFIX)
    groups["libc / libm"] = {}
    groups["undefined-in-C (game asm/runtime or macro)"] = {}
    libc = re.compile(r"^(?:mem\w+|str\w+|printf|sprintf|vsprintf|snprintf|puts|abs|labs|rand|srand|qsort|bsearch|"
                      r"malloc|free|calloc|realloc|memalign|atoi|atof|atol|strtol|strtoul|exit|abort|"
                      r"sinf?|cosf?|tanf?|asinf?|acosf?|atanf?|atan2f?|sqrtf?|fabsf?|powf?|expf?|logf?|"
                      r"floorf?|ceilf?|fmodf?|ldexpf?|frexpf?|modff?|toupper|tolower|isdigit|isalpha|isspace|"
                      r"bzero|bcopy|setjmp|longjmp|fopen|fclose|fread|fwrite|fprintf|va_start|va_end|va_arg|"
                      r"__builtin_\w+)$")
    for n, c in calls.most_common():
        for g, rx in SDK_PREFIX:
            if re.match(rx + r"\w*$", n) if g == "other sce*" else re.match(r"^(?:" + rx + r")\w*$", n):
                groups[g][n] = c
                break
        else:
            if libc.match(n):
                groups["libc / libm"][n] = c
            elif re.match(r"^[a-z_]", n) is None or True:
                groups["undefined-in-C (game asm/runtime or macro)"][n] = c
    return groups


def ps2_symbol_owners():
    """symbol -> (unit, tu, category) from the PS2 link maps.

    The defining input object (build/c/<tu>.o, build/expected/<tu>.o,
    build/hasm/<tu>.o, build/data/...) is mapped to the TU category of
    config/objects/main.compile.json (game / sdk / runtime / asm)."""
    cats = {k: v["category"] for k, v in
            json.load(open(os.path.join(ROOT, "config/objects/main.compile.json")))["tus"].items()}
    owners = {}
    secline = re.compile(r"^ ?\.?[\w.]*\s+0x[0-9a-f]+\s+0x[0-9a-f]+\s+(build/\S+\.o)\s*$")
    sym = re.compile(r"^\s+0x([0-9a-f]{8,16})\s+([A-Za-z_][\w.$]*)\s*$")
    for unit in ("main", "ov01", "ov02", "ov10", "ov11", "ov12"):
        mp = os.path.join(ROOT, f"build/{unit}/build/{unit}.elf.map")
        if not os.path.exists(mp):
            continue
        cur = None
        for line in open(mp, errors="replace"):
            m = secline.match(line)
            if m:
                cur = m.group(1)
                continue
            m = sym.match(line)
            if not m or cur is None or m.group(2).startswith((".L", "__t_")):
                continue
            name = m.group(2)
            if unit != "main":
                owners.setdefault(name, (unit, cur, "game"))
                continue
            base = cur.split("/")[-1][:-2].split(".")[0]
            if "/ov02/" in cur:
                cat = "game"
            elif cur.startswith("build/data/"):
                cat = cats.get(base, "data")
            else:
                cat = cats.get(base, "?")
            owners.setdefault(name, ("main", base, cat))
    return owners


def scan_externals():
    """Undefined symbols of the host objects that no game TU defines."""
    defined, undef = set(), collections.Counter()
    for fn in os.listdir(OBJDIR):
        out = subprocess.run(["nm", os.path.join(OBJDIR, fn)], capture_output=True, text=True).stdout
        for line in out.splitlines():
            parts = line.split()
            if len(parts) == 2 and parts[0] == "U":
                undef[parts[1]] += 1
            elif len(parts) == 3 and parts[1] not in "Uu":
                defined.add(parts[2])
    ext = {n: c for n, c in undef.items() if n not in defined}
    # symbols the 39 TUs that do not host-compile define (game C, just missing here)
    owners = ps2_symbol_owners()
    by_cat = collections.defaultdict(dict)
    for n, c in sorted(ext.items(), key=lambda x: -x[1]):
        unit, tu, cat = owners.get(n, (None, None, "unresolved"))
        by_cat[cat][n] = {"objs": c, "tu": tu}
    return {"host_objects": len(os.listdir(OBJDIR)), "external_symbols": len(ext),
            "by_category": by_cat}


def scan_ovl(files):
    rx = re.compile(r"\b\w*(?:[Oo]verlay|Ovl|OVL|ov(?:01|02|10|11|12))\w*\b")
    c = collections.Counter()
    for f in files:
        t = strip_comments(open(os.path.join(ROOT, f), errors="replace").read())
        for m in rx.findall(t):
            c[m] += 1
    return dict(c.most_common(40))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", type=int, default=os.cpu_count())
    ap.add_argument("--passes", default=",".join(PASSES))
    a = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    files = tus()
    build_stubtree()
    os.makedirs(OBJDIR, exist_ok=True)
    summary = {"tus": len(files), "gcc": subprocess.run([GCC, "--version"], capture_output=True,
                                                          text=True).stdout.splitlines()[0]}
    for pname in a.passes.split(","):
        res = {}
        with cf.ThreadPoolExecutor(a.j) as ex:
            for path, r in ex.map(compile_one, [(f, pname) for f in files]):
                res[path] = r
        json.dump(res, open(os.path.join(OUT, f"{pname}.json"), "w"), indent=1)
        err, warn, ferr = collections.Counter(), collections.Counter(), collections.Counter()
        per = collections.Counter()
        for f, r in res.items():
            seen = set()
            for kind, cat, msg, *_ in r["diags"]:
                (err if kind == "error" else warn)[cat] += 1
                if kind == "error":
                    per[f] += 1
                    seen.add(cat)
            for c in seen:
                ferr[c] += 1
        clean = sum(1 for r in res.values() if r["rc"] == 0)
        summary[pname] = {"compiled_ok": clean, "failed": len(files) - clean,
                          "errors_by_cat": dict(err.most_common()),
                          "files_with_error_cat": dict(ferr.most_common()),
                          "warnings_by_cat": dict(warn.most_common()),
                          "top_files": dict(per.most_common(15))}
        print(f"[{pname}] ok={clean}/{len(files)} errors={sum(err.values())} {dict(err.most_common())}",
              file=sys.stderr)
    summary["inline_asm"] = scan_inline_asm(files)
    summary["hardware"] = scan_hw(files)
    summary["sdk"] = scan_sdk(files)
    summary["externals"] = scan_externals()
    summary["overlay_symbols"] = scan_ovl(files)
    json.dump(summary, open(os.path.join(OUT, "summary.json"), "w"), indent=1)
    print(os.path.join(OUT, "summary.json"))


if __name__ == "__main__":
    main()

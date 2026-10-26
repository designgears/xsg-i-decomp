#!/usr/bin/env python3
"""Lock-free, function-level match check for one candidate C body.

    python3 -B scripts/auto_decomp/try_fn.py \\
        --unit main --tu menu_tec --name MenuTecNextSpeedPointGet --body fn.c

Builds two objects of the TU in a private scratch directory under
build/<unit>/try/ with the exact compile command of the unit's build.ninja:

  base: the TU with NAME as INCLUDE_ASM (the original bytes)
  cand: the TU with NAME replaced by --body

and compares NAME's .text bytes and relocations between them. cc1 sees file
names exactly as long as the real build's (same_len_link: they shift its heap,
and ee-gcc 2.96 can hash heap addresses) and runs with ASLR off. Nothing under
src/ or the unit's real build outputs is touched, so any number of agents can
run this at once. Iterate here until "match": true, then call integrate.py
once for the whole-file gate.

"match_after_link": true (with "false_lo16": [...]) means every differing
word is a false %lo relocation: splat symbolized a plain immediate (target
`addiu t0,t0,%lo(D_70000010)` fed by a reloc-free `lui t0,0x7000`, ours
`addiu t0,t0,16`). The linked bytes are identical; hand it to
`integrate.py --skip-try` (its whole-file gate is the real check).

It is also set when the remaining slots are "lit_literal": [...]: the target
loads a .lit4/.lit8 constant through a GPREL16 reloc to its data symbol
(D_xxx of a *.lit4.s / *.lit8.s file), the candidate writes the same value as
a float/double literal (LITERAL reloc to .lit4). That is how such constants
must be written (an extern load changes sched1's order); the carve places the
literal at D_xxx, so again: `integrate.py --skip-try`.

"match_pending_jtbl": true (implies link_check) means the only differing
words are jump-table lookups: target `%lo(jtbl_XXXXXXXX)` with addend 0, ours
`%lo(.rodata)` with the table's offset as addend. Same opcode, registers and
relocation types: it IS a match once integrate's data carve places the table
-> `integrate.py --skip-try` (nmlModelDirectSend).

Prints a JSON report; with --diff (default on) also prints a side-by-side
instruction diff to stderr. Exit 0 = match, 1 = mismatch, 2 = error.
"""
from __future__ import annotations

import argparse
import difflib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tu"))
from elfinfo import Elf  # noqa: E402
from noaslr import NOASLR  # noqa: E402,F401  (re-exported for trick_sweep/snippet)

TOOLCHAIN = Path(os.environ.get("XENO_TOOLCHAIN_DIR", ROOT / ".work/toolchains"))
OBJDUMP = TOOLCHAIN / "ee-binutils/bin/mips64r5900el-ps2-elf-objdump"


class TryError(Exception):
    pass


# ---------------------------------------------------------------- binding --

def asm_binding(unit: str, tu: str, name: str) -> str | None:
    """'local' / 'global' from the splat `glabel NAME[, local]` line, or None."""
    for asm in (ROOT / "build" / unit / "asm" / unit / "nonmatchings" / tu / f"{name}.s",
                ROOT / "build" / unit / "asm" / "nonmatchings" / unit / tu / f"{name}.s"):
        if asm.is_file():
            m = re.search(r"^\s*glabel\s+" + re.escape(name) + r"\b([^\n]*)$", asm.read_text(errors="replace"), re.M)
            if m:
                return "local" if "local" in m.group(1) else "global"
    return None


def _def_heads(text: str, name: str):
    """Offsets of the return-type prefix of NAME's definition(s) in TEXT."""
    for m in re.finditer(r"^([A-Za-z_][^\n;{}#=]*?)\b" + re.escape(name) + r"\s*\(", text, re.M):
        depth, j = 0, m.end() - 1
        while j < len(text):
            if text[j] == "(":
                depth += 1
            elif text[j] == ")":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        if re.match(r"\s*\{", text[j + 1:]):
            yield m.start(), m.group(1)


def static_fix(unit: str, tu: str, name: str, text: str) -> str | None:
    """The body text with NAME's definition made `static` when the original
    symbol is local (`glabel NAME, local`) and the body defines it non-static
    (mapcheck "C symbol differs from original" bind 0 vs 1 after integrate:
    five workers hit it on 2026-10-06), or the reverse (static -> global, not
    for inline helpers). None when nothing to change."""
    b = asm_binding(unit, tu, name)
    if not b:
        return None
    for st, pre in _def_heads(text, name):
        words = pre.split()
        if b == "local" and "static" not in words:
            return text[:st] + "static " + text[st:]
        if b == "global" and "static" in words and "inline" not in words and "__inline__" not in words:
            return text[:st] + re.sub(r"\bstatic\s+", "", pre, count=1) + text[st + len(pre):]
        return None
    return None


_ORIG_LOCAL_STATICS: dict[str, set] = {}


def original_dotted_symbols(unit: str) -> set:
    """`name.N` symbols (gcc's function-local statics) of the original image."""
    if unit in _ORIG_LOCAL_STATICS:
        return _ORIG_LOCAL_STATICS[unit]
    out: set = set()
    try:
        sys.path.insert(0, str(ROOT / "tools/tu"))
        from elfinfo import Elf  # type: ignore
        from verify_unit import original_path  # noqa: E402
        out = {x.name for x in Elf(original_path(ROOT, unit).read_bytes()).symbols
               if x.name and re.fullmatch(r"[A-Za-z_]\w*\.\d+", x.name)}
    except Exception:
        pass
    _ORIG_LOCAL_STATICS[unit] = out
    return out


def local_static_fix(unit: str, tu: str, name: str, text: str) -> str | None:
    """Move a file-scope `static T X_N ...;` into NAME as `static T X ...;` when
    the original image's symbol is gcc's function-local `X.N` (splat renames it
    X_N). Same code bytes; without it mapcheck fails "original symbol not
    defined by the C object" (o411 MAP_serach visible.0). None when nothing to do."""
    dotted = original_dotted_symbols(unit)
    if not dotted:
        return None
    heads = list(_def_heads(text, name))
    if not heads:
        return None
    changed = text
    for m in reversed(list(re.finditer(
            r"^static\s+([^;(){}=]*?)\b([A-Za-z_]\w*?)_(\d+)\b(\s*(?:\[[^\]]*\])*\s*(?:=[^;]*)?;)[^\n]*\n",
            changed, re.M))):
        base, num = m.group(2), m.group(3)
        if f"{base}.{num}" not in dotted:
            continue
        rest = changed[:m.start()] + changed[m.end():]
        st = next(iter(_def_heads(rest, name)), None)
        if st is None:
            continue
        # X already names something the function could see (prototype parameter
        # names do not count: they are out of scope)
        word = re.compile(r"\b" + re.escape(base) + r"\b")
        if word.search(rest[st[0]:]) or any(word.search(ln) and "(" not in ln
                                            for ln in rest[:st[0]].splitlines()):
            continue
        brace = rest.find("{", st[0])
        if brace < 0:
            continue
        decl = f"\n    static {m.group(1).strip()} {base}{m.group(4).rstrip()}"
        rest = rest[:brace + 1] + decl + rest[brace + 1:]
        changed = re.sub(r"\b" + re.escape(f"{base}_{num}") + r"\b", base, rest)
    return changed if changed != text else None


# ---------------------------------------------------------------- ninja ----

def _logical_lines(text: str):
    buf = ""
    for line in text.splitlines():
        if line.endswith("$") and not line.endswith("$$"):
            buf += line[:-1]
            continue
        yield buf + line
        buf = ""
    if buf:
        yield buf


def _split_unescaped(s: str, sep: str) -> list[str]:
    out, cur, i = [], "", 0
    while i < len(s):
        c = s[i]
        if c == "$" and i + 1 < len(s):
            cur += s[i:i + 2]
            i += 2
            continue
        if c == sep:
            out.append(cur)
            cur = ""
        else:
            cur += c
        i += 1
    out.append(cur)
    return out


def _unescape_path(p: str) -> str:
    return p.replace("$ ", " ").replace("$:", ":").replace("$$", "$")


def parse_ninja(path: Path):
    """(globals, rules, edges); edge = {outs, rule, ins, vars}."""
    globals_, rules, edges = {}, {}, []
    cur = None
    for line in _logical_lines(path.read_text()):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if line[0] in " \t":
            if cur is not None and "=" in line:
                k, v = line.strip().split("=", 1)
                cur[k.strip()] = v.strip()
            continue
        if line.startswith("rule "):
            cur = rules.setdefault(line[5:].strip(), {})
        elif line.startswith("build "):
            head, tail = _split_unescaped(line[6:], ":")[0], ":".join(_split_unescaped(line[6:], ":")[1:])
            parts = tail.split()
            rule, rest = parts[0], " ".join(parts[1:])
            explicit = rest.split("|")[0].split()
            cur = {}
            edges.append({"outs": [_unescape_path(o) for o in head.split()],
                          "rule": rule,
                          "ins": [_unescape_path(i) for i in explicit],
                          "vars": cur})
        elif "=" in line:
            k, v = line.split("=", 1)
            globals_[k.strip()] = v.strip()
            cur = None
        else:
            cur = None
    return globals_, rules, edges


_VAR = re.compile(r"\$(\{([A-Za-z0-9_.-]+)\}|([A-Za-z0-9_-]+)|\$|:| |\n)")


def expand(value: str, scope: dict) -> str:
    def sub(m):
        tok = m.group(1)
        if tok in ("$", ":", " "):
            return tok
        name = m.group(2) or m.group(3)
        return scope.get(name, "")
    return _VAR.sub(sub, value)


# build.ninja is ~1.7 MB for main: parsing it was ~45% of a try_function call.
# Parsed once per process per (file, mtime, size); every caller treats the
# returned dicts as read-only.
_NINJA: dict = {}
_EDGE: dict = {}


def _ninja(unit_dir: Path):
    p = unit_dir / "build.ninja"
    st = p.stat()
    sig = (st.st_mtime_ns, st.st_size)
    hit = _NINJA.get(p)
    if hit is None or hit[0] != sig:
        hit = (sig, parse_ninja(p))
        _NINJA[p] = hit
        for k in [k for k in _EDGE if k[0] == str(unit_dir)]:
            del _EDGE[k]
    return hit[1]


def compile_edge(unit: str, tu: str):
    unit_dir = ROOT / "build" / unit
    g, rules, edges = _ninja(unit_dir)
    ck = (str(unit_dir), tu)
    if ck in _EDGE:
        return _EDGE[ck]
    want = (ROOT / "src" / unit / f"{tu}.c").resolve()
    for e in edges:
        if e["rule"] in rules and len(e["ins"]) == 1 and (unit_dir / e["ins"][0]).resolve() == want:
            _EDGE[ck] = (unit_dir, g, rules[e["rule"]], e)
            return _EDGE[ck]
    raise TryError(f"no compile edge for src/{unit}/{tu}.c in {unit_dir / 'build.ninja'}")


_CC_TU_ALT: dict = {}


def _cc_tu_with_flags(flags: str) -> str | None:
    """Path of a /tmp copy of tools/tu/cc_tu.sh whose cc1 step (`-S -O2`)
    also passes FLAGS (diagnostic compiles of overlay TUs; cached per flags +
    script text). None when the script has no recognizable cc1 step."""
    src = ROOT / "tools/tu/cc_tu.sh"
    text = src.read_text()
    key = (flags, text)
    p = _CC_TU_ALT.get(key)
    if p and os.path.isfile(p):
        return p
    needle = '-nostdinc -fno-builtin -S -O2 "$G"'
    if text.count(needle) != 1 or not re.fullmatch(r"[-\w=. ]+", flags):
        return None
    import hashlib
    h = hashlib.sha1(("%s\0%s" % key).encode()).hexdigest()[:12]
    d = Path("/tmp/try_fn_cc_tu")
    d.mkdir(parents=True, exist_ok=True)
    p = str(d / f"cc_tu.{h}.sh")
    if not os.path.isfile(p):
        tmp = p + f".{os.getpid()}"
        Path(tmp).write_text(text.replace(needle, f'-nostdinc -fno-builtin -S -O2 "$G" {flags}'))
        os.chmod(tmp, 0o755)
        os.replace(tmp, p)
    _CC_TU_ALT[key] = p
    return p


_LINK_ALPHA = "abcdefghijklmnopqrstuvwxyz0123456789"


def _sweep_links(tdir: Path) -> None:
    """Drop dangling scratch links older than an hour (a killed compile)."""
    import time as _t
    now = _t.time()
    try:
        for e in os.scandir(tdir):
            try:
                if e.is_symlink() and not os.path.exists(e.path) and now - e.stat(follow_symlinks=False).st_mtime > 3600:
                    os.unlink(e.path)
            except OSError:
                pass
    except OSError:
        pass


def same_len_link(unit_dir: Path, target: Path, length: int) -> str | None:
    """A path relative to UNIT_DIR of exactly LENGTH characters, `t/<random>`,
    made a symlink to the directory TARGET; None when LENGTH is too short.

    ee-gcc 2.96's code can depend on the lengths of the file names cc1 is
    given (they shift its heap and CSE hashes heap addresses; measured
    2026-10-19: ov10 card_fread compiles a jump table as `lw $3,$L($2)` or as
    lui/addiu/addu depending only on the .i path length). Scratch compiles
    therefore name the source and the outputs with paths as long as the real
    build's (dds-decomp tools/cc.sh does the same)."""
    import random
    n = length - 2
    if n < 4:
        return None
    tdir = unit_dir / "t"
    tdir.mkdir(exist_ok=True)
    if random.random() < 0.01:
        _sweep_links(tdir)
    tgt = str(Path(target).resolve())
    for _ in range(200):
        rel = "t/" + "".join(random.choice(_LINK_ALPHA) for _ in range(n))
        try:
            os.symlink(tgt, unit_dir / rel)
            return rel
        except FileExistsError:
            continue
    return None


def run_compile_samelen(unit_dir: Path, g: dict, rule: dict, edge: dict, tu: str, src: Path,
                        out_rel: str, overlay_rel: str, extra_cflags: str = "") -> tuple[bool, str]:
    """run_compile with SRC (UNIT_DIR/.../<tu>.c) and OUT_REL (.../<tu>.o) seen
    by the compiler through same_len_link paths exactly as long as the build
    edge's own $in / $out; the files stay where SRC / OUT_REL say. Falls back
    to the plain paths when the edge's names cannot be mirrored."""
    real_in, real_out = edge["ins"][0], edge["outs"][0]
    links: list[str] = []
    try:
        if (os.path.basename(real_in) == src.name and os.path.basename(real_out) == os.path.basename(out_rel)
                and os.path.basename(out_rel) == f"{tu}.o"):
            li = same_len_link(unit_dir, src.parent, len(os.path.dirname(real_in)))
            if li:
                links.append(li)
            lo = same_len_link(unit_dir, (unit_dir / out_rel).parent, len(os.path.dirname(real_out)))
            if lo:
                links.append(lo)
            if li and lo:
                return run_compile(unit_dir, g, rule, edge, Path(f"{li}/{src.name}"),
                                   f"{lo}/{os.path.basename(out_rel)}", overlay_rel, extra_cflags)
        return run_compile(unit_dir, g, rule, edge, src, out_rel, overlay_rel, extra_cflags)
    finally:
        for li in links:
            try:
                os.unlink(unit_dir / li)
            except OSError:
                pass


def run_compile(unit_dir: Path, g: dict, rule: dict, edge: dict, src: Path, out_rel: str,
                overlay_rel: str, extra_cflags: str = "") -> tuple[bool, str]:
    scope = dict(g)
    scope.update({k: expand(v, g) for k, v in edge["vars"].items()})
    if "overlay" in scope:
        scope["overlay"] = overlay_rel   # main: cinc.py writes the filtered asm here
    scope["in"] = str(src)
    scope["out"] = out_rel
    cmd = expand(rule["command"], scope)
    if extra_cflags:           # diagnostics only (e.g. -fno-sched-interblock): the cc1 step
        if "-nostdinc -fno-builtin -S " in cmd:
            cmd = cmd.replace("-nostdinc -fno-builtin -S ", f"-nostdinc -fno-builtin -S {extra_cflags} ")
        elif "/tools/tu/cc_tu.sh " in cmd:
            # fold13 (o537): overlay TUs compile through tools/tu/cc_tu.sh; use a
            # /tmp copy whose cc1 step carries the extra flags (repo file untouched)
            alt = _cc_tu_with_flags(extra_cflags)
            if alt is None:
                return False, "extra_cflags: cc_tu.sh cc1 step not found"
            cmd = re.sub(r"\S*/tools/tu/cc_tu\.sh ", alt + " ", cmd, count=1)
        else:
            return False, "extra_cflags: no inline cc1 step in this compile command"
    proc = subprocess.run(["/bin/sh", "-c", cmd], cwd=unit_dir, capture_output=True, text=True)
    ok = proc.returncode == 0 and (unit_dir / out_rel).is_file()
    return ok, (proc.stdout + proc.stderr)


# ------------------------------------------------------------------ elf ----

def _relocs(elf: Elf, text_idx: int):
    out = []
    for s in elf.sections:
        if s.type not in (9, 4) or s.info != text_idx:   # SHT_REL / SHT_RELA
            continue
        ent = 8 if s.type == 9 else 12
        syms = {sym.index: sym for sym in elf.symbols}
        for i in range(s.size // ent):
            off, info = struct.unpack_from("<II", elf.data, s.offset + i * ent)
            sym = syms.get(info >> 8)
            name = sym.name if sym else ""
            if sym and not name and sym.shndx < len(elf.sections):
                name = elf.sections[sym.shndx].name
            out.append((off, info & 0xFF, name))
    return out


def function_slice(obj: Path, name: str) -> dict:
    elf = Elf(obj.read_bytes())
    cands = [s for s in elf.symbols if s.name == name and 0 < s.shndx < len(elf.sections)]
    if not cands:
        alt = re.compile(re.escape(name) + r"_[0-9A-Fa-f]{8}$")
        cands = [s for s in elf.symbols if alt.match(s.name) and 0 < s.shndx < len(elf.sections)]
    if not cands:
        m = re.match(r"(.+)_[0-9A-Fa-f]{8}$", name)
        if m:
            cands = [s for s in elf.symbols if s.name == m.group(1) and 0 < s.shndx < len(elf.sections)]
    if not cands:
        raise TryError(f"symbol {name} not in {obj.name}")
    sym = cands[0]
    sec = elf.sections[sym.shndx]
    start = sym.value
    if sym.size:
        end = start + sym.size
    else:
        nexts = sorted(s.value for s in elf.symbols
                       if s.shndx == sym.shndx and s.value > start and s.name
                       and not s.name.startswith((".L", "$L", "$")) and s.type in (0, 2))
        end = nexts[0] if nexts else sec.size
    data = elf.data[sec.offset + start:sec.offset + end]
    while len(data) >= 4 and data[-4:] == b"\0\0\0\0":   # alignment padding
        data = data[:-4]
    end = start + len(data)
    relocs = [(o - start, t, n) for o, t, n in _relocs(elf, sym.shndx) if start <= o < end]
    data_secs = {s.name: s.size for s in elf.sections
                 if s.name in (".rodata", ".lit4", ".lit8", ".sdata", ".data") and s.size}
    return {"start": start, "end": end, "bytes": data, "relocs": sorted(relocs),
            "section": sec.name, "data_sections": data_secs}


def disasm(obj: Path, sl: dict) -> list[str]:
    if not OBJDUMP.is_file():
        return [f"{i * 4:4x}: {sl['bytes'][i * 4:i * 4 + 4][::-1].hex()}"
                for i in range(len(sl["bytes"]) // 4)]
    # -D: untyped jump-table labels (.L00A03688) otherwise make objdump dump the
    # rest of the function as data words
    proc = subprocess.run([str(OBJDUMP), "-D", "-r", "-j", sl["section"],
                           f"--start-address={sl['start']}", f"--stop-address={sl['end']}",
                           str(obj)], capture_output=True, text=True)
    lines, by_off = [], {}
    ins = re.compile(r"^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(.*)$")
    rel = re.compile(r"^\s*([0-9a-f]+):\s+(R_\S+)\s+(\S+)")
    for raw in proc.stdout.splitlines():
        m = ins.match(raw)
        if m:
            off = int(m.group(1), 16) - sl["start"]
            text = re.sub(r"\s+", " ", m.group(3).split("<")[0]).strip()
            by_off[off] = len(lines)
            lines.append(f"{off:4x}: {text}")
            continue
        m = rel.match(raw)
        if m:
            off = int(m.group(1), 16) - sl["start"]
            if off in by_off:
                lines[by_off[off]] += f"  [{m.group(2).replace('R_MIPS_', '')} {m.group(3)}]"
    return lines


def side_by_side(a: list[str], b: list[str], limit: int) -> list[str]:
    strip = lambda s: s.split(":", 1)[1].strip() if ":" in s else s  # noqa: E731
    sm = difflib.SequenceMatcher(None, [strip(x) for x in a], [strip(x) for x in b], autojunk=False)
    rows = []
    w = max([len(x) for x in a] + [20])
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        n = max(i2 - i1, j2 - j1)
        for k in range(n):
            l = a[i1 + k] if i1 + k < i2 else ""
            r = b[j1 + k] if j1 + k < j2 else ""
            mark = " " if op == "equal" else ("|" if l and r else ("<" if l else ">"))
            rows.append(f"{l:<{w}} {mark} {r}")
    if len(rows) > limit:
        rows = rows[:limit] + [f"... ({len(rows) - limit} more rows)"]
    return ["TARGET".ljust(w) + "   CANDIDATE"] + rows


# ----------------------------------------------------------------- main ----

def tu_edit(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, "-B", str(ROOT / "tools/tu_edit.py"), *args],
                          capture_output=True, text=True, cwd=ROOT)


def _tu_edit_mod():
    if str(ROOT / "tools") not in sys.path:
        sys.path.insert(0, str(ROOT / "tools"))
    import tu_edit as m
    return m


def _to_c_text(tsrc: Path, name: str, body: Path, unit_dir: Path) -> str | None:
    """tools/tu_edit.py to-c in-process (saves a ~0.13 s interpreter start per
    call); None on any error, so the caller reruns the CLI for its message."""
    try:
        m = _tu_edit_mod()
        out = m.to_c(m.load(str(tsrc), None, str(unit_dir)), name, m._read(str(body))).text
        out.encode("utf-8")              # surrogate-escaped bytes: let the CLI path handle them
        return out
    except Exception:  # noqa: BLE001
        return None


# Base objects (the TU with NAME as INCLUDE_ASM) are the same for every
# candidate of a function: trick_sweep evaluates hundreds per body and
# permute_fn re-checks every permuter improvement. Cached per process, keyed by
# the TU text, overrides, compile command and the newest header mtime; entries
# expire after _BASE_TTL seconds (asm/data files a carve rewrites are not hashed).
_BASE: dict = {}
_BASE_TTL = 600.0


def _header_sig(unit: str) -> int:
    best = 0
    for d in (ROOT / "src" / unit, ROOT / "include"):
        try:
            for dp, _dn, fn in os.walk(d):
                for f in fn:
                    if f.endswith((".h", ".inc")):
                        try:
                            best = max(best, os.stat(os.path.join(dp, f)).st_mtime_ns)
                        except OSError:
                            pass
        except OSError:
            pass
    return best


def make_src_dir(scratch: Path, unit: str, tu: str, text: str, overrides: dict | None = None) -> Path:
    """A src/<unit> look-alike: symlinks to every sibling, own copy of the TU,
    so quoted includes resolve exactly as for the real file. OVERRIDES
    {file name: text} replaces siblings (e.g. a header with a flipped
    prototype)."""
    real = ROOT / "src" / unit
    d = scratch / "src"
    d.mkdir(parents=True, exist_ok=True)
    for p in real.iterdir():
        if p.name != f"{tu}.c":
            if overrides and p.name in overrides:
                (d / p.name).write_text(overrides[p.name])
            else:
                (d / p.name).symlink_to(p.resolve())
    (d / f"{tu}.c").write_text(text)
    return d / f"{tu}.c"


_DUP_MSG = re.compile(r"^[^:\n]*\.c:(\d+): (?:redefinition of|conflicting types for|redeclaration of|"
                      r"`?\w*'? ?redeclared as different kind of symbol|static declaration for) `(\w+)'", re.M)
_PREV_MSG = re.compile(r"^[^:\n]*\.c:(\d+): previous (?:declaration|definition) of `(\w+)'", re.M)


def decl_collisions(log: str, cand_text: str, body: Path, name: str) -> list[dict]:
    """Candidate compile errors where a declaration inside the body clashes
    with one the TU makes outside it (o482 EW_setDrawEnv: a seed's typedef /
    prototype named like the TU's): [{name, body_line, tu_line, tu_decl}]."""
    try:
        btxt = body.read_text(errors="replace").strip()
    except OSError:
        return []
    lines = cand_text.split("\n")
    blines = btxt.split("\n")
    # anchor on the function's definition line (tu_edit may rewrite / dedupe prelude lines)
    hb = next((i for i, ln in enumerate(blines) if re.search(r"\b" + re.escape(name) + r"\s*\(", ln)
               and not ln.rstrip().endswith(";")), None)
    hc = next((i for i, ln in enumerate(lines) if hb is not None and ln.strip() == blines[hb].strip()), None)
    if hb is None or hc is None:
        return []
    b0 = hc + 1 - hb
    b1 = hc + 1 + (len(blines) - hb)
    prev = {(m.group(2), int(m.group(1))) for m in _PREV_MSG.finditer(log)}
    out, seen = [], set()
    for m in _DUP_MSG.finditer(log):
        ln, nm = int(m.group(1)), m.group(2)
        for pn, pl in prev:
            if pn != nm:
                continue
            inb, inp = b0 <= ln <= b1, b0 <= pl <= b1
            if inb == inp or (nm, pl) in seen:
                continue
            bl, tl = (ln, pl) if inb else (pl, ln)
            seen.add((nm, pl))
            out.append({"name": nm, "body_line": bl - b0 + 1, "tu_line": tl,
                        "tu_decl": lines[tl - 1].strip()[:160] if 0 < tl <= len(lines) else ""})
    return out


def try_function(unit: str, tu: str, name: str, body: Path, keep: bool = False,
                 diff_limit: int = 200, overrides: dict | None = None, cand_cflags: str = "",
                 uninit: bool = False) -> dict:
    """OVERRIDES {file name in src/<unit>: text} evaluates against patched
    sources (the TU itself under "<tu>.c"): e.g. trick_sweep's void_ret family
    with a flipped forward declaration. Nothing under src/ is written.
    UNINIT (fold11): on a non-match also recompile the candidate .i with
    -Wuninitialized and add an `uninitialized` suspect reason per local gcc
    says might be used uninitialized in NAME; sets report["suspect_v"]."""
    src = ROOT / "src" / unit / f"{tu}.c"
    if not src.is_file():
        raise TryError(f"missing {src}")
    unit_dir, g, rule, edge = compile_edge(unit, tu)
    scratch_rel = Path("try") / f"{tu}.{name}.{os.getpid()}"
    scratch = unit_dir / scratch_rel
    shutil.rmtree(scratch, ignore_errors=True)
    scratch.mkdir(parents=True)
    report: dict = {"key": f"{unit}/{tu}/{name}", "scratch": str(scratch)}
    try:
        if overrides is None and auto_patch_path(unit, tu, name).is_file():
            try:                                # fold11: <name>.patch.json next to the body
                overrides = load_src_patch(unit, auto_patch_path(unit, tu, name), tolerant=True)[1] or None
                report["src_patch"] = str(auto_patch_path(unit, tu, name))
            except TryError as e:
                report["src_patch_error"] = str(e)
        cur = (overrides or {}).get(f"{tu}.c") or src.read_text()
        tsrc = src
        if overrides and f"{tu}.c" in overrides:
            tsrc = scratch / f"{tu}.c"
            tsrc.write_text(cur)
        import hashlib
        import time as _time
        import os as _os
        bkey = hashlib.sha1(json.dumps([unit, tu, name, cur, overrides or {}, rule.get("command"),
                                        edge.get("vars"), _header_sig(unit),
                                        _os.path.getmtime(str(body)) if body.is_file() else 0], sort_keys=True,
                                       default=str).encode("utf-8", "surrogateescape")).hexdigest()
        hit = _BASE.get(bkey)
        if hit and _time.time() - hit[0] > _BASE_TTL:
            hit = None
        base_text = cur
        if hit is None:
            # base: NAME as INCLUDE_ASM (revert it if the current TU already has C)
            lst = tu_edit("list", str(tsrc), "--json", "--unit-dir", str(unit_dir))
            try:
                blocks = json.loads(lst.stdout)
                blocks = blocks.get("functions", blocks) if isinstance(blocks, dict) else blocks
                state = next((b.get("state") for b in blocks
                              if isinstance(b, dict) and name in (b.get("name"), b.get("key"))), None)
            except (json.JSONDecodeError, AttributeError):
                state = None
            if state == "c":
                p = tu_edit("to-asm", str(tsrc), name, "--unit-dir", str(unit_dir))
                if p.returncode != 0 and "cannot infer the INCLUDE_ASM folder" in p.stdout + p.stderr:
                    # an all-C TU has no INCLUDE_ASM line to copy the folder from
                    for d in sorted((unit_dir / "asm").glob(f"*/nonmatchings/{tu}")):
                        if (d / f"{name}.s").is_file():
                            p = tu_edit("to-asm", str(tsrc), name, "--unit-dir", str(unit_dir),
                                        "--asm-dir", str(d.relative_to(unit_dir)))
                            break
                if p.returncode != 0:
                    raise TryError(f"tu_edit to-asm failed: {(p.stdout + p.stderr)[-1500:]}")
                base_text = p.stdout
        cand_text = _to_c_text(tsrc, name, body, unit_dir)
        if cand_text is None:
            p = tu_edit("to-c", str(tsrc), name, "--body", str(body), "--unit-dir", str(unit_dir))
            if p.returncode != 0:
                report.update(ok=False, match=False, stage="tu_edit",
                              error=(p.stdout + p.stderr)[-3000:])
                return report
            cand_text = p.stdout

        results = {}
        for tag, text in (("base", base_text), ("cand", cand_text)):
            tdir = scratch / tag
            out_rel = str(scratch_rel / tag / f"{tu}.o")
            if tag == "base" and hit is not None:
                tdir.mkdir(parents=True, exist_ok=True)
                (unit_dir / out_rel).write_bytes(hit[1])
                results[tag] = unit_dir / out_rel
                continue
            src_copy = make_src_dir(tdir, unit, tu, text, overrides)
            ok, log = run_compile_samelen(unit_dir, g, rule, edge, tu, src_copy, out_rel,
                                          str(scratch_rel / tag / "cinc"),
                                          cand_cflags if tag == "cand" else "")
            if not ok and tag == "base" and base_text != cur:
                # NAME is already C in the TU and reverting it to INCLUDE_ASM made the
                # TU fail under the build's name lengths (ov11 mini_g SetTest: the
                # PoBonusCardAnime ICE is heap-layout dependent). Base only supplies
                # NAME's assembled bytes: compile it under the plain scratch names.
                ok, log2 = run_compile(unit_dir, g, rule, edge, src_copy, out_rel,
                                       str(scratch_rel / tag / "cinc"))
                if ok:
                    report["base_path_fallback"] = True
                else:
                    log = log2
            if not ok:
                # 2026-10-19: the FIRST error is the cause; later ones cascade (a missing
                # typedef made taskUmnTopWin's function look closed early, and only the tail
                # was reported). Keep the head of the log and the first few error lines.
                errl = [l for l in log.splitlines() if ": " in l and "warning:" not in l]
                report.update(ok=False, match=False, stage=f"compile_{tag}",
                              first_errors=errl[:8],
                              error=log if len(log) <= 3000 else log[:1500] + "\n...\n" + log[-1500:])
                if tag == "cand":
                    col = decl_collisions(log, text, body, name)
                    if col:
                        report["decl_collisions"] = col
                        report["hint"] = ("the body re-declares names the TU already declares (o482 "
                                          "EW_setDrawEnv): drop the body's copy when it is the same, or "
                                          "rename the body-local typedef/struct everywhere in the body")
                return report
            results[tag] = unit_dir / out_rel
            if tag == "base":
                if len(_BASE) > 64:
                    _BASE.clear()
                _BASE[bkey] = (_time.time(), (unit_dir / out_rel).read_bytes())

        base = function_slice(results["base"], name)
        cand = function_slice(results["cand"], name)
        try:
            n_jal = resolve_local_jals(results["base"], base, results["cand"], cand)
            if n_jal:
                report["jal_resolved"] = n_jal
        except Exception:  # noqa: BLE001 - comparison falls back to raw words
            pass
        bytes_match = base["bytes"] == cand["bytes"]
        relocs_match = [(o, t) for o, t, _ in base["relocs"]] == [(o, t) for o, t, _ in cand["relocs"]]
        sym_diffs = [{"off": hex(a[0]), "target": a[2], "candidate": b[2]}
                     for a, b in zip(base["relocs"], cand["relocs"])
                     if a[:2] == b[:2] and a[2] != b[2]]
        # A relocation naming a different known symbol at a different address is
        # wrong code, not an alias (o-2026-10-13 dataUnitFileLoad: leaderCid vs the
        # adjacent leaderId scored 1.0; only the whole-file gate caught it).
        _sa = symbol_addresses(unit)
        # Only when the instruction bytes are identical: then the %lo/%hi addends
        # are equal too, so a different symbol address really is a different
        # object. With differing bytes, sym+addend can still alias (MenuAgws:
        # MenuWork/PadData sharing one %hi) -- don't flag those.
        wrong_syms = [d for d in sym_diffs
                      if bytes_match and d["target"] in _sa and d["candidate"] in _sa
                      and _sa[d["target"]] != _sa[d["candidate"]]]
        words = lambda b: [b[i:i + 4] for i in range(0, len(b), 4)]  # noqa: E731
        wa, wb = words(base["bytes"]), words(cand["bytes"])
        n = len(wa)
        sm = difflib.SequenceMatcher(None, wa, wb, autojunk=False)
        diff_words = max(n, len(wb)) - sum(b.size for b in sm.get_matching_blocks())
        # Words that differ only because the candidate carries a HI16/LO16
        # relocation where the target has an absolute address (scaffold asm of
        # an overlay addressing a main symbol by number): only the linked image
        # can tell, so flag them for integrate's whole-file gate.
        cand_rel_offs = {o for o, t, _ in cand["relocs"] if t in (5, 6)}   # R_MIPS_HI16/LO16
        base_rel_offs = {o for o, _, _ in base["relocs"]}
        link_check = False
        match_pending_jtbl = False
        if not bytes_match and len(wa) == len(wb):
            diff_offs = [i * 4 for i in range(len(wa)) if wa[i] != wb[i]]
            def same_op(i):
                a = int.from_bytes(wa[i], "little"); b = int.from_bytes(wb[i], "little")
                return (a >> 16) == (b >> 16)          # opcode + rs + rt equal; imm differs
            # Also: both sides relocate the slot but against different symbols
            # (an address alias like D_00942228 vs asPacketSource+0x10) — the
            # REL addend in the immediate differs, the linked address may not.
            alias_offs = {int(d["off"], 16) for d in sym_diffs}
            link_check = bool(diff_offs) and all(
                ((o in cand_rel_offs and o not in base_rel_offs) or o in alias_offs) and same_op(o // 4)
                for o in diff_offs)
        # Jump-table lookups: target `lw at,%lo(jtbl_XXXXXXXX)(at)` (addend 0) vs
        # ours `lw at,%lo(.rodata+N)` -- only the REL addend differs; integrate's
        # data carve puts the compiled table at jtbl_XXXXXXXX (nmlModelDirectSend).
        jt_syms = {int(d["off"], 16): d["target"] for d in sym_diffs}
        match_pending_jtbl = bool(link_check and relocs_match and diff_offs
                                  and all(str(jt_syms.get(o, "")).startswith("jtbl_") for o in diff_offs))
        # fold15 (o578 cmdThinksetSub ov01, o581 _GetStageData ov12, o540
        # cmdUnitparaSub): target `%lo(D_XXXXXXXX)` / `%lo(jtbl_XXXXXXXX)`, ours
        # `%lo(.rodata+N)`: when every such LO16 slot maps N onto the address in
        # the symbol name with ONE delta (our .rodata lands at the original
        # addresses in order), it is a match once integrate's data carve runs.
        rodata_carve = None
        if link_check and relocs_match and diff_offs:
            ctyp = {o: (t, n) for o, t, n in cand["relocs"]}
            deltas, ok_rc = set(), True
            for o in diff_offs:
                tsym, (rt, csym) = str(jt_syms.get(o, "")), ctyp.get(o, (None, ""))
                mm = re.fullmatch(r"(?:jtbl|D)_([0-9A-Fa-f]{8})", tsym)
                if not mm or csym != ".rodata" or rt not in (5, 6):
                    ok_rc = False
                    break
                if rt == 6:                    # LO16: low halves of sym+imm vs .rodata+imm
                    ti = int.from_bytes(wa[o // 4], "little") & 0xFFFF
                    ci = int.from_bytes(wb[o // 4], "little") & 0xFFFF
                    deltas.add((int(mm.group(1), 16) + ti - ci) & 0xFFFF)
            if ok_rc and len(deltas) == 1:
                rodata_carve = {"slots": len(diff_offs), "delta_lo16": hex(deltas.pop())}
                match_pending_jtbl = True
        # False %lo relocations (splat symbolized a plain immediate): every
        # differing word is one -> identical after linking.
        # .lit4/.lit8 constants: target GPREL16 -> D_xxx (a symbol of a
        # *.lit4.s / *.lit8.s data file), ours LITERAL -> .lit4 (a float
        # literal). Identical after linking when the carve puts the literal at
        # D_xxx (integrate's whole-file gate checks): match_after_link.
        flo: list[dict] = []
        ab: list[dict] = []
        unresolved: list[int] | None = None
        lit = lit_slots(unit, base, cand) if len(wa) == len(wb) else []
        lset = {int(x["off"], 16) for x in lit}
        match_after_link = False
        if len(wa) == len(wb) and (not bytes_match or (lit and not relocs_match)):
            doffs = [i * 4 for i in range(len(wa)) if wa[i] != wb[i]]
            flo = false_lo16(unit, base, cand, doffs)
            ab = abs_reloc(unit, base, cand, doffs)
            aset = {int(x["off"], 16) for x in ab}
            fset = {int(f["off"], 16) for f in flo} | lset | aset
            others_ok = ([(o, t) for o, t, _ in base["relocs"] if o not in fset]
                         == [(o, t) for o, t, _ in cand["relocs"] if o not in lset | aset])
            match_after_link = bool(flo or lit or ab) and all(o in fset for o in doffs) and others_ok
            unresolved = [o for o in doffs if o not in fset]
            if rodata_carve and not match_after_link:
                # every differing word is a carve-resolved .rodata slot
                rest = [o for o in unresolved if o not in set(diff_offs)]
                if not rest:
                    unresolved, match_after_link = [], True
        report.update(
            ok=True,
            match=bytes_match and relocs_match and not wrong_syms,
            wrong_symbols=wrong_syms[:20],
            match_after_link=match_after_link,
            false_lo16=flo,
            lit_literal=lit,
            abs_reloc=ab,
            link_check=link_check,
            match_pending_jtbl=match_pending_jtbl,
            rodata_carve=rodata_carve,
            bytes_match=bytes_match,
            relocs_match=relocs_match,
            reloc_symbol_diffs=sym_diffs[:20],
            target_insns=n,
            candidate_insns=len(wb),
            differing_words=diff_words,
            # differing words left once false_lo16 / lit_literal / abs_reloc slots
            # (identical after linking) are discounted; None = lengths differ
            unresolved_words=(len(unresolved) if unresolved is not None else None),
            score=(min(round(sm.ratio(), 4), 0.99) if wrong_syms else round(sm.ratio(), 4)),
            new_data_sections={k: v - base["data_sections"].get(k, 0)
                               for k, v in cand["data_sections"].items()
                               if v != base["data_sections"].get(k, 0)},
        )
        if (report["match"] or match_after_link or link_check or match_pending_jtbl):
            try:
                ref, ref_kind = context_reference(unit_dir, edge, src, results["base"])
                try:
                    ref_slice = function_slice(ref, name)
                except TryError:
                    ref, ref_kind, ref_slice = results["base"], "base", base
                report.update(context_check(ref, results["cand"], name, ref_slice))
                report["context_ref"] = ref_kind
            except Exception as e:  # noqa: BLE001 - diagnostic only
                report["context_error"] = str(e)[:200]
        if not report["match"]:
            try:
                report["gate"] = semantic_gate(unit, base, cand)
            except Exception as e:  # noqa: BLE001 - diagnostic only
                report["gate_error"] = str(e)[:200]
            da, db = disasm(results["base"], base), disasm(results["cand"], cand)
            # 2026-10-19 semantics gate: what the body computes vs the target (constants,
            # float literals, calls + constant args, loads across calls, conversions); a
            # failing body still gets its score but is never saved as <name>.best.c
            if not (match_after_link or link_check or match_pending_jtbl):
                try:
                    report["semantics"] = semantics_check(unit, tu, results["base"], base, results["cand"],
                                                          cand, da, db)
                    if report["semantics"].get("ok"):
                        _beh_stash(unit, tu, name, body, results["base"], results["cand"])
                except Exception as e:  # noqa: BLE001 - diagnostic only, never blocks
                    report["semantics_error"] = str(e)[:200]
            # Opcode-only similarity: a register-allocation miss scores high here
            # while the strict word score drops to ~0 (every word differs).
            op = lambda ls: [l.split(":", 1)[1].split()[0] if ":" in l and l.split(":", 1)[1].split() else ""
                             for l in ls]  # noqa: E731
            report["opcode_score"] = round(difflib.SequenceMatcher(None, op(da), op(db), autojunk=False).ratio(), 4)
            report["diff"] = side_by_side(da, db, diff_limit)
            try:
                sus = semantic_suspect(da, db, starts=(base.get("start"), cand.get("start")))
            except Exception:  # noqa: BLE001 - diagnostic only
                sus = []
            if uninit:
                try:
                    cw = cand_warnings(unit, tu, name, scratch)
                    sus = sus + [f"uninitialized: `{v}` might be used uninitialized (gcc -Wuninitialized; "
                                 "a store/arg the seed lost?)" for v in cw["uninit"]]
                    sus = sus + [f"shadowed param: a local `{v}` shadows the parameter (the body reads the "
                                 "local, not the argument)" for v in cw["shadow"]]
                    # fold14 (o564 SEQ_moveUnitChr): callees used without a prototype
                    imp = implicit_callees(cw, name, body.read_text(errors="replace"))
                    if imp:
                        report["implicit"] = imp
                        for f in imp:
                            pr = implicit_semantic(f)
                            if pr:
                                sus.append(f"implicit decl: `{f}` is called without a prototype (gcc assumes "
                                           f"int return, promotes float args to double); real: `{pr}` -- "
                                           "add that prototype")
                    report["suspect_v"] = SUSPECT_V
                except Exception:  # noqa: BLE001 - diagnostic only
                    pass
            if sus:
                report["suspect"] = sus
            try:                                # fold15: narrow-param call-cast hint
                nc = narrow_arg_calls(da, db, (text_syms(results["base"], base["section"]),
                                               text_syms(results["cand"], cand["section"])))
                if nc:
                    report["narrow_arg_calls"] = nc
            except Exception:  # noqa: BLE001 - diagnostic only
                pass
        try:
            if static_fix(unit, tu, name, body.read_text(errors="replace")):
                report["binding_fix"] = ("the original symbol is " + (asm_binding(unit, tu, name) or "?")
                                         + ": integrate.py flips `static` on the definition automatically "
                                         "(mapcheck bind gate); nothing to do by hand")
        except OSError:
            pass
        if sym_diffs and report["bytes_match"]:
            report["note"] = ("bytes/relocation types match but some relocations name different "
                              "symbols; integrate's whole-file gate decides (usually fine when "
                              "the names alias the same address)")
        elif report.get("match_pending_jtbl"):
            report["note"] = ("MATCH PENDING: the only differing words are jump-table lookups "
                              "(jtbl_* vs .rodata+N addend); opcode/relocation types are identical. "
                              "Treat as a match: run integrate.py --skip-try (its carve + whole-file gate decide)")
        elif link_check:
            report["note"] = ("every differing word is an absolute-vs-symbol / aliased relocation slot: "
                              "run integrate.py --skip-try (its whole-file gate decides)")
        elif sym_diffs:
            report["note"] = ("some relocations name different symbols (jtbl_*/D_* vs .rodata: "
                              "integrate's data carve handles those), but the code itself still "
                              "differs in differing_words places: fix those first")
        return report
    finally:
        if not keep:
            shutil.rmtree(scratch, ignore_errors=True)


def _text_names(obj: Path, section: str) -> dict:
    elf = Elf(obj.read_bytes())
    idx = next((s.index for s in elf.sections if s.name == section), None)
    out: dict = {}
    for s in elf.symbols:
        if s.shndx == idx and s.name and not s.name.startswith((".", "$")) and s.type in (0, 2):
            out.setdefault(s.value, s.name)
    return out


def resolve_local_jals(base_obj: Path, base: dict, cand_obj: Path, cand: dict) -> int:
    """`jal` to a function of the same TU is relocated against the section
    (R_MIPS_26 .text, target in the word): its bits depend on the size of
    every function before the target, not on NAME. Where base and candidate
    words differ only there and both targets are the same named function,
    clear the target bits in both slices (the linked words are equal).
    Returns the number of words so resolved."""
    rb = {o: (t, n) for o, t, n in base["relocs"]}
    rc = {o: (t, n) for o, t, n in cand["relocs"]}
    names_b = names_c = None
    bb, cb = bytearray(base["bytes"]), bytearray(cand["bytes"])
    n = 0
    for o, (t, nm) in rb.items():
        if t != 4 or not nm.startswith(".") or rc.get(o) != (t, nm) or o + 4 > min(len(bb), len(cb)):
            continue
        wb = int.from_bytes(bb[o:o + 4], "little")
        wc = int.from_bytes(cb[o:o + 4], "little")
        if wb == wc or (wb >> 26) != (wc >> 26):
            continue
        if names_b is None:
            names_b, names_c = _text_names(base_obj, nm), _text_names(cand_obj, nm)
        tb, tc = names_b.get((wb & 0x3FFFFFF) << 2), names_c.get((wc & 0x3FFFFFF) << 2)
        if tb and tb == tc:
            bb[o:o + 4] = (wb & ~0x3FFFFFF).to_bytes(4, "little")
            cb[o:o + 4] = (wc & ~0x3FFFFFF).to_bytes(4, "little")
            n += 1
    if n:
        base["bytes"], cand["bytes"] = bytes(bb), bytes(cb)
    return n


def _code_table(elf: Elf) -> dict:
    """{symbol: (section, start, bytes, [(off, reloc type, reloc symbol)])} for
    every named symbol of an executable section (functions and glabels)."""
    syms = {s.index: s for s in elf.symbols}
    rels: dict = {}
    for s in elf.sections:
        if s.type not in (9, 4) or not (0 < s.info < len(elf.sections)):
            continue
        ent = 8 if s.type == 9 else 12
        lst = rels.setdefault(s.info, [])
        for i in range(s.size // ent):
            off, info = struct.unpack_from("<II", elf.data, s.offset + i * ent)
            sym = syms.get(info >> 8)
            nm = sym.name if sym else ""
            if sym and not nm and sym.shndx < len(elf.sections):
                nm = elf.sections[sym.shndx].name
            lst.append((off, info & 0xFF, nm))
    by_sec: dict = {}
    for s in elf.symbols:
        if (not s.name or s.name.startswith((".", "$")) or s.type not in (0, 2)
                or not 0 < s.shndx < len(elf.sections)):
            continue
        sec = elf.sections[s.shndx]
        if sec.type == 8 or not sec.flags & 4:            # SHF_EXECINSTR
            continue
        by_sec.setdefault(s.shndx, []).append(s)
    out = {}
    for idx, lst in by_sec.items():
        sec = elf.sections[idx]
        starts = sorted({x.value for x in lst})
        for s in lst:
            if s.size:
                end = s.value + s.size
            else:
                nxt = [v for v in starts if v > s.value]
                end = nxt[0] if nxt else sec.size
            data = elf.data[sec.offset + s.value:sec.offset + end]
            while len(data) >= 4 and data[-4:] == b"\0\0\0\0":
                data = data[:-4]
            rl = sorted((o - s.value, t, n) for o, t, n in rels.get(idx, []) if s.value <= o < s.value + len(data))
            out[s.name] = (sec.name, s.value, data, rl)
    return out


def context_reference(unit_dir: Path, edge: dict, src: Path, base_obj: Path) -> tuple[Path, str]:
    """The object the other functions are compared against: the unit's own
    build object of the TU (identical to the original while the unit is green;
    NAME there may be C or INCLUDE_ASM) when it is at least as new as the TU
    source, else try's base object (NAME as INCLUDE_ASM). Reverting an
    integrated function to INCLUDE_ASM can itself change a neighbour (ov10
    CardPlayShuffleYama -> CardPlayCursorMove), so base is only the fallback."""
    obj = unit_dir / edge["outs"][0]
    try:
        if obj.stat().st_mtime >= src.stat().st_mtime:
            data = obj.read_bytes()
            Elf(data)
            ref = base_obj.with_name(base_obj.name + ".build_ref.o")
            ref.write_bytes(data)                # a private copy: the build may rewrite it
            return ref, "build"
    except (OSError, ValueError):
        pass
    return base_obj, "base"


def context_check(base_obj: Path, cand_obj: Path, name: str, base_slice: dict) -> dict:
    """CONTEXT check (dds-decomp tools/check_unit.py): with NAME's C in the
    unit, does any OTHER function of the TU compile differently? base is the
    reference object (context_reference: normally the unit's build object of
    the TU), cand the TU with the body,
    both compiled exactly as the build does, so a difference here is what
    integrate's whole-unit gate reports as "a later function changed size" /
    not identical. Differences confined to words relocated against a section
    (.rodata+N addends moved by NAME's own new data) are listed apart as
    context_data_shift (integrate's carve usually absorbs them)."""
    a = _code_table(Elf(base_obj.read_bytes()))
    b = _code_table(Elf(cand_obj.read_bytes()))
    lo, hi, sec = base_slice["start"], base_slice["end"], base_slice["section"]
    alt = re.compile(re.escape(name) + r"(?:_[0-9A-Fa-f]{8})?$")
    def own(n, rec):                       # NAME itself, or a label inside its INCLUDE_ASM block
        return bool(alt.match(n)) or (rec[0] == sec and lo <= rec[1] < hi)
    diffs, shift, missing = [], [], []
    for n in sorted(a):
        if own(n, a[n]):
            continue
        if n not in b:
            missing.append(n)
            continue
        _, _, da, ra = a[n]
        _, _, db, rb = b[n]
        if da == db and ra == rb:
            continue
        if len(da) == len(db) and [r[:2] for r in ra] == [r[:2] for r in rb]:
            secrel = {o for o, _t, nm in ra if nm.startswith(".")}
            dw = [i for i in range(0, len(da), 4) if da[i:i + 4] != db[i:i + 4]]
            if all(o in secrel for o in dw) and [r for r in ra if r[0] not in secrel] == \
                    [r for r in rb if r[0] not in secrel]:
                shift.append(n)
                continue
        nd = sum(1 for i in range(0, min(len(da), len(db)), 4) if da[i:i + 4] != db[i:i + 4])
        diffs.append({"name": n, "base_bytes": len(da), "cand_bytes": len(db), "differing_words": nd})
    ok = not diffs and not missing
    out = {"context_ok": ok, "context": "OK" if ok else "CONTEXT"}
    if diffs:
        out["context_diffs"] = diffs[:12]
    if missing:
        out["context_missing"] = missing[:12]
    if shift:
        out["context_data_shift"] = shift[:12]
    if not ok:
        out["context_note"] = ("CONTEXT: NAME itself matches, but with it in the unit other function(s) "
                               "compile differently (context_diffs / context_missing); integrate's "
                               "whole-unit gate will refuse it. Usual causes: a new or moved extern / "
                               "prototype (keep declarations in the top block, use the TU's existing "
                               "ones), a static helper now inlined or dropped, a type redeclared".replace("NAME", name))
    return out


def compile_file(unit: str, tu: str, c_path: Path, out: Path) -> tuple[bool, str]:
    """Compile an arbitrary (possibly preprocessed) C file with TU's exact
    compile command into `out` (used as the permuter's compile.sh)."""
    unit_dir, g, rule, edge = compile_edge(unit, tu)
    scratch_rel = Path("try") / f"cf.{tu}.{os.getpid()}"
    scratch = unit_dir / scratch_rel
    shutil.rmtree(scratch, ignore_errors=True)
    scratch.mkdir(parents=True)
    try:
        src = scratch / f"{tu}.c"
        shutil.copyfile(c_path, src)
        out_rel = str(scratch_rel / f"{tu}.o")
        ok, log = run_compile_samelen(unit_dir, g, rule, edge, tu, src, out_rel, str(scratch_rel / "cinc"))
        if ok:
            shutil.copyfile(unit_dir / out_rel, out)
        return ok, log
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


_SYMADDR: dict[str, dict[str, int]] = {}
_SYMLINE = re.compile(r"^\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;")


def symbol_addresses(unit: str) -> dict[str, int]:
    """name -> address from config/symbols/<unit>.txt and build/<unit>/undefined_syms_auto.txt."""
    if unit not in _SYMADDR:
        d: dict[str, int] = {}
        for p in (ROOT / "build" / unit / "undefined_syms_auto.txt", ROOT / "config/symbols" / f"{unit}.txt"):
            try:
                for ln in p.read_text(errors="replace").splitlines():
                    m = _SYMLINE.match(ln)
                    if m:
                        d[m.group(1)] = int(m.group(2), 0)
            except OSError:
                pass
        _SYMADDR[unit] = d
    return _SYMADDR[unit]


# ------------------------------------------------ correctness gate (permuter bests) --
# Permuter / sweep "bests" scoring 0.9+ that compute something else (a store
# instead of a subtract, a dropped return value, an extra `state++`, a missing
# field store): the target and the candidate must call the same callees the
# same number of times, store the same number of non-stack words per width and
# reference the same data symbols. Register allocation / scheduling never
# changes these.
_GATE_ST = {0x28: "sb", 0x29: "sh", 0x2B: "sw", 0x3F: "sd", 0x39: "swc1"}


def _gate_facts(unit: str, sl: dict):
    from collections import Counter
    rel = {o: (t, n) for o, t, n in sl["relocs"]}
    calls, stores, data = Counter(), Counter(), set()
    b = sl["bytes"]
    for i in range(len(b) // 4):
        w = int.from_bytes(b[i * 4:i * 4 + 4], "little")
        op = w >> 26
        if op == 3:                                    # jal
            n = rel.get(i * 4, (None, ""))[1]
            calls[n if n and not n.startswith((".", "$")) else "<local>"] += 1
        elif op in _GATE_ST and (w >> 21) & 31 != 29:  # not $sp (spills / frame)
            stores[_GATE_ST[op]] += 1
    lit, sect = lit_symbols(unit), False
    for _, t, n in sl["relocs"]:
        if t not in (5, 6, 7) or not n or n.startswith(("$", "jtbl_")) or n in lit:
            continue
        if n.startswith("."):                          # .rodata/.data: placed by the carve
            sect = True
            continue
        a = sym_address(unit, n)
        if a is None or not 0x70000000 <= a < 0x70004000:   # scratchpad: false %lo immediates
            data.add(a if a is not None else n)
    return calls, stores, data, sect


def semantic_gate(unit: str, base: dict, cand: dict) -> list[str]:
    """Failures (empty = pass) of the correctness gate: CAND vs the target
    slice BASE (function_slice dicts)."""
    tc, ts, td, _ = _gate_facts(unit, base)
    cc, cs, cd, csect = _gate_facts(unit, cand)
    out = []
    if tc != cc:
        d = {k: (tc[k], cc[k]) for k in set(tc) | set(cc) if tc[k] != cc[k]}
        out.append("calls: " + ", ".join(f"{k} target {a}x ours {b}x" for k, (a, b) in sorted(d.items())))
    if ts != cs:
        d = {k: (ts[k], cs[k]) for k in set(ts) | set(cs) if ts[k] != cs[k]}
        out.append("stores: " + ", ".join(f"{k} target {a} ours {b}" for k, (a, b) in sorted(d.items())))
    if csect:                     # ours names a section (carve) where the target names a symbol
        td = td & cd              # target-only symbols may be those section-relative refs
    if td != cd:
        f = lambda x: hex(x) if isinstance(x, int) else x  # noqa: E731
        out.append("data syms: target only " + str(sorted(map(f, td - cd), key=str)[:4])
                   + ", ours only " + str(sorted(map(f, cd - td), key=str)[:4]))
    return out


def semantics_check(unit: str, tu: str, base_obj: Path, base: dict, cand_obj: Path, cand: dict,
                    da: list[str], db: list[str]) -> dict:
    """semgate.check() of CAND vs the target slice BASE: {"ok", "diffs", "v"}."""
    import semgate

    def lits(obj: Path) -> dict:
        elf = Elf(obj.read_bytes())
        return {s.name: elf.data[s.offset:s.offset + s.size] for s in elf.sections
                if s.name in (".lit4", ".lit8") and s.size and s.type != 8}
    ctx = semgate.Ctx(unit, tu, sym_address, lit_symbols, real_proto)
    r = semgate.check(ctx, da, db, base["start"], cand["start"], _text_names(base_obj, base["section"]),
                      _text_names(cand_obj, cand["section"]), lits(base_obj), lits(cand_obj))
    r.pop("notes", None)
    return r


def sym_address(unit: str, name: str) -> int | None:
    a = symbol_addresses(unit).get(name)
    if a is None:
        m = re.fullmatch(r"D_([0-9A-Fa-f]{1,8})", name)
        a = int(m.group(1), 16) if m else None
    return a


def false_lo16(unit: str, base: dict, cand: dict, offs: list[int]) -> list[dict]:
    """Differing words (byte offsets OFFS) where splat symbolized a plain
    immediate: target slot has R_MIPS_LO16 -> sym (addend 0), candidate is the
    same opcode/regs with immediate N == sym & 0xFFFF, no reloc there, and the
    target's feeding `lui` of that register carries NO HI16 reloc and
    lui<<16 + N == sym address. Linked bytes are then identical.

    Also covers false HI16+LO16 pairs (splat rewrote a bare `lui imm_hi` /
    `addiu imm_lo` absolute load as `%hi(D_sym)` / `%lo(D_sym)`): both the
    HI16 lui slots and the LO16 addiu are identical after linking when the
    candidate keeps the absolute immediates (FontTestSub D_1000001)."""
    wa, wb = base["bytes"], cand["bytes"]
    brel = {o: (t, n) for o, t, n in base["relocs"]}
    crel = {o for o, _, _ in cand["relocs"]}
    out = []
    claimed = set()
    sx = lambda v: v - 0x10000 if v & 0x8000 else v
    # Pass 1: false HI16+LO16 pairs (splat symbolized a full absolute address).
    for o in offs:
        if o in claimed or o + 4 > len(wa) or o + 4 > len(wb):
            continue
        r = brel.get(o)
        if not r or r[0] != 6 or o in crel:
            continue
        a = int.from_bytes(wa[o:o + 4], "little")
        b = int.from_bytes(wb[o:o + 4], "little")
        if (a >> 16) != (b >> 16) or (a & 0xFFFF) != 0:
            continue
        addr = sym_address(unit, r[1])
        n = b & 0xFFFF
        if addr is None or n != (addr & 0xFFFF):
            continue
        rs = (a >> 21) & 31
        # Feeding lui must be HI16 to the same sym (the false-pair case), and
        # the candidate's matching lui must bake %hi(addr) with no reloc.
        lui = None
        for q in range(o - 4, -4, -4):
            w = int.from_bytes(wa[q:q + 4], "little")
            if (w >> 26) == 0x0F and ((w >> 16) & 31) == rs:
                lui = (q, w)
                break
        if lui is None:
            continue
        lr = brel.get(lui[0])
        if not lr or lr[0] != 5 or lr[1] != r[1] or lui[0] in crel:
            continue
        if lui[0] + 4 > len(wb):
            continue
        c_lui = int.from_bytes(wb[lui[0]:lui[0] + 4], "little")
        if (c_lui >> 16) != (lui[1] >> 16):
            continue
        want_hi = ((addr + 0x8000) >> 16) & 0xFFFF
        if (lui[1] & 0xFFFF) != 0 or (c_lui & 0xFFFF) != want_hi:
            continue
        if ((want_hi << 16) + sx(n)) & 0xFFFFFFFF != addr & 0xFFFFFFFF:
            continue
        # Claim every differing HI16 lui to this sym that bakes the same %hi,
        # plus this LO16 (FontTestSub has two lui %hi(D_1000001) before the lo).
        for q in offs:
            if q in claimed:
                continue
            rq = brel.get(q)
            if not rq or rq[0] != 5 or rq[1] != r[1] or q in crel:
                continue
            aw = int.from_bytes(wa[q:q + 4], "little")
            bw = int.from_bytes(wb[q:q + 4], "little")
            if (aw >> 26) != 0x0F or (bw >> 26) != 0x0F:
                continue
            if (aw >> 16) != (bw >> 16) or (aw & 0xFFFF) != 0:
                continue
            if (bw & 0xFFFF) != want_hi:
                continue
            out.append({"off": hex(q), "sym": r[1], "imm": want_hi, "kind": "false_hi16"})
            claimed.add(q)
        out.append({"off": hex(o), "sym": r[1], "imm": n, "lui_off": hex(lui[0]),
                    "kind": "false_hilo16"})
        claimed.add(o)
    # Pass 2: classic false LO16 (feeding lui has no HI16 reloc).
    for o in offs:
        if o in claimed or o + 4 > len(wa) or o + 4 > len(wb):
            continue
        a = int.from_bytes(wa[o:o + 4], "little")
        b = int.from_bytes(wb[o:o + 4], "little")
        r = brel.get(o)
        if not r or r[0] != 6 or o in crel or (a >> 16) != (b >> 16) or (a & 0xFFFF) != 0:
            continue
        addr = sym_address(unit, r[1])
        n = b & 0xFFFF
        if addr is None or n != (addr & 0xFFFF):
            continue
        rs = (a >> 21) & 31
        lui = None
        for q in range(o - 4, -4, -4):
            w = int.from_bytes(wa[q:q + 4], "little")
            if (w >> 26) == 0x0F and ((w >> 16) & 31) == rs:
                lui = (q, w)
                break
        if lui is None or lui[0] in brel:
            continue
        sext = n - 0x10000 if n & 0x8000 else n
        if (((lui[1] & 0xFFFF) << 16) + sext) & 0xFFFFFFFF != addr & 0xFFFFFFFF:
            continue
        out.append({"off": hex(o), "sym": r[1], "imm": n, "lui_off": hex(lui[0])})
        claimed.add(o)
    return out


def abs_reloc(unit: str, base: dict, cand: dict, offs: list[int]) -> list[dict]:
    """Differing words (byte offsets OFFS) where the TARGET has a plain
    absolute immediate (no relocation: splat did not symbolize it) and the
    candidate a R_MIPS_HI16 / LO16 against a named symbol of the same value:
    same opcode/regs, target `lui` imm == %hi(sym + addend) or `addiu`/`lw`
    imm == %lo(sym + addend) (addend from the candidate's HI16/LO16 pair,
    symbol address from config/symbols). Identical after linking
    (o418 GameCameraReset: lui 0x36 / addiu 11904 == CfCameraDefine)."""
    wa, wb = base["bytes"], cand["bytes"]
    brel = {o for o, _, _ in base["relocs"]}
    crel = {o: (t, n) for o, t, n in cand["relocs"]}
    w = lambda bs, o: int.from_bytes(bs[o:o + 4], "little")  # noqa: E731
    sx = lambda v: v - 0x10000 if v & 0x8000 else v  # noqa: E731
    out = []
    for o in offs:
        r = crel.get(o)
        if not r or r[0] not in (5, 6) or o in brel or o + 4 > len(wa) or o + 4 > len(wb):
            continue
        a, b = w(wa, o), w(wb, o)
        if (a >> 16) != (b >> 16):
            continue
        addr = sym_address(unit, r[1])
        if addr is None:
            continue
        if r[0] == 5:
            lo = next((q for q in sorted(crel) if q > o and crel[q] == (6, r[1])), None)
            if lo is None:
                continue
            val = addr + ((b & 0xFFFF) << 16) + sx(w(wb, lo) & 0xFFFF)
            want = ((val + 0x8000) >> 16) & 0xFFFF
        else:
            hi = next((q for q in sorted(crel, reverse=True) if q < o and crel[q] == (5, r[1])), None)
            if hi is None:
                continue
            val = addr + ((w(wb, hi) & 0xFFFF) << 16) + sx(b & 0xFFFF)
            want = val & 0xFFFF
        if (a & 0xFFFF) == want:
            out.append({"off": hex(o), "sym": r[1], "type": "HI16" if r[0] == 5 else "LO16",
                        "value": hex(val & 0xFFFFFFFF)})
    return out


_LITSYMS: dict[str, dict] = {}


def lit_symbols(unit: str) -> dict[str, dict]:
    """name -> {"section": ".lit4"/".lit8", "kind": "float"/"double", "bits": int}
    for every symbol of the unit's *.lit4.s / *.lit8.s data files (splat data
    under build/<unit>/asm and carved ones under build/<unit>/carve)."""
    if unit not in _LITSYMS:
        d: dict[str, dict] = {}
        files = []
        for sub in ("asm", "carve"):
            base = ROOT / "build" / unit / sub
            if base.is_dir():
                files += list(base.rglob("*.lit4.s")) + list(base.rglob("*.lit8.s"))
        word = re.compile(r"/\*\s*\S+\s+\S+\s+([0-9A-Fa-f]{8}(?:[0-9A-Fa-f]{8})?)\s*\*/\s*\.(float|double|word)")
        for f in files:
            sec = ".lit8" if f.name.endswith(".lit8.s") else ".lit4"
            cur = None
            for ln in f.read_text(errors="replace").splitlines():
                m = re.match(r"\s*dlabel\s+(\w+)", ln)
                if m:
                    cur = m.group(1)
                    continue
                m = word.search(ln)
                if m and cur:
                    raw = bytes.fromhex(m.group(1))
                    kind = "double" if sec == ".lit8" or m.group(2) == "double" else "float"
                    if kind == "double" and len(raw) == 4:
                        prev = d.get(cur)
                        if prev and prev.get("half") is not None:   # .word pair
                            raw = prev.pop("half") + raw
                        else:
                            d[cur] = {"section": sec, "kind": kind, "half": raw}
                            continue
                    d[cur] = {"section": sec, "kind": kind, "bits": int.from_bytes(raw, "little")}
                    cur = None
        _LITSYMS[unit] = {k: v for k, v in d.items() if "bits" in v}
    return _LITSYMS[unit]


def lit_slots(unit: str, base: dict, cand: dict) -> list[dict]:
    """Offsets where the target relocates GPREL16 against a .lit4/.lit8 data
    symbol and the candidate LITERAL (or GPREL16) against the .lit4/.lit8
    section, same opcode and registers."""
    syms = None
    crel = {o: (t, n) for o, t, n in cand["relocs"]}
    out = []
    for o, t, n in base["relocs"]:
        c = crel.get(o)
        if t != 7 or not c or c[0] not in (7, 8) or c[1] not in (".lit4", ".lit8"):
            continue
        if syms is None:
            syms = lit_symbols(unit)
        if n not in syms and n not in (".lit4", ".lit8"):
            continue
        a = int.from_bytes(base["bytes"][o:o + 4], "little")
        b = int.from_bytes(cand["bytes"][o:o + 4], "little")
        if (a >> 16) != (b >> 16):
            continue
        out.append({"off": hex(o), "sym": n, "imm": b & 0xFFFF})
    return out


TRY_BUDGET = 120


# ------------------------------------------- best-body selection (fold11) ----
# o511 MAP_updateUnitDoor: a correct body (0.39) lost to a permuter .best.c
# (0.43) that read an uninitialized local, dropped a store into dead code after
# `break;` and called DoubleDoorFunc() without its argument. A body the
# suspect detector flags is ranked SUSPECT_MARGIN lower than its score, so a
# non-suspect body within 0.10 below it wins (and a suspect body must beat a
# clean best by more than 0.10). Measured -Wuninitialized false-positive rate
# on matched C: 2 of ~3100 main functions.
SUSPECT_MARGIN = 0.10
SUSPECT_V = 14          # .best.json "suspect_v": suspect computed with the uninit check (12: + fold12 shadow / call-result tells; 13: fold14 implicit decl; 14: fold15 loop pointer / guarded store)


def cc1_cmd(unit: str, tu: str) -> tuple[str, list[str]]:
    """(ee-gcc path, cc1 flags) of the TU's compile edge (as trick_sweep._cc1_cmd)."""
    import os
    ud, g, rule, e = compile_edge(unit, tu)
    sc = dict(g)
    sc.update({a: expand(b, g) for a, b in e["vars"].items()})
    sc.update({"in": "IN.c", "out": "OUT.o", "overlay": "OV"})
    cmd = expand(rule["command"], sc)
    m = re.search(r"cc_tu\.sh\s+\S+\s+\S+\s+(\S+)\s+\S+\s+(-G\d+)", cmd)
    if m:
        if not m.group(1).endswith("/ee-gcc") and os.environ.get("CC1_DEBUG"):
            cmd = re.sub(r"(\S+/)ee-gcc(\s+\S+\.i\s+-o\s+\S+)", r"\1cc1\2", cmd)
            cmd = re.sub(r"\s*-o\s+\S+", " -o /tmp/cc1_debug.o", cmd)
            import subprocess
            p = subprocess.run(["/bin/bash", "-c", "cd /tmp && " + cmd], capture_output=True, text=True, timeout=120)
            import pathlib
            pathlib.Path("/tmp/cc1_debug.log").write_text(p.stdout + "\n===STDERR===\n" + p.stderr)
        return f"{m.group(1)}/ee-gcc", ["-B" + m.group(1) + "/", "-nostdinc", "-fno-builtin", "-O2", m.group(2)]
    m = re.search(r"(\S+/ee-gcc)\s+(-B\S+)\s+-nostdinc\s+-fno-builtin\s+-S\s+(.*?)\s+\S+\.i\s+-o", cmd)
    if m:
        return m.group(1), [m.group(2), "-nostdinc", "-fno-builtin"] + m.group(3).split()
    raise TryError("no cc1 step in compile command")


def cand_warnings(unit: str, tu: str, name: str, scratch: Path) -> dict:
    """{"uninit": [...], "shadow": [...]}: locals of NAME that gcc -O2
    -Wuninitialized says might be used uninitialized, and (fold12, o529
    ACT_loadResource's draft_fix `int arg1;` over the parameter arg1) locals
    -Wshadow says shadow a parameter; from the candidate .i kept in
    SCRATCH/cand (one cc1 run)."""
    ii = sorted((Path(scratch) / "cand").rglob("*.i"))
    out: dict = {"uninit": [], "shadow": []}
    if not ii:
        return out
    gcc, flags = cc1_cmd(unit, tu)
    p = subprocess.run([*NOASLR, gcc, *flags, "-S", "-Wuninitialized", "-Wshadow", "-Wimplicit-function-declaration",
                        str(ii[0]), "-o", "/dev/null"],
                       capture_output=True, text=True, timeout=120)
    out["implicit_all"] = []
    fn = None
    for ln in p.stderr.splitlines():
        m = re.search(r"In function `([^']+)'", ln)
        if m:
            fn = m.group(1)
            continue
        if re.search(r": At top level:", ln):
            fn = None
            continue
        m = re.search(r"warning: `([^']+)' might be used uninitialized", ln)
        if m and fn == name and m.group(1) not in out["uninit"]:
            out["uninit"].append(m.group(1))
        m = re.search(r"warning: declaration of `([^']+)' shadows a parameter", ln)
        if m and fn == name and m.group(1) not in out["shadow"]:
            out["shadow"].append(m.group(1))
        m = re.search(r"warning: implicit declaration of function `([^']+)'", ln)
        if m and fn:
            out["implicit_all"].append((fn, m.group(1)))
    return out


def implicit_callees(cw: dict, name: str, body_text: str = "") -> list[str]:
    """Callees NAME uses without a prototype in scope (fold14, o564): gcc warns
    once per TU, so a callee first used implicitly by an earlier function
    counts when BODY_TEXT calls it too."""
    out = []
    for fn, f in cw.get("implicit_all") or []:
        if f in out:
            continue
        if fn == name or (body_text and re.search(r"(?<![\w.>])" + re.escape(f) + r"\s*\(", body_text)):
            out.append(f)
    return out


_PROTOS: dict | None = None


def real_proto(f: str) -> str | None:
    """protos.json (proto_index.py) prototype of F, or None."""
    global _PROTOS
    if _PROTOS is None:
        try:
            _PROTOS = json.loads((ROOT / "build/auto_decomp/protos.json").read_text())
        except (OSError, ValueError):
            _PROTOS = {}
    return _PROTOS.get(f)


def implicit_semantic(f: str) -> str | None:
    """F's real prototype when calling F implicitly computes something else:
    a float/double/narrow (char/short) return, or float/double parameters
    (promoted to double without a prototype). None for int/void/pointer ones."""
    pr = real_proto(f)
    if not pr:
        return None
    m = re.match(r"^\s*(?:extern\s+)?(?P<r>.*?)\b" + re.escape(f) + r"\s*\((?P<a>.*)\)\s*;?\s*$", pr)
    if not m:
        return None
    r = m.group("r").strip()
    if "*" not in r and re.search(r"\b(float|double|f32|f64|char|short|s8|u8|s16|u16)\b", r):
        return pr.strip()
    for a in m.group("a").split(","):
        if "*" not in a and "(" not in a and re.search(r"\b(float|double|f32|f64)\b", a):
            return pr.strip()
    return None


def uninit_locals(unit: str, tu: str, name: str, scratch: Path) -> list[str]:
    """Locals of NAME that gcc -O2 -Wuninitialized says might be used
    uninitialized, from the candidate .i kept in SCRATCH/cand (one cc1 run)."""
    return cand_warnings(unit, tu, name, scratch)["uninit"]


def best_rank(score, suspect) -> float:
    return (score if score is not None else -1) - (SUSPECT_MARGIN if suspect else 0)


def full_suspect(unit: str, tu: str, name: str, body: Path, report: dict | None = None) -> list[str]:
    """Suspect reasons of BODY incl. the uninit check (REPORT reused when it
    already carries suspect_v; a match is never suspect)."""
    if report and (report.get("suspect_v") == SUSPECT_V or report.get("match")):
        return list(report.get("suspect") or [])
    try:
        r = try_function(unit, tu, name, Path(body), diff_limit=100000, uninit=True)
    except TryError:
        return list((report or {}).get("suspect") or [])
    return list(r.get("suspect") or [])


def prev_suspect(unit: str, tu: str, name: str, side: Path) -> list[str]:
    """Suspect reasons of the saved <name>.best.c (cached in .best.json under
    the current SUSPECT_V; recomputed and written back when stale)."""
    return _prev_checks(unit, tu, name, side)[0]


def _sem_v() -> int:
    import semgate
    return semgate.VERSION


def sem_failures(report: dict | None) -> list[str]:
    """Semantics-gate failures of a try_function report ([] = pass / not run;
    a link-equivalent result is never blocked)."""
    if not report or report.get("match") or is_link_match(report):
        return []
    sem = report.get("semantics") or {}
    return [] if sem.get("ok", True) else list(sem.get("diffs") or ["semantics differ"])


def body_sem_failures(unit: str, tu: str, name: str, body: Path, report: dict | None) -> list[str]:
    """sem_failures() of BODY, re-running try_function when REPORT has no gate result."""
    if report and (report.get("match") or "semantics" in report or "semantics_error" in report
                   or is_link_match(report)):
        return sem_failures(report)
    try:
        r = try_function(unit, tu, name, Path(body), diff_limit=0)
    except TryError:
        return []
    return sem_failures(r)


def _prev_checks(unit: str, tu: str, name: str, side: Path) -> tuple[list[str], list[str]]:
    """(suspect reasons, semantics-gate failures) of the saved <name>.best.c,
    cached in .best.json (suspect_v / sem_v); recomputed with one try_function
    run and written back when stale."""
    try:
        prev = json.loads(side.read_text())
    except (OSError, ValueError):
        return [], []
    if prev.get("suspect_v") == SUSPECT_V and prev.get("sem_v") == _sem_v():
        return list(prev.get("suspect") or []), list(prev.get("semantics") or [])
    best_c = side.with_name(side.name[:-len(".best.json")] + ".best.c")
    if not best_c.is_file():
        return [], []
    if (prev.get("score") or 0) >= 1:
        return [], []                      # a match (integrate keeps it)
    try:
        r = try_function(unit, tu, name, best_c, diff_limit=100000, uninit=True)
    except TryError:
        return list(prev.get("suspect") or []), []
    psus = [] if r.get("match") else list(r.get("suspect") or [])
    psem = sem_failures(r)
    try:                                   # cache it (score untouched)
        cur = json.loads(side.read_text())
        if cur.get("score") == prev.get("score"):
            cur["suspect_v"], cur["sem_v"] = SUSPECT_V, _sem_v()
            for k, v in (("suspect", psus), ("semantics", psem)):
                if v:
                    cur[k] = v[:4]
                else:
                    cur.pop(k, None)
            side.write_text(json.dumps(cur) + "\n")
    except (OSError, ValueError):
        pass
    return psus, psem


# 2026-10-19 behavioural gate (behgate.py): the objects of the last few candidates
# that passed the static gate, so prefer_over_best can run target and candidate
# side by side when a body is about to become the new best.
_BEH_IN: dict = {}


def _beh_key(unit: str, tu: str, name: str, body: Path):
    try:
        import hashlib
        return (unit, tu, name, hashlib.sha1(Path(body).read_bytes()).hexdigest())
    except OSError:
        return None


def _beh_stash(unit: str, tu: str, name: str, body: Path, base_obj: Path, cand_obj: Path) -> None:
    k = _beh_key(unit, tu, name, body)
    if k is None:
        return
    try:
        _BEH_IN[k] = (base_obj.read_bytes(), cand_obj.read_bytes())
    except OSError:
        return
    while len(_BEH_IN) > 6:
        _BEH_IN.pop(next(iter(_BEH_IN)))


def behaviour_check(unit: str, tu: str, name: str, body: Path) -> dict:
    """behgate.check() of BODY (objects from the try_function run that compiled it,
    else one more try_function run): {"ok": True/False/None, "detail", "seeds", "ms"}."""
    k = _beh_key(unit, tu, name, body)
    objs = _BEH_IN.get(k) if k else None
    if objs is None and k:
        try:
            try_function(unit, tu, name, Path(body), diff_limit=0)
        except TryError:
            pass
        objs = _BEH_IN.get(k)
    if objs is None:
        return {"ok": None, "detail": "unknown: no objects (static gate failed or did not run)"}
    import behgate
    try:
        text = Path(body).read_text(errors="replace")
    except OSError:
        text = ""
    return behgate.check(unit, name, objs[0], objs[1], sym_address, real_proto, text,
                         symbol_table=symbol_addresses)


def prefer_over_best(unit: str, tu: str, name: str, body: Path, score, report: dict | None,
                     side: Path) -> tuple[bool, list[str]]:
    """_prefer_over_best() plus the behavioural gate: a body that would become the
    new best is run against the target (behgate.py, a few hundred ms); a definite
    difference (memory written, calls + args, return value) blocks it like the
    static gate. Unknown (VU0 / step cap / ...) never blocks. The result is added
    to REPORT["semantics"]["behaviour"]."""
    ok, sus = _prefer_over_best(unit, tu, name, body, score, report, side)
    if not ok or (report and (report.get("match") or is_link_match(report))):
        return ok, sus
    try:
        beh = behaviour_check(unit, tu, name, body)
    except Exception as e:  # noqa: BLE001 - diagnostic only, never blocks
        beh = {"ok": None, "detail": f"error: {type(e).__name__}: {e}"[:200]}
    if report is not None and isinstance(report.get("semantics"), dict):
        report["semantics"]["behaviour"] = beh
    elif report is not None:
        report["semantics_behaviour"] = beh
    if beh.get("ok") is False:
        return False, ["semantics: behaviour: " + str(beh.get("detail"))]
    return ok, sus



def narrow_alias_store(src: str) -> list[str]:
    """2026-10-19 regate finding: permuter output `*(_ap = &var) = v` where _ap is
    a narrower pointer (char*/short*) than &var stores only part of VAR (a byte
    insert into a pointer/int). Neither semgate nor behgate sees it reliably."""
    out = []
    for m in re.finditer(r"\*\s*\(\s*(\w+)\s*=\s*&\s*(\w+)\s*\)\s*=", src):
        ap, var = m.group(1), m.group(2)
        da = re.search(r"([A-Za-z_][\w ]*?)\s*(\*+)\s*\b" + ap + r"\s*[;=,)]", src)
        dv = re.search(r"([A-Za-z_][\w ]*?)\s*(\**)\s*\b" + var + r"\s*(\[[^\]]*\])?\s*[;=,)]", src)
        if not da or not dv:
            continue
        ta = da.group(1).split()[-2:]
        tv = dv.group(1).split()[-2:]
        if dv.group(3):
            continue
        sz = lambda words, stars: 4 if stars else next((n for k, n in (
            ("char", 1), ("s8", 1), ("u8", 1), ("short", 2), ("s16", 2), ("u16", 2),
            ("long long", 8), ("s64", 8), ("u64", 8), ("double", 8), ("f64", 8))
            if k in " ".join(words)), 4)
        if sz(ta, da.group(2)[1:]) < sz(tv, dv.group(2)):
            out.append(f"narrow alias store *({ap} = &{var}) ({' '.join(ta)}{da.group(2)} into {' '.join(tv)}{dv.group(2)})")
    return out

def _prefer_over_best(unit: str, tu: str, name: str, body: Path, score, report: dict | None,
                      side: Path) -> tuple[bool, list[str]]:
    """(BODY should replace <name>.best.c, BODY's suspect reasons). Plain
    score order unless one side is suspect: then SUSPECT_MARGIN applies. The
    saved best's suspect is re-checked (uninit included) when its .best.json
    predates the current SUSPECT_V, and written back. Fold15 (o587
    CheckGameSymbol 0.897 / o590 MenuSystemMenuMain 0.748 wrong permuter
    bests vs correct 0.88 / 0.55 bodies): when the saved best is suspect, ANY
    body that compiles and is not suspect replaces it, whatever its score."""
    if score is None:
        return False, []
    if report and report.get("match"):
        return True, []
    # 2026-10-19 semantics gate: a body that visibly computes something else than
    # the target (constants, float literals, call args, loads moved across calls,
    # int/float conversions) is never saved as best, whatever its score
    semf = body_sem_failures(unit, tu, name, body, report)
    if semf:
        return False, ["semantics: " + d for d in semf]
    try:
        apf = narrow_alias_store(body.read_text(errors="replace"))
    except OSError:
        apf = []
    if apf:
        return False, ["semantics: " + d for d in apf]
    try:
        prev = json.loads(side.read_text())
    except (OSError, ValueError):
        prev = {}
    ps = prev.get("score", -1)
    best_c = side.with_name(side.name[:-len(".best.json")] + ".best.c")
    if ps is None or ps < 0 or not best_c.is_file():
        return True, full_suspect(unit, tu, name, body, report)
    psus, psem = _prev_checks(unit, tu, name, side)
    if psem:
        # the saved best fails the gate: any body that passes replaces it (the old
        # one is kept as <name>.best.semfail.c)
        try:
            shutil.copyfile(best_c, best_c.with_name(best_c.name[:-len(".best.c")] + ".best.semfail.c"))
        except OSError:
            pass
        return True, full_suspect(unit, tu, name, body, report)
    if score > ps + SUSPECT_MARGIN:
        sus = full_suspect(unit, tu, name, body, report)
        # 2026-10-19: nine saved bests computed the wrong thing (uninit vars,
        # wrong widths/offsets, invented literals); each carried a suspect
        # reason when it replaced a clean best. A suspect body never replaces
        # a clean one, whatever its score (a false positive is promoted by hand).
        if sus and not psus:
            return False, sus
        return True, sus
    if not psus and score <= ps - SUSPECT_MARGIN:
        return False, []
    sus = full_suspect(unit, tu, name, body, report)
    if psus and not sus:
        return True, sus                   # fold15: a clean body beats a suspect best at any score
    return best_rank(score, sus) > best_rank(ps, psus), sus


def best_record(score, report: dict | None, sus: list[str], **extra) -> dict:
    """.best.json record: score, permute_queue's --max-diff fields, suspect."""
    rec = {"score": score}
    for k in ("differing_words", "target_insns"):   # permute_queue's --max-diff gate
        if report and report.get(k) is not None:
            rec[k] = report[k]
    rec.update(extra)
    rec["suspect_v"] = SUSPECT_V
    if report and ("semantics" in report or report.get("match")):
        rec["sem_v"] = _sem_v()            # saved bodies pass the gate (prefer_over_best)
    if sus:                                         # fold9: likely semantically wrong seed
        rec["suspect"] = sus[:4]
    return rec


def _track_best(body: Path, score, report: dict | None = None, unit: str = "", tu: str = "",
                name: str = "") -> None:
    """Keep the best body of a function as <name>.best.c (+ .best.json with
    its score) so a later worse edit can't lose it; integrate --give-up
    restores it. Fold11: a suspect body ranks SUSPECT_MARGIN lower."""
    if score is None or not body.is_file() or "auto_decomp/bodies" not in str(body):
        return
    stem = body.stem[:-5] if body.stem.endswith(".best") else body.stem
    if stem != body.stem:
        return
    side = body.with_name(stem + ".best.json")
    if unit and tu and name:
        ok, sus = prefer_over_best(unit, tu, name, body, score, report, side)
    else:
        try:
            prev = json.loads(side.read_text()).get("score", -1)
        except (OSError, ValueError):
            prev = -1
        ok, sus = score > prev and not sem_failures(report), list((report or {}).get("suspect") or [])
    if ok:
        shutil.copyfile(body, body.with_name(stem + ".best.c"))
        side.write_text(json.dumps(best_record(score, report, sus)) + "\n")
TRIES_DIR = Path(__file__).resolve().parents[2] / "build/auto_decomp/tries"


def main() -> int:
    if len(sys.argv) > 1 and sys.argv[1] == "--compile-file":
        cp = argparse.ArgumentParser()
        cp.add_argument("--compile-file", type=Path, required=True)
        cp.add_argument("--unit", required=True)
        cp.add_argument("--tu", required=True)
        cp.add_argument("-o", type=Path, required=True)
        c = cp.parse_args()
        ok, log = compile_file(c.unit, c.tu, c.compile_file.resolve(), c.o.resolve())
        if not ok:
            sys.stderr.write(log if len(log) <= 3000 else log[:1500] + "\n...\n" + log[-1500:])
        return 0 if ok else 1
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", required=True)
    ap.add_argument("--tu", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--body", type=Path, required=True)
    ap.add_argument("--keep", action="store_true", help="keep the scratch directory")
    ap.add_argument("--diff-limit", type=int, default=200)
    ap.add_argument("--no-diff", action="store_true", help="omit the diff from stderr")
    ap.add_argument("--src-patch", type=Path,
                    help="JSON [[relpath, old, new], ...]: evaluate against the TU / src/<unit> files with "
                         "these replacements (nothing is written); integrate.py --src-patch applies them "
                         "with verified_edit.py (e.g. a stale prototype + its caller). Default: "
                         "bodies/<u>/<t>/<name>.patch.json when it exists (fold11 convention)")
    ap.add_argument("--gcse-diag", action="store_true",
                    help="fold12: also compile the body with -fno-gcse and report `gcse` (score_nogcse, "
                         "hint) - a much higher score means GCSE/PRE hoisting is the remaining diff")
    ap.add_argument("--sched-diag", action="store_true",
                    help="fold13: also compile the body with -fno-schedule-insns and report `sched` "
                         "(score_nosched, hint) - a higher score means sched1 moved something the "
                         "original had elsewhere (o537 camEventExec)")
    ap.add_argument("--note", help="one line: what this variant changes (logged to the attempts diary, "
                                   "attempts.py show <key>)")
    ap.add_argument("--owner", help="worker name for the attempts diary (default $TRY_FN_OWNER / the claim)")
    ap.add_argument("--no-try-hash", action="store_true", help="debug: bypass the unchanged-body hash guard")
    ap.add_argument("--json-out", type=Path, help="debug: also write the full report (incl. diff) to this file")
    a = ap.parse_args()
    # Per-lease budget: agents get TRY_BUDGET runs per function (next.py --claim
    # resets the counter). Brute-force sweeps belong to the permuter stage.
    budget = int(os.environ.get("TRY_FN_BUDGET", TRY_BUDGET))
    cnt = TRIES_DIR / f"{a.unit}__{a.tu}__{a.name}"
    n = int(cnt.read_text()) if cnt.is_file() else 0
    if budget > 0 and n >= budget:
        print(json.dumps({"ok": False, "match": False, "stage": "budget",
                          "error": f"try budget exhausted ({n}/{budget} runs for this lease): "
                                   "run integrate.py --give-up with your best body"}, indent=1))
        return 3
    # Unchanged-body guard (CLI only; try_function itself is not affected):
    # refuse a body identical to the previous CLI run for this key (same
    # --src-patch / diag flags) without counting it. Keyed to the counter
    # value, so a lease reset (counter removed) clears it.
    hfile = TRIES_DIR.parent / "try_hash" / cnt.name
    try:
        import hashlib
        h = hashlib.sha1(a.body.read_bytes() + b"\0" + (a.src_patch.read_bytes() if a.src_patch and
                         a.src_patch.is_file() else b"") + f"\0{a.gcse_diag}{a.sched_diag}".encode()).hexdigest()
    except OSError:
        h = None
    if (not a.no_try_hash) and h and n > 0 and hfile.is_file() and hfile.read_text().strip() == f"{n}:{h}":
        print(json.dumps({"ok": False, "match": False, "stage": "unchanged_body", "error": "unchanged_body",
                          "detail": "this body is identical to the last one tried for this function "
                                    "(not counted against the budget): change the C before re-running",
                          "tries": n}, indent=1))
        return 4
    TRIES_DIR.mkdir(parents=True, exist_ok=True)
    cnt.write_text(str(n + 1))
    if h:
        try:
            hfile.parent.mkdir(parents=True, exist_ok=True)
            hfile.write_text(f"{n + 1}:{h}\n")
        except OSError:
            pass
    try:
        ov = load_src_patch(a.unit, a.src_patch)[1] if a.src_patch else None
        report = try_function(a.unit, a.tu, a.name, a.body.resolve(), a.keep, a.diff_limit, overrides=ov,
                              uninit=True)
    except TryError as e:
        report = {"ok": False, "match": False, "stage": "setup", "error": str(e)}
    _track_best(a.body.resolve(), report.get("score"), report, a.unit, a.tu, a.name)
    if a.gcse_diag and report.get("ok") and not report.get("match"):
        report["gcse"] = gcse_diag(a.unit, a.tu, a.name, a.body.resolve(), report.get("score"))
    if a.sched_diag and report.get("ok") and not report.get("match"):
        report["sched"] = sched_diag(a.unit, a.tu, a.name, a.body.resolve(), report.get("score"))
    try:   # attempts diary (attempts.py): one record per CLI run; never blocks the run
        import attempts as _attempts
        _attempts.record_try(a.unit, a.tu, a.name, report, a.body, a.note, a.owner)
    except Exception:  # noqa: BLE001
        pass
    if report.get("context") == "CONTEXT":
        sys.stderr.write(report.get("context_note", "CONTEXT") + "\n")
    diff = report.pop("diff", None)
    if diff and not a.no_diff:
        sys.stderr.write("\n".join(diff) + "\n")
    if diff:
        report["diff_rows"] = len(diff) - 1
    text = json.dumps(report, indent=1) + "\n"
    if a.json_out:
        a.json_out.parent.mkdir(parents=True, exist_ok=True)
        a.json_out.write_text(json.dumps({**report, "diff": diff}, indent=1) + "\n")
    sys.stdout.write(text)
    if not report.get("ok"):
        if report.get("stage") == "setup":
            return 2
        return 1
    return 0 if report["match"] else 1


def auto_patch_path(unit: str, tu: str, name: str) -> Path:
    """Fold11 convention: a worker's prototype fix next to its body,
    build/auto_decomp/bodies/<u>/<t>/<name>.patch.json (same format as
    --src-patch). try_function uses it when called without OVERRIDES and
    integrate.py applies it with verified_edit.py (o510 HitCheckEnemy)."""
    return ROOT / "build/auto_decomp/bodies" / unit / tu / f"{name}.patch.json"


def load_src_patch(unit: str, path: Path, tolerant: bool = False) -> tuple[list, dict]:
    """--src-patch FILE: JSON [[relpath, old, new], ...] (a stale prototype /
    caller fixed together, e.g. a 1-arg prototype of a 2-arg function) ->
    (patch, try_function overrides). Only files directly under src/<unit>/;
    each OLD must occur exactly once. Raises TryError otherwise. TOLERANT:
    an entry whose OLD is gone and whose NEW is present is already applied
    (dropped from the returned patch)."""
    try:
        patch = json.loads(Path(path).read_text())
    except (OSError, ValueError) as e:
        raise TryError(f"--src-patch {path}: {e}")
    ov: dict = {}
    kept: list = []
    if not isinstance(patch, list):
        raise TryError("--src-patch must be a JSON list of [relpath, old, new]")
    for item in patch:
        if not (isinstance(item, list) and len(item) == 3):
            raise TryError("--src-patch entries must be [relpath, old, new]")
        rel, old, new = item
        f = (ROOT / rel).resolve()
        if f.parent != (ROOT / "src" / unit).resolve():
            raise TryError(f"--src-patch: {rel} is not a file in src/{unit}/")
        txt = ov.get(f.name) or f.read_text()
        if tolerant and txt.count(old) == 0 and new and txt.count(new) >= 1:
            continue                           # already applied (e.g. by an earlier integrate)
        if txt.count(old) != 1:
            raise TryError(f"--src-patch: {old[:60]!r} occurs {txt.count(old)} times in {rel} (need 1)")
        ov[f.name] = txt.replace(old, new, 1)
        kept.append(item)
    return kept, ov


# ------------------------------------------------- semantic suspicion (fold9) ----
# A seed can score 0.8-0.95 and still compute the wrong thing (o483 set_clip byte
# vs element offset, o485 / o491 / o492 wrong logic, ov01/cmd `*(int *)` on a
# short field, m2c's `p += 4` on an `int *`). Register / schedule noise never
# changes these, so they are cheap, conservative tells (measured on the 223
# pending .best.c bodies: 30 flagged, see README "suspect seeds").
_SUS_MEM = re.compile(r"^(lb|lbu|lh|lhu|lw|lwu|ld|lq|lwc1|ldc1|sb|sh|sw|sd|sq|swc1|sdc1)$")
_SUS_ST = re.compile(r"^(sb|sh|sw|sd|sq|swc1|sdc1)$")
_SUS_W = {"sb": 1, "sh": 2, "sw": 4, "sd": 8, "sq": 16, "swc1": 4, "sdc1": 8}
_SUS_K = ("li", "ori", "andi", "xori", "slti", "sltiu")
_SUS_FRAME = ("sp", "s8", "fp", "gp")


def _sus_ops(x: str):
    rel = "[" in x
    p = re.sub(r"\s*\[.*$", "", x).strip().split(None, 1)
    return (p[0] if p else ""), ([o.strip() for o in p[1].split(",")] if len(p) > 1 else []), rel


def _sus_mem(o: list[str]):
    m = re.fullmatch(r"(-?\d+)\((\w+)\)", o[1]) if len(o) == 2 else None
    return (int(m.group(1)), m.group(2)) if m and m.group(2) not in _SUS_FRAME else None


def _sus_memc(col):
    """Counter of non-frame memory accesses (mnemonic, offset, stores zero)."""
    from collections import Counter
    c = Counter()
    for x in col:
        mn, o, rel = _sus_ops(x)
        if rel or not _SUS_MEM.match(mn):
            continue
        m = _sus_mem(o)
        if m:
            c[(mn, m[0], o[0] == "zero" and bool(_SUS_ST.match(mn)))] += 1
    return c


def store_width_mismatch(T: list[str], C: list[str]) -> list[tuple[str, int, list[str]]]:
    """(our store mnemonic, offset, [target mnemonics]) where a non-frame offset
    is stored with another width only (insn texts without addresses)."""
    tm, cm = _sus_memc(T), _sus_memc(C)
    dt, dc = tm - cm, cm - tm
    tw: dict = {}
    for (mn, off, _z) in dt:
        if mn in _SUS_W:
            tw.setdefault(off, set()).add(mn)
    call = {(mn, off) for (mn, off, _z) in cm}
    out = []
    for (mn, off, _z) in sorted(dc):
        if mn not in _SUS_W or off not in tw:
            continue
        o = sorted(t for t in tw[off] if _SUS_W[t] != _SUS_W[mn] and t.endswith("c1") == mn.endswith("c1")
                   and (t, off) not in call)
        if o and (mn, off, o) not in out:
            out.append((mn, off, o))
    return out


def addend_mismatch(T: list[str], C: list[str]) -> list[tuple[str, int, int]]:
    """(source register, our addend, target addend) for aligned `addiu rX,rS,K`
    rows with the same rS and another small K that neither side has elsewhere
    and that no memory offset through rX compensates (a base shift)."""
    def addius(col):
        s = set()
        for x in col:
            mn, o, rel = _sus_ops(x)
            if mn == "addiu" and not rel and len(o) == 3:
                s.add((o[1], o[2]))
        return s

    def offs_via(col, reg):
        s = set()
        for x in col:
            mn, o, rel = _sus_ops(x)
            if not rel and _SUS_MEM.match(mn):
                m = _sus_mem(o)
                if m and m[1] == reg:
                    s.add(m[0])
        return s
    ta, ca = addius(T), addius(C)
    out: list = []
    sm = difflib.SequenceMatcher(None, T, C, autojunk=False)
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op != "replace":
            continue
        for a, b in zip(T[i1:i2], C[j1:j2]):
            ma, oa, ra = _sus_ops(a)
            mb, ob, rb = _sus_ops(b)
            if not (ma == mb == "addiu" and not ra and not rb and len(oa) == 3 == len(ob)
                    and oa[1] == ob[1] and oa[1] not in ("sp", "zero", "gp")):
                continue
            try:
                va, vb = int(oa[2], 0), int(ob[2], 0)
            except ValueError:
                continue
            if va == vb or not (1 < abs(va) < 0x1000 and 1 < abs(vb) < 0x1000):
                continue
            if (ob[1], ob[2]) in ta and (oa[1], oa[2]) in ca:
                continue                       # both present on both sides: only moved
            d = va - vb
            if any(o + d in offs_via(C, ob[0]) for o in offs_via(T, oa[0])):
                continue                       # memory offsets compensate: same addresses
            if (ob[1], vb, va) not in out:
                out.append((ob[1], vb, va))
    return out


_RU_NODEST = re.compile(r"^(sb|sh|sw|sd|sq|swc1|sdc1|swl|swr|sdl|sdr|beq|bne|beql|bnel|beqz|bnez|beqzl|bnezl|bgez|"
                        r"bgtz|blez|bltz|bgezl|bgtzl|blezl|bltzl|jr|mtc1|ctc1|dmtc1|mthi|mtlo|mthi1|mtlo1|teq|tne|"
                        r"tge|tlt|mult|multu|div|divu)$")
_RU_UNCOND = re.compile(r"^(b|j|jr|jal|jalr|bal)$")


def call_result_uses(col: list[str], strict: bool = False, start: int | None = None) -> dict:
    """{callee: [calls, calls whose v0 is used]} over insn rows ("addr: insn",
    addresses optional): after `jal f` + its delay slot, v0 is `used` when an
    insn reads it (source operand / memory base / branch test) or the function
    returns (`jr ra`) before anything writes it or calls again (an
    unconditional branch's delay slot is checked too: o531 `b L; move v0,zero`).
    The scan follows one path (fall-through, unconditional branches to their
    target): a read on it is definite. A write is a sure discard only when no
    conditional branch came first; unknown counts as used unless STRICT
    (STRICT = definite reads only, the target side; the candidate side counts
    unknown as used, so only a sure discard flags).
    A sibcall `j f` counts as a used call. START = the function's section
    offset (branch operands are section offsets, rows function-relative);
    without it an unconditional branch ends the scan (unknown). Fold12 (o531
    seed dropped a call's result instead of returning it)."""
    out: dict = {}
    split = lambda x: (x.split(":", 1)[0].strip(), x.split(":", 1)[1].strip()) if ":" in x else ("", x.strip())  # noqa: E731
    rows = [split(x) for x in col]
    ins = [r[1] for r in rows]
    addr = []
    for r in rows:
        try:
            addr.append(int(r[0], 16))
        except ValueError:
            addr.append(None)
    # same-TU callees are `.text` relocations: a `j` there is a sibcall only when
    # the same address is also a `jal` target (else it may be a local jump)
    jal_text = {x.split()[1] for x in ins if x.split()[:1] == ["jal"] and "[26 .text]" in x and len(x.split()) > 1}

    def v0(y):
        """'r' reads v0, 'w' writes it, None neither."""
        mn, o, _rel = _sus_ops(y)
        if not mn:
            return None
        srcs = o if _RU_NODEST.match(mn) else o[1:]
        if mn in ("mult", "multu", "div", "divu") and len(o) == 3:
            srcs = o[1:]
        if any(re.fullmatch(r"(?:-?\w+\()?v0\)?", a) for a in srcs) or (
                o and not _RU_NODEST.match(mn) and re.search(r"\(v0\)", o[0])):
            return "r"
        if o and o[0] == "v0" and not _RU_NODEST.match(mn):
            return "w"
        return None
    for i, x in enumerate(ins):
        p = x.split()
        if not p or p[0] not in ("jal", "j"):
            continue
        m = re.search(r"\[26 ([\w.$]+)\]", x)
        if not m or (p[0] == "j" and m.group(1) == ".text" and (len(p) < 2 or p[1] not in jal_text)):
            continue
        f = m.group(1)
        c = out.setdefault(f, [0, 0])
        c[0] += 1
        if p[0] == "j":                 # sibcall: `return f(..)`
            c[1] += 1
            continue
        used = None
        k, steps = i + 2, 0
        while k < len(ins) and steps < 60:
            steps += 1
            mn, o, rel = _sus_ops(ins[k])
            if mn == "jr" and o and o[0] == "ra":
                used = True
                break
            a = v0(ins[k])
            if a:
                used = a == "r"
                break
            if mn in ("jal", "jalr"):
                # its delay slot runs first (JS_callMethod: `jal g; sw v0,240(sp)`)
                a = v0(ins[k + 1]) if k + 1 < len(ins) else None
                used = a == "r"
                break
            if mn in ("b", "j") and not rel and o:
                # unconditional: the delay slot runs, then continue at the target
                a = v0(ins[k + 1]) if k + 1 < len(ins) else None
                if a:
                    used = a == "r"
                    break
                try:
                    k = addr.index(int(o[-1], 16) - start)
                except (ValueError, TypeError):
                    break
                continue
            if mn.startswith("b") and not strict:
                break               # conditional: another path may read it - not a sure discard
            if _RU_UNCOND.match(mn):
                break
            k += 1
        c[1] += 1 if used or (used is None and not strict) else 0
    return out


def semantic_suspect(da: list[str], db: list[str], starts: tuple | None = None) -> list[str]:
    """Reasons the candidate (DB) likely computes something other than the
    target (DA), independent of register allocation / scheduling:
      store width   same non-frame offset stored with another width only (sw vs sh)
      stored value  a non-frame (sb/sh/sw/sd, offset) slot gets `zero` on one side only
      address addend aligned `addiu rX,rS,K` with the same rS and another small K,
                    not compensated by shifted memory offsets (m2c `p += 4` on int *)
      constant      one li/ori/andi/xori/slti/sltiu class has 1-2 values per side
                    that never appear on the other side
      missing logic >= 3 non-trivial target constants absent from ours and ours
                    is >= 10% shorter (a branch of the function is not there)
      call count    (fold10) a named callee called (jal or tail `j`) a different number
                    of times, or another number of same-TU (`.text`) calls: o496
                    cmdStatSet's sibling-copy seed dropped a cmdNum call (wrong
                    arg order); m2c `inline_fn` stubs, implicit-int fptodp
      call result   (fold12) same number of calls to f, the target reads / returns
                    v0 after more of them (o531: a call's result dropped)
      uninitialized (fold11, not here: try_function(uninit=True) adds it from
                    gcc -Wuninitialized on the candidate .i; fold12 adds
                    `shadowed param` from -Wshadow in the same run)"""
    strip = lambda s: s.split(":", 1)[1].strip() if ":" in s else s.strip()  # noqa: E731
    T, C = [strip(x) for x in da], [strip(x) for x in db]
    out: list[str] = []
    from collections import Counter

    mem = _sus_memc
    tm, cm = mem(T), mem(C)
    for mn, off, tgt in store_width_mismatch(T, C):
        out.append(f"store width: ours {mn} at offset {off}, target {'/'.join(tgt)} (wrong field type / cast?)")
    # stored value (sq excluded: gcc materializes a TImode zero as `por rX,zero,zero`)
    def kinds(col):
        d: dict = {}
        for x in col:
            mn, o, rel = _sus_ops(x)
            if rel or not _SUS_ST.match(mn) or mn == "sq":
                continue
            m = _sus_mem(o)
            if m:
                d.setdefault((mn, m[0]), set()).add("zero" if o[0] == "zero" else "a value")
        return d
    tk, ck = kinds(T), kinds(C)
    for k in sorted(tk):
        if k in ck and tk[k] != ck[k]:
            miss, ex = tk[k] - ck[k], ck[k] - tk[k]
            if miss:
                out.append(f"stored value: the target stores {'/'.join(sorted(miss))} with {k[0]} at offset "
                           f"{k[1]}, ours never does")
            elif ex:
                out.append(f"stored value: ours stores {'/'.join(sorted(ex))} with {k[0]} at offset {k[1]}, "
                           f"the target never does")
    # address addend
    for reg, vb, va in addend_mismatch(T, C):
        r = f"address addend: ours adds {vb} to {reg} where the target adds {va}"
        if va and vb % va == 0 or vb and va % vb == 0:
            r += " (element vs byte stride? m2c `p += K` on a typed pointer)"
        if r not in out:
            out.append(r)
    # constants
    def consts(col):
        c = Counter()
        for x in col:
            mn, o, rel = _sus_ops(x)
            if mn in _SUS_K and o and not rel:
                try:
                    c[(mn, int(o[-1], 0))] += 1
                except ValueError:
                    pass
        return c
    tc, cc = consts(T), consts(C)
    tv, cv = {v for (_, v) in tc}, {v for (_, v) in cc}
    for mn in _SUS_K:
        a = sorted({v for (m, v) in tc - cc if m == mn and v not in cv and abs(v) > 1})
        b = sorted({v for (m, v) in cc - tc if m == mn and v not in tv and abs(v) > 1})
        if a and b and len(a) <= 2 and len(b) <= 2:
            out.append(f"constant: ours {mn} {b} where the target has {mn} {a}")
    # missing logic
    def kvals(col):
        s = set()
        for x in col:
            mn, o, rel = _sus_ops(x)
            if rel or not o:
                continue
            if mn in _SUS_K or (mn in ("addiu", "daddiu") and "sp" not in o):
                try:
                    v = int(o[-1], 0)
                except ValueError:
                    continue
                if abs(v) > 1:
                    s.add(v)
            elif _SUS_MEM.match(mn):
                m = _sus_mem(o)
                if m:
                    s.add(m[0])
        return s
    miss = sorted(kvals(T) - kvals(C))
    if len(miss) >= 3 and len(C) < 0.9 * len(T):
        out.append(f"missing logic: target constants {miss[:5]} never appear in ours "
                   f"({len(C)} insns vs {len(T)})")
    # call count (fold10)
    def calls(col):
        c = Counter()
        for x in col:
            mn = x.split()[0] if x.split() else ""
            m = re.search(r"\[26 ([\w.$]+)\]", x)
            if mn in ("jal", "j") and m:
                c[m.group(1)] += 1
        return c
    tcl, ccl = calls(T), calls(C)
    for f in sorted((set(tcl) | set(ccl)) - {".text"}):
        if tcl[f] != ccl[f]:
            out.append(f"call count: the target calls {f} {tcl[f]}x, ours {ccl[f]}x")
            break
    if tcl[".text"] != ccl[".text"]:
        out.append(f"call count: the target makes {tcl['.text']} same-TU calls, ours {ccl['.text']}")
    # call result (fold12): same number of calls, the target keeps the value of more of them
    st = starts or (None, None)
    tu_, cu_ = call_result_uses(da, strict=True, start=st[0]), call_result_uses(db, start=st[1])
    for f in sorted(set(tu_) & set(cu_)):
        if tu_[f][0] == cu_[f][0] and tu_[f][1] > cu_[f][1]:
            g = "a same-TU callee" if f == ".text" else f
            out.append(f"call result: the target uses / returns {g}'s return value after {tu_[f][1]} of "
                       f"{tu_[f][0]} calls, ours after {cu_[f][1]} (result discarded? `return f(..)`)")
            break
    # fold15 (o587 CheckGameSymbol permuter .best.c 0.897): loop pointer never
    # advanced / store hoisted out of its condition (need section starts)
    if st[0] is not None and st[1] is not None:
        try:
            out += loop_base_suspect(da, db, st) + guarded_store_suspect(da, db, st)
        except Exception:  # noqa: BLE001 - diagnostic only
            pass
    return out


_SG_COND = re.compile(r"^(beq|bne|beqz|bnez|bgez|bgtz|blez|bltz|bc1t|bc1f|bgezal|bltzal)l?$")
_SG_XFER = re.compile(r"^(b|j|jal|jalr|jr|bal)$")
_SG_CLOB = {"v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9",
            "at", "ra"}


def _sg_rows(col: list[str], start: int) -> list[tuple]:
    """[(function-relative addr, mnemonic, operands, has reloc, branch target
    (function-relative) or None)] of insn rows "addr: insn"."""
    out = []
    for x in col:
        if ":" not in x:
            continue
        a, ins = x.split(":", 1)
        try:
            addr = int(a.strip(), 16)
        except ValueError:
            continue
        mn, o, rel = _sus_ops(ins)
        tgt = None
        if (mn == "b" or _SG_COND.match(mn)) and o and not rel:
            try:
                tgt = int(o[-1], 16) - start
            except ValueError:
                tgt = None
        out.append((addr, mn, o, rel, tgt))
    return out


def _sg_is_xfer(mn: str) -> bool:
    return bool(mn == "b" or _SG_COND.match(mn) or _SG_XFER.match(mn))


def loop_base_suspect(da: list[str], db: list[str], starts: tuple) -> list[str]:
    """Fold15 (o587 CheckGameSymbol): a memory access (mnemonic, offset) the
    target makes inside a loop ONLY through a pointer the loop advances
    (`addiu rX,rX,K`) and ours ONLY through a loop-invariant base (never
    written in the loop, not a caller-saved reg across a call): our pointer
    is never incremented, or `MapUnit->f` stands where `MapUnit[i].f` was
    meant. Innermost loop per access."""
    def classes(col, start):
        rows = _sg_rows(col, start)
        loops = []
        for b, r in enumerate(rows):
            if r[4] is not None and r[4] <= r[0]:
                t = next((i for i, q in enumerate(rows) if q[0] == r[4]), None)
                if t is not None:
                    loops.append((t, min(b + 1, len(rows) - 1)))
        info = []
        for t, e in loops:
            inc, w, other, call = set(), set(), set(), False
            for (_a, mn, o, rel, _t) in rows[t:e + 1]:
                if mn in ("jal", "jalr", "bal"):
                    call = True
                if not o or _RU_NODEST.match(mn) or _sg_is_xfer(mn):
                    continue
                w.add(o[0])
                selfinc = False
                if mn in ("addiu", "daddiu") and len(o) == 3 and o[0] == o[1] and not rel:
                    try:
                        selfinc = int(o[2], 0) != 0
                    except ValueError:
                        pass
                (inc if selfinc else other).add(o[0])
            if call:
                w |= _SG_CLOB
                other |= _SG_CLOB
            info.append((t, e, inc - other, w))   # advanced = only ever self-incremented
        d: dict = {}
        for i, (_a, mn, o, rel, _t) in enumerate(rows):
            if rel or not _SUS_MEM.match(mn):
                continue
            m = _sus_mem(o)
            if not m:
                continue
            inner = [x for x in info if x[0] <= i <= x[1]]
            if not inner:
                continue
            t, e, inc, w = min(inner, key=lambda x: x[1] - x[0])
            c = "adv" if m[1] in inc else ("inv" if m[1] not in w and m[1] != "zero" else "other")
            d.setdefault((mn, m[0]), set()).add(c)
        return d
    tc, cc = classes(da, starts[0]), classes(db, starts[1])
    out = []
    for k in sorted(set(tc) & set(cc)):
        if tc[k] == {"adv"} and cc[k] == {"inv"}:
            out.append(f"loop pointer: the target's {k[0]} at offset {k[1]} in a loop goes through a pointer "
                       f"advanced each pass, ours through a loop-invariant base (pointer never incremented? "
                       f"`p->f` where `p[i].f` was meant?)")
            break
    return out


def guarded_store_suspect(da: list[str], db: list[str], starts: tuple) -> list[str]:
    """Fold15 (o587 CheckGameSymbol `found[1] = i;` hoisted out of its `if`):
    a store (mnemonic, offset, frame/non-frame base) that occurs once on each
    side, outside delay slots, guarded by fewer conditional branches in ours.
    Guard count of store k = conditional branches laid out before k that can
    reach k and that k does not post-dominate (a path from the branch to the
    function exit avoids k): layout-only noise (an extra `b`, arm order, a
    branch-likely delay-slot copy) does not change it."""
    def guards(col, start):
        rows = _sg_rows(col, start)
        n = len(rows)
        idx = {r[0]: i for i, r in enumerate(rows)}
        succ: list[list[int]] = [[] for _ in range(n)]
        exits = set()
        likely = lambda mn: bool(_SG_COND.match(mn)) and mn.endswith("l") and mn not in ("bgezal", "bltzal")  # noqa: E731
        for i, (a, mn, o, rel, tg) in enumerate(rows):
            nxt = [i + 1] if i + 1 < n else []
            if i and _sg_is_xfer(rows[i - 1][1]):          # delay slot of rows[i-1]
                p = rows[i - 1]
                pt = idx.get(p[4]) if p[4] is not None else None
                if p[1] == "jr" or p[1] == "j":
                    exits.add(i)                # return / sibcall / jump table
                elif p[1] == "b":
                    succ[i] = [pt] if pt is not None else []
                elif likely(p[1]):
                    succ[i] = [pt] if pt is not None else []
                elif _SG_COND.match(p[1]):
                    succ[i] = ([pt] if pt is not None else []) + nxt
                else:
                    succ[i] = nxt               # call returns
                continue
            if likely(mn):
                succ[i] = nxt + ([i + 2] if i + 2 < n else [])
            else:
                succ[i] = nxt
                if not nxt and not _sg_is_xfer(mn):
                    exits.add(i)
        pred: list[list[int]] = [[] for _ in range(n)]
        for i, ss in enumerate(succ):
            for j in ss:
                pred[j].append(i)
        cond = [i for i, r in enumerate(rows) if _SG_COND.match(r[1])]
        d: dict = {}
        for k, (a, mn, o, rel, _t) in enumerate(rows):
            if rel or not _SUS_ST.match(mn) or len(o) != 2:
                continue
            if k and _sg_is_xfer(rows[k - 1][1]):
                continue                        # delay slot
            m = re.fullmatch(r"(-?\d+)\((\w+)\)", o[1])
            if not m:
                continue
            key = (mn, int(m.group(1)), "frame" if m.group(2) in _SUS_FRAME else "mem")
            avoid, todo = set(), [e for e in exits if e != k]
            while todo:                         # nodes reaching an exit without passing k
                x = todo.pop()
                if x in avoid:
                    continue
                avoid.add(x)
                todo += [y for y in pred[x] if y != k and y not in avoid]
            reach, todo = set(), [k]            # nodes that can reach k
            while todo:
                x = todo.pop()
                if x in reach:
                    continue
                reach.add(x)
                todo += [y for y in pred[x] if y not in reach]
            g = sum(1 for j in cond if j < k and j in reach and j in avoid)
            d.setdefault(key, []).append(g)
        return d
    def frame(col):
        for x in col[:8]:
            mn, o, _rel = _sus_ops(x.split(":", 1)[-1])
            if mn == "addiu" and o[:2] == ["sp", "sp"]:
                return o[2]
        return None
    ts, cs = guards(da, starts[0]), guards(db, starts[1])
    same_frame = frame(da) == frame(db)
    out = []
    for k in sorted(set(ts) & set(cs)):
        if len(ts[k]) != len(cs[k]) or len(ts[k]) > 3 or (k[2] == "frame" and not same_frame):
            continue                        # spill slots move with the frame layout
        for t, c in zip(sorted(ts[k]), sorted(cs[k])):
            if c < t and c <= t // 2:      # halved: an && merged into one test is not it
                out.append(f"guarded store: the target's {k[0]} at offset {k[1]}"
                           f"{' (stack)' if k[2] == 'frame' else ''} is guarded by {t} conditional "
                           f"branch(es), ours by {c} (store hoisted out of its condition?)")
                break
        if out:
            break
    return out


def text_syms(obj: Path, section: str) -> dict:
    """{section offset: function name} of the named symbols in SECTION of OBJ
    (resolves same-TU `jal N  [26 .text]` callees)."""
    elf = Elf(obj.read_bytes())
    idx = [i for i, sec in enumerate(elf.sections) if sec.name == section]
    return {sym.value: sym.name for sym in elf.symbols
            if idx and sym.shndx == idx[0] and sym.name and not sym.name.startswith((".L", "$"))}


def narrow_arg_calls(da: list[str], db: list[str], tsyms: tuple = ({}, {})) -> list[str]:
    """Fold15 (o536 / o574 / o575 / o577 ov01/cmd `thinkAdrGet(short)`):
    callees whose argument ours sign/zero-extends into an a-register (sll/sra
    16|24, andi 0xffff|0xff in the 4 insns before the jal or its delay slot)
    at more call sites than the target: the prototype narrows a param the
    caller passed as a raw int -> call it through `((int (*)(int))f)(x)`."""
    def count(col, syms):
        ins = [x.split(":", 1)[1].strip() if ":" in x else x.strip() for x in col]
        c: dict = {}
        for i, x in enumerate(ins):
            m = re.search(r"\[26 ([\w.$]+)\]", x)
            if not m or not x.startswith("jal "):
                continue
            f = m.group(1)
            if f == ".text":
                try:
                    f = syms.get(int(x.split()[1], 16)) or ""
                except (ValueError, IndexError):
                    f = ""
                if not f:
                    continue
            k = i
            while k > max(0, i - 4) and not _sg_is_xfer(_sus_ops(ins[k - 1])[0]) and not (
                    k >= 2 and _sg_is_xfer(_sus_ops(ins[k - 2])[0])):
                k -= 1                          # back to the previous call / branch (and its slot)
            ext = False
            for y in ins[k:i] + ins[i + 1:i + 2]:
                mn, o, rel = _sus_ops(y)
                if rel or len(o) != 3 or not re.fullmatch(r"a[0-3]", o[0]):
                    continue
                try:
                    v = int(o[2], 0)
                except ValueError:
                    continue
                if (mn in ("sll", "sra", "srl") and v in (16, 24)) or (mn == "andi" and v in (0xFFFF, 0xFF)):
                    ext = True
            if ext:
                c[f] = c.get(f, 0) + 1
        return c
    t, c = count(da, tsyms[0]), count(db, tsyms[1])
    return sorted(f for f in c if c[f] > t.get(f, 0))


def cols_from_rows(rows) -> tuple[list[str], list[str]] | None:
    """(target insns, candidate insns) of a try_fn side-by-side diff (addresses
    kept); None when it is absent or truncated."""
    if not rows or "   CANDIDATE" not in rows[0]:
        return None
    w = rows[0].index("   CANDIDATE")
    da, db = [], []
    for r in rows[1:]:
        if r.startswith("... ("):
            return None
        a, b = r[:w].strip(), r[w + 3:].strip()
        if a:
            da.append(a)
        if b:
            db.append(b)
    return da, db


def suspect_from_rows(rows) -> list[str]:
    """semantic_suspect() on a try_fn side-by-side diff (triage / brief caches)."""
    c = cols_from_rows(rows)
    return semantic_suspect(*c) if c else []


GCSE_MIN_GAIN = 0.02


def gcse_diag(unit: str, tu: str, name: str, body: Path, score) -> dict | None:
    """Fold12 diagnostic (o526 MenuTecExMain 0.49 -> 0.92, o532 MenuTecListMain
    0.51 -> 0.86 with -fno-gcse): recompile BODY with -fno-gcse. When that
    scores >= GCSE_MIN_GAIN higher than SCORE, GCSE/PRE hoisted a common
    subexpression (`p + K`, `p->f`) into a saved register across blocks where
    the original recomputes it: {"score_nogcse", "score", "better", "hint"}.
    One extra compile; None when it does not compile with the flag."""
    try:
        r = try_function(unit, tu, name, Path(body), diff_limit=0, cand_cflags="-fno-gcse")
    except Exception:  # noqa: BLE001 - diagnostic only
        return None
    if not r.get("ok") or score is None:
        return None
    ng = r.get("score") or 0
    out = {"score_nogcse": ng, "score": score, "better": ng >= score + GCSE_MIN_GAIN}
    if out["better"]:
        out["hint"] = (f"GCSE: with -fno-gcse this body scores {ng} (vs {score}): gcc's global CSE/PRE "
                       "hoisted a shared `p + K` / `p->f` into a saved register across blocks where the "
                       "original recomputes it. Take the base pointer's address once - "
                       "`(*(_ap = &p))->f` (or `(*(_ap = &p))[K]`) at one read, `T **_ap;` declared - "
                       "which makes p an ADDRESSOF pseudo (o526 matched with it, o532 0.51 -> 0.86); "
                       "trick_sweep family `gcse_split`. Block-local recomputed pointers also help.")
    return out


SCHED_MIN_GAIN = 0.02


def sched_diag(unit: str, tu: str, name: str, body: Path, score) -> dict | None:
    """Fold13 diagnostic (o537 camEventExec: the worker found the sched1 reorder
    this way; works for overlay TUs too): recompile BODY with
    -fno-schedule-insns (sched1 off, sched2 still on). >= SCHED_MIN_GAIN
    higher means the first scheduling pass moved loads/stores the original
    keeps in source order: move the statement (into the branch that uses it,
    after the call, before the `break`) instead of tuning registers.
    {"score_nosched", "score", "better", "hint"}; None when it does not compile."""
    try:
        r = try_function(unit, tu, name, Path(body), diff_limit=0, cand_cflags="-fno-schedule-insns")
    except Exception:  # noqa: BLE001 - diagnostic only
        return None
    if not r.get("ok") or score is None:
        return None
    ns = r.get("score") or 0
    out = {"score_nosched": ns, "score": score, "better": ns >= score + SCHED_MIN_GAIN}
    if out["better"]:
        out["hint"] = (f"SCHED1: with -fno-schedule-insns this body scores {ns} (vs {score}): the first "
                       "scheduling pass moves loads/stores across each other where the original keeps "
                       "another order. Move the statement in the C instead (a read into the branch / "
                       "loop arm that uses it - o537 sank a post-loop read before the loop's `break` - "
                       "or past a call), or break the dependency with a block-local temp; trick_sweep "
                       "families `sink_into_break`, `sched_barrier`, `block_barrier`, `stmtswap`.")
    return out


def is_link_match(r: dict | None) -> bool:
    """try report that only the whole-file gate can settle (call integrate.py
    --skip-try): false %lo relocations or absolute-vs-symbol slots."""
    return bool(r and r.get("ok") and not r.get("match")
                and (r.get("match_after_link") or r.get("link_check") or r.get("match_pending_jtbl")))


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Migrate a TU onto origin/master's type model and matched C wholesale.

    python3 -B scripts/auto_decomp/migrate_tu.py --tu menu_skill
    python3 -B scripts/auto_decomp/migrate_tu.py --from-rank build/tu_migrate_rank.json \
        --min-up-only 3 --max-local-only 0 --limit 8

Replaces only the TU's own sources (src/<unit>/<tu>.c and colocated .h, plus
include/<unit>/<tu>.h when present upstream). Never touches shared headers
(common.h, shared.h, party.h, ...). After the swap:

  1. Drop this TU's carve entry (stale vs new C data)
  2. Rebuild the TU object
  3. data_carve.py derive --write for the TU
  4. Refresh ninja scripts
  5. fix_rodata_local_labels.py (jumptable VAs)
  6. Whole-unit verify; on red, restore the backup under build/tu_migrate/backup/

Prints JSON. Exit 0 only when every requested TU migrated green.
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "tools"))
from state import locked, log  # noqa: E402
from verify_unit import rebuild  # noqa: E402

REF_DEFAULT = "origin/master"
BACKUP = ROOT / "build" / "tu_migrate" / "backup"
SHARED_HEADER_NAMES = {
    "common.h",
    "shared.h",
    "include_asm.h",
    "labels.inc",
    "macro.inc",
    "party.h",  # shared across many menu TUs; do not clobber during one-TU migrate
}

# File-scope storage whose name is already in symbol_addrs must not be
# re-emitted as C static/BSS (it lands at the wrong address and shifts GP).
STATIC_DEF = re.compile(
    r"^(?P<indent>\s*)(?P<decl>static\s+(?:(?:const|volatile)\s+)*"
    r"(?:unsigned\s+)?(?:char|short|int|long(?:\s+long)?)\s+"
    r"(?P<name>\w+)\s*(?:\[|=|;|/\*))",
    re.M,
)


def run(cmd: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)


def git_show(ref: str, path: str) -> str | None:
    r = run(["git", "show", f"{ref}:{path}"])
    return r.stdout if r.returncode == 0 else None


def git_exists(ref: str, path: str) -> bool:
    return run(["git", "cat-file", "-e", f"{ref}:{path}"]).returncode == 0


def compile_manifest(unit: str) -> dict:
    for p in (
        ROOT / "build" / unit / "compile-manifest.json",
        ROOT / "config" / "objects" / f"{unit}.compile.json",
    ):
        if p.is_file():
            return json.loads(p.read_text())
    raise SystemExit(f"no compile manifest for {unit}")


def tu_id(unit: str, tu: str) -> str | None:
    entry = (compile_manifest(unit).get("tus") or {}).get(tu)
    return entry.get("id") if isinstance(entry, dict) else None


def tu_paths(unit: str, tu: str, ref: str) -> list[str]:
    """Own-file paths to take from upstream for this TU."""
    paths = [f"src/{unit}/{tu}.c"]
    for p in (f"src/{unit}/{tu}.h", f"include/{unit}/{tu}.h", f"include/main/{tu}.h"):
        if git_exists(ref, p) and Path(p).name not in SHARED_HEADER_NAMES:
            paths.append(p)
    # de-dupe
    seen = set()
    out = []
    for p in paths:
        if p not in seen:
            seen.add(p)
            out.append(p)
    return out


def backup_paths(unit: str, tu: str, rels: list[str], *, force: bool = False) -> Path:
    dest = BACKUP / unit / tu
    # Never clobber a good local backup with a half-migrated tree: a failed
    # restore followed by a retry would snapshot upstream as "local".
    if dest.exists() and not force and (dest / "manifest.json").is_file():
        return dest
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True, exist_ok=True)
    meta = []
    for rel in rels + ["config/tu/data-carves.json"]:
        src = ROOT / rel
        entry = {"rel": rel, "existed": src.is_file()}
        if src.is_file():
            target = dest / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, target)
        meta.append(entry)
    (dest / "manifest.json").write_text(json.dumps(meta, indent=1) + "\n")
    return dest


def restore_backup(unit: str, tu: str) -> None:
    """Restore TU sources from backup.

    `data-carves.json` is shared across every TU. Restoring the whole file from a
    stale per-TU backup would wipe carve entries written by other successful
    migrates. Only this TU's carve slice is restored (see restore_carve_slice).
    """
    dest = BACKUP / unit / tu
    meta = json.loads((dest / "manifest.json").read_text())
    for entry in meta:
        rel = entry["rel"]
        if rel == "config/tu/data-carves.json":
            continue
        src = dest / rel
        target = ROOT / rel
        if entry["existed"]:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, target)
        elif target.is_file():
            target.unlink()
    restore_carve_slice(unit, tu)


def restore_carve_slice(unit: str, tu: str) -> None:
    """Put back only this TU's carve entry from the per-TU backup snapshot."""
    tid = tu_id(unit, tu)
    if not tid:
        return
    bak = BACKUP / unit / tu / "config" / "tu" / "data-carves.json"
    path = ROOT / "config" / "tu" / "data-carves.json"
    if not path.is_file():
        if bak.is_file():
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(bak, path)
        return
    data = json.loads(path.read_text())
    tus = data.setdefault("tus", {})
    if bak.is_file():
        old = json.loads(bak.read_text())
        entry = (old.get("tus") or {}).get(tid)
        if entry is not None:
            tus[tid] = entry
        else:
            tus.pop(tid, None)
    else:
        tus.pop(tid, None)
    path.write_text(json.dumps(data, indent=1) + "\n")


def drop_carve(unit: str, tu: str) -> dict | None:
    tid = tu_id(unit, tu)
    if not tid:
        return None
    path = ROOT / "config" / "tu" / "data-carves.json"
    if not path.is_file():
        return {"id": tid, "dropped": False}
    data = json.loads(path.read_text())
    had = tid in (data.get("tus") or {})
    if had:
        del data["tus"][tid]
        path.write_text(json.dumps(data, indent=1) + "\n")
    return {"id": tid, "dropped": had}


def derive_carve(unit: str, tu: str) -> dict:
    tid = tu_id(unit, tu)
    if not tid:
        return {"ok": False, "error": "no tu id"}
    source = ROOT / "src" / unit / f"{tu}.c"
    # ninja -C build/<unit> writes objects under build/<unit>/build/c/
    obj = ROOT / "build" / unit / "build" / "c" / f"{tu}.o"
    if not obj.is_file():
        alt = ROOT / "build" / unit / "c" / f"{tu}.o"
        if alt.is_file():
            obj = alt
    p = run(
        [
            sys.executable,
            "-B",
            str(ROOT / "tools" / "tu" / "data_carve.py"),
            "derive",
            "--root",
            str(ROOT),
            "--unit",
            unit,
            "--unit-dir",
            str(ROOT / "build" / unit),
            "--tu",
            tid,
            "--source",
            str(source),
            "--obj",
            str(obj),
            "--write",
        ]
    )
    try:
        return json.loads(p.stdout[p.stdout.index("{") :])
    except ValueError:
        return {"ok": False, "error": (p.stderr or p.stdout)[-500:], "rc": p.returncode}


def refresh_ninja(unit: str) -> dict:
    if unit == "main":
        cmd = [sys.executable, "-B", str(ROOT / "tools" / "tu" / "ninja_main.py"), "--root", str(ROOT), "--unit-dir", f"build/{unit}"]
    else:
        cmd = [
            sys.executable,
            "-B",
            str(ROOT / "tools" / "tu" / "ninja_ovl.py"),
            f"build/{unit}",
            unit,
            "--manifest",
            f"build/{unit}/compile-manifest.json",
            "--root",
            str(ROOT),
        ]
    p = run(cmd)
    return {"rc": p.returncode, "tail": (p.stdout or p.stderr)[-300:]}


def fix_rodata() -> dict:
    script = ROOT / "scripts" / "fix_rodata_local_labels.py"
    if not script.is_file():
        return {"skipped": True}
    p = run([sys.executable, "-B", str(script)])
    return {"rc": p.returncode, "tail": (p.stdout or p.stderr)[-300:]}


def known_symbols(unit: str) -> set[str]:
    return set(known_symbol_sizes(unit))


def known_symbol_sizes(unit: str) -> dict[str, int | None]:
    """name -> size in bytes when the symbol_addrs comment has `size:0x..`."""
    out: dict[str, int | None] = {}
    for path in (
        ROOT / "config" / "symbols" / f"{unit}.txt",
        ROOT / "build" / unit / "symbol_addrs.txt",
    ):
        if not path.is_file():
            continue
        for line in path.read_text(errors="replace").splitlines():
            m = re.match(r"^(\w+)\s*=\s*([^;]+);(.*)$", line)
            if not m:
                continue
            name = m.group(1)
            size = None
            sm = re.search(r"size:(0x[0-9A-Fa-f]+|\d+)", m.group(3))
            if sm:
                size = int(sm.group(1), 0)
            # Prefer first file's entry; don't overwrite a sized entry with None
            if name not in out or (out[name] is None and size is not None):
                out[name] = size
    return out


def known_symbol_vas(unit: str) -> dict[str, int]:
    """name -> VA from symbol_addrs."""
    out: dict[str, int] = {}
    for path in (
        ROOT / "config" / "symbols" / f"{unit}.txt",
        ROOT / "build" / unit / "symbol_addrs.txt",
    ):
        if not path.is_file():
            continue
        for line in path.read_text(errors="replace").splitlines():
            m = re.match(r"^(\w+)\s*=\s*([^;]+);", line)
            if not m:
                continue
            try:
                va = int(m.group(2).strip(), 0)
            except ValueError:
                continue
            out.setdefault(m.group(1), va)
    return out


def externize_d_rodata_objects(unit: str, tu: str) -> list[str]:
    """Extern file-scope `D_*` / named rodata objects so splat asm keeps the bytes.

    Used when derive cannot place C-emitted strings (missing siblings in a run).
    Turns `const char D_foo[N] = \"...\";` and `const T D_foo = {...};` into
    `extern const … D_foo;` (initializer dropped).
    """
    import tu_edit  # noqa: WPS433

    src = ROOT / "src" / unit / f"{tu}.c"
    text = src.read_text()
    items = tu_edit.scan(text)
    pieces: list[tuple[int, int, str]] = []
    changed: list[str] = []
    for it in items:
        if it.kind != "decl":
            continue
        chunk = it.text(text)
        stripped = chunk.lstrip()
        if stripped.startswith("extern") or stripped.startswith("typedef"):
            continue
        if re.match(r"(?:struct|union|enum)\b", stripped):
            continue
        try:
            names = {k for kind, k in tu_edit.decl_keys(chunk) if kind == "ordinary"}
        except Exception:
            continue
        hit = [n for n in names if n.startswith("D_") or n.startswith("jtbl_")]
        if not hit:
            continue
        if "{" in chunk and "(" in chunk.split("{", 1)[0]:
            continue
        head = chunk.split("=", 1)[0]
        if "(" in head and "[" not in head:
            continue
        # Include BSS tentatives (`static T D_foo;`) as well as inits/arrays.
        pieces.append((it.start, it.end, _extern_decl(chunk)))
        changed.extend(sorted(hit))
    if not pieces:
        return []
    out: list[str] = []
    last = 0
    for start, end, repl in pieces:
        out.append(text[last:start])
        out.append(repl)
        last = end
    out.append(text[last:])
    src.write_text("".join(out))
    seen: set[str] = set()
    return [x for x in changed if not (x in seen or seen.add(x))]


def reorder_static_inits_by_va(unit: str, tu: str) -> list[str]:
    """Reorder file-scope static initialized data so emission matches VA order.

    gcc emits file-scope static inits in source order. When upstream lists
    `moveHandlerTbl_1` before `waitHandlerTbl_0` but the ELF places wait at a
    lower VA, .data shifts and mapcheck fails. Sort those decls by symbol VA.
    """
    import tu_edit  # noqa: WPS433

    src = ROOT / "src" / unit / f"{tu}.c"
    text = src.read_text()
    vas = known_symbol_vas(unit)
    funcs = known_func_symbols(unit)
    items = tu_edit.scan(text)
    blocks: list[tuple[int, int, int, str, str]] = []  # va, start, end, name, chunk
    for it in items:
        if it.kind != "decl":
            continue
        chunk = it.text(text)
        stripped = chunk.lstrip()
        if not stripped.startswith("static"):
            continue
        if "=" not in chunk or "{" not in chunk:
            # Scalars like `static int flag = 0;` must not swap with tables —
            # that corrupts declarators (msg_init mfunc vs ctrlCodeFlags).
            continue
        try:
            names = {k for kind, k in tu_edit.decl_keys(chunk) if kind == "ordinary"}
        except Exception:
            continue
        hit = [n for n in names if n in vas and n not in funcs]
        if len(hit) != 1:
            continue
        name = hit[0]
        blocks.append((vas[name], it.start, it.end, name, chunk))
    if len(blocks) < 2:
        return []
    blocks.sort(key=lambda b: b[1])  # file order
    file_order = [b[3] for b in blocks]
    va_order = [b[3] for b in sorted(blocks, key=lambda b: b[0])]
    if file_order == va_order:
        return []
    # Place each decl's text into the file slots sorted by VA: the earliest
    # file slot gets the lowest-VA chunk, etc. Works even when function bodies
    # sit between the tables (sc_wait_parse_script).
    slots = [(b[1], b[2]) for b in blocks]
    va_chunks = [b[4] for b in sorted(blocks, key=lambda b: b[0])]
    # Apply end-first so earlier offsets stay valid.
    new_text = text
    for (start, end), chunk in sorted(zip(slots, va_chunks), key=lambda x: -x[0][0]):
        new_text = new_text[:start] + chunk + new_text[end:]
    if new_text == text:
        return []
    src.write_text(new_text)
    return va_order


def known_func_symbols(unit: str) -> set[str]:
    """Names marked `type:func` in symbol_addrs (never externize these decls)."""
    out: set[str] = set()
    for path in (
        ROOT / "config" / "symbols" / f"{unit}.txt",
        ROOT / "build" / unit / "symbol_addrs.txt",
    ):
        if not path.is_file():
            continue
        for line in path.read_text(errors="replace").splitlines():
            m = re.match(r"^(\w+)\s*=\s*([^;]+);(.*)$", line)
            if not m:
                continue
            if re.search(r"\btype:func\b", m.group(3)):
                out.add(m.group(1))
    return out


def _extern_decl(chunk: str) -> str:
    """`static T name[=...]` / `T name[=...]` -> `extern T name;` (drop initializer)."""
    body = chunk.lstrip()
    indent = chunk[: len(chunk) - len(body)]
    if body.startswith("static "):
        body = "extern " + body[len("static ") :]
    elif body.startswith("extern "):
        pass
    else:
        body = "extern " + body
    if "=" in body:
        # Drop initializer / brace init; keep a plain extern declaration.
        body = re.sub(r"\s*=\s*(?:\{(?:[^{}]|\{[^{}]*\})*\}|[^;]+);", ";", body, count=1)
    if not body.endswith("\n"):
        body += "\n"
    return indent + body


def externize_known_statics(unit: str, tu: str) -> list[str]:
    """Extern known file-scope statics/globals; rewrite function-local BSS to splat names.

    File-scope `static T name` in symbol_addrs becomes `extern` (GP/.sbss stable).
    File-scope non-static data defs of known symbols (`short PhCunt = 0;`) also
    become `extern` so splat owns the bytes (avoids .sdata alignment padding
    that breaks derive).

    Skips typedefs, struct/union/enum tags, and `type:func` symbols so a name
    clash like `typedef struct MenuTecList {…} MenuTecSelectList` is not
    rewritten when `MenuTecList` is also a data pointer in symbol_addrs.

    Function-local BSS (`static T flag;`) whose splat name `flag_N` is known becomes
    an extern + renamed uses, so C does not emit unowned .sbss.

    Function-local *initialized* statics stay put so gcc still emits `name.K` data.
    Removing earlier BSS locals shifts the file-wide local-static counter, so the
    original ELF's `name.N` may no longer match `name.K`. When splat encodes N
    (`head_1` → N=1) and K != N, append `__asm__("head.1 = head.0");` so the .o
    defines the original dotted symbol at the same address (ee-as accepts `=` /
    `.equ`; `.set` rejects dotted names).
    """
    import tu_edit  # noqa: WPS433

    src = ROOT / "src" / unit / f"{tu}.c"
    text = src.read_text()
    sizes = known_symbol_sizes(unit)
    funcs = known_func_symbols(unit)
    syms = set(sizes) - funcs
    changed: list[str] = []

    # --- file-scope static / known-global data decls ---
    items = tu_edit.scan(text)
    pieces: list[tuple[int, int, str]] = []
    for it in items:
        if it.kind != "decl":
            continue
        chunk = it.text(text)
        stripped = chunk.lstrip()
        is_static = stripped.startswith("static")
        if stripped.startswith("extern"):
            continue
        if stripped.startswith("typedef"):
            continue
        # Skip tag-only / aggregate definitions (struct Foo { … };).
        if re.match(r"(?:struct|union|enum)\b", stripped):
            continue
        try:
            # Ordinary names only — never match a struct tag against a data symbol.
            names = {k for kind, k in tu_edit.decl_keys(chunk) if kind == "ordinary"}
        except Exception:
            continue
        hit = names & syms
        if not hit:
            continue
        if "{" in chunk and "(" in chunk.split("{", 1)[0]:
            continue  # function definition
        # Function prototype / any declarator with (...) and no data object shape.
        head = chunk.split("=", 1)[0]
        if "(" in head and "[" not in head:
            continue
        # Non-static known data: include tentative defs (`T *name;`) so they
        # do not become COMMON/.sbss (mapcheck rejects unexpected COMMON).
        pieces.append((it.start, it.end, _extern_decl(chunk)))
        changed.extend(sorted(hit))
    if pieces:
        out = []
        last = 0
        for start, end, repl_text in pieces:
            out.append(text[last:start])
            out.append(repl_text)
            last = end
        out.append(text[last:])
        text = "".join(out)

    # --- function-local statics ---
    local_pat = re.compile(
        r"(?m)^(?P<indent>\s+)static\s+(?P<ty>(?:(?:const|volatile|unsigned|signed)\s+)*\w+(?:\s*\*)*)\s+"
        r"(?P<name>\w+)\s*(?P<rest>(?:\[[^\]]*\])?\s*(?:=\s*[^;]*)?;)"
    )
    tu_obj = tu_edit.TU(text)
    # Collect (abs_start, abs_end, fn_name, match, kind, splat) in file order.
    planned: list[dict] = []
    for block in sorted(tu_obj.by_name.values(), key=lambda b: b.start):
        body = text[block.start : block.end]
        counts: dict[str, int] = {}
        for m in local_pat.finditer(body):
            name = m.group("name")
            rest = m.group("rest")
            idx = counts.get(name, 0)
            counts[name] = idx + 1
            candidates = sorted(
                (s for s in syms if re.match(rf"^{re.escape(name)}_\d+$", s)),
                key=lambda s: int(s.rsplit("_", 1)[1]),
            )
            if not candidates:
                continue
            ty = m.group("ty").strip()
            if "=" in rest:
                # Initialized: keep decl; pick splat by size when possible.
                est = None
                sm = re.search(r'=\s*"((?:\\.|[^"\\])*)"\s*;', rest)
                if sm:
                    est = len(sm.group(1).encode("utf-8").decode("unicode_escape")) + 1
                splat = None
                if est is not None:
                    for c in candidates:
                        if sizes.get(c) == est:
                            splat = c
                            break
                if splat is None:
                    splat = candidates[idx] if idx < len(candidates) else candidates[0]
                planned.append(
                    dict(
                        kind="init",
                        abs_start=block.start + m.start(),
                        abs_end=block.start + m.end(),
                        fn=block.name if hasattr(block, "name") else None,
                        name=name,
                        ty=ty,
                        splat=splat,
                        indent=m.group("indent"),
                    )
                )
            else:
                splat = f"{name}_{idx}"
                if splat not in syms:
                    splat = candidates[min(idx, len(candidates) - 1)]
                planned.append(
                    dict(
                        kind="bss",
                        abs_start=block.start + m.start(),
                        abs_end=block.start + m.end(),
                        fn=block.name if hasattr(block, "name") else None,
                        name=name,
                        ty=ty,
                        splat=splat,
                        indent=m.group("indent"),
                        match=m,
                    )
                )

    # Assign post-externize counter K to kept (init) locals in file order.
    keep_idx = 0
    for p in planned:
        if p["kind"] == "init":
            p["k"] = keep_idx
            keep_idx += 1
            p["n"] = int(p["splat"].rsplit("_", 1)[1])

    # Apply BSS removals end-first; record renames per function.
    extern_lines: list[str] = []
    renames: list[tuple[str, str, str]] = []  # fn, old, splat
    for p in sorted((x for x in planned if x["kind"] == "bss"), key=lambda x: -x["abs_start"]):
        text = (
            text[: p["abs_start"]]
            + f"{p['indent']}/* storage: {p['splat']} */\n"
            + text[p["abs_end"] :]
        )
        extern_lines.append(f"extern {p['ty']} {p['splat']};\n")
        renames.append((p.get("fn") or "", p["name"], p["splat"]))
        changed.append(p["splat"])

    # Rename uses of BSS locals inside their functions.
    if renames:
        tu_obj = tu_edit.TU(text)
        # Group by fn
        by_fn: dict[str, list[tuple[str, str]]] = {}
        for fn, old, splat in renames:
            by_fn.setdefault(fn, []).append((old, splat))
        for fn, pairs in by_fn.items():
            fblock = tu_obj.by_name.get(fn)
            if fblock is None:
                continue
            ftext = text[fblock.start : fblock.end]
            new_f = ftext
            for old, splat in pairs:
                new_f = re.sub(rf"\b{re.escape(old)}\b", splat, new_f)
                new_f = new_f.replace(f"/* storage: {old} */", f"/* storage: {splat} */")
            if new_f != ftext:
                text = text[: fblock.start] + new_f + text[fblock.end :]
                tu_obj = tu_edit.TU(text)

    # Asm aliases for shifted initialized locals (name.N = name.K).
    alias_lines: list[str] = []
    for p in planned:
        if p["kind"] != "init":
            continue
        n, k = p["n"], p["k"]
        if n == k:
            continue
        # ee-as: `head.1 = head.0` (`.set` rejects dotted names).
        alias_lines.append(f'__asm__("{p["name"]}.{n} = {p["name"]}.{k}");\n')
        changed.append(p["splat"])

    if extern_lines or alias_lines:
        seen_e: set[str] = set()
        uniq = []
        for line in extern_lines:
            if line not in seen_e:
                seen_e.add(line)
                uniq.append(line)
        insert_at = 0
        for m in re.finditer(r'(?m)^#include\s+[<"].*$', text):
            insert_at = m.end()
        while insert_at < len(text) and text[insert_at] == "\n":
            insert_at += 1
            break
        block = "".join(uniq)
        if block:
            block += "\n"
        text = text[:insert_at] + block + text[insert_at:]
        if alias_lines:
            if not text.endswith("\n"):
                text += "\n"
            text += "\n" + "".join(dict.fromkeys(alias_lines))

    if changed:
        src.write_text(text)
    seen: set[str] = set()
    return [x for x in changed if not (x in seen or seen.add(x))]


def mirror_includes(unit: str, rels: list[str]) -> None:
    for rel in rels:
        if not rel.startswith("include/"):
            continue
        src = ROOT / rel
        if not src.is_file():
            continue
        mirror = ROOT / "build" / unit / rel
        mirror.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, mirror)
    # Keep build/<unit>/include/shared.h in sync — migrates often need additive
    # shared typedefs (JavaClass, f32, SceneClass.instance_size) that live only
    # in the repo include/ tree until the next full configure.
    shared = ROOT / "include" / "shared.h"
    if shared.is_file():
        dest = ROOT / "build" / unit / "include" / "shared.h"
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(shared, dest)


def compile_tu_object(unit: str, tu: str) -> dict:
    """Force rebuild of build/c/<tu>.o so derive sees the new C."""
    obj = f"build/c/{tu}.o"
    # ninja paths are relative to build/<unit>
    p = run(["ninja", "-C", f"build/{unit}", obj])
    return {
        "rc": p.returncode,
        "tail": ((p.stdout or "") + (p.stderr or ""))[-800:],
    }


def _scaffold_local_funcs(unit: str, tu: str) -> list[dict]:
    """LOCAL / dotted original names for this TU (scaffold-labels + objects sizes)."""
    path = ROOT / "build" / unit / "scaffold-labels.json"
    if not path.is_file():
        return []
    sizes: dict[str, int] = {}
    objs_path = ROOT / "config" / "objects" / f"{unit}.objects.json"
    if objs_path.is_file():
        objs = json.loads(objs_path.read_text())
        for t in objs.get("tus") or []:
            if t.get("name") != tu:
                continue
            for f in t.get("functions") or []:
                n = f.get("name") or ""
                if "." in n and f.get("size") is not None:
                    sizes[n] = int(f["size"])
    out = []
    for x in json.loads(path.read_text()):
        if x.get("tu") != tu:
            continue
        name = x.get("name") or ""
        if "." not in name:
            continue
        row = dict(x)
        if row.get("size") is None and name in sizes:
            row["size"] = sizes[name]
        out.append(row)
    return out


def _nm_text_symbols(unit: str, tu: str) -> dict[str, str]:
    """Map symbol -> type char (t/T) from the TU's .o."""
    obj = ROOT / "build" / unit / "build" / "c" / f"{tu}.o"
    if not obj.is_file():
        obj = ROOT / "build" / unit / "c" / f"{tu}.o"
    if not obj.is_file():
        return {}
    p = run(["nm", str(obj)])
    syms: dict[str, str] = {}
    for line in (p.stdout or "").splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        # address type name
        syms[parts[2]] = parts[1]
    return syms


def alias_nested_locals(unit: str, tu: str) -> list[str]:
    """Alias gcc nested-function counters onto original ELF dotted names.

    Upstream nested `void subListChange(void) {…}` often emits `subListChange.0`
    while the original ELF used `subListChange.11`. Mapcheck requires the C
    object to define the original spelling. Append
    `__asm__("subListChange.11 = subListChange.0");` when the .o has a dotted
    sibling for the same basename and is missing the expected name.
    """
    expected = _scaffold_local_funcs(unit, tu)
    if not expected:
        return []
    syms = _nm_text_symbols(unit, tu)
    if not syms:
        return []

    # basename -> sorted emitted counters present in the .o
    emitted: dict[str, list[int]] = {}
    for sym in syms:
        m = re.fullmatch(r"([A-Za-z_][A-Za-z0-9_]*)\.(\d+)", sym)
        if not m:
            continue
        emitted.setdefault(m.group(1), []).append(int(m.group(2)))
    for v in emitted.values():
        v.sort()

    alias_lines: list[str] = []
    changed: list[str] = []
    for x in expected:
        name = x["name"]  # e.g. subListChange.11
        m = re.fullmatch(r"([A-Za-z_][A-Za-z0-9_]*)\.(\d+)", name)
        if not m:
            continue
        base, want_n = m.group(1), int(m.group(2))
        if name in syms:
            continue  # already correct
        cands = emitted.get(base) or []
        if not cands:
            continue
        # Prefer a counter that is not itself an expected original name.
        expected_ns: set[int] = set()
        for e in expected:
            em = re.fullmatch(rf"{re.escape(base)}\.(\d+)", e["name"])
            if em:
                expected_ns.add(int(em.group(1)))
        k = next((c for c in cands if c not in expected_ns), cands[0])
        if k == want_n:
            continue
        size = x.get("size")
        if size is None:
            # scaffold-labels may omit size; fall back to objects.json via nm -S on emittee
            size = _nm_symbol_size(unit, tu, f"{base}.{k}")
        if not size:
            continue
        # Plain `name.N = name.K` yields size 0; mapcheck needs the ELF size.
        alias_lines.append(f'__asm__(".equ {base}.{want_n}, {base}.{k}");\n')
        alias_lines.append(f'__asm__(".size {base}.{want_n}, {int(size)}");\n')
        changed.append(name)

    if not alias_lines:
        return []

    src = ROOT / "src" / unit / f"{tu}.c"
    text = src.read_text()
    # Skip if this basename alias block already present.
    fresh = [ln for ln in alias_lines if ln.strip() not in text]
    if not fresh:
        return []
    if not text.endswith("\n"):
        text += "\n"
    text += "\n/* nested-function ELF name aliases (migrate_tu) */\n"
    text += "".join(dict.fromkeys(fresh))
    src.write_text(text)
    return changed


def _nm_symbol_size(unit: str, tu: str, sym: str) -> int | None:
    obj = ROOT / "build" / unit / "build" / "c" / f"{tu}.o"
    if not obj.is_file():
        obj = ROOT / "build" / unit / "c" / f"{tu}.o"
    if not obj.is_file():
        return None
    p = run(["nm", "-S", str(obj)])
    for line in (p.stdout or "").splitlines():
        parts = line.split()
        # addr size type name
        if len(parts) >= 4 and parts[3] == sym:
            try:
                return int(parts[1], 16)
            except ValueError:
                return None
    return None


def _nm_sized_symbols(unit: str, tu: str) -> dict[str, tuple[str, int]]:
    """Map symbol -> (type_char, size) from nm -S."""
    obj = ROOT / "build" / unit / "build" / "c" / f"{tu}.o"
    if not obj.is_file():
        obj = ROOT / "build" / unit / "c" / f"{tu}.o"
    if not obj.is_file():
        return {}
    p = run(["nm", "-S", str(obj)])
    out: dict[str, tuple[str, int]] = {}
    for line in (p.stdout or "").splitlines():
        parts = line.split()
        if len(parts) == 4:
            # addr size type name
            try:
                out[parts[3]] = (parts[2], int(parts[1], 16))
            except ValueError:
                continue
        elif len(parts) == 3:
            # addr type name (no size)
            out[parts[2]] = (parts[1], 0)
    return out


def inject_pad_data_view(unit: str, tu: str) -> list[str]:
    """If the TU uses `PadData.half_*` without a decl, inject a local Pad view.

    Upstream menu_shop (and similar) reads half_2a/half_32/half_34 but does not
    include party.h / a Pad typedef. Add a TU-local struct + extern into the
    colocated .h (or the .c) so ee-gcc can compile.
    """
    src = ROOT / "src" / unit / f"{tu}.c"
    if not src.is_file():
        return []
    text = src.read_text()
    if "PadData" not in text:
        return []
    # Already declared somewhere in the TU's own sources?
    hdr = ROOT / "src" / unit / f"{tu}.h"
    blob = text + (hdr.read_text() if hdr.is_file() else "")
    if re.search(r"\bextern\s+\w[\w\s\*]*\bPadData\b", blob) or re.search(
        r"\b(?:PadPrefix|ShopPadData|SkillPadData|TSliderPadData)\s+PadData\b", blob
    ):
        return []
    # Which halfwords are touched?
    halves = sorted({int(m, 16) for m in re.findall(r"PadData\.half_([0-9a-fA-F]+)", text)})
    if not halves:
        return []
    max_off = max(halves)
    # Build a packed view covering through the highest halfword used.
    # half_N means offset N; need bytes through N+2.
    pad_bytes = max_off
    fields = [f"    unsigned char pad_00[0x{pad_bytes:x}];"]
    # Emit every touched halfword; fill gaps as pad if sparse.
    # Simpler: one pad to first half, then consecutive u16s through max.
    first = min(halves)
    fields = [f"    unsigned char pad_00[0x{first:x}];"]
    for off in range(first, max_off + 2, 2):
        fields.append(f"    unsigned short half_{off:x}; /* +0x{off:x} */")
    block = (
        "\n/* Auto-injected PadData view (migrate_tu) — splat owns the object. */\n"
        "typedef struct MigratePadData {\n"
        + "\n".join(fields)
        + "\n} MigratePadData;\n"
        "extern MigratePadData PadData;\n"
    )
    target = hdr if hdr.is_file() else src
    t = target.read_text()
    if "MigratePadData" in t:
        return []
    if target == hdr and "#endif" in t:
        t = t.replace("#endif", block + "\n#endif", 1)
    else:
        # Insert after includes in .c
        insert_at = 0
        for m in re.finditer(r'(?m)^#include\s+[<"].*$', t):
            insert_at = m.end()
        t = t[:insert_at] + "\n" + block + t[insert_at:]
    target.write_text(t)
    return [f"PadData through half_{max_off:x}"]


def rewrite_splat_friendly_defines(unit: str, tu: str) -> list[str]:
    """Rewrite `#define splat_name friendly` + `static T friendly` into splat storage.

    Upstream sometimes keeps a readable C name and aliases the splat spelling
    via `#define image_004DC554 radarImage` while defining `static u8 *radarImage`.
    Mapcheck / derive need the splat (or original) symbol in the .o, so rename
    the decl to the splat spelling and drop the define.
    """
    src = ROOT / "src" / unit / f"{tu}.c"
    text = src.read_text()
    sizes = known_symbol_sizes(unit)
    changed: list[str] = []
    # #define SPLAT friendly   where SPLAT is a known data symbol
    define_pat = re.compile(
        r"(?m)^#define\s+([A-Za-z_][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*$"
    )
    renames: list[tuple[str, str]] = []  # friendly -> splat
    for m in define_pat.finditer(text):
        splat, friendly = m.group(1), m.group(2)
        if splat not in sizes:
            # also accept when friendly is unknown but splat looks like image_VA / D_
            if not (splat.startswith("D_") or splat.startswith("image_") or splat.startswith("jtbl_")):
                continue
        if splat == friendly:
            continue
        renames.append((friendly, splat))
        changed.append(f"{friendly}->{splat}")
    if not renames:
        return []
    friendly_to_splat = dict(renames)
    # Drop matching #define lines.
    def _keep_line(line: str) -> bool:
        m = define_pat.match(line)
        if not m:
            return True
        return (m.group(2), m.group(1)) not in renames

    text = "\n".join(ln for ln in text.splitlines() if _keep_line(ln))
    if not text.endswith("\n"):
        text += "\n"
    for friendly, splat in renames:
        text = re.sub(
            rf"\bstatic\s+((?:(?:const|volatile|unsigned|signed)\s+)*\w+(?:\s*\*)*)\s+{re.escape(friendly)}\b",
            rf"static \1 {splat}",
            text,
        )
        text = re.sub(rf"\b{re.escape(friendly)}\b", splat, text)
    src.write_text(text)
    return changed


def alias_splat_underscore_to_dot(unit: str, tu: str) -> list[str]:
    """Alias C `name_N` data/text locals onto original ELF `name.N`.

    Splat rewrites `waitHandlerTbl.0` → `waitHandlerTbl_0`. Upstream C often
    keeps the splat spelling; mapcheck still requires the dotted original name
    in the C object when that TU owns the piece.
    """
    sizes = known_symbol_sizes(unit)
    syms = _nm_sized_symbols(unit, tu)
    if not syms:
        return []
    alias_lines: list[str] = []
    changed: list[str] = []
    for splat, (ty, sz) in syms.items():
        m = re.fullmatch(r"([A-Za-z_][A-Za-z0-9_]*)_(\d+)", splat)
        if not m:
            continue
        if splat not in sizes and not sz:
            continue
        base, n = m.group(1), m.group(2)
        dotted = f"{base}.{n}"
        if dotted in syms:
            continue
        # Only alias storage / locals (not undefined).
        if ty not in set("dDbBrRTt"):
            continue
        use_sz = sz or sizes.get(splat) or 0
        if not use_sz:
            continue
        alias_lines.append(f'__asm__(".equ {dotted}, {splat}");\n')
        alias_lines.append(f'__asm__(".size {dotted}, {int(use_sz)}");\n')
        changed.append(dotted)
    if not alias_lines:
        return []
    src = ROOT / "src" / unit / f"{tu}.c"
    text = src.read_text()
    fresh = [ln for ln in alias_lines if ln.strip() not in text]
    if not fresh:
        return []
    if not text.endswith("\n"):
        text += "\n"
    text += "\n/* splat _N → ELF name.N aliases (migrate_tu) */\n"
    text += "".join(dict.fromkeys(fresh))
    src.write_text(text)
    return changed


def migrate_one(unit: str, tu: str, ref: str) -> dict:
    report: dict = {"unit": unit, "tu": tu, "ref": ref}
    if not (ROOT / "src" / unit / f"{tu}.c").is_file():
        report.update(ok=False, error="local TU missing")
        return report
    if not git_exists(ref, f"src/{unit}/{tu}.c"):
        report.update(ok=False, error="upstream TU missing")
        return report

    rels = tu_paths(unit, tu, ref)
    report["paths"] = rels

    with locked():
        backup_paths(unit, tu, rels)
        written = []
        for rel in rels:
            text = git_show(ref, rel)
            if text is None:
                continue
            dest = ROOT / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(text)
            written.append(rel)
        report["written"] = written
        mirror_includes(unit, written)
        report["splat_defines"] = rewrite_splat_friendly_defines(unit, tu)
        report["pad_inject"] = inject_pad_data_view(unit, tu)
        report["reorder_inits"] = reorder_static_inits_by_va(unit, tu)
        report["externized"] = externize_known_statics(unit, tu)
        report["carve_drop"] = drop_carve(unit, tu)

        # Fresh object → name aliases → derive → ninja → rodata fix → verify
        refresh_ninja(unit)
        cc = compile_tu_object(unit, tu)
        report["compile"] = {"rc": cc["rc"], "tail": cc["tail"][-500:]}
        if cc["rc"] != 0:
            restore_backup(unit, tu)
            mirror_includes(unit, rels)
            refresh_ninja(unit)
            rebuild(ROOT, unit)
            report.update(ok=False, reverted=True, error="compile failed")
            log(f"MIGRATE_FAIL {unit}/{tu} compile")
            return report

        nested = alias_nested_locals(unit, tu)
        dotted = alias_splat_underscore_to_dot(unit, tu)
        report["nested_aliases"] = nested
        report["splat_dot_aliases"] = dotted
        if nested or dotted:
            cc2 = compile_tu_object(unit, tu)
            report["compile_alias"] = {"rc": cc2["rc"], "tail": cc2["tail"][-500:]}
            if cc2["rc"] != 0:
                restore_backup(unit, tu)
                mirror_includes(unit, rels)
                refresh_ninja(unit)
                rebuild(ROOT, unit)
                report.update(ok=False, reverted=True, error="compile failed after name alias")
                log(f"MIGRATE_FAIL {unit}/{tu} compile_alias")
                return report

        derived = derive_carve(unit, tu)
        problems = derived.get("problems") or []
        # written=False with no problems means "no local data / carve unchanged-empty"
        # (e.g. everything externized). That is success for derive.
        derive_ok = (not problems) and not derived.get("error")
        if "ok" in derived:
            derive_ok = bool(derived.get("ok")) and derive_ok
        report["derive"] = {
            "ok": derive_ok,
            "written": derived.get("written"),
            "problems": problems[:5],
            "error": (derived.get("error") or None),
        }
        if not derive_ok:
            # Fallback: restore pre-migrate carve (asm owns data via existing
            # splits), strip C D_*/jtbl_* so the .o does not enlarge those
            # runs, recompile, verify. If C still emits section data that
            # disagrees with the carve, verify fails and we revert.
            restore_carve_slice(unit, tu)
            stripped = externize_d_rodata_objects(unit, tu)
            stripped2 = externize_known_statics(unit, tu)
            report["derive_fallback_extern_d"] = stripped
            report["derive_fallback_externize"] = stripped2
            refresh_ninja(unit)
            cc3 = compile_tu_object(unit, tu)
            report["derive_fallback_compile"] = {"rc": cc3["rc"], "tail": cc3["tail"][-400:]}
            if cc3["rc"] != 0:
                restore_backup(unit, tu)
                mirror_includes(unit, rels)
                for p in (
                    ROOT / "build" / unit / "build" / "c" / f"{tu}.o",
                    ROOT / "build" / unit / "build" / "c" / f"{tu}.carved.o",
                    ROOT / "build" / unit / "c" / f"{tu}.o",
                    ROOT / "build" / unit / "c" / f"{tu}.carved.o",
                ):
                    if p.is_file():
                        p.unlink()
                refresh_ninja(unit)
                rebuild(ROOT, unit)
                report.update(ok=False, reverted=True, error="derive fallback compile failed")
                log(f"MIGRATE_FAIL {unit}/{tu} derive_fallback_cc")
                return report
            report["derive_fallback"] = True
        refresh_ninja(unit)
        report["fix_rodata"] = fix_rodata()
        v = rebuild(ROOT, unit)
        report["verify"] = {
            "identical": v.get("identical"),
            "mapcheck_ok": v.get("mapcheck_ok"),
            "diff_range_count": v.get("diff_range_count"),
            "ninja_rc": v.get("ninja_rc"),
            "error": v.get("error"),
            "diffs": (v.get("diffs") or [])[:6],
            "mapcheck_problems": (v.get("mapcheck_problems") or [])[:6],
            "ninja_tail": (v.get("ninja_tail") or "")[-600:],
        }
        ok = bool(v.get("identical")) and (unit != "main" or v.get("mapcheck_ok", True))
        if not ok:
            restore_backup(unit, tu)
            mirror_includes(unit, rels)
            # Drop objects built from the upstream TU so the next build
            # cannot pair a stale .o with the restored carve registry.
            for p in (
                ROOT / "build" / unit / "build" / "c" / f"{tu}.o",
                ROOT / "build" / unit / "build" / "c" / f"{tu}.carved.o",
                ROOT / "build" / unit / "c" / f"{tu}.o",
                ROOT / "build" / unit / "c" / f"{tu}.carved.o",
            ):
                if p.is_file():
                    p.unlink()
            refresh_ninja(unit)
            rebuild(ROOT, unit)
            report.update(ok=False, reverted=True)
            log(f"MIGRATE_FAIL {unit}/{tu} reverted")
            return report
        report.update(ok=True, reverted=False)
        log(f"MIGRATE_OK {unit}/{tu} wrote={written}")
        return report


def tus_from_rank(path: Path, min_up_only: int, max_local_only: int) -> list[str]:
    rows = json.loads(path.read_text())
    return [
        r["tu"]
        for r in rows
        if r.get("n_up_only", 0) >= min_up_only and r.get("n_local_only", 99) <= max_local_only
    ]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", default="main")
    ap.add_argument("--tu", action="append", default=[])
    ap.add_argument("--ref", default=REF_DEFAULT)
    ap.add_argument("--from-rank", type=Path)
    ap.add_argument("--min-up-only", type=int, default=3)
    ap.add_argument("--max-local-only", type=int, default=0)
    ap.add_argument("--limit", type=int, default=0)
    a = ap.parse_args()
    tus = list(a.tu)
    if a.from_rank:
        tus.extend(tus_from_rank(a.from_rank, a.min_up_only, a.max_local_only))
    seen = set()
    tus = [t for t in tus if not (t in seen or seen.add(t))]
    if a.limit:
        tus = tus[: a.limit]
    if not tus:
        ap.error("pass --tu and/or --from-rank")

    results = []
    for tu in tus:
        print(f"=== migrating {a.unit}/{tu} ===", flush=True)
        r = migrate_one(a.unit, tu, a.ref)
        results.append(r)
        summary = {k: r.get(k) for k in ("tu", "ok", "reverted", "written", "error")}
        print(json.dumps(summary, indent=1), flush=True)
        if not r.get("ok"):
            v = r.get("verify") or {}
            print(
                f"  FAIL compile_rc={((r.get('compile') or {}).get('rc'))} "
                f"identical={v.get('identical')} mapcheck={v.get('mapcheck_ok')} "
                f"diffs={v.get('diff_range_count')}",
                flush=True,
            )
            if (r.get("compile") or {}).get("tail"):
                print("  compile tail:", r["compile"]["tail"][-400:], flush=True)
            if v.get("diffs"):
                print("  diffs:", v["diffs"][:3], flush=True)
            if v.get("mapcheck_problems"):
                print("  mapcheck:", v["mapcheck_problems"][:3], flush=True)
            if v.get("ninja_tail"):
                print("  ninja:", v["ninja_tail"][-400:], flush=True)

    out = {
        "ok": all(r.get("ok") for r in results),
        "migrated": [r["tu"] for r in results if r.get("ok")],
        "failed": [r["tu"] for r in results if not r.get("ok")],
        "results": results,
    }
    out_path = ROOT / "build" / "tu_migrate.results.json"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(out, indent=1) + "\n")
    print(json.dumps({"ok": out["ok"], "migrated": out["migrated"], "failed": out["failed"]}, indent=1))
    return 0 if out["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Project-wide function prototype index for m2c drafts.

    .venv/bin/python -B scripts/auto_decomp/proto_index.py      # -> build/auto_decomp/protos.json

m2c only sees the TU's own declarations, so a callee defined in another TU
comes out as `? Foo(s32, void *, ?)` and its arguments and return value are
typed by guesswork. This parses every TU's preprocessed source (the .i the
build writes, sanitized like m2c_seed's context) and records each function's
prototype, rewritten to stand alone in any TU: typedefs are resolved to their
base type, any struct/union/enum pointer becomes `void *` (m2c still learns
the argument count, widths, signedness and the return type). Prototypes whose
types cannot be made self-contained (a struct by value) are skipped.

It also writes each TU's sanitized declarations to build/ghidra/types/<unit>/
<tu>.h, which scripts/ghidra/export.sh imports so Ghidra knows the real
structs, prototypes and global types.

When several TUs declare a function, the declaration from the TU that defines
it wins, then the most common spelling.
"""
from __future__ import annotations

import collections
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import m2c_seed  # noqa: E402
from m2c_pycparser import c_ast, c_generator, c_parser  # noqa: E402

OUT = ROOT / "build/auto_decomp/protos.json"
TYPES = ROOT / "build/ghidra/types"            # each TU's declarations, for Ghidra
BASIC = {"void", "char", "short", "int", "long", "float", "double", "signed", "unsigned"}


class Unresolvable(Exception):
    pass


def typedef_table(ast: c_ast.FileAST) -> dict[str, c_ast.Node]:
    return {n.name: n.type for n in ast.ext if isinstance(n, c_ast.Typedef)}


def resolve(t: c_ast.Node, tds: dict, depth: int = 0) -> c_ast.Node:
    """Rewrite type `t` with only basic types: typedefs inlined, aggregates behind
    a pointer turned into void. Raises Unresolvable for aggregates by value."""
    if depth > 40:
        raise Unresolvable
    if isinstance(t, c_ast.PtrDecl):
        try:
            inner = resolve(t.type, tds, depth + 1)
        except Unresolvable:
            inner = c_ast.TypeDecl(None, [], None, c_ast.IdentifierType(["void"]))
        return c_ast.PtrDecl(t.quals, inner)
    if isinstance(t, c_ast.ArrayDecl):          # parameter arrays decay
        try:
            inner = resolve(t.type, tds, depth + 1)
        except Unresolvable:
            inner = c_ast.TypeDecl(None, [], None, c_ast.IdentifierType(["void"]))
        return c_ast.PtrDecl([], inner)
    if isinstance(t, c_ast.FuncDecl):
        args = None
        if t.args:
            args = c_ast.ParamList([p if isinstance(p, c_ast.EllipsisParam)
                                    else c_ast.Typename(None, [], None, None, resolve(p.type, tds, depth + 1))
                                    for p in t.args.params])
        return c_ast.FuncDecl(args, resolve(t.type, tds, depth + 1))
    if isinstance(t, c_ast.TypeDecl):
        inner = t.type
        if isinstance(inner, c_ast.IdentifierType):
            names = inner.names
            if all(n in BASIC for n in names):
                return c_ast.TypeDecl(None, [], None, c_ast.IdentifierType(list(names)))
            if len(names) == 1 and names[0] in tds:
                return resolve(tds[names[0]], tds, depth + 1)
            raise Unresolvable
        if isinstance(inner, c_ast.Enum):
            return c_ast.TypeDecl(None, [], None, c_ast.IdentifierType(["int"]))
        raise Unresolvable                       # struct/union by value
    if isinstance(t, c_ast.Typename):
        return resolve(t.type, tds, depth + 1)
    raise Unresolvable


def render(name: str, fdecl: c_ast.FuncDecl) -> str:
    def named(t):
        if isinstance(t, c_ast.TypeDecl):
            return c_ast.TypeDecl(name, [], None, t.type)
        if isinstance(t, (c_ast.PtrDecl, c_ast.FuncDecl)):
            return type(t)(*([t.quals, named(t.type)] if isinstance(t, c_ast.PtrDecl) else [t.args, named(t.type)]))
        return t
    d = c_ast.Decl(name, [], [], [], [], None, named(fdecl), None, None, None)
    return c_generator.CGenerator().visit(d) + ";"


def global_typedefs(ast: c_ast.FileAST) -> str:
    """`typedef T __gvar_<name>;` per object declaration: Ghidra's C parser drops
    extern objects, so ImportTypes.java reads their types from these."""
    gen, out = c_generator.CGenerator(), []
    for n in ast.ext:
        if not isinstance(n, c_ast.Decl) or not n.name or isinstance(n.type, c_ast.FuncDecl):
            continue
        def rename(t):
            if isinstance(t, c_ast.TypeDecl):
                return c_ast.TypeDecl(f"__gvar_{n.name}", t.quals, t.align, t.type)
            if isinstance(t, c_ast.ArrayDecl):
                return c_ast.ArrayDecl(rename(t.type), t.dim, t.dim_quals)
            if isinstance(t, c_ast.PtrDecl):
                return c_ast.PtrDecl(t.quals, rename(t.type))
            raise Unresolvable
        try:
            out.append(gen.visit(c_ast.Typedef(name=f"__gvar_{n.name}", quals=[], storage=["typedef"], gcc_attributes=[], type=rename(n.type))) + ";")
        except Exception:
            continue
    return "\n/* object types for Ghidra (ImportTypes.java) */\n" + "\n".join(out) + "\n" if out else ""


def tus():
    for src in sorted((ROOT / "src").glob("*/*.c")):
        yield src.parent.name, src.stem, src


def main() -> int:
    parser = c_parser.CParser()
    seen: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    owner: dict[str, str] = {}
    stats = collections.Counter()
    for unit, tu, src in tus():
        ctx = m2c_seed.context_file(unit, tu)
        if not ctx:
            stats["no .i"] += 1
            continue
        decls = m2c_seed.sanitize(ctx.read_text(errors="replace"))
        hdr = TYPES / unit / f"{tu}.h"          # scripts/ghidra/ImportTypes.java
        hdr.parent.mkdir(parents=True, exist_ok=True)
        hdr.write_text(decls)
        try:
            ast = parser.parse(decls)
        except Exception:
            stats["parse failed"] += 1
            continue
        stats["parsed"] += 1
        hdr.write_text(decls + global_typedefs(ast))
        tds = typedef_table(ast)
        defined = set(re.findall(r"^[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{",
                                 src.read_text(errors="replace"), re.M))
        for n in ast.ext:
            if not (isinstance(n, c_ast.Decl) and isinstance(n.type, c_ast.FuncDecl) and n.name):
                continue
            try:
                text = render(n.name, resolve(n.type, tds))
            except Unresolvable:
                stats["unresolvable"] += 1
                continue
            seen[n.name][text] += 1
            if n.name in defined:
                owner[n.name] = text
    protos = {name: owner.get(name) or c.most_common(1)[0][0] for name, c in sorted(seen.items())}
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(protos, indent=0, sort_keys=True))
    print(json.dumps({**stats, "prototypes": len(protos), "from_definition": len(owner),
                      "out": str(OUT.relative_to(ROOT))}))
    return 0


if __name__ == "__main__":
    sys.exit(main())

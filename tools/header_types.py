#!/usr/bin/env python3
"""Shared type registry: one canonical definition per shared struct, and additive member proposals.

A struct/union another TU needs has exactly one definition: in the defining TU's
own header (`src/<unit>/<tu>.h`, published as `include/<unit>/<tu>.h` by
tools/header_harvest.py) or in `include/shared.h` when no TU owns it
(include/README.md). A worker who needs a member the canonical definition does not
name yet must not restate the type (a second view: the review preflight's
`redefinition` check refuses it); it proposes an additive extension instead, and
the integrator applies the reviewed proposal to the owner's file.

    python3 -B tools/header_types.py restated [--json OUT]
    python3 -B tools/header_types.py propose --type NAME --file OWNER_FILE \\
        --replace 'unsigned char unmodeled_08[0x18];' --with 'u32 flags;' \\
        --with 'unsigned char unmodeled_0c[0x14];' --evidence 'main 0x00212340 lw $2,0x8($4)' \\
        --by WORKER -o PROPOSAL.json
    python3 -B tools/header_types.py check PROPOSAL.json [--root ROOT]
    python3 -B tools/header_types.py apply PROPOSAL.json [--root ROOT]

`restated` lists every struct/union/typedef body that more than one published TU
declares for itself (identical copies and divergent views), with the owner
`config/header-types.json` records for it, if any. A proposal
(`header-type-proposal/1`) replaces one run of member declarations of the owner's
body with another run of exactly the same byte size (a padding span split into a
named member and the remaining padding, typically). `check` refuses a proposal
whose owner file's code moved (`base_code_sha256`, the owner text without comments
and whitespace; `base_sha256` is kept as provenance), whose replaced text is not in the body
(compared without comments and whitespace, so a comment-only edit of the owner
never invalidates a proposal), which renames or removes a named member, or whose
sizes cannot be proven equal. `apply` writes the owner file and prints its new
SHA-256; the integrator then runs the harvest and the whole-file gate as for any
published header edit. Nothing here changes a byte of any image by itself.
"""
import argparse
import hashlib
import importlib.util
import json
import os
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
TYPES_FILE = 'config/header-types.json'
PROPOSAL_SCHEMA = 'header-type-proposal/1'
PAD_RE = re.compile(r'^(?:unmodeled|pad|unk|reserved|_)\w*$', re.I)


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, str(path))
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def strip_comments(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*', ' ', text, flags=re.S)


def norm(text):
    return ' '.join(strip_comments(text).split())


def load_registry(root):
    path = Path(root) / TYPES_FILE
    if not path.is_file():
        path = Path(root) / 'tools/header_inputs' / TYPES_FILE
    if not path.is_file():
        return {}
    doc = json.loads(path.read_text(encoding='utf-8'))
    if doc.get('schema_version') != 1:
        raise SystemExit(f'{TYPES_FILE}: schema_version must be 1')
    return doc.get('types') or {}


# ---------------------------------------------------------------------------
# restatements
# ---------------------------------------------------------------------------

def restated(root):
    hh = _load('xeno_hh_types', Path(root) / 'tools/header_harvest.py')
    registry = load_registry(root)
    views = {}
    for tu in hh.published_tus(str(root)):
        for key, canon in tu.local_canon.items():
            if key[0] == 'ordinary':
                continue
            c = hh.normalize_ws(canon)
            # a forward `typedef struct X X;` or `struct X;` is a name, not a view
            if '{' not in c:
                continue
            name = key[1].split()[-1]
            views.setdefault(name, {}).setdefault(c, []).append(tu.tu)
    rows = []
    for name, spellings in sorted(views.items()):
        tus = sorted({t for ts in spellings.values() for t in ts})
        if len(tus) < 2:
            continue
        rows.append(dict(name=name, tus=tus, views=len(spellings), divergent=len(spellings) > 1,
                         units=sorted({t.split('/')[0] for t in tus}),
                         owner=(registry.get(name) or {}).get('owner'),
                         spellings=[dict(tus=sorted(ts), canon=c[:400]) for c, ts in sorted(
                             spellings.items(), key=lambda kv: -len(kv[1]))]))
    return rows


# ---------------------------------------------------------------------------
# proposals
# ---------------------------------------------------------------------------

def find_body(text, type_name):
    """(start, stop) of the member text between the braces of TYPE's definition, or None.

    Matches `struct TYPE {`, `union TYPE {` and `typedef struct [TAG] { ... } TYPE;`."""
    # comments blanked in place, so offsets stay aligned with `text`
    code = re.sub(r'/\*.*?\*/|//[^\n]*', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text, flags=re.S)
    candidates = []
    for m in re.finditer(r'\b(?:struct|union)\s+' + re.escape(type_name) + r'\s*\{', code):
        candidates.append(m.end())
    for m in re.finditer(r'\btypedef\s+(?:struct|union)\s*(?:\w+\s*)?\{', code):
        depth, i = 1, m.end()
        while i < len(code) and depth:
            depth += {'{': 1, '}': -1}.get(code[i], 0)
            i += 1
        if re.match(r'\s*' + re.escape(type_name) + r'\s*;', code[i:]):
            candidates.append(m.end())
    if len(set(candidates)) != 1:
        return None
    start = candidates[0]
    depth, i = 1, start
    while i < len(code) and depth:
        depth += {'{': 1, '}': -1}.get(code[i], 0)
        i += 1
    return start, i - 1


def members(text):
    return [p.strip() for p in strip_comments(text).split(';') if p.strip()]


def member_name(decl):
    m = re.search(r'([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*$', decl)
    return m.group(1) if m else None


def check(proposal, root):
    """List of problems (empty: the proposal is additive and applies)."""
    tu_edit = _load('xeno_tu_edit_types', Path(root) / 'tools/tu_edit.py')
    problems = []
    if proposal.get('schema') != PROPOSAL_SCHEMA:
        return [f'schema must be {PROPOSAL_SCHEMA}']
    for field in ('type', 'file', 'base_sha256', 'base_code_sha256', 'replace', 'with', 'evidence', 'proposed_by'):
        if not proposal.get(field):
            problems.append(f'missing `{field}`')
    if problems:
        return problems
    path = Path(root) / proposal['file']
    if not path.is_file():
        return [f'{proposal["file"]} does not exist']
    registry = load_registry(root)
    owner = (registry.get(proposal['type']) or {}).get('file')
    if owner and owner != proposal['file']:
        problems.append(f'{TYPES_FILE} records {owner} as the owner of {proposal["type"]}, not {proposal["file"]}')
    text = path.read_text(encoding='utf-8')
    if sha256(path) != proposal['base_sha256']:
        # a comment-only edit of the owner never invalidates a proposal (user rule
        # 2026-09-19): only a change of the code itself does
        if hashlib.sha256(norm(text).encode()).hexdigest() != proposal.get('base_code_sha256'):
            problems.append(f'{proposal["file"]} code moved since the proposal (base_code_sha256); re-propose '
                            f'on the current file')
    body = find_body(text, proposal['type'])
    if body is None:
        return problems + [f'{proposal["file"]} holds no single definition of {proposal["type"]}']
    old = members('\n'.join(proposal['replace']) if isinstance(proposal['replace'], list) else proposal['replace'])
    new = members('\n'.join(proposal['with']) if isinstance(proposal['with'], list) else proposal['with'])
    have = members(text[body[0]:body[1]])
    norm_have = [' '.join(m.split()) for m in have]
    norm_old = [' '.join(m.split()) for m in old]
    at = next((i for i in range(len(norm_have) - len(norm_old) + 1)
               if norm_have[i:i + len(norm_old)] == norm_old), None) if norm_old else None
    if at is None:
        problems.append('the replaced members are not a consecutive run of the current body')
    removed_names = {member_name(m) for m in old} - {member_name(m) for m in new}
    named = sorted(n for n in removed_names if n and not PAD_RE.match(n))
    if named:
        problems.append(f'not additive: removes or renames named member(s) {named}')
    size_old, size_new = tu_edit.members_size(old), tu_edit.members_size(new)
    if size_old is None or size_new is None:
        problems.append(f'sizes are not provable from the text (replaced {size_old}, new {size_new}); use '
                        f'fixed-size scalar/pointer/array members')
    elif size_old != size_new:
        problems.append(f'size changes: replaced {size_old:#x} bytes with {size_new:#x}')
    return problems


def apply(proposal, root):
    path = Path(root) / proposal['file']
    text = path.read_text(encoding='utf-8')
    start, stop = find_body(text, proposal['type'])
    body = text[start:stop]
    old = proposal['replace'] if isinstance(proposal['replace'], list) else [proposal['replace']]
    # locate the replaced run line-wise, comments allowed around it
    lines = body.split('\n')
    target = [norm(x) for x in old]
    code = [norm(l) for l in lines]
    idx = [i for i, c in enumerate(code) if c]
    for k in range(len(idx) - len(target) + 1):
        window = idx[k:k + len(target)]
        if [code[i] for i in window] == target:
            first, last = window[0], window[-1]
            indent = re.match(r'\s*', lines[first]).group(0)
            new = [indent + w.strip() if w.strip() else w for w in
                   (proposal['with'] if isinstance(proposal['with'], list) else [proposal['with']])]
            lines[first:last + 1] = new
            break
    else:
        raise SystemExit('the replaced run is not one member per line in the owner body; edit by hand')
    path.write_text(text[:start] + '\n'.join(lines) + text[stop:], encoding='utf-8')
    return sha256(path)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    r = sub.add_parser('restated')
    r.add_argument('--root', default=str(HERE.parent))
    r.add_argument('--json', dest='json_out')
    p = sub.add_parser('propose')
    p.add_argument('--root', default=str(HERE.parent))
    p.add_argument('--type', required=True)
    p.add_argument('--file', required=True, help='the owner file (repo-relative), e.g. src/main/<tu>.h')
    p.add_argument('--replace', action='append', required=True, help='one member declaration (repeatable)')
    p.add_argument('--with', dest='with_', action='append', required=True, help='replacement line (repeatable)')
    p.add_argument('--evidence', action='append', required=True)
    p.add_argument('--by', required=True)
    p.add_argument('-o', '--out', required=True)
    for name in ('check', 'apply'):
        c = sub.add_parser(name)
        c.add_argument('proposal')
        c.add_argument('--root', default=str(HERE.parent))
    a = ap.parse_args(argv)
    root = Path(a.root).resolve()
    if a.cmd == 'restated':
        rows = restated(root)
        if a.json_out:
            Path(a.json_out).write_text(json.dumps(rows, indent=1) + '\n', encoding='utf-8')
        for row in rows:
            print(f'{"DIVERGENT" if row["divergent"] else "IDENTICAL"} {row["name"]}: {row["views"]} view(s) in '
                  f'{", ".join(row["tus"])}' + (f' (owner {row["owner"]})' if row['owner'] else ''))
        print(f'OK restated={len(rows)} divergent={sum(1 for r in rows if r["divergent"])}')
        return 0
    if a.cmd == 'propose':
        proposal = dict(schema=PROPOSAL_SCHEMA, type=a.type, file=a.file, base_sha256=sha256(root / a.file),
                        base_code_sha256=hashlib.sha256(norm((root / a.file).read_text(encoding='utf-8'))
                                                        .encode()).hexdigest(),
                        replace=a.replace, **{'with': a.with_}, evidence=a.evidence, proposed_by=a.by)
        problems = check(proposal, root)
        Path(a.out).write_text(json.dumps(dict(proposal, check=problems), indent=1) + '\n', encoding='utf-8')
        for line in problems:
            print(f'PROBLEM {line}')
        print('OK' if not problems else f'{len(problems)} problem(s)')
        return 1 if problems else 0
    proposal = json.loads(Path(a.proposal).read_text(encoding='utf-8'))
    problems = check(proposal, root)
    for line in problems:
        print(f'PROBLEM {line}')
    if problems:
        return 1
    if a.cmd == 'check':
        print('OK additive, same size, applies to', proposal['file'])
        return 0
    print(f'applied: {proposal["file"]} sha256 {apply(proposal, root)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())

"""Express unpaired splat %hi/%lo operands symbolically in objdiff TARGET objects.

splat leaves `addiu $2, $10, 0xEE0` numeric when the matching `lui $10,
%hi(htypebuf)` is far away (hoisted by cc1), and leaves `lui $3, 0x48` /
`addiu $3, $3, -0x6580` numeric when it did not pair them at all. The
compiler object carries R_MIPS_HI16/LO16 at those sites, so native objdiff
counts a metadata-only mismatch on byte-identical functions.

This pass rewrites only a local report copy of the original disassembly:

* register flow is tracked linearly from `lui $r, %hi(SYM)` / `lui $r, IMM`
  until `$r` is written (or clobbered by a call after its delay slot);
* a numeric low operand on `$r` is symbolized only when hi<<16 + simm16 lies
  inside SYM (or, for a raw lui, inside exactly one named object of the
  verified linked ELF);
* the result is reassembled and must reproduce every allocated section byte,
  every function symbol and every pre-existing relocation; the only additions
  allowed are HI16/LO16 at sites that had no relocation before.

Symbol addresses come only from the byte-identical linked ELF, never from the
compiler (base) object.  Nothing in the build, src/ or scaffolding changes.
"""
from collections import Counter
import json
from pathlib import Path
import bisect
import re
import struct
import subprocess

from tu.elfinfo import Elf

INSN = re.compile(r'^(\s*/\*\s+[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s+\*/\s*)'
                  r'(\S+)(\s+)(.*?)\s*$')
REG = re.compile(r'^\$(\d+|[a-z]\w*)$')
MEM = re.compile(r'^(-?(?:0x[0-9A-Fa-f]+|\d+))\((\$\w+)\)$')
NUM = re.compile(r'^-?(?:0x[0-9A-Fa-f]+|\d+)$')
SHIFTED = re.compile(r'^\(0x[0-9A-Fa-f]+\s*>>\s*16\)$')
HI = re.compile(r'^%hi\(([A-Za-z_.$][\w.$]*)\)$')
NO_DEST = {'sb', 'sh', 'sw', 'sd', 'sq', 'swc1', 'sdc1', 'swl', 'swr', 'sdl', 'sdr', 'sqc2',
           'j', 'jr', 'b', 'bal', 'nop', 'sync', 'sync.p', 'break', 'syscall', 'teq',
           'mtc1', 'dmtc1', 'ctc1', 'mtc0', 'ctc2', 'qmtc2', 'mthi', 'mtlo', 'mthi1', 'mtlo1',
           'mtsa', 'mtsab', 'cache'}
CALL_CLOBBER = {*(f'${n}' for n in [*range(1, 16), 24, 25, 31])}
LOADSTORE = {'lb', 'lbu', 'lh', 'lhu', 'lw', 'lwu', 'ld', 'lq', 'lwc1', 'ldc1', 'lqc2',
             'sb', 'sh', 'sw', 'sd', 'sq', 'swc1', 'sdc1', 'sqc2'}


class SymbolTable:
    def __init__(self, elfs):
        self.by_name, starts, self.loaded = {}, {}, []
        for path in elfs:
            elf = Elf(Path(path).read_bytes())
            self.loaded += [(x.addr, x.addr + x.size) for x in elf.sections if x.flags & 2 and x.size]
            for s in elf.symbols:
                if (not s.name or s.type not in (0, 1) or s.shndx == 0 or '.' in s.name
                        or s.name.startswith(('$', '_gp'))):
                    continue
                if not self.is_loaded(s.value):
                    continue
                self.by_name.setdefault(s.name, set()).add((s.value, s.size))
                starts.setdefault(s.value, set()).add((s.name, s.size))
        self.starts = starts
        self.addresses = sorted(starts)

    def is_loaded(self, address):
        return any(lo <= address < hi for lo, hi in self.loaded)

    def named(self, name):
        found = self.by_name.get(name, set())
        return next(iter(found)) if len(found) == 1 else None

    def owner(self, address):
        """The single named symbol starting at, or (sized object) containing, address."""
        found = self._owner(address)
        sym = self.named(found[0]) if found else None
        return found if sym and sym[0] == found[1] else None

    def _owner(self, address):
        exact = self.starts.get(address, set())
        if exact:
            names = {n for n, _ in exact}
            return (next(iter(names)), address) if len(names) == 1 else None

        i = bisect.bisect_right(self.addresses, address) - 1
        if i < 0:
            return None
        start = self.addresses[i]
        names = {(n, z) for n, z in self.starts[start]}
        if len({n for n, _ in names}) != 1:
            return None
        name, size = next(iter(names))
        return (name, start) if start <= address < start + size else None


def lo_expr(name, offset):
    return f'%lo({name})' if offset == 0 else f'%lo({name} + 0x{offset:X})'


def simm(value):
    value &= 0xffff
    return value - 0x10000 if value & 0x8000 else value


LABEL = re.compile(r'^\s*(?:jlabel\s+)?(\.?L[0-9A-Za-z_]+):?\s*$')
LIKELY = {'beql', 'bnel', 'blezl', 'bgtzl', 'bltzl', 'bgezl', 'bc1tl', 'bc1fl', 'beqzl', 'bnezl'}
CALLS = {'jal', 'jalr', 'bal', 'bgezal', 'bltzal'}


def is_branch(mnem):
    return mnem in ('j', 'jr') or (mnem.startswith('b') and mnem not in ('break',))


def destination(mnem, operands):
    if mnem in NO_DEST or is_branch(mnem) or mnem.startswith('c.') or not operands:
        return None
    return operands[0] if REG.match(operands[0]) else None


def functions_of(lines):
    """Split a disassembly file into functions: lists of (line, insn-match) and labels."""
    current = None
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith('glabel'):
            current = dict(name=stripped.split()[1].rstrip(','), insns=[], labels={}, jlabels=[])
            yield current
            continue
        if current is None:
            continue
        if stripped.startswith(('endlabel', '.size', '.end')):
            current = None
            continue
        label = LABEL.match(line)
        if label:
            current['labels'][label[1]] = len(current['insns'])
            if stripped.startswith('jlabel'):
                current['jlabels'].append(len(current['insns']))
            continue
        m = INSN.match(line)
        if m:
            ops = [o.strip() for o in m[6].split(',')] if m[6] else []
            current['insns'].append((i, m, m[4], ops, int.from_bytes(bytes.fromhex(m[3]), 'little')))


def successors(fn):
    insns, labels = fn['insns'], fn['labels']
    n = len(insns)
    succ = [set() for _ in range(n)]
    clobber = [False] * n
    for k, (_, _, mnem, ops, _) in enumerate(insns):
        nxt = {k + 1} if k + 1 < n else set()
        if not is_branch(mnem) and mnem not in CALLS:
            if not (k > 0 and is_branch(insns[k - 1][2]) and insns[k - 1][2] not in CALLS):
                succ[k] |= nxt
            continue
        if mnem in CALLS:
            succ[k] |= nxt
            if k + 1 < n:
                succ[k + 1] |= {k + 2} if k + 2 < n else set()
                clobber[k + 1] = True
            continue
        target = labels.get(ops[-1]) if ops else None
        if mnem == 'jr':
            targets = set() if ops and ops[0] == '$31' else set(fn['jlabels'])
        elif target is None:
            targets = None                    # unknown destination: give up below
        else:
            targets = {target}
        unconditional = mnem in ('b', 'j', 'jr') or (mnem == 'beq' and len(ops) == 3
                                                    and ops[0] == ops[1])
        if targets is None:
            return None, None
        if k + 1 < n:
            succ[k] |= {k + 1}
            succ[k + 1] |= targets
            if not unconditional:
                if mnem in LIKELY:
                    succ[k] |= {k + 2} if k + 2 < n else set()
                else:
                    succ[k + 1] |= {k + 2} if k + 2 < n else set()
    return succ, clobber


def transfer(insn, state, clobber):
    i, _, mnem, ops, word = insn
    state = dict(state)
    if mnem == 'lui' and len(ops) == 2:
        h = HI.match(ops[1])
        state.pop(ops[0], None)
        if h or NUM.match(ops[1]) or SHIFTED.match(ops[1]):
            state[ops[0]] = (i, h[1] if h else None, word & 0xffff)
    else:
        d = destination(mnem, ops)
        if d:
            state.pop(d, None)
    if clobber:
        for r in CALL_CLOBBER:
            state.pop(r, None)
    return state


CONSTANT = re.compile(r'%(hi|lo)\((D_([0-9A-Fa-f]+))\)')


def numeric_constants(lines, symbols, edits):
    """`lui $3, %hi(D_FFFF)` / `addiu $3, $3, %lo(D_FFFF)` loads 65535, not an
    address: a D_<address> operand outside every loaded section of the linked
    ELF is a plain constant (hardware register, packed value). Keep the
    original immediate, so the target has no relocation the C cannot have."""
    func, func_va = None, None
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith('glabel'):
            func, func_va = stripped.split()[1].rstrip(','), None
            continue
        m = INSN.match(line)
        if not m:
            continue
        if func_va is None:
            func_va = int(m[2], 16)
        c = CONSTANT.search(m[6])
        if not c or symbols.is_loaded(int(c[3], 16)) or func is None:
            continue
        word = int.from_bytes(bytes.fromhex(m[3]), 'little')
        if word >> 26 in (0x0c, 0x0d, 0x0e, 0x0f):          # andi, ori, xori, lui
            immediate = f'0x{word & 0xffff:X}'
        else:
            immediate = str(simm(word))
        lines[i] = f'{m[1]}{m[4]}{m[5]}{CONSTANT.sub(immediate, m[6], count=1)}'
        edits.append(dict(func=func, delta=int(m[2], 16) - func_va, kind=0, symbol=c[2],
                          offset=0, word=word))


def rewrite(text, symbols):
    """Symbolize numeric low halves reached only by one %hi/raw lui on every CFG path."""
    lines = text.split('\n')
    edits, hi_named = [], {}
    numeric_constants(lines, symbols, edits)
    for fn in list(functions_of(lines)):
        insns = fn['insns']
        if not insns:
            continue
        succ, clobber = successors(fn)
        if succ is None:
            continue
        TOP = object()
        state_in = [TOP] * len(insns)
        state_in[0] = {}
        work = [0]
        while work:
            k = work.pop()
            out = transfer(insns[k], state_in[k], clobber[k])
            for s in succ[k]:
                old = state_in[s]
                merged = dict(out) if old is TOP else {r: v for r, v in old.items() if out.get(r) == v}
                if old is TOP or merged != old:
                    state_in[s] = merged
                    work.append(s)
        func_va = int(insns[0][1][2], 16)
        for k, (i, m, mnem, ops, word) in enumerate(insns):
            state = state_in[k]
            if state is TOP:
                continue
            site = None
            if mnem == 'addiu' and len(ops) == 3 and ops[1] in state and NUM.match(ops[2]):
                site, base = 2, ops[1]
            elif mnem in LOADSTORE and len(ops) == 2:
                mm = MEM.match(ops[1])
                if mm and mm[2] in state:
                    site, base = 1, mm[2]
            if site is None:
                continue
            lui_line, name, hival = state[base]
            address = ((hival << 16) + simm(word)) & 0xffffffff
            name = name or hi_named.get(lui_line)
            resolved = None
            if name is not None:
                sym = symbols.named(name)
                if sym and sym[0] <= address < sym[0] + max(sym[1], 1):
                    resolved = (name, sym[0])
            else:
                resolved = symbols.owner(address)
            if resolved is None:
                continue
            name_r, start = resolved
            expr = lo_expr(name_r, address - start)
            ops = list(ops)
            if site == 2:
                ops[2] = expr
            else:
                ops[1] = f'{expr}({base})'
            lines[i] = f'{m[1]}{mnem}{m[5]}{", ".join(ops)}'
            edits.append(dict(func=fn['name'], delta=int(m[2], 16) - func_va, kind=6,
                              symbol=name_r, offset=address - start, word=word))
            if lui_line not in hi_named:
                lm = INSN.match(lines[lui_line])
                lops = [o.strip() for o in lm[6].split(',')]
                if NUM.match(lops[1]) or SHIFTED.match(lops[1]):
                    off = address - start
                    hexpr = f'%hi({name_r})' if off == 0 else f'%hi({name_r} + 0x{off:X})'
                    lines[lui_line] = f'{lm[1]}lui{lm[5]}{lops[0]}, {hexpr}'
                    edits.append(dict(func=fn['name'], delta=int(lm[2], 16) - func_va, kind=5,
                                      symbol=name_r, offset=off,
                                      word=int.from_bytes(bytes.fromhex(lm[3]), 'little')))
                hi_named[lui_line] = name_r
    return '\n'.join(lines), edits


def relocations(elf):
    rows = []
    for rel in elf.sections:
        if rel.type != 9:
            continue
        target = elf.sections[rel.info].name
        symtab = [s for s in elf.symbols]
        for at in range(rel.offset, rel.offset + rel.size, 8):
            offset, info = struct.unpack_from('<II', elf.data, at)
            sym = symtab[info >> 8] if info >> 8 < len(symtab) else None
            name = sym.name or (elf.sections[sym.shndx].name if sym and 0 < sym.shndx < len(elf.sections) else '')
            rows.append((target, offset, info & 255, name))
    return rows


def field(kind, offset):
    """In-place REL addend gas writes for %lo/%hi(symbol + offset)."""
    return (offset & 0xffff) if kind == 6 else (((offset + 0x8000) >> 16) & 0xffff)


def final(kind, address):
    return (address & 0xffff) if kind == 6 else (((address + 0x8000) >> 16) & 0xffff)


def symbolize(reference, cwd, destination, assembler, flags, symbols):
    """Return (object, added relocations): a re-assembled copy of `reference`
    with recovered %hi/%lo, or `reference` unchanged when any proof fails."""
    assembly = Path(str(reference) + '.s')
    if not assembly.is_file():
        return reference, 0
    body = assembly.read_text()
    out_dir = destination.parent / 'original-asm'
    edits = []

    def include(match):
        path = cwd / match[1]
        old = path.read_text()
        new, found = rewrite(old, symbols)
        if not found:
            return match[0]
        edits.extend(found)
        local = out_dir / match[1].replace('/', '__')
        local.parent.mkdir(parents=True, exist_ok=True)
        local.write_text(new)
        return f'.include {json.dumps(str(local))}'

    body = re.sub(r'\.include\s+"(asm/[^"]+\.s)"', include, body)
    if not edits:
        return reference, 0
    rewritten = destination.with_suffix('.s')
    rewritten.write_text(body)
    gflag = next(flag for flag in flags if flag.startswith('-G'))
    proc = subprocess.run(['timeout', '-k', '5', '60', str(assembler), '-EL', '-m5900', '-mabi=eabi',
                           gflag, '-I.', '-Iinclude', '-o', str(destination), str(rewritten)],
                          cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    status = destination.with_suffix('.status.json')
    if proc.returncode != 0 or proc.stdout.strip():
        status.write_text(json.dumps(dict(used=False, reason='assembler',
                                          output=proc.stdout.decode(errors='replace')[:2000])))
        return reference, 0
    ok, added = verify(Elf(reference.read_bytes()), Elf(destination.read_bytes()), edits, symbols)
    status.write_text(json.dumps(dict(used=ok, edits=len(edits), added=added)))
    return (destination, added) if ok else (reference, 0)


def verify(before, after, edits, symbols):
    """Prove the copy is the original plus relocations that link to the original words.

    * every allocated section other than .text is byte-identical;
    * .text differs only at edited sites, where the upper 16 bits (opcode and
      registers) are unchanged, the in-place field is exactly gas's REL addend
      for symbol+offset, and the linked symbol address reproduces the original
      immediate;
    * function symbols and every pre-existing relocation are unchanged, and
      each added relocation is the recorded HI16/LO16 against the recorded
      symbol at an edited site.
    """
    alloc = lambda e: {s.name: (s.type, s.flags, s.size, e.section_bytes(s))
                       for s in e.sections if s.flags & 2 and s.name != '.text'}
    funcs = lambda e: {(s.name, s.value, s.size, s.bind) for s in e.symbols if s.type == 2}
    if alloc(before) != alloc(after) or funcs(before) != funcs(after):
        return False, 0
    starts = {s.name: s.value for s in before.symbols if s.type == 2 and s.size}
    old_text = before.section_bytes(before.section('.text'))
    new_text = after.section_bytes(after.section('.text'))
    if len(old_text) != len(new_text):
        return False, 0
    sites = {}
    for e in edits:
        if e['func'] not in starts:
            return False, 0
        offset = starts[e['func']] + e['delta']
        if offset in sites and sites[offset] != e:
            return False, 0
        sites[offset] = e
    for offset in range(0, len(old_text), 4):
        a = int.from_bytes(old_text[offset:offset + 4], 'little')
        b = int.from_bytes(new_text[offset:offset + 4], 'little')
        e = sites.get(offset)
        if e is None:
            if a != b:
                return False, 0
            continue
        if e['kind'] == 0:
            # the old field held splat's REL addend; the copy holds the original word
            if b != e['word'] or a >> 16 != b >> 16:
                return False, 0
            continue
        sym = symbols.named(e['symbol'])
        if sym is None:
            return False, 0
        start = sym[0]
        if (a != e['word'] or a >> 16 != b >> 16 or (b & 0xffff) != field(e['kind'], e['offset'])
                or final(e['kind'], start + e['offset']) != a & 0xffff):
            return False, 0
    old, new = Counter(relocations(before)), Counter(relocations(after))
    removed, added = old - new, new - old
    numeric = {o for o, e in sites.items() if e['kind'] == 0}
    # Removed: exactly the HI16/LO16 of each numeric constant site.
    if Counter((t, o) for t, o, _, _ in removed) != Counter(('.text', o) for o in numeric) or \
            any(ty not in (5, 6) for _, _, ty, _ in removed):
        return False, 0
    expected = Counter(('.text', o, e['kind'], e['symbol']) for o, e in sites.items() if e['kind'])
    if added != expected:
        return False, 0
    return True, sum(added.values()) + sum(removed.values())

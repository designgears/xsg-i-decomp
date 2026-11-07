#!/usr/bin/env python3
"""Find scalar locals whose address is handed to a callee that treats it as a
buffer (STAGE12_BATTLE.md).

The matched decompilation sometimes declares `int packet;` or `char name;` and
passes `&packet` / `&name` to a function that fills a larger record (grGpInit's
two-word GrPacket, dataSndSeNameGet's file name).  On the PS2 the overflow
lands in frame padding the original compiler laid out; on the host it
overwrites saved registers (%ebx/%esi) and the caller crashes later, far from
the cause.

Heuristic: a function-local `<scalar type> NAME;` (no array, no pointer) and a
call `callee(..., &NAME ...)` or `callee(..., (cast)&NAME ...)` where callee is
not a known by-reference scalar out-parameter.  Prints file:line, callee, type.

usage: scan_scalar_buffers.py [src dirs...]   (default: src/ov01 src/main)
"""
import os, re, sys

SCALAR = r'(?:unsigned\s+|signed\s+)?(?:char|short|int|long(?:\s+long)?|u8|s8|u16|s16|u32|s32|u64|s64|float|double)'
DECL = re.compile(r'^\s*(?:register\s+)?(' + SCALAR + r')\s+([A-Za-z_]\w*)\s*;', re.M)
# callees whose argument is a buffer / record (not a single scalar out-param)
BUFFER_CALLEES = re.compile(r'^(gr\w+|str\w+|sprintf|memcpy|memset|data\w*Name\w*|\w*Name\w*Get|'
                            r'sceVif\w+|xglPacket\w+|menu\w*Put|\w*Packet\w*)$')


def functions(text):
    """Yield (start_offset, body) of top-level brace blocks preceded by ')'."""
    i, n = 0, len(text)
    while i < n:
        j = text.find('{', i)
        if j < 0:
            return
        k = j - 1
        while k >= 0 and text[k] in ' \t\r\n':
            k -= 1
        depth, e = 0, j
        while e < n:
            c = text[e]
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    break
            e += 1
        if k >= 0 and text[k] == ')':
            yield j, text[j:e + 1]
        i = e + 1


def main():
    dirs = sys.argv[1:] or ['src/ov01', 'src/main']
    hits = 0
    for d in dirs:
        for root, _, files in os.walk(d):
            for f in sorted(files):
                if not f.endswith('.c'):
                    continue
                p = os.path.join(root, f)
                text = open(p, errors='replace').read()
                for off, body in functions(text):
                    for m in DECL.finditer(body):
                        typ, name = m.group(1), m.group(2)
                        use = re.compile(r'([A-Za-z_]\w*)\s*\(([^;]*?)(?:\([^()]*\)\s*)?&\s*' + re.escape(name) + r'\b(?!\s*[\[.])')
                        for u in use.finditer(body):
                            callee = u.group(1)
                            if callee in ('if', 'while', 'for', 'switch', 'return', 'sizeof'):
                                continue
                            if not BUFFER_CALLEES.match(callee):
                                continue
                            line = text.count('\n', 0, off + u.start()) + 1
                            print(f'{p}:{line}: {callee}(&{name}) with {typ} {name};')
                            hits += 1
    print(f'{hits} candidate(s)', file=sys.stderr)


if __name__ == '__main__':
    main()

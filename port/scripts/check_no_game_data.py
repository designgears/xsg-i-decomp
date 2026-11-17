#!/usr/bin/env python3
"""Fail if port/data carries bytes of the game.

The skeleton assembly (gen_host_data.py / gen_iop_sound_data.py --skeleton) may
only lay out space (.skip/.zero/.space/.balign), define labels, and hold
`.long` words that are symbol references (relocations), zero, or fill-table
entries.  Any data directive with a literal value is game data.

  check_no_game_data.py port/data
"""
import os
import re
import sys

ALLOWED = {".skip", ".zero", ".space", ".balign", ".section", ".data", ".globl", ".global",
           ".weak", ".local", ".type", ".size", ".long"}
LITERAL = re.compile(r"^-?(0x[0-9A-Fa-f]+|\d+)$")


FILL = re.compile(r"^\.long \.L\w+, 0x[0-9a-f]+, \d+, \.L\w+ - \.L\w+$")


def check_file(path):
    bad = []
    for n, line in enumerate(open(path, encoding="latin-1"), 1):
        code = re.sub(r"/\*.*?\*/", "", line).strip()
        if not code or code.startswith("#") or code.endswith(":"):
            continue
        op = code.split()[0]
        if op not in ALLOWED:
            bad.append((n, line.rstrip()))
        elif op == ".long" and not FILL.match(code):
            if any(LITERAL.match(v.strip()) and int(v.strip(), 0) for v in code[5:].split(",")):
                bad.append((n, line.rstrip()))
    return bad


def main():
    root = sys.argv[1]
    errors = 0
    for dp, _, fs in os.walk(root):
        for f in sorted(fs):
            if not f.endswith((".s", ".S")):
                continue
            for n, line in check_file(os.path.join(dp, f))[:5]:
                print("%s:%d: game data in the skeleton: %s" % (os.path.join(dp, f), n, line))
                errors += 1
    if errors:
        sys.exit(1)
    print("check_no_game_data: %s holds layout only" % root)


if __name__ == "__main__":
    main()

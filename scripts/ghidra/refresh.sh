#!/usr/bin/env bash
# Rebuild the prototype index and Ghidra export together (single instance), then
# refresh the agents' .ghidra.c seeds from it (reseed.py).
# next.py --claim starts this detached when build/ghidra/.stamp is over a day old.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$ROOT/build/ghidra"
exec 9>"$ROOT/build/ghidra/refresh.lock"
flock -n 9 || exit 0                     # already running
{
  date
  "$ROOT/.venv/bin/python" -B "$ROOT/scripts/auto_decomp/proto_index.py" &&
  nice -n 10 "$ROOT/scripts/ghidra/export.sh" &&
  python3 -B "$ROOT/scripts/ghidra/reseed.py" &&
  touch "$ROOT/build/ghidra/.stamp"
  date
} >"$ROOT/build/ghidra/refresh.log" 2>&1

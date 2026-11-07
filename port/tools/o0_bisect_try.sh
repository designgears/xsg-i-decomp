#!/bin/sh
# Stage 10: one bisection step for the optimised-build bugs.
#   o0_bisect_try.sh <build-dir> <regex of game files to build at -O0> [seconds]
# Prints the last vblank reached in the field-recipe run (a per-run
# settings.ini passed as XSG_CONFIG, port/docs/MENU.md).
B=$1
R=$2
T=${3:-45}
cmake -S "$(dirname "$0")/.." -B "$B" -DXSG_O0_REGEX="$R" >/dev/null || exit 1
cmake --build "$B" -j16 >/dev/null 2>&1 || { echo build failed; exit 1; }
CFG=$(mktemp /tmp/xsg-o0-XXXXXX.ini)
cat > "$CFG" <<EOF
[renderer]
gs = gpu
[game]
movies = 0
[audio]
enabled = 0
[debug]
headless = 1
skip_events = 1
fps = 100
timeout = $T
pad = 300:START,1100:START,1200:CROSS,1500:CIRCLE*400@60
EOF
XSG_CONFIG=$CFG "$B/xsg" 2>&1 |
  grep -E '^\[fps\]|^\[fatal\]|^\[timeout\]' | tail -2 | tr '\n' ' '
echo
rm -f "$CFG"

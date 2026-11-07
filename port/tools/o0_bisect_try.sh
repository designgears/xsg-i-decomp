#!/bin/sh
# Stage 10: one bisection step for the optimised-build bugs.
#   o0_bisect_try.sh <build-dir> <regex of game files to build at -O0> [seconds]
# Prints the last vblank reached in the field-recipe run.
B=$1
R=$2
T=${3:-45}
cmake -S "$(dirname "$0")/.." -B "$B" -DXSG_O0_REGEX="$R" >/dev/null || exit 1
cmake --build "$B" -j16 >/dev/null 2>&1 || { echo build failed; exit 1; }
XSG_GS=gpu XSG_GPU_HEADLESS=1 XSG_AUDIO=0 XSG_MPEG=0 XSG_TIMEOUT=$T XSG_SKIP_EVENTS=1 XSG_FPS=100 \
XSG_PAD='300:START,1100:START,1200:CROSS,1500:CIRCLE*400@60' "$B/xsg" 2>&1 |
  grep -E '^\[fps\]|^\[fatal\]|^\[timeout\]' | tail -2 | tr '\n' ' '
echo

#!/bin/sh
# Stage 10: the field recipe with [debug] fps, for before/after speed numbers.
#   fps_run.sh <xsg binary> <seconds> [fast]  -> [fps] lines on stdout
# Title = vblanks 300-1000, cutscene = 1300-2500 (KOS-MOS, Virtual Tutorial),
# field = from about vblank 4500 on.  Configured by a per-run settings.ini
# (XSG_CONFIG), port/docs/MENU.md.
BIN=$1
T=$2
FPS=60
[ "$3" = fast ] && FPS=uncapped
CFG=$(mktemp /tmp/xsg-fps-XXXXXX.ini)
cat > "$CFG" <<EOF
[renderer]
gs = gpu
[game]
movies = 0
fps_limit = $FPS
[audio]
enabled = 0
[debug]
headless = 1
gpu_bin = $(dirname "$BIN")/gpu/xsg-gpu
skip_events = 1
fps = 250
timeout = $T
pad = 300:START,1100:START,1200:CROSS,1500:CIRCLE*400@60
EOF
XSG_CONFIG=$CFG "$BIN" 2>&1 | grep -E '^\[fps\]|^\[fatal\]'
rm -f "$CFG"

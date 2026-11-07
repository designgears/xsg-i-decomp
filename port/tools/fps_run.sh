#!/bin/sh
# Stage 10: the field recipe with XSG_FPS, for before/after speed numbers.
#   fps_run.sh <xsg binary> <seconds> [fast]  -> [fps] lines on stdout
# Title = vblanks 300-1000, cutscene = 1300-2500 (KOS-MOS, Virtual Tutorial),
# field = from about vblank 4500 on.
BIN=$1
T=$2
FAST=
[ "$3" = fast ] && FAST=1
XSG_GPU_BIN=${XSG_GPU_BIN:-$(dirname "$BIN")/gpu/xsg-gpu} XSG_GS=gpu XSG_GPU_HEADLESS=1 XSG_AUDIO=0 \
XSG_MPEG=0 XSG_SKIP_EVENTS=1 XSG_FPS=250 XSG_TIMEOUT=$T XSG_FAST=$FAST \
XSG_PAD='300:START,1100:START,1200:CROSS,1500:CIRCLE*400@60' "$BIN" 2>&1 | grep -E '^\[fps\]|^\[fatal\]'

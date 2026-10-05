# Source before configure/ninja:
#   source scripts/env.sh
#
# Host-built assemblers/binutils will not match the pinned SHA-256s (they embed
# absolute build paths). XENO_TOOLCHAIN_ALLOW accepts them; the patched ee-gcc
# binaries are bit-identical to the project pins.

export XENO_TOOLCHAIN_ALLOW=ee-as-la29-vsqrt,ee-as-2.9-plain,ps2dev-binutils,dvp-as-3eb45ea
export XENO_GAME_DIR="${XENO_GAME_DIR:-$PWD/game/extracted-iso}"
export XENO_TOOLCHAIN_DIR="${XENO_TOOLCHAIN_DIR:-$PWD/.work/toolchains}"
export XENO_SPLAT_PYTHON="${XENO_SPLAT_PYTHON:-$PWD/.venv/bin/python}"

#!/bin/sh
# Regenerate port/data from the PS2 build (build/<unit>, made by configure.py from
# your disc) after a decomp change.  port/data holds no game bytes: the data's layout,
# its pointer words and the fill tables the runtime copies the bytes with
# (runtime/datafill.c).  Commit the result.
#
#   port/scripts/gen_data.sh [IOP_DIR]     (default game/extracted-iso/IOP)
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
iop=${1:-$root/game/extracted-iso/IOP}
py=${PYTHON:-python3}
$py "$root/port/scripts/gen_host_data.py" --root "$root" --skeleton --irx-dir "$iop"
$py "$root/port/scripts/gen_iop_sound_data.py" --skeleton "$iop/SSD.IRX" "$iop/RSSD.IRX" \
    "$root/port/data/iop_sound_data.S"
$py "$root/port/scripts/gen_vu0_microcode.py" "$root/build/main/build/vu0/Vu0MicroCode.o" \
    "$root/port/data/vu0_microcode.c"
$py "$root/port/scripts/check_no_game_data.py" "$root/port/data"

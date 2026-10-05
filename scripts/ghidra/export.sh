#!/usr/bin/env bash
# Headless Ghidra (EE module): import each original image, apply config/symbols,
# analyse, and export per-function decompilation to build/ghidra/<unit>/<name>.c.
# m2c_seed.py copies it next to each m2c draft as <body>.ghidra.c.
# Run scripts/auto_decomp/proto_index.py first: it writes build/ghidra/types/,
# the project's own declarations that ImportTypes.java applies.
#   scripts/ghidra/export.sh [unit ...]      (default: all units)
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
GHIDRA_HEADLESS=${GHIDRA_HEADLESS:-$(command -v ghidra-headless || echo ~/.local/opt/ghidra_12.1.3_PUBLIC/support/analyzeHeadless)}
GAME=${XENO_GAME_DIR:-$ROOT/game/extracted-iso}
PROJ=$ROOT/build/ghidra-project   # Ghidra rejects dot-dirs like .work
declare -A IMG=([main]=SLUS_204.69 [ov01]=OV01.OVL [ov02]=OV02.OVL [ov10]=OV10.OVL [ov11]=OV11.OVL [ov12]=OV12.OVL)
units=("$@"); [ ${#units[@]} -eq 0 ] && units=(main ov01 ov02 ov10 ov11 ov12)
mkdir -p "$PROJ"
for u in "${units[@]}"; do
  final=$ROOT/build/ghidra/$u; out=$final.new; rm -rf "$out"
  "$GHIDRA_HEADLESS" "$PROJ" xsg -overwrite -import "$GAME/${IMG[$u]}" \
    -processor r5900:LE:32:default -scriptPath "$ROOT/scripts/ghidra" \
    -preScript ApplySymbols.java "$ROOT/config/symbols/$u.txt" \
    -postScript ApplySymbols.java "$ROOT/config/symbols/$u.txt" \
    -postScript ImportTypes.java "$ROOT/build/ghidra/types/$u" \
    -postScript ExportDecomp.java "$out" -max-cpu "${GHIDRA_CPU:-4}" 2>&1 \
    | grep -E "ApplySymbols:|ImportTypes:|ExportDecomp:|ERROR|Exception" || true
  python3 -B "$ROOT/scripts/ghidra/rename.py" "$out"
  if [ -n "$(ls -A "$out" 2>/dev/null)" ]; then       # swap in only a complete export
    rm -rf "$final.old"; [ -d "$final" ] && mv "$final" "$final.old"
    mv "$out" "$final"; rm -rf "$final.old"
  fi
done

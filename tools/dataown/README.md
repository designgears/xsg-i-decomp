# Data ownership: moving a TU's data into its C

All functions are matched C. These tools move each translation unit's (TU's) data —
`.data`, `.rodata`, `.sdata`, `.lit4`/`.lit8`, `.bss`, `.sbss` — from splat scaffold into the
TU's C source while every image stays byte-identical, and measure it honestly in the
native objdiff report (`tools/objdiff_report.py`).

## What "100%" means here

objdiff credits a data section only when it measures it at exactly 100%:

- the C object's section holds every original item in order, with the original sizes and
  alignment gaps, byte for byte, and every data relocation resolves to the same address;
- BSS labels are sized to the next label on both sides (the last one is compared at the C's
  size when the original is less than 128 bytes larger), so a BSS section matches only when
  the C defines objects at the original offsets;
- a unit is *complete* when objdiff measures every one of its sections and functions at 100%;
  a section objdiff could not measure keeps it incomplete.

The report normalizes only representation, never content: link fill at section edges
(< 128 zero bytes, unreferenced pad labels only), splat's constants read as addresses
(`.word D_8000 + 0x1`), same-object pointers written by name vs section-relative, pointers
the original stores as raw numbers, and static-function pointers are compared at their
linked value. Corrupting one byte of the C still drops the section below 100%.

## The loop (checkout root, after `source scripts/env.sh`)

    tools/dataown/report.sh main/foo ov12/bar   # gate + verification + report (~60 s); prints each TU's sections
    python3 tools/dataown/secdiff.py main/foo .data   # what differs: symbols, words, relocations
    python3 tools/dataown/tu_data.py status main/foo  # splat items and which the C owns
    python3 tools/dataown/tu_data.py carve main/foo   # compile, data_carve derive --write, reconfigure

`tu_data.py carve` records the C-owned runs in `config/tu/data-carves.json`
(`git add -f`, the directory is ignored). The gate is the byte verdict; the report is the
measurement.

## Moving data between TUs (the split)

- `move_boundary.py SECTION PREV NEXT NEW_VA` — move the boundary of two adjacent TUs
  (MAIN stems, or overlay `ovl/tu` names).
- `create_piece.py SECTION PREV NEW START_VA` — give a TU a new section piece from its
  neighbour's tail (the TU ordinals must allow it).
- `check_split.py` — `main.yaml` and `main.objects.json` agree; `sync_objects.py` realigns the
  manifest to the YAML after a merge; `resolve3.py` / `merge_carves.py` resolve config merges.
- `own_bss.py UNIT/STEM .bss` — hand a TU's BSS to its C object (MAIN `owners`, overlay splat
  type `.bss`).
- `inline_strs.py UNIT/STEM` — strings anonymous in the original become literals at their uses.

Data belongs to the TU whose code uses it; an aligned object at an offset its own section
cannot hold (16-aligned at 8 mod 16), or link fill before it, marks a wrong boundary.

## Honesty rules

- No `__attribute__((weak))` to steer relocations; no top-level `__asm__` labels, aliases or
  `.space`; no objects invented only for padding; no arrays over-sized to swallow gaps.
- `aligned(N)` / `section(...)` only to reproduce the original object's alignment/section;
  `__asm__("name.N")` to keep an original local-static name is fine.
- A type gets the original object's full size (an `_unmodeled` tail) when the code views only
  part of it.
- Function code never changes beyond declarations/definitions: the gate checks it.

## GCC 2.96 (EE) facts that decide layout

- File-scope initialized data is emitted at its definition; function-local statics at their
  declaration (a block's statics when the block is reached); a function's own literals when
  the function is compiled; a pointer-array initializer's strings in reverse index order;
  identical literals are merged; uninitialized statics (`.lcomm`) in definition order.
- `-G8` (MAIN): initialized objects ≤ 8 bytes go to `.sdata`; arrays/structs ≥ 8 bytes are
  8-aligned; a visible small definition can switch code from `%hi/%lo` to `$gp`.
- Local statics are named `name.N` with a TU-wide counter; splat spells them `name_N`.
- Copy-initialized locals: initialize the code's wrapper struct type, not a plain pointer
  array, to get the original doubleword copies.

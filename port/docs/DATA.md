# Game data: layout in the repo, bytes from the disc

The port builds without any game files. The executable holds the game's code
(compiled from `src/`) and the **layout** of its data, and copies the data's bytes
from the player's disc image when it starts.

## What is in the repo

`port/data/` (generated, committed):

| file | what |
|---|---|
| `<unit>/*.s` | One per data object of the PS2 link (main, ov01, ov02, ov10, ov11, ov12): labels, sizes, alignment, and the pointer words as `.long Sym+off` (relocations). Every run of the game's own bytes is a `.skip n` between `.LxsN`/`.LxeN` labels, listed in the file's fill table `xsgft_<unit>_<n>` as `{host address, file offset, bytes, laid-out bytes}`. |
| `iop_sound_data.S` | The same for the IOP sound driver's tables (SSD.IRX / RSSD.IRX, `gen_iop_sound_data.py --skeleton`). |
| `fill_index.c` | `xsg_fill_files`: the disc files the bytes come from, with size and SHA-256. `xsg_fill_tabs`: every fill table and its file. |
| `vu0_microcode.c` | The resident VU0 program, assembled from `src/main/vu0/Vu0MicroCode.dvp`. |
| `syms/<unit>/` | The PS2 link's symbol scripts (names and addresses) that `link_xsg.py` resolves leftovers with. |

`port/scripts/check_no_game_data.py port/data` fails on any directive with a literal
value (`.byte`, `.ascii`, `.short`, `.float`, `.incbin`, a non-zero numeric `.long`).

## At start-up

`runtime/datafill.c xsg_data_fill()` runs after the main image is loaded and before
the overlays snapshot their data. For each file of `xsg_fill_files` (`SLUS_204.69`,
`OV01.OVL` … `OV12.OVL`, `IOP/SSD.IRX`, `IOP/RSSD.IRX`) it reads
`<game_dir>/extracted-iso/<file>` if present, else the file on the disc image; checks
size and SHA-256 (another release or a bad dump stops the boot with a log line); then
copies every run. A run whose laid-out size differs from its byte count (padding
inside a run) is refused. Log: `[data] 10526 runs, 1809133 bytes filled from 8 disc files`.

## After a decomp change

Regenerate from the PS2 build (`build/<unit>`, made by `configure.py` from your disc)
and commit:

```sh
port/scripts/gen_data.sh                       # gen_host_data.py --skeleton, IOP, VU0, check
```

Check that the filled data equals the full-data build (`gen_host_data.py` without
`--skeleton`, the old pipeline) object by object, pointers compared by symbol + offset:

```sh
python3 port/scripts/verify_data_fill.py <full-data xsg> port/build/xsg --game game/extracted-iso
# filled 10526 runs, 1809133 bytes
# 11961 objects compared, 0 mismatched, 8198 pointer words equivalent
```

`find_overlap_access.py` reads `build/<unit>` too; a checkout without the PS2 build
skips it (the CMake link step says so).

## Verified 2026-11-18

* `verify_data_fill.py` against a full-data build of the same sources: 11961 objects,
  0 mismatches; the IOP tables (ssd 0x5fb4, rssd 0x21c bytes) equal too.
* Headless boot from the ISO only (`game_dir` empty): Linux build and the Windows build
  under Wine both fill from 8 disc files and reach the first field (ev1000 → ev1010 →
  cf0210 MC_VOK21), the same scenes as the full-data build.
* Scanning the binaries for 32-byte windows of the disc files finds only the repo's own
  decompiled C string/float literals, the VU0 program from the `.dvp`, and debug info.
* Side fix: the PS2 mod-16 placement probe read `/* Automatically generated … */` as
  address 0xA, so 13 pad objects started 10 bytes in instead of at their PS2 offset.

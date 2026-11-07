# PC port, Stage 3: a linked host executable that boots headless

> **Superseded in part by [STAGE4_BOOT.md](STAGE4_BOOT.md)**: boot now reaches the title
> screen and overlay 1; blockers 1, 2, 4 and 5 below are resolved there, and vblanks are paced
> at 60 Hz by default (`XSG_FAST=1` for unthrottled).

Branch `pc-port`, 2026-11-06/07, gcc 16, i386 (`-m32`), non-PIE.

**Result:** `port/build/xsg` links the whole game (main + ov01/ov02/ov10/ov11/ov12, all
C from `src/`, plus the game's data converted from the PS2 build) with a host runtime. It maps
the PS2 address space, enters the game's own `main()` (`src/main/control_entry.c`), runs
`InitializeSystem()` to the end, creates the game's EE threads, and settles in the
thread-rotation main loop. With nothing pacing it, it runs ~20,000 "frames" (vblanks) a second
without crashing. It has not reached the first overlay load (`xglCdLoadOverlay(2)`, the logo
and title screen) yet; see "Next blockers".

## Build and run

```sh
# prerequisite: the PS2 build has been configured from your own disc, so build/<unit>/ exists
source scripts/env.sh && .venv/bin/python -B configure.py --quiet   # (and ninja, for the gate)
cmake -S port -B port/build && cmake --build port/build -j
port/build/xsg                                    # logs to stderr
XSG_LOG_FILE=/tmp/xsg.log XSG_TIMEOUT=20 port/build/xsg
```

Environment:

| variable | meaning |
|---|---|
| `XSG_GAME_DIR` | default `<repo>/game` (CMake cache `XSG_GAME_DIR`); `iso/*.iso` and `extracted-iso/SLUS_204.69` are read from it |
| `XSG_ISO` | the disc image, read-only (default: the first `*.iso` in `$XSG_GAME_DIR/iso`) |
| `XSG_ELF` | the original main ELF (default `$XSG_GAME_DIR/extracted-iso/SLUS_204.69`) |
| `XSG_LOG` | 0 quiet, 1 (default) first 3 calls of each stub then every 1000th, 2 every call |
| `XSG_LOG_FILE` | log to a file instead of stderr |
| `XSG_TIMEOUT` | after N seconds, print the running EE thread's backtrace and the thread table, then exit 124 |
| `XSG_FRAME_US` | sleep this long per vblank (0 = run flat out) |

Backtraces from `XSG_TIMEOUT` are raw addresses; resolve them with
`addr2line -f -e port/build/xsg`. Crashes print `[fatal] signal N at address A` and dump core;
under gdb, `gdb -batch -ex run -ex bt port/build/xsg` works (a non-PIE i386 executable with
debug info for the runtime). The game objects have no `-g`; add it to `XSG_FLAGS` if needed.
`-rdynamic` must not be used: with the generated absolute symbols it crashes ld.so.

Nothing from the disc is copied into the tree: game data reaches the build only through
`build/` (the PS2 build's own outputs, already git-ignored) and the runtime reads the ISO and
ELF at run time.

## The gate

`ninja gate` passes (all units, `"result": "pass"`) after every `src/`/`include/` change in this
stage. Host-only code is under `#ifdef PORT_HOST`, or in macros whose PS2 expansion is
token-for-token the original.

## Memory map (32-bit process)

| range | what |
|---|---|
| `0x00010000`–`0x02000000` | EE main RAM, `mmap(MAP_FIXED_NOREPLACE)`. It starts at `vm.mmap_min_addr` (read from `/proc`, 64 KiB here), so PS2 addresses below `0x10000` are not mapped. The original SLUS ELF's PT_LOAD segments are copied in at their PS2 addresses (`0x200000` text, `0x338600` data, `0xA00000` ov02), so code that reads main-image data through hard-coded addresses (ov01 `data_unit_org_get` reads `0x426F48`) sees the original bytes. The game's own heap (`WorkEnd = 0xA80000`...) and the threads' PS2 stacks (`0x1E5000`–`0x1F8000`) live here. |
| `0x08048000`... | the host executable (non-PIE, i386 default base), then the host heap |
| `0x10000000`–`0x10010000` | EE hardware registers, plain memory. A runtime thread clears `CHCR.STR` of every DMA channel, so DMA "completes" immediately |
| `0x11000000`–`0x11010000` | VU0/VU1 micro and data memory, plain memory (the xgl matrix stack lives at `0x11004000`) |
| `0x12000000`–`0x12002000` | GS privileged registers, plain memory |
| `0x70000000`–`0x70004000` | scratchpad |

Overlays are not loaded at `0xA00000`; they are linked into the executable, so that area is plain RAM.

## Data pipeline

`port/scripts/gen_host_data.py` (run by CMake into `port/build/data/`):

1. For each unit it reads the linker script the PS2 build actually uses
   (`build/main/main.elf.ld`, `build/ovXX/ovXX.elf.modern.ld`) and lists, in link order, every
   input that is not a compiled C object (`build/c/`, `build/scaffold/`), not SDK/libc text
   (`build/expected/`, `build/hasm/`), not an ELF/crt blob (`reginfo`, `ctors`, `dtors`,
   `eh_frame`, ELF headers), and not main's embedded copy of ov02. What is left: splat data
   pieces (`asm/.../X.{data,rodata,sdata,lit4,bss,sbss}.s`), data carves (`carve/<unit>/*.s`),
   the linker COMMON pieces and the VU data blob (`vudata_rest`, via `.incbin`). The object →
   source mapping comes from the unit's `build.ninja` (`build X.o: as Y.s`).
2. Each piece is rewritten for `as --32`: `.word` → `.long` (i386 `.word` is 16-bit);
   `.align n` → `.balign 2^n`; `.word/.short/.float` get the natural alignment MIPS gas would
   give them; splat's `dlabel`/`enddlabel`/`nonmatching` macros are expanded; `.incbin` paths
   become absolute. `.word .Lxxxx` jump-table entries that point into MIPS text become 0 (673
   of them; host C has its own switch tables). Each piece starts at its PS2 address modulo 16,
   so 16-byte quadword data stays 16-aligned (host gcc copies `ps2_u128` with `movaps`).
3. The linker COMMON pieces (`linker/common.bss`, `scommon.sbss`) are made **weak**: ee-gcc
   compiled with `-fcommon` and the PS2 link turned tentative definitions into COMMON, while
   the host compiles with `-fno-common`, so the C objects define those symbols.
4. Sections: main uses `.data/.rodata/.bss`; each overlay uses `xsgd_<ov>`, `xsgr_<ov>` and
   `xsgb_<ov>`, so the runtime finds its bounds through `__start_`/`__stop_` symbols.

Result: 920 files (main 597, overlays 323), all assembled. Not converted: `Vu0MicroCode.o`
(built by `dvpas` from `src/main/vu0/Vu0MicroCode.dvp`, which has no `as` source). Its
`Vu0Call*` labels are auto-stubs; nothing on the host runs VU0 microcode anyway.

## Link and overlay strategy

`port/scripts/link_xsg.py` (CMake target `xsg_exe`):

1. Per unit, `ld -r` all its C objects and its converted data into `<unit>.r.o`.
2. **File-static symbols named by data tables**: data pieces refer by name to functions and
   data the C defines `static` (they were LOCAL in the original symbol tables, e.g.
   `scERRORScript`, `_BallVsBall`, `command_disk`). Any such name with exactly one local
   definition in the unit is globalized (`objcopy --globalize-symbols`). 1075 are, and there
   are no ambiguous ones. The list is in `port/build/globalized.txt`.
3. **PS2-address tables**: for each overlay, every `type:func` entry in
   `config/symbols/<ov>.txt` that the overlay defines goes into a `{PS2 address, host
   function}` table, `xsg_ovtab_<ov>` (1073 / 109 / 361 / 139 / 1726 entries).
4. **Symbol scoping**: every global an overlay defines that main or another overlay also
   defines is made local to that overlay (`objcopy --localize-symbols`), so each overlay binds
   to its own copy, as it did when it alone sat at `0xA00000`. 495 symbols are scoped (3 of
   them clash with main). They are all data labels (`D_00A4xxxx`, `buffer`, `tbl`...); no
   function names clash. The list is in `port/build/overlay_clashes.txt`.
5. `main` is renamed `xsg_game_main`. The executable is linked `-m32 -no-pie` with the
   runtime, then the remaining undefined symbols are resolved and it is relinked:
   * `--defsym` (93) from the PS2 link's own scripts (`undefined_syms_auto.txt`,
     `undefined_funcs_auto.txt`, `aliases.ld`, `externals.ld`): bare PS2 addresses that splat
     symbolized (`D_8000`, `D_426F48`...), which now point into the mapped RAM image, and
     `X = Y + off` aliases (`TestEnv4`, `UmnDataBaseStateData`...);
   * **trampolines** (76) for `func_A19xxx`/`func_00A0xxxx` and main's `dataXxxGet` externals:
     `push $addr; call xsg_ps2_func; add $4,%esp; jmp *%eax`. `xsg_ps2_func` looks the address
     up in the active overlay's table first, then in the others (logged);
   * **auto-stubs** (`autostubs.c`): a logging function for each remaining text reference
     (the 18 `Vu0Call*` microcode labels and `_gp`), and a 4 KiB zero block for each data
     reference (`D_004DB3A8`, `D_004DB3B0`, the c_aliases offsets into C-owned literals, and
     `TestEnv_2_00A10910`). The full list is in `port/build/autostubs.txt`.

**Overlay load:** `xglCdLoadOverlay` is unchanged. It calls `sceSifLoadElf("cdrom0:\OVxx.OVL;1")`,
and the runtime's `sceSifLoadElf` parses the number, marks that overlay active and restores its
converted `.data`/`.bss` from a pristine copy taken at startup. Limitation: the overlays' **C**
objects' own `.data`/`.bss` (initialised statics compiled from `src/ovXX`) are not reset yet.
The fix is to rename those sections per overlay too (`objcopy --rename-section` in step 1).

`ControlEntry` turns the title screen's PS2 return code into a call. Under `PORT_HOST` it now
compares against `xsg_ps2_addr_of(Title)` and calls `xsg_ps2_func(selection & 0xffffff)` after
loading the overlay.

## Runtime (`port/runtime/`)

| file | contents |
|---|---|
| `host_main.c` | memory mapping, the RAM image load, the DMAC stand-in thread, fault and timeout reporting, the call to `xsg_game_main(0, 0)` |
| `kernel.c` | threads as ucontext coroutines with a 1 MiB host stack each (the PS2 stack pointer is ignored); a priority scheduler with no timer that preempts when a higher-priority thread becomes ready; semaphores; wakeup counts; `sceGsSyncV`/vsync callback/INTC VBLANK; when nothing is ready, it advances a vblank (and declares a deadlock after 600 idle vblanks); Intc/Dmac handler registration; no-op cache ops |
| `disc.c` | ISO9660 lookup in the disc image, including layer 1 of the dual-layer image (its PVD is at layer0 size, so layer 1 starts at LSN 2077312); `sceCdRead`, `sceCdSt*` streaming, `sceCdLayerSearchFile`; `sceOpen/Read/Lseek/Close` for `cdrom0:` paths; HDD and other devices fail |
| `overlay.c` | overlay activation, PS2 address → host function lookup |
| `sdk.c` | SIF RPC (bind and call return success; a minimal "IOP": the RSSD init call answers with the 4096 *busy* callback to the EE server 0x901, which `SsdInit` waits for); `sceSifLoadElf`; GS no-ops; a working libvifpk packet builder (buffers are filled, nothing consumes them); pad (DualShock 2, no buttons); memory card (reports *no card*); IPU/MPEG (every movie ends at once); EE soft-float `fptodp/dptofp/dpadd/dpmul/dpdiv/litodp/dptoli`; `ldf_off` (a gnu89 `extern inline` with no body); `strlwr`; the auto-stub logger |

**Game thread structure** (from the log): main = thread 1 (priority 1, as crt0 leaves it). It
runs `InitializeSystem`, then `xglThreadInitial` creates four threads from
`asSystemThreadList` (PS2 stacks counting down from `0x1F8000`):

| id | priority | PS2 stack | role |
|---|---|---|---|
| 4 | 9 | `0x1F7000`+4 KiB | `xglRenderEntry` (render, DMA kicks) |
| 5 | 10 | `0x1F6000`+4 KiB | camera/geometry thread |
| 7 | 11 | `0x1E5000`+64 KiB | `ControlEntry`, the game flow |
| 6 | 12 | `0x1F5000`+4 KiB | `xglStudioEntry` |

Main then runs `xglThreadRotate`: it wakes thread *i* and waits on the rotate semaphore. Each
thread does its slice and calls `xglSleep()`, which signals the semaphore and sleeps. The sound
library adds threads 2 and 3 (priority 6: the RSSD RPC thread, parked in `sceSifRpcLoop`, and
the next-wave transfer thread).

## Source changes this stage (all gate-verified)

* **128-bit types**: `include/port/ps2_types.h`. The 50 `mode(TI)` typedefs in 44 files become
  `PS2_TI_TYPEDEF(base, name)` (the same tokens on PS2; a 16-byte, 16-aligned `ps2_u128`
  union on the host). The 29 zero/constant assignments use `PS2_QZERO` /
  `PS2_QCONST(lit, w0..w3)`. `-DTI=DI` is gone.
* `src/main/control_entry.c`: the overlay-entry mapping under `PORT_HOST` (above).
* `src/main/matrix_print.c`: the u64 soft-float views of `acos`/`fptodp` are replaced with
  `__builtin_acos` under `PORT_HOST`.
* `src/main/xgl_camera.c`: the `xglVectorScale(dst, src, scale)` view of xgl_2.c's
  `xglVectorScale(scale, dst, src)` is remapped under `PORT_HOST` (see blocker 2).
* `port/include/ps2_host_compat.h`: `__builtin_args_info(n)` is defined as 8, so the game's
  hand-written MIPS `va_start` macros become `__builtin_next_arg(last)` on i386.
* `port/CMakeLists.txt`: drops the orphan `src/main/nml_packet_add/` work copy, which
  duplicates `nmlPacketAddTransMicrocode`; the PS2 build does not compile it.

## How far boot gets (sample log, `XSG_LOG=1`)

```
[mem] RAM 0x00010000-0x02000000, EE HW 0x10000000, VU mem 0x11000000, GS priv 0x12000000, SPR 0x70000000
[mem] .../SLUS_204.69 segment 1 -> 0x00200000 (+0x138588)
[ovl] ov01: data 36240 bytes, bss 40880 bytes, 1073 PS2-address functions
[disc] ...(USA).iso: layer0 2077328 sectors, layer1 at LSN 2077312
[boot] entering game main()
[sceSifRebootIop #1] cdrom0:\IOP\IOPRP24D.IMG;1
[sif] LoadModule cdrom0:\IOP\SIO2MAN.IRX;1 (args 0 bytes) -> pretend loaded
   ... MCMAN MCSERV PADMAN LIBSD SSD RSSD
[kernel] CreateThread -> 2 func=0x81568ad prio=6 ...       (RSSD threads)
[sceSifCallRpc #1] client=0x83b5ef0 rpc=0x900 fno=0x1 mode=1 send=32 recv=32
[iop] -> EE server 0x901 command 4096                       (SsdInit unblocks)
[sceCdReadDvdDualInfo #1] -> dual=1 layer1=2077312
[disc] SearchFile \SYSTEM.CNF;1 layer 0 -> ok lsn 2673 size 79
[disc] SearchFile \XENOSAGA.00;1 layer 0 -> ok lsn 2674 size 169984
[sceCdRead #3] lsn=2675 sectors=82 buf=0xa80800             (archive index)
[disc] SearchFile \XENOSAGA.20;1 layer 1 -> ok lsn 2077583 size 6144
[scePadPortOpen #1] 0 0 0x8976b60
[sceGsResetGraph #1] 0 1 2 0
[sceGsSetDefDispEnv #1] psm=0 512x448
[disc] SearchFile \DATA\FONT1.TEX;1 layer 0 -> NOT FOUND
[kernel] CreateThread -> 4 func=0x818212a prio=9 ps2stack=0x1f7000+0x1000
   ... threads 5, 6, 7
[disc] SearchFile \DATA\SOUND\SED\REGIST.SWD;1 layer 0 -> NOT FOUND
[sceGsSyncV #1] mode=0 frame=0
[sceMcGetInfo #1] port=0 slot=0
[sceGsPutDrawEnv #1] ...   [sceGsPutDispEnv #1] ...
[sceMcGetDir #1] no memory card
[kernel] frame 600 (thread 4) ... frame 397000+          (steady state; no crash)
```

Crashes fixed along the way, in order:
1. `sceCdReadDvdDualInfo` is called with one argument (the SDK has two); the stub no longer
   writes the second.
2. Misaligned `movdqa` on `ps2_u128` data: converted data pieces now start at their PS2
   address modulo 16.
3. `xglVectorScale` argument order (MIPS float-register ABI; blocker 2).
4. Stores to VU0 data memory at `0x11004800`: that range is now mapped.
5. A hang in `xglDmaDirectNormal`, polling `CHCR.STR`: the DMAC stand-in now clears it.

## Next blockers, in priority order

1. **ControlEntry does not reach the overlay load.** Thread 7 is asleep in steady state, and
   no `sceSifLoadElf` is logged. It runs right after the boot sound-bank lookups
   (`\DATA\SOUND\SED\REGIST.SWD`...), which fail as loose ISO paths; on retail they are
   presumably served from the `XENOSAGA.xx` archives through `xglCdArc*`. Then it waits on
   the sound DMA (`SsdAddWaveData` / `SsdSpuDmaCompleted`) and the RSSD request/reply cycle,
   which the minimal IOP does not answer beyond init. Next: trace thread 7 (temporarily
   `XSG_LOG=2`, or breakpoints on `xglSoundLoadEffect`/`SsdAddWaveData`), and extend the IOP
   model (RSSD fno → server 0x901 replies such as 37/218) or make the EE-side sound queue
   complete at once.
2. **Float-argument ABI mismatches.** On the EE, float arguments go in FPRs whatever their
   position, so `f(float, ptr, ptr)` and `f(ptr, ptr, float)` are the same call, and the decomp
   has differing per-TU prototypes that matched anyway. On i386 they corrupt arguments
   silently. One case is fixed (`xglVectorScale`); a scanner should compare every function's
   declarations and definition across TUs by float-parameter position (the Stage 1
   inventory's prototype data is a start) and remap them the same way.
3. **Inline asm (1039 stubbed sites)**: VU0 macro-mode math (`m_math.c`, `xgl_2.c`, `sef.c`,
   `nml_model_set.c`) currently does nothing, so matrices and vectors are garbage. The
   simulation will diverge as soon as real geometry is used. That needs a VU0 macro-op
   emulation layer behind `PS2_ASMV`.
4. **Overlay C data reset**: the overlays' C `.data`/`.bss` are not restored when an overlay is
   activated (only the converted data is).
5. **Hard-coded PS2 addresses into main data** read the original image copy, but writes go to
   the host's symbol copies, so the two can disagree (e.g. ov01 `data_unit_org_get` at
   `0x426F48`, ov02 `title.c` `&D_2A007F8`, which is outside the 32 MiB RAM).
6. **Graphics/input/sound**: GS/VIF/DMA output is built and dropped; the pad reports no
   buttons; the memory card reports no card; MPEG ends at once; there is no SPU.
7. Warnings not yet reviewed (int-conversion and incompatible-pointer types, ~650). Some will be
   real 64-bit/u128 truncations now that quadwords are structs.

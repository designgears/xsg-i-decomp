# PC port, Stage 1: host-compile blocker inventory

Measured on 2026-11-06 with `gcc (GCC) 16` (`/usr/bin/gcc`), on branch `pc-port`.
Scope: the **347 game C TUs** in `src/main` (184), `src/ov01` (37), `src/ov02` (15),
`src/ov10` (11), `src/ov11` (3), `src/ov12` (96, including
`src/main/nml_packet_add/nml_packet_trans_microcode.c`, which the PS2 build pulls in via cinc).
`src/audio` (IOP) is out of scope. The SDK and runtime TUs listed in
`config/objects/main.compile.json` (`category: sdk|runtime|asm`, 224 TUs) have no
C source in `src/`. They exist only as `build/expected/*.o` and `build/hasm/*.o`, so
they make up the platform surface below and are not part of the survey.

Nothing under `src/`, `include/`, `config/` or `build/` was modified.

## How the PS2 build compiles a TU

Taken from `configure.py`, `tools/tu/ninja_main.py`, `tools/tu/ninja_ovl.py`,
`tools/tu/cc_tu.sh` and the generated `build/<unit>/build.ninja`:

* cwd is `build/<unit>/`. In that directory `src` is a symlink to `../../src`, and
  `include/` is a copy of the repo `include/` (`common.h`, `include_asm.h`, `labels.inc`,
  `macro.inc`, `shared.h`, `main/ ov01/ ov10/ ov12/ umbrella/`).
* cpp: `ee-gcc -nostdinc -fno-builtin -E -O2 -G0|-G8 -Iinclude -I. <tu>.c`. There are no
  `-D` defines, and no game file includes a `<system>` header.
* cc1: `ee-gcc 2.96 -S -O2 -G0|-G8` on the `.i`. Then `cinc.py` (main `ccas_c`) and
  the LA29 `ee-as`.
* Per-TU flags in `config/objects/*.compile.json` are only `-G0`/`-G8`.
* No game TU uses `INCLUDE_ASM`/`ACCEPTED_ASM`/`INCLUDE_RODATA` any more; they appear only
  in comments. `include/include_asm.h` emits one top-level
  `__asm__(".include \"include/labels.inc\"")` unless `SKIP_ASM` is defined.

So the host equivalent, run from the repo root, is `-Iinclude -I.`. No generated header
under `build/` is needed.

## Reproduce

```sh
python3 port/scripts/host_compile_survey.py        # ~4 s on 16 threads
# results: port/build/survey/{m32,m32stub,m64,m64stub}.json (per-file diagnostics)
#          port/build/survey/summary.json (all counts below)
#          port/build/survey/obj/*.o (host objects of the m32stub pass)
```

Common flags for every pass:
```
gcc -std=gnu89 -c -fno-builtin -fno-common -fpermissive -fno-pic -fno-stack-protector \
    -Wno-error=implicit-function-declaration -Wno-error=implicit-int \
    -Wno-error=int-conversion -Wno-error=incompatible-pointer-types \
    -Wno-error=return-mismatch -Wno-error=declaration-missing-parameter-type \
    -Wint-to-pointer-cast -Wpointer-to-int-cast \
    -include port/include/ps2_host_compat.h -Iinclude -I.  <tu>.c
```

| pass | extra | what it measures |
|---|---|---|
| `m32` | `-m32` | the sources as they are. MIPS asm reaches the x86 assembler |
| `m32stub` | `-m32 -DTI=DI`, compiled from a mirror tree | the C-level blockers behind the asm/TI ones |
| `m64` | (x86-64) | the sources as they are, LP64 |
| `m64stub` | `-DTI=DI`, mirror tree | C blockers plus pointer-size diagnostics on LP64 |

**Asm strategy.** Macros can't remove `__asm__ __volatile__ (...)`, because the qualifier
between the keyword and `(` defeats a function-like macro. So the stub passes compile
`port/build/survey/stubtree/`, a copy of `src/` and `include/` in which the script makes
these textual changes:

* every asm *statement* (balanced parentheses, string-aware) becomes `((void)0)`;
* file-scope `__asm__(".equ ...")` is dropped;
* `register T x asm("$n")` loses its binding.

Asm *labels* (`extern T x[] __asm__("sym")` and `#define SYM(n) __asm__(#n)`) are kept,
because gcc on x86 accepts them. The inline-asm counts below come from a static scan of
the same statements.

`port/include/ps2_host_compat.h` is force-included. It defines `SKIP_ASM` and no-op
`INCLUDE_ASM`/`ACCEPTED_ASM`/`INCLUDE_RODATA`, and supplies `ps2_u128`/`u_long128`/`u128`
(`unsigned __int128` on x86-64, a 16-byte-aligned struct on i386) as the intended
replacement for the game's own `mode(TI)` typedefs.

## Results: error counts

Files that compile cleanly, out of 347:

| pass | OK | failing |
|---|---|---|
| m32 | **239** | 108 |
| m32stub | **308** | 39 |
| m64 | 243 | 104 |
| m64stub | 308 | 39 |

Errors by category (count of errors, then files affected):

| category | m32 | m32stub | m64 | m64stub |
|---|---|---|---|---|
| MIPS inline asm (assembler rejects / unknown register name / `register asm`) | 491 (44 files) | 1 (1) | 610 (71) | 1 (1) |
| 128-bit `mode(TI)` (`unable to emulate 'TI'`, i386 only) | 60 (44 files) | 0 | 0 | 0 |
| conflicting declarations (`static` follows non-static: 67; conflicting types: 33; redeclared: 1) | 101 (35) | 101 (35) | 101 (35) | 101 (35) |
| GNU cast-as-lvalue (`*((int *)p)++`) | 3 (2) | 3 (2) | 3 (2) | 3 (2) |
| other (`duplicate 'static'` in sef.c; incomplete `EnemyWork` array in call_java_method.h) | 2 | 2 | 2 | 2 |
| missing SDK headers | 0 | 0 | 0 | 0 |
| implicit int / implicit function declaration | 0 | 0 | 0 | 0 |

The m32 and m64 counts for asm and TI are lower bounds. gcc only reaches the assembler
for a TU with no C errors, and with TI errors present the asm errors in those files never
show up. The m64 row has more asm errors because there TI is accepted, so more files get
as far as assembly.

Warnings that matter for portability (same in every pass unless stated otherwise):

| warning | count |
|---|---|
| incompatible pointer types | 520 |
| int <-> pointer conversion without cast | 136 |
| conflicting-decl warnings | 37 |
| **LP64 only:** cast to pointer from integer of different size | 1228 |
| **LP64 only:** cast from pointer to integer of different size | 988 |
| **LP64 total** (m64stub, 170 of 308 files) | **2216** |

LP64 hot spots: sef.c 177, ov01/menu.c 146, ov01/battle_init.c 117, ov01/cmd.c 87,
ov10/card_fread.c 58, ov12/rg_robot.c 57, Undulate.c 53, ov10/cgp.c 50,
ov12/rg_announce.c 50, res_get_path.c 48.

Struct-layout assumptions (static grep of `src` and `include`):
* 5917 `/* +0xNN */` offset comments;
* **767 pointer-typed members carrying a fixed-offset comment**, which break silently on LP64;
* 34 `offsetof` uses or sizeof-array asserts;
* about 1464 `(u32|int)` casts of identifiers;
* widespread `AF(float, a, 0x66c)` / `*(int *)(p + 0x44)` raw-offset accesses into structs
  that mix pointers and data.

Disk data (save files, archives, scripts, Java-VM class records) is also laid out with
32-bit pointers or offsets.

### Top files

m32 (as-is, asm-dominated): align.c 116, ov12/rg_geom_pillar.c 66,
ov12/rg_geom_collision.c 38, ov12/rg_geom_tray.c 30, ov01/m_cam.c 28, outer_product.c 20,
ov01/m_ef.c 20, xgl_camera.c 15, jc_face.c 14, m_math.c 13, sv_get_image_item.c 13.

m32stub (C-only blockers): sv_get_image_item.c 10, ov12/xrg_sysinit.c 8,
ew_sprt_set_cursor_uv.c 7, sef.c 7, end_print.c 5, nml_model_set.c 5, act_3.c 4,
init_uwamono_sys.c 4, ov01/calc.c 4, ov12/xrg_sound.c 4.

All 39 TUs still failing after stubbing: act_2, act_3, call_java_method, end_print,
ew_sprt_set_cursor_uv, game, game_defocus, game_movie, init_uwamono_sys, js_init,
karakama_test, matrix_print, menu_1, menu_ether, nml_filter_set, nml_model_set,
nml_packet_add, sc_get, sef, seisan_count_init1, set_path, sv_get_image_item, xgl_1,
xgl_cd (main); battle_init, calc, data_unit_org_get, m_ef_create_msp_00/msp_02/smp_01,
menu, snd (ov01); tya_uml (ov02); rg_announce, rg_battlemgr, rg_robot, xrg_event,
xrg_sound, xrg_sysinit (ov12).

## Inline asm breakdown (static scan)

There are **1043 asm statements in 83 files**, plus 26 `register ... asm("$n")` variables
in 9 files, 161 asm symbol labels in 35 files, and 9 file-scope asm directives
(`.equ`/`.size` aliases) in 5 files.

| kind | instructions |
|---|---|
| VU0 macro mode (COP2): `lqc2`, `sqc2`, `qmtc2`, `qmfc2`, `ctc2`, `v*`, `vcallms`/`vcallmsr` | **3754** |
| 128-bit GPR `lq`/`sq` (quadword copy and clear) | 904 |
| other scalar MIPS / FPU (`mfc1`, `mtc1`, `c.lt.s`, `bc1f`, `addiu`, `ldl`/`ldr`, `movz`...) | 347 |
| empty or `nop` compiler barriers | 217 |
| MMI (`pcgth`, `pextlh`, `pcpyld`, `paddw`...) | 42 |
| system (`sync`, cop0, cache) | 6 |

Top mnemonics: lqc2 966, sqc2 596, sq 476, lq 428, nop 143, vmulax.xyzw 129,
vmadday/vmaddaz/vmaddw.xyzw 105 each (4x4 matrix times vector), vnop 90, mfc1 89,
**vcallms 82**, mtc1 75, qmtc2(.ni) 140, vsub.xyz 69, vmr32 63, vadd.xyz 62, vwaitq 62,
vdiv 35, **vcallmsr 30**, vsqrt 15, vrsqrt 13, vclipw 19, vftoi/vitof 43.

`vcallms` targets are VU0 micro subroutines at `0x20` and `0xe8`, plus `vcallmsr $vi27`.

Top files: sef.c 161, nml_model_set.c 72, xgl_2.c 64, m_math.c 59, ov01/m_cam.c 33,
ov01/m_ef_create_kosbw_02.c 33, ov01/m_ef_create_smp_01.c 32, ov12/rg_geom_ball.c 30,
sv_get_image_item.c 29, face_point.c 28, xgl_camera.c 28.

## Platform API surface

### A. Host-link view (exact, from `nm -u` of the 308 host objects)

These are undefined symbols that no game object defines, attributed to their PS2 defining
object through `build/*/build/*.elf.map` and `main.compile.json` categories.

| defined on PS2 by | symbols |
|---|---|
| SDK C TUs (`category: sdk`) | 98 |
| runtime: libc, libm, soft-float (`category: runtime`) | 37 |
| hand-written asm (`crt0`, `kernel_syscalls.s`, `mem*`/`str*`) | 24 |
| game data carves (`build/data/*`: VM class tables, `scommon`, rodata pieces) | 511 |
| game C in the 39 not-yet-compiling TUs, or game data | 3864 |
| unresolved: VU1 microcode labels (`PacketDataNewVu1*MicroCode`...), `func_A19xxx` ov-addresses, `_gp`, `__builtin_args_info` (gcc 2.x varargs) | 95 |

### B. Source view (call-site counts across all 347 TUs, so it includes the 39 failing TUs)

| library | functions | call sites | functions used |
|---|---|---|---|
| libvifpk (`sceVif1Pk*`) + `sceDevctl` | 27 | 808 | PkCnt 114, AddDirectDataN 93, Ref 84, AddDataN 52, Open/CloseDirectHLCode 43/42, AddCode 42, AddGsAD 39, AddGsPacked 36, Open/CloseUpkCode 35/35, AddUpkData128N 31, Devctl 22, OpenDirectCode 18, AddGsData 17, Open/CloseGifTag 16/13, Init 13, Size 11, Terminate 10, Align 10, ... |
| sce file I/O (`sceOpen` / `sceRead` / ..., host0: / cdrom0: / hdd / pfs) | 17 | 205 | sceClose 45, sceOpen 38, sceWrite 24, sceRead 22, sceLseek 20, sceMount 10, sceMkdir 8, sceDopen/Dclose 6, sceUmount 5, sceIoctl2 5, sceFormat 5, sceChstat 4, sceDread 3, sceFsReset 2, sceGetstat 1, sceRemove 1 |
| kernel (syscalls) | 16 | 113 | FlushCache 41, DIntr/EIntr 10/10, SyncDCache 9, Create/Start/Sleep/Delete/Terminate/WakeupThread 5/4/3/3/3/2, Wait/Signal/Create/Delete/iSignalSema 5/5/4/4/2, GetThreadId 3 |
| libgraph (`sceGs*`) | 12 | 73 | SyncPath 19, SetDefLoadImage 10, SetDefStoreImage 10, ExecStoreImage 10, ResetGraph 5, SetDefDispEnv 4, ExecLoadImage 4, SyncV 3, ResetPath 2, SyncVCallback 2, PutDrawEnv 2, PutDispEnv 2 |
| libcdvd (`sceCd*`, including streaming `sceCdSt*`) | 22 | 60 | Sync 10, DiskReady 6, Init 4, Pause 4, Mmode 3, GetReadPos 3, Status 3, Read 2, St{Init,Start,Read,Stat,Seek,Stop}, LayerSearchFile 2, ReadDvdDualInfo 2, PowerOff/POffCallback 2/2, GetDiskType, GetError, Break, ReadClock |
| libmc (`sceMc*`) | 12 | 55 | Sync 13, Open 11, Close 7, Write 5, GetInfo 5, Read 3, GetDir, Mkdir, Format, Unformat, Delete 2 each, Init 1 |
| libsif / sifrpc / IOP loading (`sceSif*`) | 18 | 36 | InitRpc 3, BindRpc/CallRpc/CheckStatRpc, RegisterRpc/SetRpcQueue/RpcLoop/RemoveRpc/GetNextRequest/ExecRequest (an EE-side RPC server), RebootIop, SyncIop, LoadModule, **LoadElf** (overlays), LoadFileReset, Init/Alloc/FreeIopHeap |
| libipu / libmpeg (FMV) | 11 | 35 | sceIpuSync 12, sceMpeg{Create,Init,Delete,Reset,AddCallback,AddStrCallback,DemuxPss,GetPicture,IsEnd}, sceIpuInit |
| libpad (`scePad*`) | 12 | 16 | Init, PortOpen, Read, GetState, GetReqState, InfoMode 3, SetMainMode, InfoAct, SetActAlign, SetActDirect, InfoPressMode, EnterPressMode |
| libc / libm / libgcc | 48 | 2435 | printf 487, memset 292, strcasecmp 225, strcpy 185, strcat 176, sprintf 107, strcmp 96, atan2f 69, strlen 59, sinf/cosf 41/38, plus soft-float helpers `fptodp`/`dptofp`/`dpmul`/`litodp`/`dptoli` (called **directly** by game C), `__divdi3`/`__moddi3` |

Not used directly by game C: sceDma, sceVu0 (the libvu0 API; the game uses inline VU0
instead), sceHdd, scePfs (only through sceOpen/sceMount/sceDevctl on `hdd0:` and `pfs0:`).
Sound goes through the game's own SSD/RSSD IOP driver over sifrpc (`src/audio` is IOP code).

## Hardware and address-space dependencies (static scan, comments stripped)

| what | occurrences | files | hottest |
|---|---|---|---|
| EE hardware registers `0x1000xxxx` (DMAC, VIF, GIF, timers, INTC) | 89 | 22 | xgl_1.c 15, xgl_dma.c 15, end_print.c 14, xgl_render.c 8 |
| `0x1001xxxx`..`0x110xxxxx` (VIF FIFOs, VU memory) | 11 | 9 | xgl_1.c, xgl_packet.c |
| GS privileged registers `0x1200xxxx` (CSR, PMODE, DISPFB...) | 7 | 2 | xgl_render.c 6, tya.c 1 |
| **Scratchpad `0x70000000`..`0x70003fff`** | **559** | **37** | nml_filter_set.c 146, game_defocus.c 75, db_light_write.c 64, tya_menu_bg_entry.c 48, game_camera.c 35 |
| `0x2xxxxxxx` constants (uncached alias, plus false positives such as float bit patterns) | 69 | 28 | end_print.c 11, db_light_write.c 9, game.c 7 |
| `0x3xxxxxxx` constants (uncached-accelerated) | 21 | 17 | xgl_1.c 3 |
| explicit `\| 0x20000000` / `\| 0x30000000` / `& ...` | 7 | 5 | act_2.c 2, game.c 2, sdv.c, sv_get_image_item.c, xgl_1.c |
| `0x8xxxxxxx` / `0xAxxxxxxx` constants (KSEG, or just sign or flag masks) | 51 | 22 | (mostly flag masks, needs manual triage) |

Hard-coded absolute addresses: there are 95 unresolved `func_A19xxx`/`D_xxxx` references
from main into overlay addresses. All overlays link at fixed, overlapping EE addresses
(ov01/ov10/ov11/ov12 share one region, and ov02 is embedded in main).

### VU microcode

* **VU0 micro subroutines.** A `Vu0MicroCode` blob (`build/main/build/vu0/Vu0MicroCode.o`,
  symbols `PacketDataVu0MicroCode` / `PacketSizeVu0MicroCode`) is uploaded by game code. It
  is called from inline asm: `vcallms 0x20` 42 times, `vcallms 0xe8` 38 times,
  `vcallmsr $vi27` 31 times.
* **VU1 renderer.** `.vudata` (66,576 bytes, `blobs/vudata_rest`) holds the VU1 microcode
  programs (`PacketDataNewVu1{,Add,Drop,Env,AddEnv,TexEnv,Pro}MicroCode`, ...). They are
  uploaded through VIF1 packets (`sceVif1PkAddDirect*`, `nmlPacketAddTransMicrocode` 13 calls,
  `nmlPacketAddWaitMicrocode` 8 calls) and do all the transform, lighting and clipping for
  the nml model renderer, and probably the xgl 2D paths.

### Overlay loading

`xglCdLoadOverlay(id)` (src/main/xgl_cd.c:1669, 29 call sites) builds the path
`<head>OVnn<tail>`, then runs `FlushCache(0); FlushCache(2); sceSifLoadElf(path)`, so the
IOP DMAs the overlay ELF to its fixed link address. It is guarded by `loaded_overlay`.
Callers include control_entry.c (`xglCdLoadOverlay(2)`, `selection >> 24`). On the 308 host
objects there are **no duplicate global definitions** across main and all overlays
(`-fno-common`).

## Recommended approach per blocker

1. **VU0 inline asm** (3754 instructions, the biggest item). Build a tiny C "VU0 macro"
   emulation layer: `vf[32]` as `float[4]` vectors, `ACC`, `Q`, `I`, `vi[16]`. Rewrite each
   asm block, at source level and under `#ifdef PS2_HOST`, as calls to inline helpers. Most
   blocks are a dozen recurring idioms (mat4 x vec4 via
   vmulax/vmadday/vmaddaz/vmaddw, normalize via vdiv/vsqrt/vrsqrt, vector add/sub/outer
   product, `vclipw`, `vftoi`/`vitof`), so pattern-match them into named functions
   (`ps2_vu0_mul_mat_vec`, ...) and translate the long tail by hand. A small transpiler
   from template to C is feasible because the operand syntax is regular. The two VU0 micro
   subroutines (`vcallms 0x20` and `0xe8`) need to be disassembled once and hand-ported as C
   functions that operate on the emulated register file. The register file must be
   **persistent across asm blocks**, because the game passes values between blocks in
   vf registers.
2. **128-bit lq/sq** (904), **MMI** (42) and **`mode(TI)`** (60 errors, 44 files). On
   x86-64 `__int128` covers TI as-is. For `-m32`, change the game typedefs (source-level,
   behind `#ifdef`) to `ps2_u128` from the compat header. `lq`/`sq` are 16-byte copies and
   zero stores: use `memcpy(dst, src, 16)` / `memset`, or SSE `_mm_load_si128` with matching
   alignment. Port MMI one by one with SSE2 intrinsics or plain C.
3. **Scalar MIPS asm, register-asm variables, empty barriers** (347 + 26 + 217). Use plain
   C. The `register asm("$f21")` and `"=f"` bindings exist only to force register
   allocation for matching, so drop the binding. Map barriers to
   `__asm__ volatile("" ::: "memory")`. Return-address tricks (`sw $31`) map to
   `__builtin_return_address(0)`. `__builtin_args_info` / `__builtin_next_arg` (gcc 2.x
   varargs, 9 uses) become `<stdarg.h>`.
4. **Conflicting declarations** (101 errors in 35 files). Most are `static` definitions that
   follow a non-static prototype in a header, or prototypes whose types differ from the
   definition. ee-gcc 2.96 accepted them and modern gcc has no flag to downgrade them. Fix
   them in the shared headers/prototypes: mechanical, roughly 1 to 3 days, and probably
   matching-neutral since it only touches declarations. Re-check against the PS2 build.
5. **Cast-as-lvalue** (3), **duplicate static** (1), **incomplete array type** (1). Small
   source edits; they could be upstreamed to decomp-work if they still match.
6. **Pointer size.** **Target `-m32` (ILP32) for the first playable port.** It removes
   2216 LP64 cast diagnostics, 767 pointer members at fixed offsets, the raw-offset struct
   accesses and the 32-bit pointers in on-disk data (VM class records, scripts, saves) at a
   stroke, and i386 multilib works with gcc 16. Track LP64 as a later stage (it would need
   `uintptr_t` sweeps, explicit relocation of loaded data and serialisation layers).
   Expect behaviour bugs from the 520 incompatible-pointer and 136 int-conversion warnings
   to stay benign on ILP32. Keep `-fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks`.
   Use the 2.96-era float semantics (`-ffloat-store` or SSE `-mfpmath=sse`); the PS2 FPU is
   non-IEEE (no denormals or infinities, and saturating), which matters for replay or
   physics parity.
7. **SDK surface** (about 140 functions). Write a host "libps2" shim with one module per
   library:
   * **sce file I/O, libcdvd, libmc**: map to POSIX on an extracted disc image directory and
     a memcard directory. `sceCdSt*` streaming becomes a file reader thread.
   * **kernel threads and semaphores**: pthreads or, better, cooperative fibers. The EE
     kernel is non-preemptive priority scheduling and the game relies on that ordering.
   * **FlushCache / SyncDCache / DIntr / EIntr**: no-ops or a global lock.
   * **libpad**: SDL3 gamepad.
   * **libsif/sifrpc and the SSD sound driver**: replace the RPC endpoints with a host audio
     backend that implements the IOP side's command protocol.
   * **libmpeg/libipu**: an FFmpeg-based PSS decoder.
   * **libgraph plus libvifpk (808 call sites)**: keep the packet-building API, but have the
     packet sink feed a software **GS/VIF/GIF interpreter** that rasterises GS primitives
     with OpenGL or Vulkan. The game emits raw GIF/VIF packets, so emulating the GS command
     stream at the packet level is far less work than rewriting every renderer.
   * **libc**: use the host libc. The soft-float helpers `fptodp`/`dptofp`/`dpmul`/`litodp`/
     `dptoli` become one-line C functions.
8. **VU1 microcode** (66 KB `.vudata`). This is the hardest piece of the graphics path. The
   options are (a) a VU1 interpreter or JIT, as PCSX2 and Play! do, running the original
   microcode on the packets the nml renderer builds, or (b) re-implementing each program
   (about 7 variants: base, add, drop, env, add-env, tex-env, pro) as C or compute shaders
   after disassembly. Start with (a) for correctness and use (b) for speed later.
9. **Hardware registers, scratchpad, uncached aliases.**
   * Scratchpad (559 uses): allocate a 16 KB aligned host buffer and make the `0x70000000`
     literal a macro (`PS2_SPR(off)`). This needs a source pass over 37 files, because a
     literal address cannot be remapped from a header.
   * The 0x1000xxxx/0x1200xxxx register accesses (about 107, concentrated in xgl_1, xgl_dma,
     xgl_render and end_print): route through `ps2_hw_read32/write32()` into the GS/DMA
     emulation layer.
   * Uncached ORs (`| 0x20000000` / `0x30000000`): strip them with a `PS2_UNCACHED(p)` macro
     that is the identity on the host.
   * Alternative with zero source changes: `mmap` fixed host mappings at 0x70000000 and the
     register pages under `-m32`, and trap register pages with SIGSEGV or userfaultfd. This
     is useful for bring-up but too slow for DMA-heavy paths.
10. **Overlays.** Link main and all overlays into one host executable (there are no symbol
    collisions in the compiled subset). Turn `xglCdLoadOverlay` into "call the overlay's
    init/reset of its static data" (re-run `.data` initialisation from a saved copy, because
    reloading an overlay used to reset its globals). Resolve the 95 `func_A19xxx`-style
    absolute overlay references to real symbols.

## Suggested next steps (Stage 2)

* Fix the 101 conflicting declarations, 3 cast-lvalues and 2 others, so that **all 347
  TUs compile** in the stub passes.
* Then link the 347 objects against an auto-generated stub library for the about 160
  platform symbols, to get an unresolved-symbol-clean `-m32` link.
* Start the VU0 helper layer with the 4 most common idioms, which cover the
  vmulax/vmadday/vmaddaz/vmaddw family, lqc2/sqc2 and vcallms.

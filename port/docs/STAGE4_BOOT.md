# PC port, Stage 4: boot through the logos and the title screen into overlay 1

> **Settings (2026-11-07):** the port no longer reads `XSG_*` environment variables. Every
> `XSG_FOO=v` in this document is now `[section] key = v` in settings.ini (the mapping is the
> "replaces" column of [MENU.md, settings.ini reference](MENU.md#settingsini-reference)); run
> with `XSG_CONFIG=<file> port/build/xsg`. Samples: [examples/](examples/).

Branch `pc-port`, 2026-11-07. Builds on [STAGE3_BOOT.md](STAGE3_BOOT.md); build, memory map,
data pipeline and overlay strategy are described there and still apply.

**Result:** headless boot now gets through `InitializeSystem`, the boot sound-bank upload
(RSSD over SIF RPC), the HDD and memory-card checks, both logo movies (each ends at once: there
is no IPU), and into the ov02 **title screen**, whose state machine runs at 60 Hz. Without
input, the title times out every 600 frames and `ControlEntry` re-enters it, as on the PS2.
With a scripted START press (`XSG_PAD=300:0x0800`), the title picks New Game, `ControlEntry`
loads **overlay 1** (`sceSifLoadElf("cdrom0:\OV01.OVL;1")`, C data restored) and calls main's
`0x245C58` entry. A few frames later it crashes in `xglFontFlush` (render thread), see
blocker 1.

## New environment variables

| variable | meaning |
|---|---|
| `XSG_FAST=1` | run unthrottled; otherwise vblanks are paced at 60 Hz real time |
| `XSG_FRAME_US` | vblank period in µs (overrides 60 Hz; 0 = unthrottled) |
| `XSG_PAD` | scripted input on port 0: `frame:mask[:hold],...`, SCE_PAD masks (`0x0800` START, `0x0040` CROSS, `0x0020` CIRCLE, `0x1000` UP, `0x4000` DOWN), held `hold` vblanks (default 6) |
| `XSG_MC_DIR` | host directory backing the memory card in slot 1 (default `~/.local/share/xsg/mc0`) |
| `XSG_MC=0` | no memory card in either slot (the boot then waits for a button on the "no card" screen) |

On a timeout (`XSG_TIMEOUT`), the thread dump now shows where each blocked thread blocked
(`blocked at:` raw addresses, `addr2line -f -e port/build/xsg`).

## 1. Float-argument / ABI mismatch scanner

`port/scripts/check_call_types.py` (about 2 s, `-j` parallel). For every C TU the host builds it
runs the host compiler with `-fsyntax-only -aux-info`, which lists every function declaration
and definition gcc saw, with file:line and prototype or K&R. It takes implicit declarations from
the warnings, and classifies every parameter/return type string with a probe compile
(`__builtin_classify_type` and `sizeof` of a parameter of that type, read back from the `.comm`
size in the asm), so typedefs resolve exactly as gcc sees them. Classes: `F` float, `D` double,
`I` ≤4-byte int, `L` 8-byte int, `P` pointer, `S<n>` struct. It compares each declaration with
the definition (or the majority declaration when there is none) in i386 terms: int/pointer/void
returns and narrow ints are one class, double and 8-byte ints are the same two stack slots as
arguments, and extra trailing arguments are ignored. Findings:

* `RET`: return class differs (float vs void/int is an x87-stack leak or garbage; a 16-byte
  struct return is a hidden pointer argument);
* `ARGS`: a parameter slot sequence differs (float position, 4 vs 8 bytes);
* `NOPROTO`: a definition with float or 8-byte parameters is called through an unprototyped or
  implicit declaration.

Exit 1 on any finding not in `port/scripts/check_call_types_allow.txt`. Status: **0 open**
(2 allow-listed). The 59 findings, all fixed under `PORT_HOST`:

| kind | functions | fix |
|---|---|---|
| float position (EE: FPRs and GPRs are numbered separately) | `Check_InsideFan`, `Check_InsideFan_Wooo`, `Check_MoveSquare`, `Check_Straight`, `Check_Straight_ID`, `Check_Wall` (×2), `CreateUwamono`, `CrossPointUwamono`, `InitXrgPaint2DLine3D`, `nmlFilterStealthMake`, `nmlModelSetFadeInInterrupt`, `xglVectorInter`, `xglVectorScale` (×2), `xglVectorScaleAddXYZ`, `xglVectorScaleXYZ` | true prototype plus a same-name macro that reorders the call's arguments (k-th float to k-th float, k-th int to k-th int) |
| `void` view of a float function (x87 stack leak) | `UnduCheck` (×4), `XrgNormalizeVector` (×4), `RgRobSubTargetting`, `xglFRand` (`int`) | declare `float` |
| float left in `$f0` by a tail call, definition `void` | `Get_Height` (`UnduCheck`), `MMathCalcDir` (`srsAtan2`) | definition returns the value under `PORT_HOST` |
| struct return seen as `void` | `JNT_getRootTrans` (16-byte struct: hidden pointer, `ret $4`) | declare the struct return |
| 4 vs 8 bytes | `Check_Undu`, `MenuNumberTextGet` (×3), `grGsRegSet` (×5), `nmlModelSetAlpha` (×2), `nmlPacketAddGsFrame` (×6), `nmlPacketAddGsFrame1` (×2), `nmlPacketAddGsZbuf` | declare the 64-bit parameter |
| u64 view of a soft-float double result | `dpmul`, `litodp` in ov11 `mini_g.c` | bound with an asm label to `xsg_dpmul_bits`/`xsg_litodp_bits` (u64 result) in the runtime; allow-listed |

Each fixed declaration is `#ifndef PORT_HOST <original> #else <host> #endif`, so the PS2
tokens are unchanged.

## 2. ControlEntry stall: archive lookup and the RSSD protocol

**How `\DATA\...` files are found.** `xglCdGetFilePos` (src/main/xgl_cd.c) first walks up to
four archive indexes (`ArcHeader` and the next three 0x18-byte records), and only falls back to
`sceCdLayerSearchFile` (a loose ISO file) when none matches. `xglCdArcInit` reads each index
from sector 0 of `XENOSAGA.00` (byte 0 = header sector count, 83), `.10` (4) and `.20` (3,
layer 1). The index is a front-coded path tree: a record byte `b` with bit 7 set is a directory
(`b&0x3f`−2 name bytes, preceded by a pop count), otherwise a file (`b&0x3f`−1 name bytes, then
a 3-byte LBN offset and a 4-byte size, plus 3 more bytes when bit 6 is set). There are 8573 + 349
+ 266 entries, all lower case (`namco.ipu`, `endou\cfwin.xtx`, `movie\mpeg2\prologue.pss`...).

The stall was not the sound driver. `gen_host_data.py` put the modulo-16 start padding of each
converted data piece *after* the piece's first label, so `D_004D2468` ("xenosaga.10") read as an
empty string, the second archive was looked up as `\;1`, and lookups fell through to loose
files. With that fixed, every boot file comes from the archives.

**RSSD protocol** (EE library src/main/ssd_init.c and ssd_1.c, IOP module src/audio/rssd):

* EE → IOP: SIF RPC server `0x900`, `fno` = command, mode 1 (nowait). The send and receive
  buffer is the same 32-byte packet `{s16 command, s16 error, int param, int value,
  int data_size, int arg[4]}`, followed by `data_size` bytes (≤ 0x10000). `RssdCallFunc` takes
  `RssdWork.sema_id`, sets flag bit 3, and its end callback copies the reply into
  `RssdWork.response`, clears bit 3 and signals the sema. The IOP's `RssdSifRpcServer` runs
  `RssdExecFunc` (command 4 = a batch of packets) and writes the packet back, with
  `error = SsdGetFuncErrorCode()` and results in `value`/`arg`.
* IOP → EE: the IOP binds the EE server `0x901` (served by the EE's `RSsdSifRpcThread` in
  `sceSifRpcLoop`, or polled by `SsdSpuDmaCompleted(1)`), same packet layout:
  `0x1000` init done (`arg0` runtime buffer, `arg1` size, `arg2 0x6001f4`, `arg3 0x10250`,
  copied by `RssdBusy`); `0x1b` SPU read chunk (`arg1` bytes follow, `arg2` 0 = last);
  `0x25` wave chunk DMA done (`arg0` status: ≥ 0 wakes `RssdBackNextWaveThread`, which sends
  the next ≤ 64 KiB chunk as command `0x21`; bit 2 of `RssdWork.flags` is the busy bit
  `SsdSpuDmaCompleted` spins on); `0xda`/`0xdb` sampling DMA / key-off; `0x0a` stream end
  (`arg0` channel mask); others go to the server callback.
* Commands seen at boot: `0x01` init, `0x20` add wave data (first 64 KiB), `0x21` next wave
  chunk, `0x60` add effect data (result `value` = bank id).

`port/runtime/iop.c` implements SIF RPC with a queue of IOP → EE calls (delivered after the
reply and end callback, by the thread parked in `sceSifRpcLoop`) and an RSSD model: `0x01` →
`0x1000`; `0x20`/`0x21` → bank id and `0x25`(0); `0x1b` → one empty final `0x1b`; `0x60`/`0x30`
/`0x31`/`0x38` → ids and free memory; `0xd2` → `0xda`; status queries (`0x24`, `0x62`,
`0x80`–`0x82`, stream status) → idle/0; `0x100` → free SPU memory; anything else acknowledged.
No sound is produced.

Beyond the sound driver, the boot needed:
* a **memory card** (`port/runtime/memcard.c`). With no card, `mc_check` (ov02 logo.c) waits
  for X on the "no memory card" screen. Slot 1 is now a formatted 8 MB PS2 card backed by a
  host directory (GetInfo/Open/Read/Write/Seek/Close/Mkdir/Delete/GetDir with `*` globs);
  slot 2 is empty;
* `sceCdStStat` returning the ring size (it returned 0x7fff, and the logo player then streamed
  64 MB into a stack buffer);
* `sceIpuSync` timing out, so `xglMoviePlay` returns −1 and each movie ends;
* **`PS2_LONG`**: the EE's `long` is 64 bits. `XglMovieTag {int tag, addr; unsigned long pad;}`
  was 12 bytes on the host, which moved every later movie-record field and smashed the logo
  player's stack. All 180 standalone `long`s in src/ are now `PS2_LONG` (include/common.h:
  `long` on the PS2, `long long` under `PORT_HOST`). This is token-identical on the PS2.

## 3. Overlay C data reset

`link_xsg.py` renames each overlay's compiled-C `.data`/`.bss` to `xsgcd_<ov>`/`xsgcb_<ov>`.
`overlay.c` snapshots both those and the converted `xsgd_`/`xsgb_` ranges at startup. On
activation it restores both and zeroes both `.bss` ranges, as the PS2 did by reloading the
image (ov10 27 B, ov11 284 B, ov12 2064 B of C data; the rest is in the converted data). ov02 is
active from boot: it is part of the main ELF, and `xglCdInitial` sets `loaded_overlay = 2`.

## 4. Frame pacing

`xsg_vblank` sleeps to absolute `CLOCK_MONOTONIC` deadlines, 16.67 ms apart (60 Hz). Game time
between vblanks counts toward the frame, and more than 4 frames behind resyncs. Measured: 300
vblanks in a 5 s run. `XSG_FAST=1` gives ~19,000 vblanks/s.

## 5. Fixed PS2 addresses

* `link_xsg.py`: a bare-address symbol from the PS2 link scripts that falls inside a main data
  object the host defines (`config/symbols/main.txt` extent) is `--defsym`'d to *that object +
  offset*. Reads and writes through either name then hit the same storage. 23 are aliased
  (`port/build/address_aliases.txt`): `D_426F48..D_426F8C` = `batDatBuf+8..`,
  `D_4229B6/C2/CE` = `orgData+0x76..`, `D_4A1A0C`/`D_4A6840` = `SaveData+..`. The others
  still point into the RAM image: small offsets, `D_004ADE80..` in the eh_frame/ctor area,
  heap/VRAM-staging addresses such as `D_1245C58`, and SDK labels.
* ov01 `m_mv.c`'s literal `0x004A90E0` is `&sRender` under `PORT_HOST`. The only other literal
  pointer into main, `(u8 *)0x00E00000` in xgl_packet.c, is a RAM work area, so it is fine.
* **`0x2A007F8` is not memory.** ov02 `title.c` returns `&D_2A007F8` on its 600-frame idle
  timeout. `ControlEntry` decodes a title return as `overlay = sel >> 24`,
  `entry = sel & 0xFFFFFF`, so it means "overlay 2, entry 0xA007F8" (= `Title`, i.e. show the
  title again). `D_1245C58` is the same encoding: overlay 1, main entry `0x245C58`. They stay
  plain values. `xsg_ps2_func` now also resolves main-text addresses (`xsg_ovtab_main`).

Placing the converted data at its PS2 addresses with a linker script was not done. The C
objects' own data cannot follow the PS2 layout anyway, and the aliasing covers every
fixed-address symbol the link scripts name.

## How far boot gets (sample log, `XSG_PAD=300:0x0800 XSG_FAST=1`)

```
[boot] entering game main()
[sceSifCallRpc #1] client=... rpc=0x900 fno=0x1 mode=1 send=32 recv=32
[iop_module_call #1] rssd cmd 0x1 args 20000 0 0 0 data 0
[iop_call_ee #1] -> EE server 0x901 command 0x1000
[sceSifRpcLoop #1] ...: serving IOP -> EE calls
[disc] SearchFile \XENOSAGA.00;1 layer 0 -> ok lsn 2674 size 169984
[disc] SearchFile \XENOSAGA.10;1 layer 0 -> ok lsn 700326 size 8192
[disc] SearchFile \XENOSAGA.20;1 layer 1 -> ok lsn 2077583 size 6144
[sceSifCallRpc #2] ... fno=0x20 mode=1 send=65568 recv=32      (REGIST2 wave bank, chunk 1)
[iop_call_ee #2] -> EE server 0x901 command 0x25
[sceSifCallRpc #3] ... fno=0x21 mode=1 send=33504 recv=32      (chunk 2, last)
[sceMcGetInfo #1] port=0 slot=0
[mc] port 0: ~/.local/share/xsg/mc0
[sceMcGetDir #1] /BASLUS-20469Xeno* max=100                     (logo.c mc_check passes)
[sceCdStStart #1] lsn=3226                                       (namco.ipu from XENOSAGA.00)
[sceCdStStart #2] lsn=4092                                       (logo_msi.ipu)
[sceCdReadClock #1]                                              (Title runs; START at 300)
[sif] LoadElf cdrom0:\OV01.OVL;1 -> overlay 1
[ovl] overlay ov01 active (was 2)
[disc] SearchFile \DATA\SCENE\EV1000.A;1 layer 0 -> NOT FOUND    (probes; not in any index)
[disc] SearchFile \DATA\MOVIE\MPEG2\PROLOGUE.CAP;1 layer 0 -> NOT FOUND
[disc] SearchFile \DATA\MTNPACK\SCE010OP.FPK;1 layer 0 -> NOT FOUND
[fatal] signal 11 at address 0x85293 (frame 306, overlay 1)
```

Without `XSG_PAD`, the title loops on its idle timeout forever (no crash).

## Next blockers

1. **Crash after New Game**: render thread 4, `xglRenderEntry → xglRenderMove → xglFontFlush`
   calls a garbage address (0x85293) a few frames after ov01 is activated. It is likely a
   function pointer in converted data that was zeroed or is unrelocated (gen_host_data turns
   `.word .Lxxx` into 0), or a PS2 code address stored in data and called directly instead of
   through `xsg_ps2_func`. Next: break on the faulting call in `xglFontFlush`/
   `xglFontFlushCore` and see where the pointer comes from.
2. **Opening-scene files not found**: `\DATA\SCENE\EV1000.A`, `PROLOGUE.CAP`, `10OP.CAP`,
   `SCE010OP.FPK/.ARC` are in none of the three archive indexes. They are probably optional
   probes (the `.pss` movie is in `.20`), but confirm the fallbacks once the crash is fixed.
3. **Inline asm** (Stage 3 blocker 3): VU0 macro math is still a no-op.
4. **No IPU/MPEG decode, no SPU, no GS output**: movies end at once, sound is acknowledged
   only, and graphics packets are dropped.
5. **`long` in printf formats**: `%ld` with a `PS2_LONG` argument now passes 8 bytes. That only
   affects debug prints, but the host warnings list them.
6. Remaining fixed-address symbols that still read the RAM image (`D_004ADE80..` and others,
   see §5) should be checked one by one when they show up in a trace.

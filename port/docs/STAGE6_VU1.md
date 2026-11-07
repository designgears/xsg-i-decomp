# PC port, Stage 6: synchronous DMA, VU1 interpreter, past the New Game crash

Branch `pc-port`, 2026-11-07. Builds on [STAGE5_GS.md](STAGE5_GS.md).

**Result:** VIF1/GIF DMA now runs synchronously on the game's thread. A VU1 micro-mode
interpreter runs every MSCAL/MSCNT, and XGKICK output reaches the GS on PATH1. Both title
buffers now show the menu. On the title, VU1 only runs a two-instruction "kick a prebuilt
packet" program: 9 runs per frame, about 32 instruction pairs, PATH1 A+D packets that set GS
state and draw one full-screen sprite. The real geometry microcode (1163 pairs, uploaded every
frame) is not called until the 3D renderer runs. After New Game the game now reaches about
vblank 372 instead of 306. It is stopped by a fault in the event VM (`JTHREAD_waitFor`), and
every frame after New Game is black.

## 1. Synchronous DMA

`xglDmaDirectNormal` / `xglDmaDirectSrcChain` (src/main/xgl_dma.c) call `xsg_dma_kick(channel)`
under `PORT_HOST` right after the CHCR write, and gs_bridge runs the transfer at once. The
DMAC stand-in thread no longer touches ch1/ch2 (`XSG_DMA_THREAD=1` restores Stage 5's
asynchronous run). `sceGs*` packets still poll first, which keeps them in order.

**The "other buffer" black title** (Stage 5 blocker 3) was a race. The first run after the
change still showed a black FBP 0xE0 frame. Once the thread was removed from the graphics path,
both buffers show the menu in every run (`stage6_title_fbp_e0.png`, `stage6_title_fbp_70.png`).
Later runs with `XSG_DMA_THREAD=1` did not reproduce the black buffer either, so the race was
timing-dependent. The background itself is still black: the 512×448 `buf` at 0x01009800 is all
zeros, because it is the JPEG/IPU output.

## 2. VU1 interpreter (port/runtime/vu1.cpp, written for the port)

ps2-port-engine's `vu.cpp` was not vendored. It is built on that project's lifted COP2 runtime
(`ps2rt/lifted.h`, `generated.h`, `vu_math.h`) and its ahead-of-time compiled-program machinery.
Pulling those in would have been far larger than an interpreter (~700 lines) written against
our memory model.

* **Memory:** micro = `PS2_VU_BASE+0x8000`, data = `+0xC000` (16 KB each, wrapping). This is the
  memory the VIF1 decoder and EE already write.
* **Pairs:** the upper op is computed first into a temporary. The lower op then executes, reading
  the pre-upper registers, and the upper result is committed last, so it wins over a lower write
  to the same field. I bit: the lower word is loaded into I after the upper op has run. E bit:
  the program stops after the next pair. Branch delay slots are modelled. M/D/T bits are
  ignored. A program is capped at 4 M pairs.
* **Upper:** ADD/SUB/MUL/MADD/MSUB/MAX/MINI in vector, bc, q and i forms, the ACC forms,
  OPMULA/OPMSUB, ITOF0/4/12/15, FTOI0/4/12/15 (saturating), ABS, CLIP, NOP.
* **Lower:** LQ/SQ/LQI/SQI/LQD/SQD, ILW/ISW/ILWR/ISWR, IADD/ISUB/IADDI/IADDIU/ISUBIU/IAND/IOR,
  B/BAL/JR/JALR/IBEQ/IBNE/IBLTZ/IBGTZ/IBLEZ/IBGEZ, MOVE/MR32, MFIR/MTIR,
  DIV/SQRT/RSQRT (Q, 7/7/13 cycles, I/D status bits), WAITQ, the EFU ops ESADD ERSADD ELENG
  ERLENG EATAN(xy/xz) ESUM ESQRT ERSQRT ERCPR ESIN EATAN EEXP (P, with latencies), MFP, WAITP,
  RINIT/RGET/RNEXT/RXOR, FSAND/FSEQ/FSOR/FSSET, FMAND/FMEQ/FMOR, FCAND/FCEQ/FCOR/FCGET/FCSET,
  XGKICK, XTOP/XITOP.
* **Timing:** one pair per cycle, with no data-hazard stalls (results are visible to the next
  pair). MAC/status/clip flags reach the lower unit `XSG_VU_FLAGLAT` pairs late (default 4,
  0 = immediate). Q and P land at their latency, or at WAITQ/WAITP. Everything in flight lands
  at program end.
* **Floats:** inputs with exponent 255 read as ±FLT_MAX and denormals as ±0. Results clamp to
  ±FLT_MAX and set O; NaN becomes +MAX. MXCSR is round-toward-zero with FTZ/DAZ while a program
  runs. vu1.cpp is built with `-mfpmath=sse`.
* **VIF1** (gs_bridge.cpp):
  * MSCAL/MSCALF run at `imm*8`, and MSCNT runs at the last stop address. TOP = TOPS and
    ITOP = ITOPS are handed over, then DBF flips (TOPS = BASE or BASE+OFFSET). OFFSET resets
    DBF.
  * UNPACK FLG uses the real TOPS. ITOP now sets ITOPS (Stage 5 wrongly put it into TOPS).
  * STROW/STCOL/STMASK are stored, and UNPACK applies the mask (data/row/col/protect) and
    MODE 1/2.
  * The FLUSH family is a no-op, because everything is synchronous.
* **XGKICK** walks GIF tags in VU1 data memory (wrapping) to EOP and feeds `Gs::Qword(1,…)`.

### Microcode check

Five MPG uploads per frame (0x0000, 0x0800, 0x1000, 0x1800 × 256 pairs, plus 0x2000 × 139)
fill 0x0000–0x2458. The dump (`XSG_VU_DUMP=400` writes `vu1_micro.bin`/`vu1_data.bin` to the
shot dir) is **byte-identical** to the MPG payloads parsed out of the `.vudata` blob
(`build/main/assets/main/blobs/vudata_rest.databin.bin`: a DMA tag of 0x249 qw, then
NOP + MPG ×5).

A static scan of the 1163 pairs shows the lower ops the code uses: MOVE 391, LQ 112, IADDIU 84,
ILW 62, ISW 60, IADD 53, SQ 49, LQI 43, JALR 35, IADDI 30, IBNE 26, B 25, SQI 22, IOR 21,
XGKICK 20, LOI 20, IAND 15, IBEQ 13, FCAND 12, DIV 12, MFIR 11, MTIR 10, JR 8, ERLENG 6, MFP 6,
**FSAND 5**, WAITQ 5, XTOP 3, WAITP 3, MR32 1. E bits are at 0x0, 0x28, 0x160, 0x408 and 0x5d8.
The programs do read the status flag (FSAND) and the clip flag (FCAND after CLIP). There are
no MAC-flag reads (FM*), so MAC latency cannot matter. Status latency might.

### What runs on the title (XSG_VU_LOG=50)

```
[vu1] frame 400: runs 450 pairs 1575 kicks 225 (tags 225, qw 1250, prims 0 ...) flagreads 0 clipreads 0 bad 0
```

This is per 50 frames: 9 runs, 31 pairs and 4.5 kicks per frame. The programs at 0x0 (NOP/E)
and 0x10 (`IADDIU vi14, 0x3d5; B; XGKICK vi14`) kick a packet that the EE UNPACKed at
qword 0x3d5. They are PACKED A+D tags (no PRE, so the counter shows 0 PRE prims), which set GS
state and draw one 512×448 untextured sprite with alpha 1 (`prim 46`, seen in `XSG_GS_TRACE`).
No undecoded instructions (`bad 0`).

The geometry programs (0x28.., 0x160.., 0x408.., 0x5d8..) have **not yet executed**: the nml
model renderer only starts after New Game. So the interpreter's FMAC/flag/EFU paths are
exercised only by the code above. They have not been validated against real geometry output.

## 3. After New Game

Fixed, in order (all under `PORT_HOST`; `ninja gate` passes):

1. **frame 306** `xglFontFlushCore` op 0x17 calls a function pointer that
   `xglFontPrintExtFunc` stored as 24 bits (`EW_sendPacket` → 0x09cd15). Fix: restore the host
   text segment's top byte.
2. **frame 366** `nmlModelEntry`: `layout->clipMasks` was NULL. `nmlPacketSetAttributeData16N`/
   `64N`/`64` are lq/sq asm copy loops (no-ops on host), so the layout was never copied into
   the packet. Fix: memcpy. The `sq $0` clip-mask zeroing loops also get memsets.
3. **frame 366** `srsAtan2` read `atanTbl_0 = 0x2040FAC0`, an uncached-view pointer. Fix: EE RAM
   is now a memfd mapped at 0x00010000, 0x20010000 and 0x30010000 (host_main.c). Also:
   `MMathCalcAngle/Cam` got C versions of their VU0 blocks, and `srsAtan2` returns 0 for NaN.
4. **frame 370** `xglLightIntensityAmbient` faulted on `movdqa` with an unaligned stack
   `Vector4` (called from the Java VM's `Java_xeno_Light_setColor`). Fix: game code is built
   with `-mno-sse -mno-sse2` (struct copies no longer use aligned SSE moves).
5. **frame 372**: **open**. `JTHREAD_waitFor` (case 3/4) dereferences `wait_target = 0x16f…`,
   called from `JTHREAD_defaultChr` → `JNI_callMethod`. The wait target is set by the event
   script VM. It is probably a value that VU0/asm-stubbed code or a missing opening-scene file
   (`EV1000.A`, `SCE010OP.FPK`, `1001_1.CAP` are not found) left wrong.

All frames from 320 to 370 are black: the fade-out, missing movies and an empty scene
(`stage6_after_new_game_340.png`).

## Screenshots (port/docs/screenshots/, git-ignored)

| file | what it shows |
|---|---|
| `stage6_title_fbp_e0.png` | vblank 400, FBP 0xE0: menu bars + stars (was black in Stage 5) |
| `stage6_title_fbp_70.png` | vblank 401, FBP 0x70: same, the other buffer |
| `stage6_after_new_game_340.png` | vblank 340 after START: all black |

The title is otherwise unchanged from Stage 5: the background is black (JPEG, needs the IPU).
VU1's only visible contribution is the full-screen alpha-1 sprite, which makes no visible
difference.

## Performance

Title, `XSG_FAST=1`, 10 s: about 80 vblanks/s with VU1 on, about 90 with `XSG_VU1=0`. The
difference is within run-to-run noise: VU1 runs ~32 pairs per frame. Frame time (~11–12 ms)
is dominated by the software GS taking the 512×448 background upload (~21 M DIRECT qw per
500 frames) and by PNG-less GS work, not by VU1. The interpreter has not been timed on real
geometry programs.

## Environment (new)

| variable | meaning |
|---|---|
| `XSG_DMA_THREAD=1` | run ch1/ch2 on the DMAC thread again (Stage 5 behaviour) |
| `XSG_VU1=0` | skip VU1 programs (MSCAL is then counted only) |
| `XSG_VU_LOG=N` | every N vblanks: VU1 runs, pairs, kicks, PATH1 tags/qw/PRE prims by type, flag reads |
| `XSG_VU_HIST=1` | instruction histogram (with `XSG_VU_LOG`, every 100·N vblanks) |
| `XSG_VU_TRACE=N` | trace each pair of the first N programs |
| `XSG_VU_FLAGLAT=N` | flag latency in pairs (default 4) |
| `XSG_VU_DUMP=F` | at vblank F write `vu1_micro.bin`, `vu1_data.bin` to the shot dir |
| `XSG_MPG_TRACE=1` | log MPG uploads (address, size) |

## Next blockers

1. **`JTHREAD_waitFor` fault at ~372** (above): find who sets `wait_kind`/`wait_target`
   (jthread.c, the event VM natives).
2. **Inline asm / VU0**: about 1000 `PS2_ASM*` sites are no-ops on the host. The renderer path
   alone has nml_model_set.c 72, xgl_2.c 64, m_math.c 59, face_point.c 28, nml_packet_add.c 15
   and nml_filter_set.c 13. Matrices passed to VU1 (camera/world/clip in nml_model_set.c
   `nmlModelCalc*`, xgl_2.c `xglMatrix*`, m_math.c `MMath*`, the `vcallms 0xE8/0x20` sin/cos
   in `MMathAddRotateVectorY`/`MMathCalcOffset`) will be garbage, as will lq/sq copies like the
   ones fixed here. A systematic translation (or a small VU0 macro-op interpreter driven per
   site) is needed before 3D output can be judged.
3. **IPU** (title background, movies).
4. VU1: data-hazard stalls are not modelled, so status-flag latency is approximate once the
   FSAND programs run. Validate on the first real nml packet: GIF tags, PRIM types, vertex
   counts (`XSG_VU_LOG`).

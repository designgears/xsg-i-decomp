# PC port, Stage 9: hardware GS in a separate 64-bit process (xsg-gpu, paraLLEl-GS)

Branch `pc-port` (agent worktree), 2026-11-07. Builds on [STAGE5_GS.md](STAGE5_GS.md),
[STAGE6_VU1.md](STAGE6_VU1.md) and [STAGE8_GFX.md](STAGE8_GFX.md).

**Result:** `XSG_GS=gpu` renders on the GPU through paraLLEl-GS in a separate 64-bit
process, `xsg-gpu`, fed over a shared-memory ring. The output matches the software GS
exactly on the Namco logo, the logo movies and the title. In the 3D cutscene it is within
about 1% of pixels. Screenshots and the live stream work in gpu mode. The software GS does
not run at all in that mode. In the 3D cutscene the game runs about 4x faster (3.5 → 14–17
game frames/s, both with `XSG_FAST=1`). The game's own CPU work, not the GS, is now the limit.

## 1. Architecture

```
xsg (i386, MIT)                                     xsg-gpu (x86-64)
  DMA ch1/ch2, sceGs*, XGKICK                         ring consumer (main thread)
  gs_bridge.cpp: GsQword / GifSend                     GPU_MSG_GIF   -> GSInterface::gif_transfer
     |  XSG_GS=soft|both: software GS (unchanged)      GPU_MSG_PRIV  -> PrivRegisterState
     |  XSG_GS=gpu|both:  gpu_client.cpp               GPU_MSG_VSYNC -> flush + vsync + present
     v                                                 GPU_MSG_READ_VRAM / READ_FIFO -> reply area
  memfd (68 MB) ===== fd 3, MAP_SHARED ========>     paraLLEl-GS (LGPL) on Granite/Vulkan
     header | 64 MB command ring | 4 MB reply          SDL3 window + Vulkan swapchain, or headless
```

* **Spawning.** On the first GS call, `ModeInit()` reads `XSG_GS`. For gpu/both,
  `xsggpu::Start()` creates a memfd, maps it, and `posix_spawn`s
  `<dir of xsg>/gpu/xsg-gpu` (`XSG_GPU_BIN` overrides this) with the memfd dup'ed to fd 3.
  It then waits for `ready` (the Vulkan device is up). If the renderer cannot start, the
  game stops with a message and does not fall back silently.
* **Where traffic is tapped.** All GS input already passed through three places in
  gs_bridge.cpp: `GifSend` (PATH3 DMA and the `sceGs*` packets), VIF1 DIRECT/DIRECTHL
  (PATH2) and `Path1Kick` (XGKICK, PATH1). These now go through `GsQword`/`GifSend`, which
  feed the software GS and/or the ring. Everything runs on the game's thread under the
  bridge's lock, at the DMA kick (Stage 6 made DMA synchronous). The ring order is
  therefore exactly the order the software GS sees, and the data is copied out of game
  memory at kick time.
* **Privileged registers** are plain memory at 0x12000000 (the game writes DISPFB etc.
  directly). Before every vsync and every readback, `SyncPriv()` compares PMODE…BGCOLOR
  (0x00–0xE0) and CSR/IMR/BUSDIR/SIGLBLID (0x1000–0x1080) with what was last sent, and
  sends each change as `GPU_MSG_PRIV`. The renderer writes them into paraLLEl-GS's
  `PrivRegisterState`, which is what `vsync()` scans out from.
* **Vsync.** Each `xsg_gs_vblank` sends `GPU_MSG_VSYNC(frame, field = frame & 1)`. The
  renderer calls `flush()`, then `vsync()` (force_progressive, anti_blur,
  adapt_to_internal_horizontal_resolution, with the result in TRANSFER_SRC layout), and
  blits the scanout into the swapchain, letterboxed to 4:3.
* **GS→host.** `sceGsExecStoreImage` (previously a stub) now goes through
  `xsg_gs_store_image` (port/runtime/sdk.c → gs_bridge.cpp). It sends the BITBLTBUF/
  TRXPOS/TRXREG/TRXDIR=1 packet, then a synchronous `GPU_MSG_READ_FIFO(seq, qwords)`. The
  renderer answers with `read_transfer_fifo()` in the reply area. The software GS has no
  local→host path, so the bridge reads the rectangle out of its memory itself and packs it
  as 32/24/16/8/4 bpp. In `both` mode the two results are compared, and the soft one is
  used.

## 2. IPC protocol (port/runtime/gpu_ipc.h)

* Only 32-bit fields and 32-bit atomics are shared, so the layout is identical in both
  ABIs. Header: magic/version, then `wpos`/`wpos_waiting` (producer), `rpos`/
  `rpos_waiting` (consumer) and `reply_seq`/`ready`/`vsyncs_done`, each on its own
  cache line.
* **Ring:** 64 MB. Counters are free-running byte counts (mod 2^32), and messages are
  16-byte aligned: `{type, len, a, b}` + payload. A message that does not fit before the
  end is preceded by `PAD`. Only the producer moves `wpos`; it publishes with a seq-cst
  store after writing the payload. The consumer publishes `rpos` after it has handled the
  message.
* **Signalling:** shared futexes on the counters. The waiter sets its `*_waiting` flag,
  re-checks, then sleeps. The other side calls `FUTEX_WAKE` only when the flag is set, so
  the steady state makes no system calls. Waits time out every 50–100 ms to check that the
  peer is alive (`waitpid WNOHANG` in the game, `getppid` + `PR_SET_PDEATHSIG` in
  xsg-gpu).
* **Back-pressure:** when the ring is full, the producer blocks in `Reserve()`. Nothing is
  ever dropped. The `[gpu]` stats line counts full-ring stalls. Windowed runs with
  `XSG_FAST=1` reach them during the uncapped movie phases and recover.
* **GIF batching:** qwords are appended in place into an open `GPU_MSG_GIF` (64 KB
  reserved). The message is closed and published when the path changes, when it is full,
  or before any other message. **Cut points matter:** paraLLEl-GS's PACKED fast path
  sizes its batch from the whole call (`size / nreg`, gs_interface.cpp:4346), not from
  what is left of it. A `gif_transfer` that ends inside a PACKED packet therefore
  overruns into the bytes after it. With naive 64 KB cuts, this lost exactly one 16×16
  movie macroblock per frame. The client now tracks each path's GIF tag state. It cuts
  only on a tag boundary or inside IMAGE data, and a PACKED/REGLIST packet may run up to
  1 MB past the chunk.
* **Synchronous requests:** `READ_VRAM(seq)` (4 MB `map_vram_read` copy) and
  `READ_FIFO(seq, n)`. The renderer writes the reply area, stores `reply_seq = seq`, and
  wakes the game.

## 3. Output paths in gpu mode

* **Window** (SDL3 + Granite WSI with our own `WSIPlatform`). The swapchain gets
  `TRANSFER_DST|TRANSFER_SRC` extra usage, because Granite creates COLOR_ATTACHMENT only
  by default. This is ico-recomp's WSI usage-flag fix. The present mode is mailbox
  (`XSG_GPU_VSYNC=1` selects FIFO), so presenting never paces the game. Closing the window
  ends xsg-gpu, and the game exits with it.
* **Screenshots** (`XSG_SHOT*`, `XSG_SHOT_FBP`, `XSG_SHOT_VRAM`): the same code as for
  soft, through a `PixelSource`. For gpu this is a synchronous VRAM readback, decoded with
  the software GS's own swizzle (`ps2host::gs::Memory::Read`). In `both` mode the files
  are `frameNNNNNN.png` (soft) and `frameNNNNNN_gpu.png`.
* **Live stream** (`XSG_STREAM`, 512×448 RGB24): in gpu mode the game passes the FIFO to
  xsg-gpu as `XSG_GPU_STREAM`. xsg-gpu then decodes the displayed buffer from
  `map_vram_read` at each vsync (when its writer thread is free) and writes it, so the game
  thread never waits for it. In `both` mode the soft GS feeds the stream as before.
* **Super-sampling** (off by default): `XSG_GPU_SSAA=2|4|8|16`, `XSG_GPU_SSAA_TEX=1`
  (super-sampled textures), and `XSG_GPU_HIRES=1` (high-resolution scanout, SSAA ≥ 4).
  These affect only the window. Screenshots and the stream read single-sampled VRAM.

## 4. Build and run

```
cmake -S port -B port/build -G Ninja      # XSG_GPU=ON by default; fetches paraLLEl-GS
cmake --build port/build -j               # -> port/build/xsg, port/build/gpu/xsg-gpu
#   -DXSG_GPU=OFF                 no renderer process (soft only)
#   -DXSG_PGS_SOURCE_DIR=<dir>    use a local paraLLEl-GS checkout (with Granite submodules)
```

`port/gpu` is its own CMake project, built through `ExternalProject` because it is
x86-64 and the rest is `-m32`. It needs the Vulkan headers/loader and SDL3 (pkg-config
`sdl3`; without SDL3 it builds headless-only). Release build type.

| variable | meaning |
|---|---|
| `XSG_GS=soft\|gpu\|both` | renderer (default soft) |
| `XSG_GS_COMPARE=N` | both: compare the displayed frame every N vblanks (default 60; 0 = only at shots) |
| `XSG_GPU_HEADLESS=1` | no window |
| `XSG_GPU_BIN` | path of xsg-gpu |
| `XSG_GPU_SCALE=N` | initial window size N×(512×448), default 2 |
| `XSG_GPU_VSYNC=1` | FIFO present instead of mailbox |
| `XSG_GPU_SSAA`, `XSG_GPU_SSAA_TEX`, `XSG_GPU_HIRES` | see above |
| `XSG_GPU_VVL=1` | allow the Vulkan validation layer |
| `XSG_FPS=N` | every N vblanks: wall-clock vblanks/s and game frames/s (DISPFB1 flips) |

## 5. Licence boundary

paraLLEl-GS is LGPL-3.0-or-later. It is fetched at configure time (pinned commit
`cc6184af7e0c03da603045ca371ffa5dae9b0655`) into the build tree, never committed, and
linked only into `xsg-gpu`. The game binary has no LGPL code. Its only interface to the
renderer is our MIT `gpu_ipc.h` protocol over shared memory, the same kind of process
boundary used for FFmpeg in Stage 8. See [../third_party/THIRD_PARTY.md](../third_party/THIRD_PARTY.md).

## 6. Results (RTX 5090, driver 615.78)

### Correctness (`XSG_GS=both`, comparison of the displayed 512×448 buffer)

| scene | vblanks | pixels off by > 8 | notes |
|---|---|---|---|
| Namco logo, logo_msi movie, black frames | 60–600 | 0.00% (identical) | before the cut-point fix, one 16×16 macroblock at (80,208) was black on gpu |
| Title (JPEG background, menu, stars) | 780–2880 | 0.00%, max error 8 | mean abs 0.004: blending rounding on a few pixels |
| Cutscene / Virtual Tutorial (3D) | 1200–1650 | 0.0–1.8%, mean abs 0.3–2.0 | polygon edges, alpha-blended text edges and a few textured areas differ; the images are visually indistinguishable |

(The cutscene run used `XSG_PAD=300:0x0800,1100:0x0800,1200:0x0040 XSG_SKIP_EVENTS=1
XSG_MPEG=0 XSG_FAST=1`. With skipped events, 1400 is the "Morning, KOS-MOS" scene and 1550
is the Virtual Tutorial.)

I did not investigate which renderer is closer to a real PS2 in the 3D differences.
paraLLEl-GS aims for bit accuracy, so the software GS's rasterisation rules are the more
likely suspect, but this has not been checked against hardware.

Screenshots (port/docs/screenshots/, git-ignored): `stage9_{soft,gpu}_140.png` (Namco),
`stage9_{soft,gpu}_1000.png` (title), `stage9_{soft,gpu}_1400.png` (KOS-MOS),
`stage9_{soft,gpu}_1550.png` (Virtual Tutorial), `stage9_gpu_ssaa4_1550.png` (the run with
SSAA 4x + hires; the shot itself is single-sampled VRAM), and `stage9_gpu_stream_namco.png`
(the last frame read from the gpu-mode `XSG_STREAM` FIFO).

### Speed (cutscene, vblanks 1300–1700, `XSG_FAST=1`, headless, default debug build)

| renderer | vblanks/s | game frames/s |
|---|---|---|
| soft | 7.1–7.3 | 3.5–3.6 |
| gpu | 23–34 | 11.5–17 |

In gpu mode xsg-gpu keeps up: the ring holds less than 60 KB at every check, and the
renderer alone runs at 200–1700 vsyncs/s on the same stream. The remaining time is the
game thread itself: game C code built without optimisation (the default CMake build type;
Release still crashes, see STAGE8_MOVIES), plus the VU1 interpreter. Neither is a GS cost.

## 7. Open points

* The ~1% 3D differences between soft and gpu (see above).
* Interlace: the renderer is given `field = vblank & 1` and force_progressive. SMODE1/
  SYNC registers are not set by the game's stubs (`sceGsResetGraph`), so paraLLEl-GS
  infers the mode from DISPLAY.
* CSR side effects (FINISH/SIGNAL/HSINT) are not modelled on either path. CSR is only
  forwarded.
* The stream does a full `map_vram_read` per vsync in xsg-gpu, which serialises that
  frame's GPU work. It is cheap here, but an async copy of the scanout image would avoid it.
* paraLLEl-GS's `size / nreg` overrun is worked around on our side. It should be reported
  upstream.

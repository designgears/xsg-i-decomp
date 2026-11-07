# PC port, Stage 5: software GS and screenshots

Branch `pc-port`, 2026-11-07. Builds on [STAGE4_BOOT.md](STAGE4_BOOT.md).

**Result:** the game's GIF and VIF1 DMA traffic now reaches a software GS, and the displayed
frame buffer can be written to PNG at any vblank. The **title screen menu** ("New Game" /
"Continue" over the star particles) renders. Logos are black (they are IPU movies), the title's
background is black (it is a JPEG that needs the IPU), and nothing drawn by VU1 microcode appears
(no VU1 interpreter yet).

## Design

```
game: xglDmaDirectSrcChain / xglDmaDirectNormal (CHCR writes at 0x10009000 / 0x1000A000)
      sceGsPutDrawEnv, sceGsExecLoadImage, sceGsPutDispEnv, *GS_DISPFB2 = ...
        |                                    |
DMAC stand-in thread (host_main.c)     sdk.c (game thread)
        |  CHCR.STR set on ch1/ch2           |  xsg_dma_poll() first, keeps order
        v                                    v
gs_bridge.cpp  (one recursive mutex)
  source-chain walker: refe cnt next ref refs call ret end, TTE upper words to VIF1,
                       normal mode, bit 31 = scratchpad, 0x2/0x3xxxxxxx uncached views
  ch2 -> GS PATH3
  ch1 -> VIF1 decoder: DIRECT/DIRECTHL -> GS PATH2; MPG -> VU1 micro (0x11008000);
         UNPACK (V/S x 32/16/8, V4-5, USN, MODE offset, CL/WL skipping) -> VU1 data
         (0x1100C000); STCYCL OFFSET BASE ITOP STMOD STMASK STROW STCOL FLUSH* MSKPATH3 MARK
         NOP accepted; MSCAL/MSCALF/MSCNT counted only
  software GS: port/third_party/ps2-port-engine-gs (MIT, vendored, unchanged)
  xsg_gs_vblank(frame) from xsg_vblank(): screenshots
```

* **GS**: the ps2-port-engine software GS (`Gs::Qword(path, lo, hi)`), unchanged. It already
  models swizzled local memory for every PSM, sprites/triangles/strips/fans/lines/points, Gouraud,
  TEX0/TEX1/CLAMP with CLUT (CSM1/2), mipmaps, ALPHA, alpha/Z/destination tests, scissor,
  PRMODE, FBA, fog, host→local and local→local transfers. Its dependencies on upstream's profiler,
  DMA and frame blending are three small shim headers. C++17 (CMake now enables CXX; the link
  adds `-lstdc++ -pthread`; `link_xsg.py --libs` is now repeatable).
* **libvifpk** (sdk.c): the stubs did not close DMA tags or VIF codes, so chains were garbage.
  `Cnt/End/Ref/Terminate` close the open tag (QWC), `Close{Direct,DirectHL,Upk}Code` write
  sizes, `CloseGifTag` writes NLOOP, `RefLoadImage` emits CNT(DIRECT 6: BITBLTBUF/TRXPOS/TRXREG/
  TRXDIR + IMAGE tag) + REF(image).
* **Privileged registers**: plain memory at 0x12000000. `sceGsPutDispEnv` writes PMODE, SMODE2,
  DISPFB1, DISPLAY1, BGCOLOR (the game flips by changing `dispfb1.fbp`, and writes DISPFB2/
  DISPLAY2 itself); `sceGsSetDefDispEnv` fills SDK-like defaults.
* **Screenshots**: DISPFB1 (circuit 1 if PMODE.EN1, else 2; the game leaves both EN bits clear,
  so circuit 1), size from DISPLAY (`(DW+1)/(MAGH+1)` × `DH+1`, 512×448 here), pixels through
  `Gs::FramePixel` (PSMCT32/24/16/16S), stored-deflate PNG written by hand (no zlib).

## Environment

| variable | meaning |
|---|---|
| `XSG_SHOT=f1,f2,...` | write `frameNNNNNN.png` of the displayed buffer at these vblanks |
| `XSG_SHOT_EVERY=N` | ... and at every N-th vblank |
| `XSG_SHOT_DIR` | output directory (default `port/build/shots/`) |
| `XSG_SHOT_VRAM=1` | also `vramNNNNNN.png`: all 4 MB as a 1024×1024 PSMCT32 buffer (FBW 16) |
| `XSG_SHOT_FBP=70,e0[:fbw[:psm]]` | also `fbXXX_NNNNNN.png`, 512×448, for these FBPs (hex, FRAME units) |
| `XSG_GS_TRACE=N` | the GS's primitive/transfer trace for vblank N (stderr) |
| `XSG_DMA_TRACE=1` | log every DMA tag walked |

Example: `XSG_MC_DIR=/tmp/xsgmc XSG_FAST=1 XSG_SHOT=401 XSG_SHOT_VRAM=1 XSG_TIMEOUT=10 port/build/xsg`.

## What renders

Counts at vblank 500 of the title: 249 GIF kicks, 249 VIF1 kicks, 10.8k tags, 21 M DIRECT
quadwords (the title re-uploads its 512×448 background image every frame), 2241 MSCAL, 1245 MPG,
2988 UNPACK.

* **Title** (`port/docs/screenshots/title.png`, vblank 401): the 512×448 frame shows the
  "New Game" / "Continue" bars at the right place and the 256 star particles (textured sprites,
  alpha 1–15, blended). The bars are white with dark text exactly as the CPU-built
  `buf` image uploaded them; the black area is where the JPEG background should be.
* **The other buffer** (`title_other_buffer.png`, vblank 400): the game draws at 30 Hz into two
  buffers (FBP 0x70 / 0xE0); in the 0xE0 one the uploaded image is black, so every other vblank
  shows stars only. Not yet explained (see blockers).
* **VRAM** (`title_vram.png`): both frame buffers, the font/menu textures at TBP 0x3800–0x3C00
  (PSMT/CT32 with FBW 5, so noise in the FBW-16 view, which is expected).
* **Logos** (`logo_period.png`, vblank 120): black. Namco/MSI logos are IPU movies.
* **After New Game**: the game still crashes at vblank 306 in `xglFontFlush` (Stage 4 blocker
  1), so nothing past the title has been seen.

## Next blockers for correct graphics

1. **VU1**: everything 3D and most 2D UI (nml packets: UNPACK + MSCAL; XGKICK = PATH1) needs a
   VU1 interpreter. The MPG uploads and UNPACKed data already land in VU1 memory.
2. **IPU**: JPEG (`xglJpegDecode`, title background) and MPEG movies (logos, FMVs).
3. **Alternate title buffer black**: the upload into FBP 0xE0 comes out black while the same
   chain into 0x70 has the menu. Check whether `buf` is double-buffered by the CPU, and whether
   DMA run on the DMAC thread (asynchronous, 50 µs polling) races the game rewriting the source.
   Running DMA synchronously on the CHCR write (a hook in `xglDmaDirect*`) would remove the race.
4. **Display selection**: PMODE has EN1=EN2=0 (the game clears EN2 and our SetDefDispEnv's EN1
   is overwritten), so the shot just uses circuit 1. Interlace/field (SMODE2 INT=1, FFMD=0) is
   ignored: the full 448-line frame is dumped.
5. **VU0 macro ops** (Stage 3 blocker): matrices are garbage, which will matter once VU1 runs.
6. `sceGsExecStoreImage` (GS→host) is not modelled.

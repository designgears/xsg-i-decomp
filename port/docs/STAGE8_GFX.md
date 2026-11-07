# PC port, Stage 8: full frames again, and real 3D geometry

Branch `pc-port` (agent worktree), 2026-11-07. Builds on [STAGE7_VU0.md](STAGE7_VU0.md),
[STAGE7_IPU.md](STAGE7_IPU.md) and [STAGE6_VU1.md](STAGE6_VU1.md).

**Result:**
* The logo movies and the JPEG title background are complete again. Nothing is lost below
  y=192 or y=320.
* After New Game, the opening cutscene draws real, lit and textured geometry: KOS-MOS and the
  hangar behind her, with subtitles.
* The last untranslated asm site (`src/ov01/m_cam.c`) is hand-ported:
  `vu0_translate.py --report` gives 0 untranslated.

## 1. Black below y=192 / y=320

### Cause 1: a relocated integer table (`asVramBase`)

`xglRenderSetReso` copies one 0x18-byte row of `asVramBase` (main 0x004A8DF0) into
`sRender.vram.row`. Each row starts with the u16 pair `0x0000, 0x008C` (or `0x0000, 0x0070`,
…). splat printed that word as `.word D_008C0000` / `D_00700000`, because it looks like a RAM
address. That does no harm on the PS2, where the bytes are the same. On the host,
`gen_host_data.py` emitted `.long D_008C0000`, so the word became the **host address** of that
BSS symbol. `depth_base` is the low u16 of that pointer, so the host got ZBP 0x130 where the
PS2 has 0.

The depth buffer then started inside the 448-line frame buffers (FBP 0x70/0xE0 + 0x70 pages).
Each frame the render thread draws a ZB_ONLY sprite (`test 32001`) that clears depth. As a
PSMZ24 write it zeroes the RGB of the lower lines of the *displayed* buffer. The movie thread
redraws its picture only every second vblank, so the bottom of every other displayed frame was
black.

The VU0 commits did not cause this directly. They grew BSS, which moved `D_00700000`, and
the low half of its address became an in-range ZBP.

**Fix:** `gen_host_data.py` has a `NO_RELOC_LABELS` set (`asVramBase`). Inside those labels
every word is emitted as its raw bytes. The other `D_00XX0000` hits in data (TLB tables in
`set_tlb_handler`/`init_map_trap`, `end_print`) are not read by the host path.

### Cause 2: band-ordered drawing in the software GS (why the cut moved between 192 and 320)

`gs_draw.cpp` deals rows out to worker-thread bands. Each band draws every queued primitive
over its own rows. A depth write whose address aliases a *different row* of a frame buffer was
therefore ordered by band rather than by primitive, so the result changed from run to run.
`Draw()` now detects frame/depth ranges that overlap at a different base, within one job or
against the queued jobs. It flushes, then draws that job alone, in order (the same treatment as
texture-reads-frame).

The GS trace (`XSG_GS_TRACE`) now also prints SCISSOR and XYOFFSET.

Checks: 14 of 14 runs give a full namco frame at vblank 140 (before the fix, between 1 in 6 and 3 in 4
runs were cut, depending on the build). With `XSG_FAST=1`, vblanks 140/141/400/401/1000/1001 are all complete.

## 2. Collapsed 3D vertices: the `Vu0Call*` entry labels

The VU1 output was garbage. In `XSG_VU_DUMP` the XGKICK packets held XYZ = 0x80000000 and
ST = FLT_MAX with the ADC bit set. The VU1 constants at qw 0x3C2 (the view-screen matrix that
`nmlPacketAddMatrix` builds from `camera->viewMatrix`) had a zero w column.

I logged the camera code (`xglCameraViewVolume`). `camera->transform` and
`inverseTransform` were not orthonormal and contained FLT_MAX. The matrix stack produces
both: `xglMatrixStackTrans/RotX/RotY/RotZ/Frustum/Mul/Inverse/Save`. Each of these sets CMSAR0
to `(unsigned)Vu0CallMatrixStackX >> 3` and does `vcallmsr`. On the host the `Vu0Call*`
labels did not exist. `link_xsg.py` had made logging **function stubs** of them (they were 19
of the 20 autostubs), so every matrix-stack call ran the VU0 microcode from a host text
address modulo 4 KB.

**Fix:** `gen_vu0_microcode.py` now reads the DVP object's symbol table and emits each global
`Vu0Call*` label as an absolute symbol with its VU0 micro-memory byte address, as the PS2 link
has it (`Vu0CallSin` 0x20 … `Vu0CallLightCalcMatrix` 0x6A8). Autostubs dropped from 20 to 1.
The camera matrices are now sane: the transform is identity plus a translation, and the view
matrix has the frustum's 1/tan terms and w = −z.

`XSG_VU_LOG=100` from vblank 1300: ~4,170 tri-strips per frame, all drawn.

The run used `XSG_PAD=300:0x0800,1100:0x0800,1200:0x0040`, with real-time pacing and no
`XSG_FAST`. The movies now play in full, so the title is reached only at vblank ~1000, and the
old `300:0x0800` alone stays on the title.

## 3. `src/ov01/m_cam.c` (MCAM_VU0_TRIG)

The macro stringises `#addr` into its template. It now has a `#ifndef PORT_HOST` / `#else`
pair. The host form does `$8 = in`, `qmtc2 vf4`, `vu0_callms(addr)`, `qmfc2 vf1` → `out`
through the VU0 model. The PS2 token stream is unchanged, because only preprocessor lines were
added around the original `#define`.

## Screenshots (port/docs/screenshots/, git-ignored)

| file | what it shows |
|---|---|
| `stage8_namco_140_full.png` | vblank 140: the whole "Produced by NAMCO / namco" frame, no black lower part |
| `stage8_logo_msi_400_full.png` | vblank 400: logo_msi clouds, full height |
| `stage8_title_bg_1000_full.png` | vblank 1000: JPEG title background, logo, menu and the copyright line at the bottom (previously cut at 192 or 320) |
| `stage8_cutscene_kosmos_1400.png` | vblank 1400, opening cutscene: KOS-MOS (textured, lit) in front of the hangar geometry; subtitle "Morning, KOS-MOS. How do you feel?" |
| `stage8_cutscene_kosmos_1700.png` | vblank 1700: the same shot, the camera has moved; two subtitle lines overlap (a text-layout issue, not geometry) |

## Next

* ~~The overlapping subtitle lines at vblank 1700~~: an `XSG_SKIP_EVENTS` artefact, and the
  scene's darkness matches PCSX2. See [STAGE10_COMPARE.md](STAGE10_COMPARE.md).
* Other splat-symbolised integer words: a general check (for example, words whose symbol is
  never referenced as a pointer by code) would be safer than the label list.
* The scene is dark grey. Compare the lighting and fog with a PS2 capture.

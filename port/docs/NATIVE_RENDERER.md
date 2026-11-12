# Native renderer: design and live slice

> **Settings (2026-11-07):** the port no longer reads `XSG_*` environment variables. Every
> `XSG_FOO=v` in this document is now `[section] key = v` in settings.ini (the mapping is the
> "replaces" column of [MENU.md, settings.ini reference](MENU.md#settingsini-reference)); run
> with `XSG_CONFIG=<file> port/build/xsg`. Samples: [examples/](examples/).

Branch `pc-port` (agent worktree), 2026-11-07. Builds on [VU1_PROGRAMS.md](VU1_PROGRAMS.md)
(programs, I/O, C references, the motion-vector plan), [STAGE9_GPU.md](STAGE9_GPU.md) (xsg-gpu,
the IPC ring) and [STAGE10_PERF.md](STAGE10_PERF.md) §6 (the VU1 interpreter).

**Goal.** Draw the 3D scene with our own Vulkan pipeline, in the style of ico-pc, instead of
emulating VU1 + GS at 512×448. That gives real high resolution, room for widescreen, and
true per-vertex motion vectors for DLSS. paraLLEl-GS stays for everything that is not
(yet) native.

**Status (live slice, 2026-11-07):** `XSG_GPU_NATIVE=1` redraws every set-0 strip batch
(program 0x0bd8, loop 0x0c18: static scenery) natively and **shows it live in the window** at
`XSG_GPU_NATIVE_SCALE` (default 2), composited per pixel over the paraLLEl-GS frame on the GPU
(§3). Those batches no longer run on the VU1 interpreter (§1). The window seams at x≈256 / y≈339
are fixed (§2). Screenshots and the `XSG_STREAM` stream still read paraLLEl-GS VRAM at 512×448.
`XSG_GPU_NATIVE=0` (or unset) is byte-identical to the old path.

**Update (characters + motion vectors, 2026-11-07):** set-2 batches (skinned, rigid and two-bone
characters, all twelve vertex loops) also run the C reference instead of VU1 and are drawn
natively with GPU skinning (§7), so character edges are high-res in the composite. Both the
scenery and the character shaders write an RG16F motion-vector target (§8). Speed: §9 (with
everything on, 104 vblanks/s headless in the field, 77 with native off; 60.0 in a window).

## 1. Where to intercept

### Target design: the EE builder

The batch inputs exist on the EE before VU1 runs. `nmlPacketAddParts` uploads the constants:
M962 (object→clip), screen 966/967, the light block 973–980, and the bone palette 732+4k.
`nmlPacketAddBlockMaterial` uploads material 968–972 and 1008. `nmlPacketAddFog` uploads 998
and 1007. `nmlPacketAddGifTag` uploads 1011/1012. `add_exec_prog` → `nmlPacketAddExecProg`
picks the program (`s_aUcodeTbl`), and `nmlPacketAddTransData` `REF`s the prebuilt
geometry (UNPACK + MSCNT per batch, `material + 0x24`). A native draw is therefore
`(program, constants, the geometry REF)`, plus the identity key
`(model, parts, material, batch index)` that motion vectors need. Intercepting there
means:

* hooking these functions in the port, as wrappers around the decompiled functions. A
  `PORT_HOST` link-time wrap is enough; `src/` stays untouched;
* sending `GPU_MSG_NATIVE_DRAW {key, program, constant block}`, where the constant block is
  the VU1 memory ranges above, with the same layout as the references. The geometry goes
  once per model as a buffer: the REF chain's UNPACK payloads are already in vertex-stream
  form (VU1_PROGRAMS.md §2);
* skipping the VIF1 MSCAL/MSCNT for natively drawn programs, so VU1 does not run at all.
  That is where the CPU win is.

### What is implemented: the MSCNT boundary, with the C reference instead of VU1

The hook stays at the VIF1 MSCNT in `gs_bridge.cpp` (`RunVu1` → `NativeStrip`), not in
`nml_packet_add.c`: the EE builder only `REF`s prebuilt UNPACK chains, so building the native
message there means re-walking VIF code that the bridge already decodes, and `src/` would need a
`PORT_HOST` hook. At the MSCNT the UNPACKed inputs are in VU1 data memory, and that is enough to
skip the interpreter:

* a batch into loop 0x0c18 (vi15 + micro-memory signature; register batches excluded) runs
  `vu1_ref_run` (`port/render/vu1_ref/vu1_set0.c`, bit-exact with the interpreter per
  test_vu1_ref) **in place** on VU1 data memory and the register file (`Vu1Ref::memp`; the
  references are now linked into the game, compiled with the interpreter's float flags);
* then `GPU_MSG_NATIVE` (inputs after the header: morph applied, colours as floats);
* then the reference's GIF packet goes to the GS exactly as XGKICK would. paraLLEl-GS still
  receives the same bytes (see §3 for why).

Screenshots at vsync 1401/1550/2500/4000 are byte-identical between `XSG_GPU_NATIVE=0`, `=1`
and `=1 XSG_GPU_NATIVE_VU1=1` (the interpreter kept, for A/B). No file in `src/` changed.
Moving the message build up to the EE builder (and caching geometry per REF) is still the way to
drop the remaining per-batch copy; it no longer buys the VU1 time.

## 2. Reproducing the GS state of a batch

A VU1 batch carries only PRIM (from the GIF tag template at 1011). Everything else is GS
register state that the game sets through `nmlPacketAddGsFlush` (A+D at 0x3d5, `MSCAL
0x10`), register batches (`y = 0x4000`), PATH2 DIRECT and PATH3. xsg-gpu therefore runs a
**GIF state tracker** (`native.cpp`, `GifQword`) over every `GPU_MSG_GIF`, in ring order,
before handing the data to paraLLEl-GS. It handles PACKED (A+D, PRIM, TEX0/CLAMP
descriptors), REGLIST and IMAGE, per path, across message cuts. Each native batch snapshots
the registers of its context (PRIM.CTXT): TEX0, CLAMP, TEX1, XYOFFSET, SCISSOR, ALPHA, TEST,
FRAME, ZBUF, plus TEXA and FOGCOL. PRMODECONT/PRMODE are honoured.

| GS feature | native slice |
|---|---|
| geometry | `native.vert`: the strip loop's math (VU1_PROGRAMS.md §2) in IEEE floats. `gl_Position = (ndc·w, z·w, w)` with `w = clip.w`, so ST is perspective-correct. +0.5 pixel: the GS samples pixel corners, Vulkan pixel centres |
| ADC (guard band) | not reproduced. Vulkan clips properly, so triangles with a guard-band-rejected vertex are drawn where the GS drops them (rarely visible) |
| textures | decoded on the CPU into RGBA8 (alpha = the GS byte), all PSMs (CT32/24/16/16S, T8, T4, T8H, T4HL/HH), CLUT CSM1 CT32/CT16, TEXA. Cached per (TEX0, TEXA, upload generation) |
| texture source | **our own GS-memory mirror**: `ps2host::gs::Memory` (the software GS's swizzle), written by every host→local IMAGE transfer (BITBLTBUF/TRXPOS/TRXREG/TRXDIR). The textures are decoded when the batch arrives. The game re-uploads a few texture slots for every material (decoding from paraLLEl-GS VRAM at the end of the frame gave 5 garbage textures for 5,399 batches), so the decode must happen at batch time. paraLLEl-GS's VRAM is not usable mid-frame without a flush |
| texture function | TFX modulate/decal/highlight/highlight2, TCC; `(t·c)>>7` with floor |
| sampling | in the shader, the GS way: TEX1.MMAG nearest/bilinear (4-bit fraction), CLAMP WMS and WMT separately, incl. REGION_CLAMP/REGION_REPEAT. The old "WMS for both axes" sampler wrapped the floor texture in V and caused **both one-pixel seams** (x≈256, y≈339); they are gone |
| vertex snapping | X/Y truncated to 1/16 pixel like XYZF2 (ftoi4), so shared vertices of different batches coincide at high resolution |
| alpha test | ATE/ATST/AREF; AFAIL KEEP/ZB_ONLY → discard, FB_ONLY → pass (approximation) |
| fog | FGE: `(F·C + (255−F)·FOGCOL) >> 8`, F = floor(clamp(1007.z + 1007.w·q)) |
| blending | ABE with ALPHA (A,B,C,D) = (Cs,Cd,As/FIX,Cd) → SRC_ALPHA/1−SRC_ALPHA; (Cs,0,·,Cd) → additive; (Cd,Cs,·,Cd) → reverse-subtract. Ad (C = 1) and others are drawn opaque and counted. As/FIX scale 0x80 = 1.0 |
| depth | D32F, depth = Z / 2^24 (Z24) or 2^32/2^16. ZTE/ZTST (GEQUAL/GREATER/ALWAYS/NEVER), ZBUF.ZMSK |
| scissor, FBMSK | SCISSOR in target pixels; FBMSK = all-ones disables colour writes |
| dithering, COLCLAMP, FBA, PABE, DATE | not reproduced |

Still needed for full coverage: local→local copies and render-to-texture into the mirror
(today a texture the GS drew is decoded stale), CSM2 CLUTs, and Ad blending, which needs the
native target's own alpha.

## 3. Hybrid: what stays on paraLLEl-GS, and compositing

UI (`xgl_*` builders over PATH2/3), fonts, movies (IPU → PATH3 image
uploads) and full-screen effects (buffer copies and blurs through GS texture reads of the
frame buffer) all stay on paraLLEl-GS. The slice keeps sending **all** GS traffic to
paraLLEl-GS, including the VU1 output of native batches, so the GS frame and its Z buffer
are complete. The native layer is composited on top of that frame.

**Composite rule (slice, CPU, at shot vsyncs).** For each output sample at scale S, a native
sample replaces the GS pixel `(x/S, y/S)` when both of these hold:

1. it is not behind the GS depth there: `z_native + ε ≥ z_GS` (the GS Z buffer is decoded
   from VRAM with the ZBUF format);
2. where the native 1× render covers that pixel, the GS kept the colour the native pass
   predicts (max channel difference ≤ 24).

Rule 1 keeps characters and anything else in front of the scenery. Rule 2 keeps whatever
was drawn over the scenery later without depth: UI, the dialogue box, transparent effects
and post-processing. At silhouette edges, where the 1× render does not cover the pixel, rule 1
alone decides, so edges get the high-resolution native geometry.

**Chosen live design: the GS stays authoritative, native owns pixels it provably still holds.**

* paraLLEl-GS keeps receiving the full GIF stream, including the native batches' own output
  (from the C reference). Its frame and Z buffer are therefore exactly what they were, and every
  GS-side effect that reads them back (the hangar depth-of-field blur at ~1401, copies, fades)
  works unchanged. No resolve into GS VRAM is needed, because GS VRAM already holds the same
  content at 1×.
* At each vsync xsg-gpu takes the native batches of the **displayed** buffer (DISPFB.FBP; a
  frame's batches are grouped per FRAME.FBP and per vsync interval), draws them at 1× and at S×,
  decodes the GS colour + Z of that buffer from VRAM, and runs `composite.comp`: a native S×
  sample replaces the GS pixel under it when (1) it is not behind the GS Z there and (2) where
  the 1× native render covers the pixel, the GS colour is within `XSG_GPU_NATIVE_THR` (12) of it.
  Rule 1 keeps characters; rule 2 keeps everything drawn later without Z (UI, dialogue box,
  translucent minimap) and every post effect. The result is blitted to the swapchain, cropped to
  the DISPFB rectangle. A frame on screen for two vsyncs (30 fps) is composited once.
* This replaces "interleave native batches in ring order": ordering can never be wrong,
  because any later GS write that changes a pixel hands that pixel back to the GS.

Trade-offs:

* Post-processed frames fall back to 1×. In the hangar cutscene the whole frame is a GS-side
  DOF pass, so the composite takes nothing and the window shows the paraLLEl-GS frame (correct,
  but not high-res; the blurred background loses little). Doing the effect natively at S× is the
  only way around it (next steps).
* paraLLEl-GS still rasterises the scenery (cheap on the GPU) and the composite needs a VRAM
  readback + CPU decode of the displayed buffer per new frame (~1.8 MB).
* Rule 2 is a colour comparison, not an exact "last writer" bit. Something drawn over the
  scenery that leaves it within 12 levels would still show native scenery under it; at
  silhouette edges (1× not covered) only the depth rule applies.
* PMODE circuit blending (two-circuit fades) is not applied to the native image.
* ~~Characters (set 2) depth-test against the 1× GS Z, so their silhouettes stay 1×.~~ Characters
  are native now (§7), and their silhouettes are S× as well.

## 4. Running it

```ini
# XSG_CONFIG=native.ini port/build/xsg      (window)
[renderer]
gs = gpu
native = 1
native_scale = 4
[game]
movies = 0
[audio]
enabled = 0
[debug]
skip_events = 1
pad = 300:START,1100:START,1200:CROSS,1500:CIRCLE*400@60
# headless captures:
# headless = 1
# native_shot = 1401,1550,4001
# native_dir = /tmp/native
```

| settings.ini key (was) | meaning |
|---|---|
| `[renderer] native = 1` (`XSG_GPU_NATIVE`) | native set-0 path (game: skip VU1; xsg-gpu: live composite) |
| `[renderer] native_scale` (`XSG_GPU_NATIVE_SCALE`) | 1–8 (default 2) |
| `[debug] native_live = 0` (`XSG_GPU_NATIVE_LIVE`) | no live composite (shots only) |
| `[debug] native_thr` (`XSG_GPU_NATIVE_THR`) | rule-2 colour threshold (default 12) |
| `[debug] native_vu1 = 1` (`XSG_GPU_NATIVE_VU1`) | keep the VU1 interpreter for native set-0 batches (A/B); set 2 then stays on the interpreter too |
| `[debug] native_skin = 0` (`XSG_GPU_NATIVE_SKIN`) | set 2 (characters) stays on VU1 + paraLLEl-GS (A/B) |
| `[debug] native_set0_lit = 0` (`XSG_GPU_NATIVE_SET0_LIT`) | set-0 lit loops 0x1458 / 0x1fa0 stay on VU1 + paraLLEl-GS (A/B); default on: `NativeSet0Lit` → `GPU_NATIVE_SET0_LIT`, drawn by `native_skin.vert` group 4 (no bones, per-vertex 1007/vf9 fog) |
| `[debug] mv_view = 1` (`XSG_GPU_MV_VIEW`) | the window shows the motion vectors as colour (hue = direction, brightness = length, full at 16 px at 1×; `=N` sets that length) |
| `[debug] gpu_prof = 1` (`XSG_GPU_PROF`) | xsg-gpu per-stage CPU timers and GPU timestamps, a `[prof]` line every 600 vsyncs (§9) |
| `[debug] owner_thread = 0` (`XSG_GPU_OWNER_THREAD`) | run the ownership GS on the main thread (default: its own thread, §9) |
| `[debug] native_shot`, `native_dir` (`XSG_GPU_NATIVE_SHOT`, `_DIR`) | diagnostic images per listed vsync |

Per shot vsync: `native_VVVVVV_{gs_1x, native_only_1x, comp_1x, diff_1x, native_only_Sx,
comp_Sx, live_Sx, live_down_1x}.png`. `live_Sx` is a readback of what the window shows;
`live_down_1x` is it box-downsampled. The `[xsg-gpu]` stats line counts native presents.
With the live path there are also `_mv.{png,f32}` (the resolved motion vectors of the live
composite) and `_rawmv.{png,f32}` (the native render's own, before the GS-ownership resolve);
`.f32` is `u32 w, h`, then `w·h` RG float pairs in high-res pixels.

## 5. Validation (live slice, 2026-11-07, RTX 5090)

Screenshots: `port/docs/screenshots/live_native_*` (headless, scale 4) and
`live_window_4x*.png` (grim captures of the Wayland window, 1100×760, field at ~vsync 5000).

**1× composite vs paraLLEl-GS, whole frame (229,376 px)**

| vsync | composite takes | exact | ±1–2 | ±3–8 | >8 |
|---|---|---|---|---|---|
| 1401 hangar (DOF blur) | 0 | 229,376 | 0 | 0 | 0 |
| 1550 field + dialogue box | 176,931 | 63,013 | 165,425 | 760 | 178 |
| 4001 field + minimap | 199,428 | 44,152 | 184,318 | 728 | 178 |

The live 4× composite, box-downsampled to 1×: 1550 exact 62,197 / ±2 163,867 / ±8 3,038 / >8 274;
4001 exact 43,306 / ±2 182,650 / ±8 3,132 / >8 288 (the extra differences are 4× edges).
Where the GS depth is the native surface's, native-only vs GS at 1550: exact 10,568, ±2
165,425, ±8 760, >8 4,098 (was 1,065 at ±8 before the GS-style sampler). The remaining >8 pixels
are polygon edges. The longest >8 run in a row outside the minimap at 4001 is now 42 px (was a
249-px row at y=339; the x≈256 column is gone too).

**Speed** (headless, `XSG_FAST=1`, sequential runs, mean of the `XSG_FPS=100` samples):

| run | field (vblank 2500–6000) | hangar cutscene (1300–1500) |
|---|---|---|
| `XSG_GPU_NATIVE=0` | 77.7 vblanks/s | 57.9 |
| old slice (VU1 interpreter + capture), no live | 70.7 | 56.6 |
| VU1 skipped, no live | 87.3 (+12% vs off, +23% vs old slice) | 60.2 |
| VU1 skipped, live 4× | 85.2 | 60.2 |
| VU1 skipped, live 2× | 83.6 | 61.0 |

Windowed (mailbox, live 4×): 81–82 vsyncs/s in the field, 7,221 of 8,400 presents native.

## 5a. Validation of the first slice (CPU composite only)


Screenshots are in `port/docs/screenshots/native_*` (git-ignored).

**Field (tutorial room, vsync 1550 with the dialogue box, 6001 with the minimap):** 5,399
native batches (about 21.7k vertices), 512×448, Z24.

| pixels (1×) | 1550 | 6001 |
|---|---|---|
| covered by native geometry | 229,376 (all: floor and walls) | 229,376 |
| taken by the composite | 176,895 (77%; the rest: characters, dialogue box) | 199,434 (87%; characters, minimap) |
| GS depth = native surface: exact | 10,056 | 13,671 |
| … within ±2 per channel | 165,343 | 184,241 |
| … ±3 to 8 | 1,065 | 1,011 |
| … over 8 | 4,386 (2.4%: polygon edges, two one-pixel seams) | 16,494 (7.6%: the translucent minimap, which writes no Z, plus the same edges) |

The composite keeps the dialogue box and the translucent minimap, through rule 2.

The ±1–2 differences come from texture filtering and the modulate rounding. paraLLEl-GS's
bilinear runs at 8-bit precision, and the slice uses Vulkan's float filtering. At 4×
(2048×1792) the floor logo's edges are clean, while the texture itself is low resolution.
There are two one-pixel seams (a vertical one at x ≈ 256 and a horizontal one at y ≈ 340).
Native coverage leaves a gap there that paraLLEl-GS's rasteriser closes, most likely a
T-junction under the different fill rules.

**Cutscene (hangar, vsync 1401):** the native geometry is right; `native_001401_native_only_1x`
shows the hangar walls and the city behind them. The composite correctly takes **nothing**,
though, because the game post-processes this frame: a full-screen blur/darkening pass that
reads the frame buffer back as a texture and redraws it, and it also reuses the Z buffer.
Neither rule holds anywhere. This is the reason §3's live design resolves the native
target back into GS VRAM before such effects instead of compositing afterwards.

## 6. Next steps

0. Effects (§10): avoid the per-frame VRAM snapshot (read the GS frame on the GPU), decode
   GS-rendered textures from the final VRAM instead of falling back, motion vectors for replays
   (today replayed/PMODE pixels get zero motion).
1. ~~Characters~~ done (§7). ~~Motion vectors~~ done (§8). ~~Effects at S×~~ done (§10).
2. Effects at S×: detect GS draws that sample the displayed frame/Z (TEX0.TBP0 in the FRAME/ZBUF
   range) and redo the hangar DOF and fades on the native target, so post-processed frames stay
   high-res. Replace rule 2 with an exact last-writer plane once more of the frame is native.
3. DLSS: feed `xsgskin::MvImage()` (RG16F, high-res pixels, current − previous) plus the native
   depth. Add sub-pixel jitter to the S× projection (there is none today) and take it out of the
   vectors; the 1× pass must stay unjittered because rule 2 compares it with the GS.
4. Move the message build to the EE builder and cache geometry per REF as GPU buffers; drop the
   per-frame VRAM readback by reading paraLLEl-GS VRAM on the GPU.
5. Mirror completeness (local→local, render-to-texture, CSM2), Ad blending, dithering.
6. Widescreen: widen M962's projection and the GS scissor/guard band.
7. Speed of the live path (§9): ~~record the 1× and S× renders once~~ (the 1× render is gone with
   exact ownership). Left: per-draw recording cost (bindless textures and one constant buffer per
   frame would allow multi-draw), the GPU-side effect snapshot (needs access to paraLLEl-GS's VRAM
   buffer), and the GIF trackers (§9).

## 7. Characters (set 2), natively

**Game side** (`gs_bridge.cpp`, `NativeSkin`, next to `NativeStrip`). The path runs at every VIF1
MSCNT that resumes at 0x0188 while set 2 is loaded and `vi15` is one of the twelve loops
(0x05f0 … 0x3550). Set 2 is recognised by the CRC32 of micro memory 0..0x3a68 (0x9d68d149),
checked again after every MPG. `vu1_ref_run` (`vu1_set2*.c`, bit-exact) then runs in place on VU1
memory and the register file. Register batches go through the same reference. For vertex
batches the game then sends:

* `GPU_NATIVE_SET2_PALETTE`: qw 732–961 (230 qw, about 57 bones), only when it differs from the
  last palette sent. VU memory carries the palette (and M962/973) forward across the
  `s_pMatrixCache` skip, so the message does too.
* `GPU_NATIVE_SET2` (`GpuNativeSkin` + `GpuNativeSkinVertex[n]` + ADC bits). It holds the streams
  P, N, C, B and W as the header leaves them (morph and constant colour applied), and the
  constants the loop reads: vf17–31, 970, 972, 976, 1004–1006, 1008, 1009, 1017, tags
  1010/1011/1012, 995.y and the entry I. It also holds the fog word, the draw key, and **one ADC
  bit per vertex, taken from the reference's own XYZF2**. The facing test (995.y and the ring of
  skinned positions) and the guard-band rejection are therefore exactly the VU's.
* The reference's kicks, in order. For the six-kick env programs, `GPU_NATIVE_SET2_ENV` goes
  between kick 3 (0x3da, the env pass's A+D state) and kick 4 (the env strip). That way xsg-gpu
  snapshots that pass's GS state at the right point in the ring.

paraLLEl-GS gets the same bytes as before. Game screenshots at vsync 1550, 4000 and 6000 are
byte-identical between `XSG_GPU_NATIVE=0` and `=1`. At 1401 two of three run pairs matched. That
frame is timing-dependent (the GPU-side screenshot read), not a difference of the native path.

**xsg-gpu side** (`native_skin.{h,cpp}`, `native_skin.vert`; `native.frag` is shared). Each batch
becomes one `Stored` per pass in the displayed buffer's list, in ring order with the scenery:

* pass 0 (tag 1011);
* the specular strip (tag 1012, GS context 2) of the two-pass loops;
* the env strip (tag 1010) of the six-kick loops, with its state snapshotted at `SET2_ENV`.

When a batch arrives, its strip becomes an index list without the triangles whose last vertex
has ADC set. `native_skin.vert` repeats the loops of VU1_PROGRAMS.md §2 in IEEE floats:

| loops | skinning |
|---|---|
| 0x05f0 / 0920 / 0da0 / 1268 | up to 4 bones (B = qw addresses, W = weights), stop at the first zero address |
| 0x17d0 / 1ac8 / 1f00 / 2378 | rigid: one matrix (B.x) |
| 0x2890 | "blend 8": bones B.x/B.y, weights B.z/B.w (floats), always both |
| 0x2c28 / 3098 / 3550 | as 0x2890, bone 1 only when B.y ≠ 0 |

After skinning, the shader does the following:

* optional renormalisation (ILW 1008.z) and mirror (1017.z);
* `L = max(M973·n, 0)`, `rgb = (980 + M977·L·968)·C`, alpha `968.w·C.w`;
* specular power `max(M973·R, 0)^(2^(1008.x+1))`, where `R` is the reflected eye vector;
* the one-pass specular colour, `min(min(lit, I) + sp·C, 255)`, with I = the entry I for
  vertex 0;
* the two-pass second strip, `min(sp·C·(970·1008.y), 255)`, alpha `min(C.w, 128)`;
* the env programs: pass 1 adds `128·sp·(970·1008.y)`; pass 2 uses colour 1009 and
  ST = `((e.xy+1)/2)·vf24.yw + vf24.xz` with `e = M1004·(M973·R)`;
* fog is the constant F of 998.w.

XYZ snapping, depth, the texture function, alpha test, blending and fog are the scenery path's.

When a message arrives, everything its batch needs on the GPU is written **once** into a 256 MB
host-visible ring: the vertices with last frame's positions, the indices and the 0x2a0-byte
constant block. Palettes go to a ring of 16,384 slots that the shader indexes (`cfg2.w`, `cfg3.x`).
The 1× and S× renders then only bind offsets. The main cost before this change was filling
constant blocks field by field in Granite's write-combined UBO memory, which halved the field
speed. They are now built on the stack and copied in one go, the scenery `Ubo` too. The texture
cache is now keyed by a content hash of the GS pages a texture spans, re-hashed only when an
upload touched those pages. Characters re-upload the same slots for every material, and the old
key (the global upload generation) made every character batch decode its texture again.

**Validation, 1×, native-only vs paraLLEl-GS.** Character pixels are the pixels where the
native-only render changes when set 2 is drawn natively. They include pixels that the GS later
covers with the dialogue box and the minimap, which explains most of the > 8 at 1550 and 4000.

| vsync | character px | exact | ±1–2 | ±3–8 | > 8 | composite takes |
|---|---|---|---|---|---|---|
| 1350 hangar cutscene (close-up) | 84,398 | 17,149 | 64,091 | 3,019 | 139 | 60,205 |
| 1550 field + dialogue box | 14,315 | 935 | 9,678 | 840 | 2,862 | 11,971 |
| 4000 field + minimap | 14,153 | 795 | 7,882 | 857 | 4,619 | 10,094 |

Over the whole frame, the composite now takes 188,881 px at 1550 (176,931 with set 0 only),
209,519 at 4000 (199,428) and 168,775 at 1350 (113,837).

The 1× composite vs paraLLEl-GS at 1550 gives exact 51,998, ±2 175,068, ±8 1,584 and > 8 726.
With set 0 only it was 63,013 / 165,425 / 760 / 178. The character pixels it takes are within the
rule-2 threshold (12) but not exact, because of lighting in IEEE floats and the `(t·c)>>7`
rounding. 1401 still falls back to the GS frame (the DOF blur).

Run log: 84,311 set-2 batches by vsync 1200, 7,608 triangles culled by ADC or facing, and no bone
outside the palette with a non-zero weight.

Images are in `port/docs/screenshots/skin_*`. `live_4x` is the window at 4×.
`skin_001550_edges_gs1x_vs_live4x.png` shows a character's edges at 1× GS next to live 4×.

## 8. Motion vectors

* **Draw key.** The key is `(VIF address of the batch's MSCNT, occurrence in this frame)`. The
  MSCNT sits inside the model's prebuilt geometry chain, which `nmlPacketAddTransData` `REF`s, so
  its host address is stable per (material, batch). Instances that share a mesh are told apart by
  the occurrence count, which is stable as long as the game draws them in the same order. Unlike
  the EE-side key of VU1_PROGRAMS.md §5, this needs no `src/` hook.
* **History** (xsg-gpu, `native_skin.cpp`). Per key it keeps last frame's M962 and positions. For
  set 0 the positions are after morphing, so morph weights need no separate tracking. For set 2 it
  keeps P before skinning, the palette serial and the mirror plane. A key is new, and gets zero
  motion, when its history is older than 6 vblanks or its vertex count changed. Last frame's
  positions ride in the vertex stream: `GpuNativeVertex.pad` for set 0 (filled in xsg-gpu) and
  `GpuVertex.pp` for set 2.
* **Shaders.** `native.vert` and `native_skin.vert` compute the previous clip position with last
  frame's matrix (and palette). `native.frag` writes `(cur.xy/cur.w − prev.xy/prev.w) · 966.xy · S`
  to colour attachment 1 (RG16F): current − previous, in high-res pixels. Nothing is jittered, so
  this is the jitter-free projection. Blended draws and draws without colour writes leave the
  vector of what is under them.
* **Resolve** (`native_mv.comp`, run after `composite.comp`, which is unchanged). A sample keeps
  its native vector only where the composite took the native sample: the exact ownership plane
  (§10) says native owns the pixel, and the final colour is still the native sample's, so no post
  effect changed it (§9). With `XSG_GPU_NATIVE_OWNER=0` the old test stays: the same colour, and
  where that is also the GS colour, composite rule 1 decides. Everything the GS owns gets zero: UI, 2D,
  dialogue box, minimap and post-processed frames. At 1401 (the DOF fallback) the resolved field
  is all zero.
* `XSG_GPU_MV_VIEW=1` replaces the window image with the colour view.

**Validation.** Frame A is warped by frame B's vectors and compared with B (`B(p) ≈ A(p − mv(p))`),
on the native-only 4× images with the raw vectors:

| pair (vsync) | what moves | samples with \|mv\| ≥ 0.5 | mean \|mv\| | mean abs error, no warp → warped | warp better / worse |
|---|---|---|---|---|---|
| 1350 → 1351 hangar | Shion turns (camera static) | 1,026,580 | 10.3 px | 14.98 → 4.40 | 73% / 17% |
| 1352 → 1353 hangar | same | 1,010,003 | 11.3 px | 15.88 → 4.24 | 74% / 17% |
| 4002 → 4003 field | idle animation | 7,028 | 0.56 px | 6.30 → 4.47 | 64% / 19% |
| 1550 → 1551 field | nothing moves ≥ 0.5 px | 0 | – | 0.12 → 0.13 | – |

0.95% of the samples (1350) and 0.75–0.90% (field) changed between the frames but have no vector,
mostly animated textures and shading changes. The rest of "worse" is disocclusion at
silhouettes, where no single vector can be right.

Images: `skin_001351_rawmv.png` shows Shion turning; the static scenery and KOS-MOS are grey
(zero). `skin_004003_mv.png` is the resolved field: idle characters, and zero in the minimap area.

## 9. Speed with characters, effects and ownership on

Recipe: `XSG_GS=gpu XSG_GPU_NATIVE=1 XSG_GPU_NATIVE_SCALE=4` (characters, effect replay and the
exact ownership plane all on), the §4 pad script, `XSG_FPS=600`. Before this pass the xsg-gpu
process was the bottleneck: about 54–56 vblanks/s headless fast in the field against 77 with native
off, and with a window xsg-gpu crashed (below).

### Profile (`XSG_GPU_PROF=1`)

`native_prof.{h,cpp}`: per-stage CPU timers on the xsg-gpu main thread and Vulkan timestamps around
the live pass, printed as a `[prof]` line every 600 vsyncs (ms per vsync; one live composite per
two vsyncs, since the game runs at 30 fps). perf is not installed, and a SIGPROF sampler on the
xsg-gpu main thread got the process SIGKILLed inside the NVIDIA driver, so the stages are timed
instead. The timers cost about 10% of the speed. Field, headless fast, vblank ~4200–6000:

| stage (ms per vsync) | before | after |
|---|---|---|
| GIF trackers (native.cpp state + GS-memory mirror; native_fx.cpp parser) | 5.34 | 1.13 + 1.95 |
| paraLLEl-GS `gif_transfer` (main GS) | 0.76 | 0.76 |
| ownership GS `gif_transfer` (main thread) | 1.16 | 0.11 (hand-off to its thread) |
| GPU_MSG_NATIVE handling (textures, set-2 arena, history) | 3.25 | 2.29 |
| effect snapshot at the post-phase trigger | 1.14 | 0.52 |
| live: GS flush + VRAM wait | 0.02 | 0.03 |
| live: CPU decode of the displayed buffer (colour, Z, owner) | 1.35 | 0.51 |
| live: command recording (draws, composite, replay, MV) | 5.07 | 2.59 |
| outside the native code (ring wait, vsync, paraLLEl-GS flush) | 1.26 | 1.12 |
| **wall** | **19.4** | **11.0** |
| GPU per live frame: 1× render / S× render / composite+replay+MV | 1.29 / 0.94 / 0.40 | – / 0.84 / 0.42 |

The GPU is idle most of the time (≈1.3 ms per live frame on an RTX 5090); everything was CPU on the
xsg-gpu main thread.

### What changed

* **GS-memory mirror.** Host→local IMAGE data went into the mirror one pixel at a time through
  `Memory::Write`. Each qword is now unpacked and stored a row run at a time (`WriteRun`): ≈1.8 ms.
* **One render per live frame.** The 1× render only fed the colour rule of `composite.comp`, which
  the exact ownership plane replaces, so with ownership on the displayed list is recorded once, at
  S× (≈6,200 draws per field frame instead of 12,400): ≈2.5 ms. Multiview or downsampling was not
  needed; with `XSG_GPU_NATIVE_OWNER=0` the 1× render is still drawn for the colour rule.
* **Ownership GS on its own thread.** It already had its own Vulkan device; its GIF stream is now
  handed to a worker thread in pieces of up to 512 KB (consecutive messages of one path
  concatenated: one hand-off per message cost a futex wake each and was no faster). The main thread
  waits for it only before reading its VRAM, and its part of the effect snapshot runs on that
  thread too. Exact results are unchanged: it is the same second paraLLEl-GS, fed the same bytes.
  `XSG_GPU_OWNER_THREAD=0` keeps it inline.
* **Effect snapshot.** It copied both GSes' whole 4 MB VRAM into vectors that were reallocated and
  zero-filled every frame. It now copies only the pages of the frame buffer and the Z buffers
  (the trigger's and the native batches') into persistent buffers; what is left is the wait for
  paraLLEl-GS to finish the frame so far.
* **Decode of the displayed buffer** (colour, Z, owner plane): split over four threads.
* **Texture cache keys.** A texture's key hashed every page it spans with a serial FNV chain each
  time an upload touched them, once per (TEX0, TEXA); characters re-upload their slots for every
  material. Each page now keeps its own four-lane hash, recomputed only after an upload, and a
  texture's key folds the page hashes (batch-snap 1.20 → 0.43 ms). Set-2 messages no longer copy
  their vertices.
* **Motion vectors** (`native_mv.comp`): the exact owner plane instead of the colour comparison (§8).

Not done, and why: keeping the snapshot and the decode entirely on the GPU needs paraLLEl-GS's VRAM
buffer, which is private to its renderer (the main GS and our pipeline share a device, so a GPU copy
would work once it is exposed). Tagging native writes inside the main GS (FBA/alpha or a Z bit) is
not safe here: the game reads destination alpha (DATE, Ad blending, the afterimage strips' alpha-bit
marks), and Z24 has no spare bits the GS writes. Rasterising ownership ourselves would need every GS
primitive's coverage, which is what the second GS provides; since it now costs the main thread only
the hand-off, it stays.

### Speed

Headless, `XSG_FAST=1`, sequential runs, `[fps]` every 600 vblanks (RTX 5090, 16 threads):

| run | cutscene (vblank 1201–1801, hangar) | field (2401–13800) |
|---|---|---|
| native off (`XSG_GPU_NATIVE=0`) | 64.3 | 76.4 (73.6–76.9) |
| native on, before (pc-port 8b3736a) | 49.0 | 53.9 (52.1–55.8) |
| native on, after | 79.6 | 103.6 (101.0–104.0) |
| native on, after, with `XSG_GPU_PROF=1` | 72.9 | 88 (84.5–90.7) |

Native on is now faster than native off (the game thread skips VU1 for all native batches; the
no-composite run of the old §9 did 108). xsg-gpu's main thread spends ≈11 ms per vsync in the
field, so at 60 Hz it has ≈35% headroom.

Windowed (Wayland, mailbox, live 4×): real time holds 60.0 vblanks/s and 30.0 game frames/s
throughout (vblank 0–5400; every present after the cutscene is the native composite). With
`XSG_FAST=1` the window runs 104.7–107.7 vblanks/s in the field.

**Windowed crash (fixed).** With a window, `XSG_GPU_NATIVE=1` crashed xsg-gpu in
`WSI::begin_frame` (a null function pointer in `blocking_init_swapchain`), also on pc-port before
this pass. Granite loads instance-level entry points into volk's globals, and creating the
ownership GS's own instance (without surface extensions) left the window's surface and swapchain
functions null. `native_fx.cpp` reloads the main instance's entry points afterwards.

### Validation

Shots at 1350, 1351, 1352, 1401, 1550, 1551, 4001, 4002, 4003 (headless, scale 4) against pc-port
8b3736a run the same way: every image (`gs_1x`, `native_only_*`, `comp_*`, `diff_1x`, `owner_1x`,
`replay_1x`, `live_4x`, `live_down_1x`, raw MV) is byte-identical, and so are all the logged
statistics; only the resolved motion vectors differ (the new rule). The numbers:

| vsync | owner (native px) | replay at 1× vs paraLLEl-GS: exact / ≤2 / ≤8 / >8 | live 4× box-downsampled: exact / ≤2 / ≤8 / >8 | resolved MV samples (before → after) |
|---|---|---|---|---|
| 1350 hangar | 212,233 | – | 51,147 / 118,677 / 33,044 / 26,508 | 1,035,255 → 1,056,527 |
| 1401 hangar DOF | 229,376 (pre-post) | 201,524 / 27,606 / 246 / 0 | 118,701 / 98,613 / 7,709 / 4,353 | 1,373,677 → 1,374,477 |
| 1550 field + dialogue | 226,357 (pre-post) | 218,608 / 9,823 / 230 / 715 | 33,912 / 181,373 / 6,984 / 7,107 | 147,610 → 147,581 |
| 4001 field | 208,586 (pre-post) | 219,327 / 9,805 / 244 / 0 | 33,328 / 186,415 / 5,592 / 4,041 | 120,219 → 120,187 |

The replay rows match §10. (The live-downsampled numbers differ from §10's, which predate the
characters; they are unchanged by this pass.)

MV warp check (§8; frame A warped by B's vectors, compared with B, 4× images): native-only with the
raw vectors, 1351 → 1352 (Shion turning): 1,026,580 samples ≥ 0.5 px, mean 10.33 px, error
14.98 → 4.62, 71% better / 25% worse (unchanged). Live composite with the resolved vectors: before
1,001,314 samples, 15.06 → 4.69; after 1,020,978 samples, 14.94 → 4.64 (70% / 25%). Field
4001 → 4002: 4,985 samples, 6.48 → 5.94 both before and after. 1550 → 1551 is the same frame.

### The GS-owned parts of the characters at 1550

In `owner_1x` at 1550 Shion's legs and the dark parts of her jacket, KOS-MOS's lower legs at the
right edge and a blob under the soldier are GS-owned. A GS trace (`XSG_GPU_FX_TRACE=1549`) shows that
every character pass is drawn as native (marked) in the ownership GS; what takes these pixels are
the game's own **afterimage strips** (PATH2, after the characters and before more native batches):
pairs of narrow strips around the player and at (420–468, 393–493), first FBMSK 0x7fffffff
alpha-bit marks (TEST: alpha NEVER + FB_ONLY, Z GREATER), then strips that read the frame buffer
itself (TEX0 = FBP·32) shifted by a few pixels and blend it in with ALPHA (Cs − Cd)·FIX 0x10 + Cd,
alpha test EQUAL 0x10. They change those pixels in the main GS too, so the owner plane is right:
the GS really wrote them last, with an effect the native layer does not draw. The blob under the
soldier is a flat ABE fan (his shadow, ALPHA 0x44). Showing them at S× needs the strips replayed at
S×; they cannot start the post phase, because native batches follow them in the same frame (a
mid-scene frame read needs the native layer resolved into the frame first, as in "next steps").
Left as is.

## 10. Effects at high resolution and exact ownership (native_fx.*)

`port/gpu/native_fx.{h,cpp}` + `native_fx.comp`; `composite.comp` now takes an ownership plane.
Shared-file edits are small: `xsg_gpu.cpp` lets `xsgnative::Gif` do the paraLLEl-GS transfer,
`native.cpp` calls the fx hooks from `Gif`/`Batch`/`Vsync`/`Live`/`Shot`.

### Inventory (GS traces, `XSG_GPU_FX_TRACE=v1,v2`, plus `src/main`)

| effect | where | GS operations |
|---|---|---|
| defocus blur (type 01/03, preset 1/2) | hangar cutscene ~1401, **and every field frame** (light) | full-screen FST sprites, TEX0 = the draw buffer itself (TBP = FBP·32, CT32, TBW 8), ALPHA 0x44 with vertex alpha 0x40, TCC 0, bilinear, UV offset by a few pixels; TEST ZTE GREATER with the sprite's Z, so only far pixels blur (Z-based DOF); ZMSK |
| alpha-only sprite before the blur | hangar | full-screen sprite, FBMSK 0x00ffffff (writes destination alpha) |
| dialogue box | hangar, field 1550 | REGION_REPEAT stripe sprite (T8+CLUT), Ad-blended strip (ALPHA C = Ad), glyph sprites (T4, ALPHA 0x11, alpha test GREATER 0) |
| screen fade (`nml_model_set.c` fade_render) | hangar 1401 (alpha 0x34) | full-screen flat sprite, ALPHA 0x44, TEST 0x30000 |
| afterimage strip | field, around the player | narrow tri strips: FBMSK 0x7fffffff alpha-bit marks, then strips reading the frame (ALPHA FIX 0x10) |
| radar/minimap (`game_radar.c`) | field | own texture, ALPHA 0x44 sprites; no frame read |
| others in code, not reached | `game_defocus.c` | 05 mosaic (REGION_REPEAT frame read), 07 radial zoom, 08 glare streaks, 09 PMODE/EXTBUF feedback, 10 frozen-frame fade (local→local copy), 11 wave distortion |
| PMODE | whole run | constant 0x8004 (vsync 0–9000): no two-circuit fades reached; only defocus type 09 changes it |

### Exact ownership (replaces the colour check)

A second paraLLEl-GS instance (on its **own** Vulkan context/device: two GSInterfaces on one
Granite Device deadlock in `next_frame_context`) receives the same GIF stream, except that each
native batch's PATH1 packet is drawn with fog forced on (FGE, F = 0, FOGCOL = marker), ABE and
DTHE off; Z, alpha test, masks and scissor are untouched. Local→host transfers are disabled there.
A pixel is native-owned iff the ownership GS still holds the marker colour there at the end of
the frame (or at the effect snapshot). `composite.comp` takes the native S× sample exactly on
owned pixels. `XSG_GPU_NATIVE_OWNER=0` restores the old colour/depth rule. Cost: a second
paraLLEl-GS (on its own thread since §9, so it costs the main thread almost nothing) and a
readback of the displayed buffer's pages per composited frame.

### Effect replay

Once native batches have drawn into buffer F this frame, the first later draw into F that
(a) samples F or its Z as a texture over ≥ 50 % of the screen, or (b) is a blended full-screen
sprite that is not a no-op, starts the **post phase**: the GIF message is split there, both GSes
are flushed and their VRAM snapshotted (colour, Z, owner), and every later draw into F is
recorded with its full state. A native batch after the trigger cancels it (mid-scene sprite).
At vsync the composite starts from the snapshot, and `native_fx.comp` redraws the recorded
draws at S× on it (one dispatch per primitive): sprites and triangles with the top-left rule,
Gouraud/flat, STQ/UV, TFX (all four), REGION_CLAMP/REPEAT, nearest/bilinear (4-bit), fog,
alpha test + AFAIL, DATE, Z test/write against the S× Z (native depth in GS units where owned),
ALPHA (A,B,C,D with As/Ad/FIX), PABE, COLCLAMP, FBA, FBMSK. A frame read samples a copy of the
S× surface with UVs ×S. Other textures are decoded from the GS-memory mirror. Not replayable
(→ the frame falls back to 1×, logged): points/lines, Z read as texture, textures the GS rendered
this frame, transfers into F, > 4096 post draws. Small frame reads (the afterimage strip) do not
trigger; the ownership plane hands their pixels to the GS (1×). PMODE: a blend pass with BGCOLOR
(SLBG) or same-buffer circuits runs after the replay; a second circuit reading another buffer
is not done at S× (not reached).

### Validation (scale 4, headless, RTX 5090)

| vsync | post draws | replay at 1× on the snapshot vs paraLLEl-GS: exact / ≤2 / ≤8 / >8 | live 4× box-downsampled vs paraLLEl-GS |
|---|---|---|---|
| 1401 hangar (DOF + box + fade) | 9 | 201,524 / 27,606 / 246 / **0** | 169,687 / 58,006 / 1,273 / 410 |
| 1550 field + dialogue | 62 | 218,608 / 9,823 / 230 / 715 | 45,110 / 177,886 / 4,072 / 2,308 |
| 4001 field | 2 | 219,327 / 9,805 / 244 / **0** | 41,782 / 183,934 / 3,503 / 157 |
| 6001 field | 2 | 219,333 / 9,801 / 242 / **0** | 41,773 / 183,946 / 3,500 / 157 |

1401 was a 1× fallback before (composite took 0 px); now the native layer owns 171,631 px
before the post phase and the whole frame is S×. 1550's remaining >8 are glyph edges. Speed in
the field: ~66 vsyncs/s headless (was ~85), the cost of the per-frame snapshot (the field blurs
every frame) and the ownership GS. Images: `port/docs/screenshots/fx_VVVVVV_{live_4x,
live_down_1x,gs_1x,owner_1x,replay_1x}.png`. Shot logs show ownership, post phase, trigger.

### With characters (merge)

Set-2 batches take part in ownership like set 0: every GPU_NATIVE_SET2 message marks the next
PATH1 GIF tag (two for the two-pass programs, whose one packet holds tags 1011 and 1012) and
GPU_NATIVE_SET2_ENV marks the env-pass packet, so the ownership GS draws all character passes in
the marker colour; `NativeFbp` gets each pass's FRAME. In the live resolve the order is
composite, effect replay, PMODE, then the motion-vector pass, so MV sees the final image and
gives post-effect and GS-owned pixels zero motion.

### Deferred effects, texture order and the CLUT buffer (2026-11-09)

- **Deferred effects** (`native_fx.cpp` `Defer`, `g_ov`): a blended triangle / sprite drawn into a
  buffer that native batches drew into this frame is recorded and replayed at S× (before any post
  phase) instead of left to the 1x GS. Its vertex kicks reach the ownership GS without their drawing
  bit (ADC, or XYZ2 -> XYZ3), so the native pixels under it stay owned; inside its box the composite
  takes the non-native GS colour from the ownership GS (the scene without it). Not deferred, and kept
  on the GS with a "veto" box (main GS colour there): masked colour writes, DATE, frame / Z / fresh
  render-target textures, points / lines, REGLIST kicks. Before, battle sparks, barriers and glows
  took everything behind them down to 1x. `[debug] fx_defer = 0` turns it off.
- **Texture order**: `native.cpp` `Gif` now feeds the upload mirror qword by qword with the effect
  parser (`xsgfx::Gif` `pre`); recorded draws used to decode the *last* upload of a message (one
  texture area is reloaded a dozen times per message in battle: the garbled effect squares).
- **CLUT buffer** (`ClutLoad`): indexed textures take their colours from a CLUT buffer loaded on
  TEX0 / TEX2 per CLD (and CSA for 4-bit), as the GS does, not from palette memory at decode time.
- **GS-rendered textures** (`GsRenderedTexture`): a native batch sampling pages the GS rendered into
  after their last upload decodes its texture from the GS memory at composite time.

# Native renderer: design and live slice

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

Characters (set 2), UI (`xgl_*` builders over PATH2/3), fonts, movies (IPU → PATH3 image
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
* Characters (set 2) depth-test against the 1× GS Z, so their silhouettes stay 1×.

## 4. Running it

```
XSG_GS=gpu XSG_GPU_NATIVE=1 XSG_GPU_NATIVE_SCALE=4 XSG_AUDIO=0 XSG_MPEG=0 XSG_SKIP_EVENTS=1 \
XSG_PAD='300:START,1100:START,1200:CROSS,1500:CIRCLE*400@60' port/build-wt/xsg      # window
# headless captures: + XSG_GPU_HEADLESS=1 XSG_GPU_NATIVE_SHOT=1401,1550,4001 XSG_GPU_NATIVE_DIR=/tmp/native
```

| variable | meaning |
|---|---|
| `XSG_GPU_NATIVE=1` | native set-0 path (game: skip VU1; xsg-gpu: live composite) |
| `XSG_GPU_NATIVE_SCALE` | 1–8 (default 2) |
| `XSG_GPU_NATIVE_LIVE=0` | no live composite (shots only) |
| `XSG_GPU_NATIVE_THR` | rule-2 colour threshold (default 12) |
| `XSG_GPU_NATIVE_VU1=1` | keep the VU1 interpreter for native batches (A/B) |
| `XSG_GPU_NATIVE_SHOT`, `_DIR` | diagnostic images per listed vsync |

Per shot vsync: `native_VVVVVV_{gs_1x, native_only_1x, comp_1x, diff_1x, native_only_Sx,
comp_Sx, live_Sx, live_down_1x}.png`. `live_Sx` is a readback of what the window shows;
`live_down_1x` is it box-downsampled. The `[xsg-gpu]` stats line counts native presents.

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

## 7. Effects at high resolution and exact ownership (native_fx.*)

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
paraLLEl-GS and one 4 MB readback per composited frame.

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

## 6. Next steps

0. Effects: avoid the per-frame VRAM snapshot (read the GS frame on the GPU), decode
   GS-rendered textures from the final VRAM instead of falling back, motion vectors for replays.
1. Characters: port set 2 (`skin_lit` and family; the references exist for all 12 loops) the
   same way (reference in place of VU1, native draw), with the specular second pass (CTXT 2,
   tag 1012) and the env-map forms. Then characters get S× depth against the scenery.
2. Effects at S×: detect GS draws that sample the displayed frame/Z (TEX0.TBP0 in the FRAME/ZBUF
   range) and redo the hangar DOF and fades on the native target, so post-processed frames stay
   high-res. Replace rule 2 with an exact last-writer plane once more of the frame is native.
3. Motion vectors: keep last frame's M962 and bone palette per `(model, parts, material,
   batch)` key (VU1_PROGRAMS.md §5) and output `clip_prev` from the vertex shader.
4. Move the message build to the EE builder and cache geometry per REF as GPU buffers; drop the
   per-frame VRAM readback by reading paraLLEl-GS VRAM on the GPU.
5. Mirror completeness (local→local, render-to-texture, CSM2), Ad blending, dithering.
6. Widescreen: widen M962's projection and the GS scissor/guard band.

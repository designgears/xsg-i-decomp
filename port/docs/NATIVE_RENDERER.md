# Native renderer: design and first slice

Branch `pc-port` (agent worktree), 2026-11-07. Builds on [VU1_PROGRAMS.md](VU1_PROGRAMS.md)
(programs, I/O, C references, the motion-vector plan), [STAGE9_GPU.md](STAGE9_GPU.md) (xsg-gpu,
the IPC ring) and [STAGE10_PERF.md](STAGE10_PERF.md) §6 (the VU1 interpreter).

**Goal.** Draw the 3D scene with our own Vulkan pipeline, in the style of ico-pc, instead of
emulating VU1 + GS at 512×448. That gives real high resolution, room for widescreen, and
true per-vertex motion vectors for DLSS. paraLLEl-GS stays for everything that is not
(yet) native.

**Status of the slice:** `XSG_GPU_NATIVE=1` redraws every set-0 strip batch (program 0x0bd8,
loop 0x0c18: static scenery) natively, at any `XSG_GPU_NATIVE_SCALE`. In the tutorial field it
matches paraLLEl-GS to ±2 per channel on 97.0% of the pixels where the native surface is
visible (dialogue-box frame; the rest is polygon edges). On the vsyncs listed in `XSG_GPU_NATIVE_SHOT` it composites over the paraLLEl-GS
frame and writes images. There is no live presentation of the composite yet (§4).

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

### What the slice does instead: VU1 run boundary

For the first slice the hook is one level lower, in `gs_bridge.cpp` `RunVu1`. After an
MSCNT batch into loop 0x0c18 (checked by `vi15` and a 16-byte micro-memory signature of set
0), it copies the batch's inputs out of VU1 data memory: qw 962–967, 971 (vf24), 972, 1007,
1011, and the P/T/C streams at TOP+1…. They are the same values the EE uploaded. They are
taken after the 0x0170 batch header, so morph is already applied and colours are already
floats. Nothing in `src/` changes, the VU1 output is still kicked to paraLLEl-GS, and the
references prove the data contract (test_vu1_ref). The cost is that VU1 still runs, so this
is a correctness slice, not a speed one.

Moving the hook up to the EE builder changes only where the message is built. The renderer
side is the same.

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
| sampling | TEX1.MMAG nearest/linear; CLAMP repeat/clamp (region modes → clamp) |
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

**Target design (live).** The composite rule is a validation tool. Real hybrid rendering
must interleave instead of overlaying, because post effects read the frame buffer (see the
cutscene result in §5):

* Render the native batches into a high-resolution colour + depth target, in ring order, at
  the point the GS would have drawn them. The GS still receives a cheap
  **depth/coverage-only** version of each native batch, so its Z buffer stays right for the
  characters drawn after it. Characters then depth-test against the low-resolution Z, which
  is fine for a first step.
* When the GS reads the frame buffer as a texture (post effects, detected by TEX0.TBP0 inside
  the FRAME range), or at the end of the frame, resolve: downsample the native target into
  paraLLEl-GS VRAM (a `GSInterface` hook, or a compute pass on its VRAM buffer) so GS-side
  effects see the native pixels.
* At vsync, present `native high-res ⊕ GS non-native layer`. The GS layer is upscaled, and
  the mask comes from a per-pixel "last writer was native" bit (a stencil-like plane kept in
  the native pass and invalidated by GS draws that touch the pixel). That replaces the
  colour heuristic.
* Movies and 2D-only frames have no native batches. The present path then shows the
  paraLLEl-GS scanout unchanged, as today.
* Depth sharing at high resolution, in the other direction (characters native too), comes
  for free once set 2 is native. Then only 2D and effects remain on the GS.

## 4. Running it

```
XSG_GS=gpu XSG_GPU_HEADLESS=1 XSG_AUDIO=0 XSG_MPEG=0 XSG_SKIP_EVENTS=1 XSG_FAST=1 \
XSG_PAD='300:START,1100:START,1200:CROSS,1500:CIRCLE*400@60' \
XSG_GPU_NATIVE=1 XSG_GPU_NATIVE_SCALE=4 XSG_GPU_NATIVE_SHOT=1401,1550,6001 \
XSG_GPU_NATIVE_DIR=/tmp/native port/build-wt/xsg
```

For each shot vsync it writes `native_VVVVVV_{gs_1x, native_only_1x, comp_1x, diff_1x,
native_only_Sx, comp_Sx}.png`. `diff_1x` is black or dim green for exact matches, grey
scaled ×8 for differences, and the dimmed GS frame where nothing native covers. The log
gives per-shot statistics. At 30 fps a shot on a vsync with no batches uses the previous
vsync's frame. Batches and textures are kept only in the 3 vsyncs before a shot. Otherwise
only the GIF tracker and the GS-memory mirror run (the mirror costs one `Memory::Write`
per uploaded pixel). `XSG_GPU_NATIVE=2` also logs vertex 0 of every 2000th batch, recomputed
on the CPU against the VU1 output. They agree to 1/16 pixel.

## 5. Validation (2026-11-07)

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

1. Live path: native pass in ring order + depth-only GS stand-ins + resolve-before-effects +
   presentation of the high-resolution target (§3 "target design"). Drop the CPU composite.
2. Move the hook to the EE builder (§1) and stop running VU1 for native programs. Send each
   model's geometry once and cache it as a GPU buffer, keyed by the REF.
3. Port set 2 (`skin_lit` and family, the references exist for all 12 loops). This puts
   characters on the native path, with the specular second pass as a second draw (CTXT 2,
   GIF tag 1012) and env-map 6-kick forms as a third.
4. Motion vectors: keep last frame's M962 and bone palette per `(model, parts, material,
   batch)` key (VU1_PROGRAMS.md §5) and output `clip_prev` from the vertex shader.
5. Mirror completeness: local→local copies and render-to-texture, CSM2, Ad blending and
   dithering; then shader tests through `test_vu1_ref --tol`.
6. Widescreen: widen M962's projection, so the screen scale in 966 changes, and the GS
   scissor/guard band for the non-native layer.

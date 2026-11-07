# VU1 programs: inventory, I/O spec, C references

> **Settings (2026-11-07):** the port no longer reads `XSG_*` environment variables. Every
> `XSG_FOO=v` in this document is now `[section] key = v` in settings.ini (the mapping is the
> "replaces" column of [MENU.md, settings.ini reference](MENU.md#settingsini-reference)); run
> with `XSG_CONFIG=<file> port/build/xsg`. Samples: [examples/](examples/).

Branch `pc-port` (agent worktree), 2026-11-07. This is groundwork for a native renderer in the
ico-pc style: each VU1 microprogram becomes a vertex shader, VIF UNPACK data becomes vertex
streams, and VU1 data memory becomes a constant buffer. Builds on [STAGE6_VU1.md](STAGE6_VU1.md)
(the interpreter) and [STAGE8_GFX.md](STAGE8_GFX.md) (first real 3D).

Nothing here touches the GS path. The capture hook adds one line to `gs_bridge.cpp` (to note
UNPACK codes) and a few lines to `vu1.cpp`.

## Files

| path | what it is |
|---|---|
| `port/tools/vu_disasm.py` | VU micro-mode disassembler. Its decode tables are the interpreter's. Reads `--blob [--set N]`, `--sets` or `--micro FILE` |
| `port/tools/vu1_features.py` | static scan per program: size, subroutine calls, building blocks |
| `port/tools/vu1cap_stats.py` | capture summary: runs per entry/loop, GIF prim types, UNPACK formats |
| `port/runtime/vu1_capture.{h,cpp}` | `XSG_VU1_CAPTURE=dir`: records VU1 runs and their XGKICK output |
| `port/render/vu1_ref/` | C references: `vu1_ref.h` (API), `vu_float.h` (VU float rules), `vu1_ref.c` (dispatch + common programs), `vu1_set0.c`, `vu1_set2.c` (+ `vu1_set2_int.h` shared header/skin/facing, `vu1_set2_spec.c`, `vu1_set2_rigid.c`, `vu1_set2_blend.c`) |
| `port/render/vu1_ref.cmake` | `xsg_vu1_ref` library and `test_vu1_ref`; included from `port/CMakeLists.txt`, not built by default |
| `port/tests/test_vu1_ref.c` | runs the references against captures |

## 1. Microcode: seven sets, not one

`vudata_rest.databin.bin` holds **seven** DMA chains, and each chain is a full microcode image
uploaded to VU1 address 0 (`vu_disasm.py --sets`):

| set | size | pairs | `s_nProgType` | EE table family (`s_aUcodeTbl` column) | seen in the cutscene |
|---|---|---|---|---|---|
| 0 | 0x2458 | 1163 | 2 | `s_aUcodeAdr` / `FaceAdr` / `BackAdr` / `AdrZbuf` | yes: set and hangar geometry |
| 1 | 0x0f78 | 495 | 6 or 7 | `s_aUcodeTexEnv` / `s_aUcodeDrop` (max 0xd68 / 0xc80) | no |
| 2 | 0x3a68 | 1869 | 1 | `s_aUcodeEnv*Adr` | yes: characters (skinned) |
| 3 | 0x1a90 | 850 | 3 | `s_aUcodePro*Adr` (projection) | no |
| 4 | 0x2748 | 1257 | 5 | `s_aUcodeAdd*Adr` | no |
| 5 | 0x3d30 | 1958 | 4 | `s_aUcodeAddEnv*Adr` | no |
| 6 | 0x0dc8 | 441 | 7 or 6 | `s_aUcodeDrop` / `TexEnv` | no |

STAGE6's "five MPG uploads, byte-identical" describes set 0, the first chain. During the
cutscene the game switches between sets 0 and 2 within a frame, whenever `s_nProgType`
changes. The capture shows two distinct `MICR` images per frame.

The family-to-set mapping comes from the largest entry address in each `s_aUcode*` table
(main.elf data, `s_aUcodeTbl` at 0x004A9868). A table can only belong to a set that is large
enough to hold its entries. Sets 0 and 2 are confirmed by the capture. The pairs 1/6 and 3/4/5
are inferred from sizes only.

All seven sets share the first 0x50 bytes:

| entry | code | meaning |
|---|---|---|
| 0x0000 | `nop[E]` | empty program; also where an MSCNT continues after a kick program |
| 0x0010 | `iaddiu vi14,0x3d5 ; b 0 ; xgkick vi14` | kick the A+D packet that `nmlPacketAddGsFlush` UNPACKed at qw 0x3d5 |
| 0x0028 | `nop[E]` | |
| 0x0038 | as 0x10 at qw 0x2dc | `nmlPacketAddGsFlushWide` (`D_28` = 0x28 in the PS2 link) |
| 0x0050 | 21 `lq` + table init | the constant loader every entry calls (`jalr vi15, 0xa*8`) |

### Who issues MSCAL

* `nmlPacketAddGsFlush` / `GsFlushWide` (nml_packet_add.c): `MSCAL 0x28>>3` / `(0x28-0x28)>>3`
  (0x10 / 0x38 after the VIF imm×8). Each one kicks GS register state such as TEX0, ALPHA or TEST.
* `add_exec_prog` → `nmlPacketAddExecProg` (nml_packet_add.c): `MSCAL program>>3`, where
  `program = s_aUcodeTbl[row].programs[s_nProgType-1][slot]`. `row` is 0..5 (0 Zbuf, 1 Face,
  2 Back, 3 plain, 4/5 mixed). The material decides the `slot`. For `s_nProgType` 1
  (set 2): `{0,4,8}`[render_flags & 2 / & 4] + {0, +1/+2 (material_flags & 8, specular
  variants), +3 (material_flags & 0x1000, env)}. For type 2 (set 0): 0/1 (flag 0x10) or 2..5.
* The geometry itself (UNPACKs + `MSCNT`) is prebuilt in the model file. `nmlPacketAddTransData`
  `REF`s it (`material + 0x24`). The EE does not build vertex data per frame.
* No `xgl_*` packet builder emits MSCAL. `xgl_packet.c`, `xgl_prim_add.c` and
  `sv_get_image_item.c` use only FLUSH/DIRECT. In the cutscene, every VU1 run is an nml model.

### How a draw runs

```
MSCAL entry          entry: jalr 0x50 (constants -> vf8, vf12..vf31; ADC tables), set vi15 =
                     its vertex loop, then jalr into the shared stop pair (set 0: 0x160,
                     set 2: 0x178): nop[E]
{ UNPACK -> TOP      the batch (header + streams), double-buffered by VIF BASE/OFFSET
  MSCNT }*           resume at 0x170 / 0x188: the batch header parses TOP, then jr vi15 into
                     the loop; the loop ends with  b <entry+0x20> ; xgkick  -> stops again
```

A **program** is therefore the pair (set, entry), and its vertex loop is identified by `vi15`.
Each MSCNT run is one batch and makes one XGKICK. The 6-kick variants kick once per
sub-batch.

### Counts (capture of vblanks 1300/1400/1500/1600/1700, 53,693 runs)

`vu1cap_stats.py --sets /tmp/vu1cap/vu1cap_00*.bin`. The `0x0170>0x0c18` form means
"MSCNT at 0x170 that returns into the loop at 0xc18".

| set | entry → loop | MSCAL | MSCNT batches | vertices | prims | reference |
|---|---|---|---|---|---|---|
| all | 0x0000 / 0x0010 | 845 / 845 | – | – | A+D state | `nop`, `gs_flush_3d5` ✔ |
| 0 | 0x0bd8 → 0x0c18 *strip* | 136 | 19,720 | 82,253 | tstrip 0x01c/0x03c | ✔ |
| 0 | 0x0f90 → 0x0fd0 *strip_face* | 13 | 34 (with 0x12d8) | 228 | tstrip 0x05c | ✔ |
| 0 | 0x12d8 → 0x0fd0 *strip_back* | 13 | (shared) | | | ✔ |
| 2 | 0x05b0 → 0x05f0 *skin_lit* | 85 | 6,308 | 31,625 | tstrip 0x03c/0x07c | ✔ |
| 2 | 0x08e0 → 0x0920 skin_lit_spec | 20 | 810 | 4,887 | 0x01c/0x05c | ✔ `skin_lit_spec` |
| 2 | 0x0d60 → 0x0da0 skin_lit_spec_2pass | 48 | 5,971 | 40,683 | 0x01c + 0x24c (pass 2, CTXT 2, ABE) | ✔ `skin_lit_spec_2pass` |
| 2 | 0x1228 → 0x1268 skin_lit_spec ×6 kicks | 12 | 310 | 1,619 | | ✔ `skin_lit_spec6` |
| 2 | 0x1790 → 0x17d0 rigid_lit | 73 | 1,817 | 9,632 | 0x03c | ✔ `rigid_lit` |
| 2 | 0x1a88 → 0x1ac8 rigid_lit_spec | 13 | 367 | 1,502 | | ✔ `rigid_lit_spec` |
| 2 | 0x1ec0 → 0x1f00 rigid_lit_spec_2pass | 55 | 1,817 | 11,484 | 0x24c/0x26c | ✔ `rigid_lit_spec_2pass` |
| 2 | 0x2338 → 0x2378 rigid_lit_spec ×6 | 24 | 1,408 | 7,885 | | ✔ `rigid_lit_spec_6k` |
| 2 | 0x2850 → 0x2890 lit (blend 8) | 155 | 5,949 | 26,165 | | ✔ `blend_lit` |
| 2 | 0x2be8 → 0x2c28 lit_spec (blend 8) | 40 | 3,798 | 14,886 | | ✔ `blend_lit_spec` |
| 2 | 0x3058 → 0x3098 lit_spec_2pass (blend 8) | 52 | 2,071 | 13,752 | | ✔ `blend_lit_spec_2pass` |
| 2 | 0x3510 → 0x3550 lit_spec ×6 (blend 8) | 34 | 842 | 5,090 | | ✔ `blend_lit_spec_env` |
| 2 | 0x2fe8 / 0x3020 (Face/Back of 0x2be8) | 4 / 4 | (shared) | | | ✔ (face/back) |

A reference now exists for every one of the 53,693 runs (all set-0 and set-2 programs that
ran). "blend 8" is a **two-bone** blend: B.x/B.y are the two bone addresses, B.z/B.w their
weights, packed into one quadword (skin_lit uses four addresses + a separate weight qw).

The names come from the static scan (`vu1_features.py --set 2 --entries …`). Within each
group of four in `s_aUcodeEnvAdr`, the programs grow from plain lighting to a specular power
loop (`mul vf5,vf5,vf5` repeated `1008.x` times), then to a second strip with GIF tag 1012
(two-pass, specular/env in GS context 2), then to the multi-kick form. The first group skins
(the 4-bone loop), the second is rigid (one matrix), and the third repeats the first two for
`render_flags & 4`. The Face/Back entries (7 pairs each) only set `mem[995].y`, the
triangle-facing mode, and branch into the main loop.

**Set 0, other entries** (`vu1_features.py --set 0`): 0x0de0 rigid strip with the 0x408 header
(no morph); 0x11e0/0x1320 per-vertex subroutine form (calls 0x888, 0x900/0x9f8); 0x1418
lit + fog; 0x16f8 lit + specular; 0x1b08 two-pass; 0x1f60 six kicks; plus their Face/Back
stubs. None of these ran in the captured frames.

## 2. Per-program I/O

### Constants (VU1 data memory, qw address; EE builder in `nml_packet_add.c`)

| qw | reg | content | built by (the `sceVif1PkOpenUpkCode` address in nml_packet_add.c) |
|---|---|---|---|
| 0x2DC + 4k (732..) | bone stream addresses | bone k matrix, 4 qw (set 2) | `nmlPacketAddParts`: `model+0x258` matrices by `parts+0x30` ids |
| 734.w | | morph weight (key index + fraction) | (part of the same upload) |
| 962–965 | vf28–31 | **object → clip matrix** (rows), the motion-vector matrix | `nmlPacketAddParts` 0x3C2: `camera->viewMatrix` [× `model+0x180`] [× `model+0x80`] × part local |
| 966 / 967 | vf26 / vf27 | screen scale / offset (`.w` of 967 also picks the register-batch layout) | `nmlPacketAddScreen` 0x3C6/0x3C7 |
| 968 | vf25 | material diffuse (× 971 at entry), `.w` = alpha | `nmlPacketAddBlockMaterial` 0x3C8 |
| 969 / 971 | vf24 | vertex colour scale (971 reloaded by every batch header) | `nmlPacketAddBlockMaterial` 0x3C9 |
| 972 | | UV offset `.xy` (texture scroll) | `nmlPacketAddBlockMaterial` |
| 973–975 (+976) | vf17–19 (+vf8) | normal → light matrix; row 3 = eye position in object space | `nmlPacketAddParts` 0x3CD (`light`, `trans = inv(world)·camera->position`) |
| 977–980 | vf20–23 | light colours (columns) + ambient | `nmlPacketAddParts` light block (0x3D1 in BlockMaterial) |
| 991–995 | | facing ring buffer (3 positions), `995.x` index, `995.y` mode | microcode |
| 998.w | | XYZF2 w bits (fog word) for set 2 | `nmlPacketAddFog` (998) |
| 999/1000.w | | ADC words: 0 / 0x7000 for "clipped"; `999.x` = 0x8000 (set 2 all-culled) | entry 0x50 |
| 1007 | | fog: x min, y max, z bias, w scale | `nmlPacketAddFog` (1007) |
| 1008 | | x: specular power steps, z: renormalise, w: 2 = morph colours | `nmlPacketAddBlockMaterial` |
| 1011 / 1012 | | GIF tag template, pass 1 / pass 2 | `nmlPacketAddGifTag` (0x3F3/0x3F4/0x3F2) |
| 1013–1016.w | | ADC words for the facing test (0x4000/0); Back entries swap them | entry |
| 1017 | | x: packet qw, y: batch TOP (microcode); z: mirror-y plane | microcode / EE |
| 0x3D5 / 0x2DC | | A+D GS-register packet for `MSCAL 0x10/0x38` | `nmlPacketAddGsFlush[Wide]` |

### Vertex streams (one UNPACK per stream, measured: the `UPK` records, `vu1cap_stats.py`)

Batch header at TOP+0: `x` = n \| 0x8000 (EOP, copied into the GIF tag), `y` = 0x4000 for a
register batch (two A+D qw kicked from TOP+0xf3), `z` = constant-colour fill count (set 2) or a
constant reload (register batch), and `w` = the morph key count.

**Set 0** (strip, strip_face), with n vertices:

| stream | qw | UNPACK | content |
|---|---|---|---|
| header + P | TOP+0, +1.. | V4-32 (one UNPACK from +0) | position xyz, **w = S** |
| T | +1+n | S-32 or S-8, masked into w | **w = T** |
| C | +1+2n | V4-8 unsigned | RGBA as integers. The header converts them (ITOF0) in place |
| N | +1+3n | V3-32 | normal (strip_face only) |
| morph keys | +1+6n | V4-32 | pairs of xyz per vertex, stride keys+1 |

**Set 2** (all V4-32): P (+1, w = S), N (+1+n, w = T), C (+1+2n, float RGBA), (+1+3n unused by
skin_lit), B (+1+4n, four bone **qw addresses** as integers), W (+1+5n, four weights). The
bone loop stops at the first zero address after the first bone.

### Computation

`strip` (set 0, loop 0xc18), per vertex:

```
clip  = M962 · (P.xyz, 1)            CLIP(clip.xyz, |clip.w|)    q = 1/clip.w
STQ   = ((0+P.w)+uv.x, (0+T.w)+uv.y, 1) · q
RGBAQ = ftoi0(min(C · vf24, 255))
fog   = clamp(1007.z + 1007.w · q, 1007.x, 1007.y)
XYZF2 = ftoi4(967 + (clip·q) · 966) ; w = sext16((ftoi4(fog) & 0xff0) | ADC-shift)
ADC-shift = ((shift | (clipped ? 0x7000 : 0)) << 1) & 0xffff      -> bit 15 = ADC
```

`strip_face` (0xfd0) adds `face = mem[1013 + 2·(dot(eye − P, N) < 0)].w`, ORed in before the
shift. A clipped vertex sets ADC on itself and the next two vertices. That drops every strip
triangle that uses it. **These programs have no clipper:** the only clipping is guard-band
rejection by ADC, and the GS scissor does the rest. A shader port can clip normally. To
match the output, it must still drop a triangle if any of its vertices fails CLIP against
|w|, and the 2048-pixel guard band makes that rarely visible.

`skin_lit` (set 2, loop 0x5f0), per vertex:

```
for k < 4 while bone_k != 0:  pos += w_k·(M_k·(P,1)),  nrm += w_k·(M_k3·N)   (pos/nrm start 0)
if 1008.z: nrm *= 1/|nrm|                                   (ERLENG)
if 1017.z: pos.y = 1017.z − pos.y                           (mirror)
if 995.y : face = facing of the triangle (ring[−1], ring[−2], pos) against the eye,
           winding flipped on odd strip positions -> 0x4000 into the ADC shift
L     = max(M973 · nrm, 0)
rgb   = 980 + (M977 · L) · 968        a = 968.w
RGBAQ = ftoi0(min((rgb, a) · C, (255,255,255,128)))
XYZF2 = ftoi4(967 + (M962·pos / W) · 966), w = sext16(998.w | ADC-shift)
STQ   = as strip
```

If every vertex of a batch ends with ADC set, the program kicks qw 0 instead of the batch
(`999.x` = 0x8000). The GIF walker then reads whatever tag is at qw 0. This shows up as the
"nreg 16, no PRE" tags in the stats, which are not real draws.

The specular variants (references in `vu1_set2_spec.c`, `vu1_set2_rigid.c`, `vu1_set2_blend.c`) also compute the reflection of the eye vector
(`erleng` of `eye − pos`), raise `max(R·L, 0)` to the power 2^(1008.x) by repeated squaring,
and either add it to the colour (one pass) or write it as a second strip. The second strip
reuses the vertices' STQ/XYZ with GIF tag 1012, and its colour is `M977 · spec · (970 ×
1008.y)`.

Details from the references (same for the skinned, rigid and two-bone groups):

* **specular**: `v = 976 − pos`, `p = 1/|v|`, `d = (973.w, 974.w, 975.w) − pos`,
  `R = 2·(nrm·d)·p·nrm − v·p`; `S = max(M973·R, 0)` squared `1008.x + 1` times;
  `sp = (M977·S)·(970·1008.y)`.
* **one pass** (0x0920, 0x1ac8, 0x2c28): `RGB = min(min(lit·C, I) + sp·C, 255)`. The first
  vertex of a batch clamps with whatever the I register held at entry (no LOI before it);
  later vertices use 255. The references carry `regs.I` for that.
* **two pass** (0x0da0, 0x1f00, 0x3098): pass 1 is the plain lit strip; pass 2 is a second
  strip at `out+1+3n` with tag 1012 (header NLOOP/EOP), the same STQ/XYZF2, colour
  `min(sp·C·(970·1008.y), 255)`, alpha `min(C.w, 128)`. Kicks: 1017.x, then the pass-2 tag.
* **six kicks** (0x1268, 0x2378, 0x3550): pass-1 RGB = `min(min(lit·C,255) + 128·sp·(970·1008.y), 255)`.
  Pass 2 (tag 1010, constant colour qw 1009) is **environment-mapped**:
  `e = M1004·(M973·R)` (before the clamp), `s = ((e.x+1)·0.5·vf24.y + vf24.x)·q`,
  `t = ((e.y+1)·0.5·vf24.w + vf24.z)·q`; the STQ w lane is `vf8.w`. Kicks: 1017.x, 0x3fa,
  0x3da (A+D state for the env pass), the pass-2 packet, 0x3d5, 0x3fd (state restore).
* **rigid** (0x17d0…): one matrix per vertex, addressed by the bone stream's x lane, no weights.
* **two-bone** (0x2890…): `pos = Bz·M_Bx(P) + Bw·M_By(P)`; the lit loop blends both even when
  B.y = 0 (then qw 0..3 is read as a matrix); the specular loops skip bone 1 when B.y = 0.
* Face/Back entries of every group (0x0cf0/0x0d28, 0x11b8/0x11f0, 0x1720/0x1758, 0x1a18/0x1a50,
  0x1e50/0x1e88, 0x22c8/0x2300, 0x27e0/0x2818, 0x2b78/0x2bb0, 0x2fe8/0x3020, 0x34a0/0x34d8,
  0x39f8/0x3a30) run the constant loader and set 995.y; only 0x2fe8/0x3020 ran in the captures.

### GIF output

Each batch produces a PACKED GIF tag (from qw 1011, NLOOP = n, EOP from the header) with
NREG 3 and REGS = ST, RGBAQ, XYZF2. It is followed by 3 qw per vertex, written in place from
TOP+0xf3. **The ST quadword's w lane is not written.** It keeps whatever the buffer held,
and the references reproduce that by working on a copy of the memory. PRIM comes from the
template: tri-strip (4) + IIP + TME (+ FGE 0x20, + ABE 0x40). Pass-2 strips set CTXT
(0x24c/0x26c). Strip restarts use ADC only (XYZF2 bit 15); there is no XYZ3. The two-pass
programs kick one packet holding both tags.

## 3. C references and the test

The references in `port/render/vu1_ref/` take the run's entry registers plus data memory,
exactly as captured, and return the kicked packets. Each one is a literal reading of its
software-pipelined loop, with the same float operations in the same order.
`vu_float.h` repeats the interpreter's VU float rules: inputs with exponent 255 read as
±FLT_MAX, denormals read as 0, results clamp, MADD rounds the product before the add, and
MXCSR is set to round-toward-zero with FTZ/DAZ.

```
cmake --build port/build-wt --target test_vu1_ref
port/build-wt/test_vu1_ref /tmp/vu1cap/vu1cap_00*.bin          # exit 0 = no mismatches
port/build-wt/test_vu1_ref --tol 2 --verbose ...               # shader-tolerance mode
```

**Result (fresh captures, 2026-11-07, `XSG_GS=gpu XSG_FAST=1`):** every run has a reference
and every run is **bit-exact** with the interpreter, 0 failures, 0 "no reference":

| capture | runs | GIF loops kicked (vertices + A+D) | programs seen |
|---|---|---|---|
| 1300, 1400 (hangar cutscene), 1550, 1700 (tutorial room) | 42,233 | 260,823 | set 0: strip, strip_face (+ entries); set 2: all 12 loops + 0x2fe8/0x3020 |
| field (tutorial room) 6001 | 11,460 | 53,633 | set 0 strip; set 2 skin_lit, skin_lit_spec_2pass, rigid_lit(_spec, _spec_2pass), blend_lit(_spec_2pass) |

That is 35 program/entry kinds: `nop`, `gs_flush_3d5`, set 0 `strip`, `strip_face` and the three
set-0 entries, and set 2's 12 loops with their entries.

**Tolerance.** References compare bit-exact (`--tol 0`) because they repeat the
interpreter's rounding. The `--tol N` mode is for GPU ports, which round to nearest, may
contract into FMA, and have no ±MAX clamp. It accepts ST within N ulp, XY/Z of XYZF2 within N
units of 1/16 pixel, and RGBA, fog, ADC and tags exact. N = 1–2 covers one rounding
difference per product-sum in the 4×4 transform. ADC must stay exact, because it decides
which triangles exist. The interpreter itself differs from the real VU, which has no guard
bits and truncates differently than x87/SSE RTZ. Matching real hardware would also need a
PS2 capture. That is outside this harness.

Not verified: the registers and memory a run leaves behind (only the GIF output is
compared), and `kind` in the capture header (MSCAL vs MSCNT is guessed from `pc == previous
stop`, which also matches MSCAL 0x10 after a stop at 0x0).

## 4. Capture mode

```
XSG_VU1_CAPTURE=/tmp/vu1cap XSG_VU1_CAPTURE_FROM=1300 XSG_VU1_CAPTURE_FRAMES=5 \
XSG_VU1_CAPTURE_EVERY=100 XSG_AUDIO=0 XSG_MPEG=0 XSG_SKIP_EVENTS=1 \
XSG_PAD=300:0x0800,1100:0x0800,1200:0x0040 XSG_MC_DIR=/tmp/xsgmc port/build-wt/xsg
```

This writes `vu1cap_FFFFFF.bin` per captured vblank, about 170–200 MB per frame (~10,000 runs
× 16 KB). The format is in `vu1_capture.h`: `MICR` (micro memory, rewritten when it
changes), `UPK ` (the UNPACK codes before a run) and `RUN ` (header, registers, 16 KB data,
kicks). The hooks are `CapBegin/CapKick/CapEnd` in `vu1.cpp::Run`/`Kick` and one
`CapUnpack` call in the VIF1 UNPACK decoder in `gs_bridge.cpp`. VU0 runs are skipped.

## 5. Motion vectors: feasibility

**Yes, and the matrix is already explicit.** Every geometry batch is transformed by exactly
one constant, `M962` (qw 962–965). It is the object (part) → clip matrix that
`nmlPacketAddParts` uploads (`0x3C2`), right before `add_exec_prog` issues the MSCAL. In set
2, the bone palette (qw 732 + 4k) is uploaded in the same function. The previous frame's clip
position is then `M962_prev · P` (rigid) or `M962_prev · Σ w_k M_k,prev P` (skinned). Both
are available if the renderer keeps last frame's constants per draw.

Stable identity. `nmlPacketAddParts(parts, model, camera)` is called per part with:

* `model` (`NmlModelRenderState *`, the actor's render state) is stable across frames while
  the model is loaded;
* `parts` (a pointer into the model's part table) is stable per part;
* the material (`NmlMaterialRenderState *`, from `nmlPacketAddExecProg`) and its prebuilt
  geometry `REF` (`material + 0x24`) are stable too.

The key `(model, parts, material, batch index within the REF chain)` therefore identifies a
draw across frames. The native renderer would record the key at the 0x3C2 upload and the
MSCAL (EE side, where the pointers are known), with no need to infer it from VU memory. The
previous-frame matrices are a lookup on the same key. Skinning needs the per-bone matrices
(`model + 0x258`, indexed by `parts + 0x30` ids) for both frames, from the same key. The
`s_pMatrixCache` early-out (`changed == 0`) skips re-uploading 962/973 when consecutive
parts share bones. The recorder must then carry the last uploaded values forward, as VU
memory does.

Caveats:

1. Morph targets (`734.w` weight, keys at TOP+1+6n) also move vertices. For exact vectors the
   previous weight has to be kept per key as well.
2. `mem[1017].z` mirroring and the camera-relative `model+0x180` path (render_status 0x8000)
   are already part of M962 or applied per vertex, so they need no special handling.
3. Objects drawn through `xgl_*` builders (UI, fonts, 2D) never use VU1 geometry and get
   zero motion.
4. A model that appears for the first time, or whose key is new, has no history. Use a zero
   vector, the usual DLSS "reset" for that draw.

## Next

* Native renderer: [NATIVE_RENDERER.md](NATIVE_RENDERER.md) (set-0 strip slice done).
* Shaders: `skin_lit` and `strip` cover 2/3 of the captured vertices; test them through
  `test_vu1_ref --tol`.
* Sets 1 and 3–6 are still unseen. With the standard pad script the game reaches the
  hangar cutscene (vsync ~1300–1500) and then the *Virtual Tutorial* field (from ~1550 on;
  vsync 6001 is the same room with the minimap). Both use only sets 0 and 2 (two `MICR`
  images per frame). Reaching the projection (3), additive (4, 5) or drop/tex-env (1, 6)
  families needs a scene with those materials, past the tutorial; no quick pad route was
  found within this pass.

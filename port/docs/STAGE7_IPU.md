# PC port, Stage 7: the IPU (logo movies), and where the other "IPU" images really come from

> **Settings (2026-11-07):** the port no longer reads `XSG_*` environment variables. Every
> `XSG_FOO=v` in this document is now `[section] key = v` in settings.ini (the mapping is the
> "replaces" column of [MENU.md, settings.ini reference](MENU.md#settingsini-reference)); run
> with `XSG_CONFIG=<file> port/build/xsg`. Samples: [examples/](examples/).

Branch `pc-port`, 2026-11-07. Builds on [STAGE6_VU1.md](STAGE6_VU1.md).

**Result:** the namco and logo_msi logo movies (`namco.ipu`, `logo_msi.ipu`) now decode
through a clean-room IPU model and play to the end at the game's own pace (one picture per
two vblanks, 121 + 228 pictures, no bitstream errors). The game then reaches the title as
before. Two parts of the original goal turned out **not** to be IPU work:

* **Title background:** this is `xglJpegDecode` (src/main/xgl_jpeg.c), a *software* JPEG
  decoder that uses VU0 inline asm for the IDCT and the colour conversion. It does not use the
  IPU. The buffer was zero because that asm was a no-op on the host. The VU0 translation
  (1358896, STAGE7_VU0.md) fixes it, and the background now decodes (see screenshots).
* **.pss movies** (opening `1001_1.pss`, `prologue`, `10op`): these go through libmpeg
  (`sceMpeg*`). They are still stubbed (they end at once). See "Next" below for the reasons.

## 1. How the game drives the IPU

Only `xglMoviePlay` (src/main/xgl_1.c) talks to the IPU directly. libmpeg would too, but the
port replaces libmpeg. An `.ipu` file is `"ipum"`, a size, u16 width/height/frame count, and
then per picture:

```
[flag byte v][IDEC macroblocks ...][byte-aligned 0x000001b0]
```

For each picture: CTRL=RST, `SETTH` (bca/bcb), `BCLR bp` (bp is kept from the last frame).
D4 is restarted at the rewound address. Then `FDEC 0` peeks v and `FDEC 8` consumes it.
`CTRL = (v & 0xfb) << 16` sets IDP/AS/IVF/QST/MP1 and `IDEC (DTD = v bit 2, QSC 1)`.
One `fromIPU` DMA (ch3, normal mode) then runs per 16-line strip into a VRAM staging buffer,
each followed by a GS upload (`make_gstrans`). The picture ends with `FDEC 32`, which skips the
end code. The game then stops D4 and uses `IPU_BP` (BP, IFC, FP) to rewind `D4_MADR` by the
qwords still in the FIFO. In stream mode, `checkdma` chains the two halves of the CD ring by
restarting ch4 with a fake `REF` tag in CHCR and patching `D4_TADR`.

## 2. Design (port/runtime/ipu.cpp, written for the port, MIT)

* **Register access:** the EE register page is plain memory. `xgl_1.c` calls
  `IPU_WRITTEN(reg)` after every write to IPU_CMD, IPU_CTRL, D3_CHCR and D4_CHCR. On the PS2
  build this expands to `((void)0)`, so the gate is byte-identical. `sceIpuSync` (sdk.c) asks
  `xsg_ipu_busy()`. The DMAC stand-in thread (host_main.c) no longer touches ch3/ch4.
* **ch4 (toIPU):** a real chain walker (refe/cnt/next/ref/refs/end, and the CHCR TAG field
  decides whether a restarted transfer ends after its QWC). Tags may sit on host thread
  stacks, so addresses are used unmasked; 0x8xxxxxxx means scratchpad. The model keeps the
  8-qword input FIFO full like the DMAC does. That keeps `D4_MADR` ahead of the decoder by
  IFC qwords, which `checkdma` and the BP rewind depend on. Without it, the second picture
  started at the wrong place.
* **Bit reader** over the FIFO. `IPU_BP` = BP | IFC<<8 (FP is always 0; the game adds
  FP+IFC).
* **Commands:** BCLR, IDEC, FDEC, SETIQ, SETVQ (consumed), SETTH. BDEC/VDEC/CSC/PACK are
  logged as not implemented (the game never issues them outside libmpeg).
* **IDEC:** MPEG-2 intra macroblocks. Macroblock type (intra / intra+quant), dct_type when
  DTD is set, 5-bit quantiser code, DC size tables B.12/B.13 with IDP precision, AC tables
  B.14/B.15 (IVF) transcribed from ISO/IEC 13818-2 into a 16-bit lookup (checked for
  prefix-freedom and Kraft completeness), MPEG-2 escape (MPEG-1 escape under MP1), zigzag or
  alternate scan (AS), linear or non-linear qscale (QST), the default intra matrix (that is
  what sceIpuInit loads), saturation and mismatch control, a float IDCT, frame or field DCT
  placement, 4:2:0 chroma by replication, and CSC to RGBA32. The SETTH alpha rule: all of RGB
  below TH0 gives A=0 and black; below TH1 gives A=0x40; otherwise A=0x80. RGB16 (OFM) is
  supported but unused.
  After each macroblock, a macroblock_address_increment of `1` continues, and stuffing or
  escape is skipped. 16 zero bits (a start code) end the picture. The IPU then stops **byte
  aligned on the start code**, which is what the game's `FDEC 32` expects.
* **Suspension:** macroblocks are decoded one at a time into an output queue that is drained
  into ch3 whenever ch3 runs (the game starts ch3 for strip 0 *before* issuing IDEC). If the
  input runs dry mid-macroblock, the bit position, DC predictors and qscale roll back, and
  decoding resumes on the next ch4 start. FDEC/SETIQ wait in the same way.
* **Logging:** `XSG_IPU_LOG=1` logs every command, ch4 start and tag, and "IDEC done, N MBs"
  or bitstream errors per picture.

## 3. Results and performance

`XSG_FAST=1`, after e74f7c3: namco starts at vblank 44, and logo_msi ends at about vblank 750.
All 349 pictures give `IDEC done, 896 MBs` (32×28 MBs = 512×448). The title follows (menu
over the star field at vblank 1100). With 60 Hz pacing that is 121 pictures in about 4 s and
228 in about 7.6 s, 30 pictures per second, as on the PS2. Decoding a 512×448 picture takes
a few ms with the reference float IDCT. The whole boot up to the title at vblank 3000 ran in
25 s unthrottled, so decoding is not a bottleneck.

### Screenshots (port/docs/screenshots/, git-ignored)

| file | what it shows |
|---|---|
| `stage7_ipu_namco_140.png` | vblank 140: "Produced by NAMCO" logo, full frame, clean (e74f7c3) |
| `stage7_ipu_logo_msi_400.png` | vblank 400: logo_msi movie, blue/teal clouds with light rays, no block artefacts (e74f7c3) |
| `stage7_ipu_logo_msi_550.png` | vblank 550: later logo_msi picture, light burst (e74f7c3) |
| `stage7_ipu_title_after_movies_1100.png` | vblank 1100: title menu after both movies (pre-VU0, so no background) |
| `stage7_title_bg_head_1000_cut192.png` | 9a2a96a (with VU0 translation): the title **background now decodes** (Xenosaga logo, sky); **but everything below y=192 is black** |
| `stage7_namco_head_140_cut192.png` | 9a2a96a: the same y≥192 loss on the IPU movie, so this is a regression outside the decoder |

## 4. Licence notes

ipu.cpp is new code for this port. The VLC tables, scan orders, default matrix and
non-linear qscale table are facts from ISO/IEC 13818-2 annex B and section 7, transcribed
by hand. No FFmpeg/libmpeg2/PCSX2 code was used or vendored. Nothing new is linked.

## 5. Next / blockers

1. **Regression at 9a2a96a: frame content at y ≥ 192 is lost** (black, with leftover
   stripes) on both the IPU movie and the JPEG title. At e74f7c3 the same movies were complete.
   It came in with the VU0 commits (6e4bd08..9ff9a4e). That points to the GS upload/display
   path, or to a VU0-driven primitive drawn over the lower screen, not to the decoder.
   Bisect with `XSG_SHOT_VRAM=1` to see whether the uploads reach VRAM.
2. **.pss movies (libmpeg).** Two problems:
   * **Disc mapping:** the opening `1001_1.pss` is started at LSN 709078 (from the
     XENOSAGA.10 index: 700326 + 8752). That layer-0 sector is not a program stream, but
     layer-1 LSN 2077312 + 709078 holds `00 00 01 BA` packs. So archive entries need the
     layer-1 base, at least for the movie archives. This is disc.c / xgl_cd (the file-lookup
     owner's area), so it is not changed here.
   * **Decoder:** this system has only a 64-bit libavcodec (`pkg-config` 63.1.102,
     /usr/lib). There is no lib32 build, so the `-m32` executable cannot link it. Options:
     (a) a 64-bit helper process (`ffmpeg -f mpegvideo -i pipe: -f rawvideo -pix_fmt rgba
     pipe:`) fed with the elementary stream that the game's own `nodataCallback` programs
     into D4_MADR/QWC. sceMpegDemuxPss would demux packs and call the registered
     video/audio callbacks; sceMpegGetPicture would write 0x400-byte RGBA macroblocks in
     the order `setLoadImageTags` uploads them (MB column-major: outer loop over X).
     Pacing would come from `sceGsSyncV` 60/fps times per picture, because
     `GameMpeg2Play`'s loop does not sleep. (b) Extend this IPU model with BDEC/VDEC and
     write a small libmpeg on top (P/B motion compensation is the bulk of the work).
3. Audio: PSS audio (`audioCallback` → SSD VAG stream) is still not played.

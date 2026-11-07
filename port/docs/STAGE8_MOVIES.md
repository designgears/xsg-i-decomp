# PC port, Stage 8: the .pss movies (libmpeg through an external ffmpeg)

Branch `pc-port`, 2026-11-07. Builds on [STAGE7_IPU.md](STAGE7_IPU.md).

**Result:** with `XSG_PAD=3100:0x0800 XSG_FAST=1` (START on the title, New Game), the
prologue (`prologue.pss`, 8104 pictures), the opening (`10OP.pss`, 8221) and `1001_1.pss`
(1477) play from start to finish with video, at 2.00 vblanks per picture (29.97 fps). Their
ADPCM audio is decoded and goes to ALSA. The game then carries on into the SCE01001 scene
("Morning, KOS-MOS.").

## 1. Why the movies never started (it was not the disc)

* **No sector offset bug.** The disc is addressed linearly, the way the PS2's sceCdRead
  expects for this PTP dual-layer image. An LSN below 2077312 is on layer 0. Layer 1 starts
  at LSN 2077312 (layer 0's volume size, 2077328, minus 16). `sceCdLayerSearchFile(..., 1)`
  returns the absolute LSN (layer-relative + 2077312), and the archive bases inherit it:
  `.20` = 2077583. Every movie lands on a pack header (`00 00 01 BA`):
  prologue 2077583+1951019 = 4028602, 10OP 2077583+2003748 = 4081331, and 1001_1 in `.10`
  700326+13553 = 713879.
  LSN 709078 (700326+8752) is **not** `1001_1.pss`. It is `sound\vda\s191000.vds`, the
  scene voice stream that `Sound.streamPlay` starts right after the movie. It starts with
  SPU-ADPCM frames (16 zero bytes, then `0c 00 ...`), which is valid data. The pack at
  2077312+709078 is a coincidence inside `XENOSAGA.22`. disc.c needed no change. It only
  logs every `sceCdStStart` now.
* **The real bug was an argument-count mismatch** (the STAGE7_EVENTS class): game_movie.c
  calls `xglMpeg2Open(&mpeg, name)`, but xgl_1.c defines `xglMpeg2Open(info)` and calls
  `xglCdStreamOpen(info)`, which takes `(stream, path)`. On the EE, `$a1` still holds the
  name. On x86 the path was garbage, so the open failed and every movie was skipped.
  Fixed under `#ifdef PORT_HOST` in src/main/xgl_1.c.

## 2. Video design (port/runtime/mpeg.c)

I chose an external process over a full IPU MPEG-2 model (VDEC/BDEC/MC P/B prediction plus
a libmpeg state machine). The helper is far less code and is correct by construction. It
also keeps the game's own player (`xglMpeg2Open/Play`, its callbacks and rings) untouched.

* `sceMpegCreate` spawns `ffmpeg -f mpegvideo -i pipe:0 -f rawvideo -pix_fmt rgba pipe:1`
  (`posix_spawnp`, with `-probesize 32 -analyzeduration 0 -fflags nobuffer -threads 1` for
  low latency). `XSG_FFMPEG` overrides the binary, and `XSG_MPEG=0` restores "movies end at
  once". A reader thread drains stdout into a byte queue.
* `sceMpegDemuxPss` parses whole PS packs and PES packets. 0xE0 goes to the video callback
  (type 0) and 0xBD to the audio callback (type 3), with `{type, mp, data, len}`. If a packet
  is cut off, or a callback refuses it because its ring is full, the function stops and
  returns the bytes consumed, as libmpeg does.
* `sceMpegGetPicture` copies libmpeg's IPU-feeding loop. It calls the game's nodata
  callback, which programs D4 MADR/QWC/CHCR with the next slice of its ES ring. It then
  takes that slice (clearing CHCR.STR) and writes it to ffmpeg. While mpeg.c is active,
  ipu.cpp ignores D4 writes (`xsg_mpeg_active`). A picture is written as RGBA (A=0x80) in
  the order `setLoadImageTags` uploads it: 0x400 bytes per macroblock, macroblock columns
  outermost.
* An ES scanner reads the sequence header (size, frame rate) and the picture types. It
  reorders the types to display order (a B picture shows at once, an I/P picture when the
  next I/P picture arrives), so `flags & 7` is right for the game's I-picture checks. It
  also finds `sequence_end_code`: ffmpeg's stdin is then closed and the last pictures are
  drained, and `sceMpegIsEnd` returns 1 once ffmpeg reaches EOF.
* **Pacing:** picture *n* is held until `first_vblank + n*60*den/num`, using `sceGsSyncV`.
  This gives 2.002 vblanks per picture at 29.97 fps, measured as 8104 pictures in 16226
  vblanks.

## 3. Audio (port/runtime/audio.c, the RSSD VAG stream in iop.c)

The movie audio is SPU ADPCM, according to the PSS `SShd` header: type 0x10, 48000 Hz, 2
channels, interleave 0x400. The game passes 0x1000-byte blocks through
`SsdSetVagStreamDataStereo` (RSSD 0xb5). The IOP model used to answer stream status 0xb7
with 0. The game read that as "no free blocks", so its sound ring filled up and demux
stalled, which showed as "A seek error has occurred". The model now:

* keeps a 4-block queue that drains at `48000*pitch/0x1000` Hz, timed in vblanks so that
  `XSG_FAST` stays consistent, and answers 0xb7 with `free_blocks << 8`;
* decodes each block (L/R runs of 0x400, the standard 5-filter SPU ADPCM) and writes it to
  ALSA, using the 32-bit `libasound.so.2` through `dlopen`, non-blocking. If the device is
  full, samples are dropped. `XSG_AUDIO=0` keeps the timing but turns sound off.

The scene voice streams (`.vds`) use the same commands, so they are now fed and timed too.
Their interleave is assumed to be the same, but I have not checked it. I also have not
listened to the audio. I only checked it: L/R correlation is 0.41 at interleave 0x400 and
below 0 at 0x800/0x1000 on prologue.pss, and the ALSA device opens.

## 4. Licence notes

FFmpeg runs as a separate executable over pipes, so nothing from FFmpeg is linked, vendored
or distributed with the port. Users bring their own `ffmpeg`. libasound is loaded with
`dlopen` at run time (LGPL, not linked). The PSS demuxer, ES scanner and ADPCM decoder are
new code written from the public format descriptions (ISO/IEC 13818-1/-2 headers, SPU ADPCM
filter coefficients).

## 5. Screenshots (port/docs/screenshots/, git-ignored)

| file | what it shows |
|---|---|
| `stage8_prologue_3600.png` | prologue.pss: lake under clouds, caption "20xx A.D. Lake Turkana Kenya", clean full frame |
| `stage8_10op_25200.png` | 10OP.pss: Shion seen through a translucent holographic panel, CG lab |
| `stage8_1001_1_36900.png` | 1001_1.pss: Shion at a console, burned-in subtitle "It looks like...the bi-directional link in the V4 field is lagged, too" |
| `stage8_after_movies_41400.png` | after the movies: the SCE01001 event text "Morning, KOS-MOS." (the 3D scene is still black, a known issue from Stage 7) |

## 6. Open points

* The `.vds` interleave and real audio quality have not been checked by listening.
* `Release` builds (`-DCMAKE_BUILD_TYPE=Release`) crash at frame 16. This is not related
  to this work. Build with the default type.

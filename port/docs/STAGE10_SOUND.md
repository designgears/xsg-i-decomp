# PC port, Stage 10: sound (the game's IOP sound driver on a software SPU2)

Branch `pc-port`. Builds on [STAGE4_BOOT.md](STAGE4_BOOT.md) (the RSSD RPC protocol and the
old fake IOP), [STAGE8_MOVIES.md](STAGE8_MOVIES.md) (the movie audio stream and the ALSA
loader) and [STAGE7_EVENTS.md](STAGE7_EVENTS.md) (the scene voice/BGM stream).

**Result:** the game's own sound driver now runs on the host. Sound effects, the movie
soundtrack and the scene voice/BGM stream are rendered by a software SPU2 and played through
ALSA, and can be recorded with `XSG_WAV`. Measured below: the title-menu SFX plays at the
pitch its source sample and pitch register predict (8 of 8 key-ons within 1 %), the
270 s prologue movie's audio runs in step with its pictures, and the SCE01001 cutscene
stream (which was opened but never read before) plays continuously with no clipping.
**Sequenced music** (field BGM of the Virtual Tutorial, RSSD 0x31 add + 0x34 start) now plays
and is checked in §7; a reverb work-area bug that put a stuck tone over it was fixed there.

## New environment variables

| variable | meaning |
|---|---|
| `XSG_SOUND=0` | do not run the driver; fall back to the Stage 4 model (acknowledgements, Stage 8 movie-stream decoder) |
| `XSG_WAV=path` | record the final mix (48 kHz, stereo, s16 WAV). The header is refreshed every ~0.5 s, so a killed run is still readable |
| `XSG_AUDIO=0` | no ALSA output (the driver still runs; useful with `XSG_WAV`) |
| `XSG_SOUND_LOG=1` | log every EE → IOP command (with audio time), the driver's own printf, and a status line every 5 s |
| `XSG_SOUND_LOG=2` | also dump every active voice (phase, envelope, pitch, ADSR, volume, addresses, NOISE/PMOD/wet) every 5 s |
| `XSG_SOUND_CLOCK=real` | run the IOP on wall-clock time instead of the game clock (see §4) |
| `XSG_SPU_TRACE=dir` | at each key-on (first 256), dump the sample being played to `dir/kNNN_cC_vVV_pPITCH_aADDR.adpcm` and log the time |

## 1. The sound driver

The IOP modules are `SSD.IRX` ("PS2_SuzukiSoundDriver" 2.5, 442 functions) and `RSSD.IRX`
(its SIF RPC front end, 53 functions). Both are fully decompiled C in `src/audio/ssd` and
`src/audio/rssd`. The C builds the original modules byte for byte (config/iop-build.json).
Their tables are not recovered as C.

**EE → IOP** (server 0x900, `RssdExecFunc`, one 32-byte packet `{s16 command, s16 error,
int param, int value, int data_size, int arg[4]}` + data; command 4 is a batch). These are
the commands the game sends:

| group | commands | driver function |
|---|---|---|
| init / output | 0x01 init (`SsdInit`: SPU memory manager, DMA, voices, 2 ms timer, reverb), 0x05/0x06 resume/suspend, 0x10 output mode, 0x12/0x13 SPU/CD volume, 0x14 reverb | `RssdInit`, `SsdSetReverbParameter` |
| wave banks (`swdm`, "SWD") | 0x20 add (first 64 KiB), 0x21 next chunk (IOP → EE 0x25 after each SPU DMA), 0x22 dispose, 0x24 check, 0x2c/0x2d segment alloc mode | `SsdAddWaveData`: ADPCM to SPU2 RAM by `sceSdVoiceTrans` |
| sequences (music) | 0x30 play data, 0x31 add, 0x32 dispose, 0x33 check, 0x34 start, 0x35 resume, 0x36 stop, 0x37 pause, 0x40 status, 0x41–0x4c tempo/volume/note/pan/mute/faders/output volume, 0x50/0x51 patches, 0x58 signal | `ssd_seq.c`, `ssd_main.c` |
| effects ("SED", SE sequences) | 0x38 add seq-effect, 0x60 add effect data, 0x61/0x62 dispose/check, 0x70 play, 0x71 play with params, 0x78 stop all, 0x79/0x7a/0x7e/0x7f fade out, 0x7b/0x7c params, 0x7d continue, 0x80–0x82 is-playing | `ssd_sec.c`, the effect sequence |
| direct waves | 0x90–0x92 | `SsdPlayWave*` |
| streams | 0xa0–0xa8 PCM stream (AutoDMA), 0xb0–0xbe VAG streams: init stereo/mono, play, stop, data (0x1000-byte blocks), status (`value >> 8` = free blocks), pitch/volume/pan | `rssdstrm.c` ring → `ssd_vag.c` |
| sampling, debug | 0xd0–0xd7, 0x100 SPU memory, 0x102/0x103 envelopes / play info, 0x2xx/0x3xx debug, 0x400 key off all | |

**The sequence format.** Each part (track) is a byte stream. Bytes below 0x80 are notes: the
low nibble plus the part's octave gives the key, the rest is velocity or length. Bytes from
0x80 up index a 128-entry command table (`SsdSeqFuncTrap`, 95 handlers in `ssd_trap.c`:
tempo, program, volume, pan, octave, loops, LFOs, ...). `SsdSeqFuncLength` gives each
command's length, used when skipping. The 2 ms timer (`SsdMainInterruptProcess` →
`SsdSeqInterruptProcess`) advances every sequence, the effect sequence (sound effects are
sequences too), the LFOs and the fades, then writes the changed voice registers through libsd
(`ssd_voic.c`).

**Voice streams** (movie audio and the scenes' voice/BGM `VAG` streams, `.vds`-style data
read by the EE from the disc) go EE → IOP ring (4 × 0x1000-byte blocks of stereo SPU ADPCM,
interleaved in 0x400-byte runs) → the IOP copies half-buffers by SPU DMA into a
2 × 1 KiB-per-channel double buffer at SPU address 0x6000/0x6800, which two voices loop over.
The SPU2 **IRQ** (core 1, IRQA at the far half) tells the driver which half has been played.

### Why the original code and not a reimplementation or an R3000 interpreter

All of the driver is plain C that matches the original, so compiling it for i386 gives the
exact driver behaviour for free, sequencer included. The only per-target work is the data
(below) and the IOP services it imports. An interpreter (e.g. ps2-port-engine's `iop_vm.cpp`)
would run the same code more slowly and would still need the same kernel/libsd/SIF services.

## 2. Host build of the driver (CMake, `port/scripts/gen_iop_sound_data.py`)

* `src/audio/ssd/*.c` and `src/audio/rssd/*.c` compile with `-m32 -std=gnu89 -fno-pic`, and
  `-D_fdata=ssd_fdata` / `rssd_fdata` (the code prints strings at offsets of the module's
  `_fdata`).
* `gen_iop_sound_data.py` reads the user's `SSD.IRX`/`RSSD.IRX` and emits each module's
  `.rodata` + `.data` + `.bss` image as i386 assembly. Every word that `.rel.data` relocates
  becomes a `.long function+off` (for text targets) or `.long label+off` (for data). The
  objects the C names get weak labels: IRX globals (`SsdWork`, `SsdAllPitchTable`,
  `SsdVolumeTable`, `SsdSeqFuncTrap`, `RssdWork`, ...). Four tables have no symbol and were
  found from the HI16/LO16 pairs of the functions that use them: `WaveformTable` 0xe940
  (`SsdInitLfoParam`), `SsdSeqFuncLength` 0xefc0 (`SsdEffectTrackSequence`),
  `SpuReverbWorkSize` 0xec20 (`SsdInit` reads `[4]` at 0xec30), `SsdSpuMemTypeTextTable`
  0xebec (`SsdCheckSpuMemory`). Nothing from the disc is committed: the data is generated at
  build time.
* `ld -r` of all of it, then `objcopy --prefix-symbols=ssdiop_` gives `iop_sound_driver.o`.
  Every driver symbol, and every import, is `ssdiop_*`, so the driver's `SsdInit`,
  `WaitSema`, `sceSifCallRpc`... cannot clash with the EE's. The struct layouts are the IOP's,
  because the IOP and i386 are both 32-bit little-endian and the work structures contain no
  8-byte types.

## 3. The IOP services (`port/runtime/iop_sound.c`)

* **Threads:** each IOP thread is a host pthread, and they all share one **IOP lock**. A
  thread holds the lock while it runs and releases it only while blocked (`SleepThread`,
  `WaitSema`, `sceSifRpcLoop`), so at most one runs at a time, as on the single IOP CPU.
  `CpuDisableIntr` is a no-op because nothing else runs while a thread holds the lock.
  Semaphores and `Wakeup`/`iWakeup` are counters plus condition variables.
* **SIF:** loading `RSSD.IRX` runs RSSD's `start()` (server and client threads). The EE's
  calls to server 0x900 run `RssdSifRpcServer` on the EE thread under the IOP lock (iop.c →
  `xsg_sound_rpc`). The driver's IOP → EE calls (0x1000 ready, 0x25 wave chunk done, 0x0a
  stream end, ...) go into iop.c's queue. The queue is now locked and holds up to
  64 KiB per entry. The EE's `sceSifRpcLoop` thread is woken from the EE side, after each
  call and at every vblank (`xsg_iop_poll`).
* **libsd** on `spu2.c`: `sceSdInit` (mixer defaults: MMIX core 0 0xff0, core 1 0xffc),
  Set/GetParam, Switch, Addr, CoreAttr, Set/GetEffectAttr (reverb presets),
  ClearEffectWorkArea, VoiceTrans (copies at once; the transfer-complete interrupt fires on the
  next 2 ms tick), VoiceTransStatus, BlockTrans/BlockTransStatus (AutoDMA),
  SetTransIntrHandler, SetSpu2IntrHandler.
* **Hard timer:** `SetTimerHandler` + `StartHardTimer` give the driver's 2 ms tick (73728
  sysclocks; the handler's return value sets the next interval).
* **The IOP clock (audio thread):** every 2 ms it fires pending DMA interrupts, renders the
  SPU2 sample by sample up to each timer expiry, runs the timer handler, and outputs. This is
  where the driver's interrupt work runs: sequencer ticks, SPU2 IRQs and DMA completions.

Two EE spin loops needed `PORT_HOST` help. On a PS2 an interrupt preempts them, but the host's
EE threads are cooperative coroutines:
* `SsdSpuDmaCompleted(0)` (src/main/ssd_1.c), which `xglSoundSendEffect` spins on while a bank
  uploads. It now calls `xsg_iop_yield()`, which wakes the RPC thread with preemption when the
  IOP has called.
* `xglRenderSyncMove`'s `while (VSyncCount < 2)` (src/main/xgl_render.c). It spins forever
  if `sceGsSyncV` returns on the first even field. It now waits for a vblank inside the loop.
  This was a latent issue; the driver's different boot timing exposed it.

## 4. Software SPU2 (`port/runtime/spu2.c`)

Clean-room C, MIT. The hardware tables (Gaussian interpolation table, reverb presets) are
ico-pc's `spu2_tables.c` (MIT; notice kept; see `port/third_party/THIRD_PARTY.md`). What it
models:

* 2 cores × 24 voices, 2 MB RAM (halfword-addressed internally, byte addresses at the libsd
  interface);
* PS-ADPCM with filters 0–4 and the loop flags: start (sets LSA), end (ENDX, jump to LSA),
  and repeat (without it, the voice stops);
* pitch (0x1000 = 48 kHz, up to 0x3fff), pitch modulation (PMON), and Gaussian 4-point
  interpolation;
* ADSR (attack/decay/sustain/release, linear/exponential, psx-spx rate rules), volume sweeps,
  and noise (NON, noise clock);
* VMIXL/R dry and VMIXEL/R wet buses, MMIX gates, core 0's output into core 1's external
  input (AVOL), AutoDMA input (BVOL);
* reverb: the full psx-spx formula (same/different-side reflections with vIIR and vWALL, four
  combs, two all-pass stages, the intermediate clamps) on the core's work area
  (ESA = EEA − size + 1; EEA keeps only A20..A17, so it ends a 128 KiB page) at 24 kHz, offsets
  ×4 for halfword addressing, then EVOL. The work area is written only while effect enable is
  set. 48 → 24 kHz and back go through the 39-tap half-band FIR (zero-stuffed, gain 2 on the
  way up). The presets are libsd's (`spu2_libsd_presets`, from ps2-port-engine, MIT; they match
  the table in the game's LIBSD.IRX and differ from psx-spx's PS1 examples). For echo (7) and
  delay (8), `sceSdSetEffectAttr`'s delay and feedback (0–127) rewrite the registers as libsd
  does: d = (delay+1)·32, vWALL = feedback·258, mLSAME = 2d − dAPF1, mRSAME = d − dAPF2,
  dLSAME = dRSAME + d, mLCOMB1 = mRCOMB1 + d, mL/RAPF1 = mL/RAPF2 + d (delay 127 gives the preset);
* MVOL, mute, and the IRQ on voice block fetch and DMA reads/writes at IRQA (latched until
  IRQ enable is rewritten).

`port/tests/test_spu2.c` (target `test_spu2`) encodes a 1 kHz sine to ADPCM and checks: pitch
0x1000 / 0x800 / 0x2000 → 1000.0 / 500.0 / 2000.0 Hz, the attack reaches full level (RMS
11229, ideal 11314), the loop and ENDX, half volume → 0.500 of the level, key off → silence,
IRQ fires once, the hall reverb leaves a tail with no clipping, delay mode with delay 127
reproduces libsd's registers, echo delay 63 / feedback 64 gives mLSAME 0x0ffd and vWALL 0x4080,
the delay-63 echo starts at 0.3412 s (predicted (0x800 − 6)·4 steps at 24 kHz + 2·19 FIR
samples = 0.3411 s) with nothing before it, and a 1 kHz tone through the FIR and the delay
line comes out at 1.000 of the dry level. Cost: 10 s of audio with all 48
voices and reverb on both cores takes 0.33 s of CPU (3.3 % of a core).

**Clock and output.** The EE starts sounds and feeds streams once per game frame, so the IOP
clock must not run ahead of the game. By default it renders at most `(vblanks + 2) × 800`
samples, and never more than real time. With the game at full speed (60 vblanks/s) this is
real time. When the game runs slower, as headless cutscenes do here (~30 % of real time,
the same with sound off), the sound slows down with it at the correct pitch, instead of the
streams underflowing. Before this gate, the cutscene stream got 9.2 data blocks/s against
13.3 needed. After it, the feed is 1 block per 2 IRQs, which is exactly the driver's cadence.
ALSA (`audio.c`'s dlopen loader, now shared: `xsg_alsa_open/write/avail`) is opened
non-blocking with a 250 ms buffer. A full device drops samples, an xrun is recovered, and the
render amount is nudged ±8 frames per tick to keep the queue near a target against the device's
clock drift. The game never waits on audio.

**Live output when the game is slow.** Because the clock is gated, a slow game would run the
device dry (xruns: hard gaps with clicks). Now, when the gate held the IOP back and less than
20 ms is queued, the gap is filled with the last output sample decaying to zero (×0.98 per
sample, ~1 ms), and the next real audio ramps in over 2 ms. Each fill raises the target queue
by 20 ms (60 → at most 200 ms) and it relaxes by 1 ms per second, so a game that stutters
gets a bigger cushion and one at full speed returns to low latency. Only the device sees the
filler; `XSG_WAV` stays in game time. Measured on the headless KOS-MOS cutscene (~40 % speed,
`XSG_AUDIO=1`, 60 s): **0 underruns** (before: an xrun at nearly every frame), 408 filled
gaps, 1 drop of 800 frames at start-up. The status line reports the filler.

## 5. Verification without ears

Tools: `port/tools/wav_report.py` (per-window RMS L/R, peak, clipped fraction, the 3 strongest
spectral peaks; needs numpy, installed in `.venv`) and `port/tools/sfx_pitch_check.py` (with
`XSG_SPU_TRACE`: decodes each key-on's source sample, predicts the frequency from the pitch
register, and looks for it in the recording at the key-on time).

Recipe: `XSG_GS=gpu XSG_GPU_HEADLESS=1 XSG_PAD='300:START,1100:START,1200:CROSS,1500:CIRCLE*400@60'
XSG_AUDIO=0 XSG_SOUND_LOG=1 XSG_WAV=/tmp/x.wav`, with `XSG_MPEG=0` or `1`.

**Movies off** (60 s run with `XSG_SPU_TRACE` for the pitch check, and a 420 s run that gave
159 s of game-time audio):
* 0–5 s: silent. Boot, two bank uploads (`swdm`, 97 KiB and 94 KiB), effect data.
* 5.05 s: START on the title → SE 0x70 → 4 voices key on the same sample at pitch
  0x0ec4/0x0eb3/0x0ed5/0x0ec4 (a detuned chorus). 1 s window RMS −23 dBFS, peak −10 dBFS,
  then a decay over ~3 s (−38 → −66 → −84 dBFS). **Pitch:** the source sample decodes to
  863.0 Hz at 48 kHz. The prediction is 792.9–800.0 Hz and the recording peaks at
  792.0–794.4 Hz. `sfx_pitch_check`: **8/8 key-ons match** (both presses of the SE).
* 18.4 s: New Game → overlay 1 → bank upload, movies skipped, and at 19.5 s the SCE01001
  voice/BGM stream (lsn 709078) starts: 0xb0 init, 0xb3 play, two stream voices at 0x6000
  and 0x6800, pitch 0x1000.
* 19.5–158.9 s (the end of the run; still in the scene): continuous. 10 s window RMS −20.7 to
  −26.1 dBFS, peak −2.1 dBFS, **no window with clipped samples, 0 dropouts ≥ 10 ms**. The
  dominant frequencies are mostly 200–300 Hz (the range of a voice fundamental), with
  ~80 Hz and 440–545 Hz components. I cannot separate voice from music by numbers alone. Feed:
  1919 data blocks for 3831 IRQs (one 0x1000 block per two 1 KiB half-buffers, as the driver
  consumes them).
* Before the game-clock gate (§4), the same scene ran the IOP on wall time while the game
  ran at ~30 %. The stream got 9.2 blocks/s against 13.3 needed, and the recording had **440
  dropouts of ~75 ms** (cleared half-buffers). With the gate there are none.

**Movies on, 300 s run (game at full speed, 17 400 vblanks):**
* 18.42 s → 288.80 s: the prologue movie's soundtrack through the driver's VAG stream:
  3765 data blocks, 7531 IRQs. 270.4 s of audio for 8104 pictures at 29.97 fps (270.4 s).
* 10 s window RMS −12 to −28 dBFS, peak −0.6 dBFS, 30/30 windows above −60 dBFS, **0 windows
  with clipping, 0 dropouts ≥ 10 ms** between 18.5 and 288.7 s. Dominant 60–110 Hz in the loud orchestral section (140–200 s,
  −12 dBFS RMS), 200–1300 Hz elsewhere.
* At 288.8 s the next movie's stream starts and plays.

**Title left idle (no input), 50 s:** only the boot uploads (0x01, 0x20/0x21, 0x60). The
game starts no music or SE on the title, so the WAV is digitally silent. That is the game's
choice, not a driver gap: no command arrives.

**ALSA:** with `XSG_AUDIO=1` the device opens (48 kHz, 100 ms). In a 40 s movie run, 800
frames were dropped once at start-up and there were no stalls. The game's frame rate with the
driver equals that without it (1937 vs 1868 vblanks in 60 s).

## 6. Gaps

1. **Music not compared with PCSX2 yet.** §7 checks the music by numbers only; a PCSX2 audio
   dump of the Virtual Tutorial (`.work/ref/`, STAGE10_COMPARE) was not recorded in this pass.
   The title has no music (only the SE on START).
2. Echo/delay reverb modes are implemented and unit-tested, but the game used only mode 4
   (studio C, both cores) in the scenes reached.
3. Noise (NON) and pitch modulation (PMON) are implemented but **not used** by anything
   reached (0 voices with NOISE/PMOD in all `XSG_SOUND_LOG=2` dumps of the field), so they are
   unchecked against the game. Likewise the PCM (AutoDMA) stream path (0xa0–0xa8) and the
   sampling commands (0xd0–0xd7).
4. The key-on delay, the exact ADSR timing at 48 kHz versus the PS1's 44.1 kHz rules, and
   volume-sweep corner cases follow psx-spx and are not compared with hardware.
5. When the game runs slower than real time the sound still slows with it (§4); live output
   no longer xruns but contains filled (silent) gaps. Time-stretching was not attempted.

## 7. Sequenced music (field BGM)

**Where it starts.** The script calls `xeno.Sound.sequencePlay` (`src/main/sound.c`) →
`xglSoundSequenceNormal*` → `SsdStartSequence` (RSSD **0x34**: handle, volume ≤ 0x7f, fade
time). The sequence data is uploaded before by `xglSoundSendSmd2` → `SsdAddSequenceData`
(RSSD **0x31**, the `.smd` blob, 14 860 bytes here), after the field's wave bank (0x20/0x21,
~1.1 MB) and effect data (0x60). In the field the sequence is then run by the driver's 2 ms
timer (`SsdSeqInterruptProcess`). The title has no BGM. Battle music was not reached.

**Scene:** the Virtual Tutorial, `XSG_FAST=1 XSG_SKIP_EVENTS=1` with the field recipe; the
0x31/0x34 pair arrives between vblank 1200 and 1800 (audio time 11.9–13.8 s, which is game
time because the clock is gated). Runs of 70–120 s, `XSG_SOUND_LOG=2`, `XSG_SPU_TRACE`.

**Bug found and fixed:** the first recordings had a constant, perfectly stable sine
(727.5 Hz in one run, 1149.8 Hz and 1756.5 Hz in others, ±0.25 Hz over 4 s windows) 12–18 dB
above everything else, with images at 6000 ± f. With the reverb output muted it vanished, and
the remaining peaks were musical. Cause: `sceSdSetAddr(EEA)` was masked with `| 0x1ffff`
halfwords (256 KiB), so core 0's EEA (0x1dffff) became the top of RAM, the same as core 1's,
and the two cores' studio-C reverbs ran in one work area and fed each other. EEA now ends a
128 KiB page (`| 0xffff`), and the reset values are libsd's (core 0 0x1dffff, core 1
0x1fffff). After the fix no window has a stuck tone.

**Analysis after the fix** (`wav_report.py`, `/tmp` spectrum scripts, the voice dumps):
* Level: 10 s windows −21.7 to −22.8 dBFS RMS, peak −3.6 dBFS, **no clipped samples**, no
  dropouts after the music starts (the two at 11.5/11.9 s are the cutscene stream's end).
* Notes: the strongest spectral peaks per 4 s window are 82.2, 110.0, 123.5, 146–147, 164.5,
  245.8, 293, 326.8, 439, 490, 582.6 Hz: E2 A2 B2 D3 E3 B3 D4 E4 A4 B4 D5 (E minor/dorian),
  within 1 % of equal temperament (some instruments are tuned ~15 cents flat in their sample).
  The pitch registers predict exact ET frequencies for the traced key-ons (440.0, 493.9,
  586.8, 660.4 Hz, `sfx_pitch_check` "expected" column). 22 different samples over 246
  key-ons in 25 s; drum-like samples always at the same pitch, melodic ones at whole/half-step
  offsets (e.g. −3.5, −1.5, −0.5, +1.5 semitones from 0x1000).
* Tempo: the inter-onset intervals cluster at 0.16–0.17 s (79 of 246) and 0.33 s and 0.66 s,
  i.e. sixteenths of a 0.663 s beat: **~90.5 BPM**, steady over the run.
* Polyphony: up to 21 voices active on core 0 (status lines), typically 13–19; the VAG stream
  voices (core 1, 20/21) key off when the cutscene stream stops (0xb4 then 0xb2).
* Envelopes: the dumps show attack 0x80ff (exponential, fastest), sustain levels holding at
  0x7fff for pads (ADSR2 0xdfe6/0xdfec: sustain rate 0x7f = hold) and releases decaying
  (phase 4, env 0x3057 → 0x0938 between dumps). No voice was found stuck: every sustained voice
  is retriggered by the sequence (the hard-panned pad pair at pitch 0x0aab, ssa 0x0b66d0 /
  0x0c2d20, keyed on 10 times in 25 s).
* `sfx_pitch_check` finds the predicted frequency as the window's *dominant* one for only 19
  of 254 key-ons: in a polyphonic mix the dominant peak is usually another voice, so this tool
  is not meant for music.

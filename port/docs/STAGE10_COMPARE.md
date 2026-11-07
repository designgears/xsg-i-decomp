# Stage 10: comparison against a reference emulator

> **Settings (2026-11-07):** the port no longer reads `XSG_*` environment variables. Every
> `XSG_FOO=v` in this document is now `[section] key = v` in settings.ini (the mapping is the
> "replaces" column of [MENU.md, settings.ini reference](MENU.md#settingsini-reference)); run
> with `XSG_CONFIG=<file> port/build/xsg`. Samples: [examples/](examples/).

**Result:** PCSX2 v2.8.2 with OpenPS2BIOS 0.6 boots Xenosaga and runs it through the prologue,
the KOS-MOS scene and into the Virtual Tutorial field. It needs two small local fixes to
OpenPS2BIOS's IOP services (§1.3). Unpatched, OpenPS2BIOS stops on a black screen before the
Namco logo.

What the comparison shows:

* The two Stage 8 open issues are not port bugs:
  * **Subtitle overlap:** this comes from the `XSG_SKIP_EVENTS=1` debug switch. At normal
    script timing, vblank 1700 shows one subtitle, the same as on PCSX2.
  * **Darkness:** the scene is meant to be this dark. Mean luma matches PCSX2 within 1/255
    in every matched 3D frame.
* One real rendering difference was found and fixed: in the field, the projected drop
  shadow was wrong because a GIF packet's CLAMP word had been relocated as a pointer in the
  host data (§3.3).

## 1. Reference setup (dev tools only, outside the repo)

Everything lives under `.work/ref/` (gitignored). Nothing is linked or vendored.

### 1.1 Download

```
mkdir -p .work/ref/pcsx2 .work/ref/bios
gh release download v2.8.2 -R PCSX2/pcsx2 -p '*linux-appimage*' -D .work/ref/pcsx2
gh release download v0.6 -R TamaribaEmu/OpenPS2BIOS -D .work/ref/bios   # MIT; sha256 checked
cd .work/ref/pcsx2 && chmod +x *.AppImage && ./*.AppImage --appimage-extract
cd squashfs-root/usr/bin && touch portable.txt && mkdir -p bios inis   # portable mode: config next to the binary
```

### 1.2 Display and input

* There is no Xvfb, weston or gamescope here. The AppImage bundles only the `wayland` and
  `xcb` Qt platform plugins. `QT_QPA_PLATFORM=offscreen` aborts, and the system
  `libqoffscreen.so` is Qt 6.12, which the bundled Qt 6.11 rejects.
* What works is a **headless Hyprland output**: `hyprctl output create headless XSGREF`
  (1920×1080, workspace 11), plus a window rule that sends PCSX2 there without focus:
  `hyprctl eval 'hl.window_rule({ name = "xsgref", match = { class = "(?i).*pcsx2.*" }, workspace = "11 silent", no_initial_focus = true })'`.
* Captures use `grim -o XSGREF`. In window mode the 4:3 picture sits at (306,75)–(1613,1055)
  of the output. `-fullscreen` did not take effect.
* Input: there is a keyboard mapping in `inis/PCSX2.ini` (`[Pad1]`: Start=Return, Cross=X,
  Circle=Z, …). `wtype -P/-p` sends the keys after
  `hyprctl dispatch 'hl.dsp.focus({ window = "class:pcsx2-qt" })'`. Hotkeys: Tab toggles
  turbo, F1/F3 save/load a state.
* Minimal ini: `[UI] SettingsVersion = 1` (without it, PCSX2 shows a reset-settings dialog and
  hangs), `SetupWizardIncomplete = false`, `[Filenames] BIOS = …`, `[EmuCore] EnableFastBoot = true`.
  `[Logging] EnableEEConsole/EnableIOPConsole = true` shows OpenPS2BIOS's `[openbios:*]` trace.
* Scripts (in `.work/ref/scripts/`):
  * `run_ref.sh SECONDS [args]`: run PCSX2.
  * `drive.py TOTAL OUTDIR "t:key …" "t …"`: timed key presses and grim captures.
  * `compare.py OUT REF PORT [label]`: side-by-side image with mean RGB/luma.
  * `match.py REF PORTDIR`: find the closest port frame.
  * `grid.py`: contact sheet.
  * `make_stage10.py`: rebuild the images below.

### 1.3 Where stock OpenPS2BIOS fails, and the local patch

Stock 0.6 behaves like this:

1. It loads SLUS_204.69 and reboots the IOP with IOPRP24D.IMG. It does not load that image:
   it serves its own native services instead.
2. The game's IRXs load (sio2man, mcman, padman, libsd, ssd, rssd).
3. Then the game loops forever on
   `[openbios:iop] cdvd search not found: XENOSAGA.20;1 (request size 0000012c)`.
   The screen stays black.

The cause is that `cd_search` (`src/iop/rpc.c`) ignores the layer word of the 0x12c
`sceCdLayerSearchFile` packet and only looks in layer 0. `XENOSAGA.20` is on layer 1 (LSN
2077583, see STAGE4_BOOT). The BIOS already mounts the layer-1 volume
(`ob_iop_cd_iso_layer1`), so the fix is three lines.

With that fixed, New Game fails next with
`loadfile fno=00000001 path=cdrom0:OV01.OVL;1 -> unsupported`, followed by a TLB miss at
0xa80000. LOADFILE function 1 (`SifLoadElf`/`SifLoadElfPart`, used for the overlays) is not
implemented. The patch adds it: it reads the ELF, sends each PT_LOAD over SIF0, and replies
{epc, gp=0}.

The patch is `.work/ref/openps2bios-xsg.patch`, against tag v0.6. To build it, use the
upstream Makefile with system `clang` + `ld.lld` + `llvm-objcopy`, then copy the result as
`bios/OpenPS2BIOS-0.6-layer1.bin`. Both fixes are generic, so they are worth reporting
upstream. A user-dumped Sony BIOS would work without any patch. No Sony BIOS was downloaded.

## 2. Captures

* Reference: `drive.py 600 /tmp/ref3 "22:Return 25:x 30:Tab" "<every 5 s>"`. This selects New
  Game on the title, then runs in turbo. Turbo makes the reference's timeline non-linear, so
  frames are paired by content with `match.py`.
* Port: `XSG_GS=gpu XSG_GPU_HEADLESS=1 XSG_AUDIO=0 XSG_MPEG=0 XSG_FAST=1
  XSG_PAD='300:START,1100:START,1200:CROSS' XSG_SHOT_EVERY=100`.
  * Run once without `XSG_SKIP_EVENTS`: the cutscene at normal script timing up to vblank 11000.
  * Run once with `XSG_SKIP_EVENTS=1` to reach the Virtual Tutorial at about 1550.

## 3. Differences

Images are in `port/docs/screenshots/` (git-ignored). PCSX2 is on the left, the port on the
right. Y is mean luma over the whole picture.

| scene | image | ref Y | port Y | difference | cause |
|---|---|---|---|---|---|
| Namco logo | `stage10_cmp_namco.png` | 243.6 | 227.0 | port is grey instead of white | timing only: port vblank 200 is mid-fade (vblank 100 is 254/254/254) |
| Title | `stage10_cmp_title.png` | 208.6 | 208.9 | none visible | – |
| Prologue | (ref only, e.g. `/tmp/ref3/ref_0*.png`) | – | – | the port goes straight from New Game to KOS-MOS | `XSG_MPEG=0` ends movies at once; the Lake Turkana / Zohar / credits prologue is `prologue.pss` + `10OP.pss` (STAGE8_MOVIES). Not a bug |
| "Good morning, Shion" | `stage10_cmp_kosmos_1700.png` | 71.2 | 70.9 | none: same lighting, colours, text layout | – |
| same, `XSG_SKIP_EVENTS=1` | `stage10_cmp_kosmos_1700_skipevents.png` | 71.2 | 62.5 | "How do you feel?Good morning, Shion." on one line | debug switch, see §3.1 |
| "the emotion module…" | `stage10_cmp_emotion_6300.png` | 49.8 | 49.1 | none beyond the animation phase | – |
| "mixed feelings" | `stage10_cmp_mixed_9100.png` | 52.3 | 51.4 | none | – |
| hangar close-up | `stage10_cmp_hangar_10200.png` | 98.5 | 98.8 | none | – |
| Virtual Tutorial | `stage10_cmp_tutorial_1800.png`, zoom `stage10_tutorial_shadow_zoom.png` | 189.4 | 189.1 | port (before): translucent grey full-height "capsule" around Shion and KOS-MOS, and no soft shadow at their feet. Fixed: relocated CLAMP word in `Head_9` | §3.3, fixed |

There were no differences in fog, alpha blending, text layout or geometry in the matched
cutscene frames. The subtitle font, outline and line breaks are identical.

### 3.1 Subtitle overlap (Stage 8 open issue): the `XSG_SKIP_EVENTS` switch

`XSG_SKIP_EVENTS=1` sets `wait_parameter = 0` in `Java_xeno_vm_System_sleep__I`
(`src/main/system.c`, PORT_HOST). That also removes the script's sleeps between closing one
message and opening the next. The next text is appended to the still-open window, which gives
"How do you feel?Good morning, Shion.". Without the switch, vblank 1700 is a clean
"Good morning, Shion. / All systems are normal.", the same as PCSX2
(`stage10_cmp_kosmos_1700.png`). The font, msg_init and twsys paths are correct, and no
code change is needed. Shots meant to check text should not use `XSG_SKIP_EVENTS`.

### 3.2 Lighting (Stage 8 open issue): not darker

Every matched 3D frame is within 1/255 of PCSX2 in mean luma, with the same per-channel
means. The KOS-MOS scene really is lit this dimly. The earlier impression came from looking
at frames without a reference. Nothing to fix in nmlModelSetLight, the colour scaling or
SGsRestoreEnv.

### 3.3 Drop shadow in the field (fixed)

PCSX2 draws a soft projected shadow of Shion's feet on the floor. The port drew no floor
shadow and instead a semi-transparent grey silhouette, wider than her and standing
upright, from her feet to above her head (every shot 1550 to 1900). The soldier's circle
shadow was correct.

**Root cause: host data relocation, not VU0/VU1.** These actors use shadow type 4,
`DrawDropShadow` in act_3.c (not `nmlModelCalcDropShadow`, which is why zeroing that
matrix changed nothing). It is a destination-alpha shadow volume. Per bone segment it builds
a capsule outline and projects it to the ground plane 0.5 below the actor, then draws three passes:

1. back faces with FBMSK 0x7FFFFFFF (alpha bit 31 only), alpha 0x00, Z GREATER;
2. TEXFLUSH, then front faces textured **from the frame buffer itself** (TBP = FBP,
   TCC=1, MODULATE with alpha = shadow alpha, ATST EQUAL AREF = shadow alpha), so a
   pixel darkens only where its alpha bit 31 is still set, i.e. where the floor is inside
   the volume;
3. the back faces again with alpha 0x80 to restore the bit.

`ACT_DrawShadowBegin` sends `Head_9` first. It sets CLAMP_1 to REGION_CLAMP
0..511 × 0..447 (`0x000006FC_007FC00A`). splat symbolised that low word as
`D_007FC00A` (an act_1 .bss label), so `gen_host_data.py` emitted a relocation. The host
packet then carried `0x6FC_08817C4A`, which is MINU 708 > MAXU 511. Every texel of pass 2 was
clamped to u = 708, outside the 512-wide frame, so it read an unrelated row whose alpha was
0x80. The front faces therefore passed the alpha test everywhere: the whole capsule
was darkened instead of only its floor intersection. Both renderers drew this the same way
because the input was wrong.

Found with a per-pixel probe in the software GS: the frame alpha at the pixel was 0x00
after pass 1, but pass 2 sampled 0x80. The trace showed CLAMP = 0x6FC08816C4A, set by
the begin sprite's A+D list. The PS2 ELF bytes of `Head_9` showed the original value.

Fix: `port/scripts/gen_host_data.py` adds `Head_9` and `TestPrim_0_004A8D00`
(xgl_packet, which has the same CLAMP word) to `NO_RELOC_LABELS`, so their words are
emitted raw. No `src/` change. After the fix, both `XSG_GS=soft` and `XSG_GS=gpu` show
the soft foot shadow and no silhouette, as PCSX2 does (`stage10_tutorial_1800_after_soft.png`,
`stage10_tutorial_1800_after_gpu.png`, zoom `stage10_tutorial_shadow_zoom_after.png`:
PCSX2 left, port right).

Other data labels may have the same problem: any GIF packet word between 0x00010000 and
0x02000000 that splat turned into a `D_` symbol. To find more, grep `build/*/asm` for
`.word D_` inside `Head_*`/`Tail_*`/`TestEnv*`/packet labels.

## 4. Fixes

* None in `src/` were needed for the two Stage 8 issues (§3.1, §3.2). No `src/` files changed.
* Reference-side fixes (outside the repo): the OpenPS2BIOS layer-1 SearchFile and the
  LOADFILE ELF load (§1.3).
* §3.3: `port/scripts/gen_host_data.py` NO_RELOC_LABELS += `Head_9`, `TestPrim_0_004A8D00`.
  No `src/` change.

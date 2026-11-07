# Stage 10: comparison against a reference emulator

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
* One real rendering difference was found and is still open: in the field, the projected drop
  shadow is wrong (§3.3).

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
| Virtual Tutorial | `stage10_cmp_tutorial_1800.png`, zoom `stage10_tutorial_shadow_zoom.png` | 189.4 | 189.1 | port: translucent grey full-height "capsule" around Shion and KOS-MOS, and no soft shadow at their feet; the soldier's round shadow is correct | §3.3, open |

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

### 3.3 Drop shadow in the field (open)

PCSX2 draws a soft projected shadow of Shion's feet on the floor. The port draws no floor
shadow. Instead it draws a semi-transparent grey silhouette, slightly wider than her and
standing upright, from her feet to above her head. It is present in every shot from
1550 to 1900, so it is not a one-frame entry effect. The circle shadow (soldier) is correct.

What is known so far:

* These models use the `NML_RENDER_TEXMAP_DROP` path: `nmlModelCalcDropShadow` builds a
  flattening matrix at layout +0x180 (row 1 = (-sx/sy, 0, -sz/sy), translation y = plane +
  0.005), and the VU1 microcode family `s_aUcodeDrop` (set 1 or 6 in VU1_PROGRAMS.md) draws
  it. This VU1 set has never been exercised before. The cutscene captures use only sets 0 and 2.
* Zeroing the +0x180 matrix in `nmlModelCalcDropShadow` (temporary `XSG_DBG_DROP` hack, not
  committed) did **not** change the silhouette. So either the matrix the drop program uses
  is not that one (another copy, the `U_nmlModelCalcDropShadow` alias, or a stale VU1 memory
  slot), or the silhouette is drawn by another pass.
* A VRAM dump at 1800 (`XSG_SHOT_VRAM=1`) shows no off-screen shadow texture.

Next steps:

1. Run `XSG_VU1_CAPTURE` over vblanks 1550–1560 and check which program and entry draws the
   grey primitives, and which matrix it reads.
2. Compare the set 1/6 interpreter results with expected PS2 VU behaviour for that program's
   `mr32`/`clipw`/`fcand` paths.
3. Confirm at normal script timing. The no-skip port run did not reach the tutorial in the
   time budget.

## 4. Fixes

* None in `src/` were needed for the two Stage 8 issues (§3.1, §3.2). No `src/` files changed.
* Reference-side fixes (outside the repo): the OpenPS2BIOS layer-1 SearchFile and the
  LOADFILE ELF load (§1.3).

# In-game menu (xsg-gpu)

Branch `pc-port`, 2026-11-07. The menu is drawn by the renderer process `xsg-gpu` with
Dear ImGui over the game image, so it needs `XSG_GS=gpu` (or `both`). Its design follows
TotK Recomp's menu (MIT). The look follows Xenosaga's own UI: indigo "virtual space" panels
like the minimap, thin periwinkle and cyan outlines, corner brackets, a grid and scanline
backdrop, and the Rajdhani font (OFL). Code: `port/gpu/menu/`. Licences:
`port/third_party/THIRD_PARTY.md`.

## Controls

| action | keyboard / mouse | gamepad |
|---|---|---|
| open / close | F1; Esc opens it (unless Esc is bound in `[keyboard]`) and closes it | Back+Start together, or Guide; B closes |
| switch tab | Q / E, click a tab | LB / RB |
| move | arrows | D-pad, left stick (on past 50 %, off under 35 %: hysteresis) |
| change a row | Enter / Space / click (next value), Left / Right (step) | A (Nintendo layout: the right button), D-pad left/right |
| clear a binding cell | Delete / Backspace | X (left face button) |
| performance overlay | F10: off → fps → full | |
| fullscreen | F11 (also a row) | |

While the menu is open the game keeps running, but its pad is neutral: xsg-gpu publishes
no buttons and centred sticks. Keys are cleared when the menu opens and when it closes. The
game image is dimmed, and the menu fades and rises in over about 0.16 s.

## Tabs and options

Every row marks its default value. A row reads "AFTER RESTART" when the setting is only read
at start. A row is greyed out ("SET BY ENVIRONMENT") when an env var fixes the setting. The
description pane shows the help text, the `settings.ini` key and the env var. **Restart now**
(bottom row; marked `*` when a renderer setting changed) saves the settings. The game then
ends xsg-gpu and re-executes itself with the same command line (`gpu_client.cpp Restart`).
**Quit game** works like the window's close button.

| tab | row | settings.ini key | env | values | applies |
|---|---|---|---|---|---|
| Display | Fullscreen | `video.fullscreen` | `XSG_GPU_FULLSCREEN` | 0/1 | live |
| | Window size | `video.window_scale` | `XSG_GPU_SCALE` | 1–4 × 512×448 | live |
| | Present mode | `video.present` | `XSG_GPU_PRESENT` (`XSG_GPU_VSYNC`) | mailbox, fifo, immediate, auto | live (swapchain recreated) |
| | Aspect ratio | `video.aspect` | `XSG_GPU_ASPECT` | 4:3, 16:9, stretch | live |
| | Scaling | `video.scale_mode` | `XSG_GPU_SCALEMODE` | fit, integer | live |
| | Filter | `video.filter` | `XSG_GPU_FILTER` | linear, nearest | live |
| Renderer | Renderer | `renderer.native` | `XSG_GPU_NATIVE` | 0 paraLLEl-GS, 1 native | restart |
| | Native internal scale | `renderer.native_scale` | `XSG_GPU_NATIVE_SCALE` | 1–8 | restart |
| | Super-sampling | `renderer.ssaa` | `XSG_GPU_SSAA` | 1, 2, 4, 8, 16 | restart |
| | Super-sampled textures | `renderer.ssaa_textures` | `XSG_GPU_SSAA_TEX` | 0/1 | restart |
| | High-resolution scanout | `renderer.hires_scanout` | `XSG_GPU_HIRES` | 0/1 | restart |
| | DLSS / frame generation | — | — | disabled, "coming soon" | — |
| Game | FPS limit | `game.fps_limit` | `XSG_FRAME_US`, `XSG_FAST` | 60, 30, 120, uncapped | live (game process) |
| | Audio | `audio.enabled` | `XSG_AUDIO` | 0/1 (mute) | live |
| | Master / Music / Effects / Voice and movies | `audio.master` … `audio.voice` | — | 0–100 % | live |
| | Memory card folder | — | `XSG_MC_DIR` | read-only | — |
| Controls | Button labels | `gamepad.layout` | — | xbox, playstation, nintendo | live |
| | Stick dead zone, Trigger threshold | `gamepad.deadzone`, `gamepad.trigger_threshold` | — | 0–32000 | live |
| | Bindings (2 keys + 1 gamepad button per DS2 action), Reset | `[keyboard]`, `[gamepad]` | — | | live |
| Info | Performance overlay | `overlay.mode` | `XSG_GPU_OVERLAY` | off, fps, full | live |
| | Take screenshot | — | `XSG_SCREENSHOT_DIR` | PNG of the game frame, without the menu, to `~/Pictures/xsg/` | |
| | Version, Menu, GPU, Settings file | — | — | read-only | |
| | Active XSG_* environment | — | — | read-only list | |

The FPS limit is the game's vblank rate, and the game logic runs per vblank, so it is also
the game speed: 30 is half speed, 120 double, uncapped is fast-forward.

**Renderer settings** take effect on the next start. The game reads `[renderer]` from
settings.ini before it spawns xsg-gpu, and exports each value that the environment does not
already set (`gpu_client.cpp InjectIniSettings`). It lists the exported names in
`XSG_CFG_FROM_INI`, so the menu does not mistake them for user overrides, and a restart
drops them again.

**Volume classes** come from how the sound driver allocates SPU2 voices. Music is the
core 0 voices (the sequencer). Effects are core 1 voices 0–19. Voice is core 1 voices 20–23
(the VAG stream voices, `SsdAllocVoiceVagStream`, which carry scene voice and the movie
soundtracks) plus AutoDMA PCM. Scenes mix their background music into that same stream, so
it follows "Voice and movies". The gain curve is percent squared (50 % ≈ −12 dB), in Q15
per voice inside `spu2.c core_sample`. Master is applied to the final output and so also
affects `XSG_WAV`. Audio off zeroes the master. When `XSG_AUDIO` is set in the environment,
the env var decides the device and the menu never mutes. The Stage 8 fallback decoder
(`XSG_SOUND=0`) is not scaled.

## Game-process settings: `GpuSettings`

`port/runtime/gpu_ipc.h` (protocol version 4) adds `GpuSettings` at offset 2048 of the shm
header page, next to `GpuPad` at 1024. It is a seqlock like the pad, and holds `frame_us`
(`XSG_GPU_FRAME_US_KEEP` = leave pacing alone, 0 = uncapped), `audio_on`, `vol[4]`
(master, music, effects, voice), `restart` and `menu_open`. xsg-gpu publishes it whenever a
setting changes. The game polls it once per vblank in `kernel.c settings_poll` →
`xsg_gpu_settings_poll`. A change is applied at that vblank: the frame period (kept when
`XSG_FRAME_US`/`XSG_FAST` are set), or `xsg_sound_set_volume` → `spu2_set_mix_gains`.
All zero (no xsg-gpu, or soft GS) keeps the old behaviour.

## Config file: `~/.config/xsg/settings.ini`

Location: `$XSG_SETTINGS_INI`, else `$XDG_CONFIG_HOME/xsg/settings.ini`, else
`~/.config/xsg/settings.ini`. Precedence is **environment > settings.ini > default**.
Changes are written about 0.6 s after the last edit, and at once when the menu closes. The
file is rewritten whole: sections in a fixed order, unknown keys kept, comments not kept.

```ini
[video]
present = fifo
aspect = 4:3

[renderer]
native = 1
native_scale = 4

[game]
fps_limit = 60

[audio]
master = 80
music = 70

[overlay]
mode = fps

[keyboard]
# only differences from the default layout (STAGE10_INPUT.md §2); NONE unbinds a default
Space = CROSS
Escape = NONE

[gamepad]
deadzone = 8000
layout = playstation
south = CIRCLE
```

**Migration and backward compatibility.** When settings.ini does not exist yet,
`~/.config/xsg/input.ini` (same sections and syntax) is read and written into settings.ini,
and input.ini itself is left as it is. After that, input.ini is ignored. An explicit
`XSG_INPUT_INI=<file>` is still read, on top of settings.ini. The remapping syntax is
unchanged. When a binding is edited in the menu, the `[keyboard]`/`[gamepad]` sections are
rewritten as the difference from the defaults.

**Change:** Esc no longer maps to CROSS by default, because it opens the menu. K is still
CROSS.

## Performance overlay (F10)

* **fps:** the frame rate over the last half second, in the top-right corner.
* **full:** fps, the 1 % and 0.1 % lows over the last 2048 presents, average and maximum
  frame ms, a frame-time graph of the last 240 presents, window and image/scanout sizes,
  present mode, GPU name and VRAM, ring fill, vsyncs and presents, and with the native
  renderer the native batches per frame and frames native vs VU1+paraLLEl-GS.

## Test hooks

```
XSG_GPU_MENU_TEST=<vsync>[:<vsyncs per page>[:<W>x<H>]]   open the menu at that vsync and write
    menu_<n>_<tab>.png for each tab, then menu_5_overlay.png (full overlay), to
    XSG_GPU_MENU_TEST_DIR (default .).  Headless works; images are game frame + menu at WxH.
XSG_GPU_MENU_TEST_SET=<vsync>:<key>=<value>[,...][;...]   timed settings without input;
    special keys SAVE=1, OPEN=0|1, TAB=n, SHOT=<png path>, RESTART=1.
```

`XSG_GPU_INPUT_TEST` (STAGE10_INPUT.md) also drives the menu. Its synthetic key events go
through the menu first, both windowed and headless.

## Verification (RTX 5090, Wayland/Hyprland)

* **Screenshots** (headless, field scene at vsync 5500, 1280×960):
  `port/docs/screenshots/menu_{0_display,1_renderer,2_game,3_controls,4_info,5_overlay}.png`.
  Windowed: `menu_window_wayland.png`, after F1, Down, Down, Right sent by `XSG_GPU_INPUT_TEST`.
  Present mode changed to FIFO, and the swapchain was recreated live.
* **FPS limit reaches the game.** `[fps]` showed 60.0 vblanks/s. After `game.fps_limit=uncapped`
  (`[settings] frame 1702: vblank period 16666 us -> 0 us (uncapped)`) it rose to 74–79
  (CPU-bound). At 30 it was 30.0, and back at 60 it was 59.6–60.0.
* **Volume reaches the mix.** Recorded with `XSG_WAV`: master 100 → 25 at vblank 2400. The
  RMS per 2 s went from about 2450 to about 160, i.e. ×0.065, against the expected 0.25² = 0.0625.
* **Input is blocked while open.** F1 at 1900, then W held 1950–2050: no live mask. F1 at
  2200, then W at 2250: `[pad] vblank 2251: live 0x1000`.
* **Persistence.** The second run read `live_settings.ini: 3 entries`, and the first published
  settings were master 40 / music 70. Windowed, the run wrote `[video] present = fifo` and
  `[overlay] mode = full`.

## Present-path fix found on the way: PMODE

The window and every scanout-based image were black, also with the pre-menu binaries. The
game leaves PMODE at 0x8004 or 0, which has both read circuits off and MMOD 0. paraLLEl-GS
honours that, while the software GS and `XSG_SHOT` ignore PMODE. xsg-gpu now passes PMODE to
paraLLEl-GS as EN1 + MMOD + ALP 0xFF, i.e. circuit 1 opaque, like the software GS. It
leaves PMODE alone when only EN2 is set. `XSG_GPU_PMODE_RAW=1` restores the raw register.
The root cause is probably in the libgraph stubs (`sdk.c` labels bit 2 as MMOD, but bit 2 is
CRTMD). That was not investigated further.

## Not done / limits

* `imgui_impl_vulkan` is not used. Granite owns the device, queue and swapchain, so
  `menu_render.cpp` draws ImGui's draw lists with Granite command buffers instead.
* Not testable here: a physical gamepad (open combo, B, LB/RB and stick navigation are coded
  against SDL's gamepad state and were not exercised), gamepad rebinding capture, HiDPI
  scale > 1.
* The volume classes are a heuristic based on voice allocation; see above.
* Only Latin-1 glyphs; the menu is English only.

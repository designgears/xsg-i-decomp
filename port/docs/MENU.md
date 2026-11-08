# In-game menu (xsg-gpu)

Branch `pc-port`, 2026-11-07. The menu is drawn by the renderer process `xsg-gpu` with
Dear ImGui over the game image, so it needs `[renderer] gs = gpu` (the default) or `both`. Its design follows
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
at start. A row is greyed out, with a note, only when another setting makes it meaningless
(for example the SSAA texture rows below 2x). There is no environment layer any more:
**every setting of the port lives in settings.ini and every one can be changed here**
(see "settings.ini reference" below). The description pane shows the help text, the
`[section] key` and the XSG_* variable it replaced. **Restart now** (bottom row; marked `*`
when an after-restart setting changed) saves the settings. The game then ends xsg-gpu and
re-executes itself with the same command line (`gpu_client.cpp Restart`), which reads
settings.ini afresh. **Quit game** works like the window's close button.

Text rows (paths, pad scripts, frame lists) open a text field on Enter / A / click: Enter
keeps the text, Esc cancels, empty means the default. Tab switching and Esc-to-close are
held while a field is open.

| tab | rows | settings.ini keys | applies |
|---|---|---|---|
| Display | Fullscreen, Window size, Present mode, Aspect ratio, Scaling, Filter | `[display]` all | live (present: swapchain recreated) |
| Renderer | GS backend | `[renderer] gs` | restart |
| | Renderer (paraLLEl-GS / native), Native internal scale, Super-sampling, Super-sampled textures, High-resolution scanout | `[renderer] native, native_scale, ssaa, ssaa_textures, hires_scanout` | live |
| | DLSS / frame generation | — (disabled, "coming soon") | — |
| Game | FPS limit | `[game] fps_limit` | live (game process) |
| | Play movies, ffmpeg program | `[game] movies, ffmpeg` | restart |
| | Audio, Sound driver | `[audio] enabled` (live), `sound_driver` (restart) | |
| | Master / Music / Effects / Voice and movies | `[audio] master … voice` | live |
| | Memory card, Memory card folder | `[game] memcard, memcard_dir` | restart |
| | Game folder, Disc image, Main executable | `[game] game_dir, iso, elf` | restart |
| Controls | Button labels, Stick dead zone, Trigger threshold | `[gamepad] layout, deadzone, trigger_threshold` | live |
| | Bindings (2 keys + 1 gamepad button per DS2 action), Reset | `[keyboard]`, `[gamepad]` entries | live |
| Info | Performance overlay, Take screenshot | `[overlay] mode`; PNG to `[game] screenshot_dir` | live |
| | Version, Menu, GPU, Settings file | read-only | |
| | Ignored XSG_* environment | read-only list (only when some are set) | |
| Debug | Performance overlay, Screenshot folder | `[overlay] mode`, `[game] screenshot_dir` | live |
| | every `[debug]` key, in table order: toggles for 0/1, choices for lists, text fields otherwise | `[debug]` all | restart |

The FPS limit is the game's vblank rate, and the game logic runs per vblank, so it is also
the game speed: 30 is half speed, 120 double, uncapped is fast-forward. `[debug] frame_us`,
when set, fixes the period instead (0 = unthrottled) and the menu's limit is not applied.

**Start-up settings.** Both processes read the same file at start: the game
(`port/runtime/settings.c`) for `[renderer] gs`, `[game]`, `[audio] enabled/sound_driver`
and the game-side `[debug]` keys; xsg-gpu (`menu/cfg.cpp`) for `[display]`, `[renderer]`,
the menu and the xsg-gpu-side `[debug]` keys. Nothing is passed through the environment any
more except `XSG_CONFIG`, which xsg-gpu inherits. The live-stream FIFO goes to xsg-gpu on its
command line (`--stream=`), since only one process may feed it.

**Volume classes** come from how the sound driver allocates SPU2 voices. Music is the
core 0 voices (the sequencer). Effects are core 1 voices 0–19. Voice is core 1 voices 20–23
(the VAG stream voices, `SsdAllocVoiceVagStream`, which carry scene voice and the movie
soundtracks) plus AutoDMA PCM. Scenes mix their background music into that same stream, so
it follows "Voice and movies". The gain curve is percent squared (50 % ≈ −12 dB), in Q15
per voice inside `spu2.c core_sample`. Master is applied to the final output and so also
affects `[debug] wav`. Audio off zeroes the master; `[audio] enabled = 0` at start opens no
audio device at all. The Stage 8 fallback decoder (`[audio] sound_driver = 0`) is not scaled.

## Game-process settings: `GpuSettings`

`port/runtime/gpu_ipc.h` (protocol version 4) adds `GpuSettings` at offset 2048 of the shm
header page, next to `GpuPad` at 1024. It is a seqlock like the pad, and holds `frame_us`
(`XSG_GPU_FRAME_US_KEEP` = leave pacing alone, 0 = uncapped), `audio_on`, `vol[4]`
(master, music, effects, voice), `restart` and `menu_open`. xsg-gpu publishes it whenever a
setting changes. The game polls it once per vblank in `kernel.c settings_poll` →
`xsg_gpu_settings_poll`. A change is applied at that vblank: the frame period (kept when
`[debug] frame_us` is set), or `xsg_sound_set_volume` → `spu2_set_mix_gains`. All zero (no
xsg-gpu, or soft GS) keeps what the game read from settings.ini at start.

## Config file: `~/.config/xsg/settings.ini`

Location: `$XSG_CONFIG`, else `$XDG_CONFIG_HOME/xsg/settings.ini`, else
`~/.config/xsg/settings.ini`. **`XSG_CONFIG` is the only environment variable the port
reads** (besides `HOME`/`XDG_CONFIG_HOME` for the default path). Precedence is
**settings.ini > default**. Changes are written about 0.6 s after the last edit, and at
once when the menu closes. The file is rewritten whole: sections in a fixed order
(`display renderer game audio gamepad overlay keyboard debug`), unknown keys kept, comments
not kept.

**Legacy environment variables (decision).** Every old `XSG_*` variable is **ignored**; the
environment never silently wins. When one is present, the game logs a deprecation line that
names the replacement, for example

```
[settings] DEPRECATED: XSG_MPEG is ignored; set [game] movies = 0 in /home/u/.config/xsg/settings.ini
```

and the menu's Info tab lists the ignored variables. They are not migrated automatically
(a run's env must not rewrite the user's file). Internal variables are gone:
`XSG_CFG_FROM_INI`, `XSG_SETTINGS_INI` (now `XSG_CONFIG`), `XSG_INPUT_INI`, `XSG_GPU_STREAM`
(now xsg-gpu's `--stream=` argument), `XSG_GPU_VSYNC=1` (now `[display] present = fifo`),
`XSG_FAST=1` (now `[game] fps_limit = uncapped`).

**Schema: one source.** `port/runtime/xsg_settings_table.h` is an X-macro list
`XSG_SETTING(key, legacy_env, default, values, live, description)`. The game's reader
(`settings.c`: `xsg_cfg`, `xsg_cfg_int`, `xsg_cfg_on`) and the menu's table (`menu/cfg.cpp`)
both include it, so keys, defaults and value lists cannot drift. A new setting is one line
there; the Debug tab shows new `[debug]` keys by itself. `xsg_cfg()` returns NULL for an
empty value, like `getenv()` of an unset variable, so the old call sites kept their logic.

```ini
[display]
present = fifo
aspect = 4:3

[renderer]
gs = gpu
native = 1
native_scale = 4

[game]
fps_limit = 60
memcard_dir = /home/u/saves/xsg

[audio]
master = 80
music = 70

[gamepad]
deadzone = 8000
layout = playstation
south = CIRCLE

[overlay]
mode = fps

[keyboard]
# only differences from the default layout (STAGE10_INPUT.md §2); NONE unbinds a default
Space = CROSS
Escape = NONE

[debug]
fps = 600
```

Ready-made files: `port/docs/examples/` (`field-run.ini`: the headless field recipe used by
the tools; `status-site.ini`: what the status site writes per run; `desktop.ini`: a windowed
player setup). Run one with `XSG_CONFIG=port/docs/examples/field-run.ini port/build/xsg`.

**Migration.** When settings.ini does not exist yet, `~/.config/xsg/input.ini` (same
sections and syntax) is read and written into settings.ini, and input.ini itself is left as
it is. A `[video]` section (before 2026-11-07) is moved to `[display]` on load. The
remapping syntax is unchanged. When a binding is edited in the menu, the
`[keyboard]`/`[gamepad]` sections are rewritten as the difference from the defaults.

### settings.ini reference

Generated from `port/runtime/xsg_settings_table.h` (31 user settings, 63 `[debug]`
switches; plus the free-form binding entries of `[keyboard]`/`[gamepad]`). "replaces" is the
environment variable the setting took over; it is no longer read.

| key | default | values | applies | replaces | description |
|---|---|---|---|---|---|
| `[display] fullscreen` | `0` | 0, 1 | live | `XSG_GPU_FULLSCREEN` | Start fullscreen (F11 toggles). |
| `[display] window_scale` | `2` | 1..4 | live | `XSG_GPU_SCALE` | Window size as a multiple of 512x448. |
| `[display] present` | `mailbox` | mailbox, fifo, immediate, auto | live | `XSG_GPU_PRESENT` | Swapchain present mode (fifo = vsync; replaces XSG_GPU_VSYNC=1). |
| `[display] aspect` | `4:3` | 4:3, 16:9, stretch | live | `XSG_GPU_ASPECT` | Display aspect ratio. |
| `[display] widescreen` | `0` | 0, 1 | live | – | Native 16:9 (needs `[renderer] native`): the game widens its screen-edge culling, the native layer draws 684 x 448 with the GS frame pillarboxed in the centre; frames without a native layer stay 4:3 with bars. Overrides `aspect`. |
| `[display] scale_mode` | `fit` | fit, integer | live | `XSG_GPU_SCALEMODE` | Fit the window or integer scaling. |
| `[display] filter` | `linear` | linear, nearest | live | `XSG_GPU_FILTER` | Blit filter. |
| `[renderer] gs` | `gpu` | gpu, soft, both | restart | `XSG_GS` | GS backend: gpu (xsg-gpu, the menu), soft (software GS, no window/menu), both (compare). |
| `[renderer] native` | `0` | 0, 1 | live | `XSG_GPU_NATIVE` | Native Vulkan renderer (NATIVE_RENDERER.md). |
| `[renderer] native_scale` | `2` | 1..8 | live | `XSG_GPU_NATIVE_SCALE` | Native renderer internal scale. |
| `[renderer] neural` | `0` | 0, 1 | restart | `XSG_GPU_NEURAL` | DLSS 5 Neural Rendering via DLSS5VKLayer, off by default (the layer only loads when this is on at start). |
| `[renderer] ssaa` | `1` | 1, 2, 4, 8, 16 | live | `XSG_GPU_SSAA` | paraLLEl-GS super-sampling. |
| `[renderer] ssaa_textures` | `0` | 0, 1 | live | `XSG_GPU_SSAA_TEX` | Super-sampled textures (SSAA >= 2). |
| `[renderer] hires_scanout` | `0` | 0, 1 | live | `XSG_GPU_HIRES` | High-resolution scanout (SSAA >= 4). |
| `[game] fps_limit` | `60` | 60, 30, 120, uncapped | live | `XSG_FAST` | Vblank rate = game speed; uncapped replaces XSG_FAST=1. |
| `[game] movies` | `1` | 0, 1 | live | `XSG_MPEG` | Play the FMV movies (needs ffmpeg). |
| `[game] ffmpeg` | (empty) | text | live | `XSG_FFMPEG` | ffmpeg binary for movies (empty: ffmpeg on PATH). |
| `[game] memcard` | `1` | 0, 1 | live | `XSG_MC` | Memory card in slot 1. |
| `[game] memcard_dir` | (empty) | text | live | `XSG_MC_DIR` | Memory card folder (empty: ~/.local/share/xsg/mc0). |
| `[game] game_dir` | (empty) | text | restart | `XSG_GAME_DIR` | Game data folder with iso/ and extracted-iso/ (empty: build default). |
| `[game] iso` | (empty) | text | restart | `XSG_ISO` | Disc image path (empty: the first image in <game_dir>/iso). |
| `[game] elf` | (empty) | text | restart | `XSG_ELF` | SLUS_204.69 path (empty: <game_dir>/extracted-iso/SLUS_204.69). |
| `[game] screenshot_dir` | (empty) | text | live | `XSG_SCREENSHOT_DIR` | Folder for menu screenshots (empty: ~/Pictures/xsg). |
| `[audio] enabled` | `1` | 0, 1 | live | `XSG_AUDIO` | Sound output (0 at start: no audio device is opened; live: mutes the mix). |
| `[audio] sound_driver` | `1` | 0, 1 | restart | `XSG_SOUND` | Run the game's sound driver (RSSD) at all. |
| `[audio] master` | `100` | 0..100 | live | — | Master volume. |
| `[audio] music` | `100` | 0..100 | live | — | Music volume. |
| `[audio] effects` | `100` | 0..100 | live | — | Effects volume. |
| `[audio] voice` | `100` | 0..100 | live | — | Voice and movie volume. |
| `[gamepad] deadzone` | `8000` | 0..32000 | live | — | Stick dead zone. |
| `[gamepad] trigger_threshold` | `8000` | 0..32000 | live | — | Trigger press threshold. |
| `[gamepad] layout` | `xbox` | xbox, playstation, nintendo | live | — | Button labels in the menu. |
| `[overlay] mode` | `off` | off, fps, full | live | `XSG_GPU_OVERLAY` | Performance overlay (F10 cycles). |
| `[debug] log` | (empty) | text | restart | `XSG_LOG` | Log level (number; empty: default). |
| `[debug] log_file` | (empty) | text | restart | `XSG_LOG_FILE` | Write the game log to this file instead of stderr. |
| `[debug] timeout` | (empty) | text | restart | `XSG_TIMEOUT` | Exit after N seconds. |
| `[debug] gpu_bin` | (empty) | text | restart | `XSG_GPU_BIN` | xsg-gpu binary (empty: gpu/xsg-gpu next to xsg). |
| `[debug] headless` | `0` | 0, 1 | restart | `XSG_GPU_HEADLESS` | xsg-gpu without a window. |
| `[debug] vulkan_validation` | `0` | 0, 1 | restart | `XSG_GPU_VVL` | Allow the Vulkan validation layer. |
| `[debug] pmode_raw` | `0` | 0, 1 | restart | `XSG_GPU_PMODE_RAW` | Scan out PMODE as the game wrote it. |
| `[debug] stream` | (empty) | text | restart | `XSG_STREAM` | Raw 512x448 RGB24 live stream FIFO path. |
| `[debug] jump` | (empty) | text | restart | — | Test room: once the first field runs, jump to this scene (cf1234, ev0123). |
| `[debug] room` | (empty) | text | restart | — | Test room by name: a scene plus pad steps (see Test rooms below). |
| `[debug] give_all` | `0` | 0, 1 | restart | — | At the first field: whole party, A.G.W.S., money, every item, weapon, ammo and accessory. |
| `[debug] scene_list` | `0` | 0, 1 | restart | — | Log every scene file on the disc at the first field. |
| `[debug] skip_events` | `0` | 0, 1 | restart | `XSG_SKIP_EVENTS` | Skip story events (test runs). |
| `[debug] frame_us` | (empty) | text | restart | `XSG_FRAME_US` | Fixed vblank period in us (overrides game.fps_limit; 0 = unthrottled). |
| `[debug] fast_until` | (empty) | text | restart | `XSG_FAST_UNTIL` | Unthrottled until this vblank. |
| `[debug] fps` | (empty) | text | restart | `XSG_FPS` | Log [fps] every N frames. |
| `[debug] dma_thread` | `0` | 0, 1 | restart | `XSG_DMA_THREAD` | Run VIF1/GIF DMA on the DMAC thread. |
| `[debug] pad` | (empty) | text | restart | `XSG_PAD` | Scripted pad input, e.g. 300:START,1100:START. |
| `[debug] pad_log` | (empty) | text | restart | `XSG_PAD_LOG` | Log pad reads. |
| `[debug] pad_live` | (empty) | text | restart | `XSG_PAD_LIVE` | Live pad command file. |
| `[debug] pad_stick` | (empty) | text | restart | `XSG_PAD_STICK` | Scripted analog stick input. |
| `[debug] remote_pad` | (empty) | text | restart | | Remote pad state file (32-byte record, STATUS_SITE.md), read each vblank. |
| `[debug] twin_log` | (empty) | text | restart | `XSG_TWIN_LOG` | Twin (pad lock-step) log. |
| `[debug] shot` | (empty) | text | restart | `XSG_SHOT` | Frames to screenshot (list). |
| `[debug] shot_every` | (empty) | text | restart | `XSG_SHOT_EVERY` | Screenshot every N frames. |
| `[debug] shot_vram` | `0` | 0, 1 | restart | `XSG_SHOT_VRAM` | Also dump VRAM with each shot. |
| `[debug] shot_dir` | (empty) | text | restart | `XSG_SHOT_DIR` | Screenshot folder (empty: port/build/shots). |
| `[debug] shot_fbp` | (empty) | text | restart | `XSG_SHOT_FBP` | Force the screenshot frame buffer page. |
| `[debug] vu1_capture` | (empty) | text | restart | `XSG_VU1_CAPTURE` | VU1 capture folder. |
| `[debug] vu1_capture_from` | (empty) | text | restart | `XSG_VU1_CAPTURE_FROM` | VU1 capture first frame. |
| `[debug] vu1_capture_frames` | (empty) | text | restart | `XSG_VU1_CAPTURE_FRAMES` | VU1 capture frame count. |
| `[debug] vu1_capture_every` | (empty) | text | restart | `XSG_VU1_CAPTURE_EVERY` | VU1 capture interval. |
| `[debug] gs_trace` | (empty) | text | restart | `XSG_GS_TRACE` | GS trace of frame N. |
| `[debug] gs_compare` | (empty) | text | restart | `XSG_GS_COMPARE` | renderer.gs=both: compare every N frames (empty: 60). |
| `[debug] dma_trace` | `0` | 0, 1 | restart | `XSG_DMA_TRACE` | Trace DMA kicks. |
| `[debug] mpg_trace` | `0` | 0, 1 | restart | `XSG_MPG_TRACE` | Trace MPEG uploads. |
| `[debug] ipu_log` | (empty) | text | restart | `XSG_IPU_LOG` | IPU log level. |
| `[debug] vu1` | (empty) | text | restart | `XSG_VU1` | 0 disables VU1 microprograms. |
| `[debug] vu_fast` | (empty) | text | restart | `XSG_VU_FAST` | 0: reference VU1 interpreter instead of the fast path. |
| `[debug] vu_flaglat` | (empty) | text | restart | `XSG_VU_FLAGLAT` | VU flag latency model. |
| `[debug] vu_hist` | (empty) | text | restart | `XSG_VU_HIST` | VU1 opcode histogram. |
| `[debug] vu_trace` | (empty) | text | restart | `XSG_VU_TRACE` | Trace N VU1 runs. |
| `[debug] vu_dump` | (empty) | text | restart | `XSG_VU_DUMP` | Dump VU1 state at frame N. |
| `[debug] vu_log` | (empty) | text | restart | `XSG_VU_LOG` | VU1 log level. |
| `[debug] vu0_stats` | (empty) | text | restart | `XSG_VU0_STATS` | VU0 statistics. |
| `[debug] prof` | (empty) | text | restart | `XSG_PROF` | Sampling profiler output file. |
| `[debug] prof_start` | (empty) | text | restart | `XSG_PROF_START` | Profiler start frame. |
| `[debug] prof_hz` | (empty) | text | restart | `XSG_PROF_HZ` | Profiler rate (empty: 1000). |
| `[debug] sound_log` | (empty) | text | restart | `XSG_SOUND_LOG` | Sound driver log level. |
| `[debug] spu_trace` | (empty) | text | restart | `XSG_SPU_TRACE` | SPU2 trace folder. |
| `[debug] sound_clock` | `game` | game, real | restart | `XSG_SOUND_CLOCK` | IOP sound clock source. |
| `[debug] wav` | (empty) | text | restart | `XSG_WAV` | Record the mix to this WAV file. |
| `[debug] native_vu1` | `0` | 0, 1 | restart | `XSG_GPU_NATIVE_VU1` | Native renderer: interpret VU1 programs. |
| `[debug] native_skin` | `1` | 0, 1 | restart | `XSG_GPU_NATIVE_SKIN` | Native renderer: GPU skinning. |
| `[debug] native_live` | (empty) | text | restart | `XSG_GPU_NATIVE_LIVE` | 0: native renderer shots only. |
| `[debug] native_thr` | (empty) | text | restart | `XSG_GPU_NATIVE_THR` | Native renderer threshold. |
| `[debug] native_dir` | (empty) | text | restart | `XSG_GPU_NATIVE_DIR` | Native renderer shot folder. |
| `[debug] native_shot` | (empty) | text | restart | `XSG_GPU_NATIVE_SHOT` | Native renderer shot vsyncs (list). |
| `[debug] native_owner` | (empty) | text | restart | `XSG_GPU_NATIVE_OWNER` | Native FX owner tracking. |
| `[debug] owner_thread` | (empty) | text | restart | `XSG_GPU_OWNER_THREAD` | Native FX owner thread. |
| `[debug] fx_trace` | (empty) | text | restart | `XSG_GPU_FX_TRACE` | Native FX trace. |
| `[debug] mv_view` | (empty) | text | restart | `XSG_GPU_MV_VIEW` | Motion vector view. |
| `[debug] gpu_prof` | (empty) | text | restart | `XSG_GPU_PROF` | xsg-gpu profiler. |
| `[debug] input_test` | (empty) | text | restart | `XSG_GPU_INPUT_TEST` | Synthetic SDL input events (STAGE10_INPUT.md). |
| `[debug] menu_test` | (empty) | text | restart | `XSG_GPU_MENU_TEST` | Menu screenshot tour: at:per:WxH. |
| `[debug] menu_test_dir` | (empty) | text | restart | `XSG_GPU_MENU_TEST_DIR` | Menu tour output folder. |
| `[debug] menu_test_set` | (empty) | text | restart | `XSG_GPU_MENU_TEST_SET` | Timed settings changes: vsync:key=v,...;... |

### Old env recipe → ini

Each `XSG_FOO=v` of an older doc or note is `[section] key = v` from the table above (the
"replaces" column). For instance the Stage 10 field recipe

```
XSG_GS=gpu XSG_GPU_HEADLESS=1 XSG_AUDIO=0 XSG_MPEG=0 XSG_SKIP_EVENTS=1 XSG_TIMEOUT=120 \
XSG_PAD='300:START,1100:START,1200:CROSS,1500:CIRCLE*400@60' port/build/xsg
```

is now `XSG_CONFIG=port/docs/examples/field-run.ini port/build/xsg`, with

```ini
[renderer]
gs = gpu
[game]
movies = 0
[audio]
enabled = 0
[debug]
headless = 1
skip_events = 1
timeout = 120
pad = 300:START,1100:START,1200:CROSS,1500:CIRCLE*400@60
```

**Change:** Esc no longer maps to CROSS by default, because it opens the menu. K is still
CROSS.

## Rebuilt game UI (`[display] ui = rebuilt`)

The game publishes UI state in the shared-memory header (`gpu_ipc.h`, 256 KB since version 7),
and xsg-gpu draws it with Dear ImGui in the menu's style (`port/gpu/ui/dialog.cpp`). The game
keeps its painted art (portraits, the turn glow) and skips the sprites that are redrawn.

| piece | game side (`PORT_HOST`) | shared memory |
|---|---|---|
| dialog window | `twsys_init.c` TWIN_draw2, `ew_sprt_set_cursor_uv.c` | `GpuUiDialog` @2560 |
| battle party panels | ov01 `menu.c` menuPlStatPut | `GpuUiBattle` @3328 |
| battle command menu (CROSS) | ov01 `menu.c` menuBatObjDraw | `GpuUiBattleCmd` @3584 |
| battle key guide (attacks, L1/R1) | ov01 `menu.c` menuKeyGuideDisp, in capture mode | `GpuUiKeyGuide` channel 0 @4096 |
| battle target name plate | ov01 `battle_init.c` monsNameDisp, in capture mode | `GpuUiKeyGuide` channel 1 |
| every menu (camp menu and its pages, shops, save / load, battle lists) | `end_print.c` PrintFlush -> xsg_ui_print_capture, `e_message.c` text | `GpuUiMenuList` @32 KB |

### The menus (`port/gpu/ui/menus.cpp`)

All menu screens draw 2D through one display list (`PrintFunc`, flushed by `PrintFlush`). Under
the rebuilt UI the flush records each entry as an op instead of drawing it: windows, the
game's gradient ribbons, sprites (with their uv table, palette and texture set), title tags,
numbers, lines, circles and scissors. The menu text from `eMessageDrawType01` is recorded too,
with control codes normalised to text, newlines, colour changes and inline-icon slots. xsg-gpu
redraws the whole list in the game's depth order (a stable sort, with text after entries at
equal depth), each op under the scissor active at its queue position:

* windows: layered glass panels (drop shadow, gradient, hatch, sheen, brackets) that fade and
  settle in, keyed by their order in the list so the game's own slides stay smooth;
* ribbons: tinted glass with capped opacity, a sheen and an underline;
* neutral rules: fine periwinkle hairlines that fade at the ends;
* the cursor: the game's art, gliding between rows over a soft glow;
* numbers, tags and text: the UI font with a halo; PlayStation button symbols in text become
  controller glyphs for `[gamepad] layout`;
* everything else: the game's own art.

Art: the menu texture shares VRAM with a kanji font page, and the game swaps a second atlas in
mid-list, so it is never read at frame end. The capture puts a marker in the GIF stream (a
TEXFLUSH carrying "XSGM" and a slot) after each texture change; when xsg-gpu's GS-memory mirror
(kept by the native renderer) reaches it, the 256 KB menu region is snapshotted as that slot's
texture set, so the art is exactly what the GS would sample there. With the native renderer off,
the capture falls back to sending the atlas files themselves (`GPU_MSG_UI_TEX`); portraits the
game patches in VRAM are not covered by that fallback.

Also captured: every `xglFontPrint` / `xglFontPrintDirect` string (results, subtitles, game over,
boot messages; direct prints join the positioned print before them), choice menus (twsys
TMENU), scene captions (TWIN_drawScene2), the pause screen, the title choices, and the battle
HUD's sprite parts (portraits, radar, turn order, boost gauge: capture channels 2-3, with
per-texture snapshots so art whose VRAM is reused later in the frame stays right).

Button prompts: `[display] prompts` (auto, xbox, playstation, nintendo, keyboard; auto follows the
connected controller and falls back to the keyboard's default binds) and `[display] prompt_style`
(default, light, retro, alt, alt2, drawn) pick icons from JulioCacko's "FREE Input Prompts Pack"
(CC0), vendored as a 96 px subset in `port/assets/prompts` by `port/tools/vendor_prompts.py`.

Art overrides: pictures the user supplies in `$XDG_CONFIG_HOME/xsg/art/` (`~/.config/xsg/art`),
never shipped. `title_logo.png` (a transparent logo, e.g. the release's clear logo) rebuilds the
title screen: the picture's side bands, the copyright lines, and that logo in the original's
place. Without it the title picture is shown upscaled.

Layout (`[display] ui_layout`, `port/gpu/ui/layout.cpp`): every element is placed with an anchor
(left, right, centre, stretch) at the 4:3 display's proportions. `game` keeps the UI on the
picture; `wide` moves edge UI out to the window's edges, the groundwork for a widescreen picture. With `[display] widescreen = 1` the
game area is the 16:9 picture's centre 4:3 and edge UI is anchored to the picture's edges.

Not captured (still the game's): the menu background (the frozen frame), 3D models (camp menu,
shop, formation), save thumbnails, the minigames (ov10 Xenocard, ov11 casino, ov12 robot battle: their own 2D painters), and the HDD install screen.

Capture mode: the key guide runs its own code unchanged while its drawing helpers (`keyGideDisp`,
`menuGuideFrameDisp`, `menuFontPut`, `menuNumPut`, `menuStatSprite`) record what they would draw.
xsg-gpu redraws the record, so every game rule (unavailable moves, help modes, A.G.W.S. tables)
stays the game's. Button icons are drawn by position for `[gamepad] layout`: Xbox letters,
PlayStation shapes, or Nintendo letters (□ west, △ north, ○ east, × south).

## Performance overlay (F10)

* **fps:** the frame rate over the last half second, in the top-right corner.
* **full:** fps, the 1 % and 0.1 % lows over the last 2048 presents, average and maximum
  frame ms, a frame-time graph of the last 240 presents, window and image/scanout sizes,
  present mode, GPU name and VRAM, ring fill, vsyncs and presents, and with the native
  renderer the native batches per frame and frames native vs VU1+paraLLEl-GS.

## Test hooks

`[debug]` keys (after restart):

```
menu_test = <vsync>[:<vsyncs per page>[:<W>x<H>]]   open the menu at that vsync and write
    menu_<n>_<tab>.png for each tab (0 display .. 5 debug), then menu_6_overlay.png (full
    overlay), to menu_test_dir (default .).  Headless works; images are game frame + menu at WxH.
menu_test_set = <vsync>:<key>=<value>[,...][;...]   timed settings without input;
    special keys SAVE=1, OPEN=0|1, TAB=n, SHOT=<png path>, RESTART=1.
```

### Test rooms (`port/runtime/rooms.c`)

These jump straight to a screen for UI work. Once the first field scene runs, `debug.jump`
jumps to a scene the way the developers' debug menu does. `debug.room` picks a named
scene plus pad steps. Pad entries written `+N:BUTTON` count N vblanks from the moment the
jumped-to scene starts, so they don't depend on how long the title screen took. The title
still needs its own presses (`300:START,1100:START,1200:CROSS` with an empty memory card).
The log shows every scene change as `[room] vblank V scene cfNNNN map NAME`.
`scene_list = 1` lists the 568 scenes on the disc.

| room | scene | shows |
|---|---|---|
| `battle_tutorial` | cf9001 | Shion + KOS-MOS vs attack drones, with tutorial pages and dialog |
| `battle_kosmos`, `battle_kosmos2` | cf9002, cf9003 | KOS-MOS: full command menu, AP, boost |
| `battle_boss` | cf9005 | boss, three-member turn list |
| `battle`, `battle_fx` | cf9011, cf9021 | U-TIC soldier; a battle that opens on a large effect |
| `field`, `menu` | cf9051 | free walking in MC_KOU01; the main menu |
| `menu_items`, `menu_agws`, `menu_options`, `menu_formation` | cf9051 | those main-menu pages |
| `menu_ether`, `menu_tech`, `menu_skills`, `menu_characters` | cf9051 | those pages, with Shion picked |
| `menu_ether_tree` | cf9051 | Ether > Shion > Use E.Pts (the Ether tree) |
| `save`, `save_files` | cf9051 | a save point's file menu, opened the way the field loop opens it; slot 1's file list |
| `shop`, `shop2`, `shop3`, `shop_buy`, `shop_sell` | cf9051 | shops 1-3 (`MenuShopMain`); shop 1's Buy and Sell lists |

The menu rooms imply `give_all` (from the developers' `ShopTest` set-up plus the full
item tables), so lists are full and A.G.W.S. is unlocked. U.M.N. stays locked (a story
flag). The cf90xx scenes are the developers' battle tests. cf9999 is the game's exit id, which
hangs the field loop.

`[debug] input_test` (STAGE10_INPUT.md) also drives the menu. Its synthetic key events go
through the menu first, both windowed and headless.

## Verification of settings.ini-only configuration (2026-11-07)

* **Ini-only run = env run.** The field recipe (gpu, headless, no audio, no movies, skip
  events, the pad script, shots at vblanks 1000/2000/3000/4500/6000) run with the old env
  recipe on the pre-change build and with only `XSG_CONFIG` on the new build: all five
  512×448 screenshots are byte-identical.
* **Menu change persists and takes effect.** `menu_test_set = 600:audio.master=40,
  display.filter=nearest,game.fps_limit=30,debug.fps=120,SAVE=1`: the game applied
  `vblank period 16666 us -> 33333 us` and `master 40` at vblank 601, the file gained
  `[display] filter = nearest`, `[game] fps_limit = 30`, `[audio] master = 40`,
  `[debug] fps = 120`, and the menu showed RESTART NOW `*` (a restart key changed). A second
  run of the same file started at 30 vblanks/s with master 40 and printed `[fps]` every 120
  vblanks (the after-restart key took effect).
* **Environment ignored.** `XSG_MPEG=1` alongside `[game] movies = 0`: movies stayed off and
  the log said `DEPRECATED: XSG_MPEG is ignored; set [game] movies = 1 in …`.
* **Status-site runner.** `server.py runner` against a private LIVE_DIR wrote its
  settings.ini, started the game with only `XSG_CONFIG` (a stale `XSG_GS=soft` in the
  server's environment was stripped), ran to vblank 2010 on the gpu renderer and wrote a
  shot every 30 vblanks. `[debug] stream` reached xsg-gpu through `--stream=` (485 MB of
  RGB24 read from the FIFO in 12 s).

## Verification (RTX 5090## Verification (RTX 5090, Wayland/Hyprland)

* **Screenshots** (headless, field scene at vsync 5500, 1280×960; taken before the Debug tab):
  `port/docs/screenshots/menu_{0_display,1_renderer,2_game,3_controls,4_info,5_overlay}.png`.
  Windowed: `menu_window_wayland.png`, after F1, Down, Down, Right sent by `[debug] input_test`.
  Present mode changed to FIFO, and the swapchain was recreated live.
* **FPS limit reaches the game.** `[fps]` showed 60.0 vblanks/s. After `game.fps_limit=uncapped`
  (`[settings] frame 1702: vblank period 16666 us -> 0 us (uncapped)`) it rose to 74–79
  (CPU-bound). At 30 it was 30.0, and back at 60 it was 59.6–60.0.
* **Volume reaches the mix.** Recorded with `[debug] wav`: master 100 → 25 at vblank 2400. The
  RMS per 2 s went from about 2450 to about 160, i.e. ×0.065, against the expected 0.25² = 0.0625.
* **Input is blocked while open.** F1 at 1900, then W held 1950–2050: no live mask. F1 at
  2200, then W at 2250: `[pad] vblank 2251: live 0x1000`.
* **Persistence.** The second run read `live_settings.ini: 3 entries`, and the first published
  settings were master 40 / music 70. Windowed, the run wrote `[video] present = fifo` (now `[display]`) and
  `[overlay] mode = full`.

## Present-path fix found on the way: PMODE

The window and every scanout-based image were black, also with the pre-menu binaries. The
game leaves PMODE at 0x8004 or 0, which has both read circuits off and MMOD 0. paraLLEl-GS
honours that, while the software GS and `[debug] shot` ignore PMODE. xsg-gpu now passes PMODE to
paraLLEl-GS as EN1 + MMOD + ALP 0xFF, i.e. circuit 1 opaque, like the software GS. It
leaves PMODE alone when only EN2 is set. `[debug] pmode_raw = 1` restores the raw register.
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

## Status site (web controller)

`port/tools/status_site/server.py` runs the game headless and serves the live video with a virtual
DualShock 2. It feeds the game through `[debug] remote_pad`. See STATUS_SITE.md.

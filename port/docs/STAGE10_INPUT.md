# Stage 10: keyboard and gamepad play, window options

> **Settings (2026-11-07):** the port no longer reads `XSG_*` environment variables. Every
> `XSG_FOO=v` in this document is now `[section] key = v` in settings.ini (the mapping is the
> "replaces" column of [MENU.md, settings.ini reference](MENU.md#settingsini-reference)); run
> with `XSG_CONFIG=<file> port/build/xsg`. Samples: [examples/](examples/).

Real input without a 32-bit SDL. Only the 64-bit SDL3 exists (STAGE8_INPUT.md §2), and the
64-bit renderer process `xsg-gpu` already owns the SDL3 window, so input is read there and
handed to the game through the shared memory it already maps (STAGE9_GPU.md).

## 1. Design

```
SDL3 events (window keyboard, gamepads with hotplug, or synthetic test events)
  -> xsg-gpu port/gpu/xsg_input.cpp: pad state kept from events only
  -> once per game vblank (GPU_MSG_VSYNC): GpuPad in the shm header page (offset 1024)
  -> game port/runtime/gpu_client.cpp xsg_gpu_pad_read() (seqlock read)
  -> port/runtime/sdk.c scePadRead(port 0): DS2 report, OR'ed with the XSG_PAD script
```

* `GpuPad` (port/runtime/gpu_ipc.h, protocol version 2): `buttons` (SCE_PAD mask, active
  high), `stick[4]` in DS2 byte order (right x, right y, left x, left y; 0x80 = centre,
  0 = up/left), `press[12]` (DS2 pressure order R L U D △ ○ ✕ □ L1 R1 L2 R2), `active`,
  `gamepads`, `updates`. Only 32-bit fields, so the i386 and x86-64 layouts agree. The
  writer bumps `seq` to odd, writes, then to even; the reader retries on odd/changed `seq`.
* State comes from SDL events only (key down/up, gamepad button/axis), never from
  `SDL_GetKeyboardState`, so injected events follow exactly the real path. Key state is
  cleared on focus loss (no stuck keys after alt-tab).
* Several gamepads are OR'ed (buttons) or the largest deflection wins (axes).
  `SDL_EVENT_GAMEPAD_ADDED/REMOVED` open and close them (hotplug).
* The state is published after the event pump of each vsync, so input reaches the game's
  next `scePadRead`. Latency is one vblank plus however far the game runs ahead of the
  renderer in the ring (normally under a frame).
* **Merge with `XSG_PAD`**: buttons are OR'ed with the script. Pressure bytes come from the
  live pad, and are 0xff for scripted buttons. `XSG_PAD_LIVE=0` ignores live input (a pure
  scripted run on a desktop). `XSG_PAD_STICK=FROM-TO:LX,LY[:RX,RY],...` scripts the analog
  sticks (vblank range, bytes 0..255) and replaces the live sticks while an entry is active.
* **Headless / soft GS / no window**: xsg-gpu never sets `active`, so the game builds the
  neutral pad (no buttons, sticks 0x80, pressures 0) it always did. With `XSG_GS=soft`
  there is no xsg-gpu and no live input at all. Live input therefore needs `XSG_GS=gpu`
  (or `both`) with a window.

## 2. Default controls

| DualShock 2 | gamepad (SDL3 standard layout, by position) | keyboard |
|---|---|---|
| ✕ CROSS (cancel) | south (A on Xbox, ✕ on PS) | K, Esc |
| ○ CIRCLE (confirm, advance text) | east (B / ○) | L, Space |
| □ SQUARE | west (X / □) | J |
| △ TRIANGLE | north (Y / △) | I |
| L1 / R1 | left / right shoulder | Q / E |
| L2 / R2 | left / right trigger (analog pressure) | 1 / 3 |
| L3 / R3 | left / right stick click | Z / C |
| SELECT | back | Backspace |
| START | start | Enter, keypad Enter |
| d-pad | d-pad | arrows, W A S D |
| left stick | left stick (dead zone 8000/32768) | (none; bind `LS_*`) |
| right stick | right stick | keypad 8 / 5 / 4 / 6 |

In this game CIRCLE confirms and advances text windows; CROSS cancels (STAGE8_INPUT.md §1).

Window keys (not pad input): **F11** toggles fullscreen.

## 3. Remap file: `~/.config/xsg/input.ini`

Looked up as `$XSG_INPUT_INI`, else `$XDG_CONFIG_HOME/xsg/input.ini`, else
`~/.config/xsg/input.ini`. Entries add to or override the defaults; `NONE` unbinds.
Targets: `CROSS` (`X`), `CIRCLE` (`O`), `SQUARE`, `TRIANGLE`, `L1 R1 L2 R2 L3 R3`, `START`,
`SELECT`, `UP DOWN LEFT RIGHT`, `LS_UP LS_DOWN LS_LEFT LS_RIGHT`, `RS_UP ...` (push a
stick to its edge), joined with `+` for several.

```ini
[keyboard]
# SDL scancode names (SDL_GetScancodeFromName): A, Return, Left Shift, Keypad 8, ...
Space = CROSS
Escape = NONE
Left Shift = LS_UP
H = CIRCLE+SQUARE

[gamepad]
# SDL gamepad button names: south east west north back start guide leftstick rightstick
# leftshoulder rightshoulder dpup dpdown dpleft dpright misc1 ...; axes lefttrigger,
# righttrigger
south = CIRCLE           # Japanese-style confirm on the bottom button
east = CROSS
lefttrigger = L2
deadzone = 8000          # stick dead zone, 0..32767
trigger_threshold = 8000 # trigger travel that counts as a press

[video]
aspect = 4:3            # 4:3 (default), 16:9, stretch
scale_mode = fit        # fit (default) or integer
filter = linear         # linear (default) or nearest
present = mailbox       # mailbox (default), fifo, immediate, auto
fullscreen = 0
```

Unknown names are logged (`[xsg-gpu] input: ...`) and skipped.

## 4. Window

| option | env | ini `[video]` | default |
|---|---|---|---|
| display aspect | `XSG_GPU_ASPECT=4:3\|16:9\|stretch` | `aspect` | 4:3, letter/pillarboxed |
| scaling | `XSG_GPU_SCALEMODE=fit\|integer` | `scale_mode` | fit. `integer` = largest whole multiple of the scanout height that fits, width from the aspect; falls back to fit below 1x |
| filter | `XSG_GPU_FILTER=linear\|nearest` | `filter` | linear |
| present mode | `XSG_GPU_PRESENT=mailbox\|fifo\|immediate\|auto` (`XSG_GPU_VSYNC=1` = fifo) | `present` | mailbox |
| fullscreen | `XSG_GPU_FULLSCREEN=1`, F11 | `fullscreen` | off (borderless desktop fullscreen) |
| initial size | `XSG_GPU_SCALE=N` | | 2 × 512×448 |

The window is resizable (minimum 320×240) and the swapchain follows pixel-size changes.
Env vars win over the ini.

**Frame pacing.** The renderer thread is the ring consumer, so it must not block on
presentation. Mailbox (default) never blocks: the game's own vblank timer (59.94 Hz, or
`XSG_FAST`) paces everything and xsg-gpu presents the latest frame; no tearing. `fifo`
blocks in `vkQueuePresent` on the display's refresh. That paces the ring consumer to the
display: if the display is slower than the game's vblank rate, the ring fills and the game
stalls on back-pressure, never dropping frames. Use it for a display at or above 60 Hz.
`immediate` tears.
Input is polled and published per game vblank in all modes.

## 5. Test hook: `XSG_GPU_INPUT_TEST`

No one can press keys in CI, so xsg-gpu can inject synthetic SDL events (`SDL_PushEvent`).
They go through the same event handler, state, and shared-memory publish as real input:

```
XSG_GPU_INPUT_TEST=FRAME:KIND:NAME[:VALUE]:HOLD,...
  KIND key     NAME = SDL scancode name (W, Return, Space...)
       button  NAME = SDL gamepad button (south, east, dpup...), on a virtual gamepad
       axis    NAME = leftx lefty rightx righty lefttrigger righttrigger, VALUE -32768..32767
  pressed at game vblank FRAME, released HOLD vblanks later
```

The hook also works headless: with `XSG_GPU_HEADLESS=1` and `XSG_GPU_INPUT_TEST` set,
xsg-gpu starts only SDL's event subsystem (no window, no real devices) to carry the
synthetic events. Without the hook, headless stays neutral.

`XSG_PAD_LOG=1` (game) logs each change of the live mask and sticks
(`[pad] vblank N: live 0x1000 sticks R 80,80 L 80,80`), and every 60 vblanks the player
actor's position (`GameLoopState[1]` + 0x10: `[pad] vblank N: player at x y z`).

### Verification

Field recipe, headless, `XSG_FAST=1`, two runs side by side:

```
XSG_GS=gpu XSG_AUDIO=0 XSG_MPEG=0 XSG_SKIP_EVENTS=1 XSG_GPU_HEADLESS=1 XSG_FAST=1 XSG_PAD_LOG=1 \
XSG_PAD='300:START,1100:START,1200:CROSS,1500:CIRCLE*400@60' XSG_SHOT=5500,6150,6950 \
XSG_GPU_INPUT_TEST='5600:key:W:500,6300:axis:leftx:30000:600' port/build-wt/xsg
```

| vblank | baseline (no hook) | with hook |
|---|---|---|
| 5520 | player at 0, 0, 0 | 0, 0, 0 |
| 5600 | | `live 0x1000` (W = UP) published in the same vblank |
| 5640–6000 | 0, 0, 0 | z 0 -> -2.51 -> -7.20 (walks forward, then stops at an obstacle) |
| 6100 | | `live 0x0000` (released) |
| 6300 | | `sticks L f5,80` (left stick right) |
| 6420–6780 | 0, 0, 0 | x 0 -> 5.13 -> 6.78 -> 7.20 |
| 6900 | | stick back to 80,80; the player stops |

Screenshots `port/docs/screenshots/stage10_walk_up_6150.png` (Shion walked away from the
camera; the minimap marker moved up) and `stage10_stick_right_6950.png` (facing right, moved
right). In the baseline she stands at the start with KOS-MOS and the guard in view.
The baseline also confirms the neutral headless pad.

Windowed (Wayland, RTX 5090), normal speed, with an ini remapping `Space = CROSS` and
`south = CIRCLE` and `aspect = stretch`: `button:south` -> `live 0x0020`, `dpleft` ->
`0x8000`, `axis:lefttrigger` -> `0x0001` (L2), `key:Space` -> `0x0040`, `axis:lefty
-32768` -> `L 80,00`, each in the vblank it was pushed. The unknown key in the ini was
logged and skipped.

With `XSG_FAST=1` the game can run up to about 90 vblanks ahead of the renderer in the
ring, and live input lags by that much. At normal speed the lag is 0–2 vblanks.

Not testable here: a physical gamepad (hotplug code is SDL's `GAMEPAD_ADDED/REMOVED`
path; the virtual test pad uses the same button/axis handling) and F11 (`SDL_SetWindowFullscreen`).

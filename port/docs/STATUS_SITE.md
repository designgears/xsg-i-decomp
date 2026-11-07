# Status site and web controller

`port/tools/status_site/server.py` is a stdlib-only Python server. It runs a private copy of
`xsg` headless, configured only by a per-run `LIVE_DIR/settings.ini` passed as `XSG_CONFIG`
(MENU.md). It serves a mobile-first page with the live MJPEG video (`/live.mjpg`, about
60 fps), and a virtual DualShock 2 under the video lets anyone with the page open play the game.

```
python3 port/tools/status_site/server.py [--port 8765] [--live-dir /tmp/xsg-live]
    [--bin port/build/xsg] [--interactive | --pad 300:START,...] [--run-seconds 120]
```

- **Interactive** (the default when no `--pad` is given): no pad script, no `[debug] timeout`.
  The game restarts only when it exits, or from the page's **Restart game** button
  (`POST /api/restart`, which works only in this mode).
- **`--pad SCRIPT`**: the old automated mode, with a scripted pad and a restart every `--run-seconds`.
  The web controller still works in this mode.
- `--live-dir` holds `settings.ini`, `frames/`, `pad.bin`, `stream.fifo` and the binary copy.
  Use a separate folder (and `--port`) for a second instance so it does not disturb the live one.

## Pad path

```
browser --(WebSocket /ws/pad, or POST /api/pad)--> server: merge clients --> LIVE_DIR/pad.bin
game: scePadRead (port/runtime/sdk.c), each vblank: open + pread 32 bytes + close
```

### `[debug] remote_pad` record (32 bytes, little endian)

| off | size | field |
|---|---|---|
| 0 | 4 | magic `XSGP` |
| 4 | 4 | sequence (incremented on every write) |
| 8 | 4 | SCE_PAD button mask (`0x0040` CROSS, `0x0020` CIRCLE, `0x0800` START...; the `[debug] pad` names) |
| 12 | 4 | lx, ly, rx, ry (0..255, 128 = centre) |
| 16 | 12 | pressures in scePadRead order (0 = full 0xff for each held button) |
| 28 | 4 | writer time, CLOCK_REALTIME ms (low 32 bits) |

The server writes a temp file and `rename`s it into place, so a read always sees a whole record.
The game re-opens the file each vblank, because the rename gives it a new inode. A missing file,
a short record, a bad magic, or a timestamp older than 1 s (the server rewrites the record every
200 ms) reads as a released pad. If the server dies, the game sees no buttons held within 1 s.

The record is merged into the scePadRead result like this:

- Buttons: the remote pad, the xsg-gpu GpuPad (keyboard and gamepad in the window) and the
  `[debug] pad` script are OR'ed together.
- Sticks: each stick from the remote pad wins while it is off centre (more than 24 from 128).
  Otherwise the GpuPad sticks are used. `[debug] pad_stick` still overrides both.
- Remote buttons are included in the `[debug] pad_log` "live" mask. The status site turns
  `pad_log` on, so the game log shows `[pad] vblank N: live 0x0020 sticks ...`.

### Server

- Clients send `{buttons: ["CROSS", ...] | mask, lx, ly, rx, ry, t?}` as WebSocket text frames,
  or as the JSON body of `POST /api/pad` (an optional `id` identifies a POST client).
  If a message includes `t`, the server echoes it with the merged state (`mask`, `buttons`,
  `sticks`, `clients`). The page uses this for its round-trip time.
- With several clients, the buttons are OR'ed. Each stick comes from the client that moved it
  most recently, as long as it is off centre.
- A client that closes is released at once. A client that is silent for more than 500 ms is also
  released, so no button stays stuck. The page sends a keepalive every 200 ms.
- Limits: at most 16 controllers. A socket that sends more than 250 messages in one second is
  closed (code 1008). Frames larger than 4 KB are rejected. File writes happen at most every
  4 ms, plus the 200 ms refresh.
- The WebSocket code is a minimal RFC 6455 implementation (handshake, masked client frames,
  ping/pong, close) with `TCP_NODELAY` set.

### Page

- **Touch:** a d-pad with diagonals (the angle is split into 8 sectors, and you can slide between
  directions), the face buttons in PS positions, L1/L2/R1/R2, SELECT/START, and draggable left
  and right analog sticks. It uses multi-touch pointer events with pointer capture, and you can
  slide from one button to another. A short `navigator.vibrate` buzz plays on each press.
  `touch-action:none` and a prevented `touchstart` stop scrolling and zooming on the pad.
- **Layouts:**
  - Portrait: the pad sits under the video.
  - Phone landscape (`max-height:560px`): the controls go on both sides of the video.
  - Desktop (1100 px and wider): the pad sits under the video in the first dashboard column,
    and the dense panels are unchanged.
- **Keyboard:** the native defaults (STAGE10_INPUT.md):
  - WASD / arrows: d-pad
  - L / Space: ○, K: ✕, J: □, I: △
  - Q / E: L1 / R1, 1 / 3: L2 / R2
  - Enter: START, Backspace: SELECT
  - Z / C: L3 / R3
  - keypad 8 / 5 / 4 / 6: right stick
- **Gamepad API** (standard mapping, PS positions): south ✕, east ○, west □, north △, 4-7
  L1/R1/L2/R2, 8 SELECT, 9 START, 10/11 L3/R3, 12-15 d-pad, axes 0-3 for the sticks
  (dead zone 0.2).
- **Status indicator:** the connection state, the round-trip time, whether a gamepad is
  connected, and the held buttons.
- **Sending:** the page sends only on change, plus the 200 ms keepalive. It reconnects 1 s after
  a close.

## Measured locally (2026-11-07, 60 fps, loopback)

- WebSocket round trip (send to echo): 0.1–0.2 ms.
- Press (left stick right, in the field) to the first MJPEG frame that visibly changes: median
  124 ms (111–197 ms over 6 trials). Most of this is the game itself: it applies the stick on the
  next vblank, then turns and accelerates the character over a few frames. The rest is ffmpeg's
  MJPEG encoding and the frame pipeline. Over the cloudflared tunnel, add the network round trip.
- Driven only through `/ws/pad`:
  1. START on the title gave `live 0x0800` and started the game.
  2. CIRCLE advanced the opening text into the field.
  3. The left stick moved the player from (0, 0, 0) to (6.95, 0, 0) in 2 s (stick right), and
     then toward −z (stick up).
- Two clients plus a POST client were OR'ed. Closing a socket released its buttons at once, and
  silent clients were released about 0.5 s after their last message.

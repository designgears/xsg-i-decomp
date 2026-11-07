# Stage 8: input, leftover-register calls, past the tutorial prompt

## 1. Why `waitkey(64)` in ST0210 never ended

The pad was never the problem. `XSG_PAD` reached the game: `xglPadRead` turns the
`scePadRead` packet into `PadData[0].buttons/pressed` with an identity `button_map`
(`01 02 04 08 10 20 40 80`, logged by `XSG_PAD_LOG=1`).

`Window.waitkey(n)` prints `/[waitkey(n)]`. `MSG_convert` (src/main/msg_init.c) turns every
`/[func(args)]` into a control code by calling the MIF2 handler from `findMsgFunc`, but only
after a sanity check that the handler lies in EE RAM code:

```c
if (address - 0x1F0000U <= 0x1E0FFFFU && !(address & 3))
    output = handler(output, argc, args);
```

Host code is at 0x08xxxxxx, so **every** MIF2 command (`waitkey`, `wait`, `close`, `clear`
when it goes through a handler...) was dropped silently. The window printed its text, ran out
of queue and sat in state 0, never reaching state 8 (key wait). The script thread waited for
the window's close state forever. Confirmed under gdb:
`call MSG_convert(buf, 0x100, "/[waitkey(64)]", -1)` produced an empty string.

The same EE-RAM code check guards callbacks in `act_2.c` (actor draw hooks, 2 sites),
`jnt.c` (joint consumer and producer, 2 sites) and `near_dir.c` (2 handler loops). On the
host, all of them now use `XSG_HOST_IS_CODE(addr)` (port/include/ps2_host_compat.h: between
ld's `__executable_start` and `etext`, overlays included, no alignment test). The actor draw
hooks were skipped before, which may be part of why field geometry was missing.

What the key waits test: `TWIN_update2` (src/main/twsys_init.c) waits on
`PadData[0].pressed & 0x20`, i.e. **CIRCLE** (SCE_PAD `0x0020`), for page breaks (state 12),
`waitkey` (state 8) and the end of a message (11/17). The `64` in `waitkey(64)` is not a
button: it is the value stored in the window's close state when the key wait ends
(`closeState = pendingMessage`). CROSS (`0x40`) does not advance these windows. In this
build, toolkit menus (`TMENU_updateDefault`) also select with `0x20` (CIRCLE) and cancel
with `0x40` (CROSS).

## 2. XSG_PAD

```
XSG_PAD=FRAME:BUTTONS[:HOLD][*COUNT[@EVERY]],...
```

| part | meaning |
|---|---|
| `FRAME` | vblank of the first press (decimal or `0x` hex) |
| `BUTTONS` | `+`-joined names, or a numeric SCE_PAD mask: `CROSS`/`X` 0x40, `CIRCLE`/`O`/`OK` 0x20, `SQUARE`/`SQ` 0x80, `TRIANGLE`/`TRI` 0x10, `START` 0x0800, `SELECT` 0x0100, `UP` 0x1000, `RIGHT` 0x2000, `DOWN` 0x4000, `LEFT` 0x8000, `L1` 0x04, `L2` 0x01, `R1` 0x08, `R2` 0x02, `L3` 0x0200, `R3` 0x0400 (case-insensitive) |
| `:HOLD` | vblanks held (default 6) |
| `*COUNT` | number of presses (default 1) |
| `@EVERY` | vblanks between presses (default 60) |

Up to 512 entries; their masks are OR'ed. The old form `300:0x0800` still works.
`scePadRead` now also fills the 12 DualShock 2 pressure bytes (0xff for held buttons), although
`xglPadRead` derives pressures from the digital bits for mode 0x73 anyway.

Examples:
* through the opening to the field: `XSG_PAD='300:START,11000:CIRCLE*95@120'`
* then walk and talk: `...,23000:UP:240*20@600,23300:CIRCLE*40@300`

Debug switches: `XSG_PAD_LOG=1` logs each change of the scripted mask with the game's held word
and `button_map`. `XSG_TWIN_LOG=1` logs each text-window component's flags, state and group
status when they change (hooked from `TWSYS_update` under PORT_HOST). States: 0 printing,
1 opening, 2 closing, 8 waitkey, 9 clear, 10 newline/scroll, 12 page full (CIRCLE),
13 `wait(n)`, 17 end of text (CIRCLE).

**SDL2 keyboard/gamepad input: skipped.** The system has only the 64-bit libSDL2; there is
no `/usr/lib32/libSDL2*`, and `pkg-config` with `PKG_CONFIG_PATH=/usr/lib32/pkgconfig` falls
back to the 64-bit .pc file. To add it, install lib32-sdl2, then feed `scePadRead` port 0
from an SDL event pump on the vblank thread, OR'ed with `xsg_pad_script_buttons()`.

## 3. Leftover-register calls: `check_missing_args.py`

`port/scripts/check_missing_args.py` (about 3 s) parses every host TU with
`clang -Xclang -ast-dump=json` (host defines, `PORT_HOST`). It collects function definitions
(parameter count, variadic, static) and every call whose callee is a named function after
stripping parentheses, casts, `&` and `*`, plus `__asm__("label")` aliases on declarations.
It reports each call that passes fewer arguments than the definition takes. Exceptions go in
`check_missing_args_allow.txt` (`name reason` or `file:line:name reason`).

First run: 64 findings across 23 callees. Each was checked against the PS2 asm at the `jal`.

Fixed under PORT_HOST by passing what the EE register held:

| callee | PS2 register held | now passes |
|---|---|---|
| `nmlModelFlush` (6) | junk; it only reaches `AlphaGroupSort`, which ignores it | `0` (macro) |
| `srsGetWeaponEffectIdx` (2) | the caller's own `$a0` | `srsEftNo{Weapon,2Weapon}…(int id)` pass `id` |
| `_LoadSeq` | `XrgSoundSystemPlayBGM`'s `$a0` | `seq` |
| `MenuSortAddrGet` | `list` | `list` |
| `UmnDataBaseMonsterSet` | charaId | charaId |
| `unitActdraw` (2) | `actor` | `actor` |
| `MCamGetCurrentCoord` | `camMode` (delay slot) | `camMode` |
| `sndBankGet` | `sndSeStop`'s `$a0/$a1` | both |
| `MenuModelUnitBreak` | `work` | `work` |
| `UmnEventTextMake` | `UmnManzaiInit`'s `$a0` | `ev` |
| `Set_Spline_By_Random` | `self` | `self` |
| `CheckDoorDist` (2) | `door` | `door` |
| `AreaCheckBox` | `$f12` = incoming `r` | `r` |
| `xglCdStreamOpen` | `xglMpeg2Open`'s `$a1` (file name) | `name` |
| `SsdCheckPlayEffect` | `xglSoundEffectCheckID`'s `$a1` | new `source` parameter |

Allowlisted because the callee never reads the missing parameter: `menuStatSprite` (30 calls,
8th parameter `srcRow`), `_InitDashVRStatus`, `CharactorAllRecovery`/`AgwsAllRecovery`,
`scenarioBatExecMain`, `dataEtherLearnSet`, `SsdAddEffectData`, `tyaElevatorTask` (dead
caller). `RgFontStr` is fine: 6 of 6 arguments everywhere.

## 4. Structs by value: `check_call_types.py`

EE gcc passes aggregates larger than 8 bytes by invisible reference; i386 copies them onto the
stack. A new **STRUCT** class reports any slot where one view has a struct of 9 or more bytes
and another view has a pointer, an int or a different struct. It ranks with RET. `ps2_u128`
is classified `Q16`, because it is a 128-bit register scalar on the EE (`lq $5` before
`jal SGsAddGifRGBA`), not a struct. `--structs` lists every definition that takes a large
struct by value. The current tree has no STRUCT findings. `--structs` lists only
`EndouApplyModelLighting` (static inline, consistent). A by-value struct passed through a
function-pointer cast is not visible to `-aux-info`.

## 5. How far it gets

`XSG_PAD='300:START,11000:CIRCLE*500@120'`, `XSG_FAST=1`, under gdb with the STAGE7 §4
dprintfs:

* The opening KOS-MOS/Shion dialogue runs, with page breaks advanced by CIRCLE.
* `ST0210.evt` "Virtual Tutorial": `/[waitkey(64)]/[clear()]` ends at the first CIRCLE.
  The next text follows ("During quests, you'll trigger a battle when you come in contact with
  an enemy. Here you'll need to talk to KOS-MOS..."), and the window closes after its
  `/[waitkey(64)]/[close()]` (vblank ~22400).
* The game is then **in the first field (player control)**. The screenshots show the
  first rendered UI: the message window with text and cursor at vblank 22000, and the field
  minimap frame afterwards. The 3D scene is still black (VU0 work, STAGE7_VU0.md).
  No crash to vblank 35,000+. The tutorial waits for the player to walk to KOS-MOS and talk.
* Movement test (`...,23000:UP:240*20@600,23300:CIRCLE*40@300,23400:LEFT:60*20@600`,
  1500 s timeout): no crash up to vblank 32,298 (timeout). Every thread is in its normal
  per-frame sleep. No new message or file load follows, so blind walking did not reach
  KOS-MOS. The screen shows only the minimap frame.

## 6. Next

* Field movement: without visible geometry, steering to KOS-MOS is blind. A trace of the player
  actor's position (and the "talk" trigger check) would show whether `UP`/`LEFT` move
  Shion at all, and where KOS-MOS stands.
* 3D in the field is black, while the minimap and UI render (VU0/VU1 work).

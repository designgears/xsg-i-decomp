# First-run setup: the disc image

The only thing a player needs is their own disc image of Xenosaga Episode I (USA,
SLUS-20469).  When the game starts without a usable one, it shows a setup screen
instead of the game, saves the image the player picks as `[game] iso` in
`~/.config/xsg/settings.ini`, and boots.  No terminal, no editing settings.

![setup](screenshots/setup_found.png)

## What counts as usable

`port/runtime/xsg_iso_check.h` (header-only C, used by both processes, so they
always agree):

| check | message |
|---|---|
| no `[game] iso` and no `*.iso` in `<game_dir>/iso` | No disc image found |
| the configured path is gone | The disc image set before is gone (`<path>` was moved or deleted) |
| not a regular file / cannot open | `<file>` is not a file / Cannot read `<file>`: errno |
| no ISO9660 PVD at sector 16 (2048-byte sectors) | not a disc image (no ISO9660 volume) |
| PVD at 16 x 2352 (+16 / +24) | raw 2352-byte sectors (.bin/.cue): convert it to an .iso |
| no `SYSTEM.CNF` in the root | not a PlayStation 2 disc |
| `BOOT2` names another executable | This is not Xenosaga Episode I (SLUS-20469): it boots `SLES_...` |
| layer 0 longer than the file, or no `SLUS_204.69` | Xenosaga Episode I but incomplete (truncated, or a bad dump) |
| all good | Found: `<file>` (`7.9 GB`) |

The extension does not matter: a `.bin` / `.img` with 2048-byte sectors is accepted.
Paths with spaces and non-ASCII characters work (Latin-1 characters render in the
screen's font; others show as `?`, but the path itself is kept exactly).

## Flow

1. `host_main.c main()`: after the PS2 memory map, `xsg_disc_setup()` (disc.c)
   resolves the image as `xsg_disc_init` does (`[game] iso`, else the first `*.iso`
   by name in `<game_dir>/iso`) and checks it.  OK: nothing happens.
2. Otherwise it starts xsg-gpu early (`gs_bridge.cpp xsg_gs_start_gpu`, the same
   start the first GS call would do) and sends `GPU_MSG_SETUP` (gpu_ipc.h:
   `GpuSetupReq` = reason, the path tried, the folder searched).  The game waits for
   the reply (`gpu_client.cpp xsg_gpu_setup`).
3. xsg-gpu (`xsg_gpu.cpp Renderer::Setup`) runs its own ~60 Hz loop: SDL events,
   `xsgmenu::Frame` (which draws only the setup screen while it is active), present.
   **Choose disc image…** opens `SDL_ShowOpenFileDialog` (the XDG portal on Linux;
   its callback may come on another thread and is handed to the main thread under a
   mutex); `SDL_EVENT_DROP_FILE` takes a file dropped on the window.  Each pick is
   checked at once; **Continue** is enabled only for a good image.  Mouse, keyboard
   (arrows / Tab, Enter, Ctrl+O) and gamepad (D-pad / stick, A, Y = choose) all work.
4. Continue: the image is checked again, saved with `xsgcfg::Set("game.iso")` +
   `SaveNow()` (the gpu side is the only writer), and replied (`GpuSetupReply`).
   The game checks it once more, reloads settings.ini and continues: `xsg_disc_init`,
   then `xsg_load_main_image`, then everything else.  Nothing reads the disc before.
5. Quit, or closing the window: xsg-gpu replies "quit" (the game `_exit(0)`s) or
   exits (the game's `CheckChild` exits with it).

The next start finds `[game] iso` and goes straight to the game.  If that file is
later moved or deleted, the setup screen comes back saying so.

## Only the ISO is needed

The main executable's segments (`xsg_load_main_image`) come from `[game] elf`, else
`<game_dir>/extracted-iso/SLUS_204.69`, else **`SLUS_204.69` read from the disc image**
(`disc.c xsg_disc_read_file`; byte-identical to the extracted file).  No other runtime
code reads `extracted-iso/` or `game_dir`, except the data fill: the bytes of the
game's data (SLUS_204.69, OV*.OVL, IOP/SSD.IRX, IOP/RSSD.IRX) are copied from
`extracted-iso/` when present, else from the disc image ([DATA.md](DATA.md)).  The
build needs no game files.

## Headless and test configs

`[debug] headless = 1`, or `[renderer] gs = soft` (no xsg-gpu window): no setup
screen; the game logs the reason and where to set `[game] iso`, then carries on as
before.  Configs that set `[game] iso` to a good image never see setup.

## Testing

Test hooks in `[debug] menu_test_set` (MENU.md): `SETUP_DROP=<path>` as a dropped
file, `SETUP=continue|quit|choose` as the buttons; vsyncs count from the screen's
open.  A fresh profile: `env HOME=/tmp/h XDG_CONFIG_HOME= ...` with
`[game] game_dir = <empty folder>` in `/tmp/h/.config/xsg/settings.ini`.

Verified 2026-11-12 (Hyprland, workspace 9): setup shown, a wrong ISO (SLES boot)
rejected, the real image (via a symlink `My Games/Xenosaga ü disc.img`) found,
Continue boots the game; the next start skips setup; with an empty `game_dir` (no
extracted-iso) SLUS comes from the ISO and the game reaches the first field scene
(cf0210, MC_VOK21), as with extracted-iso; a moved image shows "set before is gone";
Quit exits both processes; headless runs unchanged.

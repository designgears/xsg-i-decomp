# Packaging: one AppImage (Linux), one zip (Windows)

`port/packaging/make_appimage.sh` packs the built port (`port/build`) into
`Xenosaga_Episode_I-x86_64.AppImage`, which runs on a typical x86-64 Linux desktop
with Vulkan, including one with no 32-bit (multilib) libraries installed.

## Your copy only

`xsg` and `xsg-gpu` contain code and data built from **your** copy of the game:
the decompiled code, plus data split from the game's executable and IOP modules.
The AppImage is for your own use. Don't distribute it. It contains no disc image
and no extracted game files. The game reads your ISO at run time (`[game] iso` in
`~/.config/xsg/settings.ini`, or the first-run setup screen). The script refuses to
pack anything that looks like game data (`*.iso`, `*.bin`, `*.img`, `*.7z`,
`SLUS_*`, `*.irx`, `*.pss`, `*.elf`, or any file over 60 MB).
The one exception is `uncensored/3039C_un.pss` (below), which you add yourself.

## The uncensored 3039C scene

`[game] uncensored` (Settings → Game → Uncensored 3039C, MENU.md) plays `3039C_un.pss` in
place of the disc's `movie\mpeg2\3039C.pss`. Nothing else can be swapped. The file must be a
PS2 `.pss` like the disc's (MPEG-2 video, SShd ADPCM audio). Ours is the Japanese release's
uncut 3039C with its audio replaced by the US release's English: the picture, which carries
Japanese subtitles, is untouched, so it plays exactly as the disc's movie does.

`3039C_un.cap` next to it holds the English subtitles (UTF-8, the layout of the game's own movie
captions: `m:s:f<tab>text`, time in pictures at 30 a second, `\` a line break). With `[game]
uncensored_subs` on, `GameMpeg2Play` opens them through `disc.c xsg_movie_caption_open` and reports its
picture count every frame (`xsg_movie_caption_frame`); the current line goes to xsg-gpu (`GpuMouse
g_caption`), which draws it centred at the top of the picture in the rebuilt UI's font. The game's own
caption printer is not used: no US movie has captions, and its font draws English spaces and
punctuation as Japanese glyphs.

* Put it in `game/uncensored/3039C_un.pss` (git-ignored, like all of `game/`; or
  `$XSG_GAME_DIR/uncensored`). Both packaging scripts pack that folder as `uncensored/` at
  the package root: the AppImage's root, or next to `xsg.exe` in the zip. Without the file
  the folder is packed empty. The game-data refusal skips exactly that one path.
* The game looks for it (`disc.c xsg_cd_stream_override`) in `[game] uncensored_dir` when
  set, else in `<dir of the executable>/uncensored` (zip), `<dir of the executable>/../../uncensored`
  (AppImage: `/proc/self/exe` is the loader in `usr/bin`), then `<game_dir>/uncensored` (dev
  builds). Turning the toggle on with no file found logs where it looked and plays the disc's.
* A package with the file in it contains game data from another release of the game: keep it
  to yourself like the rest of the package.


## Build it

```sh
cmake --build port/build -j16              # or pass --build (takes /tmp/xsg-build.lock)
port/packaging/make_appimage.sh            # -> ./Xenosaga_Episode_I-x86_64.AppImage
port/packaging/make_appimage.sh --out /tmp/x.AppImage --keep-appdir
```

The script downloads two tools on first use to `.work/tools/appimage/` (not tracked)
and checks their sha256: appimagetool **1.9.1** and the type2 runtime
**20251108**, both from the official AppImage GitHub releases. The AppDir is built
in `.work/appimage/AppDir` and removed afterwards unless you pass `--keep-appdir`.
Packaging needs the host's 32-bit glibc (`lib32-glibc`, `lib32-gcc-libs`), because
the libraries are copied from there. Running the AppImage doesn't need them.

Without FUSE, run it with `--appimage-extract-and-run` or `APPIMAGE_EXTRACT_AND_RUN=1`.

## What's inside

```
AppRun                          runs the bundled 32-bit loader on usr/bin/xsg
usr/bin/xsg                     the game (i386, non-PIE, debug info stripped)
usr/bin/ld-linux.so.2           32-bit glibc loader (next to xsg on purpose, see below)
usr/lib32/                      32-bit libc, libm, libstdc++, libgcc_s, + libpthread/libdl/librt/...
usr/bin/gpu/xsg-gpu             shell wrapper: the path the game spawns
usr/libexec/xsg/xsg-gpu         the real renderer (x86-64, debug info stripped)
usr/lib64/libSDL3.so.0          always used
usr/lib64/cxx/                  libstdc++, libgcc_s (+ VERSION: highest GLIBCXX_)
usr/lib64/glibc/                64-bit loader + libc set (+ VERSION)
usr/share/xsg/prompts/          input prompt icons (CC0), see "Known gaps"
uncensored/                     3039C_un.pss + 3039C_un.cap when game/uncensored has them (see above), else empty
xsg.desktop, xsg.png, .DirIcon  original icon (port/packaging/xsg.svg), no game art
```

Debug info is stripped from the bundled copies only (`strip --strip-debug`, which
keeps the symbol tables for backtraces). `port/build` keeps `-g`. The AppDir is 63 MB
before stripping and 29 MB after, and the AppImage (zstd squashfs) is about 13 MB.

Vulkan is never bundled: libvulkan, the ICDs and the GPU driver always come from
the host. Neither is ffmpeg (movies) nor a 32-bit libasound (the game's optional
ALSA path, normally unused because audio goes through xsg-gpu/SDL3).

## How it runs

**The game (32-bit).** AppRun runs
`usr/bin/ld-linux.so.2 --library-path usr/lib32 --argv0 xsg usr/bin/xsg`, so no host
32-bit library is used and no `LD_LIBRARY_PATH` is set. Its children (xsg-gpu,
ffmpeg) get the user's environment unchanged. Started this way, `/proc/self/exe` is
the loader, not `xsg`. That's why the loader sits in `usr/bin`:
`gpu_client.cpp DefaultBin()` resolves `<dir of /proc/self/exe>/gpu/xsg-gpu`, which
is `usr/bin/gpu/xsg-gpu`. "Restart now" (`Restart()`) `execv`s `/proc/self/exe`
with `/proc/self/cmdline`, which is the loader with its full argument list, so it
runs the same command again.

**The renderer (64-bit).** `usr/bin/gpu/xsg-gpu` loads host libraries into its own
process: libvulkan and the driver, plus whatever SDL3 opens (Wayland/X11, PipeWire).
Those need a glibc at least as new as the one they were built against, so the
wrapper picks the newer glibc:

* glibc: the host's (`/lib64/ld-linux-x86-64.so.2`) when its version is at least
  the bundled one, else the bundled loader and libc set in `lib64/glibc`;
* libstdc++/libgcc_s: the bundled pair unless the host's libstdc++ has a higher
  `GLIBCXX_` version;
* SDL3: always the bundled one.

It also uses `--library-path`, never `LD_LIBRARY_PATH`, and sets `SDL_APP_ID=xsg-gpu`
(the window class) unless you've already set it. For testing,
`XSGPKG_GLIBC=host|bundled` forces the glibc choice, and `XSGPKG_DEBUG=1` logs it.
These variables are read by the wrapper only, not by the port. The port reads only
`XSG_CONFIG` and warns about any other `XSG_*` variable.

The binaries are built on the packaging host (here Arch, glibc 2.44). They need
`GLIBC_2.43`, which the bundled glibc provides on older hosts.

## Tested (2026-11)

* Arch/Hyprland, NVIDIA + RADV: reaches the title. The game maps only bundled
  libraries (`/proc/PID/maps`).
* The same desktop under `bwrap --tmpfs /usr/lib32`, so there's no host 32-bit
  loader or libraries, with `XSGPKG_GLIBC=bundled`: reaches the title. `LD_DEBUG=libs`
  shows no host lib32 path.
* `debian:bookworm-slim` container (glibc 2.36, no lib32, no Vulkan): the game boots
  from the ISO to `main()`, and xsg-gpu starts on the bundled glibc 2.44, then stops
  at "no Vulkan loader" as expected.

## Known gaps

* **Input prompt icons**: `port/gpu/ui/prompts.cpp` loads PNGs from
  `XSG_PROMPTS_DIR`, an absolute source-tree path compiled in
  (`port/gpu/CMakeLists.txt`). On another machine they're missing, and the UI
  falls back to no icon. The icons are already in `usr/share/xsg/prompts`, but
  nothing reads them there. To fix it, embed them as `.inc` like the fonts, or have
  `Load()` try `$APPDIR/usr/share/xsg/prompts` first.
* `ps`/`top` show the game as `ld-linux.so.2` and the renderer as
  `ld-linux-x86-64.so.2` (the loader is the executable).
* The `XSG_GPU_BIN` column in `xsg_settings_table.h` is no longer read. Only
  `[debug] gpu_bin` in the ini overrides the renderer path.


## Windows zip

`port/packaging/make_windows_zip.sh` packs the MinGW cross build ([WINDOWS.md](WINDOWS.md)) into
`Xenosaga_Episode_I-Windows.zip`. The same rules apply as for the AppImage: your copy only, and no
game data (the same refusal list, plus `*.ovl`, with the same `uncensored/3039C_un.pss` exception).

```sh
XSG_GAME_DIR=$PWD/game port/packaging/make_windows_zip.sh --build   # configure+build port/build-win32-pkg (XSG_PACKAGED=ON)
port/packaging/make_windows_zip.sh --out /tmp/x.zip --keep-dir      # pack an existing build
```

```
Xenosaga_Episode_I/
  xsg.exe                 stub at image base 0x10000 (reserves PS2 RAM; icon + version resource)
  xsg.dll                 the game and runtime (32-bit)
  gpu/xsg-gpu.exe         the renderer (64-bit; icon + version resource)
  gpu/SDL3.dll            SDL 3.4.20 (official MinGW release)
  share/xsg/prompts/      input prompt icons (CC0), found relative to xsg-gpu.exe
  uncensored/             3039C_un.pss + 3039C_un.cap when game/uncensored has them, else empty
  README.txt
```

* The MinGW runtime (libgcc, libstdc++, winpthreads) is linked statically, so no runtime DLL is
  needed. The script still reads every binary's imports (`objdump -p`). Any DLL the MinGW toolchain
  ships (`libstdc++-6.dll`, `libgcc_s_dw2-1.dll` / `libgcc_s_seh-1.dll`, `libwinpthread-1.dll`) is
  copied from the matching 32-bit or 64-bit toolchain; everything else is a system DLL or the UCRT.
* Binaries are stripped of debug info (`strip --strip-debug`; symbol tables stay for crash
  addresses): 77 MB staged becomes 19 MB, and the zip is about 7 MB.
* `-DXSG_PACKAGED=ON` keeps source-tree and build-tree paths out of the binaries (checked:
  `strings` finds none). That means no default game dir, so a fresh install shows the setup
  screen, and the icons are found next to the executable.
* The AppImage is still built from `port/build`, which compiles in the repo's game dir and
  prompt path. Configuring that tree with `-DXSG_PACKAGED=ON` would make the AppImage
  path-free too (prompts then come from `$APPDIR`).

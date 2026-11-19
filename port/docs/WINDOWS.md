# Windows port (phase 1: cross-built, runs under Wine)

Branch `pc-port`, 2026-11-12. Both processes cross-compile on Linux with MinGW-w64 (gcc 16.2):
the game is 32-bit (`xsg.exe` + `xsg.dll`), the renderer is 64-bit (`gpu\xsg-gpu.exe` + `SDL3.dll`).
Under Wine 11.20 (winevulkan on the NVIDIA driver) the build boots to the title screen and plays
into the first field scenes with scripted input. Nothing has been run on real Windows yet.

## Status

| milestone | result |
|---|---|
| M1 link | `port/build-win32/xsg.exe` (stub, 5 KB), `xsg.dll` (21 MB), `gpu/xsg-gpu.exe` |
| M2 boot | PS2 RAM mapped at 0x10000 with its 0x2/0x3xxxxxxx alias views, ISO opened, SLUS segments copied, `xsg-gpu.exe` started with a Vulkan window |
| M3 title | software GS: frames 300-1500 are pixel-identical to the Linux build. xsg-gpu: `screenshots/windows_wine_title.png` |
| M4 field | `room = field` script reaches ev1000 → cf0210 → cf9051 → cf1210 (MC_KOU01) at 60 fps: `screenshots/windows_wine_field.png` |

## Building

```sh
# prerequisites: the PS2 build is configured (build/<unit>/ exists), and the game dir holds
# extracted-iso/IOP/{SSD,RSSD}.IRX (the sound driver's tables are generated from them at build time)
cmake -S port -B port/build-win32 -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/port/cmake/mingw-i686.cmake \
      -DXSG_GAME_DIR=$PWD/game \
      -DXSG_PGS_SOURCE_DIR=$PWD/.work/win/parallel_gs-src    # optional: a local paraLLEl-GS checkout
ninja -C port/build-win32
```

* The top-level project builds the game with `mingw-i686.cmake`. Its ExternalProject builds
  `port/build-win32/gpu/xsg-gpu.exe` with `mingw-x86_64.cmake`, and copies `SDL3.dll` next to it.
* SDL3 is the official MinGW devel release, **3.4.20**
  (`SDL3-devel-3.4.20-mingw.tar.gz`, sha256 `d2d3bf3f…cd9ed`). It is unpacked in
  `.work/tools/SDL3-3.4.20/`, outside git; override the location with `-DXSG_SDL3_DIR=<…>/x86_64-w64-mingw32`.
* Shaders are compiled by the build machine's `glslc`, as on Linux.
* The MinGW runtime (libgcc, libstdc++, winpthreads) is linked statically. The executables import
  only system DLLs, the UCRT, winmm and `SDL3.dll`. The Vulkan loader (`vulkan-1.dll`) is loaded at
  run time.
* The xsg-gpu project also builds on its own:
  `cmake -S port/gpu -B port/build-win64 -DCMAKE_TOOLCHAIN_FILE=$PWD/port/cmake/mingw-x86_64.cmake`.

Linux is unchanged. Every Windows path is under `if(WIN32)` / `#ifdef _WIN32`, or in files only
the Windows build uses. The Linux build also needs `XSG_GAME_DIR`'s `extracted-iso/IOP/*.IRX` at
build time (the default is `<repo>/game`). If that directory is missing, `iop_sound_data.S` has no
rule. Point `-DXSG_GAME_DIR` at the real folder.

## Running under Wine

```sh
export WINEPREFIX=$PWD/.work/wine WINEDEBUG=-all
XSG_CONFIG='Z:\path\to\settings.ini' wine port/build-win32/xsg.exe
```

* Use Windows-form paths in settings.ini, e.g. `game_dir = Z:\srv\…\game`.
* Default locations: settings in `%APPDATA%\xsg\settings.ini`, memory cards in
  `%LOCALAPPDATA%\xsg\mc0`, the pipeline cache in `%LOCALAPPDATA%\xsg\cache`, screenshots in
  `%USERPROFILE%\Pictures\xsg`. `XSG_CONFIG` still overrides the settings file.
* `.work/win/bin/winrun.sh NAME.ini` launches `.work/win/runs/NAME.ini` on Hyprland workspace 9.
* **Hidden workspace:** Hyprland throttles a window on a workspace nobody is viewing, and Wine's
  Vulkan present then blocks for about 1 s per frame (`[prof]` shows `present 1000 ms`). Test
  configs use `[display] present = immediate`.

## Design

### PS2 RAM at fixed low addresses

The decompiled code uses PS2 addresses as host pointers. It needs EE RAM at
0x10000-0x2000000, plus two aliases of the same memory at 0x20000000 and 0x30000000 (the
uncached views, e.g. sef's `atanTbl_0 = 0x2040FAC0`).

In a Windows process, the heap, the main stack and the NLS tables land at low addresses
(0x110000 and up) before any of the program's code runs. Wine already places them before
`CREATE_SUSPENDED` returns, so a launcher that does `VirtualAllocEx` on a suspended child fails
with error 487. Only the main executable's image is mapped earlier than those. Hence:

* **`xsg.exe` is a stub** (`runtime/win32/stub.c`):
  * no CRT, no relocations, image base **0x10000**;
  * a 32 MB bss array makes its image span the whole RAM range;
  * it imports `xsg.dll`, and its entry point calls `xsg_win32_start`.
* **`xsg.dll`** holds the game and runtime. It is linked at a fixed **0x40000000** with no
  relocation section, so the absolute `--defsym` PS2 addresses can never be rebased.
* **`xsg_win32_start`** (`runtime/win32/platform_win32.c`):
  * notes the exe path, sets binary file mode and `timeBeginPeriod(1)`, then runs the usual `main()`;
  * when that main maps memory, it unmaps the stub's image (`NtUnmapViewOfSection`) and maps one
    pagefile-backed section three times: at 0x10000, 0x20010000 and 0x30010000, the same as the
    Linux memfd.
  * Nothing else runs at that moment, so nothing can take the range in between.
  * EE hardware, VU, GS privileged registers and the scratchpad come from `VirtualAlloc` at their
    fixed addresses.
  * Failures print which allocation is in the way.
* Proven under Wine: Wine put its own allocations above 0x2010000, and writes through 0x200000 are
  read back at 0x20200000.

### Linking (`scripts/link_xsg.py --target pe`)

The ELF pipeline is reused as-is: per-unit `ld -r`, globalized statics, overlay tables, localized
overlay clashes, defsyms, trampolines and autostubs. It runs on COFF objects with the
`i686-w64-mingw32-` binutils. PE-specific steps:

* **Underscores.** i386 COFF prefixes C names with `_`. Names read from `nm` drop it, and names
  written to symbol lists, `--defsym` and the generated assembly get it back (`sym()`).
* **`__asm__("name")` aliases** are emitted verbatim by GCC. They are renamed to `_name`: every
  bare identifier, plus `_`-led names found in the sources.
* **The data assembly from `gen_host_data.py`** is rewritten (`coff_source()`): `.type`/`.size`
  dropped, `"dw"`/`"bw"` section flags, prefixed symbols.
* **No weak definitions** (`scripts/coff_weak_defs.py`). A COFF weak external points at its default
  by symbol index, and `ld -r` does not renumber that index; `RssdWork` resolved into `.text`. A weak
  label that the C defines becomes a local label; any other becomes a plain global.
* **The IOP sound driver** is compiled with `-fno-leading-underscore` and prefixed `_ssdiop_`
  through `--redefine-syms` (`scripts/coff_prefix_symbols.py`). `objcopy --prefix-symbols` also
  renames COFF section symbols, which breaks relocations against static data.
* **Smaller fixes:**
  * The PS2 `main` overlay-table entry maps to `xsg_host_main`.
  * `__executable_start` is defined as the DLL's `__ImageBase`, and `etext` comes from the default
    script.
  * `bcopy` is supplied, since only glibc has it.
  * `gen_vu0_microcode.py` emits `XSG_SYM()`-prefixed `.set` labels.
  * `gen_iop_sound_data.py` guards its GNU-stack note with `#ifdef __ELF__`.
* **Linux i386 ABI for game code.** Game C and the IOP driver are compiled with `-mno-ms-bitfields
  -mno-align-double -fpcc-struct-return`, so `long long`/`double` struct members are 4-aligned,
  bitfields use the GCC layout, and structs are returned in memory. The runtime keeps the MinGW
  defaults, because it shares `struct stat` and similar structs with the CRT; runtime C and C++
  only add `-mno-ms-bitfields`.
* **`-faligned-new=8` for the runtime C++.** The 32-bit CRT's malloc aligns to 8, but GCC assumed
  16 for `operator new`; `vu1.cpp`'s 16-byte `DP` faulted on `movaps`.

Result: 502 overlay symbols scoped, 87 defsyms, 76 trampolines, 1 function stub and 2 data stubs,
the same counts as the Linux link.

### Platform layer

* `runtime/win32/xsg_win_compat.h` is force-included into the runtime. It covers `mkdir`,
  64-bit `pread`, `pipe2`, `O_CLOEXEC`, `fcntl`, `gmtime_r`, `aligned_alloc`, and a
  **high-resolution `clock_nanosleep`/`nanosleep`**: a high-resolution waitable timer, then a
  yield loop for the last millisecond. This drives vblank pacing and the sound clock.
* `runtime/win32/include/` holds shim headers (`dlfcn.h`, `execinfo.h`, `elf.h`, `fnmatch.h`,
  `spawn.h`, `sys/wait.h`). `platform_win32.c` implements them: `backtrace` via
  `RtlCaptureStackBackTrace`, `posix_spawn`/`waitpid`/`kill` over `CreateProcess` (used to
  start xsg-gpu), and `fnmatch`.
* **EE threads** (`kernel.c`) are fibers instead of ucontexts: 1 MiB reserve,
  `FIBER_FLAG_FLOAT_SWITCH`. The calls go through `platform_win32.c`, because `<windows.h>` clashes
  with the EE kernel's names (`CreateThread`, …).
* **Crashes:** a vectored exception handler logs the exception, `eip`, `esp`, `[esp]` and a
  stack scan of `.text` words. Resolve them with `addr2line -f -e xsg.dll`.
* **`[debug] timeout`** runs on a watchdog thread.
* **xsg-gpu IPC** (`runtime/xsg_win32_ipc.h`, shared by both processes):
  * The shared memory is an inheritable pagefile mapping. Its handle value is `argv[1]` (Linux
    passes fd 3), with `--parent=<pid>`.
  * Windows has no cross-process futex, so each shared word gets a named auto-reset event,
    `Local\xsg-gpu-<pid>-<offset>`. The waiter-flag protocol is unchanged.
  * xsg-gpu runs in a kill-on-close job object, which replaces `PR_SET_PDEATHSIG`. It also checks
    the parent process handle, which replaces `getppid()`.
  * The `gpu_ipc.h` layout is unchanged: only 32-bit fields, so the static_asserts still hold
    on Win64.
* **Paths:** `settings.c`, `memcard.c`, `menu/cfg.cpp`, `ui/art.cpp`, `menu/menu.cpp` and the
  pipeline cache use the folders listed under Running. `rename()` over an existing file goes
  through `MoveFileEx` on the xsg-gpu side.

### Not on Windows (logged instead)

* the SIGPROF sampling profiler (`[debug] prof`);
* ALSA output (`audio.c`: sound goes through xsg-gpu and SDL3 anyway);
* the `[debug] stream` FIFO;
* DLSS 5 neural rendering.

## Game bug found by this port

`InitUwamono` (`src/main/init_uwamono_sys.c`) passes a 0x3C-byte local `InitUwamonoQuery` to
`UnduParamInit`, which fills a whole 0x4C-byte `UnduQuery`. The 64-bit `includedAttributes` ends 4
bytes past the copy. On Linux that lands in frame padding; MinGW's frame keeps the saved `ebx`
there, so `MAP_updateUnit` crashed when MC_KOU01 loaded. The struct is padded under `PORT_HOST`
only; the PS2 build is untouched. 19 other `UnduParamInit` callers have not been audited.

## Packaging (phase 2): `Xenosaga_Episode_I-Windows.zip`

`port/packaging/make_windows_zip.sh --build` configures and builds `port/build-win32-pkg` with
`-DXSG_PACKAGED=ON`, then packs `<repo>/Xenosaga_Episode_I-Windows.zip` (about 7 MB). See
[PACKAGING.md](PACKAGING.md#windows-zip). `XSG_PACKAGED` compiles no source-tree or build-tree path
into the binaries:

* the default game dir is empty, so a fresh install always shows the first-run setup screen;
* the prompt icons are found relative to `xsg-gpu.exe` (`ui/prompts.cpp`): `<exe dir>/../share/xsg/prompts`;
* the shot dir defaults to `shots`.

`XSG_GAME_DIR` then only tells the build where to read the IOP modules.

The stub and xsg-gpu.exe carry an icon (`port/packaging/xsg.svg` rendered with ImageMagick) and a
version resource (`port/packaging/windows/xsg.rc.in`, `resources.cmake`). The `.rsrc` section sits
after the stub's reservation (0x2001000).

**Tested under Wine** with a fresh prefix (`.work/win/prefix-zip`) and the zip unpacked to a fresh
folder:

* No settings: `[setup] No disc image found`, and the setup screen appears
  (`screenshots/windows_zip_setup.png`). "Choose disc image…" opens the Windows common file dialog
  through SDL (`screenshots/windows_zip_file_dialog.png`), starting in `%USERPROFILE%`. The start
  folder is passed with backslashes; with forward slashes the dialog took it as a file name.
* With `[game] iso` set in `%APPDATA%\xsg\settings.ini`: boots to the title
  (`screenshots/windows_zip_title.png`). SLUS_204.69 is read from the disc image. The memory card
  is in `%LOCALAPPDATA%\xsg\mc0` and the pipeline cache in `%LOCALAPPDATA%\xsg\cache\pipeline_cache.bin`.

## Left for phase 2 (packaging) and later

* **Done in phase 2:** the zip, the exe-relative prompt icons, and the setup screen with the
  native file dialog (above).
* **Setup message:** with an empty default game dir, the setup screen says "No disc image found
  in /iso". `disc.c resolve_iso` should skip the `<game_dir>/iso` search, and leave `search_dir`
  empty, when the game dir is empty. `disc.c` is being edited in another session, so this change
  is left to it.
* **Real Windows:**
  * confirm the 0x10000 image base, and that 0x70000000 (scratchpad) and 0x10000000-0x12000000
    are never taken by system DLLs under WOW64;
  * Defender may scan the 21 MB DLL;
  * code-signing.
* **Open:** audit the other `UnduParamInit` callers' local query sizes, and mixed-layout structs
  between game and runtime if any carry `long long`/`double` fields.

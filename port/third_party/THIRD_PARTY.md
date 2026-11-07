# Third-party code used by the PC port

| component | licence | how it is used | where |
|---|---|---|---|
| ps2-port-engine GS | MIT | vendored, built into the game (`xsg`) as the software GS; `gs_memory.cpp` is also compiled into `xsg-gpu` to decode local memory | `ps2-port-engine-gs/` (see its own THIRD_PARTY.md) |
| paraLLEl-GS (Arntzen Software AS, Hans-Kristian Arntzen) | LGPL-3.0-or-later | **not vendored.** Fetched at configure time by `port/gpu/CMakeLists.txt` (FetchContent, pinned to commit `cc6184af7e0c03da603045ca371ffa5dae9b0655`) and linked only into the separate renderer executable `xsg-gpu` | https://github.com/Arntzen-Software/parallel-gs |
| Granite (Hans-Kristian Arntzen), paraLLEl-GS's submodule, and the libraries it bundles (volk, Vulkan headers, …) | MIT (Granite); their own permissive licences | fetched with paraLLEl-GS, linked only into `xsg-gpu` | paraLLEl-GS's `Granite/` submodule |
| SDL3 | Zlib | system library (`pkg-config sdl3`), linked only into `xsg-gpu` for its window | system |
| FFmpeg | LGPL/GPL | separate executable over pipes, not linked (Stage 8) | user's `ffmpeg` |
| ico-pc (the ico decompilation contributors) | MIT | Stage 10: the SPU2 hardware tables (4-point Gaussian interpolation table, reverb FIR, the ten libsd reverb presets; transcribed there from psx-spx) copied into `port/runtime/spu2_tables.c` with the licence notice kept. The rest of the software SPU2 (`spu2.c`) is our own code | https://github.com/nathanialf/ico-pc, `port/audio/spu2_tables.c` |
| ps2-port-engine (Daniel Gomes Vieira) | MIT | Stage 10: libsd's ten reverb presets and work-area sizes (`kEffectParams`/`kEffectSizes`), copied into `port/runtime/spu2_libsd_tables.c` with the licence notice kept; the echo/delay rewrite of the delay and feedback registers follows its `Spu2::SetEffectAttr` | https://github.com/ (ps2-port-engine), `host/src/spu2.cpp` |
| Dear ImGui 1.91.9 (Omar Cornut and contributors) | MIT | vendored unmodified in `imgui/` (core + `backends/imgui_impl_sdl3`), built only into `xsg-gpu` for the in-game menu (port/docs/MENU.md). The draw data is rendered through Granite by our `port/gpu/menu/menu_render.cpp` (same vertex layout and shader as `imgui_impl_vulkan`, which is not used: Granite owns the device, queue and swapchain) | https://github.com/ocornut/imgui, `imgui/LICENSE.txt` |
| TotK Recomp (Valiantsin Dzerakh) | MIT | the in-game menu's design and parts of its code are adapted from `runtime/ui/menu.cpp`, `option_rows.{h,cpp}`, `menu_sdl.cpp`, `binds_page.cpp` and `runtime/cfg.h` (two-pane option rows, default / after-restart / env notes, stepping with hold speed-up, binding cells, stick-to-D-pad hysteresis, the F10 overlay, the per-tab screenshot hook, the settings table with env > ini > default and a debounced save). Attribution in the headers of `port/gpu/menu/*.cpp`. TotK Recomp's own option rows credit SavageWindRecomp (MIT, Revivedx and contributors) | https://github.com/valentderah/totk-pc-recomp (MIT, Copyright (c) 2026 Valiantsin Dzerakh) |
| Rajdhani (Indian Type Foundry) | SIL Open Font License 1.1 | the menu font: `port/gpu/menu/fonts/Rajdhani-{Medium,SemiBold}.ttf`, embedded into `xsg-gpu`; licence in `port/gpu/menu/fonts/OFL.txt` | Google Fonts, https://github.com/google/fonts/tree/main/ofl/rajdhani |

## The LGPL boundary (Stage 9)

* The game executable `xsg` (32-bit, MIT code plus the vendored MIT software GS)
  contains no paraLLEl-GS or Granite code. It talks to `xsg-gpu` only through a
  shared-memory command ring whose protocol is defined in our own header
  `port/runtime/gpu_ipc.h`.
* `xsg-gpu` (64-bit) is the only binary that links paraLLEl-GS. Its own source
  (`port/gpu/xsg_gpu.cpp`) is MIT; the combined executable is distributed under
  the LGPL's terms for the library part: the paraLLEl-GS source is the public
  upstream at the pinned commit, and anyone can rebuild `xsg-gpu` against a
  modified paraLLEl-GS with `-DXSG_PGS_SOURCE_DIR=/path/to/checkout`
  (or `-DFETCHCONTENT_SOURCE_DIR_PARALLEL_GS=` when building `port/gpu` alone).
* Nothing from paraLLEl-GS is committed to this repository. The fetched tree
  lives in the build directory.

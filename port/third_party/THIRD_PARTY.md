# Third-party code used by the PC port

| component | licence | how it is used | where |
|---|---|---|---|
| ps2-port-engine GS | MIT | vendored, built into the game (`xsg`) as the software GS; `gs_memory.cpp` is also compiled into `xsg-gpu` to decode local memory | `ps2-port-engine-gs/` (see its own THIRD_PARTY.md) |
| paraLLEl-GS (Arntzen Software AS, Hans-Kristian Arntzen) | LGPL-3.0-or-later | **not vendored.** Fetched at configure time by `port/gpu/CMakeLists.txt` (FetchContent, pinned to commit `cc6184af7e0c03da603045ca371ffa5dae9b0655`) and linked only into the separate renderer executable `xsg-gpu` | https://github.com/Arntzen-Software/parallel-gs |
| Granite (Hans-Kristian Arntzen), paraLLEl-GS's submodule, and the libraries it bundles (volk, Vulkan headers, …) | MIT (Granite); their own permissive licences | fetched with paraLLEl-GS, linked only into `xsg-gpu` | paraLLEl-GS's `Granite/` submodule |
| SDL3 | Zlib | system library (`pkg-config sdl3`), linked only into `xsg-gpu` for its window | system |
| FFmpeg | LGPL/GPL | separate executable over pipes, not linked (Stage 8) | user's `ffmpeg` |

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

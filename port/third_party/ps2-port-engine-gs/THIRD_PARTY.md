# ps2-port-engine software GS (vendored)

Source: https://github.com/danielgomesvieira2000/ps2-port-engine, commit
606a451c26994878c14e78605ac9bc164ca70688 (2026-10-31), directory `host/`.
License: MIT, Copyright (c) 2026 Daniel Gomes Vieira (see LICENSE in this directory).

Vendored unchanged:
* `include/ps2host/gs.h`
* `src/gs.cpp`, `src/gs_draw.cpp`, `src/gs_memory.cpp`, `src/gs_decode.h`

Written for the xsg port (not upstream code), standing in for upstream headers the GS
includes but whose implementations are not vendored:
* `include/ps2host/profile.h` - `Timed` as a no-op
* `include/ps2host/dma.h` - only `g_dma_source` (defined in port/runtime/gs_bridge.cpp)
* `include/ps2host/gs_blend.h` - only `GsPicture`

Not vendored: upstream's GPU bridge, GS thread split, frame blending, recording replay.

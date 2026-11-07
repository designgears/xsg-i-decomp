/* Stage 10: keyboard/gamepad input in xsg-gpu, published to the game as a
   DualShock 2 state (GpuPad in port/runtime/gpu_ipc.h).  See
   port/docs/STAGE10_INPUT.md. */
#ifndef XSG_INPUT_H
#define XSG_INPUT_H

#include "gpu_ipc.h"

#include <string>

#ifdef XSG_GPU_SDL
union SDL_Event;
#endif

namespace xsginput {

/* Video options from the [video] section of input.ini (env vars win). */
std::string Option(const char* key, const char* env, const char* def);

/* Loads the config; `sdl` = SDL events are available (window, or the
   XSG_GPU_INPUT_TEST hook).  Without it the pad stays neutral. */
void Init(GpuPad* pad, bool sdl);
#ifdef XSG_GPU_SDL
/* Every SDL event; returns true when it was consumed as pad input. */
bool Event(const SDL_Event& e);
#endif
/* Once per game vblank, before the event pump: runs XSG_GPU_INPUT_TEST. */
void BeginFrame(uint32_t frame);
/* After the event pump: publishes the DS2 state to the shared block. */
void Publish();
/* SDL_PollEvent loop for runs without a window (test hook only). */
void PumpHeadless();

}  // namespace xsginput

#endif

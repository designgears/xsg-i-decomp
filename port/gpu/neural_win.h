/*
 * Windows: DLSS 5 Neural Rendering through the DLSS5VKLayer helper ([renderer] neural).
 *
 * On Linux the DLSS5VKLayer Vulkan layer intercepts the present and talks to its helper.  Windows
 * has no such layer, so xsg-gpu plays the layer's part itself: it starts the helper
 * (dlssnr_helper.exe, a separate AGPL program the player installs; it loads nvngx_dlssnr.dll) and
 * exchanges frames with it through the helper's shared-memory file.  Fail-open: any problem and the
 * game's own frame is shown.  The helper is never linked in: only the documented file layout is
 * used (neural_win.cpp).
 */
#pragma once
#include <cstdint>

namespace xsgneural {

/* Start the helper and open the exchange (once, at start, when [renderer] neural = 1). */
bool Start();
/* Live: the helper is up and has not been given up on. */
bool Active();
/* One frame: `in` (w x h RGBA8, display-referred) to the model; its answer into `out`.
   False: show the original (helper missing, warming up past the budget, or refused the frame). */
bool Process(const uint8_t* in, uint32_t w, uint32_t h, uint8_t* out);
/* [renderer] neural turned off/on live (the helper stays; frames pass through while off). */
void SetEnabled(bool on);
void Stop();

}  // namespace xsgneural

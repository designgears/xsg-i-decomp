/*
 * xsg-gpu native renderer, first slice (port/docs/NATIVE_RENDERER.md).
 *
 * XSG_GPU_NATIVE=1: the game sends the inputs of every set-0 strip batch
 * (GPU_MSG_NATIVE); this module tracks the GS register state from the GIF
 * stream, redraws those batches with its own Vulkan pipeline at
 * XSG_GPU_NATIVE_SCALE x the PS2 resolution, and composites them over the
 * paraLLEl-GS frame.  For now it does this on the vsyncs listed in
 * XSG_GPU_NATIVE_SHOT and writes the images to XSG_GPU_NATIVE_DIR.
 */
#ifndef XSG_GPU_NATIVE_H
#define XSG_GPU_NATIVE_H

#include <cstdint>

namespace Vulkan { class Device; }
namespace ParallelGS { class GSInterface; }

namespace xsgnative {

bool Init(Vulkan::Device* dev);   // false: XSG_GPU_NATIVE is off
bool On();
void Gif(uint32_t path, const uint8_t* data, uint32_t len);
void Batch(uint32_t kind, uint32_t nvert, const uint8_t* payload, uint32_t len);
void Vsync(uint32_t frame, ParallelGS::GSInterface& gs);

}  // namespace xsgnative

#endif

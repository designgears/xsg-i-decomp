/*
 * xsg-gpu native renderer, first slice (port/docs/NATIVE_RENDERER.md).
 *
 * XSG_GPU_NATIVE=1: the game sends the inputs of every set-0 strip batch
 * (GPU_MSG_NATIVE, ahead of the batch's own GIF output, which paraLLEl-GS
 * still draws); this module tracks the GS register state from the GIF
 * stream, redraws the displayed buffer's batches with its own Vulkan
 * pipeline at XSG_GPU_NATIVE_SCALE x the PS2 resolution, and composites them
 * over the paraLLEl-GS frame at every vsync (LiveImage, presented in the
 * window).  XSG_GPU_NATIVE_SHOT lists vsyncs whose diagnostic images go to
 * XSG_GPU_NATIVE_DIR.
 */
#ifndef XSG_GPU_NATIVE_H
#define XSG_GPU_NATIVE_H

#include <cstdint>

#include "image.hpp"

namespace Vulkan { class Device; }
namespace ParallelGS { class GSInterface; }

namespace xsgnative {

bool Init(Vulkan::Device* dev);   // false: XSG_GPU_NATIVE is off
bool On();
void Gif(uint32_t path, const uint8_t* data, uint32_t len);
void Batch(uint32_t kind, uint32_t nvert, const uint8_t* payload, uint32_t len);
void Vsync(uint32_t frame, ParallelGS::GSInterface& gs);
/* The live composite of the displayed buffer (TRANSFER_SRC_OPTIMAL, frame
   buffer size x Scale()), or null when this vsync had no native layer. */
const Vulkan::ImageHandle& LiveImage(uint32_t* fb_w, uint32_t* fb_h);
int Scale();

}  // namespace xsgnative

#endif

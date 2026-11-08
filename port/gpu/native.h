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
#include <functional>
#include <vector>

#include "image.hpp"

namespace Vulkan { class Device; }
namespace ParallelGS { class GSInterface; }

namespace xsgnative {

bool Init(Vulkan::Device* dev);   // false: XSG_GPU_NATIVE is off
bool On();
/* Live [renderer] native / native_scale (after a settings change). */
void Apply(ParallelGS::GSInterface& gs);
/* One GPU_MSG_GIF: tracked here, then handed to paraLLEl-GS (gs) by native_fx. */
void Gif(uint32_t path, const uint8_t* data, uint32_t len, ParallelGS::GSInterface& gs);
void Batch(uint32_t kind, uint32_t nvert, const uint8_t* payload, uint32_t len);
/* show = false: the renderer is behind and this vsync is not presented; the live
   composite (the expensive part) is skipped, the bookkeeping is not. */
void Vsync(uint32_t frame, ParallelGS::GSInterface& gs, bool show = true);
/* The live composite of the displayed buffer (GENERAL layout, frame
   buffer size x Scale()), or null when this vsync had no native layer. */
const Vulkan::ImageHandle& LiveImage(uint32_t* fb_w, uint32_t* fb_h);
int Scale();
/* A TEX0 texture decoded from the GS-memory mirror (RGBA8), and the mirror's upload generation. */
std::vector<uint32_t> DecodeMirrorTexture(uint64_t tex0, uint64_t texa, uint32_t* w, uint32_t* h);
uint32_t MirrorGen();
/* The newest upload generation of a TEX0 texture's pages (and its CLUT's). */
uint32_t TextureGen(uint64_t tex0);
/* A TEX0 texture decoded from a 4 MB GS-memory image (RGBA8). */
std::vector<uint32_t> DecodeVramTexture(const uint8_t* vram, uint64_t tex0, uint64_t texa, uint32_t* w, uint32_t* h);
/* fn(y0, y1) over [0, h) split across a few threads (frame decodes). */
void ParallelRows(uint32_t h, const std::function<void(uint32_t, uint32_t)>& fn);

}  // namespace xsgnative

#endif

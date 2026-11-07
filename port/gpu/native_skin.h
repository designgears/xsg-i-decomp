/*
 * xsg-gpu native renderer, set 2 (characters) and motion vectors
 * (port/docs/NATIVE_RENDERER.md §7, §8).
 *
 * The game runs the set-2 C references in place of VU1 and sends each vertex
 * batch's inputs (GPU_NATIVE_SET2: streams, constants, ADC bits) plus the bone
 * palette when it changes (GPU_NATIVE_SET2_PALETTE).  This module keeps them,
 * keeps last frame's matrix / palette / positions per draw key for motion
 * vectors (set 0 too), binds a draw for native_skin.vert, and resolves the
 * motion-vector target after the live composite (native_mv.comp).
 */
#ifndef XSG_GPU_NATIVE_SKIN_H
#define XSG_GPU_NATIVE_SKIN_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gpu_ipc.h"
#include "image.hpp"

namespace Vulkan { class Device; class CommandBuffer; class Image; class Buffer; }

namespace xsgskin {

struct Draw {
    GpuNativeSkin b;
    std::vector<GpuNativeSkinVertex> v;
    uint32_t nidx = 0;                      // the strip triangles the GS draws (ADC clear), as an index list
    uint64_t vofs = 0, iofs = 0, uofs = 0;  // vertices / indices / constants in the GPU arena (written once, at arrival)
    uint64_t pal = 0, ppal = 0;             // palette serials (GPU palette ring), this / last frame; 0 = none
    float pm[16];                           // last frame's object -> clip
    float pmirror = 0;
    bool prev = false, pmirror_on = false;
    int group = 0, variant = 0;
};

bool Init(Vulkan::Device* dev);
/* Variant of a set-2 loop: 0 lit, 1 one-pass specular, 2 second specular strip,
   3 env second strip; -1 for anything else. */
int Variant(uint32_t loop);
/* GPU_NATIVE_SET2 -> a draw; GPU_NATIVE_SET2_PALETTE -> nullptr (kept). */
std::shared_ptr<const Draw> Message(uint32_t kind, uint32_t nvert, const uint8_t* payload, uint32_t len);
/* Set 0 motion history: records this batch; when `out` is given and the key
   was drawn last frame, writes last frame's positions into out[i].pad and its
   matrix into mprev (returns true). */
bool Set0Prev(const GpuNativeBatch& b, const GpuNativeVertex* v, uint32_t n, GpuNativeVertex* out, float mprev[16]);
/* Start of a render pass (bound palettes are forgotten). */
void Begin();
/* Bind program, vertices, constants and indices for `pass` (0 = tag 1011,
   1 = specular strip, 2 = env strip).  Returns the index count. */
uint32_t Bind(Vulkan::CommandBuffer& cmd, const Draw& d, int pass);
/* After composite.comp wrote `out` (GENERAL): the motion-vector target. */
void ResolveMv(Vulkan::CommandBuffer& cmd, Vulkan::Image& out, Vulkan::Image& color, Vulkan::Image& depth,
               const Vulkan::ImageHandle& mv, const Vulkan::Buffer& gs, int scale, float zmul, bool own);
/* The resolved motion vectors of the last live composite (RG16F, high-res pixels). */
const Vulkan::ImageHandle& MvImage();
/* Diagnostic: <prefix>mv.png (colour view) and <prefix>mv.f32 (w, h, then RG floats) of the
   resolved vectors; <prefix>mvraw.* of the native render's own (before the resolve). */
void ShotMv(const std::string& prefix);
void Vsync(uint32_t frame);

}  // namespace xsgskin

#endif

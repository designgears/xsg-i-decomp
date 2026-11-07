/*
 * xsg-gpu native renderer: full-screen effects at high resolution and exact
 * pixel ownership (port/docs/NATIVE_RENDERER.md §7).
 *
 * A second GIF parser over the same stream as paraLLEl-GS decodes every
 * primitive with its GS state.  It drives:
 *
 *   - the ownership GS: a second paraLLEl-GS instance fed the same stream,
 *     except that native batches are drawn in a marker colour (fog forced on
 *     with F = 0 and FOGCOL = marker, blending and dithering off; Z, alpha
 *     test, scissor and masks unchanged).  A pixel is owned by the native
 *     layer exactly when it still holds the marker: the last write to it came
 *     from a native batch.
 *   - the effect replay: once a frame has drawn native batches into buffer F,
 *     the first later draw into F that samples F (or its Z buffer) as a
 *     texture, or covers most of the screen, starts the "post" phase.  The
 *     state of both GSes is snapshotted right before it (colour, Z, owner),
 *     and every later draw into F is recorded and redrawn at the native scale
 *     on the high-resolution composite (native_fx.comp), frame reads sampling
 *     the high-resolution surface.
 *   - XSG_GPU_FX_TRACE=v1,v2,..: a per-draw dump of those vsyncs (stderr).
 */
#ifndef XSG_GPU_NATIVE_FX_H
#define XSG_GPU_NATIVE_FX_H

#include <cstdint>
#include <vector>

#include "image.hpp"

namespace Vulkan { class Device; class CommandBuffer; }
namespace ParallelGS { class GSInterface; }

namespace xsgfx {

bool Init(Vulkan::Device* dev);
/* One GPU_MSG_GIF: parse it and hand it to paraLLEl-GS (gs) and to the
   ownership GS, split where a snapshot has to be taken. */
void Gif(uint32_t path, const uint8_t* data, uint32_t len, ParallelGS::GSInterface& gs);
/* GPU_MSG_NATIVE arrived: the next `ntags` PATH1 GIF tags are that batch's
   output (set 0: one; set-2 two-pass programs: both tags of their packet). */
void NativeNext(uint32_t ntags = 1);
/* ... and the frame buffer it draws into (native.cpp knows it). */
void NativeFbp(uint32_t fbp);
void Vsync(uint32_t frame, ParallelGS::GSInterface& gs);

struct Surface { uint32_t fbp, fbw, psm, zbp, zpsm, w, h; };

/* The GS memory the composite should start from for the displayed buffer:
   the pre-effect snapshot when this frame has a post phase, else null. */
const uint8_t* PreVram(uint32_t fbp);
/* Append the w*h ownership plane (1 = native owns the pixel): from the
   pre-effect snapshot, else from the ownership GS now.  False when ownership
   is off (XSG_GPU_NATIVE_OWNER=0). */
bool OwnerPlane(const Surface& sf, std::vector<uint32_t>& out);
enum class Post { None, Replay, Unsupported };
Post PostState(uint32_t fbp, const char** why = nullptr);
size_t PostCount(uint32_t fbp);
const char* PostTrigger(uint32_t fbp);   // the draw that started the post phase
/* Record the replay of the post draws on (color, z) at scale s; both images
   in GENERAL layout, and they stay there. */
void Replay(Vulkan::CommandBuffer& cmd, uint32_t fbp, int s, uint32_t w, uint32_t h, const Vulkan::Image& color,
            const Vulkan::Image& z);
/* The PMODE circuit blend over `color`, when the read circuits ask for one. */
bool Pmode(Vulkan::CommandBuffer& cmd, const ParallelGS::GSInterface& gs, int s, uint32_t w, uint32_t h,
           const Vulkan::Image& color, const Vulkan::Image& z);
/* Validation: the post phase replayed at 1x on the pre-effect GS frame vs the
   final paraLLEl-GS frame.  hist: exact, <=2, <=8, >8. */
bool Validate1x(const Surface& sf, const uint8_t* final_vram, uint64_t hist[4], std::vector<uint32_t>* img);

}  // namespace xsgfx

#endif

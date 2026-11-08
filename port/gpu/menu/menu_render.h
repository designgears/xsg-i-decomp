/* Dear ImGui draw data on Granite (port/gpu/menu/menu_render.cpp).
 *
 * xsg-gpu's Vulkan device, queue and swapchain are owned by Granite (through
 * paraLLEl-GS), so the menu records into Granite command buffers instead of
 * using imgui_impl_vulkan's own pools, queue submits and render passes: a
 * small renderer with the same vertex layout and shader as the official
 * backend (imgui_impl_vulkan.cpp, MIT). */
#ifndef XSG_MENU_RENDER_H
#define XSG_MENU_RENDER_H

#include <cstdint>

struct ImDrawData;
struct ImDrawList;
struct ImDrawCmd;
namespace Vulkan { class Device; class CommandBuffer; }

namespace xsgmenu {

bool RenderInit(Vulkan::Device* dev);
/* Uploads ImGui's font atlas (again after the fonts changed). */
void RenderFonts();
/* A texture for ImGui images (the rebuilt UI's art): RGBA8, w x h.  Returns the
   ImTextureID to draw it with; updating an existing id replaces its image.
   Ids 2.. (1 is the font).  Call outside a render pass. */
uint64_t RenderTexture(uint64_t id, const uint32_t* rgba, uint32_t w, uint32_t h);
/* Draw-list callbacks that switch blending for what follows: additive (glows,
   the game's additive sprites) and back to normal alpha. */
void BlendAdd(const ImDrawList*, const ImDrawCmd*);
void BlendAlpha(const ImDrawList*, const ImDrawCmd*);
/* Inside a render pass on a fbw x fbh colour target. */
void RenderDraw(Vulkan::CommandBuffer& cmd, ImDrawData* dd, uint32_t fbw, uint32_t fbh);

}  // namespace xsgmenu

#endif

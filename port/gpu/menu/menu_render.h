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
namespace Vulkan { class Device; class CommandBuffer; }

namespace xsgmenu {

bool RenderInit(Vulkan::Device* dev);
/* Uploads ImGui's font atlas (again after the fonts changed). */
void RenderFonts();
/* Inside a render pass on a fbw x fbh colour target. */
void RenderDraw(Vulkan::CommandBuffer& cmd, ImDrawData* dd, uint32_t fbw, uint32_t fbh);

}  // namespace xsgmenu

#endif

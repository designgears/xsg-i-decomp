/* Dear ImGui on Granite: see menu_render.h. */
#include "menu_render.h"

#include "imgui.h"

#include "command_buffer.hpp"
#include "device.hpp"

#include <cstring>

namespace xsgmenu {
namespace {

const uint32_t kVert[] = {
#include "imgui.vert.inc"
};
const uint32_t kFrag[] = {
#include "imgui.frag.inc"
};

Vulkan::Device* g_dev;
Vulkan::Program* g_prog;
Vulkan::ImageHandle g_font;

}  // namespace

bool RenderInit(Vulkan::Device* dev) {
    g_dev = dev;
    Vulkan::ResourceLayout vl, fl;
    vl.input_mask = 7;
    vl.push_constant_size = 16;
    fl.output_mask = 1;
    fl.sets[0].sampled_image_mask = 1;
    fl.sets[0].meta[0].array_size = 1;
    auto* vs = dev->request_shader(kVert, sizeof kVert, &vl);
    auto* fs = dev->request_shader(kFrag, sizeof kFrag, &fl);
    g_prog = dev->request_program(vs, fs);
    return g_prog != nullptr;
}

void RenderFonts() {
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* px = nullptr;
    int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&px, &w, &h);
    auto info = Vulkan::ImageCreateInfo::immutable_2d_image(unsigned(w), unsigned(h), VK_FORMAT_R8G8B8A8_UNORM);
    Vulkan::ImageInitialData init = {};
    init.data = px;
    g_font = g_dev->create_image(info, &init);
    io.Fonts->SetTexID(ImTextureID(1));
}

void RenderDraw(Vulkan::CommandBuffer& cmd, ImDrawData* dd, uint32_t fbw, uint32_t fbh) {
    if (!dd || dd->TotalVtxCount == 0 || !g_font) return;
    cmd.set_opaque_state();
    cmd.set_program(g_prog);
    cmd.set_depth_test(false, false);
    cmd.set_cull_mode(VK_CULL_MODE_NONE);
    cmd.set_blend_enable(true);
    cmd.set_blend_factors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    cmd.set_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    VkViewport vp = {0, 0, float(fbw), float(fbh), 0, 1};
    cmd.set_viewport(vp);
    const ImVec2 off = dd->DisplayPos, sz = dd->DisplaySize;
    float pc[4] = {2.0f / sz.x, 2.0f / sz.y, -1.0f - off.x * 2.0f / sz.x, -1.0f - off.y * 2.0f / sz.y};
    cmd.push_constants(pc, 0, sizeof pc);
    cmd.set_texture(0, 0, g_font->get_view(), Vulkan::StockSampler::LinearClamp);

    // One vertex / index allocation for the whole frame, as imgui_impl_vulkan does.
    const size_t vb = size_t(dd->TotalVtxCount) * sizeof(ImDrawVert), ib = size_t(dd->TotalIdxCount) * sizeof(ImDrawIdx);
    auto* v = static_cast<ImDrawVert*>(cmd.allocate_vertex_data(0, vb, sizeof(ImDrawVert)));
    auto* i = static_cast<ImDrawIdx*>(cmd.allocate_index_data(ib, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32));
    for (const ImDrawList* l : dd->CmdLists) {
        std::memcpy(v, l->VtxBuffer.Data, size_t(l->VtxBuffer.Size) * sizeof(ImDrawVert));
        std::memcpy(i, l->IdxBuffer.Data, size_t(l->IdxBuffer.Size) * sizeof(ImDrawIdx));
        v += l->VtxBuffer.Size;
        i += l->IdxBuffer.Size;
    }
    cmd.set_vertex_attrib(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos));
    cmd.set_vertex_attrib(1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv));
    cmd.set_vertex_attrib(2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col));

    int vo = 0, io = 0;
    for (const ImDrawList* l : dd->CmdLists) {
        for (const ImDrawCmd& c : l->CmdBuffer) {
            if (c.UserCallback) {
                if (c.UserCallback != ImDrawCallback_ResetRenderState) c.UserCallback(l, &c);
                continue;
            }
            const ImVec2 fs = dd->FramebufferScale;
            float x0 = (c.ClipRect.x - off.x) * fs.x, y0 = (c.ClipRect.y - off.y) * fs.y;
            float x1 = (c.ClipRect.z - off.x) * fs.x, y1 = (c.ClipRect.w - off.y) * fs.y;
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 > float(fbw)) x1 = float(fbw);
            if (y1 > float(fbh)) y1 = float(fbh);
            if (x1 <= x0 || y1 <= y0) continue;
            VkRect2D sc = {{int32_t(x0), int32_t(y0)}, {uint32_t(x1 - x0), uint32_t(y1 - y0)}};
            cmd.set_scissor(sc);
            cmd.draw_indexed(c.ElemCount, 1, c.IdxOffset + uint32_t(io), int32_t(c.VtxOffset) + vo, 0);
        }
        vo += l->VtxBuffer.Size;
        io += l->IdxBuffer.Size;
    }
}

}  // namespace xsgmenu

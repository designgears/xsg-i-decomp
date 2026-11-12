/* Rebuilt UI: layout.  See layout.h. */
#include "layout.h"

#include "../menu/cfg.h"

#include <algorithm>
#include <string>

namespace xsgui {
namespace {
float g_gx, g_gy, g_gw, g_gh;   // the game image
float g_ux0, g_ux1;             // the UI area, horizontally
float g_kx = 1, g_k = 1;
}  // namespace

void LayoutBegin(float gx, float gy, float gw, float gh) {
    g_gx = gx; g_gy = gy; g_gw = gw; g_gh = gh;
    g_k = gh / 448.0f;
    g_kx = g_k * 448.0f / 512.0f * 4.0f / 3.0f;   // the 4:3 display's pixel aspect, whatever the picture's
    const float uiw = 512.0f * g_kx;
    g_ux0 = gx + (gw - uiw) * 0.5f;
    g_ux1 = g_ux0 + uiw;
    static std::string mode;
    static bool wide_picture;
    static unsigned tick;
    if (tick++ % 30 == 0) {
        mode = xsgcfg::Get("display.ui_layout");
        wide_picture = xsgcfg::GetBool("display.widescreen") && xsgcfg::GetBool("renderer.native");
    }
    if (wide_picture) {
        // [display] widescreen: the game area is the 16:9 picture's centre 4:3; edge UI is
        // anchored to the picture's own edges (684 PS2 px wide, xsg_gpu.cpp kWideW), within
        // the window.  This is the wide layout, relative to the picture.
        const float sw = ImGui::GetIO().DisplaySize.x, band = 86.0f * g_kx;
        g_ux0 = std::max(0.0f, g_ux0 - band);
        g_ux1 = std::min(sw, g_ux1 + band);
    } else if (mode == "wide") {
        const float sw = ImGui::GetIO().DisplaySize.x;
        // edge UI moves out toward the window's edges, but stays near the picture (at most 72 game
        // px beyond it, never past a 2% margin), so it reads as part of the scene
        const float margin = std::max(8.0f, sw * 0.02f), reach = 72.0f * g_kx;
        g_ux0 = std::max(margin, g_ux0 - reach);
        g_ux1 = std::min(sw - margin, g_ux1 + reach);
    }
}

ImVec2 Map(float x, float y, int anchor) {
    const float sy = g_gy + y * g_k;
    if (anchor == kStretch) anchor = x < 256.0f ? kLeft : kRight;
    switch (anchor) {
    case kLeft: return ImVec2(g_ux0 + x * g_kx, sy);
    case kRight: return ImVec2(g_ux1 - (512.0f - x) * g_kx, sy);
    default: return ImVec2((g_ux0 + g_ux1) * 0.5f + (x - 256.0f) * g_kx, sy);
    }
}

ImVec2 Unmap(ImVec2 p, int anchor) {
    const float y = (p.y - g_gy) / g_k;
    switch (anchor) {
    case kRight: return ImVec2(512.0f - (g_ux1 - p.x) / g_kx, y);
    case kCentre: return ImVec2(256.0f + (p.x - (g_ux0 + g_ux1) * 0.5f) / g_kx, y);
    default: return ImVec2((p.x - g_ux0) / g_kx, y);
    }
}

int AnchorFor(float x0, float x1) {
    if (x1 - x0 >= 440.0f || (x0 <= 8.0f && x1 >= 504.0f)) return kStretch;
    const float c = (x0 + x1) * 0.5f;
    if (c < 512.0f / 3.0f) return kLeft;
    if (c > 512.0f * 2.0f / 3.0f) return kRight;
    return kCentre;
}

float LayoutKx() { return g_kx; }
float LayoutK() { return g_k; }
ImVec4 LayoutArea() { return ImVec4(g_ux0, g_gy, g_ux1, g_gy + g_gh); }

}  // namespace xsgui

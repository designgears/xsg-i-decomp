/* Rebuilt UI: the shared look and motion.  See style.h. */
#include "style.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace xsgui {
namespace {
double g_now, g_dt = 1 / 60.0;
std::unordered_map<uint64_t, AnimState> g_anim;

constexpr ImU32 kCyan = IM_COL32(118, 214, 255, 255);
constexpr ImU32 kFrame = IM_COL32(150, 180, 235, 150);
constexpr ImU32 kTop = IM_COL32(30, 30, 76, 226);
constexpr ImU32 kBottom = IM_COL32(11, 11, 34, 236);
}  // namespace

void AnimFrame(double now) {
    if (g_now > 0) g_dt = std::clamp(now - g_now, 1e-4, 0.1);
    g_now = now;
}
double AnimNow() { return g_now; }

AnimState& Anim(uint64_t key, float x, float y) {
    AnimState& s = g_anim[key];
    if (s.init && g_now - s.last > 0.25) s = AnimState();   // gone a moment: animate in again
    if (!s.init) {
        s.init = true;
        s.x = x; s.y = y;
        s.born = g_now;
    }
    s.last = g_now;
    return s;
}

ImVec2 Spring(AnimState& s, float tx, float ty, float stiffness) {
    // critically damped: x'' = k^2 (t - x) - 2k x'
    const float k = stiffness, dt = float(g_dt);
    for (int i = 0; i < 2; ++i) {
        const float h = dt * 0.5f;
        s.vx += (k * k * (tx - s.x) - 2 * k * s.vx) * h;
        s.vy += (k * k * (ty - s.y) - 2 * k * s.vy) * h;
        s.x += s.vx * h;
        s.y += s.vy * h;
    }
    if (std::fabs(tx - s.x) < 0.01f && std::fabs(s.vx) < 0.01f) s.x = tx, s.vx = 0;
    if (std::fabs(ty - s.y) < 0.01f && std::fabs(s.vy) < 0.01f) s.y = ty, s.vy = 0;
    return ImVec2(s.x, s.y);
}

float FadeIn(AnimState& s, float rate) {
    s.a = std::min(1.0f, s.a + rate * float(g_dt));
    // ease-out cubic
    const float u = 1 - s.a;
    return 1 - u * u * u;
}

float Tween(AnimState& s, float target, float rate) {
    s.v += (target - s.v) * (1 - std::exp(-rate * float(g_dt)));
    if (std::fabs(target - s.v) < 0.01f) s.v = target;
    return s.v;
}

void AnimGc(double ttl) {
    for (auto it = g_anim.begin(); it != g_anim.end();)
        it = g_now - it->second.last > ttl ? g_anim.erase(it) : std::next(it);
}

void Shadow(ImDrawList* dl, ImVec2 a, ImVec2 b, float k, float t, float rounding) {
    // a soft shadow from stacked, growing, fading rectangles, offset down-right
    for (int i = 5; i >= 1; --i) {
        const float g = i * 1.6f * k;
        const ImVec2 o(2.0f * k, 3.0f * k);
        dl->AddRectFilled(ImVec2(a.x - g + o.x, a.y - g + o.y), ImVec2(b.x + g + o.x, b.y + g + o.y),
                          IM_COL32(0, 0, 8, int(18 * t)), rounding + g);
    }
}

void Panel(ImDrawList* dl, ImVec2 a, ImVec2 b, float k, float t, bool accent, ImU32 tint) {
    Shadow(dl, a, b, k, t);
    dl->AddRectFilledMultiColor(a, b, Alpha(kTop, t), Alpha(kTop, t), Alpha(kBottom, t), Alpha(kBottom, t));
    if (tint) dl->AddRectFilled(a, b, Alpha(tint, t));
    // a faint diagonal hatch, like the game's window texture
    dl->PushClipRect(a, b, true);
    const float step = 7 * k;
    for (float x = a.x - (b.y - a.y); x < b.x; x += step)
        dl->AddLine(ImVec2(x, b.y), ImVec2(x + (b.y - a.y), a.y), IM_COL32(150, 170, 255, int(7 * t)), 1.0f);
    dl->PopClipRect();
    // sheen along the top edge
    const float sh = std::min(10 * k, (b.y - a.y) * 0.4f);
    dl->AddRectFilledMultiColor(a, ImVec2(b.x, a.y + sh), IM_COL32(170, 200, 255, int(26 * t)),
                                IM_COL32(170, 200, 255, int(26 * t)), IM_COL32(170, 200, 255, 0), IM_COL32(170, 200, 255, 0));
    dl->AddRect(a, b, Alpha(accent ? kCyan : kFrame, t), 0, 0, (accent ? 1.4f : 1.0f) * k);
    // corner brackets
    const ImU32 bc = Alpha(accent ? kCyan : IM_COL32(150, 180, 235, 210), t);
    const float L = std::min(8 * k, std::min(b.x - a.x, b.y - a.y) * 0.3f), w = 1.3f * k, g = 2 * k;
    const ImVec2 p0(a.x - g, a.y - g), p1(b.x + g, b.y + g);
    dl->AddLine(p0, ImVec2(p0.x + L, p0.y), bc, w); dl->AddLine(p0, ImVec2(p0.x, p0.y + L), bc, w);
    dl->AddLine(ImVec2(p1.x, p0.y), ImVec2(p1.x - L, p0.y), bc, w); dl->AddLine(ImVec2(p1.x, p0.y), ImVec2(p1.x, p0.y + L), bc, w);
    dl->AddLine(ImVec2(p0.x, p1.y), ImVec2(p0.x + L, p1.y), bc, w); dl->AddLine(ImVec2(p0.x, p1.y), ImVec2(p0.x, p1.y - L), bc, w);
    dl->AddLine(p1, ImVec2(p1.x - L, p1.y), bc, w); dl->AddLine(p1, ImVec2(p1.x, p1.y - L), bc, w);
}

void GlowBar(ImDrawList* dl, ImVec2 a, ImVec2 b, float k, float t, double now, ImU32 col) {
    const ImU32 c0 = Alpha(col, 0.34f * t), c1 = Alpha(col, 0.06f * t);
    // outer glow
    for (int i = 3; i >= 1; --i) {
        const float g = i * 1.5f * k;
        dl->AddRectFilled(ImVec2(a.x - g, a.y - g), ImVec2(b.x + g, b.y + g), Alpha(col, 0.05f * t), g);
    }
    dl->AddRectFilledMultiColor(a, b, c0, c1, c1, c0);
    // a light sweep crossing the bar every ~2.4 s
    const float w = b.x - a.x, ph = float(std::fmod(now, 2.4) / 2.4);
    const float cx = a.x - w * 0.3f + ph * w * 1.6f, hw = std::max(6 * k, w * 0.12f);
    dl->PushClipRect(a, b, true);
    dl->AddRectFilledMultiColor(ImVec2(cx - hw, a.y), ImVec2(cx, b.y), Alpha(col, 0), Alpha(col, 0.22f * t),
                                Alpha(col, 0.22f * t), Alpha(col, 0));
    dl->AddRectFilledMultiColor(ImVec2(cx, a.y), ImVec2(cx + hw, b.y), Alpha(col, 0.22f * t), Alpha(col, 0), Alpha(col, 0),
                                Alpha(col, 0.22f * t));
    dl->PopClipRect();
    dl->AddLine(ImVec2(a.x, b.y), ImVec2(b.x, b.y), Alpha(col, 0.7f * t), 1.0f * k);
    dl->AddRectFilled(a, ImVec2(a.x + 2.5f * k, b.y), Alpha(col, t));
}

void Chevron(ImDrawList* dl, ImVec2 tip, float size, float t, double now) {
    const float bob = float(std::sin(now * 5.0)) * size * 0.18f;
    const ImVec2 p(tip.x + bob, tip.y);
    const float h = size * 0.55f;
    dl->AddTriangleFilled(ImVec2(p.x - size, p.y - h), p, ImVec2(p.x - size, p.y + h), Alpha(kCyan, t));
    dl->AddTriangleFilled(ImVec2(p.x - size * 1.6f, p.y - h * 0.7f), ImVec2(p.x - size * 0.75f, p.y),
                          ImVec2(p.x - size * 1.6f, p.y + h * 0.7f), Alpha(kCyan, 0.35f * t));
}

void SweepRule(ImDrawList* dl, ImVec2 a, ImVec2 b, float k, float t, float age, ImU32 col) {
    const float u = std::clamp(age / 0.45f, 0.0f, 1.0f), e = 1 - (1 - u) * (1 - u);
    const float x = a.x + (b.x - a.x) * e;
    dl->AddLine(a, ImVec2(x, b.y), Alpha(col, 0.8f * t), 1.2f * k);
    if (u < 1) dl->AddCircleFilled(ImVec2(x, b.y), 2.2f * k, Alpha(IM_COL32(220, 245, 255, 255), t));
}

void HaloText(ImDrawList* dl, ImFont* font, float size, ImVec2 p, ImU32 col, const char* s, float t) {
    const ImU32 h = IM_COL32(4, 4, 20, int(150 * t));
    const float o = std::max(1.0f, size * 0.07f);
    dl->AddText(font, size, ImVec2(p.x + o, p.y + o), h, s);
    dl->AddText(font, size, ImVec2(p.x - o * 0.5f, p.y + o * 0.5f), IM_COL32(4, 4, 20, int(70 * t)), s);
    dl->AddText(font, size, p, Alpha(col, t), s);
}

}  // namespace xsgui

/* Rebuilt UI: the menus (camp menu, shop, file screen, battle lists).
 *
 * The game's whole menu display list arrives each frame (end_print.c
 * xsg_ui_print_capture, gpu_ipc.h GpuUiMenuList) together with the menu text
 * (e_message.c, normalised).  It is redrawn here in the game's depth order:
 * windows as layered panels that settle in, the game's gradient ribbons as soft
 * glass bars, its art (icons, portraits, cursors) from the menu texture sets,
 * and text, numbers and title tags in the UI font. */
#include "menus.h"

#include "art.h"
#include "dialog.h"
#include "style.h"
#include "gpu_ipc.h"
#include "../menu/option_rows.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace xsgui {
namespace {

GpuUiMenuList* g_m;
GpuUiMenuList* g_snap;   // heap: ~200 KB

ImU32 Ps2(uint32_t c, float t = 1.0f) {   // PS2 colour (0x80 = 1.0, alpha 0x80 = opaque)
    auto ch = [&](int s) { return std::min(255u, ((c >> s) & 0xffu) * 2); };
    return IM_COL32(ch(0), ch(8), ch(16), int(float(ch(24)) * std::clamp(t, 0.0f, 1.0f)));
}

struct Item {
    const GpuUiKgOp* o;
    int order;
    ImVec4 clip;   // PS2 pixels; z <= x: none
};

// a widget's key: its type and its ordinal among that type in the list (stable while
// the game slides it; positions would change every frame)
uint64_t Key(const GpuUiKgOp& o, int ordinal) { return (uint64_t(0x4D45u) << 48) ^ (uint64_t(o.type) << 32) ^ uint64_t(ordinal); }

void DrawText(ImDrawList* dl, ImFont* font, const char* s, int len, ImVec2 p, float fs, float lh, float t,
              const std::function<float(int slot, ImVec2 at, float h)>& icon) {
    using namespace xsgmenu;
    ImU32 colr = col::kText;
    std::string seg;
    float x = p.x, y = p.y;
    auto flush = [&]() {
        if (seg.empty()) return;
        const std::string u = Decode(seg.c_str());
        x += RichText(dl, font, fs, x, y, colr, u.c_str(), t);
        seg.clear();
    };
    for (int i = 0; i < len; ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '\n') { flush(); x = p.x; y += lh; }
        else if (c == '\t') { flush(); x = p.x + std::ceil((x - p.x + 1) / (64 * lh / 24)) * (64 * lh / 24); }
        else if (c == 1 && i + 3 < len) {
            flush();
            const int r = static_cast<unsigned char>(s[i + 1]) & ~1, g = static_cast<unsigned char>(s[i + 2]) & ~1,
                      b = static_cast<unsigned char>(s[i + 3]) & ~1;
            // the game's colours are 0x80 = 1.0; white text stays the UI white
            colr = (r >= 0x7e && g >= 0x7e && b >= 0x7e) ? col::kText
                                                          : IM_COL32(std::min(255, r * 2), std::min(255, g * 2), std::min(255, b * 2), 255);
            i += 3;
        } else if (c == 2 && i + 1 < len) {   // an inline icon: drawn here, advancing by its width
            flush();
            x += icon(static_cast<unsigned char>(s[i + 1]) - 1, ImVec2(x, y), fs);
            i += 1;
        } else seg += char(c);
    }
    flush();
}

}  // namespace

void MenuInit(void* shm_base) {
    g_m = reinterpret_cast<GpuUiMenuList*>(static_cast<uint8_t*>(shm_base) + XSG_GPU_UI_MENU_OFF);
    if (!g_snap) g_snap = new GpuUiMenuList();
}

bool MenuActive(uint32_t vsync) {
    if (!g_m || !g_snap) return false;
    for (int tries = 0; tries < 4; ++tries) {
        const uint32_t s0 = __atomic_load_n(&g_m->seq, __ATOMIC_ACQUIRE);
        if (s0 & 1) continue;
        const uint32_t n = std::min<uint32_t>(g_m->nops, XSG_GPU_UI_MENU_OPS);
        const uint32_t pu = std::min<uint32_t>(g_m->pool_used, XSG_GPU_UI_MENU_POOL);
        const uint32_t fr = g_m->frame;
        std::memcpy(g_snap->op, const_cast<const GpuUiKgOp*>(g_m->op), n * sizeof(GpuUiKgOp));
        std::memcpy(g_snap->pool, const_cast<const char*>(g_m->pool), pu);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_m->seq, __ATOMIC_ACQUIRE) == s0) {
            g_snap->nops = n;
            g_snap->pool_used = pu;
            g_snap->frame = fr;
            break;
        }
    }
    const int32_t age = int32_t(vsync - g_snap->frame);
    return g_snap->frame && age <= 4 && age >= -2000 && g_snap->nops;   // the game may run far ahead (uncapped)
}

void MenuDraw(ImDrawList* dl, ImFont* font, float gx, float gy, float gw, float gh) {
    using namespace xsgmenu;
    const float k = gh / 448.0f, kx = gw / 512.0f;
    auto P = [&](float x, float y) { return ImVec2(gx + x * kx, gy + y * k); };
    const GpuUiMenuList& m = *g_snap;
    const double now = AnimNow();

    // the whole menu fades in on open
    AnimState& whole = Anim(0x3E3E000000000000ull, 0, 0);
    const float tw = FadeIn(whole, 7.0f);

    // queue order -> clip rect, then a stable sort on depth (text after entries at equal depth)
    std::vector<Item> items;
    items.reserve(m.nops);
    ImVec4 clip(0, 0, 0, 0);
    int ordinal[16] = {};
    std::vector<int> ord(m.nops);
    for (uint32_t i = 0; i < m.nops; ++i) {
        const GpuUiKgOp& o = m.op[i];
        ord[i] = ordinal[o.type & 15]++;
        if (o.type == GPU_KG_SCISSOR) {
            clip = o.dw > 0 ? ImVec4(float(o.x), float(o.y), float(o.x + o.dw), float(o.y + o.dh)) : ImVec4(0, 0, 0, 0);
            continue;
        }
        items.push_back({&o, int(i), clip});
    }
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.o->a != b.o->a) return a.o->a < b.o->a;
        return (a.o->type == GPU_KG_TEXT) < (b.o->type == GPU_KG_TEXT);
    });

    // windows fit their content: one whose content ends well above its bottom (a five-item
    // list in an eleven-row window) shrinks to it plus padding; the rectangles inside it follow
    std::vector<int> fitBottom(m.nops, 0);
    for (uint32_t i = 0; i < m.nops; ++i) {
        const GpuUiKgOp& w = m.op[i];
        if (w.type != GPU_KG_WINDOW || !w.b || w.dh < 80) continue;
        int bottom = 0, items = 0;
        for (uint32_t j = 0; j < m.nops; ++j) {
            const GpuUiKgOp& c = m.op[j];
            if (c.a < w.a || c.x < w.x - 2 || c.x > w.x + w.dw || c.y < w.y + 2 || c.y > w.y + w.dh) continue;
            int cb = 0;
            if (c.type == GPU_KG_TEXT) {
                int lines = 1;
                const char* t = m.pool + std::min<uint32_t>(uint32_t(c.b), XSG_GPU_UI_MENU_POOL - 1);
                for (int q = 0; q < c.u; ++q) lines += t[q] == '\n';
                cb = c.y + 24 * lines;
            } else if (c.type == GPU_KG_ART || c.type == GPU_KG_TAG) cb = c.y + std::max<int>(c.dh, 16);
            else if (c.type == GPU_KG_NUMBER) cb = c.y + 16;
            else continue;
            bottom = std::max(bottom, cb);
            items++;
        }
        if (!items) continue;
        const int want = bottom + 10;
        if (want < w.y + w.dh - 24) fitBottom[i] = std::max(want, w.y + 40);
    }
    char buf[32];
    // the GS clipped at the screen edge (windows wait off-screen for their slide-in)
    dl->PushClipRect(P(0, 0), P(512, 448), true);
    for (const Item& it : items) {
        const GpuUiKgOp& o = *it.o;
        const bool clipped = it.clip.z > it.clip.x;
        if (clipped) dl->PushClipRect(P(it.clip.x, it.clip.y), P(it.clip.z, it.clip.w), true);
        const float x = float(o.x), y = float(o.y);
        switch (o.type) {
        case GPU_KG_WINDOW: {
            AnimState& an = Anim(Key(o, ord[size_t(it.order)]), 0, 6);
            const float t = FadeIn(an, 6.5f) * tw;
            const float dy = Spring(an, 0, 0, 15.0f).y;
            // its height (fitted to the content) springs when the content changes
            AnimState& ah = Anim(Key(o, ord[size_t(it.order)]) ^ 0x4848ull, 0, 0);
            const float hTarget = float(fitBottom[size_t(it.order)] ? fitBottom[size_t(it.order)] - o.y : o.dh);
            if (ah.v == 0) ah.v = hTarget;
            const float hh = Tween(ah, hTarget, 14.0f);
            const ImVec2 a = P(x, y + dy), b = P(x + o.dw, y + hh + dy);
            if (o.b) {
                // a corner under a header ribbon or another window gets no bracket
                int corners = 15;
                const int cx[4] = {o.x, o.x + o.dw, o.x, o.x + o.dw}, cy[4] = {o.y, o.y, o.y + o.dh, o.y + o.dh};
                for (uint32_t j = 0; j < m.nops; ++j) {
                    const GpuUiKgOp& h = m.op[j];
                    if (&h == &o || (h.type != GPU_KG_RECT && h.type != GPU_KG_WINDOW && h.type != GPU_KG_TAG) || h.a < o.a) continue;
                    const int hw = h.type == GPU_KG_TAG ? 64 : h.dw, hh = h.type == GPU_KG_TAG ? 16 : h.dh;
                    for (int c = 0; c < 4; ++c)
                        if (cx[c] >= h.x - 4 && cx[c] <= h.x + hw + 4 && cy[c] >= h.y - 4 && cy[c] <= h.y + hh + 4) corners &= ~(1 << c);
                }
                // the game's window colour tints the glass a little (its blue / red / green windows)
                const ImU32 tint = Alpha(Ps2(o.rgba), 0.10f);
                Panel(dl, a, b, k, t, false, tint, corners);
                SweepRule(dl, ImVec2(a.x + 3 * k, a.y + 1 * k), ImVec2(b.x - 3 * k, a.y + 1 * k), k, t * 0.6f,
                          float(now - an.born), col::kCyan);
            } else {   // the open / close frame
                dl->AddRect(a, b, Alpha(col::kCyan, 0.8f * t), 0, 0, 1.2f * k);
                DrawBrackets(dl, ImVec2(a.x - 2 * k, a.y - 2 * k), ImVec2(b.x + 2 * k, b.y + 2 * k), Alpha(col::kCyan, t), 7 * k,
                             1.2f * k);
            }
            break;
        }
        case GPU_KG_RECT: {
            float clampB = 1e9f;
            for (uint32_t j = 0; j < m.nops; ++j)
                if (fitBottom[j] && o.x >= m.op[j].x - 2 && o.x + o.dw <= m.op[j].x + m.op[j].dw + 2 && o.y >= m.op[j].y - 2 &&
                    o.y + o.dh <= m.op[j].y + m.op[j].dh + 2)
                    clampB = std::min(clampB, float(fitBottom[j]));
            if (o.y >= clampB) break;
            const float rh = std::min(float(o.dh), clampB - float(o.y));
            // the game's ribbons (title bars, info bars, highlight bars) as tinted glass: the
            // game's hue, capped opacity, a fade to the right, a sheen and an underline
            const ImVec2 a = P(x, y), b = P(x + o.dw, y + rh);
            if (o.dw <= 1 && o.dh <= 1) {
                dl->AddRectFilled(a, ImVec2(a.x + k, a.y + k), Ps2(o.rgba, tw));
                break;
            }
            auto glass = [&](uint32_t c, float cap) {
                const ImU32 g = Ps2(c);
                const float al = std::min(float((g >> 24) & 0xff) / 255.0f, cap) * tw;
                return Alpha(g | 0xff000000u, al);
            };
            const ImU32 l = glass(o.rgba, 0.62f), r = glass(uint32_t(o.b), 0.40f);
            dl->AddRectFilledMultiColor(a, b, l, r, r, l);
            if (o.dh >= 8) {
                const ImVec2 c(b.x, a.y + (b.y - a.y) * 0.4f);
                dl->AddRectFilledMultiColor(a, c, IM_COL32(255, 255, 255, int(18 * tw)), IM_COL32(255, 255, 255, int(5 * tw)),
                                            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
                dl->AddLine(ImVec2(a.x, b.y - 0.5f), ImVec2(b.x, b.y - 0.5f), Alpha(l | 0xff000000u, 0.55f * tw), 1.0f);
            }
            break;
        }
        case GPU_KG_ART: {
            if (o.text[0] == 1) break;   // an inline text icon: drawn with its text
            const uint64_t tex0 = uint64_t(o.tex0_lo) | uint64_t(o.tex0_hi) << 32;
            const bool cursor = o.w == 16 && o.h == 16 && ((o.u == 0 && (o.v == 192 || o.v == 176)) || (o.u == 64 && (o.v == 160 || o.v == 176)));
            if (o.u == 512 && o.v == 186 && o.w == 165 && o.h == 24) {   // the info bar (win_uv[11]): tinted glass
                const ImVec2 a = P(x, y + 2), b = P(x + o.dw, y + o.dh - 2);
                const ImU32 hue = IM_COL32(120, 92, 190, 255);
                dl->AddRectFilledMultiColor(a, b, Alpha(hue, 0.50f * tw), Alpha(hue, 0.16f * tw), Alpha(hue, 0.16f * tw),
                                            Alpha(hue, 0.50f * tw));
                dl->AddRectFilledMultiColor(a, ImVec2(b.x, a.y + (b.y - a.y) * 0.45f), IM_COL32(255, 255, 255, int(20 * tw)),
                                            IM_COL32(255, 255, 255, int(4 * tw)), IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
                dl->AddRectFilled(a, ImVec2(a.x + 2.5f * k, b.y), Alpha(IM_COL32(190, 160, 255, 255), tw));
                dl->AddLine(ImVec2(a.x, b.y), ImVec2(b.x, b.y), Alpha(IM_COL32(190, 160, 255, 255), 0.45f * tw), 1.0f);
                break;
            }
            if ((o.dh <= 2 && o.dw >= 12) || (o.dw <= 2 && o.dh >= 12)) {   // separator strips: fine hairlines
                const bool horiz = o.dh <= 2;
                const ImVec2 a = P(x, y), b = horiz ? P(x + o.dw, y) : P(x, y + o.dh);
                const ImU32 c = Alpha(col::kFrame, 0.55f * tw), c0 = Alpha(col::kFrame, 0.0f);
                const float th = std::max(1.0f, 0.9f * k);
                if (horiz) {
                    const float mx = a.x + (b.x - a.x) * 0.12f, nx = b.x - (b.x - a.x) * 0.12f;
                    dl->AddRectFilledMultiColor(ImVec2(a.x, a.y), ImVec2(mx, a.y + th), c0, c, c, c0);
                    dl->AddRectFilled(ImVec2(mx, a.y), ImVec2(nx, a.y + th), c);
                    dl->AddRectFilledMultiColor(ImVec2(nx, a.y), ImVec2(b.x, a.y + th), c, c0, c0, c);
                } else {
                    dl->AddRectFilledMultiColor(ImVec2(a.x, a.y), ImVec2(a.x + th, b.y), c, c, c0, c0);
                }
                break;
            }
            if (cursor) {   // the hand cursor glides between rows over a soft glow
                AnimState& cu = Anim(0x43555253ull + uint64_t(ord[size_t(it.order)]) * 0, x, y);
                const ImVec2 q = Spring(cu, x, y, 24.0f);
                const ImVec2 c = P(q.x + o.dw * 0.5f, q.y + o.dh * 0.5f);
                for (int g = 3; g >= 1; --g)
                    dl->AddCircleFilled(c, (5.0f + 4.0f * g) * k, IM_COL32(118, 214, 255, int(10 * tw)), 24);
                ArtSprite(dl, tex0, 0, float(o.u), float(o.v), float(o.w), float(o.h), P(q.x, q.y), P(q.x + o.dw, q.y + o.dh),
                          o.rgba, tw, false, uint32_t(o.b));
                break;
            }
            ArtSprite(dl, tex0, 0, float(o.u), float(o.v), float(o.w), float(o.h), P(x, y), P(x + o.dw, y + o.dh), o.rgba, tw,
                      false, uint32_t(o.b));
            break;
        }
        case GPU_KG_TAG: {   // title letters; 0x03 n advances n pixels
            const char* s = m.pool + std::min<uint32_t>(uint32_t(o.b), XSG_GPU_UI_MENU_POOL - 1);
            const float fs = 15.0f * k;
            struct Seg { std::string t; float dx; };
            std::vector<Seg> segs(1);
            float width = 0;
            for (int i = 0; i < o.u; ++i) {
                const unsigned char c = static_cast<unsigned char>(s[i]);
                if (c == 3 && i + 1 < o.u) {
                    segs.push_back({std::string(), float(static_cast<unsigned char>(s[i + 1])) * kx});
                    i++;
                } else segs.back().t += char(c);
            }
            for (const Seg& g : segs) width += g.dx + font->CalcTextSizeA(fs, 1e9f, 0, g.t.c_str()).x;
            ImVec2 p = P(x, y + 1);
            if (o.w) p.x -= width;
            for (const Seg& g : segs) {
                p.x += g.dx;
                const std::string u = Decode(g.t.c_str());
                HaloText(dl, font, fs, p, col::kText, u.c_str(), tw);
                p.x += font->CalcTextSizeA(fs, 1e9f, 0, u.c_str()).x;
            }
            break;
        }
        case GPU_KG_NUMBER: {
            const int digits = std::clamp<int>(o.u, 1, 10), style = o.v & 0xf;
            const float gwid = (style == 1 || style >= 3) ? 10.0f : 11.0f, ghgt = (style == 1 || style >= 3) ? 13.0f : 16.0f;
            if (o.b < 0) {
                std::memset(buf, '-', size_t(digits));
                buf[digits] = 0;
            } else {
                std::snprintf(buf, sizeof buf, "%0*d", digits, o.b);
            }
            float px = x;
            if (o.v & 0x10) px -= gwid * float(digits);
            // one digit per game cell, so the game's separators (':', '/') still line up
            // the number as one string inside the game's digit span (natural spacing)
            const float fs = ghgt * 1.12f * k;
            const ImVec2 sz = font->CalcTextSizeA(fs, 1e9f, 0, buf);
            const ImVec2 c0 = P(px, y), c1 = P(px + gwid * float(digits), y + ghgt);
            // right-aligned numbers hug their span's end, the others its start (no stray gaps)
            const float nx = (o.v & 0x10) ? c1.x - sz.x : c0.x + (c1.x - c0.x - sz.x) * 0.15f;
            HaloText(dl, font, fs, ImVec2(nx, c1.y - fs * 0.95f), Ps2(o.rgba) | 0xff000000u, buf, tw);
            break;
        }
        case GPU_KG_TEXT: {
            const char* s = m.pool + std::min<uint32_t>(uint32_t(o.b), XSG_GPU_UI_MENU_POOL - 1);
            DrawText(dl, font, s, o.u, P(x, y + 2), 18.0f * k, 24.0f * k, tw, [&](int slot, ImVec2 at, float h) -> float {
                for (uint32_t j = 0; j < m.nops; ++j) {
                    const GpuUiKgOp& ic = m.op[j];
                    if (ic.type != GPU_KG_ART || ic.text[0] != 1 || ic.text[1] != slot) continue;
                    const float iw = float(ic.dw) * kx, ih = float(ic.dh) * k;
                    const float cy = at.y + h * 0.55f;
                    const uint64_t t0 = uint64_t(ic.tex0_lo) | uint64_t(ic.tex0_hi) << 32;
                    ArtSprite(dl, t0, 0, float(ic.u), float(ic.v), float(ic.w), float(ic.h), ImVec2(at.x + 1 * kx, cy - ih * 0.5f),
                              ImVec2(at.x + 1 * kx + iw, cy + ih * 0.5f), ic.rgba, tw, false, uint32_t(ic.b));
                    return iw + 2 * kx;
                }
                return 20.0f * kx;
            });
            break;
        }
        case GPU_KG_LINE: {
            // neutral (white / grey) rules become fine periwinkle hairlines fading at the ends;
            // coloured lines (targets, graphs) keep their colour
            const int r = o.rgba & 0xff, g = (o.rgba >> 8) & 0xff, bl = (o.rgba >> 16) & 0xff;
            const bool neutral = std::abs(r - g) < 24 && std::abs(g - bl) < 24;
            const ImVec2 a = P(x, y), b = P(x + o.dw, y + o.dh);
            if (neutral && (o.dh == 0 || o.dw == 0)) {
                const ImU32 c = Alpha(col::kFrame, 0.5f * tw), c0 = Alpha(col::kFrame, 0.0f);
                const float th = std::max(1.0f, 0.9f * k);
                if (o.dh == 0) {
                    const float lx = std::min(a.x, b.x), rx = std::max(a.x, b.x), m = (rx - lx) * 0.1f;
                    dl->AddRectFilledMultiColor(ImVec2(lx, a.y), ImVec2(lx + m, a.y + th), c0, c, c, c0);
                    dl->AddRectFilled(ImVec2(lx + m, a.y), ImVec2(rx - m, a.y + th), c);
                    dl->AddRectFilledMultiColor(ImVec2(rx - m, a.y), ImVec2(rx, a.y + th), c, c0, c0, c);
                } else {
                    dl->AddRectFilled(ImVec2(a.x, std::min(a.y, b.y)), ImVec2(a.x + th, std::max(a.y, b.y)), c);
                }
            } else {
                dl->AddLine(a, b, Ps2(o.rgba, tw), 1.2f * k);
            }
            break;
        }
        case GPU_KG_CIRCLE: {
            const ImVec2 c = P(x, y);
            dl->AddCircleFilled(c, float(o.dw) * k, Ps2(uint32_t(o.b), tw), 32);
            dl->AddCircleFilled(c, float(o.dw) * k * 0.4f, Ps2(o.rgba, tw), 24);
            break;
        }
        default:
            break;
        }
        if (clipped) dl->PopClipRect();
    }
    dl->PopClipRect();
}

}  // namespace xsgui

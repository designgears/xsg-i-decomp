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
#include "layout.h"
#include "prompts.h"
#include "gpu_ipc.h"
#include "../menu/option_rows.h"
#include "../menu/cfg.h"

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
    float x = p.x, y = p.y, wrap = 0;   // wrap: the game's wrap width on screen (0: none)
    auto flush = [&]() {
        if (seg.empty()) return;
        const std::string u = Decode(seg.c_str());
        if (wrap <= 0) {
            x += RichText(dl, font, fs, x, y, colr, u.c_str(), t);
        } else {   // word by word, to the next line at the wrap width
            size_t i = 0;
            while (i < u.size()) {
                size_t j = u.find(' ', i);
                j = j == std::string::npos ? u.size() : j + 1;
                const std::string w = u.substr(i, j - i);
                const float ww = RichTextWidth(font, fs, w.c_str());
                if (x > p.x && x + ww - fs * 0.3f > p.x + wrap) { x = p.x; y += lh; }
                x += RichText(dl, font, fs, x, y, colr, w.c_str(), t);
                i = j;
            }
        }
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
        } else if (c == 6 && i + 2 < len) {   // the wrap width, game pixels
            flush();
            const int w = ((static_cast<unsigned char>(s[i + 1]) & 0x7f) << 7) | (static_cast<unsigned char>(s[i + 2]) & 0x7f);
            wrap = float(w) * lh / 24.0f * (LayoutKx() / LayoutK());
            i += 2;
        } else if ((c == 4 || c == 5) && i + 2 < len) {   // set x / y (an offset from the origin, game pixels)
            flush();
            const int d = (((static_cast<unsigned char>(s[i + 1]) & 0x7f) << 7) | (static_cast<unsigned char>(s[i + 2]) & 0x7f)) - 0x4000;
            if (c == 4) x = p.x + float(d) * lh / 24.0f * (LayoutKx() / LayoutK());
            else y = p.y + float(d) * lh / 24.0f;
            i += 2;
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

bool MenuWantsSettings() {
    if (!g_snap) return false;
    int windows = 0;
    for (uint32_t i = 0; i < g_snap->nops; ++i) {
        if (g_snap->op[i].type == GPU_KG_PAUSE) return true;
        if (g_snap->op[i].type == GPU_KG_WINDOW && g_snap->op[i].b && g_snap->op[i].dw >= 100) windows++;
    }
    return windows >= 2;   // a full menu screen (not a lone choice box)
}

namespace {
// "[View] Settings": a capsule with the layout's button name, at the top right of the UI area
// "[View] Settings" as part of the help bar: right-aligned inside it, vertically centred, set off
// by a thin divider, in the help text's own type and colour (barTop / barBottom / barRight: game px;
// barTop < 0: no help bar, the top-right corner instead)
// the camp menu's own row under Money: same width and look as the Reserve / Time / Money rows
void SettingsHint(ImDrawList* dl, ImFont* font, float t, float x0, float x1, float y) {
    using namespace xsgmenu;
    const float k = LayoutK(), kx = LayoutKx();
    ImFont* const bf = UiFont(kFontBodyBold, font);
    const std::string lay = xsgcfg::Get("gamepad.layout");
    const char* btn = lay == "playstation" ? "SHARE" : lay == "nintendo" ? "\xe2\x88\x92" : "VIEW";
    const float hs = 17.0f * k, ih = hs * 1.4f;
    const bool icon = PromptIcon(kBack) != 0;
    const ImVec2 a = Map(x0, y, kRight), b = Map(x1, y + 22, kRight);
    dl->AddRectFilledMultiColor(a, b, IM_COL32(52, 64, 140, int(150 * t)), IM_COL32(52, 64, 140, int(60 * t)),
                                IM_COL32(52, 64, 140, int(60 * t)), IM_COL32(52, 64, 140, int(150 * t)));
    dl->AddLine(ImVec2(a.x, b.y - 0.5f), ImVec2(b.x, b.y - 0.5f), Alpha(IM_COL32(52, 64, 140, 255), 0.55f * t), 1.0f);
    const float cy = (a.y + b.y) * 0.5f;
    const ImVec2 lw = bf->CalcTextSizeA(hs, 1e9f, 0, "Settings");
    const float bw = icon ? ih : bf->CalcTextSizeA(hs * 0.7f, 1e9f, 0, btn).x + 12 * kx;
    const float bx = Map(x0 + 46, 0, kRight).x;
    if (icon) {
        PromptDraw(dl, kBack, ImVec2(bx + bw * 0.5f, cy), ih, t);
    } else {
        const float ph = hs * 0.95f;
        dl->AddRect(ImVec2(bx, cy - ph * 0.5f), ImVec2(bx + bw, cy + ph * 0.5f), Alpha(col::kFrame, 0.7f * t), ph * 0.5f, 0, 1.0f);
        const ImVec2 bb = bf->CalcTextSizeA(hs * 0.7f, 1e9f, 0, btn);
        dl->AddText(bf, hs * 0.7f, ImVec2(bx + (bw - bb.x) * 0.5f, cy - bb.y * 0.5f), Alpha(col::kText, t), btn);
    }
    HaloText(dl, bf, hs, ImVec2(bx + bw + 8 * kx, cy - lw.y * 0.5f), col::kText, "Settings", t);
}
}  // namespace

void MenuDraw(ImDrawList* dl, ImFont* font, float gx, float gy, float gw, float gh) {
    using namespace xsgmenu;
    LayoutBegin(gx, gy, gw, gh);
    const float k = LayoutK(), kx = LayoutKx();
    int anchor = kCentre;   // the current op's (layout.h); P maps under it
    auto P = [&](float x, float y) { return Map(x, y, anchor); };
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
    // the help bar: a full-width window near the bottom; content above it never runs under it
    float helpTop = 1e9f;
    int helpZ = 0;
    for (uint32_t i = 0; i < m.nops; ++i) {
        const GpuUiKgOp& w = m.op[i];
        if (w.type == GPU_KG_WINDOW && w.b && w.y > 330 && w.dw >= 400 && float(w.y) < helpTop) helpTop = float(w.y), helpZ = w.a;
    }
    // title windows (top left, one line): as wide as their title plus padding
    std::vector<int> fitRight(m.nops, 0);
    for (uint32_t i = 0; i < m.nops; ++i) {
        const GpuUiKgOp& w = m.op[i];
        if (w.type != GPU_KG_WINDOW || !w.b || w.y > 24 || w.dh > 40 || w.dw < 120) continue;
        float right = 0;
        for (uint32_t j = 0; j < m.nops; ++j) {
            const GpuUiKgOp& c = m.op[j];
            if ((c.type != GPU_KG_TEXT && c.type != GPU_KG_TAG) || c.y < w.y - 4 || c.y > w.y + w.dh || c.x < w.x || c.x > w.x + w.dw) continue;
            const char* t = m.pool + std::min<uint32_t>(uint32_t(c.b), XSG_GPU_UI_MENU_POOL - 1);
            // in game px, as drawn: the body font at 22 game px
            const float tw2 = UiFont(kFontBodyBold, font)->CalcTextSizeA(22.0f, 1e9f, 0, Decode(std::string(t, size_t(std::max<int>(c.u, 0))).c_str()).c_str()).x;
            right = std::max(right, float(c.x) + tw2 / 1.1667f * 1.05f + 4);   // screen / kx, k = 1
        }
        (void)right;
        // the title window spans the header bar behind it (the game's ribbon often runs past the window)
        int bar = w.x + w.dw;
        for (uint32_t j = 0; j < m.nops; ++j) {
            const GpuUiKgOp& r = m.op[j];
            if (r.type == GPU_KG_RECT && r.dw > 40 && r.y < w.y + w.dh && r.y + r.dh > w.y && r.x <= w.x + w.dw)
                bar = std::max(bar, std::min(r.x + r.dw, 496));
        }
        if (bar > w.x + w.dw) fitRight[i] = bar;
    }
    char buf[32];
    // the GS clipped at the screen edge (windows wait off-screen for their slide-in)
    // anchors: each op follows the smallest window containing it (or its own extent)
    std::vector<int> anchors(m.nops, kCentre);
    for (uint32_t i = 0; i < m.nops; ++i) {
        const GpuUiKgOp& o = m.op[i];
        const float ow = o.type == GPU_KG_TEXT || o.type == GPU_KG_TAG || o.type == GPU_KG_NUMBER ? 60.0f : float(std::max<int>(o.dw, 1));
        int best = -1, area = 1 << 30;
        if (o.type != GPU_KG_WINDOW)
            for (uint32_t j = 0; j < m.nops; ++j) {
                const GpuUiKgOp& w = m.op[j];
                if (w.type != GPU_KG_WINDOW || o.x < w.x - 2 || o.x > w.x + w.dw || o.y < w.y - 18 || o.y > w.y + w.dh) continue;
                if (w.dw * w.dh < area) area = w.dw * w.dh, best = int(j);
            }
        anchors[i] = best >= 0 ? AnchorFor(float(m.op[best].x), float(m.op[best].x + m.op[best].dw))
                               : AnchorFor(float(o.x), float(o.x) + ow);
        if (best < 0 && o.type != GPU_KG_WINDOW) anchors[i] = 100 + int(i);   // loose: clustered below
    }
    {   // loose elements on one row with small gaps (an icon bar, a label and its value) move as one
        std::vector<int> root(m.nops);
        for (uint32_t i = 0; i < m.nops; ++i) root[i] = int(i);
        auto find = [&](int a) { while (root[size_t(a)] != a) a = root[size_t(a)] = root[size_t(root[size_t(a)])]; return a; };
        auto ext = [&](const GpuUiKgOp& o) {
            const float w = o.type == GPU_KG_TEXT || o.type == GPU_KG_TAG || o.type == GPU_KG_NUMBER ? 60.0f : float(std::max<int>(o.dw, 1));
            return ImVec2(float(o.x), float(o.x) + w);
        };
        for (uint32_t i = 0; i < m.nops; ++i) {
            if (anchors[i] < 100) continue;
            for (uint32_t j = i + 1; j < m.nops; ++j) {
                if (anchors[j] < 100 || std::abs(m.op[i].y - m.op[j].y) > 10) continue;
                const ImVec2 a = ext(m.op[i]), b = ext(m.op[j]);
                if (a.x <= b.y + 16 && b.x <= a.y + 16) root[size_t(find(int(i)))] = find(int(j));
            }
        }
        std::vector<ImVec2> span(m.nops, ImVec2(1e9f, -1e9f));
        for (uint32_t i = 0; i < m.nops; ++i)
            if (anchors[i] >= 100) {
                ImVec2& sp = span[size_t(find(int(i)))];
                const ImVec2 e = ext(m.op[i]);
                sp.x = std::min(sp.x, e.x);
                sp.y = std::max(sp.y, e.y);
            }
        for (uint32_t i = 0; i < m.nops; ++i)
            if (anchors[i] >= 100) {
                const ImVec2 sp = span[size_t(find(int(i)))];
                anchors[i] = AnchorFor(sp.x, sp.y);
            }
    }
    const ImVec4 area = LayoutArea();
    dl->PushClipRect(ImVec2(area.x, area.y), ImVec2(area.z, area.w), true);
    for (const Item& it : items) {
        const GpuUiKgOp& o = *it.o;
        anchor = anchors[size_t(it.order)];
        // a list entry (a window and its header tag) that does not fit whole above the help bar is not
        // shown at all: lists show complete items, never the top of the next one
        {
            const GpuUiKgOp& o2 = *it.o;
            bool cut = false;
            for (uint32_t j = 0; j < m.nops && helpTop < 1e8f && !cut && o2.a <= helpZ; ++j) {   // the help bar's own content stays
                const GpuUiKgOp& w = m.op[j];
                if (w.type != GPU_KG_WINDOW || !w.b || w.y + w.dh <= helpTop - 4 || w.dw >= 400) continue;
                // the window itself, anything inside it, and its header (tag / ribbon) just above it
                if (o2.x >= w.x - 4 && o2.x <= w.x + w.dw && o2.y >= w.y - 20 && o2.y <= w.y + w.dh) cut = true;
            }
            if (cut) continue;
        }
        // anything under the help bar (below its top, behind it in depth) is hidden, not shown through the glass
        if (helpTop < 1e8f && it.o->y >= helpTop - 8 && it.o->a < helpZ && it.o->type != GPU_KG_PAUSE) continue;
        ImVec4 cr = it.clip;
        if (helpTop < 1e8f && it.o->y < helpTop - 8 && it.o->y + std::max<int>(it.o->dh, 1) > helpTop - 4) {
            // runs under the help bar: cut at its top
            if (cr.z <= cr.x) cr = ImVec4(-512, -448, 1024, 896);
            cr.w = std::min(cr.w, helpTop - 4);
        }
        const bool clipped = cr.z > cr.x;
        if (clipped) dl->PushClipRect(P(cr.x, cr.y), P(cr.z, cr.w), true);
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
            const float wx0 = std::max(x, 8.0f);
            const float wx1 = std::min(fitRight[size_t(it.order)] ? float(fitRight[size_t(it.order)]) : x + float(o.dw), 504.0f);   // inside the edge; titles fitted
            if (wx1 - wx0 < std::min(float(o.dw), wx1 - x) * 0.5f || wx1 <= wx0 + 8) break;   // mostly off-screen: not drawn
            const ImVec2 a = P(wx0, y + dy), b = P(wx1, y + hh + dy);
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
            } else {   // the open / close animation's frames (a trail of several): faint outlines
                dl->AddRect(a, b, Alpha(col::kCyan, 0.28f * t), 0, 0, 1.0f);
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
            // inside the safe area (16 px from the edges): the game runs bars off the screen
            const float rx0 = std::max(x, 16.0f), rx1 = std::min(x + float(o.dw), 496.0f);
            if (o.dw > 40 && rx1 - rx0 < 8) break;
            const ImVec2 a = P(o.dw > 40 ? rx0 : x, y), b = P(o.dw > 40 ? rx1 : x + o.dw, y + rh);
            if (o.dw <= 1 && o.dh <= 1) {
                dl->AddRectFilled(a, ImVec2(a.x + k, a.y + k), Ps2(o.rgba, tw));
                break;
            }
            // one palette: the game's purple / magenta info bars become indigo glass, its red
            // accents cyan; other hues keep their colour
            auto recolour = [&](uint32_t c) -> ImU32 {
                const ImU32 g = Ps2(c);
                const int r = g & 0xff, gr = (g >> 8) & 0xff, bl = (g >> 16) & 0xff;
                if (r > gr + 30 && bl > gr + 10) return (g & 0xff000000u) | IM_COL32(52, 64, 140, 0);   // purple / magenta
                if (r > gr + 60 && r > bl + 60) return (g & 0xff000000u) | IM_COL32(118, 214, 255, 0);  // red accents
                return g;
            };
            auto glass = [&](uint32_t c, float cap) {
                const ImU32 g = recolour(c);
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
            if (o.text[2] == 1 + 1 && o.text[3] >= 9 && o.text[3] <= 22 && o.w == 29 && o.h == 20) {
                // others_uv shoulder tabs (L1 / R1 arrows): the layout's shoulder button with an outward chevron
                if (((o.rgba >> 24) & 0xff) < 0x40) break;   // a faded copy (mid-slide)
                const bool right = o.text[3] == 16 || o.text[3] == 14 || o.text[3] == 12 || o.text[3] == 10 || o.text[3] == 20 || o.text[3] == 22;
                const ImVec2 c = P(x + o.dw * 0.5f, y + o.dh * 0.5f);
                const float h = 13.0f * k;
                const float w = font->CalcTextSizeA(h * 0.78f, 1e9f, 0, "RB").x + h * 0.7f;
                const float ch = 4.5f * k, gx = 3 * kx;
                if (right) {
                    ShoulderGlyph(dl, font, c.x - (w + gx + ch) * 0.5f, c.y, h, 1);
                    const float ax = c.x + (w + gx + ch) * 0.5f - ch;
                    dl->AddTriangleFilled(ImVec2(ax, c.y - ch), ImVec2(ax + ch, c.y), ImVec2(ax, c.y + ch), Alpha(col::kCyan, tw));
                } else {
                    const float ax = c.x - (w + gx + ch) * 0.5f;
                    dl->AddTriangleFilled(ImVec2(ax + ch, c.y - ch), ImVec2(ax, c.y), ImVec2(ax + ch, c.y + ch), Alpha(col::kCyan, tw));
                    ShoulderGlyph(dl, font, ax + ch + gx, c.y, h, 0);
                }
                break;
            }
            if (o.text[2] == 1 + 11 && (o.text[3] == 1 || o.text[3] == 2)) {
                // the help bar's red top strip (others2_uv[1]) and left bar ([2]): the cyan accent
                const ImVec2 a = P(x, y), b = P(x + o.dw, y + o.dh);
                if (o.text[3] == 1) {
                    const float yy = a.y + (b.y - a.y) * 0.5f;
                    dl->AddRectFilledMultiColor(ImVec2(a.x, yy - 0.75f * k), ImVec2(b.x, yy + 0.75f * k), Alpha(col::kCyan, 0.75f * tw),
                                                Alpha(col::kCyan, 0.15f * tw), Alpha(col::kCyan, 0.15f * tw), Alpha(col::kCyan, 0.75f * tw));
                } else {
                    dl->AddRectFilled(ImVec2(a.x + 3 * kx, a.y), ImVec2(a.x + 6 * kx, b.y), Alpha(col::kCyan, 0.7f * tw));
                }
                break;
            }
            if (o.text[2] == 1 + 7 && (o.text[3] == 3 || o.text[3] == 2)) {   // num_uv's '/' and ':' between numbers: font glyphs
                const char* g = o.text[3] == 3 ? "/" : ":";
                const float fs = std::max(8.0f, float(o.dh)) * 1.2f * k;
                const ImVec2 gs = font->CalcTextSizeA(fs, 1e9f, 0, g);
                const ImVec2 c = P(x + o.dw * 0.5f, y + o.dh);
                dl->AddText(font, fs, ImVec2(c.x - gs.x * 0.5f, c.y - fs * 0.95f), Alpha(col::kTextDim, tw), g);
                break;
            }
            if (o.text[2] == 1 + 2 && o.text[3] >= 0 && o.text[3] < 4) {   // pad_uv[0..3]: the face buttons
                static const int kPad[4] = {1, 2, 3, 0};   // pad_uv: circle, square, triangle, cross (u 0, 22, 44, 66)
                const ImVec2 c = P(x + o.dw * 0.5f, y + o.dh * 0.5f);
                Glyph(dl, font, c.x, c.y, std::min(o.dw * kx, o.dh * k) * 0.46f, kPad[size_t(o.text[3])], false);
                break;
            }
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
            if (cursor && ((o.rgba >> 24) & 0xff) < 0x40) break;   // a fading cursor left behind: not drawn
            if (cursor) {   // the hand cursor glides between rows over a soft glow, just outside its window
                float cxp = x;
                {
                    int best = -1, area = 1 << 30;
                    const float mx = x + o.dw * 0.5f, my = y + o.dh * 0.5f;
                    for (uint32_t j = 0; j < m.nops; ++j) {
                        const GpuUiKgOp& w = m.op[j];
                        if (w.type != GPU_KG_WINDOW || !w.b || mx < w.x - 4 || mx > w.x + w.dw || my < w.y || my > w.y + w.dh) continue;
                        if (w.dw * w.dh < area) area = w.dw * w.dh, best = int(j);
                    }
                    (void)best;   // the game's own place: snapping outside misplaced cursors (two windows, sliding)
                }
                const float x = cxp;
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
            const float fs = 28.0f * k;   // the original's tag caps are ~14 px tall (Saira's caps are 0.44 of the ImGui size)
            ImFont* const lf = UiFont(kFontLabel, font);   // the original's heavy condensed small caps, pale teal
            const ImU32 kLabelCol = IM_COL32(200, 224, 224, 255);   // the original's pale grey-teal
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
            for (const Seg& g : segs) width += g.dx + SmallCaps(dl, lf, fs, ImVec2(0, 0), 0, Decode(g.t.c_str()).c_str(), 0, false);
            ImVec2 p = P(x, y + 13.5f);   // on the tag bar's centre line (the game's 16 px baseline sat low)
            p.y -= fs * (lf->Ascent / lf->FontSize);   // ImGui draws from the ascender line
            if (o.w) p.x -= width;
            for (const Seg& g : segs) {
                p.x += g.dx;
                const std::string u = Decode(g.t.c_str());
                p.x += SmallCaps(dl, lf, fs, p, kLabelCol, u.c_str(), tw);
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
            const float fs = ghgt * 1.9f * k;   // digits are ~0.44 of the size: ~5/6 of the cell height, as the original's
            const ImVec2 sz = font->CalcTextSizeA(fs, 1e9f, 0, buf);
            const ImVec2 c0 = P(px, y), c1 = P(px + gwid * float(digits), y + ghgt);
            {   // the original's tall rounded-square digits, outlined; width fitted to the game's digit span
                ImFont* const nf = UiFont(kFontLabel, font);   // Saira Semi Condensed: the original's heavy condensed digits
                float nfs = fs, adv = 0;
                for (char d = '0'; d <= '9'; ++d) { const char one[2] = {d, 0}; adv = std::max(adv, nf->CalcTextSizeA(nfs, 1e9f, 0, one).x); }
                const int n = int(std::strlen(buf));
                const float span = c1.x - c0.x;
                if (adv * n > span * 1.02f) nfs *= span * 1.02f / (adv * n);
                adv = std::min(span / float(n), adv * (nfs / fs) * 1.06f);
                // the run centred on the game's digit span: its ':' '/' 'G' sit between spans, evenly spaced
                const float nx2 = c0.x + (span - adv * float(n)) * 0.5f;
                ImU32 nc = Ps2(o.rgba) | 0xff000000u;
                if (((nc & 0xff) > 0xe0) && (((nc >> 8) & 0xff) > 0xe0)) nc = IM_COL32(236, 240, 232, 255);   // white -> the original's warm white
                // tabular: each digit centred in a fixed cell, so columns stay rectangular; baseline 1.5 px
                // above the cell's bottom, on the list text's baseline
                const float by = c1.y - 1.5f * k - nfs * (nf->Ascent / nf->FontSize);
                for (int q = 0; q < n; ++q) {
                    const char one[2] = {buf[q], 0};
                    const float w = nf->CalcTextSizeA(nfs, 1e9f, 0, one).x;
                    OutlineText(dl, nf, nfs, ImVec2(nx2 + adv * q + (adv - w) * 0.5f, by), nc, one, tw);
                }
            }
            break;
        }
        case GPU_KG_TEXT: {
            ImFont* const tfont = UiFont(kFontBodyBold, font);   // Exo 2 SemiBold: the original's heavy text
            const char* s = m.pool + std::min<uint32_t>(uint32_t(o.b), XSG_GPU_UI_MENU_POOL - 1);
            // proportions: when the game measured the string (o.dw), the widest line takes the same width
            float fs = 22.0f * k;   // the original's text: 16 px capitals at 2x
            bool wraps = false;
            for (int q = 0; q < o.u; ++q) wraps |= s[q] == 6;
            if (o.dw > 8 && !wraps) {
                float widest = 0;
                std::string line;
                for (int q = 0; q <= o.u; ++q) {
                    const unsigned char c = q < o.u ? static_cast<unsigned char>(s[q]) : '\n';
                    if (c == '\n') { widest = std::max(widest, RichTextWidth(tfont, fs, Decode(line.c_str()).c_str())); line.clear(); }
                    else if (c == 1 || c == 4 || c == 5) q += c == 1 ? 3 : 2;
                    else if (c == 2) q += 1;
                    else line += char(c);
                }
                // as tall as the original; narrower only when it would run well past the game's width
                if (widest > float(o.dw) * kx * 1.08f) fs *= std::max(0.6f, float(o.dw) * kx * 1.08f / widest);
            }
            DrawText(dl, tfont, s, o.u, P(x, y + 2), fs, 24.0f * k, tw, [&](int slot, ImVec2 at, float h) -> float {
                for (uint32_t j = 0; j < m.nops; ++j) {
                    const GpuUiKgOp& ic = m.op[j];
                    if (ic.type != GPU_KG_ART || ic.text[0] != 1 || ic.text[1] != slot) continue;
                    const float iw = float(ic.dw) * kx, ih = float(ic.dh) * k;
                    const float cy = at.y + h * 0.55f;
                    if (ic.text[2] == 1 + 2 && ic.text[3] >= 0 && ic.text[3] < 4) {   // a pad button: the controller's
                        static const int kPad[4] = {1, 2, 3, 0};
                        const float r = h * 0.55f;
                        Glyph(dl, font, at.x + r + h * 0.08f, cy, r, kPad[size_t(ic.text[3])], false);
                        return 2 * r + h * 0.22f;
                    }
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
        case GPU_KG_SELECT: {   // a choice menu's selected row: the cursor glides, the bar glows
            AnimState& cu = Anim(0x53454C00ull, x, y);
            const ImVec2 q = Spring(cu, x, y, 22.0f);
            const ImVec2 a = P(q.x, q.y), b = P(q.x + o.dw, q.y + o.dh);
            GlowBar(dl, a, b, k, tw, now, col::kCyan);
            Chevron(dl, P(q.x - 2, q.y + o.dh * 0.5f), 6 * k, tw, now);
            break;
        }
        case GPU_KG_TITLE: {   // the title choices: the original's place and spacing (y 288 + 18 per row)
            static const char* const kItem[] = {"New Game", "Continue", "Install to HDD", "Uninstall from HDD", "Settings"};
            anchor = kCentre;
            // the picture, upscaled once (art.cpp ArtImageSet), then a starfield drifting over it
            uint32_t lw = 0, lh = 0;
            const ImTextureID logo = ArtOverride("title_logo", &lw, &lh);
            if (ImTextureID pic = ArtImage(1)) {
                if (logo && lw && lh) {
                    // rebuilt: the picture's side bands (a logo-free row, stretched), the copyright lines,
                    // then the user's high-resolution logo where the original sat
                    const float row = 272.0f / 448.0f;
                    dl->AddImage(pic, P(0, 0), P(512, 448), ImVec2(0, row), ImVec2(1, row), IM_COL32(255, 255, 255, 255));

                } else {
                    dl->AddImage(pic, P(0, 0), P(512, 448), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
                }
                static float sx[160], sy[160], sp[160], ph[160];
                static bool init = false;
                if (!init) {
                    uint32_t r = 0x9e3779b9u;
                    auto rnd = [&]() { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return float(r & 0xffffff) / float(0xffffff); };
                    for (int i = 0; i < 160; ++i) sx[i] = rnd() * 512, sy[i] = rnd() * 448, sp[i] = 4 + rnd() * 14, ph[i] = rnd() * 6.28f;
                    init = true;
                }
                for (int i = 0; i < 160; ++i) {
                    // the stars live in the dark side bands (the centre is a white glow)
                    const float xx = std::fmod(sx[i] + float(now) * sp[i], 512.0f);
                    const float edge = std::min(xx, 512.0f - xx);
                    if (edge > 120) continue;
                    const float tw2 = 0.5f + 0.5f * std::sin(float(now) * 2.2f + ph[i]);
                    const float fade = std::clamp((120 - edge) / 60.0f, 0.0f, 1.0f);
                    const ImVec2 c = P(xx, sy[i]);
                    const float rr = (0.6f + 0.8f * tw2) * k;
                    dl->AddCircleFilled(c, rr * 2.4f, IM_COL32(170, 210, 255, int(30 * tw2 * fade)), 10);
                    dl->AddCircleFilled(c, rr, IM_COL32(235, 245, 255, int(220 * tw2 * fade)), 8);
                }
            }
            const float t = tw * (1.0f - std::clamp(float(o.rgba) / 128.0f, 0.0f, 1.0f));
            if (logo && lw && lh && ArtImage(1)) {   // the logo: 380 px wide, centred over the original's place
                const float w2 = 344.0f, h2 = w2 * float(lh) / float(lw) * (LayoutKx() / LayoutK());
                const float top = 34.0f + std::max(0.0f, (200.0f - h2) * 0.5f);
                dl->AddImage(logo, P(256 - w2 * 0.5f, top), P(256 + w2 * 0.5f, top + h2), ImVec2(0, 0), ImVec2(1, 1),
                             IM_COL32(255, 255, 255, int(255 * std::min(1.0f, tw))));
            }
            if (logo && lw && lh && ArtImage(1)) {   // the copyright lines, set in the UI font
                const char* l1 = "Xenosaga\xe2\x84\xa2 EPISODE I  Der Wille zur Macht";
                const char* l2 = "\xc2\xa9 2001 NAMCO LTD., ALL RIGHTS RESERVED";
                const float s1 = 9.5f * k, s2 = 9.0f * k;
                const ImU32 c1 = IM_COL32(70, 80, 105, int(230 * tw)), c2 = IM_COL32(95, 105, 130, int(220 * tw));
                const ImVec2 w1 = font->CalcTextSizeA(s1, 1e9f, 0, l1), w2 = font->CalcTextSizeA(s2, 1e9f, 0, l2);
                const ImVec2 c = P(256, 412);
                // "Xenosaga" + a small raised TM + the rest (the font has no U+2122)
                const char* a1 = "Xenosaga";
                const char* b1 = " EPISODE I Der Wille zur Macht &";
                const float tms = s1 * 0.55f;
                const float wa = font->CalcTextSizeA(s1, 1e9f, 0, a1).x, wt = font->CalcTextSizeA(tms, 1e9f, 0, "TM").x,
                            wb = font->CalcTextSizeA(s1, 1e9f, 0, b1).x;
                float lx = c.x - (wa + wt + wb) * 0.5f;
                dl->AddText(font, s1, ImVec2(lx, c.y - w1.y), c1, a1);
                dl->AddText(font, tms, ImVec2(lx + wa, c.y - w1.y), c1, "TM");
                dl->AddText(font, s1, ImVec2(lx + wa + wt, c.y - w1.y), c1, b1);
                dl->AddText(font, s2, ImVec2(c.x - w2.x * 0.5f, c.y + 3 * k), c2, l2);
            }
            int rows[4], idx[4], n = 0;
            rows[n] = 0, idx[n++] = 0;
            rows[n] = 1, idx[n++] = 1;
            if (o.b != -1) rows[n] = o.b == 1 ? 3 : 2, idx[n++] = 2;
            rows[n] = 4, idx[n] = 4, n++;   // Settings (title.c: index 4)
            const float fs = 17.0f * k, rowh = 18.0f;
            float wmax = 0;
            for (int r = 0; r < n; ++r) wmax = std::max(wmax, font->CalcTextSizeA(fs, 1e9f, 0, kItem[rows[r]]).x);
            const float half = wmax * 0.5f / kx + 26.0f;
            const ImVec2 a = P(256 - half, 288 - 6), b = P(256 + half, 288 + rowh * float(n) + 4);
            Panel(dl, a, b, k, t * 0.92f);
            for (int r = 0; r < n; ++r) {
                const int item = idx[r];   // the game's selection index
                const bool sel = o.a == item;
                const float ry = 288 + rowh * float(r);
                if (sel) {
                    AnimState& cu = Anim(0x5449544Cull, 0, ry);
                    const float yy = Spring(cu, 0, ry, 22.0f).y;
                    GlowBar(dl, P(256 - half + 4, yy - 1), P(256 + half - 4, yy + rowh - 1), k, t, now, col::kCyan);
                    Chevron(dl, P(256 - half + 14, yy + rowh * 0.5f - 1), 6 * k, t, now);
                }
                const ImVec2 ts = font->CalcTextSizeA(fs, 1e9f, 0, kItem[rows[r]]);
                const ImVec2 c = P(256, ry + rowh * 0.5f - 1);
                HaloText(dl, font, fs, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), sel ? col::kText : col::kTextDim,
                         kItem[rows[r]], t);
            }
            break;
        }
        case GPU_KG_THUMB: {   // a save thumbnail (the game draws the picture): a frame and brackets around it
            const ImVec2 a = P(x - 1, y - 1), b = P(x + o.dw + 1, y + o.dh + 1);
            dl->AddRect(a, b, Alpha(col::kFrame, 0.85f * tw), 0, 0, 1.0f * k);
            DrawBrackets(dl, ImVec2(a.x - 2 * k, a.y - 2 * k), ImVec2(b.x + 2 * k, b.y + 2 * k), Alpha(col::kCyan, 0.8f * tw), 6 * k, 1.2f * k);
            break;
        }
        case GPU_KG_PAUSE: {   // a centred card: title, a rule, two prompts in a column
            AnimState& pa = Anim(0x50415553ull, 0, 8);
            const float t = FadeIn(pa, 6.0f) * tw;
            const float rise = Spring(pa, 0, 0, 16.0f).y;
            const float age = float(now - pa.born);
            anchor = kCentre;
            // a vignette over the game's dim layer: the card's area stays clearest
            {
                const ImVec4 ar = LayoutArea();
                const ImVec2 c0(ar.x, ar.y), c1(ar.z, ar.w);
                const float ex = (c1.x - c0.x) * 0.22f;
                const ImU32 e = IM_COL32(2, 2, 10, int(150 * t)), z = IM_COL32(2, 2, 10, 0);
                dl->AddRectFilledMultiColor(c0, ImVec2(c0.x + ex, c1.y), e, z, z, e);
                dl->AddRectFilledMultiColor(ImVec2(c1.x - ex, c0.y), c1, z, e, e, z);
                dl->AddRectFilled(c0, c1, IM_COL32(2, 2, 12, int(60 * t)));
            }
            const float cw = 176, chh = 112, cx0 = 256 - cw * 0.5f, cy0 = 224 - chh * 0.5f + rise;
            const ImVec2 a = P(cx0, cy0), b2 = P(cx0 + cw, cy0 + chh);
            Panel(dl, a, b2, k, t);
            // title
            const char* title = "P A U S E D";
            const float fs = 22.0f * k;
            const ImVec2 ts = font->CalcTextSizeA(fs, 1e9f, 0, title);
            const ImVec2 tc = P(256, cy0 + 26 + rise * 0);
            HaloText(dl, font, fs, ImVec2(tc.x - ts.x * 0.5f, tc.y - ts.y * 0.5f), col::kText, title, t);
            // a short rule under it that sweeps out from the centre
            {
                const float u = std::clamp(age / 0.4f, 0.0f, 1.0f), e = 1 - (1 - u) * (1 - u);
                const float half = 56.0f * e;
                const ImVec2 l = P(256 - half, cy0 + 44), r = P(256 + half, cy0 + 44);
                dl->AddRectFilledMultiColor(ImVec2(l.x, l.y), ImVec2((l.x + r.x) * 0.5f, l.y + 1.2f * k), Alpha(col::kCyan, 0), Alpha(col::kCyan, 0.9f * t),
                                            Alpha(col::kCyan, 0.9f * t), Alpha(col::kCyan, 0));
                dl->AddRectFilledMultiColor(ImVec2((l.x + r.x) * 0.5f, l.y), ImVec2(r.x, r.y + 1.2f * k), Alpha(col::kCyan, 0.9f * t), Alpha(col::kCyan, 0),
                                            Alpha(col::kCyan, 0), Alpha(col::kCyan, 0.9f * t));
            }
            // two rows: [button] label, the buttons in one column, equal slots
            const std::string lay = xsgcfg::Get("gamepad.layout");
            const int fam = PromptFamily();
            const char* start = fam == 3 ? "ENTER" : lay == "playstation" ? "OPTIONS" : lay == "nintendo" ? "+" : "MENU";
            const char* view = fam == 3 ? "BKSP" : lay == "playstation" ? "SHARE" : lay == "nintendo" ? "\xe2\x88\x92" : "VIEW";
            struct Row { int icon; const char *b, *l; } rows2[2] = {{kStart, start, "Resume"}, {kBack, view, "Settings"}};
            const float hs = 13.0f * k, slot = 22.0f;
            for (int r = 0; r < 2; ++r) {
                const float ry = cy0 + 62 + 22.0f * float(r);
                const ImVec2 sc = P(cx0 + 46, ry + 9);
                if (!PromptDraw(dl, rows2[r].icon, sc, slot * k, t)) {
                    const float bh = 15 * k, bw = std::max(slot * kx, font->CalcTextSizeA(10.0f * k, 1e9f, 0, rows2[r].b).x + 10 * kx);
                    dl->AddRectFilled(ImVec2(sc.x - bw * 0.5f, sc.y - bh * 0.5f), ImVec2(sc.x + bw * 0.5f, sc.y + bh * 0.5f),
                                      Alpha(IM_COL32(12, 12, 30, 240), t), bh * 0.5f);
                    dl->AddRect(ImVec2(sc.x - bw * 0.5f, sc.y - bh * 0.5f), ImVec2(sc.x + bw * 0.5f, sc.y + bh * 0.5f),
                                Alpha(col::kFrame, 0.8f * t), bh * 0.5f, 0, 1.0f);
                    const ImVec2 bs = font->CalcTextSizeA(10.0f * k, 1e9f, 0, rows2[r].b);
                    dl->AddText(font, 10.0f * k, ImVec2(sc.x - bs.x * 0.5f, sc.y - bs.y * 0.5f), Alpha(col::kText, t), rows2[r].b);
                }
                const ImVec2 ls = font->CalcTextSizeA(hs, 1e9f, 0, rows2[r].l);
                HaloText(dl, font, hs, ImVec2(P(cx0 + 66, 0).x, sc.y - ls.y * 0.5f), r == 0 ? col::kText : col::kTextDim, rows2[r].l, t);
            }
            break;
        }
        default:
            break;
        }
        if (clipped) dl->PopClipRect();
    }
    dl->PopClipRect();
    bool pause = false;
    for (uint32_t i = 0; i < m.nops; ++i) pause |= m.op[i].type == GPU_KG_PAUSE;
    if (!pause) {   // the camp menu (its list has "Game Options"): a Settings row under Money
        bool camp = false;
        for (uint32_t i = 0; i < m.nops && !camp; ++i)
            if (m.op[i].type == GPU_KG_TEXT) {
                const char* t = m.pool + std::min<uint32_t>(uint32_t(m.op[i].b), XSG_GPU_UI_MENU_POOL - 1);
                camp = std::strstr(std::string(t, size_t(std::max<int>(m.op[i].u, 0))).c_str(), "Game Options") != nullptr;
            }
        // the Reserve / Time / Money rows: the lowest ribbon in the right half, above the help bar
        float x0 = 0, x1 = 0, ybot = 0;
        for (uint32_t i = 0; camp && i < m.nops; ++i) {
            const GpuUiKgOp& r = m.op[i];
            if (r.type == GPU_KG_RECT && r.x > 250 && r.dw > 100 && r.dh >= 16 && r.y < helpTop - 20 && r.y + r.dh > ybot)
                x0 = float(std::max(r.x, 16)), x1 = float(std::min(r.x + r.dw, 496)), ybot = float(r.y + r.dh);
        }
        if (camp && ybot > 0 && ybot + 34 < helpTop) SettingsHint(dl, font, tw, x0, x1, ybot + 9);
    }
}

}  // namespace xsgui

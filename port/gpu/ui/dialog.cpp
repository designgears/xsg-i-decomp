/* Rebuilt UI: the dialog window.  See dialog.h. */
#include "dialog.h"
#include "gpu_ipc.h"
#include "../menu/cfg.h"
#include "../menu/option_rows.h"
#include "style.h"
#include "layout.h"
#include "art.h"
#include "../menu/menu_render.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iconv.h>
#include <set>
#include <vector>

namespace xsgui {
namespace {

GpuUiDialog* g_d;
GpuUiBattle* g_b;
GpuUiBattle g_bsnap;
GpuUiBattleCmd* g_c;
GpuUiBattleCmd g_csnap;
GpuUiKeyGuide* g_k;
GpuUiDmgList* g_dm;
GpuUiDmgList g_dmsnap;
GpuUiKeyGuide g_ksnap[XSG_GPU_UI_KG_CHANNELS];
GpuUiDialog g_snap;   // the last consistent copy

/* The game's own 0xD1xx symbols (font page 0x51): the ones seen in English text. */
const char* Symbol(uint8_t lo) {
    switch (lo) {
    case 0xb7: return "?";
    case 0xb8: return "!";
    case 0xb9: return ".";
    case 0xba: return ",";
    case 0xfd: return ":";
    default: return nullptr;
    }
}

void AppendUtf8(std::string& o, uint32_t c) {
    if (c < 0x80) o += char(c);
    else if (c < 0x800) { o += char(0xc0 | c >> 6); o += char(0x80 | (c & 63)); }
    else { o += char(0xe0 | c >> 12); o += char(0x80 | ((c >> 6) & 63)); o += char(0x80 | (c & 63)); }
}

/* One JIS X 0208 character (EUC bytes hi, lo) through iconv, full-width forms
   folded to ASCII (the menu font is Latin). */
void Jis(std::string& o, uint8_t hi, uint8_t lo) {
    static iconv_t cd = iconv_open("UTF-32LE", "EUC-JP");
    char in[2] = {char(hi), char(lo)};
    uint32_t u = 0;
    char* ip = in;
    char* op = reinterpret_cast<char*>(&u);
    size_t il = 2, ol = 4;
    if (cd == iconv_t(-1) || iconv(cd, &ip, &il, &op, &ol) == size_t(-1) || ol) {
        o += '?';
        return;
    }
    if (u == 0x3000) u = ' ';
    else if (u >= 0xff01 && u <= 0xff5e) u -= 0xfee0;
    else if (u == 0x3001) u = ',';
    else if (u == 0x3002) u = '.';
    else if (u == 0x30fb) u = 0xb7;
    else if (u == 0x2019 || u == 0x2018) u = '\'';
    else if (u == 0x201c || u == 0x201d) u = '"';
    AppendUtf8(o, u);
}

}  // namespace

std::string Decode(const char* s) {
    std::string o;
    static std::set<uint32_t> unknown;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(s);
    while (*p) {
        const uint8_t c = *p;
        if (c == 0x19) { p += p[1] ? 2 : 1; continue; }   // style code + its argument
        if (c < 0x20) { ++p; continue; }
        if (c < 0x80) { o += char(c); ++p; continue; }
        if (!p[1]) break;
        if (c == 0xd1) {
            if (const char* sym = Symbol(p[1])) o += sym;
            else if (unknown.insert(0xd100u | p[1]).second) {
                std::fprintf(stderr, "[ui] unknown symbol d1%02x\n", p[1]);
                o += '?';
            }
            p += 2;
            continue;
        }
        if (c >= 0xa1 && c <= 0xfe) Jis(o, c, p[1]);
        p += 2;
    }
    return o;
}

void Init(void* shm_base) {
    g_d = reinterpret_cast<GpuUiDialog*>(static_cast<uint8_t*>(shm_base) + XSG_GPU_UI_OFF);
    g_b = reinterpret_cast<GpuUiBattle*>(static_cast<uint8_t*>(shm_base) + XSG_GPU_UI_BATTLE_OFF);
    g_c = reinterpret_cast<GpuUiBattleCmd*>(static_cast<uint8_t*>(shm_base) + XSG_GPU_UI_CMD_OFF);
    g_k = reinterpret_cast<GpuUiKeyGuide*>(static_cast<uint8_t*>(shm_base) + XSG_GPU_UI_KG_OFF);
    g_dm = reinterpret_cast<GpuUiDmgList*>(static_cast<uint8_t*>(shm_base) + XSG_GPU_UI_DMG_OFF);
}

bool DialogActive(uint32_t vsync) {
    if (!g_d) return false;
    for (int tries = 0; tries < 4; ++tries) {   // seqlock read
        const uint32_t s0 = __atomic_load_n(&g_d->seq, __ATOMIC_ACQUIRE);
        if (s0 & 1) continue;
        GpuUiDialog c;
        std::memcpy(&c, const_cast<const GpuUiDialog*>(g_d), sizeof c);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_d->seq, __ATOMIC_ACQUIRE) == s0) {
            g_snap = c;
            break;
        }
    }
    // the game runs a few vblanks ahead of the renderer (the command ring): signed age
    const int32_t age = int32_t(vsync - g_snap.frame);
    return g_snap.frame && g_snap.w > 0 && age <= 3 && age >= -120;
}

void DialogDraw(ImDrawList* dl, ImFont* font, float gx, float gy, float gw, float gh, double now) {
    using namespace xsgmenu;   // the menu theme: col::, DrawBrackets
    const GpuUiDialog& d = g_snap;
    const float sx = gw / 512.0f, sy = gh / 448.0f;
    // the game's window rectangle, a little roomier
    // where the game's own frame sits (its text box starts 26 px under the window origin)
    LayoutBegin(gx, gy, gw, gh);
    const ImVec2 m0 = Map(float(d.x - 16), float(d.y + 24), kCentre), m1 = Map(float(d.x + d.w + 4), float(std::min(d.y + d.h + 34, 442)), kCentre);
    float x0 = m0.x, y0 = m0.y, x1 = m1.x, y1 = m1.y;
    // the menu's look (menu/option_rows.h col::, menu.cpp Panel): indigo glass, a doubled
    // periwinkle frame and corner brackets; cyan for the accents
    const float k = sy;   // one PS2 pixel on screen
    // opens with a fade and a short rise; the speaker's rule sweeps in
    AnimState& an = Anim(0xD1A1000000000000ull, 0, 10 * k);
    const float t = FadeIn(an, 6.0f);
    const float rise = Spring(an, 0, 0, 16.0f).y;
    y0 += rise; y1 += rise;
    Panel(dl, ImVec2(x0, y0), ImVec2(x1, y1), k, t, false, 0, d.name[0] ? 14 : 15);   // the speaker plate takes the top-left
    dl->AddRect(ImVec2(x0 + 2 * k, y0 + 2 * k), ImVec2(x1 - 2 * k, y1 - 2 * k), IM_COL32(150, 180, 235, int(36 * t)), 0, 0, 1.0f);
    const float fs = 25.5f * sy;   // body text: the original's proportions (its font is ~1.25x the UI font's width)
    // speaker: a plate on the frame's top edge, a cyan rule under the name
    if (d.name[0]) {
        const std::string name = Decode(d.name);
        const float ns = 16.0f * k;
        const ImVec2 tsz = font->CalcTextSizeA(ns, 1e9f, 0, name.c_str());
        const float px0 = x0 + 12 * k, px1 = px0 + tsz.x + 20 * k, py1 = y0 + 1 * k, py0 = py1 - ns - 7 * k;
        Shadow(dl, ImVec2(px0, py0), ImVec2(px1, py1), k, t);
        dl->AddRectFilledMultiColor(ImVec2(px0, py0), ImVec2(px1, py1), IM_COL32(48, 50, 112, int(245 * t)),
                                    IM_COL32(34, 36, 90, int(245 * t)), IM_COL32(24, 24, 64, int(245 * t)),
                                    IM_COL32(30, 30, 80, int(245 * t)));
        dl->AddRect(ImVec2(px0, py0), ImVec2(px1, py1), IM_COL32(150, 180, 235, int(150 * t)), 0, 0, 1.0f * k);
        SweepRule(dl, ImVec2(px0, py1), ImVec2(x1 - 12 * k, py1), k, t * 0.55f, float(now - an.born), col::kCyan);
        dl->AddRectFilled(ImVec2(px0, py1 - 1.5f * k), ImVec2(px0 + 28 * k, py1 + 0.5f * k), Alpha(col::kCyan, t));
        HaloText(dl, font, ns, ImVec2(px0 + 10 * k, py0 + 3 * k), col::kText, name.c_str(), t);
    }
    // body: the lines typed out so far, with a soft shadow
    float ty = y0 + 12 * sy;
    for (uint32_t i = 0; i < d.nlines && i < XSG_GPU_UI_LINES; ++i) {
        const std::string tx = Decode(d.line[i]);
        if (!tx.empty()) RichText(dl, font, fs, x0 + 22 * sx, ty, col::kText, tx.c_str(), t);
        ty += 26.0f * sy;
    }
    // page arrow (the game waits for a key)
    if (d.state == 8 || d.state == 0xb || d.state == 0x11) {
        const float b = float(std::sin(now * 5.0) * 2.5) * sy;
        const float ax = x1 - 26 * sx, ay = y1 - 18 * sy + b, a = 7.0f * sy;
        dl->AddTriangleFilled(ImVec2(ax - a, ay - a * 0.6f), ImVec2(ax + a, ay - a * 0.6f), ImVec2(ax, ay + a * 0.6f),
                              col::kCyan);
    }
}

namespace {
bool Fresh(uint32_t vsync, uint32_t frame) {
    const int32_t age = int32_t(vsync - frame);
    return frame && age <= 3 && age >= -2000;   // the game may run far ahead (uncapped)
}
}  // namespace

bool BattleActive(uint32_t vsync) {
    if (!g_b) return false;
    for (int tries = 0; tries < 4; ++tries) {
        const uint32_t s0 = __atomic_load_n(&g_b->seq, __ATOMIC_ACQUIRE);
        if (s0 & 1) continue;
        GpuUiBattle c;
        std::memcpy(&c, const_cast<const GpuUiBattle*>(g_b), sizeof c);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_b->seq, __ATOMIC_ACQUIRE) == s0) { g_bsnap = c; break; }
    }
    for (int tries = 0; g_c && tries < 4; ++tries) {
        const uint32_t s0 = __atomic_load_n(&g_c->seq, __ATOMIC_ACQUIRE);
        if (s0 & 1) continue;
        GpuUiBattleCmd c;
        std::memcpy(&c, const_cast<const GpuUiBattleCmd*>(g_c), sizeof c);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_c->seq, __ATOMIC_ACQUIRE) == s0) { g_csnap = c; break; }
    }
    for (int ch = 0; g_k && ch < XSG_GPU_UI_KG_CHANNELS; ++ch) {
        GpuUiKeyGuide* src = g_k + ch;
        GpuUiKeyGuide& dst = g_ksnap[ch];
        for (int tries = 0; tries < 4; ++tries) {
            const uint32_t s0 = __atomic_load_n(&src->seq, __ATOMIC_ACQUIRE);
            if (s0 & 1) continue;
            uint32_t n = src->nops;
            n = std::min<uint32_t>(n, XSG_GPU_UI_KG_OPS);
            std::memcpy(dst.op, const_cast<const GpuUiKgOp*>(src->op), n * sizeof(GpuUiKgOp));
            const uint32_t fr = src->frame;
            __atomic_thread_fence(__ATOMIC_ACQUIRE);
            if (__atomic_load_n(&src->seq, __ATOMIC_ACQUIRE) == s0) { dst.nops = n; dst.frame = fr; break; }
        }
    }
    for (const GpuUiKeyGuide& kg : g_ksnap)
        if (Fresh(vsync, kg.frame)) return true;
    for (int tries = 0; g_dm && tries < 4; ++tries) {
        const uint32_t s0 = __atomic_load_n(&g_dm->seq, __ATOMIC_ACQUIRE);
        if (s0 & 1) continue;
        GpuUiDmgList c;
        std::memcpy(&c, const_cast<const GpuUiDmgList*>(g_dm), sizeof c);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_dm->seq, __ATOMIC_ACQUIRE) == s0) { g_dmsnap = c; break; }
    }
    for (const GpuUiDmg& d : g_dmsnap.d)
        if (Fresh(vsync, d.frame)) return true;
    if (Fresh(vsync, g_csnap.frame)) return true;
    for (const GpuUiUnit& u : g_bsnap.unit)
        if (Fresh(vsync, u.frame)) return true;
    return false;
}

namespace {
int Layout() {   // 0 xbox, 1 playstation, 2 nintendo
    static std::string last;
    static int v;
    const std::string l = xsgcfg::Get("gamepad.layout");
    if (l != last) { last = l; v = l == "playstation" ? 1 : l == "nintendo" ? 2 : 0; }
    return v;
}
}  // namespace

void Glyph(ImDrawList* dl, ImFont* font, float cx, float cy, float r, int pos, bool dim) {
    const int lay = Layout();
    const int a = dim ? 110 : 255;
    const ImVec2 c(cx, cy);
    dl->AddCircleFilled(c, r, IM_COL32(12, 12, 30, dim ? 170 : 230), 24);
    if (lay == 1) {   // PlayStation: the four shapes in their colours
        static const ImU32 kc[] = {IM_COL32(124, 178, 232, 255), IM_COL32(232, 96, 110, 255), IM_COL32(214, 140, 206, 255),
                                   IM_COL32(80, 204, 168, 255)};
        const ImU32 cc = (kc[pos & 3] & 0x00ffffffu) | (ImU32(a) << 24);
        const float s = r * 0.48f, t = std::max(1.0f, r * 0.16f);
        dl->AddCircle(c, r - t * 0.5f, (cc & 0x00ffffffu) | (ImU32(a / 3) << 24), 24, t * 0.6f);
        switch (pos & 3) {
        case 0: dl->AddLine(ImVec2(cx - s, cy - s), ImVec2(cx + s, cy + s), cc, t); dl->AddLine(ImVec2(cx - s, cy + s), ImVec2(cx + s, cy - s), cc, t); break;
        case 1: dl->AddCircle(c, s * 1.05f, cc, 20, t); break;
        case 2: dl->AddRect(ImVec2(cx - s * 0.9f, cy - s * 0.9f), ImVec2(cx + s * 0.9f, cy + s * 0.9f), cc, 0, 0, t); break;
        default: dl->AddTriangle(ImVec2(cx, cy - s * 1.05f), ImVec2(cx + s * 1.05f, cy + s * 0.75f), ImVec2(cx - s * 1.05f, cy + s * 0.75f), cc, t); break;
        }
        return;
    }
    // Xbox: coloured letters; Nintendo: white letters (positions swap A/B and X/Y)
    static const char* const kXbox[] = {"A", "B", "X", "Y"};
    static const char* const kNin[] = {"B", "A", "Y", "X"};
    static const ImU32 kXc[] = {IM_COL32(110, 196, 84, 255), IM_COL32(232, 76, 64, 255), IM_COL32(64, 140, 232, 255),
                                IM_COL32(244, 196, 50, 255)};
    const ImU32 cc = lay == 2 ? IM_COL32(236, 236, 244, a) : (kXc[pos & 3] & 0x00ffffffu) | (ImU32(a) << 24);
    dl->AddCircle(c, r - 0.5f, (cc & 0x00ffffffu) | (ImU32(a / 2) << 24), 24, std::max(1.0f, r * 0.1f));
    const char* t = (lay == 2 ? kNin : kXbox)[pos & 3];
    const float fs = r * 1.35f;
    const ImVec2 ts = font->CalcTextSizeA(fs, 1e9f, 0, t);
    dl->AddText(font, fs, ImVec2(cx - ts.x * 0.5f, cy - ts.y * 0.5f), cc, t);
}

float ShoulderGlyph(ImDrawList* dl, ImFont* font, float x, float cy, float h, int side) {
    const int lay = Layout();
    static const char* const k[3][2] = {{"LB", "RB"}, {"L1", "R1"}, {"L", "R"}};
    const char* t = k[lay][side & 1];
    const float fs = h * 0.78f;
    const ImVec2 ts = font->CalcTextSizeA(fs, 1e9f, 0, t);
    const float w = ts.x + h * 0.7f;
    dl->AddRectFilled(ImVec2(x, cy - h * 0.5f), ImVec2(x + w, cy + h * 0.5f), IM_COL32(12, 12, 30, 230), h * 0.3f);
    dl->AddRect(ImVec2(x, cy - h * 0.5f), ImVec2(x + w, cy + h * 0.5f), IM_COL32(150, 180, 235, 160), h * 0.3f, 0, 1.0f);
    dl->AddText(font, fs, ImVec2(x + (w - ts.x) * 0.5f, cy - ts.y * 0.5f), xsgmenu::col::kText, t);
    return w;
}

namespace {
// the next button symbol in s at or after i: its byte length (0: none) and position (Glyph)
size_t NextButton(const char* s, size_t i, size_t* len, int* pos) {
    for (; s[i]; ++i) {
        const unsigned char c0 = static_cast<unsigned char>(s[i]);
        if (c0 == 0xe2 && static_cast<unsigned char>(s[i + 1]) == 0x96) {
            const unsigned char c2 = static_cast<unsigned char>(s[i + 2]);
            if (c2 == 0xb3) { *len = 3; *pos = 3; return i; }   // U+25B3 triangle
            if (c2 == 0xa1) { *len = 3; *pos = 2; return i; }   // U+25A1 square
        }
        if (c0 == 0xe2 && static_cast<unsigned char>(s[i + 1]) == 0x97 && static_cast<unsigned char>(s[i + 2]) == 0x8b) {
            *len = 3; *pos = 1; return i;   // U+25CB circle
        }
        if (c0 == 0xc3 && static_cast<unsigned char>(s[i + 1]) == 0x97) { *len = 2; *pos = 0; return i; }   // U+00D7 cross
    }
    *len = 0;
    return i;
}
}  // namespace

float RichText(ImDrawList* dl, ImFont* font, float size, float x, float y, unsigned col, const char* s, float t) {
    const float x0 = x;
    size_t i = 0;
    for (;;) {
        size_t len;
        int pos;
        const size_t j = NextButton(s, i, &len, &pos);
        if (j > i) {
            const std::string seg(s + i, j - i);
            HaloText(dl, font, size, ImVec2(x, y), col, seg.c_str(), t);
            x += font->CalcTextSizeA(size, 1e9f, 0, seg.c_str()).x;
        }
        if (!len) break;
        const float r = size * 0.42f;
        Glyph(dl, font, x + r + size * 0.08f, y + size * 0.55f, r, pos, t < 0.5f);
        x += 2 * r + size * 0.16f;
        i = j + len;
    }
    return x - x0;
}

float RichTextWidth(ImFont* font, float size, const char* s) {
    float w = 0;
    size_t i = 0;
    for (;;) {
        size_t len;
        int pos;
        const size_t j = NextButton(s, i, &len, &pos);
        if (j > i) w += font->CalcTextSizeA(size, 1e9f, 0, std::string(s + i, j - i).c_str()).x;
        if (!len) break;
        w += 2 * size * 0.42f + size * 0.16f;
        i = j + len;
    }
    return w;
}

namespace {
/* The attack panel laid out from the captured key guide (channel 0): one glass row per
   command with its button inside, the label left-aligned, the kind stripe, the cost box
   joined to its row, and the LB / RB footer in a container under the stack. */
void KeyGuideRows(ImDrawList* dl, ImFont* font, const GpuUiKeyGuide& g) {
    using namespace xsgmenu;
    const float k = LayoutK(), kx = LayoutKx();
    auto P = [&](float x, float y) { return Map(x, y, kLeft); };
    static const int kPos[] = {2, 3, 0, 1};
    static const ImU32 kAccent[] = {IM_COL32(118, 170, 255, 255), IM_COL32(255, 96, 170, 255), IM_COL32(110, 220, 150, 255)};
    struct Row { int y, btn = 0, kind = 0; std::string label; std::vector<const GpuUiKgOp*> cost; };
    std::vector<Row> rows;
    const GpuUiKgOp* strip = nullptr;
    for (uint32_t i = 0; i < g.nops; ++i) {
        const GpuUiKgOp& o = g.op[i];
        if (o.type == GPU_KG_SPRITE && o.a == 0x4c && o.b == 0xaf) { strip = &o; continue; }
        if (o.type == GPU_KG_BUTTON) { rows.push_back(Row{o.y}); rows.back().btn = o.a; continue; }
        if (rows.empty()) continue;
        Row& r = rows.back();
        if (o.type == GPU_KG_FRAME && o.y == r.y && r.label.empty() && r.cost.empty()) r.kind = std::clamp(o.b, 0, 2);
        else if (o.type == GPU_KG_TEXT && r.label.empty()) r.label = Decode(o.text);
        else if (o.type == GPU_KG_NUMBER || (o.type == GPU_KG_SPRITE && o.v == 0xAE)) r.cost.push_back(&o);
    }
    if (rows.empty()) return;
    const float ls = 13.0f * k, rh = 22.0f, x0 = 8.0f;
    float labelW = 0;
    for (const Row& r : rows) labelW = std::max(labelW, font->CalcTextSizeA(ls, 1e9f, 0, r.label.c_str()).x / kx);
    const float rw = std::max(96.0f, labelW + 40.0f);
    const double now = AnimNow();
    for (size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        AnimState& ra = Anim(0x4B52000000000000ull + i, -18, 0);
        if (now - ra.born < double(i) * 0.04) ra.a = 0;
        const float t = FadeIn(ra, 8.0f), dx = Spring(ra, 0, 0, 16.0f).x;
        const bool off = r.btn > 4;
        const float rx = x0 + dx, ry = float(r.y) + 2;   // aligned, no stagger
        float right = rx + rw;
        const float costW = r.cost.empty() ? 0.0f : 92.0f;
        const ImVec2 a = P(rx, ry), b = P(right + costW, ry + rh);
        Panel(dl, a, b, k, t * (off ? 0.75f : 1.0f));
        const ImU32 acc = kAccent[size_t(r.kind)];
        dl->AddRectFilledMultiColor(a, P(rx + 40, ry + rh), Alpha(acc, 0.20f * t), Alpha(acc, 0), Alpha(acc, 0), Alpha(acc, 0.20f * t));
        dl->AddRectFilled(a, P(rx + 3, ry + rh), Alpha(acc, t));
        if (r.btn >= 1) {
            const ImVec2 c = P(rx + 15, ry + rh * 0.5f);
            Glyph(dl, font, c.x, c.y, 7.5f * k, kPos[(r.btn - 1) & 3], off);
        }
        const ImVec2 lsz = font->CalcTextSizeA(ls, 1e9f, 0, r.label.c_str());
        HaloText(dl, font, ls, ImVec2(P(rx + 28, 0).x, P(0, ry + rh * 0.5f).y - lsz.y * 0.5f), off ? col::kTextOff : col::kText,
                 r.label.c_str(), t);
        if (!r.cost.empty()) {   // cost box: ammo, EP cost, AP cost
            dl->AddLine(P(right, ry + 3), P(right, ry + rh - 3), Alpha(col::kFrame, 0.4f * t), 1.0f);
            int nums[3] = {-1, -1, -1}, ni = 0;
            for (const GpuUiKgOp* c : r.cost)
                if (c->type == GPU_KG_NUMBER && ni < 3) nums[ni++] = c->a;
            if (ni == 2) nums[2] = nums[1], nums[1] = nums[0], nums[0] = -1;   // no ammo count
            char buf[16];
            const float cy = P(0, ry + rh * 0.5f).y, ns = 14.0f * k, ts = 9.5f * k;
            auto num = [&](float xr, int v, int digits) {
                if (v < 0) return;
                std::snprintf(buf, sizeof buf, "%0*d", digits, v);
                const ImVec2 sz = font->CalcTextSizeA(ns, 1e9f, 0, buf);
                HaloText(dl, font, ns, ImVec2(P(xr, 0).x - sz.x, cy - sz.y * 0.5f), col::kText, buf, t);
            };
            auto tag = [&](float xl, const char* s) {
                const ImVec2 sz = font->CalcTextSizeA(ts, 1e9f, 0, s);
                dl->AddText(font, ts, ImVec2(P(xl, 0).x, cy - sz.y * 0.5f + 1), Alpha(col::kTextDim, t), s);
            };
            num(right + 22, nums[0], 2);
            tag(right + 30, "EP");
            num(right + 56, nums[1], 2);
            tag(right + 64, "AP");
            num(right + 84, nums[2], 1);
        }
    }
    if (strip) {   // the footer: LB Change ... Boost RB, in a bar under the stack
        const float fy = float(strip->y) - 2, fw = rw;
        const ImVec2 a = P(x0, fy), b = P(x0 + fw, fy + 16);
        Panel(dl, a, b, k, 0.85f, false, 0, 0);
        const float cy = (a.y + b.y) * 0.5f, h = 11.0f * k, ts = 10.0f * k;
        float cx = a.x + 4 * kx;
        cx += ShoulderGlyph(dl, font, cx, cy, h, 0) + 5 * kx;
        const ImVec2 cs = font->CalcTextSizeA(ts, 1e9f, 0, "Change"), bs = font->CalcTextSizeA(ts, 1e9f, 0, "Boost");
        dl->AddText(font, ts, ImVec2(cx, cy - cs.y * 0.5f), col::kTextDim, "Change");
        const float w = font->CalcTextSizeA(h * 0.78f, 1e9f, 0, "RB").x + h * 0.7f;
        ShoulderGlyph(dl, font, b.x - 4 * kx - w, cy, h, 1);
        dl->AddText(font, ts, ImVec2(b.x - 4 * kx - w - 5 * kx - bs.x, cy - bs.y * 0.5f), col::kTextDim, "Boost");
    }
}

/* The key guide (menuKeyGuideDisp, captured): one row per button, the
   technique in a frame coloured by kind, the weapon's ammo / EP / AP, and the
   L1 / R1 strip.  Icon ids (uvTbl): 1 square, 2 triangle, 3 cross, 4 circle,
   +4 the unavailable variant. */
void KgDraw(ImDrawList* dl, ImFont* font, float gx, float gy, float gw, float gh, const GpuUiKeyGuide& g, uint64_t akey, bool rows) {
    using namespace xsgmenu;
    LayoutBegin(gx, gy, gw, gh);
    const float k = LayoutK(), kx = LayoutKx();
    // the key guide and name plate (channels 0, 1) sit at the left; captured art elsewhere
    // (party portraits: channel 3) anchors like the panel it belongs to
    int anchor = kLeft;
    auto P = [&](float x, float y) { return Map(x, y, anchor); };
    static const int kPos[] = {2, 3, 0, 1};   // id 1..4 -> west, north, south, east
    static const ImU32 kAccent[] = {IM_COL32(118, 170, 255, 255), IM_COL32(255, 96, 170, 255), IM_COL32(110, 220, 150, 255)};
    char b[16];
    // layout: each name frame fits its text (padding either side, a minimum width) instead of
    // the game's fixed 172 px; whatever sits to its right in the row moves in by the same amount
    std::vector<GpuUiKgOp> ops(g.op, g.op + g.nops);
    for (size_t f = 0; f < ops.size(); ++f) {
        GpuUiKgOp& fr = ops[f];
        if (fr.type != GPU_KG_FRAME) continue;
        const int fx0 = fr.x, fx1 = fr.x + 2 * fr.a;
        for (size_t t = f + 1; t < ops.size(); ++t) {
            GpuUiKgOp& tx = ops[t];
            if (tx.type == GPU_KG_FRAME) break;
            if (tx.type != GPU_KG_TEXT || tx.y < fr.y || tx.y >= fr.y + 24 || tx.x < fx0 || tx.x >= fx1) continue;
            const std::string u = Decode(tx.text);
            const float want = RichTextWidth(font, 13.0f * k, u.c_str()) / kx + 30.0f;
            const int half = std::min(fr.a, std::max(44, int(std::ceil(want * 0.5f))));
            const int delta = 2 * (fr.a - half);
            fr.a = half;
            tx.x = fr.x + 2;   // stays inside its frame (centred there when drawn)
            if (delta > 0)
                for (GpuUiKgOp& r : ops)
                    if (&r != &fr && r.y >= fr.y - 2 && r.y < fr.y + 24 && r.x >= fx1) r.x -= delta;
            break;
        }
    }
    for (uint32_t i = 0; i < uint32_t(ops.size()); ++i) {
        const GpuUiKgOp& o = ops[i];
        if (!rows && akey != 0x4B47000000000000ull + (1ull << 32)) anchor = AnchorFor(float(o.x), float(o.x) + 0x92);
        // each row (keyed by its y) eases in from the left, a beat after the one above
        AnimState& ra = Anim(rows ? akey + uint32_t(o.y / 28) : akey, rows ? -18.0f : 0.0f, 0);
        if (rows && AnimNow() - ra.born < (o.y / 28 % 8) * 0.04) ra.a = 0;
        const float rt = FadeIn(ra, 8.0f);
        const float x = float(o.x) + Spring(ra, 0, 0, 16.0f).x, y = float(o.y);
        switch (o.type) {
        case GPU_KG_BUTTON:
            if (o.a >= 1 && o.a <= 8) {
                const ImVec2 c = P(x + 11, y + 12);
                Glyph(dl, font, c.x, c.y, 9.5f * k * (0.7f + 0.3f * rt), kPos[(o.a - 1) & 3], o.a > 4 || rt < 0.5f);
            }
            break;
        case GPU_KG_FRAME: {
            const ImVec2 a = P(x, y + 1), c = P(x + 2 * o.a, y + 23);
            const ImU32 acc = kAccent[std::min(2, std::max(0, o.b))];
            Panel(dl, a, c, k, rt);
            dl->AddRectFilledMultiColor(a, ImVec2(a.x + 40 * kx, c.y), Alpha(acc, 0.22f * rt), Alpha(acc, 0), Alpha(acc, 0),
                                        Alpha(acc, 0.22f * rt));
            dl->AddRectFilled(a, ImVec2(a.x + 3 * kx, c.y), Alpha(acc, rt));
            break;
        }
        case GPU_KG_TEXT: {
            // centred in the captured frame it sits in (the game centres it by character count)
            const std::string t = Decode(o.text);
            const float fs = 13.0f * k;
            const ImVec2 ts = font->CalcTextSizeA(fs, 1e9f, 0, t.c_str());
            float fl = float(o.x) - 2, fr = fl + 10 * float(std::strlen(o.text)) + 4;
            for (uint32_t j = i; j-- > 0;) {
                const GpuUiKgOp& f = ops[j];
                if (f.type == GPU_KG_FRAME && o.y >= f.y && o.y < f.y + 24 && o.x >= f.x && o.x < f.x + 2 * f.a) {
                    fl = float(f.x) + Spring(ra, 0, 0, 16.0f).x * 0;
                    fr = float(f.x + 2 * f.a);
                    break;
                }
            }
            const float dx = x - float(o.x);
            const ImVec2 l = P(fl + dx, y), r = P(fr + dx, y);
            HaloText(dl, font, fs, ImVec2((l.x + r.x - ts.x) * 0.5f, P(x, y + 9).y - ts.y * 0.5f), col::kText, t.c_str(), rt);
            break;
        }
        case GPU_KG_NUMBER: {
            // menuNumPut styles: 1 small (10 x 13), 2 large (12 x 16+), others 13 px
            const int style = o.text[0] ? o.text[0] - '0' : 0;
            const float nh = (style == 2 || o.b >= 4) ? 20.0f : 13.0f;   // four-digit counters (the radar's distance): large
            std::snprintf(b, sizeof b, "%0*d", std::max(1, std::min(6, o.b)), o.a);
            float base = y + (style == 2 ? 16.0f : 11.0f);   // the baseline, game pixels
            for (const GpuUiKgOp& u : ops)   // a unit label right of the number (the radar's "M"): share its baseline
                if (u.type == GPU_KG_ART && u.x > o.x && u.x < o.x + 90 && std::abs(u.y - o.y) < 12) { base = float(u.y + u.dh) - 1; break; }
            HaloText(dl, font, nh * k, ImVec2(P(x, 0).x, P(0, base).y - nh * k * 0.86f), col::kText, b, rt);
            break;
        }
        case GPU_KG_ART: {
            if (o.u == 0xC8 && o.v == 0xA0) break;   // the game's turn glow (a blue box): the panel's own glow replaces it
            if (o.w == 0x28 && o.h == 0x50 && o.dw == 0x28) {   // a party portrait: framed like the panel
                const ImVec2 pa = P(x, y), pb = P(x + o.dw, y + o.dh);
                Shadow(dl, pa, pb, k, rt);
                uint32_t set;
                std::memcpy(&set, o.text + 4, 4);
                ArtSprite(dl, uint64_t(o.tex0_lo) | uint64_t(o.tex0_hi) << 32, 0, float(o.u), float(o.v), float(o.w), float(o.h), pa, pb,
                          o.rgba, rt, false, set);
                dl->AddRect(ImVec2(pa.x - 1, pa.y - 1), ImVec2(pb.x + 1, pb.y + 1), Alpha(col::kFrame, 0.8f * rt), 0, 0, 1.0f * k);
                break;
            }
            // the game's own art, in the game's order; ALPHA B = 2 (0): additive, C = 2: FIX alpha
            const bool add = ((o.a >> 2) & 3) == 2;
            const float fix = ((o.a >> 4) & 3) == 2 ? float(o.b) / 128.0f : 1.0f;
            if (add) dl->AddCallback(xsgmenu::BlendAdd, nullptr);
            uint32_t set;
            std::memcpy(&set, o.text + 4, 4);
            ArtSprite(dl, uint64_t(o.tex0_lo) | uint64_t(o.tex0_hi) << 32, 0, float(o.u), float(o.v), float(o.w), float(o.h), P(x, y), P(x + o.dw, y + o.dh), o.rgba,
                      rt * fix, false, set);
            if (add) dl->AddCallback(xsgmenu::BlendAlpha, nullptr);
            break;
        }
        case GPU_KG_QUAD: {   // a textured quad (the boost gauge); strip order 0 1 2 3 -> perimeter 0 1 3 2
            int16_t c[6];
            std::memcpy(c, o.text, sizeof c);
            const int al = static_cast<unsigned char>(o.text[12]), fix = static_cast<unsigned char>(o.text[13]);
            const bool add = ((al >> 2) & 3) == 2;
            const float fa = ((al >> 4) & 3) == 2 ? float(fix) / 128.0f : 1.0f;
            const uint64_t t0 = uint64_t(o.tex0_lo) | uint64_t(o.tex0_hi) << 32;
            uint32_t tw = 0, th = 0;
            const ImTextureID id = ArtTexture(t0, 0, &tw, &th, uint32_t(o.b));
            if (!id || !tw || !th) break;
            const float dx = x - float(o.x);
            const ImVec2 p0 = P(float(o.x) + dx, float(o.y)), p1 = P(float(c[0]) + dx, float(c[1])), p2 = P(float(c[2]) + dx, float(c[3])),
                         p3 = P(float(c[4]) + dx, float(c[5]));
            auto uv = [&](float u, float v) { return ImVec2(u / float(tw), v / float(th)); };
            const ImVec2 t0v = uv(o.u, o.v), t1v = uv(o.w, o.h), t2v = uv(o.dw, o.dh),
                         t3v = uv(float(int16_t(o.a & 0xffff)), float(int16_t(uint32_t(o.a) >> 16)));
            auto ch = [&](int sh) { return std::min(255u, ((o.rgba >> sh) & 0xffu) * 2); };
            const ImU32 colr = IM_COL32(ch(0), ch(8), ch(16), int(float(ch(24)) * rt * fa));
            if (add) dl->AddCallback(xsgmenu::BlendAdd, nullptr);
            dl->AddImageQuad(id, p0, p1, p3, p2, t0v, t1v, t3v, t2v, colr);
            if (add) dl->AddCallback(xsgmenu::BlendAlpha, nullptr);
            break;
        }
        case GPU_KG_LINE:
            dl->AddLine(P(x, y), P(x + o.dw, y + o.dh),
                        IM_COL32(std::min(255u, (o.rgba & 0xff) * 2), std::min(255u, ((o.rgba >> 8) & 0xff) * 2),
                                 std::min(255u, ((o.rgba >> 16) & 0xff) * 2), int(std::min(255u, ((o.rgba >> 24) & 0xff) * 2) * rt)),
                        1.2f * k);
            break;
        case GPU_KG_SPRITE:
            if (o.a == 0x12 || o.a == 0x24)   // the EP / AP labels
                dl->AddText(font, 10.0f * k, P(x, y), col::kTextDim, o.a == 0x12 ? "EP" : "AP");
            else if (o.a == 0x4c && o.b == 0xaf) {   // the L1 change / boost R1 strip
                const float h = 13.0f * k, cy = P(x, y + 5).y;
                float cx = P(x, y).x;
                cx += ShoulderGlyph(dl, font, cx, cy, h, 0) + 4 * kx;
                dl->AddText(font, 11.0f * k, ImVec2(cx, cy - 6.5f * k), col::kTextDim, "Change");
                cx = P(x + 0xb2, y).x;
                const ImVec2 bs = font->CalcTextSizeA(11.0f * k, 1e9f, 0, "Boost");
                float w = font->CalcTextSizeA(h * 0.78f, 1e9f, 0, "RB").x + h * 0.7f;
                ShoulderGlyph(dl, font, cx - w, cy, h, 1);
                dl->AddText(font, 11.0f * k, ImVec2(cx - w - 4 * kx - bs.x, cy - 6.5f * k), col::kTextDim, "Boost");
            }
            break;
        }
    }
}

/* Damage / heal numbers: pop in oversized and settle, a soft glow in the type's colour
   under a dark outline, and a fade as the game's countdown runs out. */
void DmgDraw(ImDrawList* dl, ImFont* font, float gx, float gy, float gw, float gh, uint32_t vsync) {
    LayoutBegin(gx, gy, gw, gh);
    const float k = LayoutK(), kx = LayoutKx();
    auto P = [&](float x, float y) { return Map(x, y, kCentre); };
    char b[16];
    for (const GpuUiDmg& d : g_dmsnap.d) {
        if (!Fresh(vsync, d.frame)) continue;
        AnimState& an = Anim(0xD3610000ull ^ uint32_t(d.key), 0, 0);
        const float age = float(AnimNow() - an.born);
        const float pop = age < 0.18f ? 1.0f + 0.55f * (1.0f - age / 0.18f) * (1.0f - age / 0.18f) : 1.0f;
        const float t = std::min(1.0f, age / 0.06f) * std::clamp(float(d.count) / 6.0f, 0.0f, 1.0f);
        ImU32 core, glow;
        switch (d.type) {
        case 2: case 4: core = IM_COL32(200, 255, 205, 255); glow = IM_COL32(60, 230, 120, 255); break;
        case 3: case 9: case 10: core = IM_COL32(205, 245, 255, 255); glow = IM_COL32(60, 200, 255, 255); break;
        default: core = IM_COL32(255, 248, 236, 255); glow = IM_COL32(255, 150, 60, 255); break;
        }
        std::snprintf(b, sizeof b, "%d", d.value);
        const float fs = 38.0f * k * pop;
        const ImVec2 sz = font->CalcTextSizeA(fs, 1e9f, 0, b);
        const ImVec2 c = P(float(d.x), float(d.y) + 4);
        const ImVec2 p(c.x - sz.x * 0.5f, c.y - sz.y * 0.5f);
        dl->AddCallback(xsgmenu::BlendAdd, nullptr);
        for (int g = 3; g >= 1; --g)
            dl->AddCircleFilled(c, sz.y * (0.35f + 0.18f * g), Alpha(glow, 0.07f * t), 24);
        dl->AddCallback(xsgmenu::BlendAlpha, nullptr);
        const float o = std::max(1.5f, fs * 0.075f);
        for (int i = 0; i < 8; ++i) {
            const float a = float(i) * 0.785398f;
            dl->AddText(font, fs, ImVec2(p.x + std::cos(a) * o, p.y + std::sin(a) * o), IM_COL32(10, 8, 24, int(220 * t)), b);
        }
        dl->AddText(font, fs, ImVec2(p.x, p.y + o * 0.6f), Alpha(glow, 0.9f * t), b);
        dl->AddText(font, fs, p, Alpha(core, t), b);
    }
}

/* The command menu (menuBatObjDraw): five rows sliding in from the left, the
   selected one framed in cyan.  The game shows only the selected row's label;
   here every row is labelled, unavailable ones dimmed. */
void CmdDraw(ImDrawList* dl, ImFont* font, float gx, float gy, float gw, float gh) {
    using namespace xsgmenu;
    static const char* const kLabel[] = {"Ether", "Item", "Move", "A.G.W.S.", "A.G.W.S.", "Guard", "Double"};
    LayoutBegin(gx, gy, gw, gh);
    const float k = LayoutK(), kx = LayoutKx();
    auto P = [&](float x, float y) { return Map(x, y, kLeft); };
    const GpuUiBattleCmd& c = g_csnap;
    const double now = AnimNow();
    AnimState& menu = Anim(0xC0DE000000000000ull, 0, 0);
    const float t = FadeIn(menu, 8.0f);
    for (int i = 0; i < 5; ++i) {
        const GpuUiCmdRow& r = c.row[i];
        const bool sel = i == c.sel;
        // rows arrive one after another
        AnimState& ra = Anim(0xC0DE000000000100ull + uint64_t(i), 0, 0);
        if (now - menu.born < i * 0.035) ra.a = 0;
        const float rt = FadeIn(ra, 9.0f) * t;
        const float x = float(r.x), y = float(r.y);
        const ImVec2 a = P(x + 0x14, y + 6), b = P(x + 0x8a, y + 0x1b);
        Panel(dl, a, b, k, rt * (sel ? 1.0f : 0.92f), false);
        if (sel) GlowBar(dl, a, b, k, rt * (r.enabled ? 1.0f : 0.45f), now, r.enabled ? col::kCyan : col::kTextOff);
        const ImVec2 m = P(x + 0xa, y + 0x12);
        const float d = 3.6f * k;
        dl->AddQuadFilled(ImVec2(m.x, m.y - d), ImVec2(m.x + d, m.y), ImVec2(m.x, m.y + d), ImVec2(m.x - d, m.y),
                          Alpha(sel ? col::kCyan : col::kFrameDim, rt));
        const char* tl = r.label >= 0 && r.label < 7 ? kLabel[r.label] : "?";
        const float ts = 13.0f * k;
        const ImVec2 tp = P(x + 0x20 + (sel ? 2 : 0), y + 0x0b);
        const ImU32 tc = !r.enabled ? col::kTextOff : sel ? col::kText : col::kTextDim;
        HaloText(dl, font, ts, tp, tc, tl, rt);
        if (r.label == 3 || r.label == 4) {   // board / leave: an up / down arrow after the name
            const float w = font->CalcTextSizeA(ts, 1e9f, 0, tl).x, h = 6 * k;
            const ImVec2 o(tp.x + w + 5 * k, tp.y + ts * 0.5f);
            const ImU32 ac = Alpha(tc, rt);
            if (r.label == 3) dl->AddTriangleFilled(ImVec2(o.x, o.y + h / 2), ImVec2(o.x + h, o.y + h / 2), ImVec2(o.x + h / 2, o.y - h / 2), ac);
            else dl->AddTriangleFilled(ImVec2(o.x, o.y - h / 2), ImVec2(o.x + h, o.y - h / 2), ImVec2(o.x + h / 2, o.y + h / 2), ac);
        }
    }
    // the cursor glides to the selected row
    if (c.sel >= 0 && c.sel < 5) {
        const GpuUiCmdRow& r = c.row[c.sel];
        const ImVec2 target = P(float(r.x) + 0x2, float(r.y) + 0x12);
        AnimState& cur = Anim(0xC0DE0000000000FFull, target.x, target.y);
        const ImVec2 p = Spring(cur, target.x, target.y, 22.0f);
        Chevron(dl, p, 7 * k, t, now);
    }
}
}  // namespace

void BattleDraw(ImDrawList* dl, ImFont* font, float gx, float gy, float gw, float gh, uint32_t vsync) {
    using namespace xsgmenu;
    LayoutBegin(gx, gy, gw, gh);
    const float k = LayoutK(), kx = LayoutKx();
    int anchor = kCentre;
    auto P = [&](float x, float y) { return Map(x, y, anchor); };
    char b[32];
    DmgDraw(dl, font, gx, gy, gw, gh, vsync);
    for (int ch = 0; ch < XSG_GPU_UI_KG_CHANNELS; ++ch)
        if (ch == 0 && Fresh(vsync, g_ksnap[0].frame)) { LayoutBegin(gx, gy, gw, gh); KeyGuideRows(dl, font, g_ksnap[0]); continue; }
        else if (Fresh(vsync, g_ksnap[ch].frame)) KgDraw(dl, font, gx, gy, gw, gh, g_ksnap[ch], 0x4B47000000000000ull + (uint64_t(ch) << 32), ch == 0);
    if (Fresh(vsync, g_csnap.frame)) CmdDraw(dl, font, gx, gy, gw, gh);
    for (const GpuUiUnit& u : g_bsnap.unit) {
        if (!Fresh(vsync, u.frame)) continue;
        // slides down into place and fades in; the active unit's frame breathes
        AnimState& an = Anim(0xBA770000ull ^ uint32_t(u.key), 0, -14);
        const float t = FadeIn(an, 6.0f);
        const float x = float(u.x), y = float(u.y) + Spring(an, 0, 0, 14.0f).y;
        anchor = AnchorFor(x + 0x10, x + 0xa2);
        // the stats panel to the right of the portrait (portrait: x+16..x+56)
        const ImVec2 a = P(x + 0x3a, y - 5), c = P(x + 0xa2, y + 0x40);
        Panel(dl, a, c, k, t, u.active != 0);
        if (u.active) {
            const float br = 0.5f + 0.5f * float(std::sin(AnimNow() * 3.2));
            dl->AddRect(ImVec2(a.x - 3 * k, a.y - 3 * k), ImVec2(c.x + 3 * k, c.y + 3 * k), Alpha(col::kCyan, 0.25f * br * t), 0, 0, 2.0f * k);
        }
        const float ls = 10.0f * k, vs = 14.0f * k, ss = 10.0f * k;
        auto label = [&](float ly, const char* t) { dl->AddText(font, ls, P(x + 0x40, ly + 1), col::kTextDim, t); };
        auto value = [&](float ly, int v, int vmax, bool warn) {
            std::snprintf(b, sizeof b, "%d", v);
            const ImVec2 w = font->CalcTextSizeA(vs, 1e9f, 0, b);
            const ImVec2 r = P(x + 0x7a, ly - 2);
            dl->AddText(font, vs, ImVec2(r.x - w.x, r.y), warn ? col::kWarn : col::kText, b);
            std::snprintf(b, sizeof b, "/%d", vmax);
            dl->AddText(font, ss, P(x + 0x7c, ly + 1), col::kTextDim, b);
        };
        label(y, "HP");
        value(y, u.hp, u.hp_max, u.idol || (u.hp_max > 0 && u.hp * 4 <= u.hp_max));
        label(y + 0x11, "EP");
        value(y + 0x11, u.ep, u.ep_max, false);
        // AP: six chevrons
        label(y + 0x22, "AP");
        for (int i = 0; i < 6; ++i) {
            const ImVec2 o = P(x + 0x52 + i * 10.5f, y + 0x22);
            const float h = 9 * k, w = 6 * kx;
            const ImU32 cc = i < u.ap ? col::kCyan : IM_COL32(118, 214, 255, 45);
            dl->AddTriangleFilled(o, ImVec2(o.x + w, o.y + h / 2), ImVec2(o.x, o.y + h), cc);
        }
        // BG: the boost count and its gauge
        label(y + 0x30, "BG");
        const ImVec2 g0 = P(x + 0x52, y + 0x33), g1 = P(x + 0x52 + 60, y + 0x37);
        dl->AddRectFilled(g0, g1, IM_COL32(255, 32, 128, 50));
        dl->AddRectFilled(g0, ImVec2(g0.x + (g1.x - g0.x) * std::min(1.0f, std::max(0, u.bp) / 60.0f), g1.y),
                          IM_COL32(255, 60, 150, 230));
        std::snprintf(b, sizeof b, "%d", u.bg);
        dl->AddText(font, vs, P(x + 0x92, y + 0x2d), col::kText, b);
    }
}

}  // namespace xsgui


/* Rebuilt UI: the dialog window.  See dialog.h. */
#include "dialog.h"
#include "gpu_ipc.h"
#include "../menu/cfg.h"
#include "../menu/option_rows.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iconv.h>
#include <set>

namespace xsgui {
namespace {

GpuUiDialog* g_d;
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
    const float x0 = gx + (d.x - 16) * sx, y0 = gy + (d.y + 24) * sy;
    const float x1 = gx + (d.x + d.w + 4) * sx, y1 = gy + std::min(d.y + d.h + 34, 442) * sy;
    // the menu's look (menu/option_rows.h col::, menu.cpp Panel): indigo glass, a doubled
    // periwinkle frame and corner brackets; cyan for the accents
    const float k = sy;   // one PS2 pixel on screen
    dl->AddRectFilledMultiColor(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(26, 25, 66, 228), IM_COL32(26, 25, 66, 228),
                                IM_COL32(14, 13, 40, 238), IM_COL32(14, 13, 40, 238));
    dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(150, 180, 235, 150), 0, 0, 1.0f * k);
    dl->AddRect(ImVec2(x0 + 2 * k, y0 + 2 * k), ImVec2(x1 - 2 * k, y1 - 2 * k), IM_COL32(150, 180, 235, 40), 0, 0, 1.0f);
    DrawBrackets(dl, ImVec2(x0 - 2 * k, y0 - 2 * k), ImVec2(x1 + 2 * k, y1 + 2 * k), col::kFrame, 10 * k, 1.2f * k);
    const float fs = 21.0f * sy;   // body text size
    // speaker: a plate on the frame's top edge, a cyan rule under the name
    if (d.name[0]) {
        const std::string name = Decode(d.name);
        const float ns = 16.0f * k;
        const ImVec2 tsz = font->CalcTextSizeA(ns, 1e9f, 0, name.c_str());
        const float px0 = x0 + 12 * k, px1 = px0 + tsz.x + 20 * k, py1 = y0 + 1 * k, py0 = py1 - ns - 7 * k;
        dl->AddRectFilled(ImVec2(px0, py0), ImVec2(px1, py1), IM_COL32(40, 40, 96, 240));
        dl->AddRect(ImVec2(px0, py0), ImVec2(px1, py1), IM_COL32(150, 180, 235, 150), 0, 0, 1.0f * k);
        dl->AddRectFilled(ImVec2(px0, py1 - 1.5f * k), ImVec2(px0 + 28 * k, py1 + 0.5f * k), col::kCyan);
        dl->AddText(font, ns, ImVec2(px0 + 10 * k, py0 + 3 * k), col::kText, name.c_str());
    }
    // body: the lines typed out so far, with a soft shadow
    float ty = y0 + 12 * sy;
    for (uint32_t i = 0; i < d.nlines && i < XSG_GPU_UI_LINES; ++i) {
        const std::string t = Decode(d.line[i]);
        if (!t.empty()) {
            dl->AddText(font, fs, ImVec2(x0 + 22 * sx + 1.5f * sy, ty + 1.5f * sy), IM_COL32(0, 0, 0, 160), t.c_str());
            dl->AddText(font, fs, ImVec2(x0 + 22 * sx, ty), col::kText, t.c_str());
        }
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

}  // namespace xsgui

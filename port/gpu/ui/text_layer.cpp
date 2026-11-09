/* "Enhanced original" UI: the game's font, upscaled.  See text_layer.h.
 *
 * The game's font code (src/main/xgl_font.c xglFontFlushSub) draws every character as
 * one GIF packet: a PACKED tag with NLOOP 1, PRE, PRIM sprite, NREG 6, registers
 * TEX0_1 RGBAQ UV XYZ2 UV XYZ2, sampling a font page (TBP0 0x3820 or 0x3C00, CLUT at
 * CBP 0x3800/0x3801).  Such a packet is recorded (rectangle in frame pixels through
 * XYOFFSET_1, texels, colour) and emptied in place (its second corner set to its
 * first: a zero-area sprite draws nothing), so the GS frame has no text.  At the vsync
 * after the frame the recorded glyphs are drawn over the picture.
 *
 * The font pages are reloaded between print passes (xglFontReloadTexture 2, then 1:
 * different pages at the same address), so VRAM at vsync time is not what a glyph was
 * drawn from.  This keeps its own copy of the font memory (TBP 0x3800 up), applying the
 * game's uploads to it as they pass, and keys each glyph to the content it was drawn
 * with (the last upload's hash at its texture page and at its CLUT).  New content is
 * decoded once from the copy and upscaled 4x with Scale2x twice on a worker (the
 * original letterforms: one-pixel strokes whole, diagonals smoothed); the plain page
 * shows until then. */
#include "text_layer.h"

#include "../menu/cfg.h"
#include "../menu/menu_render.h"
#include "../native.h"
#include "gs_interface.hpp"
#include "ps2host/gs.h"

#include <algorithm>
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <future>
#include <map>
#include <string>
#include <tuple>
#include <memory>
#include <unordered_map>
#include <vector>

namespace xsgtext {
namespace {
ParallelGS::GSInterface* g_gs;
inline uint32_t Bits(uint64_t v, int lo, int n) { return uint32_t(v >> lo) & uint32_t((1ull << n) - 1); }

struct Glyph {
    uint64_t key;           // the atlas: font page + the content it was drawn with
    uint32_t tw, th;        // the page's size in texels
    float x0, y0, x1, y1;   // frame pixels
    float u0, v0, u1, v1;   // texels
    uint32_t rgba;          // PS2 colour (0x80 = 1.0)
    uint32_t z, seq;        // depth (the font draws with Z test GREATER) and order
    uint16_t sc[4];         // scissor x0 x1 y0 y1 (frame pixels, inclusive)
    uint32_t fbp;
    bool hidden;            // covered by a shape drawn after it (the overlay is on top of everything)
    float vis;              // how much of it still shows through the shapes drawn over it later (1: all)
};
std::vector<Glyph> g_cur, g_show;
// glyphs of this frame by 32 x 32 cell of the frame (shapes check only the cells they touch)
constexpr int kCell = 32, kCw = 24, kCh = 16;
std::vector<uint32_t> g_cells[kCw * kCh];
uint32_t g_stamp;
std::vector<uint32_t> g_mark;   // per glyph: the shape that last checked it
std::map<uint32_t, std::vector<Glyph>> g_byfb;   // frame buffer -> the text last drawn into it
std::vector<uint32_t> g_fbs;                     // buffers drawn into this frame
std::vector<uint32_t> g_shown_fbs;               // buffers the GS has displayed (text drawn elsewhere is
                                                 // rendered to a texture, e.g. the dialog: left to the GS)
bool g_drew;   // GIF data arrived since the last vsync (a frame is being drawn)

struct Path { uint32_t left = 0, flg = 0, nreg = 0, idx = 0; uint64_t regs = 0; } g_path[4];
int32_t g_ofx = 2048 * 16 - 256 * 16, g_ofy = 2048 * 16 - 224 * 16;   // XYOFFSET_1 (12.4)
uint64_t g_test;   // TEST_1
uint64_t g_scissor = 0x01ff000001ff0000ull, g_frame;   // SCISSOR_1, FRAME_1
uint32_t g_prim;           // PRIM: the shape being drawn
float g_vx[3], g_vy[3];    // its last vertices (frame pixels)
uint32_t g_vz[3];
uint32_t g_nv;
uint64_t g_zbuf;   // ZBUF_1 (bit 32: ZMSK)
uint32_t g_alpha = 0x80;   // RGBAQ alpha (0x80 = 1)
uint64_t g_alphareg;       // ALPHA_1
void Kick(uint32_t x16, uint32_t y16, uint32_t z);

/* ---- the font memory: a copy of local memory from TBP 0x3800 on, kept from the uploads */
constexpr uint32_t kFontBase = 0x3800;
std::unique_ptr<ps2host::gs::Memory> g_mem;
struct Xfer {
    bool on = false;
    uint32_t dbp = 0, dbw = 0, dpsm = 0, dx = 0, dy = 0, w = 0, h = 0, x = 0, y = 0;
    uint64_t hash = 0;
} g_x;
uint64_t g_bitblt, g_trxpos, g_trxreg;
std::unordered_map<uint32_t, uint64_t> g_content;   // DBP -> hash of the last upload there
uint64_t g_font_state;   // all of them combined: the font memory's content (one big upload covers several pages)

void SetReg(uint32_t reg, uint64_t v) {
    switch (reg) {
    case 0x47: g_test = v; break;
    case 0x40: g_scissor = v; break;
    case 0x4c: g_frame = v; break;
    case 0x4e: g_zbuf = v; break;
    case 0x42: g_alphareg = v; break;
    case 0x18: g_ofx = int32_t(Bits(v, 0, 16)); g_ofy = int32_t(Bits(v, 32, 16)); break;   // XYOFFSET_1
    case 0x50: g_bitblt = v; break;
    case 0x51: g_trxpos = v; break;
    case 0x52: g_trxreg = v; break;
    case 0x53:   // TRXDIR: a host -> local transfer starts
        if (Bits(v, 0, 2) != 3) {   // into a frame buffer: replaces what was drawn there, glyphs included
            const uint32_t dbp = Bits(g_bitblt, 32, 14);
            const float dx = float(Bits(g_trxpos, 32, 11)), dy = float(Bits(g_trxpos, 48, 11));
            const float w = float(Bits(g_trxreg, 0, 12)), h = float(Bits(g_trxreg, 32, 12));
            int hid = 0;
            for (Glyph& g : g_cur)
                if (!g.hidden && dbp == g.fbp * 32 && g.x0 >= dx - 1 && g.x1 <= dx + w + 1 && g.y0 >= dy - 1 && g.y1 <= dy + h + 1) { g.hidden = true; ++hid; }
        }
        g_x = Xfer();
        if (Bits(v, 0, 2) == 0 && Bits(g_bitblt, 32, 14) >= kFontBase) {
            g_x.on = true;
            g_x.dbp = Bits(g_bitblt, 32, 14);
            g_x.dbw = Bits(g_bitblt, 48, 6);
            g_x.dpsm = Bits(g_bitblt, 56, 6);
            g_x.dx = Bits(g_trxpos, 32, 11);
            g_x.dy = Bits(g_trxpos, 48, 11);
            g_x.w = Bits(g_trxreg, 0, 12);
            g_x.h = Bits(g_trxreg, 32, 12);
            g_x.hash = 0xcbf29ce484222325ull ^ g_bitblt ^ (g_trxpos * 31) ^ (g_trxreg * 131);
        }
        break;
    default: break;
    }
}

void Pixel(uint32_t v) {
    if (g_x.y >= g_x.h) return;
    g_mem->Write(g_x.dbp, g_x.dbw, g_x.dx + g_x.x, g_x.dy + g_x.y, g_x.dpsm, v);
    if (++g_x.x >= g_x.w) {
        g_x.x = 0;
        if (++g_x.y >= g_x.h) {   // complete: this is now the content at DBP
            g_content[g_x.dbp] = g_x.hash;
            g_font_state = 0;
            for (const auto& [dbp, h] : g_content) g_font_state += (h ^ dbp) * 0x9e3779b97f4a7c15ull;
            g_x.on = false;
        }
    }
}

void Image(uint64_t lo, uint64_t hi) {
    g_x.hash = (g_x.hash ^ lo) * 0x100000001b3ull;
    g_x.hash = (g_x.hash ^ hi) * 0x100000001b3ull;
    const uint32_t w[4] = {uint32_t(lo), uint32_t(lo >> 32), uint32_t(hi), uint32_t(hi >> 32)};
    for (uint32_t word : w) {
        switch (g_x.dpsm) {
        case ps2host::gs::PSMCT32: case ps2host::gs::PSMCT24: Pixel(word); break;
        case ps2host::gs::PSMCT16: case ps2host::gs::PSMCT16S: Pixel(word & 0xffff); Pixel(word >> 16); break;
        case ps2host::gs::PSMT8: case ps2host::gs::PSMT8H: for (int k = 0; k < 4; ++k) Pixel((word >> (8 * k)) & 0xff); break;
        default: for (int k = 0; k < 8; ++k) Pixel((word >> (4 * k)) & 15); break;   // 4-bit formats
        }
    }
}

/* ---- atlases: a font page as it was when drawn, upscaled */
struct Atlas {
    ImTextureID id = 0;
    std::future<std::vector<uint32_t>> up;
    uint32_t up_w = 0, up_h = 0;
};
std::unordered_map<uint64_t, Atlas> g_atlas;

/* Scale2x (EPX): each texel becomes 2x2, its corners taking a neighbour's colour where two
   neighbours agree; one-pixel strokes stay whole and their diagonals are smoothed. */
/* Close enough to count as the same colour: the font's soft edge (grey around white) is
   one shape, not speckles. */
bool Eq(uint32_t a, uint32_t b) {
    int d = 0;
    for (int k = 0; k < 32; k += 8) d += std::abs(int((a >> k) & 255) - int((b >> k) & 255));
    return d < 96;
}

std::vector<uint32_t> Scale2x(const std::vector<uint32_t>& s, uint32_t w, uint32_t h) {
    std::vector<uint32_t> d(size_t(w) * h * 4);
    auto at = [&](int x, int y) { return s[size_t(std::clamp(y, 0, int(h) - 1)) * w + size_t(std::clamp(x, 0, int(w) - 1))]; };
    for (int y = 0; y < int(h); ++y)
        for (int x = 0; x < int(w); ++x) {
            const uint32_t P = at(x, y), A = at(x, y - 1), B = at(x + 1, y), C = at(x - 1, y), D = at(x, y + 1);
            uint32_t e0 = P, e1 = P, e2 = P, e3 = P;
            if (Eq(C, A) && !Eq(C, D) && !Eq(A, B)) e0 = A;
            if (Eq(A, B) && !Eq(A, C) && !Eq(B, D)) e1 = B;
            if (Eq(D, C) && !Eq(D, B) && !Eq(C, A)) e2 = C;
            if (Eq(B, D) && !Eq(B, A) && !Eq(D, C)) e3 = D;
            uint32_t* o = d.data() + size_t(y) * 2 * (2 * w) + size_t(x) * 2;
            o[0] = e0; o[1] = e1; o[2 * w] = e2; o[2 * w + 1] = e3;
        }
    return d;
}

/* The atlas for a glyph drawn now with `tex0`: made from the font memory as it is now. */
uint64_t AtlasKey(uint64_t tex0, uint32_t* tw, uint32_t* th) {
    const uint32_t tbp = Bits(tex0, 0, 14), cbp = Bits(tex0, 37, 14);
    const uint64_t page = (tex0 & 0x3ffffffffull) | (tex0 & (0x7ffffull << 37));
    (void)tbp;
    (void)cbp;
    const uint64_t key = (page * 0x9e3779b97f4a7c15ull) ^ g_font_state;
    *tw = 1u << std::min(10u, Bits(tex0, 26, 4));
    *th = 1u << std::min(10u, Bits(tex0, 30, 4));
    if (g_atlas.count(key)) return key;
    uint32_t w = 0, h = 0;
    std::vector<uint32_t> px = xsgnative::DecodeVramTexture(g_mem->data(), tex0, 0, &w, &h);
    Atlas& a = g_atlas[key];
    if (px.empty()) return key;
    if (const char* d = xsgcfg::CStr("debug.text_dump"); d && *d) {   // the decoded page, raw RGBA
        char n[256];
        std::snprintf(n, sizeof n, "%s/font_%016llx_%ux%u.rgba", d, (unsigned long long)key, w, h);
        if (FILE* f = std::fopen(n, "wb")) { std::fwrite(px.data(), 4, px.size(), f); std::fclose(f); }
    }
    for (uint32_t& c : px) c = (c & 0x00ffffffu) | (std::min(255u, (c >> 24) * 2) << 24);   // PS2 alpha 0x80 = 1
    a.id = ImTextureID(xsgmenu::RenderTexture(0, px.data(), w, h));   // plain until the upscale is ready
    a.up_w = w * 4;
    a.up_h = h * 4;
    a.up = std::async(std::launch::async, [px = std::move(px), w, h] { return Scale2x(Scale2x(px, w, h), w * 2, h * 2); });
    if (g_atlas.size() > 64) {   // old contents: drop those not shown
        for (auto it = g_atlas.begin(); it != g_atlas.end();) {
            bool used = it->first == key;
            for (const Glyph& g : g_show) used |= g.key == it->first;
            it = used || it->second.up.valid() ? std::next(it) : g_atlas.erase(it);
        }
    }
    return key;
}

/* A shape drawn after glyphs hides those it covers completely (in the GS it is painted over
   them; here the glyphs are drawn on top of the picture afterwards). */
void Kick(uint32_t x16, uint32_t y16, uint32_t z) {
    const float x = float(int32_t(x16) - g_ofx) / 16.0f, y = float(int32_t(y16) - g_ofy) / 16.0f;
    const uint32_t type = g_prim & 7;
    g_vx[g_nv % 3] = x;
    g_vy[g_nv % 3] = y;
    g_vz[g_nv % 3] = z;
    ++g_nv;
    const uint32_t need = type == 6 ? 2 : (type >= 3 && type <= 5) ? 3 : 0;
    if (!need || g_nv < need) return;
    if (g_cur.empty()) { g_nv = 0; return; }
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (uint32_t k = 0; k < need; ++k) {
        const uint32_t j = (g_nv - 1 - k) % 3;
        x0 = std::min(x0, g_vx[j]); x1 = std::max(x1, g_vx[j]);
        y0 = std::min(y0, g_vy[j]); y1 = std::max(y1, g_vy[j]);
    }
    if (type == 6) g_nv = 0;
    if (x1 - x0 < 1 || y1 - y0 < 1) return;
    // its opacity: blending off paints over; otherwise the blend factor (ALPHA C: the vertex alpha, or FIX)
    float opacity = 1.0f;
    if (g_prim & 0x40) {
        const uint32_t c = Bits(g_alphareg, 4, 2);
        opacity = std::min(1.0f, float(c == 2 ? Bits(g_alphareg, 32, 8) : g_alpha) / 128.0f);
    }
    if (opacity < 0.05f) return;
    const uint32_t fbp = Bits(g_frame, 0, 9);
    if (std::find(g_fbs.begin(), g_fbs.end(), fbp) == g_fbs.end()) g_fbs.push_back(fbp);
    // the GS paints over a glyph only where the shape passes the Z test against the glyph's depth
    const bool zte = Bits(g_test, 16, 1);
    const uint32_t ztst = Bits(g_test, 17, 2);
    if (zte && ztst == 0) return;   // NEVER
    if ((Bits(g_frame, 32, 32) & 0xffffff) == 0xffffff) return;   // FBMSK: no colour written (a Z or alpha pass)
    if (Bits(g_test, 0, 1) && Bits(g_test, 1, 3) == 0 && Bits(g_test, 12, 2) != 1 && Bits(g_test, 12, 2) != 3) return;   // alpha test NEVER, colour kept
    uint32_t zmin = 0xffffffffu;
    for (uint32_t k = 0; k < need; ++k) zmin = std::min(zmin, g_vz[(g_nv - 1 - k) % 3]);
    ++g_stamp;
    const int cx0 = std::clamp(int(x0) / kCell, 0, kCw - 1), cx1 = std::clamp(int(x1) / kCell, 0, kCw - 1);
    const int cy0 = std::clamp(int(y0) / kCell, 0, kCh - 1), cy1 = std::clamp(int(y1) / kCell, 0, kCh - 1);
    for (int cy = cy0; cy <= cy1; ++cy)
    for (int cx = cx0; cx <= cx1; ++cx)
    for (uint32_t gi : g_cells[cy * kCw + cx]) {
        if (g_mark[gi] == g_stamp) continue;
        g_mark[gi] = g_stamp;
        Glyph& g = g_cur[gi];
        // the glyph's middle (its box less a quarter each side: the ink, not the empty margin), covered by
        // this shape or by several (a panel drawn as tiles): hidden once 90% of it is painted over
        const float ix = (g.x1 - g.x0) * 0.25f, iy = (g.y1 - g.y0) * 0.25f;
        if (g.hidden || g.fbp != fbp) continue;
        const float ox = std::min(g.x1 - ix, x1) - std::max(g.x0 + ix, x0), oy = std::min(g.y1 - iy, y1) - std::max(g.y0 + iy, y0);
        if (ox <= 0 || oy <= 0) continue;
        if (zte && ztst == 2 && zmin < g.z) continue;    // GEQUAL fails
        if (zte && ztst == 3 && zmin <= g.z) continue;   // GREATER fails
        const float frac = std::min(1.0f, ox * oy / ((g.x1 - g.x0 - 2 * ix) * (g.y1 - g.y0 - 2 * iy)));
        g.vis *= 1.0f - opacity * frac;
        if (g.vis < 0.35f) {   // a panel painted over it (several translucent layers add up)
            g.hidden = true;
            if (xsgcfg::CStr("debug.text_dump")) {
                static int n;
                if (n++ < 12)
                    std::fprintf(stderr, "[text] hid glyph %.0f,%.0f z %x: prim %x a %x op %.2f rect %.0f,%.0f-%.0f,%.0f z %x test %llx alpha %llx fbmsk %x\n",
                                 g.x0, g.y0, g.z, g_prim, g_alpha, opacity, x0, y0, x1, y1, zmin, (unsigned long long)g_test,
                                 (unsigned long long)g_alphareg, Bits(g_frame, 32, 32));
            }
        }
    }
}

void TakeGlyph(uint8_t* q) {
    uint64_t r[6][2];
    std::memcpy(r, q, sizeof r);
    const uint64_t tex0 = r[0][0];
    if (Bits(tex0, 0, 14) < kFontBase || Bits(tex0, 37, 14) < kFontBase) return;   // not the font
    Glyph g;
    g.rgba = Bits(r[1][0], 0, 8) | Bits(r[1][0], 32, 8) << 8 | Bits(r[1][1], 0, 8) << 16 | Bits(r[1][1], 32, 8) << 24;
    g.u0 = float(Bits(r[2][0], 0, 14)) / 16.0f;
    g.v0 = float(Bits(r[2][0], 32, 14)) / 16.0f;
    g.u1 = float(Bits(r[4][0], 0, 14)) / 16.0f;
    g.v1 = float(Bits(r[4][0], 32, 14)) / 16.0f;
    g.x0 = float(int32_t(Bits(r[3][0], 0, 16)) - g_ofx) / 16.0f;
    g.y0 = float(int32_t(Bits(r[3][0], 32, 16)) - g_ofy) / 16.0f;
    g.x1 = float(int32_t(Bits(r[5][0], 0, 16)) - g_ofx) / 16.0f;
    g.y1 = float(int32_t(Bits(r[5][0], 32, 16)) - g_ofy) / 16.0f;
    if (g.x1 <= g.x0 || g.y1 <= g.y0) return;
    if (std::find(g_shown_fbs.begin(), g_shown_fbs.end(), Bits(g_frame, 0, 9)) == g_shown_fbs.end()) return;
    g.z = uint32_t(r[5][1]);
    g.sc[0] = uint16_t(Bits(g_scissor, 0, 11)); g.sc[1] = uint16_t(Bits(g_scissor, 16, 11));
    g.sc[2] = uint16_t(Bits(g_scissor, 32, 11)); g.sc[3] = uint16_t(Bits(g_scissor, 48, 11));
    g.fbp = Bits(g_frame, 0, 9);
    g.hidden = false;
    g.vis = 1;

    g.seq = uint32_t(g_cur.size());
    g.key = AtlasKey(tex0, &g.tw, &g.th);
    if (std::find(g_fbs.begin(), g_fbs.end(), g.fbp) == g_fbs.end()) g_fbs.push_back(g.fbp);
    {
        const uint32_t gi = uint32_t(g_cur.size());
        const int cx0 = std::clamp(int(g.x0) / kCell, 0, kCw - 1), cx1 = std::clamp(int(g.x1) / kCell, 0, kCw - 1);
        const int cy0 = std::clamp(int(g.y0) / kCell, 0, kCh - 1), cy1 = std::clamp(int(g.y1) / kCell, 0, kCh - 1);
        for (int cy = cy0; cy <= cy1; ++cy)
            for (int cx = cx0; cx <= cx1; ++cx) g_cells[cy * kCw + cx].push_back(gi);
        g_mark.push_back(0);
    }
    g_cur.push_back(g);
    std::memcpy(q + 5 * 16, q + 3 * 16, 8);   // the second corner onto the first: the GS draws nothing
}

bool g_on;
unsigned g_ver = ~0u;
}  // namespace

void Init(ParallelGS::GSInterface* gs) { g_gs = gs; }

bool On() {
    if (g_ver != xsgcfg::Version()) {
        g_ver = xsgcfg::Version();
        g_on = xsgcfg::Get("display.ui") == "enhanced";
        if (!g_on) { g_cur.clear(); g_show.clear(); g_mark.clear(); for (auto& c : g_cells) c.clear(); }
        if (g_on && !g_mem) {   // seeded once from the GS (the CLUT may have been uploaded long ago)
            g_mem = std::make_unique<ps2host::gs::Memory>();
            const uint8_t* v = xsgnative::On() ? xsgnative::MirrorData() : nullptr;
            if (!v && g_gs) { g_gs->flush(); v = static_cast<const uint8_t*>(g_gs->map_vram_read(0, 4u << 20)); }
            if (v) std::memcpy(g_mem->data() + size_t(kFontBase) * 256, v + size_t(kFontBase) * 256, (4u << 20) - size_t(kFontBase) * 256);
        }
    }
    return g_on;
}

void Gif(uint32_t path, uint8_t* data, uint32_t len) {
    Path& p = g_path[path & 3];
    g_drew = true;
    for (uint32_t i = 0; i + 16 <= len; i += 16) {
        uint64_t lo, hi;
        std::memcpy(&lo, data + i, 8);
        std::memcpy(&hi, data + i + 8, 8);
        if (!p.left) {
            const uint32_t nloop = Bits(lo, 0, 15);
            p.flg = Bits(lo, 58, 2);
            p.nreg = Bits(lo, 60, 4);
            if (!p.nreg) p.nreg = 16;
            p.regs = hi;
            p.idx = 0;
            p.left = p.flg == 0 ? nloop * p.nreg : p.flg == 1 ? (nloop * p.nreg + 1) / 2 : nloop;
            if (Bits(lo, 46, 1)) { g_prim = Bits(lo, 47, 11); g_nv = 0; }
            // a font glyph: NLOOP 1, PRE, sprite, PACKED, 6 regs TEX0 RGBAQ UV XYZ2 UV XYZ2
            if (nloop == 1 && Bits(lo, 46, 1) && Bits(lo, 47, 3) == 6 && p.flg == 0 && p.nreg == 6 &&
                (hi & 0xffffff) == 0x535316 && i + 16 * 7 <= len)
                TakeGlyph(data + i + 16);
            continue;
        }
        p.left--;
        if (p.flg >= 2) {   // IMAGE data
            if (g_x.on) Image(lo, hi);
            continue;
        }
        if (p.flg == 0) {
            const uint32_t d = uint32_t(p.regs >> (4 * p.idx)) & 15;
            p.idx = (p.idx + 1) % p.nreg;
            if (d == 0xe) {
                const uint32_t reg = uint32_t(hi & 0xff);
                if (reg == 0x00) { g_prim = Bits(lo, 0, 11); g_nv = 0; }
                else if (reg == 0x01) g_alpha = Bits(lo, 24, 8);
                else if (reg == 0x05 || reg == 0x04) Kick(Bits(lo, 0, 16), Bits(lo, 16, 16), uint32_t(lo >> 32));
                else SetReg(reg, lo);
            } else if (d == 0x1) g_alpha = Bits(hi, 32, 8);
            else if (d == 0x5 || d == 0x4) Kick(Bits(lo, 0, 16), Bits(lo, 32, 16), d == 0x5 ? uint32_t(hi) : Bits(hi, 4, 24));
        } else {   // REGLIST
            for (int k = 0; k < 2; ++k) {
                const uint32_t d = uint32_t(p.regs >> (4 * p.idx)) & 15;
                p.idx = (p.idx + 1) % p.nreg;
                if (d != 0xe && d != 0xf && d >= 0x6) SetReg(d, k ? hi : lo);
            }
        }
    }
}

void Finish();

void Vsync() {
    for (auto& [k, a] : g_atlas)   // finished upscales replace the plain pages
        if (a.up.valid() && a.up.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const std::vector<uint32_t> px = a.up.get();
            a.id = ImTextureID(xsgmenu::RenderTexture(uint64_t(a.id), px.data(), a.up_w, a.up_h));
        }
    if (g_drew) Finish();
    // shown: the text of the buffer the GS displays now
    g_show.clear();
    if (g_gs) {
        const auto& pr = g_gs->get_priv_register_state();
        const uint64_t pmode = pr.qwords_lo[0];
        const uint32_t fb = (pmode & 2) ? Bits(pr.qwords_lo[0x90 >> 3], 0, 9) : Bits(pr.qwords_lo[0x70 >> 3], 0, 9);
        if (std::find(g_shown_fbs.begin(), g_shown_fbs.end(), fb) == g_shown_fbs.end() && g_shown_fbs.size() < 8) g_shown_fbs.push_back(fb);
        auto it = g_byfb.find(fb);
        if (it != g_byfb.end()) g_show = it->second;
    }
}

/* A frame was drawn: settle its glyphs into the buffers they went to. */
void Finish() {
    g_drew = false;
    // the GS keeps a stamp only where it is nearer than what is there (Z test GREATER): painted
    // back to front, and among equal depths the first drawn wins (later ones fail the test).
    // The font's outline is its dark copies stamped behind the glyph this way.
    if (const char* d = xsgcfg::CStr("debug.text_dump"); d && *d) {   // per frame: glyphs by target and scissor
        static unsigned n;
        if (++n % 120 == 0) {
            std::map<std::tuple<uint32_t, int, int, int, int>, std::pair<int, float>> m;
            for (const Glyph& g : g_cur) {
                auto& e = m[{g.fbp, g.sc[0], g.sc[1], g.sc[2], g.sc[3]}];
                e.first++;
                e.second = e.first == 1 ? g.y0 : std::min(e.second, g.y0);
            }
            for (const auto& [k, v] : m)
                std::fprintf(stderr, "[text] frame %u: fbp %x sc %d-%d,%d-%d: %d glyphs from y %.0f\n", n, std::get<0>(k), std::get<1>(k),
                             std::get<2>(k), std::get<3>(k), std::get<4>(k), v.first, v.second);
        }
    }
    std::sort(g_cur.begin(), g_cur.end(), [](const Glyph& a, const Glyph& b) { return a.z != b.z ? a.z < b.z : a.seq > b.seq; });
    // each buffer's text, as last drawn into it (double buffering: drawn into the back buffer,
    // shown once the game flips to it)
    if (xsgcfg::CStr("debug.text_dump")) {
        static unsigned n;
        int hid = 0;
        for (const Glyph& g : g_cur) hid += g.hidden;
        if (++n % 60 == 0 && !g_cur.empty())
            std::fprintf(stderr, "[text] frame: %zu glyphs, %d hidden, first fbp %x, shown fbs %zu\n", g_cur.size(), hid, g_cur[0].fbp, g_shown_fbs.size());
    }
    for (uint32_t fb : g_fbs) g_byfb[fb].clear();
    for (const Glyph& g : g_cur)
        if (!g.hidden) g_byfb[g.fbp].push_back(g);
    g_fbs.clear();
    g_cur.clear();
    g_mark.clear();
    for (auto& c : g_cells) c.clear();
}

bool Active() { return On() && !g_show.empty(); }

void Draw(ImDrawList* dl, float gx, float gy, float gw, float gh) {
    if (xsgcfg::GetBool("debug.text_hide")) return;   // what is left in the GS frame
    const float sx = gw / 512.0f, sy = gh / 448.0f;
    for (const Glyph& g : g_show) {
        if (g.hidden) continue;
        auto it = g_atlas.find(g.key);
        if (it == g_atlas.end() || !it->second.id) continue;
        auto ch = [&](int s) { return std::min(255u, ((g.rgba >> s) & 0xffu) * 2); };
        dl->AddImage(it->second.id, ImVec2(gx + g.x0 * sx, gy + g.y0 * sy), ImVec2(gx + g.x1 * sx, gy + g.y1 * sy),
                     ImVec2(g.u0 / float(g.tw), g.v0 / float(g.th)), ImVec2(g.u1 / float(g.tw), g.v1 / float(g.th)),
                     IM_COL32(ch(0), ch(8), ch(16), ch(24)));
    }
}

}  // namespace xsgtext

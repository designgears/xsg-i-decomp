/* Controller button glyphs: see glyphs.h. */
#include "glyphs.h"
#include "menu/cfg.h"
#include "native.h"
#include "gs_interface.hpp"
#include <map>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <sys/stat.h>

namespace xsgglyph {
namespace {

void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void Log(const char* fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "[glyphs] %s\n", b);
}

uint32_t Bits(uint64_t v, int lo, int n) { return uint32_t((v >> lo) & ((1ull << n) - 1)); }

/* GIF parse state per path, and the transfer registers. */
struct Path {
    uint32_t left = 0, nreg = 0, idx = 0, flg = 0;
    uint64_t regs = 0;
} g_path[4];
uint64_t g_bitbltbuf, g_trxpos, g_trxreg;
uint64_t g_prim, g_tex0[2], g_uv, g_texa;
uint32_t g_nvtx;
struct V { int x, y, u, v; } g_vtx[2];
ParallelGS::GSInterface* g_gs;
std::map<std::string, int> g_icons;   // catalogued sprite keys (log mode)
int g_icon_n;

/* The host -> local transfer being received. */
struct Xfer {
    bool on = false;
    uint32_t dbp, dbw, psm, x, y, w, h;
    uint32_t bytes;   // expected
    std::vector<uint8_t> data;
} g_x;

int g_family = 0;
bool g_log;
std::string g_dir;
std::set<uint64_t> g_seen;

uint32_t BitsPerPixel(uint32_t psm) {
    switch (psm) {
    case 0x00: return 32;
    case 0x01: return 24;
    case 0x02: case 0x0a: return 16;
    case 0x13: return 8;
    case 0x14: return 4;
    case 0x1b: return 8;    // T8H
    case 0x24: case 0x2c: return 4;   // T4HL / T4HH
    default: return 32;
    }
}

uint64_t Fnv(const uint8_t* p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

/* A completed upload: catalogue it (log mode). */
void Done(std::vector<uint8_t>& inject) {
    (void)inject;
    Xfer& x = g_x;
    x.on = false;
    if (!g_log || true || x.w > 256 || x.h > 256 || x.w * x.h < 16) return;   // (upload dumps off: icons are catalogued by draw)
    const uint64_t h = Fnv(x.data.data(), x.data.size()) ^ (uint64_t(x.psm) << 56) ^ (uint64_t(x.w) << 40) ^ (uint64_t(x.h) << 28);
    if (!g_seen.insert(h).second) return;
    // grey / RGBA view
    std::vector<uint8_t> img(size_t(x.w) * x.h * 4);
    const uint32_t bpp = BitsPerPixel(x.psm);
    for (uint32_t i = 0; i < x.w * x.h; ++i) {
        uint8_t* o = &img[size_t(i) * 4];
        const uint8_t* d = x.data.data();
        if (bpp == 32) { std::memcpy(o, d + i * 4, 4); o[3] = 255; }
        else if (bpp == 24) { std::memcpy(o, d + i * 3, 3); o[3] = 255; }
        else if (bpp == 16) {
            const uint32_t c = d[i * 2] | d[i * 2 + 1] << 8;
            o[0] = uint8_t((c & 31) << 3); o[1] = uint8_t(((c >> 5) & 31) << 3); o[2] = uint8_t(((c >> 10) & 31) << 3); o[3] = 255;
        } else if (bpp == 8) { o[0] = o[1] = o[2] = d[i]; o[3] = 255; }
        else { const uint8_t v = (d[i >> 1] >> ((i & 1) * 4)) & 15; o[0] = o[1] = o[2] = uint8_t(v * 17); o[3] = 255; }
    }
    char name[512];
    std::snprintf(name, sizeof name, "%s/up_%016llx_psm%02x_%ux%u_bp%04x.png", g_dir.c_str(), (unsigned long long)h, x.psm,
                  x.w, x.h, x.dbp);
    stbi_write_png(name, int(x.w), int(x.h), 4, img.data(), int(x.w * 4));
    Log("upload %016llx psm %#x %ux%u -> bp %#x bw %u (%u,%u)", (unsigned long long)h, x.psm, x.w, x.h, x.dbp, x.dbw, x.x, x.y);
}

/* A small textured sprite: an icon candidate (log mode: decode its rectangle). */
void Sprite() {
    const uint64_t tex0 = g_tex0[Bits(g_prim, 9, 1)];
    const V& a = g_vtx[0];
    const V& b = g_vtx[1];
    const int u0 = std::min(a.u, b.u) >> 4, u1 = (std::max(a.u, b.u) + 15) >> 4;
    const int v0 = std::min(a.v, b.v) >> 4, v1 = (std::max(a.v, b.v) + 15) >> 4;
    const int sw = std::abs(a.x - b.x) >> 4, sh = std::abs(a.y - b.y) >> 4;
    if (u1 - u0 < 6 || v1 - v0 < 6 || u1 - u0 > 64 || v1 - v0 > 64 || sw > 96 || sh > 96) return;
    char key[160];
    std::snprintf(key, sizeof key, "%04x_%02x_%04x_%d_%d_%d_%d", Bits(tex0, 0, 14), Bits(tex0, 20, 6), Bits(tex0, 37, 14), u0, v0,
                  u1 - u0, v1 - v0);
    if (g_icons.count(key)) return;
    const int id = g_icon_n++;
    g_icons[key] = id;
    if (!g_gs) return;
    g_gs->flush();
    const uint8_t* vram = static_cast<const uint8_t*>(g_gs->map_vram_read(0, 4u << 20));
    uint32_t tw, th;
    std::vector<uint32_t> px = xsgnative::DecodeVramTexture(vram, tex0, g_texa, &tw, &th);
    {   // the whole sheet, once per texture + palette
        static std::set<std::string> sheets;
        char sk[64];
        std::snprintf(sk, sizeof sk, "%04x_%04x", Bits(tex0, 0, 14), Bits(tex0, 37, 14));
        if (sheets.insert(sk).second) {
            std::vector<uint8_t> all(size_t(tw) * th * 4);
            for (size_t i = 0; i < px.size(); ++i) {
                all[i * 4] = uint8_t(px[i]); all[i * 4 + 1] = uint8_t(px[i] >> 8); all[i * 4 + 2] = uint8_t(px[i] >> 16);
                all[i * 4 + 3] = uint8_t(std::min(255u, (px[i] >> 24) * 2));
            }
            char sn[512];
            std::snprintf(sn, sizeof sn, "%s/sheet_%s_%04d.png", g_dir.c_str(), sk, id);
            stbi_write_png(sn, int(tw), int(th), 4, all.data(), int(tw * 4));
        }
    }
    const int w = u1 - u0, h = v1 - v0;
    std::vector<uint8_t> img(size_t(w) * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const uint32_t c = px[size_t((v0 + y) % th) * tw + size_t((u0 + x) % tw)];
            uint8_t* o = &img[(size_t(y) * w + x) * 4];
            o[0] = uint8_t(c); o[1] = uint8_t(c >> 8); o[2] = uint8_t(c >> 16);
            o[3] = uint8_t(std::min(255u, (c >> 24) * 2));
        }
    char name[512];
    std::snprintf(name, sizeof name, "%s/spr_%04d_%s.png", g_dir.c_str(), id, key);
    stbi_write_png(name, w, h, 4, img.data(), w * 4);
    Log("sprite %d %s tex0 %#llx screen %dx%d at (%d,%d)", id, key, (unsigned long long)tex0, sw, sh, std::min(a.x, b.x) >> 4,
        std::min(a.y, b.y) >> 4);
}

void Vertex(uint64_t xyz) {
    if (!g_log) return;
    const uint32_t type = Bits(g_prim, 0, 3);
    if (type != 6 || !Bits(g_prim, 4, 1) || !Bits(g_prim, 8, 1)) { g_nvtx = 0; return; }   // TME FST sprites
    V& v = g_vtx[g_nvtx & 1];
    v.x = int(Bits(xyz, 0, 16)) - 0;
    v.y = int(Bits(xyz, 16, 16));
    v.u = int(Bits(g_uv, 0, 14));
    v.v = int(Bits(g_uv, 16, 14));
    if (++g_nvtx == 2) { g_nvtx = 0; Sprite(); }
}

void SetReg(uint32_t a, uint64_t v) {
    switch (a) {
    case 0x00: g_prim = v; g_nvtx = 0; break;
    case 0x03: g_uv = v; break;
    case 0x04: case 0x05: case 0x0c: case 0x0d: Vertex(v); break;
    case 0x06: g_tex0[0] = v; break;
    case 0x07: g_tex0[1] = v; break;
    case 0x3b: g_texa = v; break;
    case 0x50: g_bitbltbuf = v; break;
    case 0x51: g_trxpos = v; break;
    case 0x52: g_trxreg = v; break;
    case 0x53:
        if ((v & 3) == 0) {   // host -> local
            Xfer& x = g_x;
            x.on = true;
            x.dbp = Bits(g_bitbltbuf, 32, 14);
            x.dbw = Bits(g_bitbltbuf, 48, 6);
            x.psm = Bits(g_bitbltbuf, 56, 6);
            x.x = Bits(g_trxpos, 32, 11);
            x.y = Bits(g_trxpos, 48, 11);
            x.w = Bits(g_trxreg, 0, 12);
            x.h = Bits(g_trxreg, 32, 12);
            x.bytes = (x.w * x.h * BitsPerPixel(x.psm) + 7) / 8;
            x.data.clear();
        }
        break;
    default: break;
    }
}

}  // namespace

void Init() {
    g_log = xsgcfg::GetBool("debug.glyph_log");
    const char* d = xsgcfg::CStr("debug.glyph_dir");
    g_dir = d && *d ? d : "/tmp/xsg_glyphs";
    if (g_log) {
        ::mkdir(g_dir.c_str(), 0755);
        Log("logging small uploads to %s", g_dir.c_str());
    }
}

void SetController(int family) { g_family = family; }

void SetGs(ParallelGS::GSInterface* gs) { g_gs = gs; }

void Feed(uint32_t path, const uint8_t* data, uint32_t len, std::vector<uint8_t>& inject) {
    Path& p = g_path[path & 3];
    for (uint32_t i = 0; i + 16 <= len; i += 16) {
        uint64_t lo, hi;
        std::memcpy(&lo, data + i, 8);
        std::memcpy(&hi, data + i + 8, 8);
        if (!p.left) {
            const uint32_t nloop = Bits(lo, 0, 15);
            if (Bits(lo, 46, 1) && Bits(lo, 58, 2) == 0) SetReg(0x00, Bits(lo, 47, 11));   // PRE
            p.flg = Bits(lo, 58, 2);
            p.nreg = Bits(lo, 60, 4);
            if (!p.nreg) p.nreg = 16;
            p.regs = hi;
            p.idx = 0;
            p.left = p.flg == 0 ? nloop * p.nreg : p.flg == 1 ? (nloop * p.nreg + 1) / 2 : nloop;
            continue;
        }
        p.left--;
        if (p.flg >= 2) {   // IMAGE
            if (g_x.on) {
                const uint32_t take = std::min<uint32_t>(16, g_x.bytes - uint32_t(g_x.data.size()));
                g_x.data.insert(g_x.data.end(), data + i, data + i + take);
                if (g_x.data.size() >= g_x.bytes) Done(inject);
            }
            continue;
        }
        if (p.flg == 0) {
            const uint32_t d = uint32_t(p.regs >> (4 * p.idx)) & 15;
            p.idx = (p.idx + 1) % p.nreg;
            switch (d) {
            case 0x0: SetReg(0x00, lo & 0x7ff); break;
            case 0x3: SetReg(0x03, (lo & 0x3fff) | (Bits(lo, 32, 14) << 16)); break;
            case 0x4: case 0x5: Vertex(Bits(lo, 0, 16) | uint64_t(Bits(lo, 32, 16)) << 16); break;
            case 0xe: SetReg(uint32_t(hi & 0xff), lo); break;
            default: break;
            }
        } else {   // REGLIST: two raw register writes per qword
            for (int k = 0; k < 2; ++k) {
                const uint32_t d = uint32_t(p.regs >> (4 * p.idx)) & 15;
                p.idx = (p.idx + 1) % p.nreg;
                const uint64_t v = k ? hi : lo;
                if (d == 0x5 || d == 0x4 || d == 0xc || d == 0xd) Vertex(v);
                else if (d != 0xe && d != 0xf) SetReg(d, v);
            }
        }
    }
}

}  // namespace xsgglyph

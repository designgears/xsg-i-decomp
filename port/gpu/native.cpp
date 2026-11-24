/*
 * xsg-gpu native renderer: set-0 strip batches (VU1 loop 0x0c18) drawn by our
 * own Vulkan pipeline at XSG_GPU_NATIVE_SCALE and composited live over the
 * paraLLEl-GS frame.  See native.h and port/docs/NATIVE_RENDERER.md.
 */
#include "native.h"
#include "ui/pointer.h"
#include "menu/cfg.h"
#include "native_skin.h"
#include "native_fx.h"
#include "native_prof.h"
#include "gs_addr.h"

#include "gpu_ipc.h"
#include "device.hpp"
#include "gs_interface.hpp"
#include "ps2host/gs.h"
#include "ui/art.h"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <array>
#include <deque>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <tuple>
#include <set>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>
#include <vector>

namespace xsgnative {
namespace {

const uint32_t kVert[] = {
#include "native.vert.inc"
};
const uint32_t kFrag[] = {
#include "native.frag.inc"
};
const uint32_t kComp[] = {
#include "composite.comp.inc"
};

void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void Log(const char* fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "[native] %s\n", b);
}

inline uint32_t Bits(uint64_t v, int lo, int n) { return uint32_t(v >> lo) & ((1u << n) - 1); }

/* ---------------------------------------------------------------- GS state from the GIF stream */
enum : uint32_t {
    R_PRIM = 0x00, R_TEX0_1 = 0x06, R_CLAMP_1 = 0x08, R_TEX1_1 = 0x14, R_XYOFFSET_1 = 0x18, R_PRMODECONT = 0x1a,
    R_PRMODE = 0x1b, R_TEXA = 0x3b, R_FOGCOL = 0x3d, R_SCISSOR_1 = 0x40, R_ALPHA_1 = 0x42, R_TEST_1 = 0x47,
    R_FRAME_1 = 0x4c, R_ZBUF_1 = 0x4e,
};

uint64_t g_reg[256];

/* Our own copy of GS local memory, fed by the host->local IMAGE transfers in
   the GIF stream (BITBLTBUF/TRXPOS/TRXREG/TRXDIR), with the software GS's
   swizzle.  Textures are decoded from it when a batch arrives, so a texture
   slot the game re-uploads for every material still decodes right.  It does
   not see local->local copies or render-to-texture. */
ps2host::gs::Memory g_mem;
uint32_t g_upload_gen;
/* Per 8 KB GS page: the upload generation that last wrote it (conservative
   page ranges), so a texture is re-hashed only when its pages changed and
   re-decoded only when their content changed (characters re-upload the same
   texture slots for every material). */
uint32_t g_page_gen[512];
/* Pages [first, first + count) that a (base block, buffer width, PSM, rows) area may touch. */
void PageRange(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t rows, uint32_t* first, uint32_t* count) {
    using namespace ps2host::gs;
    uint32_t pw = 64, ph = 32;
    if (psm == PSMCT16 || psm == PSMCT16S || psm == 0x32 || psm == 0x3a) ph = 64;
    else if (psm == PSMT8) { pw = 128; ph = 64; }
    else if (psm == PSMT4) { pw = 128; ph = 128; }
    const uint32_t ppr = std::max(1u, bw * 64 / pw);
    *first = bp / 32;
    *count = std::min(512u, ((rows + ph - 1) / ph + 1) * ppr + 1);
}
struct Xfer {
    bool on = false;
    uint32_t dbp, dbw, psm, x0, y0, w, h, x, y;
    uint64_t pend = 0;   // leftover bits (24-bit pixels straddle bytes)
    int pend_bits = 0;
    /* The current row's pixels not yet in the mirror: one WriteRun per row (its SSE
       paths need eight pixels or more), not one per qword.  XferFlush before anything
       reads the mirror. */
    uint32_t row[2048];
    uint32_t rown = 0, rx = 0, ry = 0;
} g_xfer;

void XferFlush() {
    Xfer& t = g_xfer;
    if (!t.rown) return;
    const uint32_t x = (t.x0 + t.rx) & 2047, y = (t.y0 + t.ry) & 2047;
    if (x + t.rown <= 2048) g_mem.WriteRun(t.dbp, t.dbw, x, y, t.psm, t.row, t.rown);
    else
        for (uint32_t k = 0; k < t.rown; ++k) g_mem.Write(t.dbp, t.dbw, (x + k) & 2047, y, t.psm, t.row[k]);
    t.rown = 0;
}

void XferQword(const uint8_t* q) {
    Xfer& t = g_xfer;
    if (!t.on) return;
    using namespace ps2host::gs;
    const int bpp = t.psm == PSMCT32 || t.psm == PSMZ32 ? 32 : t.psm == PSMCT24 || t.psm == PSMZ24 ? 24
                  : (t.psm == PSMT8 || t.psm == PSMT8H) ? 8 : (t.psm == PSMT4 || t.psm == PSMT4HL || t.psm == PSMT4HH) ? 4 : 16;
    // Unpack the qword's pixels, then store them a row run at a time (WriteRun).
    uint32_t px[32];
    int n = 0;
    if (bpp == 32) {
        std::memcpy(px, q, 16);
        n = 4;
    } else if (bpp == 16) {
        for (int i = 0; i < 8; ++i) px[i] = uint32_t(q[2 * i]) | uint32_t(q[2 * i + 1]) << 8;
        n = 8;
    } else if (bpp == 8) {
        for (int i = 0; i < 16; ++i) px[i] = q[i];
        n = 16;
    } else if (bpp == 4) {
        for (int i = 0; i < 16; ++i) { px[2 * i] = q[i] & 15; px[2 * i + 1] = q[i] >> 4; }
        n = 32;
    } else {   // 24 bits: pixels straddle qwords
        for (int i = 0; i < 16; ++i) {
            t.pend |= uint64_t(q[i]) << t.pend_bits;
            t.pend_bits += 8;
            if (t.pend_bits >= 24) {
                px[n++] = uint32_t(t.pend & 0xffffff);
                t.pend >>= 24;
                t.pend_bits -= 24;
            }
        }
    }
    for (int i = 0; i < n && t.on;) {
        const uint32_t run = std::min<uint32_t>(uint32_t(n - i), t.w - t.x);
        if (!t.rown) { t.rx = t.x; t.ry = t.y; }
        if (t.rown + run > 2048) { XferFlush(); t.rx = t.x; t.ry = t.y; }
        std::memcpy(t.row + t.rown, px + i, run * 4);
        t.rown += run;
        i += int(run);
        t.x += run;
        if (t.x >= t.w) { XferFlush(); t.x = 0; if (++t.y >= t.h) t.on = false; }
    }
}

struct PathParse {
    uint32_t left = 0;      // qwords (PACKED/REGLIST) or IMAGE qwords still to come
    uint32_t flg = 0, nreg = 0, idx = 0;
    uint64_t regs = 0;
    bool skip = false;      // the tag writes no register tracked here (vertex data): its qwords are skipped
};
/* A PACKED / REGLIST tag reaches a register this tracker reads: PRIM (0), TEX0 (6, 7),
   CLAMP (8, 9); PACKED also A+D (0xe).  Others only set vertex registers nothing reads. */
bool TagTracked(uint32_t flg, uint32_t nreg, uint64_t regs) {
    for (uint32_t k = 0; k < nreg; ++k) {
        const uint32_t d = uint32_t(regs >> (4 * k)) & 15;
        if (d == 0 || (d >= 6 && d <= 9) || (flg == 0 && d == 0xe)) return true;
    }
    return false;
}
PathParse g_path[4];

/* The GS's CLUT buffer: 512 halfword slots (a 32-bit entry: low half at slot b, high at
   b + 256), loaded from local memory when TEX0 / TEX2 is written with CLD asking for it, as the
   GS does.  Indexed textures read their colours from here, not from memory at decode time (the
   game reloads a palette's memory while textures loaded from it are still drawn). */
struct ClutBuf {
    uint16_t slot[512] = {};
    uint32_t cbp0 = ~0u, cbp1 = ~0u;
    uint64_t hash = 1;   // of slot[], for the texture caches
} g_cbuf;
void ClutLoad(uint64_t tex0) {
    using namespace ps2host::gs;
    const uint32_t psm = Bits(tex0, 20, 6);
    const bool eight = psm == PSMT8 || psm == PSMT8H, four = psm == PSMT4 || psm == PSMT4HL || psm == PSMT4HH;
    if (!eight && !four) return;
    const uint32_t cld = Bits(tex0, 61, 3), cbp = Bits(tex0, 37, 14);
    switch (cld) {
    case 0: return;
    case 1: break;
    case 2: g_cbuf.cbp0 = cbp; break;
    case 3: g_cbuf.cbp1 = cbp; break;
    case 4: if (cbp == g_cbuf.cbp0) return; g_cbuf.cbp0 = cbp; break;
    case 5: if (cbp == g_cbuf.cbp1) return; g_cbuf.cbp1 = cbp; break;
    default: return;
    }
    const uint32_t cpsm = Bits(tex0, 51, 4), csa = Bits(tex0, 56, 5);
    const uint32_t fmt = cpsm == 0 ? PSMCT32 : cpsm;
    const bool c32 = fmt == PSMCT32 || fmt == PSMCT24;
    const uint32_t count = eight ? 256 : 16;
    const uint32_t base = eight ? 0 : (c32 ? (csa & 15) : csa) * 16;
    /* The palette's entries as read from the mirror, cached while its pages
       saw no upload: most loads re-read an unchanged palette, and the swizzled
       reads were ~6% of xsg-gpu's thread in battle effects. */
    struct ClutRaw { uint64_t key = ~0ull; uint32_t gen = 0; uint32_t raw[256]; };
    static ClutRaw cache[64];
    const uint64_t key = uint64_t(cbp) | uint64_t(fmt) << 14 | uint64_t(eight) << 20;
    const uint32_t cp = cbp / 32;
    const uint32_t gen = std::max(g_page_gen[cp & 511], g_page_gen[(cp + 1) & 511]);
    /* The same palette into the same slots as the load before: nothing changes. */
    static uint64_t last_key = ~0ull;
    static uint32_t last_gen, last_base;
    if (key == last_key && gen == last_gen && base == last_base && !g_xfer.on) return;
    last_key = key;
    last_gen = gen;
    last_base = base;
    ClutRaw& c = cache[(key * 0x9E3779B97F4A7C15ull) >> 58];
    if (c.key != key || c.gen != gen || g_xfer.on) {   // (an upload in progress: its pages are still changing)
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t x, y, nib = 0;
            if (eight) { const uint32_t p = (i & 0xE7) | ((i & 0x08) << 1) | ((i & 0x10) >> 1); x = p % 16; y = p / 16; }
            else { x = i % 8; y = i / 8; }
            const uint32_t ad = Memory::Address(cbp, 1, x, y, fmt, &nib) & 0x3fffff;
            std::memcpy(&c.raw[i], g_mem.data() + ad, 4);
        }
        c.key = key;
        c.gen = gen;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t raw = c.raw[i];
        const uint32_t b = (base + i) & 511;
        if (c32) {
            g_cbuf.slot[b & 255] = uint16_t(raw);
            g_cbuf.slot[(b & 255) + 256] = uint16_t(raw >> 16);
        } else {
            g_cbuf.slot[b] = uint16_t(raw);
        }
    }
    uint64_t h = 1469598103934665603ull;
    for (int i = 0; i < 512; ++i) h = (h ^ g_cbuf.slot[i]) * 1099511628211ull;
    g_cbuf.hash = h | 1;
}

void SetReg(uint32_t a, uint64_t v) {
    a &= 0xff;
    if (a == 0x06 || a == 0x07) {   // TEX0: the CLUT load it asks for
        g_reg[a] = v;
        ClutLoad(v);
        return;
    }
    if (a == 0x16 || a == 0x17) {   // TEX2: PSM and the CLUT fields of TEX0, then its load
        const uint64_t m = (0x3full << 20) | (~0ull << 37);
        g_reg[a - 0x10] = (g_reg[a - 0x10] & ~m) | (v & m);
        g_reg[a] = v;
        ClutLoad(g_reg[a - 0x10]);
        return;
    }
    if (a == 0x3f && (v >> 32) == 0x5853474Du) {   // rebuilt-UI marker (end_print.c xsgMark, a TEXFLUSH)
        if (v & 0x200) xsgui::ArtSnapshotTex(0x5B000000u + uint32_t(v & 0xff), g_reg[0x06], g_mem.data());
        else xsgui::ArtSnapshot(uint32_t(v & 0xff));
        return;
    }
    g_reg[a] = v;
    if (a == 0x53) {   // TRXDIR: a host->local transfer starts
        Xfer& t = g_xfer;
        const uint64_t bb = g_reg[0x50], pos = g_reg[0x51], reg = g_reg[0x52];
        t.dbp = Bits(bb, 32, 14);
        t.dbw = Bits(bb, 48, 6);
        t.psm = Bits(bb, 56, 6);
        t.x0 = Bits(pos, 32, 11);
        t.y0 = Bits(pos, 48, 11);
        t.w = Bits(reg, 0, 12);
        t.h = Bits(reg, 32, 12);
        t.x = t.y = 0;
        t.pend = 0;
        t.pend_bits = 0;
        t.on = (v & 3) == 0 && t.w && t.h;
        if (t.on) {
            g_upload_gen++;
            uint32_t p0, np;
            PageRange(t.dbp, t.dbw, t.psm, t.y0 + t.h, &p0, &np);
            for (uint32_t i = 0; i < np; ++i) g_page_gen[(p0 + i) & 511] = g_upload_gen;
        }
    }
}

void GifQword(PathParse& p, const uint8_t* q) {
    uint64_t lo, hi;
    std::memcpy(&lo, q, 8);
    std::memcpy(&hi, q + 8, 8);
    if (!p.left) {
        const uint32_t nloop = Bits(lo, 0, 15), pre = Bits(lo, 46, 1);
        p.flg = Bits(lo, 58, 2);
        p.nreg = Bits(lo, 60, 4);
        if (!p.nreg) p.nreg = 16;
        p.regs = hi;
        p.idx = 0;
        if (pre && p.flg == 0) SetReg(R_PRIM, Bits(lo, 47, 11));
        if (p.flg == 0) p.left = nloop * p.nreg;
        else if (p.flg == 1) p.left = (nloop * p.nreg + 1) / 2;
        else p.left = nloop;
        p.skip = p.flg < 2 && !TagTracked(p.flg, p.nreg, p.regs);
        return;
    }
    p.left--;
    if (p.flg >= 2) {   // IMAGE
        XferQword(q);
        return;
    }
    XferFlush();   // register writes may read the mirror (CLUT loads)
    if (p.skip) return;
    auto one = [&](uint64_t v, uint64_t hiw, bool packed) {
        const uint32_t d = uint32_t(p.regs >> (4 * p.idx)) & 15;
        p.idx = (p.idx + 1) % p.nreg;
        if (packed) {
            if (d == 0xe) SetReg(uint32_t(hiw & 0xff), v);
            else if (d == 0x0) SetReg(R_PRIM, v & 0x7ff);
            else if (d >= 0x6 && d <= 0x9) SetReg(d, v);
        } else if (d != 0xe && d != 0xf) {
            SetReg(d, v);
        }
    };
    if (p.flg == 0) one(lo, hi, true);
    else { one(lo, 0, false); one(hi, 0, false); }
}

/* ---------------------------------------------------------------- batches */
struct Tex {
    uint32_t w = 1, h = 1;
    std::vector<uint32_t> px{0xffffffffu};
};
/* The scene camera (GPU_NATIVE_CAMERA) the batches arriving after it were built with;
   inv: its inverse (row vectors: clip = p * vp, so world = clip * inv). */
struct Camera { float vp[4][4], inv[4][4], scale[4], offset[4]; bool valid; };
std::shared_ptr<const Camera> g_cam;   // the current one (null: none yet)

bool Invert4(const float m[4][4], float out[4][4]) {
    double a[4][8];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 8; ++c) a[r][c] = c < 4 ? m[r][c] : (c - 4 == r ? 1.0 : 0.0);
    for (int c = 0; c < 4; ++c) {
        int piv = c;
        for (int r = c + 1; r < 4; ++r) if (std::fabs(a[r][c]) > std::fabs(a[piv][c])) piv = r;
        if (std::fabs(a[piv][c]) < 1e-20) return false;
        if (piv != c) for (int k = 0; k < 8; ++k) std::swap(a[c][k], a[piv][k]);
        const double d = a[c][c];
        for (int k = 0; k < 8; ++k) a[c][k] /= d;
        for (int r = 0; r < 4; ++r)
            if (r != c && a[r][c] != 0.0) {
                const double f = a[r][c];
                for (int k = 0; k < 8; ++k) a[r][k] -= f * a[c][k];
            }
    }
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) out[r][c] = float(a[r][c + 4]);
    return true;
}
void Mul4(const float a[4][4], const float b[4][4], float o[4][4]) {   // row vectors: p * (a * b)
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) o[r][c] = a[r][0] * b[0][c] + a[r][1] * b[1][c] + a[r][2] * b[2][c] + a[r][3] * b[3][c];
}

struct Stored {
    std::shared_ptr<Tex> tex;
    GpuNativeBatch b;
    std::vector<GpuNativeVertex> v;
    std::shared_ptr<const xsgskin::Draw> skin;   // set 2 (native_skin.cpp); v unused
    std::shared_ptr<const Camera> cam;           // the scene camera it was built with (shadows), or null
    int pass = 0;                                // set 2: 0 tag 1011, 1 specular strip (1012), 2 env strip (1010)
    float mprev[16];                             // set 0: last frame's M962 (v[i].pad = last frame's position)
    bool hasprev = false;
    int obj = 0;                                 // the object id (actor + 1) of the batch (GPU_NATIVE_OBJECT), 0 none
    bool vram_tex = false;                       // its texture is GS-rendered: decoded from the GS memory at Live
    std::shared_ptr<const std::array<uint16_t, 512>> clut;   // vram_tex, indexed: the CLUT buffer when it was drawn
    uint64_t prim, tex0, clamp, tex1, xyoff, texa, fogcol, scissor, alpha, test, frame, zbuf;
};
/* The batches of the last frame drawn into each frame buffer (FRAME.FBP).  A
   frame's DMA kick is synchronous, so all its batches arrive within one vsync
   interval: the first batch for a buffer in a new interval starts it over. */
struct FbFrame {
    std::vector<Stored> b;
    uint32_t vsync = ~0u;
};
std::map<uint32_t, FbFrame> g_fb;

/* ---------------------------------------------------------------- Vulkan */
Vulkan::Device* g_dev;
Vulkan::Program* g_prog;
Vulkan::Program* g_comp;
bool g_on, g_live = true;
int g_scale = 2;
int g_thr = 12;
std::set<uint32_t> g_shots;
std::string g_dir = "/tmp/xsg_native";
uint64_t g_nbatch, g_nvert, g_ntex, g_live_frames;
uint32_t g_last_vsync, g_vsyncs;
Vulkan::ImageHandle g_live_img;     // the composite of the displayed buffer, scale g_scale
uint32_t g_live_w, g_live_h;        // its frame-buffer size (pixels at 1x)
uint32_t g_live_mx;                 // widescreen: its side bands (1x pixels each side; the image is w + 2 mx wide)
Vulkan::ImageHandle g_hold_img;     // the last composite without an unreplayable post effect
uint32_t g_hold_w, g_hold_h, g_hold_mx;
/* Without the live path, batches are kept (and textured) only in the few vsyncs before a shot. */
bool Near() {
    if (g_live) return true;
    auto it = g_shots.upper_bound(g_last_vsync);
    return it != g_shots.end() && *it <= g_last_vsync + 3;
}

struct Ubo {
    float m[4][4];
    float scale[4], offset[4], cscale[4], uv[4], fog[4];
    float target[4], zscale[4], fogcol[4];
    int32_t mode[4];
    float fix[4];
    int32_t wrap[4];
    int32_t region[4];
    float mprev[4][4];
    float mv[4];
    float obj[4];
};

uint32_t ReadU32(const uint8_t* vram, uint32_t a) {
    uint32_t v;
    std::memcpy(&v, vram + (a & 0x3fffff), 4);
    return v;
}

/* A TEX0 texture as RGBA8 (alpha = the GS byte), decoded from local memory. */
std::vector<uint32_t> DecodeTexture(const uint8_t* vram, uint64_t tex0, uint64_t texa, uint32_t* w, uint32_t* h,
                                    const uint16_t* cbuf = nullptr) {
    using namespace ps2host::gs;
    const uint32_t tbp = Bits(tex0, 0, 14), tbw = std::max(1u, Bits(tex0, 14, 6)), psm = Bits(tex0, 20, 6);
    const uint32_t tw = std::min(10u, Bits(tex0, 26, 4)), th = std::min(10u, Bits(tex0, 30, 4));
    const uint32_t cbp = Bits(tex0, 37, 14), cpsm = Bits(tex0, 51, 4);
    const bool aem = Bits(texa, 15, 1);
    const uint32_t ta0 = Bits(texa, 0, 8), ta1 = Bits(texa, 32, 8);
    *w = 1u << tw;
    *h = 1u << th;
    auto c32 = [&](uint32_t c) { return c; };
    auto c24 = [&](uint32_t c) { c &= 0xffffff; return c | ((aem && !c) ? 0u : ta0 << 24); };
    auto c16 = [&](uint32_t c) {
        uint32_t rgb = ((c & 31) << 3) | (((c >> 5) & 31) << 11) | (((c >> 10) & 31) << 19);
        uint32_t a = (c & 0x8000) ? ta1 : ((aem && !(c & 0x7fff)) ? 0u : ta0);
        return rgb | a << 24;
    };
    // CLUT (CSM1): entries at their swizzled spots in the CLUT's buffer.
    uint32_t clut[256] = {};
    const bool eight = psm == PSMT8 || psm == PSMT8H, four = psm == PSMT4 || psm == PSMT4HL || psm == PSMT4HH;
    if ((eight || four) && cbuf) {   // the CLUT buffer as loaded (ClutLoad), CSA for 4-bit
        const uint32_t fmt = cpsm == 0 ? PSMCT32 : cpsm;
        const bool w32 = fmt == PSMCT32 || fmt == PSMCT24;
        const uint32_t csa = Bits(tex0, 56, 5);
        const uint32_t count = eight ? 256 : 16, base = eight ? 0 : (w32 ? (csa & 15) : csa) * 16;
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t b = (base + i) & 511;
            const uint32_t raw = w32 ? uint32_t(cbuf[b & 255]) | uint32_t(cbuf[(b & 255) + 256]) << 16 : cbuf[b];
            clut[i] = fmt == PSMCT32 ? raw : fmt == PSMCT24 ? c24(raw) : c16(raw & 0xffff);
        }
    } else if (eight || four) {
        const uint32_t fmt = cpsm == 0 ? PSMCT32 : cpsm;
        const uint32_t count = eight ? 256 : 16;
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t x, y, nib = 0;
            if (eight) { uint32_t p = (i & 0xE7) | ((i & 0x08) << 1) | ((i & 0x10) >> 1); x = p % 16; y = p / 16; }
            else { x = i % 8; y = i / 8; }
            const uint32_t a = Memory::Address(cbp, 1, x, y, fmt, &nib);
            const uint32_t raw = ReadU32(vram, a);
            clut[i] = fmt == PSMCT32 ? raw : fmt == PSMCT24 ? c24(raw) : c16(raw & 0xffff);
        }
    }
    std::vector<uint32_t> out(size_t(*w) * *h);
    for (uint32_t y = 0; y < *h; ++y)
        for (uint32_t x = 0; x < *w; ++x) {
            uint32_t nib = 0;
            const uint32_t a = Memory::Address(tbp, tbw, x, y, psm, &nib) & 0x3fffff;
            const uint8_t* p = vram + a;
            uint32_t c;
            switch (psm) {
            case PSMCT32: c = c32(ReadU32(vram, a)); break;
            case PSMCT24: c = c24(ReadU32(vram, a)); break;
            case PSMCT16: case PSMCT16S: c = c16(uint32_t(p[0]) | uint32_t(p[1]) << 8); break;
            case PSMT8: c = clut[p[0]]; break;
            case PSMT4: c = clut[nib ? p[0] >> 4 : p[0] & 15]; break;
            case PSMT8H: c = clut[p[3]]; break;
            case PSMT4HL: c = clut[p[3] & 15]; break;
            case PSMT4HH: c = clut[p[3] >> 4]; break;
            default: c = 0xff00ff80u; break;
            }
            out[size_t(y) * *w + x] = c;
        }
    return out;
}

struct Target {
    uint32_t w, h;                  // pixels at this scale
    std::vector<uint32_t> color;    // RGBA8
    std::vector<float> depth;       // 0 = nothing drawn
};
struct GpuTarget {
    uint32_t w, h;
    Vulkan::ImageHandle color, depth, mv;   // SHADER_READ_ONLY_OPTIMAL after RenderGpu (mv: RG16F motion vectors)
    Vulkan::ImageHandle id;                 // R16 UNORM object ids (the mouse), SHADER_READ_ONLY_OPTIMAL
};
int g_obj;   // the object id of the batches arriving now (GPU_NATIVE_OBJECT)

struct Region {
    uint32_t fbp, fbw, fpsm, zbp, zpsm, w, h;
    float ofx, ofy, zscale;
    uint32_t mx = 0;   // widescreen: native-only columns on each side of the GS frame (1x pixels)
    uint32_t W() const { return w + 2 * mx; }   // the native target's width (1x pixels)
};

/* [display] widescreen: the margin that widens a 512-wide frame to 684 PS2 pixels (16:9 at the
   4:3 display's pixel aspect; the game's culling is widened by the same 684/512,
   runtime/settings.c xsg_widescreen_cull).  0 for anything but a full-width frame scissor
   (split views, other resolutions keep 4:3). */
uint32_t WideMargin(const Stored& f, const Region& rg) {
    static unsigned ver = ~0u;
    static bool wide = false;
    if (ver != xsgcfg::Version()) { ver = xsgcfg::Version(); wide = xsgcfg::GetBool("display.widescreen"); }
    if (!wide) return 0;
    if (rg.w != 512 || Bits(f.scissor, 0, 11) != 0) return 0;
    return 86;
}

Region MakeRegion(const Stored& f) {
    Region rg;
    rg.fbp = Bits(f.frame, 0, 9);
    rg.fbw = std::max(1u, Bits(f.frame, 16, 6));
    rg.fpsm = Bits(f.frame, 24, 6);
    rg.zbp = Bits(f.zbuf, 0, 9);
    rg.zpsm = Bits(f.zbuf, 24, 4);
    rg.w = std::min(Bits(f.scissor, 16, 11) + 1, rg.fbw * 64);
    rg.h = Bits(f.scissor, 48, 11) + 1;
    rg.ofx = float(Bits(f.xyoff, 0, 16)) / 16.0f;
    rg.ofy = float(Bits(f.xyoff, 32, 16)) / 16.0f;
    const uint32_t zpsm = 0x30 | rg.zpsm;
    rg.zscale = zpsm == 0x30 ? 1.0f / 4294967296.0f : (zpsm == 0x31 ? 1.0f / 16777216.0f : 1.0f / 65536.0f);
    return rg;
}

/* Textures as GPU images, kept across frames (keyed by the decoded Tex, which
   is itself cached per TEX0/TEXA/upload generation). */
std::unordered_map<const Tex*, std::pair<std::shared_ptr<Tex>, Vulkan::ImageHandle>> g_teximg;

/* Record the batches drawn into `rg` at `scale`; colour and depth end up
   readable by shaders and transfers (SHADER_READ_ONLY_OPTIMAL). */
/* Shadow-map mode for RenderGpu: every batch's object -> clip matrix becomes
   object -> world (the scene camera's inverse) -> light (lvp, orthographic, w = 1),
   into an S x S target whose depth grows toward the light; blended, colour-masked,
   reflected (mirror) and secondary character passes do not cast. */
struct ShadowMode { bool on = false; float lvp[4][4]; } g_shadow_mode;
Vulkan::Program* g_shadow;
const uint32_t kShadow[] = {
#include "shadow.comp.inc"
};
const float kShadowZo = 8388608.0f, kShadowZs = 8388608.0f * 0.98f;
Vulkan::Program* g_outline;
const uint32_t kOutline[] = {
#include "outline.comp.inc"
};
/* The mouse's picking: the object id under the pointer, read back a few frames later. */
struct PickRead { Vulkan::BufferHandle buf; Vulkan::Fence fence; };
std::vector<PickRead> g_picks;
std::vector<Vulkan::BufferHandle> g_pick_pool;   // 4-byte read-back buffers, reused

/* The GS blend (A - B) * C + D (A/B/D: 0 Cs, 1 Cd, 2 zero; C the source alpha or FIX,
   which the fragment stage outputs as its alpha) as a Vulkan blend, src * Cs op dst * Cd.
   False for the forms a fixed-function blend cannot express (Cs * (1 + a) - Cd * a ...). */
bool BlendOf(uint32_t A, uint32_t B, uint32_t D, VkBlendFactor* sf, VkBlendFactor* df, VkBlendOp* op) {
    constexpr VkBlendFactor Z = VK_BLEND_FACTOR_ZERO, O = VK_BLEND_FACTOR_ONE, SA = VK_BLEND_FACTOR_SRC_ALPHA,
                            ISA = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    constexpr VkBlendOp ADD = VK_BLEND_OP_ADD, SUB = VK_BLEND_OP_SUBTRACT, RSUB = VK_BLEND_OP_REVERSE_SUBTRACT;
    auto set = [&](VkBlendFactor s, VkBlendFactor d, VkBlendOp o) { *sf = s; *df = d; *op = o; return true; };
    if (A == 3 || B == 3 || D == 3) return false;
    if (A == B) return D == 0 ? set(O, Z, ADD) : D == 1 ? set(Z, O, ADD) : set(Z, Z, ADD);     // D
    if (A == 0 && B == 1) return D == 1 ? set(SA, ISA, ADD) : D == 2 ? set(SA, SA, SUB) : false;   // (Cs - Cd) a + D
    if (A == 0 && B == 2) return D == 1 ? set(SA, O, ADD) : D == 2 ? set(SA, Z, ADD) : false;      // Cs a + D
    if (A == 1 && B == 0)                                                                          // (Cd - Cs) a + D
        return D == 0 ? set(ISA, SA, ADD) : D == 2 ? set(SA, SA, RSUB)
                      : set(SA, O, RSUB);   // + Cd: approximated as Cd - Cs a (as before)
    if (A == 1 && B == 2) return D == 0 ? set(O, SA, ADD) : D == 2 ? set(Z, SA, ADD) : false;      // Cd a + D
    if (A == 2 && B == 0) return D == 1 ? set(SA, O, RSUB) : D == 0 ? set(ISA, Z, ADD) : set(Z, Z, ADD);   // D - Cs a
    if (A == 2 && B == 1) return D == 1 ? set(Z, ISA, ADD) : D == 0 ? set(O, SA, SUB) : set(Z, Z, ADD);    // D - Cd a
    return false;
}

GpuTarget RenderGpu(Vulkan::CommandBuffer& cmdr, const std::vector<const Stored*>& list, const Region& rg, int scale,
                    unsigned* odd_blend_out) {
    GpuTarget t;
    t.w = rg.W() * scale;
    t.h = rg.h * scale;
    auto& dev = *g_dev;
    auto* cmd = &cmdr;
    auto ci = Vulkan::ImageCreateInfo::render_target(t.w, t.h, VK_FORMAT_R8G8B8A8_UNORM);
    ci.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ci.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    t.color = dev.create_image(ci);
    auto di = Vulkan::ImageCreateInfo::render_target(t.w, t.h, VK_FORMAT_D32_SFLOAT);
    di.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    di.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    t.depth = dev.create_image(di);
    auto mi = Vulkan::ImageCreateInfo::render_target(t.w, t.h, VK_FORMAT_R16G16_SFLOAT);
    mi.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    mi.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    t.mv = dev.create_image(mi);
    auto ii = Vulkan::ImageCreateInfo::render_target(t.w, t.h, VK_FORMAT_R16_UNORM);
    ii.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    t.id = dev.create_image(ii);
    cmd->image_barrier(*t.id, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    cmd->image_barrier(*t.mv, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    auto& color = t.color;
    auto& depth = t.depth;
    // The draws' uniform data in one buffer of this pass (bound per draw at an offset): a Granite
    // allocate_constant_data per draw took ~9 % of the main thread at 8x (its handle's atomic
    // refcount waits on the write-combined UBO stores before it).
    const VkDeviceSize ualign = std::max<VkDeviceSize>(16, dev.get_gpu_properties().limits.minUniformBufferOffsetAlignment);
    const VkDeviceSize ustride = (sizeof(Ubo) + ualign - 1) & ~(ualign - 1);
    Vulkan::BufferCreateInfo ubi = {};
    ubi.domain = Vulkan::BufferDomain::LinkedDeviceHostPreferDevice;
    ubi.size = std::max<size_t>(1, list.size()) * ustride;
    ubi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    const Vulkan::BufferHandle ubuf = dev.create_buffer(ubi);
    uint8_t* const umap = static_cast<uint8_t*>(dev.map_host_buffer(*ubuf, Vulkan::MEMORY_ACCESS_WRITE_BIT));
    VkDeviceSize uoff = 0;

    cmd->image_barrier(*color, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    cmd->image_barrier(*depth, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                       VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    Vulkan::RenderPassInfo rp;
    rp.num_color_attachments = 3;
    rp.color_attachments[0] = &color->get_view();
    rp.color_attachments[1] = &t.mv->get_view();
    rp.color_attachments[2] = &t.id->get_view();
    rp.depth_stencil = &depth->get_view();
    rp.clear_attachments = 7;
    rp.store_attachments = 7;
    rp.op_flags = Vulkan::RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT | Vulkan::RENDER_PASS_OP_STORE_DEPTH_STENCIL_BIT;
    rp.clear_depth_stencil.depth = 0.0f;
    cmd->begin_render_pass(rp);
    cmd->set_opaque_state();
    cmd->set_program(g_prog);
    cmd->set_cull_mode(VK_CULL_MODE_NONE);
    cmd->set_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    unsigned odd_blend = 0;
    xsgskin::Begin();
    const bool shadow = g_shadow_mode.on;
    const float sh_scale[4] = {float(rg.w) * 0.5f, float(rg.h) * 0.5f, kShadowZs, 0.0f};
    const float sh_offset[4] = {float(rg.w) * 0.5f, float(rg.h) * 0.5f, kShadowZo, 0.0f};
    for (const Stored* s : list) {
        uint32_t n = uint32_t(s->v.size());
        float sh_m[4][4];
        if (shadow) {
            // casters: the characters (set 2) only; the scenery's own shading is baked into
            // its textures, and a room's ceiling would otherwise shadow everything under it
            if (!s->skin || !s->cam || Bits(s->prim, 6, 1) || Bits(s->frame, 32, 32) == 0xffffffffu) continue;
            if (s->skin && (s->pass != 0 || s->skin->b.mirror_on)) continue;
            float w[4][4];
            Mul4(s->skin ? s->skin->b.m : s->b.m, s->cam->inv, w);
            Mul4(w, g_shadow_mode.lvp, sh_m);
        }
        if (s->skin) {   // set 2: native_skin.vert, indexed triangle list
            n = shadow ? xsgskin::BindShadow(*cmd, *s->skin, sh_m, sh_scale, sh_offset) : xsgskin::Bind(*cmd, *s->skin, s->pass);
            if (!n) continue;
        } else {
            cmd->set_program(g_prog);
            cmd->set_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
            auto* vp = static_cast<GpuNativeVertex*>(cmd->allocate_vertex_data(0, n * sizeof(GpuNativeVertex), sizeof(GpuNativeVertex)));
            std::memcpy(vp, s->v.data(), n * sizeof(GpuNativeVertex));
            cmd->set_vertex_attrib(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuNativeVertex, p));
            cmd->set_vertex_attrib(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuNativeVertex, c));
            cmd->set_vertex_attrib(2, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuNativeVertex, t));
            cmd->set_vertex_attrib(3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GpuNativeVertex, pad));
        }
        Ubo uu;   // built on the stack, copied to the (write-combined) constant buffer in one go
        Ubo* u = &uu;
        std::memset(u, 0, sizeof *u);
        std::memcpy(u->m, shadow ? &sh_m[0][0] : &s->b.m[0][0], sizeof u->m);
        std::memcpy(u->scale, shadow ? sh_scale : s->b.scale, 16);
        std::memcpy(u->offset, shadow ? sh_offset : s->b.offset, 16);
        std::memcpy(u->cscale, s->b.cscale, 16);
        std::memcpy(u->uv, s->b.uv, 16);
        std::memcpy(u->fog, s->b.fog, 16);
        u->target[0] = rg.ofx - float(rg.mx);
        u->target[1] = rg.ofy;
        u->target[2] = 1.0f / float(rg.W());
        u->target[3] = 1.0f / float(rg.h);
        u->zscale[0] = rg.zscale;
        u->fogcol[0] = float(Bits(s->fogcol, 0, 8));
        u->fogcol[1] = float(Bits(s->fogcol, 8, 8));
        u->fogcol[2] = float(Bits(s->fogcol, 16, 8));
        if (s->hasprev) std::memcpy(u->mprev, s->mprev, sizeof u->mprev);
        u->mv[0] = s->b.scale[0] * float(scale);
        u->mv[1] = s->b.scale[1] * float(scale);
        u->mv[2] = s->hasprev ? 1.0f : 0.0f;
        u->mv[3] = float(s->pass);
        u->obj[0] = float(s->obj) / 65535.0f;
        const uint64_t prim = s->prim;
        const bool tme = Bits(prim, 4, 1), fge = Bits(prim, 5, 1), abe = Bits(prim, 6, 1);
        u->mode[0] = int(Bits(s->tex0, 35, 2) | Bits(s->tex0, 34, 1) << 2 | uint32_t(tme) << 3 | uint32_t(fge) << 4);
        u->mode[1] = int(Bits(s->test, 0, 1) | Bits(s->test, 1, 3) << 1 | Bits(s->test, 12, 2) << 4);
        u->mode[2] = int(Bits(s->test, 4, 8));
        // ALPHA: (A - B) * C >> 7 + D; A/B/D 0 Cs 1 Cd 2 zero, C 0 As 1 Ad 2 FIX.
        const uint32_t A = Bits(s->alpha, 0, 2), B = Bits(s->alpha, 2, 2), C = Bits(s->alpha, 4, 2), D = Bits(s->alpha, 6, 2);
        u->mode[3] = int(C);
        u->fix[0] = float(Bits(s->alpha, 32, 8));
        cmd->set_blend_enable(false);
        bool blended = false;
        if (abe) {
            VkBlendFactor sf, df;
            VkBlendOp op;
            if (C != 1 && BlendOf(A, B, D, &sf, &df, &op)) {
                cmd->set_blend_enable(true);
                blended = true;
                cmd->set_blend_factors(sf, df);
                cmd->set_blend_op(op);
            } else {
                odd_blend++;
            }
        }
        // Depth: ZTE/ZTST from TEST, ZMSK from ZBUF.
        const bool zte = Bits(s->test, 16, 1);
        const uint32_t ztst = Bits(s->test, 17, 2);
        const bool zmsk = Bits(s->zbuf, 32, 1);
        cmd->set_depth_test(true, shadow || !zmsk);
        cmd->set_depth_compare(shadow ? VK_COMPARE_OP_GREATER_OR_EQUAL
                               : !zte || ztst == 1 ? VK_COMPARE_OP_ALWAYS : ztst == 0 ? VK_COMPARE_OP_NEVER
                               : ztst == 2 ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_GREATER);
        if (shadow) cmd->set_blend_enable(false);
        // Motion vectors: written by opaque draws that write colour (translucent layers keep what is under them).
        const bool cw = Bits(s->frame, 32, 32) != 0xffffffffu;
        // object ids (the mouse's picking and outlines): with the motion vectors, opaque draws only
        cmd->set_color_write_mask((cw ? 0xf : 0) | (cw && !blended ? 0x30 | 0x100 : 0));
        // Texture.
        auto it = g_teximg.find(s->tex.get());
        if (it == g_teximg.end()) {
            auto ti = Vulkan::ImageCreateInfo::immutable_2d_image(s->tex->w, s->tex->h, VK_FORMAT_R8G8B8A8_UNORM);
            Vulkan::ImageInitialData init = {s->tex->px.data(), 0, 0};
            it = g_teximg.emplace(s->tex.get(), std::make_pair(s->tex, dev.create_image(ti, &init))).first;
        }
        // Addressing and filtering are done in the shader, the GS way (CLAMP, TEX1.MMAG).
        u->wrap[0] = int(Bits(s->clamp, 0, 2));
        u->wrap[1] = int(Bits(s->clamp, 2, 2));
        u->wrap[2] = int(Bits(s->tex1, 5, 1));
        u->region[0] = int(Bits(s->clamp, 4, 10));
        u->region[1] = int(Bits(s->clamp, 14, 10));
        u->region[2] = int(Bits(s->clamp, 24, 10));
        u->region[3] = int(Bits(s->clamp, 34, 10));
        cmd->set_texture(0, 1, it->second.second->get_view(), Vulkan::StockSampler::NearestClamp);
        // GS scissor, in this target's pixels.
        const int sx0 = int(Bits(s->scissor, 0, 11)), sx1 = int(Bits(s->scissor, 16, 11));
        const int sy0 = int(Bits(s->scissor, 32, 11)), sy1 = int(Bits(s->scissor, 48, 11));
        VkRect2D sc;
        sc.offset = {(sx0 + int(rg.mx)) * scale, sy0 * scale};
        sc.extent = {uint32_t(std::max(0, std::min(sx1 + 1, int(rg.w)) - sx0) * scale),
                     uint32_t(std::max(0, std::min(sy1 + 1, int(rg.h)) - sy0) * scale)};
        // widescreen: the full-width frame scissor opens onto the side bands too
        if (rg.mx && sx0 == 0 && sx1 + 1 >= int(rg.w)) {
            sc.offset.x = 0;
            sc.extent.width = rg.W() * scale;
        }
        if (shadow) sc = {{0, 0}, {rg.w, rg.h}};
        cmd->set_scissor(sc);
        std::memcpy(umap + uoff, &uu, sizeof uu);
        cmd->set_uniform_buffer(0, 0, *ubuf, uoff, sizeof(Ubo));
        uoff += ustride;
        if (s->skin) cmd->draw_indexed(n);
        else cmd->draw(n);
    }
    dev.unmap_host_buffer(*ubuf, Vulkan::MEMORY_ACCESS_WRITE_BIT);
    cmd->end_render_pass();
    cmd->image_barrier(*t.id, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                       VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);
    cmd->image_barrier(*t.mv, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                       VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);
    cmd->image_barrier(*color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                       VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);
    cmd->image_barrier(*depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                       VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);
    if (odd_blend_out) *odd_blend_out = odd_blend;
    return t;
}

/* Copy an RGBA8 / D32 image (SHADER_READ_ONLY_OPTIMAL) to the host; waits. */
template <typename T>
std::vector<T> Readback(Vulkan::Image& img, VkImageAspectFlags aspect, VkImageLayout layout) {
    auto& dev = *g_dev;
    const uint32_t w = img.get_width(), h = img.get_height();
    Vulkan::BufferCreateInfo bi = {};
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.domain = Vulkan::BufferDomain::CachedHost;
    bi.size = VkDeviceSize(w) * h * 4;
    auto buf = dev.create_buffer(bi);
    auto cmd = dev.request_command_buffer();
    const VkImageLayout xfer = img.get_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    cmd->image_barrier(img, layout, xfer, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    cmd->copy_image_to_buffer(*buf, img, 0, {}, {w, h, 1}, 0, 0, {aspect, 0, 0, 1});
    cmd->image_barrier(img, xfer, layout, VK_PIPELINE_STAGE_2_COPY_BIT, 0,
                       VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
    cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                 VK_ACCESS_HOST_READ_BIT);
    Vulkan::Fence fence;
    dev.submit(cmd, &fence);
    fence->wait();
    std::vector<T> out(size_t(w) * h);
    std::memcpy(out.data(), dev.map_host_buffer(*buf, Vulkan::MEMORY_ACCESS_READ_BIT), out.size() * 4);
    return out;
}

Target Render(const std::vector<const Stored*>& list, const Region& rg, int scale) {
    auto cmd = g_dev->request_command_buffer();
    unsigned odd = 0;
    GpuTarget g = RenderGpu(*cmd, list, rg, scale, &odd);
    g_dev->submit(cmd);
    Target t;
    t.w = g.w;
    t.h = g.h;
    t.color = Readback<uint32_t>(*g.color, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    t.depth = Readback<float>(*g.depth, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Log("  %ux: %zu batches drawn (%u with an unsupported blend drawn opaque)", unsigned(scale), list.size(), odd);
    return t;
}

void WritePng(const std::string& path, uint32_t w, uint32_t h, const std::vector<uint32_t>& rgba) {
    std::vector<uint8_t> rgb(size_t(w) * h * 3);
    for (size_t i = 0; i < size_t(w) * h; ++i) {
        rgb[i * 3] = uint8_t(rgba[i]);
        rgb[i * 3 + 1] = uint8_t(rgba[i] >> 8);
        rgb[i * 3 + 2] = uint8_t(rgba[i] >> 16);
    }
    stbi_write_png(path.c_str(), int(w), int(h), 3, rgb.data(), int(w * 3));
}

uint32_t GsPixel(const uint8_t* vram, uint32_t fbp, uint32_t fbw, uint32_t psm, uint32_t x, uint32_t y) {
    const uint32_t a = xsggs::Addr(fbp, fbw, x, y, psm);
    const uint8_t* p = vram + a;
    if (psm == 0x02 || psm == 0x0A) {
        const uint32_t c = uint32_t(p[0]) | uint32_t(p[1]) << 8;
        return ((c & 31) << 3) | (((c >> 5) & 31) << 11) | (((c >> 10) & 31) << 19) | ((c & 0x8000) ? 0x80000000u : 0);
    }
    const uint32_t c = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    return psm == 0x01 ? (c & 0xffffff) | 0x80000000u : c;   // alpha kept: the effect replay reads it (Ad, DATE)
}

uint32_t GsZ(const uint8_t* vram, uint32_t zbp, uint32_t fbw, uint32_t zpsm, uint32_t x, uint32_t y) {
    const uint32_t psm = 0x30 | (zpsm & 0xf);
    const uint32_t a = xsggs::Addr(zbp, fbw, x, y, psm);
    const uint8_t* p = vram + a;
    if (psm == 0x32 || psm == 0x3a) return uint32_t(p[0]) | uint32_t(p[1]) << 8;
    uint32_t v = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16;
    return psm == 0x30 ? v | uint32_t(p[3]) << 24 : v;
}

int ChanDiff(uint32_t a, uint32_t b) {
    int m = 0;
    for (int k = 0; k < 24; k += 8) m = std::max(m, std::abs(int((a >> k) & 255) - int((b >> k) & 255)));
    return m;
}

/* The frame buffer on display (DISPFB of the enabled read circuit). */
uint32_t DisplayFbp(ParallelGS::GSInterface& gs) {
    const auto& priv = gs.get_priv_register_state();
    auto P = [&](uint32_t off) { return priv.qwords_lo[(off >> 4) * 2]; };
    const uint64_t pmode = P(0x00);
    const int circ = (pmode & 1) ? 1 : (pmode & 2) ? 2 : 1;
    uint64_t dispfb = P(circ == 1 ? 0x70 : 0x90), display = P(circ == 1 ? 0x80 : 0xA0);
    if (circ == 1 && dispfb == 0 && display == 0) dispfb = P(0x90);
    return uint32_t(dispfb & 0x1ff);
}

/* The native batches of the displayed buffer (empty when it has none). */
std::vector<const Stored*> DisplayList(ParallelGS::GSInterface& gs, uint32_t* fbp_out) {
    std::vector<const Stored*> list;
    const uint32_t fbp = DisplayFbp(gs);
    *fbp_out = fbp;
    auto it = g_fb.find(fbp);
    // A frame more than a few vsyncs old is stale (the game stopped drawing 3D into it).
    if (it == g_fb.end() || it->second.b.empty() || g_vsyncs - it->second.vsync > 4) return list;
    for (auto& s : it->second.b) list.push_back(&s);
    return list;
}

xsgfx::Surface Surf(const Region& rg) { return {rg.fbp, rg.fbw, rg.fpsm, rg.zbp, rg.zpsm, rg.w, rg.h}; }

/* The GS colour (RGBA8 with the GS alpha) and Z of the region, from a VRAM copy. */
void DecodeGs(const uint8_t* vram, const Region& rg, std::vector<uint32_t>& out) {
    const size_t n = size_t(rg.w) * rg.h;
    out.resize(2 * n);
    uint32_t* o = out.data();
    ParallelRows(rg.h, [&](uint32_t y0, uint32_t y1) {
        const uint32_t zpsm = 0x30 | (rg.zpsm & 0xf);
        for (uint32_t y = y0; y < y1; ++y) {
            const xsggs::Row fr(rg.fbp, rg.fbw, y, rg.fpsm), zr(rg.zbp, rg.fbw, y, zpsm);
            for (uint32_t x = 0; x < rg.w; ++x) {
                o[size_t(y) * rg.w + x] = xsggs::Pixel(vram + fr(x), rg.fpsm);
                const uint8_t* p = vram + zr(x);
                uint32_t z = uint32_t(p[0]) | uint32_t(p[1]) << 8;
                if (zpsm != 0x32 && zpsm != 0x3a) {
                    z |= uint32_t(p[2]) << 16;
                    if (zpsm == 0x30) z |= uint32_t(p[3]) << 24;
                }
                o[n + size_t(y) * rg.w + x] = z;
            }
        }
    });
}

/* GPU timestamps of the live passes, read once Granite has resolved them. */
std::deque<std::array<Vulkan::QueryPoolHandle, 4>> g_ts;
void ProfGpu() {
    const double tick = double(g_dev->get_gpu_properties().limits.timestampPeriod) * 1e-6;
    while (!g_ts.empty() && g_ts.front()[3]->is_signalled()) {
        auto& t = g_ts.front();
        double ms[3];
        for (int k = 0; k < 3; ++k) {
            ms[k] = double(t[k + 1]->get_timestamp_ticks() - t[k]->get_timestamp_ticks()) * tick;
            xsgprof::g_gpu_ms[k] += ms[k];
        }
        if (ms[0] + ms[1] + ms[2] > 8.0)
            Log("[prof] vsync %u: slow live pass on the GPU: Sx %.1f, 1x %.1f, post %.1f ms", g_last_vsync, ms[0], ms[1], ms[2]);
        xsgprof::g_gpu_n++;
        g_ts.pop_front();
    }
    while (g_ts.size() > 64) g_ts.pop_front();
}

/* Shadows: the key light (the brightest of a character's three lights, taken from
   object to world space), an orthographic shadow map fitted around the characters,
   rendered from the same batches (RenderGpu in shadow mode), then shadow.comp. */
void ShadowPass(Vulkan::CommandBuffer& cmd, const std::vector<const Stored*>& list, const Region& rg,
                const Vulkan::Image& depth, const Vulkan::Image& out, const Vulkan::Image& czimg, int strength) {
    float L[3] = {0, 0, 0};
    bool have_light = false;
    std::vector<std::array<float, 3>> pos;
    std::shared_ptr<const Camera> cam;
    for (const Stored* s : list) {
        if (!s->skin || !s->cam || s->pass != 0 || s->skin->b.mirror_on) continue;
        const GpuNativeSkin& b = s->skin->b;
        float w[4][4];
        Mul4(b.m, s->cam->inv, w);
        pos.push_back({w[3][0], w[3][1], w[3][2]});
        cam = s->cam;
        if (have_light) continue;
        int best = -1;
        float bl = 0;
        for (int k = 0; k < 3; ++k) {
            const float l = b.lcol[k][0] + b.lcol[k][1] + b.lcol[k][2];
            const float n2 = b.lmat[0][k] * b.lmat[0][k] + b.lmat[1][k] * b.lmat[1][k] + b.lmat[2][k] * b.lmat[2][k];
            if (l > bl && n2 > 1e-8f) { bl = l; best = k; }
        }
        if (best < 0) continue;
        const float lo[3] = {b.lmat[0][best], b.lmat[1][best], b.lmat[2][best]};
        for (int c = 0; c < 3; ++c) L[c] = lo[0] * w[0][c] + lo[1] * w[1][c] + lo[2] * w[2][c];
        const float n = std::sqrt(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]);
        if (n > 1e-6f && std::isfinite(n)) {
            for (float& v : L) v /= n;
            have_light = true;
        }
    }
    if (!have_light || pos.empty() || !cam) return;
    double cx = 0, cy = 0, cz = 0;
    for (auto& q : pos) { cx += q[0]; cy += q[1]; cz += q[2]; }
    const int nc = int(pos.size());
    const float c[3] = {float(cx / nc), float(cy / nc), float(cz / nc)};
    float upsign = 1.0f;
    // One light for the whole scene.  Every character carries its own light set (the
    // player party and the enemies differ), so taking any one batch's light made the
    // shadow direction depend on which character came first.  The shadow light is the
    // world's vertical (the game's key lights are overhead) leaned by shadow_angle toward
    // a fixed world azimuth (shadow_azimuth): a sun that does not move between camera
    // cuts or characters.
    {
        upsign = L[1] >= 0 ? 1.0f : -1.0f;
        const float up = upsign;
        const double ang = std::max(0, std::min(85, xsgcfg::GetInt("renderer.shadow_angle"))) * 3.14159265 / 180.0;
        const double az = xsgcfg::GetInt("renderer.shadow_azimuth") * 3.14159265 / 180.0;
        L[0] = float(std::sin(ang) * std::cos(az));
        L[1] = float(up * std::cos(ang));
        L[2] = float(std::sin(ang) * std::sin(az));
    }
    float R = 0;
    for (auto& q : pos)
        R = std::max(R, std::sqrt((q[0] - c[0]) * (q[0] - c[0]) + (q[1] - c[1]) * (q[1] - c[1]) + (q[2] - c[2]) * (q[2] - c[2])));
    R += float(std::max(1, xsgcfg::GetInt("debug.shadow_margin"))) * 0.1f;
    // the light's basis: r = up x L, u = L x r
    float up[3] = {0, 1, 0};
    if (std::fabs(L[1]) > 0.95f) { up[0] = 1; up[1] = 0; }
    float r[3] = {up[1] * L[2] - up[2] * L[1], up[2] * L[0] - up[0] * L[2], up[0] * L[1] - up[1] * L[0]};
    const float rn = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
    for (float& v : r) v /= rn;
    const float u[3] = {L[1] * r[2] - L[2] * r[1], L[2] * r[0] - L[0] * r[2], L[0] * r[1] - L[1] * r[0]};
    ShadowMode& sm = g_shadow_mode;
    for (int k = 0; k < 3; ++k) {
        sm.lvp[k][0] = r[k] / R;
        sm.lvp[k][1] = u[k] / R;
        sm.lvp[k][2] = L[k] / (2.0f * R);
        sm.lvp[k][3] = 0;
    }
    sm.lvp[3][0] = -(c[0] * r[0] + c[1] * r[1] + c[2] * r[2]) / R;
    sm.lvp[3][1] = -(c[0] * u[0] + c[1] * u[1] + c[2] * u[2]) / R;
    sm.lvp[3][2] = -(c[0] * L[0] + c[1] * L[1] + c[2] * L[2]) / (2.0f * R);
    sm.lvp[3][3] = 1;
    const uint32_t S = 2048;
    Region srg = rg;
    srg.w = S;
    srg.h = S;
    srg.ofx = 0;
    srg.ofy = 0;
    srg.mx = 0;
    srg.zscale = 1.0f / 16777216.0f;
    sm.on = true;
    GpuTarget st = RenderGpu(cmd, list, srg, 1, nullptr);
    sm.on = false;
    static uint32_t logn;
    if (logn++ % 600 == 0)
        Log("shadows: light (%.2f %.2f %.2f), %d characters, centre (%.2f %.2f %.2f) radius %.2f", L[0], L[1], L[2], nc,
            c[0], c[1], c[2], R);
    cmd.set_program(g_shadow);
    cmd.set_texture(0, 0, depth.get_view(), Vulkan::StockSampler::NearestClamp);
    cmd.set_texture(0, 1, st.depth->get_view(), Vulkan::StockSampler::NearestClamp);
    cmd.set_storage_texture(0, 2, out.get_view());
    cmd.set_storage_texture(0, 4, czimg.get_view());
    struct {
        float inv[4][4], lvp[4][4], cscale[4], coffset[4], origin[2], zmul, strength;
        int32_t w, h, s, size;
        float bias;
        int32_t nchars;
        int32_t pad_[2];   // std140: the vec4 below starts on 16 bytes
        float lup[4];
        float chars[32][4];
    } pc = {};
    std::memcpy(pc.inv, cam->inv, 64);
    std::memcpy(pc.lvp, sm.lvp, 64);
    std::memcpy(pc.cscale, cam->scale, 16);
    std::memcpy(pc.coffset, cam->offset, 16);
    pc.origin[0] = rg.ofx - float(rg.mx);
    pc.origin[1] = rg.ofy;
    pc.zmul = 1.0f / rg.zscale;
    pc.strength = std::min(100, strength) * 0.01f;
    pc.w = int32_t(rg.W());
    pc.h = int32_t(rg.h);
    pc.s = g_scale;
    pc.size = int32_t(S);
    pc.bias = std::max(1, xsgcfg::GetInt("debug.shadow_bias")) * 1e-5f;
    // character origins (deduplicated): no shadows received within their radius
    for (auto& q : pos) {
        bool dup = false;
        for (int i = 0; i < pc.nchars && !dup; ++i)
            dup = std::fabs(pc.chars[i][0] - q[0]) + std::fabs(pc.chars[i][1] - q[1]) + std::fabs(pc.chars[i][2] - q[2]) < 1e-3f;
        if (dup || pc.nchars >= 32) continue;
        pc.chars[pc.nchars][0] = q[0];
        pc.chars[pc.nchars][1] = q[1];
        pc.chars[pc.nchars][2] = q[2];
        pc.chars[pc.nchars][3] = std::max(1, xsgcfg::GetInt("debug.shadow_char_radius")) * 0.01f;
        pc.nchars++;
    }
    pc.lup[1] = upsign;
    std::memcpy(cmd.allocate_constant_data(0, 3, sizeof pc), &pc, sizeof pc);
    cmd.dispatch((rg.W() * g_scale + 7) / 8, (rg.h * g_scale + 7) / 8, 1);
    cmd.barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);
}

/* Live path: draw the displayed buffer's native batches at 1x and g_scale and
   composite them over the paraLLEl-GS frame on the GPU (composite.comp). */
void Live(ParallelGS::GSInterface& gs) {
    uint32_t fbp;
    auto list = DisplayList(gs, &fbp);
    if (list.empty()) { g_live_img.reset(); return; }
    // At 30 fps the same frame is on display for two vsyncs: keep its composite.
    static uint32_t last_fbp = ~0u, last_gen = ~0u;
    const uint32_t gen = g_fb[fbp].vsync;
    if (g_live_img && fbp == last_fbp && gen == last_gen) return;
    last_fbp = fbp;
    last_gen = gen;
    Region rg = MakeRegion(*list.front());
    rg.mx = WideMargin(*list.front(), rg);
    // A post-processed frame the replay cannot redo: the paraLLEl-GS frame (1x).
    // The GS frame lacks what the native batches draw (the 3D scene), so it is no
    // fallback: hold the last good composite for up to three seconds (a battle's
    // opening transition reads the frame back, and its frames between may have no
    // native batches at all), then composite over the GS frame as it is.
    const char* why = "";
    xsgfx::Post post = xsgfx::PostState(fbp, &why);
    static uint32_t good_vsync = 0;
    bool good = true;
    if (post == xsgfx::Post::Unsupported) {
        static std::string last_why;
        const bool hold = g_hold_img && g_last_vsync - good_vsync < 180;
        if (last_why != why) {
            Log("vsync %u: post effect not replayable (%s; started by %s): %s", g_last_vsync, why,
                xsgfx::PostTrigger(fbp), hold ? "holding the last frame" : "drawn without it");
            last_why = why;
        }
        if (hold) {
            g_live_img = g_hold_img;
            g_live_w = g_hold_w;
            g_live_h = g_hold_h;
            g_live_mx = g_hold_mx;
            return;
        }
        post = xsgfx::Post::None;
        good = false;
    } else {
        good_vsync = g_last_vsync;
    }
    // With a post phase the composite starts from the GS frame just before it.
    const uint8_t* vram = xsgfx::PreVram(fbp);
    {
        xsgprof::Scope ps(xsgprof::LiveWait);
        gs.flush();
        if (!vram) vram = static_cast<const uint8_t*>(gs.map_vram_read(0, 4u << 20));
    }
    {   // GS-rendered textures (render targets the scene samples): from the GS memory now
        const uint8_t* mem = nullptr;
        std::map<std::pair<uint64_t, uint64_t>, std::shared_ptr<Tex>> done;
        for (const Stored* s : list) {
            if (!s->vram_tex) continue;
            if (!mem) mem = static_cast<const uint8_t*>(gs.map_vram_read(0, 4u << 20));
            auto& t = done[{s->tex0 ^ uint64_t(uintptr_t(s->clut.get())) * 31, s->texa}];
            if (!t) {
                t = std::make_shared<Tex>();
                t->px = DecodeTexture(mem, s->tex0, s->texa, &t->w, &t->h, s->clut ? s->clut->data() : nullptr);
            }
            const_cast<Stored*>(s)->tex = t;
        }
    }
    static std::vector<uint32_t> gsbuf;
    bool own;
    {
        xsgprof::Scope ps(xsgprof::LiveDecode);
        DecodeGs(vram, rg, gsbuf);
        xsgfx::OverlayColour(Surf(rg), gsbuf);   // the scene without the deferred effects (replayed below)
        own = xsgfx::OwnerPlane(Surf(rg), gsbuf);
        if (!own) gsbuf.resize(gsbuf.size() + size_t(rg.w) * rg.h);
    }
    uint64_t prof_rec0 = xsgprof::g_on ? xsgprof::Now() : 0;
    auto& dev = *g_dev;
    Vulkan::BufferCreateInfo bi = {};
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bi.domain = Vulkan::BufferDomain::LinkedDeviceHost;
    bi.size = gsbuf.size() * 4;
    auto gsb = dev.create_buffer(bi, gsbuf.data());

    auto cmd = dev.request_command_buffer();
    Vulkan::QueryPoolHandle ts[4];
    if (xsgprof::g_on) ts[0] = cmd->write_timestamp(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    // The 1x render only feeds the colour rule; with the exact ownership plane only Sx is drawn.
    GpuTarget ns = RenderGpu(*cmd, list, rg, g_scale, nullptr);
    if (xsgprof::g_on) ts[1] = cmd->write_timestamp(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    GpuTarget n1 = own ? ns : g_scale > 1 ? RenderGpu(*cmd, list, rg, 1, nullptr) : ns;
    if (xsgprof::g_on) ts[2] = cmd->write_timestamp(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    auto oi = Vulkan::ImageCreateInfo::immutable_2d_image(ns.w, ns.h, VK_FORMAT_R8G8B8A8_UNORM);
    oi.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    oi.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    oi.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    oi.layout = Vulkan::ImageLayout::General;   // storage, copies and the present blit all use GENERAL
    auto out = dev.create_image(oi);
    auto zi = Vulkan::ImageCreateInfo::immutable_2d_image(ns.w, ns.h, VK_FORMAT_R32_UINT);
    zi.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    zi.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    zi.layout = Vulkan::ImageLayout::General;
    auto zimg = dev.create_image(zi);
    cmd->image_barrier(*out, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, 0,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    cmd->image_barrier(*zimg, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, 0,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    cmd->set_program(g_comp);
    cmd->set_texture(0, 0, ns.color->get_view(), Vulkan::StockSampler::NearestClamp);
    cmd->set_texture(0, 1, ns.depth->get_view(), Vulkan::StockSampler::NearestClamp);
    cmd->set_texture(0, 2, n1.color->get_view(), Vulkan::StockSampler::NearestClamp);
    cmd->set_texture(0, 3, n1.depth->get_view(), Vulkan::StockSampler::NearestClamp);
    cmd->set_storage_buffer(0, 4, *gsb);
    cmd->set_storage_texture(0, 5, out->get_view());
    cmd->set_storage_texture(0, 6, zimg->get_view());
    struct { int32_t w, h, s, thr; float zmul; int32_t own, mx; } pc = {int32_t(rg.w), int32_t(rg.h), g_scale, g_thr,
                                                                         1.0f / rg.zscale, own ? 1 : 0, int32_t(rg.mx)};
    cmd->push_constants(&pc, 0, sizeof pc);
    cmd->dispatch((ns.w + 7) / 8, (ns.h + 7) / 8, 1);
    cmd->barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                 VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                 VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);
    // Shadows from the key light ([renderer] shadows): shadow map + receiver, before the effects.
    if (const int strength = xsgcfg::GetInt("renderer.shadows"); strength > 0 && g_shadow)
        ShadowPass(*cmd, list, rg, *ns.depth, *out, *zimg, strength);
    // Effects after the scenery (native_fx.cpp), then the PMODE circuit blend, at scale g_scale.
    if (post == xsgfx::Post::Replay || xsgfx::Overlay(fbp)) xsgfx::Replay(*cmd, fbp, g_scale, rg.w, rg.h, *out, *zimg, rg.mx);
    xsgfx::Pmode(*cmd, gs, g_scale, rg.W(), rg.h, *out, *zimg);
    cmd->barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                 VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    // Motion vectors last, so they see the final composite (+ XSG_GPU_MV_VIEW).
    xsgskin::ResolveMv(*cmd, *out, *ns.color, *ns.depth, ns.mv, *gsb, g_scale, 1.0f / rg.zscale, own, int(rg.w),
                       int(rg.mx));
    // The mouse (port/docs/MOUSE.md): outlines around the hovered / selected objects, and the
    // object id under the pointer (read back below, a few frames later).
    {
        int sel = 0, hov = 0, mx = 0, my = 0;
        xsgui::PointerOutlineIds(&sel, &hov);
        uint32_t hmask[8], hany = 0;
        xsgui::PointerHoverable(hmask);
        for (uint32_t w : hmask) hany |= w;
        // the pointer may lie in the widescreen margins (rg.mx columns either side of the GS frame)
        const bool ptr = xsgui::PointerGame(&mx, &my) && mx >= -int(rg.mx) && my >= 0 && mx < int(rg.w + rg.mx) && my < int(rg.h);
        static const bool mlog = std::getenv("XSG_MOUSE_LOG") != nullptr;
        if (mlog) {
            static int ls = -1, lh = -1, lo = -1;
            if (sel != ls || hov != lh || g_obj != lo)
                ls = sel, lh = hov, lo = g_obj, Log("mouse: outline %d / %d, object now %d, pointer %d,%d", sel, hov, g_obj, mx, my);
        }
        if ((sel || hov || (hany && ptr)) && g_outline) {
            cmd->barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
            cmd->set_program(g_outline);
            cmd->set_texture(0, 0, ns.id->get_view(), Vulkan::StockSampler::NearestClamp);
            cmd->set_storage_texture(0, 1, out->get_view());
            // the hovered object is found by the shader itself at the pointer's pixel (no read-back
            // delay), among the actors the game allows; the game's own choice (hov) otherwise.
            // Colours: the menu's kWarn (244,196,98) selected, kCyan (118,214,255) hovered; no pulse
            struct { int32_t a[4]; float cs[4], ch[4]; int32_t ptr[4]; uint32_t hm[8]; } op = {
                {sel, hov, std::max(2, g_scale), 0},
                {0.957f, 0.769f, 0.384f, 0.90f},
                {0.463f, 0.839f, 1.0f, 0.85f},
                {ptr ? (mx + int(rg.mx)) * g_scale + g_scale / 2 : -1, ptr ? my * g_scale + g_scale / 2 : -1, 0, 0},
                {hmask[0], hmask[1], hmask[2], hmask[3], hmask[4], hmask[5], hmask[6], hmask[7]}};
            cmd->push_constants(&op, 0, sizeof op);
            cmd->dispatch((ns.w + 7) / 8, (ns.h + 7) / 8, 1);
        }
        if (ptr && g_picks.size() < 8) {
            PickRead pr;
            if (!g_pick_pool.empty()) {
                pr.buf = std::move(g_pick_pool.back());
                g_pick_pool.pop_back();
            } else {
                Vulkan::BufferCreateInfo pb = {};
                pb.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                pb.domain = Vulkan::BufferDomain::CachedHost;
                pb.size = 4;
                pr.buf = dev.create_buffer(pb);
            }
            // the id target is sampled above (SHADER_READ_ONLY); a copy reads it as TRANSFER_SRC
            cmd->image_barrier(*ns.id, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT, 0,
                               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
            const int32_t px = (mx + int(rg.mx)) * g_scale + g_scale / 2, py = my * g_scale + g_scale / 2;
            cmd->copy_image_to_buffer(*pr.buf, *ns.id, 0, {px, py, 0}, {1, 1, 1}, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1});
            cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
                         VK_ACCESS_2_HOST_READ_BIT);
            g_picks.push_back(std::move(pr));
        }
    }
    cmd->image_barrier(*out, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    if (xsgprof::g_on) {
        ts[3] = cmd->write_timestamp(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
        g_ts.push_back({ts[0], ts[1], ts[2], ts[3]});
        xsgprof::g_ns[xsgprof::LiveRecord] += xsgprof::Now() - prof_rec0;
    }
    {
        xsgprof::Scope ps(xsgprof::LiveSubmit);
        if (!g_picks.empty() && !g_picks.back().fence) dev.submit(cmd, &g_picks.back().fence);
        else dev.submit(cmd);
    }
    // the oldest finished pick read: the object under the pointer
    while (!g_picks.empty() && g_picks.front().fence && g_picks.front().fence->wait_timeout(0)) {
        const uint16_t* v = static_cast<const uint16_t*>(dev.map_host_buffer(*g_picks.front().buf, Vulkan::MEMORY_ACCESS_READ_BIT));
        if (v) xsgui::PointerSetPick(int(*v));
        dev.unmap_host_buffer(*g_picks.front().buf, Vulkan::MEMORY_ACCESS_READ_BIT);
        g_pick_pool.push_back(std::move(g_picks.front().buf));
        g_picks.erase(g_picks.begin());
    }
    if (g_picks.size() >= 8) g_picks.erase(g_picks.begin());   // never finished: drop
    ProfGpu();
    g_live_img = out;
    g_live_w = rg.w;
    g_live_h = rg.h;
    g_live_mx = rg.mx;
    if (good) {
        g_hold_img = out;
        g_hold_w = rg.w;
        g_hold_h = rg.h;
        g_hold_mx = rg.mx;
    }
    g_live_frames++;
}

void Shot(uint32_t vsync, ParallelGS::GSInterface& gs) {
    if (const char* t = xsgcfg::CStr("debug.dump_tex"); t && *t) {   // TEX0 values (hex, comma list) -> PNGs
        gs.flush();
        const uint8_t* vram = static_cast<const uint8_t*>(gs.map_vram_read(0, 4u << 20));
        for (const char* q = t; *q;) {
            char* end;
            const uint64_t tex0 = std::strtoull(q, &end, 16);
            if (end == q) break;
            uint32_t w, h;
            auto px = DecodeTexture(vram, tex0, 0, &w, &h);
            for (auto& c : px) c |= 0xff000000u;
            char name[256];
            std::snprintf(name, sizeof name, "%s/tex_%06u_%llx.png", g_dir.c_str(), vsync, (unsigned long long)tex0);
            WritePng(name, w, h, px);
            Log("  texture %llx -> %s", (unsigned long long)tex0, name);
            q = *end ? end + 1 : end;
        }
    }
    uint32_t fbp;
    auto list = DisplayList(gs, &fbp);
    if (list.empty()) { Log("vsync %u: no native batches for the displayed FBP 0x%x", vsync, fbp); return; }
    const Region rg = MakeRegion(*list.front());
    const uint32_t zpsm = 0x30 | rg.zpsm;
    Log("vsync %u: %zu batches to the displayed FBP 0x%x, %ux%u, FBW %u PSM 0x%x, ZBP 0x%x ZPSM 0x%x, "
        "XYOFFSET (%.1f, %.1f)",
        vsync, list.size(), fbp, rg.w, rg.h, rg.fbw, rg.fpsm, rg.zbp, zpsm, rg.ofx, rg.ofy);

    gs.flush();
    std::vector<uint8_t> vram(4u << 20);
    std::memcpy(vram.data(), gs.map_vram_read(0, 4u << 20), vram.size());
    Target n1 = Render(list, rg, 1);
    Target ns = g_scale > 1 ? Render(list, rg, g_scale) : n1;
    Log("  %zu texture images (%llu decoded so far)", g_teximg.size(), (unsigned long long)g_ntex);

    // The paraLLEl-GS frame and depth at 1x.
    std::vector<uint32_t> gsbuf;
    DecodeGs(vram.data(), rg, gsbuf);
    std::vector<uint32_t> gsc(gsbuf.begin(), gsbuf.begin() + size_t(rg.w) * rg.h);
    const uint32_t* gsz = gsbuf.data() + gsc.size();
    const float zmul = 1.0f / rg.zscale;
    std::vector<uint32_t> ownv;
    const bool own = xsgfx::OwnerPlane(Surf(rg), ownv);
    // The same rule as composite.comp, on the CPU (ownership plane, or the old colour rule).
    auto composite = [&](const Target& t, int s, uint64_t* covered, uint64_t* used) {
        std::vector<uint32_t> out(size_t(t.w) * t.h);
        for (uint32_t y = 0; y < t.h; ++y)
            for (uint32_t x = 0; x < t.w; ++x) {
                const uint32_t gx = x / s, gy = y / s;
                const size_t gi = size_t(gy) * rg.w + gx, ti = size_t(y) * t.w + x;
                uint32_t c = gsc[gi];
                const float d = t.depth[ti];
                if (d > 0.0f) {
                    ++*covered;
                    const double zn = double(d) * zmul, zg = double(gsz[gi]);
                    const double eps = std::max(64.0, zg * 2e-4);
                    bool ok = zn + eps >= zg;
                    if (own) ok = ownv[gi] != 0;
                    else if (ok && n1.depth[gi] > 0.0f) ok = ChanDiff(n1.color[gi], gsc[gi]) <= g_thr;
                    if (ok) { c = t.color[ti] | 0xff000000u; ++*used; }
                }
                out[ti] = c;
            }
        return out;
    };
    mkdir(g_dir.c_str(), 0755);
    char p[600];
    auto path = [&](const char* what) {
        std::snprintf(p, sizeof p, "%s/native_%06u_%s.png", g_dir.c_str(), vsync, what);
        return std::string(p);
    };
    WritePng(path("gs_1x"), rg.w, rg.h, gsc);
    if (own) {
        std::vector<uint32_t> oimg(gsc.size());
        uint64_t n = 0;
        for (size_t i = 0; i < gsc.size(); ++i) { oimg[i] = ownv[i] ? 0xffffffu : (gsc[i] >> 2) & 0x3f3f3f; n += ownv[i]; }
        WritePng(path("owner_1x"), rg.w, rg.h, oimg);
        Log("  ownership: native owns %llu of %zu px%s", (unsigned long long)n, gsc.size(),
            xsgfx::PreVram(fbp) ? " (before the post phase)" : "");
    }
    {
        const char* why = "";
        const xsgfx::Post post = xsgfx::PostState(fbp, &why);
        if (post != xsgfx::Post::None)
            Log("  post phase: %zu draws, %s%s; started by %s", xsgfx::PostCount(fbp),
                post == xsgfx::Post::Replay ? "replayed at native scale" : "NOT replayable: ", why, xsgfx::PostTrigger(fbp));
        uint64_t hv[4];
        std::vector<uint32_t> vimg;
        if (post == xsgfx::Post::Replay && xsgfx::Validate1x(Surf(rg), vram.data(), hv, &vimg)) {
            WritePng(path("replay_1x"), rg.w, rg.h, vimg);
            Log("  replay at 1x on the pre-effect GS frame vs paraLLEl-GS: exact %llu, <=2 %llu, <=8 %llu, >8 %llu",
                (unsigned long long)hv[0], (unsigned long long)hv[1], (unsigned long long)hv[2], (unsigned long long)hv[3]);
        }
    }
    WritePng(path("native_only_1x"), n1.w, n1.h, n1.color);
    uint64_t cov1 = 0, use1 = 0;
    auto c1 = composite(n1, 1, &cov1, &use1);
    WritePng(path("comp_1x"), n1.w, n1.h, c1);
    // The 1x composite against the paraLLEl-GS frame, over the whole frame.
    uint64_t hc[4] = {};
    for (size_t i = 0; i < gsc.size(); ++i) {
        const int dd = ChanDiff(c1[i], gsc[i]);
        hc[dd == 0 ? 0 : dd <= 2 ? 1 : dd <= 8 ? 2 : 3]++;
    }
    // 1x diff: native-only vs GS where native covers. Exact / within 2 / within 8 / worse,
    // all covered pixels (h) and those where the GS depth is the native surface's (hz:
    // nothing non-native in front of it).
    uint64_t h[4] = {}, hz[4] = {};
    std::vector<uint32_t> dimg(gsc.size());
    for (size_t i = 0; i < gsc.size(); ++i) {
        if (!(n1.depth[i] > 0.0f)) { dimg[i] = (gsc[i] >> 2) & 0x3f3f3f; continue; }
        const int dd = ChanDiff(n1.color[i], gsc[i]);
        const int bin = dd == 0 ? 0 : dd <= 2 ? 1 : dd <= 8 ? 2 : 3;
        h[bin]++;
        const double zn = double(n1.depth[i]) * zmul, zg = double(gsz[i]);
        if (std::fabs(zn - zg) <= std::max(64.0, zg * 2e-4)) hz[bin]++;
        const uint32_t v = uint32_t(std::min(255, dd * 8));
        dimg[i] = dd == 0 ? 0x004000u : v | (v << 8) | (v << 16);
    }
    WritePng(path("diff_1x"), rg.w, rg.h, dimg);
    Log("  1x: native covers %llu px, composite takes %llu; vs paraLLEl-GS where covered: exact %llu, <=2 %llu, "
        "<=8 %llu, >8 %llu",
        (unsigned long long)cov1, (unsigned long long)use1, (unsigned long long)h[0], (unsigned long long)h[1],
        (unsigned long long)h[2], (unsigned long long)h[3]);
    Log("  1x: where the GS depth is the native surface's: exact %llu, <=2 %llu, <=8 %llu, >8 %llu",
        (unsigned long long)hz[0], (unsigned long long)hz[1], (unsigned long long)hz[2], (unsigned long long)hz[3]);
    Log("  1x composite vs paraLLEl-GS, whole frame (%zu px): exact %llu, <=2 %llu, <=8 %llu, >8 %llu", gsc.size(),
        (unsigned long long)hc[0], (unsigned long long)hc[1], (unsigned long long)hc[2], (unsigned long long)hc[3]);
    if (g_scale > 1) {
        uint64_t cov = 0, use = 0;
        auto cs = composite(ns, g_scale, &cov, &use);
        char what[32];
        std::snprintf(what, sizeof what, "comp_%dx", g_scale);
        WritePng(path(what), ns.w, ns.h, cs);
        std::snprintf(what, sizeof what, "native_only_%dx", g_scale);
        WritePng(path(what), ns.w, ns.h, ns.color);
        Log("  %dx: native covers %llu samples, composite takes %llu", g_scale, (unsigned long long)cov,
            (unsigned long long)use);
    }
    // What the window shows: the live GPU composite of this vsync.
    if (g_live && g_live_img) {
        auto live = Readback<uint32_t>(*g_live_img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL);
        char what[32];
        std::snprintf(what, sizeof what, "live_%dx", g_scale);
        WritePng(path(what), g_live_img->get_width(), g_live_img->get_height(), live);
        // Box-downsampled to 1x, against the paraLLEl-GS frame.
        const int s = g_scale;
        const uint32_t lw = g_live_img->get_width() / s, lm = lw > rg.w ? (lw - rg.w) / 2 : 0;
        if (lm) {   // widescreen: the whole picture box-downsampled too
            std::vector<uint32_t> wide(size_t(lw) * rg.h);
            for (uint32_t y = 0; y < rg.h; ++y)
                for (uint32_t x = 0; x < lw; ++x) {
                    uint32_t acc[3] = {};
                    for (int j = 0; j < s; ++j)
                        for (int i = 0; i < s; ++i) {
                            const uint32_t c = live[size_t(y * s + j) * lw * s + x * s + i];
                            for (int k = 0; k < 3; ++k) acc[k] += (c >> (8 * k)) & 255;
                        }
                    uint32_t c = 0xff000000u;
                    for (int k = 0; k < 3; ++k) c |= ((acc[k] + s * s / 2) / (s * s)) << (8 * k);
                    wide[size_t(y) * lw + x] = c;
                }
            WritePng(path("live_wide_1x"), lw, rg.h, wide);
        }
        std::vector<uint32_t> down(gsc.size());
        uint64_t hl[4] = {};
        for (uint32_t y = 0; y < rg.h; ++y)
            for (uint32_t x = 0; x < rg.w; ++x) {
                uint32_t acc[3] = {};
                for (int j = 0; j < s; ++j)
                    for (int i = 0; i < s; ++i) {
                        const uint32_t c = live[size_t(y * s + j) * lw * s + (x + lm) * s + i];
                        for (int k = 0; k < 3; ++k) acc[k] += (c >> (8 * k)) & 255;
                    }
                uint32_t c = 0xff000000u;
                for (int k = 0; k < 3; ++k) c |= ((acc[k] + s * s / 2) / (s * s)) << (8 * k);
                down[size_t(y) * rg.w + x] = c;
                const int dd = ChanDiff(c, gsc[size_t(y) * rg.w + x]);
                hl[dd == 0 ? 0 : dd <= 2 ? 1 : dd <= 8 ? 2 : 3]++;
            }
        WritePng(path("live_down_1x"), rg.w, rg.h, down);
        Log("  live %dx composite, box-downsampled to 1x vs paraLLEl-GS: exact %llu, <=2 %llu, <=8 %llu, >8 %llu", s,
            (unsigned long long)hl[0], (unsigned long long)hl[1], (unsigned long long)hl[2], (unsigned long long)hl[3]);
    }
    if (g_live) {
        std::snprintf(p, sizeof p, "%s/native_%06u_", g_dir.c_str(), vsync);
        xsgskin::ShotMv(p);
    }
    Log("  images: %s/native_%06u_*.png", g_dir.c_str(), vsync);
}

bool g_ready;   // pipelines, the skin and fx state and the ownership GS exist
bool InitResources();

}  // namespace

/* The device; the resources are made when the renderer is first turned on
   (here or from the menu, Apply). */
bool Init(Vulkan::Device* dev) {
    g_dev = dev;
    if (const char* s = xsgcfg::CStr("renderer.native_scale")) g_scale = std::max(1, std::min(8, std::atoi(s)));
    const char* e = xsgcfg::CStr("renderer.native");
    if (!(e && *e && std::strcmp(e, "0") != 0)) return false;
    if (!InitResources()) return false;
    g_on = true;
    return true;
}

/* Live settings ([renderer] native, native_scale) after a menu change.  Turned on
   mid-game, the renderer has not followed the GS stream: its upload mirror and the
   ownership GS start from the main GS's memory, its register mirror refills from
   the game's next packets (the composite can be off for a frame). */
void Apply(ParallelGS::GSInterface& gs) {
    g_scale = std::max(1, std::min(8, xsgcfg::GetInt("renderer.native_scale")));
    const bool want = xsgcfg::GetBool("renderer.native");
    if (want == g_on) return;
    if (!want) {
        g_on = false;
        g_live_img.reset();
        g_hold_img.reset();
        Log("off (menu)");
        return;
    }
    if (!g_ready && !InitResources()) { Log("cannot turn on: resource setup failed"); return; }
    gs.flush();
    const uint8_t* v = static_cast<const uint8_t*>(gs.map_vram_read(0, 4u << 20));
    XferFlush();
    std::memcpy(g_mem.data(), v, 4u << 20);
    g_upload_gen++;
    for (auto& pg : g_page_gen) pg = g_upload_gen;
    xsgfx::SeedOwn(v);
    g_on = true;
    Log("on (menu): scale %dx, state seeded from the GS", g_scale);
}

namespace {
bool InitResources() {
    Vulkan::Device* dev = g_dev;
    xsgprof::Init();
    if (const char* s = xsgcfg::CStr("debug.native_live"); s && *s) g_live = std::atoi(s) != 0;
    if (const char* s = xsgcfg::CStr("debug.native_thr"); s && *s) g_thr = std::atoi(s);
    if (const char* s = xsgcfg::CStr("debug.native_dir"); s && *s) g_dir = s;
    if (const char* s = xsgcfg::CStr("debug.native_shot"))
        for (const char* q = s; *q;) {
            char* end;
            unsigned long v = std::strtoul(q, &end, 0);
            if (end == q) break;
            g_shots.insert(uint32_t(v));
            q = *end ? end + 1 : end;
        }
    Vulkan::ResourceLayout vl, fl, cl;
    vl.input_mask = 15;
    vl.sets[0].uniform_buffer_mask = 1;
    vl.sets[0].meta[0].array_size = 1;
    fl.output_mask = 7;
    fl.sets[0].uniform_buffer_mask = 1;
    fl.sets[0].sampled_image_mask = 2;
    fl.sets[0].meta[0].array_size = 1;
    fl.sets[0].meta[1].array_size = 1;
    auto* vs = dev->request_shader(kVert, sizeof kVert, &vl);
    auto* fs = dev->request_shader(kFrag, sizeof kFrag, &fl);
    g_prog = dev->request_program(vs, fs);
    cl.sets[0].sampled_image_mask = 0xf;
    cl.sets[0].storage_buffer_mask = 1u << 4;
    cl.sets[0].storage_image_mask = (1u << 5) | (1u << 6);
    for (int i = 0; i < 7; ++i) cl.sets[0].meta[i].array_size = 1;
    cl.push_constant_size = 32;
    auto* cs = dev->request_shader(kComp, sizeof kComp, &cl);
    g_comp = dev->request_program(cs);
    {
        Vulkan::ResourceLayout sl;
        sl.sets[0].sampled_image_mask = 3;
        sl.sets[0].storage_image_mask = 4 | 16;
        sl.sets[0].uniform_buffer_mask = 8;
        for (int i = 0; i < 5; ++i) sl.sets[0].meta[i].array_size = 1;
        g_shadow = dev->request_program(dev->request_shader(kShadow, sizeof kShadow, &sl));
    }
    {   // the mouse's object outlines (outline.comp)
        Vulkan::ResourceLayout ol;
        ol.sets[0].sampled_image_mask = 1;
        ol.sets[0].storage_image_mask = 2;
        for (int i = 0; i < 2; ++i) ol.sets[0].meta[i].array_size = 1;
        ol.push_constant_size = 64;
        g_outline = dev->request_program(dev->request_shader(kOutline, sizeof kOutline, &ol));
    }
    if (!xsgskin::Init(dev)) return false;
    xsgfx::Init(dev);
    Log("on: set-0 strip batches, scale %dx, live %s, %zu shot vsyncs -> %s", g_scale, g_live ? "on" : "off",
        g_shots.size(), g_dir.c_str());
    g_ready = g_prog != nullptr && g_comp != nullptr;
    return g_ready;
}
}  // namespace

bool On() { return g_on; }

void Gif(uint32_t path, const uint8_t* data, uint32_t len, ParallelGS::GSInterface& gs) {
    PathParse& p = g_path[path & 3];
    // qword by qword together with the effect parser, so its draws see the uploads before them
    // the GIF state tracker, per qword ahead of the effect parser (a plain function pointer: per 16 bytes)
    xsgfx::Gif(path, data, len, gs, [](void* c, const uint8_t* q) { GifQword(*static_cast<PathParse*>(c), q); }, &p);
    XferFlush();   // a message of another path may come next
}

namespace {
/* The GS state a batch draws with (PRIM from its GIF tag, the registers of that
   context), and its texture. */
uint64_t PageHash(uint32_t page);
void Snap(Stored& s, const uint32_t* tagw) {
    xsgprof::Scope ps(xsgprof::BatchSnap);
    s.obj = g_obj;
    uint64_t tag = uint64_t(tagw[0]) | uint64_t(tagw[1]) << 32;
    uint64_t prim = Bits(tag, 46, 1) ? Bits(tag, 47, 11) : g_reg[R_PRIM];
    if (!(g_reg[R_PRMODECONT] & 1)) prim = (prim & 7) | (g_reg[R_PRMODE] & 0x7f8);
    const int c = Bits(prim, 9, 1);
    s.prim = prim;
    s.tex0 = g_reg[R_TEX0_1 + c];
    s.clamp = g_reg[R_CLAMP_1 + c];
    s.tex1 = g_reg[R_TEX1_1 + c];
    s.xyoff = g_reg[R_XYOFFSET_1 + c];
    s.scissor = g_reg[R_SCISSOR_1 + c];
    s.alpha = g_reg[R_ALPHA_1 + c];
    s.test = g_reg[R_TEST_1 + c];
    s.frame = g_reg[R_FRAME_1 + c];
    s.zbuf = g_reg[R_ZBUF_1 + c];
    s.texa = g_reg[R_TEXA];
    s.fogcol = g_reg[R_FOGCOL];
    static auto white = std::make_shared<Tex>();
    s.tex = white;
    if (Bits(prim, 4, 1)) {
        // (TEX0, TEXA) -> the newest generation of its pages and their content hash.
        struct Ver { uint32_t gen; uint64_t hash; };
        static std::map<std::pair<uint64_t, uint64_t>, Ver> ver;
        static std::map<std::tuple<uint64_t, uint64_t, uint64_t>, std::shared_ptr<Tex>> cache;
        if (cache.size() > 1024) cache.clear();
        if (ver.size() > 4096) ver.clear();
        const uint64_t t0 = s.tex0 & ((1ull << 61) - 1);
        uint32_t p0, np, gen = 0;
        PageRange(Bits(t0, 0, 14), std::max(1u, Bits(t0, 14, 6)), Bits(t0, 20, 6), 1u << std::min(10u, Bits(t0, 30, 4)), &p0, &np);
        const uint32_t cpage = Bits(t0, 37, 14) / 32;
        for (uint32_t i = 0; i < np; ++i) gen = std::max(gen, g_page_gen[(p0 + i) & 511]);
        gen = std::max(gen, std::max(g_page_gen[cpage & 511], g_page_gen[(cpage + 1) & 511]));
        Ver& vv = ver[{t0, s.texa}];
        if (!vv.hash || vv.gen != gen) {
            uint64_t h = 1469598103934665603ull;
            auto mix = [&](uint32_t page) { h = (h ^ PageHash(page & 511)) * 1099511628211ull; };
            for (uint32_t i = 0; i < np; ++i) mix(p0 + i);
            (void)cpage;
            vv.gen = gen;
            vv.hash = h | 1;
        }
        const bool indexed = Bits(t0, 20, 6) >= 0x13;   // its colours: the CLUT buffer now
        auto key = std::make_tuple(t0, s.texa, vv.hash ^ (indexed ? g_cbuf.hash * 31 : 0));
        auto it = cache.find(key);
        if (it == cache.end()) {
            auto t = std::make_shared<Tex>();
            xsgprof::Scope ps(xsgprof::TexDecode);
            t->px = DecodeTexture(g_mem.data(), s.tex0, s.texa, &t->w, &t->h, g_cbuf.slot);
            it = cache.emplace(key, t).first;
            g_ntex++;
        }
        s.tex = it->second;
        s.vram_tex = xsgfx::GsRenderedTexture(s.tex0);
        if (s.vram_tex && indexed) {
            // One snapshot per CLUT content: the live pass decodes each (TEX0, snapshot) once a
            // frame, and a scene that switches palettes back and forth (A, B, A, ...) between
            // batches otherwise gets a new snapshot, and a new decode, per switch (#3: the
            // A.G.W.S. cutscene decoded 18 Mpx a frame for 4 distinct textures).
            static std::unordered_map<uint64_t, std::shared_ptr<const std::array<uint16_t, 512>>> snaps;
            if (snaps.size() > 256 && !snaps.count(g_cbuf.hash)) snaps.clear();
            auto& c = snaps[g_cbuf.hash];
            if (!c) {
                auto n = std::make_shared<std::array<uint16_t, 512>>();
                std::memcpy(n->data(), g_cbuf.slot, sizeof(uint16_t) * 512);
                c = n;
            }
            s.clut = c;
        }
    }
}
/* A content hash per 8 KB page of the mirror, recomputed only after an upload
   touched the page (g_page_gen).  Four independent lanes (the one-lane FNV
   chain was latency-bound), folded at the end. */
uint64_t PageHash(uint32_t page) {
    static uint64_t hash[512];
    static uint32_t at[512];
    static bool valid[512];
    if (valid[page] && at[page] == g_page_gen[page]) return hash[page];
    const uint64_t* q = reinterpret_cast<const uint64_t*>(g_mem.data() + size_t(page) * 8192);
    uint64_t l[4] = {1469598103934665603ull, 0x9e3779b97f4a7c15ull, 0xc2b2ae3d27d4eb4full, 0x165667b19e3779f9ull};
    for (int i = 0; i < 1024; i += 4)
        for (int k = 0; k < 4; ++k) l[k] = (l[k] ^ q[i + k]) * 1099511628211ull;
    uint64_t h = 0;
    for (int k = 0; k < 4; ++k) h = (h ^ (l[k] + (h << 6) + (h >> 2))) * 0x100000001b3ull;
    hash[page] = h;
    at[page] = g_page_gen[page];
    valid[page] = true;
    return h;
}

void Push(Stored&& s) {
    s.cam = g_cam;
    FbFrame& f = g_fb[Bits(s.frame, 0, 9)];
    if (f.vsync != g_vsyncs) {
        f.b.clear();
        f.vsync = g_vsyncs;
    }
    f.b.push_back(std::move(s));
}
std::shared_ptr<const xsgskin::Draw> g_env_pending;   // a six-kick set-2 batch waiting for its env pass state

/* native_fx: the next `ntags` PATH1 GIF tags are this batch's output (drawn in
   the marker colour by the ownership GS), and the frame buffer it draws into. */
void FxNative(const uint32_t* tagw, uint32_t ntags) {
    if (ntags) xsgfx::NativeNext(ntags);
    const uint64_t tg = uint64_t(tagw[0]) | uint64_t(tagw[1]) << 32;
    const uint64_t pr = Bits(tg, 46, 1) ? Bits(tg, 47, 11) : g_reg[R_PRIM];
    xsgfx::NativeFbp(Bits(g_reg[R_FRAME_1 + Bits(pr, 9, 1)], 0, 9), Bits(g_reg[R_ZBUF_1 + Bits(pr, 9, 1)], 0, 9));
}
}  // namespace

void Batch(uint32_t kind, uint32_t nvert, const uint8_t* payload, uint32_t len) {
    XferFlush();
    xsgprof::Scope ps(xsgprof::Batch);
    if (kind == GPU_NATIVE_OBJECT) {   // the object (actor + 1) of the batches that follow
        g_obj = int(nvert);
        return;
    }
    if (kind == GPU_NATIVE_CAMERA) {
        g_obj = 0;   // a new scene pass: no object until its models mark theirs
        if (len < sizeof(GpuNativeCamera)) return;
        const GpuNativeCamera* c = reinterpret_cast<const GpuNativeCamera*>(payload);
        auto cam = std::make_shared<Camera>();
        std::memcpy(cam->vp, c->vp, 64);
        std::memcpy(cam->scale, c->scale, 16);
        std::memcpy(cam->offset, c->offset, 16);
        cam->valid = Invert4(cam->vp, cam->inv);
        g_cam = cam->valid ? cam : nullptr;
        return;
    }
    if (kind == GPU_NATIVE_SET2_ENV) {   // the env pass's A+D state has been kicked: snapshot it now
        if (g_env_pending) FxNative(g_env_pending->b.tag1010, 1);   // the env pass packet is kicked next
        if (g_env_pending && Near()) {
            Stored s;
            s.skin = g_env_pending;
            s.pass = 2;
            s.b = {};
            std::memcpy(s.b.scale, s.skin->b.scale, 16);
            Snap(s, s.skin->b.tag1010);
            Push(std::move(s));
        }
        g_env_pending.reset();
        return;
    }
    if (kind == GPU_NATIVE_SET2 || kind == GPU_NATIVE_SET2_PALETTE || kind == GPU_NATIVE_SET0_LIT) {
        std::shared_ptr<const xsgskin::Draw> d;
        {
            xsgprof::Scope ps(xsgprof::BatchSkin);
            d = xsgskin::Message(kind, nvert, payload, len);
        }
        g_env_pending.reset();
        if (!d) return;
        g_nbatch++;
        g_nvert += nvert;
        // Two-pass programs kick one packet with both tags (1011, 1012).
        FxNative(d->b.tag1011, d->variant == 2 ? 2 : 1);
        if (d->variant == 2) FxNative(d->b.tag1012, 0);   // pass 2's frame buffer (context 2) too
        if (d->variant == 3) g_env_pending = d;
        if (!Near()) return;
        for (int pass = 0; pass < (d->variant == 2 ? 2 : 1); ++pass) {
            Stored s;
            s.skin = d;
            s.pass = pass;
            s.b = {};
            std::memcpy(s.b.scale, d->b.scale, 16);
            Snap(s, pass ? d->b.tag1012 : d->b.tag1011);
            Push(std::move(s));
        }
        return;
    }
    if (kind != GPU_NATIVE_SET0_STRIP || len < sizeof(GpuNativeBatch) + nvert * sizeof(GpuNativeVertex)) return;
    g_nbatch++;
    g_nvert += nvert;
    FxNative(reinterpret_cast<const GpuNativeBatch*>(payload)->tag, 1);
    const GpuNativeBatch* pb = reinterpret_cast<const GpuNativeBatch*>(payload);
    const GpuNativeVertex* pv = reinterpret_cast<const GpuNativeVertex*>(payload + sizeof(GpuNativeBatch));
    if (!Near()) { xsgskin::Set0Prev(*pb, pv, nvert, nullptr, nullptr); return; }
    Stored s;
    std::memcpy(&s.b, payload, sizeof s.b);
    s.v.resize(nvert);
    std::memcpy(s.v.data(), pv, nvert * sizeof(GpuNativeVertex));
    s.hasprev = xsgskin::Set0Prev(s.b, pv, nvert, s.v.data(), s.mprev);
    Snap(s, s.b.tag);
    Push(std::move(s));
}

void Vsync(uint32_t frame, ParallelGS::GSInterface& gs, bool show) {
    XferFlush();
    g_last_vsync = frame;
    g_obj = 0;   // batches of the next frame carry no object id until a model marks one
    {
        xsgprof::Scope ps(xsgprof::FxVsync);
        xsgfx::Vsync(frame, gs);   // first: it closes the post phase of the frame just drawn
    }
    {
        xsgprof::Scope ps(xsgprof::Other);
        xsgskin::Vsync(frame);
    }
    if (g_live && show) Live(gs);
    if (g_shots.count(frame)) Shot(frame, gs);
    if (frame % 600 == 0)
        Log("vsync %u: %llu batches, %llu vertices, %llu live composites so far", frame, (unsigned long long)g_nbatch,
            (unsigned long long)g_nvert, (unsigned long long)g_live_frames);
    // Texture images whose decoded texture only the image cache still holds are gone.
    if (g_teximg.size() > 512)
        for (auto it = g_teximg.begin(); it != g_teximg.end();)
            it = it->second.first.use_count() <= 1 ? g_teximg.erase(it) : std::next(it);
    if (g_fb.size() > 8) g_fb.clear();
    g_vsyncs++;
    xsgprof::Vsync(frame);
}

const Vulkan::ImageHandle& LiveImage(uint32_t* fb_w, uint32_t* fb_h, uint32_t* margin) {
    *fb_w = g_live_w;
    *fb_h = g_live_h;
    if (margin) *margin = g_live_mx;
    return g_live_img;
}

int Scale() { return g_scale; }

std::vector<uint32_t> DecodeMirrorTexture(uint64_t tex0, uint64_t texa, uint32_t* w, uint32_t* h) {
    XferFlush();
    return DecodeTexture(g_mem.data(), tex0, texa, w, h, g_cbuf.slot);
}
uint64_t ClutHash() { return g_cbuf.hash; }
uint32_t MirrorGen() { return g_upload_gen; }
const uint8_t* MirrorData() { XferFlush(); return g_mem.data(); }
uint32_t TextureGen(uint64_t tex0) {
    uint32_t p0, np, gen = 0;
    PageRange(Bits(tex0, 0, 14), std::max(1u, Bits(tex0, 14, 6)), Bits(tex0, 20, 6), 1u << std::min(10u, Bits(tex0, 30, 4)), &p0, &np);
    for (uint32_t i = 0; i < np; ++i) gen = std::max(gen, g_page_gen[(p0 + i) & 511]);
    const uint32_t cpage = Bits(tex0, 37, 14) / 32;
    return std::max(gen, std::max(g_page_gen[cpage & 511], g_page_gen[(cpage + 1) & 511]));
}
uint64_t TextureHash(uint64_t tex0) {
    uint32_t p0, np;
    PageRange(Bits(tex0, 0, 14), std::max(1u, Bits(tex0, 14, 6)), Bits(tex0, 20, 6), 1u << std::min(10u, Bits(tex0, 30, 4)), &p0, &np);
    uint64_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < np; ++i) h = (h ^ PageHash((p0 + i) & 511)) * 1099511628211ull;
    return h;
}
uint32_t TextureGenMin(uint64_t tex0) {
    uint32_t p0, np, gen = ~0u;
    PageRange(Bits(tex0, 0, 14), std::max(1u, Bits(tex0, 14, 6)), Bits(tex0, 20, 6), 1u << std::min(10u, Bits(tex0, 30, 4)), &p0, &np);
    for (uint32_t i = 0; i < np; ++i) gen = std::min(gen, g_page_gen[(p0 + i) & 511]);
    return np ? gen : 0;
}
uint32_t PageGen(uint32_t page) { return g_page_gen[page & 511]; }
std::vector<uint32_t> DecodeVramTexture(const uint8_t* vram, uint64_t tex0, uint64_t texa, uint32_t* w, uint32_t* h) {
    return DecodeTexture(vram, tex0, texa, w, h);
}

void ParallelRows(uint32_t h, const std::function<void(uint32_t, uint32_t)>& fn) {
    constexpr uint32_t kThreads = 4;
    if (h < 64) { fn(0, h); return; }
    std::thread th[kThreads - 1];
    const uint32_t step = (h + kThreads - 1) / kThreads;
    for (uint32_t i = 1; i < kThreads; ++i)
        th[i - 1] = std::thread([&fn, i, step, h] { fn(std::min(h, i * step), std::min(h, (i + 1) * step)); });
    fn(0, std::min(h, step));
    for (auto& t : th) t.join();
}

}  // namespace xsgnative

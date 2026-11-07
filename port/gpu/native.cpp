/*
 * xsg-gpu native renderer: set-0 strip batches (VU1 loop 0x0c18) drawn by our
 * own Vulkan pipeline at XSG_GPU_NATIVE_SCALE and composited live over the
 * paraLLEl-GS frame.  See native.h and port/docs/NATIVE_RENDERER.md.
 */
#include "native.h"
#include "native_skin.h"
#include "native_fx.h"
#include "native_prof.h"

#include "gpu_ipc.h"
#include "device.hpp"
#include "gs_interface.hpp"
#include "ps2host/gs.h"

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
} g_xfer;

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
        const uint32_t x = (t.x0 + t.x) & 2047, y = (t.y0 + t.y) & 2047;
        if (x + run <= 2048) g_mem.WriteRun(t.dbp, t.dbw, x, y, t.psm, px + i, run);
        else
            for (uint32_t k = 0; k < run; ++k) g_mem.Write(t.dbp, t.dbw, (x + k) & 2047, y, t.psm, px[i + k]);
        i += int(run);
        t.x += run;
        if (t.x >= t.w) { t.x = 0; if (++t.y >= t.h) t.on = false; }
    }
}

struct PathParse {
    uint32_t left = 0;      // qwords (PACKED/REGLIST) or IMAGE qwords still to come
    uint32_t flg = 0, nreg = 0, idx = 0;
    uint64_t regs = 0;
};
PathParse g_path[4];

void SetReg(uint32_t a, uint64_t v) {
    a &= 0xff;
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
        return;
    }
    p.left--;
    if (p.flg >= 2) {   // IMAGE
        XferQword(q);
        return;
    }
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
struct Stored {
    std::shared_ptr<Tex> tex;
    GpuNativeBatch b;
    std::vector<GpuNativeVertex> v;
    std::shared_ptr<const xsgskin::Draw> skin;   // set 2 (native_skin.cpp); v unused
    int pass = 0;                                // set 2: 0 tag 1011, 1 specular strip (1012), 2 env strip (1010)
    float mprev[16];                             // set 0: last frame's M962 (v[i].pad = last frame's position)
    bool hasprev = false;
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
};

uint32_t ReadU32(const uint8_t* vram, uint32_t a) {
    uint32_t v;
    std::memcpy(&v, vram + (a & 0x3fffff), 4);
    return v;
}

/* A TEX0 texture as RGBA8 (alpha = the GS byte), decoded from local memory. */
std::vector<uint32_t> DecodeTexture(const uint8_t* vram, uint64_t tex0, uint64_t texa, uint32_t* w, uint32_t* h) {
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
    if (eight || four) {
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
};

struct Region {
    uint32_t fbp, fbw, fpsm, zbp, zpsm, w, h;
    float ofx, ofy, zscale;
};

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
GpuTarget RenderGpu(Vulkan::CommandBuffer& cmdr, const std::vector<const Stored*>& list, const Region& rg, int scale,
                    unsigned* odd_blend_out) {
    GpuTarget t;
    t.w = rg.w * scale;
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
    cmd->image_barrier(*t.mv, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    auto& color = t.color;
    auto& depth = t.depth;

    cmd->image_barrier(*color, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    cmd->image_barrier(*depth, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                       VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    Vulkan::RenderPassInfo rp;
    rp.num_color_attachments = 2;
    rp.color_attachments[0] = &color->get_view();
    rp.color_attachments[1] = &t.mv->get_view();
    rp.depth_stencil = &depth->get_view();
    rp.clear_attachments = 3;
    rp.store_attachments = 3;
    rp.op_flags = Vulkan::RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT | Vulkan::RENDER_PASS_OP_STORE_DEPTH_STENCIL_BIT;
    rp.clear_depth_stencil.depth = 0.0f;
    cmd->begin_render_pass(rp);
    cmd->set_opaque_state();
    cmd->set_program(g_prog);
    cmd->set_cull_mode(VK_CULL_MODE_NONE);
    cmd->set_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    unsigned odd_blend = 0;
    xsgskin::Begin();
    for (const Stored* s : list) {
        uint32_t n = uint32_t(s->v.size());
        if (s->skin) {   // set 2: native_skin.vert, indexed triangle list
            n = xsgskin::Bind(*cmd, *s->skin, s->pass);
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
        std::memcpy(u->m, s->b.m, sizeof u->m);
        std::memcpy(u->scale, s->b.scale, 16);
        std::memcpy(u->offset, s->b.offset, 16);
        std::memcpy(u->cscale, s->b.cscale, 16);
        std::memcpy(u->uv, s->b.uv, 16);
        std::memcpy(u->fog, s->b.fog, 16);
        u->target[0] = rg.ofx;
        u->target[1] = rg.ofy;
        u->target[2] = 1.0f / float(rg.w);
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
            if (A == 0 && B == 1 && D == 1 && C != 1) {
                cmd->set_blend_enable(true);
                blended = true;
                cmd->set_blend_factors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
                cmd->set_blend_op(VK_BLEND_OP_ADD);
            } else if (A == 0 && B == 2 && D == 1 && C != 1) {
                cmd->set_blend_enable(true);
                blended = true;
                cmd->set_blend_factors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE);
                cmd->set_blend_op(VK_BLEND_OP_ADD);
            } else if (A == 1 && B == 0 && D == 1 && C != 1) {   // Cd - (Cs - ... ) : (Cd - Cs) * a + Cd ~ subtract
                cmd->set_blend_enable(true);
                blended = true;
                cmd->set_blend_factors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE);
                cmd->set_blend_op(VK_BLEND_OP_REVERSE_SUBTRACT);
            } else {
                odd_blend++;
            }
        }
        // Depth: ZTE/ZTST from TEST, ZMSK from ZBUF.
        const bool zte = Bits(s->test, 16, 1);
        const uint32_t ztst = Bits(s->test, 17, 2);
        const bool zmsk = Bits(s->zbuf, 32, 1);
        cmd->set_depth_test(true, !zmsk);
        cmd->set_depth_compare(!zte || ztst == 1 ? VK_COMPARE_OP_ALWAYS : ztst == 0 ? VK_COMPARE_OP_NEVER
                               : ztst == 2 ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_GREATER);
        // Motion vectors: written by opaque draws that write colour (translucent layers keep what is under them).
        const bool cw = Bits(s->frame, 32, 32) != 0xffffffffu;
        cmd->set_color_write_mask((cw ? 0xf : 0) | (cw && !blended ? 0x30 : 0));
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
        sc.offset = {sx0 * scale, sy0 * scale};
        sc.extent = {uint32_t(std::max(0, std::min(sx1 + 1, int(rg.w)) - sx0) * scale),
                     uint32_t(std::max(0, std::min(sy1 + 1, int(rg.h)) - sy0) * scale)};
        cmd->set_scissor(sc);
        std::memcpy(cmd->allocate_constant_data(0, 0, sizeof(Ubo)), &uu, sizeof uu);
        if (s->skin) cmd->draw_indexed(n);
        else cmd->draw(n);
    }
    cmd->end_render_pass();
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
    uint32_t nib = 0;
    const uint32_t a = ps2host::gs::Memory::Address(fbp * 32, fbw, x, y, psm, &nib) & 0x3fffff;
    const uint8_t* p = vram + a;
    if (psm == 0x02 || psm == 0x0A) {
        const uint32_t c = uint32_t(p[0]) | uint32_t(p[1]) << 8;
        return ((c & 31) << 3) | (((c >> 5) & 31) << 11) | (((c >> 10) & 31) << 19) | ((c & 0x8000) ? 0x80000000u : 0);
    }
    const uint32_t c = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    return psm == 0x01 ? (c & 0xffffff) | 0x80000000u : c;   // alpha kept: the effect replay reads it (Ad, DATE)
}

uint32_t GsZ(const uint8_t* vram, uint32_t zbp, uint32_t fbw, uint32_t zpsm, uint32_t x, uint32_t y) {
    uint32_t nib = 0;
    const uint32_t psm = 0x30 | (zpsm & 0xf);
    const uint32_t a = ps2host::gs::Memory::Address(zbp * 32, fbw, x, y, psm, &nib) & 0x3fffff;
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
    for (uint32_t y = 0; y < rg.h; ++y)
        for (uint32_t x = 0; x < rg.w; ++x) {
            out[size_t(y) * rg.w + x] = GsPixel(vram, rg.fbp, rg.fbw, rg.fpsm, x, y);
            out[n + size_t(y) * rg.w + x] = GsZ(vram, rg.zbp, rg.fbw, rg.zpsm, x, y);
        }
}

/* GPU timestamps of the live passes, read once Granite has resolved them. */
std::deque<std::array<Vulkan::QueryPoolHandle, 4>> g_ts;
void ProfGpu() {
    const double tick = double(g_dev->get_gpu_properties().limits.timestampPeriod) * 1e-6;
    while (!g_ts.empty() && g_ts.front()[3]->is_signalled()) {
        auto& t = g_ts.front();
        for (int k = 0; k < 3; ++k)
            xsgprof::g_gpu_ms[k] += double(t[k + 1]->get_timestamp_ticks() - t[k]->get_timestamp_ticks()) * tick;
        xsgprof::g_gpu_n++;
        g_ts.pop_front();
    }
    while (g_ts.size() > 64) g_ts.pop_front();
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
    const Region rg = MakeRegion(*list.front());
    // A post-processed frame the replay cannot redo: the paraLLEl-GS frame (1x).
    const char* why = "";
    const xsgfx::Post post = xsgfx::PostState(fbp, &why);
    if (post == xsgfx::Post::Unsupported) {
        static std::string last_why;
        if (last_why != why) {
            Log("vsync %u: post effect not replayable (%s; started by %s): 1x", g_last_vsync, why, xsgfx::PostTrigger(fbp));
            last_why = why;
        }
        g_live_img.reset();
        return;
    }
    // With a post phase the composite starts from the GS frame just before it.
    const uint8_t* vram = xsgfx::PreVram(fbp);
    {
        xsgprof::Scope ps(xsgprof::LiveWait);
        gs.flush();
        if (!vram) vram = static_cast<const uint8_t*>(gs.map_vram_read(0, 4u << 20));
    }
    static std::vector<uint32_t> gsbuf;
    bool own;
    {
        xsgprof::Scope ps(xsgprof::LiveDecode);
        DecodeGs(vram, rg, gsbuf);
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
    struct { int32_t w, h, s, thr; float zmul; int32_t own; } pc = {int32_t(rg.w), int32_t(rg.h), g_scale, g_thr,
                                                                     1.0f / rg.zscale, own ? 1 : 0};
    cmd->push_constants(&pc, 0, sizeof pc);
    cmd->dispatch((ns.w + 7) / 8, (ns.h + 7) / 8, 1);
    cmd->barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                 VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                 VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);
    // Effects after the scenery (native_fx.cpp), then the PMODE circuit blend, at scale g_scale.
    if (post == xsgfx::Post::Replay) xsgfx::Replay(*cmd, fbp, g_scale, rg.w, rg.h, *out, *zimg);
    xsgfx::Pmode(*cmd, gs, g_scale, rg.w, rg.h, *out, *zimg);
    cmd->barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                 VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    // Motion vectors last, so they see the final composite (+ XSG_GPU_MV_VIEW).
    xsgskin::ResolveMv(*cmd, *out, *ns.color, *ns.depth, ns.mv, *gsb, g_scale, 1.0f / rg.zscale, own);
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
        dev.submit(cmd);
    }
    ProfGpu();
    g_live_img = out;
    g_live_w = rg.w;
    g_live_h = rg.h;
    g_live_frames++;
}

void Shot(uint32_t vsync, ParallelGS::GSInterface& gs) {
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
        std::vector<uint32_t> down(gsc.size());
        uint64_t hl[4] = {};
        for (uint32_t y = 0; y < rg.h; ++y)
            for (uint32_t x = 0; x < rg.w; ++x) {
                uint32_t acc[3] = {};
                for (int j = 0; j < s; ++j)
                    for (int i = 0; i < s; ++i) {
                        const uint32_t c = live[size_t(y * s + j) * rg.w * s + x * s + i];
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

}  // namespace

bool Init(Vulkan::Device* dev) {
    const char* e = std::getenv("XSG_GPU_NATIVE");
    g_on = e && *e && std::strcmp(e, "0") != 0;
    if (!g_on) return false;
    g_dev = dev;
    xsgprof::Init();
    if (const char* s = std::getenv("XSG_GPU_NATIVE_SCALE")) g_scale = std::max(1, std::min(8, std::atoi(s)));
    if (const char* s = std::getenv("XSG_GPU_NATIVE_LIVE"); s && *s) g_live = std::atoi(s) != 0;
    if (const char* s = std::getenv("XSG_GPU_NATIVE_THR"); s && *s) g_thr = std::atoi(s);
    if (const char* s = std::getenv("XSG_GPU_NATIVE_DIR"); s && *s) g_dir = s;
    if (const char* s = std::getenv("XSG_GPU_NATIVE_SHOT"))
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
    fl.output_mask = 3;
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
    if (!xsgskin::Init(dev)) return false;
    xsgfx::Init(dev);
    Log("on: set-0 strip batches, scale %dx, live %s, %zu shot vsyncs -> %s", g_scale, g_live ? "on" : "off",
        g_shots.size(), g_dir.c_str());
    return g_prog != nullptr && g_comp != nullptr;
}

bool On() { return g_on; }

void Gif(uint32_t path, const uint8_t* data, uint32_t len, ParallelGS::GSInterface& gs) {
    PathParse& p = g_path[path & 3];
    {
        xsgprof::Scope ps(xsgprof::GifParse);
        for (uint32_t i = 0; i + 16 <= len; i += 16) GifQword(p, data + i);
    }
    xsgfx::Gif(path, data, len, gs);
}

namespace {
/* The GS state a batch draws with (PRIM from its GIF tag, the registers of that
   context), and its texture. */
void Snap(Stored& s, const uint32_t* tagw) {
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
            auto mix = [&](uint32_t page) {
                const uint64_t* q = reinterpret_cast<const uint64_t*>(g_mem.data() + size_t(page & 511) * 8192);
                for (int i = 0; i < 1024; ++i) h = (h ^ q[i]) * 1099511628211ull;
            };
            for (uint32_t i = 0; i < np; ++i) mix(p0 + i);
            if (Bits(t0, 20, 6) >= 0x13) { mix(cpage); mix(cpage + 1); }   // CLUT formats
            vv.gen = gen;
            vv.hash = h | 1;
        }
        auto key = std::make_tuple(t0, s.texa, vv.hash);
        auto it = cache.find(key);
        if (it == cache.end()) {
            auto t = std::make_shared<Tex>();
            xsgprof::Scope ps(xsgprof::TexDecode);
            t->px = DecodeTexture(g_mem.data(), s.tex0, s.texa, &t->w, &t->h);
            it = cache.emplace(key, t).first;
            g_ntex++;
        }
        s.tex = it->second;
    }
}
void Push(Stored&& s) {
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
    xsgfx::NativeFbp(Bits(g_reg[R_FRAME_1 + Bits(pr, 9, 1)], 0, 9));
}
}  // namespace

void Batch(uint32_t kind, uint32_t nvert, const uint8_t* payload, uint32_t len) {
    xsgprof::Scope ps(xsgprof::Batch);
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
    if (kind == GPU_NATIVE_SET2 || kind == GPU_NATIVE_SET2_PALETTE) {
        auto d = xsgskin::Message(kind, nvert, payload, len);
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

void Vsync(uint32_t frame, ParallelGS::GSInterface& gs) {
    g_last_vsync = frame;
    {
        xsgprof::Scope ps(xsgprof::FxVsync);
        xsgfx::Vsync(frame, gs);   // first: it closes the post phase of the frame just drawn
    }
    {
        xsgprof::Scope ps(xsgprof::Other);
        xsgskin::Vsync(frame);
    }
    if (g_live) Live(gs);
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

const Vulkan::ImageHandle& LiveImage(uint32_t* fb_w, uint32_t* fb_h) {
    *fb_w = g_live_w;
    *fb_h = g_live_h;
    return g_live_img;
}

int Scale() { return g_scale; }

std::vector<uint32_t> DecodeMirrorTexture(uint64_t tex0, uint64_t texa, uint32_t* w, uint32_t* h) {
    return DecodeTexture(g_mem.data(), tex0, texa, w, h);
}
uint32_t MirrorGen() { return g_upload_gen; }

}  // namespace xsgnative

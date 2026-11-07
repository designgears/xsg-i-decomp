/*
 * xsg-gpu native renderer, first slice: set-0 strip batches (VU1 loop 0x0c18)
 * drawn by our own Vulkan pipeline.  See native.h and
 * port/docs/NATIVE_RENDERER.md.
 */
#include "native.h"

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
    for (int i = 0; i < 16 && t.on; ++i) {
        t.pend |= uint64_t(q[i]) << t.pend_bits;
        t.pend_bits += 8;
        while (t.pend_bits >= bpp && t.on) {
            g_mem.Write(t.dbp, t.dbw, (t.x0 + t.x) & 2047, (t.y0 + t.y) & 2047, t.psm, uint32_t(t.pend & ((1ull << bpp) - 1)));
            if (++t.x >= t.w) { t.x = 0; if (++t.y >= t.h) t.on = false; }
            t.pend >>= bpp;
            t.pend_bits -= bpp;
        }
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
        if (t.on) g_upload_gen++;
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
    if (p.flg >= 2) { XferQword(q); return; }   // IMAGE
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
    uint64_t prim, tex0, clamp, tex1, xyoff, texa, fogcol, scissor, alpha, test, frame, zbuf;
};
std::vector<Stored> g_batches, g_prev;   // this vsync's batches; the last vsync that had any

/* ---------------------------------------------------------------- Vulkan */
Vulkan::Device* g_dev;
Vulkan::Program* g_prog;
bool g_on;
int g_scale = 2;
std::set<uint32_t> g_shots;
std::string g_dir = "/tmp/xsg_native";
uint64_t g_nbatch, g_nvert, g_ntex;
uint32_t g_last_vsync;
/* Batches are kept (and textured) only in the few vsyncs before a shot. */
bool Near() {
    auto it = g_shots.upper_bound(g_last_vsync);
    return it != g_shots.end() && *it <= g_last_vsync + 3;
}

struct Ubo {
    float m[4][4];
    float scale[4], offset[4], cscale[4], uv[4], fog[4];
    float target[4], zscale[4], fogcol[4];
    int32_t mode[4];
    float fix[4];
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

struct Region {
    uint32_t fbp, fbw, fpsm, zbp, zpsm, w, h;
    float ofx, ofy, zscale;
};

/* Draw the batches drawn into `rg` at `scale`, read color and depth back. */
Target Render(const std::vector<const Stored*>& list, const Region& rg, int scale, const uint8_t* vram,
              std::map<const Tex*, Vulkan::ImageHandle>& texcache) {
    Target t;
    t.w = rg.w * scale;
    t.h = rg.h * scale;
    auto& dev = *g_dev;
    auto ci = Vulkan::ImageCreateInfo::render_target(t.w, t.h, VK_FORMAT_R8G8B8A8_UNORM);
    ci.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    auto color = dev.create_image(ci);
    auto di = Vulkan::ImageCreateInfo::render_target(t.w, t.h, VK_FORMAT_D32_SFLOAT);
    di.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    auto depth = dev.create_image(di);

    auto cmd = dev.request_command_buffer();
    cmd->image_barrier(*color, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    cmd->image_barrier(*depth, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                       VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    Vulkan::RenderPassInfo rp;
    rp.num_color_attachments = 1;
    rp.color_attachments[0] = &color->get_view();
    rp.depth_stencil = &depth->get_view();
    rp.clear_attachments = 1;
    rp.store_attachments = 1;
    rp.op_flags = Vulkan::RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT | Vulkan::RENDER_PASS_OP_STORE_DEPTH_STENCIL_BIT;
    rp.clear_depth_stencil.depth = 0.0f;
    cmd->begin_render_pass(rp);
    cmd->set_opaque_state();
    cmd->set_program(g_prog);
    cmd->set_cull_mode(VK_CULL_MODE_NONE);
    cmd->set_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    unsigned drawn = 0, blended = 0, odd_blend = 0;
    for (const Stored* s : list) {
        const uint32_t n = uint32_t(s->v.size());
        auto* vp = static_cast<GpuNativeVertex*>(cmd->allocate_vertex_data(0, n * sizeof(GpuNativeVertex), sizeof(GpuNativeVertex)));
        std::memcpy(vp, s->v.data(), n * sizeof(GpuNativeVertex));
        cmd->set_vertex_attrib(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuNativeVertex, p));
        cmd->set_vertex_attrib(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuNativeVertex, c));
        cmd->set_vertex_attrib(2, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuNativeVertex, t));
        auto* u = static_cast<Ubo*>(cmd->allocate_constant_data(0, 0, sizeof(Ubo)));
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
        if (abe) {
            blended++;
            if (A == 0 && B == 1 && D == 1 && C != 1) {
                cmd->set_blend_enable(true);
                cmd->set_blend_factors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
                cmd->set_blend_op(VK_BLEND_OP_ADD);
            } else if (A == 0 && B == 2 && D == 1 && C != 1) {
                cmd->set_blend_enable(true);
                cmd->set_blend_factors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE);
                cmd->set_blend_op(VK_BLEND_OP_ADD);
            } else if (A == 1 && B == 0 && D == 1 && C != 1) {   // Cd - (Cs - ... ) : (Cd - Cs) * a + Cd ~ subtract
                cmd->set_blend_enable(true);
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
        cmd->set_depth_test(zte, !zmsk);
        cmd->set_depth_compare(!zte || ztst == 1 ? VK_COMPARE_OP_ALWAYS : ztst == 0 ? VK_COMPARE_OP_NEVER
                               : ztst == 2 ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_GREATER);
        if (!zte) cmd->set_depth_test(true, !zmsk), cmd->set_depth_compare(VK_COMPARE_OP_ALWAYS);
        cmd->set_color_write_mask(Bits(s->frame, 32, 32) == 0xffffffffu ? 0 : 0xf);
        // Texture.
        auto it = texcache.find(s->tex.get());
        if (it == texcache.end()) {
            auto ti = Vulkan::ImageCreateInfo::immutable_2d_image(s->tex->w, s->tex->h, VK_FORMAT_R8G8B8A8_UNORM);
            Vulkan::ImageInitialData init = {s->tex->px.data(), 0, 0};
            it = texcache.emplace(s->tex.get(), dev.create_image(ti, &init)).first;
        }
        const bool linear = Bits(s->tex1, 5, 1);
        const bool clampu = Bits(s->clamp, 0, 2) != 0;
        cmd->set_texture(0, 1, it->second->get_view(),
                         linear ? (clampu ? Vulkan::StockSampler::LinearClamp : Vulkan::StockSampler::LinearWrap)
                                : (clampu ? Vulkan::StockSampler::NearestClamp : Vulkan::StockSampler::NearestWrap));
        // GS scissor, in this target's pixels.
        const int sx0 = int(Bits(s->scissor, 0, 11)), sx1 = int(Bits(s->scissor, 16, 11));
        const int sy0 = int(Bits(s->scissor, 32, 11)), sy1 = int(Bits(s->scissor, 48, 11));
        VkRect2D sc;
        sc.offset = {sx0 * scale, sy0 * scale};
        sc.extent = {uint32_t(std::max(0, std::min(sx1 + 1, int(rg.w)) - sx0) * scale),
                     uint32_t(std::max(0, std::min(sy1 + 1, int(rg.h)) - sy0) * scale)};
        cmd->set_scissor(sc);
        cmd->draw(n);
        drawn++;
    }
    cmd->end_render_pass();
    cmd->image_barrier(*color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    cmd->image_barrier(*depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    Vulkan::BufferCreateInfo bi = {};
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.domain = Vulkan::BufferDomain::CachedHost;
    bi.size = VkDeviceSize(t.w) * t.h * 4;
    auto cbuf = dev.create_buffer(bi);
    auto dbuf = dev.create_buffer(bi);
    cmd->copy_image_to_buffer(*cbuf, *color, 0, {}, {t.w, t.h, 1}, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1});
    cmd->copy_image_to_buffer(*dbuf, *depth, 0, {}, {t.w, t.h, 1}, 0, 0, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1});
    cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                 VK_ACCESS_HOST_READ_BIT);
    Vulkan::Fence fence;
    dev.submit(cmd, &fence);
    fence->wait();
    t.color.resize(size_t(t.w) * t.h);
    t.depth.resize(size_t(t.w) * t.h);
    std::memcpy(t.color.data(), dev.map_host_buffer(*cbuf, Vulkan::MEMORY_ACCESS_READ_BIT), t.color.size() * 4);
    std::memcpy(t.depth.data(), dev.map_host_buffer(*dbuf, Vulkan::MEMORY_ACCESS_READ_BIT), t.depth.size() * 4);
    Log("  %ux: %u batches drawn (%u blended, %u with an unsupported blend drawn opaque)", unsigned(scale), drawn,
        blended, odd_blend);
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
        return ((c & 31) << 3) | (((c >> 5) & 31) << 11) | (((c >> 10) & 31) << 19);
    }
    return (uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16) & 0xffffff;
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

void Shot(uint32_t vsync, ParallelGS::GSInterface& gs) {
    if (g_batches.empty()) g_batches.swap(g_prev);   // 30 fps: the frame was drawn before the last vsync
    if (g_batches.empty()) { Log("vsync %u: no native batches", vsync); return; }
    // The buffer most batches went to.
    std::map<uint32_t, unsigned> fbps;
    for (auto& s : g_batches) fbps[Bits(s.frame, 0, 9)]++;
    uint32_t fbp = 0;
    unsigned best = 0;
    for (auto& [k, c] : fbps) if (c > best) { best = c; fbp = k; }
    std::vector<const Stored*> list;
    for (auto& s : g_batches) if (Bits(s.frame, 0, 9) == fbp) list.push_back(&s);
    const Stored& f = *list.front();
    Region rg;
    rg.fbp = fbp;
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
    Log("vsync %u: %zu batches (%zu to FBP 0x%x of %zu buffers), %ux%u, FBW %u PSM 0x%x, ZBP 0x%x ZPSM 0x%x, "
        "XYOFFSET (%.1f, %.1f)",
        vsync, g_batches.size(), list.size(), fbp, fbps.size(), rg.w, rg.h, rg.fbw, rg.fpsm, rg.zbp, zpsm, rg.ofx,
        rg.ofy);

    gs.flush();
    std::vector<uint8_t> vram(4u << 20);
    std::memcpy(vram.data(), gs.map_vram_read(0, 4u << 20), vram.size());
    std::map<const Tex*, Vulkan::ImageHandle> texcache;
    Target n1 = Render(list, rg, 1, vram.data(), texcache);
    Target ns = g_scale > 1 ? Render(list, rg, g_scale, vram.data(), texcache) : n1;
    Log("  %zu textures (%llu decoded so far)", texcache.size(), (unsigned long long)g_ntex);

    // The paraLLEl-GS frame and depth at 1x.
    std::vector<uint32_t> gsc(size_t(rg.w) * rg.h);
    std::vector<uint32_t> gsz(gsc.size());
    for (uint32_t y = 0; y < rg.h; ++y)
        for (uint32_t x = 0; x < rg.w; ++x) {
            gsc[size_t(y) * rg.w + x] = GsPixel(vram.data(), rg.fbp, rg.fbw, rg.fpsm, x, y) | 0xff000000u;
            gsz[size_t(y) * rg.w + x] = GsZ(vram.data(), rg.zbp, rg.fbw, rg.zpsm, x, y);
        }
    const float zmul = 1.0f / rg.zscale;
    // Composite: a native sample replaces the GS pixel under it when it is not
    // behind the GS depth there, and (where the native 1x render covers that
    // pixel) the GS kept the colour the native pass predicts - so 2D, effects
    // and anything else drawn over the scenery stay.
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
                    if (ok && n1.depth[gi] > 0.0f) ok = ChanDiff(n1.color[gi], gsc[gi]) <= 24;
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
    WritePng(path("native_only_1x"), n1.w, n1.h, n1.color);
    uint64_t cov1 = 0, use1 = 0;
    auto c1 = composite(n1, 1, &cov1, &use1);
    WritePng(path("comp_1x"), n1.w, n1.h, c1);
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
    Log("  images: %s/native_%06u_*.png", g_dir.c_str(), vsync);
}

}  // namespace

bool Init(Vulkan::Device* dev) {
    const char* e = std::getenv("XSG_GPU_NATIVE");
    g_on = e && *e && std::strcmp(e, "0") != 0;
    if (!g_on) return false;
    g_dev = dev;
    if (const char* s = std::getenv("XSG_GPU_NATIVE_SCALE")) g_scale = std::max(1, std::min(8, std::atoi(s)));
    if (const char* s = std::getenv("XSG_GPU_NATIVE_DIR"); s && *s) g_dir = s;
    if (const char* s = std::getenv("XSG_GPU_NATIVE_SHOT"))
        for (const char* q = s; *q;) {
            char* end;
            unsigned long v = std::strtoul(q, &end, 0);
            if (end == q) break;
            g_shots.insert(uint32_t(v));
            q = *end ? end + 1 : end;
        }
    Vulkan::ResourceLayout vl, fl;
    vl.input_mask = 7;
    vl.sets[0].uniform_buffer_mask = 1;
    vl.sets[0].meta[0].array_size = 1;
    fl.output_mask = 1;
    fl.sets[0].uniform_buffer_mask = 1;
    fl.sets[0].sampled_image_mask = 2;
    fl.sets[0].meta[0].array_size = 1;
    fl.sets[0].meta[1].array_size = 1;
    auto* vs = dev->request_shader(kVert, sizeof kVert, &vl);
    auto* fs = dev->request_shader(kFrag, sizeof kFrag, &fl);
    g_prog = dev->request_program(vs, fs);
    Log("on: set-0 strip batches, scale %dx, %zu shot vsyncs -> %s", g_scale, g_shots.size(), g_dir.c_str());
    return g_prog != nullptr;
}

bool On() { return g_on; }

void Gif(uint32_t path, const uint8_t* data, uint32_t len) {
    PathParse& p = g_path[path & 3];
    for (uint32_t i = 0; i + 16 <= len; i += 16) GifQword(p, data + i);
}

void Batch(uint32_t kind, uint32_t nvert, const uint8_t* payload, uint32_t len) {
    if (kind != GPU_NATIVE_SET0_STRIP || len < sizeof(GpuNativeBatch) + nvert * sizeof(GpuNativeVertex)) return;
    g_nbatch++;
    g_nvert += nvert;
    if (!Near()) return;
    Stored s;
    std::memcpy(&s.b, payload, sizeof s.b);
    s.v.resize(nvert);
    std::memcpy(s.v.data(), payload + sizeof s.b, nvert * sizeof(GpuNativeVertex));
    uint64_t tag = uint64_t(s.b.tag[0]) | uint64_t(s.b.tag[1]) << 32;
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
        static std::map<std::tuple<uint64_t, uint64_t, uint32_t>, std::shared_ptr<Tex>> cache;
        if (cache.size() > 4096) cache.clear();
        auto key = std::make_tuple(s.tex0 & ((1ull << 61) - 1), s.texa, g_upload_gen);
        auto it = cache.find(key);
        if (it == cache.end()) {
            auto t = std::make_shared<Tex>();
            t->px = DecodeTexture(g_mem.data(), s.tex0, s.texa, &t->w, &t->h);
            it = cache.emplace(key, t).first;
            g_ntex++;
        }
        s.tex = it->second;
    }
    g_batches.push_back(std::move(s));
}

void Vsync(uint32_t frame, ParallelGS::GSInterface& gs) {
    if (g_shots.count(frame)) Shot(frame, gs);
    if (frame % 600 == 0)
        Log("vsync %u: %llu batches, %llu vertices so far", frame, (unsigned long long)g_nbatch,
            (unsigned long long)g_nvert);
    if (!g_batches.empty()) g_prev.swap(g_batches);
    g_batches.clear();
    g_last_vsync = frame;
}

}  // namespace xsgnative

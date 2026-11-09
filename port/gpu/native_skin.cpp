/*
 * xsg-gpu native renderer: set-2 characters (GPU skinning) and motion vectors.
 * See native_skin.h and port/docs/NATIVE_RENDERER.md §7-8.
 */
#include "native_skin.h"
#include "menu/cfg.h"

#include "device.hpp"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace xsgskin {
namespace {

const uint32_t kVert[] = {
#include "native_skin.vert.inc"
};
const uint32_t kFrag[] = {
#include "native_ppl.frag.inc"   // native.frag with SKIN_PPL: per-pixel lighting
};
const uint32_t kMv[] = {
#include "native_mv.comp.inc"
};

Vulkan::Device* g_dev;
Vulkan::Program* g_prog;
Vulkan::Program* g_mvprog;
Vulkan::ImageHandle g_mv, g_rawmv;
bool g_view;
float g_view_full = 16.0f;
/* GPU rings, host-visible, written once when a message arrives: bone palettes
   (slot = serial % kPalSlots) and skin vertices + indices. */
constexpr uint32_t kPalSlots = 16384;
constexpr uint64_t kArena = 256ull << 20;
Vulkan::BufferHandle g_palbuf, g_arenabuf;
uint8_t* g_palmap;
uint8_t* g_arenamap;
uint64_t g_pal;         // serial of the current palette (0 = none yet)
uint64_t g_arena_head;  // bytes ever allocated (offset = head % kArena)
uint64_t Alloc(uint64_t n) {
    uint64_t at = (g_arena_head + 15) & ~15ull;
    if (at % kArena + n > kArena) at = (at / kArena + 1) * kArena;
    g_arena_head = at + n;
    return at;
}
bool PalLive(uint64_t serial) { return serial && g_pal - serial < kPalSlots - 1024; }
bool ArenaLive(uint64_t at) { return g_arena_head - at < kArena - (16ull << 20); }
uint64_t g_nbatch, g_nvert, g_ntri, g_nculled, g_nbadbone, g_nprev, g_nset0prev, g_nset0, g_nset0lit;

/* Motion history per draw key (VIF address of the batch's MSCNT, occurrence). */
struct Hist {
    uint32_t frame = 0;
    float m[16];
    std::vector<float> p;
    uint64_t pal = 0;
    float mirror = 0;
    bool mirror_on = false;
};
std::unordered_map<uint64_t, Hist> g_hist2, g_hist0;
constexpr uint32_t kMaxAge = 6;   // vblanks: older history is a "new" draw (zero motion)

inline uint64_t Key(uint32_t key, uint32_t occ) { return uint64_t(key) << 32 | occ; }

struct GpuVertex {
    float p[4], n[4], c[4];
    uint32_t b[4];
    float w[4], pp[4];
};

struct SkinUbo {
    float m[4][4], scale[4], offset[4], lmat[3][4], lcol[4][4], diffuse[4], env[4], eye[4], c970[4], uvo[4],
        c1008[4], mirror[4], e1004[3][4], c1009[4];
    int32_t cfg[4], cfg2[4], cfg3[4];
    float pm[4][4], pmirror[4];
    float fog[4];   // set-0 lit (group 4): per-vertex fog clamp(z + w q, x, y)
};


/* The draw's constants (native_skin.vert binding 2). */
void BuildUbo(const Draw& d, SkinUbo* u) {
    const bool prev = d.prev && (d.group == 4 || PalLive(d.ppal));
    std::memset(u, 0, sizeof *u);
    const GpuNativeSkin& b = d.b;
    std::memcpy(u->m, b.m, 64);
    std::memcpy(u->scale, b.scale, 16);
    std::memcpy(u->offset, b.offset, 16);
    std::memcpy(u->lmat, b.lmat, 48);
    std::memcpy(u->lcol, b.lcol, 64);
    std::memcpy(u->diffuse, b.diffuse, 16);
    std::memcpy(u->env, b.env, 16);
    std::memcpy(u->eye, b.eye, 16);
    std::memcpy(u->c970, b.c970, 16);
    std::memcpy(u->uvo, b.uv, 16);
    std::memcpy(u->c1008, b.c1008, 16);
    std::memcpy(u->mirror, b.mirror, 16);
    std::memcpy(u->e1004, b.e1004, 48);
    for (int l = 0; l < 4; ++l) u->c1009[l] = float(int32_t(b.c1009[l]));
    uint32_t c1008x;
    std::memcpy(&c1008x, &b.c1008[0], 4);
    uint32_t c1008z;
    std::memcpy(&c1008z, &b.c1008[2], 4);
    u->cfg[0] = d.group;
    u->cfg[1] = d.variant;
    {   // read once per settings change, not per draw (the lookup was ~4% of xsg-gpu in battle)
        static unsigned ver = ~0u;
        static int ppl;
        if (ver != xsgcfg::Version()) {
            ver = xsgcfg::Version();
            ppl = xsgcfg::Get("renderer.lighting") == "perpixel";
        }
        u->cfg[2] = ppl;
    }
    u->cfg[3] = ((c1008z & 0xffff) ? 1 : 0) | (b.mirror_on ? 2 : 0) | (prev ? 4 : 0) | (prev && d.pmirror_on ? 8 : 0) |
                (d.group == 4 ? 16 : 0);
    std::memcpy(u->fog, b.fog, 16);
    u->cfg2[0] = int32_t(std::min<uint32_t>(c1008x & 0xffff, 16));
    u->cfg2[1] = int32_t(b.ifirst);
    u->cfg2[2] = int32_t((b.fogw >> 4) & 0xff);
    u->cfg2[3] = int32_t((d.pal % kPalSlots) * GPU_SKIN_PAL_NQW);
    u->cfg3[0] = int32_t(((prev ? d.ppal : d.pal) % kPalSlots) * GPU_SKIN_PAL_NQW);
    if (prev) std::memcpy(u->pm, d.pm, 64);
    u->pmirror[2] = d.pmirror;
}
/* ... written once into the arena. */
void FillUbo(const Draw& d) {
    SkinUbo u;
    BuildUbo(d, &u);
    std::memcpy(g_arenamap + d.uofs % kArena, &u, sizeof u);
}

}  // namespace

int Variant(uint32_t loop) {
    switch (loop) {
    case 0x05f0: case 0x17d0: case 0x2890: return 0;
    case 0x0920: case 0x1ac8: case 0x2c28: return 1;
    case 0x0da0: case 0x1f00: case 0x3098: return 2;
    case 0x1268: case 0x2378: case 0x3550: return 3;
    }
    return -1;
}

/* Set 0 (GPU_NATIVE_SET0_LIT): 0x1458 lit, 0x1fa0 lit + specular + env strip. */
static int Set0Variant(uint32_t loop) { return loop == 0x1458 ? 0 : loop == 0x1fa0 ? 3 : -1; }

static int Group(uint32_t loop) {
    if (loop < 0x1790) return 0;
    if (loop < 0x2850) return 1;
    return loop == 0x2890 ? 2 : 3;
}

bool Init(Vulkan::Device* dev) {
    g_dev = dev;
    Vulkan::ResourceLayout vl, fl, cl;
    vl.input_mask = 0x3f;
    vl.sets[0].uniform_buffer_mask = 1 | 4;
    vl.sets[0].storage_buffer_mask = 8;
    for (int i = 0; i < 4; ++i) vl.sets[0].meta[i].array_size = 1;
    fl.output_mask = 3;
    fl.sets[0].uniform_buffer_mask = 1 | 4;   // + SkinConst (per-pixel lighting)
    fl.sets[0].sampled_image_mask = 2;
    fl.sets[0].meta[0].array_size = 1;
    fl.sets[0].meta[1].array_size = 1;
    fl.sets[0].meta[2].array_size = 1;
    g_prog = dev->request_program(dev->request_shader(kVert, sizeof kVert, &vl), dev->request_shader(kFrag, sizeof kFrag, &fl));
    cl.sets[0].sampled_image_mask = 7;
    cl.sets[0].storage_image_mask = 8 | 16;
    cl.sets[0].storage_buffer_mask = 32;
    for (int i = 0; i < 6; ++i) cl.sets[0].meta[i].array_size = 1;
    cl.push_constant_size = 36;
    g_mvprog = dev->request_program(dev->request_shader(kMv, sizeof kMv, &cl));
    Vulkan::BufferCreateInfo bi = {};
    bi.domain = Vulkan::BufferDomain::LinkedDeviceHost;
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bi.size = VkDeviceSize(kPalSlots) * GPU_SKIN_PAL_NQW * 16;
    g_palbuf = dev->create_buffer(bi);
    bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bi.size = kArena;
    g_arenabuf = dev->create_buffer(bi);
    if (!g_palbuf || !g_arenabuf) return false;
    g_palmap = static_cast<uint8_t*>(dev->map_host_buffer(*g_palbuf, Vulkan::MEMORY_ACCESS_WRITE_BIT));
    g_arenamap = static_cast<uint8_t*>(dev->map_host_buffer(*g_arenabuf, Vulkan::MEMORY_ACCESS_WRITE_BIT));
    if (const char* s = xsgcfg::CStr("debug.mv_view"); s && *s) {
        g_view = std::atof(s) != 0.0;
        if (std::atof(s) > 1.0) g_view_full = float(std::atof(s));
    }
    std::fprintf(stderr, "[native] set-2 characters on; motion vectors on%s\n", g_view ? " (XSG_GPU_MV_VIEW)" : "");
    return g_prog && g_mvprog;
}

KeyLight g_light;
const KeyLight& Light() { return g_light; }

std::shared_ptr<const Draw> Message(uint32_t kind, uint32_t nvert, const uint8_t* payload, uint32_t len) {
    if (kind == GPU_NATIVE_SET2_PALETTE) {
        if (len < GPU_SKIN_PAL_NQW * 16) return nullptr;
        ++g_pal;
        std::memcpy(g_palmap + (g_pal % kPalSlots) * GPU_SKIN_PAL_NQW * 16, payload, GPU_SKIN_PAL_NQW * 16);
        return nullptr;
    }
    if ((kind != GPU_NATIVE_SET2 && kind != GPU_NATIVE_SET0_LIT) || len < sizeof(GpuNativeSkin)) return nullptr;
    const bool set0 = kind == GPU_NATIVE_SET0_LIT;
    auto d = std::make_shared<Draw>();
    std::memcpy(&d->b, payload, sizeof d->b);
    const uint32_t n = d->b.n;
    const int variant = set0 ? Set0Variant(d->b.loop) : Variant(d->b.loop);
    if (n != nvert || len < sizeof(GpuNativeSkin) + n * sizeof(GpuNativeSkinVertex) + (n + 31) / 32 * 4 || variant < 0)
        return nullptr;
    d->variant = variant;
    {   // the key light (KeyLight): light k's object-space direction is (lmat[0][k], lmat[1][k], lmat[2][k]),
        // its colour lcol[k]; the brightest one, through this batch's object -> clip matrix
        const GpuNativeSkin& b = d->b;
        int best = -1;
        float bl = 0.0f;
        for (int k = 0; k < 3; ++k) {
            const float l = b.lcol[k][0] + b.lcol[k][1] + b.lcol[k][2];
            const float len2 = b.lmat[0][k] * b.lmat[0][k] + b.lmat[1][k] * b.lmat[1][k] + b.lmat[2][k] * b.lmat[2][k];
            if (l > bl && len2 > 1e-8f && std::isfinite(l)) { bl = l; best = k; }
        }
        if (best >= 0) {
            const float L[3] = {b.lmat[0][best], b.lmat[1][best], b.lmat[2][best]};
            for (int c = 0; c < 4; ++c) g_light.dir[c] = b.m[0][c] * L[0] + b.m[1][c] * L[1] + b.m[2][c] * L[2];
            std::memcpy(g_light.scale, b.scale, 16);
            std::memcpy(g_light.offset, b.offset, 16);
            g_light.frame = b.frame;
            g_light.valid = std::isfinite(g_light.dir[0] + g_light.dir[1] + g_light.dir[2] + g_light.dir[3]);
        }
    }
    d->group = set0 ? 4 : Group(d->b.loop);   // 4: no bones (P/N in object space)
    if (set0) g_nset0lit++;
    // The vertices are read in place (the ring payload), written once into the arena below.
    const GpuNativeSkinVertex* sv = reinterpret_cast<const GpuNativeSkinVertex*>(payload + sizeof d->b);
    const uint32_t* adc = reinterpret_cast<const uint32_t*>(payload + sizeof d->b + n * sizeof(GpuNativeSkinVertex));
    d->pal = g_pal;
    static std::vector<uint16_t> idx;
    idx.clear();
    for (uint32_t i = 2; i < n; ++i) {
        if (adc[i >> 5] >> (i & 31) & 1) { g_nculled++; continue; }
        idx.push_back(uint16_t(i - 2));
        idx.push_back(uint16_t(i - 1));
        idx.push_back(uint16_t(i));
    }
    d->nidx = uint32_t(idx.size());
    g_nbatch++;
    g_nvert += n;
    g_ntri += d->nidx / 3;
    // Bones outside the palette with a non-zero weight would be dropped by the shader.
    for (uint32_t i = 0; i < n; ++i) {
        const GpuNativeSkinVertex& v = sv[i];
        auto bad = [&](uint32_t a, float w) {
            const int i = int(a & 0x3ff) - int(GPU_SKIN_PAL_QW0);
            if (w != 0.0f && (i < 0 || i + 3 >= int(GPU_SKIN_PAL_NQW))) g_nbadbone++;
        };
        if (d->group == 4) break;
        if (d->group == 1) bad(v.b[0], 1.0f);
        else if (d->group == 0) {
            for (int k = 0; k < 4; ++k) {
                bad(v.b[k], v.w[k]);
                if (k < 3 && !(v.b[k + 1] & 0xffff)) break;
            }
        } else {
            float w0, w1;
            std::memcpy(&w0, &v.b[2], 4);
            std::memcpy(&w1, &v.b[3], 4);
            bad(v.b[0], w0);
            if (v.b[1] & 0xffff) bad(v.b[1], w1);
        }
    }
    // Motion history.
    Hist& h = g_hist2[Key(d->b.key, d->b.occ)];
    const float* pp = nullptr;
    if (h.frame && h.frame != d->b.frame && d->b.frame - h.frame <= kMaxAge &&
        (set0 || (PalLive(h.pal) && d->pal)) && h.p.size() == size_t(n) * 4) {
        d->prev = true;
        std::memcpy(d->pm, h.m, sizeof d->pm);
        pp = h.p.data();
        d->ppal = h.pal;
        d->pmirror = h.mirror;
        d->pmirror_on = h.mirror_on;
        g_nprev++;
    }
    if (d->nidx) {
        d->vofs = Alloc(n * sizeof(GpuVertex));
        d->iofs = Alloc(d->nidx * 2);
        GpuVertex* vp = reinterpret_cast<GpuVertex*>(g_arenamap + d->vofs % kArena);
        for (uint32_t i = 0; i < n; ++i) {
            const GpuNativeSkinVertex& s = sv[i];
            GpuVertex g;
            std::memcpy(g.p, s.p, 16);
            std::memcpy(g.n, s.n, 16);
            std::memcpy(g.c, s.c, 16);
            std::memcpy(g.b, s.b, 16);
            std::memcpy(g.w, s.w, 16);
            std::memcpy(g.pp, pp ? pp + size_t(i) * 4 : s.p, 16);
            std::memcpy(&vp[i], &g, sizeof g);
        }
        std::memcpy(g_arenamap + d->iofs % kArena, idx.data(), d->nidx * 2);
        d->uofs = (Alloc(sizeof(SkinUbo) + 256) + 255) & ~255ull;
        FillUbo(*d);
    }
    if (h.frame != d->b.frame) {
        h.frame = d->b.frame;
        std::memcpy(h.m, d->b.m, sizeof h.m);
        h.p.resize(size_t(n) * 4);
        for (uint32_t i = 0; i < n; ++i) std::memcpy(&h.p[size_t(i) * 4], sv[i].p, 16);
        h.pal = d->pal;
        std::memcpy(&h.mirror, &d->b.mirror[2], 4);
        h.mirror_on = d->b.mirror_on != 0;
    }
    return d;
}

bool Set0Prev(const GpuNativeBatch& b, const GpuNativeVertex* v, uint32_t n, GpuNativeVertex* out, float mprev[16]) {
    g_nset0++;
    Hist& h = g_hist0[Key(b.key, b.occ)];
    bool ok = false;
    if (h.frame && h.frame != b.frame && b.frame - h.frame <= kMaxAge && h.p.size() == size_t(n) * 3) {
        ok = true;
        g_nset0prev++;
        if (out) {
            std::memcpy(mprev, h.m, 64);
            for (uint32_t i = 0; i < n; ++i) std::memcpy(out[i].pad, &h.p[size_t(i) * 3], 12);
        }
    }
    if (h.frame != b.frame) {
        h.frame = b.frame;
        std::memcpy(h.m, b.m, 64);
        h.p.resize(size_t(n) * 3);
        for (uint32_t i = 0; i < n; ++i) std::memcpy(&h.p[size_t(i) * 3], v[i].p, 12);
    }
    return ok;
}

void Begin() {}

uint32_t Bind(Vulkan::CommandBuffer& cmd, const Draw& d, int pass) {
    const uint32_t ni = d.nidx;
    cmd.set_program(g_prog);
    cmd.set_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    if (!ni || (d.group != 4 && !PalLive(d.pal)) || !ArenaLive(d.vofs)) return 0;
    cmd.set_vertex_binding(0, *g_arenabuf, d.vofs % kArena, sizeof(GpuVertex));
    cmd.set_index_buffer(*g_arenabuf, d.iofs % kArena, VK_INDEX_TYPE_UINT16);
    cmd.set_storage_buffer(0, 3, *g_palbuf);
    cmd.set_vertex_attrib(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuVertex, p));
    cmd.set_vertex_attrib(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuVertex, n));
    cmd.set_vertex_attrib(2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuVertex, c));
    cmd.set_vertex_attrib(3, 0, VK_FORMAT_R32G32B32A32_UINT, offsetof(GpuVertex, b));
    cmd.set_vertex_attrib(4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuVertex, w));
    cmd.set_vertex_attrib(5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuVertex, pp));
    cmd.set_uniform_buffer(0, 2, *g_arenabuf, d.uofs % kArena, sizeof(SkinUbo));
    return ni;
}

uint32_t BindShadow(Vulkan::CommandBuffer& cmd, const Draw& d, const float m[4][4], const float scale[4],
                    const float offset[4]) {
    const uint32_t n = Bind(cmd, d, 0);
    if (!n) return 0;
    SkinUbo u;
    BuildUbo(d, &u);
    std::memcpy(u.m, m, 64);
    std::memcpy(u.scale, scale, 16);
    std::memcpy(u.offset, offset, 16);
    u.cfg[3] &= ~(2 | 4 | 8);   // no mirror (folded into m), no motion history
    u.cfg[2] = 0;
    void* p = cmd.allocate_constant_data(0, 2, sizeof u);
    std::memcpy(p, &u, sizeof u);
    return n;
}

void ResolveMv(Vulkan::CommandBuffer& cmd, Vulkan::Image& out, Vulkan::Image& color, Vulkan::Image& depth,
               const Vulkan::ImageHandle& mv, const Vulkan::Buffer& gs, int scale, float zmul, bool own, int gw, int mx) {
    if (!gw) gw = int(out.get_width()) / scale;
    g_rawmv = mv;
    const uint32_t w = out.get_width(), h = out.get_height();
    if (!g_mv || g_mv->get_width() != w || g_mv->get_height() != h) {
        auto ci = Vulkan::ImageCreateInfo::immutable_2d_image(w, h, VK_FORMAT_R16G16_SFLOAT);
        ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ci.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        g_mv = g_dev->create_image(ci);
        cmd.image_barrier(*g_mv, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, 0,
                          VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    } else {
        cmd.image_barrier(*g_mv, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                          VK_ACCESS_2_MEMORY_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                          VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    cmd.barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    cmd.set_program(g_mvprog);
    cmd.set_texture(0, 0, color.get_view(), Vulkan::StockSampler::NearestClamp);
    cmd.set_texture(0, 1, depth.get_view(), Vulkan::StockSampler::NearestClamp);
    cmd.set_texture(0, 2, mv->get_view(), Vulkan::StockSampler::NearestClamp);
    cmd.set_storage_buffer(0, 5, gs);
    cmd.set_storage_texture(0, 3, out.get_view());
    cmd.set_storage_texture(0, 4, g_mv->get_view());
    struct { int32_t w, h, view; float full; int32_t s; float zmul; int32_t own, gw, mx; } pc = {
        int32_t(w), int32_t(h), g_view ? 1 : 0, g_view_full * float(scale), scale, zmul, own ? 1 : 0, gw, mx};
    cmd.push_constants(&pc, 0, sizeof pc);
    cmd.dispatch((w + 7) / 8, (h + 7) / 8, 1);
    cmd.barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
}

const Vulkan::ImageHandle& MvImage() { return g_mv; }

static void DumpMv(Vulkan::Image& img, VkImageLayout layout, const std::string& prefix) {
    auto& dev = *g_dev;
    const uint32_t w = img.get_width(), h = img.get_height();
    Vulkan::BufferCreateInfo bi = {};
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.domain = Vulkan::BufferDomain::CachedHost;
    bi.size = VkDeviceSize(w) * h * 4;
    auto buf = dev.create_buffer(bi);
    auto cmd = dev.request_command_buffer();
    cmd->image_barrier(img, layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                       VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    cmd->copy_image_to_buffer(*buf, img, 0, {}, {w, h, 1}, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1});
    cmd->image_barrier(img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout, VK_PIPELINE_STAGE_2_COPY_BIT, 0,
                       VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
    cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                 VK_ACCESS_HOST_READ_BIT);
    Vulkan::Fence fence;
    dev.submit(cmd, &fence);
    fence->wait();
    const uint16_t* hv = static_cast<const uint16_t*>(dev.map_host_buffer(*buf, Vulkan::MEMORY_ACCESS_READ_BIT));
    auto half = [](uint16_t x) {
        const uint32_t s = (x >> 15) & 1, e = (x >> 10) & 31, m = x & 1023;
        float f = e == 0 ? std::ldexp(float(m), -24) : e == 31 ? INFINITY : std::ldexp(float(m | 1024), int(e) - 25);
        return s ? -f : f;
    };
    std::vector<float> f(size_t(w) * h * 2);
    for (size_t i = 0; i < f.size(); ++i) f[i] = half(hv[i]);
    static const bool dump = xsgcfg::CStr("debug.native_mv_dump") && std::atoi(xsgcfg::CStr("debug.native_mv_dump"));
    if (std::FILE* fp = dump ? std::fopen((prefix + "mv.f32").c_str(), "wb") : nullptr) {
        uint32_t hdr[2] = {w, h};
        std::fwrite(hdr, 4, 2, fp);
        std::fwrite(f.data(), 4, f.size(), fp);
        std::fclose(fp);
    }
    std::vector<uint8_t> rgb(size_t(w) * h * 3);
    double sum = 0, maxl = 0;
    uint64_t nz = 0;
    for (size_t i = 0; i < size_t(w) * h; ++i) {
        const float x = f[2 * i], y = f[2 * i + 1], len = std::sqrt(x * x + y * y);
        float c[3] = {0.25f, 0.25f, 0.25f};
        if (len >= 0.05f) {
            ++nz;
            sum += len;
            maxl = std::max(maxl, double(len));
            const float hue = std::atan2(y, x) / 6.2831853f + 0.5f, v = 0.25f + 0.75f * std::min(len / (16.0f * 4), 1.0f);
            for (int k = 0; k < 3; ++k) {
                float t = std::fabs(std::fmod(hue + (k == 0 ? 0.0f : k == 1 ? 2.0f / 3 : 1.0f / 3), 1.0f) * 6.0f - 3.0f) - 1.0f;
                c[k] = std::min(1.0f, std::max(0.0f, t)) * v;
            }
        }
        for (int k = 0; k < 3; ++k) rgb[i * 3 + k] = uint8_t(c[k] * 255.0f + 0.5f);
    }
    stbi_write_png((prefix + "mv.png").c_str(), int(w), int(h), 3, rgb.data(), int(w * 3));
    std::fprintf(stderr, "[native]   motion vectors %ux%u: %llu non-zero samples, mean %.2f px, max %.2f px -> %smv.{png,f32}\n", w,
                 h, (unsigned long long)nz, nz ? sum / double(nz) : 0.0, maxl, prefix.c_str());
}

void ShotMv(const std::string& prefix) {
    if (g_mv) DumpMv(*g_mv, VK_IMAGE_LAYOUT_GENERAL, prefix);
    if (g_rawmv) DumpMv(*g_rawmv, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, prefix + "raw");
}

void Vsync(uint32_t frame) {
    if (frame % 600 == 0)
        std::fprintf(stderr,
                     "[native] vsync %u: set 2: %llu batches, %llu vertices, %llu triangles (%llu ADC/facing-culled), "
                     "%llu with history, %llu off-palette bones, %llu set-0 lit batches; set 0: %llu/%llu with history\n",
                     frame, (unsigned long long)g_nbatch, (unsigned long long)g_nvert, (unsigned long long)g_ntri,
                     (unsigned long long)g_nculled, (unsigned long long)g_nprev, (unsigned long long)g_nbadbone,
                     (unsigned long long)g_nset0lit,
                     (unsigned long long)g_nset0prev, (unsigned long long)g_nset0);
    if (frame % 60 == 0) {
        for (auto* m : {&g_hist0, &g_hist2})
            for (auto it = m->begin(); it != m->end();)
                it = frame - it->second.frame > 120 ? m->erase(it) : std::next(it);
    }
}

}  // namespace xsgskin

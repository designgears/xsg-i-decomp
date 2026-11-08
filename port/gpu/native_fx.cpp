/*
 * xsg-gpu native renderer: effects at high resolution, exact ownership, GS
 * draw trace.  See native_fx.h and port/docs/NATIVE_RENDERER.md §7.
 */
#include "native_fx.h"
#include "menu/cfg.h"
#include "native.h"
#include "native_prof.h"

#include "context.hpp"
#include "device.hpp"
#include "gs_interface.hpp"
#include "ps2host/gs.h"
#include "thread_id.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace xsgfx {
namespace {

const uint32_t kFx[] = {
#include "native_fx.comp.inc"
};

void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void Log(const char* fmt, ...) {
    char b[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "[fx] %s\n", b);
}

inline uint32_t Bits(uint64_t v, int lo, int n) { return uint32_t(v >> lo) & uint32_t((1ull << n) - 1); }

/* ---------------------------------------------------------------- state */
Vulkan::Device* g_dev;
Vulkan::Program* g_prog;
std::unique_ptr<Vulkan::Context> g_own_ctx;
std::unique_ptr<Vulkan::Device> g_own_dev;
std::unique_ptr<ParallelGS::GSInterface> g_own;   // the ownership GS (own device)
bool g_own_on = true;
std::vector<uint8_t> g_obuf;                       // pending ownership-GS data of the current message
uint32_t g_frame;                                  // vsync interval being drawn
std::vector<uint32_t> g_trace;
bool Tracing() {
    for (uint32_t t : g_trace)
        if (g_frame == t || g_frame == t + 1) return true;
    return false;
}

/* Marker: FOGCOL; the GS writes (F*C + (255-F)*FOGCOL) >> 8 with F = 0. */
const uint32_t kMarker = 0x5be713;   // FOGCOL (B, G, R) = 0x5b, 0xe7, 0x13
uint32_t MarkerOut() {
    uint32_t m = 0;
    for (int k = 0; k < 24; k += 8) m |= ((((kMarker >> k) & 255) * 255) >> 8) << k;
    return m;
}

struct GpuPrim {
    float xy[3][4];
    float col[3][4];
    float q[4];
    float fog[4];       // F per vertex; w: FOGCOL (bits)
    uint32_t z[4];
    uint32_t kind[4];
    uint32_t tex[4];
    uint32_t st[4];
    uint32_t fb[4];
    int32_t sc[4];
    int32_t tsz[4];
};
static_assert(sizeof(GpuPrim) == 4 * (12 + 12 + 4 * 9), "std430 layout");

struct Tex {
    uint32_t w, h;
    std::vector<uint32_t> px;
    uint64_t tex0 = 0, texa = 0;
    bool from_snap = false;   // decoded at Replay from the trigger-time GS memory
    Vulkan::ImageHandle img;
};

struct PostFrame {
    uint32_t nat_frame = ~0u;      // vsync interval of its native batches
    bool active = false;           // post phase running (until vsync)
    bool done = false;             // post phase of a finished frame
    bool bad = false;
    std::string why, trigger;
    uint64_t texels = 0;
    uint32_t fbw = 8, psm = 0, zbp = 0, zpsm = 0;
    bool snapped = false;                     // g_snap[fbp] holds the pre-effect VRAM
    uint32_t trig_gen = 0;                    // upload generation at the trigger
    std::set<uint32_t> post_drawn;            // other FBPs the GS drew into after the trigger
    std::map<std::pair<uint64_t, uint64_t>, std::shared_ptr<Tex>> snap_tex;   // one decode per texture
    uint32_t nat_zbp[4], nzbp = 0;            // Z buffers of its native batches (the composite decodes them)
    std::vector<GpuPrim> prims;
    std::vector<std::shared_ptr<Tex>> texs;   // per prim (null: none / frame)
};
std::map<uint32_t, PostFrame> g_post;
/* The VRAM of both GSes at the post-phase trigger, per FBP.  4 MB images of
   which only the frame buffer's (and for the main GS the Z buffer's) pages
   are copied; kept across frames so they are not reallocated. */
struct SnapVram {
    std::vector<uint8_t> main, own;
};
std::map<uint32_t, SnapVram> g_snap;
std::set<uint32_t> g_drawn;      // FBPs drawn this interval
std::set<uint32_t> g_rendered;   // FBPs the GS has ever drawn into (render targets)
std::map<uint32_t, uint32_t> g_rt_read;   // render-target FBP -> frame an effect last read it as a texture

/* ---------------------------------------------------------------- GIF parse with vertices */
struct Vtx {
    uint32_t x, y, z;     // 12.4 fixed, Z
    uint8_t r, g, b, a, f;
    float s, t, q;
    uint32_t u, v;
};

struct Gs {
    uint64_t reg[256] = {};
    uint8_t rgba[4] = {0x80, 0x80, 0x80, 0x80};
    float q = 1.0f, s = 0, t = 0;
    uint32_t u = 0, v = 0, fog = 0;
    Vtx vq[3];
    int nv = 0;
} g;

uint32_t g_native_next;   // PATH1 GIF tags still to come from the last native batch
bool g_in_native, g_snap_req, g_restore_after;
// A local->local copy during a post phase: its destination bytes, refreshed in the
// snapshots from the GS memory right after the copy (the effect samples them).
bool g_copy_req;
uint32_t g_copy_off, g_copy_len;
uint64_t g_ndraw, g_npost_frames, g_nbad, g_ncancel;
uint64_t g_saved[4];   // PRMODECONT, FOGCOL, FOG, DTHE around a native packet

/* trace: consecutive primitives with the same state */
struct Run {
    bool open = false;
    uint64_t key[12];
    uint32_t n = 0, path = 0;
    bool native = false;
    int x0, y0, x1, y1;
    float s0, t0, s1, t1;
    uint32_t c0;
} g_run;

void FlushRun() {
    Run& r = g_run;
    if (!r.open) return;
    r.open = false;
    const uint64_t prim = r.key[0], tex0 = r.key[1], frame = r.key[4], zbuf = r.key[5], test = r.key[6], alpha = r.key[7];
    Log("v%u p%u %s prim %u%s%s%s%s ctx%u x%u | FRAME fbp %#x fbw %u psm %#x msk %08x | ZBUF zbp %#x psm %#x msk %u | "
        "TEST %08x ALPHA %#llx | TEX0 tbp %#x tbw %u psm %#x %ux%u tcc %u tfx %u cbp %#x | CLAMP %#llx TEX1 %#llx | "
        "rect (%d,%d)-(%d,%d) uv (%.1f,%.1f)-(%.1f,%.1f) rgba %08x | FBA %u PABE %u DTHE %u COLCLAMP %u",
        g_frame, r.path, r.native ? "NAT" : "gs ", Bits(prim, 0, 3), Bits(prim, 4, 1) ? " TME" : "",
        Bits(prim, 6, 1) ? " ABE" : "", Bits(prim, 8, 1) ? " FST" : "", Bits(prim, 5, 1) ? " FGE" : "",
        Bits(prim, 9, 1), r.n, Bits(frame, 0, 9), Bits(frame, 16, 6), Bits(frame, 24, 6), Bits(frame, 32, 32),
        Bits(zbuf, 0, 9), Bits(zbuf, 24, 4), Bits(zbuf, 32, 1), uint32_t(test), (unsigned long long)alpha,
        Bits(tex0, 0, 14), Bits(tex0, 14, 6), Bits(tex0, 20, 6), 1u << Bits(tex0, 26, 4), 1u << Bits(tex0, 30, 4),
        Bits(tex0, 34, 1), Bits(tex0, 35, 2), Bits(tex0, 37, 14), (unsigned long long)r.key[2],
        (unsigned long long)r.key[3], r.x0, r.y0, r.x1, r.y1, r.s0, r.t0, r.s1, r.t1, r.c0, Bits(r.key[8], 0, 1),
        Bits(r.key[9], 0, 1), Bits(r.key[10], 0, 1), Bits(r.key[11], 0, 1));
}

void TraceDraw(uint32_t path, const Vtx* v, int n) {
    const uint64_t prim = g.reg[0x00];
    const int c = Bits(prim, 9, 1);
    const uint64_t xyoff = g.reg[0x18 + c];
    uint64_t key[12] = {prim & 0x7ff, g.reg[0x06 + c] & ~(7ull << 61), g.reg[0x08 + c], g.reg[0x14 + c],
                        g.reg[0x4c + c], g.reg[0x4e + c], g.reg[0x47 + c], g.reg[0x42 + c], g.reg[0x4a + c],
                        g.reg[0x49], g.reg[0x45], g.reg[0x46]};
    Run& r = g_run;
    if (r.open && (r.path != path || r.native != g_in_native || std::memcmp(r.key, key, sizeof key))) FlushRun();
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
    float s0 = 1e30f, t0 = 1e30f, s1 = -1e30f, t1 = -1e30f;
    const bool fst = Bits(prim, 8, 1);
    const float tw = float(1u << Bits(key[1], 26, 4)), th = float(1u << Bits(key[1], 30, 4));
    for (int i = 0; i < n; ++i) {
        const int x = (int(v[i].x) - int(Bits(xyoff, 0, 16))) / 16, y = (int(v[i].y) - int(Bits(xyoff, 32, 16))) / 16;
        x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y);
        const float s = fst ? v[i].u / 16.0f : v[i].s / v[i].q * tw, t = fst ? v[i].v / 16.0f : v[i].t / v[i].q * th;
        s0 = std::min(s0, s); t0 = std::min(t0, t); s1 = std::max(s1, s); t1 = std::max(t1, t);
    }
    if (!r.open) {
        r.open = true;
        std::memcpy(r.key, key, sizeof key);
        r.n = 0;
        r.path = path;
        r.native = g_in_native;
        r.x0 = x0; r.y0 = y0; r.x1 = x1; r.y1 = y1;
        r.s0 = s0; r.t0 = t0; r.s1 = s1; r.t1 = t1;
        r.c0 = uint32_t(v[n - 1].r) | uint32_t(v[n - 1].g) << 8 | uint32_t(v[n - 1].b) << 16 | uint32_t(v[n - 1].a) << 24;
    }
    r.n++;
    r.x0 = std::min(r.x0, x0); r.y0 = std::min(r.y0, y0); r.x1 = std::max(r.x1, x1); r.y1 = std::max(r.y1, y1);
    r.s0 = std::min(r.s0, s0); r.t0 = std::min(r.t0, t0); r.s1 = std::max(r.s1, s1); r.t1 = std::max(r.t1, t1);
}

void Bad(PostFrame& pf, const char* why) {
    if (!pf.bad) {
        pf.bad = true;
        pf.why = why;
        g_nbad++;
    }
}

/* Does [tbp, tbp + size) overlap the frame buffer at fbp (512 x 448 x 32 bits)? */
bool Overlaps(uint32_t tbp, uint32_t tsize_blocks, uint32_t fbp_blocks) {
    const uint32_t fb_blocks = 512 * 448 * 4 / 256;
    return tbp < fbp_blocks + fb_blocks && fbp_blocks < tbp + tsize_blocks;
}

void Record(PostFrame& pf, uint32_t fbp, const Vtx* v, int n, uint32_t type) {
    if (pf.bad) return;
    if (pf.prims.size() >= 32768) { Bad(pf, "more than 32768 post draws"); return; }
    const uint64_t prim = g.reg[0x00];
    const int c = Bits(prim, 9, 1);
    const uint64_t tex0 = g.reg[0x06 + c], frame = g.reg[0x4c + c], zbuf = g.reg[0x4e + c];
    const uint64_t xyoff = g.reg[0x18 + c], scissor = g.reg[0x40 + c];
    GpuPrim p;
    std::memset(&p, 0, sizeof p);
    const uint32_t gtype = type == 6 ? 6 : (type >= 3 && type <= 5) ? 3 : type;
    if (gtype != 6 && gtype != 3) { Bad(pf, "point/line primitive"); return; }
    const bool fst = Bits(prim, 8, 1);
    for (int i = 0; i < n; ++i) {
        p.xy[i][0] = (float(v[i].x) - float(Bits(xyoff, 0, 16))) / 16.0f;
        p.xy[i][1] = (float(v[i].y) - float(Bits(xyoff, 32, 16))) / 16.0f;
        p.xy[i][2] = fst ? v[i].u / 16.0f : v[i].s;
        p.xy[i][3] = fst ? v[i].v / 16.0f : v[i].t;
        p.col[i][0] = v[i].r; p.col[i][1] = v[i].g; p.col[i][2] = v[i].b; p.col[i][3] = v[i].a;
        p.q[i] = v[i].q;
        p.fog[i] = v[i].f;
        p.z[i] = v[i].z;
    }
    {
        const uint32_t fc = uint32_t(g.reg[0x3d] & 0xffffff);
        std::memcpy(&p.fog[3], &fc, 4);
    }
    p.kind[0] = gtype;
    p.kind[1] = uint32_t(prim & 0x7ff);
    p.tex[0] = uint32_t(tex0);
    p.tex[1] = uint32_t(tex0 >> 32);
    p.tex[2] = uint32_t(g.reg[0x08 + c]);
    p.tex[3] = uint32_t(g.reg[0x08 + c] >> 32);
    p.st[0] = uint32_t(g.reg[0x14 + c]);
    p.st[1] = uint32_t(g.reg[0x42 + c]);
    p.st[2] = uint32_t(g.reg[0x42 + c] >> 32);
    p.st[3] = uint32_t(g.reg[0x47 + c]);
    p.fb[0] = Bits(frame, 32, 32);
    p.fb[1] = Bits(zbuf, 32, 1);
    p.fb[2] = Bits(g.reg[0x4a + c], 0, 1) | Bits(g.reg[0x49], 0, 1) << 1 | Bits(g.reg[0x46], 0, 1) << 2;
    p.fb[3] = Bits(zbuf, 24, 4);
    p.sc[0] = int(Bits(scissor, 0, 11));
    p.sc[1] = int(Bits(scissor, 32, 11));
    p.sc[2] = int(Bits(scissor, 16, 11));
    p.sc[3] = int(Bits(scissor, 48, 11));
    const uint32_t tw = std::min(10u, Bits(tex0, 26, 4)), th = std::min(10u, Bits(tex0, 30, 4));
    p.tsz[0] = int(1u << tw);
    p.tsz[1] = int(1u << th);
    p.tsz[2] = 512;
    p.tsz[3] = 448;
    std::shared_ptr<Tex> t;
    if (Bits(prim, 4, 1)) {
        const uint32_t tbp = Bits(tex0, 0, 14), tpsm = Bits(tex0, 20, 6), tbw = Bits(tex0, 14, 6);
        const uint32_t tblocks = std::max(1u, (1u << tw) * (1u << th) * 4 / 256);
        if (tbp == fbp * 32 && (tpsm == 0 || tpsm == 1) && tbw == pf.fbw) {
            p.kind[2] = 2;
        } else if (Overlaps(tbp, tblocks, pf.zbp * 32)) {
            Bad(pf, "Z buffer read as a texture");
            return;
        } else {
            for (uint32_t d : pf.post_drawn)
                if (Overlaps(tbp, tblocks, d * 32)) { Bad(pf, "texture rendered by the GS after the effect started"); return; }
            // A render target read back (the previous frame for a motion blur): the upload
            // mirror never sees GS drawing, so take it from the GS memory at the trigger
            // (unless an upload replaced those pages since).
            bool rt = false;
            for (uint32_t d : g_rendered)
                if (Overlaps(tbp, tblocks, d * 32)) {
                    // Snapshots copy this target's pages from now on; this frame's was taken
                    // already, so the first frame that reads it is not replayed.
                    if (!g_rt_read.count(d)) { g_rt_read[d] = g_frame; Bad(pf, "render target read first time (snapshotted from now on)"); return; }
                    g_rt_read[d] = g_frame;
                    rt = true;
                }
            if (rt && xsgnative::TextureGen(tex0) <= pf.trig_gen) {
                auto& nt = pf.snap_tex[{tex0 & ((1ull << 61) - 1), g.reg[0x3b]}];
                if (!nt) {
                    nt = std::make_shared<Tex>();
                    nt->w = 1u << tw;
                    nt->h = 1u << th;
                    nt->tex0 = tex0;
                    nt->texa = g.reg[0x3b];
                    nt->from_snap = true;
                }
                p.kind[2] = 1;
                pf.prims.push_back(p);
                pf.texs.push_back(nt);
                return;
            }
            static std::map<std::tuple<uint64_t, uint64_t, uint32_t>, std::shared_ptr<Tex>> cache;
            if (cache.size() > 256) cache.clear();
            const auto key = std::make_tuple(tex0 & ((1ull << 61) - 1), g.reg[0x3b], xsgnative::MirrorGen());
            auto it = cache.find(key);
            if (it == cache.end()) {
                pf.texels += uint64_t(1u << tw) << th;
                if (pf.texels > (24u << 20)) { Bad(pf, "too many texture decodes"); return; }
                auto nt = std::make_shared<Tex>();
                nt->px = xsgnative::DecodeMirrorTexture(tex0, g.reg[0x3b], &nt->w, &nt->h);
                it = cache.emplace(key, nt).first;
            }
            t = it->second;
            p.kind[2] = 1;
        }
    }
    pf.prims.push_back(p);
    pf.texs.push_back(t);
}

void Draw(uint32_t path, const Vtx* v, int n, uint32_t type) {
    g_ndraw++;
    if (Tracing()) TraceDraw(path, v, n);
    if (g_in_native) return;
    const uint64_t prim = g.reg[0x00];
    const int c = Bits(prim, 9, 1);
    const uint64_t frame = g.reg[0x4c + c];
    const uint32_t fbp = Bits(frame, 0, 9);
    const bool writes = Bits(frame, 32, 32) != 0xffffffffu || !Bits(g.reg[0x4e + c], 32, 1);
    // The post frame of this FBP, if active.
    auto it = g_post.find(fbp);
    if (it != g_post.end() && it->second.active) {
        Record(it->second, fbp, v, n, type);
        return;
    }
    if (writes) {
        g_drawn.insert(fbp);
        g_rendered.insert(fbp);
        for (auto& [f, q] : g_post)
            if (q.active && f != fbp) q.post_drawn.insert(fbp);
    }
    if (it == g_post.end() || it->second.nat_frame != g_frame) return;
    PostFrame& pf = it->second;
    // Trigger: a read of this buffer (or its Z) as a texture, or a draw over most of the screen.
    const uint64_t tex0 = g.reg[0x06 + c];
    // The draw's on-screen area (its vertices' box; sprites and strips alike).
    const uint64_t xyoff = g.reg[0x18 + c];
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int i = 0; i < n; ++i) {
        const float x = (float(v[i].x) - float(Bits(xyoff, 0, 16))) / 16.0f;
        const float y = (float(v[i].y) - float(Bits(xyoff, 32, 16))) / 16.0f;
        x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y);
    }
    const float area = std::max(0.0f, std::min(x1, 512.0f) - std::max(x0, 0.0f)) *
                       std::max(0.0f, std::min(y1, 448.0f) - std::max(y0, 0.0f));
    bool trig = false;
    if (Bits(prim, 4, 1)) {
        // A read of this buffer or its Z as a texture over at least half the screen (DOF
        // blur, distortions).  Small local reads (the field's afterimage strip) stay on the
        // GS: the ownership plane hands their pixels back to it.
        const uint32_t tbp = Bits(tex0, 0, 14);
        const uint32_t tw = std::min(10u, Bits(tex0, 26, 4)), th = std::min(10u, Bits(tex0, 30, 4));
        const uint32_t tblocks = std::max(1u, (1u << tw) * (1u << th) * 4 / 256);
        trig = (Overlaps(tbp, tblocks, fbp * 32) || Overlaps(tbp, tblocks, Bits(g.reg[0x4e + c], 0, 9) * 32)) &&
               area >= 0.5f * 512 * 448;
    }
    if (!trig && writes) {
        // A full-screen blended sprite (fades, flashes); opaque ones (clears) are not effects,
        // and neither is a blend that leaves the destination as it is ((A - B) * 0 + Cd).
        const uint64_t al = g.reg[0x42 + c];
        const uint32_t A = Bits(al, 0, 2), B = Bits(al, 2, 2), C = Bits(al, 4, 2), D = Bits(al, 6, 2);
        const bool noop = D == 1 && (A == B || (C == 2 && Bits(al, 32, 8) == 0));
        trig = type == 6 && Bits(prim, 6, 1) && !noop && area >= 0.9f * 512 * 448;
    }
    if (!trig) return;
    pf.active = true;
    pf.done = false;
    pf.bad = false;
    pf.why.clear();
    pf.prims.clear();
    pf.texs.clear();
    pf.snap_tex.clear();
    pf.fbw = std::max(1u, Bits(frame, 16, 6));
    pf.psm = Bits(frame, 24, 6);
    pf.zbp = Bits(g.reg[0x4e + c], 0, 9);
    pf.zpsm = Bits(g.reg[0x4e + c], 24, 4);
    pf.trig_gen = xsgnative::MirrorGen();
    pf.post_drawn.clear();
    g_snap_req = true;
    g_npost_frames++;
    {
        char b[256];
        std::snprintf(b, sizeof b, "path %u prim %#x TEX0 %#llx ALPHA %#llx TEST %#llx FRAME %#llx", path,
                      uint32_t(prim & 0x7ff), (unsigned long long)tex0, (unsigned long long)g.reg[0x42 + c],
                      (unsigned long long)g.reg[0x47 + c], (unsigned long long)frame);
        pf.trigger = b;
    }
    Record(pf, fbp, v, n, type);
}

void Kick(uint32_t path, uint32_t x, uint32_t y, uint32_t z, bool draw) {
    Vtx nv;
    nv.x = x; nv.y = y; nv.z = z;
    nv.r = g.rgba[0]; nv.g = g.rgba[1]; nv.b = g.rgba[2]; nv.a = g.rgba[3];
    nv.f = uint8_t(g.fog);
    nv.s = g.s; nv.t = g.t; nv.q = g.q; nv.u = g.u; nv.v = g.v;
    const uint32_t type = Bits(g.reg[0x00], 0, 3);
    if (type == 7) return;
    const int need = type == 0 ? 1 : (type <= 2 || type == 6) ? 2 : 3;
    if (g.nv < 3) g.vq[g.nv++] = nv;
    if (g.nv < need) return;
    if (draw) {
        Draw(path, g.vq, need, type);
    }
    switch (type) {
    case 2: g.vq[0] = g.vq[1]; g.nv = 1; break;
    case 4: g.vq[0] = g.vq[1]; g.vq[1] = g.vq[2]; g.nv = 2; break;
    case 5: g.vq[1] = g.vq[2]; g.nv = 2; break;
    default: g.nv = 0; break;
    }
}

void SetReg(uint32_t path, uint32_t a, uint64_t v) {
    a &= 0xff;
    switch (a) {
    case 0x00: g.reg[0] = v; g.nv = 0; return;
    case 0x01:
        g.rgba[0] = uint8_t(v); g.rgba[1] = uint8_t(v >> 8); g.rgba[2] = uint8_t(v >> 16); g.rgba[3] = uint8_t(v >> 24);
        { uint32_t qb = uint32_t(v >> 32); std::memcpy(&g.q, &qb, 4); }
        return;
    case 0x02: { uint32_t sb = uint32_t(v), tb = uint32_t(v >> 32); std::memcpy(&g.s, &sb, 4); std::memcpy(&g.t, &tb, 4); return; }
    case 0x03: g.u = Bits(v, 0, 14); g.v = Bits(v, 16, 14); return;
    case 0x0a: g.fog = Bits(v, 56, 8); return;
    case 0x04: case 0x0c: g.fog = Bits(v, 56, 8); Kick(path, Bits(v, 0, 16), Bits(v, 16, 16), Bits(v, 32, 24), a == 0x04); return;
    case 0x05: case 0x0d: Kick(path, Bits(v, 0, 16), Bits(v, 16, 16), uint32_t(v >> 32), a == 0x05); return;
    default: break;
    }
    g.reg[a] = v;
    if (a == 0x53) {
        const uint64_t bb = g.reg[0x50], pos = g.reg[0x51], rg = g.reg[0x52];
        if (Tracing()) {
            FlushRun();
            Log("v%u p%u TRXDIR %u: src bp %#x bw %u psm %#x (%u,%u) -> dst bp %#x bw %u psm %#x (%u,%u) %ux%u", g_frame,
                path, uint32_t(v & 3), Bits(bb, 0, 14), Bits(bb, 16, 6), Bits(bb, 24, 6), Bits(pos, 0, 11),
                Bits(pos, 16, 11), Bits(bb, 32, 14), Bits(bb, 48, 6), Bits(bb, 56, 6), Bits(pos, 32, 11),
                Bits(pos, 48, 11), Bits(rg, 0, 12), Bits(rg, 32, 12));
        }
        // A transfer into a buffer whose post phase runs cannot be replayed.
        for (auto& [fbp, pf] : g_post)
            if (pf.active && (v & 3) != 1 && Overlaps(Bits(bb, 32, 14), 1, fbp * 32)) Bad(pf, "transfer into the frame");
        if ((v & 3) == 2) {
            // GS-side copy: its destination is GS-made content the upload mirror lacks.
            const uint32_t dbp = Bits(bb, 32, 14), dbw = std::max(1u, Bits(bb, 48, 6)), dpsm = Bits(bb, 56, 6);
            const uint32_t rows = Bits(pos, 48, 11) + Bits(rg, 32, 12);
            const uint32_t bpp = dpsm == 0 || dpsm == 1 ? 4 : dpsm == 2 || dpsm == 0xa ? 2 : 4;
            g_rendered.insert(dbp / 32);
            g_copy_off = (dbp * 256) & 0x3fffff;
            g_copy_len = std::min<uint32_t>((4u << 20) - g_copy_off, ((dbw * 64 * rows * bpp + 8191) / 8192 + 1) * 8192);
            for (auto& [fbp, pf] : g_post) g_copy_req |= pf.active && pf.snapped;
        }
    }
}

/* ---------------------------------------------------------------- ownership GS stream */
void OwnAd(uint32_t reg, uint64_t v) {
    uint8_t q[32] = {};
    const uint64_t tag = 1 | (1ull << 15) | (1ull << 60);   // NLOOP 1, EOP, PACKED, NREG 1
    const uint64_t regs = 0xe;
    std::memcpy(q, &tag, 8);
    std::memcpy(q + 8, &regs, 8);
    std::memcpy(q + 16, &v, 8);
    const uint64_t a = reg;
    std::memcpy(q + 24, &a, 8);
    g_obuf.insert(g_obuf.end(), q, q + 32);
}
/* The ownership GS runs on its own thread: it has its own Vulkan device, and
   nothing on the main thread waits for it except the owner-plane reads
   (OwnSync).  Work items are GIF data (path 0-3), a flush (kOwnFlush) or a
   function (kOwnCall: the effect snapshot of its VRAM). */
constexpr uint32_t kOwnFlush = 0xff, kOwnCall = 0xfe;
struct OwnItem {
    uint32_t first;
    std::vector<uint8_t> second;
    std::function<void()> fn;
    OwnItem(uint32_t p, std::vector<uint8_t>&& d) : first(p), second(std::move(d)) {}
};
struct OwnWorker {
    std::thread th;
    std::mutex m;
    std::condition_variable cv, idle_cv;
    std::deque<OwnItem> q;
    std::vector<std::vector<uint8_t>> pool;   // recycled buffers
    bool busy = false;
    void Run() {
        Util::register_thread_index(0);
        std::unique_lock<std::mutex> l(m);
        for (;;) {
            cv.wait(l, [&] { return !q.empty(); });
            auto item = std::move(q.front());
            q.pop_front();
            busy = true;
            l.unlock();
            if (item.first == kOwnFlush) g_own->flush();
            else if (item.first == kOwnCall) item.fn();
            else g_own->gif_transfer(item.first, item.second.data(), item.second.size());
            item.second.clear();
            l.lock();
            pool.push_back(std::move(item.second));
            busy = false;
            if (q.empty()) idle_cv.notify_all();
        }
    }
    void Push(uint32_t path, std::vector<uint8_t>& data) {
        std::vector<uint8_t> next;
        {
            std::lock_guard<std::mutex> l(m);
            q.emplace_back(path, std::move(data));
            if (!pool.empty()) { next = std::move(pool.back()); pool.pop_back(); }
        }
        cv.notify_one();
        data = std::move(next);
        data.clear();
    }
    void Call(std::function<void()> fn) {
        {
            std::lock_guard<std::mutex> l(m);
            q.emplace_back(kOwnCall, std::vector<uint8_t>());
            q.back().fn = std::move(fn);
        }
        cv.notify_one();
    }
    void Sync() {
        std::unique_lock<std::mutex> l(m);
        idle_cv.wait(l, [&] { return q.empty() && !busy; });
    }
};
OwnWorker* g_worker;
uint32_t g_obuf_path;   // path of the data in g_obuf (consecutive messages of one path are handed over together)
void OwnHandOff() {
    if (!g_obuf.empty()) g_worker->Push(g_obuf_path, g_obuf);
}
/* Before the main thread touches g_own (flush, VRAM reads): the worker is idle. */
void OwnSync() {
    xsgprof::Scope ps(xsgprof::OwnWait);
    if (g_worker) { OwnHandOff(); g_worker->Sync(); }
}
/* The end of a GIF message (or a cut in it): its ownership-GS data so far. */
void OwnFlush(uint32_t path) {
    xsgprof::Scope ps(xsgprof::GifOwn);
    if (!g_own || g_obuf.empty()) { g_obuf.clear(); return; }
    if (!g_worker) { g_own->gif_transfer(path, g_obuf.data(), g_obuf.size()); g_obuf.clear(); return; }
    // Whole packets only (the client cuts messages on tag boundaries), so
    // messages of one path may be concatenated: hand over in large pieces.
    g_obuf_path = path;
    if (g_obuf.size() >= (512u << 10)) OwnHandOff();
}
/* A message on another path starts: hand over what the last path left. */
void OwnPath(uint32_t path) {
    if (g_worker && !g_obuf.empty() && g_obuf_path != path) OwnHandOff();
    g_obuf_path = path;
}

struct PathParse {
    uint32_t left = 0, flg = 0, nreg = 0, idx = 0, rl = 0;
    uint64_t regs = 0;
    bool eop = false;
} g_path[4];

/* One qword; `oq` gets the qword the ownership GS should see. */
void Qword(uint32_t path, PathParse& p, const uint8_t* q, uint8_t* oq) {
    std::memcpy(oq, q, 16);
    uint64_t lo, hi;
    std::memcpy(&lo, q, 8);
    std::memcpy(&hi, q + 8, 8);
    if (!p.left) {
        const uint32_t nloop = Bits(lo, 0, 15), pre = Bits(lo, 46, 1);
        if (path == 1 && g_native_next) {
            g_native_next--;
            const bool was = g_in_native;   // a tag without EOP before it: still marked
            g_in_native = true;
            if (g_own) {
                // Native output -> marker colour in the ownership GS.
                if (!was) {
                    g_saved[0] = g.reg[0x1a];
                    g_saved[1] = g.reg[0x3d];
                    g_saved[2] = g.reg[0x0a];
                    g_saved[3] = g.reg[0x45];
                    OwnAd(0x1a, 1);
                    OwnAd(0x3d, kMarker);
                    OwnAd(0x0a, 0);
                    OwnAd(0x45, 0);
                }
                if (pre) {
                    uint64_t l2 = lo;
                    l2 |= 1ull << (47 + 5);    // FGE
                    l2 &= ~(1ull << (47 + 6)); // ABE
                    std::memcpy(oq, &l2, 8);
                } else {
                    OwnAd(0x00, (g.reg[0x00] | 0x20) & ~0x40ull);
                }
            }
        }
        p.eop = Bits(lo, 15, 1);
        p.flg = Bits(lo, 58, 2);
        p.nreg = Bits(lo, 60, 4);
        if (!p.nreg) p.nreg = 16;
        p.regs = hi;
        p.idx = 0;
        if (pre && p.flg == 0) SetReg(path, 0x00, Bits(lo, 47, 11));
        if (p.flg == 0) p.left = nloop * p.nreg;
        else if (p.flg == 1) { p.left = (nloop * p.nreg + 1) / 2; p.rl = nloop * p.nreg; }
        else p.left = nloop;
        if (!p.left && p.eop && path == 1 && g_in_native) { g_in_native = false; g_restore_after = true; }
        return;
    }
    p.left--;
    if (p.flg < 2) {
        auto one = [&](uint64_t v, uint64_t hiw, bool packed) {
            const uint32_t d = uint32_t(p.regs >> (4 * p.idx)) & 15;
            p.idx = (p.idx + 1) % p.nreg;
            if (!packed) { if (d != 0xe && d != 0xf) SetReg(path, d, v); return; }
            switch (d) {
            case 0x0:
                SetReg(path, 0x00, v & 0x7ff);
                if (g_in_native && g_own) { uint64_t v2 = (v | 0x20) & ~0x40ull; std::memcpy(oq, &v2, 8); }
                break;
            case 0x1: {
                uint32_t qb;
                std::memcpy(&qb, &g.q, 4);
                SetReg(path, 0x01, (v & 0xff) | (v >> 32 & 0xff) << 8 | (hiw & 0xff) << 16 | (hiw >> 32 & 0xff) << 24 |
                                       uint64_t(qb) << 32);
                break;
            }
            case 0x2: {
                SetReg(path, 0x02, v);
                uint32_t qb = uint32_t(hiw);
                std::memcpy(&g.q, &qb, 4);
                break;
            }
            case 0x3: g.u = Bits(v, 0, 14); g.v = Bits(v, 32, 14); break;
            case 0x4:
                if (g_in_native && g_own) { uint64_t h2 = hiw & ~(0xffull << 36); std::memcpy(oq + 8, &h2, 8); }
                g.fog = Bits(hiw, 36, 8);
                Kick(path, Bits(v, 0, 16), Bits(v, 32, 16), Bits(hiw, 4, 24), !Bits(hiw, 47, 1));
                break;
            case 0x5: Kick(path, Bits(v, 0, 16), Bits(v, 32, 16), uint32_t(hiw), !Bits(hiw, 47, 1)); break;
            case 0xa: g.fog = Bits(hiw, 36, 8); break;
            case 0xe:
                SetReg(path, uint32_t(hiw & 0xff), v);
                // The ownership GS never answers a local->host transfer (only the main GS's FIFO is read).
                if ((hiw & 0xff) == 0x53 && (v & 3) == 1) { const uint64_t off = 3; std::memcpy(oq, &off, 8); }
                break;
            case 0xf: break;
            default: SetReg(path, d, v); break;
            }
        };
        if (p.flg == 0) one(lo, hi, true);
        else {
            one(lo, 0, false);
            if (p.rl) --p.rl;
            if (p.rl) { one(hi, 0, false); --p.rl; }
        }
    }
    if (!p.left && p.eop && path == 1 && g_in_native) { g_in_native = false; g_restore_after = true; }
}

void Snapshot(ParallelGS::GSInterface& gs) {
    xsgprof::Scope ps(xsgprof::Snapshot);
    for (auto& [fbp, pf] : g_post) {
        if (!pf.active || pf.snapped) continue;
        pf.snapped = true;
        SnapVram& sv = g_snap[fbp];
        // A buffer's pages: 16 page rows (512 lines at 32 bits per pixel) of FBW pages.
        const uint32_t npages = std::min(512u, pf.fbw * 16);
        auto copy = [npages](ParallelGS::GSInterface& g, std::vector<uint8_t>& dst, uint32_t page0) {
            dst.resize(4u << 20);
            for (uint32_t i = 0; i < npages;) {   // in runs up to the 4 MB wrap
                const uint32_t p = (page0 + i) & 511, n = std::min(npages - i, 512 - p);
                std::memcpy(dst.data() + size_t(p) * 8192, g.map_vram_read(size_t(p) * 8192, size_t(n) * 8192),
                            size_t(n) * 8192);
                i += n;
            }
        };
        gs.flush();
        // This buffer and its Z, and the render targets effects read as textures lately
        // (the previous frame of a motion blur, local-copy scratch).  Copying all 4 MB at
        // every trigger cost a GPU readback of the whole memory twice a frame.
        copy(gs, sv.main, fbp);
        copy(gs, sv.main, pf.zbp);
        for (uint32_t i = 0; i < pf.nzbp; ++i)
            if (pf.nat_zbp[i] != pf.zbp) copy(gs, sv.main, pf.nat_zbp[i]);
        for (auto it = g_rt_read.begin(); it != g_rt_read.end();) {
            if (g_frame - it->second > 120) { it = g_rt_read.erase(it); continue; }
            copy(gs, sv.main, it->first);
            ++it;
        }
        if (g_worker) {   // on the ownership thread, after the data before the trigger
            OwnHandOff();
            std::vector<uint8_t>* dst = &sv.own;
            const uint32_t f = fbp;
            g_worker->Call([copy, dst, f] { g_own->flush(); copy(*g_own, *dst, f); });
        } else if (g_own) {
            g_own->flush();
            copy(*g_own, sv.own, fbp);
        }
    }
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
    return psm == 0x01 ? (c & 0xffffff) | 0x80000000u : c;
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

Vulkan::ImageHandle Storage(uint32_t w, uint32_t h, VkFormat fmt, const void* init) {
    auto ci = Vulkan::ImageCreateInfo::immutable_2d_image(w, h, fmt);
    ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT;
    ci.initial_layout = VK_IMAGE_LAYOUT_GENERAL;
    ci.misc = 0;
    Vulkan::ImageInitialData d = {init, 0, 0};
    ci.layout = Vulkan::ImageLayout::General;
    return g_dev->create_image(ci, init ? &d : nullptr);
}

}  // namespace

bool Init(Vulkan::Device* dev) {
    g_dev = dev;
    if (const char* s = xsgcfg::CStr("debug.fx_trace"))
        for (const char* q = s; *q;) {
            char* end;
            unsigned long v = std::strtoul(q, &end, 0);
            if (end == q) break;
            g_trace.push_back(uint32_t(v));
            q = *end ? end + 1 : end;
        }
    if (const char* s = xsgcfg::CStr("debug.native_owner"); s && *s) g_own_on = std::atoi(s) != 0;
    if (g_own_on) {
        // Its own Vulkan context and device: two GSInterfaces on one Granite
        // Device deadlock in next_frame_context (each flush cycles the frame
        // contexts while the other still holds an unsubmitted command buffer).
        g_own_ctx = std::make_unique<Vulkan::Context>();
        g_own_ctx->set_num_thread_indices(1);
        const auto flags = Vulkan::CONTEXT_CREATION_ENABLE_PUSH_DESCRIPTOR_BIT |
                           Vulkan::CONTEXT_CREATION_ENABLE_DESCRIPTOR_HEAP_BIT |
                           Vulkan::CONTEXT_CREATION_ENABLE_DESCRIPTOR_BUFFER_BIT;
        if (g_own_ctx->init_instance_and_device(nullptr, 0, nullptr, 0, flags)) {
            g_own_dev = std::make_unique<Vulkan::Device>();
            g_own_dev->set_context(*g_own_ctx);
            g_own_dev->init_frame_contexts(4);
            g_own = std::make_unique<ParallelGS::GSInterface>();
            ParallelGS::GSOptions o = {};
            if (!g_own->init(g_own_dev.get(), o)) g_own.reset();
        }
        // Granite loads instance-level entry points into volk's globals: creating the
        // second instance (no surface extensions) left the window's surface and
        // swapchain functions null (xsg-gpu crashed in WSI::begin_frame with a window).
        volkLoadInstance(g_dev->get_instance());
        if (!g_own) { Log("ownership GS: init failed"); g_own_on = false; }
        const char* th = xsgcfg::CStr("debug.owner_thread");
        if (g_own && !(th && *th && std::atoi(th) == 0)) {
            g_worker = new OwnWorker;
            g_worker->th = std::thread([] { g_worker->Run(); });
            g_worker->th.detach();
        }
    }
    Vulkan::ResourceLayout cl;
    cl.sets[0].storage_buffer_mask = 1u << 0;
    cl.sets[0].storage_image_mask = (1u << 1) | (1u << 2);
    cl.sets[0].sampled_image_mask = 1u << 3;
    for (int i = 0; i < 4; ++i) cl.sets[0].meta[i].array_size = 1;
    cl.push_constant_size = 48;
    g_prog = dev->request_program(dev->request_shader(kFx, sizeof kFx, &cl));
    Log("on: ownership %s (marker %06x), effect replay, %zu trace vsyncs", g_own ? (g_worker ? "exact (second GS, own thread)" : "exact (second GS)") : "off",
        MarkerOut(), g_trace.size());
    return true;
}

void SeedOwn(const uint8_t* vram) {
    g_post.clear();
    g_drawn.clear();
    if (!g_own) return;
    OwnSync();
    void* w = g_own->map_vram_write(0, 4u << 20);
    std::memcpy(w, vram, 4u << 20);
    g_own->end_vram_write(0, 4u << 20);
}

void Gif(uint32_t path, const uint8_t* data, uint32_t len, ParallelGS::GSInterface& gs) {
    path &= 3;
    PathParse& p = g_path[path];
    OwnPath(path);
    uint32_t start = 0;
    uint8_t oq[16];
    const uint64_t t0 = xsgprof::g_on ? xsgprof::Now() : 0;
    for (uint32_t i = 0; i + 16 <= len; i += 16) {
        Qword(path, p, data + i, oq);
        if (g_snap_req) {
            g_snap_req = false;
            if (i > start) gs.gif_transfer(path, data + start, i - start);
            start = i;
            OwnFlush(path);
            Snapshot(gs);
        }
        g_obuf.insert(g_obuf.end(), oq, oq + 16);
        if (g_copy_req) {
            g_copy_req = false;
            gs.gif_transfer(path, data + start, i + 16 - start);   // through the copy
            start = i + 16;
            const uint8_t* v = static_cast<const uint8_t*>(gs.map_vram_read(g_copy_off, g_copy_len));
            for (auto& [fbp, pf] : g_post)
                if (pf.active && pf.snapped && g_snap[fbp].main.size() == (4u << 20))
                    std::memcpy(g_snap[fbp].main.data() + g_copy_off, v, g_copy_len);
        }
        if (g_restore_after) {
            g_restore_after = false;
            if (g_own) {
                OwnAd(0x1a, g_saved[0]);
                OwnAd(0x3d, g_saved[1]);
                OwnAd(0x0a, g_saved[2]);
                OwnAd(0x45, g_saved[3]);
            }
        }
    }
    if (xsgprof::g_on) xsgprof::g_ns[xsgprof::GifFx] += xsgprof::Now() - t0;
    {
        xsgprof::Scope ps(xsgprof::GifMain);
        if (len > start) gs.gif_transfer(path, data + start, len - start);
    }
    OwnFlush(path);
}

void NativeNext(uint32_t ntags) {
    g_native_next = ntags;
}

void NativeFbp(uint32_t fbp, uint32_t zbp) {
    PostFrame& pf = g_post[fbp];
    if (pf.nat_frame != g_frame) {
        pf = PostFrame();
        pf.nat_frame = g_frame;
    } else if (pf.active) {
        // The "effect" was mid-scene after all: the GS keeps it (the ownership
        // plane sees what it changed); start over at the next trigger.
        const uint32_t nf = pf.nat_frame;
        const PostFrame old = pf;
        pf = PostFrame();
        pf.nat_frame = nf;
        std::memcpy(pf.nat_zbp, old.nat_zbp, sizeof pf.nat_zbp);
        pf.nzbp = old.nzbp;
        g_ncancel++;
    }
    bool seen = false;
    for (uint32_t i = 0; i < pf.nzbp; ++i) seen |= pf.nat_zbp[i] == zbp;
    if (!seen && pf.nzbp < 4) pf.nat_zbp[pf.nzbp++] = zbp;
}

void Vsync(uint32_t frame, ParallelGS::GSInterface& gs) {
    if (Tracing()) {
        FlushRun();
        const auto& pr = gs.get_priv_register_state();
        Log("v%u: vsync (%llu prims total) PMODE %#llx DISPFB1 %#llx DISPFB2 %#llx BGCOLOR %#llx", frame,
            (unsigned long long)g_ndraw, (unsigned long long)pr.qwords_lo[0], (unsigned long long)pr.qwords_lo[0x70 >> 3],
            (unsigned long long)pr.qwords_lo[0x90 >> 3], (unsigned long long)pr.qwords_lo[0xe0 >> 3]);
    }
    for (auto& [fbp, pf] : g_post)
        if (pf.active) { pf.active = false; pf.done = true; }
    g_drawn.clear();
    if (g_own) {
        if (g_worker) { OwnHandOff(); std::vector<uint8_t> none; g_worker->Push(kOwnFlush, none); }
        else g_own->flush();
    }
    if (frame % 600 == 0)
        Log("vsync %u: %llu post phases (%llu cancelled by a later native batch, %llu not replayable)", frame,
            (unsigned long long)g_npost_frames, (unsigned long long)g_ncancel, (unsigned long long)g_nbad);
    g_frame = frame + 1;
}

const uint8_t* PreVram(uint32_t fbp) {
    auto it = g_post.find(fbp);
    if (it == g_post.end() || !it->second.done || !it->second.snapped) return nullptr;
    return g_snap[fbp].main.data();
}

const char* PostTrigger(uint32_t fbp) {
    auto it = g_post.find(fbp);
    return it == g_post.end() ? "" : it->second.trigger.c_str();
}

Post PostState(uint32_t fbp, const char** why) {
    auto it = g_post.find(fbp);
    if (it == g_post.end() || !it->second.done) return Post::None;
    if (why) *why = it->second.why.c_str();
    return it->second.bad ? Post::Unsupported : Post::Replay;
}

size_t PostCount(uint32_t fbp) {
    auto it = g_post.find(fbp);
    return it == g_post.end() ? 0 : it->second.prims.size();
}

bool OwnerPlane(const Surface& sf, std::vector<uint32_t>& out) {
    if (!g_own) return false;
    const uint8_t* v = nullptr;
    auto it = g_post.find(sf.fbp);
    if (it != g_post.end() && it->second.done && it->second.snapped && g_own) {
        OwnSync();   // the snapshot may still be running on the ownership thread
        v = g_snap[sf.fbp].own.data();
    }
    if (!v) {
        OwnSync();
        g_own->flush();
        v = static_cast<const uint8_t*>(g_own->map_vram_read(0, 4u << 20));
    }
    const uint32_t m = MarkerOut(), m2 = kMarker;
    const bool c16 = sf.psm == 0x02 || sf.psm == 0x0a;
    const uint32_t mask = c16 ? 0xf8f8f8u : 0xffffffu;
    const size_t base = out.size();
    out.resize(base + size_t(sf.w) * sf.h);
    uint32_t* o = out.data() + base;
    xsgnative::ParallelRows(sf.h, [&](uint32_t y0, uint32_t y1) {
        for (uint32_t y = y0; y < y1; ++y)
            for (uint32_t x = 0; x < sf.w; ++x) {
                const uint32_t c = GsPixel(v, sf.fbp, sf.fbw, sf.psm, x, y) & mask;
                o[size_t(y) * sf.w + x] = c == (m & mask) || c == (m2 & mask);
            }
    });
    return true;
}

void Replay(Vulkan::CommandBuffer& cmd, uint32_t fbp, int s, uint32_t w, uint32_t h, const Vulkan::Image& color,
            const Vulkan::Image& z) {
    auto it = g_post.find(fbp);
    if (it == g_post.end() || it->second.prims.empty()) return;
    PostFrame& pf = it->second;
    auto& dev = *g_dev;
    Vulkan::BufferCreateInfo bi = {};
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bi.domain = Vulkan::BufferDomain::LinkedDeviceHost;
    bi.size = pf.prims.size() * sizeof(GpuPrim);
    auto buf = dev.create_buffer(bi, pf.prims.data());
    // The frame-read source: a copy of the surface, refreshed before each frame-reading primitive.
    Vulkan::ImageHandle copy = Storage(w * s, h * s, VK_FORMAT_R8G8B8A8_UNORM, nullptr);
    static Vulkan::ImageHandle white;
    if (!white) { const uint32_t px = 0xffffffffu; white = Storage(1, 1, VK_FORMAT_R8G8B8A8_UNORM, &px); }
    cmd.set_program(g_prog);
    // Rectangles written since the last barrier: a primitive waits for the earlier ones
    // only where it overlaps them (GS order).  Effects of thousands of small sprites
    // (particles, the battle transition's 8000 strips) mostly do not overlap, and a
    // barrier after every dispatch serialised them (30-45 ms frames).
    struct Rect { int x0, y0, x1, y1; };
    static std::vector<Rect> pending;
    pending.clear();
    auto flush_pending = [&] {
        if (pending.empty()) return;
        cmd.barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        pending.clear();
    };
    for (size_t i = 0; i < pf.prims.size(); ++i) {
        const GpuPrim& p = pf.prims[i];
        const Vulkan::Image* src = white.get();
        // The game's soft-focus feedback: the frame itself blended back full-screen at
        // (nearly) its own size.  At 1x it is a faint glow; at native scale it shows as a
        // doubled outline around every figure, so the native layer leaves it out.
        if (p.kind[2] == 2 && p.kind[0] == 6) {
            const float rw = std::fabs(p.xy[1][0] - p.xy[0][0]), rh = std::fabs(p.xy[1][1] - p.xy[0][1]);
            const float tw = std::fabs(p.xy[1][2] - p.xy[0][2]), th = std::fabs(p.xy[1][3] - p.xy[0][3]);
            if (rw >= 0.9f * float(w) && rh >= 0.9f * float(h) && tw > 0 && th > 0 && std::fabs(rw / tw - 1.0f) < 0.05f &&
                std::fabs(rh / th - 1.0f) < 0.05f)
                continue;
        }
        if (p.kind[2] == 2) {
            pending.clear();   // the copy's barrier below orders everything before it
            cmd.barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
            cmd.copy_image(*copy, color);
            cmd.barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
            src = copy.get();
            cmd.set_program(g_prog);
        } else if (p.kind[2] == 1 && pf.texs[i]) {
            Tex& t = *pf.texs[i];
            if (t.from_snap && t.px.empty() && pf.snapped) {
                uint32_t tw, th;
                t.px = xsgnative::DecodeVramTexture(g_snap[fbp].main.data(), t.tex0, t.texa, &tw, &th);
                t.w = tw;
                t.h = th;
            }
            if (t.px.empty()) { t.px.assign(1, 0); t.w = t.h = 1; }
            if (!t.img) t.img = Storage(t.w, t.h, VK_FORMAT_R8G8B8A8_UNORM, t.px.data());
            src = t.img.get();
        }
        cmd.set_storage_buffer(0, 0, *buf);
        cmd.set_storage_texture(0, 1, color.get_view());
        cmd.set_storage_texture(0, 2, z.get_view());
        cmd.set_texture(0, 3, src->get_view(), Vulkan::StockSampler::NearestClamp);
        // Bounding box in s x samples.
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        const int nv = p.kind[0] == 6 ? 2 : 3;
        for (int k = 0; k < nv; ++k) {
            x0 = std::min(x0, p.xy[k][0]); y0 = std::min(y0, p.xy[k][1]);
            x1 = std::max(x1, p.xy[k][0]); y1 = std::max(y1, p.xy[k][1]);
        }
        const int bx0 = std::max(0, int(std::floor(x0 * s)) - 1), by0 = std::max(0, int(std::floor(y0 * s)) - 1);
        const int bx1 = std::min(int(w * s), int(std::ceil(x1 * s)) + s + 1);
        const int by1 = std::min(int(h * s), int(std::ceil(y1 * s)) + s + 1);
        if (bx1 <= bx0 || by1 <= by0) continue;
        const uint32_t zmax = p.fb[3] == 0 ? 0xffffffffu : (p.fb[3] == 1 ? 0xffffffu : 0xffffu);
        // dispatch rows/cols are 8-aligned from (bx0, by0): the written area
        const Rect r = {bx0, by0, bx0 + (bx1 - bx0 + 7) / 8 * 8, by0 + (by1 - by0 + 7) / 8 * 8};
        bool hit = pending.size() >= 64;
        for (const Rect& q : pending)
            if (!hit && r.x0 < q.x1 && q.x0 < r.x1 && r.y0 < q.y1 && q.y0 < r.y1) hit = true;
        if (hit) flush_pending();
        struct { int32_t prim, s, w, h, ox, oy, pad0, pad1; uint32_t pmode, bg, zmax, pad2; } pc = {
            int32_t(i), s, int32_t(w), int32_t(h), bx0, by0, 0, 0, 0, 0, zmax, 0};
        cmd.push_constants(&pc, 0, sizeof pc);
        cmd.dispatch((bx1 - bx0 + 7) / 8, (by1 - by0 + 7) / 8, 1);
        pending.push_back(r);
    }
    flush_pending();
}

bool Pmode(Vulkan::CommandBuffer& cmd, const ParallelGS::GSInterface& gs, int s, uint32_t w, uint32_t h,
           const Vulkan::Image& color, const Vulkan::Image& z) {
    const auto& pr = gs.get_priv_register_state();
    const uint64_t pmode = pr.qwords_lo[0], bg = pr.qwords_lo[0xe0 >> 3];
    const bool en1 = pmode & 1, en2 = pmode & 2, slbg = pmode & 0x80;
    // Two circuits reading different buffers, or one circuit blended with the background.
    if (!((en1 && en2) || (en1 && slbg))) return false;
    if (en1 && en2) {
        const uint64_t d1 = pr.qwords_lo[0x70 >> 3], d2 = pr.qwords_lo[0x90 >> 3];
        if ((d1 & 0x1ff) != (d2 & 0x1ff) && !slbg) return false;   // circuit 2 is another buffer: not at high res
    }
    static Vulkan::ImageHandle white;
    if (!white) { const uint32_t px = 0xffffffffu; white = Storage(1, 1, VK_FORMAT_R8G8B8A8_UNORM, &px); }
    static GpuPrim dummy;
    Vulkan::BufferCreateInfo bi = {};
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bi.domain = Vulkan::BufferDomain::LinkedDeviceHost;
    bi.size = sizeof dummy;
    auto buf = g_dev->create_buffer(bi, &dummy);
    cmd.set_program(g_prog);
    cmd.set_storage_buffer(0, 0, *buf);
    cmd.set_storage_texture(0, 1, color.get_view());
    cmd.set_storage_texture(0, 2, z.get_view());
    cmd.set_texture(0, 3, white->get_view(), Vulkan::StockSampler::NearestClamp);
    // Same buffer on both circuits: c1 = c2, the blend is then only with BGCOLOR (SLBG).
    const uint32_t pm = uint32_t(pmode) & ~(en2 && !slbg ? 2u : 0u);
    struct { int32_t prim, s, w, h, ox, oy, pad0, pad1; uint32_t pmode, bg, zmax, pad2; } pc = {
        -1, s, int32_t(w), int32_t(h), 0, 0, 0, 0, pm, uint32_t(bg), 0, 0};
    cmd.push_constants(&pc, 0, sizeof pc);
    cmd.dispatch((w * s + 7) / 8, (h * s + 7) / 8, 1);
    cmd.barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
    return true;
}

bool Validate1x(const Surface& sf, const uint8_t* final_vram, uint64_t hist[4], std::vector<uint32_t>* img) {
    auto it = g_post.find(sf.fbp);
    if (it == g_post.end() || !it->second.done || !it->second.snapped) return false;
    const uint8_t* pre = g_snap[sf.fbp].main.data();
    PostFrame& pf = it->second;
    const size_t n = size_t(sf.w) * sf.h;
    std::vector<uint32_t> c(n), z(n);
    for (uint32_t y = 0; y < sf.h; ++y)
        for (uint32_t x = 0; x < sf.w; ++x) {
            c[size_t(y) * sf.w + x] = GsPixel(pre, sf.fbp, sf.fbw, sf.psm, x, y);
            z[size_t(y) * sf.w + x] = GsZ(pre, sf.zbp, sf.fbw, sf.zpsm, x, y);
        }
    auto ci = Storage(sf.w, sf.h, VK_FORMAT_R8G8B8A8_UNORM, c.data());
    auto zi = Storage(sf.w, sf.h, VK_FORMAT_R32_UINT, z.data());
    auto cmd = g_dev->request_command_buffer();
    Replay(*cmd, sf.fbp, 1, sf.w, sf.h, *ci, *zi);
    Vulkan::BufferCreateInfo bi = {};
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.domain = Vulkan::BufferDomain::CachedHost;
    bi.size = n * 4;
    auto rb = g_dev->create_buffer(bi);
    cmd->barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT,
                 VK_ACCESS_2_TRANSFER_READ_BIT);
    cmd->copy_image_to_buffer(*rb, *ci, 0, {}, {sf.w, sf.h, 1}, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1});
    cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                 VK_ACCESS_HOST_READ_BIT);
    Vulkan::Fence fence;
    g_dev->submit(cmd, &fence);
    fence->wait();
    const uint32_t* out = static_cast<const uint32_t*>(g_dev->map_host_buffer(*rb, Vulkan::MEMORY_ACCESS_READ_BIT));
    if (img) img->assign(out, out + n);
    for (int k = 0; k < 4; ++k) hist[k] = 0;
    for (uint32_t y = 0; y < sf.h; ++y)
        for (uint32_t x = 0; x < sf.w; ++x) {
            const uint32_t a = out[size_t(y) * sf.w + x], b = GsPixel(final_vram, sf.fbp, sf.fbw, sf.psm, x, y);
            int m = 0;
            for (int k = 0; k < 24; k += 8) m = std::max(m, std::abs(int((a >> k) & 255) - int((b >> k) & 255)));
            hist[m == 0 ? 0 : m <= 2 ? 1 : m <= 8 ? 2 : 3]++;
        }
    return true;
}

}  // namespace xsgfx

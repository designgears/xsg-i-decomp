/*
 * xsg-gpu native renderer: effects at high resolution, exact ownership, GS
 * draw trace.  See native_fx.h and port/docs/NATIVE_RENDERER.md §7.
 */
#include "native_fx.h"
#include "menu/cfg.h"
#include "native.h"
#include "native_prof.h"
#include "gs_addr.h"

#include "context.hpp"
#include "device.hpp"
#include "gs_interface.hpp"
#include "ps2host/gs.h"
#include "thread_id.hpp"

#include <array>
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
/* Bytes that grow without zero-filling: the ownership stream is appended a qword at a time. */
template <class T>
struct NoInit : std::allocator<T> {
    template <class U> struct rebind { using other = NoInit<U>; };
    NoInit() = default;
    template <class U> NoInit(const NoInit<U>&) {}
    template <class U> void construct(U* p) { ::new (static_cast<void*>(p)) U; }
    template <class U, class... A> void construct(U* p, A&&... a) { ::new (static_cast<void*>(p)) U(std::forward<A>(a)...); }
};
using Bytes = std::vector<uint8_t, NoInit<uint8_t>>;
Bytes g_obuf;                                       // pending ownership-GS data of the current message
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
/* Deferred effects (the battle's sparks, barriers, glows): blended draws into a buffer that
   native batches drew into this frame, recorded and redrawn at S× over the composite instead of
   left to the 1x GS (whose pixels would take the scene behind them down to 1x).  The ownership GS
   does not draw them (their vertex kicks lose their drawing bit there), so the native pixels under
   them stay owned; the composite takes the GS colour inside their boxes from the ownership GS,
   which has the scene without them.  Per FBP; nat_frame as g_post's. */
std::map<uint32_t, PostFrame> g_ov;
struct OvBox { float x0, y0, x1, y1; };
struct VetoList { uint32_t frame = ~0u; std::vector<OvBox> boxes; };
/* Per FBP: the triangles (sprites as two) of GS draws gated by DATE that read a copy of a frame
   (Draw), the frame they were drawn in; OwnerPlane hands the pixels they cover back to the GS. */
struct FrTris { uint32_t frame = ~0u; std::vector<std::array<float, 6>> tris; };
std::map<uint32_t, FrTris> g_fr_tris;
struct CopyDst { uint32_t bp, blocks, frame; };
std::vector<CopyDst> g_copy_dst;   // this frame's GS-side (local -> local) copies (SetReg TRXDIR 2)
bool OverlapsCopy(uint64_t tex0);   // below
std::map<uint32_t, std::vector<OvBox>> g_ov_box;
bool g_kick_rw;      // the vertex kick being parsed can be rewritten for the ownership GS
bool g_defer_kick;   // ... and it was deferred: rewrite it so the ownership GS does not draw
uint64_t g_ndefer;
uint64_t g_nrefuse[8];   // blended draws over native content not deferred: 0 not rewritable, 1 point/line, 2 texture, 3 record
/* The VRAM of both GSes at the post-phase trigger, per FBP.  4 MB images of
   which only the frame buffer's (and for the main GS the Z buffer's) pages
   are copied; kept across frames so they are not reallocated. */
struct SnapVram {
    std::vector<uint8_t> main, own;
};
std::map<uint32_t, SnapVram> g_snap;
std::set<uint32_t> g_drawn;      // FBPs drawn this interval
std::set<uint32_t> g_rendered;   // FBPs the GS has ever drawn into (render targets)
std::map<uint32_t, uint32_t> g_rt_read;
std::map<uint32_t, uint32_t> g_rt_gen;    // render-target FBP -> upload generation when the GS last drew into it   // render-target FBP -> frame an effect last read it as a texture

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

bool g_trace_def;   // the primitive being traced was deferred (g_ov)
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
        g_frame, r.path, (prim >> 20) & 1 ? "DEF" : r.native ? "NAT" : "gs ", Bits(prim, 0, 3), Bits(prim, 4, 1) ? " TME" : "",
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
    uint64_t key[12] = {(prim & 0x7ff) | (g_trace_def ? 1ull << 20 : 0), g.reg[0x06 + c] & ~(7ull << 61), g.reg[0x08 + c], g.reg[0x14 + c],
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

void Record(PostFrame& pf, uint32_t fbp, const Vtx* v, int n, uint32_t type, bool mirror_ok = false) {
    if (pf.bad) return;
    if (pf.prims.size() >= 32768) { Bad(pf, "more than 32768 post draws"); return; }
    const uint64_t prim = g.reg[0x00];
    const int c = Bits(prim, 9, 1);
    const uint64_t tex0 = g.reg[0x06 + c], frame = g.reg[0x4c + c], zbuf = g.reg[0x4e + c];
    const uint64_t xyoff = g.reg[0x18 + c], scissor = g.reg[0x40 + c];
    GpuPrim p;
    std::memset(&p, 0, sizeof p);
    const uint32_t gtype = type == 6 ? 6 : (type >= 3 && type <= 5) ? 3 : type;
    if (gtype != 6 && gtype != 3) {
        // Points and lines (battle target markers and beams, #3): one pixel wide on the GS.
        // Replayed as what the GS covers: a point as a 1x1 sprite, a line as a quad one pixel
        // across its minor axis (two triangles), each keeping its vertices' colours.
        if (type == 0 && n >= 1) {
            Vtx s[2] = {v[0], v[0]};
            s[0].x -= 8; s[0].y -= 8;
            s[1].x += 8; s[1].y += 8;
            Record(pf, fbp, s, 2, 6, mirror_ok);
            return;
        }
        if ((type == 1 || type == 2) && n >= 2) {
            const int dx = int(v[1].x) - int(v[0].x), dy = int(v[1].y) - int(v[0].y);
            const bool xmajor = std::abs(dx) >= std::abs(dy);
            Vtx a0 = v[0], a1 = v[0], b0 = v[1], b1 = v[1];
            if (xmajor) { a0.y -= 8; a1.y += 8; b0.y -= 8; b1.y += 8; }
            else { a0.x -= 8; a1.x += 8; b0.x -= 8; b1.x += 8; }
            const Vtx t0[3] = {a0, a1, b0}, t1[3] = {b0, a1, b1};
            Record(pf, fbp, t0, 3, 3, mirror_ok);
            Record(pf, fbp, t1, 3, 3, mirror_ok);
            return;
        }
        Bad(pf, "point/line primitive");
        return;
    }
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
                if (!mirror_ok && Overlaps(tbp, tblocks, d * 32)) {
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
            static std::map<std::tuple<uint64_t, uint64_t, uint64_t>, std::shared_ptr<Tex>> cache;
            if (cache.size() > 256) cache.clear();
            // the texture's content (its pages' hashes; the game re-uploads the same effect
            // textures every frame, so neither the mirror's nor the pages' upload generation
            // keeps a decode), and the CLUT buffer for indexed formats
            const bool indexed = ((tex0 >> 20) & 63) >= 0x13;
            const auto key = std::make_tuple(tex0 & ((1ull << 61) - 1), g.reg[0x3b],
                                             xsgnative::TextureHash(tex0) ^ (indexed ? xsgnative::ClutHash() * 31 : 0));
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
        g_rt_gen[fbp] = xsgnative::MirrorGen();
        for (auto& [f, q] : g_post)
            if (q.active && f != fbp) q.post_drawn.insert(fbp);
    }
    // A GS draw textured from a GS-rendered buffer (a frame, or a copy of one) that is no post
    // effect: in the ownership GS it reads the marker colour wherever native content was copied,
    // so a refraction over a native character (the stealth suit in MC_ELS01, a frame copy drawn
    // inside the character's alpha mark) left the marker -- the composite then took native's
    // pixels and the effect vanished (#3).  The pixels its triangles cover go back to the GS
    // (OwnerPlane).  Only draws gated by the destination alpha test (DATE: the effect is confined
    // to a mark a native batch wrote) that read a copy the GS made of a buffer this frame; reads
    // of the frame itself (the characters' soft-edge pass) and render-to-texture buffers (blurs)
    // are replayed or drawn over native content as before.  [debug] fx_frame_read_box = 0 turns this off.
    static const bool fr_on = !(xsgcfg::CStr("debug.fx_frame_read_box") && std::atoi(xsgcfg::CStr("debug.fx_frame_read_box")) == 0);
    if (fr_on && writes && Bits(prim, 4, 1) && Bits(g.reg[0x47 + c], 14, 1) && Bits(g.reg[0x06 + c], 0, 14) != fbp * 32 &&
        OverlapsCopy(g.reg[0x06 + c])) {
        const uint64_t xyoff = g.reg[0x18 + c];
        float x[3], y[3];
        const int m = std::min(n, 3);
        for (int i = 0; i < m; ++i) {
            x[i] = (float(v[i].x) - float(Bits(xyoff, 0, 16))) / 16.0f;
            y[i] = (float(v[i].y) - float(Bits(xyoff, 32, 16))) / 16.0f;
        }
        FrTris& fl = g_fr_tris[fbp];
        if (fl.frame != g_frame) { fl.frame = g_frame; fl.tris.clear(); }
        if (fl.tris.size() < 16384) {
            if (type == 6 && m == 2) {   // sprite: two triangles
                fl.tris.push_back({x[0], y[0], x[1], y[0], x[0], y[1]});
                fl.tris.push_back({x[1], y[0], x[1], y[1], x[0], y[1]});
            } else if (m == 3) {
                fl.tris.push_back({x[0], y[0], x[1], y[1], x[2], y[2]});
            }
        }
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

/* A blended draw over native content that stays on the GS: inside its box the composite keeps
   the main GS's colour (the ownership GS drew it over the marker).  Returns false. */
std::map<uint32_t, VetoList> g_ov_veto;   // per FBP, for the frame it was drawn in
bool Veto(uint32_t fbp, const Vtx* v, int n) {
    VetoList& vl = g_ov_veto[fbp];
    if (vl.frame != g_frame) { vl.frame = g_frame; vl.boxes.clear(); }
    const int c = Bits(g.reg[0x00], 9, 1);
    const uint64_t xyoff = g.reg[0x18 + c];
    OvBox b = {1e9f, 1e9f, -1e9f, -1e9f};
    for (int i = 0; i < n; ++i) {
        const float x = (float(v[i].x) - float(Bits(xyoff, 0, 16))) / 16.0f;
        const float y = (float(v[i].y) - float(Bits(xyoff, 32, 16))) / 16.0f;
        b.x0 = std::min(b.x0, x); b.y0 = std::min(b.y0, y); b.x1 = std::max(b.x1, x); b.y1 = std::max(b.y1, y);
    }
    auto& boxes = vl.boxes;
    if (!boxes.empty()) {
        OvBox& l = boxes.back();
        if ((b.x0 <= l.x1 + 2 && l.x0 <= b.x1 + 2 && b.y0 <= l.y1 + 2 && l.y0 <= b.y1 + 2) || boxes.size() >= 4096) {
            l.x0 = std::min(l.x0, b.x0); l.y0 = std::min(l.y0, b.y0); l.x1 = std::max(l.x1, b.x1); l.y1 = std::max(l.y1, b.y1);
            return false;
        }
    }
    boxes.push_back(b);
    return false;
}

/* [renderer] native_edge_fx (default 1): the characters' frame-reading soft-edge pass replayed at
   native scale.  Live. */
bool EdgeFx() {
    // asked per primitive: the setting is read again only when the settings change
    static unsigned ver = ~0u;
    static bool on = true;
    if (ver != xsgcfg::Version()) {
        ver = xsgcfg::Version();
        const char* v = xsgcfg::CStr("renderer.native_edge_fx");
        on = !(v && *v == '0');
    }
    return on;
}

/* Defer this primitive (see g_ov)?  Records it when so. */
bool Defer(const Vtx* v, int n, uint32_t type) {
    static const bool on = !(xsgcfg::CStr("debug.fx_defer") && std::atoi(xsgcfg::CStr("debug.fx_defer")) == 0);
    if (!on || g_in_native || !g_own) return false;
    const uint64_t prim = g.reg[0x00];
    const int c = Bits(prim, 9, 1);
    const uint64_t frame = g.reg[0x4c + c];
    // an alpha-only triangle (the mask half of a frame-reading pair, below) is replayed even unblended
    const bool alpha_mark = EdgeFx() && (Bits(frame, 32, 32) & 0x00ffffffu) == 0x00ffffffu && type >= 3 && type <= 5;
    if (!Bits(prim, 6, 1) && !alpha_mark) return false;      // opaque: the ownership plane handles it
    if (Bits(frame, 32, 32) == 0xffffffffu) return false;
    const uint32_t fbp = Bits(frame, 0, 9);
    auto it = g_post.find(fbp);
    if (it == g_post.end() || it->second.nat_frame != g_frame || it->second.active) return false;
    // masked colour writes (the afterimage's alpha-bit marks) and destination-alpha tests depend on
    // exactly what the ownership GS has drawn: they stay on the GS (a pure alpha mark changes no colour)
    // ...except triangles, which may be the mask half of a pair whose other half reads the frame
    // (the characters' soft-edge pass): replayed in order, so the mark is in the native frame when
    // the textured half tests it.
    if ((Bits(frame, 32, 32) & 0x00ffffffu) == 0x00ffffffu && !alpha_mark) return false;
    if ((Bits(frame, 32, 32) & 0x00ffffffu) && (Bits(frame, 32, 32) & 0x00ffffffu) != 0x00ffffffu) { g_nrefuse[4]++; return Veto(fbp, v, n); }
    if (Bits(g.reg[0x47 + c], 14, 1)) { g_nrefuse[4]++; return Veto(fbp, v, n); }
    if (!g_kick_rw) { g_nrefuse[0]++; return Veto(fbp, v, n); }
    if (type < 3 || type > 6) { g_nrefuse[1]++; return Veto(fbp, v, n); }   // points / lines stay on the GS
    if (Bits(prim, 4, 1)) {   // its texture must be decodable from the upload mirror
        const uint64_t tex0 = g.reg[0x06 + c];
        const uint32_t tbp = Bits(tex0, 0, 14);
        const uint32_t tw = std::min(10u, Bits(tex0, 26, 4)), th = std::min(10u, Bits(tex0, 30, 4));
        const uint32_t tblocks = std::max(1u, (1u << tw) * (1u << th) * 4 / 256);
        // a render target's pages only when an upload replaced them since the GS last drew there
        // The frame itself, exactly (its base, width and a 24/32-bit format): a frame read, which the
        // replay does at native resolution (Record, kind 2) -- the characters' soft-edge pass draws
        // their triangles again textured from the frame, and on the GS it left every character pixel
        // it touched as blocky 1x pixels in the composite.
        const uint32_t tpsm = Bits(tex0, 20, 6), tbw = Bits(tex0, 14, 6);
        // Triangles only: the full-screen soft-focus sprites that read the frame stay on the GS as before
        // (the replay leaves the frame-sized feedback out, and with it the floor shadows' tone).
        // [renderer] native_edge_fx = 0 keeps them on the GS (blocky character edges, but the soft
        // contact shadows under some characters, which the native-scale replay loses for now).
        const bool frame_read = EdgeFx() && type >= 3 && type <= 5 && tbp == fbp * 32 && (tpsm == 0 || tpsm == 1) &&
                                tbw == std::max(1u, Bits(frame, 16, 6));
        // (its declared size, 512x512, reaches past the 448 rows into the next buffer; only the frame
        // is sampled, so a frame read skips the overlap checks)
        for (uint32_t d : g_rendered)
            if (!frame_read && Overlaps(tbp, tblocks, d * 32) && (d == fbp || xsgnative::PageGen(d) <= g_rt_gen[d])) {
                g_nrefuse[2]++;
                return Veto(fbp, v, n);
            }
        if (Overlaps(tbp, tblocks, Bits(g.reg[0x4e + c], 0, 9) * 32)) { g_nrefuse[2]++; return Veto(fbp, v, n); }
    }
    PostFrame& ov = g_ov[fbp];
    if (ov.nat_frame != g_frame) {
        ov = PostFrame();
        ov.nat_frame = g_frame;
        g_ov_box[fbp].clear();
    }
    ov.fbw = std::max(1u, Bits(frame, 16, 6));
    ov.psm = Bits(frame, 24, 6);
    ov.zbp = Bits(g.reg[0x4e + c], 0, 9);
    ov.zpsm = Bits(g.reg[0x4e + c], 24, 4);
    ov.trig_gen = xsgnative::MirrorGen();
    const size_t before = ov.prims.size();
    Record(ov, fbp, v, n, type, true);
    if (ov.bad || ov.prims.size() == before) {   // could not record: this one stays on the GS
        if (Tracing() || g_nrefuse[3] < 5) Log("defer: not recorded (%s)", ov.why.c_str());
        g_nrefuse[3]++;
        ov.bad = false;
        ov.prims.resize(before);
        ov.texs.resize(before);
        return Veto(fbp, v, n);
    }
    // its box (1x pixels), for the composite's choice of GS colour
    const uint64_t xyoff = g.reg[0x18 + c];
    OvBox b = {1e9f, 1e9f, -1e9f, -1e9f};
    for (int i = 0; i < n; ++i) {
        const float x = (float(v[i].x) - float(Bits(xyoff, 0, 16))) / 16.0f;
        const float y = (float(v[i].y) - float(Bits(xyoff, 32, 16))) / 16.0f;
        b.x0 = std::min(b.x0, x); b.y0 = std::min(b.y0, y); b.x1 = std::max(b.x1, x); b.y1 = std::max(b.y1, y);
    }
    auto& boxes = g_ov_box[fbp];
    // merged into the last box when they meet (strips, particle bursts): fewer boxes to test
    if (!boxes.empty()) {
        OvBox& l = boxes.back();
        if ((b.x0 <= l.x1 + 2 && l.x0 <= b.x1 + 2 && b.y0 <= l.y1 + 2 && l.y0 <= b.y1 + 2) || boxes.size() >= 4096) {
            l.x0 = std::min(l.x0, b.x0); l.y0 = std::min(l.y0, b.y0); l.x1 = std::max(l.x1, b.x1); l.y1 = std::max(l.y1, b.y1);
            g_ndefer++;
            return true;
        }
    }
    boxes.push_back(b);
    g_ndefer++;
    return true;
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
        if (Defer(g.vq, need, type)) {
            g_defer_kick = true;
            g_ndraw++;
            if (Tracing()) { g_trace_def = true; TraceDraw(path, g.vq, need); g_trace_def = false; }
        } else {
            Draw(path, g.vq, need, type);
        }
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
            g_rt_gen[dbp / 32] = xsgnative::MirrorGen();
            if (!g_copy_dst.empty() && g_copy_dst.front().frame != g_frame) g_copy_dst.clear();
            if (g_copy_dst.size() < 64) g_copy_dst.push_back({dbp, std::max(1u, dbw * 64 * rows * bpp / 256), g_frame});
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
    const size_t o = g_obuf.size();
    g_obuf.resize(o + 32);
    std::memcpy(g_obuf.data() + o, q, 32);
}
/* The ownership GS runs on its own thread: it has its own Vulkan device, and
   nothing on the main thread waits for it except the owner-plane reads
   (OwnSync).  Work items are GIF data (path 0-3), a flush (kOwnFlush) or a
   function (kOwnCall: the effect snapshot of its VRAM). */
constexpr uint32_t kOwnFlush = 0xff, kOwnCall = 0xfe;
struct OwnItem {
    uint32_t first;
    Bytes second;
    std::function<void()> fn;
    OwnItem(uint32_t p, Bytes&& d) : first(p), second(std::move(d)) {}
};
struct OwnWorker {
    std::thread th;
    std::mutex m;
    std::condition_variable cv, idle_cv;
    std::deque<OwnItem> q;
    std::vector<Bytes> pool;   // recycled buffers
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
    void Push(uint32_t path, Bytes& data) {
        Bytes next;
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
            q.emplace_back(kOwnCall, Bytes());
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
                g_kick_rw = true;
                Kick(path, Bits(v, 0, 16), Bits(v, 32, 16), Bits(hiw, 4, 24), !Bits(hiw, 47, 1));
                g_kick_rw = false;
                if (g_defer_kick) {   // deferred: the ownership GS takes the vertex without drawing (ADC)
                    g_defer_kick = false;
                    uint64_t h2;
                    std::memcpy(&h2, oq + 8, 8);
                    h2 |= 1ull << 47;
                    std::memcpy(oq + 8, &h2, 8);
                }
                break;
            case 0x5:
                g_kick_rw = true;
                Kick(path, Bits(v, 0, 16), Bits(v, 32, 16), uint32_t(hiw), !Bits(hiw, 47, 1));
                g_kick_rw = false;
                if (g_defer_kick) {
                    g_defer_kick = false;
                    uint64_t h2;
                    std::memcpy(&h2, oq + 8, 8);
                    h2 |= 1ull << 47;
                    std::memcpy(oq + 8, &h2, 8);
                }
                break;
            case 0xa: g.fog = Bits(hiw, 36, 8); break;
            case 0xe:
                g_kick_rw = (hiw & 0xff) == 0x04 || (hiw & 0xff) == 0x05;
                SetReg(path, uint32_t(hiw & 0xff), v);
                g_kick_rw = false;
                if (g_defer_kick) {   // XYZF2 / XYZ2 -> XYZF3 / XYZ3 for the ownership GS: no drawing
                    g_defer_kick = false;
                    const uint64_t h2 = (hiw & ~0xffull) | ((hiw & 0xff) + 8);
                    std::memcpy(oq + 8, &h2, 8);
                }
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
    const uint32_t a = xsggs::Addr(fbp, fbw, x, y, psm);
    const uint8_t* p = vram + a;
    if (psm == 0x02 || psm == 0x0A) {
        const uint32_t c = uint32_t(p[0]) | uint32_t(p[1]) << 8;
        return ((c & 31) << 3) | (((c >> 5) & 31) << 11) | (((c >> 10) & 31) << 19) | ((c & 0x8000) ? 0x80000000u : 0);
    }
    const uint32_t c = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    return psm == 0x01 ? (c & 0xffffff) | 0x80000000u : c;
}

uint32_t GsZ(const uint8_t* vram, uint32_t zbp, uint32_t fbw, uint32_t zpsm, uint32_t x, uint32_t y) {
    const uint32_t psm = 0x30 | (zpsm & 0xf);
    const uint32_t a = xsggs::Addr(zbp, fbw, x, y, psm);
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
    cl.sets[0].storage_buffer_mask = (1u << 0) | (1u << 4);   // 4: the batch table
    cl.sets[0].storage_image_mask = (1u << 1) | (1u << 2);
    cl.sets[0].sampled_image_mask = 1u << 3;
    for (int i = 0; i < 5; ++i) cl.sets[0].meta[i].array_size = 1;
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

void Gif(uint32_t path, const uint8_t* data, uint32_t len, ParallelGS::GSInterface& gs,
         void (*pre)(void* ctx, const uint8_t* q), void* ctx,
         uint32_t (*pre_image)(void* ctx, const uint8_t* q, uint32_t nq)) {
    path &= 3;
    PathParse& p = g_path[path];
    OwnPath(path);
    uint32_t start = 0;
    uint8_t oq[16];
    const uint64_t t0 = xsgprof::g_on ? xsgprof::Now() : 0;
    if (g_obuf.capacity() < g_obuf.size() + len)   // one qword out per qword in: no growth inside the loop
        g_obuf.reserve(std::max(g_obuf.capacity() * 2, g_obuf.size() + len + (256u << 10)));
    for (uint32_t i = 0; i + 16 <= len; i += 16) {
        // IMAGE data (texture uploads, most of the GIF bytes): nothing here reads it, so a run
        // goes through in one piece -- the upload mirror, then the ownership stream (#3: the
        // per-qword path was the largest single cost of xsg-gpu's thread in busy scenes).
        if (p.left && p.flg >= 2 && (!pre || pre_image)) {
            const uint32_t nq = std::min<uint32_t>(p.left, (len - i) / 16);
            if (nq > 1 && (!pre || pre_image(ctx, data + i, nq) == nq)) {
                const size_t o = g_obuf.size();
                g_obuf.resize(o + size_t(nq) * 16);
                std::memcpy(g_obuf.data() + o, data + i, size_t(nq) * 16);
                p.left -= nq;
                i += (nq - 1) * 16;
                if (!p.left && p.eop && path == 1 && g_in_native) { g_in_native = false; g_restore_after = true; }
                if (g_restore_after) {
                    g_restore_after = false;
                    if (g_own) {
                        OwnAd(0x1a, g_saved[0]);
                        OwnAd(0x3d, g_saved[1]);
                        OwnAd(0x0a, g_saved[2]);
                        OwnAd(0x45, g_saved[3]);
                    }
                }
                continue;
            }
        }
        if (pre) pre(ctx, data + i);
        Qword(path, p, data + i, oq);
        if (g_snap_req) {
            g_snap_req = false;
            if (i > start) gs.gif_transfer(path, data + start, i - start);
            start = i;
            OwnFlush(path);
            Snapshot(gs);
        }
        {
            const size_t o = g_obuf.size();
            g_obuf.resize(o + 16);   // (no zero fill: Bytes)
            std::memcpy(g_obuf.data() + o, oq, 16);
        }
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
        if (g_worker) { OwnHandOff(); Bytes none; g_worker->Push(kOwnFlush, none); }
        else g_own->flush();
    }
    if (frame % 600 == 0)
        Log("vsync %u: %llu deferred effect draws; refused: %llu not rewritable, %llu point/line, %llu texture, %llu record",
            frame, (unsigned long long)g_ndefer, (unsigned long long)g_nrefuse[0], (unsigned long long)g_nrefuse[1],
            (unsigned long long)g_nrefuse[2], (unsigned long long)g_nrefuse[3]);
    if (frame % 600 == 0)
        Log("vsync %u: %llu post phases (%llu cancelled by a later native batch, %llu not replayable)", frame,
            (unsigned long long)g_npost_frames, (unsigned long long)g_ncancel, (unsigned long long)g_nbad);
    g_frame = frame + 1;
}

const uint8_t* PreVram(uint32_t fbp) {
    auto it = g_post.find(fbp);
    // An effect the replay could not record is drawn from the GS frame as it ended (with what the
    // game drew after the effect started, the battle HUD among it), not from before it (#3).
    if (it == g_post.end() || !it->second.done || !it->second.snapped || it->second.bad) return nullptr;
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
    if (it != g_post.end() && it->second.done && it->second.snapped && !it->second.bad && g_own) {
        OwnSync();   // the snapshot may still be running on the ownership thread
        v = g_snap[sf.fbp].own.data();
    }
    if (!v) {
        OwnSync();
        xsgprof::Scope ps(xsgprof::OwnRead);
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
        for (uint32_t y = y0; y < y1; ++y) {
            const xsggs::Row row(sf.fbp, sf.fbw, y, sf.psm);
            for (uint32_t x = 0; x < sf.w; ++x) {
                const uint32_t c = xsggs::Pixel(v + row(x), sf.psm) & mask;
                o[size_t(y) * sf.w + x] = c == (m & mask) || c == (m2 & mask);
            }
        }
    });
    if (auto fr = g_fr_tris.find(sf.fbp); fr != g_fr_tris.end())
        for (const auto& tr : fr->second.tris) {   // pixel centres inside the triangle (either winding)
            const float x0 = tr[0], y0 = tr[1], x1 = tr[2], y1 = tr[3], x2 = tr[4], y2 = tr[5];
            const float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
            if (std::fabs(area) < 1e-6f) continue;
            const int bx0 = std::max(0, int(std::floor(std::min({x0, x1, x2})))), by0 = std::max(0, int(std::floor(std::min({y0, y1, y2}))));
            const int bx1 = std::min(int(sf.w) - 1, int(std::ceil(std::max({x0, x1, x2})))), by1 = std::min(int(sf.h) - 1, int(std::ceil(std::max({y0, y1, y2}))));
            const float s = area > 0 ? 1.0f : -1.0f;
            for (int py = by0; py <= by1; ++py)
                for (int px = bx0; px <= bx1; ++px) {
                    const float cx = px + 0.5f, cy = py + 0.5f;
                    const float w0 = s * ((x1 - cx) * (y2 - cy) - (x2 - cx) * (y1 - cy));
                    const float w1 = s * ((x2 - cx) * (y0 - cy) - (x0 - cx) * (y2 - cy));
                    const float w2 = s * ((x0 - cx) * (y1 - cy) - (x1 - cx) * (y0 - cy));
                    if (w0 >= 0 && w1 >= 0 && w2 >= 0) o[size_t(py) * sf.w + px] = 0;
                }
        }
    return true;
}

namespace {
void ReplayList(Vulkan::CommandBuffer& cmd, uint32_t fbp, PostFrame& pf, int s, uint32_t w, uint32_t h,
                const Vulkan::Image& color, const Vulkan::Image& z, uint32_t mx) {
    if (pf.prims.empty()) return;
    auto& dev = *g_dev;
    // Widescreen: a sprite over the whole width under a full-width scissor (fade, flash, the
    // defocus blur's frame reads) is stretched across the side bands.  A frame read keeps its
    // texel-per-pixel mapping (its UVs are extended, and read the bands of the copy);
    // other textures are stretched.  Everything else stays in the centre (its scissor).
    static std::vector<GpuPrim> wide;
    const GpuPrim* prims = pf.prims.data();
    if (mx) {
        wide = pf.prims;
        const float m = float(mx), fw = float(w);
        for (GpuPrim& p : wide) {
            if (p.kind[0] != 6 || p.sc[0] > 0 || p.sc[2] < int(w) - 1) continue;
            const int l = p.xy[0][0] <= p.xy[1][0] ? 0 : 1, r = 1 - l;
            if (p.xy[l][0] > 0.0f || p.xy[r][0] < fw - 1.0f) continue;   // (games draw to 511 as often as to 512)
            const float x0 = p.xy[l][0], x1 = p.xy[r][0];
            if (p.kind[2] == 2 && x1 > x0) {
                const float du = (p.xy[r][2] - p.xy[l][2]) / (x1 - x0);
                p.xy[l][2] += (-m - x0) * du;
                p.xy[r][2] += (fw + m - x1) * du;
            }
            p.xy[l][0] = -m;
            p.xy[r][0] = fw + m;
            p.sc[0] = -int(mx);
            p.sc[2] = int(w + mx) - 1;
        }
        prims = wide.data();
    }
    Vulkan::BufferCreateInfo bi = {};
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bi.domain = Vulkan::BufferDomain::LinkedDeviceHost;
    bi.size = pf.prims.size() * sizeof(GpuPrim);
    auto buf = dev.create_buffer(bi, prims);
    // The frame-read source: a copy of the surface, refreshed before each frame-reading primitive.
    Vulkan::ImageHandle copy = Storage((w + 2 * mx) * s, h * s, VK_FORMAT_R8G8B8A8_UNORM, nullptr);
    static Vulkan::ImageHandle white;
    if (!white) { const uint32_t px = 0xffffffffu; white = Storage(1, 1, VK_FORMAT_R8G8B8A8_UNORM, &px); }
    cmd.set_program(g_prog);
    // Rectangles written since the last barrier: a segment waits for the earlier ones only
    // where it overlaps them (GS order).  A barrier after every dispatch serialised effects of
    // thousands of small sprites (the battle transition's 8000 strips: 30-45 ms frames).
    struct Rect { int x0, y0, x1, y1; size_t prim; };
    // [debug] fx_batch = 0: one dispatch and a barrier per primitive (for A/B checks).
    // Exact overlap of two primitives' areas (1x space), separating axes: shapes that only touch
    // (a mesh's triangles sharing an edge; the top-left rule gives each edge pixel to one) do not.
    auto shape = [&](const GpuPrim& q, float (*v)[2]) -> int {
        if (q.kind[0] == 6) {
            // a sprite covers the whole GS pixels whose corner is in [mn, mx) (native_fx.comp): its
            // area is [ceil(mn), ceil(mx)) in pixel corners, which a pixel reaches half a pixel either side of
            const float x0 = std::ceil(std::min(q.xy[0][0], q.xy[1][0])) - 0.5f, x1 = std::ceil(std::max(q.xy[0][0], q.xy[1][0])) - 0.5f;
            const float y0 = std::ceil(std::min(q.xy[0][1], q.xy[1][1])) - 0.5f, y1 = std::ceil(std::max(q.xy[0][1], q.xy[1][1])) - 0.5f;
            v[0][0] = x0; v[0][1] = y0; v[1][0] = x1; v[1][1] = y0; v[2][0] = x1; v[2][1] = y1; v[3][0] = x0; v[3][1] = y1;
            return 4;
        }
        if (q.kind[0] != 3) return 0;
        for (int k = 0; k < 3; ++k) v[k][0] = q.xy[k][0], v[k][1] = q.xy[k][1];
        return 3;
    };
    static const bool batch_on = !(xsgcfg::CStr("debug.fx_batch") && std::atoi(xsgcfg::CStr("debug.fx_batch")) == 0);
    auto overlaps = [&](const GpuPrim& a, const GpuPrim& b) {
        if (!batch_on) return true;
        float va[4][2], vb[4][2];
        const int na = shape(a, va), nb = shape(b, vb);
        if (!na || !nb) return true;   // points, lines: by their rectangles
        auto separated = [&](float (*v)[2], int n) {
            for (int e = 0; e < n; ++e) {
                const float ax = -(v[(e + 1) % n][1] - v[e][1]), ay = v[(e + 1) % n][0] - v[e][0];
                if (ax == 0 && ay == 0) continue;
                float amin = 1e30f, amax = -1e30f, bmin = 1e30f, bmax = -1e30f;
                for (int k = 0; k < na; ++k) { const float d = va[k][0] * ax + va[k][1] * ay; amin = std::min(amin, d); amax = std::max(amax, d); }
                for (int k = 0; k < nb; ++k) { const float d = vb[k][0] * ax + vb[k][1] * ay; bmin = std::min(bmin, d); bmax = std::max(bmax, d); }
                const float eps = 1e-4f * (std::fabs(ax) + std::fabs(ay));
                if (amax <= bmin + eps || bmax <= amin + eps) return true;
            }
            return false;
        };
        return !separated(va, na) && !separated(vb, nb);
    };
    static std::vector<Rect> pending;
    pending.clear();
    // A segment: consecutive primitives with the same source, drawn by one binned dispatch (mode -3):
    // the surface in 64x64 bins, each 8x8 tile's workgroup running its samples through every
    // primitive whose box touches it in GS order, so overlap inside a segment needs no barrier.  Splitting at every
    // overlap (one dispatch and a barrier each) made a cutscene's 1200 overlapping primitives 1150
    // serialised dispatches (#3).  Between segments a barrier only where they overlap.
    struct SegE { uint32_t prim; int x0, y0, x1, y1; uint32_t zmax; };
    static std::vector<SegE> seg;
    seg.clear();
    const Vulkan::Image* seg_src = nullptr;
    bool seg_ov = false;   // primitives of the segment overlap each other
    const int W = int((w + 2 * mx) * s), H = int(h * s);
    const int ntx = (W + 63) / 64, nty = (H + 63) / 64;
    static std::vector<uint32_t> tcount, tfirst, touched;
    tcount.assign(size_t(ntx) * nty, 0);
    tfirst.resize(size_t(ntx) * nty);
    static std::vector<uint32_t> rows;   // uvec4s: bin headers, then the lists
    size_t prof_frame = 0, prof_tex = 0, prof_disp = 0, prof_barriers = 0;
    auto barrier = [&] {
        cmd.barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        pending.clear();
        prof_barriers++;
    };
    auto bind = [&](const Vulkan::Image* src) {
        cmd.set_storage_buffer(0, 0, *buf);
        cmd.set_storage_texture(0, 1, color.get_view());
        cmd.set_storage_texture(0, 2, z.get_view());
        cmd.set_texture(0, 3, src->get_view(), Vulkan::StockSampler::NearestClamp);
    };
    struct PC { int32_t prim, s, w, h, ox, oy, mx, pad1; uint32_t pmode, bg, zmax, pad2; };
    // one binned dispatch of seg[b, e): the lists per 64x64 bin (native_fx.comp, mode -3)
    auto draw_binned = [&](size_t b, size_t e) {
        constexpr int T = 64;
        const int ntx = (W + T - 1) / T;
        touched.clear();
        size_t n = 0;
        for (size_t k = b; k < e; ++k) {
            const SegE& q = seg[k];
            for (int ty = q.y0 / T; ty <= (q.y1 - 1) / T; ++ty)
                for (int tx = q.x0 / T; tx <= (q.x1 - 1) / T; ++tx) {
                    uint32_t& c = tcount[size_t(ty) * ntx + tx];
                    if (!c++) touched.push_back(uint32_t(ty * ntx + tx));
                    n++;
                }
        }
        rows.resize((touched.size() + n) * 4);
        uint32_t at = uint32_t(touched.size());
        for (size_t i = 0; i < touched.size(); ++i) {
            const uint32_t t = touched[i];
            uint32_t* r = &rows[i * 4];
            r[0] = (t % ntx) * T; r[1] = (t / ntx) * T; r[2] = at; r[3] = tcount[t];
            tfirst[t] = at;
            at += tcount[t];
            tcount[t] = 0;
        }
        for (size_t k = b; k < e; ++k) {
            const SegE& q = seg[k];
            const uint32_t b0 = uint32_t(q.x0) | uint32_t(q.y0) << 16, b1 = uint32_t(q.x1) | uint32_t(q.y1) << 16;
            for (int ty = q.y0 / T; ty <= (q.y1 - 1) / T; ++ty)
                for (int tx = q.x0 / T; tx <= (q.x1 - 1) / T; ++tx) {
                    uint32_t* r = &rows[size_t(tfirst[size_t(ty) * ntx + tx]++) * 4];
                    r[0] = q.prim; r[1] = b0; r[2] = b1; r[3] = q.zmax;
                }
        }
        Vulkan::BufferCreateInfo tb = {};
        tb.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        tb.domain = Vulkan::BufferDomain::Host;
        tb.size = rows.size() * 4;
        auto tbl = dev.create_buffer(tb, nullptr);
        std::memcpy(dev.map_host_buffer(*tbl, Vulkan::MEMORY_ACCESS_WRITE_BIT), rows.data(), tb.size);
        dev.unmap_host_buffer(*tbl, Vulkan::MEMORY_ACCESS_WRITE_BIT);
        cmd.set_storage_buffer(0, 4, *tbl);
        const uint32_t nb = uint32_t(touched.size()), nt = nb * 64;
        const PC pc = {-3, s, int32_t(w), int32_t(h), 0, int32_t(nb), int32_t(mx), 0, 0, 0, 0, 0};
        cmd.push_constants(&pc, 0, sizeof pc);
        cmd.dispatch(std::min(nt, 65535u), (nt + 65534) / 65535, 1);
        prof_disp++;
    };
    auto draw_one = [&](const SegE& q) {
        static Vulkan::BufferHandle dummy;
        if (!dummy) {
            Vulkan::BufferCreateInfo tb = {};
            tb.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            tb.domain = Vulkan::BufferDomain::Device;
            tb.size = 16;
            dummy = dev.create_buffer(tb, nullptr);
        }
        cmd.set_storage_buffer(0, 4, *dummy);
        const PC pc = {int32_t(q.prim), s, int32_t(w), int32_t(h), q.x0, q.y0, int32_t(mx), 0, 0, 0, q.zmax, 0};
        cmd.push_constants(&pc, 0, sizeof pc);
        cmd.dispatch(uint32_t(q.x1 - q.x0 + 7) / 8, uint32_t(q.y1 - q.y0 + 7) / 8, 1);
        prof_disp++;
    };
    // [debug] fx_one_by_one: segments up to this size get a dispatch per primitive (was 32; the
    // soft-edge pass's segments of 5-30 small triangles were ~1800 dispatches a frame, #3)
    static const size_t kOneByOne = [] {
        const char* s = xsgcfg::CStr("debug.fx_one_by_one");
        return s && *s ? size_t(std::atoi(s)) : size_t(4);
    }();
    auto flush_seg = [&] {
        if (seg.empty()) return;
        // a barrier first when the segment overlaps what was drawn since the last one
        bool hit = pending.size() * seg.size() > 400000;
        for (size_t k = 0; k < seg.size() && !hit; ++k) {
            const SegE& a = seg[k];
            for (const Rect& q : pending)
                if (a.x0 < q.x1 && q.x0 < a.x1 && a.y0 < q.y1 && q.y0 < a.y1 && overlaps(prims[a.prim], prims[q.prim])) { hit = true; break; }
        }
        if (hit) barrier();
        bind(seg_src);
        if (!batch_on) {   // [debug] fx_batch = 0: a dispatch and a barrier per primitive
            for (size_t k = 0; k < seg.size(); ++k) {
                if (k) barrier();
                draw_one(seg[k]);
            }
        } else if (!seg_ov && seg.size() <= kOneByOne) {   // a few: a dispatch each; more: one binned dispatch
            for (const SegE& q : seg) draw_one(q);
        } else {
            draw_binned(0, seg.size());
        }
        for (const SegE& q : seg) pending.push_back({q.x0, q.y0, q.x1, q.y1, q.prim});
        if (pending.size() > 2048) barrier();
        seg.clear();
        seg_src = nullptr;
        seg_ov = false;
    };
    cmd.set_program(g_prog);
    // what `copy` lacks of `color`: everything at first, then what was drawn since the last copy
    static std::vector<std::array<int, 4>> drects;   // drawn since their last copy (s x samples)
    drects.assign(1, {0, 0, W, H});                   // the copy starts empty
    const GpuPrim* last_read = nullptr;   // the frame read just before (a run of one draw)
    for (size_t i = 0; i < pf.prims.size(); ++i) {
        const GpuPrim& p = prims[i];
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
        // Bounding box in s x samples: the samples it can write lie in [bx0, bx1) x [by0, by1) (a
        // conservative box); whether two primitives whose boxes meet really overlap is decided on
        // their shapes (overlaps())
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        const int nv = p.kind[0] == 6 ? 2 : 3;
        for (int k = 0; k < nv; ++k) {
            x0 = std::min(x0, p.xy[k][0]); y0 = std::min(y0, p.xy[k][1]);
            x1 = std::max(x1, p.xy[k][0]); y1 = std::max(y1, p.xy[k][1]);
        }
        x0 += float(mx);
        x1 += float(mx);
        const int bx0 = std::max(0, int(std::floor(x0 * s)) - 1), by0 = std::max(0, int(std::floor(y0 * s)) - 1);
        const int bx1 = std::min(W, int(std::ceil(x1 * s)) + s + 1);
        const int by1 = std::min(H, int(std::ceil(y1 * s)) + s + 1);
        if (bx1 <= bx0 || by1 <= by0) continue;
        const uint32_t zmax = p.fb[3] == 0 ? 0xffffffffu : (p.fb[3] == 1 ? 0xffffffu : 0xffffu);
        const SegE q = {uint32_t(i), bx0, by0, bx1, by1, zmax};
        prof_frame += p.kind[2] == 2;
        prof_tex += p.kind[2] == 1;
        // A frame read whose sampled area (its UV box, in s x samples, plus the filter's reach)
        // misses everything drawn since the last copy reads an up-to-date copy as it is: no
        // flush, copy or barriers.  The cutscene glows read the frame in hundreds of small
        // pieces that do not touch each other (#3: ev1080 / cf0540 post 7-18 ms a frame).
        // The copy is brought up to date only where this read samples: the rectangles drawn since
        // their last copy that meet its sampled box (in s x samples, plus the filter's reach) are
        // copied, clipped to the box.  The soft-edge pass reads the frame ~1500 times a frame,
        // each right after its mask; copying the union of everything drawn since the last copy
        // moved megabytes per read (#3: ev1080 / cf0540 post 7-18 ms a frame on an RTX 5060).
        int sb[4] = {0, 0, W, H};
        bool boxed = false;
        if (p.kind[2] == 2) {
            float u0 = 1e9f, v0 = 1e9f, u1 = -1e9f, v1 = -1e9f;
            const bool fst = Bits(p.kind[1], 8, 1) != 0u;
            boxed = true;
            for (int k = 0; k < nv && boxed; ++k) {
                float u = p.xy[k][2], v = p.xy[k][3];
                if (!fst) {   // STQ: s/q, t/q times the texture size (perspective-correct values stay in the hull)
                    const float q = nv == 2 ? p.q[1] : p.q[k];
                    if (!(q > 1e-12f) || !std::isfinite(q)) { boxed = false; break; }
                    u = u / q * float(p.tsz[0]);
                    v = v / q * float(p.tsz[1]);
                }
                if (!std::isfinite(u) || !std::isfinite(v)) { boxed = false; break; }
                u0 = std::min(u0, u); v0 = std::min(v0, v); u1 = std::max(u1, u); v1 = std::max(v1, v);
            }
            if (boxed) {
                sb[0] = std::max(0, int(std::floor((u0 + float(mx) - 2.0f) * s)));
                sb[1] = std::max(0, int(std::floor((v0 - 2.0f) * s)));
                sb[2] = std::min(W, int(std::ceil((u1 + float(mx) + 2.0f) * s)));
                sb[3] = std::min(H, int(std::ceil((v1 + 2.0f) * s)));
                if (sb[2] <= sb[0] || sb[3] <= sb[1]) { sb[0] = sb[1] = 0; sb[2] = sb[3] = 0; }
            }
        }
        // The triangles of one frame-reading draw (a strip, the same texture and blend state as
        // the read before) read the frame as it was when the draw began: on the GS they go
        // through the texture cache, which sees the frame's new pixels only after a TEXFLUSH.
        // The soft-edge strips read pixels their neighbours just wrote, and a copy per triangle
        // was ~1500 copies and dispatches a frame in the cutscenes (#3).  [debug] fx_read_runs = 0
        // refreshes before every triangle instead.
        static const bool read_runs = !(xsgcfg::CStr("debug.fx_read_runs") && std::atoi(xsgcfg::CStr("debug.fx_read_runs")) == 0);
        const bool same_run = read_runs && p.kind[2] == 2 && last_read && last_read->kind[1] == p.kind[1] &&
                              std::memcmp(last_read->tex, p.tex, sizeof p.tex) == 0 &&
                              std::memcmp(last_read->st, p.st, sizeof p.st) == 0 && std::memcmp(last_read->fb, p.fb, sizeof p.fb) == 0;
        last_read = p.kind[2] == 2 ? &p : nullptr;
        bool stale = false;
        if (p.kind[2] == 2 && !same_run)
            for (const auto& d : drects)
                if (d[0] < sb[2] && sb[0] < d[2] && d[1] < sb[3] && sb[1] < d[3]) { stale = true; break; }
        if (p.kind[2] == 2 && !stale) {
            src = copy.get();
        } else if (p.kind[2] == 2) {
            flush_seg();
            cmd.barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
            const VkImageSubresourceLayers sub = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            if (!boxed || drects.size() > 64) {   // the union, as before
                int u[4] = {INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN};
                for (const auto& d : drects) { u[0] = std::min(u[0], d[0]); u[1] = std::min(u[1], d[1]); u[2] = std::max(u[2], d[2]); u[3] = std::max(u[3], d[3]); }
                if (u[2] > u[0] && u[3] > u[1])
                    cmd.copy_image(*copy, color, {u[0], u[1], 0}, {u[0], u[1], 0}, {uint32_t(u[2] - u[0]), uint32_t(u[3] - u[1]), 1}, sub, sub);
                drects.clear();
            } else {   // every rectangle drawn since its last copy (a group's masks, all at once)
                for (const auto& d : drects)
                    if (d[2] > d[0] && d[3] > d[1])
                        cmd.copy_image(*copy, color, {d[0], d[1], 0}, {d[0], d[1], 0}, {uint32_t(d[2] - d[0]), uint32_t(d[3] - d[1]), 1}, sub, sub);
                drects.clear();
            }
            cmd.barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
            pending.clear();
            src = copy.get();
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
        if (!drects.empty()) {   // merged into the last rectangle when it meets it
            auto& l = drects.back();
            if (bx0 <= l[2] && l[0] <= bx1 && by0 <= l[3] && l[1] <= by1) {
                l = {std::min(l[0], bx0), std::min(l[1], by0), std::max(l[2], bx1), std::max(l[3], by1)};
            } else {
                drects.push_back({bx0, by0, bx1, by1});
            }
        } else {
            drects.push_back({bx0, by0, bx1, by1});
        }
        // An untextured primitive samples nothing: it joins the open segment whatever that binds
        // (the soft-edge pass alternates untextured alpha masks with frame reads, and a new
        // segment per primitive was ~1800 dispatches a frame in the cutscenes, #3)
        const bool untex = Bits(p.kind[1], 4, 1) == 0u;
        if (untex && seg_src) src = seg_src;
        if (src != seg_src) flush_seg();
        seg_src = src;
        // (only decides between separate dispatches and binning, which a segment past 32 gets anyway)
        for (size_t k = 0; k < seg.size() && !seg_ov && seg.size() < 32; ++k) {
            const SegE& a = seg[k];
            seg_ov = a.x0 < q.x1 && q.x0 < a.x1 && a.y0 < q.y1 && q.y0 < a.y1 && overlaps(p, prims[a.prim]);
        }
        seg.push_back(q);
        if (p.kind[2] == 2 && stale) flush_seg();   // the next frame read needs it in the copy
    }
    flush_seg();
    if (xsgprof::g_on && pf.prims.size() >= 500)
        Log("[prof] replay fbp %u: %zu prims (frame-read %zu, textured %zu), %zu dispatches, %zu barriers", fbp, pf.prims.size(),
            prof_frame, prof_tex, prof_disp, prof_barriers);
}
}  // namespace

/* The deferred effects of the frame the buffer last showed (g_ov), or null. */
static PostFrame* OverlayOf(uint32_t fbp) {
    auto it = g_ov.find(fbp);
    auto pt = g_post.find(fbp);
    if (it == g_ov.end() || pt == g_post.end() || it->second.prims.empty()) return nullptr;
    if (it->second.nat_frame != pt->second.nat_frame) return nullptr;
    return &it->second;
}

bool Overlay(uint32_t fbp) { return OverlayOf(fbp) != nullptr; }

namespace {
/* The texture lies in a copy the GS made of a buffer this frame (TRXDIR local -> local). */
bool OverlapsCopy(uint64_t tex0) {
    const uint32_t tbp = Bits(tex0, 0, 14);
    const uint32_t tw = std::min(10u, Bits(tex0, 26, 4)), th = std::min(10u, Bits(tex0, 30, 4));
    const uint32_t tblocks = std::max(1u, (1u << tw) * (1u << th) * 4 / 256);
    for (const CopyDst& d : g_copy_dst)
        if (d.frame == g_frame && tbp < d.bp + d.blocks && d.bp < tbp + tblocks) return true;
    return false;
}
}  // namespace

bool GsRenderedTexture(uint64_t tex0) {
    const uint32_t tbp = Bits(tex0, 0, 14);
    const uint32_t tw = std::min(10u, Bits(tex0, 26, 4)), th = std::min(10u, Bits(tex0, 30, 4));
    const uint32_t tblocks = std::max(1u, (1u << tw) * (1u << th) * 4 / 256);
    // GS-rendered while the page the GS drew into has not been uploaded over since (a large
    // atlas whose other pages are not re-uploaded every frame is not, after one effect).
    for (uint32_t d : g_rendered)
        if (Overlaps(tbp, tblocks, d * 32)) {
            auto it = g_rt_gen.find(d);
            if (it != g_rt_gen.end() && xsgnative::PageGen(d) <= it->second) return true;
        }
    return false;
}

void Replay(Vulkan::CommandBuffer& cmd, uint32_t fbp, int s, uint32_t w, uint32_t h, const Vulkan::Image& color,
            const Vulkan::Image& z, uint32_t mx) {
    if (PostFrame* ov = OverlayOf(fbp)) {   // the deferred effects first (they came before any post phase)
        ReplayList(cmd, fbp, *ov, s, w, h, color, z, mx);
        cmd.barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);
    }
    auto it = g_post.find(fbp);
    if (it == g_post.end() || !it->second.done || it->second.bad) return;
    ReplayList(cmd, fbp, it->second, s, w, h, color, z, mx);
}

void OverlayColour(const Surface& sf, std::vector<uint32_t>& gs) {
    PostFrame* ov = OverlayOf(sf.fbp);
    if (!ov || !g_own || gs.size() < size_t(sf.w) * sf.h) return;
    const uint8_t* v = nullptr;
    auto it = g_post.find(sf.fbp);
    if (it != g_post.end() && it->second.done && it->second.snapped) {
        OwnSync();
        v = g_snap[sf.fbp].own.data();
    }
    if (!v) {
        OwnSync();
        xsgprof::Scope ps(xsgprof::OwnRead);
        g_own->flush();
        v = static_cast<const uint8_t*>(g_own->map_vram_read(0, 4u << 20));
    }
    const uint32_t m = MarkerOut(), m2 = kMarker;
    const bool c16 = sf.psm == 0x02 || sf.psm == 0x0a;
    const uint32_t mask = c16 ? 0xf8f8f8u : 0xffffffu;
    static std::vector<uint8_t> veto;
    veto.assign(size_t(sf.w) * sf.h, 0);
    const size_t n = size_t(sf.w) * sf.h;
    const bool zplane = gs.size() >= 2 * n;   // DecodeGs: colour, then Z
    if (std::getenv("XSG_FX_OVLOG")) {
        static int n;
        if (n++ % 120 == 0) {
            Log("overlay fbp %#x frame %u: %zu prims, %zu boxes, %zu veto (veto frame %u)", sf.fbp, ov->nat_frame, ov->prims.size(),
                g_ov_box[sf.fbp].size(), g_ov_veto[sf.fbp].boxes.size(), g_ov_veto[sf.fbp].frame);
        }
    }
    if (g_ov_veto[sf.fbp].frame == ov->nat_frame)
        for (const OvBox& b : g_ov_veto[sf.fbp].boxes) {
            const int x0 = std::max(0, int(std::floor(b.x0)) - 1), y0 = std::max(0, int(std::floor(b.y0)) - 1);
            const int x1 = std::min(int(sf.w), int(std::ceil(b.x1)) + 2), y1 = std::min(int(sf.h), int(std::ceil(b.y1)) + 2);
            for (int y = y0; y < y1; ++y) std::memset(veto.data() + size_t(y) * sf.w + x0, 1, size_t(std::max(0, x1 - x0)));
        }
    // the boxes' pixels, each once (the boxes overlap), a row at a time
    static std::vector<uint8_t> mark;
    mark.assign(size_t(sf.w) * sf.h, 0);
    int ux0 = int(sf.w), uy0 = int(sf.h), ux1 = 0, uy1 = 0;
    for (const OvBox& b : g_ov_box[sf.fbp]) {
        const int x0 = std::max(0, int(std::floor(b.x0)) - 1), y0 = std::max(0, int(std::floor(b.y0)) - 1);
        const int x1 = std::min(int(sf.w), int(std::ceil(b.x1)) + 2), y1 = std::min(int(sf.h), int(std::ceil(b.y1)) + 2);
        if (x1 <= x0 || y1 <= y0) continue;
        for (int y = y0; y < y1; ++y) std::memset(mark.data() + size_t(y) * sf.w + x0, 1, size_t(x1 - x0));
        ux0 = std::min(ux0, x0); uy0 = std::min(uy0, y0); ux1 = std::max(ux1, x1); uy1 = std::max(uy1, y1);
    }
    const uint32_t zpsm = 0x30 | (sf.zpsm & 0xf);
    if (uy1 <= uy0) return;
    xsgnative::ParallelRows(uint32_t(uy1 - uy0), [&](uint32_t r0, uint32_t r1) {
    for (int y = uy0 + int(r0); y < uy0 + int(r1); ++y) {
        const xsggs::Row row(sf.fbp, sf.fbw, uint32_t(y), sf.psm), zrow(sf.zbp, sf.fbw, uint32_t(y), zpsm);
        for (int x = ux0; x < ux1; ++x) {
            const size_t i = size_t(y) * sf.w + x;
            if (!mark[i] || veto[i]) continue;   // veto: a GS-kept blend here: the main GS's colour
            const uint32_t c = xsggs::Pixel(v + row(uint32_t(x)), sf.psm);
            if ((c & mask) == (m & mask) || (c & mask) == (m2 & mask)) continue;   // native: the composite's anyway
            gs[i] = c;
            // and its depth: the main GS's Z holds the deferred draws' own Z, so a replayed
            // draw failed its own Z test against it (menu text: each glyph is drawn with Z
            // one above its shadow, GREATER; the glyph failed and the 1x text showed through,
            // dark under the replayed shadows)
            if (zplane) {
                const uint8_t* p = v + zrow(uint32_t(x));
                uint32_t z = uint32_t(p[0]) | uint32_t(p[1]) << 8;
                if (zpsm != 0x32 && zpsm != 0x3a) {
                    z |= uint32_t(p[2]) << 16;
                    if (zpsm == 0x30) z |= uint32_t(p[3]) << 24;
                }
                gs[n + i] = z;
            }
        }
    }
    });
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
    cmd.set_storage_buffer(0, 4, *buf);   // the batch table (unused here)
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

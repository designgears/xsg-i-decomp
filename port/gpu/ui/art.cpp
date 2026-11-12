/* Rebuilt UI: the game's own art, drawn by ImGui.  See art.h. */
#include "art.h"

#include "../menu/menu_render.h"
#include "../menu/cfg.h"
#include "../native.h"
#include "../native_prof.h"
#include "gs_interface.hpp"
#include "ps2host/gs.h"

#include <future>
#include <memory>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>
extern "C++" unsigned char* xsgui_stbi_load(const char* path, int* w, int* h, int* n, int want);
extern "C++" void xsgui_stbi_free(unsigned char* p);
#include <cstring>
#include <unordered_map>
#include <vector>

namespace xsgui {
namespace {
ParallelGS::GSInterface* g_gs;
uint32_t g_frame;
uint64_t g_decode_ns;   // [debug] gpu_prof: art decode time this frame
uint32_t g_decodes;

struct Tex {
    uint64_t id = 0;
    uint32_t w = 0, h = 0;
    uint32_t gen = ~0u;        // the mirror's upload generation it was decoded at
    uint32_t checked = 0;      // the frame of the last check
    uint64_t hash = 0;
    uint32_t set = 0;
    uint64_t image = 0;        // TEX0's image bits (ImageBits): a set's snapshot invalidates only its own image
    uint32_t ticket = 0;       // the latest upscale started for it (an older one finishing is dropped)
};
struct Upscaling { uint64_t key; uint32_t ticket, w, h; std::future<std::vector<uint32_t>> px; };
std::vector<Upscaling> g_upscales;
std::unordered_map<uint64_t, Tex> g_tex;   // key: TEX0 (+ TEXA) (+ set)
std::unordered_map<uint32_t, std::unique_ptr<ps2host::gs::Memory>> g_sets;   // texture sets (GPU_MSG_UI_TEX)
uint32_t g_set_gen;
std::vector<uint32_t> g_set_order;   // oldest first; at most kSets kept (4 MB each)
constexpr size_t kSets = 6;

uint64_t Fnv(const uint32_t* p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

// the parts of TEX0 that name the image: TBP0 TBW PSM TW TH, CBP CPSM CSM CSA
uint64_t ImageBits(uint64_t tex0) { return (tex0 & 0x3fffffffull) | (tex0 & (0x7ffffull << 37)); }

// PS2 colours: 0x80 = 1.0; alpha 0x80 = opaque
void Expand(std::vector<uint32_t>& px) {
    for (uint32_t& c : px) {
        const uint32_t a = std::min(255u, (c >> 24) * 2);
        c = (c & 0x00ffffffu) | (a << 24);
    }
}
}  // namespace

void ArtInit(ParallelGS::GSInterface* gs) { g_gs = gs; }

void ArtTexset(uint32_t key, uint32_t entries, const void* payload, uint32_t len) {
    struct E { uint32_t dbp, dbw, w, h, words; };
    if (len < entries * sizeof(E)) return;
    auto& m = g_sets[key];
    if (!m) {
        m = std::make_unique<ps2host::gs::Memory>();
        g_set_order.push_back(key);
        while (g_set_order.size() > kSets) {
            const uint32_t old = g_set_order.front();
            g_set_order.erase(g_set_order.begin());
            g_sets.erase(old);
        }
    }
    const E* e = static_cast<const E*>(payload);
    const uint32_t* px = reinterpret_cast<const uint32_t*>(e + entries);
    const uint32_t* end = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(payload) + len);
    for (uint32_t i = 0; i < entries; ++i) {
        if (px + e[i].words > end) break;
        for (uint32_t y = 0; y < e[i].h; ++y)
            m->WriteRun(e[i].dbp, e[i].dbw, 0, y, ps2host::gs::PSMCT32, px + size_t(y) * e[i].w, e[i].w);
        px += e[i].words;
    }
    ++g_set_gen;
    // decoded pages of dropped sets go; this set's re-decode on next use.  The other sets keep
    // theirs: re-decoding (and upscaling) every set's art on each arrival stalled a frame for
    // ~80 ms at a battle attack.
    for (auto it = g_tex.begin(); it != g_tex.end();) {
        if (it->second.set && !g_sets.count(it->second.set)) { it = g_tex.erase(it); continue; }
        if (it->second.set == key) it->second.gen = ~0u;
        ++it;
    }
}
void ArtSnapshot(uint32_t slot) {
    // TBP 0x3C00..0x4000: the menu texture (PSMT8 1024x256) and its CLUTs, the last 256 KB
    constexpr size_t kOff = size_t(0x3C00) * 256, kLen = (4u << 20) - kOff;
    const uint8_t* src = xsgnative::MirrorData() + kOff;
    const uint64_t* q = reinterpret_cast<const uint64_t*>(src);
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < kLen / 8; ++i) h = (h ^ q[i]) * 1099511628211ull;
    const uint32_t key = 0x5A000000u + slot;
    static std::unordered_map<uint32_t, uint64_t> last;
    auto& m = g_sets[key];
    if (m && last[key] == h) return;
    if (!m) m = std::make_unique<ps2host::gs::Memory>();
    std::memcpy(m->data() + kOff, src, kLen);
    last[key] = h;
    if (const char* t = xsgcfg::CStr("debug.art_dump"); t && *t) {   // TEX0 (hex, comma list) decoded from the snapshot -> raw RGBA
        static int n;
        for (const char* q = t; *q && n < 40;) {
            char* end;
            const uint64_t tex0 = std::strtoull(q, &end, 16);
            if (end == q) break;
            uint32_t tw = 0, th = 0;
            std::vector<uint32_t> px = xsgnative::DecodeVramTexture(m->data(), tex0, 0, &tw, &th);
            char name[256];
            std::snprintf(name, sizeof name, "/tmp/art_%02d_%llx_%ux%u.rgba", n++, (unsigned long long)tex0, tw, th);
            if (FILE* f = std::fopen(name, "wb")) { std::fwrite(px.data(), 4, px.size(), f); std::fclose(f); }
            q = *end ? end + 1 : end;
        }
    }
    for (auto& [k, t] : g_tex)
        if (t.set == key) t.gen = ~0u;
}

void ArtSnapshotTex(uint32_t key, uint64_t tex0, const uint8_t* mem) {
    auto bits = [&](int lo, int n) { return uint32_t((tex0 >> lo) & ((1ull << n) - 1)); };
    const uint32_t tbp = bits(0, 14), psm = bits(20, 6), tw = 1u << std::min(10u, bits(26, 4)), th = 1u << std::min(10u, bits(30, 4));
    const uint32_t bpp = psm == 0x14 ? 4 : psm == 0x13 ? 8 : (psm == 0x02 || psm == 0x0a) ? 16 : 32;   // T4, T8, CT16(S), CT32/24
    // a page is 8 KB whatever the format; the image spans tw * th * bpp / 8 bytes from TBP (whole pages)
    const size_t off = size_t(tbp) * 256, len = std::min<size_t>((size_t(tw) * th * bpp / 8 + 8191) & ~size_t(8191), (4u << 20) - off);
    const size_t coff = size_t(bits(37, 14)) * 256, clen = std::min<size_t>(8192, (4u << 20) - coff);
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < len / 8; ++i) h = (h ^ reinterpret_cast<const uint64_t*>(mem + off)[i]) * 1099511628211ull;
    for (size_t i = 0; i < clen / 8; ++i) h = (h ^ reinterpret_cast<const uint64_t*>(mem + coff)[i]) * 1099511628211ull;
    h ^= tex0 * 0x9e3779b97f4a7c15ull;
    // per image: a set can hold several (the battle's key guide alternates two CLUTs each frame),
    // and a snapshot of one must not throw away the others' decodes
    static std::unordered_map<uint64_t, uint64_t> last;   // (set, image) -> content hash
    const uint64_t image = ImageBits(tex0), lk = image ^ (uint64_t(key) * 0xc2b2ae3d27d4eb4full);
    auto& m = g_sets[key];
    if (m && last[lk] == h) return;
    if (!m) m = std::make_unique<ps2host::gs::Memory>();
    std::memcpy(m->data() + off, mem + off, len);
    std::memcpy(m->data() + coff, mem + coff, clen);
    last[lk] = h;
    for (auto& [k, t] : g_tex)
        if (t.set == key && t.image == image) t.gen = ~0u;
}

namespace {
std::unordered_map<uint32_t, ImTextureID> g_images;
}

// Catmull-Rom upscale by S, then a light unsharp mask on colour; src: RGBA8 (alpha kept)
std::vector<uint32_t> Upscale(const uint32_t* src32, uint32_t w, uint32_t h, uint32_t S, bool opaque) {
    const uint8_t* src = reinterpret_cast<const uint8_t*>(src32);
    const uint32_t W = w * S, H = h * S;
    auto px = [&](int x, int y, int c) -> float {
        x = std::clamp(x, 0, int(w) - 1);
        y = std::clamp(y, 0, int(h) - 1);
        return (c == 3 && opaque) ? 255.0f : float(src[(size_t(y) * w + x) * 4 + c]);
    };
    auto cub = [](float t, float a, float b, float c, float d) {
        return b + 0.5f * t * (c - a + t * (2 * a - 5 * b + 4 * c - d + t * (3 * (b - c) + d - a)));
    };
    std::vector<float> up(size_t(W) * H * 4);
    for (uint32_t y = 0; y < H; ++y) {
        const float fy = (float(y) + 0.5f) / float(S) - 0.5f;
        const int iy = int(std::floor(fy));
        const float ty = fy - float(iy);
        for (uint32_t x = 0; x < W; ++x) {
            const float fx = (float(x) + 0.5f) / float(S) - 0.5f;
            const int ix = int(std::floor(fx));
            const float tx = fx - float(ix);
            for (int c = 0; c < 4; ++c) {
                float col[4];
                for (int j = 0; j < 4; ++j)
                    col[j] = cub(tx, px(ix - 1, iy - 1 + j, c), px(ix, iy - 1 + j, c), px(ix + 1, iy - 1 + j, c), px(ix + 2, iy - 1 + j, c));
                up[(size_t(y) * W + x) * 4 + c] = cub(ty, col[0], col[1], col[2], col[3]);
            }
        }
    }
    std::vector<uint32_t> out(size_t(W) * H);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            uint32_t o = 0;
            for (int c = 0; c < 4; ++c) {
                float v = up[(size_t(y) * W + x) * 4 + c];
                if (c < 3) {
                    float blur = 0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx) {
                            const uint32_t xx = uint32_t(std::clamp(int(x) + dx, 0, int(W) - 1)), yy = uint32_t(std::clamp(int(y) + dy, 0, int(H) - 1));
                            blur += up[(size_t(yy) * W + xx) * 4 + c];
                        }
                    v += 0.6f * (v - blur / 9.0f);
                }
                o |= uint32_t(std::clamp(v, 0.0f, 255.0f) + 0.5f) << (8 * c);
            }
            out[size_t(y) * W + x] = o;
        }
    return out;
}

/* The 4x upscale of a whole picture takes ~240 ms: it runs on a worker, and the picture already
   shown stays until it is done.  The game sends the title picture each time the title comes up:
   an unchanged picture is not upscaled again. */
namespace {
struct PendingImage { uint32_t w, h; std::future<std::vector<uint32_t>> px; };
std::unordered_map<uint32_t, PendingImage> g_pending;
std::unordered_map<uint32_t, uint64_t> g_image_hash;
}  // namespace

void ArtImageSet(uint32_t id, uint32_t w, uint32_t h, const void* rgba, uint32_t len) {
    if (!w || !h || len < w * h * 4) return;
    uint64_t hash = 0xcbf29ce484222325ull ^ (uint64_t(w) << 32 | h);
    const uint64_t* q = static_cast<const uint64_t*>(rgba);
    for (uint32_t i = 0; i < w * h / 2; ++i) hash = (hash ^ q[i]) * 0x100000001b3ull;
    auto old = g_image_hash.find(id);
    if (old != g_image_hash.end() && old->second == hash) return;
    g_image_hash[id] = hash;
    constexpr uint32_t S = 4;
    std::vector<uint32_t> src(static_cast<const uint32_t*>(rgba), static_cast<const uint32_t*>(rgba) + size_t(w) * h);
    g_pending[id] = PendingImage{w * S, h * S, std::async(std::launch::async, [src = std::move(src), w, h] {
                                     return Upscale(src.data(), w, h, S, true);
                                 })};
}

static void UpscalesPoll() {
    for (size_t i = 0; i < g_upscales.size();) {
        Upscaling& u = g_upscales[i];
        if (u.px.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { ++i; continue; }
        const std::vector<uint32_t> out = u.px.get();
        auto it = g_tex.find(u.key);
        if (it != g_tex.end() && it->second.ticket == u.ticket && it->second.id)
            xsgmenu::RenderTexture(it->second.id, out.data(), u.w, u.h);
        g_upscales.erase(g_upscales.begin() + ptrdiff_t(i));
    }
}

static void ImagesPoll() {
    for (auto it = g_pending.begin(); it != g_pending.end();) {
        if (it->second.px.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { ++it; continue; }
        const std::vector<uint32_t> out = it->second.px.get();
        g_images[it->first] = ImTextureID(xsgmenu::RenderTexture(uint64_t(g_images[it->first]), out.data(), it->second.w, it->second.h));
        it = g_pending.erase(it);
    }
}

ImTextureID ArtOverride(const char* name, uint32_t* w, uint32_t* h) {
    struct O { ImTextureID id = 0; uint32_t w = 0, h = 0; bool tried = false; };
    static std::unordered_map<std::string, O> cache;
    O& o = cache[name];
    if (!o.tried) {
        o.tried = true;
        const char* xdg = std::getenv("XDG_CONFIG_HOME");
        const char* home = std::getenv("HOME");
        const std::string dir = xdg && *xdg ? std::string(xdg) + "/xsg/art" : std::string(home ? home : ".") + "/.config/xsg/art";
        int iw = 0, ih = 0, n = 0;
        if (unsigned char* px = xsgui_stbi_load((dir + "/" + name + ".png").c_str(), &iw, &ih, &n, 4)) {
            o.id = ImTextureID(xsgmenu::RenderTexture(0, reinterpret_cast<const uint32_t*>(px), uint32_t(iw), uint32_t(ih)));
            o.w = uint32_t(iw);
            o.h = uint32_t(ih);
            xsgui_stbi_free(px);
            std::fprintf(stderr, "[ui] art override %s: %dx%d\n", name, iw, ih);
        }
    }
    if (w) *w = o.w;
    if (h) *h = o.h;
    return o.id;
}

ImTextureID ArtImage(uint32_t id) {
    auto it = g_images.find(id);
    return it == g_images.end() ? ImTextureID(0) : it->second;
}

void ArtFrame() {
    if (xsgprof::g_on && g_decode_ns > 5000000)
        std::fprintf(stderr, "[ui] [prof] frame %u: %u art decodes, %.1f ms\n", g_frame, g_decodes, double(g_decode_ns) * 1e-6);
    g_decode_ns = 0;
    g_decodes = 0;
    ++g_frame;
    ImagesPoll();
    UpscalesPoll();
}

ImTextureID ArtTexture(uint64_t tex0, uint64_t texa, uint32_t* w, uint32_t* h, uint32_t texset) {
    const uint64_t key = ImageBits(tex0) ^ (texa * 0x9e3779b97f4a7c15ull) ^
                         (uint64_t(texset) * 0xc2b2ae3d27d4eb4full);
    Tex& t = g_tex[key];
    const uint64_t prof_t0 = xsgprof::g_on ? xsgprof::Now() : 0;
    struct Acc { uint64_t t0; ~Acc() { if (xsgprof::g_on) g_decode_ns += xsgprof::Now() - t0; } } acc{prof_t0};
    struct Log1 { const Tex& t; uint64_t t0; uint32_t n0, set; bool m; ~Log1() { if (xsgprof::g_on && g_decodes != n0 && xsgprof::Now() - t0 > 3000000)
        std::fprintf(stderr, "[ui] [prof]   decode set %08x mirror %d %ux%u: %.1f ms\n", set, m, t.w, t.h, double(xsgprof::Now() - t0) * 1e-6); } } log1{t, prof_t0, g_decodes, texset, xsgnative::On()};
    if (texset) {   // a texture set: decoded once per arrival
        auto it = g_sets.find(texset);
        if (it == g_sets.end()) return ImTextureID(0);
        t.set = texset;
        t.image = ImageBits(tex0);
        if (!t.id || t.gen == ~0u) {   // invalidated when its set changes (ArtTexset / ArtSnapshot*)
            uint32_t tw = 0, th = 0;
            ++g_decodes;
            std::vector<uint32_t> px = xsgnative::DecodeVramTexture(it->second->data(), tex0, texa, &tw, &th);
            if (!px.empty()) {
                Expand(px);
                // the menu's art (portraits, icons) upscaled 2x: smooth and sharp at high resolutions; uvs stay
                // normalised, so the bigger page needs no other change
                // (menu sets only: battle art changes per frame).  The upscale takes ~75 ms for the 1024x256
                // menu page: it runs on a worker, the plain page is shown until it is done.
                t.id = xsgmenu::RenderTexture(t.id, px.data(), tw, th);
                if (tw * th <= 1024u * 512u && (texset & 0xff000000u) == 0x5A000000u) {
                    const uint32_t ticket = ++t.ticket;
                    g_upscales.push_back({key, ticket, tw * 2, th * 2, std::async(std::launch::async, [px = std::move(px), tw, th] {
                                              return Upscale(px.data(), tw, th, 2, false);
                                          })});
                }
                t.w = tw;
                t.h = th;
            }
            t.gen = g_set_gen;
        }
        if (w) *w = t.w;
        if (h) *h = t.h;
        return ImTextureID(t.id);
    }
    const bool mirror = xsgnative::On();
    bool decode = t.id == 0;
    if (!decode && g_frame - t.checked >= (mirror ? 2u : 30u)) {
        t.checked = g_frame;
        decode = mirror ? xsgnative::TextureGen(tex0) != t.gen : true;
    }
    if (decode) {
        ++g_decodes;
        uint32_t tw = 0, th = 0;
        std::vector<uint32_t> px;
        uint32_t gen = 0;
        if (mirror) {
            gen = xsgnative::TextureGen(tex0);
            px = xsgnative::DecodeMirrorTexture(tex0, texa, &tw, &th);
        } else if (g_gs) {
            g_gs->flush();
            const uint8_t* vram = static_cast<const uint8_t*>(g_gs->map_vram_read(0, 4u << 20));
            px = xsgnative::DecodeVramTexture(vram, tex0, texa, &tw, &th);
        }
        t.checked = g_frame;
        if (!px.empty()) {
            Expand(px);
            const uint64_t hsh = Fnv(px.data(), px.size());
            if (hsh != t.hash || !t.id) {
                t.id = xsgmenu::RenderTexture(t.id, px.data(), tw, th);
                t.hash = hsh;
            }
            t.w = tw;
            t.h = th;
            t.gen = gen;
        }
    }
    if (w) *w = t.w;
    if (h) *h = t.h;
    return ImTextureID(t.id);
}

void ArtSprite(ImDrawList* dl, uint64_t tex0, uint64_t texa, float u, float v, float tw, float th, ImVec2 a, ImVec2 b,
               uint32_t rgba, float alpha, bool additive, uint32_t texset) {
    uint32_t w = 0, h = 0;
    const ImTextureID id = ArtTexture(tex0, texa, &w, &h, texset);
    if (!id || !w || !h) return;
    auto ch = [&](int s) { return std::min(255u, ((rgba >> s) & 0xffu) * 2); };
    const uint32_t r = ch(0), g = ch(8), bl = ch(16);
    uint32_t al = uint32_t(std::min(255.0f, float(ch(24)) * std::clamp(alpha, 0.0f, 1.0f)));
    if (additive) {
        // ImGui blends src-alpha / one-minus-src-alpha; an additive sprite is approximated
        // by pre-dimming with its alpha over black (dark texels add nothing)
        al = al * 3 / 4;
    }
    dl->AddImage(id, a, b, ImVec2(u / float(w), v / float(h)), ImVec2((u + tw) / float(w), (v + th) / float(h)),
                 IM_COL32(r, g, bl, al));
}

}  // namespace xsgui

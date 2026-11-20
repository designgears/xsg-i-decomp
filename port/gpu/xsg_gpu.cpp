/*
 * xsg-gpu: the hardware GS renderer process (Stage 9, port/docs/STAGE9_GPU.md).
 *
 * A native 64-bit executable spawned by the 32-bit game (gs_bridge.cpp) with
 * the shared memory of port/runtime/gpu_ipc.h on fd 3.  It consumes the
 * command ring in order and drives paraLLEl-GS (LGPLv3+, linked only here):
 *
 *   GPU_MSG_GIF        GSInterface::gif_transfer(path, data, size)
 *   GPU_MSG_PRIV       the privileged register file (PrivRegisterState)
 *   GPU_MSG_VSYNC      GSInterface::flush() + vsync(), present, stream
 *   GPU_MSG_READ_VRAM  map_vram_read(0, 4 MB) -> reply area
 *   GPU_MSG_READ_FIFO  read_transfer_fifo(n) -> reply area
 *
 * Settings: settings.ini only (port/docs/MENU.md; the file is
 * ~/.config/xsg/settings.ini or $XSG_CONFIG, inherited from the game), read
 * by menu/cfg.cpp from the shared table port/runtime/xsg_settings_table.h:
 *   [display]  fullscreen, window_scale, present, aspect, scale_mode, filter (live)
 *   [renderer] ssaa, ssaa_textures, hires_scanout, native, native_scale
 *   [debug]    headless, vulkan_validation, pmode_raw, input_test, native_*, ...
 * The in-game menu (F1, menu/menu.cpp) edits all of them.
 * Keyboard/gamepad input: xsg_input.cpp, port/docs/STAGE10_INPUT.md.
 * Command line (from the game): [fd] [--stream=FIFO], the 512x448 RGB24 live
 * stream ([debug] stream) when the game does not run the soft GS itself.
 */
#include "gpu_ipc.h"
#include "xsg_input.h"
#include "native.h"
#include "glyphs.h"
#include "ui/art.h"
#include "ui/menus.h"
#include "ui/pointer.h"
#include "ui/dialog.h"
#include "native_prof.h"
#include "audio.h"
#include "menu/cfg.h"
#include "menu/menu.h"

namespace {
const uint32_t kCrtVert[] = {
#include "crt.vert.inc"
};
const uint32_t kCrtFrag[] = {
#include "crt.frag.inc"
};
}  // namespace

#include "context.hpp"
#include "device.hpp"
#include "wsi.hpp"
#ifdef _WIN32
#include "../runtime/win32/xsg_pcores.h"
#endif
#include "gs_interface.hpp"
#include "ps2host/gs.h"

#ifdef XSG_GPU_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#ifndef _WIN32
#include <linux/futex.h>
#endif
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>
#ifdef _WIN32
#include "xsg_win32_ipc.h"   // port/runtime: the cross-process wait/wake (named events)
#else
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/wait.h>
#include <spawn.h>
#endif

// File-local stb_image_write (static): the menu's screenshots and test shots.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#pragma GCC diagnostic pop

using namespace ParallelGS;

namespace {

GpuShm* shm;
#ifdef _WIN32
/* The game, from --parent=<pid>: getppid() below answers its pid while it runs and 0 once it
   has exited, so the "game went away" checks read as on Linux. */
unsigned long g_parent;
HANDLE g_parent_h;
unsigned long getppid() {
    return g_parent_h && WaitForSingleObject(g_parent_h, 0) == WAIT_OBJECT_0 ? 0 : g_parent;
}
#else
pid_t g_parent;
#endif
uint8_t* ring;
uint8_t* reply;

void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void Log(const char* fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "[xsg-gpu] %s\n", b);
}

long Futex(volatile uint32_t* addr, int op, uint32_t val, const timespec* ts) {
#ifdef _WIN32
    return xsg_w32_futex(addr, op, val, ts);
#else
    return syscall(SYS_futex, (uint32_t*)addr, op, val, ts, nullptr, 0);
#endif
}


/* ---------------------------------------------------------------- GS memory decode
   The displayed pixel from a raw local-memory copy, with the software GS's
   own (MIT) swizzle code, ps2host::gs::Memory::Address. */
uint32_t Pixel(const uint8_t* vram, uint32_t fbp, uint32_t fbw, uint32_t psm, uint32_t x, uint32_t y) {
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

/* ---------------------------------------------------------------- stream (XSG_GPU_STREAM) */
struct Stream {
    std::string path;
    std::mutex m;
    std::condition_variable cv;
    std::vector<uint8_t> buf, out;
    bool ready = false;
    int fd = -1;
};
Stream* g_stream;

void StreamWriter() {
    Stream& s = *g_stream;
#ifdef _WIN32
    (void)s;   // [debug] stream (a FIFO) is Linux-only; main() never starts this thread on Windows
#else
    for (;;) {
        {
            std::unique_lock<std::mutex> l(s.m);
            s.cv.wait(l, [&] { return s.ready; });
            s.out.swap(s.buf);
            s.ready = false;
        }
        if (s.fd < 0) {
            s.fd = ::open(s.path.c_str(), O_WRONLY | O_NONBLOCK);
            if (s.fd < 0) continue;
            ::fcntl(s.fd, F_SETFL, ::fcntl(s.fd, F_GETFL) & ~O_NONBLOCK);
        }
        size_t off = 0;
        while (off < s.out.size()) {
            ssize_t n = ::write(s.fd, s.out.data() + off, s.out.size() - off);
            if (n <= 0) { ::close(s.fd); s.fd = -1; break; }
            off += size_t(n);
        }
    }
#endif
}

bool StreamWants() {
    if (!g_stream) return false;
    std::lock_guard<std::mutex> l(g_stream->m);
    return !g_stream->ready;
}

/* ---------------------------------------------------------------- window */
#ifdef XSG_GPU_SDL
struct SdlPlatform final : Vulkan::WSIPlatform {
    SDL_Window* win = nullptr;
    bool is_alive = true;
    bool fullscreen = false;
    bool minimized = false;   // no presents (SDL_EVENT_WINDOW_MINIMIZED .. RESTORED)
    uint32_t w = 1024, h = 896;

    VkSurfaceKHR create_surface(VkInstance instance, VkPhysicalDevice) override {
        VkSurfaceKHR s = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(win, instance, nullptr, &s)) {
            Log("SDL_Vulkan_CreateSurface: %s", SDL_GetError());
            return VK_NULL_HANDLE;
        }
        return s;
    }
    void destroy_surface(VkInstance instance, VkSurfaceKHR surface) override {
        SDL_Vulkan_DestroySurface(instance, surface, nullptr);
    }
    std::vector<const char*> get_instance_extensions() override {
        Uint32 n = 0;
        const char* const* e = SDL_Vulkan_GetInstanceExtensions(&n);
        return e ? std::vector<const char*>(e, e + n) : std::vector<const char*>{};
    }
    uint32_t get_surface_width() override { return w; }
    uint32_t get_surface_height() override { return h; }
    bool alive(Vulkan::WSI&) override { return is_alive; }
    void poll_input() override {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_F11 && !e.key.repeat) {
                fullscreen = !fullscreen;
                SDL_SetWindowFullscreen(win, fullscreen);
                xsgcfg::Set("display.fullscreen", fullscreen ? "1" : "0");
                continue;
            }
            if (xsgmenu::Event(e)) continue;   // F1 / Esc / F10, and everything while the menu is open
            if (xsginput::Event(e)) continue;
            if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) is_alive = false;
            if (e.type == SDL_EVENT_WINDOW_MINIMIZED) minimized = true;
            if (e.type == SDL_EVENT_WINDOW_RESTORED || e.type == SDL_EVENT_WINDOW_MAXIMIZED || e.type == SDL_EVENT_WINDOW_SHOWN) {
                if (minimized) resize = true;   // a fresh swapchain for the restored window
                minimized = false;
            }
            if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                w = uint32_t(e.window.data1 > 0 ? e.window.data1 : 1);
                h = uint32_t(e.window.data2 > 0 ? e.window.data2 : 1);
                resize = true;
            }
        }
    }
    void poll_input_async(Granite::InputTrackerHandler*) override { poll_input(); }
};
#endif

/* The Vulkan pipeline cache, kept across runs in $XDG_CACHE_HOME/xsg (~/.cache/xsg): a pipeline's
   first use compiled synchronously (a 30+ ms hitch at an effect's first appearance, every run).
   xsg-gpu is usually ended by a signal, so it is saved whenever it grew (at the 600-vsync tick). */
std::string PipelineCachePath() {
#ifdef _WIN32
    // %LOCALAPPDATA%\xsg\cache
    std::string dir = XsgWinDir("LOCALAPPDATA");
    if (dir.empty()) return dir;
    dir += "/xsg";
    mkdir(dir.c_str(), 0755);
    dir += "/cache";
#else
    const char* x = std::getenv("XDG_CACHE_HOME");
    const char* home = std::getenv("HOME");
    std::string dir = x && *x ? std::string(x) + "/xsg" : home ? std::string(home) + "/.cache/xsg" : std::string();
    if (dir.empty()) return dir;
#endif
    mkdir(dir.c_str(), 0755);
    return dir + "/pipeline_cache.bin";
}
bool g_pcache;   // the device's cache is initialised (Granite does it only with a filesystem attached)
void PipelineCacheLoad(Vulkan::Device& dev) {
    const std::string path = PipelineCachePath();
    FILE* f = path.empty() ? nullptr : std::fopen(path.c_str(), "rb");
    if (!f) { g_pcache = dev.init_pipeline_cache(nullptr, 0); return; }
    std::vector<uint8_t> data;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) {
        data.resize(size_t(n));
        if (std::fread(data.data(), 1, data.size(), f) != data.size()) data.clear();
    }
    std::fclose(f);
    g_pcache = dev.init_pipeline_cache(data.data(), data.size());   // a stale or corrupt file starts fresh
    if (g_pcache) Log("pipeline cache: %zu KB from %s", data.size() >> 10, path.c_str());
}
void PipelineCacheWrite(const std::vector<uint8_t>& data, size_t n);
void PipelineCacheSave(Vulkan::Device& dev) {
    static size_t saved;
    if (!g_pcache) return;
    // paraLLEl-GS compiles on background threads, so the cache can grow between the size query
    // and the copy (vkGetPipelineCacheData: VK_INCOMPLETE); try again with the new size.
    size_t n = 0;
    std::vector<uint8_t> data;
    for (int tries = 0;; tries++) {
        n = dev.get_pipeline_cache_size();
        if (!n || n == saved) return;
        data.resize(n);
        if (dev.get_pipeline_cache_data(data.data(), n)) break;
        if (tries == 3) return;
    }
    static std::atomic<bool> writing{false};
    if (writing.exchange(true)) return;   // the last save is still being written
    saved = n;
    std::thread([data = std::move(data), n] {
        PipelineCacheWrite(data, n);
        writing = false;
    }).detach();
}
void PipelineCacheWrite(const std::vector<uint8_t>& data, size_t n) {
    const std::string path = PipelineCachePath();
    if (path.empty()) return;
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return;
    const bool ok = std::fwrite(data.data(), 1, n, f) == n;
    if (std::fclose(f) != 0 || !ok) return;
#ifdef _WIN32
    // rename() does not replace an existing file on Windows: the cache stayed at its first,
    // partial save, and every start compiled the rest again (issue #3).
    MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
#else
    std::rename(tmp.c_str(), path.c_str());
#endif
}

struct Renderer {
    std::unique_ptr<Vulkan::Context> ctx;
    std::unique_ptr<Vulkan::Device> headless_dev;
#ifdef XSG_GPU_SDL
    std::unique_ptr<SdlPlatform> platform;
    std::unique_ptr<Vulkan::WSI> wsi;
#endif
    Vulkan::Device* dev = nullptr;
    GSInterface gs;
    bool windowed = false;
    uint64_t vsyncs = 0, gif_bytes = 0, presents = 0, native_presents = 0;
    uint32_t native_msgs = 0, native_last = 0;   // GPU_MSG_NATIVE this / last vsync (menu overlay)
    std::string shot_path;                        // the menu's screenshot request
    int window_scale = 2;
    bool pmode_raw = xsgcfg::GetBool("debug.pmode_raw"), pmode_logged = false;
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    uint32_t hires = 0;
    std::string aspect = "4:3";
    bool integer_scale = false, nearest = false;

    static constexpr Vulkan::ContextCreationFlags kFlags = Vulkan::CONTEXT_CREATION_ENABLE_PUSH_DESCRIPTOR_BIT |
                                                           Vulkan::CONTEXT_CREATION_ENABLE_DESCRIPTOR_HEAP_BIT |
                                                           Vulkan::CONTEXT_CREATION_ENABLE_DESCRIPTOR_BUFFER_BIT;

    bool InitWindow() {
#ifdef XSG_GPU_SDL
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { Log("SDL_Init: %s", SDL_GetError()); return false; }
        int scale = xsgcfg::GetInt("display.window_scale");
        if (scale < 1) scale = 1;
        window_scale = scale;
        platform = std::make_unique<SdlPlatform>();
        platform->win = SDL_CreateWindow("Xenosaga Episode I (xsg-gpu)", 512 * scale, 448 * scale,
                                         SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!platform->win) { Log("SDL_CreateWindow: %s", SDL_GetError()); return false; }
        int pw = 0, ph = 0;
        SDL_SetWindowMinimumSize(platform->win, 320, 240);
        if (xsginput::Option("fullscreen", "XSG_GPU_FULLSCREEN", "0") != "0") {
            platform->fullscreen = true;
            SDL_SetWindowFullscreen(platform->win, true);
            SDL_SyncWindow(platform->win);
        }
        SDL_GetWindowSizeInPixels(platform->win, &pw, &ph);
        platform->w = uint32_t(pw > 0 ? pw : 512 * scale);
        platform->h = uint32_t(ph > 0 ? ph : 448 * scale);
        wsi = std::make_unique<Vulkan::WSI>();
        wsi->set_platform(platform.get());
        wsi->set_backbuffer_format(Vulkan::BackbufferFormat::UNORM);
        // Granite creates the swapchain COLOR_ATTACHMENT only unless told otherwise;
        // the present path clears and blits into it (ico-recomp's WSI usage fix).
        wsi->set_extra_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        // Present mode.  The renderer thread is the ring consumer, so a blocking
        // present (fifo) paces the whole pipeline to the display; mailbox (the
        // default) never blocks and the game's own vblank timer paces it.
        std::string pm = xsginput::Option("present", nullptr, "mailbox");
        wsi->set_present_mode(PresentModeOf(pm));
        aspect = xsginput::Option("aspect", "XSG_GPU_ASPECT", "4:3");
        integer_scale = xsginput::Option("scale_mode", "XSG_GPU_SCALEMODE", "fit") == "integer";
        nearest = xsginput::Option("filter", "XSG_GPU_FILTER", "linear") == "nearest";
        Log("window: present %s, aspect %s, %s scaling, %s filter, F11 fullscreen", pm.c_str(), aspect.c_str(),
            integer_scale ? "integer" : "fit", nearest ? "nearest" : "linear");
        if (!wsi->init_context_from_platform(1, {}, kFlags) || !wsi->init_device() || !wsi->init_surface_swapchain()) {
            Log("WSI init failed");
            wsi.reset();
            SDL_DestroyWindow(platform->win);
            platform.reset();
            return false;
        }
        dev = &wsi->get_device();
        windowed = true;
        return true;
#else
        return false;
#endif
    }

    static Vulkan::PresentMode PresentModeOf(const std::string& pm) {
        if (pm == "fifo" || pm == "vsync") return Vulkan::PresentMode::SyncToVBlank;
        if (pm == "immediate") return Vulkan::PresentMode::UnlockedForceTearing;
        if (pm == "auto") return Vulkan::PresentMode::UnlockedMaybeTear;
        return Vulkan::PresentMode::UnlockedNoTearing;
    }

    /* The in-game menu changed a video.* setting: follow it (between frames). */
    void ApplyVideo() {
        aspect = xsginput::Option("aspect", "XSG_GPU_ASPECT", "4:3");
        integer_scale = xsginput::Option("scale_mode", "XSG_GPU_SCALEMODE", "fit") == "integer";
        nearest = xsginput::Option("filter", "XSG_GPU_FILTER", "linear") == "nearest";
#ifdef XSG_GPU_SDL
        if (!windowed) return;
        bool fs = xsgcfg::GetBool("display.fullscreen");
        if (fs != platform->fullscreen) {
            platform->fullscreen = fs;
            SDL_SetWindowFullscreen(platform->win, fs);
        }
        int sc = std::max(1, xsgcfg::GetInt("display.window_scale"));
        if (sc != window_scale) {
            window_scale = sc;
            if (!fs) SDL_SetWindowSize(platform->win, 512 * sc, 448 * sc);
        }
        auto mode = PresentModeOf(xsgcfg::Get("display.present"));
        if (mode != wsi->get_present_mode()) {
            Log("present mode -> %s (swapchain recreated)", xsgcfg::Get("display.present").c_str());
            wsi->set_present_mode(mode);
        }
#endif
    }

    const char* PresentName() const {
#ifdef XSG_GPU_SDL
        if (windowed) {
            switch (wsi->get_present_mode()) {
            case Vulkan::PresentMode::SyncToVBlank: return "FIFO (vsync)";
            case Vulkan::PresentMode::UnlockedForceTearing: return "Immediate";
            case Vulkan::PresentMode::UnlockedMaybeTear: return "Auto";
            default: return "Mailbox";
            }
        }
#endif
        return "headless";
    }

    bool InitHeadless() {
        ctx = std::make_unique<Vulkan::Context>();
        ctx->set_num_thread_indices(1);
        if (!ctx->init_instance_and_device(nullptr, 0, nullptr, 0, kFlags)) return false;
        headless_dev = std::make_unique<Vulkan::Device>();
        headless_dev->set_context(*ctx);
        headless_dev->init_frame_contexts(4);
        dev = headless_dev.get();
        return true;
    }

    bool Init() {
        if (!xsgcfg::GetBool("debug.vulkan_validation")) setenv("GRANITE_VULKAN_NO_VALIDATION", "1", 0);
        if (!Vulkan::Context::init_loader(nullptr)) { Log("no Vulkan loader"); return false; }
        if (!xsgcfg::GetBool("debug.headless") && !InitWindow()) Log("no window; running headless");
        bool sdl_input = windowed;
#ifdef XSG_GPU_SDL
        // Headless: input only for the synthetic test hook (no real devices).
        if (!windowed && xsgcfg::CStr("debug.input_test") && *xsgcfg::CStr("debug.input_test"))
            sdl_input = SDL_Init(SDL_INIT_EVENTS);
#endif
        xsginput::Init(reinterpret_cast<GpuPad*>(reinterpret_cast<uint8_t*>(shm) + XSG_GPU_PAD_OFF), sdl_input);
        if (!dev && !InitHeadless()) { Log("no Vulkan device"); return false; }
        PipelineCacheLoad(*dev);
        GSOptions o = {};
        o.dynamic_super_sampling = true;   // the menu changes the rate live (ApplyLive)
        int ss = xsgcfg::CStr("renderer.ssaa") ? std::atoi(xsgcfg::CStr("renderer.ssaa")) : 1;
        switch (ss) {
        case 2: o.super_sampling = SuperSampling::X2; break;
        case 4: o.super_sampling = SuperSampling::X4; break;
        case 8: o.super_sampling = SuperSampling::X8; break;
        case 16: o.super_sampling = SuperSampling::X16; break;
        default: ss = 1; o.super_sampling = SuperSampling::X1; break;
        }
        o.super_sampled_textures = ss > 1 && xsgcfg::GetBool("renderer.ssaa_textures");
        hires = ss >= 4 && xsgcfg::GetBool("renderer.hires_scanout");
        if (!gs.init(dev, o)) { Log("GSInterface::init failed"); return false; }
        xsgnative::Init(dev);
        InitMenu();
        InitCrt();
        xsgglyph::Init();
        xsgglyph::SetGs(&gs);
        xsgui::ArtInit(&gs);
        xsgui::Init(shm);
        xsgui::MenuInit(shm);
        xsgui::PointerInit(shm);
        const auto& p = dev->get_gpu_properties();
        Log("paraLLEl-GS on \"%s\" (Vulkan %u.%u), %s, SSAA %dx%s%s", p.deviceName, VK_API_VERSION_MAJOR(p.apiVersion),
            VK_API_VERSION_MINOR(p.apiVersion), windowed ? "windowed" : "headless", ss,
            o.super_sampled_textures ? " +textures" : "", hires ? " +hires scanout" : "");
        return true;
    }

    void Priv(uint32_t off, uint64_t v) {
        auto& priv = const_cast<PrivRegisterState&>(gs.get_priv_register_state());
        off &= 0x1ff0;
        // PMODE as the software GS and the screenshots read it (gs_bridge.cpp):
        // circuit 1 (or 2 when only EN2 is set), opaque.  The game's PMODE
        // (from the libgraph stubs in sdk.c, e.g. 0x8004: EN1/EN2 off, MMOD 0)
        // made paraLLEl-GS scan out black or near-black.  XSG_GPU_PMODE_RAW=1
        // passes the register through as written.
        if (off == 0 && !pmode_raw && !((v & 3) == 2)) {
            const uint64_t nv = (v & ~uint64_t(0xffe3)) | 1 | 0x20 | 0xff00;   // EN1, MMOD (use ALP), ALP 0xff
            if (!pmode_logged && nv != v) {
                Log("PMODE 0x%llx -> 0x%llx (circuit 1 opaque, as the software GS shows it)", (unsigned long long)v, (unsigned long long)nv);
                pmode_logged = true;
            }
            v = nv;
        }
        if (off < 0x1000) priv.qwords_lo[(off >> 4) * 2] = v;
        else priv.qwords_hi[((off - 0x1000) >> 4) * 2] = v;
    }

    /* Destination size in a bw x bh backbuffer: the display aspect (4:3 unless
       "stretch"), and with integer scaling the largest whole multiple of the
       scanout height that fits (fit scaling if even 1x does not). */
    /* [display] widescreen (native renderer): the picture is the native layer's 684 x 448,
       at the 4:3 display's pixel aspect (684/512 x 4:3, about 16:9), never stretched. */
    static constexpr int kWideW = 684, kNarrowW = 512;
    static bool Widescreen() { return xsgnative::On() && xsgcfg::GetBool("display.widescreen"); }
    void Fit(int bw, int bh, int src_h, int* dw, int* dh) const {
        int an = 4, ad = 3;
        if (Widescreen()) { an = 4 * kWideW; ad = 3 * kNarrowW; }
        else if (aspect == "stretch") { *dw = bw; *dh = bh; return; }
        else if (aspect == "16:9") { an = 16; ad = 9; }
        int h = bh, w = bh * an / ad;
        if (w > bw) { w = bw; h = bw * ad / an; }
        if (integer_scale && src_h > 0) {
            int k = h / src_h;
            if (k >= 1) { h = k * src_h; w = h * an / ad; }
        }
        *dw = w;
        *dh = h;
    }

    void InitMenu();

    /* The game image a present shows: the native composite (cropped to the
       scanned-out rectangle) when this vsync has one, else the scanout. */
    /* Widescreen: *wide is set when the image has the native side bands (the crop then
       spans them); without them the image is the 4:3 frame, shown pillarboxed. */
    const Vulkan::Image* Source(const ScanoutResult& so, int* sx0, int* sy0, int* sw, int* sh, int* src_h,
                                bool* wide = nullptr) {
        uint32_t nfw = 0, nfh = 0, margin = 0;
        if (wide) *wide = false;
        const Vulkan::ImageHandle* live = xsgnative::On() ? &xsgnative::LiveImage(&nfw, &nfh, &margin) : nullptr;
        const Vulkan::Image* src = so.image.get();
        *sx0 = 0;
        *sy0 = 0;
        *sw = src ? int(src->get_width()) : 0;
        *sh = src ? int(src->get_height()) : 0;
        *src_h = *sh;
        if (live && *live && so.image) {
            const int s = xsgnative::Scale();
            const auto& priv = gs.get_priv_register_state();
            const uint64_t pmode = priv.qwords_lo[0];
            const uint64_t dispfb = priv.qwords_lo[((pmode & 1) || !(pmode & 2) ? 0x70 : 0x90) >> 4 << 1];
            const int dbx = int(dispfb >> 32) & 0x7ff, dby = int(dispfb >> 43) & 0x7ff;
            const int w = std::min(int(so.internal_width), int(nfw) - dbx), h = std::min(int(so.internal_height), int(nfh) - dby);
            if (w > 0 && h > 0) {
                src = live->get();
                *sx0 = dbx * s;   // with side bands: the GS frame's x = 0 is at margin, so the crop starts margin earlier
                *sy0 = dby * s;
                *sw = (w + 2 * int(margin)) * s;
                *sh = h * s;
                *src_h = h;
                if (wide) *wide = margin > 0;
            }
        }
        return src;
    }

    /* Clear bb (TRANSFER_DST) and blit the game image into it, fitted. */
    /* CRT filter ([display] crt): the game image goes to crt_src (sampled) instead of
       the target, and DrawCrt draws it into the target's render pass. */
    Vulkan::Program* crt_prog = nullptr;
    Vulkan::ImageHandle crt_src;
    int crt_rect[4] = {0, 0, 0, 0}, crt_lines[2] = {448, 512};
    struct CrtParams { float scan, mask, halation, bloom, curvature, blur; int masktype; };
    bool CrtOn() const { return crt_prog && xsgcfg::Get("display.crt") != "off" && !xsgcfg::Get("display.crt").empty(); }
    CrtParams CrtStyle() {
        const std::string m = xsgcfg::Get("display.crt");
        CrtParams c = {0.55f, 0.0f, 0.0f, 0.10f, 0.0f, 0.35f, 0};   // scanlines
        if (m == "consumer") c = {0.35f, 0.40f, 0.25f, 0.25f, 0.06f, 1.10f, 2};
        else if (m == "trinitron") c = {0.50f, 0.40f, 0.15f, 0.15f, 0.03f, 0.60f, 1};
        else if (m == "pvm") c = {0.75f, 0.30f, 0.05f, 0.05f, 0.00f, 0.30f, 4};
        else if (m == "shadow") c = {0.40f, 0.50f, 0.20f, 0.20f, 0.08f, 0.80f, 3};
        auto ovr = [](const char* key, float& v, float scale) {
            const int x = xsgcfg::GetInt(key);
            if (x >= 0) v = float(x) * scale;
        };
        ovr("display.crt_scanlines", c.scan, 0.01f);
        ovr("display.crt_mask", c.mask, 0.01f);
        ovr("display.crt_halation", c.halation, 0.01f);
        ovr("display.crt_bloom", c.bloom, 0.01f);
        ovr("display.crt_curvature", c.curvature, 0.0025f);   // 0..100 -> 0..0.25
        return c;
    }
    void InitCrt() {
        Vulkan::ResourceLayout vl, fl;
        fl.output_mask = 1;
        fl.sets[0].sampled_image_mask = 1;
        fl.sets[0].meta[0].array_size = 1;
        fl.push_constant_size = 52;
        vl.push_constant_size = 52;
        crt_prog = dev->request_program(dev->request_shader(kCrtVert, sizeof kCrtVert, &vl),
                                        dev->request_shader(kCrtFrag, sizeof kCrtFrag, &fl));
    }
    void DrawCrt(Vulkan::CommandBuffer& cmd) {
        if (!crt_src) return;
        const CrtParams c = CrtStyle();
        cmd.set_opaque_state();
        cmd.set_program(crt_prog);
        cmd.set_depth_test(false, false);
        cmd.set_cull_mode(VK_CULL_MODE_NONE);
        cmd.set_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        VkViewport vp = {float(crt_rect[0]), float(crt_rect[1]), float(crt_rect[2]), float(crt_rect[3]), 0, 1};
        cmd.set_viewport(vp);
        cmd.set_texture(0, 0, crt_src->get_view(), Vulkan::StockSampler::LinearClamp);
        struct { float lines[2], outsz[2], scan, mask, halation, bloom, curvature, blur, strength; int32_t masktype, interlace; } pc = {
            {float(crt_lines[0]), float(crt_lines[1])}, {float(crt_rect[2]), float(crt_rect[3])}, c.scan, c.mask, c.halation,
            c.bloom, c.curvature, c.blur, std::max(0, std::min(100, xsgcfg::GetInt("display.crt_strength"))) * 0.01f,
            c.masktype, xsgcfg::GetBool("display.crt_interlace") ? 1 : 0};
        static_assert(sizeof pc == 52, "crt push constants");
        cmd.push_constants(&pc, 0, sizeof pc);
        cmd.draw(3);
    }

    void BlitGame(Vulkan::CommandBuffer& cmd, const Vulkan::Image& bb, const ScanoutResult& so, bool fit, bool* native,
                  bool crt = false) {
        VkClearValue clear = {};
        cmd.clear_image(bb, clear);
        int sx0, sy0, sw, sh, src_h;
        bool wide = false;
        const Vulkan::Image* src = Source(so, &sx0, &sy0, &sw, &sh, &src_h, &wide);
        if (native) *native = false;
        if (!src) return;
        int dw = int(bb.get_width()), dh = int(bb.get_height());
        if (fit) Fit(int(bb.get_width()), int(bb.get_height()), src_h, &dw, &dh);
        int x0 = (int(bb.get_width()) - dw) / 2;
        const int y0 = (int(bb.get_height()) - dh) / 2;
        // Widescreen: the 16:9 picture's centre 4:3 is the GS frame (and the UI's game area);
        // a frame without side bands is pillarboxed into it.
        const bool ws = fit && Widescreen();
        const int cw = ws ? dw * kNarrowW / kWideW : dw, cx0 = x0 + (dw - cw) / 2;
        if (ws && !wide) { x0 = cx0; dw = cw; }
        cmd.barrier(VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT);
        if (native) *native = src != so.image.get();
        game_rect[0] = float(cx0); game_rect[1] = float(y0); game_rect[2] = float(cw); game_rect[3] = float(dh);
        last_image_w = uint32_t(sw);
        last_image_h = uint32_t(sh);
        // A source larger than the target (the native layer at 4-8x in a window) is
        // brought down by 2x linear halvings, each a 2x2 box filter, until it is within
        // 2x of the target.  One nearest (or even linear) blit from 4x+ skips most source
        // pixels: jagged edges and lost detail.  "nearest" applies to upscaling only.
        static std::vector<Vulkan::ImageHandle> chain;
        chain.clear();
        while (sw >= 2 * dw && sh >= 2 * dh) {
            const int hw = sw / 2, hh = sh / 2;
            auto ci = Vulkan::ImageCreateInfo::immutable_2d_image(uint32_t(hw), uint32_t(hh), VK_FORMAT_R8G8B8A8_UNORM);
            ci.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            ci.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            ci.layout = Vulkan::ImageLayout::General;
            auto half = dev->create_image(ci);
            cmd.image_barrier(*half, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, 0,
                              VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
            cmd.blit_image(*half, *src, {0, 0, 0}, {hw, hh, 1}, {sx0, sy0, 0}, {sw, sh, 1}, 0, 0, 0, 0, 1,
                           VK_FILTER_LINEAR);
            cmd.barrier(VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT,
                        VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);
            chain.push_back(half);
            src = half.get();
            sx0 = sy0 = 0;
            sw = hw;
            sh = hh;
        }
        const bool down = sw > dw || sh > dh;
        if (crt) {
            // the (downsampled) game image into a sampled image for DrawCrt
            if (!crt_src || int(crt_src->get_width()) != sw || int(crt_src->get_height()) != sh) {
                auto ci = Vulkan::ImageCreateInfo::immutable_2d_image(uint32_t(sw), uint32_t(sh), VK_FORMAT_R8G8B8A8_UNORM);
                ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
                ci.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
                ci.layout = Vulkan::ImageLayout::General;
                crt_src = dev->create_image(ci);
                cmd.image_barrier(*crt_src, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, 0,
                                  VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
            } else {
                cmd.barrier(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
            }
            cmd.blit_image(*crt_src, *src, {0, 0, 0}, {sw, sh, 1}, {sx0, sy0, 0}, {sw, sh, 1}, 0, 0, 0, 0, 1,
                           VK_FILTER_NEAREST);
            cmd.barrier(VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
            crt_rect[0] = x0;
            crt_rect[1] = y0;
            crt_rect[2] = dw;
            crt_rect[3] = dh;
            crt_lines[0] = src_h;
            crt_lines[1] = std::max(1, int(int64_t(src_h) * last_image_w / std::max<uint32_t>(1, last_image_h)));
            return;
        }
        cmd.blit_image(bb, *src, {x0, y0, 0}, {dw, dh, 1}, {sx0, sy0, 0}, {sw, sh, 1}, 0, 0, 0, 0, 1,
                       nearest && !down ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
    }
    uint32_t last_image_w = 0, last_image_h = 0;
    double worst_ms = 0;
    float game_rect[4] = {0, 0, 0, 0};
    const Vulkan::Image* last_live = nullptr;
    uint64_t repeats = 0, behind_skips = 0;
    uint32_t worst_at = 0, slow_frames = 0;

    void Present(const ScanoutResult& so, bool menu) {
#ifdef XSG_GPU_SDL
        if (!windowed) return;
        if (!platform->is_alive) {
            Log("window closed");
            std::_Exit(0);   // the game sees the process go and exits too
        }
        // Minimized: no present.  Granite's WSI blocks inside begin_frame until the window
        // comes back, and with it the ring stopped draining and the game froze (#3).
        if (platform->minimized || (SDL_GetWindowFlags(platform->win) & SDL_WINDOW_MINIMIZED)) return;
        if (!wsi->begin_frame()) return;
        auto cmd = dev->request_command_buffer();
        auto& bb = dev->get_swapchain_view().get_image();
        cmd->swapchain_touch_in_stages(VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT);
        cmd->image_barrier(bb, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT, 0,
                           VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        // The native layer (XSG_GPU_NATIVE): the high-resolution composite of the
        // displayed buffer, cropped to the scanned-out rectangle, replaces the
        // paraLLEl-GS scanout.  Frames without native batches show the scanout.
        bool native = false;
        const bool crt = CrtOn();
        if (!crt) crt_src.reset();
        BlitGame(*cmd, bb, so, true, &native, crt);
        native_presents += native;
        // The in-game menu / overlay (menu/menu.cpp) is drawn over the game image.
        const bool draw_menu = (menu && xsgmenu::Visible()) || crt;
        cmd->image_barrier(bb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                           VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                           draw_menu ? VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_2_NONE,
                           draw_menu ? VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT : 0);
        if (draw_menu) {
            Vulkan::RenderPassInfo rp;
            rp.num_color_attachments = 1;
            rp.color_attachments[0] = &dev->get_swapchain_view();
            rp.load_attachments = 1;
            rp.store_attachments = 1;
            cmd->begin_render_pass(rp);
            if (crt) DrawCrt(*cmd);
            if (menu && xsgmenu::Visible()) xsgmenu::Draw(*cmd, bb.get_width(), bb.get_height());
            cmd->end_render_pass();
        }
        dev->submit(cmd);
        wsi->end_frame();
        presents++;
#endif
    }

    /* The game image (fitted into w x h, or at its own size when w = 0),
       optionally with the menu over it, as a PNG.  Waits for the GPU. */
    void WritePngOf(const ScanoutResult& so, const std::string& path, uint32_t w, uint32_t h, bool menu) {
        gs.flush();   // submits paraLLEl-GS's vsync command buffer (the scanout) before ours reads it
        int sx0, sy0, sw, sh, src_h;
        if (!Source(so, &sx0, &sy0, &sw, &sh, &src_h)) { Log("screenshot: no image yet"); return; }
        if (!w || !h) {   // the game image at the window's display aspect, at least its own size
            h = uint32_t(sh);
            int dw, dh;
            Fit(1 << 20, int(h), int(h), &dw, &dh);
            w = aspect == "stretch" ? uint32_t(sw) : uint32_t(dw);
        }
        Vulkan::ImageCreateInfo ii = Vulkan::ImageCreateInfo::render_target(w, h, VK_FORMAT_R8G8B8A8_UNORM);
        ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ii.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        auto img = dev->create_image(ii);
        Vulkan::BufferCreateInfo bi = {};
        bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bi.domain = Vulkan::BufferDomain::CachedHost;
        bi.size = VkDeviceSize(w) * h * 4;
        auto buf = dev->create_buffer(bi);
        auto cmd = dev->request_command_buffer();
        cmd->image_barrier(*img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, 0,
                           VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        BlitGame(*cmd, *img, so, true, nullptr);
        if (menu && xsgmenu::Visible()) {
            cmd->image_barrier(*img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
            Vulkan::RenderPassInfo rp;
            rp.num_color_attachments = 1;
            rp.color_attachments[0] = &img->get_view();
            rp.load_attachments = 1;
            rp.store_attachments = 1;
            cmd->begin_render_pass(rp);
            xsgmenu::Draw(*cmd, w, h);
            cmd->end_render_pass();
            cmd->image_barrier(*img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        } else {
            cmd->image_barrier(*img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        }
        cmd->copy_image_to_buffer(*buf, *img, 0, {}, {w, h, 1}, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1});
        cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                     VK_ACCESS_HOST_READ_BIT);
        Vulkan::Fence fence;
        dev->submit(cmd, &fence);
        fence->wait();
        const uint8_t* px = static_cast<const uint8_t*>(dev->map_host_buffer(*buf, Vulkan::MEMORY_ACCESS_READ_BIT));
        std::vector<uint8_t> rgb(size_t(w) * h * 3);
        for (size_t i = 0; i < size_t(w) * h; ++i) {
            rgb[i * 3] = px[i * 4];
            rgb[i * 3 + 1] = px[i * 4 + 1];
            rgb[i * 3 + 2] = px[i * 4 + 2];
        }
        dev->unmap_host_buffer(*buf, Vulkan::MEMORY_ACCESS_READ_BIT);
        if (size_t sl = path.rfind('/'); sl != std::string::npos && sl > 0) {
            std::string d = path.substr(0, sl);
            for (size_t i = 1; i <= d.size(); ++i)
                if (i == d.size() || d[i] == '/') ::mkdir(d.substr(0, i).c_str(), 0755);
        }
        bool ok = stbi_write_png(path.c_str(), int(w), int(h), 3, rgb.data(), int(w * 3)) != 0;
        Log("%s %s (%ux%u)", ok ? "wrote" : "FAILED to write", path.c_str(), w, h);
    }

    /* The displayed buffer as 512x448 RGB24, decoded from local memory the way
       gs_bridge.cpp's StreamFrame does it for the software GS. */
    void Stream() {
        const auto& priv = gs.get_priv_register_state();
        auto P = [&](uint32_t off) { return priv.qwords_lo[(off >> 4) * 2]; };
        const uint64_t pmode = P(0x00);
        int circ = (pmode & 1) ? 1 : (pmode & 2) ? 2 : 1;
        uint64_t dispfb = P(circ == 1 ? 0x70 : 0x90), display = P(circ == 1 ? 0x80 : 0xA0);
        if (circ == 1 && dispfb == 0 && display == 0) dispfb = P(0x90);
        uint32_t fbp = uint32_t(dispfb & 0x1ff), fbw = uint32_t(dispfb >> 9) & 63, psm = uint32_t(dispfb >> 15) & 31;
        uint32_t dbx = uint32_t(dispfb >> 32) & 0x7ff, dby = uint32_t(dispfb >> 43) & 0x7ff;
        if (!fbw) fbw = 8;
        const uint8_t* vram = static_cast<const uint8_t*>(gs.map_vram_read(0, 4u << 20));
        std::vector<uint8_t> v(512 * 448 * 3);
        for (uint32_t y = 0; y < 448; ++y)
            for (uint32_t x = 0; x < 512; ++x) {
                uint32_t px = Pixel(vram, fbp, fbw, psm, x + dbx, y + dby);
                uint8_t* o = &v[(size_t(y) * 512 + x) * 3];
                o[0] = uint8_t(px); o[1] = uint8_t(px >> 8); o[2] = uint8_t(px >> 16);
            }
        std::lock_guard<std::mutex> l(g_stream->m);
        g_stream->buf.swap(v);
        g_stream->ready = true;
        g_stream->cv.notify_one();
    }

    /* Settings the menu changes take effect here, with no restart: paraLLEl-GS
       super-sampling (dynamic), hi-res scanout, the native renderer and DLSS 5. */
    unsigned live_version = 0;
    int live_ss = -1, live_tex = -1;
    void ApplyLive() {
        if (xsgcfg::Version() == live_version) return;
        live_version = xsgcfg::Version();
        int ss = xsgcfg::GetInt("renderer.ssaa");
        SuperSampling rate = SuperSampling::X1;
        switch (ss) {
        case 2: rate = SuperSampling::X2; break;
        case 4: rate = SuperSampling::X4; break;
        case 8: rate = SuperSampling::X8; break;
        case 16: rate = SuperSampling::X16; break;
        default: ss = 1; break;
        }
        const int tex = ss > 1 && xsgcfg::GetBool("renderer.ssaa_textures");
        if (ss != live_ss || tex != live_tex) {
            if (live_ss >= 0) {
                gs.flush();
                gs.set_super_sampling_rate(rate, true, tex != 0);
                Log("SSAA %dx%s (live)", ss, tex ? " +textures" : "");
            }
            live_ss = ss;
            live_tex = tex;
        }
        hires = ss >= 4 && xsgcfg::GetBool("renderer.hires_scanout");
        xsgnative::Apply(gs);
        Neural(xsgcfg::GetBool("renderer.neural"));
    }

    /* DLSS 5 Neural Rendering: the game starts xsg-gpu with the DLSS5VKLayer
       loaded whenever it is installed; the setting switches the helper's model on
       and off through its shared settings (dlssnr-shmctl), starting the helper if
       it is not running. */
    int neural_state = -1;
    void Neural(bool on) {
        // Only the instance the layer is attached to (VKLayer_DLSS5=1) drives the helper; it
        // always syncs it at start, so a model left on by another run cannot linger.
        const char* layer = getenv("VKLayer_DLSS5");
        if (!layer || std::strcmp(layer, "1") != 0 || int(on) == neural_state) return;
        neural_state = on;
#ifdef _WIN32
        Log("DLSS 5 Neural Rendering is not supported on Windows");
#else
        const std::string shm = "/tmp/dlssnr-" + std::to_string(getuid()) + "/shm.bin";
        auto run = [](std::vector<std::string> a) {
            std::vector<char*> argv;
            for (auto& s : a) argv.push_back(&s[0]);
            argv.push_back(nullptr);
            pid_t pid;
            if (posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), environ) == 0) {
                int st;
                waitpid(pid, &st, 0);
                return WIFEXITED(st) && WEXITSTATUS(st) == 0;
            }
            return false;
        };
        if (on && access(shm.c_str(), F_OK) != 0) run({"dlssnr-helper", "start"});
        const bool ok = run({"dlssnr-shmctl", shm, "set", "enabled", on ? "1" : "0"});
        Log("DLSS 5 Neural Rendering %s%s", on ? "on" : "off", ok ? "" : " (dlssnr-shmctl failed: is the helper installed?)");
#endif
    }


    void Vsync(uint32_t frame, uint32_t field) {
        uint64_t pt[8] = {xsgprof::g_on ? xsgprof::Now() : 0};
        auto mark = [&](int i) { if (xsgprof::g_on) pt[i] = xsgprof::Now(); };
        ApplyLive();
        xsgaudio::Poll();
        if (xsgnative::On()) xsgnative::Vsync(frame, gs);
        mark(1);
        VSyncInfo info = {};
        info.phase = field;
        info.dst_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        info.dst_stage = VK_PIPELINE_STAGE_2_BLIT_BIT;
        info.dst_access = VK_ACCESS_2_TRANSFER_READ_BIT;
        info.adapt_to_internal_horizontal_resolution = true;
        info.force_progressive = true;
        info.anti_blur = true;
        info.high_resolution_scanout = hires != 0;
        gs.flush();
        mark(6);
        ScanoutResult so = gs.vsync(info);
        mark(2);
        xsginput::BeginFrame(frame);
#ifdef XSG_GPU_SDL
        if (windowed) platform->poll_input();
        else {   // headless: the test hooks' synthetic events, through the menu first like the window's
            SDL_Event e;
            if (SDL_WasInit(SDL_INIT_EVENTS))
                while (SDL_PollEvent(&e))
                    if (!xsgmenu::Event(e)) xsginput::Event(e);
        }
#endif
        xsginput::Publish();
        // the in-game menu: input, settings, layout for this frame's target
        uint32_t tw = 0, th = 0;
        const bool test = xsgmenu::TestRunning(&tw, &th);
        xsgmenu::Stats st;
        st.windowed = windowed && !test;
#ifdef XSG_GPU_SDL
        if (st.windowed) {
            st.target_w = platform->w;
            st.target_h = platform->h;
        }
#endif
        if (!st.windowed) {
            st.target_w = tw;
            st.target_h = th;
        }
        st.image_w = last_image_w;
        st.image_h = last_image_h;
        st.scan_w = so.image ? so.image->get_width() : 0;
        st.scan_h = so.image ? so.image->get_height() : 0;
        st.present = PresentName();
        st.ring_used = __atomic_load_n(&shm->wpos, __ATOMIC_RELAXED) - __atomic_load_n(&shm->rpos, __ATOMIC_RELAXED);
        st.ring_size = XSG_GPU_RING;
        st.vsyncs = vsyncs;
        st.presents = windowed ? presents : vsyncs;
        st.native_presents = native_presents;
        native_last = native_msgs;
        native_msgs = 0;
        st.native_batches = native_last;
        st.native_on = xsgnative::On();
        st.game_x = game_rect[0]; st.game_y = game_rect[1]; st.game_w = game_rect[2]; st.game_h = game_rect[3];
        mark(3);
        xsgmenu::Frame(frame, st);
        mark(4);
        // A 30 fps scene shows each frame for two vsyncs: the native composite is then the
        // same image, and presenting it again only doubles the present work (and, with DLSS 5,
        // the neural model's).  Skipped while the menu is open (it animates).
        bool repeat = false;
        if (xsgnative::On() && !xsgmenu::IsOpen() && !test) {
            uint32_t nw, nh;
            const Vulkan::Image* live = xsgnative::LiveImage(&nw, &nh).get();
            repeat = live && live == last_live;
            last_live = live;
        } else {
            last_live = nullptr;
        }
        // Behind the game by more than a vblank: process the ring instead of presenting.  A
        // present can cost a whole display refresh (FIFO, or a compositor that throttles
        // mailbox, as on Windows), and when that is slower than the game's 60 Hz the ring
        // backs up and the game waits on it (issue #3).  The display drops a frame instead.
        const bool behind = !test && !xsgmenu::IsOpen() &&
                            int32_t(__atomic_load_n(&shm->game_vblank, __ATOMIC_ACQUIRE) - frame) >= 2;
        if (behind) behind_skips++;
        else if (!repeat) Present(so, !test);
        else repeats++;
        mark(5);
        if (xsgprof::g_on) {
            const double tot = double(pt[5] - pt[0]) * 1e-6;
            if (tot > 12.0)
                Log("[prof] slow vsync %u: %.1f ms = native %.1f, gs %.1f (flush %.1f), input %.1f, menu/ui %.1f, present %.1f", frame, tot,
                    double(pt[1] - pt[0]) * 1e-6, double(pt[2] - pt[1]) * 1e-6, double(pt[6] - pt[1]) * 1e-6, double(pt[3] - pt[2]) * 1e-6,
                    double(pt[4] - pt[3]) * 1e-6, double(pt[5] - pt[4]) * 1e-6);
        }
        if (std::string p = xsgmenu::TestShot(&tw, &th); !p.empty()) WritePngOf(so, p, tw, th, true);
        if (!shot_path.empty()) {
            WritePngOf(so, shot_path, 0, 0, false);
            shot_path.clear();
        }
        if (!windowed) dev->next_frame_context();
        if (StreamWants()) Stream();
        vsyncs++;
        __atomic_store_n(&shm->vsyncs_done, uint32_t(vsyncs), __ATOMIC_RELEASE);
        {   // frame-time spikes (the 600-vsync average hides them): worst frame, frames over 20 ms
            static auto last = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            const double ms = std::chrono::duration<double, std::milli>(now - last).count();
            last = now;
            if (ms > worst_ms) { worst_ms = ms; worst_at = frame; }
            slow_frames += ms > 20.0;
        }
        {   // save the pipeline cache every 10 s while it grows (by time: with a cold cache the
            // renderer can be slow enough that 600 vsyncs take minutes, issue #3)
            static auto last_save = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            if (dev && now - last_save > std::chrono::seconds(10)) { last_save = now; PipelineCacheSave(*dev); }
        }
        if (vsyncs % 600 == 0) {
            double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            Log("vsync %u: %.1f vsyncs/s, %.1f MB/s GIF, %llu presents (%llu native); worst %.1f ms at vsync %u, %u over 20 ms, %llu repeats not presented, %llu skipped behind the game, %u audio underruns", frame,
                600.0 / s, gif_bytes / s / 1e6, (unsigned long long)presents, (unsigned long long)native_presents, worst_ms,
                worst_at, slow_frames, (unsigned long long)repeats, (unsigned long long)behind_skips, xsgaudio::Underruns());
            worst_ms = 0;
            slow_frames = 0;
            t0 = std::chrono::steady_clock::now();
            gif_bytes = 0;
        }
    }

    /* GPU_MSG_SETUP (port/docs/SETUP.md): the first-run setup screen, drawn and presented here at
       ~60 Hz until the user continues with a valid disc image or quits; the game waits for the
       reply.  Closing the window ends xsg-gpu, and the game with it. */
    void Setup(const GpuSetupReq& req) {
        GpuSetupReply* out = reinterpret_cast<GpuSetupReply*>(reply);
        std::memset(out, 0, sizeof *out);
        std::string tried(req.path, strnlen(req.path, sizeof req.path)),
            dir(req.search_dir, strnlen(req.search_dir, sizeof req.search_dir));
#ifdef XSG_GPU_SDL
        if (!windowed) {
            Log("setup: no window (headless); set [game] iso in %s", xsgcfg::Path().c_str());
            out->ok = 2;
            return;
        }
        Log("setup: no usable disc image (reason %u, '%s'): the setup screen", req.reason, tried.c_str());
        xsgmenu::SetupBegin(int(req.reason), tried, dir);
        std::string path;
        int r = 0;
        for (uint32_t n = 1; !r; ++n) {
            const auto t = std::chrono::steady_clock::now();
            xsginput::BeginFrame(n);
            platform->poll_input();
            if (!platform->is_alive) {
                Log("setup: window closed");
                std::_Exit(0);   // the game sees the process go and exits too
            }
            if (getppid() != g_parent) { Log("game went away"); std::_Exit(0); }
            xsgmenu::Stats st;
            st.windowed = true;
            st.target_w = platform->w;
            st.target_h = platform->h;
            st.present = PresentName();
            xsgmenu::Frame(n, st);
            r = xsgmenu::SetupResult(&path);
            Present(ScanoutResult{}, true);
            std::this_thread::sleep_until(t + std::chrono::microseconds(16667));
        }
        if (r > 0) {
            out->ok = 1;
            std::snprintf(out->path, sizeof out->path, "%s", path.c_str());
            Log("setup: %s (saved as [game] iso in %s)", path.c_str(), xsgcfg::Path().c_str());
        } else {
            Log("setup: quit");
        }
#else
        out->ok = 2;
#endif
    }

    void Reply(uint32_t seq) {
        __atomic_store_n(&shm->reply_seq, seq, __ATOMIC_RELEASE);
        Futex(&shm->reply_seq, FUTEX_WAKE, INT32_MAX, nullptr);
    }
};

Renderer* g_r;

void Renderer::InitMenu() {
    g_r = this;
    xsgmenu::Host host;
    host.apply_video = [] { g_r->ApplyVideo(); };
    host.screenshot = [](const std::string& p) { g_r->shot_path = p; };
    host.quit = [] {
#ifdef XSG_GPU_SDL
        if (g_r->platform) g_r->platform->is_alive = false;   // as the window's close button
        else std::_Exit(0);
#else
        std::_Exit(0);
#endif
    };
    auto* settings = reinterpret_cast<GpuSettings*>(reinterpret_cast<uint8_t*>(shm) + XSG_GPU_SETTINGS_OFF);
#ifdef XSG_GPU_SDL
    xsgmenu::Init(dev, platform ? platform->win : nullptr, settings, host);
#else
    xsgmenu::Init(dev, nullptr, settings, host);
#endif
}

}  // namespace

#ifdef _WIN32
#include <tlhelp32.h>
/* A crash in xsg-gpu on Windows ends it with only an exit status in the game's log.
   Log the exception, the module it hit (xsg-gpu.exe or a DLL, with the offset from
   its base, for addr2line on the unstripped build) and the unwound call stack of the
   crashing thread, to stderr (the game's console, or wherever its stderr goes). */
static void CrashModule(uintptr_t a, char* name, size_t n, uintptr_t* base) {
    HMODULE m = nullptr;
    name[0] = 0;
    *base = 0;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(a), &m) && m) {
        *base = reinterpret_cast<uintptr_t>(m);
        char path[MAX_PATH];
        if (GetModuleFileNameA(m, path, sizeof path)) {
            const char* b = std::strrchr(path, '\\');
            std::snprintf(name, n, "%s", b ? b + 1 : path);
        }
    }
}

static void CrashUnwind(CONTEXT c) {
    char mod[128];
    uintptr_t base;
    for (int i = 0; i < 48 && c.Rip; ++i) {
        CrashModule(uintptr_t(c.Rip), mod, sizeof mod, &base);
        std::fprintf(stderr, "[xsg-gpu]   #%d %s+0x%llx\n", i, mod[0] ? mod : "?", (unsigned long long)(c.Rip - base));
        DWORD64 image = 0;
        PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(c.Rip, &image, nullptr);
        if (!fn) {   // leaf function: the return address is at [rsp]
            c.Rip = *reinterpret_cast<DWORD64*>(c.Rsp);
            c.Rsp += 8;
            continue;
        }
        void* handler_data = nullptr;
        DWORD64 frame = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, c.Rip, fn, &c, &handler_data, &frame, nullptr);
    }
}

static LONG CALLBACK OnCrash(EXCEPTION_POINTERS* ep) {
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    static volatile LONG once;
    if (InterlockedExchange(&once, 1)) return EXCEPTION_CONTINUE_SEARCH;
    const CONTEXT* c = ep->ContextRecord;
    char mod[128];
    uintptr_t base;
    CrashModule(uintptr_t(c->Rip), mod, sizeof mod, &base);
    const ULONG_PTR* info = ep->ExceptionRecord->ExceptionInformation;
    const bool av = code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2;
    std::fprintf(stderr, "[xsg-gpu] CRASH exception 0x%08lx at %s+0x%llx (thread %lu)", (unsigned long)code,
                 mod[0] ? mod : "?", (unsigned long long)(c->Rip - base), GetCurrentThreadId());
    if (av)
        std::fprintf(stderr, ", %s of address 0x%llx", info[0] == 1 ? "write" : info[0] == 8 ? "execute" : "read",
                     (unsigned long long)info[1]);
    std::fprintf(stderr, "\n");
    if (code != EXCEPTION_STACK_OVERFLOW) CrashUnwind(*c);
    std::fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(OnCrash);   /* real crashes only, not first-chance exceptions a driver handles */
    /* Bandicam installs an implicit Vulkan layer, VK_LAYER_bandicam_helper (bdcamvk64.dll), that
       loads into every Vulkan app, even with Bandicam closed, and crashes this one at start-up
       (issue #3).  Keep the loader from loading it: by name (loaders from 1.3.234 on; unless the
       player set their own filter), and through the disable_environment variables its manifests
       declare (any loader; the 64-bit and 32-bit manifests name different ones). */
    if (!std::getenv("VK_LOADER_LAYERS_DISABLE")) _putenv("VK_LOADER_LAYERS_DISABLE=*bandicam*");
    _putenv("VK_LAYER_bandicam_helper_DEBUG_1=1");
    _putenv("DISABLE_VK_LAYER_bandicam_helper_1=1");
    /* The game (gpu_client.cpp) passes its file mapping as an inherited handle value in argv[1]
       and its pid as --parent=<pid>; it also puts this process in a kill-on-close job object,
       which ends it with the game (Linux: PR_SET_PDEATHSIG). */
    unsigned long long hmap = 0;
    const char* stream = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (!std::strncmp(argv[i], "--stream=", 9)) stream = argv[i] + 9;
        else if (!std::strncmp(argv[i], "--parent=", 9)) g_parent = std::strtoul(argv[i] + 9, nullptr, 0);
        else hmap = std::strtoull(argv[i], nullptr, 0);
    }
    if (g_parent) g_parent_h = OpenProcess(SYNCHRONIZE, FALSE, DWORD(g_parent));
    void* m = hmap ? MapViewOfFile(HANDLE(uintptr_t(hmap)), FILE_MAP_ALL_ACCESS, 0, 0, XSG_GPU_SHM_SIZE) : nullptr;
    if (!m) { Log("mapping of handle %llu failed (xsg-gpu is started by the game)", hmap); return 1; }
    xsg_w32_ipc_init(m, g_parent);
    xsg_w32_prefer_pcores([](const char* msg) { Log("%s", msg); });
    if (stream && *stream) { Log("[debug] stream is not supported on Windows"); stream = nullptr; }
#else
    std::signal(SIGPIPE, SIG_IGN);
    g_parent = getppid();
    prctl(PR_SET_PDEATHSIG, SIGTERM);  // the game died: go too
    int fd = XSG_GPU_FD;
    const char* stream = nullptr;   // the game passes [debug] stream here when xsg-gpu feeds it
    for (int i = 1; i < argc; ++i) {
        if (!std::strncmp(argv[i], "--stream=", 9)) stream = argv[i] + 9;
        else fd = std::atoi(argv[i]);
    }
    void* m = mmap(nullptr, XSG_GPU_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (m == MAP_FAILED) { Log("mmap of fd %d failed (run by the game with XSG_GS=gpu|both)", fd); return 1; }
#endif
    shm = static_cast<GpuShm*>(m);
    if (shm->magic != XSG_GPU_MAGIC || shm->version != XSG_GPU_VERSION) { Log("bad shm header"); return 1; }
    ring = static_cast<uint8_t*>(m) + XSG_GPU_HDR;
    reply = ring + XSG_GPU_RING;
    xsgaudio::Init(m);

    if (const char* s = stream; s && *s) {
        g_stream = new ::Stream;
        g_stream->path = s;
        std::thread(StreamWriter).detach();
        Log("stream 512x448 rgb24 -> %s", s);
    }

    xsgcfg::Load();   // settings.ini (and the legacy input.ini) before anything reads a setting
    static Renderer r;
    if (!r.Init()) {
        __atomic_store_n(&shm->ready, 2u, __ATOMIC_RELEASE);
        Futex(&shm->ready, FUTEX_WAKE, INT32_MAX, nullptr);
        return 1;
    }
    __atomic_store_n(&shm->ready, 1u, __ATOMIC_RELEASE);
    Futex(&shm->ready, FUTEX_WAKE, INT32_MAX, nullptr);

    const uint32_t mask = XSG_GPU_RING - 1;
    uint32_t rpos = shm->rpos;
    for (;;) {
        uint32_t wpos = __atomic_load_n(&shm->wpos, __ATOMIC_ACQUIRE);
        if (wpos == rpos) {
            // Empty: announce, re-check, sleep (100 ms timeout so a dead game is noticed).
            __atomic_store_n(&shm->wpos_waiting, 1u, __ATOMIC_SEQ_CST);
            wpos = __atomic_load_n(&shm->wpos, __ATOMIC_SEQ_CST);
            if (wpos == rpos) {
                timespec ts = {0, 100 * 1000 * 1000};
                Futex(&shm->wpos, FUTEX_WAIT, wpos, &ts);
                __atomic_store_n(&shm->wpos_waiting, 0u, __ATOMIC_SEQ_CST);
                if (getppid() != g_parent) { Log("game went away"); std::_Exit(0); }
                continue;
            }
            __atomic_store_n(&shm->wpos_waiting, 0u, __ATOMIC_SEQ_CST);
        }
        GpuMsg h;
        std::memcpy(&h, ring + (rpos & mask), sizeof h);
        const uint8_t* payload = ring + ((rpos + 16) & mask);
        uint32_t adv = 16 + h.len;
        const uint64_t prof_t0 = xsgprof::g_on ? xsgprof::Now() : 0;
        switch (h.type) {
        case GPU_MSG_PAD: adv = XSG_GPU_RING - (rpos & mask); break;
        case GPU_MSG_GIF: {
            if (xsgnative::On()) xsgnative::Gif(h.a, payload, h.len, r.gs);   // transfers too (native_fx.cpp)
            else r.gs.gif_transfer(h.a, payload, h.len);
            // button glyphs: replacement uploads right after the icon they replace (glyphs.cpp)
            static std::vector<uint8_t> inj;
            inj.clear();
            xsgglyph::Feed(h.a, payload, h.len, inj);
            if (!inj.empty()) {
                if (xsgnative::On()) xsgnative::Gif(2, inj.data(), uint32_t(inj.size()), r.gs);
                else r.gs.gif_transfer(2, inj.data(), inj.size());
            }
            r.gif_bytes += h.len;
            break;
        }
        case GPU_MSG_PRIV: {
            uint64_t v;
            std::memcpy(&v, payload, 8);
            r.Priv(h.a, v);
            break;
        }
        case GPU_MSG_VSYNC: r.Vsync(h.a, h.b); break;
        case GPU_MSG_NATIVE: xsgnative::Batch(h.a, h.b, payload, h.len); r.native_msgs++; break;
        case GPU_MSG_UI_TEX: xsgui::ArtTexset(h.a, h.b, payload, h.len); break;
        case GPU_MSG_UI_IMAGE: xsgui::ArtImageSet(h.a, h.b & 0xffff, h.b >> 16, payload, h.len); break;
        case GPU_MSG_READ_VRAM: {
            r.gs.flush();
            const void* v = r.gs.map_vram_read(0, 4u << 20);
            std::memcpy(reply, v, 4u << 20);
            r.Reply(h.a);
            break;
        }
        case GPU_MSG_READ_FIFO: {
            uint32_t n = h.b;
            if (n > XSG_GPU_REPLY / 16) n = XSG_GPU_REPLY / 16;
            r.gs.read_transfer_fifo(reply, n);
            r.Reply(h.a);
            break;
        }
        case GPU_MSG_SETUP: {
            static GpuSetupReq req;
            std::memcpy(&req, payload, std::min<size_t>(sizeof req, h.len));
            r.Setup(req);
            r.Reply(h.a);
            break;
        }
        case GPU_MSG_QUIT:
            Log("quit after %llu vsyncs", (unsigned long long)r.vsyncs);
            r.dev->wait_idle();
            std::_Exit(0);
        default:
            Log("bad message type %u at %u", h.type, rpos);
            return 1;
        }
        if (xsgprof::g_on) {   // [debug] gpu_prof: one message that stalls the loop
            const double ms = double(xsgprof::Now() - prof_t0) * 1e-6;
            if (ms > 8.0) Log("[prof] slow message type %u (a %u b %u, %u bytes) at vsync %llu: %.1f ms", h.type, h.a, h.b, h.len,
                              (unsigned long long)r.vsyncs, ms);
        }
        rpos += adv;
        __atomic_store_n(&shm->rpos, rpos, __ATOMIC_SEQ_CST);
        if (__atomic_load_n(&shm->rpos_waiting, __ATOMIC_SEQ_CST)) Futex(&shm->rpos, FUTEX_WAKE, 1, nullptr);
    }
}

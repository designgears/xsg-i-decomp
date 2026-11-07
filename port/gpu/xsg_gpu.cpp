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
 * Environment (inherited from the game):
 *   XSG_GPU_HEADLESS=1   no window (Vulkan device only)
 *   XSG_GPU_SSAA=1|2|4|8|16  paraLLEl-GS super-sampling (default 1)
 *   XSG_GPU_SSAA_TEX=1   super-sampled textures (with SSAA >= 2)
 *   XSG_GPU_HIRES=1      high-resolution scanout (with SSAA >= 4)
 *   XSG_GPU_SCALE=N      initial window size N x 512x448 (default 2)
 *   XSG_GPU_VSYNC=1      FIFO present (blocks on the display); default mailbox
 *   XSG_GPU_PRESENT=mailbox|fifo|immediate   present mode (overrides VSYNC)
 *   XSG_GPU_ASPECT=4:3|16:9|stretch          display aspect (default 4:3)
 *   XSG_GPU_SCALEMODE=fit|integer            image scaling (default fit)
 *   XSG_GPU_FILTER=linear|nearest            blit filter (default linear)
 *   XSG_GPU_FULLSCREEN=1 start fullscreen (F11 toggles)
 *   XSG_GPU_INPUT_TEST   synthetic SDL input events (STAGE10_INPUT.md)
 * The same video options can be set in [video] of ~/.config/xsg/input.ini.
 * Keyboard/gamepad input: xsg_input.cpp, port/docs/STAGE10_INPUT.md.
 *   XSG_GPU_STREAM=fifo  the 512x448 RGB24 live stream (set by the game from
 *                        XSG_STREAM when it does not run the soft GS itself)
 *   XSG_GPU_VVL=1        allow the Vulkan validation layer
 */
#include "gpu_ipc.h"
#include "xsg_input.h"

#include "context.hpp"
#include "device.hpp"
#include "wsi.hpp"
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
#include <linux/futex.h>
#include <memory>
#include <mutex>
#include <string>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace ParallelGS;

namespace {

GpuShm* shm;
pid_t g_parent;
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
    return syscall(SYS_futex, (uint32_t*)addr, op, val, ts, nullptr, 0);
}

bool EnvOn(const char* n) {
    const char* v = std::getenv(n);
    return v && *v && std::strcmp(v, "0") != 0;
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
                continue;
            }
            if (xsginput::Event(e)) continue;
            if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) is_alive = false;
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
    uint64_t vsyncs = 0, gif_bytes = 0, presents = 0;
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
        int scale = std::getenv("XSG_GPU_SCALE") ? std::atoi(std::getenv("XSG_GPU_SCALE")) : 2;
        if (scale < 1) scale = 1;
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
        std::string pm = xsginput::Option("present", "XSG_GPU_PRESENT", EnvOn("XSG_GPU_VSYNC") ? "fifo" : "mailbox");
        Vulkan::PresentMode mode = Vulkan::PresentMode::UnlockedNoTearing;
        if (pm == "fifo" || pm == "vsync") mode = Vulkan::PresentMode::SyncToVBlank;
        else if (pm == "immediate") mode = Vulkan::PresentMode::UnlockedForceTearing;
        else if (pm == "auto") mode = Vulkan::PresentMode::UnlockedMaybeTear;
        wsi->set_present_mode(mode);
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
        if (!EnvOn("XSG_GPU_VVL")) setenv("GRANITE_VULKAN_NO_VALIDATION", "1", 0);
        if (!Vulkan::Context::init_loader(nullptr)) { Log("no Vulkan loader"); return false; }
        if (!EnvOn("XSG_GPU_HEADLESS") && !InitWindow()) Log("no window; running headless");
        bool sdl_input = windowed;
#ifdef XSG_GPU_SDL
        // Headless: input only for the synthetic test hook (no real devices).
        if (!windowed && std::getenv("XSG_GPU_INPUT_TEST") && *std::getenv("XSG_GPU_INPUT_TEST"))
            sdl_input = SDL_Init(SDL_INIT_EVENTS);
#endif
        xsginput::Init(reinterpret_cast<GpuPad*>(reinterpret_cast<uint8_t*>(shm) + XSG_GPU_PAD_OFF), sdl_input);
        if (!dev && !InitHeadless()) { Log("no Vulkan device"); return false; }
        GSOptions o = {};
        int ss = std::getenv("XSG_GPU_SSAA") ? std::atoi(std::getenv("XSG_GPU_SSAA")) : 1;
        switch (ss) {
        case 2: o.super_sampling = SuperSampling::X2; break;
        case 4: o.super_sampling = SuperSampling::X4; break;
        case 8: o.super_sampling = SuperSampling::X8; break;
        case 16: o.super_sampling = SuperSampling::X16; break;
        default: ss = 1; o.super_sampling = SuperSampling::X1; break;
        }
        o.super_sampled_textures = ss > 1 && EnvOn("XSG_GPU_SSAA_TEX");
        hires = ss >= 4 && EnvOn("XSG_GPU_HIRES");
        if (!gs.init(dev, o)) { Log("GSInterface::init failed"); return false; }
        const auto& p = dev->get_gpu_properties();
        Log("paraLLEl-GS on \"%s\" (Vulkan %u.%u), %s, SSAA %dx%s%s", p.deviceName, VK_API_VERSION_MAJOR(p.apiVersion),
            VK_API_VERSION_MINOR(p.apiVersion), windowed ? "windowed" : "headless", ss,
            o.super_sampled_textures ? " +textures" : "", hires ? " +hires scanout" : "");
        return true;
    }

    void Priv(uint32_t off, uint64_t v) {
        auto& priv = const_cast<PrivRegisterState&>(gs.get_priv_register_state());
        off &= 0x1ff0;
        if (off < 0x1000) priv.qwords_lo[(off >> 4) * 2] = v;
        else priv.qwords_hi[((off - 0x1000) >> 4) * 2] = v;
    }

    /* Destination size in a bw x bh backbuffer: the display aspect (4:3 unless
       "stretch"), and with integer scaling the largest whole multiple of the
       scanout height that fits (fit scaling if even 1x does not). */
    void Fit(int bw, int bh, int src_h, int* dw, int* dh) const {
        if (aspect == "stretch") { *dw = bw; *dh = bh; return; }
        int an = 4, ad = 3;
        if (aspect == "16:9") { an = 16; ad = 9; }
        int h = bh, w = bh * an / ad;
        if (w > bw) { w = bw; h = bw * ad / an; }
        if (integer_scale && src_h > 0) {
            int k = h / src_h;
            if (k >= 1) { h = k * src_h; w = h * an / ad; }
        }
        *dw = w;
        *dh = h;
    }

    void Present(const ScanoutResult& so) {
#ifdef XSG_GPU_SDL
        if (!windowed) return;
        if (!platform->is_alive) {
            Log("window closed");
            std::_Exit(0);   // the game sees the process go and exits too
        }
        if (!wsi->begin_frame()) return;
        auto cmd = dev->request_command_buffer();
        auto& bb = dev->get_swapchain_view().get_image();
        cmd->swapchain_touch_in_stages(VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT);
        cmd->image_barrier(bb, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT, 0,
                           VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        VkClearValue clear = {};
        cmd->clear_image(bb, clear);
        if (so.image) {
            int dw, dh;
            Fit(int(bb.get_width()), int(bb.get_height()), int(so.image->get_height()), &dw, &dh);
            const int x0 = (int(bb.get_width()) - dw) / 2, y0 = (int(bb.get_height()) - dh) / 2;
            cmd->barrier(VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT,
                         VK_ACCESS_2_TRANSFER_WRITE_BIT);
            cmd->blit_image(bb, *so.image, {x0, y0, 0}, {dw, dh, 1}, {0, 0, 0},
                            {int(so.image->get_width()), int(so.image->get_height()), 1}, 0, 0, 0, 0, 1,
                            nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
        }
        cmd->image_barrier(bb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                           VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                           VK_PIPELINE_STAGE_2_NONE, 0);
        dev->submit(cmd);
        wsi->end_frame();
        presents++;
#endif
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

    void Vsync(uint32_t frame, uint32_t field) {
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
        ScanoutResult so = gs.vsync(info);
        xsginput::BeginFrame(frame);
#ifdef XSG_GPU_SDL
        if (windowed) platform->poll_input();
        else xsginput::PumpHeadless();
#endif
        xsginput::Publish();
        Present(so);
        if (!windowed) dev->next_frame_context();
        if (StreamWants()) Stream();
        vsyncs++;
        __atomic_store_n(&shm->vsyncs_done, uint32_t(vsyncs), __ATOMIC_RELEASE);
        if (vsyncs % 600 == 0) {
            double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            Log("vsync %u: %.1f vsyncs/s, %.1f MB/s GIF, %llu presents", frame, 600.0 / s, gif_bytes / s / 1e6,
                (unsigned long long)presents);
            t0 = std::chrono::steady_clock::now();
            gif_bytes = 0;
        }
    }

    void Reply(uint32_t seq) {
        __atomic_store_n(&shm->reply_seq, seq, __ATOMIC_RELEASE);
        Futex(&shm->reply_seq, FUTEX_WAKE, INT32_MAX, nullptr);
    }
};

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    g_parent = getppid();
    prctl(PR_SET_PDEATHSIG, SIGTERM);  // the game died: go too
    int fd = XSG_GPU_FD;
    if (argc > 1) fd = std::atoi(argv[1]);
    void* m = mmap(nullptr, XSG_GPU_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (m == MAP_FAILED) { Log("mmap of fd %d failed (run by the game with XSG_GS=gpu|both)", fd); return 1; }
    shm = static_cast<GpuShm*>(m);
    if (shm->magic != XSG_GPU_MAGIC || shm->version != XSG_GPU_VERSION) { Log("bad shm header"); return 1; }
    ring = static_cast<uint8_t*>(m) + XSG_GPU_HDR;
    reply = ring + XSG_GPU_RING;

    if (const char* s = std::getenv("XSG_GPU_STREAM"); s && *s) {
        g_stream = new ::Stream;
        g_stream->path = s;
        std::thread(StreamWriter).detach();
        Log("stream 512x448 rgb24 -> %s", s);
    }

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
        switch (h.type) {
        case GPU_MSG_PAD: adv = XSG_GPU_RING - (rpos & mask); break;
        case GPU_MSG_GIF:
            r.gs.gif_transfer(h.a, payload, h.len);
            r.gif_bytes += h.len;
            break;
        case GPU_MSG_PRIV: {
            uint64_t v;
            std::memcpy(&v, payload, 8);
            r.Priv(h.a, v);
            break;
        }
        case GPU_MSG_VSYNC: r.Vsync(h.a, h.b); break;
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
        case GPU_MSG_QUIT:
            Log("quit after %llu vsyncs", (unsigned long long)r.vsyncs);
            r.dev->wait_idle();
            std::_Exit(0);
        default:
            Log("bad message type %u at %u", h.type, rpos);
            return 1;
        }
        rpos += adv;
        __atomic_store_n(&shm->rpos, rpos, __ATOMIC_SEQ_CST);
        if (__atomic_load_n(&shm->rpos_waiting, __ATOMIC_SEQ_CST)) Futex(&shm->rpos, FUTEX_WAKE, 1, nullptr);
    }
}

/*
 * The in-game menu of xsg-gpu (port/docs/MENU.md): Dear ImGui drawn over the
 * game image on the swapchain, modelled on TotK Recomp's runtime/ui/menu.cpp
 * (MIT, see port/third_party/THIRD_PARTY.md) and themed after Xenosaga's own
 * UI.
 *
 * F1 / Esc (or Back+Start / Guide on a gamepad) opens it; F1, Esc, B or
 * Continue close it.  F10 cycles the performance overlay off -> fps -> full.
 * While it is open the game keeps running with a neutral pad and the game
 * image is dimmed.  Settings live in menu/cfg.h (settings.ini); the ones the
 * game process applies (FPS limit, audio) go through GpuSettings in shared
 * memory (gpu_ipc.h).
 *
 * Test hook: XSG_GPU_MENU_TEST=<vsync>[:<vsyncs per page>[:<W>x<H>]] opens
 * the menu at that vsync and writes one PNG per tab (and one of the full
 * overlay) to XSG_GPU_MENU_TEST_DIR (default "."), also headless.
 * XSG_GPU_MENU_TEST_SET=<vsync>:<key>=<value>[,...][;...] sets settings at
 * given vsyncs without input (key SAVE=1 writes settings.ini, OPEN=0|1 opens
 * or closes the menu).
 */
#ifndef XSG_MENU_H
#define XSG_MENU_H

#include <cstdint>
#include <string>

#include "gpu_ipc.h"

#ifdef XSG_GPU_SDL
union SDL_Event;
struct SDL_Window;
#endif
namespace Vulkan { class Device; class CommandBuffer; }

namespace xsgmenu {

/* What the overlay shows; filled by xsg_gpu.cpp before Frame(). */
struct Stats {
    uint32_t target_w = 0, target_h = 0;   // the swapchain (or the test image)
    uint32_t image_w = 0, image_h = 0;     // the game image presented (source rectangle)
    uint32_t scan_w = 0, scan_h = 0;       // the paraLLEl-GS scanout
    const char* present = "";              // present mode in use
    uint32_t ring_used = 0, ring_size = 0;
    uint64_t vsyncs = 0, presents = 0, native_presents = 0;
    uint32_t native_batches = 0;           // GPU_MSG_NATIVE in the last vsync
    bool native_on = false;
    bool windowed = false;
};

struct Host {
    void (*apply_video)() = nullptr;                    // a video.* setting changed (outside a frame)
    void (*screenshot)(const std::string& path) = nullptr;   // a PNG of the next game frame
    void (*quit)() = nullptr;
};

#ifdef XSG_GPU_SDL
void Init(Vulkan::Device* dev, SDL_Window* win, GpuSettings* shared, const Host& host);
/* Every SDL event first; true when the menu consumed it. */
bool Event(const SDL_Event& e);
#else
void Init(Vulkan::Device* dev, void* win, GpuSettings* shared, const Host& host);
#endif
/* Once per vsync, before presenting: input, settings, layout. */
void Frame(uint32_t vsync, const Stats& s);
/* This frame has something to draw (menu open, fading, or an overlay). */
bool Visible();
/* Inside a render pass on the target Frame() laid out for. */
void Draw(Vulkan::CommandBuffer& cmd, uint32_t w, uint32_t h);
bool IsOpen();
/* The test hook wants this frame saved (path), else empty; and its image size. */
std::string TestShot(uint32_t* w, uint32_t* h);
bool TestRunning(uint32_t* w, uint32_t* h);

}  // namespace xsgmenu

#endif

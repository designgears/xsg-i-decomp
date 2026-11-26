/*
 * xsg-gpu native renderer: per-stage CPU timers and GPU timestamps
 * (XSG_GPU_PROF=1, port/docs/NATIVE_RENDERER.md §9).  Every 600 vsyncs a
 * "[prof]" line gives ms per vsync for each stage, plus the GPU time of the
 * live pass (Vulkan timestamps).
 */
#ifndef XSG_GPU_NATIVE_PROF_H
#define XSG_GPU_NATIVE_PROF_H

#include <chrono>
#include <cstdint>
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

namespace xsgprof {

enum Stage {
    GifParse,     // native.cpp's GIF state tracker
    GifFx,        // native_fx.cpp's GIF parser (vertices, post phase, ownership stream)
    GifMain,      // paraLLEl-GS gif_transfer, main GS
    GifOwn,       // ownership GS: gif_transfer (or the hand-off to its thread)
    OwnWait,      // waiting for the ownership GS thread
    Batch,        // GPU_MSG_NATIVE handling (textures, skin arena)
    TexDecode,    // CPU texture decode (part of Batch)
    BatchSkin,    // set-2 message: arena, history (part of Batch)
    BatchSnap,    // GS state + texture lookup per stored pass (part of Batch)
    Snapshot,     // effect snapshot at the post-phase trigger
    FxVsync,      // xsgfx::Vsync (ownership flush)
    LiveWait,     // Live: GS flush + VRAM wait
    LiveDecode,   // Live: CPU decode of the displayed buffer (colour, Z, owner)
    OwnRead,      // ownership GS flush + VRAM read-back (part of LiveDecode)
    LiveRecord,   // Live: command recording (draws, composite, replay, mv)
    LiveSubmit,   // Live: submit
    Other,        // rest of xsgnative::Vsync
    kStages
};

extern bool g_on;
extern uint64_t g_ns[kStages];
extern double g_gpu_ms[4];   // live pass GPU time, summed: Sx render, 1x render (colour rule only), composite+replay+mv
extern uint64_t g_gpu_n;

/* Nanoseconds from the CPU's time-stamp counter (a few cycles; the profile
   scopes run per message, and steady_clock was ~8% of xsg-gpu with the
   profiler on).  g_ns_per_tick is calibrated against steady_clock by Init. */
extern double g_ns_per_tick;
inline uint64_t SteadyNs() {
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count());
}
inline uint64_t Now() { return uint64_t(double(__rdtsc()) * g_ns_per_tick); }

struct Scope {
    Stage s;
    uint64_t t0;
    explicit Scope(Stage st) : s(st), t0(g_on ? Now() : 0) {}
    ~Scope() { if (g_on) g_ns[s] += Now() - t0; }
};

void Init();
/* At each vsync (after the frame's work): report every 600. */
void Vsync(uint32_t frame);

}  // namespace xsgprof

#endif

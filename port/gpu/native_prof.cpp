/* xsg-gpu: per-stage timers (native_prof.h). */
#include "native_prof.h"
#include "menu/cfg.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace xsgprof {
bool g_on;
uint64_t g_ns[kStages];
double g_ns_per_tick = 1.0;
double g_gpu_ms[4];
uint64_t g_gpu_n;

namespace {
const char* kName[kStages] = {"gif-parse", "gif-fx", "gif-main", "gif-own", "own-wait", "batch", "tex-decode", "batch-skin", "batch-snap", "snapshot", "fx-vsync",
                              "live-wait", "live-decode", "own-read", "live-record", "live-submit", "other"};
uint64_t g_last;
}  // namespace

void Init() {
    const char* e = xsgcfg::CStr("debug.gpu_prof");
    g_on = e && *e && std::strcmp(e, "0") != 0;
    if (g_on) {
        /* TSC rate over 20 ms of steady_clock (an invariant TSC on any CPU this runs on) */
        const uint64_t n0 = SteadyNs(), t0 = __rdtsc();
        while (SteadyNs() - n0 < 20000000ull) {}
        const uint64_t n1 = SteadyNs(), t1 = __rdtsc();
        if (t1 > t0) g_ns_per_tick = double(n1 - n0) / double(t1 - t0);
        g_last = Now();
    }
}

void Vsync(uint32_t frame) {
    if (!g_on || frame % 600 != 0) return;
    const uint64_t now = Now();
    const double per = 1e-6 / 600.0, wall = double(now - g_last) * per;
    char b[1024];
    int o = std::snprintf(b, sizeof b, "[prof] vsync %u: wall %.2f ms/vsync;", frame, wall);
    double sum = 0;
    for (int i = 0; i < kStages; ++i) {
        if (i != TexDecode && i != OwnWait && i != BatchSkin && i != BatchSnap && i != OwnRead) sum += double(g_ns[i]) * per;   // inside batch / snapshot / live-decode
        o += std::snprintf(b + o, sizeof b - o, " %s %.2f", kName[i], double(g_ns[i]) * per);
        g_ns[i] = 0;
    }
    o += std::snprintf(b + o, sizeof b - o, "; outside native %.2f", wall - sum);
    if (g_gpu_n)
        o += std::snprintf(b + o, sizeof b - o, "; GPU/live frame (%llu): Sx %.2f 1x %.2f post %.2f ms",
                           (unsigned long long)g_gpu_n, g_gpu_ms[0] / g_gpu_n, g_gpu_ms[1] / g_gpu_n,
                           g_gpu_ms[2] / g_gpu_n);
    g_gpu_n = 0;
    for (double& d : g_gpu_ms) d = 0;
    std::fprintf(stderr, "%s\n", b);
    g_last = now;
}

}  // namespace xsgprof

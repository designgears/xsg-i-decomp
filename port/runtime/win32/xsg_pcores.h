/* Windows: keep the game and xsg-gpu on the fast cores (#3).
 *
 * Hybrid CPUs (Intel 12th gen and later) have performance and efficiency cores.
 * Windows 10 has no Thread Director support and often runs a busy thread on an
 * efficiency core, where the game and the renderer ran 2-4 times slower
 * (late frames in battle effects, an i5-13600KF on Windows 10).  When the
 * system reports more than one core class, the process's default CPU set
 * becomes the cores of the highest class.  Power throttling (EcoQoS) is turned
 * off as well.  Both APIs are looked up at run time (Windows 10 1709+); on
 * older systems, or a CPU with one core class, nothing changes.
 * Header-only, so the game (xsg.dll) and xsg-gpu.exe share it. */
#pragma once
#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static inline void xsg_w32_prefer_pcores(void (*log)(const char *msg))
{
    HMODULE k = GetModuleHandleA("kernel32.dll");
    char msg[160];
    if (!k) return;

    /* EcoQoS off: PROCESS_POWER_THROTTLING_STATE {Version, ControlMask, StateMask},
     * ProcessPowerThrottling = 4, PROCESS_POWER_THROTTLING_EXECUTION_SPEED = 1. */
    typedef BOOL(WINAPI * SetProcInfo)(HANDLE, int, LPVOID, DWORD);
    SetProcInfo spi = (SetProcInfo)(void (*)(void))GetProcAddress(k, "SetProcessInformation");
    if (spi) {
        struct { ULONG version, control, state; } pt = {1, 1, 0};
        spi(GetCurrentProcess(), 4, &pt, sizeof pt);
    }

    typedef BOOL(WINAPI * GetCpuSets)(PVOID, ULONG, PULONG, HANDLE, ULONG);
    typedef BOOL(WINAPI * SetDefCpuSets)(HANDLE, const ULONG *, ULONG);
    GetCpuSets gcs = (GetCpuSets)(void (*)(void))GetProcAddress(k, "GetSystemCpuSetInformation");
    SetDefCpuSets sds = (SetDefCpuSets)(void (*)(void))GetProcAddress(k, "SetProcessDefaultCpuSets");
    if (!gcs || !sds) return;
    ULONG len = 0;
    gcs(NULL, 0, &len, GetCurrentProcess(), 0);
    if (!len) return;
    unsigned char *buf = (unsigned char *)malloc(len);
    if (!buf) return;
    if (!gcs(buf, len, &len, GetCurrentProcess(), 0)) { free(buf); return; }

    /* SYSTEM_CPU_SET_INFORMATION: Size (DWORD), Type (DWORD, 0 = CpuSetInformation),
     * then CpuSet: Id (DWORD) +8, Group (WORD) +12, LogicalProcessorIndex +14,
     * CoreIndex +15, LastLevelCacheIndex +16, NumaNodeIndex +17, EfficiencyClass +18. */
    unsigned char lo = 255, hi = 0;
    ULONG n = 0;
    for (ULONG off = 0; off + 19 <= len;) {
        const DWORD size = *(DWORD *)(buf + off), type = *(DWORD *)(buf + off + 4);
        if (!size) break;
        if (type == 0) {
            const unsigned char ec = buf[off + 18];
            if (ec < lo) lo = ec;
            if (ec > hi) hi = ec;
            n++;
        }
        off += size;
    }
    if (n && hi > lo) {
        ULONG ids[256];
        ULONG m = 0;
        for (ULONG off = 0; off + 19 <= len && m < 256;) {
            const DWORD size = *(DWORD *)(buf + off), type = *(DWORD *)(buf + off + 4);
            if (!size) break;
            if (type == 0 && buf[off + 18] == hi) ids[m++] = *(DWORD *)(buf + off + 8);
            off += size;
        }
        if (m && sds(GetCurrentProcess(), ids, m)) {
            snprintf(msg, sizeof msg, "hybrid CPU: running on the %lu performance cores of %lu", (unsigned long)m,
                     (unsigned long)n);
            if (log) log(msg);
        }
    }
    free(buf);
}
#endif

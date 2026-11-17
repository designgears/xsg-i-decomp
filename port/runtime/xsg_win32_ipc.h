/*
 * Windows port (port/docs/WINDOWS.md): the cross-process wait/wake that the
 * Linux build does with shared futexes on the GpuShm words (gpu_ipc.h).
 *
 * Windows has no cross-process futex (WaitOnAddress works only inside one
 * process), so every shared word that someone sleeps on gets a named
 * auto-reset event, "Local\xsg-gpu-<game pid>-<offset of the word>".  Both
 * processes open the same events lazily (CreateEvent opens an existing one).
 * The protocol is unchanged: a waiter announces itself (the *_waiting flags),
 * re-checks the word and sleeps with a timeout; a waker stores the word and
 * signals.  An auto-reset event stays signalled until the next wait, so a wake
 * between the re-check and the wait is not lost; a stale signal only causes
 * one spurious wake-up, which every caller already tolerates (they loop).
 *
 * The shared memory itself is a pagefile-backed file mapping created by the
 * game with an inheritable handle; xsg-gpu.exe gets the handle value as
 * argv[1] (where Linux passes fd 3) and the game's pid as --parent=<pid>.
 *
 * Header-only, C and C++; included by runtime/gpu_client.cpp and
 * gpu/xsg_gpu.cpp under _WIN32 only.
 */
#ifndef XSG_WIN32_IPC_H
#define XSG_WIN32_IPC_H
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifndef FUTEX_WAIT
#define FUTEX_WAIT 0
#endif
#ifndef FUTEX_WAKE
#define FUTEX_WAKE 1
#endif

#define XSG_W32_IPC_EVENTS 16

typedef struct {
    const volatile uint8_t *base;   /* the mapping's address in this process */
    unsigned long owner;            /* the game's pid: names are unique per game instance */
    uint32_t off[XSG_W32_IPC_EVENTS];
    HANDLE ev[XSG_W32_IPC_EVENTS];
    volatile LONG lock;
} XsgW32Ipc;

static XsgW32Ipc xsg_w32_ipc;

static inline void xsg_w32_ipc_init(const volatile void *shm_base, unsigned long owner_pid)
{
    xsg_w32_ipc.base = (const volatile uint8_t *)shm_base;
    xsg_w32_ipc.owner = owner_pid;
}

/* The event of one shared word (created on first use). */
static inline HANDLE xsg_w32_ipc_event(const volatile uint32_t *addr)
{
    const uint32_t off = (uint32_t)((const volatile uint8_t *)addr - xsg_w32_ipc.base);
    for (int i = 0; i < XSG_W32_IPC_EVENTS; i++)
        if (xsg_w32_ipc.ev[i] && xsg_w32_ipc.off[i] == off)
            return xsg_w32_ipc.ev[i];
    while (InterlockedExchange(&xsg_w32_ipc.lock, 1))
        Sleep(0);
    HANDLE h = NULL;
    int i;
    for (i = 0; i < XSG_W32_IPC_EVENTS && xsg_w32_ipc.ev[i]; i++)
        if (xsg_w32_ipc.off[i] == off)
            h = xsg_w32_ipc.ev[i];
    if (!h && i < XSG_W32_IPC_EVENTS) {
        char name[64];
        snprintf(name, sizeof name, "Local\\xsg-gpu-%lu-%x", xsg_w32_ipc.owner, (unsigned)off);
        h = CreateEventA(NULL, FALSE, FALSE, name);
        if (h) {
            xsg_w32_ipc.off[i] = off;
            MemoryBarrier();
            xsg_w32_ipc.ev[i] = h;
        }
    }
    InterlockedExchange(&xsg_w32_ipc.lock, 0);
    return h;
}

/* futex(2) as gpu_client.cpp / xsg_gpu.cpp call it: FUTEX_WAIT sleeps while
 * *addr == val (up to ts, or forever), FUTEX_WAKE signals the word's event. */
static inline long xsg_w32_futex(volatile uint32_t *addr, int op, uint32_t val, const struct timespec *ts)
{
    HANDLE h = xsg_w32_ipc_event(addr);
    if (!h) {
        Sleep(1);
        return -1;
    }
    if (op == FUTEX_WAKE) {
        SetEvent(h);
        return 0;
    }
    if (*addr != val)
        return -1;
    DWORD ms = INFINITE;
    if (ts)
        ms = (DWORD)(ts->tv_sec * 1000 + (ts->tv_nsec + 999999) / 1000000);
    return WaitForSingleObject(h, ms) == WAIT_OBJECT_0 ? 0 : -1;
}

#endif /* _WIN32 */
#endif /* XSG_WIN32_IPC_H */

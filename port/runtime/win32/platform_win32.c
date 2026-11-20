/*
 * Windows port (port/docs/WINDOWS.md): the host runtime's platform layer for
 * MinGW-w64 i686.  Built into xsg.dll only (port/CMakeLists.txt, WIN32).
 *
 *   - xsg_win32_start: the entry the stub xsg.exe calls (win32/stub.c).  The
 *     stub's image reserves PS2 RAM (0x10000-0x2000000) before Windows
 *     allocates anything; xsg_w32_map_ps2_memory unmaps that image and maps
 *     the three EE RAM views of one section in its place.
 *   - crash reporting (vectored exception handler), the [debug] timeout
 *     watchdog, backtrace(), fnmatch(), posix_spawn/waitpid/kill over
 *     CreateProcess, and a high-resolution clock_nanosleep.
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <fcntl.h>
#include <io.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "xsg_rt.h"
#include "spawn.h"
#include "sys/wait.h"

/* ---- process start ---------------------------------------------------- */
static char g_exe_path[MAX_PATH * 2];
static uintptr_t g_stub_base, g_stub_end;

const char *xsg_w32_exe_path(void) { return g_exe_path; }

extern int xsg_host_main(int argc, char **argv);   /* host_main.c's main() */

/* Called by the stub's entry point (win32/stub.c) with the bounds of its
   reservation; never returns. */
__declspec(dllexport) void xsg_win32_start(void *reserve_lo, void *reserve_hi)
{
    (void)reserve_hi;
    _set_fmode(_O_BINARY);
    _setmode(0, _O_BINARY);
    timeBeginPeriod(1);
    DWORD n = GetModuleFileNameA(NULL, g_exe_path, sizeof g_exe_path);
    if (n == 0 || n >= sizeof g_exe_path)
        g_exe_path[0] = 0;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reserve_lo, &mbi, sizeof mbi)) {
        g_stub_base = (uintptr_t)mbi.AllocationBase;
        uintptr_t p = g_stub_base;
        while (VirtualQuery((void *)p, &mbi, sizeof mbi) && (uintptr_t)mbi.AllocationBase == g_stub_base)
            p = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        g_stub_end = p;
    }
    ExitProcess((UINT)xsg_host_main(__argc, __argv));
}

/* ---- PS2 memory -------------------------------------------------------- */
typedef LONG(NTAPI *NtUnmapViewOfSection_t)(HANDLE, PVOID);

static void map_fail(const char *what, uint32_t addr, uint32_t size)
{
    fprintf(stderr, "xsg: cannot map %s at 0x%08x (+0x%x): error %lu\n", what, addr, size, GetLastError());
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t p = addr; p < (uintptr_t)addr + size && VirtualQuery((void *)p, &mbi, sizeof mbi);
         p = (uintptr_t)mbi.BaseAddress + mbi.RegionSize)
        if (mbi.State != MEM_FREE)
            fprintf(stderr, "xsg:   in use: 0x%08lx-0x%08lx (allocation 0x%08lx, type 0x%lx)\n",
                    (unsigned long)(uintptr_t)mbi.BaseAddress,
                    (unsigned long)((uintptr_t)mbi.BaseAddress + mbi.RegionSize),
                    (unsigned long)(uintptr_t)mbi.AllocationBase, mbi.Type);
    exit(1);
}

static void map_fixed(uint32_t addr, uint32_t size, const char *what)
{
    void *p = VirtualAlloc((void *)(uintptr_t)addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (p != (void *)(uintptr_t)addr)
        map_fail(what, addr, size);
}

void xsg_w32_map_ps2_memory(void)
{
    /* Allocation granularity is 64 KiB: RAM from 0x10000, as on Linux. */
    xsg_ram_low = 0x10000;
    const uint32_t len = PS2_RAM_SIZE - xsg_ram_low;
    HANDLE sec = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, PS2_RAM_SIZE, NULL);
    if (!sec) {
        fprintf(stderr, "xsg: CreateFileMapping for EE RAM: error %lu\n", GetLastError());
        exit(1);
    }
    /* The stub's image is the reservation: release it.  Nothing else runs yet
       (the game's threads start later), so the range stays free until the
       views below take it. */
    /* Windows reads the main image's PE header (PEB->ImageBaseAddress) whenever a
       thread is created, for the default stack sizes.  With the stub unmapped that
       page would be EE RAM and every later CreateThread fails with
       ERROR_BAD_EXE_FORMAT (the sound driver's threads: the game hangs at boot,
       issue #3; Wine does not look).  Keep a copy of the header page and put it
       back at the image base, in the EE kernel area (0x10000), which the game
       does not use. */
    static unsigned char stub_header[0x1000];
    int restore_header = 0;
    if (g_stub_base && g_stub_base <= xsg_ram_low && g_stub_end >= PS2_RAM_SIZE) {
        memcpy(stub_header, (void *)g_stub_base, sizeof stub_header);
        restore_header = 1;
        NtUnmapViewOfSection_t unmap =
            (NtUnmapViewOfSection_t)(void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtUnmapViewOfSection");
        LONG st = unmap ? unmap(GetCurrentProcess(), (void *)g_stub_base) : -1;
        if (st != 0)
            xsg_logf("[mem] releasing the stub's reservation at 0x%08lx failed (0x%lx)\n",
                     (unsigned long)g_stub_base, (unsigned long)st);
    } else {
        xsg_logf("[mem] no stub reservation (image 0x%08lx-0x%08lx): mapping PS2 RAM directly\n",
                 (unsigned long)g_stub_base, (unsigned long)g_stub_end);
    }
    for (int view = 0; view < 3; view++) {
        static const uint32_t bases[3] = {0, 0x20000000u, 0x30000000u};
        void *want = (void *)(uintptr_t)(bases[view] + xsg_ram_low);
        void *p = MapViewOfFileEx(sec, FILE_MAP_WRITE, 0, xsg_ram_low, len, want);
        if (p != want) {
            if (view == 0)
                map_fail("EE main RAM", xsg_ram_low, len);
            xsg_logf("[mem] RAM view at 0x%08x not mapped (error %lu)\n", bases[view], GetLastError());
        }
    }
    if (restore_header)
        memcpy((void *)g_stub_base, stub_header, sizeof stub_header);
    map_fixed(PS2_EEHW_BASE, PS2_EEHW_SIZE, "EE hardware registers");
    map_fixed(PS2_VU_BASE, PS2_VU_SIZE, "VU0/VU1 micro and data memory");
    map_fixed(PS2_GSPRIV_BASE, PS2_GSPRIV_SIZE, "GS privileged registers");
    map_fixed(PS2_SPR_BASE, PS2_SPR_SIZE, "scratchpad");
    xsg_logf("[mem] RAM 0x%08x-0x%08x (views at +0x20000000/+0x30000000), EE HW 0x%08x, VU mem 0x%08x, "
             "GS priv 0x%08x, SPR 0x%08x\n", xsg_ram_low, PS2_RAM_SIZE, PS2_EEHW_BASE, PS2_VU_BASE,
             PS2_GSPRIV_BASE, PS2_SPR_BASE);
}

/* ---- EE threads as fibers (kernel.c) ----------------------------------- */
struct fiber_start { void (*fn)(int); int arg; };

static VOID CALLBACK fiber_main(void *p)
{
    struct fiber_start st = *(struct fiber_start *)p;
    free(p);
    st.fn(st.arg);   /* never returns: a finished EE thread is switched away from */
}

void *xsg_w32_fiber_init(void)
{
    void *f = GetCurrentFiber();
    if (f && f != (void *)0x1e00)   /* already a fiber */
        return f;
    return ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
}

void *xsg_w32_fiber_create(unsigned stack, void (*fn)(int), int arg)
{
    struct fiber_start *st = malloc(sizeof *st);
    st->fn = fn;
    st->arg = arg;
    void *f = CreateFiberEx(64 << 10, stack, FIBER_FLAG_FLOAT_SWITCH, fiber_main, st);
    if (!f)
        free(st);
    return f;
}

void xsg_w32_fiber_switch(void *f) { SwitchToFiber(f); }

void xsg_w32_fiber_delete(void *f)
{
    if (f && f != GetCurrentFiber())
        DeleteFiber(f);
}

/* The running fiber's committed stack, from the TIB (SwitchToFiber keeps it current). */
void xsg_w32_stack_bounds(char **lo, char **hi)
{
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    *lo = (char *)tib->StackLimit;
    *hi = (char *)tib->StackBase;
}

/* ---- backtrace --------------------------------------------------------- */
int backtrace(void **buf, int n)
{
    return RtlCaptureStackBackTrace(1, (DWORD)(n > 62 ? 62 : n), buf, NULL);
}

static void fd_hex(int fd, const char *label, unsigned long v)
{
    char b[96];
    int n = snprintf(b, sizeof b, "%s0x%08lx\n", label, v);
    if (n > 0)
        _write(fd, b, (unsigned)n);
}

void backtrace_symbols_fd(void *const *buf, int n, int fd)
{
    for (int i = 0; i < n; i++)
        fd_hex(fd, "  ", (unsigned long)(uintptr_t)buf[i]);
}

/* ---- crash reporting --------------------------------------------------- */
extern char __executable_start[], etext[];

static LONG CALLBACK on_exception(EXCEPTION_POINTERS *ep)
{
    static volatile LONG in_fault;
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: case EXCEPTION_ILLEGAL_INSTRUCTION: case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_INT_DIVIDE_BY_ZERO: case EXCEPTION_INT_OVERFLOW: case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: case EXCEPTION_IN_PAGE_ERROR: case EXCEPTION_DATATYPE_MISALIGNMENT:
        break;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (InterlockedExchange(&in_fault, 1))
        TerminateProcess(GetCurrentProcess(), 3);
    const CONTEXT *c = ep->ContextRecord;
    unsigned long addr = ep->ExceptionRecord->NumberParameters >= 2
                             ? (unsigned long)ep->ExceptionRecord->ExceptionInformation[1]
                             : (unsigned long)(uintptr_t)ep->ExceptionRecord->ExceptionAddress;
    xsg_logf("[fatal] exception 0x%08lx at address 0x%08lx (frame %u, overlay %d)\n", (unsigned long)code, addr,
             xsg_frame, xsg_active_overlay);
    fflush(NULL);
    int fd = _fileno(xsg_log_fp);
    /* addresses are in xsg.dll: addr2line -f -e xsg.dll ADDR */
    fd_hex(fd, "[fatal] eip ", c->Eip);
    fd_hex(fd, "[fatal] esp ", c->Esp);
    if (code != EXCEPTION_STACK_OVERFLOW && c->Esp) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((void *)(uintptr_t)c->Esp, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT) {
            const unsigned long *w = (const unsigned long *)(uintptr_t)c->Esp;
            const uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
            fd_hex(fd, "[fatal] [esp] (return address if eip is a bad call target) ", w[0]);
            static const char sh[] = "[fatal] stack scan, .text words (addr2line -f -e xsg.dll ADDR):\n";
            _write(fd, sh, sizeof sh - 1);
            for (int i = 0, k = 0; i < 512 && k < 40 && (uintptr_t)(w + i + 1) <= end; i++) {
                unsigned long v = w[i];
                if (v >= (unsigned long)(uintptr_t)__executable_start && v < (unsigned long)(uintptr_t)etext) {
                    fd_hex(fd, "  ", v);
                    k++;
                }
            }
        }
    }
    _commit(fd);
    TerminateProcess(GetCurrentProcess(), 128 + 11);
    return EXCEPTION_CONTINUE_SEARCH;
}

void xsg_w32_install_crash_handler(void)
{
    AddVectoredExceptionHandler(1, on_exception);
}

/* ---- [debug] timeout --------------------------------------------------- */
static void (*g_timeout_fn)(void);
static DWORD g_timeout_s;

static DWORD WINAPI timeout_thread(void *arg)
{
    (void)arg;
    Sleep(g_timeout_s * 1000);
    g_timeout_fn();
    return 0;
}

void xsg_w32_start_timeout(unsigned seconds, void (*fn)(void))
{
    g_timeout_s = seconds;
    g_timeout_fn = fn;
    HANDLE h = CreateThread(NULL, 0, timeout_thread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
}

void xsg_w32_vm_report(void)
{
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a = 0x10000, freeb = 0, big = 0, bigat = 0;
    while (a < 0xFFFF0000u && VirtualQuery((void *)a, &m, sizeof m)) {
        uintptr_t n = m.RegionSize;
        if (m.State == MEM_FREE) {
            freeb += n;
            if (n > big) big = n, bigat = a;
        } else if (n >= (16u << 20))
            xsg_logf("[vm] 0x%08lx +0x%08lx %s\n", (unsigned long)a, (unsigned long)n,
                     m.State == MEM_COMMIT ? "commit" : "reserve");
        if (a + n < a) break;
        a += n;
    }
    xsg_logf("[vm] free %lu MB, largest 0x%lx MB at 0x%08lx, last error %lu\n", (unsigned long)(freeb >> 20),
             (unsigned long)(big >> 20), (unsigned long)bigat, GetLastError());
}

/* ---- hang watchdog ------------------------------------------------------ */
/* When the frame counter stands still for XSG_HANG_S seconds after boot (the setup
   screen is over by then), log every other thread's eip/esp and the xsg.dll code
   addresses on its stack, once, so a hang on a machine we cannot reproduce on
   still says where it is (addr2line -f -e xsg.dll ADDR, on the unstripped DLL of
   the same build).  The game keeps running. */
#include <tlhelp32.h>
#define XSG_HANG_S 20

static void dump_thread(int fd, DWORD tid)
{
    HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
    if (!h)
        return;
    if (SuspendThread(h) == (DWORD)-1) {
        CloseHandle(h);
        return;
    }
    CONTEXT c;
    memset(&c, 0, sizeof c);
    c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (GetThreadContext(h, &c)) {
        char b[128];
        int n = snprintf(b, sizeof b, "[hang] thread %lu eip 0x%08lx esp 0x%08lx\n", (unsigned long)tid,
                         (unsigned long)c.Eip, (unsigned long)c.Esp);
        _write(fd, b, (unsigned)n);
        MEMORY_BASIC_INFORMATION mbi;
        if (c.Esp && VirtualQuery((void *)(uintptr_t)c.Esp, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT) {
            const unsigned long *w = (const unsigned long *)(uintptr_t)c.Esp;
            const uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
            for (int i = 0, k = 0; i < 2048 && k < 24 && (uintptr_t)(w + i + 1) <= end; i++) {
                unsigned long v = w[i];
                if (v >= (unsigned long)(uintptr_t)__executable_start && v < (unsigned long)(uintptr_t)etext) {
                    fd_hex(fd, "    ", v);
                    k++;
                }
            }
        }
    }
    ResumeThread(h);
    CloseHandle(h);
}

static DWORD WINAPI hang_thread(void *arg)
{
    (void)arg;
    unsigned last = xsg_frame, still = 0;
    for (;;) {
        Sleep(1000);
        unsigned f = xsg_frame;
        if (f != last) {
            last = f;
            still = 0;
            continue;
        }
        if (++still < XSG_HANG_S)
            continue;
        xsg_logf("[hang] no frame for %d s (frame %u, overlay %d): thread states follow. "
                 "Please attach this log to a GitHub issue.\n", XSG_HANG_S, f, xsg_active_overlay);
        fflush(NULL);
        int fd = _fileno(xsg_log_fp);
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        THREADENTRY32 te;
        te.dwSize = sizeof te;
        if (snap != INVALID_HANDLE_VALUE) {
            for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te))
                if (te.th32OwnerProcessID == GetCurrentProcessId() && te.th32ThreadID != GetCurrentThreadId())
                    dump_thread(fd, te.th32ThreadID);
            CloseHandle(snap);
        }
        fd_hex(fd, "[hang] xsg.dll .text starts at ", (unsigned long)(uintptr_t)__executable_start);
        _commit(fd);
        return 0;
    }
}

void xsg_w32_start_hang_watchdog(void)
{
    HANDLE h = CreateThread(NULL, 0, hang_thread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
}

/* ---- [debug] sample: where the game thread spends its time ------------- */
/* A thread stops the game thread about 1000 times a second and counts its eip
   (and, outside xsg.dll, the first xsg.dll code address on its stack: the
   caller).  Every 600 vblanks the busiest addresses go to the log as
   module+offset (addr2line -f -e xsg.dll 0x<base+offset> or nm on the same
   build), for slowdowns that only happen on machines we cannot run. */
#define SAMPLE_SLOTS 8192
static struct sample_slot { uintptr_t eip, caller; unsigned n; } g_samples[SAMPLE_SLOTS];
static HANDLE g_sample_thread;

static void module_of(uintptr_t a, char *name, size_t cap, uintptr_t *base)
{
    HMODULE m = NULL;
    *base = 0;
    snprintf(name, cap, "?");
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)a, &m) || !m)
        return;
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(m, path, sizeof path);
    if (!n)
        return;
    const char *s = path + n;
    while (s > path && s[-1] != '\\' && s[-1] != '/')
        s--;
    snprintf(name, cap, "%s", s);
    *base = (uintptr_t)m;
}

static int sample_cmp(const void *a, const void *b)
{
    const struct sample_slot *x = a, *y = b;
    return x->n < y->n ? 1 : x->n > y->n ? -1 : 0;
}

static void sample_dump(unsigned frame, unsigned total)
{
    static struct sample_slot s[SAMPLE_SLOTS];
    int k = 0;
    for (int i = 0; i < SAMPLE_SLOTS; i++)
        if (g_samples[i].n)
            s[k++] = g_samples[i];
    memset(g_samples, 0, sizeof g_samples);
    qsort(s, (size_t)k, sizeof s[0], sample_cmp);
    unsigned in_dll = 0;
    for (int i = 0; i < k; i++)
        if (!s[i].caller)
            in_dll += s[i].n;
    xsg_logf("[sample] vblank %u: %u samples, %u in xsg.dll (base 0x%08lx), top addresses:\n", frame, total, in_dll,
             (unsigned long)(uintptr_t)__executable_start);
    for (int i = 0; i < k && i < 40; i++) {
        char m[64], cm[64];
        uintptr_t b, cb;
        module_of(s[i].eip, m, sizeof m, &b);
        if (s[i].caller) {
            module_of(s[i].caller, cm, sizeof cm, &cb);
            xsg_logf("[sample]   %5u %s+0x%lx <- %s+0x%lx\n", s[i].n, m, (unsigned long)(s[i].eip - b), cm,
                     (unsigned long)(s[i].caller - cb));
        } else
            xsg_logf("[sample]   %5u %s+0x%lx\n", s[i].n, m, (unsigned long)(s[i].eip - b));
    }
}

static DWORD WINAPI sample_thread(void *arg)
{
    (void)arg;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    const uintptr_t lo = (uintptr_t)__executable_start, hi = (uintptr_t)etext;
    unsigned total = 0, next = (xsg_frame / 600 + 1) * 600;
    for (;;) {
        Sleep(1);
        if (SuspendThread(g_sample_thread) == (DWORD)-1)
            return 0;
        CONTEXT c;
        memset(&c, 0, sizeof c);
        c.ContextFlags = CONTEXT_CONTROL;
        uintptr_t eip = 0, caller = 0;
        if (GetThreadContext(g_sample_thread, &c)) {
            eip = c.Eip;
            MEMORY_BASIC_INFORMATION mbi;
            if ((eip < lo || eip >= hi) && c.Esp && VirtualQuery((void *)(uintptr_t)c.Esp, &mbi, sizeof mbi) &&
                mbi.State == MEM_COMMIT) {
                const uintptr_t *w = (const uintptr_t *)(uintptr_t)c.Esp;
                const uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
                caller = 1; /* outside xsg.dll, no xsg.dll caller found */
                for (int i = 0; i < 1024 && (uintptr_t)(w + i + 1) <= end; i++)
                    if (w[i] >= lo && w[i] < hi) {
                        caller = w[i];
                        break;
                    }
            }
        }
        ResumeThread(g_sample_thread);
        if (eip) {
            unsigned h = (unsigned)((eip * 2654435761u) ^ (caller * 40503u)) & (SAMPLE_SLOTS - 1);
            for (int i = 0; i < SAMPLE_SLOTS; i++, h = (h + 1) & (SAMPLE_SLOTS - 1))
                if (!g_samples[h].n || (g_samples[h].eip == eip && g_samples[h].caller == caller)) {
                    g_samples[h].eip = eip;
                    g_samples[h].caller = caller;
                    g_samples[h].n++;
                    break;
                }
            total++;
        }
        if (xsg_frame >= next) {
            sample_dump(xsg_frame, total);
            total = 0;
            next = (xsg_frame / 600 + 1) * 600;
        }
    }
}

/* Called on the game thread. */
void xsg_w32_start_sampler(void)
{
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_sample_thread,
                         THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0))
        return;
    HANDLE h = CreateThread(NULL, 0, sample_thread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
    xsg_logf("[sample] sampling the game thread, a summary every 600 vblanks\n");
}

/* ---- clock_nanosleep with sub-millisecond precision ------------------- */
/* Vblank pacing (kernel.c) and the sound clock (iop_sound.c) sleep to absolute
   CLOCK_MONOTONIC deadlines.  A high-resolution waitable timer gets within a
   fraction of a millisecond; the rest is a yield loop. */
static HANDLE sleep_timer(void)
{
    static __thread HANDLE t;
    if (!t) {
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
        t = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!t)
            t = CreateWaitableTimerExW(NULL, NULL, 0, TIMER_ALL_ACCESS);
    }
    return t;
}

static long long ts_ns(const struct timespec *t) { return (long long)t->tv_sec * 1000000000LL + t->tv_nsec; }

int xsg_w32_clock_nanosleep(clockid_t clk, int flags, const struct timespec *req, struct timespec *rem)
{
    (void)rem;
    struct timespec now;
    clock_gettime(clk, &now);
    long long target = (flags & TIMER_ABSTIME) ? ts_ns(req) : ts_ns(&now) + ts_ns(req);
    long long left = target - ts_ns(&now);
    if (left > 1500000LL) {
        HANDLE t = sleep_timer();
        LARGE_INTEGER due;
        due.QuadPart = -((left - 1000000LL) / 100);   /* relative, 100 ns units */
        if (t && SetWaitableTimer(t, &due, 0, NULL, NULL, FALSE))
            WaitForSingleObject(t, INFINITE);
        else
            Sleep((DWORD)((left - 1000000LL) / 1000000LL));
    }
    for (;;) {
        clock_gettime(clk, &now);
        if (ts_ns(&now) >= target)
            return 0;
        SwitchToThread();
    }
}

int xsg_w32_nanosleep(const struct timespec *req, struct timespec *rem)
{
    return xsg_w32_clock_nanosleep(CLOCK_MONOTONIC, 0, req, rem);
}

/* ---- fnmatch ----------------------------------------------------------- */
int fnmatch(const char *p, const char *s, int flags)
{
    (void)flags;
    for (;; p++, s++) {
        if (*p == '*') {
            while (*p == '*')
                p++;
            if (!*p)
                return 0;
            for (; *s; s++)
                if (!fnmatch(p, s, flags))
                    return 0;
            return 1;
        }
        if (!*p)
            return *s ? 1 : 0;
        if (!*s)
            return 1;
        if (*p == '[') {
            const char *q = p + 1;
            int neg = *q == '!' || *q == '^', ok = 0;
            if (neg)
                q++;
            for (; *q && *q != ']'; q++) {
                if (q[1] == '-' && q[2] && q[2] != ']') {
                    if ((unsigned char)*s >= (unsigned char)q[0] && (unsigned char)*s <= (unsigned char)q[2])
                        ok = 1;
                    q += 2;
                } else if (*q == *s)
                    ok = 1;
            }
            if (!*q || ok == neg)
                return 1;
            p = q;
            continue;
        }
        if (*p != '?' && *p != *s)
            return 1;
    }
}

/* ---- libc functions glibc has and the CRT does not -------------------- */
/* bcopy (the game's PS2 libc names it; on Linux glibc's is linked) */
void bcopy(const void *src, void *dst, size_t n) { memmove(dst, src, n); }

/* ---- posix_spawn / waitpid / kill ------------------------------------- */
static struct { pid_t pid; HANDLE h; } g_procs[32];
static pthread_mutex_t g_procs_mu = PTHREAD_MUTEX_INITIALIZER;

int posix_spawn_file_actions_init(posix_spawn_file_actions_t *fa) { fa->n = 0; return 0; }
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fa) { (void)fa; return 0; }
int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *fa, int fd, int to)
{
    if (fa->n >= 8)
        return ENOMEM;
    fa->fd[fa->n] = fd;
    fa->to[fa->n] = to;
    fa->n++;
    return 0;
}

/* Command line from argv, quoted the way CommandLineToArgvW / the CRT parse it. */
static void append_arg(char *cmd, size_t cap, const char *a)
{
    size_t n = strlen(cmd);
    int quote = !*a || strpbrk(a, " \t\"") != NULL;
    if (n && n + 1 < cap)
        cmd[n++] = ' ';
    if (quote && n + 1 < cap)
        cmd[n++] = '"';
    for (const char *c = a; *c && n + 3 < cap; c++) {
        unsigned bs = 0;
        while (*c == '\\') {
            bs++;
            c++;
        }
        if (!*c) {
            for (unsigned i = 0; i < (quote ? bs * 2 : bs) && n + 2 < cap; i++)
                cmd[n++] = '\\';
            break;
        }
        if (*c == '"') {
            for (unsigned i = 0; i < bs * 2 + 1 && n + 2 < cap; i++)
                cmd[n++] = '\\';
        } else {
            for (unsigned i = 0; i < bs && n + 2 < cap; i++)
                cmd[n++] = '\\';
        }
        cmd[n++] = *c;
    }
    if (quote && n + 1 < cap)
        cmd[n++] = '"';
    cmd[n] = 0;
}

static int spawn(pid_t *pid, const char *file, const posix_spawn_file_actions_t *fa, char *const argv[],
                 char *const envp[])
{
    static char cmd[32768];
    static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mu);
    cmd[0] = 0;
    for (int i = 0; argv[i]; i++)
        append_arg(cmd, sizeof cmd, argv[i]);
    STARTUPINFOA si;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE dup[3] = {0, 0, 0};
    for (int i = 0; fa && i < fa->n; i++) {
        if (fa->to[i] < 0 || fa->to[i] > 2)
            continue;
        HANDLE src = (HANDLE)_get_osfhandle(fa->fd[i]);
        if (src == INVALID_HANDLE_VALUE ||
            !DuplicateHandle(GetCurrentProcess(), src, GetCurrentProcess(), &dup[fa->to[i]], 0, TRUE,
                             DUPLICATE_SAME_ACCESS))
            continue;
    }
    if (dup[0]) si.hStdInput = dup[0];
    if (dup[1]) si.hStdOutput = dup[1];
    if (dup[2]) si.hStdError = dup[2];
    /* environment block from envp */
    char *env = NULL;
    if (envp) {
        size_t len = 1;
        for (int i = 0; envp[i]; i++)
            len += strlen(envp[i]) + 1;
        env = malloc(len);
        char *o = env;
        for (int i = 0; envp[i]; i++) {
            size_t l = strlen(envp[i]) + 1;
            memcpy(o, envp[i], l);
            o += l;
        }
        *o = 0;
    }
    PROCESS_INFORMATION pi;
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, env, NULL, &si, &pi);
    DWORD err = GetLastError();
    (void)file;
    free(env);
    for (int i = 0; i < 3; i++)
        if (dup[i])
            CloseHandle(dup[i]);
    pthread_mutex_unlock(&mu);
    if (!ok)
        return err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ? ENOENT : EACCES;
    CloseHandle(pi.hThread);
    pthread_mutex_lock(&g_procs_mu);
    for (unsigned i = 0; i < sizeof g_procs / sizeof g_procs[0]; i++)
        if (!g_procs[i].h) {
            g_procs[i].pid = (pid_t)pi.dwProcessId;
            g_procs[i].h = pi.hProcess;
            break;
        }
    pthread_mutex_unlock(&g_procs_mu);
    *pid = (pid_t)pi.dwProcessId;
    return 0;
}

int posix_spawnp(pid_t *pid, const char *file, const posix_spawn_file_actions_t *fa,
                 const posix_spawnattr_t *attr, char *const argv[], char *const envp[])
{
    (void)attr;
    return spawn(pid, file, fa, argv, envp);
}

int posix_spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *fa,
                const posix_spawnattr_t *attr, char *const argv[], char *const envp[])
{
    (void)attr;
    return spawn(pid, path, fa, argv, envp);
}

static HANDLE proc_handle(pid_t pid, int forget)
{
    HANDLE h = NULL;
    pthread_mutex_lock(&g_procs_mu);
    for (unsigned i = 0; i < sizeof g_procs / sizeof g_procs[0]; i++)
        if (g_procs[i].h && g_procs[i].pid == pid) {
            h = g_procs[i].h;
            if (forget)
                g_procs[i].h = NULL;
            break;
        }
    pthread_mutex_unlock(&g_procs_mu);
    return h;
}

pid_t waitpid(pid_t pid, int *status, int options)
{
    HANDLE h = proc_handle(pid, 0);
    if (!h) {
        errno = ECHILD;
        return -1;
    }
    if (WaitForSingleObject(h, (options & WNOHANG) ? 0 : INFINITE) != WAIT_OBJECT_0)
        return 0;
    DWORD code = 0;
    GetExitCodeProcess(h, &code);
    if (status)
        *status = (int)((code & 0xff) << 8);
    proc_handle(pid, 1);
    CloseHandle(h);
    return pid;
}

int kill(pid_t pid, int sig)
{
    HANDLE h = proc_handle(pid, 0);
    if (!h) {
        errno = ESRCH;
        return -1;
    }
    if (sig)
        TerminateProcess(h, 128 + (unsigned)sig);
    return 0;
}

#endif /* _WIN32 */

/* Hybrid CPUs: the performance cores, no power throttling (win32/xsg_pcores.h). */
#include "xsg_pcores.h"
static void pcores_log(const char *msg) { xsg_logf("[win32] %s\n", msg); }
void xsg_w32_pcores(void) { xsg_w32_prefer_pcores(pcores_log); }

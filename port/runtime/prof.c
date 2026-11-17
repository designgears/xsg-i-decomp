/* Stage 10: poor-man's sampling profiler (perf is not available here).
 *
 *   XSG_PROF=<file>      sample the game thread at XSG_PROF_HZ (default 1000) on
 *                        the wall clock; each sample is a short backtrace()
 *   XSG_PROF_START=<vb>  only sample from that vblank on (default 0)
 *
 * Samples are kept in memory and written at exit / XSG_TIMEOUT as lines of hex
 * addresses (innermost first); port/tools/prof_report.py symbolises them. */
#ifdef _WIN32
/* Windows (port/docs/WINDOWS.md): no SIGPROF timer; the profiler is Linux-only. */
#include "xsg_settings.h"
#include "xsg_rt.h"
void xsg_prof_dump(void) {}
void xsg_prof_init(void)
{
    if (xsg_cfg("debug.prof"))
        xsg_logf("[prof] the sampling profiler is not available on Windows\n");
}
#else
#define _GNU_SOURCE
#include <execinfo.h>
#include "xsg_settings.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include "xsg_rt.h"

#define PROF_DEPTH 12
#define PROF_MAX (1u << 20)

extern char __executable_start, etext;
static void **prof_buf;
static volatile unsigned prof_n;
static unsigned prof_start;
static const char *prof_path;

static void on_prof(int sig, siginfo_t *si, void *ucv)
{
    (void)sig;
    (void)si;
    if (xsg_frame < prof_start || prof_n >= PROF_MAX)
        return;
    ucontext_t *uc = ucv;
    void **s = prof_buf + (size_t)prof_n * PROF_DEPTH;
    void *bt[PROF_DEPTH + 3];
    unsigned long pc = (unsigned long)uc->uc_mcontext.gregs[REG_EIP];
    memset(s, 0, PROF_DEPTH * sizeof *s);
    s[0] = (void *)pc;
    prof_n++;
    /* Unwind only from our own code: libgcc's unwinder faults in libc's
     * hand-written routines (memcpy) whose CFI does not cover every pc. */
    if (pc < (unsigned long)&__executable_start || pc >= (unsigned long)&etext)
        return;
    int n = backtrace(bt, PROF_DEPTH + 3);
    /* bt[0] = this handler, bt[1] = the sigreturn trampoline, bt[2] = the
     * interrupted pc again (unwound through the signal frame) */
    for (int i = 3, j = 1; i < n && j < PROF_DEPTH; i++, j++)
        s[j] = bt[i];
}

void xsg_prof_dump(void)
{
    if (!prof_buf || !prof_path)
        return;
    unsigned n = prof_n;
    prof_n = PROF_MAX; /* stop sampling */
    FILE *f = fopen(prof_path, "w");
    if (!f)
        return;
    for (unsigned i = 0; i < n; i++) {
        void **s = prof_buf + (size_t)i * PROF_DEPTH;
        for (int j = 0; j < PROF_DEPTH && s[j]; j++)
            fprintf(f, j ? " %lx" : "%lx", (unsigned long)s[j]);
        fputc('\n', f);
    }
    fclose(f);
    xsg_logf("[prof] %u samples -> %s\n", n, prof_path);
    prof_path = NULL;
}

void xsg_prof_init(void)
{
    prof_path = xsg_cfg("debug.prof");
    if (!prof_path || !*prof_path) {
        prof_path = NULL;
        return;
    }
    prof_buf = calloc((size_t)PROF_MAX * PROF_DEPTH, sizeof(void *));
    if (xsg_cfg("debug.prof_start"))
        prof_start = (unsigned)atoi(xsg_cfg("debug.prof_start"));
    int hz = xsg_cfg("debug.prof_hz") ? atoi(xsg_cfg("debug.prof_hz")) : 1000;
    void *warm[2];
    backtrace(warm, 2); /* load libgcc_s before the first signal */
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_prof;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGPROF, &sa, NULL);
    struct sigevent sev;
    memset(&sev, 0, sizeof sev);
    sev.sigev_notify = SIGEV_THREAD_ID;
    sev.sigev_signo = SIGPROF;
    sev._sigev_un._tid = (pid_t)syscall(SYS_gettid);
    timer_t t;
    if (timer_create(CLOCK_MONOTONIC, &sev, &t) == 0) {
        struct itimerspec its = {{0, 1000000000L / hz}, {0, 1000000000L / hz}};
        timer_settime(t, 0, &its, NULL);
    }
    atexit(xsg_prof_dump);
    xsg_logf("[prof] sampling the game thread at %d Hz -> %s\n", hz, prof_path);
}
#endif /* !_WIN32 */

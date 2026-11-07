/*
 * EE kernel stand-in: threads, semaphores, interrupts, caches.
 *
 * Threads are ucontext coroutines on one host thread, scheduled the way the
 * EE kernel does it but without a timer: the highest-priority READY thread
 * runs (lower number = higher priority) and keeps running until it blocks
 * (WaitSema, SleepThread, sceGsSyncV...) or makes a higher-priority thread
 * ready.  When nothing is ready the scheduler "advances a frame" (vblank):
 * it runs the registered vsync callbacks/INTC handlers, which normally wake
 * someone.  If even that wakes nobody, it is a deadlock and is logged.
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include "xsg_rt.h"

#define MAX_THREADS 64
#define MAX_SEMAS 128
#define HOST_STACK (1u << 20)

enum { TS_FREE, TS_DORMANT, TS_READY, TS_WAIT_SEMA, TS_SLEEP };
static const char *ts_name[] = {"free", "dormant", "ready", "wait-sema", "sleep"};

struct ee_ThreadParam { /* sdk eekernel.h layout */
    int status;
    void (*func)(void *);
    void *stack;
    int stackSize;
    void *gpReg;
    int initPriority;
    int currentPriority;
    unsigned attr, option;
    int waitType, waitId, wakeupCount;
};
struct ee_SemaParam {
    int count, max_count, init_count, wait_threads;
    unsigned attr, option;
};

struct thread {
    int state;
    int prio;
    int wakeups;
    int wait_sema;
    unsigned seq; /* FIFO order among equal priorities */
    void (*func)(void *);
    void *arg;
    ucontext_t ctx;
    void *host_stack;
    void *bt[12]; /* where it last blocked (for the timeout dump) */
    int nbt;
};
static struct thread th[MAX_THREADS];
static int cur = 1;
static unsigned seq_counter;

struct sema {
    int used, count, max;
};
static struct sema sem[MAX_SEMAS];

volatile unsigned xsg_frame;
static int (*vsync_cb)(int);
static struct { int cause; int (*fn)(int); int used; } intc[32];
static int frame_ns = 0; /* XSG_FRAME_US: real-time pacing per frame (0 = none) */

void xsg_kernel_init(void)
{
    th[1].state = TS_READY;
    th[1].prio = 1; /* crt0 runs main at the highest user priority */
    th[1].seq = seq_counter++;
    const char *fp = getenv("XSG_FRAME_US");
    frame_ns = fp ? atoi(fp) * 1000 : 0;
}

static void thread_trampoline(int id)
{
    th[id].func(th[id].arg);
    xsg_logf("[kernel] thread %d returned from its entry\n", id);
    th[id].state = TS_DORMANT;
    extern void xsg_schedule(void);
    xsg_schedule();
}

static int pick(void)
{
    int best = -1;
    for (int i = 1; i < MAX_THREADS; i++) {
        if (th[i].state != TS_READY)
            continue;
        if (best < 0 || th[i].prio < th[best].prio ||
            (th[i].prio == th[best].prio && th[i].seq < th[best].seq))
            best = i;
    }
    return best;
}

static void switch_to(int next)
{
    if (next == cur)
        return;
    int prev = cur;
    cur = next;
    swapcontext(&th[prev].ctx, &th[next].ctx);
}

static void dump_threads(void)
{
    for (int i = 1; i < MAX_THREADS; i++)
        if (th[i].state != TS_FREE) {
            xsg_logf("  thread %2d prio %3d %-9s sema %d wakeups %d func %p\n", i, th[i].prio,
                     ts_name[th[i].state], th[i].wait_sema, th[i].wakeups, (void *)th[i].func);
            if (th[i].state != TS_READY && th[i].nbt) {
                xsg_logf("    blocked at:");
                for (int k = 1; k < th[i].nbt; k++)
                    xsg_logf(" %p", th[i].bt[k]);
                xsg_logf("\n");
            }
        }
}

/* Record where the current thread blocks: a frame-pointer walk (the game is
 * built at -O0, so every frame has one), bounded to the thread's own host
 * stack so a smashed frame cannot fault.  glibc backtrace() can crash on
 * such a frame. */
static void note_block(void)
{
    char *lo = th[cur].host_stack, *hi = lo + HOST_STACK;
    void **fp = __builtin_frame_address(0);
    int n = 0;
    if (!lo) { /* main thread: the process stack */
        lo = (char *)fp;
        hi = lo + (8u << 20);
    }
    while (n < 12 && (char *)fp >= lo && (char *)fp + 8 <= hi) {
        th[cur].bt[n++] = fp[1];
        void **next = fp[0];
        if (next <= fp)
            break;
        fp = next;
    }
    th[cur].nbt = n;
}

void xsg_kernel_dump(void)
{
    xsg_logf("[kernel] running thread %d, frame %u\n", cur, xsg_frame);
    dump_threads();
}

/* Current thread gave up the CPU (blocked or yielded): run the best one. */
void xsg_schedule(void)
{
    int idle_frames = 0;
    for (;;) {
        int n = pick();
        if (n > 0) {
            switch_to(n);
            return;
        }
        /* nobody ready: next vblank */
        xsg_vblank();
        if (++idle_frames > 600) {
            xsg_logf("[kernel] DEADLOCK: no thread ready after %d vblanks (frame %u)\n",
                     idle_frames, xsg_frame);
            dump_threads();
            exit(3);
        }
    }
}

/* A thread became ready; preempt if it outranks the current one. */
static void maybe_preempt(void)
{
    int n = pick();
    if (n > 0 && th[cur].state == TS_READY && th[n].prio < th[cur].prio)
        switch_to(n);
    else if (th[cur].state != TS_READY)
        xsg_schedule();
}

static void make_ready(int id)
{
    th[id].state = TS_READY;
    th[id].seq = seq_counter++;
}

/* ---- vblank --------------------------------------------------------- */
extern int xsg_vsync_waiters_wake(void);
static int vsync_waiters[MAX_THREADS];

void xsg_vblank(void)
{
    xsg_frame++;
    if (frame_ns) {
        struct timespec ts = {0, frame_ns};
        nanosleep(&ts, NULL);
    }
    if ((xsg_frame % 600) == 0)
        xsg_logf("[kernel] frame %u (thread %d)\n", xsg_frame, cur);
    for (int i = 1; i < MAX_THREADS; i++)
        if (vsync_waiters[i]) {
            vsync_waiters[i] = 0;
            if (th[i].state == TS_SLEEP)
                make_ready(i);
        }
    for (int i = 0; i < 32; i++)
        if (intc[i].used && intc[i].cause == 2 /* INTC_VBLANK_S */)
            intc[i].fn(2);
    if (vsync_cb)
        vsync_cb(0);
}

/* sceGsSyncV: block the caller until the next vblank. */
int sceGsSyncV(int mode)
{
    STUB_LOG("mode=%d frame=%u", mode, xsg_frame);
    vsync_waiters[cur] = 1;
    th[cur].state = TS_SLEEP;
    note_block();
    xsg_schedule();
    return xsg_frame & 1; /* field */
}

void *sceGsSyncVCallback(int (*fn)(int))
{
    STUB_LOG("fn=%p", (void *)fn);
    void *old = (void *)vsync_cb;
    vsync_cb = fn;
    return old;
}

/* ---- threads -------------------------------------------------------- */
int CreateThread(struct ee_ThreadParam *p)
{
    for (int i = 2; i < MAX_THREADS; i++) {
        if (th[i].state != TS_FREE)
            continue;
        memset(&th[i], 0, sizeof th[i]);
        th[i].state = TS_DORMANT;
        th[i].prio = p->initPriority;
        th[i].func = p->func;
        th[i].host_stack = malloc(HOST_STACK);
        xsg_logf("[kernel] CreateThread -> %d func=%p prio=%d ps2stack=%p+0x%x\n", i,
                 (void *)p->func, p->initPriority, p->stack, p->stackSize);
        return i;
    }
    xsg_logf("[kernel] CreateThread: out of threads\n");
    return -1;
}

int StartThread(int id, void *arg)
{
    xsg_logf("[kernel] StartThread %d arg=%p\n", id, arg);
    struct thread *t = &th[id];
    t->arg = arg;
    getcontext(&t->ctx);
    t->ctx.uc_stack.ss_sp = t->host_stack;
    t->ctx.uc_stack.ss_size = HOST_STACK;
    t->ctx.uc_link = NULL;
    makecontext(&t->ctx, (void (*)(void))thread_trampoline, 1, id);
    make_ready(id);
    maybe_preempt();
    return id;
}

int DeleteThread(int id) { STUB_LOG("%d", id); th[id].state = TS_FREE; return id; }
int TerminateThread(int id) { STUB_LOG("%d", id); th[id].state = TS_DORMANT; return id; }
int GetThreadId(void) { return cur; }

int SleepThread(void)
{
    if (th[cur].wakeups > 0) {
        th[cur].wakeups--;
        return cur;
    }
    th[cur].state = TS_SLEEP;
    note_block();
    xsg_schedule();
    return cur;
}

static int wakeup(int id)
{
    if (id <= 0 || id >= MAX_THREADS || th[id].state == TS_FREE)
        return -1;
    if (th[id].state == TS_SLEEP && !vsync_waiters[id])
        make_ready(id);
    else
        th[id].wakeups++;
    return id;
}
int WakeupThread(int id) { int r = wakeup(id); maybe_preempt(); return r; }
int iWakeupThread(int id) { return wakeup(id); }
int CancelWakeupThread(int id)
{
    if (id == 0)
        id = cur;
    int n = th[id].wakeups;
    th[id].wakeups = 0;
    return n;
}
int iCancelWakeupThread(int id) { return CancelWakeupThread(id); }
int ChangeThreadPriority(int id, int prio)
{
    if (id == 0)
        id = cur;
    int old = th[id].prio;
    th[id].prio = prio;
    maybe_preempt();
    return old;
}
int RotateThreadReadyQueue(int prio)
{
    if (th[cur].prio == prio) {
        th[cur].seq = seq_counter++;
        xsg_schedule();
    }
    return 0;
}
int ReleaseWaitThread(int id) { STUB_LOG("%d", id); return wakeup(id); }
int SuspendThread(int id) { STUB_LOG("%d", id); return id; }
int ResumeThread(int id) { STUB_LOG("%d", id); return id; }

/* ---- semaphores ------------------------------------------------------ */
int CreateSema(struct ee_SemaParam *p)
{
    for (int i = 1; i < MAX_SEMAS; i++) {
        if (sem[i].used)
            continue;
        sem[i].used = 1;
        sem[i].count = p->init_count;
        sem[i].max = p->max_count;
        STUB_LOG("-> %d init=%d max=%d", i, p->init_count, p->max_count);
        return i;
    }
    return -1;
}
int DeleteSema(int id) { STUB_LOG("%d", id); sem[id].used = 0; return id; }

static int signal_sema(int id)
{
    if (id <= 0 || id >= MAX_SEMAS || !sem[id].used)
        return -1;
    int best = -1;
    for (int i = 1; i < MAX_THREADS; i++)
        if (th[i].state == TS_WAIT_SEMA && th[i].wait_sema == id &&
            (best < 0 || th[i].seq < th[best].seq))
            best = i;
    if (best > 0) {
        th[best].wait_sema = 0;
        make_ready(best);
    } else {
        sem[id].count++;
    }
    return id;
}
int SignalSema(int id) { int r = signal_sema(id); maybe_preempt(); return r; }
int iSignalSema(int id) { return signal_sema(id); }

int WaitSema(int id)
{
    if (id <= 0 || id >= MAX_SEMAS || !sem[id].used)
        return -1;
    if (sem[id].count > 0) {
        sem[id].count--;
        return id;
    }
    th[cur].state = TS_WAIT_SEMA;
    th[cur].wait_sema = id;
    th[cur].seq = seq_counter++;
    note_block();
    xsg_schedule();
    return id;
}
int PollSema(int id)
{
    if (id > 0 && id < MAX_SEMAS && sem[id].count > 0) {
        sem[id].count--;
        return id;
    }
    return -1;
}
int iPollSema(int id) { return PollSema(id); }

/* ---- interrupts / caches -------------------------------------------- */
int AddIntcHandler(int cause, int (*fn)(int), int next)
{
    (void)next;
    for (int i = 0; i < 32; i++)
        if (!intc[i].used) {
            intc[i].used = 1;
            intc[i].cause = cause;
            intc[i].fn = fn;
            STUB_LOG("cause=%d fn=%p -> %d", cause, (void *)fn, i);
            return i + 1;
        }
    return -1;
}
int AddIntcHandler2(int cause, int (*fn)(int), int next, void *arg)
{
    (void)arg;
    return AddIntcHandler(cause, fn, next);
}
int RemoveIntcHandler(int cause, int id) { (void)cause; if (id > 0) intc[id - 1].used = 0; return 0; }
int AddDmacHandler(int ch, int (*fn)(int), int next) { STUB_LOG("ch=%d fn=%p", ch, (void *)fn); (void)next; return 1; }
int AddDmacHandler2(int ch, int (*fn)(int), int next, void *arg) { (void)arg; return AddDmacHandler(ch, fn, next); }
int RemoveDmacHandler(int ch, int id) { (void)ch; (void)id; return 0; }
int EnableIntc(int c) { STUB_LOG("%d", c); return 1; }
int DisableIntc(int c) { STUB_LOG("%d", c); return 1; }
int EnableDmac(int c) { STUB_LOG("%d", c); return 1; }
int DisableDmac(int c) { STUB_LOG("%d", c); return 1; }
int iEnableIntc(int c) { return EnableIntc(c); }
int iDisableIntc(int c) { return DisableIntc(c); }
int iEnableDmac(int c) { return EnableDmac(c); }
int iDisableDmac(int c) { return DisableDmac(c); }
int DIntr(void) { return 1; }
int EIntr(void) { return 1; }
void FlushCache(int mode) { (void)mode; }
void iFlushCache(int mode) { (void)mode; }
void SyncDCache(void *a, void *b) { (void)a; (void)b; }
void iSyncDCache(void *a, void *b) { (void)a; (void)b; }
void InvalidDCache(void *a, void *b) { (void)a; (void)b; }
void iInvalidDCache(void *a, void *b) { (void)a; (void)b; }

/*
 * The IOP sound driver, run natively (Stage 10, port/docs/STAGE10_SOUND.md).
 *
 * The game's IOP sound modules SSD.IRX ("PS2_SuzukiSoundDriver") and
 * RSSD.IRX (its SIF RPC front end) are decompiled in src/audio as plain C.
 * The port compiles that C for the host as one relocatable object whose
 * symbols all get the prefix `ssdiop_` (CMake: iop_sound_driver.o, data
 * tables from gen_iop_sound_data.py), so the driver's names cannot clash
 * with the EE's, and its imports arrive here as ssdiop_<import>:
 *
 *   IOP kernel   threads, semaphores, sysmem, interrupts and the hard timer.
 *                Each IOP thread is a host pthread, but only one runs at a
 *                time: they all hold the "IOP lock" while running and drop it
 *                only while blocked (SleepThread, WaitSema, sceSifRpcLoop),
 *                like threads on the single IOP CPU.  Interrupt handlers run
 *                with the lock held, from the audio thread.
 *   libsd        sceSd* on the software SPU2 (spu2.c).
 *   SIF          the EE -> IOP server 0x900 (RssdSifRpcServer, called on the
 *                EE thread from iop.c's sceSifCallRpc), and IOP -> EE calls
 *                to server 0x901 (queued for the EE by iop.c).
 *
 * The audio thread is the IOP's clock: every 2 ms of real time it fires the
 * SPU2 DMA-completion interrupts, then renders the SPU2 in steps up to each
 * hard-timer expiry (the driver's 2 ms SsdMainInterruptProcess tick:
 * sequencer, envelopes, fades), and writes the samples to ALSA (never
 * blocking: a full device drops, an underrun recovers) and to XSG_WAV.
 *
 * XSG_SOUND=0 falls back to the Stage 4 model (no driver, iop.c acks).
 */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "spu2.h"
#include "xsg_rt.h"

#define RATE 48000
#define SYSCLK 36864000u /* IOP sysclock, Hz */
#define CLK_PER_SAMPLE (SYSCLK / RATE)

void *xsg_alsa_open(unsigned rate, unsigned latency_us);
long xsg_alsa_write(void *pcm, const short *s, unsigned frames);
long xsg_alsa_avail(void *pcm);
void xsg_iop_call_ee(unsigned rpc, unsigned command, const void *data, int size);

/* the driver (prefixed) */
int ssdiop_start(void);

static pthread_mutex_t L = PTHREAD_MUTEX_INITIALIZER;
static FILE *wav;
static pthread_mutex_t wav_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned long long wav_frames;
static void *pcm;
static volatile unsigned long long rendered;
static unsigned long long alsa_dropped, alsa_underruns;
static unsigned stream_blocks, stream_irqs; /* 0xb5 data commands, SPU2 IRQs */
static int game_clock = 1;
static unsigned long long clock_waits; /* ticks the game clock held the IOP back */
static int log_on;
static int started;

#define SLOG(...)                                                                                 \
    do {                                                                                          \
        if (log_on)                                                                               \
            xsg_logf(__VA_ARGS__);                                                                \
    } while (0)

/* ---- IOP kernel: threads -------------------------------------------- */
#define MAXTH 16
typedef struct {
    unsigned attributes, option;
    int (*entry)(void *);
    int stackSize, priority;
    unsigned unmodeled_014;
} IopThreadParam;
static struct {
    int used, wakeups;
    int (*entry)(void *);
    pthread_t pt;
    pthread_cond_t cv;
} TH[MAXTH];
static __thread int cur_tid;

static void *th_main(void *p)
{
    int id = (int)(intptr_t)p;
    pthread_mutex_lock(&L);
    cur_tid = id;
    TH[id].entry(NULL);
    pthread_mutex_unlock(&L);
    return NULL;
}

int ssdiop_CreateThread(IopThreadParam *p)
{
    for (int i = 1; i < MAXTH; i++)
        if (!TH[i].used) {
            TH[i].used = 1;
            TH[i].entry = p->entry;
            TH[i].wakeups = 0;
            pthread_cond_init(&TH[i].cv, NULL);
            return i;
        }
    return -1;
}

int ssdiop_StartThread(int id, void *arg)
{
    (void)arg;
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&a, 256 * 1024);
    int r = pthread_create(&TH[id].pt, &a, th_main, (void *)(intptr_t)id);
    pthread_attr_destroy(&a);
    return r ? -1 : 0;
}

int ssdiop_GetThreadId(void) { return cur_tid; }

int ssdiop_SleepThread(void)
{
    int id = cur_tid;
    if (id <= 0 || id >= MAXTH)
        return -1; /* not an IOP thread */
    while (TH[id].wakeups == 0)
        pthread_cond_wait(&TH[id].cv, &L);
    TH[id].wakeups--;
    return 0;
}

int ssdiop_WakeupThread(int id)
{
    if (id <= 0 || id >= MAXTH || !TH[id].used)
        return -1;
    TH[id].wakeups++;
    pthread_cond_signal(&TH[id].cv);
    return 0;
}
int ssdiop_iWakeupThread(int id) { return ssdiop_WakeupThread(id); }

/* ---- semaphores ----------------------------------------------------- */
typedef struct {
    unsigned attributes, option;
    int initial, max;
} IopSemaParam;
#define MAXSEM 16
static struct {
    int used, count, max;
    pthread_cond_t cv;
} SEM[MAXSEM];

int ssdiop_CreateSema(IopSemaParam *p)
{
    for (int i = 1; i < MAXSEM; i++)
        if (!SEM[i].used) {
            SEM[i].used = 1;
            SEM[i].count = p->initial;
            SEM[i].max = p->max;
            pthread_cond_init(&SEM[i].cv, NULL);
            return i;
        }
    return -1;
}

int ssdiop_WaitSema(int id)
{
    if (id <= 0 || id >= MAXSEM)
        return -1;
    while (SEM[id].count <= 0)
        pthread_cond_wait(&SEM[id].cv, &L);
    SEM[id].count--;
    return 0;
}

int ssdiop_SignalSema(int id)
{
    if (id <= 0 || id >= MAXSEM)
        return -1;
    if (SEM[id].count < SEM[id].max || SEM[id].max <= 0)
        SEM[id].count++;
    pthread_cond_broadcast(&SEM[id].cv);
    return 0;
}
int ssdiop_iSignalSema(int id) { return ssdiop_SignalSema(id); }

/* ---- interrupts, sysmem, misc ---------------------------------------- */
int ssdiop_CpuDisableIntr(void) { return 0; }
int ssdiop_CpuEnableIntr(void) { return 0; }
int ssdiop_DisableIntr(int irq, int *old) { (void)irq; if (old) *old = 0; return 0; }
int ssdiop_EnableIntr(int irq) { (void)irq; return 0; }

void *ssdiop_AllocSysMemory(int mode, int size, void *addr)
{
    (void)mode; (void)addr;
    void *p = NULL;
    if (posix_memalign(&p, 64, size > 0 ? (size_t)size : 1))
        return NULL;
    memset(p, 0, size > 0 ? (size_t)size : 1);
    return p;
}
int ssdiop_FreeSysMemory(void *p) { free(p); return 0; }
int ssdiop_QueryMaxFreeMemSize(void) { return 0x100000; }
int ssdiop_RegisterLibraryEntries(void *lib) { (void)lib; return 0; }
unsigned ssdiop_ssd_entry[8]; /* SSD's export table (SsdStart only) */

int ssdiop_printf(const char *fmt, ...)
{
    if (!log_on)
        return 0;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    xsg_logf("[ssd] %s", buf);
    return 0;
}
int ssdiop_Kprintf(const char *fmt, ...)
{
    if (!log_on)
        return 0;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    xsg_logf("[ssd] %s", buf);
    return 0;
}
int ssdiop_sprintf(char *dst, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(dst, fmt, ap);
    va_end(ap);
    return r;
}

/* ---- hard timer ----------------------------------------------------- */
typedef struct { unsigned low, high; } IopSysClock;
static struct {
    int running;
    unsigned interval; /* sysclocks */
    unsigned (*handler)(void *);
    void *arg;
    long long remaining; /* sysclocks to the next expiry */
    unsigned long long count;
} T;

void ssdiop_USec2SysClock(unsigned us, IopSysClock *c)
{
    unsigned long long v = (unsigned long long)us * SYSCLK / 1000000u;
    c->low = (unsigned)v;
    c->high = (unsigned)(v >> 32);
}
int ssdiop_AllocHardTimer(int src, int size, int prescale) { (void)src; (void)size; (void)prescale; return 1; }
int ssdiop_SetTimerHandler(int t, unsigned interval, unsigned (*fn)(void *), void *arg)
{
    (void)t;
    T.interval = interval;
    T.handler = fn;
    T.arg = arg;
    T.remaining = interval;
    return 0;
}
int ssdiop_SetupHardTimer(int t, int src, int mode, int prescale) { (void)t; (void)src; (void)mode; (void)prescale; return 0; }
int ssdiop_StartHardTimer(int t) { (void)t; T.running = 1; return 0; }
int ssdiop_StopHardTimer(int t) { (void)t; T.running = 0; return 0; }
unsigned ssdiop_GetTimerCounter(int t) { (void)t; return (unsigned)(T.interval - T.remaining); }

/* ---- libsd ---------------------------------------------------------- */
typedef struct {
    int core, mode;
    short depth_l, depth_r;
    int delay, feedback;
} SdEffectAttr;
static SdEffectAttr effect[2];
static int (*trans_handler[2])(int, void *);
static void *trans_arg[2];
static int trans_pending[2];
static int (*spu2_handler)(int, void *);
static void *spu2_handler_arg;

static void on_irq(int core)
{
    stream_irqs++;
    if (spu2_handler)
        spu2_handler(1 << core, spu2_handler_arg);
}

/* XSG_SPU_TRACE=dir: each key on (the first 256) dumps the sample it plays,
 * from its start to the first loop-end block (at most 256 KiB), as
 * dir/kNNN_cC_vVV_pPPPP_aADDR.adpcm, and logs the time it happened. */
static const char *trace_dir;
static unsigned trace_n;
static void on_keyon(int core, int voice, uint32_t ssa, uint16_t pitch)
{
    if (!trace_dir || trace_n >= 256)
        return;
    static uint8_t buf[256 * 1024];
    uint32_t n = 0;
    while (n < sizeof buf) {
        spu2_peek(ssa + n, buf + n, 16);
        n += 16;
        if (buf[n - 15] & 1)
            break;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/k%03u_c%d_v%02d_p%04x_a%06x.adpcm", trace_dir, trace_n, core, voice,
             pitch, ssa);
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(buf, 1, n, f);
        fclose(f);
    }
    xsg_logf("[sound] %7.2f keyon %u core %d voice %d ssa 0x%06x pitch 0x%04x (%u bytes)\n",
             rendered / (double)RATE, trace_n, core, voice, ssa, pitch, n);
    trace_n++;
}

static void on_adma(int core)
{
    if (trans_handler[core])
        trans_handler[core](core, trans_arg[core]);
}

int ssdiop_sceSdInit(int flag)
{
    (void)flag;
    spu2_reset();
    spu2_set_irq_callback(on_irq);
    spu2_set_adma_callback(on_adma);
    spu2_set_keyon_callback(on_keyon);
    memset(effect, 0, sizeof effect);
    /* libsd's sceSdInit mixer defaults: core 0 voices and AutoDMA input into
     * its output, core 1 additionally takes core 0's output (dry) */
    spu2_set_param(0x0880, 0x0ff0);
    spu2_set_param(0x0881, 0x0ffc);
    return 0;
}
void ssdiop_sceSdSetParam(unsigned short e, unsigned short v) { spu2_set_param(e, v); }
unsigned short ssdiop_sceSdGetParam(unsigned short e) { return spu2_get_param(e); }
void ssdiop_sceSdSetSwitch(unsigned short e, unsigned v) { spu2_set_switch(e, v); }
unsigned ssdiop_sceSdGetSwitch(unsigned short e) { return spu2_get_switch(e); }
void ssdiop_sceSdSetAddr(unsigned short e, unsigned v) { spu2_set_addr(e, v); }
unsigned ssdiop_sceSdGetAddr(unsigned short e) { return spu2_get_addr(e); }
void ssdiop_sceSdSetCoreAttr(unsigned short e, unsigned short v) { spu2_set_core_attr(e, v); }
unsigned short ssdiop_sceSdGetCoreAttr(unsigned short e) { return (unsigned short)spu2_get_core_attr(e); }

int ssdiop_sceSdSetEffectAttr(int core, SdEffectAttr *a)
{
    core &= 1;
    int mode = a->mode & 0xff;
    effect[core] = *a;
    effect[core].mode = mode;
    spu2_set_effect(core, mode, a->depth_l, a->depth_r);
    if (a->mode & 0x100)
        spu2_clear_effect_area(core, mode);
    SLOG("[ssd] effect core %d mode %d depth %d/%d\n", core, mode, a->depth_l, a->depth_r);
    return 0;
}
void ssdiop_sceSdGetEffectAttr(int core, SdEffectAttr *a) { *a = effect[core & 1]; }
int ssdiop_sceSdClearEffectWorkArea(int core, int channel, int mode)
{
    (void)channel;
    spu2_clear_effect_area(core & 1, mode & 0xff);
    return 0;
}

void *ssdiop_sceSdSetTransIntrHandler(int ch, int (*fn)(int, void *), void *arg)
{
    void *old = (void *)trans_handler[ch & 1];
    trans_handler[ch & 1] = fn;
    trans_arg[ch & 1] = arg;
    return old;
}
void *ssdiop_sceSdSetSpu2IntrHandler(int (*fn)(int, void *), void *arg)
{
    void *old = (void *)spu2_handler;
    spu2_handler = fn;
    spu2_handler_arg = arg;
    return old;
}

/* DMA completes at once; its interrupt fires on the next audio tick. */
int ssdiop_sceSdVoiceTrans(short ch, unsigned short mode, void *iop, unsigned spu, unsigned size)
{
    ch &= 1;
    if ((mode & 3) == 2) /* stop */
        return 0;
    if (mode & 1)
        spu2_dma_read(spu, iop, size);
    else
        spu2_dma_write(spu, iop, size);
    trans_pending[ch] = 1;
    return (int)size;
}
unsigned ssdiop_sceSdVoiceTransStatus(short ch, short flag)
{
    (void)flag;
    (void)ch;
    return 1; /* the data is always already there */
}

int ssdiop_sceSdBlockTrans(short ch, unsigned short mode, void *iop, unsigned size)
{
    ch &= 1;
    if ((mode & 3) == 2) {
        spu2_adma_stop(ch);
        return 0;
    }
    spu2_adma_start(ch, iop, size, (mode & 0x10) != 0);
    return 0;
}
unsigned ssdiop_sceSdBlockTransStatus(short ch, short flag)
{
    (void)flag;
    return spu2_adma_status(ch & 1);
}

/* ---- SIF ------------------------------------------------------------ */
static void *(*rpc_fn)(int, void *, int);
static void *rpc_buf;
static pthread_cond_t rpc_cv = PTHREAD_COND_INITIALIZER;

int ssdiop_sceSifCheckInit(void) { return 1; }
void ssdiop_sceSifInit(void) {}
void ssdiop_sceSifInitRpc(int mode) { (void)mode; }
void ssdiop_sceSifSetRpcQueue(void *q, int tid) { (void)q; (void)tid; }
void ssdiop_sceSifRegisterRpc(void *srv, int id, void *(*fn)(int, void *, int), void *buf,
                              void *cf, void *cb, void *q)
{
    (void)srv; (void)cf; (void)cb; (void)q;
    if (id == 0x900) {
        rpc_fn = fn;
        rpc_buf = buf;
        pthread_cond_broadcast(&rpc_cv);
    }
}
void ssdiop_sceSifRpcLoop(void *q)
{
    (void)q;
    static pthread_cond_t never = PTHREAD_COND_INITIALIZER;
    for (;;) /* requests are served on the EE thread, see xsg_sound_rpc */
        pthread_cond_wait(&never, &L);
}
int ssdiop_sceSifBindRpc(void *client, int id, int mode)
{
    (void)mode;
    ((void **)client)[9] = (void *)(intptr_t)id; /* "server" (offset 36) non-null: bound */
    return 0;
}
int ssdiop_sceSifCheckStatRpc(void *client) { (void)client; return 0; }

/* IOP -> EE: queued for the EE (iop.c), the end callback runs right away
 * (the packet has been copied, as after the real SIF DMA). */
int ssdiop_sceSifCallRpc(void *client, int fno, int mode, void *send, int ssize, void *recv,
                         int rsize, void (*end)(void *), void *ep)
{
    (void)client; (void)mode; (void)recv; (void)rsize;
    SLOG("[ssd] IOP -> EE 0x901 cmd 0x%x (%d bytes)\n", fno, ssize);
    xsg_iop_call_ee(0x901, (unsigned)fno, send, ssize);
    if (end)
        end(ep);
    return 0;
}

/* ---- output: WAV and ALSA ------------------------------------------- */

static void wav_header(void)
{
    unsigned data = (unsigned)(wav_frames * 4);
    unsigned char h[44];
    memcpy(h, "RIFF", 4);
    unsigned v = 36 + data;
    memcpy(h + 4, &v, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    v = 16; memcpy(h + 16, &v, 4);
    unsigned short s = 1; memcpy(h + 20, &s, 2);
    s = 2; memcpy(h + 22, &s, 2);
    v = RATE; memcpy(h + 24, &v, 4);
    v = RATE * 4; memcpy(h + 28, &v, 4);
    s = 4; memcpy(h + 32, &s, 2);
    s = 16; memcpy(h + 34, &s, 2);
    memcpy(h + 36, "data", 4);
    memcpy(h + 40, &data, 4);
    long pos = ftell(wav);
    fseek(wav, 0, SEEK_SET);
    fwrite(h, 1, 44, wav);
    fseek(wav, pos, SEEK_SET);
}

static void output(const int16_t *s, int frames)
{
    pthread_mutex_lock(&wav_lock);
    if (wav) {
        static unsigned calls;
        fwrite(s, 4, frames, wav);
        wav_frames += frames;
        if (++calls % 256 == 0) { /* keep the header current (~0.5 s): killed runs stay readable */
            wav_header();
            fflush(wav);
        }
    }
    pthread_mutex_unlock(&wav_lock);
    if (pcm) {
        long r = xsg_alsa_write(pcm, s, (unsigned)frames);
        if (r == -11)
            alsa_dropped += frames;
        else if (r < 0)
            alsa_dropped += frames;
        else if (r < frames)
            alsa_dropped += frames - r;
    }
}

static void wav_close(void)
{
    pthread_mutex_lock(&wav_lock);
    if (wav) {
        wav_header();
        fclose(wav);
        wav = NULL;
    }
    pthread_mutex_unlock(&wav_lock);
}

/* ---- the IOP clock -------------------------------------------------- */
static void render(int frames, int16_t *out)
{
    while (frames > 0) {
        int n = frames;
        if (T.running && T.handler) {
            long long until = (T.remaining + CLK_PER_SAMPLE - 1) / CLK_PER_SAMPLE;
            if (until < 1)
                until = 1;
            if (until < n)
                n = (int)until;
        }
        spu2_render(out, n);
        out += 2 * n;
        frames -= n;
        if (T.running && T.handler) {
            T.remaining -= (long long)n * CLK_PER_SAMPLE;
            if (T.remaining <= 0) {
                unsigned next = T.handler(T.arg);
                T.count++;
                if (next)
                    T.interval = next;
                if (!next)
                    T.running = 0;
                T.remaining += T.interval;
                if (T.remaining <= 0)
                    T.remaining = T.interval;
            }
        }
    }
}

static void *audio_thread(void *p)
{
    (void)p;
    static int16_t buf[2 * 2048];
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    double budget = 0; /* frames real time allows and that are not rendered yet */
    unsigned frame0 = xsg_frame;
    unsigned long long ticks = 0;
    for (;;) {
        next.tv_nsec += 2000000;
        if (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }
        while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL) == EINTR) {}
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long long late = (now.tv_sec - next.tv_sec) * 1000000000LL + (now.tv_nsec - next.tv_nsec);
        if (late > 100000000LL) /* > 100 ms behind (stopped in a debugger...): resync */
            next = now;
        budget += RATE * 0.002;
        /* keep the ALSA queue near 60 ms: the device clock drifts against
         * CLOCK_MONOTONIC, so render a little more or less */
        if (pcm) {
            long avail = xsg_alsa_avail(pcm);
            if (avail < 0)
                alsa_underruns++; /* xrun: the next write recovers */
            long queued = avail >= 0 ? (long)(RATE / 10) - avail : 0; /* 100 ms buffer */
            if (avail >= 0 && queued < RATE * 40 / 1000)
                budget += 8;
            else if (avail >= 0 && queued > RATE * 80 / 1000)
                budget -= 8;
        }
        if (budget > RATE / 10) /* never burst more than 100 ms */
            budget = RATE / 10;
        int frames = budget > 0 ? (int)budget : 0;
        /* The game clock: the EE feeds streams and starts sounds per game
         * frame, so the IOP must not run ahead of the EE's vblanks (800
         * samples each, 2 vblanks of slack).  When the game runs at full
         * speed this is the same as real time; when it runs slow, so does
         * the sound, at the right pitch (XSG_SOUND_CLOCK=real: wall clock). */
        if (game_clock) {
            long long allow = (long long)(xsg_frame - frame0 + 2) * (RATE / 60) - (long long)rendered;
            if (allow < frames) {
                frames = allow > 0 ? (int)allow : 0;
                clock_waits++;
            }
        }
        if (frames > 2048)
            frames = 2048;
        budget -= frames;
        pthread_mutex_lock(&L);
        for (int ch = 0; ch < 2; ch++)
            if (trans_pending[ch]) {
                trans_pending[ch] = 0;
                if (trans_handler[ch])
                    trans_handler[ch](ch, trans_arg[ch]);
            }
        if (frames)
            render(frames, buf);
        pthread_mutex_unlock(&L);
        rendered += frames;
        if (frames)
            output(buf, frames);
        if (++ticks % 2500 == 0) /* every ~5 s */
            SLOG("[sound] %.1f s rendered, %d voices active, %u keyons, timer ticks %llu, "
                 "stream: %u data blocks, %u IRQs; game-clock waits %llu; ALSA dropped %llu frames, "
                 "%llu underruns\n",
                 rendered / (double)RATE, spu2_active_voices(), spu2_keyons(), T.count,
                 stream_blocks, stream_irqs, clock_waits, alsa_dropped, alsa_underruns);
    }
    return NULL;
}

/* ---- entry points (iop.c) ------------------------------------------- */
int xsg_sound_enabled(void)
{
    const char *e = getenv("XSG_SOUND");
    return !(e && atoi(e) == 0);
}

/* sceSifLoadModule("...RSSD.IRX"): run the module's start(), then the IOP clock */
void xsg_sound_start(void)
{
    if (started || !xsg_sound_enabled())
        return;
    started = 1;
    const char *e = getenv("XSG_SOUND_LOG");
    log_on = e && atoi(e) != 0;
    trace_dir = getenv("XSG_SPU_TRACE");
    const char *ck = getenv("XSG_SOUND_CLOCK");
    game_clock = !(ck && !strcmp(ck, "real"));
    const char *w = getenv("XSG_WAV");
    if (w && *w) {
        wav = fopen(w, "wb");
        if (wav) {
            wav_header();
            atexit(wav_close);
            xsg_logf("[sound] recording the mix to %s (48 kHz stereo s16)\n", w);
        }
    }
    pcm = xsg_alsa_open(RATE, 100000);
    pthread_mutex_lock(&L);
    ssdiop_sceSdInit(0);
    ssdiop_start();
    while (!rpc_fn)
        pthread_cond_wait(&rpc_cv, &L);
    pthread_mutex_unlock(&L);
    pthread_t t;
    pthread_create(&t, NULL, audio_thread, NULL);
    pthread_detach(t);
    xsg_logf("[sound] IOP sound driver (src/audio) running on the software SPU2\n");
}

/* EE -> IOP server 0x900 */
int xsg_sound_rpc(unsigned fno, const void *send, int ssize, void *recv, int rsize)
{
    if (!started)
        return -1;
    if (log_on && send && ssize >= 32) {
        const int *w = send;
        const short *h = send;
        if (fno == 4) {
            for (int i = 0; i < w[5] && 32 * (i + 2) <= ssize; i++) {
                const int *q = w + 8 * (i + 1);
                xsg_logf("[sound] %7.2f EE -> IOP cmd 0x%x args %x %x %x %x (batch)\n",
                         rendered / (double)RATE, (unsigned short)*(const short *)q, q[4], q[5], q[6], q[7]);
            }
        } else if (fno != 0xb7 && fno != 0xb5 && fno != 0x80 && fno != 0x81 && fno != 0x82)
            xsg_logf("[sound] %7.2f EE -> IOP cmd 0x%x args %x %x %x %x data %d\n",
                     rendered / (double)RATE, (unsigned short)h[0], w[4], w[5], w[6], w[7], w[3]);
    }
    if (fno == 0xb5)
        stream_blocks++;
    pthread_mutex_lock(&L);
    if (ssize > 0x10020)
        ssize = 0x10020;
    if (send && ssize > 0)
        memcpy(rpc_buf, send, ssize);
    void *r = rpc_fn((int)fno, rpc_buf, ssize);
    if (recv && rsize > 0 && r)
        memcpy(recv, r, rsize);
    pthread_mutex_unlock(&L);
    return 0;
}

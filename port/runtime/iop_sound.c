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
 * sequencer, envelopes, fades), as much as tops the output queue up (xsg-gpu's
 * ring, or ALSA, never blocking), and records XSG_WAV.  It is held back only to
 * keep a VAG stream fed (sound_clock = auto, see "stream hold") or, with
 * sound_clock = game, to stay within 8 vblanks of the EE.
 *
 * XSG_SOUND=0 falls back to the Stage 4 model (no driver, iop.c acks).
 */
#define _GNU_SOURCE
#include <errno.h>
#include "xsg_settings.h"
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
/* gpu_client.cpp: the audio ring xsg-gpu plays (gpu_ipc.h GpuAudio) */
int xsg_audio_ring_open(unsigned rate);
long xsg_audio_ring_queued(void);
long xsg_audio_ring_write(const short *s, unsigned frames);
void xsg_iop_call_ee(unsigned rpc, unsigned command, const void *data, int size);

/* the driver (prefixed) */
int ssdiop_start(void);

static pthread_mutex_t L = PTHREAD_MUTEX_INITIALIZER;
static FILE *wav;
static pthread_mutex_t wav_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned long long wav_frames;
static void *pcm;
/* [renderer] gs = gpu: the mix goes to xsg-gpu's audio ring (played through SDL3 there);
 * live once xsg-gpu is up (it starts after the sound driver).  Otherwise ALSA here. */
static int use_ring, ring_live;
static volatile unsigned long long rendered;
static unsigned long long alsa_dropped, alsa_underruns;
static unsigned stream_blocks, stream_irqs; /* 0xb5 data commands, SPU2 IRQs */
/* [debug] sound_clock (§4 of STAGE10_SOUND.md): auto = real time, held only at a VAG
 * stream IRQ that would find no data; game = at most 8 vblanks ahead of the EE; real */
enum { CLOCK_AUTO, CLOCK_GAME, CLOCK_REAL };
static int sound_clock = CLOCK_AUTO;
static unsigned long long clock_waits; /* ticks the game clock or a stream hold kept the IOP back */
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
    if (r) {
        xsg_logf("[sound] cannot start IOP thread %d (pthread_create: %d)\n", id, r);
#ifdef _WIN32
        void xsg_w32_vm_report(void);
        xsg_w32_vm_report();
#endif
    }
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
#ifdef _WIN32
    if (!(p = _aligned_malloc(size > 0 ? (size_t)size : 1, 64)))
        return NULL;
#else
    if (posix_memalign(&p, 64, size > 0 ? (size_t)size : 1))
        return NULL;
#endif
    memset(p, 0, size > 0 ? (size_t)size : 1);
    return p;
}
#ifdef _WIN32
int ssdiop_FreeSysMemory(void *p) { _aligned_free(p); return 0; }
#else
int ssdiop_FreeSysMemory(void *p) { free(p); return 0; }
#endif
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

/* ---- stream hold (sound_clock = auto) --------------------------------
 * A VAG stream (cutscene voices, movie audio, the game-over theme) is fed by the EE once
 * per game frame (xglSoundStreamMain / xglMpeg2Play: one 4 KiB block when RSSD's ring
 * has room).  The SPU plays it from a 2 x 1 KiB buffer per channel; the SPU2 IRQ at each
 * 1 KiB (IRQA) DMAs the next block from SSD's double buffer, and every `half_blocks` IRQs
 * SsdVagStreamSpuTransfer calls RSSD's callback, which refills that half from the ring.
 * An empty ring there means silence, and two in a row tell the EE the stream has ended
 * (0x0a).  So the one moment the IOP must not pass while the EE is behind is that IRQ:
 * on_irq holds it (and stops the render) when the next refill would find an active
 * channel's ring empty, and the audio thread delivers it as soon as the EE has fed the
 * ring.  A held IRQ is up to one 2 ms render step late, far less than the ~37 ms the
 * SPU still has in the other half.  If the EE runs HOLD_VBLANKS vblanks (or HOLD_MAX_MS
 * of wall time) without feeding, it is not going to (stream end, or a stall): the IRQ
 * goes through and the stream drains as it would on a PS2, until the ring gets data.
 * Read-only views of the driver's state (layouts of src/audio/ssd/ssd_vag.c SsdVagWork
 * and src/audio/rssd/rssdstrm.c RssdIopStreamWork, checked against the build's debug
 * info). */
#define HOLD_VBLANKS 4
#define HOLD_MAX_MS 500
typedef struct {
    unsigned char flags, status, buffer_selector, dma_channel, mode;
    signed char channel_count, dma_channel_index;
    unsigned char process_state[4];
    int master_volume[4];
    unsigned char transfer_state[4];
    void *buffers[2];
    int buffer_bytes, half_buffer_bytes, transfer_stride;
    void *callback, *callback_arg, *transfer_source;
    unsigned char total_blocks, half_blocks, completed_blocks, buffer_index;
} SsdVagWorkView;
typedef struct { signed char capacity; unsigned char count; } RssdRingView;
typedef struct {
    short format;
    unsigned char active, drain_count;
    int chunk_size;
    RssdRingView *ring;
} RssdChannelView;
typedef struct {
    short initialized;
    unsigned char mode, channel_count;
    int chunk_size;
    RssdChannelView pcm, vag[4];
} RssdStreamView;
_Static_assert(sizeof(void *) != 4 || __builtin_offsetof(SsdVagWorkView, completed_blocks) == 66,
               "SsdVagWork layout");
_Static_assert(sizeof(void *) != 4 || __builtin_offsetof(RssdStreamView, vag) == 20, "RssdIopStreamWork layout");
extern SsdVagWorkView ssdiop_SsdVagStrWork;
extern RssdStreamView ssdiop_RssdStrWork;

static int in_render, held_core = -1, hold_released, stream_on;
static unsigned hold_frame;
static struct timespec hold_t0;
static unsigned long long holds, hold_frames_total, hold_gave_up;
static unsigned long long stream_starved; /* refills that found an active stream's ring empty */

static int refill_irq(void)
{
    const SsdVagWorkView *v = &ssdiop_SsdVagStrWork;
    return v->completed_blocks == 0 || v->completed_blocks == v->half_blocks;
}

/* an active VAG stream channel with nothing queued in RSSD's ring (IOP lock held) */
static int stream_hungry(void)
{
    const SsdVagWorkView *v = &ssdiop_SsdVagStrWork;
    const RssdStreamView *r = &ssdiop_RssdStrWork;
    if (!(v->flags & 0x20) || (v->flags & 2) || v->status != 1 || !r->initialized)
        return 0;
    int n = r->mode == 3 ? 1 : r->mode == 2 ? r->channel_count : 0;
    for (int i = 0; i < n && i < 4; i++)
        if (r->vag[i].active && r->vag[i].ring && r->vag[i].ring->count == 0)
            return 1;
    return 0;
}

static void on_irq(int core)
{
    stream_irqs++;
    /* this IRQ makes SsdVagStreamSpuTransfer refill a half (see above) */
    int hungry = refill_irq() && stream_hungry();
    if (hungry && sound_clock == CLOCK_AUTO && in_render && !hold_released) {
        held_core = core;
        hold_frame = xsg_frame;
        clock_gettime(CLOCK_MONOTONIC, &hold_t0);
        holds++;
        return;
    }
    stream_starved += hungry;
    if (spu2_handler)
        spu2_handler(1 << core, spu2_handler_arg);
}

/* Each tick, IOP lock held: deliver a held IRQ once the ring has data (or the stream
 * stopped), or once the EE has had its chance.  Returns 1 while still holding. */
static int stream_hold_poll(void)
{
    stream_on = sound_clock == CLOCK_AUTO && (ssdiop_SsdVagStrWork.flags & 0x20);
    if (!stream_hungry())
        hold_released = 0; /* fed (or stopped): hold again next time it runs dry */
    if (held_core < 0)
        return 0;
    if (stream_hungry()) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long long ms = (now.tv_sec - hold_t0.tv_sec) * 1000LL + (now.tv_nsec - hold_t0.tv_nsec) / 1000000;
        if (xsg_frame - hold_frame < HOLD_VBLANKS && ms < HOLD_MAX_MS)
            return 1;
        hold_released = 1; /* the EE is not feeding: let it drain */
        hold_gave_up++;
        stream_starved++;
    }
    int core = held_core;
    held_core = -1;
    if (spu2_handler)
        spu2_handler(1 << core, spu2_handler_arg);
    return 0;
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
    spu2_set_effect(core, mode, a->depth_l, a->depth_r, a->delay, a->feedback);
    if (a->mode & 0x100)
        spu2_clear_effect_area(core, mode);
    SLOG("[ssd] effect core %d mode %d depth %d/%d delay %d feedback %d\n", core, mode, a->depth_l, a->depth_r, a->delay, a->feedback);
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

/* ALSA output while the IOP is held back (§4 of STAGE10_SOUND.md: game clock,
 * a stream hold): the device would run dry.  Instead of an xrun (a hard gap and a click), the queue is
 * kept at a floor with "filler": the last output sample decaying to zero
 * (~1 ms time constant), and the next real audio ramps in over 2 ms.  Every
 * time filler is needed the target queue grows by 20 ms (up to 200 ms), so
 * a game that stutters regularly is absorbed by the buffer; it shrinks back
 * by 1 ms a second.  Only the device sees this: XSG_WAV keeps IOP time. */
#define FADE_FRAMES 96
#define ALSA_BUF_MS 250
#define FILL_FLOOR_MS 20
#define RING_TARGET_MS 40 /* queued in xsg-gpu's ring (its device buffer comes on top) */
/* while a VAG stream plays (auto): a cushion for stream holds, which last at most
   HOLD_VBLANKS (4, ~67 ms).  It was 100: the voice queued that far ahead played ~100 ms
   behind the faces, whose lip animation is keyed to game frames (#3, lip-sync drift in the
   Elsa cutscenes). */
#define RING_STREAM_MS 70
static double target_ms = 60;
static int fade_in, in_gap;
static int16_t last_out[2];
static double fill_level[2];
static unsigned long long filled_frames, fill_events;

static void output_fill(int frames)
{
    static int16_t buf[2 * 2048];
    if (frames > 2048)
        frames = 2048;
    if (!in_gap) {
        fill_level[0] = last_out[0];
        fill_level[1] = last_out[1];
        in_gap = 1;
        fill_events++;
    }
    for (int i = 0; i < frames; i++) {
        fill_level[0] *= 0.98;
        fill_level[1] *= 0.98;
        buf[2 * i] = (int16_t)fill_level[0];
        buf[2 * i + 1] = (int16_t)fill_level[1];
    }

    xsg_alsa_write(pcm, buf, (unsigned)frames);
    filled_frames += frames;
    fade_in = FADE_FRAMES;
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
    if (ring_live) {
        long r = xsg_audio_ring_write(s, (unsigned)frames);
        if (r < frames)
            alsa_dropped += frames - r;
    } else if (pcm) {
        /* after a filled gap the device resumes from silence: ramp in over 2 ms */
        static int16_t ramp_buf[2 * 2048];
        if (fade_in > 0 && frames <= 2048) {
            for (int i = 0; i < frames; i++) {
                int g = fade_in > 0 ? (FADE_FRAMES - fade_in--) : FADE_FRAMES;
                ramp_buf[2 * i] = (int16_t)(s[2 * i] * g / FADE_FRAMES);
                ramp_buf[2 * i + 1] = (int16_t)(s[2 * i + 1] * g / FADE_FRAMES);
            }
            s = ramp_buf;
        }
        in_gap = 0;
        last_out[0] = s[2 * (frames - 1)];
        last_out[1] = s[2 * (frames - 1) + 1];
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
/* Renders up to `frames`; returns fewer when a stream IRQ was held (on_irq), which
 * stops the IOP at that sample until stream_hold_poll delivers it. */
static int render(int frames, int16_t *out)
{
    int done = 0;
    in_render = 1;
    while (done < frames && held_core < 0) {
        int n = frames - done;
        if (T.running && T.handler) {
            long long until = (T.remaining + CLK_PER_SAMPLE - 1) / CLK_PER_SAMPLE;
            if (until < 1)
                until = 1;
            if (until < n)
                n = (int)until;
        }
        spu2_render(out + 2 * done, n);
        done += n;
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
    in_render = 0;
    return done;
}

static void *audio_thread(void *p)
{
    (void)p;
    static int16_t buf[2 * 2048];
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    double budget = 0; /* no device: frames real time allows and that are not rendered yet */
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
        /* Real time is the device's clock: render what tops its queue up to the target
         * (xsg-gpu's ring: SDL pulls it at the device's pace; ALSA: 60 ms, more after
         * gaps, see output_fill), so the two clocks stay locked without a drift nudge.
         * With no device, count wall time. */
        int frames;
        long queued = -1;
        if (use_ring && !ring_live)
            ring_live = xsg_audio_ring_open(RATE);
        if (ring_live) {
            queued = xsg_audio_ring_queued();
            frames = (int)(RATE * (stream_on ? RING_STREAM_MS : RING_TARGET_MS) / 1000 - queued);
        } else if (pcm) {
            long avail = xsg_alsa_avail(pcm);
            if (avail < 0)
                alsa_underruns++; /* xrun: the next write recovers */
            queued = avail >= 0 ? (long)(RATE * ALSA_BUF_MS / 1000) - avail : 0;
            frames = (int)(RATE * target_ms / 1000 - queued);
            if (target_ms > 60)
                target_ms -= 0.002; /* 1 ms a second */
        } else {
            budget += RATE * 0.002;
            if (budget > RATE / 10)
                budget = RATE / 10;
            frames = (int)budget;
        }
        if (frames < 0)
            frames = 0;
        int gated = 0;
        /* sound_clock = game: the IOP must not run ahead of the EE's vblanks (800 samples
         * each, 8 vblanks of slack: a renderer hitch shorter than ~130 ms does not gap the
         * music).  When the game runs slow, so does all the sound, at the right pitch. */
        if (sound_clock == CLOCK_GAME) {
            long long allow = (long long)(xsg_frame - frame0 + 8) * (RATE / 60) - (long long)rendered;
            if (allow < frames) {
                frames = allow > 0 ? (int)allow : 0;
                gated = 1;
            }
        }
        if (frames > 2048)
            frames = 2048;
        pthread_mutex_lock(&L);
        for (int ch = 0; ch < 2; ch++)
            if (trans_pending[ch]) {
                trans_pending[ch] = 0;
                if (trans_handler[ch])
                    trans_handler[ch](ch, trans_arg[ch]);
            }
        /* sound_clock = auto: a stream IRQ held until the EE feeds the stream */
        if (stream_hold_poll()) {
            hold_frames_total += RATE / 500; /* one 2 ms tick */
            frames = 0;
            gated = 1;
        }
        if (frames) {
            int done = render(frames, buf);
            if (done < frames)
                gated = 1;
            frames = done;
        }
        pthread_mutex_unlock(&L);
        if (gated)
            clock_waits++;
        if (!ring_live && !pcm)
            budget -= frames;
        rendered += frames;
        if (frames)
            output(buf, frames);
        /* the IOP is behind real time and the device is about to run dry */
        if (pcm && !ring_live && gated && queued >= 0 && queued + frames < RATE * FILL_FLOOR_MS / 1000) {
            output_fill(RATE * FILL_FLOOR_MS / 1000 - (int)(queued + frames));
            target_ms = target_ms + 20 > 200 ? 200 : target_ms + 20;
        }
        if (++ticks % 2500 == 0) /* every ~5 s */
            SLOG("[sound] %.1f s rendered, %d voices active, %u keyons, timer ticks %llu, "
                 "stream: %u data blocks, %u IRQs, %llu starved refills, %llu holds (%.2f s held, %llu let drain); clock waits %llu; "
                 "dropped %llu frames, ALSA %llu underruns, gap fill %llu frames in %llu gaps (target %.0f ms)\n",
                 rendered / (double)RATE, spu2_active_voices(), spu2_keyons(), T.count,
                 stream_blocks, stream_irqs, stream_starved, holds, hold_frames_total / (double)RATE, hold_gave_up,
                 clock_waits, alsa_dropped, alsa_underruns, filled_frames, fill_events, target_ms);
        if (log_on >= 2 && ticks % 2500 == 0) { /* XSG_SOUND_LOG=2: every voice that is on */
            pthread_mutex_lock(&L);
            for (int c = 0; c < 2; c++)
                for (int v = 0; v < 24; v++) {
                    spu2_voice_info_t vi;
                    if (spu2_voice_info(c, v, &vi))
                        xsg_logf("[sound]   c%d v%02d ph %d env %04x pitch %04x adsr %04x/%04x vol %6d/%6d "
                                 "ssa %06x lsa %06x nax %06x%s%s%s\n",
                                 c, v, vi.phase, vi.env, vi.pitch, vi.adsr1, vi.adsr2, vi.vol_l, vi.vol_r,
                                 vi.ssa, vi.lsa, vi.nax, vi.noise ? " NOISE" : "", vi.pmod ? " PMOD" : "",
                                 vi.wet ? " wet" : "");
                }
            pthread_mutex_unlock(&L);
        }
    }
    return NULL;
}

/* ---- entry points (iop.c) ------------------------------------------- */
/* In-game menu (kernel.c, per vblank when changed): audio on/off and the
 * master / music / effects / voice volumes in percent. */
void xsg_sound_set_volume(int on, const int vol[4])
{
    int q[4];
    for (int i = 0; i < 4; i++) {
        int v = vol[i] < 0 ? 0 : vol[i] > 100 ? 100 : vol[i];
        /* a perceptual curve: percent squared (50 % = -12 dB) */
        q[i] = v * v * 32768 / 10000;
    }
    if (!on)
        q[0] = 0;
    spu2_set_mix_gains(q[0], q[1], q[2], q[3]);
    xsg_logf("[settings] audio %s, volume master %d music %d effects %d voice %d (Q15 %d/%d/%d/%d)\n",
             on ? "on" : "muted", vol[0], vol[1], vol[2], vol[3], q[0], q[1], q[2], q[3]);
}

int xsg_sound_enabled(void)
{
    const char *e = xsg_cfg("audio.sound_driver");
    return !(e && atoi(e) == 0);
}

/* sceSifLoadModule("...RSSD.IRX"): run the module's start(), then the IOP clock */
void xsg_sound_start(void)
{
    if (started || !xsg_sound_enabled())
        return;
    started = 1;
    const char *e = xsg_cfg("debug.sound_log");
    log_on = e ? atoi(e) : 0;
    trace_dir = xsg_cfg("debug.spu_trace");
    const char *ck = xsg_cfg("debug.sound_clock");
    sound_clock = ck && !strcmp(ck, "game") ? CLOCK_GAME : ck && !strcmp(ck, "real") ? CLOCK_REAL : CLOCK_AUTO;
    const char *w = xsg_cfg("debug.wav");
    if (w && *w) {
        wav = fopen(w, "wb");
        if (wav) {
            wav_header();
            atexit(wav_close);
            xsg_logf("[sound] recording the mix to %s (48 kHz stereo s16)\n", w);
        }
    }
    {
        const char *gs = xsg_cfg("renderer.gs"), *en = xsg_cfg("audio.enabled");
        const int enabled = !(en && atoi(en) == 0);
        use_ring = enabled && gs && (!strcmp(gs, "gpu") || !strcmp(gs, "both"));
    }
    if (!use_ring)
        pcm = xsg_alsa_open(RATE, ALSA_BUF_MS * 1000);
    xsg_logf("[sound] starting the IOP sound driver (output: %s)\n", use_ring ? "xsg-gpu" : pcm ? "ALSA" : "none");
    pthread_mutex_lock(&L);
    ssdiop_sceSdInit(0);
    ssdiop_start();
    xsg_logf("[sound] driver threads started; waiting for its RPC server\n");
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

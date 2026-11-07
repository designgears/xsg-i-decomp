/*
 * RSSD stereo VAG stream (SsdInitVagStreamStereo / SsdSetVagStreamDataStereo /
 * SsdGetVagStreamStatusStereo...), as used by the .pss movie player and the
 * scene voice streams.
 *
 * The game hands the "IOP" 0x1000-byte blocks of SPU ADPCM, stereo
 * interleaved in 0x400-byte runs (L, R, L, R; the PSS SShd header says
 * interleave 0x400), and asks the stream status for how many blocks it may
 * send: value >> 8 = free blocks, low byte nonzero = stream finished.  The
 * model keeps a 4-block queue that drains at the stream's sample rate
 * (48 kHz * pitch / 0x1000) measured in vblanks, so the game's feeding is
 * paced by the same clock as the rest of the game, also under XSG_FAST.
 *
 * Decoded samples go to ALSA when the 32-bit libasound.so.2 can be dlopen'ed
 * (nothing is linked; XSG_AUDIO=0 disables it).  If the host device falls
 * behind, samples are dropped rather than slowing the game.
 *
 * Stage 10: this model is only used with XSG_SOUND=0.  By default the real
 * sound driver (iop_sound.c) plays the streams through the software SPU2 and
 * uses the xsg_alsa_* helpers below for its output.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "xsg_rt.h"

#define QUEUE_BLOCKS 4
#define BLOCK_BYTES 0x1000
#define INTERLEAVE 0x400
#define SAMPLES_PER_BLOCK (BLOCK_BYTES / 2 / 16 * 28)

static struct {
    int initialised, playing;
    unsigned rate;            /* Hz */
    double queued;            /* samples per channel not yet "played" */
    unsigned last_frame;
    int hist[2][2];
    unsigned long long blocks;
} V;

/* ---- ALSA through dlopen ------------------------------------------- */
/* Shared with iop_sound.c (Stage 10): xsg_alsa_* open independent PCMs. */
typedef struct snd_pcm snd_pcm_t;
static struct {
    int tried, ok;
    snd_pcm_t *pcm; /* the VAG-stream model's PCM (XSG_SOUND=0 only) */
    unsigned rate;
    int (*open)(snd_pcm_t **, const char *, int, int);
    int (*set_params)(snd_pcm_t *, int, int, unsigned, unsigned, int, unsigned);
    long (*writei)(snd_pcm_t *, const void *, unsigned long);
    int (*recover)(snd_pcm_t *, int, int);
    int (*close)(snd_pcm_t *);
    int (*drop)(snd_pcm_t *);
    long (*avail)(snd_pcm_t *);
} A;

static int alsa_load(void)
{
    if (!A.tried) {
        A.tried = 1;
        const char *e = getenv("XSG_AUDIO");
        if (e && atoi(e) == 0)
            return 0;
        void *h = dlopen("libasound.so.2", RTLD_NOW);
        if (!h) {
            xsg_logf("[audio] no 32-bit libasound.so.2 (%s): no host audio output\n", dlerror());
            return 0;
        }
        A.open = dlsym(h, "snd_pcm_open");
        A.set_params = dlsym(h, "snd_pcm_set_params");
        A.writei = dlsym(h, "snd_pcm_writei");
        A.recover = dlsym(h, "snd_pcm_recover");
        A.close = dlsym(h, "snd_pcm_close");
        A.drop = dlsym(h, "snd_pcm_drop");
        A.avail = dlsym(h, "snd_pcm_avail_update");
        A.ok = A.open && A.set_params && A.writei && A.recover && A.close;
    }
    return A.ok;
}

/* Non-blocking S16LE stereo PCM on "default"; NULL when there is none. */
void *xsg_alsa_open(unsigned rate, unsigned latency_us)
{
    snd_pcm_t *pcm = NULL;
    if (!alsa_load())
        return NULL;
    /* SND_PCM_STREAM_PLAYBACK 0, SND_PCM_NONBLOCK 1, FORMAT_S16_LE 2,
     * ACCESS_RW_INTERLEAVED 3; 2 channels, resample */
    if (A.open(&pcm, "default", 0, 1) < 0) {
        xsg_logf("[audio] snd_pcm_open failed\n");
        return NULL;
    }
    if (A.set_params(pcm, 2, 3, 2, rate, 1, latency_us) < 0) {
        xsg_logf("[audio] snd_pcm_set_params(%u Hz) failed\n", rate);
        A.close(pcm);
        return NULL;
    }
    xsg_logf("[audio] ALSA output %u Hz stereo, %u us buffer\n", rate, latency_us);
    return pcm;
}

/* Frames written; never blocks.  A full device drops the rest (EAGAIN);
 * an underrun (EPIPE) or suspend is recovered and the write retried once. */
long xsg_alsa_write(void *pcm, const short *s, unsigned frames)
{
    if (!pcm)
        return 0;
    long r = A.writei(pcm, s, frames);
    if (r < 0 && r != -11) {
        A.recover(pcm, (int)r, 1);
        r = A.writei(pcm, s, frames);
    }
    return r;
}

/* Frames the device can take now (< 0: error, e.g. -EPIPE after an underrun). */
long xsg_alsa_avail(void *pcm)
{
    if (!pcm || !A.avail)
        return -1;
    return A.avail(pcm);
}

void xsg_alsa_close(void *pcm)
{
    if (!pcm)
        return;
    if (A.drop)
        A.drop(pcm);
    A.close(pcm);
}

static void alsa_open(unsigned rate)
{
    if (A.pcm && A.rate == rate)
        return;
    if (A.pcm) {
        A.close(A.pcm);
        A.pcm = NULL;
    }
    A.pcm = xsg_alsa_open(rate, 200000);
    A.rate = A.pcm ? rate : 0;
}

static void alsa_write(const short *s, unsigned frames)
{
    xsg_alsa_write(A.pcm, s, frames);
}

/* ---- SPU ADPCM ------------------------------------------------------ */
static const int filt[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};

static void decode_run(const unsigned char *p, int bytes, int ch, short *out)
{
    int h1 = V.hist[ch][0], h2 = V.hist[ch][1];
    for (int k = 0; k < bytes; k += 16, p += 16) {
        int shift = p[0] & 15, f = p[0] >> 4;
        if (f > 4)
            f = 4;
        for (int j = 0; j < 28; j++) {
            int n = (p[2 + j / 2] >> ((j & 1) * 4)) & 15;
            if (n >= 8)
                n -= 16;
            int s = (shift <= 12 ? (n << 12) >> shift : 0) + ((h1 * filt[f][0] + h2 * filt[f][1] + 32) >> 6);
            if (s > 32767)
                s = 32767;
            if (s < -32768)
                s = -32768;
            *out = (short)s;
            out += 2;
            h2 = h1;
            h1 = s;
        }
    }
    V.hist[ch][0] = h1;
    V.hist[ch][1] = h2;
}

static void drain(void)
{
    unsigned now = xsg_frame;
    if (V.playing && V.rate)
        V.queued -= (double)(now - V.last_frame) * V.rate / 60.0;
    if (V.queued < 0)
        V.queued = 0;
    V.last_frame = now;
}

/* ---- RSSD commands (iop.c) ------------------------------------------ */
void xsg_vagstream_init(void)
{
    memset(&V.hist, 0, sizeof V.hist);
    V.initialised = 1;
    V.playing = 0;
    V.queued = 0;
    V.rate = 48000;
    V.last_frame = xsg_frame;
}

void xsg_vagstream_play(int pitch)
{
    drain();
    V.rate = pitch > 0 ? (unsigned)((long long)pitch * 48000 / 0x1000) : 48000;
    V.playing = 1;
    alsa_open(V.rate);
    xsg_logf("[audio] VAG stream play, %u Hz\n", V.rate);
}

void xsg_vagstream_stop(void)
{
    if (V.playing)
        xsg_logf("[audio] VAG stream stop after %llu blocks\n", V.blocks);
    V.playing = 0;
    V.queued = 0;
    V.blocks = 0;
    if (A.pcm && A.drop)
        A.drop(A.pcm);
    if (A.pcm) {
        A.close(A.pcm);
        A.pcm = NULL;
    }
}

void xsg_vagstream_data(const void *data, int size)
{
    static short pcm[SAMPLES_PER_BLOCK * 2];
    drain();
    const unsigned char *p = data;
    int frames = 0;
    for (int off = 0; off + 2 * INTERLEAVE <= size; off += 2 * INTERLEAVE) {
        decode_run(p + off, INTERLEAVE, 0, pcm + frames * 2);
        decode_run(p + off + INTERLEAVE, INTERLEAVE, 1, pcm + frames * 2 + 1);
        frames += INTERLEAVE / 16 * 28;
    }
    V.queued += frames;
    V.blocks++;
    if (V.playing)
        alsa_write(pcm, (unsigned)frames);
}

/* value >> 8: blocks the game may send now; low byte: finished */
int xsg_vagstream_status(void)
{
    drain();
    if (!V.initialised)
        return 0;
    int used = (int)((V.queued + SAMPLES_PER_BLOCK - 1) / SAMPLES_PER_BLOCK);
    int free_blocks = QUEUE_BLOCKS - used;
    if (free_blocks < 0)
        free_blocks = 0;
    return free_blocks << 8;
}

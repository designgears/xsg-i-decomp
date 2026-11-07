/*
 * Software SPU2 checks (Stage 10): port/build/test_spu2 (cmake target test_spu2).
 *
 * A 1 kHz sine is encoded to PS-ADPCM (filter 0/1, best shift per block)
 * as a looping sample and played through the libsd-style register
 * interface.  Checks: the output frequency follows the pitch register
 * (0x1000 = 48 kHz: 1 kHz; 0x0800: 500 Hz; 0x2000: 2 kHz), the loop keeps
 * the voice sounding long after the sample's end, the ADSR attack reaches
 * full level and key off releases to silence, ENDX, the IRQ address fires,
 * volume halves the level, and the reverb neither blows up nor stays silent.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spu2.h"

static int fails;
#define CHECK(c, ...)                                                                             \
    do {                                                                                          \
        if (!(c)) {                                                                               \
            printf("FAIL: " __VA_ARGS__);                                                         \
            printf("\n");                                                                         \
            fails++;                                                                              \
        } else {                                                                                  \
            printf("ok:   " __VA_ARGS__);                                                         \
            printf("\n");                                                                         \
        }                                                                                         \
    } while (0)

/* ---- a small PS-ADPCM encoder (filters 0 and 1) ---------------------- */
static void encode(const short *pcm, int n, unsigned char *out, int loop_start_block)
{
    int s1 = 0, s2 = 0, blocks = n / 28;
    static const int fa[2][2] = {{0, 0}, {60, 0}};
    for (int b = 0; b < blocks; b++) {
        const short *x = pcm + b * 28;
        int best_err = -1, best_f = 0, best_sh = 0;
        unsigned char best[16];
        int best_s1 = 0, best_s2 = 0;
        for (int f = 0; f < 2; f++)
            for (int sh = 0; sh <= 12; sh++) {
                unsigned char blk[16] = {0};
                int h1 = s1, h2 = s2, err = 0;
                for (int i = 0; i < 28; i++) {
                    int pred = (h1 * fa[f][0] + h2 * fa[f][1] + 32) >> 6;
                    int d = x[i] - pred;
                    int q = (int)lround((double)d * (1 << sh) / 4096.0);
                    if (q > 7) q = 7;
                    if (q < -8) q = -8;
                    int s = ((short)(q << 12) >> sh) + pred;
                    if (s > 32767) s = 32767;
                    if (s < -32768) s = -32768;
                    err += abs(s - x[i]);
                    blk[2 + i / 2] |= (q & 15) << ((i & 1) * 4);
                    h2 = h1;
                    h1 = s;
                }
                if (best_err < 0 || err < best_err) {
                    best_err = err;
                    best_f = f;
                    best_sh = sh;
                    memcpy(best, blk, 16);
                    best_s1 = h1;
                    best_s2 = h2;
                }
            }
        best[0] = (unsigned char)(best_f << 4 | best_sh);
        best[1] = 0;
        if (b == loop_start_block)
            best[1] |= 4;
        if (b == blocks - 1)
            best[1] |= 3; /* loop end + repeat */
        memcpy(out + b * 16, best, 16);
        s1 = best_s1;
        s2 = best_s2;
    }
}

static double freq_of(const short *st, int n)
{
    /* zero crossings of the left channel, interpolated */
    int first = -1, last = -1, count = 0;
    double fpos = 0, lpos = 0;
    for (int i = 1; i < n; i++) {
        int a = st[2 * (i - 1)], b = st[2 * i];
        if (a < 0 && b >= 0) {
            double p = i - 1 + (double)-a / (b - a);
            if (first < 0) {
                first = i;
                fpos = p;
            } else
                count++;
            last = i;
            lpos = p;
        }
    }
    (void)last;
    return count > 0 ? count / ((lpos - fpos) / 48000.0) : 0;
}

static double rms(const short *st, int n)
{
    double s = 0;
    for (int i = 0; i < n; i++)
        s += (double)st[2 * i] * st[2 * i];
    return sqrt(s / n);
}

static int irq_hits;
static void on_irq(int core) { (void)core; irq_hits++; }

#define N (48000 / 2)
static short out[2 * N];

static void play(uint16_t pitch, uint16_t vol)
{
    spu2_set_param(0x0200 | (0 << 1) | 1, pitch);
    spu2_set_param(0x0000 | 1, vol);
    spu2_set_param(0x0100 | 1, vol);
    spu2_set_switch(0x1501, 1);
    spu2_render(out, N);
}

int main(void)
{
    enum { LEN = 28 * 200 };
    static short pcm[LEN];
    static unsigned char adpcm[LEN / 28 * 16];
    /* 1 kHz at 48 kHz: 48 samples a cycle; 5600 samples = 116.67 cycles, so
     * loop from block 12 (336 samples = 7 cycles) to the end: 5264 = 109.67
     * cycles -- not whole; use 5376 samples (192 blocks, 112 cycles) */
    int n = 28 * 192;
    for (int i = 0; i < n; i++)
        pcm[i] = (short)(16000 * sin(2 * M_PI * 1000.0 * i / 48000.0));
    encode(pcm, n, adpcm, 0);

    spu2_reset();
    spu2_set_irq_callback(on_irq);
    spu2_set_param(0x0881, 0x0ffc); /* core 1: voices + ext dry */
    spu2_set_param(0x0981, 0x3fff);
    spu2_set_param(0x0a81, 0x3fff);
    spu2_set_switch(0x1801, 0xffffff); /* dry L/R */
    spu2_set_switch(0x1a01, 0xffffff);
    spu2_dma_write(0x10000, adpcm, n / 28 * 16);
    spu2_set_addr(0x2040 | 1, 0x10000);
    spu2_set_param(0x0300 | 1, 0x000f); /* attack fastest (linear rate 0), sustain level max */
    spu2_set_param(0x0400 | 1, 0xc000 | 0x1f); /* sustain: hold; release: slow-ish */
    spu2_set_param(0x0400 | 1, 0x0000 | 0x0a);  /* sustain increase rate 0 = stay at max, release rate 0xa */

    play(0x1000, 0x3fff);
    double f = freq_of(out + 2 * 4800, N - 4800);
    CHECK(fabs(f - 1000) < 5, "pitch 0x1000 plays the 1 kHz sample at %.1f Hz", f);
    double r_full = rms(out + 2 * 4800, N - 4800);
    CHECK(r_full > 5000, "attack reaches full level (RMS %.0f)", r_full);
    CHECK(spu2_get_switch(0x1701) & 1, "ENDX set after the loop end");
    CHECK(spu2_active_voices() == 1, "the looping voice is still on after %.2f s (sample is %.2f s)",
          N / 48000.0, n / 48000.0);

    play(0x0800, 0x3fff);
    f = freq_of(out + 2 * 4800, N - 4800);
    CHECK(fabs(f - 500) < 3, "pitch 0x0800 plays at %.1f Hz", f);
    play(0x2000, 0x3fff);
    f = freq_of(out + 2 * 4800, N - 4800);
    CHECK(fabs(f - 2000) < 10, "pitch 0x2000 plays at %.1f Hz", f);

    play(0x1000, 0x1fff);
    double r_half = rms(out + 2 * 4800, N - 4800);
    CHECK(fabs(r_half / r_full - 0.5) < 0.02, "volume 0x1fff gives %.3f of 0x3fff", r_half / r_full);

    spu2_set_switch(0x1601, 1);
    spu2_render(out, N);
    CHECK(rms(out + 2 * (N - 4800), 4800) < 1 && spu2_active_voices() == 0,
          "key off releases to silence (voice off: %d)", spu2_active_voices() == 0);

    /* IRQ: core 1 IRQA inside the sample, enable, play */
    irq_hits = 0;
    spu2_set_addr(0x1f01, 0x10000 + 16 * 50);
    spu2_set_core_attr(4 | 1, 1);
    play(0x1000, 0x3fff);
    CHECK(irq_hits == 1, "IRQ at a block address fires once until re-enabled (%d)", irq_hits);
    spu2_set_core_attr(4 | 1, 0);

    /* reverb (hall) on core 1 */
    spu2_set_addr(0x1d01, 0x1fffff);
    spu2_set_effect(1, 5, 0x3000, 0x3000, 0, 0);
    spu2_clear_effect_area(1, 5);
    spu2_set_core_attr(2 | 1, 1);
    spu2_set_switch(0x1901, 1);
    spu2_set_switch(0x1b01, 1);
    play(0x1000, 0x3fff);
    spu2_set_switch(0x1601, 1);
    spu2_render(out, N);
    double tail = rms(out + 2 * 2400, 9600);
    int clipped = 0;
    for (int i = 0; i < 2 * N; i++)
        clipped += out[i] == 32767 || out[i] == -32768;
    CHECK(tail > 50 && clipped < N / 100, "reverb tail after key off (RMS %.0f), %d clipped samples", tail,
          clipped);

    /* the resampling FIR passes 1 kHz at unity: a wet-only 1 kHz tone through a
     * reverb whose only path is in -> mLSAME -> comb (vLIN/vCOMB1 0x7fff, the
     * rest zero) comes out at the dry level */
    spu2_set_effect(1, 8, 0x7fff, 0x7fff, 127, 0); /* delay mode: d = 0x1000 */
    CHECK(spu2_get_reverb_reg(1, 10) == 0x1ffd && spu2_get_reverb_reg(1, 11) == 0x0ffd &&
              spu2_get_reverb_reg(1, 7) == 0,
          "delay 127 reproduces libsd's preset (mLSAME 0x%04x mRSAME 0x%04x vWALL 0x%04x)",
          spu2_get_reverb_reg(1, 10), spu2_get_reverb_reg(1, 11), spu2_get_reverb_reg(1, 7));
    spu2_set_effect(1, 7, 0x7fff, 0x7fff, 63, 64);
    CHECK(spu2_get_reverb_reg(1, 10) == 0x0ffd && spu2_get_reverb_reg(1, 7) == 64 * 258 &&
              spu2_get_reverb_reg(1, 16) == (uint16_t)(spu2_get_reverb_reg(1, 17) + 0x800),
          "echo delay 63 feedback 64: mLSAME 0x%04x vWALL 0x%04x dLSAME 0x%04x",
          spu2_get_reverb_reg(1, 10), spu2_get_reverb_reg(1, 7), spu2_get_reverb_reg(1, 16));
    /* delay mode, delay 63: mLSAME - mLCOMB1 = d - 12 units, plus the two all-pass
     * stages (dAPF 3 units each, vAPF 0: pure delays): (0x800 - 6) * 4 = 8168
     * 24 kHz steps = 0.3403 s, plus the FIR's 2 x 19 samples: 0.3411 s.  Dry off, a 1 kHz tone keyed on at t = 0. */
    static short big[2 * 48000];
    spu2_set_switch(0x1801, 0);
    spu2_set_switch(0x1a01, 0);
    spu2_set_effect(1, 8, 0x7fff, 0x7fff, 63, 0);
    spu2_set_param(0x0500 | 1, 0); /* end the hall test's release */
    spu2_render(big, 2400);        /* and flush the resampling rings */
    spu2_clear_effect_area(1, 8);
    spu2_set_param(0x0200 | 1, 0x1000);
    spu2_set_switch(0x1501, 1);
    spu2_render(big, 48000);
    int onset = -1;
    for (int i = 0; i < 48000 && onset < 0; i++)
        if (abs(big[2 * i]) > 2000)
            onset = i;
    CHECK(onset >= 0 && fabs(onset / 48000.0 - 0.3411) < 0.002, "delay 63 echo onset at %.4f s (0.3411)",
          onset / 48000.0);
    double wet = rms(big + 2 * 24000, 24000);
    CHECK(fabs(wet / r_full - 1.0) < 0.03, "1 kHz through the FIR and the delay line: %.3f of the dry level",
          wet / r_full);
    int16_t hi = 0;
    for (int i = 0; i < onset - 40; i++)
        hi = abs(big[2 * i]) > hi ? (int16_t)abs(big[2 * i]) : hi;
    CHECK(hi < 4, "nothing before the echo (peak %d)", hi);

    printf("%s (%d failure%s)\n", fails ? "FAILED" : "all passed", fails, fails == 1 ? "" : "s");
    return fails != 0;
}

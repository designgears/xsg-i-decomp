/*
 * MPEG-2 video decoder for the movies (see mpeg2dec.h).
 *
 * New code for this port, written from ISO/IEC 13818-2: the VLC tables
 * (annex B), scan orders, default matrix and non-linear quantiser scale are
 * transcribed from the standard (ipu.cpp holds the same facts for the IPU).
 * Nothing from FFmpeg, libmpeg2 or PCSX2 is used.
 *
 * It replaced an ffmpeg child process fed through pipes: that cost a timed
 * wait per slice of input, which Windows rounds up to its timer tick, and the
 * movies stuttered there (issue #3).
 */
#include "mpeg2dec.h"

#include <math.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "xsg_rt.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- tables (ISO/IEC 13818-2 annex B, section 7) ------------------------ */
typedef struct {
    const char *code;
    int a, b;
} vlc_def;

/* B.14 / B.15 (DCT coefficients): a = run, b = level (0 EOB, -1 escape). */
static const vlc_def B14[] = {
    {"10", 0, 0}, {"11", 0, 1}, {"011", 1, 1}, {"0100", 0, 2}, {"0101", 2, 1}, {"00101", 0, 3},
    {"00111", 3, 1}, {"00110", 4, 1}, {"000110", 1, 2}, {"000111", 5, 1}, {"000101", 6, 1},
    {"000100", 7, 1}, {"0000110", 0, 4}, {"0000100", 2, 2}, {"0000111", 8, 1}, {"0000101", 9, 1},
    {"000001", 0, -1}, {"00100110", 0, 5}, {"00100001", 0, 6}, {"00100101", 1, 3},
    {"00100100", 3, 2}, {"00100111", 10, 1}, {"00100011", 11, 1}, {"00100010", 12, 1},
    {"00100000", 13, 1}, {"0000001010", 0, 7}, {"0000001100", 1, 4}, {"0000001011", 2, 3},
    {"0000001111", 4, 2}, {"0000001001", 5, 2}, {"0000001110", 14, 1}, {"0000001101", 15, 1},
    {"0000001000", 16, 1},
    {"000000011101", 0, 8}, {"000000011000", 0, 9}, {"000000010011", 0, 10}, {"000000010000", 0, 11},
    {"000000011011", 1, 5}, {"000000010100", 2, 4}, {"000000011100", 3, 3}, {"000000010010", 4, 3},
    {"000000011110", 6, 2}, {"000000010101", 7, 2}, {"000000010001", 8, 2}, {"000000011111", 17, 1},
    {"000000011010", 18, 1}, {"000000011001", 19, 1}, {"000000010111", 20, 1}, {"000000010110", 21, 1},
    {NULL, 0, 0}};
static const vlc_def B15[] = {
    {"0110", 0, 0}, {"10", 0, 1}, {"010", 1, 1}, {"110", 0, 2}, {"00101", 2, 1}, {"0111", 0, 3},
    {"00111", 3, 1}, {"000110", 4, 1}, {"00110", 1, 2}, {"000111", 5, 1}, {"0000110", 6, 1},
    {"0000100", 7, 1}, {"11100", 0, 4}, {"0000111", 2, 2}, {"0000101", 8, 1}, {"1111000", 9, 1},
    {"000001", 0, -1}, {"11101", 0, 5}, {"000101", 0, 6}, {"1111001", 1, 3}, {"00100110", 3, 2},
    {"1111010", 10, 1}, {"00100001", 11, 1}, {"00100101", 12, 1}, {"00100100", 13, 1},
    {"000100", 0, 7}, {"00100111", 1, 4}, {"11111100", 2, 3}, {"11111101", 4, 2},
    {"000000100", 5, 2}, {"000000101", 14, 1}, {"000000111", 15, 1}, {"0000001101", 16, 1},
    {"1111011", 0, 8}, {"1111100", 0, 9}, {"00100011", 0, 10}, {"00100010", 0, 11},
    {"00100000", 1, 5}, {"0000001100", 2, 4},
    {"000000011100", 3, 3}, {"000000010010", 4, 3}, {"000000011110", 6, 2}, {"000000010101", 7, 2},
    {"000000010001", 8, 2}, {"000000011111", 17, 1}, {"000000011010", 18, 1}, {"000000011001", 19, 1},
    {"000000010111", 20, 1}, {"000000010110", 21, 1},
    {"11111010", 0, 12}, {"11111011", 0, 13}, {"11111110", 0, 14}, {"11111111", 0, 15},
    {NULL, 0, 0}};
/* 13..16-bit codes, shared by both. */
static const vlc_def BLONG[] = {
    {"0000000011010", 0, 12}, {"0000000011001", 0, 13}, {"0000000011000", 0, 14}, {"0000000010111", 0, 15},
    {"0000000010110", 1, 6}, {"0000000010101", 1, 7}, {"0000000010100", 2, 5}, {"0000000010011", 3, 4},
    {"0000000010010", 5, 3}, {"0000000010001", 9, 2}, {"0000000010000", 10, 2}, {"0000000011111", 22, 1},
    {"0000000011110", 23, 1}, {"0000000011101", 24, 1}, {"0000000011100", 25, 1}, {"0000000011011", 26, 1},
    {"00000000011111", 0, 16}, {"00000000011110", 0, 17}, {"00000000011101", 0, 18}, {"00000000011100", 0, 19},
    {"00000000011011", 0, 20}, {"00000000011010", 0, 21}, {"00000000011001", 0, 22}, {"00000000011000", 0, 23},
    {"00000000010111", 0, 24}, {"00000000010110", 0, 25}, {"00000000010101", 0, 26}, {"00000000010100", 0, 27},
    {"00000000010011", 0, 28}, {"00000000010010", 0, 29}, {"00000000010001", 0, 30}, {"00000000010000", 0, 31},
    {"000000000011000", 0, 32}, {"000000000010111", 0, 33}, {"000000000010110", 0, 34}, {"000000000010101", 0, 35},
    {"000000000010100", 0, 36}, {"000000000010011", 0, 37}, {"000000000010010", 0, 38}, {"000000000010001", 0, 39},
    {"000000000010000", 0, 40}, {"000000000011111", 1, 8}, {"000000000011110", 1, 9}, {"000000000011101", 1, 10},
    {"000000000011100", 1, 11}, {"000000000011011", 1, 12}, {"000000000011010", 1, 13}, {"000000000011001", 1, 14},
    {"0000000000010011", 1, 15}, {"0000000000010010", 1, 16}, {"0000000000010001", 1, 17}, {"0000000000010000", 1, 18},
    {"0000000000010100", 6, 3}, {"0000000000011010", 11, 2}, {"0000000000011001", 12, 2}, {"0000000000011000", 13, 2},
    {"0000000000010111", 14, 2}, {"0000000000010110", 15, 2}, {"0000000000010101", 16, 2}, {"0000000000011111", 27, 1},
    {"0000000000011110", 28, 1}, {"0000000000011101", 29, 1}, {"0000000000011100", 30, 1}, {"0000000000011011", 31, 1},
    {NULL, 0, 0}};

/* B.1 macroblock_address_increment (a); 34 = escape (+33), 35 = stuffing. */
static const vlc_def B1[] = {
    {"1", 1, 0}, {"011", 2, 0}, {"010", 3, 0}, {"0011", 4, 0}, {"0010", 5, 0}, {"00011", 6, 0},
    {"00010", 7, 0}, {"0000111", 8, 0}, {"0000110", 9, 0}, {"00001011", 10, 0}, {"00001010", 11, 0},
    {"00001001", 12, 0}, {"00001000", 13, 0}, {"00000111", 14, 0}, {"00000110", 15, 0},
    {"0000010111", 16, 0}, {"0000010110", 17, 0}, {"0000010101", 18, 0}, {"0000010100", 19, 0},
    {"0000010011", 20, 0}, {"0000010010", 21, 0}, {"00000100011", 22, 0}, {"00000100010", 23, 0},
    {"00000100001", 24, 0}, {"00000100000", 25, 0}, {"00000011111", 26, 0}, {"00000011110", 27, 0},
    {"00000011101", 28, 0}, {"00000011100", 29, 0}, {"00000011011", 30, 0}, {"00000011010", 31, 0},
    {"00000011001", 32, 0}, {"00000011000", 33, 0}, {"00000001000", 34, 0}, {"00000001111", 35, 0},
    {NULL, 0, 0}};

/* macroblock_type flags */
enum { MB_QUANT = 1, MB_FWD = 2, MB_BWD = 4, MB_PAT = 8, MB_INTRA = 16 };
static const vlc_def B2[] = {{"1", MB_INTRA, 0}, {"01", MB_INTRA | MB_QUANT, 0}, {NULL, 0, 0}};
static const vlc_def B3[] = {
    {"1", MB_FWD | MB_PAT, 0}, {"01", MB_PAT, 0}, {"001", MB_FWD, 0}, {"00011", MB_INTRA, 0},
    {"00010", MB_FWD | MB_PAT | MB_QUANT, 0}, {"00001", MB_PAT | MB_QUANT, 0},
    {"000001", MB_INTRA | MB_QUANT, 0}, {NULL, 0, 0}};
static const vlc_def B4[] = {
    {"10", MB_FWD | MB_BWD, 0}, {"11", MB_FWD | MB_BWD | MB_PAT, 0}, {"010", MB_BWD, 0},
    {"011", MB_BWD | MB_PAT, 0}, {"0010", MB_FWD, 0}, {"0011", MB_FWD | MB_PAT, 0},
    {"00011", MB_INTRA, 0}, {"00010", MB_FWD | MB_BWD | MB_PAT | MB_QUANT, 0},
    {"000011", MB_FWD | MB_PAT | MB_QUANT, 0}, {"000010", MB_BWD | MB_PAT | MB_QUANT, 0},
    {"000001", MB_INTRA | MB_QUANT, 0}, {NULL, 0, 0}};

/* B.9 coded_block_pattern (4:2:0) */
static const vlc_def B9[] = {
    {"111", 60, 0}, {"1101", 4, 0}, {"1100", 8, 0}, {"1011", 16, 0}, {"1010", 32, 0},
    {"10011", 12, 0}, {"10010", 48, 0}, {"10001", 20, 0}, {"10000", 40, 0}, {"01111", 28, 0},
    {"01110", 44, 0}, {"01101", 52, 0}, {"01100", 56, 0}, {"01011", 1, 0}, {"01010", 61, 0},
    {"01001", 2, 0}, {"01000", 62, 0}, {"001111", 24, 0}, {"001110", 36, 0}, {"001101", 3, 0},
    {"001100", 63, 0}, {"0010111", 5, 0}, {"0010110", 9, 0}, {"0010101", 17, 0}, {"0010100", 33, 0},
    {"0010011", 6, 0}, {"0010010", 10, 0}, {"0010001", 18, 0}, {"0010000", 34, 0},
    {"00011111", 7, 0}, {"00011110", 11, 0}, {"00011101", 19, 0}, {"00011100", 35, 0},
    {"00011011", 13, 0}, {"00011010", 49, 0}, {"00011001", 21, 0}, {"00011000", 41, 0},
    {"00010111", 14, 0}, {"00010110", 50, 0}, {"00010101", 22, 0}, {"00010100", 42, 0},
    {"00010011", 15, 0}, {"00010010", 51, 0}, {"00010001", 23, 0}, {"00010000", 43, 0},
    {"00001111", 25, 0}, {"00001110", 37, 0}, {"00001101", 26, 0}, {"00001100", 38, 0},
    {"00001011", 29, 0}, {"00001010", 45, 0}, {"00001001", 53, 0}, {"00001000", 57, 0},
    {"00000111", 30, 0}, {"00000110", 46, 0}, {"00000101", 54, 0}, {"00000100", 58, 0},
    {"000000111", 31, 0}, {"000000110", 47, 0}, {"000000101", 55, 0}, {"000000100", 59, 0},
    {"000000011", 27, 0}, {"000000010", 39, 0}, {"000000001", 0, 0}, {NULL, 0, 0}};

/* B.10 motion_code magnitude (the sign bit follows, except for 0) */
static const vlc_def B10[] = {
    {"1", 0, 0}, {"01", 1, 0}, {"001", 2, 0}, {"0001", 3, 0}, {"000011", 4, 0}, {"0000101", 5, 0},
    {"0000100", 6, 0}, {"0000011", 7, 0}, {"000001011", 8, 0}, {"000001010", 9, 0},
    {"000001001", 10, 0}, {"0000010001", 11, 0}, {"0000010000", 12, 0}, {"0000001111", 13, 0},
    {"0000001110", 14, 0}, {"0000001101", 15, 0}, {"0000001100", 16, 0}, {NULL, 0, 0}};

static const uint8_t ZIGZAG[64] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
                                   12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
                                   35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
                                   58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};
static const uint8_t ALTERNATE[64] = {0,  8,  16, 24, 1,  9,  2,  10, 17, 25, 32, 40, 48, 56, 57, 49,
                                      41, 33, 26, 18, 3,  11, 4,  12, 19, 27, 34, 42, 50, 58, 35, 43,
                                      51, 59, 20, 28, 5,  13, 6,  14, 21, 29, 36, 44, 52, 60, 37, 45,
                                      53, 61, 22, 30, 7,  15, 23, 31, 38, 46, 54, 62, 39, 47, 55, 63};
static const uint8_t DEFAULT_INTRA[64] = {8,  16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37,
                                          19, 22, 26, 27, 29, 34, 34, 38, 22, 22, 26, 27, 29, 34, 37, 40,
                                          22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32, 35, 40, 48, 58,
                                          26, 27, 29, 34, 38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83};
static const uint8_t NONLINEAR_Q[32] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  10, 12, 14, 16, 18,  20,  22,
                                        24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88, 96, 104, 112};

/* Lookup tables indexed by the next `bits` bits: entry = len << 16 | value
 * (value biased by +0x8000 so it may be negative); 0 = invalid. */
typedef struct {
    int bits;
    uint32_t *lut;
} vlc_lut;

static vlc_lut T_AC14, T_AC15, T_MBA, T_MBI, T_MBP, T_MBB, T_CBP, T_MV;
static float IDCT_C[8][8];
static int g_tables_ready;

static void lut_add(vlc_lut *t, const vlc_def *v, int ac)
{
    for (; v->code; v++) {
        uint32_t len = (uint32_t)strlen(v->code), code = 0;
        for (uint32_t i = 0; i < len; i++)
            code = code * 2 + (uint32_t)(v->code[i] - '0');
        uint32_t val = ac ? (uint32_t)(v->a << 8 | (v->b < 0 ? 0xff : v->b)) : (uint32_t)(v->a + 0x8000);
        uint32_t base = code << (t->bits - len), n = 1u << (t->bits - len);
        for (uint32_t i = 0; i < n; i++)
            t->lut[base + i] = len << 16 | val;
    }
}

static void lut_build(vlc_lut *t, int bits, const vlc_def *a, const vlc_def *b, int ac)
{
    t->bits = bits;
    t->lut = calloc((size_t)1 << bits, sizeof *t->lut);
    lut_add(t, a, ac);
    if (b)
        lut_add(t, b, ac);
}

static void tables_init(void)
{
    if (g_tables_ready)
        return;
    lut_build(&T_AC14, 16, B14, BLONG, 1);
    lut_build(&T_AC15, 16, B15, BLONG, 1);
    lut_build(&T_MBA, 11, B1, NULL, 0);
    lut_build(&T_MBI, 2, B2, NULL, 0);
    lut_build(&T_MBP, 6, B3, NULL, 0);
    lut_build(&T_MBB, 6, B4, NULL, 0);
    lut_build(&T_CBP, 9, B9, NULL, 0);
    lut_build(&T_MV, 10, B10, NULL, 0);
    for (int k = 0; k < 8; k++)
        for (int n = 0; n < 8; n++)
            IDCT_C[k][n] = (float)((k == 0 ? sqrt(0.125) : 0.5) * cos((2 * n + 1) * k * M_PI / 16));
    g_tables_ready = 1;
}

/* ---- decoder state ------------------------------------------------------ */
typedef struct {
    uint8_t *p[3]; /* Y, Cb, Cr */
} frame;

typedef struct {
    uint8_t *rgba;
    int type;
} outpic;

struct mpeg2dec {
    uint8_t *buf; /* elementary stream not yet decoded; 16 zero bytes after len */
    size_t len, cap, pos, scan;
    int ended, done;

    int w, h, mbw, mbh, cw, ch; /* coded sizes are mbw*16 x mbh*16 */
    int rate_num, rate_den;
    uint8_t q_intra[64], q_nonintra[64]; /* raster order */
    int seq_ok, warned;

    /* picture */
    int in_pic, type, f_code[2][2], dcprec, structure, fpfd, conceal, qst, ivlc, altscan, skip_pic;
    frame fr[3];
    int a_old, a_new, bbuf, cur; /* buffer indices */
    int anchors;                 /* anchors decoded so far in this sequence (capped at 2) */
    int pending;                 /* an anchor waiting for its display slot */
    int pending_type;

    /* output queue */
    outpic q[8];
    int qhead, qn;

    /* slice / macroblock */
    const uint8_t *bs;
    size_t bs_bits, bitpos;
    jmp_buf err;
    int qscale_code, dc_pred[3], pmv[2][2][2];
    int last_flags, last_motion_type, last_mv[2][2][2], last_fs[2][2];
};

/* ---- bits ---- */
static inline uint32_t peek(mpeg2dec *d, int n)
{
    const uint8_t *p = d->bs + (d->bitpos >> 3);
    uint64_t v = (uint64_t)p[0] << 32 | (uint64_t)p[1] << 24 | (uint64_t)p[2] << 16 | (uint64_t)p[3] << 8 | p[4];
    return (uint32_t)(v << (24 + (d->bitpos & 7)) >> (64 - n)) & (n == 32 ? 0xffffffffu : ((1u << n) - 1));
}
static inline void skipb(mpeg2dec *d, int n)
{
    d->bitpos += (size_t)n;
    if (d->bitpos > d->bs_bits + 32)
        longjmp(d->err, 1);
}
static inline uint32_t getb(mpeg2dec *d, int n)
{
    uint32_t v = peek(d, n);
    skipb(d, n);
    return v;
}
static inline int vlc(mpeg2dec *d, const vlc_lut *t)
{
    uint32_t e = t->lut[peek(d, t->bits)];
    if (!e)
        longjmp(d->err, 2);
    skipb(d, (int)(e >> 16));
    return (int)(e & 0xffff) - 0x8000;
}

/* ---- frames / output ---- */
static void frames_free(mpeg2dec *d)
{
    for (int i = 0; i < 3; i++)
        free(d->fr[i].p[0]);
    memset(d->fr, 0, sizeof d->fr);
}

static void frames_alloc(mpeg2dec *d)
{
    frames_free(d);
    size_t ys = (size_t)d->mbw * 16 * d->mbh * 16, cs = ys / 4;
    for (int i = 0; i < 3; i++) {
        uint8_t *m = malloc(ys + 2 * cs);
        memset(m, 16, ys);
        memset(m + ys, 128, 2 * cs);
        d->fr[i].p[0] = m;
        d->fr[i].p[1] = m + ys;
        d->fr[i].p[2] = m + ys + cs;
    }
}

static inline uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

/* BT.601 studio range to RGB, chroma sample shared by its 2x2 pixels. */
static void emit(mpeg2dec *d, int idx, int type)
{
    if (d->qn == (int)(sizeof d->q / sizeof d->q[0]))
        return; /* the reader is far behind: drop (does not happen with mpeg.c) */
    outpic *o = &d->q[(d->qhead + d->qn) % 8];
    if (!o->rgba)
        o->rgba = malloc((size_t)d->w * d->h * 4);
    o->type = type;
    const frame *f = &d->fr[idx];
    const int ys = d->mbw * 16, cs = ys / 2;
    for (int y = 0; y < d->h; y++) {
        const uint8_t *Y = f->p[0] + (size_t)y * ys;
        const uint8_t *U = f->p[1] + (size_t)(y >> 1) * cs, *V = f->p[2] + (size_t)(y >> 1) * cs;
        uint8_t *out = o->rgba + (size_t)y * d->w * 4;
        for (int x = 0; x < d->w; x++) {
            int c = 298 * (Y[x] - 16), u = U[x >> 1] - 128, v = V[x >> 1] - 128;
            out[0] = clamp8((c + 409 * v + 128) >> 8);
            out[1] = clamp8((c - 100 * u - 208 * v + 128) >> 8);
            out[2] = clamp8((c + 516 * u + 128) >> 8);
            out[3] = 0x80;
            out += 4;
        }
    }
    d->qn++;
}

static void flush_anchor(mpeg2dec *d)
{
    if (d->pending) {
        emit(d, d->a_new, d->pending_type);
        d->pending = 0;
    }
}

static void finish_picture(mpeg2dec *d)
{
    if (!d->in_pic)
        return;
    d->in_pic = 0;
    if (d->skip_pic)
        return;
    if (d->type == 3) {
        emit(d, d->bbuf, 3);
    } else {
        d->pending = 1;
        d->pending_type = d->type;
    }
}

/* ---- IDCT (separable, float; IEEE 1180 accuracy) ---- */
static void idct(const int in[64], int out[64], int last)
{
    if (last == 0) { /* DC only */
        int v = (int)lrintf((float)in[0] * 0.125f);
        v = v < -256 ? -256 : v > 255 ? 255 : v;
        for (int i = 0; i < 64; i++)
            out[i] = v;
        return;
    }
    float tmp[64];
    for (int y = 0; y < 8; y++) {
        const int *r = in + y * 8;
        if (!(r[0] | r[1] | r[2] | r[3] | r[4] | r[5] | r[6] | r[7])) {
            for (int x = 0; x < 8; x++)
                tmp[y * 8 + x] = 0;
            continue;
        }
        for (int x = 0; x < 8; x++) {
            float s = 0;
            for (int u = 0; u < 8; u++)
                s += IDCT_C[u][x] * (float)r[u];
            tmp[y * 8 + x] = s;
        }
    }
    for (int x = 0; x < 8; x++)
        for (int y = 0; y < 8; y++) {
            float s = 0;
            for (int v = 0; v < 8; v++)
                s += IDCT_C[v][y] * tmp[v * 8 + x];
            int i = (int)lrintf(s);
            out[y * 8 + x] = i < -256 ? -256 : i > 255 ? 255 : i;
        }
}

/* ---- blocks ---- */
static int dc_size(mpeg2dec *d, int luma)
{
    if (luma) {
        uint32_t b = peek(d, 9);
        if ((b >> 7) == 0) { skipb(d, 2); return 1; }
        if ((b >> 7) == 1) { skipb(d, 2); return 2; }
        if ((b >> 6) == 4) { skipb(d, 3); return 0; }
        if ((b >> 6) == 5) { skipb(d, 3); return 3; }
        if ((b >> 6) == 6) { skipb(d, 3); return 4; }
        int ones = 3;
        while (ones < 9 && ((b >> (8 - ones)) & 1))
            ones++;
        if (ones == 9) { skipb(d, 9); return 11; }
        skipb(d, ones + 1);
        return ones + 2;
    }
    uint32_t b = peek(d, 10);
    if ((b >> 8) == 0) { skipb(d, 2); return 0; }
    if ((b >> 8) == 1) { skipb(d, 2); return 1; }
    if ((b >> 8) == 2) { skipb(d, 2); return 2; }
    int ones = 2;
    while (ones < 10 && ((b >> (9 - ones)) & 1))
        ones++;
    if (ones == 10) { skipb(d, 10); return 11; }
    skipb(d, ones + 1);
    return ones + 1;
}

/* Decode and dequantise one block into F (raster order); returns the highest
 * scan index set (-1 none, 0 DC only). */
static int block(mpeg2dec *d, int intra, int comp, int qscale, int F[64])
{
    memset(F, 0, 64 * sizeof(int));
    const uint8_t *scan = d->altscan ? ALTERNATE : ZIGZAG;
    const uint8_t *W = intra ? d->q_intra : d->q_nonintra;
    const vlc_lut *t = intra && d->ivlc ? &T_AC15 : &T_AC14;
    int i, sum = 0, last = -1;
    if (intra) {
        int sz = dc_size(d, comp == 0), diff = 0;
        if (sz) {
            uint32_t v = getb(d, sz);
            diff = (v & (1u << (sz - 1))) ? (int)v : (int)v - (int)((1u << sz) - 1);
        }
        d->dc_pred[comp] += diff;
        F[0] = d->dc_pred[comp] << (3 - d->dcprec);
        sum = F[0];
        last = 0;
        i = 0;
    } else {
        i = -1;
        if (peek(d, 1)) { /* first coefficient: "1s" is run 0, level +-1 */
            skipb(d, 1);
            int level = getb(d, 1) ? -1 : 1;
            i = 0;
            int pos = scan[0];
            int f = ((2 * level + (level > 0 ? 1 : -1)) * W[pos] * qscale) / 32;
            F[pos] = f;
            sum += f;
            last = pos ? 1 : 0;
        }
    }
    for (;;) {
        uint32_t e = t->lut[peek(d, 16)];
        if (!e)
            longjmp(d->err, 3);
        uint32_t len = e >> 16, run = (e >> 8) & 0xff, lvl = e & 0xff;
        int level;
        if (lvl == 0) {
            skipb(d, (int)len);
            break;
        }
        if (lvl == 0xff) {
            skipb(d, 6);
            run = getb(d, 6);
            int l = (int)getb(d, 12);
            level = l >= 2048 ? l - 4096 : l;
            if (!level)
                longjmp(d->err, 4);
        } else {
            skipb(d, (int)len);
            level = getb(d, 1) ? -(int)lvl : (int)lvl;
        }
        i += (int)run + 1;
        if (i > 63)
            longjmp(d->err, 5);
        int pos = scan[i], f;
        if (intra)
            f = (level * W[pos] * qscale * 2) / 32;
        else
            f = ((2 * level + (level > 0 ? 1 : -1)) * W[pos] * qscale) / 32;
        f = f > 2047 ? 2047 : f < -2048 ? -2048 : f;
        F[pos] = f;
        sum += f;
        last = 1;
    }
    if (last >= 0 && !(sum & 1))
        F[63] ^= 1;
    if (last == 0 && F[63])
        last = 1;
    if (last > 0 && !(F[1] | F[63])) { /* DC-only shortcut needs every AC zero */
        int any = 0;
        for (int k = 1; k < 64 && !any; k++)
            any = F[k];
        if (!any)
            last = 0;
    }
    return last;
}

/* ---- motion ---- */
static int motion_delta(mpeg2dec *d, int fcode)
{
    int code = vlc(d, &T_MV);
    if (code && getb(d, 1))
        code = -code;
    int r = fcode - 1;
    if (r == 0 || code == 0)
        return code;
    int res = (int)getb(d, r);
    int delta = ((abs(code) - 1) << r) + res + 1;
    return code < 0 ? -delta : delta;
}

static int wrap(int v, int fcode)
{
    int r = fcode - 1, low = -(16 << r), high = (16 << r) - 1, range = 32 << r;
    if (v < low)
        v += range;
    else if (v > high)
        v -= range;
    return v;
}

/* motion_vectors(s) for a frame picture; fills mv[s] / fs[s]. */
static void read_mvs(mpeg2dec *d, int s, int motion_type, int mv[2][2][2], int fs[2][2])
{
    if (motion_type == 2) { /* frame */
        for (int t = 0; t < 2; t++) {
            int v = wrap(d->pmv[0][s][t] + motion_delta(d, d->f_code[s][t]), d->f_code[s][t]);
            d->pmv[0][s][t] = d->pmv[1][s][t] = v;
            mv[0][s][t] = v;
        }
        fs[0][s] = 0;
    } else if (motion_type == 1) { /* field */
        for (int r = 0; r < 2; r++) {
            fs[r][s] = (int)getb(d, 1);
            int v = wrap(d->pmv[r][s][0] + motion_delta(d, d->f_code[s][0]), d->f_code[s][0]);
            d->pmv[r][s][0] = v;
            mv[r][s][0] = v;
            v = wrap((d->pmv[r][s][1] >> 1) + motion_delta(d, d->f_code[s][1]), d->f_code[s][1]);
            d->pmv[r][s][1] = v * 2;
            mv[r][s][1] = v;
        }
    } else {
        if (!d->warned++)
            xsg_logf("[mpeg2] dual-prime motion is not supported; picture skipped\n");
        longjmp(d->err, 6);
    }
}

/* Predict a w x h block at half-pel position (hx, hy) of a plane. */
static void predict(const uint8_t *ref, int stride, int pw, int ph, int hx, int hy, int w, int h, uint8_t *dst,
                    int dstride, int avg)
{
    int x0 = hx >> 1, y0 = hy >> 1, fx = hx & 1, fy = hy & 1;
    int inside = x0 >= 0 && y0 >= 0 && x0 + w + fx <= pw && y0 + h + fy <= ph;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int a, b, c, e;
            if (inside) {
                const uint8_t *p = ref + (y0 + y) * stride + x0 + x;
                a = p[0];
                b = fx ? p[1] : a;
                c = fy ? p[stride] : a;
                e = fx && fy ? p[stride + 1] : fx ? b : c;
            } else {
#define PX(X, Y) ref[((Y) < 0 ? 0 : (Y) >= ph ? ph - 1 : (Y)) * stride + ((X) < 0 ? 0 : (X) >= pw ? pw - 1 : (X))]
                int X = x0 + x, Y = y0 + y;
                a = PX(X, Y);
                b = fx ? PX(X + 1, Y) : a;
                c = fy ? PX(X, Y + 1) : a;
                e = fx && fy ? PX(X + 1, Y + 1) : fx ? b : c;
#undef PX
            }
            int v;
            if (fx && fy)
                v = (a + b + c + e + 2) >> 2;
            else if (fx)
                v = (a + b + 1) >> 1;
            else if (fy)
                v = (a + c + 1) >> 1;
            else
                v = a;
            uint8_t *o = dst + y * dstride + x;
            *o = avg ? (uint8_t)((*o + v + 1) >> 1) : (uint8_t)v;
        }
    }
}

/* Form the prediction of macroblock (mbx, mby) from reference ref for direction s. */
static void mc(mpeg2dec *d, const frame *ref, int mbx, int mby, int motion_type, int mv[2][2][2], int fs[2][2],
               int s, uint8_t *py, uint8_t *pu, uint8_t *pv, int avg)
{
    const int W = d->mbw * 16, H = d->mbh * 16, CW = W / 2, CH = H / 2;
    if (motion_type == 2) {
        int vx = mv[0][s][0], vy = mv[0][s][1];
        predict(ref->p[0], W, W, H, mbx * 32 + vx, mby * 32 + vy, 16, 16, py, 16, avg);
        int cx = vx / 2, cy = vy / 2;
        predict(ref->p[1], CW, CW, CH, mbx * 16 + cx, mby * 16 + cy, 8, 8, pu, 8, avg);
        predict(ref->p[2], CW, CW, CH, mbx * 16 + cx, mby * 16 + cy, 8, 8, pv, 8, avg);
    } else { /* field prediction: r = 0 the top field's lines, 1 the bottom's */
        for (int r = 0; r < 2; r++) {
            int vx = mv[r][s][0], vy = mv[r][s][1], f = fs[r][s];
            predict(ref->p[0] + f * W, W * 2, W, H / 2, mbx * 32 + vx, mby * 16 + vy, 16, 8, py + r * 16, 32, avg);
            int cx = vx / 2, cy = vy / 2;
            predict(ref->p[1] + f * CW, CW * 2, CW, CH / 2, mbx * 16 + cx, mby * 8 + cy, 8, 4, pu + r * 8, 16, avg);
            predict(ref->p[2] + f * CW, CW * 2, CW, CH / 2, mbx * 16 + cx, mby * 8 + cy, 8, 4, pv + r * 8, 16, avg);
        }
    }
}

/* ---- macroblocks ---- */
static void put_block(mpeg2dec *d, int b, int mbx, int mby, int dct_type, const int *res, const uint8_t *pred,
                      int add)
{
    frame *f = &d->fr[d->cur];
    const int W = d->mbw * 16, CW = W / 2;
    uint8_t *dst;
    int stride;
    const uint8_t *p;
    int pstride;
    if (b < 4) {
        int bx = (b & 1) * 8;
        if (dct_type) {
            dst = f->p[0] + (size_t)(mby * 16 + (b >> 1)) * W + mbx * 16 + bx;
            stride = W * 2;
            p = pred + (b >> 1) * 16 + bx;
            pstride = 32;
        } else {
            dst = f->p[0] + (size_t)(mby * 16 + (b >> 1) * 8) * W + mbx * 16 + bx;
            stride = W;
            p = pred + (b >> 1) * 8 * 16 + bx;
            pstride = 16;
        }
    } else {
        dst = f->p[b - 3] + (size_t)mby * 8 * CW + mbx * 8;
        stride = CW;
        p = pred;
        pstride = 8;
    }
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            dst[y * stride + x] = add ? clamp8(p[y * pstride + x] + (res ? res[y * 8 + x] : 0))
                                      : clamp8(res[y * 8 + x]);
}

static void predicted_mb(mpeg2dec *d, int mbx, int mby, int flags, int motion_type, int mv[2][2][2], int fs[2][2],
                         int cbp, int dct_type, int qscale)
{
    uint8_t py[256], pu[64], pv[64];
    int s_fwd = flags & MB_FWD, s_bwd = flags & MB_BWD;
    const frame *fwd = &d->fr[d->a_old]; /* picture_header made the new anchor a_new */
    const frame *bwd = &d->fr[d->a_new];
    if (d->type == 2 || s_fwd)
        mc(d, fwd, mbx, mby, motion_type, mv, fs, 0, py, pu, pv, 0);
    if (s_bwd)
        mc(d, bwd, mbx, mby, motion_type, mv, fs, 1, py, pu, pv, s_fwd);
    int F[64], R[64];
    for (int b = 0; b < 6; b++) {
        const uint8_t *pred = b < 4 ? py : b == 4 ? pu : pv;
        if (cbp & (32 >> b)) {
            int last = block(d, 0, 0, qscale, F);
            idct(F, R, last);
            put_block(d, b, mbx, mby, dct_type, R, pred, 1);
        } else {
            put_block(d, b, mbx, mby, dct_type, NULL, pred, 1);
        }
    }
}

static void macroblock(mpeg2dec *d, int addr)
{
    int mbx = addr % d->mbw, mby = addr / d->mbw;
    if (mby >= d->mbh)
        longjmp(d->err, 7);
    const vlc_lut *tt = d->type == 1 ? &T_MBI : d->type == 2 ? &T_MBP : &T_MBB;
    int flags = vlc(d, tt);
    int motion_type = 2, dct_type = 0;
    if (!d->fpfd && (flags & (MB_FWD | MB_BWD)))
        motion_type = (int)getb(d, 2);
    if (!d->fpfd && (flags & (MB_INTRA | MB_PAT)))
        dct_type = (int)getb(d, 1);
    if (flags & MB_QUANT)
        d->qscale_code = (int)getb(d, 5);
    int qscale = d->qst ? NONLINEAR_Q[d->qscale_code] : d->qscale_code * 2;
    int mv[2][2][2] = {{{0}}}, fs[2][2] = {{0}};
    if (flags & MB_INTRA) {
        if (d->conceal) {
            read_mvs(d, 0, 2, mv, fs);
            skipb(d, 1); /* marker */
        } else {
            memset(d->pmv, 0, sizeof d->pmv);
        }
        int F[64], R[64];
        for (int b = 0; b < 6; b++) {
            int last = block(d, 1, b < 4 ? 0 : b - 3, qscale, F);
            idct(F, R, last);
            put_block(d, b, mbx, mby, dct_type, R, NULL, 0);
        }
        d->last_flags = MB_INTRA;
        return;
    }
    d->dc_pred[0] = d->dc_pred[1] = d->dc_pred[2] = 128 << d->dcprec;
    if (flags & MB_FWD)
        read_mvs(d, 0, motion_type, mv, fs);
    if (flags & MB_BWD)
        read_mvs(d, 1, motion_type, mv, fs);
    if (d->type == 2 && !(flags & MB_FWD)) { /* P, no MC: zero vector, frame prediction */
        memset(d->pmv, 0, sizeof d->pmv);
        motion_type = 2;
    }
    int cbp = flags & MB_PAT ? vlc(d, &T_CBP) : 0;
    predicted_mb(d, mbx, mby, flags, motion_type, mv, fs, cbp, dct_type, qscale);
    d->last_flags = flags;
    d->last_motion_type = motion_type;
    memcpy(d->last_mv, mv, sizeof mv);
    memcpy(d->last_fs, fs, sizeof fs);
}

static void skipped_mb(mpeg2dec *d, int addr)
{
    int mbx = addr % d->mbw, mby = addr / d->mbw;
    if (mby >= d->mbh)
        longjmp(d->err, 8);
    d->dc_pred[0] = d->dc_pred[1] = d->dc_pred[2] = 128 << d->dcprec;
    int mv[2][2][2] = {{{0}}}, fs[2][2] = {{0}};
    if (d->type == 2) {
        memset(d->pmv, 0, sizeof d->pmv);
        predicted_mb(d, mbx, mby, MB_FWD, 2, mv, fs, 0, 0, 0);
    } else { /* B: the previous macroblock's prediction */
        if (d->last_flags & MB_INTRA)
            longjmp(d->err, 9);
        memcpy(mv, d->last_mv, sizeof mv);
        memcpy(fs, d->last_fs, sizeof fs);
        predicted_mb(d, mbx, mby, d->last_flags & (MB_FWD | MB_BWD), d->last_motion_type, mv, fs, 0, 0, 0);
    }
}

static int mba_increment(mpeg2dec *d)
{
    int inc = 0;
    for (;;) {
        int v = vlc(d, &T_MBA);
        if (v == 35)
            continue; /* stuffing */
        if (v == 34) {
            inc += 33;
            continue;
        }
        return inc + v;
    }
}

static void slice(mpeg2dec *d, int code, const uint8_t *p, size_t n)
{
    d->bs = p;
    d->bs_bits = n * 8;
    d->bitpos = 32;
    if (setjmp(d->err)) {
        if (d->warned++ < 8)
            xsg_logf("[mpeg2] bad slice %d in a type %d picture\n", code, d->type);
        return;
    }
    d->qscale_code = (int)getb(d, 5);
    if (peek(d, 1)) {
        skipb(d, 9);
        while (peek(d, 1))
            skipb(d, 9);
    }
    skipb(d, 1);
    d->dc_pred[0] = d->dc_pred[1] = d->dc_pred[2] = 128 << d->dcprec;
    memset(d->pmv, 0, sizeof d->pmv);
    d->last_flags = MB_INTRA;
    int addr = (code - 1) * d->mbw - 1 + mba_increment(d);
    for (;;) {
        macroblock(d, addr);
        /* the slice ends where 23 zero bits start (the next start code, or the buffer's padding) */
        if (d->bitpos >= d->bs_bits || peek(d, 23) == 0)
            return;
        int inc = mba_increment(d);
        for (int k = 1; k < inc; k++)
            skipped_mb(d, addr + k);
        addr += inc;
    }
}

/* ---- headers ---- */
static void load_matrix(mpeg2dec *d, uint8_t *q)
{
    for (int i = 0; i < 64; i++)
        q[ZIGZAG[i]] = (uint8_t)getb(d, 8);
}

static void headers(mpeg2dec *d, int code, const uint8_t *p, size_t n)
{
    static const int fr[9][2] = {{0, 1}, {24000, 1001}, {24, 1}, {25, 1}, {30000, 1001},
                                 {30, 1}, {50, 1}, {60000, 1001}, {60, 1}};
    d->bs = p;
    d->bs_bits = n * 8;
    d->bitpos = 32;
    if (setjmp(d->err))
        return;
    if (code == 0xb3) {
        int w = (int)getb(d, 12), h = (int)getb(d, 12);
        skipb(d, 4);
        int rc = (int)getb(d, 4);
        skipb(d, 18 + 1 + 10 + 1);
        if (rc >= 1 && rc <= 8) {
            d->rate_num = fr[rc][0];
            d->rate_den = fr[rc][1];
        }
        if (getb(d, 1))
            load_matrix(d, d->q_intra);
        else
            memcpy(d->q_intra, DEFAULT_INTRA, 64);
        if (getb(d, 1))
            load_matrix(d, d->q_nonintra);
        else
            memset(d->q_nonintra, 16, 64);
        if (w != d->w || h != d->h || !d->fr[0].p[0]) {
            d->w = w;
            d->h = h;
            d->mbw = (w + 15) / 16;
            d->mbh = (h + 15) / 16;
            frames_alloc(d);
            for (int i = 0; i < 8; i++) {
                free(d->q[i].rgba);
                d->q[i].rgba = NULL;
            }
            d->qn = 0;
            d->anchors = 0;
        }
        d->seq_ok = 1;
    } else if (code == 0xb5) {
        int id = (int)getb(d, 4);
        if (id == 1) { /* sequence extension */
            skipb(d, 8 + 1);
            int chroma = (int)getb(d, 2);
            if (chroma != 1 && !d->warned++)
                xsg_logf("[mpeg2] chroma format %d is not supported (4:2:0 only)\n", chroma);
        } else if (id == 3) { /* quant matrix extension */
            if (getb(d, 1))
                load_matrix(d, d->q_intra);
            if (getb(d, 1))
                load_matrix(d, d->q_nonintra);
        } else if (id == 8 && d->in_pic) { /* picture coding extension */
            d->f_code[0][0] = (int)getb(d, 4);
            d->f_code[0][1] = (int)getb(d, 4);
            d->f_code[1][0] = (int)getb(d, 4);
            d->f_code[1][1] = (int)getb(d, 4);
            d->dcprec = (int)getb(d, 2);
            d->structure = (int)getb(d, 2);
            skipb(d, 1); /* top_field_first */
            d->fpfd = (int)getb(d, 1);
            d->conceal = (int)getb(d, 1);
            d->qst = (int)getb(d, 1);
            d->ivlc = (int)getb(d, 1);
            d->altscan = (int)getb(d, 1);
            if (d->structure != 3) {
                if (!d->warned++)
                    xsg_logf("[mpeg2] field pictures are not supported; picture skipped\n");
                d->skip_pic = 1;
            }
        }
    }
}

static void picture_header(mpeg2dec *d, const uint8_t *p, size_t n)
{
    d->bs = p;
    d->bs_bits = n * 8;
    d->bitpos = 32;
    if (setjmp(d->err))
        return;
    skipb(d, 10);
    int type = (int)getb(d, 3);
    if (type < 1 || type > 3 || !d->seq_ok) {
        d->skip_pic = 1;
        d->in_pic = 1;
        return;
    }
    /* MPEG-1 style defaults until the picture coding extension */
    d->type = type;
    d->f_code[0][0] = d->f_code[0][1] = d->f_code[1][0] = d->f_code[1][1] = 1;
    d->dcprec = 0;
    d->structure = 3;
    d->fpfd = 1;
    d->conceal = d->qst = d->ivlc = d->altscan = 0;
    d->skip_pic = type == 2 && d->anchors < 1;
    d->skip_pic |= type == 3 && d->anchors < 2;
    if (type == 3) {
        d->cur = d->bbuf;
    } else {
        flush_anchor(d); /* the previous anchor's display slot comes before the B pictures that follow */
        int t = d->a_old;
        d->a_old = d->a_new;
        d->a_new = t;
        d->cur = t;
        if (d->anchors < 2)
            d->anchors++;
    }
    d->in_pic = 1;
}

/* Find the next start code at or after `from`; len when none. */
static size_t next_sc(const mpeg2dec *d, size_t from)
{
    const uint8_t *b = d->buf;
    for (size_t i = from; i + 3 < d->len; i++)
        if (b[i + 2] <= 1 && !b[i] && !b[i + 1] && b[i + 2] == 1)
            return i;
    return d->len;
}

/* Handle one complete unit; 0 when the next unit is not complete yet. */
static int step(mpeg2dec *d)
{
    size_t s = next_sc(d, d->pos);
    if (s == d->len) {
        if (d->ended) {
            d->pos = d->len;
            return 0;
        }
        d->pos = s > 3 ? s - 3 : 0;
        return 0;
    }
    size_t from = d->scan > s + 4 ? d->scan : s + 4;
    size_t e = next_sc(d, from);
    if (e == d->len && !d->ended) {
        d->pos = s;
        d->scan = e > 3 ? e - 3 : e;
        return 0;
    }
    d->scan = 0;
    const uint8_t *p = d->buf + s;
    size_t n = e - s;
    int code = p[3];
    d->pos = e;
    if (code == 0x00) {
        finish_picture(d);
        picture_header(d, p, n);
    } else if (code >= 0x01 && code <= 0xaf) {
        if (d->in_pic && !d->skip_pic)
            slice(d, code, p, n);
    } else if (code == 0xb3 || code == 0xb8) {
        finish_picture(d);
        if (code == 0xb3)
            headers(d, code, p, n);
    } else if (code == 0xb5) {
        headers(d, code, p, n);
    } else if (code == 0xb7) {
        finish_picture(d);
        flush_anchor(d);
        d->anchors = 0;
    }
    return 1;
}

static void compact(mpeg2dec *d)
{
    if (d->pos > (1u << 20) || (d->pos && d->pos == d->len)) {
        memmove(d->buf, d->buf + d->pos, d->len - d->pos);
        d->len -= d->pos;
        if (d->scan)
            d->scan = d->scan > d->pos ? d->scan - d->pos : 0;
        d->pos = 0;
        memset(d->buf + d->len, 0, 16);
    }
}

/* ---- API ---- */
mpeg2dec *mpeg2dec_new(void)
{
    tables_init();
    mpeg2dec *d = calloc(1, sizeof *d);
    d->a_old = 0;
    d->a_new = 1;
    d->bbuf = 2;
    d->rate_num = 30000;
    d->rate_den = 1001;
    memcpy(d->q_intra, DEFAULT_INTRA, 64);
    memset(d->q_nonintra, 16, 64);
    return d;
}

void mpeg2dec_free(mpeg2dec *d)
{
    if (!d)
        return;
    frames_free(d);
    for (int i = 0; i < 8; i++)
        free(d->q[i].rgba);
    free(d->buf);
    free(d);
}

void mpeg2dec_push(mpeg2dec *d, const uint8_t *p, size_t n)
{
    if (d->len + n + 16 > d->cap) {
        d->cap = (d->len + n + 16) * 2;
        d->buf = realloc(d->buf, d->cap);
    }
    memcpy(d->buf + d->len, p, n);
    d->len += n;
    memset(d->buf + d->len, 0, 16);
}

void mpeg2dec_end(mpeg2dec *d) { d->ended = 1; }

int mpeg2dec_picture(mpeg2dec *d, uint8_t *rgba, int *type)
{
    while (!d->qn) {
        if (d->done)
            return -1;
        if (!d->buf || !step(d)) {
            compact(d);
            if (!d->ended)
                return 0;
            finish_picture(d);
            flush_anchor(d);
            d->done = 1;
        }
    }
    outpic *o = &d->q[d->qhead];
    memcpy(rgba, o->rgba, (size_t)d->w * d->h * 4);
    *type = o->type;
    d->qhead = (d->qhead + 1) % 8;
    d->qn--;
    return 1;
}

int mpeg2dec_width(const mpeg2dec *d) { return d->w; }
int mpeg2dec_height(const mpeg2dec *d) { return d->h; }
void mpeg2dec_rate(const mpeg2dec *d, int *num, int *den)
{
    *num = d->rate_num;
    *den = d->rate_den;
}

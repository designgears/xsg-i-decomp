#include "common.h"
#include "shared.h"

/*
 * The JPEG work area the codec keeps behind the gp-relative `sw` pointer.
 * Only the two regions FFIDCT uses are recovered here: the 8x8 block that holds
 * the result of the first one-dimensional pass, and the quadword pair each pass
 * stages one line through on its way to and from VU0.  The area is quadword
 * aligned, which is what the lqc2/sqc2 pair in FFIDCT needs.  Everything before
 * those two regions belongs to the parts of the codec that are still assembler
 * and stays an opaque reserved span.
 */
typedef struct JpegWork {
    u8 reserved0[5232];
    float idct_block[8][8];
    float idct_stage[8];
} JpegWork;

extern JpegWork *sw;

INCLUDE_ASM("asm/main/nonmatchings/xgl_jpeg", ConvertYUV2MCU);

INCLUDE_ASM("asm/main/nonmatchings/xgl_jpeg", FFDCT);

extern int F2I(float value);

static void Quantize(int *quantized, const float *coefficients, const float *reciprocals)
{
    float coefficient;
    float reciprocal;
    float scaled;
    int index;

    for (index = 63; index >= 0; index--) {
        coefficient = *coefficients;
        coefficients++;
        reciprocal = *reciprocals;
        reciprocals++;
        scaled = coefficient * reciprocal;
        if (coefficient < 0.0f) {
            scaled -= 0.5f;
        } else {
            scaled += 0.5f;
        }
        *quantized = F2I(scaled);
        quantized++;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/xgl_jpeg", RebuildQuantizeTable);

typedef struct JpegPutBitsView {
    char pad[0x350];
    unsigned char *buffer;
    int position;
    signed char count;
    unsigned char byte;
} JpegPutBitsView;

static void PutBits(u16 code, int length)
{
    unsigned char *buffer = ((JpegPutBitsView *)sw)->buffer;
    int bits = code;

    if (length != 0) {
        bits &= (1 << length) - 1;
        while (length + ((JpegPutBitsView *)sw)->count >= 8) {
            ((JpegPutBitsView *)sw)->byte |= bits >> (length + ((JpegPutBitsView *)sw)->count - 8);
            buffer[((JpegPutBitsView *)sw)->position] = ((JpegPutBitsView *)sw)->byte;
            if (((JpegPutBitsView *)sw)->byte == 0xFF) {
                buffer[++((JpegPutBitsView *)sw)->position] = 0;
            }
            length = length + ((JpegPutBitsView *)sw)->count - 8;
            ((JpegPutBitsView *)sw)->position++;
            ((JpegPutBitsView *)sw)->count = 0;
            ((JpegPutBitsView *)sw)->byte = 0;
        }
        if (length > 0) {
            ((JpegPutBitsView *)sw)->byte |= bits << (8 - ((JpegPutBitsView *)sw)->count - length);
            ((JpegPutBitsView *)sw)->count += length;
        }
    }
}

typedef struct JpegHuffCodeDc {
    u16 code;
    s16 length;
} JpegHuffCodeDc;
static void PutBits(u16 code, int length);

static void EncodeDc(int value, JpegHuffCodeDc *table)
{
    int magnitude;
    int negative;
    int remaining;
    int length;
    u16 bits;

    magnitude = value;
    if (magnitude == 0) {
        PutBits(table[0].code, table[0].length);
        return;
    }

    negative = 0;
    if (magnitude < 0) {
        negative = 1;
        magnitude = -magnitude;
    }

    remaining = magnitude;
    length = 0;
    while (remaining != 0) {
        remaining >>= 1;
        length++;
    }

    PutBits(table[length].code, table[length].length);

    if (negative) {
        bits = (u16) (~magnitude & 0xFFFF);
    } else {
        bits = (u16) (magnitude & 0xFFFF);
    }
    PutBits(bits, length);
}

/*
 * A Huffman code table entry: the variable-length bit pattern PutBits emits
 * and its bit count. EncodeAc indexes such a table by (run << 4) + size, the
 * scaled run of preceding zero coefficients plus the bit length of the
 * nonzero coefficient that ends the run; a run alone (size 0) is the table's
 * zero-run control code (end-of-block or ZRL).
 */
typedef struct JpegHuffCode {
    u16 code;
    s16 length;
} JpegHuffCode;

static void PutBits(u16 code, int length);

static void EncodeAc(int value, int runIndex, JpegHuffCode *table)
{
    int magnitude;
    int negative;
    int remaining;
    int length;
    u16 bits;

    magnitude = value;
    if (magnitude == 0) {
        PutBits(table[runIndex].code, table[runIndex].length);
        return;
    }

    negative = 0;
    if (magnitude < 0) {
        negative = 1;
        magnitude = -magnitude;
    }

    remaining = magnitude;
    length = 0;
    while (remaining != 0) {
        remaining >>= 1;
        length++;
    }

    PutBits(table[length + runIndex].code, table[length + runIndex].length);

    if (negative) {
        bits = (u16) (~magnitude & 0xFFFF);
    } else {
        bits = (u16) (magnitude & 0xFFFF);
    }
    PutBits(bits, length);
}

extern unsigned char ZigZag[];

static void HuffmanEncode(short *coef, JpegHuffCodeDc *dc, JpegHuffCode *ac, u16 *prev)
{
    int run = 0;
    int i = 1;
    short v;
    short diff = *(u16 *)coef - *prev;
    *(int *)prev = *(int *)prev + diff;
    EncodeDc(diff, dc);
    for (;;) {
        v = coef[ZigZag[i] * 2];
        i++;
        if (v == 0) {
            if (i >= 64) {
                EncodeAc(0, 0, ac);
                break;
            }
            run = (short)(run + 1);
            continue;
        }
        while (run >= 16) {
            EncodeAc(0, 0xF0, ac);
            run = (short)(run - 16);
        }
        EncodeAc(v, run << 4, ac);
        if (i >= 64)
            break;
        run = 0;
    }
}

static void ExtractHuffmanTableSub(JpegHuffCode *table, u8 *data, int table_size)
{
    int code;
    int length;
    int count;
    int i;
    int temp;
    u8 *counts;
    u8 *symbols;
    u8 symbol;
    JpegHuffCode *entry;

    memset(table, 0, table_size * 4);
    symbols = data;
    counts = data;
    code = 0;
    length = 1;
    symbols += 0x10;
    i = 0xF;
    do {
        count = *counts;
        counts += 1;
        if (count-- > 0) {
            do {
                symbol = *symbols;
                symbols += 1;
                temp = count;
                count = count - 1;
                entry = (JpegHuffCode *)((int)table + (symbol << 2));
                entry->code = code;
                code = (code + 1) & 0xFFFF;
                entry->length = length;
            } while (temp > 0);
        }
        i -= 1;
        code = (code * 2) & 0xFFFE;
        length = (s16)(length + 1);
    } while (i >= 0);
}

INCLUDE_ASM("asm/main/nonmatchings/xgl_jpeg", xglJpegEncode);

/*
 * Parses a DRI (Define Restart Interval) marker segment: marker[0..1] is the
 * segment length, marker[2..3] the big-endian restart interval. Stores it and
 * that value plus one into the codec work area past the JpegWork extent
 * modeled above (lw/sw at main 0x002248ac/0x002248b4, +0x2ba0/+0x2ba4 of
 * `sw`); nothing in this allocation reads either field back, so their
 * consuming role is not evidenced here. Modeling them as named JpegWork
 * members needs a published extension of that struct (an
 * unmodeled_1590[0x1610] gap then two u32 members), which is a shared-header
 * change outside a single function's additive edit and is reported rather
 * than made here. Returns the byte after the segment, as the marker dispatch
 * table's other parsers do.
 */
static void *DefineRestartInterval(void *marker)
{
    u8 *segment = marker;
    u8 *work = (u8 *)sw;
    u32 interval;

    interval = (segment[2] << 8) + segment[3];
    *(u32 *)(work + 0x2ba0) = interval;
    *(u32 *)(work + 0x2ba4) = interval + 1;
    return segment + 4;
}

extern float I2F(int value);

/*
 * The luminance and chrominance quantization tables the segment's byte
 * coefficients are converted into lie inside the JpegWork work area at
 * fixed offsets from `sw`: table id 0 (luminance) at +0xD0, any other id
 * (chrominance) at +0x1D0. Both offsets fall inside the reserved0 span
 * documented above; naming them as JpegWork members would edit that
 * already-published struct, which is a shared-header change outside a
 * single function's additive edit and is reported rather than made here
 * (the same constraint DefineRestartInterval's own note records for its
 * fields).
 */
static u8 *DefineQuantizeTable(u8 *marker)
{
    u8 *segment;
    u16 count;
    float *dest;
    u8 value;

    segment = marker;
    count = (segment[0] << 8) + segment[1] - 3;
    if (segment[2] == 0) {
        dest = (float *)((u8 *)sw + 0xD0);
    } else {
        dest = (float *)((u8 *)sw + 0x1D0);
    }
    segment += 3;

    while (count != 0) {
        value = segment[0];
        segment += 1;
        *dest = I2F(value);
        dest += 1;
        count--;
    }
    return segment;
}

typedef struct SofView {
    u8 pad0[0x360];
    int mode;
    short height;
    short width;
    int pad368;
    int size;
} SofView;

static u8 *StartOfFrame(u8 *segment)
{
    SofView *v = (SofView *)sw;
    int w;
    int mode;

    v->width = segment[4] + (segment[3] << 8);
    w = segment[6] + (segment[5] << 8);
    v->height = w;
    v->size = (short)w * 3;
    switch (segment[9]) {
    case 0x11:
        v->mode = 0;
        break;
    case 0x21:
        v->mode = 1;
        break;
    case 0x22:
    default:
        v->mode = 2;
        break;
    }
    return segment + ((segment[0] << 8) + segment[1]);
}

typedef struct { unsigned short code; short val; } DhtEnt;

static u8 *DefineHuffmanTable(u8 *p)
{
    u8 *end = p + ((p[0] << 8) + p[1]);

    for (p += 2; p < end; ) {
        DhtEnt *t;
        u8 *counts;
        unsigned short code;
        int i;

        switch (*p) {
        case 0:
            t = (DhtEnt *)((u8 *)sw + 0x370);
            break;
        case 1:
            t = (DhtEnt *)((u8 *)sw + 0x3F0);
            break;
        case 0x10:
            t = (DhtEnt *)((u8 *)sw + 0x470);
            break;
        default:
            t = (DhtEnt *)((u8 *)sw + 0xC70);
            break;
        }
        p++;
        code = 0;
        counts = p;
        p += 16;
        for (i = 0; i < 16; i++) {
            int n = *counts++;
            while (n-- > 0) {
                t->code = code;
                code++;
                t->val = *p++;
                t++;
            }
            t->val = 0xFE;
            t++;
            code <<= 1;
        }
        t->val = 0xFF;
    }
    return p;
}

typedef struct { char pad[0x350]; unsigned char *ptr; int pad1; signed char cnt; unsigned char byte; char pad2[0x2e]; } JBits;
#define S ((JBits *)sw)

static int GetBit(void)
{
    JBits *w = S;
    unsigned char c;
    unsigned char *bp;

    if (w->cnt == 0) {
        c = *w->ptr;
        if (c == 0xFF)
            w->ptr++;
        bp = &w->byte;
        *bp = c;
        w->ptr++;
        S->cnt = 7;
    } else {
        w->cnt--;
    }
    return S->byte >> S->cnt & 1;
}

typedef struct { unsigned short code; short val; } HEnt;
extern int GetBit();

static short decode_sub(HEnt *p)
{
    int code = 0;

    for (;;) {
        code = code * 2 + GetBit();
        if (p->val == 0xFE)
            goto skip;
    chk:
        if (p->code == code)
            return p->val;
        p++;
        if (p->val != 0xFE)
            goto chk;
    skip:
        p++;
    }
}

static void HuffmanDecode(float *out, int comp)
{
    HEnt *dc;
    HEnt *ac;
    float *q;
    int *pred;
    int v;
    int k;
    int s;
    int m;
    int m2;

    if (comp != 0) {
        if (comp >= 0) {
            if (comp < 3) {
                goto good;
            }
        }
    }
    dc = (HEnt *)((u8 *)sw + 0x370);
    ac = (HEnt *)((u8 *)sw + 0x470);
    q = (float *)((u8 *)sw + 0xD0);
    goto join;
good:
    dc = (HEnt *)((u8 *)sw + 0x3F0);
    ac = (HEnt *)((u8 *)sw + 0xC70);
    q = (float *)((u8 *)sw + 0x1D0);
join:
    v = 0;
    pred = (int *)((u8 *)sw + comp * 4 + 0x1590);
    s = decode_sub(dc);
    if (s != 0) {
        if (GetBit() == 0) {
            m = (1 << s) - 1;
        } else {
            m = 0;
            v = 1;
        }
        for (s--; s > 0; s--) {
            v = v * 2 + GetBit();
        }
        v -= m;
    }
    v += *pred;
    *pred = v;
    out[0] = I2F(v) * q[0];
    k = 1;
    do {
        v = 0;
        s = decode_sub(ac);
        if (s != 0) {
            while (s >= 16) {
                out[ZigZag[k++]] = 0;
                s -= 16;
            }
            if (s != 0) {
                if (GetBit() == 0) {
                    v = 0;
                    m2 = (1 << s) - 1;
                } else {
                    m2 = 0;
                    v = 1;
                }
                for (s--; s > 0; s--) {
                    v = v * 2 + GetBit();
                }
                v -= m2;
            }
            out[ZigZag[k]] = I2F(v) * q[k];
        } else {
            while (k < 64) {
                out[ZigZag[k++]] = 0;
            }
        }
        k++;
    } while (k < 64);
}

/*
 * Two-dimensional inverse DCT of one 8x8 block of floating-point coefficients,
 * in place.
 *
 * StartOfScan leaves the one-dimensional transform in the VU0 register file and
 * it stays there for every block of the scan: vf1-vf8 for the first pass and
 * vf11-vf18, the same rows scaled through I, for the second.  A pass therefore
 * reduces to one matrix-by-vector product per line, which is the only part of
 * this routine that cannot be written in C.  The even-indexed samples of the
 * line go to vf9 and the odd-indexed ones to vf10, and the accumulator chain
 * multiplies them by the transform; what VU0 returns is the butterfly's four
 * sums and four differences, which the scalar code below combines into the
 * eight output samples.
 *
 * The first pass reads the columns of the caller's block and writes the rows of
 * the decoder's intermediate block; the second pass reads the columns of that
 * intermediate block and writes the rows of the caller's block back.
 */
static void FFIDCT(float block[8][8])
{
    JpegWork *work;
    float *column;
    float *stage;
    float *line;
    float *out;
    int i;

    stage = sw->idct_stage;

    for (i = 0; i < 8; i++) {
        stage[0] = block[0][i];
        stage[1] = block[2][i];
        stage[2] = block[4][i];
        stage[3] = block[6][i];
        stage[4] = block[1][i];
        stage[5] = block[3][i];
        stage[6] = block[5][i];
        stage[7] = block[7][i];
        __asm__ __volatile__(
            "lqc2 vf9,0(%0)\n"
            "lqc2 vf10,16(%0)\n"
            "vmulax.xyzw ACCxyzw,vf1xyzw,vf9x\n"
            "vmadday.xyzw ACCxyzw,vf2xyzw,vf9y\n"
            "vmaddaz.xyzw ACCxyzw,vf3xyzw,vf9z\n"
            "vmaddw.xyzw vf19xyzw,vf4xyzw,vf9w\n"
            "vmulax.xyzw ACCxyzw,vf5xyzw,vf10x\n"
            "vmadday.xyzw ACCxyzw,vf6xyzw,vf10y\n"
            "vmaddaz.xyzw ACCxyzw,vf7xyzw,vf10z\n"
            "vmaddw.xyzw vf20xyzw,vf8xyzw,vf10w\n"
            "sqc2 vf19,0(%0)\n"
            "sqc2 vf20,16(%0)\n"
            :
            : "r"(stage)
            : "memory");
        work = sw;
        line = work->idct_block[i];
        line[0] = stage[0] + stage[2] + stage[4];
        line[1] = stage[1] + stage[3] + stage[5];
        line[2] = stage[1] - stage[3] + stage[6];
        line[3] = stage[0] - stage[2] + stage[7];
        line[4] = stage[0] - stage[2] - stage[7];
        line[5] = stage[1] - stage[3] - stage[6];
        line[6] = stage[1] + stage[3] - stage[5];
        line[7] = stage[0] + stage[2] - stage[4];
    }

    /*
     * The work-area pointer the second pass reads is the one the first pass
     * left behind, so the first column starts without reloading it; every
     * later column re-reads sw, because the VU0 block above clobbers memory.
     * The original's loop is entered past that reload for exactly that reason
     * (see the style note in the attempt report: the structured spelling of
     * this loop compiles to a strength-reduced destination walk instead of the
     * original's recomputed sw + 5232 + i * 4 and block + i * 32).
     */
    i = 0;
    goto transform_column;
reload_work:
    work = sw;
    for (;;) {
transform_column:
        column = &work->idct_block[0][i];
        stage[0] = column[0 * 8];
        stage[1] = column[2 * 8];
        stage[2] = column[4 * 8];
        stage[3] = column[6 * 8];
        stage[4] = column[1 * 8];
        stage[5] = column[3 * 8];
        stage[6] = column[5 * 8];
        stage[7] = column[7 * 8];
        __asm__ __volatile__(
            "lqc2 vf9,0(%0)\n"
            "lqc2 vf10,16(%0)\n"
            "vmulax.xyzw ACCxyzw,vf11xyzw,vf9x\n"
            "vmadday.xyzw ACCxyzw,vf12xyzw,vf9y\n"
            "vmaddaz.xyzw ACCxyzw,vf13xyzw,vf9z\n"
            "vmaddw.xyzw vf19xyzw,vf14xyzw,vf9w\n"
            "vmulax.xyzw ACCxyzw,vf15xyzw,vf10x\n"
            "vmadday.xyzw ACCxyzw,vf16xyzw,vf10y\n"
            "vmaddaz.xyzw ACCxyzw,vf17xyzw,vf10z\n"
            "vmaddw.xyzw vf20xyzw,vf18xyzw,vf10w\n"
            "sqc2 vf19,0(%0)\n"
            "sqc2 vf20,16(%0)\n"
            :
            : "r"(stage)
            : "memory");
        out = block[i];
        out[0] = stage[0] + stage[2] + stage[4];
        out[1] = stage[1] + stage[3] + stage[5];
        out[2] = stage[1] - stage[3] + stage[6];
        out[3] = stage[0] - stage[2] + stage[7];
        out[4] = stage[0] - stage[2] - stage[7];
        out[5] = stage[1] - stage[3] - stage[6];
        out[6] = stage[1] + stage[3] - stage[5];
        out[7] = stage[0] + stage[2] - stage[4];
        if (++i >= 8) {
            break;
        }
        goto reload_work;
    }
}

static void ConvertMCU2YUV(void) {
    int mode = *(int *)((u8 *)sw + 0x360);
    float *yuv = (float *)((u8 *)sw + 0x15A0);
    float *mcu = (float *)((u8 *)sw + 0x25A0);
    int i, j, k;
    
    switch (mode) {
    case 0:
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                yuv[i * 64 + j * 4 + 0] = mcu[i * 8 + j];
                yuv[i * 64 + j * 4 + 1] = mcu[i * 8 + j + 256];
                yuv[i * 64 + j * 4 + 2] = mcu[i * 8 + j + 320];
            }
        }
        break;
    case 1:
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                float f, g;
                yuv[i * 64 + j * 4] = mcu[i * 8 + j];
                yuv[i * 64 + j * 4 + 32] = mcu[i * 8 + j + 64];
                f = mcu[i * 8 + j + 256];
                yuv[i * 64 + j * 8 + 5] = f;
                yuv[i * 64 + j * 8 + 1] = f;
                g = mcu[i * 8 + j + 320];
                yuv[i * 64 + j * 8 + 6] = g;
                yuv[i * 64 + j * 8 + 2] = g;
            }
        }
        break;
    case 2:
    default:
        i = 0;
        k = 0;
        for (; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                float f, g;
                yuv[i * 64 + j * 4] = mcu[k + j];
                yuv[i * 64 + j * 4 + 32] = mcu[k + j + 64];
                yuv[i * 64 + j * 4 + 512] = mcu[k + j + 128];
                yuv[i * 64 + j * 4 + 544] = mcu[k + j + 192];
                f = mcu[k + j + 256];
                yuv[i * 128 + j * 8 + 69] = f;
                yuv[i * 128 + j * 8 + 65] = f;
                yuv[i * 128 + j * 8 + 5] = f;
                yuv[i * 128 + j * 8 + 1] = f;
                g = mcu[k + j + 320];
                yuv[i * 128 + j * 8 + 70] = g;
                yuv[i * 128 + j * 8 + 66] = g;
                yuv[i * 128 + j * 8 + 6] = g;
                yuv[i * 128 + j * 8 + 2] = g;
            }
            k += 8;
        }
        break;
    }
}

static void ConvertYUV2RGB(int w, int h) {
    int i, n;
    unsigned int *dst = (unsigned int *)0x70003000;
    for (i = 0; i < h; i++) {
        float *src = (float *)((u8 *)sw + (i << 8) + 0x15A0);
        if (w > 0) {
        n = w;
        do {
            int v;
            __asm__ __volatile__(
                "lqc2 $vf10,0(%1)\n"
                "vmulax.xyzw ACCxyzw,$vf21xyzw,$vf10x\n"
                "vmadday.xyzw ACCxyzw,$vf22xyzw,$vf10y\n"
                "vmaddaz.xyzw ACCxyzw,$vf23xyzw,$vf10z\n"
                "vmaddw.xyzw $vf9xyzw,$vf24xyzw,$vf0w\n"
                "qmfc2.ni $8,$vf25\n"
                "vftoi0.xyzw $vf10xyzw,$vf9xyzw\n"
                "qmfc2.ni $9,$vf26\n"
                "qmfc2.ni %0,$vf10\n"
                "pmaxw $3,%0,$8\n"
                "pminw %0,$3,$9\n"
                "ppach $3,$0,%0\n"
                "ppacb %0,$0,$3\n"
                "sw %0,0(%2)\n"
                : "=&r"(v)
                : "r"(src), "r"(dst)
                : "$3", "$8", "$9", "memory");
            n--;
            src += 4;
            dst++;
        } while (n != 0);
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/xgl_jpeg", StartOfScan);

INCLUDE_ASM("asm/main/nonmatchings/xgl_jpeg", xglJpegDecode);

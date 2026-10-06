#include "common.h"
#include "shared.h"
#include "end_print.h"
#include "main/xgl_jpeg.h"

#define NULL ((void *)0)

typedef struct EndPrintRenderState {
    unsigned char unmodeled_00[4];
    short scissor_width;                 /* +0x04 */
    short scissor_height;                /* +0x06 */
    unsigned char unmodeled_08[0x0c];
    unsigned short draw_back_value;      /* +0x14 */
    unsigned char unmodeled_16[0x0a];
    unsigned short draw_back_reset_value; /* +0x20 */
} EndPrintRenderState;

extern EndPrintRenderState sRender;
extern unsigned char D_00532A38[];
extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int count);

int ePrintWHGet(void)
{
    return 0;
}

extern void eMessageSpriteReset(void);

void endPrintInit(void)
{
    pPrintFuncTop = PrintFunc;
    PrintFunc[0].func = NULL;
    eMessageSpriteReset();
}

INCLUDE_ASM("asm/main/nonmatchings/end_print", endPrintWinTexLoad);

extern void xglFontReloadTexture(EndPrintContext *context, int mode);

static void PrintFlush(EndPrintContext *context)
{
    PrintFuncEntry *cursor;
    void (*func)(EndPrintContext *context, int param);
    int param;

    xglFontReloadTexture(context, 2);
    cursor = PrintFunc;
print_flush_loop:
    func = cursor->func;
    if (func != NULL) {
        param = cursor->param;
        cursor++;
        func(context, param);
        goto print_flush_loop;
    }
    endPrintInit();
    xglFontReloadTexture(context, 1);
    eMessageSpriteReset();
}

void endPrintExtFuncPack(void (*func)(EndPrintContext *context, int param), int param)
{
    PrintFuncEntry *entry;

    entry = pPrintFuncTop;
    entry->func = func;
    entry->param = param;
    entry++;
    pPrintFuncTop = entry;
    if ((u32)D_00532A38 < (u32)entry) {
        endPrintInit();
        entry = pPrintFuncTop;
    }
    entry->func = NULL;
}

extern void xglPrimAddGifTagDirect(XglPacket *packet, const void *data, int count);
static inline u64 *endSetReg(u64 *p, u64 data, u64 addr)
{
    p[0] = data;
    p[1] = addr;
    return p + 2;
}

static void endPrintInfoSet(EndPrintContext *context, int clut, int flag)
{
    u64 *p;
    int n;
    unsigned short f;

    p = &context->scratch[2];
    p = endSetReg(p, 0, 0x3F);
    p = endSetReg(p, 0x44, 0x42);
    p = endSetReg(p, 0x3FFFF0, 8);
    n = 3;
    f = flag & 1;
    if (f) {
        if (clut) {
            p[0] = ((u64)((unsigned int)clut >> 6) << 37) | 0x2000000629343C00;
        } else {
            p[0] = 0x5DC00BC00;
        }
        n = 4;
        p[1] = 6;
        p += 2;
    }
    p = endSetReg(p, 0x5000D, 0x47);
    n++;
    p = context->scratch;
    p[0] = n + 0x1000000000000000;
    p[1] = 0xE;
    xglPrimAddGifTagDirect(context->packet, p, n + 1);
}

/*
 * uv_clut: a fixed table of UV/CLUT entries; only the CLUT id at +0x04 of
 * each 8-byte entry is read here.
 */
typedef struct UvClutEntry {
    unsigned char unmodeled_00[4]; /* +0x00 */
    int clut;                      /* +0x04 */
} UvClutEntry;

extern UvClutEntry uv_clut[26];

typedef struct { int pad; unsigned short u, v; } EndUvEnt;

void endSpriteSet(unsigned char *data, int kind)
{
    int idx = data[0x10];
    EndUvEnt *tbl = *(EndUvEnt **)uv_clut[data[0x11]].unmodeled_00;

    if (kind & 1) {
        *(unsigned short *)(data + 8) = tbl[idx].u;
        *(unsigned short *)(data + 0xA) = tbl[idx].v;
    }
    if (kind & 2) {
        data[0xC] = data[0xD] = data[0xE] = data[0xF] = 0x80;
    }
    if (kind & 4) {
        data[0x13] = data[0x12] = 0;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/end_print", subPrintSprite);

/*
 * The sprite descriptor subPrintSprite (still asm) draws; only the CLUT
 * index byte at +0x11 is read by PrintSprite00.
 */
typedef struct PrintSpriteInfo {
    unsigned char unmodeled_00[0x11]; /* +0x00..+0x10 */
    unsigned char clutIndex;          /* +0x11 */
} PrintSpriteInfo;

extern void endPrintInfoSet(EndPrintContext *context, int clut, int flag);
extern void subPrintSprite(EndPrintContext *context, PrintSpriteInfo *sprite);

static void PrintSprite00(EndPrintContext *context, PrintSpriteInfo *sprite)
{
    endPrintInfoSet(context, uv_clut[sprite->clutIndex].clut, 1);
    subPrintSprite(context, sprite);
}

INCLUDE_ASM("asm/main/nonmatchings/end_print", PrintSprite01);

typedef struct PrintPointInfo {
    short x;
    short y;
    unsigned int z;
    unsigned char r, g, b, a;
} PrintPointInfo;

static void PrintPoint(EndPrintContext *context, PrintPointInfo *pt)
{
    int *p;

    p = (int *)context->scratch;
    endPrintInfoSet(context, 0, 0);
    p[0] = 0x8001;
    p[1] = 0x20204000;
    p[2] = 0x51;
    p[3] = 0;
    p = (int *)&context->scratch[2];
    p[0] = pt->r;
    p[1] = pt->g;
    p[2] = pt->b;
    p[3] = pt->a;
    p = (int *)&context->scratch[4];
    p[0] = pt->x * 16 + 0x6FF8;
    p[1] = pt->y * 16 + 0x71F8;
    p[2] = pt->z & 0xFFFFFF;
    p[3] = 0;
    sceVif1PkAddDirectDataN(context->packet, context->scratch, 3);
}

static void subPrintLine(EndPrintContext *context, int line) {
    int *p;
    int n;
    int count;
    int q;
    int r;
    int w;

    n = 0;
    p = (int *)context->scratch;
    w = line + 0xC;
    if (*(int *)(line + 4) != 0) {
        do {
            r = w + 4;
            w += 0xC;
            n += 1;
        } while (*(int *)r != 0);
    }
    p[0] = n + 0x8000;
    p[1] = 0x20654000;
    count = 1;
    q = line;
    p[2] = 0x51;
    p[3] = 0;
    p += 4;
    if (n > 0) {
        do {
            n -= 1;
            count += 2;
            p[0] = *(unsigned char *)(q + 8);
            p[1] = *(unsigned char *)(q + 9);
            p[2] = *(unsigned char *)(q + 0xA);
            p[3] = *(unsigned char *)(q + 0xB);
            p += 4;
            p[0] = (*(short *)q * 0x10) + 0x6FF8;
            p[1] = (*(short *)(q + 2) * 0x10) + 0x71F8;
            q += 0xC;
            p[2] = *(int *)(line + 4) & 0xFFFFFF;
            p[3] = 0;
            p += 4;
        } while (n > 0);
    }
    sceVif1PkAddDirectDataN(context->packet, context->scratch, count);
}

extern void subPrintLine(EndPrintContext *context, int line);

static void PrintLine(EndPrintContext *context, int line)
{
    endPrintInfoSet(context, 0, 0);
    subPrintLine(context, line);
}

INCLUDE_ASM("asm/main/nonmatchings/end_print", PrintLine2);

INCLUDE_ASM("asm/main/nonmatchings/end_print", endPrintDirectRibbon);

static void subPrintRibbon(EndPrintContext *context, int line) {
    int *p;
    int n;
    int count;
    int q;
    int r;
    int w;

    n = 0;
    p = (int *)context->scratch;
    w = line + 0xC;
    if (*(int *)(line + 4) != 0) {
        do {
            r = w + 4;
            w += 0xC;
            n += 1;
        } while (*(int *)r != 0);
    }
    p[0] = n + 0x8000;
    p[1] = 0x20A64000;
    count = 1;
    q = line;
    p[2] = 0x51;
    p[3] = 0;
    p += 4;
    if (n > 0) {
        do {
            n -= 1;
            count += 2;
            p[0] = *(unsigned char *)(q + 8);
            p[1] = *(unsigned char *)(q + 9);
            p[2] = *(unsigned char *)(q + 0xA);
            p[3] = *(unsigned char *)(q + 0xB);
            p += 4;
            p[0] = (*(short *)q * 0x10) + 0x6FF8;
            p[1] = (*(short *)(q + 2) * 0x10) + 0x71F8;
            q += 0xC;
            p[2] = *(int *)(line + 4) & 0xFFFFFF;
            p[3] = 0;
            p += 4;
        } while (n > 0);
    }
    sceVif1PkAddDirectDataN(context->packet, context->scratch, count);
}

extern void subPrintRibbon(EndPrintContext *context, int ribbon);

static void PrintRibbon(EndPrintContext *context, int ribbon)
{
    endPrintInfoSet(context, 0, 0);
    subPrintRibbon(context, ribbon);
}

INCLUDE_ASM("asm/main/nonmatchings/end_print", PrintTagFont);

INCLUDE_ASM("asm/main/nonmatchings/end_print", ScissorSet);

static void ScissorReset(EndPrintContext *context)
{
    u64 *scratch;

    scratch = context->scratch;
    scratch[0] = ((u64)0x10000000 << 32) | 0x8001;
    scratch[1] = 14;
    scratch[2] = ((u64)(sRender.scissor_width - 1) << 16) |
                 ((u64)(sRender.scissor_height - 1) << 48);
    scratch[3] = 64;
    sceVif1PkAddDirectDataN(context->packet, scratch, 2);
}

INCLUDE_ASM("asm/main/nonmatchings/end_print", PrintNumber);

INCLUDE_ASM("asm/main/nonmatchings/end_print", PrintWindow);

INCLUDE_ASM("asm/main/nonmatchings/end_print", PrintFrame);

INCLUDE_ASM("asm/main/nonmatchings/end_print", PrintThumbnail);

INCLUDE_ASM("asm/main/nonmatchings/end_print", PrintCircleCore);

#include "main/xgl_packet.h"

/* EE scratchpad RAM base (src/ov01/gr_gp_init.c's grPacketSend and
 * src/ov01/m_gs.c's MGsGPTerm build the same VIF1 direct-mode packet at the
 * same address). */
#define SCRATCHPAD_BASE ((void *)0x70000000)

extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);
extern void sceVif1PkCloseDirectHLCode(XglPacket *packet);

/* PrintCircleCore (this TU, still asm) draws into the scratchpad buffer this
 * function points it at. */
static void PrintCircleCore(XglPacket *packet, void *buffer, int radius);

void endPrintDirectCircle(int radius) {
    XglPacket *vif1Packet;

    vif1Packet = xglPacketGetCurrent();
    sceVif1PkCnt(vif1Packet, 0);
    sceVif1PkOpenDirectHLCode(vif1Packet, 0);
    PrintCircleCore(vif1Packet, SCRATCHPAD_BASE, radius);
    sceVif1PkCloseDirectHLCode(vif1Packet);
}

static void PrintCircle(EndPrintContext *context, int radius)
{
    PrintCircleCore(context->packet, context->scratch, radius);
}

extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int count);

static void FontTexReload(EndPrintContext *context)
{
    u64 *scratch;

    scratch = context->scratch;
    scratch[0] = ((u64) 0x10000000 << 32) | 0x8000;
    scratch[1] = 0xE;
    scratch[3] = 0x3F;
    scratch[2] = 0;
    sceVif1PkAddDirectDataN(context->packet, scratch, 1);
    xglFontReloadTexture(context, 2);
}

INCLUDE_ASM("asm/main/nonmatchings/end_print", FontTexChange);

static void DrawBackSet(DrawBackContext *context)
{
    u64 *scratch;
    unsigned int draw_back_control;

    scratch = context->scratch;
    draw_back_control = sRender.draw_back_value;
    draw_back_control |= 0x80000;
    scratch[0] = ((u64)0x10000000 << 32) | 0x8003;
    scratch[1] = 14;
    scratch[3] = 63;
    scratch[4] = (u64)0x8000 << 17;
    scratch[5] = 78;
    scratch[6] = draw_back_control;
    scratch[7] = 76;
    scratch[2] = 0;
    sceVif1PkAddDirectDataN(context->packet, scratch, 4);
}

static void DrawBackReset(DrawBackContext *context)
{
    u64 *scratch;
    unsigned int draw_back_control;

    scratch = context->scratch;
    draw_back_control = sRender.draw_back_reset_value;
    draw_back_control |= 0x80000;
    scratch[0] = ((u64)0x10000000 << 32) | 0x8003;
    scratch[1] = 14;
    scratch[3] = 63;
    scratch[4] = (u64)0x3100 << 16;
    scratch[5] = 78;
    scratch[6] = draw_back_control;
    scratch[7] = 76;
    scratch[2] = 0;
    sceVif1PkAddDirectDataN(context->packet, scratch, 4);
}

typedef union { u64 d; unsigned int w[2]; } ScU;
typedef struct { unsigned int v[8]; } ScS;

static void ScreenClear(DrawBackContext *context)
{
    ScU *scratch;

    scratch = (ScU *)context->scratch;
    ((u64*)scratch)[0] = ((u64)0x40AB4000 << 32) | 0x8001;
    ((u64*)scratch)[1] = 0x551E;
    ((u64*)scratch)[2] = 0x31001;
    ((u64*)scratch)[3] = 0x47;
    ((u64*)scratch)[4] = 0;
    ((u64*)scratch)[5] = 0;
    scratch = (ScU *)&context->scratch[6];
    ((unsigned int *)scratch)[0] = 0x6FF8;
    ((unsigned int *)scratch)[1] = 0x71F8;
    ((unsigned int *)scratch)[4] = 0x8FF8;
    ((unsigned int *)scratch)[5] = 0x8DF8;
    ((unsigned int *)scratch)[2] = 0;
    ((unsigned int *)scratch)[3] = 0;
    ((unsigned int *)scratch)[6] = 0;
    ((unsigned int *)scratch)[7] = 0;
    sceVif1PkAddDirectDataN(context->packet, context->scratch, 5);
}

INCLUDE_ASM("asm/main/nonmatchings/end_print", ZScissor);

INCLUDE_ASM("asm/main/nonmatchings/end_print", PrintBackSprite);

INCLUDE_ASM("asm/main/nonmatchings/end_print", PrintBackSprite2);

INCLUDE_ASM("asm/main/nonmatchings/end_print", PrintEtherLine);

extern int xglFontPrintExtFunc(unsigned int flags, void (*draw)(EndPrintContext *context, int param), int arg);
extern void (*func_tbl_4[])(EndPrintContext *context, int param);
extern void (*func_tbl2_5[])(EndPrintContext *context, int param);

int endPrintExtFunc(unsigned int flags, int id, int param)
{
    if (id < 100) {
        if (id < 18) {
            endPrintExtFuncPack(func_tbl_4[id], param);
        }
    } else {
        id -= 100;
        if (id < 7) {
            xglFontPrintExtFunc(flags, func_tbl2_5[id], param);
        }
    }
}

/* VIF1 direct-mode environment for endPrintDirectFrameCopy; only element 4
 * (the tag word written below) is touched here. */
extern u64 FrameCopyEnv_6[12];

void endPrintDirectFrameCopy(XglPacket *packet, int tagLow, int tagHigh)
{
    if (packet != NULL) {
        FrameCopyEnv_6[4] = (u64) (u32) (tagLow | 0x80000) |
                             ((u64) tagHigh << 0x20) |
                             ((u64) 0x8000 << 0x24);
        sceVif1PkRef(packet, FrameCopyEnv_6, 6, 0, 0, 0);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/end_print", endPrintDirectLine);

/*
 * The JPEG-decode setup context: only the fields endDecodeJpeg itself
 * reads or writes are named. +0x10/+0x14 are filled from the source and
 * destination words already stored at +0x30/+0x34, and +0x20..+0x27 is
 * cleared before the decode call; the rest of the struct is untouched
 * here.
 */
typedef struct EndDecodeJpegContext {
    unsigned char unmodeled_00[8]; /* +0x00..+0x07 */
    short width;                   /* +0x08 */
    short height;                  /* +0x0a */
    unsigned char unmodeled_0c[4]; /* +0x0c..+0x0f */
    int decodeSource;              /* +0x10 */
    int decodeDestination;         /* +0x14 */
    unsigned char unmodeled_18[8]; /* +0x18..+0x1f */
    short decodeParam[4];          /* +0x20..+0x27 */
    unsigned char unmodeled_28[8]; /* +0x28..+0x2f */
    int source;                    /* +0x30 */
    int destination;               /* +0x34 */
} EndDecodeJpegContext;

void endDecodeJpeg(EndDecodeJpegContext *context) {
    int source;
    int destination;

    source = context->source;
    destination = context->destination;
    context->width = 0x200;
    context->height = 0x1C0;
    context->decodeSource = source;
    context->decodeDestination = destination;
    context->decodeParam[0] = 0;
    context->decodeParam[1] = 0;
    context->decodeParam[2] = 0;
    context->decodeParam[3] = 0;
    xglJpegDecode(&context->decodeSource);
}

INCLUDE_ASM("asm/main/nonmatchings/end_print", endPrintJpeg);

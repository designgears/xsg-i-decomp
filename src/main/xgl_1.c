#include "common.h"
#include "shared.h"

extern const char D_004DC328[];

/*
 * xglMpeg2InfoInit2's own body is still INCLUDE_ASM below, but its tail at
 * 0x00222818..0x00222828 computes align64(arena + allocated_size +
 * audio_buffer_size) into v0 immediately before return: that value is the
 * function's result, not a void return. xglMpeg2InfoInit calls it and
 * discards the result, but the forward declaration used here must still
 * spell the pointer-return signature the body actually implements.
 */
extern void *xglMpeg2InfoInit2(void *info, void *arena, int audio_buffer_size);

struct CdStreamParam;

typedef struct XglMovieInfo {
    unsigned char unmodeled_00[8];
    unsigned char streamState;          /* +0x08 */
    unsigned char unmodeled_09[0x17];
    unsigned char *ringBuffer;          /* +0x20 */
    int ringCapacity;                   /* +0x24 */
    int ringWrite;                      /* +0x28 */
    int ringRead;                       /* +0x2c */
    unsigned char mpeg[0x10];           /* +0x30 */
    short width;                        /* +0x40 */
    short height;                       /* +0x42 */
    unsigned char unmodeled_44[2];
    short currentFrame;                 /* +0x46 */
    unsigned char unmodeled_48[0x30];
    unsigned char *arena;               /* +0x78 */
    int reservedSize;                   /* +0x7c */
    unsigned char *videoBuffer;         /* +0x80 */
    unsigned char *mpegBuffer;          /* +0x84 */
    unsigned char *readBuffer;          /* +0x88 */
    int pendingBytes;                   /* +0x8c */
    int audioSampleRate;                /* +0x90 */
    unsigned char audioHeaderRead;      /* +0x94 */
    unsigned char unmodeled_95[3];
    unsigned char decodeState;          /* +0x98 */
    unsigned char streamOpen;           /* +0x99 */
    unsigned char missCount;            /* +0x9a */
    unsigned char videoCount;           /* +0x9b */
    unsigned char *dmaBuffer;           /* +0x9c */
    int dmaSize;                        /* +0xa0 */
    int dmaWrite;                       /* +0xa4 */
    int dmaRead;                        /* +0xa8 */
    int dmaNextRead;                    /* +0xac */
    unsigned char *soundBuffer;         /* +0xb0 */
    int soundBufferSize;                /* +0xb4 */
    int soundWrite;                     /* +0xb8 */
    int soundRead;                      /* +0xbc */
} XglMovieInfo;

typedef struct XglMovieInitInfo {
    unsigned char unmodeled_00[8];
    unsigned char streamState;          /* +0x08 */
    unsigned char unmodeled_09[0x17];
    unsigned char *ringBuffer;          /* +0x20 */
    int ringCapacity;                   /* +0x24 */
    int ringWrite;                      /* +0x28 */
    int ringRead;                       /* +0x2c */
    unsigned char unmodeled_30[0x16];
    short currentFrame;                 /* +0x46 */
    unsigned char unmodeled_48[0x40];
    unsigned char *readBuffer;          /* +0x88 */
    unsigned char unmodeled_8c[0x0e];
    unsigned char missCount;            /* +0x9a */
    unsigned char unmodeled_9b[0x1d];
    unsigned int packetAddress[2];      /* +0xb8 */
    unsigned char unmodeled_c0[9];
    unsigned char transMode;            /* +0xc9 */
    unsigned char threshold1;           /* +0xca */
    unsigned char threshold0;           /* +0xcb */
} XglMovieInitInfo;

typedef struct MpegStreamData {
    unsigned char unmodeled_00[8];
    unsigned char *buffer;              /* +0x08 */
    int length;                         /* +0x0c */
} MpegStreamData;

#define D_ENABLEW ((volatile u32 *) 0x1000f590)
#define D3_CHCR   ((volatile u32 *) 0x1000b000)
#define D3_MADR   ((volatile u32 *) 0x1000b010)
#define D3_QWC    ((volatile u32 *) 0x1000b020)
#define D4_MADR   ((volatile u32 *) 0x1000b410)
#define D4_QWC    ((volatile u32 *) 0x1000b420)
#define D4_CHCR   ((volatile u32 *) 0x1000b400)
#define IPU_CMD   ((volatile u32 *) 0x10002000)
#define IPU_CTRL  ((volatile u32 *) 0x10002010)
#define UNCACHED(address) \
    ((void *) (((unsigned int) (address) & 0x0FFFFFFF) | 0x20000000))

extern int DIntr(void);
extern int EIntr(void);
extern void *memcpy(void *destination, const void *source, unsigned int count);
extern void FlushCache(int mode);
extern int sceIpuSync(int mode, int timeout);
extern int xglCdStreamClose(struct CdStreamParam *stream);
extern void xglCdStreamParamInit(struct CdStreamParam *param);
extern void xglCdStreamReadRingCore(struct CdStreamParam *stream);
extern void xglCdStreamReadRing(struct CdStreamParam *stream, int byte_count);
extern int xglCdStreamOpen(struct CdStreamParam *stream);
extern void sceMpegInit(void);
extern void sceMpegCreate(void *mpeg, void *work, int work_size);
extern void sceMpegDelete(void *mpeg);
extern void sceMpegAddCallback(void *mpeg, int type, int (*callback)(), void *argument);
extern void sceMpegAddStrCallback(void *mpeg, int stream, int channel,
                                  int (*callback)(), void *argument);
extern int sceMpegDemuxPss(void *mpeg, unsigned char *source, int size);
extern int SsdGetResultValue(int *value);
extern void SsdInitVagStreamStereo(int size, int channels);
static int fillBuff(XglMovieInfo *info, int wait);
static int nodataCallback(int event, void *data, XglMovieInfo *info);

/*
 * Maps a scratchpad address (0x7000xxxx) into the DMA scratchpad alias
 * before programming D3_MADR/D3_QWC, then starts DMA channel 3 (fromIPU) in
 * direct mode (D3_CHCR = 0x100) with the D_ENABLEW bit 0x10000 held around
 * the CHCR write.
 */
static int myDmaDirectFromIPU(u32 address, u32 count)
{
    if ((address >> 16) != 0x7000)
        *D3_MADR = address;
    else
        *D3_MADR = (address & 0x3fff) + 0x80000000;
    *D3_QWC = count;

    DIntr();
    *D_ENABLEW = *D_ENABLEW | 0x10000;
    *D3_CHCR = 0x100;
    *D_ENABLEW = *D_ENABLEW & 0xfffeffff;
    return EIntr();
}

/*
 * Writes value to D4_CHCR (DMA channel 4, toIPU) with the D_ENABLEW bit
 * 0x10000 held around the write, like myDmaDirectFromIPU above.
 */
static void setD4_CHCR(u32 value)
{
    DIntr();
    *D_ENABLEW = *D_ENABLEW | 0x10000;
    *D4_CHCR = value;
    *D_ENABLEW = *D_ENABLEW & 0xfffeffff;
    EIntr();
}

extern struct {
    unsigned char pad00[0x14];
    u16 w14;
    unsigned char pad16[4];
    u16 w1a;
    unsigned char pad1c[0x40];
} sRender;
typedef struct XglMovieGsView {
    unsigned char unmodeled_00[0x40];
    short height;                       /* +0x40 */
    unsigned char unmodeled_42[0x6e];
    int vramAddr[2];                    /* +0xb0 */
    unsigned long *packet[2];           /* +0xb8 */
    unsigned long texReg;               /* +0xc0 */
    unsigned char bufIndex;             /* +0xc8 */
    unsigned char transMode;            /* +0xc9 */
} XglMovieGsView;

static unsigned long *make_gstrans(XglMovieGsView *info, int x) {
    unsigned long *p;
    int addr;
    int h;
    int y;

    h = info->height;
    p = info->packet[info->bufIndex];
    addr = info->vramAddr[info->bufIndex];
    p[0] = 0x10000003;
    p[2] = 0x1000000000008002;
    p[3] = 0xE;
    p[1] = 0;
    switch (info->transMode) {
    case 0:
    default:
        p[4] = (unsigned long)sRender.w14 << 37 | 0x0008000000000000;
        break;
    case 1:
        p[4] = (unsigned long)sRender.w1a << 37 | 0x0008000000000000;
        break;
    case 2:
        p[4] = info->texReg;
        break;
    }
    p[5] = 0x50;
    p[6] = 0x0000001000000010;
    p[7] = 0x52;
    p += 8;
    for (y = 0; y < h; y += 16) {
        p[0] = 0x10000004;
        p[1] = 0;
        p[2] = 0x1000000000008002;
        p[3] = 0xE;
        p[4] = (long)y << 32 | (long)x << 48;
        p[5] = 0x51;
        p[6] = 0;
        p[7] = 0x53;
        p[8] = 0x0800000000008040;
        p[9] = 0;
        p += 10;
        ((int *)p)[1] = addr;
        addr += 0x400;
        ((int *)p)[0] = 0x30000040;
        p[1] = 0;
        p += 2;
    }
    ((int *)p)[-4] &= 0x0FFFFFFF;
    return p;
}

typedef struct XglMovieDmaView {
    unsigned char pad00[8];
    unsigned char wait;                 /* +0x08 */
    unsigned char pad09[0xf];
    int loop;                           /* +0x18 */
    unsigned char pad1c[0xc];
    int end;                            /* +0x28 */
    u32 pos;                            /* +0x2c */
    unsigned char pad30[0x30];
    unsigned char tag0[0x10];           /* +0x60 */
    unsigned char tag1[0x10];           /* +0x70 */
    unsigned char tag2[0x10];           /* +0x80 */
    int base;                           /* +0x90 */
    u32 size;                           /* +0x94 */
    unsigned char state;                /* +0x98 */
} XglMovieDmaView;
#define D4_TADR   ((volatile u32 *) 0x1000b430)

static int checkdma(XglMovieDmaView *d)
{
    u32 off;
    int ret = 0;

    if (d->state == 99) {
        return 0;
    }
    setD4_CHCR(5);
    off = *D4_MADR - d->base;
    switch (d->state) {
    case 0:
        if (d->size < off) {
            if (d->pos != d->end && d->end - off < (u32)((int)d->size / 2) && d->wait == 0) {
                ret = 1;
            } else {
                d->pos = d->size;
                if (d->loop == 0) {
                    *D4_TADR = (u32)d->tag2;
                    d->state = 3;
                } else {
                    *D4_TADR = (u32)d->tag0;
                    d->state = 1;
                }
            }
        } else {
            *D4_TADR = (u32)d->tag1;
        }
        break;
    case 1:
        if (off < d->size) {
            if (d->pos != d->end && d->end - off < (u32)((int)d->size / 2) && d->wait == 0) {
                ret = 1;
            } else {
                d->pos = 0;
                if (d->loop == 0) {
                    *D4_TADR = (u32)d->tag2;
                    d->state = 2;
                } else {
                    *D4_TADR = (u32)d->tag1;
                    d->state = 0;
                }
            }
        } else {
            *D4_TADR = (u32)d->tag0;
        }
        break;
    case 2:
        if (d->size < off) {
            d->state = 99;
        }
        break;
    case 3:
        if (off < d->size) {
            d->state = 99;
        }
        break;
    }
    if (d->state != 99) {
        setD4_CHCR(0x30000105);
    } else {
        setD4_CHCR(0x105);
    }
    return ret;
}

typedef struct XglMovieTag {
    int tag;
    int addr;
    unsigned long pad;
} XglMovieTag;
typedef struct XglMovieOpenView {
    unsigned char pad00[0x14];
    int pos;                            /* +0x14 */
    unsigned char pad18[8];
    int *ringBuffer;                    /* +0x20 */
    int ringCapacity;                   /* +0x24 */
    int ringWrite;                      /* +0x28 */
    int ringRead;                       /* +0x2c */
    unsigned char pad30[0x10];
    short width;                        /* +0x40 */
    short height;                       /* +0x42 */
    short w44;                          /* +0x44 */
    short currentFrame;                 /* +0x46 */
    int stride;                         /* +0x48 */
    int frameSize;                      /* +0x4c */
    XglMovieTag tag[4];                 /* +0x50 */
    int *data;                          /* +0x90 */
    int size;                           /* +0x94 */
    unsigned char state;                /* +0x98 */
    unsigned char streamOpen;           /* +0x99 */
    unsigned char pad9a[5];
    unsigned char b9f;                  /* +0x9f */
    int dmaChcr;                        /* +0xa0 */
    int dmaAddr;                        /* +0xa4 */
    int dmaQwc;                         /* +0xa8 */
    XglMovieTag *dmaTag;                /* +0xac */
    unsigned char padb0[0x18];
    unsigned char bufIndex;             /* +0xc8 */
    unsigned char padc9[7];
} XglMovieOpenView;
extern void SyncDCache(void *start, void *end);

int xglMovieOpen(XglMovieOpenView *info, int mode)
{
    int *data;
    int w;

    if (mode != 0) {
        XglMovieTag *t = info->tag;
        info->streamOpen = 1;
        info->ringBuffer = info->data;
        info->ringCapacity = info->size * 2;
        ((int (*)(struct CdStreamParam *, int)) xglCdStreamOpen)((struct CdStreamParam *) info, mode);
        t[0].tag = info->size / 16 - 1;
        data = info->data;
        t[0].addr = (int) (data + 4);
        t[0].pad = 0;
        t[1].pad = 0;
        t[1].tag = info->size / 16;
        t[1].addr = (int) data;
        t[2].pad = 0;
        t[2].tag = info->size / 16;
        t[2].addr = (int) data + info->size;
        t[3].tag = (info->pos % info->size + 15) / 16;
        t[3].addr = info->size * ((info->pos / info->size) & 1) + (int) data;
        t[3].pad = 0;
        xglCdStreamReadRing((struct CdStreamParam *) info, info->size);
        info->ringRead = 0;
        info->dmaTag = t;
        info->dmaChcr = 0x105;
        info->state = 0;
        info->dmaAddr = 0;
        info->dmaQwc = 0;
        info->b9f = 0;
    } else {
        info->dmaChcr = 0x105;
        info->dmaAddr = (int) (info->data + 4);
        info->dmaQwc = (info->size - 1) / 16;
        info->streamOpen = 0;
        info->dmaTag = 0;
        info->b9f = 0;
    }
    data = info->data;
    if (data[0] != 0x6D757069) {
        xglMovieClose((XglMovieInfo *) info);
        return 1;
    }
    info->width = ((unsigned short *) data)[4];
    w = (info->width >> 4) << 6;
    info->stride = w;
    info->height = ((unsigned short *) data)[5];
    info->w44 = ((unsigned short *) data)[6];
    info->currentFrame = 0;
    info->bufIndex = 0;
    info->frameSize = (info->height >> 4) * w;
    SyncDCache(info, info + 1);
    return 0;
}

typedef unsigned int XglMovieQuad __attribute__((mode(TI)));
typedef struct XglMoviePlayView {
    unsigned char pad00[0x28];
    int ringWrite;                      /* +0x28 */
    int ringRead;                       /* +0x2c */
    unsigned char pad30[0x10];
    short width;                        /* +0x40 */
    short height;                       /* +0x42 */
    short frames;                       /* +0x44 */
    short currentFrame;                 /* +0x46 */
    int stride;                         /* +0x48 */
    int frameSize;                      /* +0x4c */
    XglMovieTag tag[4];                 /* +0x50 */
    int data;                           /* +0x90 */
    int size;                           /* +0x94 */
    unsigned char state;                /* +0x98 */
    unsigned char streamOpen;           /* +0x99 */
    unsigned char loop;                 /* +0x9a */
    unsigned char pad9b[4];
    unsigned char b9f;                  /* +0x9f */
    int dmaChcr;                        /* +0xa0 */
    int dmaAddr;                        /* +0xa4 */
    u32 dmaQwc;                         /* +0xa8 */
    XglMovieTag *dmaTag;                /* +0xac */
    int vramAddr[2];                    /* +0xb0 */
    int packet[2];                      /* +0xb8 */
    unsigned long texReg;               /* +0xc0 */
    unsigned char bufIndex;             /* +0xc8 */
    unsigned char transMode;            /* +0xc9 */
    unsigned char bca;                  /* +0xca */
    unsigned char bcb;                  /* +0xcb */
    unsigned char padcc[4];
} XglMoviePlayView;
#define IPU_BP    ((volatile u32 *) 0x10002020)
extern int printf(const char *format, ...);
extern char D_004D2580[];
extern void xglDmaDirectSrcChain(u32 channel, u32 address);
extern int xglCdStreamRewind(struct CdStreamParam *stream);

int xglMoviePlay(XglMoviePlayView *info)
{
    int y;
    int i;
    u32 v;
    u32 c;

    if (!(*D4_CHCR & 0x100)) {
        *IPU_CTRL = 0x40000000;
        if (sceIpuSync(0, 0x40) < 0)
            return -1;
        *IPU_CMD = (info->bca << 16) | info->bcb | 0x90000000;
        if (sceIpuSync(0, 0x40) < 0)
            return -1;
        *IPU_CMD = info->b9f;
        if (sceIpuSync(0, 0x40) < 0)
            return -1;
        *D4_MADR = info->dmaAddr;
        if (info->dmaQwc > 0xffff)
            *D4_QWC = 0xffff;
        else
            *D4_QWC = info->dmaQwc;
        *D4_TADR = (u32)info->dmaTag;
        setD4_CHCR(info->dmaChcr);
    }
    if (info->streamOpen != 0 && checkdma((XglMovieDmaView *)info) != 0) {
        printf(D_004D2580);
        return -1;
    }
    if (info->transMode != 0xff) {
        sceGsSyncPath(0, 0);
        for (y = 0; y < info->height; y += 16) {
            myDmaDirectFromIPU(info->vramAddr[info->bufIndex], info->stride);
            if (y == 0) {
                *IPU_CMD = 0x40000000;
                if (sceIpuSync(0, 0x40) < 0)
                    return -1;
                c = *IPU_CMD;
                *IPU_CMD = 0x40000008;
                v = c >> 24;
                if (sceIpuSync(0, 0x40) < 0)
                    return -1;
                *IPU_CTRL = (v & 0xfb) << 16;
                *IPU_CMD = (((v & 4) >> 2) << 24) | 0x10010000;
            }
            make_gstrans((XglMovieGsView *)info, y);
            i = 0;
            if (*D3_CHCR & 0x100) {
                do {
                    if (++i > 0xffffff)
                        break;
                } while (*D3_CHCR & 0x100);
            }
            xglDmaDirectSrcChain(2, info->packet[info->bufIndex]);
            info->bufIndex ^= 1;
        }
        sceGsSyncPath(0, 0);
    } else {
        for (y = 0; y < info->height; y += 16) {
            int w, x, j, s;
            short h;
            XglMovieQuad *src;
            XglMovieQuad *dst;

            myDmaDirectFromIPU(0x70000000, info->stride);
            if (y == 0) {
                *IPU_CMD = 0x40000000;
                if (sceIpuSync(0, 0x40) < 0)
                    return -1;
                c = *IPU_CMD;
                *IPU_CMD = 0x40000008;
                v = c >> 24;
                if (sceIpuSync(0, 0x40) < 0)
                    return -1;
                *IPU_CTRL = (v & 0xfb) << 16;
                *IPU_CMD = (((v & 4) >> 2) << 24) | 0x10010000;
            }
            i = 0;
            if (*D3_CHCR & 0x100) {
                do {
                    if (++i > 0xffffff)
                        break;
                } while (*D3_CHCR & 0x100);
            }
            h = info->width;
            w = h;
            src = (XglMovieQuad *)0x70000000;
            s = w / 4;
            dst = (XglMovieQuad *)(info->vramAddr[info->bufIndex] + w * y * 4);
            for (x = 0; x < info->width; x += 16) {
                for (j = 15; j >= 0; j--) {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                    dst[3] = src[3];
                    src += 4;
                    dst += s;
                }
                dst = dst - s * 16 + 4;
            }
        }
        info->bufIndex ^= 1;
    }
    if (sceIpuSync(0, 0x40) < 0)
        return -1;
    *IPU_CMD = 0x40000020;
    if (sceIpuSync(0, 0x40) < 0)
        return -1;
    if (++info->currentFrame >= info->frames) {
        if (info->loop == 0) {
            info->currentFrame = info->frames;
        } else {
            info->currentFrame = 0;
            setD4_CHCR(5);
            DIntr();
            *D_ENABLEW = *D_ENABLEW | 0x10000;
            *D3_CHCR = 0;
            *D_ENABLEW = *D_ENABLEW & 0xfffeffff;
            EIntr();
            if (info->streamOpen != 0) {
                xglCdStreamRewind((struct CdStreamParam *)info);
                info->ringWrite = 0;
                info->ringRead = 0;
                xglCdStreamReadRing((struct CdStreamParam *)info, info->size);
                info->ringRead = 0;
                info->dmaChcr = 0x105;
                info->dmaTag = info->tag;
                info->state = 0;
                info->dmaAddr = 0;
                info->dmaQwc = 0;
                info->b9f = 0;
            } else {
                info->dmaChcr = 0x105;
                info->dmaAddr = info->data + 0x10;
                info->dmaQwc = (info->size - 1) / 16;
                info->dmaTag = 0;
                info->b9f = 0;
            }
            SyncDCache(info, info + 1);
        }
        return 1;
    }
    if (info->streamOpen != 0) {
        u32 bp;
        int n;

        info->dmaChcr = *D4_CHCR;
        setD4_CHCR(5);
        bp = *IPU_BP;
        n = ((bp >> 16) & 3) + ((bp >> 8) & 0xf);
        info->dmaAddr = *D4_MADR - n * 16;
        info->dmaQwc = *D4_QWC + n;
        info->dmaTag = (XglMovieTag *)*D4_TADR;
        info->b9f = bp & 0x7f;
    } else {
        u32 bp;
        u32 n;

        info->dmaChcr = *D4_CHCR;
        setD4_CHCR(5);
        bp = *IPU_BP;
        n = (*D4_MADR - info->dmaAddr) - (((bp >> 16) & 3) + ((bp >> 8) & 0xf)) * 16;
        info->dmaAddr += n;
        info->dmaQwc -= (u32)n >> 4;
        info->dmaTag = (XglMovieTag *)*D4_TADR;
        info->b9f = bp & 0x7f;
    }
    return 0;
}

int xglMovieClose(XglMovieInfo *info)
{
    if (info->streamOpen) {
        xglCdStreamClose((struct CdStreamParam *) info);
    }
    setD4_CHCR(0);
    *IPU_CTRL = 0x40000000;
    if (sceIpuSync(0, 0x40) < 0) {
        return -1;
    }
    *IPU_CMD = 0;
    return sceIpuSync(0, 0x40) < 0 ? -1 : 0;
}

void xglMovieInfoInit(XglMovieInitInfo *info)
{
    xglCdStreamParamInit((struct CdStreamParam *) info);
    info->packetAddress[0] = 0x70000040;
    info->packetAddress[1] = 0x70001000;
    info->transMode = info->currentFrame = -1;
    info->missCount = 0;
    info->threshold1 = 0;
    info->threshold0 = 0;
}

void *xglMovieMakeXtxHeader(XglMovieInfo *info, unsigned char *header)
{
    int count;
    int *p;

    count = *(int *)&info->unmodeled_48[4];
    ((unsigned long *)header)[0] = 0x0000005000585458;
    ((unsigned long *)header)[1] = 0x0000001000000001;
    p = (int *)(header + 0x10);
    p[0] = info->width + 0x40000;
    p[1] = info->height;
    p[2] = 0;
    p[3] = count + 2;
    ((unsigned long *)header)[4] = 0x30;
    ((unsigned long *)header)[5] = 0;
    ((unsigned long *)header)[6] = 0;
    p = (int *)(header + 0x38);
    p[0] = 0;
    p[1] = count + 0x50000001;
    p[2] = count + 0x8000;
    p[3] = 0x08000000;
    ((unsigned long *)header)[9] = 0;
    return header + 0x50;
}

static int fileRead(XglMovieInfo *info)
{
    unsigned char *destination;
    int retry;
    int length;
    int available;
    int tail;

    available = 0;
    length = 0x4000;
    destination = info->readBuffer;
    for (retry = 0; retry <= 0xFFFFFF; retry++) {
        xglCdStreamReadRingCore((struct CdStreamParam *) info);
        available = (info->ringWrite - info->ringRead + info->ringCapacity) %
                    info->ringCapacity;
        if (available == 0) {
            available = info->ringCapacity;
        }
        if (info->streamState != 0) {
            break;
        }
        if (available > length) {
            break;
        }
    }
    if (available < length) {
        length = available;
    }
    tail = info->ringCapacity - info->ringRead;
    if (length <= tail) {
        memcpy(destination, info->ringBuffer + info->ringRead, length);
    } else {
        memcpy(destination, info->ringBuffer + info->ringRead, tail);
        memcpy(destination + tail, info->ringBuffer, length - tail);
    }
    info->ringRead = (info->ringRead + length) % info->ringCapacity;
    return length;
}

static int fillBuff(XglMovieInfo *info, int wait)
{
    unsigned char *position;
    int remaining;
    int count;
    int retry;

    remaining = info->pendingBytes;
    if (remaining != 0) {
        position = info->readBuffer + 0x4000 - remaining;
    } else {
        for (retry = 0; retry <= 0xFFFFFF; retry++) {
            count = fileRead(info);
            info->pendingBytes = count;
            if (count == 0) {
                if (wait == 0) {
                    return count;
                }
            } else {
                break;
            }
        }
        position = info->readBuffer;
        remaining = info->pendingBytes;
    }
    count = sceMpegDemuxPss(info->mpeg, position, remaining);
    info->pendingBytes -= count;
    return count;
}

static int videoCallback(int event, MpegStreamData *data, XglMovieInfo *info)
{
    int free;
    int overflow;
    int write;

    info->videoCount++;
    write = info->dmaWrite;
    free = info->dmaRead - write;
    if (free < 0) {
        free += info->dmaSize;
    }
    if ((unsigned int) free < (unsigned int) data->length) {
        return 0;
    }
    overflow = write + data->length - info->dmaSize;
    if (overflow > 0) {
        memcpy(UNCACHED(info->dmaBuffer + write), data->buffer,
               data->length - overflow);
        memcpy(UNCACHED(info->dmaBuffer), data->buffer + (data->length - overflow),
               overflow);
        info->dmaWrite = overflow;
    } else {
        memcpy(UNCACHED(info->dmaBuffer + write), data->buffer, data->length);
        info->dmaWrite += data->length;
    }
    return 1;
}

static int audioCallback(int event, MpegStreamData *data, XglMovieInfo *info)
{
    unsigned char *source;
    int length;
    int free;
    int tail;

    source = data->buffer;
    length = data->length;
    if (info->audioHeaderRead == 0) {
        info->audioSampleRate = (source[0x13] << 24) + (source[0x12] << 16) + (source[0x11] << 8) + source[0x10];
        info->audioHeaderRead = 1;
        length -= 0x2C;
        source += 0x2C;
    } else {
        source += 4;
        length -= 4;
    }
    free = info->soundRead - info->soundWrite;
    if (free <= 0) {
        free += info->soundBufferSize;
    }
    if (free <= length) {
        return 0;
    }
    tail = info->soundBufferSize - info->soundWrite;
    if (length <= tail) {
        memcpy(info->soundBuffer + info->soundWrite, source, length);
    } else {
        memcpy(info->soundBuffer + info->soundWrite, source, tail);
        memcpy(info->soundBuffer, source + tail, length - tail);
    }
    info->soundWrite = (info->soundWrite + length) % info->soundBufferSize;
    return 1;
}

static int nodataCallback(int event, void *data, XglMovieInfo *info)
{
    unsigned int *pattern;
    int i;
    int retry;
    int available;
    int length;
    unsigned int dma_address;
    int qwc;
    /* DMA channel 4 is programmed in QWC, MADR, then CHCR order. */
    volatile u32 *qwc_register;

    if (++info->missCount >= 0x41) {
        info->decodeState = 0x10;
        pattern = (unsigned int *) info->dmaBuffer;
        for (i = 0x1000 - 4; i >= 0; i -= 4) {
            *pattern = 0x100;
            pattern++;
        }
        FlushCache(0);
        qwc_register = D4_QWC;
        dma_address = (unsigned int) info->dmaBuffer;
        qwc = 0x1000;
        *qwc_register = qwc;
        *D4_MADR = dma_address;
        *D4_CHCR = 0x101;
        return 1;
    }

    info->dmaRead = info->dmaNextRead;
    available = info->dmaWrite - info->dmaRead;
    if (available < 0) {
        available += info->dmaSize;
    }
    for (retry = 0; retry <= 0xFFFFFF; retry++) {
        if (available >= 0x1000) {
            break;
        }
        if (fillBuff(info, 1) == 0) {
            available += 0xF;
            break;
        }
        available = info->dmaWrite - info->dmaRead;
        if (available < 0) {
            available += info->dmaSize;
        }
    }
    length = available & ~0xF;
    if (length > 0x1000) {
        length = 0x1000;
    }
    if (info->dmaSize < info->dmaRead + length) {
        length = info->dmaSize - info->dmaRead;
    }
    *D4_QWC = length / 16;
    *D4_MADR = (unsigned int) info->dmaBuffer + info->dmaRead;
    *D4_CHCR = 0x101;
    info->dmaNextRead = (info->dmaRead + length) % info->dmaSize;
    return 1;
}

/*
 * errorCallback (main:0x00222158) is registered as the MPEG movie decoder's
 * error handler. The original bytes read a pointer at arg1+0x04 and pass it
 * as the "%s\n" argument of D_004DC328's printf, then read, increment and
 * store back an unsigned byte at arg2+0x98 and clear a byte at arg2+0x99
 * right after it (lbu/sb at those fixed offsets).
 */
typedef struct {
    unsigned char unmodeled_00[4];
    const char *message;
} MpegErrorInfo;

typedef struct {
    unsigned char unmodeled_00[0x98];
    unsigned char errorCount;
    unsigned char errorFlag;
} MpegCallbackState;

extern int printf(const char *format, ...);

static int errorCallback(int event, MpegErrorInfo *error, MpegCallbackState *state)
{
    printf(D_004DC328, error->message);
    state->errorCount++;
    state->errorFlag = 0;
    return 1;
}

int xglMpeg2Open(XglMovieInfo *info)
{
    int result;
    int retry;

    if (xglCdStreamOpen((struct CdStreamParam *) info) < 0) {
        return -1;
    }
    xglCdStreamReadRing((struct CdStreamParam *) info, info->ringCapacity / 2);
    info->ringRead = 0;
    sceMpegInit();
    sceMpegCreate(info->mpeg, info->arena, info->reservedSize);
    sceMpegAddStrCallback(info->mpeg, 0, 0, videoCallback, info);
    sceMpegAddStrCallback(info->mpeg, 3, 0, audioCallback, info);
    sceMpegAddCallback(info->mpeg, 1, nodataCallback, info);
    sceMpegAddCallback(info->mpeg, 0, errorCallback, info);
    info->pendingBytes = 0;
    info->audioHeaderRead = 0;
    info->audioSampleRate = 0;
    info->decodeState = 0;
    info->streamOpen = 0;
    info->missCount = 0;
    info->videoCount = 0;
    for (retry = 0; retry <= 0xFFFFFF; retry++) {
        if (fillBuff(info, 1) == 0) {
            break;
        }
    }
    SsdInitVagStreamStereo(0x1000, 4);
    do {
    } while (SsdGetResultValue(&result) < 0);
    return 0;
}

static void setLoadImageTags(void *buf, int addr, int h, int w) {
    unsigned long *p;
    int x, y;

    p = UNCACHED(buf);
    p[0] = 0x10000003;
    p[2] = 0x1000000000008002;
    p[3] = 0xE;
    p[4] = 0x000E000000000000;
    p[5] = 0x50;
    p[6] = 0x0000001000000010;
    p[7] = 0x52;
    p[1] = 0;
    p += 8;
    for (y = 0; y < h; y += 16) {
        for (x = 0; x < w; x += 16) {
            p[0] = 0x10000004;
            p[1] = 0;
            p[2] = 0x1000000000008002;
            p[3] = 0xE;
            p[4] = (long)y << 32 | (long)x << 48;
            p[5] = 0x51;
            p[6] = 0;
            p[7] = 0x53;
            p[8] = 0x0800000000008040;
            p[9] = 0;
            p += 10;
            ((int *)p)[1] = addr;
            addr += 0x400;
            ((int *)p)[0] = 0x30000040;
            p[1] = 0;
            p += 2;
        }
    }
    ((int *)p)[-4] &= 0x0FFFFFFF;
}

typedef struct XglMpegView {
    int width;          /* +0x00 */
    int height;         /* +0x04 */
    int frameCount;     /* +0x08 */
    int pad0c;
    long pts;           /* +0x10 */
    long dts;           /* +0x18 */
    unsigned long flags; /* +0x20 */
} XglMpegView;
extern int sceMpegIsEnd(void *mpeg);
extern int sceMpegGetPicture(void *mpeg, void *buffer, int count);
extern void SsdPlayVagStream(int channel, int start, int loop);
extern void SsdSetVagStreamDataStereo(int channel, int address, int size);
extern int SsdGetVagStreamStatusStereo(void);
extern int sceGsSyncPath(int mode, unsigned short timeout);
extern void xglDmaDirectSrcChain(unsigned int channel, unsigned int address);
extern void nmlModelDirectSend(int mode, u8 *data, int count);
extern u64 TestEnv_0_004A7B60[];
#define MPEG(p) ((XglMpegView *) (p)->mpeg)

int xglMpeg2Play(XglMovieInfo *info)
{
    int status[1];
    int stat;
    int diff;
    int i;
    unsigned char *buf;

    if (info->decodeState >= 5 && *(int *) &info->unmodeled_09[0xf] != 0) {
        return -1;
    }
    if (sceMpegIsEnd(info->mpeg) != 0) {
        if (info->missCount != 0) {
            return -1;
        }
        return 1;
    }
    if (sceMpegGetPicture(info->mpeg, info->videoBuffer, 0x380) < 0) {
        printf("sceMpegGetPicture failed\n");
        return -1;
    }
    info->missCount = 0;
    if ((MPEG(info)->flags & 7) == 1) {
        if (info->videoCount == 0) {
            info->decodeState++;
        }
        info->videoCount = 0;
        info->streamOpen++;
        if (info->decodeState != 0 && info->streamOpen >= 5) {
            info->streamOpen = 0;
            info->decodeState--;
        }
    }
    if (MPEG(info)->frameCount == 0) {
        setLoadImageTags(info->mpegBuffer, (int) info->videoBuffer, MPEG(info)->width, MPEG(info)->height);
        if (MPEG(info)->frameCount == 0) {
            SsdPlayVagStream(0x7F, 0, (info->audioSampleRate << 12) / 48000);
            do {
            } while (SsdGetResultValue(status) < 0);
        }
    }
    diff = info->soundWrite - info->soundRead;
    if (diff <= 0) {
        diff += info->soundBufferSize;
    }
    if (diff > 0x1000) {
        SsdGetVagStreamStatusStereo();
        do {
        } while (SsdGetResultValue(&stat) < 0);
        if ((stat >> 8) != 0) {
            SsdSetVagStreamDataStereo(0, (int) (info->soundBuffer + info->soundRead), 0x1000);
            info->soundRead = (info->soundRead + 0x1000) % info->soundBufferSize;
        }
    }
    for (i = 0xF; i >= 0; i--) {
        fillBuff(info, 0);
    }
    sceGsSyncPath(0, 0);
    buf = info->mpegBuffer;
    ((u64 *) UNCACHED(buf))[4] = ((u64) sRender.w14 << 37) | ((u64) 0x8000 << 36);
    xglDmaDirectSrcChain(2, (unsigned int) buf);
    sceGsSyncPath(0, 0);
    TestEnv_0_004A7B60[8] = (0x24020000 | (sRender.w14 << 5)) | 0x2000000640000000;
    nmlModelDirectSend(5, (u8 *) TestEnv_0_004A7B60, 0xC);
    return 0;
}

/*
 * xglCdStreamClose's own parameter type (CdStreamParam, main/tu092
 * src/main/xgl_cd.c) is TU-local to that file; this TU only forwards a
 * pointer to it and does not access its members, so the tag stays opaque.
 */
typedef struct CdStreamParam CdStreamParam;

extern void SsdDisposeVagStream(void);
extern int SsdGetResultValue(int *value);
extern void SsdStopVagStream(int channel);
extern void sceMpegDelete(void *mpegHandle);
extern void sceMpegReset(void *mpegHandle);
extern int xglCdStreamClose(CdStreamParam *stream);

/*
 * arg0+0x30 (temp_16 = s1+0x30 in the original) is passed to sceMpegReset
 * and sceMpegDelete as a raw address; sceMpegReset/sceMpegDelete have no
 * public prototype (docs/naming.md), so the embedded MPEG handle stays an
 * untyped pointer computed by byte offset from the stream object.
 */
int xglMpeg2Close(unsigned char *stream)
{
    void *mpegHandle;
    int value;

    mpegHandle = stream + 0x30;
    sceMpegReset(mpegHandle);
    sceMpegDelete(mpegHandle);
    SsdStopVagStream(0);
    do {
    } while (SsdGetResultValue(&value) < 0);
    SsdDisposeVagStream();
    do {
    } while (SsdGetResultValue(&value) < 0);
    xglCdStreamClose((CdStreamParam *) stream);
    return 0;
}

void *xglMpeg2InfoInit2(void *info, void *arena, int audio_buffer_size)
{
    XglMovieInfo *movie;
    unsigned char *next;

    movie = info;
    movie->arena = arena;
    next = (unsigned char *) (((unsigned int) arena + 0xFD768 + 0x3F) & ~0x3F);
    movie->videoBuffer = next;
    next = (unsigned char *) (((unsigned int) next + 0xE0000 + 0x3F) & ~0x3F);
    movie->mpegBuffer = next;
    next = (unsigned char *) (((unsigned int) next + 0x15040 + 0x3F) & ~0x3F);
    movie->readBuffer = next;
    next = (unsigned char *) (((unsigned int) next + 0x4000 + 0x3F) & ~0x3F);
    movie->dmaBuffer = next;
    next = (unsigned char *) (((unsigned int) next + 0x80000 + 0x3F) & ~0x3F);
    movie->reservedSize = 0xFD768;
    movie->dmaSize = 0x80000;
    movie->dmaWrite = 0x10;
    movie->dmaNextRead = 0x10;
    movie->soundBuffer = next;
    next = (unsigned char *) (((unsigned int) next + 0x20000 + 0x3F) & ~0x3F);
    movie->soundBufferSize = 0x20000;
    movie->dmaRead = 0;
    movie->soundWrite = 0;
    movie->soundRead = 0;
    xglCdStreamParamInit((struct CdStreamParam *) movie);
    movie->ringBuffer = next;
    movie->ringCapacity = audio_buffer_size;
    return (void *) (((unsigned int) next + audio_buffer_size + 0x3F) & ~0x3F);
}

void xglMpeg2InfoInit(void *info, void *arena)
{
    xglMpeg2InfoInit2(info, arena, 0x100000);
}

extern int sceIpuInit(void);

int xglMovieInit(void)
{
    return sceIpuInit();
}

const char D_004DC328[] = "%s\n";

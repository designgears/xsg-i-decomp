#include "common.h"
#include "shared.h"
#include "xgl_packet.h"

extern XglPacket asPacketSource[];
extern void sceVif1PkInit(XglPacket *packet, u32 buffer_address);

extern struct { char pad[0x20]; u16 fbp; char pad2[0x1e]; } sRender;

extern XglPacket *pCurrentPacket;

extern void sceVif1PkCnt(XglPacket *p, u32 flag);
extern void sceVif1PkAddCode(XglPacket *p, u32 code);
extern void sceVif1PkAddDirectDataN(XglPacket *p, void *data, u32 n);
extern void sceVif1PkCloseDirectCode(XglPacket *p);
extern void sceVif1PkOpenDirectCode(XglPacket *packet, int mode);
extern int sceGsSetDefLoadImage(void *lp, short dbp, short dbw, short dpsm,
                                short x, short y, short w, short h);
typedef struct {
    u32 wd;
    short h;
    short pad;
    int addr;
    int size;
    int offset;
} XglTexEntry;

void xglPacketTextureTrans(void *texture)
{
    u64 load[12];
    XglTexEntry *e;
    int i;
    u32 v;
    int w;
    int dbw;
    int dbp;

    i = 0;
    e = (XglTexEntry *)((char *)texture + *(int *)((char *)texture + 0xc));
    if (*(int *)((char *)texture + 8) > 0) {
        do {
            sceVif1PkCnt(pCurrentPacket, 0);
            sceVif1PkAddCode(pCurrentPacket, 0x11000000);
            dbp = ((union { u16 h[16]; } *)&sRender)->h[13] * 2048 + e->addr;
            v = e->wd;
            w = v & 0xffff;
            if (v == w) {
                dbw = (w + 0x3f) >> 6;
            } else {
                dbw = v >> 16;
            }
            sceGsSetDefLoadImage(load,
                                 dbp >> 6,
                                 dbw, 0, 0, 0, w, e->h);
            i++;
            load[0] |= 0x8000;
            sceVif1PkOpenDirectCode(pCurrentPacket, 0);
            sceVif1PkAddDirectDataN(pCurrentPacket, load, 5);
            sceVif1PkCloseDirectCode(pCurrentPacket);
            sceVif1PkRef(pCurrentPacket, (char *)texture + e->offset, e->size, 0, 0, 0);
            e++;
        } while (i < *(int *)((char *)texture + 8));
    }
}

extern void sceVif1PkCnt(XglPacket *p, u32 flag);
extern void sceVif1PkAddCode(XglPacket *p, u32 code);
extern void sceVif1PkOpenDirectHLCode(XglPacket *p, int i);
extern void sceVif1PkAddDirectDataN(XglPacket *p, void *data, u32 n);
extern void sceVif1PkCloseDirectCode(XglPacket *p);

void xglPacketInterpolate(void)
{
    static u64 TestPrim_0_004A8D00[24] __asm__("TestPrim.0") __attribute__((aligned(16))) = {
        0xB0AB400000008001ULL, 0x53531EEEEEEULL,
        0x0ULL, 0x3FULL,
        0x6FC007FC00AULL, 0x8ULL,
        0x60ULL, 0x14ULL,
        0x0ULL, 0x6ULL,
        0x31001ULL, 0x47ULL,
        0x4000000064ULL, 0x42ULL,
        0x8000000080ULL, 0x8000000080ULL,
        0x1000000000ULL, 0x0ULL,
        0x71F700006FF8ULL, 0x0ULL,
        0x1C1000002000ULL, 0x0ULL,
        0x8DF700008FF8ULL, 0x0ULL
    };
    XglPacket *pkt = pCurrentPacket;
    u64 *spr;

    sceVif1PkCnt(pkt, 0);
    sceVif1PkAddCode(pkt, 0x11000000);
    sceVif1PkOpenDirectHLCode(pkt, 0);
    sceVif1PkAddDirectDataN(pkt, TestPrim_0_004A8D00, 4);
    spr = (u64 *)0x70000000;
    spr[1] = 6;
    spr[0] = 0x24020000 | (sRender.fbp << 5) | ((u64)0xC800 << 19);
    sceVif1PkAddDirectDataN(pkt, (void *)0x70000000, 1);
    sceVif1PkAddDirectDataN(pkt, TestPrim_0_004A8D00 + 10, 7);
    sceVif1PkCloseDirectCode(pkt);
}

extern XglPacket *pCurrentPacket;

XglPacket *xglPacketGetCurrent(void)
{
    return pCurrentPacket;
}

extern XglPacket *pSendPacket;

void xglPacketInit(void)
{
    sceVif1PkInit(&asPacketSource[0], 0x00c00000);
    sceVif1PkInit(&asPacketSource[1], 0x00e00000);

    asPacketSource[0].limit = 0x00e00000;
    asPacketSource[1].limit = 0x01000000;
    asPacketSource[0].cursor = (u8 *)0x00e00000;
    asPacketSource[1].cursor = (u8 *)0x01000000;
    pCurrentPacket = &asPacketSource[0];
    pSendPacket = 0;
}

extern void sceVif1PkEnd(void *pkt, u32 irq);
extern void sceVif1PkTerminate(void *pkt);
extern void sceVif1PkReset(void *pkt);
extern void FlushCache(int mode);
extern XglPacket D_00942228;

void xglPacketMove(void)
{
    sceVif1PkEnd(pCurrentPacket, 0);
    sceVif1PkTerminate(pCurrentPacket);
    FlushCache(0);
    *(XglPacket **)&pCurrentPacket->cursor = (XglPacket *)pCurrentPacket->limit;
    pSendPacket = pCurrentPacket;
    if (pCurrentPacket == &D_00942228) {
        pCurrentPacket = &D_00942228 - 1;
    } else {
        pCurrentPacket = &D_00942228;
    }
    sceVif1PkReset(pCurrentPacket);
}

extern XglPacket *pSendPacket;
extern void xglDmaDirectSrcChain(u32 channel, u32 address);

void xglPacketSend(void)
{
    if (pSendPacket != 0) {
        xglDmaDirectSrcChain(1, (u32)pSendPacket->start);
    }
}

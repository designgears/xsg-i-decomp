/*
 * SDK stand-ins that are not kernel or disc: SIF/IOP, overlay loading, GS,
 * VIF1 packet builder, pad, memory card, IPU/MPEG, EE soft-float helpers and
 * a few libc extras.  Graphics/input/sound are logging no-ops for now.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "xsg_rt.h"

typedef union ps2_u128 {
    unsigned int w[4];
    unsigned long long d[2];
    float f[4];
    unsigned char b[16];
} __attribute__((aligned(16))) ps2_u128;

/* ---- generated stubs ----------------------------------------------- */
void xsg_autostub_hit(const char *name, unsigned *count, void *ra)
{
    ++*count;
    if (xsg_log_level >= 2 || (xsg_log_level >= 1 && (*count <= XSG_LOG_FIRST || *count % 1000 == 0)))
        xsg_logf("[stub %s #%u] (auto) called from %p\n", name, *count, ra);
}

/* ---- EE soft-float (libgcc fp-bit names used directly by game code) --- */
double fptodp(float f) { return f; }
float dptofp(double d) { return (float)d; }
double dpadd(double a, double b) { return a + b; }
double dpsub(double a, double b) { return a - b; }
double dpmul(double a, double b) { return a * b; }
double dpdiv(double a, double b) { return a / b; }
double litodp(long l) { return (double)l; }
/* mini_g.c (ov11) declares these with a u64 result; it gets the double's bits. */
unsigned long long xsg_dpmul_bits(double a, double b) { union { double d; unsigned long long u; } v; v.d = a * b; return v.u; }
unsigned long long xsg_litodp_bits(long l) { union { double d; unsigned long long u; } v; v.d = (double)l; return v.u; }
long dptoli(double d) { return (long)d; }
int dpcmp(double a, double b) { return a < b ? -1 : a > b; }

/* gnu89 `extern inline` in menu_para_pt_rate_get.c has no out-of-line body */
float ldf_off(void *base, unsigned int o) { return *(float *)((char *)base + o); }

char *strlwr(char *s)
{
    for (char *p = s; *p; p++)
        *p = tolower((unsigned char)*p);
    return s;
}

/* ---- SIF / IOP: iop.c ---------------------------------------------- */

/* Overlay load: "cdrom0:\\OV01.OVL;1" -> activate the linked-in overlay. */
int sceSifLoadElf(const char *name, void *info)
{
    const char *p = strstr(name, "OV");
    int id = p ? atoi(p + 2) : -1;
    xsg_logf("[sif] LoadElf %s -> overlay %d\n", name, id);
    if (info)
        memset(info, 0, 16);
    return xsg_overlay_activate(id);
}

/* ---- GS ------------------------------------------------------------- */
void sceGsResetGraph(int a, int b, int c, int d) { STUB_LOG("%d %d %d %d", a, b, c, d); }
void sceGsResetPath(void) { STUB_LOG(""); }
void sceGsSetDefDispEnv(void *e, int psm, int w, int h, int dx, int dy) { STUB_LOG("psm=%d %dx%d", psm, w, h); (void)e; (void)dx; (void)dy; }
void sceGsPutDispEnv(void *e) { STUB_LOG("%p", e); }
int sceGsSetDefDrawEnv(void *e, int psm, int w, int h, int zt, int zp) { STUB_LOG("psm=%d %dx%d", psm, w, h); (void)e; (void)zt; (void)zp; return 8; }
int sceGsPutDrawEnv(void *e) { STUB_LOG("%p", e); return 0; }
void sceGsSetDefLoadImage(void *li, int bp, int bw, int psm, int x, int y, int w, int h) { STUB_LOG("%dx%d", w, h); (void)li; (void)bp; (void)bw; (void)psm; (void)x; (void)y; }
void sceGsSetDefStoreImage(void *si, int bp, int bw, int psm, int x, int y, int w, int h) { STUB_LOG("%dx%d", w, h); (void)si; (void)bp; (void)bw; (void)psm; (void)x; (void)y; }
int sceGsExecLoadImage(void *li, void *src) { STUB_LOG("%p %p", li, src); return 0; }
int sceGsExecStoreImage(void *si, void *dst) { STUB_LOG("%p %p", si, dst); return 0; }
int sceGsSyncPath(int mode, unsigned short timeout) { (void)mode; (void)timeout; return 0; }

/* ---- VIF1 packet builder (libvifpk semantics, nothing consumes it) --- */
typedef struct {
    unsigned *pCurrent;
    ps2_u128 *pBase;
    ps2_u128 *pDmaTag;
    unsigned *pVifCode;
    unsigned numlen;
    unsigned *pGifTag;
} sceVif1Packet;

void sceVif1PkInit(sceVif1Packet *p, ps2_u128 *base)
{
    p->pBase = base;
    p->pCurrent = (unsigned *)base;
    p->pDmaTag = 0;
    p->pVifCode = 0;
    p->pGifTag = 0;
}
void sceVif1PkReset(sceVif1Packet *p)
{
    p->pCurrent = (unsigned *)p->pBase;
    p->pDmaTag = 0;
    p->pVifCode = 0;
}
unsigned *sceVif1PkReserve(sceVif1Packet *p, unsigned n) { unsigned *r = p->pCurrent; p->pCurrent += n; return r; }
void sceVif1PkAddCode(sceVif1Packet *p, unsigned code) { *p->pCurrent++ = code; }
void sceVif1PkAddData(sceVif1Packet *p, unsigned d) { *p->pCurrent++ = d; }
void sceVif1PkAddDataN(sceVif1Packet *p, const unsigned *d, unsigned n)
{
    memcpy(p->pCurrent, d, n * 4);
    p->pCurrent += n;
}
void sceVif1PkAddDirectDataN(sceVif1Packet *p, const ps2_u128 *d, unsigned n)
{
    memcpy(p->pCurrent, d, n * 16);
    p->pCurrent += n * 4;
}
void sceVif1PkAddUpkData128(sceVif1Packet *p, ps2_u128 d) { memcpy(p->pCurrent, &d, 16); p->pCurrent += 4; }
void sceVif1PkAddUpkData128N(sceVif1Packet *p, const ps2_u128 *d, unsigned n) { sceVif1PkAddDirectDataN(p, d, n); }
void sceVif1PkAddGsData(sceVif1Packet *p, unsigned long long d) { memcpy(p->pCurrent, &d, 8); p->pCurrent += 2; }
void sceVif1PkAddGsAD(sceVif1Packet *p, unsigned addr, unsigned long long d)
{
    memcpy(p->pCurrent, &d, 8);
    p->pCurrent[2] = addr;
    p->pCurrent[3] = 0;
    p->pCurrent += 4;
}
void sceVif1PkAddGsPacked(sceVif1Packet *p, ps2_u128 d) { memcpy(p->pCurrent, &d, 16); p->pCurrent += 4; }
static void align_qw(sceVif1Packet *p)
{
    while (((uintptr_t)p->pCurrent & 15) != 0)
        *p->pCurrent++ = 0;
}
void sceVif1PkCnt(sceVif1Packet *p, unsigned opt)
{
    (void)opt;
    align_qw(p);
    p->pDmaTag = (ps2_u128 *)p->pCurrent;
    p->pCurrent[0] = 0x10000000;
    p->pCurrent[1] = 0;
    p->pCurrent += 2;
}
void sceVif1PkRef(sceVif1Packet *p, void *addr, unsigned size, unsigned o1, unsigned o2, unsigned f)
{
    (void)f;
    align_qw(p);
    p->pDmaTag = (ps2_u128 *)p->pCurrent;
    p->pCurrent[0] = 0x30000000 | size;
    p->pCurrent[1] = (unsigned)(uintptr_t)addr;
    p->pCurrent[2] = o1;
    p->pCurrent[3] = o2;
    p->pCurrent += 4;
}
void sceVif1PkRefLoadImage(sceVif1Packet *p, unsigned short bp, unsigned char psm, unsigned short bw,
                           void *img, unsigned size, unsigned x, unsigned y, unsigned w, unsigned h)
{
    (void)bp; (void)psm; (void)bw; (void)img; (void)size; (void)x; (void)y; (void)w; (void)h;
    align_qw(p);
    memset(p->pCurrent, 0, 4 * 20);
    p->pCurrent += 20;
}
void sceVif1PkEnd(sceVif1Packet *p, unsigned opt)
{
    (void)opt;
    align_qw(p);
    p->pDmaTag = (ps2_u128 *)p->pCurrent;
    p->pCurrent[0] = 0x70000000;
    p->pCurrent[1] = 0;
    p->pCurrent += 2;
}
unsigned sceVif1PkTerminate(sceVif1Packet *p) { align_qw(p); return ((char *)p->pCurrent - (char *)p->pBase) / 16; }
unsigned sceVif1PkSize(sceVif1Packet *p) { return ((char *)p->pCurrent - (char *)p->pBase + 15) / 16; }
void sceVif1PkAlign(sceVif1Packet *p, unsigned bit, unsigned pos)
{
    unsigned mask = (1u << bit) - 1;
    while ((((uintptr_t)p->pCurrent >> 2) & mask) != pos)
        *p->pCurrent++ = 0;
}
void sceVif1PkOpenDirectCode(sceVif1Packet *p, int i) { (void)i; p->pVifCode = p->pCurrent++; }
void sceVif1PkOpenDirectHLCode(sceVif1Packet *p, int i) { (void)i; p->pVifCode = p->pCurrent++; }
static unsigned close_code(sceVif1Packet *p)
{
    if (!p->pVifCode)
        return 0;
    unsigned qw = ((char *)p->pCurrent - (char *)(p->pVifCode + 1)) / 16;
    p->pVifCode = 0;
    return qw;
}
unsigned sceVif1PkCloseDirectCode(sceVif1Packet *p) { return close_code(p); }
unsigned sceVif1PkCloseDirectHLCode(sceVif1Packet *p) { return close_code(p); }
void sceVif1PkOpenUpkCode(sceVif1Packet *p, unsigned short addr, unsigned mode, int cl, int wl, int usn, int flg)
{
    (void)addr; (void)mode; (void)cl; (void)wl; (void)usn; (void)flg;
    p->pVifCode = p->pCurrent++;
}
unsigned sceVif1PkCloseUpkCode(sceVif1Packet *p) { return close_code(p); }
void sceVif1PkOpenGifTag(sceVif1Packet *p, ps2_u128 tag)
{
    p->pGifTag = p->pCurrent;
    memcpy(p->pCurrent, &tag, 16);
    p->pCurrent += 4;
}
unsigned sceVif1PkCloseGifTag(sceVif1Packet *p) { p->pGifTag = 0; return 0; }

/* ---- pad -------------------------------------------------------------- */
int scePadInit(int m) { STUB_LOG("%d", m); return 1; }
int scePadPortOpen(int port, int slot, void *buf) { STUB_LOG("%d %d %p", port, slot, buf); return 1; }
int scePadGetState(int port, int slot) { (void)port; (void)slot; return 6; /* stable */ }
int scePadGetReqState(int port, int slot) { (void)port; (void)slot; return 0; /* complete */ }
int scePadRead(int port, int slot, unsigned char *data)
{
    (void)slot;
    memset(data, 0xff, 32);
    data[0] = 0;
    data[1] = 0x73; /* DualShock 2 analog */
    data[4] = data[5] = data[6] = data[7] = 0x80;
    for (int i = 8; i < 20; i++)
        data[i] = 0;
    if (port == 0) {
        unsigned m = xsg_pad_script_buttons();
        data[2] &= ~(m >> 8);  /* buttons are active low; SCE_PAD masks are */
        data[3] &= ~(m & 0xff); /* 0xffff ^ (data[2] << 8 | data[3]) */
    }
    return 32;
}

/* XSG_PAD="frame:mask[:hold],..." presses SCE_PAD button masks (0x0800
 * START, 0x0040 CROSS, 0x0020 CIRCLE, 0x1000 UP, 0x4000 DOWN...) on port 0
 * from vblank `frame` for `hold` frames (default 6). */
unsigned xsg_pad_script_buttons(void)
{
    static struct { unsigned frame, mask, hold; } ev[64];
    static int nev = -1;
    if (nev < 0) {
        nev = 0;
        const char *s = getenv("XSG_PAD");
        while (s && *s && nev < 64) {
            char *e;
            ev[nev].frame = strtoul(s, &e, 0);
            if (*e != ':')
                break;
            ev[nev].mask = strtoul(e + 1, &e, 0);
            ev[nev].hold = 6;
            if (*e == ':')
                ev[nev].hold = strtoul(e + 1, &e, 0);
            nev++;
            s = *e == ',' ? e + 1 : e;
        }
        if (nev)
            xsg_logf("[pad] %d scripted presses\n", nev);
    }
    unsigned m = 0;
    for (int i = 0; i < nev; i++)
        if (xsg_frame >= ev[i].frame && xsg_frame < ev[i].frame + ev[i].hold)
            m |= ev[i].mask;
    return m;
}
int scePadInfoMode(int port, int slot, int term, int offs) { (void)port; (void)slot; (void)offs; return term == 1 ? 7 : term == 2 ? 1 : 0; }
int scePadSetMainMode(int port, int slot, int mode, int lock) { STUB_LOG("%d %d", mode, lock); (void)port; (void)slot; return 1; }
int scePadInfoAct(int port, int slot, int act, int term) { (void)port; (void)slot; (void)act; (void)term; return 0; }
int scePadSetActAlign(int port, int slot, const unsigned char *d) { (void)port; (void)slot; (void)d; return 1; }
int scePadSetActDirect(int port, int slot, const unsigned char *d) { (void)port; (void)slot; (void)d; return 1; }
int scePadInfoPressMode(int port, int slot) { (void)port; (void)slot; return 1; }
int scePadEnterPressMode(int port, int slot) { (void)port; (void)slot; return 1; }

/* memory card: memcard.c */

/* ---- IPU / MPEG: every movie ends immediately ------------------------ */
void sceIpuInit(void) { STUB_LOG(""); }
/* No IPU: report a timeout, so xglMoviePlay returns -1 and the movie ends. */
int sceIpuSync(int mode, unsigned short timeout) { (void)mode; (void)timeout; return -1; }
int sceMpegInit(void) { STUB_LOG(""); return 0; }
int sceMpegCreate(void *mp, void *work, int size) { STUB_LOG("%p %p %d", mp, work, size); return 0; }
int sceMpegDelete(void *mp) { (void)mp; return 0; }
int sceMpegReset(void *mp) { (void)mp; return 0; }
int sceMpegIsEnd(void *mp) { (void)mp; return 1; }
int sceMpegGetPicture(void *mp, void *rgb, int mbcount) { STUB_LOG("-> end of stream"); (void)mp; (void)rgb; (void)mbcount; return -1; }
int sceMpegAddCallback(void *mp, int type, void *cb, void *data) { (void)mp; (void)type; (void)cb; (void)data; return 0; }
int sceMpegAddStrCallback(void *mp, int type, int ch, void *cb, void *data) { (void)mp; (void)type; (void)ch; (void)cb; (void)data; return 0; }
int sceMpegDemuxPss(void *mp, void *buf, int size) { (void)mp; (void)buf; return size; }

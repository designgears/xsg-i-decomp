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
/* Stage 5: the display environment goes to the GS privileged registers (plain
 * memory at 0x12000000, read by the screenshot code), draw environments and
 * image loads go to the software GS (gs_bridge.cpp). */
void xsg_gs_send_packet(const void *p);
void xsg_gs_send(const void *p, uint32_t qwords);
void xsg_dma_poll(void);
#define GS_PRIV(off) (*(volatile uint64_t *)(uintptr_t)(PS2_GSPRIV_BASE + (off)))
void sceGsSetDefDispEnv(void *e, int psm, int w, int h, int dx, int dy)
{
    uint64_t *d = e;
    int magh = w ? 2560 / w - 1 : 4;
    STUB_LOG("psm=%d %dx%d", psm, w, h);
    d[0] = 0x3 | (1ull << 2) /* MMOD */ | (0x80ull << 8);               /* PMODE EN1 EN2 */
    d[1] = 1;                                                             /* SMODE2 INT, frame mode */
    d[2] = ((uint64_t)((w + 63) / 64) << 9) | ((uint64_t)(psm & 31) << 15); /* DISPFB */
    d[3] = (uint64_t)(636 + dx * (magh + 1)) | ((uint64_t)(50 + dy) << 12) | ((uint64_t)magh << 23) |
           ((uint64_t)(w * (magh + 1) - 1) << 32) | ((uint64_t)(h - 1) << 44); /* DISPLAY */
    d[4] = 0;                                                             /* BGCOLOR */
}
void sceGsPutDispEnv(void *e)
{
    uint64_t *d = e;
    STUB_LOG("%p", e);
    GS_PRIV(0x00) = d[0];
    GS_PRIV(0x20) = d[1];
    GS_PRIV(0x70) = d[2];
    GS_PRIV(0x80) = d[3];
    GS_PRIV(0xE0) = d[4];
}
int sceGsSetDefDrawEnv(void *e, int psm, int w, int h, int zt, int zp) { STUB_LOG("psm=%d %dx%d", psm, w, h); (void)e; (void)zt; (void)zp; return 8; }
int sceGsPutDrawEnv(void *e) { STUB_LOG("%p", e); xsg_gs_send_packet(e); return 0; }
static void def_image(void *li, int bp, int bw, int psm, int x, int y, int w, int h, int dir)
{
    uint64_t *q = li;
    static const int bpp[64] = {[0] = 32, [1] = 24, [2] = 16, [0xA] = 16, [0x13] = 8, [0x14] = 4,
                                [0x1B] = 32, [0x24] = 32, [0x2C] = 32, [0x30] = 32, [0x31] = 24,
                                [0x32] = 16, [0x3A] = 16};
    uint64_t bits = (uint64_t)w * h * (bpp[psm & 63] ? bpp[psm & 63] : 32);
    q[0] = 4 | (1ull << 60); q[1] = 0xE;                                    /* GIFtag A+D x4 */
    q[2] = dir ? ((uint64_t)bp | (uint64_t)bw << 16 | (uint64_t)psm << 24)
               : ((uint64_t)bp << 32 | (uint64_t)bw << 48 | (uint64_t)psm << 56);
    q[3] = 0x50;
    q[4] = dir ? ((uint64_t)x | (uint64_t)y << 16) : ((uint64_t)x << 32 | (uint64_t)y << 48);
    q[5] = 0x51;
    q[6] = (uint64_t)w | (uint64_t)h << 32; q[7] = 0x52;
    q[8] = dir; q[9] = 0x53;
    q[10] = ((bits + 127) / 128) | (1ull << 15) | (2ull << 58);             /* IMAGE, EOP */
    q[11] = 0;
}
void sceGsSetDefLoadImage(void *li, int bp, int bw, int psm, int x, int y, int w, int h) { STUB_LOG("%dx%d", w, h); def_image(li, bp, bw, psm, x, y, w, h, 0); }
void sceGsSetDefStoreImage(void *si, int bp, int bw, int psm, int x, int y, int w, int h) { STUB_LOG("%dx%d", w, h); def_image(si, bp, bw, psm, x, y, w, h, 1); }
int sceGsExecLoadImage(void *li, void *src)
{
    uint64_t *q = li;
    STUB_LOG("%p %p", li, src);
    xsg_dma_poll();
    xsg_gs_send(q, 6);
    xsg_gs_send(src, (uint32_t)(q[10] & 0x7fff));
    return 0;
}
int sceGsExecStoreImage(void *si, void *dst) { STUB_LOG("%p %p (GS->host not modelled)", si, dst); return 0; }
int sceGsSyncPath(int mode, unsigned short timeout) { (void)mode; (void)timeout; return 0; }

/* ---- VIF1 packet builder (libvifpk semantics; gs_bridge.cpp consumes it) --- */
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
/* Close the open CNT/END tag: its QWC is the data written since it. */
static void close_tag(sceVif1Packet *p)
{
    align_qw(p);
    if (p->pDmaTag) {
        unsigned *t = (unsigned *)p->pDmaTag;
        unsigned qwc = (unsigned)(((char *)p->pCurrent - (char *)t) / 16 - 1);
        t[0] = (t[0] & 0xffff0000u) | (qwc & 0xffff);
        p->pDmaTag = 0;
    }
}
static void open_tag(sceVif1Packet *p, unsigned id, unsigned opt)
{
    close_tag(p);
    p->pDmaTag = (ps2_u128 *)p->pCurrent;
    p->pCurrent[0] = id | (opt ? 0x80000000u : 0);
    p->pCurrent[1] = 0;
    p->pCurrent += 2; /* the tag's upper half: two VIF codes the caller adds */
}
void sceVif1PkCnt(sceVif1Packet *p, unsigned opt) { open_tag(p, 0x10000000, opt); }
void sceVif1PkEnd(sceVif1Packet *p, unsigned opt) { open_tag(p, 0x70000000, opt); }
void sceVif1PkRef(sceVif1Packet *p, void *addr, unsigned size, unsigned o1, unsigned o2, unsigned f)
{
    close_tag(p);
    p->pCurrent[0] = 0x30000000 | (size & 0xffff) | (f ? 0x80000000u : 0);
    p->pCurrent[1] = (unsigned)(uintptr_t)addr;
    p->pCurrent[2] = o1;
    p->pCurrent[3] = o2;
    p->pCurrent += 4;
}
/* CNT tag with DIRECT 6 (A+D BITBLTBUF/TRXPOS/TRXREG/TRXDIR + IMAGE GIFtag), then REF to the image. */
void sceVif1PkRefLoadImage(sceVif1Packet *p, unsigned short bp, unsigned char psm, unsigned short bw,
                           void *img, unsigned size, unsigned x, unsigned y, unsigned w, unsigned h)
{
    unsigned long long q[12];
    close_tag(p);
    p->pCurrent[0] = 0x10000006;
    p->pCurrent[1] = 0;
    p->pCurrent[2] = 0;
    p->pCurrent[3] = 0x50000006;
    q[0] = 4 | (1ull << 60); q[1] = 0xE;
    q[2] = (unsigned long long)bp << 32 | (unsigned long long)bw << 48 | (unsigned long long)psm << 56; q[3] = 0x50;
    q[4] = (unsigned long long)x << 32 | (unsigned long long)y << 48; q[5] = 0x51;
    q[6] = w | (unsigned long long)h << 32; q[7] = 0x52;
    q[8] = 0; q[9] = 0x53;
    q[10] = (size & 0x7fff) | (1ull << 15) | (2ull << 58); q[11] = 0;
    memcpy(p->pCurrent + 4, q, sizeof q);
    p->pCurrent += 4 + 24;
    p->pCurrent[0] = 0x30000000 | (size & 0xffff);
    p->pCurrent[1] = (unsigned)(uintptr_t)img;
    p->pCurrent[2] = 0;
    p->pCurrent[3] = 0x50000000 | (size & 0xffff);
    p->pCurrent += 4;
}
unsigned sceVif1PkTerminate(sceVif1Packet *p) { close_tag(p); return ((char *)p->pCurrent - (char *)p->pBase) / 16; }
unsigned sceVif1PkSize(sceVif1Packet *p) { return ((char *)p->pCurrent - (char *)p->pBase + 15) / 16; }
void sceVif1PkAlign(sceVif1Packet *p, unsigned bit, unsigned pos)
{
    unsigned mask = (1u << bit) - 1;
    while ((((uintptr_t)p->pCurrent >> 2) & mask) != pos)
        *p->pCurrent++ = 0;
}
void sceVif1PkOpenDirectCode(sceVif1Packet *p, int i) { p->pVifCode = p->pCurrent; *p->pCurrent++ = 0x50000000u | (i ? 0x80000000u : 0); }
void sceVif1PkOpenDirectHLCode(sceVif1Packet *p, int i) { p->pVifCode = p->pCurrent; *p->pCurrent++ = 0x51000000u | (i ? 0x80000000u : 0); }
static unsigned close_direct(sceVif1Packet *p)
{
    unsigned qw;
    if (!p->pVifCode)
        return 0;
    qw = (unsigned)(((char *)p->pCurrent - (char *)(p->pVifCode + 1)) / 16);
    *p->pVifCode = (*p->pVifCode & 0xffff0000u) | (qw & 0xffff);
    p->pVifCode = 0;
    return qw;
}
unsigned sceVif1PkCloseDirectCode(sceVif1Packet *p) { return close_direct(p); }
unsigned sceVif1PkCloseDirectHLCode(sceVif1Packet *p) { return close_direct(p); }
/* UNPACK code: cmd (vn/vl/m bits), VU address; NUM is set on close from the data size. */
void sceVif1PkOpenUpkCode(sceVif1Packet *p, unsigned short addr, unsigned mode, int cl, int wl, int usn, int flg)
{
    (void)cl; (void)wl; (void)usn; (void)flg;
    p->pVifCode = p->pCurrent;
    *p->pCurrent++ = ((mode & 0xff) << 24) | (addr & 0xffff);
}
unsigned sceVif1PkCloseUpkCode(sceVif1Packet *p)
{
    unsigned code, vn, vl, bits, words, num;
    if (!p->pVifCode)
        return 0;
    code = *p->pVifCode;
    vn = (code >> 26) & 3; vl = (code >> 24) & 3;
    bits = (vn == 3 && vl == 3) ? 16 : (32u >> vl) * (vn + 1);
    words = (unsigned)(p->pCurrent - (p->pVifCode + 1));
    num = words * 32 / bits;
    *p->pVifCode = (code & 0xff00ffffu) | ((num & 0xff) << 16);
    p->pVifCode = 0;
    return num;
}
void sceVif1PkOpenGifTag(sceVif1Packet *p, ps2_u128 tag)
{
    p->pGifTag = p->pCurrent;
    memcpy(p->pCurrent, &tag, 16);
    p->pCurrent += 4;
}
/* NLOOP from the data written since the tag. */
unsigned sceVif1PkCloseGifTag(sceVif1Packet *p)
{
    unsigned *t = p->pGifTag, qw, nreg, flg, nloop;
    if (!t)
        return 0;
    qw = (unsigned)((p->pCurrent - (t + 4)) / 4);
    nreg = (t[1] >> 28) & 15;
    if (!nreg)
        nreg = 16;
    flg = (t[1] >> 26) & 3;
    nloop = flg == 0 ? qw / nreg : flg == 1 ? qw * 2 / nreg : qw;
    t[0] = (t[0] & ~0x7fffu) | (nloop & 0x7fff);
    p->pGifTag = 0;
    return nloop;
}

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

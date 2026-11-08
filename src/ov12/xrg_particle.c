/*
 * OV12 original TU 84: 0x00a48c08..0x00a49f70 (20 functions)
 */
#include "common.h"
#include "shared.h"
#include "xrg_particle.h"
#include "ov12/rg_draw.h"

extern void assert_prog(const char *expression, const char *source_file,
                        int line);

/*
 * _openVifGif and _openVifGifAD build the 128-bit VIF1 GIF tag
 * sceVif1PkOpenGifTag takes in the single register the original loads with
 * one `lq`; no wide arithmetic is done on it: bit 15 is EOP, bit 46 is PRE,
 * bits 47..57 are PRIM, bits 60..63 are NREG, and the high 64 bits are the
 * REGS descriptor. The union lets the two halves be stored as ordinary
 * 64-bit fields and the whole 16 bytes be read back as the single register
 * sceVif1PkOpenGifTag takes.
 */
typedef unsigned int Quadword __attribute__((mode(TI)));

typedef union XrgParticleGifTag {
    struct {
        u64 lo;
        u64 hi;
    } part;
    Quadword quad;
} XrgParticleGifTag;

extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkAlign(XglPacket *packet, int align, int size);
extern void sceVif1PkOpenDirectCode(XglPacket *packet, int mode);
extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);
extern void sceVif1PkOpenGifTag(XglPacket *packet, Quadword tag);

static void _openVifGif(XglPacket *packet, unsigned int prim,
                        unsigned int nreg, u64 regs)
{
    XrgParticleGifTag tag;

    tag.part.lo = (1u << 15) | (1ULL << 46) | ((u64)prim << 47) |
                  ((u64)nreg << 60);
    tag.part.hi = regs;
    sceVif1PkAlign(packet, 2, 3);
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkOpenGifTag(packet, tag.quad);
}

/*
 * ov12:0x00a58f10 is a fixed GIF tag: EOP set, NREG 1, REGS 0xE (the A+D
 * address+data register code), the constant _openVifGifAD passes to
 * sceVif1PkOpenGifTag.
 */
const XrgParticleGifTag D_00A58F10 = {
    .part = { 0x1000000000008000ULL, 0x000000000000000EULL }
};
const char D_00A58F20[] = "pDrv != NIL";
const char D_00A58F30[] = "../xrg_particle.euc.c";

static void _openVifGifAD(XglPacket *packet)
{
    XrgParticleGifTag tag;

    tag = D_00A58F10;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAlign(packet, 2, 3);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, tag.quad);
}

/*
 * Both libvifpk closers take the packet being built: _closeVifGif keeps its
 * argument in $a0 for sceVif1PkCloseGifTag and restores it from $s0 for
 * sceVif1PkCloseDirectHLCode (the same helper and prototypes as
 * src/ov12/xrg_paint2d.c).
 */
typedef struct sceVif1Packet sceVif1Packet;
extern void sceVif1PkCloseGifTag(sceVif1Packet *packet);
extern void sceVif1PkCloseDirectHLCode(sceVif1Packet *packet);

static void _closeVifGif(sceVif1Packet *packet)
{
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectHLCode(packet);
}

static void _InitDriver(XrgParticleDriver *driver)
{
    if (driver == 0) {
        assert_prog(D_00A58F20, D_00A58F30, 128);
    }
    driver->params0 = 0;
    driver->params1 = 0;
    driver->m_uReqCalcNum = 0;
    driver->m_uReqDispNum = 0;
}

static void _DisposeDriver(XrgParticleDriver *driver)
{
    if (driver == 0) {
        assert_prog(D_00A58F20, D_00A58F30, 138);
    }
}

#define XRG_PARTICLE_SIZE_MAX 100.0f
#define XRG_PI 3.1415927f
#define XRG_TWO_PI 6.2831855f
typedef union XrgParticleRow {
    Quadword quad;
    int word[4];
    RgVector vec;
} XrgParticleRow;
struct XrgParticle {
    XrgParticleRow position;
    XrgParticleRow velocity;
    XrgParticleRow acceleration;
    XrgParticleRow appearance;
    XrgParticleRow appearanceRate;
};
static inline void _Damp(float *v, float factor)
{
    v[0] *= factor;
    v[1] *= factor;
    v[2] *= factor;
    v[3] *= factor;
}
static inline float *_ScaleFor(float *dst, float *tmp, float *src, float elapsed)
{
    tmp[0] = src[0] * elapsed;
    tmp[1] = src[1] * elapsed;
    tmp[2] = src[2] * elapsed;
    tmp[3] = src[3] * elapsed;
    return dst;
}
static inline float _WrapAngle(float angle)
{
    while (XRG_PI < angle) {
        angle -= XRG_TWO_PI;
    }
    while (angle < -XRG_PI) {
        angle += XRG_TWO_PI;
    }
    return angle;
}
static inline void _ClampState(XrgParticle *e, float zero, float sizeMax, float one)
{
    e->appearance.vec[0] = (e->appearance.vec[0] < zero)
        ? 0.0f : ((sizeMax < e->appearance.vec[0]) ? XRG_PARTICLE_SIZE_MAX : e->appearance.vec[0]);
    e->appearance.vec[1] = (e->appearance.vec[1] < zero)
        ? 0.0f : ((one < e->appearance.vec[1]) ? 1.0f : e->appearance.vec[1]);
    e->appearance.vec[2] = _WrapAngle(e->appearance.vec[2]);
    e->appearance.vec[3] = _WrapAngle(e->appearance.vec[3]);
}

static void _Calc(XrgParticle *particle, int count, float rate,
                  float elapsed)
{
    float tmp[4];
    float factor;
    float *scratch;
    int old;
    XrgParticle *e = particle;
    float zero = 0.0f;
    float sizeMax = XRG_PARTICLE_SIZE_MAX;
    float one = 1.0f;

    if (rate <= zero) {
        for (;;) {
            old = count;
            count--;
            if (old == 0) {
                break;
            }
            scratch = tmp;
            {
                float *dst = _ScaleFor(e->velocity.vec, tmp, e->acceleration.vec, elapsed);

                __asm__ __volatile__("lqc2 vf30, 0(%1)\n" "lqc2 vf31, 0(%2)\n" "vadd.xyzw vf30xyzw, vf30xyzw, vf31xyzw\n" "sqc2 vf30, 0(%0)\n" : : "r" (dst), "r" (dst), "r" (scratch) : "memory");
            }
            {
                float *dst = _ScaleFor(e->position.vec, tmp, e->velocity.vec, elapsed);

                __asm__ __volatile__("lqc2 vf30, 0(%1)\n" "lqc2 vf31, 0(%2)\n" "vadd.xyzw vf30xyzw, vf30xyzw, vf31xyzw\n" "sqc2 vf30, 0(%0)\n" : : "r" (dst), "r" (dst), "r" (scratch) : "memory");
            }
            {
                float *dst = _ScaleFor(e->appearance.vec, tmp, e->appearanceRate.vec, elapsed);

                __asm__ __volatile__("lqc2 vf30, 0(%1)\n" "lqc2 vf31, 0(%2)\n" "vadd.xyzw vf30xyzw, vf30xyzw, vf31xyzw\n" "sqc2 vf30, 0(%0)\n" : : "r" (dst), "r" (dst), "r" (scratch) : "memory");
            }
            _ClampState(e, zero, sizeMax, one);
            e++;
        }
        return;
    }
    factor = one - rate * elapsed;
    for (; (unsigned int) count-- != 0;) {
        {
            float *dst = _ScaleFor(e->velocity.vec, tmp, e->acceleration.vec, elapsed);

            __asm__ __volatile__("lqc2 vf30, 0(%1)\n" "lqc2 vf31, 0(%2)\n" "vadd.xyzw vf30xyzw, vf30xyzw, vf31xyzw\n" "sqc2 vf30, 0(%0)\n" : : "r" (dst), "r" (dst), "r" (tmp) : "memory");
        }
        {
            float *dst = _ScaleFor(e->position.vec, tmp, e->velocity.vec, elapsed);

            __asm__ __volatile__("lqc2 vf30, 0(%1)\n" "lqc2 vf31, 0(%2)\n" "vadd.xyzw vf30xyzw, vf30xyzw, vf31xyzw\n" "sqc2 vf30, 0(%0)\n" : : "r" (dst), "r" (dst), "r" (tmp) : "memory");
        }
        {
            float *dst = _ScaleFor(e->appearance.vec, tmp, e->appearanceRate.vec, elapsed);

            __asm__ __volatile__("lqc2 vf30, 0(%1)\n" "lqc2 vf31, 0(%2)\n" "vadd.xyzw vf30xyzw, vf30xyzw, vf31xyzw\n" "sqc2 vf30, 0(%0)\n" : : "r" (dst), "r" (dst), "r" (tmp) : "memory");
        }
        _Damp(e->velocity.vec, factor);
        _ClampState(e, zero, sizeMax, one);
        e++;
    }
}

typedef union XrgParticleRectQ {
    Quadword q;
    int i[4];
} XrgParticleRectQ;

static RgDrawView *s_pCurView_00A511F4 = 0;

#define SCE_GS_PABE 0x49
#define SCE_GS_TEXFLUSH 0x3F
#define SCE_GS_TEST_1 0x47
#define SCE_GS_CLAMP_1 0x08
#define SCE_GS_ALPHA_1 0x42
#define SCE_GS_TEX0_1 0x06
#define GS_PRIM_TRISTRIP 4
#define GS_PRIM_TME 0x10
#define GS_PRIM_ABE 0x40
#define GS_PRIM_FST 0x100
#define SCRATCHPAD_BASE 0x70000000
#define XRG_PARTICLE_DISP_BATCH 32
#define XRG_PARTICLE_DEPTH_LIMIT 28000.0f
#define XRG_PARTICLE_SPRITE_HALF_MAX 2047u
#define XRG_PARTICLE_GS_COORD_LIMIT 0x10000
#define XRG_PARTICLE_SPRITE_PRIM (GS_PRIM_TRISTRIP | GS_PRIM_TME | GS_PRIM_ABE | GS_PRIM_FST)
#define XRG_PARTICLE_SPRITE_NREG 9
#define XRG_PARTICLE_SPRITE_REGS 0x434343431ULL
typedef Vector4 XrgDispVec4 __attribute__((aligned(8)));
extern void sceVif1PkInit(XglPacket *packet, unsigned int base);
extern int sceVif1PkSize(XglPacket *packet);
extern void sceVif1PkAddGsAD(XglPacket *packet, int reg, u64 value);
extern void sceVif1PkAddGsPacked(XglPacket *packet, Quadword value);
extern void nmlModelDirectSend(int mode, const void *data, int count);
extern void XrgUnitVector(RgVector vector);
extern float cosf(float);
static const XrgDispVec4 D_00A58F50 __attribute__((aligned(16))) = { 128.0f, 128.0f, 128.0f, 128.0f };
const char D_00A58F60[] = "sizeof(D128) == 16";

static void _Disp(struct XrgParticle *pPoints, int count, long long tex0, XrgParticleRectQ *rect)
{
    float depthLimit = XRG_PARTICLE_DEPTH_LIMIT;
    XrgParticleRow work[13];
    unsigned char *packet = (unsigned char *)work;
    RgDrawView *pView;
    unsigned int done;
    int n;
    int rem;
    int ret;
    int mask;
    XrgParticleRow *pStore;

    *(XrgDispVec4 *)&work[2] = D_00A58F50;
    pView = s_pCurView_00A511F4;
    sceVif1PkInit((XglPacket *)packet, SCRATCHPAD_BASE);
    _openVifGifAD((XglPacket *)packet);
    sceVif1PkAddGsAD((XglPacket *)packet, SCE_GS_TEXFLUSH, 0);
    sceVif1PkAddGsAD((XglPacket *)packet, SCE_GS_TEST_1, 0x73001ULL);
    sceVif1PkAddGsAD((XglPacket *)packet, SCE_GS_CLAMP_1, 0x3fc003fc000ULL);
    sceVif1PkAddGsAD((XglPacket *)packet, SCE_GS_PABE, 0);
    sceVif1PkAddGsAD((XglPacket *)packet, SCE_GS_ALPHA_1, 0x8000000068ULL);
    sceVif1PkAddGsAD((XglPacket *)packet, SCE_GS_TEX0_1, (unsigned long long)tex0);
    _closeVifGif((sceVif1Packet *)packet);

    __asm__ __volatile__(
        "lqc2 vf27, 0(%0)\n"
        "lqc2 vf28, 16(%0)\n"
        "lqc2 vf29, 32(%0)\n"
        "lqc2 vf30, 48(%0)\n"
        :
        : "r"(pView->viewMatrix)
        : "memory");
    __asm__ __volatile__(
        "lqc2 vf25, 0(%0)\n"
        "lqc2 vf26, 0(%1)\n"
        :
        : "r"(pView->axisX), "r"(pView->axisY)
        : "memory");
    __asm__ __volatile__(
        "lqc2 vf31, 0(%0)\n"
        "sqc2 vf31, 0(%1)\n"
        :
        : "r"(pView->axisX), "r"(&work[4])
        : "memory");
    work[4].vec[0] *= 0.375f;
    work[4].vec[1] *= 0.375f;
    XrgUnitVector(work[3].vec);
    pStore = &work[5];
    for (done = 0; done < count; done += XRG_PARTICLE_DISP_BATCH) {
        n = XRG_PARTICLE_DISP_BATCH;
        rem = count - done;
        if (!(n < rem)) {
            n = rem;
        }
        for (; n-- > 0; pPoints++) {
            XrgParticleRow *pDir = &work[3];
            XrgParticleRow *pScratch = &work[6];
            XrgParticleRow *pScale = &work[7];
            XrgParticleRow *pSprite = &work[12];
            XrgParticleRow *pConst = &work[2];

            __asm__ __volatile__(
                "ctc2 $0,$vi18\n"
                "lqc2 vf31, 0(%1)\n"
                "vmulax.xyzw ACC, vf27, vf31x\n"
                "vmadday.xyzw ACC, vf28, vf31y\n"
                "vmaddaz.xyzw ACC, vf29, vf31z\n"
                "vmaddw.xyzw vf31, vf30, vf0w\n"
                "nop\n"
                "nop\n"
                "nop\n"
                "vclipw.xyz vf31, vf31w\n"
                "vdiv Q, vf0w, vf31w\n"
                "vwaitq\n"
                "vmulq.xyzw vf31, vf31, Q\n"
                "vmulaw.xyzw ACC, vf26, vf0w\n"
                "vmadd.xyzw vf31, vf31, vf25\n"
                "vsub.w vf31, vf31, vf31\n"
                "sqc2 vf31, 0(%2)\n"
                "vftoi4.xyzw vf20, vf31\n"
                "sqc2 vf20, 0(%3)\n"
                "cfc2 $2, $vi18\n"
                "nop\n"
                "andi $2, $2, 0x3f\n"
                "move %0, $2\n"
                : "=r"(mask)
                : "r"(pPoints), "r"(pScratch), "r"(pStore)
                : "$2", "memory");
            if (mask != 0) {
                continue;
            }
            if (work[6].vec[2] < depthLimit) {
                int ix;
                int iy;
                int x0;
                int x1;
                Quadword corner;

                work[3].vec[1] = work[3].vec[0] = pPoints->appearance.vec[0] * 0.5f;
                __asm__ __volatile__(
                    "lqc2 vf11, 0(%0)\n"
                    "vmulq.xyzw vf11, vf11, Q\n"
                    :
                    : "r"(pDir)
                    : "memory");
                __asm__ __volatile__(
                    "sqc2 vf11, 0(%0)\n"
                    :
                    : "r"(pDir)
                    : "memory");
                work[7].vec[0] = work[3].vec[0] * work[4].vec[0];
                work[7].vec[1] = work[3].vec[1] * work[4].vec[1];
                work[7].vec[2] = work[3].vec[2] * work[4].vec[2];
                work[7].vec[3] = work[3].vec[3] * work[4].vec[3];
                if (pPoints->appearance.word[2] != 0) {
                    work[7].vec[0] *= cosf(pPoints->appearance.vec[2]);
                }
                if (pPoints->appearance.word[3] != 0) {
                    work[7].vec[1] *= cosf(pPoints->appearance.vec[3]);
                }
                __asm__ __volatile__(
                    "lqc2 vf11, 0(%0)\n"
                    "vftoi4.xyzw vf11, vf11\n"
                    "sqc2 vf11, 0(%1)\n"
                    :
                    : "r"(pScale), "r"(pScale)
                    : "memory");
                ix = work[7].word[0];
                if ((unsigned int)(ix - 1) >= XRG_PARTICLE_SPRITE_HALF_MAX) {
                    continue;
                }
                work[12].vec[0] = pPoints->appearance.vec[1];
                __asm__ __volatile__(
                    "lqc2 vf10, 0(%0)\n"
                    "lqc2 vf11, 0(%1)\n"
                    "vmulx.xyzw vf10, vf10, vf11x\n"
                    "vftoi0.xyzw vf10, vf10\n"
                    "sqc2 vf10, 0(%2)\n"
                    :
                    : "r"(pConst), "r"(pSprite), "r"(pSprite)
                    : "memory");
                corner = work[5].quad;
                work[8].quad = corner;
                work[9].quad = corner;
                x0 = work[8].word[0] - ix;
                x1 = work[9].word[0] + ix;
                work[10].quad = corner;
                work[11].quad = corner;
                work[8].word[0] = x0;
                work[9].word[0] = x1;
                if (x0 > 0) {
                    x0 = XRG_PARTICLE_GS_COORD_LIMIT;
                    if (x1 < x0) {
                        iy = work[7].word[1];
                        work[10].word[0] -= ix;
                        work[8].word[1] -= iy;
                        work[9].word[1] -= iy;
                        work[11].word[0] += ix;
                        work[10].word[1] += iy;
                        work[11].word[1] += iy;
                        _openVifGif((XglPacket *)packet, XRG_PARTICLE_SPRITE_PRIM,
                                    XRG_PARTICLE_SPRITE_NREG,
                                    XRG_PARTICLE_SPRITE_REGS);
                        sceVif1PkAddGsPacked((XglPacket *)packet, work[12].quad);
                        sceVif1PkAddGsPacked((XglPacket *)packet, rect[0].q);
                        sceVif1PkAddGsPacked((XglPacket *)packet, work[8].quad);
                        sceVif1PkAddGsPacked((XglPacket *)packet, rect[1].q);
                        sceVif1PkAddGsPacked((XglPacket *)packet, work[9].quad);
                        sceVif1PkAddGsPacked((XglPacket *)packet, rect[2].q);
                        sceVif1PkAddGsPacked((XglPacket *)packet, work[10].quad);
                        sceVif1PkAddGsPacked((XglPacket *)packet, rect[3].q);
                        sceVif1PkAddGsPacked((XglPacket *)packet, work[11].quad);
                        _closeVifGif((sceVif1Packet *)packet);
                    }
                }
            }
        }
        if ((ret = sceVif1PkSize((XglPacket *)packet)) != 0) {
            nmlModelDirectSend(pView->projectionMode, (void *)SCRATCHPAD_BASE, ret);
            sceVif1PkInit((XglPacket *)packet, SCRATCHPAD_BASE);
        }
    }
}

const char D_00A58F78[] = "pDrv->m_uReqCalcNum < REQ_CALC_MAX";
const char D_00A58FA0[] = "pPoints != NIL";
const char D_00A58FB0[] = "pDrv->m_uReqDispNum < REQ_DISP_MAX";
const char D_00A58FD8[] = "pPic != NIL";
const char D_00A58FE8[] = "pEnv != NIL";
const char D_00A58FF8[] = "pView != NIL";
const char D_00A59008[] = "p != NIL";
const char D_00A59018[] = "pArray != NIL";

extern RgHeap *InstanceOfRgHeap(void);
extern void *RgHeapAlloc(RgHeap *heap, unsigned int size,
                         const char *source_file, int line);

static XrgParticleDriver *CreateXrgParticleDriver(void)
{
    XrgParticleDriver *driver;

    driver = RgHeapAlloc(InstanceOfRgHeap(), sizeof(XrgParticleDriver),
                         D_00A58F30, 340);
    _InitDriver(driver);
    return driver;
}

extern void RgHeapFree(RgHeap *heap, void *pointer, const char *source_file,
                       int line);

void DisposeXrgParticleDriver(XrgParticleDriver *pDrv)
{
    if (pDrv == 0) {
        assert_prog(D_00A58F20, D_00A58F30, 347);
    }
    _DisposeDriver(pDrv);
    RgHeapFree(InstanceOfRgHeap(), pDrv, D_00A58F30, 349);
}

/*
 * The singleton destructor: it forwards the instance it is handed ($a0,
 * untouched before the tail call) to DisposeXrgParticleDriver, which asserts
 * it non-null and frees it.
 */
static void _WrapperDestruct(XrgParticleDriver *driver)
{
    DisposeXrgParticleDriver(driver);
}

extern void *RgSingletonIDGet(unsigned int singleton_id);
extern void RgSingletonIDEntry(int singleton_id, RgSimpleDB *database,
                               void (*destructor)(RgSimpleDB *database));

XrgParticleDriver *InstanceOfXrgParticleDriver(void)
{
    XrgParticleDriver *driver;

    driver = RgSingletonIDGet(7U);
    if (driver == 0) {
        driver = CreateXrgParticleDriver();
        RgSingletonIDEntry(7, (RgSimpleDB *)driver,
                           (void (*)(RgSimpleDB *))_WrapperDestruct);
    }
    return driver;
}

void XrgParticleDriverSet(XrgParticleDriver *driver, int params0, int params1)
{
    if (driver == 0) {
        assert_prog(D_00A58F20, D_00A58F30, 373);
    }
    driver->params0 = params0;
    driver->params1 = params1;
}

#define REQ_CALC_MAX 16

void XrgParticleDriverPassTimeReq(XrgParticleDriver *driver, struct XrgParticle *pPoints,
                                  int count, float rate)
{
    unsigned int idx;

    if (driver == 0) {
        assert_prog(D_00A58F20, D_00A58F30, 392);
    }
    if (driver->m_uReqCalcNum >= REQ_CALC_MAX) {
        assert_prog(D_00A58F78, D_00A58F30, 394);
    }
    if (pPoints == 0) {
        assert_prog(D_00A58FA0, D_00A58F30, 395);
    }
    idx = driver->m_uReqCalcNum++;
    driver->calcReq[idx].particle = pPoints;
    driver->calcReq[idx].count = count;
    driver->calcReq[idx].rate = rate;
}

#define REQ_DISP_MAX 1024

void XrgParticleDriverDispReq(XrgParticleDriver *driver, struct XrgParticle *pPoints,
                              int count, RgBxxPic *pPic)
{
    unsigned int idx;

    if (driver == 0) {
        assert_prog(D_00A58F20, D_00A58F30, 417);
    }
    if (driver->m_uReqDispNum >= REQ_DISP_MAX) {
        assert_prog(D_00A58FB0, D_00A58F30, 419);
    }
    if (pPoints == 0) {
        assert_prog(D_00A58FA0, D_00A58F30, 420);
    }
    if (pPic == 0) {
        assert_prog(D_00A58FD8, D_00A58F30, 421);
    }
    idx = driver->m_uReqDispNum++;
    driver->dispReq[idx].pPoints = pPoints;
    driver->dispReq[idx].count = count;
    driver->dispReq[idx].pPic = pPic;
}

static void _Calc(struct XrgParticle *particle, int count, float rate,
                  float elapsed);

void XrgParticleDriverPassTime(XrgParticleDriver *pDrv, float elapsed)
{
    unsigned int i;
    XrgParticleCalcReq *req;

    if (pDrv == 0) {
        assert_prog(D_00A58F20, D_00A58F30, 438);
    }
    for (i = 0; i < pDrv->m_uReqCalcNum; i++) {
        req = &pDrv->calcReq[i];
        _Calc(req->particle, req->count, req->rate, elapsed);
    }
    pDrv->m_uReqCalcNum = 0;
}

extern RgDrawView *RgDrawStudioGetView(void *pStudio);
extern void *RgBxxGetXtx(void *pBxx);
extern void nmlModelDirectSendXtx(int modelId, void *data);
extern long long XrgBxxPs2Tex0(void *texture);
typedef struct XrgParticleDrawPicView {
    unsigned char unmodeled_00[0x2C];
    int u;
    int v;
    int w;
    int h;
    unsigned char unmodeled_3C[0x1C];
    void *bxx;
} XrgParticleDrawPicView;
static void _Disp(struct XrgParticle *pPoints, int count, long long tex0,
                  XrgParticleRectQ *rect);

void _DrawDriver(XrgParticleDriver *driver, void *pStudio)
{
    XrgParticleRectQ rect[4];
    RgDrawView *view;
    XrgParticleDrawPicView *pic;
    XrgParticleDispReq *req;
    void *curBxx;
    unsigned int i;
    long long tex0;
    int w, h;

    curBxx = 0;
    if (driver == 0) {
        assert_prog(D_00A58F20, D_00A58F30, 459);
    }
    if (pStudio == 0) {
        assert_prog(D_00A58FE8, D_00A58F30, 460);
    }
    view = RgDrawStudioGetView(pStudio);
    if (view == 0) {
        assert_prog(D_00A58FF8, D_00A58F30, 462);
    }
    s_pCurView_00A511F4 = view;
    for (i = 0; i < driver->m_uReqDispNum; i++) {
        req = &driver->dispReq[i];
        pic = (XrgParticleDrawPicView *)req->pPic;
        if (pic->bxx != curBxx) {
            curBxx = pic->bxx;
            nmlModelDirectSendXtx(view->projectionMode, RgBxxGetXtx(curBxx));
        }
        tex0 = XrgBxxPs2Tex0(pic);
        rect[0].q = 0;
        rect[0].i[0] = pic->u * 16 + 8;
        rect[0].i[1] = pic->v * 16 + 8;
        rect[1].q = rect[0].q;
        rect[2].q = rect[0].q;
        rect[3].q = rect[0].q;
        w = pic->w * 16 - 16;
        h = pic->h * 16 - 16;
        rect[3].i[0] += w;
        rect[1].i[0] += w;
        rect[3].i[1] += h;
        rect[2].i[1] += h;
        _Disp(req->pPoints, req->count, tex0, rect);
    }
}

static void _ClearReqDisp(XrgParticleDriver *driver)
{
    if (driver == 0) {
        assert_prog(D_00A58F20, D_00A58F30, 504);
    }
    driver->m_uReqDispNum = 0;
}

extern RgDraw *InstanceOfRgDraw(void);
extern void RgDrawReq(RgDraw *pDraw, XrgParticleDriver *driver,
                      void (*drawFunc)(XrgParticleDriver *driver, void *pStudio),
                      void (*clearFunc)(XrgParticleDriver *driver, void *pStudio),
                      int prio, int drawID);
void _DrawDriver(XrgParticleDriver *driver, void *pStudio);

void XrgParticleDriverDisp(XrgParticleDriver *driver)
{
    if (driver == 0) {
        assert_prog(D_00A59008, D_00A58F30, 511);
    }
    RgDrawReq(InstanceOfRgDraw(), driver, _DrawDriver,
             (void (*)(XrgParticleDriver *, void *))_ClearReqDisp, 2, -2);
}

extern RgHeap *InstanceOfRgHeapData(void);
typedef union XrgParticleQ {
    Quadword q;
    float f[4];
} XrgParticleQ;
typedef struct XrgParticleView {
    XrgParticleQ v[5];
} XrgParticleView;

XrgParticle *CreateArrayOfXrgParticle(unsigned int num)
{
    XrgParticleView *array;
    XrgParticleView *p;
    unsigned int i;

    array = RgHeapAlloc(InstanceOfRgHeapData(), num * sizeof(XrgParticleView),
                        D_00A58F30, 524);
    if (array == 0) {
        assert_prog(D_00A58FA0, D_00A58F30, 526);
    }
    p = array;
    for (i = 0; i < num; i++) {
        p->v[0].q = 0;
        p->v[1].q = 0;
        p->v[2].q = 0;
        p->v[3].q = 0;
        p->v[4].q = 0;
        p->v[0].f[3] = 1.0f;
        p++;
    }
    return (XrgParticle *)array;
}

extern RgHeap *InstanceOfRgHeapData(void);
extern void RgHeapFree(RgHeap *heap, void *pointer, const char *source_file,
                       int line);

void DisposeArrayOfXrgParticle(XrgParticle *array)
{
    if (array == 0) {
        assert_prog(D_00A59018, D_00A58F30, 543);
    }
    RgHeapFree(InstanceOfRgHeapData(), array, D_00A58F30, 544);
}


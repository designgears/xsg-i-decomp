/*
 * OV12 original TU 86: 0x00a4a620..0x00a4c840 (42 functions)
 */
#include "common.h"
#include "shared.h"
#include "xrg_paint2d.h"
#include "ov12/rg_draw.h"
#include "ov12/rg_singleton_id.h"
#include "ov12/xrg_rand_int.h"

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

typedef union XrgPaint2DGifTag {
    struct {
        u64 lo;
        u64 hi;
    } part;
    Quadword quad;
} XrgPaint2DGifTag;

extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkAlign(XglPacket *packet, int align, int size);
extern void sceVif1PkOpenDirectCode(XglPacket *packet, int mode);
extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);
extern void sceVif1PkOpenGifTag(XglPacket *packet, Quadword tag);

static void _openVifGif(XglPacket *packet, unsigned int prim,
                        unsigned int nreg, u64 regs)
{
    XrgPaint2DGifTag tag;

    tag.part.lo = (1u << 15) | (1ULL << 46) | ((u64)prim << 47) |
                  ((u64)nreg << 60);
    tag.part.hi = regs;
    sceVif1PkAlign(packet, 2, 3);
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkOpenGifTag(packet, tag.quad);
}

/*
 * ov12:0x00a59030 is a fixed GIF tag: EOP set, NREG 1, REGS 0xE (the A+D
 * address+data register code), the constant _openVifGifAD passes to
 * sceVif1PkOpenGifTag.
 */
extern const XrgPaint2DGifTag D_00A59030;

static void _openVifGifAD(XglPacket *packet)
{
    XrgPaint2DGifTag tag;

    tag = D_00A59030;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAlign(packet, 2, 3);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, tag.quad);
}

/*
 * The VIF1 direct/HL packet the paint renderer's draw calls build (_DrawNoTex,
 * _DrawWithTex, _Paint2DFlush, all outside this allocation) is closed through
 * this pair, both of which take the packet itself: the caller keeps its
 * argument live across both calls (a0 into s0) rather than discarding it
 * after the first.
 */
typedef struct sceVif1Packet sceVif1Packet;
extern void sceVif1PkCloseGifTag(sceVif1Packet *packet);
extern void sceVif1PkCloseDirectHLCode(sceVif1Packet *packet);

static void _closeVifGif(sceVif1Packet *packet) {
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectHLCode(packet);
}

static void _Geom2D();
static void _DrawNoTex();

static void _InitReq(XrgPaint2DDrawReq *req)
{
    char *p = (char *)req;
    Quadword z = 0;

    *(void **)(p + 0x0) = _Geom2D;
    req->draw = (void (*)(XrgPaint2D *, void *))_DrawNoTex;
    *(int *)(p + 0x8) = 0;
    *(Quadword *)(p + 0x10) = z;
    *(Quadword *)(p + 0x20) = z;
    *(Quadword *)(p + 0x30) = z;
    *(Quadword *)(p + 0x50) = z;
    *(Quadword *)(p + 0x60) = z;
    req->color[3] = 0x7F;
    req->color[2] = 0x7F;
    req->color[1] = 0x7F;
    req->alpha = 0;
    req->tex0 = 0;
    req->texture = 0;
    req->offsetMode = 0;
    req->color[0] = 0x7F;
    req->uvMode = 0;
}

static void _DeriveInfoReq(XrgPaint2DDrawReq *dst, XrgPaint2DDrawReq *src)
{
    dst->offsetMode = src->offsetMode;
    dst->offsetAnchor = src->offsetAnchor;
    dst->offsetResult = src->offsetResult;
}

#define XRG_PAINT2D_COORD_MAX 0x3FFF

static void _CalcRectangle(XrgPaint2DVertex corners[4], const XrgIntRect *rect,
                           int color, int prio)
{
    corners[0].x = rect->x;
    corners[0].y = rect->y;
    corners[1].x = rect->x + rect->width;
    corners[1].y = rect->y;
    corners[2].x = rect->x;
    corners[2].y = rect->y + rect->height;
    corners[3].x = rect->x + rect->width;
    corners[3].y = rect->y + rect->height;

    if (corners[2].x >= XRG_PAINT2D_COORD_MAX)
        corners[2].x = XRG_PAINT2D_COORD_MAX;
    if (corners[2].y >= XRG_PAINT2D_COORD_MAX)
        corners[2].y = XRG_PAINT2D_COORD_MAX;
    if (corners[3].x >= XRG_PAINT2D_COORD_MAX)
        corners[3].x = XRG_PAINT2D_COORD_MAX;
    if (corners[3].y >= XRG_PAINT2D_COORD_MAX)
        corners[3].y = XRG_PAINT2D_COORD_MAX;

    corners[3].color = color;
    corners[2].color = color;
    corners[1].color = color;
    corners[0].color = color;
    corners[3].prio = prio;
    corners[2].prio = prio;
    corners[1].prio = prio;
    corners[0].prio = prio;
}

/*
 * _CalcUV expands one rectangle into the four vertices _CalcRectangle
 * builds, after applying up to two independent overrides from a second
 * rectangle-shaped argument: bit 0 of `mode` adds its x/y to the base
 * rectangle's x/y, bit 1 replaces the base's width/height with its own.
 * The base rectangle is brought in as one 16-byte COP2 quadword transfer
 * (lqc2/sqc2), the same admitted idiom XrgPaint2DOffset3D and
 * XrgPaint2DColor use elsewhere in this allocation.
 */
static void _CalcUV(XrgPaint2DVertex corners[4], const XrgIntRect *base,
                    int mode, const XrgIntRect *override)
{
    XrgIntRect rect;

    __asm__ __volatile__("lqc2 vf31, 0(%0)\n\tsqc2 vf31, 0(%1)"
                         : : "r"(base), "r"(&rect) : "memory");
    if (mode & 1) {
        rect.x += override->x;
        rect.y += override->y;
    }
    if (mode & 2) {
        rect.width = override->width;
        rect.height = override->height;
    }
    _CalcRectangle(corners, &rect, 0, 0);
}

extern void *s_pCurView_00A51220;

static int _CalcPosOnScreen(void *destination, const void *position, float *offsetWork)
{
    void *view;
    void *view2;
    void *offset;
    void *addr;
    int clip;
    float x;
    float scale;

    __asm__ __volatile__(
        "lui %0, %%hi(s_pCurView_00A51220)\n\t"
        "addiu %0, %0, %%lo(s_pCurView_00A51220)\n\t"
        "lw %3, 0(%0)\n\t"
        "lqc2 $vf27, 0(%3)\n\t"
        "lqc2 $vf28, 0x10(%3)\n\t"
        "lqc2 $vf29, 0x20(%3)\n\t"
        "lqc2 $vf30, 0x30(%3)\n\t"
        "lw %2, 0(%0)\n\t"
        "addiu %1, %2, 0x50\n\t"
        "addiu %2, %2, 0x40\n\t"
        "lqc2 $vf25, 0(%2)\n\t"
        "lqc2 $vf26, 0(%1)\n\t"
        "lw %3, 0(%0)\n\t"
        "addiu %3, %3, 0x40\n\t"
        "lqc2 $vf31, 0(%3)\n\t"
        "sqc2 $vf31, 0(%4)"
        : "=&r"(addr), "=&r"(offset), "=&r"(view2), "=&r"(view)
        : "r"(offsetWork)
        : "memory");

    x = offsetWork[0];
    __asm__ __volatile__("" : : "f"(x) : "$f0");
    scale = 0.375f;
    __asm__ __volatile__("" : "+f"(scale) : "f"(x));
    offsetWork[0] = x * scale;
    offsetWork[1] *= scale;

    __asm__ __volatile__(
        "ctc2.ni $0, $vi18\n\t"
        "lqc2 $vf31, 0(%2)\n\t"
        "vmulax.xyzw ACC, $vf27, $vf31x\n\t"
        "vmadday.xyzw ACC, $vf28, $vf31y\n\t"
        "vmaddaz.xyzw ACC, $vf29, $vf31z\n\t"
        "vmaddw.xyzw $vf31, $vf30, $vf0w\n\t"
        "nop\n\t"
        "nop\n\t"
        "nop\n\t"
        "vclipw.xyz $vf31xyz, $vf31w\n\t"
        "vdiv Q, $vf0w, $vf31w\n\t"
        "vwaitq\n\t"
        "vmulq.xyzw $vf31, $vf31, Q\n\t"
        "vmulaw.xyzw ACC, $vf26, $vf0w\n\t"
        "vmadd.xyzw $vf31, $vf31, $vf25\n\t"
        "vftoi4.xyzw $vf20, $vf31\n\t"
        "vsub.w $vf20, $vf20, $vf20\n\t"
        "sqc2 $vf20, 0(%1)\n\t"
        "cfc2.ni $2, $vi18\n\t"
        "nop\n\t"
        "andi $2, $2, 0x3f\n\t"
        "daddu %0, $2, $0"
        : "=r"(clip)
        : "r"(destination), "r"(position)
        : "$2", "memory");
    return clip == 0;
}

extern void *s_pCurView_00A51220;

static void _CalcPosOnScreenFloat(void *destination, const void *position)
{
    void *view;
    void *view2;
    void *offset;
    void *addr;

    __asm__ __volatile__(
        "lui %1, %%hi(s_pCurView_00A51220)\n\t"
        "addiu %1, %1, %%lo(s_pCurView_00A51220)\n\t"
        "lw %3, 0(%1)\n\t"
        "lqc2 $vf27, 0(%3)\n\t"
        "lqc2 $vf28, 0x10(%3)\n\t"
        "lqc2 $vf29, 0x20(%3)\n\t"
        "lqc2 $vf30, 0x30(%3)\n\t"
        "lw %2, 0(%1)\n\t"
        "addiu %0, %2, 0x50\n\t"
        "addiu %2, %2, 0x40\n\t"
        "lqc2 $vf25, 0(%2)\n\t"
        "lqc2 $vf26, 0(%0)\n\t"
        "lqc2 $vf31, 0(%4)\n\t"
        "vmulax.xyzw ACC, $vf27, $vf31x\n\t"
        "vmadday.xyzw ACC, $vf28, $vf31y\n\t"
        "vmaddaz.xyzw ACC, $vf29, $vf31z\n\t"
        "vmaddw.xyzw $vf31, $vf30, $vf0w\n\t"
        "nop\n\t"
        "nop\n\t"
        "nop\n\t"
        "vclipw.xyz $vf31xyz, $vf31w\n\t"
        "vdiv Q, $vf0w, $vf31w\n\t"
        "vwaitq\n\t"
        "vmulq.xyzw $vf31, $vf31, Q\n\t"
        "vmulaw.xyzw ACC, $vf26, $vf0w\n\t"
        "vmadd.xyzw $vf31, $vf31, $vf25\n\t"
        "vsub.w $vf31, $vf31, $vf31\n\t"
        "sqc2 $vf31, 0(%5)\n\t"
        "nop\n\t"
        : "=&r"(offset), "=&r"(addr), "=&r"(view2), "=&r"(view)
        : "r"(position), "r"(destination)
        : "memory");
}

INCLUDE_ASM("asm/nonmatchings/ov12/xrg_paint2d", _CalcOffset);

INCLUDE_ASM("asm/nonmatchings/ov12/xrg_paint2d", _Geom2D);

INCLUDE_ASM("asm/nonmatchings/ov12/xrg_paint2d", _Line3D);

extern void sceVif1PkAddGsAD(XglPacket *packet, int reg, u64 value);
extern void sceVif1PkAddGsPacked(XglPacket *packet, Quadword data);

static void _DrawNoTex(XrgPaint2DDrawReq *req, XglPacket *pk, Quadword *v)
{
    Quadword col;
    unsigned int prim = 4;

    __asm__ __volatile__("lqc2 vf31, 0(%0)\n\tsqc2 vf31, 0(%1)"
                         : : "r"(req->color), "r"(&col) : "memory");
    if (req->alpha != 0) {
        _openVifGifAD(pk);
        sceVif1PkAddGsAD(pk, 0x42, req->alpha);
        _closeVifGif((sceVif1Packet *)pk);
        prim = 0x44;
    }
    _openVifGif(pk, prim, 5, 0x44441);
    sceVif1PkAddGsPacked(pk, col);
    sceVif1PkAddGsPacked(pk, v[0]);
    sceVif1PkAddGsPacked(pk, v[1]);
    sceVif1PkAddGsPacked(pk, v[2]);
    sceVif1PkAddGsPacked(pk, v[3]);
    _closeVifGif((sceVif1Packet *)pk);
}

INCLUDE_ASM("asm/nonmatchings/ov12/xrg_paint2d", _DrawWithTex);

static void _InitPaint(XrgPaint2D *paint, const char *sourceFile, int line) {
    XrgPaint2DDrawReq *req;
    int id;

    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 0x2BD);
    }
    strcpy((char *)paint + 0xD018, sourceFile);
    id = -2;
    *(int *)((char *)paint + 0xD058) = line;
    paint->prio = 3;
    paint->drawID = id;
    req = &paint->req;
    paint->request = req;
    paint->reqCount = 0;
    _InitReq(req);
}

static void _DestructPaint(XrgPaint2D *paint) {
    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 713);
    }
}

extern RgHeap *InstanceOfRgHeap(void);
extern void *RgHeapAlloc(RgHeap *heap, unsigned int size, const char *source_file,
                        int line);
extern void RgHeapFree(RgHeap *heap, void *ptr, const char *source_file,
                       int line);
static void _InitPaint(XrgPaint2D *paint, const char *sourceFile, int line);

XrgPaint2D *CreateXrgPaint2D_sub(const char *sourceFile, int line) {
    XrgPaint2D *paint;

    paint = RgHeapAlloc(InstanceOfRgHeap(), XRG_PAINT2D_SIZE, D_00A59058, 720);
    _InitPaint(paint, sourceFile, line);
    return paint;
}

void DisposeXrgPaint2D_sub(XrgPaint2D *paint, const char *sourceFile, int line) {
    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 727);
    }
    _DestructPaint(paint);
    RgHeapFree(InstanceOfRgHeap(), paint, sourceFile, line);
}

static void _WrapperDestruct(RgSimpleDB *database) {
    DisposeXrgPaint2D_sub((XrgPaint2D *) database, D_00A59058, 0x2DC);
}

void XrgPaint2DSetDrawPrio(XrgPaint2D *paint, int prio);

XrgPaint2D *InstanceOfXrgPaint2D(void) {
    XrgPaint2D *paint;

    paint = RgSingletonIDGet(10);
    if (paint == 0) {
        paint = CreateXrgPaint2D_sub(D_00A59058, 742);
        XrgPaint2DSetDrawPrio(paint, 3);
        RgSingletonIDEntry(10, (RgSimpleDB *) paint, _WrapperDestruct);
    }
    return paint;
}

void XrgPaint2DSetDrawID(XrgPaint2D *paint, int drawID) {
    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 757);
    }
    paint->drawID = drawID;
}

void XrgPaint2DSetDrawPrio(XrgPaint2D *paint, int prio) {
    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 765);
    }
    paint->prio = prio;
}

void XrgPaint2DOffset2D(XrgPaint2D *paint, int x, int y)
{
    XrgPaint2DDrawReq *req;

    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 777);
    }
    req = paint->request;
    req->offsetAnchor.i.x = x;
    req->offsetMode = 1;
    paint->request->offsetAnchor.i.y = y;
}

void XrgPaint2DOffset2DDot(XrgPaint2D *paint, int u, int v)
{
    XrgPaint2DDrawReq *req;

    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 786);
    }
    req = paint->request;
    req->offsetAnchor.i.x = u * 0x10;
    req->offsetMode = 1;
    paint->request->offsetAnchor.i.y = v * 0x10;
}

void XrgPaint2DOffsetCenter(XrgPaint2D *paint)
{
    XrgPaint2DDrawReq *req;

    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 795);
    }
    req = paint->request;
    req->offsetAnchor.i.x = 0x1000;
    req->offsetMode = 1;
    paint->request->offsetAnchor.i.y = 0xE00;
}

void XrgPaint2DOffset3D(XrgPaint2D *paint, const XrgPaint2DOffset *anchor)
{
    XrgPaint2DDrawReq *req;

    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 804);
    }
    req = paint->request;
    req->offsetMode = 2;
    __asm__ __volatile__("lqc2 vf31, 0(%0)\n\tsqc2 vf31, 0(%1)"
                         : : "r"(anchor), "r"(&req->offsetAnchor) : "memory");
}

/*
 * XrgPaint2DOffset3DForce (0x00a4bc18) re-reads paint->request for the final
 * store instead of reusing `req`: the original reloads it into a second
 * register there rather than keeping the first one live across the COP2
 * transfer.
 */
void XrgPaint2DOffset3DForce(XrgPaint2D *paint, const XrgPaint2DOffset *anchor,
                             float force)
{
    XrgPaint2DDrawReq *req;

    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 812);
    }
    req = paint->request;
    req->offsetMode = 3;
    __asm__ __volatile__("lqc2 vf31, 0(%0)\n\tsqc2 vf31, 0(%1)"
                         : : "r"(anchor), "r"(&req->offsetAnchor) : "memory");
    paint->request->offsetResult.f.x = force;
}

/*
 * XrgPaint2DOffsetResult stores an explicit offset-result override into the
 * bound paint's draw request and flags it as such (offsetMode bit 0x10,
 * XrgPaint2DDrawReq above).
 */
void XrgPaint2DOffsetResult(XrgPaint2D *paint, int value)
{
    XrgPaint2DDrawReq *req;

    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 821);
    }
    req = paint->request;
    req->offsetResult.i.y = value;
    req->offsetMode |= 0x10;
}

void XrgPaint2DOffsetLinear(XrgPaint2D *paint, int x, int y)
{
    XrgPaint2DDrawReq *request;

    request = paint->request;
    request->offsetResult.i.z = x;
    request->offsetMode |= 8;
    paint->request->offsetResult.i.w = y;
}

void XrgPaint2DColor(XrgPaint2D *paint, const unsigned int *color)
{
    unsigned int *dst;

    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 843);
    }
    dst = paint->request->color;
    __asm__ __volatile__("lqc2 vf31, 0(%0)" : : "r"(color) : "memory");
    __asm__ __volatile__("sqc2 vf31, 0(%0)" : : "r"(dst) : "memory");
}

/*
 * XrgPaint2DAlpha (0x00a4bd98) selects one of six fixed GS ALPHA_1-shaped
 * 64-bit blend-equation values by its own mode argument; an out-of-range
 * mode leaves the request's alpha at the zero this function seeds before
 * the switch (the same value case 4 also selects).
 */
void XrgPaint2DAlpha(XrgPaint2D *paint, unsigned int mode)
{
    long long alpha;

    alpha = 0;
    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 852);
    }
    switch (mode) {
    case 0:
        alpha = 0x44;
        break;
    case 1:
        alpha = ((long long)0x80 << 32) | 0x68;
        break;
    case 3:
        alpha = 0x48;
        break;
    case 2:
        alpha = ((long long)0x60 << 32) | 0x62;
        break;
    case 4:
        alpha = 0;
        break;
    case 5:
        alpha = 0x42;
        break;
    }
    paint->request->alpha = alpha;
}

extern void RgBxxGetHeader(int header);
extern long long XrgBxxPs2Tex0(void *texture);
extern const char D_00A590C8[];
static void _DrawWithTex(XrgPaint2D *paint, void *pStudio);

/*
 * The texture object XrgPaint2DUseTexture binds to a paint request: only the
 * four members this function itself reads are named, all of them consumed
 * as the source rectangle for the GS TEXCLAMP bounds it derives (see
 * XrgPaint2DDrawReq's uLow/vLow/uHigh/vHigh). `header` is the value the
 * function forwards to RgBxxGetHeader without itself reading the result.
 * Nothing else about this foreign object is evidenced here.
 */
typedef struct XrgPaint2DTexture {
    unsigned char unmodeled_00[0x2C];
    int left;
    int top;
    int right;
    int bottom;
    unsigned char unmodeled_3C[0x1C];
    int header;
} XrgPaint2DTexture;

void XrgPaint2DUseTexture(XrgPaint2D *paint, XrgPaint2DTexture *texture)
{
    XrgPaint2DDrawReq *req;
    long long tex0;

    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 887);
    }
    if (texture == 0) {
        assert_prog(D_00A590C8, D_00A59058, 888);
    }
    req = paint->request;
    RgBxxGetHeader(texture->header);
    tex0 = XrgBxxPs2Tex0(texture);
    req->texture = texture;
    req->tex0 = tex0;
    req->draw = _DrawWithTex;
    req->uLow = texture->left * 0x10 + 8;
    req->vLow = texture->top * 0x10 + 8;
    req->uHigh = texture->right * 0x10 - 8;
    req->vHigh = texture->bottom * 0x10 - 8;
}

void XrgPaint2DSetUVOffset(XrgPaint2D *paint, int uOffset, int vOffset)
{
    int scaledUOffset;
    XrgPaint2DDrawReq *req;

    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 913);
    }
    scaledUOffset = uOffset * 0x10;
    req = paint->request;
    req->vOffset = vOffset * 0x10;
    req->uOffset = scaledUOffset;
    req->uvMode |= 1;
}

void XrgPaint2DSetUVSize(XrgPaint2D *paint, int width, int height)
{
    int uSize;
    XrgPaint2DDrawReq *req;

    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 927);
    }
    uSize = width * 0x10;
    req = paint->request;
    req->vSize = height * 0x10;
    req->uSize = uSize;
    req->uvMode |= 2;
}

/*
 * InitXrgPaint2DRect clears the rectangle request consumed by the adjacent
 * XrgPaint2DDrawRect/XrgPaint2DDrawXYWH routines: an all-zero rectangle at the
 * screen origin, unrotated, at its natural size.  The layout the members come
 * from is recovered in src/ov12/xrg_paint2d.h.
 *
 * The store order (mode, scale, height, width, y, x, angle) is the one the
 * accepted form was matched with and is unchanged here, so this form isolates
 * the layout recovery from the schedule.
 */

void InitXrgPaint2DRect(XrgPaint2DRect *rectangle, int mode)
{
    rectangle->mode = mode;
    rectangle->scale = 1.0f;
    rectangle->height = 0;
    rectangle->width = 0;
    rectangle->y = 0;
    rectangle->x = 0;
    rectangle->angle = 0.0f;
}

extern const char D_00A590D8[];

void XrgPaint2DDrawRect(XrgPaint2D *paint, XrgPaint2DRect *rect)
{
    XrgPaint2DDrawReq *req;
    XrgPaint2DDrawReq *prev;

    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 0x3BA);
    }
    if ((unsigned int)paint->reqCount >= 0xFF) {
        assert_prog(D_00A590D8, D_00A59058, 0x3BB);
    }
    req = paint->request;
    *(int *)((char *)req + 8) = rect->mode;
    __asm__ __volatile__("lqc2 vf31, 0(%0)\n\tsqc2 vf31, 0(%1)"
                         : : "r"(&rect->x), "r"((char *)req + 0x10) : "memory");
    ((union { int i; float f; } *)((char *)req + 0x24))->f = rect->angle;
    ((union { int i; float f; } *)((char *)req + 0x20))->f = rect->scale;
    {
        int n = paint->reqCount + 1;
        prev = paint->request;
        paint->reqCount = n;
        paint->request = &(&paint->req)[n];
    }
    _InitReq(paint->request);
    _DeriveInfoReq(paint->request, prev);
}

extern void XrgPaint2DDrawRect(XrgPaint2D *paint, XrgPaint2DRect *rect);

void XrgPaint2DDrawXYWH(XrgPaint2D *paint, int mode, int x, int y,
                        int width, int height)
{
    XrgPaint2DRect rect;

    InitXrgPaint2DRect(&rect, mode);
    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    XrgPaint2DDrawRect(paint, &rect);
}

void InitXrgPaint2DLine3D(XrgPaint2DLine3D *line, float width)
{
    line->mode = 0;
    XrgClearVector(line->start);
    XrgClearVector(line->end);
    line->width = width;
}

INCLUDE_ASM("asm/nonmatchings/ov12/xrg_paint2d", XrgPaint2DDrawLine);

INCLUDE_ASM("asm/nonmatchings/ov12/xrg_paint2d", _Paint2DFlush);

static void _InitReq(XrgPaint2DDrawReq *req);

static void _Paint2DClearReq(XrgPaint2D *paint, void *pStudio) {
    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 0x484);
    }
    paint->reqCount = 0;
    paint->request = &paint->req;
    _InitReq(&paint->req);
}

extern RgDraw *InstanceOfRgDraw(void);
/*
 * RgDraw's _DrawMain (ov12:0x00a28320) calls both callbacks with the paint
 * and the studio being drawn; _Paint2DFlush passes that studio to
 * RgDrawStudioGetView.
 */
extern void RgDrawReq(RgDraw *draw, XrgPaint2D *paint,
                      void (*flush)(XrgPaint2D *paint, void *pStudio),
                      void (*clear)(XrgPaint2D *paint, void *pStudio),
                      int prio, int drawID);

static void _Paint2DFlush(XrgPaint2D *paint, void *pStudio);
static void _Paint2DClearReq(XrgPaint2D *paint, void *pStudio);

void XrgPaint2DFlush(XrgPaint2D *paint)
{
    if (paint == 0) {
        assert_prog(D_00A59098, D_00A59058, 1168);
    }
    RgDrawReq(InstanceOfRgDraw(), paint, _Paint2DFlush, _Paint2DClearReq,
             paint->prio, paint->drawID);
}

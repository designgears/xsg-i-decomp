#include "common.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "nml_packet_add.h"
#include "main/xgl_packet.h"

/*
 * GS A+D register entries: each one is a data doubleword followed by the
 * register address doubleword. 32-bit register values are written as two
 * words through the union.
 */
typedef union GsTagWord {
    u64 d;
    u32 w[2];
} GsTagWord;

extern GsTagWord g_aGsTag[];
extern int g_nGsEntry;

/*
 * Public inline helpers: gcc 2.96 inlines them into the callers below and
 * defers their out-of-line copies to the end of the object (after every other
 * function), in definition order.
 */
inline void packet_gs_entry32(u32 reg, u32 hi, u32 lo)
{
    g_aGsTag[g_nGsEntry * 2 + 2].w[0] = lo;
    g_aGsTag[g_nGsEntry * 2 + 2].w[1] = hi;
    g_aGsTag[g_nGsEntry++ * 2 + 3].d = reg;
}

inline void packet_gs_entry64(u32 reg, u64 *data)
{
    g_aGsTag[g_nGsEntry * 2 + 2].d = *data;
    g_aGsTag[g_nGsEntry++ * 2 + 3].d = reg;
}

inline void nmlPacketAddGsFba(u64 fba)
{
    g_aGsTag[g_nGsEntry * 2 + 2].d = fba;
    g_aGsTag[g_nGsEntry * 2 + 3].d = 74;
    g_nGsEntry++;
}

/* VIF command MSCAL: start the VU1 microprogram at immediate * 8. */
#define VIF_CODE_MSCAL 0x14000000

/* VIF command FLUSH: wait for the VU1 microprogram to finish. */
#define VIF_CODE_FLUSH 0x11000000

/*
 * One s_aUcodeTbl row (0x1c bytes; the 168-byte symbol holds six rows).
 * programs[type - 1] lists the VU1 program byte addresses of microcode family
 * `type`, indexed by the material's variant slot.
 */
typedef struct UcodeTableRow {
    u32 *programs[7];
} UcodeTableRow;

extern UcodeTableRow s_aUcodeTbl[];

extern int g_aSubWindow[4];

/* One VIF unpack value passed to sceVif1PkAddUpkData128 by value. */
typedef unsigned int Quadword __attribute__((mode(TI)));

typedef union NmlPacketData128 {
    u32 words[4];
    Quadword quad;
} NmlPacketData128;

extern void sceVif1PkAddUpkData128(NmlPacket packet, Quadword data);

/*
 * Every case ends with its own "queue the program or clear result" tail.  The
 * compiler cross-jumps the seven copies back into the single tail seen in the
 * original, but before that each copy is a reference to result, which is what
 * keeps result in a callee-saved register (s4).  Written once after the switch result is spilled to
 * the stack and the program pointers take s4..s8 instead.
 */

/*
 * _CurMatrixSet loads a 4x4 matrix into vf27..vf30, the VU0 macro-mode
 * registers this TU's packet builder keeps as its persistent "current
 * matrix" across _CurMatrixMul and _CurApplyMatrix (main VA 0x00236448,
 * ee-vu-cop2, docs/ps2-capabilities.md).
 */
static void _CurMatrixSet(const Matrix4 matrix)
{
    __asm__ __volatile__(
        "lqc2 vf27, 0(%0)\n\t"
        "lqc2 vf28, 16(%0)\n\t"
        "lqc2 vf29, 32(%0)\n\t"
        "lqc2 vf30, 48(%0)\n\t"
        "nop"
        :
        : "r"(matrix)
        : "memory"
    );
}

/*
 * _CurMatrixGet stores the VU0 macro-mode "current matrix" vf27..vf30
 * _CurMatrixSet/_CurSetMatrix left live back into the caller's 4x4 matrix
 * (main VA 0x00236460, ee-vu-cop2, docs/ps2-capabilities.md).
 */
static void _CurMatrixGet(Matrix4 matrix)
{
    __asm__ __volatile__(
        "sqc2 vf27, 0(%0)\n\t"
        "sqc2 vf28, 16(%0)\n\t"
        "sqc2 vf29, 32(%0)\n\t"
        "sqc2 vf30, 48(%0)\n\t"
        "nop"
        :
        : "r"(matrix)
        : "memory"
    );
}

/*
 * _CurMatrixMul loads a 4x4 matrix and concatenates it onto the VU0
 * macro-mode "current matrix" _CurMatrixSet left in vf27..vf30, writing the
 * product's three scaled rows back into vf27..vf29 (main VA 0x00236478,
 * ee-vu-cop2, docs/ps2-capabilities.md).
 */
static void _CurMatrixMul(const Matrix4 matrix)
{
    __asm__ __volatile__(
        "lqc2 vf2, 0(%0)\n\t"
        "lqc2 vf3, 16(%0)\n\t"
        "lqc2 vf4, 32(%0)\n\t"
        "lqc2 vf5, 48(%0)\n\t"
        "vmulax.xyzw ACC, vf27, vf2x\n\t"
        "vmadday.xyzw ACC, vf28, vf2y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf2z\n\t"
        "vmaddw.xyzw vf2, vf30, vf2w\n\t"
        "vmulax.xyzw ACC, vf27, vf3x\n\t"
        "vmadday.xyzw ACC, vf28, vf3y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf3z\n\t"
        "vmaddw.xyzw vf3, vf30, vf3w\n\t"
        "vmulax.xyzw ACC, vf27, vf4x\n\t"
        "vmadday.xyzw ACC, vf28, vf4y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf4z\n\t"
        "vmaddw.xyzw vf4, vf30, vf4w\n\t"
        "vmulax.xyzw ACC, vf27, vf5x\n\t"
        "vmadday.xyzw ACC, vf28, vf5y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf5z\n\t"
        "vmaddw.xyzw vf30, vf30, vf5w\n\t"
        "vmulw.xyzw vf27, vf2, vf0w\n\t"
        "vmulw.xyzw vf28, vf3, vf0w\n\t"
        "vmulw.xyzw vf29, vf4, vf0w\n\t"
        "nop"
        :
        : "r"(matrix)
        : "memory"
    );
}

/*
 * _CurApplyMatrix transforms one vector by the VU0 macro-mode "current
 * matrix" (vf27..vf30) a prior _CurMatrixSet/_CurMatrixMul left live,
 * storing the result to destination (main VA 0x002364e0, ee-vu-cop2,
 * docs/ps2-capabilities.md).
 */
static void _CurApplyMatrix(Vector4 *destination, const Vector4 *source)
{
    __asm__ __volatile__(
        "lqc2 vf31, 0(%0)\n\t"
        "vmulax.xyzw ACC, vf27, vf31x\n\t"
        "vmadday.xyzw ACC, vf28, vf31y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf31z\n\t"
        "vmaddw.xyzw vf31, vf30, vf0w\n\t"
        "sqc2 vf31, 0(%1)\n\t"
        "nop"
        :
        : "r"(source), "r"(destination)
        : "memory"
    );
}

/*
 * _CurMatrixMul33norm transforms the caller's three-row orientation matrix
 * by the VU0 macro-mode "current matrix" vf27..vf29 a prior _CurMatrixSet/
 * _CurMatrixMul left live, normalizes each transformed row to unit length
 * and stores the three rows to destination (main VA 0x00236500,
 * ee-vu-cop2, docs/ps2-capabilities.md).
 */
static void _CurMatrixMul33norm(Vector4 *destination, const Vector4 *source)
{
    __asm__ __volatile__(
        "lqc2 vf20, 0(%0)\n\t"
        "lqc2 vf21, 16(%0)\n\t"
        "lqc2 vf22, 32(%0)\n\t"
        "vmulax.xyz ACC, vf27, vf20x\n\t"
        "vmadday.xyz ACC, vf28, vf20y\n\t"
        "vmaddz.xyz vf20, vf29, vf20z\n\t"
        "vmulax.xyz ACC, vf27, vf21x\n\t"
        "vmadday.xyz ACC, vf28, vf21y\n\t"
        "vmaddz.xyz vf21, vf29, vf21z\n\t"
        "vmulax.xyz ACC, vf27, vf22x\n\t"
        "vmadday.xyz ACC, vf28, vf22y\n\t"
        "vmaddz.xyz vf22, vf29, vf22z\n\t"
        "vaddz.x vf25, vf0, vf20z\n\t"
        "vaddy.x vf24, vf0, vf20y\n\t"
        "vaddx.x vf23, vf0, vf20x\n\t"
        "vaddz.y vf25, vf0, vf21z\n\t"
        "vaddy.y vf24, vf0, vf21y\n\t"
        "vaddx.y vf23, vf0, vf21x\n\t"
        "vaddz.z vf25, vf0, vf22z\n\t"
        "vaddy.z vf24, vf0, vf22y\n\t"
        "vaddx.z vf23, vf0, vf22x\n\t"
        "vmula.xyz ACC, vf23, vf23\n\t"
        "vmadda.xyz ACC, vf24, vf24\n\t"
        "vmadd.xyz vf10, vf25, vf25\n\t"
        "vrsqrt Q, vf0w, vf10x\n\t"
        "vwaitq\n\t"
        "vmulq.x vf23, vf23, Q\n\t"
        "vmulq.x vf24, vf24, Q\n\t"
        "vmulq.x vf25, vf25, Q\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vrsqrt Q, vf0w, vf10y\n\t"
        "vwaitq\n\t"
        "vmulq.y vf23, vf23, Q\n\t"
        "vmulq.y vf24, vf24, Q\n\t"
        "vmulq.y vf25, vf25, Q\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vrsqrt Q, vf0w, vf10z\n\t"
        "vwaitq\n\t"
        "vmulq.z vf23, vf23, Q\n\t"
        "vmulq.z vf24, vf24, Q\n\t"
        "vmulq.z vf25, vf25, Q\n\t"
        "sqc2 vf23, 0(%1)\n\t"
        "sqc2 vf24, 16(%1)\n\t"
        "sqc2 vf25, 32(%1)\n\t"
        "nop"
        :
        : "r"(source), "r"(destination)
        : "memory"
    );
}

/*
 * _CurSetViewScaleTrans loads the view-scale vector into the VU0 macro-mode
 * register vf25 and the view-translation vector into vf26, the same
 * persistent "current matrix" state family the _CurMatrixSet/_CurMatrixMul/
 * _CurApplyMatrix group reads (main VA 0x002365c0, ee-vu-cop2,
 * docs/ps2-capabilities.md).
 */
static void _CurSetViewScaleTrans(const Vector4 *view_scale, const Vector4 *view_translation)
{
    __asm__ __volatile__(
        "lqc2 vf25, 0(%0)\n\t"
        "lqc2 vf26, 0(%1)\n\t"
        "nop"
        :
        : "r"(view_scale), "r"(view_translation)
        : "memory"
    );
}

static int _CurRotTransPersClip(Vector4 *destination, Vector4 *destination2, const Vector4 *vector, const Vector4 *vector2) {
    int clip;
    __asm__ __volatile__(
        "ctc2.ni $0, $vi18\n\t"
        "lqc2 $vf31, 0(%3)\n\t"
        "lqc2 $vf20, 0(%4)\n\t"
        "vmulax.xyzw ACC, $vf27, $vf31x\n\t"
        "vmadday.xyzw ACC, $vf28, $vf31y\n\t"
        "vmaddaz.xyzw ACC, $vf29, $vf31z\n\t"
        "vmaddw.xyzw $vf31, $vf30, $vf0w\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vclipw.xyz $vf31xyz, $vf31w\n\t"
        "vdiv Q, $vf0w, $vf31w\n\t"
        "vwaitq\n\t"
        "vmulq.xyzw $vf31, $vf31, Q\n\t"
        "vmulq.xyzw $vf20, $vf20, Q\n\t"
        "vmulaw.xyzw ACC, $vf26, $vf0w\n\t"
        "vmadd.xyzw $vf31, $vf31, $vf25\n\t"
        "vftoi4.xyw $vf23, $vf31\n\t"
        "vftoi0.z $vf23, $vf31\n\t"
        "vsub.w $vf23, $vf23, $vf23\n\t"
        "sqc2 $vf23, 0(%1)\n\t"
        "sqc2 $vf20, 0(%2)\n\t"
        "cfc2.ni $2, $vi18\n\t"
        "nop\n\t"
        "andi $2, $2, 0x3f\n\t"
        "daddu %0, $2, $0"
        : "=r"(clip)
        : "r"(destination), "r"(destination2), "r"(vector), "r"(vector2)
        : "$2", "memory"
    );
    return clip;
}

/*
 * _CurSetMatrix loads a 4x4 matrix into the VU0 macro-mode "current matrix"
 * registers vf27..vf30, the same persistent state _CurMatrixSet writes (main
 * VA 0x00236640, ee-vu-cop2, docs/ps2-capabilities.md).
 */
static void _CurSetMatrix(const Matrix4 matrix)
{
    __asm__ __volatile__(
        "lqc2 vf27, 0(%0)\n\t"
        "lqc2 vf28, 16(%0)\n\t"
        "lqc2 vf29, 32(%0)\n\t"
        "lqc2 vf30, 48(%0)\n\t"
        "nop"
        :
        : "r"(matrix)
        : "memory"
    );
}

extern char FD_28[] __asm__("D_28-0x28");
extern char FD_28_1[] __asm__("D_28-0x28");
extern char FD_28_2[] __asm__("D_28-0x28");
extern char FD_28_3[] __asm__("D_28-0x28");
extern char FD_28_4[] __asm__("D_28-0x28");
extern char FD_28_5[] __asm__("D_28-0x28");
extern char FD_28_6[] __asm__("D_28-0x28");

void nmlPacketAddGsFlush(void)
{
    s_pPacket = xglPacketGetCurrent();
    if (g_nGsEntry > 0) {
        g_aGsTag[0].w[1] = 0x10000000;
        g_aGsTag[1].w[0] = 0xE;
        g_aGsTag[0].w[0] = g_nGsEntry + 0x8000;
        g_aGsTag[1].w[1] = 0;
        sceVif1PkCnt(s_pPacket, 0);
        sceVif1PkOpenUpkCode(s_pPacket, 0x3D5, 0x6C, 1, 1);
        sceVif1PkAddUpkData128N(s_pPacket, g_aGsTag, g_nGsEntry + 1);
        sceVif1PkCloseUpkCode(s_pPacket);
        switch (s_nProgType) {
        case 7:
            sceVif1PkAddCode(s_pPacket, ((u32)FD_28 >> 3) | VIF_CODE_MSCAL);
            break;
        case 1:
            sceVif1PkAddCode(s_pPacket, ((u32)FD_28_1 >> 3) | VIF_CODE_MSCAL);
            break;
        case 2:
            sceVif1PkAddCode(s_pPacket, ((u32)FD_28_2 >> 3) | VIF_CODE_MSCAL);
            break;
        case 3:
            sceVif1PkAddCode(s_pPacket, ((u32)FD_28_3 >> 3) | VIF_CODE_MSCAL);
            break;
        case 4:
            sceVif1PkAddCode(s_pPacket, ((u32)FD_28_4 >> 3) | VIF_CODE_MSCAL);
            break;
        case 5:
            sceVif1PkAddCode(s_pPacket, ((u32)FD_28_5 >> 3) | VIF_CODE_MSCAL);
            break;
        case 6:
            sceVif1PkAddCode(s_pPacket, ((u32)FD_28_6 >> 3) | VIF_CODE_MSCAL);
            break;
        }
        sceVif1PkAddCode(s_pPacket, 0x17000000);
        nmlPacketAddWaitMicrocode();
    }
    g_nGsEntry = 0;
}

extern char D_28[];
extern char D_28_1[] __asm__("D_28");
extern char D_28_2[] __asm__("D_28");
extern char D_28_3[] __asm__("D_28");
extern char D_28_4[] __asm__("D_28");
extern char D_28_5[] __asm__("D_28");
extern char D_28_6[] __asm__("D_28");

void nmlPacketAddGsFlushWide(void)
{
    s_pPacket = xglPacketGetCurrent();
    if (g_nGsEntry > 0) {
        g_aGsTag[0].w[1] = 0x10000000;
        g_aGsTag[1].w[0] = 0xE;
        g_aGsTag[0].w[0] = g_nGsEntry + 0x8000;
        g_aGsTag[1].w[1] = 0;
        sceVif1PkCnt(s_pPacket, 0);
        nmlPacketAddWaitMicrocode();
        sceVif1PkOpenUpkCode(s_pPacket, 0x2DC, 0x6C, 1, 1);
        sceVif1PkAddUpkData128N(s_pPacket, g_aGsTag, g_nGsEntry + 1);
        sceVif1PkCloseUpkCode(s_pPacket);
        switch (s_nProgType) {
        case 7:
            sceVif1PkAddCode(s_pPacket, ((u32)D_28 >> 3) | VIF_CODE_MSCAL);
            break;
        case 1:
            sceVif1PkAddCode(s_pPacket, ((u32)D_28_1 >> 3) | VIF_CODE_MSCAL);
            break;
        case 2:
            sceVif1PkAddCode(s_pPacket, ((u32)D_28_2 >> 3) | VIF_CODE_MSCAL);
            break;
        case 3:
            sceVif1PkAddCode(s_pPacket, ((u32)D_28_3 >> 3) | VIF_CODE_MSCAL);
            break;
        case 4:
            sceVif1PkAddCode(s_pPacket, ((u32)D_28_4 >> 3) | VIF_CODE_MSCAL);
            break;
        case 5:
            sceVif1PkAddCode(s_pPacket, ((u32)D_28_5 >> 3) | VIF_CODE_MSCAL);
            break;
        case 6:
            sceVif1PkAddCode(s_pPacket, ((u32)D_28_6 >> 3) | VIF_CODE_MSCAL);
            break;
        }
        sceVif1PkAddCode(s_pPacket, 0x17000000);
        nmlPacketAddWaitMicrocode();
    }
    g_nGsEntry = 0;
}

extern struct { char pad[8]; u16 zbp; } sRender;

void nmlPacketAddReflectParam(NmlMaterialRenderState *material, NmlModelRenderState *model)
{
  union { u32 w[20]; u64 d[10]; } packet;
  int color[4];
  float reflect;
  u32 new_var;
  u32 tbw;
  *((u32 *) (&s_pPacket)) = (u32) xglPacketGetCurrent();
  reflect = *((float *) (((u8 *) model) + 0x224));
  if ((model->render_status & 0x100000) && (strstr(material->name, eye_name) != 0))
    reflect = 2.0f;
  sceVif1PkCnt(s_pPacket, 0);
  packet.w[0] = 0x8004;
  tbw = ((union { u16 h[16]; } *) (&sRender))->h[13];
  new_var = ((u32 *) material)[0x4A];
  packet.w[1] = 0x10000000;
  packet.w[2] = 0xE;
  packet.d[3] = 0x3F;
  packet.w[8] = ((u32 *) material)[0x48] + (tbw << 5);
  packet.w[9] = ((u32 *) material)[0x49] + (tbw << 10);
  packet.d[5] = 6;
  packet.w[12] = new_var;
  packet.w[13] = ((u32 *) material)[0x4B];
  packet.d[7] = 0x14;
  packet.w[16] = ((u32 *) material)[0x4C];
  packet.w[17] = ((u32 *) material)[0x4D];
  packet.w[3] = 0;
  packet.d[2] = 0;
  packet.d[9] = 8;
  sceVif1PkOpenUpkCode(s_pPacket, 0x3D5, 0x6C, 1, 1);
  sceVif1PkAddUpkData128N(s_pPacket, &packet, 5);
  sceVif1PkCloseUpkCode(s_pPacket);
  tbw = ((union { u16 h[16]; } *) (&sRender))->h[13];
  packet.w[8] = ((u32 *) material)[0x4E] + (tbw << 5);
  packet.w[9] = ((u32 *) material)[0x4F] + (tbw << 10);
  packet.w[12] = ((u32 *) material)[0x50];
  packet.w[13] = ((u32 *) material)[0x51];
  packet.w[16] = ((u32 *) material)[0x52];
  packet.w[17] = ((u32 *) material)[0x53];
  sceVif1PkOpenUpkCode(s_pPacket, 0x3DA, 0x6C, 1, 1);
  sceVif1PkAddUpkData128N(s_pPacket, &packet, 5);
  sceVif1PkCloseUpkCode(s_pPacket);
  color[0] = (*((float *) (((u8 *) material) + 0xFC))) * 128.0f;
  if (reflect < 2.0f) color[0] = reflect * 128.0f;
  color[1] = color[0]; color[2] = color[0]; color[3] = color[0];
  sceVif1PkOpenUpkCode(s_pPacket, 0x3F1, 0x6C, 1, 1);
  sceVif1PkAddUpkData128N(s_pPacket, color, 1);
  sceVif1PkCloseUpkCode(s_pPacket);
}

extern int D_004AE5A0[];
extern u8 D_004AE5B0[];
extern struct {
    u8 _unmodeled_00[0x10];
    u32 flags;
} GameLoopState;
typedef union NmlLightColor {
    Quadword rows[4];
    float m[4][4];
} NmlLightColor;
extern NmlLightColor s_inLc0;
extern int s_nPixelTestType;
extern int s_nModelParts;
extern void *s_pCacheTexture;
extern void *s_pMatrixCache;
extern void *s_pModelCache;
extern void *s_pLightLayout;
extern float xglSin(float angle);

void nmlPacketAddTransMicrocodeInit(void)
{
    float matrix[4][4];

    s_pPacket = xglPacketGetCurrent();
    sceVif1PkRef(s_pPacket, D_004AE5B0, D_004AE5A0[0], 0, 0, 0);
    s_nProgType = 2;
    s_nPixelTestType = -1;
    s_pCacheTexture = 0;
    s_pMatrixCache = 0;
    s_pModelCache = 0;
    s_nModelParts = 0;
    s_pLightLayout = 0;
    s_inReflRotX = xglSin(s_inReflRotY);
    if (!(GameLoopState.flags & 1))
        s_inReflRotY += 0.01f;
    xglMatrixUnit(matrix);
    sceVif1PkCnt(s_pPacket, 0);
    sceVif1PkOpenUpkCode(s_pPacket, 0x3EC, 0x6C, 1, 1);
    sceVif1PkAddUpkData128N(s_pPacket, matrix, 3);
    sceVif1PkCloseUpkCode(s_pPacket);
    s_nReflRotType = 1;
    __asm__ __volatile__("sq $0, 0(%0)\n\tsq $0, 16(%0)\n\tsq $0, 32(%0)\n\tsq $0, 48(%0)"
                         : : "r"(&s_inLc0) : "memory");
    s_inLc0.m[3][3] = 1.0f;
    s_inLc0.m[3][2] = 1.0f;
    s_inLc0.m[3][1] = 1.0f;
    s_inLc0.m[3][0] = 1.0f;
}

extern int D_004B0A40[];
extern u8 D_004B0A50[];
extern int D_004B19E0[];
extern u8 D_004B19F0[];
extern int D_004B54A0[];
extern u8 D_004B54B0[];
extern int D_004B6F70[];
extern u8 D_004B6F80[];
extern int D_004B9700[];
extern u8 D_004B9710[];
extern int D_004BD490[];
extern u8 D_004BD4A0[];
extern u8 s_aNormalTbl[];

void nmlPacketAddTransMicrocode(const NmlMaterialRenderState *material,
                                const NmlModelRenderState *model)
{
    u32 status;
    int type;
    int prog;

    *(int *)&s_pPacket = (int)xglPacketGetCurrent();
    status = model->render_status;
    type = 2;
    if (status & 0x8000) {
        type = 6;
    } else if (status & 0x800) {
        type = 3;
    } else if (status & 0x80000) {
        type = 7;
    } else {
        if (status & 0x2001000) {
            type = 5;
            if (material->render_flags & 1)
                type = 4;
            else if ((*(u64 *)&material->material_flags & 0x10010) == 0x10)
                type = 2;
        } else if (material->render_flags & 1) {
            type = 1;
        }
    }
    switch (type) {
    case 3:
        prog = 3;
        if (s_nProgType == prog)
            return;
        sceVif1PkRef(s_pPacket, D_004B54B0, D_004B54A0[0], 0, 0, 0);
        s_nProgType = prog;
        sceVif1PkCnt(s_pPacket, 0);
        sceVif1PkOpenUpkCode(s_pPacket, 0x350, 0x6C, 1, 1);
        sceVif1PkAddUpkData128N(s_pPacket, s_aNormalTbl, 0x72);
        sceVif1PkCloseUpkCode(s_pPacket);
        return;
    case 4:
        prog = 4;
        if (s_nProgType == prog)
            return;
        sceVif1PkRef(s_pPacket, D_004B9710, D_004B9700[0], 0, 0, 0);
        s_nProgType = prog;
        return;
    case 5:
        prog = 5;
        if (s_nProgType == prog)
            return;
        sceVif1PkRef(s_pPacket, D_004B6F80, D_004B6F70[0], 0, 0, 0);
        s_nProgType = prog;
        sceVif1PkCnt(s_pPacket, 0);
        sceVif1PkOpenUpkCode(s_pPacket, 0x350, 0x6C, 1, 1);
        sceVif1PkAddUpkData128N(s_pPacket, s_aNormalTbl, 0x72);
        sceVif1PkCloseUpkCode(s_pPacket);
        return;
    case 6:
        prog = 6;
        if (s_nProgType == prog)
            return;
        sceVif1PkRef(s_pPacket, D_004B0A50, D_004B0A40[0], 0, 0, 0);
        s_nProgType = prog;
        return;
    case 7:
        prog = 7;
        if (s_nProgType == prog)
            return;
        sceVif1PkRef(s_pPacket, D_004BD4A0, D_004BD490[0], 0, 0, 0);
        s_nProgType = prog;
        return;
    case 1:
        prog = 1;
        if (s_nProgType == prog)
            return;
        sceVif1PkRef(s_pPacket, D_004B19F0, D_004B19E0[0], 0, 0, 0);
        s_nProgType = prog;
        return;
    case 2:
        prog = 2;
        if (s_nProgType == prog)
            return;
        sceVif1PkRef(s_pPacket, D_004AE5B0, D_004AE5A0[0], 0, 0, 0);
        s_nProgType = prog;
        return;
    }
}

void nmlPacketAddScreen(StudioCamera *camera)
{
    NmlPacketData128 offset;

    s_pPacket = xglPacketGetCurrent();
    sceVif1PkCnt(s_pPacket, 0);
    __asm__ __volatile__("lq $2,0x0(%1)\n\tsq $2,0x0(%0)" : : "r"(&offset), "r"(&camera->screenOffset) : "$2", "memory");
    ((float *)offset.words)[3] = 0.0f;
    if ((((u16 *)&sRender)[13] << 5) != 0x3800)
        ((float *)offset.words)[3] = 3.1415927f;
    sceVif1PkOpenUpkCode(s_pPacket, 0x3C7, 0x6C, 1, 1);
    sceVif1PkAddUpkData128(s_pPacket, offset.quad);
    sceVif1PkCloseUpkCode(s_pPacket);
    sceVif1PkOpenUpkCode(s_pPacket, 0x3C6, 0x6C, 1, 1);
    sceVif1PkAddUpkData128(s_pPacket, *(Quadword *)&camera->screenScale);
    sceVif1PkCloseUpkCode(s_pPacket);
}

void nmlPacketAddPixelTestPacket(int test_reg, u64 test1, u64 test, int zbuf_reg,
                                 u64 test1_zbuf, u64 zbuf)
{
    struct {
        u32 tag[4];
        u64 data[4];
    } packet;

    s_pPacket = xglPacketGetCurrent();
    sceVif1PkCnt(s_pPacket, 0);
    packet.tag[0] = 0x8002;
    packet.tag[1] = 0x10000000;
    packet.tag[2] = 0xE;
    packet.tag[3] = 0;
    packet.data[0] = test;
    packet.data[1] = test_reg;
    packet.data[2] = zbuf;
    packet.data[3] = zbuf_reg;
    sceVif1PkOpenUpkCode(s_pPacket, 0x3FA, 0x6C, 1, 1);
    sceVif1PkAddUpkData128N(s_pPacket, &packet, 3);
    sceVif1PkCloseUpkCode(s_pPacket);
    packet.data[1] = test_reg;
    packet.data[0] = test1;
    packet.data[3] = zbuf_reg;
    packet.data[2] = test1_zbuf;
    sceVif1PkOpenUpkCode(s_pPacket, 0x3FD, 0x6C, 1, 1);
    sceVif1PkAddUpkData128N(s_pPacket, &packet, 3);
    sceVif1PkCloseUpkCode(s_pPacket);
}

void nmlPacketAddWaitMicrocode(void)
{
    s_pPacket = xglPacketGetCurrent();
    sceVif1PkCnt(s_pPacket, 0);
    sceVif1PkAddCode(s_pPacket, VIF_CODE_FLUSH);
}

extern int s_nLightPara;

void nmlPacketAddBlockMaterial(NmlMaterialRenderState *material, NmlModelRenderState *model,
                               int keep_alpha, int viewport)
{
    struct {
        int code;
        float color;
        int scaled;
        int priority;
    } param, *p;
    union {
        int i[4];
        float f[4];
    } fog;
    float length;
    union {
        int i;
        float f;
    } scale;
    u8 *proreal;
    float alpha;
    float base;
    float color;
    float ratio;

    proreal = (u8 *)model->proreal;
    if (model->proreal_override != 0)
        proreal = (u8 *)model->proreal_override;
    s_pPacket = xglPacketGetCurrent();
    sceVif1PkCnt(s_pPacket, 0);
    base = *(float *)((u8 *)material + 0xC4);
    alpha = base * *(float *)((u8 *)model + 0x220);
    ((union { float f; } *)((u8 *)material + 0xDC))->f = alpha;
    scale.i = *(int *)(*(u8 **)((u8 *)model + 0x258) + *(int *)((u8 *)material + 0x2C) * 64 + 0x1C);
    if (scale.i & 1)
        *(float *)((u8 *)material + 0xDC) = alpha * scale.f;
    if (keep_alpha != 0)
        *(float *)((u8 *)material + 0xDC) = base;
    sceVif1PkOpenUpkCode(s_pPacket, 0x3C8, 0x6C, 1, 1);
    sceVif1PkAddUpkData128N(s_pPacket, (u8 *)material + 0xD0, 5);
    sceVif1PkCloseUpkCode(s_pPacket);
    p = &param;
    color = *(float *)((u8 *)model + 0x228);
    if (viewport != *(int *)((u8 *)model + 0x27C))
        color = ((float *)((u8 *)model + 0x300))[viewport];
    if ((model->render_status & 0x100000) &&
        (strstr(material->name, lenz_name) != 0 || strstr(material->name, eye_name) != 0))
        color = *(float *)((u8 *)model + 0x2BC);
    ratio = *(float *)((u8 *)model + 0x22C);
    p->code = *(int *)((u8 *)material + 0xCC);
    ((union { float f; } *)&param.color)->f = color;
    p->scaled = 0;
    if (ratio > 1.02f || ratio < 0.98f)
        p->scaled = 1;
    xglVectorLength(&length, (Vector4 *)(*(u8 **)((u8 *)model + 0x258) +
                                         *(int *)((u8 *)material + 0x2C) * 64));
    if (length > 1.02f || length < 0.98f)
        p->scaled = 1;
    if (material->render_flags & 1) {
        xglVectorLength(&length, (Vector4 *)(*(u8 **)((u8 *)model + 0x258) +
                                             *(u16 *)((u8 *)material + 0x32) * 64 - 64));
        if (length > 1.02f || length < 0.98f)
            p->scaled = 1;
    }
    p->priority = *(signed char *)((u8 *)material + 0x16F);
    sceVif1PkOpenUpkCode(s_pPacket, 0x3F0, 0x6C, 1, 1);
    sceVif1PkAddUpkData128N(s_pPacket, &param, 1);
    sceVif1PkCloseUpkCode(s_pPacket);
    fog.i[2] = 0;
    if (*(u32 *)((u8 *)model + 0x230) & 1)
        fog.f[2] = *(float *)((u8 *)model + 0x230);
    sceVif1PkOpenUpkCode(s_pPacket, 0x3F9, 0x6C, 1, 1);
    sceVif1PkAddUpkData128N(s_pPacket, &fog, 1);
    sceVif1PkCloseUpkCode(s_pPacket);
    if (model->render_status & 0x800) {
        sceVif1PkOpenUpkCode(s_pPacket, 0x3C9, 0x6C, 1, 1);
        sceVif1PkAddUpkData128N(s_pPacket, proreal + 0x140, 1);
        sceVif1PkCloseUpkCode(s_pPacket);
    }
    *(int *)&s_pLightLayout = (int)model;
    s_nLightPara = material->material_flags & 0x20000;
    sceVif1PkCnt(s_pPacket, 0);
    sceVif1PkOpenUpkCode(s_pPacket, 0x3D1, 0x6C, 1, 1);
    if (model->render_status & 0x800) {
        sceVif1PkAddUpkData128N(s_pPacket, proreal + 0xC0, 4);
    } else if (model->render_status & 0x80000) {
        *(float *)((u8 *)model + 0x3C) =
            0x80 - (int)((float)*(int *)((u8 *)model + 0x270) * *(float *)((u8 *)model + 0x220) *
                         model->fog_intensity[viewport]);
        sceVif1PkAddUpkData128N(s_pPacket, model, 4);
    } else {
        sceVif1PkAddUpkData128N(s_pPacket, model, 4);
    }
    sceVif1PkCloseUpkCode(s_pPacket);
    if (material->material_flags & 0x20000) {
        sceVif1PkOpenUpkCode(s_pPacket, 0x3D1, 0x6C, 1, 1);
        sceVif1PkAddUpkData128N(s_pPacket, &s_inLc0, 4);
        sceVif1PkCloseUpkCode(s_pPacket);
    }
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct NmlAttributePacket NmlAttributePacket;

typedef union GsTag {
    u64 value;
    u32 words[2];
} GsTag;

typedef struct NmlModelResource {
    unsigned char unmodeled_000[0x6c];
    u32 texture_reference;
} NmlModelResource;

typedef struct NmlTextureResourceSet {
    int count;
    unsigned char unmodeled_04[0x2c];
} NmlTextureResourceSet;

typedef struct NmlTextureResourceEntry {
    short width;
    unsigned char unmodeled_02[2];
    short height;
    unsigned char unmodeled_06[2];
    short psm;
    unsigned char unmodeled_0a[2];
    short buffer_width;
    unsigned char unmodeled_0e[2];
    int vram_offset;
    int pixel_bytes;
    unsigned char unmodeled_18[0x28];
} NmlTextureResourceEntry;

typedef struct XglLoadImage {
    u64 giftag;
    unsigned char unmodeled_08[0x58];
} XglLoadImage;

typedef struct NmlScreenGsScratchEntry {
    GsTag payload;
    GsTag address;
} NmlScreenGsScratchEntry;

typedef union NmlCurrentPacket {
    XglPacket *sdk;
    NmlAttributePacket *attributes;
} NmlCurrentPacket;

typedef union NmlAttributeCursor {
    u8 *bytes;
    Quadword *quadwords;
} NmlAttributeCursor;

struct NmlAttributePacket {
    unsigned char unmodeled_00[0x20];
    u32 limit;
    NmlAttributeCursor cursor;
};

typedef struct NmlPixelTestPacket {
    u32 loops;
    u32 format;
    u32 registers;
    u32 register_high;
    u64 first_value;
    u64 first_address;
    u64 second_value;
    u64 second_address;
} NmlPixelTestPacket;

typedef struct NmlZbufRenderState {
    unsigned char unmodeled_00[2];
    short frame_psm;
    short frame_width;
    short frame_height; /* +0x6 */
    u16 depth_buffer_base;
    unsigned char unmodeled_0a[0x10]; /* +0xa */
    u16 texture_buffer_base; /* +0x1a */
    unsigned char unmodeled_1c[4]; /* +0x1c */
    u16 display_buffer_base; /* +0x20 */
} NmlZbufRenderState;

extern float D_004D8970;
extern const float D_004D8974;
typedef union NmlCircleVector {
    Vector4 vector;
    int words[4];
    u32 unsigned_words[4];
} NmlCircleVector;
typedef union NmlCircleOffsetTable {
    float values[4];
    u64 pairs[2];
} NmlCircleOffsetTable;
typedef struct NmlCircleScratchpad {
    NmlCircleVector position[4];
    NmlCircleVector transform[4];
    NmlCircleVector projected[4];
    NmlCircleVector perspective[4];
} NmlCircleScratchpad;
typedef struct NmlCircleGsRecord {
    GsTag value;
    GsTag address;
} NmlCircleGsRecord;
extern const NmlCircleOffsetTable D_004D3090;
extern const NmlCircleOffsetTable D_004D30A0;
static inline NmlCircleGsRecord *nmlCircleGsRecord(unsigned int index)
{
    return (NmlCircleGsRecord *)(0x70001000u +
                                 index * sizeof(NmlCircleGsRecord));
}
static inline void nmlCircleSetGsRecord(NmlCircleGsRecord *record,
                                        int address, u64 value)
{
    record->value.value = value;
    record->address.value = address;
}
static inline int nmlCircleSetPositionRecord(NmlCircleGsRecord *record,
                                              u64 packed_position,
                                              const int *clip_flags,
                                              int previous_flags,
                                              int earlier_flags)
{
    int flags;

    record->value.value = packed_position;
    flags = *clip_flags;
    record->address.value = (flags | previous_flags) | earlier_flags;
    return flags;
}
static inline u64 nmlCircleMakeTextureParameter(u32 texture_base)
{
    int low = 0x19304000;
    u64 parameter;

    low |= texture_base;
    parameter = low;
    parameter |= (u64)(texture_base + 0x80u) << 37;
    parameter |= 0x2000000580000000ULL;
    return parameter;
}

void nmlPacketMakeCircleTexture(NmlModelRenderState *model,
                                const StudioCamera *camera)
{
    NmlCircleScratchpad *scratch = (NmlCircleScratchpad *)0x70000000;
    NmlCircleVector *transform = (NmlCircleVector *)0x70000040;
    float radius_scale;
    float height_offset;
    float *matrix;
    NmlCircleOffsetTable x_offsets;
    NmlCircleOffsetTable z_offsets;
    u32 texture_base;
    int i;
    int first_clip_flags;
    int second_clip_flags;
    int third_clip_flags;

    x_offsets = D_004D3090;
    z_offsets = D_004D30A0;

    matrix = (float *)transform;

    matrix[14] = 1.0f;
    matrix[0] = 0.0f;
    matrix[1] = 0.0f;
    matrix[2] = 1.0f;
    matrix[4] = 1.0f;
    matrix[5] = 0.0f;
    matrix[6] = 1.0f;
    matrix[8] = 0.0f;
    matrix[9] = 1.0f;
    matrix[10] = 1.0f;
    matrix[12] = 1.0f;
    radius_scale = D_004D8970;
    matrix[13] = 1.0f;

    _CurSetViewScaleTrans(&camera->screenScale, &camera->screenOffset);
    _CurSetMatrix(camera->viewMatrix);
    height_offset = D_004D8974;

    for (i = 0; i < 4;) {
        int clip_flags;
        NmlCircleVector *position = &scratch->position[i];
        NmlCircleVector *projected = &scratch->projected[i];
        NmlCircleVector *perspective = &scratch->perspective[i];
        NmlCircleVector *vertex_transform = &transform[i];

        position->vector.x =
            model->circle_center_x +
            x_offsets.values[i] * model->circle_scale * radius_scale;
        position->vector.z =
            model->circle_center_z +
            z_offsets.values[i] * model->circle_scale * radius_scale;
        position->vector.y =
            model->projection_elements[13] + height_offset;
        i++;
        if (model->shadow_height_enabled != 0)
            position->vector.y =
                model->shadow_height + height_offset;

        clip_flags = _CurRotTransPersClip(&projected->vector,
            &perspective->vector, &position->vector, &vertex_transform->vector);
        if (clip_flags != 0)
            projected->words[3] = 12;
        else
            projected->words[3] = 4;
    }

    nmlCircleSetGsRecord(nmlCircleGsRecord(0), 0x3f, 0);
    texture_base = ((u16 *)&sRender)[13] << 5;
    nmlCircleSetGsRecord(nmlCircleGsRecord(1), 6,
                         nmlCircleMakeTextureParameter(texture_base));
    nmlCircleSetGsRecord(nmlCircleGsRecord(2), 0x14, 0x60);
    nmlCircleSetGsRecord(nmlCircleGsRecord(3), 0x47, 0x71001);
    nmlCircleSetGsRecord(nmlCircleGsRecord(4), 8, 0);
    nmlCircleSetGsRecord(nmlCircleGsRecord(5), 0x42, 0x89);
    nmlCircleSetGsRecord(nmlCircleGsRecord(6), 0, 0x54);
    nmlCircleSetGsRecord(nmlCircleGsRecord(7), 1,
        ((u64)scratch->perspective[0].unsigned_words[2] << 32) | 0x80808080);
    nmlCircleSetGsRecord(nmlCircleGsRecord(8), 2,
        scratch->perspective[0].unsigned_words[0] |
        ((u64)scratch->perspective[0].unsigned_words[1] << 32));
    first_clip_flags = nmlCircleSetPositionRecord(nmlCircleGsRecord(9),
        scratch->projected[0].words[0] |
        ((u64)scratch->projected[0].words[1] << 16) |
        ((u64)scratch->projected[0].words[2] << 32),
        &scratch->projected[0].words[3], 0, 0);

    nmlCircleSetGsRecord(nmlCircleGsRecord(10), 1,
        ((u64)scratch->perspective[1].unsigned_words[2] << 32) | 0x80808080);
    nmlCircleSetGsRecord(nmlCircleGsRecord(11), 2,
        scratch->perspective[1].unsigned_words[0] |
        ((u64)scratch->perspective[1].unsigned_words[1] << 32));
    second_clip_flags = nmlCircleSetPositionRecord(nmlCircleGsRecord(12),
        scratch->projected[1].words[0] |
        ((u64)scratch->projected[1].words[1] << 16) |
        ((u64)scratch->projected[1].words[2] << 32),
        &scratch->projected[1].words[3], first_clip_flags, 0);

    nmlCircleSetGsRecord(nmlCircleGsRecord(13), 1,
        ((u64)scratch->perspective[2].unsigned_words[2] << 32) | 0x80808080);
    nmlCircleSetGsRecord(nmlCircleGsRecord(14), 2,
        scratch->perspective[2].unsigned_words[0] |
        ((u64)scratch->perspective[2].unsigned_words[1] << 32));
    third_clip_flags = nmlCircleSetPositionRecord(nmlCircleGsRecord(15),
        scratch->projected[2].words[0] |
        ((u64)scratch->projected[2].words[1] << 16) |
        ((u64)scratch->projected[2].words[2] << 32),
        &scratch->projected[2].words[3], second_clip_flags, first_clip_flags);

    nmlCircleSetGsRecord(nmlCircleGsRecord(16), 1,
        ((u64)scratch->perspective[3].unsigned_words[2] << 32) | 0x80808080);
    nmlCircleSetGsRecord(nmlCircleGsRecord(17), 2,
        scratch->perspective[3].unsigned_words[0] |
        ((u64)scratch->perspective[3].unsigned_words[1] << 32));
    nmlCircleSetPositionRecord(nmlCircleGsRecord(18),
        scratch->projected[3].words[0] |
        ((u64)scratch->projected[3].words[1] << 16) |
        ((u64)scratch->projected[3].words[2] << 32),
        &scratch->projected[3].words[3], third_clip_flags, second_clip_flags);

    g_nGsEntry = 0;
    {
        int remaining;
        NmlCircleGsRecord *cursor = (NmlCircleGsRecord *)0x70001000;

        for (remaining = 18; remaining >= 0; remaining--) {
            packet_gs_entry64(cursor->address.words[0],
                                   &cursor->value.value);
            cursor++;
        }
    }
    nmlPacketAddGsFlushWide();
}

typedef struct NmlGsLoadImage {
    u64 giftag0[2];
    u64 bitbltbuf[2];
    u64 trxpos[2];
    u64 trxreg[2];
    u64 trxdir[2];
    u64 giftag1[2];
} NmlGsLoadImage;

extern int sceGsSetDefLoadImage(NmlGsLoadImage *lp, short dbp, short dbw, short dpsm,
                                short x, short y, short w, short h);

extern void sceVif1PkOpenDirectCode(XglPacket *packet, int mode);

extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int count);

extern void sceVif1PkCloseDirectCode(XglPacket *packet);

extern u8 *nmlPacketSetAttributeData16N(const void *data, u32 count);

void nmlPacketSendCircleTexture(void *texture, void *clut)
{
    NmlGsLoadImage load;

    s_pPacket = xglPacketGetCurrent();
    sceVif1PkCnt(s_pPacket, 0);
    sceVif1PkAddCode(s_pPacket, VIF_CODE_FLUSH);
    sceGsSetDefLoadImage(&load, ((union { u16 h[16]; } *)&sRender)->h[13] << 5, 1, 0x13,
                         0, 0, 0x40, 0x40);
    load.giftag0[0] |= 0x8000;
    sceVif1PkOpenDirectCode(s_pPacket, 0);
    sceVif1PkAddDirectDataN(s_pPacket, &load, 5);
    sceVif1PkCloseDirectCode(s_pPacket);
    sceVif1PkRef(s_pPacket, texture, 0x102, 0, 0, 0);
    sceVif1PkCnt(s_pPacket, 0);
    sceGsSetDefLoadImage(&load, (((union { u16 h[16]; } *)&sRender)->h[13] << 5) + 0x80, 1, 0,
                         0, 0, 0x10, 0x10);
    load.giftag0[0] |= 0x8000;
    sceVif1PkOpenDirectCode(s_pPacket, 0);
    sceVif1PkAddDirectDataN(s_pPacket, &load, 5);
    sceVif1PkCloseDirectCode(s_pPacket);
    sceVif1PkRef(s_pPacket, nmlPacketSetAttributeData16N(clut, 0x42), 0x42, 0, 0, 0);
}

typedef struct NmlTextureImage {
    int width;
    int height;
    int psm;
    int bw;
    int tbp;
    int size;
    u8 pad[0x28];
    u8 data[0x10];
} NmlTextureImage;
typedef struct NmlTextureBlock {
    int count;
    u8 pad[0x2c];
    int images[1];
} NmlTextureBlock;
static void nmlPacketTextureTrans(NmlModelRenderState *model)
{
    NmlGsLoadImage load;
    u8 *texture;
    int offset;
    NmlTextureBlock *block;
    NmlTextureImage *image;
    int *images;
    int i;
    int dbp;

    s_pPacket = xglPacketGetCurrent();
    texture = *(u8 **)((u8 *)model + 0x254);
    offset = *(int *)(texture + 0x6C);
    if (offset == 0)
        return;
    block = (NmlTextureBlock *)(offset + (int)texture);
    i = 0;
    images = block->images;
    if (block->count <= 0)
        return;
    do {
        image = (NmlTextureImage *)(images[i] + (int)block);
        sceVif1PkCnt(s_pPacket, 0);
        sceVif1PkAddCode(s_pPacket, VIF_CODE_FLUSH);
        dbp = (((union { u16 h[16]; } *)&sRender)->h[13] << 5) + image->tbp;
        sceGsSetDefLoadImage(&load, dbp, image->bw,
                             image->psm, 0, 0, image->width, image->height);
        load.giftag0[0] |= 0x8000;
        sceVif1PkOpenDirectCode(s_pPacket, 0);
        sceVif1PkAddDirectDataN(s_pPacket, &load, 5);
        sceVif1PkCloseDirectCode(s_pPacket);
        sceVif1PkRef(s_pPacket, image->data, image->size / 16, 0, 0, 0);
        i++;
    } while (i < block->count);
}

/* The cached texture built by the (still asm) texture cache builder;
 * cleared to invalidate it, like s_pMatrixCache/s_pModelCache/
 * s_pModelLayout/s_pLightLayout below. */
extern void *s_pCacheTexture;

void nmlPacketClrTextureCache(void)
{
    s_pCacheTexture = 0;
}

/* The cached VU1 upload state a model was last sent with: matrix data,
 * model geometry and the light/model attribute layouts. Cleared together
 * to force the next nmlPacket* draw call to resend all four. */
extern void *s_pMatrixCache;
extern void *s_pModelCache;
extern void *s_pModelLayout;
extern void *s_pLightLayout;

void nmlPacketClrModelCache(void)
{
    s_pMatrixCache = 0;
    s_pModelCache = 0;
    s_pModelLayout = 0;
    s_pLightLayout = 0;
}

extern void xglPacketTextureTrans(void *texture);
static inline int packet_is_full(XglPacket *packet)
{
    int n = sceVif1PkSize(packet) * 0x10;

    return (unsigned)(n + (*(int *)&s_pPacket->limit - *(int *)&s_pPacket->cursor) + 0x10000)
        > 0x200000U;
}

int nmlPacketAddTexture(NmlModelRenderState *model)
{
    NmlProRealParam *proreal;
    void *texture;
    u32 offset;

    proreal = model->proreal;
    if (model->proreal_override != 0)
        proreal = model->proreal_override;
    s_pPacket = xglPacketGetCurrent();
    if (packet_is_full(s_pPacket))
        return 1;
    if (model->render_status & 0x800) {
        texture = *(void **)((u8 *)proreal + 0x1C0);
        if (texture != 0 && s_pCacheTexture != texture) {
            s_pCacheTexture = texture;
            xglPacketTextureTrans(texture);
        }
    } else {
        offset = *(u32 *)(*(u8 **)((u8 *)model + 0x254) + 0x6C);
        if (offset - 1 < 0x7FFFFF) {
            if (s_pCacheTexture != (void *)offset) {
                nmlPacketTextureTrans(model);
                s_pCacheTexture = *(void **)(*(u8 **)((u8 *)model + 0x254) + 0x6C);
            }
        } else {
            texture = *(void **)((u8 *)model + 0x240);
            if (texture != 0 && s_pCacheTexture != texture) {
                s_pCacheTexture = texture;
                xglPacketTextureTrans(*(void **)((u8 *)model + 0x240));
            }
        }
    }
    return packet_is_full(s_pPacket);
}

extern int memcmp(const void *left, const void *right, unsigned int count);
extern void xglMatrixInverse(void *destination, void *source);
extern void xglMatrixMul(void *destination, void *left, void *right);
typedef union NmlPartsMatrix {
    Quadword rows[4];
    float m[4][4];
} NmlPartsMatrix;
static inline void parts_copy_matrix(void *dst, const void *src)
{
    __asm__ __volatile__("lq $2,0x0(%1)\n\tsq $2,0x0(%0)\n\t"
                         "lq $2,0x10(%1)\n\tsq $2,0x10(%0)\n\t"
                         "lq $2,0x20(%1)\n\tsq $2,0x20(%0)\n\t"
                         "lq $2,0x30(%1)\n\tsq $2,0x30(%0)"
                         : : "r"(dst), "r"(src) : "$2", "memory");
}

int nmlPacketAddParts(u8 *parts, NmlModelRenderState *model, StudioCamera *camera)
{
    NmlPartsMatrix local;
    NmlPartsMatrix light;
    Vector4 trans;
    float dirs[4][4];
    NmlPartsMatrix world;
    NmlPartsMatrix proj;
    NmlPartsMatrix screen;
    u16 *ids;
    u8 *proreal;
    u8 *data;
    void *cache;
    int changed;
    u32 i;
    int code;
    int j;
    u16 *upk_ids;
    u8 *upk_base;
    NmlPartsMatrix *src;
    u8 *mats;

    ids = (u16 *)(parts + 0x30);
    proreal = (u8 *)model->proreal;
    data = *(u8 **)((u8 *)model + 0x254);
    if (model->proreal_override != 0)
        proreal = (u8 *)model->proreal_override;
    s_pPacket = xglPacketGetCurrent();
    sceVif1PkCnt(s_pPacket, 0);
    if (packet_is_full(s_pPacket))
        return 1;
    if (*(u32 *)(parts + 0x20) & 0x10) {
        int *tab = (int *)(data + *(int *)(data + 0x60));
        i = 0;
        upk_base = data + tab[*(int *)(parts + 0xA0)] + 0x30;
        upk_ids = (u16 *)(parts + *(int *)(parts + 0xA4));
        if (*(u32 *)(parts + 0xA8) != 0) {
            do {
                sceVif1PkOpenUpkCode(s_pPacket, (u16)(*(u16 *)(parts + 0xAC) + i * 2), 0x6C, 1, 1);
                sceVif1PkAddUpkData128N(s_pPacket, upk_base + upk_ids[i] * 32, 2);
                i++;
                sceVif1PkCloseUpkCode(s_pPacket);
            } while (i < *(u32 *)(parts + 0xA8));
        }
    }
    if (s_pModelLayout != model) {
        s_pModelLayout = model;
        s_pMatrixCache = 0;
    }
    if (s_nModelParts != *(int *)(parts + 0x2C)) {
        s_nModelParts = *(int *)(parts + 0x2C);
        s_pMatrixCache = 0;
    }
    cache = s_pMatrixCache;
    if (cache != 0) {
        j = memcmp(cache, ids, 0x3E);
        cache = s_pMatrixCache;
        if (j != 0)
            cache = 0;
    }
    s_pMatrixCache = ids;
    changed = cache == 0;
    mats = *(u8 **)((u8 *)model + 0x258);
    if (*ids != 0) {
        code = 0x2DC;
        do {
            { int off = (*ids++ << 6) - 64; mats = mats + off; }
            sceVif1PkOpenUpkCode(s_pPacket, (u16)code, 0x6C, 1, 1);
            sceVif1PkAddUpkData128N(s_pPacket, mats, 4);
            code += 4;
            sceVif1PkCloseUpkCode(s_pPacket);
            mats = *(u8 **)((u8 *)model + 0x258);
        } while (*ids != 0);
    }
    if (changed) {
        src = (NmlPartsMatrix *)(mats + ((*(u16 *)s_pMatrixCache << 6) - 64));
        parts_copy_matrix(&local, src);
        local.m[2][3] = local.m[1][3] = local.m[0][3] = 0.0f;
        if (!(*(u32 *)(parts + 0x20) & 1) && !(*(u32 *)(parts + 0xC0) & 0x10))
            xglMatrixInverse(&world, &local);
        else
            parts_copy_matrix(&world, &src[*(int *)(data + 0x40)]);
        _CurMatrixSet(world.m);
        _CurMatrixMul((const float (*)[4])((u8 *)model + 0xC0));
        if (model->render_status & 0x800)
            _CurMatrixMul33norm((Vector4 *)&light, (Vector4 *)(proreal + 0x80));
        else
            _CurMatrixMul33norm((Vector4 *)&light, (Vector4 *)((u8 *)model + 0x40));
        _CurApplyMatrix(&trans, &camera->position);
        if (model->render_status & 0x2001000) {
            _CurApplyMatrix((Vector4 *)dirs[3], (Vector4 *)((u8 *)model + 0x100));
            dirs[0][0] = dirs[3][0];
            dirs[1][0] = dirs[3][1];
            dirs[2][0] = dirs[3][2];
            _CurApplyMatrix((Vector4 *)dirs[3], (Vector4 *)((u8 *)model + 0x110));
            dirs[0][1] = dirs[3][0];
            dirs[1][1] = dirs[3][1];
            dirs[2][1] = dirs[3][2];
            _CurApplyMatrix((Vector4 *)dirs[3], (Vector4 *)((u8 *)model + 0x120));
            dirs[0][2] = dirs[3][0];
            dirs[1][2] = dirs[3][1];
            dirs[2][2] = dirs[3][2];
            sceVif1PkOpenUpkCode(s_pPacket, 0x3F5, 0x6C, 1, 1);
            sceVif1PkAddUpkData128N(s_pPacket, dirs, 4);
            sceVif1PkCloseUpkCode(s_pPacket);
            sceVif1PkOpenUpkCode(s_pPacket, 0x3E9, 0x6C, 1, 1);
            sceVif1PkAddUpkData128N(s_pPacket, (u8 *)model + 0x140, 3);
            sceVif1PkCloseUpkCode(s_pPacket);
        }
        if (model->render_status & 0x800) {
            xglMatrixMul(&proj, proreal + 0x40, (u8 *)model + 0x80);
            sceVif1PkOpenUpkCode(s_pPacket, 0x3EC, 0x6C, 1, 1);
            sceVif1PkAddUpkData128N(s_pPacket, &proj, 4);
            sceVif1PkCloseUpkCode(s_pPacket);
            ((u64 *)&trans)[0] = ((u64 *)(proreal + 0x190))[0];
            ((u64 *)&trans)[1] = ((u64 *)(proreal + 0x190))[1];
        }
        light.m[3][3] = 0.0f;
        for (j = 0; j < 3; j++) {
            float *tp = (float *)&trans;
            light.m[j][3] = tp[j];
            light.m[3][j] = tp[j];
        }
        if (model->render_status & 0x800)
            __asm__ __volatile__("lq $2,0x0(%1)\n\tsq $2,0x0(%0)" : : "r"(light.rows + 3), "r"(proreal + 0x160) : "$2", "memory");
        sceVif1PkOpenUpkCode(s_pPacket, 0x3CD, 0x6C, 1, 1);
        sceVif1PkAddUpkData128N(s_pPacket, &light, 4);
        sceVif1PkCloseUpkCode(s_pPacket);
    }
    if (changed) {
        if (model->render_status & 0x8000) {
            _CurMatrixSet((const float (*)[4])((u8 *)model + 0x180));
        } else {
            _CurMatrixSet((const float (*)[4])camera->viewMatrix);
            if (model->render_status & 0x80000)
                _CurMatrixMul((const float (*)[4])((u8 *)model + 0x180));
        }
        if (model->render_status & 1) {
            _CurMatrixMul((const float (*)[4])((u8 *)model + 0x80));
            _CurMatrixMul(local.m);
        } else {
            _CurMatrixMul(local.m);
        }
        _CurMatrixGet(screen.m);
        sceVif1PkOpenUpkCode(s_pPacket, 0x3C2, 0x6C, 1, 1);
        sceVif1PkAddUpkData128N(s_pPacket, &screen, 4);
        sceVif1PkCloseUpkCode(s_pPacket);
    }
    return packet_is_full(s_pPacket);
}

void nmlPacketAddTextureParam(NmlMaterialRenderState *material,
                              NmlModelRenderState *model)
{
    u64 tex0;
    NmlProRealParam *proreal;
    u32 tbw;
    u32 lo;
    u32 hi;

    proreal = model->proreal;
    if (model->proreal_override != 0)
        proreal = model->proreal_override;

    /*
     * Stored as a word: the render_status load below must stay after this
     * store, which only an int-typed (aliasing) store keeps it.
     */
    *(u32 *)&s_pPacket = (u32)xglPacketGetCurrent();

    if (model->render_status & 0x800) {
        tex0 = 0x60;
        packet_gs_entry32(0x3F, 0, 0);
        packet_gs_entry64(6, (u64 *)((u8 *)proreal + 0x130));
        packet_gs_entry64(0x14, &tex0);
        packet_gs_entry64(8, (u64 *)((u8 *)proreal + 0x138));
    } else if (material->material_flags & 1) {
        tbw = ((u16 *)&sRender)[13];
        lo = ((u32 *)material)[0x48] + (tbw << 5);
        hi = ((u32 *)material)[0x49] + (tbw << 10);
        packet_gs_entry32(0x3F, 0, 0);
        packet_gs_entry32(6, hi, lo);
        packet_gs_entry32(0x14, ((u32 *)material)[0x4B], ((u32 *)material)[0x4A]);
        packet_gs_entry32(8, ((u32 *)material)[0x4D], ((u32 *)material)[0x4C]);
    }
}

static int add_exec_prog(NmlMaterialRenderState *material, NmlModelRenderState *model,
                         int program_index)
{
    int eye_material;
    NmlProRealParam *proreal;
    int result = 1;
    u32 *type1_programs;
    u32 *type2_programs;
    u32 *type3_programs;
    u32 *type4_programs;
    u32 *type5_programs;
    u32 *type6_programs;
    u32 *type7_programs;
    u32 program;

    type1_programs = s_aUcodeTbl[program_index].programs[0];
    type2_programs = s_aUcodeTbl[program_index].programs[1];
    type3_programs = s_aUcodeTbl[program_index].programs[2];
    type4_programs = s_aUcodeTbl[program_index].programs[3];
    type5_programs = s_aUcodeTbl[program_index].programs[4];
    type6_programs = s_aUcodeTbl[program_index].programs[5];
    type7_programs = s_aUcodeTbl[program_index].programs[6];

    eye_material = 0;
    proreal = model->proreal;
    if (model->proreal_override != 0)
        proreal = model->proreal_override;

    if ((model->render_status & 0x100000) != 0 &&
        (strstr(material->name, lenz_name) != 0 ||
         strstr(material->name, eye_name) != 0))
        eye_material = 1;

    s_pPacket = xglPacketGetCurrent();
    sceVif1PkCnt(s_pPacket, 0);

    switch (s_nProgType) {
    case 7: {
        int slot = 0;

        if (material->material_flags & 0x10)
            slot = 1;
        else if (material->render_flags & 1) {
            slot = 2;
            if (material->render_flags & 2)
                slot = 3;
            else if (material->render_flags & 4)
                slot = 4;
        }
        program = type7_programs[slot];
        if (program != 0)
            sceVif1PkAddCode(s_pPacket, (program >> 3) | VIF_CODE_MSCAL);
        else
            result = 0;
        break;
    }

    case 6: {
        int slot;

        if (material->render_flags & 1) {
            slot = 2;
            if (material->render_flags & 2)
                slot = 3;
            else if (material->render_flags & 4)
                slot = 4;
        }
        else if (material->material_flags & 0x10)
            slot = 1;
        else
            slot = 0;
        program = type6_programs[slot];
        if (program != 0)
            sceVif1PkAddCode(s_pPacket, (program >> 3) | VIF_CODE_MSCAL);
        else
            result = 0;
        break;
    }

    case 5: {
        int slot = 0;

        if ((material->material_flags & 0x10) == 0) {
            slot = 1;
            if ((material->material_flags & 0x1000) &&
                (!(model->render_level & 0x10) || eye_material))
                slot = 4;
            else if ((material->material_flags & 8) &&
                     (!(model->render_level & 2) || eye_material)) {
                if ((material->material_flags & 0x800) ||
                    ((model->render_level & 1) && !eye_material))
                    slot = 2;
                else
                    slot = 3;
            }
        }
        program = type5_programs[slot];
        if (program != 0)
            sceVif1PkAddCode(s_pPacket, (program >> 3) | VIF_CODE_MSCAL);
        else
            result = 0;
        break;
    }

    case 4: {
        int slot;

        if (material->render_flags & 2)
            slot = 4;
        else if (material->render_flags & 4)
            slot = 8;
        else
            slot = 0;
        if ((material->material_flags & 0x1000) &&
            (!(model->render_level & 0x10) || eye_material))
            slot += 3;
        else if ((material->material_flags & 8) &&
                 (!(model->render_level & 2) || eye_material)) {
            if ((material->material_flags & 0x800) ||
                ((model->render_level & 1) && !eye_material))
                slot += 1;
            else
                slot += 2;
        }
        program = type4_programs[slot];
        if (program != 0)
            sceVif1PkAddCode(s_pPacket, (program >> 3) | VIF_CODE_MSCAL);
        else
            result = 0;
        break;
    }

    case 3: {
        int slot;

        if (material->material_flags & 0x10) {
            slot = 3;
            if (proreal->projection_entry)
                slot = 4;
        }
        else if (material->render_flags & 1) {
            if (material->render_flags & 2)
                slot = 1;
            else if (material->render_flags & 4)
                slot = 2;
            else
                slot = 0;
        }
        else
            slot = 5;
        program = type3_programs[slot];
        if (program != 0)
            sceVif1PkAddCode(s_pPacket, (program >> 3) | VIF_CODE_MSCAL);
        else
            result = 0;
        break;
    }

    case 1: {
        int slot;

        if (material->render_flags & 2)
            slot = 4;
        else if (material->render_flags & 4)
            slot = 8;
        else
            slot = 0;
        if ((material->material_flags & 0x1000) &&
            (!(model->render_level & 0x10) || eye_material))
            slot += 3;
        else if ((material->material_flags & 8) &&
                 (!(model->render_level & 2) || eye_material)) {
            if ((material->material_flags & 0x800) ||
                ((model->render_level & 1) && !eye_material))
                slot += 1;
            else
                slot += 2;
        }
        program = type1_programs[slot];
        if (program != 0)
            sceVif1PkAddCode(s_pPacket, (program >> 3) | VIF_CODE_MSCAL);
        else
            result = 0;
        break;
    }

    case 2: {
        int slot;

        if ((material->material_flags & 0x10) == 0) {
            slot = 2;
            if ((material->material_flags & 0x1000) &&
                (!(model->render_level & 0x10) || eye_material))
                slot = 5;
            else if ((material->material_flags & 8) &&
                     (!(model->render_level & 2) || eye_material)) {
                if ((material->material_flags & 0x800) ||
                    ((model->render_level & 1) && !eye_material))
                    slot = 3;
                else
                    slot = 4;
            }
        }
        else if (material->render_flags & 0x10)
            slot = 1;
        else
            slot = 0;
        program = type2_programs[slot];
        if (program != 0)
            sceVif1PkAddCode(s_pPacket, (program >> 3) | VIF_CODE_MSCAL);
        else
            result = 0;
        break;
    }

    default:
        break;
    }

    return result;
}

void nmlPacketAddExecProg(NmlMaterialRenderState *material,
                          NmlModelRenderState *model,
                          int allow_extended_programs,
                          int add_transform_data)
{
    if (add_transform_data) {
        if (!add_exec_prog(material, model, 0))
            return;
        nmlPacketAddTransData(material);
        return;
    }

    if ((material->render_flags & 0x80)
        || (model->render_status & 0x100)) {
        if (!add_exec_prog(material, model, 1))
            return;
        nmlPacketAddTransData(material);
        return;
    }

    if ((material->render_flags & 0x100)
        || (model->render_status & 0x200)) {
        if (!add_exec_prog(material, model, 2))
            return;
        nmlPacketAddTransData(material);
        return;
    }

    if (material->render_flags & 0x200) {
        if (add_exec_prog(material, model, 2))
            nmlPacketAddTransData(material);
        nmlPacketAddWaitMicrocode();
        if (!add_exec_prog(material, model, 1))
            return;
        nmlPacketAddTransData(material);
        return;
    }

    if (allow_extended_programs
        && !(material->render_flags & 0x400)
        && !(model->render_status & 0x8)) {
        if (model->render_level & 0x4) {
            if (!add_exec_prog(material, model, 3))
                return;
            nmlPacketAddTransData(material);
            return;
        }

        if (add_exec_prog(material, model, 5))
            nmlPacketAddTransData(material);
        nmlPacketAddWaitMicrocode();
        if (!add_exec_prog(material, model, 4))
            return;
        nmlPacketAddTransData(material);
        return;
    }

    if (!add_exec_prog(material, model, 3))
        return;
    nmlPacketAddTransData(material);
}

void nmlPacketAddTransData(NmlMaterialRenderState *material)
{
    s_pPacket = xglPacketGetCurrent();
    sceVif1PkRef(s_pPacket, (void *)(*(int *)((u8 *)material + 0x24) + (int)material),
                 *(int *)((u8 *)material + 0x28) / 16, 0, 0, 0);
    nmlPacketAddWaitMicrocode();
}

extern void *memcpy(void *destination, const void *source, unsigned int count);

/*
 * Both allocators here (and nmlPacketSetAttributeData16N/64/64N, still
 * INCLUDE_ASM in this TU) share one pattern: fetch the current packet,
 * decrement its cursor (main/xgl_packet.h, "+0x24: the write cursor") by the
 * requested size, and hand back the new cursor as the reserved storage's
 * address. A plain `packet->cursor` read after the subtraction lets gcc 2.96
 * -O2 prove the earlier write cannot alias it and cache the value across the
 * memcpy call, dropping the reload the original object performs; reading
 * through a cast on the field's own address defeats that alias proof.
 */
u8 *nmlPacketSetAttributeData(const void *data, u32 size)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    s_pPacket = packet;
    *(u8 **)&packet->cursor -= size;
    memcpy(*(u8 **)&s_pPacket->cursor, data, size);
    return *(u8 **)&s_pPacket->cursor;
}

u8 *nmlPacketSetAttributeData64(const void *data)
{
    XglPacket *packet;
    u8 *dst;

    packet = xglPacketGetCurrent();
    s_pPacket = packet;
    *(u8 **)&packet->cursor -= 0x40;
    dst = *(u8 **)&s_pPacket->cursor;
    asm volatile("lq $2,0x0(%1)\n\tsq $2,0x0(%0)\n\tlq $2,0x10(%1)\n\tsq $2,0x10(%0)\n\tlq $2,0x20(%1)\n\tsq $2,0x20(%0)\n\tlq $2,0x30(%1)\n\tsq $2,0x30(%0)"
                 : : "r"(dst), "r"(data) : "$2", "memory");
    return *(u8 **)&s_pPacket->cursor;
}

/* Handwritten function (original is assembly): copies count 64-byte blocks. */
u8 *nmlPacketSetAttributeData64N(const void *data, u32 count)
{
    XglPacket *packet;
    u8 *dst;

    packet = xglPacketGetCurrent();
    s_pPacket = packet;
    *(u8 **)&packet->cursor -= count << 6;
    dst = *(u8 **)&s_pPacket->cursor;
    __asm__ __volatile__(
        "1:\n\t"
        "lq $2,0(%1)\n\t"
        "sq $2,0(%0)\n\t"
        "lq $2,16(%1)\n\t"
        "sq $2,16(%0)\n\t"
        "lq $2,32(%1)\n\t"
        "sq $2,32(%0)\n\t"
        "lq $2,48(%1)\n\t"
        "sq $2,48(%0)\n\t"
        "addi %2,%2,-1\n\t"
        "addi %0,%0,64\n\t"
        "addi %1,%1,64\n\t"
        ".set push\n\t"
        ".set noreorder\n\t"
        "bne $0,%2,1b\n\t"
        "nop\n\t"
        ".set pop"
        : "+r"(dst), "+r"(data), "+r"(count)
        :
        : "$2", "memory");
    return *(u8 **)&s_pPacket->cursor;
}

/* Handwritten function (original is assembly): copies count quadwords. */
u8 *nmlPacketSetAttributeData16N(const void *data, u32 count)
{
    XglPacket *packet;
    u8 *dst;

    packet = xglPacketGetCurrent();
    s_pPacket = packet;
    *(u8 **)&packet->cursor -= count << 4;
    dst = *(u8 **)&s_pPacket->cursor;
    __asm__ __volatile__(
        "1:\n\t"
        "lq $2,0(%1)\n\t"
        "sq $2,0(%0)\n\t"
        "addi %2,%2,-1\n\t"
        "addi %0,%0,16\n\t"
        "addi %1,%1,16\n\t"
        ".set push\n\t"
        ".set noreorder\n\t"
        "bne $0,%2,1b\n\t"
        "nop\n\t"
        ".set pop"
        : "+r"(dst), "+r"(data), "+r"(count)
        :
        : "$2", "memory");
    return *(u8 **)&s_pPacket->cursor;
}

/* Same aliasing proof as nmlPacketSetAttributeData above (main 0x00238e00). */
u8 *nmlPacketSetAttributeAlloc16N(u32 count)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    s_pPacket = packet;
    *(u8 **)&packet->cursor -= count * 0x10;
    return *(u8 **)&s_pPacket->cursor;
}

void nmlPacketAddGifTag(NmlMaterialRenderState *material, NmlModelRenderState *model,
                        int alpha)
{
    NmlPacketData128 tag;
    u32 fog;

    fog = (model->render_status & 0x1000002) != 0;
    if (model->render_status & 0x2000)
        fog = 0;
    s_pPacket = xglPacketGetCurrent();
    sceVif1PkCnt(s_pPacket, 0);
    tag.quad = *(Quadword *)((u8 *)material + 0xB0);
    tag.words[1] |= (alpha << 21) | (fog << 20);
    if (material->material_flags & 1)
        tag.words[1] |= 0x80000;
    else
        tag.words[1] &= ~0x80000;
    if (model->render_status & 0x8000)
        tag.words[1] &= ~((alpha << 21) | 0x80000);
    if (model->render_status & 0x800)
        tag.words[1] |= 0x80000;
    sceVif1PkOpenUpkCode(s_pPacket, 0x3F3, 0x6C, 1, 1);
    sceVif1PkAddUpkData128(s_pPacket, tag.quad);
    sceVif1PkCloseUpkCode(s_pPacket);
    tag.words[1] = (tag.words[1] & ~0x80000) | 0x1200000;
    sceVif1PkOpenUpkCode(s_pPacket, 0x3F4, 0x6C, 1, 1);
    sceVif1PkAddUpkData128(s_pPacket, tag.quad);
    sceVif1PkCloseUpkCode(s_pPacket);
    tag.words[1] = (tag.words[1] | 0x80000) & ~0x1000000;
    sceVif1PkOpenUpkCode(s_pPacket, 0x3F2, 0x6C, 1, 1);
    sceVif1PkAddUpkData128(s_pPacket, tag.quad);
    sceVif1PkCloseUpkCode(s_pPacket);
}

void nmlPacketAddGifTagStandard(int eop, int count)
{
    NmlPacketData128 gif_tag;

    s_pPacket = xglPacketGetCurrent();
    sceVif1PkCnt(s_pPacket, 0);

    gif_tag.words[0] = 0x8000;
    gif_tag.words[1] = ((u32)eop << 0x13) | ((u32)count << 0x15) | 0x30064000;
    gif_tag.words[2] = 0x412;
    gif_tag.words[3] = 0;
    sceVif1PkOpenUpkCode(s_pPacket, 0x3f3, 0x6c, 1, 1);
    sceVif1PkAddUpkData128(s_pPacket, gif_tag.quad);
    sceVif1PkCloseUpkCode(s_pPacket);
}

void nmlPacketAddFog(NmlModelRenderState *model, int viewport_index)
{
    s_pPacket = xglPacketGetCurrent();
    sceVif1PkCnt(s_pPacket, 0);

    if (g_aSubWindow[viewport_index] != 0)
        model->fog_color[3] = ((int)(model->fog_intensity[viewport_index] * 255.0f)) << 4;

    sceVif1PkOpenUpkCode(s_pPacket, 998, 108, 1, 1);
    sceVif1PkAddUpkData128N(s_pPacket, model->fog_color, 1);
    sceVif1PkCloseUpkCode(s_pPacket);
    sceVif1PkOpenUpkCode(s_pPacket, 1007, 108, 1, 1);
    sceVif1PkAddUpkData128N(s_pPacket, model->fog_parameters, 1);
    sceVif1PkCloseUpkCode(s_pPacket);
}

static inline u32 nmlPacketInvalidatePixelModel(
    NmlModelResource *const *resource, const u32 *material_flags)
{
    s_nPixelTestType = -1;
    s_pModelCache = *resource;
    return *material_flags;
}
static inline u64 nmlPacketMakeGsTest(int alpha_test_enabled, int alpha_test,
                                      int alpha_reference, int alpha_failure,
                                      int destination_alpha_enabled,
                                      int destination_alpha, int depth_enabled,
                                      int depth_test)
{
    u64 test = (u64)alpha_reference << 4;

    test |= (u64)depth_test << 17;
    test |= (u64)depth_enabled << 16;
    test |= (u64)alpha_failure << 12;
    test |= (u64)destination_alpha_enabled << 14;
    test |= (u64)destination_alpha << 15;
    test |= (u64)(alpha_test_enabled | (alpha_test << 1));
    return test;
}
static inline u64 nmlPacketMakeGsFixedAlpha(int alpha_value)
{
    return ((u64)alpha_value << 32) | 0x68;
}
static inline void nmlPacketSetPixelBlend(NmlModelRenderState *model,
                                          int pixel_type, float alpha,
                                          float reflect_alpha,
                                          int alpha_reference,
                                          int use_alpha_reference)
{
    if (s_nPixelTestType != pixel_type) {
        int alpha_value =
            (int)(model->material_alpha_scale * 128.0f * alpha);
        u64 test_value = 0x50000;
        u64 blend_value;

        if (reflect_alpha < 2.0f)
            alpha_value = (int)(reflect_alpha * 128.0f * alpha);
        if (use_alpha_reference)
            test_value = nmlPacketMakeGsTest(1, 3, alpha_reference,
                                             0, 0, 0, 1, 2);
        blend_value = nmlPacketMakeGsFixedAlpha(alpha_value);
        s_nPixelTestType = pixel_type;
        packet_gs_entry32(0x47, 0, (u32)test_value);
        packet_gs_entry64(0x43, &blend_value);
        nmlPacketAddPixelTestPacket(0x47, test_value, 0x50000,
                                    0x42, *(u64 *)&model->pixel_test_parameters,
                                    blend_value);
    }
}

void nmlPacketAddPixelControl(NmlMaterialRenderState *material,
                              NmlModelRenderState *model,
                              u32 pixel_control_flags)
{
    NmlCurrentPacket selected;
    u16 viewport = pixel_control_flags;
    int pixel_reference = model->pixel_test_reference;
    float reflect_alpha = model->reflect_alpha;
    float alpha;
    int special_material = 0;
    u32 material_flags;
    u64 packed_alpha;
    int i;

    alpha = model->default_block_alpha;
    if (viewport != model->block_alpha_view)
        alpha = model->block_alpha_by_view[viewport];
    if (model->render_status & 0x100000) {
        if (strstr(material->name, lenz_name) != 0 ||
            strstr(material->name, eye_name) != 0) {
            alpha = model->special_block_alpha;
            reflect_alpha = 2.0f;
            special_material = 1;
            s_nPixelTestType = -1;
        }
    }

    for (i = 0; i < *(int *)&model->pixel_test_count; i++) {
        s_nPixelTestType = -1;
        if (((short *)((u8 *)model + i * 4))[361] ==
            *(u32 *)&material->geometry_block_index) {
            pixel_reference = model->pixel_test_entries[i].reference;
            break;
        }
    }

    selected.sdk = xglPacketGetCurrent();
    *(NmlCurrentPacket *)&s_pPacket = selected;

    material_flags = ((union { u32 w; u64 d; } *)&material->material_flags)->w;
    if (material_flags & 0x10)
        pixel_reference = 0x7F;
    if (s_pModelCache != model->resource) {
        material_flags = nmlPacketInvalidatePixelModel(
            &model->resource, &material->material_flags);
    }

    if (pixel_control_flags & 0x10000) {
        if (material_flags & 4) {
            if (pixel_control_flags & 0x20000) {
                if (s_nPixelTestType != 1) {
                    u64 blend_mode = 0x68;

                    s_nPixelTestType = 1;
                    if (*(u32 *)&material->material_flags & 0x8000) {
                        packet_gs_entry32(0x47, 0, 0x50000);
                    } else {
                        packet_gs_entry32(0x47, 0,
                            (u32)nmlPacketMakeGsTest(1, 5, pixel_reference,
                                                    0, 0, 0, 1, 2));
                    }
                    packet_gs_entry64(0x42, &blend_mode);
                }
            } else if (model->render_status & 0x20) {
                nmlPacketSetPixelBlend(model, 2, alpha, reflect_alpha,
                                       pixel_reference, 0);
            } else {
                nmlPacketSetPixelBlend(model, 3, alpha, reflect_alpha,
                                       pixel_reference, 1);
            }
        } else if (pixel_control_flags & 0x20000) {
            u64 blend_mode;

            if (s_nPixelTestType != 4) {
                s_nPixelTestType = 4;
                packet_gs_entry32(0x47, 0, 0x50000);
                blend_mode = 0x68;
                packet_gs_entry64(0x42, &blend_mode);
                nmlPacketAddPixelTestPacket(0x47, 0x50000, 0x50000,
                                            0x42, *(u64 *)&model->pixel_test_parameters,
                                            blend_mode);
            }
        } else {
            nmlPacketSetPixelBlend(model, 5, alpha, reflect_alpha,
                                   pixel_reference, 0);
        }
    } else if (material_flags & 4) {
        if (s_nPixelTestType != 6) {
            u64 test_value;
            u32 depth_base;

            pixel_reference =
                (int)(model->material_alpha_scale * (float)pixel_reference);
            test_value = nmlPacketMakeGsTest(1, 5, pixel_reference,
                                             0, 0, 0, 1, 2);
            depth_base = sRender.zbp;

            packet_gs_entry32(0x47, 0, (u32)test_value);
            s_nPixelTestType = 6;
            nmlPacketAddPixelTestPacket(0x47, test_value, 0x50001, 0x4E,
                                        depth_base | 0x01000000,
                                        depth_base | (0x8080ULL << 0x11));
        }
    } else if (s_nPixelTestType != 7) {
        u32 depth_base = sRender.zbp;

        packed_alpha = nmlPacketMakeGsFixedAlpha((int)(alpha * 128.0f));
        s_nPixelTestType = 7;
        packet_gs_entry32(0x47, 0, 0x50000);
        packet_gs_entry64(0x43, &packed_alpha);
        nmlPacketAddPixelTestPacket(0x47, 0x50000, 0x50000, 0x4E,
                                    depth_base | 0x01000000,
                                    depth_base | (0x8080ULL << 0x11));
    }

    if (special_material)
        s_nPixelTestType = -1;
}

int nmlPacketDirectData(void *arg0, int arg1) {
    XglPacket *p;
    int n, c, l;
    p = xglPacketGetCurrent();
    s_pPacket = p;
    n = sceVif1PkSize(p) * 0x10;
    c = *(int *)&s_pPacket->cursor;
    l = *(int *)&s_pPacket->limit;
    if ((unsigned)(n + (l - c) + 0x10000) > 0x200000U) {
        return 1;
    }
    sceVif1PkRef(s_pPacket, arg0, arg1, 0, 0, 0);
    return 0;
}

void nmlPacketSetCurrent(void)
{
    s_pPacket = xglPacketGetCurrent();
}

void nmlPacketAddReflRot(const NmlMaterialRenderState *material,
                         const NmlModelRenderState *model)
{
    float matrix[4][4];
    int type = 1;

    if ((material->material_flags & 0x2000u) != 0)
        type = 2;

    s_pPacket = xglPacketGetCurrent();

    if (type != 1) {
        if (type != 2)
            return;
        if (s_nReflRotType == type)
            return;
        xglMatrixStackUnit();
        xglMatrixStackRotY(s_inReflRotY);
        xglMatrixStackRotX(s_inReflRotX);
        xglMatrixStackSave(matrix);
    } else {
        if (s_nReflRotType == type)
            return;
        xglMatrixUnit(matrix);
    }

    sceVif1PkCnt(s_pPacket, 0);
    sceVif1PkOpenUpkCode(s_pPacket, 1004, 108, 1, 1);
    sceVif1PkAddUpkData128N(s_pPacket, matrix, 3);
    sceVif1PkCloseUpkCode(s_pPacket);
    s_nReflRotType = type;
}

void nmlPacketAddGsFlushWide(void);
typedef struct GsAdEntry {
    u64 data;
    u64 reg;
} GsAdEntry;
static inline void set_ad(GsTagWord *p, u64 data, u64 reg)
{
    p[0].d = data;
    p[1].d = reg;
}

void nmlPacketAddScreenClear(void)
{
    GsTagWord *ad0;
    GsTagWord *ad1;
    GsTagWord *ad2;
    GsTagWord *ad3;
    GsTagWord *ad4;
    GsTagWord *ad5;
    GsTagWord *ad6;
    GsTagWord *ad7;
    GsAdEntry *ad;
    s16 *r = (s16 *)&sRender;
    int i;
    u32 zbuf;

    zbuf = sRender.zbp | 0x1000000;
    ad0 = (GsTagWord *)0x70000000;
    set_ad(ad0, 0, 0x4A);
    ad1 = (GsTagWord *)0x70000010;
    set_ad(ad1, zbuf, 0x4E);
    ad2 = (GsTagWord *)0x70000020;
    set_ad(ad2, 0x30000, 0x47);
    ad3 = (GsTagWord *)0x70000030;
    set_ad(ad3, ((u16 *)r)[16] | ((u64)(r[2] / 64) << 16) | ((u64)r[1] << 24), 0x4C);
    ad4 = (GsTagWord *)0x70000040;
    set_ad(ad4, 0x3F80000000000000, 1);
    ad5 = (GsTagWord *)0x70000050;
    set_ad(ad5, 6, 0);
    ad6 = (GsTagWord *)0x70000060;
    set_ad(ad6, ((u64)(0x800 - r[2]) << 4) | ((u64)(0x800 - r[3]) << 20), 4);
    ad7 = (GsTagWord *)0x70000070;
    set_ad(ad7, ((u64)(r[2] + 0x800) << 4) | ((u64)(r[3] + 0x800) << 20), 4);

    g_nGsEntry = 0;
    ad = (GsAdEntry *)0x70000000;
    i = 7;
    do {
        packet_gs_entry64((u32)ad->reg, &ad->data);
        ad++;
    } while (--i >= 0);
    nmlPacketAddGsFlushWide();
}

extern int g_nGsEntry;

void nmlPacketGsInit(void)
{
    g_nGsEntry = 0;
}

void nmlPacketAddGsClamp(u64 clamp)
{
    g_aGsTag[g_nGsEntry * 2 + 2].d = clamp;
    g_aGsTag[g_nGsEntry * 2 + 3].d = 8;
    g_nGsEntry++;
}

void nmlPacketAddGsPixeltest(u32 test)
{
    packet_gs_entry32(0x47, 0, test);
}

void nmlPacketAddGsPixeltest1(u32 test)
{
    packet_gs_entry32(0x48, 0, test);
}

void nmlPacketAddGsZbuf(u64 mask)
{
    int idx = g_nGsEntry;

    g_aGsTag[idx * 2 + 2].d = sRender.zbp | (mask << 32) | 0x1000000;
    g_aGsTag[idx * 2 + 3].d = 0x4E;
    g_nGsEntry = idx + 1;
}

void nmlPacketAddGsZbuf1(u64 mask)
{
    int idx = g_nGsEntry;

    g_aGsTag[idx * 2 + 2].d = sRender.zbp | (mask << 32) | 0x1000000;
    g_aGsTag[idx * 2 + 3].d = 0x4F;
    g_nGsEntry = idx + 1;
}

void nmlPacketAddGsTexture(void)
{
    packet_gs_entry32(0x3F, 0, 0);
}

void nmlPacketAddGsAlpha(u64 alpha)
{
    g_aGsTag[g_nGsEntry * 2 + 2].d = alpha;
    g_aGsTag[g_nGsEntry * 2 + 3].d = 66;
    g_nGsEntry++;
}

void nmlPacketAddGsAlpha1(u64 alpha)
{
    g_aGsTag[g_nGsEntry * 2 + 2].d = alpha;
    g_aGsTag[g_nGsEntry * 2 + 3].d = 67;
    g_nGsEntry++;
}

void nmlPacketAddGsScissor(u64 scissor)
{
    g_aGsTag[g_nGsEntry * 2 + 2].d = scissor;
    g_aGsTag[g_nGsEntry * 2 + 3].d = 64;
    g_nGsEntry++;
}

void nmlPacketAddGsScissor1(u64 scissor)
{
    g_aGsTag[g_nGsEntry * 2 + 2].d = scissor;
    g_aGsTag[g_nGsEntry * 2 + 3].d = 65;
    g_nGsEntry++;
}

void nmlPacketAddGsFBA(int fba)
{
    packet_gs_entry32(0x4A, 0, fba != 0);
}

void nmlPacketAddGsFBA1(int fba)
{
    packet_gs_entry32(0x4B, 0, fba != 0);
}

void nmlPacketAddGsFogCol(float *color)
{
    packet_gs_entry32(0x3D, 0,
        (u64)(u32)color[0] | ((u64)(u32)color[1] << 8) | ((u64)(u32)color[2] << 16));
}

void nmlPacketAddGsFrame(u16 fbp, u64 mask)
{
    int idx = g_nGsEntry;
    s16 *r = (s16 *)&sRender;

    g_aGsTag[idx * 2 + 2].d = fbp | (mask << 32) | (((u64)r[1] << 24) | ((u64)(r[2] / 64) << 16));
    g_aGsTag[idx * 2 + 3].d = 0x4C;
    g_nGsEntry = idx + 1;
}

void nmlPacketAddGsFrame1(u16 fbp, u64 mask)
{
    int idx = g_nGsEntry;
    s16 *r = (s16 *)&sRender;

    g_aGsTag[idx * 2 + 2].d = fbp | (mask << 32) | (((u64)r[1] << 24) | ((u64)(r[2] / 64) << 16));
    g_aGsTag[idx * 2 + 3].d = 0x4D;
    g_nGsEntry = idx + 1;
}

void nmlPacketAddGsPAbe(u64 pabe)
{
    g_aGsTag[g_nGsEntry * 2 + 2].d = pabe;
    g_aGsTag[g_nGsEntry * 2 + 3].d = 73;
    g_nGsEntry++;
}

void nmlPacketAddGsPrmode(NmlMaterialRenderState *material)
{
    u64 prmode = 0x48;

    if (material->material_flags & 1)
        prmode = 0x58;
    packet_gs_entry64(0x1B, &prmode);
}

void nmlPacketAddGsPrmodecont(u64 prmode_control)
{
    g_aGsTag[g_nGsEntry * 2 + 2].d = prmode_control;
    g_aGsTag[g_nGsEntry * 2 + 3].d = 26;
    g_nGsEntry++;
}

#include "common.h"
#include "shared.h"
#include "nml_model_set.h"

/* Types recovered upstream (origin/master); kept for reference. */
/*
 * The renderer passes this 0x1e0-byte block to nmlPacketSetAttributeData16N
 * with count 0x1e, so the copied extent is established.  ProjectMap and
 * ProjectCircle access the listed float slots; nmlModelClear clears the word
 * at +0x1c0.  Other bytes remain unnamed because their roles are not shown
 * by this TU's C callers.
 */
typedef struct NmlProRealState {
    unsigned char unmodeled_000[0x190];
    float unmodeled_190[4];
    float value_1a0;
    float value_1a4;
    float value_1a8;
    float unmodeled_1ac;
    float unmodeled_1b0;
    float unmodeled_1b4;
    float value_1b8;
    float unmodeled_1bc;
    int projection_entry;
    unsigned char unmodeled_1c4[0x1e0 - 0x1c4];
} NmlProRealState;


extern int s_nClip;
extern int s_nMapShadowParts;
extern Vector4 s_inGblPos;

extern unsigned char s_inMapHandle[];

#define NML_RENDER_MAP_ENTRY 0x10000u

static float _VectorLengthSQ(const Vector4 *first, const Vector4 *second)
{
    float lengthSq;

    __asm__ __volatile__(
        "lqc2 $vf10,0(%1)\n\t"
        "lqc2 $vf11,0(%2)\n\t"
        "vsub.xyz vf10xyz,vf11xyz,vf10xyz\n\t"
        "vmul.xyz vf10xyz,vf10xyz,vf10xyz\n\t"
        "vaddz.x vf10x,vf10x,vf10z\n\t"
        "vaddy.x vf10x,vf10x,vf10y\n\t"
        "qmfc2 $2,$vf10\n\t"
        "mtc1 $2,$f2\n\t"
        "mtc1 $2,%0"
        : "=f"(lengthSq)
        : "r"(first), "r"(second)
        : "$2", "memory"
    );
    return lengthSq;
}

/*
 * Loads the current model matrix's four rows into the resident VU0
 * macro-mode registers vf27-vf30 (ee-vu-cop2), for the _CurRotTransPersClip/
 * _CurRotTransPersFog/_CurApplyMatrix family that reads them back.
 */
static void _CurSetMatrix(float matrix[4][4])
{
    __asm__ __volatile__(
        "lqc2 $vf27,0(%0)\n\t"
        "lqc2 $vf28,16(%0)\n\t"
        "lqc2 $vf29,32(%0)\n\t"
        "lqc2 $vf30,48(%0)\n\t"
        "nop"
        :
        : "r"(matrix)
        : "memory"
    );
}

/*
 * Loads the current view-scale and view-translation vectors into the
 * resident VU0 macro-mode registers vf25 and vf26 (ee-vu-cop2), for the
 * same _CurRotTransPersClip/_CurRotTransPersFog/_CurApplyMatrix family.
 */
static void _CurSetViewScaleTrans(const float viewScale[4], const float viewTrans[4])
{
    __asm__ __volatile__(
        "lqc2 $vf25,0(%0)\n\t"
        "lqc2 $vf26,0(%1)\n\t"
        "nop"
        :
        : "r"(viewScale), "r"(viewTrans)
        : "memory"
    );
}

static int _CurRotTransPersClip(Vector4 *destination, const Vector4 *vector) {
    int clip;
    __asm__ __volatile__(
        "ctc2.ni $0, $vi18\n\t"
        "lqc2 $vf31, 0(%2)\n\t"
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
        "vmulaw.xyzw ACC, $vf26, $vf0w\n\t"
        "vmadd.xyzw $vf31, $vf31, $vf25\n\t"
        "vftoi4.xyw $vf23, $vf31\n\t"
        "vftoi0.z $vf23, $vf31\n\t"
        "vsub.w $vf23, $vf23, $vf23\n\t"
        "sqc2 $vf23, 0(%1)\n\t"
        "cfc2.ni $2, $vi18\n\t"
        "nop\n\t"
        "andi $2, $2, 0x3f\n\t"
        "daddu %0, $2, $0"
        : "=r"(clip)
        : "r"(destination), "r"(vector)
        : "$2", "memory"
    );
    return clip;
}

/*
 * Transforms one vector by the resident VU0 matrix (_CurSetMatrix above),
 * perspective-divides it and folds the result into a fog coordinate clamped
 * to the supplied [min, max] bounds, the same resident-register family as
 * _CurApplyMatrix below.
 */
static void _CurRotTransPersFog(Vector4 *destination, const Vector4 *vector, const Vector4 *fog) {
    __asm__ __volatile__(
        "lqc2 vf31, 0(%1)\n\t"
        "lqc2 vf4, 0(%2)\n\t"
        "vmulax.xyzw ACC, vf27, vf31x\n\t"
        "vmadday.xyzw ACC, vf28, vf31y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf31z\n\t"
        "vmaddw.xyzw vf31, vf30, vf0w\n\t"
        "vdiv Q, vf0w, vf31w\n\t"
        "vmulz.w vf1, vf0, vf4z\n\t"
        "vwaitq\n\t"
        "vmula.w ACC, vf1, vf0\n\t"
        "vmaddq.w vf31, vf4, Q\n\t"
        "vmaxx.w vf31, vf31, vf4x\n\t"
        "vminiy.w vf31, vf31, vf4y\n\t"
        "sqc2 vf31, 0(%0)\n\t"
        "nop"
        :
        : "r"(destination), "r"(vector), "r"(fog)
        : "memory"
    );
}

/*
 * Multiplies one vector by the resident VU0 matrix (_CurSetMatrix above)
 * and stores the transformed result, the same row-by-row accumulation as
 * src/main/face_point.c's _ApplyMatrix.
 */
static void _CurApplyMatrix(Vector4 *destination, const Vector4 *vector) {
    __asm__ __volatile__(
        "lqc2 vf31, 0(%0)\n\t"
        "vmulax.xyzw ACC, vf27, vf31x\n\t"
        "vmadday.xyzw ACC, vf28, vf31y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf31z\n\t"
        "vmaddw.xyzw vf31, vf30, vf0w\n\t"
        "sqc2 vf31, 0(%1)\n\t"
        "nop"
        :
        : "r"(vector), "r"(destination)
        : "memory"
    );
}

/*
 * Inverts the rigid part of the weight matrix (transposed 3x3 rotation and
 * negated rotated translation), multiplies it by the given matrix with VU0
 * and stores the resulting translation row.
 */
static void _WeightToGlobalVec(Vector4 *destination, float matrix[4][4], float weight[4][4])
{
    __asm__ __volatile__(
        "lq $2, 0(%2)\n\t"
        "lq $3, 16(%2)\n\t"
        "lq $4, 32(%2)\n\t"
        "lqc2 vf3, 48(%2)\n\t"
        "qmfc2.ni $5, vf0\n\t"
        "pextlw $6, $3, $2\n\t"
        "pextuw $7, $3, $2\n\t"
        "pextlw $8, $5, $4\n\t"
        "pextuw $9, $5, $4\n\t"
        "pcpyld $2, $8, $6\n\t"
        "pcpyud $3, $6, $8\n\t"
        "pcpyld $4, $9, $7\n\t"
        "qmtc2.ni $2, vf5\n\t"
        "qmtc2.ni $3, vf6\n\t"
        "qmtc2.ni $4, vf7\n\t"
        "vmulax.xyz ACC, vf5, vf3x\n\t"
        "vmadday.xyz ACC, vf6, vf3y\n\t"
        "vmaddz.xyz vf2, vf7, vf3z\n\t"
        "vmove.w vf2, vf0\n\t"
        "lqc2 vf27, 0(%1)\n\t"
        "lqc2 vf28, 16(%1)\n\t"
        "lqc2 vf29, 32(%1)\n\t"
        "lqc2 vf30, 48(%1)\n\t"
        "vsub.xyz vf2, vf0, vf2\n\t"
        "vmulax.xyzw ACC, vf27, vf5x\n\t"
        "vmadday.xyzw ACC, vf28, vf5y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf5z\n\t"
        "vmaddw.xyzw vf5, vf30, vf5w\n\t"
        "vmulax.xyzw ACC, vf27, vf6x\n\t"
        "vmadday.xyzw ACC, vf28, vf6y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf6z\n\t"
        "vmaddw.xyzw vf3, vf30, vf6w\n\t"
        "vmulax.xyzw ACC, vf27, vf7x\n\t"
        "vmadday.xyzw ACC, vf28, vf7y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf7z\n\t"
        "vmaddw.xyzw vf4, vf30, vf7w\n\t"
        "vmulax.xyzw ACC, vf27, vf2x\n\t"
        "vmadday.xyzw ACC, vf28, vf2y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf2z\n\t"
        "vmaddw.xyzw vf30, vf30, vf2w\n\t"
        "sqc2 vf30, 0(%0)\n\t"
        "nop"
        :
        : "r"(destination), "r"(matrix), "r"(weight)
        : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9", "memory"
    );
}

/* Stores componentwise minima in the first vector and maxima in the second. */
static void _MinMaxSort(Vector4 *min, Vector4 *max) {
    __asm__ __volatile__(
        "lqc2 vf20, 0(%0)\n\t"
        "lqc2 vf21, 0(%1)\n\t"
        "vmini.xyz vf22, vf20, vf21\n\t"
        "vmax.xyz vf23, vf20, vf21\n\t"
        "sqc2 vf22, 0(%0)\n\t"
        "sqc2 vf23, 0(%1)\n\t"
        "nop"
        :
        : "r"(min), "r"(max)
        : "memory"
    );
}

extern void xglStudioChange(int);
extern void xglStudioGetLight(void *);
extern void xglLightCalcMatrix(void *);
extern unsigned char s_inGblPlace[];
extern Vector4 s_inScale;

static void CLEAR_LAYOUT_MODEL(LayoutStore *arg0)
{
    unsigned char *light;
    unsigned char *dst = (unsigned char *)arg0;

    xglStudioChange(0);
    xglStudioGetLight((void *)&light);
    xglLightCalcMatrix((void *)light);
    __asm__ volatile("lq $2, 0(%1)\n\tsq $2, 0(%0)\n\tlq $2, 16(%1)\n\tsq $2, 16(%0)\n\tlq $2, 32(%1)\n\tsq $2, 32(%0)\n\tlq $2, 48(%1)\n\tsq $2, 48(%0)" : : "r"(dst), "r"(light + 0xB0) : "$2", "memory");
    __asm__ volatile("lq $2, 0(%1)\n\tsq $2, 0(%0)\n\tlq $2, 16(%1)\n\tsq $2, 16(%0)\n\tlq $2, 32(%1)\n\tsq $2, 32(%0)\n\tlq $2, 48(%1)\n\tsq $2, 48(%0)" : : "r"(dst + 0x40), "r"(light + 0x70) : "$2", "memory");
    __asm__ volatile("lq $2, 0(%1)\n\tsq $2, 0(%0)\n\tlq $2, 16(%1)\n\tsq $2, 16(%0)\n\tlq $2, 32(%1)\n\tsq $2, 32(%0)\n\tlq $2, 48(%1)\n\tsq $2, 48(%0)" : : "r"(s_inGblPlace), "r"(dst + 0x80) : "$2", "memory");
    __asm__ volatile("lq $2, 0(%1)\n\tsq $2, 0(%0)" : : "r"(&s_inGblPos), "r"(dst + 0x1F0) : "$2", "memory");
    arg0->slots[0x88].f = 1.0f;
    arg0->slots[0x89].f = 2.0f;
    arg0->slots[0xB0].f = 1.0f;
    arg0->fields.texture = 0;
    xglMatrixUnit((float (*)[4])&arg0->slots[0x20]);
    xglMatrixUnit((float (*)[4])&arg0->slots[0x30]);
    arg0->slots[0x8F].f = 0.0f;
    arg0->fields.render_status &= 0xFF000000;
    arg0->fields.alpha = 0x44;
    arg0->slots[0x8A].f = 1.0f;
    arg0->slots[0xC0].f = 1.0f;
    arg0->slots[0xC1].f = 1.0f;
    arg0->slots[0xC2].f = 1.0f;
    arg0->slots[0xC3].f = 1.0f;
    arg0->slots[0xC4].f = 0.0f;
    arg0->slots[0xC5].f = 0.0f;
    arg0->slots[0xC6].f = 0.0f;
    arg0->slots[0xC7].f = 0.0f;
    s_inScale.z = 1.0f;
    s_inScale.y = 1.0f;
    s_inScale.x = 1.0f;
    arg0->fields.pixelAlpha = 0x75;
    arg0->slots[0x8B].f = 1.0f;
    arg0->slots[0x83].f = 1.0f;
    arg0->slots[0x82].f = 1.0f;
    arg0->slots[0x81].f = 1.0f;
    arg0->slots[0x80].f = 1.0f;
    arg0->slots[0x8D].f = 0.0f;
    arg0->slots[0xA0].f = 0.0f;
    arg0->slots[0xB2].f = 0.0f;
    arg0->slots[0x8C].f = 0.0f;
    arg0->slots[0x9D].f = 1.0f;
    arg0->slots[0x9F].f = 0.0f;
    arg0->slots[0x9A].f = 0.0f;
    arg0->slots[0xAA].f = 0.0f;
    arg0->slots[0xAC].f = 0.0f;
    arg0->slots[0xAD].f = 0.0f;
    arg0->slots[0xAE].f = 0.0f;
    arg0->slots[0xAF].f = 1.0f;
    arg0->slots[0xBC].f = 1.0f;
    arg0->slots[0xBD].f = 1.0f;
    arg0->slots[0xBE].f = 1.0f;
    arg0->slots[0xBF].f = 1.0f;
    arg0->slots[0xB1].f = 0.0f;
    __asm__ volatile("sq $0, 0(%0)\n\tsq $0, 16(%0)\n\tsq $0, 32(%0)\n\tsq $0, 48(%0)" : : "r"(dst + 0x140) : "memory");
    __asm__ volatile("sq $0, 0(%0)" : : "r"(dst + 0x1F0) : "memory");
    __asm__ volatile("sq $0, 0(%0)" : : "r"((unsigned char *)arg0 + 0x210) : "memory");
}

static void set_circle_shadow_ratio(int scale);
extern int s_nCircleShadow;
extern unsigned char s_aCircleShadow[];
extern unsigned char s_aCircleShadowClut[];

static void CONSTRUCT_CIRCLR_SHADOW(void)
{
    int i, j;
    int v;
    float dy2;
    float dy;
    float dx;
    float r;
    float a;
    float c14 = 14.0f;
    unsigned char *p, *q, *row;
    unsigned char *base;
    int *w;

    if (s_nCircleShadow == 0) {
        for (i = 0, base = s_aCircleShadow; i < 0x40; i++) {
            dy = (float)i - 32.0f;
            dy2 = dy * dy;
            row = (unsigned char *)(i << 6);
            row += (int)base;
            p = row + 0x20;
            for (j = 0; j < 0x40; j++) {
                dx = (float)j - 32.0f;
                r = __builtin_sqrtf(dx * dx + dy2);
                if (r <= 1.0f) {
                    r = 0.0f;
                } else if (r > 32.0f) {
                    r = 1.0f;
                } else {
                    r = (r - 1.0f) / 31.0f;
                }
                a = (1.0f - r) * c14;
                q = p;
                v = (int)a;
                if (a >= 2.1474836e9f) {
                    v = (int)(a - 2.1474836e9f);
                    v |= 0x80000000;
                }
                p++;
                *q = v;
            }
        }
        w = (int *)s_aCircleShadowClut;
        w[3] = 0x50000041;
        w[4] = 0x8040;
        w[0] = 0;
        w[1] = 0;
        w[2] = 0;
        w[5] = 0x08000000;
        w[6] = 0;
        w[7] = 0;
        w = (int *)s_aCircleShadow;
        w[4] = 0x8100;
        w[3] = 0x50000101;
        w[5] = 0x08000000;
        w[0] = 0;
        w[1] = 0;
        w[2] = 0;
        w[6] = 0;
        w[7] = 0;
        set_circle_shadow_ratio(0x1E);
        s_nCircleShadow = 1;
    }
}

static void CONSTRUCT_ALPHA_GROUP(void)
{
    s_nAlphaGroup = 0;
    s_nNonAlphaGroup = 0;
}

static void INIT_ALPHA_GROUP(void)
{
    CONSTRUCT_ALPHA_GROUP();
}

static void FLUSH_ALPHA_GROUP(void)
{
    s_nAlphaGroup = 0;
    s_nNonAlphaGroup = 0;
}

/*
 * The only word this TU's CLEAR_PROREAL evidences within its caller's
 * "proreal" render block (nmlModelClear passes &s_inProReal, main
 * 0x0095b940, further modeled in part by src/main/nml_packet_add.h's
 * NmlProRealParam at its own +0x11c field): nothing else in this
 * allocation reads or writes byte offset 0x1c0, so it stays a named
 * array index rather than a guessed struct member.
 */
static void CLEAR_PROREAL(int *proReal)
{
    proReal[0x1c0 / 4] = 0;
}

/*
 * s_inBackBuffer (main 0x0095bb20, see the header comment) is reset one
 * word at a time below; only +0x10 and +0x1c/+0x24 have evidenced roles
 * within this allocation:
 *   - +0x10 "owner index": set to -1 (no owner) by CONSTRUCT/INIT.
 *   - +0x1c: D_0095BB3C, the flag nmlModelIsBackBufferRequest reads and
 *     nmlModelSendPacketChangeSignal clears (both above, this TU).
 *   - +0x24: D_0095BB44, the value nmlModelSetMpeg2CrossFadeTime writes;
 *     INIT_BACK_BUFFER is the only one of these three that leaves it
 *     untouched, preserving the configured cross-fade time.
 */
static void CONSTRUCT_BACK_BUFFER(void)
{
    s_inBackBuffer[0x10 / 4] = -1;
    s_inBackBuffer[0x14 / 4] = 0;
    s_inBackBuffer[0x00 / 4] = 0;
    s_inBackBuffer[0x04 / 4] = 0;
    s_inBackBuffer[0x08 / 4] = 0;
    s_inBackBuffer[0x0c / 4] = 0;
    s_inBackBuffer[0x18 / 4] = 0;
    s_inBackBuffer[0x1c / 4] = 0;
    s_inBackBuffer[0x20 / 4] = 0;
    s_inBackBuffer[0x24 / 4] = 0;
}

static void INIT_BACK_BUFFER(void)
{
    s_inBackBuffer[0x10 / 4] = -1;
    s_inBackBuffer[0x14 / 4] = 0;
    s_inBackBuffer[0x00 / 4] = 0;
    s_inBackBuffer[0x04 / 4] = 0;
    s_inBackBuffer[0x08 / 4] = 0;
    s_inBackBuffer[0x0c / 4] = 0;
    s_inBackBuffer[0x18 / 4] = 0;
    s_inBackBuffer[0x1c / 4] = 0;
    s_inBackBuffer[0x20 / 4] = 0;
}

/*
 * The active-request word FLUSH_BACK_BUFFER's caller (nmlModelFlushClear)
 * clears once per frame; CONSTRUCT/INIT_BACK_BUFFER reset it the same way
 * among the other words above.
 */
static void FLUSH_BACK_BUFFER(void)
{
    s_inBackBuffer[0x00 / 4] = 0;
}

static void CONSTRUCT_PARENT_BUF(void)
{
    s_nParentBuf = 0;
}

static void INIT_PARENT_BUF(void)
{
    CONSTRUCT_PARENT_BUF();
}

static void FLUSH_PARENT_BUF(void)
{
    s_nParentBuf = 0;
}

/*
 * mapHandle[0] and mapHandle[1] are the two words this TU's
 * CONSTRUCT_MAP_HANDLE/CLEAR_MAP_HANDLE/FLUSH_MAP_HANDLE evidence within
 * the caller's map-handle record (nmlModelConstruct passes &s_inMapHandle,
 * main 0x0095db50): CLEAR_MAP_HANDLE resets only mapHandle[0] and
 * FLUSH_MAP_HANDLE resets only mapHandle[1], so the two words are kept
 * separate rather than folded into one guessed struct member.
 */
static void CONSTRUCT_MAP_HANDLE(int *mapHandle)
{
    /*
     * The original writes word 0 and puts the word-1 store in the return's
     * delay slot; the two stores are independent, and in declaration order
     * cc1 schedules them the other way round (form 01, build/form-01,
     * first difference at 0x6e8: sw zero,4(a0) for sw zero,0(a0)).
     */
    mapHandle[1] = 0;
    mapHandle[0] = 0;
}

static void INIT_MAP_HANDLE(int *mapHandle)
{
    CONSTRUCT_MAP_HANDLE(mapHandle);
}

static void CLEAR_MAP_HANDLE(int *mapHandle)
{
    mapHandle[0] = 0;
}

static void FLUSH_MAP_HANDLE(int *mapHandle)
{
    mapHandle[1] = 0;
}

static void CONSTRUCT_FADE_CONTROL(FadeControl *control)
{
    control->frame = -1;
    control->duration = 1;
    control->skipRender = 0;
    control->dispose = 0;
    control->cancelFrames = 0;
    control->startDelay = 0;
    control->locked = 0;
}

static void INIT_FADE_CONTROL(FadeControl *control)
{
    CONSTRUCT_FADE_CONTROL(control);
}

extern int s_nDispVisible;
extern char s_aMatName[];
extern char s_aTexName[];
extern int s_nToumeiNum;
extern int s_nShapeNum;

static void CLEAR_MODEL_ENTRY(void)
{
    char *tex;
    char *mat;
    s_nDispVisible = -1;
    tex = s_aTexName;
    mat = s_aMatName;
    *mat = 0;
    *tex = 0;
    s_nMapShadowParts = 0;
    s_nShadowVec = 0;
    s_nToumeiNum = 0;
    s_fSortOffsetEntry = 0;
    s_nParent = 0;
    s_nClip = 0;
    s_nShapeNum = 0;
    s_nMapClip = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", CONSTRUCT_MODELSYSTEM);

/* CONSTRUCT_MODELSYSTEM (above) is still INCLUDE_ASM; declare it so its
 * caller does not see an implicit declaration. */
static void CONSTRUCT_MODELSYSTEM(void);

static void INIT_MODELSYSTEM(void)
{
    CONSTRUCT_MODELSYSTEM();
}

typedef int FlushQ __attribute__((mode(TI)));
extern int s_nBlocks, s_nUseZwrite, s_nModel, s_nDirect, s_nBlocksAlpha, s_nBlocksAlphaLast, s_nAnotherStudio, s_nUseStealth, s_nUseGnosys, s_nMapAlphaEntry;
extern void *xglStudioSelectGetActiveCamera(int studio_index);
extern float xglPointLength(const void *point1, const void *point2);
extern FlushQ s_inMainCameraPos;
extern FlushQ s_inMainCameraAng;
extern int s_nMainCameraWarp;
extern float D_004D88D8;
extern float D_004D88DC;

static void FLUSH_MODELSYSTEM(void) {
    char *cam;
    FlushQ *pos;
    FlushQ *ang;

    s_nBlocks = 1;
    s_nModel = 0;
    s_nPacketSignal += 1;
    s_nDirect = 0;
    s_nBlocksAlpha = 0;
    s_nBlocksAlphaLast = 0;
    s_nAnotherStudio = 0;
    s_nUseStealth = 0;
    s_nUseGnosys = 0;
    s_nUseZwrite = 0;
    s_nUseBackBuffer = 0;
    s_nMapAlphaEntry = 0;
    s_nFrameLockOff = 0;
    cam = xglStudioSelectGetActiveCamera(0);
    if (cam != 0) {
        __asm__ __volatile__(
            "lq $2, 0(%1)\n"
            "sq $2, 0(%0)\n"
            :
            : "r"(&s_inMainCameraPos), "r"(cam + 0xd0)
            : "$2", "memory");
        __asm__ __volatile__(
            "lq $2, 0(%1)\n"
            "sq $2, 0(%0)\n"
            :
            : "r"(&s_inMainCameraAng), "r"(cam + 0xa0)
            : "$2", "memory");
        s_nMainCameraWarp = 0;
    }
}

void nmlModelSetBackBufferClear(void)
{
    INIT_BACK_BUFFER();
}

void nmlModelSetFaceModel(int enabled)
{
    if (enabled != 0) {
        s_inLayout.slots[0x250 / 4].i |= 0x100000;
    }
}

void nmlModelSetHumanModel(int enabled)
{
    if (enabled != 0) {
        s_inLayout.slots[0x250 / 4].i |= 0x40;
    }
}

void nmlModelSetMapClip(int enabled)
{
    if (enabled != 0) {
        s_nMapClip = 1;
    }
}

void nmlModelSendRenderCancel(void)
{
}

int nmlModelGetRenderCancel(void)
{
    return s_nRenderCancelOld;
}

void nmlModelSendBackBufferSignal(void)
{
    s_nUseBackBuffer = 1;
}

int nmlModelIsBackBufferRequest(void)
{
    return D_0095BB3C[0];
}

void nmlModelSendPauseSignal(int pause)
{
    s_nPause = pause;
}

void nmlModelSendMenuStart(void)
{
    s_nMenu = 1;
}

void nmlModelSendMenuEnd(void)
{
    s_nMenu = 0;
}

int nmlModelGetMenuStatus(void)
{
    return s_nMenu;
}

void nmlModelSendSignalStartBattle(void)
{
    s_nFrameLockOff = 1;
}

void nmlModelSendSignalEventFinish(void)
{
    s_nFrameLockOff = 1;
}

void nmlModelSendPacketChangeSignal(void)
{
    s_nPacketSignal = 0;
    D_0095BB3C[0] = 0;
}

/* INIT_BACK_BUFFER (above) is still INCLUDE_ASM; declare it so its caller
 * does not see an implicit declaration. */
static void INIT_BACK_BUFFER(void);

void nmlModelSendSignalMovieStart(void)
{
    INIT_BACK_BUFFER();
    INIT_FADE_CONTROL(&s_inFadeIn);
    INIT_FADE_CONTROL(&s_inFadeOut);
    INIT_FADE_CONTROL(&s_inActiveFadeIn);
    INIT_FADE_CONTROL(&s_inActiveFadeOut);
}

void nmlModelSetMpeg2CrossFadeTime(int time)
{
    D_0095BB44[0] = time;
}

/*
 * TU-local partial view of the shared render-state global sRender (main
 * 0x004A90E0, size 0x5C; see src/main/game_over.c's DrawImageRenderState,
 * src/main/map_1.c's MapRenderState, src/main/window_tex_load.c's
 * UmnRenderSize and src/main/game_defocus.c's DefocusRenderState for other
 * TUs' views of the same object). Only the halfword at +0x22 is evidenced
 * here: nmlModelSendSignalMovieFinish below forwards it unchanged as the
 * "ownerIndex" argument of nmlModelSetBackBuffer and
 * nmlModelSetBackBufferToBattle.
 */
typedef struct {
    unsigned char unmodeled_00[0x22];
    unsigned short ownerIndex;  /* +0x22 */
} NmlBackBufferOwnerView;

extern NmlBackBufferOwnerView sRender;

/*
 * nmlModelSetFadeInCancel/nmlModelSetFadeOutCancel/nmlModelSetBackBuffer
 * (below, this TU) and nmlModelSetBackBufferToBattle (still INCLUDE_ASM)
 * are used before their definitions; declare them so these calls do not
 * see an implicit declaration.
 */
void nmlModelSetFadeInCancel(int frames);
void nmlModelSetFadeOutCancel(int frames);
void nmlModelSetBackBuffer(int modelId, int duration, int ownerIndex, int value);
void nmlModelSetBackBufferToBattle(int ownerIndex);

/*
 * The caller (GameMpeg2Play, main 0x00250c18) never inspects the result:
 * the very next instruction after the call is an unrelated jal, so this is
 * void. The case labels are declared in the order the original jump
 * table's targets are laid out in .text (0, 1, 4, 2, 3), not in numeric
 * order, matching the compiled block layout.
 */
void nmlModelSendSignalMovieFinish(int mode)
{
    switch (mode - 1) {
    case 0:
        nmlModelSetBackBufferToBattle(sRender.ownerIndex);
        nmlModelSetFadeInCancel(30);
        nmlModelSetFadeOutCancel(30);
        break;
    case 1:
        nmlModelSetBackBuffer(0x26, 1, sRender.ownerIndex, 0);
        nmlModelSetFadeInCancel(30);
        break;
    case 4:
        nmlModelSetBackBuffer(0x26, 1, sRender.ownerIndex, 0);
        nmlModelSetFadeOutCancel(30);
        nmlModelSetFadeInCancel(30);
        break;
    case 2:
        nmlModelSetBackBuffer(3, 1, sRender.ownerIndex, 0);
        nmlModelSetFadeInCancel(30);
        break;
    case 3:
        /*
         * Unlike the other cases, this one's call is a genuine jal that
         * falls into the shared epilogue below rather than a tail jump
         * straight into the callee; the single-iteration loop reproduces
         * that call shape.
         */
        nmlModelSetBackBuffer(D_0095BB44[0], 1, sRender.ownerIndex, 1);
        do {
            nmlModelSetFadeInCancel(30);
        } while (0);
        break;
    }
}

/*
 * s_inLayout.slots[0xac] (byte offset 0x2b0) is a bitmask word this
 * function only ever ORs new bits into, the same raw-slot idiom
 * nmlModelSetFaceModel/nmlModelSetHumanModel use for the render-status word
 * at +0x250 above.
 */
int nmlModelSetRenderLevel(int level)
{
    int renderLevel = s_inLayout.slots[0x2b0 / 4].i | level;
    s_inLayout.slots[0x2b0 / 4].i = renderLevel;
    return renderLevel;
}

extern void sceVif1PkCnt(NmlPacket packet, int count);
extern void sceVif1PkOpenDirectHLCode(NmlPacket packet, int count);
extern void sceVif1PkAddDirectDataN(NmlPacket packet, const void *data, int count);
extern void sceVif1PkCloseDirectHLCode(NmlPacket packet);
typedef struct {
    int r;
    int g;
    int b;
    int alpha;
    int frame;
    int duration;
    int skipRender;
    int mode;
    int unk20;
    int dispose;
    int cancelFrames;
    int startDelay;
    int locked;
} FadeView;

static void fade_render(NmlPacket packet, FadeControl *control)
{
    FadeView *c = (FadeView *)control;
    struct {
        unsigned long long d[32];
    } pk;
    unsigned long long *p;
    unsigned long long *tag;
    unsigned int sel;
    int mode;
    int frame;
    int disp;

    mode = c->mode;
    sel = mode != 0 ? 0x2FFFFFF : 0xFFFFFC;
    if (c->startDelay != 0) {
        c->startDelay--;
        return;
    }
    frame = c->frame;
    disp = c->dispose;
    if (frame >= 0) {
        if (disp != 0) {
            c->frame = 0;
            frame = 0;
        }
        if (frame >= 0) {
            if (c->cancelFrames == 0) {
                if (mode == 0) {
                    c->alpha = (float)(frame - 5) / (float)c->duration * 128.0f;
                } else {
                    c->alpha = (float)frame / (float)c->duration * 128.0f;
                }
                if (mode == 0) {
                    c->alpha = 0x80 - c->alpha;
                }
                if (c->alpha < 0) {
                    c->alpha = 0;
                }
                if (c->alpha > 0x80) {
                    c->alpha = 0x80;
                }
                if (c->skipRender == 0) {
                    p = pk.d;
                    ((union { unsigned long long d; int w[2]; } *)p)->d = 0x1000000000008000ULL;
                    p++;
                    *p++ = 0xE;
                    *p++ = ((0x01BF0000ULL << 16) | 0x1FF) << 16;
                    *p++ = 0x40;
                    *p++ = 0x44;
                    *p++ = 0x42;
                    *p++ = 0x30000;
                    *p++ = 0x47;
                    *p++ = 0x46;
                    *p++ = 0;
                    *p++ = (unsigned long long)c->r
                        | ((unsigned long long)c->g << 8)
                        | ((unsigned long long)c->b << 16)
                        | ((unsigned long long)c->alpha << 24)
                        | ((unsigned long long)0x3F800000 << 32);
                    *p++ = 1;
                    *p++ = ((unsigned long long)sel << 32) | 0x72007000;
                    *p++ = 4;
                    *p++ = ((unsigned long long)sel << 32) | 0x8E009000;
                    *p++ = 4;
                    pk.d[0] += (p - pk.d) / 2 - 1;
                    sceVif1PkCnt(packet, 0);
                    sceVif1PkOpenDirectHLCode(packet, 0);
                    sceVif1PkAddDirectDataN(packet, pk.d, 8);
                    sceVif1PkCloseDirectHLCode(packet);
                    frame = c->frame;
                    disp = c->dispose;
                }
            }
            if (frame >= 0) {
                c->frame = frame - 1;
                if (frame - 1 < 0) {
                    if (c->locked != 0) {
                        c->frame = 0;
                    } else {
                        c->skipRender = 0;
                    }
                }
            }
        }
    }
    if (disp != 0 || c->cancelFrames > 0) {
        c->frame = -1;
        c->duration = 1;
        c->skipRender = 0;
        c->dispose = 0;
        c->locked = 0;
    }
}

static void fade_render(NmlPacket packet, FadeControl *control);

void nmlFadePacketWrite(NmlPacket packet)
{
    fade_render(packet, &s_inActiveFadeIn);
    fade_render(packet, &s_inActiveFadeOut);
    fade_render(packet, &s_inFadeIn);
    fade_render(packet, &s_inFadeOut);
    s_nFadeDoit = 0;
}

static void fade_set(FadeControl *f, float *color, int level, int duration, int a4, int a5, int delay)
{
    union { int i; float f; } *p = (void *)f;
    p[4].i = duration;
    p[5].i = duration;
    p[0].i = color[0] * 255.0f;
    p[1].i = color[1] * 255.0f;
    p[2].i = color[2] * 255.0f;
    p[6].i = level;
    p[7].i = a4;
    p[8].i = a5;
    p[11].i = delay;
    p[3].i = 0;
    p[9].i = 0;
    if (duration < 0) {
        p[4].i = -1;
    }
    *(void **)((char *)&sRender + 0x44) = nmlFadePacketWrite;
}

/*
 * D_0095DB90 is main:0x0095DB90, the same byte the FadeControl comment above
 * ties to s_inFadeOut's "locked" field at +0x30 (0x0095DB60 + 0x30): cited
 * there only as evidence, not as claimed source. Addressing it through
 * &s_inFadeOut+0x30 rather than its own symbol changes the emitted %lo
 * immediate (sw v0,48(v1) for the original's sw v0,0(v1)), so this
 * function keeps the flat scaffold symbol, as an unsized array so -G8
 * small-data gp-relative addressing does not apply to it either (matching
 * D_0095BB3C/D_0095BB44 above).
 */
extern int D_0095DB90[];

void nmlModelSetFadeOutLock(void) {
    D_0095DB90[0] = 1;
}

void nmlModelSetFadeOutLockOff(void) {
    D_0095DB90[0] = 0;
}

/* D_0095DC48 is s_inActiveFadeIn's "cancelFrames" byte (+0x28); see the
 * D_0095DB90 comment above for why it stays a flat scaffold symbol. */
extern int D_0095DC48[];

void nmlModelSetActiveFadeInCancel(int frames) {
    D_0095DC48[0] = (frames < 0) ? 0 : frames;
}

/* D_0095DBC8 is s_inFadeIn's "cancelFrames" byte (+0x28). */
extern int D_0095DBC8[];

void nmlModelSetFadeInCancel(int frames) {
    D_0095DBC8[0] = (frames < 0) ? 0 : frames;
}

/* D_0095DC08 is s_inActiveFadeOut's "cancelFrames" byte (+0x28). */
extern int D_0095DC08[];

void nmlModelSetActiveFadeOutCancel(int frames) {
    D_0095DC08[0] = (frames < 0) ? 0 : frames;
}

/* D_0095DB88 is s_inFadeOut's "cancelFrames" byte (+0x28). */
extern int D_0095DB88[];

void nmlModelSetFadeOutCancel(int frames) {
    D_0095DB88[0] = (frames < 0) ? 0 : frames;
}

void nmlModelFadeDoit(void)
{
    if (s_inFadeIn.cancelFrames != 0) {
        s_inFadeIn.cancelFrames--;
    }
    if (s_inFadeOut.cancelFrames != 0) {
        s_inFadeOut.cancelFrames--;
    }
    if (s_inActiveFadeIn.cancelFrames != 0) {
        s_inActiveFadeIn.cancelFrames--;
    }
    if (s_inActiveFadeOut.cancelFrames != 0) {
        s_inActiveFadeOut.cancelFrames--;
    }
}

void nmlModelSetFadeDoit(void)
{
    s_nFadeDoit = 1;
}

/* D_0095DB84 is s_inFadeOut's "dispose" byte (+0x24). */
extern int D_0095DB84[];

void nmlModelSetFadeOutDispose(void) {
    D_0095DB84[0] = 1;
}

/* D_0095DBC4 is s_inFadeIn's "dispose" byte (+0x24). */
extern int D_0095DBC4[];

void nmlModelSetFadeInDispose(void) {
    D_0095DBC4[0] = 1;
}

/*
 * TU-local partial view of GameLoopState (main 0x00338680, size 0x2a030;
 * TU-local by canon, config/header-canon.json). Only the two fields this
 * setter reads are modeled: the fade level at +0x58 and the three fade
 * color components at +0x90, both forwarded to fade_set unchanged.
 */
typedef struct {
    unsigned char unmodeled_00[0]; /* +0x0: zero-length alias of the former padding member, kept for saved bodies */
    unsigned char unmodeled_000[0xc - 0x0];
    unsigned short stealthMode; /* +0xc */
    unsigned char unmodeled_00e[0x58 - 0xe];
    int level;                 /* +0x58 */
    unsigned char unmodeled_5c[0x90 - 0x5c];
    float color[3];            /* +0x90 */
} NmlGameLoopState;

extern NmlGameLoopState GameLoopState;

/* fade_set (near the top of this TU) is still INCLUDE_ASM; declare it so
 * this caller does not see an implicit declaration. */
static void fade_set(FadeControl *, float *, int, int, int, int, int);

void nmlModelSetFadeOut(int duration, int mode) {
    fade_set(&s_inFadeOut, GameLoopState.color, GameLoopState.level, duration + 2, 0, mode, 0);
}

void nmlModelSetFadeIn(int duration, int mode)
{
    float *c = (float *)((char *)&GameLoopState + 0xA0);
    union { int i; float f; } *g = (void *)((char *)c - 0xA0);
    fade_set(&s_inFadeIn, c, g[0x5C / 4].i, duration, 1, mode, 0);
    {
        float z = 0.0f;
        float t = 30.0f;
        g[0x58 / 4].i = 0;
        g[0xA8 / 4].f = z;
        g[0xA4 / 4].f = z;
        g[0xA0 / 4].f = z;
        g[0xAC / 4].f = t;
        g[0x5C / 4].i = 0;
        g[0x98 / 4].f = z;
        g[0x94 / 4].f = z;
        g[0x90 / 4].f = z;
        g[0x9C / 4].f = t;
    }
}

void nmlModelSetFadeInInterrupt(float r, float g, float b, int frames)
{
    int *rgba = (int *)s_inFadeIn.color;

    rgba[0] = (int)(r * 255.0f);
    rgba[1] = (int)(g * 255.0f);
    rgba[2] = (int)(b * 255.0f);
    rgba[3] = 0;
    s_inFadeIn.duration = frames;
    s_inFadeIn.frame = frames;
}

void nmlModelSetActiveFadeOut(int duration, unsigned int color, int delay)
{
    float c[3];

    if (s_inActiveFadeOut.frame < 0) {
        c[0] = (float)(color & 0xFF) * 0.0078125f;
        c[1] = (float)((color >> 8) & 0xFF) * 0.0078125f;
        c[2] = (float)((color >> 16) & 0xFF) * 0.0078125f;
        fade_set(&s_inActiveFadeOut, c, 0, duration + 2, 0, 1, delay);
    }
}

void nmlModelSetActiveFadeIn(int duration, unsigned int color, int delay)
{
    float c[3];

    c[0] = (float)(color & 0xFF) * 0.0078125f;
    c[1] = (float)((color >> 8) & 0xFF) * 0.0078125f;
    c[2] = (float)((color >> 16) & 0xFF) * 0.0078125f;
    fade_set(&s_inActiveFadeIn, c, 0, duration, 1, 1, delay);
}

int nmlModelGetFadeLevel(void)
{
    int active = 1;

    if (s_inFadeIn.frame < 0) {
        active = 0;
    }
    if (s_inFadeOut.frame >= 0) {
        active = 1;
    }
    if (s_inActiveFadeIn.frame >= 0) {
        active = 1;
    }
    if (s_inActiveFadeOut.frame >= 0) {
        active = 1;
    }
    return active;
}

int nmlModelAskNonLinearCamera(void) {
    void *camera;
    float length;
    float y_diff;
    float *ang_ptr;
    float *cam_ptr;
    float vu_result[4];

    if (s_nMainCameraWarp == 0) {
        camera = xglStudioSelectGetActiveCamera(0);
        if (camera != 0) {
            s_nMainCameraWarp = 1;
            length = xglPointLength(&s_inMainCameraPos, camera + 0xD0);

            if (length >= 1.0f) {
                s_nMainCameraWarp = 2;
            } else {
                ang_ptr = (float *)&s_inMainCameraAng;
                cam_ptr = (float *)(camera + 0xA0);
                __asm__ __volatile__(
                    "lqc2 $vf3,0(%1)\n\t"
                    "lqc2 $vf2,0(%2)\n\t"
                    "vsub.xyz $vf2,$vf2,$vf3\n\t"
                    "sqc2 $vf2,0(%0)"
                    :
                    : "r"(vu_result), "r"(cam_ptr), "r"(ang_ptr)
                    : "memory");
                y_diff = __builtin_fabsf(vu_result[1]);
                if (D_004D88D8 < y_diff && y_diff < D_004D88DC) {
                    s_nMainCameraWarp = 2;
                }
            }
        }
    }

    return !(s_nMainCameraWarp ^ 2);
}

void nmlModelGetGblPosition(Vector4 *position)
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)"
        :
        : "r"(position), "r"(&s_inGblPos)
        : "$2", "memory"
    );
    position->w = 1.0f;
}

void nmlModelSetBackBufferToBattle(int ownerIndex)
{
    int *p = s_inBackBuffer;
    int one = 1;
    p[6] = 0x28;
    p[0] = one;
    __asm__ __volatile__("" ::: "memory");
    p[4] = ownerIndex;
    p[5] = one;
    p[8] = 0;
    p[1] = 0;
}

extern int D_0095DB70[];
extern int D_0095DBB0[];
extern int D_0095DBF0[];
extern int D_0095DC30[];

void nmlModelSetBackBufferToEventSkip(int ownerIndex)
{
    int *p = s_inBackBuffer;
    int one = 1;
    p[6] = 0x28;
    p[0] = one;
    __asm__ __volatile__("" ::: "memory");
    p[4] = ownerIndex;
    p[5] = one;
    p[8] = 0;
    p[1] = 0;
}

void nmlModelSetBackBuffer(int modelId, int duration, int ownerIndex, int value)
{
    s_inBackBuffer[8 / 4] = modelId;
    s_inBackBuffer[0x10 / 4] = ownerIndex;
    s_inBackBuffer[0x20 / 4] = value;
    s_inBackBuffer[0xC / 4] = duration;
    s_inBackBuffer[0x14 / 4] = 0;
    s_inBackBuffer[0x00 / 4] = 0;
    s_inBackBuffer[0x04 / 4] = 0;
    if (duration <= 0) {
        s_inBackBuffer[0xC / 4] = 1;
    }
}

void nmlModelSetEffectWrite(int enabled)
{
    s_nEffectWrite = enabled;
}

extern int g_aSubWindow[4];

void nmlModelUseSubWindow(unsigned int index, int enabled) {
    if (index < 4U) {
        g_aSubWindow[index] = enabled;
    }
}

void nmlModelSetWindow(u32 window) {
    if (window < 4U) {
        s_inLayout.slots[0x27c / 4].i = window;
    }
}

void nmlModelSetSortOffset(float offset)
{
    s_fSortOffsetEntry = offset;
}

typedef int qword_t __attribute__((mode(TI)));
extern qword_t s_inGblPointP[];
extern int s_inGblPointC[];
void xglVectorScaleXYZ(float scale, Vector4 *destination, const Vector4 *source);

void nmlModelSetGlobalPointLight(unsigned int idx, qword_t *pos, Vector4 *col)
{
    if (idx < 3) {
        __asm__ __volatile__(
            "lq $2, 0x0(%0)\n"
            "sq $2, 0x0(%1)\n"
            : : "r"(pos), "r"(&s_inGblPointP[idx]) : "$2", "memory");
        xglVectorScaleXYZ(128.0f, (Vector4 *)&((qword_t *)s_inGblPointC)[idx], col);
        s_inLayout.slots[0x250 / 4].i |= 0x2000000;
    }
}

void nmlModelSetGlobalPointLightCol(unsigned int idx, Vector4 *col)
{
    if (idx < 3) {
        xglVectorScaleXYZ(128.0f, (Vector4 *)&((qword_t *)s_inGblPointC)[idx], col);
        s_inLayout.slots[0x250 / 4].i |= 0x2000000;
    }
}

void nmlModelSetGlobalPointLightPos(unsigned int idx, qword_t *pos)
{
    if (idx < 3) {
        __asm__ __volatile__(
            "lq $2, 0x0(%0)\n"
            "sq $2, 0x0(%1)\n"
            : : "r"(pos), "r"(&s_inGblPointP[idx]) : "$2", "memory");
        s_inLayout.slots[0x250 / 4].i |= 0x2000000;
    }
}

void nmlModelSetGlobalPointLightReset(void)
{
    s_inLayout.slots[0x250 / 4].i &= ~0x2000000;
    __asm__ __volatile__(
        "sq $0, 0x0(%0)\n"
        "sq $0, 0x10(%0)\n"
        "sq $0, 0x20(%0)\n"
        "sq $0, 0x30(%0)\n"
        : : "r"(s_inGblPointC) : "memory");
}

void nmlModelSetPointLight(unsigned int idx, qword_t *pos, Vector4 *col)
{
    if (idx < 3) {
        __asm__ __volatile__(
            "lq $2, 0x0(%0)\n"
            "sq $2, 0x0(%1)\n"
            : : "r"(pos), "r"(&((qword_t *)&s_inLayout.slots[0x100 / 4])[idx]) : "$2", "memory");
        xglVectorScaleXYZ(128.0f, (Vector4 *)&((qword_t *)&s_inLayout.slots[0x140 / 4])[idx], col);
        s_inLayout.slots[0x250 / 4].i |= 0x1000;
    }
}

void nmlModelSetSpecularOff(int disableSpecular) {
    if (disableSpecular != 0) {
        s_inLayout.slots[0x2b0 / 4].i |= 2;
    }
}

void nmlModelSetShadowMapId(void)
{
}

void nmlModelSetShadowHeight(float height)
{
    s_inLayout.slots[0x2ac / 4].f = height;
    s_inLayout.slots[0x2a8 / 4].i = 1;
}

void nmlModelSetShadowVec(const Vector4 *shadowVector)
{
    __asm__ __volatile__(
        "lq $2,0(%0)\n\t"
        "sq $2,0(%1)"
        :
        : "r"(shadowVector), "r"(&s_inShadowVec)
        : "$2", "memory"
    );
    s_nShadowVec = 1;
}

void nmlModelSetTexMap(int type, int alpha, float scale)
{
    LayoutSlot *s = s_inLayout.slots;

    s[0x268 / 4].i = type;
    s[0x274 / 4].f = scale;
    s[0x270 / 4].i = alpha;
    if (alpha < 0) {
        s[0x270 / 4].f = 0.0f;
    }
    if (s[0x270 / 4].i > 0x80) {
        s[0x270 / 4].i = 0x80;
    }
    if ((*(unsigned long long *)&s[0x268 / 4] & 0x10001) == 0x10001) {
        s[0x250 / 4].i |= 0x40000;
    } else if ((*(unsigned long long *)&s[0x268 / 4] & 0x20001) == 0x20001) {
        s[0x250 / 4].i |= 0x200000;
    } else if ((*(unsigned long long *)&s[0x268 / 4] & 0x30000) == 0x30000) {
        s[0x250 / 4].i |= 0x400000;
    } else {
        s[0x250 / 4].i |= 0x4000;
    }
}

typedef struct NmlProRealUpFields {
    Matrix4 matrix_000;
    Matrix4 inverseTransform;
    /* The original Circle/Map bodies use the +0x80/+0xC0 ranges only as
     * whole-qword copy destinations. Their base is 0x0095B940 and every
     * access is at a 16-byte slot in the corresponding function.s; model
     * each owned qword directly, without an overlay. */
    Vector4 unmodeled_080[4];
    Vector4 unmodeled_0c0[4];
    int unmodeled_100;
    int unmodeled_104;
    int unmodeled_108;
    int unmodeled_10c;
    short unmodeled_110;
    short unmodeled_112;
    short unmodeled_114;
    short unmodeled_116;
    int unmodeled_118;
    int unmodeled_11c;
    float unmodeled_120;
    float unmodeled_124;
    unsigned char unmodeled_128[8];
    u64 textureWord;
    u64 clampWord;
    float textureUOffset;
    float textureUScale;
    float textureVOffset;
    float textureVScale;
    Vector4 transformOrigin;
    Vector4 unmodeled_160;
    Vector4 unmodeled_170[2];
    Vector4 unmodeled_190[3];
} NmlProRealUpFields;
typedef struct NmlProRealUp {
    NmlProRealUpFields fields;
    int projection_entry;
    unsigned char unmodeled_1c4[0x1e0 - 0x1c4];
} NmlProRealUp;
typedef struct NmlProRealUpFull {
    NmlProRealUpFields fields;
    int projection_entry;
    unsigned char unmodeled_1c4[0x1e0 - 0x1c4];
} NmlProRealUpFull;
typedef struct NmlProRealOrigin {
    u64 word0;
    u64 word8;
} NmlProRealOrigin;
extern NmlProRealOrigin D_004D2820;
extern unsigned char s_inProReal[];
void xglMatrixScale(Matrix4 destination, const Matrix4 source, const float scale[4]);
extern void xglVectorMulMat(Vector4 *destination, const Matrix4 matrix, const Vector4 *source);
extern void xglVectorNormal(Vector4 *destination, const Vector4 *source);
void xglMatrixInverse(void *dst, void *src);

void nmlModelSetProreal(NmlProRealState *state)
{
    NmlProRealUp *input = (NmlProRealUp *)state;
    u64 transformOrigin[2];
    Vector4 scale;
    Matrix4 scaledTransform;
    int textureStartX;
    int textureEndX;
    int textureStartY;
    int textureEndY;
    u64 textureXDirection;
    u64 textureYDirection;
    int screenWidth;
    int screenHeight;
    int textureMode;

    screenWidth = 0x400;
    screenHeight = 0x200;
    scale.x = (float)input->fields.unmodeled_108 * 0.015625f * input->fields.unmodeled_120;
    scale.y = (float)input->fields.unmodeled_10c * 0.015625f * input->fields.unmodeled_124;
    transformOrigin[0] = D_004D2820.word0;
    transformOrigin[1] = D_004D2820.word8;
    scale.w = 1.0f;
    scale.z = 1.0f;
    xglMatrixScale(scaledTransform, (const float (*)[4])input->fields.matrix_000, &scale.x);
    xglVectorMulMat(&input->fields.transformOrigin, (const float (*)[4])scaledTransform,
                    (const Vector4 *)transformOrigin);

    __asm__ __volatile__(
        "lqc2 vf3, 0(%0)\n"
        "lqc2 vf2, 0(%1)\n"
        "vsub.xyz vf2, vf2, vf3\n"
        "sqc2 vf2, 0(%2)\n"
        :
        : "r"(&scaledTransform[3]), "r"(&input->fields.transformOrigin),
          "r"(&input->fields.unmodeled_160)
        : "memory");

    xglMatrixInverse(input->fields.inverseTransform, scaledTransform);
    xglVectorNormal(&input->fields.unmodeled_160, &input->fields.unmodeled_160);

    textureMode = input->fields.unmodeled_114;
    switch (textureMode) {
    case 20: {
        int textureBase = (int)input->fields.unmodeled_116 << 5;
        input->fields.textureWord =
            (u64)(textureBase | 0x29440000) |
            ((u64)(textureBase + input->fields.unmodeled_118) << 37) |
            0x2000000640000000ULL;
        break;
    }
    case 19: {
        int textureBase = (int)input->fields.unmodeled_116 << 5;
        input->fields.textureWord =
            (u64)(textureBase | 0x29340000) |
            ((u64)(textureBase + input->fields.unmodeled_118) << 37) |
            0x2000000640000000ULL;
        break;
    }
    case 0:
        screenWidth = 0x200;
        input->fields.textureWord = (u64)((int)input->fields.unmodeled_116 << 5) |
                               0x24020000ULL | 0x2000000640000000ULL;
        break;
    }

    textureXDirection = input->fields.unmodeled_110 != 0 ? 3 : 2;
    textureYDirection = input->fields.unmodeled_112 != 0 ? 3 : 2;
    textureStartX = input->fields.unmodeled_110 != 0 ? input->fields.unmodeled_108 - 1 : input->fields.unmodeled_100;
    textureEndX = input->fields.unmodeled_110 != 0 ? input->fields.unmodeled_100 :
                  input->fields.unmodeled_100 + input->fields.unmodeled_108 - 1;
    textureStartY = input->fields.unmodeled_112 != 0 ? input->fields.unmodeled_10c - 1 : input->fields.unmodeled_104;
    textureEndY = input->fields.unmodeled_112 != 0 ? input->fields.unmodeled_104 :
                  input->fields.unmodeled_104 + input->fields.unmodeled_10c - 1;

    input->fields.clampWord = (u64)textureXDirection |
        ((u64)textureYDirection << 2) |
        ((u64)textureStartX << 4) |
        ((u64)textureEndX << 14) |
        ((u64)textureStartY << 24) |
        ((u64)textureEndY << 34);
    input->fields.textureUScale = (float)input->fields.unmodeled_108 / (float)screenWidth;
    input->fields.textureUOffset = (float)input->fields.unmodeled_100 / (float)screenWidth +
                            input->fields.textureUScale * 0.5f;
    input->fields.textureVScale = -(float)input->fields.unmodeled_10c / (float)screenHeight;
    input->fields.textureVOffset = (float)input->fields.unmodeled_104 / (float)screenHeight -
                            input->fields.textureVScale * 0.5f;

    __asm__ __volatile__("lq $2, 0(%1)\n\tsq $2, 0(%0)"
                         : : "r"(&((NmlProRealUpFull *)input)->fields.unmodeled_0c0[0]),
                             "r"(&((NmlProRealUp *)s_inProReal)->fields.unmodeled_170[0])
                         : "$2", "memory");
    __asm__ __volatile__("sq $0, 0(%0)"
                         : : "r"(&((NmlProRealUpFull *)input)->fields.unmodeled_0c0[1]) : "memory");
    __asm__ __volatile__("sq $0, 0(%0)"
                         : : "r"(&((NmlProRealUpFull *)input)->fields.unmodeled_0c0[2]) : "memory");
    __asm__ __volatile__("lq $2, 0(%1)\n\tsq $2, 0(%0)"
                         : : "r"(&((NmlProRealUpFull *)input)->fields.unmodeled_0c0[3]),
                             "r"(&((NmlProRealUp *)s_inProReal)->fields.unmodeled_170[1])
                         : "$2", "memory");
    __asm__ __volatile__("lq $2, 0(%1)\n\tsq $2, 0(%0)"
                         : : "r"(&((NmlProRealUpFull *)input)->fields.unmodeled_080[0]),
                             "r"(&((NmlProRealUp *)s_inProReal)->fields.unmodeled_160)
                         : "$2", "memory");
    input->fields.unmodeled_11c = 0;
    s_inLayout.slots[0x250 / 4].i |= 0x800;
    *(NmlProRealUp *)s_inProReal = *input;
}

int nmlModelSetTexProreal(void) {
    int renderStatus = s_inLayout.fields.render_status | 0x800;
    s_inLayout.fields.render_status = renderStatus;
    return renderStatus;
}

int nmlModelSetAxisSymmetry(float symmetry) {
    int flags;

    s_inLayout.slots[0x230 / 4].f = symmetry * 0.0f;
    flags = s_inLayout.slots[0x230 / 4].i | 1;
    s_inLayout.slots[0x230 / 4].i = flags;
    return flags;
}

void nmlModelSetParent(int parent)
{
    s_nParent = parent;
}

void nmlModelSetRenderStatus(int status)
{
    if (status == 0x100) {
        s_inLayout.fields.render_status |= 0x100;
    } else if (status == 0x200) {
        s_inLayout.fields.render_status |= 0x200;
    }
}

void nmlModelSetShapeId(int shapeId, float weight)
{
    extern int s_nShapeNum;
    extern int s_aShapeId[];
    extern float s_aShapeWeight[];

    if (s_nShapeNum < 32) {
        s_aShapeId[s_nShapeNum] = shapeId;
        s_aShapeWeight[s_nShapeNum++] = weight;
    }
}

void nmlModelSetToumeiParts(int partId, float alpha)
{
    extern int s_nToumeiNum;
    extern int s_aToumeiId[];
    extern float s_aToumei[];

    if (s_nToumeiNum < 16) {
        s_aToumeiId[s_nToumeiNum] = partId;
        s_aToumei[s_nToumeiNum++] = alpha;
    }
}

void nmlModelSetFilter(int flags, float a, float b)
{
    s_inLayout.slots[0x234 / 4].i = flags;
    if (a < 0.0f) {
        a = 0.0f;
    }
    if (a > 128.0f) {
        a = 128.0f;
    }
    s_inLayout.slots[0x1e0 / 4].f = a;
    s_inLayout.slots[0x1e4 / 4].f = b;
    if (flags & 2) {
        s_nUseStealth = 1;
        if (*(unsigned short *)((char *)&GameLoopState + 0xC) == 2) {
            if (SCRIPT_getCfTime() < 2) {
                s_nUseStealth = 0;
            }
        }
    }
}

/* ACT_modelDrawSub supplies alpha as a signed word from its actor at +0x9a8. */
void nmlModelSetPixelAlpha(int alpha)
{
    if (alpha <= 0) {
        alpha = 1;
    }
    if (alpha >= 128) {
        alpha = 127;
    }
    s_inLayout.fields.pixelAlpha = alpha;
}

void nmlModelSetPartsPixelAlpha(int id, int alpha)
{
    LayoutSlot *s = s_inLayout.slots;
    if (0 >= alpha)
        alpha = 1;
    if (alpha >= 0x80)
        alpha = 0x7F;
    if (s[0x2b4 / 4].i < 4) {
        *(short *)&s[s[0x2b4 / 4].i + 0x2d0 / 4] = alpha;
        *((short *)&s[s[0x2b4 / 4].i + 0x2d0 / 4] + 1) = id;
        s[0x2b4 / 4].i++;
    }
}

extern Vector4 s_inScale;

void nmlModelSetScale(Vector4 *scale)
{
    __asm__ __volatile__(
        "lq $2, 0(%0)\n"
        "sq $2, 0(%1)\n"
        :
        : "r"(scale), "r"(&s_inScale)
        : "$2", "memory");
    if (s_inScale.x < s_inScale.y) s_inScale.x = s_inScale.y;
    if (s_inScale.x < s_inScale.z) s_inScale.x = s_inScale.z;
    if (s_inScale.x < 1.0f) s_inScale.x = 1.0f;
    s_inLayout.slots[0x22c / 4].f = s_inScale.x;
}

void nmlModelSetClip(int clip)
{
    s_nClip = clip;
}

void nmlModelSetAlpha(u64 alpha)
{
    s_inLayout.fields.alpha = alpha;
}

#define NML_RENDER_STENCIL 0x4u
#define NML_RENDER_ZWRITE 0x8u
#define NML_RENDER_TOUMEI 0x20u

void nmlModelSetStencil(int enabled)
{
    if (enabled) {
        s_inLayout.fields.render_status |= NML_RENDER_STENCIL;
    }
}

void nmlModelSetZwrite(int enabled)
{
    if (enabled) {
        s_inLayout.fields.render_status |= NML_RENDER_ZWRITE;
    }
}

void nmlModelSetTransparency(float transparency)
{
    if (transparency < 0.0f) {
        transparency = 0.0f;
    }
    if (1.0f < transparency) {
        transparency = 1.0f;
    }
    s_inLayout.slots[0x220 / 4].f = transparency;
    s_inLayout.slots[0x2c0 / 4].f = transparency;
}

void nmlModelSetReflTransparency(float transparency)
{
    if (transparency < 0.0f) {
        transparency = 0.0f;
    }
    if (1.0f < transparency) {
        transparency = 1.0f;
    }
    s_inLayout.slots[0x224 / 4].f = transparency;
}

void nmlModelSetToumei(int enabled)
{
    if (enabled) {
        s_inLayout.fields.render_status |= NML_RENDER_TOUMEI;
    }
}

static inline void _CopyMatrix(void *s, void *d)
{
    __asm__ __volatile__(
        "lq $2, 0x0(%0)\n"
        "sq $2, 0x0(%1)\n"
        "lq $2, 0x10(%0)\n"
        "sq $2, 0x10(%1)\n"
        "lq $2, 0x20(%0)\n"
        "sq $2, 0x20(%1)\n"
        "lq $2, 0x30(%0)\n"
        "sq $2, 0x30(%1)\n"
        : : "r"(s), "r"(d) : "$2", "memory");
}

void nmlModelSetLight(void *normal, void *color)
{
    _CopyMatrix(normal, &s_inLayout.slots[0]);
    _CopyMatrix(color, &s_inLayout.slots[0x40 / 4]);
    s_inLayout.slots[0x250 / 4].i |= 0x10;
}

/*
 * Bounds and alignment a model texture pointer must satisfy: the pointer
 * lands inside main RAM at or above the executable's own load base
 * (config/tu/main/tu000.json "start": "0x00200000") and at or below the top
 * of the 32 MiB EE RAM, and it is qword (16-byte) aligned as GIF/DMA
 * texture transfers require.
 */
#define NML_TEXTURE_RAM_BASE 0x00200000u
#define NML_TEXTURE_RAM_MAX_OFFSET 0x01dfffffu
#define NML_TEXTURE_ALIGN_MASK 0xfu

void nmlModelSetTexture(const char *texture)
{
    /* Accept only a non-null, in-range, qword-aligned pointer whose header
     * carries the "XTX" texture-resource signature: texture[0..2] ==
     * 'X','T','X' (texture[2] is compared against texture[0], not a literal,
     * per the original disassembly). */
    if (texture == 0 ||
        (u32)texture - NML_TEXTURE_RAM_BASE > NML_TEXTURE_RAM_MAX_OFFSET ||
        ((u32)texture & NML_TEXTURE_ALIGN_MASK) != 0 ||
        texture[0] != 'X' || texture[1] != 'T' || texture[2] != texture[0]) {
        return;
    }
    s_inLayout.fields.texture = texture;
}

void nmlModelSetTexfunc(int texfunc)
{
    s_inLayout.fields.texfunc = texfunc;
}

/*
 * Stores the current model matrix pointer in the pointer-width s_inLayout
 * word at byte offset 0x23c (slot 143 of the shared LayoutStore view, which
 * gives that slot no pointer member).
 */
void nmlModelSetMatrix(void *matrix)
{
    *(void **)&s_inLayout.slots[0x23c / 4] = matrix;
}

void xglMatrixInverse(void *dst, void *src);

void nmlModelSetPlace(void *m)
{
    char *p = (char *)&s_inLayout + 0x80;
    __asm__ __volatile__(
        "lq $2, 0x0(%1)\n"
        "sq $2, 0x0(%0)\n"
        "lq $2, 0x10(%1)\n"
        "sq $2, 0x10(%0)\n"
        "lq $2, 0x20(%1)\n"
        "sq $2, 0x20(%0)\n"
        "lq $2, 0x30(%1)\n"
        "sq $2, 0x30(%0)\n"
        : : "r"(p), "r"(m) : "$2", "memory");
    xglMatrixInverse(p + 0x40, m);
    s_inLayout.slots[0x250 / 4].i |= 1;
}

typedef struct { char s[32]; } TexName32;

typedef union { TexName32 n; float f; } TexAlias;

typedef struct { float u, v; } TexOfs;

extern TexOfs s_aTexOffset;

void nmlModelSetTexOffset(TexAlias *name, float u, float v)
{
    *(TexName32 *)s_aTexName = name->n;
    s_aTexName[31] = 0;
    s_aTexOffset.u = u;
    s_aTexOffset.v = v;
}

typedef struct { char s[32]; } MatName32;

typedef int MulColor16 __attribute__((mode(TI)));

extern MulColor16 s_inMulColor;

void nmlModelSetMatOffset(MatName32 *name, MulColor16 *color)
{
    *(MatName32 *)s_aMatName = *name;
    s_aMatName[31] = 0;
    __asm__ __volatile__("lq $2, 0(%0)\n\tsq $2, 0(%1)" :: "r"(color), "r"(&s_inMulColor) : "$2", "memory");
}

void nmlModelSetMulColor(const float *color)
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        :
        : "r"(&s_inLayout.slots[0x200 / 4]), "r"(color)
        : "$2", "memory");
}

void nmlModelSetFogCol(const float *color)
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        :
        : "r"(&s_inLayout.slots[0x1c0 / 4]), "r"(color)
        : "$2", "memory");
    s_inLayout.fields.render_status |= 0x2;
}

static void nmlModelFogPara(LayoutSlot *fog, float fogNear, float fogFar, float fogMin, float fogMax) {
    float temp_f5;
    float prod;
    float var_f2;
    float var_f4;

    temp_f5 = fogFar - fogNear;
    prod = fogFar * fogNear;
    var_f2 = (1.0f - fogMax) * 255.0f;
    var_f4 = (1.0f - fogMin) * 255.0f;
    if (var_f2 < 0.0f) {
        var_f2 = 0.0f;
    }
    if (var_f2 > 255.0f) {
        var_f2 = 255.0f;
    }
    if (var_f4 < 0.0f) {
        var_f4 = 0.0f;
    }
    if (var_f4 > 255.0f) {
        var_f4 = 255.0f;
    }
    fog->f = var_f2;
    (*(float *)((char *)(fog) + 0x4)) = var_f4;
    (*(float *)((char *)(fog) + 0xC)) = (float) ((prod * (var_f4 - var_f2)) / temp_f5);
    (*(float *)((char *)(fog) + 0x8)) = (float) (((((var_f2 - var_f4) * (fogFar + fogNear)) / temp_f5) + (var_f2 + var_f4)) * 0.5f);
}

/* nmlModelFogPara (above) is still INCLUDE_ASM; declare it so its caller
 * does not see an implicit declaration. */
static void nmlModelFogPara(LayoutSlot *fog, float fogNear, float fogFar, float fogMin, float fogMax);

#define NML_RENDER_FOG 0x2u

/* The four fog distances of RgFog.dist (defaults 50, 250, 0, 1). */
void nmlModelSetFogDist(float fogNear, float fogFar, float fogMin, float fogMax)
{
    nmlModelFogPara(&s_inLayout.slots[0x1d0 / 4], fogNear, fogFar, fogMin, fogMax);
    s_inLayout.fields.render_status |= NML_RENDER_FOG;
}

void nmlModelSetGlobalFogCol(const float *color)
{
    extern Vector4 s_inGblFogCol;
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        :
        : "r"(&s_inGblFogCol), "r"(color)
        : "$2", "memory");
    s_inLayout.fields.render_status |= 0x1000000;
}

void nmlModelSetGlobalFogDist(float fogNear, float fogFar, float fogMin, float fogMax)
{
    extern Vector4 s_inGblFogPara;
    nmlModelFogPara((LayoutSlot *)&s_inGblFogPara, fogNear, fogFar, fogMin, fogMax);
    s_inLayout.fields.render_status |= 0x1000000;
}

int nmlModelGetFogPara(Vector4 *vector) {
    extern Vector4 s_inGblFogPara;
    int result;
    Vector4 *fogParams;
    void *camera;
    Vector4 dest;
    float fogVal;

    result = 0xFF;
    fogParams = &s_inGblFogPara;
    if (s_inLayout.fields.render_status & 2) {
        fogParams = (Vector4 *)&s_inLayout.slots[0x1d0 / 4];
    }
    camera = xglStudioSelectGetActiveCamera(s_inLayout.slots[0x27c / 4].i);
    if (camera != 0) {
        _CurSetMatrix((float(*)[4])((char *)camera + 0x470));
        _CurRotTransPersFog(&dest, vector, fogParams);
        fogVal = dest.w;
        if (fogVal >= 2147483648.0f) {
            result = (int)(fogVal - 2147483648.0f);
            result |= 0x80000000;
        } else {
            result = (int)fogVal;
        }
    }
    return result;
}

int nmlModelGetFogParaStudio(Vector4 *vector, int studioIndex) {
    extern Vector4 s_inGblFogPara;
    int result;
    Vector4 *fogParams;
    void *camera;
    Vector4 dest;
    float fogVal;

    result = 0xFF;
    fogParams = &s_inGblFogPara;
    if (s_inLayout.fields.render_status & 2) {
        fogParams = (Vector4 *)&s_inLayout.slots[0x1d0 / 4];
    }
    camera = xglStudioSelectGetActiveCamera(studioIndex);
    if (camera != 0) {
        _CurSetMatrix((float(*)[4])((char *)camera + 0x470));
        _CurRotTransPersFog(&dest, vector, fogParams);
        fogVal = dest.w;
        if (fogVal >= 2147483648.0f) {
            result = (int)(fogVal - 2147483648.0f);
            result |= 0x80000000;
        } else {
            result = (int)fogVal;
        }
    }
    return result;
}

int nmlModelSetGlobalFogReset(void) {
    int renderStatus = s_inLayout.fields.render_status & 0xfeffffff;
    s_inLayout.fields.render_status = renderStatus;
    return renderStatus;
}

int nmlModelSetFogCancel(void) {
    int renderStatus = s_inLayout.fields.render_status | 0x2000;
    s_inLayout.fields.render_status = renderStatus;
    return renderStatus;
}

void nmlModelInitPartsVisible(NmlModel *model, int visible) {
    int i;
    int *offs;
    int *base;

    if (nmlModelLexDataCheck(model) == 0) {
        base = model->blockOffsets;
        i = 0;
        if (model->blockCount > 0) {
            offs = base;
            do {
                NmlModelBlock *block = (NmlModelBlock *)((char *)model + *offs);
                offs++;
                if (visible) {
                    block->flags = block->flags & ~8;
                } else {
                    block->flags = block->flags | 8;
                }
                i++;
            } while (i < model->blockCount);
        }
    }
}

void nmlModelSetMapEntry(void)
{
    int *mapHandle = (int *)s_inMapHandle;

    s_inLayout.fields.render_status |= NML_RENDER_MAP_ENTRY;
    mapHandle[0] = 1;
}

void nmlModelSetShadowMapEntry(void) {
    s_inLayout.fields.render_status |= 0x10000;
}

/* The map callers pass halfword part IDs; the layout has eight shadow slots. */
void nmlModelSetMapShadowParts(int partId)
{
    if (s_nMapShadowParts < 8) {
        s_inLayout.fields.mapShadowParts[s_nMapShadowParts++] = partId;
    }
}

void nmlModelSetMapLastEntry(void *map, int blockId)
{
    extern void *s_pMapLast;
    extern int s_aMapLast[];

    if (s_nMapLast < 12) {
        s_pMapLast = map;
        s_aMapLast[s_nMapLast++] = blockId;
    }
}

void nmlModelSetMapLastInit(void)
{
    s_nMapLast = 0;
}

extern char s_aDispVisible[];

void nmlModelSetPartsVisible(NmlModel *model, int part, int visible)
{
    int *offs;
    NmlModelBlock *block;
    u32 set_flags;
    u32 keep_flags;
    int i;

    if (nmlModelLexDataCheck(model) == 0 && part < *(int *)((char *)model + 0x40)) {
        offs = model->blockOffsets;
        set_flags = 8;
        if (((NmlModelBlock *)((char *)model + *offs))->materialFlags & 0x10) {
            keep_flags = ~0u;
            if (visible) {
                set_flags = 0;
                keep_flags = ~8u;
            }
            i = 0;
            if (model->blockCount > 0) {
                do {
                    block = (NmlModelBlock *)((char *)model + offs[i]);
                    if (*(int *)((char *)block + 0x2c) == part) {
                        block->flags = (block->flags | set_flags) & keep_flags;
                    }
                    i++;
                } while (i < model->blockCount);
            }
        } else if (visible == 0 && s_nDispVisible < 0x3ff) {
            s_nDispVisible++;
            s_aDispVisible[s_nDispVisible] = part;
        }
    }
}

extern unsigned int strlen(const char *string);
extern int memcmp(const void *left, const void *right, unsigned int count);
extern int strcmp(const char *left, const char *right);
extern char *strstr(const char *string, const char *pattern);

/* nmlModelLexDataCheck remains assembly-owned below. */
int nmlModelLexDataCheck(NmlModel *model);

#define NML_NAME_MATCH_PREFIX 1
#define NML_NAME_MATCH_EXACT 2
#define NML_NAME_MATCH_SUBSTRING 3

void nmlModelSetNameVisible(NmlModel *model, const char *name, int visible, int match)
{
    int *block_offsets;
    NmlModelBlock *first_block;
    NmlModelBlock *block;
    u32 set_flags;
    u32 keep_flags;
    int block_index;

    if (nmlModelLexDataCheck(model) != 0) {
        return;
    }

    block_offsets = model->blockOffsets;
    first_block = (NmlModelBlock *)((char *)model + block_offsets[0]);
    if (!(first_block->materialFlags & NML_BLOCK_MATERIAL_NAMED_PARTS)) {
        return;
    }

    set_flags = NML_BLOCK_HIDDEN;
    keep_flags = ~0u;
    if (visible) {
        set_flags = 0;
        keep_flags = ~NML_BLOCK_HIDDEN;
    }

    switch (match) {
    case NML_NAME_MATCH_PREFIX:
        for (block_index = 0; block_index < model->blockCount; block_index++) {
            block = (NmlModelBlock *)((char *)model + block_offsets[block_index]);
            if (memcmp(block->name, name, strlen(name)) == 0) {
                block->flags = (block->flags | set_flags) & keep_flags;
            }
        }
        break;
    case NML_NAME_MATCH_EXACT:
        for (block_index = 0; block_index < model->blockCount; block_index++) {
            block = (NmlModelBlock *)((char *)model + block_offsets[block_index]);
            if (strcmp(block->name, name) == 0) {
                block->flags = (block->flags | set_flags) & keep_flags;
            }
        }
        break;
    case NML_NAME_MATCH_SUBSTRING:
        for (block_index = 0; block_index < model->blockCount; block_index++) {
            block = (NmlModelBlock *)((char *)model + block_offsets[block_index]);
            if (strstr(block->name, name) != 0) {
                block->flags = (block->flags | set_flags) & keep_flags;
            }
        }
        break;
    }
}

typedef struct {
    int data;
    short attr;
    short type;
} DirectEntry;
extern DirectEntry s_aDirect[];
extern int nmlPacketSetAttributeData16N(void *, int);

void nmlModelDirectSend(int type, void *data, int attr) {
    DirectEntry *p;

    if (s_nPacketSignal == 0 && s_nDirect < 5500) {
        p = &s_aDirect[s_nDirect];
        p->attr = attr;
        p->type = 1;
        switch (type) {
        case 1: case 3: case 5: case 7: case 9: case 11:
            p->data = nmlPacketSetAttributeData16N(data, attr);
            break;
        case 2: case 4: case 6: case 8: case 10: case 12:
            p->data = (int)data;
            break;
        }
        switch (type) {
        case 1: case 2: p->type = 1; break;
        case 3: case 4: p->type = 2; break;
        case 5: case 6: p->type = 3; break;
        case 7: case 8: p->type = 4; break;
        case 9: case 10: p->type = 5; break;
        case 11: case 12: p->type = 6; break;
        }
        s_nDirect++;
    }
}

extern char TestEnv_0_004A9170[];
typedef struct {
    int unk0;
    int unk4;
    int count;
    int entryOff;
} XtxHeader;
typedef union {
    u64 q;
    u32 w[2];
} XtxQ;
typedef struct {
    u32 key;
    u32 hi;
    int a8;
    int size;
    int dataOff;
} XtxEntry;

static void nmlModelDirectXtxSub(void *data, int modelId, int nextModelId)
{
    XtxHeader *hdr = data;
    XtxEntry *p;
    char *base;
    int i = 0;
    u32 lo;
    u32 hi;
    XtxQ qv;

    p =(XtxEntry *)((char *)data + hdr->entryOff);
    if (hdr->count > 0) {
        base = TestEnv_0_004A9170;
        do {
            lo = p->key & 0xFFFF;
            hi = p->key >> 16;
            if (p->key == lo) {
                hi = (int)(lo + 0x3F) >> 6;
            }
            qv.q = ((u64)(((u32)p->a8 >> 6) + 0x3800) << 32) | ((u64)hi << 48);
            *(XtxQ *)(base + 0x20) = qv;
            i++;
            *(u64 *)(base + 0x40) =
                lo | ((u64)p->hi << 32);
            nmlModelDirectSend(modelId, base, 6);
            nmlModelDirectSend(nextModelId, (char *)data + p->dataOff, p->size);
            p++;
        } while (i < hdr->count);
    }
}

/* nmlModelDirectXtxSub (above) is still INCLUDE_ASM; declare it so this
 * caller does not see an implicit declaration. */
void nmlModelDirectXtxSub(void *data, int modelId, int nextModelId);

/*
 * Callers (GameRadarDraw main 0x00252fc8, unit6003_draw main 0x003211a0;
 * both still INCLUDE_ASM) never inspect the result -- the tail call below
 * matches the compiled form, so this is void.
 */
void nmlModelDirectSendXtx(int modelId, void *data)
{
    const signed char *bytes = data;

    if (s_nPacketSignal == 0 &&
        (u32)((u32)data - NML_TEXTURE_RAM_BASE) <= NML_TEXTURE_RAM_MAX_OFFSET &&
        ((u32)data & 0xF) == 0 &&
        bytes[0] == 'X' && bytes[1] == 'T' && bytes[2] == bytes[0]) {
        nmlModelDirectXtxSub(data, modelId, modelId + 1);
    }
}

extern int s_nUseZwrite;

static void set_group_status(LayoutStore *layout, u32 *status)
{
    *status = 0;
    if (layout->fields.render_status & NML_RENDER_FOG) {
        *status = 0x4;
    }
    if (layout->fields.render_status & NML_RENDER_STENCIL) {
        *status |= 0x2;
    }
    if (layout->fields.render_status & 0x4000) {
        *status |= 0x1;
    }
    if (layout->fields.render_status & 0x800) {
        *status |= 0x8;
    }
    if (layout->fields.render_status & 0x10000) {
        *status |= 0x10;
    }
    if (layout->fields.render_status & NML_RENDER_TOUMEI) {
        *status |= 0x20;
    }
    if (layout->fields.render_status & 0x20000) {
        *status |= 0x40;
    }
    if (layout->fields.render_status & 0x40000) {
        *status |= 0x80;
    }
    if (layout->fields.render_status & 0x200000) {
        *status |= 0x100;
    }
    if (layout->fields.render_status & 0x400000) {
        *status |= 0x200;
    }
    if (layout->fields.render_status & 0x800000) {
        *status |= 0x800;
    }
    if (layout->fields.render_status & NML_RENDER_ZWRITE) {
        *status |= 0x400;
        s_nUseZwrite = 1;
    }
}

extern float D_00957944[];

extern float s_fSortOffset;

static void AlphaGroupSortEntry(Vector4 *pos)
{
    char *camera = xglStudioSelectGetActiveCamera(s_inLayout.slots[0x27c / 4].i);
    float dist = 0.0f;
    if (camera != 0) {
        dist = _VectorLengthSQ((Vector4 *)(camera + 0xd0), pos);
    }
    D_00957944[s_nAlphaGroup * 4] = dist + s_fSortOffset;
}

typedef struct {
    u16 block;
    u16 kind;
    LayoutStore *sort;
    u32 status;
    int pad;
} AlphaGroupEntry;
extern AlphaGroupEntry s_aAlphaGroup[];
extern LayoutStore *s_apBlockLast[];
extern u16 s_aBlockLayoutLast[];
extern LayoutStore *s_apModelLayout[];
extern LayoutStore *s_apBlock[];
extern u16 s_aBlockLayout[];

static void AlphaGroupLastEntry(void)
{
    int i;

    for (i = 0; i < s_nBlocksAlphaLast; i++) {
        if (s_nBlocksAlpha + s_nBlocks < 0xFFF) {
            if (s_nAlphaGroup < 0x1FF) {
                int *pn = &s_nBlocksAlpha;
                /* union so pointer store aliases shorts -> scheduled before sh */
                ((union { u16 s; LayoutStore *p; } *)&s_apBlock[0xFFF - s_nBlocksAlpha])->p = s_apBlockLast[i];
                s_aBlockLayout[0xFFF - *pn] = s_aBlockLayoutLast[i];
                s_aAlphaGroup[s_nAlphaGroup].block = 0xFFF - *pn;
                s_aAlphaGroup[s_nAlphaGroup].kind = 1;
                /* union so increment gets alias-set 0 -> before jal; sort fills delay */
                ((union { int i; u16 s; } *)pn)->i = *pn + 1;
                s_aAlphaGroup[s_nAlphaGroup].sort = 0;
                set_group_status(s_apModelLayout[s_aBlockLayoutLast[i]], &s_aAlphaGroup[s_nAlphaGroup].status);
                s_nAlphaGroup++;
            }
        }
    }
}

/*
 * Handwritten function (original is assembly): bubble sort of
 * s_aAlphaGroup[0..s_nAlphaGroup) by the float at offset 4 of each 16-byte
 * entry, descending; swaps whole quadwords. The argument is unused.
 */
static void AlphaGroupSort(int *unused)
{
    __asm__ __volatile__(
        ".set push\n\t"
        ".set noreorder\n\t"
        "lw $8,s_nAlphaGroup\n\t"
        "slti $2,$8,2\n\t"
        "bnez $2,3f\n\t"
        "lui $7,%%hi(s_aAlphaGroup)\n\t"
        "addiu $7,$7,%%lo(s_aAlphaGroup)\n"
        "1:\n\t"
        "daddu $5,$0,$0\n\t"
        "daddu $4,$8,$0\n\t"
        "addi $4,$4,-1\n\t"
        "daddu $6,$7,$0\n"
        "2:\n\t"
        "lwc1 $f1,4($6)\n\t"
        "lwc1 $f0,20($6)\n\t"
        "c.le.s $f0,$f1\n\t"
        "nop\n\t"
        "bc1t 4f\n\t"
        "nop\n\t"
        "lq $2,0($6)\n\t"
        "lq $3,16($6)\n\t"
        "sq $3,0($6)\n\t"
        "sq $2,16($6)\n\t"
        "addi $5,$5,1\n"
        "4:\n\t"
        "addi $6,$6,16\n\t"
        "addi $4,$4,-1\n\t"
        "bne $0,$4,2b\n\t"
        "nop\n\t"
        "bne $0,$5,1b\n\t"
        "nop\n"
        "3:\n\t"
        ".set pop"
        :
        :
        : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$f0", "$f1", "memory");
}

static int is_shadow_map_parts(void *m, int id)
{
    int n = *(int *)((char *)m + 0x278);
    int ret = 0;
    int i;
    if (n != 0) {
        for (i = 0; i < n; i++) {
            if (((unsigned short *)((char *)m + 0x2E0))[i] == id) {
                ret = 1;
                break;
            }
        }
    }
    return ret;
}

extern int s_aToumeiId[];

static int is_parts_transparency(void *p)
{
    int ret = 0;
    int i;
    for (i = 0; i < s_nToumeiNum; i++) {
        if (s_aToumeiId[i] == *(int *)((char *)p + 0x2C)) {
            ret = 1;
            break;
        }
    }
    return ret;
}

extern int D_0095DB70[];
extern int D_0095DBB0[];
extern int D_0095DBF0[];
extern int D_0095DC30[];
typedef int qword_t __attribute__((mode(TI)));
extern qword_t s_inGblPointP[];
extern int s_inGblPointC[];
void xglVectorScaleXYZ(float scale, Vector4 *destination, const Vector4 *source);
extern int s_aShapeId[32];
extern float s_aShapeWeight[32];
extern int s_aToumeiId[];
extern float s_aToumei[];
extern Vector4 s_inScale;
void xglMatrixInverse(void *dst, void *src);
extern int s_pMapLast;
extern int s_aMapLast[];
static int is_block_last_entry(void *m)
{
    int i;
    int ret = 0;
    for (i = 0; i < s_nMapLast; i++) {
        if (*(int *)((char *)m + 0x2C) == s_aMapLast[i]) {
            ret = 1;
            break;
        }
    }
    return ret;
}

typedef struct {
    char *model;
    int pad[3];
} ParentBufEntry;

extern ParentBufEntry s_aParentBuf[];

ParentBufEntry *parent_buf_entry(void)
{
    ParentBufEntry *e = 0;

    if (s_nParentBuf < 0x200) {
        int n;

        e = &s_aParentBuf[s_nParentBuf];
        *(int *)e = 0;
        n = s_nParentBuf;
        *(int *)((char *)&s_aParentBuf[n] + 8) = 0;
        s_nParentBuf = n + 1;
        *(int *)((char *)&s_aParentBuf[n] + 12) = 0;
    }
    return e;
}

static ParentBufEntry *parent_buf_search(int key)
{
    int i;
    ParentBufEntry *result = 0;
    for (i = s_nParentBuf - 1; i >= 0; i--) {
        ParentBufEntry *e = &s_aParentBuf[i];
        if (e->model != 0 && *(int *)(e->model + 0x254) == key && key != 0) {
            result = e;
            break;
        }
    }
    return result;
}

/*
 * D_00956940 is the .bss circle-shadow ratio table: set_circle_shadow_ratio
 * below only ever writes indices 0-7 and 16-23 (two 8-entry blocks 0x40
 * bytes apart); indices 8-15 are written elsewhere (CONSTRUCT_CIRCLR_SHADOW/
 * nmlModelRenderCircle/nmlModelRenderDropCircle, all still INCLUDE_ASM in
 * this TU), so it stays an unsized array rather than a guessed full extent.
 */
extern int D_00956940[];

static void set_circle_shadow_ratio(int scale)
{
    float ratio = (float)scale * 0.0625f;
    int i;

    for (i = 0; i < 16; i++) {
        int index = (i < 8) ? i : i + 8;

        D_00956940[index] = (int)(128.0f - (float)i * ratio) << 24;
    }
}

extern const char D_004D2890[];

int nmlModelLexDataCheck(NmlModel *model)
{
    int ret = 1;
    int entry;
    int *rp;
    char *base = (char *)model;
    char *block;
    char *name;

    if ((u32)(base + 0xFFE00000) > 0x1DFFFFF) {
        goto done;
    }
    if ((u32)model & 0xF) {
        goto done;
    }
    if ((*(u64 *)base & 0xFFFFFF) != 0x78656C) {
        goto done;
    }
    if (*(signed char *)(base + 0x3F) != 0 && *(int *)(base + 0x6C) == 0) {
        entry = *(int *)(base + (model->blockCount << 2) + 0xAC);
        block = base + entry;
        name = block + *(int *)(block + 0x24);
        if (*(int *)(base + 0xA0) != 0) {
            name += model->blockCount * 0x10;
        }
        if (strstr(name + *(int *)(block + 0x28), D_004D2890) == 0) {
            goto done;
        }
    }
    if (model->blockCount > 0) *(rp = &ret) = 0;
done:
    return ret;
}

extern int s_nCount;

extern int s_aShapeMathPara[10];
extern float D_004D88E0;

static void nmlModelLexBlockDoit(NmlModel *model)
{
    int blockIndex;
    int *blockOffsets;

    blockIndex = 0;
    if (model->blockCount > 0) {
      blockOffsets = model->blockOffsets;
      do {
        NmlModelBlock *block;
        block = (NmlModelBlock *)((char *)model + blockOffsets[blockIndex]);

        if (s_nPause == 0 &&
            (block->translationStepX != 0.0f || block->translationStepY != 0.0f)) {
            float wrapX = 0.5f;
            float wrapY = 1.0f;
            block->translationX += block->translationStepX;
            block->translationY += block->translationStepY;

            if (block->translationX >= wrapX) {
                block->translationX -= wrapX;
            } else if (block->translationX <= -wrapX) {
                block->translationX += wrapX;
            }
            if (block->translationY >= wrapY) {
                block->translationY -= wrapY;
            } else if (block->translationY <= -wrapY) {
                block->translationY += wrapY;
            }
        }

        if (s_aMatName[0] != '\0') {
            if (strcmp(s_aMatName, block->materialName) == 0) {
                __asm__ __volatile__(
                    "lq $2, 0(%1)\n\t"
                    "sq $2, 0(%0)"
                    :
                    : "r"(&block->multiplyColor), "r"(&s_inMulColor)
                    : "$2", "memory"
                );
            }
        } else {
            __asm__ __volatile__(
                "lq $2, 0(%1)\n\t"
                "sq $2, 0(%0)"
                :
                : "r"(&block->multiplyColor),
                  "r"((char *)&s_inLayout + 0x200)
                : "$2", "memory"
            );
        }

        if ((u32)(block->shapeFlags - 2) < 2) {
            int partId = block->partId;
            int index;
            int divisor;
            int phase;

            for (index = 0; index < s_nShapeNum; ++index) {
                if (s_aShapeId[index] == partId) {
                    partId = -1;
                    break;
                }
            }

            if (partId >= 0 && s_nShapeNum < 32) {
                divisor = s_aShapeMathPara[(int)block->shapeMathParameter];
                phase = s_nCount & (divisor - 1);
                switch (block->shapeMode) {
                case 0: {
                    int halfDivisor = divisor >> 1;
                    float *weightEntry;

                    s_aShapeId[s_nShapeNum] = partId;
                    weightEntry = &s_aShapeWeight[s_nShapeNum];
                    *weightEntry = (float)(phase >= halfDivisor ?
                        divisor - phase : phase) / (float)halfDivisor *
                        (float)block->shapeWeight;
                    break;
                }
                case 1: {
                    float phaseFraction = (float)phase / (float)divisor;

                    s_aShapeId[s_nShapeNum] = partId;
                    s_aShapeWeight[s_nShapeNum] =
                        phaseFraction * (float)block->shapeWeight;
                    break;
                }
                }
                if ((float)block->shapeWeight <= s_aShapeWeight[s_nShapeNum]) {
                    s_aShapeWeight[s_nShapeNum] =
                        (float)block->shapeWeight - D_004D88E0;
                }
                ++s_nShapeNum;
            }
        }
      } while (++blockIndex < model->blockCount);
    }
}

extern float tanf(float);
extern float D_004D88E4, D_004D88E8, D_004D88EC;
extern void nmlFilterStealthMake(int index, float scale, char *camera, LayoutSlot *layout);
typedef struct {
    char pad0[0xC];
    float z;
    char pad10[0x10];
    float alpha;
} StealthEnt;

static void nmlModelStealthEntry(char *model, char *camera)
{
    int screen[4];
    float scale;
    int i;
    int swapped;
    LayoutSlot *s;

    _CurSetMatrix((float (*)[4])(camera + 0x470));
    _CurSetViewScaleTrans((const float *)(camera + 0x80), (const float *)(camera + 0x70));
    _CurRotTransPersClip((Vector4 *)screen, (Vector4 *)((char *)&s_inLayout + 0x1f0));
    scale = ((float)screen[2] * D_004D88E4) / tanf(*(float *)(camera + 0x94) * 0.5f);
    if (*(int *)(model + 0x48) != 0) {
        for (i = 0; i < 10; i++) {
            nmlFilterStealthMake(i, scale, camera, s_inLayout.slots);
            s_inLayout.slots[0x2c8 / 4].i++;
        }
        swapped = 1;
        while (swapped) {
            StealthEnt **p;
            swapped = 0;
            p = (StealthEnt **)s_inLayout.slots;
            for (i = 0xa1; i < 0xaa; i++) {
                StealthEnt *a = p[i - 1];
                if (a->alpha > D_004D88E8) {
                    StealthEnt *b = p[i];
                    if (a->z + D_004D88EC < b->z) {
                        p[i - 1] = b;
                        swapped = 1;
                        p[i] = a;
                    }
                }
            }
        }
    } else {
        s = s_inLayout.slots;
        nmlFilterStealthMake(0, scale, camera, s);
        s[0x2c8 / 4].i++;
    }
}

extern float D_004D88F0;

static void nmlModelCalcSample(void *arg0, void **arg1)
{
    char *model = (char *)arg0;
    Vector4 sp;
    float fx;
    float var_f2;

    if (*(int *)(model + 0x48) != 0) {
        _WeightToGlobalVec((Vector4 *)&s_inLayout.slots[0x1F0 / 4],
                           (float (*)[4])(model + *(int *)(model + 0x58)),
                           (float (*)[4])(model + *(int *)(model + 0x5C)));
        if (s_inLayout.slots[0x230 / 4].i & 1) {
            s_inLayout.slots[0x1F4 / 4].f = s_inLayout.slots[0x230 / 4].f - s_inLayout.slots[0x1F4 / 4].f;
        }
        __asm__ __volatile__("lq $2, 0(%1)\n\tsq $2, 0(%0)" : : "r"(&s_inLayout.slots[0x210 / 4]), "r"(&s_inLayout.slots[0x1F0 / 4]) : "$2", "memory");
        if (arg1 != 0) {
            __asm__ __volatile__("lq $2, 0(%1)\n\tsq $2, 0(%0)" : : "r"(&s_inLayout.slots[0x210 / 4]), "r"((char *)*arg1 + 0x210) : "$2", "memory");
        }
    } else {
        __asm__ __volatile__(
            "lqc2 vf3, 0(%1)\n\t"
            "lqc2 vf2, 0(%0)\n\t"
            "vadd.xyz vf2, vf2, vf3\n\t"
            "sqc2 vf2, 0(%0)"
            :
            : "r"(&s_inLayout.slots[0x1F0 / 4]), "r"(model + 0x90)
            : "memory");
    }
    _CurSetMatrix((float (*)[4])&s_inLayout.slots[0x80 / 4]);
    _CurApplyMatrix((Vector4 *)&s_inLayout.slots[0x1F0 / 4], (Vector4 *)&s_inLayout.slots[0x1F0 / 4]);
    s_inLayout.slots[0x1FC / 4].f = *(float *)(model + 0x9C);
    s_inLayout.slots[0x2C4 / 4].f = *(float *)(model + 0x9C);
    if (arg1 != 0) {
        s_inLayout.slots[0x2C4 / 4].f = *(float *)(*(char **)((char *)*arg1 + 0x254) + 0x9C);
    }
    if (strstr(model + 0x11, "boss_030") != 0) {
        if (*(int *)(model + 0x48) >= 0x1E) {
            int ofs = 0x7C0;
            _WeightToGlobalVec(&sp, (float (*)[4])(model + *(int *)(model + 0x58) + ofs),
                               (float (*)[4])(model + *(int *)(model + 0x5C) + ofs));
            _CurSetMatrix((float (*)[4])&s_inLayout.slots[0x80 / 4]);
            _CurApplyMatrix(&sp, &sp);
            if (sp.y < 0.0f) {
                s_inLayout.slots[0x2C4 / 4].f = *(float *)(model + 0x9C) * 0.25f;
            }
        }
    }
    if (s_inLayout.slots[0x1FC / 4].f < D_004D88F0) {
        s_inLayout.slots[0x1FC / 4].f = 1.0f;
    }
    fx = s_inLayout.slots[0x80 / 4].f * s_inLayout.slots[0x80 / 4].f
         + s_inLayout.slots[0x84 / 4].f * s_inLayout.slots[0x84 / 4].f
         + s_inLayout.slots[0x88 / 4].f * s_inLayout.slots[0x88 / 4].f;
    var_f2 = __builtin_sqrtf(fx);
    if (var_f2 < 1.0f) {
        var_f2 = 1.0f;
    }
    s_inLayout.slots[0x1FC / 4].f *= s_inLayout.slots[0x22C / 4].f * var_f2;
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelCalcEntryClip);

extern float tanf(float);

static float deapth_for_studio(int studio, const Vector4 *vector, float size)
{
    float result;
    float ret;
    char *camera;
    float (*mat)[4];
    Vector4 *new_var;
    int screen[4];

    camera = xglStudioSelectGetActiveCamera(studio);
    new_var = (Vector4 *)screen;
    result = 0.0f;
    __asm__ __volatile__("" : "+f"(result));
    mat = (float (*)[4])(camera + 0x470);
    __asm__ __volatile__("" : : "r"(mat));
    ret = result;
    if (camera != 0) {
        _CurSetMatrix(mat);
        _CurSetViewScaleTrans((const float *)(camera + 0x80), (const float *)(camera + 0x70));
        _CurRotTransPersClip(new_var, vector);
        __asm__ __volatile__("" : : "f"(result));
        ret = ((float)screen[2] * size) / tanf(*(float *)(camera + 0x94) * 0.5f);
    }
    return ret;
}

extern const char D_004DC470[];
extern float D_004D88F8;
extern volatile float D_004D88FC;
extern float D_004D8900;
extern volatile float D_004D8904;
extern volatile float D_004D8908;
extern float D_004D890C;
extern float D_004D8910;
extern float D_004D8914;
extern volatile float D_004D8918;

static void nmlModelCalcSpecularClip(void *arg0, int arg1)
{
    float f4;
    float f3;
    float size;
    float f1;
    float *p;
    float temp_f22;
    float temp_f21;
    float temp_f2;
    float temp_f3;
    int i;

    size = s_inLayout.slots[0x7F].f;
    if (strstr((char *)arg0 + 0x10, D_004DC470) == 0 && s_nParent != 0) {
        size = *(float *)((char *)s_nParent + 0x9C);
    }
    if (arg1 != 0) {
        size = *(float *)((char *)s_nParent + 0x9C);
    }
    f4 = deapth_for_studio(s_inLayout.slots[0x9F].i, (Vector4 *)&s_inLayout.slots[0x7C], size);
    if (!(s_inLayout.slots[0xAC].i & 2)) {
        f3 = __builtin_fabsf(f4);
        if (!(s_inLayout.slots[0xAC].i & 0x20)) {
            if (f3 < D_004D88F8) {
                s_inLayout.slots[0xAC].i |= 2;
            } else if (f3 <= D_004D88FC) {
                s_inLayout.slots[0x8A].f = (s_inLayout.slots[0x8A].f * (f3 - D_004D88F8)) / D_004D8900;
            }
            if (*(float *)((char *)arg0 + 0x9C) > 10.0f) {
                s_inLayout.slots[0x8A].f = 1.0f;
                s_inLayout.slots[0xAC].i &= ~2;
            }
        }
        temp_f2 = D_004D8904;
        if (f3 < temp_f2) {
            s_inLayout.slots[0x9B].f = 0.0f;
        } else if (f3 <= D_004D8908) {
            s_inLayout.slots[0x9B].f = (s_inLayout.slots[0x9B].f * (f3 - temp_f2)) / temp_f2;
        }
        if (s_inLayout.slots[0x89].f <= 0.0f) {
            s_inLayout.slots[0xAC].i |= 0x10;
        }
        if (s_inLayout.slots[0xAC].i & 0x10) {
            s_inLayout.slots[0x89].f = 0.0f;
        }
        if (s_inLayout.slots[0xAC].i & 0x102) {
            s_inLayout.slots[0x8A].f = 0.0f;
            s_inLayout.slots[0xAC].i |= 2;
        }
        s_inLayout.slots[0xC0 + s_inLayout.slots[0x9F].i].f = s_inLayout.slots[0x8A].f;
        temp_f22 = D_004D890C;
        temp_f21 = D_004D8910;
        for (i = 0; i < 4; i++) {
            if (s_inLayout.slots[0x9F].i != i && g_aSubWindow[i] != 0) {
                f4 = deapth_for_studio(i, (Vector4 *)&s_inLayout.slots[0x7C], size);
                f1 = __builtin_fabsf(f4);
                if (f1 < D_004D8914) {
                    s_inLayout.slots[0xC0 + i].i = 0;
                } else {
                    if (f1 <= temp_f22) {
                        s_inLayout.slots[0xC0 + i].f = (s_inLayout.slots[0xC0 + i].f * (f1 - D_004D8914)) / temp_f21;
                    }
                }
            }
        }
    } else {
        s_inLayout.slots[0xC0].f = 0.0f;
        s_inLayout.slots[0xC1].f = 0.0f;
        s_inLayout.slots[0xC2].f = 0.0f;
        s_inLayout.slots[0xC3].f = 0.0f;
        s_inLayout.slots[0x8A].f = 0.0f;
    }
    if ((s_inLayout.slots[0x94].i & 0x100040) != 0) {
        temp_f3 = __builtin_fabsf(f4);
        if (temp_f3 < D_004D8918) {
            s_inLayout.slots[0xAC].i |= 0x800;
        }
    }
}

#define LAYOUT_W(off) (s_inLayout.slots[(off) / 4].f)
#define LAYOUT_S32(off) (s_inLayout.slots[(off) / 4].i)

static void nmlModelCalcDropShadow(void)
{
    xglMatrixUnit((float (*)[4])&s_inLayout.slots[0x180 / 4].f);

    {
        Vector4 shadow;

        shadow.x = LAYOUT_W(0x40);
        shadow.y = 1.5f;
        shadow.z = LAYOUT_W(0x60);

        if (s_nShadowVec != 0) {
            shadow.x = s_inShadowVec.x;
            shadow.z = s_inShadowVec.z;
        }

        if (shadow.y != 0.0f) {
            LAYOUT_W(0x190) = -shadow.x / shadow.y;
            LAYOUT_W(0x198) = -shadow.z / shadow.y;
        } else {
            LAYOUT_W(0x190) = 0.0f;
            LAYOUT_W(0x198) = 0.0f;
        }

        LAYOUT_W(0x194) = 0.0f;
        LAYOUT_W(0x1b4) = LAYOUT_W(0xb4) + 0.005f;
        LAYOUT_W(0x1b0) = -LAYOUT_W(0xb4) * LAYOUT_W(0x190);
        LAYOUT_W(0x1b8) = -LAYOUT_W(0xb4) * LAYOUT_W(0x198);

        if (LAYOUT_S32(0x2a8) != 0) {
            LAYOUT_W(0x1b4) = LAYOUT_W(0x2ac) + 0.005f;
            LAYOUT_W(0x1b0) = -LAYOUT_W(0x2ac) * LAYOUT_W(0x190);
            LAYOUT_W(0x1b8) = -LAYOUT_W(0x2ac) * LAYOUT_W(0x198);
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelCalcEntryProjectCircle);

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelCalcEntryProjectMap);

extern float D_004D8940;
extern Vector4 D_70000010;
extern LayoutStore s_inLayout;
int nmlModelCalcClip(int model);
int nmlModelCalcClipNoCulling(int model);
void _ApplyMatrix(Vector4 *destination, const void *matrix, const Vector4 *vector);

static void nmlModelCalcEntryPartsClip(char *arg0, char *arg1)
{
  int pad[4];
  Vector4 *v;
  int i;
  int j;
  int n;
  int k;
  int new_var;
  int m;
  int flag;
  int t2;
  unsigned char hit;
  int cnt;
  char *e;
  unsigned char *q;
  Vector4 sp10;
  int *vp;
  LayoutStore *lay;
  float (*mtx)[4];
  vp = (int *) (arg0 + 0xB0);
  pad[3] = 0xFF;
  pad[2] = 0xFF;
  pad[1] = 0xFF;
  pad[0] = 0xFF;
  if ((*((int *) (arg1 + 0x2B8))) == 0)
  {
    return;
  }
  i = 0;
  flag = *((int *) (arg0 + 0x48));
  if (flag > 0)
  {
    lay = &s_inLayout;
    mtx = (float (*)[4]) (arg1 + 0x80);
    v = (Vector4 *) 0x70000000;
    do
    {
      n = (*((int *) (arg0 + 0x40))) * 2;
      k = n + i;
      m = (n + flag) + i;
      i += 1;
      _WeightToGlobalVec(v, (float (*)[4]) ((k << 6) + (*((int *) (arg1 + 0x258)))), (float (*)[4]) ((m << 6) + (*((int *) (arg1 + 0x258)))));
      if (lay->slots[0x8C].i & 1)
      {
        v->y = lay->slots[0x8C].f - v->y;
      }
      _ApplyMatrix(v, mtx, v);
      v++;
    }
    while (i < (flag = *((int *) (arg0 + 0x48))));
  }
  j = 0;
  if ((*((int *) (arg0 + 0x44))) > 0)
  {
    do
    {
      e = arg0 + (*((int *) (((char *) vp) + (j * 4))));
      if (((*((int *) (e + 0x20))) & 1) == 0)
      {
        _ApplyMatrix(&sp10, (float (*)[4]) (((*((int *) (arg1 + 0x258))) + ((*((unsigned short *) (e + 0x30))) << 6)) - 0x40), (Vector4 *) (e + 0x90));
        _ApplyMatrix(&sp10, (float (*)[4]) (arg1 + 0x80), &sp10);
        *((unsigned char *) ((*((int *) (arg1 + 0x2B8))) + j)) = nmlModelCalcClip((int) (&sp10));
      }
      else
      {
        cnt = 0;
        hit = 0;
        if ((*((unsigned short *) (e + 0x32))) != 0)
        {
          do
          {
            float *p;
            flag = cnt * 2;
            q = (unsigned char *) (flag + ((int) e));
            t2 = (*((int *) (arg0 + 0x40))) * 2;
            p = (float *) ((((*((unsigned short *) (q + 0x32))) - t2) << 4) + 0x6FFFFFF0);
            ((union 
            {
              typeof(p[3]) i;
              float f;
            } *) (&p[3]))->i = (*((float *) (e + 0x9C))) * D_004D8940;
            if (nmlModelCalcClipNoCulling((int) p))
            {
              hit = (unsigned char) (hit + 1);
            }
            cnt += 1;
            new_var = cnt * 2;
            flag = new_var;
            q = (unsigned char *) (flag + ((int) e));
          }
          while ((*((unsigned short *) (q + 0x32))) != 0);
        }
        if ((hit != 0) && (hit == cnt))
        {
          *((unsigned char *) ((*((int *) (arg1 + 0x2B8))) + j)) = 1;
        }
      }
      j++;
    }
    while (j < (*((int *) (arg0 + 0x44))));
  }
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelEntry);

ACCEPTED_ASM("src/main/nml_model_set", nmlModelEntryEffect);

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelEntryCard);

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelRenderProreal);

extern float D_004D8948;
void nmlPacketAddGifTagStandard(int, int);
void nmlPacketAddExecProg();

static void *nmlModelRenderTexture(void *arg0, int arg1, int arg2)
{
    int var_22;
    LayoutStore *var_19;
    int var_18;
    int sp4;
    short temp_3;
    int temp_16;
    LayoutStore *temp_16_2;
    LayoutStore *temp_17;
    void *camera;
    u16 w;
    u16 h;

    var_22 = 0;
    temp_16 = *(unsigned short *)((char *)&sRender + 0x1A);
    camera = xglStudioSelectGetActiveCamera(arg2);
    if (camera == 0) {
        goto done;
    }
    while (var_19 = 0, !(*(float *)(*(int *)((char *)arg0 + 0xC) + 0x26C) <= D_004D8948)) {
        sp4 = temp_16 & 0xFFFF;
        nmlPacketGsInit();
        nmlPacketAddGsZbuf(1);
        nmlPacketAddGsFBA(0);
        nmlPacketAddGsFrame(sp4, 0);
        nmlPacketAddGsFlush();
        nmlFilterSetTexClear(sp4);
        nmlPacketGsInit();
        nmlPacketAddGsPixeltest(0x31001);
        nmlPacketAddGsScissor((((u64)0xDF0000 << 0x10) | 0x1FF) << 0x10);
        nmlPacketAddGsFlush();
        nmlPacketAddGifTagStandard(0, 0);
        if (s_nAnotherStudio != 0) {
            nmlPacketAddScreen(camera);
        }
        temp_3 = *(short *)arg0;
        w = *(unsigned short *)arg0;
        h = *(unsigned short *)((char *)arg0 + 2);
        var_18 = temp_3 - arg1;
        if (var_18 != ((*(short *)((char *)arg0 + 2) * arg1) + temp_3) - arg1) {
            unsigned int mask = 0xFFFF7FFF;
            do {
                var_18 += arg1;
                temp_17 = s_apBlock[var_18];
                temp_16_2 = s_apModelLayout[s_aBlockLayout[var_18]];
                if (!(temp_16_2->fields.render_status & 0x100000)) {
                    temp_16_2->fields.render_status |= 0x8000;
                    if (var_19 == 0) {
                        var_19 = temp_16_2;
                    }
                    nmlPacketAddTransMicrocode(temp_17, temp_16_2);
                    var_22 = nmlPacketAddParts(temp_17->fields.unmodeled_000, temp_16_2, camera);
                    if (var_22 != 0) {
                        goto block_21;
                    }
                    nmlPacketAddExecProg(temp_17, temp_16_2, 0, 0);
                    temp_16_2->fields.render_status &= mask;
                }
                w = *(unsigned short *)arg0;
                h = *(unsigned short *)((char *)arg0 + 2);
            } while (var_18 != (((short)w + ((short)h * arg1)) - arg1));
        }
        if (var_22 == 0) {
            if (var_19 != 0) {
                nmlFilterSetTexFillDraw(sp4, var_19, *(unsigned short *)((char *)arg0 + 2), *(unsigned short *)arg0);
            }
            nmlPacketGsInit();
            nmlPacketAddGsScissor((((u64)0x01BF0000 << 0x10) | 0x1FF) << 0x10);
            nmlPacketAddGsFlush();
            if (var_19 != 0) {
                var_22 = nmlModelRenderProreal(var_19, arg2);
            }
        }
        break;
    }
block_21:
    return (void *)var_22;
done:
    ;
}

extern float D_004D894C;
extern int nmlModelRenderProreal(void *, int);
extern void nmlPacketSendCircleTexture(void *texture, void *clut);
extern unsigned char s_aCircleShadow[];
extern unsigned char s_aCircleShadowClut[];

static int nmlModelRenderCircle(void *arg0, int arg1, int arg2)
{
    LayoutStore *var_16;
    int var_6;
    int var_9;

    var_16 = 0;
    var_9 = 0;
    if (!(*(float *)(*(int *)((char *)arg0 + 0xC) + 0x26C) <= D_004D894C)) {
        var_6 = *(short *)arg0 - arg1;
        goto test;
loop:
        var_6 += arg1;
        var_16 = s_apModelLayout[s_aBlockLayout[var_6]];
        if (!(var_16->fields.render_status & 0x100000)) {
            goto done;
        }
        var_16 = 0;
test:
        if (var_6 != ((int)(short)*(unsigned short *)arg0 + (int)*(short *)((char *)arg0 + 2) * arg1) - arg1) {
            goto loop;
        }
done:
        if (var_16 != 0) {
            set_circle_shadow_ratio((int)((float)var_16->slots[0x270 / 4].i * var_16->slots[0x2C0 / 4].f));
            nmlPacketSendCircleTexture(s_aCircleShadow, s_aCircleShadowClut);
            var_9 = nmlModelRenderProreal(var_16, arg2);
        }
    }
    return var_9;
}

extern void nmlPacketMakeCircleTexture(void *modelLayout, void *arg);

static int nmlModelRenderDropCircle(short *arg0, int arg1, void *arg2)
{
    LayoutStore *layout;
    int i;

    layout = 0;
    do {
        for (i = arg0[0] - arg1; i != arg0[0] + arg0[1] * arg1 - arg1; ) {
            i += arg1;
            layout = s_apModelLayout[s_aBlockLayout[i]];
            if (!(layout->fields.render_status & 0x100000)) {
                break;
            }
        }
    } while (0);

    set_circle_shadow_ratio((int)((float)layout->slots[0x270 / 4].i * layout->slots[0x2C0 / 4].f));
    nmlPacketSendCircleTexture(s_aCircleShadow, s_aCircleShadowClut);
    nmlPacketMakeCircleTexture(layout, arg2);
    return 0;
}

#include "nml_packet_add.h"
typedef struct { short w; short h; } DropRect;
void nmlFilterSetFrameAlphaClear(void);
void nmlPacketGsInit(void);
void nmlPacketAddGsFBA(int);
void nmlPacketAddGsZbuf(unsigned long long mask);
void nmlPacketAddGsAlpha(unsigned long long a);
void nmlPacketAddGsPixeltest(unsigned int test);
void nmlPacketAddGifTagStandard(int, int);
void nmlPacketAddGsFlush(void);
void nmlPacketAddScreen(StudioCamera *);
void nmlPacketAddBlockMaterial(NmlMaterialRenderState *, NmlModelRenderState *, int, int);
int nmlPacketAddParts(u8 *, NmlModelRenderState *, StudioCamera *);

static int nmlModelRenderDrop(DropRect *rect, int step, StudioCamera *camera, int viewport)
{
    int result;
    int index;

    result = 0;
    nmlFilterSetFrameAlphaClear();
    nmlPacketGsInit();
    nmlPacketAddGsFBA(1);
    nmlPacketAddGsZbuf(1);
    nmlPacketAddGsAlpha(0x89);
    nmlPacketAddGsPixeltest(0x54000);
    nmlPacketAddGifTagStandard(0, 1);
    nmlPacketAddGsFlush();
    if (s_nAnotherStudio != 0) {
        nmlPacketAddScreen(camera);
    }
    do {
        for (index = rect->w - step; index != rect->w + rect->h * step - step; ) {
            NmlMaterialRenderState *material;
            NmlModelRenderState *model;
            int status;

            index += step;
            material = s_apBlock[index];
            model = s_apModelLayout[s_aBlockLayout[index]];
            status = model->render_status;
            if (!(status & 0x100000)) {
                model->render_status = status | 0x80000;
                nmlPacketAddTransMicrocode(material, model);
                nmlPacketAddBlockMaterial(material, model, 0, viewport);
                result = nmlPacketAddParts((u8 *)material, model, camera);
                if (result != 0) {
                    break;
                }
                nmlPacketAddExecProg(material, model, 0, 0);
                ((union { u32 u; short s[2]; } *)&model->render_status)->u &= 0xFFF7FFFF;
            }
        }
    } while (0);
    return result;
}

typedef struct {
    s16 block;
    s16 kind;
    LayoutStore *sort;
    u32 status;
    int pad;
} NonAlphaGroupEntry;
extern NonAlphaGroupEntry s_aNonAlphaGroup[];
typedef struct {
    unsigned char u00[0x20];
    unsigned short fbp;
    unsigned char u22[0x26];
    int f48;
} NonAlphaRS;
extern int *s_nStencilVal;
extern const char D_004DC478[];
void nmlPacketClrModelCache(void);
extern void nmlPacketAddGsFrame(unsigned short fbp, unsigned long long mask);
void nmlPacketAddGsFogCol(float *color);
void nmlPacketAddGifTag(NmlMaterialRenderState *material, NmlModelRenderState *model, int flag);
int nmlPacketAddTexture(NmlModelRenderState *model);
void nmlPacketAddTextureParam(NmlMaterialRenderState *material, NmlModelRenderState *model);
void nmlPacketAddPixelControl(NmlMaterialRenderState *material, NmlModelRenderState *model, int viewport);
void nmlPacketAddReflectParam(NmlMaterialRenderState *material, NmlModelRenderState *model);

static int nmlModelFlushSubNonAlpha(int viewport, StudioCamera *camera)
{
  int result;
  int i;
  int tag;
  int j;
  int window;
  LayoutStore *model;
  LayoutStore *material;
  NmlMaterialRenderState *mat;
  NmlModelRenderState *mdl;
  result = 0;
  do
  {
    nmlPacketClrModelCache();
    i = result;
    nmlPacketGsInit();
    nmlPacketAddGsPixeltest(0x50000);
    nmlPacketAddGsZbuf(0);
  }
  while (0);
  nmlPacketAddGsFlush();
  i = result;
  if (i < s_nNonAlphaGroup)
  {
    do
    {
      tag = -1;
      if (s_aNonAlphaGroup[i].status & 0x48)
      {
        goto next;
      }
      nmlPacketAddGsFrame(((NonAlphaRS *) (&sRender))->fbp, (s_aNonAlphaGroup[i].status & 2) ? (0) : ((int) s_nStencilVal));
      nmlPacketAddGsFBA(s_aNonAlphaGroup[i].status & 2);
      model = s_apModelLayout[s_aBlockLayout[s_aNonAlphaGroup[i].block]];
      window = g_aSubWindow[viewport];
      if (window == 1)
      {
        if (viewport != model->slots[0x9F].i)
        {
          goto next;
        }
      }
      else
        if (window == 2)
      {
        if (model->slots[0x9F].i != 0)
        {
          goto next;
        }
      }
      nmlPacketAddFog((NmlModelRenderState *) model, viewport);
      nmlPacketAddGsFogCol(&model->slots[0x70].f);
      nmlPacketAddGsAlpha((((unsigned long long) ((int) (model->slots[0xC0 + viewport].f * 128.0f))) << 32) | 0x68);
      nmlPacketAddGsFlush();
      for (j = s_aNonAlphaGroup[i].block; j < (s_aNonAlphaGroup[i].block + s_aNonAlphaGroup[i].kind); j++)
      {
        material = s_apBlock[j];
        model = s_apModelLayout[s_aBlockLayout[j]];
        mat = (NmlMaterialRenderState *) material;
        mdl = (NmlModelRenderState *) model;
        if (s_nAnotherStudio != 0)
        {
          if (model->slots[0xC4 + viewport].i != 0)
          {
            continue;
          }
          if ((((unsigned char *) model->slots[0xAE].i)[viewport] >> viewport) & 1)
          {
            continue;
          }
        }
        if (tag != ((material->slots[0x30].i & 1) | (model->slots[0x94].i & 0x01004002)))
        {
          tag = (material->slots[0x30].i & 1) | (model->slots[0x94].i & 0x01004002);
          nmlPacketAddGifTag(mat, mdl, 0);
        }
        nmlPacketAddTransMicrocode(mat, mdl);
        result = nmlPacketAddTexture(mdl);
        if (result != 0)
        {
          break;
        }
        nmlPacketAddTextureParam(mat, mdl);
        nmlPacketAddPixelControl(mat, mdl, viewport);
        nmlPacketAddGsFlush();
        if (material->slots[0x30].i & 0x1000)
        {
          if ((!(model->slots[0xAC].i & 0x10)) || ((model->slots[0x94].i & 0x100000) && (strstr((char *) material, D_004DC478) != 0)))
          {
            nmlPacketAddReflRot(mat, mdl);
            nmlPacketAddReflectParam(mat, mdl);
          }
        }
        nmlPacketAddBlockMaterial(mat, mdl, 0, viewport);
        result = nmlPacketAddParts((u8 *) material, mdl, camera);
        if (result != 0)
        {
          break;
        }
        nmlPacketAddExecProg(mat, mdl, 0, 0);
      }

      next:
      i++;

    }
    while (i < s_nNonAlphaGroup);
  }
  return result;
}

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelFlushSubAlpha);

INCLUDE_ASM("asm/main/nonmatchings/nml_model_set", nmlModelFlushSub);

/*
 * CONSTRUCT_CIRCLR_SHADOW (near the top of this TU) is still INCLUDE_ASM
 * and nmlModelClear is defined below (this TU); declare them, and the
 * s_inMapHandle array nmlModelClear also declares nearer its own use, so
 * this caller does not see an implicit declaration.
 */
static void CONSTRUCT_CIRCLR_SHADOW(void);
void nmlModelClear(void);
extern unsigned char s_inMapHandle[];

void nmlModelConstruct(void)
{
    CONSTRUCT_MODELSYSTEM();
    CONSTRUCT_CIRCLR_SHADOW();
    CONSTRUCT_BACK_BUFFER();
    CONSTRUCT_ALPHA_GROUP();
    CONSTRUCT_PARENT_BUF();
    CONSTRUCT_MAP_HANDLE((int *)s_inMapHandle);
    CONSTRUCT_FADE_CONTROL(&s_inFadeIn);
    CONSTRUCT_FADE_CONTROL(&s_inFadeOut);
    CONSTRUCT_FADE_CONTROL(&s_inActiveFadeIn);
    CONSTRUCT_FADE_CONTROL(&s_inActiveFadeOut);
    nmlModelClear();
    s_inLayout.fields.render_status = 0;
}

void nmlModelInit(void)
{
    INIT_MODELSYSTEM();
    INIT_BACK_BUFFER();
    INIT_ALPHA_GROUP();
    INIT_PARENT_BUF();
    INIT_MAP_HANDLE((int *)s_inMapHandle);
    nmlModelClear();
    s_inLayout.fields.render_status = 0;
    D_0095BB44[0] = 30;
}

/* CLEAR_LAYOUT_MODEL and CLEAR_MODEL_ENTRY (near the top of this TU) are
 * still INCLUDE_ASM; declare them so this caller does not see an implicit
 * declaration. */
static void CLEAR_LAYOUT_MODEL(LayoutStore *);
static void CLEAR_MODEL_ENTRY(void);
extern unsigned char s_inMapHandle[];
extern unsigned char s_inProReal[];

void nmlModelClear(void) {
    CLEAR_LAYOUT_MODEL(&s_inLayout);
    CLEAR_PROREAL((int *) s_inProReal);
    CLEAR_MAP_HANDLE((int *) s_inMapHandle);
    CLEAR_MODEL_ENTRY();
}

extern void AlphaGroupSort(int *);
extern void nmlPacketAddTransMicrocodeInit(void);
extern void nmlPacketGsInit(void);
extern void nmlPacketAddGsZbuf(unsigned long long mask);
extern void nmlPacketAddGsZbuf1(unsigned long long mask);
extern void nmlPacketAddGsFBA(int a);
extern void nmlPacketAddGsFBA1(int a);
extern void nmlPacketAddGsPixeltest(unsigned int test);
extern void nmlPacketAddGsPixeltest1(unsigned int test);
extern void nmlPacketAddGsFrame(unsigned short fbp, unsigned long long mask);
extern void nmlPacketAddGsFrame1(unsigned short fbp, unsigned long long mask);
extern void nmlPacketAddGsAlpha1(unsigned long long a);
extern void nmlPacketAddGsAlpha(unsigned long long a);
extern void nmlPacketAddGsFlush(void);
extern void nmlPacketAddGsTexture(void);
extern void nmlPacketAddGsClamp(unsigned long long mask);
extern void nmlFilterSetFrameToBuffer(unsigned short dbp);
extern void nmlFilterSetBufferRender(float f);
extern void sefDrawEffect(void);
extern void nmlModelFlushClear(void);
extern int nmlModelFlushSub(int arg0);
extern int s_nRenderCancel;
extern int *s_nStencilVal;
extern int g_nPacketOver;
typedef struct {
    unsigned char u00[0x20];
    unsigned short fbp;
    unsigned char u22[0x26];
    int f48;
} NmlModelFlushRS;

void nmlModelFlush(int *arg0)
{
    int *var_4;
    int var_18;
    int var_16;
    int *var_17;
    int mode;
    int var_4_2;
    int *var_5;
    int var_3;
    int temp_2;
    int temp_16;
    int temp_2_2;
    int temp_2_3;
    int temp_2_4;
    float var_f12;
    short var_4_3;


    var_4 = arg0;
    var_18 = 0;
    if (s_nRenderCancel == 0) {
        var_4 = (int *)s_inBackBuffer;
        mode = s_inBackBuffer[0x14 / 4];
        switch (mode) {
        case 1:
            s_nRenderCancel = mode;
            break;
        case 0:
            if (s_inBackBuffer[0x4 / 4] > 0 && s_inBackBuffer[0x20 / 4] == 0) {
                s_nRenderCancel = 1;
            }
            break;
        }
    }
    AlphaGroupSort(var_4);
    AlphaGroupLastEntry();
    nmlPacketAddTransMicrocodeInit();
    nmlPacketGsInit();
    nmlPacketAddGsZbuf(0U);
    nmlPacketAddGsZbuf1(1U);
    nmlPacketAddGsFBA(0);
    nmlPacketAddGsFBA1(0);
    nmlPacketAddGsPixeltest(0x50000U);
    nmlPacketAddGsPixeltest1(0x50000U);
    nmlPacketAddGsFrame(((NmlModelFlushRS *)&sRender)->fbp, 0U);
    nmlPacketAddGsFrame1(((NmlModelFlushRS *)&sRender)->fbp, 0U);
    nmlPacketAddGsAlpha1(((unsigned long long)0x80 << 32) | 0x68);
    nmlPacketAddGsFlush();
    if (s_nPause == 0) {
        s_nCount += 1;
    }
    if (*(int *)((char *)&GameLoopState + 0x10) & 0x01000000) {
        s_nStencilVal = 0;
    } else {
        s_nStencilVal = (int *)0x80000000;
    }
    var_4_2 = s_nAnotherStudio;
    for (var_3 = 0; var_3 < 3; var_3++) {
        var_4_2 = (g_aSubWindow[var_3 + 1] != 0) ? 1 : var_4_2;
    }
    s_nAnotherStudio = var_4_2;
    if (s_nRenderCancel == 0) {
        var_18 = nmlModelFlushSub(0);
    }
    if (var_18 == 0) {
        if (s_nEffectWrite == 1 && s_nRenderCancel == 0) {
            nmlPacketGsInit();
            nmlPacketAddGsZbuf(0U);
            nmlPacketAddGsPixeltest(0x50000U);
            nmlPacketAddGsFrame(((NmlModelFlushRS *)&sRender)->fbp, 0U);
            nmlPacketAddGsFlush();
            sefDrawEffect();
        }
        if (var_18 == 0) {
            if (s_nUseGnosys != 0) {
                nmlFilterSetFrameToBuffer(((NmlModelFlushRS *)&sRender)->fbp);
            }
            if (s_nRenderCancel == 0) {
                for (var_16 = 1; var_16 < 4; var_16++) {
                    temp_2_4 = g_aSubWindow[var_16];
                    if (temp_2_4 != 0) {
                        var_18 = nmlModelFlushSub(var_16);
                        if (var_18 != 0) {
                            break;
                        }
                    }
                }
            }
        }
    }
    temp_2_2 = s_inBackBuffer[0xC / 4];
    if (temp_2_2 != 0) {
        temp_2_3 = temp_2_2 - 1;
        s_inBackBuffer[0xC / 4] = temp_2_3;
        if (temp_2_3 == 0) {
            s_inBackBuffer[0x0 / 4] = 1;
            s_inBackBuffer[0x4 / 4] = s_inBackBuffer[0x8 / 4];
        }
    }
    if (s_inBackBuffer[0x0 / 4] != 0) {
        var_4_3 = (short)((NmlModelFlushRS *)&sRender)->fbp;
        s_inBackBuffer[0x0 / 4] = 0;
        if (s_inBackBuffer[0x10 / 4] >= 0) {
            var_4_3 = *(short *)((char *)s_inBackBuffer + 0x10);
        }
        nmlFilterSetFrameToBuffer(var_4_3 & 0xFFFF);
    }
    temp_16 = s_inBackBuffer[0x14 / 4];
    switch (temp_16) {
    case 0:
        var_f12 = 1.0f;
        if (s_inBackBuffer[0x20 / 4] != 0) {
            var_f12 = (float)s_inBackBuffer[0x4 / 4] / (float)s_inBackBuffer[0x8 / 4];
        }
        if (s_inBackBuffer[0x4 / 4] > 0) {
            s_inBackBuffer[0x4 / 4] = s_inBackBuffer[0x4 / 4] - 1;
            nmlFilterSetBufferRender(var_f12);
            s_inBackBuffer[0x1C / 4] = 1;
        }
        break;
    case 1:
        nmlFilterSetBufferRender(1.0f);
        s_inBackBuffer[0x1C / 4] = temp_16;
        if (s_inBackBuffer[0x18 / 4] > 0) {
            temp_2_4 = s_inBackBuffer[0x18 / 4] - 1;
            s_inBackBuffer[0x18 / 4] = temp_2_4;
            if (temp_2_4 == 0) {
                INIT_BACK_BUFFER();
            }
        }
        if (s_nFrameLockOff != 0) {
            INIT_BACK_BUFFER();
        }
        break;
    }
    nmlModelFadeDoit();
    nmlPacketGsInit();
    nmlPacketAddGsTexture();
    nmlPacketAddGsClamp(0U);
    nmlPacketAddGsZbuf(0U);
    nmlPacketAddGsPixeltest(0x50000U);
    nmlPacketAddGsAlpha((unsigned long long)0x80 << 32 | 0x44);
    nmlPacketAddGsFrame(((NmlModelFlushRS *)&sRender)->fbp, 0U);
    nmlPacketAddGsFlush();
    if (var_18 != 0 || g_nPacketOver != 0) {
        ((NmlModelFlushRS *)&sRender)->f48 = 1;
    }
    g_nPacketOver = 0;
    s_nRenderCancelOld = s_nRenderCancel;
    if (s_nRenderCancel > 0) {
        s_nRenderCancel -= 1;
    } else {
        s_nRenderCancel = 0;
    }
    nmlModelFlushClear();
}

/* FLUSH_MODELSYSTEM (above) is still INCLUDE_ASM; declare it so this
 * caller does not see an implicit declaration. */
static void FLUSH_MODELSYSTEM(void);

void nmlModelFlushClear(void) {
    FLUSH_MODELSYSTEM();
    FLUSH_ALPHA_GROUP();
    FLUSH_MAP_HANDLE((int *) s_inMapHandle);
    FLUSH_PARENT_BUF();
    FLUSH_BACK_BUFFER();
    nmlModelClear();
}

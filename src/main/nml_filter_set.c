#include "common.h"
#include "shared.h"

void nmlModelSetFilterGunosys(int *model, int flags, int count);
void nmlModelSetFilterStealth(int *model, int flags);

/*
 * Loads the supplied four-row matrix into the resident VU0 macro-mode
 * registers vf27-vf30 (ee-vu-cop2), the same resident-register family
 * _CurRotTransPersClip below reads back, matching src/main/nml_model_set.c's
 * own _CurSetMatrix.
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
 * Loads the view-scale vector into VU register vf25 and the view-translation
 * vector into vf26, matching src/main/nml_model_set.c's own
 * _CurSetViewScaleTrans.
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

/*
 * Transforms *vector by the resident matrix in vf27-vf30, perspective-divides
 * it, applies the resident view scale (vf25) and translation (vf26) and stores
 * the fixed-point result at *destination (ee-vu-cop2).  The return value is the
 * VU0 clipping-flag register vi18, cleared on entry and accumulated by the
 * vclipw test, masked to its six near/far X/Y/Z bits.
 *
 * The original object masks the flags in $2 and then hands them to the caller
 * through $4 (daddu a0,v0,zero / jr ra / daddu v0,a0,zero), so the returned
 * value is a second local pinned to that register; the instruction-free barrier
 * below keeps both copies alive across the hand-off, which is the only thing
 * that separates them.
 */
static int _CurRotTransPersClip(Vector4 *destination, const Vector4 *vector)
{
    register int clipFlags asm("$2");
    register int returnedFlags asm("$4");

    __asm__ __volatile__(
        "ctc2 $0,$vi18\n\t"
        "lqc2 $vf31,0(%2)\n\t"
        "vmulax.xyzw ACC,vf27xyzw,vf31x\n\t"
        "vmadday.xyzw ACC,vf28xyzw,vf31y\n\t"
        "vmaddaz.xyzw ACC,vf29xyzw,vf31z\n\t"
        "vmaddw.xyzw vf31xyzw,vf30xyzw,vf0w\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vclipw.xyz vf31xyz,vf31w\n\t"
        "vdiv Q,vf0w,vf31w\n\t"
        "vwaitq\n\t"
        "vmulq.xyzw vf31xyzw,vf31xyzw,Q\n\t"
        "vmulaw.xyzw ACC,vf26xyzw,vf0w\n\t"
        "vmadd.xyzw vf31xyzw,vf31xyzw,vf25xyzw\n\t"
        "vftoi4.xyw vf23xyw,vf31xyw\n\t"
        "vftoi0.z vf23z,vf31z\n\t"
        "vsub.w vf23w,vf23w,vf23w\n\t"
        "sqc2 $vf23,0(%1)\n\t"
        "cfc2 %0,$vi18\n\t"
        "nop"
        : "=r"(clipFlags)
        : "r"(destination), "r"(vector)
        : "memory"
    );
    clipFlags &= 0x3f;
    returnedFlags = clipFlags;
    __asm__ __volatile__("" : : "r"(returnedFlags), "r"(clipFlags));
    return returnedFlags;
}

/* Former anonymous view names (pad0/h/pad/fbp/pad16/zbp) stay available as
 * aliases so saved bodies keep compiling. */
struct NmlFilterRenderView {
    union { u8 unmodeled_00[2]; short pad0; };
    union { short pixelStorageMode; short h; };   /* +0x02 */
    char pad[0];                                  /* +0x04: old byte view */
    short frameWidth;            /* +0x04 */
    short frameHeight;           /* +0x06 */
    u8 unmodeled_08[0x0a];
    u16 opaqueTexturePage;       /* +0x12 */
    union { u16 framebufferPage; unsigned short fbp; };   /* +0x14 */
    union { u8 unmodeled_16[0x0a]; char pad16[0xA]; };
    union { u16 displayBufferBase; unsigned short zbp; }; /* +0x20 */
};
extern struct NmlFilterRenderView sRender;

struct NmlFilterGsRegisterEntry {
    u64 value;
    u64 registerId;
};

struct NmlFilterGsPair {
    union { u64 value; } payload;
    union { u64 value; } registerSlot;
};
typedef int NmlFilterGridVector[4] __attribute__((aligned(16)));
enum {
    NML_GRID_X,
    NML_GRID_Y,
    NML_GRID_U,
    NML_GRID_V
};
struct NmlFilterGridPoint {
    NmlFilterGridVector original;
    NmlFilterGridVector displaced;
};
struct NmlFilterModelView {
    unsigned char unmodeled_000[0x1e0];
    float filterAlpha;
    float filterAmplitude;
    unsigned char unmodeled_1e8[0x234 - 0x1e8];
    u32 filterFlags;
};
float xglSin(float);
float xglCos(float);
void nmlPacketGsInit(void);
void packet_gs_entry64(int, void *);
void nmlPacketAddGsFrame(int, int);
void nmlPacketAddGsFBA(int);
void nmlPacketAddGsFlushWide(void);

static void nmlModelSetFilterGunosys(int *modelArg, int flags, int count)
{
    int entryCount = 0;
    const struct NmlFilterModelView *model = (const struct NmlFilterModelView *)modelArg;
    struct NmlFilterGridPoint grid[4][5];
    struct NmlFilterGsPair *packet = (struct NmlFilterGsPair *)0x70000000;
    struct NmlFilterGsPair *alphaEntry;
    struct NmlFilterGsPair *clampEntry;
    struct NmlFilterGsPair *entry;
    int row;
    int column;
    int remaining;

    packet[0].registerSlot.value = 0x3f;
    packet[0].payload.value = 0;
    entryCount = 1;
    if (model->filterFlags & 4) {
        packet[entryCount].payload.value = (u64)(0x24020000 | (sRender.framebufferPage << 5)) |
            0x2000000640000000ULL;
    } else {
        packet[entryCount].payload.value = (u64)(0x24020000 | (sRender.opaqueTexturePage << 5)) |
            0x2000000640000000ULL;
    }
    packet[entryCount].registerSlot.value = 6;
    ++entryCount;
    alphaEntry = (struct NmlFilterGsPair *)0x70000020;
    entryCount += 2;
    alphaEntry->payload.value = ((u64)(int)model->filterAlpha << 32) | 100;
    alphaEntry->registerSlot.value = 0x42;
    clampEntry = (struct NmlFilterGsPair *)0x70000030;
    clampEntry->registerSlot.value = 8;
    clampEntry->payload.value = 0;
    if (flags) {
        packet[entryCount].payload.value = 0x5f001;
    } else {
        packet[entryCount].payload.value = 0x5d001;
    }
    packet[entryCount++].registerSlot.value = 0x47;
    packet[entryCount].payload.value = 0x3f80000000808080ULL;
    packet[entryCount].registerSlot.value = 1;
    ++entryCount;

    {
        /* Each row has four spans and one closing grid point. */
        for (row = 0; row < 4; ++row) {
            for (column = 0; column < 5; ++column) {
                struct NmlFilterGridPoint *point = &grid[row][column];
                point->original[NML_GRID_X] = ((column * 128) + 1792) << 4;
                point->original[NML_GRID_Y] = ((row * 448 / 3) + 1824) << 4;
                point->original[NML_GRID_U] = (column * 128) << 4;
                point->original[NML_GRID_V] = (row * 448 / 3) << 4;
            }
        }
    }
    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 5; ++column) {
            struct NmlFilterGridPoint *point = &grid[row][column];
            float phase = (float)((count + row * 4 + column) % 16) * 0.0625f;
            float angle = (phase + phase) * 3.1415927f;
            float sine = xglSin(angle);
            int displacementX = (int)(model->filterAmplitude * sine);
            float cosine = xglCos(angle);
            int displacementY = (int)(model->filterAmplitude * cosine);
            displacementX = (column & 1) ? displacementX : -displacementX;
            displacementY = (column & 1) ? displacementY : -displacementY;
            point->displaced[NML_GRID_X] = point->original[NML_GRID_X] + displacementX;
            point->displaced[NML_GRID_U] = point->original[NML_GRID_U];
            point->displaced[NML_GRID_V] = point->original[NML_GRID_V];
            point->displaced[NML_GRID_Y] = point->original[NML_GRID_Y] + displacementY;
        }
    }
    for (row = 0; row < 3; ++row) {
        packet[entryCount].payload.value = 0x154;
        packet[entryCount].registerSlot.value = 0;
        ++entryCount;
        packet[entryCount].payload.value = (u64)grid[row][0].displaced[NML_GRID_U] |
            ((u64)grid[row][0].displaced[NML_GRID_V] << 16);
        packet[entryCount].registerSlot.value = 3;
        ++entryCount;
        packet[entryCount].payload.value = 0x7fffffff00000000ULL | (u64)grid[row][0].displaced[NML_GRID_X] |
            ((u64)grid[row][0].displaced[NML_GRID_Y] << 16);
        packet[entryCount].registerSlot.value = 4;
        ++entryCount;
        packet[entryCount].payload.value = (u64)grid[row + 1][0].displaced[NML_GRID_U] |
            ((u64)grid[row + 1][0].displaced[NML_GRID_V] << 16);
        packet[entryCount].registerSlot.value = 3;
        ++entryCount;
        packet[entryCount].payload.value = (u64)grid[row + 1][0].displaced[NML_GRID_X] |
            0x7fffffff00000000ULL | ((u64)grid[row + 1][0].displaced[NML_GRID_Y] << 16);
        packet[entryCount].registerSlot.value = 4;
        ++entryCount;
        for (column = 1; column < 5; ++column) {
            packet[entryCount].payload.value = (u64)grid[row][column].displaced[NML_GRID_U] |
                ((u64)grid[row][column].displaced[NML_GRID_V] << 16);
            packet[entryCount].registerSlot.value = 3;
            ++entryCount;
            packet[entryCount].payload.value = 0x7fffffff00000000ULL | (u64)grid[row][column].displaced[NML_GRID_X] |
                ((u64)grid[row][column].displaced[NML_GRID_Y] << 16);
            packet[entryCount].registerSlot.value = 4;
            ++entryCount;
            packet[entryCount].payload.value = (u64)grid[row + 1][column].displaced[NML_GRID_U] |
                ((u64)grid[row + 1][column].displaced[NML_GRID_V] << 16);
            packet[entryCount].registerSlot.value = 3;
            ++entryCount;
            packet[entryCount].payload.value = (u64)grid[row + 1][column].displaced[NML_GRID_X] |
                0x7fffffff00000000ULL | ((u64)grid[row + 1][column].displaced[NML_GRID_Y] << 16);
            packet[entryCount].registerSlot.value = 4;
            ++entryCount;
        }
    }
    nmlPacketGsInit();
    nmlPacketAddGsFrame(sRender.displayBufferBase, 0);
    nmlPacketAddGsFBA(0);
    if (entryCount > 0) {
        remaining = entryCount;
        entry = packet;
        do {
            --remaining;
            packet_gs_entry64(entry->registerSlot.value, &entry->payload.value);
            entry++;
        } while (remaining != 0);
    }
    nmlPacketAddGsFlushWide();
}

/* v2 */
/* v3 */

INCLUDE_ASM("asm/main/nonmatchings/nml_filter_set", nmlModelSetFilterStealth);

/*
 * Loads the supplied four-row placement matrix into the resident VU0
 * macro-mode registers vf10-vf13, the second resident matrix family
 * _WeightToGlobalPlaceVec below combines with vf27-30 (_CurSetMatrix).
 */
static void _WeightToGlobalPlaceInit(float matrix[4][4])
{
    __asm__ __volatile__(
        "lqc2 $vf10,0(%0)\n\t"
        "lqc2 $vf11,16(%0)\n\t"
        "lqc2 $vf12,32(%0)\n\t"
        "lqc2 $vf13,48(%0)\n\t"
        "nop"
        :
        : "r"(matrix)
        : "memory"
    );
}

static void _WeightToGlobalPlaceVec(Vector4 *destination, float placeMatrix[4][4], float weightMatrix[4][4])
{
    __asm__ __volatile__(
        "lq $2,0(%2)\n\t"
        "lq $3,16(%2)\n\t"
        "lq $4,32(%2)\n\t"
        "lqc2 $vf3,48(%2)\n\t"
        "qmfc2.ni $5,$vf0\n\t"
        "pextlw $6,$3,$2\n\t"
        "pextuw $7,$3,$2\n\t"
        "pextlw $8,$5,$4\n\t"
        "pextuw $9,$5,$4\n\t"
        "pcpyld $2,$8,$6\n\t"
        "pcpyud $3,$6,$8\n\t"
        "pcpyld $4,$9,$7\n\t"
        "qmtc2.ni $2,$vf5\n\t"
        "qmtc2.ni $3,$vf6\n\t"
        "qmtc2.ni $4,$vf7\n\t"
        "vmulax.xyz ACC,vf5xyz,vf3x\n\t"
        "vmadday.xyz ACC,vf6xyz,vf3y\n\t"
        "vmaddz.xyz vf2xyz,vf7xyz,vf3z\n\t"
        "vmove.w vf2w,vf0w\n\t"
        "lqc2 $vf27,0(%1)\n\t"
        "lqc2 $vf28,16(%1)\n\t"
        "lqc2 $vf29,32(%1)\n\t"
        "lqc2 $vf30,48(%1)\n\t"
        "vsub.xyz vf2xyz,vf0xyz,vf2xyz\n\t"
        "vmulax.xyzw ACC,vf27xyzw,vf2x\n\t"
        "vmadday.xyzw ACC,vf28xyzw,vf2y\n\t"
        "vmaddaz.xyzw ACC,vf29xyzw,vf2z\n\t"
        "vmaddw.xyzw vf31xyzw,vf30xyzw,vf2w\n\t"
        "vmulax.xyzw ACC,vf10xyzw,vf31x\n\t"
        "vmadday.xyzw ACC,vf11xyzw,vf31y\n\t"
        "vmaddaz.xyzw ACC,vf12xyzw,vf31z\n\t"
        "vmaddw.xyzw vf31xyzw,vf13xyzw,vf0w\n\t"
        "sqc2 $vf31,0(%0)\n\t"
        "nop"
        :
        : "r"(destination), "r"(placeMatrix), "r"(weightMatrix)
        : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9", "memory"
    );
}

/*
 * Squared distance between *first and *second: (second - first) dot
 * (second - first), returned as an ordinary float (the "=r" qmfc2 output
 * copied into $f0 by cc1, the same shape as src/main/xgl_2.c's
 * xglPointLength family).
 */
static float _VectorLengthSQ(const Vector4 *first, const Vector4 *second)
{
    register float lengthSq asm("$f0");

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

union NmlStealthXYZ {
    Vector4 vector;
    struct {
        int x;
        int y;
        int z;
        int w;
    };
};
union NmlStealthUV {
    Vector4 vector;
    struct {
        int u;
        int v;
        int clipFlags;
        unsigned char unmodeled_0c[4];
    };
};
struct NmlStealthGeometry {
    Vector4 endpoints[2];
    float sortClass;
    unsigned char unmodeled_24[0x0c];
    union NmlStealthXYZ xyz[14];
    union NmlStealthUV uv[14];
};
struct NmlStealthDescriptor {
    int firstJoint;
    int secondJoint;
    unsigned char unmodeled_08[4];
    int sortClass;
    float firstWidth;
    float secondWidth;
    float firstExtension;
    float secondExtension;
    float longitudinalStretch[4];
};
struct NmlStealthJointData {
    unsigned char unmodeled_00[0x48];
    int jointCount;
    unsigned char unmodeled_4c[12];
    unsigned int jointMatrixOffset;
    unsigned int jointAuxMatrixOffset;
};
union NmlStealthModelView {
    int slots[200];
    struct {
    unsigned char unmodeled_000[0x80];
    Matrix4 modelToWorld;
    unsigned char unmodeled_0c0[0x1f0 - 0xc0];
    Vector4 fallbackSphere;
    unsigned char unmodeled_200[0x254 - 0x200];
    struct NmlStealthJointData *jointData;
    unsigned char unmodeled_258[0x280 - 0x258];
    struct NmlStealthGeometry *stealthParts[10];
    } fields;
};
extern struct NmlStealthDescriptor s_aStealthDat[10];
extern struct NmlStealthDescriptor s_inStealthUnitDat;
extern void *nmlPacketSetAttributeAlloc16N(int records);
extern void xglVectorNormal(Vector4 *destination, const Vector4 *source);
extern void xglVectorScaleXYZ(float scale, Vector4 *destination, const Vector4 *source);
extern float xglAtan2(float x, float y);
#define NML_STEALTH_EDGE(vertex, center, centerY, depth, sign, width) { \
    geometry->xyz[vertex].x = (int)((float)geometry->xyz[center].x sign side.y * (width)); \
    geometry->xyz[vertex].y = (int)((centerY) sign side.x * (width)); \
    geometry->xyz[vertex].z = (depth); \
}
#define NML_STEALTH_EDGE_UV(vertex, center, centerY, sign, width) { \
    geometry->uv[vertex].u = (int)((float)geometry->xyz[center].x sign side.y * (width)); \
    geometry->uv[vertex].v = (int)((centerY) sign side.x * (width)); \
}
#define NML_STEALTH_BAND(vertex, center, centerY, depth, edge) { \
    geometry->xyz[vertex].x = (int)((float)geometry->xyz[center].x * weight + \
        (float)geometry->xyz[edge].x * (1.0f - weight)); \
    geometry->xyz[vertex].y = (int)((centerY) * weight + \
        (float)geometry->xyz[edge].y * (1.0f - weight)); \
    geometry->xyz[vertex].z = (depth); \
}
#define NML_STEALTH_BAND_UV(vertex, center, centerY, edge) { \
    geometry->uv[vertex].u = (int)((float)geometry->xyz[center].x * weight + \
        (float)geometry->xyz[edge].x * (1.0f - weight)); \
    geometry->uv[vertex].v = (int)((centerY) * weight + \
        (float)geometry->xyz[edge].y * (1.0f - weight)); \
}
#define NML_STEALTH_EXTEND_XYZ(vertex, sign, amount) { \
    geometry->xyz[vertex].x += (int)(sign longitudinal.x * (amount)); \
    geometry->xyz[vertex].y += (int)(sign longitudinal.y * (amount)); \
}
#define NML_STEALTH_EXTEND_UV(vertex, sign, amount) { \
    geometry->uv[vertex].u += (int)(sign longitudinal.x * (amount)); \
    geometry->uv[vertex].v += (int)(sign longitudinal.y * (amount)); \
}
#define NML_STEALTH_CLIP_ENDPOINT(numerator, denominator) { \
    float fraction = __builtin_fabsf((float)(numerator) / (float)(denominator)); \
    float complement = 1.0f - fraction; \
    uv[outside].u = (int)(fraction * (float)uv[1 - outside].u + complement * (float)uv[outside].u); \
    uv[outside].v = (int)(fraction * (float)uv[1 - outside].v + complement * (float)uv[outside].v); \
    xyz[outside].x = (int)(fraction * (float)xyz[1 - outside].x + complement * (float)xyz[outside].x); \
    xyz[outside].y = (int)(fraction * (float)xyz[1 - outside].y + complement * (float)xyz[outside].y); \
}

void nmlFilterStealthMake(int partIndex, StudioCamera *camera,
    int *modelArg, float widthScale)
{
    Vector4 direction;
    Vector4 side;
    struct {
        float x;
        float y;
    } longitudinal;
    Vector4 center;
    Vector4 fallback;
    struct {
        int left;
        int right;
        int top;
        int bottom;
    } clipBounds;
    union NmlStealthModelView *model = (union NmlStealthModelView *)modelArg;
    struct NmlStealthJointData *jointData = model->fields.jointData;
    struct NmlStealthGeometry *geometry;
    const struct NmlStealthDescriptor *descriptor;
    Vector4 *firstEndpoint;
    float firstY;
    float secondY;
    float weight;
    int firstDepth;
    int secondDepth;
    int jointCount;
    int index;
    union NmlStealthUV *uv;

    geometry = nmlPacketSetAttributeAlloc16N(31);
    model->fields.stealthParts[partIndex] = geometry;
    firstEndpoint = &geometry->endpoints[0];
    jointCount = model->fields.jointData->jointCount;
    descriptor = &s_aStealthDat[partIndex];
    if (descriptor->firstJoint < jointCount &&
        descriptor->secondJoint < jointCount) {
        _WeightToGlobalPlaceInit(model->fields.modelToWorld);
        _WeightToGlobalPlaceVec(firstEndpoint,
            ((Matrix4 *)((const unsigned char *)jointData +
                jointData->jointMatrixOffset))[descriptor->firstJoint],
            ((Matrix4 *)((const unsigned char *)jointData +
                jointData->jointAuxMatrixOffset))[descriptor->firstJoint]);
        _WeightToGlobalPlaceVec(&geometry->endpoints[1],
            ((Matrix4 *)((const unsigned char *)jointData +
                jointData->jointMatrixOffset))[descriptor->secondJoint],
            ((Matrix4 *)((const unsigned char *)jointData +
                jointData->jointAuxMatrixOffset))[descriptor->secondJoint]);
    } else {
        descriptor = &s_inStealthUnitDat;
        __asm__ __volatile__(
            "lq $2,0(%1)\n\t"
            "sq $2,0(%0)"
            : : "r"(&fallback), "r"(&model->fields.fallbackSphere) : "$2", "memory");
        fallback.y -= fallback.w * 0.3f;
        __asm__ __volatile__(
            "lq $2,0(%1)\n\t"
            "sq $2,0(%0)"
            : : "r"(firstEndpoint), "r"(&fallback) : "$2", "memory");
        fallback.y += fallback.w * 0.6f;
        __asm__ __volatile__(
            "lq $2,0(%1)\n\t"
            "sq $2,0(%0)"
            : : "r"(&geometry->endpoints[1]), "r"(&fallback) : "$2", "memory");
    }

    __asm__ __volatile__(
        "lqc2 $vf3,0(%1)\n\t"
        "lqc2 $vf2,0(%0)\n\t"
        "vadd.xyz vf2xyz,vf2xyz,vf3xyz\n\t"
        "sqc2 $vf2,0(%2)"
        : : "r"(firstEndpoint), "r"(&geometry->endpoints[1]), "r"(&center) : "memory");
    xglVectorScaleXYZ(0.5f, &center, &center);
    geometry->sortClass = (float)descriptor->sortClass;
    firstEndpoint->w = _VectorLengthSQ(&camera->position, &center);
    if (descriptor->firstExtension != 0.0f) {
        __asm__ __volatile__(
            "lqc2 $vf3,0(%0)\n\t"
            "lqc2 $vf2,0(%1)\n\t"
            "vsub.xyz vf2xyz,vf2xyz,vf3xyz\n\t"
            "sqc2 $vf2,0(%2)"
            : : "r"(firstEndpoint), "r"(&geometry->endpoints[1]), "r"(&direction) : "memory");
        xglVectorNormal(&direction, &direction);
        firstEndpoint->x -= direction.x * descriptor->firstExtension;
        firstEndpoint->y -= direction.y * descriptor->firstExtension;
        firstEndpoint->z -= direction.z * descriptor->firstExtension;
    }
    if (descriptor->secondExtension != 0.0f) {
        __asm__ __volatile__(
            "lqc2 $vf3,0(%0)\n\t"
            "lqc2 $vf2,0(%1)\n\t"
            "vsub.xyz vf2xyz,vf2xyz,vf3xyz\n\t"
            "sqc2 $vf2,0(%2)"
            : : "r"(firstEndpoint), "r"(&geometry->endpoints[1]), "r"(&direction) : "memory");
        xglVectorNormal(&direction, &direction);
        geometry->endpoints[1].x += direction.x * descriptor->secondExtension;
        geometry->endpoints[1].y += direction.y * descriptor->secondExtension;
        geometry->endpoints[1].z += direction.z * descriptor->secondExtension;
    }

    _CurSetMatrix(camera->viewMatrix);
    _CurSetViewScaleTrans(&camera->screenScale.x, &camera->screenOffset.x);
    _CurRotTransPersClip(&geometry->xyz[0].vector, firstEndpoint);
    _CurRotTransPersClip(&geometry->xyz[1].vector, &geometry->endpoints[1]);
    direction.x = (float)geometry->xyz[1].x - (float)geometry->xyz[0].x;
    direction.y = (float)geometry->xyz[1].y - (float)geometry->xyz[0].y;
    direction.z = 0.0f;
    xglVectorNormal(&direction, &direction);
    side.z = xglAtan2(direction.x, direction.y);
    side.x = widthScale * xglCos(side.z + 1.5707964f);
    side.y = widthScale * xglSin(side.z + 1.5707964f);
    firstY = (float)geometry->xyz[0].y;
    firstDepth = geometry->xyz[0].z;
    secondDepth = geometry->xyz[1].z;
    secondY = (float)geometry->xyz[1].y;

    NML_STEALTH_EDGE(6, 0, firstY, firstDepth, -, descriptor->firstWidth);
    NML_STEALTH_EDGE(12, 0, firstY, firstDepth, +, descriptor->firstWidth);
    NML_STEALTH_EDGE(7, 1, secondY, secondDepth, -, descriptor->secondWidth);
    NML_STEALTH_EDGE(13, 1, secondY, secondDepth, +, descriptor->secondWidth);
    NML_STEALTH_EDGE_UV(6, 0, firstY, -, descriptor->firstWidth);
    NML_STEALTH_EDGE_UV(12, 0, firstY, +, descriptor->firstWidth);
    NML_STEALTH_EDGE_UV(7, 1, secondY, -, descriptor->secondWidth);
    NML_STEALTH_EDGE_UV(13, 1, secondY, +, descriptor->secondWidth);
    weight = 0.75f;
    NML_STEALTH_BAND(4, 0, firstY, firstDepth, 6);
    NML_STEALTH_BAND(10, 0, firstY, firstDepth, 12);
    NML_STEALTH_BAND(5, 1, secondY, secondDepth, 7);
    NML_STEALTH_BAND(11, 1, secondY, secondDepth, 13);
    weight = 0.9f;
    NML_STEALTH_BAND_UV(4, 0, firstY, 6);
    NML_STEALTH_BAND_UV(10, 0, firstY, 12);
    NML_STEALTH_BAND_UV(5, 1, secondY, 7);
    NML_STEALTH_BAND_UV(11, 1, secondY, 13);
    weight = 0.5f;
    NML_STEALTH_BAND(2, 0, firstY, firstDepth, 6);
    NML_STEALTH_BAND(8, 0, firstY, firstDepth, 12);
    NML_STEALTH_BAND(3, 1, secondY, secondDepth, 7);
    NML_STEALTH_BAND(9, 1, secondY, secondDepth, 13);
    weight = 0.7f;
    NML_STEALTH_BAND_UV(2, 0, firstY, 6);
    NML_STEALTH_BAND_UV(8, 0, firstY, 12);
    NML_STEALTH_BAND_UV(3, 1, secondY, 7);
    NML_STEALTH_BAND_UV(9, 1, secondY, 13);

    longitudinal.x = direction.x * descriptor->firstWidth * widthScale;
    longitudinal.y = direction.y * descriptor->firstWidth * widthScale;
    NML_STEALTH_EXTEND_XYZ(0, -, descriptor->longitudinalStretch[0]);
    NML_STEALTH_EXTEND_XYZ(1, +, descriptor->longitudinalStretch[0]);
    geometry->uv[0].u = geometry->xyz[0].x;
    geometry->uv[0].v = geometry->xyz[0].y;
    geometry->uv[1].u = geometry->xyz[1].x;
    geometry->uv[1].v = geometry->xyz[1].y;
    NML_STEALTH_EXTEND_XYZ(4, -, descriptor->longitudinalStretch[1]);
    NML_STEALTH_EXTEND_XYZ(5, +, descriptor->longitudinalStretch[1]);
    NML_STEALTH_EXTEND_XYZ(10, -, descriptor->longitudinalStretch[1]);
    NML_STEALTH_EXTEND_XYZ(11, +, descriptor->longitudinalStretch[1]);
    NML_STEALTH_EXTEND_UV(4, -, descriptor->longitudinalStretch[1]);
    NML_STEALTH_EXTEND_UV(5, +, descriptor->longitudinalStretch[1]);
    NML_STEALTH_EXTEND_UV(10, -, descriptor->longitudinalStretch[1]);
    NML_STEALTH_EXTEND_UV(11, +, descriptor->longitudinalStretch[1]);
    NML_STEALTH_EXTEND_XYZ(2, -, descriptor->longitudinalStretch[2]);
    NML_STEALTH_EXTEND_XYZ(3, +, descriptor->longitudinalStretch[2]);
    NML_STEALTH_EXTEND_XYZ(8, -, descriptor->longitudinalStretch[2]);
    NML_STEALTH_EXTEND_XYZ(9, +, descriptor->longitudinalStretch[2]);
    NML_STEALTH_EXTEND_UV(2, -, descriptor->longitudinalStretch[2]);
    NML_STEALTH_EXTEND_UV(3, +, descriptor->longitudinalStretch[2]);
    NML_STEALTH_EXTEND_UV(8, -, descriptor->longitudinalStretch[2]);
    NML_STEALTH_EXTEND_UV(9, +, descriptor->longitudinalStretch[2]);
    NML_STEALTH_EXTEND_XYZ(6, -, descriptor->longitudinalStretch[3]);
    NML_STEALTH_EXTEND_XYZ(7, +, descriptor->longitudinalStretch[3]);
    NML_STEALTH_EXTEND_XYZ(12, -, descriptor->longitudinalStretch[3]);
    NML_STEALTH_EXTEND_XYZ(13, +, descriptor->longitudinalStretch[3]);
    NML_STEALTH_EXTEND_UV(6, -, descriptor->longitudinalStretch[3]);
    NML_STEALTH_EXTEND_UV(7, +, descriptor->longitudinalStretch[3]);
    NML_STEALTH_EXTEND_UV(12, -, descriptor->longitudinalStretch[3]);
    NML_STEALTH_EXTEND_UV(13, +, descriptor->longitudinalStretch[3]);

    uv = &geometry->uv[0];
    for (index = 0; index < 14; index++, uv++) {
        uv->u -= (4096 - sRender.frameWidth) * 8;
        uv->v -= (4096 - sRender.frameHeight) * 8;
        uv->clipFlags = 4;
    }
    uv = &geometry->uv[0];
    {
        union NmlStealthXYZ *xyz = &geometry->xyz[0];

        for (index = 0; index < 7; index++, xyz += 2, uv += 2) {
            int outside;

            clipBounds.left = 16;
            clipBounds.right = (sRender.frameWidth - 1) << 4;
            clipBounds.bottom = (sRender.frameHeight - 1) << 4;
            clipBounds.top = 16;
            if (xyz[0].z < 0) {
                uv[0].clipFlags = 12;
            }
            if (xyz[1].z < 0) {
                uv[1].clipFlags = 12;
            }
            if (uv[0].u < clipBounds.left && uv[1].u < clipBounds.left) {
                uv[0].clipFlags = 12;
                uv[1].clipFlags = 12;
            }
            if (uv[0].v < clipBounds.top && uv[1].v < clipBounds.top) {
                uv[0].clipFlags = 12;
                uv[1].clipFlags = 12;
            }
            if (uv[0].u > clipBounds.right && uv[1].u > clipBounds.right) {
                uv[0].clipFlags = 12;
                uv[1].clipFlags = 12;
            }
            if (uv[0].v > clipBounds.bottom && uv[1].v > clipBounds.bottom) {
                uv[0].clipFlags = 12;
                uv[1].clipFlags = 12;
            }

            outside = 0;
            if ((uv[0].u < clipBounds.left && uv[1].u > clipBounds.left) ||
                (outside = (uv[1].u < clipBounds.left &&
                    uv[0].u > clipBounds.left)) != 0) {
                NML_STEALTH_CLIP_ENDPOINT(uv[outside].u,
                    uv[1 - outside].u - uv[outside].u);
            }
            outside = 0;
            if ((uv[0].v < clipBounds.top && uv[1].v > clipBounds.top) ||
                (outside = (uv[1].v < clipBounds.top &&
                    uv[0].v > clipBounds.top)) != 0) {
                NML_STEALTH_CLIP_ENDPOINT(uv[outside].v,
                    uv[1 - outside].v - uv[outside].v);
            }
            outside = 0;
            if ((uv[0].u > clipBounds.right && uv[1].u < clipBounds.right) ||
                (outside = (uv[1].u > clipBounds.right &&
                    uv[0].u < clipBounds.right)) != 0) {
                NML_STEALTH_CLIP_ENDPOINT(uv[outside].u - clipBounds.right,
                    uv[outside].u - uv[1 - outside].u);
            }
            outside = 0;
            if ((uv[0].v > clipBounds.bottom && uv[1].v < clipBounds.bottom) ||
                (outside = (uv[1].v > clipBounds.bottom &&
                    uv[0].v < clipBounds.bottom)) != 0) {
                NML_STEALTH_CLIP_ENDPOINT(uv[outside].v - clipBounds.bottom,
                    uv[outside].v - uv[1 - outside].v);
            }
            uv[0].u &= 0x1fff;
            uv[0].v &= 0x1fff;
            uv[1].u &= 0x1fff;
            uv[1].v &= 0x1fff;
        }
    }
}

void nmlFilterSetPacket(int *model, int count)
{
    int flags;
    int i;

    flags = model[0x234 / 4];
    for (i = 0; i < 31; i++) {
        switch (flags & (1 << i)) {
        case 1:
            flags &= ~1;
            nmlModelSetFilterGunosys(model, flags, count);
            break;
        case 2:
            flags &= ~2;
            nmlModelSetFilterStealth(model, flags);
            break;
        }
    }
}

void nmlPacketGsInit(void);

void packet_gs_entry64(int, void *);

void nmlPacketAddGsFlush(void);

typedef struct { u64 data; u64 addr; } FbtfGsReg;

void nmlFilterSetFrameToBuffer(unsigned short dbp)
{
    FbtfGsReg *p;
    int i;

    ((FbtfGsReg *)0x70000000)->data = 0;
    ((FbtfGsReg *)0x70000000)->addr = 0x3F;
    ((FbtfGsReg *)0x70000010)->data = 0x80000 | (dbp << 5) | ((u64)sRender.framebufferPage << 37) | (0x8000LL << 36)
        | ((u64)sRender.pixelStorageMode << 24) | ((u64)sRender.pixelStorageMode << 56);
    ((FbtfGsReg *)0x70000010)->addr = 0x50;
    ((FbtfGsReg *)0x70000020)->data = 0;
    ((FbtfGsReg *)0x70000020)->addr = 0x51;
    ((FbtfGsReg *)0x70000030)->data = (0x1C0LL << 32) | 0x200;
    ((FbtfGsReg *)0x70000030)->addr = 0x52;
    ((FbtfGsReg *)0x70000040)->data = 2;
    ((FbtfGsReg *)0x70000040)->addr = 0x53;
    nmlPacketGsInit();
    p = (FbtfGsReg *)0x70000000;
    for (i = 4; i >= 0; i--) {
        packet_gs_entry64(p->addr, p);
        p++;
    }
    nmlPacketAddGsFlush();
}

void nmlFilterSetBufferToFrame(unsigned short dbp)
{
    FbtfGsReg *p;
    int i;

    ((FbtfGsReg *)0x70000000)->data = 0;
    ((FbtfGsReg *)0x70000000)->addr = 0x3F;
    ((FbtfGsReg *)0x70000010)->data = 0x80000 | (sRender.framebufferPage << 5) | ((u64)((short *)&sRender)[1] << 24)
        | ((u64)dbp << 37) | (0x8000LL << 36) | ((u64)((short *)&sRender)[1] << 56);
    ((FbtfGsReg *)0x70000010)->addr = 0x50;
    ((FbtfGsReg *)0x70000020)->data = 0;
    ((FbtfGsReg *)0x70000020)->addr = 0x51;
    ((FbtfGsReg *)0x70000030)->data = (0x1C0LL << 32) | 0x200;
    ((FbtfGsReg *)0x70000030)->addr = 0x52;
    ((FbtfGsReg *)0x70000040)->data = 2;
    ((FbtfGsReg *)0x70000040)->addr = 0x53;
    nmlPacketGsInit();
    p = (FbtfGsReg *)0x70000000;
    for (i = 4; i >= 0; i--) {
        packet_gs_entry64(p->addr, p);
        p++;
    }
    nmlPacketAddGsFlush();
}

typedef struct { u64 data; u64 addr; } FsbrGsReg;

void nmlFilterSetBufferRender(float f)
{
    FsbrGsReg *p;
    int i;

    ((FsbrGsReg *)0x70000000)->data = 0;
    ((FsbrGsReg *)0x70000000)->addr = 0x3F;
    ((FsbrGsReg *)0x70000010)->data = (((unsigned short *)&sRender)[0xA] << 5 | 0x24020000) | 0x2000000640000000LL;
    ((FsbrGsReg *)0x70000010)->addr = 6;
    ((FsbrGsReg *)0x70000020)->data = 0;
    ((FsbrGsReg *)0x70000020)->addr = 0x14;
    ((FsbrGsReg *)0x70000030)->data = 0;
    ((FsbrGsReg *)0x70000030)->addr = 8;
    ((FsbrGsReg *)0x70000040)->data = 0x31001;
    ((FsbrGsReg *)0x70000040)->addr = 0x47;
    ((FsbrGsReg *)0x70000050)->data = ((u64)(int)(f * 128.0f) << 32) | 0x64;
    ((FsbrGsReg *)0x70000050)->addr = 0x42;
    ((FsbrGsReg *)0x70000060)->data = 0x3F80000080808080LL;
    ((FsbrGsReg *)0x70000060)->addr = 1;
    ((FsbrGsReg *)0x70000070)->data = 0x116;
    if (f < 1.0f) {
        ((FsbrGsReg *)0x70000070)->data = 0x156;
    }
    ((FsbrGsReg *)0x70000070)->addr = 0;
    ((FsbrGsReg *)0x70000080)->data = 0;
    ((FsbrGsReg *)0x70000080)->addr = 3;
    ((FsbrGsReg *)0x70000090)->data = 0x72007000;
    ((FsbrGsReg *)0x70000090)->addr = 4;
    ((FsbrGsReg *)0x700000A0)->data = 0x1C102000;
    ((FsbrGsReg *)0x700000A0)->addr = 3;
    ((FsbrGsReg *)0x700000B0)->data = 0x8E009000LL;
    ((FsbrGsReg *)0x700000B0)->addr = 4;
    nmlPacketGsInit();
    p = (FsbrGsReg *)0x70000000;
    for (i = 11; i >= 0; i--) {
        packet_gs_entry64(p->addr, p);
        p++;
    }
    nmlPacketAddGsFlush();
}

typedef struct { u64 data; u64 addr; } FfacGsReg;
void nmlPacketAddGsFrame(int, int);
void nmlPacketAddGsFBA(int);

void nmlFilterSetFrameAlphaClear(void)
{
    FfacGsReg *p;
    int i;

    ((FfacGsReg *)0x70000000)->data = 0x68;
    ((FfacGsReg *)0x70000000)->addr = 0x42;
    ((FfacGsReg *)0x70000010)->data = 0x31001;
    ((FfacGsReg *)0x70000010)->addr = 0x47;
    ((FfacGsReg *)0x70000020)->data = 0x3F80000001000000LL;
    ((FfacGsReg *)0x70000020)->addr = 1;
    ((FfacGsReg *)0x70000030)->data = 0x46;
    ((FfacGsReg *)0x70000030)->addr = 0;
    ((FfacGsReg *)0x70000040)->data = 0x72007000;
    ((FfacGsReg *)0x70000040)->addr = 4;
    ((FfacGsReg *)0x70000050)->data = 0x8E009000LL;
    ((FfacGsReg *)0x70000050)->addr = 4;
    nmlPacketGsInit();
    nmlPacketAddGsFrame(((unsigned short *)&sRender)[0x10], 0);
    nmlPacketAddGsFBA(0);
    p = (FfacGsReg *)0x70000000;
    for (i = 5; i >= 0; i--) {
        packet_gs_entry64(p->addr, p);
        p++;
    }
    nmlPacketAddGsFlush();
}

void nmlPacketGsInit(void);
void packet_gs_entry64(int, void *);
void nmlPacketAddGsFlush(void);
typedef struct { u64 data; u64 addr; } FstcGsReg;

void nmlFilterSetTexClear(void)
{
    FstcGsReg *p;
    int i;

    ((FstcGsReg *)0x70000000)->data = 0x8A;
    ((FstcGsReg *)0x70000000)->addr = 0x42;
    ((FstcGsReg *)0x70000010)->data = 0x31001;
    ((FstcGsReg *)0x70000010)->addr = 0x47;
    ((FstcGsReg *)0x70000020)->data = 0x3F80000080000000LL;
    ((FstcGsReg *)0x70000020)->addr = 1;
    ((FstcGsReg *)0x70000030)->data = 0x46;
    ((FstcGsReg *)0x70000030)->addr = 0;
    ((FstcGsReg *)0x70000040)->data = 0x72007000;
    ((FstcGsReg *)0x70000040)->addr = 4;
    ((FstcGsReg *)0x70000050)->data = 0x7FF09000;
    ((FstcGsReg *)0x70000050)->addr = 4;
    nmlPacketGsInit();
    p = (FstcGsReg *)0x70000000;
    for (i = 5; i >= 0; i--) {
        packet_gs_entry64(p->addr, p);
        p++;
    }
    nmlPacketAddGsFlush();
}

void nmlPacketGsInit(void);
void nmlPacketAddGsZbuf(int);
void nmlPacketAddGsFrame(int, int);
void packet_gs_entry64(int, void *);
void nmlPacketAddGsFlush(void);
typedef struct { u64 data; u64 addr; } FbcGsReg;

void nmlFilterBackClear(void)
{
    FbcGsReg *p;
    int i;

    ((FbcGsReg *)0x70000000)->data = 0x30000;
    ((FbcGsReg *)0x70000000)->addr = 0x47;
    ((FbcGsReg *)0x70000010)->data = 0xFE00LL << 46;
    ((FbcGsReg *)0x70000010)->addr = 1;
    ((FbcGsReg *)0x70000020)->data = 6;
    ((FbcGsReg *)0x70000020)->addr = 0;
    ((FbcGsReg *)0x70000030)->data = 0x72007000;
    ((FbcGsReg *)0x70000030)->addr = 4;
    ((FbcGsReg *)0x70000040)->data = 0x8E009000LL;
    ((FbcGsReg *)0x70000040)->addr = 4;
    nmlPacketGsInit();
    nmlPacketAddGsZbuf(1);
    nmlPacketAddGsFrame(sRender.framebufferPage, 0);
    p = (FbcGsReg *)0x70000000;
    for (i = 4; i >= 0; i--) {
        packet_gs_entry64(p->addr, p);
        p++;
    }
    nmlPacketAddGsFlush();
}

typedef union { struct { u64 data; u64 addr; } r; float f; } FsfrGsReg;
void nmlPacketAddGsFrame1(unsigned short, u64);
void nmlPacketAddGsFlushWide(void);
typedef struct { char pad[0x2C0]; float f; } FsfrArg;
typedef struct { char pad[0x14]; unsigned short fbp; char pad2[0xA]; unsigned short x20; } FsfrRender;

void nmlFilterSetFlatRender(void *arg)
{
    FsfrGsReg *p;
    int i;

    if (arg == 0) {
        return;
    }
    ((FsfrGsReg *)0x70000000)->r.data = 0;
    ((FsfrGsReg *)0x70000000)->r.addr = 0x3F;
    ((FsfrGsReg *)0x70000010)->r.data = (sRender.framebufferPage << 5 | 0x24020000) | 0x2000000640000000LL;
    ((FsfrGsReg *)0x70000010)->r.addr = 6;
    ((FsfrGsReg *)0x70000020)->r.data = 0x60;
    ((FsfrGsReg *)0x70000020)->r.addr = 0x14;
    ((FsfrGsReg *)0x70000030)->r.data = 0;
    ((FsfrGsReg *)0x70000030)->r.addr = 8;
    ((FsfrGsReg *)0x70000040)->r.data = ((u64)(int)(((FsfrArg *)arg)->f * 128.0f) << 32) | 0x68;
    ((FsfrGsReg *)0x70000040)->r.addr = 0x42;
    ((FsfrGsReg *)0x70000050)->r.data = 0x30000;
    ((FsfrGsReg *)0x70000050)->r.addr = 0x47;
    ((FsfrGsReg *)0x70000060)->r.data = 0x3F80000080808080LL;
    ((FsfrGsReg *)0x70000060)->r.addr = 1;
    ((FsfrGsReg *)0x70000070)->r.data = 0x156;
    ((FsfrGsReg *)0x70000070)->r.addr = 0;
    ((FsfrGsReg *)0x70000080)->r.data = 0;
    ((FsfrGsReg *)0x70000080)->r.addr = 3;
    ((FsfrGsReg *)0x70000090)->r.data = 0x72007000;
    ((FsfrGsReg *)0x70000090)->r.addr = 4;
    ((FsfrGsReg *)0x700000A0)->r.data = 0x1C002000;
    ((FsfrGsReg *)0x700000A0)->r.addr = 3;
    ((FsfrGsReg *)0x700000B0)->r.data = 0x8E009000LL;
    ((FsfrGsReg *)0x700000B0)->r.addr = 4;
    ((FsfrGsReg *)0x700000C0)->r.data = 0x50000;
    ((FsfrGsReg *)0x700000C0)->r.addr = 0x47;
    nmlPacketGsInit();
    nmlPacketAddGsZbuf(1);
    nmlPacketAddGsFrame(sRender.displayBufferBase, 0);
    nmlPacketAddGsFrame1(sRender.displayBufferBase, 0);
    p = (FsfrGsReg *)0x70000000;
    for (i = 12; i >= 0; i--) {
        packet_gs_entry64(p->r.addr, p);
        p++;
    }
    nmlPacketAddGsFlushWide();
}

typedef union { struct { u64 data; u64 addr; } r; float f; } FtfdGsReg;
typedef struct { char pad[0x26C]; float f; int i; } FtfdArg;

INCLUDE_ASM("asm/main/nonmatchings/nml_filter_set", nmlFilterSetTexFillDraw);
/* H5 probe 1 */

INCLUDE_ASM("asm/main/nonmatchings/nml_filter_set", nmlFilterSetVolumeCubeRender);

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

INCLUDE_ASM("asm/main/nonmatchings/nml_filter_set", nmlModelSetFilterGunosys);

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

INCLUDE_ASM("asm/main/nonmatchings/nml_filter_set", nmlFilterStealthMake);

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

INCLUDE_ASM("asm/main/nonmatchings/nml_filter_set", nmlFilterSetTexFillDraw);

INCLUDE_ASM("asm/main/nonmatchings/nml_filter_set", nmlFilterSetVolumeCubeRender);

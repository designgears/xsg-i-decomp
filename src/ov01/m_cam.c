/*
 * OV01 original TU 17: 0x00a314b8..0x00a32db8 (36 functions)
 */
#include "common.h"
#include "shared.h"

extern void MOutputDebugStringWarn(const char *format, ...);
extern const char zero_duration_message[];
extern const char invalid_interpolation_message[];
extern const char D_00A50EF0[];
extern const char D_00A51000[];
extern const char D_00A51028[];

extern void bcopy(const void *source, void *destination, unsigned int count);
/*
 * Partial view of the unit init record returned by dataUnitInitGet (its
 * definer, ov01/data_unit_org_get.c, is not recovered yet); only the two
 * ranges this TU reads are modeled. Ranges are stored in hundredths.
 */
typedef struct UnitInitData {
    unsigned char unmodeled_00[0x10];
    short atkRange;
    short defRange;
} UnitInitData;

extern UnitInitData *dataUnitInitGet(int id);

/*
 * .bss for this TU is still scaffold-owned (config/tu-build.json
 * data_ownership); the scaffold defines it under the original ELF's own
 * local symbol names, imported through config/symbols/ov01.txt.
 */
extern StudioCamera *sysCam;
extern unsigned char sysCamBuff[0x5F0];
extern int camFlags;
extern unsigned char camParams[0x5B0];
extern float D_00A5B220;
extern float D_00A5B224;

#define CAM_FLAG_ACTIVE 0x1
#define CAM_FLAG_DUMP_DISABLED 0x10
#define CAM_SHAKE_BIT 12

/*
 * Linear interpolation state shared by the per-parameter MoveProc callbacks
 * (mode/duration/elapsed feed MCamMoveProc_CalcRatio; start/end/current hold
 * the eased scalar value).
 */
typedef struct MCamMoveState {
    unsigned int mode;
    unsigned int duration;
    unsigned int elapsed;
    float start;
    float end;
    float current;
} MCamMoveState;

void *MCamGetSysCam(void)
{
    return sysCam;
}

extern Vector4 D_00A5B200;
extern Vector4 D_00A5B210;

Vector4 *MCamGetCurrentBaseCoord(Vector4 *out, int type)
{
    Vector4 *src;

    __asm__ __volatile__("sq $0, 0(%0)" : : "r"(out) : "memory");
    src = &D_00A5B200;
    __asm__ __volatile__("" : "+r"(src));
    if (type == 0) goto copy;
    if (type != 1) goto end;
    src = &D_00A5B210;
copy:
    __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(src), "r"(out) : "$8", "memory");
end:
    return out;
}

extern Vector4 D_00A5B230;
extern Vector4 D_00A5B240;

Vector4 *MCamGetCurrentCoord(Vector4 *out, int type)
{
    Vector4 *src;

    __asm__ __volatile__("sqc2 $vf0, 0(%0)" : : "r"(out) : "memory");
    src = &D_00A5B230;
    __asm__ __volatile__("" : "+r"(src));
    if (type == 0) goto copy;
    if (type != 1) goto end;
    src = &D_00A5B240;
copy:
    __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(src), "r"(out) : "$8", "memory");
end:
    out->w = 1.0f;
    return out;
}

extern Vector4 D_00A5B200;
extern Vector4 D_00A5B210;
extern Vector4 D_00A5B230;
extern Vector4 D_00A5B240;
extern Vector4 D_00A5B1E0;
extern Vector4 D_00A5B1F0;

Vector4 *MCamGetCurrentAngle(Vector4 *out, int type)
{
    Vector4 *src;

    __asm__ __volatile__("sq $0, 0(%0)" : : "r"(out) : "memory");
    src = &D_00A5B1E0;
    __asm__ __volatile__("" : "+r"(src));
    if (type == 0) goto copy;
    if (type != 1) goto end;
    src = &D_00A5B1F0;
copy:
    __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(src), "r"(out) : "$8", "memory");
end:
    return out;
}

extern Vector4 D_00A5B250;

Vector4 *MCamGetCurrentEye(Vector4 *out)
{
    __asm__ __volatile__(
        "lq $8,0(%1)\n\t"
        "sq $8,0(%0)\n\t"
        :
        : "r"(out), "r"(&D_00A5B250)
        : "$8", "memory"
    );
    return out;
}

float MCamGetLengthActor(int selector)
{
    float length = 0.0f;

    switch (selector) {
    case 0:
        length = D_00A5B220;
        break;
    case 1:
        length = D_00A5B224;
        break;
    }
    return length;
}

void MCamInit(void)
{
    camFlags = 0;
    memset(camParams, 0, sizeof(camParams));
}

static void MCamExec_Control(void);
static void MCamExec_SetParams(void);
static void MCamExec_DispParams(void);

void MCamExec(void)
{
    if (camFlags & CAM_FLAG_ACTIVE) {
        MCamExec_Control();
        MCamExec_SetParams();
        MCamExec_DispParams();
    }
}

void MCamDumpDisabled(short disable)
{
    if (disable != 0) {
        camFlags |= CAM_FLAG_DUMP_DISABLED;
    } else {
        camFlags &= ~CAM_FLAG_DUMP_DISABLED;
    }
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct MCamShakeState {
    unsigned char unmodeled_00[0x4];
    float amplitude;
    float decayStep;
    float offset;
    float phase;
    float phaseStep;
    unsigned int holdFrames;
    unsigned int decayFrames;
} MCamShakeState;

typedef struct MCamCoordOffset {
    unsigned int mode;
    unsigned char unmodeled_04[0xC];
    Vector4 offset;
    float distance;
    float pitch;
    float yaw;
    unsigned char unmodeled_2C[0x14];
} MCamCoordOffset;

typedef struct MCamPointParams {
    unsigned char unmodeled_00[0x70];
    MCamCoordOffset coordOffset;
} MCamPointParams;

INCLUDE_ASM("asm/nonmatchings/ov01/m_cam", MCamTakeControl);

int MCamReleaseControl(void)
{
    if (!(camFlags & CAM_FLAG_ACTIVE)) {
        MOutputDebugStringWarn(D_00A50EF0);
        return 0;
    }

    bcopy(sysCamBuff, sysCam, sizeof(sysCamBuff));
    camFlags &= ~CAM_FLAG_ACTIVE;
    return 1;
}

void MCamStoreParams(void)
{
    unsigned char *cam;
    unsigned char *params;
    float baseZ, baseX;

    /* 12-byte ld/lw copies: the original's block-move template with 8-byte
     * alignment, which cc1 only emits as move_by_pieces from C. */
    __asm__ __volatile__(
        "ld $8, 0(%1)\n\tlw $9, 8(%1)\n\tsd $8, 0(%0)\n\tsw $9, 8(%0)"
        : : "r"(sysCamBuff + 0xD0), "r"(camParams + 0x1B0) : "$8", "$9", "memory");
    __asm__ __volatile__(
        "ld $8, 0(%1)\n\tlw $9, 8(%1)\n\tsd $8, 0(%0)\n\tsw $9, 8(%0)"
        : : "r"(sysCamBuff + 0xA0), "r"(camParams + 0x1E0) : "$8", "$9", "memory");
    cam = sysCamBuff;
    params = camParams;
    baseX = *(float *)(params + 0x1F0);
    baseZ = *(float *)(params + 0x1F4);
    *(float *)(cam + 0x94) = baseZ;
    *(float *)(cam + 0xA8) = baseX;
}

int MCamIsActive(void)
{
    return camFlags & CAM_FLAG_ACTIVE;
}

extern const char D_00A50F20[];
extern float D_00A5B270;
static void MCamExec_SetParams_Base(void);
typedef struct MCamSetBlock {
    long long v[14];
} MCamSetBlock;
typedef struct MCamSetTail {
    long long v[6];
} MCamSetTail;
typedef struct MCamSetPoint {
    MCamSetBlock base;
    MCamSetTail tail;
    float fA0;
    float fA4;
    unsigned char pad[8];
} MCamSetPoint;
typedef struct MCamSetParamsView {
    MCamSetBlock p0;
    MCamSetTail t0;
    unsigned char pad_a0[0x10];
    MCamSetBlock p1;
    MCamSetTail t1;
    unsigned char pad_150[0xA4];
    float pers;
} MCamSetParamsView;

void MCamSet(unsigned int point, MCamSetPoint *src)
{
    float f;
    MCamSetParamsView *view;
    switch (point) {
    case 0:
        ((MCamSetParamsView *)camParams)->p0 = src->base;
        ((MCamSetParamsView *)camParams)->t0 = src->tail;
        break;
    case 1:
        ((MCamSetParamsView *)camParams)->p1 = src->base;
        ((MCamSetParamsView *)camParams)->t1 = src->tail;
        break;
    case 2:
        D_00A5B270 = src->fA0;
        break;
    case 3:
        view = (MCamSetParamsView *)camParams;
        if (src->fA4 != 0.0f) {
            f = src->fA4 * 0.017453292f;
        } else {
            f = 0.6981317f;
        }
        view->pers = f;
        break;
    default:
        MOutputDebugStringWarn(D_00A50F20);
        return;
    }
    MCamExec_SetParams_Base();
}

struct MCamCoordRequest;
struct MCamBankRequest;
struct MCamMoveRequest;
extern char D_00A50F38[];
extern char D_00A50F60[];
static void MCamMove_Coord(int which, struct MCamCoordRequest *req);
static void MCamMove_Bank(struct MCamBankRequest *request);
static void MCamMove_Pers(struct MCamMoveRequest *request);
typedef struct {
    unsigned char unmodeled_00[0xB8];
    int frames;
} MCamMoveFrames;

void MCamMove(unsigned int point, struct MCamCoordRequest *req)
{
    if (((MCamMoveFrames *)req)->frames == 0) {
        MOutputDebugStringWarn(D_00A50F38);
        ((MCamMoveFrames *)req)->frames = 1;
    }
    switch (point) {
    case 0:
        MCamMove_Coord(0, req);
        break;
    case 1:
        MCamMove_Coord(1, req);
        break;
    case 2:
        MCamMove_Bank((struct MCamBankRequest *)req);
        break;
    case 3:
        MCamMove_Pers((struct MCamMoveRequest *)req);
        break;
    default:
        MOutputDebugStringWarn(D_00A50F60);
        break;
    }
}

extern char D_00A50F98[];

void MCamStopMove(unsigned int type)
{
    int mask;
    switch (type) {
    case 0: mask = 0x100; break;
    case 1: mask = 0x200; break;
    case 2: mask = 0x400; break;
    case 3: mask = 0x800; break;
    default:
        MOutputDebugStringWarn(D_00A50F98);
        return;
    }
    camFlags &= ~mask;
}

extern const char D_00A50FB8[];

int MCamIsMoving(unsigned int type)
{
    int mask;
    switch (type) {
    case 0: mask = 0x100; break;
    case 1: mask = 0x200; break;
    case 2: mask = 0x400; break;
    case 3: mask = 0x800; break;
    default:
        MOutputDebugStringWarn(D_00A50FB8);
        return 0;
    }
    return (camFlags & mask) != 0;
}

typedef struct MCamShakeBlock {
    unsigned char unk_00[4];
    float unk4;
    float unk8;
    float unkC;
    float unk10;
    float unk14;
    int unk18;
    int unk1C;
} MCamShakeBlock;

extern MCamShakeBlock D_00A5B610;

void MCamShake(int arg0, int arg1, int arg2, int arg3, int arg4)
{
    MCamShakeBlock *blk = &D_00A5B610;
    float f2;
    float f1;
    float f1b;

    if (arg1 == 0) {
        camFlags &= ~0x1000;
        return;
    }
    *(int *)blk = arg0;
    if (arg1 >= 0) {
        f2 = (float)arg1;
    } else {
        f2 = (float)(unsigned int)arg1;
    }
    blk->unk8 = 0.0f;
    blk->unkC = 0.0f;
    blk->unk10 = 0.0f;
    blk->unk4 = f2 * 0.017453292f * 0.5f;
    if (arg2 != 0) {
        if (arg2 >= 0) {
            f1 = (float)arg2;
        } else {
            f1 = (float)(unsigned int)arg2;
        }
        blk->unk14 = 1.5707964f / f1;
    } else {
        blk->unk14 = 0.0f;
    }
    blk->unk18 = arg3;
    blk->unk1C = arg4;
    if (arg4 != 0) {
        if (arg4 >= 0) {
            f1b = (float)arg4;
        } else {
            f1b = (float)(unsigned int)arg4;
        }
        blk->unk8 = blk->unk4 / f1b;
    }
    camFlags |= 0x1000;
}

int MCamIsShaking(void)
{
    return (camFlags >> CAM_SHAKE_BIT) & 1;
}

extern void sefGetNullPosition(Vector4 *out, void *actor, unsigned int id, int flag);
extern float fmodf(float x, float y);
extern const char D_00A50FD8[];
typedef struct MCamActorView {
    unsigned char unmodeled_00[0x10];
    Vector4 position;
    unsigned char unmodeled_20[0x34];
    float rotY;
} MCamActorView;

static Vector4 *MCamGetActorCoord(Vector4 *out, MCamActorView *actor, int null)
{
    __asm__ __volatile__("sq $0, 0(%0)" : : "r"(out) : "memory");
    if (actor == 0) {
        MOutputDebugStringWarn(D_00A50FD8);
        return out;
    }
    if (null == 0) {
        __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(&actor->position), "r"(out) : "$8", "memory");
    } else {
        sefGetNullPosition(out, actor, null - 1, 0);
    }
    out->w = fmodf(actor->rotY + 3.1415927f, 6.283185f);
    return out;
}

extern Vector4 *MMathVectorInterpolation(Vector4 *destination, const Vector4 *first, const Vector4 *second, float parameter);
extern float MMathCalcDistXZ(const Vector4 *first, const Vector4 *second);
extern float MMathCalcDir(const Vector4 *from, const Vector4 *to);
static float MCamGetAtkRange(int id);
static float MCamGetDefRange(int id);
typedef struct MCamBaseParams {
    unsigned int type;
    unsigned char unmodeled_04[0xC];
    Vector4 position;
    MCamActorView *actor;
    unsigned char unmodeled_24[4];
    int null;
    MCamActorView *actorA;
    int idA;
    int nullA;
    MCamActorView *actorB;
    int idB;
    int nullB;
    unsigned char unmodeled_44[0x28];
    float ratio;
} MCamBaseParams;

static float MCamCalcBaseCoord(Vector4 *out, MCamBaseParams *params)
{
    Vector4 a;
    Vector4 b;
    float dist;
    float t;

    dist = 0.0f;
    __asm__ __volatile__("sq $0, 0(%0)" : : "r"(out) : "memory");
    switch (params->type) {
    case 0:
        __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(&params->position), "r"(out) : "$8", "memory");
        out->w = dist;
        break;
    case 1:
        MCamGetActorCoord(out, params->actor, params->null);
        break;
    case 2:
        MCamGetActorCoord(&a, params->actorA, params->nullA);
        MCamGetActorCoord(&b, params->actorB, params->nullB);
        MMathVectorInterpolation(out, &a, &b, params->ratio);
        dist = MMathCalcDistXZ(&a, &b);
        out->w = MMathCalcDir(&b, &a);
        break;
    case 3:
        MCamGetActorCoord(&a, params->actorA, params->nullA);
        MCamGetActorCoord(&b, params->actorB, params->nullB);
        dist = MMathCalcDistXZ(&a, &b);
        t = (dist + MCamGetAtkRange(params->idA) - MCamGetDefRange(params->idB)) * 0.5f;
        if (dist == 0.0f || t == 0.0f) {
            __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(&a), "r"(out) : "$8", "memory");
        } else {
            MMathVectorInterpolation(out, &a, &b, t / dist);
        }
        out->w = MMathCalcDir(&b, &a);
        break;
    }
    return dist;
}

extern Vector4 *MMathCalcOffset(Vector4 *destination, const Vector4 *first, const Vector4 *second);
extern Vector4 *MMathCalcOffsetXYZ(Vector4 *destination, const Vector4 *first, const Vector4 *second);
typedef struct MCamOffsetParams {
    unsigned int type;
    unsigned char unmodeled_04[0xC];
    Vector4 offset;
    float distance;
    float angleA;
    float angleB;
    unsigned char unmodeled_2C[0x14];
} MCamOffsetParams;
typedef struct MCamCoordParams {
    MCamBaseParams base;
    MCamOffsetParams offset;
} MCamCoordParams;

static float MCamCalcCoord(Vector4 *out, MCamCoordParams *params)
{
    MCamOffsetParams *off = &params->offset;
    float dist;
    Vector4 v;

    dist = MCamCalcBaseCoord(out, &params->base);
    switch (off->type) {
    case 0:
        break;
    case 1:
        MMathCalcOffsetXYZ(out, out, &params->offset.offset);
        break;
    case 2:
        v.x = off->angleA;
        v.y = off->angleB;
        v.w = off->distance;
        MMathCalcOffset(out, out, &v);
        break;
    }
    return dist;
}

static float MCamGetAtkRange(int id)
{
    UnitInitData *unit = dataUnitInitGet(id);

    if (unit == 0) {
        MOutputDebugStringWarn(D_00A51000, id);
        return 0.0f;
    }
    return (float)unit->atkRange / 100.0f;
}

static float MCamGetDefRange(int id)
{
    UnitInitData *unit = dataUnitInitGet(id);

    if (unit == 0) {
        MOutputDebugStringWarn(D_00A51028, id);
        return 0.0f;
    }
    return (float)unit->defRange / 100.0f;
}

typedef struct MCamCoordSlot {
    MCamCoordParams p __attribute__((aligned(16)));
} MCamCoordSlot;

typedef struct MCamCoordMove {
    MCamCoordSlot from;
    MCamCoordSlot to;
    Vector4 other;
    Vector4 start;
    Vector4 end;
    Vector4 current;
    int extra[4];
} MCamCoordMove;

extern MCamCoordMove D_00A5B280;
extern MCamCoordMove D_00A5B430;
extern MCamMoveState D_00A5B5E0;
extern MCamMoveState D_00A5B5F8;
static int MCamMoveProc_Coord(MCamCoordMove *m);
static int MCamMoveProc_Bank(MCamMoveState *state);
static int MCamMoveProc_Pers(MCamMoveState *state);
static void MCamShakeProc(void);
static inline void mcam_quad_copy(void *dst, const void *src)
{
    __asm__ __volatile__("lq $8, 0(%1)\n\tsq $8, 0(%0)" : : "r"(dst), "r"(src) : "$8", "memory");
}

static void MCamExec_Control(void)
{
    if (camFlags & 0x100) {
        if (MCamMoveProc_Coord(&D_00A5B280) != 0) {
            unsigned char *cp = (unsigned char *)&D_00A5B280 - 0x200;
            *(long long *)(cp + 0x10) = *(long long *)(cp + 0x390);
            *(long long *)(cp + 0x18) = *(long long *)(cp + 0x398);
        } else {
            MCamSet(0, (MCamSetPoint *)((unsigned char *)&D_00A5B280 + 0xB0));
            if (camFlags & 0x2000) {
                unsigned char *cp = (unsigned char *)&D_00A5B280 - 0x200;
                *(int *)(cp + 0x0) = 0;
                mcam_quad_copy((unsigned char *)&D_00A5B280 - 0x1F0, (unsigned char *)&D_00A5B280 - 0x50);
                *(int *)(cp + 0x70) = 0;
            }
            camFlags &= ~0x2100;
        }
    }
    if (camFlags & 0x200) {
        if (MCamMoveProc_Coord(&D_00A5B430) != 0) {
            unsigned char *cp = (unsigned char *)&D_00A5B430 - 0x3B0;
            *(long long *)(cp + 0xC0) = *(long long *)(cp + 0x540);
            *(long long *)(cp + 0xC8) = *(long long *)(cp + 0x548);
        } else {
            MCamSet(1, (MCamSetPoint *)((unsigned char *)&D_00A5B430 + 0xB0));
            if (camFlags & 0x4000) {
                unsigned char *cp = (unsigned char *)&D_00A5B430 - 0x3B0;
                *(int *)(cp + 0xB0) = 0;
                mcam_quad_copy((unsigned char *)&D_00A5B430 - 0x2F0, (unsigned char *)&D_00A5B430 - 0x1F0);
                *(int *)(cp + 0x120) = 0;
            }
            camFlags &= ~0x4200;
        }
    }
    if (camFlags & 0x400) {
        unsigned char *cp;
        if (MCamMoveProc_Bank(&D_00A5B5E0) == 0) {
            camFlags &= ~0x400;
        }
        cp = (unsigned char *)&D_00A5B5E0 - 0x560;
        *(float *)(cp + 0x1F0) = *(float *)(cp + 0x574);
    }
    if (camFlags & 0x800) {
        unsigned char *cp;
        if (MCamMoveProc_Pers(&D_00A5B5F8) == 0) {
            camFlags &= ~0x800;
        }
        cp = (unsigned char *)&D_00A5B5F8 - 0x578;
        *(float *)(cp + 0x1F4) = *(float *)(cp + 0x58C);
    }
    if (camFlags & 0x1000) {
        MCamShakeProc();
    }
}

extern Vector4 *MMathCalcDirVector(Vector4 *out, Vector4 *from, Vector4 *to);
static inline void mcam_copy_vec3(void *dst, const void *src)
{
    __asm__ __volatile__("ld $8,0(%1)\n\t"
                         "lw $9,8(%1)\n\t"
                         "sd $8,0(%0)\n\t"
                         "sw $9,8(%0)"
                         : : "r"(dst), "r"(src) : "$8", "$9", "memory");
}

static void MCamExec_SetParams(void)
{
    float v[3];
    char *base = (char *)&D_00A5B250;
    char *p;
    int flags;

    MCamExec_SetParams_Base();
    MMathCalcDirVector((Vector4 *)base, (Vector4 *)(base - 0x20), (Vector4 *)(base - 0x10));
    mcam_copy_vec3(v, base + 0x10);
    if (camFlags & 0x1000) {
        p = base - 0x1D0;
        flags = *(int *)(p + 0x590);
        if (flags & 1) {
            v[0] += *(float *)(p + 0x59C);
        }
        if (flags & 0x10) {
            v[1] += *(float *)(p + 0x59C);
        }
        if (flags & 0x100) {
            v[2] += *(float *)(p + 0x59C);
        }
    }
    mcam_copy_vec3((char *)sysCam + 0xD0, &D_00A5B230);
    mcam_copy_vec3((char *)sysCam + 0xA0, v);
    *(float *)((char *)sysCam + 0x94) = *(float *)((char *)&D_00A5B230 + 0x44);
}

extern Vector4 *MMathCalcAngleCam(Vector4 *out, const Vector4 *from, const Vector4 *to);

static void MCamExec_SetParams_Base(void)
{
    char *base = (char *)&D_00A5B200 - 0x180;

    *(float *)(base + 0x1A0) = MCamCalcBaseCoord((Vector4 *)(base + 0x180), (MCamBaseParams *)base);
    *(float *)(base + 0x1A4) = MCamCalcBaseCoord((Vector4 *)(base + 0x190), (MCamBaseParams *)(base + 0xB0));
    MCamCalcCoord((Vector4 *)(base + 0x1B0), (MCamCoordParams *)base);
    MCamCalcCoord((Vector4 *)(base + 0x1C0), (MCamCoordParams *)(base + 0xB0));
    MMathCalcAngleCam((Vector4 *)(base + 0x160), (Vector4 *)(base + 0x1B0), (Vector4 *)(base + 0x1C0));
    __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(base + 0x160), "r"(base + 0x1E0) : "$8", "memory");
    *(float *)(base + 0x1E8) = *(float *)(base + 0x1F0);
    MMathCalcAngleCam((Vector4 *)(base + 0x170), (Vector4 *)(base + 0x1C0), (Vector4 *)(base + 0x1B0));
}

static void MCamExec_DispParams(void)
{
}

enum MCamOffsetKind { MCAM_OFFSET_NONE, MCAM_OFFSET_XYZ, MCAM_OFFSET_ANGLE };
typedef struct MCamInt3 {
    int v[3];
} MCamInt3;
typedef struct MCamCoordRequest {
    MCamCoordSlot params;
    int extra[3];
    short shake;
} MCamCoordRequest;
extern MCamCoordMove D_00A5B280;
extern MCamCoordMove D_00A5B430;

static void MCamMove_Coord(int which, MCamCoordRequest *req)
{
    MCamCoordMove *m;
    MCamCoordParams *other;
    MCamCoordSlot *cur;
    int moveFlag;
    int shakeFlag;
    short shake;

    if (which == 0) {
        moveFlag = 0x100;
        m = &D_00A5B280;
        shakeFlag = 0x2000;
        other = (MCamCoordParams *)((char *)m - 0x150);
        cur = (MCamCoordSlot *)((char *)m - 0x200);
    } else {
        moveFlag = 0x200;
        m = &D_00A5B430;
        shakeFlag = 0x4000;
        other = (MCamCoordParams *)((char *)m - 0x3B0);
        cur = (MCamCoordSlot *)((char *)m - 0x300);
    }
    m->from = *cur;
    m->to = req->params;
    MCamCalcCoord(&m->other, other);
    MCamCalcCoord(&m->start, &m->from.p);
    MCamCalcCoord(&m->end, &m->to.p);
    __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(&m->start), "r"(&m->current) : "$8", "memory");
    {
        int z = req->extra[2];
        int x = req->extra[0];
        int y = req->extra[1];
        m->extra[0] = x;
        m->extra[1] = y;
        m->extra[2] = z;
        m->extra[3] = 0;
    }
    cur->p.base.type = 0;
    __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(&m->current), "r"(&cur->p.base.position) : "$8", "memory");
    shake = req->shake;
    {
        int *flags = &camFlags;
        *(enum MCamOffsetKind *)&cur->p.offset.type = MCAM_OFFSET_NONE;
        *flags |= moveFlag;
        if (shake != 0) {
            *flags |= shakeFlag;
        }
    }
}

extern char D_00A50F98[];
extern MCamMoveState D_00A5B5E0;
typedef struct MCamBankRequest {
    unsigned char unmodeled_00[0xA0];
    float bank;
    unsigned char unmodeled_a4[0xB4 - 0xA4];
    int mode;
    int duration;
} MCamBankRequest;

static void MCamMove_Bank(MCamBankRequest *request)
{
  MCamMoveState *state = &D_00A5B5E0;
  float start;
  int mode;
  int duration;
  float bank;
  start = *((float *) (((unsigned char *) state) - 0x370));
  state->start = start;
  duration = request->duration;
  mode = request->mode;
  bank = request->bank;
  state->current = state->start;
  camFlags |= 0x400;
 do { state->mode = mode; state->duration = duration; } while (0);
  (&D_00A5B5E0)->end = bank;
  (&D_00A5B5E0)->elapsed = 0;
}

/*
 * MCamMoveProc_Pers (below) steps this move state every frame; this
 * function's own lui/addiu is the TU's only relocation into it, so it is
 * typed as MCamMoveState here instead of left as bytes.
 */
extern MCamMoveState D_00A5B5F8;

/*
 * The float 0x384 bytes before D_00A5B5F8: the compiled form reuses
 * D_00A5B5F8's own base register with a negative immediate rather than a
 * second relocation, so nothing here names a symbol for it or claims a
 * struct spanning the two; MCamMove_Pers seeds its own `start` from it.
 */
#define MCAM_PREV_PERS_START_OFFSET (-0x384)

/*
 * The caller's own move-request object (its definer is outside this
 * allocation, called through MCamMove, also outside this allocation): only
 * the three fields MCamMove_Pers reads are evidenced.
 */
typedef struct MCamMoveRequest {
    unsigned char unmodeled_00[0xA4];
    float angle;
    unsigned char unmodeled_a8[0xB4 - 0xA8];
    int mode;
    int duration;
} MCamMoveRequest;

static void MCamMove_Pers(MCamMoveRequest *request)
{
    MCamMoveState *state = &D_00A5B5F8;
    int mode;
    float start;
    int duration;
    float angle;
    float angleRad;
    float savedStart;

    mode = request->mode;
    start = *(float *)((unsigned char *) state + MCAM_PREV_PERS_START_OFFSET);
    duration = request->duration;
    state->start = start;
    state->mode = mode;
    angle = request->angle;
    state->duration = duration;
    state->elapsed = 0;
    if (angle != 0.0f) {
        angleRad = angle * 0.017453292f;
    } else {
        angleRad = 0.6981317f;
    }
    savedStart = state->start;
    state->end = angleRad;
    state->current = savedStart;
    camFlags |= 0x800;
}


static float MCamMoveProc_CalcRatio(unsigned int mode, unsigned int elapsed, unsigned int duration)
{
    float ratio;

    if (duration == 0) {
        MOutputDebugStringWarn(zero_duration_message, mode);
        return 0.0f;
    }

    ratio = (float)elapsed / (float)duration;

    switch (mode) {
    case 1: {
        float sine1;
        ratio = ratio * 3.14159265f + 4.712389f;
        __asm__ __volatile__(
            "mfc1    $8, %1\n\t"
            "qmtc2   $8, $vf4\n\t"
            "vcallms 0x20\n\t"
            "qmfc2.i $8, $vf1\n\t"
            "mtc1    $8, %0\n\t"
            : "=f"(sine1)
            : "f"(ratio)
            : "$8", "memory"
        );
        ratio = (sine1 + 1.0f) * 0.5f;
        break;
    }
    case 0:
        break;
    case 2: {
        float sine2;
        ratio = ratio * 1.57079633f + 4.712389f;
        __asm__ __volatile__(
            "mfc1    $8, %1\n\t"
            "qmtc2   $8, $vf4\n\t"
            "vcallms 0x20\n\t"
            "qmfc2.i $8, $vf1\n\t"
            "mtc1    $8, %0\n\t"
            : "=f"(sine2)
            : "f"(ratio)
            : "$8", "memory"
        );
        ratio = sine2 + 1.0f;
        break;
    }
    case 3: {
        float sine3;
        ratio = ratio * 1.57079633f;
        __asm__ __volatile__(
            "mfc1    $8, %1\n\t"
            "qmtc2   $8, $vf4\n\t"
            "vcallms 0x20\n\t"
            "qmfc2.i $8, $vf1\n\t"
            "mtc1    $8, %0\n\t"
            : "=f"(sine3)
            : "f"(ratio)
            : "$8", "memory"
        );
        ratio = sine3;
        break;
    }
    default:
        MOutputDebugStringWarn(invalid_interpolation_message);
        break;
    }

    return ratio;
}

extern Vector4 *MMathCalcAngle(Vector4 *out, const Vector4 *from, const Vector4 *to);
extern float MMathCalcDist(const Vector4 *first, const Vector4 *second);
extern float MMathCalcRotNear(float first, float second);
static inline void MCamVecSub(Vector4 *dst, Vector4 *a, Vector4 *b)
{
    __asm__ __volatile__(
        "lqc2 vf1,0(%1)\n\t"
        "lqc2 vf2,0(%2)\n\t"
        "vsub.xyz vf1xyz,vf1xyz,vf2xyz\n\t"
        "sqc2 vf1,0(%0)"
        : : "r"(dst), "r"(a), "r"(b) : "memory");
}
static inline void MCamVecScale(Vector4 *dst, Vector4 *a, float t)
{
    __asm__ __volatile__(
        "lqc2 vf1,0(%1)\n\t"
        "mfc1 $8,%2\n\t"
        "qmtc2.ni $8,vf2\n\t"
        "vmulx.xyz vf1xyz,vf1xyz,vf2x\n\t"
        "sqc2 vf1,0(%0)"
        : : "r"(dst), "r"(a), "f"(t) : "$8", "memory");
}
static inline void MCamVecAdd(Vector4 *dst, Vector4 *a, Vector4 *b)
{
    __asm__ __volatile__(
        "lqc2 vf1,0(%1)\n\t"
        "lqc2 vf2,0(%2)\n\t"
        "vadd.xyz vf1xyz,vf1xyz,vf2xyz\n\t"
        "sqc2 vf1,0(%0)"
        : : "r"(dst), "r"(a), "r"(b) : "memory");
}

static int MCamMoveProc_Coord(MCamCoordMove *m)
{
    Vector4 step;
    Vector4 origin;
    Vector4 cur;
    Vector4 tmp;
    Vector4 offs;
    float ratio;

    ratio = MCamMoveProc_CalcRatio(m->extra[1], ++m->extra[3], m->extra[2]);
    MCamCalcCoord(&cur, &m->to.p);
    MCamVecSub(&tmp, &cur, &m->end);
    MCamVecScale(&step, &tmp, ratio);
    switch (m->extra[0]) {
    case 0:
        MMathVectorInterpolation(&cur, &m->start, &m->end, ratio);
        MCamVecAdd(&m->current, &cur, &step);
        break;
    case 1:
        __asm__ __volatile__("sq $0, 0(%0)" : : "r"(&origin) : "memory");
        __asm__ __volatile__(
            "ld $8, 0(%0)\n\t"
            "lw $9, 8(%0)\n\t"
            "sd $8, 0(%1)\n\t"
            "sw $9, 8(%1)"
            : : "r"(&m->other), "r"(&origin) : "$8", "$9", "memory");
        MMathCalcAngle(&cur, &origin, &m->start);
        cur.w = MMathCalcDist(&origin, &m->start);
        MMathCalcAngle(&tmp, &origin, &m->end);
        tmp.w = MMathCalcDist(&origin, &m->end);
        __asm__ __volatile__("sq $0, 0(%0)" : : "r"(&offs) : "memory");
        offs.x = cur.x + MMathCalcRotNear(cur.x, tmp.x) * ratio;
        offs.y = cur.y + MMathCalcRotNear(cur.y, tmp.y) * ratio;
        offs.w = cur.w + (tmp.w - cur.w) * ratio;
        MMathCalcOffset(&cur, &origin, &offs);
        MCamVecAdd(&m->current, &cur, &step);
        break;
    }
    return (unsigned int)m->extra[3] < (unsigned int)m->extra[2];
}

static int MCamMoveProc_Bank(MCamMoveState *state)
{
    float start;
    float ratio;

    ratio = MCamMoveProc_CalcRatio(state->mode, ++state->elapsed, state->duration);
    start = state->start;
    state->current = start + (state->end - start) * ratio;
    return state->elapsed < state->duration;
}

static int MCamMoveProc_Pers(MCamMoveState *state)
{
    float start;
    float ratio;

    ratio = MCamMoveProc_CalcRatio(state->mode, ++state->elapsed, state->duration);
    start = state->start;
    state->current = start + (state->end - start) * ratio;
    return state->elapsed < state->duration;
}

extern float sinf(float x);

static void MCamShakeProc(void)
{
    float v;
    float d;
    float phase;

    if (D_00A5B610.unk18 != 0) {
        D_00A5B610.unk18 = D_00A5B610.unk18 - 1;
        if (D_00A5B610.unk14 != 0.0f) {
            phase = fmodf(D_00A5B610.unk10 + D_00A5B610.unk14, 6.283185f);
            D_00A5B610.unk10 = phase;
            D_00A5B610.unkC = D_00A5B610.unk4 * sinf(phase);
            return;
        }
        v = D_00A5B610.unk4;
        if (!(D_00A5B610.unkC < 0.0f)) {
            v = -v;
        }
        D_00A5B610.unkC = v;
        return;
    } else if (D_00A5B610.unk1C != 0) {
        d = D_00A5B610.unk4 - D_00A5B610.unk8;
        D_00A5B610.unk1C = D_00A5B610.unk1C - 1;
        D_00A5B610.unk4 = d;
        if (D_00A5B610.unk14 != 0.0f) {
            phase = fmodf(D_00A5B610.unk10 + D_00A5B610.unk14, 6.283185f);
            D_00A5B610.unk10 = phase;
            D_00A5B610.unkC = D_00A5B610.unk4 * sinf(phase);
            return;
        }
        v = d;
        if (!(D_00A5B610.unkC < 0.0f)) {
            v = -v;
        }
        D_00A5B610.unkC = v;
        return;
    }
    D_00A5B610.unkC = 0.0f;
    camFlags &= ~0x1000;
}

/* Types recovered upstream (origin/master); kept for reference. */
/*
 * MCamInit clears this complete 0x5B0-byte camera-parameter store. The
 * recovered float and perspective-move views are interior fields; the other
 * bytes remain part of the live store but their individual meanings are not
 * recovered yet.
 */
typedef struct MCamParameterStorage {
    unsigned char unmodeled_000[0x1A0];
    float actorLength[2];
    unsigned char unmodeled_1A8[0x4C];
    float perspectiveStart;
    unsigned char unmodeled_1F8[0x380];
    MCamMoveState perspectiveMove;
    unsigned char unmodeled_590[0x20];
} MCamParameterStorage;


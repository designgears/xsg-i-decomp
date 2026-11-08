/*
 * OV01 original TU 29: 0x00a36be8..0x00a37ab8 (7 functions)
 */
#include "common.h"
#include "shared.h"
#include "main/m_math.h"
#include "m_ef_create_solb.h"

extern void *memset(void *destination, int value, unsigned int count);
extern void MEfGetActorCoord(void *dst, u32 actor, u32 coord);
static void makePath(void *work);
static void makeHermiteParam(int mode, void *work);
extern void MGsGPInit(void *work, int flags, int mode);
static void fnSOLB_PR000(void *self, void *work);
static void fnSOLB_DP000(void *self, void *work);
static void fnSOLB_PO000(void *self, void *work);

int MEfCreate_SOLB(void *work)
{
    unsigned char *base = (unsigned char *)work;
    SolbState *state = (SolbState *)(base + 32);

    state->age = 0;
    memset(state->active, 0, sizeof(state->active));
    memset(state->birth_age, 0, sizeof(state->birth_age));
    memset(state->trail, 0, sizeof(state->trail));
    MEfGetActorCoord(state->trail, state->actor, state->coord);
    state->segment = 0;
    state->frame = 0;
    makePath(state);
    makeHermiteParam(0, state);
    /* The MGs packet record follows the SOLB state, at state + 0x350. */
    MGsGPInit(base + 880, 0, 0);
    ((SOLBProcessSlot *)(base + 4))->process_callback = fnSOLB_PR000;
    ((SOLBDrawPacketSlot *)(base + 12))->draw_packet_callback = fnSOLB_DP000;
    ((SOLBPostProcessSlot *)(base + 16))->post_process_callback = fnSOLB_PO000;
    return 1;
}

extern void MEfGetActorMatrix(Matrix4 *matrix, u32 actor, u32 coord);
extern Vector4 *MMathApplyMatrix(Vector4 *out, Matrix4 *matrix, Vector4 *vector);
extern Matrix4 *MMathRotateMatrixX(Matrix4 *out, Matrix4 *matrix, float angle);
extern Matrix4 *MMathRotateMatrixY(Matrix4 *out, Matrix4 *matrix, float angle);
extern Matrix4 *MMathRotateMatrixZ(Matrix4 *out, Matrix4 *matrix, float angle);
extern float srsAtan2(float deltaX, float deltaZ);
const int timetbl[4] = { 5, 6, 12, 23 };

static void makePath(void *work)
{
    static const Vector4 offset[SOLB_CONTROL_POINTS] = {
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 1.7f, 0.0f, 0.5f, 1.0f },
        { 2.0f, 0.0f, 3.6f, 1.0f },
        { 2.0f, 0.0f, 20.0f, 1.0f },
    };
    SolbState *state = (SolbState *)work;
    Vector4 direction;
    Matrix4 matrix;
    Vector4 worldOrigin;
    Matrix4 actorMatrix;
    Matrix4 pitchMatrix;
    Matrix4 yawMatrix;
    Matrix4 headingMatrix;
    float sweepStep;
    float heading;
    int i;

    MEfGetActorMatrix(&actorMatrix, state->actor, state->coord);
    MMathApplyMatrix(&worldOrigin, &actorMatrix, &state->localOffset);
    __asm__ __volatile__("lqc2 $vf1, 0(%0)" : : "r"(&worldOrigin) : "memory");
    __asm__ __volatile__("vmove.w $vf1, $vf0" : : : "memory");
    __asm__ __volatile__("sqc2 $vf1, 48(%0)" : : "r"(&actorMatrix) : "memory");

    __asm__ __volatile__(
        "lq $8, 0(%1)\n\t"
        "lq $9, 16(%1)\n\t"
        "lq $10, 32(%1)\n\t"
        "lq $11, 48(%1)\n\t"
        "sq $8, 0(%0)\n\t"
        "sq $9, 16(%0)\n\t"
        "sq $10, 32(%0)\n\t"
        "sq $11, 48(%0)"
        : : "r"(&matrix), "r"(&actorMatrix)
        : "$8", "$9", "$10", "$11", "memory");
    __asm__ __volatile__("sqc2 $vf0, 0(%0)" : : "r"(&direction) : "memory");
    __asm__ __volatile__("lqc2 $vf1, 0(%0)" : : "r"(&direction) : "memory");
    __asm__ __volatile__("vmove.w $vf1, $vf0" : : : "memory");
    __asm__ __volatile__("sqc2 $vf1, 48(%0)" : : "r"(&matrix) : "memory");

    direction.x = 1.0f;
    MMathApplyMatrix(&direction, &matrix, &direction);
    MMathRotateMatrixY(&matrix, (Matrix4 *)0, -state->actorAngle->yaw);
    MMathApplyMatrix(&direction, &matrix, &direction);

    heading = srsAtan2(direction.y, direction.x);
    sweepStep = (float)state->sweepDegrees * 0.017453292f;
    sweepStep /= 3.0f;
    MMathRotateMatrixX(&pitchMatrix, (Matrix4 *)0, 0.13962634f);
    MMathRotateMatrixY(&yawMatrix, (Matrix4 *)0, state->actorAngle->yaw);
    MMathRotateMatrixZ(&headingMatrix, (Matrix4 *)0, heading);

    for (i = 0; i < SOLB_CONTROL_POINTS; i++) {
        MMathApplyMatrix(&direction, &headingMatrix, &offset[i]);
        MMathApplyMatrix(&direction, &pitchMatrix, &direction);
        MMathApplyMatrix(&direction, &yawMatrix, &direction);
        __asm__ __volatile__("lqc2 $vf1, 0(%0)" : : "r"(&direction) : "memory");
        __asm__ __volatile__("lqc2 $vf2, 0(%0)" : : "r"(&worldOrigin) : "memory");
        __asm__ __volatile__("vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz" : : : "memory");
        __asm__ __volatile__("sqc2 $vf1, 0(%0)" : : "r"(&state->control[i]) : "memory");
        MMathRotateMatrixY(&yawMatrix, &yawMatrix, sweepStep);
    }
}

/* The parameter routine writes both tangents for the current path segment. */
extern void MMathCalcHermitePrm(HermiteVector *tangent_start, HermiteVector *tangent_end,
                                const HermiteVector *point_previous, const HermiteVector *point_start,
                                const HermiteVector *point_end, const HermiteVector *point_next);

static void makeHermiteParam(int segment, void *work)
{
    SolbState *state = (SolbState *)work;
    HermiteVector *point_previous;
    HermiteVector *point_start;
    HermiteVector *point_end;
    HermiteVector *point_next;

    if (segment == 0) {
        point_previous = &state->control[0];
    } else {
        point_previous = &state->control[segment - 1];
    }
    point_start = &state->control[segment];
    point_end = &state->control[segment + 1];
    if (segment < 2) {
        point_next = &state->control[segment + 2];
    } else {
        point_next = point_end;
    }
    MMathCalcHermitePrm(&state->tangent_at_segment_start, &state->tangent_at_segment_end,
                        point_previous, point_start, point_end, point_next);
}

/* makeHermiteCoord: corrected tangent/endpoint attribution.
 *
 * Proof of the corrected names from the actual helper bodies:
 * - MMathCalcHermitePrm (main 0x002ef1d0) takes its two outputs in a0/a1 and
 *   stores (vf3-vf1)*k to (a0) and (vf4-vf2)*k to (a1). Its caller
 *   makeHermiteParams passes effect+0x330 in a0 and effect+0x340 in a1, so
 *   +0x330/+0x340 hold the computed segment TANGENTS, not endpoints.
 * - MMathCalcHermite (main 0x002ef160) loads its middle operand vectors from
 *   (a1)/(a2) and weights them with the parameter-derived tangent
 *   coefficients, while the (a3)/(t0) vectors take the endpoint weights; the
 *   caller passes +0x330/+0x340 in a1/a2 and the segment-indexed +0x2f0/+0x300
 *   pair in a3/t0. Hence a1/a2 are tangent_at_segment_start/end and a3/t0
 *   are control_at_segment_start/end. Start/end ordering within each pair is
 *   unchanged from the prior exact form; only the category names and this
 *   comment are corrected. Codegen is unchanged (LOCAL binding preserved).
 *
 * Those four addresses are now the SolbState members tangent_at_segment_start,
 * tangent_at_segment_end and control[segment]/control[segment + 1]; the effect
 * this is called with is the same record MEfCreate_SOLB sets up, which
 * fnSOLB_PR000 (0x00a37010) proves by passing it in a1 to both.
 */

static void makeHermiteCoord(float *destination, void *effect)
{
    SolbState *state = (SolbState *)effect;
    short segment_index = state->segment;
    short frame_index = state->frame;

    MMathCalcHermite(destination, (float)frame_index / (float)timetbl[segment_index],
                     &state->tangent_at_segment_start,
                     &state->tangent_at_segment_end,
                     &state->control[segment_index],
                     &state->control[segment_index + 1]);
}

static void fnSOLB_PR000(void *self, void *work)
{
    SolbState *state = (SolbState *)work;
    int i;

    for (i = SOLB_TRAIL_LENGTH - 2; i >= 0; i--) {
        state->active[i + 1] = state->active[i];
        state->birth_age[i + 1] = state->birth_age[i];
        __asm__ volatile("lq $8, 0(%1)\n\tsq $8, 0(%0)" : : "r"(&state->trail[i + 1]), "r"(&state->trail[i]) : "$8", "memory");
    }
    state->active[0] = 0;
    state->birth_age[0] = 0;
    __asm__ volatile("sqc2 $vf0, 0(%0)" : : "r"(&state->trail[0]) : "memory");
    if (state->age < 23) {
        state->birth_age[0] = state->age;
        state->active[0] = 1;
        makeHermiteCoord((float *)&state->trail[0], state);
        state->frame++;
        if (state->frame >= timetbl[state->segment]) {
            state->frame = 0;
            state->segment++;
            makeHermiteParam(state->segment, state);
        }
    }
}

typedef unsigned int Quadword __attribute__((mode(TI)));
typedef unsigned int MEfGsQuadword __attribute__((mode(TI)));
typedef union MEfGsQword {
    Quadword whole;
    unsigned short nloop;
    struct { u64 tag; u64 registers; } gif;
    struct { u64 data; unsigned char address; } ad;
    u32 channel[4];
    struct { int r, g, b, a; } rgba;
    struct { int x, y, z, adc; } xyz;
    float element[4];
    Vector4 vector;
} MEfGsQword;
typedef struct MEfGsPacket {
    void *data;
    int capacity;
    MEfGsQword *current;
    int count;
    int reg_count;
} MEfGsPacket;
#define MEF_GS_SETUP_TAG 0x1000000000008000
#define MEF_GS_SETUP_REGS 0xE
#define MEF_GS_STRIP_PRIM 0x4C
#define MEF_GS_STRIP_REGS 0x5151
#define MEF_GS_XYZ_ADC 0x8000
#define MEF_GS_TEST_1_ALPHA_GREATER_ZGEQUAL 0x5000D
#define MEF_GS_ZBUF_1_PSMZ24_NO_DEPTH_WRITE 0x131000000
#define MEF_GS_ALPHA_1_ADDITIVE 0x48
#define GIF_TAG_EOP 0x8000
#define SCE_GS_ALPHA_1 0x42
#define SCE_GS_TEST_1 0x47
#define SCE_GS_ZBUF_1 0x4E
#define QuadwordCopyT0(destination, source) \
    __asm__ __volatile__("lq $8, 0(%1)\n" "sq $8, 0(%0)\n" : : "r" (destination), "r" (source) : "$8", "memory")
#define Vu0SetOrigin(destination) \
    __asm__ __volatile__("sqc2 vf0, 0(%0)\n" : : "r" (destination) : "memory")
typedef struct MEfGsCamParams {
    StudioCamera *screen;
    Matrix4 *matrix;
} MEfGsCamParams;
#define Vu0Sin(result, angle) \
    __asm__ __volatile__( \
        "mfc1 $8, %1\n" \
        "qmtc2 $8, vf4\n" \
        "vcallms 0x20\n" \
        "qmfc2.i $8, vf1\n" \
        "mtc1 $8, %0\n" \
        : "=f" (result) \
        : "f" (angle) \
        : "$8", "memory")
#define Vu0Cos(result, angle) \
    __asm__ __volatile__( \
        "mfc1 $8, %1\n" \
        "qmtc2 $8, vf4\n" \
        "vcallms 0xe8\n" \
        "qmfc2.i $8, vf1\n" \
        "mtc1 $8, %0\n" \
        : "=f" (result) \
        : "f" (angle) \
        : "$8", "memory")
typedef struct SolbDrawState {
    unsigned int : 32;                  /* +0x000 */
    SolbActorAngle *actorAngle;         /* +0x004 */
    void *actor;                       /* +0x008 */
    u32 coord;                          /* +0x00c */
    Vector4 localOffset;                /* +0x010 */
    short sweepDegrees;                 /* +0x020 */
    unsigned short : 16;                /* +0x022 */
    unsigned int : 32;                  /* +0x024 */
    unsigned int : 32;                  /* +0x028 */
    unsigned int : 32;                  /* +0x02c */
    unsigned int : 32;                  /* +0x030 */
    unsigned int : 32;                  /* +0x034 */
    unsigned int : 32;                  /* +0x038 */
    unsigned int : 32;                  /* +0x03c */
    unsigned int : 32;                  /* +0x040 */
    unsigned int : 32;                  /* +0x044 */
    unsigned int : 32;                  /* +0x048 */
    unsigned int : 32;                  /* +0x04c */
    unsigned int : 32;                  /* +0x050 */
    unsigned int : 32;                  /* +0x054 */
    unsigned int : 32;                  /* +0x058 */
    unsigned int : 32;                  /* +0x05c */
    unsigned int : 32;                  /* +0x060 */
    unsigned int : 32;                  /* +0x064 */
    unsigned int : 32;                  /* +0x068 */
    unsigned int : 32;                  /* +0x06c */
    short age;                          /* +0x070: frames since creation */
    short active[SOLB_TRAIL_LENGTH];    /* +0x072 */
    short birth_age[SOLB_TRAIL_LENGTH]; /* +0x0b0: age when the entry was made */
    unsigned short : 16;                /* +0x0ee */
    Vector4 trail[SOLB_TRAIL_LENGTH];               /* +0x0f0 */
    short segment;                      /* +0x2e0: current path segment */
    short frame;                        /* +0x2e2: frame inside that segment */
    unsigned int : 32;                  /* +0x2e4 */
    unsigned int : 32;                  /* +0x2e8 */
    unsigned int : 32;                  /* +0x2ec */
    HermiteVector control[SOLB_CONTROL_POINTS];     /* +0x2f0 */
    HermiteVector tangent_at_segment_start;         /* +0x330 */
    HermiteVector tangent_at_segment_end;           /* +0x340 */
    MEfGsPacket packet;                             /* +0x350 */
} SolbDrawState;
extern Vector4 *MEfCalcAngle(Vector4 *destination, const Vector4 *from, const Vector4 *to);
extern float MMathMakeRandom2PI(void);
extern int MMathRotTransPersClip(Vector4 *destination, const StudioCamera *camera,
                                 Matrix4 *matrix, const Vector4 *point);
extern void MGsGPTerm(MEfGsPacket *packet);
typedef struct SolbCamParams { StudioCamera *screen; Matrix4 *matrix; } SolbCamParams;
extern SolbCamParams mefCamParams;
#define SOLB_RIBBON_PRIM 0x4C
#define SOLB_LINE_PRIM 0x4A
#define SOLB_RIBBON_REGS 0x551
#define SOLB_LINE_REGS 0x51
#define SOLB_FIBER_COUNT 6
#define SOLB_TRAIL_DRAWN 30
static inline MEfGsQword *solbOpenTag(MEfGsPacket *packet, int pre, int prim, int reg_count,
                                      u64 registers)
{
    MEfGsQword *qword = packet->current;

    qword->gif.tag = GIF_TAG_EOP | ((u64)pre << 46) | ((u64)prim << 47) | ((u64)reg_count << 60);
    qword->gif.registers = registers;
    packet->reg_count = reg_count;
    return qword + 1;
}
static inline void solbSetRegister(MEfGsQword *qword, unsigned char address, u64 data)
{
    qword->ad.data = data;
    qword->ad.address = address;
}
static inline void solbCloseTag(MEfGsPacket *packet, int nloop)
{
    int size;

    size = packet->reg_count * nloop + 1;
    packet->current->nloop |= nloop;
    packet->current += size;
    packet->count += size;
}

extern Quadword coltbl_1_00A43780[3];

static void fnSOLB_DP000(void *self, void *work)
{
    SolbDrawState *state = (SolbDrawState *)work;
    MEfGsQword color;
    Matrix4 matrix;
    Vector4 aim;
    Vector4 lead;
    MEfGsQword vertex[2];
    MEfGsPacket *packet;
    Vector4 *point;
    StudioCamera *screen;
    Matrix4 *view;
    MEfGsQword *qword;
    float width;
    int *adc;
    int alpha;
    int clip;
    int drawn;
    int pass;
    int i;
    int j;

    screen = mefCamParams.screen;
    view = mefCamParams.matrix;
    point = (Vector4 *)&state->packet;
    MGsGPInit(point, 0, 0);
    {
    MEfGsQword *setting = ((MEfGsPacket *)point)->current;
    setting[0].gif.tag = MEF_GS_SETUP_TAG;
    setting[0].gif.registers = MEF_GS_SETUP_REGS;
    ((MEfGsPacket *)point)->reg_count = 1;
    qword = &setting[1];
    solbSetRegister(qword, SCE_GS_TEST_1, MEF_GS_TEST_1_ALPHA_GREATER_ZGEQUAL);
    solbSetRegister(&setting[2], SCE_GS_ZBUF_1, MEF_GS_ZBUF_1_PSMZ24_NO_DEPTH_WRITE);
    solbSetRegister(&setting[3], SCE_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    solbCloseTag((MEfGsPacket *)point, 3);
    }
    point = *(Vector4 **)point;
    adc = &vertex[1].xyz.adc;

    color.whole = coltbl_1_00A43780[0];
    for (pass = 0; pass < 2; pass++) {
        clip = 0;
        drawn = 0;
        qword = solbOpenTag(&state->packet, 1, SOLB_RIBBON_PRIM, 3, SOLB_RIBBON_REGS);
        {
            float x = point->x;

            point->x = x;
        }
        Vu0SetOrigin(&lead);
        for (i = 0; i < SOLB_TRAIL_DRAWN; i++) {
            if (state->active[i] == 0) {
                continue;
            }
            if (i == SOLB_TRAIL_DRAWN - 1) {
                color.rgba.a = 0;
            } else {
                alpha = 256 - state->birth_age[i] * 11;
                if (alpha < 0) {
                    alpha = 0;
                }
                color.rgba.a = alpha;
            }
            if (i != 0 && i != SOLB_TRAIL_DRAWN - 1) {
                width = 0.02f;
            } else {
                width = 0.0f;
            }
            point = &state->trail[i];
            MEfCalcAngle(&aim, point, &state->trail[i + 1]);
            MMathRotateMatrixYXZ(&matrix, 0, &aim);
            __asm__ __volatile__(
                "lqc2 $vf1, 0(%1)\n"
                "vmove.w $vf1w, $vf0w\n"
                "sqc2 $vf1, 48(%0)\n"
                : : "r"(&matrix), "r"(point) : "memory");

            (&lead.x)[pass] = width;
            MMathApplyMatrix(&aim, &matrix, &lead);
            if (MMathRotTransPersClip(&vertex[0].vector, screen, view, &aim)) {
                clip = 2;
            } else if (clip != 0) {
                clip--;
                vertex[0].xyz.adc = MEF_GS_XYZ_ADC;
            }
            (&lead.x)[pass] = -width;
            MMathApplyMatrix(&aim, &matrix, &lead);
            if (MMathRotTransPersClip(&vertex[1].vector, screen, view, &aim)) {
                clip = 2;
            } else if (clip != 0) {
                clip--;
                *adc = MEF_GS_XYZ_ADC;
            }
            drawn++;
            qword[0].whole = color.whole;
            qword[1].whole = vertex[0].whole;
            qword[2].whole = vertex[1].whole;
            qword += 3;
        }
        packet = &state->packet;
        if (drawn > 0) {
            solbCloseTag(packet, drawn);
        }
    }

    {
    MEfGsQword *setting = packet->current;
    setting[0].gif.tag = MEF_GS_SETUP_TAG;
    setting[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->reg_count = 1;
    qword = &setting[1];
    solbSetRegister(qword, SCE_GS_TEST_1, MEF_GS_TEST_1_ALPHA_GREATER_ZGEQUAL);
    solbSetRegister(&setting[2], SCE_GS_ZBUF_1, MEF_GS_ZBUF_1_PSMZ24_NO_DEPTH_WRITE);
    solbSetRegister(&setting[3], SCE_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    solbCloseTag(packet, 3);
    }
    color.whole = coltbl_1_00A43780[1];
    for (pass = 0; pass < SOLB_FIBER_COUNT; pass++) {
        clip = 0;
        drawn = 0;
        qword = solbOpenTag(&state->packet, 1, SOLB_LINE_PRIM, 2, SOLB_LINE_REGS);
        for (j = 0; j < SOLB_TRAIL_DRAWN; j++) {
            if (state->active[j] == 0) {
                continue;
            }
            if (j == SOLB_TRAIL_DRAWN - 1) {
                color.rgba.a = 0;
            } else {
                color.rgba.a = 256 - state->birth_age[j] * 11;
                if (color.rgba.a < 48) {
                    color.rgba.a = 48;
                }
            }
            point = &state->trail[j];
            MEfCalcAngle(&aim, point, &state->trail[j + 1]);
            MMathRotateMatrixYXZ(&matrix, 0, &aim);
            __asm__ __volatile__(
                "lqc2 $vf1, 0(%1)\n"
                "vmove.w $vf1w, $vf0w\n"
                "sqc2 $vf1, 48(%0)\n"
                : : "r"(&matrix), "r"(point) : "memory");
            point = &state->trail[j + 2];
            {
                float x = point->x;

                point->x = x;
            }
            Vu0SetOrigin(&aim);
            if (j != 0 && j < SOLB_TRAIL_DRAWN - 1) {
                width = (float)j * 0.2094395f;
                width = width + (float)pass * 1.0471975f;
                Vu0Cos(aim.x, width);
                aim.x = aim.x * 0.1f;
                Vu0Sin(aim.y, width);
                aim.y = aim.y * 0.1f;
            }
            MMathApplyMatrix(&aim, &matrix, &aim);
            if (MMathRotTransPersClip(&vertex[0].vector, screen, view, &aim)) {
                clip = 1;
            } else if (clip != 0) {
                clip--;
                vertex[0].xyz.adc = MEF_GS_XYZ_ADC;
            }
            drawn++;
            qword[0].whole = color.whole;
            qword[1].whole = vertex[0].whole;
            qword += 2;
        }
        if (drawn > 0) {
            solbCloseTag(packet, drawn);
        }
    }

    {
    MEfGsQword *setting = packet->current;
    setting[0].gif.tag = MEF_GS_SETUP_TAG;
    setting[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->reg_count = 1;
    qword = &setting[1];
    solbSetRegister(qword, SCE_GS_TEST_1, MEF_GS_TEST_1_ALPHA_GREATER_ZGEQUAL);
    solbSetRegister(&setting[2], SCE_GS_ZBUF_1, MEF_GS_ZBUF_1_PSMZ24_NO_DEPTH_WRITE);
    solbSetRegister(&setting[3], SCE_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    solbCloseTag(packet, 3);
    }
    color.whole = coltbl_1_00A43780[1];
    for (pass = 0; pass < 2; pass++) {
        drawn = 0;
        clip = 0;
        qword = solbOpenTag(&state->packet, 1, SOLB_LINE_PRIM, 2, SOLB_LINE_REGS);
        for (j = 0; j < SOLB_TRAIL_DRAWN; j++) {
            if (state->active[j] == 0) {
                continue;
            }
            if (j == SOLB_TRAIL_DRAWN - 1) {
                color.rgba.a = 0;
            } else {
                color.rgba.a = 256 - state->birth_age[j] * 11;
                if (color.rgba.a < 48) {
                    color.rgba.a = 48;
                }
            }
            point = &state->trail[j];
            MEfCalcAngle(&aim, point, &state->trail[j + 1]);
            MMathRotateMatrixYXZ(&matrix, 0, &aim);
            __asm__ __volatile__(
                "lqc2 $vf1, 0(%1)\n"
                "vmove.w $vf1w, $vf0w\n"
                "sqc2 $vf1, 48(%0)\n"
                : : "r"(&matrix), "r"(point) : "memory");
            point = &state->trail[j + 2];
            {
                float x = point->x;

                point->x = x;
            }
            {
                float y = point->y;

                point->y = y;
            }
            {
                float z = point->z;

                point->z = z;
            }
            Vu0SetOrigin(&aim);
            if (j != 0 && j < SOLB_TRAIL_DRAWN - 1) {
                width = MMathMakeRandom2PI();
                Vu0Cos(aim.x, width);
                aim.x = aim.x * 0.25f;
                Vu0Sin(aim.y, width);
                aim.y = aim.y * 0.25f;
            }
            MMathApplyMatrix(&aim, &matrix, &aim);
            if (MMathRotTransPersClip(&vertex[0].vector, screen, view, &aim)) {
                clip = 1;
            } else if (clip != 0) {
                clip--;
                vertex[0].xyz.adc = MEF_GS_XYZ_ADC;
            }
            drawn++;
            qword[0].whole = color.whole;
            qword[1].whole = vertex[0].whole;
            qword += 2;
        }
        if (drawn > 0) {
            solbCloseTag(packet, drawn);
        }
    }
    MGsGPTerm(packet);
    drawn = *(int *)point;
    drawn++;
}

/* MEfObjDestroy is defined by src/main/m_ef_obj.c; sefHitEffect
 * is defined by src/main/sef.c. */

/* MEfObjDestroy (src/main/m_ef_obj.c) and sefHitEffect (src/main/sef.c) are
 * still asm in their defining TU; declared locally until published there. */
extern void MEfObjDestroy(void *self);
extern void sefHitEffect(void);

static void fnSOLB_PO000(void *self, void *work)
{
    SolbState *state = (SolbState *)work;

    state->age++;
    if (state->age == 0x17) {
        sefHitEffect();
    }
    if (state->age >= 0x35) {
        MEfObjDestroy(self);
    }
}

u32 coltbl_1_def[3][4] __asm__("coltbl_1_00A43780") = {
    { 0xA0, 0xA0, 0xA0, 0xFF },
    { 0x00, 0x20, 0xA0, 0xFF },
    { 0xC0, 0x80, 0x20, 0xFF },
};

/*
 * OV01 original TU 30: 0x00a37ab8..0x00a38190 (4 functions)
 */
#include "common.h"
#include "shared.h"

typedef unsigned int Quadword __attribute__((mode(TI)));

typedef union ECM01QuadVector {
    unsigned int words[4];
    Quadword quad;
} ECM01QuadVector;

typedef struct ECM01Packet {
    void *data;
    int capacity;
    void *current;
    int count;
    int aux_count;
} ECM01Packet;

typedef struct ECM01Work {
    unsigned char unmodeled_00[8];
    unsigned int actor_id;
    unsigned int actor_part;
    Vector4 creation_offset;
    unsigned char unmodeled_20[4];
    unsigned int coordinate_actor_id;
    unsigned int coordinate_part;
    unsigned char unmodeled_2C[0x70 - 0x2C];
    int frame;
    unsigned char unmodeled_74[0x80 - 0x74];
    Vector4 position;
    Vector4 actor_coordinates;
    Vector4 position_delta;
    Vector4 angles;
    float size;
    float size_step;
    float initial_angle;
    float angle_step;
    ECM01QuadVector dimensions;
    ECM01QuadVector vector_origin;
    short visibility[31];
    short frame_age[31];
    ECM01Packet packet;
} ECM01Work;

typedef void ECM01Callback(void *self, void *work);

typedef struct ECM01Object {
    unsigned char unmodeled_00[4];
    ECM01Callback *process_callback;
    unsigned char unmodeled_08[4];
    ECM01Callback *draw_callback;
    ECM01Callback *post_callback;
    unsigned char unmodeled_14[0x20 - 0x14];
    ECM01Work work;
} ECM01Object;

extern void MEfGetActorMatrix(Matrix4 destination, unsigned int actor_id,
                              unsigned int part);
extern void MEfGetActorCoord(void *destination, unsigned int actor_id,
                             unsigned int part);
extern void MMathApplyMatrix(Vector4 *destination, Matrix4 matrix,
                             Vector4 *point);
extern Vector4 *MMathSubVectorDivS(Vector4 *destination, const Vector4 *first,
                                   const Vector4 *second, float scale);
extern void *MEfCalcAngle(Vector4 *destination, const Vector4 *from,
                          const Vector4 *to);
extern float MMathMakeRandom2PI(void);
extern void MGsGPInit(void *packet, int address, int size);

static void fnECM01_PR000(void *self, void *work);
static void fnECM01_DP000(void *self, void *work);
static void fnECM01_PO000(void *self, void *work);

int MEfCreate_ECM01(ECM01Object *object)
{
    ECM01Work *work = &object->work;
    Matrix4 actor_matrix;
    float angle_range;
    Quadword initial_dimensions;

    work->frame = 0;
    MEfGetActorMatrix(actor_matrix, work->actor_id, work->actor_part);
    MMathApplyMatrix(&work->position, actor_matrix, &work->creation_offset);
    MEfGetActorCoord(&work->actor_coordinates, work->coordinate_actor_id,
                     work->coordinate_part);
    MMathSubVectorDivS(&work->position_delta, &work->actor_coordinates,
                       &work->position, 30.0f);
    MEfCalcAngle(&work->angles, &work->position, &work->actor_coordinates);

    work->size = 0.1f;
    work->size_step = 0.06f;
    work->initial_angle = (float)(xglSRand() & 1) * 3.1415927f;
    angle_range = MMathMakeRandom2PI() + 6.283185f;
    /* The packed dimension words, from low to high: 24, 48, 24, 255. */
    initial_dimensions = (Quadword)0xFF000000180000003000000018;
    work->vector_origin.quad = 0;
    work->dimensions.quad = initial_dimensions;
    work->angle_step = angle_range / 30.0f;
    work->dimensions.words[0] = (xglSRand() & 0xF) + 16;
    work->dimensions.words[2] = (xglSRand() & 0xF) + 16;

    memset(work->visibility, 0, sizeof(work->visibility));
    memset(work->frame_age, 0, sizeof(work->frame_age));
    MGsGPInit(&work->packet, 0, 0);

    object->process_callback = fnECM01_PR000;
    object->draw_callback = fnECM01_DP000;
    object->post_callback = fnECM01_PO000;
    return 1;
}

/* This callback advances the per-frame state and its 31-entry history. */
#define ECM01_ACTIVE_FRAMES 60
#define ECM01_HISTORY_COUNT 31
#define ECM01_ENDPOINT_MARKER 2

typedef struct ECM01Trail {
    unsigned char unmodeled_00[0x70];
    int frame;                         /* +0x70 */
    unsigned char unmodeled_74[0xF0 - 0x74];
    short visibility[ECM01_HISTORY_COUNT]; /* +0xF0 */
    short frame_age[ECM01_HISTORY_COUNT];   /* +0x12E */
} ECM01Trail;

static void fnECM01_PR000(void *self, void *work)
{
    ECM01Trail *trail = (ECM01Trail *)work;
    int i;

    for (i = ECM01_HISTORY_COUNT - 1; i > 0; i--) {
        trail->visibility[i] = trail->visibility[i - 1];
        trail->frame_age[i] = trail->frame_age[i - 1];
    }

    trail->visibility[0] = 0;
    trail->frame_age[0] = 0;
    if (trail->frame < ECM01_ACTIVE_FRAMES) {
        if (trail->frame == 0 || trail->frame == ECM01_ACTIVE_FRAMES - 1) {
            trail->visibility[0] = ECM01_ENDPOINT_MARKER;
        } else {
            trail->visibility[0] = 1;
        }
        trail->frame_age[0] = (short)trail->frame;
    }
}

typedef unsigned int ECMQuadword __attribute__((mode(TI)));
typedef union MEfGsQword {
    ECMQuadword whole;
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
typedef struct ECM01DWork {
    unsigned char unmodeled_00[0x70];
    int frame;
    unsigned char unmodeled_74[0x0c];
    Vector4 position;
    Vector4 actor_coordinates;
    Vector4 position_delta;
    Vector4 angles;
    float size;
    float size_step;
    float initial_angle;
    float angle_step;
    ECM01QuadVector dimensions;
    ECM01QuadVector vector_origin;
    short visibility[31];
    short frame_age[31];
    MEfGsPacket packet;
} ECM01DWork;
extern MEfGsCamParams mefCamParams;
extern void MGsGPTerm(MEfGsPacket *packet);
extern int MMathRotTransPersClip(Vector4 *destination, const StudioCamera *camera,
                                 Matrix4 *matrix, const Vector4 *point);
extern Matrix4 *MMathRotateMatrixYX(Matrix4 *out, const Matrix4 *matrix, const Vector4 *angles);
extern Vector4 *ECM01ApplyMatrix(Vector4 *destination, const Matrix4 *matrix, const Vector4 *point) __asm__("MMathApplyMatrix");
#define Vu0SefAddVectorXYZ(destination, first, second) \
    __asm__ __volatile__("lqc2 vf1, 0(%1)\n" "lqc2 vf2, 0(%2)\n" "vadd.xyz vf1, vf1, vf2\n" "sqc2 vf1, 0(%0)\n" \
        : : "r" (destination), "r" (first), "r" (second) : "memory")
#define ECMW ((ECM01DWork *)vwork)
static inline MEfGsQword *ecm01OpenTag(MEfGsPacket *packet, int pre, int prim, int reg_count,
                                       u64 registers)
{
    MEfGsQword *qword = packet->current;

    qword->gif.tag = GIF_TAG_EOP | ((u64)pre << 46) | ((u64)prim << 47) | ((u64)reg_count << 60);
    qword->gif.registers = registers;
    packet->reg_count = reg_count;
    return qword + 1;
}
static inline void ecm01SetRegister(MEfGsQword *qword, unsigned char address, u64 data)
{
    qword->ad.data = data;
    qword->ad.address = address;
}
static inline void ecm01CloseTag(MEfGsPacket *packet, int nloop)
{
    int size;

    size = packet->reg_count * nloop + 1;
    packet->current->nloop |= nloop;
    packet->current += size;
    packet->count += size;
}

static void fnECM01_DP000(void *self, void *vwork)
{
    MEfGsQword colour[2];
    Vector4 currentPosition;
    Matrix4 matrix;
    Vector4 point;
    MEfGsQword vertex[2];
    StudioCamera *screen;
    Matrix4 *view;
    /* One pointer serves twice: the packet during setup, then the working point. */
    void *target;
    MEfGsPacket *packet;
    MEfGsQword *setting;
    MEfGsQword *qword;
    float radius;
    float width;
    float angle;
    int clip;
    int drawn;
    int side;
    int segment;

    screen = mefCamParams.screen;
    view = mefCamParams.matrix;
    target = &ECMW->packet;
    MGsGPInit(target, 0, 0);
    setting = ((MEfGsPacket *)target)->current;
    setting[0].gif.tag = MEF_GS_SETUP_TAG;
    setting[0].gif.registers = MEF_GS_SETUP_REGS;
    ((MEfGsPacket *)target)->reg_count = 1;
    ecm01SetRegister(&setting[1], SCE_GS_TEST_1, MEF_GS_TEST_1_ALPHA_GREATER_ZGEQUAL);
    ecm01SetRegister(&setting[2], SCE_GS_ZBUF_1, MEF_GS_ZBUF_1_PSMZ24_NO_DEPTH_WRITE);
    ecm01SetRegister(&setting[3], SCE_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    ecm01CloseTag(target, 3);
    packet = target;

    for (side = -1; side < 2; side += 2) {
        drawn = 0;
        colour[0].whole = ECMW->dimensions.quad;
        clip = 0;
        colour[1].whole = ECMW->vector_origin.quad;
        radius = ECMW->size;
        QuadwordCopyT0(&currentPosition, &ECMW->position);
        qword = ecm01OpenTag(&ECMW->packet, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
        ECMW->initial_angle = ECMW->angle_step * (float)ECMW->frame;
        for (segment = 0; segment < ECM01_HISTORY_COUNT;) {
            if (ECMW->visibility[segment] != 0) {
                width = (float)side * 0.3f;
                if (ECMW->visibility[segment] == ECM01_ENDPOINT_MARKER) {
                    width = 0.0f;
                }
                colour[0].channel[3] = 255 - segment * 8;
                if (ECMW->frame_age[segment] >= 40) {
                    colour[0].channel[3] = colour[0].channel[3] * (60 - ECMW->frame_age[segment]) / 20;
                }
                MMathRotateMatrixYX(&matrix, 0, &ECMW->angles);
                __asm__ __volatile__("lqc2 vf1, 0(%0)\n\tvmove.w vf1, vf0\n\tsqc2 vf1, 48(%1)"
                                     :
                                     : "r"(&currentPosition), "r"(&matrix)
                                     : "memory");
                target = &point;
                angle = ECMW->initial_angle + ECMW->angle_step * (float)(ECMW->frame - segment);
                Vu0SetOrigin(target);
                __asm__ __volatile__("mfc1 $8, %1\n\t"
                                     "qmtc2.ni $8, vf4\n\t"
                                     "vcallms 0x20\n\t"
                                     "qmfc2.i $8, vf1\n\t"
                                     "mtc1 $8, %0"
                                     : "=f"(angle)
                                     : "f"(angle)
                                     : "$8", "memory");
                point.y = radius * angle;
                ECM01ApplyMatrix(target, &matrix, target);
                __asm__ __volatile__("lqc2 vf1, 0(%0)\n\tvmove.w vf1, vf0\n\tsqc2 vf1, 48(%1)"
                                     :
                                     : "r"(target), "r"(&matrix)
                                     : "memory");
                __asm__ __volatile__("lqc2 vf1, 48(%1)\n\tvmove.w vf1, vf0\n\tsqc2 vf1, 0(%0)"
                                     :
                                     : "r"(target), "r"(&matrix)
                                     : "memory");
                if (MMathRotTransPersClip(&vertex[0].vector, screen, view, target)) {
                    clip = 2;
                } else if (clip != 0) {
                    clip--;
                    vertex[0].xyz.adc = MEF_GS_XYZ_ADC;
                }
                Vu0SetOrigin(target);
                point.y = width;
                ECM01ApplyMatrix(target, &matrix, target);
                if (MMathRotTransPersClip(&vertex[1].vector, screen, view, target)) {
                    clip = 2;
                } else if (clip != 0) {
                    clip--;
                    vertex[1].xyz.adc = MEF_GS_XYZ_ADC;
                }
                drawn++;
                qword[0].whole = colour[0].whole;
                qword[1].whole = vertex[0].whole;
                qword[2].whole = colour[1].whole;
                qword[3].whole = vertex[1].whole;
                qword += 4;
            }
            segment++;
            Vu0SefAddVectorXYZ(&currentPosition, &currentPosition, &ECMW->position_delta);
            radius += ECMW->size_step;
        }
        if (drawn > 0) {
            ecm01CloseTag(packet, drawn);
        }
    }
    MGsGPTerm(packet);
}

/* MEfObjDestroy (src/main/m_ef_obj.c) and sefHitEffect (src/main/sef.c) are
 * still asm in their defining TU; declared locally until published there. */
extern void MEfObjDestroy(void *self);
extern void sefHitEffect(void);

/* work's layout is unresolved beyond +0x70: the per-frame lifetime counter
 * this function advances, firing sefHitEffect at frame 0x3C and expiring
 * through MEfObjDestroy once it reaches 0x5A. */
typedef struct ECM01State {
    unsigned char unmodeled_00[0x70];
    int frame; /* +0x70 */
} ECM01State;

static void fnECM01_PO000(void *self, void *work)
{
    ECM01State *state = (ECM01State *)work;

    state->frame++;
    if (state->frame == 0x3C) {
        sefHitEffect();
    }
    if (state->frame >= 0x5A) {
        MEfObjDestroy(self);
    }
}

/*
 * OV01 original TU 23: 0x00a33f08..0x00a346c8 (5 functions)
 */
#include "common.h"
#include "shared.h"

extern Matrix4 *MMathRotateMatrixYXZ(Matrix4 *destination,
                                     const Matrix4 *source,
                                     const Vector4 *angles);
extern Matrix4 *MMathScaleMatrix(Matrix4 *destination,
                                 Matrix4 *matrix,
                                 const Vector4 *scale);
extern void MEfDrawModel(const Vector4 *place, int entry, const char *texture);
extern const Vector4 scale_0_00A51380;

extern void MEfGetActorMatrix(void *destination, u32 actor, u32 part);
extern void MEfGetActorCoord(void *destination, u32 actor, u32 part);
extern Vector4 *MEfCalcCircumXZ(void *destination, const void *center, float radius);
extern void *MMathApplyMatrix(void *destination, const void *matrix, const void *point);
extern Vector4 *MMathSubVectorDivS(void *destination, const void *first, const void *second, float divisor);
extern void *MEfCalcAngle(void *destination, const void *from, const void *to);
extern void MGsGPInit(void *packet, void *address, int size);
extern void *memset(void *destination, int value, unsigned int size);
void fnMSP00_PR000(void *self, void *work);
static void fnMSP00_DM000();
void fnMSP00_DP000(void *self, void *work);
static void fnMSP00_PO000(void *self, void *work);
typedef struct MSP00InitState {
    unsigned char unmodeled_00[8];
    int source_actor;              /* +0x08 */
    int source_part;               /* +0x0c */
    Vector4 source_offset;         /* +0x10 */
    unsigned char unmodeled_20[4];
    int coordinate_actor;          /* +0x24 */
    int coordinate_part;           /* +0x28 */
    unsigned char unmodeled_2c[0x70 - 0x2C];
    int frame;                     /* +0x70 */
    unsigned char unmodeled_74[0x80 - 0x74];
    Vector4 spawn_position;        /* +0x80 */
    Vector4 circumference_position;/* +0x90 */
    Vector4 position;              /* +0xa0 */
    Vector4 velocity;              /* +0xb0 */
    Vector4 previous_position;     /* +0xc0 */
    Vector4 angles;                /* +0xd0 */
    unsigned char packet[0x14];    /* +0xe0 */
    unsigned short active[10];     /* +0xf4 */
    unsigned char unmodeled_108[8];
    Vector4 points[30];            /* +0x110 */
    float twist[30];               /* +0x2f0 */
} MSP00InitState;
typedef struct MSP00Object {
    unsigned char unmodeled_00[4];
    void *process;
    void *draw;
    void *draw_packet;
    void *lifetime;
    unsigned char unmodeled_14[0x0C];
    MSP00InitState work;
} MSP00Object;

int MEfCreate_MSP00(MSP00Object *object)
{
    MSP00InitState *work = &object->work;
    Vector4 actor_coordinate[1];
    Vector4 actor_matrix[4];

    work->frame = 0;
    MEfGetActorMatrix(actor_matrix, work->source_actor, work->source_part);
    MMathApplyMatrix(&work->spawn_position, actor_matrix, &work->source_offset);
    MEfGetActorCoord(actor_coordinate, work->coordinate_actor, work->coordinate_part);
    MEfCalcCircumXZ(&work->circumference_position, actor_coordinate, 1.0f);
    __asm__ __volatile__("lq $8,0(%1)\n\tsq $8,0(%0)"
                         : : "r"(&work->position), "r"(&work->spawn_position)
                         : "$8", "memory");
    __asm__ __volatile__("lq $8,0(%1)\n\tsq $8,0(%0)"
                         : : "r"(&work->previous_position), "r"(&work->spawn_position)
                         : "$8", "memory");
    MMathSubVectorDivS(&work->velocity, &work->circumference_position,
                       &work->spawn_position, 18.0f);
    MEfCalcAngle(&work->angles, &work->circumference_position, &work->spawn_position);
    MGsGPInit(work->packet, 0, 0);
    memset(work->active, 0, sizeof(work->active));
    memset(work->points, 0, sizeof(work->points));
    memset(work->twist, 0, sizeof(work->twist));
    object->process = fnMSP00_PR000;
    object->draw = fnMSP00_DM000;
    object->draw_packet = fnMSP00_DP000;
    object->lifetime = fnMSP00_PO000;
    return 1;
}

extern float MMathMakeRandom(void);
extern float MMathMakeRandom2PI(void);
extern Vector4 *MMathVectorInterpolation(Vector4 *destination, const Vector4 *first,
                                         const Vector4 *second, float parameter);
extern void MMathRotateMatrixYX(Vector4 *destination, const Vector4 *source,
                               const Vector4 *angles);

void fnMSP00_PR000(void *self, void *arg)
{
    MSP00InitState *work = (MSP00InitState *)arg;
    Vector4 offset;
    Vector4 matrix[4];
    int i;
    int j;

    if (work->frame < 18) {
        __asm__ __volatile__("lq $8, 0(%1)\n\t"
                             "sq $8, 0(%0)"
                             :
                             : "r"(&work->previous_position), "r"(&work->position)
                             : "$8", "memory");
        __asm__ __volatile__("lqc2 vf1, 0(%0)\n\t"
                             "lqc2 vf2, 0(%1)\n\t"
                             "vadd.xyz vf1, vf1, vf2\n\t"
                             "sqc2 vf1, 0(%0)"
                             :
                             : "r"(&work->position), "r"(&work->velocity)
                             : "memory");
        work->angles.z += 0.5235988f;
    }
    for (i = 8; i >= 0; i--) {
        work->active[i + 1] = work->active[i];
        for (j = 0; j < 3; j++) {
            __asm__ __volatile__("lq $8, 0(%1)\n\t"
                                 "sq $8, 0(%0)"
                                 :
                                 : "r"(&work->points[(i + 1) * 3 + j]), "r"(&work->points[i * 3 + j])
                                 : "$8", "memory");
            work->twist[(i + 1) * 3 + j] = work->twist[i * 3 + j];
        }
    }
    work->active[0] = 0;
    for (j = 0; j < 3; j++) {
        __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&work->points[j]) : "memory");
    }
    if (work->frame < 18) {
        work->active[0] = 1;
        for (j = 0; j < 3; j++) {
            MMathVectorInterpolation(&offset, &work->position, &work->previous_position,
                                     (float)j / 3.0f);
            MMathRotateMatrixYX(matrix, 0, &work->angles);
            __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&offset) : "memory");
            __asm__ __volatile__("vmove.w vf1, vf0" : : : "memory");
            __asm__ __volatile__("sqc2 vf1, 48(%0)" : : "r"(matrix) : "memory");
            __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&offset) : "memory");
            offset.x = MMathMakeRandom() * 0.06f - 0.03f;
            offset.y = MMathMakeRandom() * 0.06f - 0.03f;
            MMathApplyMatrix(&work->points[j], matrix, &offset);
            work->twist[j] = MMathMakeRandom2PI();
        }
    }
}

typedef struct MSP00DrawState {
    unsigned char unmodeled_00[0x40];
    int entry;                 /* +0x40 */
    const char *texture;       /* +0x44 */
    unsigned char unmodeled_48[0x28];
    int frame;                 /* +0x70 */
    unsigned char unmodeled_74[0x2c];
    Vector4 position;          /* +0xa0 */
    unsigned char unmodeled_b0[0x20];
    Vector4 angles;             /* +0xd0 */
} MSP00DrawState;

static void fnMSP00_DM000(void *self, MSP00DrawState *work)
{
    Matrix4 matrix;

    if (work->frame < 18) {
        MMathRotateMatrixYXZ(&matrix, 0, &work->angles);
        MMathScaleMatrix(&matrix, &matrix, &scale_0_00A51380);
        __asm__ __volatile__(
            "lqc2 vf1, 0(%1)\n\t"
            "vmove.w vf1, vf0\n\t"
            "sqc2 vf1, 48(%0)\n\t"
            :
            : "r"(&matrix), "r"(&work->position)
            : "memory"
        );
        MEfDrawModel((const Vector4 *)&matrix, work->entry, work->texture);
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/m_ef_create_msp_00", fnMSP00_DP000);

/* MEfObjDestroy (src/main/m_ef_obj.c) and sefHitEffect (src/main/sef.c) are
 * still asm in their defining TU; declared locally until published there. */
extern void MEfObjDestroy(void *self);
extern void sefHitEffect(void);

/* work's layout is unresolved beyond +0x70: the per-frame lifetime counter
 * this function advances, firing sefHitEffect at frame 0x12 and expiring
 * through MEfObjDestroy once it reaches 0x1C. */
#define MSP00_HIT_FRAME 0x12
#define MSP00_LIFETIME  0x1C

typedef struct MSP00State {
    unsigned char unmodeled_00[0x70];
    int frame; /* +0x70 */
} MSP00State;

static void fnMSP00_PO000(void *self, void *work)
{
    MSP00State *state = (MSP00State *)work;

    state->frame++;
    if (state->frame == MSP00_HIT_FRAME) {
        sefHitEffect();
    }
    if (state->frame >= MSP00_LIFETIME) {
        MEfObjDestroy(self);
    }
}

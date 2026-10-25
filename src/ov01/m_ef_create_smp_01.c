/*
 * OV01 original TU 25: 0x00a34e90..0x00a358a0 (7 functions)
 */
#include "common.h"
#include "shared.h"
#include "m_ef_create_smp_01.h"

typedef struct Smp01GsParameter {
    unsigned long primitive_tag;
    unsigned char rest[8];
} Smp01GsParameter;
typedef struct Smp01CreateWork {
    unsigned char unmodeled_00[8];
    unsigned int source_actor;          /* 0x08 */
    unsigned int source_part;           /* 0x0C */
    Vector4 source_offset;              /* 0x10 */
    unsigned char unmodeled_20[4];
    unsigned int coordinate_actor;      /* 0x24 */
    unsigned char unmodeled_28[0x70 - 0x28];
    int frame;                          /* 0x70 */
    int phase;                          /* 0x74 */
    int phaseCount;                     /* 0x78 */
    unsigned char unmodeled_7C[4];
    Vector4 spawnPoint;                 /* 0x80 */
    Vector4 phaseStart;                 /* 0x90 */
    Vector4 targetPosition;             /* 0xA0 */
    Vector4 position;                   /* 0xB0 */
    Vector4 previousPosition;           /* 0xC0 */
    Vector4 angles;                     /* 0xD0 */
    float phaseAngle;                   /* 0xE0 */
    unsigned char packet[0x14];         /* 0xE4 */
    unsigned long parameter_first;      /* 0xF8 */
    unsigned long parameter_second;     /* 0x100 */
    unsigned char active[0x14];         /* 0x108 */
    unsigned char unmodeled_11C[4];
    unsigned char points[0x1E0];        /* 0x120 */
    unsigned char twist[0x78];          /* 0x300 */
} Smp01CreateWork;
typedef struct Smp01CreateObject {
    unsigned char unmodeled_00[4];
    void *process;                      /* 0x04 */
    void *draw;                         /* 0x08 */
    void *draw_packet;                  /* 0x0C */
    void *lifetime;                     /* 0x10 */
    unsigned char unmodeled_14[0x0C];
    Smp01CreateWork work;               /* 0x20 */
} Smp01CreateObject;
extern void MEfGetActorMatrix(void *destination, u32 actor, u32 part);
extern void MEfGetActorCoord(void *destination, u32 actor, u32 part);
extern Vector4 *MEfCalcCircumXZ(void *destination, const void *center, float radius);
extern void *MMathApplyMatrix(Vector4 *destination, const Vector4 *matrix,
                              const Vector4 *point);
extern void *MEfCalcAngle(Vector4 *destination, const Vector4 *from, const Vector4 *to);
extern float MMathMakeRandom2PI(void);
extern void MGsGPInit(void *packet, void *address, int size);
extern Smp01GsParameter *svGetPrmFromName(const char *name, Smp01GsParameter *output);
extern void *memset(void *destination, int value, unsigned int size);
extern const char D_00A51480[];
extern const char D_00A51490[];
static void fnSMP01_PR000(Smp01Object *object, Smp01Work *work);
static void fnSMP01_DM000(Smp01Object *self, Smp01Work *work);
static void fnSMP01_PO000(void *object, void *work);
extern void fnSMP01_DP000();

int MEfCreate_SMP01(Smp01CreateObject *object)
{
    Smp01CreateWork *work = &object->work;
    Smp01GsParameter parameter;
    Vector4 actor_coordinate[1];
    Vector4 actor_matrix[4];

    work->frame = 0;
    work->phase = 0;
    work->phaseCount = 0;
    MEfGetActorMatrix(actor_matrix, work->source_actor, work->source_part);
    MMathApplyMatrix(&work->spawnPoint, actor_matrix, &work->source_offset);
    MEfGetActorCoord(actor_coordinate, work->coordinate_actor, -1);
    MEfCalcCircumXZ(&work->targetPosition, actor_coordinate, 1.0f);
    __asm__ __volatile__("lq $8,0(%1)\n\tsq $8,0(%0)"
                         : : "r"(&work->position), "r"(&work->spawnPoint)
                         : "$8", "memory");
    __asm__ __volatile__("lq $8,0(%1)\n\tsq $8,0(%0)"
                         : : "r"(&work->previousPosition), "r"(&work->spawnPoint)
                         : "$8", "memory");
    MEfCalcAngle(&work->angles, &work->targetPosition, &work->spawnPoint);
    work->phaseAngle = MMathMakeRandom2PI();
    MGsGPInit(work->packet, 0, 0);
    svGetPrmFromName(D_00A51480, &parameter);
    work->parameter_first = parameter.primitive_tag;
    svGetPrmFromName(D_00A51490, &parameter);
    work->parameter_second = parameter.primitive_tag;
    memset(work->active, 0, sizeof(work->active));
    memset(work->points, 0, sizeof(work->points));
    memset(work->twist, 0, sizeof(work->twist));

    object->process = fnSMP01_PR000;
    object->draw = fnSMP01_DM000;
    object->draw_packet = fnSMP01_DP000;
    object->lifetime = fnSMP01_PO000;
    return 1;
}

extern float MMathMakeRandom(void);
extern Vector4 *MMathVectorInterpolation(Vector4 *destination, const Vector4 *first,
                                         const Vector4 *second, float parameter);
extern void MMathRotateMatrixYX(Vector4 *destination, const Vector4 *source,
                               const Vector4 *angles);
typedef struct Smp01SmokeWork {
    unsigned char head[0x108];         /* 0x000 */
    short count[10];                   /* 0x108 */
    unsigned char unmodeled_11C[4];    /* 0x11C */
    Vector4 smoke[10][3];              /* 0x120 */
    float rotation[10][3];             /* 0x300 */
} Smp01SmokeWork;

void updateSmoke(Smp01Work *work)
{
    Smp01SmokeWork *smoke = (Smp01SmokeWork *)work;
    Vector4 matrix[4];
    Vector4 offset;
    int i;
    int j;

    for (i = 8; i >= 0; i--) {
        smoke->count[i + 1] = smoke->count[i];
        for (j = 0; j < 3; j++) {
            __asm__ __volatile__("lq $8, 0(%1)\n\t"
                                 "sq $8, 0(%0)"
                                 :
                                 : "r"(&smoke->smoke[i + 1][j]), "r"(&smoke->smoke[i][j])
                                 : "$8", "memory");
            smoke->rotation[i + 1][j] = smoke->rotation[i][j];
        }
    }
    smoke->count[0] = 0;
    for (j = 0; j < 3; j++) {
        __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&smoke->smoke[0][j]) : "memory");
    }
    if (work->frame < 15) {
        smoke->count[0] = 1;
        for (j = 0; j < 3; j++) {
            MMathVectorInterpolation(&offset, &work->position, &work->previousPosition,
                                     (float)j / 3.0f);
            MMathRotateMatrixYX(matrix, 0, &work->angles);
            __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&offset) : "memory");
            __asm__ __volatile__("vmove.w vf1, vf0" : : : "memory");
            __asm__ __volatile__("sqc2 vf1, 48(%0)" : : "r"(matrix) : "memory");
            __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&offset) : "memory");
            offset.x = MMathMakeRandom() * 0.06f - 0.03f;
            offset.y = MMathMakeRandom() * 0.06f - 0.03f;
            MMathApplyMatrix(&smoke->smoke[0][j], matrix, &offset);
            smoke->rotation[0][j] = MMathMakeRandom2PI();
        }
    }
}

extern void *MEfCalcAngle(Vector4 *destination, const Vector4 *from,
                          const Vector4 *to);
extern void *MMathApplyMatrix(Vector4 *destination, const Vector4 *matrix,
                              const Vector4 *point);
extern void MMathRotateMatrixYX(Vector4 *destination, const Vector4 *source,
                               const Vector4 *angles);
extern void updateSmoke(Smp01Work *work);

static void fnSMP01_PR010(Smp01Object *object, Smp01Work *work);

static void fnSMP01_PR000(Smp01Object *object, Smp01Work *work)
{
    Vector4 offset;
    Vector4 matrix[4];
    int phase;
    float sine;
    float cosine;

    phase = work->phase;
    work->phase = phase + 1;
    work->phaseCount = work->phaseCount - phase + 5;

    __asm__ __volatile__("lq $8, 0(%1)\n\t"
                         "sq $8, 0(%0)"
                         :
                         : "r"(&work->previousPosition), "r"(&work->position)
                         : "$8", "memory");
    __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&offset) : "memory");

    __asm__ __volatile__("mfc1 $8, %1\n\t"
                         "qmtc2.ni $8, vf4\n\t"
                         "vcallms 0x20\n\t"
                         "qmfc2.i $8, vf1\n\t"
                         "mtc1 $8, %0"
                         : "=f"(sine)
                         : "f"(work->phaseAngle)
                         : "$8", "memory");
    offset.x = (float)work->phase * 0.06f * sine;

    __asm__ __volatile__("mfc1 $8, %1\n\t"
                         "qmtc2.ni $8, vf4\n\t"
                         "vcallms 0xe8\n\t"
                         "qmfc2.i $8, vf1\n\t"
                         "mtc1 $8, %0"
                         : "=f"(cosine)
                         : "f"(work->phaseAngle)
                         : "$8", "memory");
    offset.y = (float)work->phase * 0.06f * cosine;
    offset.z = (float)work->phaseCount * -0.02f;

    MMathRotateMatrixYX(matrix, 0, &work->angles);
    {
        /* Keep the translation load's address in v1, as at 0x00a352ec. */
        register Vector4 *spawnPoint asm("$3") = &work->spawnPoint;

        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(spawnPoint) : "memory");
    }
    __asm__ __volatile__("vmove.w vf1, vf0" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 48(%0)" : : "r"(matrix) : "memory");
    MMathApplyMatrix(&work->position, matrix, &offset);

    if (work->phase >= 4) {
        updateSmoke(work);
    }

    if (work->phase >= 5) {
        work->phase = 0;
        __asm__ __volatile__("lq $8, 0(%1)\n\t"
                             "sq $8, 0(%0)"
                             :
                             : "r"(&work->phaseStart), "r"(&work->position)
                             : "$8", "memory");
        MEfCalcAngle(&work->angles, &work->targetPosition, &work->phaseStart);
        object->updateCallback = fnSMP01_PR010;
    }
}

enum { SMP01_DRAW_FRAME_LIMIT = 15 };

extern Vector4 *MMathVectorInterpolation(Vector4 *destination, const Vector4 *first,
                                         const Vector4 *second, float parameter);

static void fnSMP01_PR010(Smp01Object *object, Smp01Work *work)
{
    (void)object;
    if (work->frame < SMP01_DRAW_FRAME_LIMIT) {
        float phase;

        __asm__ __volatile__("lq $8, 0(%1)\n\t"
                             "sq $8, 0(%0)"
                             :
                             : "r"(&work->previousPosition), "r"(&work->position)
                             : "$8", "memory");
        phase = ((float)(work->phase + 1) / 10.0f) * 1.5707964f + 4.712389f;
        __asm__ __volatile__("mfc1 $8, %0\n\t"
                             "qmtc2.ni $8, vf4\n\t"
                             "vcallms 0x20\n\t"
                             "qmfc2.i $8, vf1\n\t"
                             "mtc1 $8, %0"
                             : "+f"(phase)
                             :
                             : "$8", "memory");
        MMathVectorInterpolation(&work->position, &work->phaseStart,
                                 &work->targetPosition, phase + 1.0f);
        work->angles.z += 0.5235988f;
    }
    updateSmoke(work);
    work->phase++;
}

extern Vector4 *MMathRotateMatrixYXZ(Vector4 *out, const Vector4 *matrix, const Vector4 *angles);
extern Vector4 *MMathScaleMatrix(Vector4 *out, const Vector4 *matrix, const Vector4 *scale);
extern void MEfDrawModel(const Vector4 *place, int entry, const char *texture);
extern Vector4 scale_0_00A514A0;

static void fnSMP01_DM000(Smp01Object *self, Smp01Work *work)
{
    Vector4 matrix[4];

    (void)self;
    if (work->frame < SMP01_DRAW_FRAME_LIMIT) {
        MMathRotateMatrixYXZ(matrix, 0, &work->angles);
        MMathScaleMatrix(matrix, matrix, &scale_0_00A514A0);
        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&work->position) : "memory");
        __asm__ __volatile__("vmove.w vf1, vf0" : : : "memory");
        __asm__ __volatile__("sqc2 vf1, 48(%0)" : : "r"(matrix) : "memory");
        MEfDrawModel(matrix, work->modelEntry, work->textureName);
    }
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct Smp01Packet {
    void *data;    /* 0x00 */
    int capacity;  /* 0x04 */
    void *current; /* 0x08 */
    int count;     /* 0x0C */
    int auxCount;  /* 0x10 */
} Smp01Packet;

typedef struct Smp01ImageParameter {
    u64 texture;
    float width;
    float height;
} Smp01ImageParameter;

INCLUDE_ASM("asm/nonmatchings/ov01/m_ef_create_smp_01", fnSMP01_DP000);

extern void MEfObjDestroy(void *object);
extern void sefHitEffect(void);

static void fnSMP01_PO000(void *object, void *work)
{
    Smp01State *state = (Smp01State *)work;

    state->frame++;
    if (state->frame == 15) {
        sefHitEffect();
    }
    if (state->frame >= 25) {
        MEfObjDestroy(object);
    }
}

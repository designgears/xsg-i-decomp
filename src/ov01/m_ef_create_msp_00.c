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
static void fnMSP00_PR000(void *self, void *work);
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

static void fnMSP00_PR000(void *self, void *arg)
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

typedef unsigned int Msp00Quad __attribute__((mode(TI)));
typedef union Msp00QW {
    Msp00Quad q;
    unsigned long ul[2];
    unsigned short us[8];
    unsigned char uc[16];
    volatile unsigned long vul[2];
    volatile unsigned char vuc[16];
} Msp00QW;
extern int MEfMake4Vertex2(Vector4 *dst, const Vector4 *source, float size, float angleBase);
extern void MGsGPTerm(void *packet);
extern Msp00Quad McEfSTQtbl[];
extern Msp00Quad coltbl_1_00A51390[];
extern float sztbl_2_00A51430[];
typedef struct Msp00Packet {
    void *data;
    int capacity;
    Msp00QW *current;
    int count;
    int auxCount;
} Msp00Packet;
typedef struct Msp00Draw {
    unsigned char unmodeled_00[0xE0];
    unsigned char packet[0x14];    /* +0xe0 */
    unsigned short active[10];     /* +0xf4 */
    unsigned char unmodeled_108[8];
    Vector4 points[10][3];         /* +0x110 */
    float twist[10][3];            /* +0x2f0 */
} Msp00Draw;

static void fnMSP00_DP000(void *self, void *work)
{
    Msp00Draw *effect = (Msp00Draw *)work;
    int i;
    long regs;
    int nreg;
    float *szp;
    Vector4 verts[4];
    Msp00Packet *pk;
    Msp00Packet *gp;
    Msp00QW *q;
    Msp00QW *a1;
    Msp00QW *a2;
    Msp00QW *a3;
    Msp00QW *a4;
    Msp00QW *a5;
    Msp00Quad color;
    int j;
    int n;
    int nloop;
    int texReg;
    struct { unsigned char pad[0xF0]; short active[10]; } *sub;

    pk = (Msp00Packet *)effect->packet;
    MGsGPInit(pk, 0, 0);
    q = pk->current;
    q[0].ul[0] = 0x1000000000008000;
    q[0].ul[1] = 0xE;
    pk->auxCount = 1;
    a1 = q + 1;
    a2 = q + 2;
    a3 = q + 3;
    a4 = q + 4;
    a5 = q + 5;
    a1->vuc[8] = 0x47;
    a1->vul[0] = 0x5000D;
    a2->vuc[8] = 0x14;
    a2->vul[0] = 0x60;
    a3->vuc[8] = 0x8;
    a3->vul[0] = 5;
    a4->vuc[8] = 0x4E;
    a4->vul[0] = 0x131000000;
    a5->vuc[8] = 0x42;
    a5->vul[0] = 0x48;
    nreg = 5;
    n = pk->auxCount * nreg + 1;
    pk->current->us[0] |= nreg;
    pk->current += n;
    pk->count += n;
    for (i = 0; i < 10; i++) {
        if ((sub = (void *)((char *)work + 4))->active[i] != 0) {
                        color = coltbl_1_00A51390[i];
            for (j = 0; j < 3; j++) {
                if (MEfMake4Vertex2(verts, &effect->points[i][j], sztbl_2_00A51430[i], effect->twist[i][j]) == 0) {
                    Msp00QW *t;
                    Msp00QW *g;
                    unsigned long texVal;
                    int n1;
                    int n2;

                    gp = (Msp00Packet *)effect->packet;
                    texReg = 6;
                    regs = 0x512;
                    nloop = 3;
                    t = gp->current;
                    t[0].ul[0] = 0x1000000000008000;
                    t[0].ul[1] = 0xE;
                    gp->auxCount = 1;
                    a1 = t + 1;
                    texVal = *(unsigned long *)((char *)effect + 0x48 + ((j ^ i) & 1) * 8);
                    a1->uc[8] = texReg;
                    a1->ul[0] = texVal;
                    n1 = gp->auxCount + 1;
                    gp->current->us[0] |= 1;
                    gp->current += n1;
                    gp->count += n1;
                    g = gp->current;
                    g[0].ul[0] = 0x302A400000008000;
                    g[0].ul[1] = regs;
                    gp->auxCount = nloop;
                    g[1].q = McEfSTQtbl[0];
                    g[2].q = color;
                    g[3].q = *(Msp00Quad *)&verts[0];
                    g[4].q = McEfSTQtbl[1];
                    g[5].q = color;
                    g[6].q = *(Msp00Quad *)&verts[1];
                    g[7].q = McEfSTQtbl[2];
                    g[8].q = color;
                    g[9].q = *(Msp00Quad *)&verts[2];
                    g[10].q = McEfSTQtbl[3];
                    g[11].q = color;
                    g[12].q = *(Msp00Quad *)&verts[3];
                    n2 = gp->auxCount * 4 + 1;
                    gp->current->us[0] |= 4;
                    gp->current += n2;
                    gp->count += n2;
                }
            }
            __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__("");
        }
    }
    MGsGPTerm(pk);
}

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

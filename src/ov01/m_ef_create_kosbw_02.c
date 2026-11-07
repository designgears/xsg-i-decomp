/*
 * OV01 original TU 36: 0x00a3a090..0x00a3aac8 (5 functions)
 */
#include "common.h"
#include "shared.h"

typedef struct Kosbw02DrawWork {
    unsigned char unmodeled_00[0x40];
    int model_entry;
    const char *texture;
    unsigned char unmodeled_48[0x28];
    int frame;
    unsigned char unmodeled_74[0x8c];
    Vector4 position;
    unsigned char unmodeled_110[0x10];
    Vector4 rotation;
} Kosbw02DrawWork;

typedef struct Kosbw02GsParameter {
    u64 primitive_tag;
    float scale_u;
    float scale_v;
} Kosbw02GsParameter;

typedef struct Kosbw02CreateState {
    unsigned char unmodeled_00[0x08];
    u32 source_actor;
    u32 source_part;
    Vector4 source_offset;
    s16 rotation_degrees;
    unsigned char unmodeled_22[0x02];
    u32 target_actor;
    u32 target_part;
    unsigned char unmodeled_2c[0x44];
    int frame;
    unsigned char unmodeled_74[0x0c];
    Vector4 start_point;
    Vector4 target_point;
    Vector4 midpoint;
    float half_length;
    float angle;
    float angle_step;
    unsigned char unmodeled_bc[0x04];
    Vector4 matrix[4];
    Vector4 position;
    Vector4 previous_position;
    Vector4 rotation;
    unsigned char draw_packet[0x18];
    u64 draw_parameter;
    u64 texture_parameter;
    u16 active[10];
    unsigned char unmodeled_16c[0x04];
    Vector4 points[30];
    float twist[30];
    float random_angle;
} Kosbw02CreateState;

struct Kosbw02Effect;

typedef struct Kosbw02Allocation {
    unsigned char unmodeled_00[0x04];
    void (*process_callback)(void *task, Kosbw02CreateState *state);
    void (*model_callback)(void *task, Kosbw02DrawWork *state);
    void (*packet_callback)(void *task, void *state);
    void (*post_callback)(void *task, struct Kosbw02Effect *state);
    unsigned char unmodeled_14[0x0c];
    Kosbw02CreateState state;
} Kosbw02Allocation;

static void fnKOSBW02_PR000(void *task, Kosbw02CreateState *state);
static void fnKOSBW02_DM000(void *task, Kosbw02DrawWork *state);
static void fnKOSBW02_DP000(void *task, void *state);
static void fnKOSBW02_PO000(void *task, struct Kosbw02Effect *state);

extern void MEfGetActorMatrix(Vector4 *destination, u32 actor, u32 part);
extern void MEfGetActorCoord(Vector4 *destination, u32 actor, u32 coord);
extern Vector4 *MMathApplyMatrix(Vector4 *destination, const Vector4 *matrix,
                                 const Vector4 *source);
extern Vector4 *MMathVectorInterpolation(Vector4 *destination,
                                         const Vector4 *first,
                                         const Vector4 *second,
                                         float parameter);
extern Vector4 *MMathRotateMatrixYX(Vector4 *destination,
                                    const Vector4 *source,
                                    const Vector4 *angles);
extern float MMathCalcDist(const Vector4 *first, const Vector4 *second);
extern float MMathMakeRandom(void);
extern void *MEfCalcAngle(Vector4 *destination, const Vector4 *from,
                          const Vector4 *to);
extern void MGsGPInit(void *packet, void *address, int size);
extern Kosbw02GsParameter *svGetPrmFromName(const char *name,
                                            Kosbw02GsParameter *output);
extern float MMathMakeRandom2PI(void);
extern Vector4 *MMathRotateMatrixYXZ(Vector4 *destination,
                                     const Vector4 *source,
                                     const Vector4 *angles);
extern void MEfDrawModel(const Vector4 *place, int entry, const char *texture);

int MEfCreate_KOSBW02(void *storage)
{
    Kosbw02Allocation *allocation = storage;
    Kosbw02CreateState *state = &allocation->state;
    Kosbw02GsParameter parameter;
    Vector4 angle_vector;
    Vector4 actor_matrix[4];

    state->frame = 0;
    MEfGetActorMatrix(actor_matrix, state->source_actor, state->source_part);
    MMathApplyMatrix(&state->start_point, actor_matrix, &state->source_offset);
    MEfGetActorCoord(&state->target_point, state->target_actor, state->target_part);
    MMathVectorInterpolation(&state->midpoint, &state->start_point,
                             &state->target_point, 0.5f);
    state->half_length = MMathCalcDist(&state->start_point, &state->target_point) * 0.5f;
    MEfCalcAngle(&angle_vector, &state->target_point, &state->start_point);
    angle_vector.z = (float)state->rotation_degrees * 0.017453292f;
    MMathRotateMatrixYXZ(state->matrix, 0, &angle_vector);
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n"
        "vmove.w $vf1, $vf0\n"
        "sqc2 $vf1, 48(%0)\n"
        :
        : "r"(state->matrix), "r"(&state->midpoint)
        : "memory");
    state->angle = 0.0f;
    state->angle_step = 0.12566371f;
    __asm__ __volatile__(
        "lq $8, 0(%1)\n"
        "sq $8, 0(%0)\n"
        :
        : "r"(&state->position), "r"(&state->start_point)
        : "$8", "memory");
    __asm__ __volatile__(
        "lq $8, 0(%1)\n"
        "sq $8, 0(%0)\n"
        :
        : "r"(&state->previous_position), "r"(&state->start_point)
        : "$8", "memory");
    MEfCalcAngle(&state->rotation, &state->target_point, &state->start_point);
    MGsGPInit(state->draw_packet, 0, 0);
    svGetPrmFromName("de_cld01", &parameter);
    state->draw_parameter = parameter.primitive_tag;
    svGetPrmFromName("de_cld02", &parameter);
    state->texture_parameter = parameter.primitive_tag;
    memset(state->active, 0, sizeof(state->active));
    memset(state->points, 0, sizeof(state->points));
    memset(state->twist, 0, sizeof(state->twist));
    state->random_angle = MMathMakeRandom2PI();
    allocation->process_callback = fnKOSBW02_PR000;
    allocation->model_callback = fnKOSBW02_DM000;
    allocation->packet_callback = fnKOSBW02_DP000;
    allocation->post_callback = fnKOSBW02_PO000;
    return 1;
}

static void fnKOSBW02_PR000(void *task, Kosbw02CreateState *work)
{
    Vector4 work_vector;
    Vector4 matrix[4];
    float sine;
    float cosine;
    int trail;
    int point;
    int point_index;
    Vector4 *shifted_point;
    Vector4 *source_point;
    Vector4 *trail_point;

    if (work->frame < 25) {
        __asm__ __volatile__(
            "lq $8, 0(%1)\n"
            "sq $8, 0(%0)\n"
            :
            : "r"(&work->previous_position), "r"(&work->position)
            : "$8", "memory");
        __asm__ __volatile__("sqc2 $vf0, 0(%0)"
                             :
                             : "r"(&work_vector)
                             : "memory");
        __asm__ __volatile__(
            "mfc1 $8, %1\n"
            "qmtc2 $8, $vf4\n"
            "vcallms 0x20\n"
            "qmfc2.i $8, $vf1\n"
            "mtc1 $8, %0\n"
            : "=f"(sine)
            : "f"(work->angle)
            : "$8", "memory");
        work_vector.x = work->half_length * sine;
        __asm__ __volatile__(
            "mfc1 $8, %1\n"
            "qmtc2 $8, $vf4\n"
            "vcallms 0xe8\n"
            "qmfc2.i $8, $vf1\n"
            "mtc1 $8, %0\n"
            : "=f"(cosine)
            : "f"(work->angle)
            : "$8", "memory");
        work_vector.z = work->half_length * cosine;
        MMathApplyMatrix(&work->position, work->matrix, &work_vector);
        work->angle = work->angle + work->angle_step;
        MEfCalcAngle(&work->rotation, &work->position,
                     &work->previous_position);
    }

    for (trail = 8; trail >= 0; trail--) {
        work->active[trail + 1] = work->active[trail];
        for (point = 0; point < 3; point++) {
            point_index = trail * 3 + point;
            shifted_point = &work->points[point_index + 3];
            source_point = &work->points[point_index];
            __asm__ __volatile__(
                "lq $8, 0(%1)\n"
                "sq $8, 0(%0)\n"
                :
                : "r"(shifted_point), "r"(source_point)
                : "$8", "memory");
            work->twist[point_index + 3] = work->twist[point_index];
        }
    }

    work->active[0] = 0;
    for (point = 0; point < 3; point++) {
        trail_point = &work->points[point];
        __asm__ __volatile__("sqc2 $vf0, 0(%0)"
                             :
                             : "r"(trail_point)
                             : "memory");
    }

    if (work->frame < 25) {
        work->active[0] = 1;
        for (point = 0; point < 3; point++) {
            MMathVectorInterpolation(&work_vector, &work->position,
                                     &work->previous_position,
                                     (float)point / 3.0f);
            MMathRotateMatrixYX(matrix, 0, &work->rotation);
            __asm__ __volatile__(
                "lqc2 $vf1, 0(%1)\n"
                "vmove.w $vf1, $vf0\n"
                "sqc2 $vf1, 48(%0)\n"
                :
                : "r"(matrix), "r"(&work_vector)
                : "memory");
            __asm__ __volatile__("sqc2 $vf0, 0(%0)"
                                 :
                                 : "r"(&work_vector)
                                 : "memory");
            work_vector.x = MMathMakeRandom() * 0.06f - 0.03f;
            work_vector.y = MMathMakeRandom() * 0.06f - 0.03f;
            MMathApplyMatrix(&work->points[point], matrix, &work_vector);
            work->twist[point] = MMathMakeRandom2PI();
        }
    }
}

static void fnKOSBW02_DM000(void *task, Kosbw02DrawWork *work)
{
    Vector4 matrix[4];

    if (work->frame < 25) {
        MMathRotateMatrixYXZ(matrix, 0, &work->rotation);
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%1)\n"
            "vmove.w $vf1, $vf0\n"
            "sqc2 $vf1, 48(%0)\n"
            :
            : "r"(matrix), "r"(&work->position)
            : "memory");
        MEfDrawModel(matrix, work->model_entry, work->texture);
    }
}

typedef unsigned int Kos02Quad __attribute__((mode(TI)));
typedef union Kos02QW {
    Kos02Quad q;
    unsigned long ul[2];
    unsigned short us[8];
    unsigned char uc[16];
    volatile unsigned long vul[2];
    volatile unsigned char vuc[16];
} Kos02QW;
extern int MEfMake4Vertex(Vector4 *dst, const Vector4 *source, float size);
extern int MEfMake4Vertex2(Vector4 *dst, const Vector4 *source, float size, float angleBase);
extern void MGsGPTerm(void *packet);
extern Kos02Quad McEfSTQtbl[];
typedef struct Kos02Packet {
    void *data;
    int capacity;
    Kos02QW *current;
    int count;
    int auxCount;
} Kos02Packet;

static void fnKOSBW02_DP000(void *task, void *state)
{
    static const Kos02QW coltbl[10] = {
        { ul: { 0x0000008000000080UL, 0x0000002000000080UL } },
        { ul: { 0x0000008000000080UL, 0x0000002800000080UL } },
        { ul: { 0x0000008000000080UL, 0x0000002800000080UL } },
        { ul: { 0x0000008000000080UL, 0x0000002800000080UL } },
        { ul: { 0x0000008000000080UL, 0x0000002800000080UL } },
        { ul: { 0x0000008000000080UL, 0x0000002400000080UL } },
        { ul: { 0x0000008000000080UL, 0x0000002000000080UL } },
        { ul: { 0x0000008000000080UL, 0x0000001800000080UL } },
        { ul: { 0x0000008000000080UL, 0x0000001000000080UL } },
        { ul: { 0x0000008000000080UL, 0x0000000800000080UL } },
    };
    static const float sztbl[10] = {
        0.1f, 0.13f, 0.16f, 0.18f, 0.2f, 0.22f, 0.24f, 0.26f, 0.28f, 0.25f,
    };
    Kosbw02CreateState *effect = state;
    int i;
    int nreg;
    float *szp;
    Vector4 verts[4];
    Kos02Packet *pk;
    Kos02Packet *gp;
    Kos02QW *q;
    Kos02QW *a1;
    Kos02QW *a2;
    Kos02QW *a3;
    Kos02QW *a4;
    Kos02QW *a5;
    Kos02Quad color;
    unsigned long *tex;
    int j;
    int n;
    int off;
    int lo;
    char *base;
    long regs;
    int texReg;
    int nloop;
    unsigned long texVal;
    struct { unsigned char pad[0x150]; short active[10]; } *sub;

    pk = (Kos02Packet *)effect->draw_packet;
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
    szp = sztbl;
    for (i = 0; i < 10; i++) {
        if ((sub = (void *)((char *)state + 8))->active[i] != 0) {
            color = coltbl[i].q;
            lo = i < 6;
            off = 0x148;
            if (lo) {
                off = 0x140;
            }
            tex = (unsigned long *)((char *)sub + off);
            for (j = 0; j < 3; j++) {
                if (MEfMake4Vertex2(verts, &((Vector4 (*)[3])effect->points)[i][j], sztbl[i], ((float (*)[3])effect->twist)[i][j]) == 0) {
                    Kos02QW *t;
                    Kos02QW *g;
                    int n1;
                    int n2;

                    gp = (Kos02Packet *)effect->draw_packet;
                    texReg = 6;
                    regs = 0x512;
                    nloop = 3;
                    t = gp->current;
                    t[0].ul[0] = 0x1000000000008000;
                    t[0].ul[1] = 0xE;
                    gp->auxCount = 1;
                    a1 = t + 1;
                    texVal = *tex;
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
                    g[3].q = *(Kos02Quad *)&verts[0];
                    g[4].q = McEfSTQtbl[1];
                    g[5].q = color;
                    g[6].q = *(Kos02Quad *)&verts[1];
                    g[7].q = McEfSTQtbl[2];
                    g[8].q = color;
                    g[9].q = *(Kos02Quad *)&verts[2];
                    g[10].q = McEfSTQtbl[3];
                    g[11].q = color;
                    g[12].q = *(Kos02Quad *)&verts[3];
                    n2 = gp->auxCount * 4 + 1;
                    gp->current->us[0] |= 4;
                    gp->current += n2;
                    gp->count += n2;
                }
            }
             __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__(""); __asm__("");
        }
    }
    if (effect->frame < 26) {
        if (MEfMake4Vertex(verts, &effect->position, 0.5f) == 0) {
            Kos02QW *t;
            Kos02QW *g;
            int n1;
            int n2;


            t = pk->current;
            t[0].ul[0] = 0x1000000000008000;
            t[0].ul[1] = 0xE;
            pk->auxCount = 1;
            a1 = t + 1;
            texVal = *(unsigned long *)((char *)effect + 0x48);
            a1->uc[8] = 6;
            a1->ul[0] = texVal;
            color = 0xFF000000280000002800000028;
            n1 = pk->auxCount + 1;
            pk->current->us[0] |= 1;
            pk->current += n1;
            pk->count += n1;
            g = pk->current;
            { long r = 0x512; g[0].ul[0] = 0x302A400000008000; g[0].ul[1] = r; }
            pk->auxCount = 3;
            g[1].q = McEfSTQtbl[0];
            g[2].q = color;
            g[3].q = *(Kos02Quad *)&verts[0];
            g[4].q = McEfSTQtbl[1];
            g[5].q = color;
            g[6].q = *(Kos02Quad *)&verts[1];
            g[7].q = McEfSTQtbl[2];
            g[8].q = color;
            g[9].q = *(Kos02Quad *)&verts[2];
            g[10].q = McEfSTQtbl[3];
            g[11].q = color;
            g[12].q = *(Kos02Quad *)&verts[3];
            n2 = pk->auxCount * 4 + 1;
            pk->current->us[0] |= 4;
            pk->current += n2;
            pk->count += n2;
        }
    }
    MGsGPTerm(pk);
}

extern void MEfObjDestroy(void *self);
extern void sefHitEffect(void);

/*
 * The KOSBW02 effect record: the work area MEfCreate_KOSBW02 (outside this
 * allocation) installs fnKOSBW02_PO000 into. `frame` is the same counter role
 * the sibling MSP02 effect keeps at this same offset (MspEffect,
 * src/ov01/m_ef_create_msp_02.c): incremented once per call, it fires
 * sefHitEffect at 25 and MEfObjDestroy at 35.
 */
typedef struct Kosbw02Effect {
    unsigned char unmodeled_00[0x70];
    int frame;
} Kosbw02Effect;

static void fnKOSBW02_PO000(void *self, Kosbw02Effect *work)
{
    work->frame++;
    if (work->frame == 25) {
        sefHitEffect();
    }
    if (work->frame >= 35) {
        MEfObjDestroy(self);
    }
}

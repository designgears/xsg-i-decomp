/*
 * OV01 original TU 35: 0x00a397c8..0x00a3a090 (7 functions)
 */
#include "common.h"
#include "shared.h"
#include "main/m_math.h"

struct DoraHermiteState;
extern void MEfCalcWeaponCoord(void *destination, void *weapon);
extern void MEfGetActorCoord(void *destination, int actor_id, int part_id);
extern void MGsGPInit(void *packet, int a, int b);
static void makePath(struct DoraHermiteState *state);
static void makeHermiteParams(int segment, struct DoraHermiteState *state);
static void fnDORA_PR000(void *self, struct DoraHermiteState *state);
static void fnDORA_DP000(void *self, struct DoraHermiteState *state);
static void fnDORA_PO000(void *self, void *work);
typedef unsigned int DoraCreateQuad __attribute__((mode(TI)));
typedef struct DoraCreateObject {
    void *unk0;
    void *process;
    void *unk8;
    void *draw;
    void *lifetime;
} DoraCreateObject;

int MEfCreate_DORA(void *self)
{
    DoraCreateObject *object = (DoraCreateObject *)self;
    unsigned char *base = (unsigned char *)self;
    unsigned char *state = base + 0x20;
    unsigned char *trail = base + 0x150;
    unsigned char *weapon = base + 0xA0;

    *(short *)(state + 0x70) = 0;
    MEfCalcWeaponCoord(weapon, state);
    MEfGetActorCoord(base + 0xB0, *(int *)(state + 0x24), *(int *)(state + 0x28));
    memset(base + 0xC0, 0, 0x44);
    memset(base + 0x104, 0, 0x44);
    memset(trail, 0, 0x220);
    __asm__ volatile("lq $8, 0(%1)\n\tsq $8, 0(%0)" : : "r"(trail), "r"(weapon) : "$8", "memory");
    *(short *)(state + 0x350) = 0;
    *(short *)(state + 0x352) = 0;
    makePath((struct DoraHermiteState *)state);
    makeHermiteParams(0, (struct DoraHermiteState *)state);
    MGsGPInit(base + 0x3F0, 0, 0);
    object->process = fnDORA_PR000;
    object->draw = fnDORA_DP000;
    object->lifetime = fnDORA_PO000;
    return 1;
}

typedef unsigned int DoraQuadword __attribute__((mode(TI)));

/* The Hermite control-point storage shares the post-process state object.
 * The earlier quadword arrays preserve its observed sixteen-byte alignment. */
typedef struct DoraHermiteState {
    u32 unmodeled_00[4];
    unsigned char unmodeled_10[0x14];
    u32 actor_id;
    u32 actor_coord_id;
    unsigned char unmodeled_2c[0x44];
    short frame;
    unsigned char unmodeled_72[0x0e];
    DoraQuadword weapon_coord;
    HermiteVector actor_coord;
    short trail_active[34];
    short trail_segment[34];
    unsigned char unmodeled_128[8];
    DoraQuadword trail[34];
    short segment;
    short segment_frame;
    unsigned char unmodeled_354[0x0c];
    HermiteVector control_points[4];
    float path_progress[4];
    HermiteVector tangent_start;
    HermiteVector tangent_end;
    unsigned char mg_packet;
} DoraHermiteState;

float MMathMakeRandom(void);
float MMathMakeRandom2PI(void);
extern void MEfGetActorMatrix(Matrix4 *matrix, int actor_id, int part_id);
extern Matrix4 *MMathTranslateMatrix(Matrix4 *out, const Matrix4 *matrix, const Vector4 *translation);
extern Matrix4 *MMathRotateMatrixX(Matrix4 *out, const Matrix4 *matrix, float angle);
extern Vector4 *MMathApplyMatrix(Vector4 *out, const Matrix4 *matrix, const Vector4 *vector);
extern float rtbl_1_00A51830[4];
extern float disttbl_0_00A51820[4];

static void makePath(DoraHermiteState *state)
{
    Matrix4 matrix;
    Vector4 v;
    float step;
    float angle;
    float scale;
    float s;
    float c;
    int i;
    HermiteVector *point;
    float *progress;

    step = 0.13962634f;
    MEfGetActorMatrix(&matrix, state->unmodeled_00[2], state->unmodeled_00[3]);
    MMathTranslateMatrix(&matrix, &matrix, (Vector4 *)state->unmodeled_10);
    MMathRotateMatrixX(&matrix, &matrix, 0.27925268f);
    angle = MMathMakeRandom2PI();
    if (!(xglSRand() & 1)) {
        step = -0.13962634f;
    }
    scale = MMathMakeRandom() + 0.05f;
    for (i = 0; i < 4; i++) {
        __asm__ volatile("sqc2 $vf0, 0(%0)" : : "r"(&v) : "memory");
        __asm__ __volatile__(
            "mfc1 $8, %1\n\t"
            "qmtc2.ni $8, $vf4\n\t"
            "vcallms 0xe8\n\t"
            "qmfc2.i $8, $vf1\n\t"
            "mtc1 $8, %0\n\t"
            : "=f"(s) : "f"(angle) : "$8", "memory");
        v.x = s * rtbl_1_00A51830[i] * scale;
        __asm__ __volatile__(
            "mfc1 $8, %1\n\t"
            "qmtc2.ni $8, $vf4\n\t"
            "vcallms 0x20\n\t"
            "qmfc2.i $8, $vf1\n\t"
            "mtc1 $8, %0\n\t"
            : "=f"(c) : "f"(angle) : "$8", "memory");
        v.z = disttbl_0_00A51820[i];
        v.y = c * rtbl_1_00A51830[i] * scale;
        MMathApplyMatrix((Vector4 *)&state->control_points[i], &matrix, &v);
        state->path_progress[i] = angle;
        angle += step;
        step *= 1.2f;
    }
}

extern void MMathCalcHermitePrm(HermiteVector *tangent_start,
                                HermiteVector *tangent_end,
                                const HermiteVector *point_previous,
                                const HermiteVector *point_start,
                                const HermiteVector *point_end,
                                const HermiteVector *point_next);

static void makeHermiteParams(int segment, DoraHermiteState *state)
{
    HermiteVector *point_previous;
    HermiteVector *point_start;
    HermiteVector *point_end;
    HermiteVector *point_next;

    if (segment == 0) {
        point_previous = &state->control_points[0];
    } else {
        point_previous = &state->control_points[segment - 1];
    }
    point_start = &state->control_points[segment];
    point_end = &state->control_points[segment + 1];
    if (segment < 2) {
        point_next = &state->control_points[segment + 2];
    } else {
        point_next = point_end;
    }

    MMathCalcHermitePrm(&state->tangent_start, &state->tangent_end,
                        point_previous, point_start, point_end, point_next);
}

static void makeHermiteCoord(float *destination, void *effect)
{
    unsigned char *base = (unsigned char *)effect;
    short segment = *(short *)(base + 848);
    short frame = *(short *)(base + 850);

    MMathCalcHermite(destination, (float)frame * 0.200000003f,
                     (HermiteVector *)(base + 944),
                     (HermiteVector *)(base + 960),
                     (HermiteVector *)(base + 864 + (segment << 4)),
                     (HermiteVector *)(base + 880 + (segment << 4)));
}

static void fnDORA_PR000(void *self, DoraHermiteState *state)
{
    int i;

    for (i = 32; i >= 0; i--) {
        state->trail_active[i + 1] = state->trail_active[i];
        state->trail_segment[i + 1] = state->trail_segment[i];
        __asm__ volatile("lq $8, 0(%1)\n\tsq $8, 0(%0)" : : "r"(&state->trail[i + 1]), "r"(&state->trail[i]) : "$8", "memory");
    }
    state->trail_active[0] = 0;
    __asm__ volatile("sqc2 $vf0, 0(%0)" : : "r"(&state->trail[0]) : "memory");
    if (state->frame < 15) {
        state->trail_active[0] = 1;
        state->trail_segment[0] = state->segment;
        makeHermiteCoord((float *)&state->trail[0], state);
        state->segment_frame++;
        if (state->segment_frame >= 5) {
            state->segment_frame = 0;
            state->segment++;
            makeHermiteParams(state->segment, state);
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/m_ef_create_dora", fnDORA_DP000);

/* MEfObjDestroy (src/main/m_ef_obj.c) and sefHitEffect (src/main/sef.c) are
 * still asm in their defining TU; declared locally until published there. */
extern void MEfObjDestroy(void *self);
extern void sefHitEffect(void);

/* work's layout is unresolved beyond +0x70: the per-frame lifetime counter
 * this function advances, firing sefHitEffect on frame 0xf and expiring
 * through MEfObjDestroy from frame 0x30 on. */
#define DORA_HIT_FRAME 0xf
#define DORA_LIFETIME 0x30

typedef struct DoraState {
    unsigned char unmodeled_00[0x70];
    short frame; /* +0x70 */
} DoraState;

static void fnDORA_PO000(void *self, void *work)
{
    DoraState *state = (DoraState *)work;

    state->frame++;
    if (state->frame == DORA_HIT_FRAME) {
        sefHitEffect();
    }
    if (state->frame >= DORA_LIFETIME) {
        MEfObjDestroy(self);
    }
}

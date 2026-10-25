/*
 * OV01 original TU 32: 0x00a387a8..0x00a39088 (7 functions)
 */
#include "common.h"
#include "shared.h"
#include "main/m_math.h"

extern void MMathCalcHermitePrm(HermiteVector *tangent_start, HermiteVector *tangent_end,
                                const HermiteVector *point_prev,
                                const HermiteVector *point_start,
                                const HermiteVector *point_end,
                                const HermiteVector *point_next);

/* fork type needed by GameraWork (derwetterfrosch) */
#define GAMERA_PATH_POINTS 4

#define GAMERA_TRAIL_LENGTH 28

typedef struct GameraWork {
    unsigned char unmodeled_00[0x8];
    struct ActRecord *actor;
    u32 coordinate;
    unsigned char unmodeled_10[0x14];
    struct ActRecord *target_actor;
    u32 target_coordinate;
    unsigned char unmodeled_2c[0x44];
    short frame;
    unsigned char unmodeled_72[0xe];
    Vector4 origin;
    Vector4 target;
    short trail_active[GAMERA_TRAIL_LENGTH];
    short trail_segment[GAMERA_TRAIL_LENGTH];
    HermiteVector trail[GAMERA_TRAIL_LENGTH];
    short segment; /* +0x2d0 */
    short path_frame; /* +0x2d2 */
    unsigned char unmodeled_2d4[12];
    HermiteVector control_points[5]; /* +0x2e0 */
    HermiteVector tangent_start; /* +0x330 */
    HermiteVector tangent_end; /* +0x340 */
} GameraWork;

extern void MEfGetActorCoord(void *destination, int actor_id, int part_id);
extern void MGsGPInit(void *packet, int a, int b);
static void makePath(GameraWork *state);
static void makeHermiteParams(int segment, GameraWork *state);
struct GameraTrailState;
static void fnGAMERA_PR000(void *self, struct GameraTrailState *state);
static void fnGAMERA_DP000(void *self, struct GameraTrailState *state);
static void fnGAMERA_PO000(void *self, void *work);
typedef struct GameraCreateObject {
    void *unk0;
    void *process;
    void *unk8;
    void *draw;
    void *lifetime;
} GameraCreateObject;

int MEfCreate_GAMERA(void *self)
{
    GameraCreateObject *object = (GameraCreateObject *)self;
    unsigned char *base = (unsigned char *)self;
    unsigned char *state = base + 0x20;
    unsigned char *trail = base + 0x130;
    unsigned char *actor = base + 0xA0;

    *(short *)(state + 0x70) = 0;
    MEfGetActorCoord(actor, *(int *)(state + 0x8), *(int *)(state + 0xC));
    MEfGetActorCoord(base + 0xB0, *(int *)(state + 0x24), *(int *)(state + 0x28));
    memset(base + 0xC0, 0, 0x38);
    memset(base + 0xF8, 0, 0x38);
    memset(trail, 0, 0x1C0);
    __asm__ volatile("lq $8, 0(%1)\n\tsq $8, 0(%0)" : : "r"(trail), "r"(actor) : "$8", "memory");
    *(short *)(state + 0x2D0) = 0;
    *(short *)(state + 0x2D2) = 0;
    makePath((GameraWork *)state);
    makeHermiteParams(0, (GameraWork *)state);
    MGsGPInit(base + 0x370, 0, 0);
    object->process = fnGAMERA_PR000;
    object->draw = fnGAMERA_DP000;
    object->lifetime = fnGAMERA_PO000;
    return 1;
}

float MMathMakeRandom(void);
float MMathMakeRandom2PI(void);
extern void MEfGetActorMatrix(Matrix4 *matrix, int actor_id, int part_id);
extern Vector4 *MMathApplyMatrix(Vector4 *out, const Matrix4 *matrix, const Vector4 *vector);
extern float rtbl_1_00A517E0[4];
extern float disttbl_0_00A517D0[4];

static void makePath(GameraWork *state)
{
    Matrix4 matrix;
    Vector4 v;
    float step;
    float angle;
    float scale;
    float s;
    float c;
    int i;

    MEfGetActorMatrix(&matrix, *(int *)(state->unmodeled_00 + 8), *(int *)(state->unmodeled_00 + 0xC));
    do {
        angle = MMathMakeRandom2PI();
    } while (!(angle < 3.6651914f) && !(angle > 5.7595863f));
    step = 0.20943952f;
    if (!(xglSRand() & 1)) {
        step = -0.20943952f;
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
        v.x = s * rtbl_1_00A517E0[i] * scale;
        __asm__ __volatile__(
            "mfc1 $8, %1\n\t"
            "qmtc2.ni $8, $vf4\n\t"
            "vcallms 0x20\n\t"
            "qmfc2.i $8, $vf1\n\t"
            "mtc1 $8, %0\n\t"
            : "=f"(c) : "f"(angle) : "$8", "memory");
        v.z = disttbl_0_00A517D0[i];
        v.y = c * rtbl_1_00A517E0[i] * scale;
        MMathApplyMatrix((Vector4 *)&state->control_points[i], &matrix, &v);
        ((float *)&state->control_points[4])[i] = angle;
        angle += step;
        step *= 1.3f;
    }
}

static void makeHermiteParams(int segment, GameraWork *state)
{
    const HermiteVector *point_prev;
    const HermiteVector *point_end;
    const HermiteVector *point_next;

    if (segment == 0) {
        point_prev = &state->control_points[0];
    } else {
        point_prev = &state->control_points[segment - 1];
    }

    point_end = &state->control_points[segment + 1];
    point_next = &state->control_points[segment + 2];
    if (segment >= 2) {
        point_next = point_end;
    }

    MMathCalcHermitePrm(&state->tangent_start, &state->tangent_end,
                        point_prev, &state->control_points[segment],
                        point_end, point_next);
}

static void makeHermiteCoord(float *destination, void *effect)
{
    unsigned char *base = (unsigned char *)effect;
    short segment = *(short *)(base + 720);
    short frame = *(short *)(base + 722);

    MMathCalcHermite(destination, (float)frame * 0.125f,
                     (HermiteVector *)(base + 816),
                     (HermiteVector *)(base + 832),
                     (HermiteVector *)(base + 736 + (segment << 4)),
                     (HermiteVector *)(base + 752 + (segment << 4)));
}

typedef struct GameraQuadword {
    u32 word[4];
} GameraQuadword;
typedef struct GameraTrailState {
    unsigned char unmodeled_00[0x70];
    short frame; /* +0x70 */
    unsigned char unmodeled_72[0x2e];
    short trail_active[28]; /* +0xa0 */
    short trail_segment[28]; /* +0xd8 */
    GameraQuadword trail[28]; /* +0x110 */
    short segment; /* +0x2d0 */
    short path_frame; /* +0x2d2 */
} GameraTrailState;

static void fnGAMERA_PR000(void *self, GameraTrailState *state)
{
    int i;

    for (i = 26; i >= 0; i--) {
        state->trail_active[i + 1] = state->trail_active[i];
        state->trail_segment[i + 1] = state->trail_segment[i];
        __asm__ volatile("lq $8, 0(%1)\n\tsq $8, 0(%0)" : : "r"(&state->trail[i + 1]), "r"(&state->trail[i]) : "$8", "memory");
    }
    state->trail_active[0] = 0;
    __asm__ volatile("sqc2 $vf0, 0(%0)" : : "r"(&state->trail[0]) : "memory");
    if (state->frame < 24) {
        state->trail_active[0] = 1;
        state->trail_segment[0] = state->segment;
        makeHermiteCoord((float *)&state->trail[0], state);
        state->path_frame++;
        if (state->path_frame >= 8) {
            state->path_frame = 0;
            state->segment++;
            makeHermiteParams(state->segment, (GameraWork *)state);
        }
    }
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef union GameraGsQword {
    GameraQuadword whole;
    unsigned short nloop; /* low halfword of a GIF tag: NLOOP and EOP */
    struct {
        u64 tag;
        u64 registers;
    } gif;
    struct {
        u64 data;
        unsigned char address;
    } ad;
    struct {
        int r, g, b, a;
    } rgba;
    struct {
        int x, y, z, adc;
    } xyz;
} GameraGsQword;

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct GameraGsPacket {
    void *data;
    int capacity;
    GameraGsQword *current; /* the open GIF tag */
    int count;              /* queued quadwords */
    int regCount;           /* NREG of the open GIF tag */
} GameraGsPacket;

INCLUDE_ASM("asm/nonmatchings/ov01/m_ef_create_gamera", fnGAMERA_DP000);

/* MEfObjDestroy (src/main/m_ef_obj.c) and sefHitEffect (src/main/sef.c) are
 * still asm in their defining TU; declared locally until published there. */
extern void MEfObjDestroy(void *self);
extern void sefHitEffect(void);

/* work's layout is unresolved beyond +0x70: the per-frame lifetime counter
 * this function advances, firing sefHitEffect on frame 0x18 and expiring
 * through MEfObjDestroy from frame 0x33 on. */
#define GAMERA_HIT_FRAME 0x18
#define GAMERA_LIFETIME 0x33

typedef struct GameraState {
    unsigned char unmodeled_00[0x70];
    short frame; /* +0x70 */
} GameraState;

static void fnGAMERA_PO000(void *self, void *work)
{
    GameraState *state = (GameraState *)work;

    state->frame++;
    if (state->frame == GAMERA_HIT_FRAME) {
        sefHitEffect();
    }
    if (state->frame >= GAMERA_LIFETIME) {
        MEfObjDestroy(self);
    }
}

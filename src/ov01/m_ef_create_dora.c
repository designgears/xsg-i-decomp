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

typedef unsigned int MEfGsQuadword __attribute__((mode(TI)));
typedef union MEfGsQword {
    MEfGsQuadword whole;
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
typedef struct DoraDrawState {
    unsigned char unmodeled_00[0x90];
    Vector4 actor_coord;
    short trail_active[34];
    short trail_segment[34];
    unsigned char unmodeled_128[8];
    Vector4 trail[34];
    short segment;
    short segment_frame;
    unsigned char unmodeled_354[0x0c];
    HermiteVector control_points[4];
    float path_progress[4];
    HermiteVector tangent_start;
    HermiteVector tangent_end;
    MEfGsPacket mg_packet;
} DoraDrawState;
typedef struct DoraCamParams {
    StudioCamera *screen;
    Matrix4 *matrix;
} DoraCamParams;
extern DoraCamParams mefCamParams;
extern void MGsGPTerm(MEfGsPacket *packet);
extern const MEfGsQword coltbl_2_00A51840[4];
extern int MMathRotTransPersClip(Vector4 *destination, const StudioCamera *camera,
                                 Matrix4 *matrix, const Vector4 *point);
extern Vector4 *MEfCalcAngle(Vector4 *destination, const Vector4 *from, const Vector4 *to);
extern Matrix4 *MMathRotateMatrixYXZ(Matrix4 *destination, Matrix4 *matrix, const Vector4 *angles);
#define DORA_STRIP_SEGMENTS 33
static inline void doraSetRegister(MEfGsQword *qword, unsigned char address, u64 data)
{
    qword->ad.data = data;
    qword->ad.address = address;
}
static inline MEfGsQword *doraOpenTag(MEfGsPacket *packet, int pre, int prim, int reg_count,
                                       u64 registers)
{
    MEfGsQword *qword = packet->current;

    qword->gif.tag = GIF_TAG_EOP | ((u64)pre << 46) | ((u64)prim << 47) | ((u64)reg_count << 60);
    qword->gif.registers = registers;
    packet->reg_count = reg_count;
    return qword + 1;
}
static inline void doraCloseTag(MEfGsPacket *packet, int nloop)
{
    int size;

    size = packet->reg_count * nloop + 1;
    packet->current->nloop |= nloop;
    packet->current += size;
    packet->count += size;
}

static void fnDORA_DP000(void *self, DoraHermiteState *work)
{
    DoraDrawState *state = (DoraDrawState *)work;
    MEfGsQword colour[2];
    MEfGsQword matrix[4];
    Vector4 angle;
    MEfGsQword offset[2];
    MEfGsQword vertex[2];
    StudioCamera *screen;
    Matrix4 *view;
    MEfGsPacket *packet;
    MEfGsPacket *strip_packet;
    MEfGsQword *setting;
    MEfGsQword *qword;
    float inner;
    float outer;
    int clip;
    int drawn;
    int axis;
    int side;
    int slot;

    screen = mefCamParams.screen;
    view = mefCamParams.matrix;
    packet = &state->mg_packet;
    MGsGPInit(packet, 0, 0);
    setting = packet->current;
    setting[0].gif.tag = MEF_GS_SETUP_TAG;
    setting[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->reg_count = 1;
    doraSetRegister(&setting[1], SCE_GS_TEST_1, MEF_GS_TEST_1_ALPHA_GREATER_ZGEQUAL);
    doraSetRegister(&setting[2], SCE_GS_ZBUF_1, MEF_GS_ZBUF_1_PSMZ24_NO_DEPTH_WRITE);
    doraSetRegister(&setting[3], SCE_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    doraCloseTag(packet, 3);

    packet = &state->mg_packet;
    strip_packet = packet;
    for (axis = 0; axis < 2; axis++) {
        Vu0SetOrigin(&offset[0]);
        Vu0SetOrigin(&offset[1]);
        for (side = -1; side < 2; side += 2) {
            inner = (float)side * -0.025f;
            outer = (float)side * 0.3f;
            drawn = 0;
            clip = 0;
            qword = doraOpenTag(&state->mg_packet, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
            for (slot = 0; slot < DORA_STRIP_SEGMENTS; slot++) {
                if (state->trail_active[slot] == 0) {
                    continue;
                }
                colour[0].whole = coltbl_2_00A51840[0].whole;
                colour[1].whole = coltbl_2_00A51840[1].whole;
                if (slot == 0 || slot == DORA_STRIP_SEGMENTS - 1) {
                    colour[0].channel[3] = colour[1].channel[3] = 0;
                    offset[0].element[axis] = offset[1].element[axis] = 0.0f;
                } else {
                    offset[0].element[axis] = inner;
                    offset[1].element[axis] = outer;
                }
                MEfCalcAngle(&angle, &state->trail[slot], &state->trail[slot + 1]);
                angle.z = state->path_progress[state->trail_segment[slot]];
                MMathRotateMatrixYXZ((Matrix4 *)matrix, 0, &angle);
                __asm__ __volatile__(
                    "lqc2 vf1, 0(%1)\n"
                    "vmove.w vf1, vf0\n"
                    "sqc2 vf1, 48(%0)\n"
                    :
                    : "r"(matrix), "r"(&state->trail[slot])
                    : "memory");
                MMathApplyMatrix(&angle, matrix, &offset[0].vector);
                if (MMathRotTransPersClip(&vertex[0].vector, screen, view, &angle)) {
                    clip = 2;
                } else if (clip != 0) {
                    clip--;
                    vertex[0].xyz.adc = MEF_GS_XYZ_ADC;
                }
                MMathApplyMatrix(&angle, matrix, &offset[1].vector);
                if (MMathRotTransPersClip(&vertex[1].vector, screen, view, &angle)) {
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
            if (drawn > 0) {
                doraCloseTag(strip_packet, drawn);
            }
        }
    }
    MGsGPTerm(strip_packet);
}

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

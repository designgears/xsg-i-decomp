#include "common.h"

typedef struct CollisionPoint {
    float x;
    float y;
    float z;
    float w;
} CollisionPoint;

static void swapf(float *left, float *right)
{
    float value = *left;

    *left = *right;
    *right = value;
}

static int CheckIntersect(CollisionPoint *a, CollisionPoint *b, CollisionPoint *c, CollisionPoint *d)
{
    float denom, t;
    int neg;

    denom = (d->x - c->x) * (b->z - a->z);
    t = (b->x - a->x) * (d->z - c->z);
    if (denom == t) {
        return 0;
    }
    denom -= t;
    if (denom < 0.0f) {
        neg = 1;
        denom = -denom;
    } else {
        neg = 0;
    }
    t = (c->z - a->z) * (b->x - a->x) - (c->x - a->x) * (b->z - a->z);
    if (neg) t = -t;
    if (t < 0.0f || denom < t) return 0;
    t = (c->z - a->z) * (d->x - c->x) - (c->x - a->x) * (d->z - c->z);
    if (neg) t = -t;
    if (t < 0.0f || denom < t) return 0;
    return 1;
}

static int GetIntersectionPointLineX(float *intersection, const float *line_start,
                                     const float *line_end, float x)
{
    if (line_start[0] == line_end[0]) {
        *intersection = 0;
        return 0;
    }

    *intersection = (x - line_start[0]) / (line_end[0] - line_start[0]);
    return 1;
}

static int GetIntersectionPointLineZ(float *intersection, const float *line_start,
                                     const float *line_end, float z)
{
    if (line_start[2] == line_end[2]) {
        *intersection = 0;
        return 0;
    }

    *intersection = (z - line_start[2]) / (line_end[2] - line_start[2]);
    return 1;
}

static int CheckSlope(const CollisionPoint *p0, const CollisionPoint *p1,
                      const CollisionPoint *p2, const CollisionPoint *p3)
{
    float cross = (p1->x - p0->x) * (p3->z - p2->z)
                - (p1->z - p0->z) * (p3->x - p2->x);

    if (cross == 0.0f)
        return 0;
    if (0.0f < cross)
        return 1;
    return -1;
}

static int CheckInBox(const CollisionPoint *point, const CollisionPoint *lower,
                      const CollisionPoint *upper)
{
    if (point->x < upper->x && lower->x < point->x
            && point->z < upper->z && lower->z < point->z)
        return 1;
    return 0;
}

typedef struct BallBoxCollisionArgs {
    CollisionPoint *box_corner_a;
    CollisionPoint *box_corner_b;
    CollisionPoint *line_start;
    CollisionPoint *line_end;
} BallBoxCollisionArgs;

typedef struct CollisionBoundsFrame {
    CollisionPoint bounds[2];
    unsigned char unmodeled_20[16];
} CollisionBoundsFrame;
typedef struct FractionCell {
    float value;
} FractionCell;

static int CheckBallBoxCollision(CollisionPoint *hit_position,
                                 const BallBoxCollisionArgs *args, int mode)
{
    CollisionBoundsFrame frame;
    FractionCell fraction_cell;
    CollisionPoint *line_start = args->line_start;
    CollisionPoint *corner_a = args->box_corner_a;
    CollisionPoint *corner_b = args->box_corner_b;
    int side;
    int slope;
    int intersection;
    fraction_cell.value = line_start->w;

    if (corner_a->x > corner_b->x) {
        frame.bounds[1].x = corner_a->x + fraction_cell.value;
        frame.bounds[0].x = corner_b->x - fraction_cell.value;
    } else {
        frame.bounds[1].x = corner_b->x + fraction_cell.value;
        frame.bounds[0].x = corner_a->x - fraction_cell.value;
    }
    if (corner_a->z > corner_b->z) {
        frame.bounds[1].z = corner_a->z + fraction_cell.value;
        frame.bounds[0].z = corner_b->z - fraction_cell.value;
    } else {
        frame.bounds[1].z = corner_b->z + fraction_cell.value;
        frame.bounds[0].z = corner_a->z - fraction_cell.value;
    }
    if (CheckInBox(line_start, &frame.bounds[0], &frame.bounds[1])) {
        if (hit_position) {
            /* Copy all four original lanes. VF31 is transient VU0 scratch;
             * no ACC/Q/VI/flags are read or modified. The compiler does not
             * allocate C values in VF registers. Caller point storage is
             * 16-byte aligned and the memory clobber covers the full copy. */
            __asm__ __volatile__(
                "lqc2 vf31, 0(%1)\n\tsqc2 vf31, 0(%0)"
                : : "r"(hit_position), "r"(args->line_start) : "memory");
        }
        return 1;
    }
    if (!CheckInBox(args->line_end, &frame.bounds[0], &frame.bounds[1])
            && !CheckIntersect(args->line_start, args->line_end,
                               &frame.bounds[0], &frame.bounds[1])) {
        swapf(&frame.bounds[0].x, &frame.bounds[1].x);
        if (!CheckIntersect(args->line_start, args->line_end,
                            &frame.bounds[0], &frame.bounds[1]))
            return 0;
        swapf(&frame.bounds[0].x, &frame.bounds[1].x);
    }
    line_start = args->line_start;
    side = 0;
    if (line_start->z <= frame.bounds[0].z)
        side = 1;
    else if (frame.bounds[1].z <= line_start->z)
        side = 2;
    if (line_start->x <= frame.bounds[0].x)
        side |= 4;
    else if (frame.bounds[1].x <= line_start->x)
        side |= 8;

    switch (side) {
    case 1:
        intersection = GetIntersectionPointLineZ(&fraction_cell.value, &args->line_start->x,
                                                &args->line_end->x, frame.bounds[0].z);
        if (!intersection)
            return intersection;
        side |= 16;
        break;
    case 2:
        intersection = GetIntersectionPointLineZ(&fraction_cell.value, &args->line_start->x,
                                                &args->line_end->x, frame.bounds[1].z);
        if (!intersection)
            return intersection;
        side |= 16;
        break;
    case 4:
        if (!GetIntersectionPointLineX(&fraction_cell.value, &args->line_start->x,
                                       &args->line_end->x, frame.bounds[0].x))
            return 0;
        break;
    case 8:
        if (!GetIntersectionPointLineX(&fraction_cell.value, &args->line_start->x,
                                       &args->line_end->x, frame.bounds[1].x))
            return 0;
        break;
    case 5:
        slope = CheckSlope(line_start, args->line_end, line_start, &frame.bounds[0]);
        if (slope != -1) {
            if (slope != 1)
                return 0;
            intersection = GetIntersectionPointLineZ(&fraction_cell.value,
                &args->line_start->x, &args->line_end->x, frame.bounds[0].z);
            if (!intersection)
                return intersection;
            side |= 16;
        } else {
            if (!GetIntersectionPointLineX(&fraction_cell.value, &args->line_start->x,
                                           &args->line_end->x, frame.bounds[0].x))
                return 0;
        }
        break;
    case 6:
        swapf(&frame.bounds[0].z, &frame.bounds[1].z);
        slope = CheckSlope(args->line_start, args->line_end,
                           args->line_start, &frame.bounds[1]);
        if (slope != -1) {
            if (slope != 1)
                return 0;
            if (!GetIntersectionPointLineX(&fraction_cell.value, &args->line_start->x,
                                           &args->line_end->x, frame.bounds[0].x))
                return 0;
        } else {
            intersection = GetIntersectionPointLineZ(&fraction_cell.value,
                &args->line_start->x, &args->line_end->x, frame.bounds[0].z);
            if (!intersection)
                return intersection;
            side |= 16;
        }
        swapf(&frame.bounds[0].z, &frame.bounds[1].z);
        break;
    case 9:
        swapf(&frame.bounds[0].x, &frame.bounds[1].x);
        slope = CheckSlope(args->line_start, args->line_end,
                           args->line_start, &frame.bounds[0]);
        if (slope != -1) {
            if (slope != 1)
                return 0;
            if (!GetIntersectionPointLineX(&fraction_cell.value, &args->line_start->x,
                                           &args->line_end->x, frame.bounds[0].x))
                return 0;
        } else {
            intersection = GetIntersectionPointLineZ(&fraction_cell.value,
                &args->line_start->x, &args->line_end->x, frame.bounds[0].z);
            if (!intersection)
                return intersection;
            side |= 16;
        }
        swapf(&frame.bounds[0].x, &frame.bounds[1].x);
        break;
    case 10:
        slope = CheckSlope(line_start, args->line_end, line_start, &frame.bounds[1]);
        if (slope != -1) {
            if (slope != 1)
                return 0;
            intersection = GetIntersectionPointLineZ(&fraction_cell.value,
                &args->line_start->x, &args->line_end->x, frame.bounds[1].z);
            if (!intersection)
                return intersection;
            side |= 16;
        } else {
            if (!GetIntersectionPointLineX(&fraction_cell.value, &args->line_start->x,
                                           &args->line_end->x, frame.bounds[1].x))
                return 0;
        }
        break;
    default:
        return 0;
    }
    if (hit_position) {
        fraction_cell.value -= 0.001f;
        switch (mode) {
        case 0:
        default:
            hit_position->x = (args->line_end->x - args->line_start->x) * fraction_cell.value
                + args->line_start->x;
            hit_position->y = (args->line_end->y - args->line_start->y) * fraction_cell.value
                + args->line_start->y;
            hit_position->z = (args->line_end->z - args->line_start->z) * fraction_cell.value
                + args->line_start->z;
            break;
        case 1:
            if (side & 16) {
                hit_position->x = args->line_end->x;
                hit_position->z = (args->line_end->z - args->line_start->z) * fraction_cell.value
                    + args->line_start->z;
            } else {
                hit_position->x = (args->line_end->x - args->line_start->x) * fraction_cell.value
                    + args->line_start->x;
                hit_position->z = args->line_end->z;
            }
            hit_position->y = (args->line_end->y - args->line_start->y) * fraction_cell.value
                + args->line_start->y;
            break;
        }
    }
    return 1;
}

typedef struct ParticleState {
    CollisionPoint position;
    CollisionPoint previous_position;
    CollisionPoint velocity;
    float gravity;
    float damping;
} ParticleState;

extern void ppSetPos(ParticleState *particle, float x, float y, float z);

extern ParticleState pos1;

extern ParticleState pos2;

extern CollisionPoint box1;

extern CollisionPoint box2;

extern CollisionPoint box3;

extern CollisionPoint box4;

extern CollisionPoint hitpos;

extern float radius;

extern int hit;

extern int mode_004DB9D8;

typedef struct InitTestCamera {
    unsigned char pad[0xA0];
    float a0, a4, a8;
    unsigned char pad2[0xD0 - 0xAC];
    float d0, d4, d8;
} InitTestCamera;
extern float D_004D826C;
extern float D_004D8270;
extern float D_004D8274;
extern float D_004D8278;
extern float D_004D827C;
extern float D_004D8280;
extern float D_004D8284;
extern float D_004D8288;
extern InitTestCamera *pCamera_004DC65C;
extern void xglStudioInit(void);
extern void xglStudioChange(int);
extern void xglStudioMainCameraInit(void);
extern void xglStudioGetCamera(void *camera_out, int camera_index);
extern void ppInit(ParticleState *particle);

static int InitTest(void)
{
    xglStudioInit();
    xglStudioChange(0);
    xglStudioMainCameraInit();
    xglStudioGetCamera(&pCamera_004DC65C, 0);
    pCamera_004DC65C->d4 = 16.0f;
    pCamera_004DC65C->a0 = 4.712389f;
    pCamera_004DC65C->d0 = 0.0f;
    pCamera_004DC65C->d8 = 0.0f;
    pCamera_004DC65C->a4 = 0.0f;
    pCamera_004DC65C->a8 = 0.0f;
    box1.x = -2.45f;
    box1.z = -0.401f;
    box1.y = 0.0f;
    box2.x = 1.7f;
    box2.z = 2.2f;
    box2.y = 0.0f;
    box3.x = -3.0f;
    box3.z = -3.2f;
    box3.y = 0.0f;
    box4.x = 1.7f;
    box4.z = -1.41f;
    box4.y = 0.0f;
    ppInit(&pos1);
    ppInit(&pos2);
    ppSetPos(&pos1, 0.0f, 0.0f, 3.0f);
    radius = 0.5f;
    ppSetPos(&pos2, 0.0f, 0.0f, 3.0f);
    pos1.gravity = 0.0f;
    pos1.damping = 0.8f;
    pos2.gravity = 0.0f;
    pos2.damping = 0.8f;
    hitpos.x = 0.0f;
    hitpos.z = 0.0f;
    hitpos.y = 0.0f;
    hitpos.w = radius;
    mode_004DB9D8 = 0;
    hit = 0;
    return 1;
}

#include "shared.h"
typedef union CollisionPadButtons {
    u64 bits;
    struct {
        u16 held;
        u16 pressed;
        u16 unmodeled_2c;
        u16 unmodeled_2e;
    } halves;
} CollisionPadButtons;
typedef struct CollisionPadState {
    u8 unmodeled_00[0x28];
    CollisionPadButtons state;
    u16 movement;
} CollisionPadState;
extern CollisionPadState PadData;
static int CheckBallBoxCollision(CollisionPoint *hit_position,
                                 const BallBoxCollisionArgs *args, int mode);
static int InitTest(void);
extern const float D_004D828C;
extern const char D_004CBC58[];
extern const char D_004CBC70[];
extern void xglRenderClearFrame(void);
extern void xglSleep(void);
extern void xglFontDebugPrintf(int x, int y, const char *text, ...);
extern void ppNextStart(ParticleState *particle);
extern void ppNextEnd(ParticleState *particle);

void ColliTest(void)
{
    BallBoxCollisionArgs collision;
    BallBoxCollisionArgs opposing_collision;
    ParticleState *particle;
    float move_step;

    xglRenderClearFrame();
    if (!InitTest())
        return;
    while (PadData.state.halves.pressed & 0x20)
        xglSleep();
    if ((PadData.state.bits & 0x08000100ULL) == 0x08000100ULL)
        return;

    move_step = D_004D828C;
    do {
        switch (mode_004DB9D8) {
        case 0:
        default:
            particle = &pos1;
            break;
        case 1:
            particle = &pos2;
            break;
        }
        ppNextStart(particle);

        if (PadData.movement & 0x1000)
            particle->position.z -= move_step;
        if (PadData.movement & 0x4000)
            particle->position.z += move_step;
        if (PadData.movement & 0x8000)
            particle->position.x -= move_step;
        if (PadData.movement & 0x2000)
            particle->position.x += move_step;

        switch (mode_004DB9D8) {
        case 0:
        default:
            if (!(PadData.state.halves.held & 0x40)) {
                collision.box_corner_a = &box1;
                collision.box_corner_b = &box2;
                collision.line_start = &particle->previous_position;
                collision.line_end = &particle->position;
                particle->previous_position.w = radius;
                particle->position.w = radius;
                hit = CheckBallBoxCollision(&hitpos, &collision, 1);
                if (hit) {
                    particle->position.x = hitpos.x;
                    particle->position.y = hitpos.y;
                    particle->position.z = hitpos.z;
                }
                collision.box_corner_a = &box3;
                collision.box_corner_b = &box4;
                hit = CheckBallBoxCollision(&hitpos, &collision, 1);
                if (hit) {
                    particle->position.x = hitpos.x;
                    particle->position.y = hitpos.y;
                    particle->position.z = hitpos.z;
                }
            }
            ppSetPos(&pos2, pos1.position.x, pos1.position.y, pos1.position.z);
            break;
        case 1:
            opposing_collision.box_corner_a = &box1;
            opposing_collision.box_corner_b = &box2;
            opposing_collision.line_start = &pos1.position;
            opposing_collision.line_end = &pos2.position;
            pos2.position.w = radius;
            pos1.position.w = radius;
            hit = CheckBallBoxCollision(&hitpos, &opposing_collision, 0);
            opposing_collision.box_corner_a = &box3;
            opposing_collision.box_corner_b = &box4;
            hit = CheckBallBoxCollision(&hitpos, &opposing_collision, 0);
            break;
        }
        ppNextEnd(particle);

        if (PadData.state.halves.pressed & 0x20) {
            switch (mode_004DB9D8) {
            case 0:
            default:
                mode_004DB9D8 = 1;
                ppSetPos(&pos2, pos1.position.x, pos1.position.y,
                         pos1.position.z);
                break;
            case 1:
                mode_004DB9D8 = 2;
                ppSetPos(&pos1, pos2.position.x, pos2.position.y,
                         pos2.position.z);
                break;
            }
        }
        if (mode_004DB9D8 == 1)
            pos2.position.w = radius;
        pos1.position.w = radius;
        if (hit)
            xglFontDebugPrintf(0, 8, D_004CBC58);
        xglFontDebugPrintf(0, 0, D_004CBC70);
        xglSleep();
    } while ((PadData.state.bits & 0x08000100ULL) != 0x08000100ULL);
}

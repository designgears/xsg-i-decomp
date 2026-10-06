#include "common.h"
#include "near_dir.h"
#include "main/xgl_studio.h"

typedef struct {
    u32 active;
} StudioCameraActivePrefix;

extern void xglStudioGetCamera(StudioCamera **camera_out, int camera_index);

float nearDir(float from, float to)
{
    float d;
    float t;
    int s;

    d = to - from;
    t = d * 0.31830987f;
    s = (int)t;
    if (__builtin_fabsf(t) >= 2.0f) {
        s = -1;
        if (!(t < 0.0f)) {
            s = 1;
        }
        d -= (float)(int)t * 3.14159274f;
        if ((int)t & 1) {
            d += (float)s * 6.28318548f;
        }
    } else {
        d -= (float)(s * 2) * 3.14159274f;
    }
    return d;
}

u8 actSequence[0x9800] = {0};

void SEQ_scaleSPL(Actor *actor)
{
    u8 *actor_bytes = (u8 *)actor;
    SequenceState *sequence =
        (SequenceState *)(actSequence + actor_bytes[0x80] * 0x260u);
    SplineTrack *track = (SplineTrack *)((u8 *)sequence + 0x1b8);
    void *spline = track->spline;
    float sample[3];

    if ((sequence->flags & 0xe0) == 0) {
        sequence->flags |= 0xe0;
        track->frame = 0;
    }

    SPL_getValueXYZ(sample, spline, (float)track->frame);

    actor->scale.x = sample[0];
    actor->scale.y = sample[1];
    actor->scale.z = sample[2];

    track->frame++;
    if (track->last_frame < (short)track->frame)
        sequence->state_flags &= ~0xe0;
}

typedef struct {
    void *spline;
    u8 unmodeled_04[0x10 - 0x04];
    int frames[3];
    u8 unmodeled_1c[0x20 - 0x1c];
    Vector4 start;
    Vector4 target;
    Vector4 delta;
} ScaleTrack;

void SEQ_scale(Actor *actor)
{
    u8 *actor_bytes = (u8 *)actor;
    Vector4 *scale = &actor->scale;
    SequenceState *sequence =
        (SequenceState *)(actSequence + actor_bytes[0x80] * 0x260u);
    ScaleTrack *track = (ScaleTrack *)((u8 *)sequence + 0x1b8);
    Vector4 *start = &track->start;
    Vector4 *target = &track->target;
    Vector4 *delta = &track->delta;
    float px;
    float py;
    float pz;
    u32 state_flags;
    u32 keep;
    Vector4 diff;

    if ((sequence->flags & 0x20u) == 0) {
        state_flags = sequence->state_flags;

        if (state_flags & 0x20u) {
            int frames;

            px = scale->x;
            start->x = px;
            frames = track->frames[0];

            if (frames > 0) {
                float step = 1.0f / (float)frames;

                delta->x = (target->x - px) * step;
            }

            sequence->flags |= 0x20u;
            *(u32 *)((u8 *)sequence + 0x34) = 0;
        } else {
            delta->x = 0.0f;
            sequence->flags |= 0x20u;
        }
    } else {
        state_flags = sequence->state_flags;
    }

    if ((sequence->flags & 0x40u) == 0) {
        if (state_flags & 0x40u) {
            int frames;

            py = scale->y;
            start->y = py;
            frames = track->frames[1];

            if (frames > 0) {
                float step = 1.0f / (float)frames;

                delta->y = (target->y - py) * step;
            }

            sequence->flags |= 0x40u;
            *(u32 *)((u8 *)sequence + 0x34) = 0;
        } else {
            delta->y = 0.0f;
            sequence->flags |= 0x40u;
        }
    }

    if ((sequence->flags & 0x80u) == 0) {
        state_flags = sequence->state_flags;

        if (state_flags & 0x80u) {
            int frames;

            pz = scale->z;
            start->z = pz;
            frames = track->frames[2];

            if (frames > 0) {
                float step = 1.0f / (float)frames;

                delta->z = (target->z - pz) * step;
            }

            sequence->flags |= 0x80u;
            *(u32 *)((u8 *)sequence + 0x34) = 0;
        } else {
            delta->z = 0.0f;
            sequence->flags |= 0x80u;
        }
    }

    if (state_flags & 0x20u) {
        diff.x = __builtin_fabsf(target->x - scale->x);

        if (diff.x <= __builtin_fabsf(delta->x)) {
            scale->x = target->x;
            keep = state_flags & ~0x20u;
            sequence->state_flags = keep;
            state_flags = keep;
        } else {
            scale->x = scale->x + delta->x;
        }
    }

    if (state_flags & 0x40u) {
        diff.y = __builtin_fabsf(target->y - scale->y);

        if (diff.y <= __builtin_fabsf(delta->y)) {
            scale->y = target->y;
            keep = state_flags & ~0x40u;
            sequence->state_flags = keep;
            state_flags = keep;
        } else {
            scale->y = scale->y + delta->y;
        }
    }

    if (state_flags & 0x80u) {
        diff.z = __builtin_fabsf(target->z - scale->z);

        if (diff.z <= __builtin_fabsf(delta->z)) {
            scale->z = target->z;
            keep = state_flags & ~0x80u;
            sequence->state_flags = keep;
            state_flags = keep;
        } else {
            scale->z = scale->z + delta->z;
        }
    }
}

void SEQ_rotate(Actor *actor) {
    Vector4 *rotation = &actor->rotation;
    Vector4 *position = &actor->position;
    u8 *temp_16;
    u8 *temp_17;
    u8 *temp_19;
    u8 *temp_21;
    u8 *temp_22;
    u8 *temp_23;
    float temp_f2;
    float temp_f2_2;
    float temp_f2_3;
    float temp_near;
    int temp_2;
    int temp_2_2;
    int temp_2_3;
    int temp_3;
    u32 var_3;
    u32 var_4;
    u32 var_2;
    u32 var_2_2;
    u32 var_2_3;
    u32 var_2_4;
    u32 var_2_5;
    u32 var_2_6;
    u32 var_2_7;
    u32 var_4_2;
    volatile float dbg_x;
    volatile float dbg_y;
    volatile float dbg_z;

    SequenceState *sequence;

    temp_16 = actSequence + (actor->number * 0x260);
    sequence = (SequenceState *)temp_16;
    var_3 = sequence->flags;
    temp_21 = temp_16 + 0xB8;
    temp_23 = temp_16 + 0xD8;
    temp_17 = temp_16 + 0xE8;
    temp_22 = temp_16 + 0xF8;
    temp_19 = (u8 *)*(u32 *)(temp_16 + 0xB8);
    if (!(var_3 & 2)) {
        var_4 = sequence->state_flags;
        if (var_4 & 2) {
            *(float *)temp_23 = (float)rotation->x;
            if (*(u32 *)(temp_21 + 0x1C) & 1) {
                *(float *)(temp_17 + 0x4) = xglAtan2(*(float *)(temp_19 + 0x14) - position->y, *(float *)(temp_19 + 0x18) - position->z);
            }
            temp_2 = *(int *)(temp_21 + 0x10);
            if (temp_2 > 0) {
                float step = 1.0f / (float)temp_2;
                *(float *)temp_22 = nearDir(*(float *)temp_23, *(float *)temp_17) * step;
            }
            *(u32 *)(temp_16 + 0x34) = 0;
            sequence->flags |= 2;
            var_2 = sequence->flags;
            var_4 = sequence->state_flags;
        } else {
            *(float *)temp_22 = 0.0f;
            sequence->flags |= 2;
            var_2 = sequence->flags;
        }
        var_3 = var_2;
    } else {
        var_4 = sequence->state_flags;
    }
    if (!(var_3 & 4)) {
        if (var_4 & 4) {
            *(float *)(temp_23 + 0x4) = (float)rotation->y;
            if (*(u32 *)(temp_21 + 0x1C) & 2) {
                *(float *)(temp_17 + 0x4) = xglAtan2(*(float *)(temp_19 + 0x10) - position->x, *(float *)(temp_19 + 0x18) - position->z);
            }
            temp_2_2 = *(int *)(temp_21 + 0x14);
            if (temp_2_2 > 0) {
                float step = 1.0f / (float)temp_2_2;
                *(float *)(temp_22 + 0x4) = nearDir(*(float *)(temp_23 + 0x4), *(float *)(temp_17 + 0x4)) * step;
            }
            *(u32 *)(temp_16 + 0x34) = 0;
            sequence->flags |= 4;
            var_2_3 = sequence->flags;
            var_4 = sequence->state_flags;
        } else {
            var_2_3 = var_3 | 4;
            sequence->flags = var_2_3;
            *(float *)(temp_22 + 0x4) = 0.0f;
        }
        var_3 = var_2_3;
    }
    var_2_2 = var_3 & 8;
    if (var_2_2 == 0) {
        if (var_4 & 8) {
            *(float *)(temp_23 + 0x8) = (float)rotation->z;
            if (*(u32 *)(temp_21 + 0x1C) & 4) {
                *(float *)(temp_17 + 0x4) = xglAtan2(*(float *)(temp_19 + 0x10) - position->x, *(float *)(temp_19 + 0x14) - position->y);
            }
            temp_2_3 = *(int *)(temp_21 + 0x18);
            if (temp_2_3 > 0) {
                float step = 1.0f / (float)temp_2_3;
                *(float *)(temp_22 + 0x8) = nearDir(*(float *)(temp_23 + 0x8), *(float *)(temp_17 + 0x8)) * step;
            }
            *(u32 *)(temp_16 + 0x34) = 0;
            sequence->flags |= 8;
            var_2_5 = sequence->flags;
            var_4 = sequence->state_flags;
        } else {
            var_2_5 = var_3 | 8;
            sequence->flags = var_2_5;
            *(float *)(temp_22 + 0x8) = 0.0f;
        }
    }
    var_2_4 = sequence->state_flags & 2;
    if (var_2_4 != 0) {
        if (*(u32 *)(temp_21 + 0x1C) & 1) {
            *(float *)(temp_17 + 0x4) = xglAtan2(*(float *)(temp_19 + 0x14) - position->y, *(float *)(temp_19 + 0x18) - position->z);
        }
        temp_near = nearDir(*(float *)temp_17, rotation->x);
        temp_f2 = *(float *)temp_22;
        dbg_x = __builtin_fabsf(temp_near);
        if (dbg_x <= __builtin_fabsf(temp_f2)) {
            var_4 = sequence->state_flags & ~2;
            rotation->x = *(float *)temp_17;
            sequence->state_flags = var_4;
            if (!(var_4 & ~0x10)) {
                sequence->state_flags = 0;
                var_4 = 0;
            }
        } else {
            var_4 = sequence->state_flags;
            rotation->x += temp_f2;
        }
    }
    var_2_6 = var_4 & 4;
    if (var_2_6 != 0) {
        if (*(u32 *)(temp_21 + 0x1C) & 2) {
            *(float *)(temp_17 + 0x4) = xglAtan2(*(float *)(temp_19 + 0x10) - position->x, *(float *)(temp_19 + 0x18) - position->z);
        }
        temp_near = nearDir(*(float *)(temp_17 + 0x4), rotation->y);
        temp_f2_2 = *(float *)(temp_22 + 0x4);
        dbg_y = __builtin_fabsf(temp_near);
        if (dbg_y <= __builtin_fabsf(temp_f2_2)) {
            var_4_2 = sequence->state_flags & ~4;
            rotation->y = *(float *)(temp_17 + 0x4);
            sequence->state_flags = var_4_2;
            if (!(var_4_2 & ~0x10)) {
                sequence->state_flags = 0;
                var_4_2 = 0;
            }
        } else {
            var_4_2 = sequence->state_flags;
            rotation->y += temp_f2_2;
        }
        var_4 = var_4_2;
    }
    var_2_7 = var_4 & 8;
    if (var_2_7 != 0) {
        if (*(u32 *)(temp_21 + 0x1C) & 4) {
            *(float *)(temp_17 + 0x4) = xglAtan2(*(float *)(temp_19 + 0x10) - position->x, *(float *)(temp_19 + 0x14) - position->y);
        }
        temp_near = nearDir(*(float *)(temp_17 + 0x8), rotation->z);
        temp_f2_3 = *(float *)(temp_22 + 0x8);
        dbg_z = __builtin_fabsf(temp_near);
        if (dbg_z <= __builtin_fabsf(temp_f2_3)) {
            temp_3 = sequence->state_flags & ~8;
            rotation->z = *(float *)(temp_17 + 0x8);
            sequence->state_flags = temp_3;
            if (!(temp_3 & ~0x10)) {
                sequence->state_flags = 0;
            }
        } else {
            rotation->z += temp_f2_3;
        }
    }
}

void SEQ_rotateSPL(Actor *actor)
{
    /* Byte +0x80 is the actor's own slot in the 64-entry `actor` array, the
     * number ACT_create stores there (0x00305de0) and the one ACT_info prints
     * as "ACT[%02x]"; it selects this actor's 0x260-byte actSequence entry.
     * It is past the recovered head of Actor, so it keeps the byte view. */
    u8 *actor_bytes = (u8 *)actor;
    SequenceState *sequence =
        (SequenceState *)(actSequence + actor_bytes[0x80] * 0x260u);
    /* The sequence entry carries several spline tracks; +0xb8 is the rotation
     * track, as +0x38 is the movement track SEQ_moveSPL below walks and
     * Java_xeno_Chr_move fills in (0x002fd398..0x002fd3e4). */
    SplineTrack *track = (SplineTrack *)((u8 *)sequence + 0xb8);
    void *spline = track->spline;
    float sample[3];

    if ((sequence->flags & 0xeu) == 0) {
        sequence->flags |= 0xeu;
        track->frame = 0;
    }

    SPL_getValueXYZ(sample, spline, (float)track->frame);

    actor->rotation.x = sample[0] / 180.0f * sequence_pi;
    actor->rotation.y = sample[1] / 180.0f * sequence_pi;
    actor->rotation.z = sample[2] / 180.0f * sequence_pi;

    track->frame++;
    if (track->last_frame < (short)track->frame)
        sequence->state_flags &= ~0xeu;
}

void SEQ_moveSPL(Actor *actor)
{
    /* +0x80 is this actor's slot number and +0x38 the sequence entry's movement
     * track, the one Java_xeno_Chr_move fills in before it installs this
     * routine (0x002fd398..0x002fd3e4). */
    u8 *actor_bytes = (u8 *)actor;
    SequenceState *sequence =
        (SequenceState *)(actSequence + actor_bytes[0x80] * 0x260u);
    SplineTrack *track = (SplineTrack *)((u8 *)sequence + 0x38);
    void *spline = track->spline;
    Vector4 sample;

    if ((sequence->flags & 0x1u) == 0) {
        sequence->flags |= 0x1u;
        track->frame = 0;
    }

    SPL_getValueXYZ(&sample.x, spline, (float)track->frame);

    if ((sequence->state_flags & 0x4u) == 0) {
        if (track->flags & 0x10u) {
            float dx = sample.x - actor->position.x;
            float dz = sample.z - actor->position.z;
            actor->rotation.y = xglAtan2(dx, dz);
        }
    }

    if ((sequence->state_flags & 0x10u) == 0) {
        /* +0x6f0 is the actor's second flags word: ACT_info prints it with the
         * same "FLAGS %08x" it uses for +0x00 (0x00306374) and ACT_pauseUpdate
         * sets and clears bit 0x80000 in it across all 64 entries
         * (0x00305ff0/0x00306060). It is past the recovered head of Actor. */
        u32 *move_flags = (u32 *)(actor_bytes + 0x6f0);
        Vector4 delta;
        float length;

        __asm__ __volatile__(
            "lqc2 $vf3,0(%1)\n\t"
            "lqc2 $vf2,0(%2)\n\t"
            "vsub.xyz $vf2xyz,$vf2xyz,$vf3xyz\n\t"
            "sqc2 $vf2,0(%0)\n\t"
            :
            : "r"(&delta), "r"(&actor->position), "r"(&sample)
            : "memory"
        );

        /* The original computes the distance and never reads it: `length` is
         * written by the call and dead afterwards. The call is the observable
         * effect, not its result. */
        xglVectorLength(&length, &delta);

        /* The original really does test the flag before setting it - the branch
         * is in its bytes - so this is not a redundant read-modify-write. */
        if ((*move_flags & 0x8u) == 0)
            *move_flags |= 0x8u;
    }

    actor->position.x = sample.x;
    actor->position.y = sample.y;
    actor->position.z = sample.z;

    track->frame++;
    if (track->last_frame < (short)track->frame)
        sequence->state_flags &= ~0x1u;
}

extern void ACT_setMotion(Actor *actor, int motion);
extern float xglSin(float angle);
extern float xglCos(float angle);
extern float D_004D84C0;
typedef struct {
    void *spline;
    int frames;
    u8 unmodeled_08[0x08];
    Vector4 start;
    u8 unmodeled_20[0x0C];
    float dist;
} MoveChrTrack;

void SEQ_moveChr(Actor *actor)
{
    u8 *actor_bytes = (u8 *)actor;
    Vector4 *position = &actor->position;
    SequenceState *sequence =
        (SequenceState *)(actSequence + actor_bytes[0x80] * 0x260u);
    MoveChrTrack *track = (MoveChrTrack *)((u8 *)sequence + 0x38);
    Vector4 *start = (Vector4 *)((u8 *)sequence + 0x48);
    float *dist = &track->dist;
    Vector4 *target = (Vector4 *)((u8 *)track->spline + 0x10);
    Vector4 *rotation = &actor->rotation;
    Vector4 delta;
    Vector4 disp;
    float angle;
    float px;
    float pz;
    u32 keep;
    int frames;

    if ((sequence->flags & 0x1u) == 0) {
        sequence->flags |= 0x1u;
        px = position->x;
        start->x = px;
        frames = track->frames;
        start->y = position->y;
        pz = position->z;
        start->z = pz;

        if (frames > 0) {
            float step = 1.0f / (float)frames;

            delta.x = (target->x - px) * step;
            delta.z = (target->z - pz) * step;
            delta.y = 0.0f;
            xglVectorLength(dist, &delta);
        }

        xglAtan2(target->x - position->x, target->z - position->z);
        *(u32 *)((u8 *)sequence + 0x34) = 0;
    }

    angle = xglAtan2(target->x - position->x, target->z - position->z);

    if ((sequence->state_flags & 0x4u) == 0) {
        rotation->y = angle;
    }

    disp.x = target->x - position->x;
    disp.z = target->z - position->z;
    disp.w = disp.x * disp.x + disp.z * disp.z;
    disp.w = __builtin_sqrtf(disp.w);

    if (disp.w < __builtin_sqrtf((*dist + 0.5f) * (*dist + 0.5f))) {
        keep = sequence->state_flags & ~1u;
        sequence->state_flags = keep;
        if ((keep & ~0x10u) == 0) {
            sequence->state_flags = 0;
        }
    } else {
        position->x = position->x + xglSin(angle) * *dist;
        position->z = position->z + xglCos(angle) * *dist;

        if ((sequence->state_flags & 0x10u) == 0) {
            u32 *move_flags = (u32 *)(actor_bytes + 0x6f0);

            if ((*move_flags & 0x8u) == 0) {
                *move_flags |= 0x8u;
            }

            if (*dist <= D_004D84C0) {
                ACT_setMotion(actor, 1);
            } else {
                ACT_setMotion(actor, 3);
            }
        }

        *(u32 *)((u8 *)sequence + 0x34) += 1;
    }
}

extern int mtnIDLE_0[4];
typedef struct {
    int unk00;
    int unk04;
    int motion;
    short unk0c;
    short unk0e;
    unsigned short a;
    unsigned short b;
    char c;
    unsigned char pad[3];
    int d;
    float e;
} SetMotionTrack;

void SEQ_setMotion(Actor *actor)
{
    u8 *actor_bytes = (u8 *)actor;
    SequenceState *sequence =
        (SequenceState *)(actSequence + actor_bytes[0x80] * 0x260u);
    SetMotionTrack *t;

    sequence->state_flags |= 0x10;
    t = (SetMotionTrack *)((u8 *)sequence + 0x138);
    t->motion = mtnIDLE_0[xglSRand() & 3];
    t->d = 1;
    t->e = 1.0f;
    t->a = -1;
    t->b = -1;
    t->c = 8;
}

extern float D_004D84C4;

void SEQ_setPositionRAND(Actor *actor)
{
    u8 *actor_bytes = (u8 *)actor;
    SequenceState *sequence =
        (SequenceState *)(actSequence + actor_bytes[0x80] * 0x260u);
    u8 *track = (u8 *)sequence + 0x38;
    float *p = (float *)((u8 *)sequence + 0x58);
    float x, z;

    *(float *)(track + 0x2c) = D_004D84C4;
    sequence->state_flags |= 1;
    *(int *)(track + 4) = -1;
    p[0] = (float)(xglSRand() & 7) - 4.0f;
    p[2] = (float)(xglSRand() & 7) - 4.0f;
    x = p[0];
    z = p[2];
    if (__builtin_fabsf(x) < __builtin_fabsf(z)) {
        p[0] = x + *(float *)((u8 *)sequence + 0x240);
        p[2] = actor->position.z;
    } else {
        p[0] = actor->position.x;
        p[2] = z + *(float *)((u8 *)sequence + 0x248);
    }
}

extern void ACT_setMotion(Actor *actor, int motion);

extern void *GameLoopState[];

typedef struct {
    void *target;
    int unk04[4];
    int count;
    int unk18;
    int flags;
} RotY2PlayerTrack;

extern void SEQ_rotate(Actor *actor);

void SEQ_setRotY2Player(Actor *actor)
{
    u8 *actor_bytes = (u8 *)actor;
    SequenceState *sequence =
        (SequenceState *)(actSequence + actor_bytes[0x80] * 0x260u);
    RotY2PlayerTrack *track = (RotY2PlayerTrack *)((u8 *)sequence + 0xb8);

    sequence->handler[1] = SEQ_rotate;
    sequence->state_flags |= 4;
    track->target = GameLoopState[1];
    track->count = 10;
    track->flags |= 2;
    ACT_setMotion(actor, 7);
}

extern float D_004D84C8;
extern float D_004D84CC;
extern float xglSin(float angle);
extern float xglCos(float angle);
typedef struct {
    void *spline;
    int frames;
    u8 unmodeled_08[0x08];
    Vector4 start;
    Vector4 target;
} MoveNPCXZTrack;

void SEQ_moveNPC_XZ(Actor *actor)
{
    u8 *actor_bytes = (u8 *)actor;
    Vector4 *position = &actor->position;
    SequenceState *sequence =
        (SequenceState *)(actSequence + actor_bytes[0x80] * 0x260u);
    MoveNPCXZTrack *track = (MoveNPCXZTrack *)((u8 *)sequence + 0x38);
    Vector4 *start = (Vector4 *)((u8 *)sequence + 0x48);
    Vector4 *target = (Vector4 *)((u8 *)sequence + 0x58);
    float *dist = &target->w;
    Vector4 *rotation = &actor->rotation;
    Vector4 delta;
    Vector4 disp;
    float angle;
    float px;
    float pz;
    u32 keep;
    int frames;

    if ((sequence->flags & 0x1u) == 0) {
        sequence->flags |= 0x1u;
        px = position->x;
        start->x = px;
        frames = track->frames;
        start->y = position->y;
        pz = position->z;
        start->z = pz;

        if (frames > 0) {
            float step = 1.0f / (float)frames;

            delta.x = (target->x - px) * step;
            delta.y = 0.0f;
            delta.z = (target->z - pz) * step;
            xglVectorLength(dist, &delta);
            xglAtan2(target->x - position->x, target->z - position->z);
        }
        *(u32 *)((u8 *)sequence + 0x34) = 0;
    }

    angle = xglAtan2(target->x - position->x, target->z - position->z);

    if ((sequence->state_flags & 0x4u) == 0) {
        rotation->y = rotation->y + nearDir(rotation->y, angle) * D_004D84C8;
    }

    if ((sequence->state_flags & 0x10u) == 0) {
        u32 *move_flags = (u32 *)(actor_bytes + 0x6f0);

        if ((*move_flags & 0x8u) == 0) {
            *move_flags |= 0x8u;
        }

        if (*dist <= D_004D84CC) {
            ACT_setMotion(actor, 2);
        } else {
            ACT_setMotion(actor, 4);
        }
    }

    disp.x = target->x - position->x;
    disp.z = target->z - position->z;
    disp.w = disp.x * disp.x + disp.z * disp.z;
    disp.w = __builtin_sqrtf(disp.w);

    if (disp.w <= __builtin_sqrtf(*dist * *dist)) {
        keep = sequence->state_flags & ~1u;
        sequence->state_flags = keep;
        if ((keep & ~0x10u) == 0) {
            sequence->state_flags = 0;
        }
    }

    position->x = position->x + xglSin(angle) * *dist;
    position->z = position->z + xglCos(angle) * *dist;
    *(u32 *)((u8 *)sequence + 0x34) += 1;
}

extern float D_004D84D0;
extern float xglSin(float angle);
extern float xglCos(float angle);
typedef struct {
    void *spline;
    int frames;
    u8 unmodeled_08[0x08];
    Vector4 start;
    Vector4 target;
} MoveXZTrack;

void SEQ_moveXZ(Actor *actor)
{
    u8 *actor_bytes = (u8 *)actor;
    Vector4 *position = &actor->position;
    SequenceState *sequence =
        (SequenceState *)(actSequence + actor_bytes[0x80] * 0x260u);
    MoveXZTrack *track = (MoveXZTrack *)((u8 *)sequence + 0x38);
    Vector4 *start = (Vector4 *)((u8 *)sequence + 0x48);
    Vector4 *target = (Vector4 *)((u8 *)sequence + 0x58);
    float *dist = &target->w;
    Vector4 *rotation = &actor->rotation;
    Vector4 delta;
    Vector4 disp;
    float angle;
    float px;
    float pz;
    u32 keep;
    int frames;

    if ((sequence->flags & 0x1u) == 0) {
        sequence->flags |= 0x1u;
        px = position->x;
        start->x = px;
        frames = track->frames;
        start->y = position->y;
        pz = position->z;
        start->z = pz;

        if (frames > 0) {
            float step = 1.0f / (float)frames;

            delta.x = (target->x - px) * step;
            delta.y = 0.0f;
            delta.z = (target->z - pz) * step;
            xglVectorLength(dist, &delta);
            xglAtan2(target->x - position->x, target->z - position->z);
        }
        *(u32 *)((u8 *)sequence + 0x34) = 0;
    }

    angle = xglAtan2(target->x - position->x, target->z - position->z);

    if ((sequence->state_flags & 0x4u) == 0) {
        rotation->y = rotation->y + nearDir(rotation->y, angle) * D_004D84D0;
    }

    if ((sequence->state_flags & 0x10u) == 0) {
        u32 *move_flags = (u32 *)(actor_bytes + 0x6f0);

        if ((*move_flags & 0x8u) == 0) {
            *move_flags |= 0x8u;
        }
    }

    disp.x = target->x - position->x;
    disp.z = target->z - position->z;
    disp.w = disp.x * disp.x + disp.z * disp.z;
    disp.w = __builtin_sqrtf(disp.w);

    if (disp.w <= __builtin_sqrtf(*dist * *dist)) {
        keep = sequence->state_flags & ~1u;
        sequence->state_flags = keep;
        if ((keep & ~0x10u) == 0) {
            sequence->state_flags = 0;
        }
    }

    position->x = position->x + xglSin(angle) * *dist;
    position->z = position->z + xglCos(angle) * *dist;
    *(u32 *)((u8 *)sequence + 0x34) += 1;
}

extern float D_004D84D4;
extern void ACT_setMotion2(void *actor, int motion, int flags);
typedef struct {
    float accumulated;
    u32 unk04;
    int motion;
    short unk0c;
    short unk0e;
    short frame_from;
    short frame_to;
    u8 mode;
    u8 unmodeled_15[3];
    u32 flags;
    float speed;
} MotionTrack;
typedef struct {
    u32 flags;
    float motion_time;
    float speed;
    float f6fc;
    float f700;
    u8 unmodeled_14[0x24 - 0x14];
    float f714;
} ActorMotion;

void SEQ_motion(Actor *actor)
{
    u8 *actor_bytes = (u8 *)actor;
    ActorMotion *motion = (ActorMotion *)(actor_bytes + 0x6f0);
    SequenceState *sequence =
        (SequenceState *)(actSequence + actor_bytes[0x80] * 0x260u);
    MotionTrack *track = (MotionTrack *)((u8 *)sequence + 0x138);

    if ((sequence->flags & 0x10u) == 0) {
        float scale = D_004D84D4;
        u32 flags;

        sequence->flags |= 0x10u;
        motion->flags |= 1u;
        motion->f714 = (float)track->mode * scale;
        motion->flags &= ~1u;
        flags = track->flags | 0x20000000u;
        track->flags = flags;
        motion->speed = track->speed * scale;
        ACT_setMotion2(actor, track->motion, flags);
        if (track->frame_from >= 0) {
            float lo = (float)track->frame_from * scale;
            float hi = (float)track->frame_to * scale;

            motion->f6fc = lo;
            motion->f700 = hi;
            if (motion->speed < 0.0f) {
                motion->motion_time = hi;
            } else {
                motion->motion_time = lo;
            }
        }
    }
    if ((sequence->state_flags & ~0x10u) == 0) {
        if ((motion->flags & 0x1000u) != 0) {
            sequence->state_flags = 0;
        }
    }
    track->accumulated = track->accumulated + motion->speed;
}

INCLUDE_ASM("asm/main/nonmatchings/near_dir", ACT_updateSequence);

INCLUDE_ASM("asm/main/nonmatchings/near_dir", ACT_updateNPC);

/* Defined below in this file; forward-declared for the two callers here. */
extern void ACT_updateMotion(Actor *actor);

/* Undulate.c (main); no header is published for it yet. */
extern float UnduGet(float x, float z);

void ACT_updateDefault(Actor *actor)
{
    if (actor->flags & 0x40)
        actor->position.y = UnduGet(actor->position.x, actor->position.z);
    ACT_updateMotion(actor);
}

void ACT_updateRECTRand(Actor *actor)
{
    ACT_updateMotion(actor);
}

INCLUDE_ASM("asm/main/nonmatchings/near_dir", ACT_updatePlayer);

void ACT_initSequenceAt(Actor *actor)
{
    SequenceState *sequence =
        (SequenceState *)(actSequence + actor->number * 0x260u);

    sequence->flags = 0;
    sequence->state_flags = 0;
    sequence->cleared_on_init = 0;
    sequence->cleared_on_init_run[0] = 0;
    sequence->cleared_on_init_run[1] = 0;
    sequence->cleared_on_init_run[2] = 0;
    sequence->cleared_on_init_run[3] = 0;
    sequence->handler[0] = 0;
    sequence->handler[1] = 0;
    sequence->handler[2] = 0;
    sequence->handler[3] = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/near_dir", ACT_initSequence);

extern u8 actor[64 * 0xa70];

void ACT_initVMObject(u8 *p)
{
    int i;
    if (p == 0) {
        for (i = 0; i < 64; i++) {
            p = (u8 *)actor;
            *(int *)p = 0x20;
            p[0x90] = 1;
            p[0x91] = 0x50;
            *(int *)(p + 0x4C0) = 0;
        }
        return;
    }
    *(int *)(p + 0x4C0) = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/near_dir", ACT_createChr);

extern Actor *ACT_create(void);
extern void ACT_updateNPC();
extern void UnduParamInit(void *undulation);
static inline void ACT_initSequenceAt_i(Actor *actor)
{
    SequenceState *sequence = (SequenceState *)(actSequence + actor->number * 0x260u);
    sequence->flags = 0;
    sequence->state_flags = 0;
    sequence->cleared_on_init = 0;
    sequence->cleared_on_init_run[0] = 0;
    sequence->cleared_on_init_run[1] = 0;
    sequence->cleared_on_init_run[2] = 0;
    sequence->cleared_on_init_run[3] = 0;
    sequence->handler[0] = 0;
    sequence->handler[1] = 0;
    sequence->handler[2] = 0;
    sequence->handler[3] = 0;
}

Actor *ACT_createNPC(void)
{
    Actor *a = ACT_create();

    if (a) {
        UnduParamInit(&a->undulation);
        a->update = ACT_updateNPC;
        *(u8 *)((u8 *)a + 0x81) = 0;
        *(u32 *)((u8 *)a + 0x4c0) = 0;
        *(short *)((u8 *)a + 0x88) = 0;
        ACT_initSequenceAt_i(a);
    }
    return a;
}

void ACT_info(void)
{
    int i;

    for (i = 0; i < 64; i++) {
    }
}

static StudioCamera *getCurrentCamera(void)
{
    StudioCamera *camera;
    int camera_index;

    for (camera_index = 0; camera_index < 8; camera_index++) {
        xglStudioGetCamera(&camera, camera_index);
        if (((StudioCameraActivePrefix *)camera)->active)
            return camera;
    }

    return 0;
}

/* play.c (main); no header is published for it yet. */
extern void *PLAY_getCurrent(void);
/* Defined below in this file. */
extern void SEQ_motion(Actor *actor);
/* Undulate.c (main); no header is published for it yet. */
extern float UnduGet2(void *destination, float x, float z);
/* include/main/xgl_2.h's own declaration: not included directly because it
 * (through include/shared.h) redefines struct Vector4, which this TU's own
 * near_dir.h already defines locally. */
extern void xglMatrixStackUnit(void);

void ACT_updateMPack(Actor *actor)
{
    void *play = PLAY_getCurrent();
    Actor *linked_actor;

    xglMatrixStackUnit();
    SEQ_motion(actor);
    ACT_updateMotion(actor);

    /* PLAY_getCurrent's object (play.c) is not recovered; +0x44 is the one
     * float this TU reads from it, mirrored into motion_time. */
    actor->motion_time = *(float *)((u8 *)play + 0x44);
    actor->undulation = 0;

    linked_actor = actor->linked_actor;
    UnduGet2(&actor->undulation, linked_actor->velocity.x,
             linked_actor->velocity.z);
}

/* Undulate.c (main); no header is published for it yet, and its own
 * UnduWork type is TU-local there, so the destination stays opaque here,
 * exactly as UnduGet2 above already does. */
extern void UnduParamInit(void *undulation);
/* The 64-entry array itself (see the Actor doc comment above); no accepted
 * function in this TU has needed the symbol until now. */
extern u8 actor[64 * 0xa70];

void ACT_initScene(void)
{
    Actor *entry;
    int remaining;

    entry = (Actor *)actor;
    remaining = 63;
    do {
        remaining--;
        entry->flags = 0x20;
        entry->shadow_kind = 1;
        entry->shadow_size = 0x50;
        entry->cleared_on_scene_init = 0;
        entry->state_flags = 0;
        entry->update = 0;
        entry->draw = 0;
        entry->position.w = 1.0f;
        entry->velocity.w = 0.0f;
        entry->acceleration.w = 0.0f;
        UnduParamInit(&entry->undulation);
        entry = (Actor *)((u8 *)entry + 0xa70);
    } while (remaining >= 0);
}

/* Still original asm (0x003083b8, main); ACT_setHand's own call to it is a
 * genuine tail call (j, not jal), so its return value, if any, is never
 * observed here. */
extern void ACT_setHumanHand(Actor *actor, int hand);

void ACT_setHand(Actor *actor, int hand)
{
    int category;
    int code;

    if (actor->state_flags & 0xf000) {
        return;
    }

    category = hand & 0xf0;
    code = hand & 0xf;
    switch (category) {
    case 0x00:
        code |= code << 8;
        break;
    case 0x10:
        code |= 0x8000;
        break;
    case 0x20:
        code = (code << 8) | 0x80;
        break;
    }
    ACT_setHumanHand(actor, code);
}

/* nml_model_set.c (main); no header is published for it yet. */
extern void nmlModelSetToumei(int enabled);
extern void nmlModelSetZwrite(int enabled);
extern void nmlModelSetStencil(int enabled);
extern void nmlModelSetFilter(int mode, float filter_param_1, float filter_param_2);
extern void nmlModelSetTransparency(float transparency);
extern void nmlModelSetReflTransparency(float transparency);

void ACT_filterGuno(Actor *actor)
{
    nmlModelSetToumei(1);
    nmlModelSetZwrite(1);
    nmlModelSetStencil(1);
    nmlModelSetFilter(1, actor->filter_param_1, actor->filter_param_2);
    nmlModelSetTransparency(actor->transparency);
    nmlModelSetReflTransparency(actor->refl_transparency);
}

void ACT_filterStealth(Actor *actor)
{
    nmlModelSetToumei(1);
    nmlModelSetZwrite(1);
    nmlModelSetStencil(1);
    nmlModelSetFilter(2, actor->filter_param_1, actor->filter_param_2);
    nmlModelSetTransparency(actor->transparency);
    nmlModelSetReflTransparency(actor->refl_transparency);
}

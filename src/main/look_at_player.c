#include "common.h"
#include "shared.h"

typedef struct PlayerLookAtState {
    u32 unmodeled_00;
    void *player_actor;
    u32 unmodeled_08;
    u32 unmodeled_0c;
    u32 flags;
    u8 unmodeled_14[0x29f2d];
    signed char look_at_target;
} PlayerLookAtState;

typedef struct LookAtPlayerActor {
    u32 flags;
    void (*update)(struct LookAtPlayerActor *actor);
    void (*draw)(struct LookAtPlayerActor *actor);
    u32 quadword_alignment_gap;
    Vector4 position;
    u8 unmodeled_20[0x60];
    u8 number;
    u8 unmodeled_81[0x9ec - 0x81];
    s16 look_at_timer;
    s16 look_at_target;
    u8 unmodeled_9f0[0xa70 - 0x9f0];
} LookAtPlayerActor;

typedef union PlayerAimVector {
    Vector4 vector;
    u64 doublewords[2];
} PlayerAimVector;

typedef struct PlayerAimMapUnit {
    u8 unmodeled_00[0x10];
    PlayerAimVector position;
    u8 unmodeled_20[0x2e0];
} PlayerAimMapUnit;

extern PlayerLookAtState GameLoopState;
extern LookAtPlayerActor actor[64];
extern PlayerAimMapUnit MapUnit[64];

int Get_MostNear_Actor(LookAtPlayerActor *player_actor);
void Actor_LookAt_Set(LookAtPlayerActor *player_actor, int mode, Vector4 *target);
void Actor_LookAt_Release(LookAtPlayerActor *player_actor, int mode);
void Actor_LookAt(LookAtPlayerActor *player_actor);
void PlayerLookAtAim(void);

void LookAt_Player(LookAtPlayerActor *player_actor)
{
    int nearest_actor_index;
    signed char look_at_target;

    nearest_actor_index = Get_MostNear_Actor(player_actor);
    if (player_actor->look_at_timer == 2) {
        Actor_LookAt(player_actor);
        return;
    }
    if (GameLoopState.flags & 0x8000) {
        Actor_LookAt_Release(player_actor, 0);
        return;
    }
    look_at_target = GameLoopState.look_at_target;
    if (look_at_target != -1) {
        PlayerLookAtAim();
        return;
    }

    Actor_LookAt_Release(player_actor, 0);
    if (nearest_actor_index != look_at_target &&
        !(actor[nearest_actor_index].flags & 8)) {
        actor[player_actor->number].look_at_target = nearest_actor_index;
        Actor_LookAt_Set(player_actor, 1, &actor[nearest_actor_index].position);
        Actor_LookAt(player_actor);
        return;
    }
    Actor_LookAt_Release(player_actor, 1);
}

extern u8 actSequence[];
extern u8 enepc[];
void UnduParamInit(void *p);
void sefDeleteEffectCf(void *p);
void Actor_LookAt_Init(LookAtPlayerActor *a);

void Player_System_Init(LookAtPlayerActor *player_actor)
{
    u8 *p = (u8 *)player_actor;
    unsigned int number;
    int *seq;
    int *ip;
    int i;
    int one;
    u8 *cp;
    u8 *gls;
    u8 *ene;

    number = player_actor->number;
    *(void **)(p + 0x124) = (void *)0x70000;
    p[0x81] = 0;
    *(int *)(p + 0x4C0) = 0;
    *(short *)(p + 0x88) = 0;
    *(float *)(p + 0x6C) = 1.0f;
    *(float *)(p + 0x68) = 1.0f;
    *(float *)(p + 0x64) = 1.0f;
    *(float *)(p + 0x60) = 1.0f;
    seq = (int *)(actSequence + number * 0x260);
    seq[0] = 0;
    seq[1] = 0;
    seq[3] = 0;
    seq[5] = 0;
    seq[6] = 0;
    seq[7] = 0;
    seq[8] = 0;
    seq[9] = 0;
    seq[10] = 0;
    seq[11] = 0;
    seq[0x30 / 4] = 0;
    ene = enepc + number * 0x38B0;
    UnduParamInit(p + 0x4C8);
    *(short *)(p + 0x4D0) = 0x1910;
    for (i = 7; i >= 0; i--) {
        ((int *)(p + 0xA10))[i] = 0;
    }
    *(short *)(p + 0xA30) = -1;
    *(short *)(p + 0xA32) = 0;
    gls = (u8 *)&GameLoopState;
    if (*(void **)(gls + 0x2A014) != 0) {
        sefDeleteEffectCf(*(void **)(gls + 0x2A014));
    }
    ip = (int *)(gls + 0x29FC0);
    one = 1;
    cp = gls + 0x29FB0;
    *(void **)(gls + 0x2A014) = 0;
    *(short *)((u8 *)((PlayerLookAtState *)gls)->player_actor + 0x9F0) = 0;
    *(short *)(ene + 0x3830) = 0;
    for (i = 0; i < 16; i++) {
        cp[-0x10] = 0;
        *ip++ = 0;
        *cp++ = one;
    }
    Actor_LookAt_Init(player_actor);
}

/* Player-movement speed thresholds and vector scaling rate GameCfPlayerMove
 * compares/applies each frame; GameCfPlayerMoveInit restores their defaults. */
extern int WALK_THRESHOLD_I;
extern float WALK_THRESHOLD_F;
extern int RUN_THRESHOLD_I;
extern float RUN_THRESHOLD_F;
extern float VECTOR_RATE;

extern int F2I(float value);

void GameCfPlayerMoveParamSet(float walkThreshold, float runThreshold, float vectorRate)
{
    int walkThresholdInt;
    int runThresholdInt;

    walkThresholdInt = F2I(walkThreshold);
    WALK_THRESHOLD_F = walkThreshold;
    WALK_THRESHOLD_I = walkThresholdInt;
    runThresholdInt = F2I(runThreshold);
    RUN_THRESHOLD_F = runThreshold;
    VECTOR_RATE = vectorRate;
    RUN_THRESHOLD_I = runThresholdInt;
}

extern const float D_004D7C0C;

void GameCfPlayerMoveInit(void)
{
    WALK_THRESHOLD_I = 32;
    WALK_THRESHOLD_F = 32.0f;
    RUN_THRESHOLD_I = 96;
    RUN_THRESHOLD_F = 96.0f;
    VECTOR_RATE = D_004D7C0C;
}

INCLUDE_ASM("asm/main/nonmatchings/look_at_player", GameCfPlayerMove);

extern float D_004D7C34;
extern float D_004D7C38;
int CheckActorExist(void *actor);
#define ACT_RADIUS(a) (*(float *)((a)->unmodeled_81 + (0x9e8 - 0x81)))

static short HitCheckActor(LookAtPlayerActor *self)
{
    short i;
    float min_r = D_004D7C34;
    float max_dy = D_004D7C38;
    float dx, dz;
    char *base = (char *)actor;
    float *p = (float *)(base + 0x9e0);
    int off = 0;

    for (i = 0; i < 64; i++, p += 0xa70 / 4, off += 0xa70) {
        if (CheckActorExist((void *)(off + (int)base)) != 0 && self->number != i
            && !(p[2] < min_r)
            && !(max_dy < __builtin_fabsf(self->position.y - p[-0x9cc / 4]))) {
            dx = self->position.x - p[-0x9d0 / 4];
            dz = self->position.z - p[-0x9c8 / 4];
            if (__builtin_sqrtf(dx * dx + dz * dz) <= ACT_RADIUS(self) + p[2])
                return i;
        }
    }
    return -1;
}

extern float D_004D7C3C;
float Get_Angle(Vector4 *first, Vector4 *second);
float xglSin(float a);
float xglCos(float a);
int HitCheckMapUnit(LookAtPlayerActor *a);
#define ACT_VEL(a) ((Vector4 *)((a)->unmodeled_20 + 0x10))

static int NyuruActor(LookAtPlayerActor *self, LookAtPlayerActor *other)
{
    float angle;
    float r;
    Vector4 t;
    short hit;

    angle = Get_Angle(&self->position, &other->position);
    self->position.x -= ACT_VEL(self)->x;
    self->position.z -= ACT_VEL(self)->z;
    r = ACT_RADIUS(self) + ACT_RADIUS(other) + D_004D7C3C;
    t.x = other->position.x - r * xglSin(angle);
    t.z = other->position.z - r * xglCos(angle);
    ACT_VEL(self)->x = t.x - self->position.x;
    ACT_VEL(self)->z = t.z - self->position.z;
    self->position.x += ACT_VEL(self)->x;
    self->position.z += ACT_VEL(self)->z;
    hit = HitCheckActor(self);
    if (hit != -1) {
        self->position.x -= ACT_VEL(self)->x;
        self->position.z -= ACT_VEL(self)->z;
        return 0;
    }
    if (HitCheckMapUnit(self) != hit) {
        self->position.x -= ACT_VEL(self)->x;
        self->position.z -= ACT_VEL(self)->z;
        return 0;
    }
    return 1;
}

void PlayerLookAtAim(void)
{
    int look_at_target;
    void *player_actor;
    PlayerAimVector target_position;

    look_at_target = GameLoopState.look_at_target;
    player_actor = GameLoopState.player_actor;
    target_position = MapUnit[look_at_target].position;
    target_position.vector.y += 1.0f;
    Actor_LookAt_Set(player_actor, 0, &target_position.vector);
    Actor_LookAt(player_actor);
}

INCLUDE_ASM("asm/main/nonmatchings/look_at_player", GameCfPlayerLoadResource);

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

INCLUDE_ASM("asm/main/nonmatchings/look_at_player", Player_System_Init);

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

INCLUDE_ASM("asm/main/nonmatchings/look_at_player", NyuruActor);

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

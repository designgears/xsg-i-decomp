#include "common.h"
#include "shared.h"

/* Partial views of the actor / enemy work arrays used by Check_Actor. */
typedef struct CheckActorBody {
    float sort_offset;
    float target_angle;
    float radius;
    short lookat_timer;
    unsigned char unmodeled_0e[2];
    short encount_lock;
    unsigned char unmodeled_12[0xe];
    Vector4 saved_position;
} CheckActorBody;

typedef struct CheckActorRecord {
    u32 flags;
    unsigned char unmodeled_04[0x0c];
    Vector4 position;
    unsigned char unmodeled_20[0x62];
    unsigned char kind;
    unsigned char unmodeled_83[3];
    short sound_effect_id;
    unsigned char unmodeled_88[0x9d8 - 0x88];
    long long light_blend_frames;
    CheckActorBody body;
    unsigned char unmodeled_a10[0xa70 - 0xa10];
} CheckActorRecord;

typedef struct CheckEnemyWork {
    unsigned char unmodeled_0000[0x37e0];
    u32 state_flags;
    unsigned char unmodeled_37e4[0x38b0 - 0x37e4];
} CheckEnemyWork;

extern CheckActorRecord actor[64];
extern CheckEnemyWork enepc[16];
extern CheckActorRecord *GameLoopState[64];
extern float Get_Distance(const Vector4 *from, const Vector4 *to);

#define ACTOR_COUNT 64

/* GameLoopState[1] is the player actor pointer. */
#define PLAYER_ACTOR (GameLoopState[1])

int Check_Actor(const Vector4 *position, short number)
{
    short i;

    for (i = 0; i < ACTOR_COUNT; ++i) {
        float distance;

        if (actor[i].sound_effect_id <= 0)
            continue;
        if (actor[i].kind == 1)
            continue;
        if ((enepc[number].state_flags & 1) && &actor[i] == PLAYER_ACTOR)
            continue;
        if ((enepc[i].state_flags & 1) && &actor[number] == PLAYER_ACTOR)
            continue;
        if (actor[i].body.radius == 0.0f)
            continue;
        if (actor[i].flags & 8)
            continue;
        if (number == i)
            continue;
        if (__builtin_fabsf(actor[i].position.y - position->y) >= 1.0f)
            continue;
        distance = Get_Distance(position, &actor[i].position);
        if (distance <= actor[number].body.radius + actor[i].body.radius)
            break;
    }

    return i != ACTOR_COUNT ? i : -1;
}

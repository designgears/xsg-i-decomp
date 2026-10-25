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

typedef struct GCPlayerEffect {
    unsigned char unmodeled_00[0x6BC];
    void *owner;                     /* +0x6BC */
} GCPlayerEffect;
typedef struct GCPlayer {
    unsigned int flags;              /* +0x00 */
    unsigned char unmodeled_04[0x0C];
    Vector4 position;                /* +0x10 */
    Vector4 previous_position;       /* +0x20 */
    Vector4 velocity;                /* +0x30 */
    Vector4 external_velocity;       /* +0x40 */
    float rotation_x;                /* +0x50 */
    float rotation_y;                /* +0x54 */
    float rotation_z;                /* +0x58 */
    float move_speed;                /* +0x5C */
    unsigned char unmodeled_60[0x4C8 - 0x60];
    struct {
        unsigned char unmodeled_00[8];
        unsigned short attr_mask;    /* +0x08 of the block (+0x4D0) */
        unsigned char unmodeled_0a[0x20 - 0x0A];
    } undulation;                    /* +0x4C8 */
    unsigned long long terrain_flags; /* +0x4E8 */
    unsigned char unmodeled_4f0[0x694 - 0x4F0];
    signed char stick_x;             /* +0x694 */
    signed char stick_y;             /* +0x695 */
    unsigned char unmodeled_696[0x6F8 - 0x696];
    float motion_speed;              /* +0x6F8 */
    unsigned char unmodeled_6fc[0x704 - 0x6FC];
    unsigned short motion_id;        /* +0x704 */
    unsigned char unmodeled_708[0x9A0 - 0x708];
    unsigned int render_flags;       /* +0x9A0 */
    unsigned char unmodeled_9a4[0x9C0 - 0x9A4];
    float filter_param;              /* +0x9C0 */
    unsigned char unmodeled_9c4[0x9D8 - 0x9C4];
    long long flash_frames;          /* +0x9D8 */
    unsigned char unmodeled_9e0[0x9E4 - 0x9E0];
    float aim_angle;                 /* +0x9E4 */
    unsigned char unmodeled_9e8[0x9F0 - 0x9E8];
    short flash_timer;               /* +0x9F0 */
    unsigned char unmodeled_9f2[0xA10 - 0x9F2];
    GCPlayerEffect *effects[8];      /* +0xA10 */
    short effect_timer;              /* +0xA30 */
    short effect_slot;               /* +0xA32 */
} GCPlayer;
typedef struct GCLoopState {
    unsigned int unmodeled_00;
    void *player_actor;              /* +0x04 */
    unsigned int unmodeled_08[2];
    unsigned int flags;              /* +0x10 */
    unsigned char unmodeled_14[0x0C];
    unsigned int map_flags;          /* +0x20 */
    unsigned char unmodeled_24[0xC0 - 0x24];
    unsigned char move_mode;         /* +0xC0 */
    unsigned char unmodeled_c4[0x2A014 - 0xC4];
    void *effect;                    /* +0x2A014 */
} GCLoopState;
extern int Check_EnemyBurn(void);
extern int Check_EnemyElec(void);
extern int Check_EnemyFound(void);
extern int xglSoundEffectCheckID(int id, int a);
extern void xglSoundEffectStopID(int id, int a);
extern int Ladder_Main(void *player);
extern int SCRIPT_getCfTime(void);
extern float xglSin(float a);
extern float xglCos(float a);
extern float atan2f(float y, float x);
extern float floorf(float x);
extern float fabsf(float x);
extern float sqrtf(float x);
extern int Get_Attr(void *position, int a, int b);
extern int Get_EffectCode(int attr);
extern void *sefCreateEffectCf(int effect_id, int effect_number, int a);
extern void ACT_setMotion2(void *player, int motion, int speed);
extern void ACT_updateMotion(void *player);
extern void Enemy_FindByEar(void *player, void *position);
extern void HitCheckMapUnitWithNyuru(void *player);
static int NyuruActor(LookAtPlayerActor *self, LookAtPlayerActor *other);
extern float UnduCheck(void *position, void *velocity, void *undulation);
extern void ShootMapUnit(void);
extern void GameIdLightSet(void *player, int a);
extern void Set_Shadow(void *player);
extern void Get_EnemyIDPos(void *position, void *target);
extern void Check_Locater(void *player);
extern short EfTab_Code[];
extern short EfTab_Intr[];
extern short EfTab_Loop[];
extern char EnemyTarget[];
extern char CfCameraDefine[];
typedef struct GCPadView {
    unsigned char unmodeled_00[0x28];
    unsigned short held;             /* +0x28 */
    unsigned char unmodeled_2a[0x66 - 0x2A];
    signed char stick_x;             /* +0x66 */
    signed char stick_y;             /* +0x67 */
} GCPadView;
extern GCPadView PadData;
extern float I2F(int value);
extern float rate_0[4];
extern const float D_004D7C10;
extern const float D_004D7C14;
extern const float D_004D7C18;
extern const float D_004D7C1C;
extern const float D_004D7C20;
extern const float D_004D7C24;
extern const float D_004D7C28;
extern const float D_004D7C2C;
extern const float D_004D7C30;
extern float rate_1[4];
extern unsigned char idx_2[16];
extern float vec_3[9][2];
extern int HitCheckMapUnit(LookAtPlayerActor *a);
static short HitCheckActor(LookAtPlayerActor *self);
#define GC_LOOP (*(GCLoopState *)&GameLoopState)
#define GC_PAD16(off) (*(unsigned short *)((char *)&PadData + (off)))
#define GC_PAD8(off) (*(signed char *)((char *)&PadData + (off)))
float Get_Cursol_by_Reduce_Speed_Angle_Loop(float, float, float);

void GameCfPlayerMove(void)
{
    GCPlayer *player = (GCPlayer *)GC_LOOP.player_actor;
    float run_scale = 1.0f;
    float speed;
    int stick_speed;
    int terrain_bits;
    int ground_effect;
    int hit;
    PlayerAimVector position_before_move;
    const Vector4 *before_move;

    if (Check_EnemyBurn() == 0 && xglSoundEffectCheckID(0x10009, 0) != 0) {
        xglSoundEffectStopID(0x10009, 0);
    }
    if (Check_EnemyElec() == 0 && xglSoundEffectCheckID(0x10008, 0) != 0) {
        xglSoundEffectStopID(0x10008, 0);
    }
    if (Check_EnemyFound() == 0) {
        if (GC_LOOP.effect != 0) {
            sefDeleteEffectCf(GC_LOOP.effect);
        }
        GC_LOOP.effect = 0;
    }
    if (player->flash_timer != 0) {
        if (--player->flash_timer <= 0) {
            player->flash_timer = 0;
            player->flags &= ~0x800;
        } else {
            unsigned short phase = player->flash_timer;

            if (!(phase & 1) || (short)phase >= 19) {
                player->flags |= 0x800;
                player->render_flags |= 3;
                player->flash_frames = 0x48;
                player->filter_param = 0.5f;
            } else {
                player->flags &= ~0x800;
            }
        }
    }
    if (Ladder_Main(player) == 1) {
        player->flags &= ~0x20;
        return;
    }
    speed = 0.0f;
    stick_speed = 0;
    player->flags |= 0x20;
    player->external_velocity.y = D_004D7C10;
    player->previous_position.x = player->position.x;
    player->previous_position.y = player->position.y;
    player->previous_position.z = player->position.z;
    player->velocity.x = 0.0f;
    player->velocity.z = 0.0f;
    player->external_velocity.x = 0.0f;
    player->external_velocity.z = 0.0f;
    if (!(GC_LOOP.flags & 0x8000) && SCRIPT_getCfTime() >= 3) {
        signed char stick_x = PadData.stick_x;
        signed char stick_y = PadData.stick_y;
        float length;
        float angle;

        if (PadData.held & 0xa000) {
            stick_x = (PadData.held & 0x8000) ? -127 : 127;
        }
        if (PadData.held & 0x5000) {
            stick_y = (PadData.held & 0x1000) ? -127 : 127;
        }
        if (GC_LOOP.flags & 0x100000) {
            stick_x = player->stick_x;
            stick_y = player->stick_y;
        } else {
            player->stick_x = stick_x;
            player->stick_y = stick_y;
        }
        if (GC_LOOP.move_mode != 2) {
            float x = I2F(stick_x);
            float y = I2F(stick_y);

            length = __builtin_sqrtf(x * x + y * y);
            angle = floorf((atan2f(x, y) + 0.19634955f) / 0.3926991f) * 0.3926991f
                + *(float *)(CfCameraDefine + 0x34);
        } else {
            length = -I2F(stick_y);
            angle = player->rotation_y - I2F(stick_x) * D_004D7C1C;
        }
        if (128.0f <= length) {
            length = 128.0f;
        }
        if (WALK_THRESHOLD_F <= length) {
            player->aim_angle = angle;
            if (player->terrain_flags & 0x10) {
                if (RUN_THRESHOLD_F <= length) {
                    length = RUN_THRESHOLD_F - D_004D7C20;
                }
            }
            if (PadData.held & 2) {
                length = (RUN_THRESHOLD_F - D_004D7C24) / 1.5f;
            }
            stick_speed = F2I(length);
            speed = length * VECTOR_RATE;
            terrain_bits = (int)(player->terrain_flags & 0x3f000000);
            if (!(terrain_bits & 0x0f000000)) {
                speed *= rate_0[(u32)terrain_bits >> 28];
            }
        } else {
            stick_speed = 0;
            speed = 0.0f;
        }
        player->move_speed = speed;
        player->velocity.x += xglSin(player->aim_angle) * speed;
        player->velocity.y += 0.0f;
        player->velocity.z += xglCos(player->aim_angle) * speed;
        {
            terrain_bits = (int)(player->terrain_flags & 0x3f000000);
            if (terrain_bits & 0x0f000000) {
                float scale = VECTOR_RATE * 128.0f * rate_1[(u32)terrain_bits >> 28];
                float *direction = vec_3[idx_2[((u32)terrain_bits & 0x0f000000) >> 24]];

                player->velocity.x += scale * direction[0];
                player->velocity.z += scale * direction[1];
            }
            run_scale = rate_1[(u32)terrain_bits >> 28];
        }
    }
    if (!(GC_LOOP.flags & 0x4000)) {
        int abs_speed = stick_speed;

        if (abs_speed < 0) {
            abs_speed = -abs_speed;
        }

        if (abs_speed < WALK_THRESHOLD_I) {
            if (player->motion_id != 0x1b) {
                ACT_setMotion2(player, 0, 9);
            }
            player->motion_speed = D_004D7C28;
        } else if (abs_speed < RUN_THRESHOLD_I) {
            ACT_setMotion2(player, 1, 9);
            player->motion_speed = speed * 0.5f / run_scale;
        } else {
            ACT_setMotion2(player, 3, 9);
            player->motion_speed = speed * D_004D7C2C / run_scale;
            Enemy_FindByEar(player, &player->position);
        }
    }
    speed = __builtin_fabsf(speed);
    ground_effect = Get_EffectCode(Get_Attr(&player->position, 0, 0));
    if (D_004D7C30 < speed || EfTab_Loop[ground_effect] == 1) {
        if (ground_effect != -1) {
            if (EfTab_Intr[ground_effect] != -1) {
                player->effect_timer++;
            }
            if ((EfTab_Intr[ground_effect] != -1 && player->effect_timer >= EfTab_Intr[ground_effect])
                || player->effect_timer == -1) {
                GCPlayerEffect *effect = (GCPlayerEffect *)sefCreateEffectCf(EfTab_Code[ground_effect], 0, 0);

                player->effects[player->effect_slot] = effect;
                if (effect != 0) {
                    effect->owner = player;
                    player->effect_timer = 0;
                    player->effect_slot = (player->effect_slot + 1) % 8;
                }
            }
        } else {
            player->effect_timer = ground_effect;
        }
    }
    player->velocity.x += player->external_velocity.x;
    player->velocity.y += player->external_velocity.y;
    player->velocity.z += player->external_velocity.z;
    __builtin_memcpy(&position_before_move, &player->position, sizeof(position_before_move));
    before_move = &position_before_move.vector;
    player->position.x += player->velocity.x;
    player->position.z += player->velocity.z;
    HitCheckMapUnitWithNyuru(player);
    hit = HitCheckActor((LookAtPlayerActor *)player);
    if (hit != -1) {
        NyuruActor((LookAtPlayerActor *)player, &actor[hit]);
    }
    player->velocity.x = player->position.x - before_move->x;
    player->velocity.z = player->position.z - before_move->z;
    player->undulation.attr_mask = (player->undulation.attr_mask & 0xfff8) | 2;
    player->position.x = before_move->x;
    player->position.z = before_move->z;
    {
        float floor_height = UnduCheck(&player->position, &player->velocity, &player->undulation);

        if (floor_height != -1000.0f && player->position.y < floor_height) {
            player->position.y = floor_height;
            player->velocity.y = 0.0f;
        }
    }
    if (HitCheckMapUnit((LookAtPlayerActor *)player) != -1) {
        __builtin_memcpy(&player->position, &position_before_move, sizeof(position_before_move));
    }
    player->rotation_y = Get_Cursol_by_Reduce_Speed_Angle_Loop(player->rotation_y, player->aim_angle, 4.0f);
    xglMatrixStackUnit();
    xglMatrixStackTrans(&player->position.x);
    xglMatrixStackRotZ(player->rotation_z);
    xglMatrixStackRotY(player->rotation_y);
    xglMatrixStackRotX(player->rotation_x);
    if (GC_LOOP.map_flags & 1) {
        ShootMapUnit();
    }
    LookAt_Player((LookAtPlayerActor *)player);
    ACT_updateMotion(player);
    GameIdLightSet(player, 0);
    Set_Shadow(player);
    Get_EnemyIDPos(&player->position, EnemyTarget);
    if (!(GC_LOOP.flags & 0x8000)) {
        Check_Locater(player);
    }
}

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

extern u16 GameCfPlayerLoadResource_preCID;
extern u8 GameCfPlayerLoadResource_preCurry;
typedef struct LookAtSavedPartyPrefix {
    unsigned char unmodeled_00[0x100a4];
    u16 player_character_id;
} LookAtSavedPartyPrefix;
typedef struct LookAtGameResource {
    u32 address;
    int size;
    int handle;
    int state;
} LookAtGameResource;
extern LookAtGameResource GameResource[128];
void *GameResourceAlloc(int size);
void *GameResourceGetFreeAddr(void);
void RES_GetLeaderSeName(char *name);
char *RES_GetMotBaseName(int id);
void RES_GetMdlFileName(char *name, int id);
void xglSoundSendEffect(void *first, void *second, int id);
void xglSoundLoadEffect(const char *name, void *address, int id);
void ACT_setModelWrapper(LookAtPlayerActor *player_actor, int mode);
void ACT_setHumanHand(LookAtPlayerActor *player_actor, int mode);

void GameCfPlayerLoadResource(int use_cache)
{
    char path[256];
    char leader_se_name[64];
    char *name_end;
    unsigned short player_id;
    unsigned char curry;
    void *player;
    int header_0;
    int header_2;

    player_id = ((LookAtSavedPartyPrefix *)SaveData)->player_character_id;
    curry = 0;
    player = GameLoopState.player_actor;
    if (GameResource[1].address == 0) {
        GameResourceAlloc(0x12a000);
    } else if (GameResource[0].size != 0x12a000) {
        __asm__ volatile("break 0");
    }
    if (GameResource[2].address == 0) {
        GameResourceAlloc(0);
    }
    if (player_id == 1) {
        if (xglFlagsGet(0x19, 1) != 0) {
            if (xglFlagsGet(0x1a, 1) == 0) {
                player_id = 9;
            }
        }
        if (xglFlagsGet(0x73, 1) != 0) {
            curry = (xglFlagsGet(0x7b, 1) == 0);
        }
    }
    RES_GetLeaderSeName(leader_se_name);
    xglSoundSendEffect(0, 0, 2);
    xglSoundLoadEffect(leader_se_name, GameResourceGetFreeAddr(), 2);

    if (use_cache == 0) {
        header_0 = *(int *)0x01000000;
        if (header_0 == 0x78656c) {
            if (*(int *)0x01070800 == 0x585458) {
                header_2 = *(int *)0x010B1000;
                if (header_2 == 0x544e4a) {
                    if (*(int *)0x010B4000 == header_0) {
                        if (*(int *)0x010E6000 == header_2 &&
                            (*(int *)0x010E7000 & 0xffffff) == 0x4b5046 &&
                            GameCfPlayerLoadResource_preCID == player_id) {
                            if (GameCfPlayerLoadResource_preCurry == curry) {
                                return;
                            }
                        }
                    }
                }
            }
        }
    }

    GameCfPlayerLoadResource_preCID = player_id;
    GameCfPlayerLoadResource_preCurry = curry;
    RES_GetMdlFileName(path, player_id);
    name_end = path;
    if (path[0] != 0) {
        do {
            name_end++;
        } while (*name_end != 0);
    }
    __builtin_strcpy(name_end, ".lex");
    xglCdReadFile(path, (void *)0x01000000, 0, 1);
    __builtin_strcpy(name_end, ".xtx");
    xglCdReadFile(path, (void *)0x01070800, 0, 1);
    __builtin_strcpy(name_end, ".jnt");
    xglCdReadFile(path, (void *)0x010B1000, 0, 1);
    RES_GetMdlFileName(path, player_id | 0x10000);
    name_end = path;
    if (path[0] != 0) {
        do {
            name_end++;
        } while (*name_end != 0);
    }
    __builtin_strcpy(name_end, ".lex");
    xglCdReadFile(path, (void *)0x010B4000, 0, 1);
    __builtin_strcpy(name_end, ".jnt");
    xglCdReadFile(path, (void *)0x010E6000, 0, 1);
    if (curry == 0) {
        sprintf(path, "data\\motion\\%s.fpk", RES_GetMotBaseName(player_id));
    } else {
        __builtin_strcpy(path, "data\\motion\\shion_curry.fpk");
    }
    xglCdReadFile(path, (void *)0x010E7000, 0, 1);
    ACT_setMotion2(player, 0, 8);
    ACT_setModelWrapper(player, 0);
    ACT_setHumanHand(player, 0);
}

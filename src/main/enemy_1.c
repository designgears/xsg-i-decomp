#include "common.h"
#include "shared.h"

typedef struct EnemyParams {
    float route_step_length;
    float chase_distance;
    float value_c8;
    float chase_speed;
    float sight_angle;
    float sight_distance;
    short value_d8;
    short pause_interval;
    short value_dc;
    short threat_interval;
    short value_e0;
    short gravity_mode;
    float locator_distance;
    short wall_attribute;
    unsigned char move_kind;
    unsigned char unmodeled_2b;
    int preset_index;
    float *route_values;
    int route_value_count;
    Vector4 *homing_values;
    int homing_value_count;
    float body_radius;
    int ex_motion_resource;
    float value_108;
    int event_type;
    unsigned char unmodeled_50[0x18];
    float hearing_distance;
    short value_12c;
    short light_level;
} EnemyParams;
#include "enemy_1.h"

int Get_EffectCode(int attr)
{
    int effect[15] = {
        0x8100, 0x8200, 0x8300, 0x8400, 0x8500, 0x8600, 0x8700, 0x8800,
        0x8900, 0x8a00, 0x8b00, 0x8c00, 0x8d00, 0x8e00, 0x8f00,
    };
    short i;

    attr &= 0x8f00;
    for (i = 0; i < 15; i++) {
        if (attr == effect[i]) {
            return i;
        }
    }
    return -1;
}

typedef struct EnemyParamsEI {
    float route_step_length;
    float chase_distance;
    float value_c8;
    float chase_speed;
    float sight_angle;
    float sight_distance;
    short value_d8;
    short pause_interval;
    short value_dc;
    short threat_interval;
    short value_e0;
    short gravity_mode;
    float locator_distance;
    short wall_attribute;
    unsigned char move_kind;
    unsigned char unmodeled_2b;
    int preset_index;
    float *route_values;
    int route_value_count;
    Vector4 *homing_values;
    int homing_value_count;
    float body_radius;
    int ex_motion_resource;
    float value_108;
    int event_type;
    unsigned char unmodeled_50[0x18];
    float hearing_distance;
    short value_12c;
    short light_level;
} EnemyParamsEI;
typedef struct EnemyPresetEI {
    float route_step_length;
    float chase_distance;
    float value_c8;
    float chase_speed;
    float sight_angle;
    float sight_distance;
    float hearing_distance;
    float body_radius;
    float value_108;
    unsigned short value_d8;
    unsigned short pause_interval;
    unsigned short value_dc;
    unsigned short threat_interval;
    unsigned short value_2c;
    unsigned char move_kind;
    unsigned char unmodeled_2f;
    unsigned short chase_frame_limit;
    unsigned short initial_motion_id;
    float locator_distance;
    unsigned short locator_start_frame;
    unsigned short locator_end_frame;
    int state_flags;
    float value_3858;
    unsigned char unmodeled_44[0xc];
} EnemyPresetEI;
typedef struct EnemyUndulationEI {
    unsigned char unmodeled_00[8];
    short attr_mask;
} EnemyUndulationEI;
typedef union EnemyVector4EI {
    Vector4 vector;
    unsigned long long words[2];
} EnemyVector4EI;
typedef struct EnemyActorEI {
    u32 flags;
    unsigned char unmodeled_04[0x0c];
    union {
        Vector4 position;
        EnemyVector4EI position_pair;
    };
    unsigned char unmodeled_20[0x34];
    float angle;
    unsigned char unmodeled_58[8];
    Vector4 scale;
    unsigned char unmodeled_70[0x10];
    unsigned char number;
    unsigned char unmodeled_81;
    unsigned char kind;
    unsigned char unmodeled_83[3];
    short sound_effect_id;
    unsigned char unmodeled_88[0x38];
    EnemyParamsEI params;
    unsigned char unmodeled_130[0x398];
    EnemyUndulationEI undulation;
    unsigned char unmodeled_4d2[10];
    float ground_height;
    struct EnemyUndulationHeader *undulation_header;
    unsigned char unmodeled_4e4[0x210];
    float anim_time;
    float speed;
    float anim_start;
    float anim_final;
    unsigned short anim_number;
    unsigned char unmodeled_706[0xe];
    float anim_blend;
    unsigned char unmodeled_718[0x288];
    int light_flags;
    unsigned char unmodeled_9a4[0x1c];
    float light_scale;
    unsigned char unmodeled_9c4[0x14];
    long long light_blend_frames;
    unsigned char unmodeled_9e0[4];
    float target_angle;
    float body_radius;
    short lookat_timer;
    unsigned char unmodeled_9ee[2];
    short encount_lock;
    unsigned char unmodeled_9f2[0xe];
    EnemyVector4EI saved_position;
    struct EnemyEffectEI *effects[8];
    short effect_interval;
    short effect_next;
    unsigned char unmodeled_a34[0x3c];
} EnemyActorEI;
typedef struct EnemyEffectEI {
    unsigned char unmodeled_00[160];
    float scale_x;
    float scale_y;
    float scale_z;
    unsigned char unmodeled_ac[1724 - 172];
    EnemyActorEI *owner;
} EnemyEffectEI;
typedef union EnemyWorkState40EI {
    short alert_level;
    short spline_parameter;
} EnemyWorkState40EI;
typedef struct EnemyEscapeTargetEI {
    union {
        Vector4 position;
        EnemyVector4EI position_pair;
    };
    float scale;
    float scale_from;
    float scale_to;
    short scale_frame;
    short scale_frame_count;
} EnemyEscapeTargetEI;
typedef struct EnemyWorkEntryEI {
    float locator_distance;
    short initial_motion_id;
    short locator_start_frame;
    short locator_end_frame;
    short wall_attribute;
    float value_0c;
    float goback_x;
    unsigned char unmodeled_014[4];
    float goback_z;
    unsigned char unmodeled_01c[4];
    Vector4 step_limit;
    float drift_x;
    float fall_speed;
    float drift_z;
    unsigned char unmodeled_03c[4];
    EnemyWorkState40EI state_40;
    short fast_point_count;
    unsigned short hit_count;
    short hit_limit;
    signed char action;
    signed char move_result;
    unsigned char pause_kind;
    unsigned char unmodeled_04b[1];
    unsigned short value_4c;
    unsigned short stiff_timer;
    short effect_timer[2];
    signed char route_marks[2][8];
    short route_cursor[2];
    unsigned char unmodeled_068[2];
    unsigned char stiff_flag;
    unsigned char unmodeled_06b;
    short chase_frame_limit;
    short chase_frame;
    int map_index;
    short mark;
    short mark_timer;
    unsigned char unmodeled_078[8];
    Vector4 spline_points[256];
    short spline_point_count;
    unsigned char unmodeled_1082[0xe];
    Vector4 route_points[256];
    short action_motion_table[256][4];
    short motion_id_table[256][4];
    short unique_pattern;
    unsigned short unique_step;
    short route_point_count;
    unsigned char unmodeled_3096[0x70a];
    short homing_count;
    signed char freeze_state;
    signed char turn_lock;
    short target_number;
    unsigned char unmodeled_37a6[0x12];
    int event_type;
    unsigned char unmodeled_37bc[4];
    EnemyEscapeTargetEI escape_target;
    u32 state_flags;
    unsigned char unmodeled_37e4[0xc];
    EnemyVector4EI last_player_position;
    unsigned char unmodeled_3800[0x30];
    short encount_lock;
    unsigned char unmodeled_3832[0x22];
    float stiff_saved_speed;
    float value_3858;
    unsigned char unmodeled_385c[0x44];
    void *stiff_effect;
    unsigned char unmodeled_38a4[4];
    short applied_light_level;
    unsigned char unmodeled_38aa[2];
    short sense_block_timer;
    unsigned char unmodeled_38ae[2];
} EnemyWorkEntryEI;
typedef struct EnemyResourceRecordEI {
    void *motion;
    unsigned char unmodeled_04[0xc];
} EnemyResourceRecordEI;
typedef struct EnemyGameLoopStateEI {
    unsigned char unmodeled_00[4];
    EnemyActorEI *player;
    unsigned char unmodeled_08[0x29f8c];
    short encount_reset;
    unsigned char unmodeled_29f96[0x9a];
} EnemyGameLoopStateEI;
extern EnemyPresetEI *AdrsEnemy;
extern EnemyResourceRecordEI GameResource[128];
extern const char D_004CAB00[];
void Homing_Add(unsigned char number, float x, float y, float z, short value);
void ACT_initExMotion(EnemyActorEI *target, unsigned int slot, void *motion);
void CallMethod_I(const char *method, int value);
void Set_Spline_By_Route(EnemyActorEI *actor, int route);
void Set_Spline_By_Random();
int Get_Rnd(int low, int high);
void UnduParamInit(void *undulation);
float UnduCheck(const Vector4 *position, void *velocity,
                EnemyUndulationEI *undulation);

void Enemy_Init(EnemyActorEI *self)
{
    extern EnemyWorkEntryEI enepc_EI[16] __asm__("enepc");
    extern EnemyGameLoopStateEI GameLoopState_EI __asm__("GameLoopState");
    EnemyParamsEI *params = &self->params;
    EnemyWorkEntryEI *work = &enepc_EI[self->number];
    short i;
    short j;
    unsigned char kind;

    if (params->preset_index == -1) {
        work->initial_motion_id = 30;
        work->locator_start_frame = 5;
        work->locator_end_frame = 25;
        {
            float distance = params->locator_distance;

            work->pause_kind = params->move_kind;
            work->locator_distance = distance;
        }
        self->body_radius = params->body_radius;
        work->value_0c = params->value_108;
    } else {
        params->value_d8 = AdrsEnemy[params->preset_index].value_d8;
        params->route_step_length = AdrsEnemy[params->preset_index].route_step_length;
        params->pause_interval = AdrsEnemy[params->preset_index].pause_interval;
        params->chase_distance = AdrsEnemy[params->preset_index].chase_distance;
        params->value_dc = AdrsEnemy[params->preset_index].value_dc;
        params->value_c8 = AdrsEnemy[params->preset_index].value_c8;
        params->threat_interval = AdrsEnemy[params->preset_index].threat_interval;
        params->chase_speed = AdrsEnemy[params->preset_index].chase_speed;
        work->value_4c = AdrsEnemy[params->preset_index].value_2c;
        params->sight_angle = AdrsEnemy[params->preset_index].sight_angle;
        work->initial_motion_id = AdrsEnemy[params->preset_index].initial_motion_id;
        params->sight_distance = AdrsEnemy[params->preset_index].sight_distance;
        work->locator_start_frame = AdrsEnemy[params->preset_index].locator_start_frame;
        params->hearing_distance = AdrsEnemy[params->preset_index].hearing_distance;
        work->locator_end_frame = AdrsEnemy[params->preset_index].locator_end_frame;
        work->locator_distance = AdrsEnemy[params->preset_index].locator_distance;
        work->pause_kind = AdrsEnemy[params->preset_index].move_kind;
        self->body_radius = AdrsEnemy[params->preset_index].body_radius;
        work->value_0c = AdrsEnemy[params->preset_index].value_108;
        work->state_flags = AdrsEnemy[params->preset_index].state_flags;
        work->chase_frame_limit = AdrsEnemy[params->preset_index].chase_frame_limit;
        work->value_3858 = AdrsEnemy[params->preset_index].value_3858;
    }
    work->wall_attribute = params->wall_attribute;
    work->event_type = params->event_type;
    self->target_angle = self->angle;
    if (work->route_point_count == 0) {
        work->route_point_count = params->route_value_count / 3;
        for (i = 0; i < work->route_point_count; i++) {
            Vector4 *point = &work->route_points[i];
            float *dest = &point->z;

            dest[-2] = params->route_values[i * 3];
            dest[-1] = params->route_values[i * 3 + 1];
            dest[0] = params->route_values[i * 3 + 2];
        }
    }
    if (work->pause_kind == 9 || work->pause_kind == 4) {
        for (i = 0; i < work->route_point_count - 2; i++) {
            work->route_points[work->route_point_count + i].x =
                work->route_points[work->route_point_count - i - 2].x;
            *(&work->route_points[work->route_point_count + i].z) =
                *(&work->route_points[work->route_point_count - i - 2].z);
        }
        work->route_point_count = work->route_point_count * 2 - 2;
        if (work->pause_kind == 9)
            work->pause_kind = 8;
        if (work->pause_kind == 4)
            work->pause_kind = 3;
    }
    for (j = 0; j < work->route_point_count; j++) {
        for (i = 0; i < 4; i++) {
            work->action_motion_table[j][i] = -1;
            work->motion_id_table[j][i] = 0;
        }
    }
    if (work->homing_count == 0) {
        if (params->homing_value_count != 0) {
            work->homing_count = 0;
            for (i = 0; i < params->homing_value_count / 4; i++) {
                Homing_Add(self->number, params->homing_values[i].x,
                           params->homing_values[i].y,
                           params->homing_values[i].z,
                           (short)params->homing_values[i].w);
            }
        }
    }
    kind = work->pause_kind;
    if (kind != 8 && kind != 3 && kind != 12 && kind != 13)
        Set_Spline_By_Random(self);
    else
        Set_Spline_By_Route(self, 0);
    for (i = 0; i < 8; i++) {
        work->route_marks[1][i] = 0;
        work->route_marks[0][i] = 0;
    }
    work->route_cursor[0] = 0;
    work->route_cursor[1] = 0;
    if (params->pause_interval != 0)
        work->route_marks[0][Get_Rnd(0, params->pause_interval) % 8] = 1;
    if (params->threat_interval != 0)
        work->route_marks[1][Get_Rnd(0, params->threat_interval) % 8] = 1;
    if (self->sound_effect_id >= 0x4000 && self->sound_effect_id <= 0x4f22)
        work->map_index = 772;
    else
        work->map_index = 768;
    if (params->ex_motion_resource != -1) {
        ACT_initExMotion(self, 0,
                         GameResource[params->ex_motion_resource].motion);
    }
    Enemy_ActionReady((EnemyActor *)self, 16);
    work->hit_limit = 0;
    if (GameLoopState_EI.player == self)
        CallMethod_I(D_004CAB00, 100);
    else
        CallMethod_I(D_004CAB00, work->event_type);
    UnduParamInit(&self->undulation);
    self->undulation.attr_mask = 2;
    UnduCheck(&self->position, 0, &self->undulation);
    self->undulation.attr_mask |= 0x1000;
}

void Enemy_Damage_Explosion(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    unsigned short count;
    short limit;
    int stiff;

    work->goback_x = 0.0f;
    work->goback_z = 0.0f;
    TM_Enemy_Move_Step(actor->number, &work->step_limit, work->goback_x);
    actor->target_angle = Get_Angle(&actor->position,
                                    &GameLoopState[1]->position);

    count = work->hit_count;
    limit = work->hit_limit;
    work->hit_count = count + 1;
    if ((short)count >= limit) {
        actor->flags &= ~ACTOR_FLAG_AILMENT_ACTIVE;
        stiff = work->stiff_flag;
        if (stiff & ENEMY_STIFF_ACTIVE_BIT)
            Enemy_ActionReady(actor, 7);
        else
            Enemy_ActionReady(actor, 6);
    }
}

void Enemy_Damage_Electric(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    int count = work->hit_count + 1;
    short limit = work->hit_limit;

    work->hit_count = count;
    if ((short)count >= limit) {
        u32 flags = actor->flags;

        work->state_40.alert_level = 0;
        actor->flags = flags & ~ACTOR_FLAG_AILMENT_ACTIVE;
        Homing_Search(actor);
        Enemy_ActionReady(actor, 2);
    }
}

void Enemy_Damage_Seal(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    int count = work->hit_count + 1;
    short limit = work->hit_limit;

    work->hit_count = count;
    if ((short)count >= limit) {
        u32 flags = actor->flags;

        work->state_40.alert_level = 0;
        actor->flags = flags & ~ACTOR_FLAG_AILMENT_ACTIVE;
        Homing_Search(actor);
        Enemy_ActionReady(actor, 1);
    }
}

void Enemy_Stiff(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    int timer = work->stiff_timer - 1;

    work->stiff_timer = timer;
    if ((short)timer <= 0) {
        work->stiff_flag &= ~ENEMY_STIFF_ACTIVE_BIT;
        work->stiff_timer = 0;
        actor->speed = work->stiff_saved_speed;
        actor->flags &= ~ACTOR_FLAG_STIFF_ACTIVE;
        sefDeleteEffectCf(work->stiff_effect);
        work->stiff_effect = 0;
        Homing_Search(actor);
        Enemy_ActionReady(actor, 2);
    } else {
        actor->speed = 0.0f;
    }
}

void Enemy_Found(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    unsigned short count = work->hit_count;

    work->hit_count = count + 1;
    if ((short)count >= work->hit_limit)
        Enemy_ActionReady(actor, 6);

    if (actor->sound_effect_id != 0x2201) {
        EnemyActor *player = (EnemyActor *)GameLoopState[1];
        actor->target_angle = Get_Angle(&actor->position, &player->position);
    }
}

extern float cosf(float angle);
extern signed char FLAG_FRAME_60;
extern unsigned char MapUnit[];
void Debug_Mark(Vector4 *pos, void *model);
int CrossCheckMapUnit(Vector4 *a, Vector4 *b);
void SendBrokenSignal(void *unit);
void Check_Encount(EnemyActor *actor, int enable, signed char command);

void Enemy_Tentacle(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    Vector4 point;
    Vector4 mark;
    float dist;
    float length;
    float base;
    float step;
    float a;
    short i;
    int unit;
    char *g;
    short count;

    if (*(short *)((char *)work + 6) <= (short)work->hit_count &&
        (short)work->hit_count <= *(short *)((char *)work + 8)) {
        dist = Get_Distance(&actor->position, &GameLoopState[1]->position);
        length = actor->body_radius + ((*(float *)work - actor->body_radius) *
                         (float)((short)work->hit_count - *(short *)((char *)work + 6))) /
                        (float)(*(short *)((char *)work + 8) - *(short *)((char *)work + 6));
        actor->target_angle = Get_Angle(&actor->position, &GameLoopState[1]->position);
        if (FLAG_FRAME_60 != 0) {
            for (i = 0; i < 64; i++) {
                a = (float)i * 6.2831855f / 64.0f;
                mark.x = actor->position.x + sinf(a) * length;
                mark.y = actor->position.y + 1.0f;
                mark.z = actor->position.z + cosf(a) * length;
                mark.w = 1.0f;
                Debug_Mark(&mark, AdrsEnemySphere);
            }
        }
        Get_Point_By_AngleLength(&actor->position, &point, actor->angle, length);
        unit = CrossCheckMapUnit(&actor->position, &point);
        if (unit != -1) {
            SendBrokenSignal(MapUnit + unit * 0x300);
        }
        if (*(short *)((char *)GameLoopState[1] + 0x9F0) == 0 && dist < length) {
            Check_Encount(actor, 0, 0);
        } else {
            g = (char *)GameLoopState;
            *(short *)(g + 0x29F94) = -1;
        }
    }
    count = work->hit_count;
    work->hit_count++;
    if (count >= work->hit_limit) {
        Enemy_ActionReady(actor, 1);
    }
}

#define D_004D8008 0.1f
#define D_004D800C 0.01f
#define D_004D8010 0.01f
#define D_004D8014 0.01f
#define D_004D8018 0.01f
extern const char D_004DB7F8[];
extern const char D_004DB800[];
extern const char D_004DB808[];
extern const char D_004DB810[];
extern const char D_004DB818[];
typedef union EnemyVector4 {
    Vector4 vector;
    unsigned long long words[2];
} EnemyVector4;
typedef struct EcTarget {
    union {
        Vector4 position;
        EnemyVector4 position_pair;
    };
    unsigned char unmodeled_10[0x10];
} EcTarget;
typedef struct EcActor {
    unsigned char unmodeled_00[0x10];
    union {
        Vector4 position;
        EnemyVector4 position_pair;
    };
    unsigned char unmodeled_20[0x60];
    unsigned char number;
    unsigned char unmodeled_81[0x3f];
    EnemyParams params;
    unsigned char unmodeled_130[0x9e4 - 0x130];
    float target_angle;
    float body_radius;
    unsigned char unmodeled_9ec[0x84];
} EcActor;
typedef struct EcWork {
    unsigned char unmodeled_00[0x40];
    EnemyWorkState40 state_40;
    unsigned char unmodeled_42[6];
    signed char action;
    signed char move_result;
    unsigned char unmodeled_4a[0x20];
    unsigned char stiff_flag;
    unsigned char unmodeled_6b;
    short chase_frame_limit;
    short chase_frame;
    int map_index;
    unsigned char unmodeled_74[0x37a4 - 0x74];
    short target_number;
    unsigned char unmodeled_37a6[0x12];
    int event_type;
    unsigned char unmodeled_37bc[4];
    EcTarget escape_target;
    u32 state_flags;
    unsigned char unmodeled_37e4[0xc];
    EnemyVector4 last_player_position;
} EcWork;
int CrossPointUwamono(float radius, const Vector4 *from, const Vector4 *to,
                      Vector4 *point);
int Check_MoveSquare(float radius, const Vector4 *to, const Vector4 *from,
                     Vector4 *square, int map_index);
void xglFontDebugPrintf(int x, int y, const char *format, ...);

void Enemy_Chase(EcActor *self)
{
    EcWork *work = (EcWork *)&enepc[self->number];
    EnemyParams *params = &self->params;
    EnemyVector4 start;
    Vector4 hit;
    Vector4 cross_point;
    int color[4];
    EnemyVector4 square;
    float distance;
    float speed;
    int crossed;

    distance = Get_Distance(&self->position, &work->escape_target.position);
    if (D_004D8008 <= distance && !(work->stiff_flag & 2)) {
        speed = params->chase_speed;
        Set_Motion(self, Get_DefaultMotion(self, 2), 9);
    } else {
        speed = params->chase_speed / 1.5f;
        Set_Motion(self, Get_DefaultMotion(self, 1), 9);
    }

    start = self->position_pair;
    Check_Undu(&self->position, &work->escape_target.position, work->map_index,
               &hit, 1, 0, 0);
    crossed = CrossPointUwamono(self->body_radius, &self->position,
                                &work->escape_target.position, &cross_point);
    if (__builtin_fabsf(hit.x - work->escape_target.position.x) < D_004D800C &&
        __builtin_fabsf(hit.z - work->escape_target.position.z) < D_004D800C &&
        crossed == 0) {
        work->last_player_position = work->escape_target.position_pair;
        xglFontDebugPrintf(8, 100, D_004DB7F8);
    } else if (crossed == 1) {
        work->last_player_position.vector = cross_point;
        xglFontDebugPrintf(8, 112, D_004DB800);
    } else {
        if (Check_MoveSquare(self->body_radius, &work->escape_target.position,
                             &self->position, &square.vector,
                             work->map_index) == 1) {
            work->last_player_position = square;
            xglFontDebugPrintf(8, 112, D_004DB808);
        }
    }

    color[0] = 255;
    color[1] = 255;
    color[2] = 255;
    color[3] = 128;
    TM_Enemy_Move_Step(self->number, &work->last_player_position.vector, speed);
    if ((work->state_flags & 0x20) != 0) {
        self->position.y += (work->escape_target.position.y - self->position.y) *
                            D_004D8010;
    }

    if (work->move_result == 0x60) {
        if (Check_MoveSquare(self->body_radius, &cross_point, &self->position,
                             &square.vector, work->map_index) == 1) {
            work->last_player_position = square;
            xglFontDebugPrintf(8, 124, D_004DB800);
        } else {
            work->last_player_position.vector = cross_point;
            xglFontDebugPrintf(8, 124, D_004DB810);
        }
    }

    if (distance >= params->chase_distance || work->move_result == 0x58 ||
        ((short)work->chase_frame++ >=
             (work->chase_frame_limit < 0 ? -work->chase_frame_limit : work->chase_frame_limit) &&
         work->chase_frame_limit != 0) ||
        (Get_Distance(&self->position, &work->last_player_position.vector) <
             D_004D8014 &&
         work->move_result != 0x60 && work->move_result != 99)) {
        work->state_40.alert_level = 0;
        Homing_Search(self);
        Enemy_ActionReady(self, 2);
    } else if (Get_Distance(&self->position,
                            &work->last_player_position.vector) < D_004D8018) {
        if (Check_MoveSquare(self->body_radius, &work->escape_target.position,
                             &self->position, &square.vector,
                             work->map_index) == 1) {
            work->last_player_position = square;
            xglFontDebugPrintf(8, 112, D_004DB818);
            TM_Enemy_Move_Step(self->number, &work->last_player_position.vector,
                               speed);
        }
        self->target_angle = Get_Angle(&self->position,
                                       &actor[work->target_number].position);
    }
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct EnemyPreset {
    float route_step_length;
    float chase_distance;
    float value_c8;
    float chase_speed;
    float sight_angle;
    float sight_distance;
    float hearing_distance;
    float body_radius;
    float value_108;
    unsigned short value_d8;
    unsigned short pause_interval;
    unsigned short value_dc;
    unsigned short threat_interval;
    unsigned short value_2c;
    unsigned char move_kind;
    unsigned char unmodeled_2f;
    unsigned short chase_frame_limit;
    unsigned short initial_motion_id;
    float locator_distance;
    unsigned short locator_start_frame;
    unsigned short locator_end_frame;
    int state_flags;
    float value_3858;
    unsigned char unmodeled_44[0xc];
} EnemyPreset;

typedef struct EnemyUndulation {
    unsigned char unmodeled_00[8];
    short attr_mask;
} EnemyUndulation;

typedef struct EnemyEffect {
    unsigned char unmodeled_00[160];
    float scale_x;
    float scale_y;
    float scale_z;
    unsigned char unmodeled_ac[1724 - 172];
    EnemyActor *owner;
} EnemyEffect;

typedef struct EnemyMapUnit {
    unsigned char unmodeled_000[0x300];
} EnemyMapUnit;

typedef struct EnemyGameLoopState {
    unsigned char unmodeled_00[4];
    EnemyActor *player;
    unsigned char unmodeled_08[0x29f8c];
    short encount_reset;
    unsigned char unmodeled_29f96[0x9a];
} EnemyGameLoopState;

typedef struct EnemyResourceRecord {
    void *motion;
    unsigned char unmodeled_04[0xc];
} EnemyResourceRecord;

typedef struct EnemyPadView {
    unsigned char unmodeled_00[0x28];
    unsigned short held;
    unsigned short pressed;
} EnemyPadView;

typedef struct EnemyParamsAU {
    float route_step_length;
    float chase_distance;
    float value_c8;
    float chase_speed;
    float sight_angle;
    float sight_distance;
    short value_d8;
    short pause_interval;
    short value_dc;
    short threat_interval;
    short value_e0;
    short gravity_mode;
    float locator_distance;
    short wall_attribute;
    unsigned char move_kind;
    unsigned char unmodeled_2b;
    int preset_index;
    float *route_values;
    int route_value_count;
    Vector4 *homing_values;
    int homing_value_count;
    float body_radius;
    int ex_motion_resource;
    float value_108;
    int event_type;
    unsigned char unmodeled_50[0x18];
    float hearing_distance;
    short value_12c;
    short light_level;
} EnemyParamsAU;

typedef struct EnemyUndulationAU {
    unsigned char unmodeled_00[8];
    short attr_mask;
} EnemyUndulationAU;

typedef union EnemyVector4AU {
    Vector4 vector;
    unsigned long long words[2];
} EnemyVector4AU;

typedef struct EnemyActorAU {
    u32 flags;
    unsigned char unmodeled_04[0x0c];
    union {
        Vector4 position;
        EnemyVector4AU position_pair;
    };
    unsigned char unmodeled_20[0x34];
    float angle;
    unsigned char unmodeled_58[8];
    Vector4 scale;
    unsigned char unmodeled_70[0x10];
    unsigned char number;
    unsigned char unmodeled_81;
    unsigned char kind;
    unsigned char unmodeled_83[3];
    short sound_effect_id;
    unsigned char unmodeled_88[0x38];
    EnemyParamsAU params;
    unsigned char unmodeled_130[0x398];
    EnemyUndulationAU undulation;
    unsigned char unmodeled_4d2[10];
    float ground_height;
    struct EnemyUndulationHeader *undulation_header;
    unsigned char unmodeled_4e4[0x210];
    float anim_time;
    float speed;
    float anim_start;
    float anim_final;
    unsigned short anim_number;
    unsigned char unmodeled_706[0xe];
    float anim_blend;
    unsigned char unmodeled_718[0x288];
    int light_flags;
    unsigned char unmodeled_9a4[0x1c];
    float light_scale;
    unsigned char unmodeled_9c4[0x14];
    long long light_blend_frames;
    unsigned char unmodeled_9e0[4];
    float target_angle;
    float body_radius;
    short lookat_timer;
    unsigned char unmodeled_9ee[2];
    short encount_lock;
    unsigned char unmodeled_9f2[0xe];
    EnemyVector4AU saved_position;
    struct EnemyEffectAU *effects[8];
    short effect_interval;
    short effect_next;
    unsigned char unmodeled_a34[0x3c];
} EnemyActorAU;

typedef union EnemyWorkState40AU {
    short alert_level;
    short spline_parameter;
} EnemyWorkState40AU;

typedef struct EnemyEscapeTargetAU {
    union {
        Vector4 position;
        EnemyVector4AU position_pair;
    };
    float scale;
    float scale_from;
    float scale_to;
    short scale_frame;
    short scale_frame_count;
} EnemyEscapeTargetAU;

typedef struct EnemyWorkEntryAU {
    float locator_distance;
    short initial_motion_id;
    short locator_start_frame;
    short locator_end_frame;
    short wall_attribute;
    float value_0c;
    float goback_x;
    unsigned char unmodeled_014[4];
    float goback_z;
    unsigned char unmodeled_01c[4];
    Vector4 step_limit;
    float drift_x;
    float fall_speed;
    float drift_z;
    unsigned char unmodeled_03c[4];
    EnemyWorkState40AU state_40;
    short fast_point_count;
    unsigned short hit_count;
    short hit_limit;
    signed char action;
    signed char move_result;
    unsigned char pause_kind;
    unsigned char unmodeled_04b[1];
    unsigned short value_4c;
    unsigned short stiff_timer;
    short effect_timer[2];
    signed char route_marks[2][8];
    short route_cursor[2];
    unsigned char unmodeled_068[2];
    unsigned char stiff_flag;
    unsigned char unmodeled_06b;
    short chase_frame_limit;
    short chase_frame;
    int map_index;
    short mark;
    short mark_timer;
    unsigned char unmodeled_078[8];
    Vector4 spline_points[256];
    short spline_point_count;
    unsigned char unmodeled_1082[0xe];
    Vector4 route_points[256];
    short action_motion_table[256][4];
    short motion_id_table[256][4];
    short unique_pattern;
    unsigned short unique_step;
    short route_point_count;
    unsigned char unmodeled_3096[0x70a];
    short homing_count;
    signed char freeze_state;
    signed char turn_lock;
    short target_number;
    unsigned char unmodeled_37a6[0x12];
    int event_type;
    unsigned char unmodeled_37bc[4];
    EnemyEscapeTargetAU escape_target;
    u32 state_flags;
    unsigned char unmodeled_37e4[0xc];
    EnemyVector4AU last_player_position;
    unsigned char unmodeled_3800[0x30];
    short encount_lock;
    unsigned char unmodeled_3832[0x22];
    float stiff_saved_speed;
    float value_3858;
    unsigned char unmodeled_385c[0x44];
    void *stiff_effect;
    unsigned char unmodeled_38a4[4];
    short applied_light_level;
    unsigned char unmodeled_38aa[2];
    short sense_block_timer;
    unsigned char unmodeled_38ae[2];
} EnemyWorkEntryAU;

void BSpline_Init(Vector4 *points, const Vector4 *origin, float angle);

int TM_Check_Move(EnemyActor *arg0);

#define ENEMY_AIRBORNE_FLAG 0x20
#define D_004D801C 0.01f
#define D_004D8020 3.1415927f
#define D_004D8024 6.2831855f
#define D_004D8028 0.12f
#define D_004D802C 3.1415927f
#define D_004D8030 6.2831855f
void GetBSplineLoopDummy(short parameter, short point_count, int stride,
                         Vector4 *points, Vector4 *result, EnemyActor *actor);
void Check_Wall(float scale, float radius, const Vector4 *origin,
                signed char directions[16], int status_count, int map_index,
                unsigned char number, int territory);
short Check_Straight(float radius, const Vector4 *from, const Vector4 *to,
                    int mask, Vector4 *hit, int map_index, int number,
                    int territory);
void BSpline_Add(short parameter, short point_count, short stride,
                 Vector4 *points, const Vector4 *point, short offset);

void Enemy_Route(EnemyActor *self)
{
    EnemyWorkEntry *work = &enepc[self->number];
    EnemyParams *params = &self->params;
    Vector4 point;
    Vector4 origin;
    Vector4 target;
    Vector4 hit;
    signed char directions[16];
    int saved_parameter = (*(short *)&work->state_40);
    short length = 0;
    short i;
    short available;
    short index;
    float distance;
    float angle;
    short motion;
    signed char moved;

    if (work->spline_point_count <= 0)
        return;

    work->unique_pattern = saved_parameter / 128;
    if (work->spline_point_count * 128 - 384 <= saved_parameter &&
        (self->flags & 0x80000000) != 0) {
        if (work->pause_kind != 8) {
            Set_Spline_By_Random();
        } else {
            Set_Spline_By_Route(self, 0);
        }
        self->flags &= 0x7fffffff;
    }

    if ((work->state_40.spline_parameter % 128) == 0 &&
        (self->flags & 0x80000000) == 0) {
        for (i = 0; i < 2; i++) {
            if (work->action_motion_table[(*(short *)&work->state_40) / 128][0] != -1) {
                work->unique_step = 0;
                Enemy_ActionReady(self, 9);
                return;
            }
            if ((i != 0 || params->pause_interval != 0) &&
                (i != 1 || params->threat_interval != 0)) {
                switch (i) {
                case 0:
                    length = params->pause_interval;
                    break;
                case 1:
                    length = params->threat_interval;
                    break;
                }
                work->route_cursor[i] = (work->route_cursor[i] + 1) % length;
                if (work->route_marks[i][work->route_cursor[i]] == 1) {
                    switch (i) {
                    case 0:
                        Enemy_ActionReady(self, 1);
                        break;
                    case 1:
                        Enemy_ActionReady(self, 2);
                        break;
                    }
                    work->route_marks[0][work->route_cursor[i]] = work->route_marks[1][work->route_cursor[i]] = 0;
                    return;
                }
                if (work->route_cursor[i] == 0) {
                    short mark;

                    for (mark = 0; mark < length; mark++)
                        work->route_marks[i][mark] = 0;
                    work->route_marks[i][Get_Rnd(0, length - 1)] = 1;
                }
                        }
        }
    }

    if (work->state_flags & ENEMY_ALERT_RAISE_FLAG) {
        GetBSplineLoop((*(short *)&work->state_40), work->spline_point_count, 128,
                       work->spline_points, &point);
        if (work->fast_point_count == 0)
            work->state_40.spline_parameter +=
                work->state_40.spline_parameter % 2 + 2;
        else
            work->state_40.spline_parameter +=
                work->state_40.spline_parameter % 8 + 16;
    } else {
        GetBSplineLoopDummy((*(short *)&work->state_40), work->spline_point_count, 128,
                            work->spline_points, &point, self);
    }

    distance = Get_Distance(&self->position, &point);
    if (work->spline_point_count << 7 <= work->state_40.spline_parameter)
        work->state_40.spline_parameter = 0;
    if (D_004D801C <= distance)
        self->target_angle = Get_Angle(&self->position, &point);

    {
        int airborne = work->state_flags & ENEMY_AIRBORNE_FLAG;

        work->goback_x = point.x - self->position.x;
        work->goback_z = point.z - self->position.z;
        self->position.x = point.x;
        self->position.z = point.z;
        if (airborne)
            self->position.y = point.y;
    }

    moved = TM_Check_Move(self);
    if (!moved) {
        if ((int)self->flags < 0) {
            Homing_Search(self);
        } else if (work->pause_kind == 8 || work->pause_kind == 3) {
            work->state_40.spline_parameter = saved_parameter;
        } else {
            available = 0;
            Check_Wall(1.0f, self->body_radius, &origin, directions, -16,
                       work->map_index, self->number, work->wall_attribute);
            for (i = 0; i < 16; i++) {
                if (directions[i] == 0)
                    directions[available++] = i;
            }
            if (available == 0)
                angle = xglSRand() % 360 / 180.0f * D_004D8020;
            else
                angle = directions[xglSRand() % available] * D_004D8024 *
                        0.0625f;
            BSpline_Init(work->spline_points, &self->position, angle);
            work->state_40.spline_parameter = 0;
        }
        Set_Motion(self, Get_DefaultMotion(self, 0), 9);
    } else {
        if (D_004D8028 <= distance)
            motion = Get_DefaultMotion(self, 2);
        else
            motion = Get_DefaultMotion(self, 1);
        Set_Motion(self, motion, 9);
    }

    if ((work->pause_kind == 1 || work->pause_kind == 2) &&
        (int)self->flags >= 0 && (work->state_40.spline_parameter % 128) == 0) {
        if (work->fast_point_count > 0)
            work->fast_point_count--;
        index = (*(&work->state_40.spline_parameter) / 128 + 2) %
                work->spline_point_count;
        origin.x = work->spline_coordinates[index][0];
        origin.y = work->spline_coordinates[index][1];
        origin.z = work->spline_coordinates[index][2];
        available = 0;
        Check_Wall(1.0f, self->body_radius, &origin, directions, -16,
                   work->map_index, self->number, work->wall_attribute);
        for (i = 0; i < 16; i++) {
            if (directions[i] == 0)
                directions[available++] = i;
        }
        if (available == 0)
            angle = xglSRand() % 360 / 180.0f * D_004D802C;
        else
            angle = directions[xglSRand() % available] * D_004D8030 * 0.0625f;
        target.x = origin.x + sinf(angle) * params->route_step_length;
        target.y = self->position.y;
        target.z = origin.z + cosf(angle) * params->route_step_length;
        target.w = 1.0f;
        Check_Straight(self->body_radius, &origin, &target, 16, &hit,
                       work->map_index, self->number, work->wall_attribute);
        if (work->state_flags & ENEMY_AIRBORNE_FLAG)
            hit.y = Get_Height(&hit, work->map_index, 0) +
                    Get_Rnd(30, 50) / 10.0f;
        BSpline_Add(work->state_40.spline_parameter, work->spline_point_count,
                    128, work->spline_points, &hit, 3);
    }
}

void Enemy_Escape(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    Vector4 point;
    float distance[3];
    short i;
    int length;
    int cooldown;
    short motion;

    if (work->spline_point_count <= 0)
        return;

    for (i = 0; i < 3; i++) {
        length = work->spline_point_count * 32;
        GetBSplineLoop((work->state_40.spline_parameter + length + i - 1) % length,
                       work->spline_point_count, 32, work->spline_points,
                       &point);
        distance[i] = Get_Distance(&work->escape_target.position, &point);
    }

    if (distance[0] < distance[1])
        i = distance[1] < distance[2] ? 2 : 1;
    else
        i = distance[0] < distance[2] ? 2 : 0;

    work->state_40.spline_parameter =
        (work->state_40.spline_parameter + work->spline_point_count * 32 + i - 1) %
        (work->spline_point_count * 32);
    GetBSplineLoop(work->state_40.spline_parameter, work->spline_point_count, 32,
                   work->spline_points, &point);

    if (i == 1) {
        actor->target_angle =
            Get_Angle(&actor->position, &work->escape_target.position);
        cooldown = CoolDown_0;
        CoolDown_0 = cooldown - 1;
    } else {
        CoolDown_0 = 16;
        actor->target_angle = Get_Angle(&actor->position, &point);
    }

    if (D_004D8034 <= Get_Distance(&actor->position, &point) ||
        (short)CoolDown_0 > 0) {
        motion = Get_DefaultMotion(actor, 2);
    } else {
        motion = Get_DefaultMotion(actor, 1);
    }
    Set_Motion(actor, motion, 9);

    actor->speed = D_004D8038;
    work->goback_x = point.x - actor->position.x;
    work->goback_z = point.z - actor->position.z;
    actor->position.x = point.x;
    actor->position.z = point.z;
}

void Enemy_Route_Chase(EnemyActor *self)
{
    EnemyWorkEntry *work = &enepc[self->number];
    Vector4 point;
    float distance[3];
    short i;
    int length;
    int cooldown;
    short motion;

    if (work->spline_point_count <= 0)
        return;

    for (i = 0; i < 3; i++) {
        length = work->spline_point_count * 32;
        GetBSplineLoop((work->state_40.spline_parameter + length + i - 1) % length,
                       work->spline_point_count, 32, work->spline_points,
                       &point);
        distance[i] = Get_Distance(&work->escape_target.position, &point);
    }

    if (distance[0] < distance[1])
        i = distance[0] < distance[2] ? 0 : 2;
    else
        i = distance[1] < distance[2] ? 1 : 2;

    work->state_40.spline_parameter =
        (work->state_40.spline_parameter + work->spline_point_count * 32 + i - 1) %
        (work->spline_point_count * 32);
    GetBSplineLoop(work->state_40.spline_parameter, work->spline_point_count, 32,
                   work->spline_points, &point);

    if (i == 1) {
        self->target_angle =
            Get_Angle(&self->position, &work->escape_target.position);
        cooldown = CoolDown_1;
        CoolDown_1 = cooldown - 1;
    } else {
        CoolDown_1 = 16;
        self->target_angle = Get_Angle(&self->position, &point);
    }

    if (D_004D803C <= Get_Distance(&self->position, &point) ||
        (short)CoolDown_1 > 0) {
        motion = Get_DefaultMotion(self, 2);
    } else {
        motion = Get_DefaultMotion(self, 1);
    }
    Set_Motion(self, motion, 9);

    self->speed = D_004D8040;
    work->goback_x = point.x - self->position.x;
    work->goback_z = point.z - self->position.z;
    self->position.x = point.x;
    self->position.z = point.z;
}

void Enemy_Pause(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    unsigned short count;
    int limit;

    if (work->pause_kind == 5 || work->pause_kind == 6) {
        if (actor->flags & ACTOR_FLAG_FACE_PLAYER) {
            actor->target_angle =
                Get_Angle(&actor->position, &GameLoopState[1]->position);
        }
    }

    count = work->hit_count;
    limit = work->hit_limit - 1;
    work->hit_count = count + 1;
    if ((short)count >= limit) {
        switch (work->pause_kind) {
        case 5:
        case 6:
            Enemy_ActionReady(actor, 1);
            break;
        case 1:
        case 2:
        case 3:
        case 8:
        case 9:
            Enemy_ActionReady(actor, 4);
            if (work->state_flags & ENEMY_ALERT_RAISE_FLAG)
                work->state_40.spline_parameter += 2;
            break;
        case 12:
            Enemy_ActionReady(actor, 14);
            break;
        case 13:
            Enemy_ActionReady(actor, 15);
            break;
        default: {
            unsigned short direction = xglSRand() & 1;

            switch (direction) {
            case 0:
                Enemy_ActionReady(actor, 4);
                break;
            case 1:
                Enemy_ActionReady(actor, 2);
                break;
            }
            break;
        }
        }
    }
}

void Enemy_Threat(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    unsigned short count = work->hit_count;
    int limit = work->hit_limit - 1;

    work->hit_count = count + 1;
    if ((short)count >= limit) {
        Enemy_ActionReady(actor, 4);
        if (work->state_flags & ENEMY_ALERT_RAISE_FLAG)
            work->state_40.alert_level += 2;
    }
}

void Enemy_Unique(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];
    unsigned short count = work->hit_count;
    int limit = work->hit_limit - 1;

    work->hit_count = count + 1;
    if ((short)count >= limit) {
        int step = work->unique_step + 1;

        work->unique_step = step;
        if (work->unique_pattern_step[work->unique_pattern][(short)step] != -1) {
            Enemy_ActionReady(actor, 9);
        } else {
            Enemy_ActionReady(actor, 4);
            if (work->state_flags & ENEMY_ALERT_RAISE_FLAG)
                work->state_40.spline_parameter += 2;
        }
    }
}

#define D_004D8044 0.3f
#define D_004D8048 1.9f
#define D_004D804C -0.02f
#define D_004D8050 3.1415927f
#define D_004D8054 3.1415927f
#define D_004D8058 0.3f
#define D_004D805C 1.9f
#define D_004D8060 -0.02f
#define ENEMY_AIRBORNE_FLAG 0x20
typedef struct EnemyEffectAU {
    unsigned char unmodeled_00[160];
    float scale_x;
    float scale_y;
    float scale_z;
    unsigned char unmodeled_ac[1724 - 172];
    EnemyActorAU *owner;
} EnemyEffectAU;
typedef struct EnemyGameLoopStateAU {
    unsigned char unmodeled_00[4];
    EnemyActorAU *player;
    unsigned char unmodeled_08[0x29f8c];
    short encount_reset;
    unsigned char unmodeled_29f96[0x9a];
} EnemyGameLoopStateAU;
typedef struct EnemyPadViewAU {
    unsigned char unmodeled_00[0x28];
    unsigned short held;
    unsigned short pressed;
} EnemyPadViewAU;
extern EnemyActorAU actor_AU[64] __asm__("actor");
extern EnemyWorkEntryAU enepc_AU[16] __asm__("enepc");
extern EnemyGameLoopStateAU GameLoopState_AU __asm__("GameLoopState");
extern EnemyPadViewAU PadData;
extern short DebugActor;
extern short NowTester;
extern EnemyVector4AU EnemyTarget;
extern short EfTab_Code[];
extern short EfTab_Intr[];
extern float EfTab_Scal[];
extern const char D_004CAB48[];
extern const char D_004CAB60[];
extern const char D_004CAB78[];
extern const char D_004CAB90[];
extern const char D_004CABA8[];
extern const char D_004CABC0[];
extern int ENEMY_UNUSED_AU;
void DispPillar(const void *position, const void *size, const int *color);
void GameIdLightSet(EnemyActorAU *actor, void *query);
void *sefCreateEffectCf(int effect_id, void *position, void *orientation);
void ACT_updateSequence(EnemyActorAU *actor);
void ACT_setMotion(EnemyActorAU *actor, unsigned int data_id);
void ACT_updateMotion(EnemyActorAU *actor);
void Actor_LookAt(EnemyActorAU *actor);
void Check_Locater(EnemyActorAU *actor);
void Script_Action(EnemyActorAU *actor);
void Get_EnemyIDPos(const Vector4 *origin, Vector4 *destination);
void Start_Enemy_Command(EnemyActorAU *actor);
int Check_InsideFan(float facing, float radius, float fan_width_degrees,
                    float crossing_radius, const Vector4 *origin,
                    const Vector4 *target, int map_index);
int Check_InsideFan_Wooo(float facing, float radius, float fan_width_degrees,
                         float crossing_radius, const Vector4 *origin,
                         const Vector4 *target, int map_index);
void Check_Discovery(EnemyActorAU *actor);
void Sound_FootStep(EnemyActorAU *actor);
int RES_GetEnemySeType(short sound_effect_id);
int RES_GetEnemySeBank(short sound_effect_id);
void xglSoundEffectStopID(int sound_id, int owner);
void xglSoundEffectPosID(int sound_id, const Vector4 *position, int flags,
                         int owner);
int SCRIPT_getCfTime(void);
void Move_BeltConveyer(EnemyActorAU *actor);
float Get_Cursol_by_Reduce_Speed_Angle_Loop(float current, float target,
                                            float speed);
struct EnemyUndulationHeader *UnduDataGetHeader(int map_index, int unit_index);
void Disp_EnemyMark();

void ACT_updateEnemy(EnemyActorAU *self)
{
    Vector4 marker;
    int color[4];
    EnemyVector4AU pillar;
    EnemyParamsAU *params = &self->params;
    EnemyWorkEntryAU *work = &enepc_AU[self->number];
    int effect_code;
    float angle;
    short i;
    int type;
    unsigned short id;

    color[2] = 255;
    color[3] = 128;
    marker.y = 2.0f;
    marker.w = 1.0f;
    color[0] = 0;
    color[1] = 0;
    marker.x = 0.0f;
    marker.z = 0.0f;
    if (self->number == DebugActor) {
        xglFontDebugPrintf(8, 52, D_004CAB48, self->number, (double)self->anim_time);
        if (self->number == DebugActor) {
            xglFontDebugPrintf(8, 62, D_004CAB60, self->number, (double)self->anim_start);
            if (self->number == DebugActor) {
                xglFontDebugPrintf(8, 72, D_004CAB78, self->number, (double)self->anim_final);
                if (self->number == DebugActor) {
                    xglFontDebugPrintf(8, 82, D_004CAB90, self->number, (double)self->speed);
                    if (self->number == DebugActor) {
                        xglFontDebugPrintf(8, 92, D_004CABA8, self->number, self->anim_number);
                        if (self->number == DebugActor)
                            xglFontDebugPrintf(8, 102, D_004CABC0, self->number, (double)self->anim_blend);
                    }
                }
            }
        }
    }
    if (FLAG_FRAME_60) {
        pillar = self->position_pair;
        pillar.vector.x = self->body_radius;
        pillar.vector.y = 0.0f;
        DispPillar(&self->position, &pillar, color);
    }
    work->move_result = 99;
    if (params->gravity_mode != 1) {
        if (Get_Height(&self->position, work->map_index, 16) != -1000.0f)
            self->saved_position = self->position_pair;
    }
    if (work->sense_block_timer > 0)
        work->sense_block_timer--;
    if (!(self->flags & ACTOR_FLAG_COLLIDABLE) || self->sound_effect_id <= 0 ||
        self == GameLoopState_AU.player)
        return;

    if (work->applied_light_level != params->light_level) {
        if (params->light_level == 1000) {
            self->flags &= ~0x800;
        } else if (work->applied_light_level == 1000) {
            self->flags |= 0x800;
            self->light_flags |= 3;
            self->light_blend_frames = 72;
        }
        self->light_scale = (float)params->light_level / 1000.0f;
    }
    work->applied_light_level = params->light_level;
    GameIdLightSet(self, 0);

    if (self->sound_effect_id != 16646 && self->sound_effect_id != 20594) {
        effect_code = Get_EffectCode(Get_Attr(&self->position, 0, 0));
        if (work->action == 6 || work->action == 3 || work->action == 4 ||
            work->action == 8 || work->action == 10 || work->action == 14 ||
            work->action == 15 || EfTab_Code[effect_code] == 1) {
            if (effect_code != -1) {
                EnemyEffectAU *effect;

                if (EfTab_Intr[effect_code] != -1)
                    self->effect_interval++;
                if ((EfTab_Intr[effect_code] != -1 &&
                     self->effect_interval >= EfTab_Intr[effect_code]) ||
                    self->effect_interval == -1) {
                    effect = sefCreateEffectCf(EfTab_Code[effect_code], 0, 0);
                    self->effects[self->effect_next] = effect;
                    if (effect != 0) {
                        float size = params->body_radius * EfTab_Scal[effect_code];

                        effect->owner = self;
                        self->effect_interval = 0;
                        self->effect_next = (self->effect_next + 1) % 8;
                        effect->scale_z = size;
                        effect->scale_y = size;
                        effect->scale_x = size;
                    }
                }
            } else {
                for (i = 0; i < 8; i++) {
                    if (self->effects[i])
                        sefDeleteEffectCf(self->effects[i]);
                }
                self->effect_interval = -1;
            }
        }
    }

    if (work->target_number == 0)
        work->escape_target.position_pair = EnemyTarget;
    else
        Get_EnemyIDPos(&actor_AU[work->target_number].position,
                       &work->escape_target.position);
    Start_Enemy_Command(self);
    if (work->freeze_state == 3) {
        ACT_updateSequence(self);
        return;
    }
    if (work->escape_target.scale_frame != -1 &&
        (unsigned char)work->freeze_state < 2) {
        short frame = work->escape_target.scale_frame;
        short count = work->escape_target.scale_frame_count;
        float scale;

        work->escape_target.scale =
            work->escape_target.scale_to -
            (work->escape_target.scale_to - work->escape_target.scale_from) *
                (float)(count * count - frame * frame) / (float)(count * count);
        ACT_setMotion(self, self->anim_number);
        work->escape_target.scale_frame++;
        if (work->escape_target.scale_frame >= work->escape_target.scale_frame_count)
            work->escape_target.scale_frame = -1;
        scale = work->escape_target.scale;
        self->scale.x = self->scale.y = self->scale.z = scale;
        self->scale.w = 1.0f;
    }
    if (self->lookat_timer == 2)
        Actor_LookAt(self);
    if (self->sound_effect_id == 8705)
        Check_Locater(self);

    if (work->freeze_state != 0) {
        if (work->freeze_state < 2)
            Script_Action(self);
        if (Get_Distance(&self->position, &GameLoopState_AU.player->position) <=
                self->body_radius + GameLoopState_AU.player->body_radius + D_004D8044 &&
            __builtin_fabsf(self->position.y - GameLoopState_AU.player->position.y) <
                D_004D8048 &&
            GameLoopState_AU.player->encount_lock == 0 &&
            enepc_AU[GameLoopState_AU.player->number].encount_lock == 0 &&
            FLAG_FRAME_60 == 0)
            Check_Encount(self, 0, 0);
        {
            float scale = work->escape_target.scale;

            self->scale.x = self->scale.y = self->scale.z = scale;
            self->scale.w = 1.0f;
        }
        ACT_updateMotion(self);
        if (work->freeze_state == 1 && params->gravity_mode == 0) {
            float height;

            self->undulation_header = UnduDataGetHeader(work->map_index, 0x8000);
            height = Get_Height(&self->position, work->map_index, 16);
            self->ground_height = height;
            work->fall_speed += D_004D804C;
            self->position.y += work->fall_speed;
            if (self->position.y < height) {
                self->position.y = height;
                work->fall_speed = 0.0f;
            }
        }
        if (work->mark != 0)
            Disp_EnemyMark(self);
        self->undulation_header = UnduDataGetHeader(0, 0x8000);
        return;
    }

    if (self->sound_effect_id != 8705)
        Check_Locater(self);
    switch (work->action) {
    case 0:
        Enemy_Init(self);
    case 1:
        Enemy_Pause(self);
        break;
    case 2:
        Enemy_Threat(self);
        break;
    case 4:
        Enemy_Route(self);
        break;
    case 5:
        Enemy_Found(self);
        break;
    case 6:
        Enemy_Chase(self);
        break;
    case 10:
        Enemy_Tentacle(self);
        break;
    case 7:
        Enemy_Stiff(self);
        break;
    case 9:
        Enemy_Unique(self);
        break;
    case 11:
        Enemy_Damage_Explosion(self);
        break;
    case 12:
        Enemy_Damage_Electric(self);
        break;
    case 13:
        Enemy_Damage_Seal(self);
        break;
    case 14:
        Enemy_Escape(self);
        break;
    case 15:
        Enemy_Route_Chase(self);
        break;
    }

    if (params->sight_distance != 0.0f) {
        if (!(self->flags & 8) && work->sense_block_timer <= 0 &&
            enepc_AU[0].encount_lock == 0) {
            if (params->sight_angle < 0.0f) {
                if (self->sound_effect_id == 8705 || self->sound_effect_id == 16646) {
                    if (Check_InsideFan_Wooo(self->angle + D_004D8050,
                                             params->sight_distance,
                                             __builtin_fabsf(params->sight_angle),
                                             self->body_radius, &self->position,
                                             &work->escape_target.position,
                                             work->map_index) == 1)
                        Check_Discovery(self);
                } else if (Check_InsideFan(self->angle + D_004D8054,
                                           params->sight_distance,
                                           __builtin_fabsf(params->sight_angle),
                                           self->body_radius, &self->position,
                                           &work->escape_target.position,
                                           work->map_index) == 1) {
                    Check_Discovery(self);
                }
            } else {
                if (self->sound_effect_id == 8705 || self->sound_effect_id == 16646) {
                    if (Check_InsideFan_Wooo(self->angle, params->sight_distance,
                                             __builtin_fabsf(params->sight_angle),
                                             self->body_radius, &self->position,
                                             &work->escape_target.position,
                                             work->map_index) == 1)
                        Check_Discovery(self);
                } else if (Check_InsideFan(self->angle, params->sight_distance,
                                           __builtin_fabsf(params->sight_angle),
                                           self->body_radius, &self->position,
                                           &work->escape_target.position,
                                           work->map_index) == 1) {
                    Check_Discovery(self);
                }
            }
        }
    }
    if (FLAG_FRAME_60 && (PadData.pressed & 1)) {
        angle = self->target_angle;
        work->state_40.spline_parameter = 0;
        BSpline_Init(work->spline_points, &self->position, angle);
        Enemy_ActionReady(self, 2);
        Homing_Search(self);
    }

    if ((unsigned short)self->sound_effect_id - 0x4000 < 0xf23u) {
        if (self->flags & ACTOR_FLAG_AILMENT_ACTIVE) {
            if (self->flags & ACTOR_FLAG_STIFF_ACTIVE) {
                if (work->action != 11 && work->action != 7)
                    Enemy_ActionReady(self, 11);
            }
            if (self->flags & 0x02000000) {
                if (work->action != 12)
                    Enemy_ActionReady(self, 12);
            }
            if (self->flags & 0x08000000) {
                if (work->action != 13)
                    Enemy_ActionReady(self, 13);
            }
        }
    }
    id = self->sound_effect_id;
    type = RES_GetEnemySeType(id);
    if (type == 1) {
        if (self->anim_number == 1 || self->anim_number == 3)
            Sound_FootStep(self);
    } else {
        if (work->action != 4) {
            xglSoundEffectStopID(RES_GetEnemySeBank(self->sound_effect_id) + 2,
                                 self->number + 1);
        } else if (SCRIPT_getCfTime() >= 3) {
            xglSoundEffectPosID(RES_GetEnemySeBank(self->sound_effect_id) + 2,
                                &self->position, 1, self->number + 1);
        }
        if (work->action != 6) {
            xglSoundEffectStopID(RES_GetEnemySeBank(self->sound_effect_id) + 4,
                                 self->number + 1);
        } else {
            xglSoundEffectPosID(RES_GetEnemySeBank(self->sound_effect_id) + 4,
                                &self->position, 1, self->number + 1);
        }
    }
    if (work->effect_timer[0] > 0) {
        if (--work->effect_timer[0] <= 0) {
            work->effect_timer[0] = 0;
            work->stiff_flag &= ~2;
            sefDeleteEffectCf(work->stiff_effect);
            work->stiff_effect = 0;
            xglSoundEffectStopID(0x10008, 0);
        }
    }
    if (work->effect_timer[1] > 0) {
        if (--work->effect_timer[1] == 0) {
            work->stiff_flag &= ~4;
            xglSoundEffectStopID(0x1000b, 0);
        }
    }
    if (FLAG_FRAME_60) {
        if (PadData.held & 1)
            GameLoopState_AU.player = &actor_AU[NowTester];
        else
            GameLoopState_AU.player = &actor_AU[0];
    }
    if (Get_Distance(&self->position, &GameLoopState_AU.player->position) <=
            self->body_radius + GameLoopState_AU.player->body_radius + D_004D8058 &&
        __builtin_fabsf(self->position.y - GameLoopState_AU.player->position.y) <
            D_004D805C &&
        GameLoopState_AU.player->encount_lock == 0 &&
        enepc_AU[GameLoopState_AU.player->number].encount_lock == 0 &&
        FLAG_FRAME_60 == 0)
        Check_Encount(self, 0, 0);
    Script_Action(self);
    if (work->state_flags & 4)
        Move_BeltConveyer(self);
    if (work->turn_lock != 1 && work->action != 7) {
        if (work->stiff_flag & 2)
            self->angle = Get_Cursol_by_Reduce_Speed_Angle_Loop(self->angle, self->target_angle, 36.0f);
        else
            self->angle = Get_Cursol_by_Reduce_Speed_Angle_Loop(self->angle, self->target_angle, 6.0f);
    }
    {
        float scale = work->escape_target.scale;

        self->scale.x = self->scale.y = self->scale.z = scale;
        self->scale.w = 1.0f;
    }
    if (params->gravity_mode != 1) {
        if (Get_Height(&self->position, work->map_index, 16) == -1000.0f)
            self->position_pair = self->saved_position;
    }
    ACT_updateMotion(self);
    if (params->gravity_mode == 0) {
        float height = Get_Height(&self->position, work->map_index, 16);

        self->ground_height = height;
        work->fall_speed += D_004D8060;
        if (!(work->state_flags & ENEMY_AIRBORNE_FLAG))
            self->position.y += work->fall_speed;
        if (self->position.y < height) {
            self->position.y = height;
            work->fall_speed = 0.0f;
        }
    }
    if (work->mark != 0)
        Disp_EnemyMark(self);
}

void Get_One_Step(float length, const Vector4 *source,
                  const Vector4 *destination, float *output);
float atan2f(float y, float x);
typedef struct EnemyActorMoveUP3 {
    u32 flags;
    unsigned char unmodeled_04[0x0c];
    union {
        Vector4 position;
        EnemyVector4AU position_pair;
        float position_f[4];
    };
    unsigned char unmodeled_20[0x9e4 - 0x20];
    float target_angle;
    unsigned char unmodeled_9e8[sizeof(EnemyActorAU) - 0x9e8];
} EnemyActorMoveUP3;
typedef struct EnemyWorkMoveUP3 {
    unsigned char unmodeled_00[0x10];
    union {
        struct {
            float goback_x;
            unsigned char unmodeled_014[4];
            float goback_z;
            unsigned char unmodeled_01c[4];
        };
        float goback[4];
    };
    Vector4 step_limit;
    union {
        struct {
            float drift_x;
            float fall_speed;
            float drift_z;
            unsigned char unmodeled_03c[4];
        };
        float drift[4];
    };
    unsigned char unmodeled_40[sizeof(EnemyWorkEntryAU) - 0x40];
} EnemyWorkMoveUP3;

unsigned char TM_Enemy_Move_Step(int actor_number, Vector4 *step_limit,
                                 float goback_x)
{
    extern EnemyActorMoveUP3 actor_UP3[64] __asm__("actor");
    extern EnemyWorkMoveUP3 enepc_UP3[16] __asm__("enepc");
    EnemyActorMoveUP3 *self = &actor_UP3[actor_number];
    EnemyVector4AU old = self->position_pair;

    Get_One_Step(goback_x, &actor_UP3[actor_number].position, step_limit,
                 &enepc_UP3[actor_number].goback[0]);
    enepc_UP3[actor_number].goback[0] += (&enepc_UP3[actor_number])->drift[0];
    enepc_UP3[actor_number].goback[2] += enepc_UP3[actor_number].drift[2];

    if (old.vector.x < step_limit->x) {
        if (step_limit->x < self->position.x + enepc_UP3[actor_number].goback[0])
            enepc_UP3[actor_number].goback[0] = step_limit->x - self->position.x;
    }
    if (old.vector.z < step_limit->z) {
        if (step_limit->z < self->position.z + enepc_UP3[actor_number].goback[2])
            enepc_UP3[actor_number].goback[2] = step_limit->z - self->position.z;
    }

    enepc_UP3[actor_number].drift[0] *= 0.9f;
    enepc_UP3[actor_number].drift[2] *= 0.9f;
    actor_UP3[actor_number].position.x += enepc_UP3[actor_number].goback[0];
    actor_UP3[actor_number].position_f[2] += enepc_UP3[actor_number].goback[2];
    actor_UP3[actor_number].target_angle =
        atan2f(enepc_UP3[actor_number].goback[0], enepc_UP3[actor_number].goback[2]);
    return TM_Check_Move((EnemyActor *)self);
}

void TM_Check_Move_at_GoBack(EnemyActor *actor)
{
    EnemyWorkEntry *work = &enepc[actor->number];

    actor->position.x -= work->goback_x;
    actor->position.z -= work->goback_z;
}

extern long long EneTerritoryID[];
int Get_Attr(const Vector4 *position, int map_index, int attr_mask);
int Check_Undu(Vector4 *from, Vector4 *to, int map_index, int *out, int a, int b, int c);
int Check_Actor(const Vector4 *position, short number);
int HitCheckMapUnitPosSize(const Vector4 *position, float radius);
int CheckBrokenMapUnit(void *unit);
typedef union { Vector4 v; long long d[2]; } Vec4Words;

int TM_Check_Move(EnemyActor *arg0)
{
    EnemyWorkEntry *work;
    Vec4Words sp0;
    Vec4Words sp10;
    int sp20;
    float height;
    int attr;
    signed char kind;
    signed char ret;
    unsigned char code;

    work = &enepc[arg0->number];
    height = Get_Height(&arg0->position, work->map_index, 0x10);
    attr = Get_Attr(&arg0->position, work->map_index, 0x10);
    if (height == -1000.0f) {
        ((int (*)())TM_Check_Move_at_GoBack)(arg0);
        code = 0x40;
        goto block_27;
    }
    {
        unsigned long long lo;
        unsigned long long hi;
        lo = *(unsigned long long *)&arg0->position;
        hi = ((unsigned long long *)&arg0->position)[1];
        sp10.d[0] = lo;
        sp10.d[1] = hi;
        sp0.d[0] = lo;
        sp0.d[1] = hi;
        sp10.v.y = height;
        sp0.v.x = sp0.v.x - work->goback_x;
        sp0.v.z = sp0.v.z - work->goback_z;
        if (Check_Undu(&sp0.v, &sp10.v, work->map_index, &sp20, 1, 0, 0x10) == 0) {
            TM_Check_Move_at_GoBack(arg0);
            code = 0x40;
        } else {
            short territory = *(short *)((char *)work + 0xA);
            if ((attr != EneTerritoryID[territory]) && (territory != 0)) {
                kind = (signed char)work->unmodeled_048[0];
                if ((kind != 8) && ((int)arg0->flags >= 0)) {
                    if (work->state_flags & 8) {
                        if (kind != 6) {
                            TM_Check_Move_at_GoBack(arg0);
                            code = 0x50;
                            goto block_27;
                        }
                        goto block_17;
                    }
                    TM_Check_Move_at_GoBack(arg0);
                    code = 0x58;
                    goto block_27;
                }
            }
block_17:
            ret = Check_Actor(&arg0->position, (short)arg0->number);
            if (ret != -1) {
                TM_Check_Move_at_GoBack(arg0);
                work->unmodeled_048[1] = (unsigned char)ret;
                return 0;
            }
            if (((int)arg0->flags >= 0) && (HitCheckMapUnitPosSize(&arg0->position, arg0->body_radius) != ret)) {
                if (CheckBrokenMapUnit((HitCheckMapUnitPosSize(&arg0->position, arg0->body_radius) * 0x300) + MapUnit) & 2) {
                    SendBrokenSignal((HitCheckMapUnitPosSize(&arg0->position, arg0->body_radius) * 0x300) + MapUnit);
                }
                TM_Check_Move_at_GoBack(arg0);
                code = 0x60;
                goto block_27;
            }
            work->unmodeled_048[1] = 0x63;
            goto _sw_tl1;
        }
    }
block_27:
    work->unmodeled_048[1] = code;
    return 0;
_sw_tl1:
    return 1;
}

char TM_Check_Before_Eyes(EnemyActor *self, float length)
{
    EnemyWorkEntry *work = &enepc[self->number];
    Vector4 point;
    short i;

    Get_Point_By_AngleLength(&self->position, &point, self->angle, length);

    for (i = 0; i < 64; i++) {
        if (actor[i].sound_effect_id <= 0)
            continue;
        if (actor[i].kind == 1)
            continue;
        if (!(actor[i].flags & ACTOR_FLAG_COLLIDABLE))
            continue;
        if (self->number == i)
            continue;
        if (Get_Distance(&point, &actor[i].position) <=
            self->body_radius + actor[i].body_radius) {
            return i;
        }
    }

    if (Get_Height(&self->position, work->map_index, 0) == -1000.0f)
        return 64;
    return 99;
}

void Set_EnemyMark(EnemyActor *actor, short mark)
{
    EnemyWorkEntry *work = &enepc[actor->number];

    work->mark = mark;
    work->mark_timer = 0;
}

void Disp_EnemyMark(void)
{
    EnemyMarkSet mark[16];

    mark[15].none = 0;
    mark[15].exclamation = AdrsEnemyExclamation;
    mark[15].question = AdrsEnemyQuestion;
    mark[15].sphere = AdrsEnemySphere;
    mark[14] = mark[15];
}

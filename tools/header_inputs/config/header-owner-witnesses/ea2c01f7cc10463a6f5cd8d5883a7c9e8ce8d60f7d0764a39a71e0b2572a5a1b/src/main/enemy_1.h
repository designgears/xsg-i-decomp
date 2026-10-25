#ifndef ENEMY_1_H
#define ENEMY_1_H

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

typedef union EnemyVector4 {
    Vector4 vector;
    unsigned long long words[2];
} EnemyVector4;

typedef struct EnemyActor {
    u32 flags;
    unsigned char unmodeled_04[0x0c];
    union {
        Vector4 position;
        EnemyVector4 position_pair;
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
    EnemyParams params;
    unsigned char unmodeled_130[0x398];
    EnemyUndulation undulation;
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
    EnemyVector4 saved_position;
    struct EnemyEffect *effects[8];
    short effect_interval;
    short effect_next;
    unsigned char unmodeled_a34[0x3c];
} EnemyActor;

typedef struct EnemyEffect {
    unsigned char unmodeled_00[160];
    float scale_x;
    float scale_y;
    float scale_z;
    unsigned char unmodeled_ac[1724 - 172];
    EnemyActor *owner;
} EnemyEffect;

typedef union EnemyWorkState40 {
    short alert_level;
    short spline_parameter;
} EnemyWorkState40;

typedef struct EnemyEscapeTarget {
    union {
        Vector4 position;
        EnemyVector4 position_pair;
    };
    float scale;
    float scale_from;
    float scale_to;
    short scale_frame;
    short scale_frame_count;
} EnemyEscapeTarget;

typedef struct EnemyWorkEntry {
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
    EnemyWorkState40 state_40;
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
    EnemyEscapeTarget escape_target;
    u32 state_flags;
    unsigned char unmodeled_37e4[0xc];
    EnemyVector4 last_player_position;
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
} EnemyWorkEntry;

typedef struct EnemyMarkSet {
    void *none;
    void *exclamation;
    void *question;
    void *sphere;
} EnemyMarkSet;

typedef struct EnemyMapUnit {
    unsigned char unmodeled_000[0x300];
} EnemyMapUnit;

extern EnemyMapUnit MapUnit[64];
extern long EneTerritoryID[20];
extern signed char FLAG_FRAME_60;
extern EnemyActor actor[64];
extern EnemyWorkEntry enepc[16];
typedef struct EnemyGameLoopState {
    unsigned char unmodeled_00[4];
    EnemyActor *player;
    unsigned char unmodeled_08[0x29f8c];
    short encount_reset;
    unsigned char unmodeled_29f96[0x9a];
} EnemyGameLoopState;

extern EnemyGameLoopState GameLoopState;
extern unsigned short CoolDown_0;
extern unsigned short CoolDown_1;
extern const float D_004D8034;
extern const float D_004D8038;
extern const float D_004D803C;
extern const float D_004D8040;
extern void *AdrsEnemyExclamation;
extern void *AdrsEnemyQuestion;
extern void *AdrsEnemySphere;

float Get_Angle(const Vector4 *from, const Vector4 *to);
float sinf(float x);
float cosf(float x);
int TM_Check_Move(EnemyActor *actor);
void Set_Spline_By_Route(EnemyActor *actor, int route);
void Set_Spline_By_Random();
void BSpline_Init(Vector4 *points, const Vector4 *origin, float angle);
int Get_Rnd(int low, int high);
int CrossPointUwamono(float radius, const Vector4 *from, const Vector4 *to,
                      Vector4 *point);
int Check_MoveSquare(float radius, const Vector4 *to, const Vector4 *from,
                     Vector4 *square, int map_index);
void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern const char D_004DB7F8[];
extern const char D_004DB800[];
extern const char D_004DB808[];
extern const char D_004DB810[];
extern const char D_004DB818[];
void TM_Check_Move_at_GoBack(EnemyActor *actor);
int Get_Attr(const Vector4 *position, int map_index, int attr_mask);
int Check_Undu(Vector4 *start, Vector4 *end, int map_index, Vector4 *hit,
               int a, int b, int attr_mask);
int Check_Actor(const Vector4 *position, unsigned char number);
int HitCheckMapUnitPosSize(const Vector4 *position, float radius);
int CheckBrokenMapUnit(EnemyMapUnit *unit);
int CrossCheckMapUnit(const Vector4 *from, const Vector4 *to);
void SendBrokenSignal(EnemyMapUnit *unit);
void Debug_Mark(const Vector4 *position, void *mark);
void Check_Encount(EnemyActor *actor, int enable, signed char command);
unsigned char TM_Enemy_Move_Step(int actor_number,
                                 Vector4 *step_limit, float goback_x);
float Get_Distance(const Vector4 *from, const Vector4 *to);
float Get_Height(const Vector4 *position, int map_index, int attr_mask);
void Get_Point_By_AngleLength(const Vector4 *origin, Vector4 *point,
                              float angle, float length);
void GetBSplineLoop(short parameter, short point_count, int stride,
                    Vector4 *points, Vector4 *result);
short Get_DefaultMotion(EnemyActor *actor, int mode);
void Set_Motion(EnemyActor *actor, short motion, int mode);
void Enemy_ActionReady(EnemyActor *actor, int action);
void Homing_Search(EnemyActor *actor);
void *sefDeleteEffectCf(void *effect);
unsigned short xglSRand(void);

#define ACTOR_FLAG_AILMENT_ACTIVE 0x00800000
#define ACTOR_FLAG_STIFF_ACTIVE   0x01000000
#define ACTOR_FLAG_FACE_PLAYER    0x00400000
#define ACTOR_FLAG_COLLIDABLE     0x00010000
#define ENEMY_STIFF_ACTIVE_BIT    0x01
#define ENEMY_ALERT_RAISE_FLAG    0x10
#define ENEMY_AIRBORNE_FLAG       0x20

extern EnemyPreset *AdrsEnemy;
typedef struct EnemyResourceRecord {
    void *motion;
    unsigned char unmodeled_04[0xc];
} EnemyResourceRecord;
extern EnemyResourceRecord GameResource[128];
extern const char D_004CAB00[];
void Homing_Add(unsigned char number, float x, float y, float z, short value);
void ACT_initExMotion(EnemyActor *target, unsigned int slot, void *motion);
void CallMethod_I(const char *method, int value);
void UnduParamInit(void *undulation);
float UnduCheck(const Vector4 *position, void *velocity,
                EnemyUndulation *undulation);

int Get_EffectCode(int attr);

typedef struct EnemyPadView {
    unsigned char unmodeled_00[0x28];
    unsigned short held;
    unsigned short pressed;
} EnemyPadView;

extern EnemyPadView PadData;
extern short DebugActor;
extern short NowTester;
extern EnemyVector4 EnemyTarget;
extern short EfTab_Code[];
extern short EfTab_Intr[];
extern float EfTab_Scal[];
extern const char D_004CAB48[];
extern const char D_004CAB60[];
extern const char D_004CAB78[];
extern const char D_004CAB90[];
extern const char D_004CABA8[];
extern const char D_004CABC0[];

void DispPillar(const void *position, const void *size, const int *color);
void GameIdLightSet(EnemyActor *actor, void *query);
void *sefCreateEffectCf(int effect_id, void *position, void *orientation);
void ACT_updateSequence(EnemyActor *actor);
void ACT_setMotion(EnemyActor *actor, unsigned int data_id);
void ACT_updateMotion(EnemyActor *actor);
void Actor_LookAt(EnemyActor *actor);
void Check_Locater(EnemyActor *actor);
void Script_Action(EnemyActor *actor);
void Get_EnemyIDPos(const Vector4 *origin, Vector4 *destination);
void Start_Enemy_Command(EnemyActor *actor);
int Check_InsideFan(float facing, float radius, float fan_width_degrees,
                    float crossing_radius, const Vector4 *origin,
                    const Vector4 *target, int map_index);
int Check_InsideFan_Wooo(float facing, float radius, float fan_width_degrees,
                         float crossing_radius, const Vector4 *origin,
                         const Vector4 *target, int map_index);
void Check_Discovery(EnemyActor *actor);
void Sound_FootStep(EnemyActor *actor);
int RES_GetEnemySeType(short sound_effect_id);
int RES_GetEnemySeBank(short sound_effect_id);
void xglSoundEffectStopID(int sound_id, int owner);
void xglSoundEffectPosID(int sound_id, const Vector4 *position, int flags,
                         int owner);
int SCRIPT_getCfTime(void);
void Move_BeltConveyer(EnemyActor *actor);
float Get_Cursol_by_Reduce_Speed_Angle_Loop(float current, float target,
                                            float speed);
struct EnemyUndulationHeader *UnduDataGetHeader(int map_index, int unit_index);
void Disp_EnemyMark();
void Enemy_Init(EnemyActor *actor);
void Enemy_Pause(EnemyActor *actor);
void Enemy_Threat(EnemyActor *actor);
void Enemy_Route(EnemyActor *actor);
void Enemy_Found(EnemyActor *actor);
void Enemy_Chase(EnemyActor *actor);
void Enemy_Tentacle(EnemyActor *actor);
void Enemy_Stiff(EnemyActor *actor);
void Enemy_Unique(EnemyActor *actor);
void Enemy_Damage_Explosion(EnemyActor *actor);
void Enemy_Damage_Electric(EnemyActor *actor);
void Enemy_Damage_Seal(EnemyActor *actor);
void Enemy_Escape(EnemyActor *actor);
void Enemy_Route_Chase(EnemyActor *actor);

#endif

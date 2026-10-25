#ifndef ENEMY_1_H
#define ENEMY_1_H

#include "common.h"
#include "shared.h"


typedef struct EnemyActor {
    u32 flags;
    unsigned char unmodeled_04[0x0c];
    Vector4 position;
    unsigned char unmodeled_20[0x34];
    float angle;
    unsigned char unmodeled_58[0x28];
    unsigned char number;
    unsigned char unmodeled_81;
    unsigned char kind;
    unsigned char unmodeled_83[3];
    short sound_effect_id;
    unsigned char unmodeled_88[0x38];
    EnemyParams params;             /* +0xc0 */
    unsigned char unmodeled_130[0x6f8 - 0x130];
    float speed;
    unsigned char unmodeled_6fc[0x2e8];
    float target_angle;
    float body_radius;
    unsigned char unmodeled_9ec[0x84];
} EnemyActor;

typedef union EnemyWorkState40 {
    short alert_level;
    short spline_parameter;
} EnemyWorkState40;

typedef struct EnemyEscapeTarget {
    Vector4 position;
    unsigned char unmodeled_10[0]; /* +0x10: zero-length alias of the former padding member, kept for saved bodies */
    float scale;
    float scale_from;
    float scale_to;
    short scale_frame;
    short scale_frame_count;
} EnemyEscapeTarget;

typedef struct EnemyWorkEntry {
    unsigned char unmodeled_000[0x0a];
    short wall_attribute;           /* +0x0a */
    unsigned char unmodeled_00c[0x04];
    float goback_x;
    unsigned char unmodeled_014[4];
    float goback_z;
    unsigned char unmodeled_01c[4];
    Vector4 step_limit;
    unsigned char unmodeled_030[0x10];
    EnemyWorkState40 state_40;
    short fast_point_count;         /* +0x42 */
    unsigned short hit_count;
    short hit_limit;
    unsigned char unmodeled_048[2];
    unsigned char pause_kind;
    unsigned char unmodeled_04b[3];
    unsigned short stiff_timer;
    short effect_timer[2];
    signed char route_marks[2][8];  /* +0x54 */
    short route_cursor[2];          /* +0x64 */
    unsigned char unmodeled_068[2];
    unsigned char stiff_flag;
    unsigned char unmodeled_06b[5];
    int map_index;
    short mark;
    short mark_timer;
    unsigned char unmodeled_078[8];
    union {
        Vector4 spline_points[256];
        float spline_coordinates[256][4];
    };
    short spline_point_count;
    unsigned char unmodeled_1082[0xe];
    Vector4 route_points[256];      /* +0x1090 */
    union {
        short unique_pattern_step[512][4];
        struct {
            short action_motion_table[256][4]; /* +0x2090 */
            short motion_id_table[256][4];
        };
    };
    short unique_pattern;
    unsigned short unique_step;
    unsigned char unmodeled_3094[0x72c];
    EnemyEscapeTarget escape_target;
    u32 state_flags;
    unsigned char unmodeled_37e4[0x70];
    float stiff_saved_speed;
    unsigned char unmodeled_3858[0x48];
    void *stiff_effect;
    unsigned char unmodeled_38a4[0x0c];
} EnemyWorkEntry;

typedef struct EnemyMarkSet {
    void *none;
    void *exclamation;
    void *question;
    void *sphere;
} EnemyMarkSet;

extern EnemyActor actor[64];
extern EnemyWorkEntry enepc[16];
extern EnemyActor *GameLoopState[64];
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

int Get_EffectCode(int attr);

#endif

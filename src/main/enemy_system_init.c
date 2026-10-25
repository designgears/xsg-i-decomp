#include "common.h"
#include "shared.h"
#include "main/xgl_cd.h"

#include "enemy_system_init.h"

extern const char D_004CAE30[];
extern const char D_004CAE48[];
extern const char D_004CAE68[];
extern const char D_004CAE88[];
extern const char D_004CAEA8[];
extern const char D_004CAEC0[];
typedef struct EnemyPresetPathPrefix {
    char bytes[15];
} EnemyPresetPathPrefix;
typedef struct EnemyPresetPathParts {
    EnemyPresetPathPrefix prefix;
    char suffix[241];
} EnemyPresetPathParts;
typedef union EnemyPresetPathStorage {
    EnemyPresetPathParts parts;
    char bytes[256];
} EnemyPresetPathStorage;
extern const EnemyPresetPathPrefix D_004CAEE0;
extern const char D_004DB858[];
extern char *strcat(char *destination, const char *source);

void Enemy_SystemInit(void) {
    AdrsEnemyPreset = WorkEnd;
    GameLoopState[0x1c / sizeof(unsigned int)] = 0;
    WorkEnd += xglCdReadFile(D_004CAE30, WorkEnd, 0, 0);

    AdrsEnemySpline = WorkEnd;
    WorkEnd += xglCdReadFile(D_004CAE48, WorkEnd, 0, 0);

    AdrsEnemyExclamation = WorkEnd;
    WorkEnd += xglCdReadFile(D_004CAE68, WorkEnd, 0, 0);

    AdrsEnemyQuestion = WorkEnd;
    WorkEnd += xglCdReadFile(D_004CAE88, WorkEnd, 0, 0);

    AdrsEnemySphere = WorkEnd;
    WorkEnd += xglCdReadFile(D_004CAEA8, WorkEnd, 0, 0);

    AdrsEnemySquare = WorkEnd;
    WorkEnd += xglCdReadFile(D_004CAEC0, WorkEnd, 0, 0);
}

int Enemy_LoadPreset(void *buffer, const char *preset_name)
{
    EnemyPresetPathStorage path;

    path.parts.prefix = D_004CAEE0;
    memset(path.parts.suffix, 0, sizeof(path.parts.suffix));
    strcat(path.bytes, preset_name);
    strcat(path.bytes, D_004DB858);
    AdrsEnemyPreset = buffer;
    return xglCdReadFile(path.bytes, buffer, 0, 0);
}

typedef struct {
    char pad0[0x80];
    float spline[256][4];
    short spline_num;
    char pad1[0x38B0 - 0x1082];
} EnePcSpline;
extern EnePcSpline enepc[];

void TM_Script_Spline_Add(int no, float x, float y, float z) {
    int n = enepc[no].spline_num;
    enepc[no].spline_num++;
    enepc[no].spline[n][0] = x;
    enepc[no].spline[n][1] = y;
    enepc[no].spline[n][2] = z;
    enepc[no].spline[n][3] = 1.0f;
}

typedef struct EnemyInitParams {
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
    short move_kind;
    int preset_index;
    unsigned char unmodeled_30[0x4];
    int route_value_count;
    unsigned char unmodeled_38[0x4];
    int homing_value_count;
    float body_radius;
    int ex_motion_resource;
    float value_108;
    int event_type;
    short value_110[8];
    int value_120;
    int locator_flags;
    float hearing_distance;
    short value_12c;
    short light_level;
    short default_motion_table[7];
    unsigned char unmodeled_7e[0x182];
    int value_2c0;
    int value_2c4;
    unsigned char unmodeled_208[0x18];
    short value_2e0[4];
} EnemyInitParams;
typedef struct EnemyInitWork {
    unsigned char unmodeled_00[0x4];
    short initial_motion_id;
    short locator_start_frame;
    short locator_end_frame;
    unsigned char unmodeled_0a[0x36];
    short spline_parameter;
    short fast_point_count;
    unsigned char unmodeled_44[0x4];
    signed char action;
    unsigned char unmodeled_49[0x1f];
    signed char marker_68;
    unsigned char unmodeled_69[0x1];
    unsigned char effect_state;
    unsigned char unmodeled_6b[0x1];
    short chase_frame_limit;
    short chase_frame;
    unsigned char unmodeled_70[0x4];
    short mark;
    short mark_timer;
    unsigned char unmodeled_78[0x8];
    float spline_points[256][4];
    short spline_point_count;
    unsigned char unmodeled_1082[0x100e];
    short action_motion_table[256][4];
    short motion_id_table[256][4];
    unsigned char unmodeled_3090[0x4];
    short route_point_count;
    unsigned char unmodeled_3096[0x40a];
    short route_value[64][4];
    short route_state[64];
    unsigned char unmodeled_3720[0x80];
    short state_37a0;
    unsigned char freeze_state;
    unsigned char turn_lock;
    short target_number;
    unsigned char unmodeled_37a6[0xe];
    int state_37b4;
    unsigned char unmodeled_37b8[0x18];
    float scale;
    unsigned char unmodeled_37d4[0x8];
    short scale_frame;
    unsigned char unmodeled_37de[0x2];
    int turn_flags;
    unsigned char unmodeled_37e4[0x3c];
    short turn_state_3820;
    short turn_state_3822;
    unsigned char unmodeled_3824[0x8];
    short turn_frame;
    short turn_duration;
    short encount_lock;
    unsigned char unmodeled_3832[0x6e];
    void *effect;
    unsigned char unmodeled_38a4[0x4];
    short applied_light_level;
    short light;
    short sense_block_timer;
    unsigned char unmodeled_38ae[0x2];
} EnemyInitWork;
typedef struct EnemyInitActor {
    unsigned int flags;
    void (*update)(struct EnemyInitActor *actor);
    unsigned char unmodeled_08[0x78];
    unsigned char number;
    unsigned char unmodeled_81[0x5];
    short id;
    unsigned char unmodeled_88[0x38];
    EnemyInitParams params;
    unsigned char unmodeled_2e8[0x1e0];
    unsigned char undulation[0x524];
    short lookat_timer;
    short lookat_target;
    unsigned char unmodeled_9f0[0x20];
    void *effects[8];
    short effect_interval;
    short effect_next;
    unsigned char unmodeled_a34[0x3c];
} EnemyInitActor;
typedef struct EnemyBehaviorDefaults {
    float route_step_length;
    float chase_distance;
    float movement_speed;
    float chase_speed;
    float sight_angle;
    float sight_distance;
    short pause_duration;
    short pause_interval;
    short threat_duration;
    short threat_interval;
} EnemyBehaviorDefaults;
typedef struct EnemyEncounterParameters {
    EnemyBehaviorDefaults behavior;
    unsigned char unmodeled_20[0x50 - 0x20];
    short monster_set_numbers[8];
    unsigned char unmodeled_60[4];
    int locator_flags;
    float hearing_distance;
    short encounter_event_flags;
    short light_level;
} EnemyEncounterParameters;
typedef struct EnemyNavigationWork {
    unsigned char unmodeled_00[0x68];
    signed char encounter_side;
    unsigned char unmodeled_69[0x30a0 - 0x69];
    float homing_points[64][4];
    short neighbours[64][4];
    short neighbour_count[64];
    short linked_target[64];
    short homing_point_count;
    unsigned char unmodeled_37a2[0x3800 - 0x37a2];
    float movement_points[2][4];
    short movement_step;
    short movement_duration;
} EnemyNavigationWork;
extern EnemyInitActor actor[64];
extern void *DataSpline[1];
extern void *AdrsEnemy;
extern void UnduParamInit(void *undulation);
extern void ACT_updateEnemy(EnemyInitActor *actor);
extern void *AdrsEnemySpline;
extern void *AdrsEnemyPreset;

EnemyInitActor *ACT_createEnemy(int number, int id)
{
    EnemyInitActor *enemy;
    EnemyInitParams *params;
    EnemyBehaviorDefaults *behavior;
    EnemyEncounterParameters *encounter;
    EnemyInitWork *work;
    EnemyNavigationWork *navigation;
    short i;
    short j;
    short k;
    EnemyInitActor **loop_state;

    if (number < 0) {
        for (i = 0; i < 64; i++) {
            if (actor[i].id == 0) {
                number = i;
                break;
            }
        }
        if (number < 0) {
            return 0;
        }
    }

    enemy = &actor[number];
    params = &enemy->params;
    behavior = (void *)params;
    encounter = (void *)params;

    enemy->number = number;
    enemy->id = id;
    work = &((EnemyInitWork *)enepc)[enemy->number];
    navigation = (void *)work;
    behavior->route_step_length = 15.0f;
    params->preset_index = -1;
    params->route_value_count = 0;
    behavior->threat_duration = 20;
    behavior->pause_duration = 30;
    behavior->pause_interval = 4;
    behavior->chase_distance = 10.0f;
    behavior->sight_distance = 4.0f;
    behavior->movement_speed = 0.15f;
    behavior->chase_speed = 0.07f;
    params->value_e0 = 25;
    behavior->threat_interval = 4;
    params->wall_attribute = 0;
    work->route_point_count = 0;
    behavior->sight_angle = 90.0f;
    params->move_kind = 1;
    work->initial_motion_id = 30;
    work->locator_start_frame = 5;
    encounter->hearing_distance = 10.0f;
    params->homing_value_count = 0;
    params->locator_distance = 4.0f;
    params->body_radius = 0.35f;
    navigation->homing_point_count = 0;
    params->value_108 = 1.0f;
    work->locator_end_frame = 25;
    params->ex_motion_resource = -1;
    params->value_120 = 0;
    params->event_type = -1;
    work->scale_frame = -1;
    work->scale = 1.0f;
    for (i = 0; i < 64; i++) {
        work->route_state[i] = 0;
        for (k = 0; k < 4; k++) {
            work->route_value[i][k] = -1;
        }
    }
    navigation->homing_point_count = 0;
    navigation->encounter_side = -1;
    work->spline_parameter = 0;
    work->fast_point_count = 0;
    work->chase_frame = 0;
    work->chase_frame_limit = 0;
    loop_state = (EnemyInitActor **)GameLoopState;
    enemy->flags |= 0x10000;
    work->mark = 0;
    work->mark_timer = 0;
    enemy->flags |= 0x20000;
    enemy->flags |= 0x40000000;
    work->target_number = loop_state[1]->number;
    enemy->lookat_timer = 0;
    enemy->lookat_target = -1;
    params->default_motion_table[1] = 1;
    params->default_motion_table[2] = 3;
    params->default_motion_table[0] = 0;
    work->freeze_state = 0;
    params->default_motion_table[4] = 0;
    params->default_motion_table[5] = 0;
    params->default_motion_table[3] = 9;
    params->default_motion_table[6] = 0;
    encounter->encounter_event_flags = 0;
    work->state_37b4 = 0;
    work->turn_flags = 0;
    work->turn_lock = 0;
    work->effect_state = 0;
    params->value_2c4 = 0;
    work->encount_lock = 0;
    params->value_2c0 = 0;
    encounter->locator_flags = 0;
    if ((unsigned short)enemy->id - 0x4000 >= 0x806U) {
        encounter->locator_flags = 0x1001b;
    }
    work->turn_frame = -1;
    navigation->movement_step = -1;
    work->turn_duration = 0;
    navigation->movement_duration = 0;
    UnduParamInit(enemy->undulation);
    work->applied_light_level = 1000;
    params->gravity_mode = 0;
    enemy->update = ACT_updateEnemy;
    enemy->flags |= 0x20;
    encounter->light_level = 1000;
    for (i = 0; i < 4; i++) {
        params->value_2e0[i] = -1;
    }
    work->effect = 0;
    for (i = 0; i < 8; i++) {
        enemy->effects[i] = 0;
    }
    enemy->effect_interval = -1;
    enemy->effect_next = 0;
    DataSpline[0] = AdrsEnemySpline;
    work->light = 0;
    for (j = 0; j < 256; j++) {
        for (i = 0; i < 4; i++) {
            work->action_motion_table[j][i] = -1;
            work->motion_id_table[j][i] = 0;
        }
    }
    for (i = 0; i < 8; i++) {
        encounter->monster_set_numbers[i] = -1;
    }
    work->sense_block_timer = 0;
    work->action = 0;
    AdrsEnemy = (char *)AdrsEnemyPreset + 4;
    return enemy;
}

#include "common.h"
#include "shared.h"
#include "init_map_door.h"

extern signed char printflg;
extern const char D_004CA3B8[16];
extern const char D_004CA3C8[24];
int printf(const char *, ...);

/*
 * CheckDoorDist is called with no argument from AutoDoorStanbyFunc,
 * EventDoorStanbyFunc and MjDoorStanbyFunc (it is their first call, so $a0
 * still holds their own argument only by coincidence) and with the door
 * pointer from AutoDoorOpenNowFunc/HalfAutoDoorOpenNowFunc, which reload it
 * into $a0 after DoorOpenStanbyFunc clobbers the register. Left unprototyped
 * to admit both call shapes exactly as compiled.
 */
float CheckDoorDist();
void CheckDoorPos();
int CheckDoorSwitch(DoorUnit *door);
void DoorCommonFunc(DoorUnit *door);
void DoorOpenStanbyFunc();
void EventDoorStanbyFunc(DoorUnit *door);
void EventDoorOpenOpeFunc(DoorUnit *door);
void EventDoorOpenNowFunc(DoorUnit *door);
void AutoDoorOpenOpeFunc(DoorUnit *door);
void AutoDoorOpenNowFunc(DoorUnit *door);
void HalfAutoDoorOpenNowFunc(DoorUnit *door);
void MjDoorStanbyFunc(DoorUnit *door);
void AutoDoorStanbyFunc(DoorUnit *door);
extern DoorMapUnit MapUnit[64];
int AutoDoorCloseOpeFunc(DoorUnit *door);
void EventDoorCloseOpeFunc(DoorUnit *door);
int xglSoundEffectCheckID();
void xglSoundEffectPosID();
void xglSoundEffectStopID();

typedef struct DoorPlayerState {
    unsigned char unmodeled_00[0x10];
    Vector4 position;
    unsigned char unmodeled_20[0x34];
    float facing_angle;
} DoorPlayerState;

typedef struct DoorGameLoopState {
    unsigned char unmodeled_00[4];
    DoorPlayerState *player;
    unsigned char unmodeled_08[0x2a028];
} DoorGameLoopState;

extern DoorGameLoopState GameLoopState;
extern PadPrefix PadData;
/* The original uses these as compiler-emitted single-precision constants. */
float CheckDist2D(Vector4 *player_position, DoorPosition *door_position);
float atan2f(float y, float x);
float nearDir(float first, float second);

void GetPartsPos();
void GetPartsSize();
void MAP_updateUnitDoor(DoorUnit *door);
extern char D_004CA280[];
extern char D_004CA2B8[];
extern char D_004CA2D8[];
extern char D_004CA2F8[];
typedef struct UpDoorModel {
    float direction_x; /* +0x00 */
    unsigned char unmodeled_04[4];
    float direction_z; /* +0x08 */
    unsigned char unmodeled_0c[0x24];
    float x; /* +0x30 */
    float y; /* +0x34 */
    float z; /* +0x38 */
} UpDoorModel;
typedef struct UpDoorParams {
    unsigned char unmodeled_00[2];
    short kind; /* +0x02 */
    signed char event_signal; /* +0x04 */
    signed char linked_unit; /* +0x05 */
    signed char motion_axis; /* +0x06 */
    signed char door_type; /* +0x07 */
    short open_limit; /* +0x08 */
    unsigned char unmodeled_0a[2];
    float part_size_x; /* +0x0c */
    float part_size_y; /* +0x10 */
    float part_size_z; /* +0x14 */
    float part_offset_x; /* +0x18 */
    float part_offset_y; /* +0x1c */
    float part_offset_z; /* +0x20 */
    float trigger_distance; /* +0x24 */
    float travel_distance; /* +0x28 */
    unsigned char unmodeled_2c[8];
    int move_sound_channel; /* +0x34 */
    int close_sound_channel; /* +0x38 */
    unsigned char unmodeled_3c[8];
    unsigned int initialization_state; /* +0x44 */
} UpDoorParams;
typedef union UpDoorPosition {
    Vector4 values;
    u64 words[2];
} UpDoorPosition;
typedef struct UpDoorUnit {
    unsigned int flags; /* +0x00 */
    void (*update_func)(struct UpDoorUnit *); /* +0x04 */
    unsigned char unmodeled_008[8];
    float model_x; /* +0x10 */
    float model_y; /* +0x14 */
    float model_z; /* +0x18 */
    float model_w; /* +0x1c */
    unsigned char unmodeled_020[0x60];
    UpDoorModel *model; /* +0x80 */
    unsigned char unmodeled_084[0x1c];
    unsigned char sound_channel; /* +0xa0 */
    unsigned char unmodeled_0a1;
    signed char open_phase; /* +0xa2 */
    unsigned char unmodeled_0a3;
    short door_number; /* +0xa4 */
    unsigned char unmodeled_0a6[2];
    unsigned short open_timer; /* +0xa8 */
    short close_delay; /* +0xaa */
    unsigned char unmodeled_0ac[4];
    UpDoorPosition resting_position; /* +0xb0 */
    float x; /* +0xc0 */
    float y; /* +0xc4 */
    float z; /* +0xc8 */
    float w; /* +0xcc */
    unsigned char unmodeled_0d0[0xd0];
    UpDoorParams params; /* +0x1a0 */
} UpDoorUnit;
typedef struct UpDoorMapUnit {
    unsigned char unmodeled_000[0x10];
    UpDoorPosition model_position; /* +0x10 */
    unsigned char unmodeled_020[0x82];
    signed char open_phase; /* +0xa2 */
    unsigned char unmodeled_0a3[0x1d];
    UpDoorPosition position; /* +0xc0 */
    unsigned char unmodeled_0d0[0xd0];
    UpDoorParams params; /* +0x1a0 */
    unsigned char unmodeled_1e8[0x118];
} UpDoorMapUnit;

void InitMapDoor(DoorUnit *door_in)
{
    UpDoorUnit *door = (UpDoorUnit *)door_in;
    UpDoorParams *params = &door->params;
    int linked_index;
    signed char door_type;

    GetPartsPos(door);
    __builtin_memcpy(&door->x, &door->model_x, 16);
    GetPartsSize(door);
    if (params->travel_distance == 0.0f) {
        params->travel_distance = door->resting_position.values.x +
                                 door->resting_position.values.x;
    }
    door->update_func = (void (*)(struct UpDoorUnit *))MAP_updateUnitDoor;
    if ((unsigned char) door->open_phase != 4) {
        door->open_phase = 0;
    }
    linked_index = params->linked_unit;
    if (linked_index != -1) {
        char *mu;
        int unit_off = linked_index * 0x300;
        UpDoorParams *linked_params = (UpDoorParams *)((char *)&((UpDoorMapUnit *)MapUnit)[0].params + unit_off);
        mu = (char *)(unsigned int)MapUnit;
        linked_params->open_limit = params->open_limit;
        linked_params->part_size_x = params->part_size_x;
        linked_params->part_size_y = params->part_size_y;
        linked_params->part_size_z = params->part_size_z;
        linked_params->part_offset_x = params->part_offset_x;
        linked_params->part_offset_y = params->part_offset_y;
        linked_params->part_offset_z = params->part_offset_z;
        linked_params->travel_distance = params->travel_distance;
        linked_params->trigger_distance = params->trigger_distance;
        ((UpDoorMapUnit *)(mu + unit_off))->open_phase = 4;
        GetPartsPos(mu + unit_off);
        {
            int mp_off = unit_off + 0x10;
            int pos_off = unit_off + 0xC0;
            *(UpDoorPosition *)(mu + pos_off) = *(UpDoorPosition *)(mu + mp_off);
        }
    }
    door_type = params->door_type;
    if (door_type == 1 && CheckDoorDist((DoorUnit *)door) < params->trigger_distance) {
        params->event_signal = door_type;
        door->open_phase = 2;
    }
    if (params->event_signal == 1) {
        door->open_phase = 2;
    }
    params->initialization_state = 0;
    door->flags |= 0x80000000U;
    if (!printflg) {
        return;
    }
    printf(D_004CA280, door->door_number, params->kind,
           params->door_type, params->travel_distance);
    printf(D_004CA2B8, door->x, door->y, door->z);
    printf(D_004CA2D8, door->resting_position.values.x,
           door->resting_position.values.y, door->resting_position.values.z);
    printf(D_004CA2F8, params->travel_distance,
           params->trigger_distance, params->open_limit);
}

typedef struct UDLanes {
    float lanes[4];
} UDLanes;
typedef struct UDMapUnit {
    unsigned char unmodeled_000[0x10];
    UDLanes model_position; /* +0x10 */
    unsigned char unmodeled_020[0x60];
    DoorModel *model; /* +0x80 */
    unsigned char unmodeled_084[0x24];
    unsigned short open_timer; /* +0xa8 */
    unsigned char unmodeled_0aa[0x256];
} UDMapUnit;
void DoubleDoorFunc();

void MAP_updateUnitDoor(DoorUnit *door)
{
    extern UDMapUnit MapUnitUD[64] __asm__("MapUnit");
    UpDoorParams *params = (UpDoorParams *)&door->params;

    /* Case 5 is written before case 4: the original emitted the HalfAutoDoorFunc block first. */
    switch (params->door_type) {
    case 1:
        AutoDoorFunc(door);
        break;
    case 2:
        EventDoorFunc(door);
        break;
    case 3:
        DoubleDoorFunc(door);
        break;
    case 5:
        HalfAutoDoorFunc(door);
        break;
    case 4:
        MjDoorFunc(door);
        break;
    }
    door->model_x = door->model->x;
    door->model_y = door->model->y;
    door->model_z = door->model->z;
    if (params->linked_unit != -1) {
        int index = params->linked_unit;
        DoorModel *model;

        MapUnitUD[index].open_timer = door->open_timer;
        DoorCommonFunc((DoorUnit *) &MapUnitUD[index]);
        model = MapUnitUD[index].model;
        MapUnitUD[index].model_position.lanes[0] = model->x;
        MapUnitUD[index].model_position.lanes[1] = model->y;
        MapUnitUD[index].model_position.lanes[2] = model->z;
    }
}

void AutoDoorFunc(DoorUnit *door)
{
    switch ((unsigned char) door->open_phase) {
    case 0:
        AutoDoorStanbyFunc(door);
        return;
    case 1:
        AutoDoorOpenOpeFunc(door);
        return;
    case 2:
        AutoDoorOpenNowFunc(door);
        return;
    case 3:
        AutoDoorCloseOpeFunc(door);
        break;
    case 4:
        break;
    default:
        return;
    }
}

int EventDoorFunc(DoorUnit *door)
{
    switch ((unsigned char) door->open_phase) {
    case 0:
        EventDoorStanbyFunc(door);
        return;
    case 1:
        EventDoorOpenOpeFunc(door);
        return;
    case 2:
        EventDoorOpenNowFunc(door);
        return;
    case 3:
        EventDoorCloseOpeFunc(door);
        /* fall through */
    case 4:
        return;
    default:
        return;
    }
}

int HalfAutoDoorFunc(DoorUnit *door)
{
    switch ((unsigned char) door->open_phase) {
    case 0:
        EventDoorStanbyFunc(door);
        return;
    case 1:
        AutoDoorOpenOpeFunc(door);
        return;
    case 2:
        HalfAutoDoorOpenNowFunc(door);
        return;
    case 3:
        AutoDoorCloseOpeFunc(door);
        /* fall through */
    case 4:
        return;
    default:
        return;
    }
}

int MjDoorFunc(DoorUnit *door)
{
    switch ((unsigned char) door->open_phase) {
    case 0:
        MjDoorStanbyFunc(door);
        return;
    case 1:
        AutoDoorOpenOpeFunc(door);
        return;
    case 2:
        AutoDoorOpenNowFunc(door);
        return;
    case 3:
        AutoDoorCloseOpeFunc(door);
        /* fall through */
    case 4:
        return;
    default:
        return;
    }
}

/* MAP_updateUnitDoor's handler for a double door: nothing to do here. */
void DoubleDoorFunc(void)
{
}

/*
 * Opens when the player enters range: mirrors the door's own position into
 * its attached model, then when close enough starts the standby/opening
 * sound (restarting it first if it is already playing).
 */
void AutoDoorStanbyFunc(DoorUnit *door)
{
    int position;
    DoorParams *params;
    DoorModel *model;

    params = &door->params;
    model = door->model;
    model->x = door->x;
    model->y = door->y;
    model->z = door->z;
    if (CheckDoorDist() < params->trigger_distance) {
        door->open_phase = 1;
        if (xglSoundEffectCheckID(params->move_sound_channel, door->sound_channel + 1) != 0) {
            xglSoundEffectStopID(params->move_sound_channel, door->sound_channel + 1);
        }
        CheckDoorPos(door, &position);
        xglSoundEffectPosID(params->move_sound_channel, &position, 1, door->sound_channel + 1);
    }
}

/*
 * Advances the door's open travel counter, keeps the moving sound at the
 * door's position and applies the shared open motion (DoorCommonFunc); once
 * the travel counter reaches the configured limit, enters the open phase.
 */
void AutoDoorOpenOpeFunc(DoorUnit *door)
{
    int position;
    DoorParams *params;

    params = &door->params;
    door->open_timer = door->open_timer + 1;
    CheckDoorPos(door, &position);
    xglSoundEffectPosID(params->move_sound_channel, &position, 1, door->sound_channel + 1);
    DoorCommonFunc(door);
    if ((short) door->open_timer >= params->open_limit) {
        door->open_phase = 2;
    }
}

void AutoDoorOpenFuncSub(DoorUnit *door)
{
    signed char direction;

    direction = door->params.motion_axis;
    if (direction != 2) {
        if (direction < 3) {
            if (direction == 1) {
                door->model->x = door->model->x + door->model->direction_x;
                door->model->z = door->model->z + door->model->direction_z;
            }
        }
    } else {
        door->model->x = door->model->x - door->model->direction_x;
        door->model->z = door->model->z - door->model->direction_z;
    }
}

/*
 * Holds the door open for a short delay, then once the player leaves
 * trigger range stops the moving sound, plays the closing sound and enters
 * the closing phase.
 */
void AutoDoorOpenNowFunc(DoorUnit *door)
{
    int position[4];
    unsigned short next_close_delay;
    DoorParams *params;

    params = &door->params;
    DoorOpenStanbyFunc(door);
    door->open_timer = params->open_limit;
    /* separate load: the compiler reloads close_delay for the increment below instead of reusing the compare's value */
    next_close_delay = (unsigned short) door->close_delay;
    if (door->close_delay < 0x10) {
        door->close_delay = next_close_delay + 1;
        return;
    }
    if (params->trigger_distance < CheckDoorDist(door)) {
        if (xglSoundEffectCheckID(params->move_sound_channel, door->sound_channel + 1) != 0) {
            xglSoundEffectStopID(params->move_sound_channel, door->sound_channel + 1);
        }
        CheckDoorPos(door, position);
        xglSoundEffectPosID(params->close_sound_channel, position, 1, door->sound_channel + 1);
        door->open_phase = 3;
        door->close_delay = 0;
    }
}

int AutoDoorCloseOpeFunc(DoorUnit *door)
{
    Vector4 closing_position;
    Vector4 opening_position;
    DoorParams *params;
    unsigned int timer;

    params = &door->params;
    timer = door->open_timer;
    timer--;
    door->open_timer = timer;
    DoorCommonFunc(door);
    if (CheckDoorDist(door) < params->trigger_distance) {
        if (xglSoundEffectCheckID(params->close_sound_channel, door->sound_channel + 1) != 0) {
            xglSoundEffectStopID(params->close_sound_channel, door->sound_channel + 1);
        }
        CheckDoorPos(door, &opening_position);
        xglSoundEffectPosID(params->move_sound_channel, &opening_position, 1, door->sound_channel + 1);
        door->open_phase = 1;
    } else {
        CheckDoorPos(door, &closing_position);
        xglSoundEffectPosID(params->close_sound_channel, &closing_position, 1, door->sound_channel + 1);
        if ((short) door->open_timer <= 0) {
            door->open_phase = 0;
        }
    }
}

/*
 * Starts opening on the event signal, restarting the moving sound if it is
 * already playing and logging the door number when print mode is enabled.
 */
void EventDoorStanbyFunc(DoorUnit *door)
{
    int position;
    signed char event_signal;
    DoorParams *params;
    DoorModel *model;

    params = &door->params;
    model = door->model;
    model->x = door->x;
    model->y = door->y;
    model->z = door->z;
    event_signal = params->event_signal;
    if (event_signal == 1) {
        if (printflg != 0) {
            printf((char *) D_004CA3B8, door->door_number);
        }
        if (xglSoundEffectCheckID(params->move_sound_channel, door->sound_channel + 1) != 0) {
            xglSoundEffectStopID(params->move_sound_channel, door->sound_channel + 1);
        }
        CheckDoorPos(door, &position);
        xglSoundEffectPosID(params->move_sound_channel, &position, 1, door->sound_channel + 1);
        door->open_phase = event_signal;
    }
}

/*
 * Advances the door's open travel counter, keeps the moving sound at the
 * door's position and applies the shared open motion (DoorCommonFunc); once
 * the travel counter reaches the configured limit, enters the open phase.
 */
void EventDoorOpenOpeFunc(DoorUnit *door)
{
    int position;
    DoorParams *params;

    params = &door->params;
    door->open_timer = door->open_timer + 1;
    CheckDoorPos(door, &position);
    xglSoundEffectPosID(params->move_sound_channel, &position, 1, door->sound_channel + 1);
    DoorCommonFunc(door);
    if ((short) door->open_timer >= params->open_limit) {
        door->open_phase = 2;
    }
}

/*
 * Holds the door open at the configured travel limit until the event signal
 * clears, then logs the door number when print mode is enabled, plays the
 * closing sound and enters the closing phase.
 */
void EventDoorOpenNowFunc(DoorUnit *door)
{
    int position;
    DoorParams *params;

    params = &door->params;
    door->open_timer = params->open_limit;
    DoorOpenStanbyFunc(door);
    if (params->event_signal == 0) {
        if (printflg != 0) {
            printf((char *) D_004CA3C8, door->door_number);
        }
        door->open_phase = 3;
        door->open_timer = params->open_limit;
        if (xglSoundEffectCheckID(params->move_sound_channel, door->sound_channel + 1) != 0) {
            xglSoundEffectStopID(params->move_sound_channel, door->sound_channel + 1);
        }
        CheckDoorPos(door, &position);
        xglSoundEffectPosID(params->close_sound_channel, &position, 1, door->sound_channel + 1);
    }
}

void EventDoorCloseOpeFunc(DoorUnit *door)
{
    int position;
    DoorParams *params = &door->params;

    door->open_timer = (short) door->open_timer - 1;
    CheckDoorPos(door, &position);
    xglSoundEffectPosID(params->close_sound_channel, &position, 1, door->sound_channel + 1);
    DoorCommonFunc(door);
    if ((short) door->open_timer <= 0) {
        door->open_phase = 0;
    }
}

/*
 * Opens only once both the player is close enough and the configured switch
 * test passes, then starts the standby/opening sound as the other stanby
 * functions do.
 */
void MjDoorStanbyFunc(DoorUnit *door)
{
    int position;
    DoorParams *params;
    DoorModel *model;

    params = &door->params;
    model = door->model;
    model->x = door->x;
    model->y = door->y;
    model->z = door->z;
    if (CheckDoorDist() < params->trigger_distance && CheckDoorSwitch(door) != 0) {
        door->open_phase = 1;
        if (xglSoundEffectCheckID(params->move_sound_channel, door->sound_channel + 1) != 0) {
            xglSoundEffectStopID(params->move_sound_channel, door->sound_channel + 1);
        }
        CheckDoorPos(door, &position);
        xglSoundEffectPosID(params->move_sound_channel, &position, 1, door->sound_channel + 1);
    }
}

/*
 * Holds the door open for a short delay, arms closing once the player is
 * within trigger range, then closes with the closing sound once the player
 * leaves that range.
 */
void HalfAutoDoorOpenNowFunc(DoorUnit *door)
{
    int position[4];
    float distance;
    float trigger_distance;
    unsigned short next_close_delay;
    DoorParams *params;

    params = &door->params;
    DoorOpenStanbyFunc(door);
    door->open_timer = params->open_limit;
    /* separate load: the compiler reloads close_delay for the increment below instead of reusing the compare's value */
    next_close_delay = (unsigned short) door->close_delay;
    if (door->close_delay < 0xC) {
        door->close_delay = next_close_delay + 1;
        return;
    }
    distance = CheckDoorDist(door);
    trigger_distance = params->trigger_distance;
    if (params->event_signal == 1 && distance < trigger_distance) {
        params->event_signal = 0;
    }
    if (trigger_distance < distance && params->event_signal == 0) {
        door->close_delay = 0;
        door->open_phase = 3;
        CheckDoorPos(door, position);
        xglSoundEffectPosID(params->close_sound_channel, position, 1, door->sound_channel + 1);
    }
}

const char D_004CA3B8[16] = "id=%d DoorOpen\n";
const char D_004CA3C8[24] = "id=%d DoorClose\n";

void DoorCommonFunc(DoorUnit *door)
{
    DoorPosition resting = door->resting_position;
    DoorParams *params = &door->params;
    DoorModel *opening_model;
    DoorModel *closing_model;
    short opening_frame;
    short closing_frame;
    float opening_x_distance;
    float closing_x_distance;
    float opening_progress;
    float closing_progress;
    float opening_limit;
    float closing_limit;

    switch (params->motion_axis) {
    case 0:
        break;
    case 1:
        opening_model = door->model;
        opening_x_distance = opening_model->direction_x * params->travel_distance;
        opening_frame = (short) door->open_timer;
        opening_progress = (float) opening_frame;
        opening_limit = (float) params->open_limit;
        opening_model->x = door->x + opening_x_distance * opening_progress / opening_limit;
        opening_model->z = door->z + opening_model->direction_z * params->travel_distance * opening_progress / opening_limit;
        break;
    case 2:
        closing_model = door->model;
        closing_x_distance = closing_model->direction_x * params->travel_distance;
        closing_frame = (short) door->open_timer;
        closing_progress = (float) closing_frame;
        closing_limit = (float) params->open_limit;
        closing_model->x = door->x - closing_x_distance * closing_progress / closing_limit;
        closing_model->z = door->z - closing_model->direction_z * params->travel_distance * closing_progress / closing_limit;
        break;
    case 3:
        door->model->y = door->y + resting.values.y * (float) (short) door->open_timer / (float) params->open_limit;
        break;
    case 4:
        door->model->y = door->y - resting.values.y * (float) (short) door->open_timer / (float) params->open_limit;
        break;
    }
    door->model_x = door->model->x;
    door->model_y = door->model->y;
    door->model_z = door->model->z;
}

void DoorOpenStanbyFunc(DoorUnit *door)
{
    DoorPosition resting = door->resting_position;
    DoorParams *params = &door->params;
    DoorModel *model;

    switch (params->motion_axis) {
    case 1:
        model = door->model;
        model->x = door->x + model->direction_x * params->travel_distance;
        model->z = door->z + model->direction_z * params->travel_distance;
        break;
    case 2:
        model = door->model;
        model->x = door->x - model->direction_x * params->travel_distance;
        model->z = door->z - model->direction_z * params->travel_distance;
        break;
    case 3:
        model = door->model;
        model->y = door->y + resting.values.y;
        break;
    case 4:
        model = door->model;
        model->y = door->y - resting.values.y;
        break;
    }
}

float CheckDoorDist(DoorUnit *door)
{
    DoorPlayerState *player = GameLoopState.player;
    float distance = 65535.0f;

    if (__builtin_fabsf(player->position.y - door->y) > 2.0f) {
        return distance;
    }

    {
        DoorPosition position;
        CheckDoorPos(door, &position);
        return CheckDist2D(&player->position, &position);
    }
}

int CheckDoorSwitch(DoorUnit *door)
{
    Vector4 position;
    DoorPlayerState *player = GameLoopState.player;
    float angle;

    CheckDoorPos(door, &position);
    if ((PadData.half_2a & 0x20) != 0) {
        angle = atan2f(position.x - player->position.x, position.z - player->position.z);
        angle = nearDir(angle, player->facing_angle);
        if (__builtin_fabsf(angle) < 1.04719758f) {
            return 1;
        }
    }
    return 0;
}

void CheckDoorPos(DoorUnit *door, DoorPosition *position)
{
    signed char linked_index = door->params.linked_unit;
    DoorMapUnit *target_unit;

    if (linked_index == -1) {
        __builtin_memcpy(position, &door->x, sizeof(*position));
        return;
    }

    target_unit = &MapUnit[linked_index];
    position->values.x = door->x - (door->x - target_unit->position.x) * 0.5f;
    position->values.y = door->y;
    position->values.z = door->z - (door->z - target_unit->position.z) * 0.5f;
}

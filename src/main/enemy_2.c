#include "common.h"

#include "enemy_2.h"

int Get_ActorNumber(int target)
{
    unsigned char *base;
    int *id;
    short i;

    base = (unsigned char *)enepc;
    id = (int *)(base + ENEMY_ID_OFFSET);
    for (i = 0; i < 64; i++) {
        if (*id == target)
            return i;
        id = (int *)((unsigned char *)id + sizeof(EnemyWork));
    }
    return -1;
}

extern void ACT_setMotion2(void *actor, int a, int b);
extern float D_004D813C;

void Enemy_Command_Motion(Actor *actor, int motion, char mode, int a3, int a4, int loop, int speed)
{
    unsigned char *p = (unsigned char *)actor;
    int flags = 8;
    float scale;
    int none;
    if (mode != 1) flags = 0;
    if (loop == 0) flags |= 1;
    ACT_setMotion2(actor, motion | 0x8000, flags);
    scale = D_004D813C;
    none = -1;
    if (mode != 1) {
        *(s16 *)(p + 0x706) = 0;
        *(s16 *)(p + 0x712) = 9;
    }
    *(float *)(p + 0x6F8) = ((float)speed / 100.0f) * scale;
    if (a3 != none) *(float *)(p + 0x6FC) = (float)a3 * scale;
    if (a4 != none) *(float *)(p + 0x700) = (float)a4 * scale;
    if (a3 != none) *(float *)(p + 0x6F4) = *(float *)(p + 0x6FC);
}

void Enemy_Command_Freeze(Actor *actor, int command)
{
    unsigned char *work;

    work = (unsigned char *)(enepc + actor->number);

    if (ENEMY_FREEZE_STATE(work) == command)
        return;
    if ((unsigned char)(ENEMY_FREEZE_STATE(work) - 1) < 3 &&
        (unsigned int)(command - 1) < 3)
        return;

    ENEMY_FREEZE_STATE(work) = command;
    switch (command) {
    case 0:
        if (ENEMY_FREEZE_SAVED_SPEED(work) == ENEMY_FREEZE_NO_SAVED_SPEED)
            return;
        ACTOR_SPEED(actor) = ENEMY_FREEZE_SAVED_SPEED(work);
        return;
    case 1:
        ENEMY_FREEZE_SAVED_SPEED(work) = ENEMY_FREEZE_NO_SAVED_SPEED;
        return;
    case 2:
        ENEMY_FREEZE_SAVED_SPEED(work) = ACTOR_SPEED(actor);
        ACTOR_SPEED(actor) = 0.0f;
        return;
    case 3:
        ENEMY_FREEZE_SAVED_SPEED(work) = ACTOR_SPEED(actor);
        ACTOR_SPEED(actor) = D_004D8140;
        return;
    }
}

void Enemy_Command_Turn(Actor *actor, signed char command)
{
    unsigned char *work;

    work = (unsigned char *)(enepc + actor->number);

    switch (command) {
    case 0:
        ENEMY_TURN_LOCK(work) = 1;
        break;
    case 1:
        ENEMY_TURN_LOCK(work) = 0;
        break;
    case 2:
    {
        int current_mode;

        current_mode = ENEMY_TURN_LOCK(work);
        ENEMY_TURN_LOCK(work) = (current_mode ^ 1) != 0;
        break;
    }
    }
}

void Enemy_Command_Light(Actor *actor, signed char light)
{
    unsigned char *work;

    work = (unsigned char *)(enepc + actor->number);

    switch (light) {
    case 0:
        ENEMY_LIGHT(work) = 1;
        break;
    case 1:
        ENEMY_LIGHT(work) = 0;
        break;
    case 2:
    {
        short current_light;

        current_light = ENEMY_LIGHT(work);
        ENEMY_LIGHT(work) = (current_light ^ 1) != 0;
        break;
    }
    }
}

void Enemy_Command_Stop_FreeFall(Actor *actor, signed char command)
{
    switch (command) {
    case 0:
        ACTOR_RUNTIME_FLAGS(actor) |= 0xc0000000u;
        break;
    case 1:
        ACTOR_RUNTIME_FLAGS(actor) &= 0x3fffffffu;
        break;
    }
}

void Enemy_Command_Type(Actor *actor, signed char type)
{
    unsigned char *work;

    work = (unsigned char *)(enepc + actor->number);
    ENEMY_TYPE(work) = type;
    /* Statement-macro shape: written as a bare final call, ee-gcc 2.96 turns
     * it into a sibling jump; the original keeps jal plus the epilogue. */
    do {
        Enemy_Pause(actor);
    } while (0);
}

void Enemy_Command_Code(Actor *actor, int code)
{
    actor->command_code = code;
    /* Same statement-macro shape as Enemy_Command_Type above. */
    do {
        Enemy_Init(actor);
    } while (0);
}

void Enemy_Command_Target(Actor *actor, int target)
{
    EnemyWork *work;

    work = &enepc[actor->number];

    if (target == 100) {
        ENEMY_TARGET(work) = ((Actor *)GameLoopState[1])->number;
        return;
    }
    ENEMY_TARGET(work) = Get_ActorNumber(target);
}

int Enemy_Command_LookAt(Actor *actor, int target)
{
    extern Actor actor_table[64] __asm__("actor");
    Vector4 *pos;

    switch (target) {
    case -1:
        Actor_LookAt_Release(actor, 2);
        break;
    case 100:
        pos = (Vector4 *)((unsigned char *)GameLoopState[1] + 0x10);
        Actor_LookAt_Set(actor, 2, pos);
        *(s16 *)((unsigned char *)actor + 0x9EE) = *(unsigned char *)((unsigned char *)GameLoopState[1] + 0x80);
        break;
    default:
        pos = (Vector4 *)((unsigned char *)actor_table + Get_ActorNumber(target) * 0xA70 + 0x10);
        Actor_LookAt_Set(actor, 2, pos);
        *(s16 *)((unsigned char *)actor + 0x9EE) = Get_ActorNumber(target);
        break;
    }
}

void Enemy_Command_Scale(Actor *actor, int scale_percent, int duration)
{
    unsigned char *work;

    work = (unsigned char *)(enepc + ENEMY_ACTOR_NUMBER(actor));

    if (duration == 0) {
        float scale;
        unsigned short motion;

        scale = (float)scale_percent / 100.0f;
        ENEMY_SCALE(work)->frame = -1;
        motion = ACTOR_MOTION_NUMBER(actor);
        ENEMY_SCALE(work)->current = scale;
        ENEMY_SCALE(work)->target = scale;
        do {
            ACT_setMotion(actor, motion);
        } while (0);
    } else {
        ENEMY_SCALE(work)->duration = duration;
        ENEMY_SCALE(work)->frame = 0;
        ENEMY_SCALE(work)->start = ENEMY_SCALE(work)->current;
        ENEMY_SCALE(work)->target = (float)scale_percent / 100.0f;
    }
}

void Enemy_Command_Action(Actor *actor, int action_id, int value,
                          int argument)
{
    unsigned char *work;
    ActorAction *ext;
    short *slot;
    short *argument_slot;
    short i;

    ext = (ActorAction *)((unsigned char *)actor + ACTOR_EXT_OFFSET);
    work = (unsigned char *)(enepc + ENEMY_ACTOR_NUMBER(actor));
    slot = (short *)(work + ENEMY_ACTION_VALUE_OFFSET +
                     action_id * ENEMY_ACTION_STRIDE);

    for (i = 0; i < ENEMY_ACTION_SLOT_COUNT; i++) {
        if (*slot == -1) {
            *slot = value;
            argument_slot = (short *)((unsigned char *)slot + ENEMY_ACTION_ARGUMENT_DELTA);
            *argument_slot = argument;
            ext->frame = 0;
            ext->duration = 0;
            return;
        }
        slot++;
    }
}

void Enemy_Command_Encount(Actor *actor, signed char command)
{
    Actor *self;
    int encount_command;

    self = actor;
    encount_command = command;
    do {
        Check_Encount(self, 1, encount_command);
    } while (0);
}

void Enemy_Command_Sac_Move(Actor *actor, int mode, float x, float z)
{
    unsigned char *work;

    work = (unsigned char *)(enepc + actor->number);
    if (mode == 0) {
        *(float *)((char *)actor + 0x10) = x;
        *(float *)((char *)actor + 0x18) = z;
        *(short *)(work + 0x3820) = -1;
        return;
    }
    *(int *)(work + 0x37E0) |= 0x10000;
    *(float *)(work + 0x3800) = *(float *)((char *)actor + 0x10);
    *(short *)(work + 0x3822) = mode;
    *(float *)(work + 0x3804) = *(float *)((char *)actor + 0x14);
    *(short *)(work + 0x3820) = 0;
    *(float *)(work + 0x3808) = *(float *)((char *)actor + 0x18);
    *(float *)(work + 0x380C) = *(float *)((char *)actor + 0x1C);
    *(float *)(work + 0x3810) = x;
    *(float *)(work + 0x3814) = *(float *)((char *)actor + 0x14);
    *(float *)(work + 0x3818) = z;
    *(float *)(work + 0x381C) = *(float *)((char *)actor + 0x1C);
}

void Enemy_Command_Sac_Turn(Actor *actor, int duration,
                            float angle_degrees, float angle_mode,
                            short axis)
{
    unsigned char *work;

    work = (unsigned char *)(enepc + actor->number);

    /* `axis` selects one lane of Actor.rotation at run time, so the lane is
     * indexed from the first one instead of named. That is what the original
     * computes: it forms actor + axis * 4 once (sll v1,a2,16 / sra v1,a2,16 /
     * sll v1,a2,2 at 0x002d3408..0x002d342c) and addresses the rotation lane
     * at +0x50 of it; the instantaneous branch reuses the untouched actor for
     * the +0x9e4 target angle (0x002d34a4). A `float *rotation =
     * &actor->rotation.x;` local instead was compiled (form 01,
     * .work/attempts/style-enemy-actor-family/form01-enemy_2.c): it makes
     * ee-gcc 2.96 materialise actor + 0x50 in its own register, grows the
     * function by 8 bytes and the main link refuses it with "cannot move
     * location counter backwards (from 002d3830 to 002d3828)". */

    angle_degrees = angle_degrees / 180.0f;
    angle_degrees = angle_degrees * sac_turn_pi;

    if (angle_mode <= 0.0f) {
        if ((&actor->rotation.x)[axis] < angle_degrees) {
            angle_degrees = angle_degrees - sac_turn_two_pi_subtract;
        }
    } else {
        if (angle_degrees < (&actor->rotation.x)[axis]) {
            angle_degrees = angle_degrees + sac_turn_two_pi_add;
        }
    }

    if (duration == 0) {
        (&actor->rotation.x)[axis] = angle_degrees;
        ENEMY_TURN(work)->frame = -1;
        ACTOR_TARGET_ANGLE(actor) = angle_degrees;
    } else {
        ENEMY_TURN(work)->duration = duration;
        ENEMY_TURN(work)->frame = 0;
        ENEMY_TURN(work)->start_angle = (&actor->rotation.x)[axis];
        ENEMY_TURN(work)->target_angle = angle_degrees;
        ENEMY_TURN_FLAGS(work) |= ENEMY_TURN_REQUEST;
    }
}

void Map_Command_Ladder(int idx, unsigned char a, int b, unsigned char c)
{
  int new_var;
  unsigned char *p = ((unsigned char *) GameLoopState) + idx;
  float *fp = ((float *) GameLoopState) + idx;
  p[0x29F9F] = a;
  *((float *) (((unsigned char *) fp) + 0x29FBC)) = ((float) b) / 100.0f;
  new_var = 0x29FAF;
  (((unsigned char *) GameLoopState) + idx)[new_var] = c;
}

INCLUDE_ASM("asm/main/nonmatchings/enemy_2", Start_Enemy_Command);

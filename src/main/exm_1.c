#include "common.h"
#include "exm_1.h"

static signed char GetLastWord(signed char *text)
{
    int index;

    index = 0;
    if (text != 0) {
        if (text[0] != 0 && !(text[0] & 0x80)) {
            do {
                index++;
            } while (text[index] != 0 && !(text[index] & 0x80));
        }
        if (index != 0) {
            return text[index - 1];
        }
        return 0;
    }
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/exm_1", EXM_PresetHair);

INCLUDE_ASM("asm/main/nonmatchings/exm_1", GetHairTypeSelection);

INCLUDE_ASM("asm/main/nonmatchings/exm_1", EXM_CheckExCollision);

INCLUDE_ASM("asm/main/nonmatchings/exm_1", CheckSkirtCollisionSub);

extern int rootBorn;
void CheckSkirtCollisionSub(int arg0, int arg1, int arg2, int arg3);

static void EXM_CheckSkirtCollision(int arg0, int arg1, int arg2, int arg3)
{
    if (arg3 & 0x80) {
        rootBorn = 4;
        CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
        rootBorn = 5;
        CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
        return;
    }
    if (arg3 & 0x100) {
        rootBorn = 6;
        CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
        rootBorn = 7;
        CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
        return;
    }
    rootBorn = 0;
    CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
    rootBorn = 1;
    CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
    if (!(arg3 & 0x8000)) {
        return;
    }
    rootBorn = 2;
    CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
    rootBorn = 3;
    CheckSkirtCollisionSub(arg0, arg1, arg2, arg3);
}

static void AdjustJrCoatWidth(float *a, float *b, float width)
{
    float d[4];
    float t;
    float x, y, z;

    d[0] = a[0] - b[0];
    d[1] = a[1] - b[1];
    d[2] = a[2] - b[2];
    xglVectorLength(&d[3], (Vector4 *)d);
    if (d[3] == 0.0f) {
        d[3] = 1.0f;
    }
    t = width / d[3];
    x = (b[0] + d[0] * t) * 0.5f + a[0] * 0.5f;
    y = (b[1] + d[1] * t) * 0.5f + a[1] * 0.5f;
    z = (b[2] + d[2] * t) * 0.5f + a[2] * 0.5f;
    a[0] = x;
    a[1] = y;
    a[2] = z;
}

INCLUDE_ASM("asm/main/nonmatchings/exm_1", EXM_CalcExMotion);

void EXM_InitMovedHair(Actor *actor)
{
    if (actor != 0) {
        actor->moved_hair_mode = 2;
        actor->moved_hair_delay = 0.5f;
        actor->moved_hair_sway[0] = 0.0f;
        actor->moved_hair_sway[1] = 0.0f;
        actor->moved_hair_sway[2] = 0.0f;
        actor->moved_hair_sway[3] = 0.0f;
    }
}

void EXM_OnMovedHair(Actor *actor)
{
    if (actor != 0) {
        actor->moved_hair_mode = 2;
    }
}

void EXM_OffMovedHair(Actor *actor)
{
    if (actor != 0) {
        actor->moved_hair_mode = 0;
    }
}

void EXM_SetDelayMovedHair(Actor *actor, float delay)
{
    if (actor != 0) {
        actor->moved_hair_delay = delay;
    }
}

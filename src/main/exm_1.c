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

extern const float D_004D86D4;
extern const float D_004D86D8;
extern const float D_004D86DC;
extern const float D_004D86E0;
extern int humancol[];
extern int shelleycol[];
extern int kosmoscol[];
extern float skirtColliSize;
extern float skirtColliSizeB;
extern float skirtColliSizeS;
extern float skirtColliFootRad;

static int GetHairTypeSelection(unsigned int *flag, int **col, char *obj)
{
    int ret;
    signed char *name;

    name = *(signed char **)(obj + 0x80C);
    ret = 0;
    *flag = 0;
    if (name != 0) {
    switch (*name) {
    case 'P': {
        short type = *(short *)(obj + 0x4C2);
        *col = humancol;
        skirtColliSize = D_004D86D4;
        skirtColliSizeB = 1.0f;
        skirtColliSizeS = 1.0f;
        skirtColliFootRad = D_004D86D8;
        switch (type) {
        case 0x4A:
            ret = 0x10;
            *flag = 0x4400;
            break;
        case 0x4F:
            ret = 0x10;
            *flag = 0xC00;
            break;
        case 0x54:
            goto ret8;
        case 0x57:
        case 0x5B:
            goto ret5;
        case 0x5F:
        case 0x64:
            ret = 7;
            break;
        }
        break;
    }
    case 'S': {
        short type = *(short *)(obj + 0x4C2);
        *col = shelleycol;
        skirtColliSize = 1.0f;
        skirtColliSizeB = 1.0f;
        skirtColliSizeS = 1.0f;
        skirtColliFootRad = D_004D86DC;
        switch (type) {
        case 0x4A:
            ret = 0x10;
            *flag = 0x4400;
            break;
        case 0x4D:
            ret = 0x10;
            *flag = 0xC00;
            break;
        case 0x50:
            ret = 2;
            *flag = 1;
            break;
        case 0x56:
        case 0x5A:
            goto ret5;
        case 0x5E:
        case 0x65:
            goto ret4;
        }
        break;
    }
    case 'B': {
        short type = *(short *)(obj + 0x4C2);
        skirtColliSize = 1.0f;
        skirtColliSizeB = 1.0f;
        skirtColliSizeS = 1.0f;
        skirtColliFootRad = D_004D86E0;
        switch (type) {
        case 0x71:
            ret = 0x10;
            *flag = 0x5500;
            break;
        case 0x75:
            ret = 0x10;
            *flag = 0x4500;
            break;
        case 0x79:
            ret = 0x10;
            *flag = 0x6500;
            break;
        case 0x7D:
            ret = 0x10;
            *flag = 0x11D00;
            break;
        case 0x80:
            ret = 0x10;
            *flag = 0x12D00;
            break;
        case 0x83:
        case 0x87:
        case 0x8B:
        case 0x96:
        case 0x9B:
        ret4:
            ret = 4;
            break;
        case 0x8E:
        case 0x92:
            goto ret5;
        }
        break;
    }
    case 'K': {
        short type = *(short *)(obj + 0x4C2);
        *col = kosmoscol;
        switch (type) {
        case 0x4A:
        case 0x4D:
        case 0x50:
        case 0x53:
            ret = 0xC;
            break;
        case 0x56:
            ret = 3;
            *flag |= 1;
            break;
        case 0x5C:
        case 0x5F:
        ret5:
            ret = 5;
            break;
        case 0x62:
        case 0x67:
            ret = 4;
            *flag |= 2;
            break;
        case 0x6C:
        ret8:
            ret = 8;
            break;
        }
        break;
    }
    }
    }
    return ret;
}

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

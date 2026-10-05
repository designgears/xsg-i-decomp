#include "common.h"

struct LadderAttributeTable {
    int attributes[16];
};

struct ArrivalAttributeTable {
    int attributes[4];
};

extern const struct LadderAttributeTable D_004CAC20;
extern const struct ArrivalAttributeTable D_004CAC60;

short Get_Ladder(int attribute)
{
    struct LadderAttributeTable table = D_004CAC20;
    short index = 0;

    do {
        if (attribute == table.attributes[index]) {
            return index;
        }

        index++;
    } while (index < 16);

    return -1;
}

short Get_Arrival(int attribute)
{
    struct ArrivalAttributeTable table = D_004CAC60;
    short index = 0;

    do {
        if (attribute == table.attributes[index]) {
            return index;
        }

        index++;
    } while (index < 4);

    return -1;
}

extern int Get_Attr_NU(const float *position, int mapIndex, int attrMask);

void Get_LadderID_Limit(const float *start, float *position,
                        float xStep, float zStep)
{
    short index = 0;
    int found = 0;
    int attribute;

    position[0] = start[0];
    position[1] = start[1];
    position[2] = start[2];
    position[3] = start[3];

    for (;;) {
        if (index >= 1024) {
            break;
        }

        attribute = Get_Attr_NU(position, 0, 0);

        if (Get_Ladder(attribute) != -1) {
            found = 1;
        }

        if (Get_Ladder(attribute) == -1 && found == 1) {
            break;
        }

        position[0] += xStep;
        position[2] += zStep;
        index++;
    }

    if (index == 1024) {
        position[0] = start[0];
        position[2] = start[2];
    } else {
        position[0] -= xStep;
        position[2] -= zStep;
    }
}

extern int Get_Attr_NU(const float *position, int mapIndex, int attrMask);

int Get_ArrivalID_First(const float *start, float *position,
                        float xStep, float zStep)
{
    short index = 0;
    int attribute;

    position[0] = start[0];
    position[1] = start[1];
    position[2] = start[2];
    position[3] = start[3];

    for (;;) {
        if (index >= 256) {
            break;
        }

        attribute = Get_Attr_NU(position, 0, 0);

        if (Get_Arrival(attribute) != -1) {
            break;
        }

        position[0] += xStep;
        position[2] += zStep;
        index++;
    }

    return index != 256;
}

typedef struct {
    char pad0[0x3800];
    float p0[4];
    float p1[4];
    short index;
    short count;
    char pad3824[0xC];
    short flag;
    char pad3832[0x38B0 - 0x3832];
} EnePc;

typedef struct {
    char pad0[4];
    char *player;
    char pad8[0xC1 - 8];
    unsigned char flagsC1;
    char padC2[0x100 - 0xC2];
} GameLoopStateT;

extern EnePc enepc[];

extern GameLoopStateT GameLoopState;

extern void ACT_setMotion2(void *actor, int a, int b);

typedef struct { float x, y, z, w; } __attribute__((aligned(16))) FlyV4;
extern float D_004D8068;
extern float D_004D806C;
extern float AdjTable_X[4];
extern float AdjTable_Z[4];
extern float LadderStep[];
extern short NowLadderStep;
extern void Get_MiddlePoint_Parabora(float *a, float *b, int index, int count, float t, float *out);
extern float Get_Height(FlyV4 *pos, int map, int attr);

void Fly_to_Ladder(char *actor)
{
    EnePc *e = &enepc[*(unsigned char *)(actor + 0x80)];
    FlyV4 pos;
    float h;
    float y;
    short i;
    signed char dir;

    short f = e->flag;

    if (f == 1) {
        e->index++;
        if (e->index < 13) {
            return;
        }
        e->index = 0;
        *(float *)(actor + 0x9E4) = *(float *)((char *)e + 0x3850);
        if (*((signed char *)e + 0x384D) == 0) {
            int m = 0x15;
            signed char d = *((signed char *)&GameLoopState + 0x29FA0 + *((signed char *)e + 0x384C));
            if (d == 0 && e->p1[0] < e->p0[0]) {
                m = 0x13;
            } else if (d == f && e->p0[1] < e->p1[1]) {
                m = 0x13;
            } else if (d == 3 && e->p0[1] < e->p1[1]) {
                m = 0x13;
            }
            ACT_setMotion2(actor, m, 8);
        } else {
            ACT_setMotion2(actor, 0x1C, 8);
        }
        e->flag = 3;
    }
    Get_MiddlePoint_Parabora(e->p0, e->p1, e->index, e->count, 0.0f, (float *)(actor + 0x10));
    if (e->index++ >= e->count) {
        e->index = -1;
        e->flag = 4;
        *(short *)((char *)e + 0x384A) = 0x14;
        ACT_setMotion2(actor, 0x14, 8);
        *(float *)(actor + 0x6F8) = D_004D8068;
        pos = *(FlyV4 *)(actor + 0x10);
        dir = *((signed char *)&GameLoopState + 0x29FA0 + *((signed char *)e + 0x384C));
        pos.x -= AdjTable_X[dir];
        pos.z -= AdjTable_Z[dir];
        h = Get_Height(&pos, 0, 0);
        y = *(float *)(actor + 0x14);
        NowLadderStep = 0x3F;
        if (y < h || *((signed char *)e + 0x384D) == 1) {
            *(float *)(actor + 0x14) = h;
            y = h;
        }
        LadderStep[0x3F] = y;
        for (i = 1; i < 0x3F; i++) {
            float c = *(float *)(actor + 0x14);
            LadderStep[0x3F + i] = c + i * D_004D806C;
            LadderStep[0x3F - i] = c - i * D_004D806C;
        }
    }
}

extern float D_004D8070;
extern void Get_MiddlePoint(float *a, float *b, int index, int count, float *out);
void Get_Off_Ladder(char *actor)
{
    EnePc *e = &enepc[*(unsigned char *)(actor + 0x80)];

    e->index++;
    Get_MiddlePoint(e->p0, e->p1, e->index, e->count, (float *)(actor + 0x10));
    if (e->index >= e->count) {
        e->flag = 0;
        e->index = -1;
        ACT_setMotion2(actor, 0, 8);
        *(unsigned short *)(GameLoopState.player + 0x4D0) |= 0x1000;
        GameLoopState.flagsC1 |= 0x20;
    }
    *(float *)(actor + 0x6F8) = D_004D8070;
}

INCLUDE_ASM("asm/main/nonmatchings/get_ladder", Ready_Ladder);

INCLUDE_ASM("asm/main/nonmatchings/get_ladder", On_Ladder);

INCLUDE_ASM("asm/main/nonmatchings/get_ladder", Ladder_Main);

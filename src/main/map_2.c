#include "common.h"
#include "map_2.h"

extern MapUnitResource *RES_loadFile(int command, int callback, int resource_id, int flags);
extern void LOG(const char *format, ...);
extern int MDL_create(void *model_instance, void *resource_model);

int MAP_loadUnitResource(MapUnitSlot *unit, int resource_id)
{
    MapUnitResource *resource = RES_loadFile(-1, 4, resource_id, 0);
    void *model;
    int status;

    if (resource == 0) {
        resource = RES_loadFile(-1, 4, resource_id & 0x7F00, 0);
    }

    if (resource != 0) {
        model = resource->model;
        status = resource->status;
    } else {
        model = 0;
        status = 0;
    }

    if (model == 0) {
        /* Report the missing model resource. */
        LOG("\245\342\245\307\245\353\244\254\270\253\244\304\244\253\244\352\244\336\244\273\244\363\244\307\244\267\244\277\241\243\n");
    }

    unit->resource_status = status;
    unit->resource_model = model;
    unit->model = model;
    return MDL_create(&unit->model_state, model);
}

typedef struct MapUnitInitView {
    int flags;          /* 0x00 */
    void *update;       /* 0x04 */
    int unk8;           /* 0x08 */
    void *typeUpdate;   /* 0x0C */
    int unk10;
    int unk14;
    int unk18;
    float unk1C;
    char pad20[0x10];
    float unk30;
    float unk34;
    float unk38;
    float unk3C;
    char pad40[0x64];
    short serial;       /* 0xA4 */
    short unkA6;
    char padA8[0x28];
    int unkD0;
    void *resource_model;
    float unkD8[2];
    void *model;
    int resource_status;
    float unkE8[3];
    char padF4[8];
    int unkFC;
    char pad100[0x120];
    int unk220;
    int unk224;
    int unk228;
    float unk22C;
    int pad230;
    int unk234;
    int unk238;
    float unk23C;
    void *model_state;
    char pad244[0x9C];
    int unk2E0;
    int unk2E4;
    int unk2E8;
    int unk2EC;
    char pad2F0[0x10];
} MapUnitInitView;

void MAP_initUnit(void)
{
    MapUnitInitView *unit = (MapUnitInitView *)MapUnit;
    int i;
    int j;
    float *w;
    char *p8;

    for (i = 0; i < 0x40; i++, unit++) {
        unit->flags = 0;
        unit->serial = -1;
        unit->update = 0;
        unit->unk8 = 0;
        p8 = (char *)&unit->unk8;
        unit->typeUpdate = 0;
        unit->resource_model = 0;
        unit->unkD0 = 0;
        unit->unk30 = 1.0f;
        unit->unk34 = 1.0f;
        unit->unk38 = 1.0f;
        unit->unk3C = 1.0f;
        unit->unk10 = 0;
        unit->unk14 = 0;
        unit->unk18 = 0;
        unit->unk1C = 1.0f;
        unit->unk220 = 0;
        unit->unk224 = 0;
        unit->unk228 = 0;
        unit->unk22C = 1.0f;
        unit->unk2E4 = 0;
        unit->unk2E8 = 0;
        unit->unk234 = 0;
        unit->unk238 = 0x10;
        unit->unk2E0 = 0;
        unit->unk23C = 0.5f;
        unit->unk2EC = 0;
        unit->unkA6 = 0;
        j = 1; w = &unit->unkD8[1];
        goto L1;
        while (j >= 0) {
        L1:
            *w-- = 0;
            j--;
        }
        j = 1; w = (float *)&unit->resource_status;
        goto L2;
        while (j >= 0) {
        L2:
            *w-- = 0;
            j--;
        }
        w = (float *)(p8 + 0xE8); j = 2;
        goto L3;
        while (j >= 0) {
        L3:
            *w-- = 0;
            j--;
        }
        unit->unkFC = 0;
        unit->model_state = 0;
    }
}

void unit6003_update(MapUnitSlot *unit);

MapUnitSlot *MAP_createUnit(int index, int type)
{
    MapUnitSlot *unit;
    unsigned char *p;
    int i;
    int *w;
    void (*fn)(MapUnitSlot *);

    if (index < 0) {
        for (i = 0; i < 64; i++) {
            if (MapUnit[i].serial < 0) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            return 0;
        }
    }
    unit = &MapUnit[index];
    p = (unsigned char *)unit;
    p[0xA0] = index;
    unit->serial = type;
    *(float *)(p + 0x30) = 1.0f;
    *(float *)(p + 0x34) = 1.0f;
    *(float *)(p + 0x38) = 1.0f;
    *(float *)(p + 0x3C) = 1.0f;
    *(float *)(p + 0x22C) = 1.0f;
    *(int *)(p + 0x220) = 0;
    *(int *)(p + 0x224) = 0;
    *(int *)(p + 0x228) = 0;
    *(int *)(p + 0xD0) = 0;
    fn = type == 0x6025 ? unit6003_update : 0;
    unit->update = 0;
    *(int *)(p + 0x8) = 0;
    p[0xA2] = 0;
    unit->typeUpdate = fn;
    w = (int *)(p + 0xF0);
    for (i = 2; i >= 0; i--) {
        *w = 0;
        w--;
    }
    return unit;
}

#define NULL ((void *)0)

/*
 * Bit 0x10 of a unit's flags, tested for every MapUnit[] entry, skips both
 * per-frame callbacks below when set; the bit's own meaning is not
 * otherwise evidenced in this TU.
 */
void MAP_updateUnit(void)
{
    MapUnitSlot *unit = MapUnit;
    int i;

    for (i = 0; i < 64; i++) {
        if (unit->serial >= 0 && !(unit->flags & 0x10)) {
            if (unit->update != NULL) {
                unit->update(unit);
            }
            if (unit->typeUpdate != NULL) {
                unit->typeUpdate(unit);
            }
        }
        unit++;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/map_2", MAP_drawUnitAt);

void MAP_drawUnit(void)
{
    MapUnitSlot *unit = MapUnit;
    int i;

    for (i = 0; i < 64; i++) {
        if (unit->serial >= 0) {
            MAP_drawUnitAt(unit);
        }
        unit++;
    }
}

void MAP_getHeight(MapPosition *position)
{
    float height = UnduGet(position->x, position->z);
    int found = 1;

    if (height == -1000.0f) {
        found = 0;
    }
    if (found) {
        position->y = height;
    }
}

/*
 * OV12 original TU 30: 0x00a1eea0..0x00a1f7e0 (11 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_equip_type.h"

extern void assert_prog(const char *expression, const char *source_file,
                        int line);

extern const char D_00A53E98[]; /* "pEquip != NIL" */
extern const char D_00A53E80[]; /* "../rg_equip_type.euc.c" */
extern const char D_00A53E10[];
extern const char D_00A53EC8[];
extern const char D_00A53F90[]; /* "left" */
extern const char D_00A53F98[]; /* "right" */
extern const char D_00A53FA0[]; /* "back" */
extern const char D_00A53FA8[];
extern const int s_aePosToJntID[];

#define RG_EQUIP_TYPE_COUNT 3

int RgEquipPosToJntID(int ePos)
{
    if ((unsigned int)ePos + 1u >= 9u) {
        assert_prog(D_00A53E10, D_00A53E80, 50);
    }
    if (ePos == -1) {
        return -1;
    }
    return s_aePosToJntID[ePos];
}

void InitRgEquip(RgEquipRecord *pEquip)
{
    if (pEquip == 0) {
        assert_prog(D_00A53E98, D_00A53E80, 0x3D);
    }
    pEquip->mountCount = 0;
    pEquip->activeMount = 0;
}

INCLUDE_ASM("asm/nonmatchings/ov12/rg_equip_type", CopyRgEquip);

INCLUDE_ASM("asm/nonmatchings/ov12/rg_equip_type", RgEquipAddMount);

INCLUDE_ASM("asm/nonmatchings/ov12/rg_equip_type", RgEquipSetDefault);

int RgEquipIsEquipable(RgEquipRecord *pEquip, int eType)
{
    int mountIndex;

    if (pEquip == 0) {
        assert_prog(D_00A53E98, D_00A53E80, 131);
    }
    for (mountIndex = 0; mountIndex < pEquip->mountCount; mountIndex++) {
        if (pEquip->mounts[mountIndex].type == eType) {
            return 1;
        }
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov12/rg_equip_type", RgEquipCheckConfrict);

const char *RgEquipTypeString(int eType)
{
    if ((unsigned int)eType >= RG_EQUIP_TYPE_COUNT) {
        assert_prog(D_00A53EC8, D_00A53E80, 276);
    }
    switch (eType) {
    case 0:
        return D_00A53F90;
    case 1:
        return D_00A53F98;
    case 2:
        return D_00A53FA0;
    default:
        return D_00A53FA8;
    }
}

static void *_GetMountData(RgEquipRecord *pEquip, int eType)
{
    unsigned int i;

    if (pEquip == 0) {
        assert_prog(D_00A53E98, D_00A53E80, 296);
    }
    for (i = 0; i < pEquip->mountCount; i++) {
        if (pEquip->mounts[i].type == eType) {
            return &pEquip->mounts[i];
        }
    }
    return 0;
}

extern const char D_00A53FB0[];
extern const char D_00A53FB8[];
extern const char D_00A53FC0[];
extern int strcmp(const char *, const char *);

int RgEquipMountPos(RgEquipRecord *pEquip, int eType, int kind, const char *name)
{
    int pos;
    void *mount;

    if ((unsigned int)eType >= 3) {
        assert_prog(D_00A53EC8, D_00A53E80, 0x137);
    }
    pos = -1;
    mount = _GetMountData(pEquip, eType);
    if (mount != 0) {
        pos = *(int *)((char *)mount + 4);
    }
    if (kind == 4 && name != 0) {
        if (strcmp(name, D_00A53FB0) == 0 || strcmp(name, D_00A53FB8) == 0) {
            pos = 7;
        } else if (strcmp(name, D_00A53FC0) == 0) {
            pos = 7;
        }
    }
    return pos;
}

INCLUDE_ASM("asm/nonmatchings/ov12/rg_equip_type", RgEquipMountPosString);

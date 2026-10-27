/*
 * OV12 original TU 30: 0x00a1eea0..0x00a1f7e0 (11 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_equip_type.h"

extern void assert_prog(const char *expression, const char *source_file,
                        int line);

static int s_aePosToJntID[8] = {0, 2, 4, 1, 3, 5, 6, 7};
const char D_00A53E10[] = "(RG_EQUIP_POS_LEFT_HAND <= (ePos) && (ePos) <= RG_EQUIP_POS_BACK_RIGHT) || (ePos) == RG_EQUIP_POS_INVALID";
const char D_00A53E80[] = "../rg_equip_type.euc.c";
const char D_00A53E98[] = "pEquip != NIL";
const char D_00A53EC8[] = "RG_EQUIP_TYPE_MIN <= (eType) && (eType) < RG_EQUIP_TYPE_NUM";

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

typedef RgEquipTypeField RgEquipTypeView;

typedef RgEquipPosField RgEquipPosView;

typedef struct RgEquipMountView {
    RgEquipTypeView type;
    RgEquipPosView pos;
    int mask;
} RgEquipMountView;

typedef struct RgEquipRecordView {
    RgEquipMountView mounts[RG_EQUIP_MOUNT_MAX];
    int mountCount;
} RgEquipRecordView;

extern const char D_00A53EA8[];

void CopyRgEquip(RgEquipRecord *pDst, RgEquipRecord *pSrc)
{
    int i;

    if ((pDst == 0) || (pSrc == 0)) {
        assert_prog(D_00A53EA8, D_00A53E80, 0x45);
    }
    pDst->mountCount = pSrc->mountCount;
    for (i = 0; i < pSrc->mountCount; i++) {
        pDst->mounts[i].type = pSrc->mounts[i].type;
        pDst->mounts[i].pos = pSrc->mounts[i].pos;
        pDst->mounts[i].mask = pSrc->mounts[i].mask;
    }
}

extern const char D_00A53F08[];
int RgEquipAddMount(RgEquipRecord *pEquip, int eType, int nMount, int mask)
{
    unsigned int n;

    if (pEquip == 0) {
        assert_prog(D_00A53E98, D_00A53E80, 0x55);
    }
    if ((unsigned int)eType >= 3) {
        assert_prog(D_00A53EC8, D_00A53E80, 0x56);
    }
    if ((unsigned int)nMount + 1 >= 9) {
        assert_prog(D_00A53E10, D_00A53E80, 0x58);
    }
    if (pEquip->mountCount >= RG_EQUIP_MOUNT_MAX) {
        assert_prog(D_00A53F08, D_00A53E80, 0x59);
    }
    n = ((RgEquipRecordView *)pEquip)->mountCount++;
    ((RgEquipRecordView *)pEquip)->mounts[n].type = eType;
    ((RgEquipRecordView *)pEquip)->mounts[n].pos = nMount;
    ((RgEquipRecordView *)pEquip)->mounts[n].mask = mask;
}

extern int s_aePosToType[8];
const char D_00A53F38[] = "nMount >= 0";
int RgEquipAddMount(RgEquipRecord *pEquip, int eType, int nMount, int mask);

void RgEquipSetDefault(RgEquipRecord *pEquip, int arg1)
{
    int eType;
    int flags;
    int mask;
    int bit;
    int i;
    int nMount;
    int k;

    if (pEquip == 0) {
        assert_prog(D_00A53E98, D_00A53E80, 0x64);
    }
    pEquip->mountCount = 0;
    eType = 0;
    do {
        flags = 0;
        for (i = 0; i < 8; i++) {
            if (s_aePosToType[i] == eType) {
                flags |= 1 << i;
            }
        }
        mask = flags & arg1;
        nMount = -1;
        if (mask != 0) {
            for (k = 0; k < 8; k++) {
                bit = 1 << k;
                if (mask & bit) {
                    nMount = k;
                }
            }
            if (nMount < 0) {
                assert_prog(D_00A53F38, D_00A53E80, 0x76);
            }
            RgEquipAddMount(pEquip, eType, nMount, mask);
        }
        eType++;
    } while (eType < 3);
}

const char D_00A53F90[] = "left";
const char D_00A53F98[] = "right";
const char D_00A53FA0[] = "back";
const char D_00A53FA8[] = "???";

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

extern const char D_00A53F48[];
extern const char D_00A53F58[];

int RgEquipCheckConfrict(RgEquipRecord **apEquip, unsigned int eType)
{
    int sp[RG_EQUIP_TYPE_COUNT];
    int conflict;
    int *m;
    unsigned int j;
    unsigned int off;
    RgEquipRecord **pe;
    unsigned int i;

    if (apEquip == 0) {
        assert_prog(D_00A53F48, D_00A53E80, 0x95);
    }
    if (eType >= 3) {
        assert_prog(D_00A53F58, D_00A53E80, 0x96);
    }
    if (apEquip[eType] == 0) {
        return 0;
    }
    i = 0;
    pe = apEquip;
    while (i < 3) {
        sp[i] = 0;
        if (*pe != 0) {
            j = 0;
            if (((RgEquipRecordView *)*pe)->mountCount != 0) {
                off = 0;
                do {
                    m = (int *)((char *)*pe + off);
                    off += 12;
                    if (m[0] == i) {
                        sp[i] |= m[2];
                    }
                    j++;
                } while (j < (unsigned int)((RgEquipRecordView *)*pe)->mountCount);
            }
        }
        pe++;
        i++;
    }
    conflict = 0;
    for (i = 0; i < 3; i++) {
        if (i != eType && (sp[i] & sp[eType])) {
            conflict |= 1 << i;
        }
    }
    if (apEquip[eType] != 0 && apEquip[eType]->activeMount != 0) {
        conflict |= 1 << eType;
    }
    return conflict;
}

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

const char D_00A53FC8[] = "left hand";
const char D_00A53FD8[] = "left elbow";
const char D_00A53FE8[] = "left shoulder";
const char D_00A53FF8[] = "right hand";
const char D_00A54008[] = "right elbow";
const char D_00A54018[] = "right shoulder";
const char D_00A54028[] = "back left";
const char D_00A54038[] = "back right";

const char *RgEquipMountPosString(RgEquipRecord *pEquip, int eType, int kind, const char *name)
{
    int pos;

    if ((unsigned int)eType >= 3) {
        assert_prog(D_00A53EC8, D_00A53E80, 0x154);
    }
    if (eType == 2) {
        return D_00A53FA0;
    }
    pos = RgEquipMountPos(pEquip, eType, kind, name);
    switch (pos) {
    case 0:
        return D_00A53FC8;
    case 1:
        return D_00A53FD8;
    case 2:
        return D_00A53FE8;
    case 3:
        return D_00A53FF8;
    case 4:
        return D_00A54008;
    case 5:
        return D_00A54018;
    case 6:
        return D_00A54028;
    case 7:
        return D_00A54038;
    default:
        if (eType == 0) {
            return D_00A53F90;
        } else {
            if (eType == 1) {
                return D_00A53F98;
            }
            return RgEquipTypeString(eType);
        }
    }
}

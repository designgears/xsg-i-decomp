/*
 * OV01 original TU 5: 0x00a191c0..0x00a1d348 (93 functions)
 */
#include "common.h"
#include "data_unit_org_get.h"
#include "ov01/calc.h"

/*
 * orgData is a 33-entry table of 0x180-byte unit-origin records; callers
 * pass a one-based index, so dataUnitOrgGet returns orgData[id - 1].
 */
typedef struct UnitOrgRecord {
    unsigned char unmodeled_0[0x180];
} UnitOrgRecord;

extern const char D_00A456A8[];
extern UnitOrgRecord orgData[];

void *dataUnitOrgGet(int unitOrgId)
{
    if (unitOrgId >= 0x21) {
        printf(D_00A456A8, unitOrgId);
        return 0;
    }
    return &orgData[unitOrgId - 1];
}

extern const char D_00A456C8[];
extern unsigned char plChaData[];

PlCharacter *dataPlChaGet(int cid)
{
    unsigned char *base;

    if (cid >= 0x21) {
        printf(D_00A456C8, cid);
        return 0;
    }
    base = plChaData - 0xA8;
    return (PlCharacter *)(base + cid * 0xA8);
}

/*
 * D_426F48, D_426F50, D_426F54 and D_426F58 are byte offsets, from their own
 * loaded battle-data table's start (8, 0x10, 0x14 and 0x18 bytes before each
 * field respectively), the same idiom D_426F78 uses below for
 * dataExpTblGet; each points to a table of fixed-size records that
 * dataUnitInitGet, dataNormInitGet, dataSpecInitGet and dataEtherInitGet
 * index with a one-based id, as dataUnitOrgGet does orgData.
 */
extern int D_426F48;
extern int D_426F50;
extern int D_426F54;
extern int D_426F58;

void *dataUnitInitGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F48 - 8;
    value = D_426F48;
    return value + base + id * 0x34 - 0x34;
}

void *dataNormInitGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F50 - 0x10;
    value = D_426F50;
    return value + base + id * 0xC - 0xC;
}

void *dataSpecInitGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F54 - 0x14;
    value = D_426F54;
    return value + base + id * 0xC - 0xC;
}

void *dataEtherInitGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F58 - 0x18;
    value = D_426F58;
    return value + base + id * 0x18 - 0x18;
}

/*
 * D_426F78 is a selector field 0x38 bytes into a loaded battle-data table;
 * dataExpTblGet returns the table entry that selector picks out. Splat has
 * not named the table's own base, only this field.
 */
extern int D_426F78;

unsigned char *dataExpTblGet(void)
{
    return (unsigned char *)&D_426F78 - 0x38 + D_426F78;
}

/*
 * D_426F7C, D_426F80 and D_426F84 are the same kind of loaded-table byte
 * offset as D_426F48 above (0x3C, 0x40 and 0x44 bytes before their own table
 * start); dataParaTblGet, dataSpecTblGet and dataDefEquipGet each index a
 * table of 0x20-byte records with a one-based id.
 */
extern int D_426F7C;
extern int D_426F80;
extern int D_426F84;

void *dataParaTblGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F7C - 0x3C;
    value = D_426F7C;
    return value + base + id * 0x20 - 0x20;
}

void *dataDefEquipGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F84 - 0x44;
    value = D_426F84;
    return value + base + id * 0x20 - 0x20;
}

void *dataSpecTblGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F80 - 0x40;
    value = D_426F80;
    return value + base + id * 0x20 - 0x20;
}

extern void dataEtherLearnSet(int etherType, int techniqueId, int selector);
extern const char D_00A456E8[];
extern CalcUnitParam *calcUPGet(ObjectTask *unit);
extern void *dataUnitFileGet(ObjectTask *unit, int charaId);
extern int dataUnitFileLoadMot(ObjectTask *unit, int motionId, int slot, void *fileInfo);

extern void dataSpecLearnSet(int cid, int specialId);

void dataEtherTecSet(int selector)
{
    int etherType = 0;
    int techniqueId = 0;
    int specialId = 0;

    switch (selector) {
    case 1:
        etherType = 3;
        techniqueId = 13;
        break;
    case 2:
        etherType = 3;
        techniqueId = 14;
        break;
    case 3:
        etherType = 3;
        techniqueId = 15;
        break;
    case 4:
        etherType = 3;
        techniqueId = 16;
        break;
    case 5:
        etherType = 6;
        techniqueId = 43;
        specialId = 90;
        break;
    case 6:
        etherType = 6;
        techniqueId = 46;
        specialId = 89;
        break;
    case 7:
        etherType = 7;
        techniqueId = 57;
        specialId = 97;
        break;
    case 8:
        etherType = 6;
        specialId = 88;
        break;
    default:
        printf(D_00A456E8, selector);
        return;
    }

    if (techniqueId != 0) {
        dataEtherLearnSet(etherType, techniqueId, selector);
    }
    if (specialId != 0) {
        dataSpecLearnSet(etherType, specialId);
    }
}

extern const char D_00A45730[];

void dataEtherLearnSet(int cid, int id, int selector)
{
    PlCharacter *pl;
    int b;

    if (cid > 16) {
        printf(D_00A45730, cid);
        return;
    }
    if (id != 0) {
        pl = dataPlChaGet(cid);
        b = id - 1;
        *(unsigned char *)((b / 8) + (int)pl + 0x28) |= (1 << (b % 8));
    }
}

extern const char D_00A45750[];

int dataEtherLearnGet(int cid, int id)
{
    PlCharacter *pl;
    int b;

    if (cid > 16) {
        printf(D_00A45750, cid);
        return 0;
    }
    pl = dataPlChaGet(cid);
    b = id - 1;
    return *(signed char *)((b / 8) + (int)pl + 0x28) & (1 << (b % 8));
}

extern const char D_00A45770[];

void dataSkillLearnSet(int cid, int id)
{
    PlCharacter *pl;
    int b;

    if (cid > 16) {
        printf(D_00A45770, cid);
        return;
    }
    if (id != 0) {
        pl = dataPlChaGet(cid);
        b = id - 1;
        *(unsigned char *)((b / 8) + (int)pl + 0x38) |= (1 << (b % 8));
    }
}

extern const char D_00A45790[];
extern PlCharacter *dataPlChaGet(int cid);

int dataSkillLearnGet(int cid, int id)
{
    PlCharacter *pl;
    int b;

    if (cid > 16) {
        printf(D_00A45790, cid);
        return 0;
    }
    pl = dataPlChaGet(cid);
    b = id - 1;
    return *(signed char *)((b / 8) + (int)pl + 0x38) & (1 << (b % 8));
}

typedef struct ThinkMapTable {
    unsigned char unmodeled_000[0x18C];
    int specBase[0x11]; /* +0x18C */
} ThinkMapTable;

extern ThinkMapTable thinkMapTbl;

int dataSpecBaseGet(int cid)
{
    return thinkMapTbl.specBase[cid];
}

/*
 * specTbl's tail is the character-id-indexed backup special-technique base
 * table dataBakpBaseGet reads.
 */
typedef struct SpecTable {
    unsigned char unmodeled_00[0x3C];
    int bakpBase[1]; /* +0x3C, indexed dynamically */
} SpecTable;

extern SpecTable specTbl;

int dataBakpBaseGet(int cid)
{
    return specTbl.bakpBase[cid];
}

void dataSpecLearnSet(int cid, int specialId)
{
    PlCharacter *pl;

    if (cid >= 0x11) {
        printf(D_00A457B0, cid);
        return;
    }
    if (specialId != 0) {
        pl = dataPlChaGet(cid);
        pl->special[specialId - dataSpecBaseGet(cid)].id = specialId;
    }
}

/*
 * thinkMapTbl's tail is a per-character-id table of special-technique base
 * ids; dataSpecLearnGet subtracts the caller's entry from a learned special
 * id before indexing PlCharacter.special, the same subtraction
 * dataSpecLearnSet performs through dataSpecBaseGet (still asm).
 */
extern const char D_00A457D0[];

short dataSpecLearnGet(int cid, int specialId)
{
    short learned;

    if (cid >= 0x11) {
        printf(D_00A457D0, cid);
        return 0;
    }
    learned = 0;
    if (specialId != 0) {
        learned = dataPlChaGet(cid)->special[specialId - thinkMapTbl.specBase[cid]].id;
    }
    return learned;
}

void *dataSpecDataGet(int cid, int id)
{
    PlCharacter *pl;
    int i;

    if (cid >= 0x11) {
        return 0;
    }
    if (id == 0) {
        return 0;
    }
    pl = dataPlChaGet(cid);
    for (i = 0; i < 8; i++) {
        if (pl->special[i].id == id) {
            return &pl->special[i];
        }
    }
    return 0;
}

extern const char D_00A456C8[];
extern unsigned char plChaData[];
extern const char D_00A45790[];
extern PlCharacter *dataPlChaGet(int cid);

int dataNormIdxGet(ObjectTask *unit, int id)
{
    int i;
    int off = 0x70;
    int ret = -1;
    for (i = 0; i < 6; i++) {
        if (*(short *)((char *)calcUPGet(unit) + off + 6) == id) {
            ret = i;
            break;
        }
        off += 2;
    }
    return ret;
}

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataPlUnitInit);

void dataDummySet(void)
{
}

/*
 * playerData (7 entries) and enemyData (5 entries) are 0x180-byte battle
 * unit records; dataBattleInit is this TU's only writer and only clears the
 * calc-record index at +0x38.
 */
typedef struct BattleUnitInfo {
    unsigned char unmodeled_00[0x38];
    short calcIdx; /* +0x38 */
    unsigned char unmodeled_3A[0x146];
} BattleUnitInfo;

extern void dataFileInfoInit(void);
extern BattleUnitInfo playerData[7];
extern BattleUnitInfo enemyData[5];

void dataBattleInit(void)
{
    int i;

    dataFileInfoInit();
    for (i = 6; i >= 0; i--) {
        playerData[i].calcIdx = 0;
    }
    for (i = 4; i >= 0; i--) {
        enemyData[i].calcIdx = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataUnitInitSet);

/*
 * posTbl is a table of 0x20-byte position records; dataPosTblGet and
 * dataPosLineGet both index it with the absolute value of the caller's
 * position id (negative ids alias the same record as their positive
 * counterpart). dataPosLineGet reads the halfword at +0x10 of the record.
 */
typedef struct PosTblEntry {
    unsigned char unmodeled_00[0x10];
    short line; /* +0x10 */
    unsigned char unmodeled_12[0x20 - 0x12];
} PosTblEntry;

extern PosTblEntry posTbl[];

void *dataPosTblGet(int id)
{
    if (id < 0) {
        id = -id;
    }
    return &posTbl[id];
}

short dataPosLineGet(int id)
{
    if (id < 0) {
        id = -id;
    }
    return posTbl[id].line;
}

extern unsigned char D_00A45890[]; /* "** dataCidGet: err %d\n" */
extern short cidChgTbl[];

short dataCidGet(unsigned int cid)
{
    if (cid >= 0xC4) {
        printf(D_00A45890, cid);
        return 0;
    }
    return cidChgTbl[cid];
}

extern unsigned char D_00A458A8[]; /* "** dataWidGet: err %d" */
extern short widChgTbl[];

short dataWidGet(unsigned int wid)
{
    if (wid >= 0x75) {
        printf(D_00A458A8, wid);
        return 0;
    }
    return widChgTbl[wid];
}

extern int D_426F4C;
extern const char D_00A458C0[];

unsigned char *dataTecGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A458C0, index);
        return 0;
    }
    base = (unsigned char *)&D_426F4C - 0xC;
    return base + D_426F4C + ((index * 3) << 3) - 0x18;
}

extern const char D_00A458D8[];
extern int D_426F5C;

unsigned char *dataWpnGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A458D8, index);
        return 0;
    }
    base = (unsigned char *)&D_426F5C - 0x1C;
    return base + D_426F5C + (index << 5) - 0x20;
}

extern const char D_00A458F0[];
extern int D_426F68;

unsigned char *dataAttGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A458F0, index);
        return 0;
    }
    base = (unsigned char *)&D_426F68 - 0x28;
    return base + D_426F68 + index * 14 - 0xE;
}

extern const char D_00A45908[];
extern int D_426F60;

unsigned char *dataEthGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45908, index);
        return 0;
    }
    base = (unsigned char *)&D_426F60 - 0x20;
    return base + D_426F60 + index * 12 - 0xC;
}

extern const char D_00A45920[];
extern int D_426F64;

unsigned char *dataItmGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45920, index);
        return 0;
    }
    base = (unsigned char *)&D_426F64 - 0x24;
    return base + D_426F64 + index * 20 - 0x14;
}

extern int D_426F6C;
extern const char D_00A45938[];

unsigned char *dataAccGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45938, index);
        return 0;
    }
    base = (unsigned char *)&D_426F6C - 0x2C;
    return base + D_426F6C + (index << 4) - 0x10;
}

extern const char D_00A45950[];
extern int D_426F88;

unsigned char *dataFrmGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45950, index);
        return 0;
    }
    base = (unsigned char *)&D_426F88 - 0x48;
    return base + D_426F88 + (index << 4) - 0x10;
}

extern char D_00A45968[];
extern int D_426F8C;

unsigned char *dataEngGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45968, index);
        return 0;
    }
    base = (unsigned char *)&D_426F8C - 0x4C;
    return base + D_426F8C + index * 12 - 0xC;
}

extern const char D_00A45980[];
extern int D_426F70;

unsigned char *dataSklGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45980, index);
        return 0;
    }
    base = (unsigned char *)&D_426F70 - 0x30;
    return base + D_426F70 + (index << 3) - 0x8;
}

extern const char D_00A45998[];
extern int D_426F74;

unsigned char *dataGainGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45998, index);
        return 0;
    }
    base = (unsigned char *)&D_426F74 - 0x34;
    return base + D_426F74 + ((((index << 2) + index)) << 2) - 0x294;
}

extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
extern char D_00A459B0[];
extern char D_00A459C8[];

int dataFileLoad(const char *name, void *dst)
{
    long size = xglCdReadFile(name, dst, 0, 0);
    if (size < 0) {
        printf(D_00A459B0, name);
        return 0;
    }
    printf(D_00A459C8, name, (int)size);
    return 1;
}

int dataCdSync(void)
{
    return cdReqNum;
}

void dataCdSyncClear(void)
{
    cdReqNum = 0;
}

extern int printf(const char *format, ...);
extern const char D_00A459E0[];
extern const char D_00A459F8[];

void dataNBreadCB(int result) {
    if (result < 0) {
        printf(D_00A459E0, result);
        return;
    }
    if (result == 4) {
        if (--cdReqNum == 0) {
            printf(D_00A459F8, 0);
        }
    }
}

extern const char D_00A45A10[];
extern const char D_00A45A28[];

int dataFileLoadNB(const char *name, void *dst)
{
    long size = xglCdReadFile(name, dst, 1, (int)dataNBreadCB);
    if (size < 0) {
        printf(D_00A45A10, name);
        return 0;
    }
    cdReqNum++;
    printf(D_00A45A28, cdReqNum, name, dst);
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataFileInfoInit);

extern const char D_00A45A48[];

void dataFileInfoInitChg(ObjectTask *unit, int charaId)
{
    char *file = dataUnitFileGet(unit, charaId);
    int i, j;
    char *p4, *p8, *pc, *p10;

    if (file == 0) {
        printf(D_00A45A48);
        return;
    }
    i = 0;
    p8 = file + 8;
    pc = file + 12;
    p10 = file + 16;
    *(int *)(file + 4) = 0;
    p4 = file + 4;
    for (; i < 3; i++) {
        int off = 0x38 + i * 24;
        *(int *)(p8 + off) = 0;
        *(float *)(pc + off) = 0.0f;
        *(int *)(p10 + off) = 0;
        for (j = 0; j < 3; j++) {
            int k = 0xa0 + i * 36 + j * 12;
            *(int *)(file + k) = 0;
            *(int *)(p4 + k) = 0;
            *(int *)(p8 + k) = 0;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataEnemyMdlChk);

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataLeaderChk);

int dataLeaderCidGet(void) {
    return leaderCid;
}

void dataLeaderCidReset(void) {
    leaderCid = 0;
}

extern unsigned char *PartyDataGet(void);
extern const char D_00A45AB0[];

void dataLeaderReload(void)
{
    if (*(unsigned short *)(PartyDataGet() + 0x2C) != leaderCid) {
        printf(D_00A45AB0);
    }
}

extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern int dataMotNameGetSub(char *name, CalcUnitParam *up, int motionId, int weaponId);
extern char *motNameBase;
extern char *motNameExt;

int dataMotNameGetMenu(char *name, int unitOrgId, int weaponId)
{
    CalcUnitParam *up;
    int found;

    *name = 0;
    up = (CalcUnitParam *)dataUnitOrgGet(unitOrgId);
    if (up->sefSetupParams[weaponId] == 0) {
        return 0;
    }
    found = *(short *)(dataTecGet(*(short *)(dataWpnGet(up->sefSetupParams[weaponId]) + 0x14)) + 0x14);
    if (found == 0) {
        return 0;
    }
    strcpy(name, motNameBase);
    found = dataMotNameGetSub(name, up, found, weaponId);
    strcat(name, motNameExt);
    if (found == 0) {
        *name = 0;
    }
    return found;
}

extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern int dataMotNameGetSub(char *name, CalcUnitParam *up, int motionId, int weaponId);
extern char *motNameBase;
extern char *motNameExt;

int dataMotNameGet(char *name, ObjectTask *unit, int motionId, int weaponId)
{
    int found;

    strcpy(name, motNameBase);
    found = dataMotNameGetSub(name, calcUPGet(unit), motionId, weaponId);
    strcat(name, motNameExt);
    if (found == 0) {
        *name = 0;
    }
    return found;
}

extern char *motFile[];
extern char *agwsName[];
extern char *agwsLRName[];

int dataMotNameGetSub(char *name, CalcUnitParam *up, int motionId, int weaponId)
{
    char *mot;
    int lr;
    int i;
    int c;

    if (motionId < 0x157) {
        i = motionId;
        if (motionId >= 0x12E) {
            i = motionId - 0x2A;
        }
        mot = motFile[i - 1];
        if (mot != 0) {
            if (motionId >= 0x103 && (up->flags & 0x40)) {
                c = up->charaId - 0x11;
                if (motionId >= 0x12E) {
                    lr = 2;
                } else {
                    lr = (signed char)up->unmodeled_54[weaponId + 6] < 0x20;
                }
                strcat(name, agwsName[c]);
                strcat(name, mot);
                strcat(name, agwsLRName[lr]);
            } else {
                strcat(name, mot);
            }
            return 1;
        }
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataUnitFileLoad);

extern const char D_00A45AC8[];

void *dataUnitFileGet(ObjectTask *unit, int charaId)
{
    UnitFileInfo *slot;
    int lo;
    int hi;
    int i;

    slot = 0;
    for (i = 0; i < 6; i++) {
        if (unitFileInfo[i].charaId == charaId) {
            slot = &unitFileInfo[i];
            slot->charaId = calcUPGet(unit)->charaId;
            break;
        }
    }
    if (slot == 0) {
        if (charaId < 0x21 || charaId >= 0xBB) {
            lo = 0;
            hi = 3;
        } else {
            lo = 3;
            hi = 6;
        }
        for (i = lo; i < hi; i++) {
            if (unitFileInfo[i].charaId == 0) {
                slot = &unitFileInfo[i];
                slot->charaId = calcUPGet(unit)->charaId;
                break;
            }
        }
        if (slot == 0) {
            printf(D_00A45AC8);
        }
    }
    return slot;
}

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataPackPcMdlNameGet);

typedef struct { int id; char *name; } PackWpnEnt;
extern PackWpnEnt pcPack_1[];

char *dataPackWpnMdlNameGet(int id)
{
    char *name = 0;
    int i;
    for (i = 0; pcPack_1[i].id != 0; i++) {
        if (id == pcPack_1[i].id) {
            name = pcPack_1[i].name;
            break;
        }
    }
    return name;
}

/* Ported from upstream; local views keep this TU's existing types intact. */
typedef struct MdlLoadFile {
    int charaId;
    int modelCharaId; /* +0x04 */
    void *modelAdr; /* +0x08 */
    int modelSize; /* +0x0C */
    unsigned char unmodeled_10[4];
    void *animationAdr; /* +0x14 */
    int animationSize; /* +0x18 */
    unsigned char unmodeled_1c[4];
    void *textureAdr; /* +0x20 */
    int textureSize; /* +0x24 */
    unsigned char unmodeled_28[0x10C - 0x28];
    int faceCharaId; /* +0x10C */
} MdlLoadFile;
typedef struct MdlLoadActor {
    unsigned char unmodeled_00[0x8D0];
    void *modelAdr; /* +0x8D0 */
    void *animationAdr; /* +0x8D4 */
    void *textureAdr; /* +0x8D8 */
} MdlLoadActor;
typedef struct MdlLoadUnit {
    unsigned char unmodeled_00[0x10];
    int *work; /* +0x10: flags word first */
    MdlLoadActor *motionActor; /* +0x14 */
} MdlLoadUnit;
#define MDL_UP(unit) calcUPGet((ObjectTask *)(unit))

extern char mdlFileName[];
extern char mdlFileName2[];
extern char *pcNameBase;
extern char D_00A44898[];
extern const char D_00A45DD0[];
extern const char D_00A45DF8[];
extern const char D_00A45E00[];
extern const char D_00A45E08[];
extern const char D_00A45E10[];
extern const char D_00A45E38[];
extern char *dataPackPcMdlNameGet(ObjectTask *unit);
extern void RES_GetMdlFileName(char *name, int charaId);
extern int xglCdGetFileSize(const char *name);
extern int dataUnitFileLoadFaceMdl();
extern void ACT_initMotion();

int dataUnitFileLoadMdl(MdlLoadUnit *unit, int charaId, MdlLoadFile *file)
{
    MdlLoadActor *actor;
    char *packedName;

    actor = unit->motionActor;
    if (charaId != file->modelCharaId) {
        file->modelCharaId = charaId;
        if (((*unit->work & 0x40) == 0
             && (MDL_UP(unit)->flags & 0x40) == 0
             && (MDL_UP(unit)->charaId < 0xBB
                 || MDL_UP(unit)->charaId >= 0xC3))
            || (MDL_UP(unit)->charaId >= 0xBB
                && MDL_UP(unit)->charaId < 0xC3)) {
            if (dataLeaderCidGet() == charaId) {
                file->animationAdr = (void *)0x01070800;
                file->textureAdr = (void *)0x010B1000;
                printf(D_00A45DD0, charaId);
            } else {
                packedName = dataPackPcMdlNameGet((ObjectTask *)unit);
                strcpy(mdlFileName2, pcNameBase);
                strcat(mdlFileName2, packedName);
                strcat(mdlFileName2, D_00A44898);
                dataFileLoadNB(mdlFileName2, file->modelAdr);
                file->modelSize = ((xglCdGetFileSize(mdlFileName2) + 0x7FF) / 0x800) * 0x800;
                file->animationAdr = 0;
                file->textureAdr = 0;
            }
        } else {
            RES_GetMdlFileName(mdlFileName, charaId);
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45DF8);
            dataFileLoadNB(mdlFileName2, file->modelAdr);
            file->modelSize = ((xglCdGetFileSize(mdlFileName2) + 0x7FF) / 0x800) * 0x800;
            file->animationAdr = (unsigned char *)file->modelAdr + file->modelSize;
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45E00);
            dataFileLoadNB(mdlFileName2, file->animationAdr);
            file->animationSize = ((xglCdGetFileSize(mdlFileName2) + 0x7FF) / 0x800) * 0x800;
            file->textureAdr = (unsigned char *)file->animationAdr + file->animationSize;
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45E08);
            dataFileLoadNB(mdlFileName2, file->textureAdr);
            file->textureSize = ((xglCdGetFileSize(mdlFileName2) + 0x7FF) / 0x800) * 0x800;
        }
    } else {
        printf(D_00A45E10, charaId, file->modelAdr);
    }
    actor->modelAdr = file->modelAdr;
    actor->animationAdr = file->animationAdr;
    actor->textureAdr = file->textureAdr;
    ACT_initMotion(actor);
    printf(D_00A45E38, file->modelSize + file->animationSize + file->textureSize);
    if ((*unit->work & 0x40) == 0
        && (MDL_UP(unit)->flags & 0x40) == 0
        && (MDL_UP(unit)->charaId < 0xBB
            || MDL_UP(unit)->charaId >= 0xC3)) {
        dataUnitFileLoadFaceMdl(unit, charaId, file);
    } else {
        file->faceCharaId = 0;
    }
    return 1;
}

extern const char D_00A45E50[];

int dataUnitFileLoadFaceMdl(void *task, int cid, void *info) {
    char *p = (char *)info;
    char *a;

    if (cid != *(int *)(p + 0x10C)) {
        *(int *)(p + 0x10C) = cid;
        if (dataLeaderCidGet() == cid) {
            *(int *)(p + 0x110) = 0x010B4000;
            *(int *)(p + 0x128) = 0x010E6000;
        }
    } else {
        printf(D_00A45E50, cid, *(int *)(p + 0x110));
    }
    a = dataChildActorCreate(cid);
    *(int *)(a + 0x8D0) = *(int *)(p + 0x110);
    *(void **)((char *)task + 0x2C) = a;
    *(int *)(a + 0x8D4) = *(int *)(p + 0x14);
    *(int *)(a + 0x8D8) = *(int *)(p + 0x128);
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataUnitFileLoadMot);

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataUnitFileLoadWep);

/*
 * dataWpnLRChk shares dataUnitFileLoadWep's own (ObjectTask *, weaponId,
 * slot, fileInfo) argument list, passed through unchanged; it never reads
 * the unit argument and only reaches the loaded file's weapon-id field 0xA0
 * bytes into fileInfo.
 */
typedef struct WpnFileInfo {
    unsigned char unmodeled_00[0xA0];
    int weaponId; /* +0xA0 */
} WpnFileInfo;

int dataWpnLRChk(ObjectTask *unit, int weaponId, int slot, WpnFileInfo *fileInfo)
{
    /*
     * The original guards both checks with a bnel to a shared fail exit
     * (0x00a1c1b4, 0x00a1c1c0); a structured cascade of "if (...) return 0;"
     * statements folds the second guard into a branchless compare instead.
     */
    if (slot != 1) {
        goto fail;
    }
    if (weaponId != fileInfo->weaponId) {
        goto fail;
    }
    return 1;
fail:
    return 0;
}

/*
 * The engine's actor record ACT_create hands out (main/near_dir.h and
 * main/set_motion.h model the same 0xa70-byte record as their own TU-local
 * copy: +0x00 flags, +0x04 an update callback, +0x08 a draw callback).
 * dataChildActorCreate is this TU's only writer of that head.
 */
typedef struct ChildActor {
    int flags;                               /* +0x00 */
    void (*update)(struct ChildActor *self); /* +0x04 */
    void (*draw)(struct ChildActor *self);   /* +0x08 */
} ChildActor;

extern ChildActor *ACT_create(int parent, int callerId);
extern void ACT_initMotion(ChildActor *actor);
extern const char D_00A45F38[];

void *dataChildActorCreate(int callerId)
{
    ChildActor *actor;

    actor = ACT_create(-1, callerId);
    if (actor == 0) {
        printf(D_00A45F38);
        return 0;
    }
    ACT_initMotion(actor);
    actor->update = 0;
    actor->draw = 0;
    actor->flags = 0x208;
    return actor;
}

int dataUnitFileLoadMotSp(ObjectTask *unit, int motionId, int slot) {
    dataUnitFileLoadMot(unit, motionId, slot, dataUnitFileGet(unit, calcUPGet(unit)->charaId));
    return 1;
}

int dataUnitFileLoadMotSp2(UnitEquipInfo *unit, int motionId, int slot, int p3, int p4)
{
    EquipActor *actor;

    dataMotAdrSet(unit->motionTable, slot);
    if (p4 == 5) {
        actor = unit->equipActor[0];
        if (actor != 0) {
            actor->motionAdr = unit->motionTable->slot[slot];
        }
        actor = unit->equipActor[1];
        if (actor != 0) {
            actor->motionAdr = unit->motionTable->slot[slot];
        }
    } else if (p3 >= 0) {
        actor = unit->equipActor[p3 & 3];
        if (actor != 0) {
            actor->motionAdr = unit->motionTable->slot[slot];
        }
    }
    return 1;
}

extern char D_00A45F58[];
extern void dataUnitFileLoad2Sub(void *unit);
extern int rnd(int max);
extern void unitCmdSet(void *unit, int *cmd);

void dataUnitFileLoad2(void *unit)
{
    int cmd[8];

    printf(D_00A45F58, unit);
    dataUnitFileLoad2Sub(unit);
    cmd[0] = 0x25;
    if (!(**(int **)((char *)unit + 0x10) & 0x40)) {
        cmd[1] = 0;
    } else {
        cmd[1] = rnd(10);
    }
    unitCmdSet(unit, cmd);
}

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataUnitFileLoad2Sub);

/*
 * A packed model or animation file: 8 bytes not recovered, then one byte
 * offset per packed entry, counted from the start of the header, the same
 * idiom dataFpkAdrGet below resolves for a single entry.
 */
typedef struct PackedFile {
    unsigned char unmodeled_00[8];
    int entryOffset[3]; /* +0x08 */
} PackedFile;

void dataPackWpnMdl2(EquipActor *actor)
{
    PackedFile *pack;
    int offset;

    pack = (PackedFile *)actor->wpnMdl[0];
    offset = pack->entryOffset[0];
    actor->wpnMdl[0] = (unsigned char *)pack + offset;
    offset = pack->entryOffset[1];
    actor->wpnMdl[1] = (unsigned char *)pack + offset;
    offset = pack->entryOffset[2];
    actor->wpnMdl[2] = (unsigned char *)pack + offset;
}

typedef struct MotionAdrEntry {
    unsigned char unmodeled_00[8];
    int data_offset;
    int extra_offset;
} MotionAdrEntry;

void dataMotAdrSet(MotionAdrTable *table, int motionId)
{
    MotionAdrEntry *e;
    int d;
    int x;
    int *slot;
    int off = motionId << 2;

    off += 0x8dc;
    slot = (int *)((int)table + off);
    e = (MotionAdrEntry *)*slot;
    d = e->data_offset;
    x = e->extra_offset;
    *slot = (int)e + d;
    ((int *)((motionId << 2) + (int)table))[0x40] = (int)e + x;
}

/*
 * dataFpkAdrGet resolves a packed file's data pointer: 8 bytes not
 * recovered, then the offset of the packed data from the start of the
 * header.
 */
typedef struct FpkHeader {
    unsigned char unmodeled_00[8];
    int data_offset;
} FpkHeader;

void *dataFpkAdrGet(FpkHeader *fpk)
{
    return (unsigned char *)fpk + fpk->data_offset;
}

typedef struct WepSpEntry {
    int tid;
    int unk4;
    int unk8;
} WepSpEntry;

extern WepSpEntry wepSpTbl[];

WepSpEntry *dataTid2WepSp(int tid)
{
    int i;

    for (i = 0; wepSpTbl[i].tid != 0; i++) {
        if (wepSpTbl[i].tid == tid) {
            return &wepSpTbl[i];
        }
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataFileLoadWepSp);

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataFileLoadWepSp2);

extern void ACT_dispose(void *);

int dataFileLoadWepSpEnd(ObjectTask *unit, int tid) {
    int slot;
    void **pp;
    int off;

    slot = 2;
    if (dataTid2WepSp(tid) == 0) {
        slot = 1;
        if (((signed char *)dataTecGet(tid))[0x11] != 6) {
            return 0;
        }
    }
    off = slot * 4 + 0x1C;
    pp = (void **)((char *)unit + off);
    if (*pp != 0) {
        ACT_dispose(*pp);
        *pp = 0;
    }
    if (slot == 2) {
        calcUPGet(unit)->sefSetupParams[2] = 0;
        calcUPGet(unit)->unmodeled_54[8] = 0;
    }
    return 1;
}

/* Callers pass the unit whose default weapon they want; the function
 * always returns 0 and ignores it. */
int dataDefWpnGet(void *unit)
{
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataMtdRead);

typedef struct XtxFileEntry {
    unsigned char unmodeled_0[8];
} XtxFileEntry;

extern XtxFileEntry xtxFile[];

void *dataXtxFileGet(int index)
{
    return &xtxFile[index];
}

extern const char D_00A45FE8[];
extern unsigned short xtxAdr[0x13];

unsigned short dataXtxAdrGet(int index)
{
    unsigned short value;

    value = 0;
    if (index < 0x13) {
        if (index >= 0) {
            value = xtxAdr[index];
        } else {
            goto fail;
        }
    } else {
    fail:
        printf(D_00A45FE8, index);
    }
    return value;
}

typedef struct { char *name; void *adr; } XtxLoadEnt;

extern int dataFileLoadNB(const char *name, void *dst);

void dataXtxLoad(void)
{
    int i = 0;

    while (((XtxLoadEnt *)xtxFile)[i].name != 0) {
        ((int (*)(void *, void *))dataFileLoadNB)(((XtxLoadEnt *)xtxFile)[i].name, ((XtxLoadEnt *)xtxFile)[i].adr);
        i++;
    }
}

extern int dataFileLoad(const char *name, void *dst);

extern const char D_00A46008[];
extern unsigned char batDatBuf[0x10000];

void dataBatDatLoad(void)
{
    dataFileLoad(D_00A46008, batDatBuf);
}

extern unsigned char thinkBuf[0x4000];

void *dataThinkAdrGet(void)
{
    return thinkBuf;
}

int thinkMapGet(int id)
{
    int i;
    for (i = 0; i < 100; i++) {
        if (id == ((int *)&thinkMapTbl)[i]) {
            return i;
        }
    }
    return -1;
}

extern int thinkMapGet(int mapNo);
extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern int sprintf(char *buffer, const char *format, ...);
extern char *thinkName;
extern char *thinkNameBase;
extern char *thinkNameExt;
extern const char D_00A46028[];

int dataThinkNameGet(char *name, int thinkNo)
{
    char num[16];
    int mappedNo;

    mappedNo = thinkMapGet(thinkNo);
    if (mappedNo != -1) {
        strcpy(name, thinkNameBase);
        strcat(name, thinkName);
        sprintf(num, D_00A46028, mappedNo);
        strcat(name, num);
        strcat(name, thinkNameExt);
        return 1;
    }
    name[0] = '\0';
    return 0;
}

extern void thinkNoSet(int);

int dataThinkDataSet(int thinkNo, const void *data)
{
    int mappedNo;

    mappedNo = thinkMapGet(thinkNo);
    if (mappedNo != -1 && data != 0) {
        __builtin_memcpy(thinkBuf, data, sizeof(thinkBuf));
        thinkNoSet(mappedNo);
        return 1;
    }
    return 0;
}

extern void thinkNoSet(int);

int dataThinkFileLoad(int thinkNo)
{
    int no = thinkMapGet(thinkNo);
    if (no == -1)
        return 0;
    dataThinkLoad(no);
    thinkNoSet(no);
    return 1;
}

extern char *thinkName;
extern char *thinkNameBase;
extern char *thinkNameExt;
extern const char D_00A46028[];

void dataThinkLoad(int thinkNo)
{
    char path[256];
    char num[16];

    strcpy(path, thinkNameBase);
    strcat(path, thinkName);
    sprintf(num, D_00A46028, thinkNo);
    strcat(path, num);
    strcat(path, thinkNameExt);
    dataFileLoad(path, thinkBuf);
}

/* dataMapLoad passes the map number it is loading; every map loads into
 * the same fixed EE RAM region, so the number is ignored. */
void *dataMapLoadAdrGet(int mapNo)
{
    return (void *)0x01b9e000;
}

typedef struct MapHeader {
    unsigned char unmodeled_00[0x10];
    int cameraOffset; /* +0x10 */
} MapHeader;

void *dataMapCameraAdrGet(int mapNo)
{
    MapHeader *map = dataMapLoadAdrGet(mapNo);
    return (unsigned char *)map + map->cameraOffset;
}

extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern int sprintf(char *buffer, const char *format, ...);
extern char *mapNameBase;
extern char *mapName;
extern char *mapNameExt;
extern const char D_00A46028[];
extern const char D_00A46030[];
void dataMapLoad(int mapNo)
{
    char path[256];
    char num[16];

    if (mapNo < 100) {
        strcpy(path, mapNameBase);
        strcat(path, mapName);
        sprintf(num, D_00A46028, mapNo);
        strcat(path, num);
        strcat(path, mapNameExt);
        dataFileLoadNB(path, dataMapLoadAdrGet(mapNo));
        return;
    }
    printf(D_00A46030, mapNo);
}

void dataVPadModeSet(int mode)
{
    padMode = mode;
    padData = 0;
}

extern u16 padConvTbl[];

void dataVPadSet(u16 pad)
{
    int i;
    u16 r = 0;
    for (i = 0; i < 14; i++) {
        if ((pad >> i) & 1) {
            r |= padConvTbl[i];
        }
    }
    padData = r;
}

extern int D_426F4C;
extern const char D_00A458C0[];
extern const char D_00A458D8[];
extern int D_426F5C;
extern const char D_00A45980[];
extern int D_426F70;
typedef struct WpnLRFileInfo {
    unsigned char unmodeled_00[0xA0];
    int wpnId; /* +0xA0 */
} WpnLRFileInfo;
extern char D_00A45F58[];
extern void dataUnitFileLoad2Sub(void *unit);
extern int rnd(int max);
extern void unitCmdSet(void *unit, int *cmd);
extern void thinkNoSet(int);
extern u16 padConvTbl[];
extern unsigned short PadData[];

unsigned short dataPadRead(int arg0) {
    unsigned short r = 0;
    if (padMode == 0) {
        switch (arg0) {
        case 0: r = PadData[0x18]; break;
        case 1: r = PadData[0x19]; break;
        case 2: r = PadData[0x1a]; break;
        }
    } else {
        r = padData;
    }
    return r;
}

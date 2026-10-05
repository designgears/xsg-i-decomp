#include "common.h"

typedef struct MenuTecData {
    unsigned char unmodeled_00[0x20];
    unsigned short speedPointCost[8];
    unsigned short waitPointCost[8];
} MenuTecData;

extern MenuTecData *MenuTecDataBuf;
void *MenuTecDataGet(int chrNo);

int PartyDataGet(void);
typedef struct { char d[32]; } TecSave_t;

int MenuTecSaveDataGet(int chrNo)
{
    unsigned short c = chrNo;
    TecSave_t *p = (TecSave_t *)(PartyDataGet() + 0x3C);
    if ((unsigned)(c - 1) >= 8)
        return 0;
    return (int)&p[c - 1];
}

void *MenuTecDataGet(int characterNo)
{
    unsigned int characterIndex = characterNo & 0xFFFF;
    MenuTecData *records = MenuTecDataBuf;

    if (characterIndex - 1 >= 8U)
        return 0;

    return &records[characterIndex - 1];
}

int BitToTecNo(int b)
{
    unsigned short bit = b;
    if ((unsigned)(bit - 0x2D) < 8) return bit - 0x2D;
    if ((unsigned)(bit - 0x35) < 7) return bit - 0x35;
    if ((unsigned)(bit - 0x3C) < 8) return bit - 0x3C;
    if ((unsigned)(bit - 0x44) < 8) return bit - 0x44;
    if ((unsigned)(bit - 0x4C) < 7) return bit - 0x4C;
    if ((unsigned)(bit - 0x53) < 8) return bit - 0x53;
    if ((unsigned)(bit - 0x5B) < 8) return bit - 0x5B;
    return -1;
}

int MenuTecNextTLevPointGet(int chrNo, int point)
{
    unsigned char *data = MenuTecDataGet(chrNo);
    unsigned char *save = (unsigned char *)MenuTecSaveDataGet(chrNo);

    if (point < 0)
        return 0;
    return ((unsigned short *)(data + 0x10))[point] + data[point + 8] * (save[point] + 1);
}

int MenuTecTLevLimitCheck(int chrNo, int point)
{
    struct { unsigned char lv[0x18]; unsigned char max[0x18]; } *s;

    s = (void *)MenuTecSaveDataGet(chrNo);
    if (point < 0)
        return 0;
    return s->lv[point] < s->max[point];
}

void MenuTecTLevUp(int chrNo, int point)
{
    int maskedChrNo;
    int saveData;
    int *unit;
    int *unitBase;
    int points;
    unsigned char *techLevel;

    maskedChrNo = chrNo & 0xFFFF;
    saveData = MenuTecSaveDataGet(maskedChrNo);
    unit = func_A19210(maskedChrNo);

    if (point >= 0) {
        points = MenuTecNextTLevPointGet(chrNo, point);
        unitBase = unit + 3;
        techLevel = (unsigned char *)(saveData + point);
        *techLevel = *techLevel + 1;
        *(short *)((char *)unitBase + point * 12 + 64) = (short)(*techLevel * 3 - 3);
        unitBase[0] -= points;
    }
}

int MenuTecNextSpeedPointGet(int chrNo, int point)
{
    MenuTecData *data = MenuTecDataGet(chrNo);

    if (point < 0)
        return 0;

    return data->speedPointCost[point];
}

int MenuTecSaveDataGet(int chrNo);

int MenuTecSpeedLimitCheck(int chrNo, int point)
{
    unsigned char *saveData = (unsigned char *)MenuTecSaveDataGet(chrNo);

    if (point < 0)
        return 0;

    return saveData[point + 8] != 1;
}

int MenuTecSaveDataGet(int chrNo);
int MenuTecNextSpeedPointGet(int chrNo, int point);

/* dataPlChaGet (ov01/data_unit_org_get.c VA 0x00a19210, still INCLUDE_ASM
 * there and not yet linkable by that name): returns the running character's
 * control record, whose word at +0xC this function decrements. Kept under
 * the scaffold's undefined_funcs_auto.txt placeholder name until that TU
 * recovers it. */
int *func_A19210(int chrNo);

void MenuTecSpeedUp(int chrNo, int point)
{
    int maskedChrNo = chrNo & 0xFFFF;
    int saveData = MenuTecSaveDataGet(maskedChrNo);
    int *unit = func_A19210(maskedChrNo);

    if (point >= 0) {
        int points = MenuTecNextSpeedPointGet(chrNo, point);

        ((unsigned char *)(saveData + point))[8] = 1;
        unit[3] -= points;
    }
}

int MenuTecNextWaitPointGet(int chrNo, int point)
{
    MenuTecData *data = MenuTecDataGet(chrNo);

    if (point < 0)
        return 0;

    return data->waitPointCost[point];
}

int MenuTecWaitLimitCheck(int chrNo, int point)
{
    void *record = (void *)(point + MenuTecSaveDataGet(chrNo));
    int limitReached = 0;

    if (point >= 0) {
        limitReached = ((unsigned char *)record)[0x10] != 0;
    }
    return limitReached;
}

void MenuTecWaitUp(int chrNo, int point)
{
    int maskedChrNo = chrNo & 0xFFFF;
    int saveData = MenuTecSaveDataGet(chrNo);
    int *unit = func_A19210(maskedChrNo);

    if (point >= 0) {
        int cost = MenuTecNextWaitPointGet(chrNo, point);
        int idx = point + 0x10;
        unsigned char *p = (unsigned char *)saveData + idx;
        *p = *p - 1;
        *(unsigned short *)((char *)unit + point * 0xC + 0x4E) = *p;
        unit[3] -= cost;
    }
}

typedef struct MenuWorkState {
    unsigned char unmodeled_00[0x40];
    signed char characterNo;
    unsigned char unmodeled_41[3];
    signed char point;
    unsigned char unmodeled_45[0x3b];
} MenuWorkState;
extern MenuWorkState MenuWork;
int MenuTecTLevLimitCheck(int chrNo, int point);
int MenuTecNextTLevPointGet(int chrNo, int point);

int MenuTecCharTLevUpCheck(void)
{
    int *characterData;
    int nextPoint;

    if (MenuTecTLevLimitCheck(MenuWork.characterNo, MenuWork.point) != 0) {
        characterData = func_A19210(MenuWork.characterNo);
        nextPoint = MenuTecNextTLevPointGet(MenuWork.characterNo, MenuWork.point);
        if (characterData[3] >= nextPoint)
            return 1;
    }

    return 0;
}

int MenuTecCharSpeedUpCheck(void)
{
    int *characterData;
    int nextPoint;

    if (MenuTecSpeedLimitCheck(MenuWork.characterNo, MenuWork.point) != 0) {
        characterData = func_A19210(MenuWork.characterNo);
        nextPoint = MenuTecNextSpeedPointGet(MenuWork.characterNo, MenuWork.point);
        if (characterData[3] >= nextPoint)
            return 1;
    }

    return 0;
}

int MenuTecCharWaitUpCheck(void)
{
    int *characterData;
    int nextPoint;

    if (MenuTecWaitLimitCheck(MenuWork.characterNo, MenuWork.point) != 0) {
        characterData = func_A19210(MenuWork.characterNo);
        nextPoint = MenuTecNextWaitPointGet(MenuWork.characterNo, MenuWork.point);
        if (characterData[3] >= nextPoint)
            return 1;
    }

    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/menu_tec", MenuTecPasMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_tec", MenuTecInfoMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_tec", MenuTecMenuMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_tec", MenuTecExMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_tec", MenuTecL1R1Main);

INCLUDE_ASM("asm/main/nonmatchings/menu_tec", MenuTecStatusMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_tec", MenuTecListMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_tec", subMenuTecListChange_6);

INCLUDE_ASM("asm/main/nonmatchings/menu_tec", MenuTecListMain2);

INCLUDE_ASM("asm/main/nonmatchings/menu_tec", subListChange_11);

INCLUDE_ASM("asm/main/nonmatchings/menu_tec", MenuTecSetWinMain);

extern int func_A197E8(int, int);

void MenuTecSortSet00(void)
{
    int *addr = MenuSortAddrGet(0);
    struct { short min; short max; } tbl[7] = {
        { 0x2D, 0x34 }, { 0x35, 0x3B }, { 0x3C, 0x43 }, { 0x44, 0x4B },
        { 0x4C, 0x52 }, { 0x53, 0x5A }, { 0x5B, 0x62 },
    };
    int i;

    for (i = tbl[MenuWork.characterNo - 1].min; i <= tbl[MenuWork.characterNo - 1].max; i++) {
        if (func_A197E8(MenuWork.characterNo, i) != 0) {
            *addr++ = (i & 0xFFFF) + 0xA0000;
        }
    }
    *addr = 0;
}

/* dataUnitOrgGet (ov01/data_unit_org_get.c VA 0x00a191c0, still INCLUDE_ASM
 * there and not yet linkable by that name): returns the running character's
 * parameter record (the full record is src/main/menu_para_pt_rate_get.c's
 * CharParaData); MenuTecEquipCheck only reads the six equipped technique ids
 * at +0x82. Kept under the scaffold's undefined_funcs_auto.txt placeholder
 * name until that TU recovers it. */
typedef struct MenuTecEquipRecord {
    unsigned char unmodeled_00[0x82];
    short techniqueId[6]; /* +0x82 */
} MenuTecEquipRecord;
MenuTecEquipRecord *func_A191C0(int chrNo);

int MenuTecEquipCheck(int chrNo, int tecNo)
{
    int maskedTecNo = tecNo & 0xFFFF;
    MenuTecEquipRecord *equip = func_A191C0(chrNo & 0xFFFF);
    int count = 0;
    int i;

    for (i = 0; i < 6; i++) {
        if (equip->techniqueId[i] == maskedTecNo)
            count++;
    }

    return count;
}

/* dataTecGet (ov01/data_unit_org_get.c VA 0x00a1a378, still INCLUDE_ASM there
 * and not yet linkable by that name): returns a pointer to the technique's
 * metadata record, or a NULL pointer plus a diagnostic print for a
 * non-positive index. Kept under the scaffold's undefined_funcs_auto.txt
 * placeholder name until that TU recovers it. */
unsigned char *func_A1A378(int tecBit);

/* Returns whether either of the technique's two low target-metadata bits is
 * set (elf_names annotation). */
int MenuTecTrgCheck(int tecBit)
{
    return (func_A1A378(tecBit & 0xFFFF)[0] & 3) != 0;
}

int BitToTecNo(int bit);
int MenuTecSaveDataGet(int chrNo);

int MenuTecTypeCheck(int chrNo, int bit)
{
    int tecNo;

    chrNo = chrNo & 0xFFFF;
    tecNo = BitToTecNo(bit);
    return ((unsigned char *)(MenuTecSaveDataGet(chrNo) + tecNo))[8] == 1;
}

typedef struct { int unk0; int unk4; char unk8; char pad[3]; } MenuTecRow;
void *MenuListGet(int list);
int *MenuSortAddrGet(int list);
int MenuSortCheck(int list);
void MenuListMake(int list, int mode);

void MenuTecListMake00(void) {
    MenuTecRow *row = MenuListGet(0);
    int *addr = MenuSortAddrGet(0);
    int n = MenuSortCheck(0);
    int i;
    MenuListMake(0, 0);
    for (i = 0; i < n; i++) {
        row[i].unk4 = MenuTecEquipCheck(MenuWork.characterNo, *(short *)&addr[i]);
        row[i].unk8 = 0;
    }
}

extern unsigned char MenuTecTrgTbl00[];
extern unsigned char MenuTecTrgTbl01[];

void MenuTecListMake00_2(void) {
    MenuTecRow *row = MenuListGet(0);
    int *addr = MenuSortAddrGet(0);
    int n = MenuSortCheck(0);
    int i;
    int mode = (signed char)MenuWork.unmodeled_00[0x31] < 2;
    for (i = 0; i < n; i++) {
        int type = MenuTecTypeCheck(MenuWork.characterNo, addr[i]);
        if (mode == type || type == 1) {
            unsigned char *tbl = (MenuWork.characterNo == 7) ? MenuTecTrgTbl01 : MenuTecTrgTbl00;
            if (tbl[(signed char)MenuWork.unmodeled_00[0x31]] == MenuTecTrgCheck(addr[i])) {
                row[i].unk8 = 0;
            } else {
                row[i].unk8 = 1;
            }
        } else {
            row[i].unk8 = 1;
        }
    }
}

void MenuTecListMake01(void) {
    MenuTecRow *row = MenuListGet(0);
    int *addr = MenuSortAddrGet(0);
    int n = MenuSortCheck(0);
    int i;
    signed char saveBit = MenuWork.unmodeled_41[2];
    signed char savePoint = MenuWork.point;
    MenuListMake(0, 0);
    for (i = 0; i < n; i++) {
        row[i].unk4 = MenuTecEquipCheck(MenuWork.characterNo, *(short *)&addr[i]);
        MenuWork.unmodeled_41[2] = addr[i];
        MenuWork.point = BitToTecNo((signed char)MenuWork.unmodeled_41[2]);
        if (MenuTecCharTLevUpCheck() || MenuTecCharWaitUpCheck() || MenuTecCharSpeedUpCheck()) {
            row[i].unk8 = 0;
        } else {
            row[i].unk8 = 1;
        }
    }
    MenuWork.unmodeled_41[2] = saveBit;
    MenuWork.point = savePoint;
}

INCLUDE_ASM("asm/main/nonmatchings/menu_tec", MenuTec);

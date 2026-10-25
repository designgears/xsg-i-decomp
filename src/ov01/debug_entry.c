/*
 * OV01 original TU 14: 0x00a2e4b0..0x00a31340 (64 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov01/battle_init.h"
#include "ov01/calc.h"

/* Debug menu: enters the battle debug interface; page 0/1/2 selects the
 * status, parameter or spec display (debugBattle tail-calls one of them). */
extern void debugBattle(int page);

void debugEntry(int page)
{
    debugBattle(page);
}

extern char *err; /* fallback string returned for an out-of-range debug table index */
extern char *nameTbl[0xC3]; /* debug menu's character name table */

char *debugNameGet(int charaId) {
    if (charaId < 0xC3) {
        return nameTbl[charaId];
    }
    return err;
}

extern char *wpnTbl[0x74]; /* debug menu's weapon name table */

char *debugWpnGet(int wpnId) {
    if (wpnId < 0x74) {
        return wpnTbl[wpnId];
    }
    return err;
}

/*
 * calcUPGet's per-unit parameter record, evidenced here by debugParaDisp's
 * own printf labels (include/ov01/calc.h's CalcUnitParam is this TU's own
 * independent, differently-scoped view of the same object: it already
 * evidences agility/moveKindId +0x0E-0x0F, flags +0x1C, charaId +0x38 and
 * turnBias/waitCount/waitCountBase +0x3E-0x42 from other functions).
 */
typedef struct DebugUnitPara {
    short maxHp;             /* +0x00: "HP=%d/%d" 2nd */
    short maxEp;             /* +0x02: "EP=%d/%d" 2nd */
    short str;               /* +0x04: "STR=%3d" */
    short vit;               /* +0x06: "VIT=%3d" */
    short eatk;              /* +0x08: "EATK=%3d" */
    short edef;              /* +0x0A: "EDEF=%3d" */
    signed char hit;         /* +0x0C: "HIT=%3d" */
    signed char eva;         /* +0x0D: "EVA=%3d" */
    signed char agility;     /* +0x0E: "AGL=%3d" */
    signed char moveKindId;  /* +0x0F: "MVTYPE=%1d" */
    unsigned char unmodeled_10[0x14 - 0x10];
    signed char level;       /* +0x14: "LV=%d" */
    unsigned char unmodeled_15[0x16 - 0x15];
    signed char cRate;       /* +0x16: "C_RATE=%3d" */
    unsigned char unmodeled_17[0x18 - 0x17];
    unsigned short weak;     /* +0x18: "WEAK=%04X" */
    unsigned short spec;     /* +0x1A: "SPEC=%04X" */
    unsigned short flags;    /* +0x1C: "INFO=%04X" */
    unsigned char unmodeled_1e[0x34 - 0x1E];
    short hp;                /* +0x34: "HP=%d/%d" 1st */
    short ep;                /* +0x36: "EP=%d/%d" 1st */
    short charaId;           /* +0x38: debugNameGet's argument */
    signed char ap;          /* +0x3A: debugCmdPrint "AP=%2d" */
    unsigned char unmodeled_3b[0x3C - 0x3B];
    signed char bc;          /* +0x3C: "BC =%3d" */
    signed char bp;          /* +0x3D: "BP = %3d" */
    short turnBias;          /* +0x3E: "AGLPLUS=%3d" */
    short waitCount;         /* +0x40: "WCT=%3d/%3d" 1st */
    short waitCountBase;     /* +0x42: "WCT=%3d/%3d" 2nd */
    unsigned char unmodeled_44[0x14C - 0x44];
    unsigned short etherDis; /* +0x14C: "ETHERDIS=%04X" */
    unsigned char unmodeled_14e[0x17C - 0x14E];
    int wpnAtkAdj;           /* +0x17C: "WPNATKADJ=%3d" */
} DebugUnitPara;

extern DebugUnitPara *calcUPGet(ObjectTask *unit);

extern int unitNoGet(ObjectTask *unit);

extern char *cmdStrTbl[];

extern int *unitCmdPtrGet(ObjectTask *unit);
extern int frameCntGet(void);
extern unsigned char *dataItmGet(int index);
extern int dataTecGet(int tecId);
extern int printf(const char *format, ...);
extern const char D_00A50380[];
extern const char D_00A503A8[];
extern const char D_00A503C0[];
extern const char D_00A503E0[];
extern const char D_00A50400[];
extern const char D_00A50430[];
extern const char D_00A50438[];
extern const char D_00A50440[];
extern const char D_00A50468[];

void debugCmdPrint(ObjectTask *unit, int cmd)
{
    int *p;
    int tid;
    ObjectTask *tgt;
    int para;
    unsigned char flag;

    p = unitCmdPtrGet(unit);
    tid = p[1];
    tgt = (ObjectTask *)p[2];
    para = p[3];
    switch (cmd) {
    case 1:
    case 2:
    case 3:
        printf(D_00A50380, frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd], tid, para);
        break;
    case 15: case 17: case 19: case 21: case 23:
    case 25: case 27: case 29: case 31: case 33:
        printf(D_00A503A8, frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd]);
        break;
    case 5: case 6: case 16: case 18: case 20: case 22:
    case 24: case 26: case 28: case 30: case 32: case 34:
        printf(D_00A503C0, frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd], tid);
        break;
    case 13:
        printf(D_00A503E0, frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd], tid);
        break;
    case 4: case 8: case 9: case 10: case 11: case 12:
    case 37: case 38: case 39: case 40: case 41: case 42:
    case 44: case 45: case 46: case 47: case 48:
        printf(D_00A503A8, frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd]);
        break;
    case 14:
        if (p[4] == 3) {
            flag = dataItmGet(para)[8];
        } else {
            flag = *(unsigned char *)dataTecGet(tid);
        }
        printf(D_00A50400, frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd], tid, unitNoGet(tgt),
               debugNameGet(calcUPGet(tgt)->charaId), (flag & 0x10) ? D_00A50430 : D_00A50438);
        break;
    case 43:
        printf(D_00A50440, frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd], tid, p[2]);
        break;
    default:
        do {
            printf(D_00A50468, frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
                   calcUPGet(unit)->ap, p[0]);
        } while (0);
        break;
    }
}

extern char D_00A5AA40[];
extern const char D_00A50550[];
extern const char D_00A50558[];
extern const char D_00A50560[];
extern const char D_00A50568[];

#define DEBUG_THINK_REG_VALID 0x8000
#define DEBUG_THINK_REG_TEXT 0x4000
#define DEBUG_THINK_REG_INDEX_MASK 0x3FFF
#define DEBUG_THINK_NUMERIC_LIMIT 16
#define CMD_STR_TBL_REG_NAMES 0x28

char *debugThinkRegName(int reg_code)
{
    int reg_index;

    reg_index = reg_code & DEBUG_THINK_REG_INDEX_MASK;
    if (reg_code & DEBUG_THINK_REG_VALID) {
        if (reg_code & DEBUG_THINK_REG_TEXT) {
            if (reg_index < DEBUG_THINK_NUMERIC_LIMIT) {
                sprintf(D_00A5AA40, D_00A50550, reg_index);
            } else {
                sprintf(D_00A5AA40, D_00A50558, cmdStrTbl[CMD_STR_TBL_REG_NAMES + reg_index]);
            }
        } else {
            if (reg_index < DEBUG_THINK_NUMERIC_LIMIT) {
                sprintf(D_00A5AA40, D_00A50560, reg_index);
            } else {
                sprintf(D_00A5AA40, D_00A50568, cmdStrTbl[CMD_STR_TBL_REG_NAMES + reg_index]);
            }
        }
    } else {
        D_00A5AA40[0] = '\0';
    }

    return D_00A5AA40;
}

extern int printf(const char *format, ...);
extern short thinkRegNo(short pc);
extern short thinkRegGet(unsigned short regIndex);
extern short cmdNum(short pc);
extern char *thinkStrTbl[];
extern char *thinkExpTbl[];
extern const char D_00A50570[];
extern const char D_00A50580[];
extern const char D_00A50588[];
extern const char D_00A505A0[];
extern const char D_00A505A8[];
extern const char D_00A505B0[];
extern const char D_00A505C0[];
extern const char D_00A505D0[];
extern const char D_00A505D8[];
extern const char D_00A505E8[];

void debugThinkPrint(int cmd, short pc, short jmp)
{
    int r, a, b, c;
    char *name;

    printf(D_00A50570, pc, thinkStrTbl[cmd]);
    switch (cmd) {
    case 1:
    case 2:
        printf(D_00A50580, jmp);
        break;
    case 4:
        pc += 2;
        r = cmdNum(pc);
        pc += 2;
        b = cmdNum(pc);
        c = cmdNum(pc + 2);
        printf(D_00A50588, r, thinkExpTbl[b], c, jmp);
        break;
    case 5:
    case 6:
        a = cmdNum(pc + 2);
        printf(D_00A505A0, a, jmp);
        break;
    case 7:
        pc += 2;
        r = thinkRegNo(pc);
        a = cmdNum(pc + 2);
        name = debugThinkRegName(r);
        printf(D_00A505A8, name, a);
        break;
    case 8:
    case 9:
    case 18:
    case 19:
        pc += 2;
        printf(D_00A505B0, debugThinkRegName(thinkRegNo(pc)), cmdNum(pc));
        break;
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 20:
        pc += 2;
        r = thinkRegNo(pc);
        pc += 2;
        a = cmdNum(pc);
        jmp = cmdNum(pc - 2);
        name = debugThinkRegName(r);
        printf(D_00A505C0, name, a, jmp);
        break;
    case 21:
        a = cmdNum(pc + 2);
        printf(D_00A505D0, a);
        break;
    case 22:
        r = thinkRegNo(pc + 2);
        jmp = thinkRegGet(r);
        name = debugThinkRegName(r);
        printf(D_00A505B0, name, jmp);
        break;
    case 23:
    case 24:
        pc += 2;
        r = thinkRegNo(pc);
        pc += 2;
        a = cmdNum(pc);
        b = cmdNum(pc + 2);
        jmp = thinkRegGet(r);
        name = debugThinkRegName(r);
        printf(D_00A505D8, name, a, b, jmp);
        break;
    default:
        printf(D_00A505E8);
        r = 0;
        break;
    }
}

extern int debugStatDisp(void);
extern int debugSpecDisp(void);
extern int debugParaDisp(void);

void debugBattle(int page)
{
    if (page == 0) {
        debugStatDisp();
    } else if (page == 1) {
        debugParaDisp();
    } else if (page == 2) {
        debugSpecDisp();
    }
}

extern int unitTblGet(int side, BattleUnit ***table);

extern char *debugNameGet(int charaId);

 /* "** UNIT PARA **\n" */
extern const char D_00A50668[];

 /* " WCT=%3d/%3d AGLPLUS=%3d ETHERDIS=%04X WPNATKADJ=%3d\n" */
extern int printf(const char *format, ...);

extern const char D_00A50650[];
extern const char D_00A50690[];
extern const char D_00A50698[];
extern const char D_00A505E8[];
extern const char D_00A506A0[];
extern const char D_00A506A8[];
extern const char D_00A506B0[];
extern const char D_00A506B8[];

int debugStatDisp(void)
{
    BattleUnit **units;
    DebugUnitPara *up;
    unsigned char *base;
    unsigned char *p;
    int count;
    int i;
    int j;
    int k;

    count = unitTblGet(2, &units);
    printf(D_00A50650);
    for (i = 0; i < count; i++) {
        if (units[i] != 0) {
            up = calcUPGet((ObjectTask *)units[i]);
            base = (unsigned char *)up + 0xC;
            printf(D_00A50668, unitNoGet((ObjectTask *)units[i]), debugNameGet(up->charaId),
                   up->level, up->hp, up->maxHp, up->ep, up->maxEp);
            printf(D_00A50690);
            for (j = 0; j < 8; j++) {
                printf(D_00A50698, ((unsigned short *)((unsigned char *)up + 0xAC))[j]);
            }
            printf(D_00A505E8);
            printf(D_00A506A0);
            for (j = 0; j < 8; j++) {
                printf(D_00A50698, ((unsigned short *)(base + 0xB0))[j]);
            }
            printf(D_00A505E8);
            printf(D_00A506A8);
            for (j = 0; j < 8; j++) {
                printf(D_00A50698, ((unsigned short *)((unsigned char *)up + 0x24))[j]);
            }
            printf(D_00A505E8);
            printf(D_00A506B0);
            for (j = 0; j < 8; j++) {
                p = base + j * 16 + 0xC0;
                for (k = 0; k < 16; k++) {
                    if (*p != 0) {
                        printf(D_00A506B8, *p);
                    }
                    p++;
                }
            }
            printf(D_00A505E8);
        }
    }
    return 1;
}

extern const char D_00A506C0[]; /* "** %d) %s LV=%d HP=%d/%d EP=%d/%d\n" */
extern const char D_00A506D8[]; /* " STR=%3d VIT=%3d EATK=%3d EDEF=%3d HIT=%3d EVA=%3d\n" */
extern const char D_00A50710[]; /* " AGL=%3d BC =%3d BP = %3d MVTYPE=%1d C_RATE=%3d\n" */
extern const char D_00A50748[]; /* " WEAK=%04X SPEC=%04X INFO=%04X OBJSTAT=%04X\n" */
extern const char D_00A50778[];int debugParaDisp(void)
{
    BattleUnit **units;
    DebugUnitPara *up;
    int *objStat;
    int count;
    int i;

    count = unitTblGet(2, &units);
    printf(D_00A506C0);
    for (i = 0; i < count; i++) {
        if (units[i] != 0) {
            up = calcUPGet((ObjectTask *)units[i]);
            printf(D_00A50668, unitNoGet((ObjectTask *)units[i]), debugNameGet(up->charaId),
                   up->level, up->hp, up->maxHp, up->ep, up->maxEp);
            printf(D_00A506D8, up->str, up->vit, up->eatk, up->edef, up->hit, up->eva);
            printf(D_00A50710, up->agility, up->bc, up->bp, up->moveKindId, up->cRate);
            objStat = ((ObjectTask *)units[i])->work;
            printf(D_00A50748, up->weak, up->spec, up->flags, *objStat);
            printf(D_00A50778, up->waitCount, up->waitCountBase, up->turnBias,
                   up->etherDis, up->wpnAtkAdj);
        }
    }
    return 1;
}

typedef struct DebugSpecSlot {
    short id;
    short spd;
    short pow;
    short agl;
    short use;
    short kill;
} DebugSpecSlot;
extern char *dataPlChaGet(int cid);
extern const char D_00A507B0[];
extern const char D_00A507C8[];

int debugSpecDisp(void)
{
    BattleUnit **units;
    DebugUnitPara *up;
    DebugSpecSlot *sp;
    int count;
    int i;
    int j;

    count = unitTblGet(2, &units);
    printf(D_00A507B0);
    for (i = 0; i < count; i++) {
        if (units[i] != 0 && !(*(int *)((ObjectTask *)units[i])->work & 0x40)) {
            up = calcUPGet((ObjectTask *)units[i]);
            sp = (DebugSpecSlot *)(dataPlChaGet(up->charaId) + 0x48);
            printf(D_00A50668, unitNoGet((ObjectTask *)units[i]), debugNameGet(up->charaId),
                   up->level, up->hp, up->maxHp, up->ep, up->maxEp);
            for (j = 0; j < 8; j++) {
                printf(D_00A507C8, j, sp->id, sp->spd, sp->pow, sp->agl, sp->use, sp->kill);
                sp++;
            }
        }
    }
    return 1;
}

extern int plMuteki; /* debug menu: player invincibility flag */

int plMutekiGet(void)
{
    return plMuteki;
}

extern int enMuteki; /* debug menu: enemy invincibility flag */

int enMutekiGet(void)
{
    return enMuteki;
}

extern int plIchigeki; /* debug menu: player one-hit-kill flag */

int plIchigekiGet(void)
{
    return plIchigeki;
}

extern int dataItmBoxInc(int id);

void debugItemSet(void)
{
    int pass;
    int item_id;

    for (pass = 0; pass < 10; pass++) {
        for (item_id = 1; item_id < 36; item_id++) {
            dataItmBoxInc(item_id);
        }
    }
}

extern int calcEtherEquipOrg(int charaId, int slot, int etherId);

void debugEtherSet(void)
{
    int slot;

    for (slot = 0; slot < 12; slot++) {
        calcEtherEquipOrg(3, slot, slot + 1);
    }
    for (slot = 0; slot < 10; slot++) {
        calcEtherEquipOrg(1, slot, slot + 0x1B);
    }
    for (slot = 0; slot < 10; slot++) {
        calcEtherEquipOrg(2, slot, slot + 0x11);
    }
    for (slot = 0; slot < 12; slot++) {
        calcEtherEquipOrg(6, slot, slot + 0x25);
    }
    for (slot = 0; slot < 10; slot++) {
        calcEtherEquipOrg(7, slot, slot + 0x31);
    }
    for (slot = 0; slot < 8; slot++) {
        calcEtherEquipOrg(5, slot, slot + 0x48);
    }
}

void debugSkillSet(void) {

}

extern int calcAgwsEquipOrg(int agwsId, int charaId);
extern int calcAttEquipOrg(int charaId, int slot, int attId);
extern int calcWpnEquipOrg(int charaId, int slot, int wpnId, int variant);

/* Debug menu: forces a fixed test loadout onto character 0x1A. */
int debugEquSet(void)
{
    calcAgwsEquipOrg(6, 0x1A);
    calcWpnEquipOrg(0x1A, 0, 0x4E, 0x19);
    calcWpnEquipOrg(0x1A, 1, 0x4E, 0x21);
    calcAttEquipOrg(0x1A, 0, 1);
    return calcAttEquipOrg(0x1A, 1, 1);
}

/*
 * dataUnitOrgGet (ov01 VA 0x00a191c0, still asm in its own TU
 * data_unit_org_get.c) returns character chrNo's original stat record;
 * calcTotalParaMenu (ov01 VA 0x00a11108, still asm in ov01/calc.c) returns a
 * recalculated copy of the same record (its own maxHp/maxEp lead the same
 * way src/main/menu_para_pt_rate_get.c's CharParaData already evidences).
 * The equip screens below index its agws/engine/frame/accessory/attachment/
 * special/ether/skill ids by the character calcXxxEquipOrg is told to
 * refit.
 */
typedef struct UnitOrgData {
    short maxHp;               /* +0x00 */
    short maxEp;                /* +0x02 */
    unsigned char unmodeled_04[0x34 - 0x04];
    short hp;                   /* +0x34 */
    short ep;                   /* +0x36 */
    short charaId;               /* +0x38 */
    unsigned char unmodeled_3a[0x54 - 0x3A];
    short agwsId;                /* +0x54 */
    short engineId;               /* +0x56 */
    short frameId;                /* +0x58 */
    signed char weaponVariant[3]; /* +0x5A */
    unsigned char unmodeled_5d;
    short weaponId[3];            /* +0x5E */
    short accessory[3];           /* +0x64 */
    short attachment[3];          /* +0x6A */
    unsigned char unmodeled_70[0x82 - 0x70];
    short special[6];             /* +0x82 */
    short ether[12];              /* +0x8E */
    short skill[3];                /* +0xA6 */
} UnitOrgData;

/*
 * The party roster record plUnitTblGet (ov01 VA 0x00a0b630, still asm in its
 * own TU battle_init.c) returns the base of: configPlayer cycles player
 * (+0x00) 0..0x1F, configPlPos cycles position (+0x02) 1..9. plUnitTblSet
 * writes both fields back for one slot.
 */
typedef struct DebugPartyUnit {
    short player;   /* +0x00 */
    short position; /* +0x02 */
    unsigned char unmodeled_04[4];
} DebugPartyUnit;

extern UnitOrgData *dataUnitOrgGet(int chrNo);

extern DebugPartyUnit *plUnitTblGet(void);

extern int valLR(int *value, int min, int max, int step);

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct DebugPadView {
    unsigned char unmodeled_00[0x2C];
    unsigned short buttons; /* +0x2C */
} DebugPadView;

INCLUDE_ASM("asm/nonmatchings/ov01/debug_entry", debugBattleConfig);

int configStart(void) {
    return 1;
}

/* Debug menu: cycles the character assigned to party slot `slot`. */
extern int configPlayer(int slot);

int configPlayer1(void) {
    return configPlayer(0);
}

int configPlayer2(void) {
    return configPlayer(1);
}

int configPlayer3(void) {
    return configPlayer(2);
}

extern void plUnitTblSet(int slot, int player, int position);
int configPlayer(int slot)
{
    DebugPartyUnit *units;
    int player;

    units = plUnitTblGet();
    player = units[slot].player;
    valLR(&player, 0, 31, 1);
    plUnitTblSet(slot, player, units[slot].position);
    return 0;
}

/* Debug menu: cycles the formation position of party slot `slot`. */
extern int configPlPos(int slot);

int configPlPos1(void) {
    return configPlPos(0);
}

int configPlPos2(void) {
    return configPlPos(1);
}

int configPlPos3(void) {
    return configPlPos(2);
}

int configPlPos(int slot)
{
    DebugPartyUnit *units;
    int position;

    units = plUnitTblGet();
    position = units[slot].position;
    valLR(&position, 1, 9, 1);
    plUnitTblSet(slot, units[slot].player, position);
    return 0;
}

/* Debug menu left/right adjuster: steps *value by step (d-pad) or by
   10 * step (shoulder buttons), wrapping from past max to min and from below
   min to max. Returns nonzero when *value changed. */
extern int valLR(int *value, int min, int max, int step);

extern void monsSetNoSet(int monsSetNo);
extern int monsSet; /* debug menu's selected monster-set number */

int configMonsSet(void) {
    valLR(&monsSet, 0, 0x63, 1);
    monsSetNoSet(monsSet);
    return 0;
}

extern void mapNoSet(int mapNo);
extern int mapNo; /* debug menu's selected map number */

int configMap(void) {
    valLR(&mapNo, 0, 0x63, 1);
    mapNoSet(mapNo);
    return 0;
}

extern int cameraFlagGet(void);
extern void cameraFlagSet(int flag);

int configCamera(void) {
    int flag;

    flag = cameraFlagGet();
    valLR(&flag, 0, 1, 1);
    cameraFlagSet(flag);
    return 0;
}

int configStat(void)
{
    extern int statId;
    extern PadPrefix PadData;
    extern int configSeq;

    valLR(&statId, 1, 0xC2, 1);
    if (PadData.half_2a & 0x20) {
        configSeq = 1;
    }
    return 0;
}

extern int equipId; /* debug equip menu's selected character index, 1..0x20 */
extern PadPrefix PadData;
extern int configSeq; /* debug menu's current top-level page */
extern int equipPosX; /* debug menu's cursor column on the equip screen */

int configEqu(void) {
    valLR(&equipId, 1, 0x20, 1);
    if (PadData.half_2a & 0x20) {
        configSeq = 2;
        equipPosX = 0;
    }
    return 0;
}

int configGain(void)
{
    extern int gainId;
    extern PadPrefix PadData;
    extern int configSeq;
    extern int equipPosX;

    valLR(&gainId, 1, 0xC2, 1);
    if (PadData.half_2a & 0x20) {
        configSeq = 3;
        equipPosX = 0;
    }
    return 0;
}

int configPlMuteki(void) {
    int flag;

    flag = plMutekiGet();
    valLR(&flag, 0, 1, 1);
    plMuteki = flag;
    return 0;
}

int configEnMuteki(void) {
    int flag;

    flag = enMutekiGet();
    valLR(&flag, 0, 1, 1);
    enMuteki = flag;
    return 0;
}

int configPlIchigeki(void) {
    int flag;

    flag = plIchigekiGet();
    valLR(&flag, 0, 1, 1);
    plIchigeki = flag;
    return 0;
}

extern void cfEncountSet(int cfEncount);
extern int cfEncount; /* debug menu's CF encounter value */

int configCFEncount(void) {
    valLR(&cfEncount, 0, 0xFFFF, 1);
    cfEncountSet(cfEncount);
    return 0;
}

extern void cfEventSet(int cfEvent);
extern int cfEvent; /* debug menu's CF event value */

int configCFEvent(void) {
    valLR(&cfEvent, 0, 0xFFFF, 1);
    cfEventSet(cfEvent);
    return 0;
}

extern void thinkNoSet(int thinkNo);
extern int D_00A5AA60; /* debug menu's selected think (AI) number */

int configThink(void) {
    valLR(&D_00A5AA60, 0, 0x63, 1);
    thinkNoSet(D_00A5AA60);
    return 0;
}

extern UnitOrgData *calcTotalParaMenu(int chrNo, int *attack, int *defense);
int configYadoya(void) {
    int chrNo;
    UnitOrgData *total;
    UnitOrgData *rec;

    if (PadData.half_2a & 0x20) {
        for (chrNo = 1; chrNo <= 0x20; chrNo++) {
            total = calcTotalParaMenu(chrNo, 0, 0);
            rec = dataUnitOrgGet(chrNo);
            rec->hp = total->maxHp;
            rec->ep = total->maxEp;
        }
    }
    return 0;
}

int valUD(int *value, int min, int max, int step)
{
    int old;
    int pad;

    old = *value;
    pad = ((u16 *)&PadData)[0x16];
    if (pad & 0x4000) {
        *value += step;
        if (*value > max) {
            *value = min;
        }
    } else if (pad & 0x1000) {
        *value -= step;
        if (*value < min) {
            *value = max;
        }
    }
    return *value != old;
}

typedef struct {
    u8 pad_00[0x2C];
    u16 trig;
} PadLR;

int valLR(int *value, int min, int max, int step)
{
    int old;
    int pad;

    old = *value;
    if (((PadLR *)&PadData)->trig & 0x8) {
        *value += step * 10;
        if (*value > max) {
            *value = min;
        }
    }
    if (((PadLR *)&PadData)->trig & 0x4) {
        *value -= step * 10;
        if (*value < min) {
            *value = max;
        }
    }
    pad = ((PadLR *)&PadData)->trig;
    if (pad & 0x2000) {
        *value += step;
        if (*value > max) {
            *value = min;
        }
    } else if (pad & 0x8000) {
        *value -= step;
        if (*value < min) {
            *value = max;
        }
    }
    return *value != old;
}

extern int equipId;

int equipChar(void) {
    valLR(&equipId, 1, 0x20, 1);
    return 0;
}

int equipAgws(void) {
    UnitOrgData *unit;
    int agwsId;

    unit = dataUnitOrgGet(equipId);
    agwsId = unit->agwsId;
    valLR(&agwsId, 0x11, 0x20, 1);
    calcAgwsEquipOrg(unit->charaId, agwsId);
    return 0;
}

extern int calcEngineEquipOrg(int charaId, int engineId);

int equipEngine(void) {
    UnitOrgData *unit;
    int engineId;

    unit = dataUnitOrgGet(equipId);
    engineId = unit->engineId;
    valLR(&engineId, 0, 0x28, 1);
    calcEngineEquipOrg(unit->charaId, engineId);
    return 0;
}

extern int calcFrameEquipOrg(int charaId, int frameId);

int equipFrame(void) {
    UnitOrgData *unit;
    int frameId;

    unit = dataUnitOrgGet(equipId);
    frameId = unit->frameId;
    valLR(&frameId, 0, 0x29, 1);
    calcFrameEquipOrg(unit->charaId, frameId);
    return 0;
}

/* Debug equip menu: cycles the weapon in equipment slot `slot` of the
   character selected by equipId. */
extern int equipWpn(int slot);

int equipWpn0(void) {
    return equipWpn(0);
}

int equipWpn1(void) {
    return equipWpn(1);
}

int equipWpn2(void) {
    return equipWpn(2);
}

int equipWpn(int slot)
{
    UnitOrgData *unit;
    int weapon_id;
    int weapon_variant;

    unit = dataUnitOrgGet(equipId);
    weapon_id = unit->weaponId[slot];
    weapon_variant = unit->weaponVariant[slot];
    if (equipPosX == 0) {
        valLR(&weapon_variant, 0x18, 0x27, 1);
    } else {
        valLR(&weapon_id, 0, 0x73, 1);
    }
    calcWpnEquipOrg(unit->charaId, slot, weapon_id, weapon_variant);
    if (PadData.half_2a & 0x20) {
        equipPosX = (equipPosX != 0) ? 0 : 9;
    }
    return 0;
}

/* Debug equip menu: cycles the attachment in equipment slot `slot` of the
   character selected by equipId. */
extern int equipAtt(int slot);

int equipAtt0(void) {
    return equipAtt(0);
}

int equipAtt1(void) {
    return equipAtt(1);
}

int equipAtt2(void) {
    return equipAtt(2);
}

int equipAtt(int slot) {
    UnitOrgData *unit;
    int attId;

    unit = dataUnitOrgGet(equipId);
    attId = unit->attachment[slot];
    valLR(&attId, 0, 0xB2, 1);
    calcAttEquipOrg(unit->charaId, slot, attId);
    return 0;
}

/* Debug equip menu: cycles the accessory in equipment slot `slot` of the
   character selected by equipId. */
extern int equipAcc(int slot);

int equipAcc0(void) {
    return equipAcc(0);
}

int equipAcc1(void) {
    return equipAcc(1);
}

int equipAcc2(void) {
    return equipAcc(2);
}

extern int calcAccEquipOrg(int charaId, int slot, int accId);

int equipAcc(int slot) {
    UnitOrgData *unit;
    int accId;

    unit = dataUnitOrgGet(equipId);
    accId = unit->accessory[slot];
    valLR(&accId, 0, 0xB4, 1);
    calcAccEquipOrg(unit->charaId, slot, accId);
    return 0;
}

extern int calcSpecEquipOrg(int charaId, int slot, int specId);
extern int slot; /* shared debug cursor for the spec/ether/skill equip screens */
extern int specPos[6]; /* screen cursor column per spec slot */

int equipSpec(void) {
    UnitOrgData *unit;
    int specId;

    unit = dataUnitOrgGet(equipId);
    specId = unit->special[slot];
    valLR(&specId, 0, 0x74, 1);
    calcSpecEquipOrg(unit->charaId, slot, specId);
    if (PadData.half_2a & 0x20) {
        slot += 1;
        if (slot >= 6) {
            slot = 0;
        }
        equipPosX = specPos[slot];
    }
    return 0;
}

extern int equipEther(int firstSlot, int lastSlot);

int equipEther0(void) {
    return equipEther(0, 3);
}

int equipEther1(void) {
    return equipEther(4, 7);
}

int equipEther2(void) {
    return equipEther(8, 0xB);
}

extern int calcEtherEquipOrg(int charaId, int slot, int etherId);
extern int etherPos[4]; /* screen cursor column per ether sub-slot */

int equipEther(int firstSlot, int lastSlot) {
    UnitOrgData *unit;
    int etherId;

    unit = dataUnitOrgGet(equipId);
    etherId = unit->ether[slot + firstSlot];
    valLR(&etherId, 0, 0xB3, 1);
    calcEtherEquipOrg(unit->charaId, slot + firstSlot, etherId);
    if (PadData.half_2a & 0x20) {
        slot += 1;
        if ((lastSlot - firstSlot) < slot) {
            slot = 0;
        }
        equipPosX = etherPos[slot];
    }
    return 0;
}

extern int calcSkillEquipOrg(int charaId, int slot, int skillId);
extern int skillPos[3]; /* screen cursor column per skill slot */

int equipSkill(void) {
    UnitOrgData *unit;
    int skillId;

    unit = dataUnitOrgGet(equipId);
    skillId = unit->skill[slot];
    valLR(&skillId, 0, 0x5B, 1);
    calcSkillEquipOrg(unit->charaId, slot, skillId);
    if (PadData.half_2a & 0x20) {
        slot += 1;
        if (slot >= 3) {
            slot = 0;
        }
        equipPosX = skillPos[slot];
    }
    return 0;
}

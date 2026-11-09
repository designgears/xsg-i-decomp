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

char *err = "(ERR)"; /* fallback string returned for an out-of-range debug table index */
/* debug menu's character name table */
char *nameTbl[0xC3] = {
    "NULL    ",
    "CHAOS   ",
    "KOSMOS  ",
    "SHION   ",
    "SHITAN  ",
    "ZIGGY   ",
    "MOMO    ",
    "JR      ",
    "CECILIA ",
    "CATHERINE",
    "VIRGIL  ",
    "MARY    ",
    "SHELLEY ",
    "NULL13  ",
    "NULL14  ",
    "NULL15  ",
    "NULL16  ",
    "VX_01   ",
    "VX_02   ",
    "VX_03   ",
    "VX_04   ",
    "VX_05   ",
    "VX_06   ",
    "AG_01   ",
    "AG_02   ",
    "AG_03   ",
    "AG_04   ",
    "AG_05   ",
    "AG_06   ",
    "UKN     ",
    "AGWS14  ",
    "AGWS15  ",
    "AGWS16  ",
    "HEI00   ",
    "GUNO001 ",
    "GUNO002 ",
    "GUNO003 ",
    "GUNO004 ",
    "GUNO005 ",
    "GUNO006 ",
    "GUNO007 ",
    "GUNO008 ",
    "GUNO009 ",
    "GUNO010 ",
    "GUNO011 ",
    "GUNO012 ",
    "GUNO013 ",
    "GUNO014 ",
    "GUNO015 ",
    "GUNO016 ",
    "GUNO017 ",
    "GUNO018 ",
    "GUNO019 ",
    "GUNO020 ",
    "GUNO021 ",
    "GUNO022 ",
    "GUNO023 ",
    "GUNO024 ",
    "GUNO025 ",
    "GUNO101 ",
    "GUNO102 ",
    "GUNO103 ",
    "GUNO114 ",
    "FEMA001 ",
    "FEMA002 ",
    "FEMA003 ",
    "FEMA004 ",
    "FEMA005 ",
    "FEMA006 ",
    "FERE001 ",
    "FERE002 ",
    "FERE003 ",
    "FERO001 ",
    "FESO001 ",
    "FESO002 ",
    "FESO003 ",
    "FESO004 ",
    "UTMA001 ",
    "UTMA002 ",
    "UTMA003 ",
    "UTMA004 ",
    "UTMA005 ",
    "UTMA006 ",
    "UTMA007 ",
    "UTMA008 ",
    "UTMA009 ",
    "UTMA010 ",
    "UTMA011 ",
    "UTMA012 ",
    "UTMA013 ",
    "UTMA014 ",
    "UTRE001 ",
    "UTRE002 ",
    "UTRE003 ",
    "UTRE004 ",
    "UTRE005 ",
    "UTRE006 ",
    "UTRO001 ",
    "UTRO002 ",
    "UTRO003 ",
    "UTRO004 ",
    "UTRO005 ",
    "UTRO006 ",
    "UTRO007 ",
    "UTSO001 ",
    "UTSO002 ",
    "UTSO003 ",
    "UTSO004 ",
    "UTSO005 ",
    "UTSO006 ",
    "UTSO007 ",
    "MONS001 ",
    "MONS002 ",
    "MONS003 ",
    "MONS004 ",
    "MONS005 ",
    "MONS006 ",
    "MONS007 ",
    "MONS008 ",
    "MONS009 ",
    "MONS010 ",
    "MONS011 ",
    "MONS012 ",
    "MONS013 ",
    "MONS014 ",
    "MONS015 ",
    "MONS016 ",
    "MONS017 ",
    "MONS018 ",
    "MONS019 ",
    "MONS020 ",
    "MONS021 ",
    "MONS022 ",
    "MONS023 ",
    "MONS024 ",
    "MONS025 ",
    "MONS026 ",
    "MONS027 ",
    "MONS028 ",
    "MONS029 ",
    "MONS030 ",
    "MONS031 ",
    "MONS032 ",
    "MONS033 ",
    "MONS034 ",
    "MONS035 ",
    "MONS036 ",
    "MONS037 ",
    "MONS038 ",
    "MONS039 ",
    "MONS040 ",
    "BOSS001 ",
    "BOSS002 ",
    "BOSS003 ",
    "BOSS004 ",
    "BOSS005 ",
    "BOSS006 ",
    "BOSS007 ",
    "BOSS008 ",
    "BOSS009 ",
    "BOSS010 ",
    "BOSS011 ",
    "BOSS012 ",
    "BOSS013 ",
    "BOSS014 ",
    "BOSS015 ",
    "BOSS016 ",
    "BOSS017 ",
    "BOSS018 ",
    "BOSS019 ",
    "BOSS020 ",
    "BOSS021 ",
    "BOSS022 ",
    "BOSS023 ",
    "BOSS024 ",
    "BOSS025 ",
    "BOSS026 ",
    "BOSS027 ",
    "BOSS028 ",
    "BOSS029 ",
    "BOSS030 ",
    "BOSS031 ",
    "BOSS032 ",
    "BOSS033 ",
    "BOSS103 ",
    "BOSS104 ",
    "BOSS120 ",
    "SKJ_JR1 ",
    "SKJ_JR2 ",
    "SKJ_MOMO1",
    "SKJ_SHION1",
    "SKJ_SHION2",
    "SKJ_SHION3",
    "SKJ_SHION4",
    "SKJ_KOSMOS1"
};

char *debugNameGet(int charaId) {
    if (charaId < 0xC3) {
        return nameTbl[charaId];
    }
    return err;
}

/* debug menu's weapon name table */
char *wpnTbl[0x74] = {
    "NULL    ",
    "CHA_GR01",
    "CHA_GR02",
    "CHA_GR03",
    "CHA_GR04",
    "CHA_GR05",
    "CHA_GR06",
    "CHA_GR07",
    "CHA_GR08",
    "CHA_GR09",
    "CHA_GR10",
    "CHA_WING",
    "KOS_HW01",
    "KOS_HW02",
    "KOS_HW03",
    "KOS_HW04",
    "KOS_HW05",
    "KOS_HW06",
    "KOS_BW01",
    "KOS_BW02",
    "KOS_BW03",
    "KOS_BW04",
    "KOS_BW05",
    "KOS_BW06",
    "MMO_RD01",
    "MMO_RD02",
    "MMO_RD03",
    "MMO_RD04",
    "MMO_RD05",
    "MMO_RD06",
    "MMO_RD07",
    "MMO_RD08",
    "MMO_RD09",
    "MMO_RD10",
    "MMO_RD11",
    "MMO_RD12",
    "MMO_RD13",
    "MMO_RD14",
    "MMO_RD15",
    "MMO_RD16",
    "MMO_RD17",
    "ZIG_SW01",
    "ZIG_BW01",
    "ZIG_BW02",
    "ZIG_BW03",
    "ZIG_BW04",
    "JR_HG01",
    "JR_HG02",
    "JR_HG03",
    "JR_HG04",
    "JR_HG05",
    "JR_HG06",
    "JR_HG07",
    "JR_HG08",
    "JR_HG09",
    "JR_HG10",
    "JR_HG11",
    "STN_SW01",
    "STN_SW02",
    "STN_SW03",
    "STN_SW04",
    "STN_SW05",
    "STN_SW06",
    "STN_SW07",
    "STN_SW08",
    "STN_SW09",
    "STN_SW10",
    "STN_SW11",
    "STN_SW12",
    "SON_MBOX",
    "SUT00   ",
    "AX00    ",
    "BD00    ",
    "BD01    ",
    "BDF00   ",
    "HG00    ",
    "HG01    ",
    "HMR00   ",
    "HMR01   ",
    "HMR02   ",
    "MG00    ",
    "MG01    ",
    "RF00    ",
    "RF01    ",
    "RF02    ",
    "BAM00   ",
    "BS00    ",
    "LSR00   ",
    "PBK00   ",
    "FL00    ",
    "GND00   ",
    "GLG00   ",
    "HB00    ",
    "MSP00   ",
    "MSP02   ",
    "BL00    ",
    "CB00    ",
    "EAC00   ",
    "BP00    ",
    "LGC00   ",
    "AMP02   ",
    "SHB00   ",
    "FSD00   ",
    "VSD00   ",
    "BBC00   ",
    "EAD00   ",
    "ECM01   ",
    "ECM02   ",
    "SMP01   ",
    "WIR00   ",
    "SOL00   ",
    "DEF00   ",
    "GLG00R  ",
    "HB00R   ",
    "MSP02R  ",
    "HEI     "
};

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

char *cmdStrTbl[] = {
    "FREE",
    "ATK",
    "ETHER",
    "ITEM",
    "GUARD",
    "AGWS",
    "MOVE",
    "STAND",
    "END",
    "DMG",
    "EVA",
    "DEF",
    "DEAD",
    "MOT",
    "TGTSET",
    "TURNCHA",
    "TURNPOS",
    "MOVECHA",
    "MOVEPOS",
    "MOVECHA_NORM",
    "MOVEPOS_NORM",
    "MOVECHA_RUN",
    "MOVEPOS_RUN",
    "MOVECHA_JUMP",
    "MOVEPOS_JUMP",
    "MOVECHA_HOVER",
    "MOVEPOS_HOVER",
    "MOVECHA_WARP",
    "MOVEPOS_WARP",
    "MOVECHA_JUMP2",
    "MOVEPOS_JUMP2",
    "MOVECHA_HOVER2",
    "MOVEPOS_HOVER2",
    "MOVECHA_FLOAT",
    "MOVEPOS_FLOAT",
    "MOVECHA_HOVER3",
    "MOVEPOS_HOVER3",
    "BTST",
    "BTED",
    "REC",
    "WAKE",
    "STAT",
    "READWAIT",
    "UNITCHANGE",
    "TURNCENTER",
    "AGWS2",
    "ESCAPE",
    "FADEIN",
    "FADEOUT"
};

char *thinkExpTbl[] = {
    "EQ",
    "NE",
    "GT",
    "GE",
    "LT",
    "LE"
};

char *regName[] = {
    "UNIT",
    "CMD",
    "ID",
    "TGT",
    "UNIT_ATK",
    "CMD_ATK",
    "TID_ATK",
    "PARA_ATK",
    "RRT",
    "PL_NUM",
    "EN_NUM",
    "CAM_MODE"
};

char *thinkStrTbl[] = {
    "EXIT",
    "GO",
    "GOSUB",
    "RETURN",
    "BRA",
    "ONGO",
    "ONGOSUB",
    "RSET",
    "RINC",
    "RDEC",
    "RADD",
    "RSUB",
    "RMUL",
    "RDIV",
    "RMOD",
    "R_AND",
    "R_OR",
    "R_XOR",
    "R_NOT",
    "R_NEG",
    "RRRND",
    "RPUSH",
    "RPOP",
    "TBLGET",
    "TBLSET",
    "RPUT",
    "PRINT",
    "MEMDUMP",
    "CMDPUTON",
    "CMDPUTOFF",
    "NULL",
    "NULL",
    "MSGPOS",
    "MSG",
    "THINKSET",
    "THINKSET2",
    "BOOST",
    "STATCHK",
    "LINECHK",
    "UNITPARA_GET",
    "UNITPARA_SET",
    "UNITTBL",
    "UNIT_CREATE",
    "UNIT_CHANGE",
    "ATKTBL",
    "ATKSET",
    "ATKTBL_SORT",
    "ATKTBL_SORT_REV",
    "ATKTBL_SEARCH",
    "TECPARA_GET",
    "MONSSETNO_GET",
    "PARTYSIZE_GET",
    "LIGHT_AMB",
    "LIGHT_COL",
    "LIGHT_DIR",
    "CAM_REF_MODE",
    "CAM_CAM_MODE",
    "CAM_PERS_MODE",
    "CAM_BANK_MODE",
    "CAM_REF_MOVE_MODE",
    "CAM_CAM_MOVE_MODE",
    "CAM_PERS_MOVE_MODE",
    "CAM_BANK_MOVE_MODE",
    "CAM_LOCK",
    "CAM_COPY",
    "CAM_SET_POS",
    "CAM_SET_ACT",
    "CAM_SET_ACT2",
    "CAM_SET_ACT3",
    "CAM_OFFS_POS",
    "CAM_OFFS_ANG",
    "CAM_MOVE",
    "CAM_SHAKE",
    "CAM_CENTER_LENGTH",
    "CAM_UNIT_HEIGHT",
    "CAM_COORD_GET",
    "CAM_BANK",
    "CAM_PERS",
    "CAM_TBL_SET",
    "CAM_MOVE_STOP",
    "EVENT",
    "WAIT_EVENT",
    "WAIT_CNT",
    "WAIT_CAM_MOVE",
    "WAIT_MSG",
    "VPAD_ENEBLE",
    "VPAD_DISABLE",
    "VPAD_SET",
    "MAP_MULSET",
    "HP_PER_GET",
    "EVENT_SLOT_GET",
    "THINK_CAM_EVENT",
    "CF_ENCOUNT_GET",
    "CF_EVENT_GET",
    "THINKNO_GET",
    "STAT_GET",
    "STAT_SET",
    "STAT_RESET",
    "DEAD_TIME"
};

int configSeq = 0;
int configPos = 0;
int statId = 1;
int equipId = 1;
int gainId = 1;
int equipPos = 0;
int equipPosX = 0;
int slot = 0;
int plMuteki = 0;
int enMuteki = 0;
int plIchigeki = 0;

extern int *unitCmdPtrGet(ObjectTask *unit);
extern int frameCntGet(void);
extern unsigned char *dataItmGet(int index);
extern int dataTecGet(int tecId);
extern int printf(const char *format, ...);

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
        printf("%6d: %1d)%s AP=%2d %s tid=%d para=%d\n", frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd], tid, para);
        break;
    case 15: case 17: case 19: case 21: case 23:
    case 25: case 27: case 29: case 31: case 33:
        printf("%6d: %1d)%s AP=%2d %s\n", frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd]);
        break;
    case 5: case 6: case 16: case 18: case 20: case 22:
    case 24: case 26: case 28: case 30: case 32: case 34:
        printf("%6d: %1d)%s AP=%2d %s pos=%d\n", frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd], tid);
        break;
    case 13:
        printf("%6d: %1d)%s AP=%2d %s mot=%d\n", frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd], tid);
        break;
    case 4: case 8: case 9: case 10: case 11: case 12:
    case 37: case 38: case 39: case 40: case 41: case 42:
    case 44: case 45: case 46: case 47: case 48:
        printf("%6d: %1d)%s AP=%2d %s\n", frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd]);
        break;
    case 14:
        if (p[4] == 3) {
            flag = dataItmGet(para)[8];
        } else {
            flag = *(unsigned char *)dataTecGet(tid);
        }
        printf("%6d: %1d)%s AP=%2d %s tid=%d tgt=%1d)%s %s\n", frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd], tid, unitNoGet(tgt),
               debugNameGet(calcUPGet(tgt)->charaId), (flag & 0x10) ? "ALL" : "");
        break;
    case 43:
        printf("%6d: %1d)%s AP=%2d %s pos=%d id=%d\n", frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
               calcUPGet(unit)->ap, cmdStrTbl[cmd], tid, p[2]);
        break;
    default:
        do {
            printf("%6d: %1d)%s AP=%2d ERROR=%d\n", frameCntGet(), unitNoGet(unit), debugNameGet(calcUPGet(unit)->charaId),
                   calcUPGet(unit)->ap, p[0]);
        } while (0);
        break;
    }
}

static char D_00A5AA40[0x10];

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
                sprintf(D_00A5AA40, "@R%02d", reg_index);
            } else {
                sprintf(D_00A5AA40, "@R%s", cmdStrTbl[CMD_STR_TBL_REG_NAMES + reg_index]);
            }
        } else {
            if (reg_index < DEBUG_THINK_NUMERIC_LIMIT) {
                sprintf(D_00A5AA40, "R%02d", reg_index);
            } else {
                sprintf(D_00A5AA40, "R%s", cmdStrTbl[CMD_STR_TBL_REG_NAMES + reg_index]);
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

void debugThinkPrint(int cmd, short pc, short jmp)
{
    int r, a, b, c;
    char *name;

    printf("%04X: %s ", pc, thinkStrTbl[cmd]);
    switch (cmd) {
    case 1:
    case 2:
        printf("%Xh\n", jmp);
        break;
    case 4:
        pc += 2;
        r = cmdNum(pc);
        pc += 2;
        b = cmdNum(pc);
        c = cmdNum(pc + 2);
        printf("%d %s %d -> %Xh\n", r, thinkExpTbl[b], c, jmp);
        break;
    case 5:
    case 6:
        a = cmdNum(pc + 2);
        printf("%d %Xh\n", a, jmp);
        break;
    case 7:
        pc += 2;
        r = thinkRegNo(pc);
        a = cmdNum(pc + 2);
        name = debugThinkRegName(r);
        printf("%s %d\n", name, a);
        break;
    case 8:
    case 9:
    case 18:
    case 19:
        pc += 2;
        printf("%s -> %d\n", debugThinkRegName(thinkRegNo(pc)), cmdNum(pc));
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
        printf("%s %d -> %d\n", name, a, jmp);
        break;
    case 21:
        a = cmdNum(pc + 2);
        printf("%d\n", a);
        break;
    case 22:
        r = thinkRegNo(pc + 2);
        jmp = thinkRegGet(r);
        name = debugThinkRegName(r);
        printf("%s -> %d\n", name, jmp);
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
        printf("%s %d %d -> %d\n", name, a, b, jmp);
        break;
    default:
        printf("\n");
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

 /* " WCT=%3d/%3d AGLPLUS=%3d ETHERDIS=%04X WPNATKADJ=%3d\n" */
extern int printf(const char *format, ...);


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
    printf("** UNIT STAT **\n");
    for (i = 0; i < count; i++) {
        if (units[i] != 0) {
            up = calcUPGet((ObjectTask *)units[i]);
            base = (unsigned char *)up + 0xC;
            printf("** %d) %s LV=%d HP=%d/%d EP=%d/%d\n", unitNoGet((ObjectTask *)units[i]), debugNameGet(up->charaId),
                   up->level, up->hp, up->maxHp, up->ep, up->maxEp);
            printf(" ST  ");
            for (j = 0; j < 8; j++) {
                printf("%04X ", ((unsigned short *)((unsigned char *)up + 0xAC))[j]);
            }
            printf("\n");
            printf(" STA ");
            for (j = 0; j < 8; j++) {
                printf("%04X ", ((unsigned short *)(base + 0xB0))[j]);
            }
            printf("\n");
            printf(" STP ");
            for (j = 0; j < 8; j++) {
                printf("%04X ", ((unsigned short *)((unsigned char *)up + 0x24))[j]);
            }
            printf("\n");
            printf(" STT ");
            for (j = 0; j < 8; j++) {
                p = base + j * 16 + 0xC0;
                for (k = 0; k < 16; k++) {
                    if (*p != 0) {
                        printf("%d ", *p);
                    }
                    p++;
                }
            }
            printf("\n");
        }
    }
    return 1;
}

int debugParaDisp(void)
{
    BattleUnit **units;
    DebugUnitPara *up;
    int *objStat;
    int count;
    int i;

    count = unitTblGet(2, &units);
    printf("** UNIT PARA **\n");
    for (i = 0; i < count; i++) {
        if (units[i] != 0) {
            up = calcUPGet((ObjectTask *)units[i]);
            printf("** %d) %s LV=%d HP=%d/%d EP=%d/%d\n", unitNoGet((ObjectTask *)units[i]), debugNameGet(up->charaId),
                   up->level, up->hp, up->maxHp, up->ep, up->maxEp);
            printf(" STR=%3d VIT=%3d EATK=%3d EDEF=%3d HIT=%3d EVA=%3d\n", up->str, up->vit, up->eatk, up->edef, up->hit, up->eva);
            printf(" AGL=%3d BC =%3d BP = %3d MVTYPE=%1d C_RATE=%3d\n", up->agility, up->bc, up->bp, up->moveKindId, up->cRate);
            objStat = ((ObjectTask *)units[i])->work;
            printf(" WEAK=%04X SPEC=%04X INFO=%04X OBJSTAT=%04X\n", up->weak, up->spec, up->flags, *objStat);
            printf(" WCT=%3d/%3d AGLPLUS=%3d ETHERDIS=%04X WPNATKADJ=%3d\n", up->waitCount, up->waitCountBase, up->turnBias,
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

int debugSpecDisp(void)
{
    BattleUnit **units;
    DebugUnitPara *up;
    DebugSpecSlot *sp;
    int count;
    int i;
    int j;

    count = unitTblGet(2, &units);
    printf("** SPEC PARA **\n");
    for (i = 0; i < count; i++) {
        if (units[i] != 0 && !(*(int *)((ObjectTask *)units[i])->work & 0x40)) {
            up = calcUPGet((ObjectTask *)units[i]);
            sp = (DebugSpecSlot *)(dataPlChaGet(up->charaId) + 0x48);
            printf("** %d) %s LV=%d HP=%d/%d EP=%d/%d\n", unitNoGet((ObjectTask *)units[i]), debugNameGet(up->charaId),
                   up->level, up->hp, up->maxHp, up->ep, up->maxEp);
            for (j = 0; j < 8; j++) {
                printf("%1d: tid=%3d spd=%d pow=%d agl=%d use=%d kill=%d\n", j, sp->id, sp->spd, sp->pow, sp->agl, sp->use, sp->kill);
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

#define DEBUG_CHARA_ID_MAX 0xC2
#define PAD_CIRCLE 0x0020
#define PAD_CROSS 0x0040
extern int configSeq, statId, equipId, gainId, equipPosX, slot;
static int monsSet, mapNo, cfEncount, cfEvent, D_00A5AA60;
typedef struct DbcOrg {
    short maxHp;               /* +0x00 */
    short maxEp;                /* +0x02 */
    short str;                  /* +0x04 */
    short vit;                  /* +0x06 */
    short eatk;                 /* +0x08 */
    short edef;                 /* +0x0A */
    signed char hit;            /* +0x0C */
    signed char eva;            /* +0x0D */
    signed char agility;        /* +0x0E */
    signed char moveKindId;     /* +0x0F */
    short atkRange;             /* +0x10 */
    short defRange;             /* +0x12 */
    signed char level;          /* +0x14 */
    signed char size;           /* +0x15 */
    signed char cRate;          /* +0x16 */
    signed char ecap;           /* +0x17 */
    unsigned short weak;        /* +0x18 */
    unsigned short spec;        /* +0x1A */
    unsigned short flags;       /* +0x1C */
    short weight;               /* +0x1E */
    signed char bRate;          /* +0x20 */
    signed char bNum;           /* +0x21 */
    unsigned char unmodeled_22[0x24 - 0x22];
    unsigned short protect[8];  /* +0x24 */
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
    unsigned char unmodeled_70[0x76 - 0x70];
    short norm[6];                /* +0x76 */
    short special[6];             /* +0x82 */
    short ether[12];              /* +0x8E */
    short skill[3];                /* +0xA6 */
} DbcOrg;
extern PadPrefix PadData;
typedef struct DebugGainRecord {
    int exp;                      /* +0x00: "EXP   =%8d" */
    unsigned short money;         /* +0x04: "MONEY =%8d" */
    unsigned char tp;             /* +0x06: "TP    =%4d" */
    unsigned char ep;             /* +0x07: "EP    =%4d" */
    unsigned char sp;             /* +0x08: "SP    =%4d" */
    unsigned char unmodeled_09;
    unsigned char layer[3];       /* +0x0A: "LAY =%3d" */
    unsigned char group[3];       /* +0x0D: "GRP =%3d" */
    unsigned char item[3];        /* +0x10: "ITEM =%3d" */
} DebugGainRecord;
typedef struct DebugGrowRecord {
    int exp;                      /* +0x00: "EXP      =%8d" */
    int nextExp;                  /* +0x04: "NEXT EXP =%8d" */
    short weaponOffset;           /* +0x08: "W OFS    =%4d" */
    short weaponCount;            /* +0x0A: "W CNT    =%4d" */
    int tp;                       /* +0x0C: "TP       =%4d" */
    int ep;                       /* +0x10: "EP       =%4d" */
    int sp;                       /* +0x14: "SP       =%4d" */
    short grow[8];                /* +0x18: "GROW PARA=%4d %4d %4d %4d" x2 */
} DebugGrowRecord;
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern void xglSleep(void);
extern void dataThinkLoad(int thinkNo);
extern int cfEncountGet(void);
extern int cfEventGet(void);
extern int mapNoGet(void);
extern int monsSetNoGet(void);
extern int thinkNoGet(void);
extern int plMutekiGet(void);
extern int enMutekiGet(void);
extern int plIchigekiGet(void);
extern int cameraFlagGet(void);
extern void debugItemSet(void);
extern void debugEtherSet(void);
extern void debugSkillSet(void);
extern int debugEquSet(void);
extern UnitOrgData *dataUnitInitGet(int chrNo);
extern short *dataNormInitGet(int chrNo);
extern short *dataEtherInitGet(int chrNo);
extern DebugGainRecord *dataGainGet(int gainId);
extern int valUD(int *value, int min, int max, int step);
extern int configPos;
extern int equipPos;
extern int (*configCmdTbl[21])(int *);
extern int (*equipCmdTbl[])();
const char *posName[10] = {
    "NULL",
    "P1",
    "P2",
    "P3",
    "P4",
    "P5",
    "P6",
    "A1",
    "A2",
    "A3"
};
const char *szName[5] = {
    "SS",
    "S",
    "M",
    "L",
    "LL"
};

void debugBattleConfig(void)
{
    DebugPartyUnit *party;
    DbcOrg *rec;
    DebugGainRecord *gain;
    DebugGrowRecord *grow;
    short *norm;
    short *ether;
    int cmdCount;
    int lastCmd;
    int equipCount;
    int cmdResult;
    int hp;
    int row;
    int h = 9;
    int x = 12;
    int w = 9;
    int column;
    int ep;
    int i;
    int y;
    int *pos;

    party = plUnitTblGet();
    for (cmdCount = 0; configCmdTbl[cmdCount] != 0; cmdCount++) {
    }
    mapNo = mapNoGet();
    monsSet = monsSetNoGet();
    cfEncount = cfEncountGet();
    cfEvent = cfEventGet();
    D_00A5AA60 = thinkNoGet();
    debugItemSet();
    debugEtherSet();
    debugSkillSet();
    debugEquSet();
    lastCmd = cmdCount - 1;
    for (;;) {
        if (configSeq == 0) {
            row = 0;
            x = 12;
            valUD(&configPos, 0, lastCmd, 1);
            {
                int cursorY = h * (configPos + 1) + x;
                float word = *(float *)party;

                *(float *)party = word;
                xglFontDebugPrintf(12, cursorY, ">");
            }
            { union { int i; float f; } *view = (void *)configCmdTbl; float f = view->f; view->f = f; }
            pos = &configPos;
            cmdResult = configCmdTbl[*pos](pos);
            pos = 0;
            pos = &configPos;
            xglFontDebugPrintf(12, h * row++ + x, "\013 --BATTLE CONFIG 2001/10/01--");
            xglFontDebugPrintf(12, h * row++ + x, "\014\200\000\000 BATTLE START");
            xglFontDebugPrintf(12, h * row++ + x, "\013 THINK= %2d", D_00A5AA60);
            xglFontDebugPrintf(12, h * row++ + x, "\013 MONSTER SET= %2d", monsSet);
            xglFontDebugPrintf(12, h * row++ + x, "\013 MAP= %2d", mapNo);
            xglFontDebugPrintf(12, h * row++ + x, "\013 PLAYER1= %s", debugNameGet(party[0].player));
            xglFontDebugPrintf(12, h * row++ + x, "\013 PLAYER2= %s", debugNameGet(party[1].player));
            xglFontDebugPrintf(12, h * row++ + x, "\013 PLAYER3= %s", debugNameGet(party[2].player));
            xglFontDebugPrintf(12, h * row++ + x, "\013 PLAYER1 POS= %s", posName[party[0].position]);
            xglFontDebugPrintf(12, h * row++ + x, "\013 PLAYER2 POS= %s", posName[party[1].position]);
            xglFontDebugPrintf(12, h * row++ + x, "\013 PLAYER3 POS= %s", posName[party[2].position]);
            xglFontDebugPrintf(12, h * row++ + x, "\013 CAMERA FLAG= %1d", cameraFlagGet());
            xglFontDebugPrintf(12, h * row++ + x, "\013 PL MUTEKI= %1d", plMutekiGet());
            xglFontDebugPrintf(12, h * row++ + x, "\013 EN MUTEKI= %1d", enMutekiGet());
            xglFontDebugPrintf(12, h * row++ + x, "\013 PL ICHIGEKI= %1d", plIchigekiGet());
            xglFontDebugPrintf(12, h * row++ + x, "\013 CF ENCOUNT= %4x", cfEncount);
            xglFontDebugPrintf(12, h * row++ + x, "\013 CF EVENT= %4x", cfEvent);
            xglFontDebugPrintf(12, h * row++ + x, "\014\200\000\000 STAT = %s", debugNameGet(statId));
            xglFontDebugPrintf(12, h * row++ + x, "\014\200\000\000 EQUIP= %s", debugNameGet(equipId));
            xglFontDebugPrintf(12, h * row++ + x, "\014\200\000\000 GAIN = %s", debugNameGet(gainId));
            xglFontDebugPrintf(12, h * row++ + x, "\014\200\000\000 YADOYA");
            if (cmdResult == 1 && (PadData.half_2a & PAD_CIRCLE)) {
                goto done;
            }
            pos = 0;
            if (PadData.half_2a & PAD_CROSS) {
                goto done;
            }
        } else if (configSeq == 1) {
            row = 0;
            valLR(&statId, 1, DEBUG_CHARA_ID_MAX, 1);
            if (statId < 0x21) {
                rec = dataUnitOrgGet(statId);
                hp = rec->hp;
                ep = rec->ep;
                norm = rec->norm;
                ether = rec->ether;
            } else {
                rec = dataUnitInitGet(statId);
                hp = rec->maxHp;
                ep = rec->maxEp;
                norm = dataNormInitGet(statId);
                ether = dataEtherInitGet(statId);
            }
            xglFontDebugPrintf(12, h * row++ + 12, "\013 --STAT--");
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ID=%s", debugNameGet(statId));
            xglFontDebugPrintf(12, h * row++ + 12, "\013 HP   =%4d MAX HP=%4d", hp, rec->maxHp);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 EP   =%4d MAX EP=%4d", ep, rec->maxEp);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 STR  =%4d VIT   =%4d", rec->str, rec->vit);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 EATK =%4d EDEF  =%4d", rec->eatk, rec->edef);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 HIT  =%4d EVA   =%4d", rec->hit, rec->eva);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 AGL  =%4d MVTYPE=%4d", rec->agility, rec->moveKindId);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 AAREA=%4d DAREA =%4d", rec->atkRange, rec->defRange);
            { const char *sz = szName[rec->size]; xglFontDebugPrintf(12, h * row++ + 12, "\013 LV   =%4d SIZE  =%s", rec->level, sz); }
            xglFontDebugPrintf(12, h * row++ + 12, "\013 COUNT=%4d ECAP  =%4d", rec->cRate, rec->ecap);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 WEAK =%4x SPEC  =%4x", rec->weak, rec->spec);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 INFO =%4x WEIGHT=%4d", rec->flags, rec->weight);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 BRATE=%4d BNUM  =%4d", rec->bRate, rec->bNum);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 NORM =%4d %4d %4d", norm[0], norm[1], norm[2]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013       %4d %4d %4d", norm[3], norm[4], norm[5]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ETHER=%4d %4d %4d %4d", ether[0], ether[1], ether[2], ether[3]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013       %4d %4d %4d %4d", ether[4], ether[5], ether[6], ether[7]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013       %4d %4d %4d %4d", ether[8], ether[9], ether[10], ether[11]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 PROTC=%4x %4x %4x %4x", rec->protect[0], rec->protect[1],
                               rec->protect[2], rec->protect[3]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013       %4x %4x %4x %4x", rec->protect[4], rec->protect[5],
                               rec->protect[6], rec->protect[7]);
            if (PadData.half_2a & PAD_CROSS) {
                configSeq = 0;
            }
        } else if (configSeq == 2) {
            row = 0;
            for (equipCount = 0; equipCmdTbl[equipCount] != 0; equipCount++) {
            }
            if (valUD(&equipPos, 0, equipCount - 1, 1) == 1) {
                slot = 0;
                equipPosX = 0;
            }
            column = w;
            xglFontDebugPrintf(column * equipPosX + 12, h * (equipPos + 1) + 12, ">");
            equipCmdTbl[equipPos]();
            rec = dataUnitOrgGet(equipId);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 --EQUIP--");
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ID=%s", debugNameGet(equipId));
            xglFontDebugPrintf(12, h * row++ + 12, "\013 AGWS  =%s", debugNameGet(rec->agwsId));
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ENGINE=%3d", rec->engineId);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 FRAME =%3d", rec->frameId);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 WPN0  =%3d WID =%s", rec->weaponVariant[0],
                               debugWpnGet(rec->weaponId[0]));
            xglFontDebugPrintf(12, h * row++ + 12, "\013 WPN1  =%3d WID =%s", rec->weaponVariant[1],
                               debugWpnGet(rec->weaponId[1]));
            xglFontDebugPrintf(12, h * row++ + 12, "\013 WPN2  =%3d WID =%s", rec->weaponVariant[2],
                               debugWpnGet(rec->weaponId[2]));
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ATT0  =%3d", rec->attachment[0]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ATT1  =%3d", rec->attachment[1]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ATT2  =%3d", rec->attachment[2]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ACC0  =%3d", rec->accessory[0]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ACC1  =%3d", rec->accessory[1]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ACC2  =%3d", rec->accessory[2]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 SPEC  =%3d %3d %3d %3d %3d %3d", rec->special[0], rec->special[1],
                               rec->special[2], rec->special[3], rec->special[4], rec->special[5]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ETH0  =%4d %4d %4d %4d", rec->ether[0], rec->ether[1],
                               rec->ether[2], rec->ether[3]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ETH4  =%4d %4d %4d %4d", rec->ether[4], rec->ether[5],
                               rec->ether[6], rec->ether[7]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ETH8  =%4d %4d %4d %4d", rec->ether[8], rec->ether[9],
                               rec->ether[10], rec->ether[11]);
            xglFontDebugPrintf(12, h * row++ + 12, "\013 SKILL =%4d %4d %4d", rec->skill[0], rec->skill[1],
                               rec->skill[2]);
            if (PadData.half_2a & PAD_CROSS) {
                configSeq = 0;
            }
        } else if (configSeq == 3) {
            valLR(&gainId, 1, DEBUG_CHARA_ID_MAX, 1);
            row = 0;
            xglFontDebugPrintf(12, h * row++ + 12, "\013 --GAIN--");
            xglFontDebugPrintf(12, h * row++ + 12, "\013 ID=%s", debugNameGet(gainId));
            if (gainId >= 0x21) {
                gain = dataGainGet(gainId);
                xglFontDebugPrintf(12, h * row++ + 12, "\013 EXP   =%8d", gain->exp);
                xglFontDebugPrintf(12, h * row++ + 12, "\013 MONEY =%8d", gain->money);
                xglFontDebugPrintf(12, h * row++ + 12, "\013 TP    =%4d", gain->tp);
                xglFontDebugPrintf(12, h * row++ + 12, "\013 EP    =%4d", gain->ep);
                xglFontDebugPrintf(12, h * row++ + 12, "\013 SP    =%4d", gain->sp);
                for (i = 0; i < 3; i++) {
                    xglFontDebugPrintf(12, h * row++ + 12, "\013 LAY =%3d GRP =%3d ITEM =%3d", gain->layer[i],
                                       gain->group[i], gain->item[i]);
                }
            } else {
                if (gainId < 0x11) {
                    grow = (DebugGrowRecord *)dataPlChaGet(gainId);
                    xglFontDebugPrintf(12, h * row++ + 12, "\013 EXP      =%8d", grow->exp);
                    xglFontDebugPrintf(12, h * row++ + 12, "\013 NEXT EXP =%8d", grow->nextExp);
                    xglFontDebugPrintf(12, h * row++ + 12, "\013 W OFS    =%4d", grow->weaponOffset);
                    xglFontDebugPrintf(12, h * row++ + 12, "\013 W CNT    =%4d", grow->weaponCount);
                    xglFontDebugPrintf(12, h * row++ + 12, "\013 TP       =%4d", grow->tp);
                    xglFontDebugPrintf(12, h * row++ + 12, "\013 EP       =%4d", grow->ep);
                    xglFontDebugPrintf(12, h * row++ + 12, "\013 SP       =%4d", grow->sp);
                    xglFontDebugPrintf(12, h * row++ + 12, "\013 GROW PARA=%4d %4d %4d %4d", grow->grow[0], grow->grow[1],
                                       grow->grow[2], grow->grow[3]);
                    xglFontDebugPrintf(12, h * row++ + 12, "\013          =%4d %4d %4d %4d", grow->grow[4], grow->grow[5],
                                       grow->grow[6], grow->grow[7]);
                }
            }
            if (PadData.half_2a & PAD_CROSS) {
                configSeq = 0;
            }
        }
        xglSleep();
    }
done:
    dataThinkLoad(D_00A5AA60);
}

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

/* debug battle-config menu: one handler per config row, 0-terminated */
int (*configCmdTbl[21])(int *) = {
    (int (*)(int *))configStart,      (int (*)(int *))configThink,
    (int (*)(int *))configMonsSet,    (int (*)(int *))configMap,
    (int (*)(int *))configPlayer1,    (int (*)(int *))configPlayer2,
    (int (*)(int *))configPlayer3,    (int (*)(int *))configPlPos1,
    (int (*)(int *))configPlPos2,     (int (*)(int *))configPlPos3,
    (int (*)(int *))configCamera,     (int (*)(int *))configPlMuteki,
    (int (*)(int *))configEnMuteki,   (int (*)(int *))configPlIchigeki,
    (int (*)(int *))configCFEncount,  (int (*)(int *))configCFEvent,
    (int (*)(int *))configStat,       (int (*)(int *))configEqu,
    (int (*)(int *))configGain,       (int (*)(int *))configYadoya,
    0,
};

/* debug equip menu: one handler per equip row, 0-terminated */
int (*equipCmdTbl[])() = {
    equipChar, equipAgws, equipEngine, equipFrame,
    equipWpn0, equipWpn1, equipWpn2,
    equipAtt0, equipAtt1, equipAtt2,
    equipAcc0, equipAcc1, equipAcc2,
    equipSpec, equipEther0, equipEther1, equipEther2, equipSkill,
    0,
};

int specPos[6] = { 0, 9, 12, 15, 18, 21 };
int etherPos[4] = { 0, 10, 14, 18 };
int skillPos[3] = { 0, 10, 14 };

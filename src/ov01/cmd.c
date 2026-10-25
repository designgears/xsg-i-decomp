/*
 * OV01 original TU 6: 0x00a1d348..0x00a22628 (136 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov01/calc.h"
#include "ov01/obj.h"
#include "cmd.h"

typedef union {
    unsigned char byteValue;
    short halfwordValue;
} CmdTecparaValue;

extern CmdTecparaValue *cmdTecparaSub(int tec, short category, short *valueType);

extern void unitCmdListSet(ObjectTask *unit, int *count);

void thinkSysInit(void)
{
    memset(processBuf, 0, sizeof(processBuf));
    memset(context, 0, sizeof(context));
}

void thinkTopSet(int dataTop)
{
    pDataTop = dataTop;
}

void thinkContextSet(int context)
{
    pContext = context;
}

int thinkAdrGet(short offset)
{
    /* The offset is rounded down to an even byte count before it is added. */
    return pDataTop + (unsigned int)offset / 2 * 2;
}

extern const char D_00A46050[];

int thinkProcessAdd(ThinkProcess *proc)
{
    ThinkProcess *p = (ThinkProcess *)processBuf;
    int i;

    for (i = 0; i < 16; i++) {
        if (p->dataTop == 0) {
            proc->waitActive = 0;
            *p = *proc;
            return i + 1;
        }
        p = (ThinkProcess *)((unsigned char *)p + 0x24);
    }
    printf(D_00A46050);
    return 0;
}

int thinkProcessDel(int *slot)
{
    if (*slot != 0) {
        *slot = 0;
        return 1;
    }
    return 0;
}

/*
 * thinkProcessChk returns whether AI process slot 1-16 (matching
 * thinkProcessKindChk's sixteen-entry processBuf table) has a live entry
 * (dataTop nonzero); an out-of-range slot returns 0.
 */
int thinkProcessChk(int slot)
{
    ThinkProcess *proc;

    if ((unsigned int)(slot - 1) >= 16) {
        return 0;
    }
    proc = (ThinkProcess *)(processBuf + (slot - 1) * 0x24);
    return proc->dataTop != 0;
}

int thinkProcessKindChk(int kind)
{
    ThinkProcess *proc = (ThinkProcess *)processBuf;
    int count = 0;
    int i;

    for (i = 0; i < 16; i++) {
        if (proc->dataTop != 0 && proc->kind == kind) {
            count++;
        }
        proc = (ThinkProcess *)((unsigned char *)proc + 0x24);
    }
    return count;
}

int thinkProcessExecSub(ThinkProcess *proc);

int thinkProcessExec(void)
{
    ThinkProcess *proc;
    int i;

    dataVPadSet(0);
    proc = (ThinkProcess *)processBuf;
    for (i = 0; i < 16; i++) {
        if (proc->dataTop != 0) {
            thinkProcessExecSub(proc);
        }
        proc = (ThinkProcess *)((unsigned char *)proc + 0x24);
    }
    return 1;
}

extern int thinkExecSub(ThinkProcess *proc);
extern const char D_00A46068[];

int thinkProcessExecSub(ThinkProcess *proc)
{
    int status;

    thinkTopSet(proc->dataTop);
    thinkContextSet(proc->context);
    do {
        status = thinkExecSub(proc);
    } while (status == 0);
    if (status == 1) {
        thinkProcessDel(&proc->dataTop);
    } else if (status != 2) {
        printf(D_00A46068, status);
    }
    return 1;
}

extern int msgPosX;
extern int msgPosY;
extern int mapNoGet(void);
extern const char D_00A46088[];

void thinkInit(void)
{
    int monsSetNo;
    int mapNo;

    thinkSysInit();
    pThinkTop = dataThinkAdrGet();
    thinkTopSet(pThinkTop);
    pMonsSetTop = (MonsTblEntry *)thinkAdrGet(*(short *)pThinkTop);
    cmdPutFlag = 0;
    msgPosX = 100;
    msgPosY = 240;
    pMsgObj = 0;
    camMode = 0;
    persMode = 3;
    bankMode = 2;
    monsSetNo = monsSetNoGet();
    if (monsSetNo >= thinkMonsTblNumGet() || monsSetNo < 0) {
        printf(D_00A46088, monsSetNo);
        monsSetNoSet(0);
    }
    mapNo = *(short *)pMonsSetTop[monsSetNoGet()].unmodeled_2;
    if (mapNo == 0) {
        mapNo = mapNoGet();
    }
    mapNoSet(mapNo);
}

extern short *pCamSetTop;
extern int mapNoGet(void);
extern void *dataMapCameraAdrGet(int mapNo);

void camInit(void)
{
    pCamTop = dataMapCameraAdrGet(mapNoGet());
    thinkTopSet((int)pCamTop);
    pCamSetTop = (short *)thinkAdrGet(*(short *)pCamTop);
    camTblIdx = 0;
}

int thinkMonsTblNumGet(void)
{
    MonsTblEntry *entry = pMonsSetTop;
    int count = 0;

    if (entry->script_offset != 0) {
        do {
            entry++;
            count++;
        } while (entry->script_offset != 0);
    }
    return count;
}

int thinkMonsTblGet(int monsSetNo)
{
    thinkTopSet(pThinkTop);
    return thinkAdrGet(pMonsSetTop[monsSetNo].script_offset);
}

extern short *pCamSetTop;

int thinkCamTblNumGet(void)
{
    short *entry = pCamSetTop;
    int count = 0;

    if (*entry != 0) {
        do {
            entry++;
            count++;
        } while (*entry != 0);
    }
    return count;
}

extern int camExec(short scriptOffset);
extern int cameraFlagGet(void);
extern const char D_00A460A0[];
extern unsigned char D_00A59AF8;

int camInitExec(void)
{
    short script = pCamTop->initScript;

    thinkContextSet((int)&D_00A59AF8);
    if (script != 0) {
        thinkRegSet(0x801B, cameraFlagGet() & 0xFFFF);
        printf(D_00A460A0, script);
        return camExec(script);
    }
    return 1;
}

extern const char D_00A460B8[];
typedef struct {
    int type;
    unsigned short arg;
    short pad;
    ObjectTask *unit;
    ObjectTask *target;
} CamEvent;

int camEventExec(CamEvent *ev)
{
    short idx;
    int script;

    thinkTopSet((int)pCamTop);
    idx = pCamSetTop[camTblIdx];
    thinkContextSet((int)&D_00A59AF8);
    if (idx != 0) {
        while (1) {
            short type = *(short *)thinkAdrGet(idx);
            if (type == 0) {
                return 1;
            }
            if (type == ev->type) {
                script = *(short *)thinkAdrGet(idx + 2);
                break;
            }
            idx += 4;
        }
        {
        int t = ev->type;
        switch (t) {
        case 2: case 3: case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 20:
            thinkRegSet(0x8010, unitNoGet(ev->unit) & 0xFFFF);
            thinkRegSet(0x8013, unitNoGet(ev->target) & 0xFFFF);
            break;
        case 1: case 5: case 13: case 14: case 15: case 16: case 17: case 19: case 23: case 24: case 25:
            thinkRegSet(0x8010, unitNoGet(ev->unit) & 0xFFFF);
            break;
        }
        }
        thinkRegSet(0x8012, ev->arg);
        thinkRegSet(0x8019, unitLiveNumGet(0) & 0xFFFF);
        thinkRegSet(0x801A, unitLiveNumGet(1) & 0xFFFF);
        thinkRegSet(0x801B, cameraFlagGet() & 0xFFFF);
        printf(D_00A460B8, ev->type, script);
        return camExec(script);
    }
    return 1;
}

extern const char D_00A46148[];

int thinkInitExec(void)
{
    short script = pMonsSetTop[monsSetNoGet()].initScript;

    thinkContextSet((int)context);
    if (script != 0) {
        printf(D_00A46148, script);
        return thinkExec(script);
    }
    return 1;
}

int thinkTurnStartExec(ObjectTask *unit)
{
    short script = pMonsSetTop[monsSetNoGet()].turnStartScript;

    thinkContextSet((int)context);
    if (script != 0) {
        thinkRegSet(0x8010, unitNoGet(unit) & 0xFFFF);
        thinkRegSet(0x8019, unitLiveNumGet(0) & 0xFFFF);
        thinkRegSet(0x801A, unitLiveNumGet(1) & 0xFFFF);
        printf(D_00A46160, script);
        return thinkExec(script);
    }
    return 1;
}

int thinkTurnEndExec(ObjectTask *unit)
{
    short script = pMonsSetTop[monsSetNoGet()].turnEndScript;

    thinkContextSet((int)context);
    if (script != 0) {
        thinkRegSet(0x8010, unitNoGet(unit) & 0xFFFF);
        thinkRegSet(0x8019, unitLiveNumGet(0) & 0xFFFF);
        thinkRegSet(0x801A, unitLiveNumGet(1) & 0xFFFF);
        printf(D_00A46180, script);
        return thinkExec(script);
    }
    return 1;
}

int thinkUnitAtkExec(ObjectTask *unit)
{
    AtkTblEntry *atkTbl = *(AtkTblEntry **)((char *)calcUPGet(unit) + CALC_UNIT_PARAM_ATK_TBL_OFF);
    short script = atkTbl->script_offset;

    thinkContextSet((int)context);
    if (script != 0) {
        pThinkUnit = unit;
        thinkRegSet(0x8010, unitNoGet(unit) & 0xFFFF);
        thinkRegSet(0x8011, 1);
        thinkRegSet(0x8012, 0);
        thinkRegSet(0x8013, 0);
        thinkRegSet(0x8019, unitLiveNumGet(0) & 0xFFFF);
        thinkRegSet(0x801A, unitLiveNumGet(1) & 0xFFFF);
        printf(D_00A46198, unitNoGet(unit), script);
        return thinkExec(script);
    }
    return 1;
}

int thinkUnitDmgExec(ObjectTask *unit, UnitDmgInfo *info)
{
    AtkTblEntry *atkTbl = *(AtkTblEntry **)((char *)calcUPGet(unit) + CALC_UNIT_PARAM_ATK_TBL_OFF);
    short script = atkTbl->dmgScript;

    thinkContextSet((int)context);
    if (script != 0) {
        pThinkUnit = unit;
        thinkRegSet(0x8010, unitNoGet(unit) & 0xFFFF);
        thinkRegSet(0x8014, unitNoGet(info->attacker) & 0xFFFF);
        thinkRegSet(0x8015, info->reg8015);
        thinkRegSet(0x8016, info->reg8016);
        thinkRegSet(0x8017, info->reg8017);
        thinkRegSet(0x8019, unitLiveNumGet(0) & 0xFFFF);
        thinkRegSet(0x801A, unitLiveNumGet(1) & 0xFFFF);
        printf(D_00A461B8, unitNoGet(unit), script);
        return thinkExec(script);
    }
    return 1;
}

extern int thinkProcessAdd(ThinkProcess *proc);

int thinkExec(short scriptOffset)
{
    ThinkProcess proc;

    proc.dataTop = pThinkTop;
    proc.pc = scriptOffset;
    proc.context = pContext;
    proc.kind = 0;
    thinkProcessAdd(&proc);
    return 1;
}

int camExec(short scriptOffset)
{
    ThinkProcess proc;

    if (cameraFlagGet() != 0) {
        proc.dataTop = (int)pCamTop;
        proc.pc = scriptOffset;
        proc.context = pContext;
        proc.kind = 1;
        thinkProcessAdd(&proc);
    }
    return 1;
}

int thinkExecChk(void)
{
    return thinkProcessKindChk(0) == 0;
}

extern int (*cmdTbl[])(ThinkProcess *proc, int cmd);
extern char D_00A461D8[];
extern void debugThinkPrint(short cmd, short pc, short newPc);

int thinkExecSub(ThinkProcess *proc)
{
    int cmd;
    short pc;
    int result;

    cmd = *(short *)thinkAdrGet(proc->pc);
    if ((unsigned int)cmd < 0x63) {
        pc = proc->pc;
        proc->pc += 2;
        result = cmdTbl[cmd](proc, cmd);
        if (result == 2) {
            proc->pc = pc;
            proc->waitActive = 1;
        } else {
            if (cmdPutFlag == 1) {
                debugThinkPrint(cmd, pc, proc->pc);
            }
            proc->waitActive = 0;
        }
    } else {
        printf(D_00A461D8);
        result = 1;
    }
    return result;
}

int regChk(int operand)
{
    unsigned short reg = operand;

    if (!(reg & 0x8000)) {
        printf(D_00A461E8, reg);
        return 0;
    }
    return 1;
}

short thinkRegNo(short pc)
{
    short *reg_address = (short *)thinkAdrGet(pc);
    short reg = *reg_address;

    ((int (*)(int, int))regChk)(reg & 0xFFFF, pc);
    return reg;
}

int thinkValSet(int value)
{
    value &= 0x7FFF;

    if (value & 0x4000) {
        value |= ~0x7FFF;
    }
    return value;
}

int thinkRegGet(int regIndex)
{
    unsigned short reg = regIndex;
    short *value_address = (short *)(pContext + ((reg & 0x3FFF) * 2));
    unsigned int value = (unsigned int)(int)*value_address;

    if (reg & 0x4000) {
        value_address = (short *)thinkAdrGet(value);
        value = (unsigned int)(int)*value_address;
    }
    value = (unsigned int)thinkValSet((short)value);
    return (short)value;
}

void thinkRegSet(unsigned short r, unsigned short value)
{
    unsigned short flag = r;
    int v = value & 0x7FFF;
    r &= 0x3FFF;
    if (flag & 0x4000) {
        int a = thinkAdrGet(*(short *)(pContext + (r << 1)));
        *(short *)a = v;
    } else {
        *(short *)(pContext + (r << 1)) = v;
    }
}

int cmdReg(short pc)
{
    unsigned short *reg_address = (unsigned short *)thinkAdrGet(pc);
    unsigned short reg = *reg_address;
    int register_value;
    int result;

    ((int (*)(int, int))regChk)(reg, pc);
    result = (register_value = thinkRegGet(reg));
    return result;
}

int cmdNum(short pc)
{
    short *value_address = (short *)thinkAdrGet(pc);
    short value = *value_address;
    int result;

    /* Bit 0x8000 marks a register operand; anything else is a literal. */
    if (value & 0x8000) {
        result = cmdReg(pc);
    } else {
        result = thinkValSet(value);
    }
    return result;
}

int cmdExit(void)
{
    return 1;
}

int cmdGo(ThinkProcess *proc)
{
    proc->pc = cmdNum(proc->pc);
    return 0;
}

extern const char D_00A461F8[];

int cmdGosub(ThinkProcess *proc)
{
    int ctx = pContext;

    if (*(int *)(ctx + 0xA0) < 16) {
        int *sp = (int *)(ctx + 0xA0);
        *(short *)(ctx + *sp * 2 + 0x80) = (unsigned short)proc->pc + 2;
        (*sp)++;
    } else {
        printf(D_00A461F8);
        return 1;
    }
    proc->pc = cmdNum(proc->pc);
    return 0;
}

extern const char D_00A46210[];

int cmdReturn(ThinkProcess *proc)
{
    int ctx = pContext;

    if (*(int *)(ctx + 0xA0) > 0) {
        int sp = *(int *)(ctx + 0xA0) - 1;
        *(int *)(ctx + 0xA0) = sp;
        proc->pc = *(short *)(ctx + sp * 2 + 0x80);
    } else {
        printf(D_00A46210);
        return 1;
    }
    return 0;
}

int cmdBra(ThinkProcess *proc)
{
    int left;
    int relation;
    int right;
    int comparison;
    int branch;

    left = cmdNum(proc->pc);
    proc->pc += 2;

    relation = cmdNum(proc->pc);
    proc->pc += 2;

    right = cmdNum(proc->pc);
    proc->pc += 2;

    branch = 0;

    switch (relation) {
    case 0:
        if (left == right) {
            branch = 1;
        }
        break;
    case 1:
        if (left != right) {
            branch = 1;
        }
        break;
    case 2:
        comparison = right < left;
        if (comparison) {
            branch = 1;
        }
        break;
    case 3:
        if (left >= right) {
            branch = 1;
        }
        break;
    case 4:
        comparison = left < right;
        if (comparison) {
            branch = 1;
        }
        break;
    case 5:
        comparison = right < left;
        branch = !comparison;
        break;
    }

    if (branch == 1) {
        proc->pc = cmdNum(proc->pc);
    } else {
        proc->pc += 2;
    }
    return 0;
}

extern const char D_00A46248[];

int cmdOngo(ThinkProcess *proc)
{
    int n = cmdNum(proc->pc);
    int v;

    proc->pc = (unsigned short)proc->pc + 2;
    v = cmdNum(proc->pc);
    proc->pc = (unsigned short)proc->pc + 2;
    if (v < n) {
        printf(D_00A46248);
        return 1;
    }
    proc->pc = cmdNum((short)(proc->pc + n * 2));
    return 0;
}

extern const char D_00A46258[];
extern const char D_00A46268[];

int cmdOngosub(ThinkProcess *proc)
{
    int n = cmdNum(proc->pc);
    int idx;
    int ctx;

    proc->pc += 2;
    idx = cmdNum(proc->pc);
    proc->pc += 2;
    if (idx < n) {
        printf(D_00A46258);
        return 1;
    }
    ctx = pContext;
    if (*(int *)(ctx + 0xA0) < 16) {
        int *sp = (int *)(ctx + 0xA0);
        *(short *)(ctx + *sp * 2 + 0x80) = proc->pc + idx * 2;
        (*sp)++;
    } else {
        printf(D_00A46268);
        return 1;
    }
    proc->pc = cmdNum(proc->pc + n * 2);
    return 0;
}

int cmdRset(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int value;

    proc->pc += 2;
    value = cmdNum(proc->pc) & 0xFFFF;
    proc->pc += 2;
    thinkRegSet(regNo & 0xFFFF, value);
    return 0;
}

extern const char D_00A46280[];

int cmdRmath1(ThinkProcess *proc)
{
    short op = *(short *)thinkAdrGet(proc->pc - 2);
    int regNo = thinkRegNo(proc->pc) & 0xFFFF;
    int v;
    int t;
    short r;

    proc->pc += 2;
    v = thinkRegGet(regNo);
    switch (op) {
    case 8:
        t = v + 1;
        goto set;
    case 18:
        t = ~(v & 0xFFFF);
        goto set;
    case 19:
        t = ~(v & 0xFFFF) + 1;
        goto set;
    case 9:
        t = v - 1;
    set:
        r = t;
        break;
    default:
        printf(D_00A46280);
        return 1;
    }
    thinkRegSet(regNo, r & 0xFFFF);
    return 0;
}

extern const char D_00A46290[];

int cmdRmath2(ThinkProcess *proc)
{
    short op = *(short *)thinkAdrGet(proc->pc - 2);
    int regNo = thinkRegNo(proc->pc) & 0xFFFF;
    int n;
    int v;

    proc->pc += 2;
    n = cmdNum(proc->pc);
    proc->pc += 2;
    v = thinkRegGet(regNo);
    switch (op) {
    case 10:
        n = (short)(v + n);
        break;
    case 11:
        n = (short)(v - n);
        break;
    case 12:
        n = (short)(v * n);
        break;
    case 13:
        n = (short)(v / n);
        break;
    case 14:
        n = (short)(v % n);
        break;
    case 15:
        n = (short)(v & n);
        break;
    case 16:
        n = (short)(v | n);
        break;
    case 17:
        n = (short)(v ^ n);
        break;
    case 20:
        n = (short)(rnd(n));
        break;
    default:
        printf(D_00A46290);
        return 1;
    }
    thinkRegSet(regNo, n & 0xFFFF);
    return 0;
}

extern const char D_00A462D0[];

int cmdRpush(ThinkProcess *proc)
{
    int v = cmdNum(proc->pc);
    int ctx;

    proc->pc = (unsigned short)proc->pc + 2;
    ctx = pContext;
    if (*(int *)(ctx + 0xE4) < 0x20) {
        int *sp = (int *)(ctx + 0xE4);
        *(short *)(ctx + *sp * 2 + 0xA4) = v;
        (*sp)++;
    } else {
        printf(D_00A462D0);
        return 1;
    }
    return 0;
}

typedef struct ThinkProcessChkSlot {
    int dataTop;
    unsigned char unmodeled[0x20];
} ThinkProcessChkSlot;
extern ThinkProcessChkSlot D_00A597AC[];
extern const char D_00A462D0[];
extern const char D_00A462E8[];

int cmdRpop(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int ctx = pContext;
    int sp = *(int *)(ctx + 0xE4);
    int n;
    int val;

    proc->pc += 2;
    n = sp - 1;
    if (sp > 0) {
        *(int *)(ctx + 0xE4) = n;
        val = *(short *)(ctx + n * 2 + 0xA4);
    } else {
        printf(D_00A462E8);
        return 1;
    }
    thinkRegSet(regNo & 0xFFFF, val & 0xFFFF);
    return 0;
}

int cmdTblget(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int base;
    int index;
    unsigned short value;

    proc->pc += 2;
    base = cmdNum(proc->pc);
    proc->pc += 2;
    index = base + cmdNum(proc->pc) * 2;
    proc->pc += 2;
    value = *(unsigned short *)thinkAdrGet(index);
    thinkRegSet(regNo & 0xFFFF, (int)value);
    return 0;
}

int cmdTblset(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int base;
    int num;
    int value;

    proc->pc += 2;
    base = cmdNum(proc->pc);
    proc->pc += 2;
    num = cmdNum(proc->pc);
    proc->pc += 2;
    value = thinkRegGet(regNo & 0xFFFF);
    *(short *)thinkAdrGet(base + num * 2) = value;
    return 0;
}

int cmdRput(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int reg;

    proc->pc += 2;
    if (regNo & 0x4000) {
        printf(D_00A46300);
    }
    /* regNo's low 16 bits, kept in its own register separately from the raw
     * value the indirect-addressing test above reads. */
    reg = 0xFFFF;
    reg = regNo & reg;
    printf(D_00A46308, reg & 0x3FFF, thinkRegGet(reg));
    return 0;
}

extern const char D_00A46318[];

int cmdPrint(ThinkProcess *proc)
{
    char *s;
    short n = 0;
    int pc;

    s = (char *)thinkAdrGet(proc->pc);
    printf(D_00A46318, s);
    while (s[n++] != 0) {
    }
    if (n & 1) {
        n++;
    }
    pc = (unsigned short)proc->pc;
    proc->pc = n + pc;
    return 0;
}

int cmdMemdump(ThinkProcess *proc)
{
    int i;
    int count;
    int addr;

    i = 0;
    addr = cmdNum(proc->pc);
    proc->pc += 2;

    count = cmdNum(proc->pc);
    proc->pc += 2;

    printf("------------------ Mem Dump -----------------\n");
    printf("%04X: ", addr);
    for (; i < count; i++) {
        printf("%04X ", *(short *)thinkAdrGet(addr + i * 2));
        if (i % 8 == 7) {
            printf("\n%04X: ", addr + i * 2 + 2);
        }
    }
    printf("\n");
    printf("---------------------------------------------\n");
    return 0;
}

int cmdCmdputon(void)
{
    cmdPutFlag = 1;
    return 0;
}

int cmdCmdputoff(void)
{
    cmdPutFlag = 0;
    return 0;
}

extern int msgPosX;
extern int msgPosY;
static inline void rd(ThinkProcess *proc, int *dst)
{
    *dst = cmdNum(proc->pc);
    proc->pc += 2;
}

int cmdMsgPos(ThinkProcess *proc)
{
    rd(proc, &msgPosX);
    rd(proc, &msgPosY);
    return 0;
}

void battleMsgPut(int textId, int speaker, int target, int frames);
void battleMsgPut2(int textId, int speaker, int target);

int cmdMsg(ThinkProcess *proc)
{
    int len = 0;
    char *str = (char *)thinkAdrGet(proc->pc);
    int n;

    do {
    } while (str[len++] != 0);
    len += len & 1;
    proc->pc = (unsigned short)proc->pc + len;
    n = cmdNum(proc->pc);
    proc->pc = (unsigned short)proc->pc + 2;
    if (n != 0) {
        battleMsgPut(msgPosX, msgPosY, (int)str, n);
    } else {
        battleMsgPut2(msgPosX, msgPosY, (int)str);
    }
    return 0;
}

int battleMsgEndChk(void)
{
    return pMsgObj == 0;
}

/*
 * eBattleWinOpen2's own argument words (src/main/e_battle_win_open.c, still
 * asm there, no published parameter type). The header shape (textId/
 * speaker/flags/priority/target/iconGraphic) and the flags constant
 * 0x00FFFFFD match src/ov01/menu.c's StatNameOpenParams, the same window
 * family's argument record for this same callee; battleMsgPut passes no
 * icon graphic (iconGraphic is NULL) and a fixed priority of 0x138.
 */
struct BattleMsgOpen2Params {
    int textId;              /* +0x00 */
    int speaker;              /* +0x04 */
    int flags;                  /* +0x08 */
    int priority;                 /* +0x0C */
    int target;                     /* +0x10 */
    unsigned char *iconGraphic;      /* +0x14 */
};
extern void eBattleWinOpen2(struct BattleMsgOpen2Params *params);
extern MessageTask *objEntryPure(void (*callback)(MessageTask *));

void battleMsgPut(int textId, int speaker, int target, int frames)
{
    struct BattleMsgOpen2Params params;
    MessageTask *task;

    task = objEntryPure(msgObj);
    task->remaining_frames = frames;
    params.textId = textId;
    params.speaker = speaker;
    params.flags = 0x00FFFFFD;
    params.target = target;
    params.priority = 0x138;
    params.iconGraphic = 0;
    eBattleWinOpen2(&params);
    pMsgObj = task;
}

void msgObj(MessageTask *task)
{
    task->remaining_frames--;
    if (task->remaining_frames != 0) {
        eBattleWinMain2();
    } else {
        eBattleWinClose2();
        objRemovePure((ObjectTask *)task);
        pMsgObj = 0;
    }
}

/*
 * eBattleWinOpen4's own argument words (src/main/e_battle_win_open.c, still
 * asm there, no published parameter type). battleMsgPut2 fills textId,
 * speaker and target from its own three parameters (in that order, textId
 * and speaker before the call to objEntryPure returns, target and flags
 * after); flags is always 0x00FFFFFD and priority is always zero at this
 * call site.
 */
struct BattleMsgOpenParams {
    int textId;
    int speaker;
    int flags;
    int priority;
    int target;
};
extern void eBattleWinOpen4(struct BattleMsgOpenParams *params);
extern MessageTask *objEntryPure(void (*callback)(MessageTask *));

void battleMsgPut2(int textId, int speaker, int target)
{
    struct BattleMsgOpenParams params;
    MessageTask *task;

    task = objEntryPure(msgObj2);
    params.textId = textId;
    params.speaker = speaker;
    params.flags = 0x00FFFFFD;
    params.target = target;
    params.priority = 0;
    eBattleWinOpen4(&params);
    pMsgObj = task;
}

void msgObj2(MessageTask *task)
{
    if (eBattleWinPageCheck4() != 0) {
        eBattleWinClose4();
        objRemovePure((ObjectTask *)task);
        pMsgObj = 0;
    } else {
        eBattleWinMain4();
    }
}

int cmdThinkset(ThinkProcess *proc)
{
    int result;
    int commandList[8];

    result = cmdThinksetSub(proc);
    commandList[0] = 8;
    unitCmdListSet(pThinkUnit, commandList);
    return result;
}

void cmdThinkset2(ThinkProcess *proc)
{
    cmdThinksetSub(proc);
}

extern const char D_00A463A0[];
extern const char D_00A463C0[];
extern const char D_00A463E0[];
extern const char D_00A46400[];
extern const char D_00A46420[];
extern CalcUnitParam *calcUPGet(ObjectTask *unit);
typedef struct { char pad[6]; short tecId; } ThinksetEth;
extern ThinksetEth *dataEthGet(int id);
extern int calcStatGet(ObjectTask *unit, int category, int flag);

int cmdThinksetSub(ThinkProcess *proc)
{
    int list[8];
    int cmd = thinkRegGet(0x8011);
    int arg = thinkRegGet(0x8012);
    int tgt = thinkRegGet(0x8013);
    ObjectTask *unit = unitPtrGet(tgt);
    short id;

    if (unit == 0) {
        printf(D_00A463A0, tgt);
        unit = unitPtrGet(0);
    }
    switch (cmd) {
    case 1:
        {
        short tec = calcUPGet(pThinkUnit)->normalTechniqueId[arg];
        if (tec != 0) {
            list[0] = 1;
            list[1] = tec;
            list[2] = (int)unit;
            list[3] = -1;
            unitCmdListSet(pThinkUnit, list);
        } else {
            printf(D_00A463C0, arg);
        }
        }
        break;
    case 2:
        if (calcStatGet(pThinkUnit, 1, 0x1000) != 0) {
            printf(D_00A463E0, arg);
        } else {
            id = ((short *)((char *)calcUPGet(pThinkUnit) + 0x8E))[arg];
            if (id != 0) {
                int tec = dataEthGet(id)->tecId;
                list[0] = 2;
                list[1] = tec;
                list[2] = (int)unit;
                list[3] = id;
                unitCmdListSet(pThinkUnit, list);
            } else {
                printf(D_00A46400, arg);
            }
        }
        break;
    case 3:
        list[0] = 3;
        list[1] = 0;
        list[2] = (int)unit;
        list[3] = arg;
        unitCmdListSet(pThinkUnit, list);
        break;
    case 4:
        list[0] = 4;
        unitCmdListSet(pThinkUnit, list);
        break;
    case 6:
        if ((u16)(arg - 10) < 0x1A) {
            list[0] = 6;
            list[1] = -arg;
            unitCmdListSet(pThinkUnit, list);
        }
        break;
    case 7:
        list[0] = 0xD;
        list[1] = arg;
        list[2] = 0;
        unitCmdListSet(pThinkUnit, list);
        break;
    case 8:
        list[0] = 0x2E;
        unitCmdListSet(pThinkUnit, list);
        break;
    default:
        printf(D_00A46420, cmd);
    case 0:
        break;
    }
    return 0;
}

ObjectTask *thinkUnitPtrGetReg(int reg)
{
    int unitNo = thinkRegGet(reg & 0xFFFF);

    if (unitNo == 0x7FFF) {
        unitNo = unitNoGet(pThinkUnit);
    }
    return unitPtrGet(unitNo);
}

extern int calcBoost(ObjectTask *unit, int mode);
extern int calcBoostChk(ObjectTask *unit, int mode);
extern CalcUnitParam *calcUPGet(ObjectTask *unit);
extern const char D_00A46468[];

int cmdCounterBoost(void)
{
    ObjectTask *unit = thinkUnitPtrGetReg(0x8010);

    if (calcBoostChk(unit, 0) != 0) {
        calcBoost(unit, 0);
        printf(D_00A46468, calcUPGet(unit)->charaId);
    }
    return 0;
}

int cmdStatChk(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int mask;
    int unitNo;

    proc->pc += 2;
    mask = cmdNum(proc->pc);
    proc->pc += 2;
    unitNo = cmdNum(proc->pc);
    proc->pc += 2;
    thinkRegSet(regNo & 0xFFFF, (((CmdActorFlags *)unitPtrGet(unitNo)->work)->flags & mask) != 0);
    return 0;
}

int cmdLineChk(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int unitNo;

    proc->pc += 2;
    unitNo = cmdNum(proc->pc);

    proc->pc += 2;
    thinkRegSet(regNo & 0xFFFF, calcLineChk(unitPtrGet(unitNo)) != 0);
    return 0;
}

int cmdUnitparaGet(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int category;
    int unitNo;

    proc->pc += 2;
    category = cmdNum(proc->pc);
    proc->pc += 2;
    unitNo = cmdNum(proc->pc);
    proc->pc += 2;
    thinkRegSet(regNo & 0xFFFF, cmdUnitpara(unitPtrGet(unitNo), category, 0, 0) & 0xFFFF);
    return 0;
}

int cmdUnitparaSet(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int category;
    int unitNo;
    ObjectTask *unit;

    proc->pc += 2;
    category = cmdNum(proc->pc);
    proc->pc += 2;
    unitNo = cmdNum(proc->pc);
    proc->pc += 2;
    unit = unitPtrGet(unitNo);
    cmdUnitpara(unit, category, thinkRegGet(regNo & 0xFFFF) & 0x7FFF, 1);
    return 0;
}

extern void *cmdUnitparaSub(ObjectTask *unit, int category, short *size);

int cmdUnitpara(ObjectTask *unit, int category, int value, int mode)
{
    short size[8];
    short v = value;
    void *p = cmdUnitparaSub(unit, (short)category, size);
    int r;

    if (size[0] == 1) {
        r = *(char *)p;
        if (mode == 1) {
            *(char *)p = v;
        }
    } else {
        r = *(short *)p;
        if (mode == 1) {
            *(short *)p = v;
        }
    }
    return r;
}

void *cmdUnitparaSub(ObjectTask *unit, int category, short *size)
{
    short c = category;
    char *base = (char *)calcUPGet(unit);
    char *p;

    switch (c) {
    case 0: p = base + 0; *size = 2; break;
    case 1: p = base + 2; *size = 2; break;
    case 2: p = base + 4; *size = 2; break;
    case 3: p = base + 6; *size = 2; break;
    case 4: p = base + 8; *size = 2; break;
    case 5: p = base + 10; *size = 2; break;
    case 6: p = base + 12; *size = 1; break;
    case 7: p = base + 13; *size = 1; break;
    case 8: p = base + 14; *size = 1; break;
    case 9: p = base + 15; *size = 1; break;
    case 10: p = base + 16; *size = 2; break;
    case 11: p = base + 18; *size = 2; break;
    case 12: p = base + 20; *size = 1; break;
    case 13: p = base + 21; *size = 1; break;
    case 14: p = base + 22; *size = 1; break;
    case 15: p = base + 23; *size = 1; break;
    case 16: p = base + 24; *size = 2; break;
    case 17: p = base + 26; *size = 2; break;
    case 18: p = base + 28; *size = 2; break;
    case 19: p = base + 30; *size = 2; break;
    case 20: p = base + 52; *size = 2; break;
    case 21: p = base + 54; *size = 2; break;
    case 22: p = base + 56; *size = 2; break;
    case 23: p = base + 68; *size = 2; break;
    case 24: p = base + 58; *size = 1; break;
    case 25: p = base + 32; *size = 1; break;
    case 26: p = base + 60; *size = 1; break;
    default:
        printf("** UNIT PARA err -> %d\n", c);
        return 0;
    }
    return p;
}

int cmdUnittbl(ThinkProcess *proc)
{
    int *tbl;
    int regNo;
    int adr;
    int num;
    int i;
    short count;
    int n;
    int p;

    i = 0;
    count = 0;
    regNo = thinkRegNo(proc->pc);
    proc->pc += 2;
    adr = cmdNum(proc->pc);
    proc->pc += 2;
    num = cmdNum(proc->pc);
    proc->pc += 2;
    n = unitTblGet(num, &tbl);
    for (; i < n; i++) {
        p = tbl[i];
        if (p != 0 && !(**(int **)((char *)p + 0x10) & 0x80)) {
            { int a = thinkAdrGet(adr + count * 2); short v = unitNoGet(tbl[i]); *(short *)a = v; }
            count++;
        }
    }
    thinkRegSet(regNo & 0xFFFF, count & 0xFFFF);
    return 0;
}

extern short D_00A5A1A0[36];

int cmdUnitCreate(ThinkProcess *proc)
{
  int list[8];
  int *tbl;
  int regNo;
  int adr;
  int lo;
  int start;
  int new_var;
  int hi;
  int n;
  int i;
  int found;
  int cnt;
  ObjectTask *unit;
  int val;
  int idx;
  short *wp;
  cnt = 0;
  regNo = thinkRegNo(proc->pc);
  proc->pc += 2;
  adr = cmdNum(proc->pc);
  proc->pc += 2;
  start = cmdNum(proc->pc);
  new_var = 0x25;
  proc->pc += 2;
  hi = cmdNum(proc->pc);
  proc->pc += 2;
  n = unitTblGet(1, &tbl);
  lo = start;
  if (lo <= hi)
  {
    wp = D_00A5A1A0;
    do
    {
      found = 0;
      for (i = 0; i < n; i++)
      {
        if (tbl[i] != 0)
        {
          if (((*((short *) (((char *) calcUPGet((ObjectTask *) tbl[i])) + 0x44))) < 0) ? ((-(*((short *) (((char *) calcUPGet((ObjectTask *) tbl[i])) + 0x44)))) == lo) : ((*((short *) (((char *) calcUPGet((ObjectTask *) tbl[i])) + 0x44))) == lo))
          {
            found = 1;
            break;
          }
        }
      }

      if (!found)
      {
        *wp++ = lo;
        cnt++;
      }
      lo++;
    }
    while (lo <= hi);
  }
  if (cnt > 0)
  {
    short *pl = (short *) ((int (*)(int)) thinkAdrGet)(adr);
    idx = rnd(cnt - 1);
    pl[1] = D_00A5A1A0[idx];
    lo = dataCdSync();
    unit = unitEnCreate(pl);
    if (unit != 0)
    {
      list[0] = 0x2A;
      list[1] = lo;
      unitCmdListSet(unit, list);
      list[0] = new_var;
      list[1] = rnd(8);
      unitCmdListSet(unit, list);
      do
      {
        val = (short) unitNoGet(unit);
      }
      while (0);
    }
    else
    {
      val = -1;
    }
  }
  else
  {
    val = -1;
  }
  thinkRegSet(regNo & 0xFFFF, val & 0xFFFF);
  return 0;
}

int cmdUnitChange(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int num;
    int result;
    ObjectTask *unit;
    int list[8];
    int hp;

    proc->pc += 2;
    num = cmdNum(proc->pc);
    proc->pc += 2;
    unit = thinkUnitPtrGetReg(0x8010);
    result = 0;
    if (unit != 0) {
        list[0] = 0x2B;
        if (*(short *)((char *)calcUPGet(unit) + 0x44) < 0) {
            hp = -*(short *)((char *)calcUPGet(unit) + 0x44);
        } else {
            hp = *(short *)((char *)calcUPGet(unit) + 0x44);
        }
        list[1] = hp;
        list[2] = num;
        unitCmdListSet(unit, list);
        hp = 0x25;
        list[0] = hp;
        list[1] = 0;
        unitCmdListSet(unit, list);
        result = 1;
    }
    thinkRegSet(regNo & 0xFFFF, result);
    return 0;
}

int cmdAtktbl(ThinkProcess *proc)
{
    int *tbl;
    int regNo;
    int adr;
    int num;
    int ofs;
    ObjectTask *unit;
    short count;
    int i;
    short tec;
    int j;
    int n;
    int a;
    int p;
    short *dst;

    count = 1;
    i = 0;
    regNo = thinkRegNo(proc->pc);
    proc->pc += 2;
    adr = cmdNum(proc->pc);
    proc->pc += 2;
    num = cmdNum(proc->pc);
    proc->pc += 2;
    unit = thinkUnitPtrGetReg(0x8010);
    if (i < num) {
        ofs = 0x70;
        do {
            tec = *(short *)((char *)calcUPGet(unit) + ofs + 6);
            if (tec != 0) {
                j = 0;
                n = unitTblGet(calcTrg2PE(unit, *(unsigned char *)dataTecGet(tec)), &tbl);
                if (n > 0) {
                    a = count * 2 + adr;
                    do {
                        p = tbl[j];
                        if (p != 0 && !(**(int **)((char *)p + 0x10) & 0x80)
                            && calcAtkChk(unit, p, tec) != 0) {
                            *(short *)thinkAdrGet(a) = i;
                            a += 2;
                            count++;
                            dst = (short *)thinkAdrGet(a);
                            a += 2;
                            *dst = unitNoGet(tbl[j]);
                            count++;
                        }
                        j++;
                    } while (j < n);
                }
            }
            i++;
            ofs += 2;
        } while (i < num);
    }
    count = (count - 1) / 2;
    thinkRegSet(regNo & 0xFFFF, count & 0xFFFF);
    *(short *)((int (*)(int))thinkAdrGet)(adr) = count;
    return 0;
}

int cmdAtkset(ThinkProcess *proc)
{
    int base;
    int idx;
    short max;
    int off;

    base = cmdNum(proc->pc);
    proc->pc += 2;
    idx = cmdNum(proc->pc);
    proc->pc += 2;
    max = *(short *)((int (*)(int))thinkAdrGet)(base);
    if (idx >= max) {
        printf("** ATKSET OVER => %d/%d\n", idx, max);
        idx = 0;
    }
    off = base + ((idx << 17) >> 15);
    thinkRegSet(0x8012, *(unsigned short *)thinkAdrGet(off + 2));
    thinkRegSet(0x8013, *(unsigned short *)thinkAdrGet(off + 4));
    return 0;
}

void cmdAtktblSort(ThinkProcess *proc)
{
    proc->sortMode = 0;
    cmdAtktblSortSub(proc);
}

void cmdAtktblSortRev(ThinkProcess *proc)
{
    proc->sortMode = 1;
    cmdAtktblSortSub(proc);
}

int cmdAtktblSortSub(ThinkProcess *proc)
{
    short size;
    int tbl;
    int category;
    int n;
    int best = 0;
    int i = 0;
    int j;
    short *val;
    short a, b;
    int v;

    tbl = cmdNum(proc->pc);
    proc->pc += 2;
    category = cmdNum(proc->pc);
    proc->pc += 2;
    n = *(short *)((int (*)())thinkAdrGet)(tbl);
    tbl = (short)(tbl + 2);
    if (n - 1 > 0) {
        do {
            for (j = i; j < n; j++) {
                val = cmdUnitparaSub(unitPtrGet(*(short *)thinkAdrGet(j * 4 + tbl + 2)), category, &size);
                if (size == 1) {
                    v = *(signed char *)val;
                } else {
                    v = *val;
                }
                if (i == j) {
                    best = v;
                }
                if ((proc->sortMode == 0 && best < v) || (proc->sortMode == 1 && v < best)) {
                    a = *(short *)thinkAdrGet(j * 4 + tbl);
                    best = v;
                    b = *(short *)thinkAdrGet(i * 4 + tbl);
                    *(short *)thinkAdrGet(j * 4 + tbl) = b;
                    *(short *)thinkAdrGet(i * 4 + tbl) = a;
                    a = *(short *)thinkAdrGet(j * 4 + tbl + 2);
                    b = *(short *)thinkAdrGet(i * 4 + tbl + 2);
                    *(short *)thinkAdrGet(j * 4 + tbl + 2) = b;
                    *(short *)thinkAdrGet(i * 4 + tbl + 2) = a;
                }
            }
            i++;
        } while (i < n - 1);
    }
    return 0;
}

int cmdAtktblSearch(ThinkProcess *proc)
{
    int regNo;
    int src;
    int dst;
    int pc;
    int r10;
    int r12;
    int n;
    int p;
    int ofs;
    short a;
    short b;
    int count;
    short save;

    count = 1;
    regNo = thinkRegNo(proc->pc);
    proc->pc += 2;
    src = cmdNum(proc->pc);
    proc->pc += 2;
    dst = cmdNum(proc->pc);
    proc->pc += 2;
    pc = cmdNum(proc->pc);
    proc->pc += 2;
    r10 = thinkRegGet(0x8010);
    r12 = thinkRegGet(0x8012);
    n = *(short *)((int (*)(int))thinkAdrGet)(src);
    src = (short)(src + 2);
    save = proc->pc;
    if (n > 0) {
        int i = n;
        p = src;
        ofs = dst + 2;
        do {
            a = *(short *)thinkAdrGet(p);
            b = *(short *)thinkAdrGet(p + 2);
            thinkRegSet(0x8010, b);
            thinkRegSet(0x8012, a);
            proc->pc = pc;
            while (thinkExecSub(proc) == 0) {
            }
            if (thinkRegGet(0x8018) != 0) {
                *(short *)thinkAdrGet(ofs) = a;
                ofs += 2;
                count++;
                *(short *)thinkAdrGet(ofs) = b;
                ofs += 2;
                count++;
            }
            i--;
            p += 4;
        } while (i != 0);
    }
    proc->pc = save;
    thinkRegSet(0x8012, r12);
    thinkRegSet(0x8010, r10);
    count = (count - 1) / 2;
    thinkRegSet(regNo, count);
    *(short *)((int (*)(int))thinkAdrGet)(dst) = count;
    return 0;
}

int cmdTecparaGet(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int category;
    int tecId;

    proc->pc += 2;
    category = cmdNum(proc->pc);
    proc->pc += 2;
    tecId = cmdNum(proc->pc);
    proc->pc += 2;
    thinkRegSet(regNo & 0xFFFF, cmdTecpara(dataTecGet(tecId), category) & 0xFFFF);
    return 0;
}

int cmdTecpara(int tec, int category)
{
    short valueType;
    CmdTecparaValue *valueAddress = cmdTecparaSub(tec, category, &valueType);

    if (valueType == 1) {
        return valueAddress->byteValue;
    }
    return valueAddress->halfwordValue;
}

CmdTecparaValue *cmdTecparaSub(int tec, short category, short *valueType)
{

    switch (category) {
    case 0: *valueType = 1; break;
    case 1: tec += 2; *valueType = 2; break;
    case 2: tec += 4; *valueType = 1; break;
    case 3: tec += 5; *valueType = 1; break;
    case 4: tec += 12; *valueType = 1; break;
    case 5: tec += 13; *valueType = 1; break;
    case 6: tec += 14; *valueType = 2; break;
    case 7: tec += 16; *valueType = 1; break;
    case 8: tec += 17; *valueType = 1; break;
    case 9: tec += 18; *valueType = 2; break;
    default:
        printf("** TEC PARA err -> %d\n", category);
        return 0;
    }
    return (CmdTecparaValue *)tec;
}

int cmdMonsSetNoGet(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);

    proc->pc += 2;
    thinkRegSet(regNo & 0xFFFF, monsSetNoGet() & 0xFFFF);
    return 0;
}

int cmdPartySizeGet(ThinkProcess *proc)
{
    int regNo;
    int *tbl;
    int i;
    signed char size;
    int n;
    int num;

    i = 0;
    size = 0;
    regNo = thinkRegNo(proc->pc);
    proc->pc += 2;
    num = cmdNum(proc->pc);
    proc->pc += 2;
    n = unitTblGet(num, &tbl);
    for (; i < n; i++) {
        if (tbl[i] != 0 && size < ((signed char *)calcUPGet((ObjectTask *)tbl[i]))[0x15]) {
            size = ((signed char *)calcUPGet((ObjectTask *)tbl[i]))[0x15];
        }
    }
    thinkRegSet(regNo & 0xFFFF, size & 0xFFFF);
    return 0;
}

int cmdLightAmb(ThinkProcess *proc)
{
    Vector4 ambient;
    StudioLight *light;
    float red;
    float green;
    float blue;

    red = (float)cmdNum(proc->pc);
    proc->pc += 2;
    ambient.x = red * 0.01f;
    green = (float)cmdNum(proc->pc);
    proc->pc += 2;
    ambient.y = green * 0.01f;
    blue = (float)cmdNum(proc->pc);
    proc->pc += 2;
    ambient.w = 1.0f;
    ambient.z = blue * 0.01f;
    xglStudioGetLight(&light);
    xglLightIntensityAmbient(light, &ambient);
    return 0;
}

int cmdLightCol(ThinkProcess *proc)
{
    Vector4 color;
    StudioLight *light;
    int index;
    float red;
    float green;
    float blue;

    index = cmdNum(proc->pc);
    proc->pc += 2;
    red = (float)cmdNum(proc->pc);
    proc->pc += 2;
    color.x = red * 0.01f;
    green = (float)cmdNum(proc->pc);
    proc->pc += 2;
    color.y = green * 0.01f;
    blue = (float)cmdNum(proc->pc);
    proc->pc += 2;
    color.w = 1.0f;
    color.z = blue * 0.01f;
    xglStudioGetLight(&light);
    xglLightIntensityParallel(light, index, &color);
    return 0;
}

int cmdLightDir(ThinkProcess *proc)
{
    Vector4 direction;
    StudioLight *light;
    int index;
    float x;
    float y;
    float z;

    index = cmdNum(proc->pc);
    proc->pc += 2;
    x = (float)cmdNum(proc->pc);
    proc->pc += 2;
    direction.x = x * 0.01f;
    y = (float)cmdNum(proc->pc);
    proc->pc += 2;
    direction.y = y * 0.01f;
    z = (float)cmdNum(proc->pc);
    proc->pc += 2;
    direction.w = 1.0f;
    direction.z = z * 0.01f;
    xglStudioGetLight(&light);
    xglLightDirection(light, index, &direction);
    return 0;
}

extern McamParams mcamView;
extern McamParams mcamLookAt;
extern McamParams mcamViewMove;
extern McamParams mcamLookAtMove;

McamParams *mcamPtrGet(void)
{
    McamParams *ret;

    switch (camMode) {
    case -1:
        ret = &mcamViewMove;
        break;
    case -2:
        ret = &mcamLookAtMove;
        break;
    case 0:
        ret = &mcamView;
        break;
    case 1:
        ret = &mcamLookAt;
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

struct MpersParams;
extern struct MpersParams mcamPers;
extern struct MpersParams mcamPersMove;

struct MpersParams *mpersPtrGet(void)
{
    return persMode == -3 ? &mcamPersMove : persMode == 3 ? &mcamPers : 0;
}

struct MbankParams;
extern struct MbankParams mcamBank;
extern struct MbankParams mcamBankMove;

struct MbankParams *mbankPtrGet(void)
{
    return bankMode == -4 ? &mcamBankMove : bankMode == 2 ? &mcamBank : 0;
}

void myMCamSet(int mode, void *params)
{
    if (mode >= 0) {
        MCamSet(mode, params);
    }
}

int unitNoChk(int unitNo)
{
    if (unitNo >= 8) {
        printf(D_00A46578, unitNo);
        unitNo = 0;
    }
    return unitNo;
}

int cmdCamRefMode(void)
{
    camMode = 1;
    return 0;
}

int cmdCamCamMode(void)
{
    camMode = 0;
    return 0;
}

int cmdCamRefMoveMode(void)
{
    camMode = -2;
    return 0;
}

int cmdCamCamMoveMode(void)
{
    camMode = -1;
    return 0;
}

int cmdCamPersMode(void)
{
    persMode = 3;
    return 0;
}

int cmdCamBankMode(void)
{
    bankMode = 2;
    return 0;
}

int cmdCamPersMoveMode(void)
{
    persMode = -3;
    return 0;
}

int cmdCamBankMoveMode(void)
{
    bankMode = -4;
    return 0;
}

extern const char D_00A46590[];
typedef struct {
    void *mode;
    int pad4[3];
    Vector4 pos;
    unsigned char pad20[0x70 - 0x20];
    void *offsEnabled;
} CamLockView;

extern Vector4 *MCamGetCurrentCoord(Vector4 *out, int type);

int cmdCamLock(void)
{
    Vector4 v;
    CamLockView *cam;

    if (camMode < 0) {
        printf(D_00A46590, camMode);
        return 0;
    }
    ((Vector4 *(*)(Vector4 *))MCamGetCurrentCoord)(&v);   /* original passes only &v */
    cam = (CamLockView *)mcamPtrGet();
    cam->pos.x = v.x;
    cam->mode = 0;
    cam->offsEnabled = 0;
    cam->pos.y = v.y;
    cam->pos.z = v.z;
    cam->pos.w = 1.0f;
    myMCamSet(camMode, cam);
    return 0;
}

typedef struct {
    int mode;
    int pad4[3];
    Vector4 pos;
} McamCopyView;

int cmdCamCopy(void)
{
    Vector4 v;
    McamCopyView *cam;
    int type;

    type = 0;
    if (camMode != 1) {
        type = 1;
        if (camMode != 0) {
            printf("** CamCopy err **\n");
            return 0;
        }
    }
    MCamGetCurrentCoord(&v, type);
    cam = (McamCopyView *)mcamPtrGet();
    cam->pos.x = v.x;
    cam->mode = 0;
    ((McamParams *)cam)->offsEnabled = 0;
    cam->pos.y = v.y;
    cam->pos.z = v.z;
    cam->pos.w = 1.0f;
    myMCamSet(camMode, cam);
    return 0;
}

typedef struct McamPosParams {
    int mode;                         /* +0x00 */
    unsigned char unmodeled_4[0x10 - 0x4];
    Vector4 pos;                      /* +0x10 */
    unsigned char unmodeled_20[0x70 - 0x20];
    int offsEnabled;                  /* +0x70 */
} McamPosParams;

int cmdCamSetPos(ThinkProcess *proc)
{
    McamPosParams *cam = (McamPosParams *)mcamPtrGet();
    float posX;
    float posY;
    float posZ;

    cam->mode = 0;
    posX = (float)cmdNum(proc->pc);
    proc->pc += 2;
    cam->pos.x = posX * 0.01f;
    posY = (float)cmdNum(proc->pc);
    proc->pc += 2;
    cam->pos.y = posY * 0.01f;
    posZ = (float)cmdNum(proc->pc);
    proc->pc += 2;
    cam->pos.w = 1.0f;
    cam->offsEnabled = 0;
    cam->pos.z = posZ * 0.01f;
    myMCamSet(camMode, (McamParams *)cam);
    return 0;
}

typedef signed int s32;
typedef float f32;

s32 cmdCamSetAct(void *arg0)
{
  McamParams *temp_18;
  f32 temp_f0;
  f32 temp_f0_2;
  short *new_var;
  f32 temp_f0_3;
  int temp_3;
  int temp_3_2;
  int temp_3_3;
  int temp_3_4;
  s32 temp_2;
  s32 temp_2_2;
  temp_18 = mcamPtrGet();
  temp_2 = unitNoChk(cmdNum(*((short *) (((char *) arg0) + 0x4))));
  *((int *) (((char *) temp_18) + 0x0)) = 1;
  *((int *) (((char *) temp_18) + 0x20)) = (s32) (*((int *) (((char *) unitPtrGet(temp_2)) + 0x14)));
  new_var = &calcUPGet(unitPtrGet(temp_2))->charaId;
  temp_3 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  *((int *) (((char *) temp_18) + 0x24)) = (s32) (*new_var);
  *((short *) (((char *) arg0) + 0x4)) = temp_3;
  temp_2_2 = cmdNum(temp_3);
  temp_3_2 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  temp_18->offsEnabled = 1;
  *((int *) (((char *) temp_18) + 0x28)) = temp_2_2;
  if (arg0)
  {
  }
  *((short *) (((char *) arg0) + 0x4)) = temp_3_2;
  temp_f0 = (f32) cmdNum(temp_3_2);
  temp_3_3 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  *((short *) (((char *) arg0) + 0x4)) = temp_3_3;
  temp_18->offset.x = temp_f0 * 0.01f;
  if (!arg0)
  {
  }
  temp_f0_2 = (f32) cmdNum(temp_3_3);
  temp_3_4 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  *((short *) (((char *) arg0) + 0x4)) = temp_3_4;
  temp_18->offset.y = temp_f0_2 * 0.01f;
  temp_f0_3 = (f32) cmdNum(temp_3_4);
  *((short *) (((char *) arg0) + 0x4)) = (s16) (((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2);
  temp_18->offset.w = 1.0f;
  temp_18->offset.z = temp_f0_3 * 0.01f;
  myMCamSet(camMode, temp_18);
  return 0;
}

s32 cmdCamSetAct2(void *arg0)
{
  McamParams *temp_18;
  f32 temp_f0;
  f32 temp_f0_2;
  f32 temp_f0_3;
  f32 temp_f0_4;
  int temp_3;
  int temp_3_2;
  int temp_3_3;
  int temp_3_4;
  int temp_3_5;
  int temp_3_6;
  int temp_3_7;
  s32 temp_2;
  s32 temp_2_2;
  short *new_var;
  short *new_var2;
  temp_18 = mcamPtrGet();
  temp_2 = unitNoChk(cmdNum(*((short *) (((char *) arg0) + 0x4))));
  *((int *) (((char *) temp_18) + 0x0)) = 2;
  *((int *) (((char *) temp_18) + 0x2C)) = (s32) (*((int *) (((char *) unitPtrGet(temp_2)) + 0x14)));
  new_var = &calcUPGet(unitPtrGet(temp_2))->charaId;
  temp_3 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  *((int *) (((char *) temp_18) + 0x30)) = (s32) (*new_var);
  *((short *) (((char *) arg0) + 0x4)) = temp_3;
  *((int *) (((char *) temp_18) + 0x34)) = cmdNum(temp_3);
  temp_3_2 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  *((short *) (((char *) arg0) + 0x4)) = temp_3_2;
  temp_2_2 = unitNoChk(cmdNum(temp_3_2));
  *((int *) (((char *) temp_18) + 0x38)) = (s32) (*((int *) (((char *) unitPtrGet(temp_2_2)) + 0x14)));
  new_var2 = &calcUPGet(unitPtrGet(temp_2_2))->charaId;
  temp_3_3 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  *((int *) (((char *) temp_18) + 0x3C)) = (s32) (*new_var2);
  *((short *) (((char *) arg0) + 0x4)) = temp_3_3;
  *((int *) (((char *) temp_18) + 0x40)) = cmdNum(temp_3_3);
  temp_3_4 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  temp_18->offsEnabled = 1;
  *((short *) (((char *) arg0) + 0x4)) = temp_3_4;
  temp_f0 = (f32) cmdNum(temp_3_4);
  temp_3_5 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  *((short *) (((char *) arg0) + 0x4)) = temp_3_5;
  temp_18->offset.x = temp_f0 * 0.01f;
  temp_f0_2 = (f32) cmdNum(temp_3_5);
  temp_3_6 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  *((short *) (((char *) arg0) + 0x4)) = temp_3_6;
  temp_18->offset.y = temp_f0_2 * 0.01f;
  temp_f0_3 = (f32) cmdNum(temp_3_6);
  temp_3_7 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  temp_18->offset.w = 1.0f;
  *((short *) (((char *) arg0) + 0x4)) = temp_3_7;
  temp_18->offset.z = temp_f0_3 * 0.01f;
  temp_f0_4 = (f32) cmdNum(temp_3_7);
  *((short *) (((char *) arg0) + 0x4)) = (s16) (((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2);
  *((float *) (((char *) temp_18) + 0x6C)) = temp_f0_4 * 0.01f;
  myMCamSet(camMode, temp_18);
  return 0;
}

s32 cmdCamSetAct3(void *arg0)
{
  McamParams *temp_18;
  f32 temp_f0;
  f32 temp_f0_2;
  f32 temp_f0_3;
  int temp_3;
  int temp_3_2;
  int temp_3_3;
  int temp_3_4;
  int temp_3_5;
  int temp_3_6;
  s32 temp_2;
  s32 temp_2_2;
  short *new_var;
  short *new_var2;
  temp_18 = mcamPtrGet();
  temp_2 = unitNoChk(cmdNum(*((short *) (((char *) arg0) + 0x4))));
  *((int *) (((char *) temp_18) + 0x0)) = 3;
  *((int *) (((char *) temp_18) + 0x2C)) = (s32) (*((int *) (((char *) unitPtrGet(temp_2)) + 0x14)));
  new_var = &calcUPGet(unitPtrGet(temp_2))->charaId;
  temp_3 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  *((int *) (((char *) temp_18) + 0x30)) = (s32) (*new_var);
  *((short *) (((char *) arg0) + 0x4)) = temp_3;
  *((int *) (((char *) temp_18) + 0x34)) = cmdNum(temp_3);
  temp_3_2 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  *((short *) (((char *) arg0) + 0x4)) = temp_3_2;
  temp_2_2 = unitNoChk(cmdNum(temp_3_2));
  *((int *) (((char *) temp_18) + 0x38)) = (s32) (*((int *) (((char *) unitPtrGet(temp_2_2)) + 0x14)));
  new_var2 = &calcUPGet(unitPtrGet(temp_2_2))->charaId;
  temp_3_3 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  *((int *) (((char *) temp_18) + 0x3C)) = (s32) (*new_var2);
  *((short *) (((char *) arg0) + 0x4)) = temp_3_3;
  *((int *) (((char *) temp_18) + 0x40)) = cmdNum(temp_3_3);
  temp_3_4 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  temp_18->offsEnabled = 1;
  *((short *) (((char *) arg0) + 0x4)) = temp_3_4;
  temp_f0 = (f32) cmdNum(temp_3_4);
  temp_3_5 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  *((short *) (((char *) arg0) + 0x4)) = temp_3_5;
  temp_18->offset.x = temp_f0 * 0.01f;
  temp_f0_2 = (f32) cmdNum(temp_3_5);
  temp_3_6 = ((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2;
  *((short *) (((char *) arg0) + 0x4)) = temp_3_6;
  temp_18->offset.y = temp_f0_2 * 0.01f;
  temp_f0_3 = (f32) cmdNum(temp_3_6);
  *((short *) (((char *) arg0) + 0x4)) = (s16) (((u16) (*((short *) (((char *) arg0) + 0x4)))) + 2);
  temp_18->offset.w = 1.0f;
  temp_18->offset.z = temp_f0_3 * 0.01f;
  myMCamSet(camMode, temp_18);
  return 0;
}

int cmdCamOffsPos(ThinkProcess *proc)
{
    McamParams *cam = mcamPtrGet();
    float offsX;
    float offsY;
    float offsZ;

    cam->offsEnabled = 1;
    offsX = (float)cmdNum(proc->pc);
    proc->pc += 2;
    cam->offset.x = offsX * 0.01f;
    offsY = (float)cmdNum(proc->pc);
    proc->pc += 2;
    cam->offset.y = offsY * 0.01f;
    offsZ = (float)cmdNum(proc->pc);
    proc->pc += 2;
    cam->offset.w = 1.0f;
    cam->offset.z = offsZ * 0.01f;
    myMCamSet(camMode, cam);
    return 0;
}

int cmdCamOffsAng(ThinkProcess *proc)
{
    McamParams *cam = mcamPtrGet();
    float offsX;
    float offsY;
    float offsZ;

    cam->offsEnabled = 2;
    offsX = (float)cmdNum(proc->pc);
    cam->angleOffset.x = offsX * 0.01f;
    proc->pc += 2;
    offsY = (float)cmdNum(proc->pc);
    proc->pc += 2;
    cam->angleOffset.y = (offsY / 180.0f) * 3.1415927f;
    offsZ = (float)cmdNum(proc->pc);
    proc->pc += 2;
    cam->angleOffset.z = (offsZ / 180.0f) * 3.1415927f;
    myMCamSet(camMode, cam);
    return 0;
}

typedef struct CamMoveBlk {
    long long raw[0x16];
} CamMoveBlk;
typedef struct CamMoveArg {
    CamMoveBlk blk;
    int a;
    int b;
    int c;
    short d;
} CamMoveArg;
void MCamMove(int, CamMoveArg *);

int cmdCamMove(ThinkProcess *proc)
{
    CamMoveArg arg;
    int kind;
    int mode;
    CamMoveBlk *src;

    rd(proc, &arg.a);
    rd(proc, &arg.b);
    rd(proc, &arg.c);
    arg.d = cmdNum(proc->pc);
    proc->pc += 2;
    kind = cmdNum(proc->pc);
    proc->pc += 2;
    if (kind == 0) {
        if ((unsigned int)(camMode + 1) < 2) {
            mode = 0;
            src = (CamMoveBlk *)&mcamViewMove;
        } else {
            mode = 1;
            src = (CamMoveBlk *)&mcamLookAtMove;
        }
    } else if (kind == 1) {
        mode = 3;
        src = (CamMoveBlk *)&mcamPersMove;
    } else if (kind == 2) {
        mode = 2;
        src = (CamMoveBlk *)&mcamBankMove;
    } else {
        return 0;
    }
    arg.blk = *src;
    MCamMove(mode, &arg);
    return 0;
}

void MCamShake(int, int, int, int, int);

int cmdCamShake(ThinkProcess *proc)
{
    int e, a, b, c, d;

    a = cmdNum(proc->pc);
    proc->pc += 2;
    b = cmdNum(proc->pc);
    proc->pc += 2;
    c = cmdNum(proc->pc);
    proc->pc += 2;
    d = cmdNum(proc->pc);
    proc->pc += 2;
    e = cmdNum(proc->pc);
    proc->pc += 2;
    MCamShake(a, b, c, d, e);
    return 0;
}

extern float MCamGetLengthActor(int selector);
extern const char D_00A465C0[];

int cmdCamCenterLength(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);

    proc->pc += 2;
    if (camMode < 0) {
        printf(D_00A465C0);
        return 0;
    }
    thinkRegSet(regNo, (int)(MCamGetLengthActor(camMode) * 100.0f));
    return 0;
}

extern void sefGetNullPosition(float *out, void *obj, int idx, int flag);

int cmdCamUnitHeight(ThinkProcess *proc)
{
    float pos[4];
    short r = thinkRegNo(proc->pc);
    short u;

    proc->pc = (unsigned short)proc->pc + 2;
    u = unitNoChk(cmdNum(proc->pc));
    proc->pc = (unsigned short)proc->pc + 2;
    sefGetNullPosition(pos, *(void **)((char *)unitPtrGet(u) + 0x14), 13, 0);
    thinkRegSet(r, (int)(pos[1] * 100.0f));
    return 0;
}

int cmdCamCoordGet(ThinkProcess *proc)
{
    Vector4 v;
    char *cam;
    int r0, r1, r2;

    cam = (char *)mcamPtrGet();
    r0 = thinkRegNo(proc->pc);
    proc->pc += 2;
    r1 = thinkRegNo(proc->pc);
    proc->pc += 2;
    r2 = thinkRegNo(proc->pc);
    proc->pc += 2;
    if (camMode >= 0) {
        MCamGetCurrentCoord(&v, camMode);
    } else {
        v.x = *(float *)(cam + 0x10);
        v.y = *(float *)(cam + 0x14);
        v.z = *(float *)(cam + 0x18);
    }
    thinkRegSet(r0 & 0xFFFF, (int)(v.x * 100.0f) & 0xFFFF);
    thinkRegSet(r1 & 0xFFFF, (int)(v.y * 100.0f) & 0xFFFF);
    thinkRegSet(r2 & 0xFFFF, (int)(v.z * 100.0f) & 0xFFFF);
    return 0;
}

/*
 * mbankPtrGet (this TU, still asm) returns the bank parameter block
 * cmdCamBank fills in; only the member that function touches is modeled.
 */
typedef struct MbankParams {
    unsigned char unmodeled_0[0xA0];
    float angle;                      /* +0xA0: bank angle in radians */
} MbankParams;

extern MbankParams *mbankPtrGet(void);

int cmdCamBank(ThinkProcess *proc)
{
    MbankParams *bank = mbankPtrGet();
    float raw;
    float angle;

    raw = (float)cmdNum(proc->pc);
    angle = ((raw * 0.1f) / 180.0f) * 3.1415927f;
    proc->pc += 2;
    bank->angle = angle;
    myMCamSet(bankMode, bank);
    return 0;
}

/*
 * mpersPtrGet (this TU, still asm) returns the perspective parameter block
 * cmdCamPers fills in; only the member that function touches is modeled.
 */
typedef struct MpersParams {
    unsigned char unmodeled_0[0xA4];
    float value;                      /* +0xA4: tenths-scaled perspective value */
} MpersParams;

extern MpersParams *mpersPtrGet(void);

int cmdCamPers(ThinkProcess *proc)
{
    MpersParams *pers;
    float value;

    pers = mpersPtrGet();
    value = (float)cmdNum(proc->pc) * 0.1f;
    proc->pc += 2;
    pers->value = value;
    myMCamSet(persMode, pers);
    return 0;
}

int cmdCamTblSet(ThinkProcess *proc)
{
    int tblNo = cmdNum(proc->pc);

    proc->pc += 2;
    if (tblNo < thinkCamTblNumGet()) {
        camTblIdx = tblNo;
    }
    return 0;
}

int cmdCamMoveStop(void)
{
    MCamStopMove((unsigned int)(camMode + 1) >= 2);
    return 0;
}

int cmdEvent(ThinkProcess *proc)
{
    ThinkProcess np;
    int regNo = thinkRegNo(proc->pc);
    int n;

    proc->pc += 2;
    n = cmdNum(proc->pc);
    proc->pc += 2;
    np.dataTop = pDataTop;
    np.pc = n;
    np.context = pContext;
    thinkRegSet(regNo & 0xFFFF, thinkProcessAdd(&np) & 0xFFFF);
    return 0;
}

int cmdWaitEvent(ThinkProcess *proc)
{
    int result;

    if (!proc->waitActive) {
        proc->sortMode = cmdNum(proc->pc);
        result = 2;
    } else if (thinkProcessChk(proc->sortMode) != 0) {
        result = 2;
    } else {
        proc->pc += 2;
        result = 0;
    }
    return result;
}

int cmdWaitCnt(ThinkProcess *proc)
{
    int count;
    int result;

    if (!proc->waitActive) {
        proc->sortMode = cmdNum(proc->pc);
        result = 2;
    } else {
        count = proc->sortMode - 1;
        proc->sortMode = count;
        if (count <= 0) {
            proc->pc += 2;
            result = 0;
        } else {
            result = 2;
        }
    }
    return result;
}

extern int MCamIsMoving(int selector);

int cmdWaitCamMove(ThinkProcess *proc)
{
    if (!proc->waitActive) {
        if ((unsigned int)(camMode + 1) < 2) {
            proc->sortMode = 0;
        } else {
            proc->sortMode = 1;
        }
        return 2;
    }
    return MCamIsMoving(proc->sortMode) == 0 ? 0 : 2;
}

int cmdWaitMsg(ThinkProcess *proc)
{
    int result = 2;

    if (proc->waitActive) {
        result = pMsgObj == 0;
        result = result ? 0 : 2;
    }
    return result;
}

int cmdVPadEneble(void)
{
    dataVPadModeSet(1);
    return 0;
}

int cmdVPadDisable(void)
{
    dataVPadModeSet(0);
    return 0;
}

int cmdVPadSet(ThinkProcess *proc)
{
    int padMask = cmdNum(proc->pc) & 0xFFFF;

    proc->pc += 2;
    dataVPadSet(padMask);
    return 0;
}

int cmdMapMulSet(ThinkProcess *proc)
{
    Vector4 mul;
    float rawX;
    float rawY;
    float rawZ;
    float rawW;

    rawX = (float)cmdNum(proc->pc);
    proc->pc += 2;
    mul.x = rawX * 0.01f;
    rawY = (float)cmdNum(proc->pc);
    proc->pc += 2;
    mul.y = rawY * 0.01f;
    rawZ = (float)cmdNum(proc->pc);
    proc->pc += 2;
    mul.z = rawZ * 0.01f;
    rawW = (float)cmdNum(proc->pc);
    proc->pc += 2;
    mul.w = rawW * 0.01f;
    mapMulSet(&mul);
    return 0;
}

int cmdHpPerGet(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int unitNo;
    ObjectTask *unit;
    CalcUnitParam *up;

    proc->pc += 2;
    unitNo = cmdNum(proc->pc);
    proc->pc += 2;
    unit = unitPtrGet(unitNo);
    up = calcUPGet(unit);
    thinkRegSet(regNo & 0xFFFF,
                ((int)(*(short *)((char *)up + CALC_UNIT_PARAM_HP_OFF) * 100)
                 / *(short *)((char *)calcUPGet(unit) + CALC_UNIT_PARAM_MAX_HP_OFF)) & 0xFFFF);
    return 0;
}

int cmdEventSlotGet(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);

    proc->pc += 2;
    thinkRegSet(regNo & 0xFFFF, menuTimeGet() & 0xFFFF);
    return 0;
}

int cmdThinkCamEvent(ThinkProcess *proc)
{
    struct {
        int type;
        int arg;
        int unit;
        int target;
    } ev;
    ThinkProcess save;
    int n = cmdNum(proc->pc);

    proc->pc += 2;
    save = *proc;
    ev.type = 0x19;
    ev.unit = (int)pThinkUnit;
    ev.arg = n;
    ev.target = 0;
    camEventExec((CamEvent *)&ev);
    *proc = save;
    thinkTopSet(pThinkTop);
    thinkContextSet((int)&context);
    return 0;
}

int cmdCfEncountGet(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);

    proc->pc += 2;
    thinkRegSet(regNo & 0xFFFF, cfEncountGet() & 0xFFFF);
    return 0;
}

int cmdCfEventGet(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);

    proc->pc += 2;
    thinkRegSet(regNo & 0xFFFF, cfEventGet() & 0xFFFF);
    return 0;
}

int cmdThinkNoGet(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);

    proc->pc += 2;
    thinkRegSet(regNo & 0xFFFF, thinkNoGet() & 0xFFFF);
    return 0;
}

int cmdStatGet(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int a;
    int b;
    int unitNo;

    proc->pc += 2;
    a = cmdNum(proc->pc);
    proc->pc += 2;
    b = cmdNum(proc->pc);
    proc->pc += 2;
    unitNo = cmdNum(proc->pc);
    proc->pc += 2;
    thinkRegSet(regNo & 0xFFFF, calcStatGet(unitPtrGet(unitNo), a, b) & 0xFFFF);
    return 0;
}

int cmdStatSet(ThinkProcess *proc)
{
    int regNo = thinkRegNo(proc->pc);
    int a;
    int b;
    int unitNo;

    proc->pc += 2;
    a = cmdNum(proc->pc);
    proc->pc += 2;
    b = cmdNum(proc->pc);
    proc->pc += 2;
    unitNo = cmdNum(proc->pc);
    proc->pc += 2;
    thinkRegSet(regNo & 0xFFFF, calcStatSet(unitPtrGet(unitNo), a, b, 0) & 0xFFFF);
    return 0;
}

/*
 * calcStatReset (src/ov01/calc.c, still asm there) resets one status
 * category's clamp for a unit; calcStatResetCha (src/ov01/calc.c) calls it
 * the same way, with the same always-zero fourth word, and names the
 * parameters this declaration reuses.
 */
extern void calcStatReset(ObjectTask *unit, int category, int mask, int unused);

int cmdStatReset(ThinkProcess *proc)
{
    int category;
    int mask;
    int unitNo;

    category = cmdNum(proc->pc);
    proc->pc += 2;
    mask = cmdNum(proc->pc);
    proc->pc += 2;
    unitNo = cmdNum(proc->pc);
    proc->pc += 2;
    calcStatReset(unitPtrGet(unitNo), category, mask, 0);
    return 0;
}

/*
 * The block cmdDeadTime reaches through the pointer stored just past
 * ObjectTask's modeled task/work prefix; only the field this function
 * writes is modeled.
 */
typedef struct UnitDeadTimeBlock {
    unsigned char unmodeled_0[0xD0];
    float value;    /* +0xD0: reciprocal-scaled duration */
} UnitDeadTimeBlock;

/*
 * Offset 0x14 of the record unitPtrGet returns lands just past ObjectTask's
 * modeled task/work prefix (shared.h); the real per-unit allocation is
 * longer than that partial view (shared.h's own XglTaskPrefix comment
 * documents the same gap for the task-specific payload). cmdDeadTime is the
 * only function in this allocation that reads the pointer stored there.
 */
typedef struct UnitDeadTimeRecord {
    unsigned char unmodeled_0[0x14];
    UnitDeadTimeBlock *target;   /* +0x14 */
} UnitDeadTimeRecord;

int cmdDeadTime(ThinkProcess *proc)
{
    int unitNo;
    int duration;

    unitNo = cmdNum(proc->pc);
    proc->pc += 2;
    duration = cmdNum(proc->pc);
    proc->pc += 2;
    ((UnitDeadTimeRecord *)unitPtrGet(unitNo))->target->value = 1.0f / (float)duration;
    return 0;
}

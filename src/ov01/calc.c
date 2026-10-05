/*
 * OV01 original TU 4: 0x00a10bb8..0x00a191c0 (116 functions)
 */
#include "common.h"
#include "main/xgl_2.h"
#include "ov01/battle_init.h"
#include "calc.h"

/* calcCfEncount's own callees; both are still asm in this TU. */
extern void calcPartyStat(int, int);
extern void menuTimeSet(int);

/*
 * calcStatGet (this TU, still asm) as every caller in this file invokes it:
 * a status/flag lookup keyed by category and bit.
 */
extern int calcStatGet(ObjectTask *unit, int category, int flag);

/*
 * The one queued-command word calcTurnStart (this TU) reads
 * through unitCmdPtrGet's return (`lw $3,0x0($2)`), and the two further
 * words calcTurnStart writes through unitCmdSet's second argument
 * (`sw $2,0x0($29)` / `sw $3,0x4($29)` / `sw $0,0x8($29)` at 0x00a12c00,
 * both still asm in src/ov01/unit_cmd.c).
 */
typedef struct CalcCmdEntry CalcCmdEntry;

struct CalcCmdEntry {
    int selector;      /* +0x00 */
    int argument;      /* +0x04 */
    int extra;         /* +0x08: calcTurnStart always sets it to 0 */
    unsigned char unmodeled_0c[8]; /* +0x0C: unwritten by the one evidenced
                                     * call; only its total size (the local's
                                     * stack reservation before unitCmdSet)
                                     * is evidenced here. */
};

extern int calcAp(ObjectTask *unit, int ap);
extern CalcCmdEntry *unitCmdPtrGet(ObjectTask *unit);
extern void unitCmdSet(ObjectTask *unit, CalcCmdEntry *cmd);

/* calcDeadUnitNum: calcSpecChk's own callee, still asm in this TU. */
extern int calcDeadUnitNum(ObjectTask *unit, int flag);

/* calcBattleChk: calcWctDec's own callee, still asm in this TU. */
extern int calcBattleChk(ObjectTask *unit);

/*
 * calcWct/calcStatTurn/rnd: calcTurnEnd's own callees, defined later in
 * this TU (calcWct and rnd below, calcStatTurn still asm).
 */
extern int calcWct(ObjectTask *unit);
extern void calcStatTurn(ObjectTask *unit);
extern int rnd(int max);

/*
 * dataWpnGet's return type, as far as calcAttWpnChk reads it: a byte at
 * +0x10 that it tests for zero/nonzero (data_unit_org_get.c, dataWpnGet's
 * own TU, still asm, owns the rest of the layout).
 */
typedef struct WpnData WpnData;

struct WpnData {
    unsigned char unmodeled_00[0x10]; /* +0x00 */
    u8 equipped;                      /* +0x10: calcAttWpnChk's own check */
};

extern WpnData *dataWpnGet(int weaponId);

/*
 * calcSpline/calcSplineSub (both still asm in this TU) advance this
 * descriptor every frame; calcSplineInit sets it up with the value range,
 * the duration in frames as a float and a fixed 0.5 midpoint, the frame
 * counter starting at zero.
 */
typedef struct CalcSpline {
    int start;
    int end;
    float duration;
    float midpoint;
    int frame;
} CalcSpline;

typedef struct CalcParaTable {
    short maxHp;            /* +0x00 */
    short maxEp;            /* +0x02 */
    short val4;             /* +0x04 */
    short val6;             /* +0x06 */
    short eatk;             /* +0x08 */
    short edef;             /* +0x0A */
    signed char hit;        /* +0x0C */
    signed char eva;        /* +0x0D */
    unsigned char valE;     /* +0x0E */
    unsigned char unmodeled_0f[0x36 - 0x0F];
    short ep;               /* +0x36 */
    short agwsId;           /* +0x38 */
    unsigned char unmodeled_3a[0x56 - 0x3A];
    short engineId;         /* +0x56 */
    short frameId;          /* +0x58 */
    unsigned char unmodeled_5a[0x180 - 0x5A];
} CalcParaTable;
typedef struct CalcPartAdj {
    unsigned char unmodeled_00[8];
    short a8;               /* +0x08 */
    short aA;               /* +0x0A */
    short aC;               /* +0x0C */
    unsigned char aE;       /* +0x0E */
} CalcPartAdj;
extern int unitAgwsPilotGet(int charaId);
extern unsigned char *dataFrmGet(int index);
extern unsigned char *dataEngGet(int index);
extern int printf(const char *format, ...);

void calcCopyParaMake(CalcCharParaData *origin, unsigned char *table)
{
    unsigned char buf[0x180];
    CalcParaTable *t;
    CalcParaTable *r;
    CalcPartAdj *frm;
    CalcPartAdj *eng;
    int pilot;

    frm = 0;
    eng = 0;
    t = (CalcParaTable *)table;
    *t = *(CalcParaTable *)origin;
    if (t->agwsId > 0x10) {
        pilot = unitAgwsPilotGet(t->agwsId);
        if (t->frameId != 0) {
            frm = (CalcPartAdj *)dataFrmGet(t->frameId);
        }
        if (t->engineId != 0) {
            eng = (CalcPartAdj *)dataEngGet(t->engineId);
        }
        if (pilot != 0) {
            r = (CalcParaTable *)calcTotalParaMenuSub(pilot, 0, 0, buf);
            t->eatk = r->eatk;
            t->edef = r->edef;
            t->hit = r->hit;
            t->eva = r->eva;
            t->ep = r->ep;
            t->maxEp = r->maxEp;
            printf("** AGWS para = EP=%d EPMAX=%d\n", t->ep, t->maxEp);
            printf("** AGWS para = Eatk=%d Edef=%d HIT=%d EVA=%d\n", t->eatk, t->edef, t->hit, t->eva);
        }
        if (frm != 0) {
            t->maxHp += frm->a8;
            t->val6 += frm->aA;
            t->valE += frm->aE;
            t->edef += frm->aC;
        }
        if (eng != 0) {
            t->val4 += eng->a8;
        }
    }
}

/*
 * calcTotalParaMenuSub/calcSkillEquChkAll's (this TU) own fake unit: a
 * stack-local ObjectTask/CalcActorRecord pair whose `work`/`up` chain
 * (calcUPGet's own traversal, 0x00a11010..0x00a1101c) is pointed at a
 * temporary CalcUnitParam-compatible record instead of a live battle unit,
 * so the two functions can reuse calcSkillInit/calcWpnAtkGet/calcPhyDefGet/
 * calcMagDefGet/calcSkillEquChk for a unit that has no ObjectTask.
 * calcTotalParaMenuSub allocates 0x80/0x110 bytes for the pair
 * (`addiu v0,sp,128`, sp+0x190 for the outer's own end), matching the
 * ObjectTask/CalcActorRecord prefixes it actually writes through plus the
 * task-specific payload xglTaskEntryNext callers leave unwritten here.
 */
typedef struct CalcScratchActor {
    CalcActorRecord actor;      /* +0x00 */
    unsigned char unmodeled_88[0x110 - 0x88];
} CalcScratchActor;

typedef struct CalcScratchUnit {
    ObjectTask task;            /* +0x00 */
    unsigned char unmodeled_14[0x80 - 0x14];
} CalcScratchUnit;

/* dataUnitOrgGet (src/ov01/data_unit_org_get.c, still asm), this TU's own
 * private view (see CalcCharParaData, src/ov01/calc.h). */
extern CalcCharParaData *dataUnitOrgGet(int charaId);

extern int unitTblGet(int type, ObjectTask ***table);

void calcPara2OrgSub(ObjectTask *unit);

void calcPara2Org(void)
{
    CalcScratchUnit fakeUnit;
    CalcScratchActor fakeActor;
    ObjectTask **table;
    int count;
    int i;

    i = 0;
    count = unitTblGet(0, &table);
    if (count > 0) {
        do {
            if (table[i] != 0) {
                calcPara2OrgSub(table[i]);
                if (calcUPGet(table[i])->flags & 0x40) {
                    fakeUnit.task.work = &fakeActor;
                    fakeActor.actor.up = calcUPGet(table[i])->linkedUnit;
                    fakeActor.actor.flags = 0;
                    fakeActor.actor.up->currentEp = calcUPGet(table[i])->currentEp;
                    calcPara2OrgSub(&fakeUnit.task);
                } else if (calcUPGet(table[i])->linkedUnit != 0) {
                    fakeUnit.task.work = &fakeActor;
                    fakeActor.actor.up = calcUPGet(table[i])->linkedUnit;
                    fakeActor.actor.flags = 0;
                    calcPara2OrgSub(&fakeUnit.task);
                }
            }
            i++;
        } while (i < count);
    }
}

void calcPara2OrgSub(ObjectTask *unit) {
    CalcUnitParam *param;
    CalcCharParaData *origin;

    param = calcUPGet(unit);
    origin = dataUnitOrgGet(param->charaId);
    origin->hp = param->currentHp;
    origin->ep = param->currentEp;
}

void calcDeadReset(void)
{
    ObjectTask **table;
    ObjectTask **t;
    ObjectTask **nt;
    ObjectTask *u;
    CalcCharParaData *origin;
    int count;
    int i;

    i = 0;
    count = unitTblGet(0, &table);
    if (count > 0) {
        t = table;
        do {
            if (t[i] != 0) {
                origin = dataUnitOrgGet(calcUPGet(t[i])->charaId);
                nt = table;
                t = nt;
                u = nt[i];
                if (u == 0 || (((CalcActorRecord *)u->work)->flags & 0x80) || origin->hp == 0) {
                    origin->hp = 1;
                }
            }
            i++;
        } while (i < count);
    }
}

CalcUnitParam *calcUPGet(ObjectTask *unit)
{
    return ((CalcActorRecord *)unit->work)->up;
}

int calcHp(ObjectTask *unit, int delta);
int calcEp(ObjectTask *unit, int delta);

int calcHpDmg(ObjectTask *src, ObjectTask *dst, unsigned char type, int val)
{
    int ret;

    switch (type) {
    case 1:
        ret = calcHp(dst, -val);
        break;
    case 3:
        ret = calcEp(dst, -val);
        break;
    case 2:
        ret = calcHp(dst, val);
        break;
    case 4:
        ret = calcEp(dst, val);
        break;
    case 7:
    case 8:
        ret = calcHp(dst, -val);
        calcHp(src, val);
        break;
    case 9:
    case 10:
        ret = calcEp(dst, -val);
        calcEp(src, val);
        break;
    default:
        ret = 1;
        break;
    }
    return ret;
}

/*
 * calcTotalParaMenuSub (this TU, still asm, 0x00a11128): calcTotalParaMenu's
 * tail-called worker, given the same three arguments plus the per-charaId
 * table below.
 */
extern CalcCharParaData *calcTotalParaMenuSub(int charaId, int *attack, int *defense, unsigned char *table);

/* The per-charaId table calcTotalParaMenu hands calcTotalParaMenuSub. */
extern unsigned char D_00A57CE8[];

CalcCharParaData *calcTotalParaMenu(int charaId, int *attack, int *defense)
{
    return calcTotalParaMenuSub(charaId, attack, defense, D_00A57CE8);
}

/* calcCopyParaMake/calcSkillInit/calcWpnAtkGet/calcPhyDefGet/calcMagDefGet:
 * calcTotalParaMenuSub's own callees, still asm in this TU except
 * calcMagDefGet (this TU, below); the latter four each read their argument
 * through calcUPGet, so calcTotalParaMenuSub hands them its own fake unit
 * above. */
extern void calcCopyParaMake(CalcCharParaData *origin, unsigned char *table);
extern int calcSkillInit(ObjectTask *unit);
extern int calcWpnAtkGet(ObjectTask *unit, int slot);
extern int calcPhyDefGet(ObjectTask *unit);
extern int calcMagDefGet(ObjectTask *unit);

CalcCharParaData *calcTotalParaMenuSub(int charaId, int *attack, int *defense, unsigned char *table)
{
    CalcScratchUnit fakeUnit;
    CalcScratchActor fakeActor;
    CalcCharParaData *scratch;
    CalcCharParaData *origin;
    int i;

    scratch = (CalcCharParaData *) table;
    fakeUnit.task.work = &fakeActor;
    calcCopyParaMake(dataUnitOrgGet(charaId), table);
    fakeActor.actor.up = (CalcUnitParam *) table;
    calcSkillInit(&fakeUnit.task);

    origin = dataUnitOrgGet(charaId);
    if (scratch->maxHp < origin->hp) {
        origin->hp = scratch->maxHp;
    }
    if (origin->ep > scratch->maxEp) {
        origin->ep = scratch->maxEp;
    }
    if (attack != 0) {
        for (i = 0; i < 3; i++) {
            attack[i] = calcWpnAtkGet(&fakeUnit.task, i);
        }
    }
    if (defense != 0) {
        defense[0] = calcPhyDefGet(&fakeUnit.task);
        defense[1] = calcMagDefGet(&fakeUnit.task);
    }
    return scratch;
}

/*
 * The record calcTakeEpGetMenu builds calcTotalParaMenu's recalculated
 * record into; nothing else in this TU fills the rest of it.
 */
typedef struct CalcTakeEpPara {
    unsigned char unmodeled_00[0x84];
    CalcCharParaData *totalPara; /* +0x84: calcTotalParaMenu's own return value */
    unsigned char unmodeled_88[0x110 - 0x88];
} CalcTakeEpPara;

/*
 * The context block calcTakeEpGetMenu builds for its only call to
 * calcTakeEpGet (this TU, still asm, 0x00a15670); nothing else in this TU
 * fills the rest of it.
 */
typedef struct CalcTakeEpArg {
    unsigned char unmodeled_00[0x10];
    CalcTakeEpPara *totalPara; /* +0x10: the record above */
    unsigned char unmodeled_14[0x80 - 0x14];
} CalcTakeEpArg;

extern int calcTakeEpGet(CalcTakeEpArg *arg, int tecId);

/* dataEthGet (src/ov01/data_unit_org_get.c, still asm), this TU's own
 * private view (see CalcEtherStatus, src/ov01/calc.h). */
extern CalcEtherStatus *dataEthGet(int etherId);

int calcTakeEpGetMenu(int charaId, int etherId) {
    CalcTakeEpArg arg;
    CalcTakeEpPara para;

    arg.totalPara = &para;
    para.totalPara = calcTotalParaMenu(charaId, 0, 0);
    return calcTakeEpGet(&arg, dataEthGet(etherId)->tecId);
}

INCLUDE_ASM("asm/nonmatchings/ov01/calc", calcUseEtherMenu);

extern unsigned char *dataItmGet(int index);
extern int PartyAllPartyGet(int *out, int mode);
extern int calcUseItem(int mode, CalcTakeEpArg *arg, int itm);
typedef struct CalcUseItemSub {
    unsigned char unmodeled_00[0xB0];
    short tbl[8]; /* +0xB0 (org/total +0xBC) */
} CalcUseItemSub;

int calcUseItemMenu(int charaId, int itm)
{
    CalcTakeEpArg arg;
    CalcTakeEpPara para;
    int ids[8];
    short save[8];
    CalcCharParaData *org;
    CalcCharParaData *tot;
    short hp, ep;
    CalcUseItemSub *pt;
    unsigned char flag;
    int *p;
    int count;
    int n;
    int i;

    arg.totalPara = &para;
    flag = dataItmGet(itm)[8];
    if (flag & 0x10) {
        p = ids;
        count = PartyAllPartyGet(p, 0);
    } else {
        p = ids;
        count = 1;
        *p = charaId;
    }
    for (n = 0; n < count; n++) {
        org = dataUnitOrgGet(p[n]);
        tot = calcTotalParaMenu(p[n], 0, 0);
        hp = org->maxHp;
        ep = org->maxEp;
        for (i = 0; i < 8; i++) {
            save[i] = ((CalcUseItemSub *)((char *)org + 0xC))->tbl[i];
        }
        pt = (CalcUseItemSub *)((char *)tot + 0xC);
        org->maxHp = tot->maxHp;
        org->maxEp = tot->maxEp;
        for (i = 0; i < 8; i++) {
            ((CalcUseItemSub *)((char *)org + 0xC))->tbl[i] = pt->tbl[i];
        }
        para.totalPara = org;
        calcUseItem(0, &arg, itm);
        org->maxHp = hp;
        org->maxEp = ep;
        for (i = 0; i < 8; i++) {
            ((CalcUseItemSub *)((char *)org + 0xC))->tbl[i] = save[i];
        }
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/calc", calcUseItem);

typedef struct CalcSkillView {
    unsigned char unmodeled_00[0xA6];
    short skillId[3];
} CalcSkillView;

typedef struct CalcAccSkillView {
    unsigned char unmodeled_00[0xE];
    unsigned short skillId;
} CalcAccSkillView;

int calcSkillInitSub(ObjectTask *unit, int skillId);

int calcSkillInit(ObjectTask *unit)
{
    int i;
    int id;
    short acc;

    for (i = 0; i < 3; i++) {
        acc = calcUPGet(unit)->accessoryId[i];
        if (acc != 0) {
            id = ((CalcAccSkillView *)dataAccGet(acc))->skillId;
            if (id != 0) {
                calcSkillInitSub(unit, id);
            }
        }
    }
    for (i = 0; i < 3; i++) {
        id = ((CalcSkillView *)calcUPGet(unit))->skillId[i];
        if (id != 0) {
            calcSkillInitSub(unit, id);
        }
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/calc", calcSkillInitSub);

int calcSkillSub(ObjectTask *unit, int skillId, int typeMask, int levelMask);
int calcSkill(ObjectTask *unit, int typeMask, int levelMask)
{
    int total;
    int i;
    int id;
    short acc;

    total = 0;
    for (i = 0; i < 3; i++) {
        acc = calcUPGet(unit)->accessoryId[i];
        if (acc != 0) {
            id = ((CalcAccSkillView *)dataAccGet(acc))->skillId;
            if (id != 0) {
                total += calcSkillSub(unit, id, typeMask, levelMask);
            }
        }
    }
    for (i = 0; i < 3; i++) {
        id = ((CalcSkillView *)calcUPGet(unit))->skillId[i];
        if (id != 0) {
            total += calcSkillSub(unit, id, typeMask, levelMask);
        }
    }
    return total;
}

/*
 * dataSklGet's return record (src/ov01/data_unit_org_get.c, still asm), as
 * far as calcSkillSub reads it: two halfword bit sets.
 */
typedef struct SkillData SkillData;

struct SkillData {
    u16 typeFlags;   /* +0x00: calcSkillSub's own mask test */
    u16 levelFlags;  /* +0x02: calcSkillSub returns bits 0-6 of this field */
};

extern SkillData *dataSklGet(int skillId);

int calcSkillSub(ObjectTask *unit, int skillId, int typeMask, int levelMask) {
    SkillData *skill;
    unsigned short levelFlags;
    int result;

    skill = dataSklGet(skillId);
    calcUPGet(unit);
    result = 0;
    if (skill->typeFlags & typeMask) {
        levelFlags = skill->levelFlags;
        result = (levelFlags & levelMask) != 0 ? (levelFlags & 0x7F) : 0;
    }
    return result;
}

int calcAccEquChk(ObjectTask *unit, int id) {
    int i;
    int off;

    i = 0;
    off = 0x60;
    do {
        if (id == *(short *)((char *)calcUPGet(unit) + off + 4)) {
            return 1;
        }
        i++;
        off += 2;
    } while (i < 3);
    return 0;
}

int calcSkillEquChk(ObjectTask *unit, int skillId) {
    int i;
    short acc;

    for (i = 0; i < 3; i++) {
        acc = calcUPGet(unit)->accessoryId[i];
        if (acc != 0 && skillId == ((CalcAccSkillView *)dataAccGet(acc))->skillId) {
            return 1;
        }
    }
    for (i = 0; i < 3; i++) {
        if (skillId == ((CalcSkillView *)calcUPGet(unit))->skillId[i]) {
            return 1;
        }
    }
    return 0;
}

/* calcSkillEquChk/unitTblGet: calcSkillEquChkAll's own callees, still asm
 * in this TU. calcSkillEquChk's second call below hands it the same fake
 * unit calcMagDefGet (this TU) takes; unitTblGet writes the active unit
 * table's base address through its second argument and returns its count. */
extern int calcSkillEquChk(ObjectTask *unit, int skillId);
extern int unitTblGet(int type, ObjectTask ***table);

int calcSkillEquChkAll(int skillId)
{
    CalcScratchUnit fakeUnit;
    CalcScratchActor fakeActor;
    ObjectTask **table;
    ObjectTask *unit;
    int count;
    int i;
    int found;

    i = 0;
    count = unitTblGet(0, &table);
    found = 0;
    if (count > 0) {
        for (; i < count; i++) {
            unit = table[i];
            if (unit != 0) {
                found = 1;
                if (calcSkillEquChk(unit, skillId) == 0) {
                    if (calcUPGet(table[i])->flags & 0x40) {
                        fakeUnit.task.work = &fakeActor;
                        fakeActor.actor.up = calcUPGet(table[i])->linkedUnit;
                        if (calcSkillEquChk(&fakeUnit.task, skillId) != 0) {
                            return 1;
                        }
                    }
                } else {
                    return found;
                }
            }
        }
        return 0;
    }
    return found;
}

int calcAp(ObjectTask *unit, int delta)
{
    int ap;

    ap = *(signed char *)&calcUPGet(unit)->unmodeled_3a[0];
    if (delta < 0 && ap < -delta)
    {
        return 0;
    }
    ap += delta;
    if (ap > 6)
    {
        ap = 6;
    }
    calcUPGet(unit)->unmodeled_3a[0] = ap;
    return 1;
}

extern int printf(const char *format, ...);
extern int plMutekiGet(void);
extern int enMutekiGet(void);

int calcHp(ObjectTask *unit, int delta)
{
    int hp;

    if (!(*(unsigned int *)unit->work & 0x40)) {
        if (plMutekiGet() != 0) {
            printf("** PL MUTEKI\n");
            return 1;
        }
    } else if (enMutekiGet() != 0) {
        printf("** EN MUTEKI\n");
        return 1;
    }
    hp = (short)calcUPGet(unit)->currentHp;
    hp += delta;
    if (hp < 0) {
        hp = 0;
    } else if (*(short *)calcUPGet(unit) < hp) {
        hp = *(short *)calcUPGet(unit);
    }
    calcUPGet(unit)->currentHp = hp;
    return hp != 0;
}

int calcEp(ObjectTask *unit, int delta)
{
    int ep;

    ep = (short) calcUPGet(unit)->currentEp;
    ep += delta;
    if (ep < 0)
    {
        ep = 0;
    }
    else if (calcUPGet(unit)->maxEp < ep)
    {
        ep = calcUPGet(unit)->maxEp;
    }
    calcUPGet(unit)->currentEp = ep;
    return 1;
}

int calcBp(ObjectTask *unit, int delta)
{
    CalcUnitParam *up;
    signed char level;
    signed char max;
    unsigned char lv;
    unsigned char mx;
    int temp;

    up = calcUPGet(unit);
    level = up->unmodeled_3a[2];
    max = up->unmodeled_3a[1];
    lv = up->unmodeled_3a[2];
    mx = up->unmodeled_3a[1];
    if (level < max)
    {
        temp = up->unmodeled_3a[3] + delta;
        up->unmodeled_3a[3] = temp;
        if ((signed char)temp >= 50)
        {
            up->unmodeled_3a[3] = (signed char)temp % 50;
            calcBc((BattleUnit *)unit, 1);
            lv = up->unmodeled_3a[2];
            mx = up->unmodeled_3a[1];
        }
        if ((lv << 24) >= (mx << 24))
        {
            goto reset;
        }
    }
    else
    {
reset:
        up->unmodeled_3a[3] = 0;
    }
    return 1;
}

extern unsigned short *dataTecGet(int tecId);
extern int menuTimeGet(void);
extern int printf(const char *format, ...);

int calcBp2Tid(ObjectTask *unit, int tecId)
{
    int delta;
    signed char cat;

    if (*(unsigned *)unit->work & 0x40) {
        return 0;
    }
    delta = 4;
    cat = ((signed char *)dataTecGet(tecId))[0x11];
    if (cat != 0) {
        delta = 6;
        if (cat != 1) {
            delta = 10;
            if (cat != 2) {
                delta = 0;
            }
        }
    }
    if (menuTimeGet() == 2) {
        delta *= 3;
        printf("** BP up: EventTimer G (%d)\n", delta);
    }
    calcBp(unit, delta);
    return 1;
}

int calcBc(BattleUnit *bu, int delta)
{
    ObjectTask *unit = (ObjectTask *)bu;
    int bc;

    bc = *((signed char *)calcUPGet(unit) + 0x3C);
    bc += delta;
    if (bc < 0)
    {
        bc = 0;
    }
    else if (*((signed char *)calcUPGet(unit) + 0x3B) < bc)
    {
        bc = *((signed char *)calcUPGet(unit) + 0x3B);
    }
    *((signed char *)calcUPGet(unit) + 0x3C) = bc;
    return 1;
}

extern unsigned short *dataTecGet(int tecId);
extern int calcSkill(ObjectTask *unit, int a, int b);

int calcCounter(ObjectTask *unit, int tecId)
{
    int rate;

    if (dataTecGet(tecId)[1] & 0x800) {
        return 0;
    }
    rate = *((signed char *)calcUPGet(unit) + 0x16);
    rate += calcSkill(unit, 0x4000, 0x4000);
    if (rate > 100) {
        rate = 100;
    }
    return rnd(100) < rate;
}

extern BattleUnit *actUnitGet(void);
extern int *actStockGet(void);

int calcBoostChk(ObjectTask *unit, int mode)
{
    if (unit != 0 && !(*(u64 *)unit->work & 0x480)
        && actUnitGet() != (BattleUnit *)unit
        && ((CalcActStock *)actStockGet())->slot[mode] == 0
        && (short)calcUPGet(unit)->currentHp != 0) {
        if ((calcUPGet(unit)->flags & 0x40) && calcAccEquChk(unit, 0x83) == 0) {
            return 0;
        }
        if (mode == 1 && *((signed char *)calcUPGet(unit) + 0x3C) <= 0) {
            return 0;
        }
        if (calcStatGet(unit, 1, 4) == 0) {
            goto ok;
        }
    }
    return 0;
ok:
    if (mode == 1) {
        if (((CalcActorRecord *)unit->work)->flags & 0x10) {
            return 0;
        }
        return 1;
    }
    return 1;
}

extern int *actStockGet(void);
int calcBc(BattleUnit *unit, int delta);
void decoEffCall(BattleUnit *unit, int effectId);
int sndSysSePlay(int soundId);

int calcBoost(BattleUnit *unit, int mode) {
    CalcActorRecord *work;

    if (mode == 1) {
        calcBc(unit, -1);
    }
    work = (CalcActorRecord *) ((ObjectTask *) unit)->work;
    work->flags |= 0x400;
    ((CalcActStock *) actStockGet())->slot[mode] = (ObjectTask *) unit;
    decoEffCall(unit, 0x64);
    return sndSysSePlay(9);
}

int calcIdol2Chk(ObjectTask *unit)
{
    short v = *(short *)((char *)calcUPGet(unit) + 0x34);

    if (!(((CalcActorRecord *)unit->work)->flags & 0x40)) {
        if (v < *(short *)calcUPGet(unit) / 5) {
            return 1;
        }
    }
    return 0;
}

int calcTuigekiChk(ObjectTask *unit)
{
    return calcStatGet(unit, 7, 0x80) != 0;
}

int calcSpecChk(ObjectTask *unit, int specId)
{
    int result;

    if (specId == 0x34 && calcDeadUnitNum(unit, 0x40) <= 0)
    {
        return 0;
    }

    if (specId != 0x5A || (result = calcStatGet(unit, 7, 2)) != 0)
    {
        if (specId != 0x59 || (result = calcStatGet(unit, 7, 1)) != 0)
        {
            result = 1;
        }
    }

    return result;
}

extern unsigned char *dataItmGet(int index);
int dataItmBoxChk(int id);

int calcDItemChk(ObjectTask *unit, int itm)
{
    if (((signed char *)calcUPGet(unit))[0x3A] >= 6
        && !(dataItmGet(itm)[8] & 0x10)
        && dataItmBoxChk(itm) >= 2) {
        unsigned char f = dataItmGet(itm)[9] & 1;
        if (f) {
            return 1;
        }
    }
    return 0;
}

void calcTurnStart(ObjectTask *unit)
{
    int ap = 4;
    int apForSelect;
    CalcActorRecord *work = (CalcActorRecord *) unit->work;

    if (calcStatGet(unit, 2, 0x10) != 0 || calcStatGet(unit, 6, 0x1000) != 0)
    {
        ap = 6;
    }

    if (calcStatGet(unit, 0, 0x40) != 0 || calcStatGet(unit, 4, 0x20) != 0)
    {
        ap /= 2;
    }

    apForSelect = ap;
    calcAp(unit, calcStatGet(unit, 7, 0x20) ? apForSelect + 1 : ap);

    if (((CalcActorRecord *) unit->work)->flags & 0x800)
    {
        CalcCmdEntry *cmd = unitCmdPtrGet(unit);

        if (cmd->selector == 0)
        {
            CalcCmdEntry newCmd;

            newCmd.selector = 0xD;
            newCmd.argument = 0x1C;
            newCmd.extra = 0;
            unitCmdSet(unit, &newCmd);
        }

        work->flags &= ~0x800;
    }
}

void calcTurnEnd(ObjectTask *unit)
{
    CalcUnitParam *up;
    short wct;

    ((CalcActorRecord *) unit->work)->flags &= ~0x6418;

    up = calcUPGet(unit);
    calcUPGet(unit)->waitCount = wct = calcWct(unit);
    up->waitCountBase = wct;

    calcUPGet(unit)->turnBias = 0;

    calcStatTurn(unit);
}

int calcWct(ObjectTask *unit)
{
    int base;
    int accum = 0;

    base = (calcUPGet(unit)->turnBias - calcUPGet(unit)->agility) * 9 + 0xB4;

    if (calcStatGet(unit, 0, 0x1000) != 0 || calcStatGet(unit, 4, 0x800) != 0)
    {
        accum = base / 2;
    }

    if (calcStatGet(unit, 2, 0x100) != 0 || calcStatGet(unit, 6, 0x4000) != 0)
    {
        accum -= base / 4;
    }

    if (calcStatGet(unit, 2, 0x80) != 0 || calcStatGet(unit, 6, 0x2000) != 0)
    {
        accum -= base / 2;
    }

    return base + accum;
}

int calcWctDec(ObjectTask *unit)
{
    int wait = calcUPGet(unit)->waitCount;

    if (calcBattleChk(unit) != 0)
    {
        int decremented = wait - ((rnd(100) < 30) ? 1 : 0);

        wait = decremented - 1;
        if (wait < 0)
        {
            wait = 0;
        }
    }

    calcUPGet(unit)->waitCount = wait;
    return wait;
}

extern int dataMtdRead(ObjectTask *unit, int n);

int calcHitMotChk(ObjectTask *unit)
{
    int r;
    if ((unsigned)(unitCmdPtrGet(unit)->selector - 1) >= 3) {
        return 0;
    }
    r = dataMtdRead(unit, 1);
    switch (r) {
    case 0:
        return 0;
    case 1:
        return r;
    }
    return 0;
}

typedef struct CalcHitDmgArg {
    int unk0;
    ObjectTask *unit;
} CalcHitDmgArg;
typedef struct CalcHitDmgOut {
    int hit;
    int dmg;
    int unk8;
    int unkC;
    int unk10;
    int unk14;
    int unk18;
} CalcHitDmgOut;
extern int calcHit(ObjectTask *attacker, CalcHitDmgArg *arg, CalcHitDmgOut *out);
extern int calcDmg(ObjectTask *attacker, CalcHitDmgArg *arg, CalcHitDmgOut *out);
extern int plIchigekiGet(void);
extern int cfEventGet(void);

int calcHitDmg(ObjectTask *attacker, CalcHitDmgArg *arg, CalcHitDmgOut *out)
{
    unsigned char hit;
    int r;
    CalcUnitParam *up;

    out->unkC = 8;
    out->unk14 = 8;
    out->unk10 = 0;
    out->unk18 = 0;
    r = calcHit(attacker, arg, out);
    out->hit = r;
    if ((unsigned char)r == 0) {
        out->dmg = 0;
        printf("** DMG: EVA \n");
    } else {
        out->dmg = calcDmg(attacker, arg, out);
    }
    if (!(*(unsigned *)attacker->work & 0x40) && (cfEventGet() & 1)
        && (*(unsigned *)arg->unit->work & 0x40)) {
        out->dmg = 0;
        return 1;
    }
    hit = out->hit;
    if (!(*(unsigned *)attacker->work & 0x40) && plIchigekiGet() != 0) {
        if (hit < 2) {
            out->hit = 1;
            up = calcUPGet(arg->unit);
            out->unk8 = 1;
            out->dmg = *(short *)up->unmodeled_00;
            printf("** PL ICHIGEKI\n");
        }
    }
    return 1;
}

typedef struct CalcHitRateArg {
    int tecId;
    ObjectTask *unit;
} CalcHitRateArg;
extern int calcHitParaGet(ObjectTask *unit);

int calcHitRate(ObjectTask *attacker, CalcHitRateArg *arg)
{
    int hit;
    int eva;
    int rate;

    hit = calcHitParaGet(attacker);
    eva = (signed char)calcUPGet(arg->unit)->unmodeled_0d[0];
    if (calcStatGet(arg->unit, 0, 0x200) != 0 || calcStatGet(arg->unit, 4, 0x100) != 0) {
        eva /= 2;
    } else if (calcStatGet(attacker, 2, 0x20) != 0) {
        eva += eva / 4;
    }
    if ((short)calcUPGet(arg->unit)->currentHp == 0) {
        return 0;
    }
    if (*(unsigned *)arg->unit->work & 0x800) {
        return 1;
    }
    if (dataTecGet(arg->tecId)[1] & 0x200) {
        return 0;
    }
    rate = hit - eva;
    if (rnd(100) - rate >= 90 && !(calcUPGet(arg->unit)->flags & 1)
        && !(dataTecGet(arg->tecId)[1] & 0x400) && calcBattleChk(arg->unit) != 0) {
        return 2;
    }
    if (rnd(100) - rate >= 85 && !(calcUPGet(arg->unit)->flags & 2) && calcBattleChk(arg->unit) != 0) {
        return 1;
    }
    return 0;
}

int calcHitParaGet(ObjectTask *unit) {
    int hitRate;

    hitRate = (signed char) calcUPGet(unit)->hitStat;
    if (calcStatGet(unit, 0, 0x400) != 0 || calcStatGet(unit, 4, 0x200) != 0) {
        hitRate /= 2;
    } else if (calcStatGet(unit, 2, 0x40) != 0) {
        hitRate += hitRate / 4;
    }
    return hitRate;
}

int calcHit(ObjectTask *attacker, CalcHitDmgArg *arg, CalcHitDmgOut *out)
{
    unsigned short *tec;
    unsigned short attr;
    int r;
    int hr;

    tec = dataTecGet(arg->unk0);
    attr = tec[1];
    switch (((signed char *)tec)[4]) {
    case 0:
        r = 0;
        if (!(calcUPGet(arg->unit)->flags & 0x80)) {
            hr = calcHitRate(attacker, (CalcHitRateArg *)arg);
            r = (attr & 0x40) ? 3 : 1;
            if (hr == 1) {
                r |= 0x200;
            } else if (hr == 2) {
                r = 0;
            }
        }
        break;
    case 1:
        r = (attr & 0x40) ? 4 : 2;
        break;
    case 2:
        r = 0x802;
        break;
    case 3:
        r = 5;
        break;
    case 4:
        r = 6;
        break;
    case 5:
        r = (attr & 0x40) ? 9 : 7;
        if (calcHitRate(attacker, (CalcHitRateArg *)arg) == 1) {
            r |= 0x200;
        }
        break;
    case 6:
        r = (attr & 0x40) ? 10 : 8;
        if (calcHitRate(attacker, (CalcHitRateArg *)arg) == 1) {
            r |= 0x200;
        }
        break;
    default:
        printf("** jobType err -> %d\n", arg->unk0);
        r = 1;
        break;
    }
    return r;
}

extern int calcAttWpnChk(int weaponId);
extern unsigned char *dataAttGet(int index);

int calcWpnAtkGet(ObjectTask *unit, int slot)
{
    int atk;
    short id;
    short att;

    atk = 0;
    if (slot != -1) {
        id = calcUPGet(unit)->sefSetupParams[slot];
        if (id != 0) {
            if (calcAttWpnChk(id) != 0) {
                att = calcUPGet(unit)->sefSetupParams[slot + 6];
                if (att != 0) {
                    atk = *(short *)dataAttGet(att);
                } else {
                    printf("** calcWpnAtkGet: Att equip err! %d\n", id);
                }
            } else {
                atk = *(short *)dataWpnGet(id);
            }
        }
        if (*(int *)((char *)calcUPGet(unit) + 0x17C) != 0) {
            calcUPGet(unit);
            if (!(*(unsigned *)unit->work & 0x40) && !(calcUPGet(unit)->flags & 0x40)
                && (calcUPGet(unit)->charaId < 0xBB || calcUPGet(unit)->charaId >= 0xC3)) {
                atk = *(short *)((char *)calcUPGet(unit) + 4);
            }
            atk += atk * *(int *)((char *)calcUPGet(unit) + 0x17C) / 100;
            printf("** calcWpnAtkGet: wpnAtkAdj=%d\n", atk);
        }
    }
    return atk;
}

int calcPhyDefGet(ObjectTask *unit) {
    int res;
    int i;

    res = 0;
    for (i = 0; i < 3; i++) {
        short id = calcUPGet(unit)->accessoryId[i];
        if (id != 0) {
            res += ((CalcAccSkillView *)dataAccGet(id))->unmodeled_00[0xC];
        }
    }
    for (i = 0; i < 3; i++) {
        short id = calcUPGet(unit)->sefSetupParams[i];
        if (id != 0) {
            res += *(short *)(((char *)dataWpnGet(id)) + 0x1C);
        }
    }
    return res;
}

/* dataAccGet (src/ov01/data_unit_org_get.c, still asm), this TU's own
 * private view (see AccessoryData, src/ov01/calc.h). */
extern AccessoryData *dataAccGet(int accessoryId);

int calcMagDefGet(ObjectTask *unit)
{
    int magicDef;
    int i;
    short accessoryId;

    magicDef = 0;
    for (i = 0; i < 3; i++) {
        accessoryId = calcUPGet(unit)->accessoryId[i];
        if (accessoryId != 0) {
            magicDef += dataAccGet(accessoryId)->magicDefense;
        }
    }
    return magicDef;
}

extern short *dataSpecDataGet(int chara, int id);
extern char *dataPlChaGet(int chara);

int calcPowLevGet(ObjectTask *unit, int id)
{
    int pow;
    short *spec;

    pow = ((unsigned char *)dataTecGet(id))[6];
    if (((signed char *)dataTecGet(id))[0x11] == 2 && (*(unsigned *)unit->work & 0x40) == 0
        && (calcUPGet(unit)->flags & 0x40) == 0
        && (calcUPGet(unit)->charaId < 0xBB || calcUPGet(unit)->charaId >= 0xC3)) {
        spec = dataSpecDataGet(calcUPGet(unit)->charaId, id);
        if (spec != 0) {
            pow += spec[2];
        }
    }
    if ((unsigned)(id - 0x262) < 2) {
        pow += *(short *)(dataPlChaGet(calcUPGet(unit)->charaId) + 0x9A) / 5;
    }
    return pow;
}

int calcPlusAglGet(ObjectTask *unit, int id)
{
    int agl;
    short *spec;

    agl = ((signed char *)dataTecGet(id))[7];
    if (((signed char *)dataTecGet(id))[0x11] == 2 && (*(unsigned *)unit->work & 0x40) == 0
        && (calcUPGet(unit)->flags & 0x40) == 0
        && (calcUPGet(unit)->charaId < 0xBB || calcUPGet(unit)->charaId >= 0xC3)) {
        spec = dataSpecDataGet(calcUPGet(unit)->charaId, id);
        if (spec != 0) {
            agl += spec[3];
        }
    }
    return agl;
}

INCLUDE_ASM("asm/nonmatchings/ov01/calc", calcDmg);

int futtobiChk(ObjectTask *unit, unsigned short *tec)
{
    ObjectTask **table;
    int count;
    int i;
    int lv;

    if ((tec[1] & 0x100) == 0) {
        return 0;
    }
    if ((*(unsigned *)unit->work & 0x40)
        || (calcUPGet(unit)->flags & 0x40)
        || (calcUPGet(unit)->charaId >= 0xBB && calcUPGet(unit)->charaId < 0xC3)) {
        return 0;
    }
    lv = *(short *)calcUPGet(unit)->unmodeled_44;
    if (lv >= 4) {
        return 0;
    }
    count = unitTblGet(0, &table);
    lv += 3;
    for (i = 0; i < count; i++) {
        if (table[i] != 0 && *(short *)calcUPGet(table[i])->unmodeled_44 == lv) {
            return 0;
        }
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/calc", calcStat);

INCLUDE_ASM("asm/nonmatchings/ov01/calc", calcStatAtk);

typedef struct CalcStatClrArg {
    int tecId;
    ObjectTask *unit;
} CalcStatClrArg;
typedef struct CalcStatClrOut {
    int pad[5];
    int category;
    int mask;
} CalcStatClrOut;

int calcStatClr(int unused, CalcStatClrArg *arg, CalcStatClrOut *out)
{
    unsigned char *tec;
    int cat;
    int val;

    tec = (unsigned char *)dataTecGet(arg->tecId);
    if ((signed char)tec[4] == 4) {
        if (calcBattleChk(arg->unit) == 0) {
            ((CalcActorRecord *)arg->unit->work)->flags &= ~0x10;
        }
        cat = (signed char)tec[0xC];
        if (cat == 8) {
            out->category = cat;
            out->mask = 0xFFFF;
        } else {
            out->category = cat;
            out->mask = *(unsigned short *)(tec + 0xE);
        }
        return 1;
    }
    out->mask = 0;
    out->category = 8;
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov01/calc", calcCrit);

INCLUDE_ASM("asm/nonmatchings/ov01/calc", calcSpec);

int calcLinePos(int unused, CalcStatClrArg *arg, unsigned char *mode, int *val)
{
    int ret;
    unsigned char m;
    int line;

    ret = 0;
    m = *mode;
    line = dataPosLineGet(*(short *)((char *)calcUPGet(arg->unit) + 0x44));
    if (m == 1 && (*(unsigned *)arg->unit->work & 0x40) == 0
        && (calcUPGet(arg->unit)->flags & 0x40) == 0
        && (calcUPGet(arg->unit)->charaId < 0xBB || calcUPGet(arg->unit)->charaId >= 0xC3)
        && line == 2) {
        ret = 1;
        *val -= *val / 5;
        printf("** LinePos: adjust\n");
    }
    return ret;
}

int calcRevHit(int hitMode)
{
    int mode = hitMode & 0xFF;
    int side = (hitMode >> 8) & 0xFF;

    switch (mode)
    {
        case 1:
            mode = 2;
            break;

        case 2:
            mode = 1;
            break;

        case 3:
            mode = 4;
            break;

        case 4:
            mode = 3;
            break;
    }

    return mode | (side << 8);
}

int calcAtrChg(ObjectTask *unit, int attribute) {
    int result;

    result = attribute;
    if (calcStatGet(unit, 3, 0x4000) != 0) {
        result = (result & ~0x1F) | 0x10;
    } else if (calcStatGet(unit, 3, 0x2000) != 0) {
        result = (result & ~0x1F) | 8;
    } else if (calcStatGet(unit, 3, 0x1000) != 0) {
        result = (result & ~0x1F) | 4;
    }
    return result;
}

int calcTakeEpGet(CalcTakeEpArg *arg, int tecId)
{
    int ep;

    ep = *((signed char *)dataTecGet(tecId) + 1);
    ep = calcStatGet((ObjectTask *)arg, 1, 0x10) ? ep * 2 : ep;
    if (calcStatGet((ObjectTask *)arg, 3, 0x200)) {
        ep /= 2;
    }
    ep = calcStatGet((ObjectTask *)arg, 3, 0x40) ? ep * 2 : ep;
    if (calcStatGet((ObjectTask *)arg, 7, 0x100) && calcSlotAChk((ObjectTask *)arg, tecId)) {
        ep = 0;
    }
    return ep < 100 ? ep : 99;
}

int calcSlotAChk(ObjectTask *unit, int id) {
    int i;
    int off;

    i = 0;
    off = 0x80;
    do {
        if (*(short *)((char *)calcUPGet(unit) + off + 2) == id) {
            return 1;
        }
        i++;
        off += 2;
    } while (i < 2);
    return 0;
}

int calcAttWpnChk(int weaponId)
{
    return dataWpnGet(weaponId)->equipped != 0;
}

typedef struct CalcHitItemArg {
    int unk0;
    ObjectTask *unit;
    int itm;
} CalcHitItemArg;
typedef struct CalcHitItemOut {
    int unk0[3];
    int unkC;
    int unk10;
    int unk14;
    int unk18;
} CalcHitItemOut;

int calcHitItem(ObjectTask *attacker, CalcHitItemArg *arg, CalcHitItemOut *out)
{
    unsigned char *itm;
    unsigned short flags;
    int hit;

    itm = dataItmGet(arg->itm);
    out->unkC = 8;
    out->unk10 = 0;
    out->unk14 = 8;
    out->unk18 = 0;
    flags = *(unsigned short *)(itm + 0xC);
    if (flags & 1) {
        hit = 2;
        if (calcUPGet(arg->unit)->flags & 0x200) {
            hit = calcRevHit(2);
        }
        if (calcStatGet(arg->unit, 0, 0x10) != 0) {
            hit = calcRevHit(hit);
        }
    } else if (flags & 2) {
        hit = 4;
    } else if (flags & 4) {
        hit = 6;
    } else if (flags & 0x10) {
        hit = 5;
    } else if (flags & 0x20) {
        hit = 5;
    } else if (flags & 8) {
        hit = 0x802;
    } else if (flags & 0x100) {
        hit = itm[10] == 1 ? 1 : 6;
    } else {
        hit = 1;
    }
    return hit;
}

extern void calcStatReset(ObjectTask *unit, int category, int mask, int unused);

int calcDmgItem(ObjectTask *attacker, CalcHitItemArg *arg)
{
    unsigned char *itm;
    unsigned short flags;
    int dmg;

    dmg = 0;
    itm = dataItmGet(arg->itm);
    flags = *(unsigned short *)(itm + 0xC);
    if (flags & 1) {
        int n = itm[10] * 25;

        dmg = n * 2;
        if (calcStatGet(arg->unit, 3, 0x80) != 0) {
            dmg = n * 4;
            calcStatReset(arg->unit, 3, 0x80, 0);
        }
        if (calcStatGet(arg->unit, 0, 8) != 0) {
            dmg /= 2;
        }
        if (calcUPGet(arg->unit)->flags & 0x200) {
            dmg = -dmg;
        }
        if (calcStatGet(arg->unit, 0, 0x10) != 0) {
            dmg = -dmg;
        }
        if (itm[9] & 2) {
            if (calcUPGet(arg->unit)->flags & 0x1000) {
                dmg = 0;
            }
        }
    } else if (flags & 2) {
        int n = itm[10] * 5;

        dmg = n * 2;
        if (calcStatGet(arg->unit, 3, 0x80) != 0) {
            dmg = n * 4;
            calcStatReset(arg->unit, 3, 0x80, 0);
        }
        if (calcStatGet(arg->unit, 1, 0x40) != 0) {
            dmg /= 2;
        }
    } else if (flags & 8) {
        dmg = itm[10];
        dmg = *(short *)calcUPGet(arg->unit) * dmg / 20;
    } else if (flags & 0x2000) {
        dmg = itm[10];
        dmg = *(short *)calcUPGet(arg->unit) * dmg / 20;
    } else if (flags & 0x100) {
        if (itm[10] == 1) {
            dmg = (short)calcUPGet(arg->unit)->currentHp - 1;
        }
    }
    return dmg;
}

INCLUDE_ASM("asm/nonmatchings/ov01/calc", tgtFindFirst);

typedef struct TgtFindCmd {
    int id;
    unsigned char unmodeled_04[8];
    int kind;
} TgtFindCmd;
extern int calcAtkChk(ObjectTask *a, ObjectTask *b, int id);

int tgtFindChk(ObjectTask *attacker, ObjectTask *target, TgtFindCmd *cmd)
{
    int ret;

    if (cmd->kind != 3) {
        ret = calcAtkChk(attacker, target, cmd->id);
    } else {
        ret = 1;
    }
    return ret;
}

/*
 * tgtFindFirst's own candidate table (this TU, still asm): tgtNum entries
 * of tgtTbl are filled in target-search order, tgtIdx walks them out one at
 * a time. The gap between tgtTbl and tgtIdx (0x00a57e68..0x00a57e88, 0x20
 * bytes) is this array's only size evidence.
 */
extern ObjectTask *tgtTbl[8];
extern int tgtIdx;
extern int tgtNum;

ObjectTask *tgtFindNext(void)
{
    if (tgtIdx >= tgtNum) {
        return 0;
    }
    return tgtTbl[tgtIdx++];
}

extern char cur1[];

int calcMoveChk(ObjectTask *unit, int id)
{
    short n;
    int *p;
    int i;

    n = *(short *)((char *)calcUPGet(unit) + 0x44);
    if (!(*(unsigned *)unit->work & 0x40)) {
        if (!(calcUPGet(unit)->flags & 0x40)) {
            p = (int *)(cur1 + 0x5C + n * 0x14);
            i = 0;
            do {
                if (*p != -1 && *p == id) {
                    return 1;
                }
                p++;
                i++;
            } while (i < 5);
            return 0;
        }
    }
    return 1;
}

extern short dataPosLineGet(int id);
int calcA2Chk(ObjectTask *unit);
int calcLineChk(ObjectTask *unit);

int calcAtkChk(ObjectTask *a, ObjectTask *b, int id)
{
    int line;
    int type;

    line = dataPosLineGet(*(short *)((char *)calcUPGet(a) + 0x44));
    type = *(unsigned char *)dataTecGet(id) & 3;
    if (type == 0 && (calcUPGet(b)->flags & 0x100)) {
        return 0;
    }
    if (calcStatGet(b, 1, 0x200) != 0) {
        return 0;
    }
    if (!(*(unsigned *)a->work & 0x40)) {
        if ((unsigned)line >= 2 && type != 2) {
            return 0;
        }
    } else if (type != 2) {
        if (calcLineChk(a) == 0) {
            return 0;
        }
        if (calcA2Chk(b) == 0) {
            return 0;
        }
    }
    if (calcLineChk(b) == 0) {
        return type == 2;
    }
    return 1;
}

int calcA2Chk(ObjectTask *unit)
{
    ObjectTask **table;
    ObjectTask *u;
    int count;
    int i;
    unsigned flag;

    if (*(short *)((char *)calcUPGet(unit) + 0x44) == 8) {
        i = 0;
        flag = *(unsigned *)unit->work & 0x40;
        count = unitTblGet(flag != 0, &table);
        if (count > 0) {
            do {
                u = table[i];
                i++;
                if (u != 0 && !(*(unsigned *)u->work & 0x80)
                    && (unsigned)(*(short *)((char *)calcUPGet(u) + 0x44) - 1) < 6) {
                    return 0;
                }
            } while (i < count);
        }
    }
    return 1;
}

extern short dataPosLineGet(int id);

int calcLineChk(ObjectTask *unit)
{
    ObjectTask **table;
    ObjectTask *u;
    int count;
    int i;
    short line;
    short l;
    unsigned flag;

    line = dataPosLineGet(*(short *)((char *)calcUPGet(unit) + 0x44));
    if (line == 0) {
        return 1;
    }
    i = 0;
    flag = *(unsigned *)unit->work & 0x40;
    count = unitTblGet(flag != 0, &table);
    for (; i < count; i++) {
        u = table[i];
        if (u == 0) continue;
        if (*(unsigned *)u->work & 0x80) continue;
        l = dataPosLineGet(*(short *)((char *)calcUPGet(u) + 0x44));
        if (l == 0) continue;
        if (l < line) {
            return 0;
        }
    }
    return 1;
}

int calcBackLineChk(ObjectTask *unit, int mode)
{
    ObjectTask **table;
    ObjectTask *u;
    int count;
    int i;
    short line;
    short l;
    unsigned flag;

    line = dataPosLineGet(*(short *)((char *)calcUPGet(unit) + 0x44));
    if (line == 0) {
        return 1;
    }
    i = 0;
    flag = *(unsigned *)unit->work & 0x40;
    count = unitTblGet(flag != 0, &table);
    for (; i < count; i++) {
        u = table[i];
        if (u == 0) continue;
        if (*(unsigned *)u->work & 0x80) continue;
        l = dataPosLineGet(*(short *)((char *)calcUPGet(u) + 0x44));
        if (l == 0) continue;
        if (line < l) {
            switch (mode) {
            case 0:
                if (calcUPGet(table[i])->flags & 0x800) {
                    return 0;
                }
                break;
            case 1:
                if (calcUPGet(table[i])->charaId == 6) {
                    return 0;
                }
                break;
            }
        }
    }
    return 1;
}

int calcWpnEnableChk(ObjectTask *unit, int slot)
{
    WpnData *wpn;
    CalcUnitParam *up;
    short id;

    up = calcUPGet(unit);
    id = up->sefSetupParams[slot];
    if (id == 0) {
        return 0;
    }
    wpn = dataWpnGet(id);
    dataTecGet(*(short *)((char *)wpn + 0x14));
    if (calcAttWpnChk(id) != 0) {
        if (((short *)up->unmodeled_6a)[slot] == 0) {
            return 0;
        }
        if ((short)up->statActiveMask[slot - 30] <= 0) {
            return 0;
        }
    }
    if ((signed char)wpn->unmodeled_00[0xF] > (signed char)up->unmodeled_3a[0]) {
        return 0;
    }
    if (calcTakeEpGet((CalcTakeEpArg *)unit, *(short *)((char *)wpn + 0x14)) > (short)up->currentEp) {
        return 0;
    }
    return 1;
}

int calcWpnUse(ObjectTask *unit, int idx)
{
    CalcUnitParam *up;
    short *p;
    int w;

    if (idx == -1 || idx >= 4) {
        return 1;
    }
    up = calcUPGet(unit);
    idx <<= 1;
    w = *(short *)(idx + (int)up + 0x5E);
    idx += 0x70;
    if (calcAttWpnChk(w)) {
        p = (short *)((char *)up + idx);
        if (*p != 0xFF) {
            (*p)--;
        }
        if (*p < 0) {
            *p = 0;
        }
    }
    return 1;
}

int calcWpnEquipOrg(int charaId, int slot, int weaponId, int hand) {
    CalcCharParaData *origin;

    if (slot >= 3 || charaId >= 0x21) {
        return 0;
    }
    origin = dataUnitOrgGet(charaId);
    origin->weaponId[slot] = (short) weaponId;
    origin->hand[slot] = (signed char) hand;
    return 1;
}

int calcAccEquipOrg(int charaId, int slot, int accessoryId) {
    if (slot >= 3 || charaId >= 0x21) {
        return 0;
    }
    dataUnitOrgGet(charaId)->accessoryId[slot] = (short) accessoryId;
    return 1;
}

int calcAttEquipOrg(int charaId, int slot, int attachmentId) {
    if (slot >= 3 || charaId >= 0x21) {
        return 0;
    }
    dataUnitOrgGet(charaId)->attachmentId[slot] = (short) attachmentId;
    return 1;
}

int calcSpecEquipOrg(int charaId, int slot, int specialId) {
    if (slot >= 6 || charaId >= 0x21) {
        return 0;
    }
    dataUnitOrgGet(charaId)->specialId[slot] = (short) specialId;
    return 1;
}

int calcEtherEquipOrg(int charaId, int slot, int etherId) {
    if (slot >= 0xC || charaId >= 0x21) {
        return 0;
    }
    dataUnitOrgGet(charaId)->etherId[slot] = (short) etherId;
    return 1;
}

int calcSkillEquipOrg(int charaId, int slot, int skillId) {
    if (slot >= 3 || charaId >= 0x21) {
        return 0;
    }
    dataUnitOrgGet(charaId)->skillId[slot] = (short) skillId;
    return 1;
}

int calcAgwsEquipOrg(int charaId, int agwsId) {
    if ((unsigned int) (agwsId - 0x11) >= 0x10 || charaId >= 0x11) {
        return 0;
    }
    dataUnitOrgGet(charaId)->agwsId = (short) agwsId;
    return 1;
}

int calcEngineEquipOrg(int charaId, int engineId)
{
    CalcCharParaData *origin = dataUnitOrgGet(charaId);

    if ((origin->flags & 0x40) == 0) {
        return 0;
    }
    origin->engineId = (short) engineId;
    return 1;
}

int calcFrameEquipOrg(int charaId, int frameId)
{
    CalcCharParaData *origin = dataUnitOrgGet(charaId);

    if ((origin->flags & 0x40) == 0) {
        return 0;
    }
    origin->frameId = (short) frameId;
    return 1;
}

extern int calcSameWpnChk(ObjectTask *unit);

int calcDblActChk(ObjectTask *unit)
{
    CalcUnitParam *up;
    int idx;

    up = calcUPGet(unit);
    idx = calcSameWpnChk(unit);
    if (idx != -1 && (signed char)up->unmodeled_3a[0] == 6
        && *(short *)((char *)dataWpnGet(up->sefSetupParams[idx]) + 0x18) != 0) {
        if (calcAttWpnChk(up->sefSetupParams[idx]) != 0) {
            if ((short)up->statActiveMask[idx - 30] <= 0) {
                return 0;
            }
        }
        return 1;
    }
    return 0;
}

int calcSameWpnChk(ObjectTask *unit)
{
    CalcUnitParam *up;

    up = calcUPGet(unit);
    if (up->sefSetupParams[0] != 0) {
        if (up->sefSetupParams[0] == up->sefSetupParams[1]) return 0;
        if (up->sefSetupParams[0] == up->sefSetupParams[2]) return 0;
        if (up->sefSetupParams[0] == 0x5B || up->sefSetupParams[0] == 0x70) {
            if (up->sefSetupParams[1] == 0x5B || up->sefSetupParams[1] == 0x70) return 0;
        }
        if (up->sefSetupParams[0] == 0x5B || up->sefSetupParams[0] == 0x70) {
            if (up->sefSetupParams[2] == 0x5B || up->sefSetupParams[2] == 0x70) return 0;
        }
        if (up->sefSetupParams[0] == 0x5C || up->sefSetupParams[0] == 0x71) {
            if (up->sefSetupParams[1] == 0x5C || up->sefSetupParams[1] == 0x71) return 0;
        }
        if (up->sefSetupParams[0] == 0x5C || up->sefSetupParams[0] == 0x71) {
            if (up->sefSetupParams[2] == 0x5C || up->sefSetupParams[2] == 0x71) return 0;
        }
    }
    if (up->sefSetupParams[1] != 0) {
        if (up->sefSetupParams[1] == up->sefSetupParams[2]) return 1;
        if (up->sefSetupParams[1] == 0x5B || up->sefSetupParams[1] == 0x70) {
            if (up->sefSetupParams[2] == 0x5B || up->sefSetupParams[2] == 0x70) return 1;
        }
        if (up->sefSetupParams[1] == 0x5C || up->sefSetupParams[1] == 0x71) {
            if (up->sefSetupParams[2] == 0x5C || up->sefSetupParams[2] == 0x71) return 1;
        }
    }
    return -1;
}

extern int calcTgtUnitTbl(int, int, int);
extern int calcEtherDisableGet(ObjectTask *unit, int bit);

int calcEthEnableChk(ObjectTask *unit, int id)
{
    int i;
    int off;

    if (calcTgtUnitTbl((int)unit, *(unsigned char *)dataTecGet(dataEthGet(id)->tecId), 0) == 0) {
        return 0;
    }
    if ((short)calcUPGet(unit)->currentEp < calcTakeEpGet((CalcTakeEpArg *)unit, dataEthGet(id)->tecId)) {
        return 0;
    }
    if (id == 3 && (cfEventGet() & 8)) {
        return 0;
    }
    if ((calcStatGet(unit, 7, 2) && id == 0x2B)
        || (!calcStatGet(unit, 7, 2) && (id == 0x2C || id == 0x2D))
        || (calcStatGet(unit, 7, 1) && id == 0x2E)
        || (!calcStatGet(unit, 7, 1) && (id == 0x2F || id == 0x30))) {
        return 0;
    }
    i = 0;
    off = 0x80;
    do {
        if (*(short *)((char *)calcUPGet(unit) + off + 0xE) == id && calcEtherDisableGet(unit, i)) {
            return 0;
        }
        i++;
        off += 2;
    } while (i < 12);
    return 1;
}

extern int calcTgtUnitTbl(int, int, int);
extern int cfEventGet(void);

int calcItmEnableChk(int unit, int itm)
{
    if (calcTgtUnitTbl(unit, dataItmGet(itm)[8], 0) == 0) {
        return 0;
    }
    if (itm == 0x10 && (cfEventGet() & 8)) {
        return 0;
    }
    return 1;
}

int calcTrg2PE(ObjectTask *unit, int trgFlags);

int calcTgtUnitTbl(int unit, int flag, int out)
{
    ObjectTask **table;
    int count;
    int i;
    int n;

    n = 0;
    if (flag & 8) {
        if (!(flag & 4) || !(calcUPGet((ObjectTask *)unit)->flags & 0x40)) {
            if (out != 0) {
                *(int *)out = unit;
            }
            n = 1;
        }
    } else {
        i = 0;
        count = unitTblGet(calcTrg2PE((ObjectTask *)unit, flag), &table);
        if (count > 0) {
            do {
                if (table[i] != 0) {
                    if (!(flag & 0x20)) {
                        if (*(int *)table[i]->work & 0x80) {
                            goto next;
                        }
                    } else if (!(*(int *)table[i]->work & 0x80)) {
                        goto next;
                    }
                    if (!(flag & 4) || !(calcUPGet(table[i])->flags & 0x40)) {
                        if (out != 0) {
                            ((ObjectTask **)out)[n] = table[i];
                        }
                        n++;
                    }
                }
            next:
                i++;
            } while (i < count);
        }
    }
    return n;
}

int calcDeadUnitNum(ObjectTask *unit, int flag)
{
    ObjectTask **table;
    int count;
    int i;
    int n;

    count = unitTblGet(calcTrg2PE(unit, flag), &table);
    n = 0;
    for (i = 0; i < count; i++) {
        if (table[i] != 0) {
            if (*(int *)table[i]->work & 0x80) {
                n++;
            }
        }
    }
    return n;
}

int calcBattleChk(ObjectTask *unit)
{
    if (unit != 0 && !(*(u32 *)unit->work & 0x80)
        && calcStatGet(unit, 0, 0x2000) == 0 && calcStatGet(unit, 1, 0x2000) == 0
        && calcStatGet(unit, 4, 0x2000) == 0 && calcStatGet(unit, 5, 0x4000) == 0) {
        if (calcStatGet(unit, 5, 0x2000) == 0) {
            goto ok;
        }
    }
    return 0;
ok:
    return 1;
}

/*
 * Answers a target-selection flag word (cmdAtktbl, calcTgtUnitTbl and
 * calcDeadUnitNum supply it) against the unit's own side bit, bit 0x40 of
 * CalcActorRecord's flags: a selection of 0x40, and any word carrying 0x08,
 * asks whether the unit is on that side; a selection of 0x80 asks whether it
 * is not; every other selection answers 2.
 */
int calcTrg2PE(ObjectTask *unit, int trgFlags)
{
    if ((trgFlags & 0xC0) == 0x40 || (trgFlags & 8) != 0) {
        if ((((CalcActorRecord *)unit->work)->flags & 0x40) != 0) {
            return 1;
        }
        return 0;
    }
    if ((trgFlags & 0xC0) == 0x80) {
        if ((((CalcActorRecord *)unit->work)->flags & 0x40) == 0) {
            return 1;
        }
        return 0;
    }
    return 2;
}

/* calcStatIdx2Bit/calcStatReset: calcStatTurn's own callees, defined later
 * in this TU (calcStatIdx2Bit below) or still asm (calcStatReset). */
extern int calcStatIdx2Bit(int statIdx);
extern void calcStatReset(ObjectTask *unit, int category, int mask, int unused);

void calcStatTurn(ObjectTask *unit)
{
    CalcUnitParam *param;
    int category;
    int statIdx;
    unsigned char remaining;

    param = calcUPGet(unit);
    for (category = 0; category < 8; category++) {
        for (statIdx = 0; statIdx < 16; statIdx++) {
            if (param->statActiveMask[category] & calcStatIdx2Bit(statIdx)) {
                remaining = param->statTurnCount[category][statIdx];
                if (remaining != 0xFF) {
                    remaining--;
                    if (remaining == 0) {
                        calcStatReset(unit, category, calcStatIdx2Bit(statIdx), 0);
                    }
                    param->statTurnCount[category][statIdx] = remaining;
                }
            }
        }
    }
}

int calcStatProtectGet(ObjectTask *unit, int group, int mask)
{
    return calcUPGet(unit)->statProtect[group] & mask;
}

void calcStatProtectSet(ObjectTask *unit, int group, int mask)
{
    calcUPGet(unit)->statProtect[group] |= mask;
}

int calcStatGet(ObjectTask *unit, int category, int flag) {
    int res;

    if ((category == 3 && (flag & 1)) || (category == 6 && (flag & 0x800))) {
        ObjectTask **table;
        int count = unitTblGet(0, &table);
        int i;

        res = 0;
        for (i = 0; i < count; i++) {
            ObjectTask *u = table[i];

            if (u == 0) {
                continue;
            }
            res = calcUPGet(u)->statActiveMask[category] & flag;
            if (res != 0) {
                break;
            }
        }
    } else {
        CalcUnitParam *up;

        res = calcUPGet(unit)->statActiveMask[category] & flag;
        up = calcUPGet(unit);
        res |= *(u16 *)(up->unmodeled_bc + category * 2) & flag;
    }
    return res;
}

int calcStatGetNorm(ObjectTask *unit, int category, int flag) {
    int res;

    if ((category == 3 && (flag & 1)) || (category == 6 && (flag & 0x800))) {
        ObjectTask **table;
        int count = unitTblGet(0, &table);
        int i;

        res = 0;
        for (i = 0; i < count; i++) {
            ObjectTask *u = table[i];

            if (u == 0) {
                continue;
            }
            res = calcUPGet(u)->statActiveMask[category] & flag;
            if (res != 0) {
                break;
            }
        }
    } else {
        res = calcUPGet(unit)->statActiveMask[category] & flag;
    }
    return res;
}

int calcStatSetChk(ObjectTask *unit, unsigned int group, int mask, int force)
{
    unsigned short flags;

    flags = calcUPGet(unit)->flags;
    if (flags & 0x1000) {
        if (group < 4) {
            return 0;
        }
        if (!(flags & 8) && group == 5) {
            return 0;
        }
    } else if (group - 4 < 3) {
        return 0;
    }
    if (force != 0) {
        return 1;
    }
    if (calcStatProtectGet(unit, group, mask) != 0) {
        return 0;
    }
    if (calcStatGet(unit, 2, 0x2000) != 0 && group != 7) {
        return 0;
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/calc", calcStatSet);

void calcStatReset(ObjectTask *unit, int category, int mask, int unused)
{
    CalcUnitParam *up = calcUPGet(unit);

    if (unused == 0) {
        up->statActiveMask[category] &= ~mask;
        up->statTurnCount[category][calcStatBit2Idx(mask)] = 0;
    } else {
        *(u16 *)(up->unmodeled_bc + category * 2) &= ~mask;
    }
}

extern signed char turn_1[8][16];

int calcStatDefTurnGet(ObjectTask *unit, int category, int mask)
{
    int turn;
    int idx;

    idx = calcStatBit2Idx(mask);
    turn = turn_1[category][idx];
    if (category == 7 && (mask & 3)) {
        turn += (short)calcUPGet(unit)->currentHp < *(short *)calcUPGet(unit) / 2;
        turn += (short)calcUPGet(unit)->currentHp < *(short *)calcUPGet(unit) / 4;
    }
    return turn;
}

/*
 * The inverse of calcStatIdx2Bit below: the index of statBit's lowest set bit
 * among the 16 status bits calcStatSet, calcStatReset and calcStatDefTurnGet
 * index with it, or 16 when statBit holds none of them.
 */
int calcStatBit2Idx(int statBit)
{
    int statIdx;
    int mask;

    mask = 1;
    for (statIdx = 0; statIdx < 16; statIdx++) {
        if (statBit & mask) {
            break;
        }
        mask <<= 1;
    }
    return statIdx;
}

int calcStatIdx2Bit(int statIdx)
{
    return 1 << statIdx;
}

void calcStatReset(ObjectTask *unit, int category, int mask, int unused);

void calcStatResetCha(ObjectTask *unit) {
    calcStatReset(unit, 0, 0x7FFF, 0);
    calcStatReset(unit, 1, 0x7FFF, 0);
    calcStatReset(unit, 2, 0x7FFF, 0);
    calcStatReset(unit, 3, 0x7FFF, 0);
}

void calcStatResetAll(ObjectTask *unit) {
    calcStatReset(unit, 0, 0x7FFF, 0);
    calcStatReset(unit, 1, 0x7FFF, 0);
    calcStatReset(unit, 2, 0x7FFF, 0);
    calcStatReset(unit, 3, 0x7FFF, 0);
    calcStatReset(unit, 4, 0x7FFF, 0);
    calcStatReset(unit, 5, 0x7FFF, 0);
    calcStatReset(unit, 6, 0x7FFF, 0);
}

INCLUDE_ASM("asm/nonmatchings/ov01/calc", calcResult);

typedef struct CalcResPl {
    int exp;        /* +0x00 */
    int pad4[2];
    int ep;         /* +0x0C */
    int sp;         /* +0x10 */
    int tp;         /* +0x14 */
} CalcResPl;

void calcResultSub(ObjectTask *unit, int *gain, int div, int *out)
{
    CalcResPl *pl;
    int v;
    int pct;

    pl = (CalcResPl *)dataPlChaGet(calcUPGet(unit)->charaId);

    v = gain[0] / div;
    pct = calcSkill(unit, 0x1000, 0x4000);
    if (pct > 0) {
        v += v * pct / 100;
    }
    pl->exp += v;
    if (pl->exp > 9999999) {
        pl->exp = 9999999;
    }
    out[1] = v;

    v = gain[1] / div;
    pct = calcSkill(unit, 0x1000, 0x2000);
    if (pct > 0) {
        v += v * pct / 100;
    }
    pl->ep += v;
    if (pl->ep > 9999) {
        pl->ep = 9999;
    }
    out[2] = v;

    v = gain[2] / div;
    pct = calcSkill(unit, 0x1000, 0x1000);
    if (pct > 0) {
        v += v * pct / 100;
    }
    pl->sp += v;
    if (pl->sp > 9999) {
        pl->sp = 9999;
    }
    out[3] = v;

    v = gain[3] / div;
    pct = calcSkill(unit, 0x1000, 0x800);
    if (pct > 0) {
        v += v * pct / 100;
    }
    pl->tp += v;
    if (pl->tp > 9999) {
        pl->tp = 9999;
    }
    out[4] = v;
}

INCLUDE_ASM("asm/nonmatchings/ov01/calc", calcLvUp);

/*
 * entryPhase40 (src/ov01/entry_first_init.c) calls this as a bare statement
 * and never reads a result, but the original ends with a real jal to
 * calcPartyStat plus its own separate epilogue rather than a tail jump into
 * it, which this int return type (left unset) reproduces.
 */
int calcCfEncount(int flags)
{
    if (flags & 1)
    {
        calcPartyStat(0, 0);
        menuTimeSet(1);
    }
    if (flags & 2)
    {
        calcPartyStat(0, 0);
        calcPartyStat(0, 1);
    }
}

void calcPartyStat(int type, int mode)
{
    ObjectTask **table;
    ObjectTask *u;
    int count;
    int i = 0;

    count = unitTblGet(type, &table);
    if (count > 0) {
        do {
            u = table[i];
            i++;
            if (u != 0) {
                switch (mode) {
                case 0:
                    calcUPGet(u)->waitCount = 0;
                    break;
                case 1:
                    calcUPGet(u)->unmodeled_3a[2]++;
                    break;
                }
            }
        } while (i < count);
    }
}

int calcEtherDisableGet(ObjectTask *unit, int bit)
{
    u16 mask;

    mask = 1 << bit;
    return calcUPGet(unit)->etherDisableFlags & mask;
}

void calcEtherDisableSet(ObjectTask *unit, int bit)
{
    CalcUnitParam *param;
    u16 mask;

    mask = 1 << bit;
    param = calcUPGet(unit);
    param->etherDisableFlags |= mask;
}

int rnd(int max)
{
    return xglSRand() % (max + 1);
}

void calcSplineInit(CalcSpline *spline, int start, int end, int duration)
{
    spline->start = start;
    spline->end = end;
    spline->duration = (float) duration;
    spline->midpoint = 0.5f;
    spline->frame = 0;
}

INCLUDE_ASM("asm/nonmatchings/ov01/calc", calcSpline);

typedef struct CalcLinerVec {
    float x, y, z, w;
} CalcLinerVec;

void calcLinerSub(float t, CalcLinerVec **list, float *out, int idx)
{
    CalcLinerVec s[2];

    s[0] = (*list)[idx];
    s[1] = (*list)[idx + 1];
    out[0] = s[0].x + (s[1].x - s[0].x) * t;
    out[1] = s[0].y + (s[1].y - s[0].y) * t;
    out[2] = s[0].z + (s[1].z - s[0].z) * t;
}

void calcBezierSub(float t, CalcLinerVec **list, float *out, int idx)
{
    CalcLinerVec s[4];
    float u, uu, t3;
    float a, b, c, d;

    s[0] = (*list)[idx];
    s[1] = (*list)[idx + 1];
    s[2] = (*list)[idx + 2];
    s[3] = (*list)[idx + 3];
    u = 1.0f - t;
    t3 = t * 3.0f;
    uu = u * u;
    a = uu * u;
    b = t3 * uu;
    c = t3 * t * u;
    d = t * t * t;
    out[0] = a * s[0].x + b * s[1].x + c * s[2].x + d * s[3].x;
    out[1] = a * s[0].y + b * s[1].y + c * s[2].y + d * s[3].y;
    out[2] = a * s[0].z + b * s[1].z + c * s[2].z + d * s[3].z;
}

INCLUDE_ASM("asm/nonmatchings/ov01/calc", calcSplineSub);

typedef struct CalcHermiteList {
    CalcLinerVec *pts;
    int count;
    int pad8;
    float tension;
} CalcHermiteList;

void calcHermiteSub(float t, CalcHermiteList *list, float *out, int idx)
{
    CalcLinerVec s[4];
    float tt, ttt;
    float a, b, c, d;
    CalcLinerVec *v;
    int i, prev, next;

    tt = t * t;
    ttt = tt * t;
    s[0] = list->pts[idx];
    s[1] = list->pts[idx + 1];
    v = &s[2];
    d = ttt - tt;
    b = ttt * -2.0f + tt * 3.0f;
    c = (ttt - 2.0f * tt) + t;
    a = (2.0f * ttt - tt * 3.0f) + 1.0f;
    for (i = 1; i >= 0; i--) {
        if (idx == 0) {
            prev = 0;
            next = 1;
        } else if (idx >= list->count - 1) {
            prev = idx - 1;
            next = idx;
        } else {
            prev = idx - 1;
            next = idx + 1;
        }
        v->x = list->tension * (list->pts[next].x - list->pts[prev].x);
        v->y = list->tension * (list->pts[next].y - list->pts[prev].y);
        v->z = list->tension * (list->pts[next].z - list->pts[prev].z);
        idx++;
        v = &s[3];
    }
    out[0] = a * s[0].x + b * s[1].x + c * s[2].x + d * s[3].x;
    out[1] = a * s[0].y + b * s[1].y + c * s[2].y + d * s[3].y;
    out[2] = a * s[0].z + b * s[1].z + c * s[2].z + d * s[3].z;
}

void calcNormal(float *v)
{
    float r;

    r = 1.0f / __builtin_sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (r == 0.0f) {
        v[0] = 0.0f;
        v[1] = 0.0f;
        v[2] = 0.0f;
    } else {
        v[0] = v[0] * r;
        v[1] = v[1] * r;
        v[2] = v[2] * r;
    }
}

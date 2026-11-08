/*
 * OV01 original TU 2: 0x00a00d60..0x00a07770 (119 functions)
 */
#include "common.h"
#include "shared.h"
#include "unit_cmd.h"

/* Types recovered upstream (origin/master); kept for reference. */
/* The same eight unit pointers have full-record and scheduler-prefix views. */
typedef union UnitTableStorage {
    UnitRecord *records[8];
    ObjectTask *tasks[8];
} UnitTableStorage;


static unsigned char unitTbl[0x20];
static int unitPlNum;
static int unitEnNum;
/* spline work and its control points (no ELF symbols) */
static unsigned char D_00A57268[0x18];
static float D_00A57280[0x20];

void unitInit(void)
{
    memset(unitTbl, 0, 0x20);
    unitPlNum = 0;
    unitEnNum = 0;
}

int unitTblGet(int side, UnitRecord ***table)
{
    if (side == 0) {
        *table = (void *)unitTbl;
        return 3;
    }
    if (side == 1) {
        *table = (void *)&unitTbl[12];
        return 5;
    }
    *table = (void *)unitTbl;
    return 8;
}

int unitTblSet(int side, ObjectTask *unit)
{
    int i;
    int start;
    int end;

    if (side == 0) {
        start = 0;
        end = 3;
    } else if (side == 1) {
        end = 8;
        start = 3;
    } else {
        return 0;
    }
    for (i = start; i < end; i++) {
        if (((ObjectTask **)unitTbl)[i] == 0) {
            ((ObjectTask **)unitTbl)[i] = unit;
            if (i < 3) {
                unitPlNum++;
            } else {
                unitEnNum++;
            }
            return 1;
        }
    }
    return 0;
}

int unitTblRemove(ObjectTask *unit)
{
    ObjectTask **units = (void *)unitTbl;
    int i;

    for (i = 0; i < 8; i++) {
        if (units[i] == unit) {
            units[i] = 0;
            if (i < 3) {
                unitPlNum--;
            } else {
                unitEnNum--;
            }
            return 1;
        }
    }
    return 0;
}

int unitTblChg(ObjectTask *oldUnit, ObjectTask *newUnit)
{
    ObjectTask **units = (void *)unitTbl;
    int i;

    for (i = 0; i < 8; i++) {
        if (units[i] == oldUnit) {
            units[i] = newUnit;
            return 1;
        }
    }
    return 0;
}

int unitNumGet(int side)
{
    int count;

    if (side == 0) {
        count = unitPlNum;
    } else if (side == 1) {
        count = unitEnNum;
    } else {
        count = unitPlNum + unitEnNum;
    }
    return count;
}

int unitLiveNumGet(int side)
{
    UnitRecord **table;
    int count;
    int remaining;
    int n;
    UnitRecord **cur;
    UnitRecord *unit;

    count = unitTblGet(side, &table);
    n = 0;
    if (count > 0) {
        cur = table;
        remaining = count;
        do {
            unit = *cur;
            cur++;
            if (unit != 0) {
                if (!(**(int **)((char *)unit + 0x10) & 0x80)) {
                    n++;
                }
            }
        } while (--remaining != 0);
    }
    return n;
}

int unitLiveNumGet2(int side)
{
    ObjectTask **table;
    int count;
    int i = 0;
    int n = 0;

    count = unitTblGet(side, (UnitRecord ***)&table);
    if (count > 0) {
        do {
            int off = i * 4;
            ObjectTask *u = *(ObjectTask **)(off + (int)table);
            i++;
            if (u != 0) {
                if (!(**(int **)((char *)u + 0x10) & 0x80)) {
                    if (calcStatGet(u, 0, 0x2000) == 0) {
                        if (calcStatGet(*(ObjectTask **)(off + (int)table), 4, 0x2000) == 0)
                            n++;
                    }
                }
            }
        } while (i < count);
    }
    return n;
}

int unitCidGet(ObjectTask *unit)
{
    int cid;

    cid = dataCidGet(calcUPGet(unit)->charaId);
    if (calcStatGet(unit, 7, 2) != 0) {
        cid = 0x14;
    } else if (calcStatGet(unit, 7, 1) != 0) {
        cid = 0x15;
    }
    return cid;
}

extern int unitAgwsPilotGet(int charaId);
extern int printf(const char *format, ...);
void unitPlFunc(ObjectTask *unit);

/*
 * unitCreate/unitTblSet/unitLoad are unitPlCreate's own siblings; unitCreate
 * and unitTblSet are still asm in this TU (also forward-declared below for
 * unitEnCreate's own use) and unitLoad is defined later in this file.
 */
extern ObjectTask *unitCreate(s16 *pl, int side, ObjectTaskCallback func);
extern int unitTblSet(int side, ObjectTask *unit);
void unitLoad(ObjectTask *unit, int mode);

ObjectTask *unitPlCreate(s16 *pl)
{
    ObjectTask *unit;
    ObjectTask *pilotUnit;
    int pilotId;
    s16 pilotPl[2];

    unit = unitCreate(pl, 0, unitPlFunc);
    if (unit != 0) {
        unitTblSet(0, unit);
        unitLoad(unit, pl[0]);
    }
    if (pl[0] >= 0x11) {
        /* AGWS characters (charaId >= 0x11) also get a linked pilot unit,
         * whose own CalcUnitParam is chained through linkedUnit. */
        pilotId = unitAgwsPilotGet(pl[0]);
        if (pilotId != 0) {
            pilotPl[0] = (s16) pilotId;
            pilotPl[1] = 1;
            pilotUnit = unitCreate(pilotPl, 0, unitPlFunc);
            calcUPGet(unit)->linkedUnit = calcUPGet(pilotUnit);
        }
        printf("unitPlCreate: AGWS (pilot %d)\n", pilotId);
    }
    return unit;
}

/*
 * unitCreate/unitTblSet are unitEnCreate's own siblings, still asm in this
 * TU; forward-declared for the calls it makes before their definitions
 * appear later in the file.
 */
extern ObjectTask *unitCreate(s16 *pl, int side, ObjectTaskCallback func);
extern int unitTblSet(int side, ObjectTask *unit);
void unitLoad(ObjectTask *unit, int mode);
void unitEnFunc(ObjectTask *unit);

ObjectTask *unitEnCreate(s16 *pl)
{
    ObjectTask *unit;

    unit = unitCreate(pl, 1, unitEnFunc);
    if (unit != 0) {
        unitTblSet(1, unit);
        unitLoad(unit, *pl);
    }
    return unit;
}

typedef struct {
    unsigned char unmodeled_00[0x54];
    short agwsId; /* +0x54 */
} UnitAgwsOrg;
extern UnitAgwsOrg *dataUnitOrgGet(int charaId);

int unitAgwsPilotGet(int charaId)
{
    int i;
    int ok;

    ok = (unsigned)(charaId - 0x11) < 0x10;
    i = 1;
    if (!ok) {
        return 0;
    }
    do {
        if (dataUnitOrgGet(i)->agwsId == charaId) {
            return i;
        }
        i++;
        ok = i < 0x11;
    } while (ok);
    return 0;
}

typedef union {
    float f;
    int i;
} UnitFW;
void unitActupdate(Actor *actor);
void unitActdraw(Actor *actor);
void unitParaInit(ObjectTask *unit, int charaId, int flag);
void unitCmdClear(ObjectTask *unit);

ObjectTask *unitCreate(s16 *pl, int side, ObjectTaskCallback func)
{
    ObjectTask *unit;
    char *up;
    char *act;
    int charaId;
    int pos;
    int ang;
    int i;
    double zero = 0.0;

    charaId = pl[0];
    unit = (ObjectTask *)objEntry(func);
    if (unit == 0) {
        return 0;
    }
    objStdInit(unit);
    unitParaInit(unit, charaId, side);
    up = (char *)calcUPGet(unit);
    pos = pl[1];
    ang = 270;
    if (side != 0) {
        ang = 90;
    }
    if (side != 0) {
        pos = -pos;
    }
    *(s16 *)(up + 0x44) = pos;
    *(float *)(up + 0x4C) = ((float)ang / 180.0f) * 3.1415927f;
    if (side == 0) {
        *(void **)(up + 0x48) = 0;
    } else {
        *(void **)(up + 0x48) = pl;
        *(int *)*(int *)((char *)unit + 0x10) |= 0x40;
        *(int *)*(int *)((char *)unit + 0x10) |= 0x20;
    }
    unitCmdClear(unit);
    act = (char *)ACT_create(-1, unitCidGet(unit));
    if (act == 0) {
        return 0;
    }
    ACT_initMotion(act);
    *(char **)((char *)unit + 0x14) = act;
    *(float *)(act + 0x10) = dataPosTblGet(pl[1])->x;
    *(float *)(act + 0x14) = dataPosTblGet(pl[1])->y;
    *(float *)(act + 0x18) = dataPosTblGet(pl[1])->z;
    *(float *)(act + 0x58) = zero;
    *(float *)(act + 0x50) = zero;
    *(float *)(act + 0x54) = *(float *)(up + 0x4C);
    if (pos < 0) {
        *(float *)(act + 0x10) = -*(float *)(act + 0x10);
    }
    if (charaId != 0xB0) {
        *(u8 *)(act + 0x90) = 9;
        *(int *)(act + 0x0) |= 0x20;
        *(u8 *)(act + 0x91) = 0x3C;
        *(int *)(act + 0x9A0) |= 0x40;
    }
    if (charaId == 0xAB) {
        *(int *)(act + 0x9A4) |= 0x80;
        *(float *)(act + 0x9D0) = 10.0f;
    }
    *(int *)(act + 0x0) |= 0x300;
    *(int *)(act + 0x9A4) |= 0x200;
    ((Actor *)*(int *)((char *)unit + 0x14))->flags |= 8;
    ((UnitFW *)(act + 0xC0))->f = zero;
    ((UnitFW *)(act + 0xD0))->f = 0.1f;
    *(int *)(act + 0xE0) = 0;
    *(int *)(act + 0xF0) = 0;
    i = 0;
loop:
    ((int *)(act + 0x100))[i] = 0;
    i++;
    if (i < 5) goto loop;
    *(void **)(act + 0x4) = unitActupdate;
    *(void **)(act + 0x8) = unitActdraw;
    *(int *)((char *)unit + 0x2C) = 0;
    for (i = 0; i < 3; i++) {
        *(int *)((char *)unit + 0x1C + (i & 3) * 4) = 0;
    }
    printf("** Create (ID=%d) U=%X A=%X P=%X O=%X **\n", charaId, unit, act, calcUPGet(unit), *(void **)((char *)unit + 0x10));
    return unit;
}

extern void ACT_dispose(void *self);

void unitRemoveActor(UnitRecord *unit)
{
    Actor *motion;
    Actor *a;
    int i;

    a = *(Actor **)((char *)unit + 0x2C);
    motion = unit->motionActor;
    if (a != 0) {
        ACT_dispose(a);
    }
    for (i = 0; i < 3; i++) {
        a = *(Actor **)(((i & 3) << 2) + (int)unit + 0x1c);
        if (a != 0) {
            ACT_dispose(a);
        }
    }
    ACT_dispose(motion);
}

extern void objRemove(ObjectTask *task);
extern void sefReleaseID(int id);
extern int sndSeRegRemove(int id);
extern void sefDeleteEffect2(void *p);
extern void sefDeleteEffectWait(int id);
typedef struct {
    void *p[8];
} UnitRemoveEff;

void unitRemove(UnitRecord *unit, int mode)
{
    Actor *actor;
    short cid;
    int flag;
    void *eff;
    UnitRemoveEff effs;
    UnitRecord **table;
    UnitRecord *u;
    int count;
    int i;
    int n;

    actor = unit->motionActor;
    printf("** Remove (%d) U=%X A=%X P=%X O=%X **\n", calcUPGet(&unit->task)->charaId, unit, actor, calcUPGet(&unit->task), unit->task.work);
    unit->motionActor->flags |= 8;
    cid = calcUPGet(&unit->task)->charaId;
    flag = *(unsigned int *)unit->task.work & 0x40;
    eff = *(void **)((char *)calcUPGet(&unit->task) + 0x158);
    effs = *(UnitRemoveEff *)calcUPGet(&unit->task)->statEffUnits;
    if ((*(unsigned int *)unit->task.work & 0x40) ||
        (calcUPGet(&unit->task)->charaId >= 0xBB && calcUPGet(&unit->task)->charaId < 0xC3)) {
        calcUPGet(&unit->task)->charaId = 0;
    }
    unitRemoveActor(unit);
    objRemove(&unit->task);
    if (mode == 1) {
        return;
    }
    n = 0;
    if (flag) {
        count = unitTblGet(2, &table);
        for (i = 0; i < count; i++) {
            u = table[i];
            if (u != 0 && !(*(unsigned int *)u->task.work & 0x80)) {
                if (calcUPGet(&u->task)->charaId == cid) {
                    n++;
                }
            }
        }
        if (n == 0) {
            sefReleaseID(cid);
            sndSeRegRemove(cid);
            if (eff != 0) {
                sefDeleteEffect2(eff);
            }
        }
        for (i = 0; i < 8; i++) {
            if (effs.p[i] != 0) {
                sefDeleteEffect2(effs.p[i]);
            }
        }
    } else {
        sefReleaseID(cid);
        sndSeRegRemove(cid);
        if (eff != 0) {
            sefDeleteEffectWait(cid);
        }
    }
}

void unitLoad(ObjectTask *unit, int mode)
{
    CalcUnitParam *up;

    up = calcUPGet(unit);
    if (calcUPGet(unit)->charaId < 0xBB || calcUPGet(unit)->charaId >= 0xC3) {
        if (!(((Actor *)unit->work)->flags & ACTOR_FLAG_ENEMY)) {
            sefSetupPlayer(up->charaId, up->sefSetupParams[0],
                           up->sefSetupParams[1], up->sefSetupParams[2]);
        } else {
            sefSetupEnemy(up->charaId);
        }
    }
    dataUnitFileLoad(unit, mode);
    dataSndSeRegLoad(unit);
}

void unitActupdate(Actor *actor)
{
    if (actor->flags & 0x8) { /* motion update does not run while set. */
        return;
    }
    if (actor->flags & ACTOR_FLAG_ENEMY) {
        actor->position.y = UnduGet(actor->position.x, actor->position.z);
    }
    actor->motionFrame += 1;
    ACT_updateMotion(actor);
}

void unitActdraw(Actor *actor)
{
    float transparency;

    transparency = actor->transparency;
    if (transparency < 1.0f) {
        nmlModelSetTransparency(transparency);
        nmlModelSetToumei(1);
        nmlModelSetZwrite(1);
    }
}

extern void dataUnitInitSet(ObjectTask *unit, int charaId);
extern int calcWct(ObjectTask *unit);
extern int calcAttWpnChk(int weaponId);
extern unsigned char *dataWpnGet(int index);

void unitParaInit(ObjectTask *unit, int charaId, int flag)
{
    CalcUnitParam *up;
    short *dst;
    short *wp;
    int wct;
    int i;

    i = 2;
    dataUnitInitSet(unit, charaId);
    up = calcUPGet(unit);
    up->charaId = charaId;
    dst = (short *)&up->unmodeled_6a[6];
    wp = up->sefSetupParams;
    wct = calcWct(unit);
    up->unmodeled_3a[0] = 0;
    up->waitCount = wct;
    up->waitCountBase = wct;
    up->turnBias = 0;
    do {
        if (*wp != 0 && dst[-3] != 0 && calcAttWpnChk(*wp) != 0) {
            *dst = dataWpnGet(*wp)[0x10];
        }
        i--;
        dst++;
        wp++;
    } while (i >= 0);
    up->unmodeled_3a[3] = 0;
    up->unmodeled_3a[1] = 3;
    if (flag == 0) {
        if (calcStatGet(unit, 7, 0x1000) == 0) {
            up->unmodeled_3a[2] = 0;
            return;
        }
        up->unmodeled_3a[2] = 1;
    } else {
        up->unmodeled_3a[2] = up->unmodeled_1e[3];
    }
}

/*
 * plTbl's stride (8 bytes per entry, three entries) and its leading short are
 * the only bytes unitPlInit demonstrates; unitPlCreate is called with the
 * entry pointer itself whenever that leading short is nonzero.
 */
void unitPlInit(short *plTbl)
{
    int count;

    count = 2;
    do {
        if (*plTbl != 0) {
            unitPlCreate(plTbl);
        }
        count -= 1;
        plTbl += 4;
    } while (count >= 0);
}


void unitEnInit(s16 *tbl)
{
    int i;

    for (i = 0; *tbl != 0; tbl += 4) {
        if (i >= 5) {
            printf("** unitEnInit: EN over\n");
            break;
        }
        i++;
        unitEnCreate(tbl);
    }
}

void unitClipSet(ObjectTask *unit)
{
    char *w = *(char **)((char *)unit + 0x14);

    if (calcUPGet(unit)->charaId == 0xB4) {
        *(int *)(w + 0x9A0) |= 0x200;
    } else if (**(int **)((char *)unit + 0x10) & 8) {
        *(int *)(w + 0x9A0) |= 0x200;
    } else {
        *(int *)(w + 0x9A0) &= ~0x200;
    }
}

void unitPlFunc(ObjectTask *unit)
{
    unitCmdExec(unit);
    statEffProc(unit);
    unitClipSet(unit);
}

void unitEnFunc(ObjectTask *unit)
{
    unitCmdExec(unit);
    statEffProc(unit);
    unitClipSet(unit);
}

/*
 * Defined in a different translation unit (src/ov01/obj.c, ov01/tu001), its
 * published exact_c; unitCmdClear forwards its own unit argument straight
 * through to it.
 */
extern void objCmdClear(ObjectTask *unit);

/*
 * unitCmdClear also nulls out its unit's Actor.update callback (declared
 * above), alongside clearing the shared object command queue.
 */
void unitCmdClear(ObjectTask *unit)
{
    Actor *actor;

    objCmdClear(unit);
    actor = unit->work;
    actor->update = 0;
}

void *unitCmdTailGet(ObjectTask *unit)
{
    return objCmdTailGet(unit);
}

void *unitCmdPtrGet(ObjectTask *unit)
{
    return objCmdPtrGet(unit);
}

int unitCmdNext(ObjectTask *unit)
{
    return objCmdNext(unit);
}

typedef struct { int w[5]; } UnitCmd;
extern void *objCmdPush(ObjectTask *task);

void unitCmdSet(ObjectTask *unit, int *cmd)
{
    int *work = (int *)unit->work;
    UnitCmd *p = objCmdPush(unit);

    work[0x70 / 4] = 0;
    work[0x74 / 4] = 0;
    *p = *(UnitCmd *)cmd;
    if (p->w[0] != 0) {
        work[0] |= 2;
    }
}

extern void debugCmdPrint(ObjectTask *, int);
extern int (*unitCmdTbl[])(ObjectTask *, int);

void unitCmdExec(ObjectTask *unit)
{
    int *work = (int *)unit->work;
    int *p = (int *)unitCmdPtrGet(unit);

    if (work[0x70 / 4] == 0 && *p != 0 && *p != 7) {
        debugCmdPrint(unit, *p);
    }
    if (unitCmdTbl[*p](unit, *p) == 0) {
        work[0x70 / 4]++;
    }
}

int unitCmdFree(ObjectTask *unit)
{
    Actor *actor;

    actor = unit->work;
    actor->flags &= ~ACTOR_FLAG_CMD_PENDING;
    return 0;
}

typedef struct { int a; ObjectTask *b; int c, d; } AtkCmd;
void unitCmdAtkSub(ObjectTask *unit, int *cmd);
extern int calcTakeEpGet(ObjectTask *unit, int tec);
extern void calcEp(ObjectTask *unit, int ep);
extern void calcBp2Tid(ObjectTask *unit, int tec);
extern void *dataSpecDataGet(int cid, int id);

int unitCmdAtk(ObjectTask *unit)
{
    AtkCmd buf;
    Actor *actor = unit->work;
    AtkCmd *cmd = unitCmdPtrGet(unit);
    unsigned short *spec;
    int v;
    int t;

    if (actor->moveState == 0) {
        v = cmd->b;
        t = *(int *)&((Actor *)unit->work)->targetUnit;
        buf.a = v;
        buf.b = (ObjectTask *)t;
        v = cmd->d;
        buf.c = v;
        buf.d = 1;
        unitCmdAtkSub(unit, (int *)&buf);
        calcEp(unit, -calcTakeEpGet(unit, buf.a));
        calcBp2Tid(unit, buf.a);
        spec = dataSpecDataGet(calcUPGet(unit)->charaId, buf.a);
        if (spec != 0) {
            spec[4] = spec[4] + 1;
        }
        actor->moveState++;
    }
    return 0;
}

extern void scenarioBatInit(void);
extern int unitJobTypeAtk(ObjectTask *unit, int *cmd, char *tec, int type);

void unitCmdAtkSub(ObjectTask *unit, int *cmd)
{
    char *tec;
    char type;

    scenarioBatInit();
    tec = (char *)dataTecGet(*cmd);
    type = tec[4];
    if (type < 8) {
        if (type >= 0) {
            unitJobTypeAtk(unit, cmd, tec, type);
            return;
        }
    }
    printf("** JobType err -> %d tid=%d\n", (char)(unsigned char)tec[4], *cmd);
    for (;;) {
        break;
    }
}

typedef struct JtaRec { int w[10]; } JtaRec;
typedef struct JtaHit { int hit; int dmg; int unk8; int unkC; int unk10; int unk14; int unk18; } JtaHit;
typedef struct JtaStep { ObjectTask *unit; int a; int b; int c; int d; } JtaStep;
extern int calcHitDmg(ObjectTask *attacker, int *arg, JtaHit *out);
extern int calcPlusAglGet(ObjectTask *unit, int id);
extern ObjectTask *tgtFindNext(void);
extern int scenarioDstSet(JtaRec *src);
extern int scenarioSrcSet(JtaStep *src);

int unitJobTypeAtk(ObjectTask *unit, int *cmd, char *tec, int type)
{
    JtaStep step;
    JtaRec rec;
    JtaHit out;
    CalcUnitParam *up;
    int var_19 = 0;
    int var_16;
    int var_10;
    int temp_5;
    int temp_4;
    int p;
    int t;
    ObjectTask *temp_2;

    t = tec[4];
    for (;;) {
        if (t != 7) {
            calcHitDmg(unit, cmd, &out);
            var_10 = out.hit;
            temp_5 = var_10 & 0xFF;
            temp_4 = (var_10 >> 8) & 0xFF;
            switch (temp_5) {
            case 0:
                var_16 = 0xA;
                break;
            case 1:
            case 3:
            case 7:
            case 8:
            case 9:
            case 10:
                var_16 = (temp_4 & 2) ? 0xB : 9;
                break;
            case 2:
            case 4:
                var_16 = (temp_4 & 8) ? 0x28 : 0x27;
                break;
            case 5:
            case 6:
                var_16 = 0x29;
                break;
            default:
                printf("** Hit Type err JTYPE_ATK -> %d\n", temp_5);
                return 0;
            }
        } else {
            var_16 = 0;
            memset(&out, 0, 0x1C);
            var_10 = 0xB;
            out.hit = 0xB;
        }
        var_19 += 1;
        rec.w[0] = (int)*(ObjectTask **)&cmd[1];
        rec.w[1] = var_16;
        rec.w[2] = out.dmg;
        rec.w[3] = var_10;
        rec.w[4] = out.unk8;
        rec.w[6] = out.unkC;
        rec.w[7] = out.unk10;
        rec.w[8] = out.unk14;
        rec.w[9] = out.unk18;
        scenarioDstSet(&rec);
        *(ObjectTask **)((char *)calcUPGet((ObjectTask *)cmd[1]) + 0x150) = unit;
        *(int *)((char *)calcUPGet((ObjectTask *)cmd[1]) + 0x154) = cmd[0];
        temp_2 = tgtFindNext();
        cmd[1] = (int)temp_2;
        if (temp_2 == 0) {
            step.unit = unit;
            step.a = cmd[3];
            step.b = cmd[0];
            step.c = cmd[2];
            step.d = var_19;
            scenarioSrcSet(&step);
            up = calcUPGet(unit);
            p = calcPlusAglGet(unit, cmd[0]);
            up->turnBias = (short)(up->turnBias + p);
            return 0;
        }
    }
}

extern void unitMotSet(ObjectTask *unit, int motion);
extern void unitWpnMotSet(ObjectTask *unit, int weaponIndex, int animGroup, int motion);
extern void unitFaceMotSet(ObjectTask *unit, int motion);
extern int unitMotGet(ObjectTask *unit);

void unitCmdAtkEnd(ObjectTask *unit)
{
    int cmd[5];

    unitCmdClear(unit);
    if (unitMotGet(unit) == 0x20) {
        cmd[0] = 0xD;
        cmd[1] = 0x21;
        cmd[2] = 0;
        unitCmdSet(unit, cmd);
        unitWpnMotSet(unit, dataDefWpnGet((UnitRecord *)unit), 0, 0x32);
        if (calcUPGet(unit)->charaId == 7) {
            unitWpnMotSet(unit, 1, 0, 0x32);
        }
    } else {
        unitMotSet(unit, 1);
    }
    unitFaceMotSet(unit, 0x35);
}

extern void unitCmdSet(ObjectTask *unit, int *cmd);

int unitCmdDeadSet(ObjectTask *unit)
{
    int cmd[5];

    if (unit == 0 || (((Actor *)unit->work)->flags & 0x80)) {
        return 0;
    }
    if ((s16)calcUPGet(unit)->currentHp != 0) {
        return 0;
    }
    cmd[0] = 0xC;
    unitCmdSet(unit, cmd);
    return 1;
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
struct CmdEntry {
    int selector;      /* +0x00 */
    int argument;      /* +0x04 */
    int extra;         /* +0x08 */
    unsigned char unmodeled_0c[8]; /* +0x0C: unwritten by any claimed call */
};

typedef struct UnitActRequest {
    int id;              /* +0x00: technique id */
    unsigned char unmodeled_04[0x0C];
} UnitActRequest;

typedef struct TecJobView {
    unsigned char unmodeled_00[4];
    signed char jobType; /* +0x04 */
} TecJobView;

typedef struct EtherReqC {
    int id;
    ObjectTask *target;
    int itmId;
    int mode;
} EtherReqC;
typedef struct EtherCmdC {
    int selector;
    int argument;
    int extra;
    int itmId;
} EtherCmdC;
typedef struct EtherDataC {
    unsigned char unmodeled_00[2];
    unsigned short flags;
} EtherDataC;
typedef struct EtherUPViewC {
    unsigned char unmodeled_00[0x8E];
    short etherId[12];
} EtherUPViewC;
extern EtherDataC *dataEthGet(int id);
extern void calcEtherDisableSet(ObjectTask *unit, int index);

int unitCmdEther(ObjectTask *unit)
{
    EtherReqC request;
    Actor *actor;
    EtherCmdC *cmd;
    int i;
    int value;
    ObjectTask *target;
    int itmId;

    actor = unit->work;
    cmd = (EtherCmdC *)unitCmdPtrGet(unit);
    if (calcStatGet(unit, 1, 0x1000) != 0) {
        return unitCmdNext(unit);
    }
    if (actor->moveState == 0) {
        target = (ObjectTask *)*(int *)&((Actor *)*(int *)&unit->work)->targetUnit;
        value = cmd->argument;
        request.id = value;
        request.target = target;
        itmId = cmd->itmId;
        request.mode = 2;
        request.itmId = itmId;
        unitCmdAtkSub(unit, (int *)&request);
        calcEp(unit, -calcTakeEpGet(unit, request.id));
        for (i = 0; i < 12; i++) {
            if (dataEthGet(request.itmId)->flags & 0x2000) {
                if (((EtherUPViewC *)calcUPGet(unit))->etherId[i] == request.itmId) {
                    calcEtherDisableSet(unit, i);
                    break;
                }
            }
        }
        actor->moveState++;
    }
    return 0;
}

typedef struct { int a, b, c, d; } ItemCmd;
extern void unitCmdItemSub(ObjectTask *unit, ItemCmd *buf);

int unitCmdItem(ObjectTask *unit)
{
    ItemCmd buf;
    Actor *actor = unit->work;
    ItemCmd *cmd = unitCmdPtrGet(unit);
    int v;

    if (actor->moveState == 0) {
        v = cmd->b;
        buf.a = v;
        buf.b = (int)((Actor *)unit->work)->targetUnit;
        v = cmd->d;
        buf.c = v;
        buf.d = 3;
        unitCmdItemSub(unit, &buf);
        actor->moveState++;
    }
    return 0;
}

extern int calcHitItem(ObjectTask *attacker, void *arg, void *out);
extern int calcDmgItem(ObjectTask *attacker, void *arg);

void unitCmdItemSub(ObjectTask *unit, ItemCmd *buf)
{
    int src[5];
    int dst[10];
    int out[7];
    int count;
    int hit;
    int var10;
    int var9;
    int t;
    int b2;
    int neg;
    ItemCmd *cb;
    int *po;

    count = 0;
    scenarioBatInit();
    for (;;) {
    hit = calcHitItem(unit, buf, out);
    t = hit & 0xFF;
    b2 = (hit >> 8) & 0xFF;
    switch (t) {
    case 1:
    case 3:
        var10 = 9;
        var9 = calcDmgItem(unit, buf);
        out[2] = 0;
        neg = -var9;
        var9 = (var9 < 0) ? neg : var9;
        break;
    case 2:
    case 4:
        var9 = calcDmgItem(unit, buf);
        var10 = (b2 & 8) ? 0x28 : 0x27;
        break;
    case 5:
    case 6:
        var9 = 0;
        var10 = 0x29;
        break;
    default:
        printf("** Hit Type err -> %d\n", t);
        return;
    }
    ((void **)dst)[1] = (void *)var10;
    ((void **)dst)[0] = (void *)buf->b;
    ((void **)dst)[2] = (void *)var9;
    ((void **)dst)[3] = (void *)hit;
    po = out;
    ((void **)dst)[4] = (void *)*(int *)((char *)po + 8);
    ((void **)dst)[6] = (void *)*(int *)((char *)po + 12);
    ((void **)dst)[7] = (void *)*(int *)((char *)po + 16);
    ((void **)dst)[8] = (void *)*(int *)((char *)po + 20);
    ((void **)dst)[9] = (void *)*(int *)((char *)po + 24);
    scenarioDstSet(dst);
    count++;
    *(ObjectTask **)((char *)calcUPGet((ObjectTask *)buf->b) + 0x150) = unit;
    buf->b = (int)tgtFindNext();
    if (buf->b != 0) {
        continue;
    }
    break;
    }
    src[0] = (int)unit;
    src[1] = buf->d;
    src[2] = buf->a;
    src[3] = buf->c;
    src[4] = count;
    scenarioSrcSet(src);
}

int unitCmdMove(UnitRecord *unit)
{
    Actor *actor;
    CalcUnitParam *calcParam;
    int useWarp;

    actor = unit->task.work;
    unitCmdPtrGet(&unit->task);
    if (!(*(u32 *)unit->task.work & ACTOR_FLAG_ENEMY)) {
        calcParam = calcUPGet(&unit->task);
        useWarp = calcParam->flags & 0x40;
    } else {
        calcParam = calcUPGet(&unit->task);
        useWarp = calcParam->moveKindId == 4;
    }
    switch (actor->moveState) {
    case 0:
        unitCmdDstSet(&unit->task, 3, 1);
        if (!useWarp) {
            unitCmdDirSet(unit);
        }
        calcUPGet(&unit->task);
        unitCmdVASet(&unit->task, 3, 0);
        actor->movePhase = 0;
        actor->moveState++;
        break;
    case 1:
        if (!useWarp) {
            if (unitCmdJump(&unit->task, 3, 1, 1) != 0) {
                unitCmdMoveNext(&unit->task, 1);
            }
        } else if (unitCmdWarp(&unit->task, 3, 1) != 0) {
            unitCmdMoveNext(&unit->task, 1);
        }
        break;
    }
    return 0;
}

extern void unitMotSet(ObjectTask *unit, int motion);

int unitCmdGuard(ObjectTask *unit)
{
    Actor *actor;

    actor = unit->work;
    unitCmdPtrGet(unit);
    if (actor->moveState == 0) {
        unitMotSet(unit, 0x1B);
        ((Actor *)unit->work)->flags |= 0x800;
        actor->moveState++;
    } else if ((*(Actor **)((char *)unit + 0x14))->moveFlags & 0x1000) {
        unitMotStandSet(unit);
        return unitCmdNext(unit);
    }
    return 0;
}

extern void calcStatResetAll(ObjectTask *unit);
extern void sndSeTransPlay(int unit, int seId, int volume);
extern void decoEffCall(ObjectTask *unit, int effectId);
extern int sndSysSePlay(int soundId);

int unitCmdAgws(ObjectTask *unit)
{
    int cmd[5];
    int *p = (int *)unitCmdPtrGet(unit);
    CalcUnitParam *up = calcUPGet(unit);
    short cid;
    int eff;
    int se;

    if (up->flags & 0x40) {
        cid = up->linkedUnit->charaId;
    } else {
        cid = *(short *)up->unmodeled_54;
    }
    calcStatResetAll(unit);
    cmd[0] = 0x2B;
    cmd[1] = p[1];
    cmd[2] = cid;
    unitCmdSet(unit, cmd);
    cmd[0] = 0x2D;
    unitCmdSet(unit, cmd);
    if (calcUPGet(unit)->flags & 0x40) {
        eff = 0x67;
        se = 0x20;
    } else {
        eff = 0x66;
        se = 0x1F;
        sndSeTransPlay((int)unit, 0x17, 3);
    }
    decoEffCall(unit, eff);
    sndSysSePlay(se);
    return unitCmdNext(unit);
}

int unitCmdUnitChange(ObjectTask *arg0)
{
    int *work;
    UnitCmd *cmd;
    UnitCmd sp;
    u16 hdr[4];
    UnitCmd list[5];
    int n;
    int wflags;
    void *spA0;
    int spA4;
    int spA8;
    CalcUnitParam *up;
    ObjectTask *nu;
    CalcUnitParam *np;
    int var5;
    int k;
    int cdSync;
    signed char b2;
    signed char b3;
    s16 cid;
    s16 ep;
    UnitCmd *lp;
    int flag;
    u16 h0;
    u16 h1;

    work = (int *)arg0->work;
    cmd = unitCmdPtrGet(arg0);
    switch (work[0x74 / 4]) {
    case 0:
        work[0x74 / 4] = 1;
        return 0;
    case 1:
        if (unitTelOut(arg0) != 0) {
            work[0x74 / 4]++;
        }
        return 0;
    case 2:
        up = calcUPGet(arg0);
        n = 0;
        wflags = *(int *)arg0->work;
        h0 = ((u16 *)cmd)[4];
        lp = list - 1;
        spA0 = *(void **)((char *)up + 0x48);
        h1 = ((u16 *)cmd)[2];
        spA8 = wflags & 8;
        flag = ((wflags >> 6) ^ 1) & 1;
        hdr[0] = h0;
        hdr[1] = h1;
        hdr[2] = 0;
        hdr[3] = 0;
        while (unitCmdNext(arg0) != 0) {
            lp++;
            *lp = *(UnitCmd *)unitCmdPtrGet(arg0);
            n++;
        }
        cid = up->charaId;
        ep = up->currentEp;
        spA4 = *(int *)((char *)arg0->work + 0x80);
        b2 = up->unmodeled_3a[2];
        b3 = up->unmodeled_3a[3];
        dataFileInfoInitChg(arg0, cid);
        if ((s16)hdr[0] == 6 && (calcStatGet(arg0, 7, 2) != 0 || calcStatGet(arg0, 7, 1) != 0)) {
            var5 = 1;
        } else {
            var5 = 0;
        }
        unitRemove((UnitRecord *)arg0, var5);
        if (flag) {
            nu = unitCreate((s16 *)hdr, 0, unitPlFunc);
        } else {
            nu = unitCreate((s16 *)hdr, 1, unitEnFunc);
        }
        cdSync = dataCdSync();
        unitLoad(nu, cid);
        unitTblChg(arg0, nu);
        if (calcUPGet(nu)->flags & 0x40) {
            calcUPGet(nu)->linkedUnit = (UnitRecord *)up;
            calcUPGet(nu)->currentEp = ep;
            calcUPGet(nu)->unmodeled_3a[2] = b2;
            calcUPGet(nu)->unmodeled_3a[3] = b3;
        } else if (!(*(int *)nu->work & 0x40) && !(calcUPGet(nu)->flags & 0x40)
                   && !(calcUPGet(nu)->charaId >= 0xBB && calcUPGet(nu)->charaId < 0xC3)) {
            calcUPGet(nu)->linkedUnit = (UnitRecord *)up;
            calcUPGet(nu)->currentEp = ep;
            calcUPGet(nu)->unmodeled_3a[2] = b2;
            calcUPGet(nu)->unmodeled_3a[3] = b3;
        } else if (calcUPGet(nu)->charaId >= 0xBB && calcUPGet(nu)->charaId < 0xC3) {
            calcUPGet(nu)->linkedUnit = (UnitRecord *)up;
            calcUPGet(nu)->unmodeled_3a[2] = b2;
            calcUPGet(nu)->unmodeled_3a[3] = b3;
        }
        *(int *)nu->work |= spA8;
        np = calcUPGet(nu);
        sp.w[0] = 0x2A;
        *(void **)((char *)np + 0x48) = spA0;
        sp.w[1] = cdSync;
        unitCmdSet(nu, sp.w);
        for (k = 0; k < n; k++) {
            sp = list[k];
            unitCmdSet(nu, sp.w);
        }
        *(int *)((char *)nu->work + 0x80) = spA4;
        return 1;
    }
    return 0;
}

int unitCmdStand(ObjectTask *unit)
{
    Actor *actor;

    actor = unit->work;
    if (actor->moveState == 0) {
        unitMotStandSet(unit);
        actor->flags &= ~ACTOR_FLAG_CMD_PENDING;
        actor->moveState++;
    }
    return unitCmdNext(unit);
}

int unitCmdEnd(ObjectTask *unit)
{
    return unitCmdStand(unit);
}

int unitCmdDmg(ObjectTask *unit)
{
    int *work = (int *)unit->work;
    int *p = (int *)unitCmdPtrGet(unit);
    int type = p[1];
    int motion;
    int face;

    if (work[0x74 / 4] == 0) {
        motion = 5;
        face = 0x3B;
        if (type != 1) {
            motion = 4;
            if (type < 2) {
                face = 0x3A;
            } else if (type == 2) {
                motion = 0x26;
            } else {
                face = 0x3A;
            }
        }
        unitMotSet(unit, motion);
        unitFaceMotSet(unit, face);
        sndSeTransPlay((int)unit, 2, type + 1);
        work[0x74 / 4] = work[0x74 / 4] + 1;
    } else if (((UnitRecord *)unit)->motionActor->moveFlags & 0x1000) {
        if (type == 2 && *(short *)calcUPGet(unit)->unmodeled_44 < 4) {
            CalcUnitParam *up = calcUPGet(unit);
            int v = *(unsigned short *)up->unmodeled_44 + 3;
            Vector4 *pos;
            Actor *a;

            *(short *)up->unmodeled_44 = v;
            pos = dataPosTblGet((short)v);
            a = ((UnitRecord *)unit)->motionActor;
            a->position.x = pos->x;
            a->position.y = pos->y;
            a->position.z = pos->z;
        }
        unitMotStandSet(unit);
        if (calcStatGet(unit, 1, 0x2000) != 0) {
            unitFaceMotSet(unit, 0x37);
        } else {
            unitFaceMotSet(unit, 0x35);
        }
        return unitCmdNext(unit);
    }
    return 0;
}

extern void unitMotSet(ObjectTask *unit, int motion);

int unitCmdEva(ObjectTask *unit)
{
    Actor *actor;

    actor = unit->work;
    if (actor->moveState == 0) {
        unitMotSet(unit, 0x1A);
        actor->moveState++;
    } else if ((*(Actor **)((char *)unit + 0x14))->moveFlags & 0x1000) {
        unitMotStandSet(unit);
        return unitCmdNext(unit);
    }
    return 0;
}

int unitCmdDef(ObjectTask *unit)
{
    Actor *actor;

    actor = unit->work;
    switch (actor->moveState) {
    case 0:
        unitMotSet(unit, 0x1E);
        actor->moveState++;
        break;
    case 1:
        if ((*(Actor **)((char *)unit + 0x14))->moveFlags & 0x1000) {
            if (!(actor->flags & 0x800)) {
                unitMotSet(unit, 0x1C);
            }
            actor->moveState++;
        }
        break;
    default:
        if ((*(Actor **)((char *)unit + 0x14))->moveFlags & 0x1000) {
            unitMotStandSet(unit);
            return unitCmdNext(unit);
        }
        break;
    }
    return 0;
}

extern void calcTurnEnd(ObjectTask *unit);
extern int unitDeadSpecChk(ObjectTask *unit);

int unitCmdDead(ObjectTask *unit)
{
    Actor *actor;
    int i;
    int s2;
    int s3;
    int s7;

    actor = unit->work;
    switch (actor->moveState) {
    case 0:
        if (actor->flags & 0x40) {
            ((union { int i; void *p; } *)actor)->i = actor->flags | 0x80;
        }
        if (((Actor *)unit->work)->flags & 0x800) {
            unitMotSet(unit, 0x1C);
            actor->moveState++;
        } else {
            actor->moveState = 2;
        }
        break;
    case 1:
        if (((UnitRecord *)unit)->motionActor->moveFlags & 0x1000) {
            actor->moveState++;
        }
        break;
    case 2:
        unitMotSet(unit, 6);
        calcTurnEnd(unit);
        if (((Actor *)unit->work)->flags & 0x40) {
            int se;
            if (calcUPGet(unit)->flags & 0x1000) {
                se = 0x23;
            } else {
                se = (calcUPGet(unit)->flags & 0x2000) ? 0x22 : 0x21;
            }
            sndSysSePlay(se);
        }
        sndSeTransPlay((int)unit, 2, 4);
        actor->moveState++;
        break;
    case 3:
        if (((UnitRecord *)unit)->motionActor->moveFlags & 0x1000) {
            actor->movePhaseTimer = 0;
            actor->moveState++;
        }
        break;
    case 4:
        s2 = calcUPGet(unit)->statActiveMask[2] & 0x4000;
        s3 = calcUPGet(unit)->statActiveMask[3] & 0x800;
        s7 = calcUPGet(unit)->statActiveMask[7] & 7;
        for (i = 0; i < 8; i++) {
            calcUPGet(unit)->statActiveMask[i] = 0;
        }
        calcUPGet(unit)->statActiveMask[2] |= s2;
        calcUPGet(unit)->statActiveMask[3] |= s3;
        calcUPGet(unit)->statActiveMask[7] |= s7;
        if (((Actor *)unit->work)->flags & 0x40) {
            if (unitDeadSpecChk(unit) != 0) {
                if (unitTelOut(unit) != 0) {
                    actor->moveState++;
                }
            } else {
                actor->moveState++;
            }
        } else {
            actor->moveState++;
        }
        break;
    case 5:
        ((union { int i; void *p; } *)actor)->i = (actor->flags & 0x61) | 0x80;
        if (((Actor *)unit->work)->flags & 0x40) {
            unitTblRemove(unit);
            if (unitDeadSpecChk(unit) != 0) {
                unitRemove((UnitRecord *)unit, 0);
            }
        } else {
            actor->moveState++;
        }
        break;
    }
    return 0;
}

int unitDeadSpecChk(ObjectTask *unit)
{
    short charaId;

    charaId = calcUPGet(unit)->charaId;
    switch (charaId) {
    case 0xA2:
    case 0xAA:
    case 0xAB:
    case 0xB0:
    case 0xB2:
    case 0xB3:
    case 0xB4:
    case 0xB7:
        return 0;
    default:
        return 1;
    }
}

int unitCmdMot(ObjectTask *unit)
{
    Actor *actor;
    int *cmd;
    int t;

    actor = unit->work;
    cmd = unitCmdPtrGet(unit);
    if (actor->moveState == 0) {
        unitMotSet(unit, cmd[1]);
        actor->moveState++;
    } else {
        t = cmd[2];
        if (t == 0) {
            if (!((*(Actor **)((char *)unit + 0x14))->moveFlags & 0x1000))
                goto out;
        } else if (t != actor->runFrameCounter) {
            goto out;
        }
        unitMotStandSet(unit);
        return unitCmdNext(unit);
    }
out:
    unitMtdProc((UnitRecord *)unit);
    return 0;
}

int tgtFindFirst(ObjectTask *, int *, int);

int unitCmdTgtSet(ObjectTask *unit)
{
    Actor *actor = unit->work;
    int *cmd = unitCmdPtrGet(unit);
    int buf[4];
    int r = 0;
    int t;

    if (actor->moveState == 0) {
        buf[0] = cmd[1];
        t = cmd[2];
        buf[2] = cmd[3];
        buf[1] = t;
        buf[3] = cmd[4];
        r = tgtFindFirst(unit, buf, t);
        if (r != 0) {
            *(int *)((char *)unit->work + 0x80) = r;
            return unitCmdNext(unit);
        }
        printf("\n** unitCmdTgtSet: err\n");
        cmdListCansel((int)unit);
        unitCmdClear(unit);
        return unitCmdNext(unit);
    }
    return r;
}

extern int unitCmdTurnSub(ObjectTask *unit, int mode);

void unitCmdTurn(ObjectTask *unit)
{
    Actor *actor;
    Actor *w;
    int *cmd;
    Vector4 *tgt;
    struct { UnitRecord **p; } tb;
    UnitRecord *u;
    UnitRecord **cur;
    Actor *m;
    int count;
    int i;
    int n;
    int idx;
    float f;

    actor = unit->work;
    cmd = unitCmdPtrGet(unit);
    w = unit->work;
    tgt = (Vector4 *)&w->motionFrame;
    if (actor->moveState == 0) {
        if (cmd[0] == 15) {
            if (unit == w->targetUnit) {
                unitCmdNext(unit);
                return;
            }
            m = ((UnitRecord *)w->targetUnit)->motionActor;
            idx = 0;
            tgt->x = m->position.x;
            tgt->z = m->position.z;
        } else if (cmd[0] == 16) {
            idx = cmd[1];
            tgt->x = dataPosTblGet(idx)->x;
            tgt->z = dataPosTblGet(idx)->z;
        } else {
            n = 0;
            idx = 0;
            tgt->x = tgt->z = 0.0f;
            count = unitTblGet((((int)w->flags >> 6) ^ 1) & 1, &tb.p);
            for (i = 0; i < count; i++) {
                u = tb.p[i];
                if (u != 0) {
                    n++;
                    tgt->x += u->motionActor->position.x;
                    tgt->z += u->motionActor->position.z;
                }
            }
            if (n > 0) {
                f = n;
                tgt->x = tgt->x / f;
                tgt->z = tgt->z / f;
            }
        }
        if (idx < 0) {
            tgt->x = -tgt->x;
        }
        actor->movePhaseTimer = 0;
        actor->moveState++;
    }
    if (unitCmdTurnSub(unit, 0) != 0) {
        unitCmdNext(unit);
    }
}

extern void unitSetInterp(void *unit);
extern void unitSetInterpTime(ObjectTask *unit, float time);

int unitCmdTurnSub(ObjectTask *unit, int mode)
{
    Actor *actor;
    Actor *m;
    CalcUnitParam *param;
    Vector4 *dst;
    Vector4 *tgt;
    float rate;
    float d;
    float nd;
    int done;

    done = 0;
    rate = 0.0f;
    actor = unit->work;
    m = ((UnitRecord *)unit)->motionActor;
    param = calcUPGet(unit);
    tgt = (Vector4 *)&((Actor *)unit->work)->motionFrame;
    dst = (Vector4 *)((char *)((Actor *)unit->work) + 0xB0);
    if (actor->movePhaseTimer == 0) {
        if (tgt->x == m->position.x && tgt->z == m->position.z) {
            dst->y = *(float *)((char *)param + 0x4C);
        } else {
            dst->y = atan2f(tgt->x - m->position.x, tgt->z - m->position.z);
        }
        if (dst->y < 0.0f) {
            dst->y += 6.2831855f;
        }
        tgt->y = dst->y - m->rotation.y;
        if (tgt->y == 0.0f) {
            return 1;
        }
        if (mode == 0) {
            if ((signed char)calcUPGet(unit)->unmodeled_14[1] >= 2 &&
                (calcUPGet(unit)->charaId < 0xBB || calcUPGet(unit)->charaId >= 0xC3)) {
                rate = 16.0f;
            } else {
                rate = 5.0f;
            }
        } else {
            if ((signed char)calcUPGet(unit)->unmodeled_14[1] >= 2 &&
                (calcUPGet(unit)->charaId < 0xBB || calcUPGet(unit)->charaId >= 0xC3)) {
                rate = 10.0f;
            } else {
                rate = 5.0f;
            }
        }
        if (tgt->y < 0.0f) {
            nd = -tgt->y;
            if (nd > 3.1415927f) {
                goto wrap;
            }
        } else if (tgt->y > 3.1415927f) {
        wrap:
            if (tgt->y < 0.0f) {
                tgt->y = tgt->y + 6.2831855f;
                dst->y = dst->y + 6.2831855f;
            } else {
                tgt->y = tgt->y - 6.2831855f;
                dst->y = dst->y + 6.2831854820251465;
            }
        }
        tgt->y = tgt->y / rate;
        if (mode == 0) {
            if (calcUPGet(unit)->charaId < 0xBB || calcUPGet(unit)->charaId >= 0xC3) {
                unitSetInterp(unit);
                unitSetInterpTime(unit, rate * 0.033333335f);
            }
            d = m->rotation.y - dst->y;
            if (d < 0.0f) {
                d = -d;
                if (d > 0.000001f) {
                    goto mot;
                }
            } else if (d > 0.000001f) {
            mot:
                if ((signed char)calcUPGet(unit)->unmodeled_14[1] >= 2) {
                    unitMotSet(unit, 0x17);
                }
            }
        }
        actor->movePhaseTimer++;
    } else {
        if (m->rotation.y != dst->y) {
            d = m->rotation.y + tgt->y;
            m->rotation.y = d;
            if ((tgt->y > 0.0f && dst->y < m->rotation.y) ||
                (tgt->y < 0.0f && m->rotation.y < dst->y)) {
                goto snap;
            }
            d = m->rotation.y - dst->y;
            if (d < 0.0f) {
                d = -d;
                if (d <= 0.000001f) {
                    goto snap;
                }
            } else if (d <= 0.000001f) {
            snap:
                m->rotation.y = dst->y;
                goto set;
            }
        } else {
        set:
            done = 1;
        }
        if (mode == 0 && done) {
            unitSetInterpTime(unit, 0.26666668f);
        }
    }
    return done;
}

typedef struct { int x, y, z, w; } __attribute__((aligned(16))) TurnVec;
#define TURN_WORK(u) ((Actor *)((int *)(u))[4])

void unitCmdTurnSubInit(UnitRecord *unit, int mode)
{
    Actor *m = unit->motionActor;
    float dx, dz;

    TURN_WORK(unit)->movePhaseTimer = 0;
    *(TurnVec *)&TURN_WORK(unit)->motionFrame = *(TurnVec *)&TURN_WORK(unit)->cmdTarget;
    if (mode == 1) {
        Actor *a = TURN_WORK(unit);
        dx = ((Vector4 *)&a->motionFrame)->x - m->position.x;
        dz = ((Vector4 *)&a->motionFrame)->z - m->position.z;
        ((Vector4 *)&a->motionFrame)->x = m->position.x - dx;
        ((Vector4 *)&a->motionFrame)->z = m->position.z - dz;
    }
}

extern float unitCmdLenGet(void *arg0);
extern int unitCmdMoveChaNorm(UnitRecord *unit);
extern int unitCmdMoveChaRun(ObjectTask *unit);
extern int unitCmdMoveChaJump(ObjectTask *unit);
extern int unitCmdMoveChaHover(ObjectTask *unit);
extern int unitCmdMoveChaWarp(ObjectTask *unit);
extern int unitCmdMoveChaJump2(UnitRecord *unit);
extern int unitCmdMoveChaHover2(ObjectTask *unit);
extern int unitCmdMoveChaFloat(ObjectTask *unit);
extern int unitCmdMoveChaHover3(ObjectTask *unit);

int unitCmdMoveCha(ObjectTask *unit)
{
    Actor *actor;
    int *cmd;
    float len;

    actor = unit->work;
    cmd = unitCmdPtrGet(unit);
    unitCmdDstSet(unit, 3, 0);
    len = unitCmdLenGet(unit);
    actor->movePhase = 0;
    switch (calcUPGet(unit)->moveKindId) {
    case 0:
        if (len <= 255.0f) {
            unitCmdVASet(unit, 3, 0);
            *cmd = 0x17;
            unitCmdMoveChaJump(unit);
        } else {
            *cmd = 0x13;
            unitCmdMoveChaNorm((UnitRecord *)unit);
        }
        break;
    case 1:
        unitCmdVASet(unit, 3, 0);
        *cmd = 0x17;
        unitCmdMoveChaJump(unit);
        break;
    case 3:
        *cmd = 0x19;
        unitCmdMoveChaHover(unit);
        break;
    case 4:
        *cmd = 0x1B;
        unitCmdMoveChaWarp(unit);
        break;
    case 5:
        *cmd = 0x1D;
        unitCmdMoveChaJump2((UnitRecord *)unit);
        break;
    case 2:
    case 6:
        unitCmdVASet(unit, 1, 0);
        *cmd = 0x15;
        unitCmdMoveChaRun(unit);
        break;
    case 7:
        unitCmdVASet(unit, 9, 0);
        *cmd = 0x1F;
        unitCmdMoveChaHover2(unit);
        break;
    case 8:
        *cmd = 0x21;
        unitCmdMoveChaFloat(unit);
        break;
    case 9:
        unitCmdVASet(unit, 9, 0);
        *cmd = 0x23;
        unitCmdMoveChaHover3(unit);
        break;
    default:
        printf("** unitCmdMoveCha: err -> %d\n", calcUPGet(unit)->moveKindId);
        break;
    }
    return 0;
}

/*
 * calcUPGet(unit)+0x44 (ov01/tu004 calc.c's own unmodeled span) is the same
 * position-table-index half unitCmdDstSet and unitCmdDirAdj read later in
 * this file (CALC_UNIT_PARAM_UNK44_OFF below); unitCmdMoveNext is its only
 * claimed writer, copying it from the queued command entry's own index at
 * cmdEntry+0x4 (CMD_ENTRY_X_OR_IDX_OFF below, also read there). Offset
 * 0x4C is a float this TU does not otherwise claim, restored into the
 * motion actor's yaw on every call.
 */
#define CALC_UNIT_PARAM_UNK44_OFF 0x44
#define CMD_ENTRY_X_OR_IDX_OFF 0x4
#define CALC_UNIT_PARAM_DEFAULT_YAW_OFF 0x4C

int unitCmdMoveNext(ObjectTask *unit, int mode)
{
    Actor *actor;
    void *cmdEntry;

    actor = ((UnitRecord *) unit)->motionActor;
    cmdEntry = unitCmdPtrGet(unit);
    actor->rotation.y = *(float *)((char *)calcUPGet(unit) + CALC_UNIT_PARAM_DEFAULT_YAW_OFF);
    if (mode == 1) {
        *(u16 *)((char *)calcUPGet(unit) + CALC_UNIT_PARAM_UNK44_OFF) =
            *(u16 *)((char *)cmdEntry + CMD_ENTRY_X_OR_IDX_OFF);
    } else {
        unitCmdDstSet(unit, 0, 0);
        unitCmdDirSet((UnitRecord *) unit);
    }
    if (mode == 1) {
        unitMotStandSet(unit);
    }
    return unitCmdNext(unit);
}

extern float unitCmdLenGet(void *arg0);
extern void unitCmdDivSet(UnitRecord *unit, float distance, float step);

int unitCmdMoveChaNorm(UnitRecord *unit)
{
    Actor *actor;
    float distance;

    actor = unit->task.work;
    switch (actor->moveState) {
    case 0:
        distance = unitCmdLenGet(unit);
        unitCmdDivSet(unit, distance, distance - 255.0f);
        unitCmdVASet(&unit->task, 1, 0);
        unitCmdRun(&unit->task, 1, 2);
        actor->moveState++;
        break;
    case 1:
        if (unitCmdRun(&unit->task, 1, 2) != 0) {
            actor->moveState++;
        }
        break;
    case 2:
        actor->movePhase = 0;
        unitCmdDstSet(&unit->task, 3, 0);
        unitCmdVASet(&unit->task, 3, 0);
        unitCmdJump(&unit->task, 3, 0, 0);
        actor->moveState++;
        break;
    default:
        if (unitCmdJump(&unit->task, 3, 0, 0) != 0) {
            unitCmdMoveNext(&unit->task, 0);
        }
        break;
    }
    return 0;
}

int unitCmdMoveChaRun(ObjectTask *unit)
{
    if (unitCmdRun(unit, 1, 0) != 0) {
        unitCmdMoveNext(unit, 0);
    }
    return 0;
}

int unitCmdMoveChaJump(ObjectTask *unit)
{
    if (unitCmdJump(unit, 3, 0, 0) != 0) {
        unitCmdMoveNext(unit, 0);
    }
    return 0;
}

int unitCmdMoveChaHover(ObjectTask *unit)
{
    if (unitCmdHover(unit, 5, 0) != 0) {
        unitCmdMoveNext(unit, 0);
    }
    return 0;
}

int unitCmdMoveChaWarp(ObjectTask *unit)
{
    if (unitCmdWarp(unit, 7, 0) != 0) {
        unitCmdMoveNext(unit, 0);
    }
    return 0;
}

extern float unitCmdLenGet(void *arg0);
extern void unitCmdDivSet(UnitRecord *unit, float distance, float step);

int unitCmdMoveChaJump2(UnitRecord *unit)
{
    Actor *actor;
    float *saved;
    float distance;
    float step;
    int n;

    actor = unit->task.work;
    n = 3;
    unitCmdPtrGet(&unit->task);
    saved = (float *)((char *)unit->task.work + 0xD4);
    if (actor->moveState == 0) {
        distance = unitCmdLenGet(unit);
        *saved = distance / (float)n;
        unitCmdDivSet(unit, distance, distance / (float)n);
        unitCmdVASet(&unit->task, 3, 0);
        actor->moveState++;
    } else if (actor->moveState > n) {
        unitCmdMoveNext(&unit->task, 0);
    } else if (unitCmdJump(&unit->task, 3, 2, 0) != 0) {
        actor->movePhase = 0;
        unitCmdDstSet(&unit->task, 3, 0);
        distance = unitCmdLenGet(unit);
        if (actor->moveState == n) {
            step = distance;
        } else {
            step = *saved;
        }
        unitCmdDivSet(unit, distance, step);
        unitCmdVASet(&unit->task, 3, 0);
        actor->moveState++;
    }
    return 0;
}

int unitCmdMoveChaHover2(ObjectTask *unit)
{
    if (unitCmdJump(unit, 9, 0, 0) != 0) {
        unitCmdMoveNext(unit, 0);
    }
    return 0;
}

int unitCmdMoveChaFloat(ObjectTask *unit)
{
    if (unitCmdFloat(unit, 0xB, 0) != 0) {
        unitCmdMoveNext(unit, 0);
    }
    return 0;
}

int unitCmdMoveChaHover3(ObjectTask *unit)
{
    if (unitCmdJump(unit, 0xD, 0, 0) != 0) {
        unitCmdMoveNext(unit, 0);
    }
    return 0;
}

extern int unitCmdMovePosNorm(ObjectTask *unit);
extern int unitCmdMovePosRun(ObjectTask *unit);
extern int unitCmdMovePosJump(ObjectTask *unit);
extern int unitCmdMovePosHover(ObjectTask *unit);
extern int unitCmdMovePosWarp(ObjectTask *unit);
extern int unitCmdMovePosHover2(ObjectTask *unit);
extern int unitCmdMovePosFloat(ObjectTask *unit);
extern int unitCmdMovePosHover3(ObjectTask *unit);

int unitCmdMovePos(ObjectTask *unit)
{
    Actor *actor;
    int *cmd;
    float len;

    actor = unit->work;
    cmd = unitCmdPtrGet(unit);
    unitCmdDstSet(unit, 4, 1);
    len = unitCmdLenGet(unit);
    actor->movePhase = 0;
    switch (calcUPGet(unit)->moveKindId) {
    case 0:
        if (!(len <= 255.0f)) {
            *cmd = 0x14;
            unitCmdMovePosNorm(unit);
        } else {
            goto block_10;
        }
        break;
    case 2:
        unitCmdVASet(unit, 2, 0);
        *cmd = 0x16;
        unitCmdMovePosRun(unit);
        break;
    case 3:
        *cmd = 0x1A;
        unitCmdMovePosHover(unit);
        break;
    case 4:
        *cmd = 0x1C;
        unitCmdMovePosWarp(unit);
        break;
    case 5:
        *cmd = 0x1E;
        unitCmdMovePosJump(unit);
        break;
    case 1:
    case 6:
    block_10:
        unitCmdVASet(unit, 4, 0);
        *cmd = 0x18;
        unitCmdMovePosJump(unit);
        break;
    case 7:
        unitCmdVASet(unit, 0xA, 0);
        *cmd = 0x20;
        unitCmdMovePosHover2(unit);
        break;
    case 8:
        *cmd = 0x22;
        unitCmdMovePosFloat(unit);
        break;
    case 9:
        unitCmdVASet(unit, 0xA, 0);
        *cmd = 0x24;
        unitCmdMovePosHover3(unit);
        break;
    default:
        printf("** unitCmdMovePos: err -> %d\n", calcUPGet(unit)->moveKindId);
        break;
    }
    return 0;
}

typedef signed int s32;

s32 unitCmdMovePosNorm(ObjectTask *arg0)
{
  s32 temp_3;
  void *temp_17;
  void *temp_4;
  temp_17 = arg0->work;
  temp_4 = unitCmdPtrGet(arg0);
  temp_3 = *((int *) (((char *) temp_17) + 0x74));
  switch (temp_3)
  {
    case 0:
      *((int *) (((char *) temp_17) + 0x80)) = (s32) (*((int *) (((char *) temp_4) + 0x4)));
      unitCmdDivSet((UnitRecord *) arg0, unitCmdLenGet(arg0), 255.0f);
      unitCmdVASet(arg0, 4, 0);
      unitCmdJump(arg0, 4, 2, 0);
      *((int *) (((char *) temp_17) + 0x74)) = (s32) ((*((int *) (((char *) temp_17) + 0x74))) + 1);
      break;

    case 1:
      if (unitCmdJump(arg0, 4, 2, 0) != 0)
    {
      *((int *) (((char *) temp_17) + 0x74)) = (s32) ((*((int *) (((char *) temp_17) + 0x74))) + 1);
    }
      break;

    case 2:
      *((int *) (((char *) temp_17) + 0x78)) = 0;
      *((int *) (((char *) temp_4) + 0x4)) = (s32) (*((int *) (((char *) temp_17) + 0x80)));
      unitCmdDstSet(arg0, 2, 1);
      unitCmdVASet(arg0, 2, 0);
      unitCmdDirSet((UnitRecord *) arg0);
      unitCmdRun(arg0, 2, 1);
      goto block_17;
      block_17:
    *((int *) (((char *) temp_17) + 0x74)) = (s32) ((*((int *) (((char *) temp_17) + 0x74))) + 1);

      break;

    case 3:
      if (unitCmdRun(arg0, 2, 1) != 0)
    {
      unitCmdMoveNext(arg0, 1);
    }
      break;

  }

  return 0;
}

int unitCmdMovePosRun(ObjectTask *unit)
{
    if (unitCmdRun(unit, 2, 1) != 0) {
        unitCmdMoveNext(unit, 1);
    }
    return 0;
}

int unitCmdMovePosJump(ObjectTask *unit)
{
    if (unitCmdJump(unit, 4, 1, 0) != 0) {
        unitCmdMoveNext(unit, 1);
    }
    return 0;
}

int unitCmdMovePosHover(ObjectTask *unit)
{
    if (unitCmdHover(unit, 6, 1) != 0) {
        unitCmdMoveNext(unit, 1);
    }
    return 0;
}

int unitCmdMovePosWarp(ObjectTask *unit)
{
    if (unitCmdWarp(unit, 8, 1) != 0) {
        unitCmdMoveNext(unit, 1);
    }
    return 0;
}

int unitCmdMovePosHover2(ObjectTask *unit)
{
    if (unitCmdJump(unit, 0xA, 1, 0) != 0) {
        unitCmdMoveNext(unit, 1);
    }
    return 0;
}

int unitCmdMovePosFloat(ObjectTask *unit)
{
    if (unitCmdFloat(unit, 0xC, 1) != 0) {
        unitCmdMoveNext(unit, 1);
    }
    return 0;
}

int unitCmdMovePosHover3(ObjectTask *unit)
{
    if (unitCmdJump(unit, 0xE, 1, 0) != 0) {
        unitCmdMoveNext(unit, 1);
    }
    return 0;
}

/*
 * ov01/tu004 calc.c owns CalcUnitParam and does not model this signed half;
 * unitCmdDirAdj is the only claimed reader, using its sign to flip which side
 * of the move target the retreat offset below is placed on.
 */
#define CALC_UNIT_PARAM_UNK44_OFF 0x44

void unitCmdDirAdj(ObjectTask *unit, int cmdId, int flip)
{
    Actor *actor = unit->work;
    Actor *motionActor = ((UnitRecord *)unit)->motionActor;
    Vector4 *target = &actor->cmdTarget;
    float offset = !flip ? -1.0f : 1.0f;

    if (*(short *)((char *)calcUPGet(unit) + CALC_UNIT_PARAM_UNK44_OFF) < 0) {
        offset = -offset;
    }
    motionActor->rotation.y = atan2f(target->x + offset - motionActor->position.x,
                                      target->z - motionActor->position.z);
    if (cmdId == 4 || cmdId == 2 || cmdId == 6 || cmdId == 0xA || cmdId == 0xE) {
        motionActor->rotation.y += 3.14159274f;
    }
}

typedef struct AdjTbl {
    int charaId;
    float *tbl[4];
} AdjTbl;
typedef struct AdjVec {
    float x, y, z, w;
} __attribute__((aligned(16))) AdjVec;
extern AdjTbl adjTblPl[];
extern AdjTbl adjTblEn[];

void unitCmdPosAdj(UnitRecord *unit, int cmdId)
{
    Actor *actor;
    Actor *motion;
    Vector4 *target;
    AdjVec saved;
    int mot;
    AdjTbl *tbl;
    AdjTbl *p;
    float *adj;
    float *f;
    float step;
    float distance;
    int frame;
    int i;
    int idx;

    idx = 0;
    actor = unit->task.work;
    motion = unit->motionActor;
    target = &actor->cmdTarget;
    frame = motion->motionFrame;
    if (!(actor->flags & 0x40) && !(calcUPGet(&unit->task)->flags & 0x40) &&
        (calcUPGet(&unit->task)->charaId < 0xBB || calcUPGet(&unit->task)->charaId >= 0xC3)) {
        tbl = adjTblPl;
        for (i = 0, p = tbl; p->charaId != 0; p++, i++) {
            if (p->charaId == calcUPGet(&unit->task)->charaId) {
                idx = i;
                break;
            }
        }
    } else {
        tbl = adjTblEn;
        for (i = 0, p = tbl; p->charaId != 0; p++, i++) {
            if (p->charaId == calcUPGet(&unit->task)->charaId) {
                idx = i;
                break;
            }
        }
    }
    mot = unitMotGet(&unit->task);
    if (mot == 13) {
        adj = tbl[idx].tbl[0];
    } else if (mot == 17 || mot == 15) {
        adj = tbl[idx].tbl[1];
    } else if (mot == 18) {
        adj = tbl[idx].tbl[2];
    } else if (mot == 22 || mot == 20) {
        adj = tbl[idx].tbl[3];
    } else {
        return;
    }
    if (adj != 0) {
        calcUPGet(&unit->task);
        f = (float *)((char *)(frame * 4) + (int)adj);
        step = *f;
        if (frame != 0) {
            step -= f[-1];
        }
        saved = *(AdjVec *)target;
        distance = unitCmdLenGet(unit);
        if (distance < step) {
            step = distance;
        }
        unitCmdDivSet(unit, distance, step);
        motion->position.x = target->x;
        motion->position.z = target->z;
        *(AdjVec *)target = saved;
    }
}

int unitCmdRun(ObjectTask *unit, int arg1, int arg2)
{
    Actor *work = unit->work;
    Actor *motionActor = ((UnitRecord *)unit)->motionActor;
    Vector4 *cmdTgt = &work->cmdTarget;
    float *saved = (float *)&work->motion;
    int var5;
    int var17;
    int var22;
    float var20;
    float len;

    switch (work->movePhase) {
    case 0:
        if (arg1 == 1) {
            var5 = 0xD;
            var17 = 1;
            var22 = 0;
        } else {
            var5 = 0x12;
            var17 = 3;
            var22 = 1;
        }
        unitMotSet(unit, var5);
        saved[0] = cmdTgt->x;
        saved[1] = cmdTgt->y;
        saved[2] = cmdTgt->z;
        sndSeTransPlay((int)unit, 1, var17);
        work->movePhase++;
        unitCmdTurnSubInit((UnitRecord *)unit, var22);
        /* fallthrough */
    case 1:
        unitCmdTurnSub(unit, 1);
        if (((Actor *)unit->work)->runFrameCounter % 3 == 0) {
            decoEffCall(unit, 0x65);
        }
        if (((UnitRecord *)unit)->motionActor->moveFlags & 0x1000) {
            unitMotSet(unit, (arg1 == 1) ? 0xE : 0x13);
            var20 = 0.8f;
            if ((signed char)calcUPGet(unit)->unmodeled_14[1] != 0) {
                var20 = 1.0f;
                if ((signed char)calcUPGet(unit)->unmodeled_14[1] != 1) {
                    var20 = 1.5f;
                }
            }
            len = unitCmdLenGet(unit);
            if (len < var20) {
                var20 = len;
            }
            unitCmdDivSet((UnitRecord *)unit, len, var20);
            motionActor->position.x = cmdTgt->x;
            motionActor->position.z = cmdTgt->z;
            cmdTgt->x = saved[0];
            cmdTgt->y = saved[1];
            cmdTgt->z = saved[2];
            unitCmdVASet(unit, arg1, 0);
            work->movePhase++;
        }
        goto _sw_end;
    case 2:
        unitCmdDirAdj(unit, arg1, arg2);
        if (((Actor *)unit->work)->runFrameCounter % 3 == 0) {
            decoEffCall(unit, 0x65);
        }
        if (unitCmdMoveCalc(unit, 0, 0, 0) != 0) {
            sndSeTransPlay((int)unit, 1, ((arg1 ^ 1) == 0) ? 2 : 4);
            unitMotStandSet(unit);
            return 1;
        }
        goto _sw_end;
    default:
        _sw_end:
    return 0;
    }
}

s32 unitCmdMoveCalc(void *arg0, s32 arg1, s32 arg2, s32 arg3);

int unitCmdJump(ObjectTask *unit, int arg1, int arg2, int arg3)
{
    Actor *work = unit->work;
    Vector4 *cmdTgt = &work->cmdTarget;
    float *saved = (float *)&work->motion;
    Actor *motionActor = ((UnitRecord *)unit)->motionActor;
    int mot;
    int snd;
    int pre;
    int dir;
    float f20;
    float len;

    switch (work->movePhase) {
    case 0:
    case 1:
        if (work->movePhase == 0) {
            if (unitMotGet(unit) == 2) {
                unitMotSet(unit, 1);
                work->movePhase++;
                goto _sw_end;
            }
            work->movePhase += 2;
        } else {
            if ((s32)((motionActor->moveElapsed / 0.033333335f) + 0.5f) >= 9) {
                work->movePhase = work->movePhase + 1;
            } else {
                goto _sw_end;
            }
        }
        /* fallthrough */
    case 2:
        if (arg1 == 3 || arg1 == 9 || arg1 == 0xD) {
            mot = 0xD;
            snd = 1;
            dir = 0;
        } else {
            mot = 0x12;
            snd = 3;
            dir = 1;
        }
        unitMotSet(unit, mot);
        work->movePhase++;
        saved[0] = cmdTgt->x;
        saved[1] = cmdTgt->y;
        saved[2] = cmdTgt->z;
        decoEffCall(unit, 0x65);
        sndSeTransPlay((int)unit, 1, snd);
        unitCmdTurnSubInit((UnitRecord *)unit, dir);
        /* fallthrough */
    case 3:
        unitCmdTurnSub(unit, 1);
        if (!(((UnitRecord *)unit)->motionActor->moveFlags & 0x1000)) {
            goto block_62;
        }
        if (arg1 == 3 || arg1 == 9 || arg1 == 0xD) {
            mot = 0xE;
        } else {
            mot = 0x13;
        }
        unitMotSet(unit, mot);
        f20 = 1.0f;
        if ((signed char)calcUPGet(unit)->unmodeled_14[1] < 2) {
            f20 = 0.8f;
        }
        len = unitCmdLenGet(unit);
        if ((unsigned)(arg1 - 0xD) < 2U) {
            ((UnitRecord *)unit)->motionActor->moveStepRate = (1.0f / (len / 10.0f)) * 0.033333335f;
        }
        if (len < f20) {
            f20 = len;
        }
        unitCmdDivSet((UnitRecord *)unit, len, f20);
        motionActor->position.x = cmdTgt->x;
        work->movePhase++;
        motionActor->position.z = cmdTgt->z;
        goto _sw_end;
    case 4:
        cmdTgt->x = saved[0];
        cmdTgt->y = saved[1];
        cmdTgt->z = saved[2];
        f20 = 1.0f;
        if ((signed char)calcUPGet(unit)->unmodeled_14[1] < 2) {
            f20 = 0.8f;
        }
        len = unitCmdLenGet(unit);
        if (len < f20) {
            f20 = len;
        } else {
            f20 = len - f20;
        }
        unitCmdDivSet((UnitRecord *)unit, len, f20);
        unitCmdVASet(unit, arg1, 0);
        unitCmdMoveCalc(unit, 0, 1, 0);
        mot = (arg1 == 3 || arg1 == 9) ? 0xF : 0x14;
        if ((unsigned)(arg1 - 0xD) >= 2U) {
            unitSetInterp(unit);
            unitMotSet(unit, mot);
        }
        work->movePhase++;
        goto _sw_end;
    case 5:
        if (arg3 == 0) {
            unitCmdDirAdj(unit, arg1, arg2);
        }
        pre = (motionActor->velocity.y < 0.0f) ? -1 : 1;
        if (unitCmdMoveCalc(unit, 0, 1, 0) != 0) {
            if (arg1 == 3 || arg1 == 9) {
                mot = 0x11;
                snd = 2;
            } else {
                mot = 0x16;
                snd = 4;
            }
            if (arg1 == 0xD) {
                mot = 0xF;
                snd = 2;
            } else if (arg1 == 0xE) {
                mot = 0x14;
                snd = 4;
            }
            unitMotSet(unit, mot);
            ((UnitRecord *)unit)->motionActor->moveStepRate = 0.033333335f;
            work->movePhase++;
            cmdTgt->x = saved[0];
            cmdTgt->y = saved[1];
            cmdTgt->z = saved[2];
            decoEffCall(unit, 0x65);
            sndSeTransPlay((int)unit, 1, snd);
block_62:
            unitCmdPosAdj((UnitRecord *)unit, arg1);
            break;
        }
        if ((unsigned)(arg1 - 3) < 2U) {
            if (pre != ((motionActor->velocity.y < 0.0f) ? -1 : 1)) {
                goto motion;
            }
        }
        if ((unsigned)(arg1 - 9) >= 2U) {
            goto _sw_end;
        }
        if ((s32)((Actor *)unit->work)->posSnapX != (s32)(((Actor *)unit->work)->posSnapZ * 0.5f)) {
            goto _sw_end;
        }
motion:
            mot = (arg1 == 3 || arg1 == 9) ? 0x10 : 0x15;
            unitSetInterp(unit);
            unitMotSet(unit, mot);
            goto _sw_end;
    default:
        if (((UnitRecord *)unit)->motionActor->moveFlags & 0x1000) {
            motionActor->position.x = saved[0];
            motionActor->position.y = saved[1];
            motionActor->position.z = saved[2];
            unitSetInterpTime(unit, 0.26666668f);
            return 1;
        }
        if (arg3 == 0) {
            unitCmdDirAdj(unit, arg1, arg2);
        }
        unitCmdPosAdj((UnitRecord *)unit, arg1);
        break;
    }
    _sw_end:
    return 0;
}

extern int calcSpline(void *spline, float *out, int type);

int unitCmdHover(ObjectTask *unit, int arg1, int arg2)
{
    Actor *work = unit->work;
    Actor *motionActor = ((UnitRecord *)unit)->motionActor;
    Vector4 *cmdTgt = &work->cmdTarget;
    int var5;
    int var17;
    int var20;

    switch (work->movePhase) {
    case 0:
        if (arg1 == 5) {
            var5 = 0xD;
            var17 = 1;
            var20 = 0;
        } else {
            var5 = 0x12;
            var17 = 3;
            var20 = 1;
        }
        unitMotSet(unit, var5);
        sndSeTransPlay((int)unit, 1, var17);
        decoEffCall(unit, 0x65);
        work->movePhase++;
        unitCmdTurnSubInit((UnitRecord *)unit, var20);
        /* fallthrough */
    case 1:
        unitCmdTurnSub(unit, 1);
        if (((UnitRecord *)unit)->motionActor->moveFlags & 0x1000) {
            unitMotSet(unit, (arg1 == 5) ? 0xE : 0x13);
            unitCmdVASet(unit, arg1, 0);
            work->movePhase++;
        }
        return 0;
    case 2:
        unitCmdDirAdj(unit, arg1, arg2);
        if (calcSpline(D_00A57268, (float *)&motionActor->position, 3) != 0) {
            work->movePhase++;
        }
        if (arg1 == 5) {
            if (unitMotGet(unit) != 0xF) {
                unitSetInterp(unit);
                unitMotSet(unit, 0xF);
            }
        } else if (unitMotGet(unit) != 0x14) {
            unitSetInterp(unit);
            unitMotSet(unit, 0x14);
        }
        return 0;
    default:
        if (motionActor->moveFlags & 0x1000) {
            motionActor->position.x = cmdTgt->x;
            motionActor->position.z = cmdTgt->z;
            unitSetInterpTime(unit, 0.26666668f);
            decoEffCall(unit, 0x65);
            return 1;
        }
        unitCmdDirAdj(unit, arg1, arg2);
        return 0;
    }
}

int unitCmdFloat(ObjectTask *unit, int arg1, int arg2)
{
    Actor *work = unit->work;
    Actor *motionActor = ((UnitRecord *)unit)->motionActor;
    Vector4 *cmdTgt = &work->cmdTarget;
    int var19;
    int m0;
    int m3;
    float y;
    float var_f1;
    int var5_2;

    switch (work->movePhase) {
    case 0:
        if (arg1 == 0xB) {
            m0 = 0xD;
            var19 = 0;
        } else {
            m0 = 0x12;
            var19 = 1;
        }
        unitMotSet(unit, m0);
        work->movePhase++;
        unitCmdTurnSubInit((UnitRecord *)unit, var19);
        /* fallthrough */
    case 1:
        unitCmdTurnSub(unit, 1);
        if (((UnitRecord *)unit)->motionActor->moveFlags & 0x1000) {
            motionActor->position.x += motionActor->floatStepX;
            motionActor->position.z += motionActor->floatStepZ;
            if (arg1 == 0xB) {
                y = motionActor->position.y;
                var_f1 = 1.3f;
                var5_2 = 0xE;
                motionActor->position.y = y - var_f1;
            } else {
                y = motionActor->position.y;
                var5_2 = 0x13;
                var_f1 = 0.8f;
                motionActor->position.y = y - var_f1;
            }
            unitMotSet(unit, var5_2);
            ((UnitRecord *)unit)->motionActor->moveFlags &= ~8;
            ((UnitRecord *)unit)->motionActor->moveStepRate = (1.0f / (unitCmdLenGet(unit) / 10.0f)) * 0.033333335f;
            unitCmdVASet(unit, arg1, 0);
            work->movePhase++;
        }
        break;
    case 2:
        if (arg1 == 0xB) {
            unitCmdDirAdj(unit, arg1, arg2);
        }
        if (calcSpline(D_00A57268, (float *)&motionActor->position, 3) != 0) {
            work->movePhase++;
        }
        break;
    case 3:
        m3 = 0x14;
        if (arg1 == 0xB) {
            m3 = 0xF;
        }
        unitMotSet(unit, m3);
        ((UnitRecord *)unit)->motionActor->moveStepRate = 0.033333335f;
        motionActor->position.y = 0.0f;
        motionActor->position.x += motionActor->floatStepX;
        work->movePhase++;
        motionActor->position.z += motionActor->floatStepZ;
        break;
    default:
        if (((UnitRecord *)unit)->motionActor->moveFlags & 0x1000) {
            motionActor->position.x = cmdTgt->x;
            motionActor->position.z = cmdTgt->z;
            unitSetInterpTime(unit, 0.26666668f);
            return 1;
        }
        break;
    }
    return 0;
}

int unitCmdWarp(ObjectTask *unit, int cmdId, int flag)
{
    Actor *actor = unit->work;
    Actor *motionActor = ((UnitRecord *)unit)->motionActor;
    Vector4 *target = &actor->cmdTarget;

    switch (actor->movePhase) {
    case 0:
        unitMotStandSet(unit);
        actor->movePhaseTimer = 0;
        actor->movePhase++;
        break;
    case 1:
        if (unitTelOut(unit) != 0) {
            actor->movePhase++;
        }
        break;
    case 2:
        actor->movePhase = 3;
        motionActor->position.x = target->x;
        actor->movePhaseTimer = 0;
        motionActor->position.y = target->y;
        motionActor->position.z = target->z;
        break;
    default:
        if (unitTelIn(unit) == 0) {
            break;
        }
        return 1;
    }
    return 0;
}

/*
 * curCmdSet's queued command (src/ov01/battle_init.c, this TU's own
 * ObjectCommandEntry-shaped record via unitCmdPtrGet) stores either a raw
 * coordinate pair at +4/+0xC or a position-table index at +4, selected by
 * mode; ov01/tu001 obj.c owns the record and types both as int, so the float
 * reads below reinterpret the same bytes it does not model as float.
 */
#define CMD_ENTRY_X_OR_IDX_OFF 0x4
#define CMD_ENTRY_Z_OFF 0xC

void unitCmdDstSet(ObjectTask *unit, int cmdId, int mode)
{
    Actor *actor;
    Vector4 *target;
    void *cmdEntry;
    int idx;

    cmdEntry = unitCmdPtrGet(unit);
    actor = unit->work;
    target = &actor->cmdTarget;

    if (mode == 0) {
        ObjectTask *targetUnit = actor->targetUnit;

        idx = *(short *)((char *)calcUPGet(targetUnit) + CALC_UNIT_PARAM_UNK44_OFF);
        target->x = dataPosTblGet(idx)->x;
        target->z = dataPosTblGet(idx)->z;
        if (cmdId != 0) {
            target->x -= unitColiGet(unit, targetUnit);
        }
        if (idx < 0) {
            target->x = -target->x;
        }
    } else if (mode == 1) {
        idx = *(int *)((char *)cmdEntry + CMD_ENTRY_X_OR_IDX_OFF);
        target->x = dataPosTblGet(idx)->x;
        target->z = dataPosTblGet(idx)->z;
        if (idx < 0) {
            target->x = -target->x;
        }
    } else {
        target->x = *(float *)((char *)cmdEntry + CMD_ENTRY_X_OR_IDX_OFF);
        target->z = *(float *)((char *)cmdEntry + CMD_ENTRY_Z_OFF);
    }
    target->y = 0.0f;
}

float unitCmdDirSet(UnitRecord *unit)
{
    float zero = 0.0f;
    Actor *self = unit->task.work;
    Vector4 *target = &self->cmdTarget;
    Actor *motionActor = unit->motionActor;
    float dx = target->x - motionActor->position.x;

    if (dx != zero || (target->z - motionActor->position.z) != zero) {
        motionActor->rotation.y = atan2f(dx, target->z - motionActor->position.z);
        if (motionActor->rotation.y < zero) {
            motionActor->rotation.y += 6.2831855f;
        }
    }
    return motionActor->rotation.y;
}

float unitCmdLenGet(void *arg0) {
    char *a = *(char **)((char *)arg0 + 0x10);
    char *b = *(char **)((char *)arg0 + 0x14);
    float *p = (float *)(a + 0x90);
    float dx = p[0] - *(float *)(b + 0x10);
    float dz;
    if (dx < 0.0f) dx = -dx;
    dz = p[2] - *(float *)(b + 0x18);
    if (dz < 0.0f) dz = -dz;
    return __builtin_sqrtf(dx * dx + dz * dz);
}

void unitCmdDivSet(UnitRecord *unit, float distance, float step)
{
    Actor *self = unit->task.work;
    Vector4 *target = &self->cmdTarget;
    float ratio = step / distance;
    Actor *motionActor = unit->motionActor;

    target->x = motionActor->position.x + (target->x - motionActor->position.x) * ratio;
    target->z = motionActor->position.z + (target->z - motionActor->position.z) * ratio;
}

extern void calcSplineInit(void *a, void *b, int c, int d);

void unitCmdVASet(ObjectTask *unit, s32 arg1, s32 arg2)
{
  float q = 0.25f;
  Actor *act = ((UnitRecord *) unit)->motionActor;
  float *tgt = (float *) (((u8 *) unit->work) + 0x90);
  float *p1 = (float *) (((u8 *) unit->work) + 0xD0);
  float *p2 = (float *) (((u8 *) unit->work) + 0xD8);
  float len = unitCmdLenGet(unit) / 0.4f;
  s32 n = (s32) len;
  unitSetInterpTime(unit, (((f32) n) * q) * 0.033333335f);
  *p1 = len;
  *p2 = len;
  if (((u32) (arg1 - 5)) < 2U)
  {
    float *arr = D_00A57280;
    arr[0] = act->position.x;
    arr[1] = act->position.y;
    arr[2] = act->position.z;
    arr[4] = act->position.x + ((tgt[0] - act->position.x) * 0.125f);
    arr[5] = act->position.y + ((tgt[1] - act->position.y) * 0.125f);
    arr[6] = act->position.z + ((tgt[2] - act->position.z) * 0.125f);
    arr[8] = act->position.x + ((tgt[0] - act->position.x) * 0.5f);
    arr[9] = act->position.y + ((tgt[1] - act->position.y) * 0.5f);
    arr[10] = act->position.z + ((tgt[2] - act->position.z) * 0.5f);
    arr[12] = act->position.x + (((tgt[0] - act->position.x) * 0.125f) * 7.0f);
    arr[13] = act->position.y + (((tgt[1] - act->position.y) * 0.125f) * 7.0f);
    arr[14] = act->position.z + (((tgt[2] - act->position.z) * 0.125f) * 7.0f);
    arr[16] = tgt[0];
    arr[17] = tgt[1];
    arr[18] = tgt[2];
    calcSplineInit(D_00A57268, arr, 5, n);
    goto after_switch;
  }
  if (arg1 == 11)
  {
    tgt[1] = act->position.y;
    D_00A57280[0] = act->position.x;
    D_00A57280[1] = act->position.y;
    D_00A57280[2] = act->position.z;
    D_00A57280[4] = act->position.x + ((tgt[0] - act->position.x) * q);
    D_00A57280[5] = (act->position.y + ((tgt[1] - act->position.y) * q)) - 0.125f;
    D_00A57280[6] = act->position.z + ((tgt[2] - act->position.z) * q);
    D_00A57280[8] = act->position.x + ((tgt[0] - act->position.x) * 0.5f);
    D_00A57280[9] = (act->position.y + ((tgt[1] - act->position.y) * 0.5f)) - q;
    D_00A57280[10] = act->position.z + ((tgt[2] - act->position.z) * 0.5f);
    D_00A57280[12] = act->position.x + (((tgt[0] - act->position.x) * q) * 3.0f);
    D_00A57280[13] = (act->position.y + (((tgt[1] - act->position.y) * q) * 3.0f)) - 0.125f;
    D_00A57280[14] = act->position.z + (((tgt[2] - act->position.z) * q) * 3.0f);
    D_00A57280[16] = tgt[0];
    D_00A57280[17] = tgt[1];
    D_00A57280[18] = tgt[2];
    calcSplineInit(D_00A57268, D_00A57280, 5, n);
  }
  else
    if (arg1 == 12)
  {
    D_00A57280[0] = act->position.x;
    D_00A57280[1] = act->position.y;
    D_00A57280[2] = act->position.z;
    D_00A57280[4] = act->position.x + ((tgt[0] - act->position.x) * q);
    D_00A57280[5] = (act->position.y + ((tgt[1] - act->position.y) * q)) - 0.125f;
    D_00A57280[6] = act->position.z + ((tgt[2] - act->position.z) * q);
    D_00A57280[8] = act->position.x + ((tgt[0] - act->position.x) * 0.5f);
    D_00A57280[9] = (act->position.y + ((tgt[1] - act->position.y) * 0.5f)) - q;
    D_00A57280[10] = act->position.z + ((tgt[2] - act->position.z) * 0.5f);
    D_00A57280[12] = act->position.x + (((tgt[0] - act->position.x) * q) * 3.0f);
    D_00A57280[13] = (act->position.y + (((tgt[1] - act->position.y) * q) * 3.0f)) - 0.125f;
    D_00A57280[14] = act->position.z + (((tgt[2] - act->position.z) * q) * 3.0f);
    D_00A57280[16] = tgt[0];
    D_00A57280[17] = tgt[1];
    D_00A57280[18] = tgt[2];
    calcSplineInit(D_00A57268, D_00A57280, 5, n);
  }
  else
  {
    float az = act->position.z;
    float ax = act->position.x;
    float t0 = tgt[0];
    act->velocity.x = (t0 - ax) / len;
    act->velocity.z = (tgt[2] - az) / len;
    act->acceleration.x = 0.0f;
    act->acceleration.z = 0.0f;
  }
  after_switch:
  if (((u32) (arg1 - 3)) < 2U)
  {
    act->acceleration.y = -0.0204375f;
    act->velocity.y = (len * 0.0204375f) * 0.5f;
  }
  else
  {
    *((int *) (&act->acceleration.y)) = 0;
    *((int *) (&act->velocity.y)) = 0;
  }

}

#define UNIT_CMD_MOVE_AXIS(FAST, P, V, A, T, N, INC)                  \
    if (FAST) {                                                        \
        (V) = (V) + (A);                                               \
        (P) = (P) + (V);                                               \
    } else {                                                           \
        if ((P) != (T)) {                                              \
            float vn_ = (V) + (A);                                     \
            float pn_;                                                 \
            pn_ = (P) + vn_;                                           \
            (V) = vn_;                                                 \
            (P) = pn_;                                                 \
            if ((vn_ > 0.0f && (T) < pn_) || (vn_ < 0.0f && pn_ < (T)) \
                || (((T) - pn_) < 0.0f ? (-1.0f * ((T) - pn_)) <= 0.000001f : ((T) - pn_) <= 0.000001f)) { \
                INC;                                                   \
                (P) = (T);                                             \
            }                                                          \
        } else {                                                       \
            INC;                                                       \
        }                                                              \
    }

s32 unitCmdMoveCalc(void *arg0, s32 arg1, s32 arg2, s32 arg3) {
    char *work;
    float *motion;
    float *tg;
    float *cnt;
    s32 n;

    work = *(char **)((char *)arg0 + 0x10);
    motion = *(float **)((char *)arg0 + 0x14);
    tg = (float *)(work + 0x90);
    cnt = (float *)(work + 0xD0);
    n = 0;
    UNIT_CMD_MOVE_AXIS(arg1 == 1 && *cnt >= 1.0f, motion[4], motion[12], motion[16], tg[0], n, n = 1)
    UNIT_CMD_MOVE_AXIS(arg2 == 1 && *cnt >= 1.0f, motion[5], motion[13], motion[17], tg[1], n, n += 1)
    UNIT_CMD_MOVE_AXIS(arg3 == 1 && *cnt >= 1.0f, motion[6], motion[14], motion[18], tg[2], n, n += 1)
    *cnt = *cnt - 1.0f;
    return n == 3 ? 1 : 0;
}

int unitTelIn(ObjectTask *unit)
{
    Actor *actor = unit->work;
    Actor *m = ((UnitRecord *)unit)->motionActor;

    if (actor->movePhaseTimer == 0) {
        *(Actor **)&m->transparency = 0;
        ((UnitRecord *)unit)->motionActor->flags &= ~8;
        actor->movePhaseTimer++;
    } else {
        m->transparency += m->posSnapX;
        if (m->transparency >= 1.0f) {
            m->transparency = 1.0f;
            return 1;
        }
    }
    return 0;
}

int unitTelOut(ObjectTask *unit)
{
    Actor *actor = unit->work;
    Actor *m = ((UnitRecord *)unit)->motionActor;

    if (actor->movePhaseTimer == 0) {
        actor->movePhaseTimer = 1;
    } else {
        m->transparency -= m->posSnapX;
        if (m->transparency <= 0.0f) {
            ((union { float f; Actor *p; } *)&m->transparency)->f = 0.0f;
            ((UnitRecord *)unit)->motionActor->flags |= 8;
            return 1;
        }
    }
    return 0;
}

int unitCmdBtst(ObjectTask *unit)
{
    Actor *actor;
    int *p;

    actor = unit->work;
    p = unitCmdPtrGet(unit);
    if (actor->moveState == 0) {
        if (p[1] > 0) {
            p[1]--;
        } else {
            actor->moveState = 1;
        }
        goto end;
    }
    if (actor->moveState == 1) {
        if (((Actor *)unit->work)->flags & 0x40) {
            goto stand;
        }
        if (calcUPGet(unit)->flags & 0x40) {
            goto stand;
        }
        if (calcUPGet(unit)->charaId < 0xBB) {
            goto set;
        }
        if (calcUPGet(unit)->charaId < 0xC3) {
            goto stand;
        }
set:
        unitMotSet(unit, 0x23);
        unitWpnMotSet(unit, 0, 0, 0x2F);
        goto mt1;
stand:
        unitMotStandSet(unit);
mt1:
        unitMtdProc((UnitRecord *)unit);
        actor->movePhaseTimer = 0;
        goto inc;
    }
    if (actor->moveState != 2) {
        goto dflt;
    }
    unitMtdProc((UnitRecord *)unit);
    if (unitTelIn(unit) == 0) {
        goto end;
    }
inc:
    actor->moveState = actor->moveState + 1;
    goto end;
dflt:
    if (((Actor *)unit->work)->flags & 0x40) {
        goto nxt;
    }
    if (calcUPGet(unit)->flags & 0x40) {
        goto nxt;
    }
    if (calcUPGet(unit)->charaId < 0xBB) {
        goto mtd;
    }
    if (calcUPGet(unit)->charaId < 0xC3) {
        goto nxt;
    }
mtd:
    unitMtdProc((UnitRecord *)unit);
    if (!(((UnitRecord *)unit)->motionActor->moveFlags & 0x1000)) {
        goto end;
    }
    unitMotStandSet(unit);
    unitWpnMotSet(unit, 0, 0, 0xFF);
    return unitCmdNext(unit);
nxt:
    return unitCmdNext(unit);
end:
    return 0;
}

extern void unitMtdProc(UnitRecord *unit);
extern int unitCmdNext(ObjectTask *unit);
extern void statEffOff(ObjectTask *unit);

int unitCmdBted(ObjectTask *unit)
{
    Actor *actor;

    actor = unit->work;
    unitCmdPtrGet(unit);
    if (actor->moveState == 0) {
        if (!(((Actor *)unit->work)->flags & 0x40) && !(calcUPGet(unit)->flags & 0x40)) {
            if (calcUPGet(unit)->charaId >= 0xBB) {
                if (calcUPGet(unit)->charaId >= 0xC3) {
                    goto block_6;
                }
            } else {
block_6:
                unitMotSet(unit, 0x24);
                unitWpnMotSet(unit, 0, 0, 0x30);
                unitFaceMotSet(unit, 0x39);
            }
        }
        unitMtdProc((UnitRecord *)unit);
        statEffOff(unit);
        actor->moveState++;
    } else {
        unitMtdProc((UnitRecord *)unit);
        if (((UnitRecord *)unit)->motionActor->moveFlags & 0x1000) {
            if (calcUPGet(unit)->flags & 0x40) {
                unitWpnMotSet(unit, 0, 0, 5);
            } else if (!(((Actor *)unit->work)->flags & 0x40) && !(calcUPGet(unit)->flags & 0x40)) {
                if ((calcUPGet(unit)->charaId < 0xBB) || (calcUPGet(unit)->charaId >= 0xC3)) {
                    unitWpnMotSet(unit, 0, 0, 0xFF);
                }
            }
            return unitCmdNext(unit);
        }
    }
    return 0;
}

void unitMtdProc(UnitRecord *unit)
{
    int defaultWeapon;

    defaultWeapon = dataDefWpnGet(unit);
    if (dataMtdRead(unit, 2) == 1) {
        transWepIn(unit, defaultWeapon);
    }
    if (dataMtdRead(unit, 3) == 1) {
        transWepOut(unit, defaultWeapon);
    }
    if (dataMtdRead(unit, 6) == 1) {
        transWepIn(unit, defaultWeapon);
    }
    if (dataMtdRead(unit, 6) == 1) {
        transWepIn(unit, 1);
    }
    if (dataMtdRead(unit, 7) != 1) {
        return;
    }
    transWepOut(unit, 1);
}

/*
 * unitCmdRec/unitCmdStat both read a flag word of the queued command entry
 * (unitCmdPtrGet; unitCmdDstSet's own cmdEntry above reads its own command's
 * first argument at this same offset): nonzero skips playing the stand
 * motion below.
 */
typedef struct {
    unsigned char unmodeled_00[0x4];
    int flag; /* +0x4 */
} UnitCmdRecEntry;

void unitCmdRec(ObjectTask *unit)
{
    Actor *actor;
    UnitCmdRecEntry *cmd;

    actor = unit->work;
    cmd = unitCmdPtrGet(unit);
    if (actor->moveState == 0) {
        if (cmd->flag == 0) {
            unitMotStandSet(unit);
        }
        actor->moveState++;
    }
    unitCmdNext(unit);
}

extern void sndSeTransPlay(int unit, int seId, int volume);

int unitCmdWake(ObjectTask *unit)
{
    Actor *actor;

    actor = unit->work;
    unitCmdPtrGet(unit);
    if (actor->moveState == 0) {
        unitMotSet(unit, 0x25);
        sndSeTransPlay((int)unit, 2, 5);
        actor->moveState++;
    } else if ((*(Actor **)((char *)unit + 0x14))->moveFlags & 0x1000) {
        actor->flags &= ~0x80;
        unitMotStandSet(unit);
        return unitCmdNext(unit);
    }
    return 0;
}

void unitCmdStat(ObjectTask *unit)
{
    Actor *actor;
    UnitCmdRecEntry *cmd;

    actor = unit->work;
    cmd = unitCmdPtrGet(unit);
    if (actor->moveState == 0) {
        if (cmd->flag == 0) {
            unitMotStandSet(unit);
        }
        actor->moveState++;
    }
    unitCmdNext(unit);
}

extern int dataCdSync(void);
extern void dataUnitFileLoad2Sub(void *unit);

int unitCmdReadWait(ObjectTask *unit)
{
    Actor *actor;
    UnitCmdRecEntry *cmd;

    actor = unit->work;
    cmd = unitCmdPtrGet(unit);
    if (actor->moveState == 0) {
        if (cmd->flag >= dataCdSync()) {
            dataUnitFileLoad2Sub(unit);
            actor->moveState++;
        }
    } else {
        return unitCmdNext(unit);
    }
    return 0;
}

extern void decoEffCall(ObjectTask *unit, int effectId);

int unitCmdAgws2(ObjectTask *unit)
{
    Actor *actor;

    actor = unit->work;
    unitCmdPtrGet(unit);
    if (actor->moveState == 0) {
        decoEffCall(unit, (calcUPGet(unit)->flags & 0x40) ? 0x67 : 0x66);
        actor->moveState++;
    } else if (actor->moveState == 1) {
        unitMotStandSet(unit);
        actor->movePhaseTimer = 0;
        actor->moveState++;
    } else if (unitTelIn(unit)) {
        return unitCmdNext(unit);
    }
    return 0;
}

int unitCmdEscape(ObjectTask *unit)
{
    Actor *actor;

    actor = unit->work;
    switch (actor->moveState) {
    case 0:
        actor->moveState = 1;
        break;
    case 1:
        if (unitTelOut(unit) != 0) {
            actor->moveState++;
        }
        break;
    case 2:
        ((union { int i; void *p; } *)actor)->i = (actor->flags & 0x61) | 0x80;
        if (((Actor *)unit->work)->flags & 0x40) {
            unitTblRemove(unit);
            unitRemove((UnitRecord *)unit, 0);
        }
        break;
    }
    return 0;
}

int unitCmdFadeIn(ObjectTask *unit)
{
    Actor *actor;
    void *eff;

    actor = unit->work;
    switch (actor->moveState) {
    case 0:
        actor->moveState = 1;
        actor->movePhaseTimer = 0;
        break;
    case 1:
        if (unitTelIn(unit) != 0) {
            actor->moveState++;
        }
        break;
    case 2:
        eff = *(void **)((char *)calcUPGet(unit) + 0x158);
        if (eff != 0) {
            *(int *)((char *)eff + 0xA8C) &= ~0x100;
        }
        statEffOn(unit);
        return unitCmdNext(unit);
    }
    return 0;
}

int unitCmdFadeOut(ObjectTask *unit)
{
    Actor *actor;
    void *eff;

    actor = unit->work;
    switch (actor->moveState) {
    case 0:
        eff = *(void **)((char *)calcUPGet(unit) + 0x158);
        if (eff != 0) {
            *(int *)((char *)eff + 0xA8C) |= 0x100;
        }
        statEffOff(unit);
        actor->movePhaseTimer = 0;
        actor->moveState++;
        break;
    case 1:
        if (unitTelOut(unit) != 0) {
            actor->moveState++;
        }
        break;
    case 2:
        return unitCmdNext(unit);
    }
    return 0;
}

/*
 * unitMotSetEx is defined later in this TU, still asm; forward-declared for
 * the tail call unitMotSet makes to it, always with a zero third argument.
 */
extern void unitMotSetEx(ObjectTask *unit, int motion, int);

void unitMotSet(ObjectTask *unit, int motion)
{
    unitMotSetEx(unit, motion, 0);
}

extern int ACT_setMotion2(Actor *actor, int motion, int flags);

void unitMotSetEx(ObjectTask *unit, int motion, int arg2)
{
    Actor *actor;
    int flags;
    unsigned int m;
    int *eff;

    actor = ((UnitRecord *)unit)->motionActor;
    if (ACT_animGetData(actor, (arg2 << 8) + motion - 1) != 0) {
        if (actor->moveFlags & 0x80000) {
            if ((motion != 6) && (motion != 0x1C)) {
                actor->moveFlags |= 0x1000;
                return;
            }
            actor->moveStepRate = 0.033333335f;
            actor->moveFlags &= 0xFFF7FFFF;
        }
        actor->motionFrame = 0;
        flags = *(unsigned int *)((char *)unit->work + 0x100) & 1;
        if (arg2 == 0) {
            if (((unsigned int)(motion - 1) < 2U) || (motion == 0x1D) || (motion == 0x17) || (motion == 0x20) || (motion == 0xE) || (motion == 0x13)) {
                flags |= 8;
            }
            m = motion - 1;
            if ((motion == 2) || (unitMotGet(unit) == 2)) {
                flags |= 1;
                unitSetInterpTime(unit, 0.26666668f);
            }
            if ((unsigned int)(motion - 4) < 2U || motion == 0x1E || motion == 0x1A) {
                flags |= 0x20000000;
            }
            eff = *(int **)((char *)calcUPGet(unit) + 0x158);
            if (m < 2U) {
                if (eff != 0) {
                    *(int *)((char *)eff + 0xA8C) &= ~0x100;
                }
            } else if (eff != 0) {
                *(int *)((char *)eff + 0xA8C) |= 0x100;
            }
        }
        if (calcUPGet(unit)->charaId == 0xB4) {
            flags |= 0xC0000000;
        }
        printf("** MotSet: Mot=%d Pack=%d flag=%X (%X) **\n", motion, arg2, flags, unit);
        if ((flags & 1) != 0) {
            printf("** Intr=%f\n", actor->interpTime);
        }
        actor->moveFlags &= 0xDFFFFFF6;
        ACT_setMotion2(actor, (arg2 << 8) + motion - 1, flags);
        actor->motion = motion - 1;
        *(int *)((char *)unit->work + 0x100) = 0;
    }
}

/*
 * unitSetInterp enables interpolation on the actor object owned by a unit.
 * The surrounding Unit layout is not established here; the two scalar
 * offsets below are the members directly demonstrated by this function and
 * its callers.
 */
void unitSetInterp(void *unit)
{
    void *object = *(void **)((unsigned char *)unit + 0x10);
    *(int *)((unsigned char *)object + 0x100) = 1;
}

void unitSetInterpTime(ObjectTask *unit, float time)
{
    ((UnitRecord *)unit)->motionActor->interpTime = time;
}

void unitWpnMotSet(ObjectTask *unit, int weaponIndex, int animGroup, int motion)
{
    Actor *actor;
    unsigned int dataId;

    if (weaponIndex >= 0) {
        actor = ((UnitRecord *)unit)->wpnActor[weaponIndex & 3];
        if (actor != 0) {
            dataId = (animGroup << 8) + motion - 1;
            if (ACT_animGetData(actor, dataId) != 0 || motion == 0xFF) {
                ACT_setMotion(actor, dataId);
            }
        }
    }
}

void unitFaceMotSet(ObjectTask *unit, int motion)
{
    Actor *actor = *(Actor **)((char *)unit + 0x2C);
    int flags = 0;

    if ((unsigned int)(motion - 0x3A) >= 2U) {
        flags = 8;
    }
    if (actor != 0 && ACT_animGetData(actor, motion - 1) != 0) {
        ACT_setMotion2(actor, motion - 1, flags);
    }
}

int unitMotGet(ObjectTask *unit)
{
    return ((UnitRecord *)unit)->motionActor->motion + 1;
}

void unitMotStandSet(ObjectTask *unit)
{
    int motion;

    if (*(int *)unit->work & 0x800) {
        motion = 0x1D;
        goto done;
    }
    if (calcIdol2Chk(unit) == 0) goto one;
    if (!(*(int *)unit->work & 8)) goto two;
    if (unitMotGet(unit) == 0x1C) goto two;
    if (unitMotGet(unit) != 0x23) goto one;
two:
    motion = 2;
    goto done;
one:
    motion = 1;
done:
    unitMotSet(unit, motion);
}

int unitNoGet(ObjectTask *unit)
{
    ObjectTask **units = (void *)unitTbl;
    int i;

    for (i = 0; i < 8; i++) {
        if (units[i] == unit) {
            return i;
        }
    }
    {

        printf("** unitNoGet: err -> %X\n", unit);
    }
    return -1;
}

extern int printf(const char *format, ...);

/*
 * unitTbl (declared above) is read here as a table of eight 4-byte slots;
 * unitPtrGet is the only claimed reader of it besides unitInit's own clear.
 */
int unitPtrGet(int index)
{
    if (index < 8) {
        return ((int *)unitTbl)[index];
    }
    printf("** unitPtrGet: err -> %d\n", index);
    return 0;
}

/*
 * unitDispOn's own sibling, still asm in this TU; this TU's own UnitRecord
 * view of unitTblGet's table (already published identically as BattleUnit
 * by src/ov01/battle_init.c and src/ov01/debug_entry.c, both ov01/tu003).
 */
extern int unitTblGet(int side, UnitRecord ***table);

void unitDispOn(int side)
{
    UnitRecord **table;
    int count;
    UnitRecord **cur;
    int remaining;
    UnitRecord *unit;

    count = unitTblGet(side, &table);
    if (count > 0) {
        cur = table;
        remaining = count;
        do {
            unit = *cur;
            cur++;
            if (unit != 0) {
                /* Clears the motion actor's hidden-draw flag (bit 0x8 of
                 * Actor.flags); no other claimed function here touches it. */
                unit->motionActor->flags &= ~8;
            }
            remaining--;
        } while (remaining != 0);
    }
}

void unitDispOff(int side)
{
    UnitRecord **table;
    int count;
    UnitRecord **cur;
    int remaining;
    UnitRecord *unit;

    count = unitTblGet(side, &table);
    if (count > 0) {
        cur = table;
        remaining = count;
        do {
            unit = *cur;
            cur++;
            if (unit != 0) {
                unit->motionActor->flags |= 8;
            }
        } while (--remaining != 0);
    }
}

float unitColiGet(ObjectTask *unit, ObjectTask *targetUnit)
{
    float a = calcUPGet(unit)->coliMove;
    return (a + calcUPGet(targetUnit)->coliTarget) / 100.0f;
}

int unitAgwsChk(int side)
{
    UnitRecord **table;
    int count;
    int i;
    int n;

    n = 0;
    i = 0;
    count = unitTblGet(side, &table);
    if (count > 0) {
        do {
            UnitRecord *unit = table[i];
            i++;
            if (unit != 0) {
                if (calcUPGet((ObjectTask *)unit)->flags & 0x40) {
                    n++;
                }
            }
        } while (i < count);
    }
    return n != 0;
}

int unitIdChk(int side, int id)
{
    ObjectTask **table;
    int count;
    int i = 0;
    int hit = 0;

    count = unitTblGet(side, (UnitRecord ***)&table);
    while (i < count) {
        ObjectTask *u = table[i];
        i++;
        if (u != 0) {
            if (calcUPGet(u)->charaId == id)
                hit++;
        }
    }
    return hit != 0;
}

/* Defined later in this TU (a local sibling still in asm). */
extern void unitDispOnOff(int flag);

void unitDispOnAll(void)
{
    unitDispOnOff(0);
}

void unitDispOffAll(void)
{
    unitDispOnOff(1);
}

extern void statEffOn(ObjectTask *unit);
extern void statEffOff(ObjectTask *unit);

/*
 * calcUPGet(unit)+0x158, immediately before ov01/tu004 calc.c's own
 * published statEffUnits[8] table at +0x15C, holds a pointer to the same
 * kind of per-unit status-effect record statEffOn/statEffOff walk through
 * that table (src/ov01/battle_init.c, ov01/tu003's own published UnitWork,
 * include/ov01/battle_init.h). This TU does not own that record and cannot
 * complete a second UnitWork of its own, so only the touched flag word is
 * reached through a raw offset here.
 */
#define CALC_UNIT_PARAM_STAT_EFF_TARGET_OFF 0x158
#define UNIT_WORK_FLAGS_OFF 0xA8C
#define UNIT_WORK_STATUS_BLOCKED 0x100u

void unitDispOnOff(int flag)
{
    UnitRecord **table;
    int index;
    int count;
    UnitRecord *unit;
    void *statEffTarget;

    index = 0;
    count = unitTblGet(2, &table);
    if (count > 0) {
        do {
            unit = table[index];
            if (unit != 0) {
                if (flag == 0) {
                    unit->motionActor->flags &= ~8;
                    statEffTarget = *(void **)((char *)calcUPGet((ObjectTask *) unit) + CALC_UNIT_PARAM_STAT_EFF_TARGET_OFF);
                    if (statEffTarget != 0) {
                        *(unsigned int *)((char *)statEffTarget + UNIT_WORK_FLAGS_OFF) &= ~UNIT_WORK_STATUS_BLOCKED;
                    }
                    statEffOn((ObjectTask *) table[index]);
                } else {
                    unit->motionActor->flags |= 8;
                    statEffTarget = *(void **)((char *)calcUPGet((ObjectTask *) unit) + CALC_UNIT_PARAM_STAT_EFF_TARGET_OFF);
                    if (statEffTarget != 0) {
                        *(unsigned int *)((char *)statEffTarget + UNIT_WORK_FLAGS_OFF) |= UNIT_WORK_STATUS_BLOCKED;
                    }
                    statEffOff((ObjectTask *) table[index]);
                }
            }
            index++;
        } while (index < count);
    }
}

/* Command dispatch table and per-character position adjustment curves. */
int (*unitCmdTbl[49])(ObjectTask *, int) = {
    (int (*)(ObjectTask *, int))unitCmdFree,
    (int (*)(ObjectTask *, int))unitCmdAtk,
    (int (*)(ObjectTask *, int))unitCmdEther,
    (int (*)(ObjectTask *, int))unitCmdItem,
    (int (*)(ObjectTask *, int))unitCmdGuard,
    (int (*)(ObjectTask *, int))unitCmdAgws,
    (int (*)(ObjectTask *, int))unitCmdMove,
    (int (*)(ObjectTask *, int))unitCmdStand,
    (int (*)(ObjectTask *, int))unitCmdEnd,
    (int (*)(ObjectTask *, int))unitCmdDmg,
    (int (*)(ObjectTask *, int))unitCmdEva,
    (int (*)(ObjectTask *, int))unitCmdDef,
    (int (*)(ObjectTask *, int))unitCmdDead,
    (int (*)(ObjectTask *, int))unitCmdMot,
    (int (*)(ObjectTask *, int))unitCmdTgtSet,
    (int (*)(ObjectTask *, int))unitCmdTurn,
    (int (*)(ObjectTask *, int))unitCmdTurn,
    (int (*)(ObjectTask *, int))unitCmdMoveCha,
    (int (*)(ObjectTask *, int))unitCmdMovePos,
    (int (*)(ObjectTask *, int))unitCmdMoveChaNorm,
    (int (*)(ObjectTask *, int))unitCmdMovePosNorm,
    (int (*)(ObjectTask *, int))unitCmdMoveChaRun,
    (int (*)(ObjectTask *, int))unitCmdMovePosRun,
    (int (*)(ObjectTask *, int))unitCmdMoveChaJump,
    (int (*)(ObjectTask *, int))unitCmdMovePosJump,
    (int (*)(ObjectTask *, int))unitCmdMoveChaHover,
    (int (*)(ObjectTask *, int))unitCmdMovePosHover,
    (int (*)(ObjectTask *, int))unitCmdMoveChaWarp,
    (int (*)(ObjectTask *, int))unitCmdMovePosWarp,
    (int (*)(ObjectTask *, int))unitCmdMoveChaJump2,
    (int (*)(ObjectTask *, int))unitCmdMovePosJump,
    (int (*)(ObjectTask *, int))unitCmdMoveChaHover2,
    (int (*)(ObjectTask *, int))unitCmdMovePosHover2,
    (int (*)(ObjectTask *, int))unitCmdMoveChaFloat,
    (int (*)(ObjectTask *, int))unitCmdMovePosFloat,
    (int (*)(ObjectTask *, int))unitCmdMoveChaHover3,
    (int (*)(ObjectTask *, int))unitCmdMovePosHover3,
    (int (*)(ObjectTask *, int))unitCmdBtst,
    (int (*)(ObjectTask *, int))unitCmdBted,
    (int (*)(ObjectTask *, int))unitCmdRec,
    (int (*)(ObjectTask *, int))unitCmdWake,
    (int (*)(ObjectTask *, int))unitCmdStat,
    (int (*)(ObjectTask *, int))unitCmdReadWait,
    (int (*)(ObjectTask *, int))unitCmdUnitChange,
    (int (*)(ObjectTask *, int))unitCmdTurn,
    (int (*)(ObjectTask *, int))unitCmdAgws2,
    (int (*)(ObjectTask *, int))unitCmdEscape,
    (int (*)(ObjectTask *, int))unitCmdFadeIn,
    (int (*)(ObjectTask *, int))unitCmdFadeOut
};
float chaosF1[5] = {-0.02f, -0.05f, 0.0f, 0.18f, 0.49f};
float chaosF5[9] = {0.21f, 0.38f, 0.51f, 0.61f, 0.69f, 0.74f, 0.77f, 0.79f, 0.8f};
float chaosB1[5] = {0.02f, 0.07f, 0.16f, 0.31f, 0.52f};
float chaosB5[9] = {0.3f, 0.52f, 0.64f, 0.71f, 0.75f, 0.78f, 0.79f, 0.8f, 0.8f};
float kosmosF1[5] = {0.0f, 0.03f, 0.09f, 0.21f, 0.44f};
float kosmosF5[9] = {0.22f, 0.39f, 0.53f, 0.63f, 0.7f, 0.75f, 0.78f, 0.79f, 0.8f};
float kosmosB1[5] = {0.01f, 0.06f, 0.16f, 0.34f, 0.57f};
float kosmosB5[9] = {0.25f, 0.45f, 0.59f, 0.67f, 0.72f, 0.76f, 0.78f, 0.79f, 0.8f};
float shionF1[5] = {0.01f, 0.03f, 0.07f, 0.14f, 0.26f};
float shionF5[9] = {0.2f, 0.37f, 0.53f, 0.63f, 0.69f, 0.73f, 0.76f, 0.79f, 0.8f};
float shionB1[5] = {0.03f, 0.1f, 0.2f, 0.32f, 0.45f};
float shionB5[9] = {0.21f, 0.35f, 0.47f, 0.57f, 0.65f, 0.71f, 0.75f, 0.78f, 0.8f};
float ziggyF1[5] = {0.01f, 0.03f, 0.08f, 0.15f, 0.26f};
float ziggyF5[9] = {0.18f, 0.34f, 0.5f, 0.6f, 0.67f, 0.72f, 0.75f, 0.78f, 0.8f};
float ziggyB1[5] = {0.01f, 0.04f, 0.09f, 0.18f, 0.3f};
float ziggyB5[9] = {0.17f, 0.33f, 0.48f, 0.6f, 0.7f, 0.76f, 0.79f, 0.8f, 0.8f};
float momoF1[5] = {0.02f, 0.07f, 0.17f, 0.33f, 0.5f};
float momoF5[9] = {0.21f, 0.39f, 0.52f, 0.62f, 0.7f, 0.75f, 0.77f, 0.79f, 0.8f};
float momoB1[5] = {0.04f, 0.13f, 0.26f, 0.41f, 0.58f};
float momoB5[9] = {0.21f, 0.38f, 0.51f, 0.62f, 0.69f, 0.74f, 0.77f, 0.79f, 0.8f};
float jrF1[5] = {0.01f, 0.06f, 0.13f, 0.24f, 0.56f};
float jrF5[9] = {0.26f, 0.44f, 0.56f, 0.64f, 0.69f, 0.73f, 0.76f, 0.78f, 0.8f};
float jrB1[5] = {0.04f, 0.14f, 0.28f, 0.45f, 0.63f};
float jrB5[9] = {0.21f, 0.37f, 0.48f, 0.56f, 0.63f, 0.69f, 0.74f, 0.77f, 0.79f};
float virgilF1[5] = {-0.08f, -0.05f, 0.08f, 0.27f, 0.52f};
float virgilF5[10] = {0.34f, 0.59f, 0.75f, 0.85f, 0.89f, 0.91f, 0.89f, 0.85f, 0.82f, 0.82f};
float virgilB1[5] = {-0.02f, -0.05f, 0.01f, 0.18f, 0.44f};
float virgilB5[10] = {0.32f, 0.52f, 0.63f, 0.69f, 0.74f, 0.77f, 0.79f, 0.8f, 0.8f, 0.8f};
float vx01F1[37] = {-0.01f, -0.02f, -0.05f, -0.09f, -0.13f, -0.18f, -0.23f, -0.28f, -0.34f, -0.39f, -0.44f, -0.49f, -0.54f, -0.58f, -0.61f, -0.64f, -0.66f, -0.67f, -0.68f, -0.69f, -0.69f, -0.69f, -0.69f, -0.69f, -0.69f, -0.68f, -0.66f, -0.64f, -0.6f, -0.54f, -0.47f, -0.38f, -0.26f, -0.1f, 0.09f, 0.33f, 0.63f};
float vx01F5[23] = {0.3f, 0.5f, 0.66f, 0.79f, 0.89f, 0.96f, 1.0f, 1.04f, 1.05f, 1.06f, 1.06f, 1.06f, 1.06f, 1.05f, 1.05f, 1.04f, 1.03f, 1.02f, 1.01f, 1.0f, 1.0f, 1.0f, 1.0f};
float vx01B1[27] = {0.0f, -0.02f, -0.03f, -0.06f, -0.08f, -0.11f, -0.14f, -0.17f, -0.19f, -0.21f, -0.22f, -0.24f, -0.25f, -0.25f, -0.26f, -0.26f, -0.26f, -0.26f, -0.26f, -0.26f, -0.23f, -0.19f, -0.11f, -0.01f, 0.18f, 0.39f, 0.66f};
float vx01B5[24] = {0.34f, 0.59f, 0.75f, 0.88f, 0.98f, 1.06f, 1.12f, 1.16f, 1.19f, 1.21f, 1.22f, 1.22f, 1.22f, 1.22f, 1.21f, 1.19f, 1.17f, 1.14f, 1.11f, 1.08f, 1.06f, 1.03f, 1.02f, 1.0f};
float vx02F1[13] = {0.01f, 0.01f, 0.03f, 0.04f, 0.07f, 0.1f, 0.14f, 0.19f, 0.26f, 0.33f, 0.43f, 0.55f, 0.73f};
float vx02F5[32] = {0.17f, 0.5f, 0.79f, 1.03f, 1.24f, 1.4f, 1.54f, 1.64f, 1.71f, 1.76f, 1.79f, 1.8f, 1.79f, 1.75f, 1.7f, 1.64f, 1.57f, 1.49f, 1.41f, 1.33f, 1.26f, 1.19f, 1.12f, 1.06f, 0.99f, 0.93f, 0.88f, 0.84f, 0.81f, 0.79f, 0.78f, 0.79f};
float vx02B1[13] = {-0.02f, -0.08f, -0.14f, -0.21f, -0.25f, -0.27f, -0.27f, -0.25f, -0.18f, -0.06f, 0.12f, 0.35f, 0.64f};
float vx02B5[32] = {0.17f, 0.46f, 0.71f, 0.92f, 1.08f, 1.22f, 1.31f, 1.39f, 1.44f, 1.47f, 1.48f, 1.49f, 1.49f, 1.49f, 1.47f, 1.44f, 1.41f, 1.37f, 1.32f, 1.27f, 1.22f, 1.17f, 1.11f, 1.06f, 1.01f, 0.96f, 0.92f, 0.88f, 0.85f, 0.83f, 0.81f, 0.8f};
float vx03F1[34] = {0.0f, -0.02f, -0.05f, -0.1f, -0.18f, -0.27f, -0.37f, -0.48f, -0.59f, -0.7f, -0.79f, -0.88f, -0.94f, -0.98f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -0.97f, -0.94f, -0.9f, -0.84f, -0.77f, -0.67f, -0.54f, -0.39f, -0.2f, 0.03f, 0.3f, 0.63f};
float vx03F5[29] = {0.32f, 0.5f, 0.61f, 0.7f, 0.77f, 0.84f, 0.89f, 0.92f, 0.95f, 0.97f, 0.99f, 0.99f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
float vx03B1[29] = {0.0f, -0.02f, -0.05f, -0.1f, -0.18f, -0.27f, -0.36f, -0.47f, -0.57f, -0.67f, -0.77f, -0.85f, -0.92f, -0.97f, -1.0f, -1.01f, -1.0f, -0.98f, -0.95f, -0.9f, -0.84f, -0.77f, -0.67f, -0.54f, -0.39f, -0.2f, 0.03f, 0.31f, 0.63f};
float vx03B5[29] = {0.37f, 0.67f, 0.91f, 1.1f, 1.25f, 1.36f, 1.43f, 1.47f, 1.49f, 1.5f, 1.5f, 1.5f, 1.5f, 1.48f, 1.44f, 1.37f, 1.29f, 1.21f, 1.13f, 1.06f, 1.02f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
float vx06F1[29] = {-0.03f, -0.1f, -0.2f, -0.32f, -0.47f, -0.63f, -0.8f, -0.97f, -1.14f, -1.3f, -1.42f, -1.48f, -1.5f, -1.5f, -1.5f, -1.49f, -1.47f, -1.43f, -1.37f, -1.3f, -1.2f, -1.07f, -0.92f, -0.74f, -0.54f, -0.3f, -0.03f, 0.28f, 0.62f};
float vx06F5[29] = {0.36f, 0.64f, 0.85f, 1.02f, 1.04f, 1.24f, 1.32f, 1.4f, 1.47f, 1.52f, 1.56f, 1.58f, 1.59f, 1.6f, 1.6f, 1.59f, 1.57f, 1.54f, 1.5f, 1.45f, 1.4f, 1.34f, 1.28f, 1.23f, 1.17f, 1.12f, 1.08f, 1.04f, 1.01f};
float vx06B1[29] = {-0.01f, -0.05f, -0.11f, -0.18f, -0.26f, -0.35f, -0.44f, -0.52f, -0.59f, -0.65f, -0.69f, -0.72f, -0.74f, -0.75f, -0.75f, -0.75f, -0.75f, -0.75f, -0.74f, -0.73f, -0.7f, -0.65f, -0.57f, -0.47f, -0.33f, -0.16f, 0.05f, 0.32f, 0.63f};
float vx06B5[29] = {0.36f, 0.65f, 0.88f, 1.05f, 1.17f, 1.26f, 1.32f, 1.35f, 1.37f, 1.4f, 1.42f, 1.43f, 1.45f, 1.46f, 1.47f, 1.48f, 1.49f, 1.5f, 1.5f, 1.5f, 1.49f, 1.45f, 1.39f, 1.32f, 1.25f, 1.18f, 1.11f, 1.05f, 1.01f};
float ag04F1[55] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.02f, -0.07f, -0.14f, -0.22f, -0.29f, -0.34f, -0.38f, -0.41f, -0.43f, -0.44f, -0.46f, -0.47f, -0.48f, -0.48f, -0.49f, -0.49f, -0.49f, -0.49f, -0.49f, -0.49f, -0.48f, -0.47f, -0.45f, -0.42f, -0.37f, -0.3f, -0.24f, -0.16f, -0.09f, -0.04f, 0.0f, 0.02f, 0.03f, 0.03f, 0.03f, 0.03f, 0.02f, 0.01f, 0.0f, 0.0f, 0.02f, 0.08f, 0.2f, 0.39f, 0.65f};
float ag04F5[10] = {0.33f, 0.56f, 0.7f, 0.8f, 0.89f, 0.94f, 0.97f, 0.99f, 1.0f, 1.0f};
float ag04B1[15] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.04f, 0.17f, 0.43f};
float ag04B5[44] = {0.36f, 0.64f, 0.84f, 0.96f, 1.0f, 1.02f, 1.07f, 1.14f, 1.22f, 1.29f, 1.34f, 1.38f, 1.41f, 1.43f, 1.44f, 1.46f, 1.47f, 1.47f, 1.48f, 1.49f, 1.49f, 1.49f, 1.49f, 1.49f, 1.49f, 1.49f, 1.48f, 1.46f, 1.44f, 1.41f, 1.39f, 1.35f, 1.32f, 1.28f, 1.25f, 1.21f, 1.17f, 1.14f, 1.11f, 1.08f, 1.05f, 1.03f, 1.01f, 1.0f};
float fema005F1[34] = {-0.01f, -0.04f, -0.1f, -0.17f, -0.26f, -0.36f, -0.47f, -0.59f, -0.72f, -0.85f, -0.97f, -1.1f, -1.22f, -1.33f, -1.43f, -1.52f, -1.6f, -1.65f, -1.69f, -1.7f, -1.7f, -1.7f, -1.7f, -1.68f, -1.61f, -1.5f, -1.36f, -1.17f, -0.95f, -0.7f, -0.41f, -0.1f, 0.24f, 0.61f};
float fema005F5[34] = {0.37f, 0.7f, 1.0f, 1.21f, 1.4f, 1.57f, 1.7f, 1.8f, 1.87f, 1.93f, 1.96f, 1.98f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 1.97f, 1.94f, 1.9f, 1.86f, 1.8f, 1.73f, 1.65f, 1.57f, 1.5f, 1.4f, 1.33f, 1.25f, 1.18f, 1.12f, 1.07f, 1.03f, 1.0f};
float fema005B1[34] = {-0.01f, -0.04f, -0.1f, -0.17f, -0.26f, -0.36f, -0.47f, -0.59f, -0.72f, -0.85f, -0.97f, -1.1f, -1.22f, -1.33f, -1.43f, -1.52f, -1.6f, -1.65f, -1.69f, -1.7f, -1.7f, -1.7f, -1.7f, -1.68f, -1.61f, -1.5f, -1.36f, -1.17f, -0.95f, -0.7f, -0.41f, -0.1f, 0.24f, 0.61f};
float fema005B5[34] = {0.37f, 0.7f, 0.98f, 1.2f, 1.4f, 1.55f, 1.66f, 1.74f, 1.79f, 1.8f, 1.8f, 1.79f, 1.78f, 1.76f, 1.73f, 1.7f, 1.66f, 1.62f, 1.58f, 1.52f, 1.46f, 1.4f, 1.34f, 1.28f, 1.23f, 1.18f, 1.14f, 1.1f, 1.08f, 1.05f, 1.03f, 1.02f, 1.01f, 1.0f};
float guno001F1[11] = {-0.03f, -0.09f, -0.15f, -0.18f, -0.17f, -0.15f, -0.09f, 0.0f, 0.14f, 0.35f, 0.62f};
float guno001F5[11] = {0.23f, 0.52f, 0.72f, 0.84f, 0.87f, 0.87f, 0.86f, 0.84f, 0.83f, 0.81f, 0.8f};
float guno001B1[11] = {-0.02f, -0.06f, -0.12f, -0.16f, -0.18f, -0.18f, -0.17f, -0.11f, 0.01f, 0.23f, 0.57f};
float guno001B5[11] = {0.24f, 0.6f, 0.85f, 1.0f, 1.08f, 1.1f, 1.08f, 1.02f, 0.95f, 0.88f, 0.82f};
float guno004B1[5] = {-0.03f, -0.06f, -0.08f, 0.06f, 0.39f};
float guno004B5[9] = {0.35f, 0.6f, 0.77f, 0.86f, 0.9f, 0.9f, 0.87f, 0.84f, 0.81f};
float guno005B1[11] = {-0.01f, -0.04f, -0.08f, -0.11f, -0.12f, -0.12f, -0.1f, -0.04f, 0.07f, 0.27f, 0.57f};
float guno005B5[11] = {0.41f, 0.66f, 0.8f, 0.88f, 0.94f, 0.98f, 1.01f, 1.02f, 1.02f, 1.01f, 1.0f};
float guno006B1[11] = {-0.03f, -0.09f, -0.15f, -0.18f, -0.18f, -0.15f, -0.1f, -0.01f, 0.14f, 0.35f, 0.64f};
float guno006B5[11] = {0.17f, 0.42f, 0.62f, 0.77f, 0.87f, 0.91f, 0.9f, 0.88f, 0.85f, 0.83f, 0.81f};
float guno010B1[12] = {0.0f, 0.0f, -0.01f, -0.02f, -0.03f, -0.04f, -0.04f, -0.03f, -0.01f, 0.03f, 0.08f, 0.15f};
float guno010B5[11] = {0.2f, 0.36f, 0.5f, 0.62f, 0.72f, 0.8f, 0.87f, 0.92f, 0.96f, 0.98f, 1.0f};
float guno013F1[34] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.04f, -0.16f, -0.3f, -0.44f, -0.55f, -0.63f, -0.68f, -0.7f, -0.65f, -0.53f, -0.32f, -0.06f, 0.25f, 0.6f};
float guno013F5[29] = {0.36f, 0.64f, 0.84f, 0.96f, 1.0f, 0.99f, 0.95f, 0.9f, 0.85f, 0.81f, 0.8f, 0.8f, 0.82f, 0.85f, 0.88f, 0.92f, 0.95f, 0.97f, 0.99f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
float guno013B1[34] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.04f, -0.16f, -0.3f, -0.44f, -0.55f, -0.63f, -0.68f, -0.7f, -0.65f, -0.53f, -0.32f, -0.06f, 0.25f, 0.6f};
float guno013B5[29] = {0.36f, 0.64f, 0.84f, 0.96f, 1.0f, 0.99f, 0.95f, 0.9f, 0.85f, 0.81f, 0.8f, 0.8f, 0.82f, 0.85f, 0.88f, 0.92f, 0.95f, 0.97f, 0.99f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
float guno015B1[11] = {0.01f, 0.03f, 0.06f, 0.11f, 0.16f, 0.22f, 0.29f, 0.36f, 0.44f, 0.52f, 0.61f};
float guno015B5[11] = {0.19f, 0.35f, 0.49f, 0.61f, 0.71f, 0.79f, 0.85f, 0.9f, 0.95f, 0.98f, 0.99f};
float guno017F1[11] = {-0.03f, -0.09f, -0.15f, -0.18f, -0.17f, -0.15f, -0.09f, 0.0f, 0.14f, 0.35f, 0.62f};
float guno017F5[11] = {0.23f, 0.52f, 0.72f, 0.84f, 0.87f, 0.87f, 0.86f, 0.84f, 0.83f, 0.81f, 0.8f};
float guno017B1[11] = {-0.02f, -0.06f, -0.12f, -0.16f, -0.18f, -0.18f, -0.17f, -0.11f, 0.01f, 0.23f, 0.57f};
float guno017B5[11] = {0.24f, 0.6f, 0.85f, 1.0f, 1.08f, 1.1f, 1.08f, 1.02f, 0.95f, 0.88f, 0.82f};
float guno018F1[11] = {-0.02f, -0.06f, -0.12f, -0.18f, -0.22f, -0.24f, -0.24f, -0.21f, -0.13f, 0.03f, 0.31f};
float guno018F5[11] = {0.76f, 1.2f, 1.41f, 1.45f, 1.43f, 1.38f, 1.31f, 1.23f, 1.14f, 1.07f, 1.02f};
float guno018B1[11] = {-0.03f, -0.09f, -0.18f, -0.27f, -0.33f, -0.36f, -0.35f, -0.32f, -0.22f, -0.07f, 0.28f};
float guno018B5[11] = {0.76f, 1.14f, 1.27f, 1.28f, 1.27f, 1.23f, 1.2f, 1.14f, 1.09f, 1.04f, 1.01f};
float guno025B1[11] = {0.01f, 0.03f, 0.06f, 0.11f, 0.16f, 0.22f, 0.29f, 0.37f, 0.45f, 0.53f, 0.61f};
float guno025B5[11] = {0.29f, 0.5f, 0.66f, 0.77f, 0.86f, 0.92f, 0.96f, 0.99f, 1.0f, 1.0f, 1.0f};
float utma006F1[29] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.04f, -0.14f, -0.27f, -0.42f, -0.56f, -0.66f, -0.69f, -0.7f, -0.69f, -0.69f, -0.66f, -0.62f, -0.55f, -0.44f, -0.3f, -0.11f, 0.13f, 0.43f};
float utma006F5[28] = {0.39f, 0.73f, 1.0f, 1.18f, 1.24f, 1.19f, 1.06f, 0.9f, 0.77f, 0.72f, 0.73f, 0.76f, 0.79f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f};
float utma006B1[29] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.04f, -0.16f, -0.3f, -0.45f, -0.56f, -0.6f, -0.61f, -0.6f, -0.59f, -0.55f, -0.5f, -0.4f, -0.27f, -0.09f, 0.14f, 0.44f};
float utma006B5[28] = {0.37f, 0.68f, 0.92f, 1.07f, 1.12f, 1.09f, 1.0f, 0.9f, 0.78f, 0.7f, 0.66f, 0.67f, 0.7f, 0.73f, 0.76f, 0.79f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f};
float utma007B1[5] = {0.03f, 0.08f, 0.16f, 0.28f, 0.44f};
float utma007B5[9] = {0.46f, 0.76f, 0.9f, 0.94f, 0.93f, 0.91f, 0.87f, 0.84f, 0.81f};
float utre002B1[5] = {-0.01f, -0.01f, 0.05f, 0.2f, 0.45f};
float utre002B5[5] = {0.36f, 0.62f, 0.75f, 0.79f, 0.8f};
float utre003B1[5] = {-0.07f, -0.16f, -0.19f, -0.08f, 0.27f};
float utre003B5[9] = {0.52f, 0.82f, 0.94f, 0.96f, 0.95f, 0.92f, 0.88f, 0.84f, 0.81f};
float utre005B1[5] = {0.0f, 0.02f, 0.07f, 0.16f, 0.33f};
float utre005B5[9] = {0.34f, 0.57f, 0.71f, 0.78f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f};
float utro001B1[11] = {0.0f, 0.02f, 0.04f, 0.08f, 0.13f, 0.19f, 0.27f, 0.37f, 0.48f, 0.62f, 0.85f};
float utro001B5[11] = {0.23f, 0.38f, 0.52f, 0.66f, 0.77f, 0.86f, 0.92f, 0.96f, 0.98f, 0.99f, 1.0f};
float utro002F1[11] = {0.0f, 0.01f, 0.02f, 0.05f, 0.08f, 0.12f, 0.18f, 0.26f, 0.35f, 0.46f, 0.66f};
float utro002F5[11] = {0.16f, 0.3f, 0.44f, 0.57f, 0.69f, 0.78f, 0.85f, 0.9f, 0.95f, 0.98f, 0.99f};
float utro002B1[11] = {0.0f, 0.02f, 0.04f, 0.08f, 0.13f, 0.19f, 0.27f, 0.37f, 0.48f, 0.62f, 0.85f};
float utro002B5[11] = {0.23f, 0.38f, 0.52f, 0.66f, 0.77f, 0.86f, 0.92f, 0.96f, 0.98f, 0.99f, 1.0f};
float utso006B1[5] = {0.0f, 0.01f, 0.06f, 0.16f, 0.33f};
float utso006B5[9] = {0.34f, 0.58f, 0.73f, 0.8f, 0.83f, 0.82f, 0.82f, 0.81f, 0.81f};
float boss001B1[11] = {0.01f, 0.02f, 0.05f, 0.09f, 0.14f, 0.2f, 0.26f, 0.33f, 0.4f, 0.48f, 0.56f};
float boss001B5[11] = {0.27f, 0.48f, 0.64f, 0.76f, 0.84f, 0.9f, 0.94f, 0.97f, 0.99f, 1.0f, 1.0f};
float boss004F1[9] = {-0.02f, -0.06f, -0.09f, -0.09f, -0.07f, 0.0f, 0.15f, 0.37f, 0.66f};
float boss004F5[14] = {0.24f, 0.42f, 0.55f, 0.66f, 0.74f, 0.81f, 0.87f, 0.91f, 0.94f, 0.97f, 0.98f, 0.99f, 1.0f, 1.0f};
float boss004B1[11] = {0.0f, -0.01f, -0.02f, -0.03f, -0.02f, 0.01f, 0.06f, 0.15f, 0.28f, 0.46f, 0.7f};
float boss004B5[14] = {0.28f, 0.48f, 0.61f, 0.7f, 0.79f, 0.85f, 0.9f, 0.94f, 0.96f, 0.98f, 0.99f, 1.0f, 1.0f, 1.0f};
float boss007B1[11] = {-0.02f, -0.06f, -0.12f, -0.17f, -0.21f, -0.23f, -0.2f, -0.11f, 0.07f, 0.31f, 0.63f};
float boss007B5[11] = {0.35f, 0.61f, 0.8f, 0.92f, 0.98f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
float boss012F1[24] = {-0.17f, -0.31f, -0.44f, -0.55f, -0.64f, -0.72f, -0.79f, -0.85f, -0.89f, -0.92f, -0.94f, -0.95f, -0.96f, -0.95f, -0.94f, -0.9f, -0.85f, -0.77f, -0.64f, -0.48f, -0.27f, -0.02f, 0.28f, 0.62f};
float boss012F5[29] = {0.23f, 0.37f, 0.49f, 0.59f, 0.67f, 0.74f, 0.79f, 0.83f, 0.88f, 0.9f, 0.93f, 0.95f, 0.96f, 0.97f, 0.98f, 0.99f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
float boss012B1[24] = {-0.01f, -0.02f, -0.05f, -0.08f, -0.11f, -0.15f, -0.2f, -0.24f, -0.28f, -0.31f, -0.35f, -0.37f, -0.39f, -0.39f, -0.39f, -0.37f, -0.33f, -0.28f, -0.2f, -0.09f, 0.04f, 0.19f, 0.39f, 0.65f};
float boss012B5[39] = {0.38f, 0.71f, 1.0f, 1.26f, 1.47f, 1.65f, 1.81f, 1.95f, 2.07f, 2.18f, 2.27f, 2.33f, 2.38f, 2.41f, 2.42f, 2.42f, 2.42f, 2.41f, 2.4f, 2.37f, 2.34f, 2.31f, 2.27f, 2.22f, 2.15f, 2.08f, 1.99f, 1.9f, 1.8f, 1.7f, 1.6f, 1.5f, 1.4f, 1.31f, 1.22f, 1.15f, 1.09f, 1.04f, 1.01f};
float boss013B1[11] = {0.02f, -0.03f, -0.1f, -0.17f, -0.23f, -0.25f, -0.21f, -0.1f, 0.08f, 0.32f, 0.63f};
float boss013B5[11] = {0.37f, 0.68f, 0.93f, 1.1f, 1.2f, 1.25f, 1.23f, 1.2f, 1.14f, 1.09f, 1.05f};
float boss014F1[11] = {-0.01f, -0.04f, -0.07f, -0.1f, -0.12f, -0.11f, -0.06f, 0.03f, 0.17f, 0.37f, 0.64f};
float boss014F5[11] = {0.44f, 0.93f, 1.39f, 1.75f, 1.94f, 2.0f, 2.0f, 1.75f, 1.38f, 1.15f, 1.03f};
float boss014B1[11] = {-0.01f, -0.05f, -0.1f, -0.15f, -0.2f, -0.24f, -0.25f, -0.18f, 0.0f, 0.27f, 0.61f};
float boss014B5[11] = {0.43f, 0.9f, 1.34f, 1.7f, 1.94f, 2.0f, 1.89f, 1.69f, 1.46f, 1.23f, 1.06f};
float boss016F1[25] = {-0.01f, -0.02f, -0.05f, -0.08f, -0.12f, -0.16f, -0.21f, -0.25f, -0.3f, -0.34f, -0.38f, -0.41f, -0.43f, -0.45f, -0.46f, -0.46f, -0.45f, -0.43f, -0.39f, -0.32f, -0.22f, -0.08f, 0.1f, 0.34f, 0.64f};
float boss016F5[26] = {0.37f, 0.67f, 0.92f, 1.1f, 1.22f, 1.28f, 1.3f, 1.31f, 1.3f, 1.27f, 1.24f, 1.19f, 1.15f, 1.1f, 1.06f, 1.02f, 1.0f, 0.99f, 0.98f, 0.98f, 0.98f, 0.98f, 0.99f, 0.99f, 1.0f, 1.0f};
float boss016B1[24] = {0.0f, -0.01f, -0.02f, -0.04f, -0.05f, -0.07f, -0.09f, -0.11f, -0.13f, -0.15f, -0.17f, -0.18f, -0.19f, -0.2f, -0.2f, -0.2f, -0.21f, -0.2f, -0.16f, -0.1f, -0.0f, 0.15f, 0.36f, 0.64f};
float boss016B5[27] = {0.39f, 0.76f, 1.08f, 1.34f, 1.5f, 1.56f, 1.56f, 1.53f, 1.49f, 1.44f, 1.38f, 1.32f, 1.26f, 1.19f, 1.13f, 1.08f, 1.03f, 1.0f, 0.98f, 0.97f, 0.96f, 0.97f, 0.97f, 0.98f, 0.99f, 1.0f, 1.0f};
float boss017F1[24] = {-0.16f, -0.31f, -0.45f, -0.57f, -0.68f, -0.77f, -0.85f, -0.91f, -0.95f, -0.98f, -0.99f, -0.99f, -0.96f, -0.92f, -0.87f, -0.8f, -0.72f, -0.62f, -0.5f, -0.35f, -0.17f, 0.04f, 0.3f, 0.63f};
float boss017F5[24] = {0.33f, 0.6f, 0.81f, 0.95f, 1.04f, 1.1f, 1.13f, 1.15f, 1.16f, 1.16f, 1.16f, 1.16f, 1.15f, 1.14f, 1.12f, 1.1f, 1.09f, 1.07f, 1.06f, 1.04f, 1.03f, 1.02f, 1.01f, 1.0f};
float boss017B1[24] = {-0.01f, -0.04f, -0.08f, -0.13f, -0.19f, -0.26f, -0.33f, -0.4f, -0.46f, -0.52f, -0.58f, -0.62f, -0.64f, -0.65f, -0.64f, -0.61f, -0.55f, -0.47f, -0.37f, -0.23f, -0.08f, 0.1f, 0.33f, 0.63f};
float boss017B5[24] = {0.31f, 0.56f, 0.76f, 0.91f, 1.02f, 1.11f, 1.17f, 1.23f, 1.26f, 1.28f, 1.3f, 1.3f, 1.29f, 1.28f, 1.26f, 1.23f, 1.2f, 1.17f, 1.13f, 1.1f, 1.07f, 1.04f, 1.02f, 1.01f};
float boss019F1[54] = {0.01f, 0.03f, 0.07f, 0.12f, 0.17f, 0.22f, 0.26f, 0.3f, 0.33f, 0.34f, 0.32f, 0.29f, 0.23f, 0.13f, 0.0f, -0.21f, -0.53f, -0.93f, -1.39f, -1.87f, -2.37f, -2.84f, -3.28f, -3.64f, -3.91f, -4.12f, -4.3f, -4.45f, -4.58f, -4.67f, -4.74f, -4.78f, -4.79f, -4.76f, -4.71f, -4.62f, -4.5f, -4.34f, -4.14f, -3.91f, -3.65f, -3.38f, -3.08f, -2.77f, -2.45f, -2.11f, -1.77f, -1.42f, -1.07f, -0.72f, -0.37f, -0.03f, 0.32f, 0.65f};
float boss019F5[29] = {0.23f, 0.44f, 0.66f, 0.86f, 1.0f, 1.1f, 1.18f, 1.24f, 1.3f, 1.34f, 1.36f, 1.38f, 1.39f, 1.39f, 1.38f, 1.36f, 1.34f, 1.32f, 1.29f, 1.25f, 1.22f, 1.18f, 1.15f, 1.12f, 1.08f, 1.06f, 1.03f, 1.02f, 1.0f};
float boss019B1[19] = {-0.01f, -0.05f, -0.11f, -0.17f, -0.25f, -0.32f, -0.4f, -0.46f, -0.5f, -0.52f, -0.51f, -0.45f, -0.35f, -0.22f, -0.06f, 0.13f, 0.33f, 0.55f, 0.78f};
float boss019B5[19] = {0.38f, 0.74f, 1.07f, 1.32f, 1.48f, 1.55f, 1.59f, 1.6f, 1.59f, 1.56f, 1.51f, 1.45f, 1.37f, 1.3f, 1.23f, 1.16f, 1.09f, 1.04f, 1.01f};
float boss020F1[5] = {0.0f, 0.02f, 0.07f, 0.14f, 0.25f};
float boss020F5[9] = {0.23f, 0.47f, 0.66f, 0.75f, 0.77f, 0.79f, 0.8f, 0.8f, 0.8f};
float boss020B1[5] = {0.0f, 0.01f, 0.03f, 0.09f, 0.2f};
float boss020B5[9] = {0.17f, 0.39f, 0.61f, 0.73f, 0.76f, 0.79f, 0.8f, 0.8f, 0.8f};
float boss029F1[19] = {0.01f, 0.01f, 0.01f, 0.02f, 0.02f, 0.03f, 0.03f, 0.04f, 0.04f, 0.03f, 0.01f, -0.01f, -0.02f, 0.0f, 0.06f, 0.22f, 0.46f, 0.67f, 0.77f};
float boss029F5[29] = {0.1f, 0.25f, 0.4f, 0.55f, 0.72f, 0.88f, 1.0f, 1.07f, 1.12f, 1.14f, 1.15f, 1.14f, 1.11f, 1.08f, 1.05f, 1.01f, 0.97f, 0.94f, 0.92f, 0.91f, 0.9f, 0.91f, 0.92f, 0.93f, 0.95f, 0.96f, 0.98f, 0.99f, 1.0f};
float boss029B1[19] = {-0.01f, -0.02f, -0.03f, -0.04f, -0.05f, -0.06f, -0.06f, -0.06f, -0.04f, -0.01f, 0.02f, 0.07f, 0.14f, 0.21f, 0.31f, 0.43f, 0.57f, 0.73f, 0.89f};
float boss029B5[29] = {0.25f, 0.41f, 0.55f, 0.68f, 0.71f, 0.86f, 0.92f, 0.97f, 1.01f, 1.04f, 1.06f, 1.07f, 1.07f, 1.07f, 1.07f, 1.07f, 1.06f, 1.06f, 1.06f, 1.07f, 1.07f, 1.07f, 1.07f, 1.06f, 1.04f, 1.03f, 1.02f, 1.01f, 1.0f};
AdjTbl adjTblPl[8] = {
    {1, {chaosF1, chaosF5, chaosB1, chaosB5}},
    {2, {kosmosF1, kosmosF5, kosmosB1, kosmosB5}},
    {3, {shionF1, shionF5, shionB1, shionB5}},
    {5, {ziggyF1, ziggyF5, ziggyB1, ziggyB5}},
    {6, {momoF1, momoF5, momoB1, momoB5}},
    {7, {jrF1, jrF5, jrB1, jrB5}},
    {10, {virgilF1, virgilF5, virgilB1, virgilB5}},
    {0, {0, 0, 0, 0}}
};
AdjTbl adjTblEn[42] = {
    {17, {vx01F1, vx01F5, vx01B1, vx01B5}},
    {18, {vx02F1, vx02F5, vx02B1, vx02B5}},
    {19, {vx03F1, vx03F5, vx03B1, vx03B5}},
    {22, {vx06F1, vx06F5, vx06B1, vx06B5}},
    {26, {ag04F1, ag04F5, ag04B1, ag04B5}},
    {67, {fema005F1, fema005F5, fema005B1, fema005B5}},
    {34, {guno001F1, guno001F5, guno001B1, guno001B5}},
    {50, {guno001F1, guno001F5, guno001B1, guno001B5}},
    {59, {guno001F1, guno001F5, guno001B1, guno001B5}},
    {37, {0, 0, guno004B1, guno004B5}},
    {38, {0, 0, guno005B1, guno005B5}},
    {39, {0, 0, guno006B1, guno006B5}},
    {43, {0, 0, guno010B1, guno010B5}},
    {46, {guno013F1, guno013F5, guno013B1, guno013B5}},
    {48, {0, 0, guno015B1, guno015B5}},
    {50, {guno017F1, guno017F5, guno017B1, guno017B5}},
    {51, {guno018F1, guno018F5, guno018B1, guno018B5}},
    {58, {0, 0, guno025B1, guno025B5}},
    {82, {utma006F1, utma006F5, utma006B1, utma006B5}},
    {83, {0, 0, utma007B1, utma007B5}},
    {92, {0, 0, utre002B1, utre002B5}},
    {93, {0, 0, utre003B1, utre003B5}},
    {95, {0, 0, utre005B1, utre005B5}},
    {97, {0, 0, utro001B1, utro001B5}},
    {98, {utro002F1, utro002F5, utro002B1, utro002B5}},
    {104, {0, 0, utso006B1, utso006B5}},
    {151, {0, 0, boss001B1, boss001B5}},
    {154, {boss004F1, boss004F5, boss004B1, boss004B5}},
    {159, {boss004F1, boss004F5, boss004B1, boss004B5}},
    {160, {boss004F1, boss004F5, boss004B1, boss004B5}},
    {161, {boss004F1, boss004F5, boss004B1, boss004B5}},
    {185, {boss004F1, boss004F5, boss004B1, boss004B5}},
    {157, {0, 0, boss007B1, boss007B5}},
    {162, {boss012F1, boss012F5, boss012B1, boss012B5}},
    {163, {0, 0, boss013B1, boss013B5}},
    {164, {boss014F1, boss014F5, boss014B1, boss014B5}},
    {166, {boss016F1, boss016F5, boss016B1, boss016B5}},
    {167, {boss017F1, boss017F5, boss017B1, boss017B5}},
    {169, {boss019F1, boss019F5, boss019B1, boss019B5}},
    {170, {boss020F1, boss020F5, boss020B1, boss020B5}},
    {179, {boss029F1, boss029F5, boss029B1, boss029B5}},
    {0, {0, 0, 0, 0}}
};

/*
 * OV01 original TU 2: 0x00a00d60..0x00a07770 (119 functions)
 */
#include "common.h"
#include "shared.h"
#include "unit_cmd.h"

extern unsigned char unitTbl[0x20];
extern int unitPlNum;
extern int unitEnNum;

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
extern const char D_00A438F0[];
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
        printf(D_00A438F0, pilotId);
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

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCreate);

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
extern char D_00A43940[];
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
    printf(D_00A43940, calcUPGet(&unit->task)->charaId, unit, actor, calcUPGet(&unit->task), unit->task.work);
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

extern char D_00A43968[];

void unitEnInit(s16 *tbl)
{
    int i;

    for (i = 0; *tbl != 0; tbl += 4) {
        if (i >= 5) {
            printf(D_00A43968);
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
extern void unitJobTypeAtk(ObjectTask *unit, int *cmd, char *tec, int type);
extern char D_00A43980[];

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
    printf(D_00A43980, (char)(unsigned char)tec[4], *cmd);
    for (;;) {
        break;
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitJobTypeAtk);

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

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdEther);

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

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdItemSub);

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

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdUnitChange);

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

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdDmg);

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

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdDead);

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

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdMoveCha);

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

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdMoveChaNorm);

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

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdMovePos);

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdMovePosNorm);

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

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdPosAdj);

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdRun);

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdJump);

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdHover);

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdFloat);

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

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdVASet);

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdMoveCalc);

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

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdBtst);

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitCmdBted);

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

INCLUDE_ASM("asm/nonmatchings/ov01/unit_cmd", unitMotSetEx);

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
        extern const char D_00A43BD8[];

        printf(D_00A43BD8, unit);
    }
    return -1;
}

extern const char D_00A43BF8[];
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
    printf(D_00A43BF8, index);
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

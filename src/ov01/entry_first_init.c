/*
 * OV01 original TU 7: 0x00a22628..0x00a24a98 (50 functions)
 */
#include "common.h"
#include "entry_first_init.h"
#include "main/xgl_studio.h"
#include "ov01/battle_init.h"

extern void sefInitEffect(void);
extern void MBattleInit(void);
extern void dataBatDatLoad(void);
extern void dataPlUnitInit(void);
extern void thinkNoSet(int thinkNo);
extern void mapNoSet(int mapNo);

void entryFirstInit(void) {
    sefInitEffect();
    MBattleInit();
    dataBatDatLoad();
    dataPlUnitInit();
    thinkNoSet(0);
    mapNoSet(1);
}

INCLUDE_ASM("asm/nonmatchings/ov01/entry_first_init", battleProc);

extern int pauseFlag;
extern int sRender[];
extern void xglRenderClearOff(void);
extern void thinkSysInit(void);
extern void sefDestroyEffect(void);
extern void MBattleDestroy(void);
extern void MCamReleaseControl(void);
extern void JNT_offSmoothHair(void);
extern void xglCullingIgnoreOff(void);
extern void ACT_init(void);
extern void ACT_initSequence(void);
extern void xglSoundSendEffect(int a, int b, int c);
extern void xglSoundEffectStopBank(int bank);

void battleProcEnd(void) {
    int i;

    xglRenderClearOff();
    sRender[15] = 0;
    pauseFlag = 0;
    thinkSysInit();
    sefDestroyEffect();
    MBattleDestroy();
    if (cameraFlagGet() != 0) {
        MCamReleaseControl();
    }
    JNT_offSmoothHair();
    xglCullingIgnoreOff();
    ACT_init();
    ACT_initSequence();
    for (i = 1; i < 15; i++) {
        xglSoundSendEffect(0, 0, i);
    }
    xglSoundEffectStopBank(0);
}

extern int frameCnt;

int frameCntGet(void) {
    return frameCnt;
}

int battleTimeGet(void) {
    return frameCntGet();
}

void cameraFlagSet(int flag) {
    cameraFlag = flag;
}

int cameraFlagGet(void) {
    return cameraFlag;
}

int pauseProc(void) {
    return 1;
}

extern int calcBattleChk(void *unit);
typedef struct SelectPLParam { char pad[0x38]; short charaId; } SelectPLParam;
extern SelectPLParam *calcUPGet(void *unit);
extern int unitLiveNumGet2(int kind);
extern int unitTblGet(int kind, BattleUnit ***out);
extern int rnd(int n);

int selectPL(int excludeUnit)
{
    int list[4];
    BattleUnit **tbl;
    int n, i, cnt;
    int *p;

    if (excludeUnit != 0 && calcBattleChk((void *)excludeUnit) != 0
        && calcUPGet((void *)excludeUnit)->charaId < 8) {
        return excludeUnit;
    }
    if (unitLiveNumGet2(0) == 0) {
        return 0;
    }
    n = unitTblGet(0, &tbl);
    i = 0;
    cnt = 0;
    if (n > 0) {
        p = list;
        do {
            if (calcBattleChk(tbl[i]) != 0 && calcUPGet(tbl[i])->charaId < 8) {
                cnt++;
                *p++ = (int)tbl[i];
            }
            i++;
        } while (i < n);
    }
    if (cnt == 0) {
        return 0;
    }
    return list[rnd(cnt - 1)];
}

extern int mapNoGet(void);

/* lastBossBtlChk (0x00a22a60): map 0x34 is the game's final-boss battle map. */
#define MAP_NO_LAST_BOSS 0x34

int lastBossBtlChk(void) {
    return mapNoGet() == MAP_NO_LAST_BOSS;
}

extern int cfEventGet(void);

int bossBtlChk(void)
{
    /* Boss battle requires cfEventGet bit 3 (0x8) set and bit 4 (0x10) clear. */
    if (cfEventGet() & 8) {
        if (!(cfEventGet() & 0x10)) {
            return 1;
        }
    }
    return 0;
}

extern void UnduInit(int flag);
extern void ACT_init(void);
extern void ACT_initSequence(void);
extern void ACT_DrawShadowInit(void);
extern void objInit(void);
extern void dataCdSyncClear(void);
extern void battleInit(void);
extern void sefInitEffectBattle(void);
extern void JNT_onSmoothHair(void);
extern void xglCullingIgnore(void);
extern void xglStudioChange(int studio_index);
extern void xglLightSetDefault(StudioLight *light);
extern void xglLightIntensityParallel(StudioLight *light, int index, Vector4 *intensity);
extern void xglLightIntensityAmbient(StudioLight *light, Vector4 *ambient);
void cfSnap(void);
extern void eBattleWinInit(void);
extern void thinkInit(void);
extern void MCamTakeControl(void);
extern int monsSetNoGet(void);
extern int thinkMonsTblNumGet(void);
extern int thinkNoGet(void);
extern int cfEncountGet(void);
extern const char D_00A46620[];
extern int printf(const char *format, ...);
extern void GameDefocusQuickSet(int first, int second, int third, int fourth);
extern int sndSysSePlay(int soundId);
int entryPhase10(void);
extern int battleDispMode;
extern Vector4 sIntPararel2_0;
extern Vector4 sIntAmbient_1;

int entryPhase00(void) {
    StudioLight *light;
    int cfEncount;
    int thinkNo;
    int thinkMonsTblNum;
    int monsSetNo;
    int mapNo;

    UnduInit(0);
    battleDispMode = 0;
    ACT_init();
    ACT_initSequence();
    ACT_DrawShadowInit();
    objInit();
    dataCdSyncClear();
    battleInit();
    sefInitEffectBattle();
    JNT_onSmoothHair();
    xglCullingIgnore();
    xglStudioChange(0);
    xglStudioGetLight(&light);
    xglLightSetDefault(light);
    xglLightIntensityParallel(light, 2, &sIntPararel2_0);
    xglLightIntensityAmbient(light, &sIntAmbient_1);
    cfSnap();
    eBattleWinInit();
    thinkInit();
    if (cameraFlagGet() != 0) {
        MCamTakeControl();
    }
    mapNo = mapNoGet();
    monsSetNo = monsSetNoGet();
    thinkMonsTblNum = thinkMonsTblNumGet();
    thinkNo = thinkNoGet();
    cfEncount = cfEncountGet();
    printf(D_00A46620, mapNo, monsSetNo, thinkMonsTblNum, thinkNo, cfEncount, cfEventGet());
    GameDefocusQuickSet(0, 1, 0xFFFFFF, 0);
    sndSysSePlay(0x25);
    battleSeq = entryPhase10;
    return 1;
}

extern int sRender[];
extern void *plUnitTblGet(void);
extern void dataLeaderChk(void *tbl);
extern void *thinkMonsTblGet(int no);
extern void dataEnemyMdlChk(void *tbl);
extern int dataCdSync(void);
extern int D_00A5A214;
extern void dataSndMuLoad(int kind);
int fadeIn(int speed, int delay);
extern FadeObjTask *pFadeIn;
extern void encEffObjInit(void);
int entryPhase15(void);

int entryPhase10(void) {
    int kind;

    sRender[13] = 0;
    dataLeaderChk(plUnitTblGet());
    dataEnemyMdlChk(thinkMonsTblGet(monsSetNoGet()));
    D_00A5A214 = dataCdSync();
    if (lastBossBtlChk() != 0) {
        kind = 2;
    } else {
        kind = bossBtlChk() != 0;
    }
    dataSndMuLoad(kind);
    *(int *)&pFadeIn = fadeIn(0x80, 3);
    encEffObjInit();
    battleSeq = entryPhase15;
    return 1;
}

extern int dataCdSync(void);
extern int D_00A5A214;
extern void sndMuTransPlay(int id);
extern void dataXtxLoad(void);
extern FadeObjTask *pFadeIn;
int entryPhase16(void);

int entryPhase15(void)
{
    if (dataCdSync() <= D_00A5A214) {
        sndMuTransPlay(0);
        D_00A5A214 = dataCdSync();
        dataXtxLoad();
        battleSeq = entryPhase16;
    }
    pFadeIn->delay = 2;
    return 1;
}

extern void dataMapLoad(int mapNo);
int entryPhase20(void);

int entryPhase16(void)
{
    if (dataCdSync() <= D_00A5A214) {
        D_00A5A214 = dataCdSync();
        dataMapLoad(mapNoGet());
        battleSeq = entryPhase20;
    }
    pFadeIn->delay = 2;
    return 1;
}

/*
 * D_00A5A220 is a second camera-event request record: camEventExec's other
 * caller (src/ov01/battle_init.c's D_00A57B80) fills the same two fields,
 * a type code at +0x00 and the acting unit at +0x08, before the call.
 */
typedef struct EntryCamEvent {
    int type;                  /* +0x00 */
    unsigned char unmodeled_04[4];
    BattleUnit *unit;          /* +0x08 */
} EntryCamEvent;

extern EntryCamEvent D_00A5A220;

/*
 * This TU's own call site sets up only the event-record argument, unlike
 * src/ov01/battle_init.c's accepted call, which declares camEventExec void
 * and passes a second (BattleUnit *) argument: each TU forward-declares the
 * function as its own call site uses it.
 */
extern int camEventExec(EntryCamEvent *event);

extern void mapInit(int mapNo);
extern void camInit(void);
extern int camInitExec(void);
extern int encEffCmd;
extern int unitIdx;
int entryPhase25(void);

int entryPhase20(void)
{
    EntryCamEvent *ev;

    if (dataCdSync() <= D_00A5A214) {
        mapInit(mapNoGet());
        camInit();
        camInitExec();
        ev = &D_00A5A220;
        ev->type = 4;
        camEventExec(ev);
        battleDispMode = 1;
        encEffCmd = 1;
        D_00A5A214 = dataCdSync();
        unitIdx = 0;
        battleSeq = entryPhase25;
    } else {
        pFadeIn->delay = 2;
    }
    return 1;
}

extern int unitPlCreate(s16 *pl);
extern void menuStatDispCreate(void);
extern int entryPhase26(void);
extern int entryPhase30(void);
extern int pCreateUnit;

int entryPhase25(void)
{
    s16 *tbl;
    int i;
    int last;

    tbl = (s16 *)plUnitTblGet();
    for (; unitIdx < 3; unitIdx++) {
        if (tbl[unitIdx * 4] != 0) {
            break;
        }
    }
    if (unitIdx >= 3) {
        battleSeq = entryPhase30;
        return 1;
    }
    D_00A5A214 = dataCdSync();
    pCreateUnit = unitPlCreate(&tbl[unitIdx * 4]);
    unitIdx++;
    last = 0;
    for (i = 0; i < 3; i++) {
        if (tbl[i * 4] != 0) {
            last = i;
        }
    }
    if (last == unitIdx - 1) {
        menuStatDispCreate();
    }
    battleSeq = entryPhase26;
    return 1;
}

int entryPhase25(void);
extern int sefCheckLoad(void);
extern void dataUnitFileLoad2(int unit);
extern int pCreateUnit;

int entryPhase26(void)
{
    if (dataCdSync() <= D_00A5A214 && sefCheckLoad() == 0) {
        dataUnitFileLoad2(pCreateUnit);
        D_00A5A220.unit = (BattleUnit *)pCreateUnit;
        D_00A5A220.type = 0x17;
        camEventExec(&D_00A5A220);
        battleSeq = entryPhase25;
    }
    return 1;
}

extern int dataCdSync(void);
extern int selectPL(int excludeUnit);
extern void dataSndSeLoad(int unit, int seId);
extern int entryPhase31(void);

extern int D_00A5A214;
extern int pCvUnit;

int entryPhase30(void) {
    int unit;

    D_00A5A214 = dataCdSync();
    unit = selectPL(0);
    pCvUnit = unit;
    if (unit != 0) {
        dataSndSeLoad(unit, 0x18);
    }
    battleSeq = &entryPhase31;
    return 1;
}

extern int unitIdx;
extern void dataSndSeLoad2(int unit);
extern void sndSeTransPlay(int unit, int seId, int volume);
extern int entryPhase35(void);

int entryPhase31(void)
{
    if (dataCdSync() <= D_00A5A214) {
        if (pCvUnit != 0) {
            dataSndSeLoad2(pCvUnit);
            sndSeTransPlay(pCvUnit, 0x18, 1);
        }
        D_00A5A214 = dataCdSync();
        unitIdx = 0;
        battleSeq = entryPhase35;
    }
    return 1;
}

extern int entryPhase36(void);
extern int entryPhase40(void);
extern int unitEnCreate(short *pl);

int entryPhase35(void)
{
    short *tbl;
    int u;

    tbl = thinkMonsTblGet(monsSetNoGet());
    if (unitIdx >= 5 || *(tbl + unitIdx * 4) == 0) {
        battleSeq = entryPhase40;
        return 1;
    }
    D_00A5A214 = dataCdSync();
    pCreateUnit = unitEnCreate(tbl + unitIdx * 4);
    battleSeq = entryPhase36;
    unitIdx++;
    return 1;
}

extern void dataUnitFileLoad2(int unit);
extern int entryPhase35(void);
extern int sefCheckLoad(void);
extern int pCreateUnit;

int entryPhase36(void)
{
    if (dataCdSync() <= D_00A5A214 && sefCheckLoad() == 0) {
        dataUnitFileLoad2(pCreateUnit);
        battleSeq = entryPhase35;
    }
    return 1;
}

extern int cfEncountGet(void);
extern void calcCfEncount(int flags);
extern void thinkInitExec(void);
extern void sdvSaveAmbient(void);
extern void sefSetupEffect(void);
extern int entryPhase45(void);

int entryPhase40(void) {
    calcCfEncount(cfEncountGet());
    thinkInitExec();
    sdvSaveAmbient();
    sefSetupEffect();
    battleSeq = &entryPhase45;
    return 1;
}

extern int entryPhase50(void);
extern void encountEffDisp(void);
extern int unitTblGet(int kind, BattleUnit ***out);

int entryPhase45(void) {
    BattleUnit **units;
    int count;
    int busy;
    int t;
    int i;

    if (sefCheckLoad() == 0) {
        count = unitTblGet(2, &units);
        busy = 0;
        for (i = 0; i < count; i++) {
            if (units[i] != 0) {
                t = busy + 1;
                if (*(int *)units[i]->actor & 8) {
                    busy = t;
                }
            }
        }
        if (busy == 0) {
            encountEffDisp();
            frameCnt = 0;
            battleSeq = entryPhase50;
        }
    }
    return 1;
}

/*
 * This call site reads battleMain's return register even though
 * src/ov01/battle_init.c's own accepted definition declares it void; each TU
 * forward-declares the function as its own call site uses it.
 */
extern int battleMain(void);
extern int battleRetCodeGet(void);
extern BattleUnit *lastUnitGet(void);
extern int sndMuStop(void);
extern void GameDefocusSet(int first, int second, int third);
int entryPhase51(void);

int entryPhase50(void)
{
    int unit;

    if (battleMain() == 0) {
        D_00A5A214 = dataCdSync();
        if (battleRetCodeGet() == 1) {
            unit = selectPL((int)lastUnitGet());
            pCvUnit = unit;
            if (unit != 0) {
                dataSndSeLoad(unit, 0x19);
            }
            sndMuStop();
            if (lastBossBtlChk() == 0) {
                sndMuTransPlay(1);
            }
        } else {
            pCvUnit = 0;
        }
        GameDefocusSet(-1, 0, 0);
        battleSeq = entryPhase51;
    }
    return 1;
}

extern void dataSndSeLoad2(int unit);
extern void sndSeTransPlay(int unit, int seId, int volume);
int entryPhase60(void);

int entryPhase51(void)
{
    if (dataCdSync() <= D_00A5A214) {
        if (pCvUnit != 0) {
            dataSndSeLoad2(pCvUnit);
            sndSeTransPlay(pCvUnit, 0x19, 1);
        }
        battleSeq = entryPhase60;
    }
    return 1;
}

extern int battleAfter(void);
extern int fadeOut(int, int);
extern int entryPhase70(void);

int entryPhase60(void) {
    if (battleAfter() == 0) {
        fadeOut(4, 2);
        battleSeq = entryPhase70;
    }
    return 1;
}

int entryPhase70(void) {
    extern int battleRetCodeGet(void);
    extern int cfEventGet(void);
    extern void calcPara2Org(void);
    extern void resultProcInit(void);
    extern int entryPhase80(void);
    extern int entryPhase100(void);
    extern void battleProcEnd(void);

    if (fadeEndChk() != 0) {
        int retCode = battleRetCodeGet();

        if ((retCode == 1 || retCode == 3) && (cfEventGet() & 0x10) == 0) {
            calcPara2Org();
            battleDispMode = 2;
            resultProcInit();
            battleSeq = entryPhase80;
        } else {
            battleDispMode = 0;
            battleSeq = entryPhase100;
        }
        battleProcEnd();
    }
    return 1;
}

extern const char D_00A46660[];
extern const char D_00A46678[];
extern const char D_00A46698[];
extern int entryPhase90(void);
typedef struct {
    int exp, tp, ep, sp, money;
    int grp[8];
    int item[8];
    int num;
} MonsGain80;
extern MonsGain80 *monsGainPtrGet(void);

int entryPhase80(void) {
    int i;
    MonsGain80 *g;

    i = 0;
    g = monsGainPtrGet();
    printf(D_00A46660, g->exp, g->money);
    printf(D_00A46678, g->tp, g->ep, g->sp);
    for (; i < g->num; i++) {
        printf(D_00A46698, i, g->grp[i], g->item[i]);
    }
    battleSeq = entryPhase90;
    return 1;
}

extern int resultProc(void);
extern int entryPhase100(void);

int entryPhase90(void) {
    if (resultProc() == 0) {
        battleSeq = entryPhase100;
    }
    return 1;
}

extern void calcDeadReset(void);
extern void dataLeaderReload(void);
extern int sndMuStop(void);

int entryPhase100(void) {
    calcDeadReset();
    dataLeaderReload();
    sndMuStop();
    return 0;
}

extern void sefCreateEffect(int *params);

/*
 * The local effect-request block sefCreateEffect reads (evidenced only by
 * this call and by src/ov01/battle_init.c's DecoEffParams, which shares the
 * same 0x24-byte size and +0x14 subId offset): only subId is set here.
 */
typedef struct EncounterEffectRequest {
    unsigned char unmodeled_00[0x14];
    short subId;                      /* +0x14 */
    unsigned char unmodeled_16[0x24 - 0x16];
} EncounterEffectRequest;

void encountEffDisp(void) {
    EncounterEffectRequest params;

    memset(&params, 0, sizeof(params));
    if (cfEncountGet() & 1) {
        params.subId = 0xA42;
        sefCreateEffect((int *)&params);
    }
    if (cfEncountGet() & 2) {
        params.subId = 0xA43;
        sefCreateEffect((int *)&params);
    }
    if (cfEncountGet() & 4) {
        params.subId = 0xA44;
        sefCreateEffect((int *)&params);
    }
}

extern int grBBIdxGet(void);
extern void grDBIdxGet(int *dispIdx, int *unused);
extern ObjectTask *vramCopyObjCreate(int srcAddr, int dstAddr);

void cfSnap(void) {
    int dispIdx;
    int unused;

    grDBIdxGet(&dispIdx, &unused);
    vramCopyObjCreate(dispIdx, grBBIdxGet());
}

extern void MBattleExec(void);
extern void xglStudioFlushActiveCamera(void);
extern void mapDisp(int mapNo);
extern void ACT_draw(void);
extern void GameDrawShadow(void);
extern void SeisanDisp(void);

int battleDisp(void)
{
    switch (battleDispMode) {
    case 0:
        break;
    case 1:
        MBattleExec();
        xglStudioFlushActiveCamera();
        mapDisp(mapNoGet());
        ACT_draw();
        GameDrawShadow();
        break;
    case 2:
        SeisanDisp();
        break;
    }
    return 1;
}

extern ObjectTask *objEntry2(void *argument, void (*callback)(ObjectTask *task));
extern void objStdInit(ObjectTask *task);
void vramCopyDummy(void);
void vramCopyObj(ObjectTask *task);

ObjectTask *vramCopyObjCreate(int srcAddr, int dstAddr) {
    ObjectTask *task;
    VramCopyObjTask *self;

    task = objEntry2(vramCopyDummy, vramCopyObj);
    objStdInit(task);
    self = (VramCopyObjTask *)task;
    self->srcAddr = srcAddr;
    self->dstAddr = dstAddr;
    return task;
}

void vramCopyDummy(void) {

}

extern int grVramCopyFBtoFB(int src, int dst);
extern void objRemove(ObjectTask *task);

void vramCopyObj(ObjectTask *task) {
    VramCopyObjTask *self = (VramCopyObjTask *)task;

    grVramCopyFBtoFB(self->srcAddr, self->dstAddr);
    objRemove(task);
}

void fadeInObj(ObjectTask *task);
extern void fadeObjDraw(FadeObjTask *self);

int fadeIn(int speed, int delay) {
    FadeObjTask *self;

    self = (FadeObjTask *)objEntry2(fadeInObj, (void (*)(ObjectTask *))fadeObjDraw);
    self->delay = delay;
    self->speed = speed;
    self->life = 0;
    fadeFlag = 0;
    return (int)self;
}

void fadeOutObj(FadeObjTask *self);
extern void fadeObjDraw(FadeObjTask *self);

int fadeOut(int speed, int delay) {
    FadeObjTask *self;

    self = (FadeObjTask *)objEntry2(fadeOutObj, (void (*)(ObjectTask *))fadeObjDraw);
    self->delay = delay;
    self->speed = speed;
    self->life = 0x80;
    fadeFlag = 0;
    return (int)self;
}

int fadeEndChk(void)
{
    return fadeFlag;
}

void fadeInObj(ObjectTask *task) {
    FadeObjTask *self = (FadeObjTask *)task;

    if (self->delay != 0) {
        self->delay--;
        return;
    }
    self->life += self->speed;
    if ((unsigned int)self->life >= 0x81) {
        fadeFlag = 1;
        objRemove(task);
    }
}

extern void fadeObjDraw(FadeObjTask *self);

void fadeOutObj(FadeObjTask *self)
{
    if (self->delay != 0) {
        self->delay--;
        return;
    }

    self->life -= self->speed;
    if (self->life == 0) {
        /* The original stores 0 into life here a second time (a separate
         * `sw` right after the subtraction's own store), even though it is
         * already 0 on this path. */
        self->life = 0;
        fadeObjDraw(self);
        fadeFlag = 1;
        objRemove(&self->task);
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/entry_first_init", fadeObjDraw);

extern int grFBAdrGet(int index);
extern void GS_REG_SET(void *packet, long long register_id, long long control);

void encEnvMake(void *packet)
{
    unsigned short fbp;

    fbp = grFBAdrGet(grBBIdxGet()) << 5;
    GS_REG_SET(packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET(packet, 0x3F, 1);
    GS_REG_SET(packet, 6, (long long)fbp | 0x2000000664020000LL);
    GS_REG_SET(packet, 0x42, 0);
    GS_REG_SET(packet, 0x47, 0x30000);
    GS_REG_SET(packet, 0x14, 0x60);
}

INCLUDE_ASM("asm/nonmatchings/ov01/entry_first_init", encEffObjInit);

INCLUDE_ASM("asm/nonmatchings/ov01/entry_first_init", encEffObj);

extern void xglMatrixUnit(float (*matrix)[4]);
extern void xglMatrixTrans(float (*destination)[4], float (*source)[4], const float translation[4]);
extern void xglMatrixScale(float (*destination)[4], float (*source)[4], const float scale[4]);
extern void grRotMatrix(float (*destination)[4], float (*source)[4], float *angles);
extern float theta;

void effCalcMatrix(void *unused, float (*m)[4], int t) {
    float rot[4];
    float trans[4];
    float scale[4];

    xglMatrixUnit(m);
    trans[0] = 0.0f;
    trans[1] = 0.0f;
    trans[2] = 0.0f;
    trans[3] = 1.0f;
    rot[0] = 0.0f;
    rot[1] = 0.0f;
    rot[2] = theta * (float)t * 0.125f;
    rot[3] = 1.0f;
    scale[0] = 1.0f;
    scale[1] = 1.0f;
    scale[2] = 1.0f;
    scale[3] = 1.0f;
    xglMatrixTrans(m, m, trans);
    grRotMatrix(m, m, rot);
    xglMatrixScale(m, m, scale);
}

typedef struct EncPacket {
    int unmodeled[4];
} EncPacket;

extern void grGpInit(EncPacket *packet);

typedef struct EncDrawPacket {
    void *data;
    int count;
    int unmodeled[2];
} EncDrawPacket;
typedef struct EncEffObjEntry {
    short active;
    short pad[0x27];
    char prim[0x70];
} EncEffObjEntry;
typedef struct EncEffDrawArg {
    int unk0[7];
    int env;
    int size;
} EncEffDrawArg;
extern int encObjCnt;
extern void encEffVramCopy(int size);
extern void encEffAppend(void);
extern void grOpenFT4(void *packet);
extern void grPutFT4(void *packet, void *prim, int flag);
extern void grCloseFT4(void);
extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkAddCode(XglPacket *packet, unsigned int code);
extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int flag);
extern void sceVif1PkAddDirectDataN(XglPacket *packet, void *data, int count);
extern void sceVif1PkCloseDirectHLCode(XglPacket *packet);
extern void sceVif1PkTerminate(XglPacket *packet);

void encEffObjDraw(EncEffDrawArg *arg)
{
    EncDrawPacket packet;
    XglPacket *pk;
    EncEffObjEntry *obj;
    int i;

    obj = (EncEffObjEntry *)0x01DBF840;
    if (encEffCmd != 0) {
        encEffVramCopy(arg->size);
    }
    pk = xglPacketGetCurrent();
    grGpInit((EncPacket *)&packet);
    sceVif1PkCnt(pk, 0);
    sceVif1PkAddCode(pk, 0x11000000);
    sceVif1PkOpenDirectHLCode(pk, 0);
    ((void (*)(void *, int))encEnvMake)(&packet, arg->env);
    for (i = 0; i < encObjCnt; i++, obj++) {
        if (obj->active == 1) {
            grOpenFT4(&packet);
            grPutFT4(&packet, obj->prim, 0);
            grCloseFT4();
            if (i % 50 == 0) {
                sceVif1PkAddDirectDataN(pk, packet.data, packet.count);
                packet.count = 0;
            }
        }
    }
    sceVif1PkAddDirectDataN(pk, packet.data, packet.count);
    sceVif1PkCloseDirectHLCode(pk);
    sceVif1PkTerminate(pk);
    encEffAppend();
}

INCLUDE_ASM("asm/nonmatchings/ov01/entry_first_init", encEffAppend);

typedef struct EncFrameBaseTable {
    unsigned short base[3];
} EncFrameBaseTable;
typedef struct EncVramCopyParams {
    unsigned short source_base;
    unsigned short source_width;
    unsigned short destination_base;
    unsigned short destination_width;
    unsigned short source_x;
    unsigned short source_y;
    unsigned short destination_x;
    unsigned short destination_y;
    unsigned short width;
    unsigned short height;
} EncVramCopyParams;
extern const EncFrameBaseTable D_00A466C8;
extern const EncVramCopyParams D_00A466D0;
extern void grVramCopy(EncVramCopyParams *params);
extern void grGsRegSet(EncPacket *packet, unsigned long long register_id, unsigned long long value);
extern void grPacketSend(EncPacket *packet);

void encEffVramCopy(int size)
{
    EncPacket packet;
    EncFrameBaseTable bases = D_00A466C8;
    EncVramCopyParams params = D_00A466D0;
    int dispIdx;
    int unused;
    int bb;
    int w;
    int h;

    grDBIdxGet(&dispIdx, &unused);
    bb = grBBIdxGet();
    w = size;
    if (size > 0x200) {
        w = 0x200;
    }
    h = size;
    if (size > 0x1C0) {
        h = 0x1C0;
    }
    params.source_base = bases.base[unused];
    params.destination_base = bases.base[bb];
    params.destination_x = 0x100 - w / 2;
    params.destination_y = 0xE0 - h / 2;
    params.source_x = 0x100 - w / 2;
    params.source_y = 0xE0 - h / 2;
    params.width = w;
    params.height = h;
    grVramCopy(&params);
    grGpInit(&packet);
    grGsRegSet(&packet, 0x3F, 1);
    grPacketSend(&packet);
}

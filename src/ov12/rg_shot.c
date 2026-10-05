/*
 * OV12 original TU 23: 0x00a15600..0x00a17c90 (63 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_shot.h"

extern void assert_prog(const char *expression, const char *source_file,
                        int line);

extern const char D_00A532E8[]; /* "pShot != NIL" */
extern const char D_00A532F8[]; /* "../rg_shot.euc.c" */
extern const char D_00A53338[]; /* "pExtInfo != NIL" */
extern const char D_00A53348[]; /* "pEssence != NIL" */
extern const char D_00A53358[]; /* "pInfo != NIL" */
extern const char D_00A53368[]; /* "pBom != NIL" */
extern const char D_00A53378[]; /* "tama" (bomb model variant) */
extern const char D_00A53390[]; /* "pEss != NIL" */
extern const char D_00A533D8[]; /* "pRob != NIL" */
extern const char D_00A533E8[]; /* "pBgObj != NIL" */
extern const char D_00A53380[]; /* "eff11.ptcl" (default hit-effect file) */
extern const char D_00A533A0[]; /* "pFire != NIL" */
extern const char D_00A533C8[]; /* "pBeam != NIL" */
extern const char D_00A53310[]; /* "pGeom != NIL" */

extern void *RgCharAlloc(unsigned int size, int type);
static void *_InitBeam(void *beam, void *essence, void *info);

/* Local siblings of this TU still in asm, needed by functions of this allocation. */
static void _InitShotCommonSub(RgShot *shot, int id);
static void _InitRgShotEssence(RgShotEssence *essence);
static int _GrenadeEntryHistory(RgShot *shot, int bgObject);
static void _GrenadeBomPassTime(RgShot *shot, float deltaTime);
static void _InitGrenade(void *shot, void *essence, void *info);
static void _HomingPassTime(RgHomingShot *shot, float deltaTime);
static void _ShotCommonPassTime(RgShot *shot, float deltaTime);

extern int RgBgObjTryToBreak(int bgObject, float damage);
extern void XrgSoundRingVol(int driver, int soundId, int volume);
extern void CreateRgHitEffectPos(RgVector position, const char *effectFile,
                                 const char *defaultEffectFile);

/* Defined by other translation units of this overlay. */
extern void RgGeomFree(RgGeom *geom);
extern void RgGeomResetStatus(RgGeom *geom, unsigned int flags);
extern void RgGeomPassTime(RgGeom *geom, float deltaTime);
extern void RgCharFree(RgChar *pChar);
extern void RgCharPassTimeMethod(RgChar *pChar,
                                 void (*passTimeMethod)(RgChar *pChar, float deltaTime));
extern void RgShotEffectDisp(RgShotEffect *effect);
extern void DisposeRgShotEffect(RgShotEffect *effect);
extern void XrgSoundRingStop(int driver, int soundId);

extern void XrgCopyVector(RgVector destination, RgVector source);
extern void XrgSubVector(RgVector destination, RgVector first, RgVector second);
extern void XrgAddVector(RgVector destination, RgVector first, RgVector second);
extern void XrgScaleVector(RgVector destination, RgVector source, float scale);
extern void XrgNormalizeVector(RgVector destination, RgVector source);
extern float XrgLengthVector(RgVector vector);
extern float XrgInnerVector(RgVector first, RgVector second);
extern void XrgCalcMatrixXtoY(RgMatrix destination, RgVector x_axis,
                              RgVector y_axis);
extern void XrgRotMatrixZ(RgMatrix destination, RgMatrix source, float angle);
extern float XrgRand(float lower, float upper);
extern float cosf(float angle);

extern void RgGeomBallSetRadius(RgGeomBall *geom, float radius);
extern void RgGeomPointSetVel(RgGeomPoint *point, RgVector velocity);
extern void RgGeomPointGetVel(RgGeomPoint *point, RgVector velocity);
extern void RgGeomPointSetPos(RgGeomPoint *point, RgVector position);
extern void RgGeomPointMovePos(RgGeomPoint *point, RgVector position);
extern void __RgGeomPointGetPos(RgGeomPoint *point, RgVector destination,
                                const char *source_file, int source_line);
extern void __RgGeomPointGetOldPos(RgGeomPoint *point, RgVector destination,
                                   const char *source_file, int source_line);

extern void RgShotEffectSetPos(RgShotEffect *effect, RgVector from, RgVector to);
extern void RgShotEffectPassTime(RgShotEffect *effect, float deltaTime);
extern void RgShotEffectSetModel(RgShotEffect *effect, const char *variant);
extern void RgShotEffectSetParticle(RgShotEffect *effect, const char *file);
extern void RgShotEffectSetTexLine(RgShotEffect *effect, const char *file);
extern void RgShotEffectSetTexLineModelSize(RgShotEffect *effect, float width,
                                            float height);
extern void RgShotEffectSetTexLineModelColor(RgShotEffect *effect,
                                             const void *color);
extern void RgShotEffectStopAlive(RgShotEffect *effect, float time);
extern RgParticleEffect *RgShotEffectGetPtclEffect(RgShotEffect *effect);
extern void RgParticleEffectStopShoot(RgParticleEffect *effect);

/* Defined later in this TU (a local sibling still in asm). */
static RgGeom *_CommonGetGeom(RgShot *shot);

RgGeom *_CommonGetGeom(RgShot *shot)
{
    RgGeom *geom;

    if (shot == 0) {
        assert_prog(D_00A532E8, D_00A532F8, 0x58);
    }
    geom = shot->geomTray;
    if (geom == 0) {
        assert_prog(D_00A53310, D_00A532F8, 0x5A);
    }
    return geom;
}

extern void XrgClearVector(RgVector vector);

static int _GetShotPos2(RgShot *shot, RgVector position, RgVector aimPoint)
{
    void *posOwner;

    XrgClearVector(position);
    XrgClearVector(aimPoint);
    posOwner = shot->posOwner;
    if (posOwner == 0) {
        return 0;
    }
    if (shot->getPos == 0) {
        return 0;
    }
    return shot->getPos(posOwner, position, aimPoint);
}

static int _GetShotPosDir(RgShot *shot, RgVector position, RgVector direction)
{
    RgVector shotPosition;
    RgVector aimPoint;

    XrgClearVector(position);
    XrgClearVector(direction);
    if (_GetShotPos2(shot, shotPosition, aimPoint) == 0) {
        return 0;
    }
    XrgCopyVector(position, shotPosition);
    XrgSubVector(direction, aimPoint, shotPosition);
    XrgNormalizeVector(direction, direction);
    return 1;
}

static void _ShotCommonControl(RgChar *pChar)
{
}

static void _ShotCommonPassTimeGeom(RgShot *shot, float deltaTime)
{
    RgGeomPassTime(_CommonGetGeom(shot), deltaTime);
}

static void _ShotCommonPassTimeLife(RgShot *shot, float deltaTime)
{
    float remainingLife;

    remainingLife = shot->life;
    if (remainingLife == 1e8f) {
        return;
    }
    remainingLife -= deltaTime;
    shot->life = remainingLife;
    if (remainingLife < 0.0f) {
        if (shot->hitHistory != 0) {
            shot->hitHistory();
        } else {
            RgCharFree((RgChar *)shot);
            return;
        }
    } else {
        return;
    }
    XrgSoundRingVol(shot->soundDriver, shot->noLifeSoundId,
                    shot->noLifeSoundVolume);
}

static void _ShotCommonPassTimeEffect(RgShot *shot, float deltaTime)
{
    RgVector position;
    RgVector oldPosition;

    __RgGeomPointGetPos((RgGeomPoint *) _CommonGetGeom(shot), position,
                        D_00A532F8, 161);
    __RgGeomPointGetOldPos((RgGeomPoint *) _CommonGetGeom(shot), oldPosition,
                           D_00A532F8, 162);
    RgShotEffectSetPos(shot->effect, oldPosition, position);
    RgShotEffectPassTime(shot->effect, deltaTime);
}

/* Local sibling of this TU still in asm, needed by functions of this allocation. */
static void _ShotCommonPassTimeLife(RgShot *shot, float deltaTime);

static void _ShotCommonPassTime(RgShot *shot, float deltaTime)
{
    _ShotCommonPassTimeGeom(shot, deltaTime);
    _ShotCommonPassTimeLife(shot, deltaTime);
    _ShotCommonPassTimeEffect(shot, deltaTime);
}

static void _ShotCommonDisp(RgShot *shot)
{
    RgShotEffectDisp(shot->effect);
}

static void _ShotCommonDestruct(RgShot *shot)
{
    if (shot->geomTray != 0) {
        RgGeomFree(shot->geomTray);
        shot->geomTray = 0;
    }
    if (shot->effect != 0) {
        DisposeRgShotEffect(shot->effect);
        shot->effect = 0;
    }
    XrgSoundRingStop(shot->soundDriver, shot->hideSoundId);
}

static void _ShotCommonHit(RgShot *shot)
{
    RgGeomResetStatus(shot->geomTray, 1);
    RgCharFree((RgChar *)shot);
}

static int _ShotCommonIsReleased(RgShot *shot)
{
    return 1;
}

extern const char D_00A53320[];
extern RgShotEffect *CreateRgShotEffect(void);
extern RgGeomBall *CreateRgGeomBall(float radius, float weight);
extern RgGeom *RgGeomGroupCreateBall(void *pGroup, RgGeom *parent);
extern void RgCharControlMethod(RgChar *pChar, void (*m)(RgChar *));
extern void RgCharDispMethod(RgChar *pChar, void (*m)(RgChar *));
extern void RgCharDestructMethod(RgChar *pChar, void (*m)(RgChar *));
static void _ShotCommonHitBg(RgShot *shot, int bgObject, RgVector position);
static void _ShotCommonHitRobot(RgShot *shot, int robot, RgVector position);

static void _InitShotCommonSub(RgShot *shot, int id)
{
    if (id == 0) {
        assert_prog(D_00A53320, D_00A532F8, 0xF2);
    }
    RgCharControlMethod((RgChar *)shot, (void (*)(RgChar *))_ShotCommonControl);
    RgCharDispMethod((RgChar *)shot, (void (*)(RgChar *))_ShotCommonDisp);
    RgCharDestructMethod((RgChar *)shot, (void (*)(RgChar *))_ShotCommonDestruct);
    RgCharPassTimeMethod((RgChar *)shot, (void (*)(RgChar *, float))_ShotCommonPassTime);
    shot->notifyHit = (void (*)(RgShot *, RgVector))_ShotCommonHit;
    shot->hitRobot = _ShotCommonHitRobot;
    shot->hitBg = _ShotCommonHitBg;
    shot->isReleased = _ShotCommonIsReleased;
    shot->hitHistory = 0;
    shot->essence = 0;
    shot->life = 0.0f;
    shot->damage = 0.0f;
    shot->hitRobotCount = 0;
    shot->effect = CreateRgShotEffect();
    if (id != 0) {
        shot->geomTray = RgGeomGroupCreateBall((void *)id, (RgGeom *)shot);
    } else {
        shot->geomTray = (RgGeom *)CreateRgGeomBall(0.5f, 1.0f);
    }
    RgGeomBallSetRadius((RgGeomBall *)shot->geomTray, 0.5f);
    RgGeomResetStatus(shot->geomTray, 0x10);
    shot->soundDriver = 0;
    shot->hideSoundId = 0;
    shot->hitSoundId = 0;
    shot->bgHitSoundId = 0;
    shot->noLifeSoundId = 0;
    shot->hitSoundVolume = 0x80;
    shot->bgHitSoundVolume = 0x80;
    shot->noLifeSoundVolume = 0x80;
}

static void _InitShotCommon(void *shot, void *essence, void *info, float speed)
{
    RgShot *typedShot;
    RgShotEssence *typedEssence;
    RgShotRequest *extInfo;
    RgVector position;
    RgVector direction;

    typedShot = shot;
    typedEssence = essence;
    extInfo = info;
    if (typedShot == 0) {
        assert_prog(D_00A532E8, D_00A532F8, 290);
    }
    if (extInfo == 0) {
        assert_prog(D_00A53338, D_00A532F8, 291);
    }
    _InitShotCommonSub(typedShot, extInfo->id);
    typedShot->essence = typedEssence;
    typedShot->life = typedEssence->life;
    typedShot->damage = typedEssence->damage;
    RgShotEffectSetModel(typedShot->effect, typedEssence->modelVariant);
    RgShotEffectSetParticle(typedShot->effect, typedEssence->particleFile);
    RgShotEffectSetTexLine(typedShot->effect, typedEssence->texLineFile);
    RgShotEffectSetTexLineModelSize(typedShot->effect, typedEssence->texLineWidth,
                                    typedEssence->texLineHeight);
    RgShotEffectSetTexLineModelColor(typedShot->effect, typedEssence->texLineColor);
    if (_GetShotPosDir((RgShot *) extInfo, position, direction) != 0) {
        XrgScaleVector(direction, direction, speed);
        RgGeomPointSetPos((RgGeomPoint *) typedShot->geomTray, position);
        RgGeomPointSetVel((RgGeomPoint *) typedShot->geomTray, direction);
        return;
    }
    XrgClearVector(position);
}

static void _InitRgShotEssence(RgShotEssence *essence)
{
    essence->life = 0.0f;
    essence->modelVariant[0] = 0;
    essence->particleFile[0] = 0;
    essence->texLineFile[0] = 0;
    essence->texLineWidth = 1.0f;
    essence->texLineHeight = 0.2f;
    *(int *)&essence->texLineColor[8] = 0x80;
    *(int *)&essence->texLineColor[4] = 0x80;
    *(int *)&essence->texLineColor[0] = 0x80;
    *(int *)&essence->texLineColor[12] = 0x7F;
    essence->damage = 5.0f;
    essence->hitEffectFile[0] = 0;
    essence->hitEffectFile[0x20] = 0;
    *(int *)((char *)essence + 0x110) = 0;
}

/* Defined earlier in this TU (a local sibling still in asm). */
static void _InitShotCommon(void *shot, void *essence, void *info, float speed);

static void *_CreateRgNormalShot(RgNormalShotEssence *essence, void *info)
{
    void *shot;

    shot = RgCharAlloc(0x70, 3);
    if (shot == 0) {
        assert_prog(D_00A532E8, D_00A532F8, 348);
    }
    _InitShotCommon(shot, essence, info, essence->speed);
    return shot;
}

void InitRgNormalShotEssence(RgNormalShotEssence *essence)
{
    if (essence == 0) {
        assert_prog(D_00A53348, D_00A532F8, 0x164);
    }
    _InitRgShotEssence(&essence->common);
    essence->common.createFunc = (void *(*)(void *, void *)) _CreateRgNormalShot;
    essence->speed = 45.0f;
}

static void _HomingPassTime(RgHomingShot *shot, float deltaTime)
{
    RgVector targetPosition;
    RgVector ownPosition;
    RgVector ownVelocity;
    RgVector aimDirection;
    RgMatrix aimBasis;
    float delay;

    if (shot == 0) {
        assert_prog(D_00A532E8, D_00A532F8, 0x189);
    }
    _ShotCommonPassTime(&shot->common, deltaTime);
    delay = shot->delay - deltaTime;
    shot->delay = delay;
    if (delay < 0.0f) {
        __RgGeomPointGetPos((RgGeomPoint *) _CommonGetGeom(&shot->common),
                            ownPosition, D_00A532F8, 0x19B);
        RgGeomPointGetVel((RgGeomPoint *) _CommonGetGeom(&shot->common),
                          ownVelocity);
        __RgGeomPointGetPos((RgGeomPoint *) shot->target, targetPosition,
                            D_00A532F8, 0x19D);
        if (shot->randomTargeting != 0) {
            targetPosition[0] = ownPosition[0] + XrgRand(-50.0f, 50.0f);
            targetPosition[1] = ownPosition[1] + XrgRand(-50.0f, 50.0f);
            targetPosition[2] = ownPosition[2] + XrgRand(-50.0f, 50.0f);
        }
        XrgSubVector(aimDirection, targetPosition, ownPosition);
        XrgCalcMatrixXtoY(aimBasis, ownVelocity, aimDirection);
        if (!(shot->cosThreshold < XrgInnerVector(aimBasis, aimBasis + 4))) {
            XrgRotMatrixZ(aimBasis, aimBasis, shot->turnRate * deltaTime);
            XrgScaleVector(ownVelocity, aimBasis, shot->speed);
            RgGeomPointSetVel((RgGeomPoint *) _CommonGetGeom(&shot->common),
                              ownVelocity);
        }
    }
}

static void _InitHoming(void *shot, void *essence, void *info)
{
    RgHomingShot *typedShot;
    RgHomingShotEssence *typedEssence;
    RgShotRequest *typedInfo;
    float turnRate;
    float cosThreshold;
    float delay;
    float speed;
    RgGeom *target;
    int randomTargeting;

    typedShot = shot;
    typedEssence = essence;
    typedInfo = info;
    _InitShotCommon(&typedShot->common, &typedEssence->common, info,
                    typedEssence->speed);
    RgCharPassTimeMethod((RgChar *) typedShot,
                         (void (*)(RgChar *, float)) _HomingPassTime);
    turnRate = typedEssence->turnRate;
    typedShot->turnRate = turnRate;
    target = typedInfo->target;
    randomTargeting = typedInfo->targetConfused;
    cosThreshold = typedEssence->cosThreshold;
    typedShot->randomTargeting = randomTargeting;
    typedShot->cosThreshold = cosThreshold;
    typedShot->target = target;
    delay = typedEssence->delay;
    typedShot->delay = delay;
    speed = typedEssence->speed;
    typedShot->speed = speed;
}

/* Defined earlier in this TU (a local sibling still in asm). */
static void _InitHoming(void *shot, void *essence, void *info);

static void *_CreateHoming(void *essence, void *info)
{
    void *shot;

    shot = RgCharAlloc(0x90, 3);
    if (shot == 0) {
        assert_prog(D_00A532E8, D_00A532F8, 467);
    }
    if (essence == 0) {
        assert_prog(D_00A53348, D_00A532F8, 468);
    }
    if (info == 0) {
        assert_prog(D_00A53358, D_00A532F8, 469);
    }
    _InitHoming(shot, essence, info);
    return shot;
}

void InitRgHomingShotEssence(void *essence)
{
    RgHomingShotEssence *typedEssence;
    float cosThreshold;

    typedEssence = essence;
    if (typedEssence == 0) {
        assert_prog(D_00A53348, D_00A532F8, 479);
    }
    _InitRgShotEssence(&typedEssence->common);
    typedEssence->common.createFunc = _CreateHoming;
    typedEssence->common.life = 2.0f;
    typedEssence->speed = 30.0f;
    typedEssence->turnRate = 0.7853982f;
    cosThreshold = cosf(0.19634955f);
    typedEssence->cosThreshold = cosThreshold;
    typedEssence->delay = 0.3f;
}

typedef struct {
    unsigned char pad[0xA0];
    int history[8];
    unsigned int count;
} RgGrenadeHistSub;
typedef struct {
    int head;
    RgGrenadeHistSub sub;
} RgGrenadeHistView;

static int _GrenadeEntryHistory(RgShot *shot, int bgObject)
{
    RgGrenadeHistView *bom = (RgGrenadeHistView *)shot;
    unsigned int i;

    if (shot == 0) {
        assert_prog(D_00A53368, D_00A532F8, 0x20A);
    }
    if (bgObject == 0) {
        return 0;
    }
    if (bom->sub.count >= 8) {
        return 0;
    }
    for (i = 0; i < bom->sub.count; i++) {
        if (bom->sub.history[i] == bgObject) {
            return 0;
        }
    }
    {
        RgGrenadeHistSub *sub = &bom->sub;
        unsigned int *cnt = &sub->count;
        unsigned int n = *cnt;
        *cnt = n + 1;
        sub->history[n] = bgObject;
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov12/rg_shot", _GrenadeBomPassTime);

/* Defined later in this TU (a local sibling still in asm). */
static void _GrenadeSetBom(RgShot *shot);

static void _GrenadeShotZeroLife(RgShot *shot)
{
    _GrenadeSetBom(shot);
}

static void _GrenadeHomingHit(RgShot *shot, RgVector position)
{
    _GrenadeSetBom(shot);
}

static void _GrenadeBomHit(RgShot *shot, RgVector position)
{
}

extern RgGeomPoint *RgRobotGetGeom(int robot);
extern unsigned int RgRobotGetStatusFlags(int robot);
extern void RgRobotSetConfuse(int robot, float confuse);
extern void RgGeomPointAddForce(RgGeomPoint *point, RgVector force);

static void _GrenadeHitRobot(RgShot *shot, int robot, RgVector position)
{
    RgGrenadeShot *bom = (RgGrenadeShot *)shot;
    RgGeomPoint *geom;
    RgVector robotPosition;
    RgVector force;

    if (_GrenadeEntryHistory(shot, robot) != 0) {
        if ((float)bom->repeatCount <= 0.0f) {
            _ShotCommonHitRobot(shot, robot, position);
            geom = RgRobotGetGeom(robot);
            __RgGeomPointGetPos(geom, robotPosition, D_00A532F8, 0x27E);
            __RgGeomPointGetPos((RgGeomPoint *)_CommonGetGeom(shot), force, D_00A532F8, 0x27F);
            XrgSubVector(force, robotPosition, force);
            force[1] = 0.0f;
            XrgNormalizeVector(force, force);
            XrgScaleVector(force, force, 1500000.0f);
            RgGeomPointAddForce(geom, force);
        } else if (!(RgRobotGetStatusFlags(robot) & 0x80)) {
            RgRobotSetConfuse(robot, (float)bom->repeatCount);
        }
    }
}

/* Defined later in this TU (a local sibling still in asm). */
static void _ShotCommonHitBg(RgShot *shot, int bgObject, RgVector position);

static void _GrenadeHitBg(RgShot *shot, int bgObject, RgVector position)
{
    if (_GrenadeEntryHistory(shot, bgObject) != 0) {
        _ShotCommonHitBg(shot, bgObject, position);
    }
}

static void _GrenadeSetBom(RgShot *shot)
{
    RgGrenadeShot *typedShot;
    RgGeom *geomTray;
    RgVector velocity;
    RgVector position;
    float hitEffectTrigger;
    RgParticleEffect *particleEffect;

    typedShot = (RgGrenadeShot *) shot;
    geomTray = shot->geomTray;
    RgCharPassTimeMethod((RgChar *) shot,
                         (void (*)(RgChar *, float)) _GrenadeBomPassTime);
    shot->notifyHit = _GrenadeBomHit;
    shot->life = 1e8f;
    shot->hitHistory = 0;
    RgGeomBallSetRadius((RgGeomBall *) geomTray, 0.0f);
    XrgClearVector(velocity);
    RgGeomPointSetVel((RgGeomPoint *) geomTray, velocity);
    RgShotEffectSetModel(shot->effect, D_00A53378);
    hitEffectTrigger = (float) typedShot->repeatCount;
    if (hitEffectTrigger > 0.0f) {
        __RgGeomPointGetPos((RgGeomPoint *) geomTray, position, D_00A532F8, 693);
        CreateRgHitEffectPos(position, shot->essence->hitEffectFile, D_00A53380);
        RgShotEffectSetModel(shot->effect, D_00A53378);
    }
    particleEffect = RgShotEffectGetPtclEffect(shot->effect);
    if (particleEffect != 0) {
        RgParticleEffectStopShoot(particleEffect);
    }
}

static void _GrenadeHomingPassTime(RgGrenadeShot *bom, float deltaTime)
{
    RgVector targetPosition;
    RgVector diff;
    RgGeom *target;

    if (bom == 0) {
        assert_prog(D_00A53368, D_00A532F8, 710);
    }
    _HomingPassTime(&bom->common, deltaTime);
    if (bom->proximity > 0.0f) {
        target = bom->common.target;
        if (target != 0) {
            __RgGeomPointGetPos((RgGeomPoint *) target, targetPosition,
                                D_00A532F8, 718);
            __RgGeomPointGetPos((RgGeomPoint *) _CommonGetGeom(&bom->common.common),
                                diff, D_00A532F8, 719);
            XrgSubVector(diff, diff, targetPosition);
            if (XrgLengthVector(diff) < bom->proximity) {
                _GrenadeSetBom(&bom->common.common);
            }
        }
    }
}

static void _GrenadeHitRobot(RgShot *shot, int robot, RgVector position);

static void _InitGrenade(void *shotp, void *essencep, void *info)
{
    RgGrenadeShot *shot = (RgGrenadeShot *)shotp;
    RgGrenadeEssence *essence = (RgGrenadeEssence *)essencep;
    int *w = (int *)shotp;
    float r;
    float *f = (float *)shotp;

    _InitHoming(shot, essence, info);
    RgCharPassTimeMethod((RgChar *)shot,
                         (void (*)(RgChar *, float))_GrenadeHomingPassTime);
    shot->explosionRadius = essence->explosionRadius;
    shot->proximity = essence->proximity;
    shot->common.common.notifyHit = _GrenadeHomingHit;
    shot->duration = essence->duration;
    f[0x26] = 0.0f;
    f[0x28] = 0.0f;
    shot->common.common.hitRobot = _GrenadeHitRobot;
    r = essence->repeatCount;
    shot->common.common.hitBg = _GrenadeHitBg;
    shot->common.common.hitHistory = (void (*)(void))_GrenadeShotZeroLife;
    f[0x31] = 0.0f;
    shot->repeatCount = (unsigned int)r;
}

static void *_CreateGrenade(void *essence, void *info)
{
    void *bom;

    bom = RgCharAlloc(0xD0, 3);
    if (bom == 0) {
        assert_prog(D_00A53368, D_00A532F8, 769);
    }
    if (essence == 0) {
        assert_prog(D_00A53390, D_00A532F8, 770);
    }
    if (info == 0) {
        assert_prog(D_00A53358, D_00A532F8, 771);
    }
    _InitGrenade(bom, essence, info);
    return bom;
}

/* Defined earlier in this TU (a local sibling still in asm). */
extern void InitRgHomingShotEssence(void *essence);

/* Defined earlier in this TU (a local sibling still in asm). */
static void *_CreateGrenade(void *essence, void *info);

void InitRgGrenadeEssence(RgGrenadeEssence *essence)
{
    InitRgHomingShotEssence(essence);
    essence->createFunc = _CreateGrenade;
    essence->explosionRadius = 10.0f;
    essence->duration = 1.0f;
    essence->proximity = 0.0f;
    essence->repeatCount = 0.0f;
}

static void _FireKill(RgShot *shot)
{
    RgFireShot *typedShot;

    if (shot == 0) {
        assert_prog(D_00A533A0, D_00A532F8, 813);
    }
    typedShot = (RgFireShot *) shot;
    typedShot->status = 2;
    if (typedShot->quickFade != 0) {
        typedShot->fadeTime = 0.0f;
    } else {
        typedShot->fadeTime = 1.0f;
    }
    RgShotEffectStopAlive(shot->effect, typedShot->fadeTime);
}

extern void RgGeomSetStatus(RgGeom *geom, unsigned int flags);
extern void RgError(const char *message, const char *source_file, int line, ...);
extern const char D_00A533B0[];

static void _FirePassTime(RgChar *pChar, float deltaTime)
{
    RgShot *shot = (RgShot *)pChar;
    RgFireShot *fire = (RgFireShot *)pChar;
    char *f = (char *)pChar;
    RgGeom *geom;
    RgVector position;
    RgVector direction;
    unsigned int status;
    float t;

    if (pChar == 0) {
        assert_prog(D_00A533A0, D_00A532F8, 0x33F);
    }
    geom = _CommonGetGeom(shot);
    _ShotCommonPassTimeGeom(shot, deltaTime);
    _ShotCommonPassTimeLife(shot, deltaTime);
    status = fire->status;
    switch (status) {
    case 0:
        RgGeomSetStatus(geom, 1);
        fire->status = 1;
    case 1:
        shot->damage = *(float *)(f + 0xA8) * deltaTime;
        if (_GetShotPosDir((RgShot *)(f + 0xC0), position, direction) != 0) {
            XrgCopyVector((float *)(f + 0x80), position);
            XrgNormalizeVector(direction, direction);
            XrgScaleVector(direction, direction, *(float *)(f + 0xA0));
            XrgAddVector((float *)(f + 0x90), position, direction);
        } else {
            _FireKill(shot);
        }
        t = *(float *)(f + 0xA4) - deltaTime;
        *(float *)(f + 0xA4) = t;
        if (t < 0.0f) {
            _FireKill(shot);
        }
        break;
    case 2:
        RgGeomResetStatus(geom, 1);
        fire->fadeTime -= deltaTime;
        if (fire->fadeTime < 0.0f) {
            RgCharFree(pChar);
        }
        break;
    default:
        RgError(D_00A533B0, D_00A532F8, 0x36C, status);
        break;
    }
    RgGeomPointSetPos((RgGeomPoint *)geom, (float *)(f + 0x80));
    RgGeomPointMovePos((RgGeomPoint *)geom, (float *)(f + 0x90));
    RgShotEffectSetPos(shot->effect, (float *)(f + 0x80), (float *)(f + 0x90));
    RgShotEffectPassTime(shot->effect, deltaTime);
}

static int _FireIsReleased(RgShot *shot)
{
    RgFireShot *fire;

    if (shot == 0) {
        assert_prog(D_00A533A0, D_00A532F8, 896);
    }
    fire = (RgFireShot *)shot;
    return (unsigned int)fire->status >= 2;
}

/* Defined earlier in this TU (a local sibling still in asm). */
static void _FireKill(RgShot *shot);

static void _FireRelease(RgShot *shot)
{
    if (shot == 0) {
        assert_prog(D_00A533A0, D_00A532F8, 0x38D);
    }
    if (((RgFireShot *)shot)->status != 2) {
        _FireKill(shot);
    }
}

static void _FireHit(RgShot *shot, RgVector position)
{
}

/* Defined later in this TU (a local sibling still in asm). */
static void _ShotCommonHitRobot(RgShot *shot, int robot, RgVector position);

static void _FireHitRobot(RgShot *shot, int robot, RgVector position)
{
    RgFireShot *fire = (RgFireShot *)shot;

    if (fire->robot == 0 || fire->alreadyHitRobot == 0) {
        _ShotCommonHitRobot(shot, robot, position);
    }
    fire->alreadyHitRobot = 1;
}

extern void RgShotEffectSetParticleShootReverse(RgShotEffect *effect);
static void _FirePassTime(RgChar *pChar, float deltaTime);

static void *_InitFire(void *firep, void *essencep, void *infop)
{
    RgShot *shot = (RgShot *)firep;
    RgFireShot *fire = (RgFireShot *)firep;
    RgFireEssence *essence = (RgFireEssence *)essencep;
    int *info = (int *)infop;
    char *f = (char *)firep;
    char *e = (char *)essencep;
    int i0, i1, i2, i3;

    _InitShotCommon(firep, essencep, infop, 0.0f);
    shot->notifyHit = _FireHit;
    shot->isReleased = _FireIsReleased;
    shot->release = _FireRelease;
    shot->hitRobot = _FireHitRobot;
    RgGeomBallSetRadius((RgGeomBall *)shot->geomTray, 1.0f);
    RgShotEffectSetParticleShootReverse(shot->effect);
    XrgClearVector((float *)(f + 0x80));
    XrgClearVector((float *)(f + 0x90));
    fire->status = 0;
    *(float *)(f + 0xA0) = essence->speed;
    fire->quickFade = essence->trailingCount;
    *(float *)(f + 0xA4) = essence->duration;
    *(float *)(f + 0xA8) = shot->damage;
    fire->robot = *(int *)(e + 0x12C);
    shot->damage = 0.0f;
    shot->life = 1e8f;
    fire->alreadyHitRobot = 0;
    RgCharPassTimeMethod((RgChar *)fire, _FirePassTime);
    i0 = info[0];
    i1 = info[1];
    i2 = info[2];
    i3 = info[3];
    ((void **)f)[0x30] = (void *)i0;
    ((void **)f)[0x31] = (void *)i1;
    ((void **)f)[0x32] = (void *)i2;
    ((void **)f)[0x33] = (void *)i3;
}

/* Defined earlier in this TU (a local sibling still in asm). */
static void *_InitFire(void *fire, void *essence, void *info);

static void *_CreateFire(void *essence, void *info)
{
    void *fire;

    fire = RgCharAlloc(0xE0, 3);
    if (essence == 0) {
        assert_prog(D_00A53390, D_00A532F8, 0x3D3);
    }
    if (info == 0) {
        assert_prog(D_00A53358, D_00A532F8, 0x3D4);
    }
    _InitFire(fire, essence, info);
    return fire;
}

void InitRgFireEssence(void *essence)
{
    RgFireEssence *typedEssence;

    typedEssence = essence;
    if (typedEssence == 0) {
        assert_prog(D_00A53390, D_00A532F8, 990);
    }
    _InitRgShotEssence(&typedEssence->common);
    typedEssence->common.createFunc = _CreateFire;
    typedEssence->common.life = 1e8f;
    typedEssence->speed = 20.0f;
    typedEssence->duration = 2.0f;
    typedEssence->trailingCount = 0;
}

static void _BeamPassTime(RgBeamShot *beam, float deltaTime)
{
    RgVector movement;
    RgVector position;
    RgVector direction;
    RgGeom *geom;
    float damage;
    float remainingLifetime;
    float moveAmount;
    float travelled;
    float collapsedLength;

    if (beam == 0) {
        assert_prog(D_00A533C8, D_00A532F8, 1034);
    }
    _ShotCommonPassTime(&beam->common, deltaTime);
    damage = beam->damageRate * deltaTime;
    remainingLifetime = beam->lifetime - deltaTime;
    moveAmount = beam->speed * deltaTime;
    beam->lifetime = remainingLifetime;
    beam->common.damage = damage;
    if (remainingLifetime > 0.0f) {
        if (_GetShotPosDir((RgShot *) &beam->request, position, direction) != 0) {
            XrgCopyVector(beam->origin, position);
            XrgCopyVector(beam->direction, direction);
            travelled = beam->length + moveAmount;
            beam->length = travelled;
        } else {
            beam->lifetime = 0.0f;
        }
    }
    if (beam->lifetime <= 0.0f) {
        XrgScaleVector(movement, beam->direction, moveAmount);
        XrgAddVector(beam->origin, beam->origin, movement);
    }
    if (beam->hit != 0) {
        beam->hit = 0;
        collapsedLength = beam->length - moveAmount;
        beam->length = collapsedLength;
        if (collapsedLength <= 0.0f) {
            RgCharFree((RgChar *) beam);
            return;
        }
    }
    geom = _CommonGetGeom(&beam->common);
    XrgScaleVector(movement, beam->direction, beam->length);
    XrgAddVector(position, beam->origin, movement);
    RgGeomPointSetPos((RgGeomPoint *) geom, beam->origin);
    RgGeomPointMovePos((RgGeomPoint *) geom, position);
    RgShotEffectSetPos(beam->common.effect, beam->origin, position);
}

static int _BeamIsReleased(RgShot *shot)
{
    if (shot == 0) {
        assert_prog(D_00A533C8, D_00A532F8, 0x44B);
    }
    return ((RgBeamShot *)shot)->lifetime <= 0.0f;
}

static void _BeamHit(RgShot *shot, RgVector position)
{
    RgBeamShot *beam;

    if (shot == 0) {
        assert_prog(D_00A533C8, D_00A532F8, 0x454);
    }
    beam = (RgBeamShot *)shot;
    beam->lifetime = 0.0f;
    beam->hit = 1;
}

/*
 * Partial view of a beam object.  The callback's only evidenced state is the
 * lifetime value at byte offset 0x98; the surrounding object layout remains
 * outside this allocation.
 */

static void _BeamRelease(void *beam)
{
    BeamLifetimeField *lifetime_field =
        (BeamLifetimeField *)((unsigned char *)beam + 0x98);

    lifetime_field->lifetime = -0.5f;
}

static void *_InitBeam(void *beamp, void *essencep, void *infop)
{
    RgBeamShot *beam = (RgBeamShot *)beamp;
    RgBeamEssence *essence = (RgBeamEssence *)essencep;
    int *info = (int *)infop;
    RgVector a;
    RgVector b;
    int i0, i1, i2, i3;
    int *bw = (int *)beamp;

    _InitShotCommon(beam, essence, info, essence->speed);
    beam->common.notifyHit = _BeamHit;
    beam->common.isReleased = _BeamIsReleased;
    beam->common.release = (void (*)(RgShot *))_BeamRelease;
    RgGeomBallSetRadius((RgGeomBall *)beam->common.geomTray, 1.0f);
    XrgCopyVector(beam->origin, a);
    XrgCopyVector(beam->direction, b);
    beam->speed = essence->speed;
    beam->length = 0.0f;
    beam->lifetime = essence->duration;
    beam->hit = 0;
    beam->damageRate = beam->common.damage;
    beam->common.damage = 0.0f;
    RgCharPassTimeMethod((RgChar *)beam, (void (*)(RgChar *, float))_BeamPassTime);
    i0 = info[0];
    i1 = info[1];
    i2 = info[2];
    i3 = info[3];
    ((void **)bw)[0x2C] = (void *)i0;
    ((void **)bw)[0x2D] = (void *)i1;
    ((void **)bw)[0x2E] = (void *)i2;
    ((void **)bw)[0x2F] = (void *)i3;
}

static void *_CreateBeam(void *essence, void *info)
{
    void *beam;

    beam = RgCharAlloc(0xD0, 3);
    if (essence == 0)
        assert_prog(D_00A53390, D_00A532F8, 1166);
    if (info == 0)
        assert_prog(D_00A53358, D_00A532F8, 1167);
    _InitBeam(beam, essence, info);
    return beam;
}

void InitRgBeamEssence(void *essence)
{
    RgBeamEssence *typedEssence;

    typedEssence = essence;
    if (typedEssence == 0) {
        assert_prog(D_00A53348, D_00A532F8, 1177);
    }
    _InitRgShotEssence(&typedEssence->common);
    typedEssence->common.createFunc = _CreateBeam;
    typedEssence->common.life = 99.0f;
    typedEssence->speed = 40.0f;
    typedEssence->duration = 2.0f;
}

void *CreateRgShotFromEssence(RgShotEssence *essence, void *info)
{
    if (essence == 0) {
        assert_prog(D_00A53390, D_00A532F8, 0x4AB);
    }
    return essence->createFunc(essence, info);
}

void RgShotSetSoundDriver(RgShot *shot, int driver)
{
    if (shot == 0)
        assert_prog(D_00A532E8, D_00A532F8, 1206);
    shot->soundDriver = driver;
}

void RgShotSetHitSound(RgShot *shot, int soundId, int volume)
{
    if (shot == 0)
        assert_prog(D_00A532E8, D_00A532F8, 1214);
    shot->hitSoundId = soundId;
    shot->hitSoundVolume = volume;
}

void RgShotSetBgHitSound(RgShot *shot, int soundId, int volume)
{
    if (shot == 0)
        assert_prog(D_00A532E8, D_00A532F8, 1223);
    shot->bgHitSoundId = soundId;
    shot->bgHitSoundVolume = volume;
}

void RgShotSetNoLifeSound(RgShot *shot, int soundId, int volume)
{
    if (shot == 0)
        assert_prog(D_00A532E8, D_00A532F8, 1232);
    shot->noLifeSoundId = soundId;
    shot->noLifeSoundVolume = volume;
}

void RgShotSetHideSound(RgShot *shot, int soundId)
{
    if (shot == 0)
        assert_prog(D_00A532E8, D_00A532F8, 1241);
    shot->hideSoundId = soundId;
}

float RgShotGetDamage(RgShot *shot)
{
    if (shot == 0)
        assert_prog(D_00A532E8, D_00A532F8, 1252);
    return shot->damage;
}

int RgShotIsReleased(RgShot *shot)
{
    if (shot == 0) {
        assert_prog(D_00A532E8, D_00A532F8, 1260);
    }
    if (shot->isReleased != 0) {
        return shot->isReleased(shot);
    }
    return 1;
}

int RgShotGetHitRobNum(RgShot *shot)
{
    if (shot == 0)
        assert_prog(D_00A532E8, D_00A532F8, 1271);
    return shot->hitRobotCount;
}

void RgShotRelease(RgShot *shot)
{
    if (shot == 0)
        assert_prog(D_00A532E8, D_00A532F8, 1283);
    if (shot->release != 0)
        shot->release(shot);
}

void RgShotHitRobot(RgShot *shot, int robot, RgVector position)
{
    if (shot == 0)
        assert_prog(D_00A532E8, D_00A532F8, 1292);
    if (robot == 0)
        assert_prog(D_00A533D8, D_00A532F8, 1293);
    shot->hitRobotCount++;
    if (shot->hitRobot != 0)
        shot->hitRobot(shot, robot, position);
    if (shot->notifyHit != 0)
        shot->notifyHit(shot, position);
}

void RgShotHitBg(RgShot *shot, int bgObject, RgVector position)
{
    if (shot == 0)
        assert_prog(D_00A532E8, D_00A532F8, 1306);
    if (bgObject == 0)
        assert_prog(D_00A533E8, D_00A532F8, 1307);
    if (shot->hitBg != 0)
        shot->hitBg(shot, bgObject, position);
    if (shot->notifyHit != 0)
        shot->notifyHit(shot, position);
}

extern const char D_00A533F8[];
extern const char D_00A53408[];
extern void CreateRgHitEffectPosDir(RgVector position, RgVector direction, char *name1, char *name2);
extern void RgRobotGiveDamage(int robot, RgVector direction, int flags, float damage);
extern void RgRobotGiveWeakDamage(int robot, RgVector direction, float damage,
                                  float weak0, float weak1, float weak2, float weak3);
extern void RgRobotInvalidAttack(int robot, RgVector direction, float damage);
extern int RgRobotIsInvalidAttack(int robot, RgVector direction);

static void _ShotCommonHitRobot(RgShot *shot, int robot, RgVector position)
{
    RgVector direction;
    RgVector robotPosition;
    RgShotEssence *essence;

    if (shot == 0)
        assert_prog(D_00A532E8, D_00A532F8, 0x52C);
    if (robot == 0)
        assert_prog(D_00A533F8, D_00A532F8, 0x52D);
    __RgGeomPointGetOldPos((RgGeomPoint *)_CommonGetGeom(shot), direction, D_00A532F8, 0x530);
    __RgGeomPointGetPos(RgRobotGetGeom(robot), robotPosition, D_00A532F8, 0x531);
    XrgSubVector(direction, direction, robotPosition);
    if (RgRobotIsInvalidAttack(robot, direction) == 0) {
        essence = shot->essence;

        if (essence != 0 && ((float *)essence)[0x44] > 0.0f) {
            RgRobotGiveWeakDamage(robot, direction, shot->damage, ((float *)essence)[0x44],
                                  ((float *)essence)[0x45], ((float *)essence)[0x46],
                                  ((float *)essence)[0x47]);
        } else {
            RgRobotGiveDamage(robot, direction, 1, shot->damage);
        }
        XrgSoundRingVol(shot->soundDriver, shot->hitSoundId, shot->hitSoundVolume);
        CreateRgHitEffectPosDir(position, direction, shot->essence->hitEffectFile + 0x20, (char *)D_00A53408);
    } else {
        CreateRgHitEffectPosDir(position, direction, shot->essence->hitEffectFile, (char *)D_00A53380);
        RgRobotInvalidAttack(robot, direction, shot->damage);
    }
}

static void _ShotCommonHitBg(RgShot *shot, int bgObject, RgVector position)
{
    if (shot == 0)
        assert_prog(D_00A532E8, D_00A532F8, 1365);
    if (bgObject == 0)
        assert_prog(D_00A533E8, D_00A532F8, 1366);
    if (RgBgObjTryToBreak(bgObject, shot->damage) != 0)
        XrgSoundRingVol(shot->soundDriver, shot->bgHitSoundId, shot->bgHitSoundVolume);
    CreateRgHitEffectPos(position, shot->essence->hitEffectFile, D_00A53380);
}

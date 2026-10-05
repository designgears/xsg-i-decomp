/*
 * OV12 original TU 4: 0x00a02510..0x00a09480 (153 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_robot.h"

/* RgShotThread is defined in tu011 and stays opaque at this call site. */
struct RgShotThread;
extern void RgShotThreadSleep(struct RgShotThread *thread);

/*
 * These are external file-backed witnesses in the TU's still asm-owned
 * .rodata (config/tu/ov12/tu004.json data_ownership), not candidate-emitted
 * data. They keep their splat names per docs/naming.md ("Scaffold-owned data
 * keeps its splat name") until that data becomes C-owned.
 *
 * ov12:0x00a51840 (D_00A51840), 18 bytes, SHA-256
 * b8693e915c6e9d84b2f020600f866bdc3ba67025729e8a7566ebdf621f4380ca
 * contains the source filename "../rg_robot.euc.c".
 * ov12:0x00a51bb0 (D_00A51BB0), 14 bytes, SHA-256
 * d7e89b8cbef744999f0635dbf5f1111a85f0daeb1ad5c69e912cc14f4c1096a9
 * contains the assertion expression "pRobot != NIL".
 * ov12:0x00a51be0 (D_00A51BE0), 60 bytes, SHA-256
 * 1eafc9773d62673cff56b70f703e51d2bc76ff912cea41f18140919845228eb2
 * contains the range expression
 * "RG_EQUIP_TYPE_MIN <= (eSide) && (eSide) < RG_EQUIP_TYPE_NUM".
 */
extern void assert_prog(const char *expression, const char *source_file,
                        int line);
extern const char D_00A51840[];
extern const char D_00A51BB0[];
extern const char D_00A51BE0[];

/*
 * ov12:0x00a51830 (D_00A51830) contains the assertion expression
 * "pThread != NIL".
 * ov12:0x00a518f0 (D_00A518F0) contains the assertion expression
 * "pStatus != NIL".
 * ov12:0x00a51858 (D_00A51858) contains the assertion expression
 * "fBaseSpeed > RG_FCONST(0.0)".
 */
extern const char D_00A51830[];
extern const char D_00A518F0[];
extern const char D_00A51858[];

extern RgHeap *InstanceOfRgHeap(void);
extern void *RgHeapAlloc(void *heap, unsigned int size,
                         const char *source_file, int line);
extern void RgHeapFree(void *heap, void *pointer, const char *source_file,
                       int line);
extern void XrgSoundRing(int soundID, int count);
extern float RgGeomPointGetWeight(RgGeomPoint *point);

static void _InitHomingThread(RgHomingThread *thread);

static void _InitHomingThread(RgHomingThread *thread)
{
    if (thread == 0) {
        assert_prog(D_00A51830, D_00A51840, 73);
    }
    thread->geometry = 0;
    thread->target = 0;
    thread->actionTime = 0;
    thread->sleepTime = 0;
    *(float *)thread->unmodeled_10 = 12.566371f;
    thread->state = 0;
}

static RgHomingThread *_CreateHomingThread(void)
{
    RgHomingThread *thread;

    thread = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgHomingThread),
                          D_00A51840, 85);
    _InitHomingThread(thread);
    return thread;
}

static void _DisposeHomingThread(RgHomingThread *thread)
{
    if (thread == 0) {
        assert_prog(D_00A51830, D_00A51840, 93);
    }
    RgHeapFree(InstanceOfRgHeap(), thread, D_00A51840, 94);
}

static void _StartHomingThread(RgHomingThread *thread, RgGeomPoint *geometry,
                               void *target, float actionTime)
{
    if (thread == 0) {
        assert_prog(D_00A51830, D_00A51840, 100);
    }
    thread->actionTime = actionTime;
    thread->target = target;
    thread->geometry = geometry;
    thread->sleepTime = 0;
    if (geometry == 0) {
        thread->state = 0;
        return;
    }
    thread->state = 1;
}

static void _StopHomingThread(RgHomingThread *thread)
{
    if (thread == 0) {
        assert_prog(D_00A51830, D_00A51840, 114);
    }
    if (thread->state == 1) {
        thread->sleepTime = 0;
        thread->state = 2;
    }
}

static int _GetStatHomingThread(RgHomingThread *thread)
{
    if (thread == 0) {
        assert_prog(D_00A51830, D_00A51840, 124);
    }
    return thread->state;
}

static float _GetSleepTimeHomingThread(RgHomingThread *thread)
{
    float sleepTime;

    if (thread == 0) {
        assert_prog(D_00A51830, D_00A51840, 131);
    }
    sleepTime = 99999.9f;
    if (thread->state == 2) {
        sleepTime = thread->sleepTime;
    }
    return sleepTime;
}

extern float RgRobSubHoming(RgGeomPoint *geometry, RgGeomPoint *targetGeometry,
                            float speed, float turnRate);

void _PassTimeHomingThread(RgHomingThread *thread, float deltaTime)
{
    float t;

    if (thread == 0) {
        assert_prog(D_00A51830, D_00A51840, 0x8D);
    }
    switch (thread->state) {
    case 1:
        t = thread->actionTime - deltaTime;
        thread->actionTime = t;
        if (t <= 0.0f) {
            thread->actionTime = 0.0f;
            thread->state = 2;
        }
        RgRobSubHoming(thread->geometry, (RgGeomPoint *)thread->target,
                       *(float *)&thread->unmodeled_10[0], deltaTime);
        return;
    case 2:
        thread->sleepTime = thread->sleepTime + deltaTime;
        break;
    }
}

static float _CalcMaxSpeed(RgBody *body, float baseSpeed)
{
    float speedRating;
    float weight;

    speedRating = body->spec->speedRating;
    weight = RgGeomPointGetWeight(body->geometry);
    if (!(baseSpeed > 0.0f)) {
        assert_prog(D_00A51858, D_00A51840, 247);
    }
    return (baseSpeed * speedRating) / weight;
}

static void _StopAllShotThread(RgBody *body)
{
    unsigned int eSide;

    for (eSide = 0; eSide < RG_EQUIP_TYPE_NUM; eSide++) {
        RgShotThreadSleep(body->shotThread[eSide]);
    }
}

static void _BodyOnGround(RgBody *body)
{
    if (body->grounded == 0) {
        XrgSoundRing(body->landingSoundID, 0x32);
    }
    body->grounded = 1;
}

static void _InitStatus(RgRobotStatus *status)
{
    if (status == 0) {
        assert_prog(D_00A518F0, D_00A51840, 611);
    }
    status->type = 0;
    status->elapsedTime = 0;
    status->execCmdMethod = 0;
    status->passTimeMethod = 0;
    status->dispMethod = 0;
    status->exitMethod = 0;
}

static RgRobotStatus *_CreateStatue(void)
{
    RgRobotStatus *status;

    status = RgHeapAlloc(InstanceOfRgHeap(), 0x60U, D_00A51840, 624);
    _InitStatus(status);
    return status;
}

static void _SetStatus(RgRobotStatus *status, int type, RgBody *body)
{
    RgStatusExitFunc exitMethod;

    if (status == 0) {
        assert_prog(D_00A518F0, D_00A51840, 633);
    }
    exitMethod = status->exitMethod;
    if (exitMethod != 0) {
        exitMethod(status, body);
    }
    _InitStatus(status);
    status->type = type;
}

static void _DisposeStatus(RgRobotStatus *status)
{
    if (status == 0) {
        assert_prog(D_00A518F0, D_00A51840, 647);
    }
    RgHeapFree(InstanceOfRgHeap(), status, D_00A51840, 648);
}

static int _StatusExecCmd(RgRobotStatus *status, RgBody *body, int param)
{
    RgStatusExecCmdFunc execCmdMethod;

    if (status == 0) {
        assert_prog(D_00A518F0, D_00A51840, 657);
    }
    execCmdMethod = status->execCmdMethod;
    if (execCmdMethod != 0) {
        return execCmdMethod(status, param, body);
    }
    return 0;
}

static int _StatusPassTime(RgRobotStatus *status, RgBody *body, float dt)
{
    RgStatusPassTimeFunc passTimeMethod;
    int result;

    result = 0;
    if (status == 0) {
        assert_prog(D_00A518F0, D_00A51840, 668);
    }
    passTimeMethod = status->passTimeMethod;
    if (passTimeMethod != 0) {
        result = passTimeMethod(status, body, dt);
    }
    status->elapsedTime += dt;
    return result;
}

static void _StatusDisp(RgRobotStatus *status, RgBody *body)
{
    RgStatusDispFunc dispMethod;

    if (status == 0) {
        assert_prog(D_00A518F0, D_00A51840, 678);
    }
    dispMethod = status->dispMethod;
    if (dispMethod != 0) {
        dispMethod(status, body);
    }
}

static int _ExecAccelarateCmd(RgStatus *status, RgBody *body, RgCmd *command);
static int _ExecRotateCmd(RgRobotStatus *status, RgBody *body, RgCmd *command);
static int _ExecShotOrAttackCmd(RgRobotStatus *status, RgBody *body, RgCmd *command);
static int _ExecTargettingCmd(RgStatus *status, RgBody *body, RgCmd *command, void *target);
static int _ExecBreakCmd(RgStatus *status, RgBody *body, void *command);
static int _ExecDashCmd(RgRobotStatus *status, RgBody *body, RgCmd *command);
static int _ExecDashContinueCmd(RgRobotStatus *status, RgBody *body, RgCmd *command);
static int _ExecDropWeaponCmd(RgStatus *status, RgBody *body, RgCmd *command);
static int _ExecDamageCmd(RgRobotStatus *status, RgBody *body, RgCmd *command);
static int _ExecWeakDamageCmd(RgRobotStatus *status, RgBody *body, RgCmd *command);
static int _ExecHitByBody(RgRobotStatus *status, RgBody *body, RgCmd *command);

static int _AllowAllCmdInMoving(RgRobotStatus *status, int param, RgBody *cmd)
{
    RgBody *body = (RgBody *)param;
    RgCmd *command = (RgCmd *)cmd;
    int result;

    result = 0;
    switch ((unsigned)command->type) {
    case 0: result = _ExecAccelarateCmd((RgStatus *)status, body, command); break;
    case 1: result = _ExecRotateCmd(status, body, command); break;
    case 2: result = _ExecShotOrAttackCmd(status, body, command); break;
    case 3: result = _ExecTargettingCmd((RgStatus *)status, body, command, (void *)1); break;
    case 4: result = _ExecBreakCmd((RgStatus *)status, body, command); break;
    case 5: result = _ExecDashCmd(status, body, command); break;
    case 6: result = _ExecDashContinueCmd(status, body, command); break;
    case 7: result = _ExecDropWeaponCmd((RgStatus *)status, body, command); break;
    case 8: result = _ExecDamageCmd(status, body, command); break;
    case 9: result = _ExecWeakDamageCmd(status, body, command); break;
    case 14: result = _ExecHitByBody(status, body, command); break;
    }
    return result;
}

static int _ExecHitByBody(RgRobotStatus *status, RgBody *body, RgCmd *command);

static int _AllowDamageCmd(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    int result;

    result = 0;
    switch ((unsigned)command->type) {
    case 8:
        result = _ExecDamageCmd(status, body, command);
        break;
    case 9:
        result = _ExecWeakDamageCmd(status, body, command);
        break;
    case 14:
        result = _ExecHitByBody(status, body, command);
        break;
    }
    return result;
}

static int _ExecDamageCmd(RgRobotStatus *status, RgBody *body, RgCmd *command);
static int _ExecWeakDamageCmd(RgRobotStatus *status, RgBody *body, RgCmd *command);

static int _AllowDamageCmdNotVRDashHit(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    int result;

    result = 0;
    switch (command->type) {
    case 8:
        result = _ExecDamageCmd(status, body, command);
        break;
    case 9:
        result = _ExecWeakDamageCmd(status, body, command);
        break;
    }
    return result;
}

static int _AllowDamageCmdNotPetrify(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    int result;
    float life;

    (void)status;
    result = 0;
    if (command->type != 8) {
        if (command->type == 9) {
            goto damage;
        }
    } else {
damage:
        life = body->life - command->scratch0;
        body->life = life;
        if (life < 0.0f) {
            body->life = 0.0f;
        }
        result = 0x80;
    }
    return result;
}

extern void RgGeomPointGetVel(RgGeomPoint *point, RgVector velocity);
extern float RgGeomRobotGetRotVel(RgGeomPoint *geom);
extern int RgRobSubGetAdvanceMot(RgGeomPoint *geom, int arg);
extern int RgRobSubGetRollMotion(RgGeomPoint *geom);
static void _BodyPlayMotion(RgBody *body, int motion);
static void _BodyPlayMotionLoop(RgBody *body, int motion);

static int _MovingPassTime(RgRobotStatus *status, RgBody *body, float deltaTime)
{
    RgRobotSpec *spec;
    RgVector velocity;
    float speed;
    float rotVel;
    int advance;
    int roll;
    int motion;
    float t;

    spec = body->spec;
    RgGeomPointGetVel(body->geometry, velocity);
    speed = __builtin_sqrtf(velocity[0] * velocity[0] + velocity[2] * velocity[2]);
    rotVel = RgGeomRobotGetRotVel(body->geometry);
    if (rotVel < 0.0f) {
        rotVel = -rotVel;
    }
    advance = RgRobSubGetAdvanceMot(body->geometry, 0);
    roll = RgRobSubGetRollMotion(body->geometry);
    if (speed > 5.0f && advance != -1) {
        motion = advance;
    } else if (rotVel > 0.3926991f && roll != -1) {
        motion = roll;
    } else {
        motion = 0;
    }
    if (motion != status->scratch0) {
        status->scratch0 = motion;
        if (motion == 0) {
            _BodyPlayMotionLoop(body, 0);
        } else {
            _BodyPlayMotion(body, motion);
        }
    }
    if (body->dashTime < *(float *)&spec->unmodeled_30[0]) {
        t = body->dashTime + deltaTime;
        body->dashTime = t;
        if (*(float *)&spec->unmodeled_30[0] < t) {
            body->dashTime = *(float *)&spec->unmodeled_30[0];
        }
    }
    return 0;
}

static int _AllowAllCmdInMoving(RgRobotStatus *status, int param, RgBody *body);
static int _MovingPassTime(RgRobotStatus *status, RgBody *body, float deltaTime);

static void _InitMovingStatus(RgRobotStatus *status, RgBody *body)
{
    _SetStatus(status, 1, body);
    status->execCmdMethod = _AllowAllCmdInMoving;
    status->passTimeMethod = _MovingPassTime;
    status->scratch0 = -1;
}

extern int RgMotionInfoGetActionInTime(int motion, int actionIndex,
                                       float elapsedTime);
extern void RgWeaponPlayMotion(void *weapon, int motion);
static RgRobotStatus *_InitAttackStatus(RgRobotStatus *status, RgBody *body,
                                        int motion, int weapon, int side);
static int _IsBackWeaponReady(RgBody *body, unsigned int eType);

static int _ExecCmdInAttack(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    RgRobotStatus *st;
    int result;
    int action;
    int motion;
    int info;
    float time;

    st = status;
    result = _AllowDamageCmd(status, body, command);
    time = status->elapsedTime;
    info = *(int *)&status->scratchTimer;
    if (result == 0 && command->type == 10) {
        action = RgMotionInfoGetActionInTime(info, 1, time);
        if (action == 0) {
            return action;
        }
        motion = *(int *)(action + 0xC);
        if (motion == -1) {
            return 0;
        }
        if (status->scratch1 != command->param) {
            return 0;
        }
        st = _InitAttackStatus(status, body, motion, status->scratch1,
                               status->eSide);
        if (st != 0) {
            *(int *)&st->unmodeled_10[4] = 0;
            RgWeaponPlayMotion((void *)command->param, *(int *)(action + 0x10));
        }
        result = 0x400;
    }
    if (command->type == 2 && st->eSide != command->type
        && _IsBackWeaponReady(body, command->param) != 0
        && *(int *)&st->unmodeled_10[0] != 0) {
        result |= _ExecShotOrAttackCmd(status, body, command);
    }
    return result;
}

extern int RgWeaponGetControlFlag(void *weapon);
extern int RgWeaponShot(void *weapon);
extern void RgWeaponShotStop(void *weapon);
extern int XrgActorIsEndOfMotion(void *actor);
static void _BodyAdvanceForAttack(RgBody *body, float attackPower,
                                  float attackFactor);

static int _AttackPassTime(RgRobotStatus *status, RgBody *body, float deltaTime)
{
    void *weapon;
    float elapsed;
    int info;
    int flag;

    weapon = (void *)status->scratch1;
    elapsed = status->elapsedTime;
    info = *(int *)&status->scratchTimer;
    if (weapon != 0) {
        flag = RgWeaponGetControlFlag(weapon);
    } else {
        flag = 0;
    }
    if (RgMotionInfoGetActionInTime(info, 0, elapsed) != 0) {
        if (weapon != 0) {
            RgWeaponShot(weapon);
        }
        *(int *)&status->unmodeled_10[0] = 0;
        _StopHomingThread(body->homingThread);
    } else if (XrgActorIsEndOfMotion(body->actor) != 0) {
        if (weapon != 0) {
            RgWeaponShotStop(weapon);
        }
        _InitMovingStatus(status, body);
    } else if (*(int *)&status->unmodeled_10[0] != 0) {
        if (*(int *)&status->unmodeled_10[4] != 0 && (flag & 0x40)) {
            _BodyAdvanceForAttack(body, deltaTime, 1.0f);
        }
    } else {
        if (weapon != 0) {
            RgWeaponShotStop(weapon);
        }
        _StopHomingThread(body->homingThread);
    }
    return 0x20;
}

extern void RgWeaponShotStop(void *weapon);
extern char *RgWeaponGetEss(void *weapon);
extern void RgWeaponPlayMotion(void *weapon, int motion);
extern void RgWeaponRestartLockon(void *weapon);

/*
 * Motion-ID table offset within the weapon-essence record RgWeaponGetEss
 * returns; only this field is evidenced here (docs/style.md, "Struct
 * fields, not offset casts" -- narrowly evidenced scalar access).
 */
#define WEP_ESSENCE_ATTACK_MOTION_OFFSET 0x140

static void _AttackExit(RgRobotStatus *status, RgBody *body)
{
    void *shotWeapon;
    void *motionWeapon;
    char *ess;

    shotWeapon = (void *) status->scratch1;
    if (shotWeapon != 0) {
        RgWeaponShotStop(shotWeapon);
    }
    _StopHomingThread(body->homingThread);
    motionWeapon = (void *) status->scratch1;
    if (motionWeapon != 0) {
        ess = RgWeaponGetEss(motionWeapon);
        if (ess != 0) {
            RgWeaponPlayMotion(motionWeapon,
                *(int *) (ess + WEP_ESSENCE_ATTACK_MOTION_OFFSET
                          + (body->charID * RG_EQUIP_TYPE_NUM + status->eSide) * 4));
        }
    }
    if (status->scratch2 != 0) {
        RgWeaponRestartLockon((void *) status->scratch2);
    }
}

typedef struct RgMotionInfoDB RgMotionInfoDB;

typedef struct RgMotionShotInfo RgMotionShotInfo;

extern RgMotionInfoDB *InstanceOfRgMotionInfoDB(void);

extern RgMotionShotInfo *RgMotionInfoDBGet(RgMotionInfoDB *db, int motionNo,
                                           int charId);

extern void RgWeaponStopLockon(void *weapon);

extern int RgWeaponGetControlFlag(void *weapon);

extern int RgMotionInfoGetActionInTime(int motion, int actionIndex,
                                       float elapsedTime);

static void _BodyPlayMotion(RgBody *body, int motion);

extern const char D_00A51940[];

static int _ExecCmdInAttack();
static int _AttackPassTime(RgRobotStatus *status, RgBody *body, float deltaTime);
extern void RgWeaponPlayShotMotion(void *weapon);
extern int RgWeaponIsTargetting(void *weapon);

static RgRobotStatus *_InitAttackStatus(RgRobotStatus *status, RgBody *body,
                                        int motion, int weapon, int side)
{
    RgMotionShotInfo *info;
    int action;

    info = RgMotionInfoDBGet(InstanceOfRgMotionInfoDB(), motion, body->charID);
    if (info == 0) {
        assert_prog(D_00A51940, D_00A51840, 0x3F1);
    }
    _StopAllShotThread(body);
    if (body->actor == 0) {
        return 0;
    }
    _SetStatus(status, 2, body);
    status->execCmdMethod = _ExecCmdInAttack;
    status->passTimeMethod = _AttackPassTime;
    status->exitMethod = _AttackExit;
    *(RgMotionShotInfo **)&status->scratchTimer = info;
    status->scratch1 = weapon;
    *(int *)&status->unmodeled_10[0] = 1;
    *(int *)&status->unmodeled_10[4] = 1;
    status->eSide = side;
    switch (side) {
    case 0:
        status->scratch2 = (int)body->weapon[1];
        break;
    case 1:
        status->scratch2 = (int)body->weapon[0];
        break;
    default:
        status->scratch2 = 0;
        break;
    }
    if (status->scratch2 != 0) {
        RgWeaponStopLockon((void *)status->scratch2);
    }
    _BodyPlayMotion(body, motion);
    if (weapon != 0) {
        RgWeaponPlayShotMotion((void *)weapon);
        if ((RgWeaponGetControlFlag((void *)weapon) & 0x18) == 8
            && RgWeaponIsTargetting((void *)weapon) != 0) {
            action = RgMotionInfoGetActionInTime((int)info, 0, 1e8f);
            if (action != 0) {
                _StartHomingThread(body->homingThread, body->geometry,
                                   body->target, *(float *)(action + 4));
            }
        }
    }
    return status;
}

static int _AllowDamageCmd(RgRobotStatus *status, RgBody *body, RgCmd *command);

static int _ExecCmdInHissatu(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    return _AllowDamageCmd(status, body, command);
}

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _HissatuPassTime);

extern void RgGeomPointSetMaxXYSpd(RgGeomPoint *point, float maxSpeed);
extern void RgRobotEffectTermJet(RgRobotEffect *effect);
extern void RgRobotEffectTermDash(RgRobotEffect *effect);

static void _HissatuExit(RgRobotStatus *status, RgBody *body)
{
    float maxSpd;
    void *w0;
    void *w1;

    w0 = body->weapon[0];
    w1 = body->weapon[1];
    maxSpd = _CalcMaxSpeed(body, body->spec->baseSpeed);
    if (w0) {
        RgWeaponRestartLockon(w0);
        RgWeaponShotStop(w0);
    }
    if (w1) {
        RgWeaponRestartLockon(w1);
        RgWeaponShotStop(w1);
    }
    _StopHomingThread(body->homingThread);
    RgGeomPointSetMaxXYSpd(body->geometry, maxSpd);
    RgRobotEffectTermDash(body->effect);
    RgRobotEffectTermJet(body->effect);
}

static int _HissatuPassTime(RgRobotStatus *status, RgBody *body,
                            float deltaTime);
static void _HissatuExit(RgRobotStatus *status, RgBody *body);
static void _InitStatusHissatu(RgRobotStatus *status, RgBody *body)
{
    int motionNo = 0x4E;
    RgMotionInfoDB *db = InstanceOfRgMotionInfoDB();
    RgMotionShotInfo *info = RgMotionInfoDBGet(db, motionNo, body->charID);
    _SetStatus(status, 0xA, body);
    status->execCmdMethod = (RgStatusExecCmdFunc)_ExecCmdInHissatu;
    status->scratch0 = 0;
    status->passTimeMethod = (RgStatusPassTimeFunc)_HissatuPassTime;
    status->exitMethod = (RgStatusExitFunc)_HissatuExit;
    status->scratchTimer = 0.0f;
    *(int *)&status->unmodeled_10[0] = 0;
    *(int *)&status->unmodeled_10[4] = 1;
    *(int *)&status->unmodeled_1c[0] = 0;
    status->eSide = 1;
    status->scratch1 = motionNo;
    status->scratch2 = (int)info;
    _BodyPlayMotion(body, motionNo);
}

extern void RgWeaponSubShotNum(void *weapon, float delta);
static int _IsBackWeaponReady(RgBody *body, unsigned int eType);

static int _ExecCmdInShield(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    int result;

    result = _AllowDamageCmd(status, body, command);
    if (result != 0) {
        return result;
    }
    if (command->type == 2 && status->scratch2 == command->param && status->scratch0 != command->type) {
        *(void **)&status->scratch1 = (void *)1;
    }
    if (command->type == 11) {
        if (*(void **)&status->scratchTimer != 0) {
            RgWeaponSubShotNum(*(void **)&status->scratchTimer, command->scratch0);
        }
        *(int *)&status->unmodeled_10[4] = 1;
        result |= 0x400;
        XrgCopyVector((float *)&status->unmodeled_1c[4], (float *)command);
    }
    if (command->type == 2 && status->scratch2 != command->type && status->scratch0 == 0
        && _IsBackWeaponReady(body, command->param) != 0) {
        result |= _ExecShotOrAttackCmd(status, body, command);
    }
    return result;
}

extern const char D_00A51968[];
extern void RgError(const char *message, const char *source_file, int line, ...);
extern void RgGeomRobotCalcLocal(RgGeomPoint *geometry, RgVector local[4]);
extern void XrgNegateVector(RgVector destination, RgVector source);
extern void XrgScaleVector(RgVector destination, RgVector source, float scale);
extern void RgGeomPointAddForce(RgGeomPoint *point, RgVector force);
extern float RgWeaponGetShotNum(void *weapon);

static int _ShieldPassTime(RgRobotStatus *status, RgBody *body, float deltaTime)
{
    RgVector local[4];
    RgVector force;
    void *actor;
    void *weapon;
    int info;
    int action;
    float weight;

    actor = body->actor;
    weapon = *(void **)&status->scratchTimer;
    info = *(int *)&status->unmodeled_10[0];
    if (*(int *)&status->unmodeled_10[4] != 0) {
        weight = RgGeomPointGetWeight(body->geometry);
        *(int *)&status->unmodeled_10[4] = 0;
        action = RgMotionInfoGetActionInTime(info, 3, 1e8f);
        XrgScaleVector(force, (float *)&status->unmodeled_1c[4], (-weight * 7.0f) / deltaTime);
        RgGeomPointAddForce(body->geometry, force);
        if (action != 0) {
            _BodyPlayMotion(body, *(int *)(action + 0xC));
        }
    }
    switch ((unsigned int)status->scratch0) {
    case 0:
        if (actor == 0 || XrgActorIsEndOfMotion(actor) != 0) {
            status->scratch0 = 1;
        }
        break;
    case 1:
        body->shieldActive = status->scratch0;
        RgGeomRobotCalcLocal(body->geometry, local);
        XrgNegateVector((float *)&body->unmodeled_34[0xC], local[2]);
        *(int *)&body->unmodeled_34[0x10] = 0;
        if (status->scratch1 == 0 || RgWeaponGetShotNum(weapon) <= 0.0f) {
            action = RgMotionInfoGetActionInTime(info, 2, 1e8f);
            if (action != 0) {
                _BodyPlayMotion(body, *(int *)(action + 0xC));
                if (weapon != 0) {
                    RgWeaponPlayMotion(weapon, *(int *)(action + 0x10));
                }
            }
            status->scratch0 = 2;
        }
        break;
    case 2:
        body->shieldActive = 0;
        if (actor == 0 || XrgActorIsEndOfMotion(actor) != 0) {
            _InitMovingStatus(status, body);
        }
        break;
    default:
        RgError(D_00A51968, D_00A51840, 0x63C, status->scratch0);
        break;
    }
    status->scratch1 = 0;
    return 0x80;
}

static void _ShieldExit(RgRobotStatus *status, RgBody *body)
{
    void *weapon;

    (void)status;
    body->shieldActive = 0;
    weapon = body->weapon[0];
    if (weapon != 0) {
        RgWeaponRestartLockon(weapon);
    }
    weapon = body->weapon[1];
    if (weapon != 0) {
        RgWeaponRestartLockon(weapon);
    }
}

static int _ExecCmdInShield();
static int _ShieldPassTime(RgRobotStatus *status, RgBody *body, float deltaTime);

static void _InitShieldStatus(RgRobotStatus *status, RgBody *body, int motion,
                              int timer, int arg)
{
    RgMotionShotInfo *info;
    void *weapon;

    info = RgMotionInfoDBGet(InstanceOfRgMotionInfoDB(), motion, body->charID);
    if (info == 0) {
        assert_prog(D_00A51940, D_00A51840, 0x65D);
    }
    _StopAllShotThread(body);
    _SetStatus(status, 3, body);
    *(int *)&status->scratchTimer = timer;
    status->execCmdMethod = _ExecCmdInShield;
    status->passTimeMethod = _ShieldPassTime;
    status->exitMethod = _ShieldExit;
    status->scratch1 = 1;
    status->scratch2 = arg;
    *(RgMotionShotInfo **)&status->unmodeled_10[0] = info;
    *(int *)&status->scratch0 = 0;
    *(int *)&status->unmodeled_10[4] = 0;
    if (body->actor != 0) {
        _BodyPlayMotion(body, motion);
    }
    weapon = body->weapon[0];
    if (weapon != 0) {
        RgWeaponStopLockon(weapon);
    }
    weapon = body->weapon[1];
    if (weapon != 0) {
        RgWeaponStopLockon(weapon);
    }
}

extern float RgWeaponGetShotNum(void *weapon);

static int _ExecCmdInTargetting(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    int result;
    float time;
    void *weapon;

    time = status->elapsedTime;
    result = _AllowDamageCmd(status, body, command);
    if (*(float *)&status->scratch1 <= time) {
        switch (command->type) {
        case 1:
            result |= _AllowAllCmdInMoving(status, (int)body, (RgBody *)command);
            _StopHomingThread(body->homingThread);
            break;
        case 2:
            weapon = body->weapon[command->param];
            if (weapon == 0) {
                break;
            }
            if (!(RgWeaponGetShotNum(weapon) > 0.0f)) {
                return result;
            }
            if (RgWeaponGetControlFlag(weapon) & 7) {
                break;
            }
        case 0:
        case 4:
        case 5:
            result |= _AllowAllCmdInMoving(status, (int)body, (RgBody *)command);
            break;
        }
    }
    return result;
}

static int _TargettingPassTime(RgRobotStatus *status, RgBody *body)
{
    int result;
    float elapsed;
    float limit;

    result = 0;
    elapsed = status->elapsedTime;
    if (_GetStatHomingThread(body->homingThread) == 1) {
        result = 8;
        body->flags |= 3;
    }
    limit = *(float *)&status->scratch2;
    if (elapsed < limit) {
        if (status->scratch0 == 0 && limit - elapsed < 0.2f) {
            _BodyPlayMotion(body, 0x4B);
            status->scratch0 = 1;
        }
    } else {
        RgRobSubBreak(body->geometry, 0.3f, 0.3f);
        status->exitMethod = 0;
        _InitMovingStatus(status, body);
        RgRobotEffectTermJet(body->effect);
        RgRobotEffectSetJetInertia(body->effect, 1);
    }
    return result;
}

extern void RgRobotEffectSetJetInertia(RgRobotEffect *effect, int inertia);

static void _ExitTargettingStatus(RgRobotStatus *status, RgBody *body)
{
    int pad[4];
    (void)status;
    RgRobotEffectTermJet(body->effect);
    RgRobotEffectSetJetInertia(body->effect, 1);
}

struct RgPointVector;
extern void RgGeomPointGetVel(RgGeomPoint *point, RgVector velocity);
extern void RgGeomPointSetVel(RgGeomPoint *point, RgVector velocity);
extern float RgGetGeomGravity(void);
extern void __RgGeomPointGetPos(RgGeomPoint *point, struct RgPointVector *destination,
                                const char *source_file, int line);
extern int RgGeomGetEventFlag(RgGeom *geom);
extern void XrgSoundRingStopMoving(int soundID);
extern void XrgSoundRingMoving(int soundID, int unused, float volume);
extern void RgRobotEffectStartJet(RgRobotEffect *effect, float stopTime);
extern int _ExecCmdInTargetting();

static void _InitTargettingStatus(RgStatus *pStatus, RgBody *body, void *target)
{
    RgRobotStatus *status = (RgRobotStatus *)pStatus;
    RgVector velocity;
    RgVector position;
    void *homingTarget;
    int homing;
    float gravity;
    float height;

    homingTarget = body->target;
    if (status == 0) {
        assert_prog(D_00A518F0, D_00A51840, 0x6F2);
    }
    _StopAllShotThread(body);
    homing = RgGeomGetEventFlag((RgGeom *)body->eyeGeometry) == 0;
    if (homingTarget == 0) {
        homing = 0;
    }
    if (body->confuse > 0.0f) {
        homing = 0;
    }
    _SetStatus(status, 4, body);
    status->execCmdMethod = (RgStatusExecCmdFunc)_ExecCmdInTargetting;
    status->passTimeMethod = (RgStatusPassTimeFunc)_TargettingPassTime;
    status->exitMethod = _ExitTargettingStatus;
    if (target != 0) {
        RgGeomPoint *geometry = body->geometry;
        RgGeomPointGetVel(geometry, velocity);
        __RgGeomPointGetPos(geometry, (struct RgPointVector *)position, D_00A51840, 0x70D);
        gravity = RgGetGeomGravity();
        height = 7.5f - position[1];
        if (height > 0.0f) {
            velocity[1] = __builtin_sqrtf((gravity + gravity) * height);
        }
        RgGeomPointSetVel(geometry, velocity);
        *(float *)&status->scratch2 = (velocity[1] + velocity[1]) / RgGetGeomGravity();
        XrgSoundRingStopMoving(body->landingSoundID);
        XrgSoundRingMoving(body->landingSoundID, 1, *(float *)&status->scratch2);
        RgRobotEffectStartJet(body->effect, *(float *)&status->scratch2 * 0.4f);
        RgRobotEffectSetJetInertia(body->effect, 0);
        body->grounded = 0;
    } else {
        *(float *)&status->scratch2 = 0.8f;
    }
    *(float *)&status->scratch1 = *(float *)&status->scratch2 * 0.2f;
    if (homing) {
        _StartHomingThread(body->homingThread, body->geometry, body->target,
                           *(float *)&status->scratch1);
    }
    _BodyPlayMotion(body, 0x13);
    status->scratch0 = 0;
}

extern void RgGeomRobotAddRotForce(RgGeomPoint *point, float force);

static int _ExecCmdInBreaking(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    int result;
    float delta;
    int rotateSign;

    result = _AllowDamageCmd(status, body, command);
    if (command->type == 1) {
        delta = *(float *) command;
        if (delta > 0.0f) {
            rotateSign = 1;
        } else if (delta < 0.0f) {
            rotateSign = -1;
        } else {
            rotateSign = 0;
        }
        RgGeomRobotAddRotForce(body->geometry, (float) rotateSign * body->spec->rotateForce * 0.1f);
        body->flags |= 2;
    }
    return result;
}

static int _BreakingPassTime(RgRobotStatus *status, RgBody *body, float deltaTime)
{
    float remaining;

    remaining = status->scratch0AsFloat - deltaTime;
    status->scratch0AsFloat = remaining;
    if (remaining < 0.0f) {
        _InitMovingStatus(status, body);
    }
    return 0;
}

static void _BodyPlayMotion(RgBody *body, int motion);

static void _BreakingExit(RgRobotStatus *status, RgBody *body)
{
    (void)status;
    _BodyPlayMotion(body, 0);
}

static int _ExecCmdInBreaking();

static void _InitBreakingStatus(RgStatus *rs, RgBody *body)
{
    RgRobotStatus *status = (RgRobotStatus *)rs;

    _StopAllShotThread((int *)body);
    _SetStatus(status, 8, body);
    status->execCmdMethod = (RgStatusExecCmdFunc)_ExecCmdInBreaking;
    status->passTimeMethod = (RgStatusPassTimeFunc)_BreakingPassTime;
    status->exitMethod = (RgStatusExitFunc)_BreakingExit;
    *(float *)status = 0.3f;
    _BodyPlayMotion(body, 0x14);
}

static int _InitDashVRStatus();

static int _DashExecCmd(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    int result;
    RgVector dir;

    result = _AllowDamageCmd(status, body, command);
    if (*(int *)&status->unmodeled_1c[0x18] != 0 && command->type == 6) {
        XrgCopyVector(dir, (float *)status->unmodeled_10);
        if (_InitDashVRStatus(status, body, (RgCmd *)dir) != 0) {
            result |= 4;
        }
    }
    if (command->type == 2) {
        result = _ExecShotOrAttackCmd(status, body, command);
        if (result != 0) {
            return result;
        }
    }
    return result;
}

extern const char D_00A519A8[];
extern void RgRobSubAcceralate(RgGeomPoint *geometry, RgVector direction, float scale);

static int _DashPassTime(RgRobotStatus *status, RgBody *body, float dt)
{
    RgVector dir;
    RgRobotSpec *spec;
    int mask;
    int result;
    int next;
    float maxSpd;
    float accel;

    result = 0;
    spec = body->spec;
    mask = body->cmdMask;
    maxSpd = _CalcMaxSpeed(body, *(float *)&spec->unmodeled_10[0]);
    switch ((unsigned int)status->scratch0) {
    case 0:
        body->dashTime = body->dashTime - dt;
        if (body->dashTime < 0.0f) {
            body->dashTime = 0.0f;
        }
        *(float *)&status->unmodeled_1c[0xc] = *(float *)&status->unmodeled_1c[0xc] - dt;
        if (*(float *)&status->unmodeled_1c[0xc] < 0.0f) {
            *(float *)&status->unmodeled_1c[0xc] = 0.0f;
        }
        result = 4;
        accel = spec->accelRate * spec->attackAdvanceRate;
        RgGeomPointSetMaxXYSpd(body->geometry, maxSpd);
        RgRobSubAcceralate(body->geometry, (float *)status->unmodeled_10, accel);
        if (*(int *)&status->unmodeled_1c[4] == 0) {
            _BodyPlayMotion(body, RgRobSubGetAdvanceMot(body->geometry, 1));
            *(int *)&status->unmodeled_1c[4] = 1;
        }
        next = 0;
        if (*(float *)&status->unmodeled_1c[0xc] <= 0.0f) {
            next = 1;
            if (mask & 0x40) {
                if (*(int *)&status->unmodeled_1c[0x10] != 0) {
                    next = 2;
                }
            }
        }
        if (RgGeomGetEventFlag((RgGeom *)body->advanceGeometry) != 0) {
            next = 1;
        }
        if (next == 2) {
            XrgCopyVector(dir, (float *)status->unmodeled_10);
            _InitDashVRStatus(status, body, (RgCmd *)dir);
        } else if (next == 1) {
            status->scratch0 = next;
            *(float *)&status->unmodeled_1c[8] = 0.3f;
        }
        break;
    case 1:
        if (*(int *)&status->unmodeled_1c[4] != 0) {
            _BodyPlayMotionLoop(body, RgRobSubGetAdvanceMot(body->geometry, 0));
            *(int *)&status->unmodeled_1c[4] = 0;
            RgRobotEffectTermJet(body->effect);
            RgRobotEffectTermDash(body->effect);
        }
        *(float *)&status->unmodeled_1c[8] = *(float *)&status->unmodeled_1c[8] - dt;
        if (*(float *)&status->unmodeled_1c[8] < 0.0f) {
            _InitMovingStatus(status, body);
        }
        break;
    default:
        RgError(D_00A519A8, D_00A51840, 0x7FE, *(volatile int *)&status->scratch0);
        break;
    }
    return result;
}

extern void RgGeomPointSetMaxXYSpd(RgGeomPoint *point, float maxSpeed);
extern void RgRobotEffectTermJet(RgRobotEffect *effect);
extern void RgRobotEffectTermDash(RgRobotEffect *effect);
extern void XrgSoundRingStopMoving(int soundID);

static void _DashExit(RgRobotStatus *status, RgBody *body)
{
    RgRobotSpec *spec = body->spec;

    RgGeomPointSetMaxXYSpd(body->geometry, _CalcMaxSpeed(body, spec->baseSpeed));
    RgRobotEffectTermJet(body->effect);
    RgRobotEffectTermDash(body->effect);
    if (*(int *)&status->unmodeled_1c[0x14] != 0) {
        body->dashTime = *(float *)&spec->unmodeled_30[0];
    }
    XrgSoundRingStopMoving(body->landingSoundID);
}

extern void RgError(const char *message, const char *source_file, int line, ...);
extern const char D_00A519C0[];
static int _DashPassTime();
extern void RgRobotEffectStartDash(RgRobotEffect *effect, float stopTime);

static int _InitDashStatus(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    RgRobotSpec *spec = body->spec;
    int type;
    float dash;  /* first branch only */
    float limit;

    if (!status) {
        assert_prog(D_00A518F0, D_00A51840, 0x81E);
    }
    if (spec->type == 1) {
        return 0;
    }
    if (spec->type != 0) if ((unsigned int)spec->type < 5) {
        dash = body->dashTime;
        limit = *(float *)&spec->unmodeled_30[0] * 0.2f;
        if (dash < limit) {
            return 0;
        }
        body->dashTime = dash - limit * 0.1f;
    }
    _StopAllShotThread(body);
    _SetStatus(status, 5, body);
    status->execCmdMethod = (RgStatusExecCmdFunc)_DashExecCmd;
    status->passTimeMethod = (RgStatusPassTimeFunc)_DashPassTime;
    status->exitMethod = (RgStatusExitFunc)_DashExit;
    status->scratch0 = 0;
    *(int *)&status->unmodeled_1c[4] = 0;
    XrgCopyVector((float *)status->unmodeled_10, (float *)command);
    *(int *)&status->unmodeled_1c[0x18] = 0;
    type = spec->type;
    switch ((unsigned int)type) {
    case 0:
    case 1:
        *(int *)&status->unmodeled_1c[0x14] = 1;
        *(float *)&status->unmodeled_1c[0xc] = body->dashTime;
        *(int *)&status->unmodeled_1c[0x10] = 0;
        break;
    case 2:
        *(int *)&status->unmodeled_1c[0x10] = 1;
        *(float *)&status->unmodeled_1c[0xc] = body->dashTime < 0.5f ? body->dashTime : 0.5f;
        *(int *)&status->unmodeled_1c[0x14] = 0;
        break;
    case 3:
    case 4:
        *(int *)&status->unmodeled_1c[0x10] = 1;
        *(float *)&status->unmodeled_1c[0xc] = body->dashTime < 0.5f ? body->dashTime : 0.5f;
        *(int *)&status->unmodeled_1c[0x18] = 1;
        *(int *)&status->unmodeled_1c[0x14] = 0;
        break;
    default:
        RgError(D_00A519C0, D_00A51840, 0x85D, type);
        break;
    }
    RgRobotEffectStartJet(body->effect, *(float *)&spec->unmodeled_30[0]);
    RgRobotEffectStartDash(body->effect, *(float *)&spec->unmodeled_30[0]);
    XrgSoundRingMoving(body->landingSoundID, 1, 1e8f);
    return 1;
}

static void _BodyPlayMotion(RgBody *body, int motion);

/*
 * Sets DashVR's own scratch0/scratchTimer (see RgRobotStatus): scratch0 is
 * the follow-state tag _DashVRPassTime (still asm, ov12:0x00a052b0)
 * compares against 1/2, and scratchTimer is a countdown _DashVRPassTime
 * subtracts deltaTime from every frame.
 */
static void _DashVRMainToFollow(RgRobotStatus *status, RgBody *body)
{
    status->scratch0 = 1;
    status->scratchTimer = 0.1f;
    _BodyPlayMotion(body, 0);
}

extern float atan2f(float y, float x);
extern float sinf(float x);
extern float cosf(float x);
extern void XrgAddVector(RgVector destination, RgVector first, RgVector second);

static int _DashVRExecCmd(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    RgVector dir;
    RgVector ofs;
    void *weapon;
    int result;
    int sign;
    int flag;
    float rot;
    float diff;
    float f;
    float r;
    float c;
    float d;

    result = _AllowDamageCmdNotPetrify(status, body, command);
    rot = *(float *)&status->scratch1;
    if (command->type == 3) {
        _InitTargettingStatus((RgStatus *)status, body, (void *)1);
        return result | 0x100;
    }
    if (command->type == 1) {
        f = ((float *)command)[0];
        sign = 1;
        if (!(f > 0.0f)) {
            sign = -1;
            if (!(f < 0.0f)) {
                sign = 0;
            }
        }
        r = *(volatile float *)&status->scratch1 + sign * 1.5707964f;
        while (r > 3.1415927f) {
            r -= 6.2831855f;
        }
        while (r < -3.1415927f) {
            r += 6.2831855f;
        }
        rot = r;
    }
    if (command->type == 0) {
        rot = atan2f(((float *)command)[0], ((float *)command)[2]);
    }
    if (command->type == 2) {
        weapon = body->weapon[command->param];
        if (weapon) {
            flag = RgWeaponGetControlFlag(weapon) & 7;
            if ((flag == 2 || flag == 4) && RgWeaponGetShotNum(weapon) > 0.0f) {
                result = _ExecShotOrAttackCmd(status, body, command);
                if (result != 0) {
                    return result;
                }
            } else if (status->scratch0 == 0) {
                _DashVRMainToFollow(status, body);
            }
        }
    }
    if (command->type == 0xC) {
        dir[0] = sinf(*(float *)&status->scratch1);
        dir[1] = 0.0f;
        dir[2] = cosf(*(float *)&status->scratch1);
        XrgScaleVector(ofs, (float *)command,
                       -(dir[0] * ((float *)command)[0] + dir[1] * ((float *)command)[1] + dir[2] * ((float *)command)[2]));
        XrgAddVector(dir, dir, ofs);
        rot = atan2f(dir[0], dir[2]);
        *(float *)&status->scratch1 = rot;
    }
    diff = rot - *(float *)&status->scratch1;
    while (diff > 3.1415927f) {
        diff -= 6.2831855f;
    }
    while (diff < -3.1415927f) {
        diff += 6.2831855f;
    }
    d = diff / 0.2f;
    c = -12.566371f;
    if (!(d < c)) {
        c = 12.566371f;
        if (!(d > c)) {
            c = d;
        }
    }
    *(float *)&status->scratch2 += c;
    return result;
}

extern void RgGeomRobotSetRotate(RgGeomPoint *point, float rot);
extern void XrgAddVector(RgVector destination, RgVector first, RgVector second);
extern void RgRobSubTargetting(RgGeomPoint *geometry, struct RgPointVector *target, float speed, float dt);
extern float sinf(float x);
extern float cosf(float x);
extern const char D_00A519F8[];
extern const char D_00A51A18[];

static int _DashVRPassTime(RgRobotStatus *status, RgBody *body, float dt)
{
    RgVector vel;
    RgVector pos;
    RgGeomPoint *geom;
    int mask;
    int done;
    int result;
    int roll;
    int mode;
    float maxSpd;
    float rot;

    done = 0;
    result = 4;
    geom = body->geometry;
    mask = body->cmdMask;
    maxSpd = _CalcMaxSpeed(body, *(float *)&body->spec->unmodeled_10[0]);
    status->scratchTimer -= dt;
    switch ((unsigned int)status->scratch0) {
    case 0:
        rot = *(float *)&status->scratch1 + *(float *)&status->scratch2 * dt;
        while (rot > 3.1415927f) {
            rot -= 6.2831855f;
        }
        while (rot < -3.1415927f) {
            rot += 6.2831855f;
        }
        *(float *)&status->scratch1 = rot;
        RgGeomRobotSetRotate(geom, rot);
        *(float *)&status->scratch2 *= dt * 0.7f;
        RgGeomPointGetVel(geom, vel);
        vel[0] = sinf(*(float *)&status->scratch1) * maxSpd;
        vel[2] = cosf(*(float *)&status->scratch1) * maxSpd;
        RgGeomPointSetVel(geom, vel);
        body->dashTime -= dt;
        if (body->dashTime <= 0.0f) {
            body->dashTime = 0.0f;
            done = 1;
        }
        mode = *(int *)&status->unmodeled_1c[8];
        switch (mode) {
        case 0:
            if (!(mask & 0x40)) {
                done = 1;
            }
            break;
        case 1:
            if (mask & 0x10) {
                done = 1;
            }
            break;
        default:
            RgError(D_00A519F8, D_00A51840, 0x900, mode);
            break;
        }
        if (done) {
            _DashVRMainToFollow(status, body);
        }
        result = 0x104;
        break;
    case 1:
        RgRobotEffectTermJet(body->effect);
        RgRobotEffectTermDash(body->effect);
        if (status->scratchTimer < 0.0f) {
            status->scratch0 = 2;
            *(int *)&status->unmodeled_1c[4] = -1;
            status->scratchTimer = 0.2f;
        }
    case 2:
        if (status->scratchTimer < 0.0f) {
            _InitMovingStatus(status, body);
        } else {
            __RgGeomPointGetPos(body->geometry, (struct RgPointVector *)pos, D_00A51840, 0x920);
            XrgAddVector(pos, pos, (float *)status->unmodeled_10);
            RgRobSubTargetting(body->geometry, (struct RgPointVector *)pos, 12.566371f, dt);
            body->flags |= 3;
        }
        roll = RgRobSubGetRollMotion(body->geometry);
        if (roll != *(int *)&status->unmodeled_1c[4]) {
            *(int *)&status->unmodeled_1c[4] = roll;
            _BodyPlayMotion(body, roll);
        }
        break;
    default:
        RgError(D_00A51A18, D_00A51840, 0x931, *(volatile int *)&status->scratch0);
        break;
    }
    return result;
}

extern void RgGeomPointSetMaxXYSpd(RgGeomPoint *point, float maxSpeed);
extern void RgRobotEffectTermJet(RgRobotEffect *effect);
extern void RgRobotEffectTermDash(RgRobotEffect *effect);
extern void XrgSoundRingStopMoving(int soundID);

static void _DashVRExit(RgRobotStatus *status, RgBody *body)
{
    (void)status;
    RgGeomPointSetMaxXYSpd(body->geometry, _CalcMaxSpeed(body, body->spec->baseSpeed));
    RgRobotEffectTermJet(body->effect);
    RgRobotEffectTermDash(body->effect);
    XrgSoundRingStopMoving(body->landingSoundID);
}

extern float atan2f(float y, float x);
extern float sinf(float x);
extern float cosf(float x);
extern float RgGeomRobotGetRotate(RgGeomPoint *point);
extern void RgRobotEffectStartDash(RgRobotEffect *effect, float stopTime);
extern void RgError(const char *message, const char *source_file, int line, ...);
extern const char D_00A51A30[];
static int _DashVRExecCmd();
static int _DashVRPassTime();

static int _InitDashVRStatus(RgRobotStatus *status, RgBody *body, RgCmd *command,
                             int param1, int param2)
{
    RgRobotSpec *spec = body->spec;
    float rot;
    int type;

    if (!status) {
        assert_prog(D_00A518F0, D_00A51840, 0x94D);
    }
    _StopAllShotThread(body);
    if (body->dashTime < *(float *)&spec->unmodeled_30[0] * 0.2f) {
        return 0;
    }
    _SetStatus(status, 7, body);
    status->execCmdMethod = (RgStatusExecCmdFunc)_DashVRExecCmd;
    status->passTimeMethod = (RgStatusPassTimeFunc)_DashVRPassTime;
    status->exitMethod = (RgStatusExitFunc)_DashVRExit;
    *(int *)&status->scratch0 = 0;
    *(float *)&status->scratch1 = atan2f(((float *)command)[0], ((float *)command)[2]);
    status->scratch2 = 0;
    rot = RgGeomRobotGetRotate(body->geometry);
    *(float *)&status->unmodeled_10[0] = sinf(rot) * 50.0f;
    *(int *)&status->unmodeled_10[4] = 0;
    *(float *)&status->eSide = cosf(rot) * 50.0f;
    type = spec->type;
    switch ((unsigned int)type) {
    case 3:
        *(int *)&status->unmodeled_1c[8] = 1;
        break;
    case 4:
    case 2:
        *(int *)&status->unmodeled_1c[8] = 0;
        break;
    default:
        RgError(D_00A51A30, D_00A51840, 0x971, type);
        break;
    }
    RgRobotEffectStartJet(body->effect, *(float *)&spec->unmodeled_30[0]);
    RgRobotEffectStartDash(body->effect, *(float *)&spec->unmodeled_30[0]);
    RgGeomPointSetMaxXYSpd(body->geometry, _CalcMaxSpeed(body, *(float *)&spec->unmodeled_10[0]));
    _BodyPlayMotion(body, 0x32);
    XrgSoundRingMoving(body->landingSoundID, 1, 1e8f);
    return 1;
}

static int _AllowDamageCmdNotVRDashHit(RgRobotStatus *status, RgBody *body, RgCmd *command);
static int _ExecDashCmd(RgRobotStatus *status, RgBody *body, RgCmd *command);

/*
 * Damage status's own scratch1 (see RgRobotStatus) is nonzero while a
 * dash-escape is still allowed, gating whether command type 5 (dash) is
 * forwarded to _ExecDashCmd.
 */
static int _DamageExecCmd(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    int result;

    result = _AllowDamageCmdNotVRDashHit(status, body, command);
    if (result == 0) {
        if (status->scratch1 != 0 && command->type == 5) {
            result = _ExecDashCmd(status, body, command);
        }
        return result;
    }
}

static int _DamagePassTime(RgRobotStatus *status, RgBody *body, float deltaTime)
{
    (void)deltaTime;
    if (status->elapsedTime >= status->scratch0AsFloat) {
        _InitMovingStatus(status, body);
    }
    return 0x10;
}

extern int RgRobSubGetDamageMotion(RgGeomPoint *geometry, RgVector direction);

static void _InitDamageStatus(RgRobotStatus *status, RgBody *body,
                              RgCmd *command, float p5, int flag, float p6)
{
    RgRobotSpec *spec;
    float t;
    float v;

    spec = body->spec;
    if (status == 0) {
        assert_prog(D_00A518F0, D_00A51840, 0x9AE);
    }
    _StopAllShotThread(body);
    _SetStatus(status, 6, body);
    body->weakHitAccum = 0.0f;
    body->weakHitWindow = 0.0f;
    status->execCmdMethod = (RgStatusExecCmdFunc)_DamageExecCmd;
    status->passTimeMethod = (RgStatusPassTimeFunc)_DamagePassTime;
    t = p5 * *(float *)((char *)spec + 0x50);
    if (t < 0.1f) {
        v = 0.1f;
    } else if (t > 10.0f) {
        v = 10.0f;
    } else {
        v = t;
    }
    status->scratch0AsFloat = v;
    status->scratch1 = flag;
    if (p6 > 0.0f) {
        status->scratch0AsFloat = p6;
    }
    _BodyPlayMotion(body, RgRobSubGetDamageMotion(body->geometry, (float *)command));
}

static int _AllowDamageCmd(RgRobotStatus *status, RgBody *body, RgCmd *command);
static int _GetDropMotion(int eSide);
static int _InitDropStatus(RgStatus *status, RgBody *body, int motion, int eSide);

/*
 * Drop status's own scratch0/scratch2 (see RgRobotStatus): scratch0 is the
 * equip side already being dropped and scratch2 is nonzero while that
 * side's spare weapon still needs removing (see _DropPassTime, which also
 * reads and clears it).
 */
static int _ExecCmdInDrop(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    int mask;

    mask = _AllowDamageCmd(status, body, command);
    if (command->type == 7
        && command->param != status->scratch0
        && command->param == 2
        && status->scratch2 != 0) {
        mask |= 0x200;
        _InitDropStatus((RgStatus *)status, body, _GetDropMotion(2), 2);
    }
    return mask;
}

static void _InitMovingStatus(RgRobotStatus *status, RgBody *body);

/*
 * Drop status's own scratch1 (see RgRobotStatus) is the drop motion id
 * (see _GetDropMotion) checked against elapsedTime, and scratch0/scratch2
 * are the same equip side and pending flag _ExecCmdInDrop evidences. Once
 * the drop motion's timed removal point is reached, this clears the
 * body's spare weapon at that side (see _BodySetSpareWeapon, RgBody+0x7c)
 * by handing it to _BodyEquipWeapon.
 */
static int _DropPassTime(RgRobotStatus *status, RgBody *body)
{
    float elapsedTime;
    int side;

    elapsedTime = status->elapsedTime;
    if (RgMotionInfoGetActionInTime(status->scratch1, 0, elapsedTime) != 0
        && status->scratch2 != 0) {
        side = status->scratch0;
        _BodyEquipWeapon(body, side, body->spareWeapon[side]);
        body->spareWeapon[side] = 0;
        status->scratch2 = 0;
    }
    if (status->scratch2 == 0 && elapsedTime > 0.15f) {
        _InitMovingStatus(status, body);
    }
    return 0;
}

typedef struct RgWeapon RgWeapon;
extern int RgWeaponIsAttachedShot(void *weapon);

static int _InitDropStatus(RgStatus *st, RgBody *body, int motion, int eSide)
{
    RgRobotStatus *status = (RgRobotStatus *)st;
    RgMotionShotInfo *info;
    void *weapon;

    info = RgMotionInfoDBGet(InstanceOfRgMotionInfoDB(), motion, body->charID);
    weapon = body->weapon[eSide];
    if (weapon == 0) {
        goto z;
    }
    if (info == 0) {
        return (int)info;
    }
    if ((RgWeaponGetControlFlag(weapon) & 7) == 4) {
        return 0;
    }
    if (RgWeaponIsAttachedShot(body->weapon[eSide]) != 0) {
z:
        return 0;
    }
    _SetStatus(status, 9, body);
    status->execCmdMethod = (RgStatusExecCmdFunc)_ExecCmdInDrop;
    status->passTimeMethod = (RgStatusPassTimeFunc)_DropPassTime;
    status->scratch0 = eSide;
    status->scratch1 = (int)info;
    status->scratch2 = 1;
    _BodyPlayMotion(body, motion);
    return 1;
}

extern const char D_00A51A58[];
extern int s_anCmdLevel[];

static RgCmd *_EntryCmdQueue(void *pQ, int cmdType)
{
    RgCmdQueue *cmdQueue = pQ;
    RgCmd *command;
    int diff;
    int level;

    if (cmdQueue == 0) {
        assert_prog(D_00A51A58, D_00A51840, 0xA41);
    }
    if (cmdQueue->count >= RG_CMD_QUEUE_CAPACITY) {
        return 0;
    }
    if ((unsigned int)cmdType >= 0xF) {
        return 0;
    }
    cmdQueue->entryCount++;
    level = s_anCmdLevel[cmdType];
    if (cmdQueue->count != 0) {
        diff = s_anCmdLevel[cmdQueue->cmd[0].type] - level;
    } else {
        diff = 0;
    }
    if (diff > 0) {
        return 0;
    }
    if (diff < 0) {
        cmdQueue->count = 0;
    }
    command = &cmdQueue->cmd[cmdQueue->count++];
    command->type = cmdType;
    return command;
}

/* ov12:0x00a51a58 contains the assertion expression "pQ != NIL". */
extern const char D_00A51A58[];

static void _InitCmdQueue(RgCmdQueue *cmdQueue)
{
    if (cmdQueue == 0) {
        assert_prog(D_00A51A58, D_00A51840, 2673);
    }
    cmdQueue->count = 0;
    cmdQueue->entryCount = 0;
}

static void _ClearCmdQueue(RgCmdQueue *cmdQueue)
{
    if (cmdQueue == 0) {
        assert_prog(D_00A51A58, D_00A51840, 2681);
    }
    cmdQueue->count = 0;
}

static RgCmdQueue *_CreateCmdQueue(void)
{
    RgCmdQueue *cmdQueue;

    cmdQueue = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgCmdQueue), D_00A51840, 2689);
    _InitCmdQueue(cmdQueue);
    return cmdQueue;
}

static void _DisposeCmdQueue(RgCmdQueue *cmdQueue)
{
    if (cmdQueue == 0) {
        assert_prog(D_00A51A58, D_00A51840, 2697);
    }
    RgHeapFree(InstanceOfRgHeap(), cmdQueue, D_00A51840, 2698);
}

typedef struct XrgActor XrgActor;
extern void XrgActorSetMotion(XrgActor *actor, int motion);
extern void XrgActorSetSmoothPlay(XrgActor *actor, int smoothPlay);

static void _BodyPlayMotion(RgBody *body, int motion)
{
    XrgActor *actor;

    actor = (XrgActor *) body->actor;
    if (actor != 0) {
        if (body->smoothSuppressed != 0) {
            XrgActorSetSmoothPlay(actor, 0);
        } else {
            XrgActorSetSmoothPlay(actor, body->motSmooth);
        }
        XrgActorSetMotion(actor, motion);
    }
}

extern void XrgActorSetLoopPlay(XrgActor *actor, int loopPlay);

static void _BodyPlayMotionLoop(RgBody *body, int motion)
{
    XrgActor *actor;

    actor = (XrgActor *) body->actor;
    if (actor != 0) {
        if (body->smoothSuppressed != 0) {
            XrgActorSetSmoothPlay(actor, 0);
        } else {
            XrgActorSetSmoothPlay(actor, body->motSmooth);
        }
        XrgActorSetLoopPlay(actor, 1);
        XrgActorSetMotion(actor, motion);
    }
}

extern void RgGeomRobotCalcLocal(RgGeomPoint *geometry, RgVector local[4]);
extern void RgRobSubAcceralate(RgGeomPoint *geometry, RgVector direction,
                               float scale);
extern void XrgNegateVector(RgVector destination, RgVector source);

static void _BodyAdvanceForAttack(RgBody *body, float attackPower,
                                  float attackFactor)
{
    RgGeomPoint *geometry;
    RgRobotSpec *spec;
    RgVector direction;
    RgVector localFrame[4];

    geometry = body->geometry;
    spec = body->spec;
    RgGeomRobotCalcLocal(geometry, localFrame);
    XrgNegateVector(direction, localFrame[2]);
    RgRobSubAcceralate(geometry, direction,
                       spec->accelRate * spec->attackAdvanceRate * 0.3f * attackFactor);
}

/* ov12:0x00a51a68 contains the assertion expression "pBody != NIL". */
extern const char D_00A51A68[];

/* Opaque here: rg_geom_point.c (ov12/tu052) owns the RgPointVector definition. */
typedef struct RgPointVector RgPointVector;
extern void __RgGeomPointGetPos(RgGeomPoint *point, RgPointVector *destination,
                                const char *source_file, int source_line);

static int _BodyGetTargetPos(RgBody *body, RgPointVector *position)
{
    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 2758);
    }
    if (body->target != 0) {
        __RgGeomPointGetPos(body->target, position, D_00A51840, 2760);
        return 1;
    }
    return 0;
}

extern void RgRobotEffectSetJetPtclName(RgRobotEffect *effect, const char *name);
extern void RgRobotEffectSetDashPtclName(RgRobotEffect *effect, const char *name);

static void _BodySetSpec(RgBody *body, RgRobotSpec *spec)
{
    float life;

    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 2773);
    }
    if (spec != 0) {
        body->spec = spec;
        body->dashTime = *(float *)&spec->unmodeled_30[0];
        life = *(float *)((char *)spec + 0x4c);
        body->life = life;
        body->lifeMax = life;
        RgGeomPointSetMaxXYSpd(body->geometry, _CalcMaxSpeed(body, spec->baseSpeed));
        RgRobotEffectSetJetPtclName(body->effect, (char *)spec + 0x58);
        RgRobotEffectSetDashPtclName(body->effect, (char *)spec + 0x78);
    }
}

extern void RgGeomFree(RgGeomPoint *point);
extern void RgGeomPointSetMoveResist(RgGeomPoint *point, float resist);
extern void RgGeomPointSetWeight(RgGeomPoint *point, float weight);
extern float RgGeomRobotGetRotate(RgGeomPoint *point);
extern void RgGeomRobotSetRotResist(RgGeomPoint *point, float resist);
extern void RgRobotEffectSetGeom(RgRobotEffect *effect, RgGeomPoint *geometry);
/* ov12:0x00a51a78 contains the assertion expression "pGeom != NIL". */
extern const char D_00A51A78[];

static void _BodySetGeom(RgBody *body, RgGeomPoint *geometry)
{
    RgGeomPoint *oldGeometry;
    RgRobotSpec *spec;

    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 0xAEA);
    }
    if (geometry == 0) {
        assert_prog(D_00A51A78, D_00A51840, 0xAEB);
    }
    oldGeometry = body->geometry;
    if (geometry != oldGeometry) {
        RgGeomFree(oldGeometry);
    }
    spec = body->spec;
    body->geometry = geometry;
    RgGeomPointSetWeight(geometry, spec->speedRating);
    RgGeomPointSetMoveResist(geometry, spec->moveResist);
    RgGeomRobotSetRotResist(geometry, spec->rotResist);
    body->rotate = RgGeomRobotGetRotate(geometry);
    RgRobotEffectSetGeom(body->effect, geometry);
    RgGeomPointSetMaxXYSpd(body->geometry, _CalcMaxSpeed(body, spec->baseSpeed));
}

extern void RgGeomFree(RgGeomPoint *point);

static void _BodySetEyeGeom(RgBody *body, RgGeomPoint *eyeGeometry)
{
    RgGeomPoint *oldEyeGeometry;

    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 2816);
    }
    oldEyeGeometry = body->eyeGeometry;
    if (oldEyeGeometry != 0 && oldEyeGeometry != eyeGeometry) {
        RgGeomFree(oldEyeGeometry);
    }
    body->eyeGeometry = eyeGeometry;
}

/* ov12:0x00a51a78 contains the assertion expression "pGeom != NIL". */
extern const char D_00A51A78[];

static void _BodySetAdvanceGeom(RgBody *body, RgGeomPoint *advanceGeometry)
{
    RgGeomPoint *oldAdvanceGeometry;

    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 2826);
    }
    if (advanceGeometry == 0) {
        assert_prog(D_00A51A78, D_00A51840, 2827);
    }
    oldAdvanceGeometry = body->advanceGeometry;
    if (oldAdvanceGeometry != 0 && oldAdvanceGeometry != advanceGeometry) {
        RgGeomFree(oldAdvanceGeometry);
    }
    body->advanceGeometry = advanceGeometry;
}

extern void DisposeXrgSound(void *sound);
extern void RgWeaponSetSound(void *weapon, int sound);

static void _BodySetSound(RgBody *body, void *soundDriver)
{
    unsigned int i;
    void *w;

    if (body->landingSoundID != 0 && body->landingSoundID != (int)soundDriver) {
        DisposeXrgSound((void *)body->landingSoundID);
    }
    body->landingSoundID = (int)soundDriver;
    for (i = 0; i < RG_EQUIP_TYPE_NUM; i++) {
        w = body->weapon[i];
        if (w != 0) {
            RgWeaponSetSound(w, body->landingSoundID);
        }
        w = (void *)body->spareWeapon[i];
        if (w != 0) {
            RgWeaponSetSound(w, body->landingSoundID);
        }
    }
}

/*
 * CreateXrgActor's published parameter is `int actorID` (src/ov12/xrg_actor.c,
 * ov12/tu080); this caller passes the actorSpec pointer's own raw value as
 * that argument (ov12:0x00a06438, daddu $4, $17, $0 straight into the call,
 * no load first), then reads charID back from *actorSpec afterward
 * (ov12:0x00a06440), so actorSpec is reinterpreted as actorID here.
 */
extern XrgActor *CreateXrgActor(int actorID);
extern void DisposeXrgActor(XrgActor *actor);
extern void RgRobotEffectSetActor(RgRobotEffect *effect, void *actor);

static void _BodySetActor(RgBody *body, void *actorSpec)
{
    XrgActor *oldActor;
    XrgActor *actor;

    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 0xB2A);
    }
    oldActor = (XrgActor *) body->actor;
    if (oldActor != 0) {
        DisposeXrgActor(oldActor);
    }
    body->actor = 0;
    if (actorSpec != 0) {
        actor = CreateXrgActor((int) actorSpec);
        body->actor = actor;
        body->charID = *(int *) actorSpec;
        RgRobotEffectSetActor(body->effect, actor);
        _BodyPlayMotionLoop(body, 0);
    }
}

static void _BodySetTarget(RgBody *body, void *target)
{
    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 2890);
    }
    body->target = target;
}

extern const char D_00A51A88[];
extern float RgWeaponGetWeight(void *weapon);
extern void *RgWeaponGetActor(void *weapon);
extern void XrgActorDettachChild(void *parent, void *child);
extern void RgWeaponSetFree(void *weapon);
extern void RgWeaponDisposeLockon(void *weapon);
extern void RgWeaponSetdown(void *weapon);
extern void *DisposeRgWeaponNotActor(void *weapon);
extern void *CreateRgDropWeapon(void *actor);
extern void RgShotThreadInit(void *thread, void *weapon, int a, int b);

static int _BodyDropWeapon(RgBody *body, int eType)
{
    void *weapon;
    RgGeomPoint *geometry;
    void *actor;
    void *child;
    void *thread;

    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 2900);
    }
    if ((unsigned int)eType >= RG_EQUIP_TYPE_NUM) {
        assert_prog(D_00A51A88, D_00A51840, 2901);
    }
    weapon = body->weapon[eType];
    if (weapon != 0) {
        geometry = body->geometry;
        RgGeomPointSetWeight(geometry, RgGeomPointGetWeight(geometry) - RgWeaponGetWeight(weapon));
        actor = body->actor;
        child = RgWeaponGetActor(weapon);
        if (actor != 0 && child != 0) {
            XrgActorDettachChild(actor, child);
        }
        RgWeaponShotStop(weapon);
        RgWeaponSetFree(weapon);
        RgWeaponDisposeLockon(weapon);
        RgWeaponSetdown(weapon);
        CreateRgDropWeapon(DisposeRgWeaponNotActor(weapon));
        *(int *)&body->weapon[eType] = 0;
        thread = body->shotThread[eType];
        RgShotThreadInit(thread, 0, -1, 0);
    }
    return 0;
}

extern void *RgWeaponGetEquip(void *weapon);
extern int RgEquipMountPos(void *equip, int eSide, int charID, void *name);
extern int RgEquipPosToJntID(int pos);
extern void XrgActorDeriveMotion(void *actor, void *parent);
extern void XrgActorAttachChild(void *parent, void *child, int jnt);
extern int XrgActorGetAttachedJoint(void *actor);
extern void XrgActorCheckPakaPaka(void *actor);
extern void XrgLog(const char *, const char *, int, ...);
extern void RgShotThreadMotOff(void *thread);
extern int RgWeaponIsActiveLockOnSys(void *weapon);
extern const char D_00A51AC8[];
extern const char D_00A51AD0[];
extern const char D_00A51AF8[];
extern const char D_00A51B18[];
extern const char D_00A51B28[];
extern const char D_00A51B40[];

static int _BodyEquipWeapon(RgBody *body, int eSide, int weaponID)
{
    int ret;
    char *ess;
    int pos;
    int jnt;
    void *other;
    void *actor;
    void *w;
    float weight;

    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 0xBA9);
    }
    if ((unsigned int)eSide >= RG_EQUIP_TYPE_NUM) {
        assert_prog(D_00A51A88, D_00A51840, 0xBAA);
    }
    ret = _BodyDropWeapon(body, eSide);
    if (weaponID == 0) {
        return ret;
    }
    ess = RgWeaponGetEss((void *)weaponID);
    if (ess != 0) {
        pos = RgEquipMountPos(RgWeaponGetEquip((void *)weaponID), eSide, body->charID, ess + 0x330);
    } else {
        pos = RgEquipMountPos(RgWeaponGetEquip((void *)weaponID), eSide, body->charID, (void *)D_00A51AC8);
    }
    body->weapon[eSide] = (void *)weaponID;
    weight = RgGeomPointGetWeight(body->geometry);
    RgGeomPointSetWeight(body->geometry, weight + RgWeaponGetWeight((void *)weaponID));
    if (RgWeaponGetActor((void *)weaponID) != 0 && body->actor != 0) {
        other = 0;
        RgWeaponGetActor((void *)weaponID);
        jnt = RgEquipPosToJntID(pos);
        if (body->weapon[2] != 0) {
            other = RgWeaponGetActor(body->weapon[2]);
        }
        if (pos == -1) {
            assert_prog(D_00A51AD0, D_00A51840, 0xBE3);
        }
        actor = RgWeaponGetActor((void *)weaponID);
        XrgActorDeriveMotion(actor, body->actor);
        if (jnt == -1) {
            assert_prog(D_00A51AF8, D_00A51840, 0xBEA);
        }
        XrgActorAttachChild(body->actor, actor, jnt);
        if (eSide == 0 && other != 0) {
            if (RgWeaponIsActiveLockOnSys((void *)weaponID) != 0) {
                if (XrgActorGetAttachedJoint(other) == 4) {
                    XrgActorCheckPakaPaka(other);
                }
            }
        } else if (eSide == 1 && other != 0) {
            if (RgWeaponIsActiveLockOnSys((void *)weaponID) != 0) {
                if (XrgActorGetAttachedJoint(other) == 5) {
                    XrgActorCheckPakaPaka(other);
                }
            }
        } else if (eSide == 2) {
            w = 0;
            if (jnt == 4) {
                w = body->weapon[0];
            } else if (jnt == 5) {
                w = body->weapon[1];
            }
            if (w != 0 && RgWeaponIsActiveLockOnSys(w) != 0 && (RgWeaponGetControlFlag(w) & 0x20)) {
                XrgLog(D_00A51B18, D_00A51840, 0xC0E);
                XrgActorCheckPakaPaka(actor);
                XrgLog(D_00A51B28, D_00A51840, 0xC10);
            } else {
                XrgLog(D_00A51B40, D_00A51840, 0xC12);
            }
        }
    }
    if ((RgWeaponGetControlFlag((void *)weaponID) & 7) == 0) {
        RgShotThreadInit(body->shotThread[eSide], (void *)weaponID, eSide, (int)body->actor);
    } else {
        RgShotThreadInit(body->shotThread[eSide], 0, -1, 0);
    }
    if (pos == 2 || pos == 5) {
        RgWeaponDisposeLockon((void *)weaponID);
        if (body->shotThread[eSide] != 0) {
            RgShotThreadMotOff(body->shotThread[eSide]);
        }
    }
    if ((unsigned int)(pos - 6) < 2) {
        RgWeaponDisposeLockon((void *)weaponID);
        if (body->shotThread[eSide] != 0) {
            RgShotThreadMotOff(body->shotThread[eSide]);
        }
    }
    _BodySetSound(body, (void *)body->landingSoundID);
    return ret;
}

extern void DisposeRgWeapon(void *weapon);

static void _BodySetSpareWeapon(RgBody *body, int eSide, int weaponID)
{
    int weapon;

    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 0xC3B);
    }
    if ((unsigned int)eSide >= RG_EQUIP_TYPE_NUM) {
        assert_prog(D_00A51A88, D_00A51840, 0xC3C);
    }
    weapon = body->spareWeapon[eSide];
    if (weapon != 0 && weapon != weaponID) {
        DisposeRgWeapon((void *)weapon);
    }
    body->spareWeapon[eSide] = weaponID;
}

static void _BodySetAutoHomingEnv(RgBody *body, int autoHomingEnv)
{
    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 3142);
    }
    body->autoHomingEnv = autoHomingEnv;
}

/* ov12:0x00a51b48 contains the assertion expression "pCmd != NIL". */
extern const char D_00A51B48[];

static int _BodyExecCmd(RgRobotStatus *robotStatus, RgBody *body,
                        RgCmd *command)
{
    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 3152);
    }
    if (command == 0) {
        assert_prog(D_00A51B48, D_00A51840, 3153);
    }
    body->cmdMask |= 1 << command->type;
    return _StatusExecCmd(robotStatus, (RgBody *) command, (int) body);
}

extern float RgGeomPointGetSpeed(RgGeomPoint *point);
extern void RgGeomRobotSetMaxRotVel(RgGeom *geom, float maxRotVel);
extern void RgGeomPassTime(RgGeom *geom, float deltaTime);

static void _BodyGeomPassTime(RgBody *body, float deltaTime)
{
    RgGeomPoint *geometry;
    RgRobotSpec *spec;
    float speed;
    float maxSpeed;
    float rotVelScale;

    geometry = body->geometry;
    spec = body->spec;
    speed = RgGeomPointGetSpeed(geometry);
    maxSpeed = _CalcMaxSpeed(body, spec->baseSpeed);
    if (!(body->flags & 1)) {
        rotVelScale = (maxSpeed - speed) / maxSpeed;
        if (!(rotVelScale > 0.5f)) {
            rotVelScale = 0.5f;
        }
        RgGeomRobotSetMaxRotVel((RgGeom *)geometry, spec->turnRate * rotVelScale);
    } else {
        RgGeomRobotSetMaxRotVel((RgGeom *)geometry, 1e8f);
    }
    body->flags &= ~1;
    RgGeomPassTime((RgGeom *)geometry, deltaTime);
}

extern int RgGeomGetEventFlag(RgGeom *geom);
extern void RgGeomPointSetPos(RgGeomPoint *point, RgVector position);
extern void RgGeomPointMovePos(RgGeomPoint *point, RgVector position);

static int _EyeGeomPassTime(RgBody *body, float deltaTime)
{
    RgGeom *eyeGeom;
    int eventFlag;
    RgVector position;

    eyeGeom = (RgGeom *) body->eyeGeometry;
    eventFlag = RgGeomGetEventFlag(eyeGeom);
    RgGeomPassTime(eyeGeom, deltaTime);
    __RgGeomPointGetPos(body->geometry, (RgPointVector *) position, D_00A51840, 0xC79);
    RgGeomPointSetPos((RgGeomPoint *) eyeGeom, position);
    if (body->target != 0) {
        __RgGeomPointGetPos(body->target, (RgPointVector *) position, D_00A51840, 0xC7E);
        RgGeomPointMovePos((RgGeomPoint *) eyeGeom, position);
    }
    return eventFlag;
}

extern void RgGeomPointGetVel(RgGeomPoint *point, RgVector velocity);
extern void RgGeomPointSetPos(RgGeomPoint *point, RgVector position);
extern void RgGeomPointMovePos(RgGeomPoint *point, RgVector position);
extern void XrgAddVector(RgVector destination, RgVector first, RgVector second);
extern void XrgScaleVector(RgVector destination, RgVector source, float scale);

static void _AdvanceGeomPassTime(RgBody *body, float deltaTime)
{
    RgGeom *advanceGeom;
    RgGeomPoint *geometry;
    RgVector position;
    RgVector velocity;

    advanceGeom = (RgGeom *) body->advanceGeometry;
    geometry = body->geometry;
    RgGeomPassTime(advanceGeom, deltaTime);
    __RgGeomPointGetPos(geometry, (RgPointVector *) position, D_00A51840, 3213);
    RgGeomPointSetPos((RgGeomPoint *) advanceGeom, position);
    RgGeomPointGetVel(geometry, velocity);
    XrgScaleVector(velocity, velocity, 0.5f);
    XrgAddVector(position, position, velocity);
    RgGeomPointMovePos((RgGeomPoint *) advanceGeom, position);
}

static void _BodyAutoHomingPassTime(RgBody *body, float deltaTime)
{
    char *spec;
    RgGeomPoint *geometry;
    unsigned int flags;
    unsigned int i;
    int off;
    unsigned int mask;
    float range;
    char *rangeBase;
    char *maskBase;

    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 0xC9E);
    }
    flags = body->flags;
    spec = (char *)body->spec;
    geometry = body->geometry;
    if (flags & 2) {
        body->flags = flags & ~2;
        return;
    }
    i = 0;
    maskBase = spec + 0x10;
    rangeBase = spec + 0xC;
    for (off = 0x30; i < 2; i++) {
        range = *(float *)(rangeBase + off);
        mask = *(unsigned int *)(maskBase + off);
        off += 8;
        if (range > 0.0f && mask != 0 && (body->autoHomingEnv & mask) == mask) {
            RgRobSubHoming(geometry, (RgGeomPoint *)body->target, range, deltaTime);
            body->flags |= 1;
        }
    }
}

extern void XrgSoundPassTime(void *sound, float dt);
extern void RgRobotEffectPassTime(RgRobotEffect *effect, float time);
extern void RgShotThreadPassTime(void *thread, float deltaTime);
extern void XrgActorPassTime(void *pActor, float dt);
extern void RgWeaponPassTime(void *weapon, float dt);
extern int RgWeaponIsLockedOn(void *weapon);

static int _BodyPassTime(RgRobotStatus *robotStatus, RgBody *body,
                          float deltaTime)
{
    int result;
    unsigned int i;
    float dt;

    result = 0;
    dt = deltaTime;
    if (!body) {
        assert_prog(D_00A51A68, D_00A51840, 0xCBD);
    }
    *(float *)body->unmodeled_ac = *(float *)body->unmodeled_ac + dt;
    XrgSoundPassTime((void *)body->landingSoundID, dt);
    if (body->confuse > 0.0f) {
        body->confuse = body->confuse - dt;
    }
    if (robotStatus->type != 7 && robotStatus->type != 5) {
        body->rotate = RgGeomRobotGetRotate(body->geometry);
    }
    RgRobotEffectPassTime(body->effect, deltaTime);
    if (body->weakHitMotion > 0.0f) {
        result = 0x10;
        dt = deltaTime * body->weakHitPower;
        body->weakHitMotion = body->weakHitMotion - deltaTime;
    }
    result |= _StatusPassTime(robotStatus, body, dt);
    for (i = 0; i < 3; i++) {
        RgShotThreadPassTime(body->shotThread[i], dt);
    }
    _PassTimeHomingThread(body->homingThread, dt);
    if (_GetStatHomingThread(body->homingThread) == 1) {
        body->flags |= 1;
    }
    _BodyAutoHomingPassTime(body, dt);
    _BodyGeomPassTime(body, dt);
    if (body->actor) {
        XrgActorPassTime(body->actor, dt);
    }
    for (i = 0; i < 3; i++) {
        if (body->weapon[i]) {
            RgWeaponPassTime(body->weapon[i], dt);
            if (RgWeaponIsLockedOn(body->weapon[i])) {
                result |= 2;
            }
        }
    }
    if (!_EyeGeomPassTime(body, dt) && *(float *)body->unmodeled_ac > 0.1f) {
        result |= 1;
    }
    _AdvanceGeomPassTime(body, dt);
    body->smoothSuppressed = 0;
    return result;
}

extern void XrgActorSetLocal(void *actor, RgVector local[4]);
extern void XrgActorDraw(void *actor);
extern void RgWeaponDisp(void *weapon);
extern void RgRobotEffectDisp(RgRobotEffect *effect);

static void _BodyDisp(RgRobotStatus *robotStatus, RgBody *body)
{
    unsigned int i;
    void *w;
    RgVector local[4];

    if (!body) {
        assert_prog(D_00A51A68, D_00A51840, 0xD0E);
    }
    _StatusDisp(robotStatus, body);
    RgGeomRobotCalcLocal(body->geometry, local);
    if (body->actor) {
        XrgActorSetLocal(body->actor, local);
        XrgActorDraw(body->actor);
    }
    for (i = 0; i < RG_EQUIP_TYPE_NUM; i++) {
        w = body->weapon[i];
        if (w) {
            RgWeaponDisp(w);
        }
    }
    RgRobotEffectDisp(body->effect);
}

extern RgRobotSpec D_00A59920;
extern void InitRgRobotSpec(RgRobotSpec *pSpec);
extern void *CreateRgGeomRobot(void);
extern void *CreateRgGeomBall(float radius, float weight);
extern void *CreateRgShotThread(void);
extern RgRobotEffect *CreateRgRobotEffect(void);
typedef struct RgBodyEquip {
    unsigned char unmodeled_00[0x70];
    void *weapon[RG_EQUIP_TYPE_NUM];
    void *spareWeapon[RG_EQUIP_TYPE_NUM];
} RgBodyEquip;

static void _InitBody(RgBody *body)
{
    unsigned int i;
    RgRobotEffect *effect;

    InitRgRobotSpec(&D_00A59920);
    if (!body) {
        assert_prog(D_00A51A68, D_00A51840, 0xD37);
    }
    body->spec = &D_00A59920;
    body->flags = 0;
    body->autoHomingEnv = 0;
    body->cmdMask = 0;
    body->prevCmdMask = 0;
    body->charID = -1;
    body->life = 100.0f;
    body->lifeMax = 100.0f;
    body->grounded = 1;
    body->weakHitMotion = 0.0f;
    body->weakHitPower = 1.0f;
    body->weakHitAccum = 0.0f;
    body->weakHitWindow = 0.0f;
    body->shieldActive = 0;
    body->target = 0;
    body->actor = 0;
    body->geometry = CreateRgGeomRobot();
    body->eyeGeometry = CreateRgGeomBall(1.0f, 1.0f);
    body->advanceGeometry = CreateRgGeomBall(0.3f, 0.3f);
    RgGeomPointSetMaxXYSpd(body->geometry, _CalcMaxSpeed(body, body->spec->baseSpeed));
    body->dashTime = *(float *)&body->spec->unmodeled_30;
    for (i = 0; i < RG_EQUIP_TYPE_NUM; i++) {
        body->weapon[i] = 0;
        ((RgBodyEquip *)body)->spareWeapon[i] = 0;
        body->shotThread[i] = CreateRgShotThread();
    }
    body->homingThread = _CreateHomingThread();
    effect = CreateRgRobotEffect();
    body->effect = effect;
    RgRobotEffectSetGeom(effect, body->geometry);
    body->motSmooth = 1;
    body->smoothSuppressed = 1;
    body->confuse = 0.0f;
    body->landingSoundID = 0;
    *(int *)body->unmodeled_ac = 0;
}

extern void DisposeRgShotThread(void *thread);

static void _DestructBody(RgBody *body)
{
    unsigned int i;

    if (!body) {
        assert_prog(D_00A51A68, D_00A51840, 0xD7E);
    }
    RgGeomFree(body->geometry);
    if (body->eyeGeometry) {
        RgGeomFree(body->eyeGeometry);
    }
    if (body->advanceGeometry) {
        RgGeomFree(body->advanceGeometry);
    }
    for (i = 0; i < RG_EQUIP_TYPE_NUM; i++) {
        if (body->weapon[i]) {
            DisposeRgWeapon(body->weapon[i]);
        }
        if (body->spareWeapon[i]) {
            DisposeRgWeapon((void *)body->spareWeapon[i]);
        }
        DisposeRgShotThread(body->shotThread[i]);
    }
    _DisposeHomingThread(body->homingThread);
    DisposeRgRobotEffect(body->effect);
    if (body->actor) {
        DisposeXrgActor(body->actor);
    }
    if (body->landingSoundID) {
        DisposeXrgSound((void *)body->landingSoundID);
    }
}

static void _InitBody(RgBody *body);

static RgBody *_CreateBody(void)
{
    RgBody *body;

    body = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgBody), D_00A51840, 3482);
    _InitBody(body);
    return body;
}

extern int RgHeapIsInvalidMemory(RgHeap *heap, void *ptr);
extern void RgError(const char *message, const char *source_file, int line,
                     ...);
static void _DestructBody(RgBody *body);

/* ov12:0x00a51b58 contains the format string "already disposed body %p". */
extern const char D_00A51B58[];

static void _DisposeBody(RgBody *body)
{
    if (body == 0) {
        assert_prog(D_00A51A68, D_00A51840, 3490);
    }
    if (RgHeapIsInvalidMemory(InstanceOfRgHeap(), body) != 0) {
        RgError(D_00A51B58, D_00A51840, 3492, body);
    }
    _DestructBody(body);
    RgHeapFree(InstanceOfRgHeap(), body, D_00A51840, 3494);
}

extern void RgGeomPointSetMaxXYSpd(RgGeomPoint *point, float maxSpeed);
extern void XrgSoundRingMoving(int soundID, int unused, float volume);

static int _ExecAccelarateCmd(RgStatus *status, RgBody *body, RgCmd *command)
{
    RgRobotSpec *spec;
    RgGeomPoint *geometry;

    spec = body->spec;
    geometry = body->geometry;
    RgGeomPointSetMaxXYSpd(geometry, _CalcMaxSpeed(body, spec->baseSpeed));
    RgRobSubAcceralate(geometry, (float *) command, spec->accelRate);
    XrgSoundRingMoving(body->landingSoundID, 0, 0.2f);
    return 1;
}

extern void RgGeomRobotAddRotForce(RgGeomPoint *point, float force);

/*
 * The queued command reuses its own +0x00 slot (RgCmd.param) as a plain
 * float rotate delta for this command type (see RgCmd's own doc on that
 * reuse); only its sign is used.
 */
static int _ExecRotateCmd(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    float delta;
    int rotateSign;

    delta = *(float *) command;
    if (delta > 0.0f) {
        rotateSign = 1;
    } else if (delta < 0.0f) {
        rotateSign = -1;
    } else {
        rotateSign = 0;
    }
    if ((float) rotateSign == 0.0f) {
        return 0;
    }
    RgGeomRobotAddRotForce(body->geometry, (float) rotateSign * body->spec->rotateForce);
    body->flags |= 2;
    XrgSoundRingMoving(body->landingSoundID, 0, 0.2f);
    return 2;
}

extern const char D_00A51A88[];
extern float RgWeaponGetShotNum(void *weapon);

static int _IsBackWeaponReady(RgBody *body, unsigned int eType)
{
    if (eType >= RG_EQUIP_TYPE_NUM) {
        assert_prog(D_00A51A88, D_00A51840, 3543);
    }
    if (eType != 2) return 0;
    if (body->weapon[2] == 0) return 0;
    if (RgWeaponGetShotNum(body->weapon[2]) <= 0.0f) return 0;
    return 1;
}

extern int RgHissatuCheck(int charID, void **weapons);
extern int RgWeaponIsNotBusy(void *weapon);
extern int RgWeaponIsActiveLockOnSys(void *weapon);
extern int RgWeaponRobotShotMotion(void *weapon, int charID);
extern int RgShotThreadGetStatus(void *thread);
typedef struct RgShotMotStartInfo {
    int motion;
    float time0;
    float time1;
} RgShotMotStartInfo;
extern void RgShotThreadStart(void *thread, RgShotMotStartInfo *info);

static int _ExecShotOrAttackCmd(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    RgShotMotStartInfo info;
    int side;
    void *thread;
    void *weapon;
    int flag;
    int type;
    int motion;
    RgMotionShotInfo *mi;
    int action;

    side = command->param;
    weapon = body->weapon[side];
    thread = body->shotThread[side];
    if (side == 2 && RgHissatuCheck(body->charID, body->weapon) != 0) {
        _InitStatusHissatu(status, body);
        return 8;
    }
    if (weapon == 0) {
        return 0;
    }
    if (RgWeaponGetShotNum(weapon) <= 0.0f) {
        return _InitDropStatus((RgStatus *)status, body, _GetDropMotion(side), side) ? 0x200 : 0;
    }
    if (RgWeaponIsNotBusy(weapon) == 0 && RgWeaponIsActiveLockOnSys(weapon) == 0) {
        return 0;
    }
    flag = RgWeaponGetControlFlag(weapon);
    type = flag & 7;
    motion = RgWeaponRobotShotMotion(weapon, body->charID);
    mi = RgMotionInfoDBGet(InstanceOfRgMotionInfoDB(), motion, body->charID);
    if (side == 2) {
        RgShotThreadSleep(body->shotThread[0]);
        RgShotThreadSleep(body->shotThread[1]);
    }
    if (type == 0 && RgShotThreadGetStatus(thread) != 5) {
        action = RgMotionInfoGetActionInTime((int)mi, 0, 1e8f);
        if ((flag & 0x20) || motion == -1) {
            info.motion = -1;
            info.time0 = 0.18f;
            info.time1 = 0.4f;
        } else {
            info.motion = motion;
            if (action == 0) {
                info.time0 = 0.18f;
                info.time1 = 0.4f;
            } else {
                info.time0 = *(float *)(action + 4);
                info.time1 = *(float *)(action + 8);
            }
        }
        RgShotThreadStart(thread, &info);
        RgWeaponPlayShotMotion(weapon);
        return 8;
    }
    if (type == 3) {
        _InitShieldStatus(status, body, motion, (int)weapon, side);
        return 0x20;
    }
    _InitAttackStatus(status, body, motion != -1 ? motion : 0, (int)weapon, side);
    return 0x10;
}

static void _InitTargettingStatus(RgStatus *status, RgBody *body, void *target);

static int _ExecTargettingCmd(RgStatus *status, RgBody *body, RgCmd *command, void *target)
{
    (void)command;
    _InitTargettingStatus(status, body, target);
    return 0x100;
}

static int _ExecBreakCmd(RgStatus *status, RgBody *body, void *command)
{
    float brake_scale = 0.30000001192092895508f;

    (void)command;
    RgRobSubBreak(body->geometry, brake_scale, brake_scale);
    _InitBreakingStatus(status, body);
    return 0x40;
}

static int _InitDashStatus(RgRobotStatus *status, RgBody *body, RgCmd *command);
static int _InitDashVRStatus();

static int _ExecDashCmd(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    int result = 0;

    switch ((unsigned int)body->spec->type) {
    case 0:
    case 2:
    case 3:
    case 4:
        _InitDashStatus(status, body, command);
        result = 4;
        break;
    case 1:
        ((int (*)())_InitDashVRStatus)(status, body, command);
        result = 4;
        break;
    }
    return result;
}

static int _InitDashVRStatus(RgRobotStatus *status, RgBody *body, RgCmd *command,
                             int param1, int param2);

static int _ExecDashContinueCmd(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    int result;

    result = 0;
    if (body->spec->type == 1) {
        _InitDashVRStatus(status, body, command, 1, 0);
        result = 4;
    }
    return result;
}

/* The format string "unknown equip type (drop weapon) %d". */
extern const char D_00A51B78[];

extern void RgError(const char *message, const char *source_file, int line,
                     ...);

static int _GetDropMotion(int eSide)
{
    int motion;

    motion = 0;
    switch (eSide) {
    case 2:
        break;
    case 0:
        motion = 0x1F;
        break;
    case 1:
        motion = 0x20;
        break;
    default:
        RgError(D_00A51B78, D_00A51840, 3752, eSide);
        break;
    }
    return motion;
}

static int _InitDropStatus(RgStatus *status, RgBody *body, int motion,
                            int eSide);

static int _ExecDropWeaponCmd(RgStatus *status, RgBody *body,
                               RgCmd *command)
{
    return (_InitDropStatus(status, body, _GetDropMotion(command->param),
                             command->param) == 0)
               ? 0
               : 0x200;
}

static void _InitDamageStatus(RgRobotStatus *status, RgBody *body,
                              RgCmd *command, float p5, int flag, float p6);

static int _ExecDamageCmd(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    body->life -= command->scratch0;
    if (body->life < 0.0f) {
        body->life = 0.0f;
    }
    _InitDamageStatus(status, body, command, command->scratch0, command->scratch1, 0.0f);
    return 0x80;
}

extern void CreateRgHitEffectPosDir(RgVector position, RgVector direction,
                                    char *name1, char *name2);
extern void RgGeomPointAddForce(RgGeomPoint *point, RgVector force);
extern float XrgNormalizeVector(RgVector destination, RgVector source);
extern unsigned char D_00A51BA0[];

static int _ExecHitByBody(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    RgVector force;

    CreateRgHitEffectPosDir(&command->scratch0, (float *) command,
                            D_00A51BA0, 0);
    XrgNormalizeVector(force, (float *) command);
    XrgScaleVector(force, force, command->scratch5);
    RgGeomPointAddForce(body->geometry, force);
    body->life -= command->scratch4;
    if (body->life < 0.0f) {
        body->life = 0.0f;
    }
    _InitDamageStatus(status, body, command, 0.0f, 1, 0.5f);
    return 0x400;
}

static int _ExecWeakDamageCmd(RgRobotStatus *status, RgBody *body, RgCmd *command)
{
    float windowSeed;
    float accum;

    body->life -= command->scratch0;
    if (body->life < 0.0f) {
        body->life = 0.0f;
    }
    if (body->weakHitWindow <= 0.0f) {
        windowSeed = command->scratch2;
        body->weakHitAccum = 0.0f;
        body->weakHitWindow = windowSeed;
    }
    accum = body->weakHitAccum + command->scratch0;
    body->weakHitAccum = accum;
    if (*(float *) &command->scratch1 <= accum) {
        _InitDamageStatus(status, body, command, accum, 1, 0.0f);
        body->weakHitWindow = 0.0f;
        body->weakHitAccum = 0.0f;
    } else {
        body->weakHitMotion = command->scratch3;
        body->weakHitPower = command->scratch4;
    }
    return 0x80;
}

struct RgDullFlag;
void RgDullFlagSetFlag(struct RgDullFlag *pDull);
int RgDullFlagGet(struct RgDullFlag *pDull);
void RgDullFlagPassTime(struct RgDullFlag *pDull, float fTime);
extern int RgDrawViewIsPointInView(RgDrawView *view, RgPointVector *point, float margin);

static void _DullFlagsPassTime(RgStatus *pRobot, float deltaTime)
{
    float position[4];
    unsigned int statusFlags;
    unsigned int commandMask;
    int env;

    statusFlags = pRobot->statusFlags;
    commandMask = pRobot->acceptedCommandMask;
    if (statusFlags & 1) {
        RgDullFlagSetFlag((struct RgDullFlag *)&pRobot->unmodeled_14[0x0C]);
    }
    if (statusFlags & 2) {
        RgDullFlagSetFlag((struct RgDullFlag *)&pRobot->unmodeled_14[0x1C]);
    }
    if (commandMask & 8) {
        RgDullFlagSetFlag((struct RgDullFlag *)&pRobot->unmodeled_14[0x2C]);
    }
    RgDullFlagPassTime((struct RgDullFlag *)&pRobot->unmodeled_14[0x0C], deltaTime);
    RgDullFlagPassTime((struct RgDullFlag *)&pRobot->unmodeled_14[0x1C], deltaTime);
    RgDullFlagPassTime((struct RgDullFlag *)&pRobot->unmodeled_14[0x2C], deltaTime);
    env = 0;
    if (RgDullFlagGet((struct RgDullFlag *)&pRobot->unmodeled_14[0x2C])) {
        env = 4;
    }
    if (RgDullFlagGet((struct RgDullFlag *)&pRobot->unmodeled_14[0x0C])) {
        env |= 1;
    }
    if (RgDullFlagGet((struct RgDullFlag *)&pRobot->unmodeled_14[0x1C])) {
        env |= 2;
    }
    if (_BodyGetTargetPos(pRobot->body, (RgPointVector *)position) && pRobot->drawView != 0) {
        if (RgDrawViewIsPointInView(pRobot->drawView, (RgPointVector *)position, 0.0f)) {
            env |= 8;
        }
    }
    _BodySetAutoHomingEnv(pRobot->body, env);
}

extern int RgRobotIsDead(RgStatus *pRobot);
static int _BodyExecCmd(RgRobotStatus *robotStatus, RgBody *body,
                         RgCmd *command);
static int _BodyPassTime(RgRobotStatus *robotStatus, RgBody *body,
                          float deltaTime);
static void _ClearCmdQueue(RgCmdQueue *cmdQueue);
static void _DullFlagsPassTime(RgStatus *pRobot, float deltaTime);

static void _PassTimeRobot(RgStatus *pRobot, float deltaTime)
{
    RgBody *body;
    RgCmdQueue *cmdQueue;
    unsigned int savedCmdMask;
    unsigned int i;

    cmdQueue = pRobot->cmdQueue;
    body = pRobot->body;
    if (!RgRobotIsDead(pRobot)) {
        pRobot->acceptedCommandMask = 0;
        savedCmdMask = body->cmdMask;
        body->cmdMask = 0;
        body->prevCmdMask = savedCmdMask;
        for (i = 0; i < cmdQueue->count; i++) {
            pRobot->acceptedCommandMask |=
                _BodyExecCmd(pRobot->robotStatus, body, &cmdQueue->cmd[i]);
        }
    }
    _ClearCmdQueue(cmdQueue);
    pRobot->statusFlags =
        _BodyPassTime(pRobot->robotStatus, body, deltaTime);
    _DullFlagsPassTime(pRobot, deltaTime);
}

static void _BodyDisp(RgRobotStatus *robotStatus, RgBody *body);

static void _DispRobot(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 3976);
    }
    _BodyDisp(pRobot->robotStatus, pRobot->body);
}

/* ov12:0x00a51bc0 contains the format string "already disposed robot %p\n". */
extern const char D_00A51BC0[];

static void _DestructRobot(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 3985);
    }
    if (RgHeapIsInvalidMemory(InstanceOfRgHeap(), pRobot) != 0) {
        RgError(D_00A51BC0, D_00A51840, 3987, pRobot);
    }
    _DisposeBody(pRobot->body);
    _DisposeStatus(pRobot->robotStatus);
    _DisposeCmdQueue(pRobot->cmdQueue);
}

typedef struct RgDullFlag RgDullFlag;
void InitRgDullFlag(RgDullFlag *pDull);
void RgDullFlagSetDullTime(RgDullFlag *pDull, float fTime);

static void _InitRobot(RgStatus *pRobot)
{
    RgRobotStatus *status;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 0xFA2);
    }
    pRobot->body = _CreateBody();
    status = _CreateStatue();
    pRobot->robotStatus = status;
    _InitMovingStatus(status, pRobot->body);
    pRobot->cmdQueue = _CreateCmdQueue();
    pRobot->acceptedCommandMask = 0;
    pRobot->statusFlags = 0;
    InitRgDullFlag((RgDullFlag *)&pRobot->unmodeled_14[0x0C]);
    InitRgDullFlag((RgDullFlag *)&pRobot->unmodeled_14[0x1C]);
    InitRgDullFlag((RgDullFlag *)&pRobot->unmodeled_14[0x2C]);
    RgDullFlagSetDullTime((RgDullFlag *)&pRobot->unmodeled_14[0x0C], 0.5f);
    RgDullFlagSetDullTime((RgDullFlag *)&pRobot->unmodeled_14[0x1C], 0.5f);
    RgDullFlagSetDullTime((RgDullFlag *)&pRobot->unmodeled_14[0x2C], 0.4f);
    pRobot->drawView = 0;
    *(int *)&pRobot->unmodeled_4c[0] = 0;
    *(int *)&pRobot->unmodeled_4c[4] = 0;
}

static void _InitRobot(RgStatus *pRobot);

RgStatus *CreateRgRobot(void)
{
    RgStatus *pRobot;

    pRobot = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgStatus), D_00A51840,
                          4041);
    _InitRobot(pRobot);
    return pRobot;
}

static void _DestructRobot(RgStatus *pRobot);

void DisposeRgRobot(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4049);
    }
    _DestructRobot(pRobot);
    RgHeapFree(InstanceOfRgHeap(), pRobot, D_00A51840, 4051);
}

extern float XrgRand(float lower, float upper);
extern void CreateRgHitEffectPos(RgVector position, char *name1, char *name2);

void RgRobotPassTime(RgStatus *pRobot, float deltaTime)
{
    RgVector pos;
    float t;
    int state;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 0xFDC);
    }
    _PassTimeRobot(pRobot, deltaTime);
    if (pRobot->body->life <= 0.0f) {
        if (pRobot->robotStatus->type != 1) {
            _InitMovingStatus(pRobot->robotStatus, pRobot->body);
        }
    }
    state = *(int *)&pRobot->unmodeled_4c[0];
    t = *(float *)&pRobot->unmodeled_4c[4] - deltaTime;
    *(float *)&pRobot->unmodeled_4c[4] = t;
    if (state != 1) {
        if (state == 0) {
        if (pRobot->body->life <= 0.0f) {
            *(int *)&pRobot->unmodeled_4c[0] = 1;
            *(float *)&pRobot->unmodeled_4c[4] = 5.0f;
        }
        }
    } else {
        if (t < 0.0f) {
            *(int *)&pRobot->unmodeled_4c[0] = 2;
            return;
        }
        if (!((int)(t * 15.0f) & 1)) {
            __RgGeomPointGetPos(pRobot->body->geometry, (struct RgPointVector *)pos, D_00A51840, 0xFF7);
            pos[0] += XrgRand(-1.7f, 1.7f);
            pos[1] += XrgRand(-1.0f, 3.5f);
            pos[2] += XrgRand(-1.7f, 1.7f);
            CreateRgHitEffectPos(pos, (char *)D_00A51BA0, (char *)D_00A51BA0);
        }
        if (!((int)(*(float *)&pRobot->unmodeled_4c[4] * 20.0f) & 1)) {
            XrgSoundRing(pRobot->body->landingSoundID, 0x15);
        }
    }
}

static void _DispRobot(RgStatus *pRobot);

void RgRobotDisp(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4109);
    }
    _DispRobot(pRobot);
}

static void _BodySetSpec(RgBody *body, RgRobotSpec *spec);

void RgRobotSetSpec(RgStatus *pRobot, RgRobotSpec *spec)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4124);
    }
    _BodySetSpec(pRobot->body, spec);
}

static void _BodySetGeom(RgBody *body, RgGeomPoint *geometry);

void RgRobotSetGeom(RgStatus *pRobot, RgGeomPoint *geometry)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4133);
    }
    _BodySetGeom(pRobot->body, geometry);
}

static void _BodySetEyeGeom(RgBody *body, RgGeomPoint *eyeGeometry);

void RgRobotSetEyeGeom(RgStatus *pRobot, RgGeomPoint *eyeGeometry)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4142);
    }
    _BodySetEyeGeom(pRobot->body, eyeGeometry);
}

static void _BodySetAdvanceGeom(RgBody *body, RgGeomPoint *advanceGeometry);

void RgRobotSetAdvanceGeom(RgStatus *pRobot, RgGeomPoint *advanceGeometry)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4152);
    }
    _BodySetAdvanceGeom(pRobot->body, advanceGeometry);
}

static void _BodySetSound(RgBody *body, void *soundDriver);

void RgRobotSetSoundDriver(RgStatus *pRobot, void *soundDriver)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4161);
    }
    _BodySetSound(pRobot->body, soundDriver);
}

static void _BodySetActor(RgBody *body, void *actor);

void RgRobotSetActor(RgStatus *pRobot, void *actor)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4169);
    }
    _BodySetActor(pRobot->body, actor);
}

void RgRobotSetTarget(RgStatus *pRobot, void *target)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4177);
    }
    _BodySetTarget(pRobot->body, target);
}

void RgRobotSetRgDrawView(RgStatus *pRobot, RgDrawView *view)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4187);
    }
    pRobot->drawView = view;
}

void RgRobotSetMotSmooth(RgStatus *pRobot, int motSmooth)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4195);
    }
    pRobot->body->motSmooth = motSmooth;
}

void RgRobotSetConfuse(RgStatus *pRobot, float confuse)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4203);
    }
    pRobot->body->confuse = confuse;
}

void RgRobotSetWeapon(RgStatus *pRobot, int eSide, int weaponID)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4217);
    }
    _BodyEquipWeapon(pRobot->body, eSide, weaponID);
}

void RgRobotSetSpareWeapon(RgStatus *pRobot, int eSide, int weaponID)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4227);
    }
    _BodySetSpareWeapon(pRobot->body, eSide, weaponID);
}

RgGeomPoint *RgRobotGetGeom(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4239);
    }
    return pRobot->body->geometry;
}

void *RgRobotGetWeapon(RgStatus *pRobot, unsigned int eSide)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4247);
    }
    if (eSide >= RG_EQUIP_TYPE_NUM) {
        assert_prog(D_00A51BE0, D_00A51840, 4248);
    }
    return pRobot->body->weapon[eSide];
}

void *RgRobotGetTarget(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4256);
    }
    return pRobot->body->target;
}

int RgRobotIsDead(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4264);
    }
    return pRobot->body->life <= 0.0f;
}

float RgRobotGetLife(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4272);
    }
    return pRobot->body->life;
}

float RgRobotGetLifeMax(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4278);
    }
    return pRobot->body->lifeMax;
}

void *RgRobotGetActor(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4286);
    }
    return pRobot->body->actor;
}

int RgRobotGetCharID(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4294);
    }
    return pRobot->body->charID;
}

static inline float _InvAtkDot(float *a, float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

int RgRobotIsInvalidAttack(RgStatus *pRobot, RgVector direction)
{
    RgBody *body;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4303);
    }
    body = pRobot->body;
    if (body->shieldActive != 0
        && !(_InvAtkDot(direction, (float *) &body->unmodeled_34[0xc]) < 0.0f)) {
        return 1;
    }
    return 0;
}

float RgRobotGetDashTime(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4321);
    }
    return pRobot->body->dashTime;
}

void *RgRobotGetSpec(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4329);
    }
    return pRobot->body->spec;
}

int RgRobotIsConfused(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4337);
    }
    if (pRobot->body->confuse > 0.0f) {
        return 1;
    }
    return 0;
}

int RgRobotIsAcceptedCommand(RgStatus *pRobot, int command)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4348);
    }
    return (pRobot->acceptedCommandMask & (1 << command)) != 0;
}

unsigned int RgRobotGetStatusFlags(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4356);
    }
    return pRobot->statusFlags;
}

void RgRobotAccelarate(RgStatus *pRobot, RgVector velocity)
{
    RgCmd *command;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4370);
    }
    command = _EntryCmdQueue(pRobot->cmdQueue, 0);
    if (command != 0) {
        XrgCopyVector((float *) command, velocity);
    }
}

void RgRobotAccelarateRotate(RgStatus *pRobot, float accel)
{
    RgCmd *command;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 0x111E);
    }
    command = _EntryCmdQueue(pRobot->cmdQueue, 1);
    if (command != 0) {
        *(float *) command = accel;
    }
}

/*
 * ov12:0x00a51a88 contains the range expression
 * "RG_EQUIP_TYPE_MIN <= (eType) && (eType) < RG_EQUIP_TYPE_NUM".
 */
extern const char D_00A51A88[];

void RgRobotShot(RgStatus *pRobot, unsigned int eType)
{
    RgCmd *command;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4394);
    }
    if (eType >= RG_EQUIP_TYPE_NUM) {
        assert_prog(D_00A51A88, D_00A51840, 4395);
    }
    command = _EntryCmdQueue(pRobot->cmdQueue, 2);
    if (command != 0) {
        command->param = eType;
    }
}

void RgRobotTargetting(RgStatus *pRobot)
{
    RgCmd *command;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4407);
    }
    command = _EntryCmdQueue(pRobot->cmdQueue, 3);
    if (command != 0) {
        command->param = (int) pRobot->body->target;
    }
}

void RgRobotBreak(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4419);
    }
    _EntryCmdQueue(pRobot->cmdQueue, 4); /* command type 4: break */
}

void RgRobotDash(RgStatus *pRobot, RgVector direction)
{
    RgCmd *command;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4428);
    }
    command = _EntryCmdQueue(pRobot->cmdQueue, 5);
    if (command != 0) {
        XrgCopyVector((float *) command, direction);
    }
}

void RgRobotDashContinue(RgStatus *pRobot)
{
    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4440);
    }
    _EntryCmdQueue(pRobot->cmdQueue, 6); /* command type 6: continue dash */
}

void RgRobotGiveDamage(RgStatus *pRobot, RgVector direction, int damageType,
                       float damage)
{
    RgCmd *command;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4449);
    }
    command = _EntryCmdQueue(pRobot->cmdQueue, 8);
    if (command != 0) {
        command->scratch0 = damage;
        command->scratch1 = damageType;
        XrgCopyVector((float *) command, direction);
    }
}

void RgRobotGiveWeakDamage(RgStatus *pRobot, RgVector direction, float damage,
                           float threshold, float windowSeed,
                           float weakHitMotion, float weakHitPower)
{
    RgCmd *command;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 0x117A);
    }
    command = _EntryCmdQueue(pRobot->cmdQueue, 9);
    if (command != 0) {
        command->scratch0 = damage;
        XrgCopyVector((float *) command, direction);
        *(float *) &command->scratch1 = threshold;
        command->scratch2 = windowSeed;
        command->scratch3 = weakHitMotion;
        command->scratch4 = weakHitPower;
    }
}

void RgRobotHitByBody(RgStatus *pRobot, RgVector direction, RgVector position,
                      float damage, float scale)
{
    RgCmd *command;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 0x118C);
    }
    command = _EntryCmdQueue(pRobot->cmdQueue, 0xE);
    if (command != 0) {
        XrgCopyVector(&command->scratch0, direction);
        XrgCopyVector((float *) command, position);
        command->scratch4 = damage;
        command->scratch5 = scale;
    }
}

void RgRobotDropWeapon(RgStatus *pRobot, unsigned int eType)
{
    RgCmd *command;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4508);
    }
    if (eType >= RG_EQUIP_TYPE_NUM) {
        assert_prog(D_00A51A88, D_00A51840, 4509);
    }
    command = _EntryCmdQueue(pRobot->cmdQueue, 7);
    if (command != 0) {
        command->param = eType;
    }
}

void RgRobotHitWeaponAttack(RgStatus *pRobot, int param)
{
    RgCmd *command;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4522);
    }
    command = _EntryCmdQueue(pRobot->cmdQueue, 0xA);
    if (command != 0) {
        command->param = param;
    }
}

extern float XrgNormalizeVector(RgVector destination, RgVector source);

void RgRobotInvalidAttack(RgStatus *pRobot, RgVector direction, float scale)
{
    RgCmd *command;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4536);
    }
    command = _EntryCmdQueue(pRobot->cmdQueue, 0xB);
    if (command != 0) {
        XrgNormalizeVector((float *) command, direction);
        command->scratch0 = scale;
    }
}

void RgRobotHitBG(RgStatus *pRobot, RgVector direction)
{
    RgCmd *command;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4550);
    }
    command = _EntryCmdQueue(pRobot->cmdQueue, 0xC);
    if (command != 0) {
        XrgCopyVector((float *) command, direction);
    }
    if (!(direction[1] > 0.9f)) {
        return;
    }
    _BodyOnGround(pRobot->body);
}

void RgRobotNearBG(RgStatus *pRobot, RgVector direction)
{
    RgCmd *command;

    if (pRobot == 0) {
        assert_prog(D_00A51BB0, D_00A51840, 4568);
    }
    command = _EntryCmdQueue(pRobot->cmdQueue, 0xD);
    if (command != 0) {
        XrgCopyVector((float *) command, direction);
    }
}

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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _MovingPassTime);

static int _AllowAllCmdInMoving(RgRobotStatus *status, int param, RgBody *body);
static int _MovingPassTime(RgRobotStatus *status, RgBody *body, float deltaTime);

static void _InitMovingStatus(RgRobotStatus *status, RgBody *body)
{
    _SetStatus(status, 1, body);
    status->execCmdMethod = _AllowAllCmdInMoving;
    status->passTimeMethod = _MovingPassTime;
    status->scratch0 = -1;
}

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _ExecCmdInAttack);

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _AttackPassTime);

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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _InitAttackStatus);

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

typedef struct RgMotionInfoDB RgMotionInfoDB;
typedef struct RgMotionShotInfo RgMotionShotInfo;
extern RgMotionInfoDB *InstanceOfRgMotionInfoDB(void);
extern RgMotionShotInfo *RgMotionInfoDBGet(RgMotionInfoDB *db, int motionNo,
                                           int charId);
static int _HissatuPassTime(RgRobotStatus *status, RgBody *body,
                            float deltaTime);
static void _HissatuExit(RgRobotStatus *status, RgBody *body);
static void _BodyPlayMotion(RgBody *body, int motion);

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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _ExecCmdInShield);

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _ShieldPassTime);

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

extern const char D_00A51940[];
extern void RgWeaponStopLockon(void *weapon);
static int _ExecCmdInShield(RgRobotStatus *status, int param, RgBody *body);
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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _ExecCmdInTargetting);

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _TargettingPassTime);

extern void RgRobotEffectSetJetInertia(RgRobotEffect *effect, int inertia);

static void _ExitTargettingStatus(RgRobotStatus *status, RgBody *body)
{
    int pad[4];
    (void)status;
    RgRobotEffectTermJet(body->effect);
    RgRobotEffectSetJetInertia(body->effect, 1);
}

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _InitTargettingStatus);

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _ExecCmdInBreaking);

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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _DashExecCmd);

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _DashPassTime);

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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _InitDashStatus);

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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _DashVRExecCmd);

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _DashVRPassTime);

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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _InitDashVRStatus);

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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _InitDamageStatus);

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

extern int RgMotionInfoGetActionInTime(int motion, int actionIndex,
                                       float elapsedTime);
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
extern int RgWeaponGetControlFlag(void *weapon);
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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _EntryCmdQueue);

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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _BodyEquipWeapon);

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _BodySetSpareWeapon);

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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _BodyPassTime);

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _BodyDisp);

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _InitBody);

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _DestructBody);

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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _ExecShotOrAttackCmd);

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

static void _InitDashStatus(RgRobotStatus *status, RgBody *body, RgCmd *command);
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
        _InitDashVRStatus(status, body, command);
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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _DullFlagsPassTime);

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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", _InitRobot);

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

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot", RgRobotPassTime);

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

/*
 * OV12 original TU 26: 0x00a19388..0x00a1c7d8 (90 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_weapon.h"
#include "ov12/rg_matrices_effector.h"

extern void assert_prog(const char *expression, const char *source_file,
                        int line);

/*
 * Scaffold-owned (.rodata still owner: asm, config/tu-build.json
 * data_ownership window 0x00a53642..0x00a53818): the assert_prog expression
 * and source-file strings this allocation's assertions cite (docs/naming.md,
 * "Scaffold-owned data keeps its splat name"). ov12:0x00a536f0 holds
 * "pWpn != NIL", ov12:0x00a53658 holds "../rg_weapon.euc.c", ov12:0x00a537d8
 * holds "0 <= eCharID && eCharID < RG_ACTOR_CHAR_ROBNUM".
 */
extern const char D_00A536F0[];
extern const char D_00A53658[];
extern const char D_00A537D8[];

/*
 * Scaffold-owned (.rodata still owner: asm, config/tu-build.json
 * data_ownership window 0x00a53642..0x00a53818): the assert_prog expression
 * strings the ov12:0x00a1a6e8..0x00a1b52c allocation's create-function
 * assertions cite. ov12:0x00a53710 holds "pEss != NIL", ov12:0x00a53720
 * holds "pInfo != NIL".
 */
extern const char D_00A53710[];
extern const char D_00A53720[];

/*
 * Scaffold-owned (.rodata still owner: asm, config/tu-build.json
 * data_ownership window 0x00a53642..0x00a53818): the assert_prog
 * expression string _GetShotInfo's assertion cites. ov12:0x00a53808 holds
 * "pRobot != NIL".
 */
extern const char D_00A53808[];

extern RgHeap *InstanceOfRgHeap(void);
extern void *RgHeapAlloc(RgHeap *heap, unsigned int size,
                         const char *source_file, int line);
extern void RgHeapFree(RgHeap *heap, void *ptr, const char *source_file,
                       int line);
extern void _DestructWeaponCommon(RgWeapon *weapon);
extern void _PlayShotMotion(RgWeapon *weapon);
extern void XrgActorSetMotion(XrgActor *actor, int motion);
extern XrgActor *RgRobotGetActor(int robotId);
extern void XrgActorDisposeEffector(XrgActor *actor, int effector);
extern int RgBgObjTryToBreak(int bgObject, float damage);
extern void _InitWeaponAttack(RgWeaponAttackType *weapon,
                               RgWeaponAttackEssence *essence,
                               RgWeaponCreateInfo *info);
extern void _InitWeaponUnArmedType(RgWeapon *weapon,
                                    RgWeaponUnArmedEssence *essence,
                                    RgWeaponCreateInfo *info);
extern void _InitWeaponShield(RgWeapon *weapon, RgWeaponShieldEssence *essence,
                               RgWeaponCreateInfo *info);
extern void _InitEnergyType(RgWeaponEnergyType *weapon,
                             RgWeaponEnergyEssence *essence,
                             RgWeaponCreateInfo *info);
extern void _InitEssenceCommon(void *essence);
static int _GetNumAttach(RgWeaponAttach *pAttach);
extern void _CommonPassTime(RgWeapon *weapon, float dt);
extern int _GetDefShotPosDir(RgWeaponShotRequest *info, RgVector position, RgVector direction);
extern int RgRobotGetCharID(int robotId);
extern void *RgRobotGetTarget(int robotId);
extern int RgRobotIsConfused(int robotId);
extern void XrgSoundRingVol(int driver, int soundId, int volume);
extern void XrgActorPassTime(XrgActor *actor, float dt);

/*
 * Scaffold-owned (.rodata still owner: asm, config/tu-build.json
 * data_ownership window 0x00a53642..0x00a53818): the assert_prog expression
 * string _InitAttach/_GetNumAttach's assertions cite. ov12:0x00a53648 holds
 * "pAttach != NIL".
 */
extern const char D_00A53648[];

static void _InitAttach(RgWeaponAttach *pAttach)
{
    if (pAttach == 0) {
        assert_prog(D_00A53648, D_00A53658, 38);
    }
    pAttach->m_uAttachNum = 0;
}

extern void RgShotRelease(RgShot *shot);

static void _ReleaseAttach(RgWeaponAttach *pAttach)
{
    unsigned int i;

    if (pAttach == 0) {
        assert_prog(D_00A53648, D_00A53658, 46);
    }
    for (i = 0; i < pAttach->m_uAttachNum; i++) {
        RgShotRelease(pAttach->shots[i]);
    }
}

static int _GetNumAttach(RgWeaponAttach *pAttach)
{
    if (pAttach == 0) {
        assert_prog(D_00A53648, D_00A53658, 54);
    }
    return pAttach->m_uAttachNum;
}

static unsigned int _GetAllAttach(RgWeaponAttach *pAttach, RgShot **shots)
{
    unsigned int i;

    if (pAttach == 0) {
        assert_prog(D_00A53648, D_00A53658, 62);
    }
    for (i = 0; i < pAttach->m_uAttachNum; i++) {
        shots[i] = pAttach->shots[i];
    }
    return pAttach->m_uAttachNum;
}

extern const char D_00A53670[];

static void _AddShotAttach(RgWeaponAttach *pAttach, RgShot *pShot)
{
    if (pAttach == 0) {
        assert_prog(D_00A53648, D_00A53658, 0x47);
    }
    if (pShot != 0) {
        if (pAttach->m_uAttachNum >= 8) {
            assert_prog(D_00A53670, D_00A53658, 0x49);
        }
        pAttach->shots[pAttach->m_uAttachNum++] = pShot;
    }
}

extern int RgShotIsReleased(RgShot *shot);

static void _CheckAttach(RgWeaponAttach *pAttach)
{
    RgShot *tmp[8];
    unsigned int i;
    unsigned int n;

    if (pAttach == 0) {
        assert_prog(D_00A53648, D_00A53658, 0x53);
    }
    n = 0;
    for (i = 0; i < pAttach->m_uAttachNum; i++) {
        RgShot *s = pAttach->shots[i];
        if (RgShotIsReleased(s) == 0) {
            tmp[n++] = s;
        }
    }
    for (i = 0; i < n; i++) {
        pAttach->shots[i] = tmp[i];
    }
    pAttach->m_uAttachNum = n;
}

void _CommonDispose(void)
{
}

void _CommonPassTime(RgWeapon *weapon, float dt)
{
    XrgActor *actor;
    int charID = _GetParentCharID(weapon);
    int motion;

    actor = weapon->actor;
    if (actor != 0 && charID >= 0 && charID < 6) {
        { int off = weapon->shotMotionOffset; motion = ((int *) weapon)[0x13c / 4 + charID * 3 + off]; }
        if (motion != -1) {
            if (XrgActorIsEndOfMotion(actor) != 0) {
                XrgActorSetMotion(weapon->actor, motion);
            }
        }
    }
    _CheckAttach(&weapon->shotAttach);
}

/* Opaque here: rg_matrices_effector.c (ov12/tu068) owns MatrixConstraint's definition. */
typedef struct MatrixConstraint MatrixConstraint;
extern void RgMatricesEffectorSetActivity(MatrixEffector *effector, int activity);
extern void RgMatEffConstraintSetTarget(MatrixConstraint *constraint, RgVector target);
extern void XrgClearVector(RgVector vector);
extern void XrgAddVector(RgVector destination, RgVector first, RgVector second);
extern void XrgNormalizeVector(RgVector destination, RgVector source);
extern void XrgScaleVector(RgVector destination, RgVector source, float scale);
extern void XrgSubVector(RgVector destination, RgVector first, RgVector second);
/* Opaque here: rg_geom_point.c (ov12/tu052) owns the RgPointVector definition. */
typedef struct RgPointVector RgPointVector;
extern void __RgGeomPointGetPos(RgGeomPoint *point, RgPointVector *destination,
                                const char *source_file, int source_line);
extern int _GetDefShotPosDirFunc(RgWeapon *weapon, RgVector position, RgVector direction);
void _CheckLockOn(RgWeapon *weapon);

void _CommonDisp(RgWeapon *weapon)
{
    RgVector position;
    RgVector fromPosition;
    RgVector toPosition;
    RgVector aimPoint;
    void *target;
    int robotId;

    robotId = weapon->robotId;
    _CheckLockOn(weapon);
    if (weapon->lockedOn != 0) {
        target = RgRobotGetTarget(robotId);
        if (target != 0) {
            __RgGeomPointGetPos(target, (RgPointVector *) position, D_00A53658, 287);
        } else {
            XrgClearVector(position);
        }
        if (weapon->lockOnEffector != 0) {
            RgMatricesEffectorSetActivity((MatrixEffector *) weapon->lockOnEffector, 1);
            RgMatEffConstraintSetTarget((MatrixConstraint *) weapon->lockOnEffector, position);
        }
    } else if (weapon->lockOnEffector != 0) {
        if (weapon->equipped != 0 && (weapon->controlFlags & 0x20)) {
            _GetDefShotPosDirFunc(weapon, fromPosition, toPosition);
            XrgSubVector(aimPoint, toPosition, fromPosition);
            XrgNormalizeVector(aimPoint, aimPoint);
            XrgScaleVector(aimPoint, aimPoint, 100.0f);
            XrgAddVector(aimPoint, fromPosition, aimPoint);
            RgMatricesEffectorSetActivity((MatrixEffector *) weapon->lockOnEffector, 1);
            RgMatEffConstraintSetTarget((MatrixConstraint *) weapon->lockOnEffector, aimPoint);
        } else {
            RgMatricesEffectorSetActivity((MatrixEffector *) weapon->lockOnEffector, 0);
        }
    }
}

typedef struct EssenceCommonView {
    int f_00;
    int f_04;
    float f_08;
    float f_0c;
    int f_10;
    float f_14;
    int f_18;
    int f_1c;
    int f_20;
    int f_24;
    unsigned char pad_28[0x40];
    int f_68;
    unsigned char pad_6c[4];
    unsigned char equip[0x38];
    int f_a8;
    float f_ac;
    int table[6][3];
    int tableA[6][3];
    int tableB[6][3];
    int f_188;
    unsigned char pad_18c[4];
    unsigned char actor0[0x88];
    unsigned char actor1[0x88];
    unsigned char actor2[0x88];
    unsigned char pad_328[8];
    char f_330;
    unsigned char pad_331[0x1f];
    char f_350;
    unsigned char pad_351[0x2f];
    int f_380;
    int f_384;
    int f_388;
    int f_38c;
    int f_390;
    int f_394;
    int f_398;
    int f_39c;
    int f_3a0;
    int f_3a4;
} EssenceCommonView;

extern const char D_00A53698[];
extern XrgActor *CreateXrgActor(void *essence);
extern void CopyRgEquip(void *dst, void *src);
static inline float _NormAngle(float a)
{
    if (a > 3.1415927f) {
        do {
            a -= 6.2831855f;
        } while (a > 3.1415927f);
    }
    if (a < -3.1415927f) {
        do {
            a += 6.2831855f;
        } while (a < -3.1415927f);
    }
    return a;
}

void _InitWeaponCommon(void *weapon, void *essence, void *info)
{
    RgWeapon *w = (RgWeapon *)weapon;
    EssenceCommonView *e = (EssenceCommonView *)essence;
    int *in = (int *)info;
    unsigned int type;
    int i;
    int j;
    unsigned int mode;
    int flags, robotId, u38, u0c, hasActor;
    float weight;

    type = in[1];
    if (type >= 3) {
        assert_prog(D_00A53698, D_00A53658, 0x149);
    }
    w->id = in[2];
    ((union { int i; float f; } *)&w->shotNum)->f = e->f_14;
    w->shotRequested = 0;
    w->lockedOn = 0;
    w->targeting = 0;
    flags = e->f_04;
    robotId = in[0];
    u38 = e->f_10;
    u0c = in[3];
    weight = ((union { int i; float f; } *)&e->f_ac)->f;
    hasActor = e->f_188;
    w->controlFlags = flags;
    w->robotId = robotId;
    *(int *)w->unmodeled_38 = u38;
    *(int *)w->unmodeled_0c = u0c;
    *(int *)&w->dispose = (int)_CommonDispose;
    *(int *)w->unmodeled_18 = (int)_CommonDisp;
    ((union { int i; float f; } *)&w->weight)->f = weight;
    w->ess = (int)essence;
    *(int *)&w->shoot = 0;
    *(int *)&w->passTime = 0;
    *(int *)&w->shotStop = 0;
    *(int *)&w->hitRobot = 0;
    *(int *)&w->hitBG = 0;
    *(int *)w->unmodeled_30 = 0;
    *(int *)&w->setFree = 0;
    w->shotMotionOffset = type;
    if (hasActor != 0) {
        w->actor = CreateXrgActor(e->actor0 + type * 0x88);
    } else {
        w->actor = 0;
    }
    _InitAttach(&w->shotAttach);
    *(int *)w->unmodeled_64 = 0;
    CopyRgEquip(&w->equip, e->equip);
    *(int *)&w->equip.unmodeled_00[0x38] = e->f_a8;
    w->lockOnEffector = 0;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 6; j++) {
            w->shotMotionTable[j][i] = e->table[j][i];
            w->shotStartMotionTable[j][i] = e->tableA[j][i];
            w->shotMotionTable[j + 12][i] = e->table[j + 12][i];
        }
    }
    mode = in[1];
    if (mode == 0) {
        w->lockOnAngleMin = _NormAngle(e->f_08);
        w->lockOnAngleMax = _NormAngle(e->f_0c);
    } else if (mode == 1) {
        w->lockOnAngleMin = _NormAngle(-e->f_0c);
        w->lockOnAngleMax = _NormAngle(-e->f_08);
    } else {
        w->lockOnAngleMin = _NormAngle(e->f_08);
        w->lockOnAngleMax = _NormAngle(e->f_0c);
    }
    *(void **)&w->equipped = 0;
    w->robotHitSound = e->f_384;
    *(void **)&w->lockOnStopped = 0;
    *(void **)&w->sound = 0;
    w->bgHitSound = e->f_388;
    *(int *)w->unmodeled_1d4 = e->f_38c;
    w->shotSound = e->f_380;
    w->robotHitVolume = e->f_394;
    ((int *)w->unmodeled_1e0)[0] = e->f_398;
    ((int *)w->unmodeled_1e0)[1] = e->f_39c;
    w->shotVolume = e->f_390;
}

extern void DisposeXrgActor(XrgActor *actor);

void _DestructWeaponCommon(RgWeapon *weapon)
{
    XrgActor *actor;

    _ReleaseAttach(&weapon->shotAttach);
    actor = weapon->actor;
    if (actor != 0) {
        DisposeXrgActor(actor);
    }
    if (weapon->lockOnEffector == 0) {
        return;
    }
    if (weapon->robotId == 0) {
        return;
    }
    XrgActorDisposeEffector(RgRobotGetActor(weapon->robotId), weapon->lockOnEffector);
}

extern void InitRgEquip(void *equip);
extern void InitXrgActorEssence(void *ess, int id);
void _InitEssenceCommon(void *essence)
{
    EssenceCommonView *e = (EssenceCommonView *)essence;
    unsigned int i;
    unsigned int j;

    e->f_20 = 3;
    e->f_08 = -0.3926991f;
    e->f_0c = 1.1780972f;
    e->f_14 = 50.0f;
    e->f_00 = 0;
    e->f_04 = 0;
    e->f_10 = 0;
    e->f_18 = 0;
    e->f_1c = 0;
    e->f_24 = 0;
    e->f_68 = 0;
    InitRgEquip(e->equip);
    e->f_a8 = -1;
    for (i = 0; i < 6; i++) {
        e->table[i][0] = 0x27;
        e->table[i][1] = 0x28;
        e->table[i][2] = -1;
        for (j = 0; j < 3; j++) {
            e->tableA[i][j] = -1;
            e->tableB[i][j] = -1;
        }
    }
    e->f_188 = 1;
    InitXrgActorEssence(e->actor0, -1);
    InitXrgActorEssence(e->actor1, -1);
    InitXrgActorEssence(e->actor2, -1);
    e->f_ac = 1.0f;
    e->f_350 = 0;
    e->f_330 = 0;
    e->f_380 = 0x15;
    e->f_384 = 0x16;
    e->f_388 = 0x16;
    e->f_38c = 0;
    e->f_390 = 1;
    e->f_394 = 0x80;
    e->f_398 = 0x80;
    e->f_39c = 0x80;
    e->f_3a0 = -1;
    e->f_3a4 = 0;
}

extern const char D_00A536E0[];
extern void _GetShotInfo(RgWeapon *weapon, RgWeaponShotRequest *info);
extern void *CreateRgShotFromEssence(void *essence, void *info);
extern void RgShotSetSoundDriver(RgShot *shot, int driver);
extern void RgShotSetHitSound(RgShot *shot, int soundId, int volume);
extern void RgShotSetBgHitSound(RgShot *shot, int soundId, int volume);
extern void RgShotSetNoLifeSound(RgShot *shot, int soundId, int volume);
extern void RgShotSetHideSound(RgShot *shot, int soundId);

int _ShotTypeShot(RgWeapon *w)
{
    RgWeaponShotType *weapon = (RgWeaponShotType *)w;
    RgWeaponShotRequest info;
    RgShot *shot;
    void *ess;
    int ret;

    if (weapon == 0) {
        assert_prog(D_00A536E0, D_00A53658, 0x208);
    }
    if (weapon->cooldown > 0.0f) {
        return 0;
    }
    if (_GetNumAttach(&weapon->common.shotAttach) == 0) {
        _GetShotInfo(&weapon->common, &info);
        ess = *(void **)weapon->common.unmodeled_38;
        ret = 0;
        if (ess != 0) {
            shot = CreateRgShotFromEssence(ess, &info);
            _AddShotAttach(&weapon->common.shotAttach, shot);
            RgShotSetSoundDriver(shot, weapon->common.sound);
            RgShotSetHitSound(shot, weapon->common.robotHitSound, weapon->common.robotHitVolume);
            RgShotSetBgHitSound(shot, weapon->common.bgHitSound, *(int *)weapon->common.unmodeled_1e0);
            RgShotSetNoLifeSound(shot, *(int *)weapon->common.unmodeled_1d4, *(int *)(weapon->common.unmodeled_1e0 + 4));
            RgShotSetHideSound(shot, weapon->common.shotSound);
            XrgSoundRingVol(weapon->common.sound, weapon->common.shotSound, weapon->common.shotVolume);
            if (weapon->common.shotNum != 1e8f) {
                weapon->common.shotNum = weapon->common.shotNum - 1.0f;
            }
            ret = 1;
            weapon->cooldown = *(float *)weapon->unmodeled_1f0;
        }
    } else {
        ret = 0;
    }
    return ret;
}

int _ShotTypeIsBusy(RgWeaponShotType *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 572);
    }
    return weapon->cooldown > 0.0f;
}

void _ShotTypeFree(RgWeaponShotType *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 580);
    }
    weapon->cooldown = 0.0f;
}

void _ShotTypePassTime(RgWeaponShotType *weapon, float dt)
{
    float cooldown;

    if (_GetNumAttach(&weapon->common.shotAttach) == 0) {
        cooldown = weapon->cooldown - dt;
        weapon->cooldown = cooldown;
        if (cooldown < 0.0f) {
            weapon->cooldown = 0.0f;
        }
    }
}

extern const char D_00A53700[];
extern int XrgActorCreateEffConstraint(XrgActor *actor, int kind, XrgActor *target);

void _InitWeaponCommonEffector(RgWeaponShotType *weapon, int a, int b)
{
    RgWeapon *w = &weapon->common;
    XrgActor *act;
    XrgActor *tgt;

    if (w == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x25F);
    }
    if (a == 0) {
        assert_prog(D_00A53700, D_00A53658, 0x260);
    }
    if ((w->controlFlags & 7) != 1 && !(w->controlFlags & 0x80)) {
        act = RgRobotGetActor(a);
        tgt = w->actor;
        if (b == 0) {
            w->lockOnEffector = XrgActorCreateEffConstraint(act, 0, tgt);
        } else if (b == 1) {
            w->lockOnEffector = XrgActorCreateEffConstraint(act, 1, tgt);
        }
    }
}

extern void _InitWeaponCommonEffector(RgWeaponShotType *weapon, int a, int b);
extern int _ShotTypeShot(RgWeapon *weapon);

void _InitWeaponShot(RgWeaponShotType *weapon, RgWeaponShotEssence *essence, RgWeaponCreateInfo *info)
{
    RgWeapon *w = &weapon->common;
    _InitWeaponCommon(w, essence, info);
    w->shoot = _ShotTypeShot;
    w->passTime = (void (*)(RgWeapon *, float))_ShotTypePassTime;
    *(int (**)(RgWeaponShotType *))w->unmodeled_30 = _ShotTypeIsBusy;
    w->setFree = (void (*)(RgWeapon *))_ShotTypeFree;
    *(float *)weapon->unmodeled_1f0 = *(float *)((char *)essence + 0x3B0);
    weapon->cooldown = 0.0f;
    _InitWeaponCommonEffector(weapon, ((int *)info)[0], ((int *)info)[1]);
}

extern void _InitWeaponShot(RgWeaponShotType *weapon, RgWeaponShotEssence *essence,
                             RgWeaponCreateInfo *info);

RgWeaponShotType *_CreateWeaponShotType(RgWeaponShotEssence *pEss,
                                         RgWeaponCreateInfo *pInfo)
{
    RgWeaponShotType *pWpn;

    pWpn = RgHeapAlloc(InstanceOfRgHeap(), 0x200, D_00A53658, 668);
    if (pEss == 0) {
        assert_prog(D_00A53710, D_00A53658, 671);
    }
    if (pWpn == 0) {
        assert_prog(D_00A536F0, D_00A53658, 672);
    }
    if (pInfo == 0) {
        assert_prog(D_00A53720, D_00A53658, 673);
    }
    _InitWeaponShot(pWpn, pEss, pInfo);
    return pWpn;
}

void InitRgWeaponShotEssence(RgWeaponShotEssence *pEss)
{
    RgWeaponShotEssenceInit *init = (RgWeaponShotEssenceInit *) pEss;

    if (pEss == 0) {
        assert_prog(D_00A53710, D_00A53658, 684);
    }
    _InitEssenceCommon(pEss);
    init->m_pCreateMethod = _CreateWeaponShotType;
    init->controlMode = 0x20;
    init->busyTime = 0.4f;
}

int _ShotAttackType(RgWeaponAttackType *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 725);
    }
    if (weapon->shotSoundPlaying == 0) {
        weapon->shotSoundPlaying = 1;
        XrgSoundRingVol(weapon->common.sound, weapon->common.shotSound,
                        weapon->common.shotVolume);
    }
    return 0;
}

void _ShotStopAttackType(RgWeaponAttackType *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 737);
    }
    weapon->shotSoundPlaying = 0;
    weapon->active = 1;
}

extern int RgGeomGetStatus(int geom);
extern void RgGeomPointMovePos(int geom, RgVector position);
extern void RgGeomPointSetPos(int geom, RgVector position);
extern void RgGeomResetStatus(int geom, int status);
extern void RgGeomSetStatus(int geom, int status);
extern int XrgActorGetJointLocal(XrgActor *actor, int joint, RgMatrix local);
extern void XrgCopyVector(RgVector destination, RgVector source);

void _PassTimeAttackType(RgWeaponAttackType *weapon)
{
    /*
     * XrgActorGetJointLocal fills the whole 4x4 local transform of the
     * followed joint; only its translation row (the last four floats) is a
     * position, read below through &jointLocal[12].
     */
    RgMatrix jointLocal;

    if (weapon->followActor != 0 &&
            XrgActorGetJointLocal(weapon->followActor, weapon->followJoint, jointLocal) != 0) {
        /*
         * shotRequested and active are the original's two separate early-exit
         * tests, not one "if / else if" condition: folding them into a single
         * else-if compiles the guard scheduling for this pair differently, so
         * shotRequested's test keeps its own goto to the shared reset call.
         */
        if (weapon->common.shotRequested == 0) {
            goto resetStatus;
        }
        if (weapon->active != 0) {
            if (!(RgGeomGetStatus(weapon->geom) & 1)) {
                RgGeomSetStatus(weapon->geom, 1);
                XrgCopyVector(weapon->followPosition, &jointLocal[12]);
            }
            RgGeomPointSetPos(weapon->geom, weapon->followPosition);
            RgGeomPointMovePos(weapon->geom, &jointLocal[12]);
            XrgCopyVector(weapon->followPosition, &jointLocal[12]);
        } else {
resetStatus:
            RgGeomResetStatus(weapon->geom, 1);
        }
    }
}

extern void RgGeomFree(int geom);

void _DestructAttackType(RgWeaponAttackType *weapon)
{
    RgGeomFree(weapon->geom);
}

void _HitBgAttackType(RgWeaponAttackType *weapon, int bgObject)
{
    if (RgBgObjTryToBreak(bgObject, weapon->damage) != 0) {
        weapon->active = 0;
    }
}

extern void CreateRgHitEffectPosDir(int id, RgVector direction, void *param3, void *param4);
extern int RgRobotGetGeom(int robotId);
extern void RgRobotGiveDamage(int robotId, RgVector direction, int flags, float damage);
extern void RgRobotInvalidAttack(int robotId, RgVector direction, float damage);
extern int RgRobotIsInvalidAttack(int robotId, RgVector direction);
extern unsigned char *RgWeaponEssCastToAttack(int ess);
extern unsigned char D_00A53730[];
extern unsigned char D_00A53740[];

void _HitRobotAttackType(RgWeaponAttackType *weapon, int robotId, int damage)
{
    RgVector ownPosition;
    RgVector targetPosition;
    RgVector direction;
    int ownGeom;
    int targetGeom;
    unsigned char *attackEssence;

    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 802);
    }
    if (weapon->common.robotId != 0) {
        ownGeom = RgRobotGetGeom(weapon->common.robotId);
        targetGeom = RgRobotGetGeom(robotId);
        __RgGeomPointGetPos((RgGeomPoint *) ownGeom, (RgPointVector *) ownPosition, D_00A53658, 811);
        __RgGeomPointGetPos((RgGeomPoint *) targetGeom, (RgPointVector *) targetPosition, D_00A53658, 812);
        weapon->active = 0;
        XrgSubVector(direction, ownPosition, targetPosition);
        if (RgRobotIsInvalidAttack(robotId, direction) == 0) {
            attackEssence = RgWeaponEssCastToAttack(weapon->common.ess);
            if (attackEssence != 0) {
                CreateRgHitEffectPosDir(damage, direction, attackEssence + 0x3bc, D_00A53730);
            }
            RgRobotGiveDamage(robotId, direction, 0, weapon->damage);
            XrgSoundRingVol(weapon->common.sound, weapon->common.robotHitSound, weapon->common.robotHitVolume);
            return;
        }
        CreateRgHitEffectPosDir(damage, direction, D_00A53740, 0);
        RgRobotInvalidAttack(robotId, direction, weapon->damage);
    }
}

INCLUDE_ASM("asm/nonmatchings/ov12/rg_weapon", _InitWeaponAttack);

RgWeaponAttackType *_CreateWeaponAttackType(RgWeaponAttackEssence *pEss,
                                             RgWeaponCreateInfo *pInfo)
{
    RgWeaponAttackType *pWpn;

    pWpn = RgHeapAlloc(InstanceOfRgHeap(), 0x230, D_00A53658, 889);
    if (pEss == 0) {
        assert_prog(D_00A53710, D_00A53658, 892);
    }
    if (pWpn == 0) {
        assert_prog(D_00A536F0, D_00A53658, 893);
    }
    if (pInfo == 0) {
        assert_prog(D_00A53720, D_00A53658, 894);
    }
    _InitWeaponAttack(pWpn, pEss, pInfo);
    return pWpn;
}

typedef struct RgWeaponAttackEssenceInit {
    RgWeaponAttackType *(*m_pCreateMethod)(RgWeaponAttackEssence *pEss,
                                            RgWeaponCreateInfo *pInfo);
    int controlMode;
    unsigned char unmodeled_08[0xa8];
    int shotMotionTable[RG_ACTOR_CHAR_ROBNUM][3];
    unsigned char unmodeled_f8[0x2b8];
    int field_3b0;
    int field_3b4;
    float field_3b8;
    char field_3bc;
} RgWeaponAttackEssenceInit;

void InitRgWeaponAttackEssence(void *pEss)
{
    RgWeaponAttackEssenceInit *init = (RgWeaponAttackEssenceInit *) pEss;
    unsigned int i;

    if (pEss == 0) {
        assert_prog(D_00A53710, D_00A53658, 905);
    }
    _InitEssenceCommon(pEss);
    init->m_pCreateMethod = _CreateWeaponAttackType;
    for (i = 0; i < RG_ACTOR_CHAR_ROBNUM; i++) {
        init->shotMotionTable[i][0] = 0x2B;
        init->shotMotionTable[i][1] = 0x2C;
        init->shotMotionTable[i][2] = -1;
    }
    init->controlMode = 0x4A;
    init->field_3b8 = 10.0f;
    init->field_3bc = 0;
    init->field_3b0 = 8;
    init->field_3b4 = 8;
}

void _InitWeaponUnArmedType(RgWeapon *weapon, RgWeaponUnArmedEssence *essence, RgWeaponCreateInfo *info)
{
    int *pInfo;
    int charId;

    _InitWeaponAttack((RgWeaponAttackType *)weapon, (RgWeaponAttackEssence *)essence, info);
    pInfo = (int *)info;
    *(XrgActor **)((char *)weapon + 0x1F8) = RgRobotGetActor(pInfo[0]);
    charId = RgRobotGetCharID(pInfo[0]);
    if (charId == 3) {
        if (pInfo[1] == 0) {
            *(int *)((char *)weapon + 0x1F0) = 2;
        } else if (pInfo[1] == 1) {
            *(int *)((char *)weapon + 0x1F0) = 3;
        } else {
            *(int *)((char *)weapon + 0x1F0) = 0x14;
        }
    } else {
        if (pInfo[1] == 0) {
            *(int *)((char *)weapon + 0x1F0) = 0;
        } else if (pInfo[1] == 1) {
            *(int *)((char *)weapon + 0x1F0) = 1;
        } else {
            *(int *)((char *)weapon + 0x1F0) = 0x14;
        }
    }
}

RgWeapon *_CreateWeaponUnArmedType(RgWeaponUnArmedEssence *pEss,
                                    RgWeaponCreateInfo *pInfo)
{
    RgWeapon *pWpn;

    pWpn = RgHeapAlloc(InstanceOfRgHeap(), 0x230, D_00A53658, 969);
    if (pEss == 0) {
        assert_prog(D_00A53710, D_00A53658, 972);
    }
    if (pWpn == 0) {
        assert_prog(D_00A536F0, D_00A53658, 973);
    }
    if (pInfo == 0) {
        assert_prog(D_00A53720, D_00A53658, 974);
    }
    _InitWeaponUnArmedType(pWpn, pEss, pInfo);
    return pWpn;
}

extern void InitRgWeaponAttackEssence(void *essence);

void InitRgWeaponUnArmedEssence(RgWeaponUnArmedEssence *pEss)
{
    /*
     * pEss really addresses the RgWeaponUnArmedEssenceInit layout this
     * allocation evidences (see the header: the tag RgWeaponUnArmedEssence
     * itself is left opaque because ov12/tu027 already completes it for its
     * own, unrelated, non-overlapping field layout).
     */
    RgWeaponUnArmedEssenceInit *init = (RgWeaponUnArmedEssenceInit *) pEss;
    unsigned int i;

    if (pEss == 0) {
        assert_prog(D_00A53710, D_00A53658, 988);
    }
    InitRgWeaponAttackEssence(pEss);
    init->m_pCreateMethod = _CreateWeaponUnArmedType;
    init->resetFlag = 0;
    init->controlMode = 76;
    for (i = 0; i < RG_ACTOR_CHAR_ROBNUM; i++) {
        init->shotMotionTable[i][0] = 0x15;
        init->shotMotionTable[i][1] = 0x16;
        init->shotMotionTable[i][2] = -1;
    }
    init->terminator = -1;
}

int _ShotShieldType(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 1026);
    }
    return 0;
}

void _ShotStopShieldType(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 1033);
    }
}

void _PassTimeShieldType(RgWeapon *weapon)
{
}

void _DestructShieldType(RgWeapon *weapon)
{
}

int _ShotShieldType(RgWeapon *weapon);
void _ShotStopShieldType(RgWeapon *weapon);
void _PassTimeShieldType(RgWeapon *weapon);
void _DestructShieldType(RgWeapon *weapon);

void _InitWeaponShield(RgWeapon *weapon, RgWeaponShieldEssence *essence, RgWeaponCreateInfo *info)
{
    _InitWeaponCommon(weapon, essence, info);
    weapon->passTime = (void (*)(RgWeapon *, float))_PassTimeShieldType;
    weapon->dispose = _DestructShieldType;
    weapon->shoot = _ShotShieldType;
    weapon->lockedOn = 0;
    weapon->targeting = 0;
    *(int *)weapon->unmodeled_38 = 0;
    weapon->shotStop = _ShotStopShieldType;
}

RgWeapon *_CreateWeaponShieldType(RgWeaponShieldEssence *pEss,
                                   RgWeaponCreateInfo *pInfo)
{
    RgWeapon *pWpn;

    pWpn = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgWeapon), D_00A53658, 1068);
    if (pEss == 0) {
        assert_prog(D_00A53710, D_00A53658, 1071);
    }
    if (pWpn == 0) {
        assert_prog(D_00A536F0, D_00A53658, 1072);
    }
    if (pInfo == 0) {
        assert_prog(D_00A53720, D_00A53658, 1073);
    }
    _InitWeaponShield(pWpn, pEss, pInfo);
    return pWpn;
}

void InitRgWeaponShieldEssence(RgWeaponShieldEssence *pEss)
{
    unsigned int i;

    if (pEss == 0) {
        assert_prog(D_00A53710, D_00A53658, 1084);
    }
    _InitEssenceCommon(pEss);
    pEss->m_pCreateMethod = _CreateWeaponShieldType;
    for (i = 0; i < RG_ACTOR_CHAR_ROBNUM; i++) {
        pEss->shotMotionTable[i][0] = 0x2B;
        pEss->shotMotionTable[i][1] = 0x2C;
        pEss->shotMotionTable[i][2] = -1;
    }
    pEss->controlMode = 3;
}

int _EnergyIsBusy(RgWeaponEnergyType *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 1120);
    }
    if (_GetNumAttach(&weapon->common.shotAttach) != 0) {
        return 1;
    }
    return weapon->active != 0;
}

void _EnergyFree(RgWeaponEnergyType *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 1138);
    }
    weapon->active = 0;
    weapon->energyCharge = 0;
}

int _EnergyShot(int p)
{
    *(int *)(p + 0x1F4) = 1;
    if (_GetNumAttach(p + 0x40) != 0) return 0;
    return *(int *)(p + 0x1F0) == 0;
}

typedef struct RgWeaponEnergyView {
    RgWeapon common;
    int active;
    int requested;
    float charge;
    float shotDecay;
    float chargeRate;
    float chargeMax;
    float dischargeRate;
} RgWeaponEnergyView;

void _EnergyPassTime(RgWeapon *w, float dt)
{
    RgWeaponEnergyView *weapon = (RgWeaponEnergyView *)w;
    RgWeaponShotRequest info;
    RgShot *shot;
    RgWeaponAttach *attach;
    int idle;
    int req;
    float v;

    idle = 1;
    if (weapon->active != 0) {
        _ReleaseAttach(&weapon->common.shotAttach);
    } else {
        attach = &weapon->common.shotAttach;
        if (_GetNumAttach(attach) != 0) {
            if (weapon->requested == 0) {
                _ReleaseAttach(attach);
            } else {
                idle = 0;
            }
        } else if (weapon->requested != 0 && weapon->common.shotRequested != 0) {
            _GetShotInfo(&weapon->common, &info);
            shot = CreateRgShotFromEssence(*(void **)weapon->common.unmodeled_38, &info);
            RgShotSetSoundDriver(shot, weapon->common.sound);
            RgShotSetHitSound(shot, weapon->common.robotHitSound, weapon->common.robotHitVolume);
            RgShotSetBgHitSound(shot, weapon->common.bgHitSound, *(int *)weapon->common.unmodeled_1e0);
            RgShotSetNoLifeSound(shot, *(int *)weapon->common.unmodeled_1d4, *(int *)(weapon->common.unmodeled_1e0 + 4));
            RgShotSetHideSound(shot, weapon->common.shotSound);
            XrgSoundRingVol(weapon->common.sound, weapon->common.shotSound, weapon->common.shotVolume);
            if (shot != 0 && RgShotIsReleased(shot) == 0) {
                _AddShotAttach(attach, shot);
            }
            idle = 0;
        }
    }
    if (idle == 0) {
        if (weapon->common.shotNum != 1e8f) {
            weapon->common.shotNum = weapon->common.shotNum - weapon->shotDecay * dt;
        }
        v = weapon->charge + weapon->chargeRate * dt;
        weapon->charge = v;
        if (weapon->chargeMax < v) {
            weapon->active = 1;
        }
    } else {
        weapon->charge = weapon->charge - weapon->dischargeRate * dt;
        if (weapon->charge < 0.0f) {
            weapon->charge = 0.0f;
            weapon->active = 0;
        }
    }
    weapon->requested = 0;
}

void _InitEnergyType(RgWeaponEnergyType *weapon, RgWeaponEnergyEssence *essence, RgWeaponCreateInfo *info)
{
    RgWeaponEnergyView *e = (RgWeaponEnergyView *)weapon;
    RgWeapon *w = &weapon->common;

    _InitWeaponCommon(w, essence, info);
    e->charge = 0.0f;
    e->shotDecay = *(float *)((char *)essence + 0x3B0);
    w->shoot = (int (*)(RgWeapon *))_EnergyShot;
    w->passTime = _EnergyPassTime;
    *(int (**)(RgWeaponEnergyType *))w->unmodeled_30 = _EnergyIsBusy;
    w->setFree = (void (*)(RgWeapon *))_EnergyFree;
    e->chargeRate = *(float *)((char *)essence + 0x3B4);
    *(volatile float *)&e->chargeMax = *(float *)((char *)essence + 0x3B8);
    *(volatile int *)&e->active = 0;
    e->dischargeRate = *(float *)((char *)essence + 0x3BC);
    ((union { int i; float f; } *)&e->requested)->i = 0;
    _InitWeaponCommonEffector((RgWeaponShotType *)weapon, ((int *)info)[0], ((int *)info)[1]);
}

RgWeaponEnergyType *_CreateWeaponEnergyType(RgWeaponEnergyEssence *pEss,
                                             RgWeaponCreateInfo *pInfo)
{
    RgWeaponEnergyType *pWpn;

    pWpn = RgHeapAlloc(InstanceOfRgHeap(), 0x210, D_00A53658, 1302);
    if (pEss == 0) {
        assert_prog(D_00A53710, D_00A53658, 1305);
    }
    if (pWpn == 0) {
        assert_prog(D_00A536F0, D_00A53658, 1306);
    }
    if (pInfo == 0) {
        assert_prog(D_00A53720, D_00A53658, 1307);
    }
    _InitEnergyType(pWpn, pEss, pInfo);
    return pWpn;
}

typedef struct RgWeaponEnergyEssenceInit {
    RgWeaponEnergyType *(*m_pCreateMethod)(RgWeaponEnergyEssence *pEss,
                                            RgWeaponCreateInfo *pInfo);
    int controlMode;
    unsigned char unmodeled_08[0x0c];
    float f14;
    unsigned char unmodeled_18[0x398];
    float f3b0;
    float f3b4;
    float f3b8;
    float f3bc;
} RgWeaponEnergyEssenceInit;

void InitRgWeaponEnergyEssence(RgWeaponEnergyEssence *pEss)
{
    RgWeaponEnergyEssenceInit *init = (RgWeaponEnergyEssenceInit *) pEss;

    if (pEss == 0) {
        assert_prog(D_00A53710, D_00A53658, 0x526);
    }
    _InitEssenceCommon(pEss);
    init->m_pCreateMethod = _CreateWeaponEnergyType;
    init->f14 = 200.0f;
    init->controlMode = 0x20;
    init->f3b0 = 1.0f;
    init->f3b4 = 100.0f;
    init->f3b8 = 100.0f;
    init->f3bc = 50.0f;
}

/* ov12:0x00a53768 "pEss != NIL && pInfo != NIL" */
extern unsigned char D_00A53768[];
/* ov12:0x00a53788 "pEss->m_pCreateMethod != NIL" */
extern unsigned char D_00A53788[];

RgWeapon *CreateRgWeaponFromEssence(RgWeaponEssenceCommon *pEss, RgWeaponCreateInfo *pInfo)
{
    if (pEss == 0 || pInfo == 0) {
        assert_prog((char *) D_00A53768, D_00A53658, 1340);
    }
    if (pEss->m_pCreateMethod == 0) {
        assert_prog((char *) D_00A53788, D_00A53658, 1341);
    }
    return pEss->m_pCreateMethod(pEss, pInfo);
}

void DisposeRgWeapon(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 1348);
    }
    if (weapon->dispose != 0) {
        weapon->dispose(weapon);
    }
    _DestructWeaponCommon(weapon);
    RgHeapFree(InstanceOfRgHeap(), weapon, D_00A53658, 1352);
}

XrgActor *DisposeRgWeaponNotActor(RgWeapon *weapon)
{
    XrgActor *actor;

    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x551);
    }
    if (weapon->dispose != 0) {
        weapon->dispose(weapon);
    }
    actor = weapon->actor;
    weapon->actor = 0;
    _DestructWeaponCommon(weapon);
    RgHeapFree(InstanceOfRgHeap(), weapon, D_00A53658, 0x557);
    return actor;
}

void RgWeaponSetSound(RgWeapon *weapon, int sound)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x585);
    }
    weapon->sound = sound;
}

int RgWeaponIsLockedOn(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x58F);
    }
    return weapon->lockedOn;
}

int RgWeaponIsTargetting(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x597);
    }
    return weapon->targeting;
}

float RgWeaponGetShotNum(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x59F);
    }
    return weapon->shotNum;
}

int RgWeaponIsNotBusy(void *weapon)
{
    int (*busy)(void *);
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 1448);
    }
    busy = *(int (**)(void *))((char *)weapon + 0x30);
    if (busy != 0) {
        return busy(weapon) == 0;
    }
    return 1;
}

extern const char D_00A537A8[];
extern const char D_00A537B8[];
extern void RgWarn(const char *fmt, const char *file, int line, ...);
extern int RgWeaponGetEss(RgWeapon *weapon);
extern RgWeaponShotEssence *RgWeaponEssCastToShot(RgWeaponEssenceCommon *ess);

int RgWeaponGetRestUnit(RgWeapon *weapon)
{
    int ess;

    if (weapon == 0) {
        assert_prog(D_00A537A8, D_00A53658, 0x5B5);
    }
    ess = RgWeaponGetEss(weapon);
    if (ess == 0) {
        RgWarn(D_00A537B8, D_00A53658, 0x5BA, weapon, weapon->shotMotionOffset);
        return 2;
    }
    if (*(float *)(ess + 0x14) == 1e8f) {
        return 2;
    }
    if (RgWeaponEssCastToShot((RgWeaponEssenceCommon *) ess) != 0) {
        return 0;
    }
    return RgWeaponEssCastToAttack(ess) != 0 ? 2 : 1;
}

int RgWeaponRobotShotMotion(RgWeapon *weapon, int eCharID)
{
    if (!(0 <= eCharID && eCharID < RG_ACTOR_CHAR_ROBNUM)) {
        assert_prog(D_00A537D8, D_00A53658, 0x5D2);
    }
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x5D4);
    }
    return weapon->shotMotionTable[eCharID][weapon->shotMotionOffset];
}

XrgActor *RgWeaponGetActor(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x5DC);
    }
    return weapon->actor;
}

RgEquip *RgWeaponGetEquip(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x5E4);
    }
    return &weapon->equip;
}

float RgWeaponGetWeight(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x5EC);
    }
    return weapon->weight;
}

int RgWeaponIsAttachedShot(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 1525);
    }
    return _GetNumAttach(&weapon->shotAttach) != 0;
}

int RgWeaponGetControlFlag(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x5FE);
    }
    return weapon->controlFlags;
}

int RgWeaponGetEss(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x606);
    }
    return weapon->ess;
}

int RgWeaponGetAttachedShot(RgWeapon *weapon, RgShot **shots)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 1572);
    }
    if (shots == 0) {
        return _GetNumAttach(&weapon->shotAttach);
    }
    return _GetAllAttach(&weapon->shotAttach, shots);
}

int RgWeaponIsActiveLockOnSys(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x62F);
    }
    return weapon->lockOnEffector != 0;
}

void RgWeaponPlayMotion(RgWeapon *weapon, int motion)
{
    XrgActor *actor;

    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x63E);
    }
    actor = weapon->actor;
    if (actor != 0) {
        XrgActorSetMotion(actor, motion);
    }
}

void RgWeaponPlayShotMotion(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x649);
    }
    _PlayShotMotion(weapon);
}

int RgWeaponShot(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x651);
    }
    weapon->shotRequested = 1;
    if (weapon->shotNum > 0.0f) {
        if (weapon->shoot != 0) {
            return weapon->shoot(weapon);
        }
    }
    return 0;
}

void RgWeaponSetup(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x660);
    }
    weapon->equipped = 1;
}

void RgWeaponSetdown(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x669);
    }
    weapon->equipped = 0;
}

void RgWeaponStopLockon(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x672);
    }
    weapon->lockOnStopped = 1;
}

void RgWeaponRestartLockon(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x67B);
    }
    weapon->lockOnStopped = 0;
}

void RgWeaponDisposeLockon(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x684);
    }
    if (weapon->lockOnEffector != 0) {
        XrgActorDisposeEffector(RgRobotGetActor(weapon->robotId), weapon->lockOnEffector);
        weapon->lockOnEffector = 0;
    }
}

void RgWeaponShotStop(RgWeapon *weapon)
{
    /* separate load keeps the type's shot-stop callback in its own register */
    void (*shotStop)(RgWeapon *weapon);

    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x691);
    }
    shotStop = weapon->shotStop;
    weapon->shotRequested = 0;
    if (shotStop != 0) {
        shotStop(weapon);
    }
    weapon->equipped = 0;
}

void RgWeaponHitRobot(RgWeapon *weapon, int robotId, int damage)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x69E);
    }
    if (weapon->hitRobot != 0) {
        weapon->hitRobot(weapon, robotId, damage);
    }
}

void RgWeaponHitBG(RgWeapon *weapon, int bgObject, int damage)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x6A9);
    }
    if (weapon->hitBG != 0) {
        weapon->hitBG(weapon, bgObject, damage);
    }
    XrgSoundRingVol(weapon->sound, weapon->bgHitSound, weapon->bgHitSound);
}

void RgWeaponSubShotNum(RgWeapon *weapon, float delta)
{
    float shotNum;
    float newShotNum;

    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x6B5);
    }
    shotNum = weapon->shotNum;
    if (shotNum != 1e8f) {
        newShotNum = shotNum - delta;
        weapon->shotNum = newShotNum;
        if (newShotNum < 0.0f) {
            weapon->shotNum = 0.0f;
        }
    }
}

void RgWeaponSetFree(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x6C3);
    }
    if (weapon->setFree != 0) {
        weapon->setFree(weapon);
    }
}

extern void XrgActorDraw(XrgActor *actor);

void RgWeaponDisp(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x6CD);
    }
    (*(void (**)(RgWeapon *))weapon->unmodeled_18)(weapon);
    if (weapon->actor != 0) {
        XrgActorDraw(weapon->actor);
    }
}

void RgWeaponPassTime(RgWeapon *weapon, float dt)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x6D9);
    }
    _CommonPassTime(weapon, dt);
    if (weapon->passTime != 0) {
        weapon->passTime(weapon, dt);
    }
    if (weapon->actor != 0) {
        XrgActorPassTime(weapon->actor, dt);
    }
}

extern void RgGeomRobotCalcLocal(int geom, RgMatrix local);
extern int RgRobotGetStatusFlags(int robotId);
extern float atan2f(float y, float x);

void _CheckLockOn(RgWeapon *weapon)
{
    RgMatrix local;
    RgMatrix invLocal;
    RgVector targetLocal;
    RgVector targetWorld;
    float angle;
    int robotId;
    int statusFlags;
    int wasLockedOn;
    int lockedOn;
    void *target;

    robotId = weapon->robotId;
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 1779);
    }
    wasLockedOn = weapon->lockedOn;
    weapon->targeting = 0;
    weapon->lockedOn = 0;
    statusFlags = RgRobotGetStatusFlags(robotId);
    if (robotId != 0 && (statusFlags & 1) && RgRobotIsConfused(robotId) == 0 &&
            !(statusFlags & 0x100) && weapon->lockOnStopped == 0) {
        RgGeomRobotCalcLocal(RgRobotGetGeom(robotId), local);
        XrgInvMatrix(invLocal, local);
        target = RgRobotGetTarget(robotId);
        if (target != 0) {
            __RgGeomPointGetPos((RgGeomPoint *) target, (RgPointVector *) targetWorld, D_00A53658, 1819);
            __asm__ __volatile__(
                "lqc2 $vf2, 0(%0)\n\t"
                "lqc2 $vf3, 0(%1)\n\t"
                "lqc2 $vf4, 16(%1)\n\t"
                "lqc2 $vf5, 32(%1)\n\t"
                "lqc2 $vf6, 48(%1)\n\t"
                "vmulax.xyzw ACCxyzw, vf3xyzw, vf2x\n\t"
                "vmadday.xyzw ACCxyzw, vf4xyzw, vf2y\n\t"
                "vmaddaz.xyzw ACCxyzw, vf5xyzw, vf2z\n\t"
                "vmaddw.xyzw vf2xyzw, vf6xyzw, vf2w\n\t"
                "sqc2 $vf2, 0(%2)\n\t"
                : : "r"(targetWorld), "r"(invLocal), "r"(targetLocal) : "memory");
            angle = atan2f(targetLocal[0], targetLocal[2]) + 3.1415927f;
            while (angle > 3.1415927f) {
                angle -= 6.2831855f;
            }
            if (angle < -3.1415927f) {
                do {
                    angle += 6.2831855f;
                } while (angle < -3.1415927f);
            }
            lockedOn = 0;
            if (weapon->lockOnAngleMin <= angle && angle <= weapon->lockOnAngleMax) {
                lockedOn = 1;
            }
            weapon->targeting = lockedOn;
            if (weapon->controlFlags & 0x20) {
                weapon->lockedOn = lockedOn;
            }
            XrgCopyVector(weapon->targetPosition, targetWorld);
        }
        if (wasLockedOn == 0 && _GetNumAttach(&weapon->shotAttach) != 0 && weapon->lockedOn != 0) {
            weapon->lockedOn = 0;
        }
    }
}

int _GetParentCharID(RgWeapon *weapon)
{
    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x73A);
    }
    if (weapon->robotId != 0) {
        return RgRobotGetCharID(weapon->robotId);
    }
    return -1;
}

void _PlayShotMotion(RgWeapon *weapon)
{
    /* separate load keeps shotMotionOffset live across the call below */
    int shotMotionOffset;
    int charID;
    int motion;

    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x747);
    }
    shotMotionOffset = weapon->shotMotionOffset;
    charID = _GetParentCharID(weapon);
    if (charID != -1) {
        motion = weapon->shotStartMotionTable[charID][shotMotionOffset];
        if (motion != -1) {
            if (weapon->actor != 0) {
                XrgActorSetMotion(weapon->actor, motion);
            }
        }
    }
}

int _GetDefShotPosDir(RgWeaponShotRequest *info, RgVector position, RgVector direction)
{
    RgWeapon *weapon = (RgWeapon *) info;

    if (weapon == 0) {
        assert_prog(D_00A536F0, D_00A53658, 0x756);
    }
    if (weapon->shotRequested == 0) {
        return 0;
    }
    _GetDefShotPosDirFunc(weapon, position, direction);
    return 1;
}

typedef struct DefShotEss {
    unsigned char unk00[0x18];
    float rotX;
    float rotY;
    int dirType;
    unsigned int jointNum;
    int joints[1];
} DefShotEss;
extern void XrgCopyMatrix(RgMatrix destination, const RgMatrix source);
extern void XrgNegateVector(RgVector destination, RgVector source);
extern float XrgLengthVector(RgVector vector);
extern float *XrgVectorY(void);
extern void XrgCalcMatrixZtoY(RgMatrix destination, RgVector first, RgVector second);
extern void XrgRotMatrixY(RgMatrix destination, RgMatrix source, float angle);
extern void XrgRotMatrixX(RgMatrix destination, RgMatrix source, float angle);

int _GetDefShotPosDirFunc(RgWeapon *weapon, RgVector position, RgVector direction)
{
  RgMatrix joint;
  int new_var;
  RgMatrix local;
  RgVector dir;
  float len;
  int found;
  int robotId;
  DefShotEss *ess;
  XrgActor *actor;
  int jointNo;
  unsigned int n;
  found = 0;
  robotId = weapon->robotId;
  ess = (DefShotEss *) weapon->ess;
  if (robotId == 0)
  {
    assert_prog(D_00A53808, D_00A53658, 0x764);
  }
  RgGeomRobotCalcLocal(RgRobotGetGeom(robotId), local);
  actor = weapon->actor;
  if (actor == 0)
  {
    XrgCopyMatrix(joint, local);
  }
  else
  {
    jointNo = 8;
    if (ess->jointNum != 0)
    {
      n = *((unsigned int *) weapon->unmodeled_64);
      *((unsigned int *) weapon->unmodeled_64) = n + 1;
      new_var = ess->joints[n];
 do { jointNo = new_var; if ((n + 1) >= ess->jointNum) { *((unsigned int *) weapon->unmodeled_64) = 0; } } while (0);
    }
    found = XrgActorGetJointLocal(actor, jointNo, joint);
    if (found == 0)
    {
      XrgCopyMatrix(joint, local);
    }
  }
  XrgCopyVector(position, &joint[12]);
  if ((ess->dirType != 3) && (found != 0))
  {
    XrgAddVector(direction, position, &joint[ess->dirType * 4]);
  }
  else
    if (weapon->lockedOn != 0)
  {
    XrgCopyVector(direction, weapon->targetPosition);
  }
  else
  {
    XrgNegateVector(direction, &local[8]);
    XrgAddVector(direction, direction, position);
  }
  if (weapon->lockedOn == 0)
  {
    if ((ess->rotX != 0.0f) || (ess->rotY != 0.0f))
    {
      XrgSubVector(dir, direction, position);
      len = XrgLengthVector(dir);
      XrgCalcMatrixZtoY(joint, dir, XrgVectorY());
      XrgRotMatrixY(joint, joint, ess->rotY);
      XrgRotMatrixX(joint, joint, ess->rotX);
      XrgScaleVector(dir, &joint[8], len);
      XrgAddVector(direction, position, dir);
    }
  }
}

void _GetShotInfo(RgWeapon *weapon, RgWeaponShotRequest *info)
{
    /* separate load keeps the owning robot's id in its own register, read
     * once and passed to both RgRobotGetTarget and RgRobotIsConfused */
    int robotId;

    robotId = weapon->robotId;
    if (robotId == 0) {
        assert_prog(D_00A53808, D_00A53658, 0x7A5);
    }
    info->target = RgRobotGetTarget(robotId);
    info->id = weapon->id;
    info->weapon = weapon;
    info->getPosDir = _GetDefShotPosDir;
    info->targetConfused = RgRobotIsConfused(robotId);
}

RgWeaponShotEssence *RgWeaponEssCastToShot(RgWeaponEssenceCommon *ess)
{
    if (ess != 0 && (int) ess->m_pCreateMethod == (int) _CreateWeaponShotType) {
        return (RgWeaponShotEssence *) ess;
    }
    return 0;
}

RgWeaponEnergyEssence *RgWeaponEssCastToEnergy(RgWeaponEssenceCommon *ess)
{
    if (ess != 0 && (int) ess->m_pCreateMethod == (int) _CreateWeaponEnergyType) {
        return (RgWeaponEnergyEssence *) ess;
    }
    return 0;
}

unsigned char *RgWeaponEssCastToAttack(int ess)
{
    unsigned char *result = (unsigned char *)ess;

    if (ess != 0) {
        RgWeaponEssenceCommon *pEss = (RgWeaponEssenceCommon *)ess;
        if (pEss->m_pCreateMethod == (RgWeapon *(*)(void *, RgWeaponCreateInfo *))_CreateWeaponAttackType) {
            return result;
        }
        if (pEss->m_pCreateMethod == (RgWeapon *(*)(void *, RgWeaponCreateInfo *))_CreateWeaponUnArmedType) {
            return result;
        }
    }
    return 0;
}

RgWeaponShieldEssence *RgWeaponEssCastToShield(RgWeaponEssenceCommon *ess)
{
    if (ess != 0 && (int) ess->m_pCreateMethod == (int) _CreateWeaponShieldType) {
        return (RgWeaponShieldEssence *) ess;
    }
    return 0;
}

RgWeaponUnArmedEssence *RgWeaponEssCastToUnArmed(RgWeaponEssenceCommon *ess)
{
    if (ess != 0 && (int) ess->m_pCreateMethod == (int) _CreateWeaponUnArmedType) {
        return (RgWeaponUnArmedEssence *) ess;
    }
    return 0;
}

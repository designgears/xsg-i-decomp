/*
 * OV12 original TU 71: 0x00a3cd10..0x00a3e518 (26 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_select_robot.h"
#include "main/xgl_studio.h"

extern void assert_prog(const char *expression, const char *source_file,
                        int line);
extern RgHeap *InstanceOfRgHeap(void);
extern void *RgHeapAlloc(void *heap, unsigned int size,
                         const char *source_file, int line);
extern void RgHeapFree(void *heap, void *ptr, const char *source_file,
                       int line);

static void _InitSelRob(RgSelectRobot *pCont);
static void _DestructSelRob(RgSelectRobot *pCont);
static void _JobAct(RgSelectRobot *pCont, float deltaTime);

/* Linker witness: this TU's own source-file name, used by every assert here. */
extern const char D_00A56DA0[];
/* Linker witness for the original literal at ov12:0x00a56ef8 ("pCont != NIL"). */
extern const char D_00A56EF8[];
/* Linker witness for the original literal at ov12:0x00a56d90 ("who are you ?"). */
extern const char D_00A56D90[];

/* jal RgWarn(format, file, line, ...), the XrgLogSys-shaped debug warning
   (also declared this way by src/ov12/rg_main.c, its accepted caller). */
extern void RgWarn(const char *format, const char *file, int line, ...);

/*
 * xglCdReadFile's completion callback: only completion code 4 does
 * anything, and only when a requester (s_pWhoAreYou) is on file to receive
 * the count; any other requester-less code-4 completion is logged instead
 * of counted.  Codes below -2 and codes 1..3 are guarded out and ignored.
 */
static void _ReadCallback(int completionCode)
{
    if (completionCode < -2) {
        return;
    }
    if (completionCode < 4) {
        return;
    }
    if (completionCode != 4) {
        return;
    }
    if (s_pWhoAreYou != 0) {
        s_uOkNum++;
    } else {
        RgWarn(D_00A56D90, D_00A56DA0, 55);
    }
    s_bLoading = 0;
}

static void _KillLoad(void)
{
    if (s_bLoading != 0) {
        xglCdReadCancel();
        s_bLoading = 0;
    }
    s_uReqNum = (s_uOkNum = 0);
    s_pWhoAreYou = 0;
}

static void _KillLoadIfMe(void *owner)
{
    if (s_uReqNum != 0) {
        if (s_pWhoAreYou == owner) {
            _KillLoad();
        }
    }
}

extern const char D_00A56DC0[];
extern void *s_apBuf[4];
extern char s_aszData[4][64];
extern char *strcpy(char *, const char *);

static int _ReqLoad(char **names, void **bufs, unsigned int uFileNum, void *who)
{
    unsigned int i;

    if (uFileNum >= 5) {
        assert_prog(D_00A56DC0, D_00A56DA0, 0x5E);
    }
    if (s_pWhoAreYou == who || s_pWhoAreYou == 0) {
        _KillLoad();
        for (i = 0; i < uFileNum; i++) {
            strcpy(s_aszData[i], names[i]);
            s_apBuf[i] = bufs[i];
        }
        s_uReqNum = uFileNum;
        s_pWhoAreYou = who;
        s_uOkNum = 0;
        return 1;
    }
    return 0;
}

static int _IsEndOfLoad(void)
{
    if (s_bLoading == 0) {
        if (s_uReqNum != 0) {
            if (s_uOkNum >= s_uReqNum) return 1;
        }
    }
    return 0;
}

extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
extern void *s_apBuf[4];
extern char s_aszData[4][64];

static void _JobLoad(void)
{
    if (s_uReqNum != 0 && s_uOkNum < s_uReqNum) {
        if (s_bLoading == 0 &&
            xglCdReadFile(s_aszData[s_uOkNum], s_apBuf[s_uOkNum], 1,
                          (int) _ReadCallback) >= 0) {
            s_bLoading = 1;
        }
        return;
    }
    s_pWhoAreYou = 0;
}

/*
 * Reset the paired request-completion counters through the helper interface
 * used by the load-state owner.  Its parameter order is completion then
 * request, while the reset order is request then completion.
 */
static __inline__ void ResetLoadCounts(unsigned int *completed_count,
                                      unsigned int *request_count)
{
    *request_count = 0;
    *completed_count = 0;
}

/* Original OV12 local function at 0x00a3d008. */
static void _InitLoad(void)
{
    s_bLoading = 0;
    ResetLoadCounts(&s_uOkNum, &s_uReqNum);
    s_pWhoAreYou = 0;
    xglCdReadCancel();
}

extern const char D_00A56DD8[];
extern const char D_00A56DE8[];
extern const char D_00A56DF8[];
extern const char D_00A56E08[];
extern RgFileSysData *RgFileSysRead(RgFileSys *pSys, const char *pszName, const char *pszRoot);
RgFileSys *InstanceOfRgFileSys(void);
extern RgDispModel *CreateXrgDispModelImpl(const char *name, const char *variant);

static void *_InitAct(RgSelectRobot *pCont, void *heap, int regionBytes, int count)
{
    char *p = (char *)pCont;
    char *q;
    unsigned int i;

    if (pCont == 0) {
        assert_prog(D_00A56DD8, D_00A56DA0, 0xB7);
    }
    pCont->screenPos = 0.0f;
    q = p + 0xC;
    *(int *)(p + 0x120) = -1;
    *(int *)p = 0;
    pCont->actorFlags = 0;
    *(int *)(p + 0x10C) = 0;
    *(int *)(p + 0x124) = 0;
    *(int *)(p + 0x150) = 0;
    *(float *)(p + 0x154) = pCont->screenPos;
    pCont->displayPosition = pCont->screenPos;
    for (i = 0; i < 4; i++) {
        q[i * 0x40] = 0;
        ((void **)(p + 0x110))[i] = heap;
        heap = (char *)heap + regionBytes;
    }
    if (count != 0) {
        pCont->hasActions = 1;
        for (i = 0; i < 3; i++) {
            RgSelectRobot *pSub;
            char *s;

            pSub = RgHeapAlloc(InstanceOfRgHeap(), 0x164, D_00A56DA0, 0xCC);
            ((RgSelectRobot **)(q + 0x120))[i] = pSub;
            heap = _InitAct(pSub, heap, regionBytes, 0);
            s = (char *)pSub;
            *(RgSelectRobot **)(s + 0x124) = pCont;
            *(unsigned int *)(s + 0x160) = i;
            ((int *)(p + 0x144))[i] = 0;
            *(int *)(s + 0x15C) = 0;
        }
        pCont->fileData = RgFileSysRead(InstanceOfRgFileSys(), D_00A56DE8, D_00A56DF8);
        if (pCont->fileData != 0) {
            pCont->displayModel = CreateXrgDispModelImpl(pCont->fileData->data, D_00A56E08);
        }
    } else {
        pCont->hasActions = 0;
        pCont->fileData = 0;
        pCont->displayModel = 0;
    }
    return heap;
}

extern void xglLightIntensityAmbient(StudioLight *light,
                                     const Vector4 *intensity);
extern void xglLightIntensityParallel(StudioLight *light, unsigned int index,
                                     const Vector4 *intensity);
extern void xglLightAngle(StudioLight *light, unsigned int index,
                         const Vector4 *direction);
typedef union AlignedLightVector {
    double alignment; /* Preserve 8-byte alignment for the original copy. */
    Vector4 value;
    float components[4];
} AlignedLightVector;
typedef struct AlignedLightBlock {
    AlignedLightVector vectors[4];
} AlignedLightBlock;
extern const AlignedLightBlock D_00A56E10;
extern AlignedLightBlock asDirection_0;

void _select_light(StudioLight *light)
{
    AlignedLightBlock intensity;
    int i;

    intensity.vectors[0] = D_00A56E10.vectors[0];
    intensity.vectors[1] = D_00A56E10.vectors[1];
    intensity.vectors[2] = D_00A56E10.vectors[2];
    intensity.vectors[3] = D_00A56E10.vectors[3];

    for (i = 0; i < 4; i++) {
        intensity.vectors[i].components[0] *= 0.8f;
        intensity.vectors[i].components[1] *= 0.8f;
        intensity.vectors[i].components[2] *= 0.8f;
    }

    for (i = 0; i < 4; i++) {
        if (i == 0) {
            xglLightIntensityAmbient(light, &intensity.vectors[0].value);
        } else {
            xglLightIntensityParallel(light, i - 1,
                                      &intensity.vectors[i].value);
            xglLightAngle(light, i - 1,
                          &asDirection_0.vectors[i].value);
        }
    }
}

extern void XrgLinearIntpVector(RgVector destination, RgVector first,
                                 RgVector second, float weight);
/* The two positions _ActorPos interpolates between, weighted by
   RgSelectRobot::screenPos: index 1 is the weighted argument, index 0 the
   complement-weighted one. */
extern RgVector s_aPos_1[2];

static void _ActorPos(RgSelectRobot *pCont, RgVector position)
{
    XrgLinearIntpVector(position, s_aPos_1[1], s_aPos_1[0], pCont->screenPos);
}

typedef struct ActorPosView {
    unsigned char pad00[0x10];
    float pos[3];
    unsigned char pad1c[0x34];
    float f50;
    float f54;
    float f58;
} ActorPosView;
extern void ACT_updateMotion(void *actor);

static void _ActorPosUpdate(RgSelectRobot *pCont)
{
    RgVector position;
    ActorPosView *actor;
    float angle;

    if (pCont->actorFlags == 0) {
        return;
    }
    if (*(int *)((char *)pCont + 0x124) != 0) {
        return;
    }
    _ActorPos(pCont, position);
    actor = (ActorPosView *)pCont->actorFlags;
    actor->pos[0] = position[0];
    actor->pos[1] = position[1];
    actor->pos[2] = position[2];
    actor->f50 = 0.0f;
    actor->f54 = *(float *)((char *)pCont + 0x154);
    actor->f58 = 0.0f;
    ACT_updateMotion(actor);

    angle = pCont->displayPosition - *(float *)((char *)pCont + 0x154);
    if (angle > 3.1415927f) {
        angle -= 6.2831855f;
        while (angle > 3.1415927f) {
            angle -= 6.2831855f;
        }
    }
    if (angle < -3.1415927f) {
        do {
            angle += 6.2831855f;
        } while (angle < -3.1415927f);
    }
    *(float *)((char *)pCont + 0x154) = *(float *)((char *)pCont + 0x154) + angle * 0.2f;
    angle = *(float *)((char *)pCont + 0x154);
    if (angle > 3.1415927f) {
        angle -= 6.2831855f;
        while (angle > 3.1415927f) {
            angle -= 6.2831855f;
        }
    }
    if (angle < -3.1415927f) {
        do {
            angle += 6.2831855f;
        } while (angle < -3.1415927f);
    }
    *(float *)((char *)pCont + 0x154) = angle;
}

extern const char D_00A56DD8[];
extern const char D_00A56E50[];
extern const char D_00A56E68[];
extern const char D_00A56E80[];
extern const char D_00A56E98[];
static void _DumpHead();
extern void *ACT_create(int type, int id);
extern void ACT_allocMatrix(void *pXenoAct, int flags);
extern void ACT_setModelWrapper(void *pXenoAct, int flags);
extern void ACT_initMotion(void *pXenoAct);
extern void ACT_initExMotion(void *pXenoAct, int useExtended, int weaponMotion);
extern void ACT_setMotion2(void *pXenoAct, int motion, int param);
extern void ACT_setArms(void *child, void *parent, int accID, int flags);
extern int RgEquipMountPos(void *pEquip, int eType, int kind, const char *name);
extern int RgEquipPosToJntID(int ePos);
extern int XrgActorJntIDtoAccID(int jntID);
extern void *InstanceOfRgBattleCommonData(void);
extern int RgBattleCommonDataGetWeaponMot(void *battleCommonData);

static void _ActivateAct(RgSelectRobot *pAct)
{
    char *p = (char *)pAct;
    char *sub;
    char *chk;
    char *pNew;
    char *equip;
    int type;
    int head0, head1, head2;
    int exMot;
    int mot;
    unsigned int i;

    if (pAct == 0) {
        assert_prog(D_00A56DD8, D_00A56DA0, 0x144);
    }
    if (*(int *)p != 2) {
        assert_prog(D_00A56E50, D_00A56DA0, 0x145);
    }
    if (*(unsigned int *)(p + 0x10C) < 3) {
        assert_prog(D_00A56E68, D_00A56DA0, 0x146);
    }
    if (pAct->actorFlags != 0) {
        assert_prog(D_00A56E80, D_00A56DA0, 0x147);
    }
    chk = *(char **)(p + 0x124);
    if (chk != 0) {
        if (*(int *)chk != 2 || *(int *)(chk + 4) == 0) {
            return;
        }
    }
    head1 = *(int *)(p + 0x110);
    exMot = 0;
    head0 = *(int *)(p + 0x114);
    head2 = *(int *)(p + 0x118);
    if (*(int *)(p + 0x10C) == 4) {
        exMot = *(int *)(p + 0x11C);
    }
    _DumpHead(head0);
    _DumpHead(head1);
    _DumpHead(head2);
    pNew = ACT_create(-1, 1);
    pAct->actorFlags = (unsigned int *)pNew;
    if (pNew == 0) {
        assert_prog(D_00A56E98, D_00A56DA0, 0x15A);
    }
    for (i = 0; i < 11; i++) {
        ((int *)(pNew + 0x8D0))[i] = 0;
    }
    *(int *)(pNew + 0x8D0) = head0;
    *(int *)(pNew + 0x8D4) = head1;
    *(int *)(pNew + 0x8D8) = head2;
    ACT_allocMatrix(pNew, -1);
    ACT_setModelWrapper(pNew, 0);
    _select_light((StudioLight *)(pNew + 0x510));
    *(int *)pNew |= 0x8800;
    *(int *)(pNew + 0x9A0) |= 3;
    *(long *)(pNew + 0x9D8) = 0x44;
    *(int *)(pNew + 0x9C0) = 0;
    ACT_initMotion(pNew);
    if (exMot != 0) {
        ACT_initExMotion(pNew, 0, exMot);
        ACT_setMotion2(pNew, 0, 8);
    }
    pNew[0x90] = 7;
    pNew[0x91] = 0x30;
    sub = *(char **)(p + 0x124);
    if (sub != 0) {
        equip = *(char **)(p + 0x15C);
        type = *(int *)(p + 0x160);
        ACT_setArms(pNew, *(void **)(sub + 4),
                    XrgActorJntIDtoAccID(RgEquipPosToJntID(RgEquipMountPos(
                        equip + 0x70, type, *(int *)(sub + 0x120), equip + 0x330))),
                    0);
        if (*(unsigned int *)(sub + 0x120) < 6) {
            mot = *(int *)(equip + (*(int *)(sub + 0x120) * 3 + type) * 4 + 0x140);
            if (mot != -1) {
                if (mot >= 0x800) {
                    int wm = RgBattleCommonDataGetWeaponMot(InstanceOfRgBattleCommonData());
                    if (wm != 0) {
                        ACT_initExMotion(pNew, 0, wm);
                        ACT_setMotion2(pNew, mot - 0x800, 8);
                    }
                }
            }
        }
        if (*(void **)(p + 0x124) != 0) {
            _ActorPosUpdate(*(RgSelectRobot **)(p + 0x124));
        }
    }
    _ActorPosUpdate(pAct);
}

INCLUDE_ASM("asm/nonmatchings/ov12/rg_select_robot", _ReqAct);

extern const char D_00A56DD8[];
extern void DisposeRgDispModel(RgDispModel *pDispModel);
extern void DisposeRgFileSysData_sub(RgFileSysData *pFile,
                                     const char *sourceFile, int line);

static void _DestructAct(RgSelectRobot *pCont)
{
    unsigned int i;

    if (pCont == 0) {
        assert_prog(D_00A56DD8, D_00A56DA0, 540);
    }

    if (pCont->actorFlags != 0) {
        *pCont->actorFlags = (*pCont->actorFlags | 0x8) & ~0x20;
    }

    if (pCont->hasActions != 0) {
        for (i = 0; i < 3; i++) {
            if (pCont->actions[i] == 0) {
                continue;
            }
            if (pCont->actions[i]->actorFlags != 0) {
                *pCont->actions[i]->actorFlags =
                    (*pCont->actions[i]->actorFlags | 0x8) & ~0x20;
            }
            RgHeapFree(InstanceOfRgHeap(), pCont->actions[i], D_00A56DA0, 553);
        }
    }

    if (pCont->displayModel != 0) {
        DisposeRgDispModel(pCont->displayModel);
    }
    if (pCont->fileData != 0) {
        DisposeRgFileSysData_sub(pCont->fileData, D_00A56DA0, 561);
    }
}

extern const char D_00A56EE0[];
extern void ACT_dispose(void *actor);

static void _JobAct(RgSelectRobot *pCont, float deltaTime)
{
    char *p = (char *)pCont;
    char *names[4];
    void *bufs[4];
    unsigned int i;
    unsigned int j;
    int *chk;
    char *act;
    char *act3;
    float f;

    if (pCont == 0) {
        assert_prog(D_00A56DD8, D_00A56DA0, 0x238);
    }
    switch (*(unsigned int *)p) {
    case 0:
        if (*(unsigned int *)(p + 0x10C) != 0) {
            for (i = 0; i < *(unsigned int *)(p + 0x10C); i++) {
                if (((void **)(p + 0x110))[i] == 0) {
                    assert_prog(D_00A56EE0, D_00A56DA0, 0x240);
                }
                names[i] = p + 0xC + i * 0x40;
                bufs[i] = ((void **)(p + 0x110))[i];
            }
            chk = *(int **)(p + 0x124);
            if (chk == 0 || *chk == 2) {
                if (_ReqLoad(names, bufs, *(unsigned int *)(p + 0x10C), pCont) != 0) {
                    *(int *)p = 1;
                }
            }
        }
        break;
    case 1:
        if (_IsEndOfLoad() != 0) {
            *(int *)p = 2;
        }
        break;
    case 2:
        act = (char *)pCont->actorFlags;
        if (act == 0) {
            if (*(unsigned int *)(p + 0x10C) != 0) {
                _ActivateAct(pCont);
            }
        } else {
            f = *(float *)(act + 0x9C0) + 0.2f;
            if (f > 1.0f) {
                f = 1.0f;
                *(int *)(act + 0x9A0) &= ~1;
            }
            *(float *)(act + 0x9C0) = f;
            _ActorPosUpdate(pCont);
        }
        break;
    case 3:
        act3 = (char *)pCont->actorFlags;
        if (act3 != 0) {
            if ((*(int *)(p + 8))-- <= 0) {
                ACT_dispose(act3);
                pCont->actorFlags = 0;
                *(int *)p = 0;
            }
        }
        break;
    }
    if (pCont->hasActions != 0) {
        for (j = 0; j < 3; j++) {
            _JobAct((RgSelectRobot *)pCont->actions[j], deltaTime);
        }
    }
}

extern void XrgUnitMatrix(RgMatrix destination);
extern void XrgRotMatrixY(RgMatrix destination, RgMatrix source, float angle);

static void _DrawAct(RgSelectRobot *pCont)
{
    char *p = (char *)pCont;
    char *q;
    char *r;
    RgVector position;
    RgMatrix matrix;
    unsigned int idx[3];
    unsigned int i;
    unsigned int n;
    unsigned int count;

    if (pCont == 0) {
        assert_prog(D_00A56DD8, D_00A56DA0, 0x28D);
    }
    if (pCont->displayModel != 0) {
        _ActorPos(pCont, position);
        XrgUnitMatrix(matrix);
        XrgRotMatrixY(matrix, matrix, *(float *)(p + 0x154));
        matrix[12] = position[0];
        matrix[13] = position[1];
        matrix[14] = position[2];
        RgDispModelSetLocal(pCont->displayModel, matrix);
        RgDispModelDisplay(pCont->displayModel);
    }
    if (pCont->hasActions != 0) {
        n = 0;
        r = p + 0x10;
        for (i = 0; i < 3; i++) {
            if (((int *)(p + 0x144))[i] != 0) {
                idx[n++] = i;
            } else {
                RgSelectAction *a = ((RgSelectAction **)(r + 0x11C))[i];
                unsigned int *fl = a->actorFlags;
                if (*(int *)a == 2 && fl != 0) {
                    *fl &= ~8;
                }
            }
        }
        count = ++*(unsigned int *)(p + 0x150);
        if (n != 0) {
            unsigned int sel = count / 10 % (n + 1);
            for (i = 0; i < n; i++) {
                RgSelectAction *a;
                unsigned int *fl;
                q = p + 0xC;
                a = ((RgSelectAction **)(q + 0x120))[idx[i]];
                fl = a->actorFlags;
                if (*(int *)a == 2 && fl != 0) {
                    if (sel == i) {
                        *fl &= ~8;
                    } else {
                        *fl |= 8;
                    }
                }
            }
        }
    }
}

extern RgHeap *InstanceOfRgHeapData(void);
static void *_InitAct(RgSelectRobot *pCont, void *heap, int regionBytes, int count);

void _InitSelRob(RgSelectRobot *pCont)
{
    void *heapBuffer;

    if (pCont == 0) {
        assert_prog(D_00A56EF8, D_00A56DA0, 720);
    }
    _InitLoad();
    heapBuffer = RgHeapAlloc(InstanceOfRgHeapData(), 0xC00000U, D_00A56DA0, 726);
    pCont->heapBuffer = heapBuffer;
    _InitAct(pCont, heapBuffer, 0xC0000, 1);
}

extern void ACT_init(void);
static void _DestructAct(RgSelectRobot *pCont);

void _DestructSelRob(RgSelectRobot *pCont)
{
    if (pCont == 0) {
        assert_prog(D_00A56EF8, D_00A56DA0, 734);
    }
    _KillLoad();
    _DestructAct(pCont);
    RgHeapFree(InstanceOfRgHeapData(), pCont->heapBuffer, D_00A56DA0, 737);
    ACT_init();
}

RgSelectRobot *CreateRgSelectRobot(void)
{
    RgSelectRobot *pCont;

    pCont = RgHeapAlloc(InstanceOfRgHeap(), RG_SELECT_ROBOT_SIZE, D_00A56DA0,
                        745);
    _InitSelRob(pCont);
    return pCont;
}

void DisposeRgSelectRobot(RgSelectRobot *pCont)
{
    if (pCont == 0) {
        assert_prog(D_00A56EF8, D_00A56DA0, 753);
    }
    _DestructSelRob(pCont);
    RgHeapFree(InstanceOfRgHeap(), pCont, D_00A56DA0, 755);
    xglCdReadCancel();
}

void RgSelectRobotScreenPos(RgSelectRobot *pCont, float screenPos)
{
    if (pCont == 0) {
        assert_prog(D_00A56EF8, D_00A56DA0, 766);
    }
    pCont->screenPos = screenPos;
}

void RgSelectRobotSetMode(RgSelectRobot *pCont, int mode)
{
    if (pCont == 0) {
        assert_prog(D_00A56EF8, D_00A56DA0, 774);
    }
    switch (mode) {
    case -1:
        pCont->displayPosition = 0.0f;
        break;
    case 0:
        pCont->displayPosition = -0.98172f;
        break;
    case 1:
        pCont->displayPosition = 1.178125f;
        break;
    case 2:
        pCont->displayPosition = -2.748864f;
        break;
    default:
        break;
    }
}

extern void InitXrgActorEssence(void *essence, int actorID);
static void _ReqAct(RgSelectRobot *pCont, void *essence, int *accessoryIDs,
                     int *extra);

void RgSelectRobotSet(RgSelectRobot *pCont, int *selection)
{
    /* Sized to match InitXrgActorEssence's record (xrg_actor.c,
       ov12/tu080); RgSelectRobotSet never reads or writes its fields
       directly. */
    unsigned char essence[0x88];

    if (pCont == 0) {
        assert_prog(D_00A56EF8, D_00A56DA0, 801);
    }
    InitXrgActorEssence(essence, selection[0]);
    _ReqAct(pCont, essence, selection + 1, selection + 4);
}

void RgSelectRobotPassTime(RgSelectRobot *pCont, float deltaTime)
{
    if (pCont == 0) {
        assert_prog(D_00A56EF8, D_00A56DA0, 820);
    }
    _JobAct(pCont, deltaTime);
}

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern void ACT_draw(void);
static void _DrawAct(RgSelectRobot *pCont);
extern const char D_00A56F08[];
extern const char D_00A56F10[];

void RgSelectRobotDisp(RgSelectRobot *pCont)
{
    unsigned int i;

    if (pCont == 0) {
        assert_prog(D_00A56EF8, D_00A56DA0, 0x33D);
    }
    xglFontDebugPrintf(0, 8, D_00A56F08, *(int *)pCont);
    for (i = 0; i < 3; i++) {
        RgSelectAction *a = pCont->actions[i];
        xglFontDebugPrintf(0, i * 8 + 0x10, D_00A56F10, i, *(int *)a);
    }
    ACT_draw();
    _DrawAct(pCont);
    _JobLoad();
}

static void _DumpHead(void)
{
}

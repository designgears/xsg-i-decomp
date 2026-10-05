/*
 * OV12 original TU 80: 0x00a454f8..0x00a48548 (63 functions)
 */
#include "common.h"
#include "shared.h"
#include "xrg_actor.h"
#include "ov12/rg_draw.h"

extern void assert_prog(const char *expression, const char *source_file,
                        int line);

/*
 * These are external file-backed witnesses, not candidate-emitted data; the
 * scaffold's rodata owns this window (config/tu/ov12/tu080.json), so each
 * keeps its splat name.
 *
 * ov12:0x00a589c8 "pActor != NIL"
 * ov12:0x00a587f8 "pXenoAct != NIL"
 * ov12:0x00a587e0 "../xrg_actor.euc.c"
 * ov12:0x00a58b40 "pDup != NIL"
 * ov12:0x00a58b50 "pNewParent != NIL"
 */
extern const char D_00A589C8[];
extern const char D_00A587F8[];
extern const char D_00A587E0[];
extern const char D_00A58B40[];
extern const char D_00A58B50[];

extern RgHeap *InstanceOfRgHeap(void);
extern void *RgHeapAlloc(void *heap, unsigned int size, const char *source_file,
                         int line);

/* Defined later in this TU (still INCLUDE_ASM); called by the functions below. */
extern void _LoadXenoActor(XrgActor *pActor, int actorID);
extern void _DupXenoActor(XrgActor *pDup, XrgActor *pActor);
extern void XrgActorAttachChild(XrgActor *pParent, XrgActor *pChild, int attachID);
extern void _SetMotUseXenoActor(XrgActor *pActor, int motion);
extern int _IsEndOfMotUseXenoActor(XrgActor *pActor);

/* ACT_* animation module (main), config/symbols/main.txt. */
extern void ACT_initMotion(XenoAct *pXenoAct);
extern void ACT_initExMotion(XenoAct *pXenoAct, int useExtended, int weaponMotion);

extern void *InstanceOfRgBattleCommonData(void);
extern int RgBattleCommonDataGetWeaponMot(void *battleCommonData);

/* The 21-entry table _ActJntToAccID_sub indexes; s_aJntIDToAccID_0
 * (config/symbols/ov12.txt) is 0x54 bytes, i.e. 21 ints. */
extern const int s_aJntIDToAccID_0[21];
extern const char D_00A587B0[]; /* "_ActJntToAccID_sub joint=%d error(%s:%d)" */
extern void RgError(const char *message, const char *source_file, int line, ...);

int _ActJntToAccID_sub(int jntID, const char *sourceFile, int line)
{
    if ((unsigned int) jntID >= 21) {
        RgError(D_00A587B0, D_00A587E0, 62, jntID, sourceFile, line);
    }
    return s_aJntIDToAccID_0[jntID];
}

int XrgActorJntIDtoAccID(int jntID)
{
    return _ActJntToAccID_sub(jntID, D_00A587E0, 71);
}

/* Returns the joint's accessory entry (accessories[jntID]) or the default. */
extern void *ACT_jointGetAccessories(XenoAct *pXenoAct, int jntID);

void *_AccIDToJoint(XenoAct *pXenoAct, int jntID)
{
    if (pXenoAct == 0) {
        assert_prog(D_00A587F8, D_00A587E0, 78);
    }
    return ACT_jointGetAccessories(pXenoAct, jntID);
}

extern int _ActJntToAccID_sub(int jntID, const char *sourceFile, int line);

void *_ActJntToJoint(XenoAct *pXenoAct, int jntID)
{
    if (pXenoAct == 0) {
        assert_prog(D_00A587F8, D_00A587E0, 86);
    }
    return _AccIDToJoint(pXenoAct, _ActJntToAccID_sub(jntID, D_00A587E0, 87));
}

typedef struct XenoDatEntry {
    int id;
    int unk[3];
} XenoDatEntry;
extern XenoDatEntry s_aDatTbl_1[];
extern const char D_00A58958[];

XenoDatEntry *_GetXenoDatEntry(int id) {
    unsigned int i;
    for (i = 0; i < 25; i++) {
        if (s_aDatTbl_1[i].id == id) {
            return &s_aDatTbl_1[i];
        }
    }
    RgWarn(D_00A58958, D_00A587E0, 168, id);
    return 0;
}

/*
 * The skeleton-manipulator record _InitSkeMani fills: a small get/set
 * dispatch pair plus the joint data and element index it is bound to.
 */
typedef struct SkeMani {
    void (*get)(struct SkeMani *mani, RgMatrix matrix); /* 0x00: joint matrix out */
    void (*set)(struct SkeMani *mani, RgMatrix matrix); /* 0x04: joint matrix in */
    XenoAct *pXenoAct;  /* 0x08: NULL when this manipulator has no joint data */
    int elementIndex;    /* 0x0C: only set together with pXenoAct */
} SkeMani;

extern void XrgUnitMatrix(RgMatrix destination);
extern void XrgMulMatrix(RgMatrix destination, RgMatrix left, RgMatrix right);

void _SkeManiGet(SkeMani *mani, RgMatrix matrix)
{
    XenoAct *pXenoAct;
    RgMatrix *mats;
    RgMatrix root;

    pXenoAct = mani->pXenoAct;
    if (pXenoAct != 0) {
        mats = *(RgMatrix **)((char *)pXenoAct + 0x824);
        if (pXenoAct->renderLevelFlag != 0) {
            XrgCopyMatrix(matrix, mats[mani->elementIndex]);
            matrix[3] = 0.0f;
            matrix[7] = 0.0f;
            matrix[11] = 0.0f;
        } else {
            XrgCopyMatrix(matrix, mats[mani->elementIndex]);
            XrgCopyMatrix(root, mats[0]);
            matrix[3] = 0.0f;
            matrix[7] = 0.0f;
            matrix[11] = 0.0f;
            root[3] = 0.0f;
            root[7] = 0.0f;
            root[11] = 0.0f;
            XrgMulMatrix(matrix, root, matrix);
        }
    } else {
        XrgUnitMatrix(matrix);
    }
}

void _SkeManiSet(SkeMani *mani, RgMatrix matrix)
{
    XenoAct *pXenoAct;
    RgMatrix *mats;
    RgMatrix root;
    int idx;
    int i, j;

    pXenoAct = mani->pXenoAct;
    if (pXenoAct != 0) {
        idx = mani->elementIndex;
        mats = *(RgMatrix **)((char *)pXenoAct + 0x824);
        if (pXenoAct->renderLevelFlag == 0) {
            XrgCopyMatrix(root, mats[0]);
            root[3] = 0.0f;
            root[7] = 0.0f;
            root[11] = 0.0f;
            XrgInvMatrix(root, root);
            XrgMulMatrix(matrix, root, matrix);
        }
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 4; j++) {
                mats[idx][j * 4 + i] = matrix[j * 4 + i];
            }
        }
    }
}

/*
 * The joint/skeleton data blob XenoAct.jointData points to: only the
 * element count _InitSkeMani reads back is named here.
 */
typedef struct SkeJointData {
    unsigned char unmodeled_00[6];
    unsigned short numElement; /* 0x06 */
} SkeJointData;

extern void RgWarn(const char *format, const char *source_file, int line, ...);
extern void _SkeManiGet(SkeMani *mani, RgMatrix matrix);
extern void _SkeManiSet(SkeMani *mani, RgMatrix matrix);
extern const char D_00A58978[]; /* "joint id error (jnt:%d elm:%d)\nat %s %d" */
extern const char D_00A589A0[]; /* "pJnt->numElement > 1" */

int _InitSkeMani(SkeMani *pMani, XenoAct *pXenoAct, int elmID,
                 const char *sourceFile, int line)
{
    int result;
    SkeJointData *pJointData;
    unsigned short numElement;

    pMani->get = _SkeManiGet;
    pMani->set = _SkeManiSet;
    if (pXenoAct != 0) {
        pJointData = pXenoAct->jointData;
        result = 0;
        if (elmID > 0) {
            numElement = pJointData->numElement;
            if (elmID >= (int) numElement) {
                RgWarn(D_00A58978, D_00A587E0, 256, elmID, numElement, sourceFile, line);
                if (pJointData->numElement < 2) {
                    assert_prog(D_00A589A0, D_00A587E0, 257);
                }
                elmID = 1;
            }
            pMani->pXenoAct = pXenoAct;
            pMani->elementIndex = elmID;
            goto done;
        }
    } else {
        pMani->pXenoAct = 0;
      done:
        result = 1;
    }
    return result;
}

extern const char D_00A589B8[];

SkeMani *_CreateSkeMani_sub(XenoAct *pXenoAct, int elmID,
                           const char *sourceFile, int line)
{
    SkeMani *pMani;

    pMani = RgHeapAlloc(InstanceOfRgHeap(), 0x10, sourceFile, line);
    if (pMani == 0) {
        assert_prog(D_00A589B8, D_00A587E0, 0x110);
    }
    if (_InitSkeMani(pMani, pXenoAct, elmID, sourceFile, line) == 0) {
        RgHeapFree(InstanceOfRgHeap(), pMani, D_00A587E0, 0x112);
        return 0;
    }
    return pMani;
}

SkeMani *_CreateAccMani_sub(XenoAct *pXenoAct, int accID,
                            const char *sourceFile, int line)
{
    SkeMani *pMani;

    pMani = RgHeapAlloc(InstanceOfRgHeap(), 0x10, D_00A587E0, 0x11D);
    if (pMani == 0) {
        assert_prog(D_00A589B8, D_00A587E0, 0x11E);
    }
    if (pXenoAct != 0) {
        if (_InitSkeMani(pMani, pXenoAct, (int)_AccIDToJoint(pXenoAct, accID), sourceFile, line) == 0) {
            RgHeapFree(InstanceOfRgHeap(), pMani, D_00A587E0, 0x123);
            return 0;
        }
    } else {
        if (_InitSkeMani(pMani, 0, 0, sourceFile, line) == 0) {
            RgHeapFree(InstanceOfRgHeap(), pMani, D_00A587E0, 0x128);
            return 0;
        }
    }
    return pMani;
}

extern void ACT_dispose(void *pXenoAct);
extern void DisposeRgMatricesEffector(void *pEffector);
extern void DisposeRgVector(void *pVector, const char *source_file, int line);
extern void DisposeRgFileSysData_sub(RgFileSysData *pFile, const char *source_file,
                                     int line);
typedef struct XrgEffSet {
    unsigned char pad_00[0x70];
    void *eff[2][8];
    unsigned char pad_b0[0xF0 - 0xB0];
    unsigned int num[2];
} XrgEffSet;
static inline unsigned int _EffNum(XrgEffSet *e, unsigned int i)
{
    return e->num[i];
}
static inline void *_EffGet(XrgEffSet *e, unsigned int i, unsigned int j)
{
    return e->eff[i][j];
}

void _DisposeUseXenoActor(XrgActor *pActor)
{
    unsigned int i;
    unsigned int j;

    if (pActor->resourceFiles[0] != 0) {
        DisposeRgFileSysData_sub(pActor->resourceFiles[0], D_00A587E0, 0x141);
    }
    if (pActor->resourceFiles[1] != 0) {
        DisposeRgFileSysData_sub(pActor->resourceFiles[1], D_00A587E0, 0x143);
    }
    if (pActor->resourceFiles[2] != 0) {
        DisposeRgFileSysData_sub(pActor->resourceFiles[2], D_00A587E0, 0x145);
    }
    if (pActor->resourceFiles[3] != 0) {
        DisposeRgFileSysData_sub(pActor->resourceFiles[3], D_00A587E0, 0x147);
    }
    if (pActor->pXenoAct != 0) {
        ACT_dispose(pActor->pXenoAct);
    }
    for (i = 0; i < 2; i++) {
        for (j = 0; j < _EffNum((XrgEffSet *)((char *)pActor + 4), i); j++) {
            DisposeRgMatricesEffector(_EffGet((XrgEffSet *)((char *)pActor + 4), i, j));
        }
        pActor->effectors[i] = 0;
    }
    DisposeRgVector(pActor->children, D_00A587E0, 0x154);
    pActor->children = 0;
}

/*
 * _copy_matrix copies src into dst and drops its translation column
 * (elements 3, 7 and 11 of the 4x4 RgMatrix), giving dst a rotation-only
 * copy of src.
 */
static void _copy_matrix(RgMatrix dst, const RgMatrix src)
{
    XrgCopyMatrix(dst, src);
    dst[3] = 0.0f;
    dst[7] = 0.0f;
    dst[11] = 0.0f;
}

/*
 * _store_matrix copies src into dst but keeps dst's own translation column
 * and bottom-right element (elements 3, 7, 11 and 15 of the 4x4 RgMatrix),
 * so dst takes src's rotation while keeping its position.
 */
static void _store_matrix(RgMatrix dst, const RgMatrix src)
{
    float m03;
    float m13;
    float m23;
    float m33;

    m03 = dst[3];
    m13 = dst[7];
    m23 = dst[11];
    m33 = dst[15];
    XrgCopyMatrix(dst, src);
    dst[3] = m03;
    dst[7] = m13;
    dst[11] = m23;
    dst[15] = m33;
}

INCLUDE_ASM("asm/nonmatchings/ov12/xrg_actor", _PassTime_00A45D30);

#include "ov12/rg_debug_flags.h"

extern RgDebugFlags *InstanceOfRgDebugFlags(void);
extern void nmlModelSetFogCol(float *);
extern void nmlModelSetFogDist(float, float, float, float);
extern void nmlModelSetRenderLevel(int);
extern RgFog *s_pUseFog;

void _ActorDrawFunction(XenoAct *pXenoAct)
{
    RgFog *pFog;
    float color[3];

    if (InstanceOfRgDebugFlags()->flags[2] != 0) {
        pFog = s_pUseFog;
        if (pFog != 0) {
            color[0] = pFog->color[0];
            color[1] = pFog->color[1];
            color[2] = pFog->color[2];
            nmlModelSetFogCol(color);
            nmlModelSetFogDist(pFog->dist[0], pFog->dist[1], pFog->dist[2], pFog->dist[3]);
            if (pXenoAct->renderLevelFlag != 0) {
                nmlModelSetRenderLevel(0x10);
            }
        }
    }
}

extern RgFog *s_pUseFog; /* ov12.txt size 0x4: the fog block _ActorDrawFunction reads back */
extern int ACT_modelDraw(XenoAct *pXenoAct);

void _DrawActor(XrgActor *pActor, void *pStudio)
{
    RgDrawStudio *pDrawStudio;

    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 527);
    }
    pDrawStudio = pStudio;
    s_pUseFog = pDrawStudio->m_pFog;
    ACT_modelDraw(pActor->pXenoAct);
}

extern void ACT_setMotion2(XenoAct *, int, int, int);

void _SetMotionToXenoActorWithRgMotID(XrgActor *pActor, int rgMotID, int playbackParam)
{
    if (rgMotID >= 0x800 && pActor->weaponMotionActive != 0) {
        ACT_setMotion2(pActor->pXenoAct, (rgMotID - 0x800) | 0x100, playbackParam, playbackParam);
        return;
    }
    ACT_setMotion2(pActor->pXenoAct, rgMotID, playbackParam, playbackParam);
}

void _SetMotUseXenoActor(XrgActor *pActor, int motion)
{
    unsigned int playbackParam;

    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 0x233);
    }
    if (pActor->pXenoAct != 0) {
        playbackParam = pActor->smoothPlay != 0;
        pActor->smoothPlay = 0;
        if (pActor->loopPlay != 0) {
            playbackParam |= 8;
        }
        pActor->loopPlay = 0;
        if (pActor->parent == 0) {
            ACT_initMotion(pActor->pXenoAct);
        }
        _SetMotionToXenoActorWithRgMotID(pActor, motion, playbackParam);
        pActor->motionID = motion;
        if ((pActor->actorID == 2 && motion == 0x3F) || pActor->actorID == 3) {
            pActor->motionComaStep = 1;
        } else {
            pActor->motionComaStep = 0;
        }
        pActor->motionComaStepAccum = 0.0f;
    }
}

extern void ACT_allocMatrix(XenoAct *pXenoAct, int flags);
extern void ACT_setArms(XenoAct *pChildXenoAct, XenoAct *pParentXenoAct,
                        int accID, int flags);
extern void ACT_setModelWrapper(XenoAct *pXenoAct, int flags);
extern const char D_00A58A30[]; /* "pActor != NIL && pChild != NIL" */

/*
 * The joint-ID bounds RG_ACTOR_JNT_LEFT_HAND/RG_ACTOR_JNT_MAX, also used by
 * XrgActorAttachChild's own assert message (0x00a58a50).
 */
#define RG_ACTOR_JNT_LEFT_HAND 0
#define RG_ACTOR_JNT_MAX 21

extern const char D_00A58A50[]; /* "RG_ACTOR_JNT_LEFT_HAND <= (eJntID) && (eJntID) < RG_ACTOR_JNT_MAX" */

void _AttachChildUseXenoActor(XrgActor *pParent, XrgActor *pChild, int eJntID)
{
    if (pParent == 0 || pChild == 0) {
        assert_prog(D_00A58A30, D_00A587E0, 606);
    }
    if (!(RG_ACTOR_JNT_LEFT_HAND <= eJntID && eJntID < RG_ACTOR_JNT_MAX)) {
        assert_prog(D_00A58A50, D_00A587E0, 607);
    }
    if (pParent->pXenoAct == 0) {
        return;
    }
    if (pChild->pXenoAct == 0) {
        return;
    }
    ACT_initMotion(pChild->pXenoAct);
    ACT_allocMatrix(pChild->pXenoAct, -1);
    ACT_setModelWrapper(pChild->pXenoAct, 0);
    ACT_setArms(pChild->pXenoAct, pParent->pXenoAct,
               _ActJntToAccID_sub(eJntID, D_00A587E0, 615), 0);
}

extern void ACT_resetParent(XenoAct *pChildXenoAct, XenoAct *pParentXenoAct);
extern void ACT_setHumanHand(XenoAct *pXenoAct, int hand);

void _DetachChildUseXenoActor(XrgActor *pParent, XrgActor *pChild)
{
    if (pParent == 0 || pChild == 0) {
        assert_prog(D_00A58A30, D_00A587E0, 623);
    }
    if (pParent->pXenoAct == 0) {
        return;
    }
    if (pChild->pXenoAct == 0) {
        return;
    }
    ACT_resetParent(pChild->pXenoAct, pParent->pXenoAct);
    ACT_setHumanHand(pParent->pXenoAct, 0);
}

int _IsEndOfMotUseXenoActor(XrgActor *pActor)
{
    XenoAct *pXenoAct = pActor->pXenoAct;

    if (pActor->motionID == -1) {
        return 1;
    }
    if (pXenoAct == 0 || !(*(unsigned int *) ((char *) pXenoAct + 0x6F0) & 0x1000)) {
        return 0;
    }
    return 1;
}

void _DumpHead(void) {

}

extern RgFileSys *InstanceOfRgFileSys(void);
extern RgFileSysData *RgFileSysDup(RgFileSys *pSys, const char *pszName,
                                   const char *pszRoot);
extern RgFileSysData *RgFileSysRead(RgFileSys *pSys, const char *pszName,
                                    const char *pszRoot);
extern char *strcpy(char *, const char *);
extern char *strcat(char *, const char *);
extern const char D_00A58A98[];
extern const char D_00A58AB0[];
extern const char D_00A58AB8[];
extern const char D_00A58AC0[];

void _LoadXenoActorMdl(XrgActor *pActor, const char *pszMdlName)
{
    char mdlName[128];
    char dupName[128];
    char jntName[128];
    RgFileSysData *pMdlFile;
    RgFileSysData *pJntFile;
    RgFileSysData *pDupFile;
    XenoAct *pXenoAct;

    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 0x2A9);
    }
    if (pszMdlName == 0) {
        assert_prog(D_00A58A98, D_00A587E0, 0x2AA);
    }
    pXenoAct = pActor->pXenoAct;
    if (pXenoAct == 0) {
        assert_prog(D_00A587F8, D_00A587E0, 0x2AD);
    }
    strcat(strcpy(mdlName, pszMdlName), D_00A58AB0);
    strcat(strcpy(dupName, pszMdlName), D_00A58AB8);
    strcat(strcpy(jntName, pszMdlName), D_00A58AC0);
    pMdlFile = RgFileSysRead(InstanceOfRgFileSys(), mdlName, 0);
    pActor->resourceFiles[1] = pMdlFile;
    pJntFile = RgFileSysRead(InstanceOfRgFileSys(), jntName, 0);
    pActor->resourceFiles[3] = pJntFile;
    pDupFile = RgFileSysDup(InstanceOfRgFileSys(), dupName, 0);
    pActor->resourceFiles[2] = pDupFile;
    if (pDupFile != 0) {
        pXenoAct->dupFileData = pDupFile->data;
        ((void (*)(RgFileSysData *))_DumpHead)(pDupFile);
    }
    if (pMdlFile != 0) {
        pXenoAct->auxFileData = pMdlFile->data;
        ((void (*)(RgFileSysData *))_DumpHead)(pMdlFile);
    }
    if (pJntFile != 0) {
        pXenoAct->jointData = pJntFile->data;
        ((void (*)(RgFileSysData *))_DumpHead)(pJntFile);
    }
}

extern RgFileSys *InstanceOfRgFileSys(void);
extern char *RgFileSysDataGetName(RgFileSysData *pFile);
extern RgFileSysData *RgFileSysDup(RgFileSys *pSys, const char *pszName,
                                   const char *pszRoot);
extern RgFileSysData *RgFileSysRead(RgFileSys *pSys, const char *pszName,
                                    const char *pszRoot);
extern const char D_00A58AC8[]; /* "pSrcActor != NIL" */

void _DupLoadXenoActor(XrgActor *pActor, XrgActor *pSrcActor)
{
    RgFileSysData *pDupFile;
    RgFileSysData *pAuxFile;
    RgFileSysData *pJointFile;
    XenoAct *pXenoAct;

    pDupFile = 0;
    pAuxFile = 0;
    pJointFile = 0;
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 718);
    }
    if (pSrcActor == 0) {
        assert_prog(D_00A58AC8, D_00A587E0, 719);
    }
    pXenoAct = pActor->pXenoAct;
    if (pXenoAct == 0) {
        assert_prog(D_00A587F8, D_00A587E0, 723);
    }
    if (pSrcActor->resourceFiles[1] != 0) {
        pAuxFile = RgFileSysRead(InstanceOfRgFileSys(),
                                 RgFileSysDataGetName(pSrcActor->resourceFiles[1]), 0);
        pActor->resourceFiles[1] = pAuxFile;
    }
    if (pSrcActor->resourceFiles[3] != 0) {
        pJointFile = RgFileSysRead(InstanceOfRgFileSys(),
                                   RgFileSysDataGetName(pSrcActor->resourceFiles[3]), 0);
        pActor->resourceFiles[3] = pJointFile;
    }
    if (pSrcActor->resourceFiles[2] != 0) {
        pDupFile = RgFileSysDup(InstanceOfRgFileSys(),
                                RgFileSysDataGetName(pSrcActor->resourceFiles[2]), 0);
        pActor->resourceFiles[2] = pDupFile;
    }
    if (pSrcActor->resourceFiles[0] != 0) {
        pActor->resourceFiles[0] = RgFileSysRead(InstanceOfRgFileSys(),
                                   RgFileSysDataGetName(pSrcActor->resourceFiles[0]), 0);
    }
    if (pDupFile != 0) {
        pXenoAct->dupFileData = pDupFile->data;
    }
    if (pAuxFile != 0) {
        pXenoAct->auxFileData = pAuxFile->data;
    }
    if (pJointFile != 0) {
        pXenoAct->jointData = pJointFile->data;
    }
}

extern void *CreateRgVector(unsigned int size, const char *source_file, int line);
extern void XrgUnitMatrix(RgMatrix matrix);

void _InitXenoActor(XrgActor *pActor) {
    unsigned int i;

    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 0x2E9);
    }
    pActor->actorID = 0;
    pActor->parent = 0;
    pActor->children = CreateRgVector(4U, D_00A587E0, 0x2EC);
    i = 0;
    do {
        pActor->effectors[i] = 0;
        i += 1;
    } while (i < 2U);
    pActor->resourceFiles[0] = 0;
    pActor->attachID = -1;
    pActor->resourceFiles[2] = 0;
    pActor->resourceFiles[1] = 0;
    pActor->resourceFiles[3] = 0;
    pActor->weaponMotionActive = 0;
    XrgUnitMatrix(pActor->local);
    pActor->pXenoAct = 0;
    pActor->motionID = -1;
    pActor->smoothPlay = 0;
    pActor->loopPlay = 0;
    pActor->pakaPakaActive = 0;
}

void _SetBattleCommonWepMotion(XrgActor *pActor)
{
    int weaponMotion;
    XenoAct *pXenoAct;

    weaponMotion = RgBattleCommonDataGetWeaponMot(InstanceOfRgBattleCommonData());
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 776);
    }
    pXenoAct = pActor->pXenoAct;
    if (pXenoAct == 0) {
        assert_prog(D_00A587F8, D_00A587E0, 778);
    }
    if (weaponMotion != 0) {
        pActor->weaponMotionActive = 1;
        ACT_initExMotion(pXenoAct, 1, weaponMotion);
    }
}

extern void xglLightSetDefault(StudioLight *light);
extern const char D_00A58AE0[]; /* "pActor->m_pXenoAct != NIL" */

void _InitAfterLoadXenoActor(XrgActor *pActor)
{
    XenoAct *pXenoAct;

    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 790);
    }
    pXenoAct = pActor->pXenoAct;
    if (pXenoAct == 0) {
        assert_prog(D_00A58AE0, D_00A587E0, 791);
        pXenoAct = pActor->pXenoAct;
    }
    pActor->motionComaStep = 0;
    pActor->motionComaStepAccum = 0.0f;
    xglLightSetDefault(&pXenoAct->light);
    pXenoAct->drawFunc = _ActorDrawFunction;
    pXenoAct->flags |= 0x8000;
    ACT_allocMatrix(pXenoAct, -1);
    ACT_setModelWrapper(pXenoAct, 0);
    if (pActor->resourceFiles[0] != 0) {
        ACT_initMotion(pXenoAct);
        ACT_initExMotion(pXenoAct, 0, (int) pActor->resourceFiles[0]->data);
        _SetBattleCommonWepMotion(pActor);
    }
    pXenoAct->lightCost[0] = 7;
    pXenoAct->lightCost[1] = 0x30;
    pXenoAct->renderFlags |= 0x40;
}

void XrgActorSetLightCost(XrgActor *pActor)
{
    XenoAct *pXenoAct;

    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 841);
    }
    pXenoAct = pActor->pXenoAct;
    pXenoAct->lightCost[0] = 9;
    pXenoAct->lightCost[1] = 0x70;
    pXenoAct->renderFlags |= 0x40;
    pXenoAct->lightCostLevel = 0x12;
}

void XrgActorSetDropWeapon(XrgActor *pActor)
{
    XenoAct *pXenoAct;

    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 859);
    }
    pXenoAct = pActor->pXenoAct;
    if (pXenoAct != 0) {
        pXenoAct->flags |= 0x408;
    }
}

void XrgActorSetPlayMotionComaStep(XrgActor *pActor, int step)
{
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 872);
    }
    pActor->motionComaStep = step;
    pActor->motionComaStepAccum = 0.0f;
}

INCLUDE_ASM("asm/nonmatchings/ov12/xrg_actor", _LoadXenoActor);

extern XenoAct *ACT_create(int type, int id);
extern const char D_00A58B10[];

void _DupXenoActor(XrgActor *pDup, XrgActor *pActor)
{
    XenoDatEntry *pEntry;
    int id;

    if (pDup == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 0x3A5);
    }
    if (pActor == 0) {
        assert_prog(D_00A58AC8, D_00A587E0, 0x3A6);
    }
    pEntry = _GetXenoDatEntry(pActor->actorID);
    if (pEntry == 0) {
        assert_prog(D_00A58B10, D_00A587E0, 0x3A9);
    }
    id = pActor->actorID;
    pDup->motionComaStep = 0;
    pDup->actorID = id;
    _InitXenoActor(pDup);
    pDup->pXenoAct = ACT_create(-1, pEntry->unk[0]);
    _DupLoadXenoActor(pDup, pActor);
    _InitAfterLoadXenoActor(pDup);
    pDup->pakaPakaActive = 0;
}

XrgActor *CreateXrgActor(int actorID)
{
    XrgActor *pActor;

    pActor = RgHeapAlloc(InstanceOfRgHeap(), XRG_ACTOR_SIZE, D_00A587E0, 957);
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 958);
    }
    _LoadXenoActor(pActor, actorID);
    return pActor;
}

XrgActor *DuplicateXrgActor(XrgActor *pActor, XrgActor *pNewParent)
{
    XrgActor *pDup;

    pDup = RgHeapAlloc(InstanceOfRgHeap(), XRG_ACTOR_SIZE, D_00A587E0, 968);
    if (pDup == 0) {
        assert_prog(D_00A58B40, D_00A587E0, 969);
    }
    _DupXenoActor(pDup, pActor);
    if (pActor->attachID != -1) {
        if (pNewParent == 0) {
            assert_prog(D_00A58B50, D_00A587E0, 974);
        }
        XrgActorAttachChild(pNewParent, pDup, pActor->attachID);
    }
    return pDup;
}

extern int RgHeapIsInvalidMemory(void *heap, void *ptr);
extern void _DisposeUseXenoActor(XrgActor *pActor);
extern const char D_00A58B68[];

void DisposeXrgActor(XrgActor *pActor)
{
    if (RgHeapIsInvalidMemory(InstanceOfRgHeap(), pActor)) {
        RgError(D_00A58B68, D_00A587E0, 985, pActor);
    }
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 987);
    }
    _DisposeUseXenoActor(pActor);
    RgHeapFree(InstanceOfRgHeap(), pActor, D_00A587E0, 989);
}

void XrgActorSetLocal(XrgActor *pActor, const RgMatrix matrix)
{
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 999);
    }
    XrgCopyMatrix(pActor->local, matrix);
}

void XrgActorSetLight(XrgActor *pActor, RgVector *pLight)
{
    XenoAct *pXenoAct;
    int i;

    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1008);
    }
    pXenoAct = pActor->pXenoAct;
    if (pXenoAct == 0) {
        return;
    }
    if (pLight != 0) {
        XrgCopyVector((float *)&pXenoAct->light.ambient_color, pLight[0]);
        for (i = 0; i < 3; i++) {
            XrgCopyVector((float *)&pXenoAct->light.lights[i].color, pLight[i + 4]);
            XrgCopyVector((float *)&pXenoAct->light.lights[i].direction, pLight[i + 1]);
        }
        return;
    }
    xglLightSetDefault(&pXenoAct->light);
}

void XrgActorSetMotion(XrgActor *pActor, int motion)
{
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1030);
    }
    if (motion != -1) {
        _SetMotUseXenoActor(pActor, motion);
    }
}

void XrgActorSetSmoothPlay(XrgActor *pActor, int smoothPlay)
{
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1042);
    }
    pActor->smoothPlay = smoothPlay;
}

void XrgActorSetLoopPlay(XrgActor *pActor, int loopPlay)
{
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1050);
    }
    pActor->loopPlay = loopPlay;
}

void XrgActorDeriveMotion(XrgActor *pActor, XrgActor *pSrcActor)
{
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1059);
    }
    if (pSrcActor == 0) {
        assert_prog(D_00A58AC8, D_00A587E0, 1060);
    }
    if (pSrcActor->resourceFiles[0] == 0) {
        return;
    }
    ACT_initMotion(pActor->pXenoAct);
    ACT_initExMotion(pActor->pXenoAct, 0, (int) pSrcActor->resourceFiles[0]->data);
    _SetBattleCommonWepMotion(pActor);
}

void XrgActorSetMotionFrame(XrgActor *pActor, float frame)
{
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1072);
    }
    if ((pActor->pXenoAct != 0) && (pActor->motionID != -1)) {
        if (pActor->parent == 0) {
            ACT_initMotion(pActor->pXenoAct);
        }
        pActor->pXenoAct->motionFrame = frame;
    }
}

void XrgActorSetDraw(XrgActor *pActor, int draw)
{
    XenoAct *pXenoAct;

    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1084);
    }
    pXenoAct = pActor->pXenoAct;
    if (pXenoAct != 0) {
        if (draw == 0) {
            pXenoAct->flags |= 8;
            pXenoAct->renderFlags |= 0x100;
        } else {
            pXenoAct->flags &= ~8;
        }
    }
}

void XrgActorSetTransparent(XrgActor *pActor, float transparency)
{
    XenoAct *pXenoAct;

    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1100);
    }
    pXenoAct = pActor->pXenoAct;
    if (pXenoAct != 0) {
        pXenoAct->transparency = transparency;
        pXenoAct->flags |= 0x800;
        pXenoAct->blendMode = 0x44;
        pXenoAct->renderFlags |= 0x3;
    }
}

int XrgActorIsEndOfMotion(XrgActor *pActor)
{
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1118);
    }
    return _IsEndOfMotUseXenoActor(pActor);
}

float XrgActorGetMotionLength(XrgActor *pActor)
{
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1127);
    }
    if (pActor->motionID == -1 || pActor->pXenoAct == 0) {
        return 0.0f;
    }
    return *(float *)((char *)pActor->pXenoAct + 0x700);
}

float XrgActorGetPlayingMotTime(XrgActor *pActor)
{
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1138);
    }
    if (pActor->motionID == -1 || pActor->pXenoAct == 0) {
        return 0.0f;
    }
    return *(float *)((char *)pActor->pXenoAct + 0x6F4);
}

void XrgActorGetLocal(XrgActor *pActor, RgMatrix matrix)
{
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1149);
    }
    XrgCopyMatrix(matrix, pActor->local);
}

int XrgActorGetJointLocal(XrgActor *pActor, int eJntID, RgMatrix matrix)
{
    XenoAct *pXenoAct;
    int ret = 0;
    int joint;

    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 0x489);
    }
    pXenoAct = pActor->pXenoAct;
    if (pXenoAct == 0) {
        assert_prog(D_00A587F8, D_00A587E0, 0x48B);
    }
    if (!(RG_ACTOR_JNT_LEFT_HAND <= eJntID && eJntID < RG_ACTOR_JNT_MAX)) {
        assert_prog(D_00A58A50, D_00A587E0, 0x48C);
    }
    if (eJntID < 20) {
    if (eJntID >= 0) {
        joint = (int)_ActJntToJoint(pXenoAct, eJntID);
        if (joint >= 0) {
            RgMatrix *mats = *(RgMatrix **)((char *)pXenoAct + 0x824);
            if (joint < *(unsigned short *)((char *)pXenoAct->jointData + 6)) {
                ret = 1;
                XrgCopyMatrix(matrix, mats[joint]);
            }
        }
    }
    }
    return ret;
}

int XrgActorGetAttachedJoint(XrgActor *pActor)
{
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1208);
    }
    if (pActor->parent == 0) {
        return -1;
    }
    return pActor->attachID;
}

extern void RgVectorPush(void *vector, void *element);
extern void _AttachChildUseXenoActor(XrgActor *pParent, XrgActor *pChild,
                                     int eJntID);

void XrgActorAttachChild(XrgActor *pParent, XrgActor *pChild, int eJntID)
{
    if (pParent == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1222);
    }
    if (!(RG_ACTOR_JNT_LEFT_HAND <= eJntID && eJntID < RG_ACTOR_JNT_MAX)) {
        assert_prog(D_00A58A50, D_00A587E0, 1223);
    }
    if (pChild != 0) {
        RgVectorPush(pParent->children, pChild);
        _AttachChildUseXenoActor(pParent, pChild, eJntID);
        pChild->parent = pParent;
        pChild->attachID = eJntID;
    }
}

extern int _GetPakaPakaBaseJnt(int actorID, int attachID);
extern const char D_00A58B88[];
extern int XrgLog(const char *format, const char *source_file, int line, ...);
extern const char *RgActorCharIDToName(int charID);

int XrgActorCheckPakaPaka(XrgActor *pActor)
{
    XrgActor *pParent;

    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 0x4D8);
    }
    pParent = pActor->parent;
    if (pParent != 0) {
        pActor->pakaPakaJnt = _GetPakaPakaBaseJnt(pParent->actorID, pActor->attachID);
        if (pActor->pakaPakaJnt >= 0) {
            int id = pParent->actorID;
            pActor->pakaPakaActive = 1;
            XrgLog(D_00A58B88, D_00A587E0, 0x4E2, pActor,
                   RgActorCharIDToName(id), pActor->attachID,
                   pActor->pakaPakaJnt);
        }
    }
}

extern int RgVectorRemove(void *vector, void *element, const char *source_file,
                          int line);
extern void RgWarn(const char *format, const char *source_file, int line, ...);
extern const char D_00A58BC0[];
extern void _DetachChildUseXenoActor(XrgActor *pParent, XrgActor *pChild);

void XrgActorDettachChild(XrgActor *pParent, XrgActor *pChild)
{
    if (pParent == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1259);
    }
    if (pChild != 0) {
        if (RgVectorRemove(pParent->children, pChild, D_00A587E0, 1261) == 0) {
            RgWarn(D_00A58BC0, D_00A587E0, 1262, pParent->children, pChild);
        }
        _DetachChildUseXenoActor(pParent, pChild);
        pChild->parent = 0;
        pChild->attachID = -1;
    }
}

extern RgDraw *InstanceOfRgDraw(void);
extern void RgDrawReq(RgDraw *pDraw, XrgActor *pActor,
                      void (*drawFunc)(XrgActor *pActor, void *pStudio),
                      void (*clearFunc)(XrgActor *pActor, void *pStudio),
                      int prio, int drawID);
void _DrawActor(XrgActor *pActor, void *pStudio);

void XrgActorDraw(XrgActor *pActor)
{
    RgDrawReq(InstanceOfRgDraw(), pActor, &_DrawActor, 0, 0, -1);
}

static void _PassTime_00A45D30(XrgActor *pActor, float dt);

void XrgActorPassTime(XrgActor *pActor, float dt)
{
    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 1285);
    }
    _PassTime_00A45D30(pActor, dt);
}

extern const char D_00A58B00[];

void InitXrgActorEssence(XrgActorEssence *pEss, int actorID)
{
    if (pEss == 0) {
        assert_prog(D_00A58B00, D_00A587E0, 1295);
    }
    pEss->actorID = actorID;
    pEss->fileName[0] = 0;
    pEss->state = 0;
}

extern const char D_00A58B20[];

int XrgActorEssenceGetFileName(XrgActorEssence *pEss, char *fileName)
{
    XenoDatEntry *pEntry;
    int n;

    if (pEss == 0) {
        assert_prog(D_00A58B00, D_00A587E0, 0x51C);
    }
    pEntry = _GetXenoDatEntry(pEss->actorID);
    if (pEntry == 0) {
        return 0;
    }
    n = 0;
    if (pEss->fileName[0] == 0) {
        if (pEntry->unk[2] == 0) {
            assert_prog(D_00A58B20, D_00A587E0, 0x526);
        }
        if (fileName != 0) {
            n = 3;
            strcat(strcpy(fileName, (char *)pEntry->unk[2]), D_00A58AB0);
            strcat(strcpy(fileName + 0x40, (char *)pEntry->unk[2]), D_00A58AB8);
            strcat(strcpy(fileName + 0x80, (char *)pEntry->unk[2]), D_00A58AC0);
        }
    } else if (fileName != 0) {
        n = 3;
        strcat(strcpy(fileName, pEss->fileName), D_00A58AB0);
        strcat(strcpy(fileName + 0x40, pEss->fileName), D_00A58AB8);
        strcat(strcpy(fileName + 0x80, pEss->fileName), D_00A58AC0);
    }
    if (pEntry->unk[1] != 0) {
        strcpy(fileName + n * 0x40, (char *)pEntry->unk[1]);
        n++;
    }
    return n;
}

extern RawJntData s_aRawJntData[12];

RawJntData *_FindRawJntData(int id, int jntID)
{
    unsigned int i;
    RawJntData *entry;

    entry = s_aRawJntData;
    for (i = 0; i < 12; i++, entry++) {
        if (entry->id == id && entry->jntID == jntID) {
            return entry;
        }
    }
    return 0;
}

extern const char D_00A58BE0[];
extern const char D_00A58BE8[];

int _GetPakaPakaBaseJnt(int actorID, int attachID)
{
    switch (actorID) {
    case 0:
        if (attachID == 4) {
            return 0x35;
        }
        if (attachID == 5) {
            return 0x43;
        }
        goto neg;
    case 2:
        switch (attachID) {
        case 4:
            XrgLog(D_00A58BE0, D_00A587E0, 0x5A8);
            return 0x31;
        case 5:
            XrgLog(D_00A58BE8, D_00A587E0, 0x5AB);
            return 0x40;
        default:
            return -1;
        }
    default:
    neg:
        return -1;
    }
}

INCLUDE_ASM("asm/nonmatchings/ov12/xrg_actor", XrgActorCreateEffConstraint);

INCLUDE_ASM("asm/nonmatchings/ov12/xrg_actor", XrgActorCreateEffArmCombine);

INCLUDE_ASM("asm/nonmatchings/ov12/xrg_actor", XrgActorDisposeEffector);

extern const char D_00A58CB0[];
extern const char D_00A58CD0[];
extern const char D_00A58CF0[];
extern double fptodp(float value);
typedef struct XrgMotionState {
    unsigned int flags;   /* 0x00 */
    float time;           /* 0x04 */
    float unk08;          /* 0x08 */
    float start;          /* 0x0C */
    float end;            /* 0x10 */
    unsigned short unk14; /* 0x14 */
    unsigned short next;  /* 0x16 */
} XrgMotionState;

void XrgActorDebugDisp(XrgActor *pActor)
{
    XenoAct *pXenoAct;
    XrgMotionState *m;

    if (pActor == 0) {
        assert_prog(D_00A589C8, D_00A587E0, 0x662);
    }
    pXenoAct = pActor->pXenoAct;
    m = (XrgMotionState *)((char *)pXenoAct + 0x6F0);
    if (pXenoAct != 0) {
        XrgLog(D_00A58CB0, D_00A587E0, 0x666, pActor, pXenoAct);
        {
            const char *fmt = D_00A58CD0;
            double t = fptodp(m->time);
            double st = fptodp(m->start);
            XrgLog(fmt, D_00A587E0, 0x667, t, st, fptodp(m->end));
        }
        XrgLog(D_00A58CF0, D_00A587E0, 0x668, m->flags, 0x1000, m->next);
    }
}

/*
 * OV12 original TU 16: 0x00a10f20..0x00a129c8 (43 functions)
 */
#include "common.h"
#include "rg_camera.h"

static int _SetBit(int *flags, int bit, int value);
static void _PassTimeVersion3(RgCamera *camera, float elapsed);
static int _GetVer3TimerID(char *timerName);
extern int strcmp(const char *left, const char *right);
extern char *s_aszVer3TimerName[10];
extern void *RgHeapAlloc(RgHeap *heap, unsigned int size, const char *source_file, int line);
extern void RgError(const char *message, const char *source_file, int line, ...);
extern void XrgCopyVectorXYZ(RgVector destination, RgVector source);
extern float XrgLengthVector(RgVector vector);
extern float atan2f(float y, float x);
extern RgDrawView *RgDrawStudioGetView(RgDrawStudio *pStudio);
extern void RgDrawViewSetPosition(RgDrawView *view, RgVector position);
extern void RgDrawViewSetRotateX(RgDrawView *view, float angle);
extern void RgDrawViewSetRotateY(RgDrawView *view, float angle);
extern void RgDrawViewSetRotateZ(RgDrawView *view, float angle);
extern void RgDrawViewInit(RgDrawView *view);
extern const char D_00A52920[];
extern const char D_00A52940[];
extern const char D_00A52958[];

extern RgHeap *InstanceOfRgHeap(void);
extern void RgHeapFree(RgHeap *heap, void *ptr, const char *source_file, int line);
extern void RgCalcLocalForChar(RgMatrix dest_matrix, RgVector position,
                               RgVector facing, RgVector up_ref);
/* Opaque here: rg_geom_point.c (ov12/tu052) owns the RgPointVector definition. */
typedef struct RgPointVector RgPointVector;
extern void __RgGeomPointGetPos(RgGeomPoint *point, RgPointVector *destination,
                                const char *source_file, int source_line);
extern void XrgLog(const char *format, const char *source_file, int line, ...);

/* ov12:0x00a52948 "pRgCam != NIL" */
extern const char D_00A52948[];
/* ov12:0x00a52968 "pReader != NIL" */
extern const char D_00A52968[];
/* ov12:0x00a52978 "---------- camera dump %p\n" */
extern const char D_00A52978[];
/* ov12:0x00a52998 "  me=%p enemy=%p\n" */
extern const char D_00A52998[];

static int _SetBit(int *flags, int bit, int value)
{
    if (value != 0) {
        *flags |= 1 << bit;
    } else {
        *flags &= ~(1 << bit);
    }
    return *flags;
}

static int _CheckBit(int flags, int bit)
{
    int isSet;

    isSet = flags & (1 << bit);
    return isSet != 0;
}

static void _NonJobMethod(void)
{
}

extern void assert_prog(const char *expression, const char *source_file, int line);

extern void XrgClearVector(RgVector vector);
extern void XrgCopyVector(RgVector destination, RgVector source);
extern float *XrgVectorY(void);

/* ov12:0x00a52750 "pCam != NIL" */
extern const char D_00A52750[];
/* ov12:0x00a52760 "../rg_camera.euc.c" */
extern const char D_00A52760[];
/* ov12:0x00a52778 "pStudio != NIL" */
extern const char D_00A52778[];

static void _InitAbstructCamera(RgCamera *camera, RgDrawStudio *studio)
{
    if (camera == 0) {
        assert_prog(D_00A52750, D_00A52760, 86);
    }
    if (studio == 0) {
        assert_prog(D_00A52778, D_00A52760, 87);
    }
    camera->studio = studio;
    XrgClearVector(camera->eye);
    /*
     * cc1 2.96 -O2 reorders an unbroken run of independent stores to the
     * same base register by moving the last one to the front; grouping the
     * leading calls and stores in their own scope keeps this in the
     * original order (docs/compiler-patterns.md CP-0234/CP-0242).
     */
    do {
        XrgClearVector(camera->target);
        XrgCopyVector(camera->up, XrgVectorY());
        camera->actionState[0] = 0;
        camera->actionState[1] = 0;
        camera->actionState[4] = 1;
        camera->actionState[2] = 0;
        camera->actionState[3] = 0;
    } while (0);
    camera->actionState[5] = 0;
    camera->actionState[6] = 0;
}

static void _SetView(RgCamera *camera)
{
    float direction[3];
    RgDrawView *view;
    float pitch;
    float yaw;
    float roll;

    XrgCopyVectorXYZ(direction, camera->target);
    direction[1] = 0.0f;
    pitch = atan2f(camera->target[1], XrgLengthVector(direction));
    yaw = atan2f(direction[0], direction[2]) + 3.1415927f;
    if (yaw > 3.1415927f) {
        yaw -= 6.2831855f;
        while (yaw > 3.1415927f) {
            yaw -= 6.2831855f;
        }
    }
    if (yaw < -3.1415927f) {
        do {
            yaw += 6.2831855f;
        } while (yaw < -3.1415927f);
    }
    __builtin_memcpy(&roll, &camera->actionState[0], sizeof(roll));
    view = RgDrawStudioGetView(camera->studio);
    RgDrawViewSetPosition(view, camera->eye);
    RgDrawViewSetRotateX(view, pitch);
    RgDrawViewSetRotateY(view, yaw);
    RgDrawViewSetRotateZ(view, roll);
}

static int _GetVer3TimerID(char *timerName)
{
    unsigned int timerIndex;

    for (timerIndex = 0; timerIndex < 10; timerIndex++) {
        if (strcmp(s_aszVer3TimerName[timerIndex], timerName) == 0) {
            return timerIndex;
        }
    }
    return -1;
}

static int _IsActionRoll(RgCamera *camera)
{
    return _CheckBit(RG_CAMERA_ACTION_FLAGS(camera), 2);
}

static int _IsActionAdvance(RgCamera *camera)
{
    return _CheckBit(RG_CAMERA_ACTION_FLAGS(camera), 1);
}

static int _IsActionAttack(RgCamera *camera)
{
    return _CheckBit(RG_CAMERA_ACTION_FLAGS(camera), 4);
}

static int _IsActionSeeTarget(RgCamera *camera)
{
    return _CheckBit(RG_CAMERA_ACTION_FLAGS(camera), 5);
}

static int _IsActionLockAndAttack(RgCamera *camera)
{
    int result;

    result = 0;
    if (_CheckBit(RG_CAMERA_ACTION_FLAGS(camera), 4)) {
        result = _CheckBit(RG_CAMERA_ACTION_FLAGS(camera), 0) != 0;
    }
    return result;
}

static int _IsActionLockOn(RgCamera *camera)
{
    return _CheckBit(RG_CAMERA_ACTION_FLAGS(camera), 0);
}

static int _IsActionDash(RgCamera *camera)
{
    return _CheckBit(RG_CAMERA_ACTION_FLAGS(camera), 3);
}

extern RgDrawView *RgDrawStudioGetView(RgDrawStudio *pStudio);
extern int RgDrawViewIsPointInView(RgDrawView *view, RgPointVector *point, float margin);

static void _IsActionTargetInScrn(RgCamera *camera)
{
    /*
     * A RgPointVector-sized scratch buffer (rg_geom_point.c owns the type);
     * __RgGeomPointGetPos fills it and RgDrawViewIsPointInView reads it back.
     */
    unsigned char position[sizeof(RgVector)];

    /* actionState[3] (+0x4c) is the "enemy" geometry point of RgCameraDump's log. */
    __RgGeomPointGetPos((RgGeomPoint *)camera->actionState[3],
                         (RgPointVector *)position, D_00A52760, 220);
    RgDrawViewIsPointInView(RgDrawStudioGetView(camera->studio),
                             (RgPointVector *)position, 0.0f);
}

static int _IsActionTrue(void)
{
    return 1;
}

static int _IsActionFalse(void)
{
    return 0;
}

static void _InitVer3Timer(RgVer3Timer *timer, int (*predicate)(int), float duration)
{
    timer->predicate = predicate;
    timer->duration = duration;
    timer->remaining = 0.0f;
}

static void _PreJobVer3Timer(RgVer3Timer *timer, int value)
{
    if (timer->predicate != 0 && timer->predicate(value) != 0) {
        timer->remaining = timer->duration;
    }
}

static void _PastVer3Timer(RgVer3Timer *timer, float elapsed)
{
    float remaining;

    remaining = timer->remaining - elapsed;
    timer->remaining = remaining;
    if (remaining <= 0.0f) {
        timer->remaining = 0.0f;
    }
}

static int _IsVer3TimerActive(RgVer3Timer *timer)
{
    return timer->remaining > 0.0f;
}

static void _SettingVersion3(void)
{
}

extern int (*s_apConditionTbl_0[10])(int);
#define CAMF(c, o) (*(float *)((unsigned char *)(c) + (o)))
#define CAMI(c, o) (*(int *)((unsigned char *)(c) + (o)))

static void _SetVersion3Extent(RgCamera *camera)
{
  int i;
  XrgClearVector((float *) (((unsigned char *) camera) + 0x60));
  camera->farDistance = 12.0f;
  *((float *) (((unsigned char *) camera) + 0x74)) = 1.0f;
  *((float *) (((unsigned char *) camera) + 0x78)) = 1.0f;
  *((float *) (((unsigned char *) camera) + 0x7C)) = 0.5f;
  *((int *) (((unsigned char *) camera) + 0x14C)) = -1;
  *((float *) (((unsigned char *) camera) + 0x90)) = 0.7853982f;
  *((int *) (((unsigned char *) camera) + 0xA0)) = 0;
  *((int *) (((unsigned char *) camera) + 0x140)) = 0;
  *((float *) (((unsigned char *) camera) + 0x94)) = (*((float *) (((unsigned char *) camera) + 0x8C)) = 10.0f);
  *((float *) (((unsigned char *) camera) + 0x9C)) = 0.3f;
  *((int *) (((unsigned char *) camera) + 0x98)) = 0;
  for (i = 0; i < 10; i++)
  {
    _InitVer3Timer(((RgVer3Timer *) (((unsigned char *) camera) + 0xB0)) + i, s_apConditionTbl_0[i], 0.5f);
  }

  *((int *) (((unsigned char *) camera) + 0x144)) = 0;
  *((int *) (((unsigned char *) camera) + 0x148)) = 0;
  *((int *) (((unsigned char *) camera) + 0x134)) = 3;
  *((int *) (((unsigned char *) camera) + 0x138)) = 0;
  *((int *) (((unsigned char *) camera) + 0x130)) = 5;
  *((int *) (((unsigned char *) camera) + 0x13C)) = 3;
  }

struct RgTextInfoEntry {
    unsigned int type;
    const char *pszName;
    unsigned int destination;
    unsigned int auxiliary;
} __attribute__((aligned(8)));
struct RgTextInfoTbl {
    struct RgTextInfoEntry e[25];
};
extern void RgTextInfoRead(void *reader, struct RgTextInfoEntry *list, char *base);
extern unsigned int strlen(const char *s);
extern char D_00A527F8[], D_00A52808[], D_00A52818[], D_00A52828[], D_00A52838[];
extern char D_00A52848[], D_00A52858[], D_00A52860[], D_00A52868[], D_00A52870[];
extern char D_00A52878[], D_00A52880[], D_00A52888[], D_00A52898[], D_00A528A8[];
extern char D_00A528B8[], D_00A528C8[], D_00A528D0[], D_00A528D8[];

static void _LoadVersion3(RgCamera *camera, void *reader)
{
    char cond[4][64];
    char ope[2][64];
    char *name;
    int *op;
    int i;
    int id;
    char *p;
    char *base;
    struct RgTextInfoEntry list[25] = {
        [0]= { 1, D_00A527F8, 0x70, 0 },
        [1] = { 1, D_00A52808, 0x74, 0 },
        [2] = { 1, D_00A52818, 0x78, 0 },
        [3] = { 1, D_00A52828, 0x7C, 0 },
        [4] = { 3, D_00A52838, 0xA0, 0 },
        [5] = { 1, s_aszVer3TimerName[0], 0xB4, 0 },
        [6] = { 1, s_aszVer3TimerName[1], 0xC0, 0 },
        [7] = { 1, s_aszVer3TimerName[2], 0xCC, 0 },
        [8] = { 1, s_aszVer3TimerName[3], 0xD8, 0 },
        [9] = { 1, s_aszVer3TimerName[4], 0xE4, 0 },
        [10] = { 1, s_aszVer3TimerName[5], 0xF0, 0 },
        [11] = { 1, s_aszVer3TimerName[6], 0xFC, 0 },
        [12] = { 1, s_aszVer3TimerName[7], 0x108, 0 },
        [13] = { 1, D_00A52848, 0xB4, 0 },
        [14] = { 8, D_00A52858, (unsigned int) cond[0], 0 },
        [15] = { 8, D_00A52860, (unsigned int) cond[1], 0 },
        [16] = { 8, D_00A52868, (unsigned int) cond[2], 0 },
        [17] = { 8, D_00A52870, (unsigned int) cond[3], 0 },
        [18] = { 8, D_00A52878, (unsigned int) ope[0], 0 },
        [19] = { 8, D_00A52880, (unsigned int) ope[1], 0 },
        [20] = { 1, D_00A52888, 0x8C, 0 },
        [21] = { 1, D_00A52898, 0x90, 0 },
        [22] = { 1, D_00A528A8, 0x94, 0 },
        [23] = { 1, D_00A528B8, 0x9C, 0 },
        [24] = { 12 },
    };
    name = ope[0];
    base = (char *) camera;

    for (i = 3; i >= 0; i--) {
        cond[i][0] = 0;
    }
    name[64] = 0;
    name[0] = 0;
    RgTextInfoRead(reader, list, base);
    *(float *) (base + 0x90) = (*(float *) (base + 0x90) * 3.1415927f) / 180.0f;
    for (i = 0; i < 4; i++) {
        char *p = cond[i];
        if (*p == '!') {
            p++;
            *(int *) (base + 0x140) |= (1 << i);
        }
        id = _GetVer3TimerID(p);
        if (id >= 0) {
            ((int *) (base + 0x130))[i] = id;
        }
    }
    op = (int *) (base + 0x144);
    for (i = 0; i < 2; i++) {
        name = ope[i];
        if (!strcmp(name, D_00A528C8)) {
            *op = 1;
        } else if (strcmp(name, D_00A528D0) == 0) {
            *op = 0;
        } else if (strlen(name) != 0) {
            RgError(D_00A528D8, D_00A52760, 0x1AD, name);
        }
        op++;
    }
}

extern const char D_00A528F8[];
extern float RgGeomRobotGetRotate(void *point);
extern float XrgInnerVector(float *a, float *b);
extern void XrgOuterVector(float *dst, float *a, float *b);
extern void RgGeomPointGetVel(void *point, float *vel);
extern float sinf(float x);
extern float cosf(float x);

static void _PassTimeVersion3(RgCamera *camera, float elapsed)
{
    float sp0[4];
    float spUnk10[4];
    float sp20[4];
    float sp30[4];
    float sp40[4];
    float sp50[4];
    float sp60[4];
    int sp70[10];
    int spA0[4];
    int spB0[2];
    float spC0[4];
    float spD0[4];
    float spE0[4];
    float spF0[4];
    float *enemy;
    float *me;
    int sp100;
    int k;
    int i;
    int *pv;
    int op;
    int bit;
    float f22;
    float f23;
    float f24;
    float f20;
    float f21;
    float f6;
    float f0;
    float f1;
    float f2;
    float f4;
    float f26;
    float f25;
    float f22v;
    float fTmp;
    float fq;
    float fd;

    sp100 = CAMI(camera, 0x50);
    CAMI(camera, 0x50) = 0;
    me = (float *)(int)CAMI(camera, 0x48);
    enemy = (float *)(int)CAMI(camera, 0x4C);
    if (CAMI(camera, 0x14C) >= 0) {
        XrgPadSetID(CAMI(camera, 0x14C));
        if (XrgPadIsSelect() != 0) {
            if (camera->farDistance >= 10.0f) {
                camera->farDistance = 8.0f;
            } else {
                camera->farDistance = 14.0f;
            }
        }
    }
    XrgCopyVector(sp40, ((float *)((unsigned char *)camera + 0x10)));
    XrgCopyVector(sp50, ((float *)((unsigned char *)camera + 0x30)));
    XrgCopyVector(sp60, ((float *)((unsigned char *)camera + 0x20)));
    f21 = atan2f(sp60[0], sp60[2]);
    f23 = CAMF(camera, 0x40);
    f24 = camera->farDistance;
    __RgGeomPointGetPos((RgGeomPoint *)me, (RgPointVector *)sp0, D_00A52760, 0x1EE);
    if (enemy != 0) {
        __RgGeomPointGetPos((RgGeomPoint *)enemy, (RgPointVector *)spUnk10, D_00A52760, 0x1F0);
    } else {
        XrgClearVector(spUnk10);
    }
    XrgSubVector(spD0, spUnk10, sp0);
    f26 = XrgLengthVector(spD0);
    f25 = RgGeomRobotGetRotate(me);
    f22 = atan2f(spD0[0], spD0[2]);
    for (i = 0; i < 10; i++) {
        _PreJobVer3Timer((RgVer3Timer *)((unsigned char *)camera + 0xB0) + i, (int)camera);
        sp70[i] = _IsVer3TimerActive((RgVer3Timer *)((unsigned char *)camera + 0xB0) + i);
    }
    for (i = 0; i < 4; i++) {
        spA0[i] = sp70[CAMI(camera, 0x130 + i * 4)];
        {
            int bitI = 1 << i;

            if (CAMI(camera, 0x140) & bitI) {
                spA0[i] = !spA0[i];
            }
        }
        if (0) {
            spA0[i] = !spA0[i];
        }
    }
    for (k = 0; k < 2; k++) {
        op = CAMI(camera, 0x144 + k * 4);
        pv = &spA0[k * 2];
        switch (op) {
        case 1:
            spB0[k] = (pv[0] != 0) && (pv[1] != 0);
            break;
        case 0:
            {
                int r = 0;
                int *q = &spB0[k];
                if (pv[0] != 0) {
                    goto one;
                }
                if (pv[1] == 0) {
                    goto st;
                }
            one:
                r = 1;
            st:
                *q = r;
            }
            break;
        default:
            RgError("unknown bool operator %d (index %d)", D_00A52760, 0x20D, op, k);
            break;
        }
    }
    XrgCopyVector(sp20, sp0);
    if (spB0[0] != 0) {
        if (spB0[1] != 0) {
            XrgSubVector(spC0, spUnk10, sp20);
            f20 = atan2f(spC0[0], spC0[2]);
        } else {
            f20 = f21;
        }
    } else {
        if (CAMI(camera, 0xA0) != 0) {
            RgGeomPointGetVel(me, spC0);
            f20 = atan2f(spC0[0], spC0[2]);
        } else {
            f20 = RgGeomRobotGetRotate(me);
        }
    }
    XrgCopyVector(sp30, XrgVectorY());
    f1 = CAMF(camera, 0x8C);
    f2 = f22 - f25;
    f4 = (f1 - f26) / f1;
    while (f2 > 3.1415927f) {
        f2 -= 6.2831855f;
    }
    if (f2 < -3.1415927f) {
        do {
            f2 += 6.2831855f;
        } while (f2 < -3.1415927f);
    }
    if (f2 < 0.0f) {
        f2 = -f2;
    }
    fTmp = CAMF(camera, 0x90);
    fq = fTmp / (3.1415927f - fTmp);
    fd = f2 - fq;
    if (f4 > 0.0f) {
        if (fd > 0.0f) {
            f24 = camera->farDistance + (CAMF(camera, 0x94) * f4);
        }
    }
    f22v = 0.0f;
    if (sp70[4] != 0) {
        RgGeomPointGetVel(me, spE0);
        XrgOuterVector(spF0, ((float *)((unsigned char *)camera + 0x20)), ((float *)((unsigned char *)camera + 0x30)));
        f22v = (XrgInnerVector(spF0, spE0) > 0.0f) ? 0.03926991f : -0.03926991f;
    }
    switch (sp100) {
    default:
        {
        f21 = f20;
        CAMF(camera, 0x98) = f24;
        f23 = (f23 * 0.9f) + (f22v * 0.1f);
        sp40[0] = sp20[0] - (sinf(f21) * CAMF(camera, 0x98));
        sp40[1] = sp20[1] + CAMF(camera, 0x78);
        sp40[2] = sp20[2] - (cosf(f21) * CAMF(camera, 0x98));
        XrgCopyVector(sp50, sp30);
    }
        break;
    case 0:
        {
        f6 = f20 - f21;
        if (f6 > 3.1415927f) {
            do {
                f6 -= 6.2831855f;
            } while (f6 > 3.1415927f);
        }
        if (f6 < -3.1415927f) {
            do {
                f6 += 6.2831855f;
            } while (f6 < -3.1415927f);
        }
        f0 = CAMF(camera, 0x9C);
        f21 = f21 + (f6 * CAMF(camera, 0x7C));
        f23 = (f23 * 0.9f) + (f22v * 0.1f);
        CAMF(camera, 0x98) = (f24 * f0) + (CAMF(camera, 0x98) * (1.0f - f0));
        sp40[0] = sp20[0] - (sinf(f21) * CAMF(camera, 0x98));
        sp40[1] = sp20[1] + CAMF(camera, 0x78);
        sp40[2] = sp20[2] - (cosf(f21) * CAMF(camera, 0x98));
        XrgCopyVector(sp50, sp30);
    }
        break;
    }
    spD0[0] = sinf(f21) * camera->farDistance;
    spD0[1] = -CAMF(camera, 0x74);
    spD0[2] = cosf(f21) * camera->farDistance;
    XrgCopyVectorXYZ(((float *)((unsigned char *)camera + 0x20)), spD0);
    XrgCopyVectorXYZ(((float *)((unsigned char *)camera + 0x10)), sp40);
    XrgCopyVectorXYZ(((float *)((unsigned char *)camera + 0x30)), sp50);
    CAMF(camera, 0x40) = f23;
    _SetView(camera);
    for (i = 0; i < 10; i++) {
        _PastVer3Timer((RgVer3Timer *)((unsigned char *)camera + 0xB0) + i, elapsed);
    }
    RgCameraResetAction(camera);
}

static void _SetVersion3Extent(RgCamera *camera);
static void _LoadVersion3(RgCamera *camera, void *reader);

static void _InitVersion3(RgCamera *camera, RgDrawStudio *studio)
{
    if (camera == 0) {
        assert_prog(D_00A52750, D_00A52760, 652);
    }
    _InitAbstructCamera(camera, studio);
    camera->actionState[5] = (int) _SettingVersion3;
    camera->actionState[6] = (int) _LoadVersion3;
    _SetVersion3Extent(camera);
}

RgCamera *CreateRgCamera(int cameraVersion, RgDrawStudio *studio)
{
    RgCamera *camera;

    camera = 0;
    if (cameraVersion == 0) {
        RgError(D_00A52920, D_00A52760, 676, cameraVersion);
    } else if (cameraVersion == 1) {
        RgError(D_00A52920, D_00A52760, 679, cameraVersion);
    } else if (cameraVersion == 2) {
        RgCamera *allocatedCamera;

        allocatedCamera = RgHeapAlloc(InstanceOfRgHeap(), 0x150, D_00A52760, 683);
        if (allocatedCamera == 0) {
            assert_prog(D_00A52750, D_00A52760, 684);
        }
        _InitVersion3(allocatedCamera, studio);
        camera = allocatedCamera;
    } else {
        assert_prog(D_00A52940, D_00A52760, 689);
    }
    return camera;
}

void DisposeRgCamera(RgCamera *pRgCam)
{
    if (pRgCam == 0) {
        assert_prog(D_00A52948, D_00A52760, 697);
    }
    RgHeapFree(InstanceOfRgHeap(), pRgCam, D_00A52760, 698);
}

typedef struct RgCameraEssence {
    int me_point_address;
    int enemy_point_address;
} RgCameraEssence;

void RgCameraSetting(RgCamera *camera, RgCameraEssence *essence)
{
    void (*setCamera)(RgCamera *, RgCameraEssence *);

    if (camera == 0) {
        assert_prog(D_00A52750, D_00A52760, 709);
    }
    if (essence == 0) {
        assert_prog(D_00A52958, D_00A52760, 710);
    }
    setCamera = (void (*)(RgCamera *, RgCameraEssence *))camera->actionState[5];
    camera->actionState[2] = essence->me_point_address;
    camera->actionState[3] = essence->enemy_point_address;
    if (setCamera != 0) {
        setCamera(camera, essence);
    }
    RgDrawViewInit(RgDrawStudioGetView(camera->studio));
}

void RgCameraLoadText(RgCamera *pCam, void *pReader)
{
    void (*loadText)(RgCamera *, void *);

    if (pCam == 0) {
        assert_prog(D_00A52750, D_00A52760, 726);
    }
    if (pReader == 0) {
        assert_prog(D_00A52968, D_00A52760, 727);
    }
    /*
     * actionState[6] (+0x58) holds a per-mode text-load callback; the array's
     * other indices are unrelated (see RgCamera's own definition).
     */
    loadText = (void (*)(RgCamera *, void *))pCam->actionState[6];
    if (loadText != 0) {
        loadText(pCam, pReader);
    }
}

void RgCameraSetFarMode(RgCamera *pCam, int enable)
{
    if (pCam == 0) {
        assert_prog(D_00A52750, D_00A52760, 737);
    }
    if (enable != 0) {
        pCam->farDistance = 14.0f;
    } else {
        pCam->farDistance = 8.0f;
    }
}

void RgCameraSelectableFar(RgCamera *pCam, int level)
{
    if (pCam == 0) {
        assert_prog(D_00A52750, D_00A52760, 750);
    }
    if (level >= 2) {
        level = -1;
    }
    RG_CAMERA_SELECTABLE_FAR(pCam) = level;
}

void RgCameraLocal(RgCamera *pCam, RgMatrix matrix)
{
    if (pCam == 0) {
        assert_prog(D_00A52750, D_00A52760, 762);
    }
    RgCalcLocalForChar(matrix, pCam->eye, pCam->target, pCam->up);
}

void RgCameraGetTarget(RgCamera *pCam, RgPointVector *position)
{
    if (pCam == 0) {
        assert_prog(D_00A52750, D_00A52760, 771);
    }
    /* actionState[2] (+0x48) is the "me" geometry point of RgCameraDump's log. */
    __RgGeomPointGetPos((RgGeomPoint *)pCam->actionState[2], position, D_00A52760, 772);
}

RgDrawStudio *RgCameraGetStudio(RgCamera *pCam)
{
    if (pCam == 0) {
        assert_prog(D_00A52750, D_00A52760, 779);
    }
    return pCam->studio;
}

void RgCameraResetAction(RgCamera *pRgCam)
{
    if (pRgCam == 0) {
        assert_prog(D_00A52948, D_00A52760, 789);
    }
    pRgCam->actionState[1] = 0;
}

void RgCameraSetActionLockedOn(RgCamera *pRgCam, int enable)
{
    if (pRgCam == 0) {
        assert_prog(D_00A52948, D_00A52760, 797);
    }
    _SetBit(&pRgCam->actionState[1], 0, enable);
}

void RgCameraSetActionAdvance(RgCamera *pRgCam, int enable)
{
    if (pRgCam == 0) {
        assert_prog(D_00A52948, D_00A52760, 805);
    }
    _SetBit(&pRgCam->actionState[1], 1, enable);
}

void RgCameraSetActionRoll(RgCamera *pRgCam, int enable)
{
    if (pRgCam == 0) {
        assert_prog(D_00A52948, D_00A52760, 813);
    }
    _SetBit(&pRgCam->actionState[1], 2, enable);
}

void RgCameraSetActionDash(RgCamera *pRgCam, int enable)
{
    if (pRgCam == 0) {
        assert_prog(D_00A52948, D_00A52760, 821);
    }
    _SetBit(&pRgCam->actionState[1], 3, enable);
}

void RgCameraSetActionAttack(RgCamera *pRgCam, int enable)
{
    if (pRgCam == 0) {
        assert_prog(D_00A52948, D_00A52760, 829);
    }
    _SetBit(&pRgCam->actionState[1], 4, enable);
}

void RgCameraSetActionSeeTarget(RgCamera *pRgCam, int enable)
{
    if (pRgCam == 0) {
        assert_prog(D_00A52948, D_00A52760, 837);
    }
    _SetBit(&pRgCam->actionState[1], 5, enable);
}

void RgCameraControl(RgCamera *pCam, float elapsed)
{
    if (pCam == 0) {
        assert_prog(D_00A52750, D_00A52760, 846);
    }
    _PassTimeVersion3(pCam, elapsed);
}

void RgCameraDump(RgCamera *pRgCam)
{
    if (pRgCam == 0) {
        assert_prog(D_00A52948, D_00A52760, 854);
    }
    XrgLog(D_00A52978, D_00A52760, 858, pRgCam);
    XrgLog(D_00A52998, D_00A52760, 859,
           (void *)pRgCam->actionState[2], (void *)pRgCam->actionState[3]);
}

/*
 * OV12 original TU 38: 0x00a22a78..0x00a239d0 (17 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_disp_gameinfo.h"

extern void assert_prog(const char *expression, const char *source_file,
                        int line);
const char D_00A54560[16] = "pInfo != NIL";
const char D_00A54570[32] = "../rg_disp_gameinfo.euc.c";
extern void *InstanceOfRgBattleCommonData(void);
extern int RgBattleCommonDataGetDispTex(void *env);
extern XrgPaint2D *CreateXrgPaint2D_sub(const char *sourceFile, int line);
extern void XrgPaint2DSetDrawPrio(XrgPaint2D *paint, int prio);
extern void XrgPaint2DSetDrawID(XrgPaint2D *paint, int drawID);

void _InitInfo(RgDispGameInfo *pInfo)
{
    int dispTex;
    XrgPaint2D *paint;

    if (pInfo == 0) {
        assert_prog(D_00A54560, D_00A54570, 73);
    }
    pInfo->hostRobot = 0;
    pInfo->subRobot = 0;
    dispTex = RgBattleCommonDataGetDispTex(InstanceOfRgBattleCommonData());
    pInfo->value1 = 1.0f;
    pInfo->value2 = 1.0f;
    pInfo->value3 = 2;
    pInfo->studio = -2;
    pInfo->dispTex = dispTex;
    paint = CreateXrgPaint2D_sub(D_00A54570, 90);
    pInfo->paint = paint;
    XrgPaint2DSetDrawPrio(paint, 4);
    XrgPaint2DSetDrawID(pInfo->paint, pInfo->studio);
    pInfo->value4 = 0;
    pInfo->value5 = 0;
}

extern void assert_prog(const char *expression, const char *source_file,
                        int line);
extern void DisposeXrgPaint2D_sub(XrgPaint2D *paint, const char *source_file,
                                  int line);

/*
 * External file-backed witnesses, not candidate-emitted data: this window is
 * asm-owned scaffold data (splat names, no config/symbols/ov12.txt entry).
 *
 * ov12:0x00a54560 contains the assertion expression "pInfo != NIL".
 * ov12:0x00a54570 contains the source filename "../rg_disp_gameinfo.euc.c".
 */

void _DisposeInfo(RgDispGameInfo *pInfo) {
    if (pInfo == 0) {
        assert_prog(D_00A54560, D_00A54570, 100);
    }
    DisposeXrgPaint2D_sub(pInfo->paint, D_00A54570, 101);
}

void _SetHostRobot(RgDispGameInfo *pInfo, void *hostRobot) {
    if (pInfo == 0) {
        assert_prog(D_00A54560, D_00A54570, 108);
    }
    pInfo->hostRobot = hostRobot;
}

void _SetSubRobot(RgDispGameInfo *pInfo, void *subRobot) {
    if (pInfo == 0) {
        assert_prog(D_00A54560, D_00A54570, 114);
    }
    pInfo->subRobot = subRobot;
}

void _PassTimeInfo(RgDispGameInfo *pInfo, float deltaTime) {
    if (pInfo == 0) {
        assert_prog(D_00A54560, D_00A54570, 122);
    }
}

extern void *RgBxxGetPic(int archive, const char *pictureName);
extern void RgError(const char *message, const char *source_file, int line,
                    ...);
/*
 * The second argument selects a blend mode (XrgPaint2DAlpha switches over
 * 0..5; _DispInfo passes 1 to _paint_set_tex_alpha), not an alpha level.
 */
extern void XrgPaint2DAlpha(XrgPaint2D *paint, int blendMode);
extern void XrgPaint2DDrawXYWH(XrgPaint2D *paint, int mode, int x, int y,
                               int width, int height);
extern void XrgPaint2DUseTexture(XrgPaint2D *paint, void *pic);

/*
 * Mode bit 0x20 of XrgPaint2DRect.mode (documented in src/ov12/xrg_paint2d.h)
 * takes the drawn extent from the bound picture instead of width/height; the
 * spelling follows src/ov12/rg_piclist.c.
 */
#define XRG_PAINT2D_MODE_USE_PIC_SIZE 0x20

/*
 * ov12:0x00a54590 contains the error message "not exist '%s' picture".
 */
const char D_00A54590[24] = "not exist '%s' picture";

void _paint_one_texture_alpha(RgDispGameInfo *pInfo, const char *pictureName,
                              int x, int y, int blendMode) {
    void *pic;

    pic = RgBxxGetPic(pInfo->dispTex, pictureName);
    if (pic == 0) {
        RgError(D_00A54590, D_00A54570, 133, pictureName);
    }
    XrgPaint2DUseTexture(pInfo->paint, pic);
    XrgPaint2DAlpha(pInfo->paint, blendMode);
    XrgPaint2DDrawXYWH(pInfo->paint, XRG_PAINT2D_MODE_USE_PIC_SIZE, x, y,
                       0, 0);
}

void _paint_one_texture(RgDispGameInfo *pInfo, const char *pictureName, int x,
                        int y) {
    _paint_one_texture_alpha(pInfo, pictureName, x, y, 0);
}

void _paint_set_tex_alpha(RgDispGameInfo *pInfo, const char *pictureName,
                          int blendMode) {
    void *pic;

    pic = RgBxxGetPic(pInfo->dispTex, pictureName);
    if (pic == 0) {
        RgError(D_00A54590, D_00A54570, 150, pictureName);
    }
    XrgPaint2DUseTexture(pInfo->paint, pic);
    XrgPaint2DAlpha(pInfo->paint, blendMode);
}

/*
 * This caller's rectangle has the layout of the descriptor in the paint
 * renderer's TU. The descriptor is not yet exported by its public header;
 * retain a distinct TU-local type until that owner publishes the interface.
 * DrawRect consumes the whole descriptor, including its alignment gap.
 */
typedef struct RgDispGameInfoColorRect {
    int mode;
    unsigned char quadword_alignment_gap[12];
    int x;
    int y;
    int width;
    int height;
    float scale;
    float angle;
} RgDispGameInfoColorRect;

extern void InitXrgPaint2DRect(RgDispGameInfoColorRect *rectangle, int mode);
extern void XrgPaint2DColor(XrgPaint2D *paint, const unsigned int *color);
extern void XrgPaint2DDrawRect(XrgPaint2D *paint,
                               RgDispGameInfoColorRect *rectangle);

/* Anchored, rotated, scaled rectangle with per-frame jitter. */
#define RG_DISP_GAME_INFO_COLOR_RECT_MODE 0x1E5

void _paint_xy_color(RgDispGameInfo *pInfo, int x, int y, int intensity,
                     float angle, float scale)
{
    unsigned int color[4] = {intensity, intensity, intensity, 127};
    RgDispGameInfoColorRect rectangle;

    InitXrgPaint2DRect(&rectangle, RG_DISP_GAME_INFO_COLOR_RECT_MODE);
    rectangle.x = x;
    rectangle.y = y;
    rectangle.scale = scale;
    rectangle.angle = angle;
    XrgPaint2DColor(pInfo->paint, color);
    XrgPaint2DDrawRect(pInfo->paint, &rectangle);
}

extern const char D_00A545A8[];
extern const char D_00A545C0[];
extern const char D_00A545C8[];
extern const char D_00A545D0[];
extern const char D_00A545D8[];
extern const char D_00A545E8[];
extern const char D_00A545F0[];
extern const char D_00A545F8[];
extern const char D_00A54600[];
extern unsigned int RgRobotGetStatusFlags(void *robot);
extern void *RgRobotGetGeom(void *robot);
extern void __RgGeomPointGetPos(void *point, float *pos, const char *file, int line);
extern int RgRobotIsConfused(void *robot);
extern void XrgSubVector(float *dst, float *a, float *b);
extern float XrgLengthVector(float *v);
extern float XrgRand(float lo, float hi);
extern void XrgPaint2DOffset3DForce(XrgPaint2D *paint, float *pos, float dist);
extern void XrgPaint2DOffset2D(XrgPaint2D *paint, int x, int y);
extern void XrgPaint2DOffsetResult(XrgPaint2D *paint, void *result);
extern void XrgPaint2DFlush(XrgPaint2D *paint);
extern void *RgRobotGetWeapon(void *robot, unsigned int slot);
extern int RgWeaponIsTargetting(void *weapon);
extern float RgWeaponGetShotNum(void *weapon);
extern int RgWeaponIsNotBusy(void *weapon);
extern float atan2f(float y, float x);
extern float sinf(float x);
extern float cosf(float x);
typedef struct RgDispGameInfoOffset {
    int unk0;
    int unk4;
    int x;
    int y;
    int valid;
    int unk14[3];
    float dx;
    float dy;
} RgDispGameInfoOffset;
#define PXC(p, x, y, i, a, s) \
    ((void (*)(RgDispGameInfo *, int, int, float, float, int))_paint_xy_color)(p, x, y, a, s, i)

void _DispInfo(RgDispGameInfo *pInfo)
{
  float len;
  float one;
  float hostPos[4];
  float subPos[4];
  float diff[4];
  float shake[4];
  XrgPaint2D *paint;
  RgDispGameInfoOffset *offset;
  void *host;
  void *weapon;
  unsigned int flags;
  int confused;
  float a;
  unsigned int new_var2;
  float b;
  float c;
  void *hostGeom;
  float new_var4;
  void *subGeom;
  RgDispGameInfo *new_var5;
  float t;
  float dist;
  float scale;
  float tilt;
  float alpha;
  float angle;
  float rot;
  float r;
  float cx;
  float cy;
  int x;
  float new_var3;
  int new_var;
  int y;
  int wobble;
  int intensity;
  paint = pInfo->paint;
  new_var5 = pInfo;
  if (new_var5->subRobot != 0)
  {
    offset = (RgDispGameInfoOffset *) (new_var5->unmodeled_14 + 0xc);
    a = (*((float *) (&new_var5->value4))) + 0.09817477f;
    while (a > 3.1415927f)
    {
      a -= 6.2831855f;
    }

    while (a < (-3.1415927f))
    {
      a += 6.2831855f;
    }

    *((float *) (&new_var5->value4)) = a;
    b = 0.33379424f + (*((float *) (&new_var5->value5)));
    while (b > 3.1415927f)
    {
      b -= 6.2831855f;
    }

    while (b < (-3.1415927f))
    {
      b += 6.2831855f;
    }

    *((float *) (&new_var5->value5)) = b;
    flags = RgRobotGetStatusFlags(new_var5->hostRobot);
    host = new_var5->hostRobot;
    hostGeom = RgRobotGetGeom(host);
    subGeom = RgRobotGetGeom(new_var5->subRobot);
    __RgGeomPointGetPos(hostGeom, hostPos, D_00A54570, 201);
    __RgGeomPointGetPos(subGeom, subPos, D_00A54570, 202);
    confused = RgRobotIsConfused(host);
    XrgSubVector(diff, hostPos, subPos);
    t = XrgLengthVector(diff);
    len = t;
    dist = (len < 0.0f) ? (0.0f) : ((len > 50.0f) ? (50.0f) : (len));
    one = 1.0f;
    len = one - (dist / 50.0f);
    alpha = one;
    t = len;
    tilt = 1.5707964f * (t - 0.5f);
    scale = 0.75f + (t * 0.6f);
    if (confused != 0)
    {
      shake[0] = hostPos[0] + XrgRand(-50.0f, 50.0f);
      shake[1] = hostPos[1] + XrgRand(-10.0f, 10.0f);
      shake[2] = hostPos[2] + XrgRand(-50.0f, 50.0f);
      shake[3] = one;
      XrgPaint2DOffset3DForce(paint, shake, 180.0f);
      new_var5->value2 = alpha;
      new_var5->value3 = 3;
    }
    else
    {
      new_var2 = new_var5->value3;
      switch (new_var2)
      {
        case 1:
          new_var5->value2 -= 0.1f;
          if (new_var5->value2 < 0.0f)
        {
          new_var5->value2 = 0.0f;
          new_var5->value3 = 2;
        }

        case 2:
          if (flags & 1)
        {
          new_var5->value3 = 3;
        }
          XrgPaint2DOffset2D(paint, offset->x, offset->y);
          new_var5->value2 = 0.0f;
          break;

        case 3:
          new_var5->value2 += 0.1f;
          if (new_var5->value2 > 1.0f)
        {
          new_var5->value2 = 1.0f;
          new_var5->value3 = 0;
        }

        case 0:
          if ((flags & 1) == 0)
        {
          new_var5->value3 = 1;
        }
          XrgPaint2DOffset3DForce(paint, subPos, 180.0f);
          break;

        default:
          RgError(D_00A545A8, D_00A54570, 263, new_var5->value3);
          break;

      }

    }
    XrgPaint2DOffsetResult(new_var5->paint, offset);
    r = new_var5->value2;
    alpha = r;
    _paint_set_tex_alpha(new_var5, D_00A545C0, 1);
    PXC(new_var5, 0, 0, 0x80, tilt, alpha);
    _paint_set_tex_alpha(new_var5, D_00A545C8, 1);
    PXC(new_var5, 0, 0, 0x80, 0.0f, alpha);
    _paint_set_tex_alpha(new_var5, D_00A545D0, 1);
    PXC(new_var5, 0, 0, 0x80, 0.0f, alpha * scale);
    if ((new_var5->value3 == 0) && (confused == 0))
    {
      if (!(offset->valid == 0))
      {
        angle = atan2f(offset->dy, offset->dx);
        c = *((float *) (&new_var5->value5));
        while (c > 3.1415927f)
        {
          c -= 6.2831855f;
        }

        while ((-3.1415927f) > c)
        {
          c += 6.2831855f;
        }

        r = sinf(c) * 5.0f;
        cx = cosf(angle) * r;
        cy = sinf(angle) * r;
        angle += 1.5707964f;
        rot = -angle;
        new_var4 = cosf(rot) * cx;
        x = new_var4 - (sinf(rot) * cy);
        new_var3 = sinf(rot) * cx;
        y = new_var3 + (cosf(rot) * cy);
      }
      else
      {
        angle = *((float *) (&new_var5->value4));
        y = 0;
        x = 0;
      }
      _paint_set_tex_alpha(new_var5, D_00A545D8, 1);
      do
      {
        PXC(new_var5, x, y, 0x80, angle, 1.0f);
      }
      while (0);
    }
    if (new_var5->value2 > 0.3f)
    {
      {
        float v;
        v = *((float *) (&new_var5->value4));
        len = v;
        one = len;
        wobble = sinf(v * 4.0f) * 8.0f;
      }
      weapon = RgRobotGetWeapon(host, 0);
      if (((weapon != 0) && RgWeaponIsTargetting(weapon)) && (RgWeaponGetShotNum(weapon) > 0.0f))
      {
        intensity = 0x80;
        if ((new_var = RgWeaponIsNotBusy(weapon)) == 0)
        {
          intensity = 0x20;
        }
        _paint_set_tex_alpha(new_var5, D_00A545E8, 1);
        PXC(new_var5, wobble - 0x30, 0, intensity, 0.0f, 1.0f);
      }
      weapon = RgRobotGetWeapon(host, 1);
      if (((weapon != 0) && RgWeaponIsTargetting(weapon)) && (RgWeaponGetShotNum(weapon) > 0.0f))
      {
        intensity = 0x80;
        if (RgWeaponIsNotBusy(weapon) == 0)
        {
          intensity = 0x20;
        }
        _paint_set_tex_alpha(new_var5, D_00A545F0, 1);
        PXC(new_var5, 0x30 - wobble, 0, intensity, 0.0f, 1.0f);
      }
      weapon = RgRobotGetWeapon(host, 2);
      if (((weapon != 0) && RgWeaponIsTargetting(weapon)) && (0.0f < RgWeaponGetShotNum(weapon)))
      {
        intensity = 0x80;
        if (RgWeaponIsNotBusy(weapon) == 0)
        {
          intensity = 0x20;
        }
        wobble -= 0x30;
        _paint_set_tex_alpha(new_var5, D_00A545F8, 1);
        alpha = 0.0f;
        PXC(new_var5, 0, wobble, intensity, alpha, 1.0f);
        _paint_set_tex_alpha(new_var5, D_00A54600, 1);
        PXC(new_var5, 0, -wobble, intensity, 0.0f, 1.0f);
      }
    }
  }
  XrgPaint2DFlush(new_var5->paint);
}

extern RgHeap *InstanceOfRgHeap(void);
extern void *RgHeapAlloc(void *heap, unsigned int size,
                         const char *source_file, int line);
extern void _InitInfo(RgDispGameInfo *pInfo);

RgDispGameInfo *CreateRgDispGameInfo(void) {
    RgDispGameInfo *pInfo;

    pInfo = RgHeapAlloc(InstanceOfRgHeap(), RG_DISP_GAME_INFO_SIZE,
                        D_00A54570, 360);
    _InitInfo(pInfo);
    return pInfo;
}

extern void RgHeapFree(RgHeap *heap, void *ptr, const char *source_file,
                       int line);

void DisposeRgDispGameInfo(RgDispGameInfo *pInfo) {
    if (pInfo == 0) {
        assert_prog(D_00A54560, D_00A54570, 367);
    }
    _DisposeInfo(pInfo);
    RgHeapFree(InstanceOfRgHeap(), pInfo, D_00A54570, 369);
}

void RgDispGameInfoSetHostRobot(RgDispGameInfo *pInfo, void *hostRobot) {
    if (pInfo == 0) {
        assert_prog(D_00A54560, D_00A54570, 378);
    }
    _SetHostRobot(pInfo, hostRobot);
}

void RgDispGameInfoSetSubRobot(RgDispGameInfo *pInfo, void *subRobot) {
    if (pInfo == 0) {
        assert_prog(D_00A54560, D_00A54570, 385);
    }
    _SetSubRobot(pInfo, subRobot);
}

extern void XrgPaint2DSetDrawID(XrgPaint2D *paint, int drawId);

void RgDispGameInfoSetStudio(RgDispGameInfo *pInfo, int studio) {
    if (pInfo == 0) {
        assert_prog(D_00A54560, D_00A54570, 394);
    }
    pInfo->studio = studio;
    XrgPaint2DSetDrawID(pInfo->paint, studio);
}

void RgDispGameInfoPassTime(RgDispGameInfo *pInfo, float deltaTime) {
    if (pInfo == 0) {
        assert_prog(D_00A54560, D_00A54570, 405);
    }
    _PassTimeInfo(pInfo, deltaTime);
}

extern void _DispInfo(RgDispGameInfo *pInfo);

void RgDispGameInfoDisplay(RgDispGameInfo *pInfo) {
    if (pInfo == 0) {
        assert_prog(D_00A54560, D_00A54570, 411);
    }
    _DispInfo(pInfo);
}

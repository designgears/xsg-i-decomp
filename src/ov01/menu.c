/*
 * OV01 original TU 9: 0x00a26018..0x00a2c3a0 (91 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov01/battle_init.h"
#include "ov01/calc.h"
extern CalcUnitParam *calcUPGet(ObjectTask *unit);

typedef struct MenuEtherStatusEntry {
    unsigned char unmodeled_00[0xE];
    short etherStatus;
} MenuEtherStatusEntry;

extern const signed char *font_0;
extern const signed char *font_1;
extern void menuStatSprite();

extern ObjectTask *pMenuBat;

void menuInitBat(void) {
    pMenuBat = 0;
}

extern int menuSel;
extern int menuSelCnt;
extern int menuResBat;
void menuBatObj(ObjectTask *task);
void menuBatObjDraw(ObjectTask *task);
extern ObjectTask *objEntry2(void *argument, void (*callback)(ObjectTask *task));
extern void objStdInit(ObjectTask *task);
extern int D_00A5A310[];
typedef struct { float a; float b; } SplPt;
extern SplPt D_00A5A330[];
extern char chaTbl_00A3F038[];
extern char agwsTbl_00A3F088[];
extern void calcSplineInit(void *a, void *b, int c, int d);
int sndSysSePlay(int soundId);

void menuOpenBat(ObjectTask *unit) {
    ObjectTask *task;
    float *p;
    int *tbl;
    float y, x;

    menuResBat = 0;
    menuSel = 0;
    menuSelCnt = 0;
    task = objEntry2(menuBatObj, menuBatObjDraw);
    objStdInit(task);
    p = (float *)((char *)task->work + 0x90);
    p[0] = 8.0f;
    p[2] = 292.0f;
    *(int *)&p[1] = 0;
    if (calcUPGet(unit)->flags & 0x40) {
        tbl = (int *)agwsTbl_00A3F088;
    } else {
        tbl = (int *)chaTbl_00A3F038;
    }
    tbl += 0x10;
    D_00A5A330[0].a = 0.0f;
    x = tbl[2];
    y = tbl[3];
    D_00A5A330[1].a = 0.0f;
    D_00A5A330[2].a = 0.0f;
    D_00A5A330[3].a = 0.0f;
    D_00A5A330[4].a = x;
    D_00A5A330[5].a = y;
    D_00A5A330[6].a = x;
    D_00A5A330[7].a = y;
    calcSplineInit(D_00A5A310, D_00A5A330, 4, 4);
    pMenuBat = task;
    *(ObjectTask **)((char *)task->work + 0x80) = unit;
    sndSysSePlay(7);
}

extern int menuResBat;
void objRemove(ObjectTask *task);
int sndSysSePlay(int soundId);

void menuCloseBat(void) {
    menuResBat = 0;
    objRemove(pMenuBat);
    pMenuBat = 0;
    sndSysSePlay(8);
}

int menuResultBat(void) {
    return menuResBat;
}

int menuOpenChkBat(void) {
    return pMenuBat != 0;
}

/* obj.c (ov01/tu001), still INCLUDE_ASM: returns the address of the
 * 20-byte command entry the task's work block currently selects. */
void *objCmdPtrGet(ObjectTask *task);
void menuBatObjFree(ObjectTask *task);

void menuBatObj(ObjectTask *task) {
    void *work;

    objCmdPtrGet(task);
    menuBatObjFree(task);
    /* The original keeps jal + epilogue for this last call rather than a
     * tail jump (CP-0132, config/compiler-patterns.json): a post-call read
     * blocks the compiler's sibcall lowering and is then removed as dead
     * code. */
    work = task->work;
}

typedef signed char s8;
typedef signed int s32;
s32 menuCmdExecChk(ObjectTask *, s32);

void menuBatObjFree(ObjectTask *task) {
    ObjectTask *temp_16;
    s32 temp_2;
    s32 temp_2_2;
    s8 *var_17;

    temp_16 = *(ObjectTask **)((char *)(task->work) + 0x80);
    if (calcUPGet(temp_16)->flags & 0x40) {
        var_17 = agwsTbl_00A3F088;
    } else {
        var_17 = chaTbl_00A3F038;
    }
    if (dataPadRead(1) & 0x4000) {
        temp_2 = 4;
        if (temp_2 < ++menuSel) {
            menuSel = 0;
        }
        menuSelCnt = 0;
        sndSysSePlay(3);
    } else if (dataPadRead(1) & 0x1000) {
        temp_2_2 = menuSel - 1;
        menuSel = temp_2_2;
        if (temp_2_2 < 0) {
            menuSel = 4;
        }
        menuSelCnt = 0;
        sndSysSePlay(3);
    }
    if (dataPadRead(1) & 0x20) {
        if (menuCmdExecChk(temp_16, (*(int *)((char *)(((menuSel * 0x10) + var_17)) + 0x4))) != 0) {
            menuResBat = (*(int *)((char *)(((menuSel * 0x10) + var_17)) + 0x4));
            sndSysSePlay(1);
            return;
        }
        sndSysSePlay(5);
        return;
    }
    if (dataPadRead(1) & 0x40) {
        sndSysSePlay(2);
    }
    menuResBat = 0;
}

void menuBatObjDraw(ObjectTask *task)
{
  void grGpInit();
  void grPacketSend();
  extern void grCloseSpr();
  extern void grOpenSpr();
  extern void menuTexTrans();
  extern void menuEnvMake();
  extern int menuCmdExecChk(ObjectTask *unit, int cmd);
  extern void menuStatSpriteRGB(int packet, int x, int y, int w, int h, int u, int v, unsigned char *col);
  extern void calcSpline();
  extern unsigned char D_00A46730[];
  extern int D_00A5A310[];
  extern char chaTbl_00A3F038[];
  extern char agwsTbl_00A3F088[];

  int packet[4];
  float *new_var;
  typedef struct __attribute__((packed)) { int rgba; } DrawColor;
  DrawColor col[4];
  int *p;
  char *tbl;
  float *pos;
  int i;
  int y;
  int x;
  ObjectTask *unit;
  int cx;
  int cy;
  int t;
  float fy;
  float fx;
  pos = (float *) (((char *) task->work) + 0x90);
  new_var = &pos[2];
  fx = pos[0];
  fy = *new_var;
  unit = *((ObjectTask **) (((char *) task->work) + 0x80));
  col[0] = *(DrawColor *)D_00A46730;
  x = (int) fx;
  y = (int) fy;
  tbl = (int *) ((calcUPGet(unit)->flags & 0x40) ? (agwsTbl_00A3F088) : (chaTbl_00A3F038));
  grGpInit((int) packet);
  menuTexTrans();
  menuEnvMake((int) packet);
  grOpenSpr((int) packet);
  i = 4;
  p = (int *) (tbl + 0x4C);
  do
  {
    cx = (int) pos[24];
    if (!(pos[24] < ((float) p[-1])))
    {
      cx = p[-1];
    }
    cy = (int) pos[26];
    if (!(pos[26] < ((float) p[0])))
    {
      cy = p[0];
    }
    if (menuSel == i)
    {
      menuStatSprite((int) packet, x + cx, y + cy, 0x8E, 0x20, 0, menuSelCnt << 5);
      if (menuSelCnt == 3)
      {
        if (menuCmdExecChk(unit, *((int *) ((tbl + (menuSel * 0x10)) + 4))) != 0)
        {
          menuStatSprite((int) packet, (x + cx) + 0x1B, (y + cy) + 7, 0x69, 0x14, 0x8F, (*((int *) (tbl + (menuSel * 0x10)))) * 0x14);
        }
        else
        {
          menuStatSpriteRGB((int) packet, (x + cx) + 0x1B, (y + cy) + 7, 0x69, 0x14, 0x8F, (*((int *) (tbl + (menuSel * 0x10)))) * 0x14, (unsigned char *)col);
        }
      }
      else
      {
        t = menuSelCnt + 1;
        menuSelCnt = t;
        if (t >= 4)
        {
          menuSelCnt = 3;
        }
      }
    }
    else
    {
      menuStatSprite((int) packet, x + cx, y + cy, 0x8E, 0x20, 0, 0);
    }
    i -= 1;
    p -= 4;
  }
  while (i >= 0);
  grCloseSpr();
  calcSpline(D_00A5A310, ((char *) pos) + 0x60, 2);
  grPacketSend((int) packet);
}

extern s32 calcStatGet(ObjectTask *unit, s32 kind, s32 mask);
extern int menuEtherNumChk(ObjectTask *unit);
extern int dataItmBoxChk(int id);
extern int curPosProcInit(ObjectTask *unit, int mode);
extern int cfEventGet(void);
extern int calcDblActChk(ObjectTask *unit);

s32 menuCmdExecChk(ObjectTask *arg0, s32 arg1) {
    s32 i;
    s32 n;
    s32 ret;
    short sh;

    switch (arg1) {
    case 1:
        if (calcStatGet(arg0, 1, 0x1000) == 0) {
            if (menuEtherNumChk(arg0) != 0) {
                ret = 1;
                break;
            }
        }
        ret = 0;
        break;
    case 2:
        n = 0;
        i = 1;
        do {
            n += dataItmBoxChk(i++);
        } while (i < 0x23);
        if (n != 0) {
            if (!(calcUPGet(arg0)->flags & 0x40)) {
                ret = 1;
                break;
            }
        }
        ret = 0;
        break;
    case 5:
        ret = curPosProcInit(arg0, 6) != 0;
        break;
    case 4:
        if (cfEventGet() & 4) {
            goto zero;
        }
        if (!(calcUPGet(arg0)->flags & 0x40) && (calcUPGet(arg0)->flags & 0x20)) {
            goto zero;
        }
        if ((calcUPGet(arg0)->flags & 0x40) && (calcUPGet(arg0)->linkedUnit->flags & 0x20)) {
            goto zero;
        }
        if (calcUPGet(arg0)->flags & 0x40) {
            goto one;
        }
        sh = *(short *)((char *)calcUPGet(arg0) + 0x54);
        ret = 0;
        if (sh != 0) {
            goto one;
        }
        break;
    case 6:
        ret = calcDblActChk(arg0) != 0;
        break;
    case 3:
    one:
        ret = 1;
        break;
    default:
    zero:
        ret = 0;
        break;
    }
    return ret;
}

int menuEtherNumChk(ObjectTask *unit)
{
    int count;
    int slot;
    int offset;
    int countIfPresent;
    MenuEtherStatusEntry *etherEntry;

    count = 0;
    offset = 0x80;
    for (slot = 0xB; slot >= 0; slot--) {
        etherEntry = (MenuEtherStatusEntry *)((char *)calcUPGet(unit) + offset);
        countIfPresent = count + 1;
        offset += 2;
        count = (etherEntry->etherStatus != 0) ? countIfPresent : count;
    }
    return count;
}

typedef struct MenuEthEntry {
    int name;
    int help;
    signed char disabled;
    unsigned char ep;
    short id;
} MenuEthEntry;
typedef struct MenuEthWin {
    int x;
    int y;
    int color;
    int unk0C;
    int w;
    MenuEthEntry *list;
    char *title;
} MenuEthWin;
extern MenuEthEntry ethBw[];
extern char D_00A46758[];
extern unsigned char ethWin[];
int *dataEthNameGet(int id);
unsigned char *dataEthGet(int id);
int calcTakeEpGet(ObjectTask *unit, int ep);
int calcEthEnableChk(ObjectTask *unit, int id);
void eBattleWinOpen(void *window);

void menuOpenEth(ObjectTask *unit) {
    MenuEthEntry *e;
    short *p;
    int i;
    int n;
    int id;
    signed char dis;
    int ok;

    i = 0xB;
    n = 0;
    e = ethBw;
    p = (short *)((char *)calcUPGet(unit) + 0x8E);
    do {
        id = *p;
        p++;
        if (id != 0) {
            e->name = dataEthNameGet(id)[0];
            e->help = dataEthNameGet(id)[1];
            e->ep = calcTakeEpGet(unit, *(short *)(dataEthGet(id) + 6));
            ok = calcEthEnableChk(unit, id);
            dis = 0;
            if (ok == 0) {
                dis = 1;
                id = 0;
            }
            e->id = id;
            n++;
            e->disabled = dis;
            e++;
        }
    } while (--i >= 0);
    ethBw[n].name = 0;
    ((MenuEthWin *)ethWin)->x = 0x10;
    ((MenuEthWin *)ethWin)->y = 0x6C;
    ((MenuEthWin *)ethWin)->color = 0xFFFFFD;
    ((MenuEthWin *)ethWin)->w = 0xF0;
    ((MenuEthWin *)ethWin)->title = D_00A46758;
    ((MenuEthWin *)ethWin)->list = ethBw;
    ((MenuEthWin *)ethWin)->unk0C = 0;
    eBattleWinOpen(ethWin);
    sndSysSePlay(7);
}

extern unsigned char ethWin[];
void eBattleWinClose(void *window);

void menuCloseEth(void) {
    eBattleWinClose(ethWin);
    sndSysSePlay(8);
}

unsigned short dataPadRead(int);
void eBattleWinMain(int *);

s32 menuResultEth(s32 *arg0)
{
  s16 temp_2;
  s32 temp_16;
  s32 temp_17;
  char *new_var;
  if (1)
  {
    int arg0;
    temp_16 = dataPadRead(2) & 0xF000;
    temp_17 = dataPadRead(1) & 0xFFFF0FFF;
    new_var = (char *) ((0, ethWin));
    temp_17 |= temp_16;
    *((int *) (new_var + 0x1C)) = temp_17;
    eBattleWinMain((s32 *) ethWin);
  }
  if ((*((int *) (((char *) ethWin) + 0x20))) == (-1))
  {
    *((int *) arg0) = *((int *) (((char *) ethWin) + 0x20));
  }
  else
  {
    temp_2 = *((short *) (((char *) (((*((int *) (((char *) ethWin) + 0x20))) * 0xC) + (*((int *) (((char *) ethWin) + 0x14))))) + 0xA));
    *((int *) arg0) = (s32) temp_2;
    if (temp_17 & 0x20)
    {
      if (temp_2 != 0)
      {
        sndSysSePlay(1);
      }
      else
      {
        sndSysSePlay(5);
      }
    }
    else
      if (temp_17 & 0x40)
    {
      sndSysSePlay(2);
    }
  }
  return temp_17;
}

typedef struct MenuItmEntry {
    int name;
    int help;
    signed char disabled;
    unsigned char count;
    short id;
} MenuItmEntry;
typedef struct MenuItmWin {
    int x;
    int y;
    int color;
    int unk0C;
    int w;
    MenuItmEntry *list;
    char *title;
} MenuItmWin;
extern MenuItmEntry itmBw[];
extern char D_00A46768[];
extern unsigned char itmWin[];
int dataItmBoxChk(int id);
int *dataItmNameGet(int id);
int calcItmEnableChk(ObjectTask *unit, int id);
void eBattleWinOpen(void *window);

void menuOpenItm(ObjectTask *unit) {
    MenuItmEntry *e;
    int i;
    int n;
    int cnt;
    int id;
    signed char dis;
    int ok;

    i = 1;
    e = itmBw;
    n = 0;
    calcUPGet(unit);
    do {
        id = i;
        cnt = dataItmBoxChk(i);
        if (cnt > 0) {
            e->name = dataItmNameGet(i)[0];
            e->help = dataItmNameGet(i)[1];
            e->count = cnt;
            ok = calcItmEnableChk(unit, i);
            dis = 0;
            if (ok == 0) {
                dis = 1;
                id = 0;
            }
            e->id = id;
            n++;
            e->disabled = dis;
            e++;
        }
        i++;
    } while (i < 0x24);
    itmBw[n].name = 0;
    ((MenuItmWin *)itmWin)->x = 0x10;
    ((MenuItmWin *)itmWin)->y = 0x6C;
    ((MenuItmWin *)itmWin)->color = 0xFFFFFD;
    ((MenuItmWin *)itmWin)->w = 0xF0;
    ((MenuItmWin *)itmWin)->title = D_00A46768;
    ((MenuItmWin *)itmWin)->list = itmBw;
    ((MenuItmWin *)itmWin)->unk0C = 0;
    eBattleWinOpen(itmWin);
    sndSysSePlay(7);
}

extern unsigned char itmWin[];

void menuCloseItm(void) {
    eBattleWinClose(itmWin);
    sndSysSePlay(8);
}

unsigned short dataPadRead(int);
void eBattleWinMain(int *);

s32 menuResultItm(s32 *arg0)
{
  unsigned char *new_var;
  s16 temp_2;
  s32 temp_16;
  s32 temp_17;
  temp_16 = dataPadRead(2) & 0xF000;
  temp_17 = dataPadRead(1) & 0xFFFF0FFF;
  temp_17 |= temp_16;
  if (1)
  {
    int arg0;
    new_var = itmWin;
    *((int *) (((char *) new_var) + 0x1C)) = temp_17;
  }
  eBattleWinMain((s32 *) itmWin);
  if ((*((int *) (((char *) new_var) + 0x20))) == (-1))
  {
    *((int *) arg0) = *((int *) (((char *) new_var) + 0x20));
  }
  else
  {
    temp_2 = *((short *) (((char *) (((*((int *) (((char *) new_var) + 0x20))) * 0xC) + (*((int *) (((char *) new_var) + 0x14))))) + 0xA));
    *((int *) arg0) = (s32) temp_2;
    if (temp_17 & 0x20)
    {
      if (temp_2 != 0)
      {
        sndSysSePlay(1);
      }
      else
      {
        sndSysSePlay(5);
      }
    }
    else
      if (temp_17 & 0x40)
    {
      sndSysSePlay(2);
    }
  }
  return temp_17;
}

typedef struct StockPosEnt {
    int x;
    int y;
} StockPosEnt;

extern StockPosEnt stockPosTbl[];

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct StatIconEntry {
    int flags; /* +0x00 */
    int srcX;  /* +0x04: menuStatIconPut (0x00A2A56C..0x00A2A574) */
    int srcY;  /* +0x08 */
} StatIconEntry;

typedef struct MenuRGBA {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} MenuRGBA;

typedef union MenuColor {
    unsigned int word;
    MenuRGBA rgba;
} MenuColor;

typedef struct GrSprite {
    float x0, y0, z0, w0; /* +0x00 */
    float x1, y1, z1, w1; /* +0x10 */
    MenuColor color;      /* +0x20 */
    short u0;             /* +0x24 */
    short u1;             /* +0x26 */
    short v0;             /* +0x28 */
    short v1;             /* +0x2A */
} GrSprite;

typedef struct MenuEventTimerWork {
    unsigned char unmodeled_00[0x90];
    float x; /* +0x90 */
    unsigned char unmodeled_94[4];
    float z; /* +0x98 */
} MenuEventTimerWork;

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuStatDispCreate);

extern ObjectTask *pPreObj;
extern ObjectTask *pAtbObj;
extern ObjectTask *pKeyGuideObj;
extern ObjectTask *pBoostObj;
extern ObjectTask *pCritObj;
extern ObjectTask *pStatObj[4];
extern ObjectTask *pStockObj[5];

void menuStatDispRemove(void) {
    int i;

    if (pPreObj != 0) {
        objRemove(pPreObj);
        pPreObj = 0;
    }
    if (pAtbObj != 0) {
        objRemove(pAtbObj);
        pAtbObj = 0;
    }
    if (pKeyGuideObj != 0) {
        objRemove(pKeyGuideObj);
        pKeyGuideObj = 0;
    }
    if (pBoostObj != 0) {
        objRemove(pBoostObj);
        pBoostObj = 0;
    }
    if (pCritObj != 0) {
        objRemove(pCritObj);
        pCritObj = 0;
    }
    for (i = 0; i < 5; i++) {
        if (pStockObj[i] != 0) {
            objRemove(pStockObj[i]);
            pStockObj[i] = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        if (pStatObj[i] != 0) {
            objRemove(pStatObj[i]);
            pStatObj[i] = 0;
        }
    }
    if (pMenuBat != 0) {
        menuCloseBat();
    }
    menuCloseEth();
    menuCloseItm();
}

/* 0x00a27278: `jr $31; nop`, an empty body in the original. */
void menuStatPreObj(void) {
}

void menuStatTexTrans(void);

void menuStatPreObjDraw(void) {
    menuStatTexTrans();
}

extern f32 ETRot;
extern float ETRotDist;
extern int mapHDist;
extern int mapH;
int objCmdNext(ObjectTask *task);
int objHatoVec(ObjectTask *task, int direction);
int objStdMove(ObjectTask *task);

void menuATBObj(ObjectTask *arg0)
{
  int new_var;
  int *entry;
  float step;
  float diff;
  entry = objCmdPtrGet(arg0);
  if ((*entry) != 1)
  {
    if (new_var = (*entry) == 0)
    {
      goto mapblk;
    }
    if (entry[0] == 2)
    {
      objHatoVec(arg0, 8);
      if (objStdMove(arg0))
      {
        objCmdNext(arg0);
      }
    }
    goto mapblk;
  }
  entry[1] = entry[1] - 1;
  if (entry[1] < 0)
  {
    objCmdNext(arg0);
  }
  mapblk:
  if (mapH != mapHDist)
  {
    if (mapHDist < mapH)
    {
      if (1)
      {
      }
      mapH = mapH - 8;
    }
    else
    {
      mapH = mapH + 8;
    }
    if (mapH >= 0x41)
    {
      mapH = 0x40;
    }
    if (mapH < 0x14)
    {
      mapH = 0x14;
    }
  }

  step = 0.13962634f;
  if (ETRot != ETRotDist)
  {
    ETRot = ETRot + step;
    if (ETRot > 6.2831855f)
    {
      ETRot = ETRot - 6.2831855f;
    }
    diff = ETRot - ETRotDist;
    if (diff < 0.0f)
    {
      if ((-diff) < step)
      {
        goto snap;
      }
    }
    else
      if (diff < step)
    {
      snap:
      ETRot = ETRotDist;

    }
  }
}

/* 0x00a27438: `jr $31; nop`, an empty body in the original. */
void menuKeyGuideObj(void) {
}

/* ManWork and manWorkGet come from their definer, ov01/tu003 battle_init.c,
 * through include/ov01/battle_init.h. menuKeyGuideDisp null-checks
 * unitTask, then reads a flag (bit 3) from unitTask->work, and compares
 * controlPhase against 0xb and 0x1a. */
void menuKeyGuideDisp(int packet, ObjectTask *task, int phase);

void menuKeyGuideObjDraw(int packet) {
    ObjectTask *task;
    int phase;

    task = manWorkGet()->unitTask;
    phase = manWorkGet()->controlPhase;
    menuKeyGuideDisp(packet, task, phase);
}

int menuStatIconChk(int charaId, int iconSlot);
extern int calcStatGetNorm(ObjectTask *unit, int category, int flag);

void menuStatObj(ObjectTask *arg0) {
    char *w;
    u32 *cmd;
    ObjectTask *unit;
    CalcUnitParam *up;
    f32 *p0;
    f32 *p1;
    f32 *p2;
    f32 *p3;
    f32 d0;
    f32 n0;
    f32 d1;
    f32 n1;
    f32 d2;
    f32 n2;
    f32 d3;
    f32 n3;
    s32 i;
    s32 j;
    s32 idx;
    s32 ret;
    s32 cur;
    s32 bit;

    w = (char *)arg0->work + 0x90;
    cmd = objCmdPtrGet(arg0);
    switch (cmd[0]) {
    case 0:
        break;
    case 2:
        objHatoVec(arg0, 8);
        if (objStdMove(arg0) != 0) {
            cmd[1] = 0;
            objCmdNext(arg0);
        }
        return;
    case 1:
        if ((s32)--cmd[1] < 0) {
            cmd[1] = 0;
            objCmdNext(arg0);
        }
        return;
    }
    p0 = (f32 *)(w + 0x10);
    p1 = (f32 *)(w + 0x14);
    p2 = (f32 *)(w + 0x18);
    p3 = (f32 *)(w + 0x30);
    unit = *(ObjectTask **)((char *)arg0->work + 0x80);
    up = calcUPGet(unit);

    d0 = (s8)up->unmodeled_3a[0] - *p0;
    if (d0 < 0.0f) {
        n0 = -d0;
        if (n0 < 0.5f) goto snap0;
        goto lerp0;
    }
    if (d0 < 0.5f) {
snap0:
        *p0 = (s8)up->unmodeled_3a[0];
    } else {
lerp0:
        *p0 = *p0 + ((s8)up->unmodeled_3a[0] - *p0) * 0.25f;
    }
    d1 = (s16)up->currentHp - *p1;
    if (d1 < 0.0f) {
        n1 = -d1;
        if (n1 < 0.5f) goto snap1;
        goto lerp1;
    }
    if (d1 < 0.5f) {
snap1:
        *p1 = (s16)up->currentHp;
    } else {
lerp1:
        *p1 = *p1 + ((s16)up->currentHp - *p1) * 0.125f;
    }
    d2 = (s16)up->currentEp - *p2;
    if (d2 < 0.0f) {
        n2 = -d2;
        if (n2 < 0.5f) goto snap2;
        goto lerp2;
    }
    if (d2 < 0.5f) {
snap2:
        *p2 = (s16)up->currentEp;
    } else {
lerp2:
        *p2 = *p2 + ((s16)up->currentEp - *p2) * 0.125f;
    }
    d3 = (s8)up->unmodeled_3a[3] - *p3;
    if (d3 < 0.0f) {
        n3 = -d3;
        if (n3 < 0.5f) goto snap3;
        goto lerp3;
    }
    if (d3 < 0.5f) {
snap3:
        *p3 = (s8)up->unmodeled_3a[3];
    } else {
lerp3:
        *p3 = *p3 + ((s8)up->unmodeled_3a[3] - *p3) * 0.125f;
    }
    j = 0;
    for (i = 0; i < 7; i++) {
        j |= calcStatGetNorm(unit, i, 0x7FFF);
    }
    if (j == 0) {
        *(int *)((char *)*(int *)&arg0->work + 0x78) = 7;
        *(int *)((char *)*(int *)&arg0->work + 0x7C) = 0x8000;
        return;
    }
    cur = *(int *)((char *)*(int *)&arg0->work + 0x78);
    bit = *(u16 *)((char *)*(int *)&arg0->work + 0x7C);
    if ((s32)cmd[1] <= 0 && bit != 0) {
        cmd[1] = 0x1E;
        ret = -1;
        if (menuStatIconChk(cur, calcStatGetNorm(unit, cur, bit)) == -1) {
            *(int *)((char *)*(int *)&arg0->work + 0x78) = 7;
            *(int *)((char *)*(int *)&arg0->work + 0x7C) = 0x8000;
        }
        for (i = 0; i < 8; i++) {
            idx = (cur + i) % 8;
            for (j = bit >> 1; j != 0; j >>= 1) {
                ret = menuStatIconChk(idx, calcStatGetNorm(unit, idx, j));
                if (ret != -1) {
                    *(int *)((char *)*(int *)&arg0->work + 0x78) = idx;
                    *(int *)((char *)*(int *)&arg0->work + 0x7C) = j;
                    break;
                }
            }
            if (ret != -1) break;
            bit = 0x8000;
        }
        if (ret == -1) {
            for (j = 0x4000; j != 0; j >>= 1) {
                if (menuStatIconChk(cur, calcStatGetNorm(unit, cur, j)) != -1) {
                    *(int *)((char *)*(int *)&arg0->work + 0x78) = cur;
                    *(int *)((char *)*(int *)&arg0->work + 0x7C) = j;
                    break;
                }
            }
        }
    }
    cmd[1]--;
}

extern float RingRot;

void menuBoostObj(ObjectTask *task) {
    float twoPi;
    float rot;

    objCmdPtrGet(task);
    twoPi = 6.2831855f;
    rot = RingRot - 0.034906585f;
    RingRot = rot;
    if (rot > twoPi) {
        RingRot = rot - twoPi;
    }
}

/*
 * actStockGet (ov01/tu003 battle_init.c, exact_c) returns &actStock; calcBoost's
 * own bounded view of the same record (src/ov01/calc.h CalcActStock) already
 * names the per-mode ObjectTask *slot[2] at +0x10. menuBoostObjDraw reads
 * the same two words only to test them for null, so it indexes actStockGet()
 * the same way rather than repeating a third bounded view of the record.
 */
extern int *actStockGet(void);
void menuBoostObjDrawCha(ObjectTask *task);
void menuBoostObjDrawSel(ObjectTask *task);

void menuBoostObjDraw(ObjectTask *task) {
    if (actStockGet()[4] == 0 && actStockGet()[5] == 0) {
        menuBoostObjDrawSel(task);
        return;
    }
    menuBoostObjDrawCha(task);
}

void grGpInit(int packet);
void grPacketSend(int packet);
extern void grCloseSpr(void);
extern void grOpenSpr(int packet);
void menuATBEnvMake(int packet);
void grGsRegSet(int packet, int reg, unsigned long long value);
void menuATBEnvPalSet(int packet, int palette);
void faceUVGet(ObjectTask *arg0, s32 *arg1, s32 *arg2);
extern void xglVectorMulMat(float *dst, float mtx[4][4], void *src);
extern void grRotMatrix(float dst[4][4], float src[4][4], float *rot);
extern void grOpenFT4(int packet);
extern void grCloseFT4(void);
extern void grPutFT4(int packet, void *prim, int flag);
typedef union { float f; unsigned char c[4]; } BoostColU;
typedef struct BoostFT4 {
    Vector4 vertex[4];
    unsigned char col[4];
    float u[4];
    float v[4];
} __attribute__((aligned(16))) BoostFT4;
typedef struct { BoostFT4 e[2]; } BoostFT4Pair;
extern BoostFT4Pair D_00A46780;

void menuBoostObjDrawCha(ObjectTask *task) {
    int packet[4];
    float mtx[4][4];
    float rot[4];
    float vec[4];
    BoostFT4 vtx;
    ObjectTask *sel = 0;
    BoostFT4Pair tbl = D_00A46780;
    int fw;
    int fh;

    int mode;
    int x;
    int y;
    int j;
    int n;
    int k;

    mode = 0;
    if (actStockGet()[5] != 0) {
        sel = (ObjectTask *)actStockGet()[5];
    }
    if (actStockGet()[4] != 0) {
        mode = 2;
        sel = (ObjectTask *)actStockGet()[4];
    }
    if (sel != 0) {
        grGpInit((int)packet);
        menuATBEnvMake((int)packet);
        {
            float *pos = (float *)((char *)task->work + 0x90);
            x = (int)pos[0];
            y = (int)pos[2];
        }
        grGsRegSet((int)packet, 0x42, 0x44);
        grGsRegSet((int)packet, 0x14, 0x60);
        menuATBEnvPalSet((int)packet, mode);
        xglMatrixUnit(mtx);
        rot[0] = 0;
        rot[1] = 0;
        rot[2] = RingRot;
        rot[3] = 0;
        grRotMatrix(mtx, mtx, rot);
        for (j = 0, n = 0; j < 2; j++, n += 7) {
            Vector4 *src = (Vector4 *)((char *)(n * sizeof(Vector4)) + (int)tbl.e[0].vertex);
            for (k = 0; k < 4; k++) {
                xglVectorMulMat(vec, mtx, src);
                *(BoostColU *)vtx.col = *(BoostColU *)tbl.e[j].col;
                {
                    Vector4 *dst = &vtx.vertex[k];
                    dst->x = (float)(x + 0x32) + vec[0];
                    dst->y = (float)(y - 0x16) + vec[1];
                    dst->z = 16777212.0f;
                }
                vtx.u[k] = tbl.e[j].u[k];
                vtx.v[k] = tbl.e[j].v[k];
                src++;
            }
            grOpenFT4((int)packet);
            grPutFT4((int)packet, &vtx, 0);
            grCloseFT4();
        }
        grGsRegSet((int)packet, 0x42, 0);
        grGsRegSet((int)packet, 0x14, 0);
        menuATBEnvPalSet((int)packet, 1);
        grOpenSpr((int)packet);
        faceUVGet(sel, &fw, &fh);
        menuStatSprite((int)packet, x, y, 0x5E, 0x20, 0x9A, 0x60);
        menuStatSprite((int)packet, x + 0x28, y - 0x22, 0x14, 0x18, fw, fh);
        menuStatSprite((int)packet, x + 0x26, y - 0x24, 0x18, 0x1C, 0xC0, 0x20);
        grCloseSpr();
        grPacketSend((int)packet);
    }
}

extern int unitTblGet(int side, ObjectTask ***table);
extern int calcBoostChk(ObjectTask *unit, int mode);
void menuStatSpriteRGB(int packet, int x, int y, int w, int h, int u, int v, unsigned char *col);
typedef struct { int v[3]; } BoostOfs3;
typedef struct { unsigned char c[3][4]; } BoostCol3;
extern BoostOfs3 D_00A46860;
extern BoostOfs3 D_00A46870;
extern BoostCol3 D_00A46880;

void menuBoostObjDrawSel(ObjectTask *task) {
    int packet[4];
    BoostOfs3 xo = D_00A46860;
    BoostOfs3 yo = D_00A46870;
    BoostCol3 col = D_00A46880;
    ObjectTask **units;
    int i;
    int n;
    int x;
    int y;
    int fw;
    int fh;

    int wk = *(int *)&task->work;

    if (!(*(unsigned int *)wk & 4)) {
        grGpInit((int)packet);
        menuATBEnvMake((int)packet);
        {
            float *pos = (float *)((char *)task->work + 0x90);
            x = (int)pos[0];
            y = (int)pos[2];
        }
        grOpenSpr((int)packet);
        menuStatSprite((int)packet, x, y, 0x5E, 0x20, 0x9A, 0x60);
        menuStatSprite((int)packet, x + 0x24, y - 0x14, 0x18, 0xE, 0x80, 0x20);
        i = 0;
        n = unitTblGet(0, &units);
        if (n > 0) {
            do {
                if (units[i] != 0 && calcBoostChk(units[i], 1) != 0) {
                    faceUVGet(units[i], &fw, &fh);
                    menuStatSprite((int)packet, x + xo.v[i], y + yo.v[i], 0x14, 0x18, fw, fh);
                    menuStatSpriteRGB((int)packet, x + xo.v[i] - 2, y + yo.v[i] - 4, 0x18, 0x20, 0x80, 0, col.c[i]);
                }
                i++;
            } while (i < n);
        }
        grCloseSpr();
        grPacketSend((int)packet);
    }
}

/*
 * The boost-ring display object (data still asm-owned; original ELF symbol
 * name).
 * Its work record's first word is a flag set; bit 2 (0x4) hides the object.
 */
extern ObjectTask *pBoostObj;

void menuBoostDisp(int enabled)
{
    if (enabled != 0) {
        *(int *)pBoostObj->work &= ~4; /* clear hidden bit 2 */
        return;
    }
    *(int *)pBoostObj->work |= 4; /* set hidden bit 2 */
}

void *objCmdPush(ObjectTask *task);
typedef struct MenuCritPos {
    float x;           /* +0x00 */
    float y;           /* +0x04 */
    float z;           /* +0x08 */
    unsigned char unmodeled_0C[0x54];
    float bx;          /* +0x60 */
    float by;          /* +0x64 */
    float bz;          /* +0x68 */
} MenuCritPos;
typedef struct MenuCritWork {
    int flags;                       /* +0x00 */
    unsigned char unmodeled_04[0x7C];
    void *target;                    /* +0x80 */
    unsigned char unmodeled_84[0xC];
    MenuCritPos pos;                 /* +0x90 */
} MenuCritWork;
typedef union MenuCritWord {
    int i;
    float f;
    void *p;
} MenuCritWord;

void menuCritDispOn(ObjectTask *target)
{
    MenuCritWork *work;
    MenuCritPos *pos;
    MenuCritWord *cmd;
    MenuCritWork *w;
    MenuCritWork *w2;
    float x;
    float z;
    float bx;
    int f;

    work = pCritObj->work;
    if ((work->flags & 4) || (work->target != target)) {
        cmd = objCmdPush(pCritObj);
        x = 576.0f;
        z = 310.0f;
        bx = 376.0f;
        work = pCritObj->work;
        pos = &work->pos;
        cmd->i = 1;
        w = pCritObj->work;
        *(volatile float *)&pos->bz = z;
        *(volatile void **)&w->target = target;
        *(volatile float *)&pos->x = x;
        w2 = pCritObj->work;
        *(volatile float *)&pos->bx = bx;
        f = *(volatile int *)&w2->flags;
        *(volatile int *)&pos->y = 0;
        f &= ~4;
        *(volatile float *)&pos->z = z;
        *(volatile int *)&w2->flags = f;
        *(volatile int *)&pos->by = 0;
    }
}

/*
 * The critical-indicator display object (data still asm-owned; original
 * ELF symbol name). Its work record's first word is a flag set; bit 2 (0x4) hides it.
 */
extern ObjectTask *pCritObj;

void menuCritDispOff(void)
{
    *(int *)pCritObj->work |= 4; /* set hidden bit 2 */
}

int objCmdNext(ObjectTask *task);
int objHatoVec(ObjectTask *task, int direction);
int objStdMove(ObjectTask *task);

/*
 * objCmdPtrGet's return, as far as menuCritObj reads it: `lw $3,0x0($2)`
 * loads the word at +0x00. obj.c's own view of the same 20-byte record
 * (src/ov01/obj.h ObjectCommandEntry.selector) already names that word;
 * this TU cannot redefine that tag, so it names its own bounded view of
 * the same record instead.
 */
typedef struct MenuCmdEntryView MenuCmdEntryView;
struct MenuCmdEntryView {
    int selector; /* +0x00 */
};

/*
 * The battle actor task->work points at, as far as menuCritObj reads it
 * (0x00a281f4..0x00a28200): `lw $2,0x10($16)` loads task->work, then
 * `lw $6,0x80($2)` reads one word at +0x80. ov01/tu002 unit_cmd.c's own
 * view of the same object (src/ov01/unit_cmd.h Actor) already evidences an
 * ObjectTask *targetUnit at the same offset, set by unitCmdDstSet; this TU
 * cannot redefine that tag, so it names its own bounded view of the same
 * object instead.
 */
typedef struct MenuCritActorView MenuCritActorView;
struct MenuCritActorView {
    unsigned char unmodeled_00[0x80];
    ObjectTask *targetUnit; /* +0x80 */
};

int menuCritObj(ObjectTask *task)
{
    MenuCmdEntryView *entry;
    MenuCritActorView *actor;
    int result;

    result = 1;
    entry = objCmdPtrGet(task);
    if (entry->selector == 1) {
        actor = task->work;
        objHatoVec(task, (actor->targetUnit == 0) ? 4 : 2);
        result = objStdMove(task);
        if (result != 0) {
            result = objCmdNext(task);
        }
    }
    return result;
}

extern int mapH;
void grGpInit(int packet);
void grPacketSend(int packet);
extern void grCloseSpr(void);
extern void grOpenSpr(int packet);
void menuStatIconEnvMake(int packet);

void menuCritObjDraw(ObjectTask *task)
{
    int packet[4];
    int y;
    int x;
    int n;
    int m;
    int t;

    if (!(*(int *)task->work & 4) && mapH == 0x14) {
        grGpInit((int)packet);
        menuStatIconEnvMake((int)packet);
        x = (int)(*(float *)((char *)task->work + 0x90));
        y = (int)(*(float *)((char *)task->work + 0x98));
        grOpenSpr((int)packet);
        menuStatSprite((int)packet, x, y, 0x84, 0x18, 0x7C, 0);
        n = *(int *)((char *)task->work + 0x80);
        m = n - 1;
        if (n == 0) {
            menuStatSprite((int)packet, x + 0x12, y + 4, 0x61, 0x10, 0x9B, 0x19);
        } else {
            t = m << 1;
            t += m;
            t <<= 3;
            menuStatSprite((int)packet, x + 0x12, y + 4, 0x41, 0x10, 0xB1, 0x2A);
            menuStatSprite((int)packet, x + 0x53, y + 4, 0x18, 0x10, t + 0xB1, 0x3B);
        }
        grCloseSpr();
        grPacketSend((int)packet);
    }
}

extern int selY;
extern int selX;
extern int mapH;
void grGpInit(int packet);
void grPacketSend(int packet);
void menuEventTimerDisp(ObjectTask *task, int packet);
void menuMapDisp(ObjectTask *task, int packet);
void menuMapItemDisp(ObjectTask *task, int packet);
void menuTgtLineDisp(ObjectTask *task, int packet);

void menuATBObjDraw(ObjectTask *task)
{
    int packet[4];
    int h;

    grGpInit((int)packet);
    selX = selY = 0;
    menuEventTimerDisp(task, (int)packet);
    menuMapDisp(task, (int)packet);
    h = mapH;
    if (h == 0x40) {
        menuMapItemDisp(task, (int)packet);
        if (mapH == h) {
            menuTgtLineDisp(task, (int)packet);
        }
    }
    grPacketSend((int)packet);
}

/*
 * The battle map's height-scan distance (data still asm-owned; original
 * ELF symbol name).
 */
extern int mapHDist;

void menuMapHSet(int enable)
{
    if (enable != 0) {
        mapHDist = 0x14;
        return;
    }
    mapHDist = 0x40;
}

extern unsigned char D_00A46890[16];
int curSelGrpGet(void);
void grGsRegSet(int packet, int reg, unsigned long long value);
void menuATBEnvPalSet(int packet, int palette);
void menuATBEnvMake(int packet);
void menuStatSprite2(int packet, int x, int y, int w, int h, int u0, int v0, int u1, int v1);
void menuStatSprite2RGB(int packet, int x, int y, int flip, int w, int u, int r, int u2, int g, unsigned char *col);

void menuMapDisp(ObjectTask *task, int packet)
{
    int sp20;
    int h;
    int d;
    int x;
    int y;
    int a;
    int b;
    int y23;
    int y22;
    int x17;
    int x19;
    int x20;
    int yb;
    unsigned char col[8];

    h = mapH;
    d = 0x40 - h;
    y22 = (int)(*(float *)((char *)task->work + 0x98));
    y = y22;
    x = (int)(*(float *)((char *)task->work + 0x90));
    sp20 = 0x35;
    __builtin_memcpy(col, D_00A46890, 8);
    y23 = y + 0x4C;
    y22 = y + d;
    x17 = x + 63;
    sp20 = 0x35 - d;
    menuATBEnvMake(packet);
    grGsRegSet(packet, 0x14, 0x60U);
    grGsRegSet(packet, 0x42, 0x44U);
    menuATBEnvPalSet(packet, 0);
    grOpenSpr(packet);
    menuStatSprite2(packet, x, y22, 0x40, h, 1, 1, 0x40, h);
    menuStatSprite2(packet, x17, y22, 0x40, h, 0x40, 1, 0, h);
    menuStatSprite2(packet, x, y23, 0x40, h, 1, h, 0x40, 0);
    menuStatSprite2(packet, x17, y23, 0x40, h, 0x40, h, 0, 0);
    if (curSelGrpGet() == 0) {
        a = 0;
        b = 1;
    } else {
        a = 1;
        b = 0;
    }
    x19 = x + 0x34;
    x20 = x + 0x40;
    yb = y23 + sp20;
    menuStatSprite2RGB(packet, x19, y22, 0xC, 0xA, 0, 0x40, 0xC, 0x4A, &col[a * 4]);
    menuStatSprite2RGB(packet, x20, y22, 0xC, 0xA, 0xC, 0x40, 0, 0x4A, &col[a * 4]);
    menuStatSprite2RGB(packet, x19, yb, 0xC, 0xA, 0, 0x4B, 0xC, 0x55, &col[b * 4]);
    menuStatSprite2RGB(packet, x20, yb, 0xC, 0xA, 0xC, 0x4B, 0, 0x55, &col[b * 4]);
    grCloseSpr();
}

/*
 * ov01/tu008 src/ov01/gr_gp_init.c, still asm there; forward-declared here
 * until that TU publishes its header.
 */
extern void grCloseSpr(void);
extern void grGsRegSet(int packet, int reg, unsigned long long value);
extern void grOpenSpr(int packet);

void menuATBEnvPalSet(int packet, int palette);
void menuMapItemDispSub(ObjectTask *task, int packet, int index, int x, int z);

/*
 * task->work's own float pair at these offsets is the map item's world
 * position; only x and z feed this draw call.
 */
#define MAP_ITEM_POS_X_OFF 0x90
#define MAP_ITEM_POS_Z_OFF 0x98

void menuMapItemDisp(ObjectTask *task, int packet)
{
    void *work;
    int x;
    int z;
    int zFar;

    work = task->work;
    z = (int)(*(float *)((char *)work + MAP_ITEM_POS_Z_OFF));
    x = (int)(*(float *)((char *)work + MAP_ITEM_POS_X_OFF));
    grGsRegSet(packet, 0x42, 0);
    zFar = z + 0x4C;
    grGsRegSet(packet, 0x14, 0);
    menuATBEnvPalSet(packet, 1);
    grOpenSpr(packet);
    menuMapItemDispSub(task, packet, 0, x, zFar);
    menuMapItemDispSub(task, packet, 1, x, z);
    grCloseSpr();
}

extern int menuNumPut(int, int, int, int, int, int);
extern BattleUnit *tgtUnitGet(void);
extern int calcLineChk(ObjectTask *unit);
extern int calcSkillEquChkAll(int id);
extern void menuSelLine(void *packet, int x0, int y, int x1, int y1, int sel);
extern void menuStatEnvMake(int packet);

void menuTgtLineDisp(ObjectTask *task, int packet) {
    int var_19;
    BattleUnit *temp_2;
    s16 temp_17;
    s32 temp_16;
    s32 temp_20;
    s32 var_10;
    s32 var_16;
    s32 var_9;
    u16 temp_3_2;
    void *temp_3;

    temp_3 = task->work;
    {
        float fx = *(float *)((char *)(temp_3) + 0x90);
        float fy = *(float *)((char *)(temp_3) + 0x98);
        temp_20 = (s32) fx;
        temp_16 = (s32) fy;
    }
    if ((selX != 0) && (selY != 0)) {
        temp_2 = tgtUnitGet();
        menuSelLine((void *) packet, temp_20 + 0x80, temp_16 - 4, selX + 8, selY + 8, calcLineChk((ObjectTask *)temp_2) == 0);
        if (!(*(int *)temp_2->work & 0x40)) {
        } else {
            grOpenSpr(packet);
            temp_3_2 = calcUPGet((ObjectTask *)temp_2)->flags;
            if (temp_3_2 & 0x4000) {
                var_9 = 0xF0;
                var_10 = 0x20;
            } else if (temp_3_2 & 0x2000) {
                var_9 = 0xE0;
                var_10 = 0x30;
            } else {
                var_9 = 0xF0;
                var_10 = 0x30;
            }
            menuStatSprite(packet, temp_20 + 0x70, temp_16 - 0x12, 0x10, 0x10, var_9, var_10);
            grCloseSpr();
        }
        temp_17 = calcUPGet((ObjectTask *)temp_2)->currentHp;
        if ((temp_17 >= 0x2710) || (var_19 = 1, (calcSkillEquChkAll(0x41) == 0))) {
            var_19 = 3;
        }
        menuStatEnvMake(packet);
        grOpenSpr(packet);
        menuNumPut(packet, temp_20 + 0x48, temp_16 - 0x12, temp_17, 4, var_19);
        grCloseSpr();
    }
}

void menuStockStart(void)
{
  ObjectTask **new_var;
  char *w;
  float *p;
  int i;
  ObjectTask **pp;
  ObjectTask *task;
  int *cmd;
  int *tp;
  float fy;
  float fx;
  char *wx;
  char *wo;
  char *wn;
  new_var = pStockObj;
  tp = (int *) stockPosTbl;
  pp = new_var;
  for (i = 0; i < 5; i++)
  {
    task = *pp;
    fx = (!task) && (!task);
    if (fx && (!task))
    {
    }
    w = (char *) task->work;
    pp++;
    cmd = (int *) objCmdTailGet((void *) task);
    w += 0x90;
    p = (float *) w;
    *((void * volatile *) (((char *) task->work) + 0x80)) = (void *) i;
    *((volatile int *) cmd) = 2;
    *((volatile int *) (w + 0x4)) = 0;
    fx = (float) tp[0];
    fy = (float) tp[1];
    tp += 2;
    wx = *((char * volatile *) (&task->work));
    p[0] = fx;
    *((int *) (wx + 0x78)) = 4;
    p[2] = fy;
    if (4 <= i)
    {
      wo = *((char * volatile *) (&task->work));
      *((int *) wo) = (*((int *) wo)) | 4;
    }
    else
    {
      wn = *((char * volatile *) (&task->work));
      *((int *) wn) = (*((int *) wn)) & (~4);
    }
  }

  if (fx && fx)
  {
    if (fy)
    {
    }
  }
}

void menuStockNext(void) {
    int i;
    ObjectTask *task;
    char *work;
    char *w;
    int *cmd;
    int n;

    for (i = 0; i < 4; i++) {
        task = pStockObj[i];
        w = (char *)task->work + 0x90;
        cmd = (int *)objCmdTailGet((void *)task);
        n = *(int *)((char *)task->work + 0x80);
        if (n <= 0) {
            *(int *)((char *)task->work + 0x80) = 0;
            work = *(char * volatile *)&task->work;
            *(int *)work = *(int *)work | 4;
        } else {
            n--;
            *(float *)(w + 0x64) = 0.0f;
            *(int *)((char *)task->work + 0x80) = n;
            *cmd = 1;
            *(float *)(w + 0x60) = (float)stockPosTbl[n].x;
            *(float *)(w + 0x68) = (float)stockPosTbl[n].y;
        }
    }
}

typedef struct StockPos {
    float mx;
    float mz;
    float my;
    unsigned char unk0C[0x54];
    float x;
    float z;
    float y;
} StockPos;

typedef struct StockWork {
    int flags;
    unsigned char unk04[0x78];
    int id;
    int no;
    unsigned char unk84[0xC];
    StockPos pos;
} StockWork;

typedef struct StockSlots {
    ObjectTask *slot[1];
} StockSlots;

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuStockBoost);

extern char D_00A46898[];
extern int printf(const char *format, ...);

void menuStockDead(int no) {
    int i;
    int n;
    StockPos *pos;
    StockWork *work;
    int *cmd;
    ObjectTask *task;

    if (((StockSlots *)actStockGet())->slot[((StockWork *)pStockObj[no]->work)->id] == 0) {
        for (i = no + 1; i < 4; i++) {
            task = pStockObj[i];
            cmd = (int *)objCmdTailGet((void *)task);
            work = task->work;
            pos = &work->pos;
            n = work->no;
            if (n > 0) {
                n--;
                pos->z = 0.0f;
                work->no = n;
                *cmd = 1;
                pos->x = stockPosTbl[n].x;
                pos->y = stockPosTbl[n].y;
                printf(D_00A46898, no, i);
            }
        }
    }
}

void menuStockObj(ObjectTask *arg0) {
    s32 temp_2;
    u32 temp_3;
    void *temp_4;

    temp_3 = *(u32 *)objCmdPtrGet(arg0);
    switch (temp_3) {
    case 1:
        objHatoVec(arg0, 2);
        if (objStdMove(arg0) != 0) {
            objCmdNext(arg0);
        }
        break;
    case 0:
        break;
    case 2:
        break;
    case 3:
        if (*(s32 *)((char *)(arg0->work) + 0x70) != 0) {
            objHatoVec(arg0, 2);
            if (objStdMove(arg0) != 0) {
                objCmdNext(arg0);
            }
        }
        break;
    }
    temp_4 = arg0->work;
    temp_2 = *(s32 *)((char *)temp_4 + 0x78);
    ((union { s32 i; void *p; } *)((char *)temp_4 + 0x78))->i = (temp_2 < 0x14) ? (temp_2 + 4) : 0x14;
    temp_4 = arg0->work;
    ((union { s32 i; void *p; } *)((char *)temp_4 + 0x70))->i = *(s32 *)((char *)temp_4 + 0x70) + 1;
}

int frameCntGet(void);

void menuStockObjDraw(ObjectTask *task)
{
  int packet[4];
  unsigned char col[4];
  int sp30;
  int sp34;
  int *st;
  void *v;
  int mode;
  int n3;
  int x;
  int y;
  int w;
  int ph;
  signed char t;
  int fr;
  int idx;
  int ay;
  int ax;
  char *work;
  int flip;
  int four;
  int lim;
  four = 4;
  memset(col, 0, 4);
  col[3] = 0x80;
  if (!((*((unsigned int *) task->work)) & 4))
  {
    st = actStockGet();
    n3 = st[6];
    mode = *((int *) (((char *) task->work) + 0x80));
    idx = *((int *) (((char *) task->work) + 0x7C));
    lim = (n3 - st[7]) + 1;
    if (((n3 != 0) && ((mode < lim) || (mode == four))) && ((mode != 0) || (st[9] != 2)))
    {
      if (mode == four)
      {
        if (st[9] != 2)
        {
          return;
        }
        v = (void *) st[8];
      }
      else
      {
        v = (void *) ((struct 
        {
          typeof(st[0]) a[1];
        } *) st)->a[idx];
      }
      if ((v != 0) && (((*((long long *) (*((char **) (((char *) v) + 0x10))))) & 0x81) == 1))
      {
        work = pAtbObj->work;
        ay = (int) (((float) (ay = (int) (*((float *) (((char *) task->work) + 0x90))))) + (*((float *) (work + 0x90))));
        ax = (int) (((float) (ax = (int) (*((float *) (((char *) task->work) + 0x98))))) + (*((float *) (work + 0x98))));
        grGpInit((int) packet);
        menuATBEnvMake((int) packet);
        faceUVGet(v, &sp30, &sp34);
        flip = ((((int) v) ^ ((int) tgtUnitGet())) != 0) ? (0) : (-4);
        w = *((int *) (((char *) task->work) + 0x78));
        ph = 0xA - (w / 2);
        grOpenSpr((int) packet);
        y = ay + ((int) ph);
        flip = (x = ax + flip);
        menuStatSprite2((int) packet, y, x, w, 0x18, sp30, sp34, sp30 + 0x14, sp34 + 0x18);
        if ((mode == 0) || (mode == 4))
        {
          fr = (frameCntGet() * 2) & 0x7F;
          if (fr < 0x40)
          {
            t = fr - 0x80;
          }
          else
          {
            ;
            fr = 0x7F - fr;
          }
          col[2] = fr - 0x80;
          col[1] = fr - 0x80;
          col[0] = fr - 0x80;
          menuStatSprite2RGB((int) packet, y - 4, x - 4, w + 8, 0x20, 0xD8, 0x40, 0xF4, 0x61, col);
        }
        else
        {
          menuStatSprite2((int) packet, y - 2, x - 2, w + 4, 0x1C, 0xC0, 0x20, 0xD9, 0x3D);
        }
        grCloseSpr();
        grPacketSend((int) packet);
      }
    }
  }
}

void faceUVGet(ObjectTask *arg0, s32 *arg1, s32 *arg2) {
    s32 temp_2;
    CalcUnitParam *temp_2_2;
    u16 temp_3;

    temp_2 = menuStatChrIdGet(arg0);
    if (temp_2 < 0x20) {
        *arg1 = ((temp_2 % 3) * 0x15) + 0x40;
        *arg2 = (temp_2 / 3) * 0x19;
        return;
    }
    temp_2_2 = calcUPGet(arg0);
    *arg2 = 0;
    temp_3 = temp_2_2->flags;
    if (temp_3 & 0x4000) {
        *arg1 = 0xC0;
    } else if (temp_3 & 0x2000) {
        *arg1 = 0xD5;
    } else {
        *arg1 = 0xEA;
    }
}

extern unsigned char D_00A468B0[12];
extern int unitTblGet(int side, ObjectTask ***table);
extern int calcStatGetNorm(ObjectTask *unit, int category, int flag);
extern float *dataPosTblGet(int index);
void menuStatSpriteRGB(int packet, int x, int y, int w, int h, int u, int v, unsigned char *col);

void menuMapItemDispSub(ObjectTask *task, int packet, int index, int x, int z)
{
    unsigned char col[12];
    ObjectTask **tbl;
    ObjectTask *unit;
    int n;
    int i;
    int tmp;
    int id;
    int c;
    int pos;
    int sprite;
    int px;
    int py;
    int dy;

    __builtin_memcpy(col, D_00A468B0, 12);
    n = unitTblGet(index, &tbl);
    for (i = 0; i < n; i++) {
        unit = tbl[i];
        if (unit == 0) {
            continue;
        }
        if (*(int *)unit->work & 0x80) {
            continue;
        }
        id = *(short *)((char *)calcUPGet(unit) + 0x44);
        if (calcStatGetNorm(tbl[i], 2, 0x100) != 0 || calcStatGetNorm(tbl[i], 2, 0x80) != 0
            || calcStatGetNorm(tbl[i], 6, 0x4000) != 0 || calcStatGetNorm(tbl[i], 6, 0x2000) != 0) {
            c = 1;
        } else if (calcStatGetNorm(tbl[i], 0, 0x1000) != 0 || calcStatGetNorm(tbl[i], 4, 0x800) != 0) {
            c = 2;
        } else {
            c = 0;
        }
        if (index == 0) {
            tmp = (int)(-dataPosTblGet(id)[2] * 128.0f / 12.0f + 64.0f - 8.0f);
            dy = (int)(dataPosTblGet(id)[0] * 64.0f * 0.125f - 8.0f);
            py = z + dy;
            px = x + tmp;
            menuStatSpriteRGB(packet, px, py, 0x10, 0x10, 0xC0, 0x3E, &col[c * 4]);
        } else {
            if (id < 0) {
                id = -id;
            }
            pos = 0x1C;
            if (id != 0x23) {
                pos = id;
            }
            sprite = pos;
            if (pos == 0x20 || pos == 0x22) {
                sprite = 0x1E;
            } else if (pos == 0x1F || pos == 0x21) {
                sprite = 0x1D;
            }
            tmp = (int)(-dataPosTblGet(sprite)[2] * 128.0f / 14.0f + 64.0f - 8.0f);
            dy = (int)(64.0f - dataPosTblGet(pos)[0] * 64.0f * 0.0625f - 8.0f);
            py = z + dy;
            px = x + tmp;
            menuStatSprite2RGB(packet, px, py, 0x10, 0x10, 0xC0, 0x4E, 0xD0, 0x3E, &col[c * 4]);
        }
        if (tbl[i] == (ObjectTask *)tgtUnitGet()) {
            int *xs = &selX;
            int *ys = &selY;

            *ys = py;
            *xs = px;
        }
    }
}

typedef struct SelF2Point {
    float x;
    float y;
    float z;
    float w;
} SelF2Point;
typedef struct SelF2Line {
    SelF2Point p0;
    SelF2Point p1;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} SelF2Line;
extern void grOpenF2(void *packet);
extern void grPutF2(void *packet, void *vertex, int flags);
extern void grCloseF2(void);

void menuSelLine(void *packet, int x0, int y, int x1, int y1, int sel)
{
    SelF2Line line;
    int zi = 0xFFFFFC;
    float z;

    line.a = 0x80;
    if (sel == 0) {
        line.b = 0x80;
        line.r = 0;
        line.g = 0xFF;
    } else {
        line.b = 0x80;
        line.g = 0;
        line.r = 0xF0;
    }
    grOpenF2(packet);
    z = zi;
    line.p0.x = (float)x0;
    line.p0.y = (float)y;
    line.p0.z = z;
    line.p1.x = (float)(x0 - 58);
    line.p1.y = (float)y;
    line.p1.z = z;
    grPutF2(packet, &line, 0);
    line.p0.x = line.p1.x;
    line.p0.y = line.p1.y;
    line.p0.z = z;
    line.p1.x = (float)x1;
    line.p1.y = (float)y1;
    line.p1.z = z;
    grPutF2(packet, &line, 0);
    grCloseF2();
}

void menuATBEnvMake(int packet);
void menuEventTimerDispSub(ObjectTask *task, int packet);

void menuEventTimerDisp(ObjectTask *task, int packet)
{
    void *work;
    int x;
    int y;

    work = task->work;
    x = (int)(*(float *)((char *)work + 0x90));
    y = (int)(*(float *)((char *)work + 0x98));
    menuATBEnvMake(packet);
    menuEventTimerDispSub(task, packet);
    y += 0x38;
    x -= 0x18;
    grOpenSpr(packet);
    menuStatSprite(packet, x, y, 0x18, 0x4E, 0x80, 0x32);
    grCloseSpr();
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuEventTimerDispSub);

extern int rnd(int max);
extern int eventTimer;
extern int eventTimerX;
extern float ETRotDist;

void menuTimeSet(int time) {
    int r;

    eventTimer = time;
    r = rnd(100);
    if (r >= 0x60) {
        eventTimerX = 10;
    } else if (r >= 0x3D) {
        eventTimerX = 4;
    } else {
        eventTimerX = 2;
    }
    ETRotDist = (float)eventTimer * 1.5707964f;
    if (ETRotDist > 6.2831855f) {
        ETRotDist = ETRotDist - 6.2831855f;
    }
}

/* menuTimeSet (this TU, still asm) applies a new menu time setting. */
void menuTimeSet(int time);
extern int eventTimer;

int menuTimeNext(void) {
    eventTimer++;
    if (eventTimer >= 4) {
        eventTimer = 0;
    }
    menuTimeSet(eventTimer);
    return eventTimer;
}

int menuTimeGet(void) {
    return eventTimer;
}

extern int eventTimerX;

int menuTimeXGet(void) {
    return eventTimerX;
}

extern BattleUnit *actUnitGet(void);
void grGpInit(int packet);
void grPacketSend(int packet);
void menuPlStatPut(ObjectTask *task, int packet, BattleUnit *unit);
void menuStatNamePut(ObjectTask *task);

/*
 * The status-icon task's own work record, as far as menuStatObjDraw and
 * menuStatNamePut read it. menuStatNamePut (0x00a2a5c0..0x00a2a5c8) loads
 * `lw $17,0x78($2)` / `lw $5,0x7c($2)` from task->work and passes them to
 * menuStatIconChk/dataStatNameGet as the character id and the icon slot to
 * check; menuStatObjDraw (0x00a29bf4..0x00a29c00) loads `lw $17,0x80($2)`
 * from the same block and compares it against actUnitGet()'s BattleUnit *,
 * showing the name popup only for the currently active unit.
 */
typedef struct MenuStatObjWork MenuStatObjWork;
struct MenuStatObjWork {
    unsigned char unmodeled_00[0x78];
    int charaId;       /* +0x78 */
    int iconSlot;       /* +0x7C */
    BattleUnit *unit;   /* +0x80 */
};

void menuStatObjDraw(ObjectTask *task) {
    int packet;
    MenuStatObjWork *work;
    BattleUnit *unit;

    grGpInit((int)&packet);
    work = task->work;
    unit = work->unit;
    menuPlStatPut(task, (int)&packet, unit);
    grPacketSend((int)&packet);
    if (unit == actUnitGet()) {
        menuStatNamePut(task);
    }
}

#include "ov01/calc.h"

/* calcUPGet's return type comes from its definer, ov01/tu004 calc.c
 * (published include/ov01/calc.h); calcUPGet itself is still asm there, so
 * this TU declares it the same way ov01/tu002 unit_cmd.c and ov01/tu013
 * snd.c do. */
extern CalcUnitParam *calcUPGet(ObjectTask *unit);

/*
 * unitLoad and unitActupdate test this bit of the battle actor's flags word
 * to tell an enemy-controlled unit from a player-controlled one
 * (src/ov01/unit_cmd.h documents it as ACTOR_FLAG_ENEMY); ov01/tu013 snd.c's
 * sndConvSe already reuses the same evidenced mask off calcUPGet(unit)->flags.
 */
#define ACTOR_FLAG_ENEMY 0x40u

/* An enemy unit, or one whose charaId falls in the boss range ov01/tu013
 * snd.c's sndConvSe documents (0xBB..0xC2), reports the linked unit's
 * charaId instead of its own. */
int menuStatChrIdGet(ObjectTask *unit)
{
    int charaId;
    CalcUnitParam *up;

    up = calcUPGet(unit);
    charaId = up->charaId;
    if (!(calcUPGet(unit)->flags & ACTOR_FLAG_ENEMY)) {
        if (calcUPGet(unit)->charaId >= 0xBB) {
            if (calcUPGet(unit)->charaId < 0xC3) {
                charaId = up->linkedUnit->charaId;
            }
        }
    } else {
        charaId = up->linkedUnit->charaId;
    }
    return charaId - 1;
}

void menuApBarPut();
void menuFaceEnvMake(int packet);
int frameCntGet(void);
int calcIdol2Chk(ObjectTask *unit);
int menuBpBarPut(int, int, int, int);
void menuStatIconPut(int packet, int x, int y, int charaId, int iconSlot);

void menuPlStatPut(ObjectTask *task, int packet, BattleUnit *unit)
{
    CalcUnitParam *up;
    int charaId;
    int iconSlot;
    float *pA0;
    float *pA8;
    float *pC0;
    float *pA4;
    char *work;
    int x, y, chrId, frame, idol;
    int cx, cy;

    up = calcUPGet((ObjectTask *)unit);
    work = (char *)task->work;
    pA4 = (float *)(work + 0xA4);
    y = (int)*(float *)(work + 0x98);
    pA8 = (float *)(work + 0xA8);
    x = (int)*(float *)(work + 0x90);
    pC0 = (float *)(work + 0xC0);
    charaId = *(int *)(work + 0x78);
    x = x - 8;
    iconSlot = *(int *)(work + 0x7C);
    pA0 = (float *)(work + 0xA0);
    chrId = menuStatChrIdGet((ObjectTask *)unit);
    cx = (chrId % 6) * 0x28;
    cy = (chrId / 6) * 0x50;
    menuFaceEnvMake(packet);
    frame = (frameCntGet() * 2) & 0xFF;
    if (frame >= 0x80) {
        frame = 0xFF - frame;
    }
    grGsRegSet(packet, 0x42, ((unsigned long long)(frame + 0xA) << 32) | 0x68);
    grOpenSpr(packet);
    if (actUnitGet() == unit) {
        menuStatSprite2(packet, x + 0xC, y - 0xC, 0xA0, 0x58, 0xC8, 0xA0, 0xFF, 0xF0);
    }
    grCloseSpr();
    grGsRegSet(packet, 0x42, 0ULL);
    grOpenSpr(packet);
    menuStatSprite(packet, x + 0x10, y - 8, 0x28, 0x50, cx, cy);
    grCloseSpr();
    menuStatEnvMake(packet);
    grOpenSpr(packet);
    menuStatSprite(packet, x + 0x30, y, 18, 12, 0, 0xAE);
    menuStatSprite(packet, x + 0x30, y + 0x10, 0x12, 0xC, 0x12, 0xAE);
    menuStatSprite(packet, x + 0x30, y + 0x22, 0x12, 0xC, 0x24, 0xAE);
    menuStatSprite(packet, x + 0x30, y + 0x32, 0x12, 0xC, 0x36, 0xAE);
    idol = calcIdol2Chk((ObjectTask *)unit) ? 2 : 0;
    menuNumPut(packet, x + 0x42, y, (int)*pA4, 4, idol);
    menuStatSprite(packet, x + 0x76, y + 2, 8, 0x10, 0x7A, 0x90);
    menuNumPut(packet, x + 0x7E, y + 2, *(short *)up, 4, 1);
    menuNumPut(packet, x + 0x5A, y + 0x11, (int)*pA8, 2, 0);
    menuStatSprite(packet, x + 0x76, y + 0x13, 8, 0x10, 0x7A, 0x90);
    menuNumPut(packet, x + 0x7E, y + 0x13, up->maxEp, 2, 1);
    menuApBarPut(packet, x + 0x42, y + 0x22, (int)*pA0, unit);
    menuStatSprite(packet, x + 0x44, y + 0x30, 0x3C, 0x10, 0x30, 0xF0);
    menuNumPut(packet, x + 0x82, y + 0x30, (signed char)up->unmodeled_3a[2], 1, 1);
    grCloseSpr();
    menuBpBarPut(packet, x, y, (int)*pC0);
    if (iconSlot != 0) {
        menuStatIconEnvMake(packet);
        grOpenSpr(packet);
        menuStatIconPut(packet, x, y, charaId, iconSlot);
        grCloseSpr();
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/menu", menuNumPut);

void menuApBarPut(int packet, int x, int y, int amount)
{
    int i;
    int xPosition;

    xPosition = x;
    for (i = 0; i < 6; i++) {
        int sourceX;

        sourceX = 24;
        if (amount > 0) {
            sourceX = 0;
        }
        menuStatSprite(packet, xPosition, y, 24, 16, sourceX, 240);
        amount--;
        xPosition += 16;
    }
}

typedef struct BpCol { unsigned char r, g, b, a; } BpCol;
typedef struct BpPrim { Vector4 vertex[4]; BpCol col; } BpPrim;
extern int grOpenF4(void *packet);
extern int grPutF4(void *packet, void *prim, void *matrix);
extern int grCloseF4(void);

int menuBpBarPut(int packet, int x, int y, int amount) {
    BpPrim p;

    grOpenF4((void *)packet);
    p.col.r = 0xFF;
    p.col.g = 0x20;
    p.col.b = 0x80;
    p.col.a = 0x80;
    p.vertex[0].x = x + 0x49;
    p.vertex[0].y = y + 0x36;
    p.vertex[0].z = 16777212.0f;
    p.vertex[1].x = x + amount + 0x49;
    p.vertex[1].y = y + 0x36;
    p.vertex[1].z = 16777212.0f;
    p.vertex[2].x = x + 0x49;
    p.vertex[2].y = y + 0x3A;
    p.vertex[2].z = 16777212.0f;
    p.vertex[3].x = x + amount + 0x49;
    p.vertex[3].y = y + 0x3A;
    p.vertex[3].z = 16777212.0f;
    grPutF4((void *)packet, &p, 0);
    grCloseF4();
}

typedef struct StatTblEnt { int mask; int pad[2]; } StatTblEnt;
extern StatTblEnt *statTbl[];

int menuStatIconChk(int charaId, int iconSlot) {
    StatTblEnt *p = statTbl[charaId];
    int i;

    if (p == 0 || iconSlot == 0) {
        return -1;
    }
    for (i = 0; p[i].mask != 0; i++) {
        if (p[i].mask & iconSlot) {
            return i;
        }
    }
    return -1;
}

void menuStatIconPut(int packet, int x, int y, int charaId, int iconSlot)
{
    StatTblEnt *tbl = statTbl[charaId];
    int icon = menuStatIconChk(charaId, iconSlot);

    if (icon != -1) {
        menuStatSprite(packet, x + 0x90, y + 0x32, 0x16, 0x18, tbl[icon].pad[0], tbl[icon].pad[1]);
    }
}

/*
 * eBattleWinOpen2's own argument words (src/main/e_battle_win_open.c, still
 * asm there, no published parameter type). The header shape (textId/
 * speaker/flags/priority/target) and the flags constant 0x00FFFFFD match
 * src/ov01/cmd.c's BattleMsgOpenParams, the same window family's argument
 * record for the sibling eBattleWinOpen4; only flags and target are
 * confirmed for this callee, the textId/speaker/priority names are carried
 * over from that sibling record's field order. This call adds one more
 * word, an icon graphic table pointer, that eBattleWinOpen4 does not take.
 */
struct StatNameOpenParams {
    int textId;                /* +0x00: 8 at this call site */
    int speaker;                /* +0x04: 0x68 at this call site */
    int flags;                   /* +0x08: 0x00FFFFFD */
    int priority;                 /* +0x0C: 0x800000B0 at this call site */
    int target;                    /* +0x10: dataStatNameGet's resolved name id */
    unsigned char *iconGraphic;    /* +0x14: D_00A468E8 */
};
int *dataStatNameGet(int charaId, int iconSlot);
void eBattleWinOpen2(struct StatNameOpenParams *params);
int menuStatIconChk(int charaId, int iconSlot);
extern unsigned char D_00A468E8[];
extern void eBattleWinMain2(void);

void menuStatNamePut(ObjectTask *task)
{
    struct StatNameOpenParams params;
    MenuStatObjWork *work;
    int charaId;
    int icon;

    work = task->work;
    charaId = work->charaId;
    icon = menuStatIconChk(charaId, work->iconSlot);
    if (icon != -1 && menuOpenChkBat() != 0) {
        params.textId = 8;
        params.speaker = 0x68;
        params.flags = 0xFFFFFD;
        params.priority = 0x800000B0;
        params.target = *dataStatNameGet(charaId, icon);
        params.iconGraphic = D_00A468E8;
        eBattleWinOpen2(&params);
        eBattleWinMain2();
    }
}

typedef struct { int id; int cmd; } KeyGuideRow;
typedef struct { KeyGuideRow *rows; int *list; } KeyGuideEnt;
extern KeyGuideEnt keyGuideTblChr[];
extern KeyGuideEnt keyGuideTblAgws[];
extern int menuChk(void);
extern int calcTuigekiChk(ObjectTask *unit);
extern int keyCmdChk(ObjectTask *unit, BattleUnit *tgt, int cmd);
extern int battleCtrlHelpGet(void);
void keyGideDisp(int packet, int x, int y, int id);
void menuKeyGuideHelp(int packet, ObjectTask *task, int cmd, int x, int y);
void menuKeyGuideVal(int packet, ObjectTask *task, int cmd, int x, int y);

void menuKeyGuideDisp(int packet, ObjectTask *task, int phase)
{
    int gp[4];
    KeyGuideRow *rows;
    int *list;
    BattleUnit *tgt;
    char *t;
    int x;
    int y;
    int k;
    int id;
    int i;

    if (task != 0 && (*(int *)task->work & 8) && menuChk() == 0 && phase != 0x1A && (phase < 0xB || calcTuigekiChk(task) != 0)) {
        t = (char *)*(int *)(packet + 0x10);
        x = (int)*(float *)(t + 0x90);
        y = (int)*(float *)(t + 0x98);
        grGpInit((int)gp);
        menuStatEnvMake((int)gp);
        tgt = tgtUnitGet();
        if (calcUPGet(task)->flags & 0x40) {
            rows = keyGuideTblAgws[phase].rows;
            list = keyGuideTblAgws[phase].list;
        } else {
            rows = keyGuideTblChr[phase].rows;
            list = keyGuideTblChr[phase].list;
        }
        if (list != 0) {
            for (i = 0; list[i] != 0; i++) {
                keyGideDisp((int)gp, x, y, list[i]);
                x += 0x18;
            }
        }
        if (rows != 0) {
            for (i = 0; rows[i].id != 0; i++) {
                if (rows[i].cmd == 0 || rows[i].cmd == 0x1C) {
                    k = 0;
                } else {
                    k = keyCmdChk(task, tgt, rows[i].cmd);
                }
                if (k < 2) {
                    if (k >= 0) {
                        if (k == 0) {
                            id = rows[i].id;
                        } else {
                            id = rows[i].id + 4;
                        }
                        keyGideDisp((int)gp, x, y, id);
                        if (battleCtrlHelpGet() != 2) {
                            menuKeyGuideHelp((int)gp, task, rows[i].cmd, x, y);
                            menuKeyGuideVal((int)gp, task, rows[i].cmd, x, y);
                        }
                        x += 4;
                        y += 0x1C;
                    }
                }
            }
        }
        grOpenSpr((int)gp);
        menuStatSprite((int)gp, 8, 0x1A8, 0xB2, 0xB, 0x4C, 0xAF);
        grCloseSpr();
        grPacketSend((int)gp);
    }
}

typedef struct { int u, v, w, h; } KeyGideUV;
extern KeyGideUV uvTbl[];

void keyGideDisp(int packet, int x, int y, int id)
{
    grOpenSpr(packet);
    id--;
    menuStatSprite(packet, x, y, uvTbl[id].w, uvTbl[id].h, uvTbl[id].u, uvTbl[id].v);
    grCloseSpr();
}

typedef struct { unsigned char c[3][4]; } GuideFrameCol;
extern GuideFrameCol D_00A468F0;
void menuStatSprite2RGB(int packet, int x, int y, int flip, int w, int u, int r, int u2, int g, unsigned char *col);

void menuGuideFrameDisp(int packet, int x, int y, int h, int sel)
{
    GuideFrameCol col;

    col = D_00A468F0;
    grGsRegSet(packet, 0x42, 0x2000000029ULL);
    grOpenSpr(packet);
    menuStatSprite2RGB(packet, x, y, h, 0xC, 0x84, 0xC4, 0xBC, 0xCF, col.c[sel]);
    menuStatSprite2RGB(packet, x + h, y, h + 1, 0xC, 0xBC, 0xC4, 0x84, 0xCF, col.c[sel]);
    menuStatSprite2RGB(packet, x, y + 0xC, h, 0xC, 0x84, 0xCF, 0xBC, 0xC4, col.c[sel]);
    menuStatSprite2RGB(packet, x + h, y + 0xC, h + 1, 0xC, 0xBC, 0xCF, 0x84, 0xC4, col.c[sel]);
    grCloseSpr();
}

extern const char **dataEnemyNameGet(int index);
extern const char **dataPlayerNameGet(int id);
extern void menuFontEnvMake(int packet);
extern void menuFontPut(int packet, int x, int y, signed char *text);
extern void menuGuideFrameDisp(int packet, int x, int y, int h, int sel);
static inline char *monsNameOf(ObjectTask *unit, int isEn)
{
    if (isEn == 1) {
        return (char *)*dataEnemyNameGet(calcUPGet(unit)->charaId);
    }
    return (char *)*dataPlayerNameGet(calcUPGet(unit)->charaId);
}

void monsNameDisp(int packet, ObjectTask *unit, int x, int y, int isEn)
{
    char *text;
    int ox;
    int oy;
    menuStatEnvMake(packet);
    menuGuideFrameDisp(packet, x, y, 0x50, 2);
    x = x + 2;
    y = y + 3;
    menuFontEnvMake(packet);
    text = monsNameOf(unit, isEn);
    menuFontPut(packet, x + ((0x10 - strlen(text)) * 5), y, (signed char *)text);
}

extern int keyCmd2tid(ObjectTask *, int);
extern int keyCmd2wno(ObjectTask *, int);
extern int battleCtrlHelpGet(void);
extern unsigned char *dataTecGet(int);
extern const char **dataNrmNameGet(int);
extern const char **dataSpcNameGet(int);
extern const char **dataWepNameGet(int);
extern char D_00A46960[];
extern char D_00A46968[];
extern char D_00A46970[];
extern char D_00A46978[];
extern s8 *atr1Name[3];
extern s8 *atr2Name[4];
extern s8 *atr3Name[6];

void menuKeyGuideHelp(int packet, ObjectTask *task, int cmd, int x, int y) {
    char sp[32];
    char *text;
    CalcUnitParam *up;
    int tx;
    int ty;
    int tid;
    int sel;
    int h;
    int a;
    int i;
    int b;

    tx = x + 0x18;
    if (cmd != 0 && cmd != 0x1C) {
        sel = (*(signed char *)((char *)dataTecGet(keyCmd2tid(task, cmd)) + 5)) != 0;
    } else {
        sel = 2;
    }
    ty = y + 3;
    menuGuideFrameDisp(packet, tx, y, 0x56, sel);
    menuFontEnvMake(packet);
    tx += 2;
    text = D_00A46970;
    if (cmd != 0 && cmd != 0x1C) {
        tid = keyCmd2tid(task, cmd);
        text = D_00A46960;
        if (battleCtrlHelpGet() == 0) {
            if ((unsigned)(tid - 1) < 0x2C) {
                text = *dataNrmNameGet(tid);
            } else if ((unsigned)(tid - 0x2D) < 0x36) {
                text = *dataSpcNameGet(tid);
            } else if ((unsigned)(tid - 0x2DC) < 0x8C) {
                up = calcUPGet(task);
                text = *dataWepNameGet(up->sefSetupParams[keyCmd2wno(task, cmd)]);
            }
        } else {
            sp[0] = 0;
            text = sp;
            b = *dataTecGet(tid) & 3;
            if (b == 1) {
                i = 1;
                goto atr1_have;
            }
            if (b < 2) {
                if (b == 0) {
                    i = 0;
                    goto atr1_have;
                }
            }
            i = 2;
atr1_have:;
            strcat(sp, atr1Name[i]);
            strcat(sp, D_00A46968);
            a = *(unsigned short *)((char *)dataTecGet(tid) + 0x12) & 0xE0;
            switch (a) {
            case 0x40:
                i = 1;
                break;
            case 0x20:
                i = 2;
                break;
            case 0x80:
                i = 0;
                break;
            default:
                i = 3;
                break;
            }
            strcat(sp, atr2Name[i]);
            strcat(sp, D_00A46968);
            h = *(unsigned short *)((char *)dataTecGet(tid) + 0x12) & 0x1F;
            switch (h) {
            case 16:
                i = 0;
                break;
            case 8:
                i = 1;
                break;
            case 4:
                i = 2;
                break;
            case 2:
                i = 3;
                break;
            case 1:
                i = 4;
                break;
            default:
                i = 5;
                break;
            }
            strcat(sp, atr3Name[i]);
        }
    } else if (cmd == 0x1C) {
        text = D_00A46978;
    }
    menuFontPut(packet, tx + ((0x11 - strlen(text)) * 5), ty, text);
    menuStatEnvMake(packet);
}

extern unsigned char *dataWpnGet(int weaponId);
static inline int menuKeyGuideIs6(int tid)
{
    return *(signed char *)(dataTecGet(tid) + 0x11) == 6;
}
static inline int menuKeyGuideSel(int tid)
{
    return *(signed char *)(dataTecGet(tid) + 0x5) != 0;
}

void menuKeyGuideVal(int packet, ObjectTask *unit, int key, int x, int y)
{
    int yy;
    int xx;
    int sel;
    int tid;
    int wno;
    unsigned char *wpn;
    signed char var_23;

    wno = keyCmd2wno(unit, key);
    if (wno != -1) {
        tid = keyCmd2tid(unit, key);
        if (!(calcUPGet(unit)->flags & 0x40)) {
            if (tid == 0) {
                return;
            }
            if (*(signed char *)(dataTecGet(tid) + 0x11) != 6) {
                return;
            }
        }
        yy = y;
        xx = x + 0xC6;
        wpn = dataWpnGet(*(short *)((char *)calcUPGet(unit) + wno * 2 + 0x5E));
        sel = menuKeyGuideSel(tid);
        menuGuideFrameDisp(packet, xx, yy, 0x38, sel);
        yy += 6;
        xx += 8;
        grGsRegSet(packet, 0x42, 0U);
        grOpenSpr(packet);
        if (wpn[0x10] != 0) {
            menuNumPut(packet, xx, yy, *(short *)((char *)calcUPGet(unit) + wno * 2 + 0x70), 2, 1);
        }
        menuStatSprite(packet, x + 0xE6, yy, 0x12, 0xC, 0x12, 0xAE);
        menuNumPut(packet, x + 0xF8, yy, calcTakeEpGet(unit, *(short *)(wpn + 0x14)), 2, 1);
        var_23 = (*(signed char *)(dataTecGet(tid) + 0x11) == 6) ? 6 : *(signed char *)(wpn + 0xF);
        menuStatSprite(packet, xx + 0x42, yy, 0x12, 0xC, 0x24, 0xAE);
        menuNumPut(packet, xx + 0x54, yy, var_23, 1, 1);
        grCloseSpr();
    }
}

/* obj.c (ov01/tu001), exact_c: allocates a task from the free list and
 * stores the caller's callback into its ->exec slot. */
extern ObjectTask *objEntry2(void *argument, void (*callback)(ObjectTask *task));
/* obj.c (ov01/tu001), still INCLUDE_ASM there. */
extern void objStdInit(ObjectTask *task);

void objDmgNum(ObjectTask *task);
void objDmgNumDraw(ObjectTask *task);

/* The work block objWorkGet allocates for an objDmgNum/objDmgNumDraw task.
 * Only the fields objDmgNumCreate itself writes are evidenced; the span
 * before them is read by neither this function nor any other claimed one. */
typedef struct {
    unsigned char unmodeled_00[0x70];
    int countdown;         /* +0x70: objDmgNumCreate always sets 30 */
    int value;              /* +0x74 */
    int flags;               /* +0x78 */
    unsigned char unmodeled_7C[4];
    void *target;           /* +0x80 */
} DmgNumWork;

ObjectTask *objDmgNumCreate(void *target, int value, int flags)
{
    ObjectTask *task;
    DmgNumWork *work;

    task = objEntry2(objDmgNum, objDmgNumDraw);
    if (task != 0) {
        work = task->work;
        objStdInit(task);
        work->target = target;
        work->value = value;
        work->flags = flags;
        work->countdown = 30;
        return task;
    }
}

void objDmgNum(ObjectTask *task)
{
    int ofs[30] = {
        0, 8, 14, 8, 0, -8, -14, -3, 0, 3, 6, 5, 0, -3, -5, -3,
        0, 3, 4, 2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    DmgNumWork *work;

    { float _sb = task != 0; if (_sb) {} }
    work = task->work;
    sefGetNullPosition((char *)work + 0x90, *(int *)((char *)work->target + 0x14), 15, 0);
    *(int *)work->unmodeled_7C = ofs[30 - work->countdown];
    if (work->countdown-- <= 0) {
        objRemove(task);
    }
}

extern int grBBCalc(ObjectTask *task, int *x, int *y);
void menuStatEnvMake(int packet);
void dmgNumPut(int packet, int x, int y, int num, int type);
void dmgMsgPut(int packet, int x, int y, int id);

void objDmgNumDraw(ObjectTask *task)
{
    int packet[4];
    int x;
    int y;
    char *work;
    int flags;
    int type;
    int bits;
    int off;
    int ofs;
    int id;
    int dy;

    work = (char *)task->work;
    if (grBBCalc(task, &x, &y) != 0) {
        grGpInit((int)packet);
        menuStatTexTrans();
        menuStatEnvMake((int)packet);
        grOpenSpr((int)packet);
        flags = *(int *)(work + 0x78);
        type = flags & 0xFF;
        ofs = *(int *)(work + 0x70) - 8;
        bits = (flags >> 8) & 0xFF;
        ofs = (ofs >= 0) ? -ofs : 0;
        dy = *(int *)(work + 0x7C);
        switch (type) {
        case 0:
            dmgMsgPut((int)packet, x, y + dy, 2);
            break;
        case 1: case 2: case 3: case 4: case 7: case 8: case 9: case 10:
            dmgNumPut((int)packet, x, y + dy, *(int *)(work + 0x74), type);
            break;
        }
        id = -1;
        if (bits & 1) {
            id = 0;
        } else if (bits & 2) {
            id = 3;
        } else if (bits & 4) {
            id = 1;
        } else if (bits & 8) {
            id = 4;
        }
        if (id != -1) {
            dmgMsgPut((int)packet, x + ofs, y, id);
        }
        grCloseSpr();
        grPacketSend((int)packet);
    }
}

typedef struct { KeyGideUV e[5]; } DmgMsgUVTbl;
extern DmgMsgUVTbl D_00A46A70;

void dmgMsgPut(int packet, int x, int y, int id)
{
    DmgMsgUVTbl tbl = D_00A46A70;

    menuStatSprite(packet, x - 0x12, y - 0xE, tbl.e[id].w, tbl.e[id].h, tbl.e[id].u, tbl.e[id].v);
}

void menuStatSpriteRGB(int packet, int x, int y, int w, int h, int u, int v, unsigned char *col);

void dmgNumPut(int packet, int x, int y, int num, int type) {
    unsigned char col[4][4] = {{0xFF, 0xFF, 0xFF, 0x80}, {0x00, 0xFF, 0x00, 0x80}, {0x00, 0xFF, 0xFF, 0x80}, {0x00, 0xFF, 0x00, 0x80}};
    int var_16;
    int var_17;
    int var_18;
    int var_20;
    int var_3;
    int temp_4;
    int temp_5;
    int temp_9;
    int w;

    w = 0xF;
    var_16 = 1;
    var_17 = 0x3E8;
    var_20 = 0;
    do {
        temp_4 = num < var_16;
        var_16 *= 0xA;
        if (temp_4 == 0) {
            var_20 += 1;
        }
    } while (var_16 <= var_17);
    var_20 = (var_20 == 0) ? 1 : var_20;
    switch (type) {
    case 1:
        var_3 = 0;
        break;
    case 2:
        var_3 = 1;
        break;
    case 3:
    case 9:
    case 10:
        var_3 = 2;
        break;
    case 4:
        var_3 = 3;
        break;
    default:
        var_3 = 0;
        break;
    }
    var_16 = 4;
    var_18 = x - ((var_20 * w) / 2);
    do {
        temp_9 = (num / var_17) * w;
        num = num % var_17;
        if (var_20 >= var_16) {
            temp_5 = var_18;
            var_18 += w;
            menuStatSpriteRGB(packet, temp_5, y, w, 0x16, temp_9, 0x6A, &col[var_3][0]);
        }
        var_16 -= 1;
        var_17 = var_17 / 10;
    } while (var_16 > 0);
}

int menuFontIdxGet(const signed char *text)
{
    int index;

    for (index = 0; index < 230; index++) {
        if (text[0] == font_0[index]) {
            return index;
        }
    }
    return -1;
}

int menuDFontIdxGet(const signed char *text)
{
    int index;

    for (index = 0; index < 230; index++) {
        if (text[0] == font_1[index * 2] && text[1] == font_1[index * 2 + 1]) {
            return index + 175;
        }
    }
    return -1;
}

void menuStatSprite(int packet, int x, int y, int width, int height, int srcX, int srcY, int srcRow);

/* The font sheet is a 25-column grid of 0xA x 0x12 (10x18) glyph cells. */
#define FONT_GLYPH_COLUMNS 25
#define FONT_GLYPH_WIDTH 0xA
#define FONT_GLYPH_HEIGHT 0x12

void menuFontPutSub(int packet, int x, int y, int charCode) {
    int row;
    int col;

    row = charCode / FONT_GLYPH_COLUMNS;
    col = charCode % FONT_GLYPH_COLUMNS;
    menuStatSprite(packet, x, y, FONT_GLYPH_WIDTH, FONT_GLYPH_HEIGHT,
                   col * FONT_GLYPH_WIDTH, row * FONT_GLYPH_HEIGHT, row);
}

extern int sscanf(const char *, const char *, ...);

void menuFontPut(int packet, int x, int y, signed char *text) {
    char buf[16];
    int code;
    int i;
    signed char *p;

    i = 0;
    grOpenSpr(packet);
    while (text[i] != 0) {
        p = &text[i];
        if (*p == '#') {
            buf[0] = p[1];
            i += 2;
            buf[1] = p[2];
            buf[2] = 0;
            sscanf(buf, "%X", &code);
        } else if (*p & 0x80) {
            i += 1;
            code = menuDFontIdxGet(p);
        } else {
            code = menuFontIdxGet(p);
        }
        if (code != -1) {
            menuFontPutSub(packet, x, y, code);
        }
        i += 1;
        x += 10;
    }
    grCloseSpr();
}

typedef struct {
    long long d[16];
} CurPutSave;

void menuActCurPut(int packet, ObjectTask *task, void *target, int no)
{
    CurPutSave save;
    int x;
    int y;
    CurPutSave *pos;

    if (target != 0 && !(**(int **)((char *)target + 0x10) & 0x40)) {
        pos = (CurPutSave *)((char *)task->work + 0x90);
        save = *pos;
        sefGetNullPosition(pos, *(int *)((char *)target + 0x14), 0, 0);
        *(float *)((char *)pos + 0x14) = 1.3f;
        if (grBBCalc(task, &x, &y) != 0) {
            menuStatEnvMake(packet);
            grOpenSpr(packet);
            ((void (*)())menuStatSprite)(packet, x - 8, y - 8, 0x10, 0x10, (no % 8) * 0x10, 0x80);
            grCloseSpr();
            grPacketSend(packet);
        }
        *pos = save;
    }
}

typedef struct { unsigned char c[4]; } MenuSprCol;

typedef struct {
    float x0, y0, z0, pad0;
    float x1, y1, z1, pad1;
    MenuSprCol col;
    short u0, u1, v0, v1;
} MenuSprPrim;

extern void grPutSpr(int packet, MenuSprPrim *spr, int flag);

extern MenuSprCol D_00A46C10;

void menuStatSprite(int packet, int x, int y, int width, int height, int srcX, int srcY, int srcRow)
{
    MenuSprPrim spr;
    union { MenuSprCol c; int i; } col;

    col.c = D_00A46C10;
    spr.x0 = x;
    spr.y0 = y;
    spr.z0 = 16777212.0f;
    spr.x1 = x + width;
    spr.y1 = y + height;
    spr.z1 = 16777212.0f;
    spr.u0 = srcX;
    spr.u1 = srcX + width + 1;
    spr.v0 = srcY;
    spr.v1 = srcY + height + 1;
    *(int *)&spr.col = col.i;
    grPutSpr(packet, &spr, 0);
}

void menuStatSprite2(int packet, int x, int y, int w, int h, int u0, int v0, int u1, int v1)
{
    MenuSprPrim spr;
    union { MenuSprCol c; int i; } col;

    col.c = D_00A46C10;
    spr.u0 = u0;
    spr.x0 = x;
    spr.y0 = y;
    spr.v0 = v0;
    spr.z0 = 16777212.0f;
    spr.x1 = x + w;
    spr.y1 = y + h;
    spr.z1 = 16777212.0f;
    spr.u1 = u1;
    spr.v1 = v1;
    *(int *)&spr.col = col.i;
    grPutSpr(packet, &spr, 0);
}

void menuStatSpriteRGB(int packet, int x, int y, int w, int h, int u, int v, unsigned char *col)
{
    MenuSprPrim spr;

    spr.z0 = 16777212.0f;
    spr.x0 = x;
    spr.y0 = y;
    spr.x1 = x + w;
    spr.y1 = y + h;
    spr.z1 = 16777212.0f;
    spr.u0 = u;
    spr.v0 = v;
    spr.u1 = u + w + 1;
    spr.v1 = v + h + 1;
    __builtin_memcpy(&spr.col, col, 4);
    grPutSpr(packet, &spr, 0);
}

void menuStatSprite2RGB(int packet, int x, int y, int flip, int w, int u, int r, int u2, int g, unsigned char *col)
{
    MenuSprPrim spr;

    spr.u0 = u;
    spr.v0 = r;
    spr.x0 = x;
    spr.y0 = y;
    spr.z0 = 16777212.0f;
    spr.x1 = x + flip;
    spr.y1 = y + w;
    spr.z1 = 16777212.0f;
    spr.u1 = u2;
    spr.v1 = g;
    __builtin_memcpy(&spr.col, col, 4);
    grPutSpr(packet, &spr, 0);
}

/* dataXtxFileGet's record: only the texture id at +0x4 is read here. */
extern int *dataXtxFileGet(int index);
extern void xglPacketTextureTrans(int textureId);

void menuTexTrans(void) {
    int packet;

    xglPacketTextureTrans(dataXtxFileGet(1)[1]);
    grGpInit((int)&packet);
    grGsRegSet((int)&packet, 0x3F, 1);
    grPacketSend((int)&packet);
}

void menuStatTexTrans(void) {
    int packet;

    xglPacketTextureTrans(dataXtxFileGet(0)[1]);
    grGpInit((int)&packet);
    grGsRegSet((int)&packet, 0x3F, 1);
    grPacketSend((int)&packet);
}

extern void GS_REG_SET(void *packet, long long register_id, long long control);
extern unsigned short dataXtxAdrGet(int index);

void menuEnvMake(int packet)
{
    unsigned short tbp;
    unsigned short cbp;

    tbp = dataXtxAdrGet(0xD);
    cbp = dataXtxAdrGet(0xE);
    GS_REG_SET((void *)packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET((void *)packet, 6, (long long)tbp | ((long long)cbp << 37) | 0x2000000629340000LL);
    GS_REG_SET((void *)packet, 0x42, 0);
    GS_REG_SET((void *)packet, 0x47, 0x3000D);
    GS_REG_SET((void *)packet, 8, 0);
    GS_REG_SET((void *)packet, 0x14, 0);
}

extern void GS_REG_SET(void *packet, long long register_id, long long control);
extern unsigned short dataXtxAdrGet(int index);

void menuStatEnvMake(int packet)
{
    unsigned short tbp;
    unsigned short cbp;

    tbp = dataXtxAdrGet(0);
    cbp = dataXtxAdrGet(3);
    GS_REG_SET((void *)packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET((void *)packet, 6, (long long)tbp | ((long long)cbp << 37) | 0x2000000629340000LL);
    GS_REG_SET((void *)packet, 0x42, 0);
    GS_REG_SET((void *)packet, 0x47, 0x3000D);
    GS_REG_SET((void *)packet, 8, 0);
    GS_REG_SET((void *)packet, 0x14, 0);
}

void menuFaceEnvMake(int packet)
{
    unsigned short tbp;
    unsigned short cbp;

    tbp = dataXtxAdrGet(8);
    cbp = dataXtxAdrGet(9);
    GS_REG_SET((void *)packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET((void *)packet, 6, (long long)tbp | ((long long)cbp << 37) | 0x2000000629340000LL);
    GS_REG_SET((void *)packet, 0x42, 0);
    GS_REG_SET((void *)packet, 0x47, 0x3000D);
    GS_REG_SET((void *)packet, 8, 0);
    GS_REG_SET((void *)packet, 0x14, 0);
}

void menuATBEnvMake(int packet)
{
    unsigned short tbp;
    unsigned short cbp;

    tbp = dataXtxAdrGet(4);
    cbp = dataXtxAdrGet(7);
    GS_REG_SET((void *)packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET((void *)packet, 6, (long long)tbp | ((long long)cbp << 37) | 0x2000000629340000LL);
    GS_REG_SET((void *)packet, 0x42, 0);
    GS_REG_SET((void *)packet, 0x47, 0x3000D);
    GS_REG_SET((void *)packet, 8, 0);
    GS_REG_SET((void *)packet, 0x14, 0);
}

void menuFontEnvMake(int packet)
{
    unsigned short tbp;
    unsigned short cbp;

    tbp = dataXtxAdrGet(0xB);
    cbp = dataXtxAdrGet(0xC);
    GS_REG_SET((void *)packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET((void *)packet, 6, (long long)tbp | ((long long)cbp << 37) | 0x2000000629340000LL);
    GS_REG_SET((void *)packet, 0x42, 0x44);
    GS_REG_SET((void *)packet, 0x47, 0x3000D);
    GS_REG_SET((void *)packet, 8, 0);
    GS_REG_SET((void *)packet, 0x14, 0);
}

void menuStatIconEnvMake(int packet)
{
    unsigned short tbp;
    unsigned short cbp;

    tbp = dataXtxAdrGet(0);
    cbp = dataXtxAdrGet(1);
    GS_REG_SET((void *)packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET((void *)packet, 6, (long long)tbp | ((long long)cbp << 37) | 0x2000000629340000LL);
    GS_REG_SET((void *)packet, 0x42, 0);
    GS_REG_SET((void *)packet, 0x47, 0x3000D);
    GS_REG_SET((void *)packet, 8, 0);
    GS_REG_SET((void *)packet, 0x14, 0);
}

extern unsigned short dataXtxAdrGet(int index);

void menuATBEnvPalSet(int packet, int palette)
{
    unsigned short tbp;
    unsigned short cbp;

    tbp = dataXtxAdrGet(4);
    if (palette == 0) {
        cbp = dataXtxAdrGet(5);
    } else if (palette == 1) {
        cbp = dataXtxAdrGet(7);
    } else {
        cbp = dataXtxAdrGet(10);
    }
    grGsRegSet(packet, 6, (unsigned long long)tbp | ((unsigned long long)cbp << 37) | 0x2000000629340000ULL);
}

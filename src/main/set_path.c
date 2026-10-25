#include "common.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "main/xgl_studio.h"

extern unsigned int strlen(const char *string);

static void splitName(unsigned char *filename, unsigned char *path, unsigned char *name)
{
    int len;
    int slash;
    int i;
    int nameLen;

    len = strlen((const char *) filename);
    slash = 0;
    for (i = len; i >= 0; i--) {
        if ((signed char) filename[i] == '/') {
            slash = i;
            break;
        }
    }

    for (i = 0; i <= slash; i++) {
        path[i] = filename[i];
    }
    path[slash + 1] = 0;

    nameLen = 0;
    for (i = slash + 1; i <= len; i++) {
        name[nameLen] = filename[i];
        nameLen++;
    }
    name[nameLen] = 0;
}

void splitName(unsigned char *filename, unsigned char *path, unsigned char *name);
void srsSetViewPath(unsigned char *path);
extern unsigned char name_1[];
extern unsigned char path_0[];

void setPath(unsigned char *filename)
{
    splitName(filename, path_0, name_1);
    srsSetViewPath(path_0);
}

typedef struct CfSelectState {
    int unk0;
    int unk4;
    int unk8;
    int unkC;
    char path[0x100];
    int filelistAddr;
    int unk114;
    int printfFn;
    /* +0x11c: the selected entry's path, filled by xglCdFileSelect and
     * handed to setPath() by execFileSelect; its extent past this offset
     * is unmodeled. */
    unsigned char selectedPath[1];
} CfSelectState;

extern CfSelectState cfs;

extern int xglCdFileSelect(CfSelectState *state);

extern void xglRenderClearFrame(void);

extern void xglSleep(void);

extern int filelist;

extern char *gridlexTop;

extern char *gridxtxTop;

extern char *hamalexTop;

extern char *hamaxtxTop;

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

typedef struct CfSelectStateU {
    int horizontalOrigin; /* +0x0 */
    int verticalOrigin; /* +0x4 */
    int visibleRows; /* +0x8 */
    int phase; /* +0xc */
    char basePath[256]; /* +0x10 */
    char *listBuffer; /* +0x110 */
    int listSize; /* +0x114 */
    void (*printFunc)(int x, int y, const char *format, ...); /* +0x118 */
    /* +0x11c: the selected entry's path, filled by xglCdFileSelect and
     * handed to setPath() by execFileSelect; its extent past this offset
     * is unmodeled. */
    unsigned char selectedPath[1];
    unsigned char unmodeled_11d[0x268 - 0x11d];
} CfSelectStateU;
extern CfSelectStateU cfsU __asm__("cfs");
extern char *filelistP __asm__("filelist");
extern char *cfScriptP __asm__("cfScript");
extern char basepath[0x17];

void initFileSelect(void)
{
    char **directorySlot = &filelistP;

    if (*directorySlot != 0)
        return;
    {
        CfSelectStateU *selector = &cfsU;
        int *visibleRowLimit = &selector->visibleRows;
        char *gridxtxStart = (char *) 0x1167200;
        unsigned int gridlexAddress = 0x1160000;
        char **resourceSlot = &gridlexTop;

        selector->verticalOrigin = 20;
        selector->phase = 0;
        *resourceSlot = (char *) gridlexAddress;
        resourceSlot = &gridxtxTop;
        *resourceSlot = gridxtxStart;
        resourceSlot = &hamalexTop;
        *resourceSlot = (char *) 0x1168700;
        resourceSlot = &hamaxtxTop;
        *resourceSlot = (char *) 0x116AA00;
        resourceSlot = &cfScriptP;
        *resourceSlot = (char *) 0x1172F00;
        *directorySlot = (char *) 0x1000000;
        selector->horizontalOrigin = 8;
        *visibleRowLimit = 20;
        strcpy(selector->basePath, basepath);
        selector->listBuffer = *directorySlot;
        selector->listSize = 0x160000;
        selector->printFunc = xglFontDebugPrintf;
    }
}

int execFileSelect(void)
{
    int result;

    for (;;) {
        xglRenderClearFrame();
        result = xglCdFileSelect(&cfs);
        switch (result) {
        case 0:
            break;
        case 1:
            setPath(cfs.selectedPath);
            return 1;
        case 2:
            setPath(cfs.selectedPath);
            return 2;
        }
        xglSleep();
    }
}

/*
 * PARTIAL ACCESSED PREFIX of PadData (0xd0-byte object at 0x490d90).
 * sdbExecEffect reads only the halfword at +0x2a (masked with 0x800; the
 * accepted "pressed" name from src/main/game.c) and the halfword at +0x2c
 * (masked with 0x100), one halfword past the shared include/shared.h
 * PadPrefix view (evidenced only to +0x2c).
 *
 * header_divergence (recorded, allowed): a further local extension of
 * PadData past PadPrefix, the same situation documented in
 * src/main/game.h's PadDataDebugLayout.
 */
typedef struct PadDataEffectLayout {
    u8 unmodeled_00[0x28];
    u16 half_28;
    u16 pressed;
    u16 half_2c;
} PadDataEffectLayout;

extern PadDataEffectLayout PadData;
extern s16 _debugPause;
extern char D_004DBA08[];
extern void sefExecEffect(void);
extern void func_A31438(void);
extern void xglSoundStreamMain(void);

static void sdbExecEffect(void)
{
    PadDataEffectLayout *pad = &PadData;
    u16 toggled;
    u16 debugPaused;

    if (pad->pressed & 0x800) {
        toggled = (_debugPause == 0);
        _debugPause = toggled;
        debugPaused = toggled;
    } else {
        debugPaused = _debugPause;
    }

    if (debugPaused == 0 || (pad->half_2c & 0x100)) {
        sefExecEffect();
    } else {
        xglFontDebugPrintf(0x10, 0x18, D_004DBA08);
    }

    func_A31438();
    xglSoundStreamMain();
}

void sdbDebugCamera(void) {

}

static void sdebufPrintMenu(int x, int y, int dy, char **list, int count)
{
    char **cursor;
    char *label;
    int printY;
    int lineY;
    int lineX;
    int remaining;
    int step;

    step = dy;
    if (count > 0) {
        lineX = x + 8;
        cursor = list;
        lineY = y;
        remaining = count;
        do {
            label = *cursor;
            cursor++;
            printY = lineY;
            lineY += step;
            remaining--;
            xglFontDebugPrintf(lineX, printY, label);
        } while (remaining != 0);
    }
}

extern int sprintf(char *destination, const char *format, ...);
extern char *menu_3[];
extern int cur_2;
extern int menumax_4;
extern char D_004DBA18[];
extern char D_004CBCD8[];
extern char D_004CBCE8[];
extern char D_004CBCF8[];
typedef struct PadDataCfgLayout {
    u8 unmodeled_00[0x28];
    u64 buttons;
} PadDataCfgLayout;

static int debugEffectConfig(void)
{
  char text[0x100];
  int cur;
  int step = 0x10;
  int pressed;
  long long up;
  int sel;
  for (;;)
  {
    xglRenderClearFrame();
    sprintf(text, D_004CBCD8, D_004CBCE8, D_004CBCF8);
    xglFontDebugPrintf(0x10, 0x10, text);
    xglFontDebugPrintf(0x10, (cur_2 * step) + 0x1C, D_004DBA18);
    sdebufPrintMenu(0x10, 0x10, 0x10, menu_3, menumax_4);
    if ((((PadDataCfgLayout *) (&PadData))->buttons & 0x08000100) == 0x08000100)
    {
      cur = -1;
      cur_2 = cur;
    }
    else
    {
      pressed = PadData.pressed;
      up = pressed & 0x4000;
      sel = pressed & 0x20;
      pressed = pressed & 0x1000;
      if (pressed)
      {
        cur_2--;
      }
      if (up)
      {
        cur_2++;
      }
      if (cur_2 < 0)
      {
        cur_2 = menumax_4 - 1;
      }
      if (cur_2 >= menumax_4)
      {
        cur_2 = 0;
      }
      if (!sel)
      {
        xglSleep();
        continue;
      }
    }
    break;
  }

  xglSleep();
  xglRenderClearFrame();
  return cur_2;
}

extern int cur_5;
extern char D_004CBD18[];
extern char D_004CBD08[];
typedef struct {
    u8 unmodeled_00[0x28];
    u64 buttons;
} PadDataForEffect;

static int debugEffectSelect(void)
{
  char sp[0x100];
  int var_3;
  int cmp;
  int divisor = 10;
  long long mask = 0x08000100;
  int cmp_54;
  PadDataForEffect *pad;
  do
  {
    loop_1:
    xglRenderClearFrame();

    sprintf(sp, "db%d%d : %d", cur_5 / divisor, cur_5 % divisor, cur_5);
    xglFontDebugPrintf(0x10, 0x10, D_004CBD18);
    xglFontDebugPrintf(0x10, 0x20, sp);
    if (!((((PadDataForEffect *) (&PadData))->buttons & mask) != mask))
    {
      var_3 = -1;
      cur_5 = var_3;
    }
    else
    {
      var_3 = cur_5;
      if (PadData.half_2c & 4)
      {
        var_3 -= 10;
        cur_5 = var_3;
      }
      if ((PadData.half_2c & 1) & 0xFFFF)
      {
        var_3 -= 50;
        cur_5 = var_3;
      }
      if (PadData.half_2c & 0x8000)
      {
        var_3 -= 1;
        cur_5 = var_3;
      }
      if (PadData.half_2c & 2)
      {
        var_3 += 50;
 do { } while (0);
        cur_5 = var_3;
      }
      if (PadData.half_2c & 8)
      {
        var_3 += 10;
        cur_5 = var_3;
      }
      if (PadData.half_2c & 0x2000)
      {
        var_3 += 1;
        cur_5 = var_3;
      }
      cmp = 0;
      if (var_3 < 0x1F)
      {
        do
        {
          cmp = 1;
        }
        while (0);
      }
      if (var_3 <= 0)
      {
        var_3 = 0x1E;
        cur_5 = var_3;
        ;
      }
      if ((var_3 < 0x1F) == 0)
      {
        var_3 = 1;
        cur_5 = var_3;
        pad++;
        pad--;
      }
      if (!(PadData.pressed & 0x20))
      {
        xglSleep();
        if (var_3)
        {
        }
        goto loop_1;
      }
    }
  }
  while (0);
  xglSleep();
  xglRenderClearFrame();
  return cur_5;
}

extern int cur_6;
extern char D_004CBD38[0x30];
extern char D_004CBD28[0x10];
extern char D_004DBA28[0x10];
extern int srsGetEsdData2(int index);
typedef struct PadDataEnemyLayout {
    u8 unmodeled_00[0x28];
    u64 buttons;
} PadDataEnemyLayout;
typedef struct PadDataEnemyHalf {
    u8 unmodeled_00[0x28];
    u16 half_28;
    u16 pressed;
    u16 half_2c;
} PadDataEnemyHalf;

static int debugEnemySelect(void)
{
    u64 mask;
    u64 btn;

    char text[0x100];
    int prev;
    int var;
    int esd;
    u16 held;

    for (;;) {
    xglRenderClearFrame();
    if ((u32)(cur_6 - 0xB54) < 0x63U) {
        esd = srsGetEsdData2(cur_6 - 0x64);
        if ((esd != 0) && (*(char *)esd != 0)) {
            sprintf(text, D_004CBD28, esd, cur_6);
        } else {
            sprintf(text, D_004DBA28, esd, cur_6);
        }
    } else {
        sprintf(text, D_004DBA28, srsGetEsdData2(cur_6), cur_6);
    }
    xglFontDebugPrintf(0x10, 0x10, D_004CBD38);
    xglFontDebugPrintf(0x10, 0x20, text);
    btn = ((PadDataEnemyLayout *)&PadData)->buttons;
    mask = 0x08000100;
    if ((btn & 0x08000100) == 0x08000100) {
        mask = -1;
        var = (int)mask;
        cur_6 = var;
        goto done;
    }
    var = cur_6;
    prev = var;
    held = PadData.half_2c;
    if (held & 0x4) {
        var -= 10;
        cur_6 = var;
    }
    if (held & 0x1) {
        var -= 0x32;
        cur_6 = var;
    }
    if (held & 0x8000) {
        var -= 1;
        cur_6 = var;
    }
    if (held & 0x2) {
        var += 0x32;
        cur_6 = var;
    }
    if (held & 0x8) {
        var += 0xA;
        cur_6 = var;
    }
    if (held & 0x2000) {
        var += 1;
        cur_6 = var;
    }
    if (var <= 0) {
        var = 0xBB6;
        cur_6 = var;
    } else if ((u32)(var - 0x258) < 0x579U) {
        if (var < prev) {
            var = 0x257;
            cur_6 = var;
        } else {
            var = 0x7D1;
            cur_6 = var;
        }
    } else if (var >= 0xBB7) {
        var = 1;
        cur_6 = var;
    }
    if (PadData.pressed & 0x20) {
        goto done;
    }
    xglSleep();
    }
done:
    xglSleep();
    xglRenderClearFrame();
    return cur_6;
}

typedef struct PadDataMenuLayout {
    u8 unmodeled_00[0x28];
    u64 buttons;
} PadDataMenuLayout;

extern int sprintf(char *destination, const char *format, ...);
extern void xglRenderClearFrame(void);
extern void xglSleep(void);
extern int cur_7;
extern char D_004CBD68[];
extern char D_004DBA28[];
extern int srsGetEsdData2(int index);

static int debugCfSelect(void)
{
    char text[0x100];
    u16 held;
    int esd;

    for (;;) {
        esd = srsGetEsdData2(cur_7);
        xglRenderClearFrame();
        sprintf(text, D_004DBA28, esd, cur_7);
        xglFontDebugPrintf(0x10, 0x10, D_004CBD68);
        xglFontDebugPrintf(0x10, 0x20, text);
        if ((((PadDataMenuLayout *) &PadData)->buttons & 0x08000100) == 0x08000100) {
            cur_7 = -1;
            break;
        }
        held = PadData.half_2c;
        if (held & 0x4) cur_7 -= 10;
        if (held & 0x1) cur_7 -= 50;
        if (held & 0x8000) cur_7--;
        if (held & 0x2) cur_7 += 50;
        if (held & 0x8) cur_7 += 10;
        if (held & 0x2000) cur_7++;
        if (cur_7 < 601) cur_7 = 1999;
        if (cur_7 >= 2000) cur_7 = 601;
        if (PadData.pressed & 0x20) break;
        xglSleep();
    }
    xglSleep();
    xglRenderClearFrame();
    return cur_7;
}

extern void func_A31688(void);
extern void func_A31920(int kind, float *params);
extern void func_A318B8(void);
extern float D_004D8298;
extern float D_004D829C;
extern void *memset(void *destination, int value, unsigned int count);

static void SCamTake(void)
{
    float params[44];

    func_A31688();
    memset(params, 0, sizeof(params));
    params[0] = 0.0f;
    params[4] = D_004D8298;
    params[5] = D_004D829C;
    params[6] = 5.0f;
    params[7] = 1.0f;
    func_A31920(0, params);

    params[4] = 0.0f;
    params[5] = 2.0f;
    params[6] = 0.0f;
    params[7] = 1.0f;
    func_A31920(1, params);

    params[41] = 40.0f;
    func_A31920(3, params);

    func_A318B8();
}

extern void xglStudioChange(int studio_index);
extern void xglLightSetDefault(StudioLight *light);
typedef struct SCam {
    int p0[0xD0/4];
    float f_d0, f_d4, f_d8, f_dc;
    int p1[8];
    float f_100, f_104;
    int p2[6];
    float f_120, f_124, f_128, f_12c;
} SCam;
extern void xglStudioGetCamera(SCam **, int);

void SinitCamera(void) {
    StudioLight *light;
    SCam *cam;
    SCam *c;
    float one;
    float d8;
    float e = 1.4898003f;

    xglStudioChange(0);
    xglStudioGetCamera(&cam, 0);
    c = cam;
    one = 1.0f;
    c->f_100 = e;
    c->f_104 = 9.9308424f;
    d8 = 4.87f;
    c->f_120 = 0.0f;
    c->f_124 = one;
    c->f_128 = 0.0f;
    c->f_12c = one;
    c->f_dc = one;
    c->f_d0 = 0.435f;
    c->f_d4 = 2.0f;
    c->f_d8 = d8;
    xglStudioGetLight(&light);
    xglLightSetDefault(light);
}

extern void PartyDataInit2(void);
extern int dataItmBoxChk(int id);
extern int dataItmBoxInc(int id);
extern void func_A2F6A8(void);
extern void func_A22668(void);
extern s16 loaded;

static void sbattleDebug(void)
{
    int i;

    filelist = 0;
    loaded = 0;
    PartyDataInit2();

    for (;;) {
        xglRenderClearFrame();
        i = 0;
        func_A2F6A8();
        while (i < 0x63) {
            i++;
            if (dataItmBoxChk(0x15) >= 0x63) {
                break;
            }
            dataItmBoxInc(0x15);
        }
        func_A22668();
    }
}

extern void sresFreeReloaderMemory(int);
extern void sefInitEffectBattle(void);
extern void sefLoadEffect(int, int);
extern void sefCreateEffect(int *params);
extern int sefCheckLoad(void);
extern void sdbExecEffect(void);
extern void dummyMapDraw(void);
extern s16 casdisp;
extern s16 tgtdisp;
extern char D_004CBD90[];
extern char D_004CBDB8[];
extern int prm_8[];
extern s16 _sdrawFloor;

static void seffectDebugDb(void)
{
    int sel;
    unsigned long long held;
    int *prm;

    SinitCamera();
    sel = debugEffectSelect();
    if (sel < 0) {
        _sdrawFloor = 0;
        sresFreeReloaderMemory(1);
        xglSleep();
        return;
    }
    sel += 0xBE1;
    sefInitEffectBattle();
    dummyMapLoad();
    sefLoadEffect(sel, 1);
    prm = prm_8;
    held = ((PadDataForEffect *)&PadData)->buttons & 0x08000100;
    *(s16 *)((char *)prm + 0x14) = (s16)sel;
    prm[1] = 0;
    prm[0] = 0;
    if (held != 0x08000100) {
        do {
            xglRenderClearFrame();
            if (sefCheckLoad() == 0) {
                xglFontDebugPrintf(8, 8, D_004CBD90);
                if (PadData.pressed & 0x20) {
                    sefCreateEffect(prm_8);
                }
                _sdrawFloor = 1;
                if (PadData.pressed & 4) {
                    casdisp = (casdisp == 0);
                }
                if (PadData.pressed & 1) {
                    tgtdisp = (tgtdisp == 0);
                }
                dummyMapDraw();
                sdbExecEffect();
            } else {
                xglFontDebugPrintf(0x10, 0x10, D_004CBDB8);
            }
            xglSleep();
        } while ((((PadDataForEffect *)&PadData)->buttons & 0x08000100) != 0x08000100);
    }
    _sdrawFloor = 0;
    sresFreeReloaderMemory(1);
}

typedef struct SetPathEffectPeer {
    unsigned char unmodeled_00[0xa9a];
    signed char debugFlag;
} SetPathEffectPeer;
typedef struct DmyActorCf {
    unsigned char unmodeled_00[0x5c];
    float rotation_w;
    float scale_x;
    float scale_y;
    float scale_z;
    float scale_w;
    unsigned char unmodeled_70[0x86 - 0x70];
    short enabled;
    unsigned char unmodeled_88[0xa70 - 0x88];
} DmyActorCf;
extern SetPathEffectPeer *handle_9;
extern DmyActorCf dmyActor_12;
extern unsigned char point_10[0x10];
extern unsigned char angle_11[0x10];
extern void sefDestroyEffectCf(void);
extern void sefKillEffect(int);
extern void *sefCreateEffectCf(int, const void *, const void *);
extern int sefLoadEffectCf(int, int);
extern void sefInitEffectCf(void);
extern s16 mapdisp;
static void dummyMapLoad(void);

static void seffectDebugCf(void)
{
    int effect;

    SinitCamera();
    effect = debugCfSelect();
    if (effect < 0) {
        xglSleep();
        xglRenderClearFrame();
        return;
    }

    handle_9 = 0;
    memset(&dmyActor_12, 0, 0xA70);
    __asm__ __volatile__("sqc2 $vf0, 0(%0)" : : "r"(&dmyActor_12.scale_x) : "memory");
    dmyActor_12.rotation_w = 1.0f;
    dmyActor_12.scale_w = 1.0f;
    dmyActor_12.scale_z = 1.0f;
    dmyActor_12.scale_y = 1.0f;
    dmyActor_12.enabled = 1;
    dmyActor_12.scale_x = 1.0f;
    xglSleep();
    xglRenderClearFrame();
    sefInitEffectCf();
    dummyMapLoad();
    sefLoadEffectCf((int)cfScriptP, effect);

    while ((((PadDataForEffect *)&PadData)->buttons & 0x08000100) != 0x08000100) {
        xglRenderClearFrame();
        if (sefCheckLoad() == 0) {
            xglFontDebugPrintf(8, 8, D_004CBD90);
            if (PadData.pressed & 0x20) {
                handle_9 = sefCreateEffectCf(effect, point_10, angle_11);
            }
            if (PadData.pressed & 0x80) {
                sefKillEffect(effect);
            }
            if (PadData.pressed & 0x8) {
                mapdisp = (mapdisp == 0);
            }
            if (PadData.half_2c & 0x2) {
                if (handle_9 != 0) {
                    handle_9->debugFlag = (handle_9->debugFlag == 0);
                }
            }
            if (PadData.pressed & 0x4) {
                casdisp = (casdisp == 0);
            }
            if (PadData.pressed & 0x1) {
                tgtdisp = (tgtdisp == 0);
            }
            _sdrawFloor = 1;
            dummyMapDraw();
            sdbExecEffect();
        } else {
            xglFontDebugPrintf(0x10, 0x10, D_004CBDB8);
        }
        xglSleep();
    }

    _sdrawFloor = 0;
    sefDestroyEffectCf();
}

typedef struct SetPathEffectParamsSB {
    void *actor;
    void *target;
    void *source;
    unsigned char unmodeled_0c[4];
    short charaId;
    unsigned char unmodeled_12[2];
    short effectId;
    short variant;
    unsigned char unmodeled_18[0x24 - 0x18];
} SetPathEffectParamsSB;
typedef struct PadDataMenuLayoutSB {
    u8 unmodeled_00[0x28];
    union {
        u64 packed;
        struct {
            u16 held;
            u16 pressed;
            u16 repeat;
            u16 released;
        } halfwords;
    } buttons;
    u8 unmodeled_30[0x68 - 0x30];
} PadDataMenuLayoutSB;
extern SetPathEffectParamsSB prm_13;
extern s16 mapdisp;
extern const char D_004CBDC8[];
static void dummyMapLoad(void);
extern void func_A31840(void);
extern void sefDestroyEffectCf(void);
extern int srsEftNo2WeaponID(int effect);
extern int srsAnalyzeEftNo(int effect, int *kind, int *variant);
extern void sefSetupPlayer(int charaId, int weaponId, int setupParam1, int setupParam2);
extern void sefSetupEnemy(int charaId);
extern void sefSetupEffect(void);
extern void sefHitEffect(void);

static void seffectDebugBattle(void)
{
    extern PadDataMenuLayoutSB PadDataSB[2] __asm__("PadData");
    int effect;
    struct {
        int kind;
        int variant;
    } eftInfo;
    int weapon;

    eftInfo.kind = 0;
    SinitCamera();
    effect = debugEnemySelect();
    if (effect < 0) {
        _sdrawFloor = 0;
        sefDestroyEffectCf();
        xglSleep();
        return;
    }

    xglSleep();
    xglRenderClearFrame();
    sefInitEffectBattle();
    dummyMapLoad();
    srsAnalyzeEftNo(effect, &eftInfo.kind, &eftInfo.variant);
    if (eftInfo.kind > 0) {
        weapon = 0;
        if ((unsigned int) (effect - 2800) < 200) {
            weapon = srsEftNo2WeaponID(effect);
            eftInfo.kind = 17;
        }
        if ((unsigned int) (eftInfo.kind - 1) < 32) {
            sefSetupPlayer(eftInfo.kind, weapon, 0, 0);
        } else if (eftInfo.kind > 32) {
            sefSetupEnemy(eftInfo.kind);
        }
        sefSetupEffect();
        while (sefCheckLoad() != 0) {
            xglRenderClearFrame();
            xglFontDebugPrintf(8, 8, D_004CBDC8);
            xglSleep();
        }
    }

    memset(&prm_13, 0, sizeof(prm_13));
    prm_13.actor = 0;
    prm_13.charaId = (u16) eftInfo.kind;
    prm_13.target = 0;
    prm_13.effectId = effect;
    prm_13.source = 0;
    prm_13.variant = 1;
    if ((u16) (effect - 2027) < 3) {
        prm_13.charaId = 2;
    }
    if ((u16) (prm_13.effectId - 2040) < 2) {
        prm_13.charaId = 1;
    }
    sefLoadEffect(effect, prm_13.charaId);

    while ((PadDataSB[0].buttons.packed & 0x08000100) != 0x08000100) {
        if (PadDataSB[0].buttons.halfwords.pressed & 0x8) {
            mapdisp = (mapdisp == 0);
        }
        if (PadDataSB[0].buttons.halfwords.pressed & 0x4) {
            casdisp = (casdisp == 0);
        }
        if (PadDataSB[0].buttons.halfwords.pressed & 0x1) {
            tgtdisp = (tgtdisp == 0);
        }
        if (PadDataSB[0].buttons.halfwords.pressed & 0x10) {
            SCamTake();
        }
        if (PadDataSB[0].buttons.halfwords.pressed & 0x80) {
            func_A31840();
        }
        xglRenderClearFrame();
        _sdrawFloor = 1;
        if (sefCheckLoad() == 0) {
            xglFontDebugPrintf(8, 8, D_004CBD90);
            if (PadDataSB[0].buttons.halfwords.pressed & 0x20) {
                sefCreateEffect((int *) &prm_13);
            }
            if (PadDataSB[0].buttons.halfwords.pressed & 0x40) {
                sefHitEffect();
            }
            dummyMapDraw();
            sdbExecEffect();
        } else {
            xglFontDebugPrintf(0x10, 0x10, D_004CBDB8);
        }
        xglSleep();
    }

    _sdrawFloor = 0;
    sefDestroyEffectCf();
}

extern void func_A1D1C0(int level);
extern void func_A2C3A0(int level);
extern int srsFileLoad(void *buffer, const char *name, int mode);
extern s16 loaded;
extern char D_004CBDD8[];
extern char D_004CBDE8[];
extern char D_004DBA38[];
extern char D_004DBA40[];

static void dummyMapLoad(void)
{
    if (loaded == 0) {
        func_A2C3A0(10);
        func_A1D1C0(10);
        srsFileLoad(gridlexTop, D_004DBA38, 1);
        srsFileLoad(gridxtxTop, D_004DBA40, 1);
        srsFileLoad(hamalexTop, D_004CBDD8, 1);
        srsFileLoad(hamaxtxTop, D_004CBDE8, 1);
        loaded = 1;
    }
}

extern int func_A1A760(void);
extern void func_A2C488(int level);
extern void nmlModelEntry(int entry);
extern void nmlModelSetLight(void *colorMod, void *normal);
extern void nmlModelSetPlace(Matrix4 matrix);
extern void nmlModelSetTexture(const char *texture);
extern int sefCheckLoad(void);
extern void xglLightCalcMatrix(StudioLight *light);
extern void xglLightSetDefault(StudioLight *light);
extern StudioLight *xglStudioGetLight2(void);
extern unsigned char _defMatLcMod[];
extern unsigned char _defMatLn[];
extern s16 casdisp;
extern Vector4 caspos_15;
extern s16 mapdisp;
extern Vector4 pos_14;
extern Vector4 scale_17;
extern s16 tgtdisp;
extern Vector4 tgtpos_16;

static void dummyMapDraw(void)
{
    Matrix4 sp0;
    StudioLight *light;

    if (func_A1A760() != 0) {
        return;
    }
    if (sefCheckLoad() != 0) {
        return;
    }
    if (loaded == 0) {
        return;
    }

    light = xglStudioGetLight2();
    xglLightSetDefault(light);
    xglLightCalcMatrix(light);

    if (mapdisp != 0) {
        xglMatrixStackUnit();
        xglMatrixStackTrans(&pos_14.x);
        xglMatrixStackSave(sp0);
        func_A2C488(10);
        xglMatrixStackUnit();
        xglMatrixStackTrans(&pos_14.x);
        xglMatrixStackScale(&scale_17.x);
        xglMatrixStackSave(sp0);
        nmlModelSetTexture(gridxtxTop);
        nmlModelSetPlace(sp0);
        nmlModelSetLight(_defMatLcMod, _defMatLn);
        nmlModelEntry((int) gridlexTop);
    }
    if (casdisp != 0) {
        xglMatrixStackUnit();
        xglMatrixStackTrans(&caspos_15.x);
        xglMatrixStackScale(&scale_17.x);
        xglMatrixStackSave(sp0);
        nmlModelSetTexture(hamaxtxTop);
        nmlModelSetPlace(sp0);
        nmlModelSetLight(_defMatLcMod, _defMatLn);
        nmlModelEntry((int) hamalexTop);
    }
    if (tgtdisp != 0) {
        xglMatrixStackUnit();
        xglMatrixStackTrans(&tgtpos_16.x);
        xglMatrixStackScale(&scale_17.x);
        xglMatrixStackSave(sp0);
        nmlModelSetTexture(hamaxtxTop);
        nmlModelSetPlace(sp0);
        nmlModelEntry((int) hamalexTop);
    }
}

typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed int s32;
void func_A22628(void);
void func_A31840(void);
void initFileSelect(void);
void seffectDebugBattle(void);
void seffectDebugCf(void);
void seffectDebugDb(void);
void srsSetLoadMode(int);
void sefDestroyEffect(void);
typedef struct GsClearEnv {
    u32 tag0;
    u32 tag1;
    u32 tag2;
    u32 tag3;
    u64 target;
    u64 prim;
    u32 color_r;
    u32 color_g;
    u32 color_b;
    u32 color_a;
    u32 xy0;
    u32 xy1;
    u32 pad38;
    u32 pad3c;
    u32 xy2;
    u32 xy3;
    u32 pad48;
    u32 pad4c;
    u64 giftag;
    u64 prim2;
} GsClearEnv;
extern GsClearEnv ClearEnv;
extern s32 sftMode;

void SimajiriTest(void)
{
    s32 temp_2;

    _debugPause = 0;
    filelist = 0;
    loaded = 0;
    xglRenderClearFrame();
    ClearEnv.color_g = 4;
    ClearEnv.color_b = 0xC;
    ClearEnv.color_r = 0;
    ClearEnv.color_a = 0;
    srsSetLoadMode(1);
    while (1) {
        xglSleep();
        xglRenderClearFrame();
        initFileSelect();
        execFileSelect();
        xglSleep();
        xglRenderClearFrame();
        func_A22628();
        temp_2 = debugEffectConfig();
        sftMode = temp_2;
        if (temp_2 < 0) {
            sefDestroyEffect();
            xglSleep();
            xglRenderClearFrame();
            return;
        }
        xglSleep();
        xglRenderClearFrame();
        if (sftMode == 0) {
            sbattleDebug();
        } else if (sftMode == 1) {
            seffectDebugBattle();
        } else if (sftMode == 2) {
            seffectDebugCf();
        } else if (sftMode == 3) {
            seffectDebugDb();
        }
        func_A31840();
        sefDestroyEffect();
        xglRenderClearFrame();
        if (sftMode == 4) {
            return;
        }
    }
}

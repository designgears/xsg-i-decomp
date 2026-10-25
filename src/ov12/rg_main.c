/*
 * OV12 original TU 1: 0x00a00088..0x00a01ff8 (38 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_main.h"

static int _IsEndOfMode(void)
{
    int result;
    u16 pause;   /* PadData[0].prefix.half_2a bit 0x800: this function's debug-shortcut gate */
    u16 buttons; /* PadData[0].prefix.half_28 bit 0x100: the button bit it reports */

    buttons = PadData[0].prefix.half_28;
    pause = PadData[0].prefix.half_2a;
    result = 0;
    if (InstanceOfRgDebugFlags()->modeEnabled != 0 && (pause & 0x800) != 0) {
        if (buttons & 0x100) {
            result = 1;
        }
    }
    return result;
}

static int _DoAnyOnePausing(int arg0)
{
  unsigned char new_var;
  if (arg0 == 1)
  {
    goto L_case1;
  }
  if (new_var = arg0 == 0)
  {
    goto L_case0;
  }
  if (arg0 == 2)
  {
    goto L_case2;
  }
  if (arg0 != 3)
  {
    goto L_ret_neg1;
  }
  goto L_case3;
  L_case1:
  L_case0:
  if (PadData[0].prefix.half_2a & 0x800)
  {
    return 0;
  }


  goto L_ret_neg1;
  L_case2:
  if (PadData[1].prefix.half_2a & 0x800)
  {
    return 1;
  }

  goto L_ret_neg1;
  L_case3:
  if (PadData[0].prefix.half_2a & 0x800)
  {
    return 0;
  }

  if (PadData[1].prefix.half_2a & 0x800)
  {
    return 1;
  }
  goto L_ret_neg1;
  L_ret_neg1:
  return -1;

}

static int _IsEndOfPause(int controller)
{
    u16 held;

    if (controller < 0) {
        held = PadData[0].prefix.half_2a | PadData[1].prefix.half_2a;
    } else {
        held = PadData[controller].prefix.half_2a;
    }
    return held & 0x800;
}

static void _EndOfFrame(void)
{
    XrgPaint2DFlush(InstanceOfXrgPaint2D());
    XrgParticleDriverDisp(InstanceOfXrgParticleDriver());
    RgDrawJob(InstanceOfRgDraw());
    XrgSleep();
}

typedef struct {
    char pad0[0x40];
    float pos[4];
    float dir[4];
    char pad60[0x240 - 0x60];
    long long bgName[2];
} StageInitDat;

static void _GetStageData(int stageId, void *data)
{
    StageInitDat *pInitDat = data;
    char *p[2];

    if (data == 0) {
        assert_prog("pInitDat != NIL", "../rg_main.euc.c", 123);
    }
    p[1] = (char *)data + 0x120;
    p[0] = (char *)data;
    switch (stageId) {
    case 0:
        __builtin_strcpy((char *)pInitDat->bgName, "BG_TOWN");
        XrgSetVectorXYZ((float *)(p[0] + 0x40), 0.0f, 1.5f, -80.0f);
        XrgSetVectorXYZ((float *)(p[1] + 0x40), 0.0f, 1.5f, 80.0f);
        XrgSetVectorXYZ((float *)(p[0] + 0x50), 0.0f, 0.0f, 1.0f);
        XrgSetVectorXYZ((float *)(p[1] + 0x50), 0.0f, 0.0f, -1.0f);
        break;
    case 1:
        __builtin_strcpy((char *)pInitDat->bgName, "BG_AIRCAR");
        XrgSetVectorXYZ((float *)(p[0] + 0x40), 0.0f, 1.5f, -97.5f);
        XrgSetVectorXYZ((float *)(p[1] + 0x40), 0.0f, 1.5f, 68.0f);
        XrgSetVectorXYZ((float *)(p[0] + 0x50), 0.0f, 0.0f, 1.0f);
        XrgSetVectorXYZ((float *)(p[1] + 0x50), 0.0f, 0.0f, -1.0f);
        break;
    case 2:
        __builtin_strcpy((char *)pInitDat->bgName, "BG_COL");
        XrgSetVectorXYZ((float *)(p[0] + 0x40), 0.0f, 1.5f, -10.0f);
        XrgSetVectorXYZ((float *)(p[1] + 0x40), 0.0f, 1.5f, 10.0f);
        XrgSetVectorXYZ((float *)(p[0] + 0x50), 0.0f, 0.0f, 1.0f);
        XrgSetVectorXYZ((float *)(p[1] + 0x50), 0.0f, 0.0f, -1.0f);
        break;
    case 3:
        __builtin_strcpy((char *)pInitDat->bgName, "BG_BLOCK");
        XrgSetVectorXYZ((float *)(p[0] + 0x40), -90.0f, 1.5f, -80.0f);
        XrgSetVectorXYZ((float *)(p[1] + 0x40), 90.0f, 1.5f, 80.0f);
        XrgSetVectorXYZ((float *)(p[0] + 0x50), 0.0f, 0.0f, 1.0f);
        XrgSetVectorXYZ((float *)(p[1] + 0x50), 0.0f, 0.0f, -1.0f);
        break;
    case 4:
        __builtin_strcpy((char *)pInitDat->bgName, "BG_VS1");
        XrgSetVectorXYZ((float *)(p[0] + 0x40), 0.0f, 1.5f, -10.0f);
        XrgSetVectorXYZ((float *)(p[1] + 0x40), 0.0f, 1.5f, 10.0f);
        XrgSetVectorXYZ((float *)(p[0] + 0x50), 0.0f, 0.0f, 1.0f);
        XrgSetVectorXYZ((float *)(p[1] + 0x50), 0.0f, 0.0f, -1.0f);
        break;
    default:
        RgError("unknown stage ID %d", "../rg_main.euc.c", 163, stageId);
        break;
    }
}

/*
 * ov12:0x00a514d8 contains the format string "over stage-serial ID for
 * enemy (%d)"; ov12:0x00a51450 contains the source filename
 * "../rg_main.euc.c".
 */
extern const char D_00A514D8[];
extern const char D_00A51450[];

/* Stage id per serial, indexed 0..3 (ov12:0x00a4f4c8, size 0x10). */
extern int s_aeBgTbl_0[4];

static void _GetStageData(int stageId, void *data);

static void _GetStageDataBySerial(u32 serial, void *data)
{
    if (serial < 4U) {
        _GetStageData(s_aeBgTbl_0[serial], data);
        return;
    }
    RgWarn(D_00A514D8, D_00A51450, 180, serial);
    _GetStageData(2, data);
}

/* data: the caller's output record (asserted non-null; layout not yet recovered). */
static void _GetStageData(int stageId, void *data);

/* Same (serial, data) interface as _GetStageDataBySerial; the hard data is
 * always stage 3 and the caller's record is forwarded untouched. */
static void _GetStageHardDataBySerial(int serial, void *data)
{
    _GetStageData(3, data);
}

extern const char D_00A51450[];
extern const char D_00A514D8[];
extern int s_anEnemyLevel_1[4];

static int _GetEnemyLevelBySerial(u32 serial)
{
    int level;

    level = 0;
    if (serial < 4U) {
        level = s_anEnemyLevel_1[serial];
    } else {
        RgWarn(D_00A514D8, D_00A51450, 0x103, serial);
    }
    return level;
}

/* ov12:0x00a51500 contains "over stage-serial ID for (hard) enemy (%d)". */
extern const char D_00A51500[];

/* Enemy level per serial for hard mode, indexed 0..3 (ov12:0x00a4f4e8, size 0x10). */
extern int s_anEnemyLevel_2[4];

static int _GetEnemyHardLevelBySerial(u32 serial)
{
    int level;

    level = 0;
    if (serial < 4U) {
        level = s_anEnemyLevel_2[serial];
    } else {
        RgWarn(D_00A51500, D_00A51450, 325, serial);
    }
    return level;
}

#include "ov12/rg_select_agws.h"
#include "ov12/rg_weapon_db.h"
typedef struct RgBattleNames {
    char characterName[64];
    RgVector position;
    RgVector direction;
    union {
        char names[3][64];
        unsigned long long words[3][8];
    } weaponStorage;
} RgBattleNames;
typedef struct RgBattleInit {
    RgBattleNames players[2];
    char background[64];
    int stageKind;
    int enemyLevel;
} RgBattleInit;
extern int sRender[];
extern int s_stack[];
extern void assert_prog(const char *expression, const char *file, int line);
typedef struct RgNamedWeaponEssence {
    RgWeaponEssence common;
    unsigned char unmodeled_018[0x318];
    char name[1];
} RgNamedWeaponEssence;
extern int RgSelectGetEnemyChar(void);
extern int RgSelectGetEnemyWeapons(RgNamedWeaponEssence **weapons, int charId);
extern const char *RgActorCharIDToName(int charId);
extern char *strcpy(char *destination, const char *source);

static void _GetEnemyChar(RgBattleNames *names, int player)
{
    RgNamedWeaponEssence *weapons[3];
    int charId;
    unsigned int i;

    charId = RgSelectGetEnemyChar();
    if (RgSelectGetEnemyWeapons(weapons, charId) != 0) {
        i = 0;
        while (i < 3) {
            strcpy(names[player].weaponStorage.names[i], weapons[i]->name);
            i++;
        }
    }
    strcpy(names[player].characterName, RgActorCharIDToName(charId));
}

extern const unsigned long long D_00A51530[];
extern const unsigned long long D_00A51538[];
extern const unsigned long long D_00A51540[];
extern const unsigned long long D_00A51548[];
extern const unsigned long long D_00A51558[];
extern const unsigned long long D_00A51560[];
extern const unsigned long long D_00A51568[];
extern const unsigned long long D_00A51570[];
extern const unsigned long long D_00A51578[];
extern const unsigned long long D_00A51580[];
extern const unsigned long long D_00A51588[];
extern const unsigned long long D_00A51590[];
extern const unsigned long long D_00A51598[];
extern const unsigned long long D_00A515A0[];
extern const unsigned long long D_00A515B0[];
extern const unsigned long long D_00A515B8[];
extern const unsigned long long D_00A515C0[];

static int _GetHardEnemyChar(RgBattleNames *names, int player, unsigned int stage)
{
    switch (stage) {
    case 0:
        strcpy(names[player].characterName, RgActorCharIDToName(2));
        __builtin_memcpy(names[player].weaponStorage.words[0], D_00A51530, 8);
        __builtin_memcpy(names[player].weaponStorage.words[1], D_00A51538, 7);
        __builtin_memcpy(names[player].weaponStorage.words[2], D_00A51540, 8);
        break;
    case 1:
        strcpy(names[player].characterName, RgActorCharIDToName(3));
        __builtin_memcpy(names[player].weaponStorage.words[0], D_00A51548, 9);
        __builtin_memcpy(names[player].weaponStorage.words[1], D_00A51558, 8);
        __builtin_memcpy(names[player].weaponStorage.words[2], D_00A51560, 7);
        break;
    case 2:
        strcpy(names[player].characterName, RgActorCharIDToName(1));
        __builtin_memcpy(names[player].weaponStorage.words[0], D_00A51568, 8);
        __builtin_memcpy(names[player].weaponStorage.words[1], D_00A51570, 8);
        __builtin_memcpy(names[player].weaponStorage.words[2], D_00A51578, 8);
        break;
    case 3:
        strcpy(names[player].characterName, RgActorCharIDToName(4));
        __builtin_memcpy(names[player].weaponStorage.words[0], D_00A51580, 7);
        __builtin_memcpy(names[player].weaponStorage.words[1], D_00A51580, 7);
        __builtin_memcpy(names[player].weaponStorage.words[2], D_00A51588, 8);
        break;
    case 9999:
        strcpy(names[player].characterName, RgActorCharIDToName(5));
        __builtin_memcpy(names[player].weaponStorage.words[0], D_00A51590, 7);
        __builtin_memcpy(names[player].weaponStorage.words[1], D_00A51598, 8);
        __builtin_memcpy(names[player].weaponStorage.words[2], D_00A515A0, 9);
        break;
    case 9998:
        strcpy(names[player].characterName, RgActorCharIDToName(0));
        __builtin_memcpy(names[player].weaponStorage.words[0], D_00A515B0, 8);
        __builtin_memcpy(names[player].weaponStorage.words[1], D_00A515B8, 7);
        __builtin_memcpy(names[player].weaponStorage.words[2], D_00A515C0, 8);
        break;
    default:
        _GetEnemyChar(names, player);
        break;
    }
}

extern const char D_00A515C8[];
extern const char D_00A515E0[];
extern const char D_00A515F0[];

static void _InitDataBase(void)
{
    RgReadText *robotText;

    robotText = CreateRgReadText(D_00A515C8);
    RgRobotDBRead(InstanceOfRgRobotDB(), robotText);
    DisposeRgReadText(robotText);
    RgWeaponDBClear(InstanceOfRgWeaponDB());
    RgShotDBClear(InstanceOfRgShotDB());
    RgShotDBRead(InstanceOfRgShotDB(), D_00A515E0);
    RgWeaponDBRead(InstanceOfRgWeaponDB(), D_00A515F0);
}

typedef struct RgTitle RgTitle;
extern RgFileSys *InstanceOfRgFileSys(void);
extern void RgFileSysPrepareFile(RgFileSys *fileSys, const char *path, const char *origin);
extern void RgFileSysDisposePrepares(RgFileSys *fileSys);
extern RgTitle *CreateRgTitle(int mode);
extern void DisposeRgTitle(RgTitle *title);
extern void RgTitlePassTime(RgTitle *title, float time);
extern void RgTitleDisp(RgTitle *title);
extern int RgTitleGetResult(RgTitle *title);
extern float RgGetFrameTime(void);
extern const char D_00A51600[];
extern const char D_00A51610[];

static int _Title(int mode)
{
    RgTitle *title;
    int result;

    result = 0;
    RgFileSysPrepareFile(InstanceOfRgFileSys(), D_00A51600, D_00A51610);
    title = CreateRgTitle(mode);
    while (!_IsEndOfMode() && result == 0) {
        RgTitlePassTime(title, RgGetFrameTime());
        RgTitleDisp(title);
        result = RgTitleGetResult(title);
        _EndOfFrame();
    }
    DisposeRgTitle(title);
    RgFileSysDisposePrepares(InstanceOfRgFileSys());
    return result;
}

typedef struct RgSelectState {
    int selectedCharacter;
    int availableCharacters;
    unsigned char unmodeled_08[0x240];
} RgSelectState;
extern RgSelectState D_00A59490[2];
extern int abInit_3[2];
extern const char D_00A51620[];
extern const char D_00A51630[];
extern void InitRgSelectAGWSData(RgSelectState *state);
extern int RgSelectGetPlayerChars(void);
extern RgSelectAGWS *CreateRgSelectAGWS(void);
extern void DisposeRgSelectAGWS(RgSelectAGWS *selector);
extern void RgSelectAGWSSetSelectData(RgSelectAGWS *selector, RgSelectState *state);
extern void RgSelectAGWSSetMode(RgSelectAGWS *selector, int mode);
extern void RgSelectAGWSPassTime(RgSelectAGWS *selector, float time);
extern void RgSelectAGWSDisp(RgSelectAGWS *selector);
extern int RgSelectAGWSIsEnd(RgSelectAGWS *selector);
extern void RgSelectAGWSGetSelectData(RgSelectAGWS *selector, RgSelectState *state);
extern int RgSelectAGWSGetCollectData(RgSelectAGWS *selector, RgNamedWeaponEssence **weapons);

static int _SelectOneChar(RgBattleInit *init, int player)
{
    RgNamedWeaponEssence *weapons[3];
    RgSelectAGWS *selector;
    RgSelectState *selection;
    RgBattleNames *character;
    int result;
    unsigned int i;

    result = 0;
    if (init == 0) {
        assert_prog(D_00A51620, D_00A51450, 473);
    }
    XrgSleep();
    if (abInit_3[player] == 0) {
        selection = &D_00A59490[player];
        InitRgSelectAGWSData(selection);
        D_00A59490[player].selectedCharacter = 0;
        D_00A59490[player].availableCharacters = RgSelectGetPlayerChars();
        abInit_3[player] = 1;
    }
    selector = CreateRgSelectAGWS();
    RgSelectAGWSSetSelectData(selector, &D_00A59490[player]);
    switch (player) {
    case 0:
        RgSelectAGWSSetMode(selector, 0);
        break;
    case 1:
        RgSelectAGWSSetMode(selector, 1);
        break;
    }
    while (!_IsEndOfMode()) {
        result = RgSelectAGWSIsEnd(selector);
        if (result != 0) {
            break;
        }
        RgSelectAGWSPassTime(selector, 0.033333335f);
        RgSelectAGWSDisp(selector);
        _EndOfFrame();
    }
    XrgSleep();
    if (result == 1) {
        character = &init->players[player];
        RgSelectAGWSGetSelectData(selector, &D_00A59490[player]);
        strcpy(character->characterName,
               RgActorCharIDToName(RgSelectAGWSGetCollectData(selector, weapons)));
        for (i = 0; i < 3; i++) {
            if (weapons[i] == 0) {
                character->weaponStorage.names[i][0] = D_00A51630[0];
            } else {
                strcpy(character->weaponStorage.names[i], weapons[i]->name);
            }
        }
    }
    DisposeRgSelectAGWS(selector);
    XrgSleep();
    XrgSleep();
    return result;
}

/* RgAnnounce is defined by its owner, ov12/tu077 rg_announce.c. */
typedef struct RgAnnounce RgAnnounce;
void RgAnnounceDispInit(RgAnnounce *pAnn, int kind);

static void _AnnSetRound(RgAnnounce *pAnn, u32 round)
{
    switch (round) {
    case 0:
        RgAnnounceDispInit(pAnn, 0);
        return;
    case 1:
        RgAnnounceDispInit(pAnn, round);
        return;
    case 2:
        RgAnnounceDispInit(pAnn, round);
        return;
    }
}

static void _AnnSetResult(RgAnnounce *announcement, int roundFlags, int result,
                          float damage)
{
    int winner;
    int mode;
    int resultKind;

    winner = roundFlags & 3;
    mode = roundFlags & 12;
    if (winner == 3) {
        RgAnnounceDispInit(announcement, 7);
        return;
    }
    if (mode == 4 && damage > 1.5f) {
        RgAnnounceDispInit(announcement, 9);
        return;
    }
    resultKind = result & 3;
    if (resultKind == 1) {
        if (winner == 1) {
            RgAnnounceDispInit(announcement, 5);
            return;
        }
        RgAnnounceDispInit(announcement, 6);
        return;
    }
    if (resultKind == 2) {
        if (winner == 1) {
            RgAnnounceDispInit(announcement, 6);
            return;
        }
        RgAnnounceDispInit(announcement, 5);
        return;
    }
    if (resultKind == 3) {
        if (winner == 1) {
            RgAnnounceDispInit(announcement, 12);
            return;
        }
        RgAnnounceDispInit(announcement, 13);
        return;
    }
}

static void _GameInfoInit(RgGameInfo *info)
{
    int stage;

    info->playerResult[0] = 0;
    info->playerResult[1] = 0;
    info->stage = 0;
    info->activePlayer = 0;
    for (stage = 0; stage < 4; stage++) {
        info->playTime[stage] = 0;
        info->damage[stage] = 0;
    }
}

typedef struct RgBattleMgr RgBattleMgr;
typedef struct RgRobot RgRobot;
extern float RgBattleMgrGetPlayTime(RgBattleMgr *battleMgr);
extern float RgBattleMgrGetPlayerDamage(RgBattleMgr *battleMgr, RgRobot *robot);

static void _GameInfoEndOfBattle(RgGameInfo *info, int player, RgBattleMgr *battleMgr)
{
    if (player >= 0) {
        info->playerResult[player]++;
        info->activePlayer++;
    }
    info->playTime[info->stage] += (int)(RgBattleMgrGetPlayTime(battleMgr) * 100.0f);
    info->damage[info->stage] += RgBattleMgrGetPlayerDamage(battleMgr, 0);
}

static int _GameInfoIsPlayerGetGame(RgGameInfo *info, int player)
{
    if ((unsigned int)info->playerResult[player] < 2) {
        return 0;
    }
    return 1;
}

static int _GameInfoGetCurrentPlayTime(RgGameInfo *info)
{
    return info->playTime[info->stage];
}

static float _GameInfoGetCurrentDamage(RgGameInfo *info)
{
    return info->damage[info->stage];
}

static void _GameInfoNextStage(RgGameInfo *info)
{
    info->playerResult[0] = 0;
    info->playerResult[1] = 0;
    info->stage++;
    info->activePlayer = 0;
}

static int _GameInfoGetStage(RgGameInfo *info)
{
    return info->stage;
}

static int _GameInfoIsEndOfGame(RgGameInfo *info)
{
    int result;

    result = 0;
    if ((u32)info->playerResult[0] >= 2U || (u32)info->playerResult[1] >= 2U) {
        result = 1;
    }
    return result;
}

static void _GameInfoSetDispLife(RgGameInfo *info, RgDispLife *dispLife, int winner)
{
    if (winner < 0) {
        RgDispLifeSetWin(dispLife, info->playerResult[0], info->playerResult[1], 0);
        return;
    }
    RgDispLifeSetWin(dispLife, info->playerResult[0], info->playerResult[1], 1 << winner);
}

static void _LoadBattleData(void)
{
    RgEffectEnvLoadData(InstanceOfRgEffectEnv());
    RgBattleCommonDataLoad(InstanceOfRgBattleCommonData());
    RgMotionInfoDBLoad(InstanceOfRgMotionInfoDB());
}

static void _DisposeBattleDatas(void)
{
    RgMotionInfoDBDispose(InstanceOfRgMotionInfoDB());
    RgBattleCommonDataDispose(InstanceOfRgBattleCommonData());
    RgEffectEnvDisposeData(InstanceOfRgEffectEnv());
    XrgSoundSystemDisposeSequence(0);
}

/*
 * The pause banner's blend color, read by XrgPaint2DColor with one lqc2
 * quadword (src/ov12/xrg_paint2d.c); r/g/b/a in the GS's 0-0x80 scale, same
 * per-component layout as src/ov12/rg_title.c's XrgColor. Copying it here as
 * two 64-bit halves keeps the local copy 8-byte aligned for that quadword
 * read (a 4-member int record copies through ldl/ldr instead).
 */
typedef struct XrgColorQuad {
    u64 lo;
    u64 hi;
} XrgColorQuad;

void XrgPaint2DAlpha(XrgPaint2D *paint, int blendMode);
void XrgPaint2DColor(XrgPaint2D *paint, const void *color);
void XrgPaint2DDrawXYWH(XrgPaint2D *paint, int mode, int x, int y, int width,
                        int height);

/* ov12:0x00a51640 (0x5a,0x5a,0x5a,0x7f): the pause banner's blend color. */
extern const XrgColorQuad D_00A51640;

/* ov12:0x00a51650 contains "PAUSE". */
extern const char D_00A51650[];

static void _DispPause(void)
{
    XrgPaint2D *paint;
    int fontId;
    XrgColorQuad color;

    paint = InstanceOfXrgPaint2D();
    fontId = RgBattleCommonDataDispWeaponFont(InstanceOfRgBattleCommonData());
    color = D_00A51640;
    if (fontId != 0 && paint != 0) {
        XrgPaint2DAlpha(paint, 1);
        XrgPaint2DColor(paint, &color);
        XrgPaint2DDrawXYWH(paint, 0, 0xE2, 0xCC, 0x3C, 0x1C);
        XrgPaint2DAlpha(paint, 2);
        XrgPaint2DColor(paint, &color);
        XrgPaint2DDrawXYWH(paint, 0, 0xE4, 0xCE, 0x38, 0x18);
        RgFontStr(paint, fontId, D_00A51650, 0xEC, 0xD2, 0);
    }
}

extern const char D_00A51658[];

static void _disp_hardmode(void)
{
    int weaponFont;

    weaponFont = RgBattleCommonDataDispWeaponFont(InstanceOfRgBattleCommonData());
    if (weaponFont == 0) {
        return;
    }
    RgFontStr(InstanceOfXrgPaint2D(), weaponFont, D_00A51658, 8, 0x64, 0);
}

extern const char D_00A51668[];
extern void RgBattleMgrSetPlayerControl(RgBattleMgr *battle, int control);
void RgAnnounceDispInitTime(RgAnnounce *pAnn, int kind, float duration);
void RgBattleMgrPassTime(RgBattleMgr *battle, float dt);
void RgAnnouncePassTime(RgAnnounce *pAnn, float dt);
void RgAnnounceSetStageClear(RgAnnounce *pAnn, int time, float damage);
void RgBattleMgrControl(RgBattleMgr *battle);
void RgBattleMgrDisp(RgBattleMgr *battle);
void RgAnnounceDisp(RgAnnounce *pAnn);
int RgBattleMgrGetResult(RgBattleMgr *battle);
int RgBattleMgrGetMode(RgBattleMgr *battle);
void RgBattleMgrActivateTimer(RgBattleMgr *battle, int which);
void RgBattleMgrInitBattle(RgBattleMgr *battle, int init);
RgDispLife *RgBattleMgrGetDispLife(RgBattleMgr *battle);
static inline void _dispInitHelper7(RgAnnounce *ann, unsigned int kind)
{
    RgAnnounceDispInit(ann, kind);
}

static int _OneBattle(RgBattleMgr *battle, RgBattleInit *init, RgGameInfo *info, int difficulty)
{
    int sp0;
    int sp4;
    int sp8;
    RgAnnounce *temp_19;
    unsigned int nFour;
    RgDispLife *temp_2_2;
    float temp_f0;
    float temp_f2t;
    float var_f21;
    unsigned int temp_16;
    int temp_16_2;
    unsigned int temp_21;
    int temp_2;
    int temp_2_3;
    int temp_3;
    int var_22;
    unsigned int var_17;

    sp4 = 0;
    sp8 = -1;
    nFour = 4;
    if (battle == (RgBattleMgr *)0) {
        assert_prog(D_00A51668, D_00A51450, 0x2F3);
    }
    var_17 = 0;
    temp_19 = CreateRgAnnounce();
    RgBattleMgrInitBattle(battle, (int)init);
    temp_2_2 = RgBattleMgrGetDispLife(battle);
    var_f21 = 1.0f;
    _GameInfoSetDispLife(info, temp_2_2, -1);
    var_22 = 0;
    temp_21 = RgBattleMgrGetMode(battle);
    while ((temp_2 = _IsEndOfMode()) == 0 && var_17 != 9) {
        temp_f0 = RgGetFrameTime();
        var_f21 -= temp_f0;
        sp0 = 1;
        switch (var_17) {
        case 0:
            var_17 = 1;
            _AnnSetRound(temp_19, (u32) info->activePlayer);
            /* fallthrough */
        case 1:
            if (var_f21 < 0.0f) {
                var_f21 = 1.5f;
                var_17 = 2;
                RgAnnounceDispInit(temp_19, 3);
            }
            break;
        case 2:
            if (var_f21 < 0.0f) {
                var_17 = 3;
                RgAnnounceDispInitTime(temp_19, 4, 0.5f);
                RgBattleMgrSetPlayerControl(battle, 1);
            }
            break;
        case 3:
            var_22 = RgBattleMgrGetResult(battle);
            temp_2_3 = _DoAnyOnePausing(temp_21);
            sp8 = temp_2_3;
            var_17 = (temp_2_3 >= 0) ? 4U : var_17;
            if (var_22 != 0) {
                RgBattleMgrActivateTimer(battle, 0);
                var_f21 = 3.0f;
                var_17 = 5;
                switch (var_22 & 3) {
                case 1:
                    _GameInfoEndOfBattle(info, 0, battle);
                    _GameInfoSetDispLife(info, temp_2_2, 0);
                    break;
                default:
                    _GameInfoEndOfBattle(info, -1, battle);
                    break;
                case 2:
                    _GameInfoEndOfBattle(info, 1, battle);
                    _GameInfoSetDispLife(info, temp_2_2, 1);
                    break;
                }
            }
            break;
        case 5:
            _AnnSetResult(temp_19, var_22, RgBattleMgrGetMode(battle), var_f21);
            if (!(var_f21 >= 0.0f)) {
                RgBattleMgrSetPlayerControl(battle, 0);
                temp_16 = var_22 & 3;
                switch (temp_21) {
                case 1: {
                    if (temp_21 == temp_16 && _GameInfoIsPlayerGetGame(info, 0) != 0) {
                        var_17 = 7;
                        temp_16_2 = _GameInfoGetCurrentPlayTime(info);
                        /* playtime */
                        temp_f2t = _GameInfoGetCurrentDamage(info);
                        /* damage */
                        var_f21 = 5.0f;
                        RgAnnounceDispInit(temp_19, 0xA);
                        RgAnnounceSetStageClear(temp_19, temp_16_2, temp_f2t);
                    } else {
                        if (temp_16 == 2 && _GameInfoIsPlayerGetGame(info, 1) != 0) {
                            var_f21 = 4.0f;
                            var_17 = 6;
                            RgAnnounceDispInit(temp_19, 8);
                        } else {
                            var_17 = 9;
                        }
                    }
                }
                break;
                case 0:
                    var_17 = 9;
                    /* fallthrough */
                case 2:
                    var_17 = 9;
                    /* fallthrough */
                default:
                    var_17 = 9;
                    break;
                case 3:
                    if (_GameInfoIsEndOfGame(info) != 0) {
                        RgAnnounceDispInit(temp_19, 8);
                        var_f21 = 4.0f;
                        var_17 = 6;
                    } else {
                        var_17 = 9;
                    }
                    break;
                }
            }
            break;
        case 6:
            if (var_f21 <= 0.0f) {
                var_17 = 9;
            }
            XrgPadSetID(0);
            var_17 = (XrgPadIsStart() != 0) ? 9U : var_17;
            XrgPadSetID(1);
            if (XrgPadIsStart() != 0) {
                var_17 = 9;
            }
            break;
        case 7:
            XrgPadSetID(0);
            if ((var_f21 <= 0.0f) || (XrgPadIsStart() != 0)) {
                if (_GameInfoGetStage(info) == 3) {
                    var_f21 = 60.0f;
                    var_17 = 8U;
                    RgAnnounceDispInit(temp_19, 0xB);
                    RgAnnounceSetGameClear(temp_19, (u32 *) &info->playTime[0], &info->damage[0], (u32) nFour);
                } else {
                    var_17 = 9;
                }
            }
            if (difficulty != 0) {
                _disp_hardmode();
            }
            break;
        case 8:
            XrgPadSetID(0);
            if (XrgPadIsStart() != 0) {
                var_17 = 9;
            } else if (var_f21 <= 0.0f) {
                var_17 = 9;
            }
            if (difficulty != 0) {
                _disp_hardmode();
            }
            break;
        case 4:
            sp0 = 0;
            var_17 = (_IsEndOfPause(sp8) != 0) ? 3U : var_17;
            break;
        }
        if (sp0 != 0) {
            RgBattleMgrControl(battle);

            RgBattleMgrPassTime(battle, temp_f0);
            RgAnnouncePassTime(temp_19, temp_f0);
        }
        if (sp4 != 0) {
            RgBattleMgrDisp(battle);
        } else {
            sp4 = 1;
        }
        RgAnnounceDisp(temp_19);
        if (var_17 == 4) {
            _DispPause();
        }
        _EndOfFrame();
    }
    XrgSleep();
    XrgSleep();
    XrgSoundSystemStop();
    DisposeRgAnnounce(temp_19);
    return temp_2;
}

static void _GetEnemyChar(RgBattleNames *names, int player);
static int _GetHardEnemyChar(RgBattleNames *names, int player,
                             unsigned int stage);
extern void XrgSoundSystemPlayBGM(int sequence);
extern RgBattleMgr *CreateRgBattleMgr(void);
extern void DisposeRgBattleMgr(RgBattleMgr *battle);
extern void RgBattleMgrSetPlayerControl(RgBattleMgr *battle, int control);
static int _OneBattle(RgBattleMgr *battle, RgBattleInit *init, RgGameInfo *info, int difficulty);

static int _OneGame(RgBattleInit *init, RgGameInfo *info, int difficulty)
{
    RgBattleMgr *battle;
    int continuing;

    continuing = 1;
    XrgSoundSystemPlayBGM(0);
    while (continuing && !_GameInfoIsEndOfGame(info)) {
        battle = CreateRgBattleMgr();
        continuing = _OneBattle(battle, init, info, difficulty) == 0;
        RgBattleMgrSetPlayerControl(battle, 0);
        DisposeRgBattleMgr(battle);
    }
    return continuing;
}

static int _SelectOneChar(RgBattleInit *init, int player);
extern const char D_00A516A8[];
extern const char D_00A516C8[];
extern const char D_00A51700[];
extern const char D_00A51718[];
extern const char D_00A51728[];
extern const char D_00A51738[];
extern const char D_00A51740[];
extern const char D_00A51750[];
extern const char D_00A51760[];
extern const char D_00A51770[];
extern const char D_00A51788[];
extern const char D_00A517A0[];
extern const char D_00A517B8[];
extern void XrgLog(const char *format, const char *file, int line, ...);
extern double fptodp(float value);
extern void InitRgBattleInit(RgBattleInit *init);
extern void XrgSoundSystemStopSequence(void);

static void _VsCpuGame(int difficulty)
{
    RgBattleInit init;
    RgGameInfo info;
    int serial;

    InitRgBattleInit(&init);
    _LoadBattleData();
    if (difficulty != 0) {
        RgWarn(D_00A516A8, D_00A51450, 1039);
    }
    if (_SelectOneChar(&init, 0) == 1) {
        _GameInfoInit(&info);
        while ((unsigned int)_GameInfoGetStage(&info) < 4U) {
            serial = _GameInfoGetStage(&info);
            serial %= 4;
            if (difficulty != 0) {
                init.enemyLevel = _GetEnemyHardLevelBySerial(serial);
            } else {
                init.enemyLevel = _GetEnemyLevelBySerial(serial);
            }
            if (difficulty != 0) {
                _GetStageDataBySerial(serial, &init);
            } else {
                _GetStageDataBySerial(serial, &init);
            }
            init.stageKind = 1;
            if (difficulty != 0) {
                _GetHardEnemyChar(init.players, 1, serial);
            } else {
                _GetEnemyChar(init.players, 1);
            }
            XrgLog(D_00A516C8, D_00A51450, 1091);
            XrgLog(D_00A51700, D_00A51450, 1092, init.background, serial);
            XrgLog(D_00A51718, D_00A51450, 1093, init.stageKind);
            XrgLog(D_00A51728, D_00A51450, 1094, init.enemyLevel);
            XrgLog(D_00A51738, D_00A51450, 1095);
            XrgLog(D_00A51740, D_00A51450, 1096, &init);
            XrgLog(D_00A51750, D_00A51450, 1097,
                   fptodp(init.players[0].position[0]),
                   fptodp(init.players[0].position[1]),
                   fptodp(init.players[0].position[2]));
            XrgLog(D_00A51760, D_00A51450, 1098,
                   fptodp(init.players[0].direction[0]),
                   fptodp(init.players[0].direction[1]),
                   fptodp(init.players[0].direction[2]));
            XrgLog(D_00A51770, D_00A51450, 1099, init.players[0].weaponStorage.names[0]);
            XrgLog(D_00A51788, D_00A51450, 1100, init.players[0].weaponStorage.names[1]);
            XrgLog(D_00A517A0, D_00A51450, 1101, init.players[0].weaponStorage.names[2]);
            XrgLog(D_00A517B8, D_00A51450, 1102, &init.players[1]);
            XrgLog(D_00A51750, D_00A51450, 1103,
                   fptodp(init.players[1].position[0]),
                   fptodp(init.players[1].position[1]),
                   fptodp(init.players[1].position[2]));
            XrgLog(D_00A51760, D_00A51450, 1104,
                   fptodp(init.players[1].direction[0]),
                   fptodp(init.players[1].direction[1]),
                   fptodp(init.players[1].direction[2]));
            XrgLog(D_00A51770, D_00A51450, 1105, init.players[1].weaponStorage.names[0]);
            XrgLog(D_00A51788, D_00A51450, 1106, init.players[1].weaponStorage.names[1]);
            XrgLog(D_00A517A0, D_00A51450, 1107, init.players[1].weaponStorage.names[2]);
            XrgLog(D_00A516C8, D_00A51450, 1108);
            if (_OneGame(&init, &info, difficulty) == 0 ||
                !_GameInfoIsPlayerGetGame(&info, 0)) {
                break;
            }
            _GameInfoNextStage(&info);
        }
        XrgSoundSystemStopSequence();
    }
    _DisposeBattleDatas();
}

static void _2PGame(void)
{
    int result[2];
    RgBattleInit init;
    RgGameInfo info;

    InitRgBattleInit(&init);
    _LoadBattleData();
    result[0] = 0;
    result[1] = 0;
    do {
        result[0] = _SelectOneChar(&init, 0);
        if (result[0] == 2) {
            break;
        }
        result[1] = _SelectOneChar(&init, 1);
    } while (result[1] != 1);
    _GetStageData(4, &init);
    init.stageKind = 3;
    if (result[0] == 1 && result[1] == result[0]) {
        _GameInfoInit(&info);
        _OneGame(&init, &info, 0);
    }
    XrgSoundSystemStopSequence();
    _DisposeBattleDatas();
}

typedef struct RgHelp RgHelp;
extern RgHelp *CreateRgHelp(void);
extern void DisposeRgHelp(RgHelp *help);
extern void RgHelpPassTime(RgHelp *help, float deltaTime);
extern void RgHelpDisp(RgHelp *help);
extern int RgHelpIsEnd(RgHelp *help);
extern const char D_00A517C8[];

static void _Help(void)
{
    RgHelp *help;
    int done;

    done = 0;
    RgFileSysPrepareFile(InstanceOfRgFileSys(), D_00A517C8, D_00A51610);
    help = CreateRgHelp();
    while (!done && !_IsEndOfMode()) {
        RgHelpPassTime(help, RgGetFrameTime());
        RgHelpDisp(help);
        done = RgHelpIsEnd(help);
        _EndOfFrame();
    }
    DisposeRgHelp(help);
    RgFileSysDisposePrepares(InstanceOfRgFileSys());
}

static void _push(void)
{
    int i;
    int stateWord;

    for (i = 0; i < 8; i++) {
        stateWord = sRender[i + 9];
        sRender[i + 9] = 0;
        s_stack[i] = stateWord;
    }
}

static void _pop(void)
{
    int i;

    for (i = 0; i < 8; i++) {
        sRender[i + 9] = s_stack[i];
    }
}

static void _push(void);
static void _pop(void);
static int _Title(int mode);
static void _VsCpuGame(int difficulty);
static void _2PGame(void);
static void _Help(void);
extern void XrgSystemInit(void);
extern void XrgSystemDispose(void);
extern void InitXrgSoundSystem(void);
extern void DisposeXrgSoundSystem(void);
extern void RgDrawCreateDrawStudioFullScreen(RgDraw *draw);
extern void RgFileSysClear(RgFileSys *fileSys);
extern void RgError(const char *message, const char *file, int line, ...);
extern const char D_00A517D8[];

void RobotGameMain(void)
{
    int running;
    int titleCount;

    running = 1;
    _push();
    titleCount = 0;
    XrgSleep();
    XrgSystemInit();
    InitXrgSoundSystem();
    while (running) {
        int selection;

        XrgSystemInit();
        XrgSoundSystemPlayBGM(3);
        _InitDataBase();
        selection = _Title(titleCount++);
        XrgSleep();
        switch (selection) {
        case 1:
            _VsCpuGame(0);
            break;
        case 2:
            _VsCpuGame(1);
            break;
        case 3:
            _2PGame();
            break;
        case 4:
            _Help();
            break;
        case 0:
            running = 0;
            break;
        case 5:
            running = 0;
            break;
        default:
            RgError(D_00A517D8, D_00A51450, 1258);
            break;
        }
        RgDrawCreateDrawStudioFullScreen(InstanceOfRgDraw());
        RgFileSysClear(InstanceOfRgFileSys());
        XrgSystemDispose();
        XrgSleep();
        XrgSleep();
    }
    DisposeXrgSoundSystem();
    _pop();
}

void RobotGameMainDebug(void)
{
}

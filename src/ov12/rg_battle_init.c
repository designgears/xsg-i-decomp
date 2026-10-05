/*
 * OV12 original TU 49: 0x00a2af20..0x00a2b5e8 (6 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_battle_init.h"
#include "ov12/xrg_rand_int.h"

extern void assert_prog(const char *expression, const char *source_file,
                        int line);

extern const char D_00A54E18[]; /* "pInfo != NIL" */
extern const char D_00A54E28[]; /* "../rg_battle_init.euc.c" */
extern const char D_00A54E48[];
extern const unsigned char D_00A54E40[];
extern void XrgSetVectorXYZ(RgVector destination, float x, float y, float z);
extern char *strcpy(char *destination, const char *source);

void InitRgBattleInit(RgBattleInitState *pInfo)
{
    unsigned int playerIndex;
    unsigned int weaponIndex;

    if (pInfo == 0) {
        assert_prog(D_00A54E18, D_00A54E28, 0x1E);
    }
    for (playerIndex = 0; playerIndex < 2; playerIndex++) {
        pInfo->players[playerIndex].actorName[0] = D_00A54E40[0];
        XrgClearVector(pInfo->players[playerIndex].position);
        XrgClearVector(pInfo->players[playerIndex].direction);
        pInfo->players[playerIndex].direction[2] = 1.0f;
        for (weaponIndex = 0; weaponIndex < 3; weaponIndex++) {
            pInfo->players[playerIndex].weaponName[weaponIndex][0] =
                D_00A54E40[0];
        }
    }
    XrgSetVectorXYZ(pInfo->players[0].position, 0.0f, 2.0f, -10.0f);
    XrgSetVectorXYZ(pInfo->players[1].position, 0.0f, 2.0f, 10.0f);
    XrgSetVectorXYZ(pInfo->players[0].direction, 0.0f, 0.0f, 1.0f);
    XrgSetVectorXYZ(pInfo->players[1].direction, 0.0f, 0.0f, -1.0f);
    pInfo->bg[0] = D_00A54E40[0];
    pInfo->battleMode = 0;
    pInfo->enemyType = 0;
}

void RgBattleInitCopy(RgBattleInitState *pDst,
                      RgBattleInitState *pSrc)
{
    int playerIndex;
    int weaponCount;
    char (*pSrcWeapon)[64];
    char (*pDstWeapon)[64];

    if ((pDst == 0) || (pSrc == 0)) {
        assert_prog(D_00A54E48, D_00A54E28, 57);
    }
    strcpy(pDst->bg, pSrc->bg);
    for (playerIndex = 0; playerIndex < 2; playerIndex++) {
        strcpy(pDst->players[playerIndex].actorName,
               pSrc->players[playerIndex].actorName);
        XrgCopyVector(pDst->players[playerIndex].position,
                      pSrc->players[playerIndex].position);
        XrgCopyVector(pDst->players[playerIndex].direction,
                      pSrc->players[playerIndex].direction);
        pSrcWeapon = pSrc->players[playerIndex].weaponName;
        pDstWeapon = pDst->players[playerIndex].weaponName;
        for (weaponCount = 2; weaponCount >= 0; weaponCount--) {
            strcpy(*pDstWeapon, *pSrcWeapon);
            pDstWeapon++;
            pSrcWeapon++;
        }
    }
}

typedef struct RgPlayerEssence RgPlayerEssence;
typedef struct RgPlayer RgPlayer;
typedef struct RgBattleInitEssence {
    RgVector position;
    RgVector direction;
    void *spec;
    void *weaponEssence[3];
    void *spareWeaponEssence[3];
    int actorID;
} RgBattleInitEssence;
extern int strcmp(const char *a, const char *b);
extern RgSimpleDB *InstanceOfRgWeaponDB(void);
extern RgSimpleDB *InstanceOfRgRobotDB(void);
extern void InitRgPlayerEssence(RgPlayerEssence *pDat);
extern RgPlayer *CreateRgPlayer(RgPlayerEssence *pDat);
extern int RgActorNameToCharID(const char *pszName);
extern void *RgRobotDBGetByID(RgSimpleDB *database, int charID);
extern void *RgWeaponDBGetEssence(RgSimpleDB *pDB, const char *pszName);
extern const char D_00A54E68[];
static inline void *GetSpare(RgSimpleDB *db, void *spec)
{
    if (spec == 0) {
        return RgWeaponDBGetEssence(db, D_00A54E68);
    }
    return RgWeaponDBGetEssence(db, (char *)spec + 0x98);
}

unsigned int RgBattleInitCreatePlayers(RgBattleInitState *pInfo,
                                       RgPlayer **ppPlayers,
                                       unsigned int max)
{
    RgSimpleDB *weaponDB;
    RgBattleInitEssence essence;
    unsigned int count;
    unsigned int i;
    unsigned int j;
    char *spec;
    void *w;
    RgBattleInitPlayerInfo *player;
    void **weapon;
    void **spare;

    weaponDB = InstanceOfRgWeaponDB();
    if (pInfo == 0) {
        assert_prog(D_00A54E18, D_00A54E28, 0x52);
    }
    count = 0;
    i = 0;
    if (max != 0) {
    player = pInfo->players;
    do {
        if (strcmp(player->actorName, (const char *)D_00A54E40) != 0) {
            InitRgPlayerEssence((RgPlayerEssence *)&essence);
            essence.actorID = RgActorNameToCharID(player->actorName);
            XrgCopyVector(essence.position, player->position);
            XrgCopyVector(essence.direction, player->direction);
            essence.spec = RgRobotDBGetByID(InstanceOfRgRobotDB(), essence.actorID);
            for (j = 0; j < 3; j++) {
                w = RgWeaponDBGetEssence(weaponDB, player->weaponName[j]);
                spec = essence.spec;
                essence.weaponEssence[j] = w;
                essence.spareWeaponEssence[j] = GetSpare(weaponDB, spec);
            }
            ppPlayers[count++] = CreateRgPlayer((RgPlayerEssence *)&essence);
        }
        player++;
    } while (++i < 2 && count < max);
    }
    return count;
}

/* Defined later in this TU (a local sibling still in asm). */
static void *_CreateBg(int bgId, void *dst);

void RgBattleInitCreateBg(RgBattleInitInfo *pInfo, int bgId)
{
    if (pInfo == 0) {
        assert_prog(D_00A54E18, D_00A54E28, 0x83);
    }
    _CreateBg(bgId, pInfo->bg);
}

extern const char D_00A54E70[];
extern const char D_00A54E80[];
extern const char D_00A54EA8[];
extern void *CreateRgReadText(const char *path);
extern void RgWarn(const char *fmt, const char *file, int line, ...);
extern void *CreateRgBgBuilder(int bgId);
extern void RgBgBuilderBuildFromText(void *builder, void *text, void *name);
extern void *RgBgBuilderGetLoadData(void *builder);
extern void *InstanceOfGlobalLight(void);
extern void RgBgBuilderGetLight(void *builder, void *light);
extern void DisposeRgBgBuilder(void *builder);
extern void DisposeRgReadText(void *text);

static void *_CreateBg(int bgId, void *dst)
{
    void *text;
    void *builder;
    void *data;

    text = CreateRgReadText(D_00A54E70);
    if (bgId == 0 || dst == 0) {
        assert_prog(D_00A54E80, D_00A54E28, 0x94);
    }
    if (text == 0) {
        RgWarn(D_00A54EA8, D_00A54E28, 0x97, D_00A54E70);
        return 0;
    }
    builder = CreateRgBgBuilder(bgId);
    RgBgBuilderBuildFromText(builder, text, dst);
    data = RgBgBuilderGetLoadData(builder);
    RgBgBuilderGetLight(builder, InstanceOfGlobalLight());
    DisposeRgBgBuilder(builder);
    DisposeRgReadText(text);
    return data;
}

extern const char D_00A54EC0[];
extern const char D_00A54ED0[];
extern const char D_00A54EF0[];
extern const char D_00A54F00[];
extern const char D_00A54F10[];
extern const char D_00A54F28[];
extern void XrgLog(const char *format, const char *source_file, int line, ...);
extern double fptodp(float value);

void RgBattleInitDump(RgBattleInitState *pDst)
{
    int i;
    int j;
    RgBattleInitPlayerInfo *player;

    if (pDst == 0) {
        assert_prog(D_00A54EC0, D_00A54E28, 0xAD);
    }
    XrgLog(D_00A54ED0, D_00A54E28, 0xAF, pDst);
    XrgLog(D_00A54EF0, D_00A54E28, 0xB0, pDst->bg);
    for (i = 0; i < 2; i++) {
        char *weapon;
        player = &pDst->players[i];
        j = 0;
        XrgLog(D_00A54F00, D_00A54E28, 0xB3, player->actorName);
        weapon = player->weaponName[0];
        XrgLog(D_00A54F10, D_00A54E28, 0xB4,
               fptodp(player->position[0]),
               fptodp(player->position[1]),
               fptodp(player->position[2]));
        for (; j < 3; j++) {
            XrgLog(D_00A54F28, D_00A54E28, 0xB6, j, weapon);
            weapon += 64;
        }
    }
}

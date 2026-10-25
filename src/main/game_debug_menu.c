#include "common.h"
#include "shared.h"
#include "game_debug_menu.h"

static void PauseMenuPage0(void)
{
    int y;
    int i;

    if (PadData.half_2a & PAD_CROSS) {
        GameResourceDump(0);
    }

    xglFontDebugPrintf(136, 52, "WrkEnd:%8x", WorkEnd);

    y = 64;
    for (i = 0; i < 16; i++) {
        xglFontDebugPrintf(8, y, D_004D9EF8, actor[i].inUseId);
        y += 8;
    }

    GameResourceDump(1);
}

#define PAD_UP 0x1000
#define PAD_DOWN 0x4000
#define PAD_LEFT 0x8000
#define PAD_RIGHT 0x2000
#define PAD_CIRCLE 0x0020
#define CF_CAMERA_SWITCH 0x02
#define RUNTIME_MODEL_NUMBER_TAG 0x4
#define RUNTIME_CAPTURE_ALPHA 0x02000000
#define ACTOR_CHECK_HIDDEN 0x8
#define MAP_UNIT_CHECK_HIDDEN 0x4
#define MODEL_CHECK_ACTOR 0x200
#define MODEL_CHECK_MAP_UNIT 0x300
#define DEBUG_FLAG_COUNT 7
#define MAP_UNIT_COUNT 64
typedef struct {
    u8 pad_00[0x28];
    u16 half_28;
    u16 half_2a;
    u16 half_2c;
    u16 half_2e;
} PauseFlagsPadView;
typedef struct {
    unsigned char unmodeled_00[0x10];
    int runtimeFlags;                       /* +0x10 */
    unsigned char unmodeled_14[0xc1 - 0x14];
    unsigned char cfCameraFlags;            /* +0xc1 */
    unsigned char unmodeled_c2[0xd0 - 0xc2];
    signed char snapQuality;                /* +0xd0 */
    unsigned char unmodeled_d1[0x29f51 - 0xd1];
    unsigned char debugPageInitialized;     /* +0x29f51 */
    unsigned char unmodeled_29f52[0x29f71 - 0x29f52];
    signed char debugCursor;                /* +0x29f71 */
    union {
        unsigned short modelNumberCheck;
    } target;                               /* +0x29f72 */
    unsigned char unmodeled_29f74[0x29f80 - 0x29f74];
    struct {
        unsigned int actorFlag8Clear[2];
    } scratch;                              /* +0x29f80 */
    unsigned char unmodeled_29f88[0x29f90 - 0x29f88];
    unsigned int mapUnitFlag4Clear[2];      /* +0x29f90 */
} PauseFlagsPageState;
typedef struct {
    u32 flags;
    u8 unmodeled_04[0x300 - 0x04];
} MapUnitHead;
extern PauseFlagsPageState PauseFlagsState __asm__("GameLoopState");
extern PauseFlagsPadView PauseFlagsPad __asm__("PadData");
extern MapUnitHead MapUnit[MAP_UNIT_COUNT];
extern unsigned char SnapDrawCreditFlag;
extern u8 GameDiskChangeT10K;
extern char D_004C03A8[];
extern char D_004D9F00[];
extern char D_004D9F08[];
extern char D_004D9F10[];
extern char *tbl_0_00367A50[];

static void PauseMenuPage2(void)
{
    int i;
    int y;
    int step;
    int stepMagnitude;
    int quality;
    unsigned int right;
    unsigned int left;
    unsigned int *word;
    unsigned int bit;

    if (PauseFlagsState.debugPageInitialized == 0) {
        int slot;
        PauseFlagsState.debugPageInitialized = 1;
        PauseFlagsState.target.modelNumberCheck = MODEL_CHECK_ACTOR;
        for (slot = 0; slot < ACTOR_COUNT; slot++) {
            word = &PauseFlagsState.scratch.actorFlag8Clear[slot >> 5];
            bit = 1 << (slot & 0x1f);
            if (actor[slot].flags & ACTOR_CHECK_HIDDEN) {
                *word &= ~bit;
            } else {
                *word |= bit;
            }
        }
        for (slot = 0; slot < MAP_UNIT_COUNT; slot++) {
            word = &PauseFlagsState.mapUnitFlag4Clear[slot >> 5];
            bit = 1 << (slot & 0x1f);
            if (MapUnit[slot].flags & MAP_UNIT_CHECK_HIDDEN) {
                *word &= ~bit;
            } else {
                *word |= bit;
            }
        }
    }

    if (PauseFlagsPad.half_2c & PAD_UP) {
        PauseFlagsState.debugCursor--;
    }
    if (PauseFlagsPad.half_2c & PAD_DOWN) {
        PauseFlagsState.debugCursor++;
    }
    if (PauseFlagsState.debugCursor < 0) {
        PauseFlagsState.debugCursor = DEBUG_FLAG_COUNT - 1;
    }
    if (PauseFlagsState.debugCursor >= DEBUG_FLAG_COUNT) {
        PauseFlagsState.debugCursor = 0;
    }

    switch (PauseFlagsState.debugCursor) {
    case 0:
        if (PauseFlagsPad.half_2c & (PAD_LEFT | PAD_RIGHT | PAD_CIRCLE)) {
            PauseFlagsState.cfCameraFlags ^= CF_CAMERA_SWITCH;
        }
        break;
    case 1:
        if (PauseFlagsPad.half_2c & (PAD_LEFT | PAD_RIGHT | PAD_CIRCLE)) {
            PauseFlagsState.runtimeFlags ^= RUNTIME_MODEL_NUMBER_TAG;
        }
        break;
    case 2:
        if (PauseFlagsPad.half_2c & (PAD_LEFT | PAD_RIGHT | PAD_CIRCLE)) {
            SnapDrawCreditFlag ^= 1;
        }
        break;
    case 3: {
        int step;
        int quality;
        unsigned int right;
        unsigned int left;

        stepMagnitude = 1;
        right = PauseFlagsPad.half_2c & PAD_RIGHT;
        left = PauseFlagsPad.half_2c & PAD_LEFT;
        step = right != 0;
        if (left) {
            step = -stepMagnitude;
        }
        quality = step + PauseFlagsState.snapQuality;
        if (quality < -5) {
            quality = -5;
        }
        if (quality > 100) {
            quality = 100;
        }
        PauseFlagsState.snapQuality = quality;
        break;
    }
    case 4:
        right = PauseFlagsPad.half_2c & PAD_RIGHT;
        left = PauseFlagsPad.half_2c & PAD_LEFT;
        step = right != 0;
        if (left) {
            step = -1;
        }
        if (PauseFlagsPad.half_2a & 0x40) {
            PauseFlagsState.target.modelNumberCheck ^= 0x100;
        }
        PauseFlagsState.target.modelNumberCheck += step;
        if (PauseFlagsPad.half_28 & PAD_CIRCLE) {
            int slot;
            unsigned int kind;

            for (slot = 0; slot < ACTOR_COUNT; slot++) {
                actor[slot].flags |= ACTOR_CHECK_HIDDEN;
            }
            for (slot = 0; slot < MAP_UNIT_COUNT; slot++) {
                MapUnit[slot].flags |= MAP_UNIT_CHECK_HIDDEN;
            }
            kind = PauseFlagsState.target.modelNumberCheck & 0xff00;
            slot = PauseFlagsState.target.modelNumberCheck & 0xff;
            switch (kind) {
            case MODEL_CHECK_ACTOR:
                actor[slot].flags &= ~ACTOR_CHECK_HIDDEN;
                break;
            case MODEL_CHECK_MAP_UNIT:
                MapUnit[slot].flags &= ~MAP_UNIT_CHECK_HIDDEN;
                break;
            }
        }
        if (PauseFlagsPad.half_2e & PAD_CIRCLE) {
            int slot;
            unsigned int *saved;
            unsigned int (*actorSaved)[2];
            unsigned int clearMask;

            for (slot = 0; slot < ACTOR_COUNT; slot++) {
                unsigned int bit = 1 << (slot & 0x1f);
                clearMask = ~ACTOR_CHECK_HIDDEN;
                actorSaved = &PauseFlagsState.scratch.actorFlag8Clear;
                if ((*actorSaved)[slot >> 5] & bit) {
                    actor[slot].flags &= clearMask;
                } else {
                    actor[slot].flags |= ACTOR_CHECK_HIDDEN;
                }
            }
            saved = PauseFlagsState.mapUnitFlag4Clear;
            clearMask = ~MAP_UNIT_CHECK_HIDDEN;
            for (slot = 0; slot < MAP_UNIT_COUNT; slot++) {
                if (saved[slot >> 5] & (1 << (slot & 0x1f))) {
                    MapUnit[slot].flags &= clearMask;
                } else {
                    MapUnit[slot].flags |= MAP_UNIT_CHECK_HIDDEN;
                }
            }
        }
        break;
    case 5:
        if (PauseFlagsPad.half_2c & (PAD_LEFT | PAD_RIGHT | PAD_CIRCLE)) {
            PauseFlagsState.runtimeFlags ^= RUNTIME_CAPTURE_ALPHA;
        }
        break;
    case 6:
        if (PauseFlagsPad.half_2c & (PAD_LEFT | PAD_RIGHT | PAD_CIRCLE)) {
            GameDiskChangeT10K ^= 1;
        }
        break;
    }

    xglFontDebugPrintf(8, 52, D_004C03A8);
    y = 64;
    for (i = 0; i < DEBUG_FLAG_COUNT; i++) {
        if (i == PauseFlagsState.debugCursor) {
            xglFontDebugPrintf(16, y, D_004D9F00);
        }
        xglFontDebugPrintf(32, y, tbl_0_00367A50[i]);
        switch (i) {
        case 0:
            xglFontDebugPrintf(128, y + 3, D_004D9F08,
                               (PauseFlagsState.cfCameraFlags >> 1) & 1);
            break;
        case 1:
            xglFontDebugPrintf(128, y + 3, D_004D9F08,
                               (PauseFlagsState.runtimeFlags >> 2) & 1);
            break;
        case 2:
            xglFontDebugPrintf(128, y + 3, D_004D9F08, SnapDrawCreditFlag);
            break;
        case 3:
            xglFontDebugPrintf(128, y + 3, D_004D9F10, PauseFlagsState.snapQuality);
            break;
        case 4:
            xglFontDebugPrintf(128, y + 3, D_004D9EF8, PauseFlagsState.target.modelNumberCheck);
            break;
        case 5:
            xglFontDebugPrintf(128, y + 3, D_004D9F08,
                               (PauseFlagsState.runtimeFlags >> 25) & 1);
            break;
        case 6:
            xglFontDebugPrintf(128, y + 3, D_004D9F08, GameDiskChangeT10K);
            break;
        }
        y += 12;
    }
}

#define PAD_UP 0x1000
#define PAD_DOWN 0x4000
#define PAD_LEFT 0x8000
#define PAD_RIGHT 0x2000
#define PAD_CIRCLE 0x0020
#define DEBUG_ROW_COUNT 10
#define MAP_COUNT 2000
#define SCRIPT_ID_EVENT_BASE 0x10000000
#define GAME_RESOURCE_COUNT 128
typedef struct {
    u8 pad_00[0x28];
    u16 half_28;
    u16 half_2a;
    u16 half_2c;
    u16 half_2e;
} PadDebugView;
typedef struct {
    unsigned char unmodeled_00[0x10];
    float x;
    float y;
    float z;
    unsigned char unmodeled_1c[0x54 - 0x1c];
    float angle;
    unsigned char unmodeled_58[0x9e4 - 0x58];
    float layoutAngle;
} DebugPlayerUnit;
typedef struct {
    unsigned char unmodeled_00[0x04];
    DebugPlayerUnit *player;
    unsigned char unmodeled_08[0x0c - 0x08];
    short mode;
    unsigned char unmodeled_0e[0x10 - 0x0e];
    int runtimeFlags;
    unsigned char unmodeled_14[0x18 - 0x14];
    int scriptId;
    unsigned char unmodeled_1c[0x50 - 0x1c];
    unsigned short currentMapId;
    unsigned char unmodeled_52[0xc1 - 0x52];
    unsigned char cfCameraFlags;
    unsigned char unmodeled_c2[0xd0 - 0xc2];
    signed char snapQuality;
    unsigned char unmodeled_d1[0x29f50 - 0xd1];
    unsigned char debugPage;
    unsigned char debugPageInitialized;
    unsigned char unmodeled_29f52[0x29f71 - 0x29f52];
    signed char debugCursor;
    union {
        unsigned short modelNumberCheck;
        short mapChangeId;
    } target;
    unsigned short leaderBattleId;
    unsigned char unmodeled_29f76[0x29f80 - 0x29f76];
    union {
        unsigned int actorFlag8Clear[2];
        struct {
            unsigned short cfId;
            unsigned short eventId;
        } scriptJump;
    } scratch;
    unsigned char unmodeled_29f88[0x29f90 - 0x29f88];
    unsigned int mapUnitFlag4Clear[2];
} DebugMenuPageState;
typedef struct {
    unsigned char unmodeled_00[0x100a4];
    unsigned short leaderBattleId;
} DebugSavedParty;
typedef struct {
    u32 address;
    int size;
    unsigned char unmodeled_08[0x10 - 0x08];
} DebugResourceSlot;
typedef struct {
    float components[4];
} DebugLayoutHeader;
extern DebugMenuPageState GameLoopPage __asm__("GameLoopState");
extern PadDebugView PadDataDbg __asm__("PadData");
extern DebugResourceSlot GameResourceDbg[GAME_RESOURCE_COUNT] __asm__("GameResource");
extern unsigned char ModelPath;
extern unsigned char ScenePath;
extern u8 UseTestPath;
extern int ScanLineInterpolate;
extern int DebugMenuResourceSizeAdd;
extern char *tbl_1_00367A70[];
extern char D_004C04B0[];
extern char D_004D9F18[];
extern char D_004D9F20[];
extern char D_004C0528[];
extern char D_004C0538[];
extern char D_004D9F48[];
extern char D_004C0548[];
extern char D_004D9F28[];
extern char D_004D9F30[];
extern char D_004D9F38[];
extern char *MapGetName(int mapId);
extern void MapChange(int mapId);
extern DebugLayoutHeader *UnduDataGetHeader(int mapIndex, int unitIndex);
extern void GameCfPlayerLoadResource(int resource);
extern int GameSnapShotNumber(int number);
extern void GameStateSave(void);
extern void GameStateRestore(void);
extern void func_A22668(void);
extern void Java_xeno_util_Runtime_jump_sub(int target, int argument);
extern char *MAP_getPath(void);
extern char *RES_getPath(void);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

static void PauseMenuPage3(void)
{
    int i;
    int y;
    int dir;
    int number;
    int last;
    int used;
    int remain;
    unsigned int repeat;
    DebugPlayerUnit *player;
    DebugLayoutHeader *header;
    char *text;

    if (GameLoopPage.debugPageInitialized == 0) {
        GameLoopPage.debugPageInitialized = 1;
        GameLoopPage.target.mapChangeId = GameLoopPage.currentMapId;
        GameLoopPage.leaderBattleId = ((DebugSavedParty *)SaveData)->leaderBattleId;
        GameLoopPage.scratch.scriptJump.cfId = 0;
        GameLoopPage.scratch.scriptJump.eventId = 0;
    }

    repeat = PadDataDbg.half_2c;
    if (repeat & PAD_UP) {
        GameLoopPage.debugCursor--;
    }
    if (repeat & PAD_DOWN) {
        GameLoopPage.debugCursor++;
    }
    if (GameLoopPage.debugCursor < 0) {
        GameLoopPage.debugCursor = DEBUG_ROW_COUNT - 1;
    }
    if (GameLoopPage.debugCursor >= DEBUG_ROW_COUNT) {
        GameLoopPage.debugCursor = 0;
    }

    dir = 0;
    if (repeat & PAD_LEFT) {
        dir = -1;
    }
    if (repeat & PAD_RIGHT) {
        dir = 1;
    }
    if (PadDataDbg.half_28 & PAD_CROSS) {
        dir *= 100;
    }
    if (dir != 0) {
        switch (GameLoopPage.debugCursor) {
        case 0:
            for (;;) {
                GameLoopPage.target.mapChangeId += dir;
                if (GameLoopPage.target.mapChangeId < 0) {
                    GameLoopPage.target.mapChangeId = MAP_COUNT - 1;
                }
                if (GameLoopPage.target.mapChangeId >= MAP_COUNT) {
                    GameLoopPage.target.mapChangeId = 0;
                }
                if (MapGetName(GameLoopPage.target.mapChangeId) == 0) {
                    dir = dir > 0 ? 1 : -1;
                    continue;
                }
                break;
            }
            break;
        case 1:
            GameLoopPage.leaderBattleId += dir;
            break;
        case 3:
            number = GameSnapShotNumber(-1) + dir;
            if (number < 0) {
                number = 0;
            }
            GameSnapShotNumber(number < 10 ? number : 9);
            break;
        case 4:
            ModelPath = (ModelPath + dir) & 1;
            break;
        case 5:
            UseTestPath = (UseTestPath + dir + 3) % 3;
            break;
        case 6:
            ScenePath = (ScenePath + dir + 20) % 20;
            break;
        case 7:
            GameLoopPage.scratch.scriptJump.cfId =
                (GameLoopPage.scratch.scriptJump.cfId + dir + 10000) % 10000;
            break;
        case 8:
            GameLoopPage.scratch.scriptJump.eventId =
                (GameLoopPage.scratch.scriptJump.eventId + dir + 10000) % 10000;
            break;
        case 9:
            ScanLineInterpolate = (ScanLineInterpolate + dir + 3) % 3;
            break;
        }
    }

    if (PadDataDbg.half_2a & PAD_CIRCLE) {
        switch (GameLoopPage.debugCursor) {
        case 0: {
            DebugPlayerUnit *leader;

            MapChange(GameLoopPage.target.mapChangeId);
            leader = GameLoopPage.player;
            leader->x = 0;
            leader->y = 0;
            leader->z = 0;
            leader->angle = 0;
            header = UnduDataGetHeader(513, 0x8000);
            if (header != 0) {
                leader->x = header->components[0];
                leader->y = header->components[1];
                leader->z = header->components[2];
                leader->layoutAngle = leader->angle = header->components[3];
            }
            break;
        }
        case 1:
            ((DebugSavedParty *)SaveData)->leaderBattleId = GameLoopPage.leaderBattleId;
            GameCfPlayerLoadResource(0);
            break;
        case 2:
            GameStateSave();
            GameLoopPage.mode = 5;
            func_A22668();
            GameStateRestore();
            GameLoopPage.debugPageInitialized = 0;
            break;
        case 7:
            Java_xeno_util_Runtime_jump_sub(GameLoopPage.scratch.scriptJump.cfId, -1);
            break;
        case 8:
            Java_xeno_util_Runtime_jump_sub(
                SCRIPT_ID_EVENT_BASE + GameLoopPage.scratch.scriptJump.eventId, -1);
            GameLoopPage.runtimeFlags &= ~1;
            break;
        }
        PadDataDbg.half_2a = 0;
    }

    xglFontDebugPrintf(8, 52, D_004C04B0);
    last = GAME_RESOURCE_COUNT - 1;
    while (GameResourceDbg[last].address == 0 && last != 0) {
        last--;
    }
    remain = GameResourceDbg[last].size;
    used = 0;
    for (last--; last >= 0; last--) {
        used += GameResourceDbg[last].size;
    }
    xglFontDebugPrintf(136, 48, "USED  :%9d", used + DebugMenuResourceSizeAdd);
    if (DebugMenuResourceSizeAdd != 0) {
        xglFontDebugPrintf(246, 48, D_004D9F18);
    }
    xglFontDebugPrintf(136, 56, "REMAIN:%9d", remain);

    y = 64;
    for (i = 0; i < DEBUG_ROW_COUNT; i++) {
        if (i == GameLoopPage.debugCursor) {
            xglFontDebugPrintf(8, y + 3, D_004D9F20);
        }
        xglFontDebugPrintf(16, y, tbl_1_00367A70[i]);
        switch (i) {
        case 0:
            xglFontDebugPrintf(80, y + 3, D_004D9F28, GameLoopPage.target.mapChangeId,
                               MapGetName(GameLoopPage.target.mapChangeId));
            break;
        case 1:
            xglFontDebugPrintf(80, y + 3, D_004D9EF8, GameLoopPage.leaderBattleId);
            break;
        case 3:
            xglFontDebugPrintf(72, y + 3, "game%c000", GameSnapShotNumber(-1) + '0');
            break;
        case 4:
            if (ModelPath == 0) {
                xglFontDebugPrintf(92, y + 3, "data\\[path]");
            } else {
                xglFontDebugPrintf(92, y + 3, "data\\test\\[path]");
            }
            break;
        case 5:
            xglFontDebugPrintf(92, y + 3, D_004D9F30, MAP_getPath());
            break;
        case 6:
            xglFontDebugPrintf(92, y + 3, D_004D9F30, RES_getPath());
            break;
        case 7:
            xglFontDebugPrintf(88, y + 3, D_004D9F38, GameLoopPage.scratch.scriptJump.cfId);
            break;
        case 8:
            xglFontDebugPrintf(88, y + 3, D_004D9F38, GameLoopPage.scratch.scriptJump.eventId);
            {
                char fileName[7] = "cf0000";
                int scriptId = GameLoopPage.scriptId;

                if (scriptId <= 0x0fffffff) {
                    fileName[0] = 'c';
                    fileName[1] = 'f';
                } else {
                    fileName[0] = 'e';
                    fileName[1] = 'v';
                    scriptId &= 0xffff;
                }
                fileName[5] = scriptId % 10 + '0';
                scriptId /= 10;
                fileName[4] = scriptId % 10 + '0';
                scriptId /= 10;
                fileName[3] = scriptId % 10 + '0';
                scriptId /= 10;
                fileName[2] = scriptId % 10 + '0';
                xglFontDebugPrintf(120, y - 3, fileName);
            }
            break;
        case 9:
            switch (ScanLineInterpolate) {
            case 0:
            default:
                text = D_004D9F48;
                break;
            case 1:
                text = D_004C0528;
                break;
            case 2:
                text = D_004C0538;
                break;
            }
            xglFontDebugPrintf(80, y, text);
            break;
        }
        y += 12;
    }

    player = GameLoopPage.player;
    xglFontDebugPrintf(176, 160, D_004C0548);
    xglFontDebugPrintf(176, 172, "%f\n%f\n%f", player->x, player->y, player->z);
}

extern unsigned char GameLoopState[];
void PauseMenuPage2(void);
void PauseMenuPage3(void);
void PauseMenuPageUwamono(void);
void PauseMenuPageEnemy(void);
int PauseMenuPagePartyDebug(void);

void GameDebugMenu(void)
{
    int d;

    d = 0;
    if (PadData.half_2a & 4) {
        d = -1;
    }
    if (PadData.half_2a & 8) {
        d = 1;
    }
    if (d != 0) {
        GameLoopState[0x29F50] = (GameLoopState[0x29F50] + d + 6) % 6;
        GameLoopState[0x29F51] = 0;
    }
    switch (GameLoopState[0x29F50]) {
    case 0:
        PauseMenuPage3();
        break;
    case 1:
        PauseMenuPage0();
        break;
    case 2:
        PauseMenuPage2();
        break;
    case 3:
        PauseMenuPageUwamono();
        break;
    case 4:
        PauseMenuPageEnemy();
        break;
    case 5:
        if (PauseMenuPagePartyDebug()) {
        }
        break;
    }
}

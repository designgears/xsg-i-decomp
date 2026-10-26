#include "common.h"
#include "shared.h"
#include "game.h"
extern u8 actSequence[0x9800];
extern float D_004D7BA0;

/*
 * GNU EE native TI storage/copy type. Used only for the
 * 16-byte aligned typed storage that the original clears with por+sq; no
 * wide arithmetic.
 */
typedef unsigned int Quadword __attribute__((mode(TI)));

/*
 * 16-byte quadword-aligned vector storage, cleared through the union's quad
 * member. The wrapping union (rather than a bare Quadword field) matters for
 * GameModeDebugMenu's schedule: a store through a union member has alias set
 * 0
 */
typedef union QuadVector {
    Quadword quad;
} QuadVector;

/*
 * PARTIAL ACCESSED VIEW of GameLoopState (0x2a030-byte object at 0x338680).
 * Union of the offsets the merged handlers access:
 *  +0x0c kind u16, +0x10 flags u32, +0x28 camera_callback,
 *  +0x29f40 status u8 (read by GameModeCfEvent),
 *  +0x29f50 pause_vector 16-byte quadword (cleared with por+sq),
 *  +0x29f60 saved_kind u16.
 * Spans named _unmodeled_* are opaque sizing bytes never accessed by these
 * bodies; the remainder of the object beyond 0x29f62 is unmodeled. The name
 * is TU-local by canon
 */
/*
 * GameStateSave/GameStateRestore back up and restore two more GameLoopState
 * halfwords (main VA 0x00243278/0x002432f0): +0x0e kindDetail, the halfword
 * GameStateSave copies alongside +0x0c kind, and the +0xe0/+0xe2 backup slot
 * GameStateSave writes them into and GameStateRestore reads back to restore
 * kind and kindDetail.
 */
typedef struct GameLoopStateLayout {
    u8 _unmodeled_00[0x0c];
    u16 kind;
    u16 kindDetail;
    u32 flags;
    u8 _unmodeled_14[0x14];
    GameModeCameraCallback camera_callback;
    u8 _unmodeled_2c[0xe0 - 0x2c];
    u16 restoreKind;
    u16 restoreKindDetail;
    u8 _unmodeled_e4[0x29f40 - 0xe4];
    u8 status;
    u8 _unmodeled_29f41[0x0f];
    QuadVector pause_vector;
    u16 saved_kind;
} GameLoopStateLayout;

extern GameLoopStateLayout GameLoopState;

/*
 * PadData (0xd0-byte object at 0x490d90) through the shared
 * PadDataDebugLayout view (xeno/core/types.h): the merged handlers read the
 * halfwords at +0x28 (held), +0x2a (pressed) and +0x2e (debug_buttons,
 * GameModeDebugMenu). The canonical `extern PadPrefix PadData;` of
 * xeno/core/functions.h stops at +0x2c, so that header is not included and
 * the prototypes this TU needs are repeated below in their canonical
 * spelling.
 */
extern PadDataDebugLayout PadData;

extern void xglSoundEffectNormalDirect(int effect_id);
extern void TWSYS_update(void);
extern void EvtTools(void);
extern void PauseMenu(void);
extern void PartyTimePauseEnd(void);
extern void ACT_pauseUpdate(void);
extern StudioCamera *xglStudioGetCamera2(int camera_id);
extern void JTHREAD_cntl(void);
extern void PLAY_ctrl(void);
extern void TCAMERA_update(void);
extern void ACT_update(void);
extern void MAP_updateUnit(void);
extern void GameDebugMenu(void);
int GameModeDebugMenu(void);

#define game_mode_flags (GameLoopState.flags)
#define game_mode_kind (GameLoopState.kind)
#define game_mode_status (GameLoopState.status)
#define game_mode_saved_kind (GameLoopState.saved_kind)
#define game_mode_pause_vector (GameLoopState.pause_vector.quad)
#define game_mode_camera_callback (GameLoopState.camera_callback)

extern void xglSoundSendEffect(void *swd, void *sed, int bank);
extern void After_Talk(SceneObject object);

typedef struct EnemyWorkPostTalk {
    u8 unmodeled_00[0x4e];
    u16 stiff_timer;
    short effect_timer[2];
    u8 unmodeled_54[0x37b4 - 0x54];
    u32 post_talk_flags;
    int enemy_id;
    u8 unmodeled_37bc[0x3830 - 0x37bc];
    short encount_lock;
    u8 unmodeled_3832[0x38b0 - 0x3832];
} EnemyWorkPostTalk;

extern EnemyWorkPostTalk enepc[16];

#define ACTOR_NUMBER_OFFSET 0x80

static void GameCFSoundPurgeSub(void)
{
    int index = 0;

    xglSoundSendEffect(0, 0, 2);
    xglSoundSendEffect(0, 0, 3);
    for (; index < 8; index++) {
        xglSoundSendEffect(0, 0, index + 4);
    }
}

void xglSoundSendSwd(void *swd, int id);
int xglSoundSendSmd2(void *smd, int id);
void SsdResetSegmentAllocMode(int addr);
void SsdSetSegmentAllocMode(int addr, int size);

static void GameCFSoundPurge(void)
{
    int i;

    for (i = 0; i < 8; i++) {
        xglSoundSendSwd(0, -1 - i);
        xglSoundSendSmd2(0, i);
    }
    xglSoundSendEffect(0, 0, 1);
    GameCFSoundPurgeSub();
    SsdResetSegmentAllocMode(0x70000);
    SsdSetSegmentAllocMode(0xA8000, 0x10000);
}

void xglSoundLoadEffect(const char *bankName, void *buffer, int mode);

static void GameCFSoundReload(void)
{
    int i;
    void *end = (void *)(((int)WorkEnd + 0x3F) & ~0x3F);

    for (i = 0; i < 8; i++) {
        xglSoundSendSwd(0, -1 - i);
        xglSoundSendSmd2(0, i);
    }
    SsdResetSegmentAllocMode(0xA8000);
    SsdSetSegmentAllocMode(0x70000, 0x10000);
    xglSoundLoadEffect("REGISTCF", end, 1);
    GameCFSoundPurgeSub();
}

extern u8 GameCFSoundMenuPurgeFlag;
typedef struct GameSeqSlot { u16 a; u16 seq; } GameSeqSlot;
extern GameSeqSlot SoundWork[];
int SsdGetSeqPlayStatus(int sequence);
int SsdGetResultValue(int *value);
void xglSoundSequenceStop2(int channel);
void EnemySound_StopAll(int clear);

void GameCFSoundMenuPurge(int mode)
{
    int i;
    int st;
    int seq;
    int ps;

    if (mode == 1) {
        GameCFSoundMenuPurgeFlag = 0;
        xglSoundSendEffect(0, 0, 3);
        for (i = 0; i < 8; i++) {
            seq = SoundWork[i].seq;
            if (seq != 0xFFFF) {
                SsdGetSeqPlayStatus(seq);
                while (SsdGetResultValue(&st) < 0) {
                }
                ps = (u16)st;
                if (ps == 1) {
                    GameCFSoundMenuPurgeFlag |= ps << i;
                    xglSoundSequenceStop2(i);
                }
            }
        }
    }
    EnemySound_StopAll(1);
    for (i = 4; i < 8; i++) {
        xglSoundSendEffect(0, 0, i + 4);
    }
}

extern u8 GameCFSoundMenuPurgeFlag;
extern int MenuDrillCall;
extern int UmnSimulationNo;
unsigned int GameResourceGetFreeAddr(void);
void RES_GetMapEnvSeName(char *name);
const char *RES_GetEnemySeName(int index);
void xglSoundSequenceNormal2(int channel, int volume);

void GameCFSoundMenuReload(void)
{
    char name[64];
    int i = 0;
    unsigned int addr;
    const char *se;
    int bank;

    addr = GameResourceGetFreeAddr();
    xglSoundSendSwd(0, -5);
    xglSoundSendSmd2(0, 4);
    RES_GetMapEnvSeName(name);
    xglSoundLoadEffect(name, (void *)addr, 3);
    do {
        if (((GameCFSoundMenuPurgeFlag >> i) & 1) && UmnSimulationNo == 0 && MenuDrillCall == 0) {
            xglSoundSequenceNormal2(i, 0x7F);
        }
        i++;
    } while (i < 8);
    i = 4;
    while (1) {
        se = RES_GetEnemySeName(i);
        bank = i + 4;
        i++;
        if (se == 0 || *se == 0) break;
        xglSoundLoadEffect(se, (void *)addr, bank);
    }
}

void GameStateSave(void);
void sefPushEffect(void);
typedef struct GDPSub { u8 pad[0x160]; u32 bits[32]; } GDPSub;
typedef struct GameActorCopy { u64 w[0xA70 / 8]; } GameActorCopy;

void Game_Data_Push(void)
{
    extern GameActorCopy actor[];
    GameActorCopy *src = actor;
    u8 *gs = (u8 *)&GameLoopState;
    short i;
    short k;
    int j;
    u8 *map;
    u32 *bits;
    int *tbl;
    u8 *g2;
    GameActorCopy *d;

    for (i = 0; i < 0x40; i++) {
        ((GameActorCopy *)(gs + 0x250))[i] = src[i];
    }
    GameCFSoundPurge();
    GameStateSave();
    sefPushEffect();
    for (i = 0; i < 0x20; i++) {
        u32 *w = (u32 *)&GameLoopState;
        w[i + 0x59] = 0;
    }
    g2 = (u8 *)&GameLoopState;
    map = *(u8 **)(*(u8 **)(g2 + 0x54) + 4);
    tbl = (int *)(map + 0xB0);
    for (i = 0; i < *(int *)(map + 0x44); i++) {
        u8 *e = map + tbl[i];
        if (*(u32 *)(e + 0x20) & 8) {
            int id = *(int *)(e + 0x2c);
            ((GDPSub *)((u32 *)&GameLoopState + 1))->bits[id / 32] |= 1 << id;
        }
    }
}

#define ACTOR_COUNT 64
extern int xglCdSync(void);
extern void xglSleep(void);
extern void sefInitEffectCf(void);
extern void GameResourceLoad(int);
extern void GameStateRestore(void);
extern void sefPopEffect(void);
extern void UnlockMapUnit(void);
extern void ClearUwamonoEffect(void *);
extern void ACT_setMotion2(void *, int, int);
extern void ACT_updateMotion(void *);
extern void nmlModelSetPartsVisible(void *, int, int);
typedef struct ActorHeadU {
    u32 flags;
    void (*update)(void *actor); /* +0x4 */
    unsigned char unmodeled_008[0x10 - 0x8];
    Vector4 position;
    unsigned char unmodeled_020[0x50 - 0x20];
    Vector4 rotation; /* +0x50 */
    unsigned char unmodeled_060[0x80 - 0x60];
    u8 number;
    u8 unmodeled_81;
    u8 tag_mark;
    unsigned char unmodeled_083[0x84 - 0x83];
    short state84; /* +0x84 */
    short inUseId;
    u8 unmodeled_88[0x4e8 - 0x88];
    u64 attribute_flags;
    u8 unmodeled_4f0[0x6f0 - 0x4f0];
    u32 motion_flags;
    unsigned char unmodeled_6f4[0x6f8 - 0x6f4];
    float motion_speed; /* +0x6f8 */
    unsigned char unmodeled_6fc[0x704 - 0x6fc];
    u16 motion_id;
    u8 unmodeled_706[0x70a - 0x706];
    u16 matrix_buffer_toggle;
    u8 unmodeled_70c[0x824 - 0x70c];
    void *jointMatrices;
    void *previousJointMatrices;
    void *staticValueRecords;
    void *valueRecords;
    u8 unmodeled_834[0x8fc - 0x834];
    struct Actor *parent;
    unsigned char unmodeled_900[0x9f4 - 0x900];
    int talk_message; /* +0x9f4 */
    int touch_message;
    u8 unmodeled_9fc[0xA70 - 0x9fc];
} ActorHeadU;
typedef struct MapUnitSlotU {
    u32 flags;
    void (*update)(struct MapUnitSlotU *unit);
    u8 unmodeled_08[8];
    Vector4 position;
    float rotation[4];
    float scale[4];
    u8 unmodeled_40[0xa4 - 0x40];
    s16 serial;
    u8 unmodeled_a6[0x1a0 - 0xa6];
    s16 symbol_kind;
    s16 shop_id;
    u8 unmodeled_1a4[0x300 - 0x1a4];
} MapUnitSlotU;
typedef struct ActorUndulationU {
    u8 unmodeled_00[8];
    s16 attr_mask;
} ActorUndulationU;
typedef struct LookAtPlayerActorU {
    u8 unmodeled_00[0x4c8];
    ActorUndulationU undulation;
} LookAtPlayerActorU;
typedef struct GameResourceModelPrefixU {
    unsigned char unmodeled_00[4];
    void *data;
} GameResourceModelPrefixU;
typedef struct GameLoopStateU {
    u8 _unmodeled_00[4];
    LookAtPlayerActorU *controlled_actor;
    u8 _unmodeled_08[4];
    u16 kind;
    u16 kindDetail;
    u32 flags;
    u32 base_flags;
    u32 current_event_id;
    u32 cleared_1c;
    u8 _unmodeled_20[0x54 - 0x20];
    GameResourceModelPrefixU *model;
    u8 _unmodeled_58[0xc1 - 0x58];
    u8 move_mode;
    u8 _unmodeled_c2[0x164 - 0xc2];
    u32 hidden_parts[32];
    u8 _unmodeled_1e4[0x250 - 0x1e4];
    ActorHeadU saved_actors[64];
} GameLoopStateU;
extern ActorHeadU actorU[ACTOR_COUNT] __asm__("actor");
extern MapUnitSlotU mapUnitU[64] __asm__("MapUnit");
#define gameLoopStateP ((GameLoopStateU *)&GameLoopState)

void Game_Data_Pop(void)
{
    unsigned short serials[64];
    short index;
    ActorHeadU *backup;
    void *model;
    int bit;

    for (index = 0; index < 64; index++) {
        serials[index] = mapUnitU[index].serial;
        mapUnitU[index].serial = -1;
    }
    while (xglCdSync() != 0)
        xglSleep();
    GameCFSoundReload();
    sefInitEffectCf();
    GameResourceLoad(gameLoopStateP->current_event_id);
    for (index = 0; index < 64; index++)
        mapUnitU[index].serial = serials[index];
    GameStateRestore();
    gameLoopStateP->cleared_1c = 0;
    backup = gameLoopStateP->saved_actors;

    for (index = 0; index < ACTOR_COUNT; index++, backup++) {
        actorU[index] = *backup;
        if (actorU[index].inUseId == 0)
            continue;
        {
            ActorHeadU *unit = &actorU[index];
            {
                void *matrices;

                ACT_setMotion2((struct Actor *)unit, actorU[index].motion_id, actorU[index].motion_flags & ~3);
                ACT_updateMotion((struct Actor *)unit);
                matrices = unit->jointMatrices;
                unit->jointMatrices = unit->previousJointMatrices;
                unit->previousJointMatrices = matrices;
                matrices = unit->staticValueRecords;
                unit->staticValueRecords = unit->valueRecords;
                unit->valueRecords = matrices;
                unit->matrix_buffer_toggle = (unit->matrix_buffer_toggle + 1) & 1;
            }
            {
                void *matrices;

                ACT_setMotion2((struct Actor *)unit, actorU[index].motion_id, actorU[index].motion_flags & ~3);
                ACT_updateMotion((struct Actor *)unit);
                matrices = unit->jointMatrices;
                unit->jointMatrices = unit->previousJointMatrices;
                unit->previousJointMatrices = matrices;
                matrices = unit->staticValueRecords;
                unit->staticValueRecords = unit->valueRecords;
                unit->valueRecords = matrices;
                unit->matrix_buffer_toggle = (unit->matrix_buffer_toggle + 1) & 1;
            }
        }
    }
    sefPopEffect();
    gameLoopStateP->flags &= ~0x400000;
    gameLoopStateP->flags &= ~0x800000;
    gameLoopStateP->flags &= ~0x20000000;
    gameLoopStateP->flags &= ~0x40000000;
    UnlockMapUnit();
    for (index = 0; index < 64; index++) {
        if (mapUnitU[index].serial != -1 && (mapUnitU[index].flags & 0x20000)) {
            MapUnitSlotU *map_unit = &mapUnitU[index];
            mapUnitU[index].serial = -1;
            mapUnitU[index].update = 0;
            ClearUwamonoEffect(map_unit);
        }
    }
    model = gameLoopStateP->model->data;
    for (index = 0; index < 32; index++) {
        for (bit = 0; bit < 32; bit++) {
            /* Original uses signed SRAV before selecting the one bit.
             * This scalar interpretation does not erase an interface. */
            if (((int)gameLoopStateP->hidden_parts[index] >> bit) & 1)
                nmlModelSetPartsVisible(model, index * 32 + bit, 0);
        }
    }
    {
        u32 flags = gameLoopStateP->flags;

        gameLoopStateP->controlled_actor->undulation.attr_mask |= 0x1000;
        flags &= ~0x4000;
        flags &= ~0x8000;
        flags &= ~0x40;
        gameLoopStateP->flags = flags;
        gameLoopStateP->move_mode |= 0x20;
    }
}

void MenuGameDataPush(void)
{
    extern GameActorCopy actor[];
    GameActorCopy *src = actor;
    u8 *gs = (u8 *)&GameLoopState;
    short i;

    for (i = 0; i < 0x40; i++) {
        ((GameActorCopy *)(gs + 0x250))[i] = src[i];
    }
    GameStateSave();
}

void ACT_setModelWrapper(struct Actor *a, int b);
void ACT_setHumanHand(struct Actor *a, int b);
unsigned char *PartyDataGet(void);

void MenuGameDataPop(void)
{
    short index;

    GameStateRestore();
    for (index = 0; index < ACTOR_COUNT; index++) {
        int changed = 0;
        unsigned char *party = PartyDataGet();

        if ((void *)&actorU[index] == (void *)gameLoopStateP->controlled_actor
            && gameLoopStateP->saved_actors[index].inUseId != 0
            && gameLoopStateP->saved_actors[index].inUseId != *(unsigned short *)(party + 0x2c)) {
            gameLoopStateP->saved_actors[index].inUseId = *(unsigned short *)(party + 0x2c);
            changed = 1;
        }
        actorU[index] = gameLoopStateP->saved_actors[index];
        if (actorU[index].inUseId != 0) {
            if (changed) {
                ActorHeadU *unit = &actorU[index];

                ACT_setModelWrapper((struct Actor *)unit, 0);
                ACT_setHumanHand((struct Actor *)unit, 0);
                {
                    void *matrices;

                    ACT_setMotion2((struct Actor *)unit, 0, 8);
                    ACT_updateMotion((struct Actor *)unit);
                    matrices = unit->jointMatrices;
                    unit->jointMatrices = unit->previousJointMatrices;
                    unit->previousJointMatrices = matrices;
                    matrices = unit->staticValueRecords;
                    unit->staticValueRecords = unit->valueRecords;
                    unit->valueRecords = matrices;
                    unit->matrix_buffer_toggle = (unit->matrix_buffer_toggle + 1) & 1;
                }
                {
                    void *matrices;

                    ACT_setMotion2((struct Actor *)unit, 0, 8);
                    ACT_updateMotion((struct Actor *)unit);
                    matrices = unit->jointMatrices;
                    unit->jointMatrices = unit->previousJointMatrices;
                    unit->previousJointMatrices = matrices;
                    matrices = unit->staticValueRecords;
                    unit->staticValueRecords = unit->valueRecords;
                    unit->valueRecords = matrices;
                    unit->matrix_buffer_toggle = (unit->matrix_buffer_toggle + 1) & 1;
                }
            } else {
                ACT_updateMotion((struct Actor *)&actorU[index]);
            }
        }
    }
}

typedef struct GameLightBackup { u64 w[0x1e]; } GameLightBackup;

void GameStateSaveCameraLight(void)
{
    GameLightBackup *dst = (GameLightBackup *)((u8 *)&GameLoopState + 0x29e50);
    GameCameraStateSave();
    *dst = *(GameLightBackup *)xglStudioGetLight2();
}

extern void GameCameraStateRestore(void);
extern void *xglStudioGetLight2(void);
void GameStateRestoreCameraLight(void)
{
    GameCameraStateRestore();
    *(GameLightBackup *)xglStudioGetLight2() = *(GameLightBackup *)((u8 *)&GameLoopState + 0x29e50);
}

extern void GameStateSaveCameraLight(void);
static inline void GameQCopy(void *dst, void *src)
{
    __asm__ __volatile__("lq $2, 0(%1)\n\tsq $2, 0(%0)" : : "r"(dst), "r"(src) : "$2", "memory");
}

void GameStateSave(void)
{
    u8 *gs = (u8 *)&GameLoopState;
    u8 *p;
    GameLoopState.restoreKind = GameLoopState.kind;
    GameLoopState.restoreKindDetail = GameLoopState.kindDetail;
    p = *(u8 **)(gs + 4);
    {
        u32 a = *(u32 *)(p + 0);
        u16 h = *(u16 *)(p + 0x84);
        u32 b = *(u32 *)(p + 4);
        *(u16 *)(gs + 0x1f4) = h;
        *(u32 *)(gs + 0x1f0) = a;
        *(u16 *)(gs + 0x1f6) = *(u16 *)(p + 0x86);
        *(u32 *)(gs + 0x1f8) = b;
    }
    GameQCopy(gs + 0x200, p + 0x10);
    GameQCopy(gs + 0x210, p + 0x50);
    *(u16 *)(gs + 0x220) = *(u16 *)(gs + 0x50);
    GameStateSaveCameraLight();
}

extern void GameStateRestoreCameraLight(void);
extern void GameStateRestoreKoware(void);

void GameStateRestore(void)
{
    GameLoopState.kind = GameLoopState.restoreKind;
    GameLoopState.kindDetail = GameLoopState.restoreKindDetail;
    GameStateRestoreCameraLight();
    GameStateRestoreKoware();
}

typedef struct PSUOrgRecord {
    u8 unmodeled_00[0x180];
} PSUOrgRecord;
typedef struct PSUPlayerRecord {
    u8 unmodeled_00[0xa8];
} PSUPlayerRecord;
typedef struct PSUUserSave {
    u8 unmodeled_0000[0x101b0];
    int event_id;
    float params[8];
    u8 unmodeled_101d4[0x101e0 - 0x101d4];
    Vector4 actor_position;
    Vector4 actor_rotation;
    u8 unmodeled_10200[0x103b4 - 0x10200];
    PSUOrgRecord org[33];
    PSUPlayerRecord players[16];
    u8 unmodeled_13fb4[0x14634 - 0x13fb4];
    u16 item_box[256];
    u16 weapon_box[256];
    u16 bullet_box[256];
    u16 accessory_box[256];
    u16 event_box[256];
    u8 unmodeled_15034[4];
    u64 money;
    u8 unmodeled_15040[0x16340 - 0x15040];
    u32 user_word;
    u8 unmodeled_16344[0x16350 - 0x16344];
    u64 user_longs[2];
} PSUUserSave;
typedef struct PSUStateView {
    u8 unmodeled_000[0x18];
    u32 current_event_id;
    u8 unmodeled_01c[0x60 - 0x1c];
    float params[8];
    u8 unmodeled_080[0x200 - 0x80];
    Vector4 saved_actor_position;
    Vector4 saved_actor_rotation;
    u8 unmodeled_220[0x2a018 - 0x220];
    u32 user_word;
    u8 unmodeled_2a01c[0x2a020 - 0x2a01c];
    u64 user_longs[2];
} PSUStateView;
extern unsigned short itmBox[255];
extern unsigned short wpnBox[255];
extern unsigned short bltBox[255];
extern unsigned short accBox[255];
extern unsigned short evtBox[255];
extern long long moneyBox;
extern unsigned char orgData[33][0x180];
extern unsigned char plChaData[];

void GamePushSaveDataUser(void)
{
    PSUUserSave *save;
    PSUOrgRecord *org;
    PSUPlayerRecord *players;
    PSUStateView *state = (PSUStateView *)&GameLoopState;
    int i;

    save = (PSUUserSave *)(SaveData + 0x50);
    save->event_id = state->current_event_id;
    for (i = 0; i < 8; i++) {
        save->params[i] = state->params[i];
    }
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        :
        : "r" (&save->actor_position), "r" (&state->saved_actor_position)
        : "$2", "memory");
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        :
        : "r" (&save->actor_rotation), "r" (&state->saved_actor_rotation)
        : "$2", "memory");
    org = save->org;
    players = save->players;
    for (i = 0; i < 33; i++) {
        org[i] = ((PSUOrgRecord *)orgData)[i];
    }
    for (i = 0; i < 16; i++) {
        players[i] = ((PSUPlayerRecord *)plChaData)[i];
    }
    for (i = 0; i < 255; i++) {
        save->item_box[i] = itmBox[i];
        save->weapon_box[i] = wpnBox[i];
        save->bullet_box[i] = bltBox[i];
        save->accessory_box[i] = accBox[i];
        save->event_box[i] = evtBox[i];
    }
    save->money = moneyBox;
    save->user_word = state->user_word;
    save->user_longs[0] = state->user_longs[0];
    save->user_longs[1] = state->user_longs[1];
}

extern unsigned short itmBox[255];
extern unsigned short wpnBox[255];
extern unsigned short bltBox[255];
extern unsigned short accBox[255];
extern unsigned short evtBox[255];
extern long long moneyBox;
extern unsigned char orgData[33][0x180];
extern unsigned char plChaData[];
extern void tyaDisplaySetting(int mode);
extern int xglHddActivate(int state);
void SsdSetOutputMode(int mode);
static inline void GameQCopy(void *dst, void *src);
typedef struct SaveRec180 { unsigned char b[0x180]; } SaveRec180;
typedef struct SaveRec168 { unsigned char b[0xA8]; } SaveRec168;

void GamePopSaveDataUser(void)
{
    u8 *sv = SaveData + 0x50;
    u8 *gs = (u8 *)&GameLoopState;
    u8 *sb;
    u8 *db;
    u8 *b;
    long long money;
    int i;
    int n;
    int off;
    int o2;

    sb = sv + 4;
    *(int *)(gs + 0x18) = *(int *)(sv + 0x101B0);
    {
        f32 *fd = (f32 *)(gs + 0x60);
        f32 *fs = (f32 *)(sv + 0x101B4);
        for (n = 7; n >= 0; n--) {
            *fd++ = *fs++;
        }
    }
    GameQCopy(gs + 0x200, sv + 0x101E0);
    GameQCopy(gs + 0x210, sv + 0x101F0);
    {
        u8 *s180 = sv + 0x103B4;
        u8 *s168 = sv + 0x13534;
        u8 *d180 = (u8 *)orgData;
        off = 0;
        for (n = 32; n >= 0; n--, off += 0x180) {
            __builtin_memcpy((u8 *)((int)off + (int)d180), (u8 *)((int)off + (int)s180), 0x180);
        }
        o2 = 0;
        for (n = 15; n >= 0; n--, o2 += 0xA8) {
            __builtin_memcpy((u8 *)((int)o2 + (int)plChaData), (u8 *)((int)o2 + (int)s168), 0xA8);
        }
    }
    {
        unsigned short *pItm = itmBox;
        unsigned short *pWpn = wpnBox;
        unsigned short *pBlt = bltBox;
        unsigned short *pAcc = accBox;
        unsigned short *pEvt = evtBox;
        b = sb + 0x14E30;
        for (n = 0; n < 0xFF; n++) {
            pItm[n] = *(unsigned short *)(b - 0x800);
            pWpn[n] = *(unsigned short *)(b - 0x600);
            pBlt[n] = *(unsigned short *)(b - 0x400);
            pAcc[n] = *(unsigned short *)(b - 0x200);
            pEvt[n] = *(unsigned short *)b;
            b += 2;
        }
    }
    moneyBox = *(long long *)(sv + 0x15038);
    *(int *)(gs + 0x2A018) = *(int *)(sv + 0x16340);
    *(long long *)(gs + 0x2A020) = *(long long *)(sv + 0x16350);
    *(long long *)(gs + 0x2A028) = *(long long *)(sv + 0x16358);
    {
        int mode = SaveData[0x28];
        SsdSetOutputMode(mode);
    }
    {
        u8 *pad = (u8 *)&PadData;
        pad[0x56] = SaveData[0x3C];
        pad[0xBE] = SaveData[0x3C];
    }
    tyaDisplaySetting(0);
    xglHddActivate(SaveData[0x40] ^ 1);
}

extern int UseVMFlag;
extern int saveEffe;
extern int shopEffe;
extern int evsEffe;
extern int retEffe;
extern char GameIdLight[];
extern void xglLightSetDefault(void *light);
int xglFontLoad(int, int);
void Vibration_Stop(void);
void xglPadRead(void);
void UwamonoBgmFadeOut(void);
void ACT_init(void);
void MAP_initUnit(void);
void XTK_setWindowOwner(int);
void ResetShootSys(void);
void Player_System_Init(void *);
void xglRenderCopyDisp2Draw(void);

void InitCfSystem(void)
{
  u8 *gs;
  u8 *snd;
  u8 *light;
  u8 *t = (u8 *) (&GameLoopState);
  int i;
  int j;
  u32 flags;
  float f;
  if (!((*((u32 *) (t + 0x18))) & 0xF0000000))
  {
    xglFontLoad(1, 0);
  }
  else
  {
    xglFontLoad(0, 0);
  }
  gs = (u8 *) (&GameLoopState);
  Vibration_Stop();
  i = 15;
  xglPadRead();
  do
  {
  }
  while (0);
  snd = (u8 *) SoundWork;
  *((int *) (snd + 0xa0)) = 0;
  memset(snd + 0xa4, 0, 0x800);
  EnemySound_StopAll(1);
  UwamonoBgmFadeOut();
  ACT_init();
  MAP_initUnit();
  GameCameraReset();
  f = 3.0f;
  *((u32 *) (gs + 0x20)) |= 0xF;
  flags = *((u32 *) (gs + 0x10));
  flags &= ~0x10000;
  flags &= ~0x20000;
  flags &= ~0x4000;
  flags &= ~0x8000;
  flags &= ~0x40000;
  flags &= ~0x400000;
  flags &= ~0x800000;
  flags &= ~0x20000000;
  flags &= ~0x40000000;
  i = 0x14;
  flags &= ~0x1000;
  flags &= ~0x2000;
  flags &= ~0x100000;
  flags &= ~1;
  *((u32 *) (gs + 0x10)) = flags | 0x80000;
  *((u32 *) (gs + i)) = flags;
  *((float *) (gs + 0x29f44)) = f;
  UseVMFlag = 0;
  XTK_setWindowOwner(0);
  ResetShootSys();
  sefInitEffectCf();
  saveEffe = 0;
  shopEffe = 0;
  evsEffe = 0;
  retEffe = 0;
  if ((gs && gs) && gs)
  {
  }
  xglLightSetDefault(xglStudioGetLight2());
  Player_System_Init(*((int *) (4 + gs)));
  i = 15;
  light = (u8 *) GameIdLight;
  do
  {
    xglLightSetDefault(light);
    light += 0xF0;
  }
  while ((--i) >= 0);
  j = 0x78;
  for (i = 31; i >= 0; i--)
  {
    ((int *) (&GameLoopState))[j] = 0;
    j--;
  }

  xglRenderCopyDisp2Draw();
}

extern u64 attrPrev;

typedef struct GameActorAttributeView {
    u8 unmodeled_00[0x4e8];
    u64 attribute_flags;
} GameActorAttributeView;

static int checkAttr(GameActorAttributeView *actor)
{
    u64 attributes;
    u64 selected_attributes;

    if (actor == 0) {
        return 0;
    }
    attributes = actor->attribute_flags;
    selected_attributes = attributes & 0x1f00;
    if (attributes & 0xe000) {
        return 0;
    }
    selected_attributes >>= 8;
    attrPrev = selected_attributes;
    return selected_attributes;
}

#define ACTOR_SCRIPT_FLAGS_OFFSET 0x124

/*
 * The actor record's layout beyond this field is not recovered here.
 * talktoObserver (src/main/script.c) reads the same object's flags through
 * this function and tests bit 3 (0x8) to decide whether pending pad input
 * cancels a talk in progress.
 */
int getScriptFlag(SceneObject object)
{
    int *script_flags = (int *)((SceneByte *)object + ACTOR_SCRIPT_FLAGS_OFFSET);
    return *script_flags;
}

#define ACTOR_COUNT 64
#define ACTOR_IN_USE_ID_OFFSET 0x86

typedef struct {
    u32 flags;
    u8 unmodeled_04[0]; /* +0x4: zero-length alias of the former padding member, kept for saved bodies */
    void (*update)(void *actor);
    unsigned char unmodeled_8[0x8];
    Vector4 position;
    unsigned char unmodeled_20[0x30];
    Vector4 rotation;
    unsigned char unmodeled_60[0x20];
    u8 number;
    unsigned char unmodeled_81[0x1];
    u8 tag_mark;
    unsigned char unmodeled_83[0x1];
    short state84;
    short inUseId;
    unsigned char unmodeled_88[0x460];
    u64 attribute_flags;
    unsigned char unmodeled_4f0[0x200];
    u32 motion_flags;
    unsigned char unmodeled_6f4[0x4];
    float motion_speed;
    unsigned char unmodeled_6fc[0x8];
    u16 motion_id;
    unsigned char unmodeled_706[0x4];
    u16 matrix_buffer_toggle;
    unsigned char unmodeled_70c[0x118];
    void *jointMatrices;
    void *previousJointMatrices;
    void *staticValueRecords;
    void *valueRecords;
    unsigned char unmodeled_834[0xc8];
    struct Actor *parent;
    unsigned char unmodeled_900[0xf4];
    int talk_message;
    /* +0x9f4 */
    int touch_message;
    unsigned char unmodeled_9fc[0x74];
} ActorHead;

extern ActorHead actor[ACTOR_COUNT];

typedef struct TouchActorView {
    u8 pad00[0x10];
    float x;
    float y;
    float z;
    u8 pad1c[0x80 - 0x1c];
    u8 group;
    u8 pad81[0x9f8 - 0x81];
    int active;
} TouchActorView;

float nearDir(float from, float to);

extern float atan2f(float y, float x);

extern int UseVMFlag;
int XTK_getWindowOwner(void);

static ActorHead *checkTalk(ActorHead *self)
{
    TouchActorView *me = (TouchActorView *)self;
    float pos[4];
    ActorHead *best;
    float bestDist;
    float dx, dz, d, lim, k;
    int i;

    best = 0;
    pos[0] = me->x;
    pos[1] = me->y;
    pos[2] = me->z;
    bestDist = 65535.0f;
    pos[3] = 0.25f;
    if (GameLoopState.flags & 0x400) return 0;
    if (GameLoopState.flags & 0x2000) return 0;
    if (GameLoopState.flags & 0x1000) return 0;
    if (UseVMFlag != 0) return 0;
    if (XTK_getWindowOwner() != 0) return 0;
    for (i = 0; i < ACTOR_COUNT; i++) {
        ActorHead *a = &actor[i];
        TouchActorView *v = (TouchActorView *)a;
        if (__builtin_fabsf(v->y - me->y) > 2.0f) continue;
        if (*(int *)((u8 *)a + 0x9F4) == 0) continue;
        if (!(getScriptFlag((SceneObject)a) & 0x10000)) continue;
        if (a->inUseId == 0) continue;
        if (v->group == me->group) continue;
        dx = v->x - pos[0];
        dz = v->z - pos[2];
        k = 1.0f;
        d = __builtin_fabsf(pos[3] + k);
        lim = dx * dx + dz * dz;
        lim = __builtin_fabsf(lim);
        if (!(lim < d)) continue;
        if (!(__builtin_fabsf(nearDir(atan2f(dx, dz), *(float *)((u8 *)self + 0x54))) < 1.04719758f)) continue;
        if (lim < bestDist) {
            bestDist = lim;
            best = a;
        }
    }
    return best;
}

extern float D_004D7B8C;
static ActorHead *checkTouch(ActorHead *self)
{
    TouchActorView *me = (TouchActorView *)self;
    float pos[4];
    ActorHead *best;
    float bestDist;
    float dx, dz, d, lim, k;
    int i;

    best = 0;
    pos[0] = me->x;
    pos[1] = me->y;
    pos[2] = me->z;
    pos[3] = 0.25f;
    bestDist = 65535.0f;
    if (GameLoopState.flags & 0x400) return 0;
    if (GameLoopState.flags & 0x1000) return 0;
    if (GameLoopState.flags & 0x2000) return 0;
    for (i = 0; i < ACTOR_COUNT; i++) {
        ActorHead *a = &actor[i];
        TouchActorView *v = (TouchActorView *)a;
        if (__builtin_fabsf(v->y - me->y) > 2.0f) continue;
        if (v->active == 0) continue;
        if (!(getScriptFlag((SceneObject)a) & 0x20000)) continue;
        if (a->inUseId == 0) continue;
        if (v->group == me->group) continue;
        dx = v->x;
        dx -= pos[0];
        dz = v->z;
        dz -= pos[2];
        k = D_004D7B8C;
        d = __builtin_fabsf(pos[3] + k);
        lim = dx * dx;
        lim += dz * dz;
        lim = __builtin_fabsf(lim);
        if (lim < d && lim < bestDist) {
            bestDist = lim;
            best = a;
        }
    }
    return best;
}

extern int CheckPointLine(const Vector4 *first, const Vector4 *second,
                          const Vector4 *point);
typedef union AlignedVec {
    Vector4 v;
    u64 u[2];
} AlignedVec;
typedef struct ItemBoxView {
    u8 _pad_00[0x10];
    AlignedVec position;
    u8 _pad_20[0x60];
    Matrix4 *matrix;
    u8 _pad_84[0x2c];
    AlignedVec extent;
} ItemBoxView;

int checkItemBoxDir(u8 *actor, ItemBoxView *box)
{
    Vector4 v1;
    Vector4 v2;
    AlignedVec ext;
    AlignedVec pos;
    Matrix4 *mp;

    pos = box->position;
    ext = box->extent;
    mp = box->matrix;
    v1.x = pos.v.x - (ext.v.x * (*mp)[0][0] + ext.v.z * (*mp)[2][0]);
    v1.y = pos.v.y;
    v1.z = pos.v.z - (ext.v.x * (*mp)[0][2] + ext.v.z * (*mp)[2][2]);
    v2.x = pos.v.x + (ext.v.z * (*mp)[0][2] + ext.v.x * (*mp)[2][2]);
    v2.y = pos.v.y;
    v2.z = pos.v.z - (ext.v.z * (*mp)[0][0] + ext.v.x * (*mp)[2][0]);
    return CheckPointLine(&v1, &v2, (Vector4 *)(actor + 0x10)) == 2;
}

extern float D_004D7B90;
extern float D_004D7B94;
extern float D_004D7B98;
extern u8 MapUnit[];
static ItemBoxView *checkItemBox(u8 *self)
{
    float pos[4];
    ItemBoxView *best;
    float bestDist;
    float dx, dz, ex, ez, lim, k;
    int i;

    best = 0;
    bestDist = D_004D7B90;
    pos[0] = *(float *)(self + 0x10);
    pos[1] = *(float *)(self + 0x14);
    pos[2] = *(float *)(self + 0x18);
    for (i = 0; i < 64; i++) {
        ItemBoxView *u = (ItemBoxView *)(MapUnit + i * 0x300);
        if (__builtin_fabsf(*(float *)(self + 0x14) - u->position.v.y) > D_004D7B94) continue;
        if (*(short *)((u8 *)u + 0xA4) == -1) continue;
        if (!(*(u32 *)u & 0x08000000)) continue;
        if (*((u8 *)u + 0xA2) != 0) continue;
        dx = u->position.v.x - pos[0];
        dz = u->position.v.z - pos[2];
        ex = u->extent.v.x;
        ez = u->extent.v.z;
        k = 1.0f;
        lim = dx * dx + dz * dz;
        if (ez < ex) pos[3] = ex * ex;
        else pos[3] = ez * ez;
        lim = __builtin_fabsf(lim);
        if (!(lim < __builtin_fabsf(pos[3] + k))) continue;
        if (!(__builtin_fabsf(nearDir(atan2f(dx, dz), *(float *)(self + 0x54))) < D_004D7B98)) continue;
        if (!(lim < bestDist)) continue;
        if (checkItemBoxDir(self, u) == 0) continue;
        bestDist = lim;
        best = u;
    }
    return best;
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef union Matrix Matrix;

typedef struct GameOrgRecord {
    u8 unmodeled_00[0x180];
} GameOrgRecord;

typedef struct GamePlayerRecord {
    u8 unmodeled_00[0xa8];
} GamePlayerRecord;

typedef struct GameUserSave {
    u8 unmodeled_0000[0x101b0];
    int event_id;
    float params[8];
    u8 unmodeled_101d4[0x101e0 - 0x101d4];
    Vector4 actor_position;
    Vector4 actor_rotation;
    u8 unmodeled_10200[0x103b4 - 0x10200];
    GameOrgRecord org[33];
    GamePlayerRecord players[16];
    u8 unmodeled_13fb4[0x14634 - 0x13fb4];
    u16 item_box[256];
    u16 weapon_box[256];
    u16 bullet_box[256];
    u16 accessory_box[256];
    u16 event_box[256];
    u8 unmodeled_15034[4];
    u64 money;
    u8 unmodeled_15040[0x16340 - 0x15040];
    u32 user_word;
    u8 unmodeled_16344[0x16350 - 0x16344];
    u64 user_longs[2];
} GameUserSave;

typedef union GameVector8 {
    Vector4 vector;
    u64 pair[2];
} GameVector8;

typedef struct GameMapUnit {
    u32 flags;
    u8 unmodeled_04[0x10 - 0x04];
    GameVector8 position;
    u8 unmodeled_20[0x60];
    Matrix4 *orientation;
    u8 unmodeled_84[0x1e];
    u8 action_sub;
    u8 unmodeled_a3;
    s16 serial;
    u8 unmodeled_a6[0x0a];
    GameVector8 extent;
    u8 unmodeled_c0[0x1a0 - 0xc0];
    s16 symbol_kind;
    s16 shop_number;
    u8 item_box_hit;
    u8 unmodeled_1a5[0x300 - 0x1a5];
} GameMapUnit;

typedef struct GameClearEnvironment {
    u8 unmodeled_00[0x20];
    u32 red;
    u32 green;
    u32 blue;
    u32 alpha;
} GameClearEnvironment;

typedef struct GameTestEnvironment {
    u8 unmodeled_00[0x30];
    u32 red;
    u32 green;
    u32 blue;
    u32 alpha;
    u8 unmodeled_40[0x60 - 0x40];
} GameTestEnvironment;

typedef struct GameSequenceSlot {
    u32 flags;
    u32 state_flags;
    u8 unmodeled_08[4];
    u32 cleared_on_init;
    u8 unmodeled_10[4];
    u32 cleared_on_init_run[4];
    u32 handler[4];
    u8 unmodeled_34[0x260 - 0x34];
} GameSequenceSlot;

typedef union GameDebugMenuStorage {
    Quadword storage;
    struct {
        u8 page;
        u8 page_initialized;
        u8 unmodeled_02[14];
    } fields;
} GameDebugMenuStorage;

typedef struct NmlModel {
    unsigned char unmodeled_000[0x44];
    int blockCount;
    unsigned char unmodeled_048[0x68];
    int blockOffsets[1];
} NmlModel;

typedef struct GameResourceModelPrefix {
    unsigned char unmodeled_00[4];
    NmlModel *data;
} GameResourceModelPrefix;

typedef struct MapUnitSlot {
    u32 flags;
    void (*update)(struct MapUnitSlot *unit);
    u8 unmodeled_08[8];
    /* Vector consumer at CheckGameSymbol and position producers agree on
     * four floats at+0x10; canonical map_2.h uses float[4], so this nominal
     * proposal requires defining-owner transport. Extent remains0x300. */
    Vector4 position;
    float rotation[4];
    float scale[4];
    u8 unmodeled_40[0xa4 - 0x40];
    s16 serial;
    u8 unmodeled_a6[0x1a0 - 0xa6];
    s16 symbol_kind;
    s16 shop_id;
    u8 unmodeled_1a4[0x300 - 0x1a4];
} MapUnitSlot;

typedef struct ActorUndulation {
    u8 unmodeled_00[8];
    s16 attr_mask;
} ActorUndulation;

typedef struct PlayerEffect PlayerEffect;

struct LookAtPlayerActor {
    u32 flags;
    void (*update)(struct LookAtPlayerActor *actor);
    void (*draw)(struct LookAtPlayerActor *actor);
    u32 quadword_alignment_gap;
    Vector4 position;
    Vector4 previous_position;
    Vector4 velocity;
    Vector4 external_velocity;
    float rotation_x;
    float rotation_y;
    float rotation_z;
    float move_speed;
    Vector4 scale;
    u8 unmodeled_70[0x10];
    u8 number;
    u8 unmodeled_81[0x4c8 - 0x81];
    ActorUndulation undulation;
    u8 unmodeled_4d2[0x4e8 - 0x4d2];
    u64 terrain_flags;
    u8 unmodeled_4f0[0x694 - 0x4f0];
    signed char stick_x;
    signed char stick_y;
    u8 unmodeled_696[0x6f8 - 0x696];
    float motion_speed;
    u8 unmodeled_6fc[0x704 - 0x6fc];
    u16 motion_id;
    u8 unmodeled_706[0x9a0 - 0x706];
    u32 render_flags;
    u8 unmodeled_9a4[0x9c0 - 0x9a4];
    float filter_param;
    u8 unmodeled_9c4[0x9d8 - 0x9c4];
    u64 flash_frames;
    u8 unmodeled_9e0[0x9e4 - 0x9e0];
    float aim_angle;
    float body_radius;
    s16 look_at_timer;
    s16 look_at_target;
    s16 flash_timer;
    u8 unmodeled_9f2[0xa10 - 0x9f2];
    PlayerEffect *effects[8];
    s16 effect_timer;
    s16 effect_slot;
    u8 unmodeled_a34[0xa70 - 0xa34];
};

typedef struct GameLightParallel {
    Quadword intensity;
    Quadword direction;
} GameLightParallel;

typedef struct GameLightSet {
    Quadword ambient_intensity;
    GameLightParallel parallel[3];
    Quadword matrices[8];
} GameLightSet;

typedef struct GameProjectedPoint {
    int x;
    int y;
    int z;
    float w;
} GameProjectedPoint;

typedef struct Vector4_CG {
    float x;
    float y;
    float z;
    float w;
} Vector4_CG;
typedef struct XglTaskPrefix_CG XglTaskPrefix_CG;
typedef struct XglTaskScheduler_CG XglTaskScheduler_CG;
struct XglTaskPrefix_CG {
    XglTaskScheduler_CG *scheduler;
    XglTaskPrefix_CG *next;
    XglTaskPrefix_CG *previous;
    int (*callback)(XglTaskPrefix_CG *task);
};
struct XglTaskScheduler_CG {
    XglTaskPrefix_CG *free_tasks;
    XglTaskPrefix_CG *active_head;
    XglTaskPrefix_CG *active_tail;
    XglTaskPrefix_CG *next_to_visit;
};
typedef unsigned int Quadword_CG __attribute__((mode(TI)));
typedef void (*GameModeCameraCallback_CG)(void);
typedef union GameDebugMenuStorage_CG {
    Quadword_CG storage;
    struct {
        u8 page;
        u8 page_initialized;
        u8 unmodeled_02[14];
    } fields;
} GameDebugMenuStorage_CG;
typedef struct NmlModel_CG {
    unsigned char unmodeled_000[0x44];
    int blockCount;
    unsigned char unmodeled_048[0x68];
    int blockOffsets[1];
} NmlModel_CG;
typedef struct GameResourceModelPrefix_CG {
    unsigned char unmodeled_00[4];
    struct NmlModel_CG *data;
} GameResourceModelPrefix_CG;
struct Actor_CG {
    u8 unmodeled_00[0x86];
    short resourceKind;
    u8 unmodeled_88[0x824 - 0x88];
    void *jointMatrices;
};
typedef struct ActorUndulation_CG {
    u8 unmodeled_00[8];
    s16 attr_mask;
} ActorUndulation_CG;
typedef struct ActorHead_CG {
    u32 flags;
    void (*update)(void *actor);
    unsigned char unmodeled_008[0x10 - 0x8];
    Vector4_CG position;
    unsigned char unmodeled_020[0x50 - 0x20];
    Vector4_CG rotation;
    unsigned char unmodeled_060[0x80 - 0x60];
    u8 number;
    u8 unmodeled_81;
    u8 tag_mark;
    unsigned char unmodeled_083[0x84 - 0x83];
    short state84;
    short inUseId;
    u8 unmodeled_88[0x4c8 - 0x88];
    union {
        ActorUndulation_CG undulation;
        struct {
            u8 unmodeled_undulation_00[8];
            u16 status_flags;
        };
    };
    u8 unmodeled_4d2[0x4e8 - 0x4d2];
    u64 attribute_flags;
    u8 unmodeled_4f0[0x6f0 - 0x4f0];
    u32 motion_flags;
    unsigned char unmodeled_6f4[0x6f8 - 0x6f4];
    float motion_speed;
    unsigned char unmodeled_6fc[0x704 - 0x6fc];
    u16 motion_id;
    u8 unmodeled_706[0x70a - 0x706];
    u16 matrix_buffer_toggle;
    u8 unmodeled_70c[0x824 - 0x70c];
    void *jointMatrices;
    void *previousJointMatrices;
    void *staticValueRecords;
    void *valueRecords;
    u8 unmodeled_834[0x8fc - 0x834];
    struct Actor_CG *parent;
    unsigned char unmodeled_900[0x9e4 - 0x900];
    float target_angle;
    u8 unmodeled_9e8[0x9ec - 0x9e8];
    short lookat_timer;
    short lookat_target;
    short value_9f0;
    u8 unmodeled_9f2[0x9f4 - 0x9f2];
    int talk_message;
    int touch_message;
    u8 unmodeled_9fc[0xA70 - 0x9fc];
} ActorHead_CG;
typedef struct PlayerEffect_CG PlayerEffect_CG;
struct LookAtPlayerActor_CG {
    u32 flags;
    void (*update)(struct LookAtPlayerActor_CG *actor);
    void (*draw)(struct LookAtPlayerActor_CG *actor);
    u32 quadword_alignment_gap;
    Vector4_CG position;
    Vector4_CG previous_position;
    Vector4_CG velocity;
    Vector4_CG external_velocity;
    float rotation_x;
    float rotation_y;
    float rotation_z;
    float move_speed;
    Vector4_CG scale;
    u8 unmodeled_70[0x10];
    u8 number;
    u8 unmodeled_81[0x4c8 - 0x81];
    ActorUndulation_CG undulation;
    u8 unmodeled_4d2[0x4e8 - 0x4d2];
    u64 terrain_flags;
    u8 unmodeled_4f0[0x694 - 0x4f0];
    signed char stick_x;
    signed char stick_y;
    u8 unmodeled_696[0x6f8 - 0x696];
    float motion_speed;
    u8 unmodeled_6fc[0x704 - 0x6fc];
    u16 motion_id;
    u8 unmodeled_706[0x9a0 - 0x706];
    u32 render_flags;
    u8 unmodeled_9a4[0x9c0 - 0x9a4];
    float filter_param;
    u8 unmodeled_9c4[0x9d8 - 0x9c4];
    u64 flash_frames;
    u8 unmodeled_9e0[0x9e4 - 0x9e0];
    float aim_angle;
    float body_radius;
    s16 look_at_timer;
    s16 look_at_target;
    s16 flash_timer;
    u8 unmodeled_9f2[0xa10 - 0x9f2];
    PlayerEffect_CG *effects[8];
    s16 effect_timer;
    s16 effect_slot;
    u8 unmodeled_a34[0xa70 - 0xa34];
};
typedef struct GameLightParallel_CG {
    Quadword_CG intensity;
    Quadword_CG direction;
} GameLightParallel_CG;
typedef struct GameLightSet_CG {
    Quadword_CG ambient_intensity;
    GameLightParallel_CG parallel[3];
    Quadword_CG matrices[8];
} GameLightSet_CG;
typedef struct GameLoopStateLayout_CG {
    u8 *saved_work_end;
    struct LookAtPlayerActor_CG *controlled_actor;
    XglTaskScheduler_CG *scheduler;
    u16 kind;
    u16 kindDetail;
    union {
        struct {
            u32 flags;
            u32 base_flags;
        };
        u64 flags_wide;
    };
    u32 current_event_id;
    union {
        u32 cleared_1c;
        u32 event_sub_id;
    };
    u32 shoot_control;
    u8 _unmodeled_24[4];
    GameModeCameraCallback_CG camera_callback;
    void (*clear_callback)(void);
    void (*map_draw_callback)(void);
    void (*actor_draw_callback)(void);
    void (*shadow_draw_callback)(void);
    void (*unit_draw_callback)(void);
    void (*frame_hook)(int stage);
    unsigned char unmodeled_044[0x50 - 0x44];
    u16 map_number;
    unsigned char unmodeled_052[0x54 - 0x52];
    GameResourceModelPrefix_CG *model;
    int state58;
    int state5c;
    float params[8];
    float vector80[4];
    float vector90[4];
    float vectorA0[4];
    int stateB0;
    u8 _unmodeled_b4[4];
    int frame_countdown;
    unsigned char unmodeled_0bc[0xc1 - 0xbc];
    u8 move_mode;
    u8 _unmodeled_c2[6];
    short stateC8;
    u8 _unmodeled_ca[6];
    u8 stateD0;
    u8 _unmodeled_d1[0xe0 - 0xd1];
    u16 restoreKind;
    u16 restoreKindDetail;
    u8 _unmodeled_e4[0x164 - 0xe4];
    u32 hidden_parts[32];
    unsigned char unmodeled_1e4[0x1f0 - 0x1e4];
    u32 saved_actor_flags;
    short saved_actor_state84;
    short saved_actor_in_use_id;
    void (*saved_actor_update)(void *actor);
    unsigned char unmodeled_1fc[0x200 - 0x1fc];
    Vector4_CG saved_actor_position;
    Vector4_CG saved_actor_rotation;
    u16 saved_map_number;
    unsigned char unmodeled_222[0x250 - 0x222];
    ActorHead_CG saved_actors[64];
    GameLightSet_CG saved_light;
    u8 status;
    u8 _unmodeled_29f41[3];
    float shoot_range;
    u8 _unmodeled_29f48[8];
    GameDebugMenuStorage_CG debug_menu;
    u16 saved_kind;
    unsigned char unmodeled_29f62[0x29f97 - 0x29f62];
    u8 battle_flags;
    unsigned char unmodeled_29f98[0x29f9c - 0x29f98];
    ActorHead_CG *battle_enemy;
    unsigned char unmodeled_29fa0[0x2a018 - 0x29fa0];
    u32 user_word;
    unsigned char unmodeled_2a01c[0x2a020 - 0x2a01c];
    u64 user_longs[2];
} GameLoopStateLayout_CG;
#define gameLoopCG (*(GameLoopStateLayout_CG *)&GameLoopState)
#define mapUnitCG ((MapUnitSlot *)MapUnit)
extern void xglSoundEffectNormalID(int sound_id, int variant);
extern float CheckDist3D(const Vector4 *a, const Vector4 *b);
static inline Vector4 *GameSymbolPosition(int index)
{
    MapUnitSlot *unit = &mapUnitCG[index];
    return &unit->position;
}
static inline s16 *GameSymbolKindAddress(int index)
{
    return (s16 *)((u8 *)&mapUnitCG[0].symbol_kind + index * sizeof(MapUnitSlot));
}
extern void *sefCreateEffectCf(int effectNo, const Vector4 *position, const Vector4 *orientation);
extern void sefDeleteEffectCf(void *effect);
extern int shopNo;

void CheckGameSymbol(struct LookAtPlayerActor *player)
{
    int save_unit;
    int shop_unit;
    int event_unit;
    int return_unit;
    int index;
    s16 *symbol_kind;
    MapUnitSlot *unit;

    save_unit = -1;
    shop_unit = -1;
    event_unit = -1;
    return_unit = -1;
    for (index = 0; index < 64; index++) {
        unit = &mapUnitCG[index];
        if (unit->serial == -1 || (unit->flags & 0x10000) == 0)
            continue;
        {
            symbol_kind = GameSymbolKindAddress(index);
            if ((unit->flags & 0x20000000) &&
                CheckDist3D(&player->position, &mapUnitCG[index].position) < 0.7f)
                save_unit = index;
            if ((mapUnitCG[index].flags & 0x40000000) &&
                CheckDist3D(&player->position, &mapUnitCG[index].position) < 0.7f)
                shop_unit = index;
            if (*symbol_kind == 8 &&
                CheckDist3D(&player->position, GameSymbolPosition(index)) < 0.7f)
                event_unit = index;
            if (*symbol_kind == 9 &&
                CheckDist3D(&player->position, GameSymbolPosition(index)) < 0.7f)
                return_unit = index;
        }
    }

    if (save_unit != -1) {
        MapUnitSlot *save_symbol = &mapUnitCG[save_unit];
        if ((gameLoopCG.flags & 0x20000000) == 0) {
            if (saveEffe != 0)
                sefDeleteEffectCf((void *)saveEffe);
            saveEffe = (int)sefCreateEffectCf(0x2d8, &save_symbol->position, 0);
            xglSoundEffectNormalID(12, 0);
        }
        gameLoopCG.flags |= 0x20000000;
    } else {
        gameLoopCG.flags &= ~0x20000000;
        if (saveEffe != 0) {
            sefDeleteEffectCf((void *)saveEffe);
            saveEffe = 0;
        }
    }
    if (shop_unit != -1) {
        MapUnitSlot *shop_symbol = &mapUnitCG[shop_unit];
        shopNo = shop_symbol->shop_id;
        if ((gameLoopCG.flags & 0x40000000) == 0) {
            if (shopEffe != 0)
                sefDeleteEffectCf((void *)shopEffe);
            shopEffe = (int)sefCreateEffectCf(0x2d9, &shop_symbol->position, 0);
            xglSoundEffectNormalID(12, 0);
        }
        gameLoopCG.flags |= 0x40000000;
    } else {
        gameLoopCG.flags &= ~0x40000000;
        if (shopEffe != 0) {
            sefDeleteEffectCf((void *)shopEffe);
            shopEffe = 0;
        }
    }
    if (event_unit != -1) {
        MapUnitSlot *event_symbol = &mapUnitCG[event_unit];
        if ((gameLoopCG.flags & 0x400000) == 0) {
            if (evsEffe != 0)
                sefDeleteEffectCf((void *)evsEffe);
            evsEffe = (int)sefCreateEffectCf(0x2d8, &event_symbol->position, 0);
            xglSoundEffectNormalID(12, 0);
        }
        gameLoopCG.flags |= 0x400000;
    } else {
        gameLoopCG.flags &= ~0x400000;
        if (evsEffe != 0) {
            sefDeleteEffectCf((void *)evsEffe);
            evsEffe = 0;
        }
    }
    if (return_unit != -1) {
        MapUnitSlot *return_symbol = &mapUnitCG[return_unit];
        if ((gameLoopCG.flags & 0x800000) == 0) {
            if (retEffe != 0)
                sefDeleteEffectCf((void *)retEffe);
            retEffe = (int)sefCreateEffectCf(0x2d9, &return_symbol->position, 0);
            xglSoundEffectNormalID(12, 0);
        }
        gameLoopCG.flags |= 0x800000;
    } else {
        gameLoopCG.flags &= ~0x800000;
        if (retEffe != 0) {
            sefDeleteEffectCf((void *)retEffe);
            retEffe = 0;
        }
    }
}

extern void xglMatrixMul(Matrix4 destination, Matrix4 left, Matrix4 right);
extern float xglRotTransPers(void *out, void *matrix, void *point, int cameraId);
extern void xglMatrixStackUnit(void);
extern void xglMatrixStackScale(const float scale[4]);
extern void xglMatrixStackTrans(const float translation[4]);
extern void xglMatrixStackRotX(float angle);
extern void xglMatrixStackRotY(float angle);
extern void xglMatrixStackRotZ(float angle);
extern void xglMatrixStackSave(float matrix[4][4]);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern char str_0[];
extern char str_1[];
extern char D_004D8A60[];
typedef int gdt_q __attribute__((mode(TI)));
typedef union { float m[4][4]; gdt_q q[4]; } GdtMtx;
typedef struct { gdt_q q[4]; } GdtQ4;

static void GameDispTag(void)
{
    gdt_q t;
    GdtMtx mtx;
    int scr[4];
    float pos[4];
    int cam;
    int i;
    unsigned int color;

    pos[0] = 0;
    pos[3] = 1.0f;
    pos[1] = 0;
    pos[2] = 0;
    for (cam = 0; cam < 8; cam++) {
        if (xglStudioGetCamera2(cam)->active == 1) break;
    }
    for (i = 0; i < 64; i++) {
        ActorHead *a = &actor[i];
        ActorHead *p;
        if (a->inUseId == 0) continue;
        p = *(ActorHead **)((u8 *)a + 0x8FC);
        if (!(a->inUseId & 0xF000)) {
            if (p == 0) {
                GdtMtx *m = *(GdtMtx **)((u8 *)a + 0x824);
                xglMatrixMul(mtx.m, m->m, (m + 45)->m);
                mtx.m[3][3] = 1.0f;
                xglRotTransPers(scr, 0, mtx.m[3], cam);
                goto check;
            }
            goto other;
        }
        if (p == 0) goto copy;
    other:
        if (!(p->inUseId & 0xF000)) {
            GdtMtx *m = *(GdtMtx **)((u8 *)p + 0x824);
            xglMatrixMul(mtx.m, m->m, (m + 45)->m);
        } else {
        copy:
            p = *(ActorHead **)((u8 *)a + 0x824);
            t = ((GdtMtx *)p)->q[0]; mtx.q[0] = t;
            t = ((GdtMtx *)p)->q[1]; mtx.q[1] = t;
            t = ((GdtMtx *)p)->q[2]; mtx.q[2] = t;
            t = ((GdtMtx *)p)->q[3]; mtx.q[3] = t;
        }
        mtx.m[3][3] = 1.0f;
        xglRotTransPers(scr, 0, mtx.m[3], cam);
    check:
        if ((unsigned)(scr[0] - 0x7001) < 0x1FFF && scr[1] > 0x7200 && scr[1] < 0x8E00 && scr[2] > 0) {
            color = 0xFFFFFF;
            if (a->tag_mark == 1) {
                color = 0xFF9933;
            } else {
                switch (a->inUseId & 0xF000) {
                case 0x0:
                    color = 0x3399FF;
                    break;
                case 0x4000:
                    color = 0x33FF33;
                    break;
                case 0x2000:
                    color = 0xFF3333;
                    break;
                }
            }
            str_0[1] = color >> 16;
            str_0[2] = color >> 8;
            str_0[3] = color;
            xglFontDebugPrintf(((scr[0] >> 4) - 0x700) >> 1, ((scr[1] >> 4) - 0x720) >> 1, str_0, i + 0x200, scr[2]);
        }
    }
    for (i = 0; i < 64; i++) {
        u8 *u = MapUnit + i * 0x300;
        if (*(short *)(u + 0xA4) < 0) continue;
        xglMatrixStackUnit();
        xglMatrixStackScale((float *)(u + 0x30));
        xglMatrixStackTrans((float *)(u + 0x10));
        xglMatrixStackRotX(*(float *)(u + 0x20));
        xglMatrixStackRotY(*(float *)(u + 0x24));
        xglMatrixStackRotZ(*(float *)(u + 0x28));
        xglMatrixStackSave(mtx.m);
        xglRotTransPers(scr, mtx.m, pos, cam);
        if ((unsigned)(scr[0] - 0x7001) < 0x1FFF && scr[1] > 0x7200 && scr[1] < 0x8E00 && scr[2] > 0) {
            color = 0xFFFFFF;
            switch (*(short *)(u + 0xA4) & 0xF000) {
            case 0x5000:
                color = 0x7F7F7F;
                break;
            case 0x6000:
                color = 0xCC66FF;
                break;
            case 0x3000:
                color = 0xFFFF00;
                break;
            }
            str_1[1] = color >> 16;
            str_1[2] = color >> 8;
            str_1[3] = color;
            xglFontDebugPrintf(((scr[0] >> 4) - 0x700) >> 1, ((scr[1] >> 4) - 0x720) >> 1, D_004D8A60, i + 0x300, scr[2]);
        }
    }
}

/*
 * The engine's actor record (`actor`, 64-entry array at main 0x0043c1e0,
 * 0xa70-byte stride; fuller evidence in src/main/near_dir.h,
 * src/main/enemy_2.h, src/main/set_motion.h, src/main/db_light_write.h and
 * src/main/tya.c, which name the same two fields for the same reason).
 * GameDrawShadow only reads +0x00 flags (bits 0x8 and 0x20) and +0x86, the
 * in-use id ACT_create writes and ACT_update skips a slot on when it is
 * zero; the offsets between them are not evidenced by this TU and stay
 * unmodeled.
 */

/* Defined in src/main/act_3.c (main/tu265), not yet published in
 * include/main/act_3.h. */
void ACT_DrawShadowBegin(void);
void ACT_DrawShadow(ActorHead *unit);
void ACT_DrawShadowEnd(void);

void GameDrawShadow(void)
{
    ActorHead *unit;
    int i;

    unit = actor;
    ACT_DrawShadowBegin();
    for (i = ACTOR_COUNT - 1; i >= 0; i--, unit++) {
        if (unit->inUseId != 0) {
            if (!(unit->flags & 8)) {
                if (unit->flags & 0x20) {
                    ACT_DrawShadow(unit);
                }
            }
        }
    }
    ACT_DrawShadowEnd();
}

typedef struct GameClearEnvColor {
    u8 unmodeled_00[0x20];
    u32 color_r;
    u32 color_g;
    u32 color_b;
    u32 color_a;
} GameClearEnvColor;
extern GameClearEnvColor ClearEnv;
extern u32 TestEnv_2_00338620[];
extern int StudioEntrySkip;
typedef struct GameDrawSyncView {
    u8 unmodeled_00[0x10];
    union {
        u32 flags;
        u64 flags64;
    } f;
    u8 unmodeled_18[0x2c - 0x18];
    void (*clear_hook)(void);
    void (*map_hook)(void);
    void (*act_hook)(void);
    void (*shadow_hook)(void);
    void (*unit_hook)(void);
    void (*phase_hook)(int phase);
    u8 unmodeled_44[0xb8 - 0x44];
    int wait;
} GameDrawSyncView;
extern void VibrationEngine(void);
extern void xglStudioMainCameraMove(void);
extern void xglStudioMove(void);
extern void TWSYS_draw(void);
extern void MapDraw(void);
extern void ACT_draw(void);
extern void MAP_drawUnit(void);
extern void sefExecEffect(void);
extern void GameMovieMain(void);
extern int xglSoundStreamMain(void);
extern void GameSnapShotCheck(void);
extern void xglSleep(void);
extern void nmlModelSetGlobalFogReset(void);

void GameDrawSync(void)
{



    VibrationEngine();
    xglStudioMainCameraMove();
    xglStudioMove();
    if (((GameDrawSyncView *)&GameLoopState)->phase_hook != 0) {
        ((GameDrawSyncView *)&GameLoopState)->phase_hook(3);
    }
    if (((GameDrawSyncView *)&GameLoopState)->f.flags & 2) {
        if (((GameDrawSyncView *)&GameLoopState)->clear_hook != 0) {
            ((GameDrawSyncView *)&GameLoopState)->clear_hook();
        } else {
            TestEnv_2_00338620[12] = ClearEnv.color_r;
            TestEnv_2_00338620[13] = ClearEnv.color_g;
            TestEnv_2_00338620[14] = ClearEnv.color_b;
            TestEnv_2_00338620[15] = 0x80;
            sceVif1PkRef(xglPacketGetCurrent(), TestEnv_2_00338620, 6, 0, 0, 0);
        }
        TWSYS_draw();
        if (((GameDrawSyncView *)&GameLoopState)->map_hook != 0) {
            ((GameDrawSyncView *)&GameLoopState)->map_hook();
        } else {
            MapDraw();
        }
        if (((GameDrawSyncView *)&GameLoopState)->act_hook != 0) {
            ((GameDrawSyncView *)&GameLoopState)->act_hook();
        } else {
            ACT_draw();
        }
        if (((GameDrawSyncView *)&GameLoopState)->shadow_hook != 0) {
            ((GameDrawSyncView *)&GameLoopState)->shadow_hook();
        } else {
            GameDrawShadow();
        }
        if (((GameDrawSyncView *)&GameLoopState)->unit_hook != 0) {
            ((GameDrawSyncView *)&GameLoopState)->unit_hook();
        } else {
            MAP_drawUnit();
        }
        if (!(((GameDrawSyncView *)&GameLoopState)->f.flags64 & 0x4000001)) {
            sefExecEffect();
        }
        GameMovieMain();
        xglSoundStreamMain();
        if (((GameDrawSyncView *)&GameLoopState)->f.flags & 4) {
            GameDispTag();
        }
    }
    if (((GameDrawSyncView *)&GameLoopState)->phase_hook != 0) {
        ((GameDrawSyncView *)&GameLoopState)->phase_hook(4);
    }
    GameSnapShotCheck();
    StudioEntrySkip = 1;
    xglSleep();
    nmlModelSetGlobalFogReset();
    StudioEntrySkip = 0;
    if (((GameDrawSyncView *)&GameLoopState)->wait > 0) {
        ((GameDrawSyncView *)&GameLoopState)->wait--;
    }
    if (((GameDrawSyncView *)&GameLoopState)->phase_hook != 0) {
        ((GameDrawSyncView *)&GameLoopState)->phase_hook(5);
    }
}

static int GameModeEvtTools(void)
{
    if (game_mode_status == 0) {
        if (((u16)PadData.pressed & 0x800) != 0) {
            u16 buttons = PadData.held;
            game_mode_status = 16;

            if ((buttons & 0x10c) == 0x10c) {
                u32 flag_next = game_mode_flags | 0x80000000u;
                game_mode_flags = flag_next;
                return 1;
            } else {
                xglSoundEffectNormalDirect(4);
                game_mode_flags &= 0xfffffffeu;
                game_mode_pause_vector = 0;
                game_mode_kind = game_mode_saved_kind;
            }
        }
    }

    TWSYS_update();
    EvtTools();
    {
        StudioCamera *camera = xglStudioGetCamera2(0);
        if (camera->state == 4 && game_mode_camera_callback != 0) {
            game_mode_camera_callback();
        }
    }
    return 0;
}

static int GameModePause(void)
{
    if (game_mode_status == 0) {
        if (((u16)PadData.pressed & 0x800) != 0) {
            u16 buttons = PadData.held;
            game_mode_status = 16;

            if ((buttons & 0x10c) == 0x10c) {
                u32 flag_next = game_mode_flags | 0x80000000u;
                game_mode_flags = flag_next;
                return 1;
            } else {
                xglSoundEffectNormalDirect(4);
                game_mode_flags &= 0xfffffffeu;
                game_mode_pause_vector = 0;
                game_mode_kind = game_mode_saved_kind;
                PartyTimePauseEnd();
            }
        }
    }

    TWSYS_update();
    PauseMenu();
    if (game_mode_saved_kind == 2 || game_mode_saved_kind == 3) {
        ACT_pauseUpdate();
    }
    {
        StudioCamera *camera = xglStudioGetCamera2(0);
        if (camera->state == 4 && game_mode_camera_callback != 0) {
            game_mode_camera_callback();
        }
    }
    return 0;
}

static int GameModeEvtDebug(void)
{
    if (game_mode_status == 0) {
        if (((u16)PadData.pressed & 0x800) != 0) {
            u16 buttons = PadData.held;
            game_mode_status = 16;

            if ((buttons & 0x10c) == 0x10c) {
                u32 flag_next = game_mode_flags | 0x80000000u;
                game_mode_flags = flag_next;
                return 1;
            } else {
                xglSoundEffectNormalDirect(4);
                game_mode_flags &= 0xfffffffeu;
                game_mode_pause_vector = 0;
                game_mode_kind = game_mode_saved_kind;
            }
        }
    }

    TWSYS_update();
    {
        StudioCamera *camera = xglStudioGetCamera2(0);
        if (camera->state == 4 && game_mode_camera_callback != 0) {
            game_mode_camera_callback();
        }
    }
    return 0;
}

extern short pause_check;
typedef union VMSlot_CG2 {
    int i;
    float f;
    void *ref;
    unsigned char b;
    short s;
    unsigned short c;
} VMSlot_CG2;
typedef struct PadDataDebugLayout_CG2 {
    u8 _unmodeled_00[0x28];
    u16 held;
    u16 pressed;
    u16 _unmodeled_2c;
    u16 debug_buttons;
    u8 _unmodeled_30[0x50 - 0x30];
    u8 actuator[6];
    u8 vibrationReset;
    u8 _unmodeled_57[0xb8 - 0x57];
    u8 second_actuator[6];
    u8 vibrationStatus;
} PadDataDebugLayout_CG2;
typedef struct EnemyWorkPostTalk_CG2 {
    u8 unmodeled_00[0x4e];
    u16 stiff_timer;
    short effect_timer[2];
    u8 unmodeled_54[0x37b4 - 0x54];
    u32 post_talk_flags;
    int enemy_id;
    u8 unmodeled_37bc[0x3830 - 0x37bc];
    short encount_lock;
    u8 unmodeled_3832[0x38b0 - 0x3832];
} EnemyWorkPostTalk_CG2;
extern void *const stageVM;
extern const char D_004D8A68[];
extern const char D_004BE2D0[];
extern const char D_004BE2C0[];
#define PadDataCG (*(PadDataDebugLayout_CG2 *)&PadData)
extern short NowTester;
extern signed char FLAG_FRAME_60;
#define actorCG ((ActorHead_CG *)actor)
extern int prevTouch;
typedef struct GameRadarParams_CG3 {
    int x;
    int y;
    int color;
    u8 red;
    u8 green;
    u8 blue;
    u8 alpha;
} GameRadarParams_CG3;
extern float Get_Angle(const Vector4 *from, const Vector4 *to);
extern float Get_Cursol_by_Reduce_Speed_Angle_Loop(float current, float target, float speed);
extern void Actor_LookAt();
extern void Actor_LookAt_Init();
extern void Actor_LookAt_Release();
extern void ACT_updateSequence();
extern void CallMethod_I();
extern void checkTalkPoint();
extern int Enemy_After_Battle();
extern int func_00A0B5F0();
extern void func_A22668();
extern void GameCfPlayerMove();
extern void GameIdLightSet();
extern void GameRadarDraw();
extern void Java_xeno_util_Runtime_jumpCF__II();
extern void MainMenu();
extern void MSG_init();
extern void nmlModelSendMenuEnd();
extern void nmlModelSendMenuStart();
extern void nmlModelSendSignalStartBattle();
extern void PartyTimePauseStart();
extern void SCRIPT_execEntered();
extern void SCRIPT_execTouchto();
extern void SCRIPT_incCfTime();
extern int SCRIPT_sceneChangeTimeGet();
extern int SCRIPT_talkIgnoreGet();
extern void TWSYS_init();
extern int WIN_checkActiveWindow();
extern void WindowTexLoad();
extern void xglCullingIgnore();
extern void xglCullingIgnoreOff();
extern void xglSoundStreamStop();
extern void actSequenceClear();

int GameModeCfMain(void)
{
    VMSlot_CG2 umn_arguments[2];
    VMSlot_CG2 drill_arguments[2];
    ActorHead_CG *player;
    ActorHead_CG *window_owner;
    EnemyWorkPostTalk *encounter_enemy;
    int battle_result;
    int attribute;
    int touch;
    short i;
    short index;
    int countdown;
    u32 flags;
    u16 previous_kind;

    gameLoopCG.flags &= ~1;
    SCRIPT_incCfTime();
    if (PadDataCG.pressed & 0x810) {
        const u64 *button_words;
        u64 held_buttons;

        button_words = (const u64 *)&PadDataCG.held;
        __builtin_memcpy(&held_buttons, button_words, sizeof(held_buttons));
        if ((held_buttons & 0x0800010C) == 0x0800010C) {
            gameLoopCG.flags |= 0x80000000;
            return 1;
        }
        if (!(gameLoopCG.flags_wide & 0x238000) && WIN_checkActiveWindow() == 0
            && SCRIPT_sceneChangeTimeGet() == 0 && !(gameLoopCG.flags & 0x100000)
            && gameLoopCG.controlled_actor != 0
            && (encounter_enemy = &enepc[gameLoopCG.controlled_actor->number])->encount_lock == 0
            && xglCdSync() == 0 && SCRIPT_talkIgnoreGet() == 0 && gameLoopCG.status == 0) {
            if (PadDataCG.pressed & 0x800) {
                gameLoopCG.status = 0x10;
                xglSoundEffectNormalDirect(4);
                previous_kind = gameLoopCG.kind;
                gameLoopCG.flags |= 1;
                gameLoopCG.kind = 1;
                gameLoopCG.saved_kind = previous_kind;
                gameLoopCG.debug_menu.storage = 0;
                PartyTimePauseStart();
                return 0;
            }
            else if (PadDataCG.pressed & 0x10) {
                gameLoopCG.status = 0x20;
                GameStateSaveCameraLight();
                nmlModelSendMenuStart();
                xglCullingIgnore();
                MainMenu();
                xglCullingIgnoreOff();
                nmlModelSendMenuEnd();
                GameStateRestoreCameraLight();
                if (gameLoopCG.flags & 0x10000000) {
                    return 1;
                }
                if (UmnSimulationNo) {
                    const u64 *position_tail;
                    const u64 *position_words;
                    u32 event_id;

                    event_id = gameLoopCG.current_event_id;
                    gameLoopCG.user_word = event_id;
                    position_words = (const u64 *)&gameLoopCG.controlled_actor->position;
                    gameLoopCG.user_longs[0] = *position_words;
                    position_tail = (const u64 *)
                        ((const unsigned char *)position_words + sizeof(u64));
                    gameLoopCG.user_longs[1] = *position_tail;
                    umn_arguments[0].i = UmnSimulationNo & 0xFFFF;
                    umn_arguments[1].i = (u32)UmnSimulationNo >> 16;
                    Java_xeno_util_Runtime_jumpCF__II(stageVM, umn_arguments, 0);
                    return 0;
                }
                if (MenuDrillCall != 0) {
                    const u64 *position_tail;
                    const u64 *position_words;
                    u32 event_id;

                    event_id = gameLoopCG.current_event_id;
                    gameLoopCG.user_word = event_id;
                    position_words = (const u64 *)&gameLoopCG.controlled_actor->position;
                    gameLoopCG.user_longs[0] = *position_words;
                    position_tail = (const u64 *)
                        ((const unsigned char *)position_words + sizeof(u64));
                    gameLoopCG.user_longs[1] = *position_tail;
                    drill_arguments[0].i = 0xA0;
                    drill_arguments[1].i = 1;
                    Java_xeno_util_Runtime_jumpCF__II(stageVM, drill_arguments, 0);
                    return 0;
                }
            }
        }
    }
    gameLoopCG.flags = gameLoopCG.flags & ~0x200000;
    if (gameLoopCG.event_sub_id != 0) {
        const char *always_method;
        EnemyWorkPostTalk *battle_enemies;

        Game_Data_Push();
        gameLoopCG.kind = 5;
        nmlModelSendSignalStartBattle();
        xglSoundStreamStop(0);
        Vibration_Stop();
        PadDataCG.second_actuator[0] = 0;
        PadDataCG.actuator[0] = 0;
        func_A22668();
        gameLoopCG.frame_countdown = 0;
        gameLoopCG.flags |= 0x40;
        MSG_init();
        TWSYS_init();
        Vibration_Stop();
        gameLoopCG.model = 0;
        if (gameLoopCG.flags & 0x10000000) {
            return 1;
        }
        battle_result = func_00A0B5F0();
        if (battle_result == 2 && !(gameLoopCG.battle_flags & 0x40)) {
            xglFontLoad(1, 0);
            WindowTexLoad(0, 0);
            gameLoopCG.flags |= 0x88000000;
            return 1;
        }
        Game_Data_Pop();
        xglFontLoad(1, 0);
        WindowTexLoad(0, 0);
        always_method = D_004BE2C0;
        battle_enemies = enepc;
        CallMethod_I(always_method,
            battle_enemies[((ActorHead_CG *)gameLoopCG.battle_enemy)->number].enemy_id);
        switch (battle_result) {
        case 1:
            Enemy_After_Battle(1);
            CallMethod_I(D_004D8A68,
                battle_enemies[((ActorHead_CG *)gameLoopCG.battle_enemy)->number].enemy_id);
            break;
        case 3:
            Enemy_After_Battle(3);
            CallMethod_I(D_004BE2D0,
                battle_enemies[((ActorHead_CG *)gameLoopCG.battle_enemy)->number].enemy_id);
            break;
        default:
            Enemy_After_Battle(1);
            break;
        }
        if (gameLoopCG.kind == 3) {
            return 0;
        }
        ((ActorHead_CG *)gameLoopCG.controlled_actor)->value_9f0 = 0x4B;
        Actor_LookAt_Init(actor);
        MAP_updateUnit();
        return 0;
    }
    if (FLAG_FRAME_60 != 0) {
        if ((PadDataCG.held & 1) && (PadDataCG.pressed & 4)) {
            for (i = 1; i < ACTOR_COUNT; i++) {
                index = (NowTester + i) % ACTOR_COUNT;
                if (actorCG[index].inUseId > 0) {
                    NowTester = index;
                    gameLoopCG.controlled_actor =
                        (struct LookAtPlayerActor *)(void *)&actorCG[NowTester];
                    break;
                }
            }
        }
    }
    if ((gameLoopCG.flags_wide & 0x0001000000010000ULL) == 0x0001000000000000ULL) {
        u32 previous_flags;
        u16 previous_status;
        float restored_facing;

        player = (ActorHead_CG *)(void *)gameLoopCG.controlled_actor;
        actSequenceClear((struct ActorHead *)player);
        previous_flags = gameLoopCG.flags;
        previous_status = player->undulation.attr_mask;
        restored_facing = player->rotation.y;
        gameLoopCG.flags = previous_flags & ~0x4000;
        player->undulation.attr_mask = previous_status | 0x1000;
        player->target_angle = restored_facing;
    }
    if ((gameLoopCG.flags_wide & 0x0001000000010000ULL) == 0x10000) {
        ActorHead_CG *motion_player;

        motion_player = (ActorHead_CG *)(void *)gameLoopCG.controlled_actor;
        motion_player->undulation.attr_mask |= 0x1000;
        ACT_setMotion2((struct Actor *)motion_player, 0, 9);
    }
    flags = gameLoopCG.flags;
    player = (ActorHead_CG *)(void *)gameLoopCG.controlled_actor;
    gameLoopCG.base_flags = flags;
    if (!(flags & 0x20000)) {
        if (!(flags & 0x10000)) {
            GameCfPlayerMove();
            if (!(gameLoopCG.flags & 0x8000)) {
                checkTalkPoint();
            }
        } else {
            if (player->lookat_timer == 2) {
                Actor_LookAt(player);
            } else {
                Actor_LookAt_Release(player, 0);
            }
            ACT_updateSequence(player);
            GameIdLightSet(player, 0);
        }
    } else {
        GameIdLightSet(player, 0);
    }
    attribute = checkAttr((GameActorAttributeView *)gameLoopCG.controlled_actor);
    if (attribute != 0) {
        SCRIPT_execEntered(attribute - 1);
    }
    window_owner = (ActorHead_CG *)XTK_getWindowOwner();
    if (window_owner != 0) {
        if (getScriptFlag((SceneObject)window_owner) & 2) {
            window_owner->target_angle = Get_Angle(&window_owner->position, &gameLoopCG.controlled_actor->position);
            window_owner->rotation.y = Get_Cursol_by_Reduce_Speed_Angle_Loop(window_owner->rotation.y, window_owner->target_angle, 6.0f);
        }
    }
    touch = (int)checkTouch((ActorHead_CG *)(void *)gameLoopCG.controlled_actor);
    if (touch != 0) {
        if (touch != prevTouch) {
            prevTouch = touch;
            SCRIPT_execTouchto(touch);
        }
    } else {
        prevTouch = 0;
    }
    CheckGameSymbol(gameLoopCG.controlled_actor);
    TWSYS_update();
    JTHREAD_cntl();
    TCAMERA_update();
    if (xglStudioGetCamera2(0)->state == 4 && gameLoopCG.camera_callback != 0) {
        gameLoopCG.camera_callback();
    }
    if ((gameLoopCG.flags_wide & 0x60000) == 0) {
        ACT_update();
    } else {
        for (countdown = 15u; countdown > -1; countdown--) {
            int first_countdown = enepc[15 - countdown].stiff_timer;
            int second_countdown = (u16)enepc[15 - countdown].effect_timer[0] - 1;
            first_countdown -= 1;
            enepc[15 - countdown].effect_timer[0] = second_countdown;
            enepc[15 - countdown].stiff_timer = first_countdown;
            if ((short)first_countdown < 0) {
                enepc[15 - countdown].stiff_timer = 0;
            }
            if (enepc[15 - countdown].effect_timer[0] < 0) {
                enepc[15 - countdown].effect_timer[0] = 0;
            }
        }
        ACT_pauseUpdate();
    }
    MAP_updateUnit();
    {
    GameRadarParams_CG3 radar;
    radar.x = 0x160;
    radar.color = 0xFFFF00;
    radar.y = 0x120;
    radar.red = radar.green = radar.blue = 0x80;
    radar.alpha = 0x60;
    GameRadarDraw(&radar);
    }
    return 0;
}

extern int shopNo;
extern int MenuFileMain(int);
extern void MenuShopMain(int);
extern void CallMethod(char *);
extern int xglCdSync(void);
extern void SCRIPT_execTalkto();
typedef struct { u32 pad0; ActorHead *actor; u32 pad8[2]; u64 flags64; } TalkGLView;

void checkTalkPoint(void)
{
    ActorHead *actor;
    ItemBoxView *box;
    int st;
    GameLoopStateLayout *gs;

    if (PadData.pressed & 0x20) {
        gs = &GameLoopState;
        do {
        if (GameLoopState.status == 0 && xglCdSync() == 0) {
            if (((TalkGLView *)&GameLoopState)->flags64 & 0x20400000) {
                GameStateSave();
                MenuFileMain(0);
                GameLoopState.status = 0x20;
                return;
            }
            if (GameLoopState.flags & 0x800000) {
                CallMethod("evsExit");
                st = 0x20;
            } else if (GameLoopState.flags & 0x40000000) {
                if (shopNo == -1) {
                    shopNo = 1;
                }
                GameStateSaveCameraLight();
                MenuShopMain(shopNo);
                GameStateRestoreCameraLight();
                st = 0x20;
            } else {
                goto talk;
            }
            gs->status = st;
            return;
        }
talk:
        actor = checkTalk(((TalkGLView *)&GameLoopState)->actor);
        if (actor != 0) {
            enepc[((u8 *)actor)[ACTOR_NUMBER_OFFSET]].post_talk_flags = 0;
            SCRIPT_execTalkto(actor);
            return;
        }
        } while (0);
        box = checkItemBox((u8 *)((TalkGLView *)&GameLoopState)->actor);
        if (box != 0) {
            ((u8 *)box)[0x1a4] = 1;
        }
    }
}


void actSequenceClear(u8 *actor) {
    u32 *seq;
    u32 *flags = (u32 *)actor;
    float speed = D_004D7BA0;

    *flags &= ~2;
    *(float *)(actor + 0x6f8) = speed;
    seq = (u32 *)(actSequence + actor[0x80] * 0x260u);
    seq[0] = 0;
    seq[1] = 0;
    seq[3] = 0;
    seq[0xc] = 0;
    *(u32 *)(actor + 0x6f0) = 8;
    seq[6] = 0;
    seq[7] = 0;
    seq[8] = 0;
    seq[9] = 0;
    seq[10] = 0;
    seq[11] = 0;
    seq[5] = 0;
}

void actTalkAfter(SceneObject object)
{
    EnemyWorkPostTalk *enemy = &enepc[object[ACTOR_NUMBER_OFFSET]];

    After_Talk(object);
    enemy->post_talk_flags = GameLoopState.flags & 0x400;
}

static int GameModeCfEvent(void)
{
    int cleared = GameLoopState.flags & -2;
    u8 event_active = GameLoopState.status;

    GameLoopState.flags = cleared;
    if (event_active == 0 && (PadData.pressed & 0x800) != 0 &&
        (PadData.held & 0x10c) == 268) {
        GameLoopState.flags = cleared | 0x80000000;
        return 1;
    }

    TWSYS_update();
    JTHREAD_cntl();
    PLAY_ctrl();
    TCAMERA_update();
    ACT_update();
    MAP_updateUnit();
    return 0;
}

/*
 * GameModeDebugMenu (0x00245ba8): the debug-menu game mode, the last
 * handler of the game-mode TU whose static handlers start at 0x00244d20
 * (Game at 0x00245c58 follows it). Pad button 0x100 leaves the menu: clear
 * the pause flag, the 16-byte pause vector and restore the saved mode kind,
 * as the accepted handlers do on resume. Then run the per-frame systems,
 * the debug menu itself and the shared studio-camera callback gate.
 */
int GameModeDebugMenu(void)
{
    if ((PadData.debug_buttons & 0x100) != 0) {
        GameLoopState.pause_vector.quad = 0;
        GameLoopState.flags &= 0xfffffffeu;
        GameLoopState.kind = GameLoopState.saved_kind;
    }

    TWSYS_update();
    ACT_pauseUpdate();
    GameDebugMenu();
    {
        StudioCamera *camera = xglStudioGetCamera2(0);
        if (camera->state == 4 && GameLoopState.camera_callback != 0) {
            GameLoopState.camera_callback();
        }
    }
    return 0;
}

extern void xglRenderClearOff(void);
extern void nmlModelSetFadeInCancel(int frames);

void Game(void)
{
    int i;
    ActorHead *player;
    float zero = 0.0f;
    float one = 1.0f;
    void *work_end = (void *)WorkEnd;
    short *pause_flag = &pause_check;

    gameLoopCG.map_number = (s16)-1;
    gameLoopCG.saved_work_end = work_end;
    gameLoopCG.frame_hook = (void *)0;
    gameLoopCG.model = 0;
    gameLoopCG.stateB0 = 0;
    gameLoopCG.vector80[3] = one;
    gameLoopCG.vector80[2] = one;
    gameLoopCG.vector80[1] = one;
    gameLoopCG.vector90[2] = zero;
    gameLoopCG.vector90[1] = zero;
    gameLoopCG.vector90[0] = zero;
    gameLoopCG.vector90[3] = 30.0f;
    gameLoopCG.vectorA0[2] = zero;
    gameLoopCG.vectorA0[1] = zero;
    gameLoopCG.state5c = 0;
    gameLoopCG.state58 = 0;
    gameLoopCG.frame_countdown = 0;
    gameLoopCG.camera_callback = 0;
    gameLoopCG.clear_callback = 0;
    gameLoopCG.map_draw_callback = 0;
    *pause_flag = 0;
    gameLoopCG.kind = 0;
    gameLoopCG.stateC8 = 0;
    gameLoopCG.actor_draw_callback = 0;
    gameLoopCG.vector80[0] = one;
    gameLoopCG.vectorA0[0] = zero;
    gameLoopCG.vectorA0[3] = 30.0f;
    gameLoopCG.stateD0 = -2;
    gameLoopCG.flags = 0x302;
    nmlModelSetFadeInCancel(30u);
    nmlModelSetEffectWrite(1);
    Enemy_SystemInit();
    InitUwamonoSys();
    xglRenderClearDepth();
    xglRenderGlobalFadeInit();
    xglRenderClearOff();
    xglStudioChange(0);
    xglLightSetDefault((GameLightSet *)xglStudioGetLight2());
    GameCameraReset();
    GameCFSoundReload();
    GameResourceInit(0x1000000, 0x1000000);
    MSG_init();
    TWSYS_init();
    MapInit();
    MAP_initUnit();
    SCRIPT_sceneChangeTimeInit();
    SCRIPT_clrEventActiveFlag();
    SCRIPT_talkIgnoreClr();
    ACT_init();
    for (i = 0; i < 64; i++) {
        actor[i].update = 0;
    }
    GameDefocusSet(-1, 0, 0);
    GameBgDrawType0();
    func_A22628();
    sefInitEffectCf();
    gameLoopCG.scheduler = (XglTaskScheduler *)WorkEnd;
    WorkEnd = xglTaskInitial(WorkEnd, 64, 0);
    WorkEnd = SCRIPT_init(WorkEnd);
    GameRadarInit();
    GameCfPlayerMoveInit();
    gameLoopCG.controlled_actor = (struct LookAtPlayerActor *)ACT_create(0, 1);
    Player_System_Init(gameLoopCG.controlled_actor);
    gameLoopCG.status = 0x10;
    if (gameLoopCG.kindDetail != 0) {
        Quadword *position_dst;

        player = (ActorHead *)gameLoopCG.controlled_actor;
        GamePopSaveDataUser();
        position_dst = (Quadword *)&player->position;
        Java_xeno_util_Runtime_jump_sub(gameLoopCG.current_event_id, -1);
        __asm__ __volatile__(
            "lq $2, 0(%1)\n"
            "sq $2, 0(%0)\n"
            :
            : "r" (position_dst), "r" (&gameLoopCG.saved_actor_position)
            : "$2", "memory");
        gameLoopCG.move_mode |= 0xa0;
    } else {
        Java_xeno_util_Runtime_jump_sub(gameLoopCG.current_event_id, -1);
    }
    while (1) {
        if (gameLoopCG.frame_hook != 0) {
            gameLoopCG.frame_hook(2);
        }
        nmlModelSendPauseSignal(gameLoopCG.flags & 1);
        SCRIPT_sceneChangeTimeDec();
        Disp_Homing();
        if (gameLoopCG.status != 0) {
            gameLoopCG.status--;
        }
        switch (gameLoopCG.kind) {
        case 0:
            gameLoopCG.kind = 2;
            break;
        case 1:
            GameModePause();
            break;
        case 2:
            GameModeCfMain();
            break;
        case 3:
            GameModeCfEvent();
            break;
        case 6:
            GameModeEvtDebug();
            break;
        case 7:
            GameModeEvtTools();
            break;
        case 8:
            GameModeDebugMenu();
            break;
        }
        if (gameLoopCG.flags & 0x90000000) {
            break;
        }
        xglTaskExecute(gameLoopCG.scheduler);
        GameDrawSync();
    }
    xglSoundStreamStop(0);
    GameMovieStop();
    GameDefocusSet(-1, 0, 0);
    sefInitEffectCf();
    xglCdReset();
    xglCullingMapInit();
    GameCFSoundPurge();
    nmlModelSendPauseSignal(0);
    SCRIPT_sceneChangeTimeInit();
    SCRIPT_clrEventActiveFlag();
    SCRIPT_talkIgnoreClr();
    xglRenderGlobalFadeInit();
    xglStudioMainCameraInit();
    nmlModelConstruct();
    xglRenderClearOff();
    WorkEnd = gameLoopCG.saved_work_end;
    if (gameLoopCG.flags & 0x8000000) {
        GameOver();
    }
    gameLoopCG.kind = 0;
    xglSleep();
    xglSleep();
}

#include "common.h"
#include "shared.h"
#include "game.h"

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
    u8 unmodeled_00[0x37b4];
    u32 post_talk_flags;
    u8 unmodeled_37b8[0x38b0 - 0x37b8];
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

INCLUDE_ASM("asm/main/nonmatchings/game", GameCFSoundMenuPurge);

INCLUDE_ASM("asm/main/nonmatchings/game", GameCFSoundMenuReload);

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

INCLUDE_ASM("asm/main/nonmatchings/game", Game_Data_Pop);

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

INCLUDE_ASM("asm/main/nonmatchings/game", MenuGameDataPop);

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

INCLUDE_ASM("asm/main/nonmatchings/game", GamePushSaveDataUser);

INCLUDE_ASM("asm/main/nonmatchings/game", GamePopSaveDataUser);

INCLUDE_ASM("asm/main/nonmatchings/game", InitCfSystem);

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

INCLUDE_ASM("asm/main/nonmatchings/game", checkTalk);

typedef struct {
    u32 flags;
    u8 unmodeled_04[ACTOR_IN_USE_ID_OFFSET - 0x04];
    short inUseId;
    u8 unmodeled_88[0xA70 - (ACTOR_IN_USE_ID_OFFSET + 2)];
} ActorHead;

extern ActorHead actor[ACTOR_COUNT];

extern float D_004D7B8C;
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

INCLUDE_ASM("asm/main/nonmatchings/game", checkItemBox);

INCLUDE_ASM("asm/main/nonmatchings/game", CheckGameSymbol);

INCLUDE_ASM("asm/main/nonmatchings/game", GameDispTag);

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

INCLUDE_ASM("asm/main/nonmatchings/game", GameDrawSync);

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

INCLUDE_ASM("asm/main/nonmatchings/game", GameModeCfMain);

INCLUDE_ASM("asm/main/nonmatchings/game", checkTalkPoint);

extern u8 actSequence[0x9800];
extern float D_004D7BA0;

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

INCLUDE_ASM("asm/main/nonmatchings/game", Game);

#include "common.h"
#include "shared.h"
#include "main/party.h"

/* Types recovered upstream (origin/master); kept for reference. */
/* Runtime native lookup pairs: method signature followed by its entry point. */
typedef struct RuntimeNativeBinding {
    const char *signature;
    void (*entry)();
} RuntimeNativeBinding;


/*
 * The script VM's per-thread context, recovered as `JThread` in
 * src/main/chr.h (main's chr TU). Every Java native receives
 * (thread, arguments, result) as the sibling natives of this TU show
 * (a1 read as the argument block, a2 written as the result); these
 * wrappers only need the tag.
 */
typedef struct JThread JThread;

extern void SCRIPT_sceneChangeTimeSet(int value);
extern void CharactorAllRecovery(void);
extern void AgwsAllRecovery(void);
extern void tyaCaptureEnd(void);

/* The call block of a native taking three plain ints (java signature "(III)"). */
typedef struct {
    int first;
    int second;
    int third;
} IntTripleCall;

/*
 * An 8-byte-aligned view of Vector4, so a whole-vector assignment compiles
 * to the aligned ld/sd pair the original uses instead of an unaligned
 * ldl/ldr sequence (config/compiler-patterns.json CP-0237, same technique
 * as src/main/pp_init.c's PpVector4).
 */
typedef union RuntimeVector4 {
    Vector4 vector;
    unsigned long long words[2];
} RuntimeVector4;

/*
 * A partial, TU-local view of the engine's actor record (the same object
 * fully recovered as `Actor` in src/main/chr.h, src/main/near_dir.h and
 * src/main/set_motion.h for main's chr/near_dir/set_motion TUs) for the two
 * members scriptReset_evsExit touches: the position vector at +0x10 and the
 * status flags word at +0x4D0, both documented for the same object in
 * chr.h. Named separately from those TUs' `Actor` tag because it collides
 * with the canonical spelling elsewhere (config/header-canon.json).
 */
typedef struct RuntimeActor {
    unsigned char unmodeled_00[0x10];
    RuntimeVector4 position;              /* +0x10, ld/sd at
                                              0x002f76bc..0x002f76d8
                                              (scriptReset_evsExit) */
    unsigned char unmodeled_20[0x4D0 - 0x20];
    unsigned short status_flags;          /* +0x4D0, lhu/sh at
                                              0x002f76c0..0x002f76e0
                                              (scriptReset_evsExit) */
} RuntimeActor;

/*
 * GameLoopState offsets this TU touches. GameLoopState is a TU-local
 * divergent view (config/header-canon.json "GameLoopState"): every unit
 * models only the offsets its own bytes evidence, and no shared header
 * declares it.
 */
typedef struct {
    unsigned char unmodeled_00[4];
    RuntimeActor *player;                  /* +0x04, lw at 0x002f76b0
                                              (scriptReset_evsExit) */
    XglTaskScheduler *task_scheduler;     /* +0x08, lw/beq at 0x002f7350
                                              (setLocation) */
    unsigned char unmodeled_0c[0xE - 0xC];
    unsigned short deferred_jump_pending; /* +0x0E, sh $0 at 0x002f767c */
    unsigned int flags;                   /* +0x10, lw/nor/and/sw at
                                              0x002f74a4..0x002f74bc (disable),
                                              lw/or/sw at 0x002f74c4..0x002f74d8
                                              (enable), lw/sw at
                                              0x002f74e4..0x002f74ec
                                              (getGameState) */
    unsigned char unmodeled_14[0x18 - 0x14];
    int event_id;                         /* +0x18, lw at 0x002f86f4
                                              (evsSetRetPoint) */
    unsigned char unmodeled_1c[0x20 - 0x1C];
    unsigned int shoot_control;           /* +0x20, lw/ori/sw and lw/and/sw at
                                              0x002f78a0..0x002f78c8
                                              (setShootFlag, bit 0),
                                              0x002f78dc..0x002f7904
                                              (setShootHeightCheck, bit 1),
                                              0x002f791c..0x002f7944
                                              (setShootIDCheck, bit 2),
                                              0x002f795c..0x002f7984
                                              (setShootUwaCheck, bit 3) */
    unsigned char unmodeled_24[0x52 - 0x24];
    short entrance;                       /* +0x52, lh at 0x002f7424 */
    unsigned char unmodeled_54[0xA0 - 0x54];
    float fade_color[3];                  /* +0xA0..0xA8, execBattle */
    float fade_frames;                    /* +0xAC, execBattle */
    unsigned char unmodeled_b0[0xC1 - 0xB0];
    unsigned char player_flags;           /* +0xC1, lbu/ori/sb at
                                              0x002f7448..0x002f7460
                                              (setPlayerControl, bit 0x20) */
    unsigned char unmodeled_c2[0x29F44 - 0xC2];
    float shoot_range;                    /* +0x29F44, swc1 at 0x002f799c */
    unsigned char unmodeled_29f48[0x2A018 - 0x29F48];
    int active_event_id;                  /* +0x2A018, lw at 0x002f8730 */
    unsigned char unmodeled_2a01c[0x2A020 - 0x2A01C];
    RuntimeVector4 saved_position;        /* +0x2A020, ld/sd at
                                              0x002f76bc..0x002f76d8
                                              (scriptReset_evsExit) */
} GameLoopStateRuntimeView;

extern GameLoopStateRuntimeView GameLoopState;

/*
 * A partial view of the script VM thread (JThread, fully recovered in
 * src/main/chr.h for main/tu248) for the three words evsExit touches: the
 * state word at +0x24 and the resume/frame counters at +0x3C/+0x3E chr.h
 * documents for the same object. Modeled separately here because chr.h is
 * TU-local to tu248.
 */
typedef struct {
    unsigned char unmodeled_00[0x24];
    unsigned int flags;              /* +0x24 */
    unsigned char unmodeled_28[0x3C - 0x24 - 4];
    unsigned short resume_frames;    /* +0x3C */
    unsigned short frame_depth;      /* +0x3E */
} RuntimeThreadState;

/* setLocation below touches the same three RuntimeThreadState words as
   evsExit: resume_frames takes frame_depth and flags gains bit 0x8
   (native-call block) and 0x1 (generic wait) (0x002f7388..0x002f739c). */

extern int xglFontLoad(int type, int callback);
extern void GameStateSave(void);
extern void GameStateRestore(void);
extern void func_00A0B728(int);
extern void func_00A0B708(int);
extern void func_A22668(void);
extern void ACT_resourceInit(void);
extern void ACT_initScene(void);
extern void ACT_initMTNResource(void);
extern void nmlModelSetFadeIn(int duration, int mode);
extern void FX_ScreenMask(int unused, int color, int duration, int mode);

void Java_xeno_util_Runtime_execBattle__II(void *env, int *arguments)
{
    xglFontLoad(1, 0);
    GameStateSave();
    func_00A0B728(arguments[0]);
    func_00A0B708(arguments[1]);
    func_A22668();
    ACT_resourceInit();
    ACT_initScene();
    ACT_initMTNResource();
    nmlModelSetFadeIn((int)GameLoopState.fade_frames, 0);
    FX_ScreenMask(0,
                  0x7F000000
                      + ((int)(GameLoopState.fade_color[0] * 127.0f) << 16)
                      + ((int)(GameLoopState.fade_color[1] * 127.0f) << 8)
                      + (int)(GameLoopState.fade_color[2] * 127.0f),
                  (int)GameLoopState.fade_frames, 3);
    GameStateRestore();
}

/* The call block of a native taking two plain ints (java signature "(II)"). */
typedef struct {
    int first;
    int second;
} IntPairCall;

extern int xglFlagsGet(int group, int bit);

void Java_xeno_util_Runtime_getFlags__II(JThread *thread, IntPairCall *arguments,
                                         int *result)
{
    *result = xglFlagsGet(arguments->first, arguments->second);
}

/* The call block of Java_xeno_util_Runtime_setFlags__III, the xglFlagsSet
   operands the native passes through unchanged and in order. */
typedef struct {
    int bit_offset;
    int bit_count;
    int value;
} FlagsSetCall;

extern int xglFlagsSet(int bit_offset, int bit_count, int value);

void Java_xeno_util_Runtime_setFlags__III(JThread *thread, FlagsSetCall *arguments,
                                          unsigned int *result)
{
    xglFlagsSet(arguments->bit_offset, arguments->bit_count, arguments->value);
}

extern void LoadMapOnly(int map_id);
extern void MapChange2(int map_id);

void Java_xeno_util_Runtime_setLocation__III(RuntimeThreadState *thread,
                                             IntTripleCall *arguments,
                                             unsigned int *result)
{
    int destination = arguments->first;

    if (arguments->third == 0 || GameLoopState.task_scheduler == 0) {
        MapChange2(destination);
        return;
    }
    LoadMapOnly(destination);
    thread->resume_frames = thread->frame_depth;
    thread->flags |= 9;
}

extern int MapGetNo(void);

void Java_xeno_util_Runtime_getLocation__(JThread *thread, void *arguments, int *result)
{
    *result = MapGetNo();
}

/* The script VM's 32-entry general register file, indexed with the low 5
   bits of the requested register number (VMRegister[index & 0x1F]). */
extern int VMRegister[32];

void Java_xeno_util_Runtime_setRegister__II(JThread *thread, IntPairCall *arguments,
                                            unsigned int *result)
{
    VMRegister[arguments->first & 0x1F] = arguments->second;
}

void Java_xeno_util_Runtime_getRegister__I(JThread *thread, int *arguments, int *result)
{
    *result = VMRegister[*arguments & 0x1F];
}

void Java_xeno_util_Runtime_getEntrance__(JThread *thread, void *arguments, int *result)
{
    *result = GameLoopState.entrance;
}

void Java_xeno_util_Runtime_setPlayerControl__Z(JThread *thread, unsigned char *arguments,
                                                unsigned int *result)
{
    if (*arguments != 0) {
        GameLoopState.flags &= ~0x8000;
        GameLoopState.player_flags |= 0x20;
    } else {
        GameLoopState.flags |= 0x8000;
    }
}

/* The call block of a native taking three plain floats (java signature "(FFF)"). */
typedef struct {
    float walk_threshold;
    float run_threshold;
    float vector_rate;
} PlayerMoveParamCall;

extern void GameCfPlayerMoveParamSet(float walk_threshold, float run_threshold,
                                      float vector_rate);

void Java_xeno_util_Runtime_setPlayerMoveParam__FFF(JThread *thread,
                                                     PlayerMoveParamCall *arguments,
                                                     unsigned int *result)
{
    GameCfPlayerMoveParamSet(arguments->walk_threshold, arguments->run_threshold,
                             arguments->vector_rate);
}

void Java_xeno_util_Runtime_disable__I(JThread *thread, unsigned int *arguments,
                                       unsigned int *result)
{
    GameLoopState.flags &= ~*arguments;
}

void Java_xeno_util_Runtime_enable__I(JThread *thread, unsigned int *arguments,
                                      unsigned int *result)
{
    GameLoopState.flags |= *arguments;
}

void Java_xeno_util_Runtime_getGameState__(JThread *thread, void *arguments, int *result)
{
    *result = GameLoopState.flags;
}

void Java_xeno_util_Runtime_CaptureEnd__(JThread *thread, void *arguments,
                                         unsigned int *result)
{
    tyaCaptureEnd();
}

/*
 * A java.lang.String argument, as CaptureStart/setEventTimer below read it:
 * offset +0x00 (the class word every object reference begins with, per
 * shared.h) is untouched by this TU, and +0x04 holds the interned string
 * record the String was loaded from (lw v1,4(v0) at 0x002f7514/0x002f78ac).
 */
typedef struct {
    void *unmodeled_00;
    struct InternedString *value;
} StringRef;

/*
 * The interned constant-string record `StringRef.value` points to, modeled
 * here only for the `bytes` pointer both natives below read at +0x08 (lw
 * v0,8(v0) at 0x002f7518/0x002f78b0). Same object as `ConstString` of
 * src/main/string_utf_get_hash.h (main/tu224): next-in-bucket link and
 * packed hash/length occupy the same leading 8 bytes there.
 */
struct InternedString {
    unsigned char unmodeled_00[8];
    char *bytes;
};

/* The call block of a native taking (java.lang.String, int). */
typedef struct {
    StringRef *string;
    int value;
} StringArgCall;

extern void tyaCaptureStart(const char *name, int count);

void Java_xeno_util_Runtime_CaptureStart__Ljava_lang_String_I(JThread *thread,
                                                               StringArgCall *arguments,
                                                               unsigned int *result)
{
    tyaCaptureStart(arguments->string->value->bytes, arguments->value);
}

extern void GameDefocusSet(int mode, int strength, int *table);
extern void xglSoundStreamStop(int channel);
extern void GameMovieStop(void);
extern void xglSoundSequenceFadeOut2(int channel, int time);
extern void InitCfSystem(void);
extern void UnduInit(int flag);
extern void SCRIPT_reset(int mode);
extern void GameResourceLoad(int scene_id);
extern void ACT_setModelWrapper(void *actor, int flags);
extern void ACT_setHumanHand(void *actor, int hand);

void Java_xeno_util_Runtime_jump_sub(int destination, int argument)
{
    int i;

    tyaCaptureEnd();
    xglSoundStreamStop(0);
    GameMovieStop();
    GameDefocusSet(-1, 0, 0);
    if (!(destination & 0x10000)) {
        for (i = 0; i < 8; i++)
            xglSoundSequenceFadeOut2(i, 1000);
    }
    if (argument >= 0)
        GameLoopState.entrance = argument;
    GameLoopState.event_id = destination & 0xFFF0FFFF;
    InitCfSystem();
    *(int *)((char *)&GameLoopState + 0x54) = 0;
    UnduInit(0);
    SCRIPT_reset(0);
    if (GameLoopState.event_id == 0x270E || GameLoopState.event_id == 0x270F)
        GameLoopState.flags |= 0x88000000;
    GameResourceLoad(GameLoopState.event_id);
    if (GameLoopState.player != 0 && *(unsigned short *)((char *)&GameLoopState + 0xC) == 2) {
        ACT_setModelWrapper(GameLoopState.player, 0);
        ACT_setHumanHand(GameLoopState.player, 0);
    }
}

extern void Java_xeno_util_Runtime_jump_sub(int destination, int argument);

/* The deferred script jump scriptReset_jump below executes once scheduled. */
typedef struct {
    int destination;
    int argument;
} DeferredJumpCall;

static DeferredJumpCall args_jump;

static void scriptReset_jump(void)
{
    GameLoopState.deferred_jump_pending = 0;
    Java_xeno_util_Runtime_jump_sub(args_jump.destination, args_jump.argument);
}

static void scriptReset_evsExit(void)
{
    GameLoopState.deferred_jump_pending = 0;
    Java_xeno_util_Runtime_jump_sub(args_jump.destination, args_jump.argument);
    GameLoopState.player->position = GameLoopState.saved_position;
    GameLoopState.player->status_flags |= 0x1000;
}

extern void (*jthreadResetFunc)(void);
extern void SCRIPT_talkIgnoreSet(void);

void Java_xeno_util_Runtime_jumpCF__II(RuntimeThreadState *thread, int *arguments)
{
    args_jump.destination = arguments[0];
    *(void **)&args_jump.argument = (void *)arguments[1];
    *(int *)&jthreadResetFunc = (int)scriptReset_jump;
    SCRIPT_talkIgnoreSet();
    {
        unsigned int flags = thread->flags | 9;
        thread->resume_frames = thread->frame_depth;
        thread->flags = flags;
    }
}

extern void (*jthreadResetFunc)(void);
extern void SCRIPT_talkIgnoreSet(void);

void Java_xeno_util_Runtime_jumpEvent__I(RuntimeThreadState *thread, IntPairCall *arguments,
                                         unsigned int *result)
{
    int entrance;
    unsigned int flags;

    args_jump.destination = arguments->first + 0x10000000;
    entrance = arguments->second;
    jthreadResetFunc = scriptReset_jump;
    args_jump.argument = entrance;
    SCRIPT_talkIgnoreSet();
    flags = thread->flags | 9;
    thread->resume_frames = thread->frame_depth;
    thread->flags = flags;
}

/* The call block of a native taking four plain ints (java signature "(IIII)"). */
typedef struct {
    int first;
    int second;
    int third;
    int fourth;
} IntQuadCall;

extern void GameDefocusQuickSet(int first, int second, int third, int fourth);

void Java_xeno_util_Runtime_setDefocusQuick__IIII(JThread *thread, IntQuadCall *arguments,
                                                  unsigned int *result)
{
    GameDefocusQuickSet(arguments->first, arguments->second, arguments->third,
                        arguments->fourth);
}

/*
 * A partial, TU-local view of the Java int-array object (the same object
 * fully recovered as `VMArray` in src/main/init_vm.c) for the `data` pointer
 * setDefocus below reads at +0x08 (lw at 0x002f7800).
 */
typedef struct {
    unsigned char unmodeled_00[8];
    int *data;                            /* +0x08 */
} RuntimeIntArray;

/* The call block of setDefocus (java signature "(II[I)"): two plain ints and
   an optional int-array reference, null when the caller passes none. */
typedef struct {
    int mode;
    int strength;
    RuntimeIntArray *table;
} DefocusCall;

extern void GameDefocusSet(int mode, int strength, int *table);

void Java_xeno_util_Runtime_setDefocus__IIaI(JThread *thread, DefocusCall *arguments,
                                             unsigned int *result)
{
    if (arguments->table == 0) {
        GameDefocusSet(arguments->mode, arguments->strength, 0);
    } else {
        GameDefocusSet(arguments->mode, arguments->strength, arguments->table->data);
    }
}

/* A defocus parameter table of seventeen-entry rows, one word ahead of the
   symbol (index 0 is untouched by this TU). */
extern int GameDefocusParam[];

void Java_xeno_util_Runtime_setDefocusParam__III(JThread *thread, IntTripleCall *arguments,
                                                 unsigned int *result)
{
    GameDefocusParam[1 + arguments->first * 17 + arguments->second] = arguments->third;
}

extern void setEventTimerTaskEntry(const char *method_reference, int countdown);

void Java_xeno_util_Runtime_setEventTimer__Ljava_lang_String_I(JThread *thread,
                                                                StringArgCall *arguments,
                                                                unsigned int *result)
{
    setEventTimerTaskEntry(arguments->string->value->bytes, arguments->value);
}

extern void MapChangeResource(int map_id, int resource_id);

void Java_xeno_util_Runtime_setMap__II(JThread *thread, IntPairCall *arguments,
                                       unsigned int *result)
{
    MapChangeResource(arguments->first, arguments->second);
}

void Java_xeno_util_Runtime_setShootFlag__Z(JThread *thread, unsigned char *arguments,
                                            unsigned int *result)
{
    if (*arguments != 0) {
        GameLoopState.shoot_control |= 1;
    } else {
        GameLoopState.shoot_control &= ~1;
    }
}

void Java_xeno_util_Runtime_setShootHeightCheck__Z(JThread *thread, unsigned char *arguments,
                                                    unsigned int *result)
{
    if (*arguments != 0) {
        GameLoopState.shoot_control |= 2;
    } else {
        GameLoopState.shoot_control &= ~2;
    }
}

void Java_xeno_util_Runtime_setShootIDCheck__Z(JThread *thread, unsigned char *arguments,
                                               unsigned int *result)
{
    if (*arguments != 0) {
        GameLoopState.shoot_control |= 4;
    } else {
        GameLoopState.shoot_control &= ~4;
    }
}

void Java_xeno_util_Runtime_setShootUwaCheck__Z(JThread *thread, unsigned char *arguments,
                                                unsigned int *result)
{
    if (*arguments != 0) {
        GameLoopState.shoot_control |= 8;
    } else {
        GameLoopState.shoot_control &= ~8;
    }
}

void Java_xeno_util_Runtime_setShootRange__F(JThread *thread, float *arguments,
                                             unsigned int *result)
{
    GameLoopState.shoot_range = *arguments;
}

extern int dataBoxInc(int item, int count);

void Java_xeno_util_Runtime_addItem__II(JThread *thread, IntPairCall *arguments,
                                        signed char *result)
{
    *result = (signed char) dataBoxInc(arguments->first, arguments->second);
}

extern int CreateEvtItemGetTask(int item, int count);

void Java_xeno_util_Runtime_addItemWin__II(JThread *thread, IntPairCall *arguments,
                                           signed char *result)
{
    *result = (signed char) CreateEvtItemGetTask(arguments->first, arguments->second);
}

extern int dataBoxDec(int item, int count);

void Java_xeno_util_Runtime_removeItem__II(JThread *thread, IntPairCall *arguments,
                                           signed char *result)
{
    *result = (signed char) dataBoxDec(arguments->first, arguments->second);
}

extern int dataBoxChk(int item, int count);

void Java_xeno_util_Runtime_checkItem__II(JThread *thread, IntPairCall *arguments,
                                          int *result)
{
    *result = dataBoxChk(arguments->first, arguments->second);
}

extern char *GetItemName(int kind, int id);
extern void *classString;
extern void *classByte;
extern void *newObject(void *clazz);
extern void *newArray(void *clazz, int length);
extern unsigned int strlen(const char *s);

void Java_xeno_util_Runtime_getItemName__II(JThread *thread, int *arguments, int *result)
{
    char *name = GetItemName(arguments[0], arguments[1]);
    int *string = newObject(classString);
    int *array = newArray(classByte, 0xFF);
    string[1] = (int)array;
    array[2] = (int)name;
    array[1] = strlen(name);
    *result = (int)string;
}

extern int dataMoneyBoxInc(int amount);

void Java_xeno_util_Runtime_addGold__I(JThread *thread, int *arguments, signed char *result)
{
    *result = (signed char) dataMoneyBoxInc(*arguments);
}

extern int dataMoneyBoxDec(int amount);

void Java_xeno_util_Runtime_removeGold__I(JThread *thread, int *arguments, signed char *result)
{
    *result = (signed char) dataMoneyBoxDec(*arguments);
}

extern int dataMoneyBoxChk(void);

void Java_xeno_util_Runtime_checkGold__(JThread *thread, void *arguments, int *result)
{
    *result = dataMoneyBoxChk();
}

extern void sefProgressEffect(int frame_count);

void Java_xeno_util_Runtime_progressEffect__I(JThread *thread, int *arguments,
                                              unsigned int *result)
{
    sefProgressEffect(*arguments);
}

extern void GameStateRestoreCameraLight(void);
extern void GameStateSaveCameraLight(void);
extern void MenuShopMain(int shop);

void Java_xeno_util_Runtime_enterShop__I(JThread *thread, int *arguments,
                                         unsigned int *result)
{
    GameStateSaveCameraLight();
    MenuShopMain(*arguments);
    GameStateRestoreCameraLight();
}

/*
 * The leader's battle id: read directly out of SaveData (lui/lhu at
 * 0x002f7bd0/0x002f7bd4, no call) rather than through PartyDataGet(), which
 * getPartyDataOfs below shows returns SaveData + 0x10078, so this offset is
 * that region's own +0x2C.
 */
#define SAVE_LEADER_BATTLE_ID 0x100A4

typedef struct {
    unsigned short leader_battle_id;
} SaveDataLeaderView;

void Java_xeno_util_Runtime_getLeader__(JThread *thread, void *arguments, int *result)
{
    *result = ((SaveDataLeaderView *) (SaveData + SAVE_LEADER_BATTLE_ID))->leader_battle_id;
}

static char *getPartyDataOfs(unsigned int selector)
{
    int type = (selector >> 16) & 0xff;
    unsigned int offset = selector & 0xffff;
    char *party_data = (char *)PartyDataGet();

    char *selected;

    if (type == 1) {
        selected = party_data + 0x30;
    } else if (type < 2) {
        goto default_party;
    } else if (type == 2) {
        selected = party_data + 0x3c;
    } else if (type == 3) {
        selected = party_data + 0x140;
    } else {
default_party:
        selected = party_data;
    }
    return selected + offset;
}

void Java_xeno_util_Runtime_getPartyData__I(JThread *thread, unsigned int *arguments, int *result)
{
    unsigned int sel = arguments[0];
    int type = sel >> 24;
    char *p = getPartyDataOfs(sel);
    int v;

    if (type != 1) {
        if (type < 2) goto byte;
        if (type == 2) goto word;
        if (type == 3) {
            v = *(int *)p;
            goto done;
        }
byte:
        v = *(unsigned char *)p;
        goto done;
    }
    v = *(short *)p;
    goto done;
word:
    v = *(int *)p;
done:
    *result = v;
}

extern void GameCfPlayerLoadResource(int resource_set);

void Java_xeno_util_Runtime_setPartyData__II(JThread *thread, unsigned int *arguments)
{
    unsigned int sel = arguments[0];
    int type = sel >> 24;
    char *p = getPartyDataOfs(sel);

    switch (type) {
    default:
    case 0:
        *(unsigned char *)p = *(unsigned char *)&arguments[1];
        break;
    case 1:
        *(short *)p = *(unsigned short *)&arguments[1];
        break;
    case 2:
        *(int *)p = arguments[1];
        break;
    case 3:
        *(long *)p = (int)arguments[1];
        break;
    }
    if (arguments[0] == 0x0100002C) {
        char *pl = (char *)GameLoopState.player;
        int saved = *(int *)(pl + 0x8D0);
        if (*(short *)(pl + 0x86) != (int)arguments[1]) {
            *(int *)(pl + 0x8D0) = 0;
            xglSleep();
            xglSleep();
            *(int *)(pl + 0x8D0) = saved;
            GameCfPlayerLoadResource(1);
        }
    }
}

/* Converts a Java character id to its battle-member index, as every
   Party*Check/On/Off callee below expects: the one-based position of the id in
   the table, or zero when the table does not list it. */
static int cid2bic(int character_id)
{
    /* The zero-terminated character-id table, in the order the battle party
       uses (tbl.0, 0x42 bytes). */
    static unsigned short tbl[] = {
        0x0003, 0x0002, 0x0001, 0x0007, 0x0006, 0x0004, 0x0005, 0x0127,
        0x0128, 0x010C, 0x0120, 0x0121, 0xF013, 0xF014, 0xF015, 0xF016,
        0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2101, 0x2102,
        0x2103, 0x2104, 0x2105, 0x210A, 0x2201, 0xF014, 0xF015, 0xF016,
        0x0000,
    };
    int index;

    for (index = 0; tbl[index] != 0; index++) {
        if (tbl[index] == character_id) {
            return index + 1;
        }
    }
    return 0;
}

extern void PartyFriendOn(int battle_index);

void Java_xeno_util_Runtime_setFriend__I(JThread *thread, int *arguments,
                                         unsigned int *result)
{
    PartyFriendOn(cid2bic(*arguments));
}

extern void PartyFriendOff(int battle_index);

void Java_xeno_util_Runtime_resetFriend__I(JThread *thread, int *arguments,
                                           unsigned int *result)
{
    PartyFriendOff(cid2bic(*arguments));
}

extern int PartyFriendCheck(int battle_index);

void Java_xeno_util_Runtime_checkFriend__I(JThread *thread, int *arguments, int *result)
{
    *result = PartyFriendCheck(cid2bic(*arguments));
}

extern void PartyLockPartyOn(int battle_index);

void Java_xeno_util_Runtime_setLockParty__I(JThread *thread, int *arguments,
                                            unsigned int *result)
{
    PartyLockPartyOn(cid2bic(*arguments));
}

extern void PartyLockPartyOff(int battle_index);

void Java_xeno_util_Runtime_resetLockParty__I(JThread *thread, int *arguments,
                                              unsigned int *result)
{
    PartyLockPartyOff(cid2bic(*arguments));
}

extern int PartyLockPartyCheck(int battle_index);

void Java_xeno_util_Runtime_checkLockParty__I(JThread *thread, int *arguments, int *result)
{
    *result = PartyLockPartyCheck(cid2bic(*arguments));
}

extern void PartyOutFriendOn(int battle_index);

void Java_xeno_util_Runtime_setOutFriend__I(JThread *thread, int *arguments,
                                            unsigned int *result)
{
    PartyOutFriendOn(cid2bic(*arguments));
}

extern void PartyOutFriendOff(int battle_index);

void Java_xeno_util_Runtime_resetOutFriend__I(JThread *thread, int *arguments,
                                              unsigned int *result)
{
    PartyOutFriendOff(cid2bic(*arguments));
}

extern int PartyOutFriendCheck(int battle_index);

void Java_xeno_util_Runtime_checkOutFriend__I(JThread *thread, int *arguments, int *result)
{
    *result = PartyOutFriendCheck(cid2bic(*arguments));
}

extern void PartyTakeAgwsOn(int battle_index);

void Java_xeno_util_Runtime_setTakeAgws__I(JThread *thread, int *arguments,
                                           unsigned int *result)
{
    PartyTakeAgwsOn(cid2bic(*arguments));
}

extern void PartyTakeAgwsOff(int battle_index);

void Java_xeno_util_Runtime_resetTakeAgws__I(JThread *thread, int *arguments,
                                             unsigned int *result)
{
    PartyTakeAgwsOff(cid2bic(*arguments));
}

extern int PartyTakeAgwsCheck(int battle_index);

void Java_xeno_util_Runtime_checkTakeAgws__I(JThread *thread, int *arguments, int *result)
{
    *result = PartyTakeAgwsCheck(cid2bic(*arguments));
}

extern void PartyBattleChange(int battle_index);

void Java_xeno_util_Runtime_battleChangeParty__I(JThread *thread, int *arguments,
                                                  unsigned int *result)
{
    PartyBattleChange(cid2bic(*arguments));
}

typedef struct { char pad[0xF0]; } GameIdLight_t;

extern GameIdLight_t GameIdLight[];

extern void xglLightIntensityAmbient(void *light, float *v);
extern void xglLightIntensityParallel(void *light, int n, float *v);

void Java_xeno_util_Runtime_setIdLightCol__IIFFF(void *env, union { int i; float f; void *p; } *args)
{
    int id = args[0].i;
    int n = args[1].i;
    float v[4];
    if (id > 0) id--;
    v[0] = args[2].f;
    v[1] = args[3].f;
    v[2] = args[4].f;
    v[3] = 1.0f;
    if (n == 0)
        xglLightIntensityAmbient(&GameIdLight[id], v);
    else
        xglLightIntensityParallel(&GameIdLight[id], n - 1, v);
}

extern void xglLightAngle(void *light, int n, float *v);

void Java_xeno_util_Runtime_setIdLightDir__IIFFF(void *env, union { int i; float f; void *p; } *args)
{
    int id = args[0].i;
    int n = args[1].i;
    float v[4];
    if (id > 0) id--;
    v[0] = args[2].f;
    v[1] = args[3].f;
    v[2] = args[4].f;
    v[3] = 1.0f;
    if (n > 0)
        xglLightAngle(&GameIdLight[id], n - 1, v);
}

extern void xglLightDirection(void *light, int n, float *v);

void Java_xeno_util_Runtime_setIdLightVec__IIFFF(void *env, union { int i; float f; void *p; } *args)
{
    int id = args[0].i;
    int n = args[1].i;
    float v[4];
    if (id > 0) id--;
    v[0] = args[2].f;
    v[1] = args[3].f;
    v[2] = args[4].f;
    v[3] = 1.0f;
    if (n > 0)
        xglLightDirection(&GameIdLight[id], n - 1, v);
}

extern void EXM_StopWind(void);
extern void EXM_SetShakePower(float power);
extern void EXM_SetShakeTime(float time);
extern void EXM_SetDirectionalWind(float *dir);
extern void EXM_SetPointWind(float *pos);

void Java_xeno_util_Runtime_setWindParam__IFFFF(void *env, union { int i; float f; void *p; } *args)
{
    float v[4];
    v[0] = args[1].f;
    v[1] = args[2].f;
    v[2] = args[3].f;
    v[3] = args[4].f;
    switch (args[0].i) {
    case 0:
        EXM_StopWind();
        break;
    case 1:
        EXM_SetShakePower(v[0]);
        EXM_SetShakeTime(v[1]);
        break;
    case 2:
        EXM_SetDirectionalWind(v);
        break;
    case 3:
        EXM_SetPointWind(v);
        break;
    }
}

static int s_nMpeg2ReserveCrossFade = 0;
extern int SCRIPT_fadeGet(void);
extern int GameMpeg2Play(char *name, int mode);

void Java_xeno_util_Runtime_mpeg2__Ljava_lang_String_(JThread *thread,
                                                      StringArgCall *arguments,
                                                      unsigned int *result)
{
    static char base[] = "data\\movie\\mpeg2\\";
    static char ext[] = ".pss";
    char path[64];
    char *dst = path;
    char *src = base;
    int mode;

    while ((*dst = *src) != 0) {
        src++;
        dst++;
    }
    src = arguments->string->value->bytes;
    while ((*dst = *src) != 0) {
        src++;
        dst++;
    }
    src = ext;
    while ((*dst = *src) != 0) {
        src++;
        dst++;
    }
    mode = 3;
    if (SCRIPT_fadeGet()) {
        mode = 2;
    }
    if (s_nMpeg2ReserveCrossFade) {
        s_nMpeg2ReserveCrossFade = 0;
        mode = 4;
    }
    GameMpeg2Play(path, mode);
}

extern void nmlModelSetMpeg2CrossFadeTime(int time);

void Java_xeno_util_Runtime_mpeg2AfterCrossFade__I(JThread *thread, int *arguments,
                                                    unsigned int *result)
{
    int time = *arguments;

    s_nMpeg2ReserveCrossFade = 1;
    nmlModelSetMpeg2CrossFadeTime(time);
}

unsigned char *UmnMailDataGet(int box_id);
void UmnMailBoxSet(int box_id);
void xglSoundEffectNormalID(int id, int channel);
void Vibration_Set_Weak(int t);
typedef struct { unsigned char read : 1; unsigned char b1 : 1; unsigned char pad : 2; unsigned char b4 : 2; } MailFlagView;

void Java_xeno_util_Runtime_mailFlag__I(JThread *thread, int *arguments, int *result)
{
    int ret = 0;
    unsigned char *m = UmnMailDataGet(arguments[0] & 0xFFFFFF);
    int n, i;
    signed char *work;

    switch (arguments[0] >> 24) {
    case 0:
        ret = m[0] & 1;
        break;
    case 1:
        ret = (m[0] & 2) != 0;
        break;
    case 2:
        if (m[1] != 0) {
            ret = (m[1] & 0x80) ? 2 : 1;
        } else {
            ret = 0;
        }
        break;
    case 3:
        ret = (m[0] & 0x30) >> 4;
        break;
    case 4:
        m[0] |= 1;
        UmnMailBoxSet(arguments[0] & 0xFFFFFF);
        xglSoundEffectNormalID(0x24, 0);
        Vibration_Set_Weak(0x1E);
        break;
    case 5:

        work = (signed char *)(SaveData + 0x1025C);
        n = 0;
        i = 0;
        if (work[0] >= 0) {
            n = 1;
            do {
                i++;
                if (i >= 0x80 || work[i] < 0)
                    break;
                n++;
            } while (1);
        }
        switch (arguments[0] & 0xFFFFFF) {
        case 0:
            ret = n;
            break;
        case 1:
            work = (signed char *)(SaveData + 0x1025C);
            ret = 0;
            for (i = 0; i < n; i++) {
                if (!(*UmnMailDataGet(work[i]) & 2))
                    ret++;
            }
            break;
        }
        break;
    }
    *result = ret;
}

extern void UmnMailMain(int mail);
extern void xglCullingIgnore(void);
extern void xglCullingIgnoreOff(void);

void Java_xeno_util_Runtime_mailExec__I(JThread *thread, int *arguments,
                                        unsigned int *result)
{
    GameStateSaveCameraLight();
    xglCullingIgnore();
    UmnMailMain(*arguments);
    xglCullingIgnoreOff();
    GameStateRestoreCameraLight();
}

extern void Game_Data_Pop(void);
extern void Game_Data_Push(void);
extern void func_00A00070(void);
extern void func_00A00338(void);
extern void func_00A01EA8(void);
extern void Vibration_Stop(void);
extern void xglPadRead(void);
extern void ACT_init(void);
extern void xglCdLoadOverlay(int);
extern void xglRenderClearDepth(void);

void Java_xeno_util_Runtime_minigameExec__I(JThread *thread, int *arguments)
{
    GameLoopState.flags |= 0x04000000;
    Vibration_Stop();
    xglPadRead();
    Game_Data_Push();
    ACT_init();
    switch (arguments[0]) {
    case 12:
        xglCdLoadOverlay(0xC);
        func_00A01EA8();
        break;
    case 13:
        xglCdLoadOverlay(0xA);
        func_00A00338();
        xglRenderClearDepth();
        break;
    case 14:
        xglCdLoadOverlay(0xB);
        func_00A00070();
        break;
    }
    xglCdLoadOverlay(1);
    ACT_init();
    Game_Data_Pop();
    GameCfPlayerLoadResource(1);
    GameLoopState.flags &= 0xFBFFFFFF;
}

void Java_xeno_util_Runtime_setMenuLock__(JThread *thread, void *arguments,
                                          unsigned int *result)
{
    /*
     * Arms the script scene-change timer, which SCRIPT_sceneChangeTimeDec
     * counts down once per game frame, with 62 frames.
     */
    SCRIPT_sceneChangeTimeSet(62);
}

void Java_xeno_util_Runtime_evsSetRetPoint__(JThread *thread, void *arguments, unsigned int *result)
{
    GameLoopState.active_event_id = GameLoopState.event_id;
    GameLoopState.saved_position = GameLoopState.player->position;
}

extern void (*jthreadResetFunc)(void);
extern void scriptReset_evsExit(void);

void Java_xeno_util_Runtime_evsExit__(RuntimeThreadState *thread, void *arguments,
                                      unsigned int *result)
{
    /* Blocks the calling thread the same way CHR_sclX/System_waitFor do
       (chr.h): resume_frames takes the current frame_depth and flags gains
       0x8 (native-call block) and 0x1 (generic wait). The updated flags word
       is staged before the resume_frames store and written back after it,
       matching the original instruction order (0x002f873c..0x002f8750). */
    unsigned int flags;

    args_jump.destination = GameLoopState.active_event_id;
    args_jump.argument = -1;
    jthreadResetFunc = scriptReset_evsExit;
    flags = thread->flags | 9;
    thread->resume_frames = thread->frame_depth;
    thread->flags = flags;
}

extern void dataEtherTecSet(int value);

void Java_xeno_util_Runtime_etherTecSet__I(JThread *thread, int *arguments,
                                           unsigned int *result)
{
    dataEtherTecSet(*arguments);
}

void Java_xeno_util_Runtime_charAllRecovery__(JThread *thread, void *arguments,
                                              unsigned int *result)
{
    CharactorAllRecovery();
}

void Java_xeno_util_Runtime_AGWSAllRecovery__(JThread *thread, void *arguments,
                                              unsigned int *result)
{
    AgwsAllRecovery();
}

extern void Java_xeno_vm_System_arraycopy__Ljava_lang_Object_ILjava_lang_Object_II();
extern void Java_xeno_vm_System_sleep__I();
extern void Java_xeno_vm_System_println__Ljava_lang_String_();
extern void Java_xeno_vm_System_waitFor__Ljava_lang_Object_();
extern void Java_xeno_vm_System_methodSignal__I();
extern void Java_xeno_vm_Thread_create__();
extern void Java_xeno_vm_Thread_setTarget__Ljava_lang_Object_Ljava_lang_String_();
extern void Java_xeno_vm_Thread_start__();
extern void Java_xeno_vm_Thread_stop__();
extern void Java_xeno_vm_Math_random__();
extern void Java_xeno_vm_Math_atan2__FF();
extern void Java_xeno_vm_Math_cos__F();
extern void Java_xeno_vm_Math_sin__F();
extern void Java_xeno_util_Spline_create__();
extern void Java_xeno_util_Spline_setCtrlVertex__aFIII();
extern void Java_xeno_util_Spline_getValue__I();
extern void Java_xeno_util_Format_floatToIntBits__F();
extern void Java_xeno_util_Format_intBitsToFloat__I();
extern void Java_xeno_util_Format_toInt__Ljava_lang_String_();
extern void Java_xeno_util_Format_toString__C();
extern void Java_xeno_util_Format_toString__F();
extern void Java_xeno_util_Format_toString__I();
extern void Java_xeno_util_Format_toString__Z();
extern void Java_xeno_util_Input_create__I();
extern void Java_xeno_util_Input_getButton__();
extern void Java_xeno_util_Input_getEdge__();
extern void Java_xeno_util_Input_getRepeat__();
extern void Java_xeno_util_Layout_set__Ljava_lang_Object_I();
extern void Java_xeno_util_Layout_getManager__I();
extern void Java_xeno_util_Window_getSignal__();
extern void Java_xeno_util_Window_signal__I();
extern void Java_xeno_util_Window_clear__();
extern void Java_xeno_util_Window_close__();
extern void Java_xeno_util_Window_waitkey__I();
extern void Java_xeno_util_Window_wait__I();
extern void Java_xeno_util_Window_create__I();
extern void Java_xeno_util_Window_setSize__II();
extern void Java_xeno_util_Window_setLocation__II();
extern void Java_xeno_util_Window_print__Ljava_lang_String_();
extern void Java_xeno_util_Window_print__aLjava_lang_String_I();
extern void Java_xeno_util_Window_setName__Ljava_lang_String_();
extern void Java_xeno_util_Window_closeWaitKey__();
extern void Java_xeno_util_Menu_addQuery__Ljava_lang_String_();
extern void Java_xeno_util_Menu_addQuery__aLjava_lang_String_I();
extern void Java_xeno_util_Menu_addItem__Ljava_lang_String_();
extern void Java_xeno_util_Menu_create__();
extern void Java_xeno_util_Menu_getSelected__();
extern void Java_xeno_util_Menu_setLocation__II();
extern void Java_xeno_util_Menu_setVisible__Z();
extern void Java_xeno_util_Menu_setCursor__I();
extern void Java_xeno_util_Toolkit_getPeer__Ljava_lang_Object_();
extern void Java_xeno_util_Toolkit_call__Ljava_lang_Object_Ljava_lang_String_();
extern void Java_xeno_util_Toolkit_loadResource__Ljava_lang_Object_I();
extern void Java_xeno_util_Toolkit_loadResource__Ljava_lang_String_();
extern void Java_xeno_util_Toolkit_loadResource__Ljava_lang_Object_Ljava_lang_Object_I();
extern void Java_xeno_util_Toolkit_loadResource__Ljava_lang_Object_II();
extern void Java_xeno_util_Toolkit_peerSetGroup__II();
extern void Java_xeno_Sound_sequencePlay__I();
extern void Java_xeno_Sound_sequencePlay__II();
extern void Java_xeno_Sound_sequenceStop__I();
extern void Java_xeno_Sound_sequenceStop__II();
extern void Java_xeno_Sound_effectPlay__III();
extern void Java_xeno_Sound_effectStop__I();
extern void Java_xeno_Sound_streamPlay__IIII();
extern void Java_xeno_Movie_init__I();
extern void Java_xeno_Movie_start__I();
extern void Java_xeno_Movie_stop__();
extern void Java_xeno_Movie_update__();
extern void Java_xeno_PlayControl_create__();
extern void Java_xeno_PlayControl_getSignal__();
extern void Java_xeno_PlayControl_signal__I();
extern void Java_xeno_PlayControl_init__II();
extern void Java_xeno_PlayControl_init__IIIIF();
extern void Java_xeno_PlayControl_loadCamera__Ljava_lang_Object_();
extern void Java_xeno_PlayControl_loadTimeChart__Ljava_lang_Object_();
extern void Java_xeno_PlayControl_start__();
extern void Java_xeno_PlayControl_stop__();
extern void Java_xeno_PlayControl_setObserver__IILjava_lang_Object_Ljava_lang_String_();
extern void Java_xeno_PlayControl_setObserver__ILjava_lang_String_Ljava_lang_Object_Ljava_lang_String_();
extern void Java_xeno_PlayControl_getParams__();
extern void Java_xeno_Effect_call__I();
extern void Java_xeno_Effect_disp__Z();
extern void Java_xeno_Effect_setScale__FFF();
extern void Java_xeno_Effect_getScale__();
extern void Java_xeno_Effect_getTranslate__();
extern void Java_xeno_Effect_setTranslate__();
extern void Java_xeno_Effect_getRotate__();
extern void Java_xeno_Effect_setRotate__();
extern void Java_xeno_Effect_setCaster__Lxeno_Chr_();
extern void Java_xeno_Effect_setTarget__Lxeno_Chr_();
extern void Java_xeno_Effect_setCaster__Lxeno_Unit_();
extern void Java_xeno_Effect_setTarget__Lxeno_Unit_();
extern void Java_xeno_Effect_setTransOffset__FFF();
extern void Java_xeno_Effect_getForceLoop__();
extern void Java_xeno_Effect_setForceLoop__Z();
extern void Java_xeno_Effect_getClip__();
extern void Java_xeno_Effect_setClip__Z();
extern void Java_xeno_Effect_clearEffect__();
extern void Java_xeno_Effect_setMotion__Z();
extern void Java_xeno_Effect_noAttach__Z();
extern void Java_xeno_Camera_getRotateX__();
extern void Java_xeno_Camera_getRotateY__();
extern void Java_xeno_Camera_getRotateZ__();
extern void Java_xeno_Camera_getTranslateX__();
extern void Java_xeno_Camera_getTranslateY__();
extern void Java_xeno_Camera_getTranslateZ__();
extern void Java_xeno_Camera_fovSPL__aFI();
extern void Java_xeno_Camera_change__();
extern void Java_xeno_Camera_rotateSPL__aFI();
extern void Java_xeno_Camera_rotateSPL__aFIII();
extern void Java_xeno_Camera_setActive__Z();
extern void Java_xeno_Camera_create__I();
extern void Java_xeno_Camera_transCNS__Ljava_lang_Object_FFF();
extern void Java_xeno_Camera_transSPL__aFI();
extern void Java_xeno_Camera_transSPL__aFIII();
extern void Java_xeno_Camera_viewCNS__Ljava_lang_Object_FFF();
extern void Java_xeno_Camera_viewSPL__aFI();
extern void Java_xeno_Camera_viewSPL__aFIII();
extern void Java_xeno_Camera_rollSPL__aFI();
extern void Java_xeno_Camera_setRotate__FFF();
extern void Java_xeno_Camera_setRoll__F();
extern void Java_xeno_Camera_setTranslate__FFF();
extern void Java_xeno_Camera_setFov__F();
extern void Java_xeno_Camera_getFov__();
extern void Java_xeno_Camera_setView__FFF();
extern void Java_xeno_Camera_start__ILjava_lang_Object_();
extern void Java_xeno_Camera_setCFAngle__IFFFF();
extern void Java_xeno_Camera_setCFAnglePers__IFFFFF();
extern void Java_xeno_Camera_setCFHokan__IFF();
extern void Java_xeno_Camera_setCFLock__IIFFF();
extern void Java_xeno_Camera_setCFOffset__IFFFIFIF();
extern void Java_xeno_Camera_setMode__I();
extern void Java_xeno_Camera_getMode__();
extern void Java_xeno_Camera_setCFPedestal__IFFFFFFFF();
extern void Java_xeno_Camera_setCFPedestalHokan__II();
extern void Java_xeno_Camera_changeID__III();
extern void Java_xeno_Camera_setFog__IFFFFIIII();
extern void Java_xeno_Camera_resetFog__I();
extern void Java_xeno_Camera_setClipRange__FF();
extern void Java_xeno_Light_setColor__FFF();
extern void Java_xeno_Light_setDirection__FFF();
extern void Java_xeno_Light_setDirection2__FFF();
extern void Java_xeno_Light_setGlobalPointLightCol__IFFF();
extern void Java_xeno_Light_setGlobalPointLightPos__IFFF();
extern void Java_xeno_Light_setGlobalPointLightReset__();
extern void Java_xeno_Chr_getPlayer__();
extern void Java_xeno_Chr_setPlayer__();
extern void Java_xeno_Chr_move__IFFZ();
extern void Java_xeno_Chr_move__FFFZ();
extern void Java_xeno_Chr_move__ILjava_lang_Object_Z();
extern void Java_xeno_Chr_move__Ljava_lang_Object_FZ();
extern void Java_xeno_Chr_move__Lxeno_util_Spline_IZ();
extern void Java_xeno_Chr_rotate__Lxeno_util_Spline_IZ();
extern void Java_xeno_Chr_rotX__FFZ();
extern void Java_xeno_Chr_rotX__IFZ();
extern void Java_xeno_Chr_rotX__ILjava_lang_Object_Z();
extern void Java_xeno_Chr_rotX__Ljava_lang_Object_FZ();
extern void Java_xeno_Chr_rotY__FFZ();
extern void Java_xeno_Chr_rotY__IFZ();
extern void Java_xeno_Chr_rotY__ILjava_lang_Object_Z();
extern void Java_xeno_Chr_rotY__Ljava_lang_Object_FZ();
extern void Java_xeno_Chr_rotZ__FFZ();
extern void Java_xeno_Chr_rotZ__IFZ();
extern void Java_xeno_Chr_rotZ__ILjava_lang_Object_Z();
extern void Java_xeno_Chr_rotZ__Ljava_lang_Object_FZ();
extern void Java_xeno_Chr_start__ILjava_lang_Object_();
extern void Java_xeno_Chr_stop__();
extern void Java_xeno_Chr_mtn__IIIIIFZ();
extern void Java_xeno_Chr_mtn__IIFZ();
extern void Java_xeno_Chr_signal__I();
extern void Java_xeno_Chr_getSignal__();
extern void Java_xeno_Chr_setRotate__();
extern void Java_xeno_Chr_setTranslate__();
extern void Java_xeno_Chr_getRotate__();
extern void Java_xeno_Chr_getTranslate__();
extern void Java_xeno_Chr_setVisible__Z();
extern void Java_xeno_Chr_setVisible__IZ();
extern void Java_xeno_Chr_setCollision__Z();
extern void Java_xeno_Chr_setHand__I();
extern void Java_xeno_Chr_setArgs__III();
extern void Java_xeno_Chr_getArgs__II();
extern void Java_xeno_Chr_setArgs__ILjava_lang_Object_I();
extern void Java_xeno_Chr_getSerial__();
extern void Java_xeno_Chr_getState__();
extern void Java_xeno_Chr_mtnGetRoot__ILxeno_util_Vector4f_();
extern void Java_xeno_Chr_setScale__FFF();
extern void Java_xeno_Chr_getScale__();
extern void Java_xeno_Chr_scale__Lxeno_util_Spline_IZ();
extern void Java_xeno_Chr_sclX__FFZ();
extern void Java_xeno_Chr_sclX__IFZ();
extern void Java_xeno_Chr_sclY__FFZ();
extern void Java_xeno_Chr_sclY__IFZ();
extern void Java_xeno_Chr_sclZ__FFZ();
extern void Java_xeno_Chr_sclZ__IFZ();
extern void Java_xeno_Chr_rotCNS__ILjava_lang_Object_();
extern void Java_xeno_Chr_rotCNS__IFFF();
extern void Java_xeno_Chr_setRotCNSParam__IFFFFF();
extern void Java_xeno_Chr_relax__II();
extern void Java_xeno_Chr_getFlags__();
extern void Java_xeno_Chr_setFlags__I();
extern void Java_xeno_Chr_setEdgeFall__I();
extern void Java_xeno_Chr_setShadow__II();
extern void Java_xeno_Chr_setShadow__aB();
extern void Java_xeno_Chr_setID__I();
extern void Java_xeno_Chr_setElevatorMode__I();
extern void Java_xeno_Chr_setParent__Lxeno_Chr_III();
extern void Java_xeno_Chr_setMotionFlags__IZ();
extern void Java_xeno_Chr_setFilter__I();
extern void Java_xeno_Chr_setFilterParam__aF();
extern void Java_xeno_Chr_setClip__I();
extern void Java_xeno_Chr_setSymmetryY__I();
extern void Java_xeno_Chr_setSortOffset__F();
extern void Java_xeno_Chr_setPointLightCol__IFFF();
extern void Java_xeno_Chr_setPointLightPos__IFFF();
extern void Java_xeno_Chr_setPointLightReset__();
extern void Java_xeno_Chr_talkto__Ljava_lang_String_();
extern void Java_xeno_Chr_touchto__Ljava_lang_String_();
extern void Java_xeno_Chr_childGetPeer__II();
extern void Java_xeno_Chr_setPeer__Ljava_lang_Object_();
extern void Java_xeno_Chr_dispRadar__Z();
extern void Java_xeno_Chr_look_camera__();
extern void Java_xeno_Chr_look_char__Ljava_lang_Object_();
extern void Java_xeno_Chr_look_unit__Ljava_lang_Object_();
extern void Java_xeno_Chr_look_default__();
extern void Java_xeno_Chr_look_point__FFF();
extern void Java_xeno_Chr_look_eye_set__FF();
extern void Java_xeno_Chr_look_eye_control__I();
extern void Java_xeno_Chr_look_speed__F();
extern void Java_xeno_Chr_look_eye_speed__F();
extern void Java_xeno_Chr_renderCommand__I();
extern void Java_xeno_Chr_shadow_clip_scale__F();
extern void Java_xeno_Chr_shadow_map_id__I();
extern void Java_xeno_Chr_shadow_map_reset__();
extern void Java_xeno_Chr_hairStop__II();
extern void Java_xeno_Chr_pixelAlpha__I();
extern void Java_xeno_Chr_pixelAlphaParts__II();
extern void Java_xeno_Chr_pixelAlphaPartsReset__();
extern void Java_xeno_Chr_setMotNoUpdate__I();
extern void Java_xeno_Chr_setWeaponR__Lxeno_Chr_();
extern void Java_xeno_Chr_resetWeaponR__Lxeno_Chr_();
extern void Java_xeno_Chr_resetHand__();
extern void Java_xeno_Chr_resetEnv__();
extern void Java_xeno_Chr_ignoreShape__I();
extern void Java_xeno_Unit_rotYCNS__Ljava_lang_Object_IFFF();
extern void Java_xeno_Unit_transCNS__Ljava_lang_Object_IFFF();
extern void Java_xeno_Unit_getRotate__();
extern void Java_xeno_Unit_getSignal__();
extern void Java_xeno_Unit_getTranslate__();
extern void Java_xeno_Unit_invalidate__();
extern void Java_xeno_Unit_move__FFFZ();
extern void Java_xeno_Unit_move__IFFZ();
extern void Java_xeno_Unit_move__Lxeno_util_Spline_IZ();
extern void Java_xeno_Unit_rotate__Lxeno_util_Spline_IZ();
extern void Java_xeno_Unit_move__ILjava_lang_Object_Z();
extern void Java_xeno_Unit_move__Ljava_lang_Object_FZ();
extern void Java_xeno_Unit_mtn__IIFZ();
extern void Java_xeno_Unit_mtn__IIIIIFZ();
extern void Java_xeno_Unit_rotX__FFZ();
extern void Java_xeno_Unit_rotX__IFZ();
extern void Java_xeno_Unit_rotX__ILjava_lang_Object_Z();
extern void Java_xeno_Unit_rotX__Ljava_lang_Object_FZ();
extern void Java_xeno_Unit_rotY__FFZ();
extern void Java_xeno_Unit_rotY__IFZ();
extern void Java_xeno_Unit_rotY__ILjava_lang_Object_Z();
extern void Java_xeno_Unit_rotY__Ljava_lang_Object_FZ();
extern void Java_xeno_Unit_rotZ__FFZ();
extern void Java_xeno_Unit_rotZ__IFZ();
extern void Java_xeno_Unit_rotZ__ILjava_lang_Object_Z();
extern void Java_xeno_Unit_rotZ__Ljava_lang_Object_FZ();
extern void Java_xeno_Unit_scale__Lxeno_util_Spline_IZ();
extern void Java_xeno_Unit_sclX__FFZ();
extern void Java_xeno_Unit_sclX__IFZ();
extern void Java_xeno_Unit_sclY__FFZ();
extern void Java_xeno_Unit_sclY__IFZ();
extern void Java_xeno_Unit_sclZ__FFZ();
extern void Java_xeno_Unit_sclZ__IFZ();
extern void Java_xeno_Unit_setCollision__Z();
extern void Java_xeno_Unit_setRotate__();
extern void Java_xeno_Unit_setTranslate__();
extern void Java_xeno_Unit_setVisible__IZ();
extern void Java_xeno_Unit_setVisible__Z();
extern void Java_xeno_Unit_signal__I();
extern void Java_xeno_Unit_start__ILjava_lang_Object_();
extern void Java_xeno_Unit_stop__();
extern void Java_xeno_Unit_validate__();
extern void Java_xeno_Unit_setParent__Ljava_lang_Object_I();
extern void Java_xeno_Unit_setArgs__III();
extern void Java_xeno_Unit_getArgs__II();
extern void Java_xeno_Unit_setArgs__ILjava_lang_Object_I();
extern void Java_xeno_Unit_setArgs__IIIII();
extern void Java_xeno_Unit_getScale__();
extern void Java_xeno_Unit_setScale__FFF();
extern void Java_xeno_Unit_getSerial__();
extern void Java_xeno_Unit_mtnSetMask__I();
extern void Java_xeno_Unit_mtnGetRoot__ILxeno_util_Vector4f_();
extern void Java_xeno_Unit_getState__();
extern void Java_xeno_Unit_getPivot__Lxeno_util_Vector4f_();
extern void Java_xeno_Unit_setPivot__FFF();
extern void Java_xeno_Unit_getAxis__Lxeno_util_Vector4f_();
extern void Java_xeno_Unit_setAxis__FFFF();
extern void Java_xeno_Unit_suspend__I();
extern void Java_xeno_Unit_resume__I();
extern void Java_xeno_Unit_initElevatorFunc__();
extern void Java_xeno_Unit_map_shadow__I();
extern void Java_xeno_Unit_renderCommand__I();
extern void Java_xeno_Unit_setFilter__I();
extern void Java_xeno_Unit_setFilterParam__aF();
extern void Java_xeno_Unit_setShadow__II();
extern void Java_xeno_Unit_shadow_clip_scale__F();
extern void Java_xeno_Unit_shadow_map_id__I();
extern void Java_xeno_Unit_shadow_map_reset__();
extern void Java_xeno_Unit_setSortOffset__F();
extern void Java_xeno_Unit_setClip__I();
extern void Java_xeno_Unit_setMonitorPrio__I();
extern void Java_xeno_Scene_start__ILjava_lang_Object_();
extern void Java_xeno_Scene_stop__();
extern void Java_xeno_Stage_start__ILjava_lang_Object_();
extern void Java_xeno_Stage_stop__();
extern void Java_xeno_Stage_play__Ljava_lang_String_();
extern void Java_xeno_Stage_setPartsLast__I();
extern void Java_xeno_Stage_setPartsLastReset__();
extern void Java_xeno_Stage_setColor__FFF();
extern void Java_xeno_Stage_setFade__IIFFF();
extern void Java_xeno_Stage_setEventFade__IFFFIFFF();
extern void Java_xeno_Stage_setFrameRender__II();
extern void Java_xeno_Stage_setFadeCancel__I();
extern void Java_xeno_Stage_setVisible__IZ();
extern void Java_xeno_Stage_setCFBG__II_F();
extern void Java_xeno_Stage_setEffectRender__I();
extern void Java_xeno_Stage_renderCommand__I();
extern void Java_xeno_Stage_setBgColor__FFF();
extern void Java_xeno_Stage_setBgClip__I();
extern void Java_xeno_Stage_clrBackBuffer__();

/* The native-method table findNativeMethod (main's find_native_method TU)
   searches: each Java native's exported name and its entry point, closed by
   a NULL name. */
RuntimeNativeBinding default_native[] = {
    { "Java_xeno_vm_System_arraycopy__Ljava_lang_Object_ILjava_lang_Object_II", (void (*)())Java_xeno_vm_System_arraycopy__Ljava_lang_Object_ILjava_lang_Object_II },
    { "Java_xeno_vm_System_sleep__I", (void (*)())Java_xeno_vm_System_sleep__I },
    { "Java_xeno_vm_System_println__Ljava_lang_String_", (void (*)())Java_xeno_vm_System_println__Ljava_lang_String_ },
    { "Java_xeno_vm_System_waitFor__Ljava_lang_Object_", (void (*)())Java_xeno_vm_System_waitFor__Ljava_lang_Object_ },
    { "Java_xeno_vm_System_methodSignal__I", (void (*)())Java_xeno_vm_System_methodSignal__I },
    { "Java_xeno_vm_Thread_create__", (void (*)())Java_xeno_vm_Thread_create__ },
    { "Java_xeno_vm_Thread_setTarget__Ljava_lang_Object_Ljava_lang_String_", (void (*)())Java_xeno_vm_Thread_setTarget__Ljava_lang_Object_Ljava_lang_String_ },
    { "Java_xeno_vm_Thread_start__", (void (*)())Java_xeno_vm_Thread_start__ },
    { "Java_xeno_vm_Thread_stop__", (void (*)())Java_xeno_vm_Thread_stop__ },
    { "Java_xeno_vm_Math_random__", (void (*)())Java_xeno_vm_Math_random__ },
    { "Java_xeno_vm_Math_atan2__FF", (void (*)())Java_xeno_vm_Math_atan2__FF },
    { "Java_xeno_vm_Math_cos__F", (void (*)())Java_xeno_vm_Math_cos__F },
    { "Java_xeno_vm_Math_sin__F", (void (*)())Java_xeno_vm_Math_sin__F },
    { "Java_xeno_util_Spline_create__", (void (*)())Java_xeno_util_Spline_create__ },
    { "Java_xeno_util_Spline_setCtrlVertex__aFIII", (void (*)())Java_xeno_util_Spline_setCtrlVertex__aFIII },
    { "Java_xeno_util_Spline_getValue__I", (void (*)())Java_xeno_util_Spline_getValue__I },
    { "Java_xeno_util_Format_floatToIntBits__F", (void (*)())Java_xeno_util_Format_floatToIntBits__F },
    { "Java_xeno_util_Format_intBitsToFloat__I", (void (*)())Java_xeno_util_Format_intBitsToFloat__I },
    { "Java_xeno_util_Format_toInt__Ljava_lang_String_", (void (*)())Java_xeno_util_Format_toInt__Ljava_lang_String_ },
    { "Java_xeno_util_Format_toString__C", (void (*)())Java_xeno_util_Format_toString__C },
    { "Java_xeno_util_Format_toString__F", (void (*)())Java_xeno_util_Format_toString__F },
    { "Java_xeno_util_Format_toString__I", (void (*)())Java_xeno_util_Format_toString__I },
    { "Java_xeno_util_Format_toString__Z", (void (*)())Java_xeno_util_Format_toString__Z },
    { "Java_xeno_util_Input_create__I", (void (*)())Java_xeno_util_Input_create__I },
    { "Java_xeno_util_Input_getButton__", (void (*)())Java_xeno_util_Input_getButton__ },
    { "Java_xeno_util_Input_getEdge__", (void (*)())Java_xeno_util_Input_getEdge__ },
    { "Java_xeno_util_Input_getRepeat__", (void (*)())Java_xeno_util_Input_getRepeat__ },
    { "Java_xeno_util_Layout_set__Ljava_lang_Object_I", (void (*)())Java_xeno_util_Layout_set__Ljava_lang_Object_I },
    { "Java_xeno_util_Layout_getManager__I", (void (*)())Java_xeno_util_Layout_getManager__I },
    { "Java_xeno_util_Runtime_execBattle__II", (void (*)())Java_xeno_util_Runtime_execBattle__II },
    { "Java_xeno_util_Runtime_getFlags__II", (void (*)())Java_xeno_util_Runtime_getFlags__II },
    { "Java_xeno_util_Runtime_setFlags__III", (void (*)())Java_xeno_util_Runtime_setFlags__III },
    { "Java_xeno_util_Runtime_setLocation__III", (void (*)())Java_xeno_util_Runtime_setLocation__III },
    { "Java_xeno_util_Runtime_getLocation__", (void (*)())Java_xeno_util_Runtime_getLocation__ },
    { "Java_xeno_util_Runtime_setRegister__II", (void (*)())Java_xeno_util_Runtime_setRegister__II },
    { "Java_xeno_util_Runtime_getRegister__I", (void (*)())Java_xeno_util_Runtime_getRegister__I },
    { "Java_xeno_util_Runtime_getEntrance__", (void (*)())Java_xeno_util_Runtime_getEntrance__ },
    { "Java_xeno_util_Runtime_setPlayerControl__Z", (void (*)())Java_xeno_util_Runtime_setPlayerControl__Z },
    { "Java_xeno_util_Runtime_setPlayerMoveParam__FFF", (void (*)())Java_xeno_util_Runtime_setPlayerMoveParam__FFF },
    { "Java_xeno_util_Runtime_disable__I", (void (*)())Java_xeno_util_Runtime_disable__I },
    { "Java_xeno_util_Runtime_enable__I", (void (*)())Java_xeno_util_Runtime_enable__I },
    { "Java_xeno_util_Runtime_getGameState__", (void (*)())Java_xeno_util_Runtime_getGameState__ },
    { "Java_xeno_util_Runtime_CaptureEnd__", (void (*)())Java_xeno_util_Runtime_CaptureEnd__ },
    { "Java_xeno_util_Runtime_CaptureStart__Ljava_lang_String_I", (void (*)())Java_xeno_util_Runtime_CaptureStart__Ljava_lang_String_I },
    { "Java_xeno_util_Runtime_jumpCF__II", (void (*)())Java_xeno_util_Runtime_jumpCF__II },
    { "Java_xeno_util_Runtime_jumpEvent__I", (void (*)())Java_xeno_util_Runtime_jumpEvent__I },
    { "Java_xeno_util_Runtime_setDefocusQuick__IIII", (void (*)())Java_xeno_util_Runtime_setDefocusQuick__IIII },
    { "Java_xeno_util_Runtime_setDefocus__IIaI", (void (*)())Java_xeno_util_Runtime_setDefocus__IIaI },
    { "Java_xeno_util_Runtime_setDefocusParam__III", (void (*)())Java_xeno_util_Runtime_setDefocusParam__III },
    { "Java_xeno_util_Runtime_setEventTimer__Ljava_lang_String_I", (void (*)())Java_xeno_util_Runtime_setEventTimer__Ljava_lang_String_I },
    { "Java_xeno_util_Runtime_setMap__II", (void (*)())Java_xeno_util_Runtime_setMap__II },
    { "Java_xeno_util_Runtime_setShootFlag__Z", (void (*)())Java_xeno_util_Runtime_setShootFlag__Z },
    { "Java_xeno_util_Runtime_setShootHeightCheck__Z", (void (*)())Java_xeno_util_Runtime_setShootHeightCheck__Z },
    { "Java_xeno_util_Runtime_setShootIDCheck__Z", (void (*)())Java_xeno_util_Runtime_setShootIDCheck__Z },
    { "Java_xeno_util_Runtime_setShootUwaCheck__Z", (void (*)())Java_xeno_util_Runtime_setShootUwaCheck__Z },
    { "Java_xeno_util_Runtime_setShootRange__F", (void (*)())Java_xeno_util_Runtime_setShootRange__F },
    { "Java_xeno_util_Runtime_addItem__II", (void (*)())Java_xeno_util_Runtime_addItem__II },
    { "Java_xeno_util_Runtime_addItemWin__II", (void (*)())Java_xeno_util_Runtime_addItemWin__II },
    { "Java_xeno_util_Runtime_removeItem__II", (void (*)())Java_xeno_util_Runtime_removeItem__II },
    { "Java_xeno_util_Runtime_checkItem__II", (void (*)())Java_xeno_util_Runtime_checkItem__II },
    { "Java_xeno_util_Runtime_getItemName__II", (void (*)())Java_xeno_util_Runtime_getItemName__II },
    { "Java_xeno_util_Runtime_addGold__I", (void (*)())Java_xeno_util_Runtime_addGold__I },
    { "Java_xeno_util_Runtime_removeGold__I", (void (*)())Java_xeno_util_Runtime_removeGold__I },
    { "Java_xeno_util_Runtime_checkGold__", (void (*)())Java_xeno_util_Runtime_checkGold__ },
    { "Java_xeno_util_Runtime_progressEffect__I", (void (*)())Java_xeno_util_Runtime_progressEffect__I },
    { "Java_xeno_util_Runtime_enterShop__I", (void (*)())Java_xeno_util_Runtime_enterShop__I },
    { "Java_xeno_util_Runtime_getLeader__", (void (*)())Java_xeno_util_Runtime_getLeader__ },
    { "Java_xeno_util_Runtime_getPartyData__I", (void (*)())Java_xeno_util_Runtime_getPartyData__I },
    { "Java_xeno_util_Runtime_setPartyData__II", (void (*)())Java_xeno_util_Runtime_setPartyData__II },
    { "Java_xeno_util_Runtime_setFriend__I", (void (*)())Java_xeno_util_Runtime_setFriend__I },
    { "Java_xeno_util_Runtime_resetFriend__I", (void (*)())Java_xeno_util_Runtime_resetFriend__I },
    { "Java_xeno_util_Runtime_checkFriend__I", (void (*)())Java_xeno_util_Runtime_checkFriend__I },
    { "Java_xeno_util_Runtime_setLockParty__I", (void (*)())Java_xeno_util_Runtime_setLockParty__I },
    { "Java_xeno_util_Runtime_resetLockParty__I", (void (*)())Java_xeno_util_Runtime_resetLockParty__I },
    { "Java_xeno_util_Runtime_checkLockParty__I", (void (*)())Java_xeno_util_Runtime_checkLockParty__I },
    { "Java_xeno_util_Runtime_setOutFriend__I", (void (*)())Java_xeno_util_Runtime_setOutFriend__I },
    { "Java_xeno_util_Runtime_resetOutFriend__I", (void (*)())Java_xeno_util_Runtime_resetOutFriend__I },
    { "Java_xeno_util_Runtime_checkOutFriend__I", (void (*)())Java_xeno_util_Runtime_checkOutFriend__I },
    { "Java_xeno_util_Runtime_setTakeAgws__I", (void (*)())Java_xeno_util_Runtime_setTakeAgws__I },
    { "Java_xeno_util_Runtime_resetTakeAgws__I", (void (*)())Java_xeno_util_Runtime_resetTakeAgws__I },
    { "Java_xeno_util_Runtime_checkTakeAgws__I", (void (*)())Java_xeno_util_Runtime_checkTakeAgws__I },
    { "Java_xeno_util_Runtime_battleChangeParty__I", (void (*)())Java_xeno_util_Runtime_battleChangeParty__I },
    { "Java_xeno_util_Runtime_setIdLightCol__IIFFF", (void (*)())Java_xeno_util_Runtime_setIdLightCol__IIFFF },
    { "Java_xeno_util_Runtime_setIdLightDir__IIFFF", (void (*)())Java_xeno_util_Runtime_setIdLightDir__IIFFF },
    { "Java_xeno_util_Runtime_setIdLightVec__IIFFF", (void (*)())Java_xeno_util_Runtime_setIdLightVec__IIFFF },
    { "Java_xeno_util_Runtime_setWindParam__IFFFF", (void (*)())Java_xeno_util_Runtime_setWindParam__IFFFF },
    { "Java_xeno_util_Runtime_mpeg2__Ljava_lang_String_", (void (*)())Java_xeno_util_Runtime_mpeg2__Ljava_lang_String_ },
    { "Java_xeno_util_Runtime_mpeg2AfterCrossFade__I", (void (*)())Java_xeno_util_Runtime_mpeg2AfterCrossFade__I },
    { "Java_xeno_util_Runtime_mailFlag__I", (void (*)())Java_xeno_util_Runtime_mailFlag__I },
    { "Java_xeno_util_Runtime_mailExec__I", (void (*)())Java_xeno_util_Runtime_mailExec__I },
    { "Java_xeno_util_Runtime_minigameExec__I", (void (*)())Java_xeno_util_Runtime_minigameExec__I },
    { "Java_xeno_util_Runtime_setMenuLock__", (void (*)())Java_xeno_util_Runtime_setMenuLock__ },
    { "Java_xeno_util_Runtime_evsSetRetPoint__", (void (*)())Java_xeno_util_Runtime_evsSetRetPoint__ },
    { "Java_xeno_util_Runtime_evsExit__", (void (*)())Java_xeno_util_Runtime_evsExit__ },
    { "Java_xeno_util_Runtime_etherTecSet__I", (void (*)())Java_xeno_util_Runtime_etherTecSet__I },
    { "Java_xeno_util_Runtime_charAllRecovery__", (void (*)())Java_xeno_util_Runtime_charAllRecovery__ },
    { "Java_xeno_util_Runtime_AGWSAllRecovery__", (void (*)())Java_xeno_util_Runtime_AGWSAllRecovery__ },
    { "Java_xeno_util_Window_getSignal__", (void (*)())Java_xeno_util_Window_getSignal__ },
    { "Java_xeno_util_Window_signal__I", (void (*)())Java_xeno_util_Window_signal__I },
    { "Java_xeno_util_Window_clear__", (void (*)())Java_xeno_util_Window_clear__ },
    { "Java_xeno_util_Window_close__", (void (*)())Java_xeno_util_Window_close__ },
    { "Java_xeno_util_Window_waitkey__I", (void (*)())Java_xeno_util_Window_waitkey__I },
    { "Java_xeno_util_Window_wait__I", (void (*)())Java_xeno_util_Window_wait__I },
    { "Java_xeno_util_Window_create__I", (void (*)())Java_xeno_util_Window_create__I },
    { "Java_xeno_util_Window_setSize__II", (void (*)())Java_xeno_util_Window_setSize__II },
    { "Java_xeno_util_Window_setLocation__II", (void (*)())Java_xeno_util_Window_setLocation__II },
    { "Java_xeno_util_Window_print__Ljava_lang_String_", (void (*)())Java_xeno_util_Window_print__Ljava_lang_String_ },
    { "Java_xeno_util_Window_print__aLjava_lang_String_I", (void (*)())Java_xeno_util_Window_print__aLjava_lang_String_I },
    { "Java_xeno_util_Window_setName__Ljava_lang_String_", (void (*)())Java_xeno_util_Window_setName__Ljava_lang_String_ },
    { "Java_xeno_util_Window_closeWaitKey__", (void (*)())Java_xeno_util_Window_closeWaitKey__ },
    { "Java_xeno_util_Menu_addQuery__Ljava_lang_String_", (void (*)())Java_xeno_util_Menu_addQuery__Ljava_lang_String_ },
    { "Java_xeno_util_Menu_addQuery__aLjava_lang_String_I", (void (*)())Java_xeno_util_Menu_addQuery__aLjava_lang_String_I },
    { "Java_xeno_util_Menu_addItem__Ljava_lang_String_", (void (*)())Java_xeno_util_Menu_addItem__Ljava_lang_String_ },
    { "Java_xeno_util_Menu_create__", (void (*)())Java_xeno_util_Menu_create__ },
    { "Java_xeno_util_Menu_getSelected__", (void (*)())Java_xeno_util_Menu_getSelected__ },
    { "Java_xeno_util_Menu_setLocation__II", (void (*)())Java_xeno_util_Menu_setLocation__II },
    { "Java_xeno_util_Menu_setVisible__Z", (void (*)())Java_xeno_util_Menu_setVisible__Z },
    { "Java_xeno_util_Menu_setCursor__I", (void (*)())Java_xeno_util_Menu_setCursor__I },
    { "Java_xeno_util_Toolkit_getPeer__Ljava_lang_Object_", (void (*)())Java_xeno_util_Toolkit_getPeer__Ljava_lang_Object_ },
    { "Java_xeno_util_Toolkit_call__Ljava_lang_Object_Ljava_lang_String_", (void (*)())Java_xeno_util_Toolkit_call__Ljava_lang_Object_Ljava_lang_String_ },
    { "Java_xeno_util_Toolkit_loadResource__Ljava_lang_Object_I", (void (*)())Java_xeno_util_Toolkit_loadResource__Ljava_lang_Object_I },
    { "Java_xeno_util_Toolkit_loadResource__Ljava_lang_String_", (void (*)())Java_xeno_util_Toolkit_loadResource__Ljava_lang_String_ },
    { "Java_xeno_util_Toolkit_loadResource__Ljava_lang_Object_Ljava_lang_Object_I", (void (*)())Java_xeno_util_Toolkit_loadResource__Ljava_lang_Object_Ljava_lang_Object_I },
    { "Java_xeno_util_Toolkit_loadResource__Ljava_lang_Object_II", (void (*)())Java_xeno_util_Toolkit_loadResource__Ljava_lang_Object_II },
    { "Java_xeno_util_Toolkit_peerSetGroup__II", (void (*)())Java_xeno_util_Toolkit_peerSetGroup__II },
    { "Java_xeno_Sound_sequencePlay__I", (void (*)())Java_xeno_Sound_sequencePlay__I },
    { "Java_xeno_Sound_sequencePlay__II", (void (*)())Java_xeno_Sound_sequencePlay__II },
    { "Java_xeno_Sound_sequenceStop__I", (void (*)())Java_xeno_Sound_sequenceStop__I },
    { "Java_xeno_Sound_sequenceStop__II", (void (*)())Java_xeno_Sound_sequenceStop__II },
    { "Java_xeno_Sound_effectPlay__III", (void (*)())Java_xeno_Sound_effectPlay__III },
    { "Java_xeno_Sound_effectStop__I", (void (*)())Java_xeno_Sound_effectStop__I },
    { "Java_xeno_Sound_streamPlay__IIII", (void (*)())Java_xeno_Sound_streamPlay__IIII },
    { "Java_xeno_Movie_init__I", (void (*)())Java_xeno_Movie_init__I },
    { "Java_xeno_Movie_start__I", (void (*)())Java_xeno_Movie_start__I },
    { "Java_xeno_Movie_stop__", (void (*)())Java_xeno_Movie_stop__ },
    { "Java_xeno_Movie_update__", (void (*)())Java_xeno_Movie_update__ },
    { "Java_xeno_PlayControl_create__", (void (*)())Java_xeno_PlayControl_create__ },
    { "Java_xeno_PlayControl_getSignal__", (void (*)())Java_xeno_PlayControl_getSignal__ },
    { "Java_xeno_PlayControl_signal__I", (void (*)())Java_xeno_PlayControl_signal__I },
    { "Java_xeno_PlayControl_init__II", (void (*)())Java_xeno_PlayControl_init__II },
    { "Java_xeno_PlayControl_init__IIIIF", (void (*)())Java_xeno_PlayControl_init__IIIIF },
    { "Java_xeno_PlayControl_loadCamera__Ljava_lang_Object_", (void (*)())Java_xeno_PlayControl_loadCamera__Ljava_lang_Object_ },
    { "Java_xeno_PlayControl_loadTimeChart__Ljava_lang_Object_", (void (*)())Java_xeno_PlayControl_loadTimeChart__Ljava_lang_Object_ },
    { "Java_xeno_PlayControl_start__", (void (*)())Java_xeno_PlayControl_start__ },
    { "Java_xeno_PlayControl_stop__", (void (*)())Java_xeno_PlayControl_stop__ },
    { "Java_xeno_PlayControl_setObserver__IILjava_lang_Object_Ljava_lang_String_", (void (*)())Java_xeno_PlayControl_setObserver__IILjava_lang_Object_Ljava_lang_String_ },
    { "Java_xeno_PlayControl_setObserver__ILjava_lang_String_Ljava_lang_Object_Ljava_lang_String_", (void (*)())Java_xeno_PlayControl_setObserver__ILjava_lang_String_Ljava_lang_Object_Ljava_lang_String_ },
    { "Java_xeno_PlayControl_getParams__", (void (*)())Java_xeno_PlayControl_getParams__ },
    { "Java_xeno_Effect_call__I", (void (*)())Java_xeno_Effect_call__I },
    { "Java_xeno_Effect_disp__Z", (void (*)())Java_xeno_Effect_disp__Z },
    { "Java_xeno_Effect_setScale__FFF", (void (*)())Java_xeno_Effect_setScale__FFF },
    { "Java_xeno_Effect_getScale__", (void (*)())Java_xeno_Effect_getScale__ },
    { "Java_xeno_Effect_getTranslate__", (void (*)())Java_xeno_Effect_getTranslate__ },
    { "Java_xeno_Effect_setTranslate__", (void (*)())Java_xeno_Effect_setTranslate__ },
    { "Java_xeno_Effect_getRotate__", (void (*)())Java_xeno_Effect_getRotate__ },
    { "Java_xeno_Effect_setRotate__", (void (*)())Java_xeno_Effect_setRotate__ },
    { "Java_xeno_Effect_setCaster__Lxeno_Chr_", (void (*)())Java_xeno_Effect_setCaster__Lxeno_Chr_ },
    { "Java_xeno_Effect_setTarget__Lxeno_Chr_", (void (*)())Java_xeno_Effect_setTarget__Lxeno_Chr_ },
    { "Java_xeno_Effect_setCaster__Lxeno_Unit_", (void (*)())Java_xeno_Effect_setCaster__Lxeno_Unit_ },
    { "Java_xeno_Effect_setTarget__Lxeno_Unit_", (void (*)())Java_xeno_Effect_setTarget__Lxeno_Unit_ },
    { "Java_xeno_Effect_setTransOffset__FFF", (void (*)())Java_xeno_Effect_setTransOffset__FFF },
    { "Java_xeno_Effect_getForceLoop__", (void (*)())Java_xeno_Effect_getForceLoop__ },
    { "Java_xeno_Effect_setForceLoop__Z", (void (*)())Java_xeno_Effect_setForceLoop__Z },
    { "Java_xeno_Effect_getClip__", (void (*)())Java_xeno_Effect_getClip__ },
    { "Java_xeno_Effect_setClip__Z", (void (*)())Java_xeno_Effect_setClip__Z },
    { "Java_xeno_Effect_clearEffect__", (void (*)())Java_xeno_Effect_clearEffect__ },
    { "Java_xeno_Effect_setMotion__Z", (void (*)())Java_xeno_Effect_setMotion__Z },
    { "Java_xeno_Effect_noAttach__Z", (void (*)())Java_xeno_Effect_noAttach__Z },
    { "Java_xeno_Camera_getRotateX__", (void (*)())Java_xeno_Camera_getRotateX__ },
    { "Java_xeno_Camera_getRotateY__", (void (*)())Java_xeno_Camera_getRotateY__ },
    { "Java_xeno_Camera_getRotateZ__", (void (*)())Java_xeno_Camera_getRotateZ__ },
    { "Java_xeno_Camera_getTranslateX__", (void (*)())Java_xeno_Camera_getTranslateX__ },
    { "Java_xeno_Camera_getTranslateY__", (void (*)())Java_xeno_Camera_getTranslateY__ },
    { "Java_xeno_Camera_getTranslateZ__", (void (*)())Java_xeno_Camera_getTranslateZ__ },
    { "Java_xeno_Camera_fovSPL__aFI", (void (*)())Java_xeno_Camera_fovSPL__aFI },
    { "Java_xeno_Camera_change__", (void (*)())Java_xeno_Camera_change__ },
    { "Java_xeno_Camera_rotateSPL__aFI", (void (*)())Java_xeno_Camera_rotateSPL__aFI },
    { "Java_xeno_Camera_rotateSPL__aFIII", (void (*)())Java_xeno_Camera_rotateSPL__aFIII },
    { "Java_xeno_Camera_setActive__Z", (void (*)())Java_xeno_Camera_setActive__Z },
    { "Java_xeno_Camera_create__I", (void (*)())Java_xeno_Camera_create__I },
    { "Java_xeno_Camera_transCNS__Ljava_lang_Object_FFF", (void (*)())Java_xeno_Camera_transCNS__Ljava_lang_Object_FFF },
    { "Java_xeno_Camera_transSPL__aFI", (void (*)())Java_xeno_Camera_transSPL__aFI },
    { "Java_xeno_Camera_transSPL__aFIII", (void (*)())Java_xeno_Camera_transSPL__aFIII },
    { "Java_xeno_Camera_viewCNS__Ljava_lang_Object_FFF", (void (*)())Java_xeno_Camera_viewCNS__Ljava_lang_Object_FFF },
    { "Java_xeno_Camera_viewSPL__aFI", (void (*)())Java_xeno_Camera_viewSPL__aFI },
    { "Java_xeno_Camera_viewSPL__aFIII", (void (*)())Java_xeno_Camera_viewSPL__aFIII },
    { "Java_xeno_Camera_rollSPL__aFI", (void (*)())Java_xeno_Camera_rollSPL__aFI },
    { "Java_xeno_Camera_setRotate__FFF", (void (*)())Java_xeno_Camera_setRotate__FFF },
    { "Java_xeno_Camera_setRoll__F", (void (*)())Java_xeno_Camera_setRoll__F },
    { "Java_xeno_Camera_setTranslate__FFF", (void (*)())Java_xeno_Camera_setTranslate__FFF },
    { "Java_xeno_Camera_setFov__F", (void (*)())Java_xeno_Camera_setFov__F },
    { "Java_xeno_Camera_getFov__", (void (*)())Java_xeno_Camera_getFov__ },
    { "Java_xeno_Camera_setView__FFF", (void (*)())Java_xeno_Camera_setView__FFF },
    { "Java_xeno_Camera_start__ILjava_lang_Object_", (void (*)())Java_xeno_Camera_start__ILjava_lang_Object_ },
    { "Java_xeno_Camera_setCFAngle__IFFFF", (void (*)())Java_xeno_Camera_setCFAngle__IFFFF },
    { "Java_xeno_Camera_setCFAnglePers__IFFFFF", (void (*)())Java_xeno_Camera_setCFAnglePers__IFFFFF },
    { "Java_xeno_Camera_setCFHokan__IFF", (void (*)())Java_xeno_Camera_setCFHokan__IFF },
    { "Java_xeno_Camera_setCFLock__IIFFF", (void (*)())Java_xeno_Camera_setCFLock__IIFFF },
    { "Java_xeno_Camera_setCFOffset__IFFFIFIF", (void (*)())Java_xeno_Camera_setCFOffset__IFFFIFIF },
    { "Java_xeno_Camera_setMode__I", (void (*)())Java_xeno_Camera_setMode__I },
    { "Java_xeno_Camera_getMode__", (void (*)())Java_xeno_Camera_getMode__ },
    { "Java_xeno_Camera_setCFPedestal__IFFFFFFFF", (void (*)())Java_xeno_Camera_setCFPedestal__IFFFFFFFF },
    { "Java_xeno_Camera_setCFPedestalHokan__II", (void (*)())Java_xeno_Camera_setCFPedestalHokan__II },
    { "Java_xeno_Camera_changeID__III", (void (*)())Java_xeno_Camera_changeID__III },
    { "Java_xeno_Camera_setFog__IFFFFIIII", (void (*)())Java_xeno_Camera_setFog__IFFFFIIII },
    { "Java_xeno_Camera_resetFog__I", (void (*)())Java_xeno_Camera_resetFog__I },
    { "Java_xeno_Camera_setClipRange__FF", (void (*)())Java_xeno_Camera_setClipRange__FF },
    { "Java_xeno_Light_setColor__FFF", (void (*)())Java_xeno_Light_setColor__FFF },
    { "Java_xeno_Light_setDirection__FFF", (void (*)())Java_xeno_Light_setDirection__FFF },
    { "Java_xeno_Light_setDirection2__FFF", (void (*)())Java_xeno_Light_setDirection2__FFF },
    { "Java_xeno_Light_setGlobalPointLightCol__IFFF", (void (*)())Java_xeno_Light_setGlobalPointLightCol__IFFF },
    { "Java_xeno_Light_setGlobalPointLightPos__IFFF", (void (*)())Java_xeno_Light_setGlobalPointLightPos__IFFF },
    { "Java_xeno_Light_setGlobalPointLightReset__", (void (*)())Java_xeno_Light_setGlobalPointLightReset__ },
    { "Java_xeno_Chr_getPlayer__", (void (*)())Java_xeno_Chr_getPlayer__ },
    { "Java_xeno_Chr_setPlayer__", (void (*)())Java_xeno_Chr_setPlayer__ },
    { "Java_xeno_Chr_move__IFFZ", (void (*)())Java_xeno_Chr_move__IFFZ },
    { "Java_xeno_Chr_move__FFFZ", (void (*)())Java_xeno_Chr_move__FFFZ },
    { "Java_xeno_Chr_move__ILjava_lang_Object_Z", (void (*)())Java_xeno_Chr_move__ILjava_lang_Object_Z },
    { "Java_xeno_Chr_move__Ljava_lang_Object_FZ", (void (*)())Java_xeno_Chr_move__Ljava_lang_Object_FZ },
    { "Java_xeno_Chr_move__Lxeno_util_Spline_IZ", (void (*)())Java_xeno_Chr_move__Lxeno_util_Spline_IZ },
    { "Java_xeno_Chr_rotate__Lxeno_util_Spline_IZ", (void (*)())Java_xeno_Chr_rotate__Lxeno_util_Spline_IZ },
    { "Java_xeno_Chr_rotX__FFZ", (void (*)())Java_xeno_Chr_rotX__FFZ },
    { "Java_xeno_Chr_rotX__IFZ", (void (*)())Java_xeno_Chr_rotX__IFZ },
    { "Java_xeno_Chr_rotX__ILjava_lang_Object_Z", (void (*)())Java_xeno_Chr_rotX__ILjava_lang_Object_Z },
    { "Java_xeno_Chr_rotX__Ljava_lang_Object_FZ", (void (*)())Java_xeno_Chr_rotX__Ljava_lang_Object_FZ },
    { "Java_xeno_Chr_rotY__FFZ", (void (*)())Java_xeno_Chr_rotY__FFZ },
    { "Java_xeno_Chr_rotY__IFZ", (void (*)())Java_xeno_Chr_rotY__IFZ },
    { "Java_xeno_Chr_rotY__ILjava_lang_Object_Z", (void (*)())Java_xeno_Chr_rotY__ILjava_lang_Object_Z },
    { "Java_xeno_Chr_rotY__Ljava_lang_Object_FZ", (void (*)())Java_xeno_Chr_rotY__Ljava_lang_Object_FZ },
    { "Java_xeno_Chr_rotZ__FFZ", (void (*)())Java_xeno_Chr_rotZ__FFZ },
    { "Java_xeno_Chr_rotZ__IFZ", (void (*)())Java_xeno_Chr_rotZ__IFZ },
    { "Java_xeno_Chr_rotZ__ILjava_lang_Object_Z", (void (*)())Java_xeno_Chr_rotZ__ILjava_lang_Object_Z },
    { "Java_xeno_Chr_rotZ__Ljava_lang_Object_FZ", (void (*)())Java_xeno_Chr_rotZ__Ljava_lang_Object_FZ },
    { "Java_xeno_Chr_start__ILjava_lang_Object_", (void (*)())Java_xeno_Chr_start__ILjava_lang_Object_ },
    { "Java_xeno_Chr_stop__", (void (*)())Java_xeno_Chr_stop__ },
    { "Java_xeno_Chr_mtn__IIIIIFZ", (void (*)())Java_xeno_Chr_mtn__IIIIIFZ },
    { "Java_xeno_Chr_mtn__IIFZ", (void (*)())Java_xeno_Chr_mtn__IIFZ },
    { "Java_xeno_Chr_signal__I", (void (*)())Java_xeno_Chr_signal__I },
    { "Java_xeno_Chr_getSignal__", (void (*)())Java_xeno_Chr_getSignal__ },
    { "Java_xeno_Chr_setRotate__", (void (*)())Java_xeno_Chr_setRotate__ },
    { "Java_xeno_Chr_setTranslate__", (void (*)())Java_xeno_Chr_setTranslate__ },
    { "Java_xeno_Chr_getRotate__", (void (*)())Java_xeno_Chr_getRotate__ },
    { "Java_xeno_Chr_getTranslate__", (void (*)())Java_xeno_Chr_getTranslate__ },
    { "Java_xeno_Chr_setVisible__Z", (void (*)())Java_xeno_Chr_setVisible__Z },
    { "Java_xeno_Chr_setVisible__IZ", (void (*)())Java_xeno_Chr_setVisible__IZ },
    { "Java_xeno_Chr_setCollision__Z", (void (*)())Java_xeno_Chr_setCollision__Z },
    { "Java_xeno_Chr_setHand__I", (void (*)())Java_xeno_Chr_setHand__I },
    { "Java_xeno_Chr_setArgs__III", (void (*)())Java_xeno_Chr_setArgs__III },
    { "Java_xeno_Chr_getArgs__II", (void (*)())Java_xeno_Chr_getArgs__II },
    { "Java_xeno_Chr_setArgs__ILjava_lang_Object_I", (void (*)())Java_xeno_Chr_setArgs__ILjava_lang_Object_I },
    { "Java_xeno_Chr_getSerial__", (void (*)())Java_xeno_Chr_getSerial__ },
    { "Java_xeno_Chr_getState__", (void (*)())Java_xeno_Chr_getState__ },
    { "Java_xeno_Chr_mtnGetRoot__ILxeno_util_Vector4f_", (void (*)())Java_xeno_Chr_mtnGetRoot__ILxeno_util_Vector4f_ },
    { "Java_xeno_Chr_setScale__FFF", (void (*)())Java_xeno_Chr_setScale__FFF },
    { "Java_xeno_Chr_getScale__", (void (*)())Java_xeno_Chr_getScale__ },
    { "Java_xeno_Chr_scale__Lxeno_util_Spline_IZ", (void (*)())Java_xeno_Chr_scale__Lxeno_util_Spline_IZ },
    { "Java_xeno_Chr_sclX__FFZ", (void (*)())Java_xeno_Chr_sclX__FFZ },
    { "Java_xeno_Chr_sclX__IFZ", (void (*)())Java_xeno_Chr_sclX__IFZ },
    { "Java_xeno_Chr_sclY__FFZ", (void (*)())Java_xeno_Chr_sclY__FFZ },
    { "Java_xeno_Chr_sclY__IFZ", (void (*)())Java_xeno_Chr_sclY__IFZ },
    { "Java_xeno_Chr_sclZ__FFZ", (void (*)())Java_xeno_Chr_sclZ__FFZ },
    { "Java_xeno_Chr_sclZ__IFZ", (void (*)())Java_xeno_Chr_sclZ__IFZ },
    { "Java_xeno_Chr_rotCNS__ILjava_lang_Object_", (void (*)())Java_xeno_Chr_rotCNS__ILjava_lang_Object_ },
    { "Java_xeno_Chr_rotCNS__IFFF", (void (*)())Java_xeno_Chr_rotCNS__IFFF },
    { "Java_xeno_Chr_setRotCNSParam__IFFFFF", (void (*)())Java_xeno_Chr_setRotCNSParam__IFFFFF },
    { "Java_xeno_Chr_relax__II", (void (*)())Java_xeno_Chr_relax__II },
    { "Java_xeno_Chr_getFlags__", (void (*)())Java_xeno_Chr_getFlags__ },
    { "Java_xeno_Chr_setFlags__I", (void (*)())Java_xeno_Chr_setFlags__I },
    { "Java_xeno_Chr_setEdgeFall__I", (void (*)())Java_xeno_Chr_setEdgeFall__I },
    { "Java_xeno_Chr_setShadow__II", (void (*)())Java_xeno_Chr_setShadow__II },
    { "Java_xeno_Chr_setShadow__aB", (void (*)())Java_xeno_Chr_setShadow__aB },
    { "Java_xeno_Chr_setID__I", (void (*)())Java_xeno_Chr_setID__I },
    { "Java_xeno_Chr_setElevatorMode__I", (void (*)())Java_xeno_Chr_setElevatorMode__I },
    { "Java_xeno_Chr_setParent__Lxeno_Chr_III", (void (*)())Java_xeno_Chr_setParent__Lxeno_Chr_III },
    { "Java_xeno_Chr_setMotionFlags__IZ", (void (*)())Java_xeno_Chr_setMotionFlags__IZ },
    { "Java_xeno_Chr_setFilter__I", (void (*)())Java_xeno_Chr_setFilter__I },
    { "Java_xeno_Chr_setFilterParam__aF", (void (*)())Java_xeno_Chr_setFilterParam__aF },
    { "Java_xeno_Chr_setClip__I", (void (*)())Java_xeno_Chr_setClip__I },
    { "Java_xeno_Chr_setSymmetryY__I", (void (*)())Java_xeno_Chr_setSymmetryY__I },
    { "Java_xeno_Chr_setSortOffset__F", (void (*)())Java_xeno_Chr_setSortOffset__F },
    { "Java_xeno_Chr_setPointLightCol__IFFF", (void (*)())Java_xeno_Chr_setPointLightCol__IFFF },
    { "Java_xeno_Chr_setPointLightPos__IFFF", (void (*)())Java_xeno_Chr_setPointLightPos__IFFF },
    { "Java_xeno_Chr_setPointLightReset__", (void (*)())Java_xeno_Chr_setPointLightReset__ },
    { "Java_xeno_Chr_talkto__Ljava_lang_String_", (void (*)())Java_xeno_Chr_talkto__Ljava_lang_String_ },
    { "Java_xeno_Chr_touchto__Ljava_lang_String_", (void (*)())Java_xeno_Chr_touchto__Ljava_lang_String_ },
    { "Java_xeno_Chr_childGetPeer__II", (void (*)())Java_xeno_Chr_childGetPeer__II },
    { "Java_xeno_Chr_setPeer__Ljava_lang_Object_", (void (*)())Java_xeno_Chr_setPeer__Ljava_lang_Object_ },
    { "Java_xeno_Chr_dispRadar__Z", (void (*)())Java_xeno_Chr_dispRadar__Z },
    { "Java_xeno_Chr_look_camera__", (void (*)())Java_xeno_Chr_look_camera__ },
    { "Java_xeno_Chr_look_char__Ljava_lang_Object_", (void (*)())Java_xeno_Chr_look_char__Ljava_lang_Object_ },
    { "Java_xeno_Chr_look_unit__Ljava_lang_Object_", (void (*)())Java_xeno_Chr_look_unit__Ljava_lang_Object_ },
    { "Java_xeno_Chr_look_default__", (void (*)())Java_xeno_Chr_look_default__ },
    { "Java_xeno_Chr_look_point__FFF", (void (*)())Java_xeno_Chr_look_point__FFF },
    { "Java_xeno_Chr_look_eye_set__FF", (void (*)())Java_xeno_Chr_look_eye_set__FF },
    { "Java_xeno_Chr_look_eye_control__I", (void (*)())Java_xeno_Chr_look_eye_control__I },
    { "Java_xeno_Chr_look_speed__F", (void (*)())Java_xeno_Chr_look_speed__F },
    { "Java_xeno_Chr_look_eye_speed__F", (void (*)())Java_xeno_Chr_look_eye_speed__F },
    { "Java_xeno_Chr_renderCommand__I", (void (*)())Java_xeno_Chr_renderCommand__I },
    { "Java_xeno_Chr_shadow_clip_scale__F", (void (*)())Java_xeno_Chr_shadow_clip_scale__F },
    { "Java_xeno_Chr_shadow_map_id__I", (void (*)())Java_xeno_Chr_shadow_map_id__I },
    { "Java_xeno_Chr_shadow_map_reset__", (void (*)())Java_xeno_Chr_shadow_map_reset__ },
    { "Java_xeno_Chr_hairStop__II", (void (*)())Java_xeno_Chr_hairStop__II },
    { "Java_xeno_Chr_pixelAlpha__I", (void (*)())Java_xeno_Chr_pixelAlpha__I },
    { "Java_xeno_Chr_pixelAlphaParts__II", (void (*)())Java_xeno_Chr_pixelAlphaParts__II },
    { "Java_xeno_Chr_pixelAlphaPartsReset__", (void (*)())Java_xeno_Chr_pixelAlphaPartsReset__ },
    { "Java_xeno_Chr_setMotNoUpdate__I", (void (*)())Java_xeno_Chr_setMotNoUpdate__I },
    { "Java_xeno_Chr_setWeaponR__Lxeno_Chr_", (void (*)())Java_xeno_Chr_setWeaponR__Lxeno_Chr_ },
    { "Java_xeno_Chr_resetWeaponR__Lxeno_Chr_", (void (*)())Java_xeno_Chr_resetWeaponR__Lxeno_Chr_ },
    { "Java_xeno_Chr_resetHand__", (void (*)())Java_xeno_Chr_resetHand__ },
    { "Java_xeno_Chr_resetEnv__", (void (*)())Java_xeno_Chr_resetEnv__ },
    { "Java_xeno_Chr_ignoreShape__I", (void (*)())Java_xeno_Chr_ignoreShape__I },
    { "Java_xeno_Unit_rotYCNS__Ljava_lang_Object_IFFF", (void (*)())Java_xeno_Unit_rotYCNS__Ljava_lang_Object_IFFF },
    { "Java_xeno_Unit_transCNS__Ljava_lang_Object_IFFF", (void (*)())Java_xeno_Unit_transCNS__Ljava_lang_Object_IFFF },
    { "Java_xeno_Unit_getRotate__", (void (*)())Java_xeno_Unit_getRotate__ },
    { "Java_xeno_Unit_getSignal__", (void (*)())Java_xeno_Unit_getSignal__ },
    { "Java_xeno_Unit_getTranslate__", (void (*)())Java_xeno_Unit_getTranslate__ },
    { "Java_xeno_Unit_invalidate__", (void (*)())Java_xeno_Unit_invalidate__ },
    { "Java_xeno_Unit_move__FFFZ", (void (*)())Java_xeno_Unit_move__FFFZ },
    { "Java_xeno_Unit_move__IFFZ", (void (*)())Java_xeno_Unit_move__IFFZ },
    { "Java_xeno_Unit_move__Lxeno_util_Spline_IZ", (void (*)())Java_xeno_Unit_move__Lxeno_util_Spline_IZ },
    { "Java_xeno_Unit_rotate__Lxeno_util_Spline_IZ", (void (*)())Java_xeno_Unit_rotate__Lxeno_util_Spline_IZ },
    { "Java_xeno_Unit_move__ILjava_lang_Object_Z", (void (*)())Java_xeno_Unit_move__ILjava_lang_Object_Z },
    { "Java_xeno_Unit_move__Ljava_lang_Object_FZ", (void (*)())Java_xeno_Unit_move__Ljava_lang_Object_FZ },
    { "Java_xeno_Unit_mtn__IIFZ", (void (*)())Java_xeno_Unit_mtn__IIFZ },
    { "Java_xeno_Unit_mtn__IIIIIFZ", (void (*)())Java_xeno_Unit_mtn__IIIIIFZ },
    { "Java_xeno_Unit_rotX__FFZ", (void (*)())Java_xeno_Unit_rotX__FFZ },
    { "Java_xeno_Unit_rotX__IFZ", (void (*)())Java_xeno_Unit_rotX__IFZ },
    { "Java_xeno_Unit_rotX__ILjava_lang_Object_Z", (void (*)())Java_xeno_Unit_rotX__ILjava_lang_Object_Z },
    { "Java_xeno_Unit_rotX__Ljava_lang_Object_FZ", (void (*)())Java_xeno_Unit_rotX__Ljava_lang_Object_FZ },
    { "Java_xeno_Unit_rotY__FFZ", (void (*)())Java_xeno_Unit_rotY__FFZ },
    { "Java_xeno_Unit_rotY__IFZ", (void (*)())Java_xeno_Unit_rotY__IFZ },
    { "Java_xeno_Unit_rotY__ILjava_lang_Object_Z", (void (*)())Java_xeno_Unit_rotY__ILjava_lang_Object_Z },
    { "Java_xeno_Unit_rotY__Ljava_lang_Object_FZ", (void (*)())Java_xeno_Unit_rotY__Ljava_lang_Object_FZ },
    { "Java_xeno_Unit_rotZ__FFZ", (void (*)())Java_xeno_Unit_rotZ__FFZ },
    { "Java_xeno_Unit_rotZ__IFZ", (void (*)())Java_xeno_Unit_rotZ__IFZ },
    { "Java_xeno_Unit_rotZ__ILjava_lang_Object_Z", (void (*)())Java_xeno_Unit_rotZ__ILjava_lang_Object_Z },
    { "Java_xeno_Unit_rotZ__Ljava_lang_Object_FZ", (void (*)())Java_xeno_Unit_rotZ__Ljava_lang_Object_FZ },
    { "Java_xeno_Unit_scale__Lxeno_util_Spline_IZ", (void (*)())Java_xeno_Unit_scale__Lxeno_util_Spline_IZ },
    { "Java_xeno_Unit_sclX__FFZ", (void (*)())Java_xeno_Unit_sclX__FFZ },
    { "Java_xeno_Unit_sclX__IFZ", (void (*)())Java_xeno_Unit_sclX__IFZ },
    { "Java_xeno_Unit_sclY__FFZ", (void (*)())Java_xeno_Unit_sclY__FFZ },
    { "Java_xeno_Unit_sclY__IFZ", (void (*)())Java_xeno_Unit_sclY__IFZ },
    { "Java_xeno_Unit_sclZ__FFZ", (void (*)())Java_xeno_Unit_sclZ__FFZ },
    { "Java_xeno_Unit_sclZ__IFZ", (void (*)())Java_xeno_Unit_sclZ__IFZ },
    { "Java_xeno_Unit_setCollision__Z", (void (*)())Java_xeno_Unit_setCollision__Z },
    { "Java_xeno_Unit_setRotate__", (void (*)())Java_xeno_Unit_setRotate__ },
    { "Java_xeno_Unit_setTranslate__", (void (*)())Java_xeno_Unit_setTranslate__ },
    { "Java_xeno_Unit_setVisible__IZ", (void (*)())Java_xeno_Unit_setVisible__IZ },
    { "Java_xeno_Unit_setVisible__Z", (void (*)())Java_xeno_Unit_setVisible__Z },
    { "Java_xeno_Unit_signal__I", (void (*)())Java_xeno_Unit_signal__I },
    { "Java_xeno_Unit_start__ILjava_lang_Object_", (void (*)())Java_xeno_Unit_start__ILjava_lang_Object_ },
    { "Java_xeno_Unit_stop__", (void (*)())Java_xeno_Unit_stop__ },
    { "Java_xeno_Unit_validate__", (void (*)())Java_xeno_Unit_validate__ },
    { "Java_xeno_Unit_setParent__Ljava_lang_Object_I", (void (*)())Java_xeno_Unit_setParent__Ljava_lang_Object_I },
    { "Java_xeno_Unit_setArgs__III", (void (*)())Java_xeno_Unit_setArgs__III },
    { "Java_xeno_Unit_getArgs__II", (void (*)())Java_xeno_Unit_getArgs__II },
    { "Java_xeno_Unit_setArgs__ILjava_lang_Object_I", (void (*)())Java_xeno_Unit_setArgs__ILjava_lang_Object_I },
    { "Java_xeno_Unit_setArgs__IIIII", (void (*)())Java_xeno_Unit_setArgs__IIIII },
    { "Java_xeno_Unit_getScale__", (void (*)())Java_xeno_Unit_getScale__ },
    { "Java_xeno_Unit_setScale__FFF", (void (*)())Java_xeno_Unit_setScale__FFF },
    { "Java_xeno_Unit_getSerial__", (void (*)())Java_xeno_Unit_getSerial__ },
    { "Java_xeno_Unit_mtnSetMask__I", (void (*)())Java_xeno_Unit_mtnSetMask__I },
    { "Java_xeno_Unit_mtnGetRoot__ILxeno_util_Vector4f_", (void (*)())Java_xeno_Unit_mtnGetRoot__ILxeno_util_Vector4f_ },
    { "Java_xeno_Unit_getState__", (void (*)())Java_xeno_Unit_getState__ },
    { "Java_xeno_Unit_getPivot__Lxeno_util_Vector4f_", (void (*)())Java_xeno_Unit_getPivot__Lxeno_util_Vector4f_ },
    { "Java_xeno_Unit_setPivot__FFF", (void (*)())Java_xeno_Unit_setPivot__FFF },
    { "Java_xeno_Unit_getAxis__Lxeno_util_Vector4f_", (void (*)())Java_xeno_Unit_getAxis__Lxeno_util_Vector4f_ },
    { "Java_xeno_Unit_setAxis__FFFF", (void (*)())Java_xeno_Unit_setAxis__FFFF },
    { "Java_xeno_Unit_suspend__I", (void (*)())Java_xeno_Unit_suspend__I },
    { "Java_xeno_Unit_resume__I", (void (*)())Java_xeno_Unit_resume__I },
    { "Java_xeno_Unit_initElevatorFunc__", (void (*)())Java_xeno_Unit_initElevatorFunc__ },
    { "Java_xeno_Unit_map_shadow__I", (void (*)())Java_xeno_Unit_map_shadow__I },
    { "Java_xeno_Unit_renderCommand__I", (void (*)())Java_xeno_Unit_renderCommand__I },
    { "Java_xeno_Unit_setFilter__I", (void (*)())Java_xeno_Unit_setFilter__I },
    { "Java_xeno_Unit_setFilterParam__aF", (void (*)())Java_xeno_Unit_setFilterParam__aF },
    { "Java_xeno_Unit_setShadow__II", (void (*)())Java_xeno_Unit_setShadow__II },
    { "Java_xeno_Unit_shadow_clip_scale__F", (void (*)())Java_xeno_Unit_shadow_clip_scale__F },
    { "Java_xeno_Unit_shadow_map_id__I", (void (*)())Java_xeno_Unit_shadow_map_id__I },
    { "Java_xeno_Unit_shadow_map_reset__", (void (*)())Java_xeno_Unit_shadow_map_reset__ },
    { "Java_xeno_Unit_setSortOffset__F", (void (*)())Java_xeno_Unit_setSortOffset__F },
    { "Java_xeno_Unit_setClip__I", (void (*)())Java_xeno_Unit_setClip__I },
    { "Java_xeno_Unit_setMonitorPrio__I", (void (*)())Java_xeno_Unit_setMonitorPrio__I },
    { "Java_xeno_Scene_start__ILjava_lang_Object_", (void (*)())Java_xeno_Scene_start__ILjava_lang_Object_ },
    { "Java_xeno_Scene_stop__", (void (*)())Java_xeno_Scene_stop__ },
    { "Java_xeno_Stage_start__ILjava_lang_Object_", (void (*)())Java_xeno_Stage_start__ILjava_lang_Object_ },
    { "Java_xeno_Stage_stop__", (void (*)())Java_xeno_Stage_stop__ },
    { "Java_xeno_Stage_play__Ljava_lang_String_", (void (*)())Java_xeno_Stage_play__Ljava_lang_String_ },
    { "Java_xeno_Stage_setPartsLast__I", (void (*)())Java_xeno_Stage_setPartsLast__I },
    { "Java_xeno_Stage_setPartsLastReset__", (void (*)())Java_xeno_Stage_setPartsLastReset__ },
    { "Java_xeno_Stage_setColor__FFF", (void (*)())Java_xeno_Stage_setColor__FFF },
    { "Java_xeno_Stage_setFade__IIFFF", (void (*)())Java_xeno_Stage_setFade__IIFFF },
    { "Java_xeno_Stage_setEventFade__IFFFIFFF", (void (*)())Java_xeno_Stage_setEventFade__IFFFIFFF },
    { "Java_xeno_Stage_setFrameRender__II", (void (*)())Java_xeno_Stage_setFrameRender__II },
    { "Java_xeno_Stage_setFadeCancel__I", (void (*)())Java_xeno_Stage_setFadeCancel__I },
    { "Java_xeno_Stage_setVisible__IZ", (void (*)())Java_xeno_Stage_setVisible__IZ },
    { "Java_xeno_Stage_setCFBG__II_F", (void (*)())Java_xeno_Stage_setCFBG__II_F },
    { "Java_xeno_Stage_setEffectRender__I", (void (*)())Java_xeno_Stage_setEffectRender__I },
    { "Java_xeno_Stage_renderCommand__I", (void (*)())Java_xeno_Stage_renderCommand__I },
    { "Java_xeno_Stage_setBgColor__FFF", (void (*)())Java_xeno_Stage_setBgColor__FFF },
    { "Java_xeno_Stage_setBgClip__I", (void (*)())Java_xeno_Stage_setBgClip__I },
    { "Java_xeno_Stage_clrBackBuffer__", (void (*)())Java_xeno_Stage_clrBackBuffer__ },
    { 0, 0 },
};

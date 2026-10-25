#include "common.h"
#include "shared.h"
#include "script.h"

extern XglTaskPrefix *xglTaskEntryNext(XglTaskScheduler *scheduler,
                                       int (*callback)(XglTaskPrefix *task),
                                       XglTaskPrefix *entry);
extern int xglTaskRemove(XglTaskPrefix *task);

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern SceneString *loadConstString(const char *bytes, int length);
extern SceneMethod *findMethod(SceneClass *scene_class, SceneString *name,
                               void *signature_or_type);
extern void JNI_callMethod(SceneVm *vm, SceneMethod *method,
                           SceneObject *arguments, int *output);
extern int JNI_isInstanceOf(SceneObject object, SceneClass *target_class);
extern void JNI_initThread(SceneVm *vm);
extern void JNI_catchException(void);
extern int getScriptFlag(SceneObject object);
extern void XTK_setWindowOwner(int owner);
extern void talkCancel(ScriptObserverTask *task);
extern void actTalkAfter(SceneObject object);
extern void initVM(void);
extern SceneThread *defaultVM;
extern SceneThread *JNI_createThread(int kind, int stack_words,
                                     int frame_words);
extern void JNI_pushFrame(void);
extern void JNI_loadNativeClass(void);
extern SceneThread *stageVM;
extern SceneThread *evtVM[2];
extern int UseVMFlag;
extern int currentScriptDB;
extern SceneClass *classJava_xeno_Stage;
extern SceneClass *classJava_xeno_Chr;
extern void createTalkTask(void *actor, const char *method_name);
extern const char call_method_signature_void[4];
extern const char call_method_signature_int[5];
extern const char call_method_signature_int_int[6];
/* These TU-local signature literals remain assembler-owned data. Their ELF
 * local-symbol names are required by the CallMethod-family relocations. */
extern const char sig_2[4];
extern const char sig_3[5];
extern const char sig_4[6];
#define call_method_signature_void sig_2
#define call_method_signature_int sig_3
#define call_method_signature_int_int sig_4
extern const char func_observer_debug_text[16];
extern void funcObserver(ScriptObserverTask *task);
int getEmptyVM(ScriptObserverTask *observer);
extern PadPrefix PadData;

extern unsigned int s_nScriptCfTime;
extern unsigned int s_nScriptFadeOutTime;
extern unsigned int s_nScriptFadeRequest;
extern unsigned char s_nScriptChangeTime;
extern unsigned int s_nScriptSequenceReset;
extern unsigned int s_nScriptEventFin;
extern unsigned int s_nScriptEventActive;
extern unsigned int s_nScriptTalkLock;
extern unsigned int s_nScriptFrameLockEntry;
extern int resourceID;
extern int windowOwner;

typedef void (*JSNativeMethod)(void);
extern void JS_init(int state, int class_capacity, int method_capacity);
extern int JS_loadClass(const char *class_name);
extern void JS_classSetup(int class_id, JSNativeMethod get_peer);
extern void JS_classAddMethod(int class_id, const char *name,
                              JSNativeMethod method);
extern const char D_004DA460[];
extern const char D_004C1D50[];
extern const char D_004C1D60[];
extern void JS_classLight_getPeer(void);
extern void JS_classLight_setColor(void);
extern void JS_classLight_setDirection2(void);

/* Only the halfword at GameLoopState+0xC is evidenced by
 * SCRIPT_sendMovieSkipSignal; the rest of the game-state record remains
 * scaffold-owned. */
typedef struct GameLoopMovieStatePrefix {
    u8 unmodeled_00[0xC];
    unsigned short movie_state;
} GameLoopMovieStatePrefix;

/* TU-local declarations for the main-00261860 observer/CallMethod allocation
 * (talktoObserver, CallMethod, CallMethod_I, CallMethod_II, funcObserver).
 * New-shared-name proposals are marked below; the integrator reconciles them
 * with config/header-canon.json on promotion.
 */

/* Proposal: the script table indexed by currentScriptDB. Only the two fields
 * this allocation touches are modeled; scriptDB's element stride (0x1C bytes,
 * evidenced by *7*4 in every CallMethod family function) is fixed by that
 * indexing arithmetic, not invented. */
typedef struct ScriptDbEntry {
    u8 unmodeled_00[0]; /* +0x0: zero-length alias of the former padding member, kept for saved bodies */
    void *class_image;   /* +0x0: class database image read from the script file */
    void *motion_data;   /* +0x4: extra-motion pack read next to the script file (.fpk) */
    void *pdb;            /* +0x8: event data used for resource lookup */
    int active;          /* +0xC: nonzero when this script slot is bound (zero test only) */
    SceneThread *thread; /* +0x10: the script's VM thread; its own object is at +0x10 */
    u8 unmodeled_14[0]; /* +0x14: zero-length alias of the former padding member, kept for saved bodies */
    int class_slot;      /* +0x14: class database slot the image is loaded into */
    int skip_count;      /* +0x18: class-name skip count JNI_searchClasses reported */
} ScriptDbEntry;
extern ScriptDbEntry scriptDB[];

/* The database pointer is at scriptDB + 8. This interior scaffold symbol
 * starts at that pointer and each entry retains the evidenced 0x1C stride. */
typedef struct ScriptDbPdbSlot {
    void *pdb;
    u8 unmodeled_04[0x1C - 4];
} ScriptDbPdbSlot;
extern ScriptDbPdbSlot D_004DEDE8[];

typedef struct ScriptPdbFile {
    u8 unmodeled_00[8];
    int data;
} ScriptPdbFile;
extern ScriptPdbFile *PDB_findFile(void *pdb, const char *path);

/*
 * TU-local partial view of the engine's actor record (the 0xa70-strided
 * `actor` array at main 0x0043c1e0; other units keep their own scoped views
 * of the same record, e.g. src/main/near_dir.h). SCRIPT_execTalkto and
 * SCRIPT_execTouchto are handed one entry and read only the three members
 * below; every other byte of the record is untouched by this allocation and
 * stays an unmodeled span.
 *
 *   +0x4c0 script_object      the same byte talktoObserver already reaches
 *                             through ACTOR_SCRIPT_OBJECT_OFFSET (this
 *                             allocation's own two call sites name it as a
 *                             member instead, so as not to add more
 *                             offset-cast findings than that one already
 *                             accepted use).
 *   +0x9f4 talk_method_name   the JNI method name of the actor's talk
 *                             script method, null when it has none. Both
 *                             entry points gate on it before queuing a talk
 *                             task, and SCRIPT_execTalkto passes it straight
 *                             through as the method to call. createTalkTask
 *                             (0x00261620, still assembler) passes its own
 *                             second argument to loadConstString(bytes,
 *                             length) (0x00261698), so the field is a name
 *                             string, not an integer flag.
 *   +0x9f8 touch_method_name  the method SCRIPT_execTouchto passes instead,
 *                             once the same +0x9f4 gate lets it through.
 */
typedef struct ScriptActorMethods {
    u8 unmodeled_00[0x4c0];
    SceneObject script_object;      /* +0x4c0 */
    u8 unmodeled_4c4[0x9f4 - 0x4c4];
    const char *talk_method_name;   /* +0x9f4 */
    const char *touch_method_name;  /* +0x9f8 */
} ScriptActorMethods;

/* Reused TU-local declaration (src/core/main-0025a9c0/private.h and every
 * other GameLoopState-family accepted source in this unit). */
extern GameLoopStateAddressView GameLoopState;

/* Proposal: per-function JNI method-signature literals. Each is a duplicate
 * function-local `static const char sig[]`, so the assembler disambiguates
 * with the original ELF's own dup-static suffixing: "sig.2"/"sig.3"/"sig.4". */
                /* "(II)V"  @ 0x004da478, ELF name "sig.4" */

/* PadData (0x00490d90, 0xd0 bytes) read as one doubleword at +0x28, the
 * shared half_28/half_2a pair (original: lui 0x49; ld 3512). */
#define PAD_DATA_DOUBLEWORD_AT_28 (*(u64 *)&PadData.half_28)

/*
 * Drop the task from the scheduler and leave the task body. The usual
 * do/while (0) statement-macro idiom; its loop notes are part of the
 * original's code generation: written out plainly at the object check, cse
 * follows the equal branch and reads the class through the thread object
 * (s3/s4 swapped, attempt-4d6f277fff4a form 06). The two exits that first
 * release the claimed VM slot spell the removal out: the macro there too
 * moves their blocks (attempt-5684d45333e0 form 04, 85%).
 */
#define REMOVE_TASK_AND_RETURN(task) \
    do {                             \
        xglTaskRemove(&(task)->task); \
        return;                      \
    } while (0)

static void initJS(int state)
{
    int class_id;

    JS_init(state, 0x40, 8);
    class_id = JS_loadClass(D_004DA460);
    JS_classSetup(class_id, JS_classLight_getPeer);
    JS_classAddMethod(class_id, D_004C1D50, JS_classLight_setColor);
    JS_classAddMethod(class_id, D_004C1D60, JS_classLight_setDirection2);
}

void SCRIPT_test(SceneObject argument, SceneMethod *method)
{
    JNI_initThread(stageVM);
    if (method != 0) {
        SceneObject arguments[1];

        arguments[0] = argument;
        JNI_callMethod(stageVM, method, arguments, 0);
    }
}

void SCRIPT_talkIgnoreSet(void)
{
    s_nScriptTalkLock = 1;
}

void SCRIPT_talkIgnoreClr(void)
{
    s_nScriptTalkLock = 0;
}

int SCRIPT_talkIgnoreGet(void)
{
    return s_nScriptTalkLock;
}

void SCRIPT_sceneChangeTimeInit(void)
{
    s_nScriptChangeTime = 0;
}

int SCRIPT_sceneChangeTimeGet(void)
{
    return s_nScriptChangeTime;
}

void SCRIPT_sceneChangeTimeSet(int value)
{
    s_nScriptChangeTime = value;
}

void SCRIPT_sceneChangeTimeDec(void)
{
    if (s_nScriptChangeTime != 0)
        s_nScriptChangeTime -= 1;
    else
        s_nScriptChangeTime = 0;
}

void SCRIPT_sendMovieSkipSignal(void)
{
    GameLoopMovieStatePrefix *game_state = (GameLoopMovieStatePrefix *)GameLoopState;

    if (game_state->movie_state == 3)
        s_nScriptEventFin = 1;
}

void SCRIPT_clrEventActiveFlag(void)
{
    s_nScriptEventActive = 0;
}

void SCRIPT_setEventActiveFlag(void)
{
    s_nScriptEventActive = 1;
}

int SCRIPT_getEventActiveFlag(void)
{
    return s_nScriptEventActive;
}

void SCRIPT_eventFinish(void)
{
    s_nScriptEventFin = 1;
}

void SCRIPT_methodClearSet(void)
{
    s_nScriptSequenceReset = 1;
}

int SCRIPT_methodClearGet(void)
{
    return s_nScriptSequenceReset;
}

void SCRIPT_fade(int time)
{
    s_nScriptFadeRequest = 1;
    if (time < 0)
        time = 0;
    s_nScriptFadeOutTime = time;
}

int SCRIPT_getFadeTime(void)
{
    return s_nScriptFadeOutTime;
}

int SCRIPT_fadeGet(void)
{
    if (s_nScriptFadeRequest == 0)
        return 0;
    return s_nScriptFadeOutTime;
}

int SCRIPT_getCfTime(void)
{
    return (int)s_nScriptCfTime;
}

void SCRIPT_incCfTime(void)
{
    s_nScriptCfTime += 1u;
}

void SCRIPT_frameLock2Battle(void)
{
    s_nScriptFrameLockEntry = 1;
}

/* Rebuild the default, stage, and two event VMs after a script runtime jump. */
void SCRIPT_reset(void)
{
    SceneThread **event_vm;
    int remaining_vms;

    event_vm = evtVM;
    remaining_vms = 1;
    initVM();
    defaultVM = JNI_createThread(0, 8, 0x80);
    stageVM = JNI_createThread(0, 8, 0x46);
    do {
        --remaining_vms;
        *event_vm = JNI_createThread(0, 8, 0x40);
        ++event_vm;
    } while (remaining_vms >= 0);
    UseVMFlag = 0;
    JNI_pushFrame();
    JNI_pushFrame();
    JNI_loadNativeClass();
}

extern int xglCdReadFile(const char *path, void *buf, int a, int b);
extern void JNI_initSystem(void *heap, int size);
extern int JNI_loadClassDB(int class_id, int value);
extern int D_004DEDD4[];
extern const char D_004C1D70[];
extern const char D_004C1D80[];
typedef struct ScriptInitEntry {
    char *script;
    void *lo;
    void *hi;
    int active;
    SceneThread *thread;
    int kind;
    int unk18;
} ScriptInitEntry;
static inline void setEntry(ScriptInitEntry *db, char *p, void *lo, void *hi, int kind)
{
    ((union { int i; void *p; } *)&db->kind)->i = kind;
    db->lo = lo;
    db->hi = hi;
    db->script = p;
    db->thread = 0;
    ((union { int i; void *p; } *)&db->unk18)->i = 0;
    db->active = 0;
}

char *SCRIPT_init(char *heap)
{
    ScriptInitEntry *db;
    SceneThread **event_vm;
    int remaining_vms;
    char *sys;
    char *base;
    char *p;
    const char *fa;

    p = heap;
    db = (ScriptInitEntry *)scriptDB;
    p += 0x40000;
    sys = p;
    p += 0x8000;
    base = p;
    p += 0x8000;
    event_vm = evtVM;
    remaining_vms = 1;
    ((union { int i; void *p; } *)&db->unk18)->i = 0;
    db->lo = (void *)0x05000000;
    db->hi = (void *)0x06100000;
    *(int *)&db->kind = 2;
    db->script = p;
    db->thread = 0;
    db->active = 0;
    p += 0x2D000;
    fa = D_004C1D70;
    db++;
    D_004DEDD4[0] = 0;
    *(float *)&currentScriptDB = 0.0f;
    setEntry(db, p, (void *)0x06500000, (void *)0x06900000, 3);
    p += 0x2D000;
    xglCdReadFile(fa, sys, 0, 0);
    xglCdReadFile(D_004C1D80, base, 0, 0);
    JNI_initSystem(heap, 0x40000);
    JNI_loadClassDB(0, (int)sys);
    JNI_loadClassDB(1, (int)base);
    initVM();
    defaultVM = JNI_createThread(0, 8, 0x80);
    stageVM = JNI_createThread(0, 8, 0x46);
    do {
        --remaining_vms;
        *event_vm = JNI_createThread(0, 8, 0x40);
        ++event_vm;
    } while (remaining_vms >= 0);
    UseVMFlag = 0;
    JNI_pushFrame();
    JNI_pushFrame();
    JNI_loadNativeClass();
    return p;
}

/*
 * Queue the actor's talk task when it has a talk method, its owning script
 * is active, no other talk is in progress, the actor is a xeno/Chr instance
 * and the current script's VM thread is running on a Stage object.
 */
void SCRIPT_execTalkto(void *actor)
{
    ScriptActorMethods *methods = (ScriptActorMethods *)actor;
    ScriptDbEntry *entry = &scriptDB[currentScriptDB];

    if (methods->talk_method_name != 0 && entry->active != 0 &&
        s_nScriptTalkLock == 0 &&
        JNI_isInstanceOf(methods->script_object, classJava_xeno_Chr) != 0 &&
        JNI_isInstanceOf(entry->thread->object, classJava_xeno_Stage) != 0) {
        /*
         * The original calls createTalkTask with jal and returns through
         * the shared epilogue instead of a sibling jump. Under this TU's
         * compiler the call stays out of tail position only inside a loop
         * construct, which is the shape a do/while (0) statement macro
         * gives it.
         */
        do {
            createTalkTask(actor, methods->talk_method_name);
        } while (0);
    }
}

/*
 * Queue the actor's touch task under the same gate as SCRIPT_execTalkto,
 * but call the actor's touch method instead of its talk method.
 */
void SCRIPT_execTouchto(void *actor)
{
    ScriptActorMethods *methods = (ScriptActorMethods *)actor;
    ScriptDbEntry *entry = &scriptDB[currentScriptDB];

    if (methods->talk_method_name != 0 && entry->active != 0 &&
        s_nScriptTalkLock == 0 &&
        JNI_isInstanceOf(methods->script_object, classJava_xeno_Chr) != 0 &&
        JNI_isInstanceOf(entry->thread->object, classJava_xeno_Stage) != 0) {
        /*
         * Same shape as SCRIPT_execTalkto: the original keeps a real jal
         * to createTalkTask, which only a loop construct reproduces here.
         */
        do {
            createTalkTask(actor, methods->touch_method_name);
        } while (0);
    }
}

extern const char sig0_0[];
extern const char sig1_1[];
extern SceneClass *classJava_xeno_util_Window;
typedef struct TalkTaskWin { int class_ref; char pad[0xC]; unsigned int flags; } TalkTaskWin;
extern TalkTaskWin *TWIN_create2(int component_id);
extern void TWIN_initCF(void *window);
extern void Before_Talk(void *actor);
extern void talktoObserver(ScriptObserverTask *task);
typedef struct ScriptDbThreadSlot { SceneThread *thread; u8 pad[0x18]; } ScriptDbThreadSlot;
extern ScriptDbThreadSlot D_004DEDF0[];

void createTalkTask(void *actor, const char *method_name)
{
    SceneObject receiver;
    SceneClass *scene_class;
    SceneMethod *method;
    ScriptObserverTask *task;

    if (((GameLoopFlagsPrefix *)GameLoopState)->flags & 0x20)
        return;
    receiver = D_004DEDF0[currentScriptDB].thread->object;
    scene_class = ((SceneObjectHeader *)receiver)->class_ref->scene_class;
    method = findMethod(scene_class, loadConstString(method_name, -1),
                        loadConstString(sig0_0, -1));
    if (method != 0) {
        XglTaskScheduler *scheduler = GameLoopState[2];
        task = (ScriptObserverTask *)xglTaskEntryNext(
            scheduler, (int (*)(XglTaskPrefix *))talktoObserver,
            scheduler != 0 ? scheduler->active_tail : 0);
        if (task != 0) {
            task->state_flags = 0;
            task->owner = (GameLoopFlagsPrefix *)GameLoopState;
            task->object = 0;
        }
        task->argument2 = 0;
        task->method_signature = sig0_0;
    } else {
        TalkTaskWin *win;
        XglTaskScheduler *scheduler;

        method = findMethod(scene_class, loadConstString(method_name, -1),
                            loadConstString(sig1_1, -1));
        if (method == 0)
            return;
        scheduler = GameLoopState[2];
        task = (ScriptObserverTask *)xglTaskEntryNext(
            scheduler, (int (*)(XglTaskPrefix *))talktoObserver,
            scheduler != 0 ? scheduler->active_tail : 0);
        if (task != 0) {
            task->state_flags = 0;
            task->owner = (GameLoopFlagsPrefix *)GameLoopState;
            task->object = 0;
        }
        task->method_signature = sig1_1;
        win = TWIN_create2(-1);
        win->class_ref = (int)classJava_xeno_util_Window->instance_class_ref;
        TWIN_initCF(win);
        { unsigned int f = win->flags & ~0x40; f &= ~0x80; win->flags = f; }
        task->argument2 = (int)win;
    }
    task->receiver = receiver;
    task->method_name = method_name;
    task->method = method;
    task->object = actor;
    Before_Talk(actor);
    ((GameLoopFlagsPrefix *)GameLoopState)->flags |= 0x9000;
    XTK_setWindowOwner((int)actor);
    JNI_initThread(stageVM);
    talktoObserver(task);
}

/*
 * Task body queued by createTalkTask: resolve the talk method on the
 * receiver object and run it on the Stage VM, cancelling the talk when the
 * event VMs are busy or the pad state forbids it.
 */
void talktoObserver(ScriptObserverTask *task)
{
    unsigned int owner_flags = task->owner->flags;
    SceneObject receiver;
    SceneObject object;
    SceneMethod *method;
    int script_flags;
    SceneObject arguments[3];

    if (owner_flags & 0x800) {
        xglTaskRemove(&task->task);
        return;
    }
    if (owner_flags & 0x20) {
        xglTaskRemove(&task->task);
        return;
    }

    receiver = task->receiver;
    {
        SceneObjectHeader *header = (SceneObjectHeader *)receiver;
        SceneClass *scene_class = header->class_ref->scene_class;
        SceneString *name = loadConstString(task->method_name, -1);
        SceneString *signature = loadConstString(task->method_signature, -1);

        task->method = findMethod(scene_class, name, signature);
    }
    if (task->method == 0) {
        xglTaskRemove(&task->task);
        return;
    }

    object = task->object;
    script_flags = getScriptFlag(object);
    if (task->argument2 != 0 && (script_flags & 8)) {
        if (UseVMFlag != 0 || (PAD_DATA_DOUBLEWORD_AT_28 & 0x600000) == 0x400000) {
            XTK_setWindowOwner(0);
            talkCancel(task);
            return;
        }
    }

    /*
     * The method is read back from the task before the argument array is
     * filled: the original loads it (lw a1,60(s2)) ahead of the stageVM
     * argument, which only happens when the array stores separate the read
     * from the call (a read in the call itself, attempt-4d6f277fff4a form
     * 08, is scheduled after the stageVM load).
     */
    method = task->method;
    arguments[0] = receiver;
    arguments[1] = *(SceneObject *)((SceneByte *)object + ACTOR_SCRIPT_OBJECT_OFFSET);
    if (task->argument2 != 0)
        arguments[2] = (SceneObject)task->argument2;
    JNI_callMethod(stageVM, method, arguments, 0);
    if (stageVM->flags & SCENE_THREAD_CALL_PENDING) {
        XTK_setWindowOwner(0);
        actTalkAfter(object);
        ((GameLoopFlagsPrefix *)GameLoopState)->flags &= ~0x1000;
        ((GameLoopFlagsPrefix *)GameLoopState)->flags &= ~0x8000;
        ((GameLoopFlagsPrefix *)GameLoopState)->flags &= ~0x10000;
        xglTaskRemove(&task->task);
    }
}

extern void TWIN_dispose(void *window);

typedef struct TalkWin { char pad[0x10]; unsigned int flags; short state; } TalkWin;

void talkCancel(ScriptObserverTask *task) {
    TalkWin *win;

    actTalkAfter(task->object);
    win = (TalkWin *)task->argument2;
    win->flags = (win->flags | 0x80) & ~2;
    ((GameLoopFlagsPrefix *)GameLoopState)->flags &= ~0x1000;
    ((GameLoopFlagsPrefix *)GameLoopState)->flags &= ~0x8000;
    ((GameLoopFlagsPrefix *)GameLoopState)->flags &= ~0x10000;
    if (win != 0) {
        win->state = 2;
    }
    TWIN_dispose(win);
    xglTaskRemove(&task->task);
}

int attrObserver(XglTaskPrefix *task);

void SCRIPT_execEntered(int arg)
{
    ScriptDbEntry *entry = &scriptDB[currentScriptDB];

    if (entry->active != 0) {
        SceneObject object = entry->thread->object;

        if (!(((GameLoopFlagsPrefix *)GameLoopState)->flags & 0x400) &&
            JNI_isInstanceOf(object, classJava_xeno_Stage) != 0) {
            XglTaskScheduler *scheduler = GameLoopState[2];
            ScriptObserverTask *task = (ScriptObserverTask *)xglTaskEntryNext(
                scheduler, attrObserver,
                scheduler != 0 ? scheduler->active_tail : 0);

            if (task != 0) {
                task->owner = (GameLoopFlagsPrefix *)GameLoopState;
                task->state_flags = 0;
                task->object = 0;
            }
            task->argument1 = arg;
            task->object = object;
            ((GameLoopFlagsPrefix *)GameLoopState)->flags |= 0x100000;
        }
    }
}

/*
 * Queue a funcObserver task that invokes the no-argument script method
 * `method_name` ("()V") on the current Stage object, then run it once.
 * Every caller ignores $v0, so the entry point returns nothing.
 */
void CallMethod(const char *method_name)
{
    ScriptDbEntry *entry = &scriptDB[currentScriptDB];

    if (entry->active != 0) {
        SceneObject object = entry->thread->object;

        if (JNI_isInstanceOf(object, classJava_xeno_Stage) != 0) {
            XglTaskScheduler *scheduler = GameLoopState[2];
            ScriptObserverTask *task = (ScriptObserverTask *)xglTaskEntryNext(
                scheduler, (int (*)(XglTaskPrefix *))funcObserver,
                scheduler != 0 ? scheduler->active_tail : 0);
            unsigned char started;

            if (task != 0) {
                task->owner = (GameLoopFlagsPrefix *)GameLoopState;
                task->state_flags = 0;
                task->object = 0;
            }
            task->object = object;
            task->method_name = method_name;
            task->method_signature = call_method_signature_void;
            task->invocation_state = 0;
            funcObserver(task);
            /* separate load keeps the funcObserver call out of tail position:
             * without it (form 11) GCC emits a sibling `j funcObserver` */
            started = task->invocation_state;
        }
    }
}

/*
 * Queue a funcObserver task that invokes the one-int-argument script method
 * `method_name` ("(I)V") on the current Stage object, then run it once.
 * Every caller ignores $v0, so the entry point returns nothing.
 */
void CallMethod_I(const char *method_name, int arg1)
{
    ScriptDbEntry *entry = &scriptDB[currentScriptDB];

    if (entry->active != 0) {
        SceneObject object = entry->thread->object;

        if (JNI_isInstanceOf(object, classJava_xeno_Stage) != 0) {
            XglTaskScheduler *scheduler = GameLoopState[2];
            ScriptObserverTask *task = (ScriptObserverTask *)xglTaskEntryNext(
                scheduler, (int (*)(XglTaskPrefix *))funcObserver,
                scheduler != 0 ? scheduler->active_tail : 0);
            unsigned char started;

            if (task != 0) {
                task->owner = (GameLoopFlagsPrefix *)GameLoopState;
                task->state_flags = 0;
                task->object = 0;
            }
            task->object = object;
            task->method_name = method_name;
            task->method_signature = call_method_signature_int;
            task->argument1 = arg1;
            task->invocation_state = 0;
            funcObserver(task);
            /* separate load keeps the funcObserver call out of tail position:
             * without it (form 11) GCC emits a sibling `j funcObserver` */
            started = task->invocation_state;
        }
    }
}

/*
 * Queue a funcObserver task that invokes the two-int-argument script method
 * `method_name` ("(II)V") on the current Stage object, then run it once.
 * Every caller ignores $v0, so the entry point returns nothing.
 */
void CallMethod_II(const char *method_name, int arg1, int arg2)
{
    ScriptDbEntry *entry = &scriptDB[currentScriptDB];

    if (entry->active != 0) {
        SceneObject object = entry->thread->object;

        if (JNI_isInstanceOf(object, classJava_xeno_Stage) != 0) {
            XglTaskScheduler *scheduler = GameLoopState[2];
            ScriptObserverTask *task = (ScriptObserverTask *)xglTaskEntryNext(
                scheduler, (int (*)(XglTaskPrefix *))funcObserver,
                scheduler != 0 ? scheduler->active_tail : 0);
            unsigned char started;

            if (task != 0) {
                task->owner = (GameLoopFlagsPrefix *)GameLoopState;
                task->state_flags = 0;
                task->object = 0;
            }
            task->object = object;
            task->method_name = method_name;
            task->method_signature = call_method_signature_int_int;
            task->argument1 = arg1;
            task->argument2 = arg2;
            task->invocation_state = 0;
            funcObserver(task);
            /* separate load keeps the funcObserver call out of tail position:
             * without it (form 11) GCC emits a sibling `j funcObserver` */
            started = task->invocation_state;
        }
    }
}

/*
 * Task body queued by CallMethod/CallMethod_I/CallMethod_II: claim a free
 * event VM, resolve the named method on the Stage object that queued it and
 * invoke it with up to two int arguments. The task is dropped when the
 * owner flags forbid it, no VM is free, the Stage object changed, the
 * method is missing or the call left an exception pending; after a
 * successful call it stays queued and GameLoopState flag 0x200000 is set.
 */
void funcObserver(ScriptObserverTask *task)
{
    unsigned int owner_flags;
    ScriptDbEntry *entry;
    signed char vm_slot;
    SceneObject object;

    xglFontDebugPrintf(8, 24, func_observer_debug_text);

    owner_flags = task->owner->flags;
    if (owner_flags & 0x800)
        REMOVE_TASK_AND_RETURN(task);
    if ((task->state_flags & 1) == 0) {
        if (owner_flags & 0x400)
            REMOVE_TASK_AND_RETURN(task);
        task->state_flags |= 1;
        if (getEmptyVM(task) == 0)
            REMOVE_TASK_AND_RETURN(task);
    }

    entry = &scriptDB[currentScriptDB];
    vm_slot = task->vm_slot;
    if (entry->active == 0)
        return;

    /* The Stage object changed since CallMethod queued the task. */
    object = entry->thread->object;
    if (object != task->object)
        REMOVE_TASK_AND_RETURN(task);

    {
        SceneObjectHeader *header = (SceneObjectHeader *)task->object;
        SceneClass *scene_class = header->class_ref->scene_class;
        SceneString *name = loadConstString(task->method_name, -1);
        SceneString *signature = loadConstString(task->method_signature, -1);
        SceneMethod *method = findMethod(scene_class, name, signature);
        SceneObject arguments[3];

        task->method = method;
        if (method == 0) {
            UseVMFlag &= ~(1 << vm_slot);
            xglTaskRemove(&task->task);
            return;
        }

        arguments[0] = object;
        arguments[1] = (SceneObject)task->argument1;
        arguments[2] = (SceneObject)task->argument2;
        JNI_callMethod(evtVM[vm_slot], method, arguments, 0);
        if (evtVM[vm_slot]->flags & SCENE_THREAD_CALL_PENDING) {
            JNI_catchException();
            UseVMFlag &= ~(1 << vm_slot);
            xglTaskRemove(&task->task);
            return;
        }
    }
    ((GameLoopFlagsPrefix *)GameLoopState)->flags |= 0x200000;
}

int getEmptyVM(ScriptObserverTask *observer)
{
    int slot;

    observer->vm_slot = -1;
    for (slot = 0; slot < 2; ++slot) {
        if (((UseVMFlag >> slot) & 1) == 0) {
            JNI_initThread(evtVM[slot]);
            observer->state_flags |= 1;
            UseVMFlag |= 1 << slot;
            observer->vm_slot = (signed char)slot;
            return 1;
        }
    }
    return observer->vm_slot != -1;
}

extern void loadScriptCD(ScriptDbEntry *entry, const char *path);

void SCRIPT_load(const char *path)
{
    loadScriptCD(&scriptDB[(currentScriptDB + 1) & 1], path);
}

extern int loadScriptCD2(ScriptDbEntry *entry, const char *path);

int SCRIPT_load2(const char *path)
{
    return loadScriptCD2(&scriptDB[(currentScriptDB + 1) & 1], path);
}

extern int xglFontLoad(int type, int callback);
extern char *debugBasePath;
extern int loadScript(ScriptDbEntry *entry, const char *base, const char *path);
typedef struct { int pad; void *a; void *b; int rest[4]; } ScriptDbView;

int SCRIPT_load_DBG(const char *path)
{
  ScriptDbView *db;
  char **new_var;
  xglFontLoad((double) 0, 0);
  db = (ScriptDbView *) scriptDB;
  db->a = (void *) 0x5000000;
  db->b = (void *) 0x6100000;
  db++;
  db->a = (void *) 0x6500000;
  db->b = (void *) 0x6900000;
  new_var = &debugBasePath;
  return loadScript(&scriptDB[(currentScriptDB + 1) & 1], *new_var, path);
}

static char *getStrIndex(char *string, unsigned int length, int ch)
{
    int i;

    for (i = length - 1; i >= 0; i--) {
        if (string[i] == ch) {
            return string + i;
        }
    }
    return 0;
}

extern char *strcpy(char *destination, const char *source);
extern unsigned int strlen(const char *string);
static char *getStrIndex(char *string, unsigned int length, int ch);

/*
 * loadScriptCD/loadScriptCD2/loadScript all capture this call's return
 * value ($v0 into $s0, e.g. build/main/asm/main/nonmatchings/script/
 * loadScriptCD.s "jal replacePathExt" / "daddu $16, $2, $0"): the tail
 * call to strcpy leaves its return (the extension write position) in v0,
 * and the caller reads it, so this returns that pointer instead of void.
 */
static char *replacePathExt(char *path, const char *ext)
{
    return strcpy(getStrIndex(path, strlen(path), '.'), ext);
}

extern char D_004DA480[];
extern char D_004DA488[];

int attrObserver(XglTaskPrefix *task_)
{
    ScriptObserverTask *task = (ScriptObserverTask *)task_;
    SceneObject object = task->object;
    SceneMethod *method;
    signed char vm_slot;
    int arg;
    SceneObject arguments[2];

    if ((task->state_flags & 1) == 0) {
        unsigned int owner_flags = task->owner->flags;
        if (owner_flags & 0x400) {
            xglTaskRemove(&task->task);
            return;
        }
        task->owner->flags = owner_flags | 0x400;
        if (getEmptyVM(task) == 0) {
            xglTaskRemove(&task->task);
            return;
        }
        task->state_flags |= 1;
    }
    {
        SceneObjectHeader *header = (SceneObjectHeader *)object;
        SceneClass *scene_class = header->class_ref->scene_class;
        SceneString *name = loadConstString(D_004DA480, -1);
        SceneString *signature = loadConstString(D_004DA488, -1);
        method = findMethod(scene_class, name, signature);
    }
    if (method == 0) {
        task->owner->flags &= ~0x400;
        xglTaskRemove(&task->task);
        return;
    }
    vm_slot = task->vm_slot;
    arg = task->argument1;
    ((union { int i; SceneThread *t; } *)arguments)->t = (SceneThread *)object;
    arguments[1] = (SceneObject)arg;
    JNI_callMethod(evtVM[vm_slot], method, arguments, 0);
    if (evtVM[vm_slot]->flags & SCENE_THREAD_CALL_PENDING) {
        JNI_catchException();
        task->owner->flags &= ~0x400;
        UseVMFlag &= ~(1 << vm_slot);
        xglTaskRemove(&task->task);
        ((GameLoopFlagsPrefix *)GameLoopState)->flags &= ~0x100000;
    }
}

extern struct {
    u8 pad_00[0x58];
    u8 pause;
} sRender;
extern int s_nScriptEventSkipEntry;
extern void xglSoundEffectNormalDirect(int effect_id);
extern void xglRenderCopyDisp2Draw(void);
extern void GamePauseDispEvent(void);
extern void PartyTimePauseStart(void);
extern void PartyTimePauseEnd(void);
extern void xglSoundStreamStop(int stream);
extern void xglSoundStreamMute(void);
typedef struct {
    u8 pad_00[0x10];
    u32 flags;
    u8 pad_14[0x29F40 - 0x14];
    u8 pause_wait;
} PauseGameLoopView;

static int PauseSkipCheck(void)
{
    PauseGameLoopView *gs = (PauseGameLoopView *)GameLoopState;
    PadPrefix *pad;

    if (gs->flags & 0x90000000) {
        return 1;
    }
    if (gs->pause_wait == 0 && ((pad = &PadData)->half_2a & 0x800)) {
        gs->pause_wait = 0x20;
        xglSoundEffectNormalDirect(4);
        sRender.pause = 1;
        xglSleep();
        xglRenderCopyDisp2Draw();
        GamePauseDispEvent();
        xglSleep();
        GamePauseDispEvent();
        xglSleep();
        PartyTimePauseStart();
        if (!(pad->half_2a & 0x10)) {
            for (;;) {
                if (gs->pause_wait != 0) {
                    gs->pause_wait--;
                    xglSleep();
                    if (pad->half_2a & 0x10) break;
                    continue;
                }
                if (((volatile PadPrefix *)pad)->half_2a & 0x800) break;
                xglSleep();
                if (pad->half_2a & 0x10) break;
            }
        }
        PartyTimePauseEnd();
        xglSoundEffectNormalDirect(4);
        sRender.pause = 0;
        ((PauseGameLoopView *)GameLoopState)->pause_wait = 0x20;
        if (PadData.half_2a & 0x10) {
            ((PauseGameLoopView *)GameLoopState)->pause_wait = 0x40;
            if (s_nScriptFrameLockEntry == 0) {
                s_nScriptEventSkipEntry = 1;
            }
            xglSoundStreamStop(0);
            return 1;
        }
        xglSoundStreamMute();
    }
    return 0;
}

typedef struct ScriptGameLoopState {
    unsigned char unmodeled_00[4];
    void *main_actor;
    XglTaskScheduler *task_scheduler;
    unsigned short kind;
    unsigned short kindDetail;
    unsigned int flags;
    unsigned char unmodeled_14[0x54 - 0x14];
    int scene_reset_counter;
    unsigned char unmodeled_58[0x90 - 0x58];
    float fade_out_red;
    float fade_out_green;
    float fade_out_blue;
    float fade_out_frames;
    float fade_red;
    float fade_green;
    float fade_blue;
    float fade_frames;
    unsigned char unmodeled_b0[8];
    int frame_lock_wait;
} ScriptGameLoopState;
typedef struct ScriptRenderState {
    unsigned char unmodeled_00[0x22];
    unsigned short owner_index;     /* +0x22: passed to nmlModelSetBackBufferToBattle */
    unsigned char unmodeled_24[0x58 - 0x24];
    unsigned char scene_disabled;
} ScriptRenderState;
typedef struct ScriptSceneTask {
    XglTaskPrefix task;
    u32 state_flags;
    SceneVm *thread;
    unsigned char unmodeled_18[4];
    ScriptGameLoopState *owner;
    int fade_started;
    int elapsed_frames;
} ScriptSceneTask;
extern ScriptGameLoopState GameLoopStateV __asm__("GameLoopState");
extern ScriptRenderState sRenderV __asm__("sRender");
extern void nmlModelSetFadeOut(int frames, int option);
extern void nmlModelSetFadeIn(int frames, int option);
extern void FX_ScreenMask(int unused, int color, int frames, int mode);
extern void nmlModelSetBackBufferToBattle(int owner_index);
extern void nmlModelSetFadeInCancel(int frames);
extern void nmlModelSetFadeOutCancel(int frames);
extern void nmlModelSetEffectWrite(int enabled);
extern void nmlModelSendSignalEventFinish(void);
extern void nmlModelSetGlobalPointLightReset(void);
extern void MAP_initUnit(void);
extern void ACT_initScene(void);
extern void ACT_initMTNResource(void);
extern void TWSYS_init(void);
extern void MSG_init(void);
extern void xglRenderClearColor(u32 color);
extern void SCENE_cleanup(SceneVm *vm, SceneObject scene_object);
extern void xglStudioGetCamera(StudioCamera **camera_out, int camera_index);
extern void xglCameraClipRangeDefault(StudioCamera *camera);
#define SCRIPT_FADE_COLOR(red, green, blue)                                    \
    (0x7F000000 + ((int)((red) * 127.0f) << 16) +                              \
     ((int)((green) * 127.0f) << 8) + (int)((blue) * 127.0f))

void sceneObserver2(ScriptSceneTask *task)
{
    SceneThread *thread;
    StudioCamera *camera;
    int camera_index;

    if (!(task->state_flags & 1)) {
        task->state_flags |= 1;
        task->fade_started = 0;
        task->elapsed_frames = 0;
    }
    thread = (SceneThread *)task->thread;
    if (PauseSkipCheck() != 0 || s_nScriptEventFin != 0) {
        thread->flags = (thread->flags & ~0x10) | SCENE_THREAD_CALL_PENDING;
        s_nScriptEventFin = 0;
    }
    if (!(thread->flags & SCENE_THREAD_CALL_PENDING))
        return;
    if (task->fade_started == 0) {
        task->fade_started = 1;
        if (s_nScriptFrameLockEntry == 0) {
            if ((int)GameLoopStateV.fade_out_frames > 0) {
                nmlModelSetFadeOut((int)GameLoopStateV.fade_out_frames, 0);
                FX_ScreenMask(0,
                              SCRIPT_FADE_COLOR(GameLoopStateV.fade_out_red,
                                                GameLoopStateV.fade_out_green,
                                                GameLoopStateV.fade_out_blue),
                              (int)GameLoopStateV.fade_out_frames, 2);
            }
        } else {
            GameLoopStateV.frame_lock_wait = s_nScriptFadeOutTime + 4;
        }
        if (s_nScriptEventSkipEntry != 0 && s_nScriptFrameLockEntry == 0)
            nmlModelSetBackBufferToBattle(sRenderV.owner_index);
        s_nScriptChangeTime = (int)GameLoopStateV.fade_out_frames + 1;
    }
    task->elapsed_frames++;
    if ((int)s_nScriptFadeOutTime >= task->elapsed_frames)
        return;
    if (s_nScriptFrameLockEntry != 0) {
        nmlModelSetBackBufferToBattle(sRenderV.owner_index);
        nmlModelSetFadeInCancel(30);
        nmlModelSetFadeOutCancel(30);
    }
    if (s_nScriptEventSkipEntry != 0) {
        nmlModelSendSignalEventFinish();
        s_nScriptEventSkipEntry = 0;
    }
    s_nScriptFadeOutTime = 30;
    s_nScriptChangeTime = 25;
    camera_index = 0;
    s_nScriptEventActive = 0;
    s_nScriptFrameLockEntry = 0;
    s_nScriptFadeRequest = 0;
    s_nScriptTalkLock = 0;
    MAP_initUnit();
    ACT_initScene();
    ACT_initMTNResource();
    TWSYS_init();
    MSG_init();
    s_nScriptSequenceReset = 0;
    xglRenderClearColor(0x80000000);
    GameLoopStateV.kind = 2;
    s_nScriptCfTime = 0;
    nmlModelSetGlobalPointLightReset();
    SCENE_cleanup(stageVM, thread->object);
    JNI_catchException();
    task->owner->flags &= ~0x800;
    do {
        xglStudioGetCamera(&camera, camera_index);
        camera_index++;
        xglCameraClipRangeDefault(camera);
    } while (camera_index < 8);
    nmlModelSetGlobalPointLightReset();
    nmlModelSetEffectWrite(1);
    nmlModelSetFadeIn((int)GameLoopStateV.fade_frames, 0);
    FX_ScreenMask(0,
                  SCRIPT_FADE_COLOR(GameLoopStateV.fade_red,
                                    GameLoopStateV.fade_green,
                                    GameLoopStateV.fade_blue),
                  (int)GameLoopStateV.fade_frames, 3);
    xglTaskRemove(&task->task);
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct ScriptActorRecord {
    unsigned int flags;                 /* +0x00 */
    unsigned char unmodeled_04[0xc];
    float position[3];                  /* +0x10 */
    unsigned char unmodeled_1c[0x34];
    float rotation[3];                  /* +0x50: radians */
    unsigned char unmodeled_5c[0x86 - 0x5c];
    short in_use_id;                    /* +0x86 */
    unsigned char unmodeled_88[0x4c0 - 0x88];
    SceneObject script_object;          /* +0x4c0: the actor's own script object */
    unsigned char unmodeled_4c4[0xa70 - 0x4c4];
} ScriptActorRecord;

typedef struct ScriptPadButtons {
    unsigned char unmodeled_00[0x28];
    unsigned short held;
    unsigned short pressed;
    unsigned short repeat;
} ScriptPadButtons;

typedef struct ScriptPadRecord {
    unsigned char unmodeled_00[0x28];
    unsigned long long packed_input;
    unsigned char unmodeled_30[0x68 - 0x30];
} ScriptPadRecord;

typedef union ScriptMethodArgument {
    int integer;
    void *reference;
} ScriptMethodArgument;

typedef struct ScriptWindow {
    unsigned char unmodeled_00[0x10];
    unsigned int flags;           /* +0x10 */
    unsigned short state;         /* +0x14: 0 while open, 2 once the choice is made */
    unsigned short mode;          /* +0x16 */
    unsigned char unmodeled_18[8];
    float x;                      /* +0x20 */
    float y;                      /* +0x24 */
    unsigned char unmodeled_28[0x50 - 0x28];
    unsigned int options;         /* +0x50 */
    signed char selected_item;    /* +0x54 */
    signed char cancel_item;      /* +0x55: negative when the menu was cancelled */
    unsigned char unmodeled_56[2];
    int value;                    /* +0x58 */
    unsigned char unmodeled_5c[4];
    int max_value;                /* +0x60 */
} ScriptWindow;

typedef struct ScriptFileSelect {
    unsigned char unmodeled_00[0xc];
    int selection;                /* +0xc: cleared when the browser opens */
    char directory[1];            /* +0x10 */
    unsigned char unmodeled_11[0x11c - 0x11];
    char file_path[1];            /* +0x11c */
    unsigned char unmodeled_11d[0x268 - 0x11d];
} ScriptFileSelect;

typedef struct ScriptMenuTask {
    XglTaskPrefix task;              /* +0x00 */
    u32 state_flags;                 /* +0x10: bit 0 set once the task has set itself up */
    void *window;                    /* +0x14 */
    unsigned char unmodeled_18[4];
    ScriptGameLoopState *owner;      /* +0x1c */
    int phase;                       /* +0x20 */
    int map_no;                      /* +0x24 */
    int loaded_map;                  /* +0x28 */
} ScriptMenuTask;

typedef struct ScriptWindParams ScriptWindParams;

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct TComponent {
    SceneObjectClassRef *class_ref;
    unsigned char unmodeled_04[0xc];
    unsigned int flags;
    unsigned short state;
} TComponent;

extern void JNI_popFrame(void);

extern int MapGetNo(void);

extern void SCENE_instance(SceneVm *vm, SceneObject scene_object);

extern void SCENE_start(SceneVm *vm, SceneObject scene_object, int mode,
                        void *method_name);

extern const char D_004DA490[];

extern void GameMovieStop(void);
extern void GameDefocusSet(int index, int a, int b);
extern void ACT_init(void);
extern void sefInitEffectCf(void);

void sceneObserver(XglTaskPrefix *task_prefix)
{
    ScriptSceneTask *task = (ScriptSceneTask *)task_prefix;
    SceneThread *thread;
    ScriptDbEntry *entry;
    StudioCamera *camera;
    int camera_index;

    if (!(task->state_flags & 1)) {
        SceneObject object;

        task->state_flags |= 1;
        task->fade_started = 0;
        task->elapsed_frames = 0;
        entry = &scriptDB[currentScriptDB];
        object = newObject(loadClass(loadConstString((const char *)entry->active,
                                                     -1), 0));
        thread = JNI_createThread(2, 6, 0x46);
        {
            SceneVm *instance_vm = defaultVM;

            thread->object = object;
            *(SceneObject *)&entry->thread = (SceneObject)thread;
            *(SceneThread **)&task->thread = thread;
            SCENE_instance(instance_vm, thread->object);
            SCENE_start(defaultVM, thread->object, 8, (const char *)D_004DA490);
        }
    }

    thread = (SceneThread *)task->thread;
    if (thread->call_frame == 0)
        xglTaskRemove(task_prefix);
    if (PauseSkipCheck() != 0 || s_nScriptEventFin != 0) {
        thread->flags = (thread->flags & ~0x10) | SCENE_THREAD_CALL_PENDING;
        s_nScriptEventFin = 0;
    }
    if (!(thread->flags & SCENE_THREAD_CALL_PENDING))
        return;

    if (task->fade_started == 0) {
        task->fade_started = 1;
        if (s_nScriptFrameLockEntry == 0) {
            if ((int)GameLoopStateV.fade_out_frames > 0) {
                nmlModelSetFadeOut((int)GameLoopStateV.fade_out_frames, 0);
                FX_ScreenMask(0,
                              SCRIPT_FADE_COLOR(GameLoopStateV.fade_out_red,
                                                GameLoopStateV.fade_out_green,
                                                GameLoopStateV.fade_out_blue),
                              (int)GameLoopStateV.fade_out_frames, 2);
            }
        } else {
            GameLoopStateV.frame_lock_wait = s_nScriptFadeOutTime + 4;
        }
        if (s_nScriptEventSkipEntry != 0 && s_nScriptFrameLockEntry == 0)
            nmlModelSetBackBufferToBattle(sRenderV.owner_index);
        s_nScriptChangeTime = (int)GameLoopStateV.fade_out_frames + 1;
    }
    task->elapsed_frames++;
    if ((int)s_nScriptFadeOutTime >= task->elapsed_frames)
        return;

    if (s_nScriptFrameLockEntry != 0) {
        nmlModelSetBackBufferToBattle(sRenderV.owner_index);
        nmlModelSetFadeInCancel(30);
        nmlModelSetFadeOutCancel(30);
    }
    if (s_nScriptEventSkipEntry != 0) {
        nmlModelSendSignalEventFinish();
        s_nScriptEventSkipEntry = 0;
    }
    s_nScriptFadeOutTime = 30;
    s_nScriptChangeTime = 25;
    s_nScriptFrameLockEntry = 0;
    s_nScriptFadeRequest = 0;
    s_nScriptEventActive = 0;
    s_nScriptTalkLock = 0;
    xglSoundStreamStop(0);
    GameMovieStop();
    GameDefocusSet(-1, 0, 0);
    MAP_initUnit();
    ACT_init();
    ACT_initMTNResource();
    TWSYS_init();
    MSG_init();
    sefInitEffectCf();
    JNI_popFrame();
    GameLoopStateV.kind = 2;
    entry = &scriptDB[currentScriptDB ^ 1];
    currentScriptDB ^= 1;
    nmlModelSetGlobalPointLightReset();
    s_nScriptCfTime = 0;
    s_nScriptSequenceReset = 0;
    xglRenderClearColor(0x80000000U);
    nmlModelInit();
    GameResourceReset(0);
    GameLoopStateV.scene_reset_counter = 0;
    MapChange2(MapGetNo());
    for (camera_index = 0; camera_index < 8; camera_index++) {
        StudioCamera *configured_camera;

        xglStudioGetCamera(&camera, camera_index);
        xglCameraClipRangeDefault(camera);
        configured_camera = camera;
        if (camera_index == 0) {
            configured_camera->state = 4;
            configured_camera->active = 1;
        } else {
            configured_camera->active = 0;
        }
    }
    if (entry->thread == 0) {
        task->owner->flags &= ~0x800;
        JNI_loadClassDB(entry->class_slot, 0);
        entry = &scriptDB[(currentScriptDB + 1) & 1];
        entry->thread = 0;
    } else {
        JNI_loadClassDB(entry->class_slot, entry->class_image);
        task->owner->flags &= ~0x800;
        entry = &scriptDB[(currentScriptDB + 1) & 1];
        JNI_loadClassDB(entry->class_slot, 0);
    }
    nmlModelSetGlobalPointLightReset();
    nmlModelSetEffectWrite(1);
    nmlModelSetFadeIn((int)GameLoopStateV.fade_frames, 0);
    FX_ScreenMask(0,
                  SCRIPT_FADE_COLOR(GameLoopStateV.fade_red,
                                    GameLoopStateV.fade_green,
                                    GameLoopStateV.fade_blue),
                  (int)GameLoopStateV.fade_frames, 3);
    xglTaskRemove(task_prefix);
}

typedef struct DbLightOwner {
    unsigned char unmodeled_00[0xc];
    unsigned short kind;            /* +0x0c */
    unsigned short kind_detail;
    unsigned int flags;             /* +0x10 */
    unsigned char unmodeled_14[0x29f60 - 0x14];
    unsigned short saved_kind;      /* +0x29f60 */
} DbLightOwner;
extern ScriptActorRecord actor[64];
extern int lightSave;
extern int lightActFlags[];
extern void initLight2(void);
extern void initLight3(void);
extern int updateLight(int mode);
extern void *fileSelect;
int DB_lightSave();
extern void TWSYS_setGRPStatus(int group, int status);

static void DB_light(XglTaskPrefix *task_prefix)
{
    ScriptMenuTask *task = (ScriptMenuTask *)task_prefix;
    ScriptPadButtons *pad = (ScriptPadButtons *)&PadData;
    unsigned int state_flags;

    state_flags = task->state_flags;
    if (!(state_flags & 1)) {
        int i;

        task->state_flags = state_flags | 1;
        if (pad->held & 0x80)
            initLight3();
        else
            initLight2();

        for (i = 0; i < 64; i++) {
            int word = i >> 5;
            int shift = i & 31;
            if (actor[i].flags & 0x8000)
                lightActFlags[word] |= 1U << shift;
            else
                lightActFlags[word] &= ~(1U << shift);
            actor[i].flags &= ~0x8000;
        }
    }

    if (lightSave != 0) {
        int result = DB_lightSave(fileSelect);

        lightSave = result != 0 ? 0 : lightSave;
        pad->pressed = 0;
    } else {
        if (pad->pressed & 0x10)
            lightSave = 1;
        updateLight(0);
    }

    if (pad->pressed & 0x40) {
        int i;

        for (i = 0; i < 64; i++) {
            if ((lightActFlags[i >> 5] >> (i & 31)) & 1)
                actor[i].flags |= 0x8000;
        }
        ((DbLightOwner *)task->owner)->flags &= ~1;
        ((DbLightOwner *)task->owner)->kind = ((DbLightOwner *)task->owner)->saved_kind;
        TWSYS_setGRPStatus(0, 0);
        xglTaskRemove(&task->task);
    }
}

typedef struct DbVmMenu {
    u8 unmodeled_00[0x14];
    unsigned short state;
    unsigned short mode;
    u8 unmodeled_18[0x38];
    int value;
    signed char index;
    signed char cancel;
} DbVmMenu;
typedef struct DbVmOwner {
    u8 unmodeled_00[0xC];
    unsigned short state_0c;
    u8 unmodeled_0e[2];
    u32 flags;
    u8 unmodeled_14[0x29F60 - 0x14];
    unsigned short state_29f60;
} DbVmOwner;
typedef struct DbVmTask {
    XglTaskPrefix task;
    u32 flags;
    DbVmMenu *menu;
    u8 unmodeled_18[4];
    DbVmOwner *owner;
} DbVmTask;
typedef struct DbVmEvtTask {
    XglTaskPrefix task;
    int flags;
    int object;
    u8 unmodeled_18[4];
    void *owner;
} DbVmEvtTask;
extern DbVmMenu *TMENU_create(int slot);
extern void TMENU_setItem(DbVmMenu *menu, int index, char *text);
extern void TWSYS_setGRPStatus(int group, int status);
extern int DB_evtMonitor(XglTaskPrefix *task);
extern int vmDebugMask;
extern char *vmSwitchStr[];

static void DB_vmSwitch(DbVmTask *task)
{
    DbVmMenu *menu = task->menu;

    if (!(task->flags & 1)) {
        int i = 0;
        int bit = 1;
        menu = TMENU_create(-1);
        menu->value = 0;
        menu->mode |= 0x10;
        while (i < 8 && vmSwitchStr[i * 2] != 0) {
            int on = (vmDebugMask & bit) != 0;
            int k = i * 2;
            i++;
            TMENU_setItem(menu, -1, vmSwitchStr[k + on]);
            bit = 1 << (i & 0x1f);
        }
        task->menu = menu;
        task->flags |= 1;
    }
    if (menu->state == 0) {
        int idx = menu->index;
        int bit, on;
        if (menu->cancel < 0) {
            DbVmOwner *o = task->owner;
            menu->state = 2;
            o->flags &= ~1;
            o->state_0c = o->state_29f60;
            TWSYS_setGRPStatus(0, 0);
            xglTaskRemove(&task->task);
            return;
        }
        bit = 1 << (idx & 0x1f);
        if (vmDebugMask & bit) {
            on = 0;
            vmDebugMask &= ~bit;
        } else {
            vmDebugMask |= bit;
            on = 1;
        }
        TMENU_setItem(menu, idx, vmSwitchStr[idx * 2 + on]);
        menu->state = 0xE;
        if (idx == 4) {
            XglTaskScheduler *scheduler = GameLoopState[2];
            DbVmEvtTask *t = (DbVmEvtTask *)xglTaskEntryNext(
                scheduler, DB_evtMonitor,
                scheduler != 0 ? scheduler->active_tail : 0);
            if (t != 0) {
                t->owner = GameLoopState;
                t->object = 0;
                t->flags = 0;
            }
        }
    }
}

typedef struct DbSsSlider {
    u8 unmodeled_00[0x14];
    unsigned short state;
    unsigned short mode;
    u8 unmodeled_18[8];
    float x;
    float y;
    u8 unmodeled_28[0x30];
    int value;
    u8 unmodeled_5c[4];
    int max_value;
} DbSsSlider;
typedef struct DbSsOwner {
    u8 unmodeled_00[0xC];
    unsigned short state_0c;
    u8 unmodeled_0e[2];
    u32 flags;
    u8 unmodeled_14[0x29F60 - 0x14];
    unsigned short state_29f60;
} DbSsOwner;
typedef struct DbSsTask {
    XglTaskPrefix task;
    u32 flags;
    DbSsSlider *slider;
    u8 unmodeled_18[4];
    DbSsOwner *owner;
    int mode;
    int map;
    int entry;
} DbSsTask;
extern void *TSLIDER_create(int requestedSlot);
extern char *MapGetName(int map_id);
extern void LoadMap2(int map_id, int event_index);
extern void TWSYS_setGRPStatus(int group, int status);
extern char D_004C1E90[];

static void DB_stageSelect(DbSsTask *task)
{
    DbSsSlider *slider;
    char *name;

    if (!(task->flags & 1)) {
        task->flags |= 1;
        slider = TSLIDER_create(-1);
        slider->max_value = 2000;
        slider->mode |= 0x10;
        slider->x = 256.0f;
        slider->y = 224.0f;
        {
            int no = MapGetNo();
            task->slider = slider;
            slider->value = no;
        }
        task->mode = 0;
        task->entry = 0;
    }
    slider = task->slider;
    switch (task->mode) {
    case 0:
        if (slider->value >= 0) {
            name = MapGetName(slider->value);
            if (name != 0) {
                xglFontDebugPrintf(8, 0x20, name);
            }
        }
        if (slider->state == 2) {
            if (slider->value >= 0) {
                task->mode = 1;
                task->map = slider->value;
                slider = TSLIDER_create(-1);
                task->slider = slider;
                slider->x = 256.0f;
                slider->y = 224.0f;
                slider->mode |= 0x10;
                return;
            }
            {
                DbSsOwner *o = task->owner;
                o->flags &= ~1;
                o->state_0c = o->state_29f60;
            }
            TWSYS_setGRPStatus(0, 0);
            xglTaskRemove(&task->task);
            return;
        }
        break;
    case 1:
        xglFontDebugPrintf(8, 0x20, D_004C1E90);
        if (slider->state == 2) {
            int v = slider->value;
            if (v >= 0) {
                task->entry = v;
                LoadMap2(task->map, v);
            }
            {
                DbSsOwner *o = task->owner;
                o->flags &= ~1;
                o->state_0c = o->state_29f60;
            }
            TWSYS_setGRPStatus(0, 0);
            xglTaskRemove(&task->task);
            return;
        }
        break;
    }
}

extern int sprintf(char *buf, const char *fmt, ...);
extern unsigned int xheap_info(int *info);
extern void drawRect2(float m[4][4], int mode);
extern void drawBox2(float m[4][4], int mode);
extern int heapInfo[];
extern unsigned int heapUsed;
extern char D_004C1EA0[];

static void DB_memory(DbSsTask *task)
{
    char buf[0x100];
    float m[4][4];
    float scale[4];
    float trans[4];
    PadPrefix *pad = &PadData;
    float f;

    if (!(task->flags & 1)) {
        task->flags |= 1;
        heapUsed = xheap_info(heapInfo);
    }
    sprintf(buf, D_004C1EA0, heapUsed >> 10, 0x100);
    xglFontDebugPrintf(0x10, 0x40, buf);
    f = (float)heapUsed * 128.0f * 3.8146973e-06f;
    scale[1] = 24.0f;
    trans[0] = -64.0f;
    trans[1] = 64.0f;
    scale[0] = f;
    scale[2] = 1.0f;
    scale[3] = 1.0f;
    trans[3] = 1.0f;
    trans[2] = 0.0f;
    xglMatrixStackUnit();
    xglMatrixStackTrans(trans);
    xglMatrixStackScale(scale);
    xglMatrixStackSave(m);
    if (heapUsed < 0x2AAAA) {
        drawRect2(m, 1);
    } else {
        drawRect2(m, 0);
    }
    scale[0] = 132.0f;
    scale[1] = 28.0f;
    trans[1] = 52.0f;
    scale[2] = 1.0f;
    scale[3] = 1.0f;
    trans[3] = 1.0f;
    trans[0] = 0.0f;
    trans[2] = 0.0f;
    xglMatrixStackUnit();
    xglMatrixStackTrans(trans);
    xglMatrixStackScale(scale);
    xglMatrixStackSave(m);
    drawBox2(m, 6);
    if (pad->half_2a & 0x40) {
        DbSsOwner *o = task->owner;
        o->flags &= ~1;
        o->state_0c = o->state_29f60;
        TWSYS_setGRPStatus(0, 0);
        xglTaskRemove(&task->task);
    }
}

typedef struct {
    u8 pad_00[0x10];
    char path[0x10C];
    char selectedPath[0x100];
} DbLightSel;
typedef struct {
    u8 pad_00[0x2A];
    u16 half_2a;
    u16 half_2c;
} DbLightPad;
extern int xglCdFileSelect(void *sel);
extern int lightFileNum;
extern void *rcFile;
extern void RSRC_init(void *rc);
extern char *RSRC_loadFile2(void *rc, const char *dir, const char *name);
extern int *RSRC_getItem(void *rc, char *data);
extern void JS_exec(char *base, char *code);
extern void DB_reset(int x, int y);
extern void DB_println(const char *format, ...);
extern int sprintf(char *buf, const char *fmt, ...);
extern int sceOpen(const char *path, int flags, ...);
extern int sceClose(int fd);
extern void DB_lightWrite(int fd);
extern char D_004C1EB0[];
extern char D_004C1EC8[];
extern char D_004C1EE0[];
extern char D_004C1EF0[];

int DB_lightSave(DbLightSel *sel)
{
    DbLightPad *pad = (DbLightPad *)&PadData;
    char buf[512];
    int ret;
    int fd;
    char *data;

    switch (xglCdFileSelect(sel)) {
    case 1:
        RSRC_init(rcFile);
        data = RSRC_loadFile2(rcFile, sel->path, sel->selectedPath);
        JS_exec(data, data + *RSRC_getItem(rcFile, data));
        ret = 0;
        break;
    case 2:
        return 1;
    case 0:
    default:
        if (pad->half_2c & 0x8000) {
            lightFileNum--;
        }
        if (pad->half_2c & 0x2000) {
            lightFileNum++;
        }
        if (lightFileNum < 0) {
            lightFileNum = 99;
        }
        if (lightFileNum > 99) {
            lightFileNum = 0;
        }
        DB_reset(0x20, 0xA0);
        DB_println(D_004C1EB0);
        DB_println(D_004C1EC8);
        DB_println(D_004C1EE0, lightFileNum);
        if (pad->half_2a & 0x80) {
            sprintf(buf, D_004C1EF0, sel->path, sel->selectedPath, lightFileNum);
            fd = sceOpen(buf, 0x602);
            if (fd >= 0) {
                DB_lightWrite(fd);
                sceClose(fd);
            }
            ret = 1;
        } else {
            ret = 0;
        }
        break;
    }
    return ret;
}

extern void *fileSelect;
extern char D_004DA4A0[];
extern char D_004DA4A8[];
extern char *strrchr(const char *s, int c);
extern int strncmp(const char *a, const char *b, unsigned int n);
extern ScriptDbEntry *SCRIPT_exec(void);
extern void TWSYS_setGRPStatus(int group, int status);
typedef struct DbSfsOwner {
    u8 unmodeled_00[0xC];
    unsigned short state_0c;
    u8 unmodeled_0e[2];
    u32 flags;
    u8 unmodeled_14[0x29F60 - 0x14];
    unsigned short state_29f60;
} DbSfsOwner;
typedef struct DbSfsTask {
    XglTaskPrefix task;
    u32 flags;
    DbLightSel *sel;
    u8 unmodeled_18[4];
    DbSfsOwner *owner;
} DbSfsTask;
typedef struct {
    u8 pad_00[0xC];
    int callback;
} DbSfsSel;

static void DB_scriptFileSelect(DbSfsTask *task)
{
    DbLightSel *sel;
    DbSfsOwner *owner;
    char *path;
    char *ext;
    char *data;

    if (!(task->flags & 1)) {
        ((DbSfsSel *)fileSelect)->callback = 0;
        task->sel = fileSelect;
        task->flags |= 1;
    }
    sel = task->sel;
    switch (xglCdFileSelect(sel)) {
    case 1:
        path = sel->selectedPath;
        ext = strrchr(path, '.');
        if (ext != 0) {
            if (strncmp(ext, D_004DA4A0, 4) == 0) {
                SCRIPT_load_DBG(path);
                SCRIPT_exec();
            } else if (strncmp(ext, D_004DA4A8, 3) == 0) {
                RSRC_init(rcFile);
                data = RSRC_loadFile2(rcFile, sel->path, path);
                JS_exec(data, data + *RSRC_getItem(rcFile, data));
                {
                    DbSfsOwner *o = task->owner;
                    o->flags &= ~1;
                    o->state_0c = o->state_29f60;
                }
            }
        }
        task->owner->flags &= ~1;
        TWSYS_setGRPStatus(0, 0);
        xglTaskRemove(&task->task);
        return;
    case 2:
        owner = task->owner;
        owner->flags &= ~1;
        owner->state_0c = owner->state_29f60;
        TWSYS_setGRPStatus(0, 0);
        xglTaskRemove(&task->task);
        return;
    case 0:
        break;
    }
}

typedef unsigned int DbWindQuad __attribute__((mode(TI)));
extern char dbgWind[];
extern void EXM_GetWindPara(void *para);
extern void EXM_SetWindStruct(void *state);
extern void EXM_ClearWindStruct(void);
extern void EXM_SetWindPara(void *para);
extern void updateWind(void *wind);
extern void TWSYS_setGRPStatus(int group, int status);
typedef struct DbWindGameState {
    u8 unmodeled_00[0xC];
    unsigned short state_0c;
    u8 unmodeled_0e[2];
    u32 flags;
    u8 unmodeled_14[0x29F50 - 0x14];
    DbWindQuad quad_29f50;
    unsigned short state_29f60;
} DbWindGameState;
typedef struct DbWindTask {
    XglTaskPrefix task;
    u32 flags;
    u8 unmodeled_14[8];
    DbWindGameState *owner;
} DbWindTask;

static void DB_wind(DbWindTask *task)
{
    DbWindGameState *owner;
    PadPrefix *pad = (PadPrefix *)((u8 *)&PadData + 0x68);

    if (!(task->flags & 1)) {
        task->flags |= 1;
        EXM_GetWindPara(dbgWind);
        EXM_SetWindStruct(dbgWind);
        ((DbWindGameState *)GameLoopState)->flags &= ~1;
        ((DbWindGameState *)GameLoopState)->quad_29f50 = 0;
        ((DbWindGameState *)GameLoopState)->state_0c = ((DbWindGameState *)GameLoopState)->state_29f60;
    }
    updateWind(dbgWind);
    if (pad->half_2a & 0x40) {
        EXM_ClearWindStruct();
        EXM_SetWindPara(dbgWind);
        owner = task->owner;
        owner->flags &= ~1;
        owner->state_0c = owner->state_29f60;
        TWSYS_setGRPStatus(0, 0);
        xglTaskRemove(&task->task);
    }
}

typedef struct DbMenuView {
    u8 unmodeled_00[0x10];
    u32 flags;
    unsigned short state;
    unsigned short mode;
    u8 unmodeled_18[0x38];
    int value;
    signed char index;
    signed char cancel;
} DbMenuView;
typedef struct DbMenuTask {
    XglTaskPrefix task;
    u32 flags;
    DbMenuView *menu;
    u8 unmodeled_18[4];
    DbVmOwner *owner;
} DbMenuTask;
static void DB_light(XglTaskPrefix *task);
extern void initCursor();
extern void TMENU_addItem(DbMenuView *menu, char *text);
extern char D_004C1F00[];
static inline void dbSpawn(void *func)
{
    XglTaskScheduler *scheduler = GameLoopState[2];
    DbMenuTask *t = (DbMenuTask *)xglTaskEntryNext(
        scheduler, (int (*)(XglTaskPrefix *))func,
        scheduler != 0 ? scheduler->active_tail : 0);
    if (t != 0) {
        t->menu = 0;
        t->owner = (DbVmOwner *)GameLoopState;
        t->flags = 0;
    }
}

void SCRIPT_debugMenu(DbMenuTask *task)
{
    DbMenuView *menu = task->menu;

    if (!(task->flags & 1)) {
        TWSYS_setGRPStatus(0, 3);
        menu = (DbMenuView *)TMENU_create(-1);
        menu->value = 0;
        menu->mode |= 0x10;
        TMENU_addItem(menu, D_004C1F00);
        menu->flags &= ~0x40;
        task->menu = menu;
        task->flags |= 1;
    }
    if (menu->state != 0)
        return;
    menu->state = 2;
    switch (menu->index) {
    case 0:
        dbSpawn(DB_scriptFileSelect);
        break;
    case 1:
        dbSpawn(DB_stageSelect);
        break;
    case 2:
        dbSpawn(DB_vmSwitch);
        break;
    case 3:
        initCursor();
        task->owner->state_0c = 7;
        break;
    case 4:
        dbSpawn(DB_light);
        break;
    case 5: {
        XglTaskScheduler *scheduler = GameLoopState[2];
        DbMenuTask *t = (DbMenuTask *)xglTaskEntryNext(
            scheduler, (int (*)(XglTaskPrefix *))DB_memory,
            scheduler != 0 ? scheduler->active_tail : 0);
        if (t != 0) {
            t->flags = 0;
            t->owner = (DbVmOwner *)GameLoopState;
            t->menu = 0;
        }
        t->owner->state_29f60 = task->owner->state_29f60;
        break;
    }
    case 6:
        dbSpawn(DB_wind);
        break;
    default:
        if (menu->cancel < 0) {
            DbVmOwner *owner = task->owner;
            owner->flags &= ~1;
            owner->state_0c = owner->state_29f60;
            TWSYS_setGRPStatus(0, 0);
        }
        break;
    }
    xglTaskRemove(&task->task);
}

extern int DebugMenuResourceSizeAdd;
extern char D_004C1F60[];
extern char D_004DA4B0[];
extern char D_004DA4B8[];
extern char *MAP_getPath(void);
extern char *strcat(char *dst, const char *src);
extern int xglCdReadFile(const char *path, void *buf, int a, int b);
extern int JNI_searchClasses(void *data, const char *names, int n, int *out);
extern void ACT_initExMotion(void *actor, int n, void *data);
typedef struct {
    void *script;
    void *fpk;
    void *arc;
    int active;
    int pad_10[2];
    int count;
} ScriptDbLoadView;

void loadScriptCD(ScriptDbEntry *entry_, const char *path)
{
    ScriptDbLoadView *entry = (ScriptDbLoadView *)entry_;
    char buf[256];
    int count[1]; /* not `&x`: gcc 2.96 leaves an ADDRESSOF mem's alias set uninitialized (layout-dependent code) */
    int cls;
    int size;

    DebugMenuResourceSizeAdd = 0;
    strcpy(buf, MAP_getPath());
    strcat(buf, path);
    if (xglCdReadFile(buf, entry->script, 0, 1) < 0) {
        count[0] = 0;
        entry->count = 0;
        *(char **)&entry->active = 0;
        return;
    }
    {
    cls = JNI_searchClasses(entry->script, D_004C1F60, 2, count);
    replacePathExt(buf, D_004DA4B0);
    size = xglCdReadFile(buf, entry->fpk, 0, 1);
    if (size < 0) {
        ACT_initExMotion(0, 1, 0);
    } else {
        DebugMenuResourceSizeAdd += size;
        ACT_initExMotion(0, 1, entry->fpk);
    }
    replacePathExt(buf, D_004DA4B8);
    size = xglCdReadFile(buf, entry->arc, 0, 1);
    if (size >= 0) {
        DebugMenuResourceSizeAdd += size;
    }
    entry->active = cls;
    entry->count = count[0];
    }
}

extern int RES_loadFile(int command, int callback, int resource_id, int flags);
extern char *strrchr(const char *s, int c);
extern void *memcpy(void *d, const void *s, unsigned int n);
extern char D_004C1F78[] __attribute__((aligned(8)));

int loadScriptCD2(ScriptDbEntry *entry_, const char *path)
{
    ScriptDbLoadView *entry = (ScriptDbLoadView *)entry_;
    char buf[256];
    int count;
    int cls;
    int ret;
    char *p;

    DebugMenuResourceSizeAdd = 0;
    strcpy(buf, path);
    ret = xglCdReadFile(buf, entry->script, 0, 1);
    if (ret < 0) {
        count = 0;
        entry->count = 0;
        *(char **)&entry->active = 0;
        return ret;
    }
    cls = JNI_searchClasses(entry->script, D_004C1F60, 2, &count);
    p = strrchr(path, '\\');
    if (p == 0) {
        p = strrchr(path, '/');
    }
    __builtin_memcpy(buf, D_004C1F78, 9);
    if (p != 0) {
        strcat(buf, p + 1);
    }
    replacePathExt(buf, D_004DA4B0);
    entry->fpk = (void *)RES_loadFile(-1, 5, (int)buf, 0);
    ACT_initExMotion(0, 1, entry->fpk);
    replacePathExt(buf, D_004DA4B8);
    ret = RES_loadFile(-1, 5, (int)buf, 0);
    entry->active = cls;
    entry->arc = (void *)ret;
    entry->count = count;
    return ret;
}

extern int sceRead(int fd, void *buf, int n);
extern int sceLseek(int fd, int off, int whence);
typedef struct {
    int a;
    int b;
    int size;
    int len;
} ScriptFileHeader;

int loadScript(ScriptDbEntry *entry_, const char *base, const char *path)
{
    ScriptDbLoadView *entry = (ScriptDbLoadView *)entry_;
    char buf[256];
    ScriptFileHeader hdr;
    int count;
    int fd;
    int fd2;
    int size;
    int len;
    int cls;

    DebugMenuResourceSizeAdd = 0;
    if (base != 0) {
        strcpy(buf, base);
        strcat(buf, path);
        fd = sceOpen(buf, 1);
        if (fd < 0) {
            return 0;
        }
        sceRead(fd, &hdr, 12);
        __builtin_memcpy(entry->script, (char *)&hdr, 12);
        len = hdr.size - 12;
        if (len != sceRead(fd, (char *)entry->script + 12, len)) {
            return 0;
        }
        sceClose(fd);
        replacePathExt(buf, D_004DA4B0);
        fd2 = sceOpen(buf, 1);
        if (fd2 >= 0) {
            size = sceLseek(fd2, 0, 2);
            sceLseek(fd2, 0, 0);
            sceRead(fd2, entry->fpk, size);
            sceClose(fd2);
            ACT_initExMotion(0, 1, entry->fpk);
            DebugMenuResourceSizeAdd += size;
        } else {
            ACT_initExMotion(0, 1, 0);
        }
        replacePathExt(buf, D_004DA4B8);
        fd2 = sceOpen(buf, 1);
        if (fd2 >= 0) {
            int size2 = sceLseek(fd2, 0, 2);
            sceLseek(fd2, 0, 0);
            sceRead(fd2, entry->arc, size2);
            sceClose(fd2);
            DebugMenuResourceSizeAdd += size2;
        }
    }
    cls = JNI_searchClasses(entry->script, D_004C1F60, 2, &count);
    entry->active = cls;
    entry->count = count;
    return cls;
}

typedef struct ScriptEventDebugState {
    int mode;
    unsigned int flags;
    int selected;
} ScriptEventDebugState;
extern ScriptEventDebugState evtDebug;
typedef struct ScriptCallFrame {
    unsigned short program_counter;
    unsigned char unmodeled_02[2];
    SceneMethod *method;
    unsigned char unmodeled_08[0x18 - 8];
} ScriptCallFrame;
typedef struct EvtSceneThread {
    unsigned char unmodeled_00[16];
    SceneObject object;                  /* +0x10 */
    void (*entry)(SceneThread *thread);  /* +0x14 */
    SceneMethod *method;                 /* +0x18 */
    unsigned char unmodeled_1c[8];
    unsigned int flags;                  /* +0x24 */
    void *call_frame;                    /* +0x28 */
    ScriptCallFrame *frames;             /* +0x2c */
    unsigned char unmodeled_30[0x3e - 0x30];
    short frame_depth;                   /* +0x3e */
} EvtSceneThread;
typedef struct EvtSceneMethod {
    void *name;
} EvtSceneMethod;
typedef struct EvtConstString {
    void *next;
    unsigned short hash;
    unsigned short length;
    char *bytes;
} EvtConstString;
typedef struct EvtMapUnitSlot {
    u32 flags;                   /* +0x00 */
    unsigned char unmodeled_04[0xc];
    float position[4];           /* +0x10 */
    float rotation[4];           /* +0x20 */
    unsigned char unmodeled_30[0x70];
    unsigned char slot;          /* +0xA0 */
    unsigned char unmodeled_a1[3];
    s16 serial;                  /* +0xA4 */
    unsigned char unmodeled_a6[0x2a];
    void *sequence;              /* +0xD0 */
    unsigned char unmodeled_d4[0x300 - 0xd4];
} EvtMapUnitSlot;
typedef struct EvtUnitSequenceEntry {
    unsigned char unmodeled_00[0x04];
    int state;                   /* +0x04 */
    unsigned char unmodeled_08[0x260 - 0x08];
} EvtUnitSequenceEntry;
extern EvtMapUnitSlot MapUnit[64];
extern EvtUnitSequenceEntry unitSequence[64];
extern void DB_cmode(int mode);
extern void RES_GetMdlFileName(char *path, int resource_id);
extern char D_004C1F88[];
extern char D_004C1FA8[];
extern char D_004C1FC0[];
extern char D_004C1FD0[];
extern char D_004C1FF0[];
extern char D_004C2010[];
extern char D_004DA4C8[];
extern char D_004DA4D0[];
extern char D_004DA4D8[];
extern char D_004DA4E0[];
extern char D_004C1F98[];
extern char D_004DA4C0[];

int DB_evtMonitor(XglTaskPrefix *task_prefix)
{
    ScriptMenuTask *task = (ScriptMenuTask *)task_prefix;
    char model_path[256];
    ScriptEventDebugState *debug = &evtDebug;
    ScriptPadButtons *pad;
    EvtSceneThread *thread;
    int mode;
    int selected;

    DB_cmode(1);
    if (!(vmDebugMask & 0x10)) {
        debug->flags &= ~1;
        xglTaskRemove(&task->task);
        return;
    }
    if (!(task->state_flags & 1)) {
        if (debug->flags & 1) {
            xglTaskRemove(&task->task);
            return;
        }
        task->state_flags |= 1;
        debug->flags |= 1;
    }
    DB_reset(0, 0);
    DB_println(D_004C1F88);
    thread = (EvtSceneThread *)D_004DEDF0[currentScriptDB].thread;
    {
    EvtSceneThread *debug_thread = ((thread));
    if (debug_thread != 0 && debug_thread->object != 0) {
        {
        SceneClass *debug_class =
            ((SceneObjectHeader *)((debug_thread->object)))->class_ref->scene_class;
        SceneMethod *debug_method = ((debug_thread))->method;
        if ((((debug_thread))->flags & 0x10) && debug_method != 0) {
            DB_println(D_004DA4C0, debug_class->name->binary_name,
                       ((EvtConstString *)((EvtSceneMethod *)debug_method)->name)->bytes);
            if (((debug_thread))->frame_depth > 0) {
                int frame_index;
                for (frame_index = (((0)));
                     frame_index < ((debug_thread))->frame_depth;
                     ++frame_index) {
                    ScriptCallFrame *frame =
                        &((debug_thread))->frames[frame_index];
                    DB_println(D_004C1F98, frame->program_counter,
                               ((EvtConstString *)((EvtSceneMethod *)frame->method)->name)->bytes);
                }
            } else {
                DB_println(D_004DA4C0, debug_class->name->binary_name,
                           ((EvtConstString *)((EvtSceneMethod *)debug_method)->name)->bytes);
            }
        }
};
    }
};

    mode = debug->mode;
    switch (mode) {
    case 0: {
        ScriptActorRecord *actor_record;
        const char *base_name;
        float half_turn;
        SceneObject object;

        pad = (ScriptPadButtons *)&PadData;
        if (pad->pressed & 2)
            debug->mode = 1;
        if (pad->repeat & 0x8000)
            --debug->selected;
        if (pad->repeat & 0x2000)
            ++debug->selected;
        if (debug->selected < 0)
            debug->selected = 63;
        if (debug->selected >= 64)
            debug->selected = 0;
        selected = debug->selected;
        actor_record = &actor[selected];
        RES_GetMdlFileName(model_path, actor_record->in_use_id);
        base_name = strrchr(model_path, '\\');
        if (base_name == 0)
            base_name = D_004DA4C8;
        ++base_name;
        DB_reset(0, 10);
        DB_println(D_004C1FA8, debug->selected, actor_record->in_use_id,
                   base_name);
        half_turn = 3.1415927f;
        DB_println(D_004C1FC0, actor_record->flags);
        DB_println(D_004C1FD0, actor_record->position[0],
                   actor_record->position[1], actor_record->position[2]);
        DB_println(D_004C1FF0,
                   actor_record->rotation[0] / half_turn * 180.0f,
                   actor_record->rotation[1] / half_turn * 180.0f,
                   actor_record->rotation[2] / half_turn * 180.0f);
        object = actor_record->script_object;
        if (object != 0) {
            thread = (EvtSceneThread *)JTHREAD_get(object);
            if (thread != 0) {
                object = thread->object;
                if (object != 0) {
                    {
        SceneClass *debug_class =
            ((SceneObjectHeader *)((object)))->class_ref->scene_class;
        SceneMethod *debug_method = ((thread))->method;
        if ((((thread))->flags & 0x10) && debug_method != 0) {
            DB_println(D_004DA4C0, debug_class->name->binary_name,
                       ((EvtConstString *)((EvtSceneMethod *)debug_method)->name)->bytes);
            if (((thread))->frame_depth > 0) {
                int frame_index;
                for (frame_index = ((mode));
                     frame_index < ((thread))->frame_depth;
                     ++frame_index) {
                    ScriptCallFrame *frame =
                        &((thread))->frames[frame_index];
                    DB_println(D_004C1F98, frame->program_counter,
                               ((EvtConstString *)((EvtSceneMethod *)frame->method)->name)->bytes);
                }
            } else {
                DB_println(D_004DA4C0, debug_class->name->binary_name,
                           ((EvtConstString *)((EvtSceneMethod *)debug_method)->name)->bytes);
            }
        }
};
                }
            }
        }
        break;
    }
    default: {
        EvtMapUnitSlot *unit;
        EvtUnitSequenceEntry *sequence;
        SceneObject object;
        float half_turn;

        pad = (ScriptPadButtons *)&PadData;
        if (pad->pressed & 2)
            debug->mode = (mode + 1) & 1;
        if (pad->repeat & 0x8000)
            --debug->selected;
        if (pad->repeat & 0x2000)
            ++debug->selected;
        if (debug->selected < 0)
            debug->selected = 63;
        if (debug->selected >= 64)
            debug->selected = 0;
        selected = debug->selected;
        unit = &MapUnit[selected];
        DB_reset(0, 10);
        sequence = &unitSequence[unit->slot];
        DB_println(D_004C2010, debug->selected, unit->serial);
        DB_println(D_004C1FC0, unit->flags);
        DB_println(D_004DA4D0);
        if (sequence->state & 1)
            DB_println(D_004DA4D8);
        else
            DB_println(D_004DA4E0);
        half_turn = 3.1415927f;
        DB_println(D_004C1FD0, unit->position[0], unit->position[1], unit->position[2]);
        DB_println(D_004C1FF0, unit->rotation[0] / half_turn * 180.0f,
                   unit->rotation[1] / half_turn * 180.0f,
                   unit->rotation[2] / half_turn * 180.0f);
        object = unit->sequence;
        if (object != 0) {
            thread = (EvtSceneThread *)JTHREAD_get(object);
            if (thread != 0) {
                object = thread->object;
                if (object != 0) {
                    {
        SceneClass *debug_class =
            ((SceneObjectHeader *)((object)))->class_ref->scene_class;
        SceneMethod *debug_method = ((thread))->method;
        if ((((thread))->flags & 0x10) && debug_method != 0) {
            DB_println(D_004DA4C0, debug_class->name->binary_name,
                       ((EvtConstString *)((EvtSceneMethod *)debug_method)->name)->bytes);
            if (((thread))->frame_depth > 0) {
                int frame_index;
                for (frame_index = ((0));
                     frame_index < ((thread))->frame_depth;
                     ++frame_index) {
                    ScriptCallFrame *frame =
                        &((thread))->frames[frame_index];
                    DB_println(D_004C1F98, frame->program_counter,
                               ((EvtConstString *)((EvtSceneMethod *)frame->method)->name)->bytes);
                }
            } else {
                DB_println(D_004DA4C0, debug_class->name->binary_name,
                           ((EvtConstString *)((EvtSceneMethod *)debug_method)->name)->bytes);
            }
        }
};
                }
            }
        }
        break;
    }
    }
    DB_cmode(0);
}

INCLUDE_ASM("asm/main/nonmatchings/script", SCRIPT_exec);

typedef struct ScriptDbEntrySX {
    void *class_image;   /* +0x0: class database image read from the script file */
    void *motion_data;   /* +0x4: extra-motion pack read next to the script file (.fpk) */
    void *pdb;            /* +0x8: event data used for resource lookup */
    int active;          /* +0xC: nonzero when this script slot is bound (zero test only) */
    SceneThread *thread; /* +0x10: the script's VM thread; its own object is at +0x10 */
    int class_slot;      /* +0x14: class database slot the image is loaded into */
    int skip_count;      /* +0x18: class-name skip count JNI_searchClasses reported */
} ScriptDbEntrySX;
#define scriptDBSX ((ScriptDbEntrySX *)scriptDB)
typedef struct ScriptGameLoopStateSX {
    unsigned char unmodeled_00[4];
    void *main_actor;
    XglTaskScheduler *task_scheduler;
    unsigned short kind;
    unsigned short kindDetail;
    unsigned int flags;
    unsigned char unmodeled_14[0x90 - 0x14];
    float fade_out_red;
    float fade_out_green;
    float fade_out_blue;
    float fade_out_frames;
    float fade_red;                    /* +0xa0: fade-in colour */
    float fade_green;
    float fade_blue;
    float fade_frames;                 /* +0xac: fade-in length in frames */
} ScriptGameLoopStateSX;
#define GameLoopStateSX (*(ScriptGameLoopStateSX *)&GameLoopState)
typedef struct GameLoopMovieStateSX {
    u8 unmodeled_00[0xC];
    unsigned short movie_state;
} GameLoopMovieStateSX;
extern int JNI_loadClassDB(int class_id, int value);
extern void TCAMERA_init(void);
extern void XTK_setResourceID(int id);
extern void *PLAY_getCurrent(void);
extern void PLAY_setupDefault(void *play);
ScriptDbEntrySX *SCRIPT_exec2(void)
{
    ScriptDbEntrySX *entry;
    SceneObject object;
    SceneThread *thread;
    ScriptSceneTask *task;
    XglTaskScheduler *scheduler;
    StudioCamera *camera;
    StudioCamera *target;
    int camera_index;
    SceneThread *vm;
    int (*callback)(XglTaskPrefix *);

    XTK_setWindowOwner(0);
    currentScriptDB ^= 1;
    entry = &scriptDBSX[(currentScriptDB + 1) & 1];
    JNI_loadClassDB(entry->class_slot, 0);
    entry = &scriptDBSX[currentScriptDB];
    PLAY_setupDefault(PLAY_getCurrent());
    if (entry->active == 0) {
        s_nScriptChangeTime = 0x19;
        JNI_popFrame();
        XTK_setResourceID(1);
        GameLoopStateSX.kind = 2;
        nmlModelSetGlobalPointLightReset();
        s_nScriptCfTime = 0;
        s_nScriptTalkLock = 0;
        TCAMERA_init();
        TWSYS_init();
        MSG_init();
        entry->thread = 0;
        JNI_loadClassDB(entry->class_slot, 0);
        JNI_pushFrame();
        JNI_loadNativeClass();
        return entry;
    }
    if (GameLoopStateSX.flags & 0x800) {
        s_nScriptTalkLock = 0;
        TCAMERA_init();
        TWSYS_init();
        MSG_init();
    }
    if (entry->skip_count != 0) {
        if (entry->skip_count == 1) {
            camera_index = 0;
            JNI_popFrame();
            XTK_setResourceID(1);
            GameLoopStateSX.kind = 2;
            nmlModelSetGlobalPointLightReset();
            s_nScriptCfTime = 0;
            do {
                xglStudioGetCamera(&camera, camera_index);
                target = camera;
                if (camera_index == 0) {
                    target->state = 4;
                    target->active = 1;
                } else {
                    target->active = 0;
                }
                camera_index++;
            } while (camera_index < 8);
            s_nScriptTalkLock = 0;
            TCAMERA_init();
            TWSYS_init();
            MSG_init();
            s_nScriptChangeTime = 0x19;
            JNI_pushFrame();
            JNI_loadNativeClass();
            JNI_loadClassDB(entry->class_slot, entry->class_image);
            object = newObject(
                loadClass(loadConstString((const char *)entry->active, -1), 0));
            thread = JNI_createThread(2, 6, 0x46);
            entry->thread = thread;
            thread->object = object;
            STAGE_instance(defaultVM, object);
        }
        return entry;
    }
    XTK_setResourceID(2);
    GameLoopStateSX.kind = 3;
    GameLoopStateSX.flags |= 0x800;
    s_nScriptEventActive = 1;
    nmlModelSetGlobalPointLightReset();
    nmlModelSetFadeIn((int)GameLoopStateSX.fade_frames, 0);
    FX_ScreenMask(0,
                  SCRIPT_FADE_COLOR(GameLoopStateSX.fade_red,
                                    GameLoopStateSX.fade_green,
                                    GameLoopStateSX.fade_blue),
                  (int)GameLoopStateSX.fade_frames, 3);
    s_nScriptChangeTime = 0x19;
    s_nScriptTalkLock = 0;
    TCAMERA_init();
    TWSYS_init();
    MSG_init();
    JNI_popFrame();
    JNI_pushFrame();
    JNI_loadNativeClass();
    JNI_loadClassDB(entry->class_slot, entry->class_image);
    object = newObject(
        loadClass(loadConstString((const char *)entry->active, -1), 0));
    thread = JNI_createThread(2, 6, 0x46);
    callback = (int (*)(XglTaskPrefix *))sceneObserver2;
    scheduler = GameLoopStateSX.task_scheduler;
    thread->object = object;
    entry->thread = thread;
    task = (ScriptSceneTask *)xglTaskEntryNext(
        scheduler, callback,
        scheduler != 0 ? scheduler->active_tail : 0);
    if (task != 0) {
        task->state_flags = 0;
        task->owner = &GameLoopStateSX;
        task->thread = 0;
    }
    vm = defaultVM;
    task->thread = (SceneObject)thread;
    SCENE_instance(vm, object);
    SCENE_start(defaultVM, object, 8, D_004DA490);
    return entry;
}

void XTK_setResourceID(int id)
{
    resourceID = id;
}

int XTK_getResourceID(void)
{
    return resourceID;
}

void XTK_setWindowOwner(int owner)
{
    windowOwner = owner;
}

int XTK_getWindowOwner(void)
{
    return windowOwner;
}

int XTK_findFile(const char *path)
{
    ScriptPdbFile *file;

    file = PDB_findFile(D_004DEDE8[currentScriptDB].pdb, path);
    if (file != 0)
        return file->data;
    return 0;
}

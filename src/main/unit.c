#include "common.h"
#include "shared.h"
#include "unit.h"

/*
 * The Java VM's per-thread execution context, already recovered as
 * `JThread` in src/main/chr.h (main's chr TU). This TU forwards a pointer
 * to it without touching any member, so only the tag is declared here.
 */
typedef struct JThread JThread;

/*
 * UNIT_rotY/UNIT_rotZ/UNIT_sclX/UNIT_sclY/UNIT_sclZ are this TU's own
 * INCLUDE_ASM functions (0x00301d58, 0x003020a8, 0x00302ef8, 0x00303168,
 * 0x003033d8). Their JNI trampolines below insert the interpolation-mode
 * literal as a new first argument and forward `thread`/`arguments`/
 * `failure_result` unchanged -- the same shape as Java_xeno_Chr_sclX__FFZ /
 * CHR_sclX in src/main/chr.c, which names the same three parameters.
 */
static void UNIT_rotY(int mode, JThread *thread, void *arguments,
               u32 *failure_result);
static void UNIT_rotZ(int mode, JThread *thread, void *arguments,
               u32 *failure_result);
static void UNIT_sclX(int mode, JThread *thread, void *arguments,
               u32 *failure_result);
static void UNIT_sclY(int mode, JThread *thread, void *arguments,
               u32 *failure_result);
static void UNIT_sclZ(int mode, JThread *thread, void *arguments,
               u32 *failure_result);

/*
 * UNIT_moveXZ/UNIT_rotX/UNIT_motion are this TU's own INCLUDE_ASM functions
 * (0x00301710, 0x00301a38, 0x00302a10). Their JNI trampolines below are the
 * same shape as UNIT_rotY/UNIT_rotZ/UNIT_sclX/UNIT_sclY/UNIT_sclZ above:
 * insert the mode literal ahead of `thread`/`arguments`/`failure_result` and
 * forward those three unchanged.
 */
static void UNIT_moveXZ(int mode, JThread *thread, void *arguments,
                 u32 *failure_result);
static void UNIT_rotX(int mode, JThread *thread, void *arguments,
               u32 *failure_result);
void UNIT_motion(int mode, JThread *thread, void *arguments,
                 u32 *failure_result);

extern const char D_004D16F8[];
extern void MAP_updateUnitPartsSequence(void *);
extern void MAP_updateUnitPartsSequence2(void *);
extern void MAP_updateUnitSequence(void *);
typedef unsigned int GameLoopWords[];
extern GameLoopWords GameLoopState;
typedef struct UnitUpdateState {
    u32 flags;                                       /* +0x00 */
    void (*update)(void *);                          /* +0x04 */
    unsigned char unmodeled_08[0xd4 - 0x08];
    u32 field_d4;                                    /* +0xd4 */
} UnitUpdateState;

static void UNIT_setUpdate(u8 *object, UnitUpdateState *state)
{
    JavaField *algorithm_field;
    void (*update)(void *);
    u32 flags;
    u32 loop;
    int mode;

    algorithm_field = lookupClassField(classJava_xeno_Unit,
                                       loadConstString(D_004D16F8, -1), 0);
    mode = (*(int *)(object + algorithm_field->offset) >> 8) & 0xF;

    switch (mode) {
    case 0:
        update = MAP_updateUnitSequence;
        goto set_update;
    case 1:
        state->field_d4 = 0;
        update = MAP_updateUnitPartsSequence;
        goto set_update;
    case 2:
        flags = state->flags;
        loop = GameLoopState[1];
        state->flags = flags | 4;
        state->field_d4 = loop;
        state->update = MAP_updateUnitPartsSequence;
        break;
    case 3:
        state->field_d4 = 0;
        update = MAP_updateUnitPartsSequence2;
    set_update:
        state->update = update;
        break;
    }
}

/* The "peer" field name every lookupClassField(classJava_xeno_Unit, ...)
 * call below looks up. */
extern const char D_004DC1D0[];

extern void *classJava_xeno_Chr;

extern int SEQ_rotYCNSUnitChr();
typedef struct UnitRotYCNSCall {
    u8 *object;
    u8 *chr;
    int frames;
    float x;
    float y;
    float z;
} UnitRotYCNSCall;
typedef union UnitRotYCNSWord {
    int i;
    float f;
    void *p;
} UnitRotYCNSWord;

void Java_xeno_Unit_rotYCNS__Ljava_lang_Object_IFFF(JThread *thread, UnitRotYCNSCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    UnitSequenceEntry *entry;
    UnitRotYCNSWord *args;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    UNIT_setUpdate(object, (UnitUpdateState *)peer);
    entry = &unitSequence[peer->serial];
    args = (UnitRotYCNSWord *)((u8 *)entry + 0xb8);
    *(void **)((u8 *)entry + 0x28) = SEQ_rotYCNSUnitChr;
    entry->state |= 4;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(D_004DC1D0, -1), 0);
    args[0].p = *(void **)(arguments->chr + peer_field->offset);
    args[5].i = arguments->frames;
    args[12].f = arguments->x;
    args[13].f = arguments->y;
    args[14].f = arguments->z;
}

extern int SEQ_transCNSUnitChr();

void Java_xeno_Unit_transCNS__Ljava_lang_Object_IFFF(JThread *thread, UnitRotYCNSCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    UnitSequenceEntry *entry;
    UnitRotYCNSWord *args;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    UNIT_setUpdate(object, (UnitUpdateState *)peer);
    entry = &unitSequence[peer->serial];
    args = (UnitRotYCNSWord *)((u8 *)entry + 0x38);
    *(void **)((u8 *)entry + 0x24) = SEQ_transCNSUnitChr;
    entry->state |= 1;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(D_004DC1D0, -1), 0);
    args[0].p = *(void **)(arguments->chr + peer_field->offset);
    args[16].i = arguments->frames;
    args[4].f = arguments->x;
    args[5].f = arguments->y;
    args[6].f = arguments->z;
}

INCLUDE_ASM("asm/main/nonmatchings/unit", UNIT_moveXZ);

INCLUDE_ASM("asm/main/nonmatchings/unit", UNIT_rotX);

INCLUDE_ASM("asm/main/nonmatchings/unit", UNIT_rotY);

INCLUDE_ASM("asm/main/nonmatchings/unit", UNIT_rotZ);

extern const char D_004DC1D8[];
extern const char D_004DC1E0[];
extern const char D_004DC1E8[];

void Java_xeno_Unit_getRotate__(JThread *thread, UnitObjectCall *arguments)
{
    u8 *object;
    JavaField *field;
    float *rot;

    object = arguments->object;
    field = lookupClassField(classJava_xeno_Unit,
                             loadConstString(D_004DC1D0, -1), 0);
    rot = (float *)(*(u8 **)(object + field->offset) + 0x20);
    field = lookupClassField(classJava_xeno_Unit,
                             loadConstString(D_004DC1D8, -1), 0);
    *(float *)(object + field->offset) = rot[0] / 3.1415927f * 180.0f;
    field = lookupClassField(classJava_xeno_Unit,
                             loadConstString(D_004DC1E0, -1), 0);
    *(float *)(object + field->offset) = rot[1] / 3.1415927f * 180.0f;
    field = lookupClassField(classJava_xeno_Unit,
                             loadConstString(D_004DC1E8, -1), 0);
    *(float *)(object + field->offset) = rot[2] / 3.1415927f * 180.0f;
}

void Java_xeno_Unit_getSignal__(JThread *thread, UnitObjectCall *arguments,
                                u32 *failure_result)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;

    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Unit) == 0) {
        *failure_result = 0;
        return;
    }
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    *failure_result = peer->signal;
}

extern const char D_004DC1F0[];
extern const char D_004DC1F8[];
extern const char D_004DC200[];

void Java_xeno_Unit_getTranslate__(JThread *thread, UnitObjectCall *arguments)
{
    JavaField *field;
    u8 *object;
    float *pos;

    object = arguments->object;
    field = lookupClassField(classJava_xeno_Unit,
                             loadConstString(D_004DC1D0, -1), 0);
    pos = (float *)(*(u8 **)(object + field->offset) + 0x10);
    field = lookupClassField(classJava_xeno_Unit,
                             loadConstString(D_004DC1F0, -1), 0);
    *(float *)(object + field->offset) = pos[0];
    field = lookupClassField(classJava_xeno_Unit,
                             loadConstString(D_004DC1F8, -1), 0);
    *(float *)(object + field->offset) = pos[1];
    field = lookupClassField(classJava_xeno_Unit,
                             loadConstString(D_004DC200, -1), 0);
    *(float *)(object + field->offset) = pos[2];
}

/*
 * An empty native (`jr $31; nop`): the script VM still calls it with the
 * native signature every Java_xeno_* entry receives, but it reads nothing.
 */
void Java_xeno_Unit_invalidate__(JThread *thread, void *arguments,
                                 u32 *failure_result)
{
}

/*
 * Interpolation mode one (a float value): forward thread/arguments/
 * failure_result unchanged and insert mode=1 ahead of them.
 */
void Java_xeno_Unit_move__FFFZ(JThread *thread, void *arguments,
                               u32 *failure_result)
{
    UNIT_moveXZ(1, thread, arguments, failure_result);
}

/* Interpolation mode zero (an int value), otherwise identical. */
void Java_xeno_Unit_move__IFFZ(JThread *thread, void *arguments,
                               u32 *failure_result)
{
    UNIT_moveXZ(0, thread, arguments, failure_result);
}

INCLUDE_ASM("asm/main/nonmatchings/unit", Java_xeno_Unit_move__Lxeno_util_Spline_IZ);

INCLUDE_ASM("asm/main/nonmatchings/unit", Java_xeno_Unit_rotate__Lxeno_util_Spline_IZ);

/*
 * The Object-argument overloads of move are empty natives: they take the
 * native signature but read none of it.
 */
void Java_xeno_Unit_move__ILjava_lang_Object_Z(JThread *thread,
                                               void *arguments,
                                               u32 *failure_result)
{
}

void Java_xeno_Unit_move__Ljava_lang_Object_FZ(JThread *thread,
                                               void *arguments,
                                               u32 *failure_result)
{
}

INCLUDE_ASM("asm/main/nonmatchings/unit", UNIT_motion);

/*
 * Motion mode zero (the short, two-int overload): forward thread/arguments/
 * failure_result unchanged and insert mode=0 ahead of them.
 */
void Java_xeno_Unit_mtn__IIFZ(JThread *thread, void *arguments,
                              u32 *failure_result)
{
    UNIT_motion(0, thread, arguments, failure_result);
}

/* Motion mode one (the long, five-int overload), otherwise identical. */
void Java_xeno_Unit_mtn__IIIIIFZ(JThread *thread, void *arguments,
                                 u32 *failure_result)
{
    UNIT_motion(1, thread, arguments, failure_result);
}

/*
 * Interpolation mode one (a float value): forward thread/arguments/
 * failure_result unchanged and insert mode=1 ahead of them.
 */
void Java_xeno_Unit_rotX__FFZ(JThread *thread, void *arguments,
                              u32 *failure_result)
{
    UNIT_rotX(1, thread, arguments, failure_result);
}

/* Interpolation mode zero (an int value), otherwise identical. */
void Java_xeno_Unit_rotX__IFZ(JThread *thread, void *arguments,
                              u32 *failure_result)
{
    UNIT_rotX(0, thread, arguments, failure_result);
}

/*
 * The Object-argument overloads of rotX are empty natives: they take the
 * native signature but read none of it.
 */
void Java_xeno_Unit_rotX__ILjava_lang_Object_Z(JThread *thread,
                                               void *arguments,
                                               u32 *failure_result)
{
}

void Java_xeno_Unit_rotX__Ljava_lang_Object_FZ(JThread *thread,
                                               void *arguments,
                                               u32 *failure_result)
{
}

/*
 * Interpolation mode one (a float value): forward thread/arguments/
 * failure_result unchanged and insert mode=1 ahead of them.
 */
void Java_xeno_Unit_rotY__FFZ(JThread *thread, void *arguments,
                              u32 *failure_result)
{
    UNIT_rotY(1, thread, arguments, failure_result);
}

/* Interpolation mode zero (an int value), otherwise identical. */
void Java_xeno_Unit_rotY__IFZ(JThread *thread, void *arguments,
                              u32 *failure_result)
{
    UNIT_rotY(0, thread, arguments, failure_result);
}

/*
 * The Object-argument overloads of rotY and rotZ are empty natives, like
 * rotX's above: they take the native signature but read none of it.
 */
void Java_xeno_Unit_rotY__ILjava_lang_Object_Z(JThread *thread,
                                               void *arguments,
                                               u32 *failure_result)
{
}

void Java_xeno_Unit_rotY__Ljava_lang_Object_FZ(JThread *thread,
                                               void *arguments,
                                               u32 *failure_result)
{
}

void Java_xeno_Unit_rotZ__FFZ(JThread *thread, void *arguments,
                              u32 *failure_result)
{
    UNIT_rotZ(1, thread, arguments, failure_result);
}

void Java_xeno_Unit_rotZ__IFZ(JThread *thread, void *arguments,
                              u32 *failure_result)
{
    UNIT_rotZ(0, thread, arguments, failure_result);
}

void Java_xeno_Unit_rotZ__ILjava_lang_Object_Z(JThread *thread,
                                               void *arguments,
                                               u32 *failure_result)
{
}

void Java_xeno_Unit_rotZ__Ljava_lang_Object_FZ(JThread *thread,
                                               void *arguments,
                                               u32 *failure_result)
{
}

extern int SEQ_scaleUnitSPL();
typedef struct UnitScaleSplCall {
    u8 *object;
    u8 *spline;
    int pad;
    u8 wait;
} UnitScaleSplCall;
typedef struct UnitScaleThread {
    u8 pad00[0xc];
    u8 state;
    u8 next;
    u8 pad0e[0x24 - 0xe];
    u32 flags;
    u8 pad28[0x30 - 0x28];
    void *wait_object;
    u8 pad34[0x3c - 0x34];
    u16 pc;
    u16 pc2;
} UnitScaleThread;
static inline void unit_scale_wait(UnitScaleThread *t, void *peer)
{
    if (t->state != 5 && !(t->flags & 0x40))
        return;
    if (t->flags & 0x40) {
        t->wait_object = peer;
        t->next = 6;
        t->pc = t->pc2;
        t->flags |= 4;
    }
    t->pc = t->pc2;
    t->flags |= 1;
}

void Java_xeno_Unit_scale__Lxeno_util_Spline_IZ(JThread *thread, UnitScaleSplCall *arguments,
                                                u32 *failure_result)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    UnitSequenceEntry *entry;
    u8 *spline;
    UnitScaleThread *t;
    u8 *sargs;
    int nf;
    u32 flags;
    int wait;
    u16 v;

    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Unit) == 0) {
        *failure_result = 0;
        return;
    }
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    entry = &unitSequence[peer->serial];
    sargs = (u8 *)entry + 0x1b8;
    entry->state |= 0xE0;
    lookupClassField(classJava_xeno_Unit, loadConstString(D_004D16F8, -1), 0);
    spline = arguments->spline;
    wait = arguments->wait;
    *(void **)((u8 *)entry + 0x30) = SEQ_scaleUnitSPL;
    *(u8 **)sargs = spline;
    *(u16 *)(sargs + 6) = *(u16 *)(spline + 8);
    if (wait == 1) {
        *(u32 *)peer |= 2;
        unit_scale_wait((UnitScaleThread *)thread, peer);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/unit", UNIT_sclX);

void Java_xeno_Unit_sclX__FFZ(JThread *thread, void *arguments,
                              u32 *failure_result)
{
    UNIT_sclX(1, thread, arguments, failure_result);
}

void Java_xeno_Unit_sclX__IFZ(JThread *thread, void *arguments,
                              u32 *failure_result)
{
    UNIT_sclX(0, thread, arguments, failure_result);
}

INCLUDE_ASM("asm/main/nonmatchings/unit", UNIT_sclY);

void Java_xeno_Unit_sclY__FFZ(JThread *thread, void *arguments,
                              u32 *failure_result)
{
    UNIT_sclY(1, thread, arguments, failure_result);
}

void Java_xeno_Unit_sclY__IFZ(JThread *thread, void *arguments,
                              u32 *failure_result)
{
    UNIT_sclY(0, thread, arguments, failure_result);
}

INCLUDE_ASM("asm/main/nonmatchings/unit", UNIT_sclZ);

void Java_xeno_Unit_sclZ__FFZ(JThread *thread, void *arguments,
                              u32 *failure_result)
{
    UNIT_sclZ(1, thread, arguments, failure_result);
}

void Java_xeno_Unit_sclZ__IFZ(JThread *thread, void *arguments,
                              u32 *failure_result)
{
    UNIT_sclZ(0, thread, arguments, failure_result);
}

/*
 * Unconditional: unlike getSignal__/signal__I above, this native never
 * checks JNI_isInstanceOf before dereferencing the peer field.
 */
void Java_xeno_Unit_setCollision__Z(JThread *thread, UnitBoolCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    if (arguments->flag != 0) {
        peer->flags |= 0x100;
    } else {
        peer->flags &= ~0x100;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/unit", Java_xeno_Unit_setRotate__);

void Java_xeno_Unit_setTranslate__(JThread *thread, UnitObjectCall *arguments)
{
    JavaField *field;
    u8 *object;
    float *pos;

    object = arguments->object;
    field = lookupClassField(classJava_xeno_Unit,
                             loadConstString(D_004DC1D0, -1), 0);
    pos = (float *)(*(u8 **)(object + field->offset) + 0x10);
    field = lookupClassField(classJava_xeno_Unit,
                             loadConstString(D_004DC1F0, -1), 0);
    pos[0] = *(float *)(object + field->offset);
    field = lookupClassField(classJava_xeno_Unit,
                             loadConstString(D_004DC1F8, -1), 0);
    pos[1] = *(float *)(object + field->offset);
    field = lookupClassField(classJava_xeno_Unit,
                             loadConstString(D_004DC200, -1), 0);
    pos[2] = *(float *)(object + field->offset);
}

void Java_xeno_Unit_setVisible__IZ(JThread *thread, UnitVisibleCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    MDL_setVisible(peer->model, arguments->mode, arguments->visible);
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_setVisible__Z(JThread *thread, UnitBoolCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    if (arguments->flag != 0) {
        peer->flags &= ~4;
    } else {
        peer->flags |= 4;
    }
}

void Java_xeno_Unit_signal__I(JThread *thread, UnitSignalCall *arguments,
                              u32 *failure_result)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;

    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Unit) == 0) {
        *failure_result = 0;
        return;
    }
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    peer->signal = arguments->value;
}

INCLUDE_ASM("asm/main/nonmatchings/unit", Java_xeno_Unit_start__ILjava_lang_Object_);

INCLUDE_ASM("asm/main/nonmatchings/unit", Java_xeno_Unit_stop__);

void Java_xeno_Unit_validate__(void)
{
}

extern int ACT_jointGetAccessories(void *actor, unsigned int id);
typedef struct UnitSetParentCall {
    u8 *object;
    u8 *chr;
    int rel;
} UnitSetParentCall;

void Java_xeno_Unit_setParent__Ljava_lang_Object_I(JThread *thread, UnitSetParentCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    u8 *chr;
    int rel;
    u32 *peer;
    void *parent;

    object = arguments->object;
    chr = arguments->chr;
    rel = arguments->rel;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(u32 **)(object + peer_field->offset);
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(D_004DC1D0, -1), 0);
    parent = *(void **)(chr + peer_field->offset);
    peer[0xFC / 4] = (u32)parent;
    if (rel & 0x8000) {
        peer[0xF8 / 4] = ACT_jointGetAccessories(parent, rel & 0x7FFF) | 0x8000;
    } else {
        peer[0xF8 / 4] = rel;
    }
    UNIT_setUpdate(object, (UnitUpdateState *)peer);
}

static void copyArgs(u8 *dst, u8 *src, int count)
{
    u8 byte;

    if (count > 0) {
        count--;
        if (count >= 0) {
            do {
                byte = *src;
                src++;
                count--;
                *dst = byte;
                dst++;
            } while (count >= 0);
        }
    }
}

/*
 * The three-int overload of setArgs: unlike setArgs__ILjava_lang_Object_I
 * above, the bytes to copy come straight from the Java int argument (at
 * +0x8 of the call block) instead of an Object's data, and the destination
 * offset within the peer's argument buffer is itself a caller-supplied
 * argument (+0x4), not always the start.
 */
void Java_xeno_Unit_setArgs__III(JThread *thread, UnitArgsWordCall *arguments)
{
    JavaField *peer_field;
    UnitPeer *peer;
    int value;
    u8 *object;
    int offset;
    int size;

    value = arguments->value;
    object = arguments->object;
    offset = arguments->offset;
    size = arguments->size;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    if ((unsigned int)(size - 1) < 4U) {
        copyArgs(peer->args + offset, (u8 *)&value, size);
    }
}

void Java_xeno_Unit_getArgs__II(JThread *thread, UnitArgsGetCall *arguments,
                                u32 *failure_result)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    int size;
    int offset;
    unsigned int check;
    int result;

    size = arguments->size;
    object = arguments->object;
    offset = arguments->offset;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    check = (unsigned int)(size - 1);
    if (check < 4U) {
        copyArgs((u8 *)&result, peer->args + offset, size);
        *failure_result = (u32)result;
    }
}

/*
 * The Object overload of setArgs: it always writes to the start of the
 * peer's argument buffer, taking `size` bytes from the Object argument's
 * data.
 */
void Java_xeno_Unit_setArgs__ILjava_lang_Object_I(JThread *thread,
                                                  UnitArgsSetCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    UnitArgsSource *source;
    int size;

    object = arguments->object;
    source = arguments->source;
    size = arguments->size;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    if (size > 0) {
        copyArgs(peer->args, source->data, size);
    }
}

typedef struct { u8 *object; int index; int a; int b; int c; int d; } UnitArgs5Call;
static inline void unitPutArg(UnitPeer *p, int off, int v)
{
    *(int *)(p->args + off) = v;
}

void Java_xeno_Unit_setArgs__IIIII(JThread *thread, UnitArgs5Call *arguments)
{
    JavaField *peer_field;
    u8 *object;
    int index;
    UnitPeer *peer;

    object = arguments->object;
    index = arguments->index;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    *(int *)(index * 16 + peer->args) = arguments->a;
    unitPutArg(peer, index * 16 + 4, arguments->b);
    unitPutArg(peer, index * 16 + 8, arguments->c);
    unitPutArg(peer, index * 16 + 12, arguments->d);
}

/*
 * Unconditional, like setCollision__Z above. The peer's scale is handed back
 * through the one shared xeno.util.Vector4f object scale_0_007C0A58, whose
 * class-ref slot is re-seeded on every call (see UnitScaleVector).
 */
void Java_xeno_Unit_getScale__(JThread *thread, UnitObjectCall *arguments,
                               UnitResultValue *result)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    scale_0_007C0A58.class_ref = classJava_xeno_util_Vector4f->instance_class_ref;
    scale_0_007C0A58.x = peer->scale_x;
    scale_0_007C0A58.y = peer->scale_y;
    scale_0_007C0A58.z = peer->scale_z;
    scale_0_007C0A58.w = peer->scale_w;
    result->object = &scale_0_007C0A58;
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_setScale__FFF(JThread *thread, UnitVector3Call *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    peer->scale_x = arguments->x;
    peer->scale_y = arguments->y;
    peer->scale_z = arguments->z;
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_getSerial__(JThread *thread, UnitObjectCall *arguments,
                                u32 *failure_result)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    *failure_result = peer->serial;
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_mtnSetMask__I(JThread *thread, UnitIntCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    int mask;

    mask = arguments->value;
    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    peer->motion_mask = mask;
}

extern const float D_004D844C;
typedef struct { u8 *object; int index; float *vector; } UnitMtnRootCall;

void Java_xeno_Unit_mtnGetRoot__ILxeno_util_Vector4f_(JThread *thread,
                                                      UnitMtnRootCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    float *vector;
    int index;
    float *root;

    index = arguments->index;
    object = arguments->object;
    vector = arguments->vector;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    root = (float *)*(u8 **)(object + peer_field->offset);
    if (vector != 0) {
        switch (index) {
        case 0:
            vector[3] = root[0x170 / 4];
            vector[2] = root[0x174 / 4];
            vector[1] = root[0x178 / 4];
            break;
        case 1: {
            vector[3] = root[0x180 / 4] / D_004D844C * 180.0f;
            vector[2] = root[0x184 / 4] / D_004D844C * 180.0f;
            vector[1] = root[0x188 / 4] / D_004D844C * 180.0f;
            break;
        }
        case 2:
            vector[3] = root[0x190 / 4];
            vector[2] = root[0x194 / 4];
            vector[1] = root[0x198 / 4];
            break;
        }
    }
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_getState__(JThread *thread, UnitObjectCall *arguments,
                               u32 *failure_result)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    UnitSequenceEntry *entry;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    entry = &unitSequence[peer->serial];
    *failure_result = entry->state;
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_getPivot__Lxeno_util_Vector4f_(JThread *thread,
                                                   UnitPivotOutCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    UnitSequenceEntry *entry;
    UnitPivotVector *vector;

    vector = arguments->vector;
    object = arguments->object;
    if (vector != 0) {
        peer_field = lookupClassField(classJava_xeno_Unit,
                                      loadConstString(D_004DC1D0, -1), 0);
        peer = *(UnitPeer **)(object + peer_field->offset);
        entry = &unitSequence[peer->serial];
        vector->x = entry->pivot_x;
        vector->y = entry->pivot_y;
        vector->z = entry->pivot_z;
    }
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_setPivot__FFF(JThread *thread, UnitVector3Call *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    UnitSequenceEntry *entry;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    entry = &unitSequence[peer->serial];
    entry->pivot_x = arguments->x;
    entry->pivot_y = arguments->y;
    entry->pivot_z = arguments->z;
}

/* Unconditional peer lookup, but the vector-null check below guards the
 * writes, like getPivot__Lxeno_util_Vector4f_ above. */
void Java_xeno_Unit_getAxis__Lxeno_util_Vector4f_(JThread *thread,
                                                  UnitAxisOutCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    UnitSequenceEntry *entry;
    UnitAxisVector *vector;

    vector = arguments->vector;
    object = arguments->object;
    if (vector != 0) {
        peer_field = lookupClassField(classJava_xeno_Unit,
                                      loadConstString(D_004DC1D0, -1), 0);
        peer = *(UnitPeer **)(object + peer_field->offset);
        entry = &unitSequence[peer->serial];
        vector->x = entry->axis_x;
        vector->y = entry->axis_y;
        vector->z = entry->axis_z;
        vector->w = 1.0f;
    }
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_setAxis__FFFF(JThread *thread, UnitAxisCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    UnitSequenceEntry *entry;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    entry = &unitSequence[peer->serial];
    entry->axis_x = arguments->x;
    entry->axis_y = arguments->y;
    entry->axis_z = arguments->z;
    entry->axis_w = arguments->w;
}

/*
 * Callback shape for MAP_callUnitGroup's per-unit dispatch (src/main/
 * map_create_unit_peer.c, still INCLUDE_ASM): it forwards a pointer to a
 * flags word within the group's unit record. Only that one word's bit 0x10
 * is evidenced, so the record itself stays unmodeled here.
 */
#define UNIT_FLAG_SUSPENDED 0x10

static void unit_suspend(int *flags)
{
    *flags |= UNIT_FLAG_SUSPENDED;
}

static void unit_resume(int *flags)
{
    *flags &= ~UNIT_FLAG_SUSPENDED;
}

/* MAP_callUnitGroup is still asm elsewhere (src/main/map_create_unit_peer.c):
 * it applies `callback` to every unit record of group `group`. */
extern void MAP_callUnitGroup(int group, void (*callback)(int *flags));

void Java_xeno_Unit_suspend__I(JThread *thread, UnitGroupCall *arguments)
{
    MAP_callUnitGroup(arguments->group, unit_suspend);
}

void Java_xeno_Unit_resume__I(JThread *thread, UnitGroupCall *arguments)
{
    MAP_callUnitGroup(arguments->group, unit_resume);
}

/* The "py" field name initElevatorFunc__ looks up: a field on the same
 * Java xeno.Unit object, reused as backing storage for the elevator
 * callback's own state instead of the field's ordinary role. */
extern const char D_004DC1F8[];

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_initElevatorFunc__(JThread *thread,
                                       UnitObjectCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    JavaField *py_field;
    UnitPeer *peer;
    int py_offset;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    py_field = lookupClassField(classJava_xeno_Unit,
                                loadConstString(D_004DC1F8, -1), 0);
    py_offset = py_field->offset;
    peer->elevator_task = tyaElevatorTask;
    peer->elevator_state = 0;
    peer->elevator_field = object + py_offset;
    tyaElevatorTask(peer, py_offset);
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_map_shadow__I(JThread *thread, UnitIntCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    if (arguments->value != 0) {
        peer->flags |= 0x20;
    } else {
        peer->flags &= ~0x20;
    }
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_renderCommand__I(JThread *thread, UnitIntCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(object + peer_field->offset);
    peer->render_command = arguments->value;
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_setFilter__I(JThread *thread, UnitIntCall *arguments,
                                 u32 *failure_result)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    int value;
    int offset;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    offset = peer_field->offset;
    value = arguments->value;
    peer = *(UnitPeer **)(object + offset);
    peer->filter_mode = 0;
    switch (value) {
    case 2:
        peer->filter_mode = 1;
        *failure_result = 4;
        return;
    case 3:
        peer->filter_mode = 2;
        *failure_result = 4;
        return;
    }
}

typedef struct UnitFArrayCall {
    u8 *object;
    struct { int pad; u32 length; float *data; } *array;
} UnitFArrayCall;

void Java_xeno_Unit_setFilterParam__aF(JThread *thread, UnitFArrayCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    u8 *peer;
    u32 i;
    u32 n;
    float *src;
    float *dst;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    n = arguments->array->length;
    peer = *(u8 **)(object + peer_field->offset);
    dst = (float *)(peer + 0x2f0);
    for (i = 0; i < n; i++) {
        src = arguments->array->data;
        dst[i] = src[i];
    }
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_setShadow__II(JThread *thread, UnitShadowCall *arguments)
{
    JavaField *peer_field;
    UnitPeer *peer;

    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(arguments->object + peer_field->offset);
    peer->shadow_x = arguments->x;
    peer->shadow_y = arguments->y;
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_shadow_clip_scale__F(JThread *thread,
                                         UnitFloatCall *arguments)
{
    JavaField *peer_field;
    UnitPeer *peer;

    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(arguments->object + peer_field->offset);
    peer->shadow_clip_scale = arguments->value;
}

void Java_xeno_Unit_shadow_map_id__I(JThread *thread, UnitIntCall *arguments)
{
    JavaField *peer_field;
    UnitPeer *peer;
    int *count;

    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(arguments->object + peer_field->offset);
    if (peer->shadow_map_count < 8) {
        count = &peer->shadow_map_count;
        ((unsigned short *)&peer->model[0x90])[(*count)++] =
            *(unsigned short *)&arguments->value;
    }
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_shadow_map_reset__(JThread *thread,
                                       UnitObjectCall *arguments)
{
    JavaField *peer_field;
    UnitPeer *peer;

    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(arguments->object + peer_field->offset);
    peer->shadow_map_count = 0;
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_setSortOffset__F(JThread *thread, UnitFloatCall *arguments)
{
    JavaField *peer_field;
    UnitPeer *peer;

    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    peer = *(UnitPeer **)(arguments->object + peer_field->offset);
    peer->sort_offset = (int) arguments->value;
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_setClip__I(JThread *thread, UnitIntCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    int value;

    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    value = arguments->value;
    object = arguments->object;
    peer = *(UnitPeer **)(object + peer_field->offset);
    if (value != 0) {
        peer->flags |= 0x40;
    } else {
        peer->flags &= ~0x40;
    }
}

/* Unconditional, like setCollision__Z above. */
void Java_xeno_Unit_setMonitorPrio__I(JThread *thread, UnitIntCall *arguments)
{
    JavaField *peer_field;
    u8 *object;
    UnitPeer *peer;
    int value;
    int offset;

    peer_field = lookupClassField(classJava_xeno_Unit,
                                  loadConstString(D_004DC1D0, -1), 0);
    value = arguments->value;
    offset = peer_field->offset;
    object = arguments->object;
    peer = *(UnitPeer **)(object + offset);
    if (value != 0) {
        peer->monitor_priority = 1;
        return;
    }
    peer->monitor_priority = 0;
}

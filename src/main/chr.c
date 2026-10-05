#include "common.h"
#include "chr.h"

void Java_xeno_Chr_getPlayer__(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    u8 *object;

    /*
     * Unlike every other Chr native here, this one does not chase through
     * the peer pointer's target: it stores GameLoopState's own +0x4 word
     * directly into the Java object's `peer` field (0x002fcd28..0x002fcd34).
     */
    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    *(unsigned int *)(object + peer_field->offset) = GameLoopState[1];
}

extern unsigned char SaveData[];
extern int GameResource[];
extern const char D_004DC198[];
extern int ACT_allocMatrix(char *actor, int count);
extern void ACT_setModelWrapper(void *actor, int flags);
extern void *ACT_create(int id, int code);
extern int ACT_initMotion(void *peer);
extern int ACT_setRelation(Actor *actor, Actor *parent, int rel, int id);
extern void ACT_setHumanHand(Actor *actor, int hand);
extern void ACT_setMotion(Actor *actor, unsigned int dataId);
extern void *getPeer_Chr(void *obj);

void Java_xeno_Chr_setPlayer__(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *field;
    u8 *object;
    u8 *peer;
    u8 *hand;
    int id;

    id = *(u16 *)(SaveData + 0x100A4);
    object = arguments->object;
    if (GameResource[1] == 0x12A000 && GameResource[7] == 9) {
        field = lookupClassField(classJava_xeno_Chr,
                                 loadConstString(D_004DC198, -1), 0);
        *(int *)(object + field->offset) = id;
        peer = getPeer_Chr(object);
        *(int *)(peer + 0x8D0) = 0x01000000;
        *(int *)(peer + 0x8D4) = 0x01070800;
        *(int *)(peer + 0x8D8) = 0x010B1000;
        *(int *)(peer + 0x8DC) = 0x010E7000;
        ACT_allocMatrix(peer, 0xAA);
        ACT_setModelWrapper(peer, 0);
        hand = ACT_create(-2, id + 0x10000);
        *(int *)(hand + 0x8D0) = 0x010B4000;
        *(int *)(hand + 0x8D4) = 0;
        *(int *)(hand + 0x8D8) = 0x010E6000;
        ACT_allocMatrix(hand, 0x33);
        ACT_setModelWrapper(hand, 0);
        hand[0x90] = 0;
        *(u32 *)hand &= ~0x20;
        ACT_initMotion(hand);
        ACT_setRelation((Actor *)hand, (Actor *)peer, 0x8200, 1);
        ACT_setHumanHand((Actor *)peer, 0);
        ACT_setMotion((Actor *)peer, 0x8000);
    } else {
        field = lookupClassField(classJava_xeno_Chr,
                                 loadConstString(chr_peer_string, -1), 0);
        peer = *(u8 **)(object + field->offset);
    }
    ((Actor *)peer)->update = 0;
    GameLoopState[1] = (unsigned int)peer;
}

extern void SEQ_moveXZ(void);
extern void SEQ_moveChr(void);
struct ChrMoveCall {
    u8 *object;
    union { int i; float f; u8 *p; } a;
    union { int i; float f; u8 *p; } b;
    union { int i; float f; u8 *p; } c;
    u8 wait;
};
typedef struct ChrMoveObjCall {
    u8 *object;
    union { int i; float f; u8 *p; } a;
    union { int i; float f; u8 *p; } b;
    u8 wait;
} ChrMoveObjCall;
typedef struct ChrMoveState {
    void *target;
    int frames;
    u8 pad08[0x20 - 0x08];
    float x;
    u8 pad24[0x28 - 0x24];
    float z;
    float speed;
} ChrMoveState;
typedef struct ChrMoveEntry {
    u32 flags;
    u32 state_flags;
    u32 mask;
} ChrMoveEntry;

static void CHR_moveXZ(int mode, JThread *thread, ChrMoveCall *arguments,
                       u32 *failure_result)
{
    JavaField *field;
    Actor *peer;
    float *offset;
    ChrMoveEntry *entry;
    ChrMoveState *move;
    u8 *object;
    int wait;

    wait = 0;
    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + field->offset);
    entry = (ChrMoveEntry *)(actSequence +
        ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET] * SEQUENCE_STRIDE);
    move = (ChrMoveState *)((u8 *)entry + 0x38);
    entry->mask = 1;
    entry->state_flags |= 1;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_algorithm_string, -1), 0);
    offset = (*(u32 *)(object + field->offset) & 1)
        ? (float *)((u8 *)peer + 0x10) : defaultOffset;

    switch (mode) {
    case 0:
        move->frames = arguments->a.i;
        move->x = arguments->b.f + offset[0];
        {
            float c = arguments->c.f;
            float o2 = offset[2];
            *(void **)((u8 *)entry + 0x24) = SEQ_moveXZ;
            move->z = c + o2;
        }
        wait = arguments->wait;
        break;
    case 1:
        move->frames = -1;
        move->x = arguments->a.f + offset[0];
        move->z = arguments->b.f + offset[2];
        {
            float c = arguments->c.f;
            *(void **)((u8 *)entry + 0x24) = SEQ_moveXZ;
            move->speed = c / 30.0f;
        }
        wait = arguments->wait;
        break;
    case 2:
        field = lookupClassField(classJava_xeno_Chr,
                                 loadConstString(chr_peer_string, -1), 0);
        {
            u8 *o = ((ChrMoveObjCall *)arguments)->b.p + field->offset;
            int fr = ((ChrMoveObjCall *)arguments)->a.i;
            *(void **)((u8 *)entry + 0x24) = SEQ_moveChr;
            o = *(u8 **)o;
            move->frames = fr;
            move->target = o;
        }
        wait = ((ChrMoveObjCall *)arguments)->wait;
        break;
    case 3:
        field = lookupClassField(classJava_xeno_Chr,
                                 loadConstString(chr_peer_string, -1), 0);
        {
            u8 *o = ((ChrMoveObjCall *)arguments)->a.p + field->offset;
            move->frames = -1;
            o = *(u8 **)o;
            move->target = o;
        }
        {
            float c = ((ChrMoveObjCall *)arguments)->b.f;
            *(void **)((u8 *)entry + 0x24) = SEQ_moveChr;
            move->speed = c / 30.0f;
        }
        wait = ((ChrMoveObjCall *)arguments)->wait;
        break;
    }

    if (wait == 1) {
        peer->flags |= 2;
        if (thread->kind != wait && (thread->flags & 0x40) == 0)
            return;
        if ((thread->flags & 0x40) != 0) {
            thread->wait_target = peer;
            thread->wait_kind = JTHREAD_WAIT_ACTOR;
            thread->resume_frames = thread->frame_depth;
            thread->flags |= 4;
        }
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 1;
    }
}

/*
 * CHR_moveXZ is local to this file in the original (glabel ..., local); it
 * stays asm here but needs a prototype for the trampolines below to call.
 * Overload -> mode: IFFZ 0, FFFZ 1, I,Object,Z 2, Object,F,Z 3.
 */
static void CHR_moveXZ(int mode, JThread *thread, ChrMoveCall *arguments,
                       u32 *failure_result);

void Java_xeno_Chr_move__IFFZ(JThread *thread, ChrMoveCall *arguments,
                              u32 *failure_result)
{
    CHR_moveXZ(0, thread, arguments, failure_result);
}

void Java_xeno_Chr_move__FFFZ(JThread *thread, ChrMoveCall *arguments,
                              u32 *failure_result)
{
    CHR_moveXZ(1, thread, arguments, failure_result);
}

void Java_xeno_Chr_move__ILjava_lang_Object_Z(JThread *thread,
                                              ChrMoveCall *arguments,
                                              u32 *failure_result)
{
    CHR_moveXZ(2, thread, arguments, failure_result);
}

void Java_xeno_Chr_move__Ljava_lang_Object_FZ(JThread *thread,
                                              ChrMoveCall *arguments,
                                              u32 *failure_result)
{
    CHR_moveXZ(3, thread, arguments, failure_result);
}

extern void SEQ_moveSPL(void);
typedef struct ChrMoveSplCall {
    u8 *object;
    u8 *spline;
    int pad;
    u8 wait;
} ChrMoveSplCall;

void Java_xeno_Chr_move__Lxeno_util_Spline_IZ(JThread *thread,
                                              ChrMoveSplCall *arguments,
                                              u32 *failure_result)
{
    JavaField *peer_field;
    u8 *object;
    Actor *peer;
    ChrMoveEntry *entry;
    u8 *spline;
    u8 *sargs;
    int nf;
    int wait;

    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    entry = (ChrMoveEntry *)(actSequence +
        ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET] * SEQUENCE_STRIDE);
    sargs = (u8 *)entry + 0x38;
    entry->mask = 1;
    entry->state_flags |= 1;
    lookupClassField(classJava_xeno_Chr,
                     loadConstString(chr_algorithm_string, -1), 0);
    spline = arguments->spline;
    wait = arguments->wait;
    nf = arguments->pad;
    *(u16 *)(sargs + 8) = nf;
    *(void **)((u8 *)entry + 0x24) = SEQ_moveSPL;
    *(u8 **)sargs = spline;
    *(u16 *)(sargs + 6) = *(u16 *)(spline + 8);
    if (wait == 1) {
        peer->flags |= 2;
        if (thread->kind != wait && (thread->flags & 0x40) == 0)
            return;
        if ((thread->flags & 0x40) != 0) {
            thread->wait_target = peer;
            thread->wait_kind = JTHREAD_WAIT_ACTOR;
            thread->resume_frames = thread->frame_depth;
            thread->flags |= 4;
        }
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 1;
    }
}

extern void SEQ_rotateSPL(void);
typedef struct ChrSplCall {
    u8 *object;
    u8 *spline;
    int pad;
    u8 wait;
} ChrSplCall;

void Java_xeno_Chr_rotate__Lxeno_util_Spline_IZ(JThread *thread,
                                                ChrSplCall *arguments,
                                                u32 *failure_result)
{
    JavaField *peer_field;
    u8 *object;
    Actor *peer;
    ChrMoveEntry *entry;
    u8 *spline;
    u8 *sargs;
    int nf;
    int wait;

    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    entry = (ChrMoveEntry *)(actSequence +
        ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET] * SEQUENCE_STRIDE);
    sargs = (u8 *)entry + 0xb8;
    entry->mask = 0xE;
    entry->state_flags |= 0xE;
    lookupClassField(classJava_xeno_Chr,
                     loadConstString(chr_algorithm_string, -1), 0);
    spline = arguments->spline;
    wait = arguments->wait;
    nf = arguments->pad;
    *(u16 *)(sargs + 8) = nf;
    *(void **)((u8 *)entry + 0x28) = SEQ_rotateSPL;
    *(u8 **)sargs = spline;
    *(u16 *)(sargs + 6) = *(u16 *)(spline + 8);
    if (wait == 1) {
        peer->flags |= 2;
        if (thread->kind != wait && (thread->flags & 0x40) == 0)
            return;
        if ((thread->flags & 0x40) != 0) {
            thread->wait_target = peer;
            thread->wait_kind = JTHREAD_WAIT_ACTOR;
            thread->resume_frames = thread->frame_depth;
            thread->flags |= 4;
        }
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 1;
    }
}

extern void SEQ_rotate(void);
struct ChrRotCall {
    u8 *object;
    union { int i; float f; u8 *p; } first;
    union { int i; float f; u8 *p; } second;
    u8 wait;
};
typedef struct ChrRotState {
    void *target;
    u8 pad04[0x10 - 0x04];
    int frames;
    u8 pad14[0x1c - 0x14];
    u32 flags;
    u8 pad20[0x30 - 0x20];
    float angle;
    u8 pad34[0x40 - 0x34];
    float speed;
} ChrRotState;
typedef struct ChrRotObjCall {
    u8 *object;
    int frames;
    u8 *other;
} ChrRotObjCall;
typedef struct ChrRotObjCall2 {
    u8 *object;
    u8 *other;
    float speed;
} ChrRotObjCall2;

static void CHR_rotX(int mode, JThread *thread, ChrRotCall *arguments,
                     u32 *failure_result)
{
    JavaField *field;
    Actor *peer;
    float *rot_source;
    ChrMoveEntry *entry;
    ChrRotState *rot;
    u8 *object;
    int wait;

    wait = 0;
    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + field->offset);
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_algorithm_string, -1), 0);
    rot_source = (float *)((u8 *)peer + 0x50);
    if ((*(u32 *)(object + field->offset) & 1) == 0)
        rot_source = defaultOffset;

    entry = (ChrMoveEntry *)(actSequence +
        ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET] * SEQUENCE_STRIDE);
    rot = (ChrRotState *)((u8 *)entry + 0xb8);
    *(void **)((u8 *)entry + 0x28) = SEQ_rotate;
    entry->state_flags |= 2;

    switch (mode) {
    case 0:
        rot->frames = arguments->first.i;
        rot->flags &= ~1;
        rot->angle =
            (arguments->second.f + rot_source[0]) / 180.0f * 3.14159265f;
        wait = arguments->wait;
        break;
    case 1:
        rot->frames = -1;
        rot->flags &= ~1;
        rot->angle =
            (arguments->first.f + rot_source[0]) / 180.0f * 3.14159265f;
        rot->speed =
            arguments->second.f / 180.0f * 3.14159265f / 30.0f;
        wait = arguments->wait;
        break;
    case 2:
        {
            int off = field->offset;
            u8 *o = ((ChrRotObjCall *)arguments)->other;
            rot->frames = ((ChrRotObjCall *)arguments)->frames;
            rot->flags |= 1;
            o += off;
            o = *(u8 **)o;
            rot->target = o;
        }
        wait = arguments->wait;
        break;
    case 3:
        {
            int off = field->offset;
            u8 *o = ((ChrRotObjCall2 *)arguments)->other;
            rot->frames = -1;
            rot->flags |= 1;
            o += off;
            o = *(u8 **)o;
            rot->target = o;
        }
        rot->speed =
            arguments->second.f / 180.0f * 3.14159265f / 30.0f;
        wait = arguments->wait;
        break;
    }

    if (wait == 1) {
        peer->flags |= 2;
        if (thread->kind != wait && (thread->flags & 0x40) == 0)
            return;
        if ((thread->flags & 0x40) != 0) {
            thread->wait_target = peer;
            thread->wait_kind = JTHREAD_WAIT_ACTOR;
            thread->resume_frames = thread->frame_depth;
            thread->flags |= 4;
        }
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 1;
    }
}

typedef struct ChrRotYState {
    void *target;
    u8 pad04[0x14 - 0x04];
    int frames;
    u8 pad18[0x1c - 0x18];
    u32 flags;
    u8 pad20[0x34 - 0x20];
    float angle;
    u8 pad38[0x44 - 0x38];
    float speed;
} ChrRotYState;

static void CHR_rotY(int mode, JThread *thread, ChrRotCall *arguments,
                     u32 *failure_result)
{
    JavaField *field;
    Actor *peer;
    float *rot_source;
    ChrMoveEntry *entry;
    ChrRotYState *rot;
    u8 *object;
    int wait;

    wait = 0;
    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + field->offset);
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_algorithm_string, -1), 0);
    rot_source = (*(u32 *)(object + field->offset) & 1)
        ? (float *)((u8 *)peer + 0x50) : defaultOffset;

    entry = (ChrMoveEntry *)(actSequence +
        ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET] * SEQUENCE_STRIDE);
    rot = (ChrRotYState *)((u8 *)entry + 0xb8);
    *(void **)((u8 *)entry + 0x28) = SEQ_rotate;
    entry->state_flags |= 4;

    switch (mode) {
    case 0:
        rot->frames = arguments->first.i;
        rot->flags &= ~2;
        rot->angle =
            (arguments->second.f + rot_source[1]) / 180.0f * 3.14159265f;
        wait = arguments->wait;
        break;
    case 1:
        rot->frames = -1;
        rot->flags &= ~2;
        rot->angle =
            (arguments->first.f + rot_source[1]) / 180.0f * 3.14159265f;
        rot->speed =
            arguments->second.f / 180.0f * 3.14159265f / 30.0f;
        wait = arguments->wait;
        break;
    case 2:
        {
            int off;
            u8 *o;
            field = lookupClassField(classJava_xeno_Chr,
                                     loadConstString(chr_peer_string, -1), 0);
            off = field->offset;
            o = ((ChrRotObjCall *)arguments)->other;
            rot->frames = ((ChrRotObjCall *)arguments)->frames;
            rot->flags |= 2;
            o += off;
            o = *(u8 **)o;
            rot->target = o;
        }
        wait = arguments->wait;
        break;
    case 3:
        {
            int off;
            u8 *o;
            field = lookupClassField(classJava_xeno_Chr,
                                     loadConstString(chr_peer_string, -1), 0);
            off = field->offset;
            o = ((ChrRotObjCall2 *)arguments)->other;
            rot->frames = -1;
            rot->flags |= 2;
            o += off;
            o = *(u8 **)o;
            rot->target = o;
        }
        rot->speed =
            arguments->second.f / 180.0f * 3.14159265f / 30.0f;
        wait = arguments->wait;
        break;
    }

    if (wait == 1) {
        peer->flags |= 2;
        if (thread->kind != wait && (thread->flags & 0x40) == 0)
            return;
        if ((thread->flags & 0x40) != 0) {
            thread->wait_target = peer;
            thread->wait_kind = JTHREAD_WAIT_ACTOR;
            thread->resume_frames = thread->frame_depth;
            thread->flags |= 4;
        }
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 1;
    }
}

typedef struct ChrRotZState {
    void *target;
    u8 pad04[0x18 - 0x04];
    int frames;
    u32 flags;
    u8 pad20[0x38 - 0x20];
    float angle;
    u8 pad3c[0x48 - 0x3c];
    float speed;
} ChrRotZState;

static void CHR_rotZ(int mode, JThread *thread, ChrRotCall *arguments,
                     u32 *failure_result)
{
    JavaField *field;
    Actor *peer;
    float *rot_source;
    ChrMoveEntry *entry;
    ChrRotZState *rot;
    u8 *object;
    int wait;

    wait = 0;
    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + field->offset);
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_algorithm_string, -1), 0);
    rot_source = (float *)((u8 *)peer + 0x50);
    if ((*(u32 *)(object + field->offset) & 1) == 0)
        rot_source = defaultOffset;

    entry = (ChrMoveEntry *)(actSequence +
        ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET] * SEQUENCE_STRIDE);
    rot = (ChrRotZState *)((u8 *)entry + 0xb8);
    *(void **)((u8 *)entry + 0x28) = SEQ_rotate;
    entry->state_flags |= 8;

    switch (mode) {
    case 0:
        rot->frames = arguments->first.i;
        rot->flags &= ~4;
        rot->angle =
            (arguments->second.f + rot_source[2]) / 180.0f * 3.14159265f;
        wait = arguments->wait;
        break;
    case 1:
        rot->frames = -1;
        rot->flags &= ~4;
        rot->angle =
            (arguments->first.f + rot_source[2]) / 180.0f * 3.14159265f;
        rot->speed =
            arguments->second.f / 180.0f * 3.14159265f / 30.0f;
        wait = arguments->wait;
        break;
    case 2:
        {
            int off = field->offset;
            u8 *o = ((ChrRotObjCall *)arguments)->other;
            rot->frames = ((ChrRotObjCall *)arguments)->frames;
            rot->flags |= 4;
            o += off;
            o = *(u8 **)o;
            rot->target = o;
        }
        wait = arguments->wait;
        break;
    case 3:
        {
            int off = field->offset;
            u8 *o = ((ChrRotObjCall2 *)arguments)->other;
            rot->frames = -1;
            rot->flags |= 2;
            o += off;
            o = *(u8 **)o;
            rot->target = o;
        }
        rot->speed =
            arguments->second.f / 180.0f * 3.14159265f / 30.0f;
        wait = arguments->wait;
        break;
    }

    if (wait == 1) {
        peer->flags |= 2;
        if (thread->kind != wait && (thread->flags & 0x40) == 0)
            return;
        if ((thread->flags & 0x40) != 0) {
            thread->wait_target = peer;
            thread->wait_kind = JTHREAD_WAIT_ACTOR;
            thread->resume_frames = thread->frame_depth;
            thread->flags |= 4;
        }
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 1;
    }
}

/*
 * CHR_rotX/CHR_rotY/CHR_rotZ are local to this file in the original (glabel
 * ..., local); they stay asm here but need a prototype for the trampolines
 * below to call. Overload -> mode: FFZ 1, IFZ 0, I,Object,Z 2, Object,F,Z 3
 * (same as CHR_sclX).
 */
static void CHR_rotX(int mode, JThread *thread, ChrRotCall *arguments,
                     u32 *failure_result);
static void CHR_rotY(int mode, JThread *thread, ChrRotCall *arguments,
                     u32 *failure_result);
static void CHR_rotZ(int mode, JThread *thread, ChrRotCall *arguments,
                     u32 *failure_result);

void Java_xeno_Chr_rotX__FFZ(JThread *thread, ChrRotCall *arguments,
                             u32 *failure_result)
{
    CHR_rotX(1, thread, arguments, failure_result);
}

void Java_xeno_Chr_rotX__IFZ(JThread *thread, ChrRotCall *arguments,
                             u32 *failure_result)
{
    CHR_rotX(0, thread, arguments, failure_result);
}

void Java_xeno_Chr_rotX__ILjava_lang_Object_Z(JThread *thread,
                                              ChrRotCall *arguments,
                                              u32 *failure_result)
{
    CHR_rotX(2, thread, arguments, failure_result);
}

void Java_xeno_Chr_rotX__Ljava_lang_Object_FZ(JThread *thread,
                                              ChrRotCall *arguments,
                                              u32 *failure_result)
{
    CHR_rotX(3, thread, arguments, failure_result);
}

void Java_xeno_Chr_rotY__FFZ(JThread *thread, ChrRotCall *arguments,
                             u32 *failure_result)
{
    CHR_rotY(1, thread, arguments, failure_result);
}

void Java_xeno_Chr_rotY__IFZ(JThread *thread, ChrRotCall *arguments,
                             u32 *failure_result)
{
    CHR_rotY(0, thread, arguments, failure_result);
}

void Java_xeno_Chr_rotY__ILjava_lang_Object_Z(JThread *thread,
                                              ChrRotCall *arguments,
                                              u32 *failure_result)
{
    CHR_rotY(2, thread, arguments, failure_result);
}

void Java_xeno_Chr_rotY__Ljava_lang_Object_FZ(JThread *thread,
                                              ChrRotCall *arguments,
                                              u32 *failure_result)
{
    CHR_rotY(3, thread, arguments, failure_result);
}

void Java_xeno_Chr_rotZ__FFZ(JThread *thread, ChrRotCall *arguments,
                             u32 *failure_result)
{
    CHR_rotZ(1, thread, arguments, failure_result);
}

void Java_xeno_Chr_rotZ__IFZ(JThread *thread, ChrRotCall *arguments,
                             u32 *failure_result)
{
    CHR_rotZ(0, thread, arguments, failure_result);
}

void Java_xeno_Chr_rotZ__ILjava_lang_Object_Z(JThread *thread,
                                              ChrRotCall *arguments,
                                              u32 *failure_result)
{
    CHR_rotZ(2, thread, arguments, failure_result);
}

void Java_xeno_Chr_rotZ__Ljava_lang_Object_FZ(JThread *thread,
                                              ChrRotCall *arguments,
                                              u32 *failure_result)
{
    CHR_rotZ(3, thread, arguments, failure_result);
}

extern JThread *JTHREAD_get(void *object);
extern void JTHREAD_defaultChr();
extern void *findMethod(void *scene_class, void *name, void *type);
extern void JNI_initThread(void *vm);
extern void *TYPE_Void;
extern int SCRIPT_methodClearGet(void);
extern void ACT_updateDefault(Actor *actor);
extern void ACT_updateSequence(Actor *actor);
extern void ACT_updatePlayer(Actor *actor);
extern void ACT_updateMPack(Actor *actor);
typedef struct ChrStartString {
    int pad0;
    int length;
    char *bytes;
} ChrStartString;
typedef struct ChrStartStringObj {
    int pad0;
    ChrStartString *str;
} ChrStartStringObj;
typedef struct ChrStartEntry {
    u32 flags;
    u32 state_flags;
    u32 mask;
    int mode;
    u8 pad10[0x14 - 0x10];
    void *method;
    u8 pad18[0x24 - 0x18];
    void *handler[4];
} ChrStartEntry;
typedef struct ChrStartCall {
    u8 *object;
    int mode;
    ChrStartStringObj *name;
} ChrStartCall;

void Java_xeno_Chr_start__ILjava_lang_Object_(JThread *thread,
                                              ChrStartCall *arguments,
                                              u32 *failure_result)
{
    JavaField *field;
    u8 *object;
    Actor *peer;
    int mode;
    ChrStartEntry *entry;
    ChrStartStringObj *name;
    ChrStartString *str;
    void *method;
    void *cls;
    JThread *t;

    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_peer_string, -1), 0);
    mode = arguments->mode;
    peer = *(Actor **)(object + field->offset);
    entry = (ChrStartEntry *)(actSequence +
        ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET] * SEQUENCE_STRIDE);
    entry->mode = mode;
    switch (mode) {
    case 0:
        peer->update = ACT_updateDefault;
        break;
    case 4:
        entry->mode = 4;
        peer->update = ACT_updateSequence;
        entry->state_flags = 0;
        break;
    case 1:
        name = arguments->name;
        cls = (void *)*(u32 *)*(u32 *)object;
        entry->state_flags = 0;
        if (SCRIPT_methodClearGet()) {
            peer->flags &= ~2;
            entry->handler[0] = 0;
            entry->handler[1] = 0;
            entry->handler[2] = 0;
            entry->flags = 0;
            entry->handler[3] = 0;
        }
        str = name->str;
        method = findMethod(cls, loadConstString(str->bytes, str->length),
                            TYPE_Void);
        entry->method = method;
        t = JTHREAD_get(object);
        if (t != 0) {
            t->method = method;
            t->reset = JTHREAD_defaultChr;
            JNI_initThread(t);
            t->flags |= 0x10;
        }
        entry->mode = 1;
        peer->update = ACT_updateSequence;
        entry->handler[1] = 0;
        entry->handler[2] = 0;
        entry->handler[3] = 0;
        entry->handler[0] = 0;
        break;
    case 2:
        entry->mode = 2;
        peer->update = ACT_updatePlayer;
        break;
    case 5:
        entry->state_flags = 0;
        entry->flags = 0;
        peer->update = ACT_updateMPack;
        break;
    }
}

/*
 * JTHREAD_get is local to jthread.c (still asm there) and only that TU
 * declares its own SceneThread-typed view; this TU needs its own prototype
 * over JThread, the type it already completes above (jal JTHREAD_get at
 * 0x002fe49c).
 */
extern JThread *JTHREAD_get(void *object);

void Java_xeno_Chr_stop__(JThread *thread, ChrObjectCall *arguments,
                          u32 *failure_result)
{
    JavaField *field;
    Actor *peer;
    JThread *chr_thread;
    u8 *object;

    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + field->offset);
    chr_thread = JTHREAD_get(object);
    if (chr_thread != 0) {
        chr_thread->flags &= ~0x10;
    }
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1A0, -1), 0);
    *(float *)(object + field->offset) = peer->position.x;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1A8, -1), 0);
    *(float *)(object + field->offset) = peer->position.y;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1B0, -1), 0);
    *(float *)(object + field->offset) = peer->position.z;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1B8, -1), 0);
    *(float *)(object + field->offset) = peer->rotation.x;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1C0, -1), 0);
    *(float *)(object + field->offset) = peer->rotation.y;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1C8, -1), 0);
    *(float *)(object + field->offset) = peer->rotation.z;
}

extern void SEQ_motion(void);
typedef struct ChrMotionState {
    u8 pad00[0x08];
    int motion;
    u8 pad0c[0x10 - 0x0c];
    u16 start;
    u16 end;
    u8 loop;
    u8 pad15[0x18 - 0x15];
    int frames;
    float speed;
} ChrMotionState;
typedef union { int i; float f; u16 h; u8 b; } ChrMotionWord;

static void CHR_motion(int mode, JThread *thread, ChrMotionCall *arguments_,
                       u32 *failure_result)
{
    ChrMotionWord *arguments = (ChrMotionWord *)arguments_;
    JavaField *field;
    Actor *peer;
    ChrMoveEntry *entry;
    ChrMotionState *mot;
    u8 *object;
    int wait;

    wait = 0;
    object = (u8 *)arguments[0].i;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + field->offset);
    entry = (ChrMoveEntry *)(actSequence +
        ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET] * SEQUENCE_STRIDE);
    mot = (ChrMotionState *)((u8 *)entry + 0x138);
    *(void **)((u8 *)entry + 0x2c) = SEQ_motion;
    entry->state_flags |= 0x10;

    switch (mode) {
    case 0:
        if (arguments[1].i & 0xFF000000)
            mot->motion = arguments[1].i;
        else
            mot->motion = arguments[1].i - 1;
        mot->frames = arguments[2].i;
        mot->speed = arguments[3].f;
        wait = arguments[4].b;
        mot->start = -1;
        mot->end = -1;
        mot->loop = 8;
        break;
    case 1:
        if (arguments[1].i & 0xFF000000)
            mot->motion = arguments[1].i;
        else
            mot->motion = arguments[1].i - 1;
        mot->start = arguments[2].h;
        mot->end = arguments[3].h;
        mot->loop = arguments[4].b;
        mot->frames = arguments[5].i;
        mot->speed = arguments[6].f;
        wait = arguments[7].b;
        break;
    }

    if (wait == 1) {
        peer->flags |= 2;
        if (thread->kind != wait && (thread->flags & 0x40) == 0)
            return;
        if ((thread->flags & 0x40) != 0) {
            thread->wait_target = peer;
            thread->wait_kind = JTHREAD_WAIT_ACTOR;
            thread->resume_frames = thread->frame_depth;
            thread->flags |= 4;
        }
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 1;
    }
}

/*
 * CHR_motion is local to this file in the original (glabel ..., local); it
 * stays asm here but needs a prototype for the trampolines below to call.
 * Overload -> mode: IIIIIFZ 1, IIFZ 0.
 */
static void CHR_motion(int mode, JThread *thread, ChrMotionCall *arguments,
                       u32 *failure_result);

void Java_xeno_Chr_mtn__IIIIIFZ(JThread *thread, ChrMotionCall *arguments,
                                u32 *failure_result)
{
    CHR_motion(1, thread, arguments, failure_result);
}

void Java_xeno_Chr_mtn__IIFZ(JThread *thread, ChrMotionCall *arguments,
                             u32 *failure_result)
{
    CHR_motion(0, thread, arguments, failure_result);
}

void Java_xeno_Chr_signal__I(JThread *thread, ChrSignalCall *arguments,
                             u32 *failure_result)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->signal = arguments->value;
}

void Java_xeno_Chr_getSignal__(JThread *thread, ChrObjectCall *arguments,
                               u32 *failure_result)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    *failure_result = peer->signal;
}

void Java_xeno_Chr_setRotate__(JThread *thread, ChrScaleCall *arguments)
{
    u8 *object;
    JavaField *field;
    float *rot;

    object = arguments->object;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_peer_string, -1), 0);
    rot = (float *)(*(u8 **)(object + field->offset) + 0x50);
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1B8, -1), 0);
    rot[0] = *(float *)(object + field->offset) / 180.0f * 3.14159265f;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1C0, -1), 0);
    rot[1] = *(float *)(object + field->offset) / 180.0f * 3.14159265f;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1C8, -1), 0);
    rot[2] = *(float *)(object + field->offset) / 180.0f * 3.14159265f;
}

/*
 * The state byte at Actor+0xa40, past this TU's recovered head above:
 * growing the struct there would move data_header and every field after it
 * that other accepted functions in this file already read at their current
 * offsets, so this one narrowly evidenced access stays a byte view, exactly
 * as ACTOR_SLOT_NUMBER_OFFSET above does for +0x80. Only
 * Java_xeno_Chr_setTranslate__ reads or writes it here: it tests bit 0 and,
 * when set, stores the literal 2 back (lbu/andi/sb at
 * 0x002feb9c-0x002febb0).
 */
#define ACTOR_TRANSLATE_STATE_OFFSET 0xa40

void Java_xeno_Chr_setTranslate__(JThread *thread, ChrObjectCall *arguments)
{
    JavaField *field;
    Actor *peer;
    Vector4 *position;
    u8 *object;
    float z;

    object = arguments->object;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + field->offset);
    position = &peer->position;

    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1A0, -1), 0);
    position->x = *(float *)(object + field->offset);
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1A8, -1), 0);
    position->y = *(float *)(object + field->offset);
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1B0, -1), 0);
    z = *(float *)(object + field->offset);

    peer->status_flags |= 0x1000;
    position->z = z;
    peer->translate_y = position->y;
    if (((u8 *)peer)[ACTOR_TRANSLATE_STATE_OFFSET] & 1) {
        ((u8 *)peer)[ACTOR_TRANSLATE_STATE_OFFSET] = 2;
    }
}

void Java_xeno_Chr_getRotate__(JThread *thread, ChrScaleCall *arguments)
{
    u8 *object;
    JavaField *field;
    float *rot;

    object = arguments->object;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_peer_string, -1), 0);
    rot = (float *)(*(u8 **)(object + field->offset) + 0x50);
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1B8, -1), 0);
    *(float *)(object + field->offset) = rot[0] / 3.1415927f * 180.0f;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1C0, -1), 0);
    *(float *)(object + field->offset) = rot[1] / 3.1415927f * 180.0f;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1C8, -1), 0);
    *(float *)(object + field->offset) = rot[2] / 3.1415927f * 180.0f;
}

void Java_xeno_Chr_getTranslate__(JThread *thread, ChrScaleCall *arguments,
                                  u32 *failure_result)
{
    JavaField *field;
    Actor *peer;
    Vector4 *position;
    u8 *object;

    /* Each Java field has a runtime class offset, rather than a fixed C member. */
    object = arguments->object;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + field->offset);
    position = &peer->position;

    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1A0, -1), 0);
    *(float *)(object + field->offset) = position->x;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1A8, -1), 0);
    *(float *)(object + field->offset) = position->y;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(D_004DC1B0, -1), 0);
    *(float *)(object + field->offset) = position->z;
}

void Java_xeno_Chr_setVisible__Z(JThread *thread, ChrBoolCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    if (arguments->flag != 0) {
        peer->flags &= ~8;
    } else {
        peer->flags |= 8;
    }
}

void Java_xeno_Chr_setVisible__IZ(JThread *thread, ChrVisibleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    ACT_setVisible(peer, arguments->part, arguments->visible);
}

void Java_xeno_Chr_setCollision__Z(JThread *thread, ChrBoolCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    if (arguments->flag != 0) {
        peer->flags |= 0x40;
    } else {
        peer->flags &= ~0x40;
    }
}

void Java_xeno_Chr_setHand__I(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    ACT_setHand(peer, arguments->first.integer);
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

void Java_xeno_Chr_setArgs__III(JThread *thread, ChrArgsWordCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    int value;
    u8 *object;
    int offset;
    int size;

    value = arguments->value;
    object = arguments->object;
    offset = arguments->offset;
    size = arguments->size;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    if ((unsigned int)(size - 1) < 4U) {
        copyArgs(peer->args + offset, (u8 *)&value, size);
    }
}

void Java_xeno_Chr_getArgs__II(JThread *thread, ChrArgsReadCall *arguments,
                               int *result)
{
    JavaField *peer_field;
    Actor *peer;
    int value;
    u8 *object;
    int offset;
    int size;

    object = arguments->object;
    offset = arguments->offset;
    size = arguments->size;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    if ((unsigned int)(size - 1) < 4U) {
        copyArgs((u8 *)&value, peer->args + offset, size);
        *result = value;
    }
}

void Java_xeno_Chr_setArgs__ILjava_lang_Object_I(JThread *thread,
                                                 ChrArgsObjectCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    ChrScaleCall *source;
    int offset;
    u8 *object;
    int second;
    int first;

    source = arguments->source;
    offset = arguments->offset;
    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    second = source->second.integer;
    peer = *(Actor **)(object + peer_field->offset);
    first = source->first.integer;
    *(int *)(peer->args + offset) = second;
    *(int *)(peer->args + offset + 4) = first;
}

void Java_xeno_Chr_getSerial__(JThread *thread, ChrScaleCall *arguments,
                               unsigned int *result)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    result[0] = ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET];
}

void Java_xeno_Chr_getState__(JThread *thread, ChrObjectCall *arguments,
                              u32 *failure_result)
{
    JavaField *peer_field;
    Actor *peer;
    SequenceState *entry;
    unsigned char slot;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    slot = ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET];
    entry = (SequenceState *)(actSequence + slot * SEQUENCE_STRIDE);
    *failure_result = entry->state_flags;
}

/*
 * Unimplemented native: it looks up the peer field descriptor the same way
 * every other Chr native does, but never uses arguments or the descriptor
 * to read the motion root, and never writes to failure_result.
 */
void Java_xeno_Chr_mtnGetRoot__ILxeno_util_Vector4f_(JThread *thread,
                                                     ChrScaleCall *arguments,
                                                     u32 *failure_result)
{
    /*
     * The original calls lookupClassField with jal and returns through the
     * shared epilogue instead of a sibling jump. Under this TU's compiler
     * the call, whose result is discarded, stays out of tail position only
     * inside a loop construct, which is the shape a do/while (0) statement
     * gives it.
     */
    do {
        lookupClassField(classJava_xeno_Chr,
                         loadConstString(chr_peer_string, -1), 0);
    } while (0);
}

void Java_xeno_Chr_setScale__FFF(JThread *thread, ChrVector3Call *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->scale.x = arguments->x;
    peer->scale.y = arguments->y;
    peer->scale.z = arguments->z;
}

extern u8 *classJava_xeno_util_Vector4f;
typedef struct ChrScaleResult {
    int cls;
    float y;
    float x;
    float w;
    float z;
} ChrScaleResult;
extern ChrScaleResult scale_0_007C0A40;

void Java_xeno_Chr_getScale__(JThread *thread, ChrScaleCall *arguments,
                              u32 *failure_result)
{
    JavaField *field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    field = lookupClassField(classJava_xeno_Chr,
                             loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + field->offset);
    scale_0_007C0A40.cls = *(int *)(classJava_xeno_util_Vector4f + 0x18);
    scale_0_007C0A40.w = peer->scale.x;
    scale_0_007C0A40.x = peer->scale.y;
    scale_0_007C0A40.y = peer->scale.z;
    scale_0_007C0A40.z = peer->scale.w;
    ((union { void *p; float f; } *)failure_result)->p = &scale_0_007C0A40;
}

extern void SEQ_scaleSPL(void);

void Java_xeno_Chr_scale__Lxeno_util_Spline_IZ(JThread *thread,
                                               ChrSplCall *arguments,
                                               u32 *failure_result)
{
    JavaField *peer_field;
    u8 *object;
    Actor *peer;
    ChrMoveEntry *entry;
    u8 *spline;
    u8 *sargs;
    int wait;

    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    entry = (ChrMoveEntry *)(actSequence +
        ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET] * SEQUENCE_STRIDE);
    sargs = (u8 *)entry + 0x1b8;
    entry->mask = 0xE0;
    entry->state_flags |= 0xE0;
    lookupClassField(classJava_xeno_Chr,
                     loadConstString(chr_algorithm_string, -1), 0);
    spline = arguments->spline;
    wait = arguments->wait;
    *(void **)((u8 *)entry + 0x30) = SEQ_scaleSPL;
    *(u8 **)sargs = spline;
    *(u16 *)(sargs + 6) = *(u16 *)(spline + 8);
    if (wait == 1) {
        peer->flags |= 2;
        if (thread->kind != wait && (thread->flags & 0x40) == 0)
            return;
        if ((thread->flags & 0x40) != 0) {
            thread->wait_target = peer;
            thread->wait_kind = JTHREAD_WAIT_ACTOR;
            thread->resume_frames = thread->frame_depth;
            thread->flags |= 4;
        }
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 1;
    }
}

static void CHR_sclX(int mode, JThread *thread, ChrScaleCall *arguments,
                     u32 *failure_result)
{
    JavaField *peer_field;
    JavaField *algorithm_field;
    Actor *peer;
    float *scale_source;
    SequenceState *sequence;
    SequenceScale *scale;
    u8 *object;
    int wait;

    wait = 0;
    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }

    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    /*
     * `peer` and `algorithm` are fields of the Java class xeno.Chr, so their
     * position inside the object is the offset the JVM hands back at run time
     * and not a layout this file can name. The original loads it the same way:
     * lw v1,0x10(v0); addu v1,s3,v1; lw s2,0x0(v1) at 0x002ff5b0.
     */
    peer = *(Actor **)(object + peer_field->offset);
    algorithm_field = lookupClassField(
        classJava_xeno_Chr, loadConstString(chr_algorithm_string, -1), 0);

    scale_source = &peer->scale.x;
    if ((*(u32 *)(object + algorithm_field->offset) & 1) == 0)
        scale_source = defaultOffset;

    sequence = (SequenceState *)(actSequence +
        ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET] * SEQUENCE_STRIDE);
    SEQUENCE_SCALE_HANDLER(sequence) = SEQ_scale;
    sequence->state_flags |= 0x20;
    scale = (SequenceScale *)((u8 *)sequence + SEQUENCE_SCALE_OFFSET);

    switch (mode) {
    case 0:
        scale->frames[0] = arguments->first.integer;
        scale->target[0] = arguments->second.floating + scale_source[0];
        wait = arguments->wait;
        break;
    case 1:
        scale->frames[0] = -1;
        scale->target[0] = arguments->first.floating + scale_source[0];
        scale->step[0] = arguments->second.floating / 30.0f;
        wait = arguments->wait;
        break;
    }

    if (wait == 1) {
        peer->flags |= 2;
        /*
         * `wait` is 1 here, and kind 1 is the thread getPeer_Chr creates for a
         * character (0x002f96e0): a blocking sclX only suspends the
         * character's own thread, or a thread carrying flag 0x40. The original
         * compares the two registers (beq v1,s4 at 0x002ff6c0).
         */
        if (thread->kind != wait && (thread->flags & 0x40) == 0)
            return;
        if ((thread->flags & 0x40) != 0) {
            thread->wait_target = peer;
            thread->wait_kind = JTHREAD_WAIT_ACTOR;
            thread->resume_frames = thread->frame_depth;
            thread->flags |= 4;
        }
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 1;
    }
}

void Java_xeno_Chr_sclX__FFZ(JThread *thread, ChrScaleCall *arguments,
                             u32 *failure_result)
{
    CHR_sclX(1, thread, arguments, failure_result);
}

void Java_xeno_Chr_sclX__IFZ(JThread *thread, ChrScaleCall *arguments,
                             u32 *failure_result)
{
    CHR_sclX(0, thread, arguments, failure_result);
}

static void CHR_sclY(int mode, JThread *thread, ChrScaleCall *arguments,
                     u32 *failure_result)
{
    JavaField *peer_field;
    JavaField *algorithm_field;
    Actor *peer;
    float *scale_source;
    SequenceState *sequence;
    SequenceScale *scale;
    u8 *object;
    int wait;

    wait = 0;
    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }

    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    /*
     * `peer` and `algorithm` are fields of the Java class xeno.Chr, so their
     * position inside the object is the offset the JVM hands back at run time
     * and not a layout this file can name. The original loads it the same way:
     * lw v1,0x10(v0); addu v1,s3,v1; lw s2,0x0(v1) at 0x002ff5b0.
     */
    peer = *(Actor **)(object + peer_field->offset);
    algorithm_field = lookupClassField(
        classJava_xeno_Chr, loadConstString(chr_algorithm_string, -1), 0);

    scale_source = &peer->scale.x;
    if ((*(u32 *)(object + algorithm_field->offset) & 1) == 0)
        scale_source = defaultOffset;

    sequence = (SequenceState *)(actSequence +
        ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET] * SEQUENCE_STRIDE);
    SEQUENCE_SCALE_HANDLER(sequence) = SEQ_scale;
    sequence->state_flags |= 0x40;
    scale = (SequenceScale *)((u8 *)sequence + SEQUENCE_SCALE_OFFSET);

    switch (mode) {
    case 0:
        scale->frames[1] = arguments->first.integer;
        scale->target[1] = arguments->second.floating + scale_source[1];
        wait = arguments->wait;
        break;
    case 1:
        scale->frames[1] = -1;
        scale->target[1] = arguments->first.floating + scale_source[1];
        scale->step[1] = arguments->second.floating / 30.0f;
        wait = arguments->wait;
        break;
    }

    if (wait == 1) {
        peer->flags |= 2;
        /*
         * `wait` is 1 here, and kind 1 is the thread getPeer_Chr creates for a
         * character (0x002f96e0): a blocking sclY only suspends the
         * character's own thread, or a thread carrying flag 0x40. The original
         * compares the two registers (beq v1,s4 at 0x002ff6c0).
         */
        if (thread->kind != wait && (thread->flags & 0x40) == 0)
            return;
        if ((thread->flags & 0x40) != 0) {
            thread->wait_target = peer;
            thread->wait_kind = JTHREAD_WAIT_ACTOR;
            thread->resume_frames = thread->frame_depth;
            thread->flags |= 4;
        }
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 1;
    }
}

void Java_xeno_Chr_sclY__FFZ(JThread *thread, ChrScaleCall *arguments,
                             u32 *failure_result)
{
    CHR_sclY(1, thread, arguments, failure_result);
}

void Java_xeno_Chr_sclY__IFZ(JThread *thread, ChrScaleCall *arguments,
                             u32 *failure_result)
{
    CHR_sclY(0, thread, arguments, failure_result);
}

static void CHR_sclZ(int mode, JThread *thread, ChrScaleCall *arguments,
                     u32 *failure_result)
{
    JavaField *peer_field;
    JavaField *algorithm_field;
    Actor *peer;
    float *scale_source;
    SequenceState *sequence;
    SequenceScale *scale;
    u8 *object;
    int wait;

    wait = 0;
    object = arguments->object;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 0) {
        *failure_result = 0;
        return;
    }

    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    /*
     * `peer` and `algorithm` are fields of the Java class xeno.Chr, so their
     * position inside the object is the offset the JVM hands back at run time
     * and not a layout this file can name. The original loads it the same way:
     * lw v1,0x10(v0); addu v1,s3,v1; lw s2,0x0(v1) at 0x002ff5b0.
     */
    peer = *(Actor **)(object + peer_field->offset);
    algorithm_field = lookupClassField(
        classJava_xeno_Chr, loadConstString(chr_algorithm_string, -1), 0);

    scale_source = &peer->scale.x;
    if ((*(u32 *)(object + algorithm_field->offset) & 1) == 0)
        scale_source = defaultOffset;

    sequence = (SequenceState *)(actSequence +
        ((u8 *)peer)[ACTOR_SLOT_NUMBER_OFFSET] * SEQUENCE_STRIDE);
    SEQUENCE_SCALE_HANDLER(sequence) = SEQ_scale;
    sequence->state_flags |= 0x80;
    scale = (SequenceScale *)((u8 *)sequence + SEQUENCE_SCALE_OFFSET);

    switch (mode) {
    case 0:
        scale->frames[2] = arguments->first.integer;
        scale->target[2] = arguments->second.floating + scale_source[2];
        wait = arguments->wait;
        break;
    case 1:
        scale->frames[2] = -1;
        scale->target[2] = arguments->first.floating + scale_source[2];
        scale->step[2] = arguments->second.floating / 30.0f;
        wait = arguments->wait;
        break;
    }

    if (wait == 1) {
        peer->flags |= 2;
        /*
         * `wait` is 1 here, and kind 1 is the thread getPeer_Chr creates for a
         * character (0x002f96e0): a blocking sclZ only suspends the
         * character's own thread, or a thread carrying flag 0x40. The original
         * compares the two registers (beq v1,s4 at 0x002ff6c0).
         */
        if (thread->kind != wait && (thread->flags & 0x40) == 0)
            return;
        if ((thread->flags & 0x40) != 0) {
            thread->wait_target = peer;
            thread->wait_kind = JTHREAD_WAIT_ACTOR;
            thread->resume_frames = thread->frame_depth;
            thread->flags |= 4;
        }
        thread->resume_frames = thread->frame_depth;
        thread->flags |= 1;
    }
}

void Java_xeno_Chr_sclZ__FFZ(JThread *thread, ChrScaleCall *arguments,
                             u32 *failure_result)
{
    CHR_sclZ(1, thread, arguments, failure_result);
}

/*
 * The IFZ overload forwards the same interpolation mode 1 as FFZ (addiu
 * a0,zero,1 at 0x002ffc74): the original does not give this overload its own
 * mode 0 dispatch the way CHR_sclX/CHR_sclY do.
 */
void Java_xeno_Chr_sclZ__IFZ(JThread *thread, ChrScaleCall *arguments,
                             u32 *failure_result)
{
    CHR_sclZ(1, thread, arguments, failure_result);
}

void Java_xeno_Chr_rotCNS__ILjava_lang_Object_(JThread *thread,
                                               ChrScaleCall *arguments,
                                               u32 *failure_result)
{
}

/*
 * Unimplemented native: it fetches its peer the same way every other Chr
 * native does (peer_field then peer) and does nothing else with it.
 */
void Java_xeno_Chr_rotCNS__IFFF(JThread *thread, ChrScaleCall *arguments,
                                u32 *failure_result)
{
    JavaField *peer_field;
    Actor *peer;

    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(arguments->object + peer_field->offset);
}

/*
 * Unimplemented native: it fetches its peer the same way every other Chr
 * native does (peer_field then peer) and does nothing else with it.
 */
void Java_xeno_Chr_setRotCNSParam__IFFFFF(JThread *thread,
                                          ChrScaleCall *arguments,
                                          u32 *failure_result)
{
    JavaField *peer_field;
    Actor *peer;

    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(arguments->object + peer_field->offset);
}

/*
 * Unimplemented native: it fetches its peer the same way every other Chr
 * native does (peer_field then peer) and does nothing else with it.
 */
void Java_xeno_Chr_relax__II(JThread *thread, ChrScaleCall *arguments,
                             u32 *failure_result)
{
    JavaField *peer_field;
    Actor *peer;

    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(arguments->object + peer_field->offset);
}

void Java_xeno_Chr_getFlags__(JThread *thread, ChrScaleCall *arguments,
                              unsigned int *result)
{
    JavaField *peer_field;
    Actor *peer;

    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(arguments->object + peer_field->offset);
    result[0] = peer->flags;
}

void Java_xeno_Chr_setFlags__I(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;

    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(arguments->object + peer_field->offset);
    peer->flags = arguments->first.integer;
}

void Java_xeno_Chr_setEdgeFall__I(JThread *thread, ChrIntCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;

    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(arguments->object + peer_field->offset);
    if (arguments->value == 0) {
        /* Also clears every other status_flags bit: andi v0,v0,8 at
         * 0x002ffe48, not a mask of ~8. */
        peer->status_flags &= 8;
    } else {
        peer->status_flags |= 8;
    }
}

void Java_xeno_Chr_setShadow__II(JThread *thread, ChrShadowCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;

    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(arguments->object + peer_field->offset);
    peer->flags |= 0x20;
    peer->shadow_kind = arguments->kind;
    peer->shadow_size = arguments->size;
}

INCLUDE_ASM("asm/main/nonmatchings/chr", Java_xeno_Chr_setShadow__aB);

void Java_xeno_Chr_setID__I(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;

    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(arguments->object + peer_field->offset);
    peer->data_header = UnduDataGetHeader(0, arguments->first.integer);
    peer->status_flags |= 0x1000;
}

void Java_xeno_Chr_setElevatorMode__I(JThread *thread, ChrIntCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;

    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(arguments->object + peer_field->offset);
    if (arguments->value == 0) {
        /* Also clears every other status_flags bit: andi v0,v0,0x20 at
         * 0x003000f8, not a mask of ~0x20. */
        peer->status_flags &= 0x20;
    } else {
        peer->status_flags |= 0x20;
    }
}

extern int ACT_setRelation(Actor *actor, Actor *parent, int rel, int id);
extern int ACT_setParent(Actor *actor, int type, Actor *parent, int joint, int id);
static inline void chr_set_parent(Actor *actor, Actor *parent, int rel,
                                  int joint, int id, int type)
{
    if (rel & 0x8000) {
        ACT_setRelation(actor, parent, rel, id);
        return;
    }
    ACT_setParent(actor, type, parent, joint, id);
}

void Java_xeno_Chr_setParent__Lxeno_Chr_III(JThread *thread,
                                            union { int i; void *p; } *args)
{
    JavaField *peer_field;
    u8 *object;
    Actor *actor;
    Actor *parent;
    int joint;
    int type;
    int id;

    object = args[0].p;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    joint = args[2].i;
    id = args[4].i;
    type = args[3].i;
    actor = *(Actor **)(object + peer_field->offset);
    object = args[1].p;
    parent = *(Actor **)(object + peer_field->offset);
    if (joint & 0x8000) {
        ACT_setRelation(actor, parent, joint, id);
    } else {
        ACT_setParent(actor, type, parent, joint, id);
    }
}

void Java_xeno_Chr_setMotionFlags__IZ(JThread *thread,
                                      ChrMotionFlagsCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;
    int mask;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    mask = arguments->mask;
    peer = *(Actor **)(object + peer_field->offset);
    if (arguments->enabled != 0) {
        peer->motion_flags |= mask;
    } else {
        peer->motion_flags &= ~mask;
    }
}

extern void ACT_filterGuno(Actor *actor);
extern void ACT_filterStealth(Actor *actor);

void Java_xeno_Chr_setFilter__I(JThread *thread, ChrIntCall *arguments, int *result)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;
    void (*draw)(Actor *);
    u32 flags;
    int res;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    switch (arguments->value) {
    case 0:
        peer->draw = 0;
        peer->flags &= ~0x800;
        *result = -1;
        return;
    case 1:
        peer->flags &= ~0x800;
        *result = -1;
        return;
    case 2:
        peer->flags &= ~0x800;
        *(u32 *)&peer->draw = (u32)ACT_filterGuno;
        *result = 4;
        return;
    case 3:
        peer->flags &= ~0x800;
        *(u32 *)&peer->draw = (u32)ACT_filterStealth;
        *result = 4;
        return;
    }
}

void Java_xeno_Chr_setFilterParam__aF(JThread *thread,
                                      ChrFilterParamCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    ChrFilterParamValue *value;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    value = arguments->array->value;
    peer->filter_param[0] = value->components[0];
    peer->filter_param[1] = value->components[3];
    peer->filter_param[2] = value->components[2];
    peer->filter_param[3] = value->components[1];
}

void Java_xeno_Chr_setClip__I(JThread *thread, ChrIntCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    if (arguments->value != 0) {
        peer->render_flags |= 0x200;
    } else {
        peer->render_flags &= ~0x200;
    }
}

void Java_xeno_Chr_setSymmetryY__I(JThread *thread, ChrIntCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    if (arguments->value != 0) {
        peer->render_flags |= 0x10;
    } else {
        peer->render_flags &= ~0x10;
    }
}

void Java_xeno_Chr_setSortOffset__F(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->sort_offset = arguments->first.floating;
}

typedef union ChrU { int i; float f; unsigned u; } ChrU;
typedef struct ChrPLC { u8 *object; ChrU idx; ChrU r, g, b; } ChrPLC;
typedef struct ChrPL { unsigned flags; u8 pad[0x96c]; struct { float c[4]; } l[3]; unsigned f9a0; } ChrPL;

void Java_xeno_Chr_setPointLightCol__IFFF(JThread *thread, ChrPLC *args)
{
    JavaField *peer_field;
    ChrPL *peer;
    u8 *object;

    object = args->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(ChrPL **)(object + peer_field->offset);
    if (args->idx.u < 3) {
        peer->flags |= 0x800;
        peer->l[args->idx.u].c[0] = args->r.f;
        peer->l[args->idx.u].c[1] = args->g.f;
        peer->l[args->idx.u].c[2] = args->b.f;
        peer->f9a0 |= 0x20;
    }
}

typedef struct ChrPLP { u8 *object; ChrU idx; ChrU x, y, z; } ChrPLP;
typedef struct ChrPP { unsigned flags; u8 pad[0x93c]; struct { float c[4]; } l[3]; unsigned pad2[12]; unsigned f9a0; } ChrPP;

void Java_xeno_Chr_setPointLightPos__IFFF(JThread *thread, ChrPLP *args)
{
    JavaField *peer_field;
    ChrPP *peer;
    u8 *object;

    lookupClassField(classJava_xeno_Chr,
                     loadConstString(chr_peer_string, -1), 0);
    object = args->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(ChrPP **)(object + peer_field->offset);
    if (args->idx.u < 3) {
        peer->flags |= 0x800;
        peer->l[args->idx.u].c[0] = args->x.f;
        peer->l[args->idx.u].c[1] = args->y.f;
        peer->l[args->idx.u].c[2] = args->z.f;
        peer->f9a0 |= 0x20;
    }
}

void Java_xeno_Chr_setPointLightReset__(JThread *thread, ChrTalkCall *arguments)
{
    JavaField *peer_field;
    u8 *peer;
    u8 *object;
    u8 *p;
    int i;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(u8 **)(object + peer_field->offset);
    p = (peer + 0x970);
    for (i = 2; i >= 0; i--) {
        __asm__ volatile("sq $0, 0(%0)" : : "r"(p) : "memory");
        p += 16;
    }
    *(int *)(peer + 0x9A0) &= ~0x20;
}

void Java_xeno_Chr_talkto__Ljava_lang_String_(JThread *thread,
                                              ChrTalkCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->talk_message = arguments->message->value->message_id;
}

void Java_xeno_Chr_touchto__Ljava_lang_String_(JThread *thread,
                                               ChrTalkCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->touch_message = arguments->message->value->message_id;
}

typedef struct ChrKids { u8 pad[0x900]; int count; u8 *kids[1]; } ChrKids;
typedef struct ChrPeerCall { u8 *object; void *flag; } ChrPeerCall;

void Java_xeno_Chr_childGetPeer__II(JThread *thread, ChrPeerCall *arguments,
                                    void **result)
{
    JavaField *peer_field;
    ChrKids *peer;
    u8 *object;
    int i;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(void **)(object + peer_field->offset);
    *result = 0;
    if (arguments->flag != (void *)0x01000000)
        return;
    {
        for (i = 0; i < peer->count; i++) {
            if (peer->kids[i][0x82] == 1) {
                *result = peer->kids[i];
                break;
            }
        }
    }
}

extern void ACT_updateSequence(Actor *actor);
typedef struct ChrSetPeerCall { u8 *object; u8 *peer; } ChrSetPeerCall;

void Java_xeno_Chr_setPeer__Ljava_lang_Object_(JThread *thread,
                                               ChrSetPeerCall *arguments)
{
    JavaField *peer_field;
    u8 *peer;
    u8 *object;
    u8 *seq;
    int offset;

    peer = arguments->peer;
    object = arguments->object;
    if (peer != 0) {
        peer_field = lookupClassField(classJava_xeno_Chr,
                                      loadConstString(chr_peer_string, -1), 0);
        offset = peer_field->offset;
        *(void **)(peer + 4) = ACT_updateSequence;
        *(short *)(peer + 0x88) = 0;
        *(u8 **)(object + offset) = peer;
        peer[0x81] = 0;
        *(u8 **)(peer + 0x4c0) = object;
        seq = actSequence + peer[0x80] * 0x260;
        *(int *)(seq + 0) = 0;
        *(int *)(seq + 4) = 0;
        *(int *)(seq + 0xc) = 0;
        *(int *)(seq + 0x14) = 0;
        *(int *)(seq + 0x18) = 0;
        *(int *)(seq + 0x1c) = 0;
        *(int *)(seq + 0x20) = 0;
        *(int *)(seq + 0x24) = 0;
        *(int *)(seq + 0x28) = 0;
        *(int *)(seq + 0x2c) = 0;
        *(int *)(seq + 0x30) = 0;
    }
}

void Java_xeno_Chr_dispRadar__Z(JThread *thread, ChrBoolCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    if (arguments->flag != 0) {
        peer->flags &= ~0x80;
    } else {
        peer->flags |= 0x80;
    }
}

void Java_xeno_Chr_look_camera__(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->look_mode = 2;
}

void Java_xeno_Chr_look_char__Ljava_lang_Object_(JThread *thread,
                                                 ChrWeaponCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    Actor *target;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    target = *(Actor **)(arguments->other + peer_field->offset);
    peer->look_mode = 4;
    peer->look_target = target;
}

void Java_xeno_Chr_look_unit__Ljava_lang_Object_(JThread *thread,
                                                 ChrWeaponCall *arguments)
{
    JavaField *peer_field;
    JavaField *unit_peer_field;
    Actor *peer;
    void *target;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    unit_peer_field = lookupClassField(classJava_xeno_Unit,
                                       loadConstString(chr_peer_string, -1), 0);
    target = *(void **)(arguments->other + unit_peer_field->offset);
    peer->look_mode = 5;
    peer->look_target = target;
}

void Java_xeno_Chr_look_default__(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->look_mode = 1;
    peer->look_eye_control = 0;
    peer->look_target = 0;
    __asm__ volatile("sq $0, 0(%0)" : : "r"((u8 *)peer + 0x600) : "memory");
    __asm__ volatile("sq $0, 0(%0)" : : "r"((u8 *)peer + 0x610) : "memory");
    __asm__ volatile("sq $0, 0(%0)" : : "r"((u8 *)peer + 0x620) : "memory");
    __asm__ volatile("sq $0, 0(%0)" : : "r"((u8 *)peer + 0x640) : "memory");
    __asm__ volatile("sq $0, 0(%0)" : : "r"((u8 *)peer + 0x650) : "memory");
    __asm__ volatile("sq $0, 0(%0)" : : "r"((u8 *)peer + 0x660) : "memory");
    peer->look_speed = 1.0f;
    peer->look_eye_speed = 1.0f;
}

void Java_xeno_Chr_look_point__FFF(JThread *thread, SceneObject *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;
    float x;
    float y;
    float z;

    object = arguments[0];
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->look_mode = 3;
    __builtin_memcpy(&x, &arguments[1], sizeof(x));
    peer->look_point[0] = x;
    __builtin_memcpy(&y, &arguments[2], sizeof(y));
    peer->look_point[1] = y;
    __builtin_memcpy(&z, &arguments[3], sizeof(z));
    peer->look_point[2] = z;
}

extern float D_004D841C;

void Java_xeno_Chr_look_eye_set__FF(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;
    float k;
    float x;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    x = arguments->first.floating;
    k = D_004D841C;
    peer = *(Actor **)(object + peer_field->offset);
    ((float *)((u8 *)peer + 0x660))[0] = x * k / 180.0f;
    ((float *)((u8 *)peer + 0x660))[1] = arguments->second.floating * k / 180.0f;
    peer->look_eye_control = 14;
}

void Java_xeno_Chr_look_eye_control__I(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->look_eye_control = arguments->first.integer;
}

void Java_xeno_Chr_look_speed__F(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->look_speed = arguments->first.floating;
}

void Java_xeno_Chr_look_eye_speed__F(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->look_eye_speed = arguments->first.floating;
}

void Java_xeno_Chr_renderCommand__I(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->render_command = arguments->first.integer;
}

void Java_xeno_Chr_shadow_clip_scale__F(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->shadow_clip_scale = arguments->first.floating;
}

void Java_xeno_Chr_shadow_map_id__I(JThread *thread,
                                    ChrShadowMapIdCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;
    int *count;
    int index;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    if (peer->shadow_map < 8) {
        count = &peer->shadow_map;
        index = *count;
        *count = index + 1;
        peer->shadow_map_ids[index] = arguments->id;
    }
}

void Java_xeno_Chr_shadow_map_reset__(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->shadow_map = 0;
}

void Java_xeno_Chr_hairStop__II(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->hair_stop_a = arguments->first.integer;
    peer->hair_stop_b = arguments->second.integer;
}

void Java_xeno_Chr_pixelAlpha__I(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->pixel_alpha = arguments->first.integer;
}

typedef struct ChrAlphaPartsView {
    u8 pad[0x9ac];
    int count;
    u16 parts[4][2];
} ChrAlphaPartsView;

void Java_xeno_Chr_pixelAlphaParts__II(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    ChrAlphaPartsView *peer;
    u8 *object;
    int n;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(ChrAlphaPartsView **)(object + peer_field->offset);
    n = peer->count;
    if (n < 4) {
        peer->parts[n][0] = arguments->second.integer;
        peer->parts[n][1] = arguments->first.integer;
        peer->count = n + 1;
    }
}

void Java_xeno_Chr_pixelAlphaPartsReset__(JThread *thread, ChrScaleCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    peer->pixel_alpha_parts = 0;
}

void Java_xeno_Chr_setMotNoUpdate__I(JThread *thread, ChrIntCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    switch (arguments->value) {
    case 0:
        peer->flags &= ~0x400;
        peer->render_flags &= ~0x100;
        break;
    case 1:
        peer->flags |= 0x400;
        peer->render_flags &= ~0x100;
        break;
    case 2:
        peer->flags &= ~0x400;
        peer->render_flags |= 0x100;
        break;
    }
}

void Java_xeno_Chr_setWeaponR__Lxeno_Chr_(JThread *thread, ChrWeaponCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    Actor *other_peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    object = arguments->other;
    other_peer = *(Actor **)(object + peer_field->offset);
    ACT_setArms(peer, other_peer, 0x108, 2);
}

void Java_xeno_Chr_resetWeaponR__Lxeno_Chr_(JThread *thread, ChrWeaponCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    Actor *other_peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    object = arguments->other;
    other_peer = *(Actor **)(object + peer_field->offset);
    ACT_resetArms(peer, other_peer, 0x108);
}

void Java_xeno_Chr_resetHand__(void)
{
}

void Java_xeno_Chr_resetEnv__(void)
{
}

void Java_xeno_Chr_ignoreShape__I(JThread *thread, ChrIntCall *arguments)
{
    JavaField *peer_field;
    Actor *peer;
    u8 *object;

    object = arguments->object;
    peer_field = lookupClassField(classJava_xeno_Chr,
                                  loadConstString(chr_peer_string, -1), 0);
    peer = *(Actor **)(object + peer_field->offset);
    if (arguments->value != 0) {
        peer->render_flags |= 0x400;
    } else {
        peer->render_flags &= ~0x400;
    }
}

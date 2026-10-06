#include "common.h"
#include "act_2.h"

/* Types recovered upstream (origin/master); kept for reference. */
/* One of the eight Java camera records embedded at `tcamera + n * 0x12c0`.
 * TCAMERA_update establishes the four 0x4a0 channel strides and TCAMERA_init
 * establishes the 0x12c0 record stride. The channel union names the fields
 * proved by the channel callers; the rest stays explicitly unmodeled storage
 * until another access gives it meaning. */
typedef struct CameraChannelSpline {
    unsigned char unmodeled_00[4];
    unsigned short weight_mode;
    unsigned short first_key;
    unsigned short last_key;
    unsigned short sample_count;
    unsigned int component_count;
    const float *samples;
} CameraChannelSpline;

typedef union CameraChannelData {
    CameraChannelSpline spline;
    struct {
        Vector4 offset;
        int mode;
        float **constraint;
    } constant;
    struct {
        unsigned char unmodeled_00[0x18];
        unsigned char unmodeled_18[0x4a0 - 0x18];
    } unmodeled;
} CameraChannelData;

typedef struct CameraRecord {
    unsigned int class_header;       /* +0x00, passed to Java as the object */
    int camera_id;                   /* +0x04, selects the studio camera */
    unsigned int unmodeled_08;
    int mode[4];                     /* +0x0c, TCAMERA_update dispatch */
    int frame[4];                    /* +0x1c, per-channel frame counters */
    float debug_cursor;              /* +0x2c, edited by the camera cursor */
    CameraChannelData translate;     /* +0x30 */
    CameraChannelData view;          /* +0x4d0 */
    CameraChannelData roll;          /* +0x970 */
    CameraChannelData fov;           /* +0xe10 */
    SceneObject peer;                /* +0x12b0, seeded by Camera_start */
    float initial_fov;               /* +0x12b4, seeded by Camera_create */
    unsigned char unmodeled_12b8[8]; /* +0x12b8 to the established 0x12c0 stride */
} CameraRecord;


extern Actor actor[64];

/*
 * JNT_getMoveElement (main/tu261, src/main/jnt.c) is not yet recovered as C.
 * It dereferences its argument (main:0x00313b50, addiu v1,a0,16; lhu
 * a3,8(a0)), so the parameter is a pointer.
 */
extern int JNT_getMoveElement(void *move);

/*
 * FCV2_checkData is not yet recovered as C. It dereferences its argument to
 * compare a signature word (main:0x0030d8c8..0x0030d8d8), so the parameter is
 * a pointer.
 */
extern int FCV2_checkData(void *data);

typedef unsigned int ActMatrixRow __attribute__((mode(TI)));
typedef struct ActMatrixView {
    unsigned char unmodeled_000[0x790];
    unsigned char rootElement[0x824 - 0x790];
    ActMatrixRow *matrix;
    ActMatrixRow *parentMatrix;
    unsigned char unmodeled_82c[0x8D0 - 0x82C];
    void *jointMatrix;
    unsigned char unmodeled_8d4[0x8D8 - 0x8D4];
    unsigned char *model;
} ActMatrixView;
extern void CUR_MATRIX_Unit4s(void);
extern void CUR_MATRIX_SetT(const Vector4 *translation);
extern void CUR_MATRIX_Rx4s(const int *angle);
extern void CUR_MATRIX_Ry4s(const int *angle);
extern void CUR_MATRIX_Rz4s(const int *angle);
extern void CUR_MATRIX_Sxyz4s(const Vector4 *scale);
extern void CUR_MATRIX_Get(void *matrix);
extern void JNT_setFlags(int flags);
extern void JNT_setModel(void *matrix, unsigned char *model);
extern void JNT_setMatrix(void *matrix_buffer);
extern void JNT_resetMatrix(void *work, int count);
static inline void ACT_resetJointMatrix(unsigned char *root)
{
    *(int *)0x7000081C = 0;
    *(int *)0x7000002C = 0;
    *(int *)(root + 0x70) = 0;
    *(void **)0x70000000 = root;
    *(int *)0x700007FC = 0;
    JNT_resetMatrix((void *)0x70000000, *(int *)0x70000030);
}

void ACT_resetMatrix(Actor *actor)
{
    unsigned char *model = ((ActMatrixView *)actor)->model;
    unsigned char *matrix = (unsigned char *)((ActMatrixView *)actor)->matrix;
    unsigned char *work;
    int count;
    void *jointMatrix = ((ActMatrixView *)actor)->jointMatrix;

    if (model != 0 && matrix != 0) {
        CUR_MATRIX_Unit4s();
        CUR_MATRIX_SetT(&actor->position);
        CUR_MATRIX_Rx4s((int *)&actor->rotation.x);
        CUR_MATRIX_Ry4s((int *)&actor->rotation.y);
        CUR_MATRIX_Rz4s((int *)&actor->rotation.z);
        CUR_MATRIX_Sxyz4s(&actor->scale);
        CUR_MATRIX_Get(matrix);
        __asm__ __volatile__(
            "lq $2, 0(%1)\n\t"
            "sq $2, 0(%0)\n\t"
            "lq $2, 16(%1)\n\t"
            "sq $2, 16(%0)\n\t"
            "lq $2, 32(%1)\n\t"
            "sq $2, 32(%0)\n\t"
            "lq $2, 48(%1)\n\t"
            "sq $2, 48(%0)"
            :
            : "r"(((ActMatrixView *)actor)->parentMatrix), "r"(matrix)
            : "$2", "memory");
        JNT_setFlags(0x8000);
        work = ((ActMatrixView *)actor)->rootElement;
        JNT_setModel(jointMatrix, model);
        JNT_setMatrix(((ActMatrixView *)actor)->matrix);
        *(int *)0x7000081C = 0;
        count = *(int *)0x70000030;
        *(int *)0x7000002C = 0;
        *(int *)(work + 0x70) = 0;
        *(void **)0x70000000 = work;
        *(int *)0x700007FC = 0;
        JNT_resetMatrix((void *)0x70000000, count);
        JNT_setMatrix(((ActMatrixView *)actor)->parentMatrix);
        ACT_resetJointMatrix(work);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/act_2", ACT_resetParent);

extern void JNT_initProducer(void *producer);
extern void JNT_addConsumer(void *producer, int a, int (*func)(), void *data);
extern int JC_copyChain();
extern int JC_setParent();
typedef struct ActParentSlot {
    short type;
    short joint;
    void *mtx;
} ActParentSlot;
typedef struct ActParentView {
    unsigned char pad[0x6e0];
    signed char used[8];
} ActParentView;

int ACT_setParent(Actor *actor, int type, Actor *parent, int joint, int id)
{
    char *a = (char *)actor;
    char *p = (char *)parent;
    int n;
    int i;
    int slot;
    void *prod;
    ActParentSlot *s;

    *(Actor **)(a + 0x8fc) = parent;
    n = *(int *)(p + 0x900);
    *(int *)(p + 0x900) = n + 1;
    *(Actor **)(p + 0x904 + n * 4) = actor;
    if (id & 1) {
        *(int *)(a + 0x8d4) = *(int *)(p + 0x8d4);
        if (actor->flags & 0x4000) {
            *(int *)(a + 0x894) = *(int *)(p + 0x8d4);
        }
    }
    if (*(int *)(p + 0x698) < 8) {
        slot = -1;
        i = 0;
        if (((ActParentView *)p)->used[0] == 0) {
            slot = 0;
        } else {
        loop:
            i++;
            if (i < 8) {
                if (((ActParentView *)p)->used[i] != 0) {
                    goto loop;
                }
                slot = i;
            }
        }
        if (slot >= 0) {
            ((ActParentView *)p)->used[slot] = 1;
            *(int *)(a + 0x69c) = slot;
            s = (ActParentSlot *)(p + slot * 8 + 0x6a0);
            prod = a + 0x790;
            s->type = type;
            s->joint = joint;
            *(int *)(p + 0x698) += 1;
            s->mtx = p + 0x824;
            JNT_initProducer(prod);
            if (id & 2) {
                JNT_addConsumer(prod, 0, JC_copyChain, s);
            } else {
                JNT_addConsumer(prod, 0, JC_setParent, s);
            }
        }
    }
    return (int)actor;
}

INCLUDE_ASM("asm/main/nonmatchings/act_2", ACT_setParent2);

int ACT_jointGetMoveElementID(Actor *actor)
{
    if (actor->moveElementId < 0) {
        if (actor->move != 0) {
            actor->moveElementId = JNT_getMoveElement(actor->move);
        } else {
            actor->moveElementId = 0;
        }
    }
    return actor->moveElementId;
}

extern void *JNT_getAccessories(void *jnt);

int ACT_jointGetAccessories(Actor *actor, unsigned int id)
{
    int ret = -1;
    int *acc;

    if (*(int **)((char *)actor + 0x7f8) == 0) {
        *(int **)((char *)actor + 0x7f8) = JNT_getAccessories(actor->move);
    }
    acc = *(int **)((char *)actor + 0x7f8);
    if (acc != 0 && id < 41) {
        ret = acc[id + 8];
    }
    return ret;
}

extern void ACT_resetParent(Actor *actor, Actor *other);
extern void ACT_setHumanHand(Actor *actor, int hand);

/*
 * acc_id 0x108 is the only accessory kind reset here: ACT_resetParent(actor,
 * other) drops the parent link the accessory got from ACT_setArms, then
 * ACT_setHumanHand(other, 0) puts the character's own hand back to its
 * default (main:0x00306ae8..0x00306b1c).
 */
void ACT_resetArms(Actor *actor, Actor *other, int acc_id)
{
    if (acc_id == 0x108) {
        ACT_resetParent(actor, other);
        ACT_setHumanHand(other, 0);
    }
}

int ACT_setArms(Actor *actor, Actor *parent, int acc_id, int id)
{
    int joint;
    int result;

    if ((acc_id & 0xFF00) == 0) {
        joint = ACT_jointGetAccessories(parent, acc_id);
        if (joint > 0) {
            result = ACT_setParent(actor, 0, parent, joint, id);
        } else {
            result = 0;
        }
    } else if (acc_id == 0x108) {
        result = ACT_setParent(actor, 2, parent, 0x42, id | 2);
        ACT_setHumanHand(parent, 0x8007);
    } else {
        return (int)actor;
    }
    return result;
}

extern int JC_copyChain();
extern int JC_face();
extern int JC_setParent();
extern int ACT_setParent2(Actor *actor, int type, Actor *parent, int joint, int id, void *func);
extern void ACT_setVisible(Actor *actor, int part, int visible);

int ACT_setRelation(Actor *actor, Actor *parent, int rel, int id)
{
    void *func;
    int type;
    int joint;

    if (rel & 0x8000) {
        switch (rel & 0x7F00) {
        case 0:
            joint = ACT_jointGetAccessories(parent, rel & 0x7FFF);
            type = 0;
            func = JC_setParent;
            break;
        case 0x100:
            func = JC_copyChain;
            joint = 0x42;
            type = 2;
            ACT_setHumanHand(parent, 0x8007);
            break;
        case 0x200:
            func = JC_face;
            joint = 0x30;
            type = 2;
            ACT_setVisible(parent, 0x46414345, 0);
            break;
        default:
            joint = 0;
            func = JC_setParent;
            type = 0;
            break;
        }
    } else {
        joint = rel & 0x7FFF;
        func = JC_setParent;
        type = 0;
    }
    return ACT_setParent2(actor, type, parent, joint, id, func);
}

extern int ACT_setParent(Actor *actor, int type, Actor *parent, int joint, int id);
extern void ACT_setVisible(Actor *actor, int part, int visible);

/*
 * Attaches `actor` to `parent` at joint 0x30 (type 2, id `faceId`); on
 * success (main:0x00306d40..0x00306d64) it hides the base model's own FACE
 * part with ACT_setVisible(parent, 0x46414345, 0) - 0x46414345 is "FACE"
 * read from most- to least-significant byte, matching lui 0x4641/ori 0x4345
 * at 0x00306d44/0x00306d5c.
 */
int ACT_setFace(Actor *actor, Actor *parent, int faceId)
{
    int result;

    result = ACT_setParent(actor, 2, parent, 0x30, faceId);
    if (result != 0) {
        ACT_setVisible(parent, 0x46414345, 0);
    }
    return result;
}

void *ACT_animGetUserData(Actor *actor)
{
    return actor->animUserData;
}

int ACT_animCheckData(Actor *actor)
{
    return FCV2_checkData(actor->animData);
}

INCLUDE_ASM("asm/main/nonmatchings/act_2", ACT_updateMotionCore);

extern void ACT_updateMotionCore(Actor *actor, int pause);

void ACT_updateMotion(Actor *actor)
{
    ACT_updateMotionCore(actor, 0);
}

void ACT_updateMotionPause(Actor *actor)
{
    ACT_updateMotionCore(actor, 1);
}

INCLUDE_ASM("asm/main/nonmatchings/act_2", ACT_updateMotionSub);

extern void nmlModelSetZwrite(int enabled);
extern void nmlModelSetStencil(int enabled);
extern void nmlModelSetToumei(int enabled);
extern void nmlModelSetAlpha(u64 alpha);
extern void nmlModelSetTransparency(float transparency);
extern void nmlModelSetFogCol(const float *color);
extern void nmlModelSetFogDist(float near, float far, float min, float max);
extern void nmlModelSetPointLight(unsigned int idx, void *pos, void *col);
typedef struct ActDrawEnv {
    float fog_dist[4];   /* +0x00 */
    float fog_col[4];    /* +0x10 */
    float light_pos[3][4]; /* +0x20 */
    float light_col[3][4]; /* +0x50 */
    int flags;           /* +0x80 */
    int pad84[7];
    float transparency;  /* +0xa0 */
    int padA4[5];
    u64 alpha;           /* +0xb8 */
} ActDrawEnv;

void ACT_setDrawEnv(Actor *actor)
{
    ActDrawEnv *env = (ActDrawEnv *)((char *)actor + 0x920);
    int flags = env->flags;
    int i;

    nmlModelSetZwrite(flags & 2);
    nmlModelSetStencil(flags & 4);
    if (flags & 1) {
        nmlModelSetToumei(flags & 1);
        nmlModelSetAlpha(env->alpha);
        nmlModelSetTransparency(env->transparency);
    }
    if (flags & 8) {
        nmlModelSetFogCol(((ActDrawEnv *)((char *)actor + 0x920))->fog_col);
        nmlModelSetFogDist(env->fog_dist[0], env->fog_dist[1], env->fog_dist[2], env->fog_dist[3]);
    }
    if (flags & 0x20) {
        for (i = 0; i < 3; i++) {
            nmlModelSetPointLight(i, env->light_pos, env->light_col);
            env = (ActDrawEnv *)((char *)env + 0x10);
        }
    }
}

extern void ACT_modelDrawSub(void *a);

void ACT_modelDraw(void *arg0)
{
    int i;
    int *p;
    int *q;
    int n;
    if (*(int *)((char *)arg0 + 0x8fc) == 0) {
        ACT_modelDrawSub(arg0);
        if (!(*(unsigned int *)arg0 & 0x1008)) {
            n = *(int *)((char *)arg0 + 0x900);
            if (n > 0) {
                q = (int *)((char *)arg0 + 0x904);
                i = 0;
                if (i < n) {
                    p = q;
                    do {
                        ACT_modelDrawSub((void *)*p);
                        p++;
                        i++;
                    } while (i < *(int *)((char *)arg0 + 0x900));
                }
            }
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/act_2", ACT_modelDrawSub);

extern int MDL_create(void *model, const void *resource);
typedef struct ActModelHandle {
    unsigned char unmodeled_0[0x54];
    const char *texture; /* +0x54: the actor + 0x8d4 argument */
    void *parts;         /* +0x58: pointed at the move's root element */
} ActModelHandle;
typedef struct ActModelWrapperView {
    u32 flags; /* +0x00 */
    unsigned char unmodeled_4[0x840 - 4];
    ActModelHandle model;
    unsigned char unmodeled_89c[0x8d0 - 0x89c];
    void *modelResource; /* +0x8d0 */
    const char *texture; /* +0x8d4 */
    void *move;          /* +0x8d8 */
} ActModelWrapperView;

void ACT_setModelWrapper(Actor *actor)
{
    ActModelWrapperView *view = (ActModelWrapperView *)actor;
    void *root;
    ActModelHandle *model = &view->model;
    void *move;
    void *resource = view->modelResource;
    const char *texture;
    unsigned int sum;

    move = view->move;
    texture = view->texture;
    if (resource != 0) {
        if (move == 0) {
            MDL_create(model, resource);
            return;
        }
        MDL_create(model, resource);
        root = JNT_getRootElement(move);
        sum = *(unsigned short *)(move + 8) + *(unsigned short *)(move + 10);
        model->texture = texture;
        view->flags |= 0x4000;
        model->parts = (void *)((int)root + (sum << 6));
    }
}

INCLUDE_ASM("asm/main/nonmatchings/act_2", ACT_loadResource);

/* main:0x00307c38 is `jr ra` / nop: no argument is read and no state changes. */
void ACT_initMTNResource(void)
{
}

/* main:0x00307c40 is `jr ra` / nop: no argument is read and no state changes. */
void ACT_resourceInit(void)
{
}

INCLUDE_ASM("asm/main/nonmatchings/act_2", ACT_initMotion);

extern float D_004D8478;
extern void *ANM_reset(void *anim, void *data);
extern void JNT_setModelMatrix(void);
typedef struct ActAnimView {
    u32 flags;
    float time;
    float speed;
    float frame;
    float count;
    unsigned short current;
    short next;
    unsigned char unk18[0x22 - 0x18];
    short loop;
    unsigned char unk24[0x2C - 0x24];
    void *data;
    void *user;
    void *extra;
} ActAnimView;

void ACT_setMotion(Actor *actor, int motion)
{
    ActAnimView *anim = (ActAnimView *)&actor->animSlot;
    void *model;
    void *jointMatrix;

    if (motion == (int)0x80000000) {
        ACT_resetMatrix(actor);
        anim->extra = 0;
        anim->data = 0;
        anim->user = 0;
        anim->flags &= 0xDFFFEFF6;
        anim->speed = D_004D8478;
        model = actor->move;
        anim->current = 0;
        anim->time = 0.0f;
        anim->frame = 0.0f;
        anim->count = 0.0f;
        jointMatrix = ((ActMatrixView *)actor)->jointMatrix;
        JNT_setFlags(0x8000);
        JNT_setModel(jointMatrix, model);
        JNT_setMatrix(((ActMatrixView *)actor)->matrix);
        JNT_setModelMatrix();
        return;
    }
    if (anim->current != motion || (anim->flags & 0x20000000)) {
        anim->current = motion & 0x7FF;
        anim->next = -1;
        anim->loop = 0;
        ANM_reset(anim, actor->animPackTables[(motion >> 8) & 7]);
        anim->flags = (anim->flags & 0xDFFFFFF6) | 0x10000;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/act_2", ACT_setMotion2);

extern void *PACK_getEntry(void *table, int id);

void *ACT_animGetData(Actor *actor, unsigned int dataId)
{
    dataId &= 0x7ff;
    return PACK_getEntry(actor->animPackTables[dataId >> 8], dataId & 0xff);
}

extern void ANM_getEntry(ActorAnimSlot *slot, void *entry);

void ACT_animGetCurrent(Actor *actor)
{
    ActorAnimSlot *slot = &actor->animSlot;

    ANM_getEntry(slot, actor->animPackTables[(slot->currentDataId >> 8) & 7]);
}

INCLUDE_ASM("asm/main/nonmatchings/act_2", ACT_loadMotion);

void ACT_initExMotion(Actor *target, unsigned int slot, void *motion)
{
    int i;

    if (target == 0) {
        for (i = 0; i < 64; i++) {
            actor[i].animPackTables[slot] = motion;
        }
        return;
    }

    target->animPackTables[slot] = motion;
}

extern void MDL_setGroupVisible(void *model, int part, int visible);
extern void MDL_setVisible(void *model, int part, int visible);

void ACT_setVisible(Actor *actor, int part, int visible)
{
    if ((unsigned int)part >= 128) {
        MDL_setGroupVisible(actor->model, part, visible);
    } else {
        MDL_setVisible(actor->model, part, visible);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/act_2", ACT_setHumanHand);

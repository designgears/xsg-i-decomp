#include "common.h"

#include "act_2.h"

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

extern CameraRecord tcamera[8];


extern void ACT_resetParent(Actor *actor, Actor *other);

extern void ACT_setHumanHand(Actor *actor, int hand);

/*
 * acc_id 0x108 is the only accessory kind reset here: ACT_resetParent(actor,
 * other) drops the parent link the accessory got from ACT_setArms, then
 * ACT_setHumanHand(other, 0) puts the character's own hand back to its
 * default (main:0x00306ae8..0x00306b1c).
 */

extern Actor *ACT_setParent(Actor *actor, int type, Actor *parent, int joint, int id);

extern void ACT_setVisible(Actor *actor, int part, int visible);

/*
 * Attaches `actor` to `parent` at joint 0x30 (type 2, id `faceId`); on
 * success (main:0x00306d40..0x00306d64) it hides the base model's own FACE
 * part with ACT_setVisible(parent, 0x46414345, 0) - 0x46414345 is "FACE"
 * read from most- to least-significant byte, matching lui 0x4641/ori 0x4345
 * at 0x00306d44/0x00306d5c.
 */

extern void ACT_updateMotionCore(Actor *actor, int pause);


/* main:0x00307c38 is `jr ra` / nop: no argument is read and no state changes. */

/* main:0x00307c40 is `jr ra` / nop: no argument is read and no state changes. */

extern void *PACK_getEntry(void *table, int id);

extern void ANM_getEntry(ActorAnimSlot *slot, void *entry);

extern void MDL_setGroupVisible(void *model, int part, int visible);

extern void MDL_setVisible(void *model, int part, int visible);

extern unsigned int tblHand[9];


struct JntProducer;

extern void JNT_initProducer(struct JntProducer *producer);

extern void JNT_addConsumer(struct JntProducer *producer, int order,
                            void (*filter)(void *work, void *parameter, int flags), void *parameter);

extern void JC_setParent(void *work, void *parameter, int flags);

extern void JC_copyChain(void *work, void *parameter, int flags);

struct JntMoveResource;


extern void *JNT_getAccessories(struct JntMoveResource *move);

extern void JC_face(void *work, void *parameter, int flags);

extern Actor *ACT_setParent2(Actor *actor, int type, Actor *parent, int joint, int flags,
                            void (*filter)(void *work, void *parameter, int flags));


extern void nmlModelSetZwrite(int enabled);

extern void nmlModelSetStencil(int enabled);

extern void nmlModelSetToumei(int enabled);

extern void nmlModelSetAlpha(unsigned long long alpha);

extern void nmlModelSetTransparency(float transparency);

extern void nmlModelSetFogCol(float *color);

extern void nmlModelSetFogDist(float nearDistance, float farDistance, float nearIntensity, float farIntensity);

extern void nmlModelSetPointLight(int index, float *position, float *color);

extern int MDL_create(void *model, void *resource);

extern void *JNT_getRootElement(void *joint);

/* main:0x00307c38 is `jr ra` / nop: no argument is read and no state changes. */

/* main:0x00307c40 is `jr ra` / nop: no argument is read and no state changes. */

extern void ACT_resetMatrix(Actor *actor);

extern void JNT_setFlags(int flags);

extern void JNT_setModel(void *model, void *move);

extern void JNT_setMatrix(void *matrices);

extern void JNT_setModelMatrix(void);

extern void *ANM_reset(ActorAnimSlot *slot, void *pack);

extern unsigned int GameLoopState[];

extern int RES_loadFile(int command, int callback, int resource_id, int flags);

extern int MDL_setNameVisible(void *model, const void *pattern, int visible);

enum { HAND_NAME, HAND_SUFFIX, HAND_NAME_MASK, HAND_SUFFIX_MASK };

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

void ACT_resetParent(Actor *actor, Actor *parent)
{
    int childIndex;
    int followingIndex;
    void *producer = actor->jointProducer;

    JNT_initProducer(producer);
    for (childIndex = 0; childIndex < parent->childCount; childIndex++) {
        if (parent->children[childIndex] == actor) {
            if (actor->attachmentIndex >= 0) {
                parent->attachmentUsed[actor->attachmentIndex] = 0;
                parent->attachmentCount--;
            }
            actor->attachmentIndex = -1;
            for (followingIndex = childIndex; followingIndex < parent->childCount - 1; followingIndex++) {
                parent->children[followingIndex] = parent->children[followingIndex + 1];
            }
            parent->childCount--;
            break;
        }
    }
    actor->parent = 0;
}

Actor *ACT_setParent(Actor *actor, int type, Actor *parent, int joint, int flags)
{
    int slot;
    int slotIndex;
    void *producer;
    ActorAttachment *attachment;

    actor->parent = parent;
    parent->children[parent->childCount] = actor;
    parent->childCount++;
    if ((flags & 1) != 0) {
        actor->matrixResource = parent->matrixResource;
        if ((actor->flags & 0x4000) != 0) {
            actor->model[0].model.matrixResource = parent->matrixResource;
        }
    }
    if (parent->attachmentCount < 8) {
        slot = -1;
        for (slotIndex = 0; slotIndex < 8; slotIndex++) {
            if (parent->attachmentUsed[slotIndex] == 0) {
                slot = slotIndex;
                break;
            }
        }
        if (slot >= 0) {
            parent->attachmentUsed[slot] = 1;
            actor->attachmentIndex = slot;
            attachment = &parent->attachments[slot];
            producer = actor->jointProducer;
            attachment->type = type;
            attachment->joint = joint;
            parent->attachmentCount++;
            attachment->matrix = &parent->jointMatrices;
            JNT_initProducer(producer);
            if ((flags & 2) != 0) {
                JNT_addConsumer(producer, 0, JC_copyChain, attachment);
            } else {
                JNT_addConsumer(producer, 0, JC_setParent, attachment);
            }
        }
    }
    return actor;
}

Actor *ACT_setParent2(Actor *actor, int type, Actor *parent, int joint, int flags,
                      void (*filter)(void *work, void *parameter, int flags))
{
    int slot;
    int slotIndex;
    void *producer;
    ActorAttachment *attachment;

    actor->parent = parent;
    parent->children[parent->childCount] = actor;
    parent->childCount++;
    if ((flags & 1) != 0) {
        actor->matrixResource = parent->matrixResource;
        if ((actor->flags & 0x4000) != 0) {
            actor->model[0].model.matrixResource = parent->matrixResource;
        }
    }
    if (parent->attachmentCount < 8) {
        slot = -1;
        for (slotIndex = 0; slotIndex < 8; slotIndex++) {
            if (parent->attachmentUsed[slotIndex] == 0) {
                slot = slotIndex;
                break;
            }
        }
        if (slot >= 0) {
            parent->attachmentUsed[slot] = 1;
            actor->attachmentIndex = slot;
            attachment = &parent->attachments[slot];
            producer = actor->jointProducer;
            attachment->type = type;
            attachment->joint = joint;
            parent->attachmentCount++;
            attachment->matrix = &parent->jointMatrices;
            JNT_addConsumer(producer, 0, filter, attachment);
        }
    }
    return actor;
}

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

int ACT_jointGetAccessories(Actor *actor, unsigned int accessoryId)
{
    ActorAccessoryJointTable *accessories;
    ActorAccessoryJointTable *emptyTable;
    int result = -1;

    accessories = actor->accessoryJoints;
    if (accessories == 0) {
        actor->accessoryJoints = JNT_getAccessories(actor->move);
        accessories = actor->accessoryJoints;
    }
    emptyTable = 0;
    if (accessories != emptyTable && accessoryId < 0x29) {
        result = accessories->jointId[accessoryId];
    }
    return result;
}

void ACT_resetArms(Actor *actor, Actor *other, int acc_id)
{
    if (acc_id == 0x108) {
        ACT_resetParent(actor, other);
        ACT_setHumanHand(other, 0);
    }
}

Actor *ACT_setArms(Actor *actor, Actor *parent, unsigned int accessoryId, int flags)
{
    int joint;

    if ((accessoryId & 0xff00) == 0) {
        joint = ACT_jointGetAccessories(parent, accessoryId);
        if (joint > 0) {
            actor = ACT_setParent(actor, 0, parent, joint, flags);
        } else {
            actor = 0;
        }
    } else if (accessoryId == 0x108) {
        actor = ACT_setParent(actor, 2, parent, 0x42, flags | 2);
        ACT_setHumanHand(parent, 0x8007);
    }
    return actor;
}

Actor *ACT_setRelation(Actor *actor, Actor *parent, int relation, int flags)
{
    int joint;
    int type;
    void (*filter)(void *work, void *parameter, int flags);

    if ((relation & 0x8000) != 0) {
        switch (relation & 0x7f00) {
        case 0:
            joint = ACT_jointGetAccessories(parent, relation & 0x7fff);
            type = 0;
            filter = JC_setParent;
            break;
        case 0x100:
            filter = JC_copyChain;
            joint = 0x42;
            type = 2;
            ACT_setHumanHand(parent, 0x8007);
            break;
        case 0x200:
            filter = JC_face;
            joint = 0x30;
            type = 2;
            ACT_setVisible(parent, 0x46414345, 0);
            break;
        default:
            joint = 0;
            type = 0;
            filter = JC_setParent;
            break;
        }
    } else {
        joint = relation & 0x7fff;
        type = 0;
        filter = JC_setParent;
    }
    return ACT_setParent2(actor, type, parent, joint, flags, filter);
}

Actor *ACT_setFace(Actor *actor, Actor *parent, int faceId)
{
    Actor *result;

    result = ACT_setParent(actor, 2, parent, 0x30, faceId);
    if (result != 0) {
        ACT_setVisible(parent, 0x46414345, 0);
    }
    return result;
}

void *ACT_animGetUserData(Actor *actor)
{
    return actor->animation.animUserData;
}

int ACT_animCheckData(Actor *actor)
{
    return FCV2_checkData(actor->animation.animData);
}

static void ACT_updateMotionCore(Actor *actor, int pause)
{
    int childIndex;
    Actor **children;

    if (actor->parent != 0) {
        return;
    }
    ACT_updateMotionSub(actor, pause);
    if (actor->childCount > 0) {
        if ((actor->flags & 0x1000) == 0) {
            children = actor->children;
            for (childIndex = 0; childIndex < actor->childCount; childIndex++) {
                ACT_updateMotionSub(children[childIndex], pause);
            }
        } else {
            children = actor->children;
            for (childIndex = 0; childIndex < actor->childCount; childIndex++) {
                children[childIndex]->flags |= 0x1000;
            }
        }
    }
}

void ACT_updateMotion(Actor *actor)
{
    ACT_updateMotionCore(actor, 0);
}

void ACT_updateMotionPause(Actor *actor)
{
    ACT_updateMotionCore(actor, 1);
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct ActorJointProducer {
    unsigned char unmodeled_00[0x70];
    int state;
} ActorJointProducer;

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct ActorJointWork {
    struct JntProducer *producer;
    unsigned char unmodeled_04[0x2c - 0x04];
    int current_index;
    void *channels;
    unsigned char unmodeled_34[0x7fc - 0x34];
    float current_frame;
    unsigned char unmodeled_800[0x81c - 0x800];
    int channel_start_index;
} ActorJointWork;

typedef struct ActorResource {
    void *model;
    void *matrices;
    void *move;
} ActorResource;

extern void JNT_animSetFlags(unsigned int flags);
void ACT_setMotion2(Actor *actor, int motionId, unsigned int flags);
extern char *strstr(const char *haystack, const char *needle);
extern void JNT_startProduction(void *producer, Actor *a, float frame);
extern void JNT_getRootTrans(void *v);
extern void JNT_setFCurve(void *anim, int x);
extern void JNT_setClipR(float clip_radius);
extern void JNT_setInterpMatrix(void *m, float interpolation);
extern void JNT_setCurve(void *static_value_records, void *value_records);
extern void ANM_update(void *anim);
extern void FCV2_setStep(float frame_step);
extern unsigned char enepc[];
extern const char D_004D1800[];
extern const char D_004DC230[];
extern const char D_004D1810[];

static void ACT_updateMotionSub(Actor *actor, int pause)
{
    unsigned int temp_22 = 0;
    unsigned int var_2;
    unsigned int var_6;
    int temp_4;
    int temp_4_2;
    int temp_4_3;
    unsigned int temp_3;
    int var_5;
    int var_18;
    int temp_18;
    void *temp_17;
    void *temp_20;
    void *temp_21;
    void *temp_4_4;
    void *temp_5;
    float var_f0;
    float var_f12;
    int temp_4_5;

    ActorAnimationState *anim = &actor->animation;
    ActorAnimationState *a3 = anim;
    temp_22 = actor->animation.slot.flags;
    if (pause == 2) {
        a3 = (ActorAnimationState *)((char *)actor + 0x6F0);
    }
    var_6 = temp_22;
    if (pause == 0) {
        int cnt = *(unsigned short *)((char *)actor + 0x696);
        if (*(short *)((char *)actor + 0x696) > 0) {
            *(short *)((char *)actor + 0x696) = cnt - 1;
        } else {
            *(short *)((char *)actor + 0x696) = 0;
        }
    }
    if ((*(unsigned long long *)actor & 0x408) != 0x408) {
        temp_4 = *(int *)((char *)actor + 0x754);
        if (temp_4 != 0) {
            var_2 = actor->animation.slot.flags & 0xBFFFFFFF;
            *(int *)((char *)actor + 0x754) = temp_4 - 1;
            goto block_12;
        }
        temp_4_2 = *(int *)((char *)actor + 0x758);
        if (temp_4_2 >= 0) {
            temp_4_3 = temp_4_2 - 1;
            temp_3 = actor->animation.slot.flags | 0x40000000;
            *(int *)((char *)actor + 0x758) = temp_4_3;
            actor->animation.slot.flags = temp_3;
            if (temp_4_3 < 0) {
                var_2 = temp_3 & 0xBFFFFFFF;
block_12:
                actor->animation.slot.flags = var_2;
            }
            var_6 = a3->slot.flags;
        }
        if ((var_6 & 0x1000) && (a3->slot.nextDataId != 0xFFFF) && ((actor != (Actor *)GameLoopState[1]) || (*(short *)(enepc + 0x3830) == 0))) {
            ACT_setMotion2(actor, a3->slot.nextDataId, a3->slot.animQueuedFlags);
        }
        temp_17 = actor->jointMatrices;
        temp_20 = actor->modelResource;
        temp_21 = actor->move;
        if ((temp_17 != 0) && ((temp_20 == 0) || ((*(unsigned long long *)temp_20 & 0xFFFFFF) == 0x78656C)) && ((temp_21 == 0) || ((((unsigned short *)temp_21)[0] == 0x4E4A) && (((signed char *)temp_21)[2] == 0x54)))) {
            CUR_MATRIX_Unit4s();
            CUR_MATRIX_SetT(&actor->position);
            CUR_MATRIX_Rx4s(&actor->rotation);
            CUR_MATRIX_Ry4s(&actor->rotation.y);
            CUR_MATRIX_Rz4s(&actor->rotation.z);
            CUR_MATRIX_Sxyz4s(&actor->scale);
            CUR_MATRIX_Get(temp_17);
            actor->flags &= ~0x1000;
            if (temp_22 & 0x10000) {
                unsigned int sw = (unsigned int)actor->jointMatrices;
                temp_5 = actor->valueRecords;
                actor->jointMatrices = actor->previousJointMatrices;
                actor->previousJointMatrices = (void *)sw;
                actor->animation.slot.bufferIndex = (actor->animation.slot.bufferIndex + 1) & 1;
                sw = (unsigned int)actor->staticValueRecords;
                actor->staticValueRecords = temp_5;
                actor->valueRecords = (void *)sw;
                __asm__ __volatile__("lq $2, 0(%1)\n\tsq $2, 0(%0)\n\tlq $2, 16(%1)\n\tsq $2, 16(%0)\n\tlq $2, 32(%1)\n\tsq $2, 32(%0)\n\tlq $2, 48(%1)\n\tsq $2, 48(%0)\n" : : "r"(actor->jointMatrices), "r"(actor->previousJointMatrices) : "$2", "memory");

            }
            FCV2_setStep(anim->slot.speed);
            JNT_animSetFlags(anim->slot.flags);
            if ((temp_22 & 2) && *(short *)((char *)actor + 0x696) == 0) {
                var_18 = 0x8002;
                if ((((unsigned char *)actor)[0x70 + 0x12] ^ 1) == 0) {
                    var_18 = 0x8082;
                }
                var_18 = (actor->flags & 0x200) ? (var_18 | 0x100) : var_18;
                if (actor->flags & 8) {
                    var_18 = !(actor->drawEnv.flags & 0x100) ? (var_18 | 0x200) : var_18;
                }
                if (temp_20 != 0) {
                    char *name = (char *)temp_20 + 0x10;
                    if (strstr(name, D_004DC230) != 0) {
                        var_18 |= 0x40000;
                    }
                    if ((strstr(name, D_004D1800) != 0) || (strstr(name, D_004D1810) != 0)) {
                        var_18 |= 0x20000;
                    }
                }
                JNT_setFlags(var_18);
                JNT_setInterpMatrix(actor->previousJointMatrices, 1.0f - ((anim->blendEndFrame - anim->slot.frame) * (1.0f / *(float *)((char *)anim + 0x24))));
                JNT_setCurve(actor->staticValueRecords, actor->valueRecords);
            } else {
                var_5 = 0x8000;
                if (actor->flags & 0x200) {
                    var_5 = 0x8100;
                }
                if (actor->flags & 8) {
                    var_5 = !(actor->drawEnv.flags & 0x100) ? (var_5 | 0x200) : var_5;
                }
                JNT_setFlags(var_5);
            }
            JNT_setClipR(anim->blendWork[3]);
            (void)0;
            if (FCV2_checkData(anim->animData) != 0) {
                if (anim->animData != 0) {
                    anim->animData = 0;
                }
            }
            JNT_setModel(temp_20, temp_21);
            JNT_setFCurve(anim->animData, *(unsigned short *)((char *)anim + 0x18));
            JNT_setMatrix(actor->jointMatrices);
            JNT_setCurve(actor->staticValueRecords, actor->valueRecords);
            var_f12 = anim->slot.frame;
            if (anim->slot.speed >= 0.0f) {
                var_f0 = anim->slot.forwardLimit;
                if (var_f0 < var_f12) {
                    var_f12 = var_f0;
                }
            } else {
                var_f0 = anim->slot.reverseLimit;
                if (var_f12 < var_f0) {
                    var_f12 = var_f0;
                }
            }
            JNT_startProduction(actor->jointProducer, actor, var_f12);
            JNT_getRootTrans((char *)actor + 0x760);
            temp_4_5 = *(int *)((char *)actor + 0x7F4);
            if (temp_4_5 & 0x10) {
                actor->flags |= 0x2000;
            }
            if ((anim->slot.flags & 0x10000000) && (temp_4_5 & 0x40)) {
                actor->flags = (actor->flags | 0x1000) & ~0x2000;
            }
            if (temp_22 & 0x10000) {
                anim->slot.flags &= 0xFFFEFFFF;
            }
            ANM_update(anim);
        }
    }
}

void ACT_setDrawEnv(Actor *actor)
{
    ActorDrawEnv *environment = &actor->drawEnv;
    unsigned int flags = environment->flags;
    int lightIndex;

    nmlModelSetZwrite(flags & 2);
    nmlModelSetStencil(flags & 4);
    if ((flags & 1) != 0) {
        nmlModelSetToumei(flags & 1);
        nmlModelSetAlpha(environment->alpha);
        nmlModelSetTransparency(environment->transparency);
    }
    if ((flags & 8) != 0) {
        nmlModelSetFogCol(environment->fogColor);
        nmlModelSetFogDist(environment->fogDistance[0], environment->fogDistance[1],
                           environment->fogDistance[2], environment->fogDistance[3]);
    }
    if ((flags & 0x20) != 0) {
        for (lightIndex = 0; lightIndex < 3; lightIndex++) {
            nmlModelSetPointLight(lightIndex, environment->lightPosition[lightIndex],
                                 environment->lightColor[lightIndex]);
        }
    }
}

void ACT_modelDraw(Actor *actor)
{
    Actor **children;
    int childIndex;

    if (actor->parent == 0) {
        ACT_modelDrawSub(actor);
        if ((actor->flags & 0x1008) == 0) {
            if (actor->childCount > 0) {
                children = actor->children;
                for (childIndex = 0; childIndex < actor->childCount; childIndex++) {
                    ACT_modelDrawSub(children[childIndex]);
                }
            }
        }
    }
}

typedef struct ActorShapeBinding {
    unsigned char unmodeled_00[2];
    unsigned short jointIndex;
    unsigned short shapeId;
} ActorShapeBinding;
typedef ActorShapeBinding *ActorShapeTable[];
typedef unsigned int Quadword __attribute__((mode(TI)));
extern void JNT_animSetFlags(unsigned int flags);
extern void MATRIX_convert4(void *out, unsigned int matrices);
extern void MDL_draw(void *model, void *matrix);
extern void MDL_partsSetVisible(void *model);
extern int nmlModelAskNonLinearCamera(void);
extern void nmlModelClear(void);
extern void nmlModelEntry(unsigned int entry);
extern void nmlModelGetGblPosition(void *position);
extern void nmlModelSetAxisSymmetry(float symmetry);
extern void nmlModelSetClip(int clip);
extern void nmlModelSetFaceModel(int enabled);
extern void nmlModelSetHumanModel(unsigned char human);
extern void nmlModelSetLight(void *light, void *colors);
extern void nmlModelSetMapShadowParts(int partId);
extern void nmlModelSetParent(unsigned int parent);
extern void nmlModelSetPartsPixelAlpha(short part, short alpha);
extern void nmlModelSetPixelAlpha(unsigned int alpha);
extern void nmlModelSetPlace(void *matrix);
extern void nmlModelSetRenderLevel(unsigned int level);
extern void nmlModelSetScale(void *scale);
extern void nmlModelSetShadowHeight(float height);
extern void nmlModelSetShapeId(int shapeId, float weight);
extern void nmlModelSetSortOffset(float offset);
extern void nmlModelSetTexMap(int mode, int size, float weight);
extern void nmlModelSetTexture(unsigned int texture);
extern void nmlModelSetWindow(unsigned short window);
extern void xglLightCalcMatrix(void *light);
extern float xglPointLengthXZ(const Vector4 *point1, const Vector4 *point2);
extern void *xglStudioGetLight2(void);
extern unsigned int xglStudioSelectGetActiveCamera(int studio_index);

static void ACT_modelDrawSub(Actor *actor)
{
    unsigned int model;
    unsigned int matrixResource;
    unsigned int move;
    unsigned int matrices;
    Actor *parent;
    unsigned int flagsNow;
    Quadword matrixBuffer[4];
    ActorScale scaleVector[1];
    void (*drawHook)(Actor *);
    int lightFlag;
    int i;
    ActorPartAlpha *partAlpha;
    unsigned int texMode;
    unsigned int parentFlags;

    if ((actor->flags & 0x1008) != 0) {
        return;
    }
    model = (unsigned int)actor->modelResource;
    matrixResource = (unsigned int)actor->matrixResource;
    move = (unsigned int)actor->move;
    if (model == 0) {
        return;
    }
    if (move != 0 && (actor->flags & 0x2000) != 0) {
            JNT_animSetFlags(actor->animation.slot.flags);
            JNT_setFlags(0x8000);
            JNT_setModel((void *)model, (void *)move);
            JNT_setMatrix((void *)(unsigned int)actor->jointMatrices);
            JNT_setModelMatrix();
            i = 0u;
            if (1 <= actor->shapeCount) {
                ActorShapeTable *shapeBindings =
                    (ActorShapeTable *)actor->shapeBindingStorage;
                while (actor->shapeCount > i) {
                    ActorShapeBinding *shapeRecord = (*shapeBindings)[i];
                    Matrix4 *jointRow = (Matrix4 *)actor->jointMatrices +
                        shapeRecord->jointIndex;

                    if ((actor->drawEnv.flags & 0x400) == 0) {
                        nmlModelSetShapeId(shapeRecord->shapeId,
                                           (*jointRow)[3][2]);
                    }
                    i = i + 1;
                }
            }
            actor->flags &= 0xffffdfffu;
            matrices = (unsigned int)actor->jointMatrices;
    } else {
        matrices = (unsigned int)actor->jointMatrices;
    }
    lightFlag = 1;
    MATRIX_convert4(matrixBuffer, matrices);
    drawHook = actor->draw;
    flagsNow = actor->flags;
    parent = actor->parent;
    if ((flagsNow & 0x800) == 0) {
        if (((unsigned int)drawHook - 0x1f0000 < 0x1e10000) && (((unsigned int)drawHook & 3) == 0)) {
            drawHook(actor);
            flagsNow = actor->flags;
        } else if (parent != 0) {
            parentFlags = parent->flags;
            if ((parentFlags & 0x8000) != 0) {
                nmlModelSetLight(&parent->light.direction, &parent->light.color);
                lightFlag = 0;
                parentFlags = parent->flags;
            }
            drawHook = parent->draw;
            if ((parentFlags & 0x800) != 0) {
                ACT_setDrawEnv(parent);
            } else {
                if (((unsigned int)drawHook - 0x1f0000 < 0x1e10000) && (((unsigned int)drawHook & 3) == 0)) {
                    drawHook(parent);
                }
            }
            flagsNow = actor->flags;
        }
    } else {
        ACT_setDrawEnv(actor);
        flagsNow = actor->flags;
    }
    if ((flagsNow & 0x8000) != 0) {
        xglLightCalcMatrix(&actor->light);
        nmlModelSetLight(&actor->light.direction, &actor->light.color);
    } else if (lightFlag) {
        ActorCalculatedLight *light = (ActorCalculatedLight *)xglStudioGetLight2();
        xglLightCalcMatrix(light);
        nmlModelSetLight(&light->direction, &light->color);
    }
    scaleVector[0].words[0] = actor->scale.words[0];
    scaleVector[0].words[1] = actor->scale.words[1];
    if (parent != 0) {
        __asm__ __volatile__(
            "lqc2 vf3, 0(%0)\n\t"
            "lqc2 vf2, 0(%1)\n\t"
            "vmul.xyz vf2, vf2, vf3\n\t"
            "sqc2 vf2, 0(%0)\n\t"
            "nop"
            :
            : "r"(scaleVector), "r"(&parent->scale)
            : "memory");
    }
    nmlModelSetScale(scaleVector);
    if (((actor->drawEnv.renderLevel & 0x80) != 0) && nmlModelAskNonLinearCamera() != 0) {
        unsigned int camera = xglStudioSelectGetActiveCamera(0);
        actor->drawEnv.renderLevel = actor->drawEnv.renderLevel & 0xfffffeff;
        if (actor->drawEnv.cameraCullDistance < xglPointLengthXZ(&actor->globalPosition, (void *)(camera + 0xd0))) {
            actor->drawEnv.renderLevel = actor->drawEnv.renderLevel | 0x100;
        }
    }
    if (parent != 0) {
        if (actor->faceKind == 1 && (parent->drawEnv.flags & 0x80) != 0) {
            nmlModelClear();
            return;
        }
        nmlModelSetParent((unsigned int)parent->modelResource);
        nmlModelSetWindow(parent->windowId);
        nmlModelSetRenderLevel(parent->drawEnv.renderLevel);
        if ((parent->drawEnv.flags & 0x10) != 0) {
            nmlModelSetAxisSymmetry(parent->position.y);
        }
    } else {
        for (i = 0; i < actor->mapShadowPartCount; i = i + 1) {
            nmlModelSetMapShadowParts(actor->mapShadowPartIds[i]);
        }
        texMode = 0;
        switch (actor->shadow_kind) {
        case 5:
            nmlModelSetTexMap(1, actor->shadowSize, actor->shadowClipScale);
            break;
        case 10:
            texMode = 0x40000;
            nmlModelSetTexMap(texMode | 1, actor->shadowSize, actor->shadowClipScale);
            break;
        case 6:
            nmlModelSetTexMap(3, actor->shadowSize, actor->shadowClipScale);
            break;
        case 7:
            texMode = 0x10000;
            nmlModelSetTexMap(texMode | 1, actor->shadowSize, actor->shadowClipScale);
            break;
        case 8:
            texMode = 0x20000;
            nmlModelSetTexMap(texMode | 1, actor->shadowSize, actor->shadowClipScale);
            break;
        case 9:
            nmlModelSetTexMap(0x30000, actor->shadowSize, actor->shadowClipScale);
            break;
        default:
            break;
        }
        nmlModelSetWindow(actor->windowId);
        nmlModelSetRenderLevel(actor->drawEnv.renderLevel);
        if ((actor->drawEnv.flags & 0x10) != 0) {
            nmlModelSetAxisSymmetry(actor->position.y);
        }
    }
    nmlModelSetFaceModel(actor->faceKind == 1);
    nmlModelSetHumanModel(actor->humanKind);
    nmlModelSetSortOffset(actor->sortOffset);
    nmlModelSetPixelAlpha(actor->drawEnv.pixelAlpha);
    i = 0;
    if (actor->drawEnv.partAlphaCount >= 1) {
        partAlpha = actor->drawEnv.partAlpha;
        do {
            nmlModelSetPartsPixelAlpha(partAlpha->partId, partAlpha->alpha);
            i = i + 1;
            partAlpha++;
        } while (i < actor->drawEnv.partAlphaCount);
    }
    if ((actor->drawEnv.flags & 0x200) != 0) {
        nmlModelSetClip(1);
    }
    if ((actor->drawEnv.flags & 0x40) != 0) {
        nmlModelSetShadowHeight(0.0f);
    }
    if ((actor->flags & 0x4000) != 0) {
        MDL_draw(&actor->model[0].model, matrixBuffer);
        nmlModelGetGblPosition(&actor->globalPosition);
    } else {
        nmlModelSetTexture(matrixResource);
        nmlModelSetPlace(matrixBuffer);
        MDL_partsSetVisible(&actor->model[0].model);
        nmlModelEntry(model);
        nmlModelGetGblPosition(&actor->globalPosition);
    }
    (void)move;
    (void)scaleVector;
}

void ACT_setModelWrapper(Actor *actor)
{
    ActorModelState *modelState = actor->model;
    void *resource = actor->modelResource;
    ActorMoveHeader *move = actor->move;
    void *matrixResource = actor->matrixResource;
    Matrix4 *rootMatrices;

    if (resource != 0) {
        if (move == 0) {
            MDL_create(&modelState->model, resource);
            return;
        }
        MDL_create(&modelState->model, resource);
        rootMatrices = JNT_getRootElement(move);
        rootMatrices = &rootMatrices[move->baseJointCount + move->extraJointCount];
        modelState->model.matrixResource = matrixResource;
        actor->flags |= 0x4000;
        modelState->jointMatrices = rootMatrices;
    }
}

extern Actor *ACT_create(int, int);
extern void ACT_dispose2(Actor *, int);
extern void ACT_initMotion(Actor *);
extern int ACT_allocMatrix(char *, int);

void ACT_loadResource(Actor *act, int id)
{
    int key;
    int sid;
    int *res;
    int model;
    int tex;
    int move;

    key = id & 0xFFFFFF;
    res = RES_loadFile(-1, 0, key, 0);
    if (res != 0) {
        goto found;
    }
    if (!(key & 0xFF0000)) {
        res = RES_loadFile(-1, 0, key & 0xFFFF00, 0);
    }
    if (res != 0) {
found:
        model = res[0];
        tex = res[1];
        move = res[2];
    } else {
        model = 0;
        tex = 0;
        move = 0;
    }
    *(int *)((char *)act + 0x8D0) = model;
    *(int *)((char *)act + 0x8D4) = tex;
    *(int *)((char *)act + 0x8D8) = move;
    ACT_allocMatrix((char *)act, -1);
    ((void (*)(Actor *, int))ACT_setModelWrapper)(act, 0);
    if (!(id & 0xFFF000)) {
        sid = *(short *)((char *)act + 0x86);
        if ((sid & 0xFFFF00) < 0x601) {
            Actor *sub;

            sub = ACT_create(-2, sid + 0x10000);
            ACT_loadResource(sub, sid + 0x10000);
            if (*(int *)((char *)sub + 0x8D0) != 0) {
                *(unsigned char *)((char *)sub + 0x90) = 0;
                *(int *)sub &= ~0x20;
                ACT_initMotion(sub);
                ACT_setRelation(sub, act, 0x8200, 1);
            } else {
                ACT_dispose2(sub, 1);
            }
        }
        ACT_setHumanHand(act, 0);
        if (move == 0) {
            *(unsigned char *)((char *)act + 0x90) = 0;
            *(int *)act &= ~0x20;
        }
    }
}

void ACT_initMTNResource(void)
{
}

void ACT_resourceInit(void)
{
}

INCLUDE_ASM("asm/main/nonmatchings/act_2", ACT_initMotion);

void ACT_setMotion(Actor *actor, int motionId)
{
    ActorAnimationState *animation = &actor->animation;
    ActorAnimSlot *slot = &animation->slot;

    if (motionId == 0x80000000U) {
        void *move;
        void *model;

        ACT_resetMatrix(actor);
        animation->animPack = 0;
        animation->animData = 0;
        animation->animUserData = 0;
        animation->slot.flags &= 0xdfffeff6;
        animation->slot.speed = 0.033333335f;
        animation->slot.currentDataId = 0;
        animation->slot.frame = 0;
        animation->slot.reverseLimit = 0;
        animation->slot.forwardLimit = 0;
        move = actor->move;
        model = actor->modelResource;
        JNT_setFlags(0x8000);
        JNT_setModel(model, move);
        JNT_setMatrix(actor->jointMatrices);
        JNT_setModelMatrix();
        return;
    }
    if (slot->currentDataId == motionId && (slot->flags & 0x20000000) == 0) {
        return;
    }
    slot->currentDataId = motionId & 0x7ff;
    slot->nextDataId = 0xffff;
    slot->animQueuedFlags = 0;
    ANM_reset(slot, actor->animPackTables[(motionId >> 8) & 7]);
    slot->flags = (slot->flags & 0xdffffff6) | 0x10000;
}

void ACT_setMotion2(Actor *actor, int motionId, unsigned int flags)
{
    ActorAnimationState *animation = &actor->animation;
    ActorAnimSlot *slot = &animation->slot;
    unsigned int slotFlags;
    unsigned int bank;

    if (motionId == 0x80000000U) {
        void *move;
        void *model;

        ACT_resetMatrix((Actor *)actor);
        slot->currentDataId = 0u;
        animation->animPack = (void *)0;
        animation->animData = 0u;
        animation->animUserData = 0u;
        slot->flags &= 0xdfffeff6u;
        slot->frame = 0.0f;
        slot->speed = 0.033333335f;
        slot->reverseLimit = 0.0f;
        slot->forwardLimit = 0.0f;
        move = (void *)actor->move;
        model = (void *)actor->modelResource;
        JNT_setFlags(0x8000u);
        JNT_setModel((void *)model, (void *)move);
        JNT_setMatrix((void *)actor->jointMatrices);
        JNT_setModelMatrix();
    } else {
        if (slot->currentDataId == motionId) {
            if ((flags & 0x20000000) == 0) {
                goto early;
            }
            slotFlags = slot->flags;
            goto common;
        }
            slotFlags = slot->flags;
            if (0 == (slotFlags & 0x2)) {
                flags |= 0x10000;
            } else if (slot->nextDataId == 0xffff) {
                void *valueTmp;

                if (0 == (slotFlags & 0x20u)) {
                    slot->nextDataId = motionId;
                    slot->animQueuedFlags = flags;
                    slot->forwardLimit = animation->blendEndFrame;
                    return;
                }
                slotFlags = (unsigned int)actor->jointMatrices;
                valueTmp = actor->valueRecords;
                actor->jointMatrices = actor->previousJointMatrices;
                actor->previousJointMatrices = (void *)slotFlags;
                actor->animation.slot.bufferIndex = (actor->animation.slot.bufferIndex + 1) & 1;
                slotFlags = (unsigned int)actor->staticValueRecords;
                actor->staticValueRecords = valueTmp;
                actor->valueRecords = (void *)slotFlags;
                __asm__ __volatile__("lq $2, 0(%1)\n"
                                     "sq $2, 0(%0)\n"
                                     "lq $2, 16(%1)\n"
                                     "sq $2, 16(%0)\n"
                                     "lq $2, 32(%1)\n"
                                     "sq $2, 32(%0)\n"
                                     "lq $2, 48(%1)\n"
                                     "sq $2, 48(%0)\n"
                                     :
                                     : "r"(actor->jointMatrices), "r"(actor->previousJointMatrices)
                                     : "$2", "memory");
                animation->previousDataId = slot->currentDataId;
                slot->nextDataId = 0xffff;
                slot->currentDataId = motionId & 0x7ff;
                flags &= 0xdfffffff;
                slot->flags = (slot->flags & 0xdffffff7) | flags;
                slot->animQueuedFlags = 0;
                bank = (motionId >> 8) & 7;
                ANM_reset(slot, actor->animPackTables[bank]);
                return;
            }
        
common:
        animation->previousDataId = slot->currentDataId;
        slot->nextDataId = 0xffff;
        slot->currentDataId = motionId & 0x7ff;
        flags &= 0xdfffffff;
        slot->flags = (slotFlags & 0xdffffff6) | flags;
        slot->animQueuedFlags = 0;
        bank = (motionId >> 8) & 7;
        ANM_reset(slot, actor->animPackTables[bank]);
        goto done;
early:
        flags &= 0xdfffffff;
        slot->flags |= flags;
done:
        ;

    }
}

void *ACT_animGetData(Actor *actor, unsigned int dataId)
{
    dataId &= 0x7ff;
    return PACK_getEntry(actor->animPackTables[dataId >> 8], dataId & 0xff);
}

void ACT_animGetCurrent(Actor *actor)
{
    ActorAnimSlot *slot = &actor->animation.slot;

    ANM_getEntry(slot, actor->animPackTables[(slot->currentDataId >> 8) & 7]);
}

void ACT_loadMotion(Actor *actor, int id, int bank)
{
    unsigned short kind;
    int kindFlags;
    void *pack;

    if ((id & 0x00ff0000) != 0) {
        return;
    }
    id &= 0x00ffffff;
    kindFlags = actor->resourceKind & 0xf000;
    kind = actor->resourceKind;
    if (kindFlags == 0x5000 || kindFlags == 0x6000) {
        actor->animPackTables[0] = 0;
        ACT_setMotion(actor, 0x8000);
        return;
    }
    bank = bank == 2 ? 1 : bank;
    pack = 0;
    if ((int)GameLoopState[6] <= 0x0fffffff) {
        pack = (void *)RES_loadFile(-1, 2, ((unsigned int)bank << 24) + id, 0);
        if (pack == 0) {
            pack = (void *)RES_loadFile(-1, 2, 0x01000001, 0);
            kind = actor->resourceKind;
        } else {
            kind = actor->resourceKind;
        }
    }
    actor->animPackTables[0] = pack;
    if ((kind & 0xf000) != 0x5000 && (kind & 0xf000) != 0x6000) {
        ACT_setMotion(actor, 0x8000);
    }
}

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

void ACT_setVisible(Actor *actor, int part, int visible)
{
    if ((unsigned int)part >= 128) {
        MDL_setGroupVisible(actor->model, part, visible);
    } else {
        MDL_setVisible(actor->model, part, visible);
    }
}

void ACT_setHumanHand(Actor *actor, int hand)
{
    int left;
    int right;
    int handNameIndex;
    unsigned int pattern[4];

    if ((actor->flags & 0x4000) != 0) {
        right = hand & 0x87;
        left = (hand >> 8) & 0x87;
        if (left != right) {
            if (left != 0x80) {
                for (handNameIndex = 0; tblHand[handNameIndex] != 0; handNameIndex++) {
                    pattern[HAND_SUFFIX] = 0x5f4c0000;
                    pattern[HAND_NAME_MASK] = 0xffffffff;
                    pattern[HAND_SUFFIX_MASK] = 0xffff0000;
                    pattern[HAND_NAME] = tblHand[handNameIndex];
                    MDL_setNameVisible(actor->model, &pattern, 0);
                }
                pattern[HAND_NAME] = tblHand[left];
                pattern[HAND_SUFFIX] = 0x5f4c0000;
                pattern[HAND_NAME_MASK] = 0xffffffff;
                pattern[HAND_SUFFIX_MASK] = 0xffff0000;
                MDL_setNameVisible(actor->model, &pattern, 1);
            }
            if (right != 0x80) {
                for (handNameIndex = 0; tblHand[handNameIndex] != 0; handNameIndex++) {
                    pattern[HAND_SUFFIX] = 0x5f520000;
                    pattern[HAND_NAME_MASK] = 0xffffffff;
                    pattern[HAND_SUFFIX_MASK] = 0xffff0000;
                    pattern[HAND_NAME] = tblHand[handNameIndex];
                    MDL_setNameVisible(actor->model, &pattern, 0);
                }
                pattern[HAND_NAME] = tblHand[right];
                pattern[HAND_SUFFIX] = 0x5f520000;
                pattern[HAND_NAME_MASK] = 0xffffffff;
                pattern[HAND_SUFFIX_MASK] = 0xffff0000;
                MDL_setNameVisible(actor->model, &pattern, 1);
            }
        } else if (left != 0x80) {
            for (handNameIndex = 0; tblHand[handNameIndex] != 0; handNameIndex++) {
                pattern[HAND_SUFFIX] = 0;
                pattern[HAND_NAME_MASK] = 0xffffffff;
                pattern[HAND_SUFFIX_MASK] = 0;
                pattern[HAND_NAME] = tblHand[handNameIndex];
                MDL_setNameVisible(actor->model, &pattern, 0);
            }
            pattern[HAND_NAME] = tblHand[left];
            pattern[HAND_SUFFIX] = 0;
            pattern[HAND_NAME_MASK] = 0xffffffff;
            pattern[HAND_SUFFIX_MASK] = 0;
            MDL_setNameVisible(actor->model, &pattern, 1);
        }
    }
}

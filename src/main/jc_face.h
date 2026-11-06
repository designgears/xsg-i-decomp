#ifndef JC_FACE_H
#define JC_FACE_H

#include "common.h"
#include "shared.h"

/* 64-byte four-row matrix copy through the R5900 128-bit GPRs
 * (sceVu0CopyMatrix idiom). */
#define QuadwordCopyMatrix(destination, source) \
    __asm__ __volatile__( \
        "lq $2, 0(%1)\n" \
        "sq $2, 0(%0)\n" \
        "lq $2, 16(%1)\n" \
        "sq $2, 16(%0)\n" \
        "lq $2, 32(%1)\n" \
        "sq $2, 32(%0)\n" \
        "lq $2, 48(%1)\n" \
        "sq $2, 48(%0)\n" \
        : \
        : "r" (destination), "r" (source) \
        : "$2", "memory")

/* Joint-animation work types (jnt.c), as JC_face and the chain filters use them. */
typedef struct JntMatrix JntMatrix;
typedef struct JntWork JntWork;
struct JntMatrix {
    union {
        Matrix4 elements;
        float flat[16];
    };
};
typedef JntMatrix *JntMatrixBuffer;
typedef unsigned int JntMatrixSelect;
typedef struct JntHairIdSource {
    unsigned char unmodeled_00[4];
    unsigned short version;
    unsigned short channel_limit;
    unsigned short primary_id;
    unsigned short secondary_id;
    unsigned short additional_primary_count;
    unsigned short additional_secondary_count;
    int channel_offset;
} JntHairIdSource;
typedef struct JntChainSlot {
    struct FcvPackAttribute *attribute;
    unsigned char unmodeled_04[0x0c];
    Vector4 translation;
    Vector4 scale;
    Vector4 rotation;
} JntChainSlot;
typedef struct JntProductionOwner JntProductionOwner;
typedef struct JntTransformRecord {
    Vector4 translation;
    Vector4 rotation;
} JntTransformRecord;
typedef struct JntProducer JntProducer;
typedef struct JntInterruptElement JntInterruptElement;
struct JntInterruptElement {
    Vector4 scale;
    float rotation[3];
    unsigned char unmodeled_1c[4];
    Vector4 translation;
    unsigned short key;
    unsigned short interrupt_channel;
    void *attribute_data;
    unsigned char unmodeled_38[8];
};
struct JntWork {
    JntProducer *producer;
    unsigned short *cursor;
    unsigned char unmodeled_008[0x10 - 0x08];
    unsigned int requested_attribute_key;
    unsigned char unmodeled_014[0x28 - 0x14];
    struct JntHairIdSource *hair_id_source;
    int current_index;
    struct JntStaticChannel *channels;
    void *model;
    unsigned int flags;
    unsigned short channel_index;
    unsigned char unmodeled_03e[2];
    Vector4 root_translation;
    Vector4 root_rotation;
    Vector4 root_scale;
    Vector4 channel_translation;
    Vector4 channel_rotation;
    Vector4 channel_scale;
    JntChainSlot chain_slots[16];
    unsigned char unmodeled_4a0[0x4bc - 0x4a0];
    JntMatrixSelect matrix_select;
    unsigned short matrix_index;
    short chain_base_index;
    unsigned char unmodeled_4c4[0x4d0 - 0x4c4];
    Matrix4 transform_basis;
    unsigned char unmodeled_510[0x720 - 0x510];
    JntMatrix root_matrix;
    JntMatrix interpolation_root;
    JntMatrix *matrix_buffer;
    JntMatrixBuffer interp_matrix;
    JntTransformRecord (*static_value_records)[];
    void *value_records;
    Matrix4 root_correction;
    unsigned int attribute_key;
    unsigned int attribute_difference;
    unsigned char unmodeled_7f8[4];
    float current_frame;
    float interpolation;
    float chain_length;
    struct JntChannelTag *current_value;
    char *attribute_name;
    unsigned int animation_flags;
    float clip_radius;
    JntInterruptElement *elements;
    int channel_start_index;
    JntProductionOwner *owner;
    unsigned char unmodeled_824[0x0c];
    JntInterruptElement default_element;
};
typedef struct JntStaticChannel JntStaticChannel;
typedef void *(*JntProductionConsumer)(JntWork *work,
                                      JntStaticChannel *channels,
                                      int argument);
struct JntProducer {
    int count;
    int consumer_ids[8];
    JntProductionConsumer consumers[8];
    int consumer_arguments[8];
    int pending_output;
    struct ActorAccessoryJointTable *accessory_joints;
    int move_element_id;
    int deferred_count;
    struct ActorShapeBinding *deferred_limits[8];
};
typedef union JntRotationPayload {
    float values[3];
    int bits[3];
} JntRotationPayload;
struct JntStaticChannel {
    unsigned short type;
    unsigned short interpolation_flags;
    int rotation_limits_offset;
    unsigned char unmodeled_08[6];
    unsigned short matrix_index;
    Vector4 translation;
    JntRotationPayload rotate;
    unsigned char unmodeled_2C[0x0C];
    float translation_direction;
    unsigned char unmodeled_3c[4];
};
void *JNT_getElement(void *elements, int index);
void *JNT_nextElement(void *element);
typedef struct JcChain {
    unsigned short index;
    unsigned short static_index;
    void **source;
} JcChain;
void *JC_copyChain(JntWork *work, void *element, JcChain *chain);
extern JntProductionConsumer JNT_getFilter(void *work, JntProducer *producer);
extern int JNT_setInterrupt(JntWork *work, JntProducer *producer);
extern void xglMatrixUnit4s(Matrix4 destination);
extern JntStaticChannel *JNT_getStaticVal(JntWork *work,
                                          JntStaticChannel *channel);
extern JntStaticChannel *JNT_getVal2(JntWork *work, JntStaticChannel *channel,
                                     float frame);
extern JntStaticChannel *JNT_getVal_HOGE2(JntWork *work,
                                          JntStaticChannel *channel,
                                          float frame);
extern JntStaticChannel *JNT_getVal(JntWork *work, JntStaticChannel *channel,
                                    float frame);
extern JntStaticChannel *JNT_getVal_staticChain(JntWork *work,
                                                JntStaticChannel *channel,
                                                float frame);
extern JntStaticChannel *JNT_constructMatrix(JntWork *work,
                                             JntStaticChannel *channel);
extern JntStaticChannel *JNT_interpolate(JntWork *work,
                                         JntStaticChannel *channel);
extern void *JNT_getStaticVal2(JntWork *work, JntStaticChannel *channel);

#endif

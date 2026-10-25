#include "common.h"
#include "jnt.h"

typedef struct JntHairIdSource {
    unsigned char unmodeled_00[4];
    unsigned short version;
    unsigned short channel_limit;
    unsigned short primary_id;   /* 0x08 */
    unsigned short secondary_id; /* 0x0A */
    unsigned char unmodeled_0c[4];
    int channel_offset;
} JntHairIdSource;
typedef struct JntChannelTag {
    int flag; /* 0x00 */
    int tag;  /* 0x04 */
} JntChannelTag;
typedef struct JntChannelWork {
    unsigned char unmodeled_000[0x28];
    JntHairIdSource *hair_id_source; /* 0x28 */
    unsigned char unmodeled_02c[0x7dc];
    JntChannelTag *tag;              /* 0x808 */
} JntChannelWork;
typedef struct JntInterruptElement JntInterruptElement;
struct JntInterruptElement {
    unsigned char unmodeled_00[0]; /* +0x0: zero-length alias of the former padding member, kept for saved bodies */
    float scale[3];
    unsigned char unmodeled_0c[4];
    float rotation[3];
    unsigned char unmodeled_1c[4];
    float translation[3];
    unsigned char unmodeled_2c[4];
    unsigned short key;
    unsigned short interrupt_channel; /* 0x32, the channel that interrupts this element */
    unsigned char unmodeled_34[0]; /* +0x34: zero-length alias of the former padding member, kept for saved bodies */
    void *attribute_data;
    unsigned char unmodeled_38[8];
};
typedef struct JntInterruptWork JntInterruptWork;
struct JntInterruptWork {
    unsigned char unmodeled_000[0x818];
    JntInterruptElement *elements; /* 0x818 */
};
typedef struct JntChannelList JntChannelList;
struct JntChannelList {
    int count;                 /* 0x00 */
    int channel_elements[8];   /* 0x04, indexed by channel */
};
typedef struct JntAccessoryRecord {
    unsigned short size;
    unsigned short type;
} JntAccessoryRecord;

typedef struct JntMoveResource {
    unsigned char unmodeled_00[0x10];
    int accessory_size;
} JntMoveResource;

typedef void (*JntFilterFn)(void *work, void *param, int flag);
typedef struct JntFilterList {
    int count;
    int flags[8];
    JntFilterFn filters[8];
} JntFilterList;

/*
 * The JNT producer's work area is in the EE scratchpad (SPR, 0x70000000).
 * Two of its slots are named here, because this file's two recovered
 * functions are their writers; the area itself is not recovered, so the
 * slots are named relative to a base the caller materializes rather than
 * modeled as members (see the block comment on JNT_setMatrix).
 *
 * The base has to stay a value the function holds: folding it into each
 * store's address instead (`*(T *)0x700007a0`) makes 2.96 emit the two
 * absolute stores as separate li/sw pairs, four instructions in place of the
 * original's single shared `lui $2,0x7000` and two displaced stores.
 */
#define JNT_SCRATCH_BASE ((void *)0x70000000)
#define JNT_MATRIX_BUFFER_OFFSET 0x7A0
#define JNT_MATRIX_SELECT_OFFSET 0x4BC
#define JNT_MATRIX_BUFFER(scratch) (*(JntMatrixBuffer *)((scratch) + JNT_MATRIX_BUFFER_OFFSET))
#define JNT_MATRIX_SELECT(scratch) (*(JntMatrixSelect *)((scratch) + JNT_MATRIX_SELECT_OFFSET))

/*
 * Three more scratchpad slots share a fixed layout with the pair above, so
 * they are named with a partial struct instead of one offset macro per
 * field: JNT_setInterpMatrix stores its matrix-buffer argument at 0x7A4 and
 * its float argument at 0x800 (interp_matrix / interpolation below).
 *
 * JNT_setCurve stores its two pointer arguments at 0x7A8/0x7AC. Neither is
 * an end pointer: this same TU's JNT_getStaticVal (VA 0x0030EDC8) and
 * JNT_getVal (VA 0x0030F038) read them back as two independent per-element
 * 32-byte record tables, each gated on both slots being non-null and then
 * indexed by element_index*32 (the element's own 0x2C field):
 *   - JNT_getStaticVal: `sll $3,$11,5 / addu $3,$3,$5` with
 *     $5 = lw 0x7A8($10) -- 0x7A8 is the table it reads, static_value_records.
 *   - JNT_getVal: `sll $2,$6,5 / addu $3,$2,$3` with
 *     $3 = lw 0x7AC($17) -- 0x7AC is the table it reads, value_records.
 */
typedef struct JntWork {
    unsigned char unmodeled_000[0]; /* +0x0: zero-length alias of the former padding member, kept for saved bodies */
    JntProducer *producer;         /* +0x000 */
    unsigned short *cursor;        /* +0x004: FCV2 packed-curve state */
    unsigned char unmodeled_008[0x28 - 0x08];
    JntHairIdSource *hair_id_source; /* +0x028 */
    /*
     * JNT_getStaticVal2/JNT_getVal2 read this as the default matrix slot for
     * their CUR_MATRIX_Get call and unconditionally increment it once per
     * call (`lw/sw ...,44($.)`).
     */
    int current_index; /* 0x2C */
    unsigned char unmodeled_030[0]; /* +0x30: zero-length alias of the former padding member, kept for saved bodies */
    void *channels;                /* +0x030 */
    void *model;                   /* +0x034 */
    unsigned int flags;            /* +0x038 */
    unsigned short channel_index;  /* +0x03c */
    unsigned char unmodeled_03e[2];
    float root_translation[4];     /* +0x040, Vector4 */
    float root_rotation[4];        /* +0x050, Vector4 */
    float root_scale[4];           /* +0x060, Vector4 */
    unsigned char unmodeled_070[0x4bc - 0x70];
    JntMatrixSelect matrix_select; /* +0x4bc */
    unsigned short matrix_index;   /* +0x4c0 */
    unsigned char unmodeled_4c2[0x7a0 - 0x4c2];
    /*
     * JNT_getStaticVal2/JNT_getVal2 read this as the matrix table their
     * CUR_MATRIX_Set/Get calls index by joint slot (`lw ...,1952($.)`);
     * JNT_setMatrix/JNT_setMatrix2 write it through JNT_MATRIX_BUFFER_OFFSET
     * above (same 0x7A0 slot, kept as a raw offset there for the codegen
     * reason given on that macro).
     */
    JntMatrix *matrix_buffer; /* 0x7A0 */
    JntMatrixBuffer interp_matrix; /* 0x7A4, JNT_setInterpMatrix */
    void *static_value_records;    /* 0x7A8, JNT_setCurve arg0; read by JNT_getStaticVal */
    void *value_records;           /* 0x7AC, JNT_setCurve arg1; read by JNT_getVal */
    unsigned char unmodeled_7b0[0x7f0 - 0x7b0];
    unsigned int attribute_key;    /* +0x7f0 */
    unsigned char unmodeled_7f4[8];
    float current_frame;           /* +0x7fc */
    float interpolation; /* 0x800, JNT_setInterpMatrix */
    unsigned char unmodeled_804[4];
    unsigned char *current_value;  /* +0x808 */
    char *attribute_name;          /* +0x80c */
    unsigned int animation_flags;  /* +0x810 */
    unsigned char unmodeled_814[4];
    JntInterruptElement *elements; /* +0x818 */
    int channel_start_index;       /* +0x81c */
    struct JntProductionOwner *owner; /* +0x820 */
    unsigned char unmodeled_824[0x0c];
    JntInterruptElement default_element; /* +0x830: default element storage */
} JntWork;

/* Fork-ported (derwetterfrosch) records; not yet used by our C. */
struct FcvPackAttribute {
    unsigned char unmodeled_00[4];
    unsigned short channel_flags;
    unsigned short value_mask; /* 0x06 */
};

typedef struct JntMoveElement {
    unsigned short type;
    unsigned char unmodeled_02[0x3E];
} JntMoveElement;

typedef struct JntMoveElementHeader {
    unsigned char unmodeled_00[0x08];
    unsigned short element_count;
    unsigned char unmodeled_0A[0x06];
    int element_offset;
} JntMoveElementHeader;

typedef struct JntProductionOwner {
    unsigned char unmodeled_000[0xA60];
    JntMatrix *matrix_buffer; /* 0xA60 */
} JntProductionOwner;

typedef union JntRotationPayload {
    float values[3];
    int bits[3];
} JntRotationPayload;

/* 32-byte per-element record read through static_value_records/value_records. */
typedef struct JntTransformRecord {
    float translation[4]; /* Vector4 */
    float rotation[4];    /* Vector4 */
} JntTransformRecord;

/*
 * A joint element is a fixed-size record: JNT_getElement indexes an array of
 * them and JNT_nextElement advances by exactly one. The layout beyond that
 * 0x40-byte stride is not recovered, so it stays a single unmodeled span
 * (docs/naming.md) rather than invented members.
 */
typedef struct JntElement {
    unsigned char unmodeled_00[0x40];
} JntElement;

/*
 * The joint resource header JNT_getRootElement reads: 0x10 bytes not
 * recovered here, then a self-relative offset field whose own address plus
 * its value is the joint's root element.
 */
typedef struct JntElementHeader {
    unsigned char unmodeled_00[0x10];
    int root_element_offset;
} JntElementHeader;

/*
 * Another partial view of the same scratchpad work area as JntWork (it is
 * one layout; JntWork does not model these slots): the general flags word
 * at 0x38, which JNT_setFlags writes and JNT_getFlags reads (`sw $4,56($1)`
 * / `lw $2,56($2)` off the 0x70000000 base). Callers ACT_resetMatrix,
 * ACT_updateMotionSub, ACT_modelDrawSub, ACT_setMotion and ACT_setMotion2
 * read/write it around the same joint update the matrix/curve slots above
 * serve.
 */
typedef struct JntFlagsWork {
    unsigned char unmodeled_000[0x38];
    int flags;
} JntFlagsWork;

/*
 * A view of the same work area for two adjacent slots past the end of
 * JntWork:
 * JNT_animSetFlags writes the animation flags word at 0x810
 * (`sw $4,2064($1)`) and JNT_setClipR writes the clipping radius right
 * after it at 0x814 (`swc1 $f12,2068($1)`); both are supplied by
 * ACT_updateMotionSub (JNT_animSetFlags also by ACT_modelDrawSub).
 */
typedef struct JntAnimWork {
    unsigned char unmodeled_000[0x810];
    int anim_flags;
    float clip_radius;
} JntAnimWork;

typedef unsigned int JntRootStorage __attribute__((mode(TI)));
typedef union JntRootSlot {
    JntRootStorage storage;
    struct {
        float x;
        float y;
        float z;
        unsigned int opaque_w;
    } components;
} JntRootSlot;
typedef struct JntRootScratch {
    unsigned char unmodeled_00[0x40];
    JntRootSlot translation;
    JntRootSlot rotation;
    JntRootSlot scale;
} JntRootScratch;

float FCV2_getPackValue();

void get_fcvpack(float *out, float *def, unsigned char *element, int mask, float t)
{
    int i;
    int one = 1;

    for (i = 0; i < 3; i++) {
        if (mask & (one << i)) {
            out[i] = ((float (*)(void *, float))FCV2_getPackValue)(element + 4, t);
        } else {
            out[i] = *(float *)((char *)def + (i << 2));
        }
    }
}

void get_fcvpack_fix(float *out, unsigned char *element, int mask, float def, float t)
{
    int i;
    int one = 1;

    for (i = 0; i < 3; i++) {
        if (mask & (one << i)) {
            out[i] = ((float (*)(void *, float))FCV2_getPackValue)(element + 4, t);
        } else {
            out[i] = def;
        }
    }
}

float FCV2_getPackValue(void *pack);

void get_fcvpack_flag(unsigned char *element, int enable)
{
    if (enable & 1) {
        if (FCV2_getPackValue(element + 4) != 0.0f) {
            *(int *)(element + 0x38) |= 1;
        } else {
            *(int *)(element + 0x38) &= ~1;
        }
    }
}

extern int dummyAttr[];
typedef struct JntSvDst {
    float s[4];
    float b[4];
    float a[4];
    unsigned short f30;
    unsigned short f32;
    int *attr;
    int pad[2];
} JntSvDst;
static inline void JntSvCopy3(float *d, float *s) {
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

unsigned short *JNT_getStaticVal(char *work, unsigned short *src) {
    int idx = *(int *)(work + 0x2C);
    JntSvDst *dst = (JntSvDst *)(*(char **)(work + 0x818) + (idx << 6));
    int type = *src;
    unsigned short *ret = src + 32;
    int next = idx + 1;
    float *d;
    float *d2;

    switch (type) {
    case 0:
        break;
    case 5:
        JntSvCopy3(dst->a, (float *)(src + 8));
        JntSvCopy3(dst->b, (float *)(src + 16));
        break;
    case 1:
    case 2:
    case 3:
    case 4:
        JntSvCopy3(dst->a, (float *)(src + 8));
        JntSvCopy3(dst->b, (float *)(src + 16));
        dst->s[2] = 1.0f;
        dst->s[1] = 1.0f;
        dst->s[0] = 1.0f;
        break;
    }
    if (*(int *)(work + 0x7AC) != 0) {
        char *tbl = *(char **)(work + 0x7A8);
        if (tbl != 0) {
            d = (float *)((idx << 5) + (int)tbl);
            d2 = d + 4;
            JntSvCopy3(d, (float *)(src + 8));
            JntSvCopy3(d2, (float *)(src + 16));
            d[3] = 1.0f;
            d2[3] = 1.0f;
        }
    }
    *(int *)(work + 0x2C) = next;
    dst->f30 = 0;
    dst->attr = dummyAttr;
    dst->f32 = 0;
    return ret;
}

void *FCV2_getPackAttribute(void *pack);

void *FCV_getAttrAndValue(float *out, void *pack, float t)
{
    void *attr;
    unsigned int flags;
    int i;
    unsigned int b;

    i = 6;
    attr = FCV2_getPackAttribute(pack);
    flags = *(unsigned short *)((char *)attr + 6);
    do {
        if (flags & 1) {
            out[0] = ((float (*)(void *, float))FCV2_getPackValue)(pack, t);
        } else {
            out[0] = 0.0f;
        }
        if (flags & 2) {
            out[1] = ((float (*)(void *, float))FCV2_getPackValue)(pack, t);
        } else {
            out[1] = 0.0f;
        }
        b = flags & 4;
        flags >>= 3;
        if (b) {
            out[2] = ((float (*)(void *, float))FCV2_getPackValue)(pack, t);
        } else {
            out[2] = 0.0f;
        }
        i -= 3;
        out += 4;
    } while (i >= 0);
    if (flags & 1) {
        out[0] = ((float (*)(void *, float))FCV2_getPackValue)(pack, t);
    } else {
        out[0] = 0.0f;
    }
    return attr;
}

/*
 * A joint static-value channel: JNT_getStaticVal2 reads its type (dispatch,
 * only types 1-5 apply a transform), its target joint slot and the
 * translate/rotate triples it feeds to CUR_MATRIX_Txyz4s/Rzyx4s, then
 * returns the following channel (`channel + 1`), which fixes its size at
 * 0x40 bytes, the same stride as JntElement.
 */
typedef struct JntStaticChannel {
    unsigned short type;           /* 0x00 */
    unsigned char unmodeled_02[2];
    int rotation_limits_offset;    /* 0x04 */
    unsigned char unmodeled_08[6];
    unsigned short matrix_index;   /* 0x0E */
    float translate[3];            /* 0x10 */
    unsigned char unmodeled_1c[4];
    float rotate[3];               /* 0x20 (fork: JntRotationPayload, float/int view) */
    unsigned char unmodeled_2C[0x14];
} JntStaticChannel;

#include "shared.h"
typedef struct JntRotationLimitHeader {
    unsigned short values_offset;
} JntRotationLimitHeader;
typedef struct JntRotationLimits {
    float minimum[3];
    unsigned short flags;
    unsigned char unmodeled_0e[2];
    float maximum[3];
} JntRotationLimits;
typedef struct JntPreviousTransform {
    Vector4 translation;
    Vector4 rotation;
} JntPreviousTransform;
typedef struct JntValRecord {
    Vector4 translation;
    Vector4 rotation;
} JntValRecord;

JntStaticChannel *JNT_getVal(JntWork *work, JntStaticChannel *channel,
                            float frame)
{
    JntInterruptElement *element = &work->elements[work->current_index];
    const float *translation = channel->translate;
    const float *rotation = channel->rotate;
    void *attribute_data;
    unsigned int channel_flags;
    unsigned int value_mask;
    int inherit_channels;
    JntStaticChannel *next_channel;
    const float unit_scale = 1.0f;
    unsigned int scale_mask;

    element->interrupt_channel = 0;
    if (work->flags & 4) {
        value_mask = 0;
        attribute_data = dummyAttr;
        channel_flags = 0;
    } else {
        struct FcvPackAttribute *attribute = FCV2_getPackAttribute(&work->cursor);

        attribute_data = attribute;
        if (attribute->channel_flags == 0xffff) {
            element->key = 0;
            element->attribute_data = 0;
            work->flags |= 4;
            return channel;
        }
        channel_flags = attribute->channel_flags;
        value_mask = attribute->value_mask;
    }
    inherit_channels = channel_flags & 0x8000;
    element->attribute_data = attribute_data;
    element->key = channel_flags;
    next_channel = channel + 1;
    if (inherit_channels) {
        work->channel_start_index = work->current_index;
        get_fcvpack(element->translation, (float *)translation, (unsigned char *)work, value_mask, frame);
        value_mask >>= 3;
        get_fcvpack(element->rotation, (float *)rotation, (unsigned char *)work, value_mask, frame);
        value_mask >>= 3;
        ((void (*)(float *, float, void *, unsigned int, float))get_fcvpack_fix)(element->scale, unit_scale, work, value_mask, frame);
        ((void (*)(void *, int, float))get_fcvpack_flag)(work, value_mask >> 1, frame);
        value_mask = 0;
    }
    switch (channel->type) {
    case 0:
    case 1:
        if (!inherit_channels) {
            get_fcvpack(element->translation, (float *)translation, (unsigned char *)work, value_mask, frame);
            value_mask >>= 3;
            get_fcvpack(element->rotation, (float *)rotation, (unsigned char *)work, value_mask, frame);
            ((void (*)(float *, float, void *, unsigned int, float))get_fcvpack_fix)(element->scale, 1.0f, work, value_mask >> 3, frame);
        }
        break;
    case 3:
        get_fcvpack(element->translation, (float *)translation, (unsigned char *)work, value_mask, frame);
        get_fcvpack(element->rotation, (float *)rotation, (unsigned char *)work, value_mask >> 3, frame);
        break;
    case 2:
        {
            float (*angles)[3] = &element->rotation;
            float *sampled_translation;
            int limits_offset;

            value_mask >>= 3;
            get_fcvpack(*angles, (float *)rotation, (unsigned char *)work, value_mask, frame);
            ((void (*)(float *, float, void *, unsigned int, float))get_fcvpack_fix)(element->scale, unit_scale, work, value_mask >> 3, frame);
            sampled_translation = element->translation;
            limits_offset = channel->rotation_limits_offset;
            sampled_translation[0] = translation[0];
            sampled_translation[1] = translation[1];
            sampled_translation[2] = translation[2];
            if (limits_offset != 0) {
                JntRotationLimitHeader *header = (JntRotationLimitHeader *)
                    ((unsigned char *)channel + limits_offset);
                int index = 0;

                if (header->values_offset != 0) {
                    JntRotationLimits *limits = (JntRotationLimits *)
                        ((unsigned char *)header + header->values_offset);
                    unsigned short flags = limits->flags;
                    const float *minimum = limits->minimum;

                    do {
                        if ((flags >> index) & 1) {
                            float angle = (*angles)[(unsigned int)index];
                            float limit = minimum[index];

                            if (angle < limit) {
                                (*angles)[(unsigned int)index] = limit;
                            }
                        }
                        index++;
                    } while (index < 3);
                    {
                        const float *maximum = limits->maximum;

                        flags >>= 3;
                        index = 0;
                        do {
                            if ((flags >> index) & 1) {
                                float angle = (*angles)[(unsigned int)index];
                                float limit = maximum[index];

                                if (angle > limit) {
                                    (*angles)[(unsigned int)index] = limit;
                                }
                            }
                            index++;
                        } while (index < 3);
                    }
                }
            }
        }
        break;
    case 4:
        {
            float *sampled_translation = element->translation;

            sampled_translation[0] = translation[0];
            sampled_translation[1] = translation[1];
            sampled_translation[2] = translation[2];
        }
        break;
    case 5:
        {
            get_fcvpack(element->translation, (float *)translation, (unsigned char *)work, value_mask, frame);
            value_mask >>= 3;
            get_fcvpack(element->rotation, (float *)rotation, (unsigned char *)work, value_mask, frame);
            scale_mask = value_mask >> 3;
            ((void (*)(float *, float, void *, unsigned int, float))get_fcvpack_fix)(element->scale, unit_scale, work, scale_mask, frame);
        }
        break;
    case 6:
        break;
    }
    {
        const JntPreviousTransform *previous = work->value_records;

    if (previous != 0 && work->static_value_records != 0) {
        JntValRecord *records = (JntValRecord *)work->static_value_records;

        if (work->flags & 0x80) {
            const Vector4 *previous_translation;
            const Vector4 *previous_rotation;

            previous_translation = (const Vector4 *)((work->current_index << 5) +
                (unsigned int)previous);

            if (previous_translation->w != 0.0f) {
                previous_rotation = previous_translation + 1;
                element->translation[0] = previous_translation->x +
                    (element->translation[0] - previous_translation->x) * work->interpolation;
                element->translation[1] = previous_translation->y +
                    (element->translation[1] - previous_translation->y) * work->interpolation;
                element->translation[2] = previous_translation->z +
                    (element->translation[2] - previous_translation->z) * work->interpolation;
                element->rotation[0] = previous_rotation->x +
                    (element->rotation[0] - previous_rotation->x) * work->interpolation;
                element->rotation[1] = previous_rotation->y +
                    (element->rotation[1] - previous_rotation->y) * work->interpolation;
                element->rotation[2] = previous_rotation->z +
                    (element->rotation[2] - previous_rotation->z) * work->interpolation;
            }
        }
        {
            float *sampled_translation = element->translation;
            float *sampled_rotation = element->rotation;
            JntValRecord *record = (JntValRecord *)
                ((work->current_index << 5) + (unsigned int)records);
            Vector4 *stored_rotation = &record->rotation;

            record->translation.x = sampled_translation[0];
            record->translation.y = sampled_translation[1];
            record->translation.z = sampled_translation[2];
            stored_rotation->x = sampled_rotation[0];
            stored_rotation->y = sampled_rotation[1];
            stored_rotation->z = sampled_rotation[2];
            record->translation.w = 1.0f;
            stored_rotation->w = 1.0f;
        }
    }
    }
    work->current_index++;
    return next_channel;
}
/* ported */

JntStaticChannel *JNT_getVal_staticChain(JntWork *work,
                                        JntStaticChannel *channel, float frame)
{
    int index;
    JntInterruptElement *element = &work->elements[work->current_index];
    const float *translation = channel->translate;
    const float *rotation = channel->rotate;
    void *attribute_data;
    unsigned int channel_flags;
    unsigned int value_mask;
    int inherit_channels;
    JntStaticChannel *next_channel;

    element->interrupt_channel = 0;
    if (work->flags & 4) {
        value_mask = 0;
        attribute_data = dummyAttr;
        channel_flags = 0;
    } else {
        struct FcvPackAttribute *attribute = FCV2_getPackAttribute(&work->cursor);

        attribute_data = attribute;
        if (attribute->channel_flags == 0xffff) {
            element->key = 0;
            element->attribute_data = 0;
            work->flags |= 4;
            return channel;
        }
        channel_flags = attribute->channel_flags;
        value_mask = attribute->value_mask;
    }
    element->key = channel_flags;
    inherit_channels = (unsigned int)channel_flags & 0x8000;
    next_channel = channel + 1;
    element->attribute_data = attribute_data;
    if (inherit_channels) {
        work->channel_start_index = work->current_index;
        get_fcvpack(element->translation, (float *)translation, (unsigned char *)work, value_mask, frame);
        value_mask >>= 3;
        get_fcvpack(element->rotation, (float *)rotation, (unsigned char *)work, value_mask, frame);
        value_mask >>= 3;
        ((void (*)(float *, float, void *, unsigned int, float))get_fcvpack_fix)(element->scale, 1.0f, work, value_mask, frame);
        ((void (*)(void *, int, float))get_fcvpack_flag)(work, value_mask >> 1, frame);
        value_mask = 0;
    }
    switch (channel->type) {
    case 0:
    case 1:
        if (!inherit_channels) {
            unsigned int scale_mask;

            get_fcvpack(element->translation, (float *)translation, (unsigned char *)work, value_mask, frame);
            value_mask >>= 3;
            get_fcvpack(element->rotation, (float *)rotation, (unsigned char *)work, value_mask, frame);
            scale_mask = value_mask >> 3;
            ((void (*)(float *, float, void *, unsigned int, float))get_fcvpack_fix)(element->scale, 1.0f, work, scale_mask, frame);
        }
        break;
    case 3:
        {
            for (index = 0; index < 3; index++) {
                int channel_bit = 1 << index;

                if (value_mask & channel_bit) {
                    element->translation[index] = ((float (*)(void *, float))FCV2_getPackValue)(&work->cursor, frame);
                }
                {
                    const float *static_sample = translation;

                    static_sample += index;
                    element->translation[index] = *static_sample;
                }
            }
        }
        {
            unsigned int rotation_mask = value_mask >> 3;

            get_fcvpack(element->rotation, (float *)rotation, (unsigned char *)work, rotation_mask, frame);
        }
        break;
    case 2:
        {
            float (*angles)[3];
            float *sampled_translation;
            int limits_offset;

            value_mask >>= 3;
            angles = &element->rotation;
            get_fcvpack(*angles, (float *)rotation, (unsigned char *)work, value_mask, frame);
            ((void (*)(float *, float, void *, unsigned int, float))get_fcvpack_fix)(element->scale, 1.0f, work, value_mask >> 3, frame);
            sampled_translation = element->translation;
            limits_offset = channel->rotation_limits_offset;
            sampled_translation[0] = translation[0];
            sampled_translation[1] = translation[1];
            sampled_translation[2] = translation[2];
            if (limits_offset != 0) {
                JntRotationLimitHeader *header = (JntRotationLimitHeader *)
                    ((unsigned char *)channel + limits_offset);

                index = 0;
                if (header->values_offset != 0) {
                    JntRotationLimits *limits = (JntRotationLimits *)
                        ((unsigned char *)header + header->values_offset);
                    unsigned short flags = limits->flags;
                    const float *axis_limits = limits->minimum;

                    for (index = 0; index < 3; index++) {
                        if ((flags >> index) & 1) {
                            float angle = (*angles)[(unsigned int)index];
                            float limit = axis_limits[index];

                            if (angle < limit) {
                                (*angles)[(unsigned int)index] = limit;
                            }
                        }
                    }
                    {
                        axis_limits = limits->maximum;

                        flags >>= 3;
                        index = 0;
                        do {
                            if ((flags >> index) & 1) {
                                float angle = (*angles)[(unsigned int)index];
                                float limit = axis_limits[index];

                                if (angle > limit) {
                                    (*angles)[(unsigned int)index] = limit;
                                }
                            }
                            index++;
                        } while (index < 3);
                    }
                }
            }
        }
        break;
    case 4:
        {
            float *sampled_translation = element->translation;

            sampled_translation[0] = translation[0];
            sampled_translation[1] = translation[1];
            sampled_translation[2] = translation[2];
        }
        break;
    case 6:
        work->current_index++;
        return next_channel;
    case 5:
        get_fcvpack(element->translation, (float *)translation, (unsigned char *)work, value_mask, frame);
        value_mask >>= 3;
        get_fcvpack(element->rotation, (float *)rotation, (unsigned char *)work, value_mask, frame);
        ((void (*)(float *, float, void *, unsigned int, float))get_fcvpack_fix)(element->scale, 1.0f, work, value_mask >> 3, frame);
        break;
    }
    work->current_index++;
    return next_channel;
}

INCLUDE_ASM("asm/main/nonmatchings/jnt", JNT_constructMatrix);

/*
 * CUR_MATRIX_* (defined in another TU, not yet recovered there) form a
 * current-matrix-register API: Set/Get load and store a JntMatrix through
 * it, Txyz4s/Rzyx4s apply a translation/rotation from a float triple onto it.
 */
void CUR_MATRIX_Set(JntMatrix *matrix);
void CUR_MATRIX_Get(JntMatrix *matrix);
void CUR_MATRIX_Txyz4s(float *translate);
void CUR_MATRIX_Rzyx4s(float *rotate);

/*
 * The type-1..4 body below (label apply_index) and the type==5 body above it
 * are byte-identical in the original (both do
 * CUR_MATRIX_Set/Txyz4s/Rzyx4s/Get on the same arguments), but the original
 * still emits them as two separate instruction sequences: the type==5 copy
 * falls straight through into an unconditional jump to the shared tail,
 * while the type 1..4 copy is a separate out-of-line block that falls
 * straight through into that same tail. A structured if/else-if or two
 * independent ifs both let 2.96 fold the two identical call sequences into
 * one shared block reached from two jumps, which the original does not do;
 * the goto below reproduces the original's two independently-placed copies.
 */
void *JNT_getStaticVal2(JntWork *work, JntStaticChannel *channel)
{
    int type;
    float *translate;
    float *rotate;
    void *next;

    type = channel->type;
    translate = channel->translate;
    rotate = channel->rotate;
    next = channel + 1;

    if (type < 5) {
        if (type == 0) {
            goto done;
        }
        goto apply_index;
    }
    if (type == 5) {
        CUR_MATRIX_Set(&work->matrix_buffer[channel->matrix_index]);
        CUR_MATRIX_Txyz4s(translate);
        CUR_MATRIX_Rzyx4s(rotate);
        CUR_MATRIX_Get(&work->matrix_buffer[work->current_index]);
    }
    goto done;

apply_index:
    CUR_MATRIX_Set(&work->matrix_buffer[channel->matrix_index]);
    CUR_MATRIX_Txyz4s(translate);
    CUR_MATRIX_Rzyx4s(rotate);
    CUR_MATRIX_Get(&work->matrix_buffer[work->current_index]);

done:
    work->current_index++;
    return next;
}

INCLUDE_ASM("asm/main/nonmatchings/jnt", JNT_getVal2);

INCLUDE_ASM("asm/main/nonmatchings/jnt", JNT_getVal_HOGE2);

static void liner_interpolate(float *a, float *b, float t)
{
    __asm__ __volatile__(
        "mfc1 $3, %2\n"
        "qmtc2.ni $3, $vf31\n"
        "lqc2 $vf22, 0x30(%0)\n"
        "lqc2 $vf23, 0x30(%1)\n"
        "lqc2 $vf19, 0x0(%0)\n"
        "lqc2 $vf20, 0x10(%0)\n"
        "lqc2 $vf21, 0x20(%0)\n"
        "vsub.xyz $vf24, $vf22, $vf23\n"
        "vmove.w $vf19, $vf0\n"
        "vmove.w $vf20, $vf0\n"
        "vmove.w $vf21, $vf0\n"
        "vmulx.xyz $vf24, $vf24, $vf31x\n"
        "vadd.xyz $vf24, $vf24, $vf23\n"
        "sqc2 $vf19, 0x0(%0)\n"
        "sqc2 $vf20, 0x10(%0)\n"
        "sqc2 $vf21, 0x20(%0)\n"
        "sqc2 $vf24, 0x30(%0)\n"
        "nop\n"
        : : "r"(a), "r"(b), "f"(t) : "$3", "memory");
}

#include "shared.h"
typedef struct JntInterpMatrix {
    Matrix4 elements;
} JntInterpMatrix;
typedef struct JntInterpWork {
    unsigned char unmodeled_000[0x2C];
    int current_index;                 /* 0x2C */
    unsigned char unmodeled_030[8];
    unsigned int flags;                /* 0x38 */
    unsigned char unmodeled_03c[0x760 - 0x3C];
    Matrix4 interpolation_root;        /* 0x760 */
    JntInterpMatrix *matrix_buffer;    /* 0x7A0 */
    JntInterpMatrix *interp_matrix;    /* 0x7A4 */
    unsigned char unmodeled_7a8[0x800 - 0x7A8];
    float interpolation;               /* 0x800 */
} JntInterpWork;
typedef struct JntInterpChannel {
    unsigned short type;                /* 0x00 */
    unsigned short interpolation_flags; /* 0x02 */
    unsigned char unmodeled_04[0x0A];
    unsigned short matrix_index;        /* 0x0E */
    float translate[3];                 /* 0x10 */
    unsigned char unmodeled_1c[0x24];
} JntInterpChannel;
void MATRIX_translate4s(Matrix4 matrix, float x, float y, float z);
void xglMatrix2Quaternion(float quaternion[4], const void *matrix);
void xglQuaternion2Matrix(Matrix4 destination, const Vector4 *quaternion);
void xglQuaternionInterpolateLinear(Vector4 *destination, const Vector4 *left, const Vector4 *right, float left_weight, float right_weight);
void xglVectorLength(float *length, const Vector4 *vector);
static inline void jntInterpCopyMatrix(JntInterpMatrix *destination, const JntInterpMatrix *source)
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)\n\t"
        "lq $2, 16(%1)\n\t"
        "sq $2, 16(%0)\n\t"
        "lq $2, 32(%1)\n\t"
        "sq $2, 32(%0)\n\t"
        "lq $2, 48(%1)\n\t"
        "sq $2, 48(%0)"
        : : "r"(destination), "r"(source) : "$2", "memory");
}
static inline void jntInterpCopyMatrixRow(float destination[4], const float source[4])
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)"
        : : "r"(destination), "r"(source) : "$2", "memory");
}
static inline void jntCopyTranslation(float destination[3], const float translation[3])
{
    destination[0] = translation[0];
    destination[1] = translation[1];
    destination[2] = translation[2];
}

JntStaticChannel *JNT_interpolate(JntInterpWork *work, JntInterpChannel *channel)
{
    JntInterpMatrix result_matrix;
    Vector4 left_quaternion;
    Vector4 right_quaternion;
    Vector4 result_quaternion;
    JntInterpChannel *next_channel = channel + 1;
    float left_weight = 1.0f - work->interpolation;
    float right_weight = work->interpolation;

    switch (channel->type) {
    case 0:
    case 1:
        liner_interpolate((float *)&work->matrix_buffer[work->current_index],
                          (float *)&work->interp_matrix[work->current_index],
                          work->interpolation);
        break;
    case 3:
        {
        jntInterpCopyMatrix((JntInterpMatrix *)&work->interpolation_root, &work->matrix_buffer[channel->matrix_index]);
        MATRIX_translate4s(work->interpolation_root, channel->translate[0],
                           channel->translate[1], channel->translate[2]);
        {
            jntInterpCopyMatrix(&work->matrix_buffer[work->current_index], (JntInterpMatrix *)&work->interpolation_root);
            jntCopyTranslation(work->matrix_buffer[work->current_index + 1].elements[3],
                               work->matrix_buffer[work->current_index].elements[3]);
        }
        break;
        }
    case 4:
        {
        if (channel->interpolation_flags != 0) {
            xglMatrix2Quaternion((float *)&left_quaternion,
                                 &work->interp_matrix[work->current_index]);
            xglMatrix2Quaternion((float *)&right_quaternion,
                                 &work->matrix_buffer[work->current_index]);
            xglQuaternionInterpolateLinear(&result_quaternion,
                                           &left_quaternion,
                                           &right_quaternion,
                                           left_weight, right_weight);
            xglQuaternion2Matrix(result_matrix.elements, &result_quaternion);
            result_matrix.elements[0][3] = result_matrix.elements[1][3] =
                result_matrix.elements[2][3] = result_matrix.elements[3][3] = 1.0f;
            jntCopyTranslation(result_matrix.elements[3],
                               work->matrix_buffer[work->current_index].elements[3]);
            jntInterpCopyMatrix(&work->matrix_buffer[work->current_index], &result_matrix);
        }
        break;
        }
    case 2:
        {
        float scale;
        if (work->flags & 0x40000) {
            if ((work->current_index == 0x30) ||
                (work->current_index == 0x24)) {
                left_weight = 0.0f;
                right_weight = 1.0f;
            }
        }
        xglMatrix2Quaternion((float *)&left_quaternion,
                             &work->interp_matrix[work->current_index]);
        xglMatrix2Quaternion((float *)&right_quaternion,
                             &work->matrix_buffer[work->current_index]);
        xglQuaternionInterpolateLinear(&result_quaternion, &left_quaternion,
                                       &right_quaternion, left_weight,
                                       right_weight);
        xglQuaternion2Matrix(result_matrix.elements, &result_quaternion);
        if (work->flags & 0x20000) {
            scale = 0.0f;
            xglVectorLength(&scale, (const Vector4 *)&work->interp_matrix[work->current_index].elements[0][0]);
            result_matrix.elements[0][3] = scale;
            xglVectorLength(&scale, (const Vector4 *)&work->interp_matrix[work->current_index].elements[1][0]);
            result_matrix.elements[1][3] = scale;
            xglVectorLength(&scale, (const Vector4 *)&work->interp_matrix[work->current_index].elements[2][0]);
            result_matrix.elements[2][3] = scale;
            result_matrix.elements[3][3] = 1.0f;
        } else {
            result_matrix.elements[0][3] = result_matrix.elements[1][3] =
                result_matrix.elements[2][3] = result_matrix.elements[3][3] = 1.0f;
        }
        jntInterpCopyMatrixRow(result_matrix.elements[3],
                         work->matrix_buffer[work->current_index].elements[3]);
        jntInterpCopyMatrix(&work->matrix_buffer[work->current_index], &result_matrix);
        MATRIX_translate4s(result_matrix.elements,
                           next_channel->translate[0], 0.0f, 0.0f);
        jntCopyTranslation(work->matrix_buffer[work->current_index + 1].elements[3],
                           result_matrix.elements[3]);
        break;
        }
    case 5:
    case 6:
        break;
    }
    work->current_index++;
    return (JntStaticChannel *)next_channel;
}

void JNT_setFlags(int flags)
{
    JntFlagsWork *work = (JntFlagsWork *)JNT_SCRATCH_BASE;
    work->flags = flags;
}

void JNT_setClipR(float clip_radius)
{
    JntAnimWork *work = (JntAnimWork *)JNT_SCRATCH_BASE;
    work->clip_radius = clip_radius;
}

void JNT_animSetFlags(int anim_flags)
{
    JntAnimWork *work = (JntAnimWork *)JNT_SCRATCH_BASE;
    work->anim_flags = anim_flags;
}

int JNT_getFlags(void)
{
    JntFlagsWork *work = (JntFlagsWork *)JNT_SCRATCH_BASE;
    return work->flags;
}

void *JNT_getAccessories(JntMoveResource *move)
{
    unsigned int accumulated = 4;
    JntAccessoryRecord *record;
    int *chunk_size;

    if (move == 0) {
        return 0;
    }
    chunk_size = &move->accessory_size;
    record = (JntAccessoryRecord *)(move + 1);
    for (;;) {
        if (record->type == 4) {
            return record + 1;
        }
        if (record->type == 0) {
            break;
        }
        {
        unsigned int size = record->size;
        accumulated += size;
        record = (JntAccessoryRecord *)((unsigned char *)record + size);
        if (accumulated >= (unsigned int)*chunk_size) {
            break;
        }
        }
    }
    return 0;
}

extern char dummyCntFree[];
extern char dummyName[];
char *strncpy(char *dst, const char *src, unsigned int n);
typedef struct JntAttributeWork {
    unsigned char unmodeled_000[0x7F0];
    int name_hash;          /* 0x7F0 */
    unsigned char unmodeled_7f4[0x14];
    void *count_free;       /* 0x808 */
    char *name;             /* 0x80C */
} JntAttributeWork;

void JNT_readAttribute(JntAttributeWork *work, unsigned int *attr)
{
    unsigned int accumulated = 4;
    JntAccessoryRecord *record = (JntAccessoryRecord *)(attr + 1);

    work->count_free = dummyCntFree;
    work->name = dummyName;
    dummyName[0] = 0;
    do {
        switch (record->type) {
        case 5:
            work->count_free = record + 1;
            break;
        case 1: {
            int *data = (int *)(record + 1);
            work->name_hash = data[0] * 31 + data[1];
            strncpy(work->name, (char *)data, 8);
            work->name[8] = 0;
            break;
        }
        }
        if (record->type == 0) {
            break;
        }
        accumulated += record->size;
        record = (JntAccessoryRecord *)((unsigned char *)record + record->size);
    } while (accumulated < *attr);
}

/* JNT matrix-slot setters.
 *
 *
 * Semantics (facts):
 * - JNT_setMatrix stores a0 at scratchpad 0x700007A0 and zero at 0x700004BC:
 *     lui $v0,0x7000 / sw $a0,1952($v0) / jr $ra / sw $zero,1212($v0).
 * - JNT_setMatrix2 stores a1 at 0x700004BC and a0 at 0x700007A0, the buffer
 *   store filling the jr delay slot:
 *     lui $v0,0x7000 / sw $a1,1212($v0) / jr $ra / sw $a0,1952($v0).
 * - Caller ACT_resetMatrix (bounded jal sites at VA 0x00307620/0x0030765C)
 *   supplies caller object words +0x824/+0x828 (lw a0,2084/2088(s1) in the
 *   jal delay slots) as the buffer argument, and the surrounding reset path
 *   clears select-adjacent scratchpad words; that is the evidence for the
 *   0x7A0 buffer slot / 0x4BC select slot split. No unobserved workspace
 *   byte is asserted.
 *
 */

void JNT_setMatrix(JntMatrixBuffer matrix_buffer)
{
    unsigned char *scratch = JNT_SCRATCH_BASE;
    JNT_MATRIX_BUFFER(scratch) = matrix_buffer;
    JNT_MATRIX_SELECT(scratch) = 0;
}

void JNT_setMatrix2(JntMatrixBuffer matrix_buffer, JntMatrixSelect matrix_select)
{
    unsigned char *scratch = JNT_SCRATCH_BASE;
    JNT_MATRIX_BUFFER(scratch) = matrix_buffer;
    JNT_MATRIX_SELECT(scratch) = matrix_select;
}

void JNT_setInterpMatrix(JntMatrixBuffer matrix_buffer, float interpolation)
{
    JntWork *work = (JntWork *)JNT_SCRATCH_BASE;
    work->interp_matrix = matrix_buffer;
    work->interpolation = interpolation;
}

void JNT_setCurve(void *static_value_records, void *value_records)
{
    JntWork *work = (JntWork *)JNT_SCRATCH_BASE;
    work->static_value_records = static_value_records;
    work->value_records = value_records;
}

INCLUDE_ASM("asm/main/nonmatchings/jnt", JNT_setMDLMatrix);

void *JNT_setMDLMatrix(void *work, void *p);

void JNT_setModelMatrix(void)
{
    void *p;
    unsigned short *hdr;
    int end;
    int i;

    if (*(void **)0x70000030 != 0) {
        hdr = *(unsigned short **)0x70000028;
        p = *(void **)0x70000030;
        *(int *)0x7000002C = 0;
        end = hdr[4] + hdr[5];
        *(short *)0x700004C0 = 0;
        i = 0;
        if (end != 0) {
            do {
                p = JNT_setMDLMatrix((void *)0x70000000, p);
                i = *(int *)0x7000002C;
            } while (i < end);
            hdr = *(unsigned short **)0x70000028;
        }
        end = hdr[3];
        *(short *)0x700004C0 = 0;
        if (i < end) {
            do {
                p = JNT_setMDLMatrix((void *)0x70000000, p);
            } while (*(int *)0x7000002C < end);
        }
    }
}

int JNT_getMoveElement(char *p)
{
    int i;
    char *base = p + 0x10;
    int n = *(unsigned short *)(p + 8);
    char *e = base + *(int *)base;

    for (i = 0; i < n; i++) {
        if (*(unsigned short *)e != 1) {
            return i - 1;
        }
        e += 0x40;
    }
    return 0;
}

void *JNT_getRootElement(void *joint)
{
    int *root_element_offset = &((JntElementHeader *)joint)->root_element_offset;
    return (unsigned char *)root_element_offset + *root_element_offset;
}

void JNT_setModel(void *matrix, unsigned char *model)
{
    unsigned char *scratch = (unsigned char *)0x70000000;
    *(void **)(scratch + 0x34) = matrix;
    *(void **)(scratch + 0x28) = model;
    *(int *)(scratch + 0x7F0) = 0;
    if (model == 0) {
        *(void **)0x70000030 = 0;
        return;
    }
    if (*(unsigned short *)(model + 4) >= 2) {
        int *attr = (int *)(model + 0x10);
        JNT_readAttribute((JntAttributeWork *)0x70000000, (unsigned int *)attr);
        *(void **)0x70000030 = (unsigned char *)attr + *attr;
        return;
    }
    *(void **)0x70000030 = model + 0x10;
}

void FCV2_resetPack(void *pack, void *fcurve);
typedef struct JntFcvWork0 {
    unsigned char unmodeled_00[0x38];
    int flags;      /* 0x38 */
    short count;    /* 0x3C */
    unsigned char unmodeled_3e[0xE];
    float scale0;   /* 0x4C */
    unsigned char unmodeled_50[0xC];
    float scale1;   /* 0x5C */
    unsigned char unmodeled_60[0xC];
    float scale2;   /* 0x6C */
    unsigned char unmodeled_70[0x7A8];
    void *ptr;      /* 0x818 */
} JntFcvWork0;

void JNT_setFCurve(void *fcurve, int n)
{
    JntFcvWork0 *w = (JntFcvWork0 *)JNT_SCRATCH_BASE;
    int flags = w->flags;
    w->count = n;
    w->scale0 = 1.0f;
    w->flags = flags | 1;
    w->scale1 = 1.0f;
    w->scale2 = 1.0f;
    if (fcurve == 0) {
        w->flags = flags | 0x21;
    } else {
        FCV2_resetPack((unsigned char *)w + 4, fcurve);
    }
    w->ptr = (unsigned char *)w + 0x830;
}

float FCV2_getPackValue(void *pack);
typedef struct JntFcvWork {
    unsigned char unmodeled_00[0x38];
    int flags;      /* 0x38 */
    short count;    /* 0x3C */
    unsigned char unmodeled_3e[0xE];
    float scale0;   /* 0x4C */
    unsigned char unmodeled_50[0xC];
    float scale1;   /* 0x5C */
    unsigned char unmodeled_60[0xC];
    float scale2;   /* 0x6C */
    unsigned char unmodeled_70[0x730];
    void *buffer;   /* 0x7A0 */
} JntFcvWork;

void JNT_setFCurve2(void *fcurve, void *buf, int n)
{
  JntFcvWork *w = (JntFcvWork *) ((unsigned char *) 0x70000000);
  int flags;
  FCV2_resetPack(((unsigned char *) w) + 4, fcurve);
  flags = w->flags;
  w->buffer = buf;
  w->count = n;
  w->flags = flags | 1;
  w->scale0 = 1.0f;
  w->scale1 = 1.0f;
  w->scale2 = 1.0f;
}

/* The original commits the output quadword before JR/NOP; form03's
 * unqualified member copy instead commits it in the return delay slot. */
JntRootStorage JNT_getRootTrans(volatile JntRootSlot *destination)
{
    const JntRootScratch *work = JNT_SCRATCH_BASE;
    unsigned int workAddress = (unsigned int)work;
    const JntRootSlot *translation = (const JntRootSlot *)
        (workAddress + sizeof(work->unmodeled_00));
    JntRootStorage value = translation->storage;
    destination->storage = value;
    return value;
}

/* The original commits the output quadword before JR/NOP; form03's
 * unqualified member copy instead commits it in the return delay slot. */
JntRootStorage JNT_getRootRotate(volatile JntRootSlot *destination)
{
    const JntRootScratch *work = JNT_SCRATCH_BASE;
    unsigned int workAddress = (unsigned int)work;
    const JntRootSlot *rotation = (const JntRootSlot *)
        (workAddress + sizeof(work->unmodeled_00) + sizeof(work->translation));
    JntRootStorage value = rotation->storage;
    destination->storage = value;
    return value;
}

/* The original commits the output quadword before JR/NOP; form03's
 * unqualified member copy instead commits it in the return delay slot. */
JntRootStorage JNT_getRootScale(volatile JntRootSlot *destination)
{
    const JntRootScratch *work = JNT_SCRATCH_BASE;
    unsigned int workAddress = (unsigned int)work;
    const JntRootSlot *scale = (const JntRootSlot *)
        (workAddress + sizeof(work->unmodeled_00) + sizeof(work->translation) +
         sizeof(work->rotation));
    JntRootStorage value = scale->storage;
    destination->storage = value;
    return value;
}

typedef struct JntConsumerList {
    int count;
    int ids[8];
    void *funcs[8];
    int flags[8];
} JntConsumerList;

int JNT_addConsumer(JntConsumerList *list, int id, void *func, int flags)
{
    int count = list->count;
    int slot = -1;
    int i;

    for (i = 0; i < count; i++) {
        if (list->ids[i] == id) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        slot = count;
        list->count = slot + 1;
    }
    list->ids[slot] = id;
    list->funcs[slot] = func;
    list->flags[slot] = flags;
    return 0;
}

/*
 * A joint producer: JNT_initProducer's own evidence is a consumer count, an
 * 8-entry array at 0x24 it clears, and three trailing words it resets to an
 * idle state (0x64/0x68 to zero, 0x6C to -1).
 */
struct JntProducer {
    int count;              /* 0x00 */
    unsigned char unmodeled_04[0]; /* +0x4: zero-length alias of the former padding member, kept for saved bodies */
    int consumer_ids[8];    /* 0x04 */
    int consumers[8];       /* 0x24, consumer fns (fork: JntProductionConsumer), called by JNT_startProduction */
    unsigned char unmodeled_44[0]; /* +0x44: zero-length alias of the former padding member, kept for saved bodies */
    int consumer_arguments[8]; /* 0x44 */
    int pending_output;     /* 0x64 */
    int output_count;       /* 0x68, only evidenced as cleared by JNT_initProducer */
    int current_index;      /* 0x6C, -1 when none */
    int state;              /* 0x70, cleared by JNT_startProduction */
};

void JNT_initProducer(JntProducer *producer)
{
    int i;

    producer->pending_output = 0;
    producer->count = 0;
    producer->output_count = 0;
    producer->current_index = -1;
    for (i = 7; i >= 0; i--) {
        producer->consumers[i] = 0;
    }
}

typedef struct JntProduction {
    int count;                                         /* 0x00 */
    unsigned char unmodeled_04[0x20];
    void (*produce)(void *work, int arg, int param);   /* 0x24 */
    unsigned char unmodeled_28[0x1C];
    int param;                                         /* 0x44 */
    unsigned char unmodeled_48[0x1C];
    int result;                                        /* 0x64 */
    unsigned char unmodeled_68[0x8];
    int state;                                         /* 0x70 */
} JntProduction;

int JNT_startProduction(JntProduction *prod, void *target, float time)
{
    unsigned char *scratch = (unsigned char *)0x70000000;
    int arg;
    int param;
    void (*fn)(void *, int, int);

    *(int *)(scratch + 0x81C) = 0;
    *(int *)(scratch + 0x2C) = 0;
    prod->state = 0;
    *(float *)(scratch + 0x7FC) = time;
    *(void **)(scratch + 0x820) = target;
    *(JntProduction **)(scratch + 0x0) = prod;
    arg = *(int *)(scratch + 0x30);
    if (*(int *)(scratch + 0x810) & 0x08000000) {
        prod->result = 0;
        return 0;
    }
    if (prod->count > 0) {
        fn = prod->produce;
        param = prod->param;
        if ((unsigned int)((int)fn - 0x1F0000) <= 0x01E0FFFF && !((int)fn & 3)) {
            fn((void *)0x70000000, arg, param);
            if (target != 0) {
                *(int *)((char *)target + 0xA60) = *(int *)0x700007A0;
            }
        }
    }
    prod->result = *(int *)0x70000038;
    return 0;
}

void *JNT_getElement(void *elements, int index)
{
    return &((JntElement *)elements)[index];
}

void *JNT_nextElement(void *element)
{
    return (JntElement *)element + 1;
}

INCLUDE_ASM("asm/main/nonmatchings/jnt", JNT_defaultConsumer);

void JNT_resetHair(void)
{
    JntWork *work = (JntWork *)JNT_SCRATCH_BASE;
    unsigned short *hdr = *(unsigned short **)((char *)work + 0x28);
    JntStaticChannel *ch;
    int end;

    work->current_index = hdr[4];
    end = hdr[4] + hdr[5];
    ch = *(JntStaticChannel **)((char *)work + 0x30) + hdr[4];
    while (((JntWork *)JNT_SCRATCH_BASE)->current_index < end) {
        ch = JNT_getStaticVal2((JntWork *)JNT_SCRATCH_BASE, ch);
    }
}

static int JNT_hairID(JntChannelWork *work)
{
    JntChannelTag *tag = work->tag;
    JntHairIdSource *source = work->hair_id_source;
    int id;

    id = source->primary_id;
    if (tag->flag == 0) {
        id += source->secondary_id;
    }
    if ((tag->tag & 0xFFFF0000) == 0x008A0000) {
        id += tag->tag & 0xFFFF;
    }
    return id;
}

INCLUDE_ASM("asm/main/nonmatchings/jnt", JNT_computeHair);

int JNT_setInterrupt(JntInterruptWork *work, JntChannelList *list)
{
    int count = list->count;
    int result = 0;
    int channel;

    work->elements[0].interrupt_channel = 0;
    for (channel = 1; channel < count; channel++) {
        int element = list->channel_elements[channel];
        if (element != 0 && (element & 0x8000) == 0) {
            work->elements[element].interrupt_channel = channel;
            result++;
        }
    }

    return result;
}

JntFilterFn JNT_getFilter(void *work, JntFilterList *list)
{
    int count = list->count;
    int index = 1;

    if (index < count) {
        int remaining = count;
        JntFilterFn *filter = &list->filters[1];
        int *flag = &list->flags[1];
        for (;;) {
            if ((*flag & 0x8000) != 0) {
                return *filter;
            }
            count = remaining;
            flag++;
            index++;
            if (index >= count) {
                break;
            }
            filter++;
        }
    }
    return 0;
}

typedef struct JntHairIdSourceCounts {
    unsigned char unmodeled_00[8];
    unsigned short primary_id;                 /* 0x08 */
    unsigned short secondary_id;               /* 0x0A */
    unsigned short additional_primary_count;   /* 0x0C */
    unsigned short additional_secondary_count; /* 0x0E */
} JntHairIdSourceCounts;
typedef struct JntMatrixRows {
    float elements[4][4];
} JntMatrixRows;
typedef struct JntResetWork {
    unsigned char unmodeled_000[0x28];
    JntHairIdSourceCounts *hair_id_source; /* 0x28 */
    int current_index;                     /* 0x2C */
    unsigned char unmodeled_030[0x720 - 0x30];
    JntMatrixRows root_matrix;             /* 0x720 */
    unsigned char unmodeled_760[0x40];
    JntMatrixRows *matrix_buffer;          /* 0x7A0 */
} JntResetWork;
void MATRIX_convert4(float destination[4][4], float source[4][4]);
void xglMatrixUnit4s(float destination[4][4]);
static inline void jntResetCopyMatrix(JntMatrixRows *destination, const JntMatrixRows *source)
{
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)\n\t"
        "lq $2, 16(%1)\n\t"
        "sq $2, 16(%0)\n\t"
        "lq $2, 32(%1)\n\t"
        "sq $2, 32(%0)\n\t"
        "lq $2, 48(%1)\n\t"
        "sq $2, 48(%0)"
        : : "r"(destination), "r"(source) : "$2", "memory");
}

JntStaticChannel *JNT_resetMatrix(JntResetWork *work, JntStaticChannel *channel)
{
    JntMatrixRows base_matrix;
    JntMatrixRows result_matrix;
    JntMatrixRows source_matrix;
    JntStaticChannel *next;
    int matrix_end;
    int matrix_start;
    int has_matrix_range;

    jntResetCopyMatrix(&work->root_matrix, &work->matrix_buffer[0]);
    xglMatrixUnit4s(work->matrix_buffer[0].elements);
    next = channel;

    {
        JntHairIdSourceCounts *source = work->hair_id_source;
        matrix_start = work->current_index;
        matrix_end = source->primary_id;
        matrix_end += source->secondary_id;
        matrix_end += source->additional_primary_count;
    }
    while (matrix_start < matrix_end) {
        next = JNT_getStaticVal2((JntWork *)work, next);
        matrix_start = work->current_index;
    }

    matrix_end += work->hair_id_source->additional_secondary_count;
    has_matrix_range = matrix_start < matrix_end;
    if (has_matrix_range) {
        do {
            next = JNT_getStaticVal2((JntWork *)work, next);
        } while (work->current_index < matrix_end);
    }

    MATRIX_convert4(base_matrix.elements, work->root_matrix.elements);
    work->current_index = matrix_start;
    if (has_matrix_range) {
        do {
            MATRIX_convert4(source_matrix.elements,
                            work->matrix_buffer[work->current_index].elements);
            xglMatrixMul((void *)result_matrix.elements, (void *)base_matrix.elements, (void *)source_matrix.elements);
            jntResetCopyMatrix(&work->matrix_buffer[work->current_index], &result_matrix);
            work->current_index++;
        } while (work->current_index < matrix_end);
    }
    return next;
}

INCLUDE_ASM("asm/main/nonmatchings/jnt", JNT_computeMatrix);

/*
 * Hair-smoothing toggle at VA 0x004DC248: JNT_computeMatrix reads it with
 * `lb $2,flagSmoothHair` (VA 0x00314B14) to decide whether to apply the
 * interpolated hair matrix; this pair only sets/clears it.
 */
static signed char flagSmoothHair = 0;

void JNT_onSmoothHair(void)
{
    flagSmoothHair = 1;
}

void JNT_offSmoothHair(void)
{
    flagSmoothHair = 0;
}

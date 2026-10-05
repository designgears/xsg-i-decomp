#include "common.h"
#include "shared.h"
#include "fcv2.h"

INCLUDE_ASM("asm/main/nonmatchings/fcv2", FCV_getValue);

/*
 * FCVKeyData is the per-key-set source record FCV_resetPack reads: a packed
 * 2-bit key-type table (4 keys per byte, FCV_getFKeyType) at +0x10, 16 key
 * frames at +0x14 and the key-value stream from +0x34. FCV_getPackValue
 * (still ASM in this TU) indexes the type table by the pack's key_index and
 * walks the frame and value streams through the pack's advancing pointers.
 * key_values is declared with one element only to name where the value
 * stream starts; its length is not evidenced here.
 */
typedef struct FCVKeyData {
    unsigned char unmodeled_00[0x10];
    u8 key_types[4];
    u16 key_frames[16];
    f32 key_values[1];
} FCVKeyData;

/*
 * FCVPack is the destination FCV_resetPack initializes and the state
 * FCV_getPackValue/FCV_getPackVal2f/FCV_getPackVal2s/FCV_skipPack (still ASM
 * in this TU) consume: pointers into a FCVKeyData's three streams, plus a
 * key cursor. FCV_getPackValue passes +4 to FCV_getFKeyType as the packed
 * key-type table, reads a halfword through +8 advancing it by 2, reads a
 * float through +0xC advancing it by 4, and reads/writes +0x10 as the key
 * index; the leading word it never touches stays unmodeled.
 */
typedef struct FCVPack {
    unsigned char unmodeled_00[4];
    u8 *key_types;
    u16 *key_frames;
    f32 *key_values;
    s16 key_index;
} FCVPack;

void FCV_resetPack(FCVPack *pack, FCVKeyData *source)
{
    pack->key_types = source->key_types;
    pack->key_frames = source->key_frames;
    pack->key_values = source->key_values;
    pack->key_index = 0;
}

static f32 FCV_getPackVal2f(FCVPack *pack, s32 keyCount, f32 frame);

void FCV_skipPack(FCVPack *pack, s32 keyCount)
{
    FCV_getPackVal2f(pack, keyCount, 0.0f);
}

INCLUDE_ASM("asm/main/nonmatchings/fcv2", FCV_getPackValue);

INCLUDE_ASM("asm/main/nonmatchings/fcv2", FCV_getPackVal2f);

INCLUDE_ASM("asm/main/nonmatchings/fcv2", FCV_getPackVal2s);

/*
 * types is a packed array of 2-bit interpolation-type codes, four keys per
 * byte, stored in reverse bit order within each byte (FCV_getFKeyType,
 * VA 0x0030d060).
 */
static s32 FCV_getFKeyType(u32 keyIndex, u8 *types)
{
    unsigned char packed = types[keyIndex >> 2];
    return (packed >> ((3 - (keyIndex & 3)) * 2)) & 3;
}

INCLUDE_ASM("asm/main/nonmatchings/fcv2", FCV2_getPackVal);

INCLUDE_ASM("asm/main/nonmatchings/fcv2", FCV2_getVal);

static f32 FCV2_getVal(void *output, void *data, u16 key, f32 frame);

/*
 * FCV2Value is the curve FCV2_getValue samples: a cursor into its key table,
 * the same two-bit type code FCV_getFKeyType returns (1 = constant sample,
 * 3 = sampled spline), and either the constant itself or the base of its key
 * table, aliased in the same word (FCV2_getValue, VA 0x0030d740).
 * FCV2_getVal (VA 0x0030d428, still ASM) compares and subtracts a float
 * argument in $f12; FCV2_getValue never writes $f12 before its tail call, so
 * `frame` is its own parameter, forwarded unchanged.
 */
typedef struct FCV2Value {
    u16 cursor;
    u16 type;
    f32 value;
} FCV2Value;

f32 FCV2_getValue(FCV2Value *curve, f32 frame)
{
    if (curve->type == 3) {
        return FCV2_getVal(0, &curve->value, curve->cursor, frame);
    }
    if (curve->type == 1) {
        return curve->value;
    }
    return 0.0f;
}

/*
 * key is zeroed unconditionally, then forwarded to FCV2_getVal as its
 * output parameter only while walking a spline (curve->type == 3);
 * FCV2_getValue's equivalent call passes a null output there instead.
 */
f32 FCV2_getValueAndKey(s32 *key, FCV2Value *curve, f32 frame)
{
    *key = 0;
    if (curve->type == 3) {
        return FCV2_getVal(key, &curve->value, curve->cursor, frame);
    }
    if (curve->type == 1) {
        return curve->value;
    }
    return 0.0f;
}

typedef struct FcvAttribute {
    u32 unmodeled_0[3];
    int value_c;
    u16 key;
    int attr;
    u8 *time;
    u16 *range;
} FcvAttribute;

void FCV2_readAttribute(FcvAttribute *attribute, u32 *data)
{
    u32 pos;
    u16 *chunk;

    attribute->attr = 0;
    pos = 4;
    attribute->time = 0;
    chunk = (u16 *)(data + 1);
    attribute->range = 0;
    do {
        switch (chunk[1]) {
        case 1:
            {
                int *v = (int *)(chunk + 2);
                attribute->value_c = v[0] * 31 + v[1];
            }
            break;
        case 3:
            attribute->attr = (int)(chunk + 2);
            break;
        case 2:
            {
                u8 *p = (u8 *)(chunk + 2);
                if ((*p & 0x7F) == attribute->key) {
                    if (*p & 0x80) {
                        attribute->time = p;
                    } else {
                        attribute->range = (u16 *)p;
                    }
                }
            }
            break;
        }
        if (chunk[1] == 0) {
            return;
        }
        pos += chunk[0];
        chunk = (u16 *)((u8 *)chunk + chunk[0]);
    } while (pos < *data);
}

typedef struct FCV2DataHeader {
    u32 signature;
} FCV2DataHeader;

int FCV2_checkData(void *data)
{
    FCV2DataHeader *header;

    if (data == 0) {
        return 1;
    }

    header = data;
    if (header->signature != 0x00564346) {
        return 2;
    }
    return 0;
}

extern f32 fcv2Step;

void FCV2_setStep(f32 step)
{
    fcv2Step = __builtin_fabsf(step) * 0.5f;
}

INCLUDE_ASM("asm/main/nonmatchings/fcv2", FCV2_resetPack);

typedef struct FCV2AttrHeader {
    u16 size;
    u16 type;
} FCV2AttrHeader;

typedef struct FCV2AttrDefault {
    unsigned char unmodeled_00[0x10];
} FCV2AttrDefault;

extern FCV2AttrDefault defaultAttr;

typedef struct FCV2Pack {
    FCV2AttrHeader *cursor;
} FCV2Pack;

void *FCV2_getPackAttribute(FCV2Pack *pack)
{
    FCV2AttrHeader *attr = pack->cursor;
    FCV2AttrHeader *result;

    if (attr->type == 4) {
        result = attr;
        pack->cursor = (FCV2AttrHeader *)((u8 *)result + result->size + 4);
    } else {
        result = (FCV2AttrHeader *)&defaultAttr;
    }
    return result;
}

f32 FCV2_getPackVal(void **, unsigned char *, u16, void **);

f32 FCV2_getPackValue(void **arg0) {
    unsigned char *p = *arg0;
    f32 r = 0.0f;

    switch (*(u16 *)(p + 2) & 0xF) {
    case 0:
        *arg0 = p + 4;
        break;
    case 1:
        r = *(f32 *)(p + 4);
        *arg0 = p + 8;
        break;
    case 3:
        r = FCV2_getPackVal(arg0, p + 4, *(u16 *)p, arg0);
        break;
    default:
        *arg0 = p + 4;
        r = 0.0f;
        break;
    }
    return r;
}

extern f32 D_004D8528;

u16 *FCV2_getKey(u16 *key, f32 time)
{
    int n;
    int i;
    u16 *ret;

    ret = 0;
    if ((key[1] & 0xF) == 3) {
        n = key[0] - 1;
        key += 2;
        for (i = 0; i < n; i++) {
            ret = key;
            if (time <= key[1] * D_004D8528) {
                return ret;
            }
            switch (key[0]) {
            case 1:
                key += 8;
                break;
            case 0:
            case 2:
            case 3:
                key += 4;
                break;
            }
        }
        return key;
    }
    return ret;
}

f32 FCV2_getPackVal(void **, unsigned char *, u16, void **);

void FCV2_dump(FCV2AttrHeader *hdr)
{
    s32 n;
    s32 i;
    u16 *p;

    if (hdr->type != 3) {
        return;
    }
    n = hdr->size;
    p = (u16 *)(hdr + 1);
    i = 0;
    if (n == 0) {
        return;
    }
    do {
        switch (*p) {
        case 1:
            p += 8;
            break;
        case 0:
        case 2:
        case 3:
            p += 4;
            break;
        }
    } while (++i < n);
}

#include "common.h"
#include "shared.h"
#include "fcv2.h"

typedef struct FCVCurveKey {
    f32 t;
    f32 in;
    f32 out;
    f32 v;
} FCVCurveKey;
typedef struct FCVCurve {
    u32 interp : 4;
    u32 post : 4;
    u32 pre : 4;
    u32 : 20;
    FCVCurveKey *keys;
    u16 count;
} FCVCurve;

f32 FCV_getValue(FCVCurve *c, f32 t)
{
    FCVCurveKey *keys = c->keys;
    FCVCurveKey *p;
    f32 d, end;
    f32 o, t1, t0, v0, v1, s, inv, s2, s3, h, m, q, a;
    u32 mode;

    f32 start = keys[0].t;

    if (t < start) {
        switch (c->pre) {
        case 0:
            return keys[0].v;
        case 1:
            d = keys[c->count - 1].t - start;
            while (t < start) {
                t += d;
            }
            break;
        }
    }
    p = &keys[c->count - 1];
    end = p->t;
    if (end <= t) {
        switch (c->post) {
        case 0:
            return p->v;
        case 1:
            d = end - c->keys->t;
            while (end < t) {
                t -= d;
            }
            break;
        default:
            return 0.0f;
        }
    }
    p = keys + 1;
    while (p->t <= t) {
        if (t == p->t) {
            return p->v;
        }
        p++;
    }
    t1 = p->t;
    mode = c->interp;
    v1 = p->v;
    m = p->in;
    p--;
    t0 = p->t;
    v0 = p->v;
    o = p->out;
    switch (mode) {
    case 1:
        return v0 + (v1 - v0) * (t - t0) / (t1 - t0);
    case 0:
        return v0;
    case 2:
        s = t - t0;
        inv = 1.0f / (t1 - t0);
        s2 = s * s;
        s3 = s2 * s * (inv * inv);
        a = s2 * 3.0f * (inv * inv);
        q = s2 * inv;
        h = (s3 + s3) * inv;
        s3 = s3 - q;
        return v0 * ((h - a) + 1.0f) + v1 * (a - h) + o * ((s3 - q) + s) + m * s3;
    }
    return 0.0f;
}

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

static s32 FCV_getFKeyType(u32 keyIndex, u8 *types);
static f32 FCV_getPackVal2s(FCVPack *pack, s32 keyCount, f32 frame);

f32 FCV_getPackValue(FCVPack *pack, f32 frame)
{
    f32 ret;
    u32 type;

    type = FCV_getFKeyType(pack->key_index, pack->key_types);
        switch (type) {
        case 1:
            ret = *pack->key_values++;
            break;
        case 0:
            ret = 0.0f;
            break;
        case 3:
            ret = FCV_getPackVal2s(pack, *pack->key_frames++, frame);
            break;
        default:
            ret = 0.0f;
            break;
        }
    pack->key_index++;
    return ret;
}

static f32 FCV_getPackVal2f(FCVPack *pack, s32 keyCount, f32 frame)
{
    u32 i;
    f32 *k;
    f32 *p;
    f32 *q;
    f32 *last;
    f32 *pv;
    u32 type;
    u32 type2;
    f32 t1, t0, v0, v1, o, m, s, inv, s2, s3, a, q2, h;

    k = pack->key_values;
    type = *(u32 *)k;
    p = k;
    switch (type) {
    case 0:
    case 2:
    case 3:
        p = k + 3;
        break;
    case 4:
        p = k + 4;
        break;
    case 1:
        p = k;
        break;
    }
    i = 1;
    last = k;
    q = p;
    for (; i < (u32)keyCount; i++) {
        last = q;
        switch (*(u32 *)last) {
        case 0:
        case 2:
        case 3:
            q += 3;
            break;
        case 4:
            q += 4;
            break;
        case 1:
            q += 5;
            break;
        }
    }
    pack->key_values = q;
    if (frame < k[1]) {
        return k[2];
    }
    if (last[1] <= frame) {
        return last[2];
    }
    i = 1;
    t1 = p[1];
    while (i < (u32)keyCount) {
        if (frame <= t1) {
            if (frame == t1) {
                return p[2];
            }
            break;
        }
        k = p;
        type = *(u32 *)k;
        switch (type) {
        case 1:
            p = k + 5;
            t1 = p[1];
            break;
        case 4:
            p = k + 4;
            t1 = p[1];
            break;
        case 0:
        case 2:
            p = p + 3;
            t1 = p[1];
            break;
        }
        i++;
    }
    t0 = k[1];
    pv = p + 2;
    switch (*(u32 *)p) {
    case 0:
    case 2:
        m = 0.0f;
        v1 = pv[0];
        break;
    case 1:
    case 4:
        v1 = pv[0];
        m = pv[1];
        break;
    default:
        m = 0.0f;
        v1 = 0.0f;
        break;
    }
    pv = k + 2;
    switch (type) {
    case 0:
    case 2:
        o = 0.0f;
        v0 = pv[0];
        break;
    case 4:
        v0 = pv[0];
        o = pv[1];
        break;
    case 1:
        v0 = pv[0];
        o = pv[2];
        break;
    default:
        o = 0.0f;
        v0 = 0.0f;
        break;
    }
    switch (type) {
    case 0:
        return v0 + (v1 - v0) * (frame - t0) / (t1 - t0);
    case 1:
    case 2:
    case 4:
        s = frame - t0;
        inv = 1.0f / (t1 - t0);
        s2 = s * s;
        s3 = s2 * s * (inv * inv);
        a = s2 * 3.0f * (inv * inv);
        q2 = s2 * inv;
        h = (s3 + s3) * inv;
        s3 = s3 - q2;
        return v0 * ((h - a) + 1.0f) + v1 * (a - h) + o * ((s3 - q2) + s) + m * s3;
    }
    return 0.0f;
}

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

typedef struct FCV2Key {
    u16 type;
    u16 frame;
    f32 val;
    f32 in;
    f32 out;
} FCV2Key;
typedef union FCV2Out {
    void *p;
    f32 f;
} FCV2Out;
static inline FCV2Key *FCV2_nextKey(FCV2Key *k, s32 type)
{
    switch (type) {
    case 1:
        return (FCV2Key *)((char *)k + 0x10);
    case 0:
    case 2:
    case 3:
        return (FCV2Key *)((char *)k + 8);
    }
    return k;
}

static f32 FCV2_getVal(void *output, void *data, s32 key, f32 frame)
{
    s32 i;
    FCV2Key *prev;
    FCV2Key *p;
    s32 type;
    f32 *pv;
    f32 t, t0, t1, v0, v1, o, m, s, inv, s2, s3, a, q, h;

    prev = data;
    type = prev->type;
    switch (type) {
    case 1:
        p = (FCV2Key *)((char *)prev + 0x10);
        break;
    case 0:
    case 2:
    case 3:
        p = (FCV2Key *)((char *)data + 8);
        break;
    default:
        p = data;
        break;
    }
    if (output != 0) {
        *(void **)output = 0;
    }
    t = ((FCV2Key *)data)->frame * (1.0f / 30.0f);
    if (frame <= t) {
        if (__builtin_fabsf(frame - t) < (1.0f / 30.0f)) {
            if (output != 0) {
                *(void **)output = data;
            }
        }
        return ((FCV2Key *)data)->val;
    }
    for (i = 1; i < key; i++) {
        t = p->frame * (1.0f / 30.0f);
        if (frame <= t) {
            if (__builtin_fabsf(frame - t) < (1.0f / 30.0f)) {
                if (output != 0) {
                    ((FCV2Out *)output)->p = p;
                }
                return p->val;
            }
            break;
        }
        prev = p;
        type = prev->type;
        switch (type) {
        case 0:
        case 2:
        case 3:
            p = (FCV2Key *)((char *)p + 8);
            break;
        case 1:
            p = (FCV2Key *)((char *)prev + 0x10);
            break;
        }
    }
    if (i == key) {
        return prev->val;
    }
    t0 = prev->frame * (1.0f / 30.0f);
    t1 = p->frame * (1.0f / 30.0f);
    pv = &p->val;
    switch (p->type) {
    case 0:
    case 2:
        m = 0.0f;
        v1 = pv[0];
        break;
    case 1:
        v1 = pv[0];
        m = pv[1];
        break;
    default:
        m = 0.0f;
        v1 = m;
        break;
    }
    pv = &prev->val;
    switch (type) {
    case 0:
    case 2:
    case 3:
        o = 0.0f;
        v0 = pv[0];
        break;
    case 1:
        v0 = pv[0];
        o = pv[2];
        break;
    default:
        o = 0.0f;
        v0 = o;
        break;
    }
    switch (type) {
    case 3:
        return v0;
    case 0:
        return v0 + (v1 - v0) * (frame - t0) / (t1 - t0);
    case 1:
    case 2:
        s = frame - t0;
        inv = 1.0f / (t1 - t0);
        s2 = s * s;
        s3 = s2 * s * (inv * inv);
        a = s2 * 3.0f * (inv * inv);
        q = s2 * inv;
        h = (s3 + s3) * inv;
        s3 = s3 - q;
        return v0 * ((h - a) + 1.0f) + v1 * (a - h) + o * ((s3 - q) + s) + m * s3;
    }
    return 0.0f;
}

static f32 FCV2_getVal(void *output, void *data, s32 key, f32 frame);

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

extern f32 D_004D8518;
extern f32 D_004D851C;
extern f32 D_004D8520;
extern f32 D_004D8524;
typedef struct FCV2Curve {
    u32 signature;
    u16 type;
    u16 count;
    u16 start;
    u16 end;
    s16 scale;
    u16 pad;
    u32 data[1];
} FCV2Curve;
typedef struct FCV2PackState {
    u32 cursor;
    f32 a;
    f32 b;
    int value_c;
    u8 pad[0x10];
    f32 step;
} FCV2PackState;

void FCV2_resetPack(FcvAttribute *pack, FCV2Curve *curve)
{
    f32 a;
    f32 b;
    u32 *data;
    s16 type;
    u32 res;

    if (curve != 0) {
        if (curve->type >= 2) {
            b = curve->scale * D_004D851C;
            a = curve->count * D_004D8518 * D_004D851C;
        } else {
            b = 0.0f;
            a = (curve->end - curve->start) * D_004D8520 * D_004D8524;
        }
        type = curve->type;
        pack->value_c = 0;
        if ((u16)type >= 2) {
            data = curve->data;
            FCV2_readAttribute(pack, data);
            res = (u32)data + *data;
        } else {
            res = (u32)curve->data;
        }
        ((FCV2PackState *)pack)->cursor = res;
        ((FCV2PackState *)pack)->a = a;
        ((FCV2PackState *)pack)->b = b;
        ((FCV2PackState *)pack)->step = fcv2Step;
    }
}

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

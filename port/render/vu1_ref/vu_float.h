/*
 * VU float arithmetic for the C references, lane by lane, with the same rules
 * as the interpreter (port/runtime/vu1.cpp):
 *   - inputs with exponent 255 read as +-FLT_MAX, denormals as +-0;
 *   - results clamp to +-FLT_MAX, denormal results flush to +-0, NaN -> +MAX;
 *   - MADD/MSUB round the product first, then add to ACC (two roundings);
 *   - the caller runs with MXCSR round-toward-zero + FTZ + DAZ
 *     (vu_ref_fpu_enter/leave), and the TU is built with -msse2 -mfpmath=sse.
 * Values are kept as raw u32 bit patterns so results can be compared bit-exact.
 */
#ifndef XSG_VU_FLOAT_H
#define XSG_VU_FLOAT_H

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <xmmintrin.h>

typedef uint32_t vu_f;   /* a VU float register lane, raw bits */

static inline float vu_in(vu_f b) {
    float f;
    if ((b & 0x7f800000u) == 0x7f800000u) b = (b & 0x80000000u) | 0x7f7fffffu;
    else if ((b & 0x7f800000u) == 0) b &= 0x80000000u;
    memcpy(&f, &b, 4);
    return f;
}

static inline vu_f vu_out(float f) {
    vu_f b;
    memcpy(&b, &f, 4);
    if ((b & 0x7f800000u) == 0x7f800000u) {
        if ((b & 0x7fffffu) && !(b & 0x80000000u)) b = 0x7f7fffffu;
        else b = (b & 0x80000000u) | 0x7f7fffffu;
    } else if ((b & 0x7f800000u) == 0 && (b & 0x7fffffu)) {
        b &= 0x80000000u;
    }
    return b;
}

static inline vu_f vu_add(vu_f a, vu_f b) { return vu_out(vu_in(a) + vu_in(b)); }
static inline vu_f vu_sub(vu_f a, vu_f b) { return vu_out(vu_in(a) - vu_in(b)); }
static inline vu_f vu_mul(vu_f a, vu_f b) { return vu_out(vu_in(a) * vu_in(b)); }
/* acc + round(a*b) */
static inline vu_f vu_madd(vu_f acc, vu_f a, vu_f b) { return vu_out(vu_in(acc) + vu_in(vu_out(vu_in(a) * vu_in(b)))); }
static inline vu_f vu_max(vu_f a, vu_f b) { float x = vu_in(a), y = vu_in(b); return vu_out(x > y ? x : y); }
static inline vu_f vu_min(vu_f a, vu_f b) { float x = vu_in(a), y = vu_in(b); return vu_out(x < y ? x : y); }

static inline vu_f vu_itof(int32_t i, int shift) {
    static const float sc[16] = {1.0f, 0, 0, 0, 1.0f / 16, 0, 0, 0, 0, 0, 0, 0, 1.0f / 4096, 0, 0, 1.0f / 32768};
    return vu_out((float)i * sc[shift]);
}
static inline int32_t vu_ftoi(vu_f a, int shift) {
    double v = (double)vu_in(a) * (double)(1u << shift);
    if (v >= 2147483647.0) return 0x7fffffff;
    if (v <= -2147483648.0) return (int32_t)0x80000000u;
    return (int32_t)v;
}

/* DIV q, a / b, as the interpreter computes it. */
static inline vu_f vu_div(vu_f a, vu_f b) {
    float x = vu_in(a), y = vu_in(b);
    if (y == 0.0f) return ((a ^ b) & 0x80000000u) | 0x7f7fffffu;
    return vu_out(x / y);
}

/* CLIP judgement bits of one vertex: +x -x +y -y +z -z in bits 0..5. */
static inline uint32_t vu_clip(const vu_f v[4]) {
    float lim = fabsf(vu_in(v[3]));
    uint32_t j = 0;
    for (int l = 0; l < 3; ++l) {
        float x = vu_in(v[l]);
        if (x > lim) j |= 1u << (2 * l);
        if (x < -lim) j |= 2u << (2 * l);
    }
    return j;
}

static inline unsigned vu_ref_fpu_enter(void) {
    unsigned csr = _mm_getcsr();
    _mm_setcsr((csr & ~0x6000u) | 0x6000u | 0x8040u);
    return csr;
}
static inline void vu_ref_fpu_leave(unsigned csr) { _mm_setcsr(csr); }

#endif

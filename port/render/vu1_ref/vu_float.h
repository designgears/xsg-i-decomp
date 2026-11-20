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

/* Branchless (selects), so the per-lane loops of the references vectorise. */
static inline float vu_in(vu_f b) {
    const vu_f e = b & 0x7f800000u, s = b & 0x80000000u;
    b = e == 0x7f800000u ? (s | 0x7f7fffffu) : e == 0 ? s : b;
    float f;
    memcpy(&f, &b, 4);
    return f;
}

static inline vu_f vu_out(float f) {
    vu_f b;
    memcpy(&b, &f, 4);
    const vu_f e = b & 0x7f800000u, s = b & 0x80000000u;
    /* exponent 255: +MAX for a positive NaN, else sign | MAX: both are s | MAX */
    return e == 0x7f800000u ? (s | 0x7f7fffffu) : (e == 0 && (b & 0x7fffffu)) ? s : b;
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

/* The same rules on four lanes (SSE2), for the hot loops: lane for lane the
   results are the scalar functions' (same operations, same MXCSR). */
#include <emmintrin.h>
typedef __m128i vu_v4;   /* four lanes, raw bits */
static inline __m128i vu_sel4(__m128i m, __m128i a, __m128i b) { return _mm_or_si128(_mm_and_si128(m, a), _mm_andnot_si128(m, b)); }
static inline __m128 vu_in4(vu_v4 b) {
    const __m128i em = _mm_set1_epi32(0x7f800000), sm = _mm_set1_epi32((int)0x80000000u);
    const __m128i e = _mm_and_si128(b, em), s = _mm_and_si128(b, sm);
    b = vu_sel4(_mm_cmpeq_epi32(e, em), _mm_or_si128(s, _mm_set1_epi32(0x7f7fffff)), b);
    b = vu_sel4(_mm_cmpeq_epi32(e, _mm_setzero_si128()), s, b);
    return _mm_castsi128_ps(b);
}
static inline vu_v4 vu_out4(__m128 f) {
    const __m128i em = _mm_set1_epi32(0x7f800000), sm = _mm_set1_epi32((int)0x80000000u), mm = _mm_set1_epi32(0x7fffff);
    __m128i b = _mm_castps_si128(f);
    const __m128i e = _mm_and_si128(b, em), s = _mm_and_si128(b, sm);
    /* exponent 255: +MAX for a positive NaN, else sign | MAX (the same value) */
    const __m128i ismax = _mm_cmpeq_epi32(e, em);
    const __m128i isden = _mm_andnot_si128(_mm_cmpeq_epi32(_mm_and_si128(b, mm), _mm_setzero_si128()),
                                           _mm_cmpeq_epi32(e, _mm_setzero_si128()));
    b = vu_sel4(ismax, _mm_or_si128(s, _mm_set1_epi32(0x7f7fffff)), b);
    return vu_sel4(isden, s, b);
}
static inline vu_v4 vu_ld4(const void* p) { return _mm_loadu_si128((const __m128i*)p); }
static inline void vu_st4(void* p, vu_v4 v) { _mm_storeu_si128((__m128i*)p, v); }
static inline vu_v4 vu_bc4(vu_f a) { return _mm_set1_epi32((int)a); }
static inline vu_v4 vu_add4(vu_v4 a, vu_v4 b) { return vu_out4(_mm_add_ps(vu_in4(a), vu_in4(b))); }
static inline vu_v4 vu_sub4(vu_v4 a, vu_v4 b) { return vu_out4(_mm_sub_ps(vu_in4(a), vu_in4(b))); }
static inline vu_v4 vu_mul4(vu_v4 a, vu_v4 b) { return vu_out4(_mm_mul_ps(vu_in4(a), vu_in4(b))); }
static inline vu_v4 vu_madd4(vu_v4 acc, vu_v4 a, vu_v4 b) { return vu_out4(_mm_add_ps(vu_in4(acc), vu_in4(vu_mul4(a, b)))); }
static inline vu_v4 vu_min4(vu_v4 a, vu_v4 b) { return vu_out4(_mm_min_ps(vu_in4(a), vu_in4(b))); }
static inline vu_v4 vu_max4(vu_v4 a, vu_v4 b) { return vu_out4(_mm_max_ps(vu_in4(a), vu_in4(b))); }
/* vu_ftoi on four lanes: truncate, saturate to +0x7fffffff / 0x80000000 */
static inline vu_v4 vu_ftoi4(vu_v4 a, int shift) {
    const __m128 x = _mm_mul_ps(vu_in4(a), _mm_set1_ps((float)(1u << shift)));   /* exact (a power of two) */
    const __m128i t = _mm_cvttps_epi32(x);
    return vu_sel4(_mm_castps_si128(_mm_cmpge_ps(x, _mm_set1_ps(2147483648.0f))), _mm_set1_epi32(0x7fffffff), t);
}

static inline unsigned vu_ref_fpu_enter(void) {
    unsigned csr = _mm_getcsr();
    _mm_setcsr((csr & ~0x6000u) | 0x6000u | 0x8040u);
    return csr;
}
static inline void vu_ref_fpu_leave(unsigned csr) { _mm_setcsr(csr); }

#endif

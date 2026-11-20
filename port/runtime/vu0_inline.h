/* VU0 macro arithmetic, SSE, for the per-op variants in vu0.c.
 *
 * The game's translated COP2 blocks call vu0_arith with constant operands.
 * The generic vu0_arith decoded the op, mask and broadcast per call and ran
 * lane by lane (~11% of the game thread in battle effects).  vu0.h folds the
 * op and the vector/broadcast choice at each call site into a direct call to
 * xsg_vu0_<op>_v / _b, built from vu0_arith_inline below with the op fixed.
 * Same results, bit for bit, as the generic vu0_arith (tests/test_vu0_inline.c):
 * inputs with exponent 255 read as +-FLT_MAX and denormals as +-0, results
 * clamp to +-FLT_MAX and flush denormals, MAC/status as set_mac.  Only vu0.c
 * includes this (SSE); the game files are built without SSE. */
#ifndef XSG_VU0_INLINE_H
#define XSG_VU0_INLINE_H

#include <emmintrin.h>

#define XSG_VU0_AI static inline __attribute__((always_inline))

XSG_VU0_AI __m128i xsg_vu0_sel(__m128i m, __m128i a, __m128i b)   /* m ? a : b */
{
    return _mm_or_si128(_mm_and_si128(m, a), _mm_andnot_si128(m, b));
}

/* in_f on four lanes */
XSG_VU0_AI __m128 xsg_vu0_in(__m128i b)
{
    const __m128i expm = _mm_set1_epi32(0x7f800000), sign = _mm_set1_epi32((int)0x80000000u);
    const __m128i e = _mm_and_si128(b, expm), s = _mm_and_si128(b, sign);
    b = xsg_vu0_sel(_mm_cmpeq_epi32(e, expm), _mm_or_si128(s, _mm_set1_epi32(0x7f7fffff)), b);
    b = xsg_vu0_sel(_mm_cmpeq_epi32(e, _mm_setzero_si128()), s, b);
    return _mm_castsi128_ps(b);
}

/* out_f on four lanes; *ov, *un, *zr, *sg: per-lane MAC conditions (lane l at bit l) */
XSG_VU0_AI __m128i xsg_vu0_out(__m128 f, int *ov, int *un, int *zr, int *sg)
{
    const __m128i expm = _mm_set1_epi32(0x7f800000), sign = _mm_set1_epi32((int)0x80000000u);
    const __m128i mant = _mm_set1_epi32(0x007fffff);
    __m128i b = _mm_castps_si128(f);
    const __m128i e = _mm_and_si128(b, expm), s = _mm_and_si128(b, sign);
    const __m128i is_max = _mm_cmpeq_epi32(e, expm);
    const __m128i is_den = _mm_andnot_si128(_mm_cmpeq_epi32(_mm_and_si128(b, mant), _mm_setzero_si128()),
                                            _mm_cmpeq_epi32(e, _mm_setzero_si128()));
    /* exponent 255: +MAX for a positive NaN, else sign | MAX: both are sign | MAX */
    b = xsg_vu0_sel(is_max, _mm_or_si128(s, _mm_set1_epi32(0x7f7fffff)), b);
    b = xsg_vu0_sel(is_den, s, b);
    if (ov) {
        *ov = _mm_movemask_ps(_mm_castsi128_ps(is_max));
        *un = _mm_movemask_ps(_mm_castsi128_ps(is_den));
        *zr = _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(_mm_andnot_si128(sign, b), _mm_setzero_si128())));
        *sg = _mm_movemask_ps(_mm_castsi128_ps(b));
    }
    return b;
}

/* lane mask (lane l at bit l) -> PS2 order (x at bit 3) */
XSG_VU0_AI unsigned xsg_vu0_rev4(unsigned m)
{
    return ((m & 1) << 3) | ((m & 2) << 1) | ((m & 4) >> 1) | ((m & 8) >> 3);
}

XSG_VU0_AI void vu0_arith_inline(int op, int dest, int fd, int fs, int ft, int bc)
{
    XsgVu0 *const v = &xsg_vu0;
    const __m128 x = xsg_vu0_in(_mm_loadu_si128((const __m128i *)v->vf[fs].u));
    __m128 y;
    if (bc == VU0_VEC)
        y = xsg_vu0_in(_mm_loadu_si128((const __m128i *)v->vf[ft].u));
    else
        y = xsg_vu0_in(_mm_set1_epi32((int)(bc == VU0_BQ ? v->Q : bc == VU0_BI ? v->I : v->vf[ft].u[bc])));
    __m128 z;
    switch (op) {
    case VU0_ADD: z = _mm_add_ps(x, y); break;
    case VU0_SUB: z = _mm_sub_ps(x, y); break;
    case VU0_MUL: z = _mm_mul_ps(x, y); break;
    case VU0_MADD:
    case VU0_MSUB: {
        const __m128 m = _mm_castsi128_ps(xsg_vu0_out(_mm_mul_ps(x, y), 0, 0, 0, 0));
        const __m128 acc = xsg_vu0_in(_mm_loadu_si128((const __m128i *)v->acc.u));
        z = op == VU0_MADD ? _mm_add_ps(acc, m) : _mm_sub_ps(acc, m);
        break;
    }
    case VU0_MAX: z = _mm_max_ps(x, y); break;
    default: z = _mm_min_ps(x, y); break;
    }
    const int flags = op != VU0_MAX && op != VU0_MINI;
    int ov = 0, un = 0, zr = 0, sg = 0;
    const __m128i r = xsg_vu0_out(z, flags ? &ov : 0, &un, &zr, &sg);
    /* dest bit 3 = x = lane 0 */
    const unsigned lanes = xsg_vu0_rev4((unsigned)dest & 15);
    XsgVf *const d = fd == VU0_ACC ? &v->acc : &v->vf[fd];
    const __m128i keep = _mm_set_epi32((lanes & 8) ? 0 : -1, (lanes & 4) ? 0 : -1, (lanes & 2) ? 0 : -1,
                                       (lanes & 1) ? 0 : -1);
    _mm_storeu_si128((__m128i *)d->u, xsg_vu0_sel(keep, _mm_loadu_si128((const __m128i *)d->u), r));
    if (flags) {
        const unsigned mac = xsg_vu0_rev4((unsigned)zr & lanes) | xsg_vu0_rev4((unsigned)sg & lanes) << 4 |
                             xsg_vu0_rev4((unsigned)un & lanes) << 8 | xsg_vu0_rev4((unsigned)ov & lanes) << 12;
        unsigned st = 0;
        if (mac & 0x000f) st |= 1;
        if (mac & 0x00f0) st |= 2;
        if (mac & 0x0f00) st |= 4;
        if (mac & 0xf000) st |= 8;
        v->mac = mac;
        v->status = (v->status & 0xff0u) | st | (st << 6);
    }
    /* fix_vf0 */
    v->vf[0].u[0] = v->vf[0].u[1] = v->vf[0].u[2] = 0;
    v->vf[0].u[3] = 0x3f800000u;
}

#endif

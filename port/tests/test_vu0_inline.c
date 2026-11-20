/* The per-op SSE vu0_arith variants (vu0_inline.h) against the generic
 * vu0_arith, bit for bit: every op, dest mask, broadcast, ACC destination,
 * with random and special inputs (+-0, denormals, +-inf, NaNs, +-MAX).
 *   cc -m32 -O2 -msse2 -mfpmath=sse -DXSG_VU0_NO_INLINE -Iport/runtime \
 *      port/tests/test_vu0_inline.c port/runtime/vu0.c <stubs> */
#define XSG_VU0_NO_INLINE
#include "vu0.h"

#include <stdio.h>
#include <string.h>

void xsg_vu0_add_v(int, int, int, int);
void xsg_vu0_add_b(int, int, int, int, int);
#define D(n) void xsg_vu0_##n##_v(int, int, int, int); void xsg_vu0_##n##_b(int, int, int, int, int);
D(sub) D(mul) D(madd) D(msub) D(max) D(mini)
#undef D
typedef void (*FnV)(int, int, int, int);
typedef void (*FnB)(int, int, int, int, int);
static const FnV fv[7] = {xsg_vu0_add_v, xsg_vu0_sub_v, xsg_vu0_mul_v, xsg_vu0_madd_v, xsg_vu0_msub_v, xsg_vu0_max_v, xsg_vu0_mini_v};
static const FnB fb[7] = {xsg_vu0_add_b, xsg_vu0_sub_b, xsg_vu0_mul_b, xsg_vu0_madd_b, xsg_vu0_msub_b, xsg_vu0_max_b, xsg_vu0_mini_b};

static unsigned rs = 12345;
static unsigned rnd(void) { rs = rs * 1103515245u + 12345u; return (rs >> 8) ^ (rs << 13); }
static unsigned val(void)
{
    static const unsigned sp[] = {0, 0x80000000u, 1, 0x80000001u, 0x007fffffu, 0x7f800000u, 0xff800000u,
                                  0x7fc00000u, 0xffc00001u, 0x7f7fffffu, 0xff7fffffu, 0x3f800000u, 0x00800000u,
                                  0x7f000000u, 0x0c000000u, 0x8c000000u};
    switch (rnd() & 3) {
    case 0: return sp[rnd() % 16];
    case 1: { /* near the over/underflow edges */
        unsigned e = (rnd() & 1) ? 0xf0 + (rnd() & 15) : (rnd() & 15);
        return (rnd() & 0x80000000u) | e << 23 | (rnd() & 0x7fffff); }
    default: return rnd();
    }
}

int main(void)
{
    long bad = 0, n = 0;
    for (int it = 0; it < 400000; ++it) {
        XsgVu0 init;
        for (int r = 0; r < 32; ++r)
            for (int l = 0; l < 4; ++l) init.vf[r].u[l] = val();
        init.vf[0].u[0] = init.vf[0].u[1] = init.vf[0].u[2] = 0; init.vf[0].u[3] = 0x3f800000u;
        for (int l = 0; l < 4; ++l) init.acc.u[l] = val();
        for (int i = 0; i < 16; ++i) init.vi[i] = rnd() & 0xffff;
        init.I = val(); init.Q = val(); init.P = val(); init.R = val();
        init.status = rnd() & 0xfff; init.mac = rnd() & 0xffff; init.clip = rnd(); init.cmsar0 = 0; init.fbrst = 0;
        const int op = rnd() % 7, dest = 1 + rnd() % 15, fs = rnd() % 32, ft = rnd() % 32;
        const int fd = (rnd() % 5 == 0) ? VU0_ACC : (int)(rnd() % 32);
        const int bc = (int)(rnd() % 7) - 1;   /* -1 vec, 0..3 lane, 4 Q, 5 I */
        XsgVu0 ref;
        xsg_vu0 = init;
        vu0_arith(op, dest, fd, fs, ft, bc);
        ref = xsg_vu0;
        xsg_vu0 = init;
        if (bc == VU0_VEC) fv[op](dest, fd, fs, ft); else fb[op](dest, fd, fs, ft, bc);
        ++n;
        if (memcmp(&ref, &xsg_vu0, sizeof ref)) {
            if (bad++ < 10)
                printf("mismatch: op %d dest %x fd %d fs %d ft %d bc %d  mac %04x/%04x status %03x/%03x\n", op, dest, fd,
                       fs, ft, bc, ref.mac, xsg_vu0.mac, ref.status, xsg_vu0.status);
        }
    }
    printf("%ld cases, %ld mismatches\n", n, bad);
    return bad != 0;
}

/*
 * The VU0 microprograms the game calls, as C (no interpreter).
 *
 * VU0's micro memory holds 1792 bytes (port/data/vu0_microcode.c); the game
 * only ever runs two entries, both from VCALLMS:
 *   0x0020  sin: vf1.x = sin(vf4.x)
 *   0x00e8  cos: vf4.x += pi/2, then the sin code
 * (`vu_disasm.py --micro`).  This is that code, op by op, with the
 * interpreter's float rules (vu_float.h: clamps, two-rounding MADD, run under
 * round-toward-zero + FTZ + DAZ), and everything it leaves behind: vf1-vf4 x,
 * ACC x, I, vi1/vi2/vi15, MAC and status (sticky bits included).  Checked
 * against the interpreter by [debug] vu0_ref_check = 1 (vu0.c).
 */
#include "vu_float.h"
#include "../../runtime/vu0.h"

#define V xsg_vu0

/* MAC bits of an x-only result (x is bit 3 of each group) and the clamp. */
static inline vu_f out_x(float f, unsigned *mac)
{
    vu_f b;
    memcpy(&b, &f, 4);
    const unsigned e = b & 0x7f800000u;
    *mac = 0;
    if (e == 0x7f800000u) *mac |= 1u << 15;
    else if (e == 0 && (b & 0x7fffffu)) *mac |= 1u << 11;
    b = vu_out(f);
    if (!(b & 0x7fffffffu)) *mac |= 1u << 3;
    if (b & 0x80000000u) *mac |= 1u << 7;
    return b;
}

/* One flag-writing FMAC op: its MAC, and status as MacWrite builds it. */
static inline void flags(unsigned mac, unsigned *status, unsigned *last_mac)
{
    unsigned st = 0;
    if (mac & 0x000f) st |= 1;
    if (mac & 0x00f0) st |= 2;
    if (mac & 0x0f00) st |= 4;
    if (mac & 0xf000) st |= 8;
    *status = (*status & 0xff0u) | st | (st << 6);
    *last_mac = mac;
}

#define F(x) ({ float f_; vu_f b_ = (x); memcpy(&f_, &b_, 4); f_; })

/* 1 when the entry was run, 0 when it has no C version. */
int vu0_ref_callms(unsigned byte_addr)
{
    if (byte_addr != 0x20 && byte_addr != 0xe8) return 0;
    const unsigned csr = vu_ref_fpu_enter();
    unsigned st = V.status, mac = V.mac, m;
    unsigned st_hist[24];   /* status after each flag write, for FSAND's latency */
    int nh = 0;
    vu_f I = V.I;
    vu_f f4 = V.vf[4].u[0];
    if (byte_addr == 0xe8) {
        /* 00e8 iaddiu vi15, vi0, 0 ; 00f0 loi pi/2 ; 00f8 addi.x vf4, vf4, i ; b 0x28 (delay: nop) */
        I = 0x3fc90fdbu;
        f4 = out_x(vu_in(f4) + vu_in(I), &m); flags(m, &st, &mac); st_hist[nh++] = st;
        V.vf[4].u[0] = f4;
    }
    /* 0020 iaddiu vi15, vi0, 0 */
    vu_f f1, f2, f3, acc;
    /* 0028 add.x vf3, vf4, vf0 ; loi 0.5 */
    f3 = out_x(vu_in(f4) + 0.0f, &m); flags(m, &st, &mac); st_hist[nh++] = st;
    I = 0x3f000000u;
    /* 0030 addai.x acc, vf0, i ; loi 1/pi */
    acc = out_x(0.0f + vu_in(I), &m); flags(m, &st, &mac); st_hist[nh++] = st;
    I = 0x3ea2f983u;
    /* 0038 maddi.x vf1, vf4, i ; loi -0.5 */
    f1 = out_x(vu_in(acc) + vu_in(vu_out(vu_in(f4) * vu_in(I))), &m); flags(m, &st, &mac); st_hist[nh++] = st;
    I = 0xbf000000u;
    /* 0040 addai.x acc, vf0, i ; loi 1/pi */
    acc = out_x(0.0f + vu_in(I), &m); flags(m, &st, &mac); st_hist[nh++] = st;
    I = 0x3ea2f983u;
    /* 0048 maddi.x vf2, vf4, i ; fsand vi1, 0x2: the sign of 0028's add (flag latency 4) */
    const unsigned fs = st_hist[nh - 4];
    f2 = out_x(vu_in(acc) + vu_in(vu_out(vu_in(f4) * vu_in(I))), &m); flags(m, &st, &mac); st_hist[nh++] = st;
    unsigned vi1 = fs & 2;
    /* 0050 adda.x acc, vf4, vf0 ; ibeq vi1, vi0, 0x68 ; 0058 ftoi0.x vf3, vf1 (delay slot) ; 0060 ftoi0.x vf3, vf2 */
    acc = out_x(vu_in(f4) + 0.0f, &m); flags(m, &st, &mac); st_hist[nh++] = st;
    f3 = (vu_f)vu_ftoi(f1, 0);
    if (vi1 != 0) f3 = (vu_f)vu_ftoi(f2, 0);
    /* 0068 itof0.x vf1, vf3 ; loi pi */
    f1 = vu_itof((int32_t)f3, 0);
    I = 0x40490fdbu;
    /* 0070 msubi.x vf1, vf1, i ; mtir vi1, vf3x */
    f1 = out_x(vu_in(acc) - vu_in(vu_out(vu_in(f1) * vu_in(I))), &m); flags(m, &st, &mac);
    vi1 = f3 & 0xffff;
    /* 0078 mul.x vf3, vf1, vf1 ; iaddiu vi2, vi0, 1 */
    f3 = out_x(vu_in(f1) * vu_in(f1), &m); flags(m, &st, &mac);
    const unsigned vi2 = 1;
    /* 0080 mul.x vf2, vf1, vf3 ; iand vi1, vi1, vi2 */
    f2 = out_x(vu_in(f1) * vu_in(f3), &m); flags(m, &st, &mac);
    vi1 &= vi2;
    /* 0088 mulaw.x acc, vf1, vf0w */
    acc = out_x(vu_in(f1) * 1.0f, &m); flags(m, &st, &mac);
    /* 0090 mul.x vf1, vf2, vf3 ; loi -1/6 */
    f1 = out_x(vu_in(f2) * vu_in(f3), &m); flags(m, &st, &mac);
    I = 0xbe2aaaa4u;
    /* 0098 maddai.x acc, vf2, i */
    acc = out_x(vu_in(acc) + vu_in(vu_out(vu_in(f2) * vu_in(I))), &m); flags(m, &st, &mac);
    /* 00a0 mul.x vf2, vf1, vf3 ; loi 1/120 */
    f2 = out_x(vu_in(f1) * vu_in(f3), &m); flags(m, &st, &mac);
    I = 0x3c08873eu;
    /* 00a8 maddai.x acc, vf1, i */
    acc = out_x(vu_in(acc) + vu_in(vu_out(vu_in(f1) * vu_in(I))), &m); flags(m, &st, &mac);
    /* 00b0 mul.x vf1, vf2, vf3 ; loi -1/5040 */
    f1 = out_x(vu_in(f2) * vu_in(f3), &m); flags(m, &st, &mac);
    I = 0xb94fb21fu;
    /* 00b8 maddai.x acc, vf2, i ; loi 1/362880 */
    acc = out_x(vu_in(acc) + vu_in(vu_out(vu_in(f2) * vu_in(I))), &m); flags(m, &st, &mac);
    I = 0x362e9bf5u;
    /* 00c0 maddi.x vf1, vf1, i ; ibeq vi1, vi0, 0xd8 (delay: nop) ; 00d0 sub.x vf1, vf0, vf1 */
    f1 = out_x(vu_in(acc) + vu_in(vu_out(vu_in(f1) * vu_in(I))), &m); flags(m, &st, &mac);
    if (vi1 != 0) { f1 = out_x(0.0f - vu_in(f1), &m); flags(m, &st, &mac); }
    /* 00d8 jr vi15 (= 0) ; 0000 nop[E] ; 0008 nop: done */
    V.vf[1].u[0] = f1;
    V.vf[2].u[0] = f2;
    V.vf[3].u[0] = f3;
    V.acc.u[0] = acc;
    V.I = I;
    V.vi[1] = vi1;
    V.vi[2] = vi2;
    V.vi[15] = 0;
    V.mac = mac;
    V.status = st;
    vu_ref_fpu_leave(csr);
    return 1;
}

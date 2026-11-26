/*
 * Set 0, lit loops (material_flags & 0x10 clear; s_aUcodeAdr[2..5]).  These are set 2's
 * rigid_lit group (vu1_set2_rigid.c) without the bone step: P and N are already in object
 * space, M962 is object -> clip and 973-975 normal -> light.  See VU1_PROGRAMS.md.
 *   entry 0x1418 -> loop 0x1458 "lit"      lit strip, fog from 1007 (as the strip loop)
 *   entry 0x1f60 -> loop 0x1fa0 "lit_env"  + specular added, env-map second strip (tag 1010),
 *                                          six kicks.  Fog comes from REGISTER vf9 (whatever
 *                                          an earlier loop left there; the ilw of 998.w is dead)
 * Face/Back stubs (995.y = 0x4000, 1013-1016 facing words) 0x1648/0x16a0 -> lit and
 * 0x23a8/0x2400 -> lit_env.  0x16a0 is s_aUcodeBackAdr[2..5]: every lit slot's Back entry
 * runs the plain lit loop.
 *
 * Batch header 0x05e8 (MSCNT resume after the 0x05d8 stop) is set 2's 0x0188 with streams
 * P +1, N +1+n (w = T), C +1+2n (float), Nf +1+3n (facing normal), morph keys +1+6n.
 * Facing subroutine 0x0af0 (when 995.y != 0): mem[1013 + 2*(dot(976 - P, Nf) < 0)].w.
 *   entry 0x16f8 -> loop 0x1738 "lit_spec" set 2's rigid_lit_spec on object space (Face/Back
 *                                          0x1a58/0x1ab0).  0x1b08 (two-pass) is not referenced.
 */
#include "vu1_set2_int.h"

#include <math.h>
#include <stdio.h>

void set0_init_0050(Vu1Ref* r);   /* vu1_set0.c */

static const vu_f HALF = 0x3f000000u;

/* 0x05e8.  Returns 0 for a register batch (already kicked). */
static int header_05e8(Vu1Ref* r) {
    const uint32_t top = r->top;
    uint32_t hy = vu1_ref_ilw(r, top, 1), hz = vu1_ref_ilw(r, top, 2);
    uint32_t w967 = vu1_ref_ilw(r, 967, 3);
    VI(13) = top;
    if (hy == 0x4000) {
        Q4 a = ld(r, top + 1), b = ld(r, top + 2), c = ld(r, top + 3), d = ld(r, top + 4);
        memcpy(VF(7), vu1_ref_qw(r, 971), 16);
        if (hz == 0x4000) {
            Q4 c971 = ld(r, 971), x8 = ld(r, top + 8), x7 = ld(r, top + 7), x5 = ld(r, top + 5), x6 = ld(r, top + 6);
            st(r, 1008, x8.v, 8);
            st(r, 970, x7.v, 14);
            for (int l = 0; l < 3; ++l) VF(25)[l] = vu_mul(x5.v[l], c971.v[l]);
            VF(25)[3] = vu_mul(VF(25)[3], c971.v[3]);
            memcpy(VF(24), x6.v, 16);
        }
        uint32_t o = top + 0xf3;
        st(r, o, a.v, 15);
        st(r, o + 1, b.v, 15);
        st(r, 985, b.v, 15);
        st(r, 983, c.v, 15);
        if (w967 == 0) st(r, o + 2, c.v, 15);
        else { st(r, o + 2, d.v, 15); st(r, 983, d.v, 15); }
        vu1_ref_xgkick(r, o);
        return 0;
    }
    vu1_ref_isw(r, 1017, 1, top);
    uint32_t out = (top + 0xf3) & 0xffff;
    vu1_ref_isw(r, 1017, 0, out);
    uint32_t hx = vu1_ref_ilw(r, top, 0), n = hx & 0x7fff;
    uint32_t p = (top + 1) & 0xffff, nm = (p + n) & 0xffff, c = (nm + n) & 0xffff, x = (c + n) & 0xffff;
    st(r, out, ld(r, 1011).v, 15);
    vu1_ref_isw(r, out, 0, hx);
    Q4 c0 = ld(r, c);
    for (uint32_t i = 0; i < hz; ++i) st(r, c + i, c0.v, 15);
    uint32_t keys = vu1_ref_ilw(r, top, 3);
    if (keys) {
        vu_f wgt = vu_min(((vu_f*)vu1_ref_qw(r, 734))[3], vu_itof((int16_t)keys, 0));
        int32_t idx = vu_ftoi(wgt, 0);
        vu_f frac = vu_sub(wgt, vu_itof(idx, 0));
        vu_f inv = vu_sub(ONE, frac);
        uint32_t src = (x + 3 * n + (uint32_t)(idx & 0xffff)) & 0xffff, stride = keys + 1;
        uint32_t dst = p, cnt = 2 * n;
        if (vu1_ref_ilw(r, 1008, 3) == 2) { dst = c; cnt = n; }
        for (uint32_t i = 0; i < cnt; ++i) {
            Q4 a = ld(r, src), bb = ld(r, src + 1);
            vu_f o[4];
            for (int l = 0; l < 4; ++l) o[l] = vu_madd(vu_mul(a.v[l], inv), bb.v[l], frac);
            st(r, dst + i, o, 15);
            src = (src + stride) & 0xffff;
        }
    }
    memcpy(VF(24), vu1_ref_qw(r, 969), 16);
    VI(2) = p; VI(3) = nm; VI(4) = c; VI(10) = x;
    VI(8) = (out + 1) & 0xffff; VI(9) = n; VI(5) = 0; VI(14) = hx;
    return 1;
}

/* 0x0af0: facing word of vertex i (0 when 995.y == 0). */
static uint32_t facing_0af0(Vu1Ref* r, uint32_t i, const Q4* P) {
    if (!vu1_ref_ilw(r, 995, 1)) return 0;
    Q4 E = ld(r, 976), Nf = ld(r, VI(10) + i);
    vu_f d[3];
    for (int l = 0; l < 3; ++l) d[l] = vu_mul(vu_sub(E.v[l], P->v[l]), Nf.v[l]);
    vu_f s = vu_add(d[0], d[2]);
    s = vu_add(s, d[1]);
    return vu1_ref_ilw(r, 1013 + ((s & 0x80000000u) ? 2 : 0), 3);
}

/* fog = clamp(f.z + f.w q, f.x, f.y), as the strip loop (mulz.w / mula.w / maddq.w). */
static vu_f fog_of(const vu_f f[4], vu_f q) {
    vu_f acc = vu_mul(vu_mul(ONE, f[2]), ONE);
    vu_f g = vu_madd(acc, f[3], q);
    g = vu_max(g, f[0]);
    return vu_min(g, f[1]);
}

static vu_f xyzf_w(vu_f fog, uint32_t vi5) {
    uint32_t v = ((uint32_t)vu_ftoi(fog, 4) & 0xff0) | vi5;
    return (vu_f)(int32_t)(int16_t)(v & 0xffff);
}

/* Loop 0x1458. */
static void lit_loop(Vu1Ref* r) {
    uint32_t n = VI(9), o = VI(8), vi5 = VI(5);
    vu_f uv[4], fog[4];
    memcpy(uv, vu1_ref_qw(r, 972), 16);
    memcpy(fog, vu1_ref_qw(r, 1007), 16);
    memcpy(VF(9), fog, 16);
    if (n == 0) n = 0x10000;
    for (uint32_t i = 0; i < n; ++i, o += 3) {
        Q4 P = ld(r, VI(2) + i), N = ld(r, VI(3) + i), C = ld(r, VI(4) + i);
        vu_f s4[3];
        s4[2] = ONE;
        s4[1] = vu_add(0, N.v[3]);
        s4[0] = vu_add(0, P.v[3]);
        s4[0] = vu_add(s4[0], uv[0]);
        s4[1] = vu_add(s4[1], uv[1]);
        vu_f L[3], clip[4], col[4];
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(VF(17)[l], N.v[0]);
            acc = vu_madd(acc, VF(18)[l], N.v[1]);
            L[l] = vu_madd(acc, VF(19)[l], N.v[2]);
        }
        for (int l = 0; l < 4; ++l) {
            vu_f acc = vu_mul(VF(28)[l], P.v[0]);
            acc = vu_madd(acc, VF(29)[l], P.v[1]);
            acc = vu_madd(acc, VF(30)[l], P.v[2]);
            clip[l] = vu_madd(acc, VF(31)[l], ONE);
        }
        uint32_t face = facing_0af0(r, i, &P);
        vi5 |= face;
        for (int l = 0; l < 3; ++l) L[l] = vu_max(L[l], 0);
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(VF(20)[l], L[0]);
            acc = vu_madd(acc, VF(21)[l], L[1]);
            col[l] = vu_madd(acc, VF(22)[l], L[2]);
        }
        uint32_t cj = vu_clip(clip);
        vu_f q = vu_div(ONE, clip[3]);
        r->regs.Q = q;
        uint32_t clipw = vu1_ref_ilw(r, 999 + (cj ? 1 : 0), 3);
        for (int l = 0; l < 3; ++l) col[l] = vu_madd(vu_mul(VF(23)[l], ONE), col[l], VF(25)[l]);
        col[3] = VF(25)[3];
        vu_f fg = fog_of(fog, q);
        vu_f scr[3], stq[3];
        for (int l = 0; l < 3; ++l) scr[l] = vu_mul(clip[l], q);
        for (int l = 0; l < 3; ++l) stq[l] = vu_mul(s4[l], q);
        for (int l = 0; l < 4; ++l) col[l] = vu_mul(col[l], C.v[l]);
        for (int l = 0; l < 3; ++l) scr[l] = vu_madd(vu_mul(VF(27)[l], ONE), scr[l], VF(26)[l]);
        vi5 = ((vi5 | clipw) << 1) & 0xffff;
        for (int l = 0; l < 3; ++l) col[l] = vu_min(col[l], F255);
        col[3] = vu_min(col[3], F128);
        vu_f xyzf[4];
        int32_t ci[4];
        for (int l = 0; l < 3; ++l) xyzf[l] = (vu_f)vu_ftoi(scr[l], 4);
        xyzf[3] = xyzf_w(fg, vi5);
        for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(col[l], 0);
        st(r, o, stq, 14);
        st(r, o + 1, (vu_f*)ci, 15);
        st(r, o + 2, xyzf, 15);
    }
    VI(5) = vi5;
    VI(9) = 0;
    r->regs.I = F128;
    vu1_ref_xgkick(r, vu1_ref_ilw(r, 1017, 0));
}

/* Loop 0x1fa0: rigid_lit_spec_6k (vu1_set2_rigid.c mode 3) on object-space P/N. */
static void env_loop(Vu1Ref* r) {
    uint32_t n = VI(9), o = VI(8), vi5 = VI(5), vi12 = 0x8000;
    if (n == 0) n = 0x10000;
    uint32_t top = vu1_ref_ilw(r, 1017, 1);
    uint32_t o2 = (o + 3 * n) & 0xffff;
    st(r, o2, ld(r, 1010).v, 15);
    vu1_ref_isw(r, o2, 0, vu1_ref_ilw(r, top, 0));
    o2++;
    vu_f fog[4];
    memcpy(fog, VF(9), 16);
    for (uint32_t i = 0; i < n; ++i, o += 3, o2 += 3) {
        Q4 P = ld(r, VI(2) + i), N = ld(r, VI(3) + i), C = ld(r, VI(4) + i);
        vi5 |= facing_0af0(r, i, &P);
        vu_f nrm[3] = {N.v[0], N.v[1], N.v[2]};
        if (vu1_ref_ilw(r, 1008, 2)) {
            float x = vu_in(nrm[0]), y = vu_in(nrm[1]), z = vu_in(nrm[2]);
            vu_f p = vu_out(1.0f / sqrtf(x * x + y * y + z * z));
            for (int l = 0; l < 3; ++l) nrm[l] = vu_mul(nrm[l], p);
        }
        for (int k = 0; k < 3; ++k) VF(17 + k)[3] = vu_sub(VF(17 + k)[3], P.v[k]);
        vu_f e[3], v5[4], L[3], clip[4], col[4];
        Q4 E = ld(r, 976);
        for (int l = 0; l < 3; ++l) e[l] = vu_sub(E.v[l], P.v[l]);
        for (int l = 0; l < 4; ++l) {
            vu_f acc = vu_mul(VF(17)[l], nrm[0]);
            acc = vu_madd(acc, VF(18)[l], nrm[1]);
            v5[l] = vu_madd(acc, VF(19)[l], nrm[2]);
        }
        vu_f p;
        {
            float x = vu_in(e[0]), y = vu_in(e[1]), z = vu_in(e[2]);
            p = vu_out(1.0f / sqrtf(x * x + y * y + z * z));
        }
        for (int l = 0; l < 4; ++l) {
            vu_f acc = vu_mul(VF(28)[l], P.v[0]);
            acc = vu_madd(acc, VF(29)[l], P.v[1]);
            acc = vu_madd(acc, VF(30)[l], P.v[2]);
            clip[l] = vu_madd(acc, VF(31)[l], ONE);
        }
        st(r, o2 + 1, ld(r, 1009).v, 15);
        for (int l = 0; l < 3; ++l) L[l] = vu_max(v5[l], 0);
        vu_f s4[3];
        s4[1] = vu_add(0, N.v[3]);
        s4[0] = vu_add(0, P.v[3]);
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(VF(20)[l], L[0]);
            acc = vu_madd(acc, VF(21)[l], L[1]);
            col[l] = vu_madd(acc, VF(22)[l], L[2]);
        }
        uint32_t cj = vu_clip(clip);
        vu_f q = vu_div(ONE, clip[3]);
        r->regs.Q = q;
        vu_f uv[4];
        memcpy(uv, vu1_ref_qw(r, 972), 16);
        vu_f n5[3];
        for (int l = 0; l < 3; ++l) n5[l] = vu_mul(nrm[l], v5[3]);
        uint32_t clipw = vu1_ref_ilw(r, 999 + (cj ? 1 : 0), 3);
        s4[2] = ONE;
        for (int l = 0; l < 3; ++l) col[l] = vu_madd(vu_mul(VF(23)[l], ONE), col[l], VF(25)[l]);
        col[3] = VF(25)[3];
        s4[0] = vu_add(s4[0], uv[0]);
        s4[1] = vu_add(s4[1], uv[1]);
        vu_f scr[4];
        for (int l = 0; l < 4; ++l) scr[l] = vu_mul(clip[l], q);
        vi5 = ((vi5 | clipw) << 1) & 0xffff;
        for (int l = 0; l < 4; ++l) col[l] = vu_mul(col[l], C.v[l]);
        vu_f stq[3];
        for (int l = 0; l < 3; ++l) stq[l] = vu_mul(s4[l], q);
        for (int l = 0; l < 3; ++l) scr[l] = vu_madd(vu_mul(VF(27)[l], ONE), scr[l], VF(26)[l]);
        for (int l = 0; l < 3; ++l) col[l] = vu_min(col[l], F255);
        col[3] = vu_min(col[3], F128);
        st(r, o, stq, 14);
        st(r, o2, stq, 14);
        vu_f R[3];
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(n5[l], p);
            acc = vu_add(acc, vu_mul(n5[l], p));
            R[l] = vu_sub(acc, vu_mul(e[l], p));
        }
        vi12 &= vi5;
        vu_f v6[3];
        vu_f y = ((vu_f*)vu1_ref_qw(r, 1008))[1];
        memcpy(v6, vu1_ref_qw(r, 970), 12);
        int32_t xi[3];
        for (int l = 0; l < 3; ++l) xi[l] = vu_ftoi(scr[l], 4);
        vu_f e8w = vu_mul(ONE, fog[2]);
        vu_f fg = vu_madd(vu_mul(e8w, ONE), fog[3], q);
        vu_f Sraw[3], e8[4], S[3], sp[3];
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(VF(17)[l], R[0]);
            acc = vu_madd(acc, VF(18)[l], R[1]);
            Sraw[l] = vu_madd(acc, VF(19)[l], R[2]);
        }
        Q4 a = ld(r, 1004), b = ld(r, 1005), c = ld(r, 1006);
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(a.v[l], Sraw[0]);
            acc = vu_madd(acc, b.v[l], Sraw[1]);
            e8[l] = vu_madd(acc, c.v[l], Sraw[2]);
        }
        e8[3] = e8w;
        for (int k = 0; k < 3; ++k) memcpy(VF(17 + k), vu1_ref_qw(r, 973 + k), 16);
        for (int l = 0; l < 3; ++l) S[l] = vu_max(Sraw[l], 0);
        fg = vu_max(fg, fog[0]);
        for (int l = 0; l < 3; ++l) v6[l] = vu_mul(v6[l], y);
        for (int l = 0; l < 2; ++l) e8[l] = vu_add(e8[l], ONE);
        uint32_t cnt = vu1_ref_ilw(r, 1008, 0);
        for (;;) {
            for (int l = 0; l < 3; ++l) S[l] = vu_mul(S[l], S[l]);
            if (!cnt) break;
            cnt = (cnt - 1) & 0xffff;
        }
        for (int l = 0; l < 2; ++l) e8[l] = vu_mul(e8[l], HALF);
        fg = vu_min(fg, fog[1]);
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(VF(20)[l], S[0]);
            acc = vu_madd(acc, VF(21)[l], S[1]);
            sp[l] = vu_madd(acc, VF(22)[l], S[2]);
        }
        e8[2] = ONE;
        e8[0] = vu_mul(e8[0], VF(24)[1]);
        e8[1] = vu_mul(e8[1], VF(24)[3]);
        for (int l = 0; l < 3; ++l) sp[l] = vu_mul(sp[l], v6[l]);
        e8[0] = vu_add(e8[0], VF(24)[0]);
        e8[1] = vu_add(e8[1], VF(24)[2]);
        for (int l = 0; l < 3; ++l) sp[l] = vu_mul(sp[l], F128);
        for (int l = 0; l < 3; ++l) e8[l] = vu_mul(e8[l], q);
        for (int l = 0; l < 3; ++l) col[l] = vu_min(vu_add(col[l], sp[l]), F255);
        memcpy(VF(8), e8, 16);
        st(r, o2, e8, 15);
        vu_f xyzf[4];
        int32_t ci[4];
        for (int l = 0; l < 3; ++l) xyzf[l] = (vu_f)xi[l];
        xyzf[3] = xyzf_w(fg, vi5);
        st(r, o2 + 2, xyzf, 15);
        st(r, o + 2, xyzf, 15);
        for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(col[l], 0);
        st(r, o + 1, (vu_f*)ci, 15);
    }
    VI(5) = vi5;
    VI(9) = 0;
    r->regs.I = F255;
    if (vi12) { vu1_ref_xgkick(r, vu1_ref_ilw(r, 999, 0)); return; }
    vu1_ref_xgkick(r, vu1_ref_ilw(r, 1017, 0));
    const uint32_t ex[5] = {0x3fa, 0x3da, o & 0xffff, 0x3d5, 0x3fd};
    for (int k = 0; k < 5; ++k) vu1_ref_xgkick(r, ex[k]);
}

/* Loop 0x1738 (entry 0x16f8; Face/Back 0x1a58/0x1ab0): rigid_lit_spec (vu1_set2_rigid.c
   mode 1) on object-space P/N, fog from 1007 as the lit loop.  The loader trucks and other
   shiny props of the Woglinde's cargo bay (#3). */
static void spec_loop(Vu1Ref* r) {
    uint32_t n = VI(9), o = VI(8), vi5 = VI(5), vi12 = 0x8000;
    if (n == 0) n = 0x10000;
    vu_f fog[4];
    memcpy(fog, vu1_ref_qw(r, 1007), 16);
    memcpy(VF(9), fog, 16);
    vu_f I = r->regs.I;
    for (uint32_t i = 0; i < n; ++i, o += 3) {
        Q4 P = ld(r, VI(2) + i), N = ld(r, VI(3) + i), C = ld(r, VI(4) + i);
        vi5 |= facing_0af0(r, i, &P);
        vu_f nrm[3] = {N.v[0], N.v[1], N.v[2]};
        if (vu1_ref_ilw(r, 1008, 2)) {
            float x = vu_in(nrm[0]), y = vu_in(nrm[1]), z = vu_in(nrm[2]);
            vu_f p = vu_out(1.0f / sqrtf(x * x + y * y + z * z));
            for (int l = 0; l < 3; ++l) nrm[l] = vu_mul(nrm[l], p);
        }
        for (int k = 0; k < 3; ++k) VF(17 + k)[3] = vu_sub(VF(17 + k)[3], P.v[k]);
        vu_f e[3], v5[4], L[3], clip[4], col[4];
        Q4 E = ld(r, 976);
        for (int l = 0; l < 3; ++l) e[l] = vu_sub(E.v[l], P.v[l]);
        for (int l = 0; l < 4; ++l) {
            vu_f acc = vu_mul(VF(17)[l], nrm[0]);
            acc = vu_madd(acc, VF(18)[l], nrm[1]);
            v5[l] = vu_madd(acc, VF(19)[l], nrm[2]);
        }
        vu_f p;
        {
            float x = vu_in(e[0]), y = vu_in(e[1]), z = vu_in(e[2]);
            p = vu_out(1.0f / sqrtf(x * x + y * y + z * z));
        }
        for (int k = 0; k < 3; ++k) memcpy(VF(17 + k), vu1_ref_qw(r, 973 + k), 16);
        for (int l = 0; l < 3; ++l) L[l] = vu_max(v5[l], 0);
        for (int l = 0; l < 4; ++l) {
            vu_f acc = vu_mul(VF(28)[l], P.v[0]);
            acc = vu_madd(acc, VF(29)[l], P.v[1]);
            acc = vu_madd(acc, VF(30)[l], P.v[2]);
            clip[l] = vu_madd(acc, VF(31)[l], ONE);
        }
        vu_f s4[3], uv[4];
        memcpy(uv, vu1_ref_qw(r, 972), 16);
        s4[1] = vu_add(0, N.v[3]);
        s4[0] = vu_add(0, P.v[3]);
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(VF(20)[l], L[0]);
            acc = vu_madd(acc, VF(21)[l], L[1]);
            col[l] = vu_madd(acc, VF(22)[l], L[2]);
        }
        uint32_t cj = vu_clip(clip);
        vu_f q = vu_div(ONE, clip[3]);
        r->regs.Q = q;
        s4[2] = ONE;
        s4[0] = vu_add(s4[0], uv[0]);
        s4[1] = vu_add(s4[1], uv[1]);
        uint32_t clipw = vu1_ref_ilw(r, 999 + (cj ? 1 : 0), 3);
        for (int l = 0; l < 3; ++l) col[l] = vu_madd(vu_mul(VF(23)[l], ONE), col[l], VF(25)[l]);
        col[3] = VF(25)[3];
        vu_f stq[3], scr[3];
        for (int l = 0; l < 3; ++l) stq[l] = vu_mul(s4[l], q);
        for (int l = 0; l < 3; ++l) scr[l] = vu_mul(clip[l], q);
        VF(8)[3] = vu_mul(ONE, fog[2]);
        vi5 = ((vi5 | clipw) << 1) & 0xffff;
        for (int l = 0; l < 4; ++l) col[l] = vu_mul(col[l], C.v[l]);
        for (int l = 0; l < 3; ++l) scr[l] = vu_madd(vu_mul(VF(27)[l], ONE), scr[l], VF(26)[l]);
        vu_f n5[3];
        for (int l = 0; l < 3; ++l) n5[l] = vu_mul(nrm[l], v5[3]);
        for (int l = 0; l < 3; ++l) col[l] = vu_min(col[l], I);
        col[3] = vu_min(col[3], F128);
        st(r, o, stq, 14);
        vu_f R[3];
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(n5[l], p);
            acc = vu_add(acc, vu_mul(n5[l], p));
            R[l] = vu_sub(acc, vu_mul(e[l], p));
        }
        vu_f fg = fog_of(fog, q);
        vu_f S[3], sp[3], v6[3];
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(VF(17)[l], R[0]);
            acc = vu_madd(acc, VF(18)[l], R[1]);
            S[l] = vu_max(vu_madd(acc, VF(19)[l], R[2]), 0);
        }
        vu_f y = ((vu_f*)vu1_ref_qw(r, 1008))[1];
        for (int l = 0; l < 3; ++l) v6[l] = vu_mul(((vu_f*)vu1_ref_qw(r, 970))[l], y);
        uint32_t cnt = vu1_ref_ilw(r, 1008, 0);
        for (;;) {
            for (int l = 0; l < 3; ++l) S[l] = vu_mul(S[l], S[l]);
            if (!cnt) break;
            cnt = (cnt - 1) & 0xffff;
        }
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(VF(20)[l], S[0]);
            acc = vu_madd(acc, VF(21)[l], S[1]);
            sp[l] = vu_madd(acc, VF(22)[l], S[2]);
        }
        for (int l = 0; l < 3; ++l) sp[l] = vu_mul(sp[l], v6[l]);
        vi12 &= vi5;
        I = F255;
        for (int l = 0; l < 3; ++l) col[l] = vu_min(vu_add(vu_mul(sp[l], C.v[l]), col[l]), F255);
        vu_f xyzf[4];
        int32_t ci[4];
        for (int l = 0; l < 3; ++l) xyzf[l] = (vu_f)vu_ftoi(scr[l], 4);
        xyzf[3] = xyzf_w(fg, vi5);
        st(r, o + 2, xyzf, 15);
        for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(col[l], 0);
        st(r, o + 1, (vu_f*)ci, 15);
    }
    VI(5) = vi5;
    VI(9) = 0;
    r->regs.I = F255;
    vu1_ref_xgkick(r, vu1_ref_ilw(r, vi12 ? 999 : 1017, 0));
}

int set0_lit(Vu1Ref* r, const char** variant) {
    static const struct { uint32_t pc, loop; int face; const char* name; } ent[] = {
        {0x1418, 0x1458, 0, "set0_entry_lit"},
        {0x1648, 0x1458, 1, "set0_entry_face_lit"},
        {0x16a0, 0x1458, 2, "set0_entry_back_lit"},
        {0x1f60, 0x1fa0, 0, "set0_entry_lit_env"},
        {0x23a8, 0x1fa0, 1, "set0_entry_face_lit_env"},
        {0x2400, 0x1fa0, 2, "set0_entry_back_lit_env"},
        {0x16f8, 0x1738, 0, "set0_entry_lit_spec"},
        {0x1a58, 0x1738, 1, "set0_entry_face_lit_spec"},
        {0x1ab0, 0x1738, 2, "set0_entry_back_lit_spec"},
    };
    for (unsigned k = 0; k < sizeof ent / sizeof ent[0]; ++k) {
        if (r->pc != ent[k].pc) continue;
        if (variant) *variant = ent[k].name;
        set0_init_0050(r);
        if (ent[k].face) {
            uint32_t a = ent[k].face == 1 ? 0x4000 : 0, b = 0x4000 - a;
            vu1_ref_isw(r, 1013, 3, a); vu1_ref_isw(r, 1016, 3, a);
            vu1_ref_isw(r, 1015, 3, b); vu1_ref_isw(r, 1014, 3, b);
            vu1_ref_isw(r, 995, 1, 0x4000);
            VI(2) = 0x4000;
        }
        VI(14) = 0xbb; VI(15) = ent[k].loop / 8;
        return VU1REF_OK;
    }
    if (r->pc == 0x05e8) {
        uint32_t loop = VI(15) * 8u;
        if (loop != 0x1458 && loop != 0x1fa0 && loop != 0x1738) return VU1REF_UNSUPPORTED;
        if (variant) *variant = loop == 0x1458 ? "set0_lit" : loop == 0x1738 ? "set0_lit_spec" : "set0_lit_env";
        if (!header_05e8(r)) return VU1REF_OK;
        if (loop == 0x1458) lit_loop(r); else if (loop == 0x1738) spec_loop(r); else env_loop(r);
        return VU1REF_OK;
    }
    return VU1REF_UNSUPPORTED;
}

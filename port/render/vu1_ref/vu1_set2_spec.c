/*
 * Set 2, skinned specular loops (s_aUcodeEnvAdr slots 1..3 of the first group).
 *
 *   entry 0x08e0 -> loop 0x0920 "skin_lit_spec"        one pass, specular added to the colour
 *   entry 0x0d60 -> loop 0x0da0 "skin_lit_spec_2pass"  second strip (tag 1012) = specular colour
 *   entry 0x1228 -> loop 0x1268 "skin_lit_spec6"       second strip (tag 1010) = env-map STQ,
 *                                                     constant colour 1009; six kicks
 * Face/Back stubs (995.y = 1 / 2): 0x0cf0/0x0d28, 0x11b8/0x11f0, 0x1720/0x1758.
 *
 * Shared per vertex (as skin_lit, 0x05f0): skin, facing, diffuse, clip, STQ, XYZF2.
 * Specular, per vertex:
 *   v  = eye(976) - pos            p = 1/|v|                  (ERLENG)
 *   d  = (973.w-pos.x, 974.w-pos.y, 975.w-pos.z) . nrm
 *   R  = 2 (nrm d) p - v p                                       (reflection, as MADD/MSUB)
 *   S  = max(M973 R, 0)  squared 1008.x + 1 times
 *   sp = (M977 S) * 970 * 1008.y
 */
#include "vu1_set2_int.h"

typedef struct {
    vu_f stq[3];
    vu_f xyzf[4];
    vu_f col[4];       /* diffuse * C, xyz clamped to I, w clamped to 128 (float) */
    vu_f C[4];
    vu_f R[3];         /* reflection vector */
    vu_f q;
} SpecV;

static vu_f erleng(const vu_f v[3]) {
    float x = vu_in(v[0]), y = vu_in(v[1]), z = vu_in(v[2]);
    return vu_out(1.0f / sqrtf(x * x + y * y + z * z));
}

/* Everything up to the specular power; vi5 is the ADC shift register. */
static void spec_vertex(Vu1Ref* r, uint32_t i, uint32_t* vi5, vu_f Ixyz, SpecV* o) {
    Q4 P = ld(r, VI(2) + i), N = ld(r, VI(3) + i), C = ld(r, VI(4) + i);
    Q4 B = ld(r, VI(6) + i), W = ld(r, VI(7) + i);
    vu_f pos[4], nrm[4];
    set2_skin(r, &P, &N, &B, &W, pos, nrm);
    uint32_t face = set2_facing_0440(r, pos);
    Q4 m973 = ld(r, 973), m974 = ld(r, 974), m975 = ld(r, 975), eye = ld(r, 976), uv = ld(r, 972);
    vu_f v[3], ew[3];
    for (int l = 0; l < 3; ++l) v[l] = vu_sub(eye.v[l], pos[l]);
    ew[0] = vu_sub(m973.v[3], pos[0]);
    ew[1] = vu_sub(m974.v[3], pos[1]);
    ew[2] = vu_sub(m975.v[3], pos[2]);
    vu_f d = vu_madd(vu_madd(vu_mul(ew[0], nrm[0]), ew[1], nrm[1]), ew[2], nrm[2]);
    vu_f p = erleng(v);
    vu_f L[3], clip[4], col[4];
    for (int l = 0; l < 3; ++l)
        L[l] = vu_max(vu_madd(vu_madd(vu_mul(m973.v[l], nrm[0]), m974.v[l], nrm[1]), m975.v[l], nrm[2]), 0);
    for (int l = 0; l < 4; ++l)
        clip[l] = vu_madd(vu_madd(vu_madd(vu_mul(VF(28)[l], pos[0]), VF(29)[l], pos[1]), VF(30)[l], pos[2]), VF(31)[l], ONE);
    vu_f s4[3];
    s4[1] = vu_add(0, N.v[3]);
    s4[0] = vu_add(0, P.v[3]);
    s4[2] = vu_add(0, ONE);
    s4[0] = vu_add(s4[0], uv.v[0]);
    s4[1] = vu_add(s4[1], uv.v[1]);
    for (int l = 0; l < 3; ++l) col[l] = vu_madd(vu_madd(vu_mul(VF(20)[l], L[0]), VF(21)[l], L[1]), VF(22)[l], L[2]);
    col[3] = VF(25)[3];
    vu_f q = vu_div(ONE, clip[3]);
    uint32_t cj = vu_clip(clip);
    for (int l = 0; l < 3; ++l) col[l] = vu_madd(vu_mul(VF(23)[l], ONE), col[l], VF(25)[l]);
    uint32_t clipw = vu1_ref_ilw(r, 999 + (cj ? 1 : 0), 3);
    vu_f scr[3];
    for (int l = 0; l < 3; ++l) scr[l] = vu_mul(clip[l], q);
    for (int l = 0; l < 3; ++l) o->stq[l] = vu_mul(s4[l], q);
    for (int l = 0; l < 4; ++l) col[l] = vu_mul(col[l], C.v[l]);
    for (int l = 0; l < 3; ++l) scr[l] = vu_madd(vu_mul(VF(27)[l], ONE), scr[l], VF(26)[l]);
    *vi5 = ((*vi5 | face | clipw) << 1) & 0xffff;
    for (int l = 0; l < 3; ++l) o->col[l] = vu_min(col[l], Ixyz);
    o->col[3] = vu_min(col[3], F128);
    for (int l = 0; l < 3; ++l) o->xyzf[l] = (vu_f)vu_ftoi(scr[l], 4);
    o->xyzf[3] = (vu_f)(int32_t)(int16_t)((vu1_ref_ilw(r, 998, 3) | *vi5) & 0xffff);
    vu_f n5[3];
    for (int l = 0; l < 3; ++l) n5[l] = vu_mul(nrm[l], d);
    for (int l = 0; l < 3; ++l) o->R[l] = vu_sub(vu_madd(vu_mul(n5[l], p), n5[l], p), vu_mul(v[l], p));
    memcpy(o->C, C.v, 16);
    o->q = q;
}

/* S = M973 R (unclamped) */
static void m3(Vu1Ref* r, uint32_t q0, const vu_f in[3], vu_f out[3]) {
    Q4 a = ld(r, q0), b = ld(r, q0 + 1), c = ld(r, q0 + 2);
    for (int l = 0; l < 3; ++l) out[l] = vu_madd(vu_madd(vu_mul(a.v[l], in[0]), b.v[l], in[1]), c.v[l], in[2]);
}

/* max(S,0)^(2^(k+1)) through M977, times 970 * 1008.y */
static void spec_power(Vu1Ref* r, vu_f S[3], vu_f sp[3]) {
    for (int l = 0; l < 3; ++l) S[l] = vu_max(S[l], 0);
    uint32_t k = vu1_ref_ilw(r, 1008, 0);
    for (uint32_t j = 0;; j = (j + 1) & 0xffff) {
        for (int l = 0; l < 3; ++l) S[l] = vu_mul(S[l], S[l]);
        if (j == k) break;
    }
    for (int l = 0; l < 3; ++l) sp[l] = vu_madd(vu_madd(vu_mul(VF(20)[l], S[0]), VF(21)[l], S[1]), VF(22)[l], S[2]);
}

static void store_ci(Vu1Ref* r, uint32_t q, const vu_f c[4]) {
    int32_t ci[4];
    for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(c[l], 0);
    st(r, q, (vu_f*)ci, 15);
}

/* kind 0: 0x0920, 1: 0x0da0, 2: 0x1268 */
static void spec_loop(Vu1Ref* r, int kind) {
    uint32_t n = VI(9), o = VI(8), vi5 = VI(5), vi12 = 0x8000;
    if (n == 0) n = 0x10000;
    uint32_t o2 = 0;
    if (kind) {
        uint32_t t = (o + 3 * n) & 0xffff;
        uint32_t hx = vu1_ref_ilw(r, vu1_ref_ilw(r, 1017, 1), 0);
        st(r, t, ld(r, kind == 1 ? 1012 : 1010).v, 15);
        vu1_ref_isw(r, t, 0, hx);
        o2 = t + 1;
    }
    vu_f I = kind ? 0x437f0000u : r->regs.I;
    Q4 c970 = ld(r, 970), c1008 = ld(r, 1008);
    for (uint32_t i = 0; i < n; ++i) {
        SpecV v;
        spec_vertex(r, i, &vi5, kind ? F255 : I, &v);
        vi12 &= vi5;
        I = F255;
        vu_f vf6[3];
        for (int l = 0; l < 3; ++l) vf6[l] = vu_mul(c970.v[l], c1008.v[1]);
        vu_f S[3], sp[3];
        if (kind == 0) {
            m3(r, 973, v.R, S);
            spec_power(r, S, sp);
            for (int l = 0; l < 3; ++l) sp[l] = vu_mul(sp[l], vf6[l]);
            for (int l = 0; l < 3; ++l) v.col[l] = vu_min(vu_add(vu_mul(sp[l], v.C[l]), v.col[l]), F255);
            st(r, o, v.stq, 14);
            store_ci(r, o + 1, v.col);
            st(r, o + 2, v.xyzf, 15);
        } else if (kind == 1) {
            st(r, o, v.stq, 14);
            st(r, o2, v.stq, 14);
            store_ci(r, o + 1, v.col);
            st(r, o + 2, v.xyzf, 15);
            m3(r, 973, v.R, S);
            vu_f c1[4];
            for (int l = 0; l < 3; ++l) c1[l] = vu_mul(v.C[l], vf6[l]);
            c1[3] = vu_mul(v.C[3], ONE);
            st(r, o2 + 2, v.xyzf, 15);
            spec_power(r, S, sp);
            vu_f c2[4];
            for (int l = 0; l < 3; ++l) c2[l] = vu_min(vu_mul(sp[l], c1[l]), F255);
            c2[3] = vu_min(vu_mul(ONE, c1[3]), F128);
            store_ci(r, o2 + 1, c2);
        } else {
            st(r, o2 + 1, ld(r, 1009).v, 15);
            st(r, o, v.stq, 14);
            st(r, o2, v.stq, 14);
            m3(r, 973, v.R, S);
            st(r, o + 2, v.xyzf, 15);
            st(r, o2 + 2, v.xyzf, 15);
            vu_f e[4];
            e[3] = VF(8)[3];   /* sq.xyzw vf8: w is whatever vf8 held (976.w from the entry) */
            m3(r, 1004, S, e);
            e[0] = vu_add(e[0], ONE);
            e[1] = vu_add(e[1], ONE);
            spec_power(r, S, sp);
            e[0] = vu_mul(e[0], 0x3f000000u);
            e[1] = vu_mul(e[1], 0x3f000000u);
            e[2] = ONE;
            e[0] = vu_mul(e[0], VF(24)[1]);
            e[1] = vu_mul(e[1], VF(24)[3]);
            for (int l = 0; l < 3; ++l) sp[l] = vu_mul(sp[l], vf6[l]);
            e[0] = vu_add(e[0], VF(24)[0]);
            e[1] = vu_add(e[1], VF(24)[2]);
            for (int l = 0; l < 3; ++l) sp[l] = vu_mul(sp[l], F128);
            for (int l = 0; l < 3; ++l) e[l] = vu_mul(e[l], v.q);
            for (int l = 0; l < 3; ++l) v.col[l] = vu_min(vu_add(v.col[l], sp[l]), F255);
            st(r, o2, e, 15);
            store_ci(r, o + 1, v.col);
        }
        o += 3;
        o2 += 3;
    }
    r->regs.I = F255;
    VI(5) = vi5;
    VI(9) = 0;
    if (vi12) { vu1_ref_xgkick(r, vu1_ref_ilw(r, 999, 0)); return; }
    vu1_ref_xgkick(r, vu1_ref_ilw(r, 1017, 0));
    if (kind == 1) vu1_ref_xgkick(r, o);
    if (kind == 2) {
        vu1_ref_xgkick(r, 0x3fa);
        vu1_ref_xgkick(r, 0x3da);
        vu1_ref_xgkick(r, o);
        vu1_ref_xgkick(r, 0x3d5);
        vu1_ref_xgkick(r, 0x3fd);
    }
}

static const struct {
    uint32_t entry, loop, face, back;
    const char *ename, *lname;
} progs[] = {
    {0x08e0, 0x0920, 0x0cf0, 0x0d28, "set2_entry_skin_lit_spec", "set2_skin_lit_spec"},
    {0x0d60, 0x0da0, 0x11b8, 0x11f0, "set2_entry_skin_lit_spec_2pass", "set2_skin_lit_spec_2pass"},
    {0x1228, 0x1268, 0x1720, 0x1758, "set2_entry_skin_lit_spec6", "set2_skin_lit_spec6"},
};

int set2_spec(Vu1Ref* r, const char** variant) {
    for (int k = 0; k < 3; ++k) {
        if (r->pc == progs[k].entry || r->pc == progs[k].face || r->pc == progs[k].back) {
            if (variant) *variant = progs[k].ename;
            set2_init_0050(r);
            if (r->pc != progs[k].entry) {
                VI(1) = r->pc == progs[k].face ? 1 : 2;
                vu1_ref_isw(r, 995, 1, VI(1));
            }
            VI(14) = 0x2f;
            VI(15) = progs[k].loop / 8;
            return VU1REF_OK;
        }
        if (r->pc == 0x0188 && VI(15) * 8u == progs[k].loop) {
            if (variant) *variant = progs[k].lname;
            if (!set2_header_0188(r)) return VU1REF_OK;
            spec_loop(r, k);
            return VU1REF_OK;
        }
    }
    return VU1REF_UNSUPPORTED;
}

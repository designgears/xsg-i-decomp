/*
 * Set 2, third quarter of s_aUcodeEnvAdr (render_flags & 4): the "blend 8"
 * group.  Same constants, header (0x0188) and facing subroutine (0x0440) as
 * skin_lit; the bone stream B (TOP+1+4n) holds two bones per vertex:
 *   B.x = bone 0 qw address, B.y = bone 1 qw address, B.z / B.w = weights.
 *
 *   entry 0x2850 -> loop 0x2890  blend_lit        two bones always, lit, one pass
 *   entry 0x2be8 -> loop 0x2c28  blend_lit_spec   bone 1 only if B.y != 0; + specular
 *                                                 added to the colour (x C)
 *   entry 0x3058 -> loop 0x3098  blend_lit_spec_2pass  pass 1 lit, pass 2 = specular
 *                                                 strip, GIF tag 1012 (CTXT 2), 2 kicks
 *   entry 0x3510 -> loop 0x3550  blend_lit_spec_env    specular added (x128, not x C),
 *                                                 pass 2 = env-mapped strip (tag 1010,
 *                                                 colour = qw 1009, ST from M1004 . L(R)),
 *                                                 6 kicks (with 0x3fa/0x3da/0x3d5/0x3fd A+D)
 * Face/Back entries (995.y = 1 / 2, then the same loop):
 *   0x2b78/0x2bb0 (0x2890), 0x2fe8/0x3020 (0x2c28), 0x34a0/0x34d8 (0x3098),
 *   0x39f8/0x3a30 (0x3550).
 *
 * Specular (all three spec loops):
 *   v = eye(976) - pos, s = 1/|v|, d = (e - pos) . nrm with e = (973.w, 974.w, 975.w)
 *   R = (nrm*d)*s + (nrm*d)*s - v*s           (reflection of the eye vector)
 *   S = max(M973 . R, 0) squared (1008.x + 1) times
 *   spec = (M977 . S) * (970 * 1008.y)
 */
#include "vu1_set2_int.h"

static const vu_f HALF = 0x3f000000u;

static vu_f erleng(const vu_f v[3]) {
    float x = vu_in(v[0]), y = vu_in(v[1]), z = vu_in(v[2]);
    return vu_out(1.0f / sqrtf(x * x + y * y + z * z));
}

static void renorm(Vu1Ref* r, vu_f nrm[3]) {
    if (!vu1_ref_ilw(r, 1008, 2)) return;
    vu_f p = erleng(nrm);
    for (int l = 0; l < 3; ++l) nrm[l] = vu_mul(nrm[l], p);
}

/* M (4 qw at q) . (P,1) xyz and M3 . N xyz */
static void xform(Vu1Ref* r, uint32_t q, const vu_f* P, const vu_f* N, vu_f tp[3], vu_f tn[3]) {
    Q4 m0 = ld(r, q), m1 = ld(r, q + 1), m2 = ld(r, q + 2), m3 = ld(r, q + 3);
    for (int l = 0; l < 3; ++l) {
        vu_f acc = vu_mul(m0.v[l], P[0]);
        acc = vu_madd(acc, m1.v[l], P[1]);
        acc = vu_madd(acc, m2.v[l], P[2]);
        tp[l] = vu_madd(acc, m3.v[l], ONE);
        acc = vu_mul(m0.v[l], N[0]);
        acc = vu_madd(acc, m1.v[l], N[1]);
        tn[l] = vu_madd(acc, m2.v[l], N[2]);
    }
}

/* Common per-vertex front end: clip-space position, STQ, lit colour (pre-C),
   ADC update.  Returns clip-space-scaled xyzf in xyzf[], stq[], col[]. */
typedef struct {
    vu_f pos[3], nrm[3];
    vu_f stq[3], xyzf[4], col[4];
    vu_f q;
} Vtx;

static void light_col(Vu1Ref* r, const vu_f nrm[3], vu_f col[4]) {
    vu_f L[3];
    for (int l = 0; l < 3; ++l) {
        vu_f acc = vu_mul(VF(17)[l], nrm[0]);
        acc = vu_madd(acc, VF(18)[l], nrm[1]);
        L[l] = vu_madd(acc, VF(19)[l], nrm[2]);
    }
    for (int l = 0; l < 3; ++l) L[l] = vu_max(L[l], 0);
    for (int l = 0; l < 3; ++l) {
        vu_f acc = vu_mul(VF(20)[l], L[0]);
        acc = vu_madd(acc, VF(21)[l], L[1]);
        col[l] = vu_madd(acc, VF(22)[l], L[2]);
    }
    for (int l = 0; l < 3; ++l) col[l] = vu_madd(vu_mul(VF(23)[l], ONE), col[l], VF(25)[l]);
    col[3] = VF(25)[3];
}

/* position -> screen, ADC.  Updates *vi5 and *vi12. */
static void screen(Vu1Ref* r, Vtx* v, const vu_f s4[3], uint32_t face, uint32_t* vi5, uint32_t* vi12) {
    vu_f clip[4];
    for (int l = 0; l < 4; ++l) {
        vu_f acc = vu_mul(VF(28)[l], v->pos[0]);
        acc = vu_madd(acc, VF(29)[l], v->pos[1]);
        acc = vu_madd(acc, VF(30)[l], v->pos[2]);
        clip[l] = vu_madd(acc, VF(31)[l], ONE);
    }
    vu_f q = vu_div(ONE, clip[3]);
    uint32_t cj = vu_clip(clip);
    uint32_t clipw = vu1_ref_ilw(r, 999 + (cj ? 1 : 0), 3);
    v->q = q;
    for (int l = 0; l < 3; ++l) v->stq[l] = vu_mul(s4[l], q);
    vu_f scr[3];
    for (int l = 0; l < 3; ++l) scr[l] = vu_madd(vu_mul(VF(27)[l], ONE), vu_mul(clip[l], q), VF(26)[l]);
    *vi5 = ((*vi5 | face | clipw) << 1) & 0xffff;
    *vi12 &= *vi5;
    for (int l = 0; l < 3; ++l) v->xyzf[l] = (vu_f)vu_ftoi(scr[l], 4);
    v->xyzf[3] = (vu_f)(int32_t)(int16_t)((vu1_ref_ilw(r, 998, 3) | *vi5) & 0xffff);
}

static void stq_in(Vu1Ref* r, const Q4* P, const Q4* N, vu_f s4[3]) {
    const vu_f* uv = (const vu_f*)vu1_ref_qw(r, 972);
    s4[1] = vu_add(0, N->v[3]);
    s4[0] = vu_add(0, P->v[3]);
    s4[2] = vu_add(0, ONE);
    s4[0] = vu_add(s4[0], uv[0]);
    s4[1] = vu_add(s4[1], uv[1]);
}

static void clamp_col(vu_f col[4], vu_f ixyz, vu_f iw) {
    for (int l = 0; l < 3; ++l) col[l] = vu_min(col[l], ixyz);
    col[3] = vu_min(col[3], iw);
}

/* loop 0x2890 */
static void loop_2890(Vu1Ref* r) {
    uint32_t n = VI(9), o = VI(8), vi5 = VI(5), vi12 = 0x8000;
    if (n == 0) n = 0x10000;
    for (uint32_t i = 0; i < n; ++i) {
        Q4 P = ld(r, VI(2) + i), N = ld(r, VI(3) + i), C = ld(r, VI(4) + i), B = ld(r, VI(6) + i);
        vu_f s4[3];
        stq_in(r, &P, &N, s4);
        vu_f p0[3], n0[3], p1[3], n1[3];
        xform(r, B.v[0] & 0xffff, P.v, N.v, p0, n0);
        xform(r, B.v[1] & 0xffff, P.v, N.v, p1, n1);
        Vtx v;
        for (int l = 0; l < 3; ++l) {
            v.nrm[l] = vu_madd(vu_mul(n0[l], B.v[2]), n1[l], B.v[3]);
            v.pos[l] = vu_madd(vu_mul(p0[l], B.v[2]), p1[l], B.v[3]);
        }
        renorm(r, v.nrm);
        vu_f pos4[4] = {v.pos[0], v.pos[1], v.pos[2], ONE};
        uint32_t face = set2_facing_0440(r, pos4);
        memcpy(v.pos, pos4, 12);
        light_col(r, v.nrm, v.col);
        screen(r, &v, s4, face, &vi5, &vi12);
        for (int l = 0; l < 4; ++l) v.col[l] = vu_mul(v.col[l], C.v[l]);
        clamp_col(v.col, F255, F128);
        int32_t ci[4];
        for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(v.col[l], 0);
        st(r, o, v.stq, 14);
        st(r, o + 1, (vu_f*)ci, 15);
        st(r, o + 2, v.xyzf, 15);
        o += 3;
    }
    r->regs.I = F128;
    VI(5) = vi5; VI(9) = 0;
    vu1_ref_xgkick(r, vi12 ? vu1_ref_ilw(r, 999, 0) : vu1_ref_ilw(r, 1017, 0));
}

/* Skinning of the spec loops: bone 0 always, bone 1 if B.y != 0. */
static void skin_cond(Vu1Ref* r, const Q4* P, const Q4* N, const Q4* B, vu_f pos[3], vu_f nrm[3]) {
    for (int l = 0; l < 3; ++l) pos[l] = nrm[l] = 0;
    uint32_t bone = B->v[0] & 0xffff;
    for (int k = 0; k < 2; ++k) {
        vu_f tp[3], tn[3];
        xform(r, bone, P->v, N->v, tp, tn);
        vu_f wk = B->v[2 + k];
        for (int l = 0; l < 3; ++l) {
            pos[l] = vu_madd(vu_mul(tp[l], wk), pos[l], ONE);
            nrm[l] = vu_madd(vu_mul(tn[l], wk), nrm[l], ONE);
        }
        bone = B->v[1] & 0xffff;
        if (!bone) break;
    }
}

/* Front of the spec loops: skin, facing, d = (e - pos).nrm, R, lit colour.
   On return VF(17..19) are reloaded from 973..975 (the loops do). */
static void spec_front(Vu1Ref* r, Vtx* v, vu_f R[3], vu_f* sdot) {
    vu_f d[3];
    for (int l = 0; l < 3; ++l) d[l] = vu_sub(VF(17 + l)[3], v->pos[l]);
    vu_f acc = vu_mul(d[0], v->nrm[0]);
    acc = vu_madd(acc, d[1], v->nrm[1]);
    vu_f dw = vu_madd(acc, d[2], v->nrm[2]);
    const vu_f* eye = (const vu_f*)vu1_ref_qw(r, 976);
    vu_f e[3];
    for (int l = 0; l < 3; ++l) e[l] = vu_sub(eye[l], v->pos[l]);
    vu_f s = erleng(e);
    light_col(r, v->nrm, v->col);
    for (int k = 0; k < 3; ++k) memcpy(VF(17 + k), vu1_ref_qw(r, 973 + k), 16);
    vu_f n5[3];
    for (int l = 0; l < 3; ++l) n5[l] = vu_mul(v->nrm[l], dw);
    for (int l = 0; l < 3; ++l) {
        vu_f a = vu_mul(n5[l], s);
        a = vu_madd(a, n5[l], s);
        R[l] = vu_sub(a, vu_mul(e[l], s));
    }
    *sdot = s;
}

/* L(R) -> power -> M977 . S, the uncoloured specular (xyz). Also returns L(R)
   pre-max in lr[]. */
static void spec_pow(Vu1Ref* r, const vu_f R[3], vu_f lr[3], vu_f sp[3]) {
    vu_f S[3];
    for (int l = 0; l < 3; ++l) {
        vu_f acc = vu_mul(VF(17)[l], R[0]);
        acc = vu_madd(acc, VF(18)[l], R[1]);
        lr[l] = vu_madd(acc, VF(19)[l], R[2]);
    }
    for (int l = 0; l < 3; ++l) S[l] = vu_max(lr[l], 0);
    uint32_t k = vu1_ref_ilw(r, 1008, 0);
    for (uint32_t i = 0;; i = (i + 1) & 0xffff) {
        for (int l = 0; l < 3; ++l) S[l] = vu_mul(S[l], S[l]);
        if (i == k) break;
    }
    for (int l = 0; l < 3; ++l) {
        vu_f acc = vu_mul(VF(20)[l], S[0]);
        acc = vu_madd(acc, VF(21)[l], S[1]);
        sp[l] = vu_madd(acc, VF(22)[l], S[2]);
    }
}

/* mode 0: 0x2c28 (one pass, spec x C added), 1: 0x3098 (two pass), 2: 0x3550 (env, 6 kicks) */
static void loop_spec(Vu1Ref* r, int mode) {
    uint32_t n = VI(9), o = VI(8), vi5 = VI(5), vi12 = 0x8000;
    uint32_t o2 = 0;
    if (mode) {
        uint32_t hx = vu1_ref_ilw(r, vu1_ref_ilw(r, 1017, 1), 0);
        o2 = (o + 3 * n) & 0xffff;
        st(r, o2, ld(r, mode == 1 ? 1012 : 1010).v, 15);
        vu1_ref_isw(r, o2, 0, hx);
        o2 = (o2 + 1) & 0xffff;
    }
    if (n == 0) n = 0x10000;
    vu_f Ireg = r->regs.I;
    for (uint32_t i = 0; i < n; ++i) {
        Q4 P = ld(r, VI(2) + i), N = ld(r, VI(3) + i), C = ld(r, VI(4) + i), B = ld(r, VI(6) + i);
        Vtx v;
        skin_cond(r, &P, &N, &B, v.pos, v.nrm);
        renorm(r, v.nrm);
        vu_f pos4[4] = {v.pos[0], v.pos[1], v.pos[2], ONE};
        uint32_t face = set2_facing_0440(r, pos4);
        memcpy(v.pos, pos4, 12);
        vu_f R[3], s, lr[3], sp[3];
        spec_front(r, &v, R, &s);
        vu_f s4[3];
        stq_in(r, &P, &N, s4);
        screen(r, &v, s4, face, &vi5, &vi12);
        for (int l = 0; l < 4; ++l) v.col[l] = vu_mul(v.col[l], C.v[l]);
        const vu_f* m1008 = (const vu_f*)vu1_ref_qw(r, 1008);
        const vu_f* m970 = (const vu_f*)vu1_ref_qw(r, 970);
        vu_f c6[3];
        for (int l = 0; l < 3; ++l) c6[l] = vu_mul(m970[l], m1008[1]);
        int32_t ci[4];
        if (mode == 0) {
            clamp_col(v.col, Ireg, F128);   /* first vertex: I from entry */
            spec_pow(r, R, lr, sp);
            for (int l = 0; l < 3; ++l) sp[l] = vu_mul(sp[l], c6[l]);
            for (int l = 0; l < 3; ++l) v.col[l] = vu_min(vu_madd(vu_mul(sp[l], C.v[l]), v.col[l], ONE), F255);
            Ireg = F255;
            for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(v.col[l], 0);
            st(r, o, v.stq, 14);
            st(r, o + 1, (vu_f*)ci, 15);
            st(r, o + 2, v.xyzf, 15);
        } else if (mode == 1) {
            clamp_col(v.col, F255, F128);
            for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(v.col[l], 0);
            st(r, o, v.stq, 14);
            st(r, o2, v.stq, 14);
            st(r, o + 1, (vu_f*)ci, 15);
            st(r, o + 2, v.xyzf, 15);
            st(r, o2 + 2, v.xyzf, 15);
            spec_pow(r, R, lr, sp);
            vu_f c1[4], c[4];
            for (int l = 0; l < 3; ++l) c1[l] = vu_mul(C.v[l], c6[l]);
            c1[3] = vu_mul(C.v[3], ONE);
            for (int l = 0; l < 3; ++l) c[l] = vu_mul(sp[l], c1[l]);
            c[3] = vu_mul(ONE, c1[3]);
            clamp_col(c, F255, F128);
            for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(c[l], 0);
            st(r, o2 + 1, (vu_f*)ci, 15);
            o2 += 3;
        } else {
            st(r, o2 + 1, ld(r, 1009).v, 15);
            clamp_col(v.col, F255, F128);
            st(r, o, v.stq, 14);
            st(r, o + 2, v.xyzf, 15);
            st(r, o2 + 2, v.xyzf, 15);
            /* L(R) uses 973 rows (xyz) */
            spec_pow(r, R, lr, sp);
            vu_f e8[3];
            for (int l = 0; l < 3; ++l) {
                const vu_f* m0 = (const vu_f*)vu1_ref_qw(r, 1004);
                const vu_f* m1 = (const vu_f*)vu1_ref_qw(r, 1005);
                const vu_f* m2 = (const vu_f*)vu1_ref_qw(r, 1006);
                vu_f acc = vu_mul(m0[l], lr[0]);
                acc = vu_madd(acc, m1[l], lr[1]);
                e8[l] = vu_madd(acc, m2[l], lr[2]);
            }
            for (int l = 0; l < 2; ++l) e8[l] = vu_add(e8[l], ONE);
            for (int l = 0; l < 2; ++l) e8[l] = vu_mul(e8[l], HALF);
            e8[2] = ONE;
            e8[0] = vu_add(vu_mul(e8[0], VF(24)[1]), VF(24)[0]);
            e8[1] = vu_add(vu_mul(e8[1], VF(24)[3]), VF(24)[2]);
            for (int l = 0; l < 3; ++l) VF(8)[l] = vu_mul(e8[l], v.q);
            for (int l = 0; l < 3; ++l) sp[l] = vu_mul(vu_mul(sp[l], c6[l]), F128);
            for (int l = 0; l < 3; ++l) v.col[l] = vu_min(vu_add(v.col[l], sp[l]), F255);
            st(r, o2, VF(8), 15);
            for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(v.col[l], 0);
            st(r, o + 1, (vu_f*)ci, 15);
            o2 += 3;
        }
        o += 3;
    }
    r->regs.I = mode == 1 ? F128 : F255;   /* last LOI of each loop */
    VI(5) = vi5; VI(9) = 0;
    if (vi12) { vu1_ref_xgkick(r, vu1_ref_ilw(r, 999, 0)); return; }
    vu1_ref_xgkick(r, vu1_ref_ilw(r, 1017, 0));
    if (mode == 1) vu1_ref_xgkick(r, o);
    if (mode == 2) {
        vu1_ref_xgkick(r, 0x3fa);
        vu1_ref_xgkick(r, 0x3da);
        vu1_ref_xgkick(r, o);
        vu1_ref_xgkick(r, 0x3d5);
        vu1_ref_xgkick(r, 0x3fd);
    }
}

static const struct { uint32_t entry, loop; int face; const char* name; } k_entries[] = {
    {0x2850, 0x2890, 0, "set2_entry_blend_lit"},
    {0x2b78, 0x2890, 1, "set2_entry_blend_lit_face"},
    {0x2bb0, 0x2890, 2, "set2_entry_blend_lit_back"},
    {0x2be8, 0x2c28, 0, "set2_entry_blend_lit_spec"},
    {0x2fe8, 0x2c28, 1, "set2_entry_blend_lit_spec_face"},
    {0x3020, 0x2c28, 2, "set2_entry_blend_lit_spec_back"},
    {0x3058, 0x3098, 0, "set2_entry_blend_lit_spec_2pass"},
    {0x34a0, 0x3098, 1, "set2_entry_blend_lit_spec_2pass_face"},
    {0x34d8, 0x3098, 2, "set2_entry_blend_lit_spec_2pass_back"},
    {0x3510, 0x3550, 0, "set2_entry_blend_lit_spec_env"},
    {0x39f8, 0x3550, 1, "set2_entry_blend_lit_spec_env_face"},
    {0x3a30, 0x3550, 2, "set2_entry_blend_lit_spec_env_back"},
};

int set2_blend(Vu1Ref* r, const char** variant) {
    for (unsigned i = 0; i < sizeof k_entries / sizeof k_entries[0]; ++i)
        if (r->pc == k_entries[i].entry) {
            if (variant) *variant = k_entries[i].name;
            set2_init_0050(r);
            if (k_entries[i].face) { VI(1) = k_entries[i].face; vu1_ref_isw(r, 995, 1, k_entries[i].face); }
            VI(14) = 0x2f; VI(15) = k_entries[i].loop / 8;
            return VU1REF_OK;
        }
    if (r->pc != 0x0188) return VU1REF_UNSUPPORTED;
    uint32_t loop = VI(15) * 8u;
    const char* name;
    switch (loop) {
    case 0x2890: name = "set2_blend_lit"; break;
    case 0x2c28: name = "set2_blend_lit_spec"; break;
    case 0x3098: name = "set2_blend_lit_spec_2pass"; break;
    case 0x3550: name = "set2_blend_lit_spec_env"; break;
    default: return VU1REF_UNSUPPORTED;
    }
    if (variant) *variant = name;
    if (!set2_header_0188(r)) return VU1REF_OK;
    switch (loop) {
    case 0x2890: loop_2890(r); break;
    case 0x2c28: loop_spec(r, 0); break;
    case 0x3098: loop_spec(r, 1); break;
    case 0x3550: loop_spec(r, 2); break;
    }
    return VU1REF_OK;
}

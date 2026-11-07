/*
 * Set 2, rigid group of s_aUcodeEnvAdr (one matrix per vertex, picked by the
 * bone stream's x lane; no weights).  See VU1_PROGRAMS.md.
 *   entry 0x1790 -> loop 0x17d0 "rigid_lit"            as skin_lit, one matrix
 *   entry 0x1a88 -> loop 0x1ac8 "rigid_lit_spec"       + specular added to the colour
 *   entry 0x1ec0 -> loop 0x1f00 "rigid_lit_spec_2pass" + specular as a second strip (tag 1012)
 *   entry 0x2338 -> loop 0x2378 "rigid_lit_spec_6k"    + env-map second strip (tag 1010), 6 kicks
 * Face/Back stubs (995.y = 1 / 2): 0x1a18/0x1a50, 0x1e50/0x1e88, 0x22c8/0x2300, 0x27e0/0x2818.
 *
 * Specular, per vertex (all three spec loops):
 *   e   = eye(976) - pos,  p = 1/|e|
 *   d   = sum_k (973+k).w - pos_k) * nrm_k          (vf5.w; rows' w = eye in object space)
 *   R   = 2 d p nrm - p e                            (reflection of the eye vector)
 *   S   = max(M973 * R, 0) ^ (2^(1008.x+1))          (repeated squaring)
 *   spec= (M977 * S) * (970.xyz * 1008.y)
 */
#include "vu1_set2_int.h"

#include <stdio.h>

static const vu_f HALF = 0x3f000000u;

/* pos = M (P,1), nrm = M3 N for the bone at B.x; optional renormalise. */
static void rigid_xform(Vu1Ref* r, uint32_t i, vu_f pos[4], vu_f nrm[4]) {
    Q4 P = ld(r, VI(2) + i), N = ld(r, VI(3) + i);
    uint32_t bone = vu1_ref_ilw(r, VI(6) + i, 0);
    Q4 m0 = ld(r, bone), m1 = ld(r, bone + 1), m2 = ld(r, bone + 2), m3 = ld(r, bone + 3);
    for (int l = 0; l < 3; ++l) {
        vu_f acc = vu_mul(m0.v[l], P.v[0]);
        acc = vu_madd(acc, m1.v[l], P.v[1]);
        acc = vu_madd(acc, m2.v[l], P.v[2]);
        pos[l] = vu_madd(acc, m3.v[l], ONE);
        acc = vu_mul(m0.v[l], N.v[0]);
        acc = vu_madd(acc, m1.v[l], N.v[1]);
        nrm[l] = vu_madd(acc, m2.v[l], N.v[2]);
    }
    pos[3] = ONE; nrm[3] = ONE;
    if (vu1_ref_ilw(r, 1008, 2)) {
        float x = vu_in(nrm[0]), y = vu_in(nrm[1]), z = vu_in(nrm[2]);
        vu_f p = vu_out(1.0f / sqrtf(x * x + y * y + z * z));
        for (int l = 0; l < 3; ++l) nrm[l] = vu_mul(nrm[l], p);
    }
}

/* Everything up to the pass-1 colour (before min), shared by the loops. */
typedef struct {
    vu_f stq[3], scr[4], col[4], R[3], d;
    uint32_t vi5;
} RigidV;

static void rigid_common(Vu1Ref* r, uint32_t i, RigidV* o, int spec) {
    Q4 P = ld(r, VI(2) + i), N = ld(r, VI(3) + i), C = ld(r, VI(4) + i);
    vu_f uv[4];
    memcpy(uv, vu1_ref_qw(r, 972), 16);
    vu_f s4[3];
    s4[1] = vu_add(0, N.v[3]);
    s4[0] = vu_add(0, P.v[3]);
    s4[2] = ONE;
    s4[0] = vu_add(s4[0], uv[0]);
    s4[1] = vu_add(s4[1], uv[1]);
    vu_f pos[4], nrm[4];
    rigid_xform(r, i, pos, nrm);
    uint32_t face = set2_facing_0440(r, pos);
    vu_f L[4], e[3], p = 0, v5[4];
    if (spec) {
        vu_f eye[4];
        memcpy(eye, vu1_ref_qw(r, 976), 16);
        for (int k = 0; k < 3; ++k) VF(17 + k)[3] = vu_sub(VF(17 + k)[3], pos[k]);
        for (int l = 0; l < 3; ++l) e[l] = vu_sub(eye[l], pos[l]);
        float x = vu_in(e[0]), y = vu_in(e[1]), z = vu_in(e[2]);
        p = vu_out(1.0f / sqrtf(x * x + y * y + z * z));
    }
    for (int l = 0; l < 4; ++l) {
        vu_f acc = vu_mul(VF(17)[l], nrm[0]);
        acc = vu_madd(acc, VF(18)[l], nrm[1]);
        v5[l] = vu_madd(acc, VF(19)[l], nrm[2]);
    }
    if (spec) for (int k = 0; k < 3; ++k) memcpy(VF(17 + k), vu1_ref_qw(r, 973 + k), 16);
    for (int l = 0; l < 3; ++l) L[l] = vu_max(v5[l], 0);
    vu_f clip[4], col[4];
    for (int l = 0; l < 4; ++l) {
        vu_f acc = vu_mul(VF(28)[l], pos[0]);
        acc = vu_madd(acc, VF(29)[l], pos[1]);
        acc = vu_madd(acc, VF(30)[l], pos[2]);
        clip[l] = vu_madd(acc, VF(31)[l], ONE);
    }
    col[3] = VF(25)[3];
    for (int l = 0; l < 3; ++l) {
        vu_f acc = vu_mul(VF(20)[l], L[0]);
        acc = vu_madd(acc, VF(21)[l], L[1]);
        col[l] = vu_madd(acc, VF(22)[l], L[2]);
    }
    vu_f q = vu_div(ONE, clip[3]);
    r->regs.Q = q;
    uint32_t cj = vu_clip(clip);
    for (int l = 0; l < 3; ++l) col[l] = vu_madd(vu_mul(VF(23)[l], ONE), col[l], VF(25)[l]);
    uint32_t clipw = vu1_ref_ilw(r, 999 + (cj ? 1 : 0), 3);
    for (int l = 0; l < 3; ++l) o->scr[l] = vu_mul(clip[l], q);
    for (int l = 0; l < 3; ++l) o->stq[l] = vu_mul(s4[l], q);
    for (int l = 0; l < 4; ++l) o->col[l] = vu_mul(col[l], C.v[l]);
    for (int l = 0; l < 3; ++l) o->scr[l] = vu_madd(vu_mul(VF(27)[l], ONE), o->scr[l], VF(26)[l]);
    o->vi5 = ((VI(5) | face | clipw) << 1) & 0xffff;
    VI(5) = o->vi5;
    if (spec) {
        o->d = v5[3];
        vu_f n5[3];
        for (int l = 0; l < 3; ++l) n5[l] = vu_mul(nrm[l], v5[3]);
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(n5[l], p);
            acc = vu_add(acc, vu_mul(n5[l], p));
            o->R[l] = vu_sub(acc, vu_mul(e[l], p));
        }
    }
}

static void put_xyzf(vu_f out[4], const RigidV* v, uint32_t fogw) {
    for (int l = 0; l < 3; ++l) out[l] = (vu_f)vu_ftoi(v->scr[l], 4);
    out[3] = (vu_f)(int32_t)(int16_t)((fogw | v->vi5) & 0xffff);
}

/* S = max(M973 R, 0), squared 1008.x + 1 times; Sraw = before the max. */
static void spec_pow(Vu1Ref* r, const vu_f R[3], vu_f S[3], vu_f Sraw[3]) {
    for (int l = 0; l < 3; ++l) {
        vu_f acc = vu_mul(VF(17)[l], R[0]);
        acc = vu_madd(acc, VF(18)[l], R[1]);
        Sraw[l] = vu_madd(acc, VF(19)[l], R[2]);
    }
    for (int l = 0; l < 3; ++l) S[l] = vu_max(Sraw[l], 0);
    uint32_t c = vu1_ref_ilw(r, 1008, 0);
    for (;;) {
        for (int l = 0; l < 3; ++l) S[l] = vu_mul(S[l], S[l]);
        if (!c) break;
        c = (c - 1) & 0xffff;
    }
}

static void lcol(Vu1Ref* r, const vu_f S[3], vu_f o[3]) {
    for (int l = 0; l < 3; ++l) {
        vu_f acc = vu_mul(VF(20)[l], S[0]);
        acc = vu_madd(acc, VF(21)[l], S[1]);
        o[l] = vu_madd(acc, VF(22)[l], S[2]);
    }
}

static void finish(Vu1Ref* r, uint32_t vi12, int nk, const uint32_t* extra) {
    if (vi12) { vu1_ref_xgkick(r, vu1_ref_ilw(r, 999, 0)); return; }
    vu1_ref_xgkick(r, vu1_ref_ilw(r, 1017, 0));
    for (int k = 0; k < nk; ++k) vu1_ref_xgkick(r, extra[k]);
}

/* mode: 0 rigid_lit, 1 spec, 2 two-pass, 3 six-kick */
static void rigid_loop(Vu1Ref* r, int mode) {
    uint32_t n = VI(9), o = VI(8), vi12 = 0x8000, o2 = 0;
    uint32_t fogw = vu1_ref_ilw(r, 998, 3);
    if (n == 0) n = 0x10000;
    if (mode >= 2) {
        uint32_t top = vu1_ref_ilw(r, 1017, 1);
        o2 = (o + 3 * n) & 0xffff;
        st(r, o2, ld(r, mode == 2 ? 1012 : 1010).v, 15);
        vu1_ref_isw(r, o2, 0, vu1_ref_ilw(r, top, 0));
        o2++;
    }
    vu_f I = r->regs.I;
    for (uint32_t i = 0; i < n; ++i, o += 3, o2 += 3) {
        RigidV v;
        rigid_common(r, i, &v, mode != 0);
        vi12 &= v.vi5;
        vu_f xyzf[4], col[4];
        int32_t ci[4];
        put_xyzf(xyzf, &v, fogw);
        if (mode == 0) {
            for (int l = 0; l < 3; ++l) col[l] = vu_min(v.col[l], F255);
            col[3] = vu_min(v.col[3], F128);
            for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(col[l], 0);
            st(r, o, v.stq, 14); st(r, o + 1, (vu_f*)ci, 15); st(r, o + 2, xyzf, 15);
            continue;
        }
        if (mode == 1) {
            for (int l = 0; l < 3; ++l) col[l] = vu_min(v.col[l], I);
            col[3] = vu_min(v.col[3], F128);
            I = F255;
            st(r, o, v.stq, 14);
            st(r, o + 2, xyzf, 15);
            vu_f S[3], Sr[3], sp[3], v6[3];
            vu_f y = ((vu_f*)vu1_ref_qw(r, 1008))[1];
            for (int l = 0; l < 3; ++l) v6[l] = vu_mul(((vu_f*)vu1_ref_qw(r, 970))[l], y);
            spec_pow(r, v.R, S, Sr);
            lcol(r, S, sp);
            for (int l = 0; l < 3; ++l) sp[l] = vu_mul(sp[l], v6[l]);
            Q4 C = ld(r, VI(4) + i);
            for (int l = 0; l < 3; ++l) col[l] = vu_min(vu_add(vu_mul(sp[l], C.v[l]), col[l]), F255);
            for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(col[l], 0);
            st(r, o + 1, (vu_f*)ci, 15);
            continue;
        }
        for (int l = 0; l < 3; ++l) col[l] = vu_min(v.col[l], F255);
        col[3] = vu_min(v.col[3], F128);
        st(r, o, v.stq, 14);
        st(r, o2, v.stq, 14);
        vu_f v6[4];
        memcpy(v6, vu1_ref_qw(r, 970), 12);
        v6[3] = ONE;
        vu_f y = ((vu_f*)vu1_ref_qw(r, 1008))[1];
        for (int l = 0; l < 3; ++l) v6[l] = vu_mul(v6[l], y);
        Q4 C = ld(r, VI(4) + i);
        vu_f S[3], Sr[3], sp[4];
        if (mode == 2) {
            for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(col[l], 0);
            st(r, o + 1, (vu_f*)ci, 15);
            st(r, o + 2, xyzf, 15);
            vu_f c1[4];
            for (int l = 0; l < 4; ++l) c1[l] = vu_mul(C.v[l], v6[l]);
            st(r, o2 + 2, xyzf, 15);
            spec_pow(r, v.R, S, Sr);
            lcol(r, S, sp);
            sp[3] = ONE;
            for (int l = 0; l < 4; ++l) sp[l] = vu_mul(sp[l], c1[l]);
            for (int l = 0; l < 3; ++l) sp[l] = vu_min(sp[l], F255);
            sp[3] = vu_min(sp[3], F128);
            for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(sp[l], 0);
            st(r, o2 + 1, (vu_f*)ci, 15);
            continue;
        }
        /* mode 3: env-map second strip */
        st(r, o2 + 1, ld(r, 1009).v, 15);
        st(r, o + 2, xyzf, 15);
        st(r, o2 + 2, xyzf, 15);
        vu_f e8[4];
        {
            vu_f Sraw[3];
            for (int l = 0; l < 3; ++l) {
                vu_f acc = vu_mul(VF(17)[l], v.R[0]);
                acc = vu_madd(acc, VF(18)[l], v.R[1]);
                Sraw[l] = vu_madd(acc, VF(19)[l], v.R[2]);
            }
            Q4 a = ld(r, 1004), b = ld(r, 1005), c = ld(r, 1006);
            for (int l = 0; l < 3; ++l) {
                vu_f acc = vu_mul(a.v[l], Sraw[0]);
                acc = vu_madd(acc, b.v[l], Sraw[1]);
                e8[l] = vu_madd(acc, c.v[l], Sraw[2]);
            }
            e8[3] = VF(8)[3];
            for (int l = 0; l < 3; ++l) S[l] = vu_max(Sraw[l], 0);
            uint32_t cnt = vu1_ref_ilw(r, 1008, 0);
            for (;;) {
                for (int l = 0; l < 3; ++l) S[l] = vu_mul(S[l], S[l]);
                if (!cnt) break;
                cnt = (cnt - 1) & 0xffff;
            }
        }
        for (int l = 0; l < 2; ++l) e8[l] = vu_add(e8[l], ONE);
        for (int l = 0; l < 2; ++l) e8[l] = vu_mul(e8[l], HALF);
        lcol(r, S, sp);
        e8[2] = ONE;
        e8[0] = vu_mul(e8[0], VF(24)[1]);
        e8[1] = vu_mul(e8[1], VF(24)[3]);
        for (int l = 0; l < 3; ++l) sp[l] = vu_mul(sp[l], v6[l]);
        e8[0] = vu_add(e8[0], VF(24)[0]);
        e8[1] = vu_add(e8[1], VF(24)[2]);
        for (int l = 0; l < 3; ++l) sp[l] = vu_mul(sp[l], F128);
        for (int l = 0; l < 3; ++l) e8[l] = vu_mul(e8[l], r->regs.Q);
        for (int l = 0; l < 3; ++l) col[l] = vu_min(vu_add(col[l], sp[l]), F255);
        memcpy(VF(8), e8, 16);
        st(r, o2, e8, 15);
        for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(col[l], 0);
        st(r, o + 1, (vu_f*)ci, 15);
    }
    r->regs.I = mode == 0 || mode == 1 || mode == 3 ? F255 : F128;
    VI(9) = 0;
    if (mode == 2) {
        uint32_t ex[1] = {o & 0xffff};
        finish(r, vi12, 1, ex);
        (void)ex;
    } else if (mode == 3) {
        /* o has advanced past the batch: vi8 points at the second tag */
        uint32_t ex[5] = {0x3fa, 0x3da, o & 0xffff, 0x3d5, 0x3fd};
        finish(r, vi12, 5, ex);
    } else {
        finish(r, vi12, 0, 0);
    }
}

static const struct { uint32_t entry, loop, face, back; const char* name; } g_prog[4] = {
    {0x1790, 0x17d0, 0x1a18, 0x1a50, "rigid_lit"},
    {0x1a88, 0x1ac8, 0x1e50, 0x1e88, "rigid_lit_spec"},
    {0x1ec0, 0x1f00, 0x22c8, 0x2300, "rigid_lit_spec_2pass"},
    {0x2338, 0x2378, 0x27e0, 0x2818, "rigid_lit_spec_6k"},
};

int set2_rigid(Vu1Ref* r, const char** variant) {
    static char names[4][3][48];
    for (int m = 0; m < 4; ++m) {
        int which = r->pc == g_prog[m].entry ? 0 : r->pc == g_prog[m].face ? 1 : r->pc == g_prog[m].back ? 2 : -1;
        if (which >= 0) {
            static const char* sfx[3] = {"entry", "entry_face", "entry_back"};
            snprintf(names[m][which], 48, "set2_%s_%s", sfx[which], g_prog[m].name);
            if (variant) *variant = names[m][which];
            set2_init_0050(r);
            if (which) { vu1_ref_isw(r, 995, 1, which); VI(1) = which; }
            VI(14) = 0x2f; VI(15) = g_prog[m].loop / 8;
            return VU1REF_OK;
        }
        if (r->pc == 0x0188 && VI(15) * 8u == g_prog[m].loop) {
            snprintf(names[m][0], 48, "set2_%s", g_prog[m].name);
            if (variant) *variant = names[m][0];
            if (!set2_header_0188(r)) return VU1REF_OK;
            rigid_loop(r, m);
            return VU1REF_OK;
        }
    }
    return VU1REF_UNSUPPORTED;
}

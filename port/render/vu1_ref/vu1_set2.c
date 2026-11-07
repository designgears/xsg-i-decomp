/*
 * Microcode set 2 (.vudata MPG chain 2, 0x3a68 bytes): s_nProgType 1, the
 * s_aUcodeEnv* family.  Matrix-palette skinned (up to 4 bones per vertex),
 * directionally lit, optional per-triangle facing test, optional specular /
 * environment second pass.  In the opening cutscene: KOS-MOS and the people.
 *
 * Shape of a draw as in set 0: MSCAL entry (0x0050 constants, then stop in
 * 0x0178) and per batch MSCNT -> 0x0188 header -> jr vi15 (the entry's loop).
 * References here:
 *   entry 0x05b0 (EnvAdr[0])  -> loop 0x05f0 "skin_lit"   skinned, lit, one pass
 * Everything else in the set returns VU1REF_UNSUPPORTED (see VU1_PROGRAMS.md for
 * the list and what they add: 0x0da0 = skin_lit + specular pass, ...).
 *
 * Extra constants over set 0 (data memory qw):
 *   732+4k  bone matrices, 4 qw each (nmlPacketAddParts, 0x2DC + 4k); a vertex
 *           addresses them by qw address in its bone stream
 *   973-975 vf17-19  normal -> light-direction matrix (nmlPacketAddParts 0x3CD rows 0-2)
 *   977-980 vf20-23  light colours (3 columns) + ambient (vf23)
 *   968     vf25     material diffuse (x 971 at the entry), .w = alpha
 *   991-994          ring of the last 3 skinned positions (facing test)
 *   995     x: ring index, y: facing mode (0 off, 1/2 = strip parity) (entries 0x08a8/0x08e0..)
 *   998.w            XYZF2 w bits ORed into every vertex (fog word, nmlPacketAddFog)
 *   1008    z: renormalise blended normals, w: 2 = morph colours instead of P/N
 *   1017    z: != 0 mirrors y about 1017.z (P.y = 1017.z - P.y)
 */
#include "vu1_ref.h"
#include "vu_float.h"
#include "vu1_set2_int.h"

#include <string.h>

void set2_init_0050(Vu1Ref* r) {
    static const struct { int vf, q; } ld_tab[] = {
        {8, 976}, {12, 732}, {13, 733}, {14, 734}, {15, 735}, {16, 970}, {17, 973}, {18, 974},
        {19, 975}, {20, 977}, {21, 978}, {22, 979}, {23, 980}, {24, 971}, {25, 968}, {26, 966},
        {27, 967}, {28, 962}, {29, 963}, {30, 964}, {31, 965},
    };
    for (unsigned i = 0; i < sizeof ld_tab / sizeof ld_tab[0]; ++i) memcpy(VF(ld_tab[i].vf), vu1_ref_qw(r, ld_tab[i].q), 16);
    for (int l = 0; l < 4; ++l) VF(25)[l] = vu_mul(VF(25)[l], VF(24)[l]);
    memcpy(VF(24), vu1_ref_qw(r, 969), 16);
    vu1_ref_isw(r, 999, 3, 0);
    vu1_ref_isw(r, 1000, 3, 0x7000);
    vu1_ref_isw(r, 1013, 3, 0x4000);
    vu1_ref_isw(r, 1016, 3, 0x4000);
    vu1_ref_isw(r, 1015, 3, 0);
    vu1_ref_isw(r, 1014, 3, 0);
    vu1_ref_isw(r, 995, 1, 0);
    vu1_ref_isw(r, 999, 0, 0x8000);   /* "kick address" when every vertex is ADC */
}

/*
 * 0x0188 batch header.  TOP+0 as in set 0 (x count|EOP, y 0x4000 register
 * batch, z constant-colour fill count, w morph keys).  Six streams of n qw:
 *   P  TOP+1     position xyz, w = S        B  TOP+1+4n  bone qw addresses (u32 x4)
 *   N  TOP+1+n   normal xyz, w = T          W  TOP+1+5n  bone weights (float x4)
 *   C  TOP+1+2n  colour (float RGBA)        (TOP+1+3n: unused by skin_lit)
 */
int set2_header_0188(Vu1Ref* r) {
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
    uint32_t b = (x + n) & 0xffff, w = (b + n) & 0xffff;
    st(r, out, ld(r, 1011).v, 15);
    vu1_ref_isw(r, out, 0, hx);
    vu1_ref_isw(r, 995, 0, 0);
    /* constant colour: copy C[0] over the first hz colour slots */
    Q4 c0 = ld(r, c);
    for (uint32_t i = 0; i < hz; ++i) st(r, c + i, c0.v, 15);
    uint32_t keys = vu1_ref_ilw(r, top, 3);
    if (keys) {
        vu_f wgt = vu_min(((vu_f*)vu1_ref_qw(r, 734))[3], vu_itof((int16_t)keys, 0));
        int32_t idx = vu_ftoi(wgt, 0);
        vu_f frac = vu_sub(wgt, vu_itof(idx, 0));
        vu_f inv = vu_sub(ONE, frac);
        uint32_t src = (w + n + (uint32_t)(idx & 0xffff)) & 0xffff, stride = keys + 1;
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
    VI(2) = p; VI(3) = nm; VI(4) = c; VI(10) = x; VI(6) = b; VI(7) = w;
    VI(8) = (out + 1) & 0xffff; VI(9) = n; VI(5) = 0;
    return 1;
}

/* 0x0440: optional y mirror, then the per-triangle facing test on the strip.
   Returns the ADC word to OR into the vertex's shift register. */
uint32_t set2_facing_0440(Vu1Ref* r, vu_f pos[4]) {
    if (vu1_ref_ilw(r, 1017, 2)) {
        vu_f mz = ((vu_f*)vu1_ref_qw(r, 1017))[2];
        vu_f acc = vu_mul(pos[1], vu_sub(0, ONE));
        pos[1] = vu_madd(acc, vu_add(0, mz), ONE);
    }
    uint32_t mode = vu1_ref_ilw(r, 995, 1);
    if (!mode) return 0;
    uint32_t i0 = vu1_ref_ilw(r, 995, 0), im = (i0 - 1) & 3;
    vu_f a[3], b[3];
    memcpy(a, vu1_ref_qw(r, 991 + i0), 12);
    memcpy(b, vu1_ref_qw(r, 991 + im), 12);
    uint32_t i1 = (i0 + 1) & 3;
    st(r, 991 + i1, pos, 14);
    vu1_ref_isw(r, 995, 0, i1);
    vu_f e1[3], e2[3];
    for (int l = 0; l < 3; ++l) e1[l] = vu_sub(a[l], pos[l]);
    if ((i1 & 1) != ((mode - 1) & 0xffff))
        for (int l = 0; l < 3; ++l) e2[l] = vu_sub(b[l], a[l]);
    else
        for (int l = 0; l < 3; ++l) e2[l] = vu_sub(a[l], b[l]);
    static const int i1x[3] = {1, 2, 0}, i2x[3] = {2, 0, 1};
    vu_f eye[4], cr[3], d[3], m[3];
    memcpy(eye, vu1_ref_qw(r, 976), 16);
    for (int l = 0; l < 3; ++l) {
        vu_f acc = vu_mul(e1[i1x[l]], e2[i2x[l]]);
        cr[l] = vu_sub(acc, vu_mul(e2[i1x[l]], e1[i2x[l]]));
    }
    for (int l = 0; l < 3; ++l) d[l] = vu_sub(eye[l], pos[l]);
    for (int l = 0; l < 3; ++l) m[l] = vu_mul(cr[l], d[l]);
    vu_f s = vu_add(vu_add(m[0], m[2]), m[1]);
    return vu1_ref_ilw(r, 1013 + ((s & 0x80000000u) ? 2 : 0), 3);
}

/* Bone blend of one vertex: pos/nrm = sum_k w_k * (M_k (P,1)) / (M_k3 N), k < 4,
   stopping at the first zero bone address after the first. */
void set2_skin(Vu1Ref* r, const Q4* P, const Q4* N, const Q4* B, const Q4* W, vu_f pos[4], vu_f nrm[4]) {
    pos[0] = pos[1] = pos[2] = 0; pos[3] = ONE;
    nrm[0] = nrm[1] = nrm[2] = 0; nrm[3] = ONE;
    uint32_t bone = B->v[0] & 0xffff;
    for (int k = 0; k < 4; ++k) {
        Q4 m0 = ld(r, bone), m1 = ld(r, bone + 1), m2 = ld(r, bone + 2), m3 = ld(r, bone + 3);
        vu_f tp[3], tn[3];
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(m0.v[l], P->v[0]);
            acc = vu_madd(acc, m1.v[l], P->v[1]);
            acc = vu_madd(acc, m2.v[l], P->v[2]);
            tp[l] = vu_madd(acc, m3.v[l], ONE);
            acc = vu_mul(m0.v[l], N->v[0]);
            acc = vu_madd(acc, m1.v[l], N->v[1]);
            tn[l] = vu_madd(acc, m2.v[l], N->v[2]);
        }
        vu_f wk = W->v[k];
        for (int l = 0; l < 3; ++l) {
            pos[l] = vu_madd(vu_mul(tp[l], wk), pos[l], ONE);
            nrm[l] = vu_madd(vu_mul(tn[l], wk), nrm[l], ONE);
        }
        bone = B->v[(k + 1) & 3] & 0xffff;
        if (!bone) break;
    }
    if (vu1_ref_ilw(r, 1008, 2)) {
        float x = vu_in(nrm[0]), y = vu_in(nrm[1]), z = vu_in(nrm[2]);
        vu_f p = vu_out(1.0f / sqrtf(x * x + y * y + z * z));
        for (int l = 0; l < 3; ++l) nrm[l] = vu_mul(nrm[l], p);
    }
}

/*
 * loop 0x05f0 "skin_lit", per vertex (after skin() and facing_0440()):
 *   L      = max(M_light(973) * nrm, 0)                 (3 directional lights)
 *   rgb    = ambient(980) + (Lcol(977-979) * L) * diffuse(vf25)
 *   RGBAQ  = ftoi0(min((rgb, vf25.w) * C, (255,255,255,128)))
 *   XYZF2  = ftoi4(scrOff + (M(962) * pos) / W * scrScale), w = mem[998].w | ADC
 *   STQ    = (S + uv.x, T + uv.y, 1) / W
 * ADC as in set 0: clipped vertex -> 0x7000 into the shift register; the facing
 * word -> 0x4000.  If every vertex of the batch ends with ADC set, the program
 * kicks qw 0 (mem[999].x = 0x8000) instead of the batch.
 */
static void skin_lit_loop(Vu1Ref* r) {
    uint32_t n = VI(9), o = VI(8), vi5 = VI(5), vi12 = 0x8000;
    vu_f uv[4];
    memcpy(uv, vu1_ref_qw(r, 972), 16);
    uint32_t fogw = vu1_ref_ilw(r, 998, 3);
    if (n == 0) n = 0x10000;
    for (uint32_t i = 0; i < n; ++i) {
        Q4 P = ld(r, VI(2) + i), N = ld(r, VI(3) + i), C = ld(r, VI(4) + i);
        Q4 B = ld(r, VI(6) + i), W = ld(r, VI(7) + i);
        vu_f s4[3];
        s4[1] = vu_add(0, N.v[3]);
        s4[0] = vu_add(0, P.v[3]);
        s4[2] = vu_add(0, ONE);
        s4[0] = vu_add(s4[0], uv[0]);
        s4[1] = vu_add(s4[1], uv[1]);
        vu_f pos[4], nrm[4];
        set2_skin(r, &P, &N, &B, &W, pos, nrm);
        uint32_t face = set2_facing_0440(r, pos);
        vu_f L[3], col[4], clip[4];
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(VF(17)[l], nrm[0]);
            acc = vu_madd(acc, VF(18)[l], nrm[1]);
            L[l] = vu_madd(acc, VF(19)[l], nrm[2]);
        }
        for (int l = 0; l < 4; ++l) {
            vu_f acc = vu_mul(VF(28)[l], pos[0]);
            acc = vu_madd(acc, VF(29)[l], pos[1]);
            acc = vu_madd(acc, VF(30)[l], pos[2]);
            clip[l] = vu_madd(acc, VF(31)[l], ONE);
        }
        for (int l = 0; l < 3; ++l) L[l] = vu_max(L[l], 0);
        col[3] = VF(25)[3];
        for (int l = 0; l < 3; ++l) {
            vu_f acc = vu_mul(VF(20)[l], L[0]);
            acc = vu_madd(acc, VF(21)[l], L[1]);
            col[l] = vu_madd(acc, VF(22)[l], L[2]);
        }
        vu_f q = vu_div(ONE, clip[3]);
        uint32_t cj = vu_clip(clip);
        clip[3] = ONE;
        for (int l = 0; l < 3; ++l) col[l] = vu_madd(vu_mul(VF(23)[l], ONE), col[l], VF(25)[l]);
        uint32_t clipw = vu1_ref_ilw(r, 999 + (cj ? 1 : 0), 3);
        vu_f scr[4], stq[3];
        for (int l = 0; l < 4; ++l) scr[l] = vu_mul(clip[l], q);
        for (int l = 0; l < 3; ++l) stq[l] = vu_mul(s4[l], q);
        for (int l = 0; l < 4; ++l) col[l] = vu_mul(col[l], C.v[l]);
        for (int l = 0; l < 4; ++l) scr[l] = vu_madd(vu_mul(VF(27)[l], ONE), scr[l], VF(26)[l]);
        vi5 = ((vi5 | face | clipw) << 1) & 0xffff;
        for (int l = 0; l < 3; ++l) col[l] = vu_min(col[l], F255);
        col[3] = vu_min(col[3], F128);
        vi12 &= vi5;
        vu_f xyzf[4];
        int32_t ci[4];
        for (int l = 0; l < 3; ++l) xyzf[l] = (vu_f)vu_ftoi(scr[l], 4);
        xyzf[3] = (vu_f)(int32_t)(int16_t)((fogw | vi5) & 0xffff);
        for (int l = 0; l < 4; ++l) ci[l] = vu_ftoi(col[l], 0);
        st(r, o, stq, 14);
        st(r, o + 1, (vu_f*)ci, 15);
        st(r, o + 2, xyzf, 15);
        o += 3;
    }
    VI(5) = vi5;
    VI(9) = 0;
    vu1_ref_xgkick(r, vi12 ? vu1_ref_ilw(r, 999, 0) : vu1_ref_ilw(r, 1017, 0));
}

int vu1_ref_set2(Vu1Ref* r, const char** variant) {
    switch (r->pc) {
    case 0x05b0:
        if (variant) *variant = "set2_entry_skin_lit";
        set2_init_0050(r); VI(14) = 0x2f; VI(15) = 0x5f0 / 8;
        return VU1REF_OK;
    case 0x0188: {
        uint32_t loop = VI(15) * 8u;
        if (loop != 0x5f0) break;
        if (variant) *variant = "set2_skin_lit";
        if (!set2_header_0188(r)) return VU1REF_OK;
        skin_lit_loop(r);
        return VU1REF_OK;
    }
    }
    int rc = set2_spec(r, variant);
    if (rc == VU1REF_UNSUPPORTED) rc = set2_rigid(r, variant);
    if (rc == VU1REF_UNSUPPORTED) rc = set2_blend(r, variant);
    return rc;
}

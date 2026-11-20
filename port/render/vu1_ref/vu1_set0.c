/*
 * Microcode set 0 (.vudata MPG chain 0, 0x2458 bytes): s_nProgType 2, the
 * s_aUcodeAdr / s_aUcodeFaceAdr / s_aUcodeBackAdr family.  Rigid (one matrix
 * per part), unlit pre-coloured, textured triangle strips.  In the opening
 * cutscene this is the hangar / set geometry.
 *
 * A draw is   MSCAL entry          (constants -> registers, then stop in 0x160)
 *             { UNPACK batch to TOP ; MSCNT }*   (resume at 0x170: one batch,
 *                                                 one XGKICK, stop in 0x160)
 * The MSCNT resume point 0x170 is shared; which vertex loop it returns to is
 * vi15, set by the entry.  References here:
 *   entry 0x0bd8 (Adr[0])      -> loop 0x0c18 "strip"        plain strip
 *   entry 0x0f90 (FaceAdr[0])  -> loop 0x0fd0 "strip_face"   + per-vertex facing ADC
 *   entry 0x12d8 (BackAdr[0])  -> loop 0x0fd0 with the facing table swapped
 * The lit loops 0x1458 / 0x1fa0 (resume 0x05e8) are in vu1_set0_lit.c; 0x16f8 / 0x1b08
 * and the 0x0de0 / 0x11e0 forms have no reference yet (VU1REF_UNSUPPORTED).
 *
 * Constants (data memory qw; EE builder in nml_packet_add.c):
 *   962-965 vf28-31  object -> clip matrix (nmlPacketAddParts, 0x3C2:
 *                    camera->viewMatrix [x model+0x180] [x model+0x80] x part local)
 *   966/967 vf26/27  screen scale / offset (nmlPacketAddScreen 0x3C6/0x3C7)
 *   969     vf24     vertex colour scale (after the entry; header type B may reload)
 *   972              UV offset .xy (texture scroll)
 *   976     vf8      eye position in object space (nmlPacketAddParts 0x3CD row 3)
 *   1007             fog: x min, y max, z bias, w scale (fog = clamp(z + w/W))
 *   1011             GIF tag template (PRE, PRIM, NREG 3: ST RGBAQ XYZF2)
 *   999/1000.w       ADC words for "clipped" (0 / 0x7000)
 *   1013-1016.w      ADC words for the facing test (set by the entry)
 *   1017             x: packet address, y: TOP of the current batch
 */
#include "vu1_ref.h"
#include "vu_float.h"

#include <string.h>

typedef struct { vu_f v[4]; } Q4;

void set0_init_0050(Vu1Ref* r);
int set0_lit(Vu1Ref* r, const char** variant);   /* vu1_set0_lit.c: 0x1418 0x1f60 (+Face/Back), resume 0x05e8 */

static Q4 ld(Vu1Ref* r, uint32_t q) { Q4 o; memcpy(o.v, vu1_ref_qw(r, q), 16); return o; }
static void st(Vu1Ref* r, uint32_t q, const vu_f* v, unsigned mask /* x8 y4 z2 w1 */) {
    uint8_t* p = vu1_ref_qw(r, q);
    for (int l = 0; l < 4; ++l)
        if (mask >> (3 - l) & 1) memcpy(p + 4 * l, &v[l], 4);
}
#define VF(n) (r->regs.vf[n])
#define VI(n) (r->regs.vi[n])
static const vu_f ONE = 0x3f800000u;
static const vu_f F255 = 0x437f0000u;

/* 0x0050: the entry prologue every set-0 entry calls (jalr vi15, 0xa*8). */
void set0_init_0050(Vu1Ref* r) {
    static const struct { int vf, q; } ld_tab[] = {
        {8, 976}, {12, 732}, {13, 733}, {14, 734}, {15, 735}, {16, 970}, {17, 973}, {18, 974},
        {19, 975}, {20, 977}, {21, 978}, {22, 979}, {23, 980}, {24, 971}, {25, 968}, {26, 966},
        {27, 967}, {28, 962}, {29, 963}, {30, 964}, {31, 965},
    };
    for (unsigned i = 0; i < sizeof ld_tab / sizeof ld_tab[0]; ++i) memcpy(VF(ld_tab[i].vf), vu1_ref_qw(r, ld_tab[i].q), 16);
    for (int l = 0; l < 4; ++l) VF(25)[l] = vu_mul(VF(25)[l], VF(24)[l]);   /* ambient x scale */
    memcpy(VF(24), vu1_ref_qw(r, 969), 16);
    vu1_ref_isw(r, 999, 3, 0);
    vu1_ref_isw(r, 1000, 3, 0x7000);
    vu1_ref_isw(r, 1013, 3, 0x4000);
    vu1_ref_isw(r, 1016, 3, 0x4000);
    vu1_ref_isw(r, 1015, 3, 0);
    vu1_ref_isw(r, 1014, 3, 0);
    vu1_ref_isw(r, 995, 1, 0);
}

/*
 * 0x0170: batch header (MSCNT resume point of the strip loops).
 *   TOP+0  x: vertex count n | 0x8000 EOP (copied into the GIF tag NLOOP/EOP)
 *          y: 0x4000 = "register batch" (no vertices, see below)
 *          z: 0x4000 = register batch also reloads fog/colour constants
 *          w: morph target count (0 = rigid)
 *   geometry batch, n vertices, four streams of n qw from TOP+1:
 *     P  TOP+1      position xyz (float), w = S
 *     T  TOP+1+n    w = T (xyz unused by these loops)
 *     C  TOP+1+2n   colour RGBA, integers (UNPACK V4-8/V4-16 unsigned); converted
 *                   to float in place here (ITOF0)
 *     N  TOP+1+3n   normal xyz (facing test of loop 0xfd0)
 *     morph keys from TOP+1+6n (w != 0): pairs of xyz, interleaved per vertex
 *   output GIF packet at TOP+0xf3.
 * Returns 0 for a register batch (already kicked), 1 when vertices follow.
 */
static int header_0170(Vu1Ref* r) {
    const uint32_t top = r->top;
    VI(13) = top;
    uint32_t hy = vu1_ref_ilw(r, top, 1), hz = vu1_ref_ilw(r, top, 2);
    uint32_t w967 = vu1_ref_ilw(r, 967, 3);
    if (hy == 0x4000) {
        /* register batch: 3 qw (a GIF tag + 2 A+D) kicked from TOP+0xf3 */
        Q4 a = ld(r, top + 1), b = ld(r, top + 2), c = ld(r, top + 3), d = ld(r, top + 4);
        memcpy(VF(7), vu1_ref_qw(r, 971), 16);   /* delay-slot load, always */
        if (hz == 0x4000) {
            Q4 c971 = ld(r, 971), x8 = ld(r, top + 8), x7 = ld(r, top + 7), x5 = ld(r, top + 5), x6 = ld(r, top + 6);
            st(r, 1008, x8.v, 8);
            st(r, 970, x7.v, 14);
            for (int l = 0; l < 3; ++l) VF(25)[l] = vu_mul(x5.v[l], c971.v[l]);
            VF(25)[3] = vu_mul(VF(25)[3], c971.v[3]);
            memcpy(VF(24), x6.v, 16);
            VF(5)[0] = x8.v[0];
            memcpy(VF(6), x7.v, 12);
        }
        uint32_t o = top + 0xf3;
        st(r, o, a.v, 15);
        st(r, o + 1, b.v, 15);
        st(r, 985, b.v, 15);
        st(r, 983, c.v, 15);
        if (w967 == 0) st(r, o + 2, c.v, 15);
        else { st(r, o + 2, d.v, 15); st(r, 983, d.v, 15); }
        memcpy(VF(1), a.v, 16); memcpy(VF(2), b.v, 16); memcpy(VF(3), c.v, 16); memcpy(VF(4), d.v, 16);
        VI(12) = o & 0xffff;
        vu1_ref_xgkick(r, o);
        return 0;
    }
    vu1_ref_isw(r, 1017, 1, top);
    uint32_t out = (top + 0xf3) & 0xffff;
    vu1_ref_isw(r, 1017, 0, out);
    uint32_t hx = vu1_ref_ilw(r, top, 0);
    uint32_t n = hx & 0x7fff;
    uint32_t p = (top + 1) & 0xffff, t = (p + n) & 0xffff, c = (t + n) & 0xffff, nrm = (c + n) & 0xffff;
    Q4 tag = ld(r, 1011);
    st(r, out, tag.v, 15);
    vu1_ref_isw(r, out, 0, hx);
    memcpy(VF(24), vu1_ref_qw(r, 971), 16);
    /* colours: integers -> floats in place */
    for (uint32_t i = 0; i < n; ++i) {
        Q4 v = ld(r, c + i);
        for (int l = 0; l < 4; ++l) v.v[l] = vu_itof((int32_t)v.v[l], 0);
        st(r, c + i, v.v, 15);
    }
    /* morph: lerp key[idx] -> key[idx+1] by the fraction of weight mem[734].w */
    uint32_t keys = vu1_ref_ilw(r, top, 3);
    if (keys) {
        vu_f wgt = vu_min(((vu_f*)vu1_ref_qw(r, 734))[3], vu_itof((int16_t)keys, 0));
        int32_t idx = vu_ftoi(wgt, 0);
        vu_f frac = vu_sub(wgt, vu_itof(idx, 0));
        vu_f inv = vu_sub(ONE, frac);
        uint32_t stride = keys + 1;
        uint32_t src = (nrm + 3 * n + (uint32_t)(idx & 0xffff)) & 0xffff;
        uint32_t dst = p, cnt = 2 * n;
        if (vu1_ref_ilw(r, 1008, 3) == 2) { dst = c; cnt = n; }
        for (uint32_t i = 0; i < cnt; ++i) {
            Q4 a = ld(r, src), b = ld(r, src + 1);
            vu_f o[4];
            for (int l = 0; l < 3; ++l) o[l] = vu_madd(vu_mul(a.v[l], inv), b.v[l], frac);
            st(r, dst + i, o, 14);
            src = (src + stride) & 0xffff;
        }
    }
    VI(2) = p; VI(3) = t; VI(4) = c; VI(10) = nrm; VI(8) = (out + 1) & 0xffff; VI(9) = n; VI(5) = 0;
    return 1;
}

/* XYZF2 w word: fog (ftoi4, bits 4..11) | the ADC shift register (bit 15 = ADC).
   MFIR sign-extends the 16-bit result. */
static vu_f xyzf_w(int32_t fog4, uint32_t vi5) {
    uint32_t v = (((uint32_t)fog4 & 0xffff) & 0xff0) | vi5;
    return (vu_f)(int32_t)(int16_t)(v & 0xffff);
}

/*
 * The per-vertex work of loops 0x0c18 / 0x0fd0 (software-pipelined in the
 * microcode; the order of the float operations is kept so results are bit-exact):
 *   clip   = M(962) * (P.xyz, 1)
 *   q      = 1 / clip.w
 *   STQ    = ((0 + P.w) + uv.x, (0 + T.w) + uv.y, 1) * q
 *   RGBAQ  = ftoi0(min(C * vf24, 255))
 *   fog    = clamp(fog.z + fog.w * q, fog.x, fog.y)
 *   XYZ    = ftoi4(scrOff + (clip * q) * scrScale)
 *   ADC    = vi5 = ((vi5 | clipped ? 0x7000 : 0 | facing word) << 1)  -> bit 15
 * A clipped vertex sets ADC on itself and the next two vertices (0x7000 << 1,2,3),
 * which drops every strip triangle that touches it: the program has no clipper,
 * only guard-band rejection.  The facing word (loop 0x0fd0) is 0x4000 or 0 from
 * mem[1013 + 2*(dot(eye - P, N) < 0)].w, so it drops one vertex's triangle.
 */
static void strip_loop(Vu1Ref* r, int facing) {
    uint32_t n = VI(9);
    uint32_t p = VI(2), t = VI(3), c = VI(4), nm = VI(10), o = VI(8);
    uint32_t vi5 = VI(5);
    const vu_f* fog = (const vu_f*)vu1_ref_qw(r, 1007);
    vu_f fogv[4];
    memcpy(fogv, fog, 16);
    vu_f uv[4];
    memcpy(uv, vu1_ref_qw(r, 972), 16);
    vu_f eye[4];
    memcpy(eye, VF(8), 16);
    if (n == 0) n = 0x10000;
    /* Four lanes at a time (vu_float.h *4): the scalar steps, lane for lane. */
    const vu_v4 m28 = vu_ld4(VF(28)), m29 = vu_ld4(VF(29)), m30 = vu_ld4(VF(30)), m31 = vu_ld4(VF(31));
    const vu_v4 v24 = vu_ld4(VF(24)), v26 = vu_ld4(VF(26)), v27 = vu_ld4(VF(27));
    const vu_v4 one4 = vu_bc4(ONE), f2554 = vu_bc4(F255);
    const vu_v4 acc27 = vu_mul4(v27, one4);
    for (uint32_t i = 0; i < n; ++i) {
        Q4 P = ld(r, p + i), T = ld(r, t + i);
        /* colour */
        const vu_v4 coli = vu_ftoi4(vu_min4(vu_mul4(vu_ld4(vu1_ref_qw(r, c + i)), v24), f2554), 0);
        /* STQ source */
        vu_f s4[4];
        s4[2] = vu_add(0, ONE);
        s4[1] = vu_add(0, T.v[3]);
        s4[0] = vu_add(0, P.v[3]);
        s4[0] = vu_add(s4[0], uv[0]);
        s4[1] = vu_add(s4[1], uv[1]);
        s4[3] = 0;
        /* transform */
        vu_v4 acc = vu_mul4(m28, vu_bc4(P.v[0]));
        acc = vu_madd4(acc, m29, vu_bc4(P.v[1]));
        acc = vu_madd4(acc, m30, vu_bc4(P.v[2]));
        const vu_v4 clip4 = vu_madd4(acc, m31, one4);
        vu_f clip[4];
        vu_st4(clip, clip4);
        uint32_t cj = vu_clip(clip);
        vu_f q = vu_div(ONE, clip[3]);
        uint32_t face = 0;
        if (facing) {
            Q4 N = ld(r, nm + i);
            vu_f d[3];
            for (int l = 0; l < 3; ++l) d[l] = vu_mul(vu_sub(eye[l], P.v[l]), N.v[l]);
            vu_f x = vu_add(d[0], d[2]);
            x = vu_add(x, d[1]);
            face = vu1_ref_ilw(r, 1013 + ((x & 0x80000000u) ? 2 : 0), 3);
        }
        uint32_t clipw = vu1_ref_ilw(r, 999 + (cj ? 1 : 0), 3);
        vu_f fogf;
        if (facing) {
            fogf = vu_mul(fogv[3], q);
            fogf = vu_add(fogf, fogv[2]);
        } else {
            vu_f acc1 = vu_mul(vu_mul(ONE, fogv[2]), ONE);   /* mulz.w vf3 = vf0*vf9z ; mula.w acc = vf3*vf0 */
            fogf = vu_madd(acc1, fogv[3], q);
        }
        fogf = vu_max(fogf, fogv[0]);
        fogf = vu_min(fogf, fogv[1]);
        /* move.w vf1, vf0 ; scr = (vf27 * 1) + (clip * q) * vf26 */
        clip[3] = ONE;
        const vu_v4 q4 = vu_bc4(q);
        const vu_v4 scr4 = vu_madd4(acc27, vu_mul4(vu_ld4(clip), q4), v26);
        const vu_v4 stq4 = vu_mul4(vu_ld4(s4), q4);
        vi5 = ((vi5 | face | clipw) << 1) & 0xffff;
        vu_f xyzf[4];
        vu_st4(xyzf, vu_ftoi4(scr4, 4));
        xyzf[3] = xyzf_w(vu_ftoi(fogf, 4), vi5);
        vu_f stq[4], colv[4];
        vu_st4(stq, stq4);
        vu_st4(colv, coli);
        st(r, o, stq, 14);
        st(r, o + 1, colv, 15);
        st(r, o + 2, xyzf, 15);
        o += 3;
    }
    VI(5) = vi5;
    VI(9) = 0;
    vu1_ref_xgkick(r, vu1_ref_ilw(r, 1017, 0));
}

int vu1_ref_set0(Vu1Ref* r, const char** variant) {
    switch (r->pc) {
    case 0x0bd8:   /* entries: constants, then stop at 0x160 until the first batch */
        if (variant) *variant = "set0_entry_strip";
        set0_init_0050(r); VI(14) = 0x2c; VI(15) = 0xc18 / 8;
        return VU1REF_OK;
    case 0x0f90:
        if (variant) *variant = "set0_entry_strip_face";
        set0_init_0050(r); VI(14) = 0x2c; VI(15) = 0xfd0 / 8;
        return VU1REF_OK;
    case 0x12d8:
        if (variant) *variant = "set0_entry_strip_back";
        set0_init_0050(r);
        vu1_ref_isw(r, 1013, 3, 0);
        vu1_ref_isw(r, 1015, 3, 0x4000);
        VI(2) = 0x4000; VI(14) = 0x2c; VI(15) = 0xfd0 / 8;
        return VU1REF_OK;
    case 0x0170: {
        uint32_t loop = VI(15) * 8u;
        if (loop != 0xc18 && loop != 0xfd0) return VU1REF_UNSUPPORTED;
        if (variant) *variant = loop == 0xc18 ? "set0_strip" : "set0_strip_face";
        if (!header_0170(r)) return VU1REF_OK;
        strip_loop(r, loop == 0xfd0);
        return VU1REF_OK;
    }
    }
    return set0_lit(r, variant);
}

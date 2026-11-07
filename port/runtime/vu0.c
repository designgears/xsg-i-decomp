/*
 * Stage 7: host VU0 model (see vu0.h).  Built with SSE float maths
 * (-msse2 -mfpmath=sse) so single-precision results are not x87-extended.
 *
 * PS2 float handling as in vu1.cpp: inputs with exponent 255 read as
 * +-FLT_MAX, denormals as +-0; results clamp to +-FLT_MAX (NaN -> +MAX) and
 * flush denormals.  Rounding is the host's round-to-nearest (the VU truncates;
 * results can differ in the last bit).  Macro mode has no pipeline: Q is
 * written at once, vwaitq is a no-op.
 */
#include "vu0.h"
#include "xsg_settings.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xsg_rt.h"

XsgVu0 xsg_vu0 = { .vf = { [0] = { .f = { 0.0f, 0.0f, 0.0f, 1.0f } } }, .R = 0x3f800000u };
XsgGpr xsg_ee_gpr[32];
XsgFpr xsg_ee_fpr[32];
int xsg_ee_fcc;
const XsgGpr xsg_ee_zero;
XsgGpr xsg_ee_sink;
unsigned long long xsg_vu0_blocks;

#define V xsg_vu0

static inline float in_f(uint32_t b)
{
    if ((b & 0x7f800000u) == 0x7f800000u)
        b = (b & 0x80000000u) | 0x7f7fffffu;
    else if ((b & 0x7f800000u) == 0)
        b &= 0x80000000u;
    float f;
    memcpy(&f, &b, 4);
    return f;
}

/* Result clamp; accumulates MAC bits for lane l. */
static inline uint32_t out_f(float f, int l, uint32_t *mac)
{
    uint32_t b;
    memcpy(&b, &f, 4);
    uint32_t e = b & 0x7f800000u;
    int bit = 3 - l;
    if (e == 0x7f800000u) {
        if ((b & 0x7fffffu) && !(b & 0x80000000u))
            b = 0x7f7fffffu;
        else
            b = (b & 0x80000000u) | 0x7f7fffffu;
        if (mac) *mac |= 1u << (12 + bit);
    } else if (e == 0 && (b & 0x7fffffu)) {
        b &= 0x80000000u;
        if (mac) *mac |= 1u << (8 + bit);
    }
    if (mac) {
        if (!(b & 0x7fffffffu)) *mac |= 1u << bit;
        if (b & 0x80000000u) *mac |= 1u << (4 + bit);
    }
    return b;
}

static inline int lane_on(int dest, int l) { return (dest >> (3 - l)) & 1; }

static void set_mac(uint32_t mac)
{
    uint32_t st = 0;
    if (mac & 0x000f) st |= 1;
    if (mac & 0x00f0) st |= 2;
    if (mac & 0x0f00) st |= 4;
    if (mac & 0xf000) st |= 8;
    V.mac = mac;
    V.status = (V.status & 0xff0u) | st | (st << 6);
}

static inline void fix_vf0(void)
{
    V.vf[0].u[0] = V.vf[0].u[1] = V.vf[0].u[2] = 0;
    V.vf[0].u[3] = 0x3f800000u;
}

void vu0_arith(int op, int dest, int fd, int fs, int ft, int bc)
{
    float b[4];
    int l;
    uint32_t mac = 0, res[4];
    const XsgVf *A = &V.vf[fs];
    if (bc == VU0_VEC)
        for (l = 0; l < 4; l++) b[l] = in_f(V.vf[ft].u[l]);
    else {
        uint32_t s = bc == VU0_BQ ? V.Q : bc == VU0_BI ? V.I : V.vf[ft].u[bc];
        b[0] = b[1] = b[2] = b[3] = in_f(s);
    }
    for (l = 0; l < 4; l++) {
        if (!lane_on(dest, l)) continue;
        float x = in_f(A->u[l]), y = b[l], z;
        switch (op) {
        case VU0_ADD: z = x + y; break;
        case VU0_SUB: z = x - y; break;
        case VU0_MUL: z = x * y; break;
        case VU0_MADD: z = in_f(V.acc.u[l]) + in_f(out_f(x * y, l, NULL)); break;
        case VU0_MSUB: z = in_f(V.acc.u[l]) - in_f(out_f(x * y, l, NULL)); break;
        case VU0_MAX: z = x > y ? x : y; break;
        default: z = x < y ? x : y; break;
        }
        res[l] = out_f(z, l, (op == VU0_MAX || op == VU0_MINI) ? NULL : &mac);
    }
    XsgVf *D = fd == VU0_ACC ? &V.acc : &V.vf[fd];
    for (l = 0; l < 4; l++)
        if (lane_on(dest, l)) D->u[l] = res[l];
    if (op != VU0_MAX && op != VU0_MINI) set_mac(mac);
    fix_vf0();
}

/* OPMULA.xyz ACC = fs.yzx * ft.zxy; OPMSUB.xyz fd = ACC - fs.yzx * ft.zxy */
static const int op_i1[3] = { 1, 2, 0 }, op_i2[3] = { 2, 0, 1 };

void vu0_opmula(int dest, int fs, int ft)
{
    uint32_t mac = 0, r[3];
    int l;
    for (l = 0; l < 3; l++)
        r[l] = out_f(in_f(V.vf[fs].u[op_i1[l]]) * in_f(V.vf[ft].u[op_i2[l]]), l, &mac);
    for (l = 0; l < 3; l++)
        if (lane_on(dest, l)) V.acc.u[l] = r[l];
    set_mac(mac);
}

void vu0_opmsub(int dest, int fd, int fs, int ft)
{
    uint32_t mac = 0, r[3];
    int l;
    for (l = 0; l < 3; l++) {
        float m = in_f(out_f(in_f(V.vf[fs].u[op_i1[l]]) * in_f(V.vf[ft].u[op_i2[l]]), l, NULL));
        r[l] = out_f(in_f(V.acc.u[l]) - m, l, &mac);
    }
    for (l = 0; l < 3; l++)
        if (lane_on(dest, l)) V.vf[fd].u[l] = r[l];
    set_mac(mac);
    fix_vf0();
}

void vu0_move(int dest, int ft, int fs)
{
    XsgVf t = V.vf[fs];
    for (int l = 0; l < 4; l++)
        if (lane_on(dest, l)) V.vf[ft].u[l] = t.u[l];
    fix_vf0();
}

void vu0_mr32(int dest, int ft, int fs)
{
    XsgVf t = V.vf[fs];
    for (int l = 0; l < 4; l++)
        if (lane_on(dest, l)) V.vf[ft].u[l] = t.u[(l + 1) & 3];
    fix_vf0();
}

void vu0_abs(int dest, int ft, int fs)
{
    XsgVf t = V.vf[fs];
    for (int l = 0; l < 4; l++)
        if (lane_on(dest, l)) V.vf[ft].u[l] = out_f(fabsf(in_f(t.u[l])), l, NULL);
    fix_vf0();
}

void vu0_itof(int shift, int dest, int ft, int fs)
{
    XsgVf t = V.vf[fs];
    float sc = 1.0f / (float)(1 << shift);
    for (int l = 0; l < 4; l++)
        if (lane_on(dest, l)) V.vf[ft].u[l] = out_f((float)t.i[l] * sc, l, NULL);
    fix_vf0();
}

void vu0_ftoi(int shift, int dest, int ft, int fs)
{
    XsgVf t = V.vf[fs];
    for (int l = 0; l < 4; l++) {
        if (!lane_on(dest, l)) continue;
        double v = (double)in_f(t.u[l]) * (double)(1 << shift);
        int32_t iv;
        if (v >= 2147483647.0) iv = 0x7fffffff;
        else if (v <= -2147483648.0) iv = (int32_t)0x80000000u;
        else iv = (int32_t)v;   /* truncate toward zero */
        V.vf[ft].i[l] = iv;
    }
    fix_vf0();
}

void vu0_clip(int fs, int ft)
{
    float lim = fabsf(in_f(V.vf[ft].u[3]));
    uint32_t j = 0;
    for (int l = 0; l < 3; l++) {
        float v = in_f(V.vf[fs].u[l]);
        if (v > lim) j |= 1u << (2 * l);
        if (v < -lim) j |= 2u << (2 * l);
    }
    V.clip = ((V.clip << 6) | j) & 0xffffff;
}

static void div_flags(int inv, int dz)
{
    uint32_t b = (inv ? 0x10u : 0) | (dz ? 0x20u : 0);
    V.status = (V.status & ~0x30u) | b | (b << 6);
}

void vu0_div(int fs, int fsf, int ft, int ftf)
{
    float a = in_f(V.vf[fs].u[fsf]), b = in_f(V.vf[ft].u[ftf]);
    if (b == 0.0f) {
        div_flags(a == 0.0f, a != 0.0f);
        V.Q = ((V.vf[fs].u[fsf] ^ V.vf[ft].u[ftf]) & 0x80000000u) | 0x7f7fffffu;
    } else {
        div_flags(0, 0);
        V.Q = out_f(a / b, 0, NULL);
    }
}

void vu0_sqrt(int ft, int ftf)
{
    float b = in_f(V.vf[ft].u[ftf]);
    div_flags(b < 0, 0);
    V.Q = out_f(sqrtf(fabsf(b)), 0, NULL);
}

void vu0_rsqrt(int fs, int fsf, int ft, int ftf)
{
    float a = in_f(V.vf[fs].u[fsf]), b = in_f(V.vf[ft].u[ftf]);
    if (b == 0.0f) {
        div_flags(a == 0.0f, a != 0.0f);
        V.Q = (V.vf[fs].u[fsf] & 0x80000000u) | 0x7f7fffffu;
    } else {
        div_flags(b < 0, 0);
        V.Q = out_f(a / sqrtf(fabsf(b)), 0, NULL);
    }
}

void vu0_waitq(void) {}

static inline void set_vi(int r, uint32_t v) { if (r & 15) V.vi[r & 15] = v & 0xffff; }

void vu0_mfir(int dest, int ft, int is)
{
    for (int l = 0; l < 4; l++)
        if (lane_on(dest, l)) V.vf[ft].i[l] = (int16_t)V.vi[is];
    fix_vf0();
}
void vu0_mtir(int it, int fs, int fsf) { set_vi(it, V.vf[fs].u[fsf]); }
void vu0_iadd(int id, int is, int it) { set_vi(id, V.vi[is] + V.vi[it]); }
void vu0_isub(int id, int is, int it) { set_vi(id, V.vi[is] - V.vi[it]); }
void vu0_iaddi(int it, int is, int imm) { set_vi(it, V.vi[is] + (uint32_t)imm); }
void vu0_iand(int id, int is, int it) { set_vi(id, V.vi[is] & V.vi[it]); }
void vu0_ior(int id, int is, int it) { set_vi(id, V.vi[is] | V.vi[it]); }

/* VU0 data memory: 4 KB at PS2_VU_BASE + 0x4000 */
static inline uint8_t *vu0_qaddr(uint32_t q) { return (uint8_t *)(uintptr_t)(PS2_VU_BASE + 0x4000 + ((q & 0xff) << 4)); }
static void ld_q(int dest, int ft, uint32_t q)
{
    const uint8_t *p = vu0_qaddr(q);
    for (int l = 0; l < 4; l++)
        if (lane_on(dest, l)) memcpy(&V.vf[ft].u[l], p + 4 * l, 4);
    fix_vf0();
}
static void st_q(int dest, int fs, uint32_t q)
{
    uint8_t *p = vu0_qaddr(q);
    for (int l = 0; l < 4; l++)
        if (lane_on(dest, l)) memcpy(p + 4 * l, &V.vf[fs].u[l], 4);
}
void vu0_lqi(int dest, int ft, int is) { ld_q(dest, ft, V.vi[is]); set_vi(is, V.vi[is] + 1); }
void vu0_sqi(int dest, int fs, int it) { st_q(dest, fs, V.vi[it]); set_vi(it, V.vi[it] + 1); }
void vu0_lqd(int dest, int ft, int is) { set_vi(is, V.vi[is] - 1); ld_q(dest, ft, V.vi[is]); }
void vu0_sqd(int dest, int fs, int it) { set_vi(it, V.vi[it] - 1); st_q(dest, fs, V.vi[it]); }
void vu0_ilwr(int dest, int it, int is)
{
    const uint8_t *p = vu0_qaddr(V.vi[is]);
    for (int l = 0; l < 4; l++)
        if (lane_on(dest, l)) { uint32_t v; memcpy(&v, p + 4 * l, 4); set_vi(it, v); break; }
}
void vu0_iswr(int dest, int it, int is)
{
    uint8_t *p = vu0_qaddr(V.vi[is]);
    uint32_t v = V.vi[it];
    for (int l = 0; l < 4; l++)
        if (lane_on(dest, l)) memcpy(p + 4 * l, &v, 4);
}

void vu0_rinit(int fs, int fsf) { V.R = 0x3f800000u | (V.vf[fs].u[fsf] & 0x7fffff); }
void vu0_rxor(int fs, int fsf) { V.R = 0x3f800000u | ((V.R ^ V.vf[fs].u[fsf]) & 0x7fffff); }
void vu0_rget(int dest, int ft)
{
    for (int l = 0; l < 4; l++)
        if (lane_on(dest, l)) V.vf[ft].u[l] = V.R;
    fix_vf0();
}
void vu0_rnext(int dest, int ft)
{
    uint32_t x = (V.R >> 4) & 1, y = (V.R >> 22) & 1;
    V.R = (((V.R << 1) ^ x ^ y) & 0x7fffff) | 0x3f800000u;
    vu0_rget(dest, ft);
}

/* ---- COP2 transfers ---- */
void vu0_lqc2(int ft, const void *ea) { if (ft) memcpy(&V.vf[ft], ea, 16); }
void vu0_sqc2(int fs, void *ea) { memcpy(ea, &V.vf[fs], 16); }
void vu0_qmtc2(int ft, const XsgGpr *rt) { if (ft) memcpy(&V.vf[ft], rt, 16); }
void vu0_qmfc2(XsgGpr *rt, int fs) { memcpy(rt, &V.vf[fs], 16); }

void vu0_ctc2(int reg, const XsgGpr *rt)
{
    uint32_t v = rt->w[0];
    if (reg < 16) { set_vi(reg, v); return; }
    switch (reg) {
    case 16: V.status = (V.status & 0x3f) | (v & 0xfc0); break;
    case 18: V.clip = v & 0xffffff; break;
    case 20: V.R = 0x3f800000u | (v & 0x7fffff); break;
    case 21: V.I = v; break;
    case 22: V.Q = v; break;
    case 27: V.cmsar0 = v & 0xffff; break;
    case 28: V.fbrst = v; break;
    default: break;
    }
}

void vu0_cfc2(XsgGpr *rt, int reg)
{
    uint32_t v = 0;
    if (reg < 16) v = V.vi[reg];
    else switch (reg) {
    case 16: v = V.status; break;
    case 17: v = V.mac; break;
    case 18: v = V.clip; break;
    case 20: v = V.R; break;
    case 21: v = V.I; break;
    case 22: v = V.Q; break;
    case 27: v = V.cmsar0; break;
    case 28: v = V.fbrst; break;
    default: break;
    }
    xsg_ee_set32(rt, v);
}

/* ---- micro mode: the resident VU0 program (Vu0MicroCode.dvp) ---- */
extern const unsigned char xsg_vu0_microcode[];
extern const unsigned int xsg_vu0_microcode_size;
unsigned long long xsg_vu1_run_vu0(unsigned int pc);   /* vu1.cpp */

static int micro_loaded;
static unsigned long long callms_count;

static void load_micro(void)
{
    /* The game uploads this over VIF0 DMA (xglGeometryInit), which the host
       does not run; it is the same bytes, and nothing overwrites it. */
    memcpy((void *)(uintptr_t)PS2_VU_BASE, xsg_vu0_microcode, xsg_vu0_microcode_size);
    micro_loaded = 1;
}

void vu0_callms(unsigned int byte_addr)
{
    if (!micro_loaded) load_micro();
    callms_count++;
    xsg_vu1_run_vu0(byte_addr);
}

void vu0_callmsr(void) { vu0_callms(V.cmsar0 * 8u); }

/* ---- EE ---- */
void xsg_ee_set32(XsgGpr *d, unsigned int v) { d->sd[0] = (int32_t)v; }

#define MMI(n) void xsg_mmi_##n(XsgGpr *d, const XsgGpr *s, const XsgGpr *t)
MMI(pcpyld) { XsgGpr r; r.d[0] = t->d[0]; r.d[1] = s->d[0]; *d = r; }
MMI(pcpyud) { XsgGpr r; r.d[0] = s->d[1]; r.d[1] = t->d[1]; *d = r; }
MMI(pextlw) { XsgGpr r; r.w[0] = t->w[0]; r.w[1] = s->w[0]; r.w[2] = t->w[1]; r.w[3] = s->w[1]; *d = r; }
MMI(pextuw) { XsgGpr r; r.w[0] = t->w[2]; r.w[1] = s->w[2]; r.w[2] = t->w[3]; r.w[3] = s->w[3]; *d = r; }
MMI(pexew) { XsgGpr r = *t; r.w[0] = t->w[2]; r.w[2] = t->w[0]; *d = r; (void)s; }
MMI(pxor) { d->d[0] = s->d[0] ^ t->d[0]; d->d[1] = s->d[1] ^ t->d[1]; }
MMI(por) { d->d[0] = s->d[0] | t->d[0]; d->d[1] = s->d[1] | t->d[1]; }
MMI(pand) { d->d[0] = s->d[0] & t->d[0]; d->d[1] = s->d[1] & t->d[1]; }
MMI(pnor) { d->d[0] = ~(s->d[0] | t->d[0]); d->d[1] = ~(s->d[1] | t->d[1]); }
MMI(paddw) { XsgGpr r; for (int i = 0; i < 4; i++) r.w[i] = s->w[i] + t->w[i]; *d = r; }
MMI(psubw) { XsgGpr r; for (int i = 0; i < 4; i++) r.w[i] = s->w[i] - t->w[i]; *d = r; }
MMI(paddh) { XsgGpr r; for (int i = 0; i < 8; i++) r.h[i] = s->h[i] + t->h[i]; *d = r; }
MMI(psubh) { XsgGpr r; for (int i = 0; i < 8; i++) r.h[i] = s->h[i] - t->h[i]; *d = r; }
MMI(pcgth) { XsgGpr r; for (int i = 0; i < 8; i++) r.h[i] = s->sh[i] > t->sh[i] ? 0xffff : 0; *d = r; }
MMI(pextlh) { XsgGpr r; for (int i = 0; i < 4; i++) { r.h[2 * i] = t->h[i]; r.h[2 * i + 1] = s->h[i]; } *d = r; }
MMI(pextlb) { XsgGpr r; for (int i = 0; i < 8; i++) { r.b[2 * i] = t->b[i]; r.b[2 * i + 1] = s->b[i]; } *d = r; }
MMI(pmaxw) { XsgGpr r; for (int i = 0; i < 4; i++) r.sw[i] = s->sw[i] > t->sw[i] ? s->sw[i] : t->sw[i]; *d = r; }
MMI(pminw) { XsgGpr r; for (int i = 0; i < 4; i++) r.sw[i] = s->sw[i] < t->sw[i] ? s->sw[i] : t->sw[i]; *d = r; }
MMI(ppach) { XsgGpr r; for (int i = 0; i < 4; i++) { r.h[i] = t->h[2 * i]; r.h[4 + i] = s->h[2 * i]; } *d = r; }
MMI(ppacb) { XsgGpr r; for (int i = 0; i < 8; i++) { r.b[i] = t->b[2 * i]; r.b[8 + i] = s->b[2 * i]; } *d = r; }
MMI(pcpyh) { XsgGpr r; for (int i = 0; i < 4; i++) { r.h[i] = t->h[0]; r.h[4 + i] = t->h[4]; } *d = r; (void)s; }

static void __attribute__((destructor)) vu0_stats(void)
{
    const char *e = xsg_cfg("debug.vu0_stats");
    if (e && atoi(e))
        fprintf(stderr, "[vu0] translated asm blocks run: %llu, vcallms: %llu\n", xsg_vu0_blocks, callms_count);
}

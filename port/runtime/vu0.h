/*
 * Stage 7: host model of the EE's VU0 (macro mode + vcallms micro mode) and of
 * the EE registers the game's inline asm names directly.
 *
 * The game's PS2_ASMV/PS2_ASM blocks are translated (port/scripts/
 * vu0_translate.py) into calls on this model under `#ifdef PORT_HOST`.
 * All state is global and persists across asm blocks and functions, exactly
 * as the hardware registers do: one block may load vf28..vf31 and a later one
 * (or a vcallmsr microprogram) consume them.
 *
 * Plain C (gnu89-compatible); included from include/port/ps2_asm.h on the
 * host, so every game TU that has asm sees it.
 */
#ifndef XSG_VU0_H
#define XSG_VU0_H

#ifdef __cplusplus
extern "C" {
#endif

typedef union XsgVf {
    unsigned int u[4];
    float f[4];
    int i[4];
} XsgVf;

/* A 128-bit EE general register (MMI view). */
typedef union XsgGpr {
    unsigned long long d[2];
    long long sd[2];
    unsigned int w[4];
    int sw[4];
    unsigned short h[8];
    short sh[8];
    unsigned char b[16];
} XsgGpr;

typedef union XsgFpr {
    float f;
    unsigned int u;
    int i;
} XsgFpr;

typedef struct XsgVu0 {
    XsgVf vf[32];               /* vf0 = (0,0,0,1), kept constant */
    XsgVf acc;
    unsigned int vi[16];        /* 16-bit integer regs (stored widened) */
    unsigned int I, Q, P, R;
    unsigned int status, mac, clip;
    unsigned int cmsar0, fbrst;
} XsgVu0;

extern XsgVu0 xsg_vu0;
extern XsgGpr xsg_ee_gpr[32];
extern XsgFpr xsg_ee_fpr[32];
extern int xsg_ee_fcc;          /* COP1 condition bit */
extern const XsgGpr xsg_ee_zero;
extern XsgGpr xsg_ee_sink;        /* writes to $0 */

/* dest masks, PS2 bit order */
#define VU0_X 8
#define VU0_Y 4
#define VU0_Z 2
#define VU0_W 1

/* arithmetic ops */
enum { VU0_ADD, VU0_SUB, VU0_MUL, VU0_MADD, VU0_MSUB, VU0_MAX, VU0_MINI };
/* broadcast selector: -1 vector, 0..3 = ft.x..w, 4 = Q, 5 = I */
#define VU0_VEC (-1)
#define VU0_BQ 4
#define VU0_BI 5
#define VU0_ACC (-1)            /* fd = ACC */

/* --- VU0 macro instructions ------------------------------------------- */
void vu0_arith(int op, int dest, int fd, int fs, int ft, int bc);
void vu0_opmula(int dest, int fs, int ft);
void vu0_opmsub(int dest, int fd, int fs, int ft);
void vu0_move(int dest, int ft, int fs);
void vu0_mr32(int dest, int ft, int fs);
void vu0_abs(int dest, int ft, int fs);
void vu0_itof(int shift, int dest, int ft, int fs);   /* shift 0/4/12/15 */
void vu0_ftoi(int shift, int dest, int ft, int fs);
void vu0_clip(int fs, int ft);                         /* vclipw.xyz fs, ft.w */
void vu0_div(int fs, int fsf, int ft, int ftf);
void vu0_sqrt(int ft, int ftf);
void vu0_rsqrt(int fs, int fsf, int ft, int ftf);
void vu0_mfir(int dest, int ft, int is);
void vu0_mtir(int it, int fs, int fsf);
void vu0_iadd(int id, int is, int it);
void vu0_isub(int id, int is, int it);
void vu0_iaddi(int it, int is, int imm);
void vu0_iand(int id, int is, int it);
void vu0_ior(int id, int is, int it);
void vu0_lqi(int dest, int ft, int is);
void vu0_sqi(int dest, int fs, int it);
void vu0_lqd(int dest, int ft, int is);
void vu0_sqd(int dest, int fs, int it);
void vu0_ilwr(int dest, int it, int is);
void vu0_iswr(int dest, int it, int is);
void vu0_rinit(int fs, int fsf);
void vu0_rxor(int fs, int fsf);
void vu0_rget(int dest, int ft);
void vu0_rnext(int dest, int ft);
void vu0_waitq(void);

/* --- COP2 transfers ----------------------------------------------------- */
void vu0_lqc2(int ft, const void *ea);
void vu0_sqc2(int fs, void *ea);
void vu0_qmtc2(int ft, const XsgGpr *rt);
void vu0_qmfc2(XsgGpr *rt, int fs);
void vu0_ctc2(int reg, const XsgGpr *rt);
void vu0_cfc2(XsgGpr *rt, int reg);
void vu0_callms(unsigned int byte_addr);   /* vcallms imm (VU byte address) */
void vu0_callmsr(void);                    /* vcallmsr $vi27 (CMSAR0 pairs) */

/* --- EE helpers (MMI and friends) -------------------------------------- */
void xsg_ee_set32(XsgGpr *d, unsigned int v);   /* 32-bit result, sign-extended */
#define XSG_MMI_DECL(n) void xsg_mmi_##n(XsgGpr *d, const XsgGpr *s, const XsgGpr *t);
XSG_MMI_DECL(pcpyld) XSG_MMI_DECL(pcpyud) XSG_MMI_DECL(pextlw) XSG_MMI_DECL(pextuw)
XSG_MMI_DECL(pexew) XSG_MMI_DECL(pxor) XSG_MMI_DECL(por) XSG_MMI_DECL(pand) XSG_MMI_DECL(pnor)
XSG_MMI_DECL(paddw) XSG_MMI_DECL(psubw) XSG_MMI_DECL(pcgth) XSG_MMI_DECL(pextlh)
XSG_MMI_DECL(pmaxw) XSG_MMI_DECL(pminw) XSG_MMI_DECL(ppach) XSG_MMI_DECL(ppacb)
XSG_MMI_DECL(pcpyh) XSG_MMI_DECL(pextlb) XSG_MMI_DECL(paddh) XSG_MMI_DECL(psubh)

/* Operand marshalling for translated asm blocks. */
#define XSG_EA(r, off) ((void *)((char *)(unsigned long)(r).w[0] + (off)))
#define XSG_IN(x) ({ __auto_type xsg_t_ = (x); XsgGpr xsg_r_; \
    xsg_r_.d[0] = 0; xsg_r_.d[1] = 0; \
    __builtin_memcpy(&xsg_r_, &xsg_t_, sizeof xsg_t_); \
    if (sizeof xsg_t_ == 4) xsg_r_.d[0] = (unsigned long long)(long long)xsg_r_.sw[0]; \
    xsg_r_; })
#define XSG_OUT(x, r) do { __typeof__(x) xsg_t_; \
    __builtin_memcpy(&xsg_t_, &(r), sizeof xsg_t_); (x) = xsg_t_; } while (0)
#define XSG_FIN(x) ({ XsgFpr xsg_f_; xsg_f_.f = (float)(x); xsg_f_; })

/* Coverage counter for translated blocks (XSG_VU0_STATS=1 prints at exit). */
extern unsigned long long xsg_vu0_blocks;

#ifdef __cplusplus
}
#endif

#endif

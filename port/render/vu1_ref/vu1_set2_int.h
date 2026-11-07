/* Shared pieces of the set-2 references (vu1_set2*.c). */
#ifndef XSG_VU1_SET2_INT_H
#define XSG_VU1_SET2_INT_H
#include "vu1_ref.h"
#include "vu_float.h"
#include <string.h>

typedef struct { vu_f v[4]; } Q4;

static inline Q4 ld(Vu1Ref* r, uint32_t q) { Q4 o; memcpy(o.v, vu1_ref_qw(r, q), 16); return o; }
static inline void st(Vu1Ref* r, uint32_t q, const vu_f* v, unsigned mask) {
    uint8_t* p = vu1_ref_qw(r, q);
    for (int l = 0; l < 4; ++l)
        if (mask >> (3 - l) & 1) memcpy(p + 4 * l, &v[l], 4);
}
#define VF(n) (r->regs.vf[n])
#define VI(n) (r->regs.vi[n])
static const vu_f ONE = 0x3f800000u;
static const vu_f F255 = 0x437f0000u, F128 = 0x43000000u;


void set2_init_0050(Vu1Ref* r);                 /* entry constant loader (0x0050) */
int set2_header_0188(Vu1Ref* r);                /* batch header; 0 = register batch (done) */
uint32_t set2_facing_0440(Vu1Ref* r, vu_f pos[4]); /* mirror + facing word */
void set2_skin(Vu1Ref* r, const Q4* P, const Q4* N, const Q4* B, const Q4* W, vu_f pos[4], vu_f nrm[4]);

/* Per-file dispatch for the other loops; each returns VU1REF_UNSUPPORTED for a
   (pc, vi15) it does not own.  MSCNT batches arrive with pc 0x0188, loop = vi15*8. */
int set2_spec(Vu1Ref* r, const char** variant);   /* vu1_set2_spec.c:  0x08e0 0x0d60 0x1228 */
int set2_rigid(Vu1Ref* r, const char** variant);  /* vu1_set2_rigid.c: 0x1790 0x1a88 0x1ec0 0x2338 */
int set2_blend(Vu1Ref* r, const char** variant);  /* vu1_set2_blend.c: 0x2850 0x2be8 0x3058 0x3510 (+Face/Back) */

#endif

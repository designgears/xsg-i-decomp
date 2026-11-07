/*
 * C references of the Xenosaga VU1 microprograms (port/docs/VU1_PROGRAMS.md).
 *
 * Plain C, host-only.  A reference takes what one VU1 run sees at entry (the
 * register file and the 16 KB data memory, as XSG_VU1_CAPTURE records it) and
 * produces the GIF packets the microcode XGKICKs, byte for byte.  They are the
 * oracle the native renderer's vertex shaders are tested against; the test
 * harness (port/tests/test_vu1_ref.c) checks them against the interpreter.
 *
 * The references work on a private copy of data memory: a program builds its
 * GIF packet in place (the ST quadword's w lane, for one, is whatever the
 * buffer held), so the packet is read back from that copy the way XGKICK
 * reads it.
 */
#ifndef XSG_VU1_REF_H
#define XSG_VU1_REF_H

#include <stdint.h>
#include "../../runtime/vu1_capture.h"

#define VU1REF_MEM_QW 1024

typedef struct Vu1RefKick {
    uint32_t qaddr, nqw;
    const uint8_t* qw;          /* points into the ref's output buffer */
} Vu1RefKick;

typedef struct Vu1Ref {
    /* inputs */
    int set;                    /* microcode set (.vudata MPG chain index, 0..6) */
    uint32_t pc;                /* entry byte address (MSCAL imm*8, or the MSCNT resume point) */
    uint32_t top, itop;
    Vu1CapRegs regs;            /* register file at entry; updated by the run */
    uint8_t* memp;              /* NULL: `mem`; else run in place on this 16 KB (the port's fast path) */
    uint8_t mem[VU1REF_MEM_QW * 16];  /* data memory; updated by the run */
    /* outputs */
    uint32_t nkick;
    Vu1RefKick kick[64];
    uint8_t out[VU1REF_MEM_QW * 16 * 4];
    uint32_t out_used;
} Vu1Ref;

/* Status of a run. */
enum {
    VU1REF_OK = 0,
    VU1REF_UNSUPPORTED = 1,     /* no reference for this (set, pc, variant) */
};

/* Run the reference for r->set / r->pc.  Returns VU1REF_OK or VU1REF_UNSUPPORTED.
   `variant` (out, may be NULL) names the program that ran. */
int vu1_ref_run(Vu1Ref* r, const char** variant);

/* ---- shared helpers (vu1_ref.c) ---- */
uint8_t* vu1_ref_qw(Vu1Ref* r, uint32_t q);            /* data memory quadword q (wraps) */
uint32_t vu1_ref_ilw(Vu1Ref* r, uint32_t q, int lane);  /* ILW: low 16 bits of a lane */
void vu1_ref_isw(Vu1Ref* r, uint32_t q, int lane, uint32_t v);
void vu1_ref_xgkick(Vu1Ref* r, uint32_t q);             /* copy the GIF packet at q to EOP */

/* ---- set 0 (s_aUcodeAdr / FaceAdr / BackAdr, s_nProgType 2), vu1_set0.c ---- */
int vu1_ref_set0(Vu1Ref* r, const char** variant);
/* ---- set 2 (s_aUcodeEnv* family, s_nProgType 1), vu1_set2.c ---- */
int vu1_ref_set2(Vu1Ref* r, const char** variant);
/* ---- programs every set carries at 0x0 / 0x10 / 0x38 (vu1_ref.c) ---- */
int vu1_ref_common(Vu1Ref* r, const char** variant);

#endif

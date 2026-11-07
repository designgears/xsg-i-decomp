/*
 * VU1 capture (XSG_VU1_CAPTURE=dir): for selected frames, record every VU1
 * program run (MSCAL/MSCALF/MSCNT) as its input state (registers + the 16 KB
 * data memory) and the GIF packets it XGKICKs.  The test harness
 * (port/tests/test_vu1_ref.c) replays the C references in port/render/vu1_ref/
 * against these files.  Plain C layout, shared by the runtime and the tests.
 *
 * File: <dir>/vu1cap_FFFFFF.bin, a sequence of records
 *   u32 tag, u32 bytes (payload size), payload
 * tags:
 *   'MICR'  16384 bytes: VU1 micro memory, written before the first run of the
 *           file and whenever it changed since the last 'MICR'
 *   'RUN '  Vu1CapRun, then 16384 bytes of data memory as the program saw it
 *           at entry, then nkick x { u32 qaddr, u32 nqw, nqw x 16 bytes }
 *           (each GIF packet exactly as XGKICK sent it, tags to EOP)
 *   'UPK '  n x { u32 vifcode, u32 cl | wl << 8 | mode << 16 }: the UNPACKs VIF1
 *           decoded since the previous run; written just before that run's 'RUN '
 */
#ifndef XSG_VU1_CAPTURE_H
#define XSG_VU1_CAPTURE_H

#include <stdint.h>

#define VU1CAP_TAG_MICR 0x5243494du /* 'MICR' */
#define VU1CAP_TAG_RUN  0x204e5552u /* 'RUN ' */
#define VU1CAP_TAG_UPK  0x204b5055u /* 'UPK ' */

typedef struct Vu1CapRegs {
    uint32_t vf[32][4];
    uint32_t acc[4];
    uint32_t vi[16];
    uint32_t I, Q, P, R;
    uint32_t mac, status, clip;
    uint32_t pad;
} Vu1CapRegs;

typedef struct Vu1CapRun {
    uint32_t frame;      /* xsg_frame */
    uint32_t seq;        /* run index within the frame */
    uint32_t pc;         /* byte address in micro memory */
    uint32_t top, itop;
    uint32_t kind;       /* 0 MSCAL/MSCALF, 1 MSCNT (pc = previous stop) */
    uint32_t pairs;      /* pairs the interpreter executed */
    uint32_t nkick;
    uint32_t micro_id;   /* index of the 'MICR' record in force (0-based) */
    uint32_t end_pc;     /* where the program stopped */
    Vu1CapRegs regs;     /* at entry */
} Vu1CapRun;

#endif

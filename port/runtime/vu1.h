/*
 * Stage 6: VU1 micro-mode interpreter (written for the xsg port).
 *
 * VU1 instruction memory is PS2_VU_BASE + 0x8000 (16 KB), data memory
 * PS2_VU_BASE + 0xC000 (16 KB), both the same bytes the EE and the VIF1
 * decoder (gs_bridge.cpp) write.  Programs run synchronously to their E bit.
 */
#ifndef XSG_VU1_H
#define XSG_VU1_H

#include <cstdint>

namespace xsgvu {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

/* XGKICK: called with a pointer to VU1 data memory, the quadword address the
   packet starts at; the callee walks the GIF tags (with wrap) to EOP. */
using KickFn = void (*)(const std::uint8_t* data, u32 qaddr);

struct Stats {
    u64 runs = 0, pairs = 0, kicks = 0, kick_qw = 0, limit_hits = 0;
    u64 flag_reads = 0;     /* FMAND/FMEQ/FMOR/FSAND/FSEQ/FSOR executed */
    u64 clip_reads = 0;     /* FCAND/FCEQ/FCOR/FCGET */
    u64 bad = 0;            /* undecoded instructions */
};

void SetKick(KickFn fn);
/* Run from byte address `pc` in micro memory to the end of the program (E bit);
   TOP/ITOP as VIF1 hands them over.  Returns the pairs executed. */
u64 Run(u32 pc, u32 top, u32 itop);
/* Where the last run stopped (MSCNT continues there). */
u32 Resume();
Stats& GetStats();
/* XSG_VU_HIST=1: count each instruction kind executed; dumped by DumpHistogram. */
void DumpHistogram();

}  // namespace xsgvu

#endif

/*
 * VU1 reference dispatch and the programs shared by every microcode set.
 * See vu1_ref.h and port/docs/VU1_PROGRAMS.md.
 */
#include "vu1_ref.h"

#include <string.h>

uint8_t* vu1_ref_qw(Vu1Ref* r, uint32_t q) { return r->mem + (q & (VU1REF_MEM_QW - 1)) * 16; }

uint32_t vu1_ref_ilw(Vu1Ref* r, uint32_t q, int lane) {
    uint32_t v;
    memcpy(&v, vu1_ref_qw(r, q) + 4 * lane, 4);
    return v & 0xffff;
}

void vu1_ref_isw(Vu1Ref* r, uint32_t q, int lane, uint32_t v) {
    v &= 0xffff;
    memcpy(vu1_ref_qw(r, q) + 4 * lane, &v, 4);
}

void vu1_ref_xgkick(Vu1Ref* r, uint32_t q) {
    if (r->nkick >= sizeof r->kick / sizeof r->kick[0]) return;
    Vu1RefKick* k = &r->kick[r->nkick++];
    k->qaddr = q & (VU1REF_MEM_QW - 1);
    k->qw = r->out + r->out_used;
    k->nqw = 0;
    for (unsigned guard = 0; guard < 1024; ++guard) {
        uint64_t lo;
        memcpy(&lo, vu1_ref_qw(r, q), 8);
        unsigned nloop = (unsigned)(lo & 0x7fff), eop = (unsigned)(lo >> 15) & 1;
        unsigned flg = (unsigned)(lo >> 58) & 3, nreg = (unsigned)(lo >> 60) & 15;
        if (!nreg) nreg = 16;
        unsigned body = flg == 0 ? nloop * nreg : flg == 1 ? (nloop * nreg + 1) / 2 : nloop;
        if (body > 1024) break;
        for (unsigned i = 0; i <= body; ++i, ++q) {
            if (r->out_used + 16 > sizeof r->out) return;
            memcpy(r->out + r->out_used, vu1_ref_qw(r, q), 16);
            r->out_used += 16;
            k->nqw++;
        }
        if (eop) break;
    }
}

/*
 * 0x0000  nop[E]                       : empty program (the MSCNT stop point too)
 * 0x0010  iaddiu vi14, 0x3d5 ; b 0 ; xgkick vi14
 *                                      : kick the A+D packet nmlPacketAddGsFlush
 *                                        UNPACKed at qw 0x3d5 (then stop at 0x0)
 * 0x0038  as 0x10 for qw 0x2dc         : nmlPacketAddGsFlushWide
 *
 * vi14 keeps the packet address afterwards; nothing else changes.
 */
int vu1_ref_common(Vu1Ref* r, const char** variant) {
    switch (r->pc) {
    case 0x0000: case 0x0028:
        if (variant) *variant = "nop";
        return VU1REF_OK;
    case 0x0010:
        if (variant) *variant = "gs_flush_3d5";
        r->regs.vi[14] = 0x3d5;
        vu1_ref_xgkick(r, 0x3d5);
        return VU1REF_OK;
    case 0x0038:
        if (variant) *variant = "gs_flush_2dc";
        r->regs.vi[14] = 0x2dc;
        vu1_ref_xgkick(r, 0x2dc);
        return VU1REF_OK;
    }
    return VU1REF_UNSUPPORTED;
}

int vu1_ref_run(Vu1Ref* r, const char** variant) {
    r->nkick = 0;
    r->out_used = 0;
    if (variant) *variant = "?";
    if (vu1_ref_common(r, variant) == VU1REF_OK) return VU1REF_OK;
    switch (r->set) {
    case 0: return vu1_ref_set0(r, variant);
    case 2: return vu1_ref_set2(r, variant);
    default: return VU1REF_UNSUPPORTED;
    }
}

/*
 * Stage 6: VU1 micro-mode interpreter, written for the xsg port (see vu1.h).
 *
 * Model:
 *  - one instruction pair per cycle, no data-hazard stalls (results are
 *    visible to the next pair);
 *  - within a pair the lower instruction reads registers before the upper
 *    writes them, and where both write the same VF field the upper wins;
 *  - Q lands 7 cycles after DIV/SQRT, 13 after RSQRT (WAITQ lands it now),
 *    P after the EFU op's latency (WAITP);
 *  - MAC/status/clip flags reach the lower instructions FLAG_LATENCY (4,
 *    XSG_VU_FLAGLAT) pairs after the upper op that set them;
 *  - PS2 floats: inputs with exponent 255 read as +-FLT_MAX, denormals as 0;
 *    results clamp to +-FLT_MAX (O flag); MXCSR is round-toward-zero with
 *    FTZ/DAZ while a program runs.
 *  - I bit (lower word = I immediate), E bit (stop after the next pair),
 *    branch delay slots. M/D/T bits are ignored.
 */
#include "vu1.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <xmmintrin.h>

extern "C" {
#include "xsg_rt.h"
#include "vu1_capture.h"
}

namespace xsgvu {
bool CapOn();
void CapBegin(unsigned pc, unsigned top, unsigned itop, const Vu1CapRegs& regs,
              const unsigned char* micro, const unsigned char* data);
void CapKick(const unsigned char* data, unsigned q);
void CapEnd(unsigned pairs, unsigned end_pc);
}

namespace xsgvu {
namespace {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using s32 = std::int32_t;

/* VU1 by default; RunVu0 (Stage 7) swaps these for VU0's 4 KB memories. */
u32 g_micro_off = 0x8000, g_data_off = 0xC000, g_pc_mask = 0x3ff8, g_q_mask = 0x3ff;
u8* Micro() { return reinterpret_cast<u8*>(uintptr_t(PS2_VU_BASE + g_micro_off)); }
u8* Data() { return reinterpret_cast<u8*>(uintptr_t(PS2_VU_BASE + g_data_off)); }

union V { u32 u[4]; float f[4]; s32 i[4]; };

struct State {
    V vf[32];
    V acc;
    u16 vi[16];
    u32 I = 0, Q = 0, P = 0, R = 0x3f800000;
    u32 top = 0, itop = 0;
    // flags: as the lower instructions see them, and as the FMAC last produced them
    u32 mac = 0, status = 0, clip = 0;
    u32 mac_l = 0, status_l = 0, clip_l = 0;
    // pipelines
    u64 cycle = 0;
    bool q_busy = false, p_busy = false;
    u32 q_next = 0, p_next = 0;
    u64 q_ready = 0, p_ready = 0;
    struct FW { u64 ready; u32 mac, status, clip; };
    FW fq[16];
    int fq_head = 0, fq_n = 0;
    u32 resume = 0;
} s;

Stats stats;
KickFn kick_fn = nullptr;
int flag_latency = -1;
bool hist_on = false;
std::map<std::string, u64> hist;

inline void H(const char* name) { if (hist_on) hist[name]++; }

/* ------------------------------------------------------------ floats */

inline float In(u32 b) {
    if ((b & 0x7f800000u) == 0x7f800000u) b = (b & 0x80000000u) | 0x7f7fffffu;
    else if ((b & 0x7f800000u) == 0) b &= 0x80000000u;
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

/* Result clamp; accumulates MAC bits for lane i (0 = x) when flags != null. */
inline u32 Out(float f, int lane, u32* flags) {
    u32 b;
    std::memcpy(&b, &f, 4);
    const u32 e = b & 0x7f800000u;
    const int bit = 3 - lane;
    if (e == 0x7f800000u) {
        if ((b & 0x7fffffu) && !(b & 0x80000000u)) b = 0x7f7fffffu;  // NaN -> +max
        else b = (b & 0x80000000u) | 0x7f7fffffu;
        if (flags) *flags |= 1u << (12 + bit);
    } else if (e == 0 && (b & 0x7fffffu)) {
        b &= 0x80000000u;
        if (flags) *flags |= 1u << (8 + bit);
    }
    if (flags) {
        if (!(b & 0x7fffffffu)) *flags |= 1u << bit;
        if (b & 0x80000000u) *flags |= 1u << (4 + bit);
    }
    return b;
}

inline float Fv(const V& v, int l) { return In(v.u[l]); }

/* ------------------------------------------------------------ flags */

void PushFlags(u32 mac, bool set_mac, u32 clip, bool set_clip) {
    if (set_mac) {
        u32 st = 0;
        if (mac & 0x000f) st |= 1;
        if (mac & 0x00f0) st |= 2;
        if (mac & 0x0f00) st |= 4;
        if (mac & 0xf000) st |= 8;
        s.mac_l = mac;
        s.status_l = (s.status_l & 0xff0) | st | (st << 6);
    }
    if (set_clip) s.clip_l = clip;
    if (flag_latency == 0) { s.mac = s.mac_l; s.status = (s.status_l & ~0xc30u) | (s.status & 0xc30u); s.clip = s.clip_l; return; }
    if (s.fq_n == 16) {   // full: land the oldest
        auto& w = s.fq[s.fq_head];
        s.mac = w.mac; s.status = w.status; s.clip = w.clip;
        s.fq_head = (s.fq_head + 1) & 15; s.fq_n--;
    }
    auto& w = s.fq[(s.fq_head + s.fq_n) & 15];
    w.ready = s.cycle + u64(flag_latency);
    w.mac = s.mac_l; w.status = s.status_l; w.clip = s.clip_l;
    s.fq_n++;
}

void LandFlags(bool all) {
    while (s.fq_n && (all || s.fq[s.fq_head].ready <= s.cycle)) {
        auto& w = s.fq[s.fq_head];
        s.mac = w.mac;
        // I/D (bits 4,5 and 10,11) come from the lower DIV and are kept in both views.
        s.status = (w.status & ~0xc30u) | (s.status & 0xc30u);
        s.clip = w.clip;
        s.fq_head = (s.fq_head + 1) & 15;
        s.fq_n--;
    }
}

void LandQP(bool all) {
    if (s.q_busy && (all || s.q_ready <= s.cycle)) { s.Q = s.q_next; s.q_busy = false; }
    if (s.p_busy && (all || s.p_ready <= s.cycle)) { s.P = s.p_next; s.p_busy = false; }
}

/* ------------------------------------------------------------ upper */

struct UpRes {
    int target = 0;     // 0 none, 1 vf[reg], 2 acc
    int reg = 0;
    u32 dest = 0;       // x 8 y 4 z 2 w 1
    u32 val[4] = {};
    bool set_mac = false, set_clip = false;
    u32 mac = 0, clip = 0;
};

enum Op { ADD, SUB, MUL, MADD, MSUB, MAX, MIN };

inline bool Lane(u32 dest, int l) { return (dest >> (3 - l)) & 1; }

void Arith(Op op, const V& a, const float* b, UpRes& r) {
    r.set_mac = op != MAX && op != MIN;
    for (int l = 0; l < 4; ++l) {
        if (!Lane(r.dest, l)) continue;
        float x = Fv(a, l), y = b[l], z;
        u32* fl = r.set_mac ? &r.mac : nullptr;
        switch (op) {
        case ADD: z = x + y; break;
        case SUB: z = x - y; break;
        case MUL: z = x * y; break;
        case MADD: { float m = In(Out(x * y, l, nullptr)); z = Fv(s.acc, l) + m; break; }
        case MSUB: { float m = In(Out(x * y, l, nullptr)); z = Fv(s.acc, l) - m; break; }
        case MAX: z = x > y ? x : y; break;
        default: z = x < y ? x : y; break;
        }
        r.val[l] = Out(z, l, fl);
    }
}

void Upper(u32 w, UpRes& r) {
    const u32 dest = (w >> 21) & 15, ft = (w >> 16) & 31, fs = (w >> 11) & 31, fd = (w >> 6) & 31;
    const u32 op = w & 0x3f;
    r.dest = dest;
    const V& A = s.vf[fs];
    const V& B = s.vf[ft];
    float b[4];
    auto bvec = [&] { for (int l = 0; l < 4; ++l) b[l] = Fv(B, l); };
    auto bsc = [&](u32 bits) { float f = In(bits); for (int l = 0; l < 4; ++l) b[l] = f; };
    auto bbc = [&] { bsc(B.u[w & 3]); };
    if (op < 0x3c) {
        r.target = 1; r.reg = int(fd);
        static const char* bcn[] = {"ADDbc", "SUBbc", "MADDbc", "MSUBbc", "MAXbc", "MINIbc", "MULbc"};
        if (op < 0x1c) {
            static const Op ops[7] = {ADD, SUB, MADD, MSUB, MAX, MIN, MUL};
            H(bcn[op >> 2]);
            bbc(); Arith(ops[op >> 2], A, b, r); return;
        }
        switch (op) {
        case 0x1c: H("MULq"); bsc(s.Q); Arith(MUL, A, b, r); return;
        case 0x1d: H("MAXi"); bsc(s.I); Arith(MAX, A, b, r); return;
        case 0x1e: H("MULi"); bsc(s.I); Arith(MUL, A, b, r); return;
        case 0x1f: H("MINIi"); bsc(s.I); Arith(MIN, A, b, r); return;
        case 0x20: H("ADDq"); bsc(s.Q); Arith(ADD, A, b, r); return;
        case 0x21: H("MADDq"); bsc(s.Q); Arith(MADD, A, b, r); return;
        case 0x22: H("ADDi"); bsc(s.I); Arith(ADD, A, b, r); return;
        case 0x23: H("MADDi"); bsc(s.I); Arith(MADD, A, b, r); return;
        case 0x24: H("SUBq"); bsc(s.Q); Arith(SUB, A, b, r); return;
        case 0x25: H("MSUBq"); bsc(s.Q); Arith(MSUB, A, b, r); return;
        case 0x26: H("SUBi"); bsc(s.I); Arith(SUB, A, b, r); return;
        case 0x27: H("MSUBi"); bsc(s.I); Arith(MSUB, A, b, r); return;
        case 0x28: H("ADD"); bvec(); Arith(ADD, A, b, r); return;
        case 0x29: H("MADD"); bvec(); Arith(MADD, A, b, r); return;
        case 0x2a: H("MUL"); bvec(); Arith(MUL, A, b, r); return;
        case 0x2b: H("MAX"); bvec(); Arith(MAX, A, b, r); return;
        case 0x2c: H("SUB"); bvec(); Arith(SUB, A, b, r); return;
        case 0x2d: H("MSUB"); bvec(); Arith(MSUB, A, b, r); return;
        case 0x2e: {   // OPMSUB.xyz fd = ACC - fs.yzx * ft.zxy
            H("OPMSUB");
            r.set_mac = true;
            static const int i1[3] = {1, 2, 0}, i2[3] = {2, 0, 1};
            for (int l = 0; l < 3; ++l) {
                if (!Lane(dest, l)) continue;
                float m = In(Out(Fv(A, i1[l]) * Fv(B, i2[l]), l, nullptr));
                r.val[l] = Out(Fv(s.acc, l) - m, l, &r.mac);
            }
            r.dest &= 0xe;
            return;
        }
        case 0x2f: H("MINI"); bvec(); Arith(MIN, A, b, r); return;
        default: H("upper?"); stats.bad++; r.target = 0; return;
        }
    }
    const u32 sp = (w & 3) | ((w >> 4) & 0x7c);
    if (sp < 0x10 || (sp >= 0x18 && sp < 0x1d) || sp == 0x1e || (sp >= 0x20 && sp < 0x2b) || sp == 0x2c || sp == 0x2d) {
        r.target = 2;
        if (sp < 0x10 || (sp >= 0x18 && sp < 0x1c)) {
            static const Op ops[7] = {ADD, SUB, MADD, MSUB, ADD, ADD, MUL};
            static const char* n[] = {"ADDAbc", "SUBAbc", "MADDAbc", "MSUBAbc", "", "", "MULAbc"};
            H(n[sp >> 2]);
            bbc(); Arith(ops[sp >> 2], A, b, r); return;
        }
        switch (sp) {
        case 0x1c: H("MULAq"); bsc(s.Q); Arith(MUL, A, b, r); return;
        case 0x1e: H("MULAi"); bsc(s.I); Arith(MUL, A, b, r); return;
        case 0x20: H("ADDAq"); bsc(s.Q); Arith(ADD, A, b, r); return;
        case 0x21: H("MADDAq"); bsc(s.Q); Arith(MADD, A, b, r); return;
        case 0x22: H("ADDAi"); bsc(s.I); Arith(ADD, A, b, r); return;
        case 0x23: H("MADDAi"); bsc(s.I); Arith(MADD, A, b, r); return;
        case 0x24: H("SUBAq"); bsc(s.Q); Arith(SUB, A, b, r); return;
        case 0x25: H("MSUBAq"); bsc(s.Q); Arith(MSUB, A, b, r); return;
        case 0x26: H("SUBAi"); bsc(s.I); Arith(SUB, A, b, r); return;
        case 0x27: H("MSUBAi"); bsc(s.I); Arith(MSUB, A, b, r); return;
        case 0x28: H("ADDA"); bvec(); Arith(ADD, A, b, r); return;
        case 0x29: H("MADDA"); bvec(); Arith(MADD, A, b, r); return;
        case 0x2a: H("MULA"); bvec(); Arith(MUL, A, b, r); return;
        case 0x2c: H("SUBA"); bvec(); Arith(SUB, A, b, r); return;
        case 0x2d: H("MSUBA"); bvec(); Arith(MSUB, A, b, r); return;
        }
    }
    switch (sp) {
    case 0x10: case 0x11: case 0x12: case 0x13: {   // ITOF0/4/12/15 ft = fs
        H("ITOF");
        static const float sc[4] = {1.0f, 1.0f / 16, 1.0f / 4096, 1.0f / 32768};
        r.target = 1; r.reg = int(ft);
        for (int l = 0; l < 4; ++l) if (Lane(dest, l)) r.val[l] = Out(float(A.i[l]) * sc[sp & 3], l, nullptr);
        return;
    }
    case 0x14: case 0x15: case 0x16: case 0x17: {   // FTOI0/4/12/15
        H("FTOI");
        static const float sc[4] = {1.0f, 16.0f, 4096.0f, 32768.0f};
        r.target = 1; r.reg = int(ft);
        for (int l = 0; l < 4; ++l) {
            if (!Lane(dest, l)) continue;
            double v = double(Fv(A, l)) * sc[sp & 3];
            s32 iv;
            if (v >= 2147483647.0) iv = 0x7fffffff;
            else if (v <= -2147483648.0) iv = s32(0x80000000u);
            else iv = s32(v);
            r.val[l] = u32(iv);
        }
        return;
    }
    case 0x1d:   // ABS ft = |fs|
        H("ABS");
        r.target = 1; r.reg = int(ft);
        for (int l = 0; l < 4; ++l) if (Lane(dest, l)) r.val[l] = Out(std::fabs(Fv(A, l)), l, nullptr);
        return;
    case 0x1f: {  // CLIP fs.xyz, ft.w
        H("CLIP");
        float lim = std::fabs(Fv(B, 3));
        u32 j = 0;
        for (int l = 0; l < 3; ++l) {
            float v = Fv(A, l);
            if (v > lim) j |= 1u << (2 * l);
            if (v < -lim) j |= 2u << (2 * l);
        }
        r.set_clip = true;
        r.clip = ((s.clip_l << 6) | j) & 0xffffff;
        return;
    }
    case 0x2e: {  // OPMULA.xyz ACC = fs.yzx * ft.zxy
        H("OPMULA");
        r.target = 2; r.set_mac = true;
        static const int i1[3] = {1, 2, 0}, i2[3] = {2, 0, 1};
        for (int l = 0; l < 3; ++l)
            if (Lane(dest, l)) r.val[l] = Out(Fv(A, i1[l]) * Fv(B, i2[l]), l, &r.mac);
        r.dest &= 0xe;
        return;
    }
    case 0x2f: H("NOP"); return;
    default: H("upper-special?"); stats.bad++; return;
    }
}

void Commit(const UpRes& r) {
    if (r.target == 1) {
        if (r.reg != 0)
            for (int l = 0; l < 4; ++l) if (Lane(r.dest, l)) s.vf[r.reg].u[l] = r.val[l];
    } else if (r.target == 2) {
        for (int l = 0; l < 4; ++l) if (Lane(r.dest, l)) s.acc.u[l] = r.val[l];
    }
    if (r.set_mac || r.set_clip) PushFlags(r.mac, r.set_mac, r.clip, r.set_clip);
}

/* ------------------------------------------------------------ lower */

inline u8* QAddr(u32 q) { return Data() + ((q & g_q_mask) << 4); }
inline void SetVi(u32 r, u32 v) { if (r & 15) s.vi[r & 15] = u16(v); }
inline s32 Imm11(u32 w) { return s32(w << 21) >> 21; }

void LoadQ(u32 ft, u32 dest, u32 q) {
    if (!ft) return;
    const u8* p = QAddr(q);
    for (int l = 0; l < 4; ++l) if (Lane(dest, l)) std::memcpy(&s.vf[ft].u[l], p + 4 * l, 4);
}
void StoreQ(u32 fs, u32 dest, u32 q) {
    u8* p = QAddr(q);
    for (int l = 0; l < 4; ++l) if (Lane(dest, l)) std::memcpy(p + 4 * l, &s.vf[fs].u[l], 4);
}

void StartQ(float v, int lat) {
    LandQP(true);   // a busy unit stalls until its result lands
    s.q_busy = true;
    s.q_next = Out(v, 0, nullptr);
    s.q_ready = s.cycle + u64(lat);
}
void StartP(float v, int lat) {
    if (s.p_busy) { s.P = s.p_next; s.p_busy = false; }
    s.p_busy = true;
    s.p_next = Out(v, 0, nullptr);
    s.p_ready = s.cycle + u64(lat);
}
void DivFlags(bool inv, bool dz) {
    u32 b = (inv ? 0x10u : 0) | (dz ? 0x20u : 0);
    s.status = (s.status & ~0x30u) | b | (b << 6);
    s.status_l = (s.status_l & ~0x30u) | b | (b << 6);
}

void Kick(u32 qaddr) {
    stats.kicks++;
    if (g_data_off == 0xC000 && CapOn()) CapKick(Data(), qaddr & 0x3ff);
    if (kick_fn) kick_fn(Data(), qaddr & 0x3ff);
}

/* Returns true when the instruction branches (target in *tgt). */
bool Lower(u32 w, u32 pc, u32* tgt) {
    const u32 op7 = w >> 25;
    const u32 dest = (w >> 21) & 15, ft = (w >> 16) & 31, fs = (w >> 11) & 31, fd = (w >> 6) & 31;
    const u32 it = ft & 15, is = fs & 15, id = fd & 15;
    auto br = [&](bool take) { if (take) *tgt = (pc + 8 + u32(Imm11(w) * 8)) & g_pc_mask; return take; };
    switch (op7) {
    case 0x00: H("LQ"); LoadQ(ft, dest, u32(s.vi[is] + Imm11(w))); return false;
    case 0x01: H("SQ"); StoreQ(fs, dest, u32(s.vi[it] + Imm11(w))); return false;
    case 0x04: {
        H("ILW");
        const u8* p = QAddr(u32(s.vi[is] + Imm11(w)));
        for (int l = 0; l < 4; ++l) if (Lane(dest, l)) { u32 v; std::memcpy(&v, p + 4 * l, 4); SetVi(it, v); break; }
        return false;
    }
    case 0x05: {
        H("ISW");
        u8* p = QAddr(u32(s.vi[is] + Imm11(w)));
        u32 v = s.vi[it];
        for (int l = 0; l < 4; ++l) if (Lane(dest, l)) std::memcpy(p + 4 * l, &v, 4);
        return false;
    }
    case 0x08: H("IADDIU"); SetVi(it, s.vi[is] + (((w >> 10) & 0x7800) | (w & 0x7ff))); return false;
    case 0x09: H("ISUBIU"); SetVi(it, s.vi[is] - (((w >> 10) & 0x7800) | (w & 0x7ff))); return false;
    case 0x10: H("FCEQ"); stats.clip_reads++; SetVi(1, (s.clip & 0xffffff) == (w & 0xffffff)); return false;
    case 0x11: H("FCSET"); s.clip = s.clip_l = w & 0xffffff; return false;
    case 0x12: H("FCAND"); stats.clip_reads++; SetVi(1, (s.clip & w & 0xffffff) != 0); return false;
    case 0x13: H("FCOR"); stats.clip_reads++; SetVi(1, ((s.clip | w) & 0xffffff) == 0xffffff); return false;
    case 0x14: case 0x15: case 0x16: case 0x17: {
        const u32 imm = ((w >> 10) & 0x800) | (w & 0x7ff);
        stats.flag_reads++;
        if (op7 == 0x14) { H("FSEQ"); SetVi(it, (s.status & 0xfff) == imm); }
        else if (op7 == 0x15) { H("FSSET"); stats.flag_reads--; s.status = (s.status & 0x3f) | (imm & 0xfc0); s.status_l = (s.status_l & 0x3f) | (imm & 0xfc0); }
        else if (op7 == 0x16) { H("FSAND"); SetVi(it, s.status & imm); }
        else { H("FSOR"); SetVi(it, (s.status & 0xfff) | imm); }
        return false;
    }
    case 0x18: H("FMEQ"); stats.flag_reads++; SetVi(it, (s.mac & 0xffff) == s.vi[is]); return false;
    case 0x1a: H("FMAND"); stats.flag_reads++; SetVi(it, s.mac & s.vi[is]); return false;
    case 0x1b: H("FMOR"); stats.flag_reads++; SetVi(it, (s.mac & 0xffff) | s.vi[is]); return false;
    case 0x1c: H("FCGET"); stats.clip_reads++; SetVi(it, s.clip & 0xfff); return false;
    case 0x20: H("B"); return br(true);
    case 0x21: H("BAL"); SetVi(it, (pc + 16) / 8); return br(true);
    case 0x24: H("JR"); *tgt = (u32(s.vi[is]) * 8) & g_pc_mask; return true;
    case 0x25: H("JALR"); *tgt = (u32(s.vi[is]) * 8) & g_pc_mask; SetVi(it, (pc + 16) / 8); return true;
    case 0x28: H("IBEQ"); return br(s.vi[it] == s.vi[is]);
    case 0x29: H("IBNE"); return br(s.vi[it] != s.vi[is]);
    case 0x2c: H("IBLTZ"); return br(std::int16_t(s.vi[is]) < 0);
    case 0x2d: H("IBGTZ"); return br(std::int16_t(s.vi[is]) > 0);
    case 0x2e: H("IBLEZ"); return br(std::int16_t(s.vi[is]) <= 0);
    case 0x2f: H("IBGEZ"); return br(std::int16_t(s.vi[is]) >= 0);
    case 0x40: break;
    default: H("lower?"); stats.bad++; return false;
    }
    const u32 sub = w & 0x3f;
    if (sub < 0x3c) {
        switch (sub) {
        case 0x30: H("IADD"); SetVi(id, s.vi[is] + s.vi[it]); break;
        case 0x31: H("ISUB"); SetVi(id, s.vi[is] - s.vi[it]); break;
        case 0x32: H("IADDI"); SetVi(it, s.vi[is] + u32(s32(fd << 27) >> 27)); break;
        case 0x34: H("IAND"); SetVi(id, s.vi[is] & s.vi[it]); break;
        case 0x35: H("IOR"); SetVi(id, s.vi[is] | s.vi[it]); break;
        default: H("lower40?"); stats.bad++; break;
        }
        return false;
    }
    const u32 sp = (w & 3) | ((w >> 4) & 0x7c);
    const u32 fsf = (w >> 21) & 3, ftf = (w >> 23) & 3;
    const V& S = s.vf[fs];
    const V& T = s.vf[ft];
    switch (sp) {
    case 0x30: H("MOVE"); if (ft) for (int l = 0; l < 4; ++l) if (Lane(dest, l)) s.vf[ft].u[l] = S.u[l]; break;
    case 0x31: { H("MR32"); V t = S; if (ft) for (int l = 0; l < 4; ++l) if (Lane(dest, l)) s.vf[ft].u[l] = t.u[(l + 1) & 3]; break; }
    case 0x34: H("LQI"); LoadQ(ft, dest, s.vi[is]); SetVi(is, s.vi[is] + 1); break;
    case 0x35: H("SQI"); StoreQ(fs, dest, s.vi[it]); SetVi(it, s.vi[it] + 1); break;
    case 0x36: H("LQD"); SetVi(is, s.vi[is] - 1); LoadQ(ft, dest, s.vi[is]); break;
    case 0x37: H("SQD"); SetVi(it, s.vi[it] - 1); StoreQ(fs, dest, s.vi[it]); break;
    case 0x38: {   // DIV Q, fs.fsf, ft.ftf
        H("DIV");
        float a = Fv(S, int(fsf)), b = Fv(T, int(ftf));
        if (b == 0.0f) {
            DivFlags(a == 0.0f, a != 0.0f);
            u32 sign = ((S.u[fsf] ^ T.u[ftf]) & 0x80000000u);
            u32 r = sign | 0x7f7fffffu;
            float f; std::memcpy(&f, &r, 4);
            StartQ(f, 7);
        } else { DivFlags(false, false); StartQ(a / b, 7); }
        break;
    }
    case 0x39: {   // SQRT Q, ft.ftf
        H("SQRT");
        float b = Fv(T, int(ftf));
        DivFlags(b < 0, false);
        StartQ(std::sqrt(std::fabs(b)), 7);
        break;
    }
    case 0x3a: {   // RSQRT Q, fs.fsf / sqrt(ft.ftf)
        H("RSQRT");
        float a = Fv(S, int(fsf)), b = Fv(T, int(ftf));
        if (b == 0.0f) {
            DivFlags(a == 0.0f, a != 0.0f);
            u32 r = (S.u[fsf] & 0x80000000u) | 0x7f7fffffu;
            float f; std::memcpy(&f, &r, 4);
            StartQ(f, 13);
        } else { DivFlags(b < 0, false); StartQ(a / std::sqrt(std::fabs(b)), 13); }
        break;
    }
    case 0x3b: H("WAITQ"); if (s.q_busy) { s.Q = s.q_next; s.q_busy = false; } break;
    case 0x3c: H("MTIR"); SetVi(it, S.u[fsf] & 0xffff); break;
    case 0x3d: H("MFIR"); if (ft) for (int l = 0; l < 4; ++l) if (Lane(dest, l)) s.vf[ft].i[l] = std::int16_t(s.vi[is]); break;
    case 0x3e: {
        H("ILWR");
        const u8* p = QAddr(s.vi[is]);
        for (int l = 0; l < 4; ++l) if (Lane(dest, l)) { u32 v; std::memcpy(&v, p + 4 * l, 4); SetVi(it, v); break; }
        break;
    }
    case 0x3f: {
        H("ISWR");
        u8* p = QAddr(s.vi[is]);
        u32 v = s.vi[it];
        for (int l = 0; l < 4; ++l) if (Lane(dest, l)) std::memcpy(p + 4 * l, &v, 4);
        break;
    }
    case 0x40: case 0x41: {   // RNEXT / RGET
        if (sp == 0x40) {
            H("RNEXT");
            u32 x = (s.R >> 4) & 1, y = (s.R >> 22) & 1;
            s.R = (((s.R << 1) ^ x ^ y) & 0x7fffff) | 0x3f800000u;
        } else H("RGET");
        if (ft) for (int l = 0; l < 4; ++l) if (Lane(dest, l)) s.vf[ft].u[l] = s.R;
        break;
    }
    case 0x42: H("RINIT"); s.R = 0x3f800000u | (S.u[fsf] & 0x7fffff); break;
    case 0x43: H("RXOR"); s.R = 0x3f800000u | ((s.R ^ S.u[fsf]) & 0x7fffff); break;
    case 0x64: H("MFP"); if (ft) for (int l = 0; l < 4; ++l) if (Lane(dest, l)) s.vf[ft].u[l] = s.P; break;
    case 0x68: H("XTOP"); SetVi(it, s.top); break;
    case 0x69: H("XITOP"); SetVi(it, s.itop); break;
    case 0x6c: H("XGKICK"); Kick(s.vi[is]); break;
    case 0x70: { H("ESADD"); float x = Fv(S, 0), y = Fv(S, 1), z = Fv(S, 2); StartP(x * x + y * y + z * z, 11); break; }
    case 0x71: { H("ERSADD"); float x = Fv(S, 0), y = Fv(S, 1), z = Fv(S, 2); StartP(1.0f / (x * x + y * y + z * z), 18); break; }
    case 0x72: { H("ELENG"); float x = Fv(S, 0), y = Fv(S, 1), z = Fv(S, 2); StartP(std::sqrt(x * x + y * y + z * z), 18); break; }
    case 0x73: { H("ERLENG"); float x = Fv(S, 0), y = Fv(S, 1), z = Fv(S, 2); StartP(1.0f / std::sqrt(x * x + y * y + z * z), 24); break; }
    case 0x74: { H("EATANxy"); StartP(std::atan2(Fv(S, 1), Fv(S, 0)), 54); break; }
    case 0x75: { H("EATANxz"); StartP(std::atan2(Fv(S, 2), Fv(S, 0)), 54); break; }
    case 0x76: { H("ESUM"); StartP(Fv(S, 0) + Fv(S, 1) + Fv(S, 2) + Fv(S, 3), 12); break; }
    case 0x78: { H("ESQRT"); StartP(std::sqrt(std::fabs(Fv(S, int(fsf)))), 12); break; }
    case 0x79: { H("ERSQRT"); StartP(1.0f / std::sqrt(std::fabs(Fv(S, int(fsf)))), 18); break; }
    case 0x7a: { H("ERCPR"); StartP(1.0f / Fv(S, int(fsf)), 12); break; }
    case 0x7b: H("WAITP"); if (s.p_busy) { s.P = s.p_next; s.p_busy = false; } break;
    case 0x7c: { H("ESIN"); StartP(std::sin(Fv(S, int(fsf))), 29); break; }
    case 0x7d: { H("EATAN"); StartP(std::atan(Fv(S, int(fsf))), 54); break; }
    case 0x7e: { H("EEXP"); StartP(std::exp(-Fv(S, int(fsf))), 44); break; }
    default: H("lower-special?"); stats.bad++; break;
    }
    return false;
}

}  // namespace

/* ------------------------------------------------------------ API */

void SetKick(KickFn fn) { kick_fn = fn; }
u32 Resume() { return s.resume; }
Stats& GetStats() { return stats; }

void DumpHistogram() {
    if (!hist_on) return;
    xsg_logf("[vu1] instruction histogram (%zu kinds):\n", hist.size());
    for (auto& [k, v] : hist) xsg_logf("[vu1]   %-14s %llu\n", k.c_str(), (unsigned long long)v);
}

u64 Run(u32 pc, u32 top, u32 itop) {
    if (flag_latency < 0) {
        const char* e = std::getenv("XSG_VU_FLAGLAT");
        flag_latency = e ? std::atoi(e) : 4;
        hist_on = std::getenv("XSG_VU_HIST") && std::atoi(std::getenv("XSG_VU_HIST"));
        s.vf[0].f[3] = 1.0f;
    }
    stats.runs++;
    static long trace_runs = std::getenv("XSG_VU_TRACE") ? std::atol(std::getenv("XSG_VU_TRACE")) : 0;
    const bool trace_left = trace_runs > 0;
    if (trace_runs > 0) { trace_runs--; xsg_logf("[vu1t] run at 0x%x top %x itop %x\n", pc, top, itop); }
    s.top = top; s.itop = itop;
    const bool cap = g_data_off == 0xC000 && CapOn();
    if (cap) {
        Vu1CapRegs r;
        std::memset(&r, 0, sizeof r);
        std::memcpy(r.vf, s.vf, sizeof r.vf);
        std::memcpy(r.acc, s.acc.u, 16);
        for (int i = 0; i < 16; ++i) r.vi[i] = s.vi[i];
        r.I = s.I; r.Q = s.Q; r.P = s.P; r.R = s.R;
        r.mac = s.mac; r.status = s.status; r.clip = s.clip;
        CapBegin(pc & g_pc_mask, top, itop, r, Micro(), Data());
    }
    const unsigned csr = _mm_getcsr();
    _mm_setcsr((csr & ~0x6000u) | 0x6000u | 0x8040u);   // round toward zero, FTZ, DAZ
    pc &= g_pc_mask;
    u64 n = 0;
    bool branch = false, ending = false;
    u32 btarget = 0;
    const u64 limit = u64(1) << 22;
    const u8* micro = Micro();
    for (; n < limit; ++n) {
        s.cycle++;
        if (s.fq_n && s.fq[s.fq_head].ready <= s.cycle) LandFlags(false);
        if (s.q_busy | s.p_busy) LandQP(false);
        const u32 cur = pc;
        u32 lo, hi;
        std::memcpy(&lo, micro + cur, 4);
        std::memcpy(&hi, micro + cur + 4, 4);
        pc = (pc + 8) & g_pc_mask;
        if (trace_left) xsg_logf("[vu1t] %04x %08x %08x vi1-4 %x %x %x %x\n", cur, hi, lo, s.vi[1], s.vi[2], s.vi[3], s.vi[4]);
        const bool take_branch = branch;
        const u32 target = btarget;
        branch = false;
        // A WAITQ/WAITP in the lower slot stalls the whole pair, so the upper
        // op of the same pair already sees the new Q/P (mulq.xyz ... | waitq).
        if (!(hi & 0x80000000u) && (lo >> 25) == 0x40) {
            const u32 lsp = (lo & 3) | ((lo >> 4) & 0x7c);
            if (lsp == 0x3b && s.q_busy) { s.Q = s.q_next; s.q_busy = false; }
            if (lsp == 0x7b && s.p_busy) { s.P = s.p_next; s.p_busy = false; }
        }
        UpRes r;
        Upper(hi, r);
        if (hi & 0x80000000u) {
            Commit(r);
            s.I = lo;
        } else {
            u32 t = 0;
            if (Lower(lo, cur, &t)) { branch = true; btarget = t; }
            Commit(r);
        }
        s.vf[0].u[0] = s.vf[0].u[1] = s.vf[0].u[2] = 0; s.vf[0].f[3] = 1.0f;
        if (take_branch) pc = target;
        if (ending) { ++n; break; }
        if (hi & 0x40000000u) ending = true;
    }
    if (n >= limit) {
        stats.limit_hits++;
        if (stats.limit_hits < 5) xsg_logf("[vu1] program at 0x%x hit the %llu-pair limit\n", pc, (unsigned long long)limit);
    }
    LandFlags(true);
    LandQP(true);
    s.resume = pc;
    if (cap) CapEnd(unsigned(n), pc);
    stats.pairs += n;
    _mm_setcsr(csr);
    return n;
}

}  // namespace xsgvu

/* Stage 7: run a VU0 microprogram (vcallms) on the VU0 register file of the
   macro-mode model (vu0.c), with VU0's micro/data memories. */
#include "vu0.h"
extern "C" unsigned long long xsg_vu1_run_vu0(unsigned int pc) {
    using namespace xsgvu;
    State saved = s;
    static_assert(sizeof(xsg_vu0.vf) == sizeof(s.vf), "vf layout");
    std::memcpy(s.vf, xsg_vu0.vf, sizeof s.vf);
    std::memcpy(&s.acc, &xsg_vu0.acc, 16);
    for (int i = 0; i < 16; ++i) s.vi[i] = u16(xsg_vu0.vi[i]);
    s.I = xsg_vu0.I; s.Q = xsg_vu0.Q; s.P = xsg_vu0.P; s.R = xsg_vu0.R;
    s.mac = s.mac_l = xsg_vu0.mac; s.status = s.status_l = xsg_vu0.status; s.clip = s.clip_l = xsg_vu0.clip;
    s.fq_n = 0; s.q_busy = s.p_busy = false;
    g_micro_off = 0x0000; g_data_off = 0x4000; g_pc_mask = 0xff8; g_q_mask = 0xff;
    Stats st = stats;
    u64 n = Run(pc, 0, 0);
    stats = st;   /* keep the VU1 statistics VU1-only */
    g_micro_off = 0x8000; g_data_off = 0xC000; g_pc_mask = 0x3ff8; g_q_mask = 0x3ff;
    std::memcpy(xsg_vu0.vf, s.vf, sizeof s.vf);
    std::memcpy(&xsg_vu0.acc, &s.acc, 16);
    for (int i = 0; i < 16; ++i) xsg_vu0.vi[i] = s.vi[i];
    xsg_vu0.I = s.I; xsg_vu0.Q = s.Q; xsg_vu0.P = s.P; xsg_vu0.R = s.R;
    xsg_vu0.mac = s.mac; xsg_vu0.status = s.status; xsg_vu0.clip = s.clip;
    s = saved;
    return n;
}

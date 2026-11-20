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
#include "xsg_settings.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <xmmintrin.h>
#include <smmintrin.h>

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

/* sqrtss under the run's MXCSR (round toward zero). std::sqrt may become a
   call to i386 libm sqrtf (x87 fsqrt, round to nearest) depending on
   inlining, which differs in the last bit. */
inline float SqrtF(float v) { return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(v))); }

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

/* Stage 10: the FMAC ops on all four lanes at once (SSE). Bit-identical to
 * the per-lane In/Out path: the same single-precision ops under the same
 * MXCSR; only the dest lanes are committed and only their MAC bits kept. */
inline __m128i Bits(u32 v) { return _mm_set1_epi32(int(v)); }

inline __m128 InV(__m128i b) {
    const __m128i sign = _mm_and_si128(b, Bits(0x80000000u));
    const __m128i e = _mm_and_si128(b, Bits(0x7f800000u));
    const __m128i is_max = _mm_cmpeq_epi32(e, Bits(0x7f800000u));
    const __m128i is_den = _mm_cmpeq_epi32(e, _mm_setzero_si128());
    b = _mm_blendv_epi8(b, sign, is_den);
    b = _mm_blendv_epi8(b, _mm_or_si128(sign, Bits(0x7f7fffffu)), is_max);
    return _mm_castsi128_ps(b);
}

/* lane l (0 = x) -> MAC bit 3 - l */
const u8 kRev4[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

/* Out() on four lanes; MAC bits of the lanes in `dest` (x 8 .. w 1) are or'ed
   into *flags when flags != null. */
inline __m128i OutV(__m128 f, u32 dest, u32* flags) {
    __m128i b = _mm_castps_si128(f);
    const __m128i sign = _mm_and_si128(b, Bits(0x80000000u));
    const __m128i e = _mm_and_si128(b, Bits(0x7f800000u));
    const __m128i mant = _mm_and_si128(b, Bits(0x7fffffu));
    const __m128i mant0 = _mm_cmpeq_epi32(mant, _mm_setzero_si128());
    const __m128i is_max = _mm_cmpeq_epi32(e, Bits(0x7f800000u));
    const __m128i is_den = _mm_andnot_si128(mant0, _mm_cmpeq_epi32(e, _mm_setzero_si128()));
    // NaN with the sign clear -> +max, else sign | max
    const __m128i pos_nan = _mm_andnot_si128(mant0, _mm_cmpeq_epi32(sign, _mm_setzero_si128()));
    const __m128i maxv = _mm_blendv_epi8(_mm_or_si128(sign, Bits(0x7f7fffffu)), Bits(0x7f7fffffu), pos_nan);
    b = _mm_blendv_epi8(b, maxv, is_max);
    b = _mm_blendv_epi8(b, sign, is_den);
    if (flags) {
        const u32 dm = dest & 15;
        const u32 o = kRev4[_mm_movemask_ps(_mm_castsi128_ps(is_max))] & dm;
        const u32 u = kRev4[_mm_movemask_ps(_mm_castsi128_ps(is_den))] & dm;
        const __m128i z = _mm_cmpeq_epi32(_mm_and_si128(b, Bits(0x7fffffffu)), _mm_setzero_si128());
        const u32 zb = kRev4[_mm_movemask_ps(_mm_castsi128_ps(z))] & dm;
        const u32 sb = kRev4[_mm_movemask_ps(_mm_castsi128_ps(b))] & dm;
        *flags |= zb | (sb << 4) | (u << 8) | (o << 12);
    }
    return b;
}

inline __m128 Load(const V& v) { return _mm_castsi128_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(v.u))); }

void Arith(Op op, const V& a, __m128 y, UpRes& r) {
    r.set_mac = op != MAX && op != MIN;
    const __m128 x = InV(_mm_castps_si128(Load(a)));
    __m128 z;
    switch (op) {
    case ADD: z = _mm_add_ps(x, y); break;
    case SUB: z = _mm_sub_ps(x, y); break;
    case MUL: z = _mm_mul_ps(x, y); break;
    case MADD: case MSUB: {
        const __m128 m = InV(OutV(_mm_mul_ps(x, y), 0, nullptr));
        const __m128 acc = InV(_mm_castps_si128(Load(s.acc)));
        z = op == MADD ? _mm_add_ps(acc, m) : _mm_sub_ps(acc, m);
        break;
    }
    case MAX: z = _mm_max_ps(x, y); break;   // maxps(x, y) = x > y ? x : y
    default: z = _mm_min_ps(x, y); break;    // minps(x, y) = x < y ? x : y
    }
    _mm_storeu_si128(reinterpret_cast<__m128i*>(r.val), OutV(z, r.dest, r.set_mac ? &r.mac : nullptr));
}

void Upper(u32 w, UpRes& r) {
    const u32 dest = (w >> 21) & 15, ft = (w >> 16) & 31, fs = (w >> 11) & 31, fd = (w >> 6) & 31;
    const u32 op = w & 0x3f;
    r.dest = dest;
    const V& A = s.vf[fs];
    const V& B = s.vf[ft];
    __m128 b;
    auto bvec = [&] { b = InV(_mm_castps_si128(Load(B))); };
    auto bsc = [&](u32 bits) { b = _mm_set1_ps(In(bits)); };
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

inline __m128i DestMask(u32 dest) {
    return _mm_set_epi32(-int(dest & 1), -int((dest >> 1) & 1), -int((dest >> 2) & 1), -int((dest >> 3) & 1));
}

inline void StoreLanes(V& d, const u32* val, u32 dest) {
    __m128i* p = reinterpret_cast<__m128i*>(d.u);
    _mm_storeu_si128(p, _mm_blendv_epi8(_mm_loadu_si128(p), _mm_loadu_si128(reinterpret_cast<const __m128i*>(val)),
                                        DestMask(dest)));
}

void Commit(const UpRes& r) {
    if (r.target == 1) {
        if (r.reg != 0) StoreLanes(s.vf[r.reg], r.val, r.dest);
    } else if (r.target == 2) {
        StoreLanes(s.acc, r.val, r.dest);
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
    if (s.fq_n) LandFlags(false);
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
    /* Stage 10: MAC/status/clip land lazily, when a lower op reads or writes
       them (see Run); that gives the same values as landing every cycle. */
    if (op7 >= 0x10 && op7 <= 0x1c && s.fq_n) LandFlags(false);
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
        StartQ(SqrtF(std::fabs(b)), 7);
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
        } else { DivFlags(b < 0, false); StartQ(a / SqrtF(std::fabs(b)), 13); }
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
    case 0x72: { H("ELENG"); float x = Fv(S, 0), y = Fv(S, 1), z = Fv(S, 2); StartP(SqrtF(x * x + y * y + z * z), 18); break; }
    case 0x73: { H("ERLENG"); float x = Fv(S, 0), y = Fv(S, 1), z = Fv(S, 2); StartP(1.0f / SqrtF(x * x + y * y + z * z), 24); break; }
    case 0x74: { H("EATANxy"); StartP(std::atan2(Fv(S, 1), Fv(S, 0)), 54); break; }
    case 0x75: { H("EATANxz"); StartP(std::atan2(Fv(S, 2), Fv(S, 0)), 54); break; }
    case 0x76: { H("ESUM"); StartP(Fv(S, 0) + Fv(S, 1) + Fv(S, 2) + Fv(S, 3), 12); break; }
    case 0x78: { H("ESQRT"); StartP(SqrtF(std::fabs(Fv(S, int(fsf)))), 12); break; }
    case 0x79: { H("ERSQRT"); StartP(1.0f / SqrtF(std::fabs(Fv(S, int(fsf)))), 18); break; }
    case 0x7a: { H("ERCPR"); StartP(1.0f / Fv(S, int(fsf)), 12); break; }
    case 0x7b: H("WAITP"); if (s.p_busy) { s.P = s.p_next; s.p_busy = false; } break;
    case 0x7c: { H("ESIN"); StartP(std::sin(Fv(S, int(fsf))), 29); break; }
    case 0x7d: { H("EATAN"); StartP(std::atan(Fv(S, int(fsf))), 54); break; }
    case 0x7e: { H("EEXP"); StartP(std::exp(-Fv(S, int(fsf))), 44); break; }
    default: H("lower-special?"); stats.bad++; break;
    }
    return false;
}

/* ------------------------------------------------------------ Stage 10b: decoded fast path
 *
 * The micro memory is decoded once per content (keyed by a hash of the 16 KB,
 * looked up whenever an executed pair no longer matches its decoded words)
 * into one DP record per pair: an upper and a lower handler plus their
 * pre-extracted operands. Pairs whose two halves interact (the lower op reads
 * or writes the upper's target VF, or touches the flags / DIV unit) run the
 * reference sequence Upper/Lower/Commit (PairSlow), as do the rare ops.
 *
 * Flag liveness: an FMAC/CLIP result is queued for the lower unit (PushFlags)
 * only if some lower reader (op7 0x10-0x1c, DIV/SQRT/RSQRT) or the end of
 * the program could see it as the newest landed entry. It is skipped when the
 * next flag write follows in straight-line code and no reader lands in
 * between (see Liveness). mac_l/status_l/clip_l, which hold the sticky bits,
 * are always updated, so every entry that is queued has the same contents.
 */

struct DP;
using UpFn = void (*)(const DP&);
using LoFn = u32 (*)(const DP&);   // returns the branch target, or kNoBr
constexpr u32 kNoBr = ~0u;

struct alignas(16) DP {
    __m128i udmask;      // upper dest lanes as a blend mask
    __m128i ldmask;      // lower dest lanes
    UpFn up;
    LoFn lo;
    u16 uop, lop;        // switch-dispatch ids of up/lo (RunFast)
    V* udst;             // upper target (vf[fd], acc, or a sink for vf0)
    V* ldst;             // lower VF target (sink for vf0)
    const V* ua;         // upper fs
    const V* ub;         // upper ft
    u32 wlo, whi;        // the raw words (validation)
    u32 pc;              // byte address
    u32 limm;            // lower immediate / precomputed branch target / link
    u32 udm16;           // upper dest replicated over the 4 MAC groups
    u8 ubc, ufsf;        // bc lane
    u8 lis, lit, lid, lfs, lft, lfsf, lilane;
    u8 pre;              // 1 WAITQ, 2 WAITP stall before the upper op
    u8 ebit;
    u8 push;             // queue this pair's flag write (liveness)
    u8 is_push;          // upper writes flags (mac or clip)
    u8 is_event;         // lower reads / lands flags
    u8 leave;            // 1: control may leave after this pair (delay slot / end)
    u8 is_branch;
};

V g_sink;   // writes to vf0
u64 g_limit = u64(1) << 22;   // pair cap (DebugLimit)
int g_fast = -1;

inline __m128i DestMaskV(u32 dest) { return DestMask(dest); }
inline void Blend(V* d, __m128i v, __m128i m) {
    __m128i* p = reinterpret_cast<__m128i*>(d->u);
    _mm_storeu_si128(p, _mm_blendv_epi8(_mm_loadu_si128(p), v, m));
}

/* OutV with the MAC bits in one movemask: z, s, u, o packed to bytes, lanes
   reversed per group (x = bit 3). */
inline __m128i OutM(__m128 f, u32 dm16, u32* mac) {
    __m128i b = _mm_castps_si128(f);
    const __m128i sign = _mm_and_si128(b, Bits(0x80000000u));
    const __m128i e = _mm_and_si128(b, Bits(0x7f800000u));
    const __m128i mant0 = _mm_cmpeq_epi32(_mm_and_si128(b, Bits(0x7fffffu)), _mm_setzero_si128());
    const __m128i is_max = _mm_cmpeq_epi32(e, Bits(0x7f800000u));
    const __m128i is_den = _mm_andnot_si128(mant0, _mm_cmpeq_epi32(e, _mm_setzero_si128()));
    const __m128i pos_nan = _mm_andnot_si128(mant0, _mm_cmpeq_epi32(sign, _mm_setzero_si128()));
    const __m128i maxv = _mm_blendv_epi8(_mm_or_si128(sign, Bits(0x7f7fffffu)), Bits(0x7f7fffffu), pos_nan);
    b = _mm_blendv_epi8(b, maxv, is_max);
    b = _mm_blendv_epi8(b, sign, is_den);
    const __m128i z = _mm_cmpeq_epi32(_mm_and_si128(b, Bits(0x7fffffffu)), _mm_setzero_si128());
    const __m128i sg = _mm_srai_epi32(b, 31);
    const __m128i p = _mm_packs_epi16(_mm_packs_epi32(z, sg), _mm_packs_epi32(is_den, is_max));
    const __m128i rev = _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
    *mac = u32(_mm_movemask_epi8(_mm_shuffle_epi8(p, rev))) & dm16;
    return b;
}

inline __m128i OutN(__m128 f) { return OutV(f, 0, nullptr); }

/* PushFlags(mac, true, 0, false), with the queue write only when live. */
inline void MacWrite(const DP& d, u32 mac) {
    const u32 st = (mac & 0x000f ? 1u : 0u) | (mac & 0x00f0 ? 2u : 0u) | (mac & 0x0f00 ? 4u : 0u) | (mac & 0xf000 ? 8u : 0u);
    s.mac_l = mac;
    s.status_l = (s.status_l & 0xff0) | st | (st << 6);
    if (d.push) {
        if (s.fq_n == 16) {
            auto& w = s.fq[s.fq_head];
            s.mac = w.mac; s.status = w.status; s.clip = w.clip;
            s.fq_head = (s.fq_head + 1) & 15; s.fq_n--;
        }
        auto& w = s.fq[(s.fq_head + s.fq_n) & 15];
        w.ready = s.cycle + u64(flag_latency);
        w.mac = s.mac_l; w.status = s.status_l; w.clip = s.clip_l;
        s.fq_n++;
    }
}

enum BM { BVEC, BBC, BQ, BI };

template <int B>
inline __m128 OperandB(const DP& d) {
    if (B == BVEC) return InV(_mm_loadu_si128(reinterpret_cast<const __m128i*>(d.ub->u)));
    if (B == BBC) return _mm_set1_ps(In(d.ub->u[d.ubc]));
    if (B == BQ) return _mm_set1_ps(In(s.Q));
    return _mm_set1_ps(In(s.I));
}

template <int OP, int B>
__attribute__((always_inline)) inline void UpF(const DP& d) {
    const __m128 x = InV(_mm_loadu_si128(reinterpret_cast<const __m128i*>(d.ua->u)));
    const __m128 y = OperandB<B>(d);
    __m128 z;
    if (OP == ADD) z = _mm_add_ps(x, y);
    else if (OP == SUB) z = _mm_sub_ps(x, y);
    else if (OP == MUL) z = _mm_mul_ps(x, y);
    else if (OP == MADD || OP == MSUB) {
        const __m128 m = InV(OutN(_mm_mul_ps(x, y)));
        const __m128 acc = InV(_mm_loadu_si128(reinterpret_cast<const __m128i*>(s.acc.u)));
        z = OP == MADD ? _mm_add_ps(acc, m) : _mm_sub_ps(acc, m);
    } else if (OP == MAX) z = _mm_max_ps(x, y);
    else z = _mm_min_ps(x, y);
    if (OP == MAX || OP == MIN) {
        Blend(d.udst, OutN(z), d.udmask);
    } else {
        u32 mac;
        Blend(d.udst, OutM(z, d.udm16, &mac), d.udmask);
        MacWrite(d, mac);
    }
}

void UpNop(const DP&) {}

/* ITOF0/4/12/15: cvtdq2ps and the scale (both under the run's MXCSR, as
   float(int) * sc in Upper) */
template <int K>
__attribute__((always_inline)) inline void UpITOF(const DP& d) {
    static const float sc[4] = {1.0f, 1.0f / 16, 1.0f / 4096, 1.0f / 32768};
    const __m128 f = _mm_cvtepi32_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(d.ua->u)));
    Blend(d.udst, OutN(K ? _mm_mul_ps(f, _mm_set1_ps(sc[K])) : _mm_mul_ps(f, _mm_set1_ps(1.0f))), d.udmask);
}
/* FTOI0/4/12/15: the scale is a power of two, so the float product equals
   Upper's double product unless it saturates (>= 2^31; FLT_MAX under RZ). */
template <int K>
__attribute__((always_inline)) inline void UpFTOI(const DP& d) {
    static const float sc[4] = {1.0f, 16.0f, 4096.0f, 32768.0f};
    __m128 v = InV(_mm_loadu_si128(reinterpret_cast<const __m128i*>(d.ua->u)));
    if (K) v = _mm_mul_ps(v, _mm_set1_ps(sc[K]));
    __m128i iv = _mm_cvttps_epi32(v);
    const __m128i pos_sat = _mm_castps_si128(_mm_cmpge_ps(v, _mm_set1_ps(2147483648.0f)));
    iv = _mm_blendv_epi8(iv, Bits(0x7fffffffu), pos_sat);
    Blend(d.udst, iv, d.udmask);
}
/* CLIP fs.xyz, ft.w */
__attribute__((always_inline)) inline void UpCLIP(const DP& d) {
    const float lim = std::fabs(In(d.ub->u[3]));
    const __m128 v = InV(_mm_loadu_si128(reinterpret_cast<const __m128i*>(d.ua->u)));
    const u32 gt = u32(_mm_movemask_ps(_mm_cmpgt_ps(v, _mm_set1_ps(lim))));
    const u32 lt = u32(_mm_movemask_ps(_mm_cmplt_ps(v, _mm_set1_ps(-lim))));
    const u32 j = (gt & 1) | ((lt & 1) << 1) | ((gt & 2) << 1) | ((lt & 2) << 2) | ((gt & 4) << 2) | ((lt & 4) << 3);
    s.clip_l = ((s.clip_l << 6) | j) & 0xffffff;
    if (d.push) {
        if (s.fq_n == 16) {
            auto& w = s.fq[s.fq_head];
            s.mac = w.mac; s.status = w.status; s.clip = w.clip;
            s.fq_head = (s.fq_head + 1) & 15; s.fq_n--;
        }
        auto& w = s.fq[(s.fq_head + s.fq_n) & 15];
        w.ready = s.cycle + u64(flag_latency);
        w.mac = s.mac_l; w.status = s.status_l; w.clip = s.clip_l;
        s.fq_n++;
    }
}

/* the reference upper (rare ops), committed at once: only for pairs whose
   lower half does not interact with it */
void UpGeneric(const DP& d) {
    UpRes r;
    Upper(d.whi, r);
    if (r.target == 1) {
        if (r.reg != 0) StoreLanes(s.vf[r.reg], r.val, r.dest);
    } else if (r.target == 2) {
        StoreLanes(s.acc, r.val, r.dest);
    }
    if (r.set_mac || r.set_clip) {
        if (d.push) PushFlags(r.mac, r.set_mac, r.clip, r.set_clip);
        else {
            // PushFlags without the queue write
            if (r.set_mac) {
                u32 st = 0;
                if (r.mac & 0x000f) st |= 1;
                if (r.mac & 0x00f0) st |= 2;
                if (r.mac & 0x0f00) st |= 4;
                if (r.mac & 0xf000) st |= 8;
                s.mac_l = r.mac;
                s.status_l = (s.status_l & 0xff0) | st | (st << 6);
            }
            if (r.set_clip) s.clip_l = r.clip;
        }
    }
}

/* the whole pair in reference order (lower between upper compute and commit) */
u32 PairSlow(const DP& d) {
    UpRes r;
    Upper(d.whi, r);
    u32 t = 0;
    const bool br = Lower(d.wlo, d.pc, &t);
    Commit(r);
    return br ? t : kNoBr;
}

/* --- lower */
inline u8* QA(u32 q) { return reinterpret_cast<u8*>(uintptr_t(PS2_VU_BASE + g_data_off)) + ((q & g_q_mask) << 4); }
inline void LoadQV(const DP& d, u32 q) {
    Blend(d.ldst, _mm_loadu_si128(reinterpret_cast<const __m128i*>(QA(q))), d.ldmask);
}
inline void StoreQV(const DP& d, u32 q) {
    __m128i* p = reinterpret_cast<__m128i*>(QA(q));
    const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(s.vf[d.lfs].u));
    _mm_storeu_si128(p, _mm_blendv_epi8(_mm_loadu_si128(p), v, d.ldmask));
}

__attribute__((always_inline)) inline u32 LoNop(const DP&) { return kNoBr; }
/* flag readers: land what is due, then read (the same pair's upper write is
   queued with ready > cycle, so it is never seen, as in the reference order) */
inline void LandDue() { if (s.fq_n) LandFlags(false); }
__attribute__((always_inline)) inline u32 LoFCAND(const DP& d) { LandDue(); stats.clip_reads++; SetVi(1, (s.clip & d.limm) != 0); return kNoBr; }
__attribute__((always_inline)) inline u32 LoFCEQ(const DP& d) { LandDue(); stats.clip_reads++; SetVi(1, (s.clip & 0xffffff) == d.limm); return kNoBr; }
__attribute__((always_inline)) inline u32 LoFCOR(const DP& d) { LandDue(); stats.clip_reads++; SetVi(1, ((s.clip | d.limm) & 0xffffff) == 0xffffff); return kNoBr; }
__attribute__((always_inline)) inline u32 LoFCGET(const DP& d) { LandDue(); stats.clip_reads++; SetVi(d.lit, s.clip & 0xfff); return kNoBr; }
__attribute__((always_inline)) inline u32 LoFSAND(const DP& d) { LandDue(); stats.flag_reads++; SetVi(d.lit, s.status & d.limm); return kNoBr; }
__attribute__((always_inline)) inline u32 LoFSEQ(const DP& d) { LandDue(); stats.flag_reads++; SetVi(d.lit, (s.status & 0xfff) == d.limm); return kNoBr; }
__attribute__((always_inline)) inline u32 LoFSOR(const DP& d) { LandDue(); stats.flag_reads++; SetVi(d.lit, (s.status & 0xfff) | d.limm); return kNoBr; }
__attribute__((always_inline)) inline u32 LoFMAND(const DP& d) { LandDue(); stats.flag_reads++; SetVi(d.lit, s.mac & s.vi[d.lis]); return kNoBr; }
__attribute__((always_inline)) inline u32 LoFMEQ(const DP& d) { LandDue(); stats.flag_reads++; SetVi(d.lit, (s.mac & 0xffff) == s.vi[d.lis]); return kNoBr; }
__attribute__((always_inline)) inline u32 LoFMOR(const DP& d) { LandDue(); stats.flag_reads++; SetVi(d.lit, (s.mac & 0xffff) | s.vi[d.lis]); return kNoBr; }
__attribute__((always_inline)) inline u32 LoI(const DP& d) { s.I = d.wlo; return kNoBr; }
u32 LoSlow(const DP& d) { u32 t = 0; return Lower(d.wlo, d.pc, &t) ? t : kNoBr; }
__attribute__((always_inline)) inline u32 LoLQ(const DP& d) { LoadQV(d, u32(s.vi[d.lis] + d.limm)); return kNoBr; }
__attribute__((always_inline)) inline u32 LoSQ(const DP& d) { StoreQV(d, u32(s.vi[d.lit] + d.limm)); return kNoBr; }
__attribute__((always_inline)) inline u32 LoLQI(const DP& d) { LoadQV(d, s.vi[d.lis]); SetVi(d.lis, s.vi[d.lis] + 1); return kNoBr; }
__attribute__((always_inline)) inline u32 LoSQI(const DP& d) { StoreQV(d, s.vi[d.lit]); SetVi(d.lit, s.vi[d.lit] + 1); return kNoBr; }
__attribute__((always_inline)) inline u32 LoILW(const DP& d) {
    u32 v; std::memcpy(&v, QA(u32(s.vi[d.lis] + d.limm)) + 4 * d.lilane, 4); SetVi(d.lit, v); return kNoBr;
}
__attribute__((always_inline)) inline u32 LoISW(const DP& d) {
    u8* p = QA(u32(s.vi[d.lis] + d.limm));
    const u32 v = s.vi[d.lit];
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p),
                     _mm_blendv_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p)), Bits(v), d.ldmask));
    return kNoBr;
}
__attribute__((always_inline)) inline u32 LoIADDIU(const DP& d) { s.vi[d.lit] = u16(s.vi[d.lis] + d.limm); return kNoBr; }   // lit != 0
__attribute__((always_inline)) inline u32 LoISUBIU(const DP& d) { s.vi[d.lit] = u16(s.vi[d.lis] - d.limm); return kNoBr; }
__attribute__((always_inline)) inline u32 LoIADD(const DP& d) { s.vi[d.lid] = u16(s.vi[d.lis] + s.vi[d.lit]); return kNoBr; }  // lid != 0
__attribute__((always_inline)) inline u32 LoISUB(const DP& d) { s.vi[d.lid] = u16(s.vi[d.lis] - s.vi[d.lit]); return kNoBr; }
__attribute__((always_inline)) inline u32 LoIAND(const DP& d) { s.vi[d.lid] = u16(s.vi[d.lis] & s.vi[d.lit]); return kNoBr; }
__attribute__((always_inline)) inline u32 LoIOR(const DP& d) { s.vi[d.lid] = u16(s.vi[d.lis] | s.vi[d.lit]); return kNoBr; }
__attribute__((always_inline)) inline u32 LoIADDI(const DP& d) { s.vi[d.lit] = u16(s.vi[d.lis] + d.limm); return kNoBr; }
__attribute__((always_inline)) inline u32 LoMOVE(const DP& d) {
    Blend(d.ldst, _mm_loadu_si128(reinterpret_cast<const __m128i*>(s.vf[d.lfs].u)), d.ldmask); return kNoBr;
}
__attribute__((always_inline)) inline u32 LoMR32(const DP& d) {
    const __m128i v = _mm_shuffle_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(s.vf[d.lfs].u)), _MM_SHUFFLE(0, 3, 2, 1));
    Blend(d.ldst, v, d.ldmask); return kNoBr;
}
__attribute__((always_inline)) inline u32 LoMFIR(const DP& d) { Blend(d.ldst, Bits(u32(s32(std::int16_t(s.vi[d.lis])))), d.ldmask); return kNoBr; }
__attribute__((always_inline)) inline u32 LoMTIR(const DP& d) { SetVi(d.lit, s.vf[d.lfs].u[d.lfsf] & 0xffff); return kNoBr; }
__attribute__((always_inline)) inline u32 LoMFP(const DP& d) { Blend(d.ldst, Bits(s.P), d.ldmask); return kNoBr; }
__attribute__((always_inline)) inline u32 LoB(const DP& d) { return d.limm; }
__attribute__((always_inline)) inline u32 LoBAL(const DP& d) { SetVi(d.lit, (d.pc + 16) / 8); return d.limm; }
__attribute__((always_inline)) inline u32 LoJR(const DP& d) { return (u32(s.vi[d.lis]) * 8) & g_pc_mask; }
__attribute__((always_inline)) inline u32 LoJALR(const DP& d) { const u32 t = (u32(s.vi[d.lis]) * 8) & g_pc_mask; SetVi(d.lit, (d.pc + 16) / 8); return t; }
__attribute__((always_inline)) inline u32 LoIBEQ(const DP& d) { return s.vi[d.lit] == s.vi[d.lis] ? d.limm : kNoBr; }
__attribute__((always_inline)) inline u32 LoIBNE(const DP& d) { return s.vi[d.lit] != s.vi[d.lis] ? d.limm : kNoBr; }
__attribute__((always_inline)) inline u32 LoIBLTZ(const DP& d) { return std::int16_t(s.vi[d.lis]) < 0 ? d.limm : kNoBr; }
__attribute__((always_inline)) inline u32 LoIBGTZ(const DP& d) { return std::int16_t(s.vi[d.lis]) > 0 ? d.limm : kNoBr; }
__attribute__((always_inline)) inline u32 LoIBLEZ(const DP& d) { return std::int16_t(s.vi[d.lis]) <= 0 ? d.limm : kNoBr; }
__attribute__((always_inline)) inline u32 LoIBGEZ(const DP& d) { return std::int16_t(s.vi[d.lis]) >= 0 ? d.limm : kNoBr; }
__attribute__((always_inline)) inline u32 LoXGKICK(const DP& d) { Kick(s.vi[d.lis]); return kNoBr; }
__attribute__((always_inline)) inline u32 LoXTOP(const DP& d) { SetVi(d.lit, s.top); return kNoBr; }
__attribute__((always_inline)) inline u32 LoXITOP(const DP& d) { SetVi(d.lit, s.itop); return kNoBr; }

constexpr u32 kRd = 1, kWr = 2;   // lower VF access

/* Decode one upper word: handler, operands, whether it writes flags, and
   which VF register it targets (-1 none, 32 acc). Rare forms -> UpGeneric. */
void DecodeUpper(DP& d, u32 w, int* target, bool* flags) {
    const u32 dest = (w >> 21) & 15, ft = (w >> 16) & 31, fs = (w >> 11) & 31, fd = (w >> 6) & 31;
    const u32 op = w & 0x3f;
    d.ua = &s.vf[fs]; d.ub = &s.vf[ft]; d.ubc = u8(w & 3);
    d.udmask = DestMaskV(dest);
    d.udm16 = dest * 0x1111u;
    d.up = UpGeneric;
    *target = -1; *flags = false;
    auto set = [&](UpFn f, int tgt, bool fl) {
        d.up = f; *target = tgt; *flags = fl;
        d.udst = tgt == 32 ? &s.acc : tgt == 0 ? &g_sink : &s.vf[tgt];
    };
#define UPC(OPN, BMODE) UpF<OPN, BMODE>
    if (op < 0x3c) {
        if (op < 0x1c) {
            static const UpFn bc[7] = {UPC(ADD, BBC), UPC(SUB, BBC), UPC(MADD, BBC), UPC(MSUB, BBC), UPC(MAX, BBC), UPC(MIN, BBC), UPC(MUL, BBC)};
            const u32 k = op >> 2;
            set(bc[k], int(fd), k != 4 && k != 5);
            return;
        }
        switch (op) {
        case 0x1c: set(UPC(MUL, BQ), int(fd), true); return;
        case 0x1d: set(UPC(MAX, BI), int(fd), false); return;
        case 0x1e: set(UPC(MUL, BI), int(fd), true); return;
        case 0x1f: set(UPC(MIN, BI), int(fd), false); return;
        case 0x20: set(UPC(ADD, BQ), int(fd), true); return;
        case 0x21: set(UPC(MADD, BQ), int(fd), true); return;
        case 0x22: set(UPC(ADD, BI), int(fd), true); return;
        case 0x23: set(UPC(MADD, BI), int(fd), true); return;
        case 0x24: set(UPC(SUB, BQ), int(fd), true); return;
        case 0x25: set(UPC(MSUB, BQ), int(fd), true); return;
        case 0x26: set(UPC(SUB, BI), int(fd), true); return;
        case 0x27: set(UPC(MSUB, BI), int(fd), true); return;
        case 0x28: set(UPC(ADD, BVEC), int(fd), true); return;
        case 0x29: set(UPC(MADD, BVEC), int(fd), true); return;
        case 0x2a: set(UPC(MUL, BVEC), int(fd), true); return;
        case 0x2b: set(UPC(MAX, BVEC), int(fd), false); return;
        case 0x2c: set(UPC(SUB, BVEC), int(fd), true); return;
        case 0x2d: set(UPC(MSUB, BVEC), int(fd), true); return;
        case 0x2e: *target = int(fd); *flags = true; return;              // OPMSUB (generic)
        case 0x2f: set(UPC(MIN, BVEC), int(fd), false); return;
        default: *target = 99; return;                                  // bad: generic
        }
    }
    const u32 sp = (w & 3) | ((w >> 4) & 0x7c);
    if (sp < 0x10 || (sp >= 0x18 && sp < 0x1c)) {
        static const UpFn bc[7] = {UPC(ADD, BBC), UPC(SUB, BBC), UPC(MADD, BBC), UPC(MSUB, BBC), UPC(ADD, BBC), UPC(ADD, BBC), UPC(MUL, BBC)};
        set(bc[sp >> 2], 32, true);
        return;
    }
    switch (sp) {
    case 0x1c: set(UPC(MUL, BQ), 32, true); return;
    case 0x1e: set(UPC(MUL, BI), 32, true); return;
    case 0x20: set(UPC(ADD, BQ), 32, true); return;
    case 0x21: set(UPC(MADD, BQ), 32, true); return;
    case 0x22: set(UPC(ADD, BI), 32, true); return;
    case 0x23: set(UPC(MADD, BI), 32, true); return;
    case 0x24: set(UPC(SUB, BQ), 32, true); return;
    case 0x25: set(UPC(MSUB, BQ), 32, true); return;
    case 0x26: set(UPC(SUB, BI), 32, true); return;
    case 0x27: set(UPC(MSUB, BI), 32, true); return;
    case 0x28: set(UPC(ADD, BVEC), 32, true); return;
    case 0x29: set(UPC(MADD, BVEC), 32, true); return;
    case 0x2a: set(UPC(MUL, BVEC), 32, true); return;
    case 0x2c: set(UPC(SUB, BVEC), 32, true); return;
    case 0x2d: set(UPC(MSUB, BVEC), 32, true); return;
    case 0x10: set(UpITOF<0>, int(ft), false); return;
    case 0x11: set(UpITOF<1>, int(ft), false); return;
    case 0x12: set(UpITOF<2>, int(ft), false); return;
    case 0x13: set(UpITOF<3>, int(ft), false); return;
    case 0x14: set(UpFTOI<0>, int(ft), false); return;
    case 0x15: set(UpFTOI<1>, int(ft), false); return;
    case 0x16: set(UpFTOI<2>, int(ft), false); return;
    case 0x17: set(UpFTOI<3>, int(ft), false); return;
    case 0x1d: *target = int(ft); return;                              // ABS (generic)
    case 0x1f: d.up = UpCLIP; *flags = true; return;                     // CLIP
    case 0x2e: *target = 32; *flags = true; return;                      // OPMULA (generic)
    case 0x2f: d.up = UpNop; return;                                     // NOP
    default: *target = 99; return;
    }
#undef UPC
}

/* Decode one lower word. *vf: lower VF register accesses per index (kRd/kWr),
   *slow: needs the reference order, *event: lands flags. */
void DecodeLower(DP& d, u32 w, u32 pc, u8* vf, bool* slow, bool* event) {
    const u32 op7 = w >> 25;
    const u32 dest = (w >> 21) & 15, ft = (w >> 16) & 31, fs = (w >> 11) & 31, fd = (w >> 6) & 31;
    const u32 it = ft & 15, is = fs & 15, id = fd & 15;
    d.lis = u8(is); d.lit = u8(it); d.lid = u8(id); d.lfs = u8(fs); d.lft = u8(ft);
    d.lfsf = u8((w >> 21) & 3);
    d.ldmask = DestMaskV(dest);
    d.ldst = ft ? &s.vf[ft] : &g_sink;
    d.lilane = 0;
    for (int l = 0; l < 4; ++l) if (Lane(dest, l)) { d.lilane = u8(l); break; }
    const u32 imm11 = u32(Imm11(w));
    const u32 btgt = (pc + 8 + imm11 * 8) & g_pc_mask;
    d.lo = LoSlow;
    *slow = false; *event = false;
    auto rd = [&](u32 r) { vf[r] |= kRd; };
    auto wr = [&](u32 r) { vf[r] |= kWr; };
    auto all = [&] { for (int i = 0; i < 32; ++i) vf[i] = kRd | kWr; };
    if (op7 >= 0x10 && op7 <= 0x1c) {
        *event = true;
        const u32 fimm = ((w >> 10) & 0x800) | (w & 0x7ff);
        switch (op7) {
        case 0x10: d.lo = LoFCEQ; d.limm = w & 0xffffff; return;
        case 0x12: d.lo = LoFCAND; d.limm = w & 0xffffff; return;
        case 0x13: d.lo = LoFCOR; d.limm = w & 0xffffff; return;
        case 0x14: d.lo = LoFSEQ; d.limm = fimm; return;
        case 0x16: d.lo = LoFSAND; d.limm = fimm; return;
        case 0x17: d.lo = LoFSOR; d.limm = fimm; return;
        case 0x18: d.lo = LoFMEQ; return;
        case 0x1a: d.lo = LoFMAND; return;
        case 0x1b: d.lo = LoFMOR; return;
        case 0x1c: d.lo = LoFCGET; return;
        default: *slow = true; return;   // FCSET/FSSET write the flags: reference order
        }
    }
    switch (op7) {
    case 0x00: d.lo = LoLQ; d.limm = imm11; wr(ft); return;
    case 0x01: d.lo = LoSQ; d.limm = imm11; rd(fs); return;
    case 0x04: d.limm = imm11; d.lo = dest ? LoILW : LoNop; return;
    case 0x05: d.limm = imm11; d.lo = LoISW; return;
    case 0x08: d.limm = ((w >> 10) & 0x7800) | (w & 0x7ff); d.lo = it ? LoIADDIU : LoNop; return;
    case 0x09: d.limm = ((w >> 10) & 0x7800) | (w & 0x7ff); d.lo = it ? LoISUBIU : LoNop; return;
    case 0x20: d.lo = LoB; d.limm = btgt; d.is_branch = 1; return;
    case 0x21: d.lo = LoBAL; d.limm = btgt; d.is_branch = 1; return;
    case 0x24: d.lo = LoJR; d.is_branch = 1; return;
    case 0x25: d.lo = LoJALR; d.is_branch = 1; return;
    case 0x28: d.lo = LoIBEQ; d.limm = btgt; d.is_branch = 1; return;
    case 0x29: d.lo = LoIBNE; d.limm = btgt; d.is_branch = 1; return;
    case 0x2c: d.lo = LoIBLTZ; d.limm = btgt; d.is_branch = 1; return;
    case 0x2d: d.lo = LoIBGTZ; d.limm = btgt; d.is_branch = 1; return;
    case 0x2e: d.lo = LoIBLEZ; d.limm = btgt; d.is_branch = 1; return;
    case 0x2f: d.lo = LoIBGEZ; d.limm = btgt; d.is_branch = 1; return;
    case 0x40: break;
    default: all(); *slow = true; return;   // undecoded: reference path counts it
    }
    const u32 sub = w & 0x3f;
    if (sub < 0x3c) {
        switch (sub) {
        case 0x30: d.lo = id ? LoIADD : LoNop; return;
        case 0x31: d.lo = id ? LoISUB : LoNop; return;
        case 0x32: d.limm = u32(s32(fd << 27) >> 27); d.lo = it ? LoIADDI : LoNop; return;
        case 0x34: d.lo = id ? LoIAND : LoNop; return;
        case 0x35: d.lo = id ? LoIOR : LoNop; return;
        default: all(); *slow = true; return;
        }
    }
    const u32 sp = (w & 3) | ((w >> 4) & 0x7c);
    switch (sp) {
    case 0x30: d.lo = ft ? LoMOVE : LoNop; rd(fs); wr(ft); return;
    case 0x31: d.lo = ft ? LoMR32 : LoNop; rd(fs); wr(ft); return;
    case 0x34: d.lo = LoLQI; wr(ft); return;
    case 0x35: d.lo = LoSQI; rd(fs); return;
    case 0x38: case 0x39: case 0x3a:   // DIV/SQRT/RSQRT: lands flags, writes I/D
        /* Direct order is equivalent: the I/D bits of queued entries are
           masked when they land, and the upper's status_l update commutes
           with DivFlags (disjoint bits apart from the sticky OR). */
        d.lo = LoSlow; *event = true; rd(fs); rd(ft); return;
    case 0x3b: d.pre = 1; d.lo = LoNop; return;      // WAITQ (stalled before the upper)
    case 0x7b: d.pre = 2; d.lo = LoNop; return;      // WAITP
    case 0x3c: d.lo = LoMTIR; rd(fs); return;
    case 0x3d: d.lo = ft ? LoMFIR : LoNop; wr(ft); return;
    case 0x64: d.lo = ft ? LoMFP : LoNop; wr(ft); return;
    case 0x68: d.lo = LoXTOP; return;
    case 0x69: d.lo = LoXITOP; return;
    case 0x6c: d.lo = LoXGKICK; return;
    case 0x36: case 0x37:                       // LQD/SQD
    case 0x3e: case 0x3f:                       // ILWR/ISWR
    case 0x40: case 0x41: case 0x42: case 0x43: // R*
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76:
    case 0x78: case 0x79: case 0x7a: case 0x7c: case 0x7d: case 0x7e:
        d.lo = LoSlow; rd(fs); rd(ft); wr(ft); return;
    default: all(); *slow = true; return;
    }
}

#define LO_LIST(X) X(LoNop) X(LoSlow) X(LoFCAND) X(LoFCEQ) X(LoFCOR) X(LoFCGET) X(LoFSAND) X(LoFSEQ) X(LoFSOR) X(LoFMAND) X(LoFMEQ) X(LoFMOR) \
    X(LoI) \
    X(LoLQ) \
    X(LoSQ) \
    X(LoLQI) \
    X(LoSQI) \
    X(LoILW) \
    X(LoISW) \
    X(LoIADDIU) \
    X(LoISUBIU) \
    X(LoIADD) \
    X(LoISUB) \
    X(LoIAND) \
    X(LoIOR) \
    X(LoIADDI) \
    X(LoMOVE) \
    X(LoMR32) \
    X(LoMFIR) \
    X(LoMTIR) \
    X(LoMFP) \
    X(LoB) \
    X(LoBAL) \
    X(LoJR) \
    X(LoJALR) \
    X(LoIBEQ) \
    X(LoIBNE) \
    X(LoIBLTZ) \
    X(LoIBGTZ) \
    X(LoIBLEZ) \
    X(LoIBGEZ) \
    X(LoXGKICK) \
    X(LoXTOP) \
    X(LoXITOP) \
    X(PairSlow)
#define UP_LIST(X) \
    X(ADD, BVEC) X(ADD, BBC) X(ADD, BQ) X(ADD, BI) X(SUB, BVEC) X(SUB, BBC) X(SUB, BQ) X(SUB, BI) \
    X(MUL, BVEC) X(MUL, BBC) X(MUL, BQ) X(MUL, BI) X(MADD, BVEC) X(MADD, BBC) X(MADD, BQ) X(MADD, BI) \
    X(MSUB, BVEC) X(MSUB, BBC) X(MSUB, BQ) X(MSUB, BI) X(MAX, BVEC) X(MAX, BBC) X(MAX, BQ) X(MAX, BI) \
    X(MIN, BVEC) X(MIN, BBC) X(MIN, BQ) X(MIN, BI)
enum LoId {
#define X(n) ID_##n,
    LO_LIST(X)
#undef X
};
constexpr u16 UpId(int op, int b) { return u16(2 + op * 4 + b); }

void AssignIds(DP& d) {
    d.uop = 0xffff;
    if (d.up == UpNop) d.uop = 0;
    else if (d.up == UpGeneric) d.uop = 1;
    else if (d.up == UpCLIP) d.uop = 30;
    else if (d.up == static_cast<UpFn>(UpITOF<0>)) d.uop = 31;
    else if (d.up == static_cast<UpFn>(UpITOF<1>)) d.uop = 32;
    else if (d.up == static_cast<UpFn>(UpITOF<2>)) d.uop = 33;
    else if (d.up == static_cast<UpFn>(UpITOF<3>)) d.uop = 34;
    else if (d.up == static_cast<UpFn>(UpFTOI<0>)) d.uop = 35;
    else if (d.up == static_cast<UpFn>(UpFTOI<1>)) d.uop = 36;
    else if (d.up == static_cast<UpFn>(UpFTOI<2>)) d.uop = 37;
    else if (d.up == static_cast<UpFn>(UpFTOI<3>)) d.uop = 38;
#define X(o, b) else if (d.up == static_cast<UpFn>(UpF<o, b>)) d.uop = UpId(o, b);
    UP_LIST(X)
#undef X
    d.lop = 0xffff;
#define X(n) if (d.lo == static_cast<LoFn>(n)) d.lop = ID_##n;
    LO_LIST(X)
#undef X
}

struct Prog {
    u32 words[4096];
    DP dp[2048];
};

void Decode(Prog& p, const u8* micro) {
    std::memcpy(p.words, micro, sizeof p.words);
    for (u32 i = 0; i < 2048; ++i) {
        DP& d = p.dp[i];
        std::memset(static_cast<void*>(&d), 0, sizeof d);
        const u32 lo = p.words[2 * i], hi = p.words[2 * i + 1];
        d.wlo = lo; d.whi = hi; d.pc = i * 8;
        d.ebit = (hi >> 30) & 1;
        int tgt; bool fl;
        DecodeUpper(d, hi, &tgt, &fl);
        d.is_push = fl;
        if (hi & 0x80000000u) { d.lo = LoI; continue; }
        u8 vf[32] = {};
        bool slow, ev;
        DecodeLower(d, lo, i * 8, vf, &slow, &ev);
        d.is_event = ev;
        if (tgt == 99) slow = true;
        if (tgt >= 0 && tgt < 32 && vf[tgt]) slow = true;   // lower reads/writes the upper's target
        if (slow) { d.up = UpNop; d.lo = PairSlow; }
    }
    for (u32 i = 0; i < 2048; ++i) AssignIds(p.dp[i]);
    // control may leave after a branch's delay slot and after the pair that follows an E bit
    const u32 pm = g_pc_mask >> 3;   // pairs - 1 of this unit (VU1 2047, VU0 511)
    for (u32 i = 0; i <= pm; ++i) {
        if (p.dp[i].is_branch || p.dp[i].ebit) p.dp[(i + 1) & pm].leave = 1;
    }
    /* Liveness: the flag write at p is dead if the next write q follows in
       straight-line code (no leave in [p, q+lat-2]) and no event lands in
       [p+lat, q+lat-1]. Slow pairs always push (reference Commit). */
    const int lat = flag_latency;
    for (u32 i = 0; i <= pm; ++i) {
        DP& d = p.dp[i];
        if (!d.is_push) continue;
        d.push = 1;
        if (d.lo == PairSlow) continue;
        for (int k = 1; k < 32; ++k) {
            if (p.dp[(i + u32(k) - 1) & pm].leave) break;   // control may not reach offset k
            if (!p.dp[(i + u32(k)) & pm].is_push) continue;
            // the next write q is at offset k
            bool ok = true;
            for (int j = 0; j <= k + lat - 2 && ok; ++j) if (p.dp[(i + u32(j)) & pm].leave) ok = false;
            for (int j = lat; j <= k + lat - 1 && ok; ++j) if (p.dp[(i + u32(j)) & pm].is_event) ok = false;
            if (ok) d.push = 0;
            break;
        }
    }
}

std::map<u64, Prog*> g_progs;
Prog* g_prog_unit[2] = {nullptr, nullptr};   // the decoded program last run on VU1 [0] / VU0 [1]

u64 HashMicro(const u8* m) {
    u64 h = 1469598103934665603ull;
    const u64* w = reinterpret_cast<const u64*>(m);
    for (int i = 0; i < 2048; ++i) { h ^= w[i]; h *= 1099511628211ull; h ^= h >> 29; }
    return h;
}

Prog* Lookup(const u8* micro) {
    const u64 h = HashMicro(micro);
    auto it = g_progs.find(h);
    if (it != g_progs.end()) {
        if (!std::memcmp(it->second->words, micro, sizeof it->second->words)) return it->second;
        Decode(*it->second, micro);   // collision: re-decode in place
        return it->second;
    }
    if (g_progs.size() >= 64) {
        for (auto& kv : g_progs) delete kv.second;
        g_progs.clear();
        g_prog_unit[0] = g_prog_unit[1] = nullptr;   // the caller re-points its own
    }
    Prog* p = new Prog;
    Decode(*p, micro);
    g_progs[h] = p;
    return p;
}

u64 RunFast(u32& pc) {
    const u8* micro = Micro();
    const u32* mw = reinterpret_cast<const u32*>(micro);
    Prog*& g_prog = g_prog_unit[g_data_off != 0xC000];
    const u32 pcm = g_pc_mask;
    if (!g_prog) g_prog = Lookup(micro);
    const DP* P = g_prog->dp;
    s.vf[0].u[0] = s.vf[0].u[1] = s.vf[0].u[2] = 0; s.vf[0].f[3] = 1.0f;
    u64 n = 0;
    bool branch = false, ending = false;
    u32 btarget = 0;
    const u64 limit = g_limit;
    for (; n < limit; ++n) {
        const DP* d = &P[pc >> 3];
        if (__builtin_expect(d->wlo != mw[pc >> 2] || d->whi != mw[(pc >> 2) + 1], 0)) {
            g_prog = Lookup(micro);
            P = g_prog->dp;
            d = &P[pc >> 3];
        }
        s.cycle++;
        if (s.q_busy | s.p_busy) LandQP(false);
        pc = (pc + 8) & pcm;
        const bool take_branch = branch;
        const u32 target = btarget;
        if (d->pre) {
            if (d->pre == 1 && s.q_busy) { s.Q = s.q_next; s.q_busy = false; }
            if (d->pre == 2 && s.p_busy) { s.P = s.p_next; s.p_busy = false; }
        }
        switch (d->uop) {
        case 0: break;
        case 1: UpGeneric(*d); break;
        case 30: UpCLIP(*d); break;
        case 31: UpITOF<0>(*d); break;
        case 32: UpITOF<1>(*d); break;
        case 33: UpITOF<2>(*d); break;
        case 34: UpITOF<3>(*d); break;
        case 35: UpFTOI<0>(*d); break;
        case 36: UpFTOI<1>(*d); break;
        case 37: UpFTOI<2>(*d); break;
        case 38: UpFTOI<3>(*d); break;
#define X(o, b) case UpId(o, b): UpF<o, b>(*d); break;
        UP_LIST(X)
#undef X
        default: __builtin_unreachable();
        }
        u32 bt;
        switch (d->lop) {
#define X(n) case ID_##n: bt = n(*d); break;
        LO_LIST(X)
#undef X
        default: __builtin_unreachable();
        }
        branch = bt != kNoBr;
        btarget = bt;
        if (take_branch) pc = target;
        if (ending) { ++n; break; }
        if (d->ebit) ending = true;
    }
    s.vf[0].u[0] = s.vf[0].u[1] = s.vf[0].u[2] = 0; s.vf[0].f[3] = 1.0f;
    return n;
}

}  // namespace

/* ------------------------------------------------------------ API */

void SetKick(KickFn fn) { kick_fn = fn; }

void LoadRegs(const Vu1CapRegs& r) {
    std::memcpy(s.vf, r.vf, sizeof s.vf);
    std::memcpy(s.acc.u, r.acc, 16);
    for (int i = 0; i < 16; ++i) s.vi[i] = u16(r.vi[i]);
    s.I = r.I; s.Q = r.Q; s.P = r.P; s.R = r.R;
    s.mac = s.mac_l = r.mac; s.status = s.status_l = r.status; s.clip = s.clip_l = r.clip;
    s.fq_n = 0; s.q_busy = s.p_busy = false;
}
void SaveRegs(Vu1CapRegs& r) {
    std::memset(&r, 0, sizeof r);
    std::memcpy(r.vf, s.vf, sizeof r.vf);
    std::memcpy(r.acc, s.acc.u, 16);
    for (int i = 0; i < 16; ++i) r.vi[i] = s.vi[i];
    r.I = s.I; r.Q = s.Q; r.P = s.P; r.R = s.R;
    r.mac = s.mac; r.status = s.status; r.clip = s.clip;
}
u32 Resume() { return s.resume; }
void DebugSet(int fast, unsigned long long limit) { g_fast = fast; g_limit = limit ? limit : (u64(1) << 22); }
Stats& GetStats() { return stats; }

void DumpHistogram() {
    if (!hist_on) return;
    xsg_logf("[vu1] instruction histogram (%zu kinds):\n", hist.size());
    for (auto& [k, v] : hist) xsg_logf("[vu1]   %-14s %llu\n", k.c_str(), (unsigned long long)v);
}

u64 Run(u32 pc, u32 top, u32 itop) {
    if (flag_latency < 0) {
        const char* e = xsg_cfg("debug.vu_flaglat");
        flag_latency = e ? std::atoi(e) : 4;
        hist_on = xsg_cfg("debug.vu_hist") && std::atoi(xsg_cfg("debug.vu_hist"));
        s.vf[0].f[3] = 1.0f;
    }
    stats.runs++;
    static long trace_runs = xsg_cfg("debug.vu_trace") ? std::atol(xsg_cfg("debug.vu_trace")) : 0;
    const bool trace_left = trace_runs > 0;
    if (trace_runs > 0) { trace_runs--; xsg_logf("[vu1t] run at 0x%x top %x itop %x\n", pc, top, itop); }
    s.top = top; s.itop = itop;
    const bool cap = g_data_off == 0xC000 && CapOn();
    if (cap) {
        Vu1CapRegs r;
        SaveRegs(r);
        CapBegin(pc & g_pc_mask, top, itop, r, Micro(), Data());
    }
    const unsigned csr = _mm_getcsr();
    _mm_setcsr((csr & ~0x6000u) | 0x6000u | 0x8040u);   // round toward zero, FTZ, DAZ
    pc &= g_pc_mask;
    /* Flags are observed only by lower ops (Lower lands them first), DIV's
       status write and the end of the run. With fewer than 16 pairs of
       latency every queued entry has landed before the queue can overflow,
       so landing on demand gives exactly the per-cycle values. */
    const bool lazy_flags = flag_latency < 16;
    if (g_fast < 0) g_fast = !xsg_cfg("debug.vu_fast") || std::atoi(xsg_cfg("debug.vu_fast"));
    const bool fast_on = g_fast;
    u64 n = 0;
    const u64 limit = g_limit;
    if (fast_on && flag_latency >= 1 && lazy_flags && !trace_left && !hist_on) {
        n = RunFast(pc);
    } else {
    bool branch = false, ending = false;
    u32 btarget = 0;
    const u8* micro = Micro();
    for (; n < limit; ++n) {
        s.cycle++;
        if (!lazy_flags && s.fq_n && s.fq[s.fq_head].ready <= s.cycle) LandFlags(false);
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

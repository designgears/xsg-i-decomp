// IPU model for the PC port (clean-room, written for this port; MIT like the rest of port/).
//
// The PS2 IPU sits between DMA channel 4 (toIPU, a chain-mode source) and channel 3
// (fromIPU, normal mode). The game drives it through IPU_CMD/IPU_CTRL/IPU_BP at
// 0x10002000 and the D3/D4 registers, all plain memory on the host. The game code calls
// xsg_ipu_reg_written() (under PORT_HOST) after each write to IPU_CMD, IPU_CTRL, D3_CHCR
// and D4_CHCR, and sceIpuSync() (sdk.c) asks xsg_ipu_busy().
//
// Implemented: BCLR, IDEC (RGB32 output with CSC and the SETTH alpha rule), FDEC, SETIQ,
// SETVQ (stored), SETTH, CSC is not used by the game and is logged. BDEC/VDEC are logged
// (the game only reaches them through libmpeg, which the port replaces, see mpeg.cpp).
//
// Input is pulled lazily from channel 4: the FIFO holds only qwords the decoder asked for,
// so IPU_BP's IFC/BP describe exactly how far the bitstream has been read, which is what
// xglMoviePlay uses to rewind D4_MADR between frames. IDEC decodes macroblock by macroblock
// into an output queue that is drained into channel 3 whenever it runs; if the input runs
// dry mid-macroblock the decoder rewinds to the macroblock start and waits for more DMA.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <vector>

extern "C" void xsg_logf(const char* fmt, ...);
extern "C" volatile unsigned xsg_frame;

namespace {

typedef uint32_t u32;
typedef uint8_t u8;

inline volatile u32& R(u32 a) { return *reinterpret_cast<volatile u32*>(uintptr_t(a)); }

constexpr u32 IPU_CMD = 0x10002000, IPU_CTRL = 0x10002010, IPU_BP = 0x10002020, IPU_TOP = 0x10002030;
constexpr u32 D3_CHCR = 0x1000b000, D3_MADR = 0x1000b010, D3_QWC = 0x1000b020;
constexpr u32 D4_CHCR = 0x1000b400, D4_MADR = 0x1000b410, D4_QWC = 0x1000b420, D4_TADR = 0x1000b430;

// DMA addresses: bit 31 with 0x8 in the top nibble selects scratchpad (SPR bit); anything else
// is used as is (EE RAM is mapped at its PS2 address; tags may live on host thread stacks).
u8* Mem(u32 a) {
    if ((a >> 28) == 8) return reinterpret_cast<u8*>(uintptr_t(0x70000000u + (a & 0x3ff0)));
    return reinterpret_cast<u8*>(uintptr_t(a));
}

bool log_on() {
    static int v = std::getenv("XSG_IPU_LOG") ? std::atoi(std::getenv("XSG_IPU_LOG")) : 0;
    return v != 0;
}

// ---- DMA channel 4 (toIPU, chain source) --------------------------------------------
struct Ch4 {
    bool active = false;
    bool end_after = false;   // current transfer is the last (REFE/END tag or TAG ID 0/7)
    u32 madr = 0, qwc = 0, tadr = 0, chcr = 0;
    void writeback() {
        R(D4_MADR) = madr;
        R(D4_QWC) = qwc;
        R(D4_TADR) = tadr;
        R(D4_CHCR) = (chcr & ~0x100u) | (active ? 0x100u : 0u);
    }
    void start() {
        chcr = R(D4_CHCR);
        madr = R(D4_MADR);
        qwc = R(D4_QWC) & 0xffff;
        tadr = R(D4_TADR);
        active = true;
        u32 id = (chcr >> 28) & 7;
        end_after = (qwc > 0) && (id == 0 || id == 7);
        if (((chcr >> 2) & 3) != 1) end_after = true;   // normal mode: just MADR/QWC
    }
    // One qword into dst; false when the channel has nothing more.
    bool pull(u8* dst) {
        while (active && qwc == 0) {
            if (end_after || tadr == 0) { active = false; break; }
            const u32* t = reinterpret_cast<const u32*>(Mem(tadr));
            u32 tag = t[0], addr = t[1];
            u32 id = (tag >> 28) & 7;
            if (log_on()) xsg_logf("[ipu] ch4 tag @%08x: %08x %08x\n", tadr, tag, addr);
            qwc = tag & 0xffff;
            chcr = (chcr & 0xffff) | (tag & 0xffff0000u);
            switch (id) {
            case 0: madr = addr; tadr += 16; end_after = true; break;           // refe
            case 1: madr = tadr + 16; tadr = madr + qwc * 16; break;            // cnt
            case 2: madr = tadr + 16; tadr = addr; break;                       // next
            case 3: case 4: madr = addr; tadr += 16; break;                     // ref/refs
            case 7: madr = tadr + 16; end_after = true; break;                  // end
            default: xsg_logf("[ipu] ch4: unsupported tag id %u\n", id); active = false; break;
            }
        }
        if (!active) { writeback(); return false; }
        std::memcpy(dst, Mem(madr), 16);
        madr += 16;
        if (--qwc == 0 && end_after) active = false;
        writeback();
        return true;
    }
} ch4;

// ---- DMA channel 3 (fromIPU) ------------------------------------------------------------
struct Ch3 {
    bool active = false;
    u32 madr = 0, qwc = 0;
} ch3;

std::deque<u8> outq;  // decoded output, drained into ch3 in qword units

void drain() {
    while (ch3.active && ch3.qwc > 0 && outq.size() >= 16) {
        u8* d = Mem(ch3.madr);
        for (int i = 0; i < 16; ++i) { d[i] = outq.front(); outq.pop_front(); }
        ch3.madr = ((ch3.madr >> 28) == 8) ? (((ch3.madr + 16) & 0x3fff) | 0x80000000u) : ch3.madr + 16;
        --ch3.qwc;
    }
    if (ch3.active && ch3.qwc == 0) ch3.active = false;
    R(D3_MADR) = ch3.madr;
    R(D3_QWC) = ch3.qwc;
    R(D3_CHCR) = ch3.active ? (R(D3_CHCR) | 0x100u) : (R(D3_CHCR) & ~0x100u);
}

// ---- input FIFO / bit reader ------------------------------------------------------------
struct Starved {};

std::vector<u8> fifo;  // bytes of whole qwords pulled from ch4, fifo[0] is the oldest
u32 bitpos = 0;        // bit offset into fifo (the first qword holds BP)

void fifo_clear(u32 bp) { fifo.clear(); bitpos = bp & 0x7f; }
void refill();

void ensure(u32 nbits) {
    while (bitpos + nbits > fifo.size() * 8) {
        u8 q[16];
        if (!ch4.pull(q)) throw Starved();
        fifo.insert(fifo.end(), q, q + 16);
    }
}
u32 peek(u32 n) {  // n <= 32
    if (n == 0) return 0;
    ensure(n);
    uint64_t v = 0;
    u32 byte = bitpos >> 3, sh = bitpos & 7;
    for (int i = 0; i < 5; ++i) v = (v << 8) | (byte + i < fifo.size() ? fifo[byte + i] : 0);
    v <<= sh;
    return u32((v >> (40 - n)) & ((n == 32) ? 0xffffffffull : ((1ull << n) - 1)));
}
void skip(u32 n) {
    ensure(n);
    bitpos += n;
}
u32 get(u32 n) { u32 v = peek(n); bitpos += n; return v; }
// The real DMAC keeps the 8-qword input FIFO full while the channel runs, so D4_MADR is
// ahead of the decoder by IFC qwords (xglMoviePlay/checkdma rely on that).
void refill() {
    while (fifo.size() < 8 * 16) {
        u8 q[16];
        if (!ch4.pull(q)) break;
        fifo.insert(fifo.end(), q, q + 16);
    }
}
void compact() {  // drop fully consumed qwords
    u32 q = bitpos / 128;
    if (q) { fifo.erase(fifo.begin(), fifo.begin() + q * 16); bitpos -= q * 128; }
    refill();
}
void update_bp() {
    compact();
    u32 ifc = u32(fifo.size() / 16);
    if (ifc > 15) ifc = 15;
    R(IPU_BP) = (bitpos & 0x7f) | (ifc << 8);
}

// ---- tables (ISO/IEC 13818-2 annex B) ----------------------------------------------------
struct Vlc { const char* code; int run, level; };  // level 0 = EOB, -1 = escape
const Vlc kB14[] = {
    {"10", 0, 0}, {"11", 0, 1}, {"011", 1, 1}, {"0100", 0, 2}, {"0101", 2, 1}, {"00101", 0, 3},
    {"00111", 3, 1}, {"00110", 4, 1}, {"000110", 1, 2}, {"000111", 5, 1}, {"000101", 6, 1},
    {"000100", 7, 1}, {"0000110", 0, 4}, {"0000100", 2, 2}, {"0000111", 8, 1}, {"0000101", 9, 1},
    {"000001", 0, -1}, {"00100110", 0, 5}, {"00100001", 0, 6}, {"00100101", 1, 3},
    {"00100100", 3, 2}, {"00100111", 10, 1}, {"00100011", 11, 1}, {"00100010", 12, 1},
    {"00100000", 13, 1}, {"0000001010", 0, 7}, {"0000001100", 1, 4}, {"0000001011", 2, 3},
    {"0000001111", 4, 2}, {"0000001001", 5, 2}, {"0000001110", 14, 1}, {"0000001101", 15, 1},
    {"0000001000", 16, 1},
    {"000000011101", 0, 8}, {"000000011000", 0, 9}, {"000000010011", 0, 10}, {"000000010000", 0, 11},
    {"000000011011", 1, 5}, {"000000010100", 2, 4}, {"000000011100", 3, 3}, {"000000010010", 4, 3},
    {"000000011110", 6, 2}, {"000000010101", 7, 2}, {"000000010001", 8, 2}, {"000000011111", 17, 1},
    {"000000011010", 18, 1}, {"000000011001", 19, 1}, {"000000010111", 20, 1}, {"000000010110", 21, 1},
    {nullptr, 0, 0}};
const Vlc kB15[] = {
    {"0110", 0, 0}, {"10", 0, 1}, {"010", 1, 1}, {"110", 0, 2}, {"00101", 2, 1}, {"0111", 0, 3},
    {"00111", 3, 1}, {"000110", 4, 1}, {"00110", 1, 2}, {"000111", 5, 1}, {"0000110", 6, 1},
    {"0000100", 7, 1}, {"11100", 0, 4}, {"0000111", 2, 2}, {"0000101", 8, 1}, {"1111000", 9, 1},
    {"000001", 0, -1}, {"11101", 0, 5}, {"000101", 0, 6}, {"1111001", 1, 3}, {"00100110", 3, 2},
    {"1111010", 10, 1}, {"00100001", 11, 1}, {"00100101", 12, 1}, {"00100100", 13, 1},
    {"000100", 0, 7}, {"00100111", 1, 4}, {"11111100", 2, 3}, {"11111101", 4, 2},
    {"000000100", 5, 2}, {"000000101", 14, 1}, {"000000111", 15, 1}, {"0000001101", 16, 1},
    {"1111011", 0, 8}, {"1111100", 0, 9}, {"00100011", 0, 10}, {"00100010", 0, 11},
    {"00100000", 1, 5}, {"0000001100", 2, 4},
    {"000000011100", 3, 3}, {"000000010010", 4, 3}, {"000000011110", 6, 2}, {"000000010101", 7, 2},
    {"000000010001", 8, 2}, {"000000011111", 17, 1}, {"000000011010", 18, 1}, {"000000011001", 19, 1},
    {"000000010111", 20, 1}, {"000000010110", 21, 1},
    {"11111010", 0, 12}, {"11111011", 0, 13}, {"11111110", 0, 14}, {"11111111", 0, 15},
    {nullptr, 0, 0}};
// 13..16-bit codes, shared by both tables.
const Vlc kLong[] = {
    {"0000000011010", 0, 12}, {"0000000011001", 0, 13}, {"0000000011000", 0, 14}, {"0000000010111", 0, 15},
    {"0000000010110", 1, 6}, {"0000000010101", 1, 7}, {"0000000010100", 2, 5}, {"0000000010011", 3, 4},
    {"0000000010010", 5, 3}, {"0000000010001", 9, 2}, {"0000000010000", 10, 2}, {"0000000011111", 22, 1},
    {"0000000011110", 23, 1}, {"0000000011101", 24, 1}, {"0000000011100", 25, 1}, {"0000000011011", 26, 1},
    {"00000000011111", 0, 16}, {"00000000011110", 0, 17}, {"00000000011101", 0, 18}, {"00000000011100", 0, 19},
    {"00000000011011", 0, 20}, {"00000000011010", 0, 21}, {"00000000011001", 0, 22}, {"00000000011000", 0, 23},
    {"00000000010111", 0, 24}, {"00000000010110", 0, 25}, {"00000000010101", 0, 26}, {"00000000010100", 0, 27},
    {"00000000010011", 0, 28}, {"00000000010010", 0, 29}, {"00000000010001", 0, 30}, {"00000000010000", 0, 31},
    {"000000000011000", 0, 32}, {"000000000010111", 0, 33}, {"000000000010110", 0, 34}, {"000000000010101", 0, 35},
    {"000000000010100", 0, 36}, {"000000000010011", 0, 37}, {"000000000010010", 0, 38}, {"000000000010001", 0, 39},
    {"000000000010000", 0, 40}, {"000000000011111", 1, 8}, {"000000000011110", 1, 9}, {"000000000011101", 1, 10},
    {"000000000011100", 1, 11}, {"000000000011011", 1, 12}, {"000000000011010", 1, 13}, {"000000000011001", 1, 14},
    {"0000000000010011", 1, 15}, {"0000000000010010", 1, 16}, {"0000000000010001", 1, 17}, {"0000000000010000", 1, 18},
    {"0000000000010100", 6, 3}, {"0000000000011010", 11, 2}, {"0000000000011001", 12, 2}, {"0000000000011000", 13, 2},
    {"0000000000010111", 14, 2}, {"0000000000010110", 15, 2}, {"0000000000010101", 16, 2}, {"0000000000011111", 27, 1},
    {"0000000000011110", 28, 1}, {"0000000000011101", 29, 1}, {"0000000000011100", 30, 1}, {"0000000000011011", 31, 1},
    {nullptr, 0, 0}};

// 16-bit lookup: entry = len<<16 | (run<<8) | level-code (level 0 EOB, 0xff escape); len 0 = invalid.
struct AcTable {
    std::vector<u32> lut;  // indexed by the next 16 bits
    bool first_eob_ok = true;
    void add(const Vlc* v, bool b14) {
        for (; v->code; ++v) {
            if (b14 && v->level == 1 && v->run == 0 && !std::strcmp(v->code, "11")) { /* not first */ }
            u32 len = u32(std::strlen(v->code)), code = 0;
            for (u32 i = 0; i < len; ++i) code = code * 2 + (v->code[i] - '0');
            u32 lvl = v->level < 0 ? 0xff : u32(v->level);
            u32 e = (len << 16) | (u32(v->run) << 8) | lvl;
            u32 base = code << (16 - len), n = 1u << (16 - len);
            for (u32 i = 0; i < n; ++i) {
                if (lut[base + i]) { xsg_logf("[ipu] VLC table overlap at %s\n", v->code); }
                lut[base + i] = e;
            }
        }
    }
    void build(const Vlc* main, bool b14) {
        lut.assign(65536, 0);
        add(main, b14);
        add(kLong, b14);
    }
};
AcTable tabB14, tabB15;

const u8 kZigzag[64] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
                        12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
                        35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
                        58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};
const u8 kAlternate[64] = {0,  8,  16, 24, 1,  9,  2,  10, 17, 25, 32, 40, 48, 56, 57, 49,
                           41, 33, 26, 18, 3,  11, 4,  12, 19, 27, 34, 42, 50, 58, 35, 43,
                           51, 59, 20, 28, 5,  13, 6,  14, 21, 29, 36, 44, 52, 60, 37, 45,
                           53, 61, 22, 30, 7,  15, 23, 31, 38, 46, 54, 62, 39, 47, 55, 63};
const u8 kDefaultIntra[64] = {8,  16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37,
                              19, 22, 26, 27, 29, 34, 34, 38, 22, 22, 26, 27, 29, 34, 37, 40,
                              22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32, 35, 40, 48, 58,
                              26, 27, 29, 34, 38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83};
const u8 kNonLinearQ[32] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  10, 12, 14, 16, 18,  20,  22,
                            24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88, 96, 104, 112};

// ---- IPU state --------------------------------------------------------------------------
u8 iq_intra[64], iq_nonintra[64];   // in raster order (SETIQ data is in zigzag order)
u32 th0 = 0, th1 = 0;
u32 ctrl = 0;
float idct_c[8][8];

struct Idec {
    bool running = false;
    bool dtd = false, sgn = false, dte = false, ofm = false;
    int qsc = 0;
    int dc_pred[3];
    u32 mbs = 0;
} idec;

void Init() {
    static bool done = false;
    if (done) return;
    done = true;
    tabB14.build(kB14, true);
    tabB15.build(kB15, false);
    for (int i = 0; i < 64; ++i) { iq_intra[i] = kDefaultIntra[i]; iq_nonintra[i] = 16; }
    for (int k = 0; k < 8; ++k)
        for (int n = 0; n < 8; ++n)
            idct_c[k][n] = float((k == 0 ? std::sqrt(0.125) : 0.5) * std::cos((2 * n + 1) * k * M_PI / 16));
}

int idp() { return (ctrl >> 16) & 3; }
bool as() { return (ctrl >> 20) & 1; }
bool ivf() { return (ctrl >> 21) & 1; }
bool qst() { return (ctrl >> 22) & 1; }
bool mp1() { return (ctrl >> 23) & 1; }

int dc_size(bool luma) {
    if (luma) {
        u32 b = peek(9);
        if ((b >> 7) == 0) { skip(2); return 1; }          // 00
        if ((b >> 7) == 1) { skip(2); return 2; }          // 01
        if ((b >> 6) == 4) { skip(3); return 0; }          // 100
        if ((b >> 6) == 5) { skip(3); return 3; }          // 101
        if ((b >> 6) == 6) { skip(3); return 4; }          // 110
        // 111...: 1110 ->5, 11110 ->6, ..., 111111110 -> 10, 111111111 -> 11
        int ones = 3;
        while (ones < 9 && ((b >> (8 - ones)) & 1)) ++ones;
        if (ones == 9) { skip(9); return 11; }
        skip(ones + 1);
        return ones + 2;
    }
    u32 b = peek(10);
    if ((b >> 8) == 0) { skip(2); return 0; }
    if ((b >> 8) == 1) { skip(2); return 1; }
    if ((b >> 8) == 2) { skip(2); return 2; }
    int ones = 2;
    while (ones < 10 && ((b >> (9 - ones)) & 1)) ++ones;
    if (ones == 10) { skip(10); return 11; }
    skip(ones + 1);
    return ones + 1;
}

// Decode one intra block (coefficients dequantised, raster order) into out[64].
void intra_block(int comp, int qscale, int out[64]) {
    std::memset(out, 0, 64 * sizeof(int));
    int sz = dc_size(comp == 0);
    int diff = 0;
    if (sz) {
        u32 v = get(sz);
        diff = (v & (1u << (sz - 1))) ? int(v) : int(v) - int((1u << sz) - 1);
    }
    int& pred = idec.dc_pred[comp];
    pred += diff;
    out[0] = pred * (8 >> idp());
    int sum = out[0];
    const u8* scan = as() ? kAlternate : kZigzag;
    const AcTable& t = ivf() ? tabB15 : tabB14;
    int i = 0;
    for (;;) {
        u32 e = t.lut[peek(16)];
        if (!e) throw 1;   // bitstream error
        u32 len = e >> 16, run = (e >> 8) & 0xff, lvl = e & 0xff;
        int level;
        if (lvl == 0) { skip(len); break; }   // EOB
        if (lvl == 0xff) {                     // escape
            skip(6);
            run = get(6);
            if (mp1()) {
                int l = int(get(8));
                if (l == 0) l = int(get(8));
                else if (l == 128) l = int(get(8)) - 256;
                else if (l > 128) l -= 256;
                level = l;
            } else {
                int l = int(get(12));
                level = l >= 2048 ? l - 4096 : l;
            }
        } else {
            skip(len);
            level = get(1) ? -int(lvl) : int(lvl);
        }
        i += int(run) + 1;
        if (i > 63) throw 2;
        int pos = scan[i];
        int f = (level * qscale * int(iq_intra[pos]) * 2) / 32;
        if (f > 2047) f = 2047;
        if (f < -2048) f = -2048;
        out[pos] = f;
        sum += f;
    }
    if (!mp1() && !(sum & 1)) out[63] ^= 1;
}

void idct(const int in[64], float out[64]) {
    float tmp[64];
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            float s = 0;
            for (int u = 0; u < 8; ++u) s += idct_c[u][x] * float(in[y * 8 + u]);
            tmp[y * 8 + x] = s;
        }
    for (int x = 0; x < 8; ++x)
        for (int y = 0; y < 8; ++y) {
            float s = 0;
            for (int v = 0; v < 8; ++v) s += idct_c[v][y] * tmp[v * 8 + x];
            out[y * 8 + x] = s;
        }
}

inline int clamp255(float v) {
    int i = int(std::lrint(v));
    return i < 0 ? 0 : i > 255 ? 255 : i;
}

// One macroblock: true when one was decoded, false at the end of the picture.
bool idec_mb() {
    // macroblock_type for an I picture: 1 = intra, 01 = intra + quant
    bool quant;
    if (peek(1)) { skip(1); quant = false; }
    else if (peek(2) == 1) { skip(2); quant = true; }
    else throw 3;
    bool field = false;
    if (idec.dtd) field = get(1);
    if (quant) idec.qsc = int(get(5));
    int qscale = qst() ? kNonLinearQ[idec.qsc] : idec.qsc * 2;
    float blk[6][64];
    int coef[64];
    for (int b = 0; b < 6; ++b) {
        intra_block(b < 4 ? 0 : b - 3, qscale, coef);
        idct(coef, blk[b]);
    }
    u8 y[16][16];
    for (int b = 0; b < 4; ++b)
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 8; ++c) {
                int px = (b & 1) * 8 + c;
                int py = field ? (r * 2 + (b >> 1)) : ((b >> 1) * 8 + r);
                y[py][px] = u8(clamp255(blk[b][r * 8 + c]));
            }
    u8 out[1024];
    for (int py = 0; py < 16; ++py)
        for (int px = 0; px < 16; ++px) {
            float Y = 1.164f * (float(y[py][px]) - 16.f);
            float cb = float(clamp255(blk[4][(py >> 1) * 8 + (px >> 1)])) - 128.f;
            float cr = float(clamp255(blk[5][(py >> 1) * 8 + (px >> 1)])) - 128.f;
            int r = clamp255(Y + 1.596f * cr);
            int g = clamp255(Y - 0.391f * cb - 0.813f * cr);
            int bl = clamp255(Y + 2.018f * cb);
            int a = 0x80;
            if (u32(r) < th0 && u32(g) < th0 && u32(bl) < th0) { r = g = bl = 0; a = 0; }
            else if (u32(r) < th1 && u32(g) < th1 && u32(bl) < th1) a = 0x40;
            u8* p = out + (py * 16 + px) * 4;
            p[0] = u8(r); p[1] = u8(g); p[2] = u8(bl); p[3] = u8(a);
        }
    if (idec.ofm) {  // RGB16 (not used by the game's movies)
        for (int i = 0; i < 256; ++i) {
            u8* p = out + i * 4;
            unsigned v = (p[0] >> 3) | ((p[1] >> 3) << 5) | ((p[2] >> 3) << 10) | (p[3] ? 0x8000 : 0);
            outq.push_back(u8(v)); outq.push_back(u8(v >> 8));
        }
    } else {
        outq.insert(outq.end(), out, out + 1024);
    }
    ++idec.mbs;
    return true;
}

// macroblock_address_increment after a macroblock: true = another macroblock follows.
bool idec_next() {
    for (;;) {
        if (peek(1)) { skip(1); return true; }
        u32 b = peek(11);
        if (b == 0x00f) { skip(11); continue; }            // stuffing
        if (b == 0x008) { skip(11); continue; }            // escape (+33): treat as continue
        if (peek(16) == 0 || b < 0x18) return false;       // start code / end of picture
        // longer increments: skipped macroblocks do not exist in IDEC; consume and continue
        static const struct { u32 len, code; } mba[] = {{3, 3}, {3, 2}, {4, 3}, {4, 2}, {5, 3}, {5, 2},
                                                        {7, 7}, {7, 6}, {8, 11}, {8, 10}, {8, 9}, {8, 8},
                                                        {8, 7}, {8, 6}};
        for (auto& m : mba)
            if (peek(m.len) == m.code) { skip(m.len); return true; }
        u32 len = (b >= 0x18 && b <= 0x23) ? 11 : 10;
        skip(len);
        return true;
    }
}

void idec_run() {
    while (idec.running) {
        u32 save_bits = bitpos;
        int save_pred[3] = {idec.dc_pred[0], idec.dc_pred[1], idec.dc_pred[2]};
        int save_q = idec.qsc;
        size_t save_out = outq.size();
        try {
            idec_mb();
            if (!idec_next()) {
                // The picture ends at a start code: the IPU stops byte aligned on it
                // (the game then skips the 32-bit 0x000001b0 with FDEC 32).
                bitpos = (bitpos + 7) & ~7u;
                while (peek(24) != 1 && peek(8) == 0) skip(8);
                idec.running = false;
                if (log_on()) xsg_logf("[ipu] frame %u: IDEC done, %u MBs\n", xsg_frame, idec.mbs);
            }
        } catch (Starved&) {
            bitpos = save_bits;
            idec.dc_pred[0] = save_pred[0]; idec.dc_pred[1] = save_pred[1]; idec.dc_pred[2] = save_pred[2];
            idec.qsc = save_q;
            outq.resize(save_out);
            break;
        } catch (int e) {
            xsg_logf("[ipu] frame %u: IDEC bitstream error %d after %u MBs\n", xsg_frame, e, idec.mbs);
            idec.running = false;
        }
        compact();
    }
    update_bp();
    drain();
}

enum { Pending_None, Pending_Fdec, Pending_SetIq, Pending_SetVq, Pending_Bclr };
struct PendingCmd { int kind = Pending_None; u32 cmd = 0; } pending;

// Commands other than IDEC can also wait for input (FDEC/SETIQ). Retry them on more DMA.
bool run_simple(u32 cmd) {
    u32 op = cmd >> 28;
    try {
        switch (op) {
        case 0x4: {  // FDEC
            skip(cmd & 0x3f);
            R(IPU_CMD) = peek(32);
            R(IPU_TOP) = R(IPU_CMD);
            break;
        }
        case 0x5: {  // SETIQ: 64 bytes in zigzag order
            skip(cmd & 0x3f);
            u8* t = (cmd & (1u << 27)) ? iq_nonintra : iq_intra;
            for (int i = 0; i < 64; ++i) t[kZigzag[i]] = u8(get(8));
            break;
        }
        case 0x6: {  // SETVQ: 16 halfwords (only the CLUT for VQ output; stored nowhere)
            for (int i = 0; i < 16; ++i) get(16);
            break;
        }
        }
    } catch (Starved&) {
        return false;
    }
    update_bp();
    return true;
}

void cmd_write(u32 cmd) {
    Init();
    u32 op = cmd >> 28;
    if (log_on()) xsg_logf("[ipu] frame %u: cmd %08x\n", xsg_frame, cmd);
    switch (op) {
    case 0x0:  // BCLR
        fifo_clear(cmd & 0x7f);
        update_bp();
        break;
    case 0x1:  // IDEC
        try { skip(cmd & 0x3f); } catch (Starved&) {}
        idec.running = true;
        idec.dtd = (cmd >> 24) & 1;
        idec.sgn = (cmd >> 25) & 1;
        idec.dte = (cmd >> 26) & 1;
        idec.ofm = (cmd >> 27) & 1;
        idec.qsc = int((cmd >> 16) & 0x1f);
        idec.dc_pred[0] = idec.dc_pred[1] = idec.dc_pred[2] = 128 << idp();
        idec.mbs = 0;
        idec_run();
        break;
    case 0x4: case 0x5: case 0x6:
        if (!run_simple(cmd)) { pending.kind = Pending_Fdec; pending.cmd = cmd; }
        break;
    case 0x9:  // SETTH
        th0 = cmd & 0x1ff;
        th1 = (cmd >> 16) & 0x1ff;
        break;
    default: {
        static int n = 0;
        if (n++ < 8) xsg_logf("[ipu] command %08x not implemented\n", cmd);
        break;
    }
    }
}

void retry() {
    if (pending.kind != Pending_None) {
        if (run_simple(pending.cmd)) pending.kind = Pending_None;
    }
    if (idec.running) idec_run();
}

}  // namespace

extern "C" {

void xsg_ipu_reg_written(uint32_t addr) {
    Init();
    switch (addr & 0x1fffffff) {
    case IPU_CMD:
        cmd_write(R(IPU_CMD));
        break;
    case IPU_CTRL: {
        u32 v = R(IPU_CTRL);
        if (v & 0x40000000u) {  // RST
            idec.running = false;
            pending.kind = Pending_None;
            fifo_clear(0);
            outq.clear();
            ctrl = 0;
            update_bp();
        } else {
            ctrl = v & 0x07ff0000u;
        }
        R(IPU_CTRL) = ctrl;
        break;
    }
    case D4_CHCR: {
        u32 v = R(D4_CHCR);
        if (v & 0x100) {
            ch4.start();
            if (log_on()) xsg_logf("[ipu] ch4 start chcr %08x madr %08x qwc %x tadr %08x\n", ch4.chcr, ch4.madr, ch4.qwc, ch4.tadr);
            ch4.writeback();
            refill();
            update_bp();
            retry();
        } else {
            ch4.active = false;
            ch4.chcr = v;
            ch4.writeback();
        }
        break;
    }
    case D3_CHCR: {
        u32 v = R(D3_CHCR);
        if (v & 0x100) {
            ch3.madr = R(D3_MADR);
            ch3.qwc = R(D3_QWC) & 0xffff;
            ch3.active = true;
            drain();
            retry();
        } else {
            ch3.active = false;
        }
        break;
    }
    }
}

// sceIpuSync: nonzero while a command is still waiting for input.
int xsg_ipu_busy(void) {
    retry();
    return idec.running || pending.kind != Pending_None;
}

}  // extern "C"

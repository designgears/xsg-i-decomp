/*
 * Stage 5: graphics data path and screenshots.
 *
 *   DMA ch1 (VIF1) / ch2 (GIF) kicks  ->  source-chain walker  ->  VIF1 decoder / GIF
 *   -> software GS (port/third_party/ps2-port-engine-gs)  ->  PNG at vblank.
 *
 * Every entry point takes one mutex: the DMAC stand-in thread (host_main.c)
 * calls xsg_dma_poll() when it sees CHCR.STR set, and the game's thread calls
 * the sceGs* functions and xsg_gs_vblank().  The game's PS2 addresses are
 * identity-mapped in this 32-bit process, so DMA addresses are pointers.
 */
#include <condition_variable>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "ps2host/gs.h"
#include "vu1.h"

namespace ps2host { std::uint32_t g_dma_source = 0; }

extern "C" {
#include "xsg_rt.h"
}

using ps2host::gs::Gs;
using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

namespace {

std::recursive_mutex g_lock;
Gs* g_gs;

Gs& G() {
    if (!g_gs) g_gs = &ps2host::gs::TheGs();
    return *g_gs;
}

struct Stats {
    u64 gif_kicks = 0, vif_kicks = 0, gif_qw = 0, vif_qw = 0, tags = 0;
    u64 direct_qw = 0, mscal = 0, mscnt = 0, mpg = 0, unpack = 0, bad_vif = 0;
    // PATH1 (XGKICK) validation
    u64 p1_packets = 0, p1_tags = 0, p1_qw = 0, p1_prims = 0, p1_bad = 0;
    u64 p1_prim_type[8] = {};
    u64 p1_nloop = 0;   // vertices-ish: NLOOP of PACKED/REGLIST tags
} st;

/* PS2 DMA/tag address -> host pointer. Bit 31 = scratchpad; 0x2/0x3xxxxxxx
   uncached views of main RAM. Anything else is a host address already. */
const u8* Ptr(u32 a) {
    if (a & 0x80000000u) return reinterpret_cast<const u8*>(uintptr_t(PS2_SPR_BASE + (a & 0x3ff0u)));
    if ((a >> 28) == 2 || (a >> 28) == 3) {
        u32 r = a & 0x0fffffffu;
        if (r < PS2_RAM_SIZE) return reinterpret_cast<const u8*>(uintptr_t(r));
    }
    return reinterpret_cast<const u8*>(uintptr_t(a));
}

/* ---------------------------------------------------------------- GIF */

void GifSend(int path, const u8* p, u32 qwords) {
    Gs& gs = G();
    for (u32 i = 0; i < qwords; ++i, p += 16) {
        u64 lo, hi;
        std::memcpy(&lo, p, 8);
        std::memcpy(&hi, p + 8, 8);
        gs.Qword(path, lo, hi);
    }
}

/* A self-contained GIF packet (tags through EOP) starting at p: its length in quadwords. */
u32 GifPacketLength(const u8* p, u32 limit) {
    u32 n = 0;
    while (n < limit) {
        u64 lo;
        std::memcpy(&lo, p + n * 16, 8);
        u64 hi;
        std::memcpy(&hi, p + n * 16 + 8, 8);
        u32 nloop = u32(lo & 0x7fff), eop = u32(lo >> 15) & 1, flg = u32(lo >> 58) & 3;
        u32 nreg = u32(lo >> 60) & 15;
        if (!nreg) nreg = 16;
        n += 1;
        if (flg == 0) n += nloop * nreg;
        else if (flg == 1) n += (nloop * nreg + 1) / 2;
        else n += nloop;
        if (eop) break;
    }
    return n;
}

/* ---------------------------------------------------------------- VIF1 */

struct Vif {
    u32 code = 0;
    u32 want = 0;       // data words still owed to the current code
    u32 kind = 0;       // 1 DIRECT, 2 MPG, 3 UNPACK, 4 skip
    u32 gif_words[4];
    u32 gif_n = 0;
    u32 addr = 0;       // MPG/UNPACK destination (bytes into VU1 memory)
    // unpack
    u32 upk_vn = 0, upk_vl = 0, upk_usn = 0, upk_elem = 0, upk_bits = 0, upk_acc_n = 0;
    u8 upk_buf[16];
    u32 upk_have = 0;
    u32 cl = 1, wl = 1, cycle = 0, base = 0, ofst = 0, tops = 0, itops = 0;
    u32 row[4] = {}, col[4] = {}, mask = 0, mode = 0;
    bool dbf = false;
    u32 st_kind = 0, st_n = 0;      // STROW(1)/STCOL(2)/STMASK(3) data being received
} vif;

u8* Vu1Micro() { return reinterpret_cast<u8*>(uintptr_t(PS2_VU_BASE + 0x8000)); }
u8* Vu1Data() { return reinterpret_cast<u8*>(uintptr_t(PS2_VU_BASE + 0xC000)); }

void UnpackElement() {
    // One element in upk_buf (upk_elem bytes) -> one VU1 quadword.
    u32 out[4] = {0, 0, 0, 0};
    const u32 n = vif.upk_vn + 1;
    for (u32 c = 0; c < 4; ++c) {
        u32 comp = c < n ? c : (vif.upk_vn == 0 ? 0 : c);     // S_*: broadcast
        if (vif.upk_vn == 3 && vif.upk_vl == 3) {              // V4-5 (RGBA5551)
            u32 v = u32(vif.upk_buf[0]) | u32(vif.upk_buf[1]) << 8;
            out[0] = (v & 31) << 3; out[1] = ((v >> 5) & 31) << 3; out[2] = ((v >> 10) & 31) << 3;
            out[3] = (v >> 15) << 7;
            break;
        }
        if (vif.upk_vn == 0) comp = 0;
        else if (c >= n) { out[c] = (c == 3 && vif.upk_vn == 2) ? 0 : 0; continue; }
        if (vif.upk_vl == 0) std::memcpy(&out[c], vif.upk_buf + comp * 4, 4);
        else if (vif.upk_vl == 1) {
            u32 v = u32(vif.upk_buf[comp * 2]) | u32(vif.upk_buf[comp * 2 + 1]) << 8;
            out[c] = vif.upk_usn ? v : u32(std::int32_t(std::int16_t(v)));
        } else {
            u32 v = vif.upk_buf[comp];
            out[c] = vif.upk_usn ? v : u32(std::int32_t(std::int8_t(v)));
        }
    }
    u32 a = vif.addr & 0x3ff0;
    u32 cur[4];
    std::memcpy(cur, Vu1Data() + a, 16);
    const u32 mrow = vif.cycle < 3 ? vif.cycle : 3;
    const bool use_mask = (vif.code >> 28) & 1;     // UNPACK's M bit
    for (int c = 0; c < 4; ++c) {
        u32 m = use_mask ? (vif.mask >> (mrow * 8 + c * 2)) & 3 : 0;
        if (m == 0) {
            if (vif.mode == 1) out[c] += vif.row[c];
            else if (vif.mode == 2) { vif.row[c] += out[c]; out[c] = vif.row[c]; }
        } else if (m == 1) out[c] = vif.row[c];
        else if (m == 2) out[c] = vif.col[mrow];
        else out[c] = cur[c];
    }
    std::memcpy(Vu1Data() + a, out, 16);
    vif.addr += 16;
    if (vif.cl < vif.wl) {} else if (++vif.cycle == vif.wl) { vif.cycle = 0; vif.addr += (vif.cl - vif.wl) * 16; }
}

/* PATH1: a GIF packet in VU1 data memory from quadword `q` (wrapping) to EOP. */
void Path1Kick(const u8* data, u32 q) {
    Gs& gs = G();
    st.p1_packets++;
    for (u32 guard = 0; guard < 1024; ++guard) {
        u64 lo, hi;
        std::memcpy(&lo, data + (q & 0x3ff) * 16, 8);
        std::memcpy(&hi, data + (q & 0x3ff) * 16 + 8, 8);
        st.p1_tags++;
        const u32 nloop = u32(lo & 0x7fff), eop = u32(lo >> 15) & 1, pre = u32(lo >> 46) & 1;
        const u32 prim = u32(lo >> 47) & 0x7ff, flg = u32(lo >> 58) & 3;
        u32 nreg = u32(lo >> 60) & 15;
        if (!nreg) nreg = 16;
        if (pre) { st.p1_prims++; st.p1_prim_type[prim & 7]++; }
        if (flg != 2) st.p1_nloop += nloop;
        u32 body = flg == 0 ? nloop * nreg : flg == 1 ? (nloop * nreg + 1) / 2 : nloop;
        if (body > 1024) { st.p1_bad++; break; }
        gs.Qword(1, lo, hi);
        ++q;
        for (u32 i = 0; i < body; ++i, ++q) {
            std::memcpy(&lo, data + (q & 0x3ff) * 16, 8);
            std::memcpy(&hi, data + (q & 0x3ff) * 16 + 8, 8);
            gs.Qword(1, lo, hi);
        }
        st.p1_qw += body + 1;
        if (eop) return;
    }
}

void RunVu1(u32 pc) {
    // TOP/ITOP as VIF1 hands them over; then the double buffer flips.
    const u32 top = vif.tops, itop = vif.itops;
    if (vif.dbf) { vif.dbf = false; vif.tops = vif.base; }
    else { vif.dbf = true; vif.tops = vif.base + vif.ofst; }
    static bool init = false;
    if (!init) { init = true; xsgvu::SetKick(Path1Kick); }
    static const bool off = std::getenv("XSG_VU1") && !std::atoi(std::getenv("XSG_VU1"));
    if (!off) xsgvu::Run(pc, top, itop);
}

void VifData(u32 w) {
    switch (vif.kind) {
    case 1:
        vif.gif_words[vif.gif_n++] = w;
        if (vif.gif_n == 4) {
            vif.gif_n = 0;
            G().Qword(2, u64(vif.gif_words[0]) | u64(vif.gif_words[1]) << 32,
                      u64(vif.gif_words[2]) | u64(vif.gif_words[3]) << 32);
            st.direct_qw++;
        }
        break;
    case 2:
        std::memcpy(Vu1Micro() + (vif.addr & 0x3ffc), &w, 4);
        vif.addr += 4;
        break;
    case 3:
        for (int b = 0; b < 4; ++b) {
            if (vif.upk_acc_n == 0) break;
            vif.upk_buf[vif.upk_have++] = u8(w >> (8 * b));
            if (vif.upk_have == vif.upk_elem) {
                UnpackElement();
                vif.upk_have = 0;
                vif.upk_acc_n--;
            }
        }
        break;
    case 4:
        if (vif.st_kind == 1) vif.row[vif.st_n++ & 3] = w;
        else if (vif.st_kind == 2) vif.col[vif.st_n++ & 3] = w;
        else if (vif.st_kind == 3) vif.mask = w;
        break;
    default:
        break;
    }
    vif.want--;
}

void VifCode(u32 w) {
    const u32 cmd = (w >> 24) & 0x7f, num = (w >> 16) & 0xff, imm = w & 0xffff;
    vif.code = w;
    vif.kind = 0;
    switch (cmd) {
    case 0x00: break;                                       // NOP
    case 0x01: vif.cl = imm & 0xff; vif.wl = (imm >> 8) & 0xff; vif.cycle = 0; break;
    case 0x02: vif.ofst = imm & 0x3ff; vif.dbf = false; vif.tops = vif.base; break;
    case 0x03: vif.base = imm & 0x3ff; break;
    case 0x04: vif.itops = imm & 0x3ff; break;              // ITOP
    case 0x05: vif.mode = imm & 3; break;
    case 0x06: break;                                       // MSKPATH3
    case 0x07: break;                                       // MARK
    case 0x10: case 0x11: case 0x13: break;                 // FLUSHE/FLUSH/FLUSHA
    case 0x14: case 0x15: st.mscal++; RunVu1(imm * 8); break;   // MSCAL/MSCALF
    case 0x17: st.mscnt++; RunVu1(xsgvu::Resume()); break;     // MSCNT
    case 0x20: vif.st_kind = 3; vif.st_n = 0; vif.want = 1; break;  // STMASK
    case 0x30: vif.st_kind = 1; vif.st_n = 0; vif.want = 4; break;  // STROW
    case 0x31: vif.st_kind = 2; vif.st_n = 0; vif.want = 4; break;  // STCOL
    case 0x4A:
        st.mpg++;
        { static int n = 0; static const bool t = std::getenv("XSG_MPG_TRACE") != nullptr;
          if (t && n++ < 40) xsg_logf("[vif1] MPG addr 0x%x num %u\n", imm * 8, num ? num : 256); }
        vif.kind = 2; vif.addr = imm * 8; vif.want = (num ? num : 256) * 2;
        break;
    case 0x50: case 0x51:
        vif.kind = 1; vif.gif_n = 0; vif.want = (imm ? imm : 65536) * 4;
        break;
    default:
        if ((cmd & 0x60) == 0x60) {                         // UNPACK
            st.unpack++;
            u32 vn = (cmd >> 2) & 3, vl = cmd & 3;
            u32 n = num ? num : 256;
            u32 bits = (32u >> vl) * (vn + 1);
            if (vn == 3 && vl == 3) bits = 16;
            vif.kind = 3;
            vif.upk_vn = vn; vif.upk_vl = vl; vif.upk_usn = (imm >> 14) & 1;
            vif.upk_elem = bits / 8;
            vif.upk_have = 0;
            vif.upk_acc_n = n;
            vif.addr = (imm & 0x3ff) * 16 + ((imm & 0x8000) ? vif.tops * 16 : 0);
            vif.cycle = 0;
            u32 elems = n;
            if (vif.wl > vif.cl && vif.cl) elems = vif.cl * (n / vif.wl) + std::min(n % vif.wl, vif.cl);
            vif.want = (elems * bits + 31) / 32;
            vif.upk_acc_n = elems;
        } else {
            if (st.bad_vif++ < 10) xsg_logf("[vif1] unknown VIF code %08x\n", w);
        }
        break;
    }
    if (cmd == 0x20 || cmd == 0x30 || cmd == 0x31) vif.kind = 4;
    vif.code = w;
}

void VifWords(const u8* p, u32 words) {
    for (u32 i = 0; i < words; ++i) {
        u32 w;
        std::memcpy(&w, p + i * 4, 4);
        if (vif.want) VifData(w);
        else VifCode(w);
    }
}

/* ---------------------------------------------------------------- DMA */

struct Chan { volatile u32* chcr; volatile u32* madr; volatile u32* qwc; volatile u32* tadr; };
Chan ChanRegs(u32 base) {
    auto r = [](u32 a) { return reinterpret_cast<volatile u32*>(uintptr_t(a)); };
    return {r(base), r(base + 0x10), r(base + 0x20), r(base + 0x30)};
}

void Deliver(int ch, const u8* p, u32 qwords) {
    if (!qwords) return;
    if (ch == 2) { st.gif_qw += qwords; GifSend(3, p, qwords); }
    else { st.vif_qw += qwords; VifWords(p, qwords * 4); }
}

void RunChannel(int ch, u32 base) {
    Chan c = ChanRegs(base);
    u32 chcr = *c.chcr;
    const u32 mode = (chcr >> 2) & 3;
    const bool tte = (chcr >> 6) & 1;
    if (ch == 2) st.gif_kicks++; else st.vif_kicks++;
    if (mode == 0) {                        // normal
        Deliver(ch, Ptr(*c.madr), *c.qwc & 0xffff);
        *c.qwc = 0;
        return;
    }
    if (mode != 1) {
        xsg_logf("[dma] ch%d mode %u not supported\n", ch, mode);
        return;
    }
    // A pending QWC before the chain (as the hardware would send first).
    if (*c.qwc & 0xffff) Deliver(ch, Ptr(*c.madr), *c.qwc & 0xffff);
    u32 tadr = *c.tadr;
    u32 asr[2];
    int asp = 0;
    for (u32 n = 0; n < (1u << 20); ++n) {
        const u8* t = Ptr(tadr);
        u32 tag[4];
        std::memcpy(tag, t, 16);
        st.tags++;
        static const bool trace = std::getenv("XSG_DMA_TRACE") != nullptr;
        if (trace) xsg_logf("[dma] ch%d tag @%08x: %08x %08x %08x %08x\n", ch, tadr, tag[0], tag[1], tag[2], tag[3]);
        const u32 qwc = tag[0] & 0xffff, id = (tag[0] >> 28) & 7, addr = tag[1] & 0xfffffff0u;
        if (tte && ch == 1) VifWords(t + 8, 2);
        bool end = false;
        switch (id) {
        case 0: Deliver(ch, Ptr(addr), qwc); end = true; break;                 // refe
        case 1: Deliver(ch, t + 16, qwc); tadr += 16 + qwc * 16; break;         // cnt
        case 2: Deliver(ch, t + 16, qwc); tadr = addr; break;                   // next
        case 3: case 4: Deliver(ch, Ptr(addr), qwc); tadr += 16; break;         // ref/refs
        case 5:                                                                 // call
            Deliver(ch, t + 16, qwc);
            if (asp < 2) asr[asp++] = tadr + 16 + qwc * 16;
            tadr = addr;
            break;
        case 6:                                                                 // ret
            Deliver(ch, t + 16, qwc);
            if (asp) tadr = asr[--asp];
            else end = true;
            break;
        case 7: Deliver(ch, t + 16, qwc); end = true; break;                    // end
        }
        if (end) break;
        if ((tag[0] >> 31) && (chcr & 0x80)) break;                             // IRQ with TIE
    }
    *c.tadr = tadr;
    *c.qwc = 0;
}

/* ---------------------------------------------------------------- PNG */

u32 Crc(const u8* p, size_t n, u32 c = 0xffffffffu) {
    static u32 table[256];
    if (!table[1])
        for (u32 i = 0; i < 256; ++i) {
            u32 v = i;
            for (int k = 0; k < 8; ++k) v = v & 1 ? 0xedb88320u ^ (v >> 1) : v >> 1;
            table[i] = v;
        }
    for (size_t i = 0; i < n; ++i) c = table[(c ^ p[i]) & 255] ^ (c >> 8);
    return c;
}

void Be32(std::vector<u8>& o, u32 v) {
    o.push_back(u8(v >> 24)); o.push_back(u8(v >> 16)); o.push_back(u8(v >> 8)); o.push_back(u8(v));
}

void Chunk(std::FILE* f, const char* type, const std::vector<u8>& data) {
    std::vector<u8> b;
    Be32(b, u32(data.size()));
    b.insert(b.end(), type, type + 4);
    b.insert(b.end(), data.begin(), data.end());
    Be32(b, Crc(b.data() + 4, b.size() - 4) ^ 0xffffffffu);
    std::fwrite(b.data(), 1, b.size(), f);
}

/* RGB rows -> PNG with stored (uncompressed) deflate blocks. */
bool WritePng(const std::string& path, const std::vector<u8>& rgb, u32 w, u32 h) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    static const u8 sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    std::fwrite(sig, 1, 8, f);
    std::vector<u8> ihdr;
    Be32(ihdr, w); Be32(ihdr, h);
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    Chunk(f, "IHDR", ihdr);
    std::vector<u8> raw;
    raw.reserve(size_t(h) * (w * 3 + 1));
    for (u32 y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgb.begin() + size_t(y) * w * 3, rgb.begin() + size_t(y + 1) * w * 3);
    }
    std::vector<u8> z = {0x78, 0x01};
    u32 a = 1, b = 0;
    for (u8 c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw.size() || off == 0;) {
        size_t n = std::min<size_t>(65535, raw.size() - off);
        bool last = off + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(u8(n)); z.push_back(u8(n >> 8)); z.push_back(u8(~n)); z.push_back(u8(~n >> 8));
        z.insert(z.end(), raw.begin() + off, raw.begin() + off + n);
        off += n;
        if (last) break;
    }
    Be32(z, (b << 16) | a);
    Chunk(f, "IDAT", z);
    Chunk(f, "IEND", {});
    std::fclose(f);
    return true;
}

/* ---------------------------------------------------------------- screenshots */

std::set<u32> shot_frames;
u32 shot_every = 0;
bool shot_vram = false;
std::string shot_dir;
bool shot_init = false;

void ShotInit() {
    shot_init = true;
    if (const char* s = std::getenv("XSG_SHOT"))
        for (const char* p = s; *p;) {
            char* e;
            unsigned long v = std::strtoul(p, &e, 0);
            if (e == p) break;
            shot_frames.insert(u32(v));
            p = *e ? e + 1 : e;
        }
    if (const char* s = std::getenv("XSG_SHOT_EVERY")) shot_every = u32(std::strtoul(s, nullptr, 0));
    shot_vram = std::getenv("XSG_SHOT_VRAM") && std::atoi(std::getenv("XSG_SHOT_VRAM"));
    const char* d = std::getenv("XSG_SHOT_DIR");
    shot_dir = d ? d : XSG_DEFAULT_SHOT_DIR;
    if (!shot_frames.empty() || shot_every) {
        mkdir(shot_dir.c_str(), 0755);
        xsg_logf("[shot] %zu frames, every %u, vram %d -> %s\n", shot_frames.size(), shot_every, int(shot_vram),
                 shot_dir.c_str());
    }
}

u64 Priv(u32 off) { return *reinterpret_cast<volatile u64*>(uintptr_t(PS2_GSPRIV_BASE + off)); }

/* XSG_STREAM=<fifo>: every vblank, the displayed buffer as raw 512x448 RGB24
   on a FIFO (for the status site's live video).  A writer thread takes the
   latest frame; frames are dropped while it is busy or no reader is attached. */
struct Stream {
    std::string path;
    std::mutex m;
    std::condition_variable cv;
    std::vector<u8> buf, out;
    bool ready = false;
    int fd = -1;
};
Stream* g_stream = nullptr;

void StreamWriter() {
    Stream& s = *g_stream;
    for (;;) {
        {
            std::unique_lock<std::mutex> l(s.m);
            s.cv.wait(l, [&] { return s.ready; });
            s.out.swap(s.buf);
            s.ready = false;
        }
        if (s.fd < 0) {
            s.fd = ::open(s.path.c_str(), O_WRONLY | O_NONBLOCK);
            if (s.fd < 0) continue;  // no reader yet
            ::fcntl(s.fd, F_SETFL, ::fcntl(s.fd, F_GETFL) & ~O_NONBLOCK);
        }
        size_t off = 0;
        while (off < s.out.size()) {
            ssize_t n = ::write(s.fd, s.out.data() + off, s.out.size() - off);
            if (n <= 0) { ::close(s.fd); s.fd = -1; break; }  // reader went away
            off += size_t(n);
        }
    }
}

void StreamFrame() {
    if (!g_stream) return;
    {
        std::lock_guard<std::mutex> l(g_stream->m);
        if (g_stream->ready) return;  // writer still busy: drop
    }
    Gs& gs = G();
    gs.Finish();
    const u64 pmode = Priv(0x00);
    int circ = (pmode & 1) ? 1 : (pmode & 2) ? 2 : 1;
    u64 dispfb = Priv(circ == 1 ? 0x70 : 0x90), display = Priv(circ == 1 ? 0x80 : 0xA0);
    if (circ == 1 && dispfb == 0 && display == 0) { dispfb = Priv(0x90); display = Priv(0xA0); }
    u32 fbp = u32(dispfb & 0x1ff), fbw = u32(dispfb >> 9) & 63, psm = u32(dispfb >> 15) & 31;
    u32 dbx = u32(dispfb >> 32) & 0x7ff, dby = u32(dispfb >> 43) & 0x7ff;
    if (!fbw) fbw = 8;
    std::vector<u8> v(512 * 448 * 3);
    for (u32 y = 0; y < 448; ++y)
        for (u32 x = 0; x < 512; ++x) {
            u32 px = gs.FramePixel(fbp, fbw, psm, x + dbx, y + dby);
            u8* o = &v[(size_t(y) * 512 + x) * 3];
            o[0] = u8(px); o[1] = u8(px >> 8); o[2] = u8(px >> 16);
        }
    std::lock_guard<std::mutex> l(g_stream->m);
    g_stream->buf.swap(v);
    g_stream->ready = true;
    g_stream->cv.notify_one();
}

void StreamInit() {
    const char* p = std::getenv("XSG_STREAM");
    if (!p || !*p) return;
    ::signal(SIGPIPE, SIG_IGN);
    g_stream = new Stream;
    g_stream->path = p;
    std::thread(StreamWriter).detach();
    xsg_logf("[stream] raw 512x448 rgb24 -> %s\n", p);
}

void Shot(u32 frame) {
    Gs& gs = G();
    gs.Finish();
    const u64 pmode = Priv(0x00), smode2 = Priv(0x20);
    // Circuit 1 when enabled (the game flips through DISPFB1), else circuit 2.
    int circ = (pmode & 1) ? 1 : (pmode & 2) ? 2 : 1;
    u64 dispfb = Priv(circ == 1 ? 0x70 : 0x90), display = Priv(circ == 1 ? 0x80 : 0xA0);
    if (circ == 1 && dispfb == 0 && display == 0) { dispfb = Priv(0x90); display = Priv(0xA0); }
    u32 fbp = u32(dispfb & 0x1ff), fbw = u32(dispfb >> 9) & 63, psm = u32(dispfb >> 15) & 31;
    u32 dbx = u32(dispfb >> 32) & 0x7ff, dby = u32(dispfb >> 43) & 0x7ff;
    u32 magh = u32(display >> 23) & 15, dw = u32(display >> 32) & 0xfff, dh = u32(display >> 44) & 0x7ff;
    u32 w = (dw + 1) / (magh + 1), h = dh + 1;
    if (!display) { w = 512; h = 448; }
    if (!fbw) fbw = (w + 63) / 64;
    if (w > 1024) w = 1024;
    if (h > 1024) h = 1024;
    std::vector<u8> rgb(size_t(w) * h * 3);
    for (u32 y = 0; y < h; ++y)
        for (u32 x = 0; x < w; ++x) {
            u32 p = gs.FramePixel(fbp, fbw, psm, x + dbx, y + dby);
            u8* o = &rgb[(size_t(y) * w + x) * 3];
            o[0] = u8(p); o[1] = u8(p >> 8); o[2] = u8(p >> 16);
        }
    char name[512];
    std::snprintf(name, sizeof name, "%s/frame%06u.png", shot_dir.c_str(), frame);
    WritePng(name, rgb, w, h);
    xsg_logf("[shot] dispfb1 %llx dispfb2 %llx\n", (unsigned long long)Priv(0x70), (unsigned long long)Priv(0x90));
    xsg_logf("[shot] %s pmode %llx smode2 %llx FBP 0x%x fbw %u psm %u %ux%u (prims %llu)\n", name,
             (unsigned long long)pmode, (unsigned long long)smode2, fbp, fbw, psm, w, h,
             (unsigned long long)gs.primitives());
    // XSG_SHOT_FBP=base[:fbw[:psm]],... (FBP as FRAME has it, in 2048-word pages, hex): extra buffers, 512x448.
    if (const char* e = std::getenv("XSG_SHOT_FBP"))
        for (const char* p = e; *p;) {
            char* q;
            u32 b = u32(std::strtoul(p, &q, 16)), bw = 8, ps = 0;
            if (q == p) break;
            if (*q == ':') bw = u32(std::strtoul(q + 1, &q, 0));
            if (*q == ':') ps = u32(std::strtoul(q + 1, &q, 0));
            std::vector<u8> v(512 * 448 * 3);
            for (u32 y = 0; y < 448; ++y)
                for (u32 x = 0; x < 512; ++x) {
                    u32 px = gs.FramePixel(b, bw, ps, x, y);
                    u8* o = &v[(size_t(y) * 512 + x) * 3];
                    o[0] = u8(px); o[1] = u8(px >> 8); o[2] = u8(px >> 16);
                }
            std::snprintf(name, sizeof name, "%s/fb%03x_%06u.png", shot_dir.c_str(), b, frame);
            WritePng(name, v, 512, 448);
            p = *q ? q + 1 : q;
        }
    if (shot_vram) {
        std::vector<u8> v(1024 * 1024 * 3);
        for (u32 y = 0; y < 1024; ++y)
            for (u32 x = 0; x < 1024; ++x) {
                u32 p = gs.FramePixel(0, 16, 0, x, y);
                u8* o = &v[(size_t(y) * 1024 + x) * 3];
                o[0] = u8(p); o[1] = u8(p >> 8); o[2] = u8(p >> 16);
            }
        std::snprintf(name, sizeof name, "%s/vram%06u.png", shot_dir.c_str(), frame);
        WritePng(name, v, 1024, 1024);
    }
}

}  // namespace

/* ---------------------------------------------------------------- C API */

extern "C" {

/* Called by the DMAC stand-in for a started channel (CHCR.STR set). Runs the
   transfer for ch1/ch2 and clears STR. */
void xsg_dma_run(int ch, uint32_t base) {
    std::lock_guard<std::recursive_mutex> l(g_lock);
    volatile u32* chcr = reinterpret_cast<volatile u32*>(uintptr_t(base));
    if ((ch == 1 || ch == 2) && (*chcr & 0x100)) {
        RunChannel(ch, base);
        __atomic_fetch_and(chcr, ~0x100u, __ATOMIC_SEQ_CST);
    }
}

/* Any started VIF1/GIF transfer, run now (keeps sceGs* calls in order). */
void xsg_dma_poll(void) {
    std::lock_guard<std::recursive_mutex> l(g_lock);
    static const u32 bases[3] = {0, 0x10009000, 0x1000A000};
    for (int ch = 1; ch <= 2; ++ch) {
        volatile u32* chcr = reinterpret_cast<volatile u32*>(uintptr_t(bases[ch]));
        if (*chcr & 0x100) {
            RunChannel(ch, bases[ch]);
            __atomic_fetch_and(chcr, ~0x100u, __ATOMIC_SEQ_CST);
        }
    }
}

/* xglDmaDirectNormal / xglDmaDirectSrcChain (channel index 0..2): run the
   transfer synchronously on the game's thread, right at the CHCR write. */
void xsg_dma_kick(uint32_t channel) {
    if (channel != 1 && channel != 2) return;
    xsg_dma_poll();
}

/* A GIF packet (through EOP) on PATH3, as sceGsPutDrawEnv/ExecLoadImage send it. */
void xsg_gs_send_packet(const void* p) {
    std::lock_guard<std::recursive_mutex> l(g_lock);
    xsg_dma_poll();
    const u8* b = static_cast<const u8*>(p);
    GifSend(3, b, GifPacketLength(b, 1u << 20));
}

void xsg_gs_send(const void* p, uint32_t qwords) {
    std::lock_guard<std::recursive_mutex> l(g_lock);
    GifSend(3, static_cast<const u8*>(p), qwords);
}

void xsg_gs_vblank(unsigned frame) {
    if (!shot_init) ShotInit();
    // XSG_GS_TRACE=N: the software GS's primitive/transfer trace for frame N only (stderr).
    static const long trace_frame = std::getenv("XSG_GS_TRACE") ? std::atol(std::getenv("XSG_GS_TRACE")) : -1;
    if (trace_frame >= 0 && (frame == u32(trace_frame) || frame == u32(trace_frame) + 1)) {
        std::lock_guard<std::recursive_mutex> l(g_lock);
        G().trace = frame == u32(trace_frame);
    }
    static const long vu_dump = std::getenv("XSG_VU_DUMP") ? std::atol(std::getenv("XSG_VU_DUMP")) : -1;
    if (long(frame) == vu_dump) {
        std::lock_guard<std::recursive_mutex> l(g_lock);
        if (!shot_init) ShotInit();
        std::string n = shot_dir + "/vu1_micro.bin";
        if (std::FILE* f = std::fopen(n.c_str(), "wb")) { std::fwrite(Vu1Micro(), 1, 0x4000, f); std::fclose(f); }
        n = shot_dir + "/vu1_data.bin";
        if (std::FILE* f = std::fopen(n.c_str(), "wb")) { std::fwrite(Vu1Data(), 1, 0x4000, f); std::fclose(f); }
        xsg_logf("[vu1] dumped VU1 memories to %s\n", shot_dir.c_str());
    }
    static const long vu_log = std::getenv("XSG_VU_LOG") ? std::atol(std::getenv("XSG_VU_LOG")) : 0;
    if (vu_log > 0 && frame % u32(vu_log) == 0) {
        std::lock_guard<std::recursive_mutex> l(g_lock);
        static xsgvu::Stats last;
        static u64 lp = 0, lprims = 0, lpq = 0;
        static u64 lt[8];
        auto& v = xsgvu::GetStats();
        xsg_logf("[vu1] frame %u: runs %llu pairs %llu kicks %llu (tags %llu, qw %llu, prims %llu: pt %llu ln %llu ls %llu "
                 "tri %llu ts %llu tf %llu spr %llu) flagreads %llu clipreads %llu bad %llu\n", frame,
                 (unsigned long long)(v.runs - last.runs), (unsigned long long)(v.pairs - last.pairs),
                 (unsigned long long)(v.kicks - last.kicks), (unsigned long long)(st.p1_tags - lp),
                 (unsigned long long)(st.p1_qw - lpq), (unsigned long long)(st.p1_prims - lprims),
                 (unsigned long long)(st.p1_prim_type[0] - lt[0]), (unsigned long long)(st.p1_prim_type[1] - lt[1]),
                 (unsigned long long)(st.p1_prim_type[2] - lt[2]), (unsigned long long)(st.p1_prim_type[3] - lt[3]),
                 (unsigned long long)(st.p1_prim_type[4] - lt[4]), (unsigned long long)(st.p1_prim_type[5] - lt[5]),
                 (unsigned long long)(st.p1_prim_type[6] - lt[6]), (unsigned long long)(v.flag_reads - last.flag_reads),
                 (unsigned long long)(v.clip_reads - last.clip_reads), (unsigned long long)v.bad);
        last = v; lp = st.p1_tags; lprims = st.p1_prims; lpq = st.p1_qw;
        for (int i = 0; i < 8; ++i) lt[i] = st.p1_prim_type[i];
        if (frame % (u32(vu_log) * 100) == 0) xsgvu::DumpHistogram();
    }
    static bool stream_init = false;
    if (!stream_init) { stream_init = true; StreamInit(); }
    if (g_stream) {
        std::lock_guard<std::recursive_mutex> l(g_lock);
        xsg_dma_poll();
        StreamFrame();
    }
    if (!shot_frames.count(frame) && !(shot_every && frame % shot_every == 0)) return;
    std::lock_guard<std::recursive_mutex> l(g_lock);
    xsg_dma_poll();
    Shot(frame);
    xsg_logf("[gs] frame %u: GIF kicks %llu (%llu qw), VIF1 kicks %llu (%llu qw), tags %llu, DIRECT %llu qw, "
             "MSCAL %llu, MSCNT %llu, MPG %llu, UNPACK %llu\n",
             frame, (unsigned long long)st.gif_kicks, (unsigned long long)st.gif_qw,
             (unsigned long long)st.vif_kicks, (unsigned long long)st.vif_qw, (unsigned long long)st.tags,
             (unsigned long long)st.direct_qw, (unsigned long long)st.mscal, (unsigned long long)st.mscnt,
             (unsigned long long)st.mpg, (unsigned long long)st.unpack);
}

}  // extern "C"

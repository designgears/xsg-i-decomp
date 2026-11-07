/*
 * XSG_VU1_CAPTURE=dir: record VU1 runs and their XGKICK output (see
 * vu1_capture.h for the file format).
 *
 *   XSG_VU1_CAPTURE_FROM=F    first captured vblank (default 1300)
 *   XSG_VU1_CAPTURE_FRAMES=N  number of captured frames (default 4)
 *   XSG_VU1_CAPTURE_EVERY=K   capture every K-th vblank from F (default 100)
 *
 * Called from vu1.cpp (CapBegin/CapKick/CapEnd); VU0 runs are not recorded.
 */
#include <cstdio>
#include "xsg_settings.h"
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include "xsg_rt.h"
#include "vu1_capture.h"
}

namespace xsgvu {

namespace {
struct Cap {
    int init = 0;               // 0 unknown, 1 on, -1 off
    char dir[512] = {};
    unsigned from = 1300, frames = 4, every = 100;
    FILE* fp = nullptr;
    unsigned fp_frame = ~0u;
    unsigned seq = 0, micro_id = 0;
    bool have_micro = false;
    unsigned char micro[16384];
    bool active = false;        // inside a captured run
    Vu1CapRun run;
    std::vector<unsigned char> data;   // data memory at entry
    std::vector<unsigned char> kicks;
    unsigned last_end = ~0u;
    std::vector<uint32_t> upk;
} c;

bool Wanted(unsigned f) {
    if (f < c.from) return false;
    unsigned k = (f - c.from);
    if (k % c.every) return false;
    return k / c.every < c.frames;
}

void W(const void* p, size_t n) { std::fwrite(p, 1, n, c.fp); }
void Tag(uint32_t t, uint32_t n) { W(&t, 4); W(&n, 4); }
}  // namespace

bool CapOn() {
    if (c.init == 0) {
        const char* d = xsg_cfg("debug.vu1_capture");
        if (!d || !*d) { c.init = -1; return false; }
        std::snprintf(c.dir, sizeof c.dir, "%s", d);
        if (const char* e = xsg_cfg("debug.vu1_capture_from")) c.from = unsigned(std::atol(e));
        if (const char* e = xsg_cfg("debug.vu1_capture_frames")) c.frames = unsigned(std::atol(e));
        if (const char* e = xsg_cfg("debug.vu1_capture_every")) c.every = unsigned(std::atol(e));
        if (!c.every) c.every = 1;
        c.init = 1;
        xsg_logf("[vu1cap] capturing %u frames every %u from vblank %u to %s\n", c.frames, c.every, c.from, c.dir);
    }
    return c.init > 0;
}

void CapBegin(unsigned pc, unsigned top, unsigned itop, const Vu1CapRegs& regs,
              const unsigned char* micro, const unsigned char* data) {
    const unsigned f = xsg_frame;
    c.active = false;
    if (!Wanted(f)) {
        if (c.fp) { std::fclose(c.fp); c.fp = nullptr; }
        c.last_end = ~0u;
        return;
    }
    if (c.fp_frame != f || !c.fp) {
        if (c.fp) std::fclose(c.fp);
        char path[600];
        std::snprintf(path, sizeof path, "%s/vu1cap_%06u.bin", c.dir, f);
        c.fp = std::fopen(path, "wb");
        if (!c.fp) { xsg_logf("[vu1cap] cannot write %s\n", path); return; }
        c.fp_frame = f;
        c.seq = 0;
        c.micro_id = 0;
        c.have_micro = false;
        xsg_logf("[vu1cap] frame %u -> %s\n", f, path);
    }
    if (!c.have_micro || std::memcmp(c.micro, micro, sizeof c.micro)) {
        if (c.have_micro) c.micro_id++;
        std::memcpy(c.micro, micro, sizeof c.micro);
        c.have_micro = true;
        Tag(VU1CAP_TAG_MICR, sizeof c.micro);
        W(c.micro, sizeof c.micro);
    }
    if (!c.upk.empty()) {
        Tag(VU1CAP_TAG_UPK, uint32_t(c.upk.size() * 4));
        W(c.upk.data(), c.upk.size() * 4);
        c.upk.clear();
    }
    std::memset(&c.run, 0, sizeof c.run);
    c.run.frame = f;
    c.run.seq = c.seq++;
    c.run.pc = pc;
    c.run.top = top;
    c.run.itop = itop;
    c.run.kind = pc == c.last_end ? 1 : 0;
    c.run.micro_id = c.micro_id;
    c.run.regs = regs;
    c.data.assign(data, data + 16384);
    c.kicks.clear();
    c.active = true;
}

/* The GIF packet at qword q of VU1 data memory, walked to EOP as Path1Kick does. */
void CapKick(const unsigned char* data, unsigned q) {
    if (!c.active) return;
    std::vector<unsigned char> pk;
    unsigned start = q;
    for (unsigned guard = 0; guard < 1024; ++guard) {
        uint64_t lo;
        std::memcpy(&lo, data + (q & 0x3ff) * 16, 8);
        const unsigned nloop = unsigned(lo & 0x7fff), eop = unsigned(lo >> 15) & 1;
        const unsigned flg = unsigned(lo >> 58) & 3;
        unsigned nreg = unsigned(lo >> 60) & 15;
        if (!nreg) nreg = 16;
        unsigned body = flg == 0 ? nloop * nreg : flg == 1 ? (nloop * nreg + 1) / 2 : nloop;
        if (body > 1024) break;
        for (unsigned i = 0; i <= body; ++i, ++q) {
            const unsigned char* p = data + (q & 0x3ff) * 16;
            pk.insert(pk.end(), p, p + 16);
        }
        if (eop) break;
    }
    uint32_t hdr[2] = {start & 0x3ff, uint32_t(pk.size() / 16)};
    c.kicks.insert(c.kicks.end(), (unsigned char*)hdr, (unsigned char*)hdr + 8);
    c.kicks.insert(c.kicks.end(), pk.begin(), pk.end());
    c.run.nkick++;
}

void CapUnpack(unsigned vifcode, unsigned cycle_mode) {
    if (!CapOn() || !Wanted(xsg_frame)) { c.upk.clear(); return; }
    if (c.upk.size() < 4096) { c.upk.push_back(vifcode); c.upk.push_back(cycle_mode); }
}

void CapEnd(unsigned pairs, unsigned end_pc) {
    c.last_end = end_pc;
    if (!c.active || !c.fp) return;
    c.active = false;
    c.run.pairs = pairs;
    c.run.end_pc = end_pc;
    Tag(VU1CAP_TAG_RUN, uint32_t(sizeof c.run + c.data.size() + c.kicks.size()));
    W(&c.run, sizeof c.run);
    W(c.data.data(), c.data.size());
    W(c.kicks.data(), c.kicks.size());
}

}  // namespace xsgvu

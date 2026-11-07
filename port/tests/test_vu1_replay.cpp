/*
 * Stage 10: replay XSG_VU1_CAPTURE files through the VU1 interpreter
 * (port/runtime/vu1.cpp) and check it reproduces the capture bit for bit:
 * the number of pairs, the stop address and every XGKICKed GIF packet.
 * A capture made with one interpreter version is the reference for another.
 *
 *   cmake --build port/build --target test_vu1_replay
 *   port/build/test_vu1_replay /tmp/vu1cap/vu1cap_*.bin    # exit 0 = identical
 *   ... --bench N   replay each file N times and print pairs/s
 */
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/mman.h>
#include <vector>

#include "vu1.h"

extern "C" {
volatile unsigned xsg_frame;
void xsg_logf(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); }
}
/* the capture writer is not linked: capture stays off */
namespace xsgvu {
bool CapOn() { return false; }
void CapBegin(unsigned, unsigned, unsigned, const Vu1CapRegs&, const unsigned char*, const unsigned char*) {}
void CapKick(const unsigned char*, unsigned) {}
void CapEnd(unsigned, unsigned) {}
void CapUnpack(unsigned, unsigned) {}
}

static unsigned char* const kMicro = reinterpret_cast<unsigned char*>(0x11008000u);
static unsigned char* const kData = reinterpret_cast<unsigned char*>(0x1100C000u);

struct Kick { uint32_t q; std::vector<unsigned char> bytes; };
static std::vector<Kick> g_kicks;
static bool g_record = true;

/* GIF packet length from qaddr to EOP, as gs_bridge's Path1Kick walks it */
static void OnKick(const std::uint8_t* data, std::uint32_t q) {
    if (!g_record) return;
    Kick k;
    k.q = q & 0x3ff;
    /* the same walk as vu1_capture.cpp CapKick */
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
            k.bytes.insert(k.bytes.end(), p, p + 16);
        }
        if (eop) break;
    }
    g_kicks.push_back(std::move(k));
}

struct Rec { uint32_t tag, bytes; const unsigned char* p; };

int main(int argc, char** argv) {
    int bench = 0, verbose = 0;
    std::vector<const char*> files;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--bench") && i + 1 < argc) bench = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--verbose")) verbose = 1;
        else files.push_back(argv[i]);
    }
    if (mmap(reinterpret_cast<void*>(0x11000000), 0x10000, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED) {
        perror("mmap VU memory");
        return 2;
    }
    xsgvu::SetKick(OnKick);
    unsigned long long runs = 0, bad = 0, pairs = 0;
    for (const char* fn : files) {
        FILE* f = std::fopen(fn, "rb");
        if (!f) { perror(fn); return 2; }
        std::fseek(f, 0, SEEK_END);
        long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        std::vector<unsigned char> buf(static_cast<size_t>(sz));
        if (std::fread(buf.data(), 1, buf.size(), f) != buf.size()) { perror(fn); return 2; }
        std::fclose(f);
        std::vector<Rec> recs;
        for (size_t o = 0; o + 8 <= buf.size();) {
            Rec r;
            std::memcpy(&r.tag, &buf[o], 4);
            std::memcpy(&r.bytes, &buf[o + 4], 4);
            r.p = &buf[o + 8];
            recs.push_back(r);
            o += 8 + r.bytes;
        }
        unsigned long long frame_bad = 0, frame_runs = 0, state_bad = 0;
        /* Full-state check: the registers a run leaves must be the entry
           registers of the next run of the same file (the EE does not touch
           VU1 registers in micro mode; VU0 runs save/restore them). */
        Vu1CapRegs end_regs;
        bool have_end = false;
        uint32_t end_seq = 0;
        for (int pass = 0; pass < (bench ? bench : 1); ++pass) {
            g_record = pass == 0;
            timespec t0, t1;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            unsigned long long bench_pairs = 0;
            for (const Rec& r : recs) {
                if (r.tag == VU1CAP_TAG_MICR) { std::memcpy(kMicro, r.p, 16384); continue; }
                if (r.tag != VU1CAP_TAG_RUN) continue;
                Vu1CapRun h;
                std::memcpy(&h, r.p, sizeof h);
                std::memcpy(kData, r.p + sizeof h, 16384);
                if (pass == 0 && have_end && h.seq == end_seq + 1) {
                    Vu1CapRegs e = h.regs;
                    e.pad = 0;
                    if (std::memcmp(&e, &end_regs, sizeof e)) {
                        state_bad++;
                        if (verbose || state_bad <= 3) {
                            std::printf("%s: state after run %u differs from entry of run %u:", fn, end_seq, h.seq);
                            const uint32_t* a = reinterpret_cast<const uint32_t*>(&end_regs);
                            const uint32_t* b = reinterpret_cast<const uint32_t*>(&e);
                            for (size_t w = 0; w < sizeof e / 4; ++w)
                                if (a[w] != b[w]) std::printf(" [w%zu %08x/%08x]", w, a[w], b[w]);
                            std::printf("\n");
                        }
                    }
                }
                xsgvu::LoadRegs(h.regs);
                g_kicks.clear();
                const unsigned long long n = xsgvu::Run(h.pc, h.top, h.itop);
                bench_pairs += n;
                if (pass) continue;
                xsgvu::SaveRegs(end_regs);
                end_regs.pad = 0;
                have_end = true;
                end_seq = h.seq;
                frame_runs++;
                bool ok = n == h.pairs && xsgvu::Resume() == h.end_pc && g_kicks.size() == h.nkick;
                const unsigned char* k = r.p + sizeof h + 16384;
                for (uint32_t i = 0; ok && i < h.nkick; ++i) {
                    uint32_t q, nqw;
                    std::memcpy(&q, k, 4);
                    std::memcpy(&nqw, k + 4, 4);
                    k += 8;
                    ok = g_kicks[i].q == q && g_kicks[i].bytes.size() == size_t(nqw) * 16 &&
                         !std::memcmp(g_kicks[i].bytes.data(), k, size_t(nqw) * 16);
                    k += size_t(nqw) * 16;
                }
                if (!ok) {
                    frame_bad++;
                    if (verbose || frame_bad <= 3)
                        std::printf("%s: run %u (pc 0x%x) differs: pairs %llu/%u end 0x%x/0x%x kicks %zu/%u\n", fn, h.seq,
                                    h.pc, n, h.pairs, xsgvu::Resume(), h.end_pc, g_kicks.size(), h.nkick);
                }
            }
            clock_gettime(CLOCK_MONOTONIC, &t1);
            if (pass == 0) pairs += bench_pairs;
            if (bench) {
                double dt = double(t1.tv_sec - t0.tv_sec) + 1e-9 * double(t1.tv_nsec - t0.tv_nsec);
                std::printf("%s pass %d: %llu pairs in %.3f s = %.1f M pairs/s\n", fn, pass, bench_pairs, dt,
                            1e-6 * double(bench_pairs) / dt);
            }
        }
        std::printf("%s: %llu runs, %llu differ, %llu state mismatches\n", fn, frame_runs, frame_bad, state_bad);
        runs += frame_runs;
        bad += frame_bad + state_bad;
    }
    std::printf("total: %llu runs, %llu pairs, %llu differ\n", runs, pairs, bad);
    return bad ? 1 : 0;
}

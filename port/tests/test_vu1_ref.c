/*
 * test_vu1_ref: run the VU1 C references (port/render/vu1_ref/) against
 * XSG_VU1_CAPTURE files and compare their GIF output with what the
 * interpreter (port/runtime/vu1.cpp) XGKICKed in the game.
 *
 *   test_vu1_ref [--blob vudata_rest.databin.bin] [--tol N] [--verbose] cap...
 *
 * Every 'RUN ' record is one VU1 program run: its entry registers and data
 * memory go into the reference; the reference's kicks are compared with the
 * recorded ones.
 *
 * Comparison.  The references repeat the interpreter's float operations in
 * the same order with the same rounding mode (round toward zero, FTZ/DAZ,
 * +-FLT_MAX clamping), so the expected result is bit-exact and that is what
 * "match" means.  --tol N additionally accepts packets whose only differences
 * are, per GIF register (PACKED, decoded from the tag):
 *   ST/STQ (0x2)    float lanes within N ulp
 *   RGBAQ  (0x1)    integer lanes exactly equal
 *   XYZF2/XYZ2      X/Y within N (1/16 pixel units), Z within N, F and ADC exact
 *   anything else   exact
 * That is the budget a GPU re-implementation (a vertex shader in IEEE
 * round-to-nearest, possibly FMA-contracted) needs: one product-sum rounding
 * per matrix row moves a coordinate by a few ulp, which after ftoi4 is at most
 * one 1/16-pixel step (N = 1 or 2).  ADC must be exact: it decides which strip
 * triangles are drawn.
 *
 * Exit status 0 when every supported run matches (exactly, or within --tol).
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../render/vu1_ref/vu1_ref.h"
#include "../render/vu1_ref/vu_float.h"

#define MAXSETS 8
static uint8_t g_set_micro[MAXSETS][16384];
static uint32_t g_set_size[MAXSETS];
static int g_nsets;

static int load_blob(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 0; }
    static uint8_t d[1 << 17];
    size_t n = fread(d, 1, sizeof d, f);
    fclose(f);
    size_t off = 0;
    while (off + 16 <= n && g_nsets < MAXSETS) {
        uint32_t qwc = (uint32_t)d[off] | (uint32_t)d[off + 1] << 8;
        size_t end = off + 16 + (size_t)qwc * 16, p = off + 8;
        uint32_t size = 0;
        while (p + 4 <= end) {
            uint32_t w;
            memcpy(&w, d + p, 4);
            if (((w >> 24) & 0x7f) == 0x4a) {
                uint32_t num = ((w >> 16) & 0xff) ? ((w >> 16) & 0xff) : 256, a = (w & 0xffff) * 8;
                memcpy(g_set_micro[g_nsets] + a, d + p + 4, num * 8);
                if (a + num * 8 > size) size = a + num * 8;
                p += 4 + num * 8;
            } else p += 4;
        }
        if (!size) break;
        g_set_size[g_nsets++] = size;
        off = end;
    }
    return g_nsets;
}

static int which_set(const uint8_t* micro) {
    for (int i = 0; i < g_nsets; ++i)
        if (!memcmp(micro, g_set_micro[i], g_set_size[i])) return i;
    return -1;
}

static int ulp_close(uint32_t a, uint32_t b, int n) {
    if (a == b) return 1;
    if ((a ^ b) & 0x80000000u) return 0;
    int64_t d = (int64_t)(a & 0x7fffffff) - (int64_t)(b & 0x7fffffff);
    return d <= n && d >= -n;
}

/* Packet compare with tolerance (see the header). */
static int packet_close(const uint8_t* a, const uint8_t* b, uint32_t nqw, int tol) {
    uint32_t q = 0;
    while (q < nqw) {
        if (memcmp(a + q * 16, b + q * 16, 16)) return 0;   /* tags exact */
        uint64_t lo, hi;
        memcpy(&lo, a + q * 16, 8);
        memcpy(&hi, a + q * 16 + 8, 8);
        uint32_t nloop = lo & 0x7fff, flg = (lo >> 58) & 3, nreg = (lo >> 60) & 15;
        if (!nreg) nreg = 16;
        q++;
        if (flg != 0) {
            uint32_t body = flg == 1 ? (nloop * nreg + 1) / 2 : nloop;
            if (memcmp(a + q * 16, b + q * 16, body * 16)) return 0;
            q += body;
            continue;
        }
        for (uint32_t l = 0; l < nloop; ++l)
            for (uint32_t r = 0; r < nreg; ++r, ++q) {
                uint32_t x[4], y[4];
                memcpy(x, a + q * 16, 16);
                memcpy(y, b + q * 16, 16);
                uint32_t reg = (hi >> (4 * r)) & 15;
                if (reg == 2) {
                    for (int k = 0; k < 3; ++k) if (!ulp_close(x[k], y[k], tol)) return 0;
                } else if (reg == 4 || reg == 5) {
                    for (int k = 0; k < 3; ++k) {
                        int64_t d = (int64_t)(int32_t)x[k] - (int64_t)(int32_t)y[k];
                        if (d > tol || d < -tol) return 0;
                    }
                    if (x[3] != y[3]) return 0;
                } else if (memcmp(x, y, 16)) return 0;
            }
    }
    return 1;
}

typedef struct Stat { char name[48]; unsigned runs, exact, close, fail, kicks, verts; } Stat;
static Stat g_stat[64];
static int g_nstat;
static Stat* stat_for(int set, const char* name) {
    char key[48];
    snprintf(key, sizeof key, "s%d:%s", set, name);
    for (int i = 0; i < g_nstat; ++i) if (!strcmp(g_stat[i].name, key)) return &g_stat[i];
    if (g_nstat == 64) return &g_stat[63];
    Stat* s = &g_stat[g_nstat++];
    memset(s, 0, sizeof *s);
    strcpy(s->name, key);
    return s;
}

typedef struct Unsup { int set; uint32_t pc, loop; unsigned n; } Unsup;
static Unsup g_unsup[256];
static int g_nunsup;

int main(int argc, char** argv) {
    const char* blob = "build/main/assets/main/blobs/vudata_rest.databin.bin";
    int tol = 0, verbose = 0, nfiles = 0;
    const char* files[64];
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--blob") && i + 1 < argc) blob = argv[++i];
        else if (!strcmp(argv[i], "--tol") && i + 1 < argc) tol = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--verbose")) verbose = 1;
        else if (nfiles < 64) files[nfiles++] = argv[i];
    }
    if (!nfiles || !load_blob(blob)) {
        fprintf(stderr, "usage: test_vu1_ref [--blob vudata.bin] [--tol N] [--verbose] vu1cap_*.bin\n");
        return 2;
    }
    static Vu1Ref r;
    static uint8_t micro[16384];
    static uint8_t* body;
    static size_t body_cap;
    unsigned total = 0, unsupported = 0, fails = 0;
    int cur_set = -1;
    for (int fi = 0; fi < nfiles; ++fi) {
        FILE* f = fopen(files[fi], "rb");
        if (!f) { fprintf(stderr, "cannot open %s\n", files[fi]); return 2; }
        uint32_t h[2];
        while (fread(h, 4, 2, f) == 2) {
            if (h[1] > body_cap) { body_cap = h[1]; body = realloc(body, body_cap); }
            if (fread(body, 1, h[1], f) != h[1]) break;
            if (h[0] == VU1CAP_TAG_MICR) {
                memcpy(micro, body, 16384);
                cur_set = which_set(micro);
                continue;
            }
            if (h[0] != VU1CAP_TAG_RUN) continue;
            Vu1CapRun run;
            memcpy(&run, body, sizeof run);
            const uint8_t* data = body + sizeof run;
            const uint8_t* kp = data + 16384;
            total++;
            memset(&r, 0, offsetof(Vu1Ref, mem));
            r.set = cur_set;
            r.pc = run.pc;
            r.top = run.top;
            r.itop = run.itop;
            r.regs = run.regs;
            memcpy(r.mem, data, 16384);
            const char* variant = "?";
            unsigned csr = vu_ref_fpu_enter();
            int rc = vu1_ref_run(&r, &variant);
            vu_ref_fpu_leave(csr);
            if (rc != VU1REF_OK) {
                unsupported++;
                uint32_t loop = run.kind ? run.regs.vi[15] * 8 : 0;   /* MSCNT: the entry's loop */
                int k;
                for (k = 0; k < g_nunsup; ++k)
                    if (g_unsup[k].set == cur_set && g_unsup[k].pc == run.pc && g_unsup[k].loop == loop) break;
                if (k == g_nunsup && k < 256) { g_unsup[k].set = cur_set; g_unsup[k].pc = run.pc; g_unsup[k].loop = loop; g_unsup[k].n = 0; g_nunsup++; }
                if (k < 256) g_unsup[k].n++;
                continue;
            }
            Stat* s = stat_for(cur_set, variant);
            s->runs++;
            int exact = run.nkick == r.nkick, close = exact;
            for (uint32_t k = 0; k < run.nkick && exact | close; ++k) {
                uint32_t q, nqw;
                memcpy(&q, kp, 4);
                memcpy(&nqw, kp + 4, 4);
                const uint8_t* pk = kp + 8;
                kp += 8 + nqw * 16;
                if (k >= r.nkick) { exact = close = 0; break; }
                s->kicks++;
                if (nqw > 1) {
                    uint64_t lo;
                    memcpy(&lo, pk, 8);
                    s->verts += lo & 0x7fff;
                }
                if (r.kick[k].qaddr != q || r.kick[k].nqw != nqw) { exact = close = 0; break; }
                if (memcmp(r.kick[k].qw, pk, nqw * 16)) {
                    exact = 0;
                    if (!tol || !packet_close(r.kick[k].qw, pk, nqw, tol)) {
                        close = 0;
                        if (verbose && s->fail < 3) {
                            for (uint32_t i = 0; i < nqw; ++i)
                                if (memcmp(r.kick[k].qw + i * 16, pk + i * 16, 16)) {
                                    const uint32_t* a = (const uint32_t*)(r.kick[k].qw + i * 16);
                                    const uint32_t* b = (const uint32_t*)(pk + i * 16);
                                    printf("  %s frame %u seq %u kick %u qw %u: ref %08x %08x %08x %08x  vu1 %08x %08x %08x %08x\n",
                                           s->name, run.frame, run.seq, k, i, a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
                                    break;
                                }
                        }
                    }
                }
            }
            if (exact) s->exact++;
            else if (close) s->close++;
            else { s->fail++; fails++; }
        }
        fclose(f);
    }
    printf("%-28s %8s %8s %8s %6s %8s %9s\n", "set:program", "runs", "exact", "in-tol", "fail", "kicks", "vertices");
    for (int i = 0; i < g_nstat; ++i)
        printf("%-28s %8u %8u %8u %6u %8u %9u\n", g_stat[i].name, g_stat[i].runs, g_stat[i].exact, g_stat[i].close,
               g_stat[i].fail, g_stat[i].kicks, g_stat[i].verts);
    printf("runs %u, with a reference %u, failing %u, no reference %u\n", total, total - unsupported, fails, unsupported);
    for (int k = 0; k < g_nunsup; ++k)
        printf("  no reference: set %d pc 0x%04x (loop 0x%04x): %u runs\n", g_unsup[k].set, g_unsup[k].pc,
               g_unsup[k].loop, g_unsup[k].n);
    return fails ? 1 : 0;
}

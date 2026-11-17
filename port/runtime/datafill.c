/* Copy the game's data into the executable from the player's disc (xsg_datafill.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xsg_datafill.h"
#include "xsg_rt.h"
#include "xsg_settings.h"

int xsg_game_file(const char *name, unsigned char **buf, uint32_t *len)
{
    char path[1024], disc[64];
    const char *root = xsg_cfg("game.game_dir");
    snprintf(path, sizeof path, "%s/extracted-iso/%s", root ? root : XSG_DEFAULT_GAME_DIR, name);
    *buf = NULL;
    *len = 0;
    FILE *f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n > 0 && (*buf = malloc((size_t)n)) && fread(*buf, 1, (size_t)n, f) == (size_t)n)
            *len = (uint32_t)n;
        fclose(f);
        if (*len)
            return 1;
        free(*buf);
        *buf = NULL;
    }
    snprintf(disc, sizeof disc, "\\%s;1", name);
    for (char *c = disc; *c; c++)
        if (*c == '/')
            *c = '\\';
    return xsg_disc_read_file(disc, (void **)buf, len) == 0 && *len;
}

/* ---- SHA-256 (FIPS 180-4) ---- */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void block(uint32_t h[8], const unsigned char *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, k;
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = k + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        k = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + t2;
    }
    h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e, h[5] += f, h[6] += g, h[7] += k;
}

void xsg_sha256_hex(const unsigned char *buf, uint32_t len, char out[65])
{
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint32_t i = 0;
    for (; len - i >= 64; i += 64)
        block(h, buf + i);
    unsigned char t[128] = {0};
    uint32_t r = len - i;
    memcpy(t, buf + i, r);
    t[r] = 0x80;
    uint32_t n = r < 56 ? 64 : 128;
    uint64_t bits = (uint64_t)len * 8;
    for (int j = 0; j < 8; j++)
        t[n - 1 - j] = (unsigned char)(bits >> (8 * j));
    block(h, t);
    if (n == 128)
        block(h, t + 64);
    for (int j = 0; j < 8; j++)
        snprintf(out + 8 * j, 9, "%08x", h[j]);
}

int xsg_data_fill(void)
{
    int nfiles = 0;
    while (xsg_fill_files[nfiles].name)
        nfiles++;
    int ok = 1;
    uint32_t runs = 0, bytes = 0;
    for (int fi = 0; fi < nfiles && ok; fi++) {
        const struct xsg_fill_file *ff = &xsg_fill_files[fi];
        unsigned char *img;
        uint32_t len;
        if (!xsg_game_file(ff->name, &img, &len)) {
            xsg_logf("[data] %s: not found on the disc image\n", ff->name);
            ok = 0;
            break;
        }
        char sum[65];
        xsg_sha256_hex(img, len, sum);
        if (len != ff->size || strcmp(sum, ff->sha256)) {
            xsg_logf("[data] %s: %u bytes, sha256 %s; expected %u bytes, sha256 %s "
                     "(another release or a bad dump of the disc)\n",
                     ff->name, len, sum, ff->size, ff->sha256);
            free(img);
            ok = 0;
            break;
        }
        for (const struct xsg_fill_tab *t = xsg_fill_tabs; t->tab; t++) {
            if (t->file != fi)
                continue;
            for (const struct xsg_fill *e = t->tab; e->at; e++) {
                if (e->len != e->host_len || e->off + e->len > len) {
                    xsg_logf("[data] %s: bad fill run at %p (+0x%x, %u bytes, laid out %u)\n",
                             ff->name, (void *)e->at, e->off, e->len, e->host_len);
                    ok = 0;
                    continue;
                }
                memcpy(e->at, img + e->off, e->len);
                runs++;
                bytes += e->len;
            }
        }
        free(img);
    }
    if (ok)
        xsg_logf("[data] %u runs, %u bytes filled from %d disc files\n", runs, bytes, nfiles);
    return ok ? 0 : -1;
}

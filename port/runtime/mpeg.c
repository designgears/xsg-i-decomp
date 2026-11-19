/*
 * Host libmpeg (sceMpeg*) for the .pss movies, decoded in-process (mpeg2dec.c).
 *
 * The game's own player (src/main/xgl_1.c xglMpeg2Open/xglMpeg2Play) is kept
 * as it is.  It feeds PSS packs to sceMpegDemuxPss, which hands the video and
 * audio PES payloads to the game's stream callbacks (videoCallback copies the
 * MPEG-2 elementary stream into its DMA ring, audioCallback copies the ADPCM
 * into its sound ring).  On the PS2, libmpeg's decoder then pulls the ring
 * through the IPU: whenever the IPU input runs dry it calls the game's
 * nodataCallback, which programs D4 (toIPU) MADR/QWC/CHCR with the next
 * slice of the ring.  Here sceMpegGetPicture does the same: it calls the
 * nodata callback, takes the D4 slice the game programmed, and gives it to the
 * decoder until a picture (in display order) comes out, which is stored in the
 * 16x16 macroblock order the game's GS upload chain (setLoadImageTags)
 * expects: 0x400 bytes per macroblock, macroblock columns outermost.
 * [game] movies = 0 (XSG_MPEG=0) ends every movie at once.
 *
 * Pacing: the game's loop draws one picture per pass and does not wait for
 * the movie clock (on the PS2 the decode itself and the vblank-locked
 * renderer do that), so GetPicture waits for vblanks until the picture's
 * presentation time, 60/fps vblanks per picture.
 */
#include <stdlib.h>
#include <string.h>

#include "mpeg2dec.h"
#include "xsg_rt.h"
#include "xsg_settings.h"

extern int sceGsSyncV(int mode);

int xsg_mpeg_active; /* ipu.cpp leaves ch4 alone while set */

/* libmpeg's public sceMpeg header: the game reads width/height/frameCount
 * and flags (picture type in bits 0..2). */
typedef struct {
    int width, height, frameCount, pad;
    long long pts, dts;
    unsigned long long flags;
    long long pts2, dts2;
    unsigned long long flags2;
    void *sys;
} sceMpeg;

/* sceMpegCbDataStr: the game's callbacks read data (+8) and len (+0xc). */
typedef struct {
    int type;
    sceMpeg *mp;
    unsigned char *data;
    unsigned len;
    long long pts, dts;
} cb_str;

typedef int (*mpeg_cb)(sceMpeg *mp, void *data, void *arg);

#define D4_CHCR (*(volatile uint32_t *)0x1000b400)
#define D4_MADR (*(volatile uint32_t *)0x1000b410)
#define D4_QWC (*(volatile uint32_t *)0x1000b420)

static struct {
    mpeg_cb str_cb[4];
    void *str_arg[4];
    mpeg_cb cb[8];
    void *cb_arg[8];

    mpeg2dec *dec;           /* NULL: no movie open (or movies off) */
    unsigned char *pic;      /* the picture GetPicture hands out, RGBA */
    size_t pic_cap;
    int in_closed, empty_feeds, ended, over;
    uint32_t sc;             /* last 4 ES bytes: finds sequence_end_code */
    unsigned frames_out;
    unsigned first_vblank;
    unsigned long long bytes_in;
} M;

static int enabled(void)
{
    const char *e = xsg_cfg("game.movies");
    return !e || atoi(e) != 0;
}

static void close_input(void)
{
    if (!M.in_closed && M.dec)
        mpeg2dec_end(M.dec);
    M.in_closed = 1;
}

static void movie_close(void)
{
    mpeg2dec_free(M.dec);
    M.dec = NULL;
}

/* One nodata round: the game programs D4 with the next slice of its ring. */
static void feed_once(void)
{
    if (M.in_closed)
        return;
    if (M.ended) {
        close_input();
        return;
    }
    mpeg_cb cb = M.cb[1];
    if (!cb) {
        close_input();
        return;
    }
    D4_QWC = 0;
    struct { int type; sceMpeg *mp; } d = {1, NULL};
    cb(NULL, &d, M.cb_arg[1]);
    uint32_t qwc = D4_QWC & 0xffff, madr = D4_MADR;
    D4_CHCR &= ~0x100u;
    if (qwc == 0) {
        if (++M.empty_feeds >= 16) {
            xsg_logf("[mpeg] input ran dry after %llu bytes\n", M.bytes_in);
            M.ended = 1;
        }
        return;
    }
    M.empty_feeds = 0;
    const unsigned char *src = (const unsigned char *)(uintptr_t)(madr & 0x1fffffffu);
    size_t n = qwc * 16;
    for (size_t i = 0; i < n; i++) { /* sequence_end_code: the game feeds nothing after it */
        M.sc = M.sc << 8 | src[i];
        if (M.sc == 0x000001b7u)
            M.ended = 1;
    }
    mpeg2dec_push(M.dec, src, n);
    M.bytes_in += n;
}

/* Decode until a picture is ready in M.pic: 1, or 0 at the end of the movie. */
static int next_picture(int *type)
{
    if (!M.dec || M.over)
        return 0;
    for (;;) {
        int w = mpeg2dec_width(M.dec), h = mpeg2dec_height(M.dec);
        size_t need = (size_t)(w ? w : 1) * (h ? h : 1) * 4;
        if (need > M.pic_cap) {
            free(M.pic);
            M.pic = malloc(need);
            M.pic_cap = need;
        }
        int r = mpeg2dec_picture(M.dec, M.pic, type);
        if (r > 0)
            return 1;
        if (r < 0) {
            M.over = 1;
            return 0;
        }
        if (M.in_closed) { /* cannot happen: the decoder drains once input is closed */
            M.over = 1;
            return 0;
        }
        feed_once();
    }
}

/* ---- sceMpeg API ---------------------------------------------------- */
int sceMpegInit(void) { STUB_LOG(""); return 0; }

int sceMpegCreate(sceMpeg *mp, void *work, int size)
{
    STUB_LOG("%p %p %d", (void *)mp, work, size);
    movie_close();
    memset(M.str_cb, 0, sizeof M.str_cb);
    memset(M.cb, 0, sizeof M.cb);
    M.sc = 0xffffffffu;
    M.frames_out = 0;
    M.ended = M.over = 0;
    M.empty_feeds = 0;
    M.bytes_in = 0;
    M.in_closed = 1;
    memset(mp, 0, sizeof *mp);
    if (enabled()) {
        M.dec = mpeg2dec_new();
        M.in_closed = 0;
        xsg_mpeg_active = 1;
    }
    return 0;
}

int sceMpegDelete(sceMpeg *mp)
{
    (void)mp;
    if (M.dec)
        xsg_logf("[mpeg] closed after %u pictures (%llu ES bytes)\n", M.frames_out, M.bytes_in);
    movie_close();
    xsg_mpeg_active = 0;
    return 0;
}

int sceMpegReset(sceMpeg *mp) { (void)mp; return 0; }

int sceMpegAddCallback(sceMpeg *mp, int type, mpeg_cb cb, void *data)
{
    (void)mp;
    if (type >= 0 && type < 8) {
        M.cb[type] = cb;
        M.cb_arg[type] = data;
    }
    return 0;
}

int sceMpegAddStrCallback(sceMpeg *mp, int type, int ch, mpeg_cb cb, void *data)
{
    (void)mp;
    (void)ch;
    if (type >= 0 && type < 4) {
        M.str_cb[type] = cb;
        M.str_arg[type] = data;
    }
    return 0;
}

int sceMpegIsEnd(sceMpeg *mp)
{
    (void)mp;
    return !M.dec || M.over;
}

/* Demux whole PSS packets (MPEG-2 program stream); stop before a packet that
 * is cut off or that a callback refuses (its ring is full). */
int sceMpegDemuxPss(sceMpeg *mp, unsigned char *buf, int size)
{
    int i = 0;
    while (i + 4 <= size) {
        unsigned char *p = buf + i;
        if (p[0] != 0 || p[1] != 0 || p[2] != 1) {
            i++;
            continue;
        }
        unsigned code = p[3];
        if (code == 0xba) {
            if (i + 14 > size)
                break;
            int len = (p[4] & 0xc0) == 0x40 ? 14 + (p[13] & 7) : 12;
            if (i + len > size)
                break;
            i += len;
            continue;
        }
        if (code == 0xb9) {
            i += 4;
            continue;
        }
        if (code < 0xbb) {
            i++;
            continue;
        }
        if (i + 6 > size)
            break;
        int len = 6 + (p[4] << 8 | p[5]);
        if (i + len > size)
            break;
        int type = code >= 0xe0 && code <= 0xef ? 0 : code == 0xbd ? 3 : -1;
        if (type >= 0 && M.str_cb[type]) {
            int h = 9 + p[8];
            if (h <= len) {
                cb_str d = {type, mp, p + h, (unsigned)(len - h), 0, 0};
                if (!M.str_cb[type](mp, &d, M.str_arg[type]))
                    break;
            }
        }
        i += len;
    }
    return i;
}

int sceMpegGetPicture(sceMpeg *mp, void *rgb, int mbcount)
{
    int type = 1;
    if (!M.dec) {
        STUB_LOG("-> end of stream");
        return -1;
    }
    if (!next_picture(&type)) {
        xsg_logf("[mpeg] GetPicture: no more pictures\n");
        return -1;
    }
    int w = mpeg2dec_width(M.dec), h = mpeg2dec_height(M.dec), mbh = h / 16, n = (w / 16) * mbh;
    if (mbcount < n)
        n = mbcount;
    const unsigned char *f = M.pic;
    unsigned char *dst = (unsigned char *)(uintptr_t)((uintptr_t)rgb & 0x1fffffffu);
    for (int k = 0; k < n; k++) {
        int mx = k / mbh, my = k % mbh;
        for (int y = 0; y < 16; y++)
            memcpy(dst + k * 0x400 + y * 64, f + ((size_t)(my * 16 + y) * w + mx * 16) * 4, 64);
    }

    int num, den;
    mpeg2dec_rate(M.dec, &num, &den);
    mp->width = w;
    mp->height = h;
    mp->frameCount = (int)M.frames_out;
    mp->flags = (unsigned)type;
    mp->pts = mp->dts = 0;

    /* presentation time: picture n at first + n * 60 * den / num vblanks */
    if (M.frames_out == 0)
        M.first_vblank = xsg_frame;
    unsigned due = M.first_vblank + (unsigned)((unsigned long long)M.frames_out * 60 * den / num);
    while ((int)(due - xsg_frame) > 0)
        sceGsSyncV(0);
    if (M.frames_out == 0)
        xsg_logf("[mpeg] first picture %dx%d at vblank %u, %d/%d fps\n", w, h, xsg_frame, num, den);
    M.frames_out++;
    return 0;
}

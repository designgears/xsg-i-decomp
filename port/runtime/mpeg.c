/*
 * Host libmpeg (sceMpeg*) for the .pss movies, backed by an external ffmpeg.
 *
 * The game's own player (src/main/xgl_1.c xglMpeg2Open/xglMpeg2Play) is kept
 * as it is.  It feeds PSS packs to sceMpegDemuxPss, which hands the video and
 * audio PES payloads to the game's stream callbacks (videoCallback copies the
 * MPEG-2 elementary stream into its DMA ring, audioCallback copies the ADPCM
 * into its sound ring).  On the PS2, libmpeg's decoder then pulls the ring
 * through the IPU: whenever the IPU input runs dry it calls the game's
 * nodataCallback, which programs D4 (toIPU) MADR/QWC/CHCR with the next
 * slice of the ring.  Here sceMpegGetPicture does the same: it calls the
 * nodata callback, takes the D4 slice the game programmed, and writes it to
 * ffmpeg's stdin; ffmpeg's stdout gives raw RGBA pictures in display order,
 * which are stored in the 16x16 macroblock order the game's GS upload chain
 * (setLoadImageTags) expects: 0x400 bytes per macroblock, macroblock columns
 * outermost.
 *
 * ffmpeg is a separate process (`ffmpeg -f mpegvideo -i pipe:0 -f rawvideo
 * -pix_fmt rgba pipe:1`), found on PATH or through XSG_FFMPEG.  Nothing from
 * FFmpeg is linked or vendored.  XSG_MPEG=0 restores the old behaviour (every
 * movie ends at once).
 *
 * Pacing: the game's loop draws one picture per pass and does not wait for
 * the movie clock (on the PS2 the decode itself and the vblank-locked
 * renderer do that), so GetPicture waits for vblanks until the picture's
 * presentation time, 60/fps vblanks per picture.
 */
#define _GNU_SOURCE
#include <errno.h>
#include "xsg_settings.h"
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "xsg_rt.h"

extern char **environ;
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

    pid_t pid;
    int in_fd, out_fd;
    pthread_t reader;
    int reader_running;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    unsigned char *out;      /* decoded bytes not yet taken */
    size_t out_len, out_cap;
    int out_eof;

    int in_closed, empty_feeds, ended;
    /* elementary stream scanner (decode order) */
    uint32_t sc;             /* last 4 bytes */
    int sc_need, sc_kind;    /* bytes still wanted after a start code */
    unsigned char sc_buf[4];
    int width, height, fps_num, fps_den;
    int pending_anchor;      /* I/P picture type waiting for its display slot, 0 none */
    unsigned char types[256];/* display-order picture types */
    unsigned types_head, types_tail;
    unsigned frames_out;
    unsigned first_vblank;
    unsigned long long bytes_in;
} M = {.pid = -1, .in_fd = -1, .out_fd = -1, .mu = PTHREAD_MUTEX_INITIALIZER, .cv = PTHREAD_COND_INITIALIZER};

static int enabled(void)
{
    const char *e = xsg_cfg("game.movies");
    return !e || atoi(e) != 0;
}

/* ---- the ffmpeg process -------------------------------------------- */
static void *reader_main(void *arg)
{
    (void)arg;
    unsigned char buf[65536];
    for (;;) {
        ssize_t n = read(M.out_fd, buf, sizeof buf);
        if (n < 0 && errno == EINTR)
            continue;
        pthread_mutex_lock(&M.mu);
        if (n <= 0) {
            M.out_eof = 1;
            pthread_cond_broadcast(&M.cv);
            pthread_mutex_unlock(&M.mu);
            return NULL;
        }
        if (M.out_len + n > M.out_cap) {
            M.out_cap = (M.out_len + n) * 2;
            M.out = realloc(M.out, M.out_cap);
        }
        memcpy(M.out + M.out_len, buf, n);
        M.out_len += n;
        pthread_cond_broadcast(&M.cv);
        pthread_mutex_unlock(&M.mu);
    }
}

static void close_input(void)
{
    if (!M.in_closed && M.in_fd >= 0) {
        close(M.in_fd);
        M.in_fd = -1;
    }
    M.in_closed = 1;
}

static void helper_stop(void)
{
    close_input();
    if (M.reader_running) {
        if (M.pid > 0)
            kill(M.pid, SIGTERM);
        pthread_join(M.reader, NULL);
        M.reader_running = 0;
    }
    if (M.out_fd >= 0)
        close(M.out_fd);
    M.out_fd = -1;
    if (M.pid > 0)
        waitpid(M.pid, NULL, 0);
    M.pid = -1;
    free(M.out);
    M.out = NULL;
    M.out_len = M.out_cap = 0;
}

static int helper_start(void)
{
    int in[2], out[2];
    const char *ff = xsg_cfg("game.ffmpeg");
    if (!ff)
        ff = "ffmpeg";
    if (pipe2(in, O_CLOEXEC) || pipe2(out, O_CLOEXEC))
        return -1;
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, in[0], 0);
    posix_spawn_file_actions_adddup2(&fa, out[1], 1);
    char *argv[] = {(char *)ff, "-hide_banner", "-loglevel", "error",
                    "-probesize", "32", "-analyzeduration", "0", "-fflags", "nobuffer",
                    "-threads", "1", "-f", "mpegvideo", "-i", "pipe:0",
                    "-f", "rawvideo", "-pix_fmt", "rgba", "pipe:1", NULL};
    int r = posix_spawnp(&M.pid, ff, &fa, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    close(in[0]);
    close(out[1]);
    if (r != 0) {
        xsg_logf("[mpeg] cannot start %s: %s\n", ff, strerror(r));
        close(in[1]);
        close(out[0]);
        M.pid = -1;
        return -1;
    }
    signal(SIGPIPE, SIG_IGN);
    M.in_fd = in[1];
    M.out_fd = out[0];
    M.out_eof = 0;
    M.in_closed = 0;
    pthread_create(&M.reader, NULL, reader_main, NULL);
    M.reader_running = 1;
    xsg_logf("[mpeg] %s started (pid %d)\n", ff, (int)M.pid);
    return 0;
}

/* ---- elementary stream scanner ------------------------------------- */
static void push_type(int t)
{
    if (M.types_tail - M.types_head < sizeof M.types)
        M.types[M.types_tail++ % sizeof M.types] = (unsigned char)t;
}

static void picture_decoded(int type)
{
    /* B pictures are shown at once; an I/P picture is shown when the next
     * I/P picture arrives (the decoder's one-picture reorder delay). */
    if (type == 3) {
        push_type(3);
    } else {
        if (M.pending_anchor)
            push_type(M.pending_anchor);
        M.pending_anchor = type;
    }
}

static void scan_es(const unsigned char *p, size_t n)
{
    static const int fr[16][2] = {{30, 1}, {24000, 1001}, {24, 1}, {25, 1}, {30000, 1001}, {30, 1},
                                  {50, 1}, {60000, 1001}, {60, 1}};
    for (size_t i = 0; i < n; i++) {
        unsigned char b = p[i];
        if (M.sc_need) {
            M.sc_buf[4 - M.sc_need] = b;
            if (--M.sc_need == 0) {
                const unsigned char *s = M.sc_buf;
                if (M.sc_kind == 0xb3) {
                    M.width = s[0] << 4 | s[1] >> 4;
                    M.height = (s[1] & 15) << 8 | s[2];
                    int c = s[3] & 15;
                    if (c >= 1 && c <= 8) {
                        M.fps_num = fr[c][0];
                        M.fps_den = fr[c][1];
                    }
                } else if (M.sc_kind == 0x00) {
                    picture_decoded((s[1] >> 3) & 7);
                }
            }
        }
        M.sc = M.sc << 8 | b;
        if ((M.sc & 0xffffff00u) == 0x00000100u) {
            unsigned code = M.sc & 0xff;
            if (code == 0xb3 || code == 0x00) {
                M.sc_kind = (int)code;
                M.sc_need = 4;
            } else if (code == 0xb7) {
                M.ended = 1; /* sequence_end_code */
            }
        }
    }
}

/* One nodata round: the game programs D4 with the next slice of its ring. */
static void feed_once(void)
{
    if (M.in_closed)
        return;
    if (M.ended) {
        if (M.pending_anchor)
            push_type(M.pending_anchor);
        M.pending_anchor = 0;
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
    scan_es(src, n);
    M.bytes_in += n;
    while (n) {
        ssize_t w = write(M.in_fd, src, n);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            xsg_logf("[mpeg] ffmpeg input closed: %s\n", strerror(errno));
            close_input();
            return;
        }
        src += w;
        n -= (size_t)w;
    }
}

static size_t frame_bytes(void) { return (size_t)M.width * M.height * 4; }

static int frame_ready_locked(void) { return M.width && M.out_len >= frame_bytes(); }

/* Wait (briefly, unless input is closed) for a decoded picture. */
static int wait_frame(int ms)
{
    pthread_mutex_lock(&M.mu);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += ms * 1000000L;
    while (ts.tv_nsec >= 1000000000L) {
        ts.tv_nsec -= 1000000000L;
        ts.tv_sec++;
    }
    while (!frame_ready_locked() && !M.out_eof)
        if (pthread_cond_timedwait(&M.cv, &M.mu, &ts) == ETIMEDOUT)
            break;
    int r = frame_ready_locked() ? 1 : M.out_eof ? -1 : 0;
    pthread_mutex_unlock(&M.mu);
    return r;
}

/* ---- sceMpeg API ---------------------------------------------------- */
int sceMpegInit(void) { STUB_LOG(""); return 0; }

int sceMpegCreate(sceMpeg *mp, void *work, int size)
{
    STUB_LOG("%p %p %d", (void *)mp, work, size);
    helper_stop();
    memset(M.str_cb, 0, sizeof M.str_cb);
    memset(M.cb, 0, sizeof M.cb);
    M.sc = 0xffffffffu;
    M.sc_need = 0;
    M.width = M.height = 0;
    M.fps_num = 30000;
    M.fps_den = 1001;
    M.pending_anchor = 0;
    M.types_head = M.types_tail = 0;
    M.frames_out = 0;
    M.ended = 0;
    M.empty_feeds = 0;
    M.bytes_in = 0;
    M.in_closed = 1;
    memset(mp, 0, sizeof *mp);
    if (enabled() && helper_start() == 0)
        xsg_mpeg_active = 1;
    return 0;
}

int sceMpegDelete(sceMpeg *mp)
{
    (void)mp;
    if (M.pid > 0)
        xsg_logf("[mpeg] closed after %u pictures (%llu ES bytes)\n", M.frames_out, M.bytes_in);
    helper_stop();
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
    if (M.pid <= 0)
        return 1;
    if (!M.in_closed)
        return 0;
    return wait_frame(5000) < 0;
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
    if (M.pid <= 0) {
        STUB_LOG("-> end of stream");
        return -1;
    }
    for (;;) {
        int r = wait_frame(0);
        if (r > 0)
            break;
        if (r < 0) {
            xsg_logf("[mpeg] GetPicture: no more pictures\n");
            return -1;
        }
        if (M.in_closed)
            r = wait_frame(5000);
        else {
            feed_once();
            r = wait_frame(1);
        }
        if (r < 0 && M.in_closed)
            return -1;
    }
    int w = M.width, h = M.height, mbh = h / 16, n = (w / 16) * mbh;
    if (mbcount < n)
        n = mbcount;
    pthread_mutex_lock(&M.mu);
    const unsigned char *f = M.out;
    unsigned char *dst = (unsigned char *)(uintptr_t)((uintptr_t)rgb & 0x1fffffffu);
    for (int k = 0; k < n; k++) {
        int mx = k / mbh, my = k % mbh;
        for (int y = 0; y < 16; y++) {
            const unsigned char *s = f + ((size_t)(my * 16 + y) * w + mx * 16) * 4;
            unsigned char *d = dst + k * 0x400 + y * 64;
            for (int x = 0; x < 16; x++) {
                d[x * 4 + 0] = s[x * 4 + 0];
                d[x * 4 + 1] = s[x * 4 + 1];
                d[x * 4 + 2] = s[x * 4 + 2];
                d[x * 4 + 3] = 0x80;
            }
        }
    }
    size_t fb = frame_bytes();
    memmove(M.out, M.out + fb, M.out_len - fb);
    M.out_len -= fb;
    pthread_mutex_unlock(&M.mu);

    int type = M.types_head != M.types_tail ? M.types[M.types_head++ % sizeof M.types] : 1;
    mp->width = w;
    mp->height = h;
    mp->frameCount = (int)M.frames_out;
    mp->flags = (unsigned)type;
    mp->pts = mp->dts = 0;

    /* presentation time: picture n at first + n * 60 * den / num vblanks */
    if (M.frames_out == 0)
        M.first_vblank = xsg_frame;
    unsigned due = M.first_vblank +
                   (unsigned)((unsigned long long)M.frames_out * 60 * M.fps_den / M.fps_num);
    while ((int)(due - xsg_frame) > 0)
        sceGsSyncV(0);
    if (M.frames_out == 0)
        xsg_logf("[mpeg] first picture %dx%d at vblank %u, %d/%d fps\n", w, h, xsg_frame, M.fps_num,
                 M.fps_den);
    M.frames_out++;
    return 0;
}

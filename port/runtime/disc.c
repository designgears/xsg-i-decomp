#include <time.h>
/*
 * Disc and file I/O from the user's own disc image.
 *
 * XSG_ISO (default <game dir>/iso/<first *.iso>) is read directly: sceCdRead
 * and the streaming sceCdSt* calls read 2048-byte sectors at LSN*2048, and
 * sceCdSearchFile/sceCdLayerSearchFile walk the ISO9660 directory tree
 * (layer 1 of a dual-layer image has its own volume descriptor at
 * layer0_sectors + 16).  sceOpen/sceRead/sceLseek/sceClose accept
 * "cdrom0:\\PATH\\FILE;1" names and resolve them the same way.  Nothing is
 * cached or copied; the image is opened read-only.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include "xsg_settings.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "xsg_rt.h"
#include "xsg_iso_check.h"
#include "xsg_mouse.h"

static int iso_fd = -1;
static uint32_t layer1_start; /* LSN of layer 1 (0 = single layer) */
static uint32_t root_lsn[2], root_size[2];
static uint32_t read_pos;
static int cd_error;

static int read_sectors(uint32_t lsn, uint32_t n, void *buf)
{
    if (iso_fd < 0)
        return -1;
    ssize_t want = (ssize_t)n * 2048;
    ssize_t got = pread(iso_fd, buf, want, (off_t)lsn * 2048);
    read_pos = lsn + n;
    return got == want ? 0 : -1;
}

static uint32_t le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static int read_pvd(uint32_t base, int layer)
{
    unsigned char s[2048];
    if (read_sectors(base + 16, 1, s) || s[0] != 1 || memcmp(s + 1, "CD001", 5))
        return -1;
    root_lsn[layer] = le32(s + 156 + 2) + base;
    root_size[layer] = le32(s + 156 + 10);
    return (int)le32(s + 80); /* volume space size */
}

/* The disc image in use: [game] iso, else the first *.iso in <game_dir>/iso
 * ("" when there is none).  search_dir gets <game_dir>/iso. */
static char setup_iso[4096];   /* picked on the setup screen (also saved as [game] iso) */

static void resolve_iso(char *out, size_t len, char *search_dir, size_t dlen)
{
    const char *gd = xsg_cfg("game.game_dir");
    if (!gd || !*gd)
        gd = XSG_DEFAULT_GAME_DIR;
    char dir[1024] = "";
    if (*gd)   /* a packaged build has no default game dir: nothing to search, only [game] iso */
        snprintf(dir, sizeof dir, "%s/iso", gd);
    if (search_dir)
        snprintf(search_dir, dlen, "%s", dir);
    if (setup_iso[0]) {
        snprintf(out, len, "%s", setup_iso);
        return;
    }
    const char *iso = xsg_cfg("game.iso");
    if (iso) {
        snprintf(out, len, "%s", iso);
        return;
    }
    out[0] = 0;
    DIR *d = opendir(dir);
    struct dirent *e;
    /* the first by name, so the pick does not depend on the directory's order */
    char best[256] = "";
    while (d && (e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n > 4 && !strcasecmp(e->d_name + n - 4, ".iso") && (!best[0] || strcmp(e->d_name, best) < 0))
            snprintf(best, sizeof best, "%s", e->d_name);
    }
    if (d)
        closedir(d);
    if (best[0])
        snprintf(out, len, "%s/%s", dir, best);
}

/* First-run setup (port/docs/SETUP.md), before anything reads the disc: when the image
 * is missing or not this game, xsg-gpu shows its setup screen (gpu_client.cpp
 * xsg_gpu_setup) until the user picks a valid one (it saves it as [game] iso) or quits.
 * Headless runs and the software GS have no window: they log why and go on as before. */
extern int xsg_gs_start_gpu(void);
extern int xsg_gpu_setup(unsigned reason, const char *tried, const char *search_dir, char *out, unsigned len);

void xsg_disc_setup(void)
{
    char path[4096], dir[1024], msg[4400];
    XsgIsoInfo info;
    resolve_iso(path, sizeof path, dir, sizeof dir);
    int rc = xsg_iso_check(path, &info);
    if (rc == XSG_ISO_OK)
        return;
    xsg_iso_message(rc, &info, path, msg, sizeof msg);
    xsg_logf("[setup] %s%s%s\n", msg, rc == XSG_ISO_NONE ? " in " : "", rc == XSG_ISO_NONE ? dir : "");
    if (xsg_cfg_on("debug.headless") || !xsg_gs_start_gpu()) {
        xsg_logf("[setup] no window (headless or software GS): set [game] iso in %s\n", xsg_cfg_path());
        return;
    }
    for (;;) {
        char picked[4096];
        int r = xsg_gpu_setup((unsigned)rc, path, dir, picked, sizeof picked);
        if (r <= 0) {
            xsg_logf("[setup] %s\n", r == 0 ? "quit on the setup screen" : "xsg-gpu is not running");
            _exit(0);
        }
        /* xsg-gpu checked it too; check again here, it is what the game will read */
        rc = xsg_iso_check(picked, &info);
        xsg_iso_message(rc, &info, picked, msg, sizeof msg);
        xsg_logf("[setup] picked %s: %s\n", picked, msg);
        if (rc == XSG_ISO_OK) {
            snprintf(setup_iso, sizeof setup_iso, "%s", picked);
            xsg_cfg_poll();   /* xsg-gpu saved [game] iso */
            return;
        }
        snprintf(path, sizeof path, "%s", picked);
    }
}

void xsg_disc_init(void)
{
    char buf[4096];
    resolve_iso(buf, sizeof buf, NULL, 0);
    const char *iso = buf;
    iso_fd = open(iso, O_RDONLY);
    if (iso_fd < 0) {
        xsg_logf("[disc] cannot open disc image '%s'; set [game] iso in %s\n", iso, xsg_cfg_path());
        return;
    }
    int vol = read_pvd(0, 0);
    struct stat st;
    fstat(iso_fd, &st);
    /* PTP dual-layer dumps: layer 1 starts 16 sectors before its own PVD,
     * which immediately follows layer 0's last sector. */
    if (vol > 16 && (off_t)vol * 2048 < st.st_size && read_pvd(vol - 16, 1) > 0)
        layer1_start = vol - 16;
    xsg_logf("[disc] %s: layer0 %d sectors, layer1 at LSN %u\n", iso, vol, layer1_start);
}

/* ISO9660 lookup of "\\DIR\\NAME.EXT;1" -> lsn, size */
static int iso_find(const char *path, int layer, uint32_t *lsn, uint32_t *size)
{
    uint32_t dlsn = root_lsn[layer], dsize = root_size[layer];
    uint32_t base = layer ? layer1_start : 0;
    char comp[256];
    const char *p = path;
    if (iso_fd < 0)
        return -1;
    while (*p == '\\' || *p == '/')
        p++;
    while (*p) {
        size_t n = 0;
        while (*p && *p != '\\' && *p != '/' && n < sizeof comp - 1)
            comp[n++] = toupper((unsigned char)*p++);
        comp[n] = 0;
        while (*p == '\\' || *p == '/')
            p++;
        int last = !*p;
        char *semi = strchr(comp, ';');
        if (semi)
            *semi = 0;
        unsigned char *dir = malloc(dsize + 2048);
        if (read_sectors(dlsn, (dsize + 2047) / 2048, dir)) {
            free(dir);
            return -1;
        }
        int found = 0;
        for (uint32_t off = 0; off < dsize;) {
            unsigned char *r = dir + off;
            if (r[0] == 0) {
                off = (off / 2048 + 1) * 2048;
                continue;
            }
            char name[256];
            int nl = r[32];
            memcpy(name, r + 33, nl);
            name[nl] = 0;
            char *sc = strchr(name, ';');
            if (sc)
                *sc = 0;
            if (!strcasecmp(name, comp)) {
                dlsn = le32(r + 2) + base;
                dsize = le32(r + 10);
                found = 1;
                break;
            }
            off += r[0];
        }
        free(dir);
        if (!found)
            return -1;
        if (last) {
            *lsn = dlsn;
            *size = dsize;
            return 0;
        }
    }
    return -1;
}

/* A whole file from the disc ("SLUS_204.69"), malloc'd; 0 on success. */
int xsg_disc_read_file(const char *name, void **buf, uint32_t *size)
{
    uint32_t lsn, n;
    if (iso_find(name, 0, &lsn, &n) && (!layer1_start || iso_find(name, 1, &lsn, &n)))
        return -1;
    void *b = malloc(n + 2048);
    if (!b || read_sectors(lsn, (n + 2047) / 2048, b)) {
        free(b);
        return -1;
    }
    *buf = b;
    *size = n;
    return 0;
}

typedef struct {
    uint32_t lsn, size;
    char name[16];
    unsigned char date[8];
} sceCdlFILE;

int sceCdInit(int mode) { STUB_LOG("mode=%d", mode); return 1; }
int sceCdMmode(int m) { STUB_LOG("%d", m); return 1; }
int sceCdDiskReady(int m) { (void)m; return 2; }
int sceCdGetDiskType(void) { return 0x14; /* SCECdPS2DVD */ }
int sceCdStatus(void) { return 0; }
int sceCdGetError(void) { return cd_error; }
int sceCdSync(int m) { (void)m; return 0; }
int sceCdBreak(void) { return 1; }
int sceCdPause(void) { return 1; }
int sceCdStandby(void) { return 1; }
int sceCdStop(void) { return 1; }
uint32_t sceCdGetReadPos(void) { return read_pos; }
int sceCdPowerOff(uint32_t *r) { if (r) *r = 0; return 1; }
void *sceCdPOffCallback(void (*f)(void *), void *a) { (void)f; (void)a; return NULL; }
/* The real-time clock as BCD (sec, min, hour, -, day, month, year): the
 * play-time counter (PartyTimeUpDate) is the difference of two readings. */
static unsigned char Bcd(int v) { return (unsigned char)((v / 10) << 4 | (v % 10)); }
int sceCdReadClock(unsigned char *c)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    memset(c, 0, 8);
    c[1] = Bcd(tm.tm_sec);
    c[2] = Bcd(tm.tm_min);
    c[3] = Bcd(tm.tm_hour);
    c[5] = Bcd(tm.tm_mday);
    c[6] = Bcd(tm.tm_mon + 1);
    c[7] = Bcd(tm.tm_year % 100);
    return 1;
}

/* The game declares this with one argument (xgl_cd.c); the SDK's second
 * argument (layer-1 start) is never passed, so it is not written. */
int sceCdReadDvdDualInfo(int *on_dual)
{
    *on_dual = layer1_start != 0;
    STUB_LOG("-> dual=%d layer1=%u", *on_dual, layer1_start);
    return 1;
}

int sceCdLayerSearchFile(sceCdlFILE *fp, const char *name, int layer)
{
    uint32_t lsn, size;
    int r = iso_find(name, layer ? 1 : 0, &lsn, &size);
    if (r && layer == 0 && layer1_start)
        r = iso_find(name, 1, &lsn, &size);
    xsg_logf("[disc] SearchFile %s layer %d -> %s lsn %u size %u\n", name, layer,
             r ? "NOT FOUND" : "ok", r ? 0 : lsn, r ? 0 : size);
    if (r)
        return 0;
    fp->lsn = lsn;
    fp->size = size;
    return 1;
}
int sceCdSearchFile(sceCdlFILE *fp, const char *name) { return sceCdLayerSearchFile(fp, name, 0); }

int sceCdRead(uint32_t lsn, uint32_t sectors, void *buf, void *mode)
{
    (void)mode;
    STUB_LOG("lsn=%u sectors=%u buf=%p", lsn, sectors, buf);
    cd_error = read_sectors(lsn, sectors, buf) ? 0x32 : 0;
    return 1;
}

/* streaming */
static uint32_t st_lsn;
/* The stream ring holds bufmax sectors; sceCdStStat reports how many are
 * buffered, which is never more than that (the game sizes its reads by it). */
static uint32_t st_bufmax = 16;
/* Uncensored 3039C ([game] uncensored, uncensored_subs, uncensored_dir; MENU.md, PACKAGING.md):
 * xglCdStreamOpen (src/main/xgl_cd.c, PORT_HOST) asks xsg_cd_stream_override about each
 * stream it opens from the disc.  Only movie\mpeg2\3039C.pss can be swapped: with the
 * toggle on and 3039C_un.pss found (name matched case-insensitively), the next
 * sceCdStStart streams that file instead: sector n of the stream is byte n*2048 of the
 * file (sceCdStSeek stays relative), zeros past its end, and the game is told the file's
 * length so it stops there.  The file must be a PS2 .pss like the disc's (MPEG-2 video,
 * SShd ADPCM audio).  Every other stream, and 3039C without the file, reads the disc.
 *
 * Where it is looked for: [game] uncensored_dir when set; else the "uncensored" folder
 * at the root of the install (Windows zip: next to xsg.exe; AppImage: its root, two up
 * from the loader in usr/bin that /proc/self/exe names); else <game_dir>/uncensored. */
static int st_ovr_fd = -1, st_ovr_pending = -1;
static uint32_t st_ovr_base;

static void st_ovr_close(int *fd)
{
    if (*fd >= 0)
        close(*fd);
    *fd = -1;
}

/* <dir>/<want> (case-insensitive) into out; 1 when found */
static int find_in_dir(const char *dir, const char *want, char *out, size_t len)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    int found = 0;
    while (d && !found && (e = readdir(d)))
        if (!strcasecmp(e->d_name, want)) {
            snprintf(out, len, "%s/%s", dir, e->d_name);
            found = 1;
        }
    if (d)
        closedir(d);
    return found;
}

#ifdef _WIN32
extern const char *xsg_w32_exe_path(void);
#endif

/* the directory of the running executable ("" when unknown) */
static void exe_dir(char *out, size_t len)
{
    out[0] = 0;
#ifdef _WIN32
    const char *exe = xsg_w32_exe_path();
    if (exe)
        snprintf(out, len, "%s", exe);
#else
    ssize_t n = readlink("/proc/self/exe", out, len - 1);
    out[n > 0 ? n : 0] = 0;
#endif
    char *slash = NULL;
    for (char *p = out; *p; p++)
        if (*p == '/' || *p == '\\')
            slash = p;
    if (slash)
        *slash = 0;
    else
        out[0] = 0;
}

/* <uncensored folder>/<want> into file (case-insensitive name); 1 when found.  tried
 * gets the folders looked in, for the log. */
static int uncensored_file(const char *want, char *file, size_t flen, char *tried, size_t tlen)
{
    char dir[1100];
    file[0] = tried[0] = 0;
    const char *ud = xsg_cfg("game.uncensored_dir");
    if (ud && *ud) {
        snprintf(tried, tlen, "%s", ud);
        return find_in_dir(ud, want, file, flen);
    }
    char ed[1024];
    exe_dir(ed, sizeof ed);
    const char *gd = xsg_cfg("game.game_dir");
    char cand[3][1100];
    int nc = 0;
    if (ed[0]) {
        snprintf(cand[nc++], sizeof cand[0], "%s/uncensored", ed);         /* zip root */
        snprintf(cand[nc++], sizeof cand[0], "%s/../../uncensored", ed);   /* AppImage root */
    }
    if (!(gd && *gd))
        gd = XSG_DEFAULT_GAME_DIR;   /* "" in a packaged build (XSG_PACKAGED) */
    if (*gd)
        snprintf(cand[nc++], sizeof cand[0], "%s/uncensored", gd);
    for (int i = 0; i < nc; i++) {
        snprintf(dir, sizeof dir, "%s", cand[i]);
        size_t t = strlen(tried);
        snprintf(tried + t, tlen - t, "%s%s", t ? ", " : "", dir);
        if (find_in_dir(dir, want, file, flen))
            return 1;
    }
    return 0;
}

/* the movie name's last path component, when it is 3039C.pss (any case); else NULL */
static const char *is_3039c(const char *path)
{
    const char *base = path;
    for (const char *p = path; *p; p++)
        if (*p == '\\' || *p == '/')
            base = p + 1;
    return strcasecmp(base, "3039C.pss") ? NULL : base;
}

int xsg_cd_stream_override(const char *path, int *bytes)
{
    st_ovr_close(&st_ovr_pending);
    const char *base;
    if (!path || !xsg_cfg_on("game.uncensored") || !(base = is_3039c(path)))
        return 0;
    static const char want[] = "3039C_un.pss";
    char file[2048], tried[3400];
    if (!uncensored_file(want, file, sizeof file, tried, sizeof tried)) {
        xsg_logf("[disc] uncensored on, but no %s in %s: playing the disc's %s\n", want, tried, base);
        return 0;
    }
    int fd = open(file, O_RDONLY);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) || st.st_size <= 0 || st.st_size > 0x7fffffff) {
        xsg_logf("[disc] %s unreadable, playing the disc's 3039C\n", file);
        if (fd >= 0)
            close(fd);
        return 0;
    }
    st_ovr_pending = fd;
    *bytes = (int)st.st_size;
    xsg_logf("[disc] movie %s -> uncensored %s (%lld bytes)\n", path, file, (long long)st.st_size);
    return 1;
}

/* English subtitles for the uncensored 3039C ([game] uncensored_subs; MENU.md): its picture
 * is the Japanese release's, with Japanese subtitles burned in, and its sound the US
 * release's English.  GameMpeg2Play (src/main/game_movie.c, PORT_HOST) opens the captions
 * for each movie it plays and reports its picture count every frame; for 3039C, with the
 * swap active and the option on, <uncensored folder>/3039C_un.cap (UTF-8, the game's .cap
 * layout: "m:s:f<tab>text" lines, time in pictures at 30 a second, '\' a line break, ';'
 * a comment, a line without a time ends the list) supplies the line, which xsg-gpu draws
 * at the top of the screen in the rebuilt UI's font (mouse.cpp xsg_caption_set).  The
 * game's own caption printer is not used: no US movie has captions, and its font draws
 * English punctuation and spaces as Japanese glyphs. */
#define MAX_CAPTIONS 128
static struct { int frame; char text[256]; } captions[MAX_CAPTIONS];
static int n_captions;

void xsg_movie_caption_close(void)
{
    if (n_captions)
        xsg_caption_set("");
    n_captions = 0;
}

int xsg_movie_caption_open(const char *movie)
{
    xsg_movie_caption_close();
    if (!movie || !is_3039c(movie) || !xsg_cfg_on("game.uncensored") || !xsg_cfg_on("game.uncensored_subs"))
        return 0;
    char file[2048], tried[3400];
    if (!uncensored_file("3039C_un.pss", file, sizeof file, tried, sizeof tried))
        return 0;                                /* the disc's 3039C plays: it has English */
    if (!uncensored_file("3039C_un.cap", file, sizeof file, tried, sizeof tried)) {
        xsg_logf("[disc] uncensored subtitles on, but no 3039C_un.cap in %s\n", tried);
        return 0;
    }
    FILE *f = fopen(file, "rb");
    if (!f)
        return 0;
    char line[512];
    while (n_captions < MAX_CAPTIONS && fgets(line, sizeof line, f)) {
        if (line[0] == ';' || line[0] == '\r' || line[0] == '\n')
            continue;
        if (!isdigit((unsigned char)line[0]))
            break;                               /* a line without a time: the end */
        int v[3] = {0, 0, 0}, nv = 0;
        char *p = line;
        while (nv < 3 && isdigit((unsigned char)*p)) {
            v[nv++] = (int)strtol(p, &p, 10);
            if (*p == ':')
                p++;
        }
        int frame = nv == 3 ? (v[0] * 60 + v[1]) * 30 + v[2] : nv == 2 ? v[0] * 30 + v[1] : v[0];
        while (*p == ' ' || *p == '\t')
            p++;
        char *o = captions[n_captions].text, *end = o + sizeof captions[0].text - 1;
        for (; *p && *p != '\r' && *p != '\n' && o < end; p++)
            *o++ = *p == '\\' ? '\n' : *p;
        *o = 0;
        captions[n_captions++].frame = frame;
    }
    fclose(f);
    xsg_logf("[disc] movie %s: %d English subtitle entries from %s\n", movie, n_captions, file);
    return n_captions > 0;
}

void xsg_movie_caption_frame(int frame)
{
    if (!n_captions)
        return;
    const char *t = "";
    for (int i = 0; i < n_captions && captions[i].frame <= frame; i++)
        t = captions[i].text;
    xsg_caption_set(t);
}

int sceCdStInit(uint32_t bufmax, uint32_t bankmax, void *buf) { STUB_LOG("%u %u %p", bufmax, bankmax, buf); st_bufmax = bufmax; return 1; }
int sceCdStStart(uint32_t lsn, void *mode)
{
    (void)mode;
    xsg_logf("[disc] StStart lsn=%u%s\n", lsn, st_ovr_pending >= 0 ? " (uncensored 3039C)" : "");
    st_ovr_close(&st_ovr_fd);
    st_ovr_fd = st_ovr_pending;
    st_ovr_pending = -1;
    st_ovr_base = lsn;
    st_lsn = lsn;
    return 1;
}
int sceCdStSeek(uint32_t lsn) { STUB_LOG("lsn=%u", lsn); st_lsn = lsn; return 1; }
int sceCdStStop(void) { return 1; }
int sceCdStPause(void) { return 1; }
int sceCdStResume(void) { return 1; }
int sceCdStStat(void) { return (int)st_bufmax; }
int sceCdStRead(uint32_t sectors, void *buf, uint32_t mode, uint32_t *err)
{
    (void)mode;
    STUB_LOG("lsn=%u sectors=%u", st_lsn, sectors);
    if (st_ovr_fd >= 0) {
        size_t want = (size_t)sectors * 2048;
        ssize_t got = pread(st_ovr_fd, buf, want, (off_t)(st_lsn - st_ovr_base) * 2048);
        if (got < 0)
            got = 0;
        memset((char *)buf + got, 0, want - (size_t)got);
        if (err)
            *err = 0;
        st_lsn += sectors;
        return (int)sectors;
    }
    int r = read_sectors(st_lsn, sectors, buf);
    if (err)
        *err = r ? 0x32 : 0;
    st_lsn += sectors;
    return r ? 0 : (int)sectors;
}

/* ---- sce file I/O (cdrom0:, host0:) --------------------------------- */
#define MAX_FD 32
static struct { int used; uint32_t lsn, size, pos; int host_fd; } fds[MAX_FD];

int sceOpen(const char *name, int flags, ...)
{
    const char *p = strchr(name, ':');
    p = p ? p + 1 : name;
    for (int i = 3; i < MAX_FD; i++) {
        if (fds[i].used)
            continue;
        uint32_t lsn, size;
        if (iso_find(p, 0, &lsn, &size) && (!layer1_start || iso_find(p, 1, &lsn, &size))) {
            xsg_logf("[io] sceOpen %s flags=0x%x -> not found\n", name, flags);
            return -1;
        }
        fds[i].used = 1;
        fds[i].lsn = lsn;
        fds[i].size = size;
        fds[i].pos = 0;
        xsg_logf("[io] sceOpen %s -> fd %d (lsn %u size %u)\n", name, i, lsn, size);
        return i;
    }
    return -1;
}
int sceClose(int fd) { if (fd > 0 && fd < MAX_FD) fds[fd].used = 0; return 0; }
int sceRead(int fd, void *buf, int n)
{
    if (fd <= 0 || fd >= MAX_FD || !fds[fd].used)
        return -1;
    if (fds[fd].pos + n > fds[fd].size)
        n = fds[fd].size - fds[fd].pos;
    ssize_t r = pread(iso_fd, buf, n, (off_t)fds[fd].lsn * 2048 + fds[fd].pos);
    if (r > 0)
        fds[fd].pos += r;
    STUB_LOG("fd=%d n=%d -> %d", fd, n, (int)r);
    return (int)r;
}
int sceLseek(int fd, int off, int whence)
{
    if (fd <= 0 || fd >= MAX_FD || !fds[fd].used)
        return -1;
    int base = whence == 0 ? 0 : whence == 1 ? (int)fds[fd].pos : (int)fds[fd].size;
    fds[fd].pos = base + off;
    return fds[fd].pos;
}
int sceWrite(int fd, const void *buf, int n) { STUB_LOG("fd=%d n=%d", fd, n); (void)buf; return n; }
int sceIoctl(int fd, int req, void *arg) { STUB_LOG("%d %d", fd, req); (void)arg; return 0; }
int sceIoctl2(int fd, int req, void *a, int al, void *b, int bl) { STUB_LOG("%d %d", fd, req); (void)a; (void)al; (void)b; (void)bl; return 0; }
int sceMkdir(const char *n, int m) { STUB_LOG("%s %d", n, m); return -1; }
int sceRemove(const char *n) { STUB_LOG("%s", n); return -1; }
int sceDopen(const char *n) { STUB_LOG("%s", n); return -1; }
int sceDread(int fd, void *b) { (void)fd; (void)b; return 0; }
int sceDclose(int fd) { (void)fd; return 0; }
int sceGetstat(const char *n, void *s) { STUB_LOG("%s", n); (void)s; return -1; }
int sceChstat(const char *n, void *s, unsigned m) { STUB_LOG("%s", n); (void)s; (void)m; return -1; }
int sceFormat(const char *a, const char *b, void *c, int d) { STUB_LOG("%s", a); (void)b; (void)c; (void)d; return -1; }
int sceMount(const char *a, const char *b, int f, void *c, int d) { STUB_LOG("%s %s", a, b); (void)f; (void)c; (void)d; return -1; }
int sceUmount(const char *a) { STUB_LOG("%s", a); return 0; }
int sceDevctl(const char *n, int c, void *a, unsigned al, void *b, unsigned bl) { STUB_LOG("%s %d", n, c); (void)a; (void)al; (void)b; (void)bl; return -1; }
int sceFsReset(void) { STUB_LOG(""); return 0; }

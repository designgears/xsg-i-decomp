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
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "xsg_rt.h"

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

void xsg_disc_init(void)
{
    const char *iso = getenv("XSG_ISO");
    char buf[1024];
    if (!iso) {
        const char *gd = getenv("XSG_GAME_DIR");
        snprintf(buf, sizeof buf, "%s/iso", gd ? gd : XSG_DEFAULT_GAME_DIR);
        DIR *d = opendir(buf);
        struct dirent *e;
        size_t l = strlen(buf);
        while (d && (e = readdir(d))) {
            size_t n = strlen(e->d_name);
            if (n > 4 && !strcasecmp(e->d_name + n - 4, ".iso")) {
                snprintf(buf + l, sizeof buf - l, "/%s", e->d_name);
                break;
            }
        }
        if (d)
            closedir(d);
        iso = buf;
    }
    iso_fd = open(iso, O_RDONLY);
    if (iso_fd < 0) {
        xsg_logf("[disc] cannot open disc image %s; set XSG_ISO\n", iso);
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
int sceCdReadClock(unsigned char *c) { STUB_LOG(""); memset(c, 0, 8); c[1] = 0; c[3] = 0x12; c[5] = 1; c[6] = 1; c[7] = 3; return 1; }

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
int sceCdStInit(uint32_t bufmax, uint32_t bankmax, void *buf) { STUB_LOG("%u %u %p", bufmax, bankmax, buf); return 1; }
int sceCdStStart(uint32_t lsn, void *mode) { (void)mode; STUB_LOG("lsn=%u", lsn); st_lsn = lsn; return 1; }
int sceCdStSeek(uint32_t lsn) { STUB_LOG("lsn=%u", lsn); st_lsn = lsn; return 1; }
int sceCdStStop(void) { return 1; }
int sceCdStPause(void) { return 1; }
int sceCdStResume(void) { return 1; }
int sceCdStStat(void) { return 0x7fff; }
int sceCdStRead(uint32_t sectors, void *buf, uint32_t mode, uint32_t *err)
{
    (void)mode;
    STUB_LOG("lsn=%u sectors=%u", st_lsn, sectors);
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

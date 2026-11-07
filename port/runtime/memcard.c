/*
 * Memory card (libmc) stand-in: slot 1 (port 0) holds a formatted 8 MB PS2
 * card backed by a host directory, slot 2 (port 1) is empty.
 *
 *   XSG_MC_DIR   host directory for port 0 (default: $HOME/.local/share/xsg/mc0)
 *   XSG_MC=0     report no card in either slot (the boot then waits for X)
 *
 * libmc is asynchronous: every call queues one command and sceMcSync fetches
 * its result.  Here each call completes at once and stores the result that
 * the next sceMcSync returns (cmd = the function number, result as libmc
 * gives it).  Paths are card-absolute ("/BASLUS-20469/..."); "*" globs work
 * in sceMcGetDir.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include "xsg_settings.h"
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "xsg_rt.h"

enum { MC_FUNC_GETINFO = 1, MC_FUNC_OPEN, MC_FUNC_CLOSE, MC_FUNC_SEEK, MC_FUNC_READ,
       MC_FUNC_WRITE, MC_FUNC_FLUSH = 0xA, MC_FUNC_MKDIR, MC_FUNC_CHDIR, MC_FUNC_GETDIR,
       MC_FUNC_SETINFO, MC_FUNC_DELETE, MC_FUNC_FORMAT, MC_FUNC_UNFORMAT, MC_FUNC_GETENTSPACE };

static int pending_cmd = -1, pending_result;
static int first_info = 1;
static char root[1024];
static int enabled = -1;

static void done(int cmd, int result) { pending_cmd = cmd; pending_result = result; }

static int card_present(int port, int slot)
{
    if (enabled < 0) {
        const char *e = xsg_cfg("game.memcard");
        enabled = !(e && e[0] == '0');
        const char *d = xsg_cfg("game.memcard_dir");
        if (d && *d)
            snprintf(root, sizeof root, "%s", d);
        else {
            const char *h = getenv("HOME");
            snprintf(root, sizeof root, "%s/.local", h ? h : "/tmp");
            mkdir(root, 0755);
            strcat(root, "/share"); mkdir(root, 0755);
            strcat(root, "/xsg"); mkdir(root, 0755);
            strcat(root, "/mc0");
        }
        if (enabled)
            mkdir(root, 0755);
        xsg_logf("[mc] port 0: %s\n", enabled ? root : "no card");
    }
    return enabled && port == 0 && slot == 0;
}

static void host_path(char *out, size_t n, const char *name)
{
    while (*name == '/')
        name++;
    snprintf(out, n, "%s/%s", root, name);
}

int sceMcInit(void) { STUB_LOG(""); return 0; }

int sceMcSync(int mode, int *cmd, int *result)
{
    (void)mode;
    if (pending_cmd < 0)
        return -1; /* nothing in flight */
    if (cmd) *cmd = pending_cmd;
    if (result) *result = pending_result;
    pending_cmd = -1;
    return 1;
}

int sceMcGetInfo(int port, int slot, int *type, int *free_, int *format)
{
    STUB_LOG("port=%d slot=%d", port, slot);
    if (!card_present(port, slot)) {
        if (type) *type = 0;
        if (free_) *free_ = 0;
        if (format) *format = 0;
        done(MC_FUNC_GETINFO, -10); /* no card */
        return 0;
    }
    if (type) *type = 2;      /* PS2 card */
    if (free_) *free_ = 8000; /* free clusters (1 KiB) */
    if (format) *format = 1;
    /* the first query after power-on reports "new card" (-1), then 0 */
    done(MC_FUNC_GETINFO, first_info ? -1 : 0);
    first_info = 0;
    return 0;
}

#define MAX_FD 8
static int fds[MAX_FD] = {-1, -1, -1, -1, -1, -1, -1, -1};

int sceMcOpen(int port, int slot, const char *name, int mode)
{
    char p[1200];
    int fd = -1, i;
    STUB_LOG("port=%d %s mode=%#x", port, name, mode);
    if (!card_present(port, slot)) { done(MC_FUNC_OPEN, -10); return 0; }
    host_path(p, sizeof p, name);
    int fl = (mode & 3) == 1 ? O_RDONLY : (mode & 3) == 2 ? O_WRONLY : O_RDWR;
    if (mode & 0x200) fl |= O_CREAT; /* SCE_CREAT */
    for (i = 0; i < MAX_FD && fds[i] >= 0; i++) {}
    if (i < MAX_FD)
        fd = open(p, fl, 0644);
    if (fd < 0) { done(MC_FUNC_OPEN, -4); return 0; } /* not found */
    fds[i] = fd;
    done(MC_FUNC_OPEN, i);
    return 0;
}
static int hfd(int fd) { return fd >= 0 && fd < MAX_FD ? fds[fd] : -1; }
int sceMcClose(int fd)
{
    if (hfd(fd) >= 0) { close(fds[fd]); fds[fd] = -1; done(MC_FUNC_CLOSE, 0); }
    else done(MC_FUNC_CLOSE, -4);
    return 0;
}
int sceMcRead(int fd, void *b, int s)
{
    int r = hfd(fd) >= 0 ? (int)read(fds[fd], b, s) : -4;
    STUB_LOG("fd=%d %d -> %d", fd, s, r);
    done(MC_FUNC_READ, r < 0 ? -4 : r);
    return 0;
}
int sceMcWrite(int fd, const void *b, int s)
{
    int r = hfd(fd) >= 0 ? (int)write(fds[fd], b, s) : -4;
    STUB_LOG("fd=%d %d -> %d", fd, s, r);
    done(MC_FUNC_WRITE, r < 0 ? -4 : r);
    return 0;
}
int sceMcSeek(int fd, int off, int whence)
{
    int r = hfd(fd) >= 0 ? (int)lseek(fds[fd], off, whence) : -4;
    done(MC_FUNC_SEEK, r);
    return 0;
}
int sceMcFlush(int fd) { done(MC_FUNC_FLUSH, 0); return 0; }
int sceMcMkdir(int port, int slot, const char *name)
{
    char p[1200];
    STUB_LOG("%s", name);
    if (!card_present(port, slot)) { done(MC_FUNC_MKDIR, -10); return 0; }
    host_path(p, sizeof p, name);
    done(MC_FUNC_MKDIR, mkdir(p, 0755) == 0 ? 0 : (errno == EEXIST ? -4 : -5));
    return 0;
}
int sceMcDelete(int port, int slot, const char *name)
{
    char p[1200];
    STUB_LOG("%s", name);
    if (!card_present(port, slot)) { done(MC_FUNC_DELETE, -10); return 0; }
    host_path(p, sizeof p, name);
    done(MC_FUNC_DELETE, (unlink(p) == 0 || rmdir(p) == 0) ? 0 : -4);
    return 0;
}

struct mc_tm { unsigned char resv2, sec, min, hour, day, month; unsigned short year; };
struct mc_dirent { /* sceMcTblGetDir, 64 bytes */
    struct mc_tm create, modify;
    unsigned int size;
    unsigned short attr, resv1;
    unsigned int resv2, pda;
    char name[32];
};

static void to_mctm(struct mc_tm *t, time_t s)
{
    struct tm tm;
    gmtime_r(&s, &tm);
    t->resv2 = 0; t->sec = tm.tm_sec; t->min = tm.tm_min; t->hour = tm.tm_hour;
    t->day = tm.tm_mday; t->month = tm.tm_mon + 1; t->year = tm.tm_year + 1900;
}

int sceMcGetDir(int port, int slot, const char *name, unsigned mode, int maxent, void *table)
{
    char p[1200], dir[1200];
    STUB_LOG("%s max=%d", name, maxent);
    if (!card_present(port, slot)) { done(MC_FUNC_GETDIR, -10); return 0; }
    host_path(p, sizeof p, name);
    snprintf(dir, sizeof dir, "%s", p);
    char *slash = strrchr(dir, '/');
    const char *pat = slash ? slash + 1 : "*";
    if (slash) *slash = 0;
    DIR *d = opendir(dir);
    int n = 0;
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && n < maxent) {
            if (fnmatch(pat, e->d_name, 0) != 0)
                continue;
            struct mc_dirent *o = (struct mc_dirent *)table + n;
            struct stat st;
            char fp[2400];
            snprintf(fp, sizeof fp, "%s/%s", dir, e->d_name);
            if (stat(fp, &st) != 0)
                continue;
            memset(o, 0, sizeof *o);
            to_mctm(&o->create, st.st_ctime);
            to_mctm(&o->modify, st.st_mtime);
            o->size = (unsigned)st.st_size;
            o->attr = S_ISDIR(st.st_mode) ? 0x8427 : 0x8497;
            snprintf(o->name, sizeof o->name, "%s", e->d_name);
            n++;
        }
        closedir(d);
    }
    done(MC_FUNC_GETDIR, n);
    return 0;
}
int sceMcFormat(int port, int slot) { done(MC_FUNC_FORMAT, card_present(port, slot) ? 0 : -10); return 0; }
int sceMcUnformat(int port, int slot) { done(MC_FUNC_UNFORMAT, card_present(port, slot) ? 0 : -10); return 0; }
int sceMcChdir(int port, int slot, const char *n, char *cur) { done(MC_FUNC_CHDIR, 0); return 0; }
int sceMcSetFileInfo(int port, int slot, const char *n, const char *info, unsigned v) { done(MC_FUNC_SETINFO, 0); return 0; }
int sceMcGetEntSpace(int port, int slot, const char *n) { done(MC_FUNC_GETENTSPACE, 8000); return 0; }

/*
 * Windows port (port/docs/WINDOWS.md): POSIX stand-ins for the game-side host
 * runtime (port/runtime: the .c and .cpp files), MinGW-w64 i686.  Force-included (-include)
 * into the runtime objects on WIN32 only; the Linux build never sees it.  The
 * shim headers next to it (win32/include: dlfcn.h, execinfo.h, elf.h,
 * fnmatch.h, spawn.h, sys/wait.h) are implemented in win32/platform_win32.c.
 */
#ifndef XSG_RT_WIN_COMPAT_H
#define XSG_RT_WIN_COMPAT_H
#ifdef _WIN32

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES 1   /* M_PI */
#endif
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <malloc.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/* mkdir(path, mode): MinGW's takes no mode (direct.h/io.h are included above,
   so the macro cannot mangle their declaration). */
#define mkdir(p, m) _mkdir(p)

/* Files are binary everywhere (win32/platform_win32.c also sets _fmode). */
#ifndef O_CLOEXEC
#define O_CLOEXEC (_O_NOINHERIT | _O_BINARY)
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif
#ifndef F_GETFL
#define F_GETFL 3
#define F_SETFL 4
#endif
static inline int fcntl(int fd, int cmd, ...) { (void)fd; (void)cmd; return 0; }

/* pread at a 64-bit offset (the disc image is 8 GB).  Not atomic with respect
   to the file position, which only the caller's thread uses. */
static inline ssize_t xsg_w32_pread(int fd, void *buf, size_t n, long long off)
{
    if (_lseeki64(fd, off, SEEK_SET) < 0)
        return -1;
    return _read(fd, buf, (unsigned)n);
}
#define pread(fd, buf, n, off) xsg_w32_pread((fd), (buf), (n), (long long)(off))

/* pipe2(fds, O_CLOEXEC): binary, not inherited (spawn duplicates what a child needs). */
static inline int pipe2(int fds[2], int flags)
{
    (void)flags;
    return _pipe(fds, 1 << 20, _O_BINARY | _O_NOINHERIT);
}

/* aligned_alloc: the IOP request buffers are never freed. */
#define aligned_alloc(a, n) _aligned_malloc((n), (a))

#ifndef SIGPIPE
#define SIGPIPE 13
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif

#ifndef gmtime_r
static inline struct tm *xsg_w32_gmtime_r(const time_t *t, struct tm *r) { return gmtime_s(r, t) ? NULL : r; }
#define gmtime_r xsg_w32_gmtime_r
#endif
#ifndef localtime_r
static inline struct tm *xsg_w32_localtime_r(const time_t *t, struct tm *r) { return localtime_s(r, t) ? NULL : r; }
#define localtime_r xsg_w32_localtime_r
#endif

/* Sleeps: winpthreads' clock_nanosleep/nanosleep are Sleep()-based (1-16 ms
   granularity); vblank pacing needs better (win32/platform_win32.c). */
int xsg_w32_clock_nanosleep(clockid_t clk, int flags, const struct timespec *req, struct timespec *rem);
int xsg_w32_nanosleep(const struct timespec *req, struct timespec *rem);
#define clock_nanosleep xsg_w32_clock_nanosleep
#define nanosleep xsg_w32_nanosleep

/* %VAR% (APPDATA, LOCALAPPDATA, USERPROFILE) into out with '/' separators; "" if unset. */
static inline const char *xsg_w32_dir(const char *var, char *out, size_t n)
{
    const char *v = getenv(var);
    snprintf(out, n, "%s", v ? v : "");
    for (char *c = out; *c; c++)
        if (*c == '\\')
            *c = '/';
    return out;
}

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */
#endif /* XSG_RT_WIN_COMPAT_H */

/*
 * Windows port (port/docs/WINDOWS.md): the few POSIX calls xsg-gpu's sources
 * use, for MinGW-w64.  Force-included (-include) into xsg-gpu's own sources on
 * WIN32 only (gpu/CMakeLists.txt); the Linux build never sees it.
 *
 * Paths: the user directories (XsgWinDir) are returned with forward slashes,
 * which Windows accepts, so the code that splits paths on '/' keeps working.
 */
#ifndef XSG_WIN_COMPAT_H
#define XSG_WIN_COMPAT_H
#if defined(_WIN32) && defined(__cplusplus)

#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdlib>
#include <string>

/* mkdir(path, mode): MinGW's takes no mode. */
static inline int mkdir(const char* p, int) { return ::_mkdir(p); }

/* xsg_iso_check.h: binary, non-inherited reads at 64-bit offsets. */
#ifndef O_CLOEXEC
#define O_CLOEXEC (_O_NOINHERIT | _O_BINARY)
#endif
static inline long long pread(int fd, void* buf, size_t n, long long off) {
    if (_lseeki64(fd, off, SEEK_SET) < 0) return -1;
    return _read(fd, buf, (unsigned)n);
}

/* setenv(name, value, overwrite) */
static inline int setenv(const char* n, const char* v, int overwrite) {
    if (!overwrite && std::getenv(n)) return 0;
    return _putenv_s(n, v);
}

/* rename(tmp, existing): the CRT's fails when the target exists (settings.ini and the
   pipeline cache are saved through a temporary file); replace it as POSIX does. */
#include <cstdio>
extern "C" __declspec(dllimport) int __stdcall MoveFileExA(const char*, const char*, unsigned long);
static inline int xsg_w32_rename(const char* from, const char* to) {
    return MoveFileExA(from, to, 0x1 | 0x2) ? 0 : -1;   /* REPLACE_EXISTING | COPY_ALLOWED */
}
namespace std { using ::xsg_w32_rename; }
#define rename xsg_w32_rename

/* %VAR% (APPDATA, LOCALAPPDATA, USERPROFILE) with '/' separators, "" if unset. */
static inline std::string XsgWinDir(const char* var) {
    const char* v = std::getenv(var);
    std::string s = v ? v : "";
    for (char& c : s)
        if (c == '\\') c = '/';
    return s;
}

#endif
#endif /* XSG_WIN_COMPAT_H */

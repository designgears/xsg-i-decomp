/*
 * Is this file the user's Xenosaga Episode I disc image?  (port/docs/SETUP.md)
 *
 * Header-only C, shared by the game process (disc.c, 32-bit, built with
 * _FILE_OFFSET_BITS=64) and xsg-gpu's setup screen (menu/menu.cpp, 64-bit), so
 * both sides judge a file the same way:
 *
 *   - it opens and is a regular file;
 *   - sector 16 (2048-byte sectors) holds an ISO9660 primary volume descriptor
 *     (a raw 2352-byte-sector .bin is recognised and named as such);
 *   - the root directory has SYSTEM.CNF and its BOOT2 line names SLUS_204.69.
 *
 * The extension does not matter: a .bin / .img with 2048-byte sectors is an ISO.
 */
#ifndef XSG_ISO_CHECK_H
#define XSG_ISO_CHECK_H

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    XSG_ISO_OK = 0,
    XSG_ISO_NONE,        /* no image configured and none in <game_dir>/iso */
    XSG_ISO_GONE,        /* the path does not exist (configured before, since moved or deleted) */
    XSG_ISO_UNREADABLE,  /* exists but cannot be opened / read (err holds errno) */
    XSG_ISO_NOT_FILE,    /* a directory or a device */
    XSG_ISO_NOT_ISO,     /* no ISO9660 volume descriptor at sector 16 */
    XSG_ISO_RAW,         /* a raw 2352-byte-sector image (.bin / .cue) */
    XSG_ISO_NOT_PS2,     /* ISO9660 without SYSTEM.CNF: not a PlayStation 2 disc */
    XSG_ISO_WRONG_GAME,  /* a PS2 disc of another game (boot holds its executable) */
    XSG_ISO_INCOMPLETE,  /* this game, but truncated (shorter than its volume) or without SLUS_204.69 */
};

#define XSG_ISO_EXE "SLUS_204.69"

typedef struct {
    long long size;      /* file size in bytes (0 if unknown) */
    int err;             /* errno for XSG_ISO_UNREADABLE */
    char boot[64];       /* the BOOT2 executable SYSTEM.CNF names ("" if none) */
} XsgIsoInfo;

static inline unsigned xsg_iso_le32_(const unsigned char* p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24;
}

static inline int xsg_iso_read_(int fd, long long off, void* buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t r = pread(fd, (char*)buf + got, n - got, (off_t)(off + (long long)got));
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}

/* Checks `path`; fills *info (may be NULL).  Returns an XSG_ISO_* code. */
static inline int xsg_iso_check(const char* path, XsgIsoInfo* info) {
    XsgIsoInfo dummy;
    if (!info) info = &dummy;
    memset(info, 0, sizeof *info);
    if (!path || !*path) return XSG_ISO_NONE;
    struct stat st;
    if (stat(path, &st) != 0) {
        info->err = errno;
        return errno == ENOENT || errno == ENOTDIR ? XSG_ISO_GONE : XSG_ISO_UNREADABLE;
    }
    if (!S_ISREG(st.st_mode)) return XSG_ISO_NOT_FILE;
    info->size = (long long)st.st_size;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        info->err = errno;
        return XSG_ISO_UNREADABLE;
    }
    unsigned char s[2048];
    int rc = XSG_ISO_NOT_ISO;
    unsigned char* dir = NULL;
    if (xsg_iso_read_(fd, 16LL * 2048, s, sizeof s) || s[0] != 1 || memcmp(s + 1, "CD001", 5) || s[6] != 1) {
        /* raw 2352-byte sectors: the PVD after the 16-byte header (mode 1) or 24 (mode 2 form 1) */
        unsigned char r[32];
        if (!xsg_iso_read_(fd, 16LL * 2352, r, sizeof r) &&
            ((r[16] == 1 && !memcmp(r + 17, "CD001", 5)) || (r[24] == 1 && !memcmp(r + 25, "CD001", 5))))
            rc = XSG_ISO_RAW;
        goto out;
    }
    {
        const unsigned vol = xsg_iso_le32_(s + 80);
        const unsigned root = xsg_iso_le32_(s + 156 + 2), rsize = xsg_iso_le32_(s + 156 + 10);
        rc = XSG_ISO_NOT_PS2;
        if (!root || root >= vol || !rsize || rsize > (1u << 20)) goto out;
        dir = (unsigned char*)malloc(rsize);
        if (!dir || xsg_iso_read_(fd, (long long)root * 2048, dir, rsize)) goto out;
        unsigned cnf = 0, csize = 0, exe = 0;
        for (unsigned off = 0; off < rsize;) {
            const unsigned char* d = dir + off;
            if (d[0] == 0) { off = (off / 2048 + 1) * 2048; continue; }
            if (d[0] < 34 || off + d[0] > rsize) break;
            const int nl = d[32];
            if (nl >= 10 && 33 + nl <= d[0] && !strncasecmp((const char*)d + 33, "SYSTEM.CNF", 10) &&
                (nl == 10 || d[33 + 10] == ';')) {
                cnf = xsg_iso_le32_(d + 2);
                csize = xsg_iso_le32_(d + 10);
            }
            if (nl >= 11 && 33 + nl <= d[0] && !strncasecmp((const char*)d + 33, XSG_ISO_EXE, 11) &&
                (nl == 11 || d[33 + 11] == ';'))
                exe = xsg_iso_le32_(d + 10);
            off += d[0];
        }
        if (!cnf || !csize || cnf >= vol) goto out;
        char text[2049];
        if (csize > 2048) csize = 2048;
        if (xsg_iso_read_(fd, (long long)cnf * 2048, text, csize)) goto out;
        text[csize] = 0;
        /* BOOT2 = cdrom0:\SLUS_204.69;1 */
        const char* b = strstr(text, "BOOT2");
        if (!b) goto out;
        b = strchr(b, '=');
        if (!b) goto out;
        b++;
        while (*b == ' ' || *b == '\t') b++;
        if (!strncasecmp(b, "cdrom0:", 7)) b += 7;
        while (*b == '\\' || *b == '/') b++;
        size_t n = 0;
        while (b[n] && b[n] != ';' && b[n] != '\r' && b[n] != '\n' && !isspace((unsigned char)b[n]) && n < sizeof info->boot - 1) {
            info->boot[n] = b[n];
            n++;
        }
        info->boot[n] = 0;
        rc = !strcasecmp(info->boot, XSG_ISO_EXE) ? XSG_ISO_OK : XSG_ISO_WRONG_GAME;
        /* layer 0 must be all there (a dual-layer image has layer 1 after it) */
        if (rc == XSG_ISO_OK && (!exe || (long long)vol * 2048 > info->size)) rc = XSG_ISO_INCOMPLETE;
    }
out:
    free(dir);
    close(fd);
    return rc;
}

/* "7.9 GB" */
static inline void xsg_iso_size_str(long long n, char* out, size_t len) {
    if (n >= (1LL << 30)) snprintf(out, len, "%.1f GB", (double)n / (1LL << 30));
    else if (n >= (1LL << 20)) snprintf(out, len, "%.1f MB", (double)n / (1LL << 20));
    else snprintf(out, len, "%lld KB", (n + 1023) >> 10);
}

/* One line for the user (the setup screen and the game's log). */
static inline void xsg_iso_message(int code, const XsgIsoInfo* info, const char* path, char* out, size_t len) {
    const char* base = path ? strrchr(path, '/') : NULL;
    base = base ? base + 1 : path ? path : "";
    switch (code) {
    case XSG_ISO_OK: {
        char sz[32];
        xsg_iso_size_str(info ? info->size : 0, sz, sizeof sz);
        snprintf(out, len, "Found: %s (%s)", base, sz);
        break;
    }
    case XSG_ISO_NONE: snprintf(out, len, "No disc image found"); break;
    case XSG_ISO_GONE: snprintf(out, len, "The disc image is no longer there: %s", path ? path : ""); break;
    case XSG_ISO_UNREADABLE: snprintf(out, len, "Cannot read %s: %s", base, strerror(info ? info->err : EIO)); break;
    case XSG_ISO_NOT_FILE: snprintf(out, len, "%s is not a file", base); break;
    case XSG_ISO_NOT_ISO: snprintf(out, len, "%s is not a disc image (no ISO9660 volume)", base); break;
    case XSG_ISO_RAW:
        snprintf(out, len, "%s has raw 2352-byte sectors (.bin/.cue): convert it to an .iso with 2048-byte sectors", base);
        break;
    case XSG_ISO_NOT_PS2: snprintf(out, len, "%s is not a PlayStation 2 disc (no SYSTEM.CNF)", base); break;
    case XSG_ISO_WRONG_GAME:
        snprintf(out, len, "This is not Xenosaga Episode I (SLUS-20469): it boots %s", info && info->boot[0] ? info->boot : "something else");
        break;
    case XSG_ISO_INCOMPLETE:
        snprintf(out, len, "%s is Xenosaga Episode I but incomplete (truncated, or a bad dump)", base);
        break;
    default: snprintf(out, len, "Unknown problem with %s", base); break;
    }
}

#ifdef __cplusplus
}
#endif

#endif

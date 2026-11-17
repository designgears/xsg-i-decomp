/* Windows port: fnmatch (*, ?, [...]) for memcard.c's directory listing (win32/platform_win32.c). */
#ifndef XSG_W32_FNMATCH_H
#define XSG_W32_FNMATCH_H
#define FNM_NOMATCH 1
int fnmatch(const char *pattern, const char *string, int flags);
#endif

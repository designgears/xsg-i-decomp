/* Windows port: backtrace() through RtlCaptureStackBackTrace (win32/platform_win32.c);
   symbols are raw addresses, resolved offline with addr2line on xsg.dll. */
#ifndef XSG_W32_EXECINFO_H
#define XSG_W32_EXECINFO_H
#ifdef __cplusplus
extern "C" {
#endif
int backtrace(void **buf, int n);
void backtrace_symbols_fd(void *const *buf, int n, int fd);
#ifdef __cplusplus
}
#endif
#endif

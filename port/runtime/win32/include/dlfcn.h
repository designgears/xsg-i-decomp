/* Windows port: no dlopen (audio.c's optional ALSA output is Linux-only; the sound
   goes to xsg-gpu, which plays it through SDL3). */
#ifndef XSG_W32_DLFCN_H
#define XSG_W32_DLFCN_H
#define RTLD_NOW 2
#define RTLD_LAZY 1
static inline void *dlopen(const char *f, int m) { (void)f; (void)m; return 0; }
static inline void *dlsym(void *h, const char *s) { (void)h; (void)s; return 0; }
static inline const char *dlerror(void) { return "not available on Windows"; }
static inline int dlclose(void *h) { (void)h; return 0; }
#endif

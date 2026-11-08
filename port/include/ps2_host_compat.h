/*
 * ps2_host_compat.h -- force-included (-include) by the Stage 1 host compile
 * survey (port/scripts/host_compile_survey.py).  NOT used by the PS2 build.
 *
 * Provides host stand-ins for PS2/EE-specific constructs so the survey measures
 * the real blockers instead of drowning in trivial ones.
 */
#ifndef PS2_HOST_COMPAT_H
#define PS2_HOST_COMPAT_H

/* EE FPU division: x/0 is +-FLT_MAX (sign of x xor 0's sign), never inf/NaN, so
 * expressions like 0 * (1/0) stay finite (0) where IEEE gives NaN.  For host
 * fixes of game code that divides by a possibly-zero float (PORT_HOST only). */
static inline float xsg_ee_divf(float a, float b)
{
    if (b == 0.0f) {
        union { float f; unsigned u; } ua = {a}, ub = {b}, r;
        r.u = ((ua.u ^ ub.u) & 0x80000000u) | 0x7f7fffffu;
        return r.f;
    }
    return a / b;
}


/* Scaffold macros: the PS2 build turns these into .include of MIPS asm.
 * include/include_asm.h already honours SKIP_ASM; define them here too. */
#ifndef SKIP_ASM
#define SKIP_ASM 1
#endif
#define INCLUDE_ASM(FOLDER, NAME)
#define ACCEPTED_ASM(FOLDER, NAME)
#define INCLUDE_RODATA(FOLDER, NAME)

/* 128-bit EE quadwords: one definition shared with include/port/ps2_types.h. */
#define PS2_U128_DEFINED
typedef union ps2_u128 {
    unsigned int w[4];
    unsigned long long d[2];
    float f[4];
    unsigned char b[16];
} __attribute__((aligned(16))) ps2_u128;
typedef ps2_u128 u_long128;
typedef ps2_u128 u128;


/* ee-gcc varargs: the game's hand-written va_start macros subtract the MIPS
 * register save area, (8 - __builtin_args_info(2)) * 8 bytes.  On i386 every
 * argument is on the stack after the last named one, so the offset is 0. */
#define __builtin_args_info(n) 8

/* The game validates callback pointers by checking they lie in EE RAM code
 * (`addr - 0x1F0000 <= 0x1E0FFFF && !(addr & 3)`).  Host code lives in the
 * executable's text segment (ld's __executable_start..etext, overlays
 * included) and is not 4-aligned, so the PORT_HOST side of those sites uses
 * this instead. */
extern char __executable_start[], etext[];
#define XSG_HOST_IS_CODE(a) ((unsigned int)(a) >= (unsigned int)__executable_start && \
                             (unsigned int)(a) < (unsigned int)etext)

/* Alias views (STAGE10_PERF.md, cause 4).  The PS2 sources give one symbol a
 * second C name: `extern T FS_view __asm__("FS");` next to `extern char FS[];`.
 * GCC treats the two declarations as distinct objects and caches loads across
 * stores to the other name.  Under PORT_HOST each view is renamed to
 * `xsg_view_<name>` and `<name>` becomes XSG_VIEW(xsg_view_<name>): every
 * access goes through a pointer laundered by an empty asm, which GCC must
 * assume points to any global, so it aliases the real declaration.  The
 * renamed declaration is never accessed directly (scripts/find_alias_views.py
 * checks this). */
static inline __attribute__((always_inline)) void *xsg_launder(void *p) {
    __asm__("" : "+r"(p));
    return p;
}
#define XSG_VIEW(v) (*(__typeof__(v) *)xsg_launder((void *)&(v)))

#endif /* PS2_HOST_COMPAT_H */

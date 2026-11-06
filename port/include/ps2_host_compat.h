/*
 * ps2_host_compat.h -- force-included (-include) by the Stage 1 host compile
 * survey (port/scripts/host_compile_survey.py).  NOT used by the PS2 build.
 *
 * Provides host stand-ins for PS2/EE-specific constructs so the survey measures
 * the real blockers instead of drowning in trivial ones.
 */
#ifndef PS2_HOST_COMPAT_H
#define PS2_HOST_COMPAT_H

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

#endif /* PS2_HOST_COMPAT_H */

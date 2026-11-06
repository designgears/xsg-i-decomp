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

/* 128-bit (EE GPRs are 128-bit; game uses `mode(TI)` quadword types).
 * i386 gcc has no TImode, x86-64 does.  Game sources spell the typedef
 * themselves, so the survey cannot redirect it from here; these types are
 * the intended replacement for a future source-level fix. */
#if defined(__SIZEOF_INT128__)
typedef unsigned __int128 ps2_u128;
#else
typedef struct { unsigned int w[4]; } __attribute__((aligned(16))) ps2_u128;
#endif
typedef ps2_u128 u_long128;
typedef ps2_u128 u128;


#endif /* PS2_HOST_COMPAT_H */

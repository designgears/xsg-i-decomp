#ifndef XENO_TYPES_H
#define XENO_TYPES_H

/*
 * Project-wide fixed-width names, visible in every TU through common.h.
 * EE GCC 2.96 accepts an identical typedef twice, so the TU-local copies of
 * these (fcv2.h, cgp.h, shared.h, ...) stay valid; new code should just use
 * these. Codegen is unaffected: a typedef emits nothing.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;

#endif

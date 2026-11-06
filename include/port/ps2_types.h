#ifndef PORT_PS2_TYPES_H
#define PORT_PS2_TYPES_H

/*
 * 128-bit EE quadword typedefs shared by the PS2 matching build and the host.
 *
 *   PS2_TI_TYPEDEF(unsigned int, Quadword);
 *
 * PS2 build: expands to exactly `typedef unsigned int Quadword
 * __attribute__((mode(TI)))`, token-for-token the original.
 * Host (-DPORT_HOST): i386 gcc has no TImode, so the type becomes
 * ps2_u128, a 16-byte, 16-aligned struct.  Arithmetic on these types must be
 * ported by hand; plain copies (assignment, memcpy) keep working.
 */
/*
 * PS2_QZERO          quadword zero (`0` on PS2, a zero ps2_u128 on host)
 * PS2_QCONST(lit, w0, w1, w2, w3)
 *                    a 128-bit literal: `lit` on PS2, ps2_u128 words
 *                    w0 (least significant) .. w3 on host
 */
#ifndef PORT_HOST
#define PS2_TI_TYPEDEF(base, name) typedef base name __attribute__((mode(TI)))
#define PS2_QZERO 0
#define PS2_QCONST(lit, w0, w1, w2, w3) lit
#else
#define PS2_QZERO ((ps2_u128){{0, 0, 0, 0}})
#define PS2_QCONST(lit, w0, w1, w2, w3) ((ps2_u128){{w0, w1, w2, w3}})
#ifndef PS2_U128_DEFINED
#define PS2_U128_DEFINED
typedef union ps2_u128 {
    unsigned int w[4];
    unsigned long long d[2];
    float f[4];
    unsigned char b[16];
} __attribute__((aligned(16))) ps2_u128;
#endif
#define PS2_TI_TYPEDEF(base, name) typedef ps2_u128 name
#endif

#endif

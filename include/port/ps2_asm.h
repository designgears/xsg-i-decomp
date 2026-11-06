#ifndef PORT_PS2_ASM_H
#define PORT_PS2_ASM_H

/*
 * Inline-asm wrappers shared by the PS2 matching build and the host port.
 *
 *   PS2_ASMV(...)  an `__asm__ __volatile__ (...)` statement
 *   PS2_ASM(...)   a non-volatile `__asm__ (...)` statement
 *   PS2_REG(r)     a `register T x PS2_REG("$f21")` hard-register binding
 *
 * PS2 build (PORT_HOST undefined): each macro expands to exactly the
 * original construct, so the preprocessed C seen by ee-gcc's cc1 is
 * token-for-token the same as before.  GNU `args...` variadics are used
 * because the ee-gcc 2.9-991111 cpp variant has no __VA_ARGS__.
 *
 * Host port (-DPORT_HOST): statements become a no-op stub, or a trap with
 * -DPORT_ASM_TRAP, until each block is replaced by C (Stage 3).  Register
 * bindings only steer ee-gcc's allocator, so they are dropped.
 */
#ifndef PORT_HOST
#define PS2_ASMV(args...) __asm__ __volatile__(args)
#define PS2_ASM(args...) __asm__(args)
#define PS2_REG(r) __asm__(r)
#else
#ifdef PORT_ASM_TRAP
#define PS2_ASM_STUB() __builtin_trap()
#else
#define PS2_ASM_STUB() ((void)0)
#endif
#define PS2_ASMV(args...) PS2_ASM_STUB()
#define PS2_ASM(args...) PS2_ASM_STUB()
#define PS2_REG(r)
#endif

#endif

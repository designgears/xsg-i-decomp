#ifndef COMMON_H
#define COMMON_H

#include "include_asm.h"

/* The EE's `long` is 64 bits (ee-gcc, as SCE's u_long); i386's is 32.  Game
 * code spells it PS2_LONG, which is plain `long` in the PS2 build. */
#ifndef PS2_LONG
#ifdef PORT_HOST
#define PS2_LONG long long
#else
#define PS2_LONG long
#endif
#endif

#endif

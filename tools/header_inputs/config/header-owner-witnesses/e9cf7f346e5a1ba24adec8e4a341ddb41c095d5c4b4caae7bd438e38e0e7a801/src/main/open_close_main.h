#ifndef SRC_MAIN_OPEN_CLOSE_MAIN_H
#define SRC_MAIN_OPEN_CLOSE_MAIN_H

#include "shared.h"

typedef struct ECursol {
    signed char state;    /* +0x00 */
    signed char active;    /* +0x01 */
    signed char timer;      /* +0x02 */
    signed char width;       /* +0x03 */
    short x;                  /* +0x04 */
    short y;                   /* +0x06 */
    int work;                   /* +0x08 */
    signed char color[4];        /* +0x0c..0x0f */
} ECursol;

#endif

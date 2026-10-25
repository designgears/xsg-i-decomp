#ifndef SRC_MAIN_MSG_INIT_H
#define SRC_MAIN_MSG_INIT_H

#include "shared.h"

typedef struct MsgBuffer {
    int active;
    unsigned int capacity;
    unsigned char **entry_table;
    unsigned char *cursor;
    unsigned char workspace[0x800];
} MsgBuffer;

typedef struct MsgQueue {
    unsigned char *buffer;
    unsigned char unmodeled_04[4];
    short head;
    short tail;
    short size;
    short count;
    short width;
    short last;
    short limit;
    unsigned char unmodeled_22[6];
    int label;
} MsgQueue;

#endif

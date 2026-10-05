#include "common.h"
#include "shared.h"
#include "xgl_timer0.h"

/*
 * EE Timer 0.  T0_COUNT is the free-running counter and T0_MODE its control
 * word; bit 7 (0x80) of T0_MODE is CUE, which starts the count.
 */
#define T0_COUNT ((volatile u32 *)0x10000000)
#define T0_MODE  ((volatile u32 *)0x10000010)
#define T0_MODE_CUE 128

typedef struct ArxBitReader {
    u32 value;
    u32 bit_count;
    u32 *data;
} ArxBitReader;

void xglTimer0Reset(int mode)
{
    *T0_COUNT = 0;
    *T0_MODE = mode + T0_MODE_CUE;
}

int xglTimer0Get(void)
{
    return *T0_COUNT & 0xffff;
}

static u32 getbit1(ArxBitReader *reader)
{
    u32 bit;

    if (reader->bit_count == 0) {
        reader->value = *reader->data++;
    }
    reader->bit_count = (reader->bit_count - 1) & 0x1f;
    bit = reader->value >> 31;
    reader->value <<= 1;
    return bit;
}

static u32 getbits(ArxBitReader *reader, u32 n)
{
    u32 result = 0;
    u32 bc = reader->bit_count;
    u32 v;

    if (bc < n) {
        n -= bc;
        result = reader->value >> (32 - bc);
        reader->value = *reader->data++;
        reader->bit_count = 32;
        bc = 32;
    }
    v = reader->value;
    result = (result << n) | (v >> (32 - n));
    reader->bit_count = bc - n;
    reader->value = v << n;
    return result;
}

INCLUDE_ASM("asm/main/nonmatchings/xgl_timer0", xglArxExtract);

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

#define ARX_DECODE(v)                          \
    do {                                       \
        if (getbit1(&r) == 0) {                \
            v = *r.data++;                     \
        } else {                               \
            n = 1;                             \
            k = 0;                             \
            while (getbit1(&r)) {              \
                k += 1 << n;                   \
                n++;                           \
            }                                  \
            v = table[k + getbits(&r, n)];     \
        }                                      \
    } while (0)

int xglArxExtract(u32 *dst, int *src)
{
  ArxBitReader r;
  int size;
  int packed;
  u32 *table;
  u32 *out;
  u32 *end;
  int i;
  u32 n;
  int k;
  u32 v;
  out = dst;
  if ((src[0] != 0x585241) || (src[3] != 0))
  {
    return -1;
  }
  size = src[1];
  packed = src[2];
  table = (u32 *) (((int) (WorkEnd + 0x3f)) & (~0x3f));
  for (i = 0; i < 30; i++)
  {
    table[i] = src[i + 4];
  }

  r.value = 0;
  r.bit_count = 0;
  r.data = (u32 *) (src + 0x22);
  if (((r.data < dst) || ((dst + (size / 4)) < r.data)) && (((r.data + (packed / 4)) < dst) || ((dst + (size / 4)) < (r.data + (packed / 4)))))
  {
    goto plain;
  }
  end = (u32 *) (((char *) dst) + size);
  if (out >= end)
  {
    goto done;
  }
  {
    int *srcEnd = src + (packed / 4);
    do
    {
      do
      {
        if (getbit1(&r) == 0)
        {
          v = *(r.data++);
        }
        else
        {
          n = 1;
          k = 0;
          while (getbit1(&r))
          {
            k += 1 << n;
            n++;
          }

          out++;
          out--;
          v = table[k + getbits(&r, n)];
        }
      }
      while (0);
      *(out++) = v;
      if (out >= r.data)
      {
        u32 *s = r.data;
        u32 *d = table + 32;
        u32 *lim = (u32 *) (srcEnd + 0x22);
        r.data = d;
        while (s <= lim)
        {
          *(d++) = *(s++);
        }

        break;
      }
    }
    while (out < end);
  }
  plain:
  while (out < ((u32 *) (((char *) dst) + size)))
  {
    do
    {
      if (getbit1(&r) == 0)
      {
        v = *(r.data++);
      }
      else
      {
        n = 1;
        k = 0;
        while (getbit1(&r))
        {
          k += 1 << n;
          n++;
        }

        v = table[k + getbits(&r, n)];
      }
    }
    while (0);
    *(out++) = v;
  }


  done:
  return size;

}

#include "common.h"

extern unsigned char FS[];
static unsigned long ModeEnv[32] = {
    0x0000000000000000ULL, 0x5100000700000000ULL,
    0x1000000000008006ULL, 0x000000000000000EULL,
    0x0000000000000000ULL, 0x000000000000004CULL,
    0x0000000000000000ULL, 0x0000000000000018ULL,
    0x00DF0000013F0000ULL, 0x0000000000000040ULL,
    0x0000000100000000ULL, 0x000000000000004EULL,
    0x0000000000030000ULL, 0x0000000000000047ULL,
    0x0000000000000000ULL, 0x0000000000000022ULL,
    0x0000000000000000ULL, 0x5100000700000000ULL,
    0x1000000000008006ULL, 0x000000000000000EULL,
    0x0000000000000000ULL, 0x000000000000004CULL,
    0x0000000000000000ULL, 0x0000000000000018ULL,
    0x00DF0000013F0000ULL, 0x0000000000000040ULL,
    0x0000000100000000ULL, 0x000000000000004EULL,
    0x0000000000030000ULL, 0x0000000000000047ULL,
    0x0000000000000000ULL, 0x0000000000000022ULL
};
static unsigned char KanjiClutEnv[240] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x51, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x38, 0x05, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0x80,
    0xAA, 0xAA, 0xAA, 0x80, 0xFF, 0xFF, 0xFF, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x55, 0x55, 0x55, 0x80, 0xAA, 0xAA, 0xAA, 0x80, 0xFF, 0xFF, 0xFF, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0x80, 0x55, 0x55, 0x55, 0x80,
    0x55, 0x55, 0x55, 0x80, 0x55, 0x55, 0x55, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x55, 0x55, 0x55, 0x80, 0xAA, 0xAA, 0xAA, 0x80, 0xFF, 0xFF, 0xFF, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0x80, 0xAA, 0xAA, 0xAA, 0x80,
    0xFF, 0xFF, 0xFF, 0x80, 0xAA, 0xAA, 0xAA, 0x80, 0xAA, 0xAA, 0xAA, 0x80,
    0xAA, 0xAA, 0xAA, 0x80, 0xAA, 0xAA, 0xAA, 0x80, 0xFF, 0xFF, 0xFF, 0x80,
    0xFF, 0xFF, 0xFF, 0x80, 0xFF, 0xFF, 0xFF, 0x80, 0xFF, 0xFF, 0xFF, 0x80
};
static unsigned char ascii2euc[192] = {
    0xA1, 0xA1, 0xB5, 0xD1, 0xC2, 0xD1, 0xE5, 0xD1, 0xA9, 0xD2, 0xE6, 0xD1,
    0xA8, 0xD2, 0xC0, 0xD1, 0xCB, 0xD1, 0xCC, 0xD1, 0xB4, 0xD1, 0xFE, 0xD1,
    0xBA, 0xD1, 0xFD, 0xD1, 0xB9, 0xD1, 0xD4, 0xD1, 0xB0, 0xA3, 0xB1, 0xA3,
    0xB2, 0xA3, 0xB3, 0xA3, 0xB4, 0xA3, 0xB5, 0xA3, 0xB6, 0xA3, 0xB7, 0xA3,
    0xB8, 0xA3, 0xB9, 0xA3, 0xBC, 0xD1, 0xBB, 0xD1, 0xC7, 0xD1, 0xFC, 0xD1,
    0xC8, 0xD1, 0xB7, 0xD1, 0xAD, 0xD1, 0xC1, 0xA3, 0xC2, 0xA3, 0xC3, 0xA3,
    0xC4, 0xA3, 0xC5, 0xA3, 0xC6, 0xA3, 0xC7, 0xA3, 0xC8, 0xA3, 0xC9, 0xA3,
    0xCA, 0xA3, 0xCB, 0xA3, 0xCC, 0xA3, 0xCD, 0xA3, 0xCE, 0xA3, 0xCF, 0xA3,
    0xD0, 0xA3, 0xD1, 0xA3, 0xD2, 0xA3, 0xD3, 0xA3, 0xD4, 0xA3, 0xD5, 0xA3,
    0xD6, 0xA3, 0xD7, 0xA3, 0xD8, 0xA3, 0xD9, 0xA3, 0xDA, 0xA3, 0xCD, 0xD1,
    0xA5, 0xD1, 0xCE, 0xD1, 0xD8, 0xD1, 0xD3, 0xD1, 0xDA, 0xD1, 0xE1, 0xA3,
    0xE2, 0xA3, 0xE3, 0xA3, 0xE4, 0xA3, 0xE5, 0xA3, 0xE6, 0xA3, 0xE7, 0xA3,
    0xE8, 0xA3, 0xE9, 0xA3, 0xEA, 0xA3, 0xEB, 0xA3, 0xEC, 0xA3, 0xED, 0xA3,
    0xEE, 0xA3, 0xEF, 0xA3, 0xF0, 0xA3, 0xF1, 0xA3, 0xF2, 0xA3, 0xF3, 0xA3,
    0xF4, 0xA3, 0xF5, 0xA3, 0xF6, 0xA3, 0xF7, 0xA3, 0xF8, 0xA3, 0xF9, 0xA3,
    0xFA, 0xA3, 0xCF, 0xD1, 0xD1, 0xD1, 0xD0, 0xD1, 0xE8, 0xD1, 0xA1, 0xA1
};

void xglFontDebugMode(int mode)
{
  unsigned long *new_var2;
  if (mode >= 0)
  {
    unsigned int new_var;
    FS[11] = mode;
    new_var = 4;
    new_var2 = ModeEnv;
    (new_var2 + 16)[new_var] = ((unsigned long) FS[11]) | 0x50000;
    (ModeEnv + 0)[new_var] = ((unsigned long) (FS[11] + 0x23)) | 0x50000;
  }
}

unsigned int xglFontGetKanjiClutUV(unsigned int arg0, int *arg1)
{
  unsigned short c;
  unsigned short t;
  unsigned char lo;
  unsigned char hi;
  unsigned short new_var;
  unsigned int v;
  c = arg0;
  new_var = c >> 8;
  hi = (lo = c & 0xFF);
  t = ((lo * 0x5E) + new_var) - 0xA1;
  if (hi < 0xAD)
  {
    t += 0xC4E2;
    hi = (t >> 5) & 0xFF;
    v = t & 0x1F;
    v = v * 0x140;
  }
  else
    if (hi < 0xB0)
  {
    t += 0xC30C;
    hi = (t >> 5) & 0xFF;
    v = (t & 0x1F) * 0x140;
  }
  else
  {
    t += 0xC250;
    c = t;
    new_var = c & 0x1F;
    v = new_var * 0x140;
    hi = 0xFF & (c >> 5);
  }
  if (0 != arg1)
  {
    *arg1 = ((hi >> 5) << 12) + 0x1004;
  }
  return ((hi & 0x1F) * 0x1800000) + v;
}

/*
 * Each 4-byte slot of the font print-queue buffer at D_008811A0 (16 slots,
 * scaffold-owned splat name): link is the byte offset, from this array's
 * base, of the next queued record for this bucket (0 marks the bucket
 * empty -- xglFontFlush follows it as a chain), and selfOffset is the
 * slot's own byte offset within the array.
 */
typedef struct FontQueueSlot {
    unsigned short link;
    unsigned short selfOffset;
} FontQueueSlot;

extern FontQueueSlot D_008811A0[16];

static void set_ot(int ot)
{
    unsigned char *header = (unsigned char *)D_008811A0 - 0x20;
    FontQueueSlot *slot = (FontQueueSlot *)((unsigned char *)D_008811A0 + ((ot >> 22) & 0x3C));
    unsigned short off = *(unsigned short *)(header + 0x1C) - (int)D_008811A0;
    unsigned char *p = (unsigned char *)D_008811A0 + slot->selfOffset;
    p[0] = off;
    p[1] = off >> 8;
    slot->selfOffset = off;
    (*(unsigned char **)(header + 0x1C))[0] = 0;
    (*(unsigned char **)(header + 0x1C))[1] = 0;
    *(unsigned char **)(header + 0x1C) += 2;
}

static void set_xyz(int x, int y, int z)
{
    set_ot(z);
    if ((unsigned int)x >= 0x300 || y < 0 || y >= 0x200) {
        unsigned char *p = ((struct { unsigned char pad[0x1C]; unsigned char *cur; } *)FS)->cur;
        p[0] = 8;
        p[1] = 7;
        p[2] = x >> 8;
        p[3] = x;
        p[4] = y >> 8;
        p[5] = y;
        p[6] = z >> 16;
        p[7] = z >> 8;
        p[8] = z;
        ((struct { unsigned char pad[0x1C]; unsigned char *cur; } *)FS)->cur += 9;
    } else {
        unsigned char *p = ((struct { unsigned char pad[0x1C]; unsigned char *cur; } *)FS)->cur;
        p[0] = (x >> 8) + 1;
        p[1] = x;
        p[2] = (y >> 8) + 4;
        p[3] = y;
        p[4] = 6;
        p[5] = z >> 16;
        p[6] = z >> 8;
        p[7] = z;
        ((struct { unsigned char pad[0x1C]; unsigned char *cur; } *)FS)->cur += 8;
    }
}


unsigned char xglFontGetSPcodeSize(unsigned char code, unsigned char *text)
{
    static unsigned char spcode_0[32] __asm__("spcode.0") = {
        0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x03, 0x00, 0xFF, 0x00, 0x00, 0x00,
        0x03, 0x01, 0x05, 0x03, 0x05, 0x06, 0x02, 0x02, 0x00, 0xFF, 0x00, 0x07,
        0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00
    };
    unsigned char size;

    size = spcode_0[code];
    if (size == 0xFF) {
        switch (code) {
        case 8: {
            unsigned char tbl[8] = { 2, 2, 3, 2, 2, 0, 0, 0 };
            unsigned char mask = text[1];
            int i;

            size = 1;
            for (i = 0; i < 8; i++) {
                if (mask & 1) {
                    size += tbl[i];
                }
                mask >>= 1;
            }
            break;
        }
        case 0x15:
            size = text[1] + 1;
            break;
        }
    }
    return size;
}

extern unsigned char *D_0088119C[];

static void xglFontPrintSub(void *args)
{
  long *ap;
  char *text;
  unsigned char *fs;
  unsigned char *fsn;
  unsigned char *fsf;
  unsigned char *fsp;
  unsigned char *fse;
  unsigned char *p;
  unsigned char *pf;
  unsigned char c;
  int size;
  int width;
  unsigned char base;
  unsigned long v;
  unsigned int fv;
  float f;
  unsigned char *s;
  unsigned long st;
  unsigned char b2;
  unsigned char b3;
  unsigned char b4;
  unsigned char b5;
  unsigned char b6;
  ap = (long *) args;
  text = (char *) ((int) (*(ap++)));
  if (*text != 0)
  {
    do
    {
      c = *text;
      if (c < 0x20)
      {
        size = xglFontGetSPcodeSize(c, (unsigned char *) text);
        if (size >= 0)
        {
          do
          {
            *((*D_0088119C)++) = *text;
            text++;
          }
          while ((--size) >= 0);
        }
      }
      else if (c == 0x25)
      {
        width = 0;
        text++;
        while (1)
        {
          c = *(text++);
          switch (c)
          {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
              width = ((width * 10) + c) - '0';
              continue;
            case 'b':
              base = 2;
              goto num;
            case 'd':
              base = 10;
              goto num;
            case 'x':
              base = 16;
            num:
              if (width == 0)
                width = 1;
              v = (unsigned int) (*((int *) (ap++)));
              fsn = FS;
              p = *((unsigned char **) (fsn + 0x1C));
              b2 = width - 1;
              b3 = v >> 24;
              b4 = v >> 16;
              b5 = v >> 8;
              b6 = v;
              p[6] = b6;
              p[1] = base;
              p[2] = b2;
              p[3] = b3;
              p[4] = b4;
              p[5] = b5;
              p[0] = 0x11;
              *((unsigned char **) (fsn + 0x1C)) += 7;
              break;
            case 'f':
              f = *((double *) (ap++));
              fv = *((unsigned int *) (&f));
              fsf = FS;
              pf = *((unsigned char **) (fsf + 0x1C));
              pf[0] = 0x11;
              pf[1] = 0xFF;
              pf[2] = 0xFF;
              pf[3] = fv >> 24;
              pf[4] = fv >> 16;
              pf[5] = fv >> 8;
              pf[6] = fv;
              *((unsigned char **) (fsf + 0x1C)) += 7;
              break;
            case 's':
              s = (unsigned char *) ((int) (*(ap++)));
              while ((st = *s) != 0)
              {
                *((*D_0088119C)++) = *s;
                s++;
              }
              break;
            case 'c':
              c = *((unsigned char *) (ap++));
            case '%':
              fsp = FS;
              *((*((unsigned char **) (fsp + 0x1C)))++) = c;
              break;
            default:
              break;
          }
          break;
        }
      }
      else
      {
        fse = FS;
        *((*((unsigned char **) (fse + 0x1C)))++) = (unsigned char) (*text);
        text++;
      }
    }
    while (*text != 0);
  }
  fs = FS;
  *((*((unsigned char **) (fs + 0x1C)))++) = 0;
}

extern unsigned char *D_0088119C[];

static void xglFontPrintDirectCore(const char *text)
{
  unsigned char *fs;
  unsigned char code;
  int size;
  if ((*text) != 0)
  {
    do
    {
      code = (unsigned char) (*text);
      if (code < 0x20)
      {
        size = xglFontGetSPcodeSize(code, (unsigned char *) text);
        if (size > 0)
        {
          do
          {
            *((*D_0088119C)++) = (unsigned char) ((long) (*text));
            text++;
          }
          while ((--size) > 0);
        }
      }
      fs = FS;
      *((*D_0088119C)++) = (unsigned char) (*text);
      text++;
    }
    while ((*text) != 0);
  }
  fs = FS;
  *((*((unsigned char **) (fs + 0x1C)))++) = 0;
}

extern unsigned char D_00881188[];

static void set_xyz(int x, int y, int color);
static void xglFontPrintSub(void *args);

void xglFontPrintf(int x, int y, unsigned int color, void *arg)
{
    if (*(unsigned short *)D_00881188 & 1 & 0xFFFF) {
        set_xyz(x, y, color);
        xglFontPrintSub(arg);
    }
}

static void xglFontPrintDirectCore(const char *text);

void xglFontPrint(int x, int y, int color, const char *text)
{
    if (*(unsigned short *)D_00881188 & 1 & 0xFFFF) {
        set_xyz(x, y, color);
        xglFontPrintDirectCore(text);
    }
}

/* Prints text straight away into ordering-table slot ot (passed to set_ot). */
extern void xglFontPrintDirectOT(int ot, const char *text);

void xglFontPrintDirect(const char *text)
{
    xglFontPrintDirectOT(0, text);
}

static void set_ot(int ot);

void xglFontPrintDirectOT(int ot, const char *text)
{
    if (*(unsigned short *)D_00881188 & 1 & 0xFFFF) {
        set_ot(ot);
        xglFontPrintDirectCore(text);
    }
}

void xglFontPrintExtFunc(unsigned int ot, unsigned int x, unsigned int y)
{
    unsigned char *fs = FS;
    unsigned char *p;

    if (*(unsigned short *)(fs + 8) & 1 & 0xFFFF) {
        set_ot(ot);
        p = *(unsigned char **)(fs + 0x1C);
        p[0] = 0x17;
        p[1] = x >> 16;
        p[2] = x >> 8;
        p[3] = x;
        p[4] = y >> 24;
        p[5] = y >> 16;
        p[6] = y >> 8;
        p[7] = y;
        p[8] = 0;
        *(unsigned char **)(fs + 0x1C) += 9;
    }
}

typedef char *va_list;
#define FONT_ARGUMENT_START(list, format) ((list) = (va_list)__builtin_next_arg(format) - (8 - __builtin_args_info(2)) * 8)

void xglFontDebugPrintf(int x, int y, const char *format, ...)
{
    unsigned long long arguments[32];
    unsigned long long *output;
    unsigned long long *list;
    unsigned char *fs = FS;

    if ((*(unsigned short *)(fs + 8) & 3) == 3) {
        arguments[0] = (unsigned int)format;
        {
        int count;

        FONT_ARGUMENT_START(list, format);
        output = arguments + 1;
        count = 30;
        do {
            unsigned long long *argument = list++;
            unsigned long long value = *argument;

            count--;
            *output = value;
            output++;
        } while (count >= 0);
        }
        if (FS[0xB] == 0) {
            x <<= 1;
            y <<= 1;
        }
        set_xyz(x, y, -1);
        *(*(unsigned char **)(FS + 0x1C))++ = 11;
        xglFontPrintSub(arguments);
    }
}

/* Queues one "print N hex digits of value" record into the font print queue
 * (record byte 0 = 0xB, byte 1 = 0x10) and positions the cursor first. */
void xglFontDebugHex(int x, int y, unsigned int value, int digits)
{
    struct State {
        unsigned char unmodeled_00[8];
        unsigned short flags;
        unsigned char unmodeled_0A[1];
        unsigned char double_size;
        unsigned char unmodeled_0C[0x10];
        unsigned char *cur;
    } *fs = (void *)FS;
    unsigned char *p;

    if ((fs->flags & 3) == 3 && digits != 0) {
        if (fs->double_size == 0) {
            x <<= 1;
            y <<= 1;
        }
        set_xyz(x, y, -1);
        p = fs->cur;
        p[0] = 0xB;
        p[1] = 0x10;
        p[2] = digits - 1;
        p[3] = value >> 24;
        p[4] = value >> 16;
        p[5] = value >> 8;
        p[6] = value;
        p[7] = 0;
        fs->cur += 8;
    }
}

/*
 * buffer_reset also reaches two fields 0x20 bytes before the queue buffer:
 * a reset flag byte and the queue's free-cursor pointer. The header these
 * belong to is not otherwise evidenced within this allocation, so they are
 * reached by byte offset from the queue symbol instead of a named member
 * (matching src/ov11/res.c's RES_IsDebugMode).
 */
#define FONT_QUEUE_HEADER_OFFSET 0x20
#define FONT_QUEUE_HEADER_RESET_FLAG_OFFSET 0x11
#define FONT_QUEUE_HEADER_CURSOR_OFFSET 0x1C

static void buffer_reset(void)
{
    FontQueueSlot *slot;
    unsigned char *header;
    int remaining;

    remaining = 15;
    slot = D_008811A0;
    do {
        slot->selfOffset = (unsigned char *)slot - (unsigned char *)D_008811A0;
        remaining--;
        slot->link = 0;
        slot++;
    } while (remaining >= 0);

    header = (unsigned char *)D_008811A0 - FONT_QUEUE_HEADER_OFFSET;
    *(FontQueueSlot **)(header + FONT_QUEUE_HEADER_CURSOR_OFFSET) = slot;
    header[FONT_QUEUE_HEADER_RESET_FLAG_OFFSET] = 0;
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
struct FontExtCommand {
    unsigned char opcode;
    unsigned char function_high;
    unsigned char function_middle;
    unsigned char function_low;
    unsigned char argument_high;
    unsigned char argument_mid_high;
    unsigned char argument_mid_low;
    unsigned char argument_low;
    unsigned char terminator;
};

struct FontHexCommand {
    unsigned char opcode;
    unsigned char radix;
    unsigned char digits_minus_one;
    unsigned char value_high;
    unsigned char value_mid_high;
    unsigned char value_mid_low;
    unsigned char value_low;
    unsigned char terminator;
};

struct FontPositionCommand {
    unsigned char opcode;
    unsigned char flags;
    unsigned char x_high;
    unsigned char x_low;
    unsigned char y_high;
    unsigned char y_low;
    unsigned char color_high;
    unsigned char color_middle;
    unsigned char color_low;
};

struct FontCompactPositionCommand {
    unsigned char x_high;
    unsigned char x_low;
    unsigned char y_high;
    unsigned char y_low;
    unsigned char opcode;
    unsigned char color_high;
    unsigned char color_middle;
    unsigned char color_low;
};

union FontPrintArgument {
    unsigned long long integer;
    double real;
    unsigned char *text;
};

struct FontNumericCommand {
    unsigned char opcode;
    unsigned char radix;
    unsigned char digits_minus_one;
    unsigned char value_high;
    unsigned char value_mid_high;
    unsigned char value_mid_low;
    unsigned char value_low;
};

struct FontSpriteCorner {
    unsigned int texture_u_or_x;
    unsigned int texture_v_or_y;
    unsigned int backup_x;
    unsigned int backup_y;
    unsigned int x;
    unsigned int y;
    unsigned int depth;
    unsigned int depth_upper;
};

struct FontSpritePacket {
    unsigned int tag_low;
    unsigned int tag_high;
    unsigned int registers_low;
    unsigned int registers_high;
    unsigned long long texture;
    unsigned char unmodeled_18[8];
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int alpha;
    struct FontSpriteCorner corners[4];
};

struct KanjiClutEnvironment {
    unsigned char unmodeled_00[0xb0];
    unsigned short code_by_character[128];
};

struct FontGsRegisterWrite {
    unsigned long long value;
    unsigned long long register_address;
};

struct FontModePacket {
    unsigned int vif_commands[4];
    unsigned long long gif_tag[2];
    struct FontGsRegisterWrite frame;
    struct FontGsRegisterWrite state_registers[5];
};

struct FontWindowTextureHeader {
    char signature[4];
    unsigned int total_size;
    int subimage_count;
    unsigned int table_offset;
};

struct FontWindowTextureEntry {
    unsigned int width_and_buffer_width;
    unsigned int height_word;
    unsigned int gs_offset;
    unsigned int dma_quadwords;
    unsigned int data_offset;
};

struct FontWindowTransferPacket {
    unsigned int vif_commands[4];
    unsigned long long gif_tag[2];
    struct FontGsRegisterWrite bitbltbuf;
    struct FontGsRegisterWrite trxpos;
    struct FontGsRegisterWrite trxreg;
    struct FontGsRegisterWrite trxdir;
};

typedef struct XglPacket {
    int *cur;
    int unk4;
    int open;
    int *hl;
} XglPacket;

struct FontEffectScratchPacket {
    unsigned long long vif_tag;
    unsigned long long vif_count;
    unsigned long long effect_state;
    unsigned long long payload_count;
};

struct FontDrawCursor {
    XglPacket *packet;
    unsigned short origin_x;
    unsigned char unmodeled_06[2];
    unsigned short x;
    unsigned short y;
    int depth;
    unsigned short offset_x;
    unsigned short offset_y;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char unmodeled_17;
    unsigned char flags;
    unsigned char effect;
    unsigned char attributes;
    unsigned char draw_state_high;
    unsigned char unmodeled_1c[4];
    signed char effect_x;
    signed char effect_y;
    unsigned char unmodeled_22[2];
    unsigned char effect_red;
    unsigned char effect_green;
    unsigned char effect_blue;
    unsigned char unmodeled_27;
    unsigned char primitive_flags;
    unsigned char primitive_mode;
    unsigned char alpha;
    unsigned char unmodeled_2b;
    unsigned short scale_x;
    unsigned short scale_y;
    struct FontEffectScratchPacket effect_packet;
};

struct FontState {
    struct FontImage *font_image;
    unsigned char unmodeled_04[0]; /* +0x4: zero-length alias of the former padding member, kept for saved bodies */
    void *window_image;
    unsigned short flags;
    unsigned char font_selection;
    unsigned char debug_mode;
    unsigned short line_height;
    unsigned short character_spacing;
    unsigned char default_width;
    union {
        unsigned char unmodeled_11; /* former padding name, kept for saved bodies */
        unsigned char queue_reset_flag;
    };
    unsigned char proportional_mode;
    unsigned char horizontal_shift;
    unsigned char unmodeled_14[8];
    unsigned char *queue_cursor;
};

typedef union FontStateStorage {
    struct FontState state;
    unsigned char bytes[0x20];
} FontStateStorage;

extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int size);

static void xglFontFlushSub(int texture_u, int texture_v, int size, int flags)
{
    static signed char tbl_1_004DC2A8[4][2] __asm__("tbl.1") = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}};
    /* FS read through its typed view (the TU declares it as bytes). */
    extern FontStateStorage xglFontStateView __asm__("FS");
    struct FontDrawCursor *cursor = (struct FontDrawCursor *)0x70000000;
    struct FontSpritePacket *sprite = (struct FontSpritePacket *)0x70000100;
    int glyph_width = size >> 8;
    unsigned int glyph_height = size & 0xff;
    int texture_width;
    int texture_height;
    int draw_width;
    int draw_height;
    int effect_x;
    int effect_y;
    int cursor_x = (short)cursor->x;
    int cursor_y = (short)cursor->y;
    int offset_x = (short)cursor->offset_x;
    int offset_y = (short)cursor->offset_y;
    unsigned int left = ((offset_x + cursor_x) * 16) - 8;
    unsigned int top = ((offset_y + cursor_y) * 16) - 9;

    int attributes = flags & 15;
    texture_width = glyph_width * 16;
    texture_height = glyph_height * 16;
    cursor->attributes = attributes;
    if (!xglFontStateView.state.debug_mode && (flags & 16)) {
        draw_width = texture_width * 2;
        draw_height = texture_height * 2;
    } else {
        draw_width = glyph_width * 16;
        draw_height = glyph_height * 16;
    }
    draw_width = (draw_width * (short)cursor->scale_x) >> 4;
    effect_x = cursor->effect_x * 16;
    effect_y = cursor->effect_y * 16;
    draw_height = (draw_height * (short)cursor->scale_y) >> 4;
    cursor->x += draw_width / 16;
    if ((xglFontStateView.state.flags & 8) && (left > 0xf000U || top > 0xf000U)) {
        return;
    }
    {
        unsigned long long texture;

        sprite->tag_low = 0x8001;
        sprite->tag_high = 0x60ab4000;
        sprite->registers_low = 0x535316;
        sprite->registers_high = 0;
        texture = (attributes & 2) ? 0x2a942bc00ULL : 0x2a942b820ULL;
        sprite->texture = texture | ((!xglFontStateView.state.debug_mode) ? 0x2000000400000000ULL : 0x2000000000000000ULL) |
                   (((unsigned long long)((attributes & 1) + 0x3800)) << 37);
        sprite->red = cursor->red;
        sprite->green = cursor->green;
        sprite->blue = cursor->blue;
        sprite->alpha = cursor->alpha;
        sprite->corners[0].texture_u_or_x = texture_u;
        sprite->corners[0].texture_v_or_y = texture_v;
        if (xglFontStateView.state.horizontal_shift) {
            unsigned int shifted_left = left + xglFontStateView.state.horizontal_shift * 16;
            int u_end = texture_u + texture_width;

            sprite->corners[0].y = top;
            sprite->corners[2].texture_u_or_x = texture_u;
            sprite->corners[1].texture_v_or_y = texture_v;
            sprite->tag_low = 0x8001;
            sprite->tag_high = 0xa0aa4000;
            sprite->registers_low = 0x53535316;
            sprite->registers_high = 0x53;
            sprite->corners[0].x = shifted_left;
            sprite->corners[0].depth = cursor->depth;
            sprite->corners[0].depth_upper = 0;
            sprite->corners[1].y = top;
            sprite->corners[1].x = shifted_left + draw_width;
            sprite->corners[1].texture_u_or_x = u_end;
            sprite->corners[1].depth = cursor->depth;
            sprite->corners[1].depth_upper = 0;
            sprite->corners[2].x = left;
            sprite->corners[2].texture_v_or_y = texture_v + texture_height;
            sprite->corners[2].y = top + draw_height;
            sprite->corners[2].depth = cursor->depth;
            sprite->corners[2].depth_upper = 0;
            sprite->corners[3].texture_u_or_x = u_end;
            sprite->corners[3].texture_v_or_y = texture_v + texture_height;
            sprite->corners[3].x = left + draw_width;
            sprite->corners[3].y = top + draw_height;
            sprite->corners[3].depth = cursor->depth;
            sprite->corners[3].depth_upper = 0;
            sceVif1PkAddDirectDataN(cursor->packet, sprite, 11);
            return;
        }
        sprite->corners[0].x = left;
        sprite->corners[0].y = top;
        sprite->corners[0].depth = cursor->depth;
        sprite->corners[0].depth_upper = 0;
        sprite->corners[1].texture_u_or_x = texture_u + texture_width;
        sprite->corners[1].texture_v_or_y = texture_v + texture_height;
        sprite->corners[1].x = left + draw_width;
        sprite->corners[1].y = top + draw_height;
        sprite->corners[1].depth = cursor->depth;
        sprite->corners[1].depth_upper = 0;
        sceVif1PkAddDirectDataN(cursor->packet, sprite, 7);
        sprite->corners[0].depth--;
        sprite->corners[1].depth--;
        if (cursor->effect == 4) {
            unsigned int old_left = sprite->corners[0].x;
            unsigned int old_right = sprite->corners[1].x;
            unsigned int old_top = sprite->corners[0].y;
            unsigned int old_bottom = sprite->corners[1].y;

            sprite->corners[0].x = old_left + 16;
            sprite->corners[2].texture_v_or_y = old_top;
            sprite->corners[2].backup_x = old_right;
            sprite->corners[2].backup_y = old_bottom;
            sprite->corners[0].y = old_top;
            sprite->corners[1].y = old_bottom;
            sprite->corners[1].x = old_right + 16;
            sprite->corners[2].texture_u_or_x = old_left;
            sceVif1PkAddDirectDataN(cursor->packet, sprite, 7);
            sprite->corners[0].x = sprite->corners[2].texture_u_or_x;
            sprite->corners[0].y = sprite->corners[2].texture_v_or_y + 16;
            sprite->corners[1].x = sprite->corners[2].backup_x;
            sprite->corners[1].y = sprite->corners[2].backup_y + 16;
            sceVif1PkAddDirectDataN(cursor->packet, sprite, 7);
        } else if (cursor->effect >= 2) {
            unsigned char effect_red = cursor->effect_red;
            unsigned char effect_green = cursor->effect_green;
            unsigned char effect_blue = cursor->effect_blue;
            unsigned int old_left = sprite->corners[0].x;
            unsigned int old_top = sprite->corners[0].y;
            unsigned int old_right = sprite->corners[1].x;
            unsigned int old_bottom = sprite->corners[1].y;

            sprite->red = effect_red;
            sprite->green = effect_green;
            sprite->blue = effect_blue;
            sprite->corners[2].texture_u_or_x = old_left;
            sprite->corners[2].texture_v_or_y = old_top;
            sprite->corners[2].backup_x = old_right;
            sprite->corners[2].backup_y = old_bottom;
            if (cursor->effect == 2) {
                sprite->corners[0].x = old_left + effect_x;
                sprite->corners[0].y = old_top + effect_y;
                sprite->corners[1].x = old_right + effect_x;
                sprite->corners[1].y = old_bottom + effect_y;
                sceVif1PkAddDirectDataN(cursor->packet, sprite, 7);
            } else {
                int row;
                int column;
                int direction;

                for (row = 16; row <= effect_y; row += 16) {
                    for (column = 16; column <= effect_x; column += 16) {
                        for (direction = 0; direction < 4; direction++) {
                            int offset_x = tbl_1_004DC2A8[direction][0] * column;
                            int offset_y = tbl_1_004DC2A8[direction][1] * row;

                            sprite->corners[0].x = sprite->corners[2].texture_u_or_x + offset_x;
                            sprite->corners[0].y = sprite->corners[2].texture_v_or_y + offset_y;
                            sprite->corners[1].x = sprite->corners[2].backup_x + offset_x;
                            sprite->corners[1].y = sprite->corners[2].backup_y + offset_y;
                            sceVif1PkAddDirectDataN(cursor->packet, sprite, 7);
                        }
                    }
                }
            }
        }
    }
}

/*
 * Draws one glyph from the font texture: u and v are texel coordinates in
 * 1/16 units, size packs the glyph width (high byte) and height (low byte),
 * flags' low nibble selects the draw attributes and bit 4 can double the
 * drawn size.
 */
static void xglFontFlushSub(int u, int v, int size, int flags);

/* Hex digit glyphs are 6x8 texels, 8 texels apart at v 0x2F00 (row 752). */
static void xglFontFlushSubHex(int digit)
{
    xglFontFlushSub(digit << 7, 0x2F00, (6 << 8) | 8, 0x13);
}

static void xglFontFlushSubCRLF(void)
{
    unsigned short *p = (unsigned short *)0x70000000;
    unsigned char *fs = FS;
    int h = *(unsigned short *)(fs + 0xC);
    int y = p[5] + h;
    p[5] = y;
    p[4] = p[2];
    if (fs[0xB] == 0) {
        *(unsigned short *)0x7000000A = y + h;
    }
}




int xglFontCheckProportional(unsigned char c0, unsigned char c1);
unsigned int xglFontGetProportionalSize(int code);
int xglFontGetStringWidth2(const char *text, int arg1);
union FontScalePair {
    struct {
        unsigned short x;
        unsigned short y;
    } axis;
    unsigned int both;
};
struct FontLiteralHexCommand {
    unsigned char opcode;
    unsigned char digit_index;
    unsigned char value_high;
    unsigned char value_mid_high;
    unsigned char value_mid_low;
    unsigned char value_low;
};
struct FontPrimitiveStatePacket {
    unsigned int tag_low, tag_high;
    unsigned int register_low, register_high;
    unsigned int primitive_flags, primitive_mode;
    unsigned int register_address, register_address_high;
};
extern void *sceVif1PkReserve(XglPacket *packet, int words);
extern const unsigned char D_004D2350[96];
struct FontRealDigits {
    signed char value[16];
};

static void xglFontFlushCore(struct FontDrawCursor *cursor, const unsigned char *text)
{
    static unsigned long normal_2[8] __asm__("normal.2") __attribute__((aligned(16))) = {
        0x1000000000008003ULL, 0x000000000000000EULL,
        0x0000000000000000ULL, 0x0000000000000014ULL,
        0x0000000000070000ULL, 0x0000000000000047ULL,
        0x0000000000000000ULL, 0x0000000000000042ULL
    };
    static unsigned long back0_3[4] __asm__("back0.3") __attribute__((aligned(16))) = {
        0x1000000000008001ULL, 0x000000000000000EULL,
        0x000000000007000DULL, 0x0000000000000047ULL
    };
    static unsigned long back1_4[4] __asm__("back1.4") __attribute__((aligned(16))) = {
        0x1000000000008001ULL, 0x000000000000000EULL,
        0x0000000000070000ULL, 0x0000000000000047ULL
    };
    static unsigned long nearest_5[4] __asm__("nearest.5") __attribute__((aligned(16))) = {
        0x1000000000008001ULL, 0x000000000000000EULL,
        0x0000000000000000ULL, 0x0000000000000014ULL
    };
    static unsigned long biliner_6[4] __asm__("biliner.6") __attribute__((aligned(16))) = {
        0x1000000000008001ULL, 0x000000000000000EULL,
        0x0000000000000060ULL, 0x0000000000000014ULL
    };
    /* xglSRand as the original call site saw it (no prototype: an int result). */
    extern int xglSRandWord(void) __asm__("xglSRand");
    /* xglFontAscii2Euc's full-word result (the TU's definition below returns a halfword). */
    extern unsigned int xglFontAscii2EucWord(signed char character, signed char **text) __asm__("xglFontAscii2Euc");
    /* FS read through its typed view (the TU declares it as bytes). */
    extern FontStateStorage xglFontStateView __asm__("FS");
    struct FontRealDigits real_digits;
    signed char integer_digits[32];
    unsigned char kana[96];

    while (*text != 0) {
        unsigned char opcode = *text;
        switch (opcode) {
        case 1: case 2: case 3: {
            unsigned int position_high = (unsigned char)text[0];
            unsigned int position_low = (unsigned char)text[1];
            int position;
            text += 2;
            position = position_low + (position_high << 8) - 0x100;
            cursor->x = position;
            cursor->origin_x = position;
            break;
        }
        case 4: case 5: {
            unsigned int position_high = (unsigned char)text[0];
            unsigned int position_low = (unsigned char)text[1];
            int position;
            text += 2;
            position = position_low + (position_high << 8) - 0x400;
            cursor->y = position;
            break;
        }
        case 6: {
            int depth = ((unsigned char)text[1] << 16) + ((unsigned char)text[2] << 8) + (unsigned char)text[3];
            text += 4;
            cursor->depth = depth;
            break;
        }
        case 7:
            text++;
            break;
        case 8: {
            unsigned char flags = text[1];
            text += 2;
            if (flags & 1) {
                cursor->x = (unsigned char)text[1] + ((unsigned char)text[0] << 8);
                if (!(flags & 0x80)) cursor->origin_x = cursor->x;
                text += 2;
            }
            if (flags & 2) {
                cursor->y = (unsigned char)text[1] + ((unsigned char)text[0] << 8);
                text += 2;
            }
            if (flags & 4) {
                cursor->depth = ((unsigned char)text[0] << 16) + ((unsigned char)text[1] << 8) + (unsigned char)text[2];
                text += 3;
            }
            if (flags & 8) {
                cursor->x += (unsigned char)text[1] + ((unsigned char)text[0] << 8);
                if (!(flags & 0x80)) cursor->origin_x = cursor->x;
                text += 2;
            }
            if (flags & 16) {
                cursor->y += (unsigned char)text[1] + ((unsigned char)text[0] << 8);
                text += 2;
            }
            break;
        }
        case 9:
            cursor->x = (cursor->x + 64) & 0xffc0;
            text++;
            break;
        case 10:
            xglFontFlushSubCRLF();
            text++;
            break;
        case 11:
            cursor->effect = 1;
            cursor->red = cursor->green = cursor->blue = 0x80;
            cursor->primitive_flags = 0;
            cursor->scale_x = 16;
            cursor->alpha = 0x80;
            cursor->scale_y = 16;
            cursor->flags &= 0xf8;
            if (!xglFontStateView.state.debug_mode) sceVif1PkAddDirectDataN(cursor->packet, normal_2, 4);
            xglFontStateView.state.default_width = 4;
            xglFontStateView.state.character_spacing = 0;
            text++;
            break;
        case 12:
            cursor->red = text[1]; cursor->green = text[2]; cursor->blue = text[3];
            text += 4;
            break;
        case 13: {
            cursor->effect = text[1];
            switch (cursor->effect) {
            case 0:
            case 2:
            case 3:
            case 4:
                sceVif1PkAddDirectDataN(cursor->packet, back0_3, 2);
                break;
            case 1:
            default:
                sceVif1PkAddDirectDataN(cursor->packet, back1_4, 2);
                break;
            }
            text += 2;
            break;
        }
        case 14:
            cursor->effect_x = text[1]; cursor->effect_y = text[2];
            cursor->effect_red = text[3]; cursor->effect_green = text[4]; cursor->effect_blue = text[5];
            text += 6;
            break;
        case 15: {
            struct FontPrimitiveStatePacket *packet;
            cursor->primitive_flags = text[1]; cursor->primitive_mode = text[2]; cursor->alpha = text[3];
            packet = sceVif1PkReserve(cursor->packet, 8);
            packet->tag_low = 0x8001; packet->tag_high = 0x10000000;
            packet->register_low = 14; packet->register_high = 0;
            packet->primitive_flags = cursor->primitive_flags;
            packet->primitive_mode = cursor->primitive_mode;
            packet->register_address = 0x42; packet->register_address_high = 0;
            text += 4;
            break;
        }
        case 16: {
            const struct FontLiteralHexCommand *command =
                (const struct FontLiteralHexCommand *)text;
            unsigned int value = ((unsigned int)command->value_high << 24) +
                ((unsigned int)command->value_mid_high << 16) +
                ((unsigned int)command->value_mid_low << 8) + command->value_low;
            int digit = command->digit_index;
            while (digit >= 0) {
                xglFontFlushSubHex((value >> (digit * 4)) & 15);
                digit--;
            }
            text += sizeof(*command);
            break;
        }
        case 17: {
            const struct FontNumericCommand *command =
                (const struct FontNumericCommand *)text;
            if (command->radix == 255) {
                unsigned int mid_high = (unsigned int)command->value_mid_high << 16;
                unsigned int high = (unsigned int)command->value_high << 24;
                unsigned int mid = ((unsigned int)command->value_mid_low << 8) + mid_high;
                unsigned int bits = high + command->value_low + mid;
                unsigned int mantissa = (bits & 0x7fffff) + 0x800000;
                unsigned int shift = 150 - ((bits >> 23) & 255);
                long long integer;
                unsigned long long mask;
                unsigned long long fraction;
                int digit;
                int printed;
                int remaining;
                signed char *fraction_digit;

                if ((int)bits < 0)
                    xglFontFlushSubHex(16);
                else
                    xglFontFlushSubHex(32);
                if (shift >= 32) {
                    integer = 0;
                    mask = (0x100000000ULL << (shift - 32)) - 1;
                } else {
                    integer = mantissa >> shift;
                    mask = (int)((1U << shift) - 1);
                }
                fraction = mantissa & mask;
                for (digit = 7; digit >= 0; digit--) {
                    real_digits.value[digit] = integer % 10;
                    integer /= 10;
                    fraction *= 10;
                    if (shift >= 32)
                        real_digits.value[digit + 8] = ((long long)fraction >> 32) >> (shift - 32);
                    else
                        real_digits.value[digit + 8] = (long long)fraction >> shift;
                    fraction &= mask;
                }
                printed = 0;
                for (digit = 0; digit < 7; digit++) {
                    if (real_digits.value[digit] || printed) {
                        printed++;
                        xglFontFlushSubHex(real_digits.value[digit]);
                    }
                }
                xglFontFlushSubHex(real_digits.value[7]);
                xglFontFlushSubHex(17);
                printed += 2;
                remaining = 7;
                for (; remaining >= 0 && printed < 9; remaining--) {
                    printed++;
                    xglFontFlushSubHex(real_digits.value[remaining + 8]);
                }
            } else {
                int suppress_zero = 0;
                int radix = command->radix;
                int digit = command->digits_minus_one;
                unsigned int value = ((unsigned int)command->value_high << 24) +
                    ((unsigned int)command->value_mid_high << 16) +
                    ((unsigned int)command->value_mid_low << 8) + command->value_low;
                if (radix == 10) {
                    suppress_zero = 1;
                    if ((int)value < 0) { value = -value; suppress_zero = 2; }
                }
                integer_digits[digit + 1] = -1;
                while (digit >= 0) {
                    integer_digits[digit] = value % radix;
                    value /= radix;
                    digit--;
                }
                for (digit = 0; integer_digits[digit] >= 0; digit++) {
                    if (suppress_zero && !integer_digits[digit] && integer_digits[digit + 1] >= 0) {
                        if (suppress_zero == 2 && integer_digits[digit + 1] > 0) xglFontFlushSubHex(16);
                        else xglFontFlushSubHex(32);
                    } else {
                        xglFontFlushSubHex(integer_digits[digit]);
                        suppress_zero = 0;
                    }
                }
            }
            text += sizeof(*command);
            break;
        }
        case 18:
            cursor->scale_x = (unsigned char)text[1]; cursor->scale_y = (unsigned char)text[2];
            if (((union FontScalePair *)&cursor->scale_x)->both == 0x100010)
                sceVif1PkAddDirectDataN(cursor->packet, nearest_5, 2);
            else sceVif1PkAddDirectDataN(cursor->packet, biliner_6, 2);
            text += 3;
            break;
        case 19: {
            struct FontEffectScratchPacket *packet = (struct FontEffectScratchPacket *)
                (((unsigned int)&cursor->effect_packet + 15) & ~15);
            long long horizontal_mode;
            long long horizontal_mask;
            long long vertical_mask;
            long long vertical_mode;
            long long horizontal_random;
            long long vertical_random;
            packet->vif_tag = 0x1000000000008001ULL;
            packet->vif_count = 14;
            if (!(unsigned char)text[1]) {
                horizontal_mode = 1; horizontal_mask = horizontal_random = 0;
            } else {
                unsigned long long mask_byte = text[1];
                horizontal_mode = 3; horizontal_mask = mask_byte ^ 0x3ff;
                if (mask_byte & 0x80) {
                    unsigned int random = xglSRandWord();
                    horizontal_random = (unsigned char)text[1] & random;
                } else {
                    horizontal_random = 0;
                }
            }
            if (!(unsigned char)text[2]) {
                vertical_mode = 1; vertical_mask = vertical_random = 0;
            } else {
                vertical_mode = 3; vertical_mask = (unsigned char)text[1] ^ 0x3ff;
                if (text[2] & 0x80) {
                    unsigned int random = xglSRandWord();
                    vertical_random = (unsigned char)text[1] & random;
                } else {
                    vertical_random = 0;
                }
            }
            packet->payload_count = 8;
            packet->effect_state = horizontal_mode | (vertical_mode << 2) | (horizontal_mask << 4) |
                (horizontal_random << 14) | (vertical_mask << 24) | (vertical_random << 34);
            sceVif1PkAddDirectDataN(cursor->packet, packet, 2);
            text += 3;
            break;
        }
        case 21: {
            const unsigned char *payload = text + 2;
            int length = text[1];
            text += length + 2;
            switch (payload[0]) {
            case 1: if (length == 2) xglFontStateView.state.line_height = payload[1]; break;
            case 2: if (length == 2) xglFontStateView.state.default_width = payload[1]; break;
            case 3: if (length == 3) xglFontStateView.state.character_spacing = (payload[1] << 8) + payload[2]; break;
            case 4: if (length == 2) xglFontStateView.state.proportional_mode = payload[1]; break;
            case 5: if (length == 2) xglFontStateView.state.horizontal_shift = payload[1]; break;
            }
            break;
        }
        case 23: {
            const struct FontExtCommand *command =
                (const struct FontExtCommand *)text;
            void (*callback)(struct FontDrawCursor *, int) = (void (*)(struct FontDrawCursor *, int))
                (((unsigned int)command->function_high << 16) +
                 ((unsigned int)command->function_middle << 8) +
                 command->function_low);
            int argument = ((unsigned int)command->argument_high << 24) +
                ((unsigned int)command->argument_mid_high << 16) +
                ((unsigned int)command->argument_mid_low << 8) +
                command->argument_low;
            text += 8;
            cursor->flags &= 0xbf;
            callback(cursor, argument);
            break;
        }
        case 24: {
            unsigned char flags = cursor->flags & 0xfe;
            unsigned char enable = text[1];
            text += 2;
            if (enable) flags |= 1;
            cursor->flags = flags;
            break;
        }
        case 25: {
            unsigned char bits = text[1];
            unsigned char flags = cursor->flags & 0xf9;
            text += 2;
            if (bits & 1) flags |= 2;
            if (bits & 2) flags |= 4;
            cursor->flags = flags;
            break;
        }
        case 27: {
            signed char character = text[1];
            if (character < 64) xglFontFlushSub((character & 63) << 7, 0x2f00, 0x808, 0x13);
            else xglFontFlushSub((character & 63) << 7, 0x2f80, 0x808, 0x13);
            text += 2;
            break;
        }
        case 20: case 22: case 26:
        case 28: case 29: case 30: case 31:
            break;
        default: {
            signed char sign;
            if ((short)xglFontStateView.state.character_spacing > 0 && opcode == 32 &&
                (short)cursor->x - (short)cursor->origin_x + xglFontGetStringWidth2((const char *)text, 1) > (short)xglFontStateView.state.character_spacing) {
                xglFontFlushSubCRLF();
            } else if ((sign = *text) >= 0 && !(cursor->flags & 4)) {
                if (sign < 64) xglFontFlushSub((sign & 63) << 7, 0x2f00, 0x808, 0x13);
                else xglFontFlushSub((sign & 63) << 7, 0x2f80, 0x808, 0x13);
            } else {
                if ((unsigned char)(*text + 128) < 32) {
                    xglFontFlushSub(0x1f80, 0x2f80, 0x808, 0x13);
                } else {
                    unsigned char high;
                    unsigned int low;
                    int character;
                    if ((signed char)*text >= 0) {
                        unsigned int euc = xglFontAscii2EucWord((signed char)*text, (signed char **)&text);
                        character = euc;
                        high = (euc >> 8) & 255; low = euc & 255;
                    } else {
                        high = *text++;
                        low = *text;
                        character = ((high << 8) + low) & 0xffff;
                    }
                    if ((cursor->flags & 1) &&
                        ((unsigned char)(high + 0x5d) < 3 || character == 0xa1a6 || character == 0xa1bc || character == 0xa1dd)) {
                        if (character == 0xa1a6 || character == 0xa1bc || character == 0xa1dd) {
                            switch (character) {
                            case 0xa1a6:
                                xglFontFlushSub(0x280, 0x2e80, 0x808, 3);
                                break;
                            case 0xa1bc:
                            case 0xa1dd:
                                xglFontFlushSub(0x800, 0x2e80, 0x808, 3);
                                break;
                            }
                        } else {
                            switch ((int)high) {
                            case 0xa3:
                                if (low < 0xc0) xglFontFlushSub((low & 63) << 7, 0x2f00, 0x808, 3);
                                else xglFontFlushSub((low & 63) << 7, 0x2f80, 0x808, 3);
                                break;
                            case 0xa4:
                            case 0xa5: {
                                __builtin_memcpy(kana, D_004D2350, sizeof(kana));
                                low = kana[low - 0xa0];
                                xglFontFlushSub((low & 63) << 7, (0x101 - high) << 7, 0x808, 3);
                                if (low & 64) xglFontFlushSub((low & 128) + 0x1f00, 0x2e80, 0x808, 3);
                                break;
                            }
                            }
                        }
                    } else {
                        int proportional = xglFontCheckProportional(high, low);
                        if ((cursor->flags & 2) && proportional >= 0) {
                            unsigned int bounds = xglFontGetProportionalSize(proportional);
                            int row = proportional >> 5;
                            unsigned int left = bounds & 255;
                            unsigned int right = (bounds >> 8) & 255;
                            if (row >= 2) row = 1 - row;
                            xglFontFlushSub(((proportional & 31) * 20 + left) * 16, row * 384 + 0x2a00,
                                (right - left) * 256 + 0x118, 3);
                        } else {
                            unsigned int glyph = (low + high * 94 - 0xa1) & 0xffff;
                            if (high < 0xa7) glyph = (glyph + 0xc4e2) & 0xffff;
                            else if (high < 0xad) glyph = (glyph + 0xc4be) & 0xffff;
                            else if (high < 0xb0) glyph = (glyph + 0xc30c) & 0xffff;
                            else if (high < 0xd0) glyph = (glyph + 0xc250) & 0xffff;
                            else glyph = (glyph + 0xc240) & 0xffff;
                            low = glyph & 31;
                            high = (glyph >> 5) & 255;
                            xglFontFlushSub(low * 320, (high & 31) * 384, 0x1418, high >> 5);
                        }
                    }
                }
            }
            text++;
            break;
        }
        }
    }
}


int xglFontCheckProportional(unsigned char c0, unsigned char c1)
{
    static char tbl_7[3] __asm__("tbl.7") __attribute__((section(".data"))) = {0xF0, 0x0A, 0x25};
  unsigned short idx;

  if (c0 == 0xA7)
  {
    switch (c1)
    {
    case 0xA1: return 0x40;
    case 0xA3: return 0x41;
    case 0xA6: return 0x42;
    }
    return -1;
  }
  if (c0 == 0xA1)
  {
    if (c1 == c0) return 0x25;
    if (c1 == 0xA5) return 0xA;
  }
  if (c0 == 0xA3)
  {
    return tbl_7[(c1 - 0xA0) >> 5] + (c1 & 0x1F);
  }
  if (c0 >= 0xD0)
  {
    idx = c1 + c0 * 0x5E - 0x4D21;
    c0 = ~(idx >> 5) & 7;
    if (c0 < 2) c0 ^= 1;
    return (c0 << 5) + (idx & 0x1F);
  }
  return -1;
}

struct FontImage {
    unsigned char unmodeled_00000[0x78040];
    unsigned short proportional_widths[256];
};


extern unsigned char FS[];

unsigned int xglFontGetProportionalSize(int code)
{
    struct FontState *state = (struct FontState *)FS;
    struct FontImage *font_image = state->font_image;
    unsigned int width;
    signed char average_width;

    width = (unsigned short)(font_image->proportional_widths[code] + 0xff);
    if ((width & 0xff) == 0xff) {
        width = state->default_width << 8;
    }
    if (state->proportional_mode != 0) {
        average_width = ((int)((width & 0xff) + (width >> 8)) >> 1) - 5;
        if (average_width < 0) {
            average_width = 0;
        }
        if (average_width >= 0x0b) {
            average_width = 0x0a;
        }
        width = (((average_width + 9) << 8) + average_width) & 0xffff;
    }
    return width;
}

static unsigned int hex2val(signed char digit)
{
    unsigned int digit_value;

    digit_value = (digit - '0') & 0xff;
    if (digit_value >= 10U) {
        if ((unsigned int)(digit - 'a') < 6U) {
            return (digit - 'W') & 0xff;
        }
        if ((unsigned int)(digit - 'A') < 6U) {
            return (digit - '7') & 0xff;
        }
        return 0U;
    }
    return digit_value;
}


unsigned short xglFontAscii2Euc(char code, char **textp)
{
    unsigned short ret;
    char *p;

    ret = 0xA1A1;
    if (code >= 0x20) {
        if (code == 0x23 && textp != 0) {
            p = *textp;
            if (p[1] == code) {
                p++;
                ret = *(unsigned short *)(ascii2euc + 6);
            } else {
                ret = (hex2val(p[1]) << 4) + hex2val(p[2]) + 0x20;
                p += 2;
                ret = (((ret / 0x5E) & 0xFF) << 8) + (ret % 0x5E) - 0x2F5F;
            }
            *textp = p;
        } else {
            ret = *(unsigned short *)(KanjiClutEnv + 0xB0 + code * 2);
        }
    }
    return ret;
}

typedef struct FontWinEntry {
    unsigned int pos;
    unsigned int size;
    int addr;
    int count;
    int offset;
} FontWinEntry;
typedef struct FontWinTex {
    int unk0;
    int unk4;
    int num;
    int first;
} FontWinTex;
typedef struct FontCtx {
    XglPacket *pkt;
    unsigned char unk4[0x14];
    unsigned char flags;
    unsigned char mode;
} FontCtx;
extern int LoopCount;
extern void sceVif1PkCloseDirectHLCode(XglPacket *);
extern void sceVif1PkOpenDirectHLCode(XglPacket *, int);
extern void sceVif1PkCnt(XglPacket *, int);
extern void sceVif1PkAddDataN(XglPacket *, void *, int);
extern void sceVif1PkRef(XglPacket *, const void *, int, int, int, int);

void xglFontReloadTexture(FontCtx *ctx, int type)
{
    static unsigned long KanjiTex0Env_8[14] __asm__("KanjiTex0Env.8") __attribute__((aligned(16))) = {
        0x0000000000000000ULL, 0x5100000611000000ULL,
        0x1000000000000005ULL, 0x000000000000000EULL,
        0x0000000000000000ULL, 0x000000000000003FULL,
        0x0005382000000000ULL, 0x0000000000000050ULL,
        0x0000000000000000ULL, 0x0000000000000051ULL,
        0x000000C000000140ULL, 0x0000000000000052ULL,
        0x0000000000000000ULL, 0x0000000000000053ULL
    };
    static unsigned long KanjiTex1Env_9[14] __asm__("KanjiTex1Env.9") __attribute__((aligned(16))) = {
        0x0000000000000000ULL, 0x5100000611000000ULL,
        0x1000000000000005ULL, 0x000000000000000EULL,
        0x0000000000000000ULL, 0x000000000000003FULL,
        0x00053C0000000000ULL, 0x0000000000000050ULL,
        0x0000000000000000ULL, 0x0000000000000051ULL,
        0x000000C000000140ULL, 0x0000000000000052ULL,
        0x0000000000000000ULL, 0x0000000000000053ULL
    };
    static unsigned long WinTexEnv_10[12] __asm__("WinTexEnv.10") __attribute__((aligned(16))) = {
        0x0000000000000000ULL, 0x5100000511000000ULL,
        0x1000000000000004ULL, 0x000000000000000EULL,
        0x0000000000000000ULL, 0x0000000000000050ULL,
        0x0000000000000000ULL, 0x0000000000000051ULL,
        0x0000000000000000ULL, 0x0000000000000052ULL,
        0x0000000000000000ULL, 0x0000000000000053ULL
    };
    static unsigned long TestEnv_11_00490BC0[12] __asm__("TestEnv.11") __attribute__((aligned(16))) = {
        0x0000000000000000ULL, 0x5100000500000000ULL,
        0x1000000000008004ULL, 0x000000000000000EULL,
        0x0000000000000000ULL, 0x000000000000003FULL,
        0x0000000000000000ULL, 0x0000000000000042ULL,
        0x0000000000000000ULL, 0x0000000000000014ULL,
        0x0000000000000005ULL, 0x0000000000000008ULL
    };
    static unsigned long TestEnv_12_00490C20[12] __asm__("TestEnv.12") __attribute__((aligned(16))) = {
        0x0000000000000000ULL, 0x5100000500000000ULL,
        0x1000000000008004ULL, 0x000000000000000EULL,
        0x0000000031000000ULL, 0x000000000000004EULL,
        0x0000000000000000ULL, 0x0000000000000049ULL,
        0x0000000000000000ULL, 0x000000000000004AULL,
        0x000000000007000DULL, 0x0000000000000047ULL
    };
    static unsigned long TestEnv_13[12] __asm__("TestEnv.13") __attribute__((aligned(16))) = {
        0x0000000000000000ULL, 0x5100000500000000ULL,
        0x1000000000008004ULL, 0x000000000000000EULL,
        0x0000000031000000ULL, 0x000000000000004EULL,
        0x0000000000000000ULL, 0x0000000000000049ULL,
        0x0000000000000000ULL, 0x000000000000004AULL,
        0x0000000000070000ULL, 0x0000000000000047ULL
    };
    XglPacket *pkt;
    int *hl;
    unsigned long *env;

    pkt = ctx->pkt;
    ctx->flags |= 0x40;
    if (pkt->open != 0) {
        hl = pkt->hl;
        if (hl != 0) {
            sceVif1PkCloseDirectHLCode(pkt);
            if (*hl == 0x51000000) {
                ctx->pkt->cur -= 4;
            }
        }
    }
    type &= 3;
    if (type == 1) {
        unsigned char *fs;
        sceVif1PkRef(ctx->pkt, KanjiTex0Env_8, 7, 0, 0, 0);
        fs = FS;
        sceVif1PkRef(ctx->pkt, *(char **)(fs + 0), 0x3C02, 0, 0, 0);
        sceVif1PkRef(ctx->pkt, KanjiClutEnv, 0xF, 0, 0, 0);
        sceVif1PkRef(ctx->pkt, KanjiTex1Env_9, 7, 0, 0, 0);
        sceVif1PkRef(ctx->pkt, *(char **)(fs + 0) + 0x3C020, 0x3C02, 0, 0, 0);
    }
    if (type == 2) {
        FontWinTex *wt;
        FontWinEntry *e;
        int i;
        unsigned char *fs = FS;
        wt = ((FontWinTex **)fs)[1];
        e = (FontWinEntry *)((char *)wt + wt->first);
        for (i = 0; i < wt->num; i++) {
            unsigned int pos = e->pos;
            unsigned int hi = pos >> 16;
            unsigned int lo = pos & 0xFFFF;
            WinTexEnv_10[4] = ((unsigned long)((unsigned int)(e->addr + 0xF0000) >> 6) << 32) | ((unsigned long)hi << 48);
            WinTexEnv_10[8] = lo | ((unsigned long)e->size << 32);
            sceVif1PkCnt(ctx->pkt, 0);
            sceVif1PkAddDataN(ctx->pkt, WinTexEnv_10, 0x18);
            sceVif1PkRef(ctx->pkt, (char *)wt + e->offset, e->count, 0, 0, 0);
            e++;
        }
    }
    sceVif1PkRef(ctx->pkt, TestEnv_11_00490BC0, 6, 0, 0, 0);
    if (FS[0xB] != 0) {
        sceVif1PkRef(ctx->pkt, ModeEnv + ((LoopCount & 1) << 4), 8, 0, 0, 0);
    } else {
        switch (ctx->mode) {
        case 0:
        case 2:
        case 3:
        case 4:
            env = TestEnv_12_00490C20;
            break;
        case 1:
        default:
            env = TestEnv_13;
            break;
        }
        sceVif1PkRef(ctx->pkt, env, 6, 0, 0, 0);
    }
    sceVif1PkCnt(ctx->pkt, 0);
    sceVif1PkOpenDirectHLCode(ctx->pkt, 0);
}

struct FontStateView {
    struct FontImage *font_image;
    void *window_image;
    unsigned short flags;
    unsigned char font_selection;
    unsigned char debug_mode;
    unsigned short line_height;
    unsigned short character_spacing;
    unsigned char default_width;
    unsigned char queue_reset_flag;
    unsigned char proportional_mode;
    unsigned char horizontal_shift;
    unsigned char unmodeled_14[8];
    unsigned char *queue_cursor;
};
typedef union FontStateViewStorage {
    struct FontStateView state;
    unsigned char bytes[0x20];
} FontStateViewStorage;
extern FontStateViewStorage FS_view __asm__("FS");
typedef union FontQueueViewStorage {
    unsigned long long alignment;
    FontQueueSlot slots[16];
    unsigned char bytes[0x2860];
} FontQueueViewStorage;
extern FontQueueViewStorage D_queue __asm__("D_008811A0");
struct FontQueueRecord {
    unsigned char next_low;
    unsigned char next_high;
};
void xglFontFlushCore(struct FontDrawCursor *cursor, const unsigned char *text);

void xglFontFlush(void)
{
    static unsigned long zbuf_14[12] __asm__("zbuf.14") __attribute__((aligned(16))) = {
        0x1000000000008005ULL, 0x000000000000000EULL,
        0x0000720000007000ULL, 0x0000000000000018ULL,
        0x01BF000001FF0000ULL, 0x0000000000000040ULL,
        0x0000000031000000ULL, 0x000000000000004EULL,
        0x0000000000070000ULL, 0x0000000000000047ULL,
        0x0000000000080070ULL, 0x000000000000004CULL
    };
    static unsigned long test_15[4] __asm__("test.15") __attribute__((aligned(16))) = {
        0x1000000000008001ULL, 0x000000000000000EULL,
        0x0000000000000000ULL, 0x000000000000003FULL
    };
    /* The font drawing cursor resides at the base of EE scratchpad RAM. */
    struct FontDrawCursor *cursor = (struct FontDrawCursor *)0x70000000;
    FontQueueSlot *slot;
    int remaining;

    *FS_view.state.queue_cursor = 0;
    cursor->depth = -1;
    cursor->blue = 127;
    cursor->green = 127;
    cursor->red = 127;
    if (FS_view.state.debug_mode) {
        cursor->offset_x = 0;
        cursor->offset_y = 0;
    } else {
        cursor->offset_x = 0x700;
        cursor->offset_y = 0x720;
    }
    cursor->flags = 0;
    cursor->draw_state_high = 0;
    cursor->attributes = 255;
    cursor->effect_y = cursor->effect_x = cursor->effect = 1;
    cursor->effect_red = 0;
    cursor->scale_x = 16;
    cursor->effect_green = 0;
    cursor->primitive_flags = 0x44;
    cursor->alpha = 0x80;
    cursor->effect_blue = 0;
    cursor->primitive_mode = 0;
    cursor->scale_y = 16;
    remaining = 15;
    cursor->packet = xglPacketGetCurrent();
    xglFontReloadTexture(cursor, 0x80000001);

    slot = D_queue.slots;
    do {
        unsigned short offset = slot->link;
        if (offset) {
            do {
                struct FontQueueRecord *record =
                    (struct FontQueueRecord *)&D_queue.bytes[offset];
                unsigned char *text = (unsigned char *)(record + 1);
                offset = record->next_low + (record->next_high << 8);
                xglFontFlushCore(cursor, text);
            } while (offset);
        }
        remaining--;
        slot++;
    } while (remaining >= 0);
    if (FS_view.state.debug_mode)
        sceVif1PkAddDirectDataN(cursor->packet, zbuf_14, 6);
    sceVif1PkAddDirectDataN(cursor->packet, test_15, 2);
    sceVif1PkCloseDirectHLCode(cursor->packet);
    buffer_reset();
}

int xglFontGetStringWidth2(const char *text, int arg1)
{
  unsigned char *fs = FS;
  unsigned char new_var;
  unsigned short new_var3;
  int width = 0;
  int flag = 0;
  int first = 1;
  unsigned char new_var2;
  while ((*text) != 0)
  {
    char *q = text;
    unsigned char c = *((unsigned char *) q);
    if (0 == first)
    {
      if ((0 < arg1) && (c == 0x20))
      {
        break;
      }
      if ((arg1 == 2) && (c == 0x2F))
      {
        break;
      }
    }
    if (c == 0xA)
    {
      break;
    }
    if (c < 0x20)
    {
      int size;
      int t;
      switch (c)
      {
        case 0x15:
          switch (q[2])
        {
          case 2:
            fs[0x10] = q[3];
            break;

          case 4:
            fs[0x12] = q[3];
            if ((!c) && (!c))
          {
          }
            break;

        }

          break;

        case 0x18:
          flag = q[1];
          break;

        case 0x19:
          fs[0x11] = q[1];
          break;

      }

      size = xglFontGetSPcodeSize(c, (unsigned char *) text);
      for (; size >= 0; size--)
      {
        text++;
      }

    }
    else
      if ((c < 0xA0) ? ((((signed char) c) >= 0) && (!(fs[0x11] & 2))) : (0))
    {
      width += 0x10;
      text = q + 1;
    }
    else
    {
      unsigned char hi;
      unsigned char lo;
      if (((signed char) c) >= 0)
      {
        unsigned short e = xglFontAscii2Euc((char) c, &text);
        new_var3 = e;
        new_var2 = new_var3 >> 8;
        c = new_var2 & 0xFF;
        hi = c;
        lo = e & 0xFF;
        text = text + 1;
      }
      else
      {
        hi = c;
        lo = q[1];
        text = text + 1;
        text = q + 2;
      }
      ;
      if ((flag != 0) && (((unsigned char) (hi + 0x5D)) < 3))
      {
        width += 8;
      }
      else
      {
        int t2 = xglFontCheckProportional(hi, lo);
        if ((fs[0x11] & 1) && (0 <= t2))
        {
          unsigned int r = xglFontGetProportionalSize(t2);
          width = (width + (((r >> 8) & 0xFF) - (r & 0xFF))) + 1;
        }
        else
        {
          width += 0x14;
        }
      }
    }
    first = 0;
  }

  return width;
}

extern int xglFontGetStringWidth2(const char *text, int);

int xglFontGetStringWidth(const char *text)
{
    return xglFontGetStringWidth2(text, 0);
}

/* The font resource's load address (scaffold-owned real symbol, offset 0:
 * xglFontGetLoadAddress reads its whole first word; the object's further
 * extent is not evidenced within this allocation). */
extern unsigned char FS[];

void *xglFontGetLoadAddress(void)
{
    return *(void **)FS;
}

/* The font module's flags word (scaffold-owned splat name; xglFontGetFlags
 * and xglFontSetFlags are its only accessors in this allocation, and its
 * further extent is not evidenced here). */
extern unsigned char D_00881188[];

unsigned short xglFontGetFlags(void)
{
    return *(unsigned short *)D_00881188;
}

void xglFontSetFlags(int flags)
{
    *(unsigned short *)D_00881188 = flags & 0xFFFD;
}

extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);

int xglFontLoad(int type, int callback)
{
  int old;
  int mode;
  old = FS[10];
  FS[10] = type;
  mode = 1;
  if (callback == 0)
  {
    if (old)
    {
      mode = 0;
      callback = 1;
    }
    else
    {
      mode = 0;
      callback = 1;
    }
  }
  if (type == 0)
  {
    xglCdReadFile("data\\font0.tex", *((void **) FS), mode, callback);
  }
  else
  {
    xglCdReadFile("data\\font1.tex", *((void **) FS), mode, callback);
  }
  return old;
}

extern unsigned char FontImage[];
extern unsigned char WindowImage[];
extern void WindowTexLoad(void *window, int texture_set);
extern void buffer_reset(void);

void xglFontInitial(void)
{
    unsigned char *fs = FS;
    fs[10] = -1;
    *(void **)fs = FontImage;
    xglFontLoad(1, 0);
    *(void **)(fs + 4) = WindowImage;
    WindowTexLoad(WindowImage, 0);
    buffer_reset();
    *(short *)(fs + 8) = 9;
    *(short *)(fs + 12) = 8;
    fs[16] = 4;
    fs[11] = 0;
    fs[18] = 0;
    fs[19] = 0;
    *(short *)(fs + 14) = 0;
}

/* Types recovered upstream (origin/master); kept for reference. */
/* The partial state view shares the complete 0x20-byte retail allocation. */

typedef union FontPrintQueueStorage {
    unsigned long long alignment;
    FontQueueSlot slots[16];
    unsigned char bytes[0x2860];
} FontPrintQueueStorage;

/* xglFontInitial supplies this file-read buffer to xglFontLoad.
 * The recovered FontImage view names its proportional-width table. The
 * complete retail capacity also includes bytes beyond that partial view. */
typedef union FontImageStorage {
    struct FontImage image;
    unsigned char bytes[0x78800];
} FontImageStorage;


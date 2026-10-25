#include "common.h"

extern unsigned char FS[];
extern unsigned long ModeEnv[];

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

extern unsigned char spcode_0[];

unsigned char xglFontGetSPcodeSize(unsigned char code, unsigned char *text)
{
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

INCLUDE_ASM("asm/main/nonmatchings/xgl_font", xglFontFlushSub);

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

INCLUDE_ASM("asm/main/nonmatchings/xgl_font", xglFontFlushCore);

extern char tbl_7[];

int xglFontCheckProportional(unsigned char c0, unsigned char c1)
{
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

extern unsigned char KanjiClutEnv[];
extern unsigned char ascii2euc[];

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
extern unsigned long KanjiTex0Env_8[];
extern unsigned long KanjiTex1Env_9[];
extern unsigned long WinTexEnv_10[];
extern unsigned long TestEnv_11_00490BC0[];
extern unsigned long TestEnv_12_00490C20[];
extern unsigned long TestEnv_13[];
extern void sceVif1PkCloseDirectHLCode(XglPacket *);
extern void sceVif1PkOpenDirectHLCode(XglPacket *, int);
extern void sceVif1PkCnt(XglPacket *, int);
extern void sceVif1PkAddDataN(XglPacket *, void *, int);
extern void sceVif1PkRef(XglPacket *, const void *, int, int, int, int);

void xglFontReloadTexture(FontCtx *ctx, int type)
{
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
extern unsigned char zbuf_14[];
extern unsigned char test_15[];
void xglFontFlushCore(struct FontDrawCursor *cursor, const unsigned char *text);

void xglFontFlush(void)
{
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
extern char D_004D2448[];
extern char D_004D2458[];

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
    xglCdReadFile(D_004D2448, *((void **) FS), mode, callback);
  }
  else
  {
    xglCdReadFile(D_004D2458, *((void **) FS), mode, callback);
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
typedef union FontStateStorage {
    struct FontState state;
    unsigned char bytes[0x20];
} FontStateStorage;

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


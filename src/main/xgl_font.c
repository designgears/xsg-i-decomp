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

INCLUDE_ASM("asm/main/nonmatchings/xgl_font", xglFontGetKanjiClutUV);

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

INCLUDE_ASM("asm/main/nonmatchings/xgl_font", xglFontPrintSub);

INCLUDE_ASM("asm/main/nonmatchings/xgl_font", xglFontPrintDirectCore);

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

INCLUDE_ASM("asm/main/nonmatchings/xgl_font", xglFontDebugPrintf);

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

INCLUDE_ASM("asm/main/nonmatchings/xgl_font", xglFontFlushCore);

INCLUDE_ASM("asm/main/nonmatchings/xgl_font", xglFontCheckProportional);

struct FontImage {
    unsigned char unmodeled_00000[0x78040];
    unsigned short proportional_widths[256];
};

struct FontState {
    struct FontImage *font_image;
    unsigned char unmodeled_04[0x0c];
    unsigned char default_width;
    unsigned char unmodeled_11;
    unsigned char proportional_mode;
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

INCLUDE_ASM("asm/main/nonmatchings/xgl_font", xglFontReloadTexture);

INCLUDE_ASM("asm/main/nonmatchings/xgl_font", xglFontFlush);

INCLUDE_ASM("asm/main/nonmatchings/xgl_font", xglFontGetStringWidth2);

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


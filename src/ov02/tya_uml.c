/*
 * OV02 original TU 12: 0x00a0c488..0x00a0de20 (19 functions)
 */
#include "common.h"
#include "shared.h"

typedef unsigned char u8;
typedef signed short s16;
extern int xglCdGetFileSize(const char *name);
extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
extern u8 *buffer;
extern s16 base_tbl[];
extern char db_fileno_path[];

extern s16 *tbl;
extern void tyaUmlDispInit2(u8 *work_buffer);

typedef struct TyaUmlDispParam {
    int resource_id;
    u8 unmodeled_04[4];
    s16 render_x;
    s16 render_y;
    s16 render_width;
    s16 render_height;
    u64 color;
    s16 clip_x;
    s16 clip_y;
    s16 clip_width;
    s16 clip_height;
    int loaded_resource_id;
    u8 unmodeled_24[4];
    u8 render_pending;
} TyaUmlDispParam;

typedef struct TyaUmlRenderSize {
    u8 unmodeled_00[4];
    s16 width;
    s16 height;
} TyaUmlRenderSize;

typedef struct TyaUmlGsPacket {
    XglPacket *packet;
    u8 unmodeled_04[0x28];
    u64 commands[4];
} TyaUmlGsPacket;

extern TyaUmlRenderSize sRender;
static void tyaUmlDispType3(TyaUmlDispParam *parameter);
extern void tyaUmlDispType3Sub0(void *context, void *argument);
extern void xglFontPrintExtFunc(unsigned int flags,
                                void (*draw)(void *context, void *argument),
                                void *argument);
extern const char D_00A13380[];

/*
 * The parser cursor mark() and tail() read from: only the two columns they
 * use are evidenced, so the rest of the struct stays unmodeled.
 */
typedef struct TyaUmlParser {
    u8 unmodeled_00[0x0A];
    s16 column;                /* 0x0A */
    u8 unmodeled_0c[0x06];
    s16 lineStartColumn;       /* 0x12 */
} TyaUmlParser;

/*
 * The display line mark() and tail() write into: only the fields those two
 * functions touch are evidenced, so the rest of the struct stays unmodeled.
 */
typedef struct TyaUmlDispLine {
    u8 unmodeled_00[0x0A];
    s16 column;                /* 0x0A */
    u8 unmodeled_0c[0x0E];
    s16 baseColumn;            /* 0x1A */
    u8 unmodeled_1c[0x02];
    s16 tailColumn;            /* 0x1E: written by tail() */
    u8 unmodeled_20[0x08];
    u8 markCount;              /* 0x28: number of entries stored in marks[] */
    u8 unmodeled_29[0x03];
    /*
     * 0x2C: boundary columns stored by mark(); bounded by the separate
     * halfword tyaUmlDatabaseMain writes at 0x50 of the same line.
     */
    s16 marks[18];
} TyaUmlDispLine;

static void mark(TyaUmlParser *parser, TyaUmlDispLine *line)
{
    line->marks[line->markCount] = line->baseColumn + (parser->column - (line->column - parser->lineStartColumn));
    line->markCount++;
}

static void tail(TyaUmlParser *parser, TyaUmlDispLine *line)
{
    line->tailColumn = line->baseColumn + (parser->column - (line->column - parser->lineStartColumn));
}

/*
 * One 0x10-byte entry of the eight-slot texture table tyaUmlDispInit2
 * clears and texture_trans transfers through sceVif1PkRef: only the two
 * dimension fields, the environment pointer and the transfer count are
 * evidenced, so the rest of the record stays unmodeled.
 */
typedef struct TyaUmlImage {
    u16 x;              /* 0x00: from the texture table entry */
    u16 y;              /* 0x02 */
    u16 width;          /* 0x04: masked to an even value before use */
    s16 height;         /* 0x06 */
    void *environment;  /* 0x08: cleared by tyaUmlDispInit2 */
    int count;           /* 0x0C: sceVif1PkRef's transfer count */
} TyaUmlImage;

typedef struct TyaUmlJob {
    u8 *src;
    u8 *dst;
    u8 unk08[0xC];
    s16 w;
    s16 h;
    u8 unk18[3];
    u8 flag;
    u8 unk1C[4];
} TyaUmlJob;
typedef struct TyaUmlTexEntry {
    int offset;
    u16 x;
    u16 y;
} TyaUmlTexEntry;
extern char name_0[18];
extern const u8 D_00A13310[];
extern int xglJpegDecode(void *request_address);
extern int xglFontGetSPcodeSize(int code, unsigned char *text);
extern unsigned short xglSRand(void);
extern TyaUmlImage image[8];

static int tyaUmlDispLoad(TyaUmlDispParam *parameter)
{
    int i;
    int j;
    u32 n;
    char *nm;
    u8 *src;
    u8 *dst;
    u8 *text;
    TyaUmlTexEntry *tab;
    int flag20;
    int flag22;
    TyaUmlJob job;

    for (j = 7; j >= 0; j--)
        image[j].environment = 0;

    n = *(u16 *)parameter;
    nm = name_0;
    parameter->render_pending = 0;
    ((u8 *)parameter)[0x29] = 0;
    nm[0xC] = n % 10 + '0';
    n /= 10;
    nm[0xB] = n % 10 + '0';
    n /= 10;
    nm[0xA] = n % 10 + '0';
    n /= 10;
    nm[9] = n % 10 + '0';
    dst = buffer;
    src = buffer + 0x100000 - ((xglCdGetFileSize(nm) + 0x7FF) & ~0x7FF);
    xglCdReadFile(nm, src, 0, 1);
    __builtin_memcpy(dst, src, 0x60);
    text = src + 0x60;
    dst += 0x60;
    flag20 = 0;
    flag22 = 0;
    while (*text != 0) {
        if (*text < 0x20) {
            int len = xglFontGetSPcodeSize(*text, text);
            for (; len >= 0; len--)
                *dst++ = *text++;
        } else if (*text == '@') {
            text++;
            switch (*text++ - '(') {
            case 0:
                if (!(*(u16 *)(SaveData + 0x102E2) & 0x10))
                    flag22 = 1;
                break;
            case 1:
                flag22 = 0;
                break;
            case 20:
                if ((*(u16 *)(SaveData + 0x102E2) & 0x30) != 0x30)
                    flag20 = 1;
                break;
            case 22:
                flag20 = 0;
                break;
            case 2:
                dst[0] = 0x17;
                dst[1] = (u32)mark >> 16;
                dst[2] = (u32)mark >> 8;
                dst[3] = (u32)mark;
                dst[4] = (u32)parameter >> 24;
                dst[5] = (u32)parameter >> 16;
                dst[6] = (u32)parameter >> 8;
                dst[7] = (u32)parameter;
                dst += 8;
                break;
            }
        } else if (flag22 || flag20) {
            u32 r;
            text++;
            dst[0] = 0x13;
            dst[1] = 0x83;
            dst[2] = 0x83;
            dst[3] = 0x57;
            dst[4] = 8;
            dst[5] = 0x88;
            dst[6] = 0xFF;
            dst[7] = 0xF5;
            r = xglSRand();
            dst[9] = 0x13;
            dst[10] = 0;
            dst[11] = 0;
            dst[8] = D_00A13310[r & 3];
            dst += 0xC;
        } else {
            *dst++ = *text++;
        }
    }

    dst[0] = 0x17;
    dst[1] = (u32)tail >> 16;
    dst[2] = (u32)tail >> 8;
    dst[3] = (u32)tail;
    dst[4] = (u32)parameter >> 24;
    dst[5] = (u32)parameter >> 16;
    dst[6] = (u32)parameter >> 8;
    dst[7] = (u32)parameter;
    dst[8] = 0;
    dst = (u8 *)((u32)(dst + 0x18) & ~0xF);
    tab = (TyaUmlTexEntry *)(src + 0x20);
    for (i = 0; i < 8; i++) {
        if (tab[i].offset != 0) {
            int size;
            int len;
            memset(&job, 0, 0x20);
            job.src = src + tab[i].offset;
            job.dst = dst + 0x10;
            job.flag = 1;
            xglJpegDecode(&job);
            size = job.w * job.h * 2;
            image[i].x = tab[i].x;
            ((int *)dst)[1] = 0x08000000;
            len = size / 16;
            ((int *)dst)[2] = 0;
            ((int *)dst)[3] = 0;
            image[i].environment = dst;
            ((int *)dst)[0] = 0x8000 + len;
            dst += size;
            image[i].y = tab[i].y;
            image[i].width = job.w;
            image[i].height = job.h;
            dst = (u8 *)((u32)(dst + 0xF) & ~0xF);
            image[i].count = len + 1;
        }
    }
    parameter->loaded_resource_id = parameter->resource_id;
    *(int *)parameter->unmodeled_24 = dst - buffer;
    return 0;
}

extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data,
                                    int count);
extern void sceVif1PkCloseDirectHLCode(XglPacket *packet);
extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);

/*
 * texture_trans's VIF direct-data scratch packet (GCC names it TestEnv.1);
 * only element 8 (the width/height quadword texture_trans writes before
 * each transfer) is touched here, so the remaining template bytes are
 * supplied by the reference image, not generated here.
 */
extern u64 TestEnv_1_00A108B0[12];

static void texture_trans(XglPacket **packet, TyaUmlImage *texture)
{
    long long dimensions;

    /*
     * Packs the masked width (low half, sign-extended) and height (high
     * half) into the scratch packet's second quadword.
     */
    dimensions = (long long)(s16)(texture->width & 0xFFFE) |
                 ((long long)texture->height << 32);
    TestEnv_1_00A108B0[8] = dimensions;
    sceVif1PkAddDirectDataN(*packet, TestEnv_1_00A108B0, 6);
    sceVif1PkCloseDirectHLCode(*packet);
    sceVif1PkRef(*packet, texture->environment, texture->count, 0,
                 texture->count | 0x51000000, 0);
    sceVif1PkCnt(*packet, 0);
    sceVif1PkOpenDirectHLCode(*packet, 0);
}

extern void xglFontReloadTexture(void *context, int mode);
extern u64 TestEnv_2[];
extern int TestEnv_4[];
extern int InitEnv_3[];
extern int TestEnv_5[];
extern int TestEnv_6[];
extern TyaUmlImage image[8];

static void tyaUmlDispType3Sub0(void *context, void *argument)
{
    TyaUmlImage *img = image;
    TyaUmlDispParam *parameter = argument;
    XglPacket **packet = (XglPacket **)context;
    u8 *hdr;
    int x;
    int y;
    int var_16;
    int var_19;
    int temp_2;
    int temp_3;
    int i;
    int dx;
    int dy;
    int temp_3_3;
    int temp_2_2;
    int temp_11;
    int temp_10_2;
    long long packed;
    TyaUmlImage *entry;
    u16 hu;
    int h;
    int rh;

    x = parameter->render_x - 1792;
    y = parameter->render_y - 1824;
    packed = (long long)x | ((long long)(y + parameter->render_height - 1) << 48);
    packed |= ((long long)y << 32) | ((long long)(x + parameter->render_width - 1) << 16);
    hdr = buffer;
    TestEnv_2[12] = packed;
    sceVif1PkAddDirectDataN(*packet, TestEnv_2, 7);
    if (img->environment) {
        texture_trans(packet, img);
        TestEnv_4[4] = hdr[4];
        TestEnv_4[5] = hdr[5];
        TestEnv_4[6] = hdr[6];
        TestEnv_4[7] = hdr[7];
        TestEnv_4[16] = (s16)img->width * 16;
        TestEnv_4[17] = (s16)img->height * 16;
        TestEnv_4[8] = 0;
        TestEnv_4[9] = 0;
        TestEnv_4[14] = (int)parameter->color;
        TestEnv_4[22] = (int)parameter->color;
        sceVif1PkAddDirectDataN(*packet, InitEnv_3, 2);
        var_19 = -(parameter->clip_y % img->height);
        rh = parameter->render_height;
        hu = img->height;
        if (var_19 < rh) {
            h = hu;
            do {
                temp_3 = (parameter->render_y + var_19) * 16;
                TestEnv_4[21] = temp_3 + (((int)(u16)h << 16) >> 12);
                TestEnv_4[13] = temp_3;
                var_16 = -(parameter->clip_x % (s16)img->width);
                if (var_16 < parameter->render_width) {
                    do {
                        temp_2 = (parameter->render_x + var_16) * 16;
                        TestEnv_4[12] = temp_2;
                        TestEnv_4[20] = temp_2 + (s16)img->width * 16;
                        sceVif1PkAddDirectDataN(*packet, TestEnv_4, 6);
                        var_16 += (s16)img->width;
                    } while (var_16 < parameter->render_width);
                    rh = parameter->render_height;
                    h = (u16)img->height;
                }
                var_19 += (s16)h;
            } while (var_19 < parameter->render_height);
        }
    } else {
        TestEnv_5[4] = hdr[4];
        TestEnv_5[5] = hdr[5];
        TestEnv_5[6] = hdr[6];
        TestEnv_5[7] = hdr[7];
        TestEnv_5[8] = parameter->render_x * 16;
        TestEnv_5[9] = (int)(parameter->render_y * 16);
        TestEnv_5[12] = (parameter->render_x + parameter->render_width) * 16;
        TestEnv_5[13] = (parameter->render_y + parameter->render_height) * 16;
        TestEnv_5[10] = (int)parameter->color;
        TestEnv_5[14] = (int)parameter->color;
        sceVif1PkAddDirectDataN(*packet, TestEnv_5, 4);
    }
    for (i = 1; i <= 7; i++) {
        entry = image + i;
        if (entry->environment) {
            dx = *(s16 *)((char *)entry + 0) - parameter->clip_x;
            temp_10_2 = dx + (s16)entry->width;
            dy = *(s16 *)((char *)entry + 2) - parameter->clip_y;
            temp_11 = dy + (s16)entry->height;
            if (dx <= parameter->render_width && temp_10_2 >= 0 &&
                dy <= parameter->render_height && temp_11 >= 0) {
              {
                texture_trans(packet, entry);
                temp_3_3 = (parameter->render_x - parameter->clip_x + *(s16 *)((char *)entry + 0)) * 16;
                temp_2_2 = (parameter->render_y - parameter->clip_y + *(s16 *)((char *)entry + 2)) * 16;
                TestEnv_6[16] = temp_3_3;
                TestEnv_6[17] = temp_2_2;
                TestEnv_6[18] = (int)(parameter->color + 1);
                TestEnv_6[20] = temp_10_2 = (s16)entry->width * 16;
                TestEnv_6[21] = temp_11 = (s16)entry->height * 16;
                TestEnv_6[24] = temp_3_3 + temp_10_2;
                TestEnv_6[25] = temp_2_2 + temp_11;
                TestEnv_6[26] = (int)(parameter->color + 1);
                sceVif1PkAddDirectDataN(*packet, TestEnv_6, 7);
              }
            }
        }
    }
    xglFontReloadTexture(context, 1);
}

static void tyaUmlDispType3Sub1(void *context, void *argument)
{
    TyaUmlGsPacket *command = context;
    u64 packed_dimensions;
    u64 *command_words;
    s16 width = sRender.width;
    s16 height = sRender.height;

    packed_dimensions = ((u64)(width - 1) << 16) |
                        ((u64)(height - 1) << 48);
    command_words = command->commands;
    command_words[0] = 0x1000000000008001ULL;
    command_words[1] = 14;
    command_words[2] = packed_dimensions;
    command_words[3] = 64;
    sceVif1PkAddDirectDataN(command->packet, command_words, 2);
}

static void tyaUmlDispType3(TyaUmlDispParam *parameter)
{
    int render_x = parameter->render_x - 1792;
    int render_y = parameter->render_y - 1824;
    int x;
    int y;

    parameter->render_pending = 0;
    xglFontPrintExtFunc((unsigned int)(parameter->color + 1),
                        tyaUmlDispType3Sub0, parameter);
    xglFontPrint(0, 0, (int)(parameter->color + 15), D_00A13380);

    x = render_x - parameter->clip_x;
    y = render_y - parameter->clip_y;
    xglFontPrint(x, y, (int)(parameter->color + 15), buffer + 96);
    xglFontPrintExtFunc((unsigned int)(parameter->color + 1),
                        tyaUmlDispType3Sub1, parameter);
}

extern TyaUmlImage image[8];

int tyaUmlDispMain(TyaUmlDispParam *parameter)
{
    if (parameter == 0) return -1;
    if (parameter->resource_id < 0) {
        memset(buffer, 0, 0x64);
        memset(image, 0, 0x80);
        tyaUmlDispType3(parameter);
        return 0;
    }
    if (parameter->resource_id != parameter->loaded_resource_id) {
        return tyaUmlDispLoad(parameter);
    }
    tyaUmlDispType3(parameter);
    return 0;
}

extern int xglFlagsGet(int bit_offset, int bit_count);

static char *find_text(char *p, int id)
{
    int size;

loop:
    size = *(int *)p;
    if (size != 0) {
        if (id == *(int *)(p + 4)) {
            int flag = *(unsigned short *)(p + 0x14);
            p += 0x68;
            if (xglFlagsGet(flag, 1) == 0) p = 0;
            return p;
        } else {
            p = p + size + 4;
            goto loop;
        }
    }
    return 0;
}

static int get_fileno(int target_id, int *file_index)
{
    s16 *entry = tbl;
    int index = 0;

    while (*entry != -2) {
        s16 value = *entry;

        if (value != -1 && value != -3) {
            index++;
            if (value - 9000 == target_id) {
                *file_index = index;
                return value;
            }
        }
        entry++;
    }

    *file_index = -1;
    return -1;
}

static int get_indexno(int target_id)
{
    s16 *entry = base_tbl;
    int index = 0;

    while (*entry != -2) {
        s16 value = *entry;

        if (value == -1) {
            index++;
        }
        if (value != -1 && value != -3) {
            if (value - 9000 == target_id) {
                return index;
            }
        }
        entry++;
    }

    return -1;
}

static int get_selectno(int n)
{
    s16 *entry = base_tbl;
    int index = 0;

    while (*entry != -2) {
        s16 value = *entry;

        if (value == -1) {
            n--;
        }
        if (value != -1 && value != -3) {
            if (n == 0) {
                return index;
            }
            index++;
        }
        entry++;
    }

    return -1;
}

/* Parses db_fileno.txt into the base identifier table. */
static void dbfileno_load(void)
{
    u8 *source = buffer;
    s16 *destination = base_tbl;
    int length;

    length = xglCdGetFileSize(db_fileno_path);
    xglCdReadFile(db_fileno_path, source, 0, 1);
    source[length] = 0;

    while (*source != 0) {
        if ((u8)(*source - '0') < 10) {
            int value = 0;

            do {
                value = value * 10 + *source++ - '0';
            } while ((u8)(*source - '0') < 10);
            *destination = (s16)value;
        } else {
            if (*source != '-')
                goto skip_destination;
            *destination = -1;
        }
        destination++;

    do {
    skip_destination:
        if ((u8)*source == 0)
            goto done;
    } while (*source++ != '\n');
    }

done:
    *destination = -2;
}

extern const char D_00A133B0[];
extern int xglFontGetSPcodeSize(int code, unsigned char *text);
typedef struct DbHeaderWork {
    u8 unmodeled_00[0x52];
    s16 count;
    u8 unmodeled_54[4];
    s16 found;
} DbHeaderWork;

static void dbheader_load(DbHeaderWork *work)
{
  u8 *out = buffer;
  char *text;
  s16 *src = base_tbl;
  char *new_var5;
  u8 **new_var4;
  int found = 0;
  int n = 0;
  s16 *dst;
  u8 *p;
  int new_var3;
  const char *name;
  int new_var2;
  int new_var;
  u8 new_var6;
  s16 *t;
  s16 *tmp;
  signed char c;
  int size;
  s16 v;
  dbfileno_load();
  new_var3 = 0x7FF;
  name = D_00A133B0;
  size = xglCdGetFileSize(name);
  new_var = (size + new_var3) & (~0x7FF);
  t = (s16 *) buffer;
  t = (s16 *) (((char *) t) - new_var);
  text = ((char *) t) + 0x100000;
  xglCdReadFile(name, text, 0, 1);
  tbl = (s16 *) (((char *) t) + 0xFF800);
  dst = tbl;
  memset(out, 0, 0x64);
  memset(image, 0, 0x80);
  out += 0x60;
  out[0] = 8;
  out[3] = 0x18;
  out[1] = 8;
  out[2] = 0;
  new_var2 = 0;
  out += 4;
  for (;;)
  {
    while ((*src) == (-1))
    {
      *(dst++) = *(src++);
    }

    v = *src;
    new_var4 = &out;
    if ((*src) == (-2))
    {
      *dst = v;
      *(*new_var4) = 0;
      work->found = found;
      work->count = n - 1;
      return;
    }
    new_var5 = find_text(text, (long) (*src));
    p = (u8 *) new_var5;
    if (p != 0)
    {
      c = *p;
      *(dst++) = *src;
      while (0 != (*p))
      {
        new_var6 = *p;
        if (new_var6 < 0x20)
        {
          size = xglFontGetSPcodeSize(*p, p);
          if (size >= 0)
          {
            do
            {
              *(out++) = *(p++);
              size--;
            }
            while (size >= new_var2);
          }
        }
        else
        {
          *(out++) = *p;
          p++;
        }
        c = *p;
      }

      found++;
    }
    else
    {
      *dst = -3;
      dst++;
    }
    src++;
    n++;
    *(out++) = '\n';
  }

}

extern char *idx_7;
extern char msg_8[];
extern const char D_00A13400[];
extern const char D_00A13410[];
extern const char D_00A13418[];
typedef struct DispIndexWork {
    u8 unmodeled_00[8];
    s16 x0;
    s16 y0;
    s16 x1;
    s16 y1;
    u64 color;
    u8 unmodeled_18[0x3E];
    s16 cursor;
} DispIndexWork;

static void disp_index(DispIndexWork *work, int value)
{
    int i;
    int color;

    color = (int)(work->color + 4);
    xglFontPrint(work->x0 + work->x1 - 0x824, work->y0 + work->y1 - 0x71C, color, D_00A13400);
    for (i = 0; i < 26; i++) {
        if (i == work->cursor) {
            xglFontPrintDirectOT(color, D_00A13410);
        } else {
            xglFontPrintDirectOT(color, D_00A13418);
        }
        xglFontPrintDirectOT(color, idx_7 + i * 2);
        if (value != 0) {
            msg_8[2] = value >> 8;
            msg_8[3] = value;
            xglFontPrintDirectOT(color, msg_8);
        }
    }
}

int tyaUmlDispLoad(TyaUmlDispParam *);
void WindowDXSet(void *window);
void WindowDXMain(void *window);
void MenuSelectWindow(void *window, void *list);
void eCursolSet(void *cursor, signed char width);
void eCursolMain(void *cursor);
void xglSoundEffectNormalDirect(int effect_id);
extern unsigned char PadData[];
typedef struct TyaPadView {
    unsigned char unmodeled_00[0x2A];
    unsigned short half_2a;
    unsigned char unmodeled_2c[4];
    unsigned short half_30;
    unsigned short half_32;
    unsigned short half_34;
} TyaPadView;
typedef struct TyaDbView {
    unsigned char pad00[0x40];
    void *window;
    void *list;
    void *cursor0;
    void *cursor1;
    s16 state;
    s16 count;
    s16 sel;
    s16 index;
    s16 found;
    s16 file_index;
    s16 top;
    s16 visible;
} TyaDbView;

int tyaUmlDatabaseMain(TyaUmlDispParam *arg0)
{
    TyaDbView *db = (TyaDbView *)arg0;
    int var_5;
    int wrap_c;
    int var_4s;
    u16 var_4;
    u16 var_6;
    int var_5_2;
    int var_5_3;
    u16 var_5_4;
    s16 idx;
    s16 r;
    int temp_8;
    int temp_6;
    int temp_10;
    s16 temp_3_2;
    s16 temp_16;
    int temp_2_6;
    int temp_2_7;
    int temp_16_3;
    int temp_2_2;
    int temp_17;
    s16 temp_2_3;
    int temp_2_4;
    s16 temp_2_5;
    int var_2_2;
    int var_2_3;
    int var_3;
    int temp_3_3;
    int temp_7;
    int var_3_2;
    u16 pad2A;
    int pad32;
    int pad34;
    int file_index;
    int temp_19;
    void *temp_20;
    void *temp_22;
    void *temp_23;
    void *temp_30;

    if (arg0 == 0) {
        return -1;
    }
    temp_20 = *(void **)((char *)arg0 + 0x40);
    temp_22 = *(void **)((char *)arg0 + 0x44);
    temp_23 = *(void **)((char *)arg0 + 0x48);
    temp_30 = *(void **)((char *)arg0 + 0x4C);
    switch (*(s16 *)((char *)arg0 + 0x50)) {
    case 0:
        *(s16 *)((char *)arg0 + 0x54) = 0;
        *(s16 *)((char *)arg0 + 0x56) = 0;
        WindowDXSet(temp_20);
        *(s16 *)((char *)temp_20 + 0x0) = arg0->render_x - 0x700;
        *(int*)((char *)temp_20 + 0x4) = (int)(arg0->color + 1);
        *(s16 *)((char *)temp_20 + 0x2) = arg0->render_y - 0x720;
        *(s16 *)((char *)temp_20 + 0x8) = arg0->render_width;
        {
            u16 render_h = arg0->render_height;

            *(u8 *)((char *)temp_20 + 0x10) = 1;
            *(s16 *)((char *)temp_20 + 0xA) = render_h;
        }
        MenuSelectWindow(temp_20, temp_22);
        WindowDXMain(temp_20);
        *(u8 *)((char *)temp_22 + 0x1A9) = 1;
        *(s16 *)((char *)temp_22 + 0x2) = 0;
        *(int*)((char *)temp_22 + 0x8) = 0;
        eCursolSet(temp_23, 1);
        *(u8 *)((char *)temp_23 + 0x0) = 0x20;
        *(s16 *)((char *)temp_23 + 0x4) = *(u16 *)((char *)temp_20 + 0x0) + *(u16 *)((char *)temp_20 + 0x8) - 0x11;
        *(int*)((char *)temp_23 + 0x8) = (int)(arg0->color + 2);
        *(s16 *)((char *)temp_23 + 0x6) = *(u16 *)((char *)temp_20 + 0x2) + 4;
        eCursolSet(temp_30, 2);
        *(u8 *)((char *)temp_30 + 0x0) = 0x20;
        var_5 = 0x18;
        *(s16 *)((char *)temp_30 + 0x4) = *(u16 *)((char *)temp_20 + 0x0) + *(u16 *)((char *)temp_20 + 0x8) - 0x11;
        *(int*)((char *)temp_30 + 0x8) = (int)(arg0->color + 2);
        *(s16 *)((char *)temp_30 + 0x6) = *(u16 *)((char *)temp_20 + 0x2) + *(u16 *)((char *)temp_20 + 0xA) - 0x13;
        *(s16 *)((char *)arg0 + 0x5C) = 0;
        *(s16 *)((char *)arg0 + 0x5E) = (s16)arg0->render_height / var_5;
        /* fall through */
    case 1:
        dbheader_load((DbHeaderWork *)arg0);
        *(s16 *)((char *)arg0 + 0x50) = 2;
        /* fall through */
    case 2:
        temp_10 = db->sel;
        if (((TyaPadView *)PadData)->half_34 & 0x1000) {
            db->sel--;
        }
        if (((TyaPadView *)PadData)->half_34 & 0x4000) {
            db->sel++;
        }
        while (db->top > 0 && db->sel < db->top + 1) {
            db->top--;
        }
        while (db->top + db->visible <= db->count && db->top + db->visible - 2 < db->sel) {
            db->top++;
        }
        if (db->sel < 0) {
            db->sel = 0;
        }
        if (db->sel > db->count) {
            db->sel = db->count;
        }
        if (db->sel != temp_10) {
            xglSoundEffectNormalDirect(3);
        }
        *(int*)((char *)temp_22 + 0x4) = db->sel - db->top;
        arg0->clip_y = db->top * 0x18;
        db->index = get_indexno(db->sel);
        temp_16 = get_fileno(*(s16 *)((char *)arg0 + 0x54), &file_index);
        *(u16 *)((char *)arg0 + 0x5A) = file_index;
        if (((TyaPadView *)PadData)->half_2a & 0x20) {
            if (temp_16 >= 0) {
                *(u8 *)((char *)temp_20 + 0x10) = 3;
                xglSoundEffectNormalDirect(1);
                arg0->resource_id = temp_16;
                arg0->clip_y = 0;
                tyaUmlDispLoad(arg0);
                *(s16 *)((char *)arg0 + 0x50) = 3;
                goto ret0;
            } else {
                xglSoundEffectNormalDirect(5);
            }
        }
        if (((TyaPadView *)PadData)->half_2a & 0x40) {
            db->index = 0x1A;
            db->state = 4;
            *(u8 *)((char *)temp_20 + 0x10) = 4;
        }
        idx = db->index;
        temp_19 = idx;
        if (((TyaPadView *)PadData)->half_32 & 0xA080) {
            temp_19 = -1;
        }
        temp_17 = 0;
        if (((TyaPadView *)PadData)->half_2a & 0x80) {
            temp_17 = 1;
        }
        if (((TyaPadView *)PadData)->half_32 & 0x2000) {
            temp_17 = 1;
        }
        if (((TyaPadView *)PadData)->half_32 & 0x8000) {
            temp_17 = -1;
        }
        if (((TyaPadView *)PadData)->half_34 & 0x2000) {
            temp_17 = 1;
        }
        if (((TyaPadView *)PadData)->half_34 & 0x8000) {
            temp_17 = -1;
        }
        while (temp_17 != 0) {
            idx = idx + temp_17;
            if (idx < 0) {
                idx = 0x18;
            }
            if (idx >= 0x19) {
                idx = 0;
            }
            r = get_selectno(idx);
            if (r >= 0) {
                db->sel = r;
                break;
            }
        }
        if (temp_19 >= 0) {
            if (temp_17 > 0 && idx < temp_19) {
                idx = temp_19;
                db->sel = get_selectno(idx);
            } else if (temp_17 < 0 && temp_19 < idx) {
                idx = temp_19;
                db->sel = get_selectno(idx);
            }
        }
        if (idx != db->index) {
            xglSoundEffectNormalDirect(3);
        }
        db->index = idx;
        WindowDXMain(temp_20);
        MenuSelectWindow(temp_20, temp_22);
        if (*(signed char *)((char *)temp_20 + 0x10) == 3) {
            if (db->top != 0) {
                eCursolMain(temp_23);
            }
            if (db->top + db->visible - 1 != db->count) {
                eCursolMain(temp_30);
            }
            tyaUmlDispType3(arg0);
        }
        disp_index((DispIndexWork *)arg0, 0);
        return 0;
    case 3:
        temp_3_3 =*(u16 *)((char *)arg0 + 0x1E) - *(u16 *)((char *)arg0 + 0xE);
        temp_16_3 = (s16)temp_3_3;
        if (temp_16_3 < 0) {
            temp_16_3 = 0;
        }
        if (((TyaPadView *)PadData)->half_34 & 0x1000) {
            arg0->clip_y -= 8;
        }
        if (((TyaPadView *)PadData)->half_34 & 0x4000) {
            arg0->clip_y += 8;
        }
        if (arg0->clip_y < 0) {
            arg0->clip_y = 0;
        }
        if (arg0->clip_y > temp_16_3) {
            arg0->clip_y = temp_16_3;
        }
        if (((TyaPadView *)PadData)->half_2a & 0x40) {
            xglSoundEffectNormalDirect(2);
            arg0->resource_id = -1;
            *(s16 *)((char *)arg0 + 0x50) = 1;
        }
        tyaUmlDispMain(arg0);
        if (*(s16 *)((char *)arg0 + 0x1A) != 0) {
            eCursolMain(temp_23);
        }
        if (*(s16 *)((char *)arg0 + 0x1A) != temp_16_3) {
            eCursolMain(temp_30);
        }
        return 0;
    case 4:
    case 5:
    case 6:
        WindowDXMain(temp_20);
        MenuSelectWindow(temp_20, temp_22);
        disp_index((DispIndexWork *)arg0, *(s16 *)((char *)arg0 + 0x50) - 3);
        *(s16 *)((char *)arg0 + 0x50) = *(u16 *)((char *)arg0 + 0x50) + 1;
        return 0;
    default:
        return 1;
    }
ret0:
    return 0;
}

int tyaUmlDispParamReset(TyaUmlDispParam *parameter)
{
    parameter->resource_id = -1;
    parameter->render_x = 0;
    parameter->render_y = 0;
    parameter->render_width = 512;
    parameter->render_height = 448;
    parameter->color = 0x00FFFF00ULL;
    parameter->clip_x = 0;
    parameter->clip_y = 0;
    parameter->clip_width = 512;
    parameter->clip_height = 448;
    parameter->loaded_resource_id = -1;
    return 0;
}

extern TyaUmlImage image[8];

void tyaUmlDispInit2(u8 *work_buffer)
{
    int i;

    buffer = work_buffer;
    for (i = 7; i >= 0; i--)
        image[i].environment = 0;
}

void tyaUmlDispInit(void)
{
    tyaUmlDispInit2((u8 *)0x01000000);
}

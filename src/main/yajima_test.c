#include "common.h"
#include "shared.h"
#include "yajima_test.h"

extern int printf(const char *format, ...);
extern int sceFormat(char *device, char *block_device, void *argument, int argument_size);
extern int sceDevctl(const char *device, int command, const void *input,
                     unsigned int input_size, void *output, unsigned int output_size);
extern int sceOpen(const char *path, int flags, ...);
extern int sceClose(int descriptor);
extern int sceIoctl2(int handle, int command, const void *input, unsigned int input_size,
                     void *output, unsigned int output_size);
extern int sceMount(char *mount_point, char *device, int flags,
                    void *payload, unsigned int payload_length);
extern int sceUmount(const char *path);
extern int sceMkdir(const char *path, int mode);
extern int sceChstat(const char *path, void *status, unsigned int properties);

extern int xglHddMcGetFree(void);
extern int xglHddActivate(int state);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern void xglSleep(void);

/* tya* are defined in main/tu133 (tyaCaptureStart, tyaCaptureEnd),
 * main/tu134 (tyaMenuBgEntry), main/tu135 (tyaElevatorTask), main/tu136
 * (tyaDisplaySetting) and main/tu137 (tyaDrawGauge), none recovered yet;
 * every call in this TU passes literal 0 arguments, so only the argument
 * count is evidenced. */
extern void tyaCaptureStart(int, int);
extern void tyaCaptureEnd(void);
extern void tyaMenuBgEntry(int, int);
extern void tyaElevatorTask(int);
extern void tyaDisplaySetting(int);
extern void tyaDrawGauge(int);

/* "hdd0:" (0x004DA2D0) and "hdd:" (0x004DA2E0): HddTestFormat's record spelled
 * the first one hdd_device_name, which HddTestShutdown's record uses for the
 * second. The merged TU keeps hdd_device for 0x004DA2D0 (canon, HddTest.c).
 */
extern char hdd_device[];
extern char hdd_device_name[];
extern char pfs_device_name[];
extern char hdd_common_device[];
extern char hdd_format_start_message[];
extern char hdd_format_result_format[];
extern char hdd_pfs_format_result_format[];
extern char hdd_shutdown_message[];
extern char hdd_full_path[];
extern char hdd_create_partition_format[];
extern char hdd_partition_size[];
extern char hdd_subcommand_format[];
extern char hdd_close_result_format[];
/* "pfs1:" (0x004DA2F0): HddTestMakeYourSaves' record calls it pfs_mount_point,
 * the mount/unmount records hdd_mount_point; one object, one name here. */
extern char hdd_mount_point[];
extern char hdd_mount_result_format[];
extern char hdd_unmount_result_format[];
extern char hdd_your_saves_directory[];
/* "make YourSaves:%d\n" (0x004C0B70). */
extern char hdd_mkdir_result_format[];
extern char hdd_zone_size_format[];
extern char hdd_zone_free_format[];
extern char hdd_1024_directory_name[];
extern char hdd_chstat_failure_format[];
extern char hdd_dummy_folder_name[];
extern HddTestMenuEntry hdd_test_menu[];
extern char hdd_status_format[];
extern char hdd_status_error_format[];
extern char hdd_free_format[];
extern char hdd_test_title[];
extern char hdd_use_title[];
extern char hdd_no_use_title[];
extern char menu_cursor[];
extern char *name_47[];

/*
 * "%s:%d\n" (0x004DA2F8), the per-directory mkdir report of HddTest1024Save
 * and HddTestDummyFolder. Their records also call it hdd_mkdir_result_format,
 * which in this TU is the 0x004C0B70 string, so it is named apart here. While
 * .sdata stays scaffold-owned the only name the link has for the object is
 * the splat label of its data piece.
 */
extern char D_004DA2F8[];
#define hdd_mkdir_status_format D_004DA2F8

/*
 * PadData (0xd0-byte object at 0x490d90) through HddTest's raw view: it reads
 * the pad state as one doubleword at +40 and halfwords at +42/+44, which no
 * field view of the shared header covers (config/header-canon.json PadData,
 * byte_required src/io/main-00256190/HddTest.c).
 */
extern PadDataRawView PadData;

#define PAD_U64_AT(byte_offset) PadData.doublewords[(byte_offset) / sizeof(u64)]
#define PAD_U16_AT(byte_offset) PadData.halfwords[(byte_offset) / sizeof(u16)]

#define SCE_CST_ATTR 0x0002
#define SCE_STAT_ATTRIBUTE_WORD 1

static void HddTestMountCommon(void);
static void HddTestMakeYourSaves(void);
static void HddTestUnmountCommon(void);

/* xtxdec_sleep (below) is still INCLUDE_ASM; declare it so xtxdec_error does
 * not see an implicit declaration. */
static int xtxdec_sleep(void);

/* xtxdec_sub is still INCLUDE_ASM; declare it here so XtxDecode does not
 * see an implicit declaration. */
static int xtxdec_sub(char *filename);

extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int count);
extern unsigned char TestEnv_0_0036A030[];

/* Only the packet handle at +0x00 is evidenced. */
static void testfunc(XglPacket **packet) {
    sceVif1PkAddDirectDataN(*packet, TestEnv_0_0036A030, 9);
}

extern char D_004C07F0[];
extern char D_004C0808[];
extern char D_004C0828[];
extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
extern int xglCdGetFileSize(const char *path);
extern int sceWrite(int descriptor, void *buffer, unsigned int size);

static void FontTestSub(void)
{
  unsigned char *p;
  unsigned char *q;
  int size;
  int fd;
  unsigned char *base;
  unsigned char c;
  unsigned char c2;
  char head;
  unsigned char *end;
  unsigned char *tbl = (unsigned char *) 0x1000000;
  if (xglCdReadFile(D_004C07F0, (void *) 0x1000000, 0, 0) <= 0)
  {
    return;
  }
  p = (unsigned char *) 0x1000100;
  base = (unsigned char *) 0x1000000;
  size = xglCdGetFileSize(D_004C0808);
  xglCdReadFile(D_004C0808, (void *) 0x1000100, 0, 0);
  ((unsigned char *) 0x1000100)[size] = 0;
  ((unsigned char *) 0x1000101)[size] = 0;
  end = (unsigned char *) 0x1000000;
  while ((*p) != 0)
  {
    head = *base;
    c = *(p++);
    if (c == 10)
    {
      continue;
    }
    c2 = *(p++);
    if (c < 0xD1)
    {
      continue;
    }
    q = base;
    {
      for (; q[0] != 0; q += 2)
      {
        if ((q[0] == c) && (q[1] == c2))
        {
          break;
        }
      }

      if (q[0] != 0)
      {
        continue;
      }
    }
    q[0] = c;
    q[1] = c2;
    q[2] = 0;
    q[3] = 0;
    head = *((unsigned char *) 0x1000000);
  }

  q = end;
  if ((*base) != 0)
  {
    for (q = end; (*q) != 0; q++)
    {
    }

  }
  fd = sceOpen(D_004C0828, 0x602);
  sceWrite(fd, (void *) 0x1000000, q - tbl);
  sceClose(fd);
}

extern void xglFontDebugHex(int x, int y, int value, int digits);
extern void FlushCache(int mode);
extern int xglFontLoad(int font, void (*callback)(int));
extern int rate_2_004D9F60;
extern int dir_3;
extern int rate_8;
extern int dir_9;
extern char D_004C0838[];
extern char D_004C0888[];
extern char D_004C08A0[];
extern char D_004C08B0[];
extern unsigned char test_1[];
extern char test_4[];
extern char test_5[];
extern char test1_6[];
extern char test2_7[];
extern char test_10[];
extern unsigned char txt_11[];
extern unsigned char xy3_12[];
extern unsigned char xy5_13[];
extern unsigned char xy7_14[];
extern unsigned char xy9_15[];
extern char test_16[];
extern char test_17[];
extern char test_18[];
extern char test_19[];
extern char init_20[];
extern char test_21[];
extern char test_22[];
extern char test_23[];
extern char test_24[];
extern char test_25[];

static void FontTestP0(void)
{
    int i;
    int offset;
    unsigned char *table;

    if (PAD_U16_AT(42) & 0x20) {
        xglFontLoad(0, 0);
    }
    if (PAD_U16_AT(42) & 0x40) {
        xglFontLoad(1, 0);
    }
    if (PAD_U16_AT(42) & 0x10) {
        FontTestSub();
    }
    xglFontDebugPrintf(16, 16, D_004C0838);
    xglFontDebugPrintf(16, 32, D_004C0888);
    xglFontDebugPrintf(16, 48, D_004C08A0);
    rate_2_004D9F60 += dir_3;
    if (rate_2_004D9F60 <= 0) {
        dir_3 = 1;
    }
    if (rate_2_004D9F60 >= 128) {
        dir_3 = -1;
    }
    test_1[3] = rate_2_004D9F60;
    test_1[1] = 68;
    test_1[2] = 0;
    FlushCache(0);
    xglFontDebugPrintf(16, 64, (char *)test_1);
    xglFontDebugHex(0, 64, rate_2_004D9F60, 2);
    xglFontDebugPrintf(48, 80, test_4);
    xglFontDebugPrintf(16, 80, test_5);
    xglFontDebugPrintf(20, 96, test1_6);
    xglFontDebugPrintf(16, 99, test2_7);
    xglFontPrint(0, 0, 0xFFFFFF, test_10);
    txt_11[1] = 32;
    txt_11[3] = 0;
    xglFontPrint(0, 0, 0xFFFFFF, (char *)txt_11);
    table = xy3_12;
    for (i = 0, offset = 2; i < table[0]; i++, offset += 2) {
        txt_11[1] = table[offset] + 32;
        txt_11[3] = table[offset + 1] + 32;
        FlushCache(0);
        xglFontPrint(0, 0, 0xFFFFFF, (char *)txt_11);
    }
    table = xy5_13;
    for (i = 0, offset = 2; i < table[0]; i++, offset += 2) {
        txt_11[1] = table[offset] + 32;
        txt_11[3] = table[offset + 1] + 64;
        FlushCache(0);
        xglFontPrint(0, 0, 0xFFFFFF, (char *)txt_11);
    }
    table = xy7_14;
    for (i = 0, offset = 2; i < table[0]; i++, offset += 2) {
        txt_11[1] = table[offset] + 32;
        txt_11[3] = table[offset + 1] + 96;
        FlushCache(0);
        xglFontPrint(0, 0, 0xFFFFFF, (char *)txt_11);
    }
    table = xy9_15;
    for (i = 0, offset = 2; i < table[0]; i++, offset += 2) {
        txt_11[1] = table[offset] + 32;
        txt_11[3] = table[offset + 1] + 128;
        FlushCache(0);
        xglFontPrint(0, 0, 0xFFFFFF, (char *)txt_11);
    }
    rate_8 += dir_9;
    if (rate_8 < 2) {
        dir_9 = 1;
    }
    if (rate_8 >= 127) {
        dir_9 = -1;
    }
    xglFontDebugHex(0, 128, rate_8, 2);
    xglFontDebugPrintf(96, 32, test_16);
    xglFontDebugPrintf(96, 48, test_17);
    xglFontDebugPrintf(96, 64, test_18);
    xglFontDebugPrintf(96, 80, test_19);
    xglFontPrint(0, 0, 0x10000, init_20);
    xglFontPrint(192, 192, 0x10000, D_004C08B0);
    xglFontDebugPrintf(96, 112, test_21);
    xglFontDebugPrintf(96, 128, test_22);
    xglFontDebugPrintf(96, 144, test_23);
    xglFontDebugPrintf(96, 160, test_24);
    xglFontDebugPrintf(96, 176, test_25);
}

typedef signed char s8;
extern char test00_26[51];
extern char test01_27[];
extern char test02_28[];
extern char test03_29[];
extern char test04_30[];
extern char test05_31[];
extern char test06_32[];
extern char test07_33[];

static void FontTestP1(void) {
    u16 temp_3;

    if (PadData.halfwords[0x15] & 0x20) {
        xglFontLoad(0, 0);
    }
    if (PadData.halfwords[0x15] & 0x40) {
        xglFontLoad(1, 0);
    }
    if (PadData.halfwords[0x15] & 0x10) {
        FontTestSub();
    }
    xglFontDebugPrintf(0x10, 0x10, test00_26);
    xglFontDebugPrintf(0x10, 0x1C, test01_27);
    xglFontDebugPrintf(0x10, 0x28, test02_28);
    xglFontDebugPrintf(0x10, 0x34, test03_29);
    xglFontDebugPrintf(0x10, 0x40, test04_30);
    xglFontDebugPrintf(0x10, 0x4C, test05_31);
    xglFontDebugPrintf(0x10, 0x58, test06_32);
    xglFontDebugPrintf(0x10, 0x64, test07_33);
}

extern void nmlModelDirectSend(int mode, u8 *data, int count);

/*
 * TestEnv_34 is a 6-entry, 0x10-byte-stride vertex template (matches its
 * config/symbols/main.txt size 0x60); FontTestLine only touches the x/y of
 * the last two entries (the line's two endpoints), so the rest of each entry
 * and every earlier entry (set elsewhere, still unresolved in this TU) stays
 * unmodeled.
 */
typedef struct TestLineVertex {
    int x;
    int y;
    unsigned char unmodeled_08[8];
} TestLineVertex;

extern TestLineVertex TestEnv_34[6];

static void FontTestLine(int xIndex, int yIndex, int width)
{
  int x0;
  int y;
  int x1;
  x0 = (xIndex * 0x10) + 0x6FF8;
  y = (yIndex * 0x10) + 0x71F7;
  x1 = x0 + (width * 0x10);
  TestEnv_34[5].x = x1;
  TestEnv_34[4].y = y;
  TestEnv_34[5].y = y;
  TestEnv_34[4].x = x0;
  nmlModelDirectSend(1, (u8 *) TestEnv_34, 6);
}

extern char D_004C08D0[];
extern char test0_37[];
extern char test1_38[];
extern char test2_39[];
extern char test3_40[];
extern char test4_41[];
extern int width_35;
extern char D_004D9F78[];
extern char D_004D9F88[];
extern char D_004C08E0[];
extern char test_36[6];
extern void FontTestLine(int xIndex, int yIndex, int width);
extern int xglFontGetStringWidth(const void *data);
extern int xglFontGetProportionalSize(int index);

static void FontTestP2(void) {
    int i;
    unsigned int temp;

    xglFontDebugPrintf(8, 8, D_004D9F78, width_35);
    if (PadData.halfwords[0x4A] & 0x2000) {
        width_35 += 1;
    }
    if (PadData.halfwords[0x4A] & 0x8000) {
        width_35 -= 1;
    }
    xglFontPrint(0x40, 0x60, 0xFFFFFF, D_004C08D0);
    test_36[3] = (signed char) ((int) width_35 >> 8);
    test_36[4] = (unsigned char) width_35;
    xglFontPrintDirect((void *)test_36);
    FontTestLine(0x40, 0x5E, width_35);
    xglFontPrint(0x40, 0x60, 0xFFFFFF, test0_37);
    FontTestLine(0x40, 0x60, xglFontGetStringWidth(test0_37));
    xglFontPrint(0x40, 0x100, 0xFFFFFF, test1_38);
    FontTestLine(0x40, 0x100, xglFontGetStringWidth(test1_38));
    xglFontPrint(0x40, 0x120, 0xFFFFFF, test2_39);
    FontTestLine(0x40, 0x120, xglFontGetStringWidth(test2_39));
    xglFontPrint(0x40, 0x140, 0xFFFFFF, test3_40);
    FontTestLine(0x40, 0x140, xglFontGetStringWidth(test3_40));
    xglFontPrint(0x40, 0x160, 0xFFFFFF, test4_41);
    FontTestLine(0x40, 0x160, xglFontGetStringWidth(test4_41));
    xglFontPrint(0x40, 0x60, 0xFFFFFF, D_004D9F88);
    if (PadData.halfwords[0x15] & 0x20) {
        for (i = 0; i < 0x40; i++) {
            temp = xglFontGetProportionalSize(i);
            printf((char *)D_004C08E0, i, temp);
        }
    }
}

static void FontTest(void)
{
    int page = 0;

    GameSnapShotNumber(1);
    while ((PAD_U64_AT(40) & 0x08000100ULL) != 0x08000100ULL) {
        if (PAD_U16_AT(42) & 0x2000) {
            ++page;
        }
        if (PAD_U16_AT(42) & 0x8000) {
            --page;
        }
        switch (page) {
        default:
            page = 0;
        case 0:
            FontTestP0();
            break;
        case 1:
            FontTestP1();
            break;
        case -1:
            page = 2;
        case 2:
            FontTestP2();
            break;
        }
        GameSnapShotCheck();
        xglSleep();
    }
}

typedef struct UmlTestEnv {
    int pad[12];
    int a0, a1, a2, a3;
    int x0, y0, z0, w0;
    int x1, y1, z1;
} UmlTestEnv;
extern UmlTestEnv TestEnv_42;
typedef struct UmlTestArg {
    char pad[8];
    short x;
    short y;
    short w;
    short h;
    long z;
} UmlTestArg;

static void UmlDispTestSub(XglPacket **packet, UmlTestArg *a)
{
    UmlTestEnv *e = &TestEnv_42;
    int x0, y0, x1, y1;
    long z;

    x1 = (a->x + a->w) * 16;
    y1 = (a->y + a->h) * 16;
    x0 = a->x * 16;
    y0 = a->y * 16;
    z = a->z - 1;
    e->a2 = 0x20;
    e->a3 = 0x80;
    e->x0 = x0;
    e->y0 = y0;
    e->x1 = x1;
    e->y1 = y1;
    e->a0 = 0;
    e->a1 = 0;
    e->z0 = (int)z;
    e->z1 = (int)z;
    sceVif1PkAddDirectDataN(*packet, &TestEnv_42, 6);
}

typedef struct UmlDispParam {
    int file_number;                 /* +0x00 */
    unsigned char unmodeled_04[4];
    short x;                         /* +0x08 */
    short y;                         /* +0x0A */
    short width;                     /* +0x0C */
    short height;                    /* +0x0E */
    unsigned char unmodeled_10[8];
    short scroll_x;                 /* +0x18 */
    short scroll_y;                  /* +0x1A */
    unsigned char unmodeled_1c[8];
    int flags;                       /* +0x24 */
    unsigned char scroll_enable;     /* +0x28 */
    unsigned char scrolling;         /* +0x29 */
    unsigned char unmodeled_2a[2];
    short scroll_target;             /* +0x2C */
} UmlDispParam;
typedef struct SaveDataDebug {
    unsigned char unmodeled_00[0x102E2];
    unsigned short umn_flags;        /* +0x102E2 */
} SaveDataDebug;
typedef struct UmlHeaderRecord {
    int size;                        /* +0x00 */
    int id;                          /* +0x04 */
    unsigned char unmodeled_08[0x60];
    char text[1];                    /* +0x68 */
} UmlHeaderRecord;
extern char D_004C08F0[];
extern char D_004C0900[];
extern char D_004C0918[];
extern char D_004C0928[];
extern char D_004C0938[];
extern char D_004D9F90[];
extern void xglCdLoadOverlay(int overlay);
extern void tyaUmlDispInit2(unsigned char *base);
extern int tyaUmlDispParamReset(void *disp, int mode);
extern void tyaUmlDispMain(void *disp);
extern int xglFontLoad(int font, void (*callback)(int));
extern int xglFontGetSPcodeSize(int code, char *text);

static void UmlDispTest(void)
{
    UmlDispParam param;
    int pad_frame[4];
    SaveDataDebug *save;
    SaveDataDebug *first;
    int detail = 0;
    int number = 0;
    UmlHeaderRecord *record;
    char *text;
    int id;
    int size;
    unsigned char character;
    int delta;
    short step;

    xglCdLoadOverlay(2);
    tyaUmlDispInit2((unsigned char *)0x1000000);
    xglFontLoad(1, 0);
    first = (SaveDataDebug *)SaveData;
    first->umn_flags = 0;
    tyaUmlDispParamReset(&param, 0);
    param.file_number = 0;
    param.x = 1808;
    param.y = 1856;
    param.width = 480;
    param.height = 400;
    xglSleep();
    while ((PAD_U64_AT(40) & 0x08000100ULL) != 0x08000100ULL) {
        save = (SaveDataDebug *)SaveData;
        xglFontDebugPrintf(0, 0, D_004C08F0, number);
        if (detail == 0) {
            if (PAD_U16_AT(42) & 0x10) {
                record = (UmlHeaderRecord *)0x1000000;
                xglCdReadFile(D_004C0900, record, 0, 0);
                size = record->size;
                if (size != 0) {
                    do {
                        id = record->id;
                        text = record->text;
                        while ((character = *text) != 0) {
                            if (character < 32) {
                                text += xglFontGetSPcodeSize(character, text);
                            }
                            text++;
                        }
                        printf(D_004C0918, id, record->text, text + 1);
                        record = (UmlHeaderRecord *)((char *)record + size + 4);
                        size = record->size;
                    } while (size != 0);
                }
            }
            if (PAD_U16_AT(42) & 0x8) {
                save->umn_flags ^= 0x10;
            }
            if (PAD_U16_AT(42) & 0x2) {
                save->umn_flags ^= 0x20;
            }
            xglFontDebugPrintf(16, 40, D_004D9F90, (save->umn_flags >> 4) & 1,
                               (save->umn_flags >> 5) & 1);
            xglFontDebugPrintf(16, 16, D_004C0928, number);
            delta = 0;
            if (PAD_U16_AT(44) & 0x1000) {
                delta = -1;
            }
            if (PAD_U16_AT(44) & 0x8000) {
                delta = -1;
            }
            if (PAD_U16_AT(44) & 0x4000) {
                delta = 1;
            }
            if (PAD_U16_AT(44) & 0x2000) {
                delta = 1;
            }
            if (PAD_U16_AT(40) & 0x80) {
                delta *= 100;
            }
            number = (number + delta + 10000) % 10000;
            if (PAD_U16_AT(42) & 0x20) {
                param.file_number = number;
                detail = 1;
                param.scroll_x = 0;
                param.scroll_y = 0;
            }
        } else {
            if (PAD_U16_AT(44) & 0x1000) {
                param.scroll_y -= 8;
            }
            if (PAD_U16_AT(44) & 0x4000) {
                param.scroll_y += 8;
            }
            if (PAD_U16_AT(44) & 0x8000) {
                param.scroll_x -= 8;
            }
            if (PAD_U16_AT(44) & 0x2000) {
                param.scroll_x += 8;
            }
            if (PAD_U16_AT(42) & 0x40) {
                detail = 0;
            }
            if (param.scroll_enable != 0) {
                if (PAD_U16_AT(42) & 0x80) {
                    param.scrolling = 1;
                }
                if (param.scrolling != 0) {
                    step = param.scroll_target - param.scroll_y;
                    if (step == 0) {
                        param.scrolling = 0;
                    } else {
                        if ((unsigned short)step < 4) {
                            step = 4;
                        }
                        param.scroll_y += step >> 2;
                    }
                }
            }
            xglFontDebugPrintf(0, 8, D_004C0938, param.scroll_x, param.scroll_y,
                               param.file_number, param.flags);
            tyaUmlDispMain(&param);
        }
        xglSleep();
    }
}

extern void xglCdLoadOverlay(int overlay);
extern void tyaUmlDispInit2(unsigned char *base);
extern void WindowTexLoad(unsigned char *base, unsigned int slot);
extern void xglFlagsInitial(void);
extern int tyaUmlDispParamReset(void *disp, int mode);
extern char D_004C0948[];
extern void xglFlagsSet(int index, int a, int b);
extern void xglSoundLoadRequestSmd(const char *name, void *dst);
extern int tyaUmlDatabaseMain(void *disp);
extern void endPrintExtFunc(unsigned int a, int b, int c);
extern char D_004D9F98[];
extern char D_004D9FA0[];
typedef struct DatabaseTestDisp {
    unsigned char unmodeled_00[0x08];
    short x;
    short y;
    short width;
    short height;
    unsigned long color;
    unsigned char unmodeled_18[0x40 - 0x18];
    void *window;
    void *list;
    void *selectCursor;
    void *indexCursor;
    short state;
    unsigned char unmodeled_52[0x58 - 0x52];
    short indexNo;
    short selectNo;
} DatabaseTestDisp;

static void DatabaseTest(void)
{
    unsigned char a0[0x1A0];
    unsigned char a1[0x600];
    unsigned char a2[0x30];
    unsigned char a3[0x30];
    DatabaseTestDisp disp;
    int var;

    var = 0;
    xglCdLoadOverlay(2);
    tyaUmlDispInit2((unsigned char *)0x01000000);
    WindowTexLoad((unsigned char *)0x01100000, 1);
    WindowTexLoad((unsigned char *)0x01200000, 2);
    xglFontLoad(1, 0);
    xglFlagsInitial();
    tyaUmlDispParamReset(&disp, 0);
    disp.window = a0;
    disp.list = a1;
    disp.selectCursor = a2;
    disp.indexCursor = a3;
    disp.x = 0x708;
    disp.y = 0x740;
    disp.width = 0x1F0;
    disp.height = 0x168;
    disp.state = 0;
    do {
        if (PadData.halfwords[0x14] & 8) {
            int temp = var;
            if (xglSRand() & 1 & 0xFFFF) {
                xglFlagsSet(temp, 1, 1);
            }
        } else {
            xglFlagsSet(var, 1, 1);
        }
        var += 1;
    } while (var < 0x1F4);
    xglSoundLoadRequestSmd(D_004D9F98, (void *)0x01000000);
    xglSleep();
    while ((PadData.doublewords[5] & 0x08000100) != 0x08000100) {
        xglFontDebugPrintf(0, 0, D_004C0948);
        if (tyaUmlDatabaseMain(&disp) != 0) {
            break;
        }
        xglFontDebugPrintf(0x50, 0, D_004D9FA0, disp.selectNo, disp.indexNo);
        endPrintExtFunc(0, 0x64, 0);
        xglSleep();
    }
    xglSoundLoadRequestSmd(0, 0);
}

extern char *name_43[];
extern char D_004C0978[];
extern char D_004C0A68[];
extern void GameResourceInit(int resource_bytes, int work_bytes);
extern int xglFontLoad(int font, void (*callback)(int));
extern void xglRenderClearColor(unsigned int color);
extern void xglRenderClearFrame(void);
extern int GameMpeg2Play(char *name, int mode);

static void XenoMovieCheck(void)
{
    char buf[256];
    int var;

    var = 0;
    GameResourceInit(0x01000000, 0x01000000);
    xglFontLoad(0, 0);
    if ((PadData.doublewords[5] & 0x08000100) != 0x08000100) {
        do {
            xglFontDebugPrintf(0, 0, D_004C0978, name_43[var]);
            if (PadData.halfwords[0x16] & 0x1000) {
                var = var - 1;
            }
            if (PadData.halfwords[0x16] & 0x4000) {
                var = var + 1;
            }
            if (var < 0) {
                var = 0x66;
            }
            if (var >= 0x67) {
                var = 0;
            }
            if (PadData.halfwords[0x15] & 0x20) {
                xglRenderClearColor(0x80000000);
                do {
                    xglSleep();
                } while (!(PadData.halfwords[0x15] & 0x20));
                sprintf(buf, D_004C0A68, name_43[var]);
                GameMpeg2Play(buf, 0);
                xglRenderClearFrame();
                do {
                    xglSleep();
                } while (!(PadData.halfwords[0x15] & 0x20));
                xglRenderClearColor(0x80004000);
            }
            xglSleep();
        } while ((PadData.doublewords[5] & 0x08000100) != 0x08000100);
    }
    xglFontLoad(1, 0);
}

static void HddTestFormat(void)
{
    int format_size;
    int status;

    printf(hdd_format_start_message);
    status = sceFormat(hdd_device, 0, 0, 0);
    printf(hdd_format_result_format, status);
    format_size = 0x2000;
    status = sceFormat(pfs_device_name, hdd_common_device, &format_size, 4);
    printf(hdd_pfs_format_result_format, status);
}

static void HddTestShutdown(void)
{
    printf(hdd_shutdown_message);
    sceDevctl(pfs_device_name, 0x5003, 0, 0, 0, 0);
    sceDevctl(hdd_device_name, 0x4806, 0, 0, 0, 0);
}

static void HddTestHddFull(void)
{
    int handle;
    int status;
    int subcommand;

    handle = sceOpen(hdd_full_path, 0x203);
    printf(hdd_create_partition_format, handle);
    for (subcommand = 0;; subcommand++) {
        status = sceIoctl2(handle, 0x6801, hdd_partition_size, 3, 0, 0);
        printf(hdd_subcommand_format, subcommand, status);
        if (status < 0) {
            break;
        }
    }
    status = sceClose(handle);
    printf(hdd_close_result_format, status);
}

static void HddTestMountCommon(void)
{
    int status;

    status = sceMount(hdd_mount_point, hdd_common_device, 4, 0, 0);
    printf(hdd_mount_result_format, status);
}

static void HddTestUnmountCommon(void)
{
    int status;

    status = sceUmount(hdd_mount_point);
    printf(hdd_unmount_result_format, status);
}

static void HddTestMakeYourSaves(void)
{
    int zone_size;
    int zone_free;

    zone_size = sceMkdir(hdd_your_saves_directory, 0x1ff);
    printf(hdd_mkdir_result_format, zone_size);
    zone_size = sceDevctl(hdd_mount_point, 0x5001, 0, 0, 0, 0);
    zone_free = sceDevctl(hdd_mount_point, 0x5002, 0, 0, 0, 0);
    printf(hdd_zone_size_format, zone_size);
    printf(hdd_zone_free_format, zone_free);
}

static void HddTest1024Save(void)
{
    int status;
    int folder_number;
    int tens;
    int hundreds;
    int thousands;
    SceStatRawWords directory_status;

    HddTestMountCommon();
    HddTestMakeYourSaves();
    for (folder_number = 0; folder_number < 1024;) {
        tens = folder_number / 10;
        hdd_1024_directory_name[20] = (folder_number % 10) + '0';
        hundreds = tens / 10;
        hdd_1024_directory_name[19] = (tens % 10) + '0';
        thousands = hundreds / 10;
        hdd_1024_directory_name[18] = (hundreds % 10) + '0';
        hdd_1024_directory_name[17] = (thousands % 10) + '0';
        status = sceMkdir(hdd_1024_directory_name, 0x1ff);
        if ((folder_number++ & 31) == 0) {
            printf(hdd_mkdir_status_format, hdd_1024_directory_name, status);
        }
        if (status < 0) {
            printf(hdd_mkdir_status_format, hdd_1024_directory_name, status);
            break;
        }
        /* Only the 32-bit attribute scalar at byte 4 is caller-evidenced;
           sceChstat copies the complete 64-byte raw record. */
        directory_status[SCE_STAT_ATTRIBUTE_WORD] = 0xc4a7;
        status = sceChstat(hdd_1024_directory_name, directory_status, SCE_CST_ATTR);
        if (status < 0) {
            printf(hdd_chstat_failure_format, status);
            break;
        }
    }
    HddTestUnmountCommon();
}

static void HddTestDummyFolder(void)
{
    int folder_number;
    int tens;
    int hundreds;
    int mkdir_status;

    HddTestMountCommon();
    for (folder_number = 0; folder_number < 256; ++folder_number) {
        tens = folder_number / 10;
        hdd_dummy_folder_name[8] = (folder_number % 10) + '0';
        hundreds = tens / 10;
        hdd_dummy_folder_name[7] = (tens % 10) + '0';
        hdd_dummy_folder_name[6] = (hundreds % 10) + '0';
        mkdir_status = sceMkdir(hdd_dummy_folder_name, 0x1ff);
        printf(hdd_mkdir_status_format, hdd_dummy_folder_name, mkdir_status);
        if (mkdir_status < 0) {
            break;
        }
    }
    HddTestUnmountCommon();
}

extern int sceWrite(int descriptor, void *buffer, unsigned int size);
extern char D_004C0BB8[];
extern char D_004C0BC8[];
extern char D_004C0BD8[];
extern char D_004C0BE8[];
extern char D_004C0BF8[];
extern char D_004C0C08[];
extern char D_004DA300[];

static void HddTestDummySave(void)
{
    int zone_size;
    int chunk_size;
    int remaining;
    int write_size;
    int counter;
    int handle;
    int wrote;
    int mkdir_status;
    int close_status;

    HddTestMountCommon();
    zone_size = sceDevctl(hdd_mount_point, 0x5001, 0, 0, 0, 0);
    remaining = sceDevctl(hdd_mount_point, 0x5002, 0, 0, 0, 0) - 3;
    chunk_size = 0x1000000 / zone_size;
    printf(D_004C0BB8, remaining, zone_size);
    printf(D_004C0BC8, chunk_size);
    mkdir_status = sceMkdir(D_004C0BD8, 0x1ff);
    printf(D_004C0BE8, mkdir_status);
    handle = sceOpen(D_004C0BF8, 0x602, 0x1ff);
    printf(D_004C0C08, handle);

    counter = 0;
    for (;;) {
        if (remaining <= 0) {
            break;
        }
        if (chunk_size < remaining) {
            write_size = chunk_size * zone_size;
            remaining -= chunk_size;
        } else {
            write_size = remaining * zone_size;
            remaining = 0;
        }
        wrote = sceWrite(handle, (void *) 0x1000000, write_size);
        printf(D_004DA300, counter, wrote);
        counter++;
        if (wrote < 0) {
            break;
        }
    }

    close_status = sceClose(handle);
    printf(hdd_close_result_format, close_status);
    HddTestUnmountCommon();
}

static void HddTestMakeYS(void)
{
    HddTestMountCommon();
    HddTestMakeYourSaves();
    HddTestUnmountCommon();
}

static void HddTest(void)
{
    int status;
    int menu_count;
    int selection;
    int row;
    const char *use_title;

    status = sceDevctl(hdd_device, 0x4807, 0, 0, 0, 0);
    printf(hdd_status_format, status);
    if ((unsigned int)status >= 2) {
        printf(hdd_status_error_format, status);
        return;
    }

    for (menu_count = 0; hdd_test_menu[menu_count].label != 0; menu_count++) {
    }
    selection = 0;
    printf(hdd_free_format, xglHddMcGetFree());

    while ((PAD_U64_AT(40) & 0x08000100ULL) != 0x08000100ULL) {
        int item;

        xglFontDebugPrintf(0, 0, hdd_test_title);
        if (PAD_U16_AT(44) & 0x1000) {
            selection--;
        }
        if (PAD_U16_AT(44) & 0x4000) {
            selection++;
        }
        if (selection < 0) {
            selection = menu_count - 1;
        }
        if (selection >= menu_count) {
            selection = 0;
        }
        if ((PAD_U16_AT(42) & 0x20) &&
            hdd_test_menu[selection].action != 0) {
            hdd_test_menu[selection].action();
        }

        if (xglHddActivate(-1)) {
            use_title = hdd_use_title;
        } else {
            use_title = hdd_no_use_title;
        }
        xglFontDebugPrintf(16, 16, use_title);

        row = 32;
        for (item = 0; item < menu_count; item++) {
            if (item == selection) {
                xglFontDebugPrintf(8, row, menu_cursor);
            }
            xglFontDebugPrintf(16, row, hdd_test_menu[item].label);
            row += 8;
        }
        xglSleep();
    }
}

static char *mt_GetPortName(int port)
{
    return name_47[port];
}

extern int xglMcMain(void);
extern unsigned char SaveData[];
extern void xglMcSetMapName(const unsigned char *primary_euc_name, const unsigned char *secondary_euc_name);
extern int xglHddCheck(void);
extern int sceMcSync(int mode, int *cmd, int *result);
extern unsigned char tbl_48[4];
extern unsigned char tbl_49[4];
extern char *mt_GetPortName(int port);
typedef struct SaveDataDebugMc {
    unsigned char unmodeled_00[0x74];
    unsigned char flags[128];
    unsigned char unmodeled_f4[0x102e2 - 0xf4];
    unsigned short umn_flags;
    unsigned char unmodeled_102e4[0x163b0 - 0x102e4];
} SaveDataDebugMc;
#define SAVE_FLAGS (((SaveDataDebugMc *)SaveData)->flags)
typedef struct McRequestParam {
    int file;
    char *name;
    void *data;
    int size;
} McRequestParam;
typedef struct HddMcRequestParam {
    int file;
    void *data;
    int size;
} HddMcRequestParam;
typedef struct CardDataHeader {
    unsigned char unmodeled_00[4];
    int begin;
    int end;
} CardDataHeader;
typedef struct MemcardTestWork {
    int result;
    int check;
    int file;
} MemcardTestWork;
extern int xglMcRequest(int port, int command, McRequestParam *param, int *result);
extern int xglMcEasySave(const char *path, const void *buffer, int size);
extern int sceMcSync(int mode, int *command, int *result);
extern int xglHddMcExist(HddMcRequestParam *request);
extern int xglHddMcLoad(HddMcRequestParam *request);
extern int xglHddMcCreate(HddMcRequestParam *request);
extern int xglHddMcSave(HddMcRequestParam *request);
#define MC_MAP_NAME_ENCEPHALON "\xA5\xA8\xA5\xF3\xA5\xBB\xA5\xD5\xA5\xA7\xA5\xED\xA5\xF3"
#define MC_MAP_NAME_SIMULATOR "\xA5\xB7\xA5\xDF\xA5\xE5\xA5\xEC\xA1\xBC\xA5\xBF\xA1\xBC\xC6\xE2"
#define MC_TEXT_TITLE "\x0B\xA5\xE1\xA5\xE2\xA5\xAB\xA5\xC6\xA5\xB9\xA5\xC8"
#define MC_TEXT_SELECT_PORT "\xA5\xDD\xA1\xBC\xA5\xC8\xC1\xAA\xC2\xF2"
#define MC_TEXT_FORMAT "\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8"
#define MC_TEXT_UNFORMAT "\xA5\xA2\xA5\xF3\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8"
#define MC_TEXT_LOAD_CHECKING \
    "\xA5\xED\xA1\xBC\xA5\xC9\xA1\xA1\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB" \
    "\xB3\xCE\xC7\xA7\xC3\xE6"
#define MC_TEXT_LOAD_NO_FILE \
    "\xA5\xED\xA1\xBC\xA5\xC9\xA1\xA1\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB" \
    "\xA4\xAC\xA4\xA2\xA4\xEA\xA4\xDE\xA4\xBB\xA4\xF3\n\n\xA3\xCF\xA1" \
    "\xA7\xCC\xE1\xA4\xEB"
#define MC_TEXT_FILE_OF_99 "\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB\xC8\xD6\xB9\xE6 %2d/99"
#define MC_TEXT_FILE_ABSENT "\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB\xA4\xCA\xA4\xB7"
#define MC_TEXT_FILE_PRESENT "\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB\xA4\xA2\xA4\xEA"
#define MC_TEXT_LOAD_READING \
    "\xA5\xED\xA1\xBC\xA5\xC9\xA1\xA1\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB" \
    "%d\xC6\xC9\xA4\xDF\xB9\xFE\xA4\xDF\xC3\xE6"
#define MC_TEXT_SAVE_CHECKING_FREE \
    "\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xB6\xF5\xA4\xAD\xCD\xC6\xCE\xCC" \
    "\xA1\xA7\xB3\xCE\xC7\xA7\xC3\xE6"
#define MC_TEXT_SAVE_FREE \
    "\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xB6\xF5\xA4\xAD\xCD\xC6\xCE\xCC" \
    "\xA1\xA7%5dKbyte"
#define MC_TEXT_FILE_NUMBER "\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB\xC8\xD6\xB9\xE6 %2d"
#define MC_TEXT_CREATE_CONFIRM \
    "\xBF\xB7\xB5\xAC\xBA\xEE\xC0\xAE\xA4\xB7\xA4\xDE\xA4\xB9\n\n\xA3" \
    "\xCF\xA1\xA7\xA4\xCF\xA4\xA4\xA1\xA1\xA1\xA1\xA3\xD8\xA1\xA7\xA4" \
    "\xA4\xA4\xA4\xA4\xA8"
#define MC_TEXT_SAVE_CREATING \
    "\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xBF\xB7\xB5\xAC\xBA\xEE\xC0\xAE" \
    "\xC3\xE6"
#define MC_TEXT_SAVE_CHECKING_FILE \
    "\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB" \
    "%d\xB3\xCE\xC7\xA7\xC3\xE6"
#define MC_TEXT_SAVE_FILE "\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB%d"
#define MC_TEXT_OVERWRITE_CONFIRM \
    "\xBE\xE5\xBD\xF1\xA4\xAD\xA4\xB7\xA4\xC6\xA4\xE8\xA4\xED\xA4\xB7" \
    "\xA4\xA4\xA4\xC7\xA4\xB9\xA4\xAB\xA1\xA9\n\n\xA3\xCF\xA1\xA7\xA4" \
    "\xCF\xA4\xA4\xA1\xA1\xA1\xA1\xA3\xD8\xA1\xA7\xA4\xA4\xA4\xA4\xA4" \
    "\xA8"
#define MC_TEXT_SAVE_WRITING \
    "\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB" \
    "%d\xBD\xF1\xA4\xAD\xB9\xFE\xA4\xDF\xC3\xE6"
#define MC_TEXT_FORMAT_CONFIRM \
    "\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8\xA4\xB7\xA4\xC6" \
    "\xA4\xE8\xA4\xED\xA4\xB7\xA4\xA4\xA4\xC7\xA4\xB9\xA4\xAB\xA1\xA9" \
    "\n\n\xA3\xCF\xA1\xA7\xA4\xCF\xA4\xA4\xA1\xA1\xA1\xA1\xA1\xDF\xA1" \
    "\xA7\xA4\xA4\xA4\xA4\xA4\xA8"
#define MC_TEXT_FORMATTING \
    "\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8\xC3\xE6\xA1\xA6" \
    "\xA1\xA6\xA1\xA6"
#define MC_TEXT_FORMAT_DONE \
    "\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8\xA4\xAC\xBD\xAA" \
    "\xCE\xBB\xA4\xB7\xA4\xDE\xA4\xB7\xA4\xBF\n\n\xA3\xCF\xA1\xA7\xCC" \
    "\xE1\xA4\xEB"
#define MC_TEXT_UNFORMAT_CONFIRM \
    "\xA5\xA2\xA5\xF3\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8" \
    "\xA4\xB7\xA4\xC6\xA4\xE8\xA4\xED\xA4\xB7\xA4\xA4\xA4\xC7\xA4\xB9" \
    "\xA4\xAB\xA1\xA9\n\n\xA3\xCF\xA1\xA7\xA4\xCF\xA4\xA4\xA1\xA1\xA1" \
    "\xA1\xA1\xDF\xA1\xA7\xA4\xA4\xA4\xA4\xA4\xA8"
#define MC_TEXT_UNFORMATTING \
    "\xA5\xA2\xA5\xF3\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8" \
    "\xC3\xE6\xA1\xA6\xA1\xA6\xA1\xA6"
#define MC_TEXT_UNFORMAT_DONE \
    "\xA5\xA2\xA5\xF3\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8" \
    "\xA4\xAC\xBD\xAA\xCE\xBB\xA4\xB7\xA4\xDE\xA4\xB7\xA4\xBF\n\n\xA3" \
    "\xCF\xA1\xA7\xCC\xE1\xA4\xEB"
#define MC_TEXT_HDD_LOAD_NO_FILE \
    "\xA3\xC8\xA3\xC4\xA3\xC4\xA5\xED\xA1\xBC\xA5\xC9\xA1\xA1\xA5\xD5" \
    "\xA5\xA1\xA5\xA4\xA5\xEB\xA4\xAC\xA4\xA2\xA4\xEA\xA4\xDE\xA4\xBB" \
    "\xA4\xF3\n\n\xA3\xCF\xA1\xA7\xCC\xE1\xA4\xEB"
#define MC_TEXT_HDD_LOAD "\xA3\xC8\xA3\xC4\xA3\xC4\xA5\xED\xA1\xBC\xA5\xC9"
#define MC_TEXT_HDD_SAVE_FREE \
    "\xA3\xC8\xA3\xC4\xA3\xC4\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xB6\xF5" \
    "\xA4\xAD\xCD\xC6\xCE\xCC\xA1\xA7%5dKbyte"
#define MC_TEXT_LOAD "\xA5\xED\xA1\xBC\xA5\xC9"
#define MC_TEXT_SAVE "\xA5\xBB\xA1\xBC\xA5\xD6"

static void MemcardTest(void)
{
    unsigned short port_status[2];
    MemcardTestWork work;
    McRequestParam list_request;
    McRequestParam load_request;
    McRequestParam check_request;
    McRequestParam create_request;
    char progress[32];
    McRequestParam save_request;
    HddMcRequestParam hdd_list_request;
    HddMcRequestParam hdd_load_request;
    HddMcRequestParam hdd_check_request;
    HddMcRequestParam hdd_create_request;
    HddMcRequestParam hdd_save_request;
    int sync_command;
    int sync_result;
    CardDataHeader *carddata = (CardDataHeader *)0x01002000;
    unsigned char *found = (unsigned char *)0x01000000;
    char *name;
    char *bar;
    unsigned char *flags;
    unsigned char *shown_flags;
    int port;
    int hdd;
    int state;
    int dir;
    int menu_step;
    int save_file_step;
    int hdd_menu_step;
    int hdd_file_step;
    int done;
    int i;

    port = -1;
    hdd = -99;
    state = 0;
    printf("carddata:%08x\n", carddata);
    xglCdReadFile("data\\carddata.new", carddata, 0, 0);
    xglMcSetMapName(MC_MAP_NAME_ENCEPHALON, MC_MAP_NAME_SIMULATOR);
    printf("sizeof(sceMcIconSys):%d\n", 964);
    printf("sizeof(sSaveData):%d\n", 0x163B0);
    printf("sizeof(carddata):%d\n", carddata->end - carddata->begin);

    while ((PAD_U64_AT(40) & 0x08000100ULL) != 0x08000100ULL) {
        xglFontDebugPrintf(0, 0, MC_TEXT_TITLE);
        /* xglMcMain returns both ports' status halfwords as one word. */
        *(unsigned int *)port_status = xglMcMain();
        xglFontDebugPrintf(64, 0, "%4x %4x", port_status[0], port_status[1]);
        if (port >= 0 && port < 2) {
            if (port_status[port] != 1 && port_status[port] != 0xFF) {
                state = 0;
            }
        }
        xglFontDebugPrintf(0, 16, "%2x", state);

        switch (state) {
        case 0x00:
            state = 0x01;
            hdd = xglHddCheck();
            /* fall through */
        case 0x01:
            xglFontDebugPrintf(0, 32, MC_TEXT_SELECT_PORT);
            name = mt_GetPortName(0);
            if (port_status[0] == 1) {
                xglFontDebugPrintf(16, 48, "%s", name);
            } else {
                xglFontDebugPrintf(16, 48, "\x0C   %s", name);
            }
            name = mt_GetPortName(1);
            if (port_status[1] == 1) {
                xglFontDebugPrintf(16, 64, "%s", name);
            } else {
                xglFontDebugPrintf(16, 64, "\x0C   %s", name);
            }
            name = mt_GetPortName(2);
            if (hdd == 0) {
                xglFontDebugPrintf(16, 80, "%s", name);
            } else {
                xglFontDebugPrintf(16, 80, "\x0C   %s", name);
            }
            if (port_status[0] == 1 || port_status[1] == 1 || hdd == 0) {
                dir = 0;
                if (PAD_U16_AT(42) & 0x1000) {
                    dir = -1;
                }
                if (PAD_U16_AT(42) & 0x4000) {
                    dir = 1;
                }
                if (dir != 0) {
                    work.result += dir;
                } else {
                    dir = 1;
                }
                for (;;) {
                    work.result = (work.result + 3) % 3;
                    if ((port_status[0] != 1 && work.result == 0) ||
                        (port_status[1] != 1 && work.result == 1) ||
                        (hdd < 0 && work.result == 2)) {
                        work.result += dir;
                    } else {
                        break;
                    }
                }
            } else {
                work.result = -1;
            }
            if (work.result != -1 && (PAD_U16_AT(42) & 0x20)) {
                if (work.result == 2) {
                    port = 2;
                    state = 0x60;
                } else {
                    port = work.result;
                    state = 0x10;
                }
            }
            xglFontDebugPrintf(8, work.result * 16 + 48, menu_cursor);
            break;

        case 0x10:
            work.result = 0;
            state = 0x11;
            /* fall through */
        case 0x11:
            xglFontDebugPrintf(0, 32, "%s", mt_GetPortName(port));
            xglFontDebugPrintf(16, 48, MC_TEXT_LOAD);
            xglFontDebugPrintf(16, 64, MC_TEXT_SAVE);
            xglFontDebugPrintf(16, 80, MC_TEXT_FORMAT);
            xglFontDebugPrintf(16, 96, MC_TEXT_UNFORMAT);
            menu_step = (PAD_U16_AT(42) & 0x1000) ? -1 : 0;
            if (PAD_U16_AT(42) & 0x4000) {
                menu_step = 1;
            }
            if (menu_step != 0) {
                work.result = (work.result + menu_step + 4) % 4;
            }
            xglFontDebugPrintf(8, work.result * 16 + 48, menu_cursor);
            if (PAD_U16_AT(42) & 0x20) {
                if (PAD_U16_AT(40) & 0x8) {
                    state = 0;
                    sceMcSync(0, &sync_command, &sync_result);
                    printf("%d %d\n", sync_command, sync_result);
                    xglMcEasySave("/BISLPS-99999Dummy", (void *)0x01000000, 0x77D800);
                } else {
                    state = tbl_48[work.result];
                }
            }
            if (PAD_U16_AT(42) & 0x40) {
                state = 0;
            }
            break;

        case 0x20:
            list_request.file = -1;
            list_request.data = found;
            xglMcRequest(port, 2, &list_request, &work.result);
            state = 0x21;
            break;

        case 0x21:
            xglFontDebugPrintf(0, 32, MC_TEXT_LOAD_CHECKING);
            if (port_status[port] != 0xFF) {
                state = 0x22;
                if (work.result >= 0) {
                    work.file = 0;
                    state = 0x23;
                }
            }
            break;

        case 0x22:
            xglFontDebugPrintf(0, 32, MC_TEXT_LOAD_NO_FILE, work.file);
            if (PAD_U16_AT(42) & 0x20) {
                state = 0x10;
            }
            break;

        case 0x23:
            xglFontDebugPrintf(0, 32, MC_TEXT_LOAD);
            dir = 0;
            if (PAD_U16_AT(44) & 0x8000) {
                dir = -1;
            }
            if (PAD_U16_AT(44) & 0x2000) {
                dir = 1;
            }
            if (dir != 0) {
                work.file = (work.file + dir + 100) % 100;
            }
            xglFontDebugPrintf(16, 48, MC_TEXT_FILE_OF_99, work.file);
            if (found[work.file] == 0) {
                xglFontDebugPrintf(16, 64, MC_TEXT_FILE_ABSENT);
            } else {
                xglFontDebugPrintf(16, 64, MC_TEXT_FILE_PRESENT);
            }
            if ((PAD_U16_AT(42) & 0x20) && found[work.file] != 0) {
                load_request.file = work.file;
                load_request.data = SaveData;
                load_request.size = 0x163B0;
                state = 0x24;
                xglMcRequest(port, 4, &load_request, &work.result);
            }
            if (PAD_U16_AT(42) & 0x40) {
                state = 0x10;
            }
            break;

        case 0x24:
            xglFontDebugPrintf(0, 32, MC_TEXT_LOAD_READING, work.file);
            if (port_status[port] != 0xFF) {
                state = 0x10;
            }
            break;

        case 0x30:
            xglMcRequest(port, 1, 0, &work.result);
            state = 0x31;
            work.check = 0;
            break;

        case 0x31:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_CHECKING_FREE);
            if (port_status[port] != 0xFF) {
                work.file = 0;
                state = 0x32;
            }
            break;

        case 0x32:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_FREE, work.result);
            save_file_step = (PAD_U16_AT(44) & 0x8000) ? -1 : 0;
            if (PAD_U16_AT(44) & 0x2000) {
                save_file_step = 1;
            }
            if (save_file_step != 0) {
                work.file = (work.file + save_file_step + 100) % 100;
            }
            xglFontDebugPrintf(16, 48, MC_TEXT_FILE_NUMBER, work.file);
            if (PAD_U16_AT(42) & 0x20) {
                check_request.file = work.file;
                xglMcRequest(port, 2, &check_request, &work.check);
                state = 0x35;
            }
            if (PAD_U16_AT(42) & 0x40) {
                state = 0x10;
            }
            break;

        case 0x33:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_FREE, work.result);
            xglFontDebugPrintf(16, 48, MC_TEXT_CREATE_CONFIRM);
            if (PAD_U16_AT(42) & 0x20) {
                create_request.file = work.file;
                create_request.data = carddata;
                xglMcRequest(port, 8, &create_request, &work.result);
                state = 0x34;
            }
            if (PAD_U16_AT(42) & 0x40) {
                state = 0x10;
            }
            break;

        case 0x34:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_CREATING);
            bar = progress;
            for (done = 0; done < work.result; done++) {
                *bar++ = '*';
            }
            for (; done < 14; done++) {
                *bar++ = '.';
            }
            *bar = '\0';
            xglFontDebugPrintf(16, 48, progress);
            if (port_status[port] != 0xFF) {
                state = 0x37;
            }
            break;

        case 0x35:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_CHECKING_FILE, work.file);
            if (port_status[port] != 0xFF) {
                state = 0x33;
            }
            break;

        case 0x36:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_FILE, work.file);
            xglFontDebugPrintf(16, 48, MC_TEXT_OVERWRITE_CONFIRM);
            if (PAD_U16_AT(42) & 0x20) {
                state = 0x37;
            }
            if (PAD_U16_AT(42) & 0x40) {
                state = 0x10;
            }
            break;

        case 0x37:
            save_request.file = work.file;
            save_request.data = SaveData;
            save_request.size = 0x163B0;
            state = 0x38;
            xglMcRequest(port, 6, &save_request, &work.result);
            break;

        case 0x38:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_WRITING, work.file);
            if (port_status[port] != 0xFF) {
                state = 0x10;
            }
            break;

        case 0x40:
            state = 0x41;
            /* fall through */
        case 0x41:
            xglFontDebugPrintf(0, 32, MC_TEXT_FORMAT_CONFIRM);
            if (PAD_U16_AT(42) & 0x20) {
                xglMcRequest(port, 9, 0, &work.result);
                state = 0x42;
            }
            if (PAD_U16_AT(42) & 0x40) {
                state = 0x10;
            }
            break;

        case 0x42:
            xglFontDebugPrintf(0, 32, MC_TEXT_FORMATTING);
            if (port_status[port] != 0xFF) {
                state = 0x43;
            }
            break;

        case 0x43:
            xglFontDebugPrintf(0, 32, MC_TEXT_FORMAT_DONE);
            if (PAD_U16_AT(42) & 0x20) {
                state = 0x10;
            }
            break;

        case 0x50:
            state = 0x51;
            /* fall through */
        case 0x51:
            xglFontDebugPrintf(0, 32, MC_TEXT_UNFORMAT_CONFIRM);
            if (PAD_U16_AT(42) & 0x20) {
                xglMcRequest(port, 10, 0, &work.result);
                state = 0x52;
            }
            if (PAD_U16_AT(42) & 0x40) {
                state = 0x10;
            }
            break;

        case 0x52:
            xglFontDebugPrintf(0, 32, MC_TEXT_UNFORMATTING);
            if (port_status[port] != 0xFF) {
                state = 0x53;
            }
            break;

        case 0x53:
            xglFontDebugPrintf(0, 32, MC_TEXT_UNFORMAT_DONE);
            if (PAD_U16_AT(42) & 0x20) {
                state = 0x10;
            }
            break;

        case 0x60:
            work.result = 0;
            state = 0x61;
            /* fall through */
        case 0x61:
            xglFontDebugPrintf(0, 32, "%s", mt_GetPortName(port));
            xglFontDebugPrintf(16, 48, MC_TEXT_LOAD);
            xglFontDebugPrintf(16, 64, MC_TEXT_SAVE);
            hdd_menu_step = (PAD_U16_AT(42) & 0x1000) ? -1 : 0;
            if (PAD_U16_AT(42) & 0x4000) {
                hdd_menu_step = 1;
            }
            if (hdd_menu_step != 0) {
                work.result = (work.result + hdd_menu_step + 2) % 2;
            }
            xglFontDebugPrintf(8, work.result * 16 + 48, menu_cursor);
            if (PAD_U16_AT(42) & 0x20) {
                state = tbl_49[work.result];
            }
            if (PAD_U16_AT(42) & 0x40) {
                state = 0;
            }
            break;

        case 0x70:
            hdd_list_request.file = -1;
            hdd_list_request.data = found;
            if (xglHddMcExist(&hdd_list_request) < 0) {
                state = 0x71;
            } else {
                state = 0x72;
            }
            break;

        case 0x71:
            xglFontDebugPrintf(0, 32, MC_TEXT_HDD_LOAD_NO_FILE, work.file);
            if (PAD_U16_AT(42) & 0x20) {
                state = 0x60;
            }
            break;

        case 0x72:
            xglFontDebugPrintf(0, 32, MC_TEXT_HDD_LOAD);
            dir = 0;
            if (PAD_U16_AT(44) & 0x8000) {
                dir = -1;
            }
            if (PAD_U16_AT(44) & 0x2000) {
                dir = 1;
            }
            if (dir != 0) {
                work.file = (work.file + dir + 100) % 100;
            }
            xglFontDebugPrintf(16, 48, MC_TEXT_FILE_OF_99, work.file);
            if (found[work.file] == 0) {
                xglFontDebugPrintf(16, 64, MC_TEXT_FILE_ABSENT);
            } else {
                xglFontDebugPrintf(16, 64, MC_TEXT_FILE_PRESENT);
            }
            if (PAD_U16_AT(42) & 0x20) {
                if (found[work.file] != 0) {
                    hdd_load_request.file = work.file;
                    hdd_load_request.data = SaveData;
                    hdd_load_request.size = 0x163B0;
                    printf("xglHddMcLoad():%d\n", xglHddMcLoad(&hdd_load_request));
                    state = 0x60;
                }
            }
            if (PAD_U16_AT(42) & 0x40) {
                state = 0x60;
            }
            break;

        case 0x80:
            work.result = 0;
            state = 0x81;
            /* fall through */
        case 0x81:
            xglFontDebugPrintf(0, 32, MC_TEXT_HDD_SAVE_FREE, work.result);
            hdd_file_step = (PAD_U16_AT(42) & 0x8000) ? -1 : 0;
            if (PAD_U16_AT(42) & 0x2000) {
                hdd_file_step = 1;
            }
            if (hdd_file_step != 0) {
                work.file = (work.file + hdd_file_step + 100) % 100;
            }
            xglFontDebugPrintf(16, 48, MC_TEXT_FILE_NUMBER, work.file);
            if (PAD_U16_AT(42) & 0x20) {
                int exist;

                hdd_check_request.file = work.file;
                state = 0x82;
                exist = xglHddMcExist(&hdd_check_request);
                printf("xglHddMcExist():%d\n", exist);
                if (exist < 0) {
                    state = 0x60;
                }
            }
            if (PAD_U16_AT(42) & 0x40) {
                state = 0x60;
            }
            break;

        case 0x82:
            xglFontDebugPrintf(0, 32, MC_TEXT_HDD_SAVE_FREE, work.result);
            xglFontDebugPrintf(16, 48, MC_TEXT_CREATE_CONFIRM);
            if (PAD_U16_AT(42) & 0x20) {
                hdd_create_request.file = work.file;
                hdd_create_request.data = carddata;
                hdd_create_request.size = 0x163B0;
                printf("xglHddMcCreate():%d\n", xglHddMcCreate(&hdd_create_request));
                state = 0x84;
            }
            if (PAD_U16_AT(42) & 0x40) {
                state = 0x60;
            }
            break;

        case 0x83:
            xglFontDebugPrintf(0, 32, MC_TEXT_HDD_SAVE_FREE, work.result);
            xglFontDebugPrintf(16, 48, MC_TEXT_OVERWRITE_CONFIRM);
            if (PAD_U16_AT(42) & 0x20) {
                state = 0x84;
            }
            if (PAD_U16_AT(42) & 0x40) {
                state = 0x60;
            }
            break;

        case 0x84:
            hdd_save_request.file = work.file;
            hdd_save_request.data = SaveData;
            hdd_save_request.size = 0x163B0;
            printf("xglHddMcSave():%d\n", xglHddMcSave(&hdd_save_request));
            state = 0x60;
            break;
        }

        if (PAD_U16_AT(42) & 0x10) {
            flags = SAVE_FLAGS;
            for (i = 0; i < 128; i++) {
                *flags++ = xglSRand();
            }
        }
        shown_flags = SAVE_FLAGS;
        xglFontDebugPrintf(16, 128, "%2x %2x %2x %2x %2x %2x %2x %2x", shown_flags[0], shown_flags[1],
                           shown_flags[2], shown_flags[3], shown_flags[4], shown_flags[5], shown_flags[6],
                           shown_flags[7]);
        xglSleep();
    }
}

static char *xtxdec_nextline(char *text)
{
    while (*text != '\n') {
        if ((unsigned char)*text == 0)
            return 0;
        ++text;
    }
    ++text;
    return (unsigned char)*text ? text : 0;
}

static char *xtxdec_getdec(char *s, int *out)
{
    int n = 0;

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    while ((unsigned char)(*s - '0') < 10) {
        n = n * 10 + *s - '0';
        s++;
    }
    if (out) {
        *out = n;
    }
    while (*s != ',') {
        s++;
    }
    return s + 1;
}

static void xtxdec_put4byte(LittleEndianWord *destination, int value)
{
    (*destination)[0] = value;
    (*destination)[1] = value >> 8;
    (*destination)[2] = value >> 16;
    (*destination)[3] = value >> 24;
}

extern char h_50[7];
extern int sceGsSyncPath(int mode, unsigned int timeout);
extern void sceGsSetDefLoadImage(void *image, short address, short width, short psm, short x,
                                 short y, short w, short h);
extern int sceGsExecLoadImage(void *image, void *source);
extern int sceGsSetDefStoreImage(void *image, short address, short width, short psm, short x,
                                 short y, short w, short h);
extern int sceGsExecStoreImage(void *image, void *destination);
extern char D_004C13D8[];
extern char D_004C13E8[];
extern char D_004C13F8[];
extern char D_004C1408[];
extern char D_004C1418[];
typedef struct XtxHeader {
    int magic[2]; 
    int image_count;
    int table_offset;
} XtxHeader;
typedef struct XtxImageEntry {
    unsigned int dimensions;
    short height;
    unsigned char unmodeled_06[2];
    unsigned int vram_address;
    unsigned char unmodeled_0c[4];
    unsigned int data_offset;
} XtxImageEntry;
extern char D_004C13D8[16];
extern char D_004C13E8[16];
extern char D_004C13F8[16];
extern char D_004C1408[16];
extern char D_004C1418[40];

static int xtxdec_sub(char *filename)
{
    char path[256];
    unsigned char load_image[96];
    unsigned char store_image[112];
    int bits_per_pixel;
    int left;
    int top;
    int width;
    int height;
    int origin_x;
    int origin_y;
    int palette_size;
    int count;
    int tbw;
    int i;
    int j;
    int fd;
    int read_size;
    unsigned int dimensions;
    int pixel_width;
    char *line;
    char *src_name;
    char *dst;
    unsigned char *out;
    unsigned char *vram_dst;
    XtxImageEntry *entry;
    XtxHeader *header;
    unsigned char *bmp;
    unsigned char *palette;
    int status;

    tbw = 64;
    bmp = (unsigned char *)0x01410000;
    sprintf(path, D_004C13D8, filename);
    memset((void *)0x01000000, 0, 0x10000);
    fd = sceOpen(path, 1);
    read_size = sceRead(fd, (void *)0x01000000, 0x10000);
    printf(D_004C13E8, fd, path, read_size);
    sceClose(fd);
    if (((XtxHeader *)0x01000000)->magic[0] != 0x58455423 ||
        ((XtxHeader *)0x01000000)->magic[1] != 0x314C5455)
        return -1;
    line = xtxdec_nextline((char *)0x01000000);
    if (line == 0)
        return -1;
    line = xtxdec_nextline(line);
    if (line == 0)
        return -1;
    sprintf(path, D_004C13F8, filename);
    memset((void *)0x01010000, 0, 256);
    fd = sceOpen(path, 1);
    count = 0;
    read_size = sceRead(fd, (void *)0x01010000, 0x400000);
    printf(D_004C13E8, fd, path, read_size);
    sceClose(fd);
    xglSleep();
    sceGsSyncPath(0, 0);
    header = (XtxHeader *)0x01010000;
    entry = (XtxImageEntry *)((char *)header + header->table_offset);
    for (; count < header->image_count; count++) {
        dimensions = entry->dimensions;
        pixel_width = dimensions & 0xFFFF;
        if (dimensions == pixel_width)
            tbw = (pixel_width + 63) >> 6;
        else
            tbw = dimensions >> 16;
        printf(D_004C1408, tbw, entry->vram_address);
        sceGsSetDefLoadImage(load_image, (entry->vram_address + 0xE0000) >> 6, tbw, 0, 0, 0,
                             pixel_width, entry->height);
        FlushCache(0);
        sceGsExecLoadImage(load_image, (void *)(0x01010020 + entry->data_offset));
        entry++;
    }
    while (line != 0) {
        dst = path;
        for (src_name = h_50; *src_name != 0; src_name++)
            *dst++ = (unsigned char)*src_name;
        if (*line != '"')
            return -1;
        line++;
        dst = path + 6;
        while (*line != '"') {
            *dst++ = *line;
            if (*line == '\\')
                dst = path + 6;
            line++;
        }
        *dst = 0;
        out = (unsigned char *)0x01410036;
        status = 0;
        line = xtxdec_getdec(line + 2, 0);
        line = xtxdec_getdec(line, &bits_per_pixel);
        line = xtxdec_getdec(line, 0);
        line = xtxdec_getdec(line, &left);
        line = xtxdec_getdec(line, &top);
        line = xtxdec_getdec(line, &width);
        line = xtxdec_getdec(line, &height);
        line = xtxdec_getdec(line, 0);
        line = xtxdec_getdec(line, &origin_x);
        line = xtxdec_getdec(line, &origin_y);
        printf(D_004C1418, path, bits_per_pixel, left, top, width, height, origin_x, origin_y);
        switch (bits_per_pixel) {
        default:
            palette_size = 0;
            break;
        case 4:
            sceGsSetDefStoreImage(store_image, 0x3800, tbw, 0, origin_x, origin_y, 8, 2);
            FlushCache(0);
            sceGsExecStoreImage(store_image, (void *)0x01810000);
            palette_size = 64;
            break;
        case 8:
            vram_dst = (unsigned char *)0x01810000;
            for (i = 0; i < 16; i += 2) {
                for (j = 0; j < 16; j += 8) {
                    sceGsSetDefStoreImage(store_image, 0x3800, tbw, 0, origin_x + j,
                                          origin_y + i, 8, 2);
                    FlushCache(0);
                    sceGsExecStoreImage(store_image, vram_dst);
                    vram_dst += 64;
                }
            }
            palette_size = 1024;
            break;
        }
        palette = (unsigned char *)0x01810000;
        for (i = 0; i < palette_size; i += 4) {
            out[0] = palette[i + 2];
            out[1] = palette[i + 1];
            out[2] = palette[i];
            out[3] = 0;
            out += 4;
        }
        for (i = height - 1; i >= 0; i--) {
            j = 0;
            switch (bits_per_pixel) {
            case 4: break;
            case 8:
                sceGsSetDefStoreImage(store_image, 0x3800, tbw * 2, 19, left, top + i, width, 1);
                j = width;
                break;
            case 16: case 24: case 32: break;
            }
            if (out != 0) {
                FlushCache(0);
                sceGsExecStoreImage(store_image, (void *)0x01810000);
                memcpy(out, (void *)0x01810000, j);
                out += j;
            }
        }
        i = out - bmp;
        out = (unsigned char *)0x0141000E;
        vram_dst = (unsigned char *)0x01410000;
        *vram_dst++ = 'B';
        *vram_dst++ = 'M';
        xtxdec_put4byte((LittleEndianWord *)0x01410002, i);
        xtxdec_put4byte((LittleEndianWord *)0x01410006, 0);
        xtxdec_put4byte((LittleEndianWord *)0x0141000A, palette_size + 54);
        xtxdec_put4byte((LittleEndianWord *)out, 40);
        xtxdec_put4byte((LittleEndianWord *)0x01410012, width);
        xtxdec_put4byte((LittleEndianWord *)0x01410016, height);
        out[12] = 1;
        out[13] = 0;
        out[14] = bits_per_pixel;
        out[15] = 0;
        xtxdec_put4byte((LittleEndianWord *)0x0141001E, 0);
        xtxdec_put4byte((LittleEndianWord *)0x01410022, 0);
        xtxdec_put4byte((LittleEndianWord *)0x01410026, 0);
        xtxdec_put4byte((LittleEndianWord *)0x0141002A, 0);
        xtxdec_put4byte((LittleEndianWord *)0x0141002E, 0);
        xtxdec_put4byte((LittleEndianWord *)0x01410032, 0);
        fd = sceOpen(path, 0x602);
        sceWrite(fd, (void *)0x01410000, i);
        sceClose(fd);
        line = xtxdec_nextline(line);
    }
    return status;
}

extern char D_004C14B8[];

static int xtxdec_sleep(void)
{
    if ((PAD_U64_AT(40) & 0x08000100ULL) == 0x08000100ULL) {
        return 1;
    }
    xglFontDebugPrintf(0, 0, D_004C14B8);
    xglSleep();
    return 0;
}

static void xtxdec_error(int y, const char *message)
{
    while (xtxdec_sleep() == 0) {
        xglFontDebugPrintf(0, y, message);
        if (PAD_U16_AT(42) & 0x20) {
            break;
        }
    }
}

extern int main_param_argc;
extern int main_param_argv;
extern char D_004C14C8[];
extern char D_004C14F8[];

static void XtxDecode(void)
{
    int i;
    int index;
    int y;
    int status;

    if (main_param_argc < 2) {
        xtxdec_error(16, D_004C14C8);
        return;
    }

    i = 1;
    y = 16;
    for (;;) {
        if (xtxdec_sleep()) {
            break;
        }
        if (i >= main_param_argc) {
            y = 16;
            break;
        }
        y = 16;
        for (index = 1; index <= i; index++) {
            xglFontDebugPrintf(0, y, ((char **) main_param_argv)[index]);
            y += 8;
        }
        status = xtxdec_sub(((char **) main_param_argv)[i]);
        if (status < 0) {
            break;
        }
        i++;
    }
    xtxdec_error(y, D_004C14F8);
}

static void Dummy(void)
{
    tyaCaptureStart(0, 0);
    tyaCaptureEnd();
    tyaElevatorTask(0);
    tyaMenuBgEntry(0, 0);
    tyaDisplaySetting(0);
    tyaDrawGauge(0);
}

extern void xglRenderClearFrame(void);
extern void xglRenderClearColor(unsigned int color);
extern void xglRenderClearDepth(void);
extern void xglMenuInitial(void);
extern void xglMenuOpen(int mode, YajimaMenuState *menu);
extern void xglMenuDraw(void);
extern void xglClockRead(XglClock *clock);
extern int xglFRand(void);
extern YajimaMenuState m_54;
extern void (*func_51[7])(void);
extern char D_004DA3A0[];
extern char D_004C1518[];

void YajimaTest(void)
{
    int selection;
    XglClock clock;

    PadData.bytes[0x4E] = 0x40;
    PadData.bytes[0x4F] = 0x40;
    xglRenderClearFrame();
    xglRenderClearColor(0x80004000);

    for (;;) {
        xglMenuInitial();
        m_54.selection = &selection;
        selection = 0;
        xglMenuOpen(-1, &m_54);
        while (selection == 0) {
            xglFontDebugPrintf(0, 0, D_004DA3A0);
            xglMenuDraw();
            xglClockRead(&clock);
            xglFontDebugPrintf(0x80, 0x10, D_004C1518, clock.year, clock.month,
                                clock.day, clock.hour, clock.minute, clock.second);
            xglFRand();
            xglSleep();
        }
        if (selection == -1) {
            break;
        }
        func_51[selection - 1]();
        xglSleep();
    }
    xglRenderClearDepth();
}

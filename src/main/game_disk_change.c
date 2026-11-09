#include "common.h"
#include "shared.h"
#include "main/xgl_packet.h"

/* Types recovered upstream (origin/master); kept for reference. */
typedef struct GifAdData {
    u64 value;
    u64 register_address;
} GifAdData;

typedef struct GifRgbaqData {
    u32 rgba;
    u32 q;
    u32 ignored_08;
    u32 ignored_0c;
} GifRgbaqData;

typedef struct GifXyz2Data {
    u16 x;
    u16 ignored_02;
    u16 y;
    u16 ignored_06;
    u32 z;
    u16 ignored_0c;
    u16 disable_drawing;
} GifXyz2Data;

typedef struct GameDiskChangeGifPacket {
    /* GIFtag control: NLOOP=1, EOP=1, PRE=1, PRIM=6, PACKED, NREG=4. */
    const u8 *tag_control_low;
    u32 tag_control_high;
    /* The low four nibbles select A+D, RGBAQ, XYZ2 and XYZ2 in that order. */
    u64 register_ids;
    GifAdData test_1;
    GifRgbaqData color;
    GifXyz2Data first_vertex;
    GifXyz2Data second_vertex;
} GameDiskChangeGifPacket;

typedef struct GameDiskChangeTestEnv {
    u32 vif_nop[3];
    u32 vif_direct;
    GameDiskChangeGifPacket gif;
} GameDiskChangeTestEnv;


extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkAddDataN(XglPacket *packet, const void *data, int count);

static void drawbg(void)
{
    static u64 TestEnv[12] __attribute__((aligned(16))) = {
        0x0000000000000000, 0x5000000500000000, 0x4003400000008001, 0x000000000000551E,
        0x0000000000030000, 0x0000000000000047, 0x0000004000000000, 0x0000008000000000,
        0x000071F700006FF8, 0x0000000000000000, 0x00008DF700008FF8, 0x0000000000000000,
    };
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, TestEnv, 0x18);
}

static void replace(u8 *str, int disk_number) {
    u8 *ch;

    ch = str;
    if (*ch != 0) {
        do {
            /* The placeholder is a fixed two-byte code read as one unit. */
            if ((*(u16 *)ch & 0xF0FF) == 0xB0A3) {
                ch[1] = (u8)(disk_number - 0x50);
            }
            ch += 2;
        } while (*ch != 0);
    }
}

extern int printf(const char *format, ...);

extern void xglSleep(void);
extern int xglCdDiskCheck(void);
extern void xglCdArcCheck(void);
extern void xglFontPrint(int x, int y, int color, const char *text);
extern void xglFontPrintDirectOT(int ot, const char *text);

extern u8 GameDiskChangeT10K;
extern PadPrefix PadData;

int GameDiskChange(int disk_number)
{
    /* EUC-JP messages; replace() patches the disc digits in place. */
    static u8 mes00_1[] __asm__("mes00.1") = "\xA3\xC4\xA3\xC9\xA3\xD3\xA3\xC3\xA3\xB1\xA4\xF2\xBC\xE8\xA4\xEA\xBD\xD0\xA4\xB7\xA4\xC6\xA1\xA2\n\n\xA3\xC4\xA3\xC9\xA3\xD3\xA3\xC3\xA3\xB2\xA4\xF2\xC1\xDE\xC6\xFE\xA4\xB7\xA4\xC6\xB2\xBC\xA4\xB5\xA4\xA4\xA1\xA3";
    static u8 mes01_2[] __asm__("mes01.2") = "\xA3\xC4\xA3\xC9\xA3\xD3\xA3\xC3\xA4\xAC\xB0\xE3\xA4\xA4\xA4\xDE\xA4\xB9\xA1\xA3\xA3\xC4\xA3\xC9\xA3\xD3\xA3\xC3\xA4\xF2\xBC\xE8\xA4\xEA\xBD\xD0\xA4\xB7\xA4\xC6\xA1\xA2\n\n\xA5\xBC\xA5\xCE\xA5\xB5\xA1\xBC\xA5\xAC\xA4\xCE\xA3\xC4\xA3\xC9\xA3\xD3\xA3\xC3\xA3\xB2\xA4\xF2\xC1\xDE\xC6\xFE\xA4\xB7\xA4\xC6\xB2\xBC\xA4\xB5\xA4\xA4\xA1\xA3";
    static u8 mes02_3[] __asm__("mes02.3") = "\xA5\xBC\xA5\xCE\xA5\xB5\xA1\xBC\xA5\xAC\xA4\xCE\xA3\xC4\xA3\xC9\xA3\xD3\xA3\xC3\xA3\xB2\xA4\xF2\xC1\xDE\xC6\xFE\xA4\xB7\xA4\xC6\xB2\xBC\xA4\xB5\xA4\xA4\xA1\xA3";
    int disk_check;
    int retry;

    printf("GameDiskChange:%d %d\n", disk_number, GameDiskChangeT10K);

    if (disk_number < 1 || disk_number > 3) {
        return -1;
    }

    disk_check = xglCdDiskCheck();
    switch (disk_check) {
    case 1:
        /* Low byte of the Shift-JIS fullwidth digit (0xA3 0xB1) naming the disc in the drive. */
        mes00_1[9] = 0xB1;
        break;
    case 2:
        mes00_1[9] = 0xB2;
        break;
    case 4:
        mes00_1[9] = 0xB3;
        break;
    case 0x7F:
        return 0;
    default:
        break;
    }

    retry = 0;
    replace(&mes00_1[0x10], disk_number);
    replace(mes01_2, disk_number);
    replace(mes02_3, disk_number);

    /* From here on the requested disc is held as its bit in the drive's disc mask. */
    disk_number = 1 << (disk_number - 1);
    while ((disk_check = xglCdDiskCheck()) <= 0 || !(disk_check & disk_number)) {
        if (disk_check == -2) {
            retry = 1;
        }
        drawbg();
        xglFontPrintDirectOT(0xFFFFFF, "\v\r\x03");
        if (retry == 0) {
            xglFontPrint(0x40, 0x40, 0xFFFFFF, (const char *)mes00_1);
        } else {
            /* Indexed by xglCdDiskCheck() + 3. */
            static char *tbl_4[] __asm__("tbl.4") = {
                "\xC7\xA7\xBC\xB1\xC3\xE6\xA1\xC4",
                (char *)mes02_3, (char *)mes01_2, (char *)mes02_3,
                (char *)mes01_2, (char *)mes01_2,
                "\xA3\xC4\xA3\xC9\xA3\xD3\xA3\xCB\xA3\xB1\xA1\xF5\xA3\xB2",
                (char *)mes01_2,
            };

            xglFontPrint(0x40, 0x40, 0xFFFFFF, tbl_4[disk_check + 3]);
        }
        xglFontPrint(0x40, 0x100, 0xFFFFFF, "\x19\x01\xA3\xD3\xA3\xD4\xA3\xC1\xA3\xD2\xA3\xD4\x19\x00\xA4\xC7\xB6\xAF\xC0\xA9\xC2\xB3\xB9\xD4");
        if (PadData.half_2a & 0x800) {
            break;
        }
        xglSleep();
    }

    xglCdArcCheck();
    xglSleep();
    return 0;
}

/* Emitted after GameDiskChange's .sdata string, as in the original. */
u8 GameDiskChangeT10K = 0;

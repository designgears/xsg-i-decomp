/*
 * OV11 original TU 1: 0x00a00100..0x00a091f8 (135 functions)
 */
#include "common.h"
#include "shared.h"
#include "res.h"
#include "mini_g.h"

/* xglCdReadFile is main:0x0021e310 (src/main/xgl_cd.c); every call below
 * passes it an asset path string and a fixed EE main RAM staging address. */
extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);

typedef char *va_list;
#define va_start(ap, last) ((ap) = (va_list)__builtin_next_arg(last) - (8 - __builtin_args_info(2)) * 8)
#define va_end(ap) ((void)0)

/* Partial newlib struct _reent: only the _stderr file pointer this debug
 * printf forwards to is touched; the preceding _errno/_stdin/_stdout
 * fields (newlib 1.9.0 libc/include/sys/reent.h field order) stay
 * unmodeled. _impure_ptr is the reentrant global newlib state pointer. */
struct Reent {
    unsigned char unmodeled_00[0xC];
    void *stderr_file;
};
extern struct Reent *_impure_ptr;
extern int vfprintf(void *stream, const char *format, va_list args);

extern unsigned char Cursor_Rect[4][0x10];

extern unsigned char Cursor_Tex[8][0x1C];

extern unsigned char result_rect_97[][0x10];

extern unsigned char PoCurTex[8][28];

extern short selpos_94[5][2];

extern short D_00A0DEB8[5];

extern unsigned char point_rect_20[9][16];

extern unsigned char GuidPtTex[10][28];

extern unsigned char pay_window[2][28];

extern unsigned char tex_37[3][0x1C];

extern unsigned char Tex_106[8][0x1C];

typedef struct {
    int w[7];
} NgRealShape;

extern NgRealShape NgReal_Tex_48[];

extern short *ng_realtbl[20];

extern signed char fileinfo[];

extern char *MUSICLIST_125[];

extern unsigned char CardPos[5][0x10];

extern unsigned short ptmove_50[11][4];

void dprintf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(_impure_ptr->stderr_file, fmt, args);
    va_end(args);
}

typedef unsigned int Quadword __attribute__((mode(TI)));

typedef union { struct { u64 lo; u64 hi; } part; Quadword quad; } SetRegADTag;

extern void sceVif1PkCnt(XglPacket *packet, int count);

extern void sceVif1PkOpenDirectCode(XglPacket *packet, int mode);

extern void sceVif1PkOpenGifTag(XglPacket *packet, Quadword tag);

extern void sceVif1PkCloseGifTag(XglPacket *packet);

extern void sceVif1PkCloseDirectCode(XglPacket *packet);

extern XglPacket *Pkt;

extern void sceVif1PkAddGsData(XglPacket *packet, u64 data);
extern const SetRegADTag D_00A0BB30;
extern const SetRegADTag D_00A0BB40;

static void MakeSprite(void *sprite, void *rect)
{
    int *spr = sprite;
    int *rc = rect;
    SetRegADTag tag0;
    SetRegADTag tag1;
    int x;
    int y;
    int w;
    int h;

    tag0 = D_00A0BB30;
    tag1 = D_00A0BB40;
    x = spr[0] + 1;
    y = spr[1] + 1;
    w = spr[2] - 1;
    h = spr[3] - 1;
    sceVif1PkCnt(Pkt, 0);
    sceVif1PkOpenDirectCode(Pkt, 0);
    sceVif1PkOpenGifTag(Pkt, tag0.quad);
    switch (spr[6]) {
    case 1:
        sceVif1PkAddGsData(Pkt, (u64)(spr[4] + 0x3800) | 0x25440000 | ((u64)(spr[5] + 0x3800) << 37) | 0x2000000E40000000);
        y = y * 2 - 1;
        break;
    case 2:
        sceVif1PkAddGsData(Pkt, ((u64)(spr[5] + 0x3800) << 37) | 0x4000000E21343800);
        break;
    case 3:
        sceVif1PkAddGsData(Pkt, ((u64)(spr[5] + 0x3800) << 37) | 0x4000000EA9443800);
        y = y * 2 - 1;
        break;
    default:
        sceVif1PkAddGsData(Pkt, ((u64)(spr[5] + 0x3800) << 37) | 0x4000000E65323800);
        break;
    }
    sceVif1PkCloseGifTag(Pkt);
    sceVif1PkOpenGifTag(Pkt, tag1.quad);
    sceVif1PkAddGsData(Pkt, 0x154);
    sceVif1PkAddGsData(Pkt, (u64)(x * 16) | ((u64)(y * 16) << 16));
    sceVif1PkAddGsData(Pkt, (u64)(rc[0] * 16 + 0x7000) | ((u64)(rc[1] * 16 + 0x7200) << 16) | ((u64)0x80 << 32));
    sceVif1PkAddGsData(Pkt, (u64)((x + w) * 16) | ((u64)(y * 16) << 16));
    sceVif1PkAddGsData(Pkt, (u64)((rc[0] + rc[2]) * 16 + 0x7000) | ((u64)(rc[1] * 16 + 0x7200) << 16) | ((u64)0x80 << 32));
    sceVif1PkAddGsData(Pkt, (u64)(x * 16) | ((u64)((y + h) * 16) << 16));
    sceVif1PkAddGsData(Pkt, (u64)(rc[0] * 16 + 0x7000) | ((u64)((rc[1] + rc[3]) * 16 + 0x7200) << 16) | ((u64)0x80 << 32));
    sceVif1PkAddGsData(Pkt, (u64)((x + w) * 16) | ((u64)((y + h) * 16) << 16));
    sceVif1PkAddGsData(Pkt, (u64)((rc[0] + rc[2]) * 16 + 0x7000) | ((u64)((rc[1] + rc[3]) * 16 + 0x7200) << 16) | ((u64)0x80 << 32));
    sceVif1PkCloseGifTag(Pkt);
    sceVif1PkCloseDirectCode(Pkt);
}

extern void sceVif1PkAddGsAD(XglPacket *packet, int reg, u64 value);

extern const SetRegADTag D_00A0BB50;

static void SetDrawStatus(int mode, int enable)
{
    SetRegADTag tag;

    tag = D_00A0BB50;
    sceVif1PkCnt(Pkt, 0);
    sceVif1PkOpenDirectCode(Pkt, 0);
    sceVif1PkOpenGifTag(Pkt, tag.quad);
    sceVif1PkAddGsAD(Pkt, 0x47, 0x5001D);
    sceVif1PkAddGsAD(Pkt, 0x14, 0x20);
    sceVif1PkAddGsAD(Pkt, 8, 0);
    switch (mode) {
    case 0:
        sceVif1PkAddGsAD(Pkt, 0x42, 0x48);
        break;
    case 2:
        sceVif1PkAddGsAD(Pkt, 0x42, ((u64)0x60 << 32) | 0x62);
        break;
    case 3:
        sceVif1PkAddGsAD(Pkt, 0x42, 0);
        break;
    case 4:
        sceVif1PkAddGsAD(Pkt, 0x42, 0x88);
        break;
    case 5:
        sceVif1PkAddGsAD(Pkt, 0x42, ((u64)enable << 32) | 0x29);
        break;
    case 6:
        sceVif1PkAddGsAD(Pkt, 0x42, ((u64)enable << 32) | 0x68);
        break;
    case 1:
        sceVif1PkAddGsAD(Pkt, 0x42, 0x44);
        break;
    default:
        sceVif1PkAddGsAD(Pkt, 0x42, 0);
        break;
    }
    sceVif1PkCloseGifTag(Pkt);
    sceVif1PkCloseDirectCode(Pkt);
}

extern const SetRegADTag D_00A0BB80;

static void SetTest(int value)
{
    SetRegADTag tag;

    tag = D_00A0BB80;
    sceVif1PkCnt(Pkt, 0);
    sceVif1PkOpenDirectCode(Pkt, 0);
    sceVif1PkOpenGifTag(Pkt, tag.quad);
    sceVif1PkAddGsAD(Pkt, 0x47, value);
    sceVif1PkCloseGifTag(Pkt);
    sceVif1PkCloseDirectCode(Pkt);
}

extern const SetRegADTag D_00A0BB90;
static void SetRegAD(XglPacket *packet, int reg, int value)
{
    SetRegADTag tag;

    tag = D_00A0BB90;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, tag.quad);
    sceVif1PkAddGsAD(packet, reg, value);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

extern short BoxRGBA[4];

static void ResetRGBA(void)
{
    int index;
    short reset_color;

    reset_color = 0x80;
    for (index = 3; index >= 0; index--) {
        BoxRGBA[index] = reset_color;
    }
}

/* ov11:0x00a007e0. Stores the four caller-supplied components in the
 * shared untextured box color state (original LOCAL data symbol
 * BoxRGBA, ov11:0x00a0df78). */
extern short BoxRGBA[4];

static void SetRGBA(short red, short green, short blue, short alpha)
{
    BoxRGBA[0] = red;
    BoxRGBA[1] = green;
    BoxRGBA[2] = blue;
    BoxRGBA[3] = alpha;
}

extern unsigned long BoxPOS[4];

static void MakeBoxPos(int x, int y, int w, int h)
{
    BoxPOS[0] = (unsigned long)((x << 4) + 0x7000) | ((unsigned long)((y << 4) + 0x7200) << 16) | (1UL << 32);
    BoxPOS[1] = (unsigned long)(((x + w) << 4) + 0x7000) | ((unsigned long)((y << 4) + 0x7200) << 16) | (1UL << 32);
    BoxPOS[2] = (unsigned long)((x << 4) + 0x7000) | ((unsigned long)(((y + h) << 4) + 0x7200) << 16) | (1UL << 32);
    BoxPOS[3] = (unsigned long)(((x + w) << 4) + 0x7000) | ((unsigned long)(((y + h) << 4) + 0x7200) << 16) | (1UL << 32);
}

extern const SetRegADTag D_00A0BBA0;

static void DrawNonTextureBox(XglPacket *packet, int x, int y, int w, int h)
{
    SetRegADTag tag;
    int i;

    tag = D_00A0BBA0;
    MakeBoxPos(x, y, w, h);
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, tag.quad);
    sceVif1PkAddGsData(packet, 0x144);
    for (i = 0; i < 4; i++) {
        sceVif1PkAddGsData(packet, ((u64)(BoxRGBA[0]) << 0) | ((u64)(BoxRGBA[1]) << 8) | ((u64)(BoxRGBA[2]) << 16) | ((u64)(BoxRGBA[3]) << 24) | ((u64)(1) << 32));
        sceVif1PkAddGsData(packet, BoxPOS[i]);
    }
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

/* Casino settings, prize text/costs and coin-purchase offers from CASINO.res.
 * Original RES_* symbols: slus-20469-412d448de315 / ov11:0x00a00998..0x00a00a58.
 * See reports/readability-semantics.md for asset/consumer evidence.
 *
 * All eight are LOCAL symbols in the original overlay, so they have internal
 * linkage and are defined `static` here. The compiled object binds them the
 * same way, which is what the linked comparison checks alongside the bytes.
 */

static int RES_IsDebugMode(void)
{
    /* The debug word is proven; the intervening sound region's extent is not. */
    int *debug_mode = (int *)((unsigned char *)ResData + CASINO_DEBUG_SETTING_OFFSET);
    return *debug_mode;
}

static int RES_GetBonusTime(void)
{
    return ResData->bonus_duration;
}

static int RES_GetRealSpeed(void)
{
    return ResData->reel_step;
}

static char *RES_ShopDataInfo(int prize_index)
{
    return ResData->prizes[prize_index].description;
}

static int RES_ShopDataCoin(int prize_index)
{
    CasinoPrize *prize = ResData->prizes;
    prize += prize_index;
    return prize->coin_cost;
}

static char *RES_ShopDataName(int prize_index)
{
    return ResData->prizes[prize_index].name;
}

static int RES_Coin_Rate(int offer_index)
{
    return ResData->coin_offers[offer_index].money_cost;
}

static char *RES_Coin_Info(int offer_index)
{
    return ResData->coin_offers[offer_index].description;
}

/* The sound-effect ID table sits between the known CasinoResourcePrefix
 * prefix and CASINO_DEBUG_SETTING_OFFSET (0x1d3c); its own extent past this
 * indexed access is not proven, so it is reached by byte offset rather than
 * a named CasinoResourcePrefix member (matching RES_IsDebugMode above). */
#define CASINO_SOUND_ID_TABLE_OFFSET 0x193C

extern void xglSoundEffectNormalID(int soundEffectId, int variant);
extern const char D_00A0BBB0[]; /* "Sound Req = SE_%.3d:Sound num = %x\n" */

static void RES_SoundEffect(int soundId)
{
    int soundEffectId = *(int *)((unsigned char *)ResData + soundId * 4 + CASINO_SOUND_ID_TABLE_OFFSET);

    dprintf(D_00A0BBB0, soundId, soundEffectId);
    xglSoundEffectNormalID(soundEffectId, 0);
}

extern void xglSoundEffectStopDirect(int soundEffectId);
extern const char D_00A0BBD8[]; /* "Sound Req = SE_%.3d:Sound Stop num = %x\n" */

static void RES_SoundEffectStop(int soundId)
{
    int soundEffectId = *(int *)((unsigned char *)ResData + soundId * 4 + CASINO_SOUND_ID_TABLE_OFFSET);

    dprintf(D_00A0BBD8, soundId, soundEffectId);
    xglSoundEffectStopDirect(soundEffectId);
}

/* ov11:0x00a00af0. Loads the casino resource table (CASINO.res) into the
 * fixed asset staging address and records it as ResData, the same
 * (void *)FIXED_ADDRESS pattern PokerInit and the other loaders below use. */
extern const char D_00A0BC08[]; /* "data\\tanaka\\CASINO.res" */

#define CASINO_RES_BUFFER 0x01000000

static void RES_Load(void)
{
    xglCdReadFile(D_00A0BC08, (void *)CASINO_RES_BUFFER, 0, 0);
    ResData = (CasinoResourcePrefix *)CASINO_RES_BUFFER;
}

/* The coin balance is the word at offset 0x50 in the persistent save block. */
struct CasinoSaveWork {
    unsigned char unmodeled_00[0x50];
    int coin_balance;
};
extern int dataMoneyBoxInc(int amount);

static int AddCoin(int amount)
{
    struct CasinoSaveWork *save;
    int total;
    int excess;

    save = (struct CasinoSaveWork *)SaveWork;
    total = save->coin_balance + amount;
    if (total > 9999999) {
        excess = total - 9999999;
        dataMoneyBoxInc(excess * 8);
        amount -= excess;
    }
    save = (struct CasinoSaveWork *)SaveWork;
    save->coin_balance += amount;
    return 1;
}

extern short *realpt[18];
extern int D_00A0DD34[];

static short SlGetPic(int reel, int positionOffset, int row)
{
    int position;
    int pictureIndex;
    short *pictures;

    position = D_00A0DD34[reel] / 64;
    pictures = realpt[reel + row * 3];
    position -= positionOffset;
    pictureIndex = position + 2;
    if (pictureIndex < 0) {
        pictureIndex = position + 23;
    }
    if (pictureIndex >= 21) {
        pictureIndex -= 21;
    }
    return pictures[pictureIndex];
}

extern int xpos_3[3];
extern unsigned char Real_2[][0x1C];
extern unsigned char slot_window_0[0x1C];
extern unsigned char slot_window_rect_1[3][0x10];

static void SlRealDraw(void)
{
    struct { int x, y, w, h; } rect;
    int i;
    int j;

    SetDrawStatus(3, 0);
    for (j = 0; j < 3; j++) {
        if (Gwork[5 + j] > 0x540) {
            Gwork[5 + j] -= 0x540;
        }
    }
    rect.w = 0x70;
    rect.h = 0x40;
    for (i = 0; i < 3; i++) {
        rect.x = xpos_3[i];
        for (j = 4; j >= -1; j--) {
            rect.y = Gwork[5 + i] % 64 + (2 - j) * 64 + 0x32;
            MakeSprite(Real_2[SlGetPic(i, 2 - j, ((short *)Gwork)[0x48 / 2])], &rect);
        }
    }
    SetDrawStatus(2, 0);
    for (j = 0; j < 3; j++) {
        MakeSprite(slot_window_0, slot_window_rect_1[j]);
    }
    SetDrawStatus(3, 0);
    SetTest(0x3000D);
    ResetRGBA();
    SetRGBA(0, 0, 0, 0x80);
    DrawNonTextureBox(Pkt, 0x50, 0x12C, 0x160, 0x40);
}

extern double dpow(double, double) __asm__("pow");
extern unsigned char decfont[10][0x1C];

static void decprint(int value, int digits, int x, int y)
{
    int i;
    int rect[4];

    if (digits < 0) {
        digits = 0;
    loop:
        if (digits < 11) {
            if (value >= dpow(10.0, digits)) {
                digits++;
                goto loop;
            }
        }
    }
    rect[2] = 16;
    rect[3] = 24;
    if (digits > 0) {
        for (i = 0; i < digits; i++) {
            rect[0] = x - i * 16;
            rect[1] = y;
            MakeSprite(decfont[(int)(value / dpow(10.0, i)) % 10], rect);
        }
    } else {
        rect[0] = x;
        rect[1] = y;
        MakeSprite(decfont, rect);
    }
}

static void decprint(int value, int digits, int x, int y);
extern int D_00A0DDC8;

static void SlCashPrint(void)
{
    decprint(((struct CasinoSaveWork *)SaveWork)->coin_balance, -1, 440, 309);
    decprint(D_00A0DDC8, -1, 448, 373);
}

typedef struct { short x, y; } PayPos;
typedef struct { PayPos p[5]; } PayPrintPos;
extern const PayPrintPos D_00A0BC20;

static void pay_print(void)
{
    PayPrintPos t;
    int i;
    short *g;

    t = D_00A0BC20;
    g = (short *)Gwork;
    for (i = 0; i < 5; i++) {
        decprint(g[0x2B + i], 2, t.p[i].x, t.p[i].y);
    }
}

/* ov11:0x00a01060. Draws the three-digit slot total stake at its fixed
 * screen position through decprint (ov11:0x00a00de0, LOCAL asm sibling). */
static void decprint(int value, int digits, int x, int y);
extern short D_00A0DD74;

static void total_pay_print(void)
{
    decprint(D_00A0DD74, 3, 0x1DC, 0x1B);
}

static int check_mode(void)
{
    int index;

    for (index = 0; index < 3; index++) {
        if (Gwork[index + 8] == 1) {
            return 1;
        }
    }
    return 0;
}

extern unsigned char pay_window_rect_4[5][16];
static void MakeSprite(void *sprite, void *rect);
static void SetDrawStatus(int mode, int enable);

static void SlPayWindowPrint(void)
{
    int rectIndex;
    int windowIndex;

    SetDrawStatus(0, 0);
    windowIndex = check_mode() != 1;
    for (rectIndex = 0; rectIndex < 5; rectIndex++) {
        MakeSprite(pay_window[windowIndex], pay_window_rect_4[rectIndex]);
    }
}

extern int status_keep_5;

static int check_spin(void)
{
    int active;

    active = check_mode();
    if (active == 0) {
        status_keep_5 = 0;
        return active;
    }
    if (active != status_keep_5) {
        status_keep_5 = active;
        return 1;
    }
    status_keep_5 = active;
    return 2;
}

extern unsigned char spinbt[3][0x1C];
extern unsigned char spinbt_rect_6[0x10];
extern int pushcnt_7;

/* Draws the spin button: lit while spinning is possible, pressed for a
 * few frames after a push, greyed out when spinning is not allowed. */
static void SpinBtnPrint(void)
{
    int state;

    switch (check_spin()) {
    case 0:
        state = 1;
        break;
    case -1:
        state = 0;
        break;
    case 1:
        state = 2;
        pushcnt_7 = 0;
        break;
    default:
        if (pushcnt_7 >= 0) {
            state = 2;
            if (++pushcnt_7 >= 5) {
                pushcnt_7 = -1;
            }
        } else {
            state = 3;
        }
        break;
    }
    if (state != 0) {
        MakeSprite(spinbt[state - 1], spinbt_rect_6);
    }
}

extern unsigned char stopbt[3][0x1C];
extern unsigned char stopbt_rect_8[3][0x10];

/* Draws the three reel stop buttons: state 1 (spinning) -> lit,
 * 2 -> pressed, anything else -> disabled. */
static void SlStopBtnPrint(void)
{
    int i;
    int state;

    for (i = 0; i < 3; i++) {
        switch (Gwork[8 + i]) {
        case 1:
            state = 0;
            break;
        case 2:
            state = 1;
            break;
        default:
            state = 2;
            break;
        }
        MakeSprite(stopbt[state], stopbt_rect_8[i]);
    }
}

extern unsigned char DeciList_9[10][0x1C];

static void SlDecPrintSmall(int x, int y, int step, int value)
{
    int i;
    int digits;
    int rect[4];

    digits = 0;
loop:
    if (digits < 11) {
        if (value >= dpow(10.0, digits)) {
            digits++;
            goto loop;
        }
    }
    rect[2] = 12;
    rect[3] = 16;
    if (digits > 0) {
        for (i = 0; i < digits; i++) {
            rect[0] = x - i * step;
            rect[1] = y;
            MakeSprite(DeciList_9[(int)(value / dpow(10.0, i)) % 10], rect);
        }
    } else {
        rect[0] = x;
        rect[1] = y;
        MakeSprite(DeciList_9, rect);
    }
}

typedef struct { int w[4]; } SlRateRect;
extern const SlRateRect D_00A0BC38;
extern const SlRateRect D_00A0BC48;
extern const SlRateRect D_00A0BC58;
extern const SlRateRect D_00A0BC68;
extern const SlRateRect D_00A0BC78;
extern const SlRateRect D_00A0BC88;
extern int pos_10[7][2];
extern unsigned char PicTable[][0x1C];
extern unsigned char OptionText[][0x1C];
extern int rate_11[6];
extern int double_rate_12[6];
typedef union {
    short h[0x30];
} SlRateWork;

static void SlRateTableDraw(void)
{
    SlRateRect rect;
    int i;

    SetDrawStatus(3, 0);
    rect.w[2] = 0x54;
    rect.w[3] = 0x10;
    for (i = 0; i < 6; i++) {
        rect.w[0] = pos_10[i][0];
        rect.w[1] = pos_10[i][1];
        MakeSprite(PicTable[i], &rect);
    }
    rect.w[1] = pos_10[i][1];
    rect.w[0] = pos_10[i][0];
    rect.w[2] = 0x1C;
    MakeSprite(PicTable[i], &rect);
    rect = D_00A0BC38;
    MakeSprite(OptionText[1], &rect);
    rect = D_00A0BC48;
    MakeSprite(OptionText[1], &rect);
    rect = D_00A0BC58;
    MakeSprite(OptionText[1], &rect);
    rect = D_00A0BC68;
    MakeSprite(OptionText[2], &rect);
    rect = D_00A0BC78;
    MakeSprite(OptionText[3], &rect);
    rect = D_00A0BC88;
    MakeSprite(OptionText[4], &rect);
    for (i = 0; i < 6; i++) {
        if (((SlRateWork *)Gwork)->h[0x25] != 0) {
            SlDecPrintSmall(0xAF, 0x125 + i * 20, 12, double_rate_12[i]);
        } else {
            SlDecPrintSmall(0xAF, 0x125 + i * 20, 12, rate_11[i]);
        }
    }
}

typedef struct {
    short reel[3];
    short pay;
    short mult;
    short flag;
} SlPrize;

extern SlPrize D_00A0DD8A[];

INCLUDE_ASM("asm/nonmatchings/ov11/mini_g", SlResultDraw);

typedef struct { int w[4]; } SlModeRect;
extern const SlModeRect D_00A0BCA8;
extern const SlModeRect D_00A0BCB8;
extern const SlModeRect D_00A0BCC8;
extern const SlModeRect D_00A0BCD8;
extern unsigned char Text_17[3][0x1C];
extern unsigned char Bar_18[0x1C];
extern unsigned char TimeBar_19[0x1C];
typedef union {
    short h[0x30];
} SlModeWork;

static void SlModePrint(void)
{
    SlModeRect rect;
    SlModeRect trect;
    float f;

    if (((SlModeWork *)Gwork)->h[0x25] != 0 && ((SlModeWork *)Gwork)->h[0x26] == 0) {
        rect = D_00A0BCA8;
        MakeSprite(Text_17[1], &rect);
    }
    if (((SlModeWork *)Gwork)->h[0x26] != 0) {
        rect = D_00A0BCA8;
        MakeSprite(Text_17[2], &rect);
    }
    if (((SlModeWork *)Gwork)->h[0x26] == 0 && ((SlModeWork *)Gwork)->h[0x25] == 0) {
        rect = D_00A0BCB8;
        MakeSprite(Text_17[0], &rect);
        return;
    }
    rect = D_00A0BCC8;
    MakeSprite(Bar_18, &rect);
    trect = D_00A0BCD8;
    f = ((SlModeWork *)Gwork)->h[0x27];
    f = (f / (float)RES_GetBonusTime()) * 180.0f;
    trect.w[2] = (int)f;
    MakeSprite(TimeBar_19, &trect);
    if (Gwork[0xB0 / 4] == 0) {
        ((SlModeWork *)Gwork)->h[0x27]--;
    }
    if (((SlModeWork *)Gwork)->h[0x27] == 0) {
        if (Gwork[0x50 / 4] != 0) {
            ((SlModeWork *)Gwork)->h[0x27] = 1;
        } else {
            Gwork[0xB0 / 4] = 7;
        }
    }
}

extern int seq_21;

static void SlGuidPrint(int index)
{
    MakeSprite(GuidPtTex[seq_21], point_rect_20[index]);
    seq_21++;
    seq_21 %= 10;
}

extern unsigned short PadData[];
extern int D_00A0DD5C;

static void point_control(void)
{
    switch (check_mode()) {
    case 0:
        SlGuidPrint(D_00A0DD5C);
        break;
    case 1:
        SlGuidPrint(Gwork[14] + 5);
        if (PadData[0x2A / 2] & 0x2000) {
            Gwork[14]++;
            if (Gwork[14] >= 3) {
                Gwork[14] = 0;
            }
            RES_SoundEffect(0x24);
        }
        if (PadData[0x2A / 2] & 0x8000) {
            Gwork[14]--;
            if (Gwork[14] < 0) {
                Gwork[14] = 2;
            }
            RES_SoundEffect(0x25);
        }
        break;
    }
}

/* ov11:0x00a01e28. Configures background draw state, then emits the fixed
 * slot background sprite built from BoxRect (ov11:0x00a0a0e0) into BoxSpr
 * (ov11:0x00a0a0c0). MakeSprite, SetDrawStatus and SetTest are LOCAL asm
 * siblings whose parameter roles beyond this one call site are not evidenced. */
static void MakeSprite(void *sprite, void *rect);
static void SetDrawStatus(int mode, int enable);
static void SetTest(int value);
extern unsigned char BoxSpr[0x1C];
extern unsigned char BoxRect[0x10];

static void SlBgDraw(void)
{
    SetDrawStatus(3, 0);
    SetTest(0x5000D);

    /* MakeSprite is the last statement of this void function and gcc 2.96
     * compiles a trailing call like that as a sibling jump; the original
     * keeps a real jal plus the shared epilogue (ld $31/jr $31). Wrapping
     * only this call in do{}while(0) defeats the sibcall pass instead
     * (CP-0288, config/compiler-patterns.json). */
    do {
        MakeSprite(BoxSpr, BoxRect);
    } while (0);
}

typedef struct { short v[4]; } SlCoinTbl;
extern const SlCoinTbl D_00A0BCE8;
typedef struct {
    char pad[0x2A];
    unsigned short trg;
    unsigned short rep;
} SlCoinPad;
typedef union {
    short h[0x40];
} SlCoinWork;
typedef struct {
    short h[0x40];
} SlCoinWorkS;

static void SlCoinEntry(void)
{
    SlCoinTbl tbl = D_00A0BCE8;
    short *g;
    short *g2;
    short *g3;
    short *p;
    short *p2;
    int off;
    int off2;
    int i;

    if (((SlCoinPad *)PadData)->trg & 0x1000) {
        Gwork[0x44 / 4] = 0;
        if (--Gwork[0x3C / 4] < 0) {
            Gwork[0x3C / 4] = 4;
        }
        RES_SoundEffect(0x1D);
    }
    if (((SlCoinPad *)PadData)->trg & 0x4000) {
        Gwork[0x44 / 4] = 0;
        if (++Gwork[0x3C / 4] >= 5) {
            Gwork[0x3C / 4] = 0;
        }
        RES_SoundEffect(0x1E);
    }
    if (((SlCoinPad *)PadData)->rep & 0x2000) {
        g = (short *)Gwork;
        off = ((int *)g)[0x3C / 4] * 2 + 0x60;
        ((int *)g)[0x44 / 4] = 0;
        p = (short *)((char *)g + off);
        if (*p < 3) {
            if (g[0x2A] + tbl.v[Gwork[0xC / 4]] < ((struct CasinoSaveWork *)SaveWork)->coin_balance) {
                (*p)++;
                RES_SoundEffect(0x1F);
            } else {
                RES_SoundEffect(0x21);
            }
        }
    }
    if (((SlCoinPad *)PadData)->rep & 0x8000) {
        g2 = (short *)Gwork;
        off2 = ((int *)g2)[0x3C / 4] * 2 + 0x60;
        ((int *)g2)[0x44 / 4] = 0;
        p2 = (short *)((char *)g2 + off2);
        if (*p2 > 0) {
            (*p2)--;
            RES_SoundEffect(0x20);
        }
    }
    if (((SlCoinPad *)PadData)->trg & 0x99F) {
        if (Gwork[0x44 / 4] == 0) {
            Gwork[0x44 / 4] = 1;
        } else {
            Gwork[0x44 / 4] = 0;
        }
    }
    ((SlCoinWorkS *)Gwork)->h[0x2A] = 0;
    for (i = 0; i < 5; i++) {
        ((SlCoinWorkS *)Gwork)->h[0x2B + i] = ((SlCoinWorkS *)Gwork)->h[0x30 + i] * tbl.v[Gwork[3]];
        ((SlCoinWorkS *)Gwork)->h[0x2A] += ((SlCoinWorkS *)Gwork)->h[0x2B + i];
    }
}

extern short D_00A0DD6A;

static void SlPrizeCheck(int line)
{
    SlPrize *p;
    short prize;

    prize = -1;
    p = &D_00A0DD8A[line];
    p->flag = 1;
    if (p->reel[0] == p->reel[1] && p->reel[0] == p->reel[2]) {
        prize = p->reel[0];
        p->flag = 0;
    }
    if (p->reel[0] == 6 && p->reel[1] == p->reel[2]) {
        prize = p->reel[1];
    }
    if (p->reel[1] == 6 && p->reel[0] == p->reel[2]) {
        prize = p->reel[0];
    }
    if (p->reel[2] == 6 && p->reel[0] == p->reel[1]) {
        prize = p->reel[0];
    }
    if (p->reel[0] == 6) {
        if (p->reel[1] == p->reel[0]) {
            prize = p->reel[2];
        }
        if (p->reel[2] == p->reel[0]) {
            prize = p->reel[1];
        }
    }
    if (p->reel[1] == 6 && p->reel[2] == p->reel[1]) {
        prize = p->reel[0];
    }
    p->mult = 0;
    if (prize != -1) {
        switch (prize) {
        case 0:
            p->pay = 5;
            break;
        case 1:
            p->pay = 10;
            p->mult = 1;
            break;
        case 2:
            p->pay = 20;
            p->mult = 1;
            break;
        case 3:
            p->pay = 20;
            p->mult = 1;
            break;
        case 4:
            p->pay = 1;
            p->mult = 2;
            break;
        case 5:
            p->pay = 30;
            break;
        case 7:
            p->pay = 30;
            p->mult = 3;
            break;
        default:
            p->pay = 0;
            break;
        }
        if (D_00A0DD6A != 0) {
            p->pay *= 2;
        }
        if (p->flag == 1) {
            p->pay = (p->pay + 1) / 2;
        }
    }
}

typedef struct {
    short reel;
    short pos;
} SlPat;
extern SlPat pat_22[15];
extern const char D_00A0BD10[];
extern const char D_00A0BD28[];
extern const char D_00A0BD40[];
typedef struct {
    char pad0[0x48];
    short row;
    char pad1[0x56 - 0x4A];
    short bet[5];
    short win[5];
    SlPrize line[5];
    char pad2[0xA8 - 0xA6];
    int total;
    char pad3[0xB0 - 0xAC];
    int effect;
} SlLineGwork;

static void SlLineCheck(void)
{
    int i;
    int j;
    int r;
    int best;
    int t;
    int m;
    SlPrize *p;
    SlPrize *p1;
    SlPrize *p2;
    SlPrize *p3;
    short *bet;

    for (i = 0; i < 5; i++) {
        p1 = &((SlLineGwork *)Gwork)->line[i];
        p1->pay = 0;
        p1->mult = 0;
        for (j = 0; j < 3; j++) {
            r = pat_22[i * 3 + j].reel;
            p1->reel[r] = SlGetPic(r, pat_22[i * 3 + j].pos, ((SlLineGwork *)Gwork)->row);
        }
    }
    p2 = D_00A0DD8A;
    for (i = 0; i < 5; i++) {
        dprintf(D_00A0BD10, i, p2->reel[0], p2->reel[1], p2->reel[2]);
        p2++;
    }
    best = 0;
    for (i = 0; i < 5; i++) {
        p3 = &((SlLineGwork *)Gwork)->line[i];
        if (((SlLineGwork *)Gwork)->bet[i] != 0) {
            SlPrizeCheck(i);
            t = p3->pay * ((SlLineGwork *)Gwork)->bet[i];
            Gwork[0xA8 / 4] += t;
            dprintf(D_00A0BD28, t, p3->mult);
            m = p3->mult;
            if (m == 1 && best < 1) {
                best = m;
            }
            if (m == 2 && best < 2) {
                best = m;
            }
            if (m == 3 && best < 3) {
                best = m;
            }
        }
    }
    if (Gwork[0xA8 / 4] != 0) {
        if (best != 0) {
            Gwork[0xB0 / 4] = best;
        } else {
            RES_SoundEffect(0x42);
        }
    }
    dprintf(D_00A0BD40, Gwork[0xA8 / 4]);
}

/* ov11:0x00a02460. Runs the complete slot line evaluation and reports
 * whether the resulting payout is nonzero. SlLineCheck is a LOCAL asm
 * sibling. */
static void SlLineCheck(void);
extern int D_00A0DDC8;

static int check_real(void)
{
    SlLineCheck();
    return D_00A0DDC8 != 0;
}

extern unsigned short PadData[];
static void GameModeChange(int mode);
extern int inter_23;

static int paid_action(void)
{
    int *g;

    g = Gwork;
    if (g[43] == 1) {
        GameModeChange(6);
        g[43] = 0;
        RES_SoundEffectStop(0x27);
        inter_23 = 0;
        return 1;
    }
    if (PadData[0x2A / 2] & 0x20) {
        AddCoin(g[42]);
        g[42] = 0;
        RES_SoundEffectStop(0x27);
    }
    if (g[42] == 0) {
        if (PadData[0x2A / 2] & 0x20) {
            inter_23 = 0;
            return 1;
        }
    } else if (g[42] > 0) {
        g[42]--;
        if (g[42] == 0) {
            RES_SoundEffectStop(0x27);
        }
        AddCoin(1);
        inter_23++;
        if (inter_23 == 1) {
            RES_SoundEffect(0x27);
        }
    }
    return 0;
}

typedef struct {
    short unmodeled_00[0x2B];
    short line_pay[5];
    short line_win[5];
} SlotLineWork;

/* Walks the five lines in display order and zeroes every line payout
 * that would push the running total past the coin balance. */
static void linepay_check(void)
{
    int order[5] = { 2, 1, 3, 0, 4 };
    int sum;
    int i;

    sum = 0;
    for (i = 0; i < 5; i++) {
        sum += ((SlotLineWork *)Gwork)->line_pay[order[i]];
        if (sum > ((struct CasinoSaveWork *)SaveWork)->coin_balance) {
            ((SlotLineWork *)Gwork)->line_pay[order[i]] = 0;
            ((SlotLineWork *)Gwork)->line_win[order[i]] = 0;
        }
    }
}

static void slot_key(void)
{
    int i;
    int stopped;
    int pressed;
    int d;
    int *p;

    stopped = 0;
    pressed = 0;
    switch (Gwork[0x50 / 4]) {
    case 0:
        if (PadData[0x2A / 2] & 0x40) {
            RES_SoundEffect(0x17);
            GameModeChange(0);
            break;
        }
        SlCoinEntry();
        if ((PadData[0x2A / 2] & 0x20) && ((short *)Gwork)[0x2A] > 0) {
            Gwork[0x44 / 4] = 0;
            Gwork[0x38 / 4] = 0;
            p = &Gwork[0x28 / 4];
            for (i = 0; i < 3; i++) {
                *p-- = 1;
            }
            Gwork[0x50 / 4] = 1;
            ((struct CasinoSaveWork *)SaveWork)->coin_balance -= ((short *)Gwork)[0x2A];
            RES_SoundEffect(0x22);
        }
        break;
    case 1:
        for (i = 0; i < 3; i++) {
            switch (Gwork[0x20 / 4 + i]) {
            case 1:
                if (Gwork[0x38 / 4] == i && (PadData[0x2A / 2] & 0x20)) {
                    Gwork[0x20 / 4 + i] = 2;
                    Gwork[0x2C / 4 + i] = 0;
                    RES_SoundEffect(0x23);
                    pressed = 1;
                }
                Gwork[0x14 / 4 + i] += RES_GetRealSpeed();
                break;
            case 2:
                if (Gwork[0x2C / 4 + i] >= 0) {
                    d = 64 - Gwork[0x14 / 4 + i] % 64;
                    if (d <= 16) {
                        Gwork[0x14 / 4 + i] += d;
                        Gwork[0x20 / 4 + i] = 0;
                        break;
                    }
                }
                Gwork[0x2C / 4 + i] += 16;
                Gwork[0x14 / 4 + i] += 16;
                break;
            case 0:
                if ((PadData[0x2A / 2] & 0x20) && Gwork[0x38 / 4] == i) {
                    RES_SoundEffect(0x26);
                }
                stopped++;
                break;
            }
        }
        if (stopped == 3) {
            if (check_real() == 0) {
                Gwork[0x50 / 4] = 0;
            } else {
                Gwork[0x50 / 4] = 2;
            }
            linepay_check();
        }
        break;
    case 2:
        if (paid_action() != 0) {
            Gwork[0x50 / 4] = 0;
        }
        break;
    }
    if (pressed == 1) {
        if (++Gwork[0x38 / 4] >= 3) {
            Gwork[0x38 / 4] = 0;
        }
    }
}

extern void xglPacketTextureTrans(void *buffer);
typedef struct {
    int f[7];
} SlPrizeTex;
typedef struct {
    int f[4];
} SlPrizeRct;
extern int mode_24;
extern int D_00A0DC28;
extern int D_00A0DC2C;
extern int D_00A0DC30;
extern const SlPrizeTex D_00A0BD68;
extern SlPrizeTex D_00A0DC38;
extern const SlPrizeRct D_00A0BD88;
extern SlPrizeRct D_00A0DC58;

int SlPrizeEffect(int arg0)
{
    switch (mode_24) {
    case 0:
        D_00A0DC38 = D_00A0BD68;
        switch (arg0) {
        case 1:
            D_00A0DC38.f[1] = 0x40;
            break;
        case 2:
            D_00A0DC38.f[1] = 0xC0;
            break;
        case 3:
            D_00A0DC38.f[1] = 0x80;
            break;
        case 4:
            D_00A0DC38.f[1] = 0;
            break;
        }
        D_00A0DC58 = D_00A0BD88;
        D_00A0DC28 = 0;
        D_00A0DC2C = -0x20;
        D_00A0DC30 = 0;
        mode_24 = 1;
        RES_SoundEffect(0x28);
        break;
    case 1: {
        int x;
        int y;

        x = D_00A0DC58.f[2] + D_00A0DC2C;
        y = D_00A0DC58.f[3] + D_00A0DC30;
        D_00A0DC58.f[2] = x;
        D_00A0DC58.f[3] = y;
        D_00A0DC58.f[0] = 0x100 - x / 2;
        D_00A0DC58.f[1] = 0x90 - y / 2;
        if (x < 0x141) {
            D_00A0DC2C = 0;
            D_00A0DC58.f[2] = 0x140;
        } else {
            D_00A0DC2C -= 8;
        }
        if (D_00A0DC58.f[3] >= 0x40) {
            D_00A0DC30 = 0;
            D_00A0DC58.f[3] = 0x40;
        } else {
            D_00A0DC30 += 1;
        }
        if (D_00A0DC2C == 0 && D_00A0DC30 == 0) {
            mode_24 = 2;
        }
        break;
    }
    case 2:
        D_00A0DC28 += 1;
        if (D_00A0DC28 >= 0x11) {
            mode_24 = 3;
            D_00A0DC2C = -4;
            D_00A0DC30 = 0x10;
        }
        break;
    case 3: {
        int x;
        int y;

        x = D_00A0DC58.f[2] + D_00A0DC2C;
        y = D_00A0DC58.f[3] + D_00A0DC30;
        D_00A0DC58.f[2] = x;
        D_00A0DC58.f[3] = y;
        D_00A0DC58.f[0] = 0x100 - x / 2;
        D_00A0DC58.f[1] = 0x90 - y / 2;
        if (x < 0) {
            D_00A0DC2C = 0;
            D_00A0DC58.f[2] = 0;
        } else {
            D_00A0DC2C -= 4;
        }
        if (D_00A0DC58.f[3] >= 0x800) {
            D_00A0DC30 = 0;
            D_00A0DC58.f[3] = 0x800;
        } else {
            D_00A0DC30 += 8;
        }
        if (D_00A0DC2C == 0 && D_00A0DC30 == 0) {
            mode_24 = 0;
            return 0;
        }
        break;
    }
    }
    SetDrawStatus(1, 0);
    xglPacketTextureTrans((void *)0x010C9800);
    SetRegAD(Pkt, 0x3F, 0);
    MakeSprite(&D_00A0DC38, &D_00A0DC58);
    SetDrawStatus(3, 0);
    xglPacketTextureTrans((void *)0x01089000);
    SetRegAD(Pkt, 0x3F, 0);
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov11/mini_g", SlChangeRealEffect);

extern int busy_35;
extern int D_00A0DDD0;
int SlChangeRealEffect(int arg0);
typedef struct {
    char pad0[0x4A];
    short h4A;
    short pad4C;
    short h4E;
    char pad50[0xAC - 0x50];
    int wAC;
    int wB0;
} SlEventWork;

static int SlEvent(int arg0)
{
    SlEventWork *g;

    if (busy_35 == 0 && arg0 == 0) {
        return 0;
    }
    switch (arg0) {
    case 1:
        if (SlPrizeEffect(arg0) != 0) {
            return 1;
        }
        D_00A0DDD0 = 6;
        break;
    case 2:
        if (SlPrizeEffect(arg0) == 0) {
            int *w = Gwork;
            ((short *)w)[0x25] = 1;
            w[0x2C] = 0;
            ((short *)w)[0x27] = RES_GetBonusTime();
            RES_SoundEffect(0x2B);
        }
        break;
    case 3:
        if (SlPrizeEffect(arg0) != 0) {
            return 1;
        }
        g = (SlEventWork *)Gwork;
        g->wAC = 1;
        g->wB0 = 0;
        RES_SoundEffect(0x2C);
        break;
    case 4:
        if (SlPrizeEffect(arg0) != 0) {
            return 1;
        }
        D_00A0DDD0 = 5;
        break;
    case 5:
        D_00A0DDD0 = 0;
        break;
    case 6:
        if (SlChangeRealEffect(1) != 0) {
            return 1;
        }
        D_00A0DDD0 = 0;
        RES_SoundEffect(0x2A);
        break;
    case 7:
        if (SlChangeRealEffect(0) != 0) {
            return 1;
        }
        D_00A0DDD0 = 0;
        RES_SoundEffect(0x2D);
        break;
    }
    return 1;
}

extern void xglPacketTextureTrans(void *buffer);
typedef struct { int w[7]; } SlHelpSprite;
typedef struct { int r[4]; } SlHelpRect;
extern const SlHelpSprite D_00A0BDE0;
extern const SlHelpRect D_00A0BE00;

static void SlHelpMode(void)
{
    SlHelpSprite sprite;
    SlHelpRect rect;

    sprite = D_00A0BDE0;
    rect = D_00A0BE00;
    SetDrawStatus(1, 0);
    xglPacketTextureTrans((void *)0x0110A000);
    SetRegAD(Pkt, 0x3F, 0);
    MakeSprite(&sprite, &rect);
}

INCLUDE_ASM("asm/nonmatchings/ov11/mini_g", slot_main);

/* ov11:0x00a034d8. Loads the five slot-machine texture resources into their
 * fixed rendering buffers through the resource loader. */
extern const char D_00A0BE10[]; /* "data\\tanaka\\slot_1.xtx" */
extern const char D_00A0BE28[]; /* "data\\tanaka\\slot_2.xtx" */
extern const char D_00A0BE40[]; /* "data\\tanaka\\base.xtx" */
extern const char D_00A0BE58[]; /* "data\\tanaka\\w1.xtx" */
extern const char D_00A0BE70[]; /* "data\\tanaka\\help_all.xtx" */

#define SLOT_1_TEX_BUFFER 0x01008000
#define SLOT_2_TEX_BUFFER 0x01048800
#define SLOT_BASE_TEX_BUFFER 0x01089000
#define SLOT_W1_TEX_BUFFER 0x010C9800
#define SLOT_HELP_TEX_BUFFER 0x0110A000

static void init_slot(void)
{
    xglCdReadFile(D_00A0BE10, (void *)SLOT_1_TEX_BUFFER, 0, 0);
    xglCdReadFile(D_00A0BE28, (void *)SLOT_2_TEX_BUFFER, 0, 0);
    xglCdReadFile(D_00A0BE40, (void *)SLOT_BASE_TEX_BUFFER, 0, 0);
    xglCdReadFile(D_00A0BE58, (void *)SLOT_W1_TEX_BUFFER, 0, 0);
    xglCdReadFile(D_00A0BE70, (void *)SLOT_HELP_TEX_BUFFER, 0, 0);
}

extern unsigned char tex_36[];

static void NgTimePrint(int time)
{
    struct { int x, y, w, h; } rect = { 0xBE, 0x23, 0xC0, 8 };

    if (time > 900) {
        time = 900;
    }
    rect.w = time * 188 / 900;
    SetDrawStatus(3, 0);
    MakeSprite(tex_36, &rect);
}

/* ov11:0x00a03610. Draws the pending nine-reel payout at its fixed screen
 * position, multiplied by the current power-of-two streak factor. pow
 * (main/tu284) computes the streak factor; litodp and dptoli (main/tu330,
 * main/tu331) are this compiler's own int<->double conversion routines.
 * They pass a double as a raw 64-bit value in an integer register (this
 * target has no hardware double support); a genuinely double-typed call
 * chain here schedules the calls' delay slots differently from the
 * original bytes, so the encoded form is required. DOUBLE_TWO is the
 * IEEE-754 double-precision encoding of the literal 2.0. */
#define DOUBLE_TWO 0x4000000000000000ULL

extern unsigned long long litodp(int value);
extern int dptoli(unsigned long long value);
extern double pow(unsigned long long base, unsigned long long exponent);
extern unsigned long long dpmul(unsigned long long a, double b);

/* pow returns a real double; the factor is handed to dptoli as its raw
 * 64-bit encoding (a double-typed argument schedules differently). */
typedef union {
    double d;
    unsigned long long bits;
} DoubleBits;

static void NgPaidPrint(void)
{
    DoubleBits factor;

    factor.d = pow(DOUBLE_TWO, litodp(Gwork[46]));
    decprint(Gwork[42] * dptoli(factor.bits), 7, 0x1B2, 0x182);
}

extern unsigned char rect_38[];
extern int count_39;

static void NgGetsDownBtnPrint(int count)
{
    int *cnt;
    int state;

    if (count >= 0) {
        count_39 = count;
    }
    cnt = &count_39;
    state = (*cnt < 0) * 2;
    if (*cnt > 0) {
        state = 1;
    }
    SetDrawStatus(3, 0);
    MakeSprite(tex_37[state], rect_38);
    if (*cnt != 0) {
        if (--*cnt == 0) {
            *cnt = -1;
        }
    }
}

extern unsigned char spinbt[3][0x1C];
extern unsigned char rect_40[];
extern int count_41;

static void NgSpinBtnPrint(int count)
{
    int *cnt;
    int state;

    if (count >= 0) {
        count_41 = count;
    }
    cnt = &count_41;
    state = (*cnt < 0) * 2;
    if (*cnt > 0) {
        state = 1;
    }
    SetDrawStatus(3, 0);
    MakeSprite(spinbt[state], rect_40);
    if (*cnt != 0) {
        if (--*cnt == 0) {
            *cnt = -1;
        }
    }
}

extern unsigned char RateLamp_Rect[9][16];
extern int anm_42;

static void NgLamp(void)
{
    int i;

    if (Gwork[46] >= 0 && ((short *)Gwork)[0xBE / 2] != 2) {
        SetDrawStatus(0, 0);
        for (i = 0; i < Gwork[46]; i++) {
            MakeSprite(pay_window[1], RateLamp_Rect[i]);
        }
        MakeSprite(pay_window[anm_42 % 2], RateLamp_Rect[i]);
        anm_42++;
    }
}

/* ov11:0x00a038c0. Selects the loaded nine-game slot background texture
 * (SLOT_2_TEX_BUFFER, the "slot_2.xtx" buffer init_slot fills), sets the
 * packet's GS register write and draw state, then emits the fixed
 * nine-game background sprite. SetRegAD and Pkt are this overlay's own
 * LOCAL data/sibling; xglPacketTextureTrans is main:0x0022c268
 * (src/main/xgl_packet.c, still INCLUDE_ASM there). */
extern void xglPacketTextureTrans(void *buffer);
static void SetRegAD(XglPacket *packet, int reg, int value);
extern unsigned char NineGameBG_Tex_43[0x1C];
extern unsigned char NineGameBG_Rect_44[0x10];

static void NineGameBG_Draw(void)
{
    xglPacketTextureTrans((void *)SLOT_2_TEX_BUFFER);
    SetRegAD(Pkt, 0x3F, 0);
    SetDrawStatus(3, 0);
    MakeSprite(NineGameBG_Tex_43, NineGameBG_Rect_44);
}

extern int seq_45;
typedef struct { int x, y; } GuidPos;
extern GuidPos GuidPt_46[11];

static void NgStopGuidPt(void)
{
    struct { int x, y, w, h; } rect;

    rect.x = GuidPt_46[((short *)Gwork)[0xBC / 2]].x;
    rect.y = GuidPt_46[((short *)Gwork)[0xBC / 2]].y;
    rect.h = 8;
    rect.w = 8;
    MakeSprite(GuidPtTex[seq_45 % 10], &rect);
    seq_45++;
}

typedef struct {
    char pad[0xBC];
    short cur;
    short mode;
    short time;
    struct NgFwdEnt { short a, b, c, d; } e[9];
} NgFwdWork;

static void NgFwdGuidPt(void)
{
    int i;
    int next;
    struct NgFwdEnt *p;
    next = 10;
    for (i = ((NgFwdWork *)Gwork)->cur + 1; i < 9; i++) {
        p = &((NgFwdWork *)Gwork)->e[i];
        if (p->c == 0) {
            next = i;
            break;
        }
    }
    if (next == 10) {
        for (i = 0; i < ((NgFwdWork *)Gwork)->cur; i++) {
            p = &((NgFwdWork *)Gwork)->e[i];
            if (p->c == 0) {
                next = i;
                break;
            }
        }
    }
    ((NgFwdWork *)Gwork)->cur = next;
}

extern GuidPos stopbtn_pos_47[9];

static void NgStopBtnPrint(void)
{
    struct { int x, y, w, h; } rect;
    unsigned char *tex;
    struct NgFwdEnt *e;
    int i;

    tex = 0;
    rect.w = 0x10;
    rect.h = 0x18;
    e = ((NgFwdWork *)Gwork)->e;
    for (i = 0; i < 9; i++) {
        switch (e[i].c) {
        case 1:
        case 3:
            tex = stopbt[2];
            break;
        case 0:
            tex = stopbt[0];
            break;
        case 2:
            tex = stopbt[1];
            break;
        }
        rect.x = stopbtn_pos_47[i].x;
        rect.y = stopbtn_pos_47[i].y;
        MakeSprite(tex, &rect);
    }
}

extern short D_00A0DDE2[];

static void NgAllStop(void)
{
    int forced_stop;
    short *reel_status;
    int remaining;

    forced_stop = 3;
    reel_status = D_00A0DDE2;
    remaining = 8;
    do {
        if (reel_status[2] == 0) {
            reel_status[2] = forced_stop;
        }
        reel_status += 4;
    } while (--remaining >= 0);
    RES_SoundEffectStop(47);
}

static void NgCheckReal(int index)
{
    short *reel = (short *)Gwork + 0x61 + index * 4;
    short *tbl = ng_realtbl[reel[0]];

    if (reel[1] >= 0x3F0) {
        reel[1] = 0;
    }
    if (tbl[reel[1] / 48] == 1) {
        Gwork[0xB8 / 4]++;
    } else {
        NgAllStop();
        Gwork[0xB8 / 4] = -1;
        Gwork[0xA8 / 4] = 0;
        ((union { short h[0x60]; } *)Gwork)->h[0xBC / 2] = 10;
    }
}

/* Returns (by value) the sprite of the picture at position pos (wrapped
 * into 0..20) on nine-game reel `reel`. */
static NgRealShape get_ngreal_shape(int reel, int pos)
{
    if (pos >= 21) {
        pos -= 21;
    }
    if (pos < 0) {
        pos += 21;
    }
    return NgReal_Tex_48[ng_realtbl[reel][pos]];
}

typedef struct { short x, y; } NgPanelPos;
extern NgPanelPos NgReal_49[9];

static void DrawNgPanel(int index, int pos, int reel)
{
    int rect[4];
    NgRealShape s;
    int q;
    int r;

    rect[2] = 48;
    rect[3] = 64;
    q = pos / rect[2];
    r = pos % rect[2];
    if (r > 24) {
        r -= 48;
        q++;
    }
    s = get_ngreal_shape(reel, q);
    rect[0] = NgReal_49[index].x + r;
    rect[1] = NgReal_49[index].y;
    MakeSprite(&s, rect);
    s = get_ngreal_shape(reel, q - 1);
    rect[2] = 24 - r;
    rect[0] += 48;
    s.w[2] = rect[2];
    if (rect[2] > 0) {
        MakeSprite(&s, rect);
    }
    s = get_ngreal_shape(reel, q + 1);
    rect[2] = r + 24;
    s.w[2] = rect[2];
    rect[0] = rect[0] - r - 72;
    s.w[0] = s.w[0] - r + 24;
    MakeSprite(&s, rect);
}

typedef struct {
    short reel;
    short pos;
    short state;
    short rest;
} NgReelState;

static void NgReal(void)
{
    NgReelState *r;
    int i;
    char *g;

    g = (char *)Gwork;
    for (i = 0; i < 9; i++) {
        if (((NgReelState *)(g + 0xC2))[i].state != 3) {
            DrawNgPanel(i, ((NgReelState *)(g + 0xC2))[i].pos, ((NgReelState *)(g + 0xC2))[i].reel);
        }
    }
    r = (NgReelState *)D_00A0DDE2;
    for (i = 0; i < 9; i++) {
        if (r->state == 0) {
            r->pos += 12;
        }
        if (r->state == 2) {
            if (r->rest > 12) {
                r->rest -= 12;
                r->pos += 12;
            } else {
                r->pos += r->rest;
                r->rest = 0;
                r->state = 1;
                NgCheckReal(i);
            }
        }
        if (r->pos >= 0x3F0) {
            r->pos = 0;
        }
        r++;
    }
}

extern void NgGetsDownBtnPrint(int);

void NgGame_GetsDown(void)
{
    int *g = Gwork;
    double factor;

    ((short *)g)[0xBE / 2] = 2;
    factor = pow(DOUBLE_TWO, litodp(g[46]));
    g[42] = dptoli(dpmul(litodp(g[42]), factor));
    g[46] = 0;
    NgGetsDownBtnPrint(0x10);
    NgAllStop();
}

static void NgGame_Select(void)
{
    int i;

    if (PadData[0x2A / 2] & 0x5000) {
        Gwork[0xB4 / 4] = 0;
        RES_SoundEffect(0x2E);
        switch (((NgFwdWork *)Gwork)->cur) {
        case 9:
            ((NgFwdWork *)Gwork)->cur = 10;
            break;
        case 10:
            ((NgFwdWork *)Gwork)->cur = 9;
            break;
        }
    }
    if (PadData[0x2A / 2] & 0x20) {
        Gwork[0xB4 / 4] = 0;
        switch (((NgFwdWork *)Gwork)->cur) {
        case 9:
            for (i = 8; i >= 0; i--) {
                ((NgFwdWork *)Gwork)->e[i].c = 0;
            }
            ((NgFwdWork *)Gwork)->mode = 1;
            NgFwdGuidPt();
            NgSpinBtnPrint(0x10);
            RES_SoundEffect(0x2F);
            break;
        case 10:
            NgGame_GetsDown();
            RES_SoundEffect(0x30);
            break;
        }
    }
    if (PadData[0x2A / 2] & 0x99F) {
        if (Gwork[0xB4 / 4] == 0) {
            Gwork[0xB4 / 4] = 1;
        } else {
            Gwork[0xB4 / 4] = 0;
        }
    }
}

extern unsigned char waku_53[2][0x1C];

static void NgGame_Key(void)
{
    struct NgFwdEnt *e;

    if (PadData[0x2C / 2] & 0x8000) {
        ((NgFwdWork *)Gwork)->cur = ptmove_50[((NgFwdWork *)Gwork)->cur][0];
        RES_SoundEffect(0x2E);
    }
    if (PadData[0x2C / 2] & 0x1000) {
        ((NgFwdWork *)Gwork)->cur = ptmove_50[((NgFwdWork *)Gwork)->cur][1];
        RES_SoundEffect(0x2E);
    }
    if (PadData[0x2C / 2] & 0x2000) {
        ((NgFwdWork *)Gwork)->cur = ptmove_50[((NgFwdWork *)Gwork)->cur][2];
        RES_SoundEffect(0x2E);
    }
    if (PadData[0x2C / 2] & 0x4000) {
        ((NgFwdWork *)Gwork)->cur = ptmove_50[((NgFwdWork *)Gwork)->cur][3];
        RES_SoundEffect(0x2E);
    }
    if (PadData[0x2A / 2] & 0x40) {
        ((NgFwdWork *)Gwork)->cur = 10;
        RES_SoundEffect(0x31);
    }
    if (PadData[0x2A / 2] & 0x20) {
        switch (((NgFwdWork *)Gwork)->cur) {
        case 0 ... 8:
            e = &((NgFwdWork *)Gwork)->e[((NgFwdWork *)Gwork)->cur];
            if (e->c == 0) {
                e->c = 2;
                e->d = 0x60 - e->b % 48;
                NgFwdGuidPt();
                RES_SoundEffect(0x32);
            } else {
                RES_SoundEffect(0x33);
            }
            break;
        case 10:
            NgGame_GetsDown();
            RES_SoundEffect(0x30);
            break;
        }
    }
}

extern unsigned short PadData[];
extern int se_51;
static void GameModeChange(int mode);

/* Pays out the pending nine-game win one coin per frame (all at once
 * while the 0x20 pad button is held), with the payout jingle running;
 * returns to the select state when nothing is left to pay. */
static void NgGame_Paid(void)
{
    int *g;

    g = Gwork;
    if (g[42] == 0) {
        RES_SoundEffectStop(0x34);
        GameModeChange(1);
        se_51 = 0;
        return;
    }
    if (se_51 == 0) {
        RES_SoundEffect(0x34);
        se_51 = 1;
    }
    if (PadData[0x2A / 2] & 0x20) {
        AddCoin(g[42]);
        g[42] = 0;
    } else {
        g[42]--;
        AddCoin(1);
    }
}

extern const SlHelpSprite D_00A0BEA0;
extern const SlHelpSprite D_00A0BEC0;
extern const SlHelpRect D_00A0BEE0;
extern const SlHelpRect D_00A0BEF0;

static void NgHelpMode(void)
{
    SlHelpSprite sprite0 = D_00A0BEA0;
    SlHelpSprite sprite1 = D_00A0BEC0;
    SlHelpRect rect0 = D_00A0BEE0;
    SlHelpRect rect1 = D_00A0BEF0;

    SetDrawStatus(1, 0);
    xglPacketTextureTrans((void *)0x0110A000);
    SetRegAD(Pkt, 0x3F, 0);
    MakeSprite(&sprite0, &rect0);
    MakeSprite(&sprite1, &rect1);
}

extern XglPacket *xglPacketGetCurrent(void);

static void NineGameMain(void)
{
    XglPacket **pkt = &Pkt;
    NgFwdWork *g;

    *pkt = xglPacketGetCurrent();
    g = (NgFwdWork *)Gwork;
    xglPacketTextureTrans((void *)0x01089000);
    NgReal();
    NgPaidPrint();
    NineGameBG_Draw();
    xglPacketTextureTrans((void *)0x01089000);
    SetRegAD(*pkt, 0x3F, 0);
    NgLamp();
    NgSpinBtnPrint(-1);
    NgGetsDownBtnPrint(-1);
    NgTimePrint(g->time);
    NgStopBtnPrint();
    NgStopGuidPt();
    switch (g->mode) {
    case 0:
        NgGame_Select();
        break;
    case 1:
        NgGame_Key();
        break;
    case 2:
        NgGame_Paid();
        break;
    }
    if (((NgFwdWork *)Gwork)->mode == 1) {
        ((NgFwdWork *)Gwork)->time--;
        if (((NgFwdWork *)Gwork)->time <= 0) {
            NgGame_GetsDown();
        }
    }
    if (Gwork[0xB4 / 4] == 1) {
        NgHelpMode();
    }
}

typedef struct {
    char pad[0xB4];
    int state;
    int win;
    short cur;
    short mode;
    short time;
    struct { short a, b, c, d; } e[9];
} NgInitWork;

static void init_ninegame(void)
{
    int i;
    int j;
    unsigned short v;

    ((NgInitWork *)Gwork)->win = 0;
    ((NgInitWork *)Gwork)->mode = 0;
    ((NgInitWork *)Gwork)->cur = 9;
    ((NgInitWork *)Gwork)->time = 900;
    for (i = 0; i < 9; i++) {
        ((NgInitWork *)Gwork)->e[i].a = -1;
        ((NgInitWork *)Gwork)->e[i].b = 0;
        ((NgInitWork *)Gwork)->e[i].c = 1;
    }
    for (i = 0; i < 9; i++) {
        v = xglSRand() % 20;
        for (j = 0; j < i; j++) {
            if (v == ((NgInitWork *)Gwork)->e[j].a) {
                j = -1;
                v = xglSRand() % 20;
            }
        }
        ((NgInitWork *)Gwork)->e[i].a = v;
    }
    NgGetsDownBtnPrint(0);
    NgSpinBtnPrint(0);
    ((NgInitWork *)Gwork)->state = 1;
}

typedef struct { int w[4]; } PoMoneyRect;
extern const PoMoneyRect D_00A0BF00;
extern const PoMoneyRect D_00A0BF10;
extern unsigned char fontlist_52[10][0x1C];
static void PoMoneyPrint(int type, int value)
{
    PoMoneyRect rect = D_00A0BF00;
    PoMoneyRect wrect = D_00A0BF10;
    int i;
    int digits;

    digits = 0;
    do {
        if (digits >= 11) {
            break;
        }
        if (value < dpow(10.0, digits)) {
            break;
        }
        digits++;
    } while (1);
    SetDrawStatus(3, 0);
    MakeSprite(waku_53[type], &wrect);
    if (digits > 0) {
        for (i = 0; i < digits; i++) {
            rect.w[0] = 0x1D8 - i * 16;
            MakeSprite(fontlist_52[(int)(value / dpow(10.0, i)) % 10], &rect);
        }
    } else {
        MakeSprite(fontlist_52, &rect);
    }
}

extern int cnt_54;
extern int D_00A0DC98[4];

static void PoCurDraw(short *pos)
{
    xglPacketTextureTrans((void *)0x013CF800);
    SetRegAD(Pkt, 0x3F, 0);
    D_00A0DC98[2] = D_00A0DC98[3] = 0x18;
    D_00A0DC98[0] = pos[0];
    D_00A0DC98[1] = pos[1];
    MakeSprite(PoCurTex[cnt_54 % 8], D_00A0DC98);
    cnt_54++;
}

extern void xglFontPrint(int x, int y, int color, const char *text);
typedef struct { int x, y, w, h; } PoWinRect;
extern const PoWinRect D_00A0BF20;
extern const char D_00A0BF30[];
extern const char D_00A0BF38[];
extern unsigned char Ltex_56[0x1C];
extern unsigned char Rtex_57[0x1C];
extern unsigned char tex_58[0x1C];

static void PoWindowMessage(const char *message, int width)
{
    PoWinRect rect;

    rect = D_00A0BF20;
    SetDrawStatus(3, 0);
    SetTest(0x5000D);
    rect.x = 0x10;
    rect.w = 8;
    MakeSprite(Ltex_56, &rect);
    rect.x = width * 9 + 0x1C;
    MakeSprite(Rtex_57, &rect);
    rect.x = 0x18;
    rect.w = width * 9 + 4;
    MakeSprite(tex_58, &rect);
    xglFontPrint(0, 0, 0, D_00A0BF30);
    xglFontPrint(0, 0, 0, D_00A0BF38);
    xglFontPrint(0x18, 0xDA, 0xFF00, message);
}

extern unsigned char tex_59[0x1C];
extern unsigned char rect_60[2][0x10];
extern unsigned char type0_tex_61[0x1C];
extern unsigned char type0_rect_62[0x10];
extern unsigned char type1_tex_63[0x1C];
extern unsigned char type1_rect_64[0x10];
extern short type0_pos_65[][2];
extern short type1_pos_66[][2];
extern int dialog_cur_67;
static void PoCurDraw(short *pos);
typedef struct {
    char pad[0x2A];
    unsigned short trg;
} PoDialogPad;

static int PoDialog(int type)
{
    int max;
    int r;

    xglPacketTextureTrans((void *)0x013CF800);
    SetRegAD(Pkt, 0x3F, 0);
    max = 2;
    if (type != 0) {
        max = 3;
    }
    if (((PoDialogPad *)PadData)->trg & 0x1000) {
        dialog_cur_67--;
        RES_SoundEffect(0x3C);
        if (dialog_cur_67 == 0) {
            dialog_cur_67 = max;
        }
    }
    if (((PoDialogPad *)PadData)->trg & 0x4000) {
        RES_SoundEffect(0x3C);
        dialog_cur_67++;
        if (dialog_cur_67 == max + 1) {
            dialog_cur_67 = 1;
        }
    }
    SetDrawStatus(3, 0);
    if (type == 0) {
        MakeSprite(tex_59, rect_60[0]);
        MakeSprite(type0_tex_61, type0_rect_62);
        PoCurDraw(type0_pos_65[dialog_cur_67 - 1]);
    } else {
        MakeSprite(tex_59, rect_60[1]);
        MakeSprite(type1_tex_63, type1_rect_64);
        PoCurDraw(type1_pos_66[dialog_cur_67 - 1]);
    }
    if (((PoDialogPad *)PadData)->trg & 0x20) {
        RES_SoundEffect(0x3F);
        r = dialog_cur_67;
        dialog_cur_67 = 1;
        return r;
    }
    if (((PoDialogPad *)PadData)->trg & 0x40) {
        RES_SoundEffect(0x40);
        dialog_cur_67 = 1;
        return -1;
    }
    return 0;
}

/* ov11:0x00a04dc0. Selects the loaded poker table-1 background texture
 * (0x0138f000, the "poker_1.xtx" buffer PokerInit below fills as
 * POKER_TABLE1_TEX_BUFFER) and emits the fixed poker background sprite. */
extern unsigned char tex_68[0x1C];
extern unsigned char rect_69[0x10];

static void PoBgDraw(void)
{
    SetDrawStatus(3, 0);
    xglPacketTextureTrans((void *)0x0138F000);
    SetRegAD(Pkt, 0x3F, 0);
    SetDrawStatus(3, 0);
    MakeSprite(tex_68, rect_69);
}

typedef struct { int lo, hi; } PoCardRange;
typedef struct {
    char pad[0x176];
    short card[5];
} PoCardDrawWork;
extern unsigned char CardTex[][0x1C];
extern void *CardBuf_70[4];
extern PoCardRange range_71[4];

static void PoCardDraw(void)
{
    int i;
    int j;
    short c;
    int base;

    base = 0;
    SetDrawStatus(3, 0);
    SetTest(0x5000D);
    for (i = 0; i < 4; i++) {
        xglPacketTextureTrans(CardBuf_70[i]);
        SetRegAD(Pkt, 0x3F, 0);
        for (j = 0; j < 5; j++) {
            c = ((PoCardDrawWork *)Gwork)->card[j];
            if (c >= range_71[i].lo && c <= range_71[i].hi) {
                MakeSprite(CardTex[c - base], CardPos[j]);
            }
        }
        base += 13;
    }
}

extern int status_73;
extern unsigned char tex_72[];
typedef struct { int x, y, w, h; } PoCardRectS;
extern PoCardRectS D_00A0DCA8[5];
extern int D_00A0DCF8;
typedef struct { int w[4]; } __attribute__((packed)) PoCardAnimeRect;

static int PoCardDrawAnime(void)
{
    int i;
    int done;

    xglPacketTextureTrans((void *)0x0128D000);
    SetRegAD(Pkt, 0x3F, 0);
    switch (status_73) {
    case 0:
        for (i = 0; i < 5; i++) {
            *(PoCardAnimeRect *)&D_00A0DCA8[i] = *(PoCardAnimeRect *)CardPos[i];
            D_00A0DCA8[i].y = D_00A0DCA8[i].y - i * 128 - 0xA0;
        }
        status_73 = 1;
    case 1:
        done = 0;
        for (i = 0; i < 5; i++) {
            MakeSprite(tex_72, &D_00A0DCA8[i]);
            if (D_00A0DCA8[i].y == 5) {
                RES_SoundEffect(0x36);
            }
            if (D_00A0DCA8[i].y < ((int *)CardPos[i])[1]) {
                D_00A0DCA8[i].y += 0x20;
            } else {
                done++;
            }
        }
        if (done == 5) {
            status_73 = 2;
            D_00A0DCF8 = 4;
        }
        break;
    case 2:
        for (i = 0; i < 5; i++) {
            MakeSprite(tex_72, &D_00A0DCA8[i]);
        }
        if (--D_00A0DCF8 == 0) {
            status_73 = 0;
            return 1;
        }
        break;
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov11/mini_g", PoBonusCardAnime);

/* ov11:0x00a05358. Emits the single fixed poker payout table sprite.
 * MakeSprite is a LOCAL asm sibling. */
static void MakeSprite(void *sprite, void *rect);
extern unsigned char tex_80[0x1C];
extern unsigned char rect_81[0x10];

static void PoRateTableDraw(void)
{
    MakeSprite(tex_80, rect_81);
}

extern int cnt_82;
extern int add_83;
extern int rate_84;
extern unsigned char tex_85[0x1C];
extern unsigned char rect_86[0x10];
extern unsigned char MarkerTex_87[0x1C];
extern unsigned char MarkerPos_88[][0x10];
extern unsigned char PtrTex_89[0x1C];
extern unsigned char PtrPos_90[][0x10];

static void PoBonusBGDraw(void)
{
    int i;

    xglPacketTextureTrans((void *)0x013CF800);
    SetRegAD(Pkt, 0x3F, 0);
    MakeSprite(tex_85, rect_86);
    if (cnt_82 < 8) {
        add_83 = 8;
    } else {
        add_83 = -8;
    }
    cnt_82++;
    if (cnt_82 == 0x10) {
        cnt_82 = 0;
    }
    rate_84 += add_83;
    SetDrawStatus(6, 0x40);
    for (i = 0; i < Gwork[0x194 / 4]; i++) {
        MakeSprite(MarkerTex_87, MarkerPos_88[i]);
    }
    for (i = 0; i < Gwork[0x194 / 4] && i != 5; i++) {
        if (i == Gwork[0x194 / 4] - 1) {
            SetDrawStatus(6, rate_84);
        } else {
            SetDrawStatus(6, 0x30);
        }
        MakeSprite(PtrTex_89, PtrPos_90[i]);
    }
}

typedef struct {
    char pad0[0x10C];
    short card[53];
    short hand[5];
    char pad1[0x198 - 0x180];
    short flag[5];
} PoResetCard_view;

static void PoResetCard(void)
{
    int i;

    for (i = 52; i >= 0; i--) {
        ((PoResetCard_view *)Gwork)->card[i] = 0;
    }
    for (i = 0; i < 5; i++) {
        ((PoResetCard_view *)Gwork)->hand[i] = 0x35;
        ((PoResetCard_view *)Gwork)->flag[i] = 1;
    }
}

extern int wait_91;
typedef struct {
    char pad[0x10C];
    short used[53];
    short card[17];
    short flag[5];
} PoShuffleWork;

static int PoShuffleCard(void)
{
    int i;
    int count;
    int c;

    c = 0;
    count = 0;
    for (i = 0; i < 5; i++) {
        if (((PoShuffleWork *)Gwork)->flag[i] == 1) {
            count++;
        }
    }
    if (count == 0) {
        return 1;
    }
    if (--wait_91 != 0) {
        return 0;
    }
    for (i = 0; i < 5; i++) {
        if (((PoShuffleWork *)Gwork)->flag[i] == 1) {
            break;
        }
    }
    for (;;) {
        if (Gwork[0x190 / 4] == 0) {
            c = (unsigned short)(xglSRand() % 52);
        } else {
            switch (i) {
            case 0:
                c = 0;
                break;
            case 1:
                c = 9;
                break;
            case 2:
                c = 10;
                break;
            case 3:
                c = 11;
                break;
            case 4:
                c = 12;
                break;
            }
        }
        if (((PoShuffleWork *)Gwork)->used[c] != 1) {
            ((PoShuffleWork *)Gwork)->card[i] = c;
            ((PoShuffleWork *)Gwork)->used[c] = 1;
            break;
        }
    }
    RES_SoundEffect(0x41);
    ((PoShuffleWork *)Gwork)->flag[i] = 0;
    wait_91 = 8;
    return 0;
}

extern unsigned char PoSelecterTex[2][0x1C];
extern unsigned char PoSelecterPos[6][0x10];

static void PoSelectMarkDraw(void)
{
    short *g;
    int i;

    xglPacketTextureTrans((void *)0x013CF800);
    SetRegAD(Pkt, 0x3F, 0);
    SetDrawStatus(1, 0);
    g = (short *)Gwork;
    for (i = 0; i < 5; i++) {
        MakeSprite(PoSelecterTex[g[0xCC + i]], PoSelecterPos[i]);
    }
    MakeSprite(PoSelecterTex[1], PoSelecterPos[5]);
}

extern unsigned short PadData[];
extern int cur_92;
extern int mode_93;
extern short chgpos_95[2];
static int PoKey(void)
{
    short *flag;

    if (PadData[0x2A / 2] & 0x1000) {
        RES_SoundEffect(0x3E);
        mode_93 = 0;
    }
    if (PadData[0x2A / 2] & 0x4000) {
        RES_SoundEffect(0x3E);
        mode_93 = 1;
    }
    if (mode_93 == 1) {
        PoCurDraw(chgpos_95);
        if (PadData[0x2A / 2] & 0x20) {
            RES_SoundEffect(0x37);
            cur_92 = 0;
            mode_93 = 0;
            return 1;
        }
        return 0;
    }
    if (PadData[0x2C / 2] & 0x2000) {
        RES_SoundEffect(0x3E);
        cur_92++;
        if (cur_92 > 4) {
            cur_92 = 0;
        }
    }
    if (PadData[0x2C / 2] & 0x8000) {
        RES_SoundEffect(0x3E);
        cur_92--;
        if (cur_92 < 0) {
            cur_92 = 4;
        }
    }
    if (PadData[0x2A / 2] & 0x20) {
        RES_SoundEffect(0x37);
        flag = &D_00A0DEB8[cur_92];
        if (*flag == 0) {
            *flag = 1;
        } else {
            *flag = 0;
        }
    }
    PoCurDraw(selpos_94[cur_92]);
    return 0;
}

extern int status_96;
typedef struct {
    char pad[0x176];
    short card[17];
    short flag[5];
} PoCardWork;

static void PoCardFlip(void)
{
    short *g;
    int i;
    int count;

    count = 0;
    for (i = 0; i < 5; i++) {
        if (((PoCardWork *)Gwork)->flag[i] == 1) {
            ((PoCardWork *)Gwork)->card[i] = 0x35;
            count++;
        }
    }
    if (count == 5) {
        status_96++;
    } else {
        status_96 = 0;
    }
    if (status_96 >= 0x20) {
        Gwork[0x190 / 4] = 1;
        g = (short *)Gwork;
        for (i = 0; i < 0x35; i++) {
            g[0x86 + i] = 0;
        }
    } else {
        Gwork[0x190 / 4] = 0;
    }
}

/* ov11:0x00a05a78. Maps a 0..0x33 standard 52-card value (13 ranks per
 * suit) to its suit index. Callers (PoChk4 and PokerMain) read cardValue
 * as a signed `lh` from the sorted hand in Gwork; any value outside
 * 0..0x33 reports 0, matching the pre-switch init below. */
static int PoGetMark(int cardValue)
{
    int mark;

    mark = 0;
    switch (cardValue) {
    case 0x0:
    case 0x1:
    case 0x2:
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
    case 0x8:
    case 0x9:
    case 0xA:
    case 0xB:
    case 0xC:
        mark = 0;
        break;
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
        mark = 1;
        break;
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
        mark = 2;
        break;
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
        mark = 3;
        break;
    }
    return mark;
}

static short PoGetBase(int cardValue);

static short PoGetBase(int cardValue)
{
    short base;

    base = 0;
    if (cardValue != 1) {
        if (cardValue >= 2) {
            switch (cardValue) {
            case 2:
                base = 26;
                break;
            case 3:
                base = 39;
                break;
            }
        }
    } else {
        base = 13;
    }
    return base;
}

extern void dprintf(const char *fmt, ...);
extern const char D_00A0C030[];
extern const char D_00A0C038[];
typedef struct {
    char pad[0x176];
    short card[22];
    short sorted[5];
} PoSortWork;

static void PoSortCard(void)
{
    int i;
    int j;
    short t;

    for (i = 0; i < 5; i++) {
        ((PoSortWork *)Gwork)->sorted[i] = ((PoSortWork *)Gwork)->card[i];
    }
    for (j = 4; j != 0; j--) {
        for (i = 0; i < j; i++) {
            t = ((PoSortWork *)Gwork)->sorted[i];
            if (((PoSortWork *)Gwork)->sorted[i + 1] < t) {
                ((PoSortWork *)Gwork)->sorted[i] = ((PoSortWork *)Gwork)->sorted[i + 1];
                ((PoSortWork *)Gwork)->sorted[i + 1] = t;
            }
        }
    }
    for (i = 0; i < 5; i++) {
        dprintf(D_00A0C030, ((PoSortWork *)Gwork)->sorted[i]);
    }
    dprintf(D_00A0C038);
}

extern const char D_00A0C040[];
extern const char D_00A0C048[];
extern const char D_00A0C050[];
extern const char D_00A0C058[];

static int PoMultiChk(void)
{
    int count;
    int i;
    int j;
    short v;
    short c;

    count = 0;
    for (i = 0; i < 5; i++) {
        v = ((PoSortWork *)Gwork)->sorted[i];
        for (j = 0; j < 5; j++) {
            if (i != j) {
                c = ((PoSortWork *)Gwork)->sorted[j];
                if (c == v + 13) {
                    count++;
                    dprintf(D_00A0C040);
                }
                if (c == v + 26) {
                    count++;
                    dprintf(D_00A0C048);
                }
                if (c == v + 39) {
                    count++;
                    dprintf(D_00A0C050);
                }
            }
        }
        dprintf(D_00A0C058);
    }
    dprintf(D_00A0C038);
    switch (count) {
    case 6:
        return 5;
    case 4:
        return 4;
    case 3:
        return 3;
    case 2:
        return 2;
    case 1:
        return 1;
    default:
        return 0;
    }
}

struct PokerHandWork {
    unsigned char unmodeled_00[0x1A2];
    short cards[5];
};

static int PoFlushChk(void)
{
    struct PokerHandWork *hand;
    short *card;
    int suit;
    int index;

    hand = (struct PokerHandWork *)Gwork;
    card = &hand->cards[1];
    suit = PoGetMark(hand->cards[0]);
    for (index = 1; index < 5; index++) {
        if (suit != PoGetMark(*card++)) {
            return 0;
        }
    }
    return 1;
}

/* PoMultiChk classifies the sorted hand's rank multiplicities and
 * PoFlushChk classifies same-suit runs. dprintf is the overlay's debug
 * printf. */
static int PoMultiChk(void);
static int PoFlushChk(void);
extern void dprintf(const char *fmt, ...);

/* ov11:0x00a05de0. Reports category 1, printing D_00A0C078 through
 * dprintf, when PoMultiChk's multiplicity equals 1. */
extern const char D_00A0C078[];

static int PoChk1(void)
{
    if (PoMultiChk() == 1) {
        dprintf(D_00A0C078);
        return 1;
    }
    return 0;
}

/* ov11:0x00a05e20. Reports category 2, printing D_00A0C088 through
 * dprintf, when PoMultiChk's multiplicity equals 2. */
extern const char D_00A0C088[];

static int PoChk2(void)
{
    if (PoMultiChk() == 2) {
        dprintf(D_00A0C088);
        return 2;
    }
    return 0;
}

/* ov11:0x00a05e60. Reports category 3, printing D_00A0C098 through
 * dprintf, when PoMultiChk's multiplicity equals 3. */
extern const char D_00A0C098[];

static int PoChk3(void)
{
    if (PoMultiChk() == 3) {
        dprintf(D_00A0C098);
        return 3;
    }
    return 0;
}

extern const char D_00A0C0A8[];

static int PoChk4(void)
{
    int v[5];
    int i;
    int j;
    int k;
    int t;
    short c;

    for (i = 0; i < 5; i++) {
        c = ((PoSortWork *)Gwork)->sorted[i];
        v[i] = c - PoGetBase(PoGetMark(c));
    }
    for (j = 4; j != 0; j--) {
        for (k = 0; k < j; k++) {
            t = v[k];
            if (v[k + 1] < t) {
                v[k] = v[k + 1];
                v[k + 1] = t;
            }
        }
    }
    if (v[1] == v[0] + 1 && v[2] == v[0] + 2 && v[3] == v[0] + 3 && v[4] == v[0] + 4) {
        dprintf(D_00A0C0A8);
        return 4;
    }
    return 0;
}

/* ov11:0x00a05fb0. Substitutes category 5 and prints D_00A0C0B8 through
 * dprintf whenever PoFlushChk returns nonzero; PoFlushChk's own nonzero
 * value is discarded rather than reused as the category. */
extern const char D_00A0C0B8[];

static int PoChk5(void)
{
    int category = PoFlushChk();

    if (category != 0) {
        dprintf(D_00A0C0B8);
        category = 5;
    }
    return category;
}

/* ov11:0x00a05fe8. Reports category 6, printing D_00A0C0C8 through
 * dprintf, when PoMultiChk's multiplicity equals 4. */
extern const char D_00A0C0C8[];

static int PoChk6(void)
{
    if (PoMultiChk() == 4) {
        dprintf(D_00A0C0C8);
        return 6;
    }
    return 0;
}

/* ov11:0x00a06028. Reports category 7, printing D_00A0C0D8 through
 * dprintf, when PoMultiChk's multiplicity equals 5. */
extern const char D_00A0C0D8[];

static int PoChk7(void)
{
    if (PoMultiChk() == 5) {
        dprintf(D_00A0C0D8);
        return 7;
    }
    return 0;
}

extern const char D_00A0C0E8[];

static int PoChk8(void)
{
    struct PokerHandWork *hand;
    int first;

    if (PoFlushChk() == 0) {
        return 0;
    }
    hand = (struct PokerHandWork *)Gwork;
    first = hand->cards[0];
    if (hand->cards[1] != first + 1 ||
        hand->cards[2] != first + 2 ||
        hand->cards[3] != first + 3) {
        return 0;
    }
    if (hand->cards[4] == first + 4) {
        dprintf(D_00A0C0E8);
        return 8;
    }
    return 0;
}

/* ov11:0x00a060e8. Reports category 9 when PoFlushChk finds a flush and
 * the sorted hand's ranks, read from the Gwork work block, form a
 * consecutive run starting at the lowest card: base, base+9, base+0xA,
 * base+0xB, base+0xC. PoGetBase is a LOCAL asm sibling; the double
 * PoGetBase(PoGetBase(base)) round trip is the original's own check, not
 * a simplification. Any other outcome, including a flush that is not
 * this run, reports 0. */
#define POKER_HAND_RANK_INDEX 0xD1 /* short index; 0x1A2 / sizeof(short) */

extern const char D_00A0C100[];

static int PoChk9(void)
{
    short *hand;
    short base;

    if (PoFlushChk() == 0) {
        return 0;
    }

    hand = (short *)Gwork;
    base = hand[POKER_HAND_RANK_INDEX];
    if (base == PoGetBase(PoGetBase(base))) {
        if (hand[POKER_HAND_RANK_INDEX + 1] == base + 9 &&
            hand[POKER_HAND_RANK_INDEX + 2] == base + 0xA &&
            hand[POKER_HAND_RANK_INDEX + 3] == base + 0xB &&
            hand[POKER_HAND_RANK_INDEX + 4] == base + 0xC) {
            dprintf(D_00A0C100);
            return 9;
        }
    }
    return 0;
}

/* ov11:0x00a06190. Sorts the hand, then checks hand-rank categories from
 * nine downward until one reports nonzero, plays one of three sound tiers
 * for the result, and reports it through the same debug printf as the
 * PoChk* family (D_00A0C120, "result = %d\n").
 *
 * The nested PoChk9..PoChk1 chain and the shared-tail-block `goto
 * defaultSound` are the shape recorded as CP-0254 in
 * config/compiler-patterns.json: a flat "if (category == 0) category =
 * PoChkN();" per-line chain lets cc1 2.96's range analysis fold the
 * repeated zero tests together with the sound-tier comparisons instead of
 * keeping each PoChkN call, and a fully structured if/else-if/else sound
 * dispatch duplicates the shared RES_SoundEffect(0x3A) call into two
 * physical blocks instead of the original's single call reached from both
 * the category<=0 and category>=0xA paths. */
static void PoSortCard(void);
static int PoChk4(void);
static int PoChk8(void);
static int PoChk9(void);
static void RES_SoundEffect(int soundId);
extern const char D_00A0C120[];

static int PoCardCheck(void)
{
    int category;

    PoSortCard();
    category = PoChk9();
    if (category == 0) {
        category = PoChk8();
        if (category == 0) {
            category = PoChk7();
            if (category == 0) {
                category = PoChk6();
                if (category == 0) {
                    category = PoChk5();
                    if (category == 0) {
                        category = PoChk4();
                        if (category == 0) {
                            category = PoChk3();
                            if (category == 0) {
                                category = PoChk2();
                                if (category == 0) {
                                    category = PoChk1();
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (category > 0) {
        if (category >= 6) {
            if (category < 0xA) {
                RES_SoundEffect(0x39);
            } else {
                goto defaultSound;
            }
        } else {
            RES_SoundEffect(0x38);
        }
    } else {
defaultSound:
        RES_SoundEffect(0x3A);
    }

    dprintf(D_00A0C120, category);
    return category;
}

/* ov11:0x00a062b8. Draws the poker result lamp sprite for hand category
 * index category, selecting its fixed rect from a shared row of 0x10-byte
 * rects (the same tex/rect MakeSprite pair shape used throughout this TU;
 * the row's entry count beyond this stride is not separately evidenced). */
extern unsigned char tex_98[0x1C];
static void PoResultLamp(int category)
{
    SetDrawStatus(0, 0);
    MakeSprite(tex_98, result_rect_97[category]);
    SetDrawStatus(3, 0);
}

/* ov11:0x00a06308. Stores the caller-supplied poker sequence state in the
 * global sequence word. */
extern int D_00A0DEA8;

static void PoSeqChange(int sequenceState)
{
    D_00A0DEA8 = sequenceState;
}

/* ov11:0x00a06318. Looks up the currently selected bet level's ready
 * message and message-box width from two fixed four-entry tables and
 * forwards both to the message window renderer. D_00A0DD2C is the level
 * index level_select stores (0..3); the four strings are the "Play N
 * coins" prompts for the four bet levels. The compiled form copies each
 * table onto the stack as a 16-byte aggregate before indexing it, which is
 * why each table is typed as a one-member array struct rather than a bare
 * array (a bare array cannot be copied by value in C). PoWindowMessage is
 * a LOCAL asm sibling. */
typedef struct {
    const char *text[4];
} PokerReadyMessageTable;

typedef struct {
    int width[4];
} PokerReadyWidthTable;

static void PoWindowMessage(const char *message, int width);
extern int D_00A0DD2C;
extern const PokerReadyMessageTable D_00A0C1B8; /* "Play 5/10/30/100 coins" */
extern const PokerReadyWidthTable D_00A0C1C8;

static void PokerReadyMes(void)
{
    PokerReadyMessageTable readyMessage = D_00A0C1B8;
    PokerReadyWidthTable readyWidth = D_00A0C1C8;

    PoWindowMessage(readyMessage.text[D_00A0DD2C], readyWidth.width[D_00A0DD2C]);
}

INCLUDE_ASM("asm/nonmatchings/ov11/mini_g", PokerMain);

/* ov11:0x00a06c08. Resets five Gwork work-state words used by the poker
 * minigame: its only caller, GameModeChange, uses it to clear the main
 * sequence, display flags, bonus mode and bonus round index before a
 * poker session starts. Field identities beyond their byte offsets are
 * not separately evidenced. */
static void PoInitWork(void)
{
    Gwork[96] = 0;
    Gwork[97] = 0;
    Gwork[100] = 0;
    Gwork[101] = 0;
    Gwork[98] = 0;
}

/* ov11:0x00a06c28. Loads the six poker minigame textures (four suits plus
 * two poker-table backgrounds; the "data\\tanaka\\*.xtx" paths are these
 * D_00A0Cxxx strings themselves) into fixed EE main RAM staging addresses,
 * following the same (void *)FIXED_ADDRESS pattern as src/main/game_over.c's
 * IMAGE_LOAD_BUFFER. */
extern const char D_00A0C270[]; /* "data\\tanaka\\spade.xtx" */
extern const char D_00A0C288[]; /* "data\\tanaka\\clover.xtx" */
extern const char D_00A0C2A0[]; /* "data\\tanaka\\heart.xtx" */
extern const char D_00A0C2B8[]; /* "data\\tanaka\\dia.xtx" */
extern const char D_00A0C2D0[]; /* "data\\tanaka\\poker_1.xtx" */
extern const char D_00A0C2E8[]; /* "data\\tanaka\\poker_2.xtx" */

#define POKER_SPADE_TEX_BUFFER  0x0128D000
#define POKER_CLOVER_TEX_BUFFER 0x012CD800
#define POKER_HEART_TEX_BUFFER  0x0130E000
#define POKER_DIA_TEX_BUFFER    0x0134E800
#define POKER_TABLE1_TEX_BUFFER 0x0138F000
#define POKER_TABLE2_TEX_BUFFER 0x013CF800

static void PokerInit(void)
{
    xglCdReadFile(D_00A0C270, (void *)POKER_SPADE_TEX_BUFFER, 0, 0);
    xglCdReadFile(D_00A0C288, (void *)POKER_CLOVER_TEX_BUFFER, 0, 0);
    xglCdReadFile(D_00A0C2A0, (void *)POKER_HEART_TEX_BUFFER, 0, 0);
    xglCdReadFile(D_00A0C2B8, (void *)POKER_DIA_TEX_BUFFER, 0, 0);
    xglCdReadFile(D_00A0C2D0, (void *)POKER_TABLE1_TEX_BUFFER, 0, 0);
    xglCdReadFile(D_00A0C2E8, (void *)POKER_TABLE2_TEX_BUFFER, 0, 0);
}

static void init_ninegame(void);
typedef struct {
    int pad00[2];
    int mode;
    int level;
    int pad10;
    int ab[6];
    int pad2C[3];
    int w38;
    int pad3C;
    int w40;
    int w44;
    short h48;
    short h4A;
    short h4C;
    short pad4E;
    int w50;
    short h54;
    short s56[10];
} GameModeWork;

static void GameModeChange(int mode)
{
    GameModeWork *g;
    int i;

    switch (mode) {
    case 0:
        ((GameModeWork *)Gwork)->level = 0;
        break;
    case 6:
        init_ninegame();
        break;
    case 1:
        if (((GameModeWork *)Gwork)->mode != 6) {
            g = (GameModeWork *)Gwork;
            for (i = 0; i < 5; i++) {
                g->s56[i + 5] = 0;
                g->s56[i] = 0;
            }
            ((GameModeWork *)Gwork)->w44 = 1;
        }
        for (i = 0; i < 3; i++) {
            ((GameModeWork *)Gwork)->ab[i] = 0;
            ((GameModeWork *)Gwork)->ab[i + 3] = 0;
        }
        g = (GameModeWork *)Gwork;
        g->w38 = 0;
        g->w50 = 0;
        g->h54 = 0;
        g->w40 = 0;
        g->h4A = 0;
        g->h4C = 0;
        g->h48 = 0;
        break;
    case 2:
        PoInitWork();
        break;
    case 3:
        break;
    case 4:
        break;
    case 5:
        break;
    }
    ((GameModeWork *)Gwork)->mode = mode;
}

/* ov11:0x00a06df0. Builds a fixed 0xB8x0x70 rect at the caller-supplied
 * (y, x) origin on the stack and draws the submenu window sprite into it;
 * this is the same {y, x, width, height} rect layout MakeSprite's other
 * (data-supplied) rect arguments use elsewhere in this TU. windowId itself
 * is not read here; level_select and exchange_select forward it on to
 * submenu_select right after calling sub_window. */
typedef struct {
    int y;
    int x;
    int width;
    int height;
} SubWindowRect;

extern unsigned char SubWindow_Tex[0x1C];

static void sub_window(int windowId, int y, int x)
{
    SubWindowRect rect;

    SetDrawStatus(1, 0);
    rect.y = y;
    rect.x = x;
    rect.width = 0xB8;
    rect.height = 0x70;
    MakeSprite(SubWindow_Tex, &rect);
}

extern int anm_100;

static int submenu_select(int windowId, int y, int x, int iconCount,
                          void *icons, int *selected)
{
    SubWindowRect rect;
    SubWindowRect cursor;
    int i;
    int cx;

    cx = 0;
    sub_window(windowId, y, x);
    SetDrawStatus(1, 0);
    rect.y = y + 0x34;
    rect.width = 0x48;
    rect.height = 0x10;
    for (i = 0; i < iconCount; i++) {
        rect.x = x + 0xC + i * 0x18;
        if (i == *selected) {
            cx = x + 0xC + i * 0x18;
        }
        MakeSprite((unsigned char *)icons + i * 0x1C, &rect);
    }
    cursor.y = rect.y - 0x10;
    cursor.x = cx;
    cursor.height = 0x10;
    cursor.width = 0x10;
    MakeSprite(Cursor_Tex[anm_100 % 8], &cursor);
    anm_100++;
    if (PadData[0x2C / 2] & 0x1000) {
        (*selected)--;
        RES_SoundEffect(5);
    }
    if (PadData[0x2C / 2] & 0x4000) {
        (*selected)++;
        RES_SoundEffect(6);
    }
    if (*selected > iconCount - 1) {
        *selected = 0;
    }
    if (*selected < 0) {
        *selected = iconCount - 1;
    }
    if (PadData[0x2A / 2] & 0x20) {
        return 1;
    }
    return 0;
}

/* ov11:0x00a07000. Draws a four-choice submenu bound to the shared game
 * level selection. sub_window's own body (asm/nonmatchings/ov11/mini_g/
 * sub_window.s) overwrites its first argument before using it, so windowId
 * only reaches submenu_select; y and x are the row/column submenu_select
 * stores for the first icon (offset there by 0x34 and 0xC before laying the
 * remaining icons out, per its and exchange_select's own disassembly).
 * level_select's own tail call (`j submenu_select`) is why its return value
 * is submenu_select's, and menu_mode branches on it (`beqz $2` right after
 * `jal level_select`). */
static void sub_window(int windowId, int y, int x);
static int submenu_select(int windowId, int y, int x, int iconCount,
                          void *icons, int *selected);
extern unsigned char Level_Tex[];
extern int D_00A0DD2C;

static int level_select(int windowId, int y, int x)
{
    sub_window(windowId, y, x);
    return submenu_select(windowId, y, x, 4, Level_Tex, &D_00A0DD2C);
}

/* ov11:0x00a07060. Draws a three-choice submenu bound to the shared exchange
 * mode selection, the same sub_window()+submenu_select() shape level_select
 * uses above; exchange_select's own tail call (`j submenu_select`) is why its
 * return value is submenu_select's. */
extern unsigned char ExChange_Tex[];
extern int D_00A0DD24;

static int exchange_select(int windowId, int y, int x)
{
    sub_window(windowId, y, x);
    return submenu_select(windowId, y, x, 3, ExChange_Tex, &D_00A0DD24);
}

extern XglPacket *xglPacketGetCurrent(void);
static void GameModeChange(int mode);
extern int curpos_101;
extern int cur_int_102;
extern int menu_mode_103;
extern unsigned char EntryBG_Tex[0x1C];
extern unsigned char EntryBG_Rect[0x10];
static int menu_mode(void)
{
    XglPacket **pkt = &Pkt;

    *pkt = xglPacketGetCurrent();
    xglPacketTextureTrans((void *)0x0114A800);
    SetRegAD(Pkt, 0x3F, 0);
    SetDrawStatus(3, 0);
    SetTest(0x5000D);
    MakeSprite(EntryBG_Tex, EntryBG_Rect);
    xglPacketTextureTrans((void *)0x0118B000);
    SetRegAD(Pkt, 0x3F, 0);

    switch (menu_mode_103) {
    case 0:
        if (PadData[0x2A / 2] & 0x1000) {
            curpos_101 = 0;
            RES_SoundEffect(0);
        }
        if (PadData[0x2A / 2] & 0x8000) {
            curpos_101 = 1;
            RES_SoundEffect(1);
        }
        if (PadData[0x2A / 2] & 0x2000) {
            curpos_101 = 2;
            RES_SoundEffect(2);
        }
        if (PadData[0x2A / 2] & 0x4000) {
            curpos_101 = 3;
            RES_SoundEffect(3);
        }
        if (PadData[0x2A / 2] & 0x20) {
            menu_mode_103 = 2;
            RES_SoundEffect(4);
        }
        MakeSprite(Cursor_Tex[cur_int_102 % 8], Cursor_Rect[curpos_101]);
        break;
    case 1:
        break;
    case 2:
        switch (curpos_101) {
        case 0:
            if (exchange_select((int)Pkt, 0x97, 0x75)) {
                switch (D_00A0DD24) {
                case 0:
                    GameModeChange(3);
                    RES_SoundEffect(9);
                    break;
                case 1:
                    GameModeChange(4);
                    RES_SoundEffect(10);
                    break;
                case 2:
                    GameModeChange(5);
                    RES_SoundEffect(11);
                    break;
                }
            }
            break;
        case 1:
            if (level_select((int)Pkt, 0x21, 0xF3)) {
                GameModeChange(1);
                RES_SoundEffect(12);
                menu_mode_103 = 0;
            }
            break;
        case 2:
            if (level_select((int)Pkt, 0x145, 0xF3)) {
                GameModeChange(2);
                RES_SoundEffect(13);
                menu_mode_103 = 0;
            }
            break;
        case 3:
            return 1;
        }
        if (PadData[0x2A / 2] & 0xA040) {
            RES_SoundEffect(8);
            menu_mode_103 = 0;
        }
        break;
    }
    cur_int_102++;
    return 0;
}

/* ov11:0x00a07410. Loads the menu background and window textures, then
 * resets the menu mode and game level selection in the Gwork work
 * block. */
extern const char D_00A0C320[]; /* "data\\tanaka\\BG.xtx" */
extern const char D_00A0C338[]; /* "data\\tanaka\\window.xtx" */

#define MENU_BG_TEX_BUFFER 0x0114A800
#define MENU_WINDOW_TEX_BUFFER 0x0118B000
#define MENU_LEVEL_INDEX 8 /* short index; 0x10 / sizeof(short) */

static void init_menu_mode(void)
{
    xglCdReadFile(D_00A0C320, (void *)MENU_BG_TEX_BUFFER, 0, 0);
    xglCdReadFile(D_00A0C338, (void *)MENU_WINDOW_TEX_BUFFER, 0, 0);

    Gwork[3] = 0;
    ((short *)Gwork)[MENU_LEVEL_INDEX] = 0;
}

/* ov11:0x00a07470. Loads the casino test-mode background texture. */
extern const char D_00A0BE40[]; /* "data\\tanaka\\base.xtx" */

#define BASE_TEX_BUFFER            0x01089000
#define TEST_MODE_BASE_TEX_BUFFER  0x01593000

static void init_test_mode(void)
{
    xglCdReadFile(D_00A0BE40, (void *)TEST_MODE_BASE_TEX_BUFFER, 0, 0);
}

/* ov11:0x00a074a0. Lazily loads the test-mode background texture on first
 * entry (flg_104 latches this), then selects it and draws the fixed
 * test-mode background sprite. xglPacketGetCurrent is main:0x0022c450
 * (src/main/xgl_packet.c). */
extern XglPacket *xglPacketGetCurrent(void);
extern unsigned char BGTEST_Tex[0x1C];
extern unsigned char BGTEST_Rect[0x10];
extern int flg_104;

static void test_mode(void)
{
    if (flg_104 == 0) {
        init_test_mode();
        flg_104 = 1;
    }
    Pkt = xglPacketGetCurrent();
    xglPacketTextureTrans((void *)TEST_MODE_BASE_TEX_BUFFER);
    SetRegAD(Pkt, 0x3F, 0);
    SetDrawStatus(3, 0);
    MakeSprite(BGTEST_Tex, BGTEST_Rect);
}

extern unsigned char tex_105[11][0x1C];

static void DecPrint2(int value, int digits, int x, int y, int sign)
{
    int i;
    int rect[4];

    if (digits < 0) {
        digits = 0;
    loop:
        if (digits < 11) {
            if (value >= dpow(10.0, digits)) {
                digits++;
                goto loop;
            }
        }
    }
    rect[2] = 14;
    rect[3] = 24;
    if (digits > 0) {
        for (i = 0; i < digits; i++) {
            rect[0] = x - i * 14;
            rect[1] = y;
            MakeSprite(tex_105[(int)(value / dpow(10.0, i)) % 10], rect);
        }
    } else {
        rect[0] = x;
        rect[1] = y;
        MakeSprite(tex_105, rect);
    }
    if (sign) {
        rect[0] = x + 16;
        rect[1] = y;
        MakeSprite(tex_105[10], rect);
    }
}

extern int anm_107;

static void UtlCurPrint(int x, int y)
{
    int rect[4];

    xglPacketTextureTrans((void *)0x0118B000);
    SetRegAD(Pkt, 0x3F, 0);
    rect[2] = rect[3] = 0x18;
    rect[0] = x;
    rect[1] = y;
    MakeSprite(Tex_106[anm_107 % 8], rect);
    anm_107++;
}

INCLUDE_ASM("asm/nonmatchings/ov11/mini_g", coin_main);

/* ov11:0x00a07c38. Loads the casino coin-exchange minigame's background and
 * exchange textures (D_00A0BE40 is the same "base.xtx" path init_test_mode
 * uses, into a different fixed buffer address here). */
extern const char D_00A0C390[]; /* "data\\tanaka\\exchange2.xtx" */

#define COIN_EXCHANGE_TEX_BUFFER 0x011CB800

static void init_coin(void)
{
    xglCdReadFile(D_00A0BE40, (void *)BASE_TEX_BUFFER, 0, 0);
    xglCdReadFile(D_00A0C390, (void *)COIN_EXCHANGE_TEX_BUFFER, 0, 0);
}

/* ov11:0x00a07c80. Loads the casino shop's exchange and background textures
 * (D_00A0BE40 "base.xtx" again, into the same fixed buffer init_coin uses). */
extern const char D_00A0C3B0[]; /* "data\\tanaka\\exchange1.xtx" */

#define SHOP_EXCHANGE_TEX_BUFFER 0x0120C000

static void init_shop(void)
{
    xglCdReadFile(D_00A0C3B0, (void *)SHOP_EXCHANGE_TEX_BUFFER, 0, 0);
    xglCdReadFile(D_00A0BE40, (void *)BASE_TEX_BUFFER, 0, 0);
}

extern int xglFlagsGet1(int flag);
extern int MenuNum;
extern int shpmenu_work[];

static void init_shpmenu(void)
{
    int i;
    int flag;
    int *p;

    MenuNum = 0;
    p = shpmenu_work;
    for (i = 0; i < 51; i++) {
        flag = 0;
        switch (i) {
        case 36:
        case 37:
            if (xglFlagsGet1(0x73) == 1) {
                flag = 1;
            }
            break;
        case 38:
            if (xglFlagsGet1(0x12D) == 1) {
                flag = 1;
            }
            break;
        case 0:
        case 1:
            flag = 1;
            break;
        case 39: case 40: case 41: case 42: case 43: case 44:
        case 45: case 46: case 47: case 48: case 49:
            if (xglFlagsGet1(0x185) == 1) {
                if (SaveWork[i] == 0) {
                    flag = 1;
                }
            }
            break;
        case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
        case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
        case 18: case 19: case 20: case 21: case 22: case 23: case 24: case 25:
        case 26: case 27: case 28: case 29: case 30: case 31: case 32: case 33:
        case 34: case 35:
            if (SaveWork[i] == 0) {
                flag = 1;
            }
            break;
        case 50:
            if (xglFlagsGet1(0x187) == 1) {
                flag = SaveWork[i] == 0;
            }
            break;
        }
        if (flag == 1) {
            *p = i;
            MenuNum++;
            p = &shpmenu_work[MenuNum];
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ov11/mini_g", shop_main);

/* ov11:0x00a083f8. Loads the data-viewer minigame's dataviewer and sampler
 * textures. */
extern const char D_00A0C590[]; /* "data\\tanaka\\dataviewer.xtx" */
extern const char D_00A0C5B0[]; /* "data\\tanaka\\sam.xtx" */

#define VW_DATAVIEWER_TEX_BUFFER 0x0124C800
#define VW_SAM_TEX_BUFFER        0x01410000

static void VW_Init(void)
{
    xglCdReadFile(D_00A0C590, (void *)VW_DATAVIEWER_TEX_BUFFER, 0, 0);
    xglCdReadFile(D_00A0C5B0, (void *)VW_SAM_TEX_BUFFER, 0, 0);
}

/* Build the compact list of the 30 sampler slots enabled in SaveWork. */
static __inline void sampler_offset(int **base, int *count)
{
    *base += 114;
    *count = 0;
}

static int VW_SamlistInit(void)
{
    int count;
    int index;
    char *enabled;
    int *sampler;

    sampler = Gwork;
    sampler_offset(&sampler, &count);
    enabled = SaveWork;
    enabled += 6;
    index = 0;

    for (index = 0; index < 30; index++) {
        if (*enabled == 1) {
            *sampler = index;
            sampler++;
            count++;
        }
        enabled++;
    }
    return count;
}

extern int old_113;
extern const char D_00A0C5C8[];
extern const char D_00A0C5D8[];
extern const char D_00A0C5E8[];
extern const char D_00A0C5F8[];
extern const char D_00A0C608[];
extern const char D_00A0C618[];
extern const char D_00A0C628[];

static int VW_ViewModeInit(int no)
{
    char path[256];
    int type;

    type = fileinfo[no];
    if (no != old_113) {
        if (type == 0) {
            sprintf(path, D_00A0C5C8, D_00A0C5D8, no + 1);
            xglCdReadFile(path, (void *)0x01491000, 0, 0);
            sprintf(path, D_00A0C5E8, D_00A0C5D8, no + 1);
            xglCdReadFile(path, (void *)0x014D1800, 0, 0);
        } else {
            sprintf(path, D_00A0C5F8, D_00A0C5D8, no + 1);
            xglCdReadFile(path, (void *)0x01491000, 0, 0);
            sprintf(path, D_00A0C608, D_00A0C5D8, no + 1);
            xglCdReadFile(path, (void *)0x014D1800, 0, 0);
            sprintf(path, D_00A0C618, D_00A0C5D8, no + 1);
            xglCdReadFile(path, (void *)0x01512000, 0, 0);
            sprintf(path, D_00A0C628, D_00A0C5D8, no + 1);
            xglCdReadFile(path, (void *)0x01552800, 0, 0);
        }
        Gwork[0x1C4 / 4] = 0;
        Gwork[0x1C0 / 4] = 0;
    }
    old_113 = no;
    return type;
}

INCLUDE_ASM("asm/nonmatchings/ov11/mini_g", VW_ViewMode);

INCLUDE_ASM("asm/nonmatchings/ov11/mini_g", VW_SamMode);

static int VW_ViewMode(int index);
static int VW_SamMode(int enable);
extern int mode_123;
extern int D_00A0DD18;

static void VW_Main(void)
{
    XglPacket **pkt = &Pkt;

    *pkt = xglPacketGetCurrent();
    if (mode_123 == 1) {
        if (VW_ViewMode(D_00A0DD18) != 0) {
            mode_123 = 0;
        }
        return;
    }
    D_00A0DD18 = VW_SamMode(1);
    if (D_00A0DD18 != -1 && (PadData[0x2A / 2] & 0x10)) {
        RES_SoundEffect(0x1A);
        mode_123 = 1;
    }
    if (PadData[0x2A / 2] & 0x40) {
        VW_SamMode(0);
        GameModeChange(0);
        RES_SoundEffect(0x1B);
    }
}

extern void xglSoundSendEffect(void *swd, void *sed, int bank);
extern int SsdSpuDmaCompleted(int wait);
extern void xglSoundSendSmd(void *smd);
extern int xglSoundSendSwd(void *swd, int bank);
extern void xglSoundSequenceNormal(int volume);
extern int sprintf(char *buf, const char *fmt, ...);
extern int oldreq_126;
extern const char D_00A0C660[];
extern const char D_00A0C680[];
extern const char D_00A0C6A0[];
extern const char D_00A0C6B0[];
extern const char D_00A0C6C0[];

static void MUSIC_CALL(int musicId)
{
    char path[256];

    if (musicId == -1 || oldreq_126 == musicId) {
        return;
    }
    xglSoundSendEffect(0, 0, 1);
    xglSoundSendEffect(0, 0, 2);
    xglCdReadFile(D_00A0C660, (void *)0x017A7000, 0, 0);
    xglCdReadFile(D_00A0C680, (void *)0x017B7000, 0, 0);
    xglSoundSendEffect((void *)0x017B7000, (void *)0x017A7000, 1);
    while (SsdSpuDmaCompleted(0) != 0) {
    }
    xglSoundSendEffect((void *)0x017B7000, (void *)0x017A7000, 2);
    sprintf(path, D_00A0C6A0, D_00A0C6B0, MUSICLIST_125[musicId]);
    xglCdReadFile(path, (void *)0x015A3000, 0, 0);
    xglSoundSendSmd((void *)0x015A3000);
    sprintf(path, D_00A0C6C0, D_00A0C6B0, MUSICLIST_125[musicId]);
    xglCdReadFile(path, (void *)0x017A3000, 0, 0);
    xglSoundSendSwd((void *)0x017A3000, -1);
    while (SsdSpuDmaCompleted(0) != 0) {
    }
    xglSoundSequenceNormal(0x7F);
}

/* xglFlagsSet1 is main:0x00222938 (src/main/xgl_flags.c, external linkage
 * there). dataMoneyBoxInc is main:0x002BCE40, still INCLUDE_ASM there
 * (src/main/data.c); src/main/runtime.c already forwards to it with this
 * same signature. AddCoin (ov11:0x00a00b28) and MUSIC_CALL (ov11:0x00a08dc0)
 * are LOCAL asm siblings of this TU. */
extern int xglFlagsSet1(int bit_offset, int value);
extern int dataMoneyBoxInc(int amount);
static int AddCoin(int amount);
static void MUSIC_CALL(int musicId);

/* ov11:0x00a08f40. Resets the four leading Gwork words and the six Gwork
 * shorts at 0x240-0x24A, loads the casino resource table and starts the
 * background sound queue, applies four debug-mode config-flag unlocks and
 * grants a near-maximum bank balance plus a matching coin total when
 * RES_IsDebugMode() is set, then seeds six more Gwork shorts starting at
 * 0x54 (a leading sentinel of 5 followed by a descending fill of the next
 * five with 4..0). Field identities beyond their byte offsets are not
 * separately evidenced. Its only caller, MiniG_Init, runs this once. */
/* The original materializes Gwork's address once (%lo(Gwork)=0, so
 * `addiu s0,v0,0`) and reaches every field below off that same register
 * with an immediate byte offset; indexing Gwork as a word array and a
 * short array through separate expressions instead recomputes the base at
 * Gwork+0x240, which does not reproduce this. */
#define WORK_RESET_WORD_0 0x0
#define WORK_RESET_WORD_1 0x4
#define WORK_RESET_WORD_2 0x8
#define WORK_RESET_WORD_3 0xC
#define WORK_MODE_SHORT_0 0x240
#define WORK_MODE_SHORT_1 0x242
#define WORK_MODE_SHORT_2 0x244
#define WORK_MODE_SHORT_3 0x246
#define WORK_MODE_SHORT_4 0x248
#define WORK_MODE_SHORT_5 0x24A
#define WORK_STAKE_SENTINEL 0x54
#define WORK_STAKE_TABLE_END 0x5E

static void InitWork(void)
{
    unsigned char *work = (unsigned char *)Gwork;
    short *slot;
    int value;

    *(short *)(work + WORK_MODE_SHORT_0) = 0;
    *(short *)(work + WORK_MODE_SHORT_1) = 0;
    *(short *)(work + WORK_MODE_SHORT_2) = 0;
    *(short *)(work + WORK_MODE_SHORT_3) = 0;
    *(short *)(work + WORK_MODE_SHORT_4) = 0;
    *(short *)(work + WORK_MODE_SHORT_5) = 0;
    *(int *)(work + WORK_RESET_WORD_0) = 0;
    *(int *)(work + WORK_RESET_WORD_1) = 0;
    *(int *)(work + WORK_RESET_WORD_2) = 0;
    *(int *)(work + WORK_RESET_WORD_3) = 0;

    RES_Load();
    MUSIC_CALL(0);

    if (RES_IsDebugMode() != 0) {
        xglFlagsSet1(0x73, 1);
        xglFlagsSet1(0x12D, 1);
        xglFlagsSet1(0x185, 1);
        xglFlagsSet1(0x187, 1);
        dataMoneyBoxInc(0x3B9AC9FF);
        AddCoin(0x989298);
    }

    *(short *)(work + WORK_STAKE_SENTINEL) = 5;
    slot = (short *)(work + WORK_STAKE_TABLE_END);
    value = 4;
    do {
        *slot = value;
        value--;
        slot--;
    } while (value >= 0);
}

/* ov11:0x00a09020. Logs the configured heap address, then delegates all
 * casino state initialization to InitWork (LOCAL asm sibling). */
static void InitWork(void);
extern const char D_00A0C6D0[]; /* "work size = %x\n" */

void MiniG_Init(void)
{
    dprintf(D_00A0C6D0, 0x7CF000);
    InitWork();
}

INCLUDE_ASM("asm/nonmatchings/ov11/mini_g", MiniG_Main);

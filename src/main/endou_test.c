#include "common.h"

#include "shared.h"

#include "endou_test.h"

extern void xglSleep(void);

/* ld at +0x28 and lhu/sh at +0x28/+0x2a access the same button storage. */
typedef struct PadDataButtonView {
    u8 unmodeled_00[0x28];
    union {
        u64 button_state;
        struct {
            u16 held;
            u16 pressed;
            u16 repeat;
            u16 released;
        } input;
    } buttons;
} PadDataButtonView;

extern PadDataButtonView PadData;

extern void xglCdLoadOverlay(int overlayId);

extern void SeisanInit(void);

extern int SeisanMain(void);

extern void SeisanDisp(void);

extern void xglRenderClearFrame(void);

typedef struct EModelTestState {
    unsigned char unmodeled_00[0x11];
    unsigned char isReady;
    unsigned char unmodeled_12[1];
    unsigned char extFuncFlag;
} EModelTestState;

extern void MenuModelUnitOpen(void *unit, int drawType);

extern void MenuModelControl(void *unit);

extern void ACT_init(void);

extern void MenuLoadInit(void);

extern void *MenuModelInit(void *work_start);

extern void MenuModelMemoryInit(void *base, int size);

extern void MenuModelMemorySet(int count);

extern void MenuModelCreate(int *model, int character_id);

extern void MenuModelExtFuncSet(int model,
                                void (*callback)(EModelTestState *),
                                int argument);

extern void MenuModelMain(void);

/* EUC-JP bytes for the model-test diagnostic with its leading control byte. */

const unsigned char D_004C2AE8[16] =
    "\x0B\xA5\xE2\xA5\xC7\xA5\xEB\xA5\xC6\xA5\xB9\xA5\xC8";

extern const char not_found_format[16];


extern const char read_error_format[24];


typedef struct GifPackedTag {
    unsigned int loop_count_and_eop;
    unsigned int primitive_flags_and_register_count;
    u64 register_descriptors;
} GifPackedTag;

typedef struct GifAdWrite {
    u64 value;
    u64 register_id;
} GifAdWrite;

typedef struct GifPackedRgbaq {
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int alpha;
} GifPackedRgbaq;

typedef struct GifUv {
    unsigned int u;
    unsigned int v;
    unsigned int q;
    unsigned int reserved;
} GifUv;

typedef struct GifXyz2 {
    unsigned int x;
    unsigned int y;
    unsigned int z;
    unsigned int fog;
} GifXyz2;

typedef struct BgTestRenderState {
    unsigned char unmodeled_00[0x14];
    unsigned short packetControl;
} BgTestRenderState;

typedef struct BgTestEnvironment {
    unsigned int vif_nop[3];
    unsigned int vif_directhl_nine_quadwords;
    GifPackedTag tag;
    GifAdWrite texture_setup;
    GifAdWrite packet_write;
    GifAdWrite primitive_setup;
    GifPackedRgbaq color;
    GifUv first_uv;
    GifXyz2 first_vertex;
    GifUv second_uv;
    GifXyz2 second_vertex;
} BgTestEnvironment;

enum {
    BG_TEST_COMMAND_HIGH = 0xc800,
    BG_TEST_COMMAND_BASE = 0x2412,
    BG_TEST_COMMAND_BASE_SHIFT = 16,
    BG_TEST_CONTROL_SHIFT = 5,
    BG_TEST_COMMAND_HIGH_SHIFT = 19
};

extern BgTestRenderState sRender;

extern BgTestEnvironment TestEnv_47;


extern void sceVif1PkCloseDirectHLCode(XglPacket *packet);

extern void sceVif1PkCnt(XglPacket *packet, int count);

extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);

extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int count);

extern void *test_data;


extern char fname_67[64];


extern float size_68;


extern unsigned int TestEnv_66[32];


typedef struct WinTexEnvironment {
    unsigned int vif_nop_before_flush;
    unsigned int vif_nop_before_direct;
    unsigned int vif_flushe;
    unsigned int vif_directhl_six_quadwords;
    GifPackedTag gif_tag;
    GifAdWrite texture_flush;
    GifAdWrite bitblt_buffer;
    GifAdWrite transfer_position;
    GifAdWrite transfer_region;
    GifAdWrite transfer_direction;
} WinTexEnvironment;

/* VIF DIRECTHL carries a GIF PACKED tag and five GS A+D register writes. */

extern WinTexEnvironment WinTexEnv_65;


/*
 * The context e_test's argument points to: only the VIF packet pointer at
 * +0x00 is evidenced here (main VA 0x002728f8, lw $4,0x0($16) before each
 * sceVif1Pk* call); the rest stays an unmodeled span (docs/naming.md).
 */

typedef struct ETestContext {
    XglPacket *packet;
} ETestContext;

extern void xglFontReloadTexture(ETestContext *context, int mode);

/*
 * The original PadData object is 0xD0 bytes at 0x00490D90. EndouTest uses the
 * same eight bytes at +0x28 as two halfwords for button state and as one
 * 64-bit exit-chord word; this local prefix models only that evidenced view.
 * SeisanTest keeps its separate full-width declaration above unchanged.
 */

typedef struct EndouPadDataPrefix {
    u8 unmodeled_00[0x28];
    union {
        u64 button_state;
        struct {
            u16 half_28;
            u16 half_2a;
            u8 unmodeled_2c[4];
        } halves;
    } buttons;
} EndouPadDataPrefix;

typedef struct EndouClearEnvironment {
    u8 unmodeled_00[0x20];
    u32 color_r;
    u32 color_g;
    u32 color_b;
    u32 color_a;
} EndouClearEnvironment;

extern EndouClearEnvironment ClearEnv;

extern const char D_004C2440[];

extern const char D_004C2458[];

extern const char D_004C2468[];

extern const char D_004C2480[];

extern const char D_004C2498[];

extern const char D_004C24B0[];

extern const char D_004C24C8[];

extern const char D_004C24E0[];

extern const char D_004C24F8[];

extern const char D_004C2510[];

extern const char D_004C2528[];

extern const char D_004C2540[];

extern const char D_004DA660[];

extern void endPrintInit(void);

extern void SeisanTest(void);

extern void eMessageTest(void);

extern void eNumberTest(void);

extern void eModelTest(void);

extern void TexturTest(void);

extern void JpegTest(void);

extern void BattleWindowTest(void);

extern void ModelTest(void);

extern void TextTest(void);

extern void ShopTest(void);

extern int MenuFileMain(int mode);

typedef struct ShopDataRecord {
    u16 values[0xb8];
} ShopDataRecord;

extern void PartyFriendOn(int party_id);

extern void PartyTakeAgwsOn(int characterId);

/* Clear the CPU button state before the render thread samples it again. */

#define ENDOU_PAD_BUTTONS ((EndouPadDataPrefix *)(void *)&PadData)

/* Volatile input view: the separate xglRenderEntry thread calls xglPadRead,
 * which writes the decoded controller words. xglSleep yields between reads. */

#define ENDOU_PAD_INPUT ((volatile EndouPadDataPrefix *)(void *)&PadData)

#define ENDOU_CLEAR_ENV ((EndouClearEnvironment *)(void *)&ClearEnv)

/* EUC-JP bytes for the model-test diagnostic with its leading control byte. */

typedef struct EMessageTestWindow {
    u8 unmodeled_00;
    u8 mode;
    u8 unmodeled_02[2];
    u16 message_x;
    u16 message_y;
    u32 message_color;
    u8 unmodeled_0c[2];
    s16 selection_value;
    u8 unmodeled_10[2];
    u8 debug_value;
    u8 debug_cursor;
    u8 unmodeled_14[4];
    const char *message_text;
    u8 page_state;
    u8 unmodeled_1d[0x33];
} EMessageTestWindow;

extern int WindowTexLoad(unsigned char *buffer, unsigned int request);

extern void eMessageSet(void *message, const char *text);

extern void eMessageTextChange(void *message, const char *text);

extern int eMessageNextPage(void *message, int mode);

extern void eMessageDraw(void *message);

extern void eMessageMain(void *message);

extern void endPrintExtFunc(int mode, int amount, void *data);

extern void xglFontDebugHex(int x, int y, int value, int digits);

extern const char *msg_45[5];

extern const char text00_46[];

extern const char D_004C2C70[];

#include "main/control_entry.h"

#include "main/xgl_cd.h"

#include "main/xgl_font.h"

extern const char D_004C2CF0[];

extern const char D_004C2D08[];

extern const char D_004C2D18[];

extern void xglFontPrintExtFunc(unsigned int mask,
                                void (*callback)(ETestContext *),
                                void *context);

typedef struct UmnEventTextRecord {
    u32 unmodeled_00;
    const char *message_text;
} UmnEventTextRecord;

typedef struct TextTestMessage {
    u8 unmodeled_00[4];
    u16 message_x;
    u16 message_y;
    int message_parameter;
    u8 unmodeled_0c[0x10];
    u8 page_state;
    u8 unmodeled_1d[0x33];
} TextTestMessage;

extern u8 *UmnEventTextInit(u8 *work_end);

extern int UmnEventTextMake(int mode);

extern UmnEventTextRecord *UmnEventTextNextGet(int index);

extern void eMessageModeChange(void *message, u8 mode);

extern const char D_004C2558[];

typedef struct UmnMailDisplayParams {
    int mode;
    u8 unmodeled_04[4];
    u16 texture_x;
    u16 texture_y;
    u16 texture_width;
    u16 texture_height;
    u8 unmodeled_10[8];
    s16 scroll_x;
    s16 scroll_y;
    u16 view_width;
    u16 view_height;
    u8 unmodeled_20[0x20];
} UmnMailDisplayParams;

extern void tyaUmlDispInit(void);

extern void tyaUmlDispParamReset(UmnMailDisplayParams *params, int mode);

extern void tyaUmlDispMain(UmnMailDisplayParams *params);

extern const char D_004C2CD0[];

typedef struct JpegTestDecodeRequest {
    void *source;
    void *destination;
    u8 unmodeled_08[8];
    u16 source_width;
    u16 source_height;
    u16 max_width;
    u16 max_height;
    u8 unmodeled_18[8];
} JpegTestDecodeRequest;

typedef struct JpegTestDisplay {
    u16 x;
    u16 y;
    u32 color;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
    void *image_data;
} JpegTestDisplay;

extern int GameSnapShotSaveThumbnail(int quality, void *jpeg_buffer);

extern const char D_004C2DA8[];

extern int Select_74;

extern int Select_76;

extern int Select_78;

extern int Select_80;

extern const char D_004C2DF0[];

extern const char D_004C2E08[];

extern const char D_004C2E38[];

extern const char D_004C2E58[];

extern const char D_004C2E88[];

extern const char D_004C2EA8[];

extern const char D_004C2EC0[];

extern const char D_004DA768[];

extern const char *text00_75[];

extern const char *text00_77[];

extern const char *text00_79[];

extern const char *text00_81[];

extern char *MenuCharNameGet(int characterId);

extern int PartyTakeAgwsCheck(int characterId);

extern void PartyTakeAgwsOff(int characterId);

extern int PartyLockPartyCheck(int characterId);

extern void PartyLockPartyOn(int characterId);

extern void PartyLockPartyOff(int characterId);

void EndouTest(void)
{
    /* Initialize the CPU-owned packet before the renderer flushes it. */
    ENDOU_CLEAR_ENV->color_r = 0;
    ENDOU_CLEAR_ENV->color_g = 0x40;
    ENDOU_CLEAR_ENV->color_b = 0;
    ENDOU_CLEAR_ENV->color_a = 0;

    ENDOU_PAD_BUTTONS->buttons.halves.half_2a = 0;
    endPrintInit();

    for (;;) {
        xglRenderClearFrame();
        if ((ENDOU_PAD_INPUT->buttons.button_state & 0x08000100ULL) == 0x08000100ULL) {
            break;
        }

        xglFontDebugPrintf(0, 0, D_004DA660);
        xglFontDebugPrintf(0, 0x10, D_004C2440);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x10) != 0) {
            SeisanTest();
        }

        xglFontDebugPrintf(0, 0x20, D_004C2458);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_28 & 0x80) != 0) {
            TextTest();
        }

        xglFontDebugPrintf(0, 0x30, D_004C2468);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x40) != 0) {
            eMessageTest();
        }

        xglFontDebugPrintf(0, 0x40, D_004C2480);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x20) != 0) {
            eNumberTest();
        }

        xglFontDebugPrintf(0, 0x50, D_004C2498);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x4) != 0) {
            eModelTest();
        }

        xglFontDebugPrintf(0, 0x60, D_004C24B0);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x1) != 0) {
            MenuFileMain(1);
        }

        xglFontDebugPrintf(0, 0x70, D_004C24C8);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x8) != 0) {
            TexturTest();
        }

        xglFontDebugPrintf(0, 0x80, D_004C24E0);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x2) != 0) {
            JpegTest();
        }

        xglFontDebugPrintf(0, 0x90, D_004C24F8);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x1000) != 0) {
            BattleWindowTest();
        }

        xglFontDebugPrintf(0, 0xa0, D_004C2510);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x4000) != 0) {
            MenuFileMain(0);
        }

        xglFontDebugPrintf(0, 0xb0, D_004C2528);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x2000) != 0) {
            ModelTest();
        }

        xglFontDebugPrintf(0, 0xc0, D_004C2540);
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x8000) != 0) {
            ShopTest();
        }

        xglSleep();
    }
}

void TextTest(void)
{
    TextTestMessage message;
    UmnEventTextRecord *record;

    xglCdLoadOverlay(2);
    xglRenderClearFrame();
    WorkEnd = UmnEventTextInit(WorkEnd);
    UmnEventTextMake(1);
    endPrintInit();

    record = UmnEventTextNextGet(0);
    if (record == 0) {
        return;
    }

    eMessageSet(&message, record->message_text);
    message.message_x = 0x40;
    message.message_y = 0x40;
    message.message_parameter = 0x00fffff0;
    message.page_state = 2;
    eMessageModeChange(&message, 0x22);

    while (xglFontDebugPrintf(0, 0x10, D_004C2558),
           (PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        eMessageMain(&message);
        if ((PadData.buttons.input.pressed & 8U) != 0 &&
            eMessageNextPage(&message, 0) == 0) {
            record = UmnEventTextNextGet(1);
            if (record == 0) {
                return;
            }
            eMessageTextChange(&message, record->message_text);
            eMessageModeChange(&message, 0x22);
        }

        endPrintExtFunc(0, 100, 0);
        xglSleep();
    }
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct PartyAttackSlot {
    unsigned short character;
    signed char position;
    unsigned char unmodeled_03;
} PartyAttackSlot;

typedef struct PartyDataView {
    unsigned char unmodeled_00[0x30];
    PartyAttackSlot attack_slots[3];
} PartyDataView;

typedef struct EndouBattleWindowParams {
    int x;
    int y;
    int style;
    int information_x;
    int information_y;
    void *commands;
    const char *title;
    int repeat;
    int selection;
} EndouBattleWindowParams;

typedef struct EndouBattleMessageParams {
    int x;
    int y;
    int style;
    const char *title;
    const char *text;
} EndouBattleMessageParams;

typedef struct EndouBattleAnalysisParams {
    int x;
    int y;
    int style;
    const char *name;
    short hp;
    short max_hp;
    unsigned short conditions;
    u8 unmodeled_16[2];
    int technique_points;
    int ether_points;
    int skill_points;
    int experience;
    long money;
    unsigned short common_drop;
    unsigned short rare_drop;
} EndouBattleAnalysisParams;

typedef struct EndouBattleRewardParams {
    int x;
    int y;
    int style;
    int width;
    const char *text;
    const char *title;
} EndouBattleRewardParams;

typedef struct TexturTestUploadEnvironment {
    u8 unmodeled_00[0x20];
    u64 destination_parameters;
    u8 unmodeled_28[0x18];
    u64 transfer_dimensions;
} TexturTestUploadEnvironment;

typedef struct TexturTestDrawEnvironment {
    u8 unmodeled_00[0x50];
    u64 texture_parameters;
} TexturTestDrawEnvironment;

extern void func_A22628(void);
extern int dataMoneyBoxInc(int amount);
extern void MenuShopMain(int menu_selection);
extern const char D_004C2568[], D_004C2588[], D_004C2598[], D_004C25B0[], D_004C25C8[];
extern const char D_004DA668[], D_004DA670[], D_004DA678[], D_004DA680[], D_004DA688[];
extern const char D_004DA690[], D_004DA698[], D_004DA6A0[], D_004DA6A8[], D_004DA6B0[];
extern const char D_004DA6B8[];
static inline void endouShopHeading(int y, const char *format, int shop_number)
{
    xglFontDebugPrintf(0, y, format, shop_number);
}

void ShopTest(void)
{
    unsigned int ab_column_offset;
    typedef struct ShopFile {
        u16 records[20][0xb8];
    } ShopFile;
    int row_index;
    int menu_open;
    unsigned int menu_selection = 1;
    ShopFile *shop_file;
    unsigned short pressed;

    xglCdLoadOverlay(1);
    func_A22628();
    ACT_init();

    shop_file = (ShopFile *)(((u32)WorkEnd + 0x7fU) & 0xffffff80U);
    WorkEnd = (u8 *)shop_file + xglCdReadFile(D_004C2568, shop_file, 0, 1);
    PartyFriendOn(3);
    PartyFriendOn(2);
    menu_open = 0;
    PartyFriendOn(7);
    PartyTakeAgwsOn(0x11);
    PartyTakeAgwsOn(0x18);
    if ((PadData.buttons.input.held & 0x100U) != 0)
        dataMoneyBoxInc(0x64);
    else
        dataMoneyBoxInc(0xf4240);
    xglRenderClearFrame();

    for (;;) {
        xglFontDebugPrintf(0, 0x10, D_004C2588);
        if ((PadData.buttons.button_state & 0x08000100U) == 0x08000100U)
            break;
        pressed = PadData.buttons.input.pressed;
        if ((pressed & 0x40) != 0)
            break;

        switch (menu_open) {
        case 0: {
            u16 repeated;
            u16 (*selected_page)[0xb8];
            const u16 *entry_values;
            const u16 *neighbor_values;
            const u16 *extra_values;

            repeated = PadData.buttons.input.repeat;
            if ((repeated & 0x1000U) != 0)
                menu_selection = menu_selection == 1 ? 0x14 : menu_selection - 1;
            if ((repeated & 0x4000U) != 0)
                menu_selection = menu_selection == 0x14 ? 1 : menu_selection + 1;
            if ((pressed & 0x20) != 0)
                menu_open = 1;
            xglFontDebugPrintf(0, 0x20, D_004C2598);
            xglFontDebugPrintf(0x80, 0x20, D_004C25B0);
            endouShopHeading(0x30, D_004C25C8, menu_selection);

            {
            u16 (*const page)[0xb8] =
                &shop_file->records[menu_selection];

            endouShopHeading(0x40, D_004DA668, menu_selection);
            endouShopHeading(0x50, D_004DA670, menu_selection);
            selected_page = page - 1;
            endouShopHeading(0x60, D_004DA678, menu_selection);
            endouShopHeading(0x70, D_004DA680, menu_selection);
            endouShopHeading(0x80, D_004DA688, menu_selection);
            endouShopHeading(0x90, D_004DA690, menu_selection);
            endouShopHeading(0xa0, D_004DA698, menu_selection);
            endouShopHeading(0xb0, D_004DA6A0, menu_selection);
            endouShopHeading(0xc0, D_004DA6A8, menu_selection);
            endouShopHeading(0xd0, D_004DA6B0, menu_selection);

            row_index = 0;
            extra_values = page[-1] + 4;
            entry_values = selected_page[0];
            neighbor_values = selected_page[0] + 0x8c;

            do {
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x40, D_004DA6B8,
                                   *entry_values);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x50, D_004DA6B8,
                                   neighbor_values[-0x78]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x60, D_004DA6B8,
                                   entry_values[0x28]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x70, D_004DA6B8,
                                   neighbor_values[-0x50]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x80, D_004DA6B8,
                                   entry_values[0x50]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x90, D_004DA6B8,
                                   neighbor_values[-0x28]);
                ab_column_offset = 6 * 20;
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0xa0, D_004DA6B8,
                                   entry_values[ab_column_offset]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0xb0, D_004DA6B8,
                                   *neighbor_values++);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0xc0, D_004DA6B8,
                                   (entry_values++)[0xa0]);
            } while (++row_index < 0x14);
            for (row_index = 0; row_index < 4; row_index++) {
                int x = (row_index % 10) * 0x18 + 0x10;
                int y = (row_index / 10) * 8;

                xglFontDebugPrintf(x, y + 0xd0, D_004DA6B8,
                                   extra_values[0xb0 + row_index]);
            }
            }
            break;
        }
        case 1:
            menu_open = 0;
            MenuShopMain(menu_selection);
            xglRenderClearFrame();
            break;
        }
        xglSleep();
    }
}

void SeisanTest(void)
{
    const int exit_chord = 0x08000100;

    xglCdLoadOverlay(1);
    SeisanInit();
    xglRenderClearFrame();

    while ((PadData.buttons.button_state & exit_chord) != exit_chord) {
        SeisanMain();
        SeisanDisp();
        xglSleep();
    }
}

INCLUDE_ASM("asm/main/nonmatchings/endou_test", ModelTest);

extern void eBattleWinOpen(void *a);
extern void eBattleWinOpen2(void *a);
extern void eBattleWinOpen3(void *a);
extern void eBattleWinOpen4(void *a);
extern void eBattleWinMain(void *a);
extern void eBattleWinClose(void *a);
extern void eBattleWinMain2(void);
extern void eBattleWinMain3(void);
extern void eBattleWinMain4(void);
extern void eBattleWinClose2(void);
extern void eBattleWinClose3(void);
extern void eBattleWinClose4(void);
extern int eBattleWinPageCheck4(void);
extern void eBattleWinInit(void);
extern char data_15[];
extern void *tag_16[];
extern char text_17[];
extern char tag_20[];
extern void *msg_19[];
extern char D_004DA720[];
extern const char D_004C2A70[];
extern const char D_004C2A88[];
extern const char D_004DA730[];
extern int count_18;
typedef struct EndouBwArg0 {
    int a;
    int b;
    int c;
    int d;
    int e;
    void *f;
    void *g;
    int h;
    int pad[4];
} EndouBwArg0;
typedef struct EndouBwArg4 {
    int a;
    int b;
    int c;
    void *d;
    void *e;
} EndouBwArg4;
typedef struct EndouBwArg3 {
    int a;
    int b;
    int c;
    void *d;
    short e;
    short f;
    short g;
    int h;
    int i;
    int j;
    int n;
    long long k;
    short l;
    short m;
} EndouBwArg3;
typedef struct EndouBwArg2 {
    int a;
    int b;
    int c;
    int d;
    void *e;
    void *f;
} EndouBwArg2;

void BattleWindowTest(void)
{
    EndouBwArg0 s0;
    EndouBwArg4 s30;
    EndouBwArg3 s50;
    EndouBwArg2 s90;
    void **mp;

    s0.a = 0x10;
    s0.c = 0xFFFFFD;
    *(int *)&s0.g = (int)tag_16[0];
    s0.f = data_15;
    s0.b = 0x90;
    *(char **)&s0.e = (char *)0xA0;
    s0.d = 0;
    xglRenderClearFrame();
    xglSleep();
    xglCdLoadOverlay(1);
    eBattleWinInit();
    if ((PadData.buttons.button_state & 0x08000100) != 0x08000100) {
        mp = msg_19;
        do {
            xglFontDebugPrintf(0, 0, D_004C2A70);
            if (PadData.buttons.input.pressed & 0x10) {
                s30.a = 0x10;
                s30.c = 0xFFFFFD;
                s30.b = 0x20;
                s30.d = D_004DA720;
                s30.e = text_17;
                eBattleWinOpen4(&s30);
            }
            if ((PadData.buttons.input.pressed & 0x80) && (eBattleWinPageCheck4() != 0)) {
                eBattleWinClose4();
            }
            eBattleWinMain4();
            if (PadData.buttons.input.pressed & 0x10) {
                s50.a = 0x40;
                s50.b = 0x100;
                s50.n = 0x2107A;
                s50.g = 0x208;
                s50.e = 0x64;
                s50.f = 0xC8;
                s50.h = 0x1EF;
                s50.i = 0x2B1;
                s50.j = 0x320;
                s50.k = 0xDAC0;
                s50.l = 5;
                s50.m = 0xA;
                s50.c = 0xFFFFF0;
                s50.d = D_004C2A88;
                eBattleWinOpen3(&s50);
            }
            if (PadData.buttons.input.pressed & 0x80) {
                eBattleWinClose3();
            }
            eBattleWinMain3();
            if (PadData.buttons.input.pressed & 4) {
                s90.d = 0x800000B4;
                s90.a = 0x10;
                s90.b = 0x20;
                s90.e = msg_19[count_18];
                s90.c = 0xFFFFF0;
                s90.f = tag_20;
                eBattleWinOpen2(&s90);
                count_18 ^= 1;
            }
            if (PadData.buttons.input.pressed & 1) {
                eBattleWinClose2();
            }
            eBattleWinMain2();
            if (PadData.buttons.input.pressed & 8) {
                eBattleWinOpen(&s0);
            }
            if (PadData.buttons.input.pressed & 2) {
                eBattleWinClose(&s0);
            }
            s0.h = PadData.buttons.input.repeat;
            xglFontDebugPrintf(0, 0x20, D_004DA730, s0.h);
            eBattleWinMain(&s0);
            xglSleep();
        } while ((PadData.buttons.button_state & 0x08000100) != 0x08000100);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/endou_test", eNumberTest);

void eModelTestMain(EModelTestState *unit)
{
    if (unit->isReady == 0) {
        return;
    }

    if (unit->extFuncFlag == 0) {
        MenuModelUnitOpen(unit, 2);
        unit->extFuncFlag = 20;
    }

    MenuModelControl(unit);
}

void eModelTest(void)
{
    int model_id;

    xglCdLoadOverlay(1);
    ACT_init();
    xglRenderClearFrame();
    xglSleep();
    MenuLoadInit();
    MenuModelInit((void *)0x01000000);
    MenuModelMemoryInit((void *)0x01100000, 0x00500000);
    MenuModelMemorySet(0);
    MenuModelCreate(&model_id, 29);
    MenuModelExtFuncSet(model_id, eModelTestMain, 0);

    while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2AE8);
        MenuModelMain();
        xglSleep();
    }
}

void ePrintTest(void)
{
}

void eMessageTest(void)
{
    EMessageTestWindow windows[2];
    EMessageTestWindow current;
    int message_index = 0;

    WindowTexLoad(0, 0);
    endPrintInit();
    eMessageSet(&windows[0], msg_45[0]);
    eMessageSet(&windows[1], msg_45[0]);
    windows[1].mode = 0x20;
    eMessageSet(&current, text00_46);
    current.page_state = 1;
    eMessageTextChange(&current, text00_46);

    PadData.buttons.input.pressed = 0;
    current.message_x = 0x40;
    current.message_y = 300;
    current.message_color = 0x00fffff0;
    current.mode = 0x24;
    ClearEnv.color_g = 0x40;
    ClearEnv.color_r = 0;
    ClearEnv.color_b = 0;
    ClearEnv.color_a = 0;

    if ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        do {
            xglFontDebugPrintf(0, 0, D_004C2C70);
            if ((PadData.buttons.input.pressed & 0x20U) != 0) {
                eMessageNextPage(&current, 0);
            }
            if ((PadData.buttons.input.pressed & 0x100U) != 0) {
                message_index += 1;
                if (message_index >= 5) {
                    message_index = 0;
                }
                windows[0].message_text = msg_45[message_index];
            }
            if ((PadData.buttons.input.pressed & 8U) != 0) {
                windows[0].mode = 0x22;
                windows[1].mode = 0x22;
                eMessageNextPage(&current, 1);
                current.mode = 0x22;
            }
            if ((PadData.buttons.input.pressed & 2U) != 0) {
                windows[0].mode = 0x70;
                windows[1].mode = 0x70;
                current.mode = 0x70;
            }
            if ((PadData.buttons.input.held & 4U) != 0) {
                windows[0].selection_value -= 1;
            }
            if ((PadData.buttons.input.held & 1U) != 0) {
                windows[0].selection_value += 1;
            }
            if ((PadData.buttons.input.held & 0x1000U) != 0) {
                windows[0].debug_cursor -= 1;
            }
            if ((PadData.buttons.input.held & 0x4000U) != 0) {
                windows[0].debug_cursor += 1;
            }

            xglFontDebugHex(0, 0x10, windows[0].debug_value, 2);
            windows[0].message_color = 0x01ffffff;
            windows[1].message_color = 0x01ffff00;
            windows[0].message_x = 100;
            windows[0].message_y = 100;
            windows[1].message_x = 0xa4;
            windows[1].message_y = 0xa4;
            eMessageDraw(&windows[1]);
            eMessageMain(&current);
            endPrintExtFunc(0x00ffffff, 100, 0);
            xglSleep();
        } while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U);
    }
}

void BgTest(void)
{
    XglPacket *packet;
    int packet_command = BG_TEST_COMMAND_BASE << BG_TEST_COMMAND_BASE_SHIFT;

    packet_command |= sRender.packetControl << BG_TEST_CONTROL_SHIFT;
    TestEnv_47.packet_write.value = packet_command |
        ((u64)BG_TEST_COMMAND_HIGH << BG_TEST_COMMAND_HIGH_SHIFT);
    packet = xglPacketGetCurrent();
    sceVif1PkRef(packet, &TestEnv_47, 10, 0, 0, 0);
}

typedef struct TexturTestEntry {
    unsigned int size;
    unsigned int w4;
    int addr;
    int qwc;
    int offset;
} TexturTestEntry;
typedef struct TexturTestHeader {
    int unk0;
    int unk4;
    int count;
    int offset;
} TexturTestHeader;
typedef struct TexturTestTexEnv {
    u8 unmodeled_00[0x20];
    u64 w20;
    u8 unmodeled_28[0x18];
    u64 w40;
} TexturTestTexEnv;
typedef struct TexturTestEnv {
    u8 unmodeled_00[0x50];
    u64 packetWord;
} TexturTestEnv;
extern void xglRenderClearDepth(void);
extern TexturTestTexEnv eNumberTexEnv_48;
extern TexturTestEnv TestEnv_52;
extern int x_53;
extern const char D_004C2C88[];
extern const char D_004C2CA0[];
extern const char D_004C2CB8[];
static inline void TexturTestEnvSend(XglPacket *packet)
{
    int x;

        xglFontDebugHex(0, 0x20, x_53, 5);
        x = x_53;
        if (PadData.buttons.input.repeat & 0x2000) {
            x_53 = x += 0x100;
        }
        if (PadData.buttons.input.repeat & 0x8000) {
            x_53 = x -= 0x100;
        }
        TestEnv_52.packetWord = ((u64)((unsigned int)(x + 0xE0000) >> 6) << 37) | 0x2000000629343800ULL;
        sceVif1PkRef(packet, &TestEnv_52, 12, 0, 0, 0);
}

void TexturTest(void)
{
    TexturTestEntry *entry;
    TexturTestTexEnv *env;
    u8 *save;
    u8 *data;
    u8 *data2;
    int size;
    int i;
    unsigned int w;
    unsigned int lo;
    unsigned int hi;

    save = WorkEnd;
    data = (u8 *)(((int)WorkEnd + 0xF) & ~0xF);
    size = xglCdReadFile(D_004C2C88, data, 0, 0);
    if (size < 0) {
        return;
    }
    WorkEnd = data + size;
    data2 = (u8 *)(((int)WorkEnd + 0xF) & ~0xF);
    size = xglCdReadFile(D_004C2CA0, data2, 0, 0);
    if (size < 0) {
        return;
    }
    WorkEnd = data2 + size;
    xglRenderClearDepth();
    for (;;) {
        BgTest();
        xglFontDebugPrintf(0, 0, D_004C2CB8);
        if ((PadData.buttons.button_state & 0x08000100U) == 0x08000100U) {
            break;
        }
        {
        XglPacket *packet;

        i = 0;
        packet = xglPacketGetCurrent();
        entry = (TexturTestEntry *)(data + ((TexturTestHeader *)data)->offset);
        if (((TexturTestHeader *)data)->count > 0) {
            do {
                w = entry->size;
                lo = w & 0xFFFF;
                hi = w >> 16;
                if (w == lo) {
                    hi = (int)(lo + 0x3F) >> 6;
                }
                i++;
                eNumberTexEnv_48.w40 = lo | ((u64)entry->w4 << 32);
                eNumberTexEnv_48.w20 = ((u64)((unsigned int)(entry->addr + 0xE0000) >> 6) << 32) | ((u64)hi << 48);
                FlushCache(0);
                sceVif1PkRef(packet, &eNumberTexEnv_48, 6, 0, 0, 0);
                sceVif1PkRef(packet, data + entry->offset, entry->qwc, 0, 0, 0);
                entry++;
            } while (i < ((TexturTestHeader *)data)->count);
        }
        TexturTestEnvSend(packet);
        xglSleep();
        }
    }
    xglRenderClearFrame();
    WorkEnd = save;
}

void UmnPrintTest(void)
{
}

void UmnMailDispTest(void)
{
    UmnMailDisplayParams windows[2];

    tyaUmlDispInit();
    tyaUmlDispParamReset(&windows[0], 0);
    tyaUmlDispInit();
    tyaUmlDispParamReset(&windows[1], 0);

    windows[0].mode = 2;
    windows[0].texture_x = 0x710;
    windows[0].texture_y = 0x730;
    windows[0].texture_width = 0x1e0;
    windows[0].texture_height = 0xc8;
    windows[0].view_width = 0x1e0;
    windows[0].view_height = 0x180;

    windows[1].mode = 3;
    windows[1].texture_x = 0x710;
    windows[1].texture_y = 0x808;
    windows[1].texture_width = 0x1e0;
    windows[1].texture_height = 0xc8;
    windows[1].view_width = 0x1e0;
    windows[1].view_height = 0x180;

    while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2CD0);
        if ((PadData.buttons.input.repeat & 0x1000U) != 0) {
            windows[0].scroll_y -= 8;
        }
        if ((PadData.buttons.input.repeat & 0x4000U) != 0) {
            windows[0].scroll_y += 8;
        }
        if ((PadData.buttons.input.repeat & 0x8000U) != 0) {
            windows[0].scroll_x -= 8;
        }
        if ((PadData.buttons.input.repeat & 0x2000U) != 0) {
            windows[0].scroll_x += 8;
        }
        if ((PadData.buttons.input.pressed & 8U) != 0) {
            windows[0].mode = 0;
        }
        if ((PadData.buttons.input.pressed & 2U) != 0) {
            windows[0].mode = 1;
        }
        if ((PadData.buttons.input.pressed & 0x10U) != 0) {
            windows[0].mode = 2;
        }
        if ((PadData.buttons.input.pressed & 0x20U) != 0) {
            windows[0].mode = 3;
        }
        if ((PadData.buttons.input.pressed & 0x80U) != 0) {
            windows[0].mode = 4;
        }
        if ((PadData.buttons.input.pressed & 0x40U) != 0) {
            windows[0].mode = 5;
        }
        tyaUmlDispMain(&windows[0]);
        tyaUmlDispMain(&windows[1]);
        xglSleep();
    }
}

static void e_test(ETestContext *context)
{
    sceVif1PkCloseDirectHLCode(context->packet);
    sceVif1PkRef(context->packet, (unsigned char *)&WinTexEnv_65,
                 7, 0, 0, 0);
    sceVif1PkRef(context->packet, (unsigned char *) test_data + 0x30, 0x4002, 0, 0, 0);
    sceVif1PkCnt(context->packet, 0);
    sceVif1PkOpenDirectHLCode(context->packet, 0);
    sceVif1PkAddDirectDataN(context->packet, TestEnv_66, 8);
    xglFontReloadTexture(context, 0);
}

void ReLoadTest(void)
{
    xglRenderClearFrame();
    xglSleep();
    test_data = (void *)(((u32)WorkEnd + 0x0fU) & 0xfffffff0U);
    WorkEnd = (u8 *)test_data +
        xglCdReadFile(D_004C2CF0, test_data, 0, 0);

    while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2D08);
        xglFontPrint(0x40, 0x28, 0x00ffffff, D_004C2D18);
        xglFontPrintExtFunc(0x01ffffffU, e_test, 0);
        xglFontPrint(0x40, 0x48, 0x02ffffff, D_004C2D18);
        xglSleep();
    }
}

static void endCallback(int result, int amount)
{
    switch (result) {
    case 0: {
        const char *source = (const char *)amount;
        char *destination = fname_67;
        do {
            *destination = *source++;
        } while (((unsigned int)*destination++ << 24) != 0);
        break;
    }
    case 1:
        size_68 = I2F(amount) * progress_percent;
        break;
    case 2:
    case 3:
        if (result != 3)
            xglSleep();
        break;
    case -1: {
        for (;;) {
            xglFontDebugPrintf(8, 216, not_found_format, fname_67);
            xglSleep();
        }
    }
    case -2: {
        for (;;) {
            xglFontDebugPrintf(8, 216, read_error_format, fname_67);
            xglSleep();
        }
    }
    case 4:
        break;
    default:
        break;
    }
}

typedef struct { unsigned char unmodeled_00[0x28]; unsigned long long button_state; } FileLoadPadView;
typedef struct { unsigned char unmodeled_00[0x2C]; unsigned short rep; } UmnMailDispTestPad;
typedef void (*FileLoadCallback)(int result, int amount);
extern FileLoadCallback xglCdSetCallback(FileLoadCallback callback);
typedef struct FileLoadTestFiles {
    int state;
    unsigned char *buffer;
    int pad[2];
} FileLoadTestFiles;
typedef struct FileLoadTestEnv {
    u8 unmodeled_00[0x30];
    u64 packetWord;
} FileLoadTestEnv;
extern FileLoadTestFiles files;
extern unsigned char eNumberTexEnv_69[];
extern FileLoadTestEnv TestEnv_70;
extern int x_71;
extern const char D_004C2D80[];
extern const char D_004C2D98[];

void FileLoadTest(void)
{
    XglPacket *packet;
    FileLoadCallback old;

    xglRenderClearFrame();
    xglSleep();
    files.buffer = (unsigned char *)(((int)WorkEnd + 0x7F) & ~0x7F);
    WorkEnd = files.buffer + 0x100000;
    old = xglCdSetCallback(endCallback);
    xglCdReadFile(D_004C2D80, files.buffer, 0, 0);
    files.state = 1;
    while ((((FileLoadPadView *)&PadData)->button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2D98);
        if (files.state == 1) {
            packet = xglPacketGetCurrent();
            sceVif1PkRef(packet, eNumberTexEnv_69, 6, 0, 0, 0);
            sceVif1PkRef(packet, files.buffer + 0x30, 0x4002, 0, 0, 0);
            xglFontDebugHex(0, 0x20, x_71, 5);
            if (((UmnMailDispTestPad *)&PadData)->rep & 0x2000) {
                x_71 += 0x100;
            }
            if (((UmnMailDispTestPad *)&PadData)->rep & 0x8000) {
                x_71 -= 0x100;
            }
            TestEnv_70.packetWord = ((u64)((unsigned int)(x_71 + 0xF0000) >> 6) << 37) | 0x2000000669343C00ULL;
            sceVif1PkRef(packet, &TestEnv_70, 10, 0, 0, 0);
        }
        xglSleep();
    }
    xglCdSetCallback(old);
}

void JpegTest(void)
{
    JpegTestDisplay display;
    JpegTestDecodeRequest request;
    u8 *source_buffer;
    u8 *destination_buffer;

    source_buffer = (u8 *)(((u32)WorkEnd + 0x7fU) & 0xffffff80U);
    WorkEnd = source_buffer + 0x80000;
    memset(source_buffer, 0, 0x80000);

    destination_buffer = (u8 *)(((u32)WorkEnd + 0x7fU) & 0xffffff80U);
    WorkEnd = destination_buffer + 0x80000;
    memset(destination_buffer, 0, 0x80000);

    GameSnapShotSaveThumbnail(100, source_buffer);

    memset(&request, 0, sizeof(request));
    request.source = source_buffer;
    request.destination = destination_buffer;
    request.source_width = 0;
    request.source_height = 0;
    request.max_width = 0;
    request.max_height = 0;
    xglJpegDecode(&request);

    display.x = 100;
    display.y = 100;
    display.color = 0x00ffffffU;
    display.alpha = -0x80;
    display.blue = -0x80;
    display.green = -0x80;
    display.red = -0x80;
    display.image_data = destination_buffer;

    while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2DA8);
        endPrintExtFunc(0, 0x0d, &display);
        xglSleep();
    }
}

extern const char D_004C2DB8[];
extern const char D_004C2DD0[];
extern const char D_004C2DE0[];
extern char *buffer_72;
extern char mm_73[];

void UmlTest(void)
{
  char *buffer;
  unsigned char *p;
  char *d;
  unsigned char c;
  int ci;
  unsigned char buf[16];
  int n;
  int i;
  int mul;
  unsigned char val;
  unsigned char k;
  unsigned char m;
  xglRenderClearFrame();
  xglSleep();
  buffer = (char *) ((((int) WorkEnd) + 0x7F) ^ 0x7F);
  WorkEnd = ((u8 *) buffer) + 0x20000;
  buffer_72 = buffer;
  memset(buffer, 0, 0x10000);
  xglCdReadFile(D_004C2DB8, buffer, 0, 0);
  d = mm_73;
  p = (unsigned char *) buffer_72;
  ci = *p;
  goto test;
  do
  {
    ;
    if ((unsigned char) ci == 0x5C)
    {
      ++p;
      c = *p;
      if (c == 0x74)
      {
        p++;
        p++;
        n = 0;
        val = 0;
        while (((unsigned char) ((*p) + 0xD0)) < 10)
        {
          buf[n++] = (*p) + 0xD0;
          p++;
        }

        mul = 1;
        for (i = 0; i < (n - 1); i++)
        {
          mul *= 10;
        }

        for (i = 0; i < n; i++)
        {
          val += buf[i] * mul;
          mul /= 10;
        }

        *(d++) = val;
        *(d++) = '\t';
        p++;
        {
          unsigned char kk = 2;
          unsigned char mm = 0;
          for (; kk != 0; kk--)
          {
            unsigned char v2 = 0;
            int j;
            int ml;
            while (((unsigned char) ((*p) + 0xD0)) < 10)
            {
              buf[mm] = (*p) + 0xD0;
              mm++;
              p++;
            }

            ml = 1;
            for (j = 0; j < (mm - 1); j++)
            {
              ml *= 10;
            }

            for (j = 0; j < mm; j++)
            {
              v2 += buf[j] * ml;
              ml /= 10;
            }

            p++;
          }

        }
      }
    }
    else
    {
      *(d++) = (unsigned char) ci;
      p++;
    }
    test:
    ;

    ;
    ci = *p;
    ;
  }
  while (ci != 0);
  *d = 0;
  while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U)
  {
    xglFontDebugPrintf(0, 0, D_004C2DD0);
    xglFontPrint(0, 0x40, 0xFFFFF0, D_004C2DE0);
    xglFontPrint(0, 0x40, 0xFFFFFF, mm_73);
    xglSleep();
  }

}

void PauseMenuPagePartyDebugTakeAgws(void)
{
    int i;

    switch (PadData.buttons.input.repeat) {
    case 0x8000:
        Select_74 -= 1;
        break;
    case 0x2000:
        Select_74 += 1;
        break;
    case 0x1000:
        Select_74 -= 2;
        break;
    case 0x4000:
        Select_74 += 2;
        break;
    default:
        break;
    }

    if (Select_74 < 0) {
        Select_74 += 0xc;
    }
    Select_74 %= 0xc;

    if (PadData.buttons.input.pressed & 0x20) {
        if (PartyTakeAgwsCheck(Select_74 + 0x11) != 0) {
            PartyTakeAgwsOff(Select_74 + 0x11);
        } else {
            PartyTakeAgwsOn(Select_74 + 0x11);
        }
    }

    xglFontDebugPrintf(8, 0x20, D_004C2DF0);
    xglFontDebugPrintf((Select_74 % 2) * 0x80 + 8, (Select_74 / 2) * 0x10 + 0x30, D_004DA768);
    for (i = 0; i < 0xc; i++) {
        xglFontDebugPrintf((i % 2) * 0x80 + 0x10, (i / 2) * 0x10 + 0x30, MenuCharNameGet(i + 0x11));
        xglFontDebugPrintf((i % 2) * 0x80 + 0x68, (i / 2) * 0x10 + 0x30, text00_75[PartyTakeAgwsCheck(i + 0x11)]);
    }
    xglFontDebugPrintf(8, 0xc0, D_004C2E08);
}

void PauseMenuPagePartyDebugLockParty(void)
{
    int i;

    switch (PadData.buttons.input.repeat) {
    case 0x8000:
        Select_76 -= 1;
        break;
    case 0x2000:
        Select_76 += 1;
        break;
    case 0x1000:
        Select_76 -= 2;
        break;
    case 0x4000:
        Select_76 += 2;
        break;
    default:
        break;
    }

    if (Select_76 < 0) {
        Select_76 += 0xc;
    }
    Select_76 %= 0xc;

    if (PadData.buttons.input.pressed & 0x20) {
        if (PartyLockPartyCheck(Select_76 + 1) != 0) {
            PartyLockPartyOff(Select_76 + 1);
        } else {
            PartyLockPartyOn(Select_76 + 1);
        }
    }

    xglFontDebugPrintf(8, 0x20, D_004C2E38);
    xglFontDebugPrintf((Select_76 % 2) * 0x80 + 8, (Select_76 / 2) * 0x10 + 0x30, D_004DA768);
    for (i = 0; i < 0xc; i++) {
        xglFontDebugPrintf((i % 2) * 0x80 + 0x10, (i / 2) * 0x10 + 0x30, MenuCharNameGet(i + 1));
        xglFontDebugPrintf((i % 2) * 0x80 + 0x58, (i / 2) * 0x10 + 0x30, text00_77[PartyLockPartyCheck(i + 1)]);
    }
    xglFontDebugPrintf(8, 0xc0, D_004C2E58);
}

void PauseMenuPagePartyDebugOutFriend(void)
{
    int i;

    switch (PadData.buttons.input.repeat) {
    case 0x8000:
        Select_78 -= 1;
        break;
    case 0x2000:
        Select_78 += 1;
        break;
    case 0x1000:
        Select_78 -= 2;
        break;
    case 0x4000:
        Select_78 += 2;
        break;
    default:
        break;
    }

    if (Select_78 < 0) {
        Select_78 += 0xc;
    }
    Select_78 %= 0xc;

    if (PadData.buttons.input.pressed & 0x20) {
        if (PartyOutFriendCheck(Select_78 + 1) != 0) {
            PartyOutFriendOff(Select_78 + 1);
        } else {
            PartyOutFriendOn(Select_78 + 1);
        }
    }

    xglFontDebugPrintf(8, 0x20, D_004C2E88);
    xglFontDebugPrintf((Select_78 % 2) * 0x80 + 8, (Select_78 / 2) * 0x10 + 0x30, D_004DA768);
    for (i = 0; i < 0xc; i++) {
        xglFontDebugPrintf((i % 2) * 0x80 + 0x10, (i / 2) * 0x10 + 0x30, MenuCharNameGet(i + 1));
        xglFontDebugPrintf((i % 2) * 0x80 + 0x58, (i / 2) * 0x10 + 0x30, text00_79[PartyOutFriendCheck(i + 1)]);
    }
    xglFontDebugPrintf(8, 0xc0, D_004C2E58);
}

void PauseMenuPagePartyDebugFriend(void)
{
    int i;

    switch (PadData.buttons.input.repeat) {
    case 0x8000:
        Select_80 -= 1;
        break;
    case 0x2000:
        Select_80 += 1;
        break;
    case 0x1000:
        Select_80 -= 2;
        break;
    case 0x4000:
        Select_80 += 2;
        break;
    default:
        break;
    }

    if (Select_80 < 0) {
        Select_80 += 7;
    }
    Select_80 %= 7;

    if (PadData.buttons.input.pressed & 0x20) {
        if (PartyFriendCheck(Select_80 + 1) != 0) {
            PartyFriendOff(Select_80 + 1);
        } else {
            PartyFriendOn(Select_80 + 1);
        }
    }

    xglFontDebugPrintf(8, 0x20, D_004C2EA8);
    xglFontDebugPrintf((Select_80 % 2) * 0x80 + 8, (Select_80 / 2) * 0x10 + 0x30, D_004DA768);
    for (i = 0; i < 7; i++) {
        xglFontDebugPrintf((i % 2) * 0x80 + 0x10, (i / 2) * 0x10 + 0x30, MenuCharNameGet(i + 1));
        xglFontDebugPrintf((i % 2) * 0x80 + 0x58, (i / 2) * 0x10 + 0x30, text00_81[PartyFriendCheck(i + 1)]);
    }
    xglFontDebugPrintf(8, 0xc0, D_004C2EC0);
}

extern int Select_82;
extern char *text00_83[];
extern char *text01_84[];
extern char *text02_85[];
extern char D_004C2F10[];
extern char D_004C2F30[];
unsigned char *PartyDataGet(void);
typedef struct { unsigned short id; signed char lv; unsigned char pad; } AttackerEntry;

int PauseMenuPagePartyDebugAttacker(void)
{
    int i;
    int y;
    unsigned char *party;
    unsigned char *p;

    if (PadData.buttons.input.repeat & 0x1000) {
        if (Select_82 == 0) {
            Select_82 = 5;
        } else {
            Select_82 -= 1;
        }
    }
    if (PadData.buttons.input.repeat & 0x4000) {
        if (Select_82 == 5) {
            Select_82 = 0;
        } else {
            Select_82 += 1;
        }
    }
    if (Select_82 < 3) {
        unsigned int v = *(unsigned short *)(PartyDataGet() + Select_82 * 4 + 0x30);
        unsigned short rep = PadData.buttons.input.repeat;
        if (rep & 0x8000) {
            v = v != 0 ? v - 1 : 0x1D;
        }
        if (rep & 0x2000) {
            v = v != 0x1D ? v + 1 : 0;
        }
        *(unsigned short *)(PartyDataGet() + Select_82 * 4 + 0x30) = v;
    } else {
        int v = *(signed char *)(PartyDataGet() + Select_82 * 4 + 0x26);
        unsigned short rep = PadData.buttons.input.repeat;
        if (rep & 0x8000) {
            v = v != 0 ? v - 1 : 9;
        }
        if (rep & 0x2000) {
            v = v != 9 ? v + 1 : 0;
        }
        *(signed char *)(PartyDataGet() + Select_82 * 4 + 0x26) = v;
    }
    party = PartyDataGet();
    xglFontDebugPrintf(8, 0x20, D_004C2F10);
    xglFontDebugPrintf(8, Select_82 * 16 + 0x30, D_004DA768);
    for (i = 0; i < 3; i++) {
        y = i * 16 + 0x30;
        xglFontDebugPrintf(0x10, y, text01_84[i]);
        xglFontDebugPrintf(0x30, y, text02_85[0]);
        xglFontDebugPrintf(0x70, y, MenuCharNameGet(*(unsigned short *)(party + i * 4 + 0x30)));
    }
    for (i = 0; i < 3; i++) {
        y = i * 16 + 0x60;
        xglFontDebugPrintf(0x10, y, text01_84[i]);
        xglFontDebugPrintf(0x30, y, text02_85[1]);
        xglFontDebugPrintf(0x70, y, text00_83[((AttackerEntry *)(party + 0x30))[i].lv]);
    }
    xglFontDebugPrintf(8, 0xC0, D_004C2F30);
}

extern int TopMode_86;
extern char D_004C2F68[];
extern char D_004C2F90[];
int PauseMenuPagePartyDebugAttacker(void);

int PauseMenuPagePartyDebug(void)
{
    unsigned int x;

    xglFontDebugPrintf(8, 0x10, D_004C2F68);
    xglFontDebugPrintf(8, 0xB0, D_004C2F90);
    if ((PadData.buttons.input.repeat & 0x80) != 0) {
        if (TopMode_86 == 4) {
            TopMode_86 = 0;
        } else {
            TopMode_86 += 1;
        }
    }
    x = TopMode_86 < 5U;
    switch (TopMode_86) {
    case 0:
        return ((int (*)(void))PauseMenuPagePartyDebugFriend)();
    case 1:
        return PauseMenuPagePartyDebugAttacker();
    case 2:
        return ((int (*)(void))PauseMenuPagePartyDebugOutFriend)();
    case 3:
        return ((int (*)(void))PauseMenuPagePartyDebugLockParty)();
    case 4:
        x = ((int (*)(void))PauseMenuPagePartyDebugTakeAgws)();
        break;
    default:
        break;
    }
    return x;
}

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


#include "main/control_entry.h"

#include "main/xgl_cd.h"

#include "main/xgl_font.h"




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
        xglFontDebugPrintf(0, 0x10, "\013\300\266\273\273\262\350\314\314\245\306\245\271\245\310\241\241\241\241\242\244");
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x10) != 0) {
            SeisanTest();
        }

        xglFontDebugPrintf(0, 0x20, "\013\245\306\245\255\245\271\245\310\241\241\242\242");
        if ((ENDOU_PAD_INPUT->buttons.halves.half_28 & 0x80) != 0) {
            TextTest();
        }

        xglFontDebugPrintf(0, 0x30, "\013\245\341\245\303\245\273\241\274\245\270\245\306\245\271\245\310\241\241\241\337");
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x40) != 0) {
            eMessageTest();
        }

        xglFontDebugPrintf(0, 0x40, "\013\244\244\244\355\244\244\244\355\245\306\245\271\245\310\241\241\241\241\241\373");
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x20) != 0) {
            eNumberTest();
        }

        xglFontDebugPrintf(0, 0x50, "\013\245\342\245\307\245\353\245\306\245\271\245\310\241\241\241\241\243\314\243\261");
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x4) != 0) {
            eModelTest();
        }

        xglFontDebugPrintf(0, 0x60, "\013\245\355\241\274\245\311\245\306\245\271\245\310\241\241\241\241\243\314\243\262");
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x1) != 0) {
            MenuFileMain(1);
        }

        xglFontDebugPrintf(0, 0x70, "\013\245\306\245\257\245\271\245\301\245\343\245\306\245\271\245\310\243\322\243\261");
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x8) != 0) {
            TexturTest();
        }

        xglFontDebugPrintf(0, 0x80, "\013\243\312\243\320\243\305\243\307\245\306\245\271\245\310\241\241\243\322\243\262");
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x2) != 0) {
            JpegTest();
        }

        xglFontDebugPrintf(0, 0x90, "\013\245\320\245\310\245\353\245\246\245\243\245\363\245\311\241\274\241\241\242\254");
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x1000) != 0) {
            BattleWindowTest();
        }

        xglFontDebugPrintf(0, 0xa0, "\013\245\273\241\274\245\326\245\306\245\271\245\310\241\241\241\241\241\241\242\255");
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x4000) != 0) {
            MenuFileMain(0);
        }

        xglFontDebugPrintf(0, 0xb0, "\013\245\342\245\307\245\353\245\306\245\271\245\310\241\241\241\241\241\241\242\252");
        if ((ENDOU_PAD_INPUT->buttons.halves.half_2a & 0x2000) != 0) {
            ModelTest();
        }

        xglFontDebugPrintf(0, 0xc0, "\013\245\267\245\347\245\303\245\327\245\306\245\271\245\310\241\241\241\241\242\253");
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

    while (xglFontDebugPrintf(0, 0x10, "\013\245\306\245\255\245\271\245\310\245\306\245\271\245\310"),
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
extern const char D_004DA668[], D_004DA670[], D_004DA678[], D_004DA680[], D_004DA688[];
extern const char D_004DA690[], D_004DA698[], D_004DA6A0[], D_004DA6A8[], D_004DA6B0[];
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
    WorkEnd = (u8 *)shop_file + xglCdReadFile("data\\endou\\shop\\shopdata.bin", shop_file, 0, 1);
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
        xglFontDebugPrintf(0, 0x10, "\013\245\267\245\347\245\303\245\327\245\306\245\271\245\310");
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
            xglFontDebugPrintf(0, 0x20, "\013\245\312\245\363\245\320\241\274\245\301\245\247\245\363\245\270\241\241\242\254\242\255");
            xglFontDebugPrintf(0x80, 0x20, "\013\274\302\271\324\241\241\241\373\241\241\241\241\275\252\316\273\241\241\241\337");
            endouShopHeading(0x30, "\013ShopNo = %2d", menu_selection);

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
                                   (row_index / 10) * 8 + 0x40, "%3d",
                                   *entry_values);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x50, "%3d",
                                   neighbor_values[-0x78]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x60, "%3d",
                                   entry_values[0x28]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x70, "%3d",
                                   neighbor_values[-0x50]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x80, "%3d",
                                   entry_values[0x50]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0x90, "%3d",
                                   neighbor_values[-0x28]);
                ab_column_offset = 6 * 20;
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0xa0, "%3d",
                                   entry_values[ab_column_offset]);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0xb0, "%3d",
                                   *neighbor_values++);
                xglFontDebugPrintf((row_index % 10) * 0x18 + 0x10,
                                   (row_index / 10) * 8 + 0xc0, "%3d",
                                   (entry_values++)[0xa0]);
            } while (++row_index < 0x14);
            for (row_index = 0; row_index < 4; row_index++) {
                int x = (row_index % 10) * 0x18 + 0x10;
                int y = (row_index / 10) * 8;

                xglFontDebugPrintf(x, y + 0xd0, "%3d",
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

#define PAD_L2       0x0001
#define PAD_R2       0x0002
#define PAD_L1       0x0004
#define PAD_R1       0x0008
#define PAD_TRIANGLE 0x0010
#define PAD_CIRCLE   0x0020
#define PAD_SELECT   0x0100
#define PAD_START    0x0800
#define PAD_UP       0x1000
#define PAD_RIGHT    0x2000
#define PAD_DOWN     0x4000
#define PAD_LEFT     0x8000
#define NULL ((void *)0)
typedef struct EndouModelTestPad {
    unsigned char unmodeled_00[0x28];
    unsigned short held;
    unsigned short pressed;
    unsigned char unmodeled_2c[4];
    unsigned char unmodeled_30[8];
    signed char stick_axis[4];
} EndouModelTestPad;
#define ENDOU_MODEL_TEST_PAD ((EndouModelTestPad *)&PadData)
extern void xglStudioChange(int studio_index);
extern StudioCamera *xglStudioGetCamera2(int camera_id);
typedef Vector4 EndouAlignedVector4 __attribute__((aligned(16)));
typedef Vector4 EndouDwordVector4 __attribute__((aligned(8)));
typedef struct EndouModelActor {
    u8 unmodeled_00[0x10];
    EndouAlignedVector4 position;
    u8 unmodeled_20[0x30];
    EndouAlignedVector4 rotation;
    EndouAlignedVector4 scale;
    u8 unmodeled_70[0x16];
    short inUseId;
    u8 unmodeled_88[0x8d0 - 0x88];
    void *modelResource;
    void *matrixResource;
    void *move;
    void *animPackTables[8];
} EndouModelActor;
#define ENDOU_EXIT_CHORD 0x08000100
#define ENDOU_STICK_DEADZONE 0x40
#define ENDOU_GRID_ROWS 5
#define ENDOU_GRID_COLUMNS 6
#define ENDOU_GRID_POINT_COUNT (ENDOU_GRID_ROWS * ENDOU_GRID_COLUMNS)
#define ENDOU_MODEL_MESSAGE_FRAMES 0x40
#define EMESSAGE_MODE_LINE_START 0x24
#define END_PRINT_CMD_LINE_RAW 11
typedef struct EndouTestLighting {
    EndouDwordVector4 ambient;
    EndouDwordVector4 parallel_intensity[3];
    Vector4 direction[3];
} EndouTestLighting;
typedef struct EndouModelLineVertex {
    int x;
    int y;
    int z;
    int terminator;
} EndouModelLineVertex;
typedef struct EndouModelLinePoint {
    EndouModelLineVertex vertex;
    unsigned char color[4];
    unsigned char unmodeled_14[12];
} EndouModelLinePoint;
typedef struct EndouModelBackQuad {
    int vertices[4][4];
    float uv[4];
} EndouModelBackQuad;
typedef struct EndouModelMessage {
    unsigned char flags;
    unsigned char mode;
    unsigned char visible;
    unsigned char wrap_width_half;
    short x;
    short y;
    int draw_parameter;
    unsigned char unmodeled_0c[0x44 - 0x0c];
} EndouModelMessage;
typedef struct EndouModelMessageWindow {
    EndouModelMessage message;
    u8 unmodeled_44[0xc];
} EndouModelMessageWindow;
struct StudioLight;
extern void xglStudioGetLight(struct StudioLight **light_out);
extern void xglLightIntensityAmbient(void *light, const void *intensity);
extern void xglLightIntensityParallel(void *light, unsigned int index, const void *intensity);
extern void xglLightDirection(void *light, unsigned int index, const Vector4 *direction);
extern StudioCamera *xglStudioGetActiveCamera(void);
extern void GameResourceInit(unsigned int address, int size);
extern EndouModelActor *ACT_create(int slot, int id);
extern void ACT_initMotion(EndouModelActor *actor);
extern int ACT_allocMatrix(EndouModelActor *self, int count);
extern void ACT_modelDraw(EndouModelActor *actor);
extern EndouModelActor *ACT_setArms(EndouModelActor *actor, EndouModelActor *parent, unsigned int accessoryId, int flags);
extern void ACT_setModelWrapper(EndouModelActor *actor, int unused);
extern void ACT_setMotion(EndouModelActor *actor, int motionId);
extern void ACT_updateMotion(EndouModelActor *actor);
extern void nmlModelFlush(void);
extern void RES_GetMdlFileName(char *destination, int selector);
extern char *strcat(char *destination, const char *source);
extern char *strcpy(char *destination, const char *source);
extern int xglCdGetFileSize(const char *name);
extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
extern void xglMatrixStackRotX(float angle);
extern void xglMatrixStackRotY(float angle);
extern void xglMatrixStackRotZ(float angle);
extern void xglMatrixStackSave(float matrix[4][4]);
extern void xglMatrixStackTrans(const float translation[4]);
extern void xglMatrixStackUnit(void);
extern void xglRotTransPersN(void *destination, Matrix4 *matrix, const void *points, int count, int cameraId);
extern const EndouTestLighting D_004C25E0;
extern const char D_004C2658[];
extern const EndouDwordVector4 D_004C2720[4];
extern const char D_004DA6C0[8];
extern const char D_004DA6C8[8];
extern unsigned char D_004DA6E0[];
extern unsigned char D_004DA6E8[];
extern float ang_0[4];
extern float pos_1[4];
extern f32 pipo_2;
extern int time_10;
extern int no_13;
extern const char *msg00_14[4];
extern EndouModelMessageWindow msg_11;
extern EndouModelLinePoint line00_3[7];
extern EndouModelLinePoint line01_4[7];
extern EndouModelLinePoint line02_5[7];
extern EndouModelLinePoint line03_6[7];
extern EndouModelLinePoint line10_7[7];
extern EndouModelLinePoint line11_8[7];
extern EndouModelLinePoint line12_9[7];
extern EndouModelBackQuad bsp_12;
#define modelTestLighting D_004C25E0
#define modelTestFpkName D_004C2658
#define modelTestMotionName "newd\\motion\\chaos_cf.fpk"
#define modelTestQuadWordTable D_004C2720
#define modelTestLexExtension D_004DA6C0
#define modelTestXtxExtension D_004DA6C8
#define modelTestJntExtension ".jnt"
#define modelTestColumnAlphaTable D_004DA6E0
#define modelTestRowAlphaTable D_004DA6E8
#define GAME_RESOURCE_BASE_ADDRESS 0x07000000
#define GAME_RESOURCE_BUFFER_SIZE 0x06000000
static inline void endModelCameraPosition(StudioCamera *camera, float x, float y, float z)
{
    camera->position.x = x;
    camera->position.y = y;
    camera->position.z = z;
}
static __inline__ void EndouApplyModelLighting(struct StudioLight **light_slot, EndouTestLighting profile)
{
    xglStudioGetLight(light_slot);
    xglLightIntensityAmbient(*light_slot, &profile.ambient);
    xglLightIntensityParallel(*light_slot, 0, &profile.parallel_intensity[0]);
    xglLightDirection(*light_slot, 0, &profile.direction[0]);
    xglLightIntensityParallel(*light_slot, 1, &profile.parallel_intensity[1]);
    xglLightDirection(*light_slot, 1, &profile.direction[1]);
    xglLightIntensityParallel(*light_slot, 2, &profile.parallel_intensity[2]);
    xglLightDirection(*light_slot, 2, &profile.direction[2]);
}

void ModelTest(void) {
    struct StudioLight *light;
    u8 **work_end = &WorkEnd;
    int *message_time = &time_10;
    int motion;

    motion = 1;
    xglStudioChange(0);
    EndouApplyModelLighting(&light, modelTestLighting);
    {
    int point_index;
    StudioCamera *temp_17_4;
    StudioCamera *temp_2;
    StudioCamera *temp_2_2;
    f32 temp_f0_21;
    f32 temp_f0_22;
    f32 temp_f0_23;
    f32 temp_f0_24;
    f32 temp_f0_25;
    f32 temp_f0_26;
    f32 temp_f0_27;
    f32 temp_f1;
    f32 temp_f20;
    f32 temp_f2;
    int temp_2_11;
    int temp_2_14;
    int temp_2_16;
    int temp_2_18;
    int temp_2_20;
    int temp_2_22;
    int temp_2_24;
    int temp_2_26;
    int temp_2_5;
    int temp_2_7;
    int temp_2_9;
    u16 temp_16;
    u16 temp_4;
    u16 var_3_2;
    u8 *temp_3;
    u8 *temp_3_10;
    u8 *temp_3_11;
    u8 *temp_3_2;
    u8 *temp_3_3;
    u8 *temp_3_4;
    u8 *temp_3_5;
    u8 *temp_3_6;
    u8 *temp_3_7;
    u8 *temp_3_8;
    u8 *temp_3_9;
    EndouModelActor *temp_23_9;
    EndouModelActor *temp_2_12;
    EndouModelActor *temp_2_3;
    u8 *resource_buffer;
    u8 *accessory_resource_buffer;

    temp_2 = xglStudioGetCamera2(0);
    temp_2->rotation.x = 0.0f;
    temp_2->rotation.y = 0.0f;
    temp_2->rotation.z = 0.0f;
    endModelCameraPosition(temp_2, 0.0f, 1.0f, 4.0f);
    temp_2->active = 1;
    temp_2->state = 1;
    temp_2_2 = xglStudioGetCamera2(1);
    temp_2_2->state = 1;
    endModelCameraPosition(temp_2_2, -1.0f, 1.0f, 4.0f);
    temp_2_2->rotation.y = 0.0f;
    temp_2_2->rotation.z = 0.0f;
    temp_2_2->active = 0;
    temp_2_2->rotation.x = 0.0f;
    GameResourceInit(GAME_RESOURCE_BASE_ADDRESS, GAME_RESOURCE_BUFFER_SIZE);
    ACT_init();
    temp_2_3 = ACT_create(-1, 0x2101);
    ACT_initMotion(temp_2_3);
    {
    char sp70[128] = "\0";
    char spF0[128] = "\0";
    EndouTestLighting unused_lighting;

    RES_GetMdlFileName(sp70, temp_2_3->inUseId);
    resource_buffer = (u8 *)((u32)((*work_end) + 0x7f) & ~0x7fU);
    strcpy(spF0, sp70);
    strcat(spF0, modelTestLexExtension);
    temp_2_5 = xglCdReadFile(spF0, resource_buffer, 0, 0);
    temp_3 = (*work_end) + temp_2_5;
    temp_2_3->modelResource = resource_buffer;
    (*work_end) = temp_3;
    resource_buffer = (u8 *)((u32)(temp_3 + 0x7f) & ~0x7fU);
    strcpy(spF0, sp70);
    strcat(spF0, modelTestXtxExtension);
    temp_2_7 = xglCdReadFile(spF0, resource_buffer, 0, 0);
    temp_3_2 = (*work_end) + temp_2_7;
    temp_2_3->matrixResource = resource_buffer;
    (*work_end) = temp_3_2;
    resource_buffer = (u8 *)((u32)(temp_3_2 + 0x7f) & ~0x7fU);
    strcpy(spF0, sp70);
    strcat(spF0, modelTestJntExtension);
    temp_2_9 = xglCdReadFile(spF0, resource_buffer, 0, 0);
    }
    temp_3_3 = ((*work_end) + temp_2_9);
    temp_2_3->move = resource_buffer;
    resource_buffer = (u8 *)((u32)(temp_3_3 + 0x7f) & ~0x7fU);
    (*work_end) = temp_3_3;
    temp_2_11 = xglCdReadFile(modelTestFpkName, resource_buffer, 0, 0);
    temp_3_4 = (*work_end) + temp_2_11;
    temp_2_3->animPackTables[0] = resource_buffer;
    (*work_end) = temp_3_4;
    ACT_allocMatrix(temp_2_3, -1);
    ACT_setModelWrapper(temp_2_3, 0);
    ACT_setMotion(temp_2_3, 1);
    temp_2_12 = ACT_create(-1, 1);
    ACT_initMotion(temp_2_12);
    {
    char sp1E0[128] = "\0";
    char sp260[128] = "\0";

    RES_GetMdlFileName(sp1E0, temp_2_12->inUseId);
    resource_buffer = (u8 *)((u32)((*work_end) + 0x7f) & ~0x7fU);
    strcpy(sp260, sp1E0);
    strcat(sp260, modelTestLexExtension);
    temp_2_14 = xglCdReadFile(sp260, resource_buffer, 0, 0);
    temp_3_5 = (*work_end) + temp_2_14;
    temp_2_12->modelResource = resource_buffer;
    (*work_end) = temp_3_5;
    resource_buffer = (u8 *)((u32)(temp_3_5 + 0x7f) & ~0x7fU);
    strcpy(sp260, sp1E0);
    strcat(sp260, modelTestXtxExtension);
    temp_2_16 = xglCdReadFile(sp260, resource_buffer, 0, 0);
    temp_3_6 = (*work_end) + temp_2_16;
    temp_2_12->matrixResource = resource_buffer;
    (*work_end) = temp_3_6;
    resource_buffer = (u8 *)((u32)(temp_3_6 + 0x7f) & ~0x7fU);
    strcpy(sp260, sp1E0);
    strcat(sp260, modelTestJntExtension);
    temp_2_18 = xglCdReadFile(sp260, resource_buffer, 0, 0);
    }
    temp_3_7 = (*work_end) + temp_2_18;
    temp_2_12->move = resource_buffer;
    resource_buffer = (u8 *)((u32)(temp_3_7 + 0x7f) & ~0x7fU);
    (*work_end) = temp_3_7;
    temp_2_20 = xglCdReadFile(modelTestMotionName, resource_buffer, 0, 0);
    temp_3_8 = (*work_end) + temp_2_20;
    temp_2_12->animPackTables[0] = resource_buffer;
    (*work_end) = temp_3_8;
    ACT_allocMatrix(temp_2_12, -1);
    ACT_setModelWrapper(temp_2_12, 0);
    ACT_setMotion(temp_2_12, 1);
    temp_23_9 = ACT_create(-1, 0x3003);
    {
    char sp2E0[128] = "\0";
    char sp360[128] = "\0";

    RES_GetMdlFileName(sp2E0, temp_23_9->inUseId);
    accessory_resource_buffer = (u8 *)((u32)((*work_end) + 0x7f) & ~0x7fU);
    strcpy(sp360, sp2E0);
    strcat(sp360, modelTestLexExtension);
    temp_2_22 = xglCdReadFile(sp360, accessory_resource_buffer, 0, 0);
    temp_3_9 = (*work_end) + temp_2_22;
    temp_23_9->modelResource = accessory_resource_buffer;
    (*work_end) = temp_3_9;
    accessory_resource_buffer = (u8 *)((u32)(temp_3_9 + 0x7f) & ~0x7fU);
    strcpy(sp360, sp2E0);
    strcat(sp360, modelTestXtxExtension);
    temp_2_24 = xglCdReadFile(sp360, accessory_resource_buffer, 0, 0);
    temp_3_10 = (*work_end) + temp_2_24;
    temp_23_9->matrixResource = accessory_resource_buffer;
    (*work_end) = temp_3_10;
    accessory_resource_buffer = (u8 *)((u32)(temp_3_10 + 0x7f) & ~0x7fU);
    strcpy(sp360, sp2E0);
    strcat(sp360, modelTestJntExtension);
    if (xglCdGetFileSize(sp360) < 0) {
        temp_23_9->move = NULL;
    } else {
        temp_2_26 = xglCdReadFile(sp360, accessory_resource_buffer, 0, 0);
        temp_3_11 = (*work_end) + temp_2_26;
        temp_23_9->move = accessory_resource_buffer;
        (*work_end) = temp_3_11;
    }
    }
    ACT_allocMatrix(temp_23_9, -1);
    ACT_setModelWrapper(temp_23_9, 0);
    ACT_setArms(temp_23_9, temp_2_3, 0x108U, 0);
    {
        while ((PadData.buttons.button_state & ENDOU_EXIT_CHORD) != ENDOU_EXIT_CHORD) {
            var_3_2 = ENDOU_MODEL_TEST_PAD->pressed;
            if (var_3_2 & PAD_R1) {
                ACT_setMotion(temp_2_3, ++motion);
                var_3_2 = ENDOU_MODEL_TEST_PAD->pressed;
            }
            if (var_3_2 & PAD_R2) {
                ACT_setMotion(temp_2_3, --motion);
            }
            temp_4 = ENDOU_MODEL_TEST_PAD->held;
            if (temp_4 & PAD_SELECT) {
                EndouAlignedVector4 reset_position = {0.0f, 0.0f, 0.0f, 1.0f};
                EndouAlignedVector4 reset_rotation = {0.0f, 0.0f, 0.0f, 1.0f};
                EndouAlignedVector4 reset_scale = {1.0f, 1.0f, 1.0f, 1.0f};
                temp_4 = ENDOU_MODEL_TEST_PAD->held;
                temp_2_3->position = reset_position;
                temp_2_3->rotation = reset_rotation;
                temp_2_3->scale = reset_scale;
            }
            {
            Matrix4 camera_matrix;
            float world_points[ENDOU_GRID_ROWS][ENDOU_GRID_COLUMNS][4];
            EndouModelLineVertex screen_points[ENDOU_GRID_ROWS][ENDOU_GRID_COLUMNS];
            Matrix4 projection_matrix;
            unsigned char column_alphas[ENDOU_GRID_ROWS];
            unsigned char row_alphas[ENDOU_GRID_COLUMNS];
            EndouDwordVector4 quad_words[4];
            if (temp_4 & PAD_TRIANGLE) {
                if (temp_4 & PAD_RIGHT) {
                    temp_2_3->scale.x += 0.1f;
                }
                if (temp_4 & PAD_LEFT) {
                    temp_2_3->scale.x -= 0.1f;
                }
                if (temp_4 & PAD_UP) {
                    temp_2_3->scale.y += 0.1f;
                }
                if (temp_4 & PAD_DOWN) {
                    temp_2_3->scale.y -= 0.1f;
                }
                if (temp_4 & PAD_L1) {
                    temp_2_3->scale.z += 0.1f;
                }
                if ((unsigned short)(temp_4 & PAD_L2)) {
                    temp_2_3->scale.z -= 0.1f;
                }
            } else {
                if (temp_4 & PAD_RIGHT) {
                    temp_2_3->position.x += 0.1f;
                }
                if (temp_4 & PAD_LEFT) {
                    temp_2_3->position.x -= 0.1f;
                }
                if (temp_4 & PAD_UP) {
                    temp_2_3->position.y += 0.1f;
                }
                if (temp_4 & PAD_DOWN) {
                    temp_2_3->position.y -= 0.1f;
                }
                if (temp_4 & PAD_L1) {
                    temp_2_3->position.z += 0.1f;
                }
                if ((unsigned short)(temp_4 & PAD_L2)) {
                    temp_2_3->position.z -= 0.1f;
                }
            }
            if (ENDOU_MODEL_TEST_PAD->stick_axis[0] < -ENDOU_STICK_DEADZONE) {
                temp_2_3->rotation.y += 0.04f;
            }
            if (ENDOU_MODEL_TEST_PAD->stick_axis[0] >= ENDOU_STICK_DEADZONE + 1) {
                temp_2_3->rotation.y -= 0.04f;
            }
            if (ENDOU_MODEL_TEST_PAD->stick_axis[1] < -ENDOU_STICK_DEADZONE) {
                temp_2_3->rotation.x += 0.04f;
            }
            if (ENDOU_MODEL_TEST_PAD->stick_axis[1] >= ENDOU_STICK_DEADZONE + 1) {
                temp_2_3->rotation.x -= 0.04f;
            }
            temp_17_4 = xglStudioGetCamera2(0);
            if (ENDOU_MODEL_TEST_PAD->stick_axis[2] < -ENDOU_STICK_DEADZONE) {
                ang_0[1] += 0.04f;
            }
            if (ENDOU_MODEL_TEST_PAD->stick_axis[2] >= ENDOU_STICK_DEADZONE + 1) {
                ang_0[1] -= 0.04f;
            }
            if (ENDOU_MODEL_TEST_PAD->stick_axis[3] < -ENDOU_STICK_DEADZONE) {
                ang_0[0] += 0.04f;
            }
            if (ENDOU_MODEL_TEST_PAD->stick_axis[3] >= ENDOU_STICK_DEADZONE + 1) {
                ang_0[0] -= 0.04f;
            }
            xglMatrixStackUnit();
            xglMatrixStackRotZ(ang_0[2]);
            xglMatrixStackRotY(ang_0[1]);
            xglMatrixStackRotX(ang_0[0]);
            xglMatrixStackTrans(pos_1);
            xglMatrixStackSave(camera_matrix);
            temp_f0_21 = ang_0[0];
            temp_16 = ENDOU_MODEL_TEST_PAD->held;
            temp_17_4->rotation.x = temp_f0_21;
            temp_f0_22 = ang_0[1];
            temp_17_4->rotation.y = temp_f0_22;
            temp_f1 = ang_0[2];
            temp_17_4->rotation.z = temp_f1;
            temp_f2 = camera_matrix[3][0];
            temp_17_4->position.x = temp_f2;
            temp_f0_23 = camera_matrix[3][1] + 1.0f;
            temp_17_4->position.y = temp_f0_23;
            temp_f20 = camera_matrix[3][2];
            temp_17_4->position.z = temp_f20;
            if (temp_16 & PAD_R1) {
                temp_f0_24 = pipo_2 + 0.2;
                pipo_2 = temp_f0_24;
            }
            if (temp_16 & PAD_R2) {
                temp_f0_26 = pipo_2 - 0.2;
                pipo_2 = temp_f0_26;
            }
            temp_f0_25 = pipo_2;
            temp_f0_27 = temp_f20 + temp_f0_25;
            temp_17_4->position.z = temp_f0_27;
            temp_2_12->position = temp_2_3->position;
            temp_2_12->position.x += 1.0f;
            temp_2_12->rotation = temp_2_3->rotation;
            temp_2_12->scale = temp_2_3->scale;
            ACT_updateMotion(temp_2_3);
            ACT_updateMotion(temp_2_12);
            xglStudioGetActiveCamera()->active = 0;
            xglStudioGetCamera2(0)->active = 1;
            ACT_modelDraw(temp_2_3);
            xglStudioGetActiveCamera()->active = 0;
            xglStudioGetCamera2(0)->active = 1;
            ACT_modelDraw(temp_2_12);
            nmlModelFlush();
            {
                int row;

                for (point_index = 0; point_index < ENDOU_GRID_COLUMNS; point_index++) {
                    float x = point_index * 0.5f + -1.25f;

                    for (row = 0; row < ENDOU_GRID_ROWS; row++) {
                        world_points[row][point_index][0] = x;
                        world_points[row][point_index][1] = 0.0f;
                        world_points[row][point_index][3] = 1.0f;
                        world_points[row][point_index][2] = row * 0.5f + -1.0f;
                    }
                }
                {
                    for (point_index = 0; point_index < ENDOU_GRID_COLUMNS; point_index++) {
                        world_points[0][point_index][2] -= 0.5f;
                        world_points[4][point_index][2] += 0.5f;
                    }
                    for (point_index = 0; point_index < ENDOU_GRID_ROWS; point_index++) {
                        world_points[point_index][0][0] -= 0.5f;
                        world_points[point_index][5][0] += 0.5f;
                    }
                }
            }
            __builtin_memcpy(column_alphas, modelTestColumnAlphaTable,
                             sizeof(column_alphas));
            __builtin_memcpy(row_alphas, modelTestRowAlphaTable, sizeof(row_alphas));
            xglMatrixStackUnit();
            xglMatrixStackSave(projection_matrix);
            xglRotTransPersN(screen_points, &projection_matrix, world_points, ENDOU_GRID_POINT_COUNT, 0);
            {
                for (point_index = 0; point_index < ENDOU_GRID_ROWS; point_index++) {
                    unsigned char *color;
                    line00_3[point_index].vertex = screen_points[point_index][1];
                    color = line00_3[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = column_alphas[point_index];
                    line00_3[point_index].vertex.terminator = 1;
                }
                line00_3[5].vertex.terminator = 0;

                {
                for (point_index = 0; point_index < ENDOU_GRID_ROWS; point_index++) {
                    unsigned char *color;
                    line01_4[point_index].vertex = screen_points[point_index][2];
                    color = line01_4[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = column_alphas[point_index];
                    line01_4[point_index].vertex.terminator = 1;
                }
                (line01_4 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < ENDOU_GRID_ROWS; point_index++) {
                    unsigned char *color;
                    line02_5[point_index].vertex = screen_points[point_index][3];
                    color = line02_5[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = column_alphas[point_index];
                    line02_5[point_index].vertex.terminator = 1;
                }
                (line02_5 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < ENDOU_GRID_ROWS; point_index++) {
                    unsigned char *color;
                    line03_6[point_index].vertex = screen_points[point_index][4];
                    color = line03_6[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = column_alphas[point_index];
                    line03_6[point_index].vertex.terminator = 1;
                }
                (line03_6 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < ENDOU_GRID_COLUMNS; point_index++) {
                    unsigned char *color;
                    line10_7[point_index].vertex = screen_points[1][point_index];
                    color = line10_7[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = row_alphas[point_index];
                    line10_7[point_index].vertex.terminator = 1;
                }
                (line10_7 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < ENDOU_GRID_COLUMNS; point_index++) {
                    unsigned char *color;
                    line11_8[point_index].vertex = screen_points[2][point_index];
                    color = line11_8[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = row_alphas[point_index];
                    line11_8[point_index].vertex.terminator = 1;
                }
                (line11_8 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < ENDOU_GRID_COLUMNS; point_index++) {
                    unsigned char *color;
                    line12_9[point_index].vertex = screen_points[3][point_index];
                    color = line12_9[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = row_alphas[point_index];
                    line12_9[point_index].vertex.terminator = 1;
                }
                (line12_9 + point_index)->vertex.terminator = 0;
                }
            }
            endPrintExtFunc(0xFFFFF0, END_PRINT_CMD_LINE_RAW, line00_3);
            endPrintExtFunc(0xFFFFF1, END_PRINT_CMD_LINE_RAW, line01_4);
            endPrintExtFunc(0xFFFFF2, END_PRINT_CMD_LINE_RAW, line02_5);
            endPrintExtFunc(0xFFFFF3, END_PRINT_CMD_LINE_RAW, line03_6);
            endPrintExtFunc(0xFFFFF4, END_PRINT_CMD_LINE_RAW, line10_7);
            endPrintExtFunc(0xFFFFF5, END_PRINT_CMD_LINE_RAW, line11_8);
            endPrintExtFunc(0xFFFFF6, END_PRINT_CMD_LINE_RAW, line12_9);
            __builtin_memcpy(quad_words, modelTestQuadWordTable, sizeof(quad_words));
            endPrintExtFunc(0, 0x6A, NULL);
            endPrintExtFunc(0, 0x6C, NULL);
            if (*message_time == 0) {
                eMessageSet(&msg_11.message, msg00_14[no_13]);
                msg_11.message.mode = EMESSAGE_MODE_LINE_START;
                *message_time = ENDOU_MODEL_MESSAGE_FRAMES;
                no_13 = (no_13 + 1) & 3;
            }
            (*message_time)--;
            msg_11.message.x = 0;
            msg_11.message.draw_parameter = 0xFFFFF0;
            msg_11.message.y = 0;
            eMessageMain(&msg_11.message);
            endPrintExtFunc(0, 0x6B, NULL);
            bsp_12.uv[0] = 0.0f;
            bsp_12.uv[2] = 0.234375f;
            bsp_12.uv[3] = 0.046875f;
            bsp_12.uv[1] = 0.0f;
            xglRotTransPersN(bsp_12.vertices, NULL, quad_words, 4, 0);
            endPrintExtFunc(0, 0x6D, &bsp_12);
            xglSleep();
            }
        }
    }
    }
}

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
            xglFontDebugPrintf(0, 0, "\013\245\320\245\310\245\353\245\246\245\243\245\363\245\311\241\274\245\306\245\271\245\310");
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
                s50.d = "\243\305\245\263\245\363\245\311\245\353\245\321\245\265\241\274";
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

#define PAD_L2       0x0001
#define PAD_R2       0x0002
#define PAD_L1       0x0004
#define PAD_R1       0x0008
#define PAD_TRIANGLE 0x0010
#define PAD_CIRCLE   0x0020
#define PAD_SELECT   0x0100
#define PAD_START    0x0800
#define PAD_UP       0x1000
#define PAD_RIGHT    0x2000
#define PAD_DOWN     0x4000
#define PAD_LEFT     0x8000
#define NULL ((void *)0)
#define ENDOU_MODEL_PAD ((EndouModelPadState *)&PadData)
#define ENDOU_EXIT_CHORD_U 0x08000100U
#define END_PRINT_CMD_LINE 0
#define END_PRINT_CMD_SPRITE 2
#define END_PRINT_CMD_RIBBON 3
#define END_PRINT_CMD_POINT 4
#define END_PRINT_CMD_SPRITE_QUAD 8
#define END_PRINT_CMD_NUMBER 10
#define END_PRINT_CMD_EXT_BASE 100
#define END_PRINT_EXT_FLUSH 0
#define E_MODE_SHOW_START 0x20
#define ELINE_STATE_DRAW_SEQUENTIAL 0x10
#define ELINE_STATE_DRAW_ALL 0x20
typedef struct EndouModelPadState {
    unsigned char unmodeled_00[0x28];
    unsigned short held;
    unsigned short pressed;
    unsigned char unmodeled_2c[4];
    unsigned char unmodeled_30[8];
    signed char stick_axis[4];
} EndouModelPadState;
typedef struct EndouPrintLinePoint {
    int x;
    int y;
    int z;
    u32 unmodeled_0c;
    u8 color[4];
    u8 unmodeled_14[12];
} EndouPrintLinePoint;
typedef struct EndouPrintPoint {
    short x;
    unsigned short y;
    int depth;
    unsigned char color[4];
} EndouPrintPoint;
typedef struct ECursolSprite {
    short x;
    short y;
    int work;
    short width;
    short height;
    signed char color[4];
    short style;
    signed char frame;
    signed char flags;
} ECursolSprite;
typedef struct ECursolMainData {
    unsigned char state;
    unsigned char active;
    unsigned char timer;
    unsigned char width;
    unsigned short x;
    unsigned short y;
    int work;
    unsigned char color[4];
    ECursolSprite sprite;
} ECursolMainData;
typedef struct EndouSpriteQuadVertex {
    int x;
    int y;
    int depth;
    unsigned char unmodeled_00c[4];
} EndouSpriteQuadVertex;
typedef struct EndouSpriteQuad {
    unsigned char flags;
    signed char coordinate_mode;
    unsigned short uv;
    unsigned char unmodeled_004[12];
    EndouSpriteQuadVertex vertices[4];
    unsigned char unmodeled_050[16];
} EndouSpriteQuad;
typedef struct ENumberSpriteDescriptor {
    short x, y;
    int work;
    short width, height;
    unsigned char color[4];
    short spriteId;
    short unmodeled_012;
} ENumberSpriteDescriptor;
typedef struct ENumberRenderDescriptor {
    unsigned short x, y;
    int work;
    union {
        struct {
            signed char red, green, blue, alpha;
            int value;
        } number;
        struct {
            short width, height;
            unsigned char color[4];
        } sprite;
        struct {
            unsigned char color[4];
            const char *text;
        } text;
    } payload;
    unsigned short argument1, argument2;
} ENumberRenderDescriptor;
typedef struct EndouQuadPosition {
    short x;
    short y;
    int depth;
} EndouQuadPosition;
typedef struct EndouQuadColor {
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
} EndouQuadColor;
typedef struct EndouQuadDescriptor {
    EndouQuadPosition points[4];
    EndouQuadColor colors[4];
    unsigned int format;
} EndouQuadDescriptor;
typedef struct EndouELinePoint {
    int x;
    int y;
    int work;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
} EndouELinePoint;
typedef struct EndouELineEntry {
    u8 state;
    u8 unmodeled_001[2];
    u8 duration;
    u8 progress[5];
    u8 unmodeled_009[3];
    EndouELinePoint points[5];
    u32 count;
    EndouELinePoint render[5];
} EndouELineEntry;
typedef struct ENumberMainData {
    short x, y;
    int work;
    unsigned char color_r;
    unsigned char color_g;
    unsigned char color_b;
    unsigned char color_a;
    unsigned char mode;
    unsigned char unmodeled_00d;
    unsigned char format;
    unsigned char digits;
    unsigned char colorModeFirst;
    unsigned char colorModeSecond;
    unsigned char alignment;
    unsigned char firstOnly;
    int value;
    ENumberRenderDescriptor render[4];
    ENumberSpriteDescriptor separators[2];
} ENumberMainData;
typedef struct ERibbonPoint {
    short x;
    short y;
    int work;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
} ERibbonPoint;
typedef struct ERibbonMainData {
    short x;
    short y;
    int work;
    short width;
    short height;
    unsigned char mode;
    unsigned char progress;
    unsigned char visible;
    unsigned char style;
    ERibbonPoint points[6];
    u8 unmodeled_058[4];
    int renderCount;
    u8 unmodeled_060[0x10];
} ERibbonMainData;
typedef struct ESprite {
    signed char state;
    u8 unmodeled_001[3];
    short x;
    short y;
    int work;
    signed char color[4];
    u8 unmodeled_010[0x14];
    short spriteId;
} ESprite;
typedef struct ESpriteMainData ESpriteMainData;
typedef struct ETagFontMainData {
    signed char state;
    signed char subState;
    u8 unmodeled_002[2];
    short x;
    short y;
    int work;
    unsigned char color[4];
    struct {
        short x;
        short y;
        int work;
        unsigned char color[4];
        const char *text;
    } render;
} ETagFontMainData;
typedef union EndouTagFontViews {
    ETagFontMainData control;
    ETagFontMainData draw;
} EndouTagFontViews;
extern void eCursolMain(ECursolMainData *cursol);
extern void eCursolSet(ECursolMainData *cursol, int width);
extern void eLineMain(EndouELineEntry *line);
extern void eLineSet(EndouELineEntry *line, int unused);
extern void endPrintDirectLine(EndouPrintLinePoint *line);
extern void endSpriteSet(void *sprite, int mode);
extern void eNumberMain(ENumberMainData *number);
extern void eNumberSet(ENumberMainData *number, int unused);
extern void eRibbonMain(ERibbonMainData *ribbon);
extern void eRibbonSet(ERibbonMainData *ribbon, int style);
extern void eSpriteMain(ESpriteMainData *sprite);
extern void eSpriteSet(ESprite *sprite, int flags);
extern void eTagFontMain(ETagFontMainData *tag);
extern void eTagFontSet(ETagFontMainData *tag, const char *text);
extern void xglClockRead(XglClock *clock);
extern void xglStudioChange(int studio_index);
extern StudioCamera *xglStudioGetCamera2(int camera_id);
extern void xglStudioGetCamera(StudioCamera **camera_out, int camera_index);
#define eNumberTestLabel "\013\244\244\244\355\244\244\244\355\245\306\245\271\245\310"
extern char moji_26[27];
extern char moji2_27[27];
extern EndouPrintPoint line_34[6];
extern EndouPrintPoint d_42[7];
extern EndouPrintPoint d_43;
extern u8 time_37;
extern ECursolMainData cur_25;
extern EndouSpriteQuad d_28;
extern ENumberSpriteDescriptor d_33;
extern ENumberSpriteDescriptor d_35[2];
extern ENumberSpriteDescriptor d_36;
extern ENumberSpriteDescriptor d_38;
extern ENumberRenderDescriptor d_39;
extern ENumberRenderDescriptor d_40;
extern ENumberSpriteDescriptor d_41;
extern EndouQuadDescriptor d_44;
extern EndouELineEntry line_23[4];
extern ENumberMainData num_29[1];
extern ENumberMainData num_30[1];
extern ENumberMainData num_31[1];
extern ENumberMainData num_32[1];
extern ERibbonMainData rbn_21[4];
extern ESprite spr_22[4];
extern EndouTagFontViews tgf_24[2];

void eNumberTest(void) {
    EndouPrintLinePoint line[3];
    XglClock sp60;
    short sp70[2];
    StudioCamera *camera;
    StudioCamera *studio_camera;
    int primary_order;
    int number_order;
    int sprite_order;
    StudioCamera *second_camera;
    u8 sprite_tick;
    short blink_sprite_id;
    int cursor_pressed;
    EndouPrintPoint *cursor;

    eSpriteSet(&spr_22[0], 0x200);
    eSpriteSet(&spr_22[1], 0x200);
    eSpriteSet(&spr_22[2], 0x200);
    eSpriteSet(&spr_22[3], 0x200);
    eRibbonSet(&rbn_21[0], 1);
    eRibbonSet(&rbn_21[1], 0);
    eRibbonSet(&rbn_21[2], 0);
    eLineSet(&line_23[0], 0);
    eLineSet(&line_23[1], 0);
    eLineSet(&line_23[2], 0);
    eLineSet(&line_23[3], 0);
    eCursolSet(&cur_25, 0);
    eTagFontSet(&tgf_24[0].control, moji_26);
    eTagFontSet(&tgf_24[1].control, moji2_27);
    cursor = &d_43;
    xglRenderClearFrame();
    xglStudioChange(0);
    xglStudioGetCamera(&camera, 0);
    PadData.buttons.input.pressed = 0;
    studio_camera = camera;
    studio_camera->rotation.x = 0.0f;
    studio_camera->rotation.y = 0.0f;
    studio_camera->rotation.z = 0.0f;
    studio_camera->position.x = 0.0f;
    studio_camera->position.y = 0.0f;
    studio_camera->position.z = 1.0f;
    studio_camera->active = 1;
    studio_camera->state = 1;
    second_camera = xglStudioGetCamera2(0);
    second_camera->position.y = 1.0f;
    second_camera->position.z = 4.0f;
    second_camera->rotation.x = 0.0f;
    second_camera->rotation.y = 0.0f;
    second_camera->rotation.z = 0.0f;
    second_camera->position.x = 0.0f;
    second_camera->active = 1;
    second_camera->state = 1;
    WindowTexLoad(NULL, 1U);
    if ((PadData.buttons.button_state & ENDOU_EXIT_CHORD_U) != ENDOU_EXIT_CHORD_U) {
        primary_order = 0xFFFFFF;
        number_order = 0xFFFFFE;
        sprite_order = 0x02FFFFF0;
        do {
            xglFontDebugPrintf(0, 0, eNumberTestLabel);
            d_28.uv = 0xE00;
            d_28.vertices[0].x = 0x100;
            d_28.vertices[0].y = 0x40;
            d_28.vertices[0].depth = primary_order;
            d_28.coordinate_mode = 0;
            d_28.flags = 1;
            endPrintExtFunc(0, END_PRINT_CMD_SPRITE_QUAD, &d_28);
            line[0].x = 0x70F8;
            line[0].y = 0x84B8;
            line[1].x = 0x8F38;
            line[1].y = 0x7BF8;
            line[0].z = primary_order;
            line[0].color[1] = 0x40;
            line[0].color[0] = 0x40;
            line[0].color[3] = 0x80;
            line[0].color[2] = 0x80;
            line[1].z = primary_order;
            line[1].color[0] = 0x80;
            line[1].color[3] = 0;
            line[1].color[2] = 0;
            line[1].color[1] = 0;
            line[2].z = 0;
            endPrintDirectLine(line);
            eNumberSet(num_29, 0);
            num_29[0].x = 0x10;
            num_29[0].y = 0x80;
            num_29[0].digits = 5;
            num_29[0].work = number_order;
            num_29[0].value = -1;
            num_29[0].format = 0;
            eNumberMain(num_29);
            xglClockRead(&sp60);
            eNumberSet(&num_30[0], 0);
            num_30[0].x = 0x10;
            num_30[0].y = 0xA0;
            __builtin_memcpy(&num_30[0].value, &sp60,
                             sizeof(num_30[0].value));
            num_30[0].work = number_order;
            num_30[0].format = 2;
            eNumberMain(num_30);
            eNumberSet(&num_31[0], 0);
            num_31[0].x = 0x40;
            num_31[0].y = 0xC0;
            num_31[0].work = number_order;
            num_31[0].format = 3;
            sp70[1] = 0xC8;
            sp70[0] = 0;
            num_31[0].value = *(int *)sp70;
            num_31[0].digits = 3;
            num_31[0].colorModeFirst = 1;
            num_31[0].colorModeSecond = 2;
            num_31[0].alignment = 0;
            num_31[0].firstOnly = 0;
            eNumberMain(num_31);
            eNumberSet(&num_32[0], 0);
            num_32[0].x = 0x40;
            num_32[0].y = 0xE0;
            num_32[0].format = 7;
            num_32[0].digits = 4;
            num_32[0].value = 0x4D2;
            num_32[0].work = number_order;
            eNumberMain(num_32);
            d_33.spriteId = 1024;
            endSpriteSet(&d_33, 0xFF);
            d_33.x = 0x64;
            d_33.y = 0x60;
            d_33.work = sprite_order;
            endPrintExtFunc(d_33.work, END_PRINT_CMD_SPRITE, &d_33);
            endPrintExtFunc(0xFFFFFF, END_PRINT_CMD_LINE, line_34);
            d_35[0].spriteId = 0x31C;
            d_35[1].spriteId = 0x31F;
            endSpriteSet(&d_35[0], 0xFF);
            endSpriteSet(&d_35[1], 0xFF);
            d_35[0].x = 0xC8;
            d_35[0].y = 0x60;
            d_35[1].x = d_35[0].x + d_35[0].width - d_35[1].width;
            d_35[1].y = d_35[0].y + d_35[0].height - d_35[1].height;
            d_35[1].work = 0x02FFFFF2;
            d_35[0].work = sprite_order;
            d_35[1].color[0] = 0xFF;
            d_35[1].color[1] = 0x20;
            d_35[1].color[2] = d_35[1].color[1];
            endPrintExtFunc(d_35[0].work, END_PRINT_CMD_SPRITE, &d_35[0]);
            endPrintExtFunc(d_35[1].work, END_PRINT_CMD_SPRITE, &d_35[1]);
            sprite_tick = time_37++;
            blink_sprite_id = ((sprite_tick >> 4) & 1) + 0x10D;
            d_36.spriteId = blink_sprite_id;
            endSpriteSet(&d_36, 0xFF);
            d_36.x = 0x108;
            d_36.y = 0x60;
            d_36.work = sprite_order;
            endPrintExtFunc(d_36.work, END_PRINT_CMD_SPRITE, &d_36);
            d_38.spriteId = 0x609;
            endSpriteSet(&d_38, 0xFF);
            d_38.x = 0x108;
            d_38.y = 0x80;
            d_38.work = sprite_order;
            endPrintExtFunc(d_38.work, END_PRINT_CMD_SPRITE, &d_38);
            rbn_21[0].work = 0xFFFFF0;
            rbn_21[0].x = 0x40;
            rbn_21[0].y = 0x80;
            rbn_21[0].width = 0x100;
            rbn_21[0].height = 0x20;
            eRibbonMain(&rbn_21[0]);
            d_39.payload.number.value = 0x7B;
            d_39.argument1 = 3;
            d_39.x = 0xA;
            d_39.y = 0x40;
            d_39.work = primary_order;
            d_39.payload.number.alpha = -0x80;
            d_39.argument2 = 0;
            d_39.payload.number.blue = -0x80;
            d_39.payload.number.green = -0x80;
            d_39.payload.number.red = -0x80;
            endPrintExtFunc(d_39.work, END_PRINT_CMD_NUMBER, &d_39);
            {
            short c5 = 5, c1 = 1, c80 = 0x50;
            d_40.x = 0xA;
            d_40.work = primary_order;
            d_40.argument1 = c5;
            d_40.y = c80;
            d_40.argument2 = c1;
            d_40.payload.number.alpha = -0x80;
            d_40.payload.number.value = 0;
            d_40.payload.number.blue = -0x80;
            d_40.payload.number.green = -0x80;
            d_40.payload.number.red = -0x80;
            endPrintExtFunc(d_40.work, END_PRINT_CMD_NUMBER, &d_40);
            }
            d_41.spriteId = 258;
            d_41.x = 0xA;
            d_41.y = 0x60;
            d_41.work = primary_order;
            endPrintExtFunc(d_41.work, END_PRINT_CMD_SPRITE, &d_41);
            cursor_pressed = PadData.buttons.input.pressed & PAD_CIRCLE;
            cur_25.x = 0x10;
            cur_25.y = 0xC8;
            cur_25.work = 0xFFFFF4;
            if (cursor_pressed) {
                cur_25.state = E_MODE_SHOW_START;
            }
            eCursolMain(&cur_25);
            endPrintExtFunc(0x01000000, END_PRINT_CMD_RIBBON, d_42);
            if ((PadData.buttons.input.held & PAD_UP) != 0) {
                int cursor_y = cursor->y;

                cursor->y = cursor_y - 2;
            }
            if ((PadData.buttons.input.held & PAD_DOWN) != 0) {
                int cursor_y = cursor->y;

                cursor->y = cursor_y + 2;
            }
            endPrintExtFunc(cursor->depth, END_PRINT_CMD_POINT, cursor);
            if (PadData.buttons.input.pressed & PAD_CIRCLE) {
                spr_22[0].state = 0x22;
                spr_22[1].state = 0x22;
                spr_22[2].state = 0x22;
                spr_22[3].state = 0x22;
            }
            spr_22[0].x = 0x64;
            spr_22[0].y = 0xC8;
            spr_22[0].work = primary_order;
            eSpriteMain((ESpriteMainData *)&spr_22[0]);
            spr_22[1].y = 0xC8;
            spr_22[1].x = 0xA4;
            spr_22[1].work = primary_order;
            eSpriteMain((ESpriteMainData *)&spr_22[1]);
            spr_22[2].x = 0xC8;
            spr_22[2].y = spr_22[2].x;
            spr_22[2].work = primary_order;
            eSpriteMain((ESpriteMainData *)&spr_22[2]);
            spr_22[3].x = 0x190;
            spr_22[3].y = spr_22[3].x;
            spr_22[3].work = primary_order;
            eSpriteMain((ESpriteMainData *)&spr_22[3]);
            tgf_24[0].draw.x = 0x40;
            tgf_24[0].draw.y = 0x12C;
            tgf_24[0].draw.work = primary_order;
            eTagFontMain(&tgf_24[0].draw);
            tgf_24[1].draw.y = 0x140;
            tgf_24[1].draw.x = 0x40;
            tgf_24[1].draw.work = primary_order;
            eTagFontMain(&tgf_24[1].draw);
            d_44.points[0].depth = 0x01FFFFF0;
            d_44.points[1].depth = 0x01FFFFF0;
            d_44.points[2].depth = 0x01FFFFF0;
            d_44.points[3].depth = 0x01FFFFF0;
            d_44.points[0].y = 0x32;
            d_44.points[1].y = 0x64;
            d_44.points[2].y = 0x14;
            d_44.points[3].y = 0x82;
            d_44.colors[3].red = -0x80;
            d_44.colors[2].red = -0x80;
            d_44.colors[1].red = -0x80;
            d_44.colors[0].red = -0x80;
            d_44.colors[3].green = -0x80;
            d_44.colors[2].green = -0x80;
            d_44.colors[1].green = -0x80;
            d_44.colors[0].green = -0x80;
            d_44.colors[3].blue = -0x80;
            d_44.colors[2].blue = -0x80;
            d_44.colors[1].blue = -0x80;
            d_44.colors[0].blue = -0x80;
            d_44.colors[0].alpha = -0x80;
            d_44.colors[1].alpha = -0x80;
            d_44.colors[2].alpha = -0x80;
            d_44.colors[3].alpha = -0x80;
            d_44.points[0].x = 0;
            d_44.points[1].x = 0;
            d_44.points[2].x = 0xA0;
            d_44.points[3].x = 0xA0;
            d_44.format = 0x1A11;
            line_23[1].count = 4;
            line_23[1].points[0].x = 0xB4;
            line_23[1].points[0].y = 0x46;
            line_23[1].points[0].work = 0x7FFFFFFF;
            line_23[1].points[1].x = 0xB4;
            line_23[1].points[1].y = 0x78;
            line_23[1].points[1].work = 0x7FFFFFFF;
            line_23[1].points[2].x = 0x96;
            line_23[1].points[2].y = 0x96;
            line_23[1].points[2].work = 0x7FFFFFFF;
            line_23[1].points[3].x = 0x64;
            line_23[1].points[3].y = 0x96;
            line_23[1].points[3].work = 0x7FFFFFFF;
            line_23[1].points[0].red = -1;
            line_23[1].points[0].green = 0;
            line_23[1].points[0].blue = 0;
            line_23[1].points[0].alpha = -0x80;
            line_23[1].points[1].red = 0;
            line_23[1].points[1].green = 0;
            line_23[1].points[1].blue = -1;
            line_23[1].points[1].alpha = -0x80;
            line_23[1].points[2].red = 0;
            line_23[1].points[2].green = -1;
            line_23[1].points[2].blue = 0;
            line_23[1].points[2].alpha = -0x80;
            line_23[1].points[3].red = -1;
            line_23[1].points[3].green = -1;
            line_23[1].points[3].blue = -1;
            line_23[1].points[3].alpha = -0x80;
            if (PadData.buttons.input.pressed & PAD_START) {
                line_23[1].state = ELINE_STATE_DRAW_ALL;
            }
            if (PadData.buttons.input.pressed & PAD_SELECT) {
                line_23[1].state = ELINE_STATE_DRAW_SEQUENTIAL;
            }
            eLineMain(&line_23[1]);
            line_23[2].count = 4;
            line_23[2].points[0].x = 0x64;
            line_23[2].points[0].y = 0x64;
            line_23[2].points[0].work = 0x7FFFFFFF;
            line_23[2].points[1].y = 0x46;
            line_23[2].points[1].x = 0x82;
            line_23[2].points[1].work = 0x7FFFFFFF;
            line_23[2].points[2].x = 0x82;
            line_23[2].points[2].y = 0x78;
            line_23[2].points[2].work = 0x7FFFFFFF;
            line_23[2].points[3].y = 0x78;
            line_23[2].points[3].x = 0xB4;
            line_23[2].points[3].work = 0x7FFFFFFF;
            line_23[2].points[0].red = -1;
            line_23[2].points[0].green = 0;
            line_23[2].points[0].blue = 0;
            line_23[2].points[0].alpha = -0x80;
            line_23[2].points[1].red = 0;
            line_23[2].points[1].green = 0;
            line_23[2].points[1].blue = -1;
            line_23[2].points[1].alpha = -0x80;
            line_23[2].points[2].red = 0;
            line_23[2].points[2].green = -1;
            line_23[2].points[2].blue = 0;
            line_23[2].points[2].alpha = -0x80;
            line_23[2].points[3].red = -1;
            line_23[2].points[3].green = -1;
            line_23[2].points[3].blue = -1;
            line_23[2].points[3].alpha = -0x80;
            if (ENDOU_MODEL_PAD->pressed & PAD_START) {
                line_23[2].state = ELINE_STATE_DRAW_ALL;
            }
            if (ENDOU_MODEL_PAD->pressed & PAD_SELECT) {
                line_23[2].state = ELINE_STATE_DRAW_SEQUENTIAL;
            }
            eLineMain(&line_23[2]);
            line_23[3].count = 4;
            line_23[3].points[0].x = 0x82;
            line_23[3].points[0].y = 0x78;
            line_23[3].points[0].work = 0x7FFFFFFF;
            line_23[3].points[1].x = 0x64;
            line_23[3].points[1].y = 0x96;
            line_23[3].points[1].work = 0x7FFFFFFF;
            line_23[3].points[2].x = 0x64;
            line_23[3].points[2].y = 0x64;
            line_23[3].points[2].work = 0x7FFFFFFF;
            line_23[3].points[3].x = 0x96;
            line_23[3].points[3].y = 0x64;
            line_23[3].points[3].work = 0x7FFFFFFF;
            line_23[3].points[0].red = -1;
            line_23[3].points[0].green = 0;
            line_23[3].points[0].blue = 0;
            line_23[3].points[0].alpha = -0x80;
            line_23[3].points[1].red = 0;
            line_23[3].points[1].green = 0;
            line_23[3].points[1].blue = -1;
            line_23[3].points[1].alpha = -0x80;
            line_23[3].points[2].red = 0;
            line_23[3].points[2].green = -1;
            line_23[3].points[2].blue = 0;
            line_23[3].points[2].alpha = -0x80;
            line_23[3].points[3].red = -1;
            line_23[3].points[3].green = -1;
            line_23[3].points[3].blue = -1;
            line_23[3].points[3].alpha = -0x80;
            if (ENDOU_MODEL_PAD->pressed & PAD_START) {
                line_23[3].state = ELINE_STATE_DRAW_ALL;
            }
            if (ENDOU_MODEL_PAD->pressed & PAD_SELECT) {
                line_23[3].state = ELINE_STATE_DRAW_SEQUENTIAL;
            }
            eLineMain(&line_23[3]);
            endPrintExtFunc(0xFFFFFF, END_PRINT_CMD_EXT_BASE + END_PRINT_EXT_FLUSH, NULL);
            xglSleep();
        } while ((PadData.buttons.button_state & ENDOU_EXIT_CHORD_U) != ENDOU_EXIT_CHORD_U);
    }
}

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

/* EUC-JP bytes for the model-test diagnostic with its leading control byte. */

const unsigned char D_004C2AE8[16] =
    "\x0B\xA5\xE2\xA5\xC7\xA5\xEB\xA5\xC6\xA5\xB9\xA5\xC8";

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
            xglFontDebugPrintf(0, 0, "\013\245\341\245\303\245\273\241\274\245\270\245\306\245\271\245\310");
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
    size = xglCdReadFile("newd\\endou\\menu01.xtx", data, 0, 0);
    if (size < 0) {
        return;
    }
    WorkEnd = data + size;
    data2 = (u8 *)(((int)WorkEnd + 0xF) & ~0xF);
    size = xglCdReadFile("data\\endou\\menu01.xtx", data2, 0, 0);
    if (size < 0) {
        return;
    }
    WorkEnd = data2 + size;
    xglRenderClearDepth();
    for (;;) {
        BgTest();
        xglFontDebugPrintf(0, 0, "\013\245\306\245\257\245\271\245\301\245\343\241\274\245\306\245\271\245\310");
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
        xglFontDebugPrintf(0, 0, "\013\243\325\241\245\243\315\241\245\243\316\241\245\243\315\243\341\243\351\243\354\243\304\243\351\243\363\243\360");
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
        xglCdReadFile("data\\endou\\floor.xtx", test_data, 0, 0);

    while ((PadData.buttons.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, "\013\245\352\245\355\241\274\245\311\245\306\245\271\245\310");
        xglFontPrint(0x40, 0x28, 0x00ffffff, "\264\301\273\372\244\316\245\306\245\271\245\310\243\260\243\261\243\262\243\263\243\301\243\302\243\303\243\304");
        xglFontPrintExtFunc(0x01ffffffU, e_test, 0);
        xglFontPrint(0x40, 0x48, 0x02ffffff, "\264\301\273\372\244\316\245\306\245\271\245\310\243\260\243\261\243\262\243\263\243\301\243\302\243\303\243\304");
        xglSleep();
    }
}

static void endCallback(int result, int amount)
{
    static float size_68 __asm__("size.68");
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
extern char mm_73[];

void UmlTest(void)
{
    static char *buffer_72 __asm__("buffer.72");
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

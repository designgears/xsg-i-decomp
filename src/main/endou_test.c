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



extern void sceVif1PkCloseDirectHLCode(XglPacket *packet);

extern void sceVif1PkCnt(XglPacket *packet, int count);

extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);

extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int count);

extern void *test_data;








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

            endouShopHeading(0x40, "\013I ", menu_selection);
            endouShopHeading(0x50, "\013CW", menu_selection);
            selected_page = page - 1;
            endouShopHeading(0x60, "\013CA", menu_selection);
            endouShopHeading(0x70, "\013CB", menu_selection);
            endouShopHeading(0x80, "\013AW", menu_selection);
            endouShopHeading(0x90, "\013AA", menu_selection);
            endouShopHeading(0xa0, "\013AB", menu_selection);
            endouShopHeading(0xb0, "\013AF", menu_selection);
            endouShopHeading(0xc0, "\013AE", menu_selection);
            endouShopHeading(0xd0, "\013A ", menu_selection);

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
} __attribute__((aligned(16))) EndouTestLighting;
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
#define modelTestFpkName "newd\\yamamoto\\fpk\\ag02.fpk"
#define modelTestMotionName "newd\\motion\\chaos_cf.fpk"
#define modelTestLexExtension ".lex"
#define modelTestXtxExtension ".xtx"
#define modelTestJntExtension ".jnt"
#define GAME_RESOURCE_BASE_ADDRESS 0x07000000
#define GAME_RESOURCE_BUFFER_SIZE 0x06000000
static inline void endModelCameraPosition(StudioCamera *camera, float x, float y, float z)
{
    camera->position.x = x;
    camera->position.y = y;
    camera->position.z = z;
}

void ModelTest(void) {
    struct StudioLight *light;
    u8 **work_end = &WorkEnd;
    static float ang[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    static float pos[4] = {0.0f, 0.0f, 4.0f, 1.0f};
    int motion;

    motion = 1;
    xglStudioChange(0);
    {
    EndouTestLighting lighting = {
        {0.3f, 0.3f, 0.3f, 1.0f},
        {{0.6f, 0.6f, 0.6f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
        {{0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
    };
    xglStudioGetLight(&light);
    xglLightIntensityAmbient(light, &lighting.ambient);
    xglLightIntensityParallel(light, 0, &lighting.parallel_intensity[0]);
    xglLightDirection(light, 0, &lighting.direction[0]);
    xglLightIntensityParallel(light, 1, &lighting.parallel_intensity[1]);
    xglLightDirection(light, 1, &lighting.direction[1]);
    xglLightIntensityParallel(light, 2, &lighting.parallel_intensity[2]);
    xglLightDirection(light, 2, &lighting.direction[2]);
    }
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
    /* Unused second lighting set: its constant (0x004C2680) and its 0x70-byte
     * frame slot are in the original, but no code reads them. */
    if (0) {
    EndouTestLighting dim_lighting = {
        {0.2f, 0.2f, 0.2f, 1.0f},
        {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
        {{0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
    };
    xglStudioGetLight(&light);
    xglLightIntensityAmbient(light, &dim_lighting.ambient);
    xglLightIntensityParallel(light, 0, &dim_lighting.parallel_intensity[0]);
    xglLightDirection(light, 0, &dim_lighting.direction[0]);
    xglLightIntensityParallel(light, 1, &dim_lighting.parallel_intensity[1]);
    xglLightDirection(light, 1, &dim_lighting.direction[1]);
    xglLightIntensityParallel(light, 2, &dim_lighting.parallel_intensity[2]);
    xglLightDirection(light, 2, &dim_lighting.direction[2]);
    }
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
            static float pipo = 0.0f;
            static EndouModelLinePoint line00[7];
            static EndouModelLinePoint line01[7];
            static EndouModelLinePoint line02[7];
            static EndouModelLinePoint line03[7];
            static EndouModelLinePoint line10[7];
            static EndouModelLinePoint line11[7];
            static EndouModelLinePoint line12[7];
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
                ang[1] += 0.04f;
            }
            if (ENDOU_MODEL_TEST_PAD->stick_axis[2] >= ENDOU_STICK_DEADZONE + 1) {
                ang[1] -= 0.04f;
            }
            if (ENDOU_MODEL_TEST_PAD->stick_axis[3] < -ENDOU_STICK_DEADZONE) {
                ang[0] += 0.04f;
            }
            if (ENDOU_MODEL_TEST_PAD->stick_axis[3] >= ENDOU_STICK_DEADZONE + 1) {
                ang[0] -= 0.04f;
            }
            xglMatrixStackUnit();
            xglMatrixStackRotZ(ang[2]);
            xglMatrixStackRotY(ang[1]);
            xglMatrixStackRotX(ang[0]);
            xglMatrixStackTrans(pos);
            xglMatrixStackSave(camera_matrix);
            temp_f0_21 = ang[0];
            temp_16 = ENDOU_MODEL_TEST_PAD->held;
            temp_17_4->rotation.x = temp_f0_21;
            temp_f0_22 = ang[1];
            temp_17_4->rotation.y = temp_f0_22;
            temp_f1 = ang[2];
            temp_17_4->rotation.z = temp_f1;
            temp_f2 = camera_matrix[3][0];
            temp_17_4->position.x = temp_f2;
            temp_f0_23 = camera_matrix[3][1] + 1.0f;
            temp_17_4->position.y = temp_f0_23;
            temp_f20 = camera_matrix[3][2];
            temp_17_4->position.z = temp_f20;
            if (temp_16 & PAD_R1) {
                temp_f0_24 = pipo + 0.2;
                pipo = temp_f0_24;
            }
            if (temp_16 & PAD_R2) {
                temp_f0_26 = pipo - 0.2;
                pipo = temp_f0_26;
            }
            temp_f0_25 = pipo;
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
            {
            unsigned char column_alphas[ENDOU_GRID_ROWS] = {0x00, 0x60, 0x80, 0x60, 0x00};
            unsigned char row_alphas[ENDOU_GRID_COLUMNS] = {0x00, 0x60, 0x80, 0x80, 0x60, 0x00};
            static int time = 0;
            static EndouModelMessageWindow msg;
            static EndouModelBackQuad bsp;
            static int no = 0;
            int *message_time = &time;
            xglMatrixStackUnit();
            xglMatrixStackSave(projection_matrix);
            xglRotTransPersN(screen_points, &projection_matrix, world_points, ENDOU_GRID_POINT_COUNT, 0);
            {
                for (point_index = 0; point_index < ENDOU_GRID_ROWS; point_index++) {
                    unsigned char *color;
                    line00[point_index].vertex = screen_points[point_index][1];
                    color = line00[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = column_alphas[point_index];
                    line00[point_index].vertex.terminator = 1;
                }
                line00[5].vertex.terminator = 0;

                {
                for (point_index = 0; point_index < ENDOU_GRID_ROWS; point_index++) {
                    unsigned char *color;
                    line01[point_index].vertex = screen_points[point_index][2];
                    color = line01[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = column_alphas[point_index];
                    line01[point_index].vertex.terminator = 1;
                }
                (line01 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < ENDOU_GRID_ROWS; point_index++) {
                    unsigned char *color;
                    line02[point_index].vertex = screen_points[point_index][3];
                    color = line02[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = column_alphas[point_index];
                    line02[point_index].vertex.terminator = 1;
                }
                (line02 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < ENDOU_GRID_ROWS; point_index++) {
                    unsigned char *color;
                    line03[point_index].vertex = screen_points[point_index][4];
                    color = line03[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = column_alphas[point_index];
                    line03[point_index].vertex.terminator = 1;
                }
                (line03 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < ENDOU_GRID_COLUMNS; point_index++) {
                    unsigned char *color;
                    line10[point_index].vertex = screen_points[1][point_index];
                    color = line10[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = row_alphas[point_index];
                    line10[point_index].vertex.terminator = 1;
                }
                (line10 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < ENDOU_GRID_COLUMNS; point_index++) {
                    unsigned char *color;
                    line11[point_index].vertex = screen_points[2][point_index];
                    color = line11[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = row_alphas[point_index];
                    line11[point_index].vertex.terminator = 1;
                }
                (line11 + point_index)->vertex.terminator = 0;
                }

                {
                for (point_index = 0; point_index < ENDOU_GRID_COLUMNS; point_index++) {
                    unsigned char *color;
                    line12[point_index].vertex = screen_points[3][point_index];
                    color = line12[point_index].color;
                    color[0] = -0x80;
                    color[2] = -0x10;
                    color[1] = -0x10;
                    color[3] = row_alphas[point_index];
                    line12[point_index].vertex.terminator = 1;
                }
                (line12 + point_index)->vertex.terminator = 0;
                }
            }
            endPrintExtFunc(0xFFFFF0, END_PRINT_CMD_LINE_RAW, line00);
            endPrintExtFunc(0xFFFFF1, END_PRINT_CMD_LINE_RAW, line01);
            endPrintExtFunc(0xFFFFF2, END_PRINT_CMD_LINE_RAW, line02);
            endPrintExtFunc(0xFFFFF3, END_PRINT_CMD_LINE_RAW, line03);
            endPrintExtFunc(0xFFFFF4, END_PRINT_CMD_LINE_RAW, line10);
            endPrintExtFunc(0xFFFFF5, END_PRINT_CMD_LINE_RAW, line11);
            endPrintExtFunc(0xFFFFF6, END_PRINT_CMD_LINE_RAW, line12);
            {
            EndouDwordVector4 quad_words[4] = {
                {-0.5f, 0.1f, 0.0f, 1.0f},
                {-0.5f, -0.1f, 0.0f, 1.0f},
                {0.5f, 0.1f, 0.0f, 1.0f},
                {0.5f, -0.1f, 0.0f, 1.0f},
            };
            static const char *msg00[4] = {
                "\245\342\245\307\245\353\245\306\245\271\245\310",
                "\244\342\244\307\244\353\245\306\245\271\245\310",
                "\245\342\245\307\245\353\244\306\244\271\244\310",
                "\244\342\244\307\244\353\244\306\244\271\244\310"
            };
            endPrintExtFunc(0, 0x6A, NULL);
            endPrintExtFunc(0, 0x6C, NULL);
            if (*message_time == 0) {
                eMessageSet(&msg.message, msg00[no]);
                msg.message.mode = EMESSAGE_MODE_LINE_START;
                *message_time = ENDOU_MODEL_MESSAGE_FRAMES;
                no = (no + 1) & 3;
            }
            (*message_time)--;
            msg.message.x = 0;
            msg.message.draw_parameter = 0xFFFFF0;
            msg.message.y = 0;
            eMessageMain(&msg.message);
            endPrintExtFunc(0, 0x6B, NULL);
            bsp.uv[0] = 0.0f;
            bsp.uv[2] = 0.234375f;
            bsp.uv[3] = 0.046875f;
            bsp.uv[1] = 0.0f;
            xglRotTransPersN(bsp.vertices, NULL, quad_words, 4, 0);
            endPrintExtFunc(0, 0x6D, &bsp);
            xglSleep();
            }
            }
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

typedef struct EndouBattleWindowItem {
    const char *name;
    const char *help;
    int flags;
} EndouBattleWindowItem;

void BattleWindowTest(void)
{
    static EndouBattleWindowItem data[20] = {
        {"\245\325\245\247\245\326\245\351\245\352\241\274\243\323", "\245\300\241\274\245\310\262\246\267\350\304\352\300\357", 0x201},
        {"\271\342\276\276\265\334\265\255\307\260", "\275\325\244\316\303\273\265\367\316\245\262\246\267\350\304\352\300\357", 0x301},
        {"\272\371\262\326\276\336", "\314\306\307\317\245\257\245\351\245\267\245\303\245\257\302\350\260\354\303\306", 0x401},
        {"\273\251\267\356\276\336", "\262\264\307\317\245\257\245\351\245\267\245\303\245\257\302\350\260\354\303\306", 0x401},
        {"\305\267\271\304\276\336\241\312\275\325\241\313", "\275\325\244\316\270\305\307\317\304\271\265\367\316\245\262\246\267\350\304\352\300\357", 0x401},
        {"\243\316\243\310\243\313\245\336\245\244\245\353\243\303", "\263\260\271\361\273\272\307\317\243\263\272\320\245\336\245\244\245\353\262\246\267\350\304\352\300\357", 0x501},
        {"\245\252\241\274\245\257\245\271", "\314\306\307\317\245\257\245\351\245\267\245\303\245\257\302\350\306\363\303\306\241\241\314\306\307\317\315\245\275\331", 0x500},
        {"\306\374\313\334\245\300\241\274\245\323\241\274", "\262\264\307\317\245\257\245\351\245\267\245\303\245\257\302\350\306\363\303\306\241\241\305\354\265\376\315\245\275\331", 0x500},
        {"\260\302\305\304\265\255\307\260", "\275\325\244\316\245\336\245\244\245\353\262\246\267\350\304\352\300\357", 0x500},
        {"\312\365\304\315\265\255\307\260", "\275\325\244\316\245\260\245\351\245\363\245\327\245\352", 0x600},
        {"\245\271\245\327\245\352\245\363\245\277\241\274\245\272\243\323", "\275\251\244\316\303\273\265\367\316\245\262\246\267\350\304\352\300\357", 0xA00},
        {"\275\251\262\332\276\336", "\314\306\307\317\245\257\245\351\245\267\245\303\245\257\302\350\273\260\303\306", 0xA00},
        {"\265\306\262\326\276\336", "\262\264\307\317\245\257\245\351\245\267\245\303\245\257\302\350\273\260\303\306", 0xA00},
        {"\305\267\271\304\276\336\241\312\275\251\241\313", "\270\305\307\317\262\246\306\273\241\241\313\342\244\316\305\267\271\304\276\336", 0xA00},
        {"\245\250\245\352\245\266\245\331\245\271\275\367\262\246\307\325", "\314\306\307\317\275\367\262\246\267\350\304\352\300\357", 0xB00},
        {"\245\336\245\244\245\353\243\303\243\323", "\275\251\244\316\245\336\245\244\245\353\262\246\267\350\304\352\300\357", 0xB00},
        {"\245\270\245\343\245\321\245\363\245\253\245\303\245\327", "\270\305\307\317\262\246\306\273\241\241\300\244\263\246\244\316\312\311", 0xB00},
        {"\272\345\277\300\243\312\243\306", "\243\262\272\320\314\306\307\317\245\301\245\343\245\363\245\324\245\252\245\363", 0xB00},
        {"\315\255\307\317\265\255\307\260", "\245\260\245\351\245\363\245\327\245\352", 0xB00},
        {NULL, NULL, 0x0},
    };
    static char *tag[] = {"G One"};
    static char text[] = "\244\242\037\244\242\244\242\244\242\012\244\244\241\241\244\244\241\241\036\001\245\334\245\277\245\363\241\241\241\241\244\244\244\244\244\244\241\241\241\241\241\241\244\244\244\244\244\244\244\244\244\244\012\014\200\000\000\300\326\300\326\300\326\300\326\300\326\300\326\300\326\300\326\300\326\300\326\300\326\014\000\000\200\300\304\300\304\300\304\300\304\300\304\300\304\300\304\300\304\300\304\300\304\300\304\037\014\200\200\200\244\246\012\244\250\244\250\244\250\244\250\244\250\244\250\037\244\252\244\252\244\252\244\252\244\252\244\252\244\252\244\252\244\252\244\252\244\252\244\252\244\252\244\252\244\252\244\252\244\252\244\252\244\252\244\252\241\252\241\252";
    EndouBwArg0 s0;
    EndouBwArg4 s30;
    EndouBwArg3 s50;
    EndouBwArg2 s90;

    s0.a = 0x10;
    s0.c = 0xFFFFFD;
    *(int *)&s0.g = (int)tag[0];
    s0.f = data;
    s0.b = 0x90;
    *(char **)&s0.e = (char *)0xA0;
    s0.d = 0;
    xglRenderClearFrame();
    xglSleep();
    xglCdLoadOverlay(1);
    eBattleWinInit();
    if ((PadData.buttons.button_state & 0x08000100) != 0x08000100) {
        do {
            xglFontDebugPrintf(0, 0, "\013\245\320\245\310\245\353\245\246\245\243\245\363\245\311\241\274\245\306\245\271\245\310");
            if (PadData.buttons.input.pressed & 0x10) {
                s30.a = 0x10;
                s30.c = 0xFFFFFD;
                s30.b = 0x20;
                s30.d = "Test";
                s30.e = text;
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
                static int count = 0;
                static char *msg[] = {"\241\241\245\271\245\355\245\246\012", "\315\266\317\307\012\315\266\317\307\315\266\317\307"};
                static char tag[] = "BattleWindowTest";

                s90.d = 0x800000B4;
                s90.a = 0x10;
                s90.b = 0x20;
                s90.e = msg[count];
                s90.c = 0xFFFFF0;
                s90.f = tag;
                eBattleWinOpen2(&s90);
                count ^= 1;
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
            xglFontDebugPrintf(0, 0x20, "%d", s0.h);
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
} __attribute__((aligned(16))) EndouSpriteQuad;
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

/* d.43: the original takes its address before the loop (the pointer is
 * rematerialized at each use), which a block-scope static cannot express. */
static EndouPrintPoint cursor_point __asm__("d.43");

void eNumberTest(void) {
    static ERibbonMainData rbn[4];
    static ESprite spr[4];
    static EndouELineEntry line[4];
    static EndouTagFontViews tgf[4];
    static ECursolMainData cur[2];
    static char moji[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static char moji2[] = "abcdefghijklmnopqrstuvwxyz";
    EndouPrintLinePoint direct_line[3];
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
    /* Unreferenced in the original's code; only the literal remains (0x004C2AC0). */
    const char *bustup_texture = "data\\endou\\bustup.xtx";

    eSpriteSet(&spr[0], 0x200);
    eSpriteSet(&spr[1], 0x200);
    eSpriteSet(&spr[2], 0x200);
    eSpriteSet(&spr[3], 0x200);
    eRibbonSet(&rbn[0], 1);
    eRibbonSet(&rbn[1], 0);
    eRibbonSet(&rbn[2], 0);
    eLineSet(&line[0], 0);
    eLineSet(&line[1], 0);
    eLineSet(&line[2], 0);
    eLineSet(&line[3], 0);
    eCursolSet(&cur[0], 0);
    eTagFontSet(&tgf[0].control, moji);
    eTagFontSet(&tgf[1].control, moji2);
    cursor = &cursor_point;
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
            {
            static EndouSpriteQuad d;
            d.uv = 0xE00;
            d.vertices[0].x = 0x100;
            d.vertices[0].y = 0x40;
            d.vertices[0].depth = primary_order;
            d.coordinate_mode = 0;
            d.flags = 1;
            endPrintExtFunc(0, END_PRINT_CMD_SPRITE_QUAD, &d);
            }
            direct_line[0].x = 0x70F8;
            direct_line[0].y = 0x84B8;
            direct_line[1].x = 0x8F38;
            direct_line[1].y = 0x7BF8;
            direct_line[0].z = primary_order;
            direct_line[0].color[1] = 0x40;
            direct_line[0].color[0] = 0x40;
            direct_line[0].color[3] = 0x80;
            direct_line[0].color[2] = 0x80;
            direct_line[1].z = primary_order;
            direct_line[1].color[0] = 0x80;
            direct_line[1].color[3] = 0;
            direct_line[1].color[2] = 0;
            direct_line[1].color[1] = 0;
            direct_line[2].z = 0;
            endPrintDirectLine(direct_line);
            {
            static ENumberMainData num[1];
            eNumberSet(num, 0);
            num[0].x = 0x10;
            num[0].y = 0x80;
            num[0].digits = 5;
            num[0].work = number_order;
            num[0].value = -1;
            num[0].format = 0;
            eNumberMain(num);
            }
            xglClockRead(&sp60);
            {
            static ENumberMainData num[1];
            eNumberSet(&num[0], 0);
            num[0].x = 0x10;
            num[0].y = 0xA0;
            __builtin_memcpy(&num[0].value, &sp60,
                             sizeof(num[0].value));
            num[0].work = number_order;
            num[0].format = 2;
            eNumberMain(num);
            }
            {
            static ENumberMainData num[1];
            eNumberSet(&num[0], 0);
            num[0].x = 0x40;
            num[0].y = 0xC0;
            num[0].work = number_order;
            num[0].format = 3;
            sp70[1] = 0xC8;
            sp70[0] = 0;
            num[0].value = *(int *)sp70;
            num[0].digits = 3;
            num[0].colorModeFirst = 1;
            num[0].colorModeSecond = 2;
            num[0].alignment = 0;
            num[0].firstOnly = 0;
            eNumberMain(num);
            }
            {
            static ENumberMainData num[1];
            eNumberSet(&num[0], 0);
            num[0].x = 0x40;
            num[0].y = 0xE0;
            num[0].format = 7;
            num[0].digits = 4;
            num[0].value = 0x4D2;
            num[0].work = number_order;
            eNumberMain(num);
            }
            {
            static ENumberSpriteDescriptor d;
            static EndouPrintPoint line[6] = {
                {64, 256, 0x1FFFFF0, {0x60, 0x60, 0x80, 0x00}},
                {128, 256, 0x1FFFFF0, {0x60, 0x60, 0x80, 0x60}},
                {256, 256, 0x1FFFFF0, {0x60, 0x60, 0x80, 0x80}},
                {384, 256, 0x1FFFFF0, {0x60, 0x60, 0x80, 0x60}},
                {448, 256, 0x1FFFFF0, {0x60, 0x60, 0x80, 0x00}},
                {0, 0, 0x0, {0x00, 0x00, 0x00, 0x00}}
            };
            d.spriteId = 1024;
            endSpriteSet(&d, 0xFF);
            d.x = 0x64;
            d.y = 0x60;
            d.work = sprite_order;
            endPrintExtFunc(d.work, END_PRINT_CMD_SPRITE, &d);
            endPrintExtFunc(0xFFFFFF, END_PRINT_CMD_LINE, line);
            }
            {
            static ENumberSpriteDescriptor d[2];
            d[0].spriteId = 0x31C;
            d[1].spriteId = 0x31F;
            endSpriteSet(&d[0], 0xFF);
            endSpriteSet(&d[1], 0xFF);
            d[0].x = 0xC8;
            d[0].y = 0x60;
            d[1].x = d[0].x + d[0].width - d[1].width;
            d[1].y = d[0].y + d[0].height - d[1].height;
            d[1].work = 0x02FFFFF2;
            d[0].work = sprite_order;
            d[1].color[0] = 0xFF;
            d[1].color[1] = 0x20;
            d[1].color[2] = d[1].color[1];
            endPrintExtFunc(d[0].work, END_PRINT_CMD_SPRITE, &d[0]);
            endPrintExtFunc(d[1].work, END_PRINT_CMD_SPRITE, &d[1]);
            }
            {
            static ENumberSpriteDescriptor d;
            static u8 time = 0;
            sprite_tick = time++;
            blink_sprite_id = ((sprite_tick >> 4) & 1) + 0x10D;
            d.spriteId = blink_sprite_id;
            endSpriteSet(&d, 0xFF);
            d.x = 0x108;
            d.y = 0x60;
            d.work = sprite_order;
            endPrintExtFunc(d.work, END_PRINT_CMD_SPRITE, &d);
            }
            {
            static ENumberSpriteDescriptor d;
            d.spriteId = 0x609;
            endSpriteSet(&d, 0xFF);
            d.x = 0x108;
            d.y = 0x80;
            d.work = sprite_order;
            endPrintExtFunc(d.work, END_PRINT_CMD_SPRITE, &d);
            }
            rbn[0].work = 0xFFFFF0;
            rbn[0].x = 0x40;
            rbn[0].y = 0x80;
            rbn[0].width = 0x100;
            rbn[0].height = 0x20;
            eRibbonMain(&rbn[0]);
            {
            static ENumberRenderDescriptor d;
            d.payload.number.value = 0x7B;
            d.argument1 = 3;
            d.x = 0xA;
            d.y = 0x40;
            d.work = primary_order;
            d.payload.number.alpha = -0x80;
            d.argument2 = 0;
            d.payload.number.blue = -0x80;
            d.payload.number.green = -0x80;
            d.payload.number.red = -0x80;
            endPrintExtFunc(d.work, END_PRINT_CMD_NUMBER, &d);
            }
            {
            static ENumberRenderDescriptor d;
            short c5 = 5, c1 = 1, c80 = 0x50;
            d.x = 0xA;
            d.work = primary_order;
            d.argument1 = c5;
            d.y = c80;
            d.argument2 = c1;
            d.payload.number.alpha = -0x80;
            d.payload.number.value = 0;
            d.payload.number.blue = -0x80;
            d.payload.number.green = -0x80;
            d.payload.number.red = -0x80;
            endPrintExtFunc(d.work, END_PRINT_CMD_NUMBER, &d);
            }
            {
            static ENumberSpriteDescriptor d;
            d.spriteId = 258;
            d.x = 0xA;
            d.y = 0x60;
            d.work = primary_order;
            endPrintExtFunc(d.work, END_PRINT_CMD_SPRITE, &d);
            }
            cursor_pressed = PadData.buttons.input.pressed & PAD_CIRCLE;
            cur[0].x = 0x10;
            cur[0].y = 0xC8;
            cur[0].work = 0xFFFFF4;
            if (cursor_pressed) {
                cur[0].state = E_MODE_SHOW_START;
            }
            eCursolMain(&cur[0]);
            {
            static EndouPrintPoint d[7] = {
                {100, 300, 0xFFFFF0, {0x40, 0x40, 0x80, 0x00}},
                {100, 316, 0xFFFFF0, {0x40, 0x40, 0x80, 0x00}},
                {200, 300, 0xFFFFF0, {0x40, 0x40, 0x80, 0x70}},
                {200, 316, 0xFFFFF0, {0x40, 0x40, 0x80, 0x80}},
                {400, 300, 0xFFFFF0, {0x40, 0x40, 0x80, 0x00}},
                {400, 316, 0xFFFFF0, {0x40, 0x40, 0x80, 0x00}},
                {0, 0, 0x0, {0x00, 0x00, 0x00, 0x00}}
            };
            endPrintExtFunc(0x01000000, END_PRINT_CMD_RIBBON, d);
            }
            {
            if ((PadData.buttons.input.held & PAD_UP) != 0) {
                int cursor_y = cursor->y;

                cursor->y = cursor_y - 2;
            }
            if ((PadData.buttons.input.held & PAD_DOWN) != 0) {
                int cursor_y = cursor->y;

                cursor->y = cursor_y + 2;
            }
            endPrintExtFunc(cursor->depth, END_PRINT_CMD_POINT, cursor);
            }
            if (PadData.buttons.input.pressed & PAD_CIRCLE) {
                spr[0].state = 0x22;
                spr[1].state = 0x22;
                spr[2].state = 0x22;
                spr[3].state = 0x22;
            }
            spr[0].x = 0x64;
            spr[0].y = 0xC8;
            spr[0].work = primary_order;
            eSpriteMain((ESpriteMainData *)&spr[0]);
            spr[1].y = 0xC8;
            spr[1].x = 0xA4;
            spr[1].work = primary_order;
            eSpriteMain((ESpriteMainData *)&spr[1]);
            spr[2].x = 0xC8;
            spr[2].y = spr[2].x;
            spr[2].work = primary_order;
            eSpriteMain((ESpriteMainData *)&spr[2]);
            spr[3].x = 0x190;
            spr[3].y = spr[3].x;
            spr[3].work = primary_order;
            eSpriteMain((ESpriteMainData *)&spr[3]);
            tgf[0].draw.x = 0x40;
            tgf[0].draw.y = 0x12C;
            tgf[0].draw.work = primary_order;
            eTagFontMain(&tgf[0].draw);
            tgf[1].draw.y = 0x140;
            tgf[1].draw.x = 0x40;
            tgf[1].draw.work = primary_order;
            eTagFontMain(&tgf[1].draw);
            {
            static EndouQuadDescriptor d __asm__("d.44");
            d.points[0].depth = 0x01FFFFF0;
            d.points[1].depth = 0x01FFFFF0;
            d.points[2].depth = 0x01FFFFF0;
            d.points[3].depth = 0x01FFFFF0;
            d.points[0].y = 0x32;
            d.points[1].y = 0x64;
            d.points[2].y = 0x14;
            d.points[3].y = 0x82;
            d.colors[3].red = -0x80;
            d.colors[2].red = -0x80;
            d.colors[1].red = -0x80;
            d.colors[0].red = -0x80;
            d.colors[3].green = -0x80;
            d.colors[2].green = -0x80;
            d.colors[1].green = -0x80;
            d.colors[0].green = -0x80;
            d.colors[3].blue = -0x80;
            d.colors[2].blue = -0x80;
            d.colors[1].blue = -0x80;
            d.colors[0].blue = -0x80;
            d.colors[0].alpha = -0x80;
            d.colors[1].alpha = -0x80;
            d.colors[2].alpha = -0x80;
            d.colors[3].alpha = -0x80;
            d.points[0].x = 0;
            d.points[1].x = 0;
            d.points[2].x = 0xA0;
            d.points[3].x = 0xA0;
            d.format = 0x1A11;
            }
            line[1].count = 4;
            line[1].points[0].x = 0xB4;
            line[1].points[0].y = 0x46;
            line[1].points[0].work = 0x7FFFFFFF;
            line[1].points[1].x = 0xB4;
            line[1].points[1].y = 0x78;
            line[1].points[1].work = 0x7FFFFFFF;
            line[1].points[2].x = 0x96;
            line[1].points[2].y = 0x96;
            line[1].points[2].work = 0x7FFFFFFF;
            line[1].points[3].x = 0x64;
            line[1].points[3].y = 0x96;
            line[1].points[3].work = 0x7FFFFFFF;
            line[1].points[0].red = -1;
            line[1].points[0].green = 0;
            line[1].points[0].blue = 0;
            line[1].points[0].alpha = -0x80;
            line[1].points[1].red = 0;
            line[1].points[1].green = 0;
            line[1].points[1].blue = -1;
            line[1].points[1].alpha = -0x80;
            line[1].points[2].red = 0;
            line[1].points[2].green = -1;
            line[1].points[2].blue = 0;
            line[1].points[2].alpha = -0x80;
            line[1].points[3].red = -1;
            line[1].points[3].green = -1;
            line[1].points[3].blue = -1;
            line[1].points[3].alpha = -0x80;
            if (PadData.buttons.input.pressed & PAD_START) {
                line[1].state = ELINE_STATE_DRAW_ALL;
            }
            if (PadData.buttons.input.pressed & PAD_SELECT) {
                line[1].state = ELINE_STATE_DRAW_SEQUENTIAL;
            }
            eLineMain(&line[1]);
            line[2].count = 4;
            line[2].points[0].x = 0x64;
            line[2].points[0].y = 0x64;
            line[2].points[0].work = 0x7FFFFFFF;
            line[2].points[1].y = 0x46;
            line[2].points[1].x = 0x82;
            line[2].points[1].work = 0x7FFFFFFF;
            line[2].points[2].x = 0x82;
            line[2].points[2].y = 0x78;
            line[2].points[2].work = 0x7FFFFFFF;
            line[2].points[3].y = 0x78;
            line[2].points[3].x = 0xB4;
            line[2].points[3].work = 0x7FFFFFFF;
            line[2].points[0].red = -1;
            line[2].points[0].green = 0;
            line[2].points[0].blue = 0;
            line[2].points[0].alpha = -0x80;
            line[2].points[1].red = 0;
            line[2].points[1].green = 0;
            line[2].points[1].blue = -1;
            line[2].points[1].alpha = -0x80;
            line[2].points[2].red = 0;
            line[2].points[2].green = -1;
            line[2].points[2].blue = 0;
            line[2].points[2].alpha = -0x80;
            line[2].points[3].red = -1;
            line[2].points[3].green = -1;
            line[2].points[3].blue = -1;
            line[2].points[3].alpha = -0x80;
            if (ENDOU_MODEL_PAD->pressed & PAD_START) {
                line[2].state = ELINE_STATE_DRAW_ALL;
            }
            if (ENDOU_MODEL_PAD->pressed & PAD_SELECT) {
                line[2].state = ELINE_STATE_DRAW_SEQUENTIAL;
            }
            eLineMain(&line[2]);
            line[3].count = 4;
            line[3].points[0].x = 0x82;
            line[3].points[0].y = 0x78;
            line[3].points[0].work = 0x7FFFFFFF;
            line[3].points[1].x = 0x64;
            line[3].points[1].y = 0x96;
            line[3].points[1].work = 0x7FFFFFFF;
            line[3].points[2].x = 0x64;
            line[3].points[2].y = 0x64;
            line[3].points[2].work = 0x7FFFFFFF;
            line[3].points[3].x = 0x96;
            line[3].points[3].y = 0x64;
            line[3].points[3].work = 0x7FFFFFFF;
            line[3].points[0].red = -1;
            line[3].points[0].green = 0;
            line[3].points[0].blue = 0;
            line[3].points[0].alpha = -0x80;
            line[3].points[1].red = 0;
            line[3].points[1].green = 0;
            line[3].points[1].blue = -1;
            line[3].points[1].alpha = -0x80;
            line[3].points[2].red = 0;
            line[3].points[2].green = -1;
            line[3].points[2].blue = 0;
            line[3].points[2].alpha = -0x80;
            line[3].points[3].red = -1;
            line[3].points[3].green = -1;
            line[3].points[3].blue = -1;
            line[3].points[3].alpha = -0x80;
            if (ENDOU_MODEL_PAD->pressed & PAD_START) {
                line[3].state = ELINE_STATE_DRAW_ALL;
            }
            if (ENDOU_MODEL_PAD->pressed & PAD_SELECT) {
                line[3].state = ELINE_STATE_DRAW_SEQUENTIAL;
            }
            eLineMain(&line[3]);
            endPrintExtFunc(0xFFFFFF, END_PRINT_CMD_EXT_BASE + END_PRINT_EXT_FLUSH, NULL);
            xglSleep();
        } while ((PadData.buttons.button_state & ENDOU_EXIT_CHORD_U) != ENDOU_EXIT_CHORD_U);
    }
}

static EndouPrintPoint cursor_point = {256, 256, 0xFFFFFFF, {0xFF, 0xFF, 0xFF, 0x80}};

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
    static const char *msg[5] __asm__("msg.45") = {
        "\015\002\031\001\031\002\243\320\243\354\243\341\243\371\243\323\243\364\243\341\243\364\243\351\243\357\243\356\243\262\012\031\000\245\274\245\316\245\265\241\274\245\254\012\031\001\014\200\000\000\243\315\243\305\243\315\243\317\243\322\243\331\243\303\243\301\243\322\243\304\012\031\000\272\271\271\376\270\375\243\261\012\031\001\014\000\200\000\243\315\243\305\243\315\243\317\243\322\243\331\243\303\243\301\243\322\243\304\012\031\000\272\271\271\376\270\375\243\262\037\031\000\014\000\000\200\245\317\241\274\245\311\245\307\245\243\245\271\245\257",
        "\015\002\014\037\037\037\305\267\271\304\276\336\275\325\275\251\317\242\307\306\012\245\271\245\332\245\267\245\343\245\353\245\246\245\243\241\274\245\257\012\245\260\245\351\245\363\245\327\245\352\243\263\317\242\307\306\012\245\260\245\351\245\271\245\357\245\363\245\300\241\274\012\245\250\245\353\245\263\245\363\245\311\245\353\245\321\245\265\241\274\012\245\265\245\363\245\307\241\274\245\265\245\244\245\354\245\363\245\271\012\273\251\267\356\276\336\012\306\374\313\334\245\300\241\274\245\323\241\274\012\265\306\262\326\276\336",
        "\015\002\031\001\014?\000\000\244\242\012\244\244\012\244\246\012\244\250\012\244\252",
        "\015\002\012\245\271\245\332\245\267\245\343\245\353\245\246\245\243\241\274\245\257\012\014\200\000\000\245\260\245\351\245\271\245\357\245\363\245\300\241\274\012\245\250\245\353\245\263\245\363\245\311\245\353\245\321\245\265\241\274",
        NULL
    };
    static char text00[] __asm__("text00.46") = "\031\001\243\320\243\354\036:\243\341\243\371\012\243\323\243\364\243\341\243\364\243\351\243\357\243\356\243\262\037\031\000\036:\036;\245\250\036<\245\353\036=\245\263\036>\245\363\036?\245\311\036\011\245\353\036\012\245\321\036\013\245\265\036\014\241\274\037\036\001\245\271\245\332\036\033\036\034\036\035\036\036\245\267\245\343\245\353\245\246\245\243\241\274\245\257\037\036\002\245\260\245\351\245\271\245\357\245\363\245\300\241\274\037\036\037\036 \036!\036\"\245\273\245\244\245\246\245\363\245\271\245\253\245\244\037\245\255\245\363\245\260\245\330\245\244\245\355\241\274\241\274\241\274\241\274\241\274\241\274\241\274\241\274\241\274\241\274\037\244\242";
    EMessageTestWindow windows[2];
    EMessageTestWindow current;
    int message_index = 0;

    WindowTexLoad(0, 0);
    endPrintInit();
    eMessageSet(&windows[0], msg[0]);
    eMessageSet(&windows[1], msg[0]);
    windows[1].mode = 0x20;
    eMessageSet(&current, text00);
    current.page_state = 1;
    eMessageTextChange(&current, text00);

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
                windows[0].message_text = msg[message_index];
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
    static BgTestEnvironment TestEnv __asm__("TestEnv.47") = {
        {0x0, 0x0, 0x0}, 0x50000009,
        {0x8001, 0x808B4000, 0x53531EEEULL},
        {0x20ULL, 0x14ULL},
        {0x0ULL, 0x6ULL},
        {0x50000ULL, 0x47ULL},
        {0x80, 0x80, 0x80, 0x80},
        {0x0, 0x0, 0x0, 0x0},
        {0x6FF8, 0x71F8, 0x0, 0x0},
        {0x2000, 0x1C00, 0x0, 0x0},
        {0x8FF8, 0x8DF8, 0x0, 0x0},
    };
    XglPacket *packet;
    int packet_command = BG_TEST_COMMAND_BASE << BG_TEST_COMMAND_BASE_SHIFT;

    packet_command |= sRender.packetControl << BG_TEST_CONTROL_SHIFT;
    TestEnv.packet_write.value = packet_command |
        ((u64)BG_TEST_COMMAND_HIGH << BG_TEST_COMMAND_HIGH_SHIFT);
    packet = xglPacketGetCurrent();
    sceVif1PkRef(packet, &TestEnv, 10, 0, 0, 0);
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
    u64 header[4];
    u64 w20;
    u64 w28[3];
    u64 w40;
    u64 w48[3];
} __attribute__((aligned(16))) TexturTestTexEnv;
typedef struct TexturTestEnv {
    u64 header[10];
    u64 packetWord;
    u64 tail[13];
} __attribute__((aligned(16))) TexturTestEnv;
extern void xglRenderClearDepth(void);

void TexturTest(void)
{
    static TexturTestTexEnv eNumberTexEnv __asm__("eNumberTexEnv.48") = {
        {0x0000000000000000ULL, 0x5100000511000000ULL, 0x1000000000000004ULL, 0x000000000000000EULL},
        0x00083C0000000000ULL,
        {0x0000000000000050ULL, 0x0000000000000000ULL, 0x0000000000000051ULL},
        0x0000008000000400ULL,
        {0x0000000000000052ULL, 0x0000000000000000ULL, 0x0000000000000053ULL},
    };
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
                eNumberTexEnv.w40 = lo | ((u64)entry->w4 << 32);
                eNumberTexEnv.w20 = ((u64)((unsigned int)(entry->addr + 0xE0000) >> 6) << 32) | ((u64)hi << 48);
                FlushCache(0);
                sceVif1PkRef(packet, &eNumberTexEnv, 6, 0, 0, 0);
                sceVif1PkRef(packet, data + entry->offset, entry->qwc, 0, 0, 0);
                entry++;
            } while (i < ((TexturTestHeader *)data)->count);
        }
            /* TestEnv.49..64 and x.50..xx.62: alternative texture packets the original
             * defines here; only TestEnv.52/x.53 are used. */
            {
            static u64 TestEnv[24] __asm__("TestEnv.49") __attribute__((aligned(16))) = {
                0x0000000000000000ULL, 0x5100000A00000000ULL,
                0xB08B400000008001ULL, 0x0000053531EEEEEEULL,
                0x00000000003FFFF0ULL, 0x0000000000000008ULL,
                0x0000000000000044ULL, 0x0000000000000042ULL,
                0x2007E78669343C00ULL, 0x0000000000000006ULL,
                0x0000000000030003ULL, 0x0000000000000047ULL,
                0x0000000000000000ULL, 0x000000000000003FULL,
                0x0000008000000080ULL, 0x0000008000000080ULL,
                0x0000000000000000ULL, 0x0000000000000000ULL,
                0x00007FF800006FF8ULL, 0x0000000000040000ULL,
                0x0000100000003000ULL, 0x0000000000000000ULL,
                0x00008DF800008FF8ULL, 0x0000000000040000ULL
            };
            static int x __asm__("x.50") = 0;
            }
            {
            static int x __asm__("x.51") = 0xDE00;
            }
            {
            static TexturTestEnv TestEnv __asm__("TestEnv.52") = {
                {
                    0x0000000000000000ULL, 0x5100000B00000000ULL,
                    0xA00B400000008001ULL, 0x00000042421EEEEEULL,
                    0x0000000000000000ULL, 0x000000000000003FULL,
                    0x0000000000000044ULL, 0x0000000000000042ULL,
                    0x01BF000001FF0000ULL, 0x0000000000000040ULL
                },
                0x0000000000000000ULL,
                {
                    0x0000000000000006ULL, 0x0000000000030003ULL,
                    0x0000000000000047ULL, 0x0000008000000080ULL,
                    0x0000008000000080ULL, 0x0000000000000000ULL,
                    0x000000003F800000ULL, 0x000071F800006FF8ULL,
                    0x0000000000040000ULL, 0x3F8000003F800000ULL,
                    0x000000003F800000ULL, 0x00008DF800008FF8ULL,
                    0x0000000000040000ULL
                },
            };
            static int x __asm__("x.53") = 0xDE00;
            XglPacket *send_packet = packet;
            int position;

            xglFontDebugHex(0, 0x20, x, 5);
            position = x;
            if (PadData.buttons.input.repeat & 0x2000) {
                x = position += 0x100;
            }
            if (PadData.buttons.input.repeat & 0x8000) {
                x = position -= 0x100;
            }
            TestEnv.packetWord = ((u64)((unsigned int)(position + 0xE0000) >> 6) << 37) | 0x2000000629343800ULL;
            sceVif1PkRef(send_packet, &TestEnv, 12, 0, 0, 0);
            }
            {
            static u64 TestEnv[20] __asm__("TestEnv.54") __attribute__((aligned(16))) = {
                0x0000000000000000ULL, 0x5100000900000000ULL,
                0x808B400000008001ULL, 0x0000000053531EEEULL,
                0x0000000000000044ULL, 0x0000000000000042ULL,
                0x2007EF8669343C00ULL, 0x0000000000000006ULL,
                0x0000000000050000ULL, 0x0000000000000047ULL,
                0x0000008000000080ULL, 0x0000008000000080ULL,
                0x0000000000001000ULL, 0x0000000000000000ULL,
                0x00007FF800007FF8ULL, 0x0000000000040000ULL,
                0x0000100000002000ULL, 0x0000000000000000ULL,
                0x00008DF800008FF8ULL, 0x0000000000040000ULL
            };
            static int x __asm__("x.55") = 0xDF00;
            }
            {
            static u64 TestEnv[22] __asm__("TestEnv.56") __attribute__((aligned(16))) = {
                0x0000000000000000ULL, 0x5100000A00000000ULL,
                0x908B400000008001ULL, 0x000000053531EEEEULL,
                0x0000000000000044ULL, 0x0000000000000042ULL,
                0x00000000003FDFF0ULL, 0x0000000000000008ULL,
                0x2007EF0669343C00ULL, 0x0000000000000006ULL,
                0x0000000000050000ULL, 0x0000000000000047ULL,
                0x0000008000000080ULL, 0x0000008000000080ULL,
                0x0000000000000000ULL, 0x0000000000000000ULL,
                0x000071F800006FF8ULL, 0x0000000000040000ULL,
                0x0000100000003000ULL, 0x0000000000000000ULL,
                0x00007FF800008FF8ULL, 0x0000000000040000ULL
            };
            static int x __asm__("x.57") = 0xDE00;
            }
            {
            static u64 TestEnv[24] __asm__("TestEnv.58") __attribute__((aligned(16))) = {
                0x0000000000000000ULL, 0x5100000B00000000ULL,
                0xA18B400000008001ULL, 0x000000E5353EEEEEULL,
                0x00000000000401E0ULL, 0x000000000000004DULL,
                0x0000000000000000ULL, 0x0000000000000019ULL,
                0x00000000FF000FFFULL, 0x0000000000000009ULL,
                0x0000000668013C00ULL, 0x0000000000000007ULL,
                0x0000000000031001ULL, 0x0000000000000048ULL,
                0x0000000000000000ULL, 0x0000000000000000ULL,
                0x0000000000000000ULL, 0x0000000000000000ULL,
                0x0000100000001000ULL, 0x0000000000000000ULL,
                0x0000100000000800ULL, 0x0000000000000000ULL,
                0x0000000000000000ULL, 0x000000000000003FULL
            };
            }
            {
            static u64 TestEnv[22] __asm__("TestEnv.59") __attribute__((aligned(16))) = {
                0x0000000000000000ULL, 0x5100000A00000000ULL,
                0x908B400000008001ULL, 0x000000053531EEEEULL,
                0x0000000000000044ULL, 0x0000000000000042ULL,
                0x00000000003FDFF0ULL, 0x0000000000000008ULL,
                0x2007EF0669343C00ULL, 0x0000000000000006ULL,
                0x0000000000050000ULL, 0x0000000000000047ULL,
                0x0000008000000080ULL, 0x0000008000000080ULL,
                0x0000000000000000ULL, 0x0000000000000000ULL,
                0x000071F800006FF8ULL, 0x0000000000040000ULL,
                0x0000100000002000ULL, 0x0000000000000000ULL,
                0x00007FF800008FF8ULL, 0x0000000000040000ULL
            };
            }
            {
            static u64 TestEnv[24] __asm__("TestEnv.60") __attribute__((aligned(16))) = {
                0x0000000000000000ULL, 0x5100000B00000000ULL,
                0xA18B400000008001ULL, 0x000000E5353EEEEEULL,
                0x0000000000000000ULL, 0x000000000000004DULL,
                0x0000000000000000ULL, 0x0000000000000019ULL,
                0x00000000007FDFF0ULL, 0x0000000000000009ULL,
                0x2007EF0669343C00ULL, 0x0000000000000007ULL,
                0x0000000000031001ULL, 0x0000000000000048ULL,
                0x0000000000000000ULL, 0x0000000000000000ULL,
                0x0000000000000000ULL, 0x0000000000000000ULL,
                0x0000100000002000ULL, 0x0000000000000000ULL,
                0x0000100000002000ULL, 0x0000000000000000ULL,
                0x0000000000000000ULL, 0x000000000000003FULL
            };
            }
            {
            static u64 TestEnv[22] __asm__("TestEnv.61") __attribute__((aligned(16))) = {
                0x0000000000000000ULL, 0x5100000A00000000ULL,
                0x908B400000008001ULL, 0x000000053531EEEEULL,
                0x0000000000000044ULL, 0x0000000000000042ULL,
                0x00000000FF000FFFULL, 0x0000000000000008ULL,
                0x0000000000000000ULL, 0x0000000000000006ULL,
                0x0000000000050000ULL, 0x0000000000000047ULL,
                0x0000008000000080ULL, 0x0000008000000080ULL,
                0x0000000000000000ULL, 0x0000000000000000ULL,
                0x00007FF800006FF8ULL, 0x0000000000040000ULL,
                0x0000100000002000ULL, 0x0000000000000000ULL,
                0x00008DF800008FF8ULL, 0x0000000000040000ULL
            };
            static int xx __asm__("xx.62") = 0;
            }
            {
            static u64 TestEnv[34] __asm__("TestEnv.63") __attribute__((aligned(16))) = {
                0x0000000000000000ULL, 0x5100001000000000ULL,
                0x1000000000008002ULL, 0x000000000000000EULL,
                0x0000000000000044ULL, 0x0000000000000042ULL,
                0x0000000000050000ULL, 0x0000000000000047ULL,
                0x2026C00000008006ULL, 0x0000000000000051ULL,
                0x0000008000000080ULL, 0x0000008000000080ULL,
                0x0000800000008000ULL, 0x0000000000FFFFFFULL,
                0x0000008000000080ULL, 0x0000000000000080ULL,
                0x0000760000007400ULL, 0x0000000000FFFFFFULL,
                0x0000008000000080ULL, 0x0000000000000080ULL,
                0x0000760000008C00ULL, 0x0000000000FFFFFFULL,
                0x0000008000000080ULL, 0x0000000000000080ULL,
                0x00008A0000008C00ULL, 0x0000000000FFFFFFULL,
                0x0000008000000080ULL, 0x0000000000000080ULL,
                0x00008A0000007400ULL, 0x0000000000FFFFFFULL,
                0x0000008000000080ULL, 0x0000000000000080ULL,
                0x0000760000007400ULL, 0x0000000000FFFFFFULL
            };
            }
            {
            static u64 TestEnv[26] __asm__("TestEnv.64") __attribute__((aligned(16))) = {
                0x0000000000000000ULL, 0x5100000C00000000ULL,
                0x1000000000008002ULL, 0x000000000000000EULL,
                0x0000000000000044ULL, 0x0000000000000042ULL,
                0x0000000000050000ULL, 0x0000000000000047ULL,
                0x8026400000008001ULL, 0x00000000515151D1ULL,
                0x0000008000000080ULL, 0x0000000000000080ULL,
                0x0000760000007400ULL, 0x0000000000FFFFFFULL,
                0x0000008000000080ULL, 0x0000000000000080ULL,
                0x00008A0000007400ULL, 0x0000000000FFFFFFULL,
                0x0000008000000080ULL, 0x0000000000000080ULL,
                0x0000760000008C00ULL, 0x0000000000FFFFFFULL,
                0x0000008000000080ULL, 0x0000000000000080ULL,
                0x00008A0000008C00ULL, 0x0000000000FFFFFFULL
            };
            }
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
    static WinTexEnvironment WinTexEnv __asm__("WinTexEnv.65") = {
        0x0, 0x0, 0x11000000, 0x51000006,
        {0x8005, 0x10000000, 0xEULL},
        {0x0ULL, 0x3FULL},
        {0x0008380000000000ULL, 0x50ULL},
        {0x0ULL, 0x51ULL},
        {0x0000008000000200ULL, 0x52ULL},
        {0x0ULL, 0x53ULL},
    };
    static unsigned int TestEnv[32] __asm__("TestEnv.66") __attribute__((aligned(16))) = {
        0x00008001, 0x208B4000, 0x000000EE, 0x00000000,
        0x00000000, 0x00000000, 0x0000003F, 0x00000000,
        0xDD343840, 0x20070005, 0x00000006, 0x00000000,
        0x00000080, 0x00000080, 0x00000080, 0x00000080,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00007C00, 0x00007BF8, 0x10000000, 0x00000000,
        0x00000800, 0x00000800, 0x00000000, 0x00000000,
        0x00008400, 0x000083F8, 0x10000000, 0x00000000
    };
    sceVif1PkCloseDirectHLCode(context->packet);
    sceVif1PkRef(context->packet, (unsigned char *)&WinTexEnv,
                 7, 0, 0, 0);
    sceVif1PkRef(context->packet, (unsigned char *) test_data + 0x30, 0x4002, 0, 0, 0);
    sceVif1PkCnt(context->packet, 0);
    sceVif1PkOpenDirectHLCode(context->packet, 0);
    sceVif1PkAddDirectDataN(context->packet, TestEnv, 8);
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
    static char fname[64] __asm__("fname.67");
    static float size __asm__("size.68");
    switch (result) {
    case 0: {
        const char *source = (const char *)amount;
        char *destination = fname;
        do {
            *destination = *source++;
        } while (((unsigned int)*destination++ << 24) != 0);
        break;
    }
    case 1:
        size = I2F(amount) * progress_percent;
        break;
    case 2:
    case 3:
        if (result != 3)
            xglSleep();
        break;
    case -1: {
        for (;;) {
            xglFontDebugPrintf(8, 216, "%s/NotFound", fname);
            xglSleep();
        }
    }
    case -2: {
        for (;;) {
            xglFontDebugPrintf(8, 216, "%s/ReadError", fname);
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
    char name[64];
} FileLoadTestFiles;
typedef struct FileLoadTestEnv {
    u64 header[6];
    u64 packetWord;
    u64 tail[13];
} __attribute__((aligned(16))) FileLoadTestEnv;
static FileLoadTestFiles files = {-1, NULL, "data\\endou\\menu00.xtx"};

void FileLoadTest(void)
{
    static TexturTestTexEnv eNumberTexEnv __asm__("eNumberTexEnv.69") = {
        {0x0000000000000000ULL, 0x5100000511000000ULL, 0x1000000000000004ULL, 0x000000000000000EULL},
        0x00083C0000000000ULL,
        {0x0000000000000050ULL, 0x0000000000000000ULL, 0x0000000000000051ULL},
        0x0000008000000200ULL,
        {0x0000000000000052ULL, 0x0000000000000000ULL, 0x0000000000000053ULL},
    };
    static FileLoadTestEnv TestEnv __asm__("TestEnv.70") = {
        {
            0x0000000000000000ULL, 0x5100000900000000ULL,
            0x80AB400000008001ULL, 0x0000000053531EEEULL,
            0x0000000000000044ULL, 0x0000000000000042ULL
        },
        0x2007EF8669343C00ULL,
        {
            0x0000000000000006ULL, 0x0000000000050000ULL,
            0x0000000000000047ULL, 0x0000008000000080ULL,
            0x0000008000000080ULL, 0x0000000000001000ULL,
            0x0000000000000000ULL, 0x00007FF800007FF8ULL,
            0x0000000040000000ULL, 0x0000100000002000ULL,
            0x0000000000000000ULL, 0x00008DF800008FF8ULL,
            0x0000000040000000ULL
        },
    };
    static int x __asm__("x.71") = 0xCF00;
    XglPacket *packet;
    FileLoadCallback old;

    xglRenderClearFrame();
    xglSleep();
    files.buffer = (unsigned char *)(((int)WorkEnd + 0x7F) & ~0x7F);
    WorkEnd = files.buffer + 0x100000;
    old = xglCdSetCallback(endCallback);
    xglCdReadFile("data\\endou\\menu00.xtx", files.buffer, 0, 0);
    files.state = 1;
    while ((((FileLoadPadView *)&PadData)->button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, "\013\245\355\241\274\245\311\245\306\245\271\245\310");
        if (files.state == 1) {
            packet = xglPacketGetCurrent();
            sceVif1PkRef(packet, &eNumberTexEnv, 6, 0, 0, 0);
            sceVif1PkRef(packet, files.buffer + 0x30, 0x4002, 0, 0, 0);
            xglFontDebugHex(0, 0x20, x, 5);
            if (((UmnMailDispTestPad *)&PadData)->rep & 0x2000) {
                x += 0x100;
            }
            if (((UmnMailDispTestPad *)&PadData)->rep & 0x8000) {
                x -= 0x100;
            }
            TestEnv.packetWord = ((u64)((unsigned int)(x + 0xF0000) >> 6) << 37) | 0x2000000669343C00ULL;
            sceVif1PkRef(packet, &TestEnv, 10, 0, 0, 0);
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
        xglFontDebugPrintf(0, 0, "\013\243\312\243\320\243\305\243\307\245\306\245\271\245\310");
        endPrintExtFunc(0, 0x0d, &display);
        xglSleep();
    }
}


void UmlTest(void)
{
    static char *buffer __asm__("buffer.72");
    static char mm[0x160] __asm__("mm.73");
  char *work;
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
  work = (char *) ((((int) WorkEnd) + 0x7F) ^ 0x7F);
  WorkEnd = ((u8 *) work) + 0x20000;
  buffer = work;
  memset(work, 0, 0x10000);
  xglCdReadFile("data\\endou\\test00.txt", work, 0, 0);
  d = mm;
  p = (unsigned char *) buffer;
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
    xglFontDebugPrintf(0, 0, "\013\243\325\243\315\243\314\245\306\245\271\245\310");
    xglFontPrint(0, 0x40, 0xFFFFF0, "\015\000\025\002\001\014\014\255");
    xglFontPrint(0, 0x40, 0xFFFFFF, mm);
    xglSleep();
  }

}

void PauseMenuPagePartyDebugTakeAgws(void)
{
    static int Select __asm__("Select.74") = 0;
    static char *text00[] __asm__("text00.75") = {"\244\312\244\267", "\244\242\244\352"};
    int i;

    switch (PadData.buttons.input.repeat) {
    case 0x8000:
        Select -= 1;
        break;
    case 0x2000:
        Select += 1;
        break;
    case 0x1000:
        Select -= 2;
        break;
    case 0x4000:
        Select += 2;
        break;
    default:
        break;
    }

    if (Select < 0) {
        Select += 0xc;
    }
    Select %= 0xc;

    if (PadData.buttons.input.pressed & 0x20) {
        if (PartyTakeAgwsCheck(Select + 0x11) != 0) {
            PartyTakeAgwsOff(Select + 0x11);
        } else {
            PartyTakeAgwsOn(Select + 0x11);
        }
    }

    xglFontDebugPrintf(8, 0x20, "\241\274\241\241\243\301\243\307\243\327\243\323\300\337\304\352\241\241\241\274");
    xglFontDebugPrintf((Select % 2) * 0x80 + 8, (Select / 2) * 0x10 + 0x30, "\241\344");
    for (i = 0; i < 0xc; i++) {
        xglFontDebugPrintf((i % 2) * 0x80 + 0x10, (i / 2) * 0x10 + 0x30, MenuCharNameGet(i + 0x11));
        xglFontDebugPrintf((i % 2) * 0x80 + 0x68, (i / 2) * 0x10 + 0x30, text00[PartyTakeAgwsCheck(i + 0x11)]);
    }
    xglFontDebugPrintf(8, 0xc0, "\275\275\273\372\245\255\241\274\241\247\245\253\241\274\245\275\245\353\260\334\306\260\241\241\241\241\241\241\241\373\241\247\244\242\244\352\241\277\244\312\244\267");
}

void PauseMenuPagePartyDebugLockParty(void)
{
    static int Select __asm__("Select.76") = 0;
    static char *text00[] __asm__("text00.77") = {"\245\252\245\325", "\245\252\245\363"};
    int i;

    switch (PadData.buttons.input.repeat) {
    case 0x8000:
        Select -= 1;
        break;
    case 0x2000:
        Select += 1;
        break;
    case 0x1000:
        Select -= 2;
        break;
    case 0x4000:
        Select += 2;
        break;
    default:
        break;
    }

    if (Select < 0) {
        Select += 0xc;
    }
    Select %= 0xc;

    if (PadData.buttons.input.pressed & 0x20) {
        if (PartyLockPartyCheck(Select + 1) != 0) {
            PartyLockPartyOff(Select + 1);
        } else {
            PartyLockPartyOn(Select + 1);
        }
    }

    xglFontDebugPrintf(8, 0x20, "\241\274\241\241\245\355\245\303\245\257\245\321\241\274\245\306\245\243\241\274\300\337\304\352\241\241\241\274");
    xglFontDebugPrintf((Select % 2) * 0x80 + 8, (Select / 2) * 0x10 + 0x30, "\241\344");
    for (i = 0; i < 0xc; i++) {
        xglFontDebugPrintf((i % 2) * 0x80 + 0x10, (i / 2) * 0x10 + 0x30, MenuCharNameGet(i + 1));
        xglFontDebugPrintf((i % 2) * 0x80 + 0x58, (i / 2) * 0x10 + 0x30, text00[PartyLockPartyCheck(i + 1)]);
    }
    xglFontDebugPrintf(8, 0xc0, "\275\275\273\372\245\255\241\274\241\247\245\253\241\274\245\275\245\353\260\334\306\260\241\241\241\241\241\241\241\373\241\247\245\252\245\363\241\277\245\252\245\325");
}

void PauseMenuPagePartyDebugOutFriend(void)
{
    static int Select __asm__("Select.78") = 0;
    static char *text00[] __asm__("text00.79") = {"\245\252\245\325", "\245\252\245\363"};
    int i;

    switch (PadData.buttons.input.repeat) {
    case 0x8000:
        Select -= 1;
        break;
    case 0x2000:
        Select += 1;
        break;
    case 0x1000:
        Select -= 2;
        break;
    case 0x4000:
        Select += 2;
        break;
    default:
        break;
    }

    if (Select < 0) {
        Select += 0xc;
    }
    Select %= 0xc;

    if (PadData.buttons.input.pressed & 0x20) {
        if (PartyOutFriendCheck(Select + 1) != 0) {
            PartyOutFriendOff(Select + 1);
        } else {
            PartyOutFriendOn(Select + 1);
        }
    }

    xglFontDebugPrintf(8, 0x20, "\241\274\241\241\245\242\245\246\245\310\245\325\245\354\245\363\245\311\300\337\304\352\241\241\241\274");
    xglFontDebugPrintf((Select % 2) * 0x80 + 8, (Select / 2) * 0x10 + 0x30, "\241\344");
    for (i = 0; i < 0xc; i++) {
        xglFontDebugPrintf((i % 2) * 0x80 + 0x10, (i / 2) * 0x10 + 0x30, MenuCharNameGet(i + 1));
        xglFontDebugPrintf((i % 2) * 0x80 + 0x58, (i / 2) * 0x10 + 0x30, text00[PartyOutFriendCheck(i + 1)]);
    }
    xglFontDebugPrintf(8, 0xc0, "\275\275\273\372\245\255\241\274\241\247\245\253\241\274\245\275\245\353\260\334\306\260\241\241\241\241\241\241\241\373\241\247\245\252\245\363\241\277\245\252\245\325");
}

void PauseMenuPagePartyDebugFriend(void)
{
    static int Select __asm__("Select.80") = 0;
    static char *text00[] __asm__("text00.81") = {"\244\244\244\312\244\244", "\244\244\244\353"};
    int i;

    switch (PadData.buttons.input.repeat) {
    case 0x8000:
        Select -= 1;
        break;
    case 0x2000:
        Select += 1;
        break;
    case 0x1000:
        Select -= 2;
        break;
    case 0x4000:
        Select += 2;
        break;
    default:
        break;
    }

    if (Select < 0) {
        Select += 7;
    }
    Select %= 7;

    if (PadData.buttons.input.pressed & 0x20) {
        if (PartyFriendCheck(Select + 1) != 0) {
            PartyFriendOff(Select + 1);
        } else {
            PartyFriendOn(Select + 1);
        }
    }

    xglFontDebugPrintf(8, 0x20, "\241\274\241\241\245\325\245\354\245\363\245\311\300\337\304\352\241\241\241\274");
    xglFontDebugPrintf((Select % 2) * 0x80 + 8, (Select / 2) * 0x10 + 0x30, "\241\344");
    for (i = 0; i < 7; i++) {
        xglFontDebugPrintf((i % 2) * 0x80 + 0x10, (i / 2) * 0x10 + 0x30, MenuCharNameGet(i + 1));
        xglFontDebugPrintf((i % 2) * 0x80 + 0x58, (i / 2) * 0x10 + 0x30, text00[PartyFriendCheck(i + 1)]);
    }
    xglFontDebugPrintf(8, 0xc0, "\275\275\273\372\245\255\241\274\241\247\245\253\241\274\245\275\245\353\260\334\306\260\241\241\241\241\241\241\241\373\241\247\244\244\244\353\241\277\244\244\244\312\244\244");
}

unsigned char *PartyDataGet(void);
typedef struct { unsigned short id; signed char lv; unsigned char pad; } AttackerEntry;

int PauseMenuPagePartyDebugAttacker(void)
{
    static int Select __asm__("Select.82") = 0;
    static char *text00[] __asm__("text00.83") = {"\243\316\243\325\243\314\243\314", "\243\320\243\261", "\243\320\243\262", "\243\320\243\263", "\243\320\243\264", "\243\320\243\265", "\243\320\243\266", "\243\301\243\261", "\243\301\243\262", "\243\301\243\263"};
    static char *text01[] __asm__("text01.84") = {"\243\261\241\245", "\243\262\241\245", "\243\263\241\245"};
    static char *text02[] __asm__("text02.85") = {"\245\255\245\343\245\351", "\245\335\245\270\245\267\245\347\245\363"};
    int i;
    int y;
    unsigned char *party;
    unsigned char *p;

    if (PadData.buttons.input.repeat & 0x1000) {
        if (Select == 0) {
            Select = 5;
        } else {
            Select -= 1;
        }
    }
    if (PadData.buttons.input.repeat & 0x4000) {
        if (Select == 5) {
            Select = 0;
        } else {
            Select += 1;
        }
    }
    if (Select < 3) {
        unsigned int v = *(unsigned short *)(PartyDataGet() + Select * 4 + 0x30);
        unsigned short rep = PadData.buttons.input.repeat;
        if (rep & 0x8000) {
            v = v != 0 ? v - 1 : 0x1D;
        }
        if (rep & 0x2000) {
            v = v != 0x1D ? v + 1 : 0;
        }
        *(unsigned short *)(PartyDataGet() + Select * 4 + 0x30) = v;
    } else {
        int v = *(signed char *)(PartyDataGet() + Select * 4 + 0x26);
        unsigned short rep = PadData.buttons.input.repeat;
        if (rep & 0x8000) {
            v = v != 0 ? v - 1 : 9;
        }
        if (rep & 0x2000) {
            v = v != 9 ? v + 1 : 0;
        }
        *(signed char *)(PartyDataGet() + Select * 4 + 0x26) = v;
    }
    party = PartyDataGet();
    xglFontDebugPrintf(8, 0x20, "\241\274\241\241\245\320\245\310\245\353\245\341\245\363\245\320\241\274\300\337\304\352\241\241\241\274");
    xglFontDebugPrintf(8, Select * 16 + 0x30, "\241\344");
    for (i = 0; i < 3; i++) {
        y = i * 16 + 0x30;
        xglFontDebugPrintf(0x10, y, text01[i]);
        xglFontDebugPrintf(0x30, y, text02[0]);
        xglFontDebugPrintf(0x70, y, MenuCharNameGet(*(unsigned short *)(party + i * 4 + 0x30)));
    }
    for (i = 0; i < 3; i++) {
        y = i * 16 + 0x60;
        xglFontDebugPrintf(0x10, y, text01[i]);
        xglFontDebugPrintf(0x30, y, text02[1]);
        xglFontDebugPrintf(0x70, y, text00[((AttackerEntry *)(party + 0x30))[i].lv]);
    }
    xglFontDebugPrintf(8, 0xC0, "\242\254\242\255\241\247\245\253\241\274\245\275\245\353\260\334\306\260\241\241\242\253\242\252\241\247\245\255\245\343\245\351\241\277\245\335\245\270\245\267\245\347\245\363\312\321\271\271");
}

int PauseMenuPagePartyDebugAttacker(void);

int PauseMenuPagePartyDebug(void)
{
    static int TopMode __asm__("TopMode.86") = 0;
    unsigned int x;

    xglFontDebugPrintf(8, 0x10, "\241\341\241\341\241\241\245\321\241\274\245\306\245\243\241\274\245\307\245\320\245\303\245\257\245\341\245\313\245\345\241\274\241\241\241\341\241\341");
    xglFontDebugPrintf(8, 0xB0, "\242\242\241\247\300\337\304\352\300\332\244\352\302\330\244\250");
    if ((PadData.buttons.input.repeat & 0x80) != 0) {
        if (TopMode == 4) {
            TopMode = 0;
        } else {
            TopMode += 1;
        }
    }
    x = TopMode < 5U;
    switch (TopMode) {
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

void *test_data = NULL;

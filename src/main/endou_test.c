#include "common.h"
#include "shared.h"
#include "endou_test.h"

extern void xglSleep(void);

INCLUDE_ASM("asm/main/nonmatchings/endou_test", EndouTest);

typedef struct PadDataButtonView {
    u8 unmodeled_00[0x28];
    u64 button_state;
} PadDataButtonView;

extern PadDataButtonView PadData;

extern void xglCdLoadOverlay(int overlayId);

extern void xglRenderClearFrame(void);

typedef struct TextTestMessage {
    u8 unmodeled_00[4];
    short x;
    short y;
    int color;
    u8 unmodeled_0C[0x10];
    u8 mode;
    u8 unmodeled_1D[0x33];
} TextTestMessage;
typedef struct TextTestEntry {
    int unk0;
    char *text;
} TextTestEntry;
extern void *UmnEventTextInit(void *work);
extern void UmnEventTextMake(int arg);
extern void endPrintInit(void);
extern TextTestEntry *UmnEventTextNextGet(int arg);
extern void eMessageSet(void *message, const char *text);
extern void eMessageModeChange(void *message, int mode);
extern void eMessageMain(void *message);
extern int eMessageNextPage(void *message, int arg);
extern void eMessageTextChange(void *message, const char *text);
extern void endPrintExtFunc(int color, int id, void *data);
extern const char D_004C2558[];

void TextTest(void)
{
    TextTestMessage message;
    TextTestEntry *entry;

    xglCdLoadOverlay(2);
    xglRenderClearFrame();
    WorkEnd = UmnEventTextInit(WorkEnd);
    UmnEventTextMake(1);
    endPrintInit();
    entry = UmnEventTextNextGet(0);
    if (entry == 0) {
        return;
    }
    eMessageSet(&message, entry->text);
    message.x = 0x40;
    message.y = 0x40;
    message.color = 0xFFFFF0;
    message.mode = 2;
    eMessageModeChange(&message, 0x22);
    while (xglFontDebugPrintf(0, 0x10, D_004C2558),
           (PadData.button_state & 0x08000100U) != 0x08000100U) {
        eMessageMain(&message);
        if ((*(u16 *)((u8 *)&PadData + 0x2A) & 8) && eMessageNextPage(&message, 0) == 0) {
            entry = UmnEventTextNextGet(1);
            if (entry == 0) {
                return;
            }
            eMessageTextChange(&message, entry->text);
            eMessageModeChange(&message, 0x22);
        }
        endPrintExtFunc(0, 100, 0);
        xglSleep();
    }
}

INCLUDE_ASM("asm/main/nonmatchings/endou_test", ShopTest);

extern void SeisanInit(void);
extern void SeisanMain(void);
extern void SeisanDisp(void);
void SeisanTest(void)
{
    const int exit_chord = 0x08000100;

    xglCdLoadOverlay(1);
    SeisanInit();
    xglRenderClearFrame();

    while ((PadData.button_state & exit_chord) != exit_chord) {
        SeisanMain();
        SeisanDisp();
        xglSleep();
    }
}

INCLUDE_ASM("asm/main/nonmatchings/endou_test", ModelTest);

INCLUDE_ASM("asm/main/nonmatchings/endou_test", BattleWindowTest);

INCLUDE_ASM("asm/main/nonmatchings/endou_test", eNumberTest);

typedef struct EModelTestState {
    unsigned char unmodeled_00[0x11];
    unsigned char isReady;
    unsigned char unmodeled_12[1];
    unsigned char extFuncFlag;
} EModelTestState;
extern void MenuModelUnitOpen(void *unit, int drawType);
extern void MenuModelControl(void *unit);

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

extern void ACT_init(void);
extern void MenuLoadInit(void);
extern int MenuModelInit(int work_start);
extern void MenuModelMemoryInit(void *base, int size);
extern void MenuModelMemorySet(int count);
extern void MenuModelCreate(int *model, int character_id);
extern void MenuModelExtFuncSet(int model,
                                void (*callback)(EModelTestState *),
                                int argument);
extern void MenuModelMain(void);
extern const char D_004C2AE8[];

void eModelTest(void)
{
    int model_id;

    xglCdLoadOverlay(1);
    ACT_init();
    xglRenderClearFrame();
    xglSleep();
    MenuLoadInit();
    MenuModelInit(0x01000000);
    MenuModelMemoryInit((void *)0x01100000, 0x00500000);
    MenuModelMemorySet(0);
    MenuModelCreate(&model_id, 29);
    MenuModelExtFuncSet(model_id, eModelTestMain, 0);

    while ((PadData.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2AE8);
        MenuModelMain();
        xglSleep();
    }
}

void ePrintTest(void)
{
}

INCLUDE_ASM("asm/main/nonmatchings/endou_test", eMessageTest);

typedef struct BgTestRenderState {
    unsigned char unmodeled_00[0x14];
    unsigned short packetControl;
} BgTestRenderState;
typedef struct BgTestEnvironment {
    unsigned char unmodeled_00[0x30];
    u64 packetWord;
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

void BgTest(void)
{
    XglPacket *packet;
    int packet_command = BG_TEST_COMMAND_BASE << BG_TEST_COMMAND_BASE_SHIFT;

    packet_command |= sRender.packetControl << BG_TEST_CONTROL_SHIFT;
    TestEnv_47.packetWord = packet_command |
        ((u64)BG_TEST_COMMAND_HIGH << BG_TEST_COMMAND_HIGH_SHIFT);
    packet = xglPacketGetCurrent();
    sceVif1PkRef(packet, &TestEnv_47, 10, 0, 0, 0);
}

typedef struct TexturTestPad {
    u8 unmodeled_00[0x2A];
    u16 trig;
    u16 rep;
} TexturTestPad;
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
extern void xglFontDebugHex(int x, int y, int value, int digits);
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
        if (((TexturTestPad *)&PadData)->rep & 0x2000) {
            x_53 = x += 0x100;
        }
        if (((TexturTestPad *)&PadData)->rep & 0x8000) {
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
        if ((PadData.button_state & 0x08000100U) == 0x08000100U) {
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

typedef struct UmnMailDispTestParam {
    int mode;
    u8 unmodeled_04[4];
    short x8;
    short xA;
    short xC;
    short xE;
    u8 unmodeled_10[8];
    short x18;
    short x1A;
    short x1C;
    short x1E;
    u8 unmodeled_20[0x20];
} UmnMailDispTestParam;
typedef struct UmnMailDispTestPad {
    u8 unmodeled_00[0x2A];
    u16 trig;
    u16 rep;
} UmnMailDispTestPad;
extern void tyaUmlDispInit(void);
extern int tyaUmlDispParamReset(UmnMailDispTestParam *disp, int mode);
extern void tyaUmlDispMain(UmnMailDispTestParam *disp);
extern const char D_004C2CD0[];

void UmnMailDispTest(void)
{
    UmnMailDispTestParam a;
    UmnMailDispTestParam b;
    u16 trig;
    u16 rep;

    tyaUmlDispInit();
    tyaUmlDispParamReset(&a, 0);
    tyaUmlDispInit();
    tyaUmlDispParamReset(&b, 0);
    a.mode = 2;
    a.x8 = 0x710;
    a.xA = 0x730;
    a.xC = 0x1E0;
    a.xE = 0xC8;
    a.x1C = 0x1E0;
    a.x1E = 0x180;
    b.mode = 3;
    b.x8 = 0x710;
    b.xA = 0x808;
    b.xC = 0x1E0;
    b.xE = 0xC8;
    b.x1C = 0x1E0;
    b.x1E = 0x180;
    while ((PadData.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2CD0);
        rep = ((UmnMailDispTestPad *)&PadData)->rep;
        if (rep & 0x1000) {
            a.x1A -= 8;
        }
        if (rep & 0x4000) {
            a.x1A += 8;
        }
        if (rep & 0x8000) {
            a.x18 -= 8;
        }
        if (rep & 0x2000) {
            a.x18 += 8;
        }
        trig = ((UmnMailDispTestPad *)&PadData)->trig;
        if (trig & 8) {
            a.mode = 0;
        }
        if (trig & 2) {
            a.mode = 1;
        }
        if (trig & 0x10) {
            a.mode = 2;
        }
        if (trig & 0x20) {
            a.mode = 3;
        }
        if (trig & 0x80) {
            a.mode = 4;
        }
        if (trig & 0x40) {
            a.mode = 5;
        }
        tyaUmlDispMain(&a);
        tyaUmlDispMain(&b);
        xglSleep();
    }
}

extern void sceVif1PkCloseDirectHLCode(XglPacket *packet);
extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);
extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int count);
extern void *test_data;
extern unsigned char TestEnv_66[];
extern unsigned char WinTexEnv_65[];

/*
 * The context e_test's argument points to: only the VIF packet pointer at
 * +0x00 is evidenced here (main VA 0x002728f8, lw $4,0x0($16) before each
 * sceVif1Pk* call); the rest stays an unmodeled span (docs/naming.md).
 */
typedef struct ETestContext {
    XglPacket *packet;
} ETestContext;

extern void xglFontReloadTexture(ETestContext *context, int mode);

static void e_test(ETestContext *context)
{
    sceVif1PkCloseDirectHLCode(context->packet);
    sceVif1PkRef(context->packet, WinTexEnv_65, 7, 0, 0, 0);
    sceVif1PkRef(context->packet, (unsigned char *) test_data + 0x30, 0x4002, 0, 0, 0);
    sceVif1PkCnt(context->packet, 0);
    sceVif1PkOpenDirectHLCode(context->packet, 0);
    sceVif1PkAddDirectDataN(context->packet, TestEnv_66, 8);
    xglFontReloadTexture(context, 0);
}

extern const char D_004C2CF0[];
extern const char D_004C2D08[];
extern const char D_004C2D18[];
extern void xglFontPrintExtFunc(int color, void (*func)(ETestContext *), void *arg);

void ReLoadTest(void)
{
    xglRenderClearFrame();
    xglSleep();
    test_data = (void *)(((int)WorkEnd + 0xF) & ~0xF);
    WorkEnd = (u8 *)test_data + xglCdReadFile(D_004C2CF0, test_data, 0, 0);
    while ((PadData.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2D08);
        xglFontPrint(0x40, 0x28, 0xFFFFFF, D_004C2D18);
        xglFontPrintExtFunc(0x01FFFFFF, e_test, 0);
        xglFontPrint(0x40, 0x48, 0x02FFFFFF, D_004C2D18);
        xglSleep();
    }
}

static void endCallback(int result, int amount)
{
    switch (result) {
    case 0: {
        const char *source = (const char *)amount;
        char *destination = callback_file_name;
        do {
            *destination = *source++;
        } while (((unsigned int)*destination++ << 24) != 0);
        break;
    }
    case 1:
        transfer_progress = I2F(amount) * progress_percent;
        break;
    case 2:
    case 3:
        if (result != 3)
            xglSleep();
        break;
    case -1: {
        for (;;) {
            xglFontDebugPrintf(8, 216, not_found_format, callback_file_name);
            xglSleep();
        }
    }
    case -2: {
        for (;;) {
            xglFontDebugPrintf(8, 216, read_error_format, callback_file_name);
            xglSleep();
        }
    }
    case 4:
        break;
    default:
        break;
    }
}

typedef void (*FileLoadCallback)(int result, int amount);
extern FileLoadCallback xglCdSetCallback(FileLoadCallback callback);
extern void xglFontDebugHex(int x, int y, int value, int digits);
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
    while ((PadData.button_state & 0x08000100U) != 0x08000100U) {
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

extern unsigned int GameSnapShotSaveThumbnail(int quality, unsigned int buffer);
extern int xglJpegDecode(void *request_address);
extern const char D_004C2DA8[];
typedef struct JpegTestRequest {
    void *src;
    void *dst;
    int unk08[2];
    short unk10;
    short unk12;
    short unk14;
    short unk16;
    int unk18[2];
} JpegTestRequest;
typedef struct JpegTestSprite {
    short w;
    short h;
    int color;
    unsigned char rgba[4];
    void *image;
} JpegTestSprite;

void JpegTest(void)
{
    JpegTestSprite sprite;
    JpegTestRequest request;
    u8 *src;
    u8 *dst;

    src = (u8 *)(((int)WorkEnd + 0x7F) & ~0x7F);
    WorkEnd = src + 0x80000;
    memset(src, 0, 0x80000);
    dst = (u8 *)(((int)WorkEnd + 0x7F) & ~0x7F);
    WorkEnd = dst + 0x80000;
    memset(dst, 0, 0x80000);
    GameSnapShotSaveThumbnail(100, (unsigned int)src);
    memset(&request, 0, sizeof(request));
    request.src = src;
    request.dst = dst;
    request.unk10 = 0;
    request.unk12 = 0;
    request.unk14 = 0;
    request.unk16 = 0;
    xglJpegDecode(&request);
    sprite.w = 100;
    sprite.h = 100;
    sprite.color = 0xFFFFFF;
    sprite.rgba[3] = 0x80;
    sprite.rgba[2] = 0x80;
    sprite.rgba[1] = 0x80;
    sprite.rgba[0] = 0x80;
    sprite.image = dst;
    while ((PadData.button_state & 0x08000100U) != 0x08000100U) {
        xglFontDebugPrintf(0, 0, D_004C2DA8);
        endPrintExtFunc(0, 0xD, &sprite);
        xglSleep();
    }
}

INCLUDE_ASM("asm/main/nonmatchings/endou_test", UmlTest);

typedef struct PadLockView {
    u8 unmodeled_00[0x2A];
    u16 trig;
    u16 rep;
} PadLockView;

extern char D_004DA768[];

extern char *MenuCharNameGet(int id);

extern char D_004C2E58[];

extern int Select_74;
extern char *text00_75[];
extern char D_004C2DF0[];
extern char D_004C2E08[];
extern void PartyTakeAgwsOn(int id);
extern void PartyTakeAgwsOff(int id);
extern int PartyTakeAgwsCheck(int id);

void PauseMenuPagePartyDebugTakeAgws(void)
{
    int i;
    char *name;

    switch (((PadLockView *)&PadData)->rep) {
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
    }
    if (Select_74 < 0) {
        Select_74 += 12;
    }
    Select_74 %= 12;
    if (((PadLockView *)&PadData)->trig & 0x20) {
        if (PartyTakeAgwsCheck(Select_74 + 0x11)) {
            PartyTakeAgwsOff(Select_74 + 0x11);
        } else {
            PartyTakeAgwsOn(Select_74 + 0x11);
        }
    }
    xglFontDebugPrintf(8, 0x20, D_004C2DF0);
    xglFontDebugPrintf((Select_74 % 2) * 128 + 8, (Select_74 / 2) * 16 + 0x30, D_004DA768);
    for (i = 0; i < 12; i++) {
        name = MenuCharNameGet(i + 0x11);
        xglFontDebugPrintf((i % 2) * 128 + 0x10, (i / 2) * 16 + 0x30, name);
        xglFontDebugPrintf((i % 2) * 128 + 0x68, (i / 2) * 16 + 0x30, text00_75[PartyTakeAgwsCheck(i + 0x11)]);
    }
    xglFontDebugPrintf(8, 0xC0, D_004C2E08);
}

extern int Select_76;
extern char *text00_77[];
extern char D_004C2E38[];
extern void PartyLockPartyOn(int id);
extern void PartyLockPartyOff(int id);
extern int PartyLockPartyCheck(int id);
int PauseMenuPagePartyDebugLockParty(void)
{
    int i;
    char *name;

    switch (((PadLockView *)&PadData)->rep) {
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
    }
    if (Select_76 < 0) {
        Select_76 += 12;
    }
    Select_76 %= 12;
    if (((PadLockView *)&PadData)->trig & 0x20) {
        if (PartyLockPartyCheck(Select_76 + 1)) {
            PartyLockPartyOff(Select_76 + 1);
        } else {
            PartyLockPartyOn(Select_76 + 1);
        }
    }
    xglFontDebugPrintf(8, 0x20, D_004C2E38);
    xglFontDebugPrintf((Select_76 % 2) * 128 + 8, (Select_76 / 2) * 16 + 0x30, D_004DA768);
    for (i = 0; i < 12; i++) {
        name = MenuCharNameGet(i + 1);
        xglFontDebugPrintf((i % 2) * 128 + 0x10, (i / 2) * 16 + 0x30, name);
        xglFontDebugPrintf((i % 2) * 128 + 0x58, (i / 2) * 16 + 0x30, text00_77[PartyLockPartyCheck(i + 1)]);
    }
    xglFontDebugPrintf(8, 0xC0, D_004C2E58);
}

extern int Select_78;
extern char *text00_79[];
extern char D_004C2E88[];

int PauseMenuPagePartyDebugOutFriend(void)
{
    int i;
    char *name;

    switch (((PadLockView *)&PadData)->rep) {
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
    }
    if (Select_78 < 0) {
        Select_78 += 12;
    }
    Select_78 %= 12;
    if (((PadLockView *)&PadData)->trig & 0x20) {
        if (PartyOutFriendCheck(Select_78 + 1)) {
            PartyOutFriendOff(Select_78 + 1);
        } else {
            PartyOutFriendOn(Select_78 + 1);
        }
    }
    xglFontDebugPrintf(8, 0x20, D_004C2E88);
    xglFontDebugPrintf((Select_78 % 2) * 128 + 8, (Select_78 / 2) * 16 + 0x30, D_004DA768);
    for (i = 0; i < 12; i++) {
        name = MenuCharNameGet(i + 1);
        xglFontDebugPrintf((i % 2) * 128 + 0x10, (i / 2) * 16 + 0x30, name);
        xglFontDebugPrintf((i % 2) * 128 + 0x58, (i / 2) * 16 + 0x30, text00_79[PartyOutFriendCheck(i + 1)]);
    }
    xglFontDebugPrintf(8, 0xC0, D_004C2E58);
}

extern int Select_80;
extern char *text00_81[];
extern char D_004DA768[];
extern char D_004C2EA8[];
extern char D_004C2EC0[];
extern void PartyFriendOn(int party_id);
extern void PartyFriendOff(int id);
extern int PartyFriendCheck(int id);
extern char *MenuCharNameGet(int id);
typedef struct PadFriendView {
    u8 unmodeled_00[0x2A];
    u16 trig;
    u16 rep;
} PadFriendView;

int PauseMenuPagePartyDebugFriend(void)
{
    int i;
    char *name;

    switch (((PadFriendView *)&PadData)->rep) {
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
    }
    if (Select_80 < 0) {
        Select_80 += 7;
    }
    Select_80 %= 7;
    if (((PadFriendView *)&PadData)->trig & 0x20) {
        if (PartyFriendCheck(Select_80 + 1)) {
            PartyFriendOff(Select_80 + 1);
        } else {
            PartyFriendOn(Select_80 + 1);
        }
    }
    xglFontDebugPrintf(8, 0x20, D_004C2EA8);
    xglFontDebugPrintf((Select_80 % 2) * 128 + 8, (Select_80 / 2) * 16 + 0x30, D_004DA768);
    for (i = 0; i < 7; i++) {
        name = MenuCharNameGet(i + 1);
        xglFontDebugPrintf((i % 2) * 128 + 0x10, (i / 2) * 16 + 0x30, name);
        xglFontDebugPrintf((i % 2) * 128 + 0x58, (i / 2) * 16 + 0x30, text00_81[PartyFriendCheck(i + 1)]);
    }
    xglFontDebugPrintf(8, 0xC0, D_004C2EC0);
}

INCLUDE_ASM("asm/main/nonmatchings/endou_test", PauseMenuPagePartyDebugAttacker);

extern char D_004C2F68[];
extern char D_004C2F90[];
extern int TopMode_86;
extern void PauseMenuPagePartyDebugTakeAgws(void);
extern int PauseMenuPagePartyDebugLockParty(void);
extern int PauseMenuPagePartyDebugOutFriend(void);
extern int PauseMenuPagePartyDebugFriend(void);
extern int PauseMenuPagePartyDebugAttacker(void);

int PauseMenuPagePartyDebug(void)
{
    xglFontDebugPrintf(8, 16, D_004C2F68);
    xglFontDebugPrintf(8, 176, D_004C2F90);
    if (*(u16 *)((u8 *)&PadData + 0x2C) & 0x80) {
        if (TopMode_86 == 4) {
            TopMode_86 = 0;
        } else {
            TopMode_86++;
        }
    }
    switch (TopMode_86) {
    case 0:
        return PauseMenuPagePartyDebugFriend();
    case 1:
        return PauseMenuPagePartyDebugAttacker();
    case 2:
        return PauseMenuPagePartyDebugOutFriend();
    case 3:
        return PauseMenuPagePartyDebugLockParty();
    case 4:
        PauseMenuPagePartyDebugTakeAgws();
        break;
    }
}

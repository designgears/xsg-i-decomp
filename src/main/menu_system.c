#include "common.h"

extern void subMenuSystemInit(int workEnd);
extern void subMenuSystemMain(void);
extern int MainMenuWorkEnd;
extern void ChangeTopLevel(int level);

/* Offsets named here are touched by MenuSystemInfoMain, MenuSystemInitSet,
 * and the existing MenuSystem2 completion poll. */
typedef struct MenuSystemWork {
    unsigned char unmodeled_00[3];
    unsigned char menuState;
    unsigned char unmodeled_04[0x11 - 4];
    unsigned char state;
    unsigned char unmodeled_12[0x20 - 0x12];
    signed char messageIndex;
    unsigned char unmodeled_21[0x22 - 0x21];
    signed char hddMounted;
    unsigned char unmodeled_23[0x0d];
    unsigned char soundOutput;
    unsigned char padMode;
    unsigned char radarMode;
    unsigned char displayMode;
    unsigned char unmodeled_34;
    unsigned char hddActive;
} MenuSystemWork;

extern MenuSystemWork MenuWork;

extern unsigned char *MenuSystemPas;
extern char *msg00_0_0036DCF8[];
extern void WindowDXSet(void *window);
extern void WindowDXMain(void *window);
extern void MoveSlide(short *value, short *destination, float speed);
extern void eMessageSet(void *message, const char *text);
extern void eMessageMain(void *message);
extern void endPrintExtFunc(int kind, int id, void *data);
typedef struct MenuSystemPasWork {
    unsigned char state;
    unsigned char pad01[3];
    int color;
    short x;
    short y;
    int previousColor;
    short width;
    short height;
    unsigned char pad14[4];
    signed char transition;
    unsigned char pad19[0x19C - 0x19];
    unsigned char message[4];
    short slide;
    short pad1A2;
    int messageColor;
    unsigned char pad1A8[0x334 - 0x1A8];
    short printX;
    short printY;
    int printColor;
    short printW;
    short printH;
} MenuSystemPasWork;

void MenuSystemPasMain(void)
{
    MenuSystemPasWork *w = (MenuSystemPasWork *)MenuSystemPas;
    short *win;
    short target[8];
    short dest[8];
    unsigned char *m;
    int c20, cB, c120;

    switch (w->state) {
    case 0:
        win = &w->x;
        w->color = 0xFFFFF0;
        c20 = 0x20;
        c120 = 0x120;
        cB = 0xB;
        WindowDXSet(win);
        *win = -0x120;
        w->previousColor = w->color;
        w->y = 8;
        w->width = 0x110;
        w->height = 0x1E;
        w->transition = 1;
        WindowDXMain(win);
        w->transition = 3;
        eMessageSet(w->message, msg00_0_0036DCF8[0]);
        m = (unsigned char *)w + 0x190;
        m[0xD] = c20;
        *(short *)(m + 0x10) = c120;
        *(short *)(m + 0x12) = cB;
        w->state = 2;
        w->messageColor = w->color + 2;
        break;
    case 2:
        win = &w->x;
        break;
    default:
        return;
    }
    dest[0] = -0x10;
    target[0] = 0x120;
    switch (MenuWork.state) {
    case 0x10:
    case 0x20:
    case 0x30:
    case 0x40:
    case 0x50:
    case 0x60:
    case 0x70:
        target[0] = 0x10;
        break;
    default:
        dest[0] = -0x120;
        break;
    }
    MoveSlide(win, dest, 3.0f);
    WindowDXMain(win);
    if (w->transition == 3) {
        w->printX = *win + 3;
        w->printY = w->y + 3;
        w->printW = w->width - 6;
        w->printH = w->height - 6;
        w->printColor = w->color;
        endPrintExtFunc(w->color, 0x65, &w->printX);
        MoveSlide(&w->slide, target, 5.0f);
        if (w->slide < 0x100) {
            eMessageMain(w->message);
        }
        endPrintExtFunc(w->color, 0x66, 0);
    }
}

#include "shared.h"
#include "main/party.h"
#include "main/xgl_hdd.h"
typedef struct MenuSystemPad {
    unsigned char unmodeled_00[0x56];
    unsigned char vibrationReset;
    unsigned char unmodeled_57[0xbe - 0x57];
    unsigned char vibrationStatus;
} MenuSystemPad;
typedef struct MenuSystemPartyDataPrefix {
    unsigned char unmodeled_00[0x2e];
    unsigned char radar_disp;
} MenuSystemPartyDataPrefix;
extern MenuSystemPad PadData;
void SsdSetOutputMode(int mode);
void PartyRadarDispSet(int enabled);
void tyaDisplaySetting(int mode);
typedef struct MenuSystemInfoBlock {
    unsigned char state;
    unsigned char active;
    unsigned char unmodeled_02[2];
    int color;
    short x;
    short y;
    int previousColor;
    short width;
    short height;
    unsigned char unmodeled_14[4];
    unsigned char transition;
    unsigned char unmodeled_19[3];
    void (*window)(void);
    void *subwindow;
    unsigned char unmodeled_24[0x1a0 - 0x24];
    short cursorX;
    short cursorY;
    unsigned char unmodeled_1a4[4];
    int visible;
    unsigned char *message;
    unsigned char unmodeled_1b0[0x350 - 0x1b0];
    unsigned char text[128];
} MenuSystemInfoBlock;
typedef struct MenuSystemMessageTable {
    int messages[7];
} MenuSystemMessageTable;
extern MenuSystemInfoBlock *MenuSystemInfo;
extern const MenuSystemMessageTable D_004C9480;
extern void MenuInfoWindow(void);
extern void WindowDXSet(void *window);
extern void WindowDXMain(void *window);
extern void eMessageCpy(unsigned char *message, int id);
extern void MoveSlide(short *value, short *destination, float speed);
extern void *memset(void *destination, int value, unsigned int count);

void MenuSystemInfoMain(void)
{
    MenuSystemInfoBlock *info = MenuSystemInfo;
    MenuSystemMessageTable messageTable = D_004C9480;
    short targetY;

    if (info->state != 0) {
        if (info->state != 2)
            return;
    } else {
        info->color = 0xFFFFF0;
        WindowDXSet(&info->x);
        info->x = -16;
        info->y = 480;
        info->width = 544;
        info->height = 78;
        info->previousColor = info->color;
        info->subwindow = &info->cursorX;
        info->cursorX = 0;
        info->window = MenuInfoWindow;
        info->transition = 1;
        info->cursorY = 0;
        info->message = 0;
        WindowDXMain(&info->x);
        info->transition = 3;
        info->visible = 1;
        info->active = (info->transition == 3);
        memset(info->text, 0, sizeof(info->text));
        info->state = 2;
    }

    targetY = 362;
    switch (MenuWork.state) {
    case 0x10:
    case 0x20:
    case 0x30:
    case 0x40:
    case 0x50:
    case 0x60:
    case 0x70:
        eMessageCpy(info->text, messageTable.messages[MenuWork.messageIndex]);
        break;
    default:
        targetY = 512;
        break;
    }
    MoveSlide(&info->y, &targetY, 3.0f);
    info->message = info->text;
    WindowDXMain(&info->x);
}

INCLUDE_ASM("asm/main/nonmatchings/menu_system", MenuSystemMenuMain);

enum {
    SAVE_DATA_BYTE_28 = 0x28,
    SAVE_DATA_BYTE_44 = 0x44,
    SAVE_DATA_BYTE_3C = 0x3c,
    SAVE_DATA_BYTE_40 = 0x40
};

void MenuSystemInitSet(void)
{
    MenuSystemPartyDataPrefix *party;

    SsdSetOutputMode(1);
    SaveData[SAVE_DATA_BYTE_28] = 1;
    MenuWork.soundOutput = 0;
    SaveData[SAVE_DATA_BYTE_44] = 0;
    MenuWork.padMode = 0;
    SaveData[SAVE_DATA_BYTE_3C] = 0;
    PadData.vibrationReset = 0;
    PadData.vibrationStatus = 0;
    MenuWork.radarMode = 0;
    PartyRadarDispSet(1);
    party = (MenuSystemPartyDataPrefix *)PartyDataGet();
    MenuWork.displayMode = party->radar_disp ^ 1;
    tyaDisplaySetting(1);
    if (MenuWork.hddMounted != 0) {
        SaveData[SAVE_DATA_BYTE_40] = 0;
        xglHddActivate(1);
        MenuWork.hddActive = SaveData[SAVE_DATA_BYTE_40];
    }
}

INCLUDE_ASM("asm/main/nonmatchings/menu_system", subMenuSystemMain);

extern int MenuHddCheck;
extern unsigned char *MenuSystemPas;
extern unsigned char *MenuSystemMenu;

void subMenuSystemInit(int workEnd)
{
    unsigned char *p;
    int r;

    p = (unsigned char *)((workEnd + 0x7F) & ~0x7F);
    MenuSystemPas = p;
    p += 0x340;
    *MenuSystemPas = 0;
    *(unsigned char **)&MenuSystemInfo = p;
    p += 0x3D0;
    MenuSystemInfo->state = 0;
    MenuSystemMenu = p;
    p += 0x100;
    *MenuSystemMenu = 0;
    MenuWork.unmodeled_12[0] = 0x10;
    MenuWork.unmodeled_12[1] = 0xC;
    MenuWork.soundOutput = SaveData[SAVE_DATA_BYTE_28] ^ 1;
    MenuWork.padMode = SaveData[SAVE_DATA_BYTE_44];
    MenuWork.radarMode = SaveData[SAVE_DATA_BYTE_3C];
    MenuWork.unmodeled_04[0xC] = 0;
    MenuWork.displayMode = ((MenuSystemPartyDataPrefix *)PartyDataGet())->radar_disp ^ 1;
    r = xglHddMount();
    MenuHddCheck = r;
    if (r == 1 && xglHddActivate(-1) != 0) {
        MenuWork.hddMounted = r;
        MenuWork.hddActive = SaveData[SAVE_DATA_BYTE_40];
    } else {
        MenuWork.hddActive = 1;
        MenuWork.hddMounted = 0;
    }
}

void MenuSystem(void)
{
    switch (MenuWork.menuState) {
        /* Keep both completion paths in this single dispatch scope. */
        do {
        case 0:
            subMenuSystemInit(MainMenuWorkEnd);
            MenuWork.menuState = 2;
            /* fall through */
        case 2:
            subMenuSystemMain();
            if (MenuWork.state != 0xFF)
                break;
            MenuWork.menuState = 4;
            /* fall through */
        case 4:
            ChangeTopLevel(0);
            break;
        } while (0);
    }
}

extern void endPrintInit(void);
extern void subMenuSystemInit(int workEnd);
extern int MainMenuWorkEnd;

void MenuSystem2Init(void) {
    endPrintInit();
    subMenuSystemInit(MainMenuWorkEnd);
}

extern void subMenuSystemMain(void);
extern void endPrintExtFunc(int kind, int id, void *data);

/*
 * Drives the system-options screen for one frame and reports whether it is
 * still running: MenuWork.state is 0xFF once the screen driven by
 * subMenuSystemMain has finished.
 */
int MenuSystem2(void)
{
    subMenuSystemMain();
    endPrintExtFunc(0, 100, 0);
    return MenuWork.state != 0xFF;
}

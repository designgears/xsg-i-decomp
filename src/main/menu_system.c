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

extern unsigned char *MenuSystemMenu;
extern void eCursolSet(void *cursol, signed char width);
extern void eCursolMain(void *cursol);
extern void MenuSelectWindow(void);
extern char D_004C9510[];
extern char D_004C9520[];
extern char *msg00_1_0036DD00[];
extern char *msg01_2_0036DD20[];
extern char *msg02_3_0036DD28[];
extern char *msg03_4_0036DD30[];
typedef struct WindowDXUp {
    short x;
    short y;
    int style;
    short width;
    short height;
    const char *tag_id;
    unsigned char state;
    unsigned char ribbon_initialized;
    unsigned char unmodeled_12[2];
    void (*close_callback)(struct WindowDXUp *window, void *arg);
    void *close_callback_arg;
    unsigned char unmodeled_1c[0x194 - 0x1c];
} WindowDXUp;
typedef struct MenuSystemMessageUp {
    unsigned char unmodeled_00;
    unsigned char flag;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    unsigned char unmodeled_0c[4];
    unsigned char rgb[3];
    unsigned char unmodeled_13[5];
    const char *text;
    unsigned char unmodeled_1c[0x44 - 0x1c];
} MenuSystemMessageUp;
typedef struct MenuSystemCursorUp {
    unsigned char flag;
    unsigned char unmodeled_01[3];
    short x;
    short y;
    int color;
    unsigned char unmodeled_0c[0x24 - 0x0c];
} MenuSystemCursorUp;
typedef struct MenuSystemChoiceUp {
    unsigned char unmodeled_00[2];
    short enabled;
    int cursor;
    const char *choices;
    const char *question;
} MenuSystemChoiceUp;
typedef struct MenuSystemMenuBlockUp {
    unsigned char state;
    unsigned char unmodeled_01[3];
    int color;
    WindowDXUp titleWindow;
    MenuSystemMessageUp label[8];
    MenuSystemMessageUp value[8];
    MenuSystemCursorUp labelCursor;
    WindowDXUp choiceWindow;
    MenuSystemMessageUp choiceLabel[3];
    MenuSystemCursorUp choiceCursor;
    WindowDXUp confirmWindow;
    MenuSystemChoiceUp confirm;
} MenuSystemMenuBlockUp;
typedef struct MenuSystemWorkUp {
    unsigned char unmodeled_00[3];
    unsigned char menuState;
    unsigned char unmodeled_04[0x10 - 4];
    unsigned char flags;
    unsigned char state;
    unsigned char nextState;
    unsigned char inputDelay;
    unsigned char unmodeled_14[0x20 - 0x14];
    signed char messageIndex;
    signed char choiceIndex;
    signed char hddMounted;
    unsigned char unmodeled_23[0x0d];
    unsigned char options[7];
} MenuSystemWorkUp;
typedef const char *MenuSystemOptionNamesUp[2];
extern MenuSystemOptionNamesUp *const D_004C9530[];
extern MenuSystemWorkUp MenuWorkUp __asm__("MenuWork");

void MenuSystemMenuMain(void)
{
    MenuSystemMenuBlockUp *menu = (MenuSystemMenuBlockUp *)MenuSystemMenu;
    short titleTarget;
    short choiceTarget;
    short confirmTarget;
    int row;
    MenuSystemCursorUp *labelCursor;
    WindowDXUp *confirmWindow;
    WindowDXUp *choiceWindow;
    MenuSystemCursorUp *choiceCursor;

    switch (menu->state) {
    case 0:
        menu->color = 0xfffff0;
        WindowDXSet(&menu->titleWindow);
        menu->titleWindow.x = 528;
        menu->titleWindow.y = 116;
        menu->titleWindow.style = menu->color;
        menu->titleWindow.width = 397;
        menu->titleWindow.height = 174;
        menu->titleWindow.tag_id = D_004C9510;
        menu->titleWindow.state = 1;
        WindowDXMain(&menu->titleWindow);
        menu->titleWindow.state = 3;
        for (row = 0; row < 7; row++) {
            eMessageSet(&menu->label[row], 0);
            menu->label[row].flag = 0x20;
            menu->label[row].color = menu->color + 2;
            eMessageSet(&menu->value[row], 0);
            menu->value[row].flag = 0x20;
            menu->value[row].color = menu->color + 2;
        }
        if (MenuWorkUp.hddMounted == 0) {
            menu->label[5].rgb[2] = 64;
            menu->label[5].rgb[1] = 64;
            menu->label[5].rgb[0] = 64;
            menu->value[5].rgb[2] = 64;
            menu->value[5].rgb[1] = 64;
            menu->value[5].rgb[0] = 64;
        }
        labelCursor = &menu->labelCursor;
        choiceWindow = &menu->choiceWindow;
        eCursolSet(labelCursor, 0);
        menu->labelCursor.color = menu->color + 2;
        labelCursor->flag = 0x20;
        WindowDXSet(choiceWindow);
        choiceWindow->x = 528;
        menu->choiceWindow.y = 116;
        menu->choiceWindow.style = menu->color;
        menu->choiceWindow.width = 113;
        menu->choiceWindow.height = 70;
        menu->choiceWindow.tag_id = D_004C9520;
        menu->choiceWindow.state = 1;
        WindowDXMain(choiceWindow);
        menu->choiceWindow.state = 3;
        for (row = 0; row < 3; row++) {
            eMessageSet(&menu->choiceLabel[row], msg01_2_0036DD20[0]);
            menu->choiceLabel[row].flag = 0x20;
            menu->choiceLabel[row].color = menu->color + 2;
        }
        choiceCursor = &menu->choiceCursor;
        confirmWindow = &menu->confirmWindow;
        eCursolSet(choiceCursor, 0);
        menu->choiceCursor.color = menu->color + 2;
        choiceCursor->flag = 0x20;
        WindowDXSet(confirmWindow);
        confirmWindow->x = 544;
        menu->confirmWindow.y = 116;
        menu->confirmWindow.style = menu->color;
        menu->confirmWindow.width = 169;
        menu->confirmWindow.height = 102;
        menu->confirmWindow.tag_id = D_004C9520;
        menu->confirm.choices = msg03_4_0036DD30[1];
        menu->confirmWindow.close_callback = (void (*)(WindowDXUp *, void *))MenuSelectWindow;
        menu->confirmWindow.close_callback_arg = &menu->confirm;
        menu->confirm.question = msg03_4_0036DD30[0];
        menu->confirm.cursor = 0;
        menu->confirmWindow.state = menu->confirm.enabled = 1;
        WindowDXMain(confirmWindow);
        menu->confirmWindow.state = 3;
        menu->state = 2;
        break;
    case 2:
        labelCursor = &menu->labelCursor;
        choiceWindow = &menu->choiceWindow;
        choiceCursor = &menu->choiceCursor;
        confirmWindow = &menu->confirmWindow;
        break;
    default:
        return;
    }

    titleTarget = 528;
    choiceTarget = 528;
    confirmTarget = 528;
    switch (MenuWorkUp.state) {
    case 0x10:
        titleTarget = 48;
        break;
    case 0x20:
        if ((unsigned char)(MenuWorkUp.flags & 1)) {
            menu->choiceWindow.height = 54;
            menu->choiceLabel[0].text = msg01_2_0036DD20[0];
            menu->choiceLabel[1].text = msg01_2_0036DD20[1];
            menu->choiceLabel[2].text = 0;
        }
        titleTarget = 0;
        choiceTarget = 400;
        break;
    case 0x30:
    case 0x40:
    case 0x50:
    case 0x70:
        if ((unsigned char)(MenuWorkUp.flags & 1)) {
            menu->choiceWindow.height = 54;
            menu->choiceLabel[0].text = msg02_3_0036DD28[0];
            menu->choiceLabel[1].text = msg02_3_0036DD28[1];
            menu->choiceLabel[2].text = 0;
        }
        titleTarget = 0;
        choiceTarget = 400;
        break;
    case 0x60:
        titleTarget = -56;
        confirmTarget = 344;
        break;
    }
    for (row = 0; row < 1; row++) {
        WindowDXUp *titleWindow = (WindowDXUp *)((unsigned char *)menu + row + 8);
        MoveSlide(&titleWindow->x, &titleTarget, 3.0f);
        WindowDXMain(titleWindow);
    }
    {
        MenuSystemOptionNamesUp *settingLabels[7];
        __builtin_memcpy(settingLabels, D_004C9530, sizeof(settingLabels));
        for (row = 0; row < 7; row++) {
            if (row == 5)
                continue;
            menu->label[row].x = menu->titleWindow.x + 19;
            menu->label[row].text = msg00_1_0036DD00[row];
            menu->label[row].y = menu->titleWindow.y + row * 24 + 3;
            eMessageMain(&menu->label[row]);
            if (row != 4 && row != 6) {
                if (row == 5 && MenuWorkUp.hddMounted == 0)
                    menu->value[5].text = msg02_3_0036DD28[1];
                else
                    menu->value[row].text = (*settingLabels[row])[MenuWorkUp.options[row]];
                menu->value[row].x = menu->titleWindow.x + 303;
                menu->value[row].y = menu->titleWindow.y + row * 24 + 3;
                eMessageMain(&menu->value[row]);
            }
        }
    }
    menu->labelCursor.x = menu->titleWindow.x + 3;
    menu->labelCursor.y = menu->titleWindow.y + MenuWorkUp.messageIndex * 24 + 7;
    eCursolMain(labelCursor);
    MoveSlide(&choiceWindow->x, &choiceTarget, 3.0f);
    WindowDXMain(choiceWindow);
    for (row = 0; row < 3; row++) {
        menu->choiceLabel[row].x = menu->choiceWindow.x + 19;
        menu->choiceLabel[row].y = menu->choiceWindow.y + row * 24 + 3;
        eMessageMain(&menu->choiceLabel[row]);
    }
    menu->choiceCursor.x = menu->choiceWindow.x + 3;
    menu->choiceCursor.y = menu->choiceWindow.y + MenuWorkUp.choiceIndex * 24 + 7;
    eCursolMain(choiceCursor);
    MoveSlide(&confirmWindow->x, &confirmTarget, 3.0f);
    menu->confirm.cursor = MenuWorkUp.choiceIndex;
    WindowDXMain(confirmWindow);
}

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

extern void xglSleep(void);
extern unsigned char D_004DB578[];
extern int MenuModelWorkTop;
extern int D_004DB580[];
extern int D_004DB588[];
extern int D_004DB590[];
extern int D_004DB598[];
extern int D_004DB5A0[];
extern char D_004DB5A8[];
extern char D_004DB5B0[];
typedef struct MenuSystemWorkSM {
    unsigned char unmodeled_00[3];
    unsigned char menuState;
    unsigned char unmodeled_04[0x10 - 4];
    unsigned char flags;
    unsigned char state;
    unsigned char nextState;
    unsigned char inputDelay;
    unsigned char unmodeled_14[0x20 - 0x14];
    signed char messageIndex;
    signed char choiceIndex;
    signed long long hddMounted : 8;
    unsigned char unmodeled_23[0x0d];
    unsigned char soundOutput;
    unsigned char padMode;
    unsigned char radarMode;
    unsigned char displayMode;
    unsigned char unmodeled_34;
    unsigned char hddActive;
} MenuSystemWorkSM;
typedef struct MenuSystemPadSM {
    unsigned char unmodeled_00[0x2a];
    unsigned short pressed;
    unsigned char unmodeled_2c[0x56 - 0x2c];
    unsigned char vibrationReset;
    unsigned char unmodeled_57[0xbe - 0x57];
    unsigned char vibrationStatus;
} MenuSystemPadSM;

void subMenuSystemMain(void)
{
    unsigned char optionStates[7];
    int outputModes[2];
    int displayModes[2];
    int vibrationModes[2];
    int padModes[2];
    int hddModes[2];
    int flags;

    if (((MenuSystemWorkSM *)&MenuWork)->state != ((MenuSystemWorkSM *)&MenuWork)->nextState) {
        ((MenuSystemWorkSM *)&MenuWork)->state = ((MenuSystemWorkSM *)&MenuWork)->nextState;
        ((MenuSystemWorkSM *)&MenuWork)->flags |= 1;
    } else {
        ((MenuSystemWorkSM *)&MenuWork)->flags &= 0xfe;
    }
    if (((MenuSystemWorkSM *)&MenuWork)->inputDelay != 0)
        ((MenuSystemWorkSM *)&MenuWork)->inputDelay--;

    switch (((MenuSystemWorkSM *)&MenuWork)->state) {
    case 0x10:
        if (((MenuSystemWorkSM *)&MenuWork)->inputDelay == 0) {
            signed char previousSelection = ((MenuSystemWorkSM *)&MenuWork)->messageIndex;
            int selection = MenuSelectMove(previousSelection, 7, 0);
            ((MenuSystemWorkSM *)&MenuWork)->messageIndex = selection;
            if (((MenuSystemWorkSM *)&MenuWork)->messageIndex == 5)
                ((MenuSystemWorkSM *)&MenuWork)->messageIndex = selection + (selection - previousSelection);
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x20) {
                __builtin_memcpy(optionStates, D_004DB578, sizeof(optionStates));
                if (((MenuSystemWorkSM *)&MenuWork)->messageIndex == 4) {
                    xglSoundEffectNormalID(1, 0);
                    MenuModelMemorySet(0);
                    xglSleep();
                    tyaDisplaySetting(MenuModelWorkTop);
                } else if (((MenuSystemWorkSM *)&MenuWork)->messageIndex == 5 && ((MenuSystemWorkSM *)&MenuWork)->hddMounted == 0) {
                    xglSoundEffectNormalID(5, 0);
                } else if (optionStates[((MenuSystemWorkSM *)&MenuWork)->messageIndex] != 0) {
                    ((MenuSystemWorkSM *)&MenuWork)->nextState = optionStates[((MenuSystemWorkSM *)&MenuWork)->messageIndex];
                    ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                    xglSoundEffectNormalID(1, 0);
                }
            }
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x40) {
                ((MenuSystemWorkSM *)&MenuWork)->nextState = 0xf0;
                xglSoundEffectNormalID(2, 0);
            }
        }
        break;
    case 0x20:
        if ((unsigned char)(((MenuSystemWorkSM *)&MenuWork)->flags & 1))
            ((MenuSystemWorkSM *)&MenuWork)->choiceIndex = 0;
        if (((MenuSystemWorkSM *)&MenuWork)->inputDelay != 0)
            break;
        {
            ((MenuSystemWorkSM *)&MenuWork)->choiceIndex = MenuSelectMove(((MenuSystemWorkSM *)&MenuWork)->choiceIndex, 2, 0);
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x20) {
                __builtin_memcpy(outputModes, D_004DB580, sizeof(outputModes));
                SsdSetOutputMode(outputModes[((MenuSystemWorkSM *)&MenuWork)->choiceIndex]);
                SaveData[0x28] = outputModes[((MenuSystemWorkSM *)&MenuWork)->choiceIndex];
                ((MenuSystemWorkSM *)&MenuWork)->nextState = 0x10;
                ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                ((MenuSystemWorkSM *)&MenuWork)->soundOutput = SaveData[0x28] ^ 1;
                xglSoundEffectNormalID(1, 0);
            }
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x40) {
                ((MenuSystemWorkSM *)&MenuWork)->nextState = 0x10;
                ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                xglSoundEffectNormalID(2, 0);
            }
        }
        break;
    case 0x30:
        if ((unsigned char)(((MenuSystemWorkSM *)&MenuWork)->flags & 1))
            ((MenuSystemWorkSM *)&MenuWork)->choiceIndex = 0;
        if (((MenuSystemWorkSM *)&MenuWork)->inputDelay != 0)
            break;
        {
            ((MenuSystemWorkSM *)&MenuWork)->choiceIndex = MenuSelectMove(((MenuSystemWorkSM *)&MenuWork)->choiceIndex, 2, 0);
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x20) {
                MenuSystemPartyDataPrefix *party;
                __builtin_memcpy(displayModes, D_004DB588, sizeof(displayModes));
                PartyRadarDispSet(displayModes[((MenuSystemWorkSM *)&MenuWork)->choiceIndex]);
                party = (MenuSystemPartyDataPrefix *)PartyDataGet();
                ((MenuSystemWorkSM *)&MenuWork)->displayMode = party->radar_disp ^ 1;
                ((MenuSystemWorkSM *)&MenuWork)->nextState = 0x10;
                ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                xglSoundEffectNormalID(1, 0);
            }
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x40) {
                ((MenuSystemWorkSM *)&MenuWork)->nextState = 0x10;
                ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                xglSoundEffectNormalID(2, 0);
            }
        }
        break;
    case 0x40:
        if ((unsigned char)(((MenuSystemWorkSM *)&MenuWork)->flags & 1))
            ((MenuSystemWorkSM *)&MenuWork)->choiceIndex = 0;
        if (((MenuSystemWorkSM *)&MenuWork)->inputDelay != 0)
            break;
        {
            ((MenuSystemWorkSM *)&MenuWork)->choiceIndex = MenuSelectMove(((MenuSystemWorkSM *)&MenuWork)->choiceIndex, 2, 0);
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x20) {
                __builtin_memcpy(vibrationModes, D_004DB590, sizeof(vibrationModes));
                ((MenuSystemPadSM *)&PadData)->vibrationReset = vibrationModes[((MenuSystemWorkSM *)&MenuWork)->choiceIndex];
                SaveData[0x3c] = vibrationModes[((MenuSystemWorkSM *)&MenuWork)->choiceIndex];
                ((MenuSystemPadSM *)&PadData)->vibrationStatus = vibrationModes[((MenuSystemWorkSM *)&MenuWork)->choiceIndex];
                ((MenuSystemWorkSM *)&MenuWork)->radarMode = vibrationModes[((MenuSystemWorkSM *)&MenuWork)->choiceIndex];
                if (vibrationModes[((MenuSystemWorkSM *)&MenuWork)->choiceIndex] == 0)
                    MenuBibrationSet(0, 1, 128, 32);
                ((MenuSystemWorkSM *)&MenuWork)->nextState = 0x10;
                ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                xglSoundEffectNormalID(1, 0);
            }
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x40) {
                ((MenuSystemWorkSM *)&MenuWork)->nextState = 0x10;
                ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                xglSoundEffectNormalID(2, 0);
            }
        }
        break;
    case 0x50:
        if ((unsigned char)(((MenuSystemWorkSM *)&MenuWork)->flags & 1))
            ((MenuSystemWorkSM *)&MenuWork)->choiceIndex = 0;
        if (((MenuSystemWorkSM *)&MenuWork)->inputDelay != 0)
            break;
        {
            ((MenuSystemWorkSM *)&MenuWork)->choiceIndex = MenuSelectMove(((MenuSystemWorkSM *)&MenuWork)->choiceIndex, 2, 0);
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x20) {
                __builtin_memcpy(padModes, D_004DB598, sizeof(padModes));
                SaveData[0x44] = padModes[((MenuSystemWorkSM *)&MenuWork)->choiceIndex];
                ((MenuSystemWorkSM *)&MenuWork)->padMode = padModes[((MenuSystemWorkSM *)&MenuWork)->choiceIndex];
                ((MenuSystemWorkSM *)&MenuWork)->nextState = 0x10;
                ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                xglSoundEffectNormalID(1, 0);
            }
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x40) {
                ((MenuSystemWorkSM *)&MenuWork)->nextState = 0x10;
                ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                xglSoundEffectNormalID(2, 0);
            }
        }
        break;
    case 0x60:
        if ((unsigned char)(((MenuSystemWorkSM *)&MenuWork)->flags & 1))
            ((MenuSystemWorkSM *)&MenuWork)->choiceIndex = 0;
        if (((MenuSystemWorkSM *)&MenuWork)->inputDelay != 0)
            break;
        {
            ((MenuSystemWorkSM *)&MenuWork)->choiceIndex = MenuSelectMove(((MenuSystemWorkSM *)&MenuWork)->choiceIndex, 2, 0);
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x20) {
                if (((MenuSystemWorkSM *)&MenuWork)->choiceIndex == 0) {
                    MenuSystemInitSet();
                    ((MenuSystemWorkSM *)&MenuWork)->nextState = 0x10;
                    ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                    xglSoundEffectNormalID(1, 0);
                } else {
                    ((MenuSystemWorkSM *)&MenuWork)->nextState = 0x10;
                    ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                    xglSoundEffectNormalID(2, 0);
                }
            }
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x40) {
                ((MenuSystemWorkSM *)&MenuWork)->nextState = 0x10;
                ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                xglSoundEffectNormalID(2, 0);
            }
        }
        break;
    case 0x70:
        if ((unsigned char)(((MenuSystemWorkSM *)&MenuWork)->flags & 1))
            ((MenuSystemWorkSM *)&MenuWork)->choiceIndex = 0;
        if (((MenuSystemWorkSM *)&MenuWork)->inputDelay != 0)
            break;
        {
            ((MenuSystemWorkSM *)&MenuWork)->choiceIndex = MenuSelectMove(((MenuSystemWorkSM *)&MenuWork)->choiceIndex, 2, 0);
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x20) {
                __builtin_memcpy(hddModes, D_004DB5A0, sizeof(hddModes));
                SaveData[0x40] = hddModes[((MenuSystemWorkSM *)&MenuWork)->choiceIndex] ^ 1;
                xglHddActivate(hddModes[((MenuSystemWorkSM *)&MenuWork)->choiceIndex]);
                ((MenuSystemWorkSM *)&MenuWork)->hddActive = SaveData[0x40];
                ((MenuSystemWorkSM *)&MenuWork)->nextState = 0x10;
                ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                xglSoundEffectNormalID(1, 0);
            }
            if (((MenuSystemPadSM *)&PadData)->pressed & 0x40) {
                ((MenuSystemWorkSM *)&MenuWork)->nextState = 0x10;
                ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 8;
                xglSoundEffectNormalID(2, 0);
            }
        }
        break;
    case 0xf0:
        if ((unsigned char)(((MenuSystemWorkSM *)&MenuWork)->flags & 1))
            ((MenuSystemWorkSM *)&MenuWork)->inputDelay = 16;
        if (((MenuSystemWorkSM *)&MenuWork)->inputDelay == 0)
            ((MenuSystemWorkSM *)&MenuWork)->nextState = 0xff;
        break;
    }
    MenuSystemPasMain();
    MenuSystemInfoMain();
    MenuSystemMenuMain();
    xglFontDebugPrintf(0, 64, D_004DB5A8);
    xglFontDebugPrintf(0, 80, D_004DB5B0, ((MenuSystemWorkSM *)&MenuWork)->messageIndex);
}

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

/* Types recovered upstream (origin/master); kept for reference. */
typedef union MenuSystemMessageStorage {
    MenuSystemMessageTable table;
    unsigned int words[8];
} MenuSystemMessageStorage;


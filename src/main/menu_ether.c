#include "common.h"

extern unsigned char *MenuEtherDataBuf;

void *MenuEtherDataGet(int id)
{
    int index = (unsigned short)id;
    unsigned char *buf = MenuEtherDataBuf;

    if ((unsigned int)(index - 1) >= 0x50U)
        return 0;
    return buf + (index << 2) - 4;
}

int MenuEtherJoutoCheck(int id)
{
    unsigned int index = (unsigned short)id;
    unsigned int flags;

    if ((index - 1) >= 0x50u)
        return 0;
    flags = ((unsigned short *)func_A1A488(index))[1] & 0x4000;
    return flags == 0;
}

extern int *func_A19210(unsigned short chrNo);

int MenuEtherCharPointCheck(unsigned short charId, unsigned short etherId)
{
    int pts = func_A19210(charId)[4];
    if (pts < ((short *)func_A1A488(etherId))[4]) {
        return 0;
    }
    return 1;
}

int MenuEtherWhoCheck(int id)
{
    unsigned int index = (unsigned short)id;

    if ((index - 1) < 0x10u)
        return 3;
    if ((index - 0x11) < 0xAu)
        return 2;
    if ((index - 0x1B) < 0xAu)
        return 1;
    if ((index - 0x25) < 0xCu)
        return 6;
    if ((index - 0x31) < 0xAu)
        return 7;
    if ((index - 0x3B) < 0xDu)
        return 4;
    if ((index - 0x48) < 0x8u)
        return 5;
    return 0;
}

extern char *func_A191C0(unsigned short);

int MenuEtherSetCheck(unsigned short charId, unsigned short etherId)
{
    short *p = (short *)(func_A191C0(charId) + 0x8E);
    int i;
    for (i = 0; i < 12; i++) {
        if (*p++ == etherId) {
            return 1;
        }
    }
    return 0;
}

/*
 * The retail ELF names these ov01 data-table accessors (still assembler in
 * their own TU, src/ov01/data_unit_org_get.c) dataEthGet (0x00a1a488) and
 * dataTecGet (0x00a1a378); until that TU recovers them, the synthetic
 * linker script only binds their scaffold func_<VA> spelling.
 */
typedef struct EtherStatus {
    unsigned char unmodeled_00[2];
    short flags; /* +2; tested as the 0x4000 acquisition bit */
    short capCost;        /* +4 */
    short tecId;          /* +6 */
    short pointCost;      /* +8 */
} EtherStatus;

typedef struct TecStatus {
    unsigned char unmodeled_00;
    signed char epCost;   /* +1 */
    unsigned char unmodeled_02[2];
    signed char type;     /* +4 */
} TecStatus;

extern EtherStatus *func_A1A488(unsigned short etherId); /* dataEthGet */
extern TecStatus *func_A1A378(short tecId);               /* dataTecGet */

signed char MenuEtherTypeGet(int etherId) {
    return func_A1A378(func_A1A488(etherId)->tecId)->type;
}

int MenuEtherUseCheck(unsigned short id)
{
    switch (id) {
    case 1:
    case 8:
    case 0x25:
        return 1;
    case 4:
    case 0x1C:
        return 2;
    case 0x4A:
        return 3;
    }
    return 0;
}

extern char *ld191c0(int) __asm__("func_A191C0");
extern EtherStatus *ld1a488(int) __asm__("func_A1A488");

void MenuEtherCapSet(void)
{
    int i;

    for (i = 1; i < 8; i++) {
        int j;
        char *org = ld191c0(i);
        short *p = (short *)(org + 0x8E);

        org[0x17] = 0;
        j = 0;
        __asm__(".align 3");
    lp:
        if (j >= 12)
            goto end;
        j++;
        {
            short id = *p++;
            if (id == 0)
                goto end;
            org[0x17] = org[0x17] + ld1a488(id)->unmodeled_00[4];
        }
        goto lp;
    end:;
    }
}

typedef struct EtherPasWindow {
    short x; /* +0x00 */
    short y; /* +0x02 */
    int color; /* +0x04 */
    short width; /* +0x08 */
    short height; /* +0x0A */
    unsigned char unmodeled_0c[0x10 - 0x0C];
    signed char state; /* +0x10 */
    unsigned char unmodeled_11[0x14 - 0x11];
    void (*callback)(void); /* +0x14 */
    void *callbackArg; /* +0x18 */
    unsigned char unmodeled_1c[0x194 - 0x1C];
} EtherPasWindow;
typedef struct EtherPasMessage {
    unsigned char unmodeled_00;
    unsigned char mode; /* +0x01 */
    unsigned char unmodeled_02[0x04 - 0x02];
    short x; /* +0x04 */
    short y; /* +0x06 */
    int color; /* +0x08 */
    unsigned char unmodeled_0c[0x10 - 0x0C];
    signed char glyphColor[4]; /* +0x10; first three bytes are RGB */
    unsigned char unmodeled_14[0x18 - 0x14];
    const char *text; /* +0x18 */
    unsigned char unmodeled_1c[0x44 - 0x1C];
} EtherPasMessage;
typedef struct EtherPasBox {
    short x; /* +0x00 */
    short y; /* +0x02 */
    int kind; /* +0x04 */
    short width; /* +0x08 */
    short height; /* +0x0A */
} EtherPasBox;
typedef struct EtherPasWork {
    unsigned char state; /* +0x000 */
    unsigned char unmodeled_01[3];
    int kind; /* +0x004 */
    EtherPasWindow window; /* +0x008 */
    EtherPasMessage messages[6]; /* +0x19C */
    EtherPasBox box; /* +0x334 */
    unsigned char callbackWork[1]; /* +0x340 */
} EtherPasWork;
typedef struct MenuEtherPasWorkView {
    unsigned char unmodeled_00[0x03];
    unsigned char state; /* +0x03 */
    unsigned char unmodeled_04[0x30 - 0x04];
    signed char commandCursor; /* +0x30 */
    signed char menuCursor; /* +0x31 */
} MenuEtherPasWorkView;
extern EtherPasWork *MenuEtherPas;
extern MenuEtherPasWorkView MenuWorkPM __asm__("MenuWork");
extern const char *msg_0_0036DC00[6];
extern void MoveSlide(short *current, short *target, float rate);
extern void WindowDXSet(void *window);
extern void WindowDXMain(void *window);
extern void MenuPasWindow(void);
extern int MenuPasLengthGet(const char *text);
extern void endPrintExtFunc(int kind, int id, void *data);
extern void eMessageSet(void *message, const char *text);
extern void eMessageMain(void *message);

void MenuEtherPasMain(void) {
    EtherPasWork *pas = MenuEtherPas;
    int tabLength[6];
    short tabTarget[6];
    short slideTarget;
    int i;

    for (i = 0; i < 6; i++) {
        tabLength[i] = MenuPasLengthGet(msg_0_0036DC00[i]);
    }

    switch (pas->state) {
    case 0:
        pas->kind = 0x00FFFFF0;
        WindowDXSet(&pas->window);
        pas->window.x = -288;
        pas->window.y = 8;
        pas->window.color = pas->kind;
        pas->window.width = 272;
        pas->window.height = 30;
        pas->window.callback = MenuPasWindow;
        pas->window.callbackArg = pas->callbackWork;
        pas->window.state = 1;
        WindowDXMain(&pas->window);
        pas->window.state = 3;
        for (i = 0; i < 6; i++) {
            eMessageSet(&pas->messages[i], msg_0_0036DC00[i]);
            pas->messages[i].mode = 0x20;
            pas->messages[i].x = 288;
            pas->messages[i].y = 11;
            pas->messages[i].color = pas->kind + 2;
        }
        pas->state = 2;
        break;
    case 2:
        break;
    default:
        return;
    }

    slideTarget = -16;
    for (i = 0; i < 6; i++) {
        tabTarget[i] = 288;
    }

    switch (MenuWorkPM.state) {
    case 16:
    case 32:
        tabTarget[0] = 16;
        break;
    case 48:
    case 50:
    case 80:
    case 112:
    case 114:
        tabTarget[0] = 16;
        tabTarget[MenuWorkPM.commandCursor + 1] = tabLength[0] + 16;
        break;
    case 116:
    case 118:
    case 120:
    case 128:
    case 130:
    case 132:
    case 134:
        tabTarget[0] = -(tabLength[0] + 16);
        tabTarget[MenuWorkPM.commandCursor + 1] = 16;
        tabTarget[MenuWorkPM.menuCursor + 4] = tabLength[MenuWorkPM.commandCursor + 1] + 16;
        break;
    default:
        slideTarget = -288;
        break;
    }

    MoveSlide(&pas->window.x, &slideTarget, 3.0f);
    WindowDXMain(&pas->window);
    if (pas->window.state == 3) {
        pas->box.x = pas->window.x + 3;
        pas->box.y = pas->window.y + 3;
        pas->box.kind = pas->kind;
        pas->box.width = pas->window.width - 6;
        pas->box.height = pas->window.height - 6;
        endPrintExtFunc(pas->kind, 101, &pas->box);
        for (i = 0; i < 6; i++) {
            MoveSlide(&pas->messages[i].x, &tabTarget[i], 5.0f);
            if (pas->messages[i].x < 256) {
                eMessageMain(&pas->messages[i]);
            }
        }
        endPrintExtFunc(pas->kind, 102, 0);
    }
}

typedef struct {
    int name;
    int value;
    unsigned char unk8;
    unsigned char unk9;
    unsigned char pad[2];
} MenuEtherRow;

extern MenuEtherRow *MenuListGet(int list);

extern struct { char pad[0x40]; signed char unk40; } MenuWork;

extern int *MenuTextGet(int index);

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct MenuEtherWork {
    unsigned char unmodeled_00[0x03];
    unsigned char state;            /* +0x03 */
    unsigned char unmodeled_04[0x10 - 0x04];
    signed char charDenied;         /* +0x10 */
    unsigned char unmodeled_11[0x20 - 0x11];
    unsigned char flags;            /* +0x20; bit 0 rebuilds ether windows */
    unsigned char nextState;        /* +0x21 */
    unsigned char unmodeled_22[0x2A - 0x22];
    unsigned char wait;             /* +0x2A */
    unsigned char unmodeled_2b[0x30 - 0x2B];
    signed char commandCursor;      /* +0x30 */
    signed char menuCursor;         /* +0x31 */
    signed char slotCursor;         /* +0x32 */
    signed char slotCount;          /* +0x33 */
    signed char confirmCursor;      /* +0x34 */
    unsigned char unmodeled_35[0x40 - 0x35];
    signed char chrNo;              /* +0x40 */
    signed char partyCount;         /* +0x41 */
    signed char partyCursor;        /* +0x42 */
    signed char targetCursor;       /* +0x43 */
    signed char etherId;            /* +0x44 */
    signed char slotEther;          /* +0x45 */
    signed char targetChrNo;        /* +0x46 */
    signed char useType;            /* +0x47 */
    signed char targetCount;        /* +0x48 */
    unsigned char unmodeled_49[0x50 - 0x49];
    int listSelect;                 /* +0x50 */
    unsigned char unmodeled_54[0x60 - 0x54];
    unsigned char targetList[8];    /* +0x60 */
} MenuEtherWork;

typedef struct EtherListRow {
    const char *name;   /* +0x00 */
    int amount;         /* +0x04 */
    signed char flag;   /* +0x08 */
    signed char status; /* +0x09 */
    unsigned char unmodeled_0a[2];
} EtherListRow;

typedef struct MenuEtherPadData {
    unsigned char unmodeled_00[0x2A];
    unsigned short pressed; /* +0x2A */
    unsigned char unmodeled_2c[0x34 - 0x2C];
    unsigned short repeat;  /* +0x34 */
} MenuEtherPadData;

typedef struct EtherTextEntry {
    const char *name;
} EtherTextEntry;

typedef struct EtherScreenWork {
    unsigned char state;
} EtherScreenWork;

extern EtherScreenWork *MenuEtherInfo;

typedef struct MenuTextEntryL2 { const char *name; const unsigned char *auxiliary; const unsigned char *sortName; } MenuTextEntryL2;

extern MenuTextEntryL2 *func_A2C5A8(unsigned int etherId);

typedef struct EtherCharRecordIM { unsigned char unmodeled_00[0x10]; int points; } EtherCharRecordIM;
typedef struct MenuTextEntryIM {
    const char *name;
    const unsigned char *auxiliary;
    const unsigned char *sortName;
} MenuTextEntryIM;
typedef struct EtherStatusIM {
    unsigned char unmodeled_00[2];
    short flags;
    short capCost;
    short tecId;
    short pointCost;
} EtherStatusIM;
typedef struct MenuEtherPadDataIM {
    unsigned char unmodeled_00[0x2A];
    unsigned short pressed;
    unsigned char unmodeled_2c[0x34 - 0x2C];
    unsigned short repeat;
} MenuEtherPadDataIM;
typedef struct MenuEtherWorkIM {
    unsigned char unmodeled_00[0x03];
    unsigned char state;            /* +0x03 */
    unsigned char unmodeled_04[0x10 - 0x04];
    signed char charDenied;         /* +0x10 */
    unsigned char unmodeled_11[0x20 - 0x11];
    unsigned char flags;            /* +0x20; bit 0 rebuilds ether windows */
    unsigned char nextState;        /* +0x21 */
    unsigned char unmodeled_22[0x2A - 0x22];
    unsigned char wait;             /* +0x2A */
    unsigned char unmodeled_2b[0x30 - 0x2B];
    signed char commandCursor;      /* +0x30 */
    signed char menuCursor;         /* +0x31 */
    signed char slotCursor;         /* +0x32 */
    signed char slotCount;          /* +0x33 */
    signed char confirmCursor;      /* +0x34 */
    unsigned char unmodeled_35[0x40 - 0x35];
    signed char chrNo;              /* +0x40 */
    signed char partyCount;         /* +0x41 */
    signed char partyCursor;        /* +0x42 */
    signed char targetCursor;       /* +0x43 */
    signed char etherId;            /* +0x44 */
    signed char slotEther;          /* +0x45 */
    signed char targetChrNo;        /* +0x46 */
    signed char useType;            /* +0x47 */
    signed char targetCount;        /* +0x48 */
    unsigned char unmodeled_49[0x50 - 0x49];
    union {
        int listSelect;             /* +0x50 */
        unsigned char listSelectBytes[4];
    };
    unsigned char unmodeled_54[0x60 - 0x54];
    unsigned char targetList[8];    /* +0x60 */
} MenuEtherWorkIM;
extern int MenuSortGet(int list, int index);
extern int func_A19578(int chrNo, int etherId);
extern short MenuEtherParty[];
int MenuInfoWindow();
typedef struct EtherInfoCharacterHelp { const char *text[2]; } EtherInfoCharacterHelp;
extern EtherInfoCharacterHelp D_004DB278[];
extern char D_004DB288[];
extern char D_004DB288[8];
extern char D_004DB298[];
extern char D_004DB2A0[];
extern char D_004DB2A8[];
extern char D_004DB2B0[];
extern char D_004DB2B8[];
typedef struct EtherInfoCommandHelp {
    const char *text[5];
} EtherInfoCommandHelp;
typedef struct EtherInfoActionHelp {
    const char *text[3];
} EtherInfoActionHelp;
typedef struct EtherInfoAcquireHelp {
    const char *text[9];
} EtherInfoAcquireHelp;
typedef struct EtherInfoTransferHelp {
    const char *text[7];
} EtherInfoTransferHelp;
typedef struct EtherInfoMessage {
    short x;
    short y;
    unsigned char unmodeled_04[4];
    int visible;
    char *text;
    unsigned char unmodeled_10[0x1B0 - 0x10];
} EtherInfoMessage;
typedef struct EtherInfoWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    const char *title;
    unsigned char state;
    unsigned char enabled;
    unsigned char unmodeled_12[2];
    void (*callback)(struct WindowDX *, void *);
    void *callbackArg;
    unsigned char unmodeled_1c[0x194 - 0x1C];
} EtherInfoWindow;
typedef struct EtherInfoWork {
    unsigned char state;
    unsigned char active;
    signed char previous;
    unsigned char helpHeld;
    int work;
    EtherInfoWindow window;
    unsigned char unmodeled_19c[4];
    EtherInfoMessage message;
    char text[256];
} EtherInfoWork;
extern const EtherInfoCommandHelp D_004C7DC8;
extern const EtherInfoActionHelp D_004C7E50;
extern const EtherInfoAcquireHelp D_004C7F98;
extern const EtherInfoTransferHelp D_004C8040;
extern const char D_004C8060[];
extern const char D_004C80A0[];
extern char *MenuCharNameGet(unsigned int charId);
extern void eMessageCpy(char *message, const char *text);
extern void eMessageCat(const char *text);
static inline EtherInfoWork *EtherInfoWorkGetIM(EtherScreenWork **slot) {
    return (void *)*slot;
}

void MenuEtherInfoMain(void) {
    extern MenuEtherWorkIM MenuWork_IM __asm__("MenuWork");
    extern MenuEtherPadDataIM PadData_IM __asm__("PadData");
    EtherInfoWork *screen = EtherInfoWorkGetIM(&MenuEtherInfo);
    const char *capHelp = D_004C8060;
    EtherInfoCharacterHelp characterHelp = D_004DB278[0];
    EtherInfoCommandHelp commandHelp = D_004C7DC8;
    EtherInfoActionHelp actionHelp = D_004C7E50;
    EtherInfoAcquireHelp acquireHelp = D_004C7F98;
    EtherInfoTransferHelp transferHelp = D_004C8040;
    short targetY;
    EtherInfoWindow *window;
    char *text;


    switch (screen->state) {
    case 0:
        screen->work = 0x00FFFFF0;
        window = &screen->window;
        text = screen->text;
        WindowDXSet((window));
        screen->window.x = -16;
        screen->window.y = 480;
        screen->window.color = screen->work;
        screen->window.width = 544;
        screen->window.height = 54;
        screen->window.callback =
            (void (*)(struct WindowDX *, void *))MenuInfoWindow;
        {
            EtherInfoMessage *message = &screen->message;
            message->x = 0;
            screen->window.callbackArg = message;
        }
        screen->message.y = 0;
        __builtin_memset(&screen->message.text, 0, sizeof(screen->message.text));
        screen->window.state = 1;
        WindowDXMain((window));
        screen->window.state = 3;
        screen->previous = -1;
        screen->active = 1;
        screen->helpHeld = 0;
        memset(text, 0, 256);
        screen->state = 2;
        break;
    case 2:
        window = &screen->window;
        text = screen->text;
        break;
    default:
        return;
    }

    targetY = 386;
    if ((unsigned char)(MenuWork_IM.flags & 1)) {
        screen->helpHeld = 0;
    }
    if (screen->helpHeld == 0) {
        if (PadData_IM.pressed & 0x20) {
            screen->helpHeld = 1;
        }
    } else if (PadData_IM.repeat != 0) {
        screen->helpHeld = 0;
    }

    switch (MenuWork_IM.state) {
    case 16:
        eMessageCpy(text, characterHelp.text[0]);
        if (screen->helpHeld != 0 && MenuWork_IM.charDenied != 0) {
            eMessageCpy(text, MenuCharNameGet(MenuWork_IM.chrNo));
            eMessageCat(characterHelp.text[1]);
        }
        break;
    case 32:
        if (MenuWork_IM.commandCursor == 0) {
            eMessageCpy(text, commandHelp.text[0]);
            eMessageCat(((MenuTextEntryIM *(*)(int))MenuTextGet)((MenuWork_IM.chrNo & 0xFFFF) + 0x60000)->name);
            eMessageCat(commandHelp.text[4]);
        } else {
            eMessageCpy(text, commandHelp.text[MenuWork_IM.commandCursor]);
        }
        break;
    case 48:
        if (MenuWork_IM.listSelect < 0) {
            eMessageCpy(text, D_004DB298);
        } else {
            eMessageCpy(text, ((MenuTextEntryIM *(*)(int))MenuTextGet)(MenuSortGet(0, MenuWork_IM.listSelect))->auxiliary);
            if (screen->helpHeld == 1) {
                int status = ((EtherListRow *(*)(int))MenuListGet)(0)[MenuWork_IM.listSelect].status;
                if (status == 1) {
                    eMessageCpy(text, acquireHelp.text[5]);
                } else if (status == 2) {
                    eMessageCpy(text, acquireHelp.text[7]);
                }
            }
        }
        break;
    case 50:
        if (MenuWork_IM.listSelect < 0) {
            eMessageCpy(text, D_004DB298);
        } else {
            eMessageCpy(text, ((MenuTextEntryIM *(*)(int))MenuTextGet)(MenuSortGet(0, MenuWork_IM.listSelect))->auxiliary);
        }
        break;
    case 80:
        if (MenuWork_IM.listSelect < 0) {
            eMessageCpy(text, D_004DB298);
        } else {
            eMessageCpy(text, ((MenuTextEntryIM *(*)(int))MenuTextGet)(MenuSortGet(0, MenuWork_IM.listSelect))->auxiliary);
            if (screen->helpHeld == 1) {
                EtherListRow *rows = ((EtherListRow *(*)(int))MenuListGet)(0);
                if (rows[MenuWork_IM.listSelect].flag == 1 &&
                    rows[MenuWork_IM.listSelect].status == 0) {
                    eMessageCpy(text, capHelp);
                }
            }
        }
        break;
    case 112:
        if (MenuWork_IM.listSelect >= 0) {
            eMessageCpy(text, ((MenuTextEntryIM *(*)(int))MenuTextGet)(MenuSortGet(0, MenuWork_IM.listSelect))->auxiliary);
            if (screen->helpHeld == 1 &&
                MenuWork_IM.chrNo != MenuEtherWhoCheck(MenuWork_IM.etherId) &&
                ((EtherListRow *(*)(int))MenuListGet)(0)[MenuWork_IM.listSelect].flag == 1) {
                eMessageCpy(text, acquireHelp.text[6]);
            }
        } else {
            eMessageCpy(text, D_004DB298);
        }
        break;
    case 114:
        eMessageCpy(text, actionHelp.text[MenuWork_IM.menuCursor]);
        if (screen->helpHeld == 1) {
            short cost = ((EtherStatusIM *(*)(int))func_A1A488)(MenuWork_IM.etherId)->pointCost;
            int points = ((EtherCharRecordIM *(*)(int))func_A19210)(MenuWork_IM.chrNo)->points;
            if (MenuWork_IM.menuCursor == 1) {
                cost /= 2;
            }
            if (cost < points) {
                if (MenuEtherJoutoCheck(MenuWork_IM.etherId) == 0) {
                    if (MenuWork_IM.menuCursor == 0) {
                        eMessageCpy(text, acquireHelp.text[3]);
                    } else {
                        eMessageCpy(text, acquireHelp.text[8]);
                    }
                } else if (MenuWork_IM.slotCount == 0) {
                    if (MenuWork_IM.menuCursor == 0) {
                        eMessageCpy(text, acquireHelp.text[3]);
                    } else {
                        eMessageCpy(text, acquireHelp.text[8]);
                    }
                }
            } else {
                eMessageCpy(text, acquireHelp.text[2]);
            }
        }
        break;
    case 116:
        eMessageCpy(text, acquireHelp.text[0]);
        eMessageCat(func_A2C5A8(MenuWork_IM.slotEther)->name);
        eMessageCat(D_004DB2A0);
        eMessageCat(func_A2C5A8(MenuWork_IM.slotEther)->auxiliary);
        if (screen->helpHeld == 1 && func_A19578(MenuWork_IM.chrNo, MenuWork_IM.slotEther) != 0) {
            eMessageCpy(text, acquireHelp.text[4]);
            eMessageCat(func_A2C5A8(MenuWork_IM.slotEther)->name);
            eMessageCat(D_004DB288);
        }
        break;
    case 118:
    case 120:
        if (MenuWork_IM.state == 118) {
            eMessageCpy(text, acquireHelp.text[0]);
        } else {
            eMessageCpy(text, acquireHelp.text[1]);
        }
        eMessageCat(func_A2C5A8(MenuWork_IM.slotEther)->name);
        if (MenuWork_IM.state == 118) {
            eMessageCat(D_004DB288);
        } else {
            eMessageCat(D_004DB2A8);
        }
        break;
    case 130:
        eMessageCpy(text, transferHelp.text[0]);
        eMessageCat(func_A2C5A8(MenuWork_IM.etherId)->name);
        eMessageCat(D_004DB288);
        if (screen->helpHeld == 1 && func_A19578(MenuWork_IM.targetChrNo, MenuWork_IM.etherId) != 0) {
            eMessageCpy(text, transferHelp.text[5]);
            eMessageCat(func_A2C5A8(MenuWork_IM.etherId)->name);
            eMessageCat(D_004DB288);
        }
        break;
    case 132:
    case 134:
        {
            short chrNo = MenuEtherParty[MenuWork_IM.targetList[MenuWork_IM.targetCursor]];
            eMessageCpy(text, transferHelp.text[1]);
            if (MenuWork_IM.state == 132) {
                eMessageCat(D_004DB298);
            } else {
                eMessageCat(D_004DB2B0);
            }
            eMessageCat(func_A2C5A8(MenuWork_IM.etherId)->name);
            if (MenuWork_IM.state == 132) {
                eMessageCat(transferHelp.text[2]);
            } else {
                eMessageCat(transferHelp.text[3]);
            }
            eMessageCat(MenuCharNameGet(chrNo));
            if (MenuWork_IM.state == 132) {
                eMessageCat(D_004DB2B8);
            } else {
                eMessageCat(D_004C80A0);
            }
        }
        break;
    default:
        targetY = 512;
        break;
    }
    screen->message.text = text;
    MoveSlide(&screen->window.y, &targetY, 3.0f);
    WindowDXMain((window));
}

typedef struct EtherMenuWindowDX {
    short x; /* +0x00 */
    short y; /* +0x02 */
    int color; /* +0x04 */
    short width; /* +0x08 */
    short height; /* +0x0A */
    const char *title; /* +0x0C */
    unsigned char state; /* +0x10: 1 while the frame opens, 3 once it is up */
    unsigned char unmodeled_11[0x14 - 0x11];
    void *select; /* +0x14 */
    void *selectArg; /* +0x18 */
    unsigned char unmodeled_1c[0x9C - 0x1C];
    const char *renderTitle; /* +0x9C; active title after initialization */
    unsigned char unmodeled_a0[0x194 - 0xA0];
} EtherMenuWindowDX;
typedef struct EtherMenuList {
    unsigned char unmodeled_00[0x02];
    unsigned short hasPrompt; /* +0x02 */
    int cursor; /* +0x04 */
    const char *items; /* +0x08 */
    const char *prompt; /* +0x0C */
    unsigned char unmodeled_10[0x5F8 - 0x10];
} EtherMenuList;
typedef struct EtherMenuScreen {
    unsigned char state; /* +0x000 */
    unsigned char unmodeled_01[3];
    int color; /* +0x004 */
    EtherMenuWindowDX window[2]; /* +0x008 */
    EtherMenuList list[2]; /* +0x330 */
} EtherMenuScreen;
typedef struct MenuEtherMenuWorkView {
    unsigned char unmodeled_00[0x03];
    unsigned char state; /* +0x03 */
    unsigned char unmodeled_04[0x20 - 0x04];
    unsigned char flags; /* +0x20 */
    unsigned char unmodeled_21[0x30 - 0x21];
    signed char commandCursor; /* +0x30 */
    signed char menuCursor; /* +0x31 */
    unsigned char unmodeled_32[0x34 - 0x32];
    signed char confirmCursor; /* +0x34 */
} MenuEtherMenuWorkView;
extern EtherMenuScreen *MenuEtherMenu;
extern MenuEtherMenuWorkView MenuWorkMM __asm__("MenuWork");
extern char D_004DB2C8[];
extern char D_004DB2D0[];
extern char *msg00_1_0036DC18[];
extern char *msg01_2_0036DC20[];
extern void MenuSelectWindow();
extern void MoveSlide(short *current, short *target, float rate);
extern void WindowDXSet(void *window);
extern void WindowDXMain(void *window);

void MenuEtherMenuMain(void) {
    EtherMenuScreen *work = MenuEtherMenu;
    short target[2];
    int i;

    switch (work->state) {
    case 0:
        work->color = 0x00FFFFF0;
        WindowDXSet(&work->window[0]);
        work->window[0].x = 528;
        work->window[0].title = D_004DB2C8;
        work->window[0].color = work->color;
        work->window[0].width = 145;
        work->window[0].selectArg = &work->list[0];
        work->list[0].items = msg00_1_0036DC18[0];
        work->window[0].state = 1;
        work->window[0].y = 192;
        work->window[0].height = 102;
        work->window[0].select = MenuSelectWindow;
        work->list[0].cursor = 0;
        work->list[0].hasPrompt = 0;
        WindowDXMain(&work->window[0]);
        work->window[0].state = 3;
        WindowDXSet(&work->window[1]);
        work->window[1].x = 528;
        work->window[1].title = D_004DB2D0;
        work->window[1].y = 192;
        work->window[1].color = work->color;
        work->window[1].width = 169;
        work->list[1].items = msg01_2_0036DC20[0];
        work->list[1].prompt = msg01_2_0036DC20[1];
        work->window[1].height = 102;
        work->window[1].select = MenuSelectWindow;
        work->window[1].selectArg = &work->list[1];
        work->list[1].cursor = 0;
        work->list[1].cursor = 0;
        work->window[1].state = work->list[1].hasPrompt = 1;
        WindowDXMain(&work->window[1]);
        work->window[1].state = 3;
        work->state = 2;
        /* fallthrough */
    case 2:
        target[1] = 528;
        target[0] = 528;
        switch (MenuWorkMM.state) {
        case 32:
            if ((unsigned char)(MenuWorkMM.flags & 1)) {
                work->window[0].y = 192;
                work->window[0].x = 528;
                work->window[0].width = 145;
                work->window[0].height = 102;
                work->list[0].items = msg00_1_0036DC18[0];
                work->list[0].cursor = 0;
            }
            target[0] = 192;
            work->list[0].cursor = MenuWorkMM.commandCursor;
            break;
        case 114:
            if ((unsigned char)(MenuWorkMM.flags & 1)) {
                work->window[0].x = 528;
                work->window[0].y = 288;
                work->window[0].width = 125;
                work->window[0].height = 78;
                work->list[0].items = msg00_1_0036DC18[1];
                work->list[0].cursor = 0;
            }
            target[0] = 131;
            work->list[0].cursor = MenuWorkMM.menuCursor;
            break;
        case 118:
        case 132:
            if ((unsigned char)(MenuWorkMM.flags & 1)) {
                if (MenuWorkMM.state == 118) {
                    work->window[1].y = 128;
                } else {
                    work->window[1].y = 240;
                }
                work->list[1].cursor = 0;
                work->window[1].x = 528;
            }
            target[1] = 285;
            work->list[1].cursor = MenuWorkMM.confirmCursor;
            break;
        }
        for (i = 0; i < 2; i++) {
            MoveSlide(&work->window[i].x, &target[i], 3.0f);
            WindowDXMain(&work->window[i]);
        }
        break;
    }
}

typedef struct EtherUnitRecord {
    unsigned char unmodeled_00[0x17];
    signed char capUsed; /* +0x17 */
    unsigned char unmodeled_18[0x1E];
    short ep; /* +0x36 */
    unsigned char unmodeled_38[0x56];
    short etherId[12]; /* +0x8E */
} EtherUnitRecord;
typedef struct EtherCharRecord {
    unsigned char unmodeled_00[0x10];
    int points; /* +0x10 */
    unsigned char unmodeled_14[0x14];
    unsigned char etherMask[10]; /* +0x28 */
} EtherCharRecord;
typedef struct EtherDetailRibbon {
    short x;
    short y;
    int work;
    short width;
    short height;
    unsigned char unmodeled_0c[0x70 - 0x0C];
} EtherDetailRibbon;
typedef struct EtherDetailFont {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int work;
    unsigned char unmodeled_0c[0x20 - 0x0C];
} EtherDetailFont;
typedef struct EtherDetailNumber {
    short x;
    short y;
    int work;
    unsigned char unmodeled_08[6];
    unsigned char format;
    unsigned char digits;
    unsigned char unmodeled_10[4];
    int value;
    unsigned char unmodeled_18[0x90 - 0x18];
} EtherDetailNumber;
typedef struct EtherDetailRow {
    EtherDetailRibbon ribbon;
    EtherDetailFont font;
    EtherDetailNumber number;
} EtherDetailRow;
typedef struct EtherDetailWork {
    unsigned char state;
    unsigned char unmodeled_01[3];
    int work;
    EtherDetailRow rows[2];
    EtherPasMessage message;
} EtherDetailWork;
extern const char *msg00_3_0036DC28[];
extern char *msg01_4_0036DC30[];
void eRibbonSet(void *ribbon, int mode);
void eRibbonMain(void *ribbon);
void eTagFontSet(void *font, const char *text);
void eTagFontMain(void *font);
void eNumberSet(void *number, int mode);
void eNumberMain(void *number);

void MenuEtherExMain(void)
{
  extern void *MenuEtherExU __asm__("MenuEtherEx");

  extern struct { unsigned char pad[3]; unsigned char state; unsigned char pad4[0x40 - 4]; signed char chrNo; } MenuWorkU __asm__("MenuWork");
  extern EtherCharRecord *ld19210U(int chrNo) __asm__("func_A19210");
  extern EtherUnitRecord *ld191U(int chrNo) __asm__("func_A191C0");
  EtherDetailWork *screen = (void *) MenuEtherExU;
  short rowTargets[2];
  short messageTarget;
  int i;
  int equippedSum;
  EtherUnitRecord *unit;
  char *messageColor;
  switch (screen->state)
  {
    case 0:
      screen->work = 0x00FFFFF0;
      for (i = 0; i < 2; i++)
    {
      eRibbonSet(&screen->rows[i].ribbon, 3);
      screen->rows[i].ribbon.x = -256;
      screen->rows[i].ribbon.work = screen->work;
      screen->rows[i].ribbon.y = 48;
      screen->rows[i].ribbon.width = 256;
      screen->rows[i].ribbon.height = 24;
      eTagFontSet(&screen->rows[i].font, msg00_3_0036DC28[i]);
      screen->rows[i].font.work = screen->work + 2;
      eNumberSet(&screen->rows[i].number, 0);
      screen->rows[i].number.work = screen->work + 2;
    }

      eMessageSet(&screen->message, msg01_4_0036DC30[0]);
      screen->message.mode = 32;
      screen->message.x = 528;
      screen->message.y = 120;
      screen->message.color = screen->work;
      screen->state = 2;

    case 2:
      break;

    default:
      return;

  }

  messageTarget = 528;
  rowTargets[1] = -256;
  rowTargets[0] = -256;
  switch (MenuWorkU.state)
  {
    case 80:
      rowTargets[1] = 0;
      messageTarget = 48;
      break;

    case 32:

    case 112:

    case 114:

    case 116:

    case 118:

    case 120:

    case 128:

    case 130:

    case 132:

    case 134:
      rowTargets[0] = 0;
      break;

  }

  for (i = 0; i < 2; i++)
  {
    /* The row's widget updates use the initialized screen work. */
    EtherDetailWork *renderWork = screen;
    MoveSlide(&screen->rows[i].ribbon.x, &rowTargets[i], 3.0f);
    eRibbonMain(&renderWork->rows[i].ribbon);
    if (i == 0)
    {
      renderWork->rows[i].font.x = (screen->rows[i].ribbon.x + renderWork->rows[i].ribbon.width) - 68;
    }
    else
    {
      screen->rows[i].font.x = (renderWork->rows[i].ribbon.x + screen->rows[i].ribbon.width) - 78;
    }
    renderWork->rows[i].font.y = renderWork->rows[i].ribbon.y + 4;
    eTagFontMain(&screen->rows[i].font);
    screen->rows[i].number.y = screen->rows[i].ribbon.y + 4;
    if (i == 0)
    {
      EtherCharRecord *character = ld19210U(MenuWorkU.chrNo);
      renderWork->rows[i].number.x = (renderWork->rows[i].ribbon.x + renderWork->rows[i].ribbon.width) - 60;
      screen->rows[i].number.digits = 4;
      renderWork->rows[i].number.format = 0;
      screen->rows[i].number.value = character->points;
    }
    else
    {
      unit = ld191U(MenuWorkU.chrNo);
      renderWork->rows[i].number.x = (screen->rows[i].ribbon.x + renderWork->rows[i].ribbon.width) - 48;
      screen->rows[i].number.digits = 2;
      screen->rows[i].number.format = 3;
      screen->rows[i].number.value = 0x000C0000 + unit->capUsed;
    }
    eNumberMain(&renderWork->rows[i].number);
  }

  equippedSum = 0;
  messageColor = &msg01_4_0036DC30[0][1];
  unit = ld191U(MenuWorkU.chrNo);
  for (i = 0; i < 12; i++)
  {
    equippedSum += unit->etherId[i];
  }

  if (equippedSum == 0)
  {
    messageColor[2] = 64;
    messageColor[1] = 64;
    messageColor[0] = 64;
  }
  else
  {
    messageColor[2] = -128;
    messageColor[1] = -128;
    messageColor[0] = -128;
  }
  MoveSlide(&screen->message.x, &messageTarget, 3.0f);
  eMessageMain(&screen->message);
}

typedef struct EtherL1R1Sprite {
    unsigned char unmodeled_00[0x04];
    short x;
    short y;
    int work;
    unsigned char unmodeled_0c[0x28 - 0x0c];
} EtherL1R1Sprite;
typedef struct EtherL1R1WorkBody {
    unsigned char state;
    unsigned char unmodeled_01;
    signed char slideOffset[2];
    int spriteWork;
    EtherL1R1Sprite sprite[2];
} EtherL1R1WorkBody;
void eSpriteSet(void *sprite, int id);
void eSpriteMain(void *sprite);
typedef struct EtherL1R1SpriteIdPair {
    short id[2];
} EtherL1R1SpriteIdPair;
extern const EtherL1R1SpriteIdPair D_004DB2E8[];
typedef struct EtherL1R1SlideStep {
    signed char side[2];
} EtherL1R1SlideStep;
extern const EtherL1R1SlideStep D_004DB2F0[];

typedef struct { unsigned char pad[0x2a]; unsigned short half_2a; } PadDataL1R1View;
extern EtherL1R1WorkBody *MenuEtherL1R1Ptr __asm__("MenuEtherL1R1");
extern unsigned char MenuWorkBytesL1R1[] __asm__("MenuWork");
extern PadDataL1R1View PadDataL1R1 __asm__("PadData");

void MenuEtherL1R1Main(void)
{
    EtherL1R1WorkBody *self = MenuEtherL1R1Ptr;
    signed char *slideOffset;
    int i;

    switch (self->state) {
    case 0: {
        EtherL1R1SpriteIdPair spriteIds = D_004DB2E8[0];

        self->spriteWork = 0x00FFFFFF;
        for (i = 0; i < 2; i++) {
            eSpriteSet(&self->sprite[i], spriteIds.id[i]);
            self->sprite[i].work = self->spriteWork;
            self->sprite[i].y = 214;
        }
        self->sprite[0].x = -45;
        self->sprite[1].x = 528;
        slideOffset = self->slideOffset;
        self->slideOffset[1] = 0;
        self->slideOffset[0] = 0;
        i = 2;
        self->state = i;
        break;
    }
    case 2:
        slideOffset = self->slideOffset;
        break;
    default:
        return;
    }

    {
        short target[2];

        target[0] = -45;
        target[1] = 528;
        if (MenuWorkBytesL1R1[3] == 0x20) {
            target[0] = 8;
            target[1] = 475;
            if (PadDataL1R1.half_2a & 4) {
                self->slideOffset[0] = -6;
            } else if (PadDataL1R1.half_2a & 8) {
                self->slideOffset[1] = 6;
            }
        }

        {
            EtherL1R1SlideStep step = D_004DB2F0[0];
            for (i = 0; i < 2; i++) {
                if (self->slideOffset[i] != 0) {
                    self->slideOffset[i] = self->slideOffset[i] + step.side[i];
                }
            }
        }

        for (i = 0; i < 2; i++) {
            MoveSlide(&self->sprite[i].x, &target[i], 3.0f);
            self->sprite[i].x = self->sprite[i].x + slideOffset[i];
            eSpriteMain(&self->sprite[i]);
        }
    }
}

typedef struct MenuEtherStHeader {
    unsigned char state;
    unsigned char cursorOn;
    unsigned char unmodeled_02[0x04 - 0x02];
    unsigned char moveVert;
    unsigned char flags;
    unsigned char subState;
    signed char lastCursor;
    int color;
} MenuEtherStHeader;
typedef struct MenuEtherStRecord {
    unsigned char chrNo;
    unsigned char fullStatus;
    unsigned char unmodeled_02[0x04 - 0x02];
    volatile short offsetX;
    short offsetY;
    unsigned char unmodeled_08[0x834 - 0x08];
} MenuEtherStRecord;
typedef struct MenuEtherStWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    char *title;
    signed char state;
    unsigned char visible;
    unsigned char unmodeled_12[0x14 - 0x12];
    void (*disp)(MenuEtherStRecord *record);
    MenuEtherStRecord *volatile dispWork;
    unsigned char unmodeled_1c[0x9C - 0x1C];
    unsigned char unmodeled_9c[0x4];
    unsigned char unmodeled_a0[0x188 - 0xA0];
} MenuEtherStWindow;
typedef struct MenuEtherStCursor {
    signed char state;
    unsigned char unmodeled_01[0x04 - 0x01];
    short x;
    short y;
    int work;
    unsigned char unmodeled_0c[0x10 - 0x0C];
} MenuEtherStCursor;
typedef struct MenuEtherStWindowEntry {
    MenuEtherStHeader header;
    MenuEtherStWindow window;
} MenuEtherStWindowEntry;
typedef struct MenuEtherStRecordEntry {
    MenuEtherStHeader header;
    MenuEtherStRecord work;
} MenuEtherStRecordEntry;
typedef struct MenuEtherStCursorEntry {
    MenuEtherStHeader header;
    MenuEtherStCursor cursor;
    unsigned char unmodeled_1c[0x24 - 0x1C];
} MenuEtherStCursorEntry;
typedef struct MenuEtherStWork {
    MenuEtherStWindowEntry window[8];
    MenuEtherStRecordEntry disp[8];
    MenuEtherStCursorEntry cursor[2];
    MenuEtherStHeader endHeader;
} MenuEtherStWork;
typedef struct MenuEtherMwView {
    unsigned char unmodeled_00[3];
    unsigned char state;
    unsigned char unmodeled_04[0x1C];
    unsigned char flags;
    unsigned char unmodeled_21[0x41 - 0x21];
    signed char partyCount;
    signed char partyCursor;
} MenuEtherMwView;
extern short MenuEtherParty[];
extern void MenuStatusDisp(MenuEtherStRecord *record);
extern int PartyAttackerCheck(int id);
extern void eCursolSet(void *cursor, signed char width);
extern void eCursolMain(void *cursor);
char *MenuTagTextGet(int kind);

void MenuEtherStatusMain(void)
{
    extern MenuEtherMwView MenuEtherMw __asm__("MenuWork");
    extern MenuEtherStWork *MenuEtherStatusP __asm__("MenuEtherStatus");
    MenuEtherStWork *status = MenuEtherStatusP;
    short targetX[8];
    short targetY[8];
    short targetWidth[8];
    short targetHeight[8];
    short targetOffset[8];
    int i;

    switch (status->window[0].header.state) {
    case 0:
        status->window[0].header.color = 0xF00000;
        for (i = 0; i < MenuEtherMw.partyCount; i++) {
            short chrNo = MenuEtherParty[i];

            WindowDXSet(&status->window[i].window);
            status->window[i].window.x = (i & 1) * 816 - 272;
            status->window[i].window.y = (i / 2) * 104 + 64;
            status->window[i].window.color = status->window[0].header.color;
            status->window[i].window.width = 240;
            status->window[i].window.height = 82;
            status->window[i].window.visible = 1;
            if (PartyAttackerCheck(chrNo) != 0) {
                status->window[i].window.title = MenuTagTextGet(1);
            } else {
                status->window[i].window.title = MenuTagTextGet(2);
            }
            status->window[i].window.disp = MenuStatusDisp;
            { MenuEtherStRecord *r = &status->disp[i].work;
            status->disp[i].work.offsetX = 0;
            status->window[i].window.dispWork = r; }
            status->disp[i].work.chrNo = chrNo;
            status->disp[i].work.fullStatus = 0;
            status->disp[i].work.offsetY = 0;
            status->window[i].window.state = 1;
            WindowDXMain(&status->window[i].window);
            status->window[i].window.state = 3;
        }
        for (i = 0; i < 1; i++) {
            eCursolSet(&status->cursor[i].cursor, 0);
            status->cursor[i].cursor.state = 0;
            status->cursor[i].cursor.work = 0x00FFFFFF;
        }
        status->window[0].header.cursorOn = 0;
        status->window[0].header.flags = 0;
        status->window[0].header.subState = 0;
        status->window[0].header.moveVert = 0;
        {
            unsigned char pc2 = MenuEtherMw.partyCursor;

            status->window[0].header.lastCursor = pc2;
            status->window[0].header.state = 2;
        }
        /* fall through */
    case 2:
        for (i = 0; i < MenuEtherMw.partyCount; i++) {
            targetX[i] = (i & 1) * 816 - 272;
            targetY[i] = (i / 2) * 104 + 64;
            targetWidth[i] = 240;
            targetHeight[i] = 82;
            targetOffset[i] = 0;
        }
        switch (MenuEtherMw.state) {
        case 0x10:
            if (MenuEtherMw.flags & 0x01) {
                status->cursor[0].cursor.state = 32;
                status->window[0].header.cursorOn = 1;
                status->window[0].header.moveVert = 1;
                if ((status->window[0].header.flags & 1) != 0) {
                    if (i != MenuEtherMw.partyCursor) {
                        status->window[i].window.x = (i & 1) * 816 - 272;
                        status->window[i].window.y = 24;
                    }
                    status->window[0].header.flags &= 0xFE;
                }
            }
            for (i = 0; i < MenuEtherMw.partyCount; i++) {
                targetX[i] = (i & 1) * 240 + 16;
            }
            break;
        case 0x20:
        case 0x30:
        case 0x32:
        case 0x50:
        case 0x70:
        case 0x72:
        case 0x74:
        case 0x76:
        case 0x78:
        case 0x80:
        case 0x82:
        case 0x84:
        case 0x86:
            if (MenuEtherMw.flags & 0x01) {
                switch (MenuEtherMw.state) {
                case 0x20:
                    status->cursor[0].cursor.state = 0;
                    status->window[0].header.lastCursor = MenuEtherMw.partyCursor;
                    /* fall through */
                case 0x70:
                case 0x72:
                    for (i = 0; i < MenuEtherMw.partyCount; i++) {
                        short chrNo = MenuEtherParty[i];

                        PartyAttackerCheck(chrNo);
                        if (PartyAttackerCheck(chrNo) != 0) {
                            *(char **)status->window[i].window.unmodeled_9c = MenuTagTextGet(1);
                        } else {
                            *(char **)status->window[i].window.unmodeled_9c = MenuTagTextGet(2);
                        }
                    }
                    break;
                case 0x30:
                    for (i = 0; i < MenuEtherMw.partyCount; i++) {
                        *(char **)status->window[i].window.unmodeled_9c = MenuTagTextGet(5);
                    }
                    break;
                case 0x82:
                    for (i = 0; i < MenuEtherMw.partyCount; i++) {
                        *(char **)status->window[i].window.unmodeled_9c = MenuTagTextGet(6);
                    }
                    break;
                }
            }
            if (status->window[0].header.lastCursor != MenuEtherMw.partyCursor) {
                status->window[MenuEtherMw.partyCursor].window.x = 264;
                status->window[MenuEtherMw.partyCursor].window.y = 32;
                status->window[0].header.lastCursor = MenuEtherMw.partyCursor;
            }
            if ((status->window[0].header.flags & 1) != 0) {
                for (i = 0; i < MenuEtherMw.partyCount; i++) {
                    if (i == MenuEtherMw.partyCursor) {
                        targetX[i] = 264;
                        targetY[i] = 32;
                    } else {
                        targetX[i] = (i & 1) * 816 - 272;
                        status->window[i].window.x = targetX[i];
                    }
                }
            } else {
                for (i = 0; i < MenuEtherMw.partyCount; i++) {
                    if (i == MenuEtherMw.partyCursor) {
                        targetX[i] = 264;
                        targetY[i] = 32;
                    }
                }
                if (targetY[MenuEtherMw.partyCursor] == status->window[MenuEtherMw.partyCursor].window.y) {
                    status->window[0].header.flags |= 1;
                }
            }
            status->window[0].header.cursorOn = 0;
            status->window[0].header.moveVert = 0;
            break;
        }
        for (i = 0; i < MenuEtherMw.partyCount; i++) {
            if (status->window[0].header.moveVert == 0) {
                MoveSlide(&status->window[i].window.width, &targetWidth[i], 3.0f);
                MoveSlide(&status->window[i].window.height, &targetHeight[i], 3.0f);
                MoveSlide(&status->disp[i].work.offsetX, &targetOffset[i], 3.0f);
                MoveSlide(&status->window[i].window.x, &targetX[i], 3.0f);
                if (status->window[i].window.x == targetX[i]) {
                    MoveSlide(&status->window[i].window.y, &targetY[i], 3.0f);
                }
            } else {
                MoveSlide(&status->window[i].window.y, &targetY[i], 3.0f);
                if (status->window[i].window.y == targetY[i]) {
                    MoveSlide(&status->window[i].window.width, &targetWidth[i], 3.0f);
                    MoveSlide(&status->disp[i].work.offsetX, &targetOffset[i], 3.0f);
                    MoveSlide(&status->window[i].window.x, &targetX[i], 3.0f);
                }
            }
            if (i == MenuEtherMw.partyCursor) {
                status->window[i].window.color = 0xF07F00;
            } else {
                status->window[i].window.color = 0xF07E00;
            }
            WindowDXMain(&status->window[i].window);
        }
        status->cursor[0].cursor.x = status->window[MenuEtherMw.partyCursor].window.x + 3;
        status->cursor[0].cursor.y = status->window[MenuEtherMw.partyCursor].window.y
                                     + status->window[MenuEtherMw.partyCursor].window.height / 2 - 4;
        status->cursor[0].cursor.work = 0x00FFFFFF;
        eCursolMain(&status->cursor[0].cursor);
        break;
    }
}

typedef struct MenuEtherWorkS2 {
    unsigned char unmodeled_00[0x03];
    unsigned char state;            /* +0x03 */
    unsigned char unmodeled_04[0x10 - 0x04];
    signed char charDenied;         /* +0x10 */
    unsigned char unmodeled_11[0x20 - 0x11];
    unsigned char flags;            /* +0x20; bit 0 rebuilds ether windows */
    unsigned char nextState;        /* +0x21 */
    unsigned char unmodeled_22[0x2A - 0x22];
    unsigned char wait;             /* +0x2A */
    unsigned char unmodeled_2b[0x30 - 0x2B];
    signed char commandCursor;      /* +0x30 */
    signed char menuCursor;         /* +0x31 */
    signed char slotCursor;         /* +0x32 */
    signed char slotCount;          /* +0x33 */
    signed char confirmCursor;      /* +0x34 */
    unsigned char unmodeled_35[0x40 - 0x35];
    signed char chrNo;              /* +0x40 */
    signed char partyCount;         /* +0x41 */
    signed char partyCursor;        /* +0x42 */
    signed char targetCursor;       /* +0x43 */
    signed char etherId;            /* +0x44 */
    signed char slotEther;          /* +0x45 */
    signed char targetChrNo;        /* +0x46 */
    signed char useType;            /* +0x47 */
    signed char targetCount;        /* +0x48 */
    unsigned char unmodeled_49[0x50 - 0x49];
    union {
        int listSelect;             /* +0x50 */
        unsigned char listSelectBytes[4];
    };
    unsigned char unmodeled_54[0x60 - 0x54];
    unsigned char targetList[8];    /* +0x60 */
} MenuEtherWorkS2;
typedef struct EtherStatusPanelNumber {
    short x;
    short y;
    int color;
    unsigned char unmodeled_08[0x90 - 0x08];
} EtherStatusPanelNumber;
extern void eCursolModeChange(void *cursor, unsigned int mode);
typedef struct EtherStatusPanel {
    signed char chrNo;
    signed char mode;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    unsigned char unmodeled_08[0x14 - 0x08];
    EtherStatusPanelNumber number[6];
    unsigned char unmodeled_374[0x840 - 0x374];
} EtherStatusPanel;
typedef struct EtherStatusWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    const char *title;
    unsigned char state;
    unsigned char enabled;
    unsigned char unmodeled_12[2];
    void (*callback)(struct WindowDX *, void *);
    struct EtherStatusPanel *callbackArg;
    unsigned char unmodeled_1c[0x9C - 0x1C];
    char *renderTitle;
    unsigned char unmodeled_a0[0x194 - 0xA0];
} EtherStatusWindow;
typedef struct EtherTargetCursor {
    unsigned char state;
    unsigned char unmodeled_01[3];
    unsigned short x;
    unsigned short y;
    int work;
    unsigned char unmodeled_0c[0x24 - 0x0C];
} EtherTargetCursor;
typedef struct EtherTargetStatusWork {
    unsigned char state;
    unsigned char active;
    unsigned char unmodeled_02;
    unsigned char targetInitialized;
    signed char visibleRow;
    signed char scrollOffset;
    signed char previousCursor;
    unsigned char drawFrame;
    signed char scrollLimit;
    signed char listMode;
    signed char multiCursor;
    signed char visibleCount;
    int color;
    EtherStatusWindow window[8];
    EtherStatusPanel panel[8];
    EtherTargetCursor cursor[3];
    EtherPasBox frame[2][2];
    EtherPasBox scrollBar;
    unsigned char unmodeled_4f58[0x4F64 - 0x4F58];
} EtherTargetStatusWork;

void MenuEtherStatusMain2(void)
{
    extern MenuEtherWorkS2 MenuWorkS2 __asm__("MenuWork");
    extern void *MenuEtherStatus2S2 __asm__("MenuEtherStatus2");
#define MenuWork MenuWorkS2
    EtherTargetStatusWork *screen = (void *)MenuEtherStatus2S2;
    int i;

    if (MenuWork.state == 255) {
        return;
    }
    switch (screen->state) {
    case 0:
        screen->color = 0x00FFF000;
        for (i = 0; i < MenuWork.partyCount; i++) {
            WindowDXSet((void *)(&screen->window[i]));
            screen->window[i].x = -272;
            screen->window[i].width = 240;
            screen->window[i].height = 82;
            screen->window[i].color = screen->color;
            screen->window[i].y = i * 104 + 64;
            screen->window[i].enabled = 1;
            screen->window[i].title = MenuTagTextGet(7);
            screen->window[i].callback =
                (void (*)(struct WindowDX *, void *))MenuStatusDisp;
            screen->window[i].callbackArg = &screen->panel[i];
            screen->panel[i].chrNo = MenuEtherParty[i];
            screen->panel[i].x = 0;
            screen->panel[i].y = 0;
            screen->window[i].state = 1;
            WindowDXMain((void *)(&screen->window[i]));
            screen->window[i].state = 3;
        }
        for (i = 0; i < 3; i++) {
            ((void (*)(void *, int))eCursolSet)(&screen->cursor[i], i);
            screen->cursor[i].state = 32;
            screen->cursor[i].work = screen->color + 16;
        }
        for (i = 0; i < 2; i++) {
            screen->frame[0][i].kind = 0x00FFFFFF;
            screen->frame[1][i].kind = 0;
        }
        screen->scrollOffset = 0;
        screen->visibleRow = 0;
        {
            unsigned char initialTargetCursor = MenuWork.targetCursor;

            screen->state = 2;
            screen->previousCursor = initialTargetCursor;
        }
        screen->targetInitialized = 0;
        screen->drawFrame = 0;
        screen->multiCursor = 0;
        screen->listMode = 0;
        screen->scrollLimit = 0;
        screen->visibleCount = 0;
        screen->active = 0;
        break;
    case 2:
        break;
    default:
        return;
    }

    {
        short targetX[8];
        short targetY[8];

        for (i = 0; i < 8; i++) {
            targetX[i] = -272;
            targetY[i] = i * 104 + 64;
        }
        switch (MenuWork.state) {
        case 48:
            screen->active = 1;
            screen->drawFrame = 1;
            screen->visibleCount = MenuWork.partyCount;
            /* fall through */
        case 50:
            if (MenuWork.flags & 1) {
                for (i = 0; i < MenuWork.partyCount; i++) {
                    screen->panel[i].chrNo = MenuEtherParty[i];
                    screen->window[i].renderTitle = MenuTagTextGet(7);
                }
                if (MenuWork.state == 48) {
                    screen->cursor[0].state = 0;
                } else {
                    screen->cursor[0].state = 32;
                }
                {
                    int viewportWidth = 256;
                    MenuEtherWorkS2 *frameMenu;

                    screen->frame[0][0].height = 48;
                    frameMenu = &MenuWork;
                    screen->frame[0][0].x = 0;
                    screen->frame[0][1].width = screen->frame[0][0].width = viewportWidth;
                    screen->frame[0][1].y = 354;
                    screen->frame[0][1].height = 158;
                    screen->frame[0][0].y = 0;
                    screen->frame[0][1].x = 0;
                    screen->previousCursor = frameMenu->targetCursor;
                    screen->scrollLimit = frameMenu->partyCount - 3;
                }
            }
            if (screen->previousCursor != MenuWork.targetCursor) {
                if (MenuWork.targetCursor < screen->previousCursor) {
                    if (screen->visibleRow == 0) {
                        screen->scrollOffset--;
                        MenuWork.wait = 32;
                    } else {
                        screen->visibleRow--;
                    }
                } else {
                    if (screen->visibleRow == 2) {
                        screen->scrollOffset++;
                        MenuWork.wait = 32;
                    } else {
                        screen->visibleRow++;
                    }
                }
                screen->previousCursor = MenuWork.targetCursor;
            }
            for (i = 0; i < MenuWork.partyCount; i++) {
                targetX[i] = 16;
                targetY[i] = (i - screen->scrollOffset) * 104 + 64;
            }
            if (screen->window[0].x == targetX[0]) {
                screen->listMode = 1;
            }
            if (screen->window[MenuWork.targetCursor].y == targetY[MenuWork.targetCursor]) {
                if (MenuWork.state == 50 && screen->cursor[0].state == 0) {
                    screen->cursor[0].state = 32;
                }
                MenuWork.wait = 0;
            }
            break;
        case 130:
            if (MenuWork.flags & 1) {
                unsigned char targetsInitialized = screen->targetInitialized;

                screen->drawFrame = 1;
                screen->active = 1;
                screen->visibleCount = MenuWork.targetCount;
                if (targetsInitialized == 0) {
                    for (i = 0; i < MenuWork.targetCount; i++) {
                        screen->panel[i].chrNo = MenuEtherParty[MenuWork.targetList[i]];
                        screen->window[i].renderTitle = MenuTagTextGet(8);
                        screen->window[i].x = -272;
                        screen->window[i].y = i * 100 + 96;
                    }
                    screen->cursor[0].state = 32;
                    {
                        unsigned char targetCursor = MenuWork.targetCursor;

                        screen->targetInitialized = 1;
                        screen->previousCursor = targetCursor;
                    }
                }
                screen->frame[0][0].height = 80;
                screen->frame[0][0].x = 0;
                screen->frame[0][1].width = screen->frame[0][0].width = 256;
                screen->frame[0][1].y = 378;
                screen->frame[0][1].height = 134;
                screen->frame[0][0].y = 0;
                screen->frame[0][1].x = 0;
                screen->scrollLimit = MenuWork.targetCount - 3;
            }
            if (screen->previousCursor != MenuWork.targetCursor) {
                if (MenuWork.targetCursor < screen->previousCursor) {
                    if (screen->visibleRow == 0) {
                        screen->scrollOffset--;
                        MenuWork.wait = 32;
                    } else {
                        screen->visibleRow--;
                    }
                } else {
                    if (screen->visibleRow == 2) {
                        screen->scrollOffset++;
                        MenuWork.wait = 32;
                    } else {
                        screen->visibleRow++;
                    }
                }
                screen->previousCursor = MenuWork.targetCursor;
            }
            for (i = 0; i < MenuWork.targetCount; i++) {
                targetX[i] = 16;
                targetY[i] = (i - screen->scrollOffset) * 100 + 96;
            }
            if (screen->window[0].x == targetX[0]) {
                screen->listMode = 2;
            }
            if (screen->window[MenuWork.targetCursor].y == targetY[MenuWork.targetCursor]) {
                eCursolModeChange(&screen->cursor[0], 32);
                MenuWork.wait = 0;
            }
            break;
        case 132:
            screen->listMode = 0;
            screen->drawFrame = 1;
            /* fall through */
        case 134:
            for (i = 0; i < MenuWork.partyCount; i++) {
                targetY[i] = (i - screen->scrollOffset) * 100 + 96;
            }
            targetX[MenuWork.targetCursor] = 16;
            targetY[MenuWork.targetCursor] = 96;
            break;
        default:
            screen->scrollOffset = 0;
            screen->visibleRow = 0;
            screen->targetInitialized = 0;
            if (screen->window[0].x == targetX[0]) {
                screen->drawFrame = 0;
            }
            screen->listMode = 0;
            break;
        }

        if (screen->active == 0) {
            return;
        }
        if (screen->drawFrame != 0) {
            for (i = 0; i < 2; i++) {
                endPrintExtFunc(screen->color, 12, &screen->frame[0][i]);
            }
            screen->scrollBar.width = 256;
            screen->scrollBar.y = 48;
            screen->scrollBar.x = 0;
            if (screen->listMode == 2) {
                screen->scrollBar.y = 80;
            }
            screen->scrollBar.height = 354 - (unsigned short)screen->scrollBar.y;
            endPrintExtFunc(screen->color, 101, &screen->scrollBar);
        }
        for (i = 0; i < screen->visibleCount; i++) {
            short *xTarget = &targetX[i];

            MoveSlide(&screen->window[i].x, xTarget, 3.0f);
            if (screen->window[i].x == *xTarget) {
                MoveSlide(&screen->window[i].y, &targetY[i], 3.0f);
            }
            {
                unsigned short clipOffset = screen->window[i].y + 39;

                if (clipOffset < 415) {
                    WindowDXMain((void *)(&screen->window[i]));
                }
            }
        }
        if (MenuWork.useType == 1 || MenuWork.useType == 3 || MenuWork.state == 130) {
            screen->cursor[0].x = screen->window[MenuWork.targetCursor].x + 3;
            screen->cursor[0].y = screen->window[MenuWork.targetCursor].y +
                                screen->window[MenuWork.targetCursor].height / 2 - 4;
            eCursolMain(&screen->cursor[0]);
            if (screen->scrollLimit > 0 && screen->cursor[0].state != 0 && screen->listMode != 0) {
                if (screen->scrollOffset != 0) {
                    screen->cursor[1].x = 240;
                    if (screen->listMode == 1) {
                        screen->cursor[1].y = 50;
                    } else {
                        screen->cursor[1].y = 82;
                    }
                    eCursolMain(&screen->cursor[1]);
                }
                if ((unsigned char)screen->scrollOffset != (unsigned char)screen->scrollLimit) {
                    screen->cursor[2].x = 240;
                    if (screen->listMode == 1) {
                        screen->cursor[2].y = 338;
                    } else {
                        screen->cursor[2].y = 362;
                    }
                    eCursolMain(&screen->cursor[2]);
                }
            }
        } else if (MenuWork.useType == 2) {
            int count = MenuWork.partyCount;
            if (count >= 4) {
                count = 3;
            }

            if (screen->multiCursor == count - 1) {
                screen->multiCursor = 0;
            } else {
                screen->multiCursor++;
            }
            screen->cursor[0].x = screen->window[screen->multiCursor].x + 3;
            screen->cursor[0].y = screen->multiCursor * 104 + 101;
            eCursolMain(&screen->cursor[0]);
        }
        if (screen->drawFrame != 0) {
            EtherPasBox *nextFrame = screen->frame[1];

            for (i = 0; i < 2; i++) {
                EtherPasBox *callFrame = nextFrame;

                screen->frame[1][i].width = screen->frame[0][i].width;
                screen->frame[1][i].x = screen->frame[0][i].x;
                screen->frame[1][i].y = screen->frame[0][i].y;
                screen->frame[1][i].height = screen->frame[0][i].height;
                nextFrame++;
                endPrintExtFunc(screen->color, 12, callFrame);
            }
            endPrintExtFunc(screen->color, 102, 0);
        }
    }
#undef MenuWork
}

extern EtherScreenWork *MenuEtherList2;

extern unsigned char MenuKeepSelect[0x64];

typedef struct MenuEtherWork_L1 {
    unsigned char unmodeled_00[0x03];
    unsigned char state;            /* +0x03 */
    unsigned char unmodeled_04[0x10 - 0x04];
    signed char charDenied;         /* +0x10 */
    unsigned char unmodeled_11[0x20 - 0x11];
    unsigned char flags;            /* +0x20; bit 0 rebuilds ether windows */
    unsigned char nextState;        /* +0x21 */
    unsigned char unmodeled_22[0x2A - 0x22];
    unsigned char wait;             /* +0x2A */
    unsigned char unmodeled_2b[0x30 - 0x2B];
    signed char commandCursor;      /* +0x30 */
    signed char menuCursor;         /* +0x31 */
    signed char slotCursor;         /* +0x32 */
    signed char slotCount;          /* +0x33 */
    signed char confirmCursor;      /* +0x34 */
    unsigned char unmodeled_35[0x40 - 0x35];
    signed char chrNo;              /* +0x40 */
    signed char partyCount;         /* +0x41 */
    signed char partyCursor;        /* +0x42 */
    signed char targetCursor;       /* +0x43 */
    signed char etherId;            /* +0x44 */
    signed char slotEther;          /* +0x45 */
    signed char targetChrNo;        /* +0x46 */
    signed char useType;            /* +0x47 */
    signed char targetCount;        /* +0x48 */
    unsigned char unmodeled_49[0x50 - 0x49];
    union {
        int listSelect;             /* +0x50 */
        unsigned char listSelectBytes[4];
    };
    unsigned char unmodeled_54[0x60 - 0x54];
    unsigned char targetList[8];    /* +0x60 */
} MenuEtherWork_L1;
typedef struct MenuEtherPadData_L1 {
    unsigned char unmodeled_00[0x2A];
    unsigned short pressed; /* +0x2A */
    unsigned char unmodeled_2c[0x34 - 0x2C];
    unsigned short repeat;  /* +0x34 */
} MenuEtherPadData_L1;
typedef struct EtherMessageColor_L1 {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
} EtherMessageColor_L1;
typedef struct EtherPasMessage_L1 {
    unsigned char unmodeled_00;
    unsigned char mode; /* +0x01 */
    unsigned char unmodeled_02[0x04 - 0x02];
    short x; /* +0x04 */
    short y; /* +0x06 */
    int color; /* +0x08 */
    unsigned char unmodeled_0c[0x10 - 0x0C];
    EtherMessageColor_L1 glyphColor; /* +0x10 */ /* +0x10; first three bytes are RGB */
    unsigned char unmodeled_14[0x18 - 0x14];
    const char *text; /* +0x18 */
    unsigned char unmodeled_1c[0x44 - 0x1C];
} EtherPasMessage_L1;
void WindowSPSet(void *cursor);
void WindowSPMain(void *cursor);
extern int *MenuSortAddrGet(int list);
extern int MenuSortCheck(int list);
extern void WindowSPSelectJump(void *window, int index);
extern void WindowSPItemChange(void *window);
void WindowSPSetSelect(void *window, unsigned char *keep);
extern const char D_004C8670[16];
extern const char D_004C8680[16];
extern const char D_004C8690[16];
extern const char D_004C86A0[16];
typedef struct EtherMenuListWindow {
    unsigned char state;
    unsigned char itemType;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int work;
    short width;
    short height;
    const char *title;
    signed char active;
    signed char lineCount;
    unsigned char unmodeled_16[6];
    struct EtherListRow *items;
    unsigned char unmodeled_20[4];
    signed char firstVisible;
    unsigned char unmodeled_25;
    signed char selectionMode;
    unsigned char unmodeled_27[3];
    short itemCount;
    unsigned char unmodeled_2c[0x172C - 0x2C];
} EtherMenuListWindow;
typedef struct EtherMenuListHeader {
    unsigned char state;
    unsigned char unmodeled_01;
    unsigned char configured;
    unsigned char acquireLayout;
    unsigned char showNumbers;
    unsigned char unmodeled_05[3];
    int work;
} EtherMenuListHeader;
typedef struct EtherMenuListEntry {
    EtherMenuListHeader header;
    EtherMenuListWindow window;
} EtherMenuListEntry;
typedef struct EtherMenuListsWork {
    EtherMenuListEntry window[2];
    EtherMenuListHeader endHeader;
    EtherL1R1Sprite sprite[8];
    EtherDetailNumber number[8];
} EtherMenuListsWork;

void MenuEtherListMain(void)
{
    extern MenuEtherWork_L1 MenuWork_L1 __asm__("MenuWork");
    extern MenuEtherPadData_L1 PadData_L1 __asm__("PadData");
    extern void * MenuEtherList_L1 __asm__("MenuEtherList");
    short targetX[2];
    EtherMenuListsWork *screen = (void *)MenuEtherList_L1;
    int i;

    switch (screen->window[0].header.state) {
    case 0:
        screen->window[0].header.work = 0x00FFFFF8;
        for (i = 0; i < 2; i++) {
            screen->window[i].window.work = screen->window[0].header.work;
            screen->window[i].window.items = 0;
            screen->window[i].window.x = 528;
            screen->window[i].window.y = 64;
            WindowSPSet((void *)(&screen->window[i].window));
        }
        for (i = 0; i < 8; i++) {
            eSpriteSet(&screen->sprite[i], 1546);
            screen->sprite[i].work = 0x00FFFFFF;
        }
        for (i = 0; i < 8; i++) {
            eNumberSet(&screen->number[i], 0);
            screen->number[i].work = 0x00FFFFFA;
        }
        screen->window[0].header.configured = 0;
        screen->window[0].header.state = 2;
        screen->window[0].header.acquireLayout = 0;
        screen->window[0].header.showNumbers = 0;
        break;
    case 2:
        break;
    default:
        return;
    }

    if (screen->window[0].header.acquireLayout == 0) {
        targetX[0] = 528;
        targetX[1] = -262;
    } else {
        targetX[1] = -262;
        targetX[0] = -262;
    }
    switch (MenuWork_L1.state) {
    case 48:
        if ((MenuWork_L1.flags & 1) && screen->window[0].header.configured == 0) {
            EtherMenuListWindow *window = &screen->window[0].window;

            screen->window[0].window.x = 528;
            screen->window[0].window.y = 144;
            screen->window[0].window.height = 222;
            screen->window[0].window.lineCount = 9;
            screen->window[0].window.width = 232;
            screen->window[0].window.title = D_004C8670;
            screen->window[0].window.active = 1;
            screen->window[0].window.items = MenuListGet(0);
            screen->window[0].window.itemType = 7;
            WindowSPItemChange((void *)((void *)window));
            WindowSPSetSelect((void *)(window), MenuKeepSelect);
            screen->window[0].window.state = 17;
            WindowSPMain((void *)(window));
            screen->window[0].header.configured = 1;
            screen->window[0].header.acquireLayout = 0;
            screen->window[0].header.showNumbers = 0;
        }
        /* fall through */
    case 50:
        targetX[0] = 272;
        break;
    case 80:
        if (MenuWork_L1.flags & 1) {
            EtherMenuListWindow *window = &screen->window[0].window;

            screen->window[0].window.x = 528;
            screen->window[0].window.y = 160;
            screen->window[0].window.height = 198;
            screen->window[0].window.lineCount = 8;
            screen->window[0].window.width = 256;
            screen->window[0].window.title = D_004C8680;
            screen->window[0].window.active = 1;
            screen->window[0].window.items = MenuListGet(0);
            screen->window[0].window.itemType = 3;
            WindowSPItemChange((void *)((void *)window));
            WindowSPSetSelect((void *)(window), &MenuKeepSelect[5]);
            screen->window[0].window.state = 17;
            WindowSPMain((void *)(window));
            screen->window[0].header.acquireLayout = 0;
            screen->window[0].header.showNumbers = 1;
        }
        targetX[0] = 16;
        break;
    case 112:
        if ((MenuWork_L1.flags & 1) && screen->window[0].header.configured == 0) {
            EtherMenuListWindow *window = &screen->window[0].window;

            screen->window[0].window.x = -262;
            screen->window[0].window.y = 92;
            screen->window[0].window.height = 174;
            screen->window[0].window.width = 246;
            screen->window[0].window.title = D_004C8690;
            screen->window[0].window.lineCount = 7;
            screen->window[0].window.active = 1;
            screen->window[0].window.items = MenuListGet(0);
            screen->window[0].window.itemType = 7;
            WindowSPItemChange((void *)((void *)window));
            WindowSPSetSelect((void *)(window), &MenuKeepSelect[10]);
            screen->window[0].window.selectionMode = 4;
            screen->window[0].window.state = 17;
            WindowSPMain((void *)(window));
            screen->window[0].header.showNumbers = 0;
            screen->window[0].header.configured = 1;
            screen->window[0].header.acquireLayout = 1;
        }
        /* fall through */
    case 114:
    case 116:
    case 118:
    case 120:
    case 128:
        targetX[0] = 10;
        break;
    case 132: {
        unsigned char initializeList = MenuWork_L1.flags & 1;

        if (initializeList) {
            EtherMenuListWindow *window = &screen->window[1].window;
            i = 0;

            screen->window[1].window.x = -262;
            screen->window[1].window.y = 196;
            screen->window[1].window.width = 246;
            screen->window[1].window.height = 174;
            screen->window[1].window.itemType = 7;
            screen->window[1].window.active = 1;
            screen->window[1].window.lineCount = 7;
            screen->window[1].window.title = D_004C86A0;
            screen->window[1].window.items = MenuListGet(1);
            WindowSPItemChange((void *)((void *)window));
            screen->window[1].window.selectionMode = 4;
            screen->window[1].window.state = 17;
            WindowSPMain((void *)(window));
            {
                int *sortRows = MenuSortAddrGet(1);
                int count = MenuSortCheck(1);

                while (i < count) {
                    short rowEther = sortRows[i];
                    signed char targetEther = MenuWork_L1.etherId;

                    if (rowEther == targetEther) {
                        break;
                    }
                    i++;
                }
                WindowSPSelectJump((void *)((void *)window), i);
            }
            screen->window[0].header.configured = 2;
        }
    }
        /* fall through */
    case 134:
        targetX[1] = 10;
        break;
    default:
        screen->window[0].header.configured = 0;
        break;
    }
    for (i = 0; i < 2; i++) {
        MoveSlide(&screen->window[i].window.x, &targetX[i], 3.0f);
        WindowSPMain((void *)(&screen->window[i].window));
    }
    if (screen->window[0].header.showNumbers != 0) {
        unsigned int iconIndex = 0;
        int displayRow;

        for (displayRow = 0; displayRow < 8; displayRow++) {
            int row = screen->window[0].window.firstVisible + displayRow;

            if (row < screen->window[0].window.itemCount) {
                if (screen->window[0].window.items[row].status != 0) {
                    screen->sprite[iconIndex].x =
                        (screen->window[0].window.x + screen->window[0].window.width) - 39;
                    screen->sprite[iconIndex].y =
                        (screen->window[0].window.y + displayRow * 24) + 3;
                    eSpriteMain(&screen->sprite[iconIndex]);
                    iconIndex++;
                }
                screen->number[displayRow].x =
                    (screen->window[0].window.x + screen->window[0].window.width) - 63;
                {
                    unsigned short numberWindowY = screen->window[0].window.y;

                    screen->number[displayRow].digits = 2;
                    screen->number[displayRow].format = 0;
                    screen->number[displayRow].y = (numberWindowY + displayRow * 24) + 7;
                }
                screen->number[displayRow].value = screen->window[0].window.items[row].amount;
                eNumberMain(&screen->number[displayRow]);
            }
        }
    }
}

typedef struct MenuEtherWork_L2 {
    unsigned char unmodeled_00[0x03];
    unsigned char state;            /* +0x03 */
    unsigned char unmodeled_04[0x10 - 0x04];
    signed char charDenied;         /* +0x10 */
    unsigned char unmodeled_11[0x20 - 0x11];
    unsigned char flags;            /* +0x20; bit 0 rebuilds ether windows */
    unsigned char nextState;        /* +0x21 */
    unsigned char unmodeled_22[0x2A - 0x22];
    unsigned char wait;             /* +0x2A */
    unsigned char unmodeled_2b[0x30 - 0x2B];
    signed char commandCursor;      /* +0x30 */
    signed char menuCursor;         /* +0x31 */
    signed char slotCursor;         /* +0x32 */
    signed char slotCount;          /* +0x33 */
    signed char confirmCursor;      /* +0x34 */
    unsigned char unmodeled_35[0x40 - 0x35];
    signed char chrNo;              /* +0x40 */
    signed char partyCount;         /* +0x41 */
    signed char partyCursor;        /* +0x42 */
    signed char targetCursor;       /* +0x43 */
    signed char etherId;            /* +0x44 */
    signed char slotEther;          /* +0x45 */
    signed char targetChrNo;        /* +0x46 */
    signed char useType;            /* +0x47 */
    signed char targetCount;        /* +0x48 */
    unsigned char unmodeled_49[0x50 - 0x49];
    union {
        int listSelect;             /* +0x50 */
        unsigned char listSelectBytes[4];
    };
    unsigned char unmodeled_54[0x60 - 0x54];
    unsigned char targetList[8];    /* +0x60 */
} MenuEtherWork_L2;
typedef struct MenuEtherPadData_L2 {
    unsigned char unmodeled_00[0x2A];
    unsigned short pressed; /* +0x2A */
    unsigned char unmodeled_2c[0x34 - 0x2C];
    unsigned short repeat;  /* +0x34 */
} MenuEtherPadData_L2;
typedef struct EtherMessageColor_L2 {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
} EtherMessageColor_L2;
typedef struct EtherPasMessage_L2 {
    unsigned char unmodeled_00;
    unsigned char mode; /* +0x01 */
    unsigned char unmodeled_02[0x04 - 0x02];
    short x; /* +0x04 */
    short y; /* +0x06 */
    int color; /* +0x08 */
    unsigned char unmodeled_0c[0x10 - 0x0C];
    EtherMessageColor_L2 glyphColor; /* +0x10 */ /* +0x10; first three bytes are RGB */
    unsigned char unmodeled_14[0x18 - 0x14];
    const char *text; /* +0x18 */
    unsigned char unmodeled_1c[0x44 - 0x1C];
} EtherPasMessage_L2;
extern char D_004DB2F8[];
typedef struct EtherListCursor {
    unsigned char state;
    unsigned char unmodeled_01[3];
    unsigned short x;
    unsigned short y;
    int work;
    unsigned char unmodeled_0c[0x24 - 0x0C];
} EtherListCursor;
typedef struct EtherResultWindowDX {
    short x; /* +0x00 */
    short y; /* +0x02 */
    int color; /* +0x04 */
    short width; /* +0x08 */
    short height; /* +0x0A */
    const char *title; /* +0x0C */
    unsigned char state; /* +0x10: 1 while the frame opens, 3 once it is up */
    unsigned char unmodeled_11[0x14 - 0x11];
    void *select; /* +0x14 */
    void *selectArg; /* +0x18 */
    unsigned char unmodeled_1c[0x9C - 0x1C];
    const char *renderTitle; /* +0x9C; active title after initialization */
    unsigned char unmodeled_a0[0x194 - 0xA0];
} EtherResultWindowDX;
typedef struct EtherResultWork {
    unsigned char state;
    unsigned char unmodeled_01;
    unsigned char rightPanel;
    unsigned char unmodeled_03;
    int work;
    EtherResultWindowDX window;
    EtherPasMessage_L2 messages[3];
    EtherDetailNumber numbers[3];
    EtherListCursor cursor;
} EtherResultWork;
extern const char D_004C8810[];

void MenuEtherListMain2(void) {
    extern MenuEtherWork_L2 MenuWork_L2 __asm__("MenuWork");
    extern MenuEtherPadData_L2 PadData_L2 __asm__("PadData");
    EtherResultWork *screen = (void *)MenuEtherList2;
    EtherResultWindowDX *window;
    short targetX;
    short targetY;
    int i;

    switch (screen->state) {
    case 0:
        screen->work = 0x00FFFFF8;
        window = &screen->window;
        WindowDXSet((void *)(window));
        screen->window.x = -240;
        screen->window.y = 280;
        screen->window.width = 185;
        screen->window.height = 78;
        screen->window.color = screen->work;
        screen->window.title = D_004DB2F8;
        screen->window.state = 1;
        WindowDXMain((void *)(&screen->window));
        screen->window.state = 3;
        for (i = 0; i < 3; i++) {
            eMessageSet(&screen->messages[i], 0);
            screen->messages[i].mode = 32;
            screen->messages[i].color = screen->work + 2;
        }
        for (i = 0; i < 3; i++) {
            eNumberSet(&screen->numbers[i], 0);
            screen->numbers[i].work = screen->work + 2;
            screen->numbers[i].format = 0;
            screen->numbers[i].digits = 4;
            screen->numbers[i].value = 0;
        }
        eCursolSet(&screen->cursor, 0);
        screen->cursor.state = 0;
        screen->rightPanel = 0;
        screen->state = 2;
        screen->cursor.work = screen->work + 2;
        break;
    case 2:
        window = &screen->window;
        break;
    default:
        return;
    }

    if (screen->rightPanel == 0) {
        targetX = -243;
        targetY = 288;
    } else {
        targetX = 528;
        targetY = 144;
    }
    switch (MenuWork_L2.state) {
    case 116:
        if ((unsigned char)(MenuWork_L2.flags & 1)) {
            unsigned char *slotEthers;
            screen->cursor.state = 32;
            screen->window.x = -240;
            screen->window.y = 288;
            screen->window.height = 78;
            screen->window.renderTitle = D_004DB2F8;
            screen->rightPanel = 0;
            slotEthers = MenuEtherDataGet(MenuSortGet(0, MenuWork_L2.listSelect));
            for (i = 0; i < 3; i++) {
                if (slotEthers[i] != 0) {
                    screen->messages[i].text = func_A2C5A8(slotEthers[i])->name;
                    screen->numbers[i].value = func_A1A488(slotEthers[i])->pointCost;
                    if (func_A19578(MenuWork_L2.chrNo, slotEthers[i]) != 0) {
                        screen->messages[i].glyphColor.blue = 64;
                        screen->messages[i].glyphColor.green = 64;
                        screen->messages[i].glyphColor.red = 64;
                    } else {
                        screen->messages[i].glyphColor.blue = -128;
                        screen->messages[i].glyphColor.green = -128;
                        screen->messages[i].glyphColor.red = -128;
                    }
                } else {
                    screen->messages[i].text = 0;
                    screen->numbers[i].value = 0;
                }
            }
        }
    case 118:
        targetX = 71;
        break;
    case 130:
        if ((unsigned char)(MenuWork_L2.flags & 1)) {
            screen->rightPanel = 1;
            screen->window.x = 528;
            screen->window.y = 144;
            screen->window.height = 30;
            screen->window.renderTitle = D_004C8810;
            {
                MenuTextEntryL2 *entry;
                int next;
                signed char normalShade = -128;
                int *firstValue = &screen->numbers[0].value;

                i = 0;
                entry = func_A2C5A8(MenuWork_L2.etherId);
                *firstValue = 0;
                screen->messages[i].text = entry->name;
                screen->messages[i].glyphColor.blue = normalShade;
                screen->messages[i].glyphColor.green = normalShade;
                screen->messages[i].glyphColor.red = normalShade;
                for (i = 1; i < 3; i = next) {
                    next = i + 1;
                    screen->messages[i].text = 0;
                    screen->numbers[i].value = 0;
                }
            }
        }
    case 132:
    case 134:
        targetX = 272;
        targetY = 144;
        break;
    default:
        screen->cursor.state = 0;
        break;
    }

    MoveSlide(&window->x, &targetX, 3.0f);
    if (window->x == targetX) {
        MoveSlide(&screen->window.y, &targetY, 3.0f);
    }
    WindowDXMain((void *)(window));
    for (i = 0; i < 3; i++) {
        screen->messages[i].x = screen->window.x + 19;
        screen->messages[i].y = screen->window.y + 24 * i + 3;
        eMessageMain(&screen->messages[i]);
    }
    screen->cursor.x = screen->window.x + 3;
    screen->cursor.y = screen->window.y + MenuWork_L2.slotCursor * 24 + 7;
    eCursolMain(&screen->cursor);
}

extern MenuEtherRow *MenuListMake(int list, int mode);
extern int *MenuSortAddrGet(int list);
extern int MenuSortCheck(int list);
extern int func_00A11220(int, int);
extern int etherUseCk(int) __asm__("MenuEtherUseCheck");
void MenuEtherListMake00(void)
{
    MenuEtherRow *row = MenuListGet(0);
    int *sort = MenuSortAddrGet(0);
    int n = MenuSortCheck(0);
    char *chr = ld191c0(MenuWork.unk40);
    int v;

    MenuListMake(0, 0);
    if (n > 0) do {
        func_A1A378(ld1a488(*(short *)sort)->tecId);
        row->name = *MenuTextGet(*sort);
        v = func_00A11220(MenuWork.unk40, *(short *)sort);
        row->value = v;
        if (*(short *)(chr + 0x36) >= v) {
            row->unk8 = 0;
            row->unk9 = 0;
        } else {
            row->unk8 = 1;
            row->unk9 = 1;
        }
        if (etherUseCk(*(short *)sort) == 0) {
            row->unk8 = 1;
            row->unk9 = 2;
        }
        row++;
        sort++;
    } while (--n != 0);
}

void MenuEtherListMake01(void)
{
    MenuEtherRow *row = MenuListGet(0);
    int *sort = MenuSortAddrGet(0);
    int n = MenuSortCheck(0);
    char *chr = ld191c0(MenuWork.unk40);
    EtherStatus *e;
    int i, j;

    MenuListMake(0, 0);
    for (i = 0; i < n; i++) {
        e = ld1a488((short)sort[i]);
        func_A1A378(e->tecId);
        row[i].name = *MenuTextGet(sort[i]);
        row[i].value = *(short *)((char *)e + 4);
        if (chr[0x17] + row[i].value < 13) {
            row[i].unk8 = 0;
        } else {
            row[i].unk8 = 1;
        }
        for (j = 0; j < 12; j++) {
            if (((short *)(chr + 0x8E))[j] == (short)sort[i]) break;
        }
        if (j != 12) {
            row[i].unk9 = 1;
        } else {
            row[i].unk9 = 0;
        }
    }
}

extern int *ld19210(int) __asm__("func_A19210");

void MenuEtherListMake02(int list)
{
    MenuEtherRow *row = MenuListGet(list);
    int *sort = MenuSortAddrGet(list);
    int n = MenuSortCheck(list);
    int *chr = ld19210(MenuWork.unk40);
    int i, v, who;

    MenuListMake(list, 0);
    for (i = 0; i < n; i++) {
        row[i].value = ((short *)ld1a488((short)sort[i]))[4];
        who = MenuEtherWhoCheck((short)sort[i]);
        v = row[i].value;
        if (v == 0) {
            row[i].unk8 = 1;
            row[i].value = -1;
        } else if (MenuWork.unk40 != who) {
            row[i].unk8 = 1;
        } else if (chr[4] >= v / 2) {
            row[i].unk8 = 0;
        } else {
            row[i].unk8 = 1;
        }
    }
}

extern void MenuSortSet(int list, int mode, int who);
extern struct { char pad[0x44]; signed char unk44; char pad45; signed char unk46; } MenuWork3 __asm__("MenuWork");

void MenuEtherListMake03(int list)
{
    MenuEtherRow *row = MenuListGet(list);
    int *sort = MenuSortAddrGet(list);
    unsigned char *p;
    int i, n, idx;

    p = (unsigned char *)ld19210(MenuWork3.unk46);
    idx = MenuWork3.unk44 - 1;
    p = (unsigned char *)((idx / 8) + (int)p) + 0x28;
    *p |= 1 << (idx % 8);
    MenuSortSet(list, 0x80, MenuWork3.unk46);
    *p &= ~(1 << ((MenuWork3.unk44 - 1) % 8));
    MenuListMake(list, 0);
    n = MenuSortCheck(list);
    for (i = 0; i < n; i++) {
        row[i].value = ((short *)ld1a488((short)sort[i]))[4];
        if ((short)sort[i] == MenuWork3.unk44) {
            row[i].unk8 = 1;
        } else {
            row[i].unk8 = 0;
        }
    }
}

typedef struct MenuEtherWorkUP3 {
    unsigned char unmodeled_00[0x03];
    unsigned char state;            /* +0x03 */
    unsigned char unmodeled_04[0x10 - 0x04];
    signed char charDenied;         /* +0x10 */
    unsigned char unmodeled_11[0x20 - 0x11];
    unsigned char flags;            /* +0x20; bit 0 rebuilds ether windows */
    unsigned char nextState;        /* +0x21 */
    unsigned char unmodeled_22[0x2A - 0x22];
    unsigned char wait;             /* +0x2A */
    unsigned char unmodeled_2b[0x30 - 0x2B];
    signed char commandCursor;      /* +0x30 */
    signed char menuCursor;         /* +0x31 */
    signed char slotCursor;         /* +0x32 */
    signed char slotCount;          /* +0x33 */
    signed char confirmCursor;      /* +0x34 */
    unsigned char unmodeled_35[0x40 - 0x35];
    signed char chrNo;              /* +0x40 */
    signed char partyCount;         /* +0x41 */
    signed char partyCursor;        /* +0x42 */
    signed char targetCursor;       /* +0x43 */
    signed char etherId;            /* +0x44 */
    signed char slotEther;          /* +0x45 */
    signed char targetChrNo;        /* +0x46 */
    signed char useType;            /* +0x47 */
    signed char targetCount;        /* +0x48 */
    unsigned char unmodeled_49[0x50 - 0x49];
    union {
        int listSelect;             /* +0x50 */
        unsigned char listSelectBytes[4];
    };
    unsigned char unmodeled_54[0x60 - 0x54];
    unsigned char targetList[8];    /* +0x60 */
} MenuEtherWorkUP3;

int MenuEtherEquip(void) {
    extern MenuEtherWorkUP3 MenuWork_UP3 __asm__("MenuWork");
    EtherListRow *rows = (EtherListRow *)MenuListGet(0);
    int *sortRow = MenuSortAddrGet(0);
    EtherUnitRecord *unit = ((EtherUnitRecord *(*)(int))func_A191C0)(MenuWork_UP3.chrNo);
    int slot;

    if (MenuWork_UP3.listSelect < 0) {
        return 0;
    }
    if (rows[MenuWork_UP3.listSelect].status != 0) {
        for (slot = 0; slot < 12; slot++) {
            if (unit->etherId[slot] == (short)sortRow[MenuWork_UP3.listSelect]) {
                unit->etherId[slot] = 0;
                unit->capUsed = (unsigned char)unit->capUsed -
                    (unsigned char)rows[MenuWork_UP3.listSelect].amount;
                break;
            }
        }
    } else {
        if (rows[MenuWork_UP3.listSelect].flag != 0) {
            return 0;
        }
        {
            short *slotId;
            slotId = unit->etherId;
            for (slot = 0; slot < 12; slot++, slotId++) {
                if (unit->etherId[slot] == 0) {
                    __builtin_memcpy(slotId, &sortRow[MenuWork_UP3.listSelect],
                                     sizeof(unit->etherId[slot]));
                    unit->capUsed = (unsigned char)unit->capUsed +
                        (unsigned char)rows[MenuWork_UP3.listSelect].amount;
                    break;
                }
            }
        }
    }
    for (slot = 0; slot < 11; slot++) {
        if (unit->etherId[slot] == 0) {
            int nextSlot;
            for (nextSlot = slot + 1; nextSlot < 12; nextSlot++) {
                if (unit->etherId[nextSlot] != 0) {
                    unit->etherId[slot] = unit->etherId[nextSlot];
                    unit->etherId[nextSlot] = 0;
                    break;
                }
            }
            if (nextSlot == 12) {
                break;
            }
        }
    }
    return 1;
}

typedef struct EtherStatusME {
    unsigned char unmodeled_00[2];
    short flags;
    short capCost;
    short tecId;
    short pointCost;
} EtherStatusME;
typedef struct TecStatusME {
    unsigned char unmodeled_00;
    signed char epCost;
    unsigned char unmodeled_02[2];
} TecStatusME;
typedef struct EtherUnitRecordME {
    unsigned char unmodeled_00[0x17];
    signed char capUsed;
    unsigned char unmodeled_18[0x1E];
    short ep;
    unsigned char unmodeled_38[0x56];
    short etherId[12];
} EtherUnitRecordME;
typedef struct EtherCharRecordME {
    unsigned char unmodeled_00[0x10];
    int points;
    unsigned char unmodeled_14[0x14];
    unsigned char etherMask[10];
} EtherCharRecordME;
typedef struct MenuEtherWorkME {
    unsigned char unmodeled_00[0x03];
    unsigned char state;
    unsigned char unmodeled_04[0x10 - 0x04];
    signed char charDenied;
    unsigned char unmodeled_11[0x20 - 0x11];
    unsigned char flags;
    unsigned char nextState;
    unsigned char unmodeled_22[0x2A - 0x22];
    unsigned char wait;
    unsigned char unmodeled_2b[0x30 - 0x2B];
    signed char commandCursor;
    signed char menuCursor;
    signed char slotCursor;
    signed char slotCount;
    signed char confirmCursor;
    unsigned char unmodeled_35[0x40 - 0x35];
    signed char chrNo;
    signed char partyCount;
    signed char partyCursor;
    signed char targetCursor;
    signed char etherId;
    signed char slotEther;
    signed char targetChrNo;
    signed char useType;
    signed char targetCount;
    unsigned char unmodeled_49[0x50 - 0x49];
    int listSelect;
    unsigned char unmodeled_54[0x60 - 0x54];
    unsigned char targetList[8];
} MenuEtherWorkME;
typedef struct EtherMenuPadData {
    unsigned char unmodeled_00[0x2A];
    unsigned short pressed;
    unsigned char unmodeled_2c[0x34 - 0x2C];
    unsigned short repeat;
} EtherMenuPadData;
typedef struct EtherListRowME {
    const char *name;
    int amount;
    signed char flag;
    signed char status;
    unsigned char unmodeled_0a[2];
} EtherListRowME;
typedef struct EtherPartySlot {
    unsigned short party_id;
    unsigned char unmodeled_02[0x02];
} EtherPartySlot;
typedef struct EtherPartyData {
    unsigned char unmodeled_00[0x30];
    EtherPartySlot attack_slots[3];
} EtherPartyData;
typedef struct EtherListSPWindow {
    unsigned char unmodeled_00[0x26];
    unsigned char state;
} EtherListSPWindow;
typedef struct EtherListWork {
    unsigned char state;
    unsigned char unmodeled_01[0x0C - 0x01];
    EtherListSPWindow window;
} EtherListWork;
typedef struct EtherL1R1Work {
    unsigned char state; /* +0x00 */
    unsigned char unmodeled_01;
    signed char slideOffset[2]; /* +0x02: added into sprite[i].x while non-zero */
    int spriteWork; /* +0x04: initial work value of both sprites */
    EtherL1R1Sprite sprite[2]; /* +0x08: L1, then R1 */
} EtherL1R1Work;
#define ETHER_DATA_SIZE    0x800
#define ETHER_PAS_SIZE     0x3B0
#define ETHER_INFO_SIZE    0x450
#define ETHER_STATUS_SIZE  0x4EF4
#define ETHER_STATUS2_SIZE 0x4F64
#define ETHER_MENU_SIZE    0xF20
#define ETHER_LIST_SIZE    0x343C
#define ETHER_LIST2_SIZE   0x43C
#define ETHER_EX_SIZE      0x28C
#define ETHER_L1R1_SIZE    0x58
extern EtherScreenWork *MenuEtherStatus;
extern EtherScreenWork *MenuEtherStatus2;
extern EtherListWork *MenuEtherList;
extern EtherScreenWork *MenuEtherEx;
extern EtherL1R1Work *MenuEtherL1R1;
extern short MenuEtherParty[16];
extern int MainMenuWorkEnd;
extern char D_004C8870[];
extern void xglCdReadFile(const char *name, void *buf, int offset, int size);
extern void xglSoundEffectNormalID(int id, int arg);
extern void MenuWorkEndCheck(unsigned char *p);
extern int EtherTreeInit(int heap, void *buf);
extern void EtherTreeMain(void);
extern void MenuEtherCapSet(void);
extern void MenuEtherPasMain(void);
extern void MenuEtherInfoMain(void);
extern void MenuEtherStatusMain(void);
extern void MenuEtherStatusMain2(void);
extern void MenuEtherMenuMain(void);
extern void MenuEtherListMain(void);
extern void MenuEtherListMain2(void);
extern void MenuEtherExMain(void);
extern void MenuEtherL1R1Main(void);
extern int MenuEtherEquip(void);
extern void MenuEtherListMake00(void);
extern void MenuEtherListMake01(void);
extern void MenuEtherListMake02(int list);
extern void MenuEtherListMake03(int list);
extern int MenuEtherUseCheck(unsigned short id);
extern int MenuEtherJoutoCheck(int id);
extern int MenuEtherWhoCheck(int id);
extern void *MenuEtherDataGet(int id);
extern void MenuKeepSelectReset(void);
extern int MenuCursorKeepCheck(void);
extern int MenuSortGet(int list, int index);
extern int MenuMaryIdChange(int id);
extern void *PartyDataGet(void);
extern int PartyFriendLockCheck(int id, int flag);
extern int PartyAttackerCheck(int id);
extern int MenuMainCharCheck(int chrNo);
extern int MenuCharHpCheck(int chrNo);
extern int MenuSelectMove(int cursor, int count, int wrap);
extern int WindowSPSelect(void *window, int repeat);
extern void WindowSPKeepSelect(void *window, unsigned char *keep);
extern void WindowSPKeepSelectCheck(unsigned char *keep);
extern void WindowSPSelectJump(void *window, int index);
extern void WindowSPItemChange(void *window);
extern void func_00A11270(int chrNo, int targetChrNo, int etherId);
extern void func_A194E0(int chrNo, int etherId);
extern int func_A19578(int chrNo, int etherId);
extern void ChangeTopLevel(int level);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

void MenuEther(void)
{
    extern MenuEtherWorkME MenuWorkME __asm__("MenuWork");
    extern EtherMenuPadData PadDataME __asm__("PadData");
    extern unsigned char D_004DB300[];
    extern char D_004DB308[];
    extern char D_004DB310[];
    extern char D_004DB318[];
    extern char D_004C8890[];
    extern char D_004DB320[];
    extern char D_004DB328[];
    extern char D_004DB330[];
    extern char D_004DB338[];
    extern char D_004DB340[];
    extern int MenuEtherUseCheckS(short id) __asm__("MenuEtherUseCheck");
    extern EtherUnitRecordME *ld191c0_me(int) __asm__("func_A191C0");
    extern EtherCharRecordME *ld19210_me(int) __asm__("func_A19210");
    extern EtherStatusME *ld1a488_me(int) __asm__("func_A1A488");
    extern TecStatusME *ld1a378_me(int) __asm__("func_A1A378");

    if (MenuWorkME.state == 0) {
        int heap;
        EtherPartySlot *slot;
        int i;
        int id;

        heap = MainMenuWorkEnd;
        MenuEtherDataBuf = (unsigned char *)((heap + 0x7FF) & ~0x7FF);
        xglCdReadFile(D_004C8870, MenuEtherDataBuf, 0, 1);
        heap = (int)MenuEtherDataBuf + ETHER_DATA_SIZE;
        MenuEtherPas = (EtherPasWork *)heap;
        heap = (int)MenuEtherPas + ETHER_PAS_SIZE;
        MenuEtherPas->state = 0;
        MenuEtherInfo = (EtherScreenWork *)heap;
        heap = (int)MenuEtherInfo + ETHER_INFO_SIZE;
        MenuEtherInfo->state = 0;
        MenuEtherStatus = (EtherScreenWork *)heap;
        heap = (int)MenuEtherStatus + ETHER_STATUS_SIZE;
        MenuEtherStatus->state = 0;
        MenuEtherStatus2 = (EtherScreenWork *)heap;
        heap = (int)MenuEtherStatus2 + ETHER_STATUS2_SIZE;
        MenuEtherStatus2->state = 0;
        MenuEtherMenu = (EtherMenuScreen *)heap;
        heap = (int)MenuEtherMenu + ETHER_MENU_SIZE;
        MenuEtherMenu->state = 0;
        MenuEtherList = (EtherListWork *)heap;
        heap = (int)MenuEtherList + ETHER_LIST_SIZE;
        MenuEtherList->state = 0;
        MenuEtherList2 = (EtherScreenWork *)heap;
        heap = (int)MenuEtherList2 + ETHER_LIST2_SIZE;
        MenuEtherList2->state = 0;
        MenuEtherEx = (EtherScreenWork *)heap;
        heap = (int)MenuEtherEx + ETHER_EX_SIZE;
        MenuEtherEx->state = 0;
        MenuEtherL1R1 = (EtherL1R1Work *)heap;
        heap = (int)MenuEtherL1R1 + ETHER_L1R1_SIZE;
        MenuEtherL1R1->state = 0;
        MenuWorkEndCheck((unsigned char *)EtherTreeInit(heap, MenuEtherDataBuf));
        MenuWorkME.nextState = 0x10;
        MenuWorkME.wait = 12;
        MenuWorkME.flags = 0;
        MenuWorkME.partyCursor = 0;
        MenuWorkME.partyCount = 0;
        MenuEtherCapSet();
        MenuKeepSelectReset();
        slot = ((EtherPartyData *)PartyDataGet())->attack_slots;
        for (i = 0; i < 3; i++, slot++) {
            id = slot->party_id;
            if (id != 0) {
                MenuEtherParty[MenuWorkME.partyCount] = MenuMaryIdChange(id);
                MenuWorkME.partyCount++;
            }
        }
        for (id = 1; id < 8; id++) {
            if (PartyFriendLockCheck(id, 1) != 0 && PartyAttackerCheck(id) == 0) {
                MenuEtherParty[MenuWorkME.partyCount] = MenuMaryIdChange(id);
                MenuWorkME.partyCount++;
            }
        }
    }
    if (MenuWorkME.state != MenuWorkME.nextState) {
        MenuWorkME.state = MenuWorkME.nextState;
        MenuWorkME.flags |= 0x01;
    } else {
        MenuWorkME.flags &= ~0x01;
    }
    if (MenuWorkME.wait != 0) {
        MenuWorkME.wait--;
    }
    switch (MenuWorkME.state) {
    /* Pick the character. */
    case 0x10:
        if (MenuWorkME.flags & 0x01) {
            MenuWorkME.commandCursor = 0;
            MenuWorkME.chrNo = MenuEtherParty[MenuWorkME.partyCursor];
        }
        if (MenuWorkME.wait != 0) {
            break;
        }
        if (PadDataME.repeat & (0x1000 | 0x2000 | 0x4000 | 0x8000)) {
            if (MenuWorkME.partyCount == 1) {
                break;
            }
            switch (PadDataME.repeat) {
            case 0x1000:
                MenuWorkME.partyCursor -= 2;
                break;
            case 0x4000:
                MenuWorkME.partyCursor += 2;
                break;
            case 0x8000:
                MenuWorkME.partyCursor -= 1;
                break;
            case 0x2000:
                MenuWorkME.partyCursor += 1;
                break;
            }
            if (MenuWorkME.partyCursor < 0) {
                MenuWorkME.partyCursor += MenuWorkME.partyCount;
            }
            if (MenuWorkME.partyCursor >= MenuWorkME.partyCount) {
                MenuWorkME.partyCursor -= MenuWorkME.partyCount;
            }
            MenuWorkME.chrNo = MenuEtherParty[MenuWorkME.partyCursor];
            xglSoundEffectNormalID(3, 0);
        } else if (PadDataME.pressed & 0x0020) {
            if (MenuMainCharCheck(MenuWorkME.chrNo) != 0) {
                MenuWorkME.nextState = 0x20;
                MenuWorkME.wait = 24;
                xglSoundEffectNormalID(1, 0);
                MenuWorkME.charDenied = 0;
            } else {
                xglSoundEffectNormalID(5, 0);
                MenuWorkME.charDenied = 1;
            }
        } else if (PadDataME.pressed & 0x0040) {
            MenuWorkME.nextState = 0xF0;
            xglSoundEffectNormalID(2, 0);
        }
        break;

    /* The command menu: use, equip, transfer or back. */
    case 0x20:
        if (MenuWorkME.flags & 0x01) {
            MenuWorkME.targetCursor = 0;
        }
        if (MenuWorkME.wait != 0) {
            break;
        }
        if (PadDataME.pressed & 0x0004) {
            int tries;

            for (tries = 0; tries < MenuWorkME.partyCount; tries++) {
                if (MenuWorkME.partyCursor == 0) {
                    MenuWorkME.partyCursor = MenuWorkME.partyCount - 1;
                } else {
                    MenuWorkME.partyCursor--;
                }
                MenuWorkME.chrNo = MenuEtherParty[MenuWorkME.partyCursor];
                if (MenuMainCharCheck(MenuWorkME.chrNo) != 0) {
                    break;
                }
            }
            xglSoundEffectNormalID(3, 0);
            MenuKeepSelectReset();
        } else if (PadDataME.pressed & 0x0008) {
            int tries;

            for (tries = 0; tries < MenuWorkME.partyCount; tries++) {
                if (MenuWorkME.partyCursor == MenuWorkME.partyCount - 1) {
                    MenuWorkME.partyCursor = 0;
                } else {
                    MenuWorkME.partyCursor++;
                }
                MenuWorkME.chrNo = MenuEtherParty[MenuWorkME.partyCursor];
                if (MenuMainCharCheck(MenuWorkME.chrNo) != 0) {
                    break;
                }
            }
            xglSoundEffectNormalID(3, 0);
            MenuKeepSelectReset();
        }
        {
            int cursor = MenuWorkME.commandCursor;

            MenuWorkME.commandCursor = MenuSelectMove(cursor, 4, 0);
            if (cursor != MenuWorkME.commandCursor) {
                break;
            }
            if (PadDataME.pressed & 0x0020) {
                unsigned char commandState[4];

                __builtin_memcpy(commandState, D_004DB300, 4);

                MenuWorkME.nextState = commandState[cursor];
                MenuWorkME.targetChrNo = MenuEtherParty[0];
                MenuWorkME.wait = 12;
                if (cursor == 3) {
                    MenuKeepSelectReset();
                    xglSoundEffectNormalID(2, 0);
                } else {
                    xglSoundEffectNormalID(1, 0);
                }
            }
            if (PadDataME.pressed & 0x0040) {
                MenuWorkME.nextState = 0x10;
                MenuWorkME.wait = 12;
                xglSoundEffectNormalID(2, 0);
                MenuKeepSelectReset();
            }
        }
        break;

    /* Use: pick the ether. */
    case 0x30:
        if (MenuWorkME.flags & 0x01) {
            MenuSortSet(0, 0x80, MenuWorkME.chrNo + 0x10000);
            MenuEtherListMake00();
        }
        if (MenuWorkME.wait != 0) {
            break;
        }
        MenuWorkME.listSelect = WindowSPSelect(&MenuEtherList->window, PadDataME.repeat);
        WindowSPKeepSelect(&MenuEtherList->window, &MenuKeepSelect[0]);
        if (PadDataME.pressed & 0x0020) {
            if (MenuWorkME.listSelect >= 0) {
                short etherId = MenuSortGet(0, MenuWorkME.listSelect);

                MenuWorkME.useType = MenuEtherUseCheckS(etherId);
                if (MenuWorkME.useType != 0) {
                    EtherUnitRecordME *unit;

                    etherId = MenuSortGet(0, MenuWorkME.listSelect);
                    unit = ld191c0_me(MenuWorkME.chrNo);
                    if (func_00A11220(MenuWorkME.chrNo, etherId) <= unit->ep) {
                        MenuWorkME.nextState = 0x32;
                        xglSoundEffectNormalID(1, 0);
                    } else {
                        xglSoundEffectNormalID(5, 0);
                    }
                } else {
                    xglSoundEffectNormalID(5, 0);
                }
            }
        } else if (PadDataME.pressed & 0x0040) {
            if (MenuCursorKeepCheck() == 0) {
                MenuWorkME.commandCursor = 0;
            }
            WindowSPKeepSelectCheck(&MenuKeepSelect[0]);
            MenuWorkME.nextState = 0x20;
            MenuWorkME.wait = 12;
            xglSoundEffectNormalID(2, 0);
        }
        break;

    /* Use: pick who the ether is used on. */
    case 0x32:
        if (MenuWorkME.wait != 0) {
            break;
        }
        if (MenuWorkME.useType == 1 || MenuWorkME.useType == 3) {
            if (PadDataME.repeat & 0x1000) {
                if (MenuWorkME.targetCursor != 0) {
                    xglSoundEffectNormalID(3, 0);
                    MenuWorkME.targetCursor--;
                }
                MenuWorkME.targetChrNo = MenuEtherParty[MenuWorkME.targetCursor];
            }
            if (PadDataME.repeat & 0x4000) {
                if (MenuWorkME.targetCursor != MenuWorkME.partyCount - 1) {
                    xglSoundEffectNormalID(3, 0);
                    MenuWorkME.targetCursor++;
                }
                MenuWorkME.targetChrNo = MenuEtherParty[MenuWorkME.targetCursor];
            }
        }
        MenuWorkME.targetChrNo = MenuEtherParty[MenuWorkME.targetCursor];
        if (PadDataME.pressed & 0x0020) {
            int user = MenuWorkME.chrNo;
            short etherId = MenuSortGet(0, MenuWorkME.listSelect);
            EtherUnitRecordME *unit;

            if (MenuWorkME.useType == 1) {
                if (MenuCharHpCheck(MenuWorkME.targetChrNo) == 0) {
                    func_00A11270(user, MenuWorkME.targetChrNo, etherId);
                    xglSoundEffectNormalID(1, 0);
                } else {
                    xglSoundEffectNormalID(5, 0);
                }
            } else if (MenuWorkME.useType == 3) {
                if (MenuWorkME.chrNo == MenuWorkME.targetChrNo && MenuCharHpCheck(MenuWorkME.chrNo) == 0) {
                    func_00A11270(user, MenuWorkME.targetChrNo, etherId);
                    xglSoundEffectNormalID(1, 0);
                } else {
                    xglSoundEffectNormalID(5, 0);
                }
            } else {
                EtherPartyData *party = (EtherPartyData *)PartyDataGet();

                if (MenuCharHpCheck(-1) == 0) {
                    if (party->attack_slots[0].party_id != 0) {
                        func_00A11270(user, party->attack_slots[0].party_id, etherId);
                    }
                    xglSoundEffectNormalID(1, 0);
                } else {
                    xglSoundEffectNormalID(5, 0);
                }
            }
            unit = ld191c0_me(MenuWorkME.chrNo);
            etherId = MenuSortGet(0, MenuWorkME.listSelect);
            if (unit->ep < ld1a378_me(ld1a488_me(etherId)->tecId)->epCost) {
                MenuWorkME.nextState = 0x30;
            }
            MenuEtherListMake00();
        }
        if (PadDataME.pressed & 0x0040) {
            xglSoundEffectNormalID(2, 0);
            MenuWorkME.nextState = 0x30;
        }
        break;

    /* Equip: pick the ether to equip or remove. */
    case 0x50:
        if (MenuWorkME.flags & 0x01) {
            MenuSortSet(0, 0x80, MenuWorkME.chrNo);
            MenuEtherListMake01();
        }
        if (MenuWorkME.wait == 0) {
            MenuWorkME.listSelect = WindowSPSelect(&MenuEtherList->window, PadDataME.repeat);
            WindowSPKeepSelect(&MenuEtherList->window, &MenuKeepSelect[5]);
            if ((PadDataME.pressed & 0x0080) && MenuWorkME.listSelect >= 0) {
                int start = MenuWorkME.listSelect + 1;
                int count = MenuSortCheck(0);
                EtherListRowME *rows = (EtherListRowME *)MenuListGet(0);
                int i;
                int row = 0;

                for (i = 0; i < count; i++) {
                    row = (start + i) % count;
                    if (rows[row].status != 0) {
                        break;
                    }
                }
                WindowSPSelectJump(&MenuEtherList->window, row);
                xglSoundEffectNormalID(1, 0);
            }
            if ((PadDataME.repeat & 0x0020) && MenuWorkME.listSelect >= 0) {
                MenuWorkME.etherId = MenuSortGet(0, MenuWorkME.listSelect);
                MenuEtherEquip();
                MenuEtherListMake01();
                xglSoundEffectNormalID(1, 0);
            }
            if (PadDataME.pressed & 0x0040) {
                xglSoundEffectNormalID(2, 0);
                MenuWorkME.nextState = 0x20;
                MenuWorkME.wait = 12;
                if (MenuCursorKeepCheck() == 0) {
                    MenuWorkME.commandCursor = 0;
                }
                WindowSPKeepSelectCheck(&MenuKeepSelect[5]);
            }
        }
        MenuWorkME.listSelect = WindowSPSelect(&MenuEtherList->window, 0);
        if (MenuWorkME.listSelect >= 0) {
            MenuWorkME.etherId = MenuSortGet(0, MenuWorkME.listSelect);
        }
        break;

    /* Transfer: pick the ether. */
    case 0x70:
        if (MenuWorkME.flags & 0x01) {
            MenuSortSet(0, 0x80, MenuWorkME.chrNo);
            MenuEtherListMake02(0);
            MenuWorkME.menuCursor = 0;
            MenuWorkME.slotCount = 0;
            MenuWorkME.etherId = 0;
        }
        if (MenuWorkME.wait == 0) {
            MenuWorkME.listSelect = WindowSPSelect(&MenuEtherList->window, PadDataME.repeat);
            WindowSPKeepSelect(&MenuEtherList->window, &MenuKeepSelect[10]);
            if (MenuWorkME.listSelect >= 0) {
                unsigned char *slots = MenuEtherDataGet(MenuSortGet(0, MenuWorkME.listSelect));
                int i;

                MenuWorkME.slotCount = 0;
                for (i = 0; i < 3; i++) {
                    if (slots[i] != 0) {
                        MenuWorkME.slotCount++;
                    }
                }
                MenuWorkME.etherId = MenuSortGet(0, MenuWorkME.listSelect);
            }
            if (PadDataME.pressed & 0x0020) {
                if (MenuWorkME.listSelect >= 0) {
                    int owner = MenuEtherWhoCheck(MenuWorkME.etherId);

                    if (MenuWorkME.chrNo == owner) {
                        MenuWorkME.nextState = 0x72;
                        MenuWorkME.wait = 8;
                        xglSoundEffectNormalID(1, 0);
                    } else {
                        xglSoundEffectNormalID(5, 0);
                    }
                }
            }
            if (PadDataME.pressed & 0x0040) {
                xglSoundEffectNormalID(2, 0);
                MenuWorkME.nextState = 0x20;
                MenuWorkME.wait = 12;
                if (MenuCursorKeepCheck() == 0) {
                    MenuWorkME.commandCursor = 0;
                }
                WindowSPKeepSelectCheck(&MenuKeepSelect[10]);
            }
        }
        MenuWorkME.listSelect = WindowSPSelect(&MenuEtherList->window, 0);
        if (MenuWorkME.listSelect >= 0) {
            MenuWorkME.etherId = MenuSortGet(0, MenuWorkME.listSelect);
        }
        break;

    /* Transfer: the command for the ether (slot, give, back). */
    case 0x72:
        if (MenuWorkME.wait != 0) {
            break;
        }
        MenuWorkME.menuCursor = MenuSelectMove(MenuWorkME.menuCursor, 3, 0);
        if (PadDataME.pressed & 0x0020) {
            short cost = ld1a488_me(MenuWorkME.etherId)->pointCost;
            int points = ld19210_me(MenuWorkME.chrNo)->points;
            int owner = MenuEtherWhoCheck(MenuWorkME.etherId);

            switch (MenuWorkME.menuCursor) {
            case 0:
            case 1:
                if (MenuWorkME.chrNo != owner) {
                    xglSoundEffectNormalID(5, 0);
                } else {
                    switch (MenuWorkME.menuCursor) {
                    case 0:
                        if (points >= cost && MenuWorkME.slotCount > 0) {
                            MenuWorkME.nextState = 0x74;
                            MenuWorkME.wait = 8;
                            xglSoundEffectNormalID(1, 0);
                        } else {
                            xglSoundEffectNormalID(5, 0);
                        }
                        break;
                    case 1:
                        if (points >= cost / 2 && MenuEtherJoutoCheck(MenuWorkME.etherId) != 0
                            && MenuWorkME.partyCount >= 2) {
                            MenuWorkME.nextState = 0x82;
                            MenuWorkME.targetCursor = 0;
                            MenuWorkME.wait = 12;
                            xglSoundEffectNormalID(1, 0);
                        } else {
                            xglSoundEffectNormalID(5, 0);
                        }
                        break;
                    }
                }
                break;
            case 2:
                MenuWorkME.nextState = 0x70;
                MenuWorkME.wait = 4;
                xglSoundEffectNormalID(2, 0);
                break;
            }
        }
        if (PadDataME.pressed & 0x0040) {
            xglSoundEffectNormalID(2, 0);
            MenuWorkME.nextState = 0x70;
            MenuWorkME.wait = 4;
        }
        break;

    /* Transfer to a slot: pick the slot. */
    case 0x74:
        if (MenuWorkME.flags & 0x01) {
            unsigned char *slots = MenuEtherDataGet(MenuWorkME.etherId);

            MenuWorkME.slotEther = slots[0];
            MenuWorkME.slotCursor = 0;
        }
        if (MenuWorkME.wait != 0) {
            break;
        }
        {
            unsigned char *slots = MenuEtherDataGet(MenuWorkME.etherId);

            MenuWorkME.slotCursor = MenuSelectMove(MenuWorkME.slotCursor, MenuWorkME.slotCount, 0);
            MenuWorkME.slotEther = slots[MenuWorkME.slotCursor];
        }
        if (PadDataME.pressed & 0x0020) {
            if (func_A19578(MenuWorkME.chrNo, MenuWorkME.slotEther) == 0) {
                short cost = ld1a488_me(MenuWorkME.etherId)->pointCost;

                if (ld19210_me(MenuWorkME.chrNo)->points >= cost) {
                    MenuWorkME.nextState = 0x76;
                    xglSoundEffectNormalID(1, 0);
                }
            } else {
                xglSoundEffectNormalID(5, 0);
            }
        }
        if (PadDataME.pressed & 0x0040) {
            if (MenuCursorKeepCheck() == 0) {
                MenuWorkME.menuCursor = 0;
            }
            MenuWorkME.nextState = 0x72;
            xglSoundEffectNormalID(2, 0);
        }
        break;

    /* Transfer to a slot: confirm. */
    case 0x76:
        if (MenuWorkME.flags & 0x01) {
            MenuWorkME.confirmCursor = 0;
        }
        if (MenuWorkME.wait != 0) {
            break;
        }
        MenuWorkME.confirmCursor = MenuSelectMove(MenuWorkME.confirmCursor, 2, 0);
        if (PadDataME.pressed & 0x0020) {
            if (MenuWorkME.confirmCursor == 0) {
                EtherCharRecordME *character;

                func_A194E0(MenuWorkME.chrNo, ((unsigned char *)MenuEtherDataGet(MenuWorkME.etherId))[MenuWorkME.slotCursor]);
                character = ld19210_me(MenuWorkME.chrNo);
                character->points -= (short)ld1a488_me(MenuWorkME.etherId)->pointCost;
                MenuSortSet(0, 0x80, MenuWorkME.chrNo);
                MenuEtherListMake02(0);
                WindowSPItemChange(&MenuEtherList->window);
                MenuEtherList->window.state = 4;
                xglSoundEffectNormalID(1, 0);
                MenuWorkME.nextState = 0x78;
            } else {
                xglSoundEffectNormalID(2, 0);
                MenuWorkME.nextState = 0x74;
            }
        }
        if (PadDataME.pressed & 0x0040) {
            xglSoundEffectNormalID(2, 0);
            MenuWorkME.nextState = 0x74;
        }
        break;

    case 0x78:
        if (MenuWorkME.wait == 0 && (PadDataME.pressed & 0x0020)) {
            MenuWorkME.nextState = 0x70;
        }
        break;

    /* Give to a party member: pick who. */
    case 0x82:
        if (MenuWorkME.flags & 0x01) {
            unsigned char *target = MenuWorkME.targetList;
            int i;

            MenuWorkME.targetCount = 0;
            for (i = 0; i < MenuWorkME.partyCount; i++) {
                if (MenuEtherParty[i] != MenuWorkME.chrNo && MenuMainCharCheck(MenuEtherParty[i]) != 0) {
                    *target++ = i;
                    MenuWorkME.targetCount++;
                }
            }
        }
        if (MenuWorkME.wait != 0) {
            break;
        }
        if ((PadDataME.repeat & 0x1000) && MenuWorkME.targetCursor != 0) {
            xglSoundEffectNormalID(3, 0);
            MenuWorkME.targetCursor--;
        }
        if ((PadDataME.repeat & 0x4000) && MenuWorkME.targetCursor != MenuWorkME.targetCount - 1) {
            xglSoundEffectNormalID(3, 0);
            MenuWorkME.targetCursor++;
        }
        MenuWorkME.targetChrNo = MenuEtherParty[MenuWorkME.targetList[MenuWorkME.targetCursor]];
        if (PadDataME.pressed & 0x0020) {
            if (func_A19578(MenuWorkME.targetChrNo, MenuWorkME.etherId) == 0) {
                MenuWorkME.nextState = 0x84;
                MenuWorkME.wait = 8;
                xglSoundEffectNormalID(1, 0);
            } else {
                xglSoundEffectNormalID(5, 0);
            }
        }
        if (PadDataME.pressed & 0x0040) {
            if (MenuCursorKeepCheck() == 0) {
                MenuWorkME.menuCursor = 0;
            }
            xglSoundEffectNormalID(2, 0);
            MenuWorkME.nextState = 0x72;
        }
        break;

    /* Give to a party member: confirm. */
    case 0x84:
        if (MenuWorkME.flags & 0x01) {
            MenuEtherListMake03(1);
            MenuWorkME.confirmCursor = 0;
        }
        if (MenuWorkME.wait != 0) {
            break;
        }
        MenuWorkME.confirmCursor = MenuSelectMove(MenuWorkME.confirmCursor, 2, 0);
        if (PadDataME.pressed & 0x0020) {
            if (MenuWorkME.confirmCursor == 0) {
                EtherCharRecordME *character;
                EtherListRowME *rows;
                int count;
                int i;

                func_A194E0(MenuWorkME.targetChrNo, MenuWorkME.etherId);
                character = ld19210_me(MenuWorkME.chrNo);
                character->points -= (short)ld1a488_me(MenuWorkME.etherId)->pointCost / 2;
                rows = (EtherListRowME *)MenuListGet(1);
                count = MenuSortCheck(1);
                for (i = 0; i < count; i++) {
                    rows[i].flag = 0;
                }
                xglSoundEffectNormalID(1, 0);
                MenuWorkME.nextState = 0x86;
            } else {
                xglSoundEffectNormalID(2, 0);
                MenuWorkME.nextState = 0x82;
            }
        }
        if (PadDataME.pressed & 0x0040) {
            xglSoundEffectNormalID(2, 0);
            MenuWorkME.nextState = 0x82;
        }
        break;

    case 0x86:
        if (MenuWorkME.wait == 0 && (PadDataME.pressed & 0x0020)) {
            MenuWorkME.nextState = 0x70;
            MenuWorkME.wait = 12;
        }
        break;

    /* Leave the menu. */
    case 0xF0:
        if (MenuWorkME.flags & 0x01) {
            MenuWorkME.wait = 16;
        }
        if (MenuWorkME.wait == 0) {
            MenuWorkME.nextState = 0xFF;
        }
        break;

    case 0xFF:
        ChangeTopLevel(0);
        break;
    }
    EtherTreeMain();
    MenuEtherPasMain();
    MenuEtherInfoMain();
    MenuEtherStatusMain();
    MenuEtherStatusMain2();
    MenuEtherMenuMain();
    MenuEtherListMain();
    MenuEtherListMain2();
    MenuEtherExMain();
    MenuEtherL1R1Main();
    xglFontDebugPrintf(0, 0x40, D_004DB308);
    xglFontDebugPrintf(0, 0x50, D_004DB310, MenuWorkME.commandCursor);
    xglFontDebugPrintf(0, 0x58, D_004DB318, MenuWorkME.menuCursor);
    xglFontDebugPrintf(0, 0x60, D_004C8890, MenuWorkME.partyCursor, MenuWorkME.partyCount);
    xglFontDebugPrintf(0, 0x68, D_004DB320, MenuWorkME.targetCursor);
    xglFontDebugPrintf(0, 0x70, D_004DB328, MenuWorkME.listSelect);
    xglFontDebugPrintf(0, 0x78, D_004DB330, MenuWorkME.confirmCursor);
    xglFontDebugPrintf(0, 0x80, D_004DB338, MenuWorkME.slotCursor);
    xglFontDebugPrintf(0, 0x88, D_004DB340, MenuWorkME.slotCount);
}

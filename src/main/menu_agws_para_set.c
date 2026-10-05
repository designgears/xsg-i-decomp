#include "common.h"
#include "shared.h"
#include "menu_agws_para_set.h"

/* Sums the WAGL rating of every nonzero mounted weapon of para's three
 * equipped weapon slots. */
static short WaglGet(AgwsCharPara *para)
{
    short total;
    short weaponId;
    int slot;

    total = 0;
    for (slot = 0; slot < 3; slot++) {
        weaponId = para->weapon[slot];
        if (weaponId != 0) {
            total += func_A1A3D8(MenuRWeaponCheck2(weaponId))->wagl;
        }
    }
    return total;
}

/* Fills one parameter list record from the recalculated parameters and the
 * attack and defence buffers calcTotalParaMenu filled beside them. */
static void ParaSet(AgwsCharPara *para, AgwsParaDisplay *dest, int *attack, int *defense)
{
    dest->value[AGWS_PARA_MAX_HP] = para->maxHp;
    dest->value[AGWS_PARA_ATTACK] = para->attack + attack[0] + attack[1] + attack[2];
    dest->value[AGWS_PARA_PHY_DEFENSE] = para->phyDefense + defense[0];
    dest->value[AGWS_PARA_MAG_DEFENSE] = para->magDefense + defense[1];
    dest->value[AGWS_PARA_STAT8] = (signed char) para->stat8;
    dest->value[AGWS_PARA_WAGL] = WaglGet(para);
    dest->value[AGWS_PARA_STAT9] = para->stat9;
}

/*
 * States 0 and 10 of the AGWS screens fill the shown record too; every other
 * state only refreshes the preview. calcTotalParaMenu writes the current hp
 * of its recalculation back into the origin record, so state 10 restores the
 * hp it found there.
 */
void MenuAgwsParaSet(int chrNo, int state)
{
    int attack[4];
    int defense[4];
    AgwsUnitOrg *org;
    short keptHp;
    AgwsCharPara *para;

    if (chrNo != 0) {
        org = func_A191C0(chrNo);
        keptHp = org->hp;
        para = func_00A11108(chrNo, attack, defense);
        ParaSet(para, &MenuAgwsPara2, attack, defense);
        if (state == 0) {
            ParaSet(para, &MenuAgwsPara, attack, defense);
        }
        if (state == 10) {
            ParaSet(para, &MenuAgwsPara, attack, defense);
            org->hp = keptHp;
        }
    }
}

/*
 * The tab strip at the top of the AGWS menu. MenuWork.state selects which
 * sub-screen currently owns the display; AgwsPasMain hides the whole strip
 * unless MenuWork.state is one of its own, and slides in the root tab, the
 * command tab under the cursor and the sub-tab it opens, pushing the ones
 * before them out to the left by their own width.
 */
void AgwsPasMain(void)
{
    AgwsPasWork *pas = AgwsPas;
    int tabLength[17];
    short tabTarget[17];
    short slideTarget;
    int i;

    for (i = 0; i < 17; i++)
        tabLength[i] = MenuPasLengthGet(msg00_0_0036D940[i]);

    switch (pas->state) {
    case AGWS_PAS_START:
        pas->color = AGWS_PAS_COLOR;
        WindowDXSet(&pas->window);
        pas->window.x = AGWS_PAS_X_OUT;
        pas->window.y = 8;
        pas->window.color = pas->color;
        pas->window.width = AGWS_PAS_WIDTH;
        pas->window.height = 0x20;
        pas->window.callback = MenuPasWindow;
        pas->window.callbackArg = pas->callbackWork;
        pas->window.state = 1;
        WindowDXMain(&pas->window);
        pas->window.state = 3;
        for (i = 0; i < 17; i++) {
            eMessageSet(&pas->message[i], msg00_0_0036D940[i]);
            pas->message[i].mode = 0x20;
            pas->message[i].x = AGWS_PAS_TAB_PARKED;
            pas->message[i].y = 0xB;
            pas->message[i].color = pas->color + 2;
        }
        pas->mask = 1;
        pas->selectNo = -1;
        pas->state = AGWS_PAS_SLIDE;
        /* fallthrough */
    case AGWS_PAS_SLIDE:
        break;
    default:
        return;
    }

    slideTarget = AGWS_PAS_X_IN;
    for (i = 0; i < 17; i++)
        tabTarget[i] = AGWS_PAS_TAB_PARKED;

    {
        AgwsMenuWork *menu = &MenuWork;

        switch (menu->state) {
        case 0x10:
        case 0x20:
        case 0x22:
            tabTarget[0] = 0x10;
            break;
        case 0x44:
            tabTarget[0] = -(tabLength[0] + AGWS_PAS_TAB_GAP);
            tabTarget[menu->reserveCursor + 1] = 0x10;
            tabTarget[menu->selectedIndex + AGWS_PAS_TAB_SLOT] =
                tabLength[menu->reserveCursor + 1] + 0x10;
            break;
        case 0x46:
            tabTarget[0] = -(tabLength[0] +
                             tabLength[menu->reserveCursor + 1] +
                             AGWS_PAS_TAB_GAP);
            tabTarget[menu->reserveCursor + 1] =
                -(tabLength[menu->reserveCursor + 1] + AGWS_PAS_TAB_GAP);
            tabTarget[menu->selectedIndex + AGWS_PAS_TAB_SLOT] = 0x10;
            tabTarget[AGWS_PAS_TAB_AMMO] =
                tabLength[menu->selectedIndex + AGWS_PAS_TAB_SLOT] + 0x10;
            break;
        case 0x62:
            tabTarget[0] = -(tabLength[0] + AGWS_PAS_TAB_GAP);
            tabTarget[menu->reserveCursor + 1] = 0x10;
            tabTarget[AGWS_PAS_TAB_ACCESSORY] =
                tabLength[menu->reserveCursor + 1] + 0x10;
            break;
        case 0x40:
        case 0x42:
        case 0xC2:
            tabTarget[0] = 0x10;
            tabTarget[menu->reserveCursor + 1] = tabLength[0] + 0x10;
            break;
        default:
            slideTarget = AGWS_PAS_X_OUT;
            break;
        }
    }

    MoveSlide(&pas->window.x, &slideTarget, 3.0f);
    WindowDXMain(&pas->window);
    if (pas->window.state == 3) {
        pas->box.x = pas->window.x + 3;
        pas->box.y = pas->window.y + 3;
        pas->box.color = pas->color;
        pas->box.width = pas->window.width - 6;
        pas->box.height = pas->window.height - 6;
        endPrintExtFunc(pas->color, 0x65, &pas->box);
        for (i = 0; i < 17; i++) {
            MoveSlide(&pas->message[i].x, &tabTarget[i], 3.0f);
            if (pas->message[i].x < AGWS_PAS_TAB_LIMIT)
                eMessageMain(&pas->message[i]);
        }
        endPrintExtFunc(pas->color, 0x66, 0);
    }
}

typedef struct AgwsInfoWin {
    short x;
    short y;
    int color;
    short width;
    short height;
    unsigned char unmodeled_0c[0x10 - 0x0C];
    unsigned char state;
    unsigned char unmodeled_11[0x14 - 0x11];
    void *callback;
    void *callbackArg;
    unsigned char unmodeled_1c[0x198 - 0x1C];
} AgwsInfoWin;
typedef struct AgwsInfoWork {
    unsigned char state;
    unsigned char unk1;
    unsigned char help;
    unsigned char unk3;
    int color;
    AgwsInfoWin window;
    short cb0;
    short cb2;
    unsigned char unmodeled_1a4[0x1AC - 0x1A4];
    char *text;
    unsigned char unmodeled_1b0[0x350 - 0x1B0];
    char buf[0x100];
} AgwsInfoWork;
typedef struct AgwsInfoOrg {
    unsigned char unmodeled_00[0x5A];
    signed char mount[4];
    unsigned short wpn[3];
    short equip[3];
    unsigned short arms[3];
} AgwsInfoOrg;
typedef union AgwsInfoAny { short s; void *p; } AgwsInfoAny;
typedef struct AgwsInfoText {
    int id;
    char *name;
} AgwsInfoText;
extern AgwsInfoWork *AgwsInfo;
extern char *msg00_1_0036D988[];
extern char *msg01_2_0036D990[];
extern char *msg02_3_0036D9A0[];
extern char *msg03_4_0036D9B8[];
extern char *msg04_5[];
extern char *msg05_6[];
extern char D_004DAF28[];
extern char D_004DAF40[];
extern unsigned char D_004DAF48[];
void MenuInfoWindow(void);
void eMessageCpy(char *dst, char *src);
void eMessageCat(char *src);
AgwsInfoText *MenuTextGet(int index);
char *MenuCharNameGet(int id);
int OptMntToWpnPos(int pos);

void AgwsInfoMain(void)
{
    char *buf;
    AgwsInfoWin *win;
    AgwsInfoWork *work = AgwsInfo;
    short target;
    AgwsUnitOrg *org;
    AgwsUnitOrg *org2;
    AgwsListRow *row;
    int a;
    int b;
    int id;
    signed char sel;
    short *cb;
    char *q;

    switch (work->state) {
    case 0:
        win = &work->window;
        work->color = 0xFFFFF0;
        buf = work->buf;
        WindowDXSet(win);
        win->x = -0x10;
        work->window.y = 0x1E0;
        work->window.width = 0x220;
        work->window.height = 0x36;
        work->window.state = 0;
        work->window.callback = MenuInfoWindow;
        cb = &work->cb0;
        *cb = 0;
        work->window.color = work->color;
        work->window.state = 1;
        ((AgwsInfoAny *)&work->window.callbackArg)->p = cb;
        work->cb2 = 0;
        ((AgwsInfoAny *)&work->text)->p = 0;
        WindowDXMain(win);
        work->window.state = 3;
        work->state = 2;
        work->help = 0;
        memset(buf, 0, 0x100);
        /* fallthrough */
    case 2:
        win = &work->window;
        buf = work->buf;
        break;
    default:
        return;
    }

    org = func_A191C0(MenuWork.chrNo);
    target = 0x182;
    if (work->help == 0) {
        if (PadData.half_2a & 0x20)
            work->help = 1;
    } else if (*(unsigned short *)((char *)&PadData + 0x34) != 0) {
        work->help = 0;
    }

    switch (MenuWork.state) {
    case 0x10:
        eMessageCpy(buf, msg00_1_0036D988[0]);
        break;
    case 0x22:
        eMessageCpy(buf, msg01_2_0036D990[MenuWork.reserveCursor]);
        break;
    case 0x40:
        if (MenuWork.weaponHand != 3) {
            if (MenuWork.equipSlot == 0) {
                q = (char *)org + 0xE;
                if (*(short *)(q + MenuWork.weaponHand * 2 + 0x50) != 0) {
                    eMessageCpy(buf, msg02_3_0036D9A0[OptMntToWpnPos(((AgwsInfoOrg *)org)->mount[MenuWork.weaponHand]) - 1]);
                    eMessageCat(msg03_4_0036D9B8[0]);
                    eMessageCat(MenuTextGet(*(unsigned short *)(q + MenuWork.weaponHand * 2 + 0x50) + 0x30000)->name);
                } else if (MenuWork.reserveCursor == 0) {
                    eMessageCpy(buf, msg03_4_0036D9B8[1]);
                } else {
                    eMessageCpy(buf, msg03_4_0036D9B8[2]);
                }
            } else {
                eMessageCpy(buf, MenuTextGet(((unsigned short *)((char *)org + 0x6A))[MenuWork.weaponHand] + 0x40000)->name);
                if (MenuWork.reserveCursor == 1 && work->help == MenuWork.reserveCursor)
                    eMessageCpy(buf, msg03_4_0036D9B8[3]);
            }
        } else {
            short s = ((AgwsInfoOrg *)org)->equip[MenuWork.equipSlot];
            if (s != 0) {
                eMessageCpy(buf, MenuTextGet((unsigned short)s + 0x50000)->name);
            } else if (MenuWork.reserveCursor == 0) {
                eMessageCpy(buf, msg03_4_0036D9B8[4]);
            } else {
                eMessageCpy(buf, msg03_4_0036D9B8[2]);
            }
        }
        break;
    case 0x42:
        a = OptMntToWpnPos(((AgwsInfoOrg *)org)->mount[MenuWork.weaponHand]) - 1;
        b = MenuWork.weaponSlot[MenuWork.selectedIndex] - 1;
        if (a < 0) {
            eMessageCpy(buf, msg04_5[3]);
            eMessageCat(msg02_3_0036D9A0[b]);
            eMessageCat(D_004DAF28);
        } else if (a == b) {
            eMessageCpy(buf, msg04_5[1]);
            eMessageCat(msg02_3_0036D9A0[b]);
            eMessageCat(D_004DAF40);
            eMessageCat(msg04_5[0]);
        } else {
            eMessageCpy(buf, msg04_5[1]);
            eMessageCat(msg02_3_0036D9A0[a]);
            eMessageCat(msg04_5[2]);
            eMessageCat(msg02_3_0036D9A0[b]);
            eMessageCat(D_004DAF28);
        }
        break;
    case 0x44:
        if ((signed char)MenuWork.unmodeled_57[0] >= 0)
            eMessageCpy(buf, MenuTextGet(MenuSortGet(0, (signed char)MenuWork.unmodeled_57[0]))->name);
        else
            eMessageCpy(buf, (char *)D_004DAF48);
        break;
    case 0x46:
        if ((signed char)MenuWork.unmodeled_57[1] >= 0)
            eMessageCpy(buf, MenuTextGet(MenuSortGet(0, (signed char)MenuWork.unmodeled_57[1]))->name);
        else
            eMessageCpy(buf, (char *)D_004DAF48);
        break;
    case 0x60:
        if (MenuWork.weaponHand != 2) {
            eMessageCpy(buf, msg01_2_0036D990[5]);
            eMessageCat(msg01_2_0036D990[MenuWork.weaponHand + 2]);
        } else {
            eMessageCpy(buf, msg02_3_0036D9A0[0]);
        }
        break;
    case 0x62:
        if ((signed char)MenuWork.unmodeled_57[2] >= 0)
            eMessageCpy(buf, MenuTextGet(MenuSortGet(0, (signed char)MenuWork.unmodeled_57[2]))->name);
        else
            eMessageCpy(buf, (char *)D_004DAF48);
        break;
    case 0xC2:
        if (MenuWork.listSelect >= 0) {
        row = &MenuListGet(0)[MenuWork.listSelect];
        if ((signed char)row->flag == 1) {
            org2 = func_A191C0((short)MenuSortAddrGet(0)[MenuWork.listSelect]);
            eMessageCpy(buf, *(char **)row);
            eMessageCat(msg05_6[2]);
            eMessageCat(MenuCharNameGet(org2->agwsId));
            eMessageCat(msg05_6[3]);
        } else {
            if ((signed char)row->equipped == 0)
                eMessageCpy(buf, msg05_6[0]);
            else
                eMessageCpy(buf, msg05_6[1]);
            eMessageCat(*(char **)row);
            eMessageCat(msg05_6[4]);
        }
        } else {
            eMessageCpy(buf, (char *)D_004DAF48);
        }
        break;
    default:
        target = 0x200;
        break;
    }
    work->text = buf;
    MoveSlide(&work->window.y, &target, 3.0f);
    WindowDXMain(win);
}

typedef struct AgwsSelWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    const char *title;
    unsigned char state;
    unsigned char unmodeled_11[0x14 - 0x11];
    void *select;
    void *selectArg;
    unsigned char unmodeled_1c[0x194 - 0x1C];
} AgwsSelWindow;
typedef struct AgwsSelList {
    unsigned char unmodeled_00[0x02];
    unsigned short hasPrompt;
    int cursor;
    const char *items;
    const char *prompt;
    unsigned char unmodeled_10[0x5F8 - 0x10];
} AgwsSelList;
typedef struct AgwsSelectWork {
    unsigned char state;
    unsigned char mask;
    signed char selectNo;
    unsigned char unmodeled_03;
    int color;
    AgwsSelWindow window[4];
    unsigned char unmodeled_658[0x658 - 0x658];
    AgwsSelList list[4];
} AgwsSelectWork;
extern AgwsSelectWork *AgwsSelect;
extern const char D_004DAF80[];
extern const char D_004DAF88[];
extern const char D_004DAF90[];
extern char mmsg_9[];
extern const char *msg_7_0036D9F8[];
extern const char *msg01_8_0036DA40[];
void MenuSelectWindow(void);
char *strcat(char *dst, const char *src);

void AgwsSelectMain(void)
{
    AgwsSelectWork *work = AgwsSelect;
    short x[8];
    short y[8];
    int i;
    signed char *p;
    int c;

    switch (work->state) {
    case 0:
        work->color = 0x00FFFFF0;
        for (i = 0; i < 4; i++) {
            WindowDXSet(&work->window[i]);
            work->window[i].color = work->color;
            work->window[i].title = D_004DAF80;
            work->list[i].cursor = 0;
            work->list[i].items = msg_7_0036D9F8[1];
            work->list[i].hasPrompt = 0;
            work->window[i].select = (void *)MenuSelectWindow;
            work->window[i].selectArg = &work->list[i];
        }
        work->list[2].items = msg01_8_0036DA40[0];
        work->window[3].title = D_004DAF88;
        work->list[3].items = msg01_8_0036DA40[2];
        work->window[0].x = 0x220;
        work->window[0].y = 0xA0;
        work->window[0].width = 0xA0;
        work->window[0].height = 0x4E;
        work->window[1].x = -0xC0;
        work->window[1].y = 0x80;
        work->window[1].width = 0xA6;
        work->window[1].height = 0;
        work->window[2].x = 0x220;
        work->window[2].y = 0xB0;
        work->window[2].width = 0xB0;
        work->window[2].height = 0x60;
        work->window[3].x = 0x220;
        work->window[3].y = 0x80;
        work->window[3].width = 0xB0;
        work->window[3].height = 0x70;
        work->list[3].hasPrompt = 1;
        work->list[3].prompt = msg01_8_0036DA40[1];
        for (i = 0; i < 4; i++) {
            work->window[i].state = 1;
            WindowDXMain(&work->window[i]);
            work->window[i].state = 3;
        }
        work->mask = 1;
        work->selectNo = -1;
        work->state = 2;
        /* fallthrough */
    case 2:
        break;
    default:
        return;
    }

    y[0] = 0xA0;
    x[1] = -0xC0;
    y[2] = 0xB0;
    x[0] = 0x220;
    y[1] = 0x80;
    x[2] = 0x220;
    x[3] = 0x220;
    y[3] = 0x80;
    {

        switch (MenuWork.state) {
        case 0x22:
            x[0] = 0x110;
            work->list[0].cursor = MenuWork.reserveCursor;
            break;
        case 0x42:
            mmsg_9[0] = D_004DAF90[0];
            i = 0;
            while ((c = MenuWork.weaponSlot[i]) != 0) {
                strcat(mmsg_9, msg_7_0036D9F8[c + 10]);
                if (++i >= 6)
                    break;
            }
            work->window[1].height = (signed char)MenuWork.unmodeled_57[3] * 0x18 + 6;
            work->list[1].items = mmsg_9;
            x[1] = 0x30;
            work->list[1].cursor = MenuWork.selectedIndex;
            break;
        }
    }

    for (i = 0; i < 4; i++) {
        MoveSlide(&work->window[i].x, &x[i], 3.0f);
        MoveSlide(&work->window[i].y, &y[i], 3.0f);
        WindowDXMain(&work->window[i]);
    }
}

/* The weapon list of the AGWS mount position under the cursor: each row is
 * flagged when the weapon cannot go on that position. */
void MenuAgwsListMake_Wpn(void)
{
    int *sortRow = MenuSortAddrGet(0);
    AgwsListRow *entries = MenuListGet(0);
    AgwsListSPWindow *window = &AgwsList->window;
    int count;
    int i;

    MenuSortSet(0, 4, MenuWork.weaponSlot[MenuWork.selectedIndex] | 0x2000);
    MenuListMake(0, 0);
    count = MenuSortCheck(0);
    for (i = 0; i < count; i++) {
        if (MenuWeaponEquipPosCheck(MenuWork.chrNo, (short) sortRow[i],
                                    MenuWork.weaponSlot[MenuWork.selectedIndex],
                                    MenuWork.weaponHand) != 0) {
            entries[i].flag = 0;
        } else {
            entries[i].flag = 1;
        }
    }
    window->width = 229;
    window->rows = 7;
    window->height = 174;
    window->columns = 1;
    window->rowCount = 7;
    window->flags = 0;
    window->items = MenuListGet(0);
    WindowSPItemChange(window);
    WindowSPSetSelect(window, &MenuKeepSelect[MenuWork.weaponSlot[MenuWork.selectedIndex] * 5 - 5]);
}

/* The ammunition list: each row is flagged when its ammunition does not fit
 * the weapon under the cursor. */
void MenuAgwsListMake_Gun(void)
{
    int *sortRow = MenuSortAddrGet(0);
    AgwsListRow *entries = MenuListGet(0);
    AgwsListSPWindow *window = &AgwsList->window;
    int count;
    int i;

    MenuSortSet(0, 8, -2);
    MenuListMake(0, 0);
    count = MenuSortCheck(0);
    for (i = 0; i < count; i++) {
        if (MenuBulletCheck(MenuWork.weaponId, (short) sortRow[i]) != 0) {
            entries[i].flag = 0;
        } else {
            entries[i].flag = 1;
        }
    }
    window->width = 229;
    window->rows = 7;
    window->height = 174;
    window->columns = 1;
    window->rowCount = 7;
    window->flags = 0;
    window->items = MenuListGet(0);
    WindowSPItemChange(window);
    WindowSPSetSelect(window, &MenuKeepSelect[30]);
}

/* The accessory list: each row is flagged when the accessory may not go into
 * the slot under the cursor. */
void MenuAgwsListMake_Acc(void)
{
    int *sortRow = MenuSortAddrGet(0);
    AgwsListRow *entries = MenuListGet(0);
    AgwsListSPWindow *window = &AgwsList->window;
    int count;
    int i;

    MenuSortSet(0, 16, -2);
    MenuListMake(0, 0);
    count = MenuSortCheck(0);
    for (i = 0; i < count; i++) {
        if (MenuAccessoryEquipCheck(MenuWork.chrNo, (short) sortRow[i], MenuWork.equipSlot) != 0) {
            entries[i].flag = 0;
        } else {
            entries[i].flag = 1;
        }
    }
    window->width = 229;
    window->height = 150;
    window->columns = 1;
    window->rows = 6;
    window->rowCount = 7;
    window->flags = 0;
    window->items = MenuListGet(0);
    WindowSPItemChange(window);
    WindowSPSetSelect(window, &MenuKeepSelect[35]);
}

/* The pilot list: a row is flagged when the character already rides another
 * AGWS, and marked as equipped when the AGWS the screen shows is his. */
void MenuAgwsListMake_Pilot(void)
{
    AgwsListRow *entries = MenuListGet(0);
    int *sortRow = MenuSortAddrGet(0);
    AgwsListSPWindow *window = &AgwsList->window;
    AgwsUnitOrg *org;
    int count;
    int i;

    MenuSortSet(0, 32, 1);
    MenuListMake(0, 0);
    count = MenuSortCheck(0);
    for (i = 0; i < count; i++) {
        org = func_A191C0((short) sortRow[i]);
        if (org->agwsId != MenuWork.chrNo && org->agwsId != 0) {
            entries[i].flag = 1;
        } else {
            entries[i].flag = 0;
        }
        if (org->agwsId == MenuWork.chrNo) {
            entries[i].equipped = 1;
        } else {
            entries[i].equipped = 0;
        }
    }
    window->width = 208;
    window->height = 144;
    window->columns = 1;
    window->rows = 5;
    window->rowCount = 3;
    window->flags = (int) D_004C6F38;
    window->items = MenuListGet(0);
    WindowSPItemChange(window);
    WindowSPSetSelect(window, &MenuKeepSelect[40]);
}

typedef struct AgwsListTag {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int color;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char unmodeled_0f[0x20 - 0x0F];
} AgwsListTag;
typedef struct AgwsListWin {
    unsigned char state;
    unsigned char rowCount;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    short width;
    short height;
    int flags;
    unsigned char columns;
    unsigned char rows;
    unsigned char unmodeled_16[0x1C - 0x16];
    AgwsListRow *items;
} AgwsListWin;
typedef struct AgwsListView {
    unsigned char state;
    unsigned char mode;
    unsigned char unk2;
    unsigned char unk3;
    int color;
    AgwsListTag tag[8];
    AgwsSprite sprite;
    AgwsListWin window;
} AgwsListView;
extern unsigned char D_004DAF48[];
void WindowSPSet(void *window);
void WindowSPMain(void *window);
int PartyAttackerCheck(int party_id);
char *MenuTagTextGet(int index);
int MenuAgwsPilotCheck(int id);

void AgwsListMain(void)
{
    AgwsListView *work = (AgwsListView *)AgwsList;
    short target;
    int count;
    AgwsListRow *rows;
    int i;
    short id;
    unsigned char s;

    switch (work->state) {
    case 0:
        work->window.x = 0x210;
        work->color = 0xFFF000;
        work->window.color = 0xFFF000;
        work->window.flags = (int)D_004DAF48;
        work->window.y = 0x40;
        work->window.rowCount = 7;
        work->window.rows = 0xB;
        work->window.height = 0x10E;
        work->window.state = 0;
        work->window.items = 0;
        WindowSPSet(&work->window);
        work->mode = 1;
        work->state = 2;
        work->unk2 = 0;
        /* fallthrough */
    case 2:
        work->mode = 0;
        switch (MenuWork.state) {
        case 0x44:
            work->state = 10;
            break;
        case 0x46:
            work->state = 20;
            break;
        case 0x62:
            work->state = 30;
            break;
        case 0xC2:
            work->state = 40;
            break;
        }
        break;
    case 10:
    case 20:
    case 30:
    case 40:
        if (work->state == 40) {
            work->window.x = 0x210;
            work->window.y = 0x80;
            MenuAgwsListMake_Pilot();
            work->mode = 2;
            work->state = 41;
        } else {
            work->window.x = -0xF4;
            if (work->state == 30) {
                work->window.y = 0x7E;
            } else {
                work->window.y = 0x68;
            }
            work->mode = 1;
            if (work->state == 20) {
                work->state = 21;
            } else {
                work->state = 11;
            }
            switch (work->state) {
            case 10:
                MenuAgwsListMake_Wpn();
                break;
            case 20:
                MenuAgwsListMake_Gun();
                break;
            case 30:
                MenuAgwsListMake_Acc();
                break;
            }
        }
        work->window.state = 0x11;
        /* fallthrough */
    case 11:
    case 13:
    case 21:
    case 41:
    case 43:
        target = 0x10;
        switch (work->state) {
        case 41:
            target = 0x120;
            break;
        case 13:
            target = -0xF4;
            break;
        case 43:
            target = 0x210;
            break;
        }
        MoveSlide(&work->window.x, &target, 3.0f);
        if (work->window.x == target) {
            switch (work->state) {
            case 21:
                work->state = 0x16;
                break;
            case 11:
            case 41:
                work->state = 0xC;
                break;
            case 13:
            case 43:
                work->state = 2;
                break;
            }
        }
        break;
    case 12:
    case 22:
        switch (MenuWork.state) {
        case 0x42:
            work->state = 0xD;
            break;
        case 0x22:
            work->state = 0x2B;
            break;
        case 0x40:
            work->state = 0xD;
            break;
        case 0x46:
            if (work->state == 0xC) {
                work->state = 20;
            }
            break;
        }
        break;
    }

    if (work->mode != 0) {
        WindowSPMain(&work->window);
        if (work->mode == 2) {
            AgwsMenuWork *menu = &MenuWork;
            if (menu->listSelect >= 0) {
                count = MenuSortCheck(0);
                rows = MenuListGet(0);
                for (i = 0; i < count; i++) {
                    id = MenuSortGet(0, i);
                    if (PartyAttackerCheck(id) != 0) {
                        eTagFontSet(&work->tag[i], (int)MenuTagTextGet(1));
                    } else {
                        eTagFontSet(&work->tag[i], (int)MenuTagTextGet(2));
                    }
                    if ((signed char)rows[i].flag == 1) {
                        work->tag[i].g = 0x40;
                        work->tag[i].b = 0x40;
                        work->tag[i].r = 0x40;
                    } else {
                        work->tag[i].g = 0x80;
                        work->tag[i].b = 0x80;
                        work->tag[i].r = 0x80;
                    }
                    work->tag[i].x = work->window.x + 0x6B;
                    work->tag[i].color = work->window.color + 2;
                    work->tag[i].y = work->window.y + i * 0x18 + 7;
                    eTagFontMain(&work->tag[i]);
                    if (id == MenuAgwsPilotCheck(MenuWork.chrNo)) {
                        eSpriteSet(&work->sprite, 0x60A);
                        work->sprite.x = work->window.x + work->window.width - 0x18;
                        work->sprite.y = work->window.y + i * 0x18 + 3;
                        work->sprite.work = work->window.color + 2;
                        eSpriteMain(&work->sprite);
                    }
                }
            }
        }
    }
}

void AgwsStatusMain(void)
{
    AgwsStatusWork *status = AgwsStatus;
    int i;

    switch (status->state) {
    case AGWS_STATUS_START:
        status->color = AGWS_STATUS_COLOR;
        WindowDXSet(&status->window);
        status->window.x = AGWS_STATUS_X_OUT;
        status->window.y = 304;
        status->window.color = status->color;
        status->window.width = 480;
        status->window.height = 64;
        status->window.state = 1;
        WindowDXMain(&status->window);
        status->window.state = 3;
        for (i = 0; i < 7; i++) {
            eTagFontSet(&status->label[i], msg_10[i]);
            status->label[i].color = status->color + 2;
            status->label[i].rgba[0] = 112;
        }
        {
            unsigned char digit[7] = { 4, 3, 3, 3, 2, 2, 4 };

            for (i = 0; i < 7; i++) {
                eNumberSet(&status->number[i], 0);
                status->number[i].color = status->color + 2;
                status->number[i].format = 5;
                status->number[i].digits = digit[i];
            }
        }
        status->selectNo = -1;
        status->state = AGWS_STATUS_SLIDE;
        status->mask = 0;
        /* fallthrough */
    case AGWS_STATUS_SLIDE:
    {
        short slideTarget = AGWS_STATUS_X_OUT;

        switch (MenuWork.state) {
        case 0x40:
            slideTarget = AGWS_STATUS_X_IN;
            if (MenuWork.reserveCursor == 1) {
                status->mask = 1;
            } else {
                status->mask = 0;
            }
            break;
        case 0x22:
            slideTarget = AGWS_STATUS_X_IN;
            break;
        case 0x42:
        case 0x44:
        case 0x46:
        case 0x62:
        case 0xC2:
            status->mask = 0;
            slideTarget = AGWS_STATUS_X_IN;
            break;
        }
        MoveSlide(&status->window.x, &slideTarget, 3.0f);
        WindowDXMain(&status->window);
        if (status->window.state == 3) {
            for (i = 0; i < 6; i++) {
                int rows = 3;

                status->label[i].x = status->window.x + (i / rows) * 160 + 48;
                status->label[i].y = status->window.y + (i % rows) * 20 + 3;
                eTagFontMain(&status->label[i]);
            }
            for (i = 0; i < 6; i++) {
                status->number[i].value =
                    MenuAgwsPara.value[i] + (MenuAgwsPara2.value[i] << 16);
            }
            for (i = 0; i < 6; i++) {
                unsigned short shown[2];

                shown[0] = status->number[i].value;
                shown[1] = status->number[i].value >> 16;
                if (shown[0] < shown[1]) {
                    status->number[i].arrow = 2;
                    status->number[i].arrowHidden = 0;
                } else if (shown[1] < shown[0]) {
                    status->number[i].arrow = 1;
                    status->number[i].arrowHidden = 0;
                } else {
                    status->number[i].arrow = 0;
                    status->number[i].arrowHidden = 1;
                }
                {
                    int rows = 3;

                    status->number[i].x =
                        status->window.x + (i / rows) * 160 + 100;
                    status->number[i].y =
                        status->window.y + (i % rows) * 20 + 3;
                }
                eNumberMain(&status->number[i]);
            }
        }
        break;
    }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/menu_agws_para_set", AgwsWeapon2Main);

INCLUDE_ASM("asm/main/nonmatchings/menu_agws_para_set", AgwsNameMain);

/* The pilot face the AGWS pilot list shows: the window slides in while the
 * list is open and carries the face sprite of the row under the cursor. */
void AgwsFaceMain(void)
{
    AgwsFaceWork *self = AgwsFace;
    short target;

    switch (self->state) {
    case 0:
        self->color = 0x00FFFFF0;
        WindowDXSet(&self->window);
        self->window.x = -103;
        self->window.color = self->color;
        self->window.y = 128;
        self->window.width = 71;
        self->window.height = 100;
        self->window.state = 1;
        WindowDXMain(&self->window);
        self->window.state = 3;
        eSpriteSet(&self->face, 0);
        self->visible = 0;
        self->state = 2;
        break;
    case 2:
        break;
    default:
        return;
    }

    target = -103;
    if (MenuWork.state == 0xC2) {
        if (MenuWork.flags & 1) {
            self->visible = 0;
        } else {
            self->visible = 1;
        }
        target = 208;
    }
    if (self->visible != 0) {
        MoveSlide(&self->window.x, &target, 3.0f);
        if (self->window.x == -103) {
            self->visible = 0;
        }
        WindowDXMain(&self->window);
        if (MenuWork.listSelect >= 0) {
            eSpriteSet(&self->face, MenuFaceEpidGet(MenuSortGet(0, MenuWork.listSelect), 0));
            self->face.x = self->window.x + 3;
            self->face.y = self->window.y + 3;
            self->face.work = self->window.color + 2;
            eSpriteMain(&self->face);
        }
    }
}

/* The L1/R1 page hints of the AGWS screens: both sprites sit just off screen
 * unless the screen is on the page that lets the pad page through. */
void AgwsSwitchMain(void)
{
    AgwsSwitchWork *self = AgwsSwitch;
    int i;

    switch (self->state) {
    case 0:
        self->spriteWork = 0x00FFFFFF;
        eSpriteSet(&self->sprite[0], 0x112);
        eSpriteSet(&self->sprite[1], 0x110);
        self->sprite[0].x = -45;
        self->sprite[1].x = 557;
        for (i = 0; i < 2; i++) {
            self->sprite[i].work = self->spriteWork;
            self->sprite[i].y = 224;
        }
        self->slideOffset[1] = 0;
        self->state = 2;
        self->slideOffset[0] = 0;
        break;
    case 2:
        break;
    default:
        return;
    }

    {
        short target[2];

        target[0] = -45;
        target[1] = 557;
        if (MenuWork.state == 34) {
            target[0] = 16;
            target[1] = 467;
            if (PadData.half_2a & PAD_L1) {
                self->slideOffset[0] = -6;
            } else if (PadData.half_2a & PAD_R1) {
                self->slideOffset[1] = 6;
            }
        }

        {
            AgwsSwitchSlideStep step = D_004DAFE8[0];

            for (i = 0; i < 2; i++) {
                if (self->slideOffset[i] != 0) {
                    self->slideOffset[i] = self->slideOffset[i] + step.side[i];
                }
            }
        }

        for (i = 0; i < 2; i++) {
            MoveSlide(&self->sprite[i].x, &target[i], 3.0f);
            self->sprite[i].x = self->sprite[i].x + self->slideOffset[i];
            eSpriteMain(&self->sprite[i]);
        }
    }
}

/* Places the AGWS model camera at its preset position and rotation. */
void MenuAgwsCameraSet(void)
{
    AgwsCameraPreset preset = D_004C7190;
    StudioCamera *camera;

    camera = xglStudioGetCamera2(0);
    xglCameraInit(camera);
    camera->position = preset.vectors[0];
    camera->state = 1;
    camera->rotation = preset.vectors[1];
}

/* Eases the menu camera towards the position the AGWS screen state asks for. */
void MenuAgwsCameraMove(void)
{
    Vector4 *position = &xglStudioGetCamera2(0)->position;
    AgwsCameraTargets targets = D_004C71B0;
    int target;

    switch (MenuWork.state) {
    case 0x10:
    case 0x22:
    case 0xC2:
        target = 0;
        break;
    default:
        target = 1;
        break;
    }
    position->x += (targets.value[target][0] - position->x) * 0.1f;
    position->y += (targets.value[target][1] - position->y) * 0.1f;
    position->z += (targets.value[target][2] - position->z) * 0.1f;
}

/* The AGWS model task: it eases the model into the pose the screen state asks
 * for and drives the open, weapon-preview and close states of its unit. */
void MenuAgwsModelMain(AgwsModelUnit *self)
{
    AgwsModelActor *actor = self->actor;
    AgwsModelTransform *transform;
    int weaponIndex;

    if (actor == 0) {
        if (MenuWork.modelBreak != 0 || MenuWork.state == 0x10) {
            MenuModelUnitBreak(self);
            MenuWork.modelBreak = 0;
        }
    }
    if (self->active == 0) {
        return;
    }
    transform = &actor->transform;
    if (MenuWork.state == 0x22) {
        MenuModelControl2(self, 1);
    } else {
        AgwsModelPositions positions = D_004C71D0;
        AgwsModelVector rotation = D_004C7240;
        Vector4 *position = &transform->position;
        Vector4 *rotationOf = &transform->rotation;
        Vector4 *from = &positions.position[0];

        switch (MenuWork.state) {
        case 0x42:
        case 0x44:
        case 0x46:
            from = &positions.position[MenuWork.weaponSlot[MenuWork.selectedIndex]];
            break;
        }
        position->x += (from->x - position->x) * 0.2f;
        position->y += (from->y - position->y) * 0.2f;
        position->z += (from->z - position->z) * 0.2f;
        rotationOf->x += (rotation.vector.x - rotationOf->x) * 0.2f;
        rotationOf->y += (rotation.vector.y - rotationOf->y) * 0.2f;
        rotationOf->z += (rotation.vector.z - rotationOf->z) * 0.2f;
    }
    if (MenuWork.modelBreak != 0 || MenuWork.state == 0x10) {
        self->state = 30;
        MenuWork.modelBreak = 0;
    }
    switch (self->state) {
    case 0:
        MenuModelUnitOpen(self, 2);
        if (MenuEquipStealMaskCheck() == 0) {
            for (weaponIndex = 0; weaponIndex < 3; weaponIndex++) {
                MenuModelWeaponCreate(self, 0, weaponIndex);
            }
        }
        self->state = 10;
        break;
    case 10:
        if (MenuWork.weaponChange != 0) {
            if (MenuWork.weaponHand >= 0) {
                if (MenuWork.weaponHand < 3) {
                    int hand = MenuWork.weaponHand;
                    short keptWeaponId = self->org->weaponId[hand];
                    signed char keptHand = self->org->hand[hand];

                    if (MenuWork.weaponChange == 1) {
                        self->org->weaponId[hand] = MenuWork.weaponId;
                        self->org->hand[hand] = MenuWork.weaponHandKind;
                        MenuModelWeaponCreate(self, 0, MenuWork.weaponHand);
                        self->org->weaponId[hand] = keptWeaponId;
                        self->org->hand[hand] = keptHand;
                    } else {
                        MenuModelWeaponCreate(self, 0, MenuWork.weaponHand);
                    }
                    MenuWork.weaponChange = 0;
                }
            }
        }
        self->state = 20;
        /* fallthrough */
    case 20:
        if (MenuWork.weaponChange != 0) {
            self->state = 10;
        }
        break;
    case 30:
        MenuModelUnitOpen(self, 1);
        self->state = 31;
        /* fallthrough */
    case 31:
        if (transform->fade == 0.0f) {
            self->taskPhase = -1;
        }
        break;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/menu_agws_para_set", MenuAgws);

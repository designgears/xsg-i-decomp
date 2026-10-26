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
        if ((signed char)MenuWork.selectedWeapon >= 0)
            eMessageCpy(buf, MenuTextGet(MenuSortGet(0, (signed char)MenuWork.selectedWeapon))->name);
        else
            eMessageCpy(buf, (char *)D_004DAF48);
        break;
    case 0x46:
        if ((signed char)MenuWork.selectedAmmo >= 0)
            eMessageCpy(buf, MenuTextGet(MenuSortGet(0, (signed char)MenuWork.selectedAmmo))->name);
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
        if ((signed char)MenuWork.selectedAccessory >= 0)
            eMessageCpy(buf, MenuTextGet(MenuSortGet(0, (signed char)MenuWork.selectedAccessory))->name);
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
            work->window[1].height = (signed char)MenuWork.nameRow * 0x18 + 6;
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

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct AgwsTextName {
    const char *name;
    const unsigned char *auxiliary;
} AgwsTextName;

typedef struct AgwsInfoContents {
    short textOffset[2]; /* MenuInfoWindow adds these to the window x and y. */
    unsigned char unmodeled_04[8];
    char *text;
    unsigned char unmodeled_10[0x1B0 - 0x10];
} AgwsInfoContents;

typedef struct AgwsInfoWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    const char *title;
    unsigned char state;
    unsigned char unmodeled_11[3];
    void (*callback)(void *window, void *contents);
    AgwsInfoContents *callbackArg;
    unsigned char unmodeled_1c[0x194 - 0x1C];
} AgwsInfoWindow;

typedef struct AgwsSelectContents {
    unsigned char unmodeled_00[2];
    unsigned short mode;
    int selectedIndex;
    char *text;
    char *annotation;
    unsigned char unmodeled_10[0x5F8 - 0x10];
} AgwsSelectContents;

typedef union AgwsSelectCallbackStorage {
    void (*function)(void *window, void *contents);
    unsigned char bytes[sizeof(void (*)(void *, void *))];
} AgwsSelectCallbackStorage;

typedef union AgwsSelectContextStorage {
    AgwsSelectContents *contents;
    unsigned char bytes[sizeof(AgwsSelectContents *)];
} AgwsSelectContextStorage;

typedef struct AgwsSelectWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    char *title;
    signed char state;
    unsigned char unmodeled_11[3];
    AgwsSelectCallbackStorage callback;
    AgwsSelectContextStorage callbackArg;
    unsigned char unmodeled_1c[0x194 - 0x1C];
} AgwsSelectWindow;

typedef struct AgwsWeapon2Message {
    unsigned char unmodeled_00;
    unsigned char mode;                     /* +0x01 */
    unsigned char unmodeled_02[2];
    short x;                                /* +0x04 */
    short y;                                /* +0x06 */
    int color;                              /* +0x08 */
    unsigned char unmodeled_0c[0x18 - 0x0C];
    const char *text;
    unsigned char unmodeled_1c[0x44 - 0x1C];
} AgwsWeapon2Message;
typedef struct AgwsWeapon2Sprite {
    unsigned char unmodeled_00[4];
    short x;                                /* +0x04 */
    short y;                                /* +0x06 */
    int color;                              /* +0x08 */
    unsigned char unmodeled_0c[0x28 - 0x0C];
} AgwsWeapon2Sprite;
typedef struct AgwsWeapon2WindowDX {
    short x;                                /* +0x000 */
    short y;                                /* +0x002 */
    int color;                              /* +0x004 */
    short width;                            /* +0x008 */
    short height;                           /* +0x00A */
    unsigned char unmodeled_0c[0x10 - 0x0C];
    unsigned char state;                    /* +0x010 */
    unsigned char unmodeled_11[0x194 - 0x11];
} AgwsWeapon2WindowDX;
typedef struct AgwsWeapon2Row {
    unsigned char unmodeled_00[0x14];
    AgwsWeapon2WindowDX window;             /* +0x014 */
    AgwsWeapon2Message message[3];          /* +0x1A8 */
    AgwsWeapon2Sprite sprite[3];            /* +0x274 */
    unsigned char unmodeled_2ec[0x300 - 0x2EC];
} AgwsWeapon2Row;
typedef struct AgwsWeapon2PanelView {
    unsigned char state;                    /* +0x00 */
    unsigned char mode;                     /* +0x01 */
    unsigned char unmodeled_02[2];
    int color;                              /* +0x04 */
    unsigned char cursorPrefix[0x10];
    AgwsWeapon2Row row[4];                  /* +0x18 */
    unsigned char unmodeled_c18[0x14];
} AgwsWeapon2PanelView;
typedef struct AgwsWeapon2CursolControl {
    signed char state;
    signed char active;
    signed char timer;
    signed char width;
    short x;
    short y;
    int work;
    signed char color[4];
} AgwsWeapon2CursolControl;
typedef struct AgwsWeapon2CursolData {
    AgwsWeapon2CursolControl control;
    unsigned char unmodeled_10[0x14];
} AgwsWeapon2CursolData;
typedef union AgwsWeapon2Work {
    AgwsWeapon2PanelView panels;
    /* Row zero's leading bytes overlap the cursor sprite tail. */
    struct {
        unsigned char controlPrefix[8];
        AgwsWeapon2CursolData cursol;       /* +0x08 */
    } cursor;
} AgwsWeapon2Work;
extern AgwsWeapon2Work *AgwsWeapon2;
typedef struct AgwsWeapon2MountRow {
    unsigned short icon[3];
} AgwsWeapon2MountRow;
typedef struct AgwsWeapon2MountSide {
    unsigned short icon[2];
} AgwsWeapon2MountSide;
extern const AgwsWeapon2MountRow D_004DAFD8[];
typedef struct AgwsUnitOrgFull {
    unsigned char unmodeled_00[0x5A];
    signed char hand[3];          /* +0x5a */
    unsigned char unmodeled_5d;
    short weaponId[3];            /* +0x5e */
    short accessoryId[3];         /* +0x64 */
    unsigned short ammoId[3];     /* +0x6a */
} AgwsUnitOrgFull;
void eCursolSet(void *cursor, signed char width);
void eCursolMain(void *cursor);
int MenuIdChange(short id, int kind);
#define MENU_TEXT_WEAPON 0x30000
#define MENU_TEXT_BULLET 0x40000
extern const AgwsWeapon2MountSide D_004DAFE0[];
#define AGWS_WEAPON2_SETUP 0
#define AGWS_WEAPON2_MAIN  2
typedef struct AgwsTextEntryView {
    char *name;
} AgwsTextEntryView;

void AgwsWeapon2Main(void)
{
    int i;
    short *bottom;
    int digit;
    AgwsUnitOrgFull *para;
    int one;
    int lines;
    AgwsWeapon2Work *storage = AgwsWeapon2;
    AgwsWeapon2PanelView *work = &storage->panels;
    int position;
    int lineY;
    int textIndex;
    short px, py, sy;
    short x[4];
    short y[4];
    unsigned int bullet[3];
    unsigned int equipped[3];
    const char *text[2];
    AgwsWeapon2MountRow mountRow;
    AgwsWeapon2MountSide mountSide;
    short icon[2];

    switch (work->state) {
    case AGWS_WEAPON2_SETUP:
        work->color = 0xFFFF00;
        for (i = 0; i < 4; i++) {
            WindowDXSet((void *)(&work->row[i].window));
            work->row[i].window.y = 0x30 + i * 0x36;
            work->row[i].window.color = work->color;
            work->row[i].window.height = 0x36;
            work->row[i].window.x = -0x102;
            work->row[i].window.width = 0xE5;
            if (i == 3) {
                bottom = (short *) ((char *) work + 0x920);
                bottom[11] = 0x4E;
            } else {
                work->row[i].window.height = 0x36;
            }
            work->row[i].window.state = 1;
            WindowDXMain((void *)(&work->row[i].window));
            work->row[i].window.state = 3;
        }
        eCursolSet(&storage->cursor.cursol.control, 0);
        work->mode = 0;
        work->state = AGWS_WEAPON2_MAIN;
        /* fallthrough */
    case AGWS_WEAPON2_MAIN:
        para = (AgwsUnitOrgFull *)func_A191C0(MenuWork.chrNo);
        for (i = 0; i < 4; i++) {
            x[i] = -0x102;
            y[i] = 0x30 + i * 0x36;
        }
        switch (MenuWork.state) {
        case 0x40:
            for (i = 0; i < 4; i++)
                x[i] = 0x10;
            storage->cursor.cursol.control.state = 0x20;
            break;
        case 0x22:
            storage->cursor.cursol.control.state = 0;
            break;
        case 0x42:
        case 0x44:
        case 0x46:
            x[MenuWork.weaponHand] = 0x10;
            y[MenuWork.weaponHand] = 0x30;
            break;
        case 0x62:
            x[3] = 0x10;
            y[3] = 0x30;
            break;
        case 0xC2:
            break;
        }
        for (i = 0; i < 4; i++) {
            MoveSlide(&work->row[i].window.x, &x[i], 3.0f);
            MoveSlide(&work->row[i].window.y, &y[i], 3.0f);
        }
        for (i = 0; i < 3; i++) {
            WindowDXMain((void *)(&work->row[i].window));
            one = 1;
            if (para->weaponId[i] != 0)
                equipped[i] = one;
            else
                equipped[i] = 0;
            bullet[i] = MenuBulletCheck(para->weaponId[i], -1);
            text[0] = ((AgwsTextEntryView *)MenuTextGet(MENU_TEXT_WEAPON + (unsigned short)para->weaponId[i]))->name;
            text[1] = ((AgwsTextEntryView *)MenuTextGet(MENU_TEXT_BULLET + para->ammoId[i]))->name;
            lines = 1;
            if (equipped[i] != 0) {
                lines = 2;
                if (bullet[i] == 0)
                    lines = 1;
            }
            for (textIndex = 0; textIndex < lines; textIndex++) {
                eMessageSet(&work->row[i].message[textIndex], text[textIndex]);
                work->row[i].message[textIndex].mode = 0x20;
                work->row[i].message[textIndex].color = work->row[i].window.color + 2;
                work->row[i].message[textIndex].x = work->row[i].window.x + 39;
                work->row[i].message[textIndex].y = work->row[i].window.y + ((lines & 1) * 12 + textIndex * 24) + 3;
                eMessageMain(&work->row[i].message[textIndex]);
            }
            if (equipped[i] != 0) {
                mountRow = D_004DAFD8[0];
                mountSide = D_004DAFE0[0];
                position = OptMntToWpnPos(para->hand[i]) - 1;
                icon[0] = mountRow.icon[position / 2];
                icon[1] = mountSide.icon[position & 1];
                for (digit = 0; digit < 2; digit++) {
                    eSpriteSet(&work->row[i].sprite[digit], icon[digit]);
                    px = work->row[i].window.x + digit + 19;
                    work->row[i].sprite[digit].x = px;
                    sy = work->row[i].window.y + (digit * 14 + (bullet[i] ^ 1) * 12) + 3;
                    work->row[i].sprite[digit].y = sy;
                    work->row[i].sprite[digit].color = work->row[i].window.color + digit * 2 + 2;
                    eSpriteMain(&work->row[i].sprite[digit]);
                }
                if (bullet[i] != 0) {
                    eSpriteSet(&work->row[i].sprite[2], 0x31B);
                    px = work->row[i].window.x + 19;
                    work->row[i].sprite[2].x = px;
                    work->row[i].sprite[2].color = work->row[i].window.color + 2;
                    py = work->row[i].window.y + 27;
                    work->row[i].sprite[2].y = py;
                    eSpriteMain(&work->row[i].sprite[2]);
                }
            } else {
                eSpriteSet(&work->row[i].sprite[2], 0x321);
                work->row[i].sprite[2].x = work->row[i].window.x + 19;
                work->row[i].sprite[2].color = work->row[i].window.color + 2;
                work->row[i].sprite[2].y = work->row[i].window.y + 15;
                eSpriteMain(&work->row[i].sprite[2]);
            }
        }
        WindowDXMain((void *)(&work->row[3].window));
        for (i = 0; i < 3; i++) {
            eSpriteSet(&work->row[3].sprite[i], 0x315);
            work->row[3].sprite[i].x = work->row[3].window.x + 19;
            work->row[3].sprite[i].color = work->row[3].window.color + 2;
            work->row[3].sprite[i].y = work->row[3].window.y + i * 24 + 3;
            eSpriteMain(&work->row[3].sprite[i]);
            eMessageSet(&work->row[3].message[i],
                        ((AgwsTextEntryView *)MenuTextGet(MenuIdChange(para->accessoryId[i], 5)))->name);
            work->row[3].message[i].mode = 0x20;
            work->row[3].message[i].x = work->row[3].window.x + 39;
            work->row[3].message[i].color = work->row[3].window.color + 2;
            work->row[3].message[i].y = work->row[3].window.y + i * 24 + 3;
            eMessageMain(&work->row[3].message[i]);
        }
        storage->cursor.cursol.control.x = work->row[MenuWork.weaponHand].window.x + 3;
        lineY = (unsigned short)work->row[MenuWork.weaponHand].window.y
                + MenuWork.equipSlot * 24;
        storage->cursor.cursol.control.y = lineY + 7;
        if (MenuWork.weaponHand != 3
                && (equipped[MenuWork.weaponHand] == 0
                    || bullet[MenuWork.weaponHand] == 0))
            storage->cursor.cursol.control.y = lineY + 19;
        storage->cursor.cursol.control.work = work->color + 2;
        eCursolMain(&storage->cursor.cursol);
        break;
    }
}

typedef struct AgwsWeapon2Cursol {
    unsigned char state;                    /* +0x00 */
    unsigned char unmodeled_01[3];
    short x;                                /* +0x04 */
    short y;                                /* +0x06 */
    int color;                              /* +0x08 */
    unsigned char unmodeled_0c[0x24 - 0x0C];
} AgwsWeapon2Cursol;
extern unsigned short AgwsNo[];
typedef struct AgwsNamePoint {
    short x, y;
    int depth;
    unsigned char r, g, b, a;
} AgwsNamePoint;
typedef struct AgwsNameWindow {
    short x, y;
    int color;
    short width, height;
    char *title;
    unsigned char state;
    unsigned char ribbonInitialized;
    unsigned char unmodeled_12[0x80 - 0x12];
    struct {
        unsigned char unmodeled_00[0x1C];
        const char *text;
    } titleFont;
    unsigned char unmodeled_a0[0x194 - 0xA0];
} AgwsNameWindow;
typedef struct AgwsNameWorkPrefix {
    unsigned char unmodeled_00[0x10];
    unsigned char state;
    unsigned char mode;
    unsigned char slide;
    signed char selection;
    int timer;
    int color;
    AgwsNameWindow window[8];
    AgwsWeapon2Sprite portrait[8];
    unsigned char unmodeled_dfc[0xF3C - 0xDFC];
    AgwsWeapon2Message message[8][2];
    AgwsWeapon2Cursol cursol;
    AgwsNamePoint ribbon[8][20];
    unsigned char unmodeled_1b20[0x1D00 - 0x1B20];
} AgwsNameWorkPrefix;
extern AgwsNameWorkPrefix *AgwsName;

typedef union AgwsNameMenuView {
    AgwsMenuWork agws;
    int words[0x80 / 4];
} AgwsNameMenuView;
#define AGWS_NAME_MENU (*(AgwsNameMenuView *)&MenuWork)

void AgwsNameMain(void) {
    int settledRows;
    int rowIndex;
    short slideTargetY;
    int titleLeft;
    int titleTop;
    int nameRight;
    int nameLeft;
    int nameTop;
    int pilot;
    int portraitLeft;
    int portraitRight;
    int portraitBottom;
    int pilotLeft;
    int pilotRight;
    int pilotTop;
    int titleRight;
    int messageDepth;
    int portraitTop;
    int pilotBottom;
    int titleBottom;
    int nameBottom;
    int portraitDepth;
    int pilotDepth;
    int titleDepth;
    int nameDepth;
    int cursorY;
    unsigned short character;
    AgwsNameWorkPrefix *name;
    char *pilotText;
    AgwsNamePoint *portraitRibbon;
    AgwsNamePoint *pilotRibbon;
    AgwsNamePoint *titleRibbon;
    AgwsNamePoint *nameRibbon;
    AgwsWeapon2Cursol *cursor;

    name = AgwsName;
    func_A191C0(AGWS_NAME_MENU.agws.chrNo);
    switch (name->state) {
    case 0x0:
    {
        int band;
        int pilot;
        int face;
        int depth;

        name->color = 0xFFFFF0;
        for (rowIndex = 0; rowIndex < AGWS_NAME_MENU.agws.agwsCount; rowIndex++) {
            int character = AgwsNo[rowIndex];
            func_A191C0(character);
            pilot = MenuAgwsPilotCheck(character);
            WindowDXSet((void *)&name->window[rowIndex]);
            name->window[rowIndex].x = (rowIndex & 1) * 0x320 - 0x100;
            name->window[rowIndex].y = rowIndex / 2 * 0x60 + 0x40;
            name->window[rowIndex].color = name->color;
            name->window[rowIndex].width = 0xE0;
            name->window[rowIndex].height = 0x36;
            if (pilot == 0)
                name->window[rowIndex].title = MenuTagTextGet(2);
            else if (PartyAttackerCheck(pilot) != 0)
                name->window[rowIndex].title = MenuTagTextGet(3);
            else
                name->window[rowIndex].title = MenuTagTextGet(4);
            name->window[rowIndex].ribbonInitialized = 1;
            name->window[rowIndex].state = 1;
            WindowDXMain((void *)&name->window[rowIndex]);
            name->window[rowIndex].state = 3;
            eMessageSet(&name->message[rowIndex][0], MenuCharNameGet(character));
            name->message[rowIndex][0].color = name->color + 2;
            name->message[rowIndex][0].mode = 0x20;
            eMessageSet(&name->message[rowIndex][1], MenuCharNameGet(pilot));
            name->message[rowIndex][1].color = name->color + 4;
            name->message[rowIndex][1].mode = 0x20;
            for (band = 0; band < 4; band++) {
                AgwsNamePoint *ribbonColors;
                ribbonColors = &name->ribbon[rowIndex][band * 5 + 3];
                ribbonColors[0].b = 1;
                ribbonColors[-1].b = 1;
                ribbonColors[-2].b = 1;
                ribbonColors[-3].b = 1;
                ribbonColors[0].g = 1;
                ribbonColors[-1].g = 1;
                ribbonColors[-2].g = 1;
                ribbonColors[-3].g = 1;
                ribbonColors[0].r = 1;
                ribbonColors[-1].r = 1;
                ribbonColors[-2].r = 1;
                ribbonColors[-3].r = 1;
                depth = name->color;
                name->ribbon[rowIndex][band * 5 + 3].depth = depth;
                name->ribbon[rowIndex][band * 5 + 2].depth = depth;
                name->ribbon[rowIndex][band * 5 + 1].depth = depth;
                name->ribbon[rowIndex][band * 5 + 0].depth = depth;
                name->ribbon[rowIndex][band * 5 + 4].depth = 0;
                ribbonColors[-2].a = 0x80;
                ribbonColors[-3].a = 0x80;
                ribbonColors[0].a = 0;
                ribbonColors[-1].a = 0;
            }
            face = ((unsigned short (*)(int, int))MenuFaceEpidGet)(character, 0);
            eSpriteSet(&name->portrait[rowIndex], face);
            name->portrait[rowIndex].color = name->color + 1;
        }
        eCursolSet(&name->cursol, 0);
        name->mode = 0;
        name->selection = -1;
        name->state = 0xA;
        name->slide = 0;
        name->timer = 0;
        break;
    }
    case 0xA:
        for (rowIndex = 0; rowIndex < (short)AGWS_NAME_MENU.agws.agwsCount; rowIndex++) {
            name->window[rowIndex].y = rowIndex / 2 * 0x60 + 0x40;
            name->window[rowIndex].x = (rowIndex & 1) * 0x320 - 0x100;
        }
        name->cursol.state = 0x20;
        name->state = 0xB;
        /* fallthrough */
    case 0xB:
    case 0x33:
        settledRows = 0;
        for (rowIndex = 0; rowIndex < (short)AGWS_NAME_MENU.agws.agwsCount; rowIndex++) {
            short target;
            if (name->state == 0xB)
                target = (rowIndex & 1) * 0xF0 + 0x18;
            else
                target = (rowIndex & 1) * 0x320 - 0x100;
            MoveSlide(&name->window[rowIndex].x, &target, 3.0f);
            if (name->window[rowIndex].x == target)
                settledRows++;
        }
        if (settledRows == (short)AGWS_NAME_MENU.agws.agwsCount) {
            if (name->state == 0xB)
                name->state = 0xC;
            else
                name->state = 2;
        }
        break;
    case 0xC:
        switch (AGWS_NAME_MENU.agws.state) {
        case 0x22:
            name->state = 0x14;
            break;
        case 0xF1:
            name->state = 0x33;
            break;
        }
        break;
    case 0x14:
        name->cursol.state = 0;
        name->state = 0x15;
        /* fallthrough */
    case 0x15:
        settledRows = 0;
        for (rowIndex = 0; rowIndex < (short)AGWS_NAME_MENU.agws.agwsCount; rowIndex++) {
            short targetX, targetY;
            if (rowIndex == AGWS_NAME_MENU.agws.selectedAgws) {
                targetX = 0x110;
                targetY = 0x18;
            } else {
                targetX = (rowIndex & 1) * 0x320 - 0x100;
                targetY = rowIndex / 2 * 0x60 + 0x40;
            }
            MoveSlide(&name->window[rowIndex].x, &targetX, 3.0f);
            if (name->window[rowIndex].x == targetX) {
                MoveSlide(&name->window[rowIndex].y, &targetY, 3.0f);
                if (name->window[rowIndex].y == targetY)
                    settledRows++;
            }
        }
        if (settledRows == (short)AGWS_NAME_MENU.agws.agwsCount)
            name->state = 0x16;
        break;
    case 0x3D:
        settledRows = 0;
        for (rowIndex = 0; rowIndex < (short)AGWS_NAME_MENU.agws.agwsCount; rowIndex++) {
            short targetY;
            short targetX;
            targetX = (rowIndex & 1) * 0xF0 + 0x18;
            targetY = rowIndex / 2 * 0x60 + 0x40;
            MoveSlide(&name->window[rowIndex].y, &targetY, 3.0f);
            if (name->window[rowIndex].y == targetY) {
                MoveSlide(&name->window[rowIndex].x, &targetX, 3.0f);
                if (name->window[rowIndex].x == targetX)
                    settledRows++;
            }
        }
        if (settledRows == (short)AGWS_NAME_MENU.agws.agwsCount)
            name->state = 0xC;
        break;
    case 0x16:
        switch (AGWS_NAME_MENU.agws.state) {
        case 0x10:
            name->cursol.state = 0x20;
            name->state = 0x3D;
            break;
        case 0x40:
            name->state = 0x1E;
            break;
        }
        for (rowIndex = 0; rowIndex < (short)AGWS_NAME_MENU.agws.agwsCount; rowIndex++) {
            if (rowIndex == AGWS_NAME_MENU.agws.selectedAgws) {
                name->window[rowIndex].x = 0x110;
                name->window[rowIndex].y = 0x18;
            } else {
                name->window[rowIndex].x = (rowIndex & 1) * 0x320 - 0x100;
                name->window[rowIndex].y = rowIndex / 2 * 0x60 + 0x40;
            }
        }
        break;
    case 0x1E:
    case 0x28:
        slideTargetY = 0x18;
        if (name->state == 0x1E)
            slideTargetY = -0x40;
        MoveSlide(&name->window[AGWS_NAME_MENU.agws.selectedAgws].y, &slideTargetY, 3.0f);
        if (name->window[AGWS_NAME_MENU.agws.selectedAgws].y == slideTargetY) {
            if (name->state == 0x1E)
                name->state = 0x20;
            else
                name->state = 0x16;
        }
        break;
    case 0x20:
        if (AGWS_NAME_MENU.agws.state == 0x22)
            name->state = 0x28;
        break;
    }
    cursor = &name->cursol;
    rowIndex = 0;
    if ((short) AGWS_NAME_MENU.agws.agwsCount > 0) {
        for (rowIndex = 0; rowIndex < (short)AGWS_NAME_MENU.agws.agwsCount; rowIndex++) {
            func_A191C0(AgwsNo[rowIndex]);
            if (rowIndex == (signed char) AGWS_NAME_MENU.agws.selectedAgws) {
                name->window[rowIndex].color = 0xFFFF00;
            } else {
                name->window[rowIndex].color = 0xFFF000;
            }
            pilot = MenuAgwsPilotCheck(AgwsNo[rowIndex]);
            if (pilot == 0) {
                name->window[rowIndex].titleFont.text = MenuTagTextGet(2);
            } else if (PartyAttackerCheck(pilot) != 0) {
                name->window[rowIndex].titleFont.text = MenuTagTextGet(3);
            } else {
                name->window[rowIndex].titleFont.text = MenuTagTextGet(4);
            }
            WindowDXMain((void *)&name->window[rowIndex]);
            portraitRibbon = &name->ribbon[rowIndex][0];
            portraitLeft = (unsigned short)name->window[rowIndex].x + 3;
            portraitDepth = name->window[rowIndex].color;
            portraitRibbon[1].x = (short) portraitLeft;
            portraitRibbon[0].x = (short) portraitLeft;
            name->ribbon[rowIndex][3].depth = portraitDepth;
            portraitRight = (unsigned short)name->window[rowIndex].x + 0x53;
            name->ribbon[rowIndex][3].x = (short) portraitRight;
            name->ribbon[rowIndex][1].depth = portraitDepth;
            name->ribbon[rowIndex][2].x = (short) portraitRight;
            name->ribbon[rowIndex][2].depth = portraitDepth;
            name->ribbon[rowIndex][0].depth = portraitDepth;
            portraitTop = (unsigned short)name->window[rowIndex].y + 3;
            name->ribbon[rowIndex][2].y = (short) portraitTop;
            name->ribbon[rowIndex][0].y = (short) portraitTop;
            portraitBottom = ((unsigned short)name->window[rowIndex].y + (unsigned short)name->window[rowIndex].height) - 3;
            name->ribbon[rowIndex][3].y = (short) portraitBottom;
            name->ribbon[rowIndex][1].y = (short) portraitBottom;
            endPrintExtFunc(name->color, 3, portraitRibbon);
            pilotRibbon = &name->ribbon[rowIndex][5];
            pilotLeft = ((unsigned short)name->window[rowIndex].x + (unsigned short)name->window[rowIndex].width) - 0x8E;
            pilotDepth = name->window[rowIndex].color;
            name->ribbon[rowIndex][6].x = (short) pilotLeft;
            name->ribbon[rowIndex][5].x = (short) pilotLeft;
            name->ribbon[rowIndex][8].depth = pilotDepth;
            name->ribbon[rowIndex][6].depth = pilotDepth;
            pilotRight = ((unsigned short)name->window[rowIndex].x + (unsigned short)name->window[rowIndex].width) - 3;
            name->ribbon[rowIndex][7].depth = pilotDepth;
            name->ribbon[rowIndex][5].depth = pilotDepth;
            name->ribbon[rowIndex][8].x = (short) pilotRight;
            name->ribbon[rowIndex][7].x = (short) pilotRight;
            pilotTop = (unsigned short)name->window[rowIndex].y + 0x1B;
            name->ribbon[rowIndex][7].y = (short) pilotTop;
            name->ribbon[rowIndex][5].y = (short) pilotTop;
            pilotBottom = ((unsigned short)name->window[rowIndex].y + (unsigned short)name->window[rowIndex].height) - 3;
            name->ribbon[rowIndex][8].y = (short) pilotBottom;
            name->ribbon[rowIndex][6].y = (short) pilotBottom;
            endPrintExtFunc(name->color, 3, pilotRibbon);
            titleRibbon = &name->ribbon[rowIndex][10];
            titleRight = ((unsigned short)name->window[rowIndex].x + (unsigned short)name->window[rowIndex].width) - 3;
            titleDepth = name->window[rowIndex].color;
            name->ribbon[rowIndex][11].x = (short) titleRight;
            name->ribbon[rowIndex][10].x = (short) titleRight;
            name->ribbon[rowIndex][13].depth = titleDepth;
            name->ribbon[rowIndex][11].depth = titleDepth;
            titleLeft = ((unsigned short)name->window[rowIndex].x + (unsigned short)name->window[rowIndex].width) - 0xA0;
            name->ribbon[rowIndex][12].depth = titleDepth;
            name->ribbon[rowIndex][10].depth = titleDepth;
            name->ribbon[rowIndex][13].x = (short) titleLeft;
            name->ribbon[rowIndex][12].x = (short) titleLeft;
            titleTop = (unsigned short)name->window[rowIndex].y + 3;
            name->ribbon[rowIndex][12].y = (short) titleTop;
            name->ribbon[rowIndex][10].y = (short) titleTop;
            titleBottom = (unsigned short)name->window[rowIndex].y + 0x1B;
            name->ribbon[rowIndex][13].y = (short) titleBottom;
            name->ribbon[rowIndex][11].y = (short) titleBottom;
            endPrintExtFunc(name->color, 3, titleRibbon);
            name->portrait[rowIndex].x = (short) ((unsigned short)name->window[rowIndex].x + 0x13);
            name->portrait[rowIndex].color = name->window[rowIndex].color;
            name->portrait[rowIndex].y = (short) ((unsigned short)name->window[rowIndex].y + 3);
            eSpriteMain(&name->portrait[rowIndex]);
            nameRibbon = &name->ribbon[rowIndex][15];
            nameRight = ((unsigned short)name->window[rowIndex].x + (unsigned short)name->window[rowIndex].width) - 0x8C;
            nameDepth = name->window[rowIndex].color;
            name->ribbon[rowIndex][16].x = (short) nameRight;
            name->ribbon[rowIndex][15].x = (short) nameRight;
            name->ribbon[rowIndex][18].depth = nameDepth;
            name->ribbon[rowIndex][16].depth = nameDepth;
            name->ribbon[rowIndex][17].depth = nameDepth;
            name->ribbon[rowIndex][15].depth = nameDepth;
            nameLeft = ((unsigned short)name->window[rowIndex].x + (unsigned short)name->window[rowIndex].width) - 0xB4;
            name->ribbon[rowIndex][18].x = (short) nameLeft;
            name->ribbon[rowIndex][17].x = (short) nameLeft;
            nameTop = (unsigned short)name->window[rowIndex].y + 0x1B;
            name->ribbon[rowIndex][17].y = (short) nameTop;
            name->ribbon[rowIndex][15].y = (short) nameTop;
            nameBottom = ((unsigned short)name->window[rowIndex].y + (unsigned short)name->window[rowIndex].height) - 3;
            name->ribbon[rowIndex][18].y = (short) nameBottom;
            name->ribbon[rowIndex][16].y = (short) nameBottom;
            endPrintExtFunc(name->color, 3, nameRibbon);
            name->message[rowIndex][0].x = (short) ((unsigned short)name->window[rowIndex].x + 0x3C);
            name->message[rowIndex][0].color = name->window[rowIndex].color + 2;
            name->message[rowIndex][0].y = (short) ((unsigned short)name->window[rowIndex].y + 0x1B);
            eMessageMain(&name->message[rowIndex][0]);
            name->message[rowIndex][1].x = (short) (((unsigned short)name->window[rowIndex].x + (unsigned short)name->window[rowIndex].width) - 0x53);
            name->message[rowIndex][1].y = (short) ((unsigned short)name->window[rowIndex].y + 3);
            character = AgwsNo[rowIndex];
            pilotText = MenuCharNameGet(MenuAgwsPilotCheck((int) character));
            messageDepth = name->window[rowIndex].color;
            name->message[rowIndex][1].text = pilotText;
            name->message[rowIndex][1].color = messageDepth + 2;
            eMessageMain(&name->message[rowIndex][1]);
        }
    }
    name->cursol.x = (unsigned short)name->window[AGWS_NAME_MENU.agws.selectedAgws].x + 3;
    cursorY = (unsigned short)name->window[AGWS_NAME_MENU.agws.selectedAgws].y + 0x13;
    name->cursol.color = name->color + 3;
    name->cursol.y = cursorY;
    eCursolMain(cursor);
    endPrintExtFunc(name->color, 6, 0);
}

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

typedef signed char s8;
typedef int s32;
typedef s32 M2C_UNK;
s32 MenuMainAgwsCheck(s16);
void MenuModelAllBreak(void);
void MenuModelCreate(int *output, int character);
void MenuModelExtFuncSet(s32, void (*)(AgwsModelUnit *), M2C_UNK);
void MenuModelMemorySet(int);
void MenuModelMenuMotionLoad(void);
s32 MenuRWeaponCheck(s16, s32);
void MenuWorkEndCheck(u8 *end);
s32 PartyTakeAgwsCheck(s32);
int dataAccBoxDec(int);
void dataAccBoxInc(int);
s32 dataBltBoxChk(s16);
void dataBltBoxDec(int);
void dataBltBoxInc(int);
void dataWpnBoxDec(int);
void dataWpnBoxInc(int);
void xglFontDebugHex(int x, int y, int value, int digits);
extern unsigned char *MainMenuWorkEnd;
extern AgwsListSPWindow *AgwsWinSP[4];
extern unsigned char D_004C72D0[];
extern unsigned char D_004DAFF0[];
extern unsigned char D_004DAFF8[];
extern int MenuModelOut[4];
extern int MenuSelectMove(int selection, int count, int wrap);
extern int MenuCursorKeepCheck(void);
typedef struct AgwsMenuWorkUp {
    unsigned char unmodeled_00[0x03];
    unsigned char state;
    unsigned char unmodeled_04[0x10 - 0x04];
    signed char weaponSlot[12];
    unsigned char unmodeled_1c[0x21 - 0x1C];
    signed char modelBreak;
    signed char weaponChange;
    unsigned char unmodeled_23[0x26 - 0x23];
    short weaponId;
    unsigned short ammoId; /* +0x28 */
    unsigned short accessoryId; /* +0x2a */
    short pilotId; /* +0x2c */
    short weaponHandKind; /* +0x2e */
    unsigned char flags;
    unsigned char unmodeled_31[3];
    unsigned char commandState;
    unsigned char unmodeled_35[0x40 - 0x35];
    unsigned char transitionTicks;
    signed char selectedAgws;
    signed char menuSelection;
    unsigned char unmodeled_43[0x50 - 0x43];
    signed char listKind;
    signed char reserveCursor;
    signed char listSelect;
    unsigned char unmodeled_53[0x54 - 0x53];
    signed char weaponHand;
    signed char equipSlot;
    signed char selectedIndex;
    signed char selectedWeapon; /* +0x57 */
    signed char selectedAmmo; /* +0x58 */
    signed char selectedAccessory; /* +0x59 */
    signed char nameRow;
    unsigned char unmodeled_5b[0x64 - 0x5B];
    short chrNo;
    short agwsCount;
    unsigned char unmodeled_68[0x70 - 0x68];
    unsigned char pendingState;
    unsigned char unmodeled_71[0x80 - 0x71];
} AgwsMenuWorkUp;
typedef struct {
    u8 pad_00[0x28];
    u16 half_28;
    u16 half_2a;
    unsigned char unmodeled_2c[6];
    u16 mappedPressed;
    u16 mappedRepeat;
} AgwsMenuPadView;
extern AgwsMenuWorkUp MenuWorkUp __asm__("MenuWork");
extern AgwsMenuPadView AgwsMenuPad __asm__("PadData");
typedef struct WeaponWaglDataUp {
    unsigned char unmodeled_00[0x12];
    short wagl;
    unsigned char unmodeled_14[0x1A - 0x14];
    short defaultAmmoId; /* +0x1a */
} WeaponWaglDataUp;
typedef struct AgwsUnitOrgUp {
    unsigned char unmodeled_00[0x34];
    short hp;
    unsigned char unmodeled_36[0x54 - 0x36];
    short agwsId;
    unsigned char unmodeled_56[0x5A - 0x56];
    signed char hand[3];          /* +0x5a */
    unsigned char unmodeled_5d;
    short weaponId[3];            /* +0x5e */
    short accessoryId[3];         /* +0x64 */
    unsigned short ammoId[3];     /* +0x6a */
} AgwsUnitOrgUp;
int WindowSPSelect(void *cursor, int button_flags);
void WindowSPKeepSelect(void *cursor, unsigned char *keep);
void MenuKeepSelectReset(void);
extern void xglSoundEffectNormalID(int sound_id, int variant);
void ChangeTopLevel(int top_level);
void WindowSPKeepSelectCheck(unsigned char *keep);

void MenuAgws(void) {
    int occupiedMount[3];
    unsigned char previousWeaponHand;
    AgwsPasWork *pas;
    AgwsUnitOrgUp *temp_19_3;
    AgwsUnitOrgUp *temp_21_3;
    AgwsUnitOrgUp *temp_2_3;
    AgwsUnitOrgUp *temp_2_8;
    AgwsUnitOrgUp *temp_3_4;
    AgwsUnitOrgUp *temp_4_2;
    AgwsUnitOrgUp *temp_6_2;
    s16 temp_16_2;
    s16 temp_16_5;
    s16 temp_17_3;
    s16 temp_19_2;
    s16 temp_21;
    s16 temp_22;
    s16 temp_23_2;
    s16 temp_4_3;
    s32 temp_18_2;
    s32 temp_21_2;
    s32 temp_22_2;
    s32 temp_2_10;
    s32 temp_2_4;
    s32 temp_2_5;
    s32 temp_2_7;
    s32 temp_3_3;
    int temp_19;
    s8 temp_23;
    u16 temp_16_3;
    u16 temp_17_4;

    if (MenuWorkUp.state == 0) {
        unsigned char *workCursor;
        unsigned char *blockBase;
        unsigned char *windowBase;
        unsigned short *acceptedAgws;
        AgwsNameWorkPrefix *name;
        AgwsMenuWorkUp *menu;
        int windowIndex;
        s32 var_16;
        s32 var_17;

        windowIndex = 3;
        workCursor = MainMenuWorkEnd;
        pas = (AgwsPasWork *) workCursor;
        AgwsPas = pas;
        blockBase = workCursor;
        workCursor += 1904;
        pas->state = 0;
        AgwsInfo = (AgwsInfoWork *) workCursor;
        blockBase = workCursor;
        workCursor += 1104;
        *blockBase = 0;
        AgwsSelect = (AgwsSelectWork *) workCursor;
        blockBase = workCursor;
        workCursor += 7736;
        *blockBase = 0;
        AgwsList = (AgwsListWork *) workCursor;
        blockBase = workCursor;
        workCursor += 0x5E10;
        *blockBase = 0;
        windowBase = (unsigned char *) &AgwsList->window;
        do {
            AgwsWinSP[windowIndex] = (AgwsListSPWindow *)
                (windowBase + windowIndex * 0x1738);
            windowIndex--;
        } while (windowIndex >= 0);
        AgwsStatus = (AgwsStatusWork *) workCursor;
        blockBase = workCursor;
        workCursor += 0x71C;
        *blockBase = 0;
        AgwsWeapon2 = (AgwsWeapon2Work *) workCursor;
        blockBase = workCursor;
        workCursor += sizeof(AgwsWeapon2Work);
        *blockBase = 0;
        AgwsSwitch = (AgwsSwitchWork *) workCursor;
        blockBase = workCursor;
        workCursor += 0x1B8;
        *blockBase = 0;
        AgwsFace = (AgwsFaceWork *) workCursor;
        blockBase = workCursor;
        workCursor += 0x1C4;
        *blockBase = 0;
        AgwsName = (AgwsNameWorkPrefix *)workCursor;
        name = AgwsName;
        workCursor += sizeof(AgwsNameWorkPrefix);
        name->state = 0;
        MenuWorkEndCheck(workCursor);
        var_17 = 0;
        MenuWorkUp.agwsCount = 0;
        var_16 = 0x11;
        acceptedAgws = AgwsNo;
        for (; var_16 < 0x21; var_16++) {
            if (PartyTakeAgwsCheck(var_16) != 0) {
                var_17 += 1;
                *acceptedAgws++ = (unsigned short) var_16;
                MenuWorkUp.agwsCount++;
            }
        }
        for (; var_17 < 0x10; var_17++)
            AgwsNo[var_17] = 0;
        MenuWorkUp.chrNo = (s16)AgwsNo[0];
        MenuAgwsParaSet((s32) (s16) AgwsNo[0], 0);
        MenuAgwsCameraSet();
        MenuModelMemorySet(0xB);
        MenuModelMenuMotionLoad();
        MenuModelOut[0] = 0;
        MenuKeepSelectReset();
        menu = &MenuWorkUp;
        menu->state = 0;
        menu->flags = 0;
        menu->pendingState = 0x10;
        menu->transitionTicks = 0x10;
    }
    if (MenuWorkUp.state != MenuWorkUp.pendingState) {
        MenuWorkUp.state = (u8) MenuWorkUp.pendingState;
        MenuWorkUp.flags |= 1;
        MenuWorkUp.commandState = (u8) MenuWorkUp.pendingState;
    } else {
        MenuWorkUp.flags &= 0xFE;
    }
    if (MenuWorkUp.transitionTicks != 0) {
        MenuWorkUp.transitionTicks = (u8) (MenuWorkUp.transitionTicks + 0xFF);
    }
    MenuAgwsCameraMove();
    switch (MenuWorkUp.state) {                       /* switch 1; irregular */
    case 0x10:                                      /* switch 1 */
        if (MenuWorkUp.transitionTicks == 0) {
            if (AgwsMenuPad.mappedRepeat & 0xF000) {
                switch (AgwsMenuPad.mappedRepeat) {     /* switch 2; irregular */
                case 0x1000:                        /* switch 2 */
                    MenuWorkUp.selectedAgws -= 2;
                    break;
                case 0x4000:                        /* switch 2 */
                    MenuWorkUp.selectedAgws += 2;
                    break;
                case 0x8000:                        /* switch 2 */
                    MenuWorkUp.selectedAgws--;
                    break;
                case 0x2000:                        /* switch 2 */
                    MenuWorkUp.selectedAgws++;
                    break;
                }
                {
                    AgwsMenuWorkUp *selectionMenu = &MenuWorkUp;
                    if (selectionMenu->selectedAgws < 0) {
                        int lastAgws = (unsigned char)selectionMenu->agwsCount - 1;
                        selectionMenu->selectedAgws = lastAgws;
                    }
                    if (selectionMenu->selectedAgws >= (short)selectionMenu->agwsCount) {
                        selectionMenu->selectedAgws = 0;
                    }
                    selectionMenu->chrNo = AgwsNo[selectionMenu->selectedAgws];
                }
                xglSoundEffectNormalID(3, 0);
            }
            if (AgwsMenuPad.half_2a & 0x20) {
                MenuWorkUp.pendingState = (u8) 0x22;
                MenuWorkUp.transitionTicks = (u8) 0x18;
                MenuWorkUp.weaponHand = (s8) 0;
                MenuWorkUp.equipSlot = (s8) 0;
                MenuWorkUp.selectedIndex = (s8) 0;
                xglSoundEffectNormalID(1, 0);
                MenuModelCreate(MenuModelOut, MenuWorkUp.chrNo);
                MenuModelExtFuncSet(MenuModelOut[0], MenuAgwsModelMain, 0);
                MenuAgwsParaSet((s32) MenuWorkUp.chrNo, 0);
            }
            if (AgwsMenuPad.half_2a & 0x40) {
                MenuWorkUp.pendingState = (u8) -0x10;
                xglSoundEffectNormalID(2, 0);
            }
        }
        break;
    case 0x22:
        if (MenuWorkUp.transitionTicks == 0) {
            if (AgwsMenuPad.half_28 & 3)
                break;
            if (AgwsMenuPad.half_2a & 0xC) {
                unsigned short character;
                switch (AgwsMenuPad.half_2a) {
                case 4:
                    MenuWorkUp.selectedAgws--;
                    break;
                case 8:
                    MenuWorkUp.selectedAgws++;
                    break;
                }
                if (MenuWorkUp.selectedAgws < 0)
                    MenuWorkUp.selectedAgws = (unsigned char)MenuWorkUp.selectedAgws
                                           + (unsigned char)MenuWorkUp.agwsCount;
                else if (MenuWorkUp.selectedAgws >= (short)MenuWorkUp.agwsCount)
                    MenuWorkUp.selectedAgws = (unsigned char)MenuWorkUp.selectedAgws
                                           - (unsigned char)MenuWorkUp.agwsCount;
                MenuWorkUp.modelBreak = 1;
                MenuWorkUp.weaponHand = 0;
                character = AgwsNo[MenuWorkUp.selectedAgws];
                MenuWorkUp.equipSlot = 0;
                MenuWorkUp.chrNo = character;
                MenuWorkUp.selectedIndex = 0;
                MenuAgwsParaSet((short)character, 0);
                xglSoundEffectNormalID(3, 0);
                MenuKeepSelectReset();
                MenuWorkUp.transitionTicks = 4;
            } else {
                int nextCursor = MenuSelectMove(MenuWorkUp.reserveCursor, 3, 0);
                if (MenuWorkUp.reserveCursor != nextCursor) {
                    MenuWorkUp.reserveCursor = nextCursor;
                } else if (MenuModelOut[0] != 0) {
                    if (AgwsMenuPad.half_2a & 0x20) {
                        if (MenuMainAgwsCheck(MenuWorkUp.chrNo) != 0) {
                            int reserve = MenuWorkUp.reserveCursor;
                            if (reserve != 2) {
                                if (reserve < 3) {
                                    if (reserve >= 0)
                                        MenuWorkUp.pendingState = 0x40;
                                }
                            } else {
                                MenuWorkUp.pendingState = 0xC2;
                            }
                            MenuWorkUp.transitionTicks = 0x10;
                            if (MenuCursorKeepCheck() == 0) {
                                MenuWorkUp.weaponHand = 0;
                                MenuWorkUp.equipSlot = 0;
                            }
                            xglSoundEffectNormalID(1, 0);
                        } else {
                            xglSoundEffectNormalID(5, 0);
                        }
                    } else if (AgwsMenuPad.half_2a & 0x40) {
                        MenuWorkUp.pendingState = 0x10;
                        MenuWorkUp.transitionTicks = 0x10;
                        MenuKeepSelectReset();
                        xglSoundEffectNormalID(2, 0);
                    }
                }
            }
            if (MenuModelOut[0] == 0) {
                MenuModelCreate(MenuModelOut, MenuWorkUp.chrNo);
                MenuModelExtFuncSet(MenuModelOut[0], MenuAgwsModelMain, 0);
            }
        }
        MenuWorkUp.menuSelection = 1;
        break;
    case 0x40:                                      /* switch 1 */
        if (MenuWorkUp.flags & 1 & 0xFF) {
            MenuWorkUp.weaponId = (s16) 0;
            MenuWorkUp.ammoId = (u16) 0;
            MenuWorkUp.accessoryId = (u16) 0;
        }
        if (MenuWorkUp.transitionTicks == 0) {
            temp_16_3 = AgwsMenuPad.mappedRepeat;
            if (temp_16_3 == 0x1000) {
                temp_6_2 = (AgwsUnitOrgUp *)func_A191C0((s32) MenuWorkUp.chrNo);
                previousWeaponHand = MenuWorkUp.weaponHand;
                if (MenuWorkUp.weaponHand == 3) {
                    if (MenuWorkUp.equipSlot == 0) {
                        if (MenuBulletCheck((s32) temp_6_2->accessoryId[0], -1) != 0) {
                            MenuWorkUp.equipSlot = (s8) 1;
                        } else {
                            MenuWorkUp.equipSlot = (s8) 0;
                        }
                        MenuWorkUp.weaponHand = (s8) 2;
                    } else {
                        MenuWorkUp.equipSlot--;
                    }
                    xglSoundEffectNormalID(3, 0);
                } else if (MenuWorkUp.equipSlot == 1) {
                    MenuWorkUp.equipSlot = (s8) 0;
                    xglSoundEffectNormalID(3, 0);
                } else if (MenuWorkUp.weaponHand == 0) {
                    if (AgwsMenuPad.mappedPressed == temp_16_3) {
                        MenuWorkUp.weaponHand = (s8) 3;
                        MenuWorkUp.equipSlot = (s8) 2;
                    }
                } else {
                    temp_3_3 = previousWeaponHand - 1;
                    MenuWorkUp.weaponHand = (s8) temp_3_3;
                    if (MenuBulletCheck(temp_6_2->weaponId[MenuWorkUp.weaponHand], -1) != 0) {
                        MenuWorkUp.equipSlot = (s8) 1;
                    }
                    xglSoundEffectNormalID(3, 0);
                }
            }
            temp_17_4 = AgwsMenuPad.mappedRepeat;
            if (temp_17_4 == 0x4000) {
                temp_4_2 = (AgwsUnitOrgUp *)func_A191C0((s32) MenuWorkUp.chrNo);
                if (MenuWorkUp.weaponHand == 3) {
                    if (MenuWorkUp.equipSlot == 2) {
                        if (AgwsMenuPad.mappedPressed == temp_17_4) {
                            MenuWorkUp.equipSlot = (s8) 0;
                            MenuWorkUp.weaponHand = (s8) 0;
                        }
                    } else {
                        MenuWorkUp.equipSlot = (s8) ((u8) MenuWorkUp.equipSlot + 1);
                        xglSoundEffectNormalID(3, 0);
                    }
                } else {
                    if (MenuWorkUp.equipSlot == 0
                            && (temp_2_10 = MenuBulletCheck(temp_4_2->weaponId[MenuWorkUp.weaponHand], -1)) == 1) {
                        MenuWorkUp.equipSlot = (s8) temp_2_10;
                    } else {
                        MenuWorkUp.equipSlot = (s8) 0;
                        MenuWorkUp.weaponHand = (s8) ((u8) MenuWorkUp.weaponHand + 1);
                    }
                    xglSoundEffectNormalID(3, 0);
                }
            }
            if (AgwsMenuPad.half_2a == 0x20) {
                if (MenuWorkUp.reserveCursor == 0) {
                    if (MenuWorkUp.weaponHand != 3) {
                        if (MenuWorkUp.equipSlot == 0) {
                            AgwsMenuWorkUp *selectionMenu = &MenuWorkUp;
                            unsigned char nextState = 0x42;
                            selectionMenu->selectedWeapon = -1;
                            selectionMenu->pendingState = nextState;
                            selectionMenu->selectedIndex = 0;
                        } else {
                            MenuWorkUp.pendingState = 0x46;
                            MenuWorkUp.selectedAmmo = (s8) -1;
                        }
                    } else {
                        MenuWorkUp.pendingState = 0x62;
                        MenuWorkUp.selectedAccessory = (s8) -1;
                    }
                    MenuWorkUp.transitionTicks = (u8) 0x10;
                    xglSoundEffectNormalID(1, 0);
                } else {
                    temp_3_4 = (AgwsUnitOrgUp *)func_A191C0((s32) MenuWorkUp.chrNo);
                    if (MenuWorkUp.weaponHand != 3) {
                        if (MenuWorkUp.equipSlot != 1) {
                            temp_18_2 = MenuRWeaponCheck2(temp_3_4->weaponId[MenuWorkUp.weaponHand]);
                            temp_3_4->weaponId[MenuWorkUp.weaponHand] = 0;
                            temp_3_4->hand[MenuWorkUp.weaponHand] = 0;
                            if (temp_18_2 != 0) {
                                dataWpnBoxInc(temp_18_2);
                            }
                            MenuWorkUp.weaponChange = (s8) 1;
                            temp_16_5 = temp_3_4->ammoId[MenuWorkUp.weaponHand];
                            temp_3_4->ammoId[MenuWorkUp.weaponHand] = 0;
                            if ((MenuBulletCheck(temp_18_2, -1) != 0) && (temp_16_5 != 0)) {
                                if ((((WeaponWaglDataUp *)func_A1A3D8(temp_18_2))->defaultAmmoId != temp_16_5) || (dataBltBoxChk(temp_16_5) == 0)) {
                                    dataBltBoxInc(temp_16_5);
                                }
                            }
                        }
                    } else {
                        int accessorySlot = MenuWorkUp.equipSlot;
                        temp_4_3 = temp_3_4->accessoryId[accessorySlot];
                        temp_3_4->accessoryId[accessorySlot] = 0;
                        if (temp_4_3 != 0) {
                            dataAccBoxInc(temp_4_3);
                        }
                    }
                    MenuAgwsParaSet((s32) MenuWorkUp.chrNo, 0);
                    xglSoundEffectNormalID(0x11, 0);
                }
            }
            if (AgwsMenuPad.half_2a == 0x40) {
                if (MenuCursorKeepCheck() == 0) {
                    MenuWorkUp.reserveCursor = (s8) 0;
                }
                    MenuWorkUp.pendingState = (u8) 0x22;
                xglSoundEffectNormalID(2, 0);
            }
        }
        break;
    case 0x42:                                      /* switch 1 */
        if (MenuWorkUp.flags & 1 & 0xFF) {
            AgwsUnitOrgUp *org = (AgwsUnitOrgUp *)func_A191C0(MenuWorkUp.chrNo);
            int hand;
            int position;

            MenuWorkUp.nameRow = 0;
            for (hand = 0; hand < 3; hand++) {
                if (hand == MenuWorkUp.weaponHand)
                    occupiedMount[hand] = 0;
                else
                    occupiedMount[hand] = OptMntToWpnPos(org->hand[hand]);
            }
            for (position = 1; position < 6; position++) {
                for (hand = 0; hand < 3; hand++) {
                    if (occupiedMount[hand] == position)
                        break;
                }
                if (hand == 3) {
                    MenuWorkUp.weaponSlot[MenuWorkUp.nameRow] = position;
                    MenuWorkUp.nameRow++;
                    MenuWorkUp.weaponSlot[MenuWorkUp.nameRow] = 0;
                }
            }
        }
        if (MenuWorkUp.transitionTicks == 0) {
            MenuWorkUp.selectedIndex = (s8) MenuSelectMove((s32) MenuWorkUp.selectedIndex, (s32) MenuWorkUp.nameRow, 0);
            if (AgwsMenuPad.half_2a & 0x40) {
                MenuWorkUp.pendingState = (u8) 0x40;
                MenuWorkUp.transitionTicks = (u8) 0x10;
                xglSoundEffectNormalID(2, 0);
            }
            if (AgwsMenuPad.half_2a & 0x20) {
                MenuWorkUp.pendingState = (u8) 0x44;
                MenuWorkUp.transitionTicks = (u8) 0x10;
                xglSoundEffectNormalID(1, 0);
            }
        }
        break;
    case 0x44:                                      /* switch 1 */
        if (MenuWorkUp.flags & 1 & 0xFF) {
            MenuAgwsListMake_Wpn();
            MenuWorkUp.selectedWeapon = (s8) -1;
            MenuWorkUp.weaponId = (s16) 0;
        } else if (MenuWorkUp.transitionTicks == 0) {
                temp_19 = WindowSPSelect((void *)(AgwsWinSP[0]), AgwsMenuPad.mappedRepeat);
                WindowSPKeepSelect((void *)(AgwsWinSP[0]), &MenuKeepSelect[MenuWorkUp.weaponSlot[MenuWorkUp.selectedIndex] * 5 - 5]);
                if (MenuWorkUp.selectedWeapon != temp_19) {
                    temp_2_3 = (AgwsUnitOrgUp *)func_A191C0((s32) MenuWorkUp.chrNo);
                    MenuWorkUp.selectedWeapon = (s8) temp_19;
                    temp_2_4 = MenuSortGet(0, MenuWorkUp.selectedWeapon);
                    MenuWorkUp.weaponId = (s16) temp_2_4;
                    temp_2_5 = MenuWeaponEquipPosCheck((s32) MenuWorkUp.chrNo, (s32) (s16) temp_2_4, (s32) MenuWorkUp.weaponSlot[MenuWorkUp.selectedIndex], (s32) MenuWorkUp.weaponHand);
                    MenuWorkUp.weaponHandKind = (short) temp_2_5;
                    if (MenuWorkUp.weaponHandKind != 0) {
                        temp_23 = temp_2_3->hand[MenuWorkUp.weaponHand];
                        temp_22 = temp_2_3->weaponId[MenuWorkUp.weaponHand];
                        temp_21 = temp_2_3->ammoId[MenuWorkUp.weaponHand];
                        temp_2_3->weaponId[MenuWorkUp.weaponHand] = MenuWorkUp.weaponId;
                        temp_2_3->hand[MenuWorkUp.weaponHand] = MenuWorkUp.weaponHandKind;
                        if (temp_2_3->weaponId[MenuWorkUp.weaponHand] != 0) {
                            temp_2_3->ammoId[MenuWorkUp.weaponHand] = ((WeaponWaglDataUp *)func_A1A3D8(MenuWorkUp.weaponId))->defaultAmmoId;
                        } else {
                            temp_2_3->ammoId[MenuWorkUp.weaponHand] = 0;
                        }
                        MenuAgwsParaSet((s32) MenuWorkUp.chrNo, 1);
                        temp_2_3->ammoId[MenuWorkUp.weaponHand] = temp_21;
                        temp_2_3->weaponId[MenuWorkUp.weaponHand] = temp_22;
                        temp_2_3->hand[MenuWorkUp.weaponHand] = temp_23;
                        MenuWorkUp.weaponChange = (s8) 1;
                    } else {
                        MenuAgwsParaSet((s32) MenuWorkUp.chrNo, 0);
                        MenuWorkUp.weaponChange = (s8) 2;
                    }
                    MenuWorkUp.selectedWeapon = (s8) temp_19;
                }
                if ((AgwsMenuPad.half_2a == 0x20) && (MenuWorkUp.selectedWeapon >= 0)) {
                    temp_2_7 = MenuWeaponEquipPosCheck((s32) MenuWorkUp.chrNo, (s32) MenuWorkUp.weaponId, (s32) MenuWorkUp.weaponSlot[MenuWorkUp.selectedIndex], (s32) MenuWorkUp.weaponHand);
                    if (temp_2_7 != 0) {
                        temp_2_8 = (AgwsUnitOrgUp *)func_A191C0((s32) MenuWorkUp.chrNo);
                        temp_21_2 = MenuRWeaponCheck2(temp_2_8->weaponId[MenuWorkUp.weaponHand]);
                        temp_19_2 = temp_2_8->ammoId[MenuWorkUp.weaponHand];
                        temp_2_8->weaponId[MenuWorkUp.weaponHand] = MenuRWeaponCheck(MenuWorkUp.weaponId, temp_2_7);
                        temp_2_8->hand[MenuWorkUp.weaponHand] = temp_2_7;
                        temp_2_8->ammoId[MenuWorkUp.weaponHand] = ((WeaponWaglDataUp *)func_A1A3D8(MenuWorkUp.weaponId))->defaultAmmoId;
                        if (temp_21_2 != 0) {
                            dataWpnBoxInc(temp_21_2);
                        }
                        dataWpnBoxDec(MenuWorkUp.weaponId);
                        if (temp_19_2 != 0) {
                            if (MenuBulletCheck(temp_21_2, -1) != 0) {
                                if (((WeaponWaglDataUp *)func_A1A3D8(temp_21_2))->defaultAmmoId == temp_19_2) {
                                    if (dataBltBoxChk(temp_19_2) == 0) {
                                        dataBltBoxInc(temp_19_2);
                                    }
                                } else {
                                    dataBltBoxInc(temp_19_2);
                                }
                            }
                        }
                        MenuAgwsParaSet((s32) MenuWorkUp.chrNo, 0);
                        MenuWorkUp.pendingState = (u8) 0x40;
                        MenuWorkUp.transitionTicks = (u8) 0x10;
                        xglSoundEffectNormalID(0x10, 0);
                    } else {
                        xglSoundEffectNormalID(5, 0);
                    }
                }
                if (AgwsMenuPad.half_2a == 0x40) {
                    MenuAgwsParaSet((s32) MenuWorkUp.chrNo, 0);
                    MenuWorkUp.pendingState = (u8) 0x42;
                    MenuWorkUp.weaponChange = (s8) 2;
                    MenuWorkUp.transitionTicks = (u8) 0x10;
                    xglSoundEffectNormalID(2, 0);
                    if (MenuCursorKeepCheck() == 0) {
                        MenuWorkUp.selectedIndex = (s8) 0;
                    }
                    {
                        int savedSelection;
                        for (savedSelection = 0; savedSelection < 8; savedSelection++)
                            WindowSPKeepSelectCheck((unsigned char *)(void *)(&MenuKeepSelect[savedSelection * 5]));
                    }
                }
        }
        break;
    case 0x46:
        if (MenuWorkUp.flags & 1 & 0xFF) {
            AgwsUnitOrgUp *org = (AgwsUnitOrgUp *)func_A191C0(MenuWorkUp.chrNo);
            MenuWorkUp.weaponId = org->weaponId[MenuWorkUp.weaponHand];
            MenuAgwsListMake_Gun();
            MenuWorkUp.selectedAmmo = -1;
        } else if (MenuWorkUp.transitionTicks == 0) {
                int selection = WindowSPSelect((void *)(AgwsWinSP[0]), AgwsMenuPad.mappedRepeat);
                WindowSPKeepSelect((void *)(AgwsWinSP[0]), &MenuKeepSelect[0x1E]);
                if (MenuWorkUp.selectedAmmo != selection) {
                    AgwsUnitOrgUp *org = (AgwsUnitOrgUp *)func_A191C0(MenuWorkUp.chrNo);
                    MenuWorkUp.selectedAmmo = selection;
                    MenuWorkUp.ammoId = MenuSortGet(0, MenuWorkUp.selectedAmmo);
                    if (MenuBulletCheck(MenuWorkUp.weaponId, (short)MenuWorkUp.ammoId) != 0) {
                        short previousAmmo = org->ammoId[MenuWorkUp.weaponHand];
                        org->ammoId[MenuWorkUp.weaponHand] = MenuWorkUp.ammoId;
                        MenuAgwsParaSet(MenuWorkUp.chrNo, 1);
                        org->ammoId[MenuWorkUp.weaponHand] = previousAmmo;
                    } else {
                        MenuAgwsParaSet(MenuWorkUp.chrNo, 0);
                    }
                }
                if (AgwsMenuPad.half_2a & 0x20) {
                    if (MenuBulletCheck(MenuWorkUp.weaponId, (short)MenuWorkUp.ammoId) != 0) {
                        AgwsUnitOrgUp *org = (AgwsUnitOrgUp *)func_A191C0(MenuWorkUp.chrNo);
                        short previousAmmo = org->ammoId[MenuWorkUp.weaponHand];
                        short weapon;
                        org->ammoId[MenuWorkUp.weaponHand] = MenuWorkUp.ammoId;
                        MenuAgwsParaSet(MenuWorkUp.chrNo, 0);
                        if (previousAmmo != 0) {
                            weapon = org->weaponId[MenuWorkUp.weaponHand];
                            if (weapon != 0) {
                                if (((WeaponWaglDataUp *)func_A1A3D8(weapon))->defaultAmmoId != previousAmmo
                                        || dataBltBoxChk(previousAmmo) == 0)
                                    dataBltBoxInc(previousAmmo);
                            } else {
                                dataBltBoxInc(previousAmmo);
                            }
                        }
                        if ((short)MenuWorkUp.ammoId != 0) {
                            weapon = org->weaponId[MenuWorkUp.weaponHand];
                            if (weapon != 0
                                    && ((WeaponWaglDataUp *)func_A1A3D8(weapon))->defaultAmmoId != (short)MenuWorkUp.ammoId)
                                dataBltBoxDec((short)MenuWorkUp.ammoId);
                        }
                        MenuWorkUp.pendingState = 0x40;
                        MenuWorkUp.transitionTicks = 0x10;
                        xglSoundEffectNormalID(0x10, 0);
                    } else {
                        xglSoundEffectNormalID(5, 0);
                    }
                }
                if (AgwsMenuPad.half_2a & 0x40) {
                    MenuAgwsParaSet(MenuWorkUp.chrNo, 0);
                    MenuWorkUp.pendingState = 0x40;
                    MenuWorkUp.transitionTicks = 0x10;
                    xglSoundEffectNormalID(2, 0);
                    WindowSPKeepSelectCheck((unsigned char *)(void *)(&MenuKeepSelect[0x1E]));
                }
        }
        break;
    case 0x62:
        if (MenuWorkUp.flags & 1 & 0xFF) {
            MenuAgwsListMake_Acc();
            MenuWorkUp.selectedAccessory = -1;
        } else if (MenuWorkUp.transitionTicks == 0) {
                signed char selection = WindowSPSelect((void *)(AgwsWinSP[0]), AgwsMenuPad.mappedRepeat);
                unsigned short buttons;
                WindowSPKeepSelect((void *)(AgwsWinSP[0]), &MenuKeepSelect[0x23]);
                if (MenuWorkUp.selectedAccessory != selection) {
                    MenuWorkUp.selectedAccessory = selection;
                    MenuWorkUp.accessoryId = MenuSortGet(0, selection);
                    if (MenuAccessoryEquipCheck(MenuWorkUp.chrNo,
                                               (short)MenuWorkUp.accessoryId,
                                               MenuWorkUp.equipSlot) != 0) {
                        AgwsUnitOrgUp *org = (AgwsUnitOrgUp *)func_A191C0(MenuWorkUp.chrNo);
                        short previousAccessory = org->accessoryId[MenuWorkUp.equipSlot];
                        org->accessoryId[MenuWorkUp.equipSlot] = MenuWorkUp.accessoryId;
                        MenuAgwsParaSet(MenuWorkUp.chrNo, 1);
                        org->accessoryId[MenuWorkUp.equipSlot] = previousAccessory;
                    } else {
                        MenuAgwsParaSet(MenuWorkUp.chrNo, 0);
                    }
                }
                if (MenuWorkUp.selectedAccessory >= 0 && AgwsMenuPad.half_2a == 0x20) {
                    if (MenuAccessoryEquipCheck(MenuWorkUp.chrNo,
                                               (short)MenuWorkUp.accessoryId,
                                               MenuWorkUp.equipSlot) != 0) {
                        AgwsUnitOrgUp *org = (AgwsUnitOrgUp *)func_A191C0(MenuWorkUp.chrNo);
                        short previousAccessory = org->accessoryId[MenuWorkUp.equipSlot];
                        org->accessoryId[MenuWorkUp.equipSlot] = MenuWorkUp.accessoryId;
                        MenuAgwsParaSet(MenuWorkUp.chrNo, 0);
                        if (previousAccessory != 0)
                            dataAccBoxInc(previousAccessory);
                        dataAccBoxDec((short)MenuWorkUp.accessoryId);
                        MenuWorkUp.pendingState = 0x40;
                        xglSoundEffectNormalID(0x10, 0);
                    } else {
                        xglSoundEffectNormalID(5, 0);
                    }
                }
                buttons = AgwsMenuPad.half_2a;
                if (buttons == 0x40) {
                    MenuAgwsParaSet(MenuWorkUp.chrNo, 0);
                    MenuWorkUp.pendingState = buttons;
                    xglSoundEffectNormalID(2, 0);
                    WindowSPKeepSelectCheck((unsigned char *)(void *)(&MenuKeepSelect[0x23]));
                }
        }
        break;
    case 0xC2:                                      /* switch 1 */
        if (MenuWorkUp.flags & 1 & 0xFF) {
            MenuAgwsListMake_Pilot();
        } else {
            if (MenuWorkUp.transitionTicks == 0) {
                MenuWorkUp.listSelect = (s8) WindowSPSelect((void *)(AgwsWinSP[0]), AgwsMenuPad.mappedRepeat);
                WindowSPKeepSelect((void *)(AgwsWinSP[0]), MenuKeepSelect + 0x28);
                temp_22_2 = MenuAgwsPilotCheck((s32) MenuWorkUp.chrNo);
                temp_23_2 = MenuSortGet(0, (s32) MenuWorkUp.listSelect);
                temp_21_3 = (AgwsUnitOrgUp *)func_A191C0(temp_22_2);
                temp_19_3 = (AgwsUnitOrgUp *)func_A191C0((s32) temp_23_2);
                temp_17_3 = temp_21_3->agwsId;
                temp_16_2 = temp_19_3->agwsId;
                temp_21_3->agwsId = (s16) 0;
                temp_19_3->agwsId = (s16) (u16) MenuWorkUp.chrNo;
                MenuAgwsParaSet((s32) MenuWorkUp.chrNo, 1);
                temp_19_3->agwsId = (s16) temp_16_2;
                temp_21_3->agwsId = (s16) temp_17_3;
                if (AgwsMenuPad.half_2a & 0x20) {
                    if (temp_22_2 == 0) {
                        if (temp_19_3->agwsId == 0) {
                            temp_19_3->agwsId = (s16) (u16) MenuWorkUp.chrNo;
                            xglSoundEffectNormalID(0x12, 0);
                            MenuAgwsParaSet(MenuWorkUp.chrNo, 0);
                        } else {
                            xglSoundEffectNormalID(5, 0);
                        }
                    } else if (temp_22_2 == temp_23_2) {
                        temp_19_3->agwsId = (s16) 0;
                        xglSoundEffectNormalID(0x13, 0);
                        MenuAgwsParaSet((s32) MenuWorkUp.chrNo, 0);
                    } else {
                        if (temp_19_3->agwsId == 0) {
                            temp_19_3->agwsId = (s16) (u16) MenuWorkUp.chrNo;
                            temp_21_3->agwsId = (s16) 0;
                            xglSoundEffectNormalID(0x12, 0);
                            MenuAgwsParaSet(MenuWorkUp.chrNo, 0);
                        } else {
                            xglSoundEffectNormalID(5, 0);
                        }
                    }
                    MenuAgwsListMake_Pilot();
                }
                if (AgwsMenuPad.half_2a & 0x40) {
                    MenuAgwsParaSet((s32) MenuWorkUp.chrNo, 0);
                    MenuWorkUp.pendingState = (u8) 0x22;
                    MenuWorkUp.transitionTicks = (u8) 0x10;
                    xglSoundEffectNormalID(2, 0);
                    if (MenuCursorKeepCheck() == 0) {
                        MenuWorkUp.reserveCursor = (s8) 0;
                    }
                    WindowSPKeepSelectCheck((unsigned char *)(void *)(MenuKeepSelect + 0x28));
                }
            } else {
                MenuWorkUp.listSelect = (s8) WindowSPSelect((void *)(AgwsWinSP[0]), 0U);
                WindowSPKeepSelect((void *)(AgwsWinSP[0]), MenuKeepSelect + 0x28);
            }
        }
        break;
    case 0xF0:                                      /* switch 1 */
        MenuWorkUp.state = (u8) -0xF;
        MenuWorkUp.pendingState = (u8) -0xF;
        MenuWorkUp.transitionTicks = (u8) 0xC;
        /* fallthrough */
    case 0xF1:                                      /* switch 1 */
        if (MenuWorkUp.transitionTicks != 0)
            break;
        MenuWorkUp.state = (u8) -1;
        /* fallthrough */
    case 0xFF:
        ChangeTopLevel(0);
        MenuModelAllBreak();
        break;
    }
    AgwsPasMain();
    AgwsInfoMain();
    AgwsSelectMain();
    AgwsListMain();
    AgwsStatusMain();
    AgwsWeapon2Main();
    AgwsSwitchMain();
    AgwsFaceMain();
    AgwsNameMain();
    xglFontDebugPrintf(0, 0x38, (s8 *) D_004DAFF0);
    xglFontDebugHex(0, 0x48, MenuWorkUp.state, 2);
    xglFontDebugHex(0, 0x60, MenuWorkUp.menuSelection, 2);
    xglFontDebugHex(0, 0x68, MenuWorkUp.selectedAgws, 2);
    xglFontDebugHex(0, 0x70, MenuWorkUp.listKind, 1);
    xglFontDebugHex(0, 0x78, MenuWorkUp.reserveCursor, 2);
    xglFontDebugHex(0, 0x80, MenuWorkUp.weaponHand, 2);
    xglFontDebugHex(0, 0x88, MenuWorkUp.equipSlot, 2);
    xglFontDebugHex(0, 0x90, MenuWorkUp.agwsCount, 1);
    xglFontDebugPrintf(0x40, 0x50, (s8 *) D_004DAFF8, MenuWorkUp.selectedWeapon);
    xglFontDebugPrintf(0x40, 0x58, (s8 *) D_004DAFF8, MenuWorkUp.selectedAmmo);
    xglFontDebugPrintf(0x40, 0x60, (s8 *) D_004DAFF8, MenuWorkUp.selectedAccessory);
    xglFontDebugPrintf(0x40, 0x68, (s8 *) D_004DAFF8, MenuWorkUp.listSelect);
    xglFontDebugPrintf(0x40, 0x78, (s8 *) D_004C72D0, MenuWorkUp.pilotId);
}

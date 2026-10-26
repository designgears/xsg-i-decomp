#include "common.h"
#include "shared.h"

#define MENU_ITEM_SPECIAL_ID 36
#define MENU_ITEM_USE_ALLOWED 0x8000
#define MENU_ITEM_USE_CATEGORY_BITS 0x1e43
#define MENU_ITEM_USE_SPECIAL_BIT 0x2000
#define MENU_ITEM_USE_STATE_BIT 0x10
#define MENU_ITEM_SPECIAL_ENABLED_BITS 0x20400000

typedef struct {
    u8 unmodeled_00[4];
    u16 use_flags; /* The high bit gates all item availability checks. */
    u8 unmodeled_06[2];
    u8 use_state;
    u8 unmodeled_09[3];
    u16 attributes;
} MenuItemUseData;

typedef struct {
    u8 unmodeled_00[0x10];
    u64 flags;
} MenuItemLoopState;

extern MenuItemUseData *func_A1A4E8(s16 itemId);
extern MenuItemLoopState GameLoopState;

int MenuItemUseCheck(s16 itemId)
{
    MenuItemUseData *item;
    u16 attributes;
    int result;
    /* A special item is unavailable when both game-loop bits are clear. */
    int specialUnavailable = -1;

    if (itemId == 0) {
        return 0;
    }
    item = func_A1A4E8(itemId);
    if ((item->use_flags & MENU_ITEM_USE_ALLOWED) == 0) {
        return 0;
    }
    if (itemId == MENU_ITEM_SPECIAL_ID) {
        result = (GameLoopState.flags & MENU_ITEM_SPECIAL_ENABLED_BITS) ? 2 : specialUnavailable;
    } else {
        attributes = item->attributes;
        if (attributes & MENU_ITEM_USE_CATEGORY_BITS) {
            result = (item->use_state & MENU_ITEM_USE_STATE_BIT) ? 2 : 1;
        } else {
            result = (attributes & MENU_ITEM_USE_SPECIAL_BIT) ? 10 : 0;
        }
    }
    return result;
}

typedef struct MenuItemPasMsg {
    unsigned char state;
    unsigned char mode;
    unsigned char pad2[2];
    short x;
    short y;
    int color;
    unsigned char pad[0x44 - 0xC];
} MenuItemPasMsg;
typedef struct MenuItemPasRect {
    short x;
    short y;
    int color;
    short w;
    short h;
} MenuItemPasRect;
typedef struct MenuItemPasWork {
    unsigned char state;
    unsigned char pad1[3];
    short x;
    short y;
    int color;
    short wx;
    short wy;
    int wcolor;
    short ww;
    short wh;
    unsigned char pad18[4];
    unsigned char wstate;
    unsigned char pad1d[3];
    void (*wfunc)(void);
    void *wbuf;
    unsigned char pad28[0x1A0 - 0x28];
    MenuItemPasMsg msg[9];
    MenuItemPasRect rect;
    unsigned char buf[4];
} MenuItemPasWork;
extern MenuItemPasWork *MenuItemPas;
extern char *msg00_0_0036D670[];
typedef struct MenuItemPasMenuWork {
    unsigned char menu_state[0x30];
    signed char select;
} MenuItemPasMenuWork;
struct MenuItemListMenuWork;
extern struct MenuItemListMenuWork MenuWork;
extern void WindowDXSet(void *window);
extern void WindowDXMain(void *window);
extern void MenuPasWindow(void);
extern void eMessageSet(void *message, const char *text);
extern void eMessageMain(void *message);
extern void MoveSlide(short *value, short *destination, float speed);
extern void endPrintExtFunc(int color, int id, void *data);

void MenuItemPasMain(void)
{
  MenuItemPasWork *w = MenuItemPas;
  short target[9];
  short slide[1];
  short *win;
  MenuItemPasMsg *m;
  int *col;
  int *c;
  short *t;
  short *xp;
  MenuItemPasMsg *e;
  MenuItemPasWork *p;
  char **msg;
  int i;
  unsigned char *mw;

  switch (w->state)
  {
    case 0:
      w->x = -0x110;
      w->y = 8;
      w->color = 0xFFFFF0;
      win = &w->wx;
      WindowDXSet(win);
      *win = w->x;
      w->wcolor = w->color;
      w->wy = w->y;
      w->ww = 0x110;
      w->wh = 0x1E;
      w->wfunc = MenuPasWindow;
      w->wbuf = w->buf;
      w->wstate = 1;
      WindowDXMain(win);
      w->wstate = 3;
      for (i = 0; i < 9; i++)
      {
        eMessageSet(&w->msg[i], msg00_0_0036D670[i]);
        w->msg[i].mode = 0x20;
        w->msg[i].x = 0x120;
        w->msg[i].color = w->color + 2;
        w->msg[i].y = w->y + 3;
      }
      w->state = 2;
      goto main;
    case 2:
      win = &w->wx;
    main:
      slide[0] = -0x10;
      for (i = 0; i < 9; i++)
        target[i] = 0x120;
      switch (((MenuItemPasMenuWork *) &MenuWork)->menu_state[3])
      {
        case 0x20:
        case 0x22:
        case 0x40:
        case 0x80:
          target[((MenuItemPasMenuWork *) &MenuWork)->select] = 0x10;
          break;
        case 0x30:
          target[8] = 0x10;
          break;
        default:
          slide[0] = -0x110;
          break;
      }
      MoveSlide(win, slide, 3.0f);
      WindowDXMain(win);
      w->rect.x = *win + 3;
      w->rect.y = w->wy + 3;
      w->rect.w = w->ww - 6;
      w->rect.h = w->wh - 6;
      w->rect.color = w->color;
      endPrintExtFunc(w->color, 0x65, &w->rect);
      p = w;
      t = target;
      for (i = 8; i >= 0; i--)
      {
        xp = &p->msg[0].x;
        MoveSlide(xp, t, 3.0f);
        e = p->msg;
        t++;
        p = (MenuItemPasWork *) ((char *) p + 0x44);
        if (*xp < 0x100)
          eMessageMain(e);
      }
      endPrintExtFunc(w->color, 0x66, 0);
      break;
  }
}

typedef struct WindowDX WindowDX;
typedef struct MenuItemPasWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    u8 unmodeled_0c[0x10 - 0x0C];
    signed char state;
    u8 unmodeled_11[0x14 - 0x11];
    void (*callback)(WindowDX *window, void *work);
    void *callbackArg;
    u8 unmodeled_1c[0x194 - 0x1C];
} MenuItemPasWindow;
typedef struct MenuItemInfoText {
    unsigned short x;
    unsigned short y;
    unsigned char unmodeled_04[8];
    const char *message;
    unsigned char unmodeled_10[0x1B0 - 0x10];
} MenuItemInfoText;
typedef struct MenuItemInfoWork {
    unsigned char state;
    unsigned char unmodeled_01[2];
    unsigned char help;
    unsigned char unmodeled_04[4];
    int color;
    MenuItemPasWindow window;
    MenuItemInfoText text;
    char buffer[128];
} MenuItemInfoWork;
typedef struct MenuItemInfoLabels {
    const char *sortMethods[3];
    const char *sortPrefix;
    const char *cancel;
} MenuItemInfoLabels;
extern const MenuItemInfoLabels D_004C4ED0;
typedef struct MenuItemInfoHelpLabels {
    const char *unavailable;
    const char *restricted;
} MenuItemInfoHelpLabels;
extern const MenuItemInfoHelpLabels D_004DAAC8[];
typedef struct MenuItemInfoPad {
    unsigned char unmodeled_00[0x2A];
    unsigned short pressed;
    unsigned char unmodeled_2C[8];
    unsigned short activity;
} MenuItemInfoPad;
extern const unsigned char D_004DAAD0[];
extern const char D_004DAAD8[];
typedef struct MenuItemInfoRow {
    unsigned char unmodeled_00[9];
    signed char state;
    unsigned char unmodeled_0a[2];
} MenuItemInfoRow;
typedef struct MenuTextEntry {
    const char *name;
    const unsigned char *auxiliary;
    const unsigned char *sortName;
} MenuTextEntry;
typedef struct MenuItemWorkData {
    u8 unmodeled_00[3];
    u8 mode;
    u8 unmodeled_04[0x20 - 0x04];
    u8 flags;
    unsigned char unmodeled_021[0x22 - 0x21];
    u8 busy; /* +0x22 */
    unsigned char unmodeled_023[0x30 - 0x23];
    signed char listTab;
    signed char sortAbility;
    signed char sortNormal;
    signed char sortCategory[3];
    u8 unmodeled_36[0x1A];
    int selectDir;
    u8 unmodeled_54[0x62 - 0x54];
    signed char agwsMode;
    signed char selectedAgws;
    signed char agwsCount;
    u8 unmodeled_65[0x72 - 0x65]; /* +0x65 */
    signed char sortSelection; /* +0x72 */
} MenuItemWorkData;
extern void *MenuItemInfo;
extern struct MenuItemListRow *MenuListGet(int list);
extern MenuTextEntry *MenuTextGet(int index);
extern int MenuSortGet(int listIndex, int entryIndex);
extern void MenuInfoWindow(void);

void MenuItemInfoMain(void)
{
    extern MenuItemWorkData MenuWorkAlias __asm__("MenuWork");
    extern MenuItemInfoPad PadDataAlias __asm__("PadData");
    MenuItemInfoWork *info = MenuItemInfo;
    MenuItemInfoLabels labels = D_004C4ED0;
    MenuItemInfoHelpLabels helpLabels = D_004DAAC8[0];
    short targetY;
    short targetHeight;

    switch (info->state) {
    case 0: {
        signed char windowState;
        unsigned short *textX;

        info->color = 0xFFFFF0;
        WindowDXSet((WindowDX *)&info->window);
        info->window.x = -16;
        info->window.y = 480;
        info->window.width = 544;
        info->window.height = 54;
        info->window.color = info->color;
        info->window.callback = (void (*)(WindowDX *, void *))MenuInfoWindow;
        windowState = 1;
        info->window.callbackArg = &info->text;
        textX = info->window.callbackArg;
        *textX = 0;
        info->window.state = windowState;
        info->text.y = 0;
        info->text.message = 0;
        WindowDXMain((WindowDX *)&info->window);
        info->window.state = 3;
        info->help = 0;
        memset(info->buffer, 0, sizeof(info->buffer));
        info->state = 2;
    }
        /* fall through */
    case 2:
        break;
    default:
        return;
    }

    targetY = 384;
    targetHeight = 54;
    if (MenuWorkAlias.flags & 1) {
        info->help = 0;
    }
    if (info->help == 0) {
        if (PadDataAlias.pressed & 32) {
            info->help = 1;
        }
    } else if (PadDataAlias.activity != 0) {
        info->help = 0;
    }

    switch (MenuWorkAlias.mode) {
    case 32:
    case 34:
    case 64:
        if (MenuWorkAlias.busy == 0) {
            if (MenuWorkAlias.selectDir >= 0) {
                strcpy(info->buffer, (const char *)MenuTextGet(MenuSortGet(0, MenuWorkAlias.selectDir))->auxiliary);
                if (MenuWorkAlias.listTab == 0 && info->help == 1) {
                    signed char use = ((MenuItemInfoRow *)MenuListGet(0))[MenuWorkAlias.selectDir].state;

                    if (use == 1) {
                        strcpy(info->buffer, helpLabels.unavailable);
                    } else if (use == 2) {
                        strcpy(info->buffer, helpLabels.restricted);
                    }
                }
            } else {
                info->buffer[0] = D_004DAAD0[0];
            }
        }
        break;
    case 128:
        if (MenuWorkAlias.sortSelection == 3) {
            strcpy(info->buffer, labels.cancel);
        } else {
            strcpy(info->buffer, labels.sortPrefix);
            strcat(info->buffer, labels.sortMethods[MenuWorkAlias.sortSelection]);
            strcat(info->buffer, D_004DAAD8);
        }
        break;
    case 48:
        targetY = 408;
        targetHeight = 30;
        break;
    default:
        targetY = 464;
        break;
    }

    info->text.message = info->buffer;
    MoveSlide(&info->window.y, &targetY, 3.0f);
    MoveSlide(&info->window.height, &targetHeight, 3.0f);
    WindowDXMain((WindowDX *)&info->window);
}

typedef struct {
    short x;
    short y;
    int color;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} MenuItemLineEntry;
typedef struct {
    unsigned char state;
    unsigned char pad1[2];
    unsigned char alpha;
    int color;
    MenuItemLineEntry line[5];
    int pad2;
    int count;
} MenuItemLineWork;
extern MenuItemLineWork *MenuItemLine;
typedef struct { int v[5]; } MenuItemLinePos;
extern MenuItemLinePos D_004C4F38;
struct MenuItemListMenuWork;
extern struct MenuItemListMenuWork MenuWork;
extern void MoveSlide(short *value, short *destination, float speed);
extern void endPrintExtFunc(int color, int id, void *data);

void MenuItemLineMain(void)
{
  MenuItemLineWork *ln = MenuItemLine;
  MenuItemLinePos pos;
  short target[8];
  short target2[8];
  int i;
  unsigned char *mw;
  int n;
  short *t;
  unsigned char *e;
  switch (ln->state)
  {
    case 0:
      ln->color = 0xFFFFF0;
      pos = D_004C4F38;
      for (i = 0; i < 5; i++)
      {
        ln->line[i].b = 0x60;
        ln->line[i].g = 0x60;
        ln->line[i].r = 0x60;
        ln->line[i].a = 0x80;
        ln->line[i].y = 0x30;
        ln->line[i].x = pos.v[i];
        ln->line[i].color = ln->color;
      }
      ln->count = 0;
      ln->state = 2;
      ln->alpha = 0;

    case 2:
      for (i = 3; i >= 0; i--)
        target[i] = 0;
      target2[0] = 0x30;
      target[3] = 0x200;
      mw = (unsigned char *) (&MenuWork);
      n = ((signed char *) mw)[0x30] * 22;
      target[0] = n + 0x12E;
      target[1] = n + 0x132;
      target[2] = n + 0x136;
      if (target[0] == ln->line[1].x)
        target2[0] = 0x28;
      switch (mw[3])
      {
        case 0x20:
        case 0x22:
        case 0x30:
        case 0x40:
        case 0x80:
          if (ln->alpha != 0x78)
            ln->alpha += 8;
          break;
        default:
          if (ln->alpha != 0)
            ln->alpha -= 8;
          break;
      }
      for (i = 1; i < 5; i++)
      {
        MoveSlide(&ln->line[i].x, &target[i - 1], 3.0f);
        ln->line[i].a = ln->alpha;
      }
      ln->line[0].a = ln->alpha;
      MoveSlide(&ln->line[2].y, target2, 8.0f);
      endPrintExtFunc(ln->color, 0, ln->line);
      break;
  }
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct MenuItemMessage {
    u8 active;
    u8 mode;
    u8 unmodeled_02[2];
    short x;
    short y;
    int color;
} MenuItemMessage;

typedef struct MenuItemPasMessage {
    u8 unmodeled_00;
    u8 mode;
    u8 unmodeled_02[2];
    short x;
    short y;
    int color;
    u8 unmodeled_0c[0x44 - 0x0C];
} MenuItemPasMessage;

typedef struct MenuItemPasBox {
    short x;
    short y;
    int color;
    short width;
    short height;
} MenuItemPasBox;

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct MenuItemAgwsTag {
    u8 unmodeled_00[4];
    short x;
    short y;
    int work;
    signed char color[4];
    struct {
        short x;
        short y;
        int work;
        signed char color[4];
        const char *text;
    } render;
} MenuItemAgwsTag;

typedef struct MenuItemAgwsNumber {
    unsigned short x;
    unsigned short y;
    int work;
    u8 unmodeled_08[6];
    u8 format;
    u8 digitCount;
    u8 unmodeled_10[4];
    int value;
    u8 unmodeled_18[0x90 - 0x18];
} MenuItemAgwsNumber;

typedef struct MenuItemAgwsRow {
    MenuItemPasWindow window;
    MenuItemPasMessage name;
    MenuItemAgwsTag hpLabel;
    MenuItemAgwsNumber hpNumber;
} MenuItemAgwsRow;

typedef struct MenuItemAgwsCursor {
    u8 mode;
    u8 unmodeled_01[3];
    short x;
    short y;
    int work;
} MenuItemAgwsCursor;

typedef struct MenuItemAgwsStatusWork {
    u8 state;
    u8 unmodeled_01[3];
    int color;
    MenuItemAgwsRow rows[6];
    MenuItemAgwsCursor cursor;
} MenuItemAgwsStatusWork;

typedef struct MenuItemHpParameters {
    short maxHp;
    u8 unmodeled_02[0x34 - 0x02];
    short hp;
} MenuItemHpParameters;

typedef struct MenuItemWorkData_SM {
    u8 unmodeled_00[3];
    u8 mode;
    u8 unmodeled_04[0x20 - 0x04];
    u8 flags;
    u8 nextMode;
    u8 busy;
    u8 partyCount;
    signed char selectionMode;
    u8 unmodeled_25[0x2E - 0x25];
    short selectedItem;
    signed char listTab;
    signed char sortChoices[8]; /* +0x31 */
    u8 unmodeled_39[0x40 - 0x39];
    signed char tabChangeDirection;
    signed char scrollDir;
    u8 unmodeled_42[0x50 - 0x42];
    int selectDir;
    u8 unmodeled_54[0x62 - 0x54];
    signed char agwsMode;
    signed char selectedAgws;
    signed char agwsCount;
    u8 unmodeled_65[0x70 - 0x65];
    signed char selectedParty;
    u8 unmodeled_71; /* +0x65 */
    signed char sortSelection; /* +0x72 */
    u8 unmodeled_73;
    u8 segmentSelection; /* +0x74 */
} MenuItemWorkData_SM;
typedef struct MenuItemPasWindow_SM {
    short x;
    short y;
    int color;
    short width;
    short height;
    const char *tag_id;
    signed char state;
    u8 ribbon_initialized; /* +0x11 */
    u8 unmodeled_12[0x14 - 0x12];
    void (*callback)(void *window, void *work);
    void *callbackArg;
    u8 unmodeled_1c[0x194 - 0x1C];
} MenuItemPasWindow_SM;
extern void *MenuItemStatus;
typedef struct MenuItemStatusContext {
    signed char characterId;
    u8 mode;
    short alternateId;
    short x;
    short y;
    short initialControls[2];
    u8 unmodeled_0c[12];
    int firstTextStyle;
    u8 unmodeled_1c[0x840 - 0x1C];
} MenuItemStatusContext;
typedef struct MenuItemStatusCursor {
    u8 state;
    u8 unmodeled_01[3];
    short x;
    short y;
    int work;
    u8 unmodeled_0c[0x24 - 0x0C];
} MenuItemStatusCursor;
typedef struct MenuItemPartyStatusWork {
    u8 state;
    unsigned char visibleCount;
    signed char scrollStart;
    u8 scrollTick;
    u8 selectionRow;
    u8 unmodeled_05[3];
    int style;
    MenuItemPasWindow_SM windows[8];
    MenuItemStatusContext contexts[8];
    MenuItemStatusCursor cursors[3];
    /* Two backgrounds, two outlines, then the clipping rectangle. */
    MenuItemPasBox boxes[5];
} MenuItemPartyStatusWork;
extern short MenuItemParty[];
extern char *MenuTagTextGet(int index);
extern int PartyAttackerCheck(int characterId);
extern void eCursolSet(void *cursor, int mode);
extern void eCursolMain(void *cursor);
void MenuStatusDisp();

void MenuItemStatusMain(void)
{
    extern MenuItemWorkData_SM MenuWork_SM __asm__("MenuWork");
    MenuItemPartyStatusWork *status = MenuItemStatus;
    short targetX[8];
    short targetY[8];
    int i;

    switch (status->state) {
    case 0: {
        status->style = 0xF000F0;
        i = 0;
        if (MenuWork_SM.partyCount != 0) {
            short *party = MenuItemParty;

            do {
                WindowDXSet((WindowDX *)&status->windows[i]);
                status->windows[i].callback = (void (*)(void *, void *))MenuStatusDisp;
                status->windows[i].x = -272;
                status->windows[i].width = 240;
                status->windows[i].height = 82;
                status->windows[i].y = 64 + i * 104;
                status->windows[i].callbackArg = &status->contexts[i];
                status->windows[i].color = status->style;
                if (PartyAttackerCheck(*party)) {
                    status->windows[i].tag_id = MenuTagTextGet(1);
                } else {
                    status->windows[i].tag_id = MenuTagTextGet(2);
                }
                status->windows[i].ribbon_initialized = 1;
                status->contexts[i].characterId = *party;
                status->contexts[i].x = 0;
                status->contexts[i].y = 0;
                status->contexts[i].mode = 0;
                status->contexts[i].initialControls[0] = 0;
                status->contexts[i].initialControls[1] = 31;
                status->windows[i].state = 1;
                WindowDXMain((WindowDX *)&status->windows[i]);
                status->windows[i].state = 3;
                party++;
            } while (++i < MenuWork_SM.partyCount);
        }
        for (i = 0; i < 3; i++) {
            eCursolSet((void *)(&status->cursors[i]), i);
            status->cursors[i].state = 32;
            status->cursors[i].work = status->style + 16;
        }
        status->boxes[0].x = 0;
        status->boxes[0].width = 256;
        status->boxes[0].y = 0;
        status->boxes[0].height = 48;
        status->boxes[1].x = 0;
        status->boxes[1].width = 256;
        status->boxes[1].y = 354;
        status->boxes[1].height = 158;
        for (i = 0; i < 2; i++) {
            status->boxes[i].color = 0xFFFFFF;
            status->boxes[i + 2].color = 0;
        }
        status->scrollTick = 0;
        status->scrollStart = 0;
        status->selectionRow = 0;
        status->visibleCount = MenuWork_SM.partyCount;
        if (status->visibleCount >= 4) {
            status->visibleCount = 3;
        }
        status->state = 2;
    }
        /* fall through */
    case 2:
        break;
    default:
        return;
    }

    for (i = 0; i < MenuWork_SM.partyCount; i++) {
        targetX[i] = -272;
        targetY[i] = 64 + (i - status->scrollStart) * 104;
    }
    switch (MenuWork_SM.mode) {
    case 32:
        if (MenuWork_SM.flags & 1) {
            status->cursors[0].state = 0;
        }
        if (MenuWork_SM.listTab == 0 && MenuWork_SM.agwsMode == 0) {
            for (i = 0; i < MenuWork_SM.partyCount; i++) {
                targetX[i] = 16;
            }
        }
        break;
    case 34:
        if (MenuWork_SM.flags & 1) {
            status->cursors[0].state = 32;
        }
        if (MenuWork_SM.agwsMode == 0) {
            for (i = 0; i < MenuWork_SM.partyCount; i++) {
                targetX[i] = 16;
            }
            if (MenuWork_SM.selectionMode == 1) {
                if (MenuWork_SM.partyCount >= 4) {
                    if (MenuWork_SM.scrollDir == -1) {
                        if (status->scrollTick == 0) {
                            if (status->scrollStart != 0) {
                                status->scrollStart--;
                            }
                        } else {
                            status->scrollTick--;
                        }
                        MenuWork_SM.scrollDir = 2;
                    } else if (MenuWork_SM.scrollDir == 1) {
                        if (status->scrollTick == 2) {
                            if (status->scrollStart != MenuWork_SM.partyCount - 3) {
                                status->scrollStart++;
                            }
                        } else {
                            status->scrollTick++;
                        }
                        MenuWork_SM.scrollDir = 2;
                    }
                    for (i = 0; i < MenuWork_SM.partyCount; i++) {
                        targetY[i] = 64 + (i - status->scrollStart) * 104;
                    }
                }
            } else {
                int rowCount = MenuWork_SM.partyCount;

                if (rowCount >= 4) {
                    rowCount = 3;
                }
                if (status->selectionRow == rowCount - 1) {
                    status->selectionRow = 0;
                } else {
                    status->selectionRow++;
                }
            }
        }
        break;
    }

    for (i = 0; i < 2; i++) {
        endPrintExtFunc(status->style, 12, &status->boxes[i]);
    }
    status->boxes[4].x = 0;
    status->boxes[4].width = 256;
    status->boxes[4].y = 64;
    status->boxes[4].height = 272;
    endPrintExtFunc(status->style, 101, &status->boxes[4]);
    for (i = 0; i < MenuWork_SM.partyCount; i++) {
        MoveSlide(&status->windows[i].x, &targetX[i], 3.0f);
        MoveSlide(&status->windows[i].y, &targetY[i], 3.0f);
        if (status->windows[i].y == targetY[i]) {
            MenuWork_SM.scrollDir = 0;
        }
        WindowDXMain((WindowDX *)&status->windows[i]);
    }
    if (MenuWork_SM.selectionMode == 1) {
        status->cursors[0].x = status->windows[MenuWork_SM.selectedParty].x + 3;
        status->cursors[0].y = status->windows[MenuWork_SM.selectedParty].y + status->windows[MenuWork_SM.selectedParty].height / 2 - 4;
        eCursolMain((void *)(&status->cursors[0]));
        if (MenuWork_SM.partyCount >= 4 && status->cursors[0].state != 0) {
            if (status->scrollStart != 0) {
                status->cursors[1].x = 240;
                status->cursors[1].y = 50;
                eCursolMain((void *)(&status->cursors[1]));
            }
            if (status->scrollStart != MenuWork_SM.partyCount - 3) {
                status->cursors[2].x = 240;
                status->cursors[2].y = 338;
                eCursolMain((void *)(&status->cursors[2]));
            }
        }
    } else if (MenuWork_SM.selectionMode == 2) {
        status->cursors[0].x = status->windows[MenuWork_SM.selectedParty].x + 3;
        status->cursors[0].y = status->selectionRow * 104 + status->windows[MenuWork_SM.selectedParty].height / 2 + 60;
        eCursolMain((void *)(&status->cursors[0]));
    }
    for (i = 0; i < 2; i++) {
        status->boxes[i + 2].x = status->boxes[i].x;
        status->boxes[i + 2].width = status->boxes[i].width;
        status->boxes[i + 2].y = status->boxes[i].y;
        status->boxes[i + 2].height = status->boxes[i].height;
        endPrintExtFunc(status->style, 12, &status->boxes[i + 2]);
    }
    endPrintExtFunc(status->style, 102, 0);
}

typedef void WindowDXSA;
typedef struct MenuItemPasWindowSA {
    short x;
    short y;
    int color;
    short width;
    short height;
    u8 unmodeled_0c[0x10 - 0x0C];
    signed char state;
    u8 unmodeled_11[0x14 - 0x11];
    void (*callback)(WindowDXSA *window, void *work);
    void *callbackArg;
    u8 unmodeled_1c[0x194 - 0x1C];
} MenuItemPasWindowSA;
typedef struct MenuItemPasMessageSA {
    u8 unmodeled_00;
    u8 mode;
    u8 unmodeled_02[2];
    short x;
    short y;
    int color;
    u8 unmodeled_0c[0x44 - 0x0C];
} MenuItemPasMessageSA;
typedef struct MenuItemAgwsTagSA {
    u8 unmodeled_00[4];
    short x;
    short y;
    int work;
    signed char color[4];
    struct {
        short x;
        short y;
        int work;
        signed char color[4];
        const char *text;
    } render;
} MenuItemAgwsTagSA;
typedef struct MenuItemAgwsNumberSA {
    unsigned short x;
    unsigned short y;
    int work;
    u8 unmodeled_08[6];
    u8 format;
    u8 digitCount;
    u8 unmodeled_10[4];
    int value;
    u8 unmodeled_18[0x90 - 0x18];
} MenuItemAgwsNumberSA;
typedef struct MenuItemAgwsRowSA {
    MenuItemPasWindowSA window;
    MenuItemPasMessageSA name;
    MenuItemAgwsTagSA hpLabel;
    MenuItemAgwsNumberSA hpNumber;
} MenuItemAgwsRowSA;
typedef struct MenuItemAgwsCursorSA {
    u8 mode;
    u8 unmodeled_01[3];
    short x;
    short y;
    int work;
} MenuItemAgwsCursorSA;
typedef struct MenuItemAgwsStatusWorkSA {
    u8 state;
    u8 unmodeled_01[3];
    int color;
    MenuItemAgwsRowSA rows[6];
    MenuItemAgwsCursorSA cursor;
} MenuItemAgwsStatusWorkSA;
typedef struct MenuItemHpParametersSA {
    short maxHp;
    u8 unmodeled_02[0x34 - 0x02];
    short hp;
} MenuItemHpParametersSA;
typedef struct MenuItemWorkDataSA {
    u8 unmodeled_00[3];
    u8 mode;
    u8 unmodeled_04[0x20 - 0x04];
    u8 flags;
    unsigned char unmodeled_021[0x22 - 0x21];
    u8 busy; /* +0x22 */
    unsigned char unmodeled_023[0x30 - 0x23];
    signed char listTab;
    signed char sortAbility;
    signed char sortNormal;
    signed char sortCategory[3];
    u8 unmodeled_36[0x1A];
    int selectDir;
    u8 unmodeled_54[0x62 - 0x54];
    signed char agwsMode;
    signed char selectedAgws;
    signed char agwsCount;
    u8 unmodeled_65[0x72 - 0x65]; /* +0x65 */
    signed char sortSelection; /* +0x72 */
} MenuItemWorkDataSA;
extern void *MenuItemStatusAgws;
extern short MenuItemAgws[];
extern const char D_004DAAE0[];
extern const char *MenuCharNameGet(int character);
extern void eTagFontSet(void *tag, const char *text);
extern void eTagFontMain(void *tag);
extern void eNumberSet(void *number, int mode);
extern void eNumberMain(void *number);
extern void eCursolSet(void *cursor, int mode);
extern void eCursolMain(void *cursor);
extern MenuItemHpParametersSA *func_00A11108(int character, int *attack, int *defense);

void MenuItemStatusAgwsMain(void)
{
    extern MenuItemWorkDataSA MenuWorkSA __asm__("MenuWork");
    MenuItemAgwsStatusWorkSA *status = MenuItemStatusAgws;
    short targetX[6];
    int attack[3];
    int defense[2];
    register int i;

    switch (status->state) {
    case 0: {
        status->color = 0xF000F0;
        for (i = 0; i < MenuWorkSA.agwsCount; i++) {
            WindowDXSet((WindowDXSA *)&status->rows[i].window);
            status->rows[i].window.width = 240;
            status->rows[i].window.height = 50;
            status->rows[i].window.color = status->color;
            status->rows[i].window.x = -272;
            status->rows[i].window.state = 1;
            status->rows[i].window.y = i * status->rows[i].window.height + 64;
            WindowDXMain((WindowDXSA *)&status->rows[i].window);
            status->rows[i].window.state = 3;
            eMessageSet(&status->rows[i].name, MenuCharNameGet(MenuItemAgws[i]));
            status->rows[i].name.mode = 32;
            status->rows[i].name.color = status->rows[i].window.color + 2;
            eTagFontSet(&status->rows[i].hpLabel, D_004DAAE0);
            status->rows[i].hpLabel.work = status->color + 2;
            status->rows[i].hpLabel.color[0] = 112;
            eNumberSet(&status->rows[i].hpNumber, 0);
            status->rows[i].hpNumber.work = status->color + 2;
            status->rows[i].hpNumber.format = 12;
            status->rows[i].hpNumber.digitCount = 4;
        }
        eCursolSet(&status->cursor, 0);
        status->cursor.mode = 32;
        status->state = 2;
        status->cursor.work = status->color + 4;
    }
        /* fall through */
    case 2:
        break;
    default:
        return;
    }

    for (i = 0; i < MenuWorkSA.agwsCount; i++) {
        targetX[i] = -272;
    }
    if (MenuWorkSA.mode == 32 || MenuWorkSA.mode == 34) {
        if (MenuWorkSA.flags & 1) {
            if (MenuWorkSA.mode == 32) {
                status->cursor.mode = 0;
            } else {
                status->cursor.mode = 32;
            }
        }
        if (MenuWorkSA.agwsMode == 1) {
            for (i = 0; i < MenuWorkSA.agwsCount; i++) {
                targetX[i] = 16;
            }
        }
    }
    i = 0;
    while (i < MenuWorkSA.agwsCount) {
        MenuItemHpParametersSA *parameters = func_00A11108(MenuItemAgws[i], attack, defense);

        MoveSlide(&status->rows[i].window.x, &targetX[i], 3.0f);
        WindowDXMain((WindowDXSA *)&status->rows[i].window);
        status->rows[i].name.x = status->rows[i].window.x + 19;
        status->rows[i].name.y = status->rows[i].window.y + 3;
        eMessageMain(&status->rows[i].name);
        status->rows[i].hpLabel.x = status->rows[i].window.x + 123;
        status->rows[i].hpLabel.y = status->rows[i].window.y + 29;
        eTagFontMain(&status->rows[i].hpLabel);
        {
            unsigned int packedHp = parameters->hp + ((unsigned int)parameters->maxHp << 16);

            status->rows[i].hpNumber.value = packedHp;
            status->rows[i].hpNumber.x = status->rows[i].hpLabel.x + 52;
            status->rows[i].hpNumber.y = status->rows[i].hpLabel.y;
        }
        eNumberMain(&status->rows[i].hpNumber);
        i++;
    }
    if (MenuWorkSA.agwsCount != 0) {
        status->cursor.x = status->rows[MenuWorkSA.selectedAgws].window.x + 3;
        status->cursor.y = status->rows[MenuWorkSA.selectedAgws].window.y + 19;
        eCursolMain(&status->cursor);
    }
}

typedef struct MenuItemListRow {
    const char *name;
    int amount;
    unsigned char flag;
} MenuItemListRow;

typedef struct MenuItemListWindow {
    unsigned char unmodeled_00;
    unsigned char row_count;
    unsigned char unmodeled_02[0x0A];
    short width;
    short height;
    const char *title;
    unsigned char columns;
    signed char rows;
    unsigned char unmodeled_16[0x06];
    MenuItemListRow *items;
    unsigned char unmodeled_20[0x06];
    unsigned char state;
} MenuItemListWindow;

typedef struct MenuItemListData {
    unsigned char unmodeled_00[0x0C];
    MenuItemListWindow window;
} MenuItemListData;

typedef struct MenuItemListMenuWork {
    unsigned char unmodeled_00[0x32];
    signed char menuCount;
} MenuItemListMenuWork;

extern MenuItemListData *MenuItemList;

extern MenuItemListMenuWork MenuWork;

extern unsigned char MenuKeepSelect[];

extern void MenuSortSet(int list, int type, int order);

extern void MenuSortChange(int list, int change);

extern MenuItemListRow *MenuListMake(int list, int mode);

extern MenuItemListRow *MenuListGet(int list);

extern void WindowSPItemChange(MenuItemListWindow *window);

extern void WindowSPSetSelect(MenuItemListWindow *window, unsigned char *saved_selection);

extern const char D_004C4F50[];
extern int *MenuSortAddrGet(int list);
extern int MenuSortCheck(int list);

void MenuItemListMake00(void)
{
  MenuItemListRow *row;
  int *sort;
  int n;
  int r;
  int i;

  row = MenuListGet(0);
  sort = MenuSortAddrGet(0);
  MenuSortSet(0, 1, 0);
  MenuSortChange(0, 0);
  MenuSortChange(0, ((signed char *)&MenuWork)[0x31]);
  MenuListMake(0, 0);
  n = MenuSortCheck(0);
  for (i = 0; i < n; i++) {
    r = MenuItemUseCheck(sort[i]);
    if (r != 0) {
      if (r > 0) {
        goto zero;
      } else if (r == -1) {
        row->flag = 1;
        ((unsigned char *)row)[9] = 2;
      } else {
        goto zero;
      }
    } else {
      row->flag = 1;
      ((unsigned char *)row)[9] = 1;
    }
    goto next;
  zero:
    row->flag = 0;
    ((unsigned char *)row)[9] = 0;
  next:
    row++;
  }
  MenuItemList->window.title = D_004C4F50;
  MenuItemList->window.row_count = 7;
  MenuItemList->window.columns = 1;
  MenuItemList->window.rows = 0xB;
  MenuItemList->window.width = 0xE0;
  MenuItemList->window.height = (MenuItemList->window.rows * 0x18) + 6;
  MenuItemList->window.items = MenuListGet(0);
  WindowSPItemChange(&MenuItemList->window);
  WindowSPSetSelect(&MenuItemList->window, MenuKeepSelect);
}

void MenuItemListMake00_1(void) {
    MenuItemListRow *row;
    int *sort;
    int n;
    int r;
    int i;

    row = MenuListGet(0);
    sort = MenuSortAddrGet(0);
    MenuListMake(0, 0);
    n = MenuSortCheck(0);
    for (i = 0; i < n; i++) {
        r = MenuItemUseCheck((short)sort[i]);
        switch (r) {
        case -1:
            row->flag = 1;
            ((unsigned char *)row)[9] = 2;
            break;
        case 0:
            row->flag = 1;
            ((unsigned char *)row)[9] = 1;
            break;
        case 1:
        case 2:
            row->flag = 0;
            ((unsigned char *)row)[9] = 0;
            break;
        }
        row++;
    }
    MenuItemList->window.title = D_004C4F50;
    MenuItemList->window.row_count = 7;
    MenuItemList->window.columns = 1;
    MenuItemList->window.rows = 0xB;
    MenuItemList->window.width = 0xE0;
    MenuItemList->window.height = (MenuItemList->window.rows * 0x18) + 6;
    MenuItemList->window.items = MenuListGet(0);
    WindowSPItemChange(&MenuItemList->window);
    WindowSPSetSelect(&MenuItemList->window, MenuKeepSelect);
}

extern const char D_004C4F60[];

void MenuItemListMake01(unsigned int type)
{
  switch (type) {
  case 0:
    MenuSortSet(0, 4, -1);
    break;
  case 1:
    MenuSortSet(0, 0x10, -1);
    break;
  case 2:
    MenuSortSet(0, 8, -1);
    break;
  case 3:
    MenuSortSet(0, 4, -2);
    break;
  case 4:
    MenuSortSet(0, 0x10, -2);
    break;
  case 5:
    MenuSortSet(0, 8, -2);
    break;
  }
  MenuSortChange(0, 0);
  MenuSortChange(0, ((signed char *)&MenuWork)[0x33 + type]);
  MenuListMake(0, 0);
  MenuItemList->window.title = D_004C4F60;
  MenuItemList->window.row_count = 7;
  MenuItemList->window.columns = 2;
  MenuItemList->window.rows = 0xB;
  MenuItemList->window.width = 0x1D4;
  MenuItemList->window.height = (MenuItemList->window.rows * 0x18) + 6;
  MenuItemList->window.items = MenuListGet(0);
  WindowSPItemChange(&MenuItemList->window);
  WindowSPSetSelect(&MenuItemList->window, &MenuKeepSelect[type * 5 + 10]);
}

void MenuItemListMake02(void)
{
  MenuSortSet(0, 2, 0);
  MenuSortChange(0, 0);
  MenuSortChange(0, MenuWork.menuCount);
  MenuListMake(0, 0);
  MenuItemList->window.title = &D_004C4F60;
  MenuItemList->window.row_count = 7;
  MenuItemList->window.columns = 2;
  MenuItemList->window.rows = 0xB;
  MenuItemList->window.width = 0x1D4;
  MenuItemList->window.height = (MenuItemList->window.rows * 0x18) + 6;
  MenuItemList->window.items = MenuListGet(0);
  WindowSPItemChange(&MenuItemList->window);
  WindowSPSetSelect(&MenuItemList->window, &MenuKeepSelect[5]);
}

extern char msg00_1_0036D698[];
extern unsigned short PadData[];
void WindowSPSet(void *);
void WindowSPMain(void *);
#define MIL_S(p, o) (*(short *)((char *)(p) + (o)))
static inline void MenuItemListSlide(MenuItemListData *p, short *slide)
{
    short *x = &MIL_S(p, 0x10);
    int k = 0x210;
    MoveSlide(x, slide, 3.0f);
    if (*x - k) {
        WindowSPMain(&p->window);
    }
}

void MenuItemListMain(void)
{
  MenuItemListData *d;
  MenuItemListData *p;
  MenuItemListData *w;
  short slide;
  d = MenuItemList;
  p = d;
  switch (p->unmodeled_00[0])
  {
    case 0:
      *((short *) (((char *) p) + 0x10)) = 0x210;
      *((int *) (((char *) p) + 0x8)) = 0xFFFFF0;
      *((int *) (0x14 + ((char *) p))) = 0xFFFFF0;
      p->window.title = msg00_1_0036D698;
      *((short *) (((char *) p) + 0x12)) = 0x40;
      p->window.row_count = 7;
      p->window.rows = 0xB;
      p->window.height = 0x10E;
      p->window.items = 0;
      WindowSPSet(&p->window);
      p->unmodeled_00[1] = 1;
      *((int *) (((char *) p) + 0x4)) = -1;
      p->unmodeled_00[0] = 2;
      p->unmodeled_00[3] = 0;

    case 2:
      if (MenuWork.unmodeled_00[3] == 0xF0)
      {
        p->unmodeled_00[3] = 99;
      }
      switch (p->unmodeled_00[3])
      {
        case 0:
          p->unmodeled_00[3] = 1;

        case 1:
          *((short *) (((char *) p) - -0x10)) = 0x210;
          switch ((signed char) MenuWork.unmodeled_00[0x30])
          {
            case 0:
              MenuItemListMake00();
              break;

            case 1:
              MenuItemListMake02();
              break;

            default:
              MenuItemListMake01(((signed char) MenuWork.unmodeled_00[0x30]) - 2);
              break;
          }
          p->window.unmodeled_00 = 0x11;
          WindowSPMain(&p->window);
          p->unmodeled_00[3] = 2;

        case 2:
          if ((*((int *) (((char *) p) + 0x4))) != ((signed char) MenuWork.unmodeled_00[0x30]))
          {
            *((int *) (((char *) p) + 0x4)) = (signed char) MenuWork.unmodeled_00[0x30];
            p->unmodeled_00[3] = 1;
          }
          if (MenuWork.unmodeled_00[0x30] == 0)
          {
            slide = 0x110;
          }
          else if (MenuWork.unmodeled_00[3] == 0x30)
          {
            slide = 0x210;
          }
          else
          {
            slide = 0x18;
          }
          switch (MenuWork.unmodeled_00[3])
          {
            case 0x22:
              p->unmodeled_00[3] = 3;
              break;

            case 0x80:
              p->unmodeled_00[3] = 4;
              break;
          }
          break;

        case 3:
          if (MenuWork.unmodeled_00[3] != 0x22)
          {
            p->unmodeled_00[3] = 2;
          }
          slide = 0x110;
          break;

        case 4:
          if (PadData[0x15] & 0x20)
          {
            switch ((signed char) MenuWork.unmodeled_00[0x30])
            {
              case 0:
                MenuItemListMake00();
                break;

              case 1:
                MenuItemListMake02();
                break;

              default:
                MenuItemListMake01(((signed char) MenuWork.unmodeled_00[0x30]) - 2);
                break;
            }
          }
          if (MenuWork.unmodeled_00[3] != 0x80)
          {
            p->unmodeled_00[3] = 2;
          }
          slide = 0x110;
          break;

        case 99:
          slide = 0x210;
          break;
      }
      w = (MenuItemListData *)((char *)d + ((unsigned int)MenuWork.unmodeled_00[0x31] >> 8));
      MenuItemListSlide(w, &slide);
      break;
  }
}

extern void *MenuItemSelect;
extern void *MenuItemSegment;
typedef struct MenuItemSelectContent {
    u8 unmodeled_00[2];
    unsigned short mode;
    int selection;
    const char *text;
    u8 unmodeled_0c[0xBF0 - 0x0C];
} MenuItemSelectContent;
typedef struct MenuItemSelectWindow MenuItemSelectWindow;
struct MenuItemSelectWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    const char *title;
    signed char state;
    u8 ribbonInitialized;
    u8 unmodeled_12[2];
    void (*callback)(MenuItemSelectWindow *, struct MenuItemSelectContent *);
    struct MenuItemSelectContent *callbackArg;
    u8 unmodeled_1c[0x194 - 0x1C];
};
typedef struct MenuItemSelectSprite {
    u8 state;
    u8 unmodeled_01[3];
    short x;
    short y;
    int color;
} MenuItemSelectSprite;
typedef struct MenuItemSelectWork {
    u8 state;
    u8 unmodeled_01;
    u8 unmodeled_02;
    u8 unmodeled_03;
    int color;
    MenuItemSelectWindow window;
    u8 unmodeled_19c[0x330 - 0x19C];
    MenuItemSelectContent content;
    MenuItemSelectSprite sprite;
} MenuItemSelectWork;
typedef struct MenuSortSelectContent {
    u8 unmodeled_00[2];
    unsigned short mode;
    int selection;
    const char *text;
    u8 unmodeled_0c[0xBF0 - 0x0C];
} MenuSortSelectContent;
typedef struct MenuSortSelectWindow MenuSortSelectWindow;
struct MenuSortSelectWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    const char *title;
    signed char state;
    u8 ribbonInitialized;
    u8 unmodeled_12[2];
    void (*callback)(MenuSortSelectWindow *, MenuSortSelectContent *);
    MenuSortSelectContent *callbackArg;
    u8 unmodeled_1c[0x194 - 0x1C];
};
typedef struct MenuSortSelectSprite {
    u8 state;
    u8 unmodeled_01[3];
    short x;
    short y;
    int color;
} MenuSortSelectSprite;
typedef struct MenuSortSelectWork {
    u8 state;
    u8 unmodeled_01;
    u8 unmodeled_02;
    u8 unmodeled_03;
    int color;
    MenuSortSelectWindow window[1];
    u8 unmodeled_19c[0x330 - 0x19C];
    MenuSortSelectContent content;
    MenuSortSelectSprite sprite;
} MenuSortSelectWork;
typedef struct MenuSortMenuWork {
    u8 unmodeled_00[3];
    u8 mode;
    u8 unmodeled_04[0x30 - 0x04];
    signed char listTab;        /* +0x30 */
    signed char sortChoices[8]; /* +0x31 */
    u8 unmodeled_39[0x72 - 0x39];
    signed char sortSelection;  /* +0x72 */
} MenuSortMenuWork;
extern const char *msg10_3[];
extern const char *msg00_2_0036D6A8[];
extern int MenuSelectWindow();
extern void eSpriteSet(void *sprite, short spriteId);
extern void eSpriteMain(void *sprite);

void MenuItemSelectMain(void)
{
    MenuSortSelectWork *select = MenuItemSelect;
    struct {
        short x;
        short y;
        short mode;
    } slideTarget;
    short *targetY;
    unsigned char sortMode;
    MenuSortMenuWork *menu;
    int n;
    int m;
    short windowWidth;
    short windowHeight;

    switch (select->state) {
    case 0:
        select->color = 0xFFFFF0;
        windowWidth = 176;
        windowHeight = 102;
        for (n = 0, m = 0; n < 1; n++, m++) {
        int *color = &select->color;
        const char **titleTable = msg10_3;
        const char **sortTextTable = msg00_2_0036D6A8;
        short *size = &select->window[n].width;
        int spriteColor;
        int windowState;

        WindowDXSet((WindowDX *)&select->window[n]);
        select->window[n].title = titleTable[0];
        windowState = 3;
        select->window[m].callbackArg = &select->content;
        select->window[n].color = *color;
        select->window[n].callback = (void (*)(MenuSortSelectWindow *, MenuSortSelectContent *))MenuSelectWindow;
        size[0] = windowWidth;
        select->content.text = sortTextTable[n];
        select->window[0].height = windowHeight;
        select->content.mode = 0;
        select->content.selection = 0;
        select->window[n].x = -208;
        select->window[n].y = 160;
        select->window[n].state = 1;
        WindowDXMain((WindowDX *)&select->window[n]);
        select->window[n].state = windowState;
        eSpriteSet(&select->sprite, 0x60A);
        spriteColor = *color;
        select->unmodeled_02 = 0;
        select->state = n + 2;
        select->sprite.color = spriteColor + 2;
    }
        /* fall through */
    case 2:
        break;
    default:
        return;
    }

    menu = (MenuSortMenuWork *)&MenuWork;
    targetY = &slideTarget.y;
    slideTarget.x = -208;
    *targetY = 160;
    slideTarget.mode = 0;
    if (menu->mode == 0x80) {
        slideTarget.x = 80;
        slideTarget.mode = 1;
        select->content.selection = menu->sortSelection;
    }
    {
        short *currentX = &select->window[0].x;
        signed char tab;

        MoveSlide(currentX, &slideTarget.x, 3.0f);
        MoveSlide(&select->window[0].y, targetY, 3.0f);
        WindowDXMain((WindowDX *)&select->window[0]);
        select->sprite.x = (unsigned short)*currentX + 144;

        /* Eight tabs share the ability, normal and six category sort choices. */
        tab = menu->listTab;
        sortMode = menu->sortChoices[tab];
        select->sprite.y = (unsigned short)select->window[0].y + ((signed char)sortMode / 2) * 24 + 2;
        eSpriteMain(&select->sprite);
    }
}

typedef struct MenuItemWorkData_P3 {
    u8 unmodeled_00[3];
    u8 mode;
    u8 unmodeled_04[0x20 - 0x04];
    u8 flags;
    u8 nextMode;
    u8 busy;
    u8 partyCount;
    signed char selectionMode;
    u8 unmodeled_25[0x2E - 0x25];
    short selectedItem;
    signed char listTab;
    signed char sortChoices[8]; /* +0x31 */
    u8 unmodeled_39[0x40 - 0x39];
    signed char tabChangeDirection;
    signed char scrollDir;
    u8 unmodeled_42[0x50 - 0x42];
    int selectDir;
    u8 unmodeled_54[0x62 - 0x54];
    signed char agwsMode;
    signed char selectedAgws;
    signed char agwsCount;
    u8 unmodeled_65[0x70 - 0x65];
    signed char selectedParty;
    u8 unmodeled_71; /* +0x65 */
    signed char sortSelection; /* +0x72 */
    u8 unmodeled_73;
    u8 segmentSelection; /* +0x74 */
} MenuItemWorkData_P3;
extern void *MenuItemIcon;
typedef struct MenuItemSpriteIds {
    short ids[10];
} MenuItemSpriteIds;
extern const MenuItemSpriteIds D_004C4FC0;
typedef struct MenuItemIconSprite {
    u8 state;
    u8 unmodeled_01[3];
    short x;
    short y;
    int style;
    signed char red;
    signed char green;
    signed char blue;
    unsigned char alpha;
    u8 unmodeled_10[0x28 - 0x10];
} MenuItemIconSprite;
typedef struct MenuItemIconWork {
    u8 state;
    u8 unmodeled_01;
    u8 slide;
    u8 pulsePhase;
    u8 pulse;
    u8 pulseDirection;
    signed char displacement[10];
    int style;
    MenuItemIconSprite sprites[10];
} MenuItemIconWork;
extern void eSpriteSet(void *sprite, short spriteId);
extern void eSpriteMain(void *sprite);
extern float xglCos(float angle);
extern const float D_004D7E08;
extern const float D_004D7E0C;

void MenuItemIconMain(void)
{
    extern MenuItemWorkData_P3 MenuWork_P3 __asm__("MenuWork");
    MenuItemIconWork *icon = MenuItemIcon;
    signed char *tabBounce;
    signed char *tabBounceIterator;
    int i;

    if (MenuWork_P3.mode == 0) {
        return;
    }
    switch (icon->state) {
    case 0: {
        MenuItemSpriteIds spriteIds = D_004C4FC0;
        signed char *bounce;

        icon->style = 0xFFFFF0;
        for (i = 0; i < 10; i++) {
            eSpriteSet(&icon->sprites[i], spriteIds.ids[i]);
            icon->sprites[i].style = icon->style;
        }
        for (i = 0; i < 2; i++) {
            icon->sprites[i].red = icon->sprites[i].green = icon->sprites[i].blue = -128;
        }
        for (; i < 10; i++) {
            icon->sprites[i].red = icon->sprites[i].green = icon->sprites[i].blue = -128;
        }
        icon->pulseDirection = 1;
        tabBounce = &icon->displacement[2];
        icon->slide = 16;
        icon->pulse = 0;
        icon->pulsePhase = 0;
        icon->displacement[0] = 0;
        icon->displacement[1] = 0;
        bounce = tabBounce + 7;
        for (i = 7; i >= 0; i--) {
            *bounce-- = 0;
        }
        icon->state = 2;
        break;
    }
    case 2:
        tabBounce = &icon->displacement[2];
        break;
    default:
        return;
    }

    switch (MenuWork_P3.mode) {
    case 240:
        icon->pulseDirection = 0;
        /* fall through */
    case 255:
        if (icon->slide != 15) {
            icon->slide++;
        }
        break;
    default:
        if (icon->slide != 0) {
            icon->slide--;
        }
        break;
    }
    if (MenuWork_P3.mode == 32 && MenuWork_P3.tabChangeDirection != 0) {
        if (MenuWork_P3.tabChangeDirection < 0) {
            icon->displacement[0] = -6;
        } else {
            icon->displacement[1] = 6;
        }
        MenuWork_P3.tabChangeDirection = 0;
    }
    if (icon->displacement[0] != 0) {
        icon->displacement[0]++;
    }
    if (icon->displacement[1] != 0) {
        icon->displacement[1]--;
    }
    if (icon->pulsePhase & 1) {
        if (icon->pulse >= 11) {
            icon->pulseDirection = -icon->pulseDirection;
        }
        icon->pulse += icon->pulseDirection;
    }
    icon->pulsePhase++;

    for (i = 0; i < 2; i++) {
        float cosine;
        int slideOffset;

        icon->sprites[i].x = 264 + i * 208 + icon->displacement[i];
        cosine = xglCos(i * D_004D7E08);
        slideOffset = -2 * icon->slide;
        icon->sprites[i].y = 13;
        icon->sprites[i].x += cosine * slideOffset;
        icon->sprites[i].alpha = 128.0f - (icon->slide / 15.0f) * 128.0f;
        if (MenuWork_P3.mode == 48) {
            icon->sprites[i].red = icon->sprites[i].green = icon->sprites[i].blue = 64;
        } else {
            icon->sprites[i].red = icon->sprites[i].green = icon->sprites[i].blue = -128;
        }
    }
    tabBounceIterator = tabBounce;
    for (i = 2; i < 10; i++) {
        int tab = i - 2;
        float cosine;
        int slideOffset;

        icon->sprites[i].x = 296 + tab * 22;
        cosine = xglCos(tab * D_004D7E0C);
        slideOffset = -2 * icon->slide;
        icon->sprites[i].y = 12;
        icon->sprites[i].x += cosine * slideOffset;
        icon->sprites[i].y += *tabBounceIterator;
        if (icon->slide == 0 && MenuWork_P3.listTab == tab) {
            if (*tabBounceIterator != 6) {
                (*tabBounceIterator)++;
            }
        } else if (*tabBounceIterator != 0) {
            (*tabBounceIterator)--;
        }
        icon->sprites[i].alpha = 128.0f - (icon->slide / 15.0f) * 128.0f;
        tabBounceIterator++;
    }
    for (i = 0; i < 10; i++) {
        eSpriteMain(&icon->sprites[i]);
    }
}

typedef struct MenuItemExWork {
    unsigned char state;
    unsigned char pad1[3];
    int color;
    unsigned char message[1];
    unsigned char mode;
    unsigned char pad2[2];
    short x;
    short y;
    int color2;
} MenuItemExWork;
extern MenuItemExWork *MenuItemEx;
extern char *msg00_4_0036D6B8[];
extern void eMessageSet(void *message, const char *text);
extern void eMessageMain(void *message);
extern void MoveSlide(short *value, short *destination, float speed);

void MenuItemExMain(void)
{
  MenuItemExWork *ex = MenuItemEx;
  int *col = &ex->color2;
  short target[8];
  unsigned char *p;
  unsigned char c;
  MenuItemExWork **new_var;
  unsigned char *mw;
  short x;
  short y;
  unsigned char m;
  switch (ex->state)
  {
    case 0:
      ex->color = 0xFFFFF8;
      x = 0x220;
      y = 0x159;
      m = 0x20;
      eMessageSet(ex->message, msg00_4_0036D6B8[0]);
      ex->x = x;
      (*(new_var = &ex))->y = y;
      *col = ex->color;
      ex->mode = m;
      ex->state = 2;

    case 2:
      target[0] = 0x220;
      mw = (unsigned char *) (&MenuWork);
      if (mw[3] == 0x20)
    {
      target[0] = 400;
      p = ((unsigned char *) msg00_4_0036D6B8[0]) + 1;
      if ((*((int *) (mw + 0x50))) >= 0)
      {
        p[2] = 0x80;
        p[1] = 0x80;
        p[0] = 0x80;
      }
      else
      {
        p[2] = 0x40;
        p[1] = 0x40;
        p[0] = 0x40;
      }
    }
      MoveSlide(&ex->x, target, 3.0f);
      eMessageMain(ex->message);
      break;

  }

}

extern int xglFlagsGet(int bit_offset, int bit_count);

int subRoboPartsCheck(int arg0)
{
    int var_16;
    int flag;

    var_16 = 0;
    switch (arg0) {
    case 1:
        var_16 = 1;
        if (xglFlagsGet(0xCE5, 1) != 0) {
            var_16 = 0x41;
        }
        break;
    case 2:
        var_16 = 2;
        if (xglFlagsGet(0xCE7, 1) != 0) {
            var_16 = 0x82;
        }
        break;
    case 9:
        var_16 = 4;
        if (xglFlagsGet(0xCE6, 1) != 0) {
            var_16 = 0x10C;
        }
        break;
    case 7:
        var_16 = 8;
        if (xglFlagsGet(0xCE6, 1) != 0) {
            var_16 = 0x10C;
        }
        break;
    case 8:
        var_16 = 0x10;
        if (xglFlagsGet(0xCE8, 1) != 0) {
            var_16 = 0x230;
        }
        break;
    case 3:
        flag = xglFlagsGet(0xCE8, 1);
        {
            int hi = 0x230;
            __asm__("xori %0, %0, 0" : "+r"(flag));
            var_16 = 0x20;
            if (flag) {
                var_16 = hi;
            }
        }
        break;
    }
    return var_16;
}

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemSegmentMain);

typedef struct MenuItemWorkData_MI {
    u8 unmodeled_00[3];
    u8 mode;
    u8 unmodeled_04[0x20 - 0x04];
    u8 flags;
    u8 nextMode;
    u8 busy;
    u8 partyCount;
    signed char selectionMode;
    u8 unmodeled_25[0x2E - 0x25];
    short selectedItem;
    signed char listTab;
    signed char sortChoices[8]; /* +0x31 */
    u8 unmodeled_39[0x40 - 0x39];
    signed char tabChangeDirection;
    signed char scrollDir;
    u8 unmodeled_42[0x50 - 0x42];
    int selectDir;
    u8 unmodeled_54[0x62 - 0x54];
    signed char agwsMode;
    signed char selectedAgws;
    signed char agwsCount;
    u8 unmodeled_65[0x70 - 0x65];
    signed char selectedParty;
    u8 unmodeled_71; /* +0x65 */
    signed char sortSelection; /* +0x72 */
    u8 unmodeled_73;
    u8 segmentSelection; /* +0x74 */
} MenuItemWorkData_MI;
typedef struct MenuItemSeparator_MI {
    short x;
    short y;
    int color;
    char red;
    char green;
    char blue;
    char alpha;
} MenuItemSeparator_MI;
typedef struct MenuItemLineWork_MI {
    unsigned char state;
    unsigned char unmodeled_01[2];
    unsigned char alpha;
    int color;
    MenuItemSeparator_MI separators[6];
} MenuItemLineWork_MI;
typedef struct MenuItemWindowSP {
    u8 state;
    u8 rowCount;
    u8 unmodeled_02[2];
    short x;
    short y;
    int color;
    short width;
    short height;
    const char *title;
    signed char columns;
    signed char rows;
    u8 unmodeled_16[6];
    struct MenuItemListRow *items;
} MenuItemWindowSP;
typedef struct MenuItemListWork {
    u8 state;
    u8 visible;
    u8 unmodeled_02;
    u8 motion;
    int activeTab;
    int color;
    MenuItemWindowSP window;
} MenuItemListWork;
typedef struct MenuItemSegmentNumber {
    unsigned short x;
    unsigned short y;
    int work;
    u8 unmodeled_08[6];
    u8 format;
    u8 digitCount;
    u8 unmodeled_10[4];
    int value;
    u8 unmodeled_18[140 - 24];
} MenuItemSegmentNumber;
typedef struct MenuItemSegmentRow {
    u8 pulse;
    signed char pulseDirection;
    u8 unmodeled_02[2];
    u8 flags;
    u8 unmodeled_05;
    u8 emphasis;
    signed char emphasisDirection;
    MenuItemIconSprite sprites[5];
    MenuItemSegmentNumber number;
} MenuItemSegmentRow;
typedef struct MenuItemSegmentMessage {
    u8 unmodeled_00;
    u8 mode;
    u8 unmodeled_02[2];
    short x;
    short y;
    int work;
    u8 unmodeled_0c[12];
    const char *text;
    u8 unmodeled_1c[68 - 28];
} MenuItemSegmentMessage;
typedef struct MenuItemSegmentPoint {
    short x;
    short y;
    int work;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
} MenuItemSegmentPoint;
typedef struct MenuItemSegmentRibbon {
    short x;
    short y;
    int work;
    short width;
    short height;
    u8 mode;
    u8 progress;
    u8 visible;
    u8 style;
    MenuItemSegmentPoint points[6];
    u8 unmodeled_58[24];
    MenuItemSegmentPoint render[6][4];
} MenuItemSegmentRibbon;
typedef struct MenuItemSegmentOutline {
    u8 pulse;
    signed char pulseDirection;
    u8 unmodeled_02[2];
    MenuItemSegmentPoint points[12];
} MenuItemSegmentOutline;
typedef struct MenuItemSegmentWork {
    u8 state;
    u8 visible;
    short x;
    short y;
    u8 unmodeled_06[2];
    int work;
    int selected;
    MenuItemPasWindow windows[2];
    MenuItemIconSprite labels[2];
    MenuItemSegmentMessage text[2];
    MenuItemSegmentRow rows[18];
    u8 unmodeled_1c88[4];
    unsigned short ownedParts;
    unsigned short selectedParts;
    MenuItemIconSprite mapSprites[19];
    MenuItemSegmentRibbon ribbon;
    MenuItemSegmentOutline outlines[2];
    int texture;
} MenuItemSegmentWork;
extern void xglSoundEffectNormalID(int sound, int channel);
typedef struct MenuItemArena {
    MenuItemPasWork pas;
    u8 unmodeled_0414[0x480 - 0x414];
    MenuItemInfoWork info;
    MenuItemPartyStatusWork status;
    u8 unmodeled_57a4[0x57B0 - 0x57A4];
    MenuItemAgwsStatusWork agws;
    u8 unmodeled_66f4[0x670C - 0x66F4];
    MenuItemListWork list;
    u8 unmodeled_6738[0x7E50 - 0x6738];
    MenuItemIconWork icon;
    u8 unmodeled_7ff4[0x8044 - 0x7FF4];
    MenuItemSelectWork select;
    u8 unmodeled_8f70[0x8FB4 - 0x8F70];
    MenuItemExWork ex;
    u8 unmodeled_8fc8[0x9640 - 0x8FC8];
    MenuItemLineWork_MI line;
    MenuItemSegmentWork segment;
} MenuItemArena;
extern void *MainMenuWorkEnd;
extern void MenuWorkEndCheck(void *end);
typedef struct MenuItemCharacterPoints {
    u8 unmodeled_00[0x0C];
    int points;
    int etherPoints;
    int skillPoints;
} MenuItemCharacterPoints;
extern unsigned char *MenuSaveDataGet(void);
extern int PartyAllPartyGet(int *characters, int mode);
extern int PartyAgwsGet(int *characters);
extern int MenuScenarioNo;
extern int MenuSelectMove(int selection, int count, int wrap);
extern int MenuBoxDec(int itemId);
extern int MenuBoxChk(int itemId);
extern int MenuCharHpCheck(int character);
extern int MenuCharEpCheck(int character);
extern MenuItemCharacterPoints *func_A19210(int character);
extern int func_00A114C0(int character, int itemId);
extern void ChangeTopLevel(int level);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern const char D_004DAB00[];
extern const char D_004DAB08[];
short WindowSPSelect(void *, int);
void WindowSPKeepSelect(void *cursor, unsigned char *keep);
void WindowSPKeepSelectCheck(unsigned char *keep);
void MenuItemSelectMain(void);
void MenuItemSegmentMain(void);

int MenuItem(void)
{
    extern MenuItemWorkData_MI MenuWork_MI __asm__("MenuWork");
    extern MenuItemInfoPad PadData_MI __asm__("PadData");
    MenuItemWorkData_MI *menu;
    MenuItemUseData *item;
    int characters[8];
    int i;
    int memberIndex;

    if (MenuWork_MI.mode == 0) {
        MenuItemArena *arena = MainMenuWorkEnd;
        unsigned char *cursor = MainMenuWorkEnd;
        void *allocation;
        MenuItemPasWork *pas;
        MenuItemInfoWork *info;
        MenuItemPartyStatusWork *status;
        MenuItemAgwsStatusWork *agws;
        MenuItemListWork *list;
        MenuItemIconWork *icon;
        MenuItemSelectWork *select;
        MenuItemExWork *ex;
        MenuItemLineWork_MI *line;
        MenuItemSegmentWork *segment;
        int remaining = 7;
        unsigned char *savedSelection;

        allocation = cursor;
        pas = allocation;
        MenuItemPas = pas;
        cursor += sizeof(arena->pas) + sizeof(arena->unmodeled_0414);
        pas->state = 0;
        allocation = cursor;
        info = allocation;
        MenuItemInfo = info;
        cursor += sizeof(arena->info);
        info->state = 0;
        allocation = cursor;
        status = allocation;
        MenuItemStatus = status;
        cursor += sizeof(arena->status) + sizeof(arena->unmodeled_57a4);
        status->state = 0;
        allocation = cursor;
        agws = allocation;
        MenuItemStatusAgws = agws;
        cursor += sizeof(arena->agws) + sizeof(arena->unmodeled_66f4);
        agws->state = 0;
        allocation = cursor;
        list = allocation;
        MenuItemList = list;
        cursor += sizeof(arena->list) + sizeof(arena->unmodeled_6738);
        list->state = 0;
        allocation = cursor;
        icon = allocation;
        MenuItemIcon = icon;
        cursor += sizeof(arena->icon) + sizeof(arena->unmodeled_7ff4);
        icon->state = 0;
        allocation = cursor;
        select = allocation;
        MenuItemSelect = select;
        cursor += sizeof(arena->select) + sizeof(arena->unmodeled_8f70);
        select->state = 0;
        allocation = cursor;
        ex = allocation;
        MenuItemEx = ex;
        cursor += sizeof(arena->ex) + sizeof(arena->unmodeled_8fc8);
        ex->state = 0;
        allocation = cursor;
        line = allocation;
        MenuItemLine = line;
        cursor += sizeof(arena->line);
        line->state = 0;
        MenuItemSegment = cursor;
        segment = (MenuItemSegmentWork *)cursor;
        cursor += sizeof(arena->segment);
        segment->state = 0;
        MenuWorkEndCheck(segment + 1);

        MenuWork_MI.nextMode = 0x20;
        MenuWork_MI.busy = 12;
        MenuWork_MI.partyCount = 0;
        MenuWork_MI.flags = 0;
        savedSelection = MenuKeepSelect;
        do {
            unsigned char *entry = savedSelection;

            savedSelection += 5;
            remaining--;
            memset(entry, 0, 5U);
        } while (remaining >= 0);
        for (i = 0; i < 8; i++) {
            unsigned char choice = MenuSaveDataGet()[i];

            MenuWork_MI.sortChoices[i] = choice;
        }
        MenuWork_MI.partyCount = PartyAllPartyGet(characters, 0);
        for (memberIndex = 0; memberIndex < MenuWork_MI.partyCount; memberIndex++) {
            MenuItemParty[memberIndex] = (unsigned short)characters[memberIndex];
        }
        if ((unsigned int)MenuScenarioNo - 0x90U < 9U) {
            MenuWork_MI.agwsCount = 3;
            MenuItemAgws[0] = 18;
            MenuItemAgws[1] = 19;
            MenuItemAgws[2] = 20;
        } else {
            int agwsCount = PartyAgwsGet(characters);

            MenuWork_MI.agwsCount = agwsCount;
            for (memberIndex = 0; memberIndex < (signed char)agwsCount; memberIndex++) {
                MenuItemAgws[memberIndex] = (unsigned short)characters[memberIndex];
            }
        }
    }

    if (MenuWork_MI.mode != MenuWork_MI.nextMode) {
        MenuWork_MI.mode = MenuWork_MI.nextMode;
        MenuWork_MI.flags |= 1;
    } else {
        MenuWork_MI.flags &= 0xFE;
    }
    menu = &MenuWork_MI;
    if (MenuWork_MI.busy != 0) {
        MenuWork_MI.busy--;
    }

    switch (MenuWork_MI.mode) {
    case 0x40:
        return menu->selectedItem;

    case 0x20:
        if ((menu->flags & 1) == 0) {
            if (menu->busy == 0) {
                menu->selectDir = WindowSPSelect(&MenuItemList->window, PadData_MI.activity);
                WindowSPKeepSelect(&MenuItemList->window, &MenuKeepSelect[menu->listTab * 5]);
                menu->tabChangeDirection = 0;
                if (PadData_MI.activity == 4) {
                    menu->tabChangeDirection = -1;
                }
                if (PadData_MI.activity == 8) {
                    menu->tabChangeDirection = 1;
                }
                if (menu->tabChangeDirection != 0) {
                    WindowSPKeepSelectCheck(&MenuKeepSelect[menu->listTab * 5]);
                    menu->listTab += menu->tabChangeDirection;
                    if (menu->listTab < 0) {
                        menu->listTab = 7;
                    }
                    if (menu->listTab >= 8) {
                        menu->listTab = 0;
                    }
                    xglSoundEffectNormalID(1, 0);
                    menu->busy = 12;
                }
                if (PadData_MI.activity == 0x10) {
                    if (MenuWork_MI.selectDir >= 0) {
                        xglSoundEffectNormalID(1, 0);
                        MenuWork_MI.nextMode = 0x80;
                    } else {
                        xglSoundEffectNormalID(5, 0);
                    }
                }
                if (MenuWork_MI.selectDir >= 0) {
                    MenuWork_MI.selectedItem = MenuSortGet(0, MenuWork_MI.selectDir);
                    MenuWork_MI.selectionMode = MenuItemUseCheck(MenuWork_MI.selectedItem);
                    if (PadData_MI.pressed == 0x20) {
                        switch (MenuWork_MI.listTab) {
                        case 0:
                            switch (MenuWork_MI.selectionMode) {
                            case 1:
                            case 2:
                                xglSoundEffectNormalID(1, 0);
                                MenuWork_MI.nextMode = 0x22;
                                MenuWork_MI.agwsMode = 0;
                                break;
                            case 10:
                                if (MenuWork_MI.agwsCount != 0) {
                                    xglSoundEffectNormalID(1, 0);
                                    MenuWork_MI.nextMode = 0x22;
                                    MenuWork_MI.agwsMode = 1;
                                    MenuWork_MI.busy = 8;
                                } else {
                                    xglSoundEffectNormalID(5, 0);
                                }
                                break;
                            default:
                                xglSoundEffectNormalID(5, 0);
                                break;
                            }
                            break;
                        case 1:
                            if ((unsigned int)((unsigned short)MenuWork_MI.selectedItem - 11) < 5U) {
                                if (MenuWork_MI.selectedItem == 11) {
                                    MenuWork_MI.nextMode = 0x30;
                                    xglSoundEffectNormalID(1, 0);
                                } else if ((unsigned int)GameLoopState.flags & 0x400000) {
                                    MenuWork_MI.nextMode = 0x40;
                                    xglSoundEffectNormalID(1, 0);
                                } else {
                                    xglSoundEffectNormalID(5, 0);
                                }
                            }
                            break;
                        }
                    }
                }
                if (PadData_MI.pressed & 0x40) {
                    xglSoundEffectNormalID(2, 0);
                    MenuWork_MI.nextMode = 0xF0;
                }
            } else {
                menu->selectDir = WindowSPSelect(&MenuItemList->window, 0);
            }
        }
        break;

    case 0x22:
        if (menu->busy == 0) {
            if (menu->agwsMode != 0) {
                int selection = MenuSelectMove(menu->selectedAgws, menu->agwsCount, 0);
                unsigned short pressed = PadData_MI.pressed;

                menu->selectedAgws = selection;
                if (pressed == 0x20) {
                    int itemKey = MenuSortGet(0, menu->selectDir);
                    short itemId = itemKey;

                    menu->flags |= 4;
                    func_A1A4E8(itemId);
                    if (MenuCharHpCheck(MenuItemAgws[menu->selectedAgws]) != 0) {
                        xglSoundEffectNormalID(5, 0);
                        break;
                    }
                    MenuBoxDec(itemKey);
                    if (MenuBoxChk(itemKey) == 0) {
                        WindowSPKeepSelect(&MenuItemList->window, &MenuKeepSelect[menu->listTab * 5]);
                        MenuItemListMake00();
                        menu->nextMode = pressed;
                    }
                    func_00A114C0(MenuItemAgws[menu->selectedAgws], itemId);
                    xglSoundEffectNormalID(1, 0);
                    MenuItemListMake00_1();
                }
                if (PadData_MI.pressed == 0x40) {
                    MenuItemListMake00();
                    xglSoundEffectNormalID(2, 0);
                    MenuWork_MI.nextMode = 0x20;
                }
            } else if (menu->scrollDir == 0) {
                signed char selectionMode = menu->selectionMode;

                if (selectionMode == 1 && menu->partyCount >= 2) {
                    if (PadData_MI.activity == 0x1000) {
                        menu->scrollDir = -1;
                    }
                    if (PadData_MI.activity == 0x4000) {
                        menu->scrollDir = selectionMode;
                    }
                    if (menu->scrollDir != 0) {
                        if (menu->scrollDir == -1) {
                            if (menu->selectedParty != 0) {
                                menu->selectedParty--;
                                xglSoundEffectNormalID(3, 0);
                            }
                        } else {
                            if (menu->selectedParty != menu->partyCount - 1) {
                                menu->selectedParty++;
                                xglSoundEffectNormalID(3, 0);
                            }
                        }
                    }
                }
                if (PadData_MI.pressed == 0x40) {
                    MenuItemListMake00();
                    xglSoundEffectNormalID(2, 0);
                    MenuWork_MI.nextMode = 0x20;
                }
                if (PadData_MI.pressed == 0x20) {
                    int itemKey = MenuSortGet(0, MenuWork_MI.selectDir);
                    short itemId = itemKey;

                    MenuWork_MI.flags |= 4;
                    item = func_A1A4E8(itemId);
                    if (itemId == MENU_ITEM_SPECIAL_ID) {
                        if (MenuCharHpCheck(-1) != 0 && MenuCharEpCheck(-1) != 0) {
                            xglSoundEffectNormalID(5, 0);
                            break;
                        }
                    } else {
                        unsigned short attributes;
                        MenuItemCharacterPoints *points = func_A19210(MenuItemParty[MenuWork_MI.selectedParty]);

                        attributes = item->attributes;
                        if (attributes & 1) {
                            if (MenuWork_MI.selectionMode == 1) {
                                if (MenuCharHpCheck(MenuItemParty[MenuWork_MI.selectedParty]) != 0) {
                                    xglSoundEffectNormalID(5, 0);
                                    break;
                                }
                            } else {
                                if (MenuCharHpCheck(-1) != 0) {
                                    xglSoundEffectNormalID(5, 0);
                                    break;
                                }
                            }
                        } else if (attributes & 2) {
                            if (MenuWork_MI.selectionMode == 1) {
                                if (MenuCharEpCheck(MenuItemParty[MenuWork_MI.selectedParty]) != 0) {
                                    xglSoundEffectNormalID(5, 0);
                                    break;
                                }
                            } else {
                                if (MenuCharEpCheck(-1) != 0) {
                                    xglSoundEffectNormalID(5, 0);
                                    break;
                                }
                            }
                        } else if (attributes & 0x800) {
                            if (points->points >= 9999) {
                                xglSoundEffectNormalID(5, 0);
                                break;
                            }
                        } else if (attributes & 0x400) {
                            if (points->etherPoints >= 9999) {
                                xglSoundEffectNormalID(5, 0);
                                break;
                            }
                        } else if (attributes & 0x200) {
                            if (points->skillPoints >= 9999) {
                                xglSoundEffectNormalID(5, 0);
                                break;
                            }
                        }
                    }
                    MenuBoxDec(itemKey);
                    if (MenuBoxChk(itemKey) == 0) {
                        WindowSPKeepSelect(&MenuItemList->window, &MenuKeepSelect[MenuWork_MI.listTab * 5]);
                        MenuItemListMake00();
                        MenuWork_MI.nextMode = 0x20;
                    }
                    func_00A114C0(MenuItemParty[MenuWork_MI.selectedParty], itemId);
                    xglSoundEffectNormalID(1, 0);
                    MenuItemListMake00_1();
                }
            }
        }
        break;

    case 0x30:
        if (menu->flags & 1) {
            menu->segmentSelection = 0;
            menu->busy = 12;
        }
        if (menu->busy == 0 && (PadData_MI.pressed & 0x40)) {
            xglSoundEffectNormalID(2, 0);
            menu->nextMode = 0x20;
            menu->busy = 12;
        }
        break;

    case 0x80: {
        if (MenuWork_MI.flags & 1) {
            signed char initialTab;
            unsigned char initialChoice;

            initialTab = MenuWork_MI.listTab;
            initialChoice = menu->sortChoices[initialTab];
            MenuWork_MI.busy = 8;
            MenuWork_MI.sortSelection = (signed char)initialChoice / 2;
        }
        if (MenuWork_MI.busy == 0) {
            int selection = MenuSelectMove(MenuWork_MI.sortSelection, 4, 0);
            int pressed = PadData_MI.pressed;

            MenuWork_MI.sortSelection = selection;
            if (pressed == 0x20) {
                if (MenuWork_MI.sortSelection != 3) {
                    unsigned char *saved;

                    *(menu->sortChoices + menu->listTab) = MenuWork_MI.sortSelection * 2;
                    saved = MenuSaveDataGet();
                    saved[menu->listTab] = menu->sortChoices[menu->listTab];
                    MenuWork_MI.flags |= 4;
                    xglSoundEffectNormalID(15, 0);
                } else {
                    MenuWork_MI.nextMode = pressed;
                    xglSoundEffectNormalID(2, 0);
                }
            }
            if (PadData_MI.pressed == 0x40) {
                MenuWork_MI.nextMode = 0x20;
                MenuWork_MI.busy = 12;
                xglSoundEffectNormalID(2, 0);
            }
        }
        break;
    }

    case 0xF0:
        if (menu->flags & 1) {
            menu->busy = 16;
        }
        if (menu->busy == 0) {
            menu->nextMode = 0xFF;
        }
        break;

    case 0xFF:
        ChangeTopLevel(0);
        break;
    }

    MenuItemPasMain();
    MenuItemInfoMain();
    MenuItemStatusMain();
    MenuItemStatusAgwsMain();
    MenuItemListMain();
    MenuItemIconMain();
    MenuItemSelectMain();
    MenuItemExMain();
    MenuItemLineMain();
    MenuItemSegmentMain();
    xglFontDebugPrintf(0, 0x40, D_004DAB00);
    xglFontDebugPrintf(0, 0x48, D_004DAB08, MenuWork_MI.selectDir);
    xglFontDebugPrintf(0, 0x50, D_004DAB08, MenuWork_MI.selectionMode);
    return 0;
}

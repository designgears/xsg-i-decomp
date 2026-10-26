#include "common.h"
#include "shared.h"
#include "main/xgl_task.h"
#include "menu_shop.h"

/* Types recovered upstream (origin/master); kept for reference. */
typedef struct ShopDataTail {
    unsigned short count;
    unsigned char unmodeled_02[6];
} ShopDataTail;


typedef struct ShopDataCategory {
    unsigned short count;
    unsigned char unmodeled_02[38];
} ShopDataCategory;

typedef struct MenuShopListRow {
    const char *name;
    int amount;
    unsigned char flag;
} MenuShopListRow;

typedef struct MenuShopWindowDX {
    short x;
    short y;
    int color;
    short width;
    short height;
    const char *title;
    unsigned char state;
    unsigned char unmodeled_11[0x183];
} MenuShopWindowDX;

typedef struct MenuShopMessage {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    unsigned char unmodeled_0c[0x0c];
    const char *text;
} MenuShopMessage;

typedef struct MenuShopEx2Work {
    unsigned char unmodeled_00[4];
    int color;
    MenuShopWindowDX window;
    MenuShopMessage message;
} MenuShopEx2Work;

typedef struct MenuShopWeapon {
    unsigned char unmodeled_00[0]; /* +0x0: zero-length alias of the former padding member, kept for saved bodies */
    unsigned short shopParameter1;
    unsigned char unmodeled_02[4];
    unsigned short flags;
    unsigned char unmodeled_08[0x0A];
    unsigned short wagl;
    unsigned char unmodeled_14[6];
    short bullet;
    unsigned short shopParameter2;
} MenuShopWeapon;

extern ShopDataCategory ShopData[];
extern int MenuScenarioNo;

extern int *MenuSortAddrGet(void);
extern int MenuSortGet(int list, int index);
extern MenuShopListRow *MenuListGet(int list);
extern int MenuSortCheck(int list);
extern int MenuBoxMoneyGet(int item, int mode);
extern long dataMoneyBoxChk(void);
extern int MenuBoxChk(int item);
extern int subMenuShopEquipCheck(int item, int category);
extern int MenuShopNoSaleCheck(int item);
extern void MenuSortSet(int list, int type, int order);
extern int dataEvtBoxChk(int event_id);

extern void MoveSlide(short *current, short *target, float rate);

extern const char D_004DB230[];
extern void WindowDXSet(MenuShopWindowDX *window);
extern void WindowDXMain(MenuShopWindowDX *window);
extern void eMessageSet(MenuShopMessage *message, int text);
extern void eMessageMain(MenuShopMessage *message);
extern MenuShopWeapon *func_A1A3D8(short item);
extern const char **func_A2C738(int bullet);

typedef struct MenuShopWindowSP {
    unsigned char unmodeled_00;
    unsigned char row_count;
    unsigned char unmodeled_02[0x0A];
    short width;
    short height;
    const char *title;
    unsigned char columns;
    unsigned char rows;
    unsigned char unmodeled_16[0x06];
    MenuShopListRow *items;
    unsigned char unmodeled_20[0x06];
    unsigned char state;
} MenuShopWindowSP;

extern MenuShopWindowSP *MenuShopWinSP;
extern unsigned char MenuKeepSelect[0x64];
extern const char D_004C7858[];
extern MenuShopListRow *MenuListMake(int list, int mode);
extern void MenuShopSortSet(int list, int type, int order, int option);
extern void WindowSPItemChange(MenuShopWindowSP *window);
extern void WindowSPSetSelect(MenuShopWindowSP *window, unsigned char *saved_selection);
extern int WindowSPSelect(MenuShopWindowSP *window, int input);

/* ---- Types ported from the derwetterfrosch fork (2026-10-19); layouts
 * match the fork's matched code for this TU.  Not yet used by our C. ---- */
typedef struct MenuShopItemData {
    unsigned char unmodeled_00[4];
    unsigned short flags;
} MenuShopItemData;

typedef struct MenuShopAttackData {
    unsigned char unmodeled_00[10];
    unsigned short flags;
} MenuShopAttackData;

/* MenuShopNoSaleCheck reads the halfword at +0x4. */
typedef struct MenuShopAccessoryData {
    unsigned char unmodeled_00[4];
    unsigned short flags;
    unsigned char unmodeled_06[6];
    unsigned char shopParameter2;
    unsigned char shopParameter3;
} MenuShopAccessoryData;

/* Frame selection displays the four contiguous values; engine reads +8. */
typedef struct MenuShopEquipmentValues {
    unsigned char unmodeled_00[8];
    unsigned short value[4];
} MenuShopEquipmentValues;

typedef struct MenuShopEngineValue {
    unsigned char unmodeled_00[8];
    unsigned short value;
} MenuShopEngineValue;

struct MenuShopModelTransform {
    unsigned char unmodeled_00[0x10];
    Vector4 position;
    Vector4 rotation;
    unsigned char unmodeled_30[0x30];
    float fade;
};

struct MenuShopModelActor {
    unsigned char unmodeled_00[0xC0];
    struct MenuShopModelTransform transform;
};

struct MenuShopModelUnit {
    unsigned char unmodeled_00[0x10];
    signed char taskPhase;
    unsigned char active;
    unsigned char unmodeled_12;
    unsigned char state;
    unsigned char unmodeled_14[0x0C];
    struct MenuShopModelActor *actor;
};

struct MenuShopModelCallbackData {
    unsigned char unmodeled_00[0x10];
    unsigned char state;
    unsigned char unmodeled_11[0x0B];
    int modelId;
};

struct MenuShopAgwsListState {
    unsigned char unmodeled_00[0x160];
    unsigned short itemIds[4];
};

/* (fork struct MenuShopListWork omitted: saved bodies define their own MenuShopListWork) */
struct MenuShopSortOption {
    unsigned char type;
    unsigned char unmodeled_01[3];
    int order;
};

struct MenuShopSortOptionTable {
    struct MenuShopSortOption entries[9];
};

typedef struct MenuShopLineContext {
    unsigned char unmodeled_00[0x10];
    unsigned char mode;
} MenuShopLineContext;

typedef struct MenuShopLineRow {
    int text_id;
    unsigned char style[4];
    short position;
    short target;
} MenuShopLineRow;

typedef struct MenuShopLineDisplay {
    unsigned char unmodeled_00[3];
    unsigned char phase;
    int text_id;
    short top_position;
    MenuShopLineRow rows[5];
    int unmodeled_48;
} MenuShopLineDisplay;

struct MenuShopRowCountTable {
    unsigned char values[18];
};

typedef struct MenuShopSortEntry {
    const short *itemIds;
    int highWord;
    int count;
} MenuShopSortEntry;

typedef struct MenuShopSortTable {
    MenuShopSortEntry entries[11];
} MenuShopSortTable;

struct MenuShopEquipScratch {
    int partyIds[15];
    int slotIndex[9];
};

/* 22 bytes: copied as 8+8+4+2 blocks, read as eleven halfword IDs. */
typedef union MenuShopIconIds {
    struct {
        unsigned char first[8];
        unsigned char second[8];
        unsigned char third[4];
        unsigned short last;
    } blocks;
    short ids[11];
} MenuShopIconIds;

typedef struct MenuShopDividerVertex {
    short x;
    short y;
    int depth;
    unsigned char color[4]; /* r, g, b, a */
} MenuShopDividerVertex;

typedef struct MenuShopDividerWork {
    unsigned char unmodeled_00[3];
    unsigned char alpha;
    int depth;
    MenuShopDividerVertex vertices[6];
} MenuShopDividerWork;

/* Views into MenuShopWorkData used by the fork's MenuShopCore. */
typedef struct MenuShopSelection {
    unsigned char mode;
    unsigned char category;
    signed char quantity;
    unsigned char quantityAction;
    unsigned char quantityLimit;
} MenuShopSelection;

typedef struct MenuShopItem {
    int listIndex;
    int itemId;
    int modelId;
} MenuShopItem;

typedef struct MenuShopAgws {
    short unitId;
    short equipmentId;
    signed char equipmentStatus;
    signed char unitCount;
    unsigned char unmodeled_06;
    signed char unitSelection;
    signed char sort_mode_selector;
} MenuShopAgws;

typedef struct MenuShopParty {
    short unitIds[6];
} MenuShopParty;

static inline MenuShopSelection *MenuShopSelectionGet(void)
{
    return (MenuShopSelection *)&MenuShopWork->mode;
}

static inline MenuShopItem *MenuShopItemGet(void)
{
    return (MenuShopItem *)&MenuShopWork->listIndex;
}

static inline MenuShopAgws *MenuShopAgwsGet(void)
{
    return (MenuShopAgws *)&MenuShopWork->unitId;
}

static inline MenuShopParty *MenuShopPartyGet(void)
{
    return (MenuShopParty *)MenuShopWork->unitIds;
}
/* ---- end of fork-ported types ---- */

extern unsigned short D_00534FB8[];
extern int PartyTakeAgwsCheck(int id);

void MenuShopAgwsListChange(void)
{
  unsigned short *d;
  unsigned short *p;
  unsigned short *new_var;
  unsigned short *q;
  unsigned short *r;
  unsigned short *s;
  char *base;
  char *tb;
  int i;
  int j;
  int k;
  unsigned short v;
  base = (char *) ShopData;
  i = 0;
  p = (unsigned short *) (base + 0x168);
  i = 3;
  do
  {
    if (1)
    {
    }
    if (((*p) != 0) && (PartyTakeAgwsCheck(*p) != 0))
    {
      *p = 0;
    }
    i--;
    p++;
  }
  while (i >= 0);
  new_var = D_00534FB8;
  d = (base = new_var);
  tb = ((char *) d) - 8;
  for (i = 0; i < 3; i++)
  {
    if (new_var[i + 0xB0] == 0)
    {
      j = i + 1;
      k = j;
      if (j < 4)
      {
        do
        {
          v = *((unsigned short *) ((tb + (2 * k)) + 0x168));
          d = D_00534FB8;
          if (v != 0)
          {
            d[i + 0xB0] = v;
            *((unsigned short *) ((tb + (2 * k)) + 0x168)) = 0;
            break;
          }
          k++;
        }
        while (k < 4);
      }
    }
  }

}

static void TskObjectSet3(TskObject *task, TskObjectWorker worker, void *data)
{
    task->data = data;
    task->worker = worker;
    task->state = 0;
}

static void tskTskMain3(TskObject *task)
{
    TskObjectWorker worker = task->worker;

    if (MenuShopWork->state == 0xff) {
        xglTaskWaitRemove(&task->base);
        return;
    }

    if (task->state != 0) {
        if (task->state != 2)
            return;
    } else {
        worker(task, task->data);
        task->state = 2;
    }

    worker(task, task->data);
}

void MenuModelUnitBreak(void *unit);
void MenuModelUnitOpen(void *unit, int drawType);

void MenuShopModelDisp(unsigned char *unit)
{
    char *actor = *(char **)(unit + 0x20);
    float *xf;

    if (actor == 0 && *(int *)((char *)MenuShopWork + 0x2C) != 0) {
        MenuModelUnitBreak(unit);
        *(int *)((char *)MenuShopWork + 0x2C) = 0;
    }
    if (unit[0x11] == 0)
        return;
    xf = (float *)(actor + 0xC0);
    if (*(int *)((char *)MenuShopWork + 0x2C) != 0) {
        unit[0x13] = 0x1E;
        *(int *)((char *)MenuShopWork + 0x2C) = 0;
    }
    switch (unit[0x13]) {
    case 0:
        xf[5] = -0.2f;
        MenuModelUnitOpen(unit, 2);
        unit[0x13] = 10;
        break;
    case 10:
        break;
    case 30:
        MenuModelUnitOpen(unit, 1);
        unit[0x13] = 0x1F;
    case 31:
        if (xf[24] == 0.0f)
            unit[0x10] = -1;
        break;
    }
}

extern const long long D_004C7390[2];
extern int MenuModelOut[4];
void MenuModelCreate(int *model, int chrNo);
void MenuModelExtFuncSet(int model, void (*func)(void), int argument);

static void MenuShopModelMain(unsigned char *unit);

INCLUDE_ASM("asm/main/nonmatchings/menu_shop", MenuShopModelMain);

typedef struct MenuShopPasMessageSlot {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    unsigned char unmodeled_0c[0x0c];
    const char *text;
    unsigned char unmodeled_1c[0x28];
} MenuShopPasMessageSlot;
typedef struct MenuShopPasBounds {
    short x1;
    short y1;
    int color;
    short x2;
    short y2;
} MenuShopPasBounds;
typedef struct MenuShopHelpWork {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int depth;
    MenuShopWindowDX window;
    MenuShopPasMessageSlot messages[16];
    MenuShopPasBounds bounds;
} MenuShopHelpWork;
extern const char *msg00_0_0036DAF0[];
extern void endPrintExtFunc(int depth, int mode, void *vertices);

static void MenuShopPas(TskObject *task, void *data)
{
    MenuShopHelpWork *work = data;
    short messageX[16];
    short windowX;
    int i;
    MenuShopWindowDX *window;
    short *target;
    const char **text;

    switch (task->state) {
    case 0:
        work->x = -272;
        work->y = 8;
        work->depth = 0xfffff0;
        window = &work->window;
        WindowDXSet(window);
        window->x = work->x;
        work->window.color = work->depth;
        work->window.width = 272;
        work->window.y = work->y;
        work->window.height = 32;
        work->window.state = 1;
        WindowDXMain(window);
        work->window.state = 3;
        text = msg00_0_0036DAF0;
        for (i = 0; i < 16; i++, text++) {
            eMessageSet((MenuShopMessage *)&work->messages[i], (int)*text);
            work->messages[i].mode = 32;
            work->messages[i].x = 288;
            work->messages[i].color = work->depth + 2;
            work->messages[i].y = work->y + 3;
        }
        break;
    case 2:
        windowX = -16;
        for (i = 0; i < 16; i++)
            messageX[i] = 288;
        switch (MenuShopWork->state) {
        case 0x20:
            messageX[0] = 16;
            break;
        case 0x30:
        case 0x90:
        case 0xa0:
        case 0xa2:
        case 0xa4:
            messageX[MenuShopWork->category + 3] = 16;
            break;
        case 0xc0:
            messageX[12] = 16;
            break;
        case 0xc2:
        case 0xc4:
        case 0xc6:
            messageX[13] = 16;
            break;
        case 0xd0:
        case 0xd2:
        case 0xd4:
        case 0xd6:
            if (MenuShopWork->sort_mode_selector != 0)
                messageX[14] = 16;
            else
                messageX[15] = 16;
            break;
        default:
            windowX = -272;
            break;
        }
        window = &work->window;
        MoveSlide(&window->x, &windowX, 3.0f);
        WindowDXMain(window);
        work->bounds.x1 = window->x + 3;
        work->bounds.y1 = work->window.y + 3;
        work->bounds.x2 = work->window.width - 6;
        work->bounds.y2 = work->window.height - 6;
        work->bounds.color = work->depth;
        endPrintExtFunc(work->depth, 101, &work->bounds);
        target = messageX;
        for (i = 0; i < 16; i++, target++) {
            MoveSlide(&work->messages[i].x, target, 3.0f);
            if (work->messages[i].x < 256)
                eMessageMain((MenuShopMessage *)&work->messages[i]);
        }
        endPrintExtFunc(work->depth, 102, 0);
        break;
    }
}

typedef struct MenuTextEntry {
    const char *name;
    const unsigned char *auxiliary;
    const unsigned char *sortName;
} MenuTextEntry;
MenuTextEntry *MenuTextGet(int index);
extern void MenuInfoWindow(void);
extern void eMessageCpy(char *destination, const char *source);
extern void eMessageCat(const char *source);
typedef struct MsInfoWindowV {
    short x;
    short y;
    int style;
    short width;
    short height;
    const char *tag_id;
    unsigned char state;
    unsigned char ribbon_initialized;
    unsigned char unmodeled_12[2];
    void (*close_callback)(void *window, void *arg);
    void *close_callback_arg;
    unsigned char unmodeled_1c[0x194 - 0x1c];
} MsInfoWindowV;
typedef struct MenuShopInfoPanel {
    short textOffset[2];
    unsigned char unmodeled_04[8];
    const char *text;
} MenuShopInfoPanel;
typedef struct MenuShopInfoWork {
    unsigned char unmodeled_00[2];
    unsigned char flag;
    unsigned char unmodeled_03;
    int unmodeled_04;
    int ticker;
    MsInfoWindowV window;
    MenuShopInfoPanel panel;
    unsigned char unmodeled_1b0[416];
    char message[128];
} MenuShopInfoWork;
typedef struct MenuShopInfoT0 { const char *p[4]; } MenuShopInfoT0;
typedef struct MenuShopInfoT16 { const char *p[4]; } MenuShopInfoT16;
typedef struct MenuShopInfoT32 { const char *p[3]; } MenuShopInfoT32;
typedef struct MenuShopInfoT48 { const char *p[4]; } MenuShopInfoT48;
typedef struct MenuShopInfoT64 { const char *p[3]; } MenuShopInfoT64;
typedef struct MenuShopInfoT80 { const char *p[8]; } MenuShopInfoT80;
typedef struct MenuShopInfoT112 { const char *p[2]; } MenuShopInfoT112;
extern const char *D_004C74B0[4];
extern const char *D_004C7500[4];
extern const char *D_004C7520[3];
extern const char *D_004C7580[4];
extern const char *D_004C75A0[3];
extern const char *D_004C7650[8];
extern const char *D_004DB138[];
extern const char D_004DB140[];
extern const char D_004DB148[];
extern const char D_004DB150[];
void *func_A191C0(int);
extern MenuTextEntry *func_A2C918(unsigned int itemId);
extern MenuTextEntry *func_A2C968(unsigned int itemId);
extern const char *MenuCharNameGet(int unitId);
extern const char *MenuNumberTextGet(long value, int digits, int flag);

static void MenuShopInfo(TskObject *task, MenuShopInfoWork *work)
{
    MenuShopInfoT0 tab0;
    MenuShopInfoT16 tab16;
    MenuShopInfoT32 tab32;
    MenuShopInfoT48 tab48;
    MenuShopInfoT64 tab64;
    MenuShopInfoT80 tab80;
    MenuShopInfoT112 tab112;
    short targetY;
    short targetH;
    MenuShopInfoPanel *panel;
    unsigned char *unit;
    short value;

    tab0 = *(MenuShopInfoT0 *)D_004C74B0;
    tab16 = *(MenuShopInfoT16 *)D_004C7500;
    tab32 = *(MenuShopInfoT32 *)D_004C7520;
    tab48 = *(MenuShopInfoT48 *)D_004C7580;
    tab64 = *(MenuShopInfoT64 *)D_004C75A0;
    tab80 = *(MenuShopInfoT80 *)D_004C7650;
    tab112 = *(MenuShopInfoT112 *)D_004DB138;

    switch (task->state) {
    case 0:
        work->ticker = 0x00fffff0;
        WindowDXSet((MenuShopWindowDX *)(void *)(&work->window));
        work->window.x = -16;
        work->window.y = 480;
        work->window.width = 544;
        work->window.height = 54;
        work->window.style = work->ticker;
        work->window.close_callback = (void (*)(void *, void *))MenuInfoWindow;
        work->window.state = 1;
        panel = &work->panel;
        work->window.close_callback_arg = panel;
        WindowDXMain((MenuShopWindowDX *)(void *)(&work->window));
        work->window.state = 3;
        panel->textOffset[0] = 0;
        work->panel.textOffset[1] = 0;
        work->panel.text = 0;
        work->flag = 0;
        work->unmodeled_04 = 0;
        memset(work->message, 0, 128);
        return;
    case 2:
        break;
    default:
        return;
    }

    targetY = 386;
    targetH = 54;
    switch (MenuShopWork->state) {
    case 32:
        eMessageCpy(work->message, tab0.p[MenuShopWork->mode]);
        break;
    case 48:
    case 144:
        if (MenuShopWork->itemId == 0) {
            eMessageCpy(work->message, D_004DB140);
        } else if ((MenuShopWork->itemId >> 16) == 7) {
            if (MenuShopWork->modelId >= 23)
                eMessageCpy(work->message, tab112.p[0]);
            else
                eMessageCpy(work->message, tab112.p[1]);
        } else {
            eMessageCpy(work->message, (const char *)MenuTextGet(MenuShopWork->itemId)->auxiliary);
        }
        break;
    case 160:
        if (MenuShopWork->itemId != 0) {
            eMessageCpy(work->message, tab16.p[2]);
            eMessageCat(tab16.p[3]);
        }
        break;
    case 162:
        if (MenuShopWork->itemId != 0) {
            long money;

            eMessageCpy(work->message, MenuNumberTextGet(MenuShopWork->quantity, 2, 0));
            eMessageCat(tab32.p[2]);
            eMessageCat(MenuTextGet(MenuShopWork->itemId)->name);
            eMessageCat(tab32.p[0]);
            switch (MenuShopWork->mode) {
            case 0:
                money = MenuBoxMoneyGet(MenuShopWork->itemId, 0) * MenuShopWork->quantity;
                break;
            default:
                money = MenuBoxMoneyGet(MenuShopWork->itemId, 1) * MenuShopWork->quantity;
                break;
            }
            eMessageCat(MenuNumberTextGet(money, 10, 0));
            eMessageCat(tab32.p[1]);
        }
        break;
    case 164:
        if (MenuShopWork->itemId != 0) {
            eMessageCpy(work->message, MenuNumberTextGet(MenuShopWork->quantity, 2, 0));
            eMessageCat(tab64.p[0]);
            eMessageCat(MenuTextGet(MenuShopWork->itemId)->name);
            eMessageCat(D_004DB140);
            eMessageCat(tab64.p[MenuShopWork->mode + 1]);
        }
        break;
    case 192:
        eMessageCpy(work->message, tab48.p[MenuShopWork->sortOption]);
        break;
    case 194:
    case 196:
        eMessageCpy(work->message, MenuCharNameGet(MenuShopWork->unitId));
        eMessageCat(tab80.p[0]);
        break;
    case 198:
        eMessageCpy(work->message, MenuCharNameGet(MenuShopWork->unitId));
        eMessageCat(tab80.p[7]);
        break;
    case 208:
        if (MenuShopWork->listIndex < 0 || MenuShopWork->equipmentId == 0) {
            eMessageCpy(work->message, D_004DB140);
        } else {
            eMessageCpy(work->message, (const char *)func_A2C968(MenuShopWork->equipmentId)->auxiliary);
        }
        break;
    case 210:
    case 212:
        if (MenuShopWork->listIndex >= 0) {
            unit = func_A191C0(MenuShopWork->unitId);
            eMessageCpy(work->message, MenuCharNameGet(MenuShopWork->unitId));
            eMessageCat(tab80.p[1]);
            if (MenuShopWork->sort_mode_selector != 0) {
                value = *(short *)(unit + 88);
                if (value != 0)
                    eMessageCat(func_A2C968(value)->name);
                eMessageCat(tab80.p[2]);
                eMessageCat(tab80.p[3]);
                if (MenuShopWork->equipmentId != 0)
                    eMessageCat(func_A2C968(MenuShopWork->equipmentId)->name);
            } else {
                value = *(short *)(unit + 86);
                if (value != 0)
                    eMessageCat(func_A2C918(value)->name);
                eMessageCat(tab80.p[2]);
                eMessageCat(tab80.p[3]);
                if (MenuShopWork->equipmentId != 0)
                    eMessageCat(func_A2C918(MenuShopWork->equipmentId)->name);
            }
            eMessageCat(D_004DB148);
        } else {
            eMessageCpy(work->message, D_004DB140);
        }
        break;
    case 214:
        eMessageCpy(work->message, MenuCharNameGet(MenuShopWork->unitId));
        if (MenuShopWork->sort_mode_selector != 0) {
            eMessageCat(tab80.p[4]);
            eMessageCat(tab80.p[6]);
            if (MenuShopWork->equipmentId != 0)
                eMessageCat(func_A2C968(MenuShopWork->equipmentId)->name);
        } else {
            eMessageCat(tab80.p[5]);
            eMessageCat(tab80.p[6]);
            if (MenuShopWork->equipmentId != 0)
                eMessageCat(func_A2C918(MenuShopWork->equipmentId)->name);
        }
        eMessageCat(D_004DB150);
        break;
    default:
        targetY = 480;
        break;
    }
    MoveSlide(&work->window.y, &targetY, 3.0f);
    MoveSlide(&work->window.height, &targetH, 3.0f);
    work->panel.text = work->message;
    WindowDXMain((MenuShopWindowDX *)(void *)(&work->window));
}


static void MenuShopSelect(TskObject *task, void *data)
{
    MenuShopSelectWork *work = data;

    MenuShopSelectCommand *ListMake(int menuIndex)
    {
        MenuShopCommandPane *pane = (MenuShopCommandPane *)(menuIndex * sizeof(MenuShopCommandPane) + (int)work + 8);

        memset(pane->commands, 0, sizeof(pane->commands));
        {
            int commandIndex;

            for (commandIndex = 0; commandIndex < 4; commandIndex++) {
                work->menus[menuIndex].commands[commandIndex].text =
                    msg00_1_0036DB30[menuIndex * 4 + commandIndex];
                work->menus[menuIndex].commands[commandIndex].locked = 0;
            }
            work->menus[menuIndex].commands[commandIndex].text = 0;
        }
        if (menuIndex == 0) {
            if (MenuShopWork->noCategories != 0)
                work->menus[menuIndex].commands[0].locked = 1;
            if (MenuShopWork->equipmentStatus == 0)
                work->menus[menuIndex].commands[2].locked = 1;
        }
        return work->menus[menuIndex].commands;
    }

    int i;

    switch (task->state) {
    case 0:
        work->depth = 0xfffff4;
        for (i = 0; i < 2; i++) {
            WindowDXSet((MenuShopWindowDX *)&work->menus[i].window);
            work->menus[i].window.x = 528;
            work->menus[i].window.style = work->depth;
            if (i == 0)
                work->menus[0].window.width = 205;
            else
                work->menus[i].window.width = 205;
            work->menus[i].window.height = 102;
            work->menus[i].window.close_callback_arg = &work->menus[i].panel;
            work->menus[i].window.close_callback = (void (*)(MenuShopSelectWindow *, void *))MenuSelectWindow;
            work->menus[i].window.tag_id = D_004DB168;
            work->menus[i].panel.cursor = 0;
            work->menus[i].panel.commands = ListMake(i);
            work->menus[i].panel.displayMode = 2;
            work->menus[i].window.state = 1;
            WindowDXMain((MenuShopWindowDX *)&work->menus[i].window);
            work->menus[i].window.state = 3;
        }
        for (i = 0; i < 1; i++) {
            WindowDXSet((MenuShopWindowDX *)&work->choices[i].window);
            work->choices[i].window.x = 528;
            work->choices[i].window.width = 169;
            work->choices[i].window.height = 102;
            work->choices[i].window.style = work->depth;
            work->choices[i].window.tag_id = D_004DB170;
            work->choices[i].panel.choices = msg01_2_0036DB50[3];
            work->choices[i].panel.prompt = msg01_2_0036DB50[0];
            work->choices[i].window.close_callback_arg = &work->choices[i].panel;
            work->choices[i].window.close_callback = (void (*)(MenuShopSelectWindow *, void *))MenuSelectWindow;
            work->choices[i].panel.cursor = 0;
            work->choices[i].panel.displayMode = 1;
            work->choices[i].window.state = 1;
            WindowDXMain((MenuShopWindowDX *)&work->choices[i].window);
            work->choices[i].window.state = 3;
        }
        work->popupFlags = 0;
        break;
    case 2: {
        short menuX[2];
        short choiceX[2];
        MenuShopWorkData *shop;

        for (i = 0; i < 2; i++) {
            unsigned char popupVisible;

            menuX[i] = 528;
            popupVisible = work->popupFlags & 1;
            if (popupVisible)
                choiceX[i] = -176;
            else
                choiceX[i] = 528;
        }
        shop = MenuShopWork;
        switch (shop->state) {
        case 0x20:
            if (shop->stateFlags & 1) {
                work->menus[0].window.x = 528;
                work->menus[0].window.y = 192;
            }
            menuX[0] = 152;
            work->menus[0].panel.cursor = shop->mode;
            break;
        case 0xc0:
            if (shop->stateFlags & 1) {
                work->menus[1].window.x = 528;
                work->menus[1].window.y = 192;
            }
            menuX[1] = 160;
            work->menus[1].panel.cursor = shop->sortOption;
            break;
        case 0xa2: {
            unsigned char *mode = &shop->mode;
            unsigned char updatePopup;

            updatePopup = shop->stateFlags & 1;

            if (updatePopup) {
                switch (*mode) {
                case 0:
                    work->choices[0].panel.prompt = msg01_2_0036DB50[0];
                    switch (mode[1]) {
                    case 0:
                        work->choices[0].window.x = 528;
                        work->choices[0].window.y = 256;
                        work->popupFlags &= ~1;
                        break;
                    case 7:
                        work->choices[0].window.x = -136;
                        work->choices[0].window.y = 208;
                        work->popupFlags |= 1;
                        break;
                    case 8:
                        work->choices[0].window.x = 528;
                        work->choices[0].window.y = 256;
                        work->popupFlags &= ~1;
                        break;
                    default:
                        work->choices[0].window.x = -136;
                        work->choices[0].window.y = 80;
                        work->popupFlags |= 1;
                        break;
                    }
                    break;
                default:
                    work->choices[0].panel.prompt = msg01_2_0036DB50[1];
                    switch (mode[1]) {
                    case 0:
                    case 8:
                        work->choices[0].window.x = 528;
                        work->choices[0].window.y = 256;
                        work->popupFlags &= ~1;
                        break;
                    case 1:
                        work->choices[0].window.x = -136;
                        work->choices[0].window.y = 272;
                        work->popupFlags |= 1;
                        break;
                    case 4:
                        work->choices[0].window.x = -136;
                        work->choices[0].window.y = 272;
                        work->popupFlags |= 1;
                        break;
                    case 7:
                        work->choices[0].window.x = -136;
                        work->choices[0].window.y = 192;
                        work->popupFlags |= 1;
                        break;
                    default:
                        work->choices[0].window.x = -136;
                        work->choices[0].window.y = 80;
                        work->popupFlags |= 1;
                        break;
                    }
                    break;
                }
            }
            switch (MenuShopWork->category) {
            case 0:
            case 8:
                choiceX[0] = 304;
                break;
            case 7:
                choiceX[0] = 48;
                break;
            default:
                choiceX[0] = 32;
                break;
            }
            work->choices[0].panel.cursor = MenuShopWork->quantityAction;
            break;
        }
        case 0xc4:
            if (shop->stateFlags & 1) {
                work->choices[0].panel.prompt = msg01_2_0036DB50[2];
                work->choices[0].window.x = -136;
                work->choices[0].window.y = 192;
                work->popupFlags |= 1;
            }
            choiceX[0] = 88;
            work->choices[0].panel.cursor = MenuShopWork->quantityAction;
            break;
        case 0xd4:
            if (shop->stateFlags & 1) {
                work->choices[0].panel.prompt = msg01_2_0036DB50[0];
                work->choices[0].window.x = 528;
                work->choices[0].window.y = 176;
                work->popupFlags &= ~1;
            }
            choiceX[0] = 304;
            work->choices[0].panel.cursor = MenuShopWork->quantityAction;
            break;
        }
        for (i = 0; i < 2; i++) {
            MoveSlide(&work->menus[i].window.x, &menuX[i], 3.0f);
            WindowDXMain((MenuShopWindowDX *)&work->menus[i].window);
        }
        MoveSlide(&work->choices[0].window.x, &choiceX[0], 3.0f);
        WindowDXMain((MenuShopWindowDX *)&work->choices[0].window);
        break;
    }
    }
}

typedef struct MenuShopSortTab {
    short *list;
    int high;
    int count;
} MenuShopSortTab;
typedef struct MenuShopSortTabs {
    MenuShopSortTab e[11];
} MenuShopSortTabs;
extern const MenuShopSortTabs D_004C7738;

void MenuShopSortSet(int list, int type, int order, int option)
{
  int *new_var;
  MenuShopSortTabs tabs;
  int *sorted = ((int *(*)(int)) MenuSortAddrGet)(list);
  int *hp;
  int *countp;
  short *p;
  int n;
  char *high_base;
  char *count_base;
  tabs = D_004C7738;
  if (type == 0x12)
  {
    type = 9;
  }
  if (type == 0x13)
  {
    type = 0xA;
    if (MenuScenarioNo && MenuScenarioNo)
    {
    }
  }
  if (type == 8)
  {
    if (MenuScenarioNo >= 0x73)
    {
      sorted[0] = 0x20035;
      sorted++;
      sorted[0] = 0x20036;
      sorted++;
    }
    if (MenuScenarioNo >= 0x12D)
    {
      sorted[0] = 0x20037;
      sorted++;
    }
  }
  else
  {
    count_base = (char *) (&tabs.e[0].count);
    ;
    new_var = (int *) ((type * 12) + count_base);
    if ((*new_var) > (n = 0))
    {
      n = 0;
      p = tabs.e[type].list;
      high_base = (char *) (&tabs.e[0].high);
      hp = (int *) (high_base + (type * 12));
      do
      {
        short id = *(p++);
        if (id == 0)
        {
          break;
        }
        n++;
        *(sorted++) = (id & 0xFFFF) + ((*hp) << 16);
      }
      while (n < (*((int *) (count_base + (type * 12)))));
    }
    if (tabs.e)
    {
    }
  }
  sorted[0] = 0;
}

void *func_A191C0(int);
int PartyAgwsGet(int *);
struct MenuShopEquipRecord {
    unsigned char unmodeled_00[0x5e];
    short itemIds[3][3];
};

int subMenuShopEquipCheck(int item, int category)
{
    int partyIds[16];
    int selectedSlot[8];
    short selectedItem = (short)item;
    int partyCount = 0;
    int matches = 0;
    int party;
    int remaining;
    short *equipment;
    struct MenuShopEquipRecord *unit;

    memset(selectedSlot, 0, sizeof(selectedSlot));
    selectedSlot[1] = 1;
    selectedSlot[4] = 1;
    switch (MenuShopWork->category) {
    case 1:
    case 2:
    case 3:
        partyCount = PartyAllPartyGet(partyIds, 0);
        break;
    case 4:
    case 5:
    case 6:
        partyCount = PartyAgwsGet(partyIds);
        break;
    }
    if (partyCount == 0)
        return 0;
    switch (MenuShopWork->category) {
    case 1:
        for (party = 0; party < partyCount; party++) {
            unit = func_A191C0(partyIds[party]);
            if (unit == 0)
                return 0;
            if (unit->itemIds[0][selectedSlot[partyIds[party] - 1]] == selectedItem)
                matches++;
        }
        break;
    case 3:
        for (party = 0; party < partyCount; party++) {
            unit = func_A191C0(partyIds[party]);
            if (unit == 0)
                return 0;
            if (unit->itemIds[2][selectedSlot[partyIds[party] - 1]] == selectedItem)
                matches++;
        }
        break;
    case 2:
        for (party = 0; party < partyCount; party++) {
            unit = func_A191C0(partyIds[party]);
            if (unit == 0)
                return 0;
            for (equipment = unit->itemIds[1], remaining = 2;
                 remaining >= 0; equipment++, remaining--) {
                if (*equipment == selectedItem)
                    matches++;
            }
        }
        break;
    case 4:
        for (party = 0; party < partyCount; party++) {
            unit = func_A191C0(partyIds[party]);
            if (unit == 0)
                return 0;
            for (equipment = unit->itemIds[0], remaining = 2;
                 remaining >= 0; equipment++, remaining--) {
                if (*equipment == selectedItem)
                    matches++;
            }
        }
        break;
    case 6:
        for (party = 0; party < partyCount; party++) {
            unit = func_A191C0(partyIds[party]);
            if (unit == 0)
                return 0;
            for (equipment = unit->itemIds[2], remaining = 2;
                 remaining >= 0; equipment++, remaining--) {
                if (*equipment == selectedItem)
                    matches++;
            }
        }
        break;
    case 5:
        for (party = 0; party < partyCount; party++) {
            unit = func_A191C0(partyIds[party]);
            if (unit == 0)
                return 0;
            for (equipment = unit->itemIds[1], remaining = 2;
                 remaining >= 0; equipment++, remaining--) {
                if (*equipment == selectedItem)
                    matches++;
            }
        }
        break;
    }
    return matches;
}

extern char *func_A1A4E8(int item);
extern char *func_A1A428(int item);
extern char *func_A1A548(int item);

int MenuShopNoSaleCheck(int item)
{
    unsigned int id = item & 0xFFFF;
    int kind = (unsigned int)item >> 16;
    unsigned short flags = 0;

    if (id == 0) {
        return 0;
    }
    switch (kind) {
    case 1:
        flags = *(unsigned short *)(func_A1A4E8(id) + 4);
        break;
    case 3:
        flags = *(unsigned short *)((char *)((MenuShopWeapon *(*)(int))func_A1A3D8)(id) + 6);
        break;
    case 4:
        flags = *(unsigned short *)(func_A1A428(id) + 0xA);
        break;
    case 5:
        flags = *(unsigned short *)(func_A1A548(id) + 4);
        break;
    }
    if (flags & 0x1000) {
        return 1;
    }
    return 0;
}

void MenuShopListColorChange(int list, int kind)
{
    int *sorted = MenuSortAddrGet();
    int count = MenuSortCheck(list);
    MenuShopListRow *entry = MenuListGet(list);
    int i;

    if (MenuShopWork->mode == 0 || kind == 2) {
        for (i = 0; i < count; i++) {
            if (MenuBoxMoneyGet(MenuSortGet(list, i), 0) > dataMoneyBoxChk())
                entry[i].flag = 1;
        }
    } else {
        for (i = 0; i < count; i++) {
            int noSale;

            MenuBoxChk(MenuSortGet(list, i));
            subMenuShopEquipCheck(MenuSortGet(list, i), 0);
            noSale = MenuShopNoSaleCheck(sorted[i]);
            if (noSale == 1)
                entry[i].flag = noSale;
        }
    }
}

typedef struct MenuShopSortPair {
    unsigned char type;
    int order;
} MenuShopSortPair;
extern const char *msg00_4_0036DB60[];

void MenuShopListChange00(void)
{
  MenuShopWorkData **new_var2;
  MenuShopWindowSP **new_var;
  new_var = &MenuShopWinSP;
  new_var2 = &MenuShopWork;
  if (MenuShopWork->mode == 0)
  {
    MenuShopSortSet(0, MenuShopWork->category, 0, 0);
    MenuListMake(0, -10);
    MenuShopListColorChange(0, 0);
  }
  else
  {
    MenuShopSortPair sort[9] = {{1, 0}, {4, -1}, {16, -1}, {8, -1}, {4, -2}, {16, -2}, {8, -2}, {2, -1}, {2, -1}};
    MenuSortSet(0, sort[MenuShopWork->category].type, sort[MenuShopWork->category].order);
    MenuListMake(0, 0);
    MenuShopListColorChange(0, 0);
  }
  {
    unsigned char rows[18] = {11, 9, 9, 9, 6, 9, 9, 4, 3, 11, 7, 9, 9, 6, 9, 9, 4, 3};
    (*new_var)->width = 0x110 - ((*new_var2)->mode << 5);
    MenuShopWinSP->height = (rows[MenuShopWork->category + (MenuShopWork->mode * 9)] * 24) + 6;
    MenuShopWinSP->rows = rows[MenuShopWork->category + (MenuShopWork->mode * 9)];
    MenuShopWinSP->columns = 1;
    MenuShopWinSP->title = msg00_4_0036DB60[MenuShopWork->mode];
    MenuShopWinSP->row_count = 7;
    MenuShopWinSP->items = MenuListGet(0);
    WindowSPItemChange(MenuShopWinSP);
    WindowSPSetSelect(MenuShopWinSP, &MenuKeepSelect[(MenuShopWork->category + (MenuShopWork->mode * 9)) * 5]);
    if (MenuShopWork->mode == 0)
    {
      MenuShopWinSP->state = 6;
    }
    WindowSPSelect(MenuShopWinSP, 0);
  }
}

int MenuShopListChange01(void)
{
    int type = 18;
    MenuShopWindowSP *window;
    short window_width;

    if (MenuShopWork->sort_mode_selector == 0)
        type = 19;

    MenuShopSortSet(0, type, 0, 0);
    MenuListMake(0, -10);
    MenuShopListColorChange(0, 2);

    window = MenuShopWinSP;
    MenuShopWinSP->rows = 9;
    window_width = 272;
    window->height = 222;
    window->width = window_width;
    MenuShopWinSP->columns = 1;
    MenuShopWinSP->title = D_004C7858;
    MenuShopWinSP->row_count = 7;
    MenuShopWinSP->items = MenuListGet(0);
    WindowSPItemChange(MenuShopWinSP);
    WindowSPSetSelect(MenuShopWinSP, &MenuKeepSelect[type * 5]);

    MenuShopWinSP->state = 6;
    return WindowSPSelect(MenuShopWinSP, 0);
}

extern int MenuShopListChange00I(int list) __asm__("MenuShopListChange00");
extern int MenuShopListChange01I(int list) __asm__("MenuShopListChange01");
void WindowSPSet(void *cursor);
void WindowSPMain(void *cursor);
extern const char D_004DB140[];
typedef struct MsWindowSPCursorV {
    unsigned char state;
    unsigned char draw_flags;
    unsigned char anim;
    unsigned char unmodeled_03;
    short x;
    short y;
    int style;
    short width;
    short height;
    const char *title;
    signed char columns;
    signed char rows;
    unsigned char unmodeled_16[2];
    int buttons;
    void *items;
} MsWindowSPCursorV;
struct MenuShopListDisplay {
    unsigned char state;
    unsigned char active;
    unsigned char initialFlags[2];
    unsigned int category;
    int color;
    MsWindowSPCursorV window;
};

static void MenuShopList(TskObject *task, struct MenuShopListDisplay *work)
{
    MenuShopWorkData *selection;
    short closeX;
    short secondTargetX;

    switch (task->state) {
    case 0:
        work->window.x = -0x110;
        work->window.y = 0x50;
        work->color = 0xffff0;
        work->window.style = 0xffff0;
        work->window.draw_flags = 7;
        work->window.title = D_004DB140;
        work->window.rows = 0xb;
        work->window.height = 0x10e;
        work->window.state = 0;
        work->window.items = 0;
        WindowSPSet((void *)(&work->window));
        work->initialFlags[0] = 0;
        work->initialFlags[1] = 0;
        work->state = 0;
        work->active = 0;
        return;
    case 2:
        switch (work->state) {
        case 0:
            work->active = 0;
            work->state = 1;
            /* fall through */
        case 1:
            switch (MenuShopWork->state) {
            case 0x30:
            case 0x90:
                work->state = 0xa;
                break;
            case 0xd0:
                work->state = 0x1e;
                break;
            }
            break;
        case 10:
            work->window.x = -0x120;
            work->window.y = 0x50;
            MenuShopListChange00I(0);
            work->window.state = 0x11;
            work->state = 0xb;
            work->active = 1;
            selection = MenuShopWork;
            work->category = selection->category;
            goto update_open_window;
        case 11:
            selection = MenuShopWork;
        update_open_window:
        {
            short targetX = 0x10;
            if (selection->state == 0xa2) {
                unsigned char category = selection->category;

                if (selection->mode == 0) {
                    if (category != 0 && category != 8 && category != 7)
                        targetX = -0x120;
                } else if (category != 0 && category != 8 && category != 1 && category != 4) {
                    targetX = -0x120;
                }
            }
            MoveSlide(&work->window.x, &targetX, 3.0f);
            selection = MenuShopWork;
            if (selection->state == 0x20) {
                work->state = 0xc;
                selection = MenuShopWork;
            }
            if (work->category != selection->category)
                work->state = 0xa;
            break;
        }
        case 12:
            closeX = -((unsigned short)work->window.width + 0x10);
            MoveSlide(&work->window.x, &closeX, 3.0f);
            if (work->window.x == closeX)
                work->state = 0;
            break;
        case 30:
            work->window.x = -0x120;
            work->window.y = 0x50;
            MenuShopListChange01I(0);
            work->window.state = 0x11;
            work->state = 0x1f;
            work->active = 1;
            /* fall through */
        case 31:
            secondTargetX = 0x10;
            MoveSlide(&work->window.x, &secondTargetX, 3.0f);
            if (MenuShopWork->state == 0xc0)
                work->state = 0xc;
            break;
        }
        if (work->active != 0)
            WindowSPMain((void *)(&work->window));
        break;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/menu_shop", MenuShopEx);

extern const char *msg00_6[3];
extern const char *msg01_7[4];
void eSpriteSet(void *sprite, short spriteId);
void eTagFontSet(void *tag, const char *text);
void eRibbonSet(void *ribbon, int style);
void eRibbonMain(void *ribbon);
void eTagFontMain(void *tag);
void eSpriteMain(void *sprite);
void eNumberSet(void *data, int unused);
void eNumberMain(void *number);

static void MenuShopSeisan(TskObject *arg0, void *arg1) {
    unsigned char *p = (unsigned char *)arg1;
    MenuShopWorkData *w2;
    int i;

    switch (((unsigned char *)arg0)[0x10]) {
    case 0:
        {
            unsigned char *b10;
            unsigned char *b4;
            unsigned char *p14;
        {
            int *msg;
            unsigned char *tag;
            int *dst;
            unsigned char *cur1;
            *(int *)(p + 0x4) = 0xFFFFF0;
            WindowDXSet((MenuShopWindowDX *)(p + 0x8));
            *(short *)(p + 0x8) = 0x210;
            *(int *)(p + 0xC) = *(int *)(p + 0x4);
            *(short *)(p + 0xA) = 0x50;
            *(short *)(p + 0x10) = 0xC0;
            *(short *)(p + 0x12) = 0x98;
            *(int *)(p + 0x14) = (int)msg00_6[0];
            p[0x18] = 1;
            WindowDXMain((MenuShopWindowDX *)(p + 0x8));
            p[0x18] = 3;
            p14 = p + 0x14;
            dst = (int *)(p + 0x1A4);
            tag = p + 0x19C;
            msg = msg01_7;
            i = 3;
            do {
                cur1 = tag;
                tag += 0x20;
                eTagFontSet(cur1, *msg);
                msg += 1;
                i -= 1;
                *dst = *(int *)(p + 0x4) + 2;
                dst = (int *)((unsigned char *)dst + 0x20);
            } while (i >= 0);
        }
        {
            int *dst;
            unsigned char *num;
            {
                unsigned char **pb10 = &b10;
                *pb10 = p + 0x10;
                dst = (int *)(p + 0x220);
            }
            num = p + 0x21C;
            for (i = 5; i >= 0; i -= 1) {
                unsigned char *n = num;
                num += 0x90;
                eNumberSet(n, 0);
                *dst = *(int *)(p + 0x4) + 2;
                dst = (int *)((unsigned char *)dst + 0x90);
            }
        }
        {
            int *dst = (int *)(p14 + 0x570);
            unsigned char *spr;
            i = 0;
            spr = p + 0x57C;
            for (; i < 2; i += 1) {
                unsigned char *cur = spr;
                spr += 0x28;
                eSpriteSet(cur, (short)(i + 0x705));
                *dst = *(int *)(p + 0x4) + 2;
                dst = (int *)((unsigned char *)dst + 0x28);
            }
        }
        for (i = 0; i < 2; i++) {
            int off = i * 0xC + 0x5C0;
            unsigned char *q = p + off;
            q[0x16] = 0xA0;
            q[0x15] = 0xA0;
            q[0x14] = 0xA0;
            q[0x17] = 0x80;
            *(int *)(b10 + off) = *(int *)(p + 0x4) + 2;
        }
        *(int *)(p + 0x5E8) = 0;
        for (i = 0; i < 2; i++) {
            int off = i * 0xC + 0x5F0;
            unsigned char *q = p + off;
            q[0xA] = 0xA0;
            q[0x9] = 0xA0;
            q[0x8] = 0xA0;
            q[0xB] = 0x80;
            b4 = p + 4;
            *(int *)(b4 + off) = *(int *)b4 + 2;
        }
        *(int *)(p + 0x60C) = 0;
        eRibbonSet(p + 0x614, 0);
        {
            int n6;
            for (n6 = 0; n6 < 6; n6++) {
                unsigned char *b = p + n6 * 0xC + 0x620;
                if (n6 < 4) {
                    b[0xC] = 0x20;
                    b[0xD] = 0x34;
                    b[0xE] = 0x34;
                } else {
                    b[0xE] = 0;
                    b[0xD] = 0;
                    b[0xC] = 0;
                }
                if ((unsigned int)(n6 - 2) < 2U) {
                    b[0xF] = 0x20;
                } else {
                    b[0xF] = 0;
                }
            }
        }
        p[0x2] = 0;
        p[0x3] = 0;
        }
        break;
    case 2:
        {
            short sp[8];
            sp[0] = 0x210;
            if (MenuShopWork->state == 0xA0 || MenuShopWork->state == 0xA2) {
                sp[0] = 0x130;
            }
            MoveSlide((short *)(p + 0x8), sp, 3.0f);
            *(int *)(p + 0xA4) = (int)msg00_6[MenuShopWork->mode & 1];
            WindowDXMain((MenuShopWindowDX *)(p + 0x8));
        }
        {
            unsigned char *tg;
            unsigned char *ent;
            i = 0;
            tg = p + 0x19C;
            ent = p + 0x190;
            do {
                int q = i / 2;
                int r = i % 2;
                *(short *)(ent + 0x10) = *(short *)(p + 0x8) + 0x43;
                *(short *)(ent + 0x12) = (short)(*(short *)(p + 0xA) + (r * 0x30 + q * 0x4C) + 4);
                ent += 0x20;
                eTagFontMain(tg);
                tg += 0x20;
                i += 1;
            } while (i < 4);
        }
        {
            unsigned char *ent;
            i = 0;
            do {
                ent = p + 0x570 + i * 0x28;
                if (MenuShopWork->mode == 0) {
                    *(short *)(ent + 0x30) = i + 0x705;
                } else {
                    *(short *)(ent + 0x30) = 0x706 - i;
                }
                *(short *)(ent + 0x10) = (short)(*(short *)(p + 0x8) + 0x39);
                *(short *)(ent + 0x12) = (short)(*(short *)(p + 0xA) + i * 0x4C + 0x1C);
                eSpriteMain(p + 0x57C + i * 0x28);
                i += 1;
            } while (i < 2);
        }
        *(short *)(p + 0x614) = (short)(*(short *)(p + 0x8) + 0x37);
        *(short *)(p + 0x616) = (short)(*(short *)(p + 0xA) + 0x1C);
        *(short *)(p + 0x61C) = 0x2C;
        *(short *)(p + 0x61E) = 0x10;
        *(int *)(p + 0x618) = *(int *)(p + 0x4) + 1;
        for (i = 0; i < 6; i++) {
            eRibbonMain(p + 0x614);
        }
        w2 = MenuShopWork;
        if (w2->listIndex >= 0) {
            int mny;
            unsigned int q2;
            int qty;
            MenuShopWorkData *w;
            p[0x34B] = 2;
            p[0x2BB] = 2;
            p[0x4FB] = 9;
            p[0x46B] = 9;
            p[0x22B] = 2;
            p[0x3DB] = 9;
            w = MenuShopWork;
            q2 = w->unmodeled_12[2];
            *(unsigned int *)(p + 0x230) = q2;
            qty = (signed char)w->unmodeled_12[0];
            *(int *)(p + 0x2C0) = qty;
            if (w->mode == 0) {
                *(unsigned int *)(p + 0x350) = q2 + qty;
                *(int *)(p + 0x3E0) = *(int *)((char *)w + 0x30);
                mny = MenuBoxMoneyGet(MenuSortGet(0, w->listIndex), 0);
                w2 = MenuShopWork;
                *(int *)(p + 0x470) = mny * (signed char)w2->unmodeled_12[0];
                *(int *)(p + 0x500) = *(int *)((char *)w2 + 0x30) - *(int *)(p + 0x470);
            } else {
                *(unsigned int *)(p + 0x350) = q2 - qty;
                *(int *)(p + 0x3E0) = *(int *)((char *)w + 0x30);
                mny = MenuBoxMoneyGet(MenuSortGet(0, w->listIndex), 1);
                w2 = MenuShopWork;
                *(int *)(p + 0x470) = mny * (signed char)w2->unmodeled_12[0];
                *(int *)(p + 0x500) = *(int *)((char *)w2 + 0x30) + *(int *)(p + 0x470);
            }
        }
        {
            int k;
            unsigned char *ent;
            MenuShopWorkData *wj = w2;
            k = 0;
            if (wj->listIndex >= 0) {
                for (;;) {
                    ent = (unsigned char *)arg1 + 0x210 + k * 0x90;
                    *(short *)(ent + 0xC) = *(short *)(p + 0x8) + 0x47;
                    *(short *)(ent + 0xE) = (short)(*(short *)(p + 0xA) + (k % 3 * 0x18 + k / 3 * 0x4C) + 4);
                    ent[0x1A] = 0;
                    eNumberMain(p + 0x21C + k * 0x90);
                    k++;
                    if (k >= 6 || (wj = MenuShopWork)->listIndex < 0) break;
                }
            }
        }
        *(short *)(p + 0x5CC) = (short)(*(short *)(p + 0x8) + 0x1A);
        *(short *)(p + 0x5D8) = (short)(*(short *)(p + 0x8) + 0x5D);
        *(short *)(p + 0x5DA) = (short)(*(short *)(p + 0xA) + 0x30);
        *(short *)(p + 0x5CE) = (short)(*(short *)(p + 0xA) + 0x30);
        *(short *)(p + 0x5F0) = (short)(*(short *)(p + 0x8) + 0x14);
        *(short *)(p + 0x5FC) = (short)(*(short *)(p + 0x8) + 0xAA);
        *(short *)(p + 0x5FE) = (short)(*(short *)(p + 0xA) + 0x7C);
        *(short *)(p + 0x5F2) = (short)(*(short *)(p + 0xA) + 0x7C);
        endPrintExtFunc(*(int *)(p + 0x4), 0, p + 0x5CC);
        endPrintExtFunc(*(int *)(p + 0x4), 0, p + 0x5F0);
        break;
    }
}

extern char D_004DB1A0[];
extern void eTagFontSet(void *tag, const char *text);
extern void eTagFontMain(void *tag);
extern void eNumberSet(void *num, int value);
extern void eNumberMain(void *num);
extern void eSpriteSet(void *sprite, short id);
extern void eSpriteMain(void *sprite);
static inline float MenuShopF3(void) {
    union { int i; float f; } u;
    u.i = 0x40400000;
    return u.f;
}
extern const char *msg01_8_0036DBA0[];

static void MenuShopSeisan02(TskObject *arg0, void *arg1) {
    unsigned char *p = (unsigned char *)arg1;
    short sp0;
    short sp2;
    short sp4;
    short sp6;
    int i;
    int j;

    switch (((unsigned char *)arg0)[0x10]) {
    case 0:
        {
            unsigned char *q;
            int *dst;
            unsigned char *tag;
            MenuShopWindowDX *w = (MenuShopWindowDX *)(p + 0x8);
            *(int *)(p + 0x4) = 0xFFFFF0;
            WindowDXSet(w);
            *(unsigned short *)w = 0x210;
            *(int *)(p + 0xC) = *(int *)(p + 0x4);
            *(unsigned short *)(p + 0xA) = 0x50;
            *(unsigned short *)(p + 0x10) = 0xD0;
            *(int *)(p + 0x14) = (int)D_004DB1A0;
            q = p + 0x14;
            *(unsigned short *)(p + 0x12) = 0x50;
            p[0x18] = 1;
            dst = (int *)(p + 0x1A4);
            tag = p + 0x19C;
            WindowDXMain(w);
            p[0x18] = 3;
            {
                const char **msg = msg01_8_0036DBA0;
                i = 1;
                do {
                    eTagFontSet(tag, *msg);
                    msg++;
                    tag += 0x20;
                    i--;
                    *dst = *(int *)(p + 0x4) + 2;
                    dst += 8;
                } while (i >= 0);
            }
            {
                int *dst = (int *)(p + 0x1E0);
                unsigned char *num = p + 0x1DC;
                i = 2;
                do {
                    eNumberSet(num, 0);
                    num += 0x90;
                    i--;
                    *dst = *(int *)(p + 0x4) + 2;
                    dst += 0x24;
                } while (i >= 0);
            }
            {
                int *st = (int *)(q + 0x380);
                eSpriteSet(p + 0x38C, 0x706);
                *st = *(int *)(p + 0x4) + 2;
            }
        }
        for (i = 0; i < 2; i++) {
            int off = i * 0xC + 0x3A0;
            unsigned char *base;
            unsigned char *e = p + off;
            e[0x1E] = 0xA0;
            e[0x1D] = 0xA0;
            e[0x1C] = 0xA0;
            e[0x1F] = 0x80;
            base = p + 0x18;
            *(int *)(base + off) = *(int *)(p + 0x4) + 2;
        }
        *(int *)(p + 0x3D0) = 0;
        p[0x3] = 0;
        p[0x0] = 0;
        return;
    case 2:
        switch (p[0x0]) {
        case 0:
            p[0x1] = 0;
            p[0x0] = 1;
            /* fallthrough */
        case 1:
            switch (MenuShopWork->state) {
            case 0xC2:
                p[0x0] = 0xA;
                break;
            case 0xD4:
                p[0x0] = 0x14;
                break;
            }
            break;
        case 10:
            *(short *)(p + 0x8) = -0xE0;
            *(short *)(p + 0xA) = 0x50;
            p[0x0] = 0xB;
            p[0x2] = 1;
            p[0x1] = 1;
            /* fallthrough */
        case 11:
            sp0 = 0x30;
            MoveSlide((short *)(p + 0x8), &sp0, MenuShopF3());
            if (MenuShopWork->state == 0xC0) {
                p[0x0] = 0xC;
            }
            break;
        case 12:
            sp2 = -0xE0;
            MoveSlide((short *)(p + 0x8), &sp2, MenuShopF3());
            if (*(short *)(p + 0x8) == sp2) {
                p[0x0] = 0;
            }
            break;
        case 20:
            *(unsigned short *)(p + 0x8) = 0x210;
            *(unsigned short *)(p + 0xA) = 0x40;
            p[0x2] = 2;
            p[0x0] = 0x15;
            p[0x1] = 1;
            /* fallthrough */
        case 21:
            sp4 = 0x120;
            MoveSlide((short *)(p + 0x8), &sp4, MenuShopF3());
            switch (MenuShopWork->state) {
            case 0xC0:
            case 0xD0:
            case 0xD2:
                p[0x0] = 0x16;
                break;
            }
            break;
        case 22:
            sp6 = 0x210;
            MoveSlide((short *)(p + 0x8), &sp6, MenuShopF3());
            if (*(short *)(p + 0x8) == sp6) {
                p[0x0] = 0;
            }
            break;
        }
        if (p[0x1] != 0) {
            WindowDXMain((MenuShopWindowDX *)(p + 0x8));
            i = 0;
            {
                unsigned char *t = p + 0x19C;
                unsigned char *r = p + 0x190;
                do {
                    *(short *)(r + 0x10) = *(unsigned short *)(p + 0x8) + 0x4B;
                    {
                        int o = (i % 2) * 0x30 + (i / 2) * 0x4C;
                        *(short *)(r + 0x12) = *(unsigned short *)(p + 0xA) + o + 7;
                    }
                    eTagFontMain(t);
                    r += 0x20;
                    t += 0x20;
                    i++;
                } while (i < 2);
            }
            {
                unsigned char *sp = p + 0x380;
                *(short *)(sp + 0x10) = *(unsigned short *)(p + 0x8) + 0x41;
                *(short *)(sp + 0x12) = *(unsigned short *)(p + 0xA) + 0x1F;
            }
            i = 1;
            eSpriteMain(p + 0x38C);
            *(int *)(p + 0x1F0) = ((int (*)(void))dataMoneyBoxChk)();
            if (p[0x2] == i) {
                *(int *)(p + 0x280) = 0x64;
            } else {
                int mv;
                if (MenuShopWork->sort_mode_selector != 0) {
                    mv = *(unsigned short *)((unsigned char *)MenuShopWork + 0x42) + 0xB0000;
                } else {
                    mv = *(unsigned short *)((unsigned char *)MenuShopWork + 0x42) + 0xC0000;
                }
                *(int *)(p + 0x280) = MenuBoxMoneyGet(mv, 0);
            }
            *(int *)(p + 0x310) = *(int *)(p + 0x1F0) - *(int *)(p + 0x280);
            {
                int nine = 9;
                unsigned char *n = p + 0x1DC;
                int k = 0;
                unsigned char *e = p + 0x1D0;
                int t;
                i = 2;
                do {
                    e[0x1B] = nine;
                    *(short *)(e + 0xC) = *(unsigned short *)(p + 0x8) + 0x53;
                    t = *(unsigned short *)(p + 0xA) + k + 7;
                    e[0x1A] = 0;
                    *(short *)(e + 0xE) = t;
                    eNumberMain(n);
                    n += 0x90;
                    e += 0x90;
                    k += 0x18;
                    i--;
                } while (i >= 0);
            }
            *(unsigned short *)(p + 0x3B4) = *(unsigned short *)(p + 0x8) + 0x1C;
            *(unsigned short *)(p + 0x3C0) = *(unsigned short *)(p + 0x8) + 0xB5;
            *(unsigned short *)(p + 0x3C2) = *(unsigned short *)(p + 0xA) + 0x33;
            *(unsigned short *)(p + 0x3B6) = *(unsigned short *)(p + 0xA) + 0x33;
            endPrintExtFunc(*(int *)(p + 0x4), 0, p + 0x3B4);
        }
        break;
    }
}

void eRibbonSet(void *ribbon, int style);
void eTagFontSet(void *tag, const char *text);
void eCursolSet(void *cursol, int width);
void eCursolMain(void *cursol);
void eRibbonMain(void *ribbon);
void eTagFontMain(void *tag);
extern void eSpriteSet(void *sprite, short spriteId);
extern void eSpriteMain(void *sprite);
extern unsigned short MenuFaceEpidGet(int unitId, int smallPortrait);
int MenuCharHpCheck(short);
int MenuEngineEquipCheck(int, int);
int MenuFrameEquipCheck(int, int);
extern const char D_004DB1B8[];
typedef struct MsRibbonV {
    short x;
    short y;
    int style;
    short width;
    short height;
    unsigned char unmodeled_0c[0x70 - 0x0c];
} MsRibbonV;
typedef struct MsTagFontV {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int style;
    unsigned char color_r;
    unsigned char color_g;
    unsigned char color_b;
    unsigned char color_a;
    unsigned char unmodeled_10[0x20 - 0x10];
} MsTagFontV;
typedef struct MsCursorV {
    unsigned char mode;
    unsigned char shown;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int style;
    unsigned char unmodeled_0c[0x24 - 0x0c];
} MsCursorV;
struct MenuShopPortrait {
    signed char state;
    unsigned char unmodeled_01[3];
    short x;
    short y;
    int style;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
    unsigned char unmodeled_10[0x14];
    short spriteId;
};
struct MenuShopUnitPanel {
    unsigned char state;
    unsigned char active;
    unsigned char selectionFlags;
    unsigned char flags;
    int depth;
    MsRibbonV ribbon;
    MsTagFontV label;
    struct MenuShopPortrait portraits[7];
    unsigned char unmodeled_1b0[4];
    MsCursorV cursor;
};

static void MenuShopEx02(TskObject *task, struct MenuShopUnitPanel *work)
{
    short openX;
    short closeX;
    int i;

    switch (task->state) {
    case 0:
    {
        short *unitIds;
        work->depth = 0xffff0;
        eRibbonSet((void *)(&work->ribbon), 3);
        work->ribbon.x = 0x210;
        work->ribbon.y = 0x40;
        work->ribbon.style = work->depth;
        work->ribbon.width = 0x100;
        work->ribbon.height = 0x90;
        eTagFontSet((void *)(&work->label), D_004DB1B8);
        work->label.style = work->depth + 2;
        for (i = 0; i < MenuShopWork->unitCount; i++) {
            unitIds = MenuShopWork->unitIds;
            eSpriteSet(&work->portraits[i], MenuFaceEpidGet(unitIds[i], 0));
            work->portraits[i].style = work->depth + 2;
        }
        eCursolSet((void *)(&work->cursor), 0);
        work->cursor.style = work->depth + 4;
        work->cursor.mode = 0;
        work->active = work->flags = work->selectionFlags = 0;
        work->state = 0;
        return;
    }
    case 2:
        switch (work->state) {
        case 0:
            work->active = 0;
            work->state = 1;
            /* fall through */
        case 1:
            switch (MenuShopWork->state) {
            case 0xd0:
            case 0xd2:
                work->state = 20;
                break;
            case 0xc2:
                work->state = 10;
                break;
            }
            break;
        case 10:
        case 20:
            work->ribbon.x = 0x210;
            work->active = 1;
            if (work->state == 10) {
                work->state = 11;
                work->cursor.mode = 0x20;
            } else {
                work->state = 21;
            }
            /* fall through */
        case 11:
        case 21:
        {
            unsigned char shopState;
            openX = 0x130;
            MoveSlide(&work->ribbon.x, &openX, 3.0f);
            shopState = MenuShopWork->state;
            if (shopState == 0xc0) {
                work->state = 12;
            }
            if (MenuShopWork->state == 0xd4) {
                work->state = 12;
            }
            if (work->state == 21) {
                for (i = 0; i < MenuShopWork->unitCount; i++) {
                    int canEquip = 0;
                    short *unitIds = MenuShopWork->unitIds;
                    eSpriteSet(&work->portraits[i], MenuFaceEpidGet(unitIds[i], 0));
                    if (MenuShopWork->sort_mode_selector != 0) {
                        short *currentIds = MenuShopWork->unitIds;
                        if (MenuFrameEquipCheck(currentIds[i], MenuShopWork->equipmentId) != 0)
                            canEquip = 1;
                    } else {
                        short *currentIds = MenuShopWork->unitIds;
                        canEquip = MenuEngineEquipCheck(currentIds[i], MenuShopWork->equipmentId) != 0;
                    }
                    if (canEquip == 0) {
                        work->portraits[i].blue = 0x40;
                        work->portraits[i].green = 0x40;
                        work->portraits[i].red = 0x40;
                    } else {
                        work->portraits[i].blue = -0x80;
                        work->portraits[i].green = -0x80;
                        work->portraits[i].red = -0x80;
                    }
                }
                if (MenuShopWork->state == 0xd2 && work->cursor.mode == 0)
                    work->cursor.mode = 0x20;
                if (MenuShopWork->state == 0xd0)
                    work->cursor.mode = 0;
            } else {
                for (i = 0; i < MenuShopWork->unitCount; i++) {
                    short *unitIds = MenuShopWork->unitIds;
                    eSpriteSet(&work->portraits[i], MenuFaceEpidGet(unitIds[i], 0));
                    unitIds = MenuShopWork->unitIds;
                    if (MenuCharHpCheck(unitIds[i]) != 0) {
                        work->portraits[i].blue = 0x40;
                        work->portraits[i].green = 0x40;
                        work->portraits[i].red = 0x40;
                    } else {
                        work->portraits[i].blue = -0x80;
                        work->portraits[i].green = -0x80;
                        work->portraits[i].red = -0x80;
                    }
                }
            }
            break;
        }
        case 12:
            closeX = 0x210;
            MoveSlide(&work->ribbon.x, &closeX, 3.0f);
            if (work->ribbon.x == closeX)
                work->state = 0;
            work->cursor.mode = 0;
            break;
        }
        if (work->active != 0) {
            eRibbonMain((void *)(&work->ribbon));
            work->label.x = work->ribbon.x + 8;
            work->label.y = (unsigned short)work->ribbon.y + 0x44;
            eTagFontMain((void *)(&work->label));
            for (i = 0; i < MenuShopWork->unitCount; i++) {
                work->portraits[i].x = work->ribbon.x + (i & 1) * 0x40 + 0x40;
                work->portraits[i].y = (unsigned short)work->ribbon.y + (i / 2) * 0x30;
                work->portraits[i].style = work->depth + 2;
                eSpriteMain(&work->portraits[i]);
            }
            work->cursor.x = work->ribbon.x + ((unsigned char)MenuShopWork->unitSelection & 1) * 0x40 + 0x40;
            work->cursor.y = (unsigned short)work->ribbon.y + (MenuShopWork->unitSelection / 2) * 0x30 + 0x10;
            work->cursor.style = work->depth + 4;
            eCursolMain((void *)(&work->cursor));
        }
        break;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/menu_shop", MenuShopIcon);

typedef struct MenuShopUnitData {
    unsigned char unmodeled_00[0x34];
    unsigned short hitPoints;
    unsigned char unmodeled_36[0x20];
    short engineId;
    short frameId;
    unsigned char unmodeled_5a[4];
    short weaponIds[3];
    short accessoryIds[3];
    short attachmentIds[3];
    unsigned char unmodeled_70[0x110];
} MenuShopUnitData;
typedef struct ShopCalculatedPara {
    s16 maxHp;
    s16 maxEp;
    unsigned short attack;
    unsigned short physicalDefense;
    unsigned short displayStat2;
    unsigned short magicDefense;
    unsigned char byteStats[3];
    unsigned char unmodeled_0f[0x25];
    unsigned short currentHp;
    unsigned char unmodeled_36[0x14A];
} ShopCalculatedPara;
typedef struct AgwsParaDisplay {
    short value[7];
} AgwsParaDisplay;
extern AgwsParaDisplay MenuShopPara;
ShopCalculatedPara *func_00A11108(int, int *, int *);

void MenuShopParaSet2(int item, int mode)
{
    int attack[4];
    int defense[4];
    MenuShopUnitData savedUnit;
    ShopCalculatedPara previous;
    MenuShopUnitData *unit;
    ShopCalculatedPara *calculated;
    int i;

    unit = func_A191C0(3);
    savedUnit = *unit;
    for (i = 0; i < 3; i++) {
        unit->weaponIds[i] = unit->accessoryIds[i] = unit->attachmentIds[i] = 0;
    }
    previous = *func_00A11108(3, attack, defense);
    previous.attack += (unsigned short)attack[0];
    previous.physicalDefense += (unsigned short)defense[0];
    previous.magicDefense += (unsigned short)defense[1];

    switch (mode) {
    case 0:
        unit->accessoryIds[0] = item;
        break;
    case 1:
        unit->accessoryIds[0] = item;
        break;
    case 2:
        unit->attachmentIds[0] = item;
        break;
    }

    calculated = func_00A11108(3, attack, defense);
    calculated->attack += (unsigned short)attack[0];
    calculated->physicalDefense += (unsigned short)defense[0];
    calculated->magicDefense += (unsigned short)defense[1];
    MenuShopPara.value[0] = calculated->attack - previous.attack;
    MenuShopPara.value[1] = calculated->physicalDefense - previous.physicalDefense;
    MenuShopPara.value[2] = calculated->displayStat2 - previous.displayStat2;
    MenuShopPara.value[3] = calculated->magicDefense - previous.magicDefense;
    MenuShopPara.value[4] = (signed char)calculated->byteStats[0] - (signed char)previous.byteStats[0];
    MenuShopPara.value[5] = (signed char)calculated->byteStats[1] - (signed char)previous.byteStats[1];
    MenuShopPara.value[6] = (signed char)calculated->byteStats[2] - (signed char)previous.byteStats[2];
    *unit = savedUnit;
}

typedef struct MenuShopWorkDataP1 {
    unsigned char unmodeled_00[0x24];
    int itemId;
    int modelId;
    unsigned char unmodeled_2C[0x14];
    short unitId;
    short equipmentId;
} MenuShopWorkDataP1;
extern MenuShopWorkDataP1 *MenuShopWorkP1 __asm__("MenuShopWork");
typedef struct MenuShopWeaponP1 {
    unsigned short shopParameter1;
    unsigned char unmodeled_02[4];
    unsigned short flags;
    unsigned char unmodeled_08[0x0A];
    unsigned short wagl;
    unsigned char unmodeled_14[6];
    short bullet;
    unsigned short shopParameter2;
} MenuShopWeaponP1;
typedef struct MenuShopAccessoryDataP1 {
    unsigned char unmodeled_00[4];
    unsigned short flags;
    unsigned char unmodeled_06[6];
    unsigned char shopParameter2;
    unsigned char shopParameter3;
} MenuShopAccessoryDataP1;
typedef struct MenuShopEquipmentValuesP1 {
    unsigned char unmodeled_00[8];
    unsigned short value[4];
} MenuShopEquipmentValuesP1;
typedef struct MenuShopEngineValueP1 {
    unsigned char unmodeled_00[8];
    unsigned short value;
} MenuShopEngineValueP1;
extern AgwsParaDisplay MenuShopPara2;
extern AgwsParaDisplay MenuAgwsPara;
extern MenuShopEquipmentValuesP1 *func_A1A598(int item);
extern MenuShopEngineValueP1 *func_A1A5E8(int item);
extern unsigned short *func_A1A428U(int item) __asm__("func_A1A428");
extern MenuShopWeaponP1 *func_A1A3D8I(int item) __asm__("func_A1A3D8");

void MenuShopParaSet(unsigned int mode)
{
    int attack[4];
    int defense[4];
    MenuShopEquipmentValuesP1 *frame;
    MenuShopEngineValueP1 *engine;
    ShopCalculatedPara *calculated;
    MenuShopUnitData *unit;
    MenuShopWorkDataP1 *selection;
    MenuShopWeaponP1 *weapon;
    MenuShopAccessoryDataP1 *accessory;
    short oldEquipment;
    int i;

    memset(&MenuShopPara, 0, sizeof(MenuShopPara));
    memset(&MenuShopPara2, 0, sizeof(MenuShopPara2));
    switch (mode) {
    case 0:
        if (MenuShopWorkP1->equipmentId != 0) {
            frame = func_A1A598(MenuShopWorkP1->equipmentId);
            MenuShopPara.value[0] = frame->value[0];
            MenuShopPara.value[2] = frame->value[1];
            MenuShopPara.value[3] = frame->value[2];
            MenuShopPara.value[4] = frame->value[3];
        }
        break;
    case 1:
        if (MenuShopWorkP1->equipmentId != 0) {
            engine = func_A1A5E8(MenuShopWorkP1->equipmentId);
            MenuShopPara.value[1] = engine->value;
        }
        break;
    case 2:
        if (MenuShopWorkP1->unitId != 0) {
            calculated = func_00A11108(MenuShopWorkP1->unitId, attack, defense);
            MenuAgwsParaSet(MenuShopWorkP1->unitId, 0);
            for (i = 0; i < 6; i++) {
                MenuShopPara.value[i] = MenuAgwsPara.value[i];
                MenuShopPara2.value[i] = MenuAgwsPara.value[i];
            }
            MenuShopPara.value[0] = calculated->currentHp;
        }
        break;
    case 3:
    case 4:
        unit = func_A191C0(MenuShopWorkP1->unitId);
        MenuAgwsParaSet(MenuShopWorkP1->unitId, 10);
        for (i = 0; i < 6; i++)
            MenuShopPara.value[i] = MenuAgwsPara.value[i];
        if (mode == 3) {
            if (MenuFrameEquipCheck(MenuShopWorkP1->unitId,
                                   MenuShopWorkP1->equipmentId)) {
                selection = MenuShopWorkP1;
                oldEquipment = unit->frameId;
                unit->frameId = selection->equipmentId;
            } else {
                for (i = 0; i < 6; i++)
                    MenuShopPara2.value[i] = MenuShopPara.value[i];
                break;
            }
        } else {
            if (MenuEngineEquipCheck(MenuShopWorkP1->unitId,
                                    MenuShopWorkP1->equipmentId)) {
                selection = MenuShopWorkP1;
                oldEquipment = unit->engineId;
                unit->engineId = selection->equipmentId;
            } else {
                for (i = 0; i < 6; i++)
                    MenuShopPara2.value[i] = MenuShopPara.value[i];
                break;
            }
        }
        MenuAgwsParaSet(selection->unitId, 10);
        for (i = 0; i < 6; i++)
            MenuShopPara2.value[i] = MenuAgwsPara.value[i];
        if (mode == 3)
            unit->frameId = oldEquipment;
        else
            unit->engineId = oldEquipment;
        break;
    case 10:
        if ((short)(MenuShopWorkP1->itemId >> 16) != 3)
            return;
        MenuShopParaSet2(MenuShopWorkP1->modelId, 0);
        return;
    case 11:
        if ((short)(MenuShopWorkP1->itemId >> 16) != 5)
            return;
        MenuShopParaSet2(MenuShopWorkP1->modelId, 1);
        return;
    case 12:
        if ((short)(MenuShopWorkP1->itemId >> 16) == 4)
            MenuShopPara.value[0] = *func_A1A428U(MenuShopWorkP1->modelId);
        break;
    case 13:
        if ((short)(MenuShopWorkP1->itemId >> 16) == 3) {
            weapon = func_A1A3D8I(MenuShopWorkP1->modelId);
            MenuShopPara.value[1] = weapon->shopParameter1;
            MenuShopPara.value[2] = weapon->shopParameter2;
            MenuShopPara.value[5] = weapon->wagl;
        }
        break;
    case 14:
        if ((short)(MenuShopWorkP1->itemId >> 16) == 5) {
            accessory = func_A1A548(MenuShopWorkP1->modelId);
            MenuShopPara.value[2] = accessory->shopParameter2;
            MenuShopPara.value[3] = accessory->shopParameter3;
        }
        break;
    case 15:
        if ((short)(MenuShopWorkP1->itemId >> 16) == 4)
            MenuShopPara.value[1] = *func_A1A428U(MenuShopWorkP1->modelId);
        break;
    case 16:
        if ((short)(MenuShopWorkP1->itemId >> 16) == 7) {
            MenuAgwsParaSet(MenuShopWorkP1->modelId, 0);
            for (i = 0; i < 6; i++)
                MenuShopPara.value[i] = MenuAgwsPara.value[i];
        }
        break;
    }
}

typedef union MsValuePair {
    short halfwords[2];
    int scalar;
} MsValuePair;
void eNumberSet(void *data, int unused);
void eNumberMain(void *number);
void MenuShopParaSet(unsigned int mode);
typedef struct MsParaV { short value[7]; } MsParaV;
struct MenuShopParameterLabel {
    unsigned char state;
    unsigned char subState;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int depth;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
    unsigned char unmodeled_10[12];
    const char *text;
};
struct MenuShopParameterNumber {
    unsigned short x;
    unsigned short y;
    int depth;
    unsigned char color[4];
    unsigned char mode;
    unsigned char unmodeled_0d;
    unsigned char format;
    unsigned char digits;
    unsigned char unmodeled_10;
    unsigned char comparisonColor;
    unsigned char alignment;
    unsigned char firstOnly;
    MsValuePair value;
    unsigned char unmodeled_18[0x78];
};
struct MenuShopParameterDisplay {
    unsigned char state;
    unsigned char active;
    unsigned char count;
    unsigned char twoColumns;
    int depth;
    MsInfoWindowV window;
    struct MenuShopParameterLabel labels[9];
    struct MenuShopParameterNumber numbers[9];
};
struct MenuShopParameterDigits8 { unsigned char digits[8]; };
struct MenuShopParameterDigits7 { signed char digits[7]; };
extern const struct MenuShopParameterDigits8 D_004DB220[];
extern const struct MenuShopParameterDigits7 D_004DB228[];
extern const char *msg00_9[];
extern const char *msg01_10[];

static void MenuShopParameter(TskObject *task, void *data)
{
    extern MsParaV MenuShopParaV __asm__("MenuShopPara");
    extern MsParaV MenuShopPara2V __asm__("MenuShopPara2");
    struct MenuShopParameterDisplay *work = data;
    struct MenuShopParameterDigits8 characterDigits = D_004DB220[0];
    struct MenuShopParameterDigits7 agwsDigits = D_004DB228[0];
    short targetX;
    int i;

    switch (task->state) {
    case 0:
        work->depth = 0xffff00;
        WindowDXSet((MenuShopWindowDX *)(void *)(&work->window));
        work->window.x = -0x1f0;
        work->window.style = work->depth;
        work->window.y = 0x149;
        work->window.width = 0x1e0;
        work->window.height = 0x36;
        work->window.state = 1;
        WindowDXMain((MenuShopWindowDX *)(void *)(&work->window));
        work->window.state = 3;
        for (i = 0; i < 9; i++) {
            eTagFontSet((void *)(&work->labels[i]), 0);
            work->labels[i].depth = work->depth + 2;
            work->labels[i].red = 0x70;
        }
        for (i = 0; i < 9; i++) {
            eNumberSet(&work->numbers[i], 0);
            work->numbers[i].format = 5;
            work->numbers[i].depth = work->depth + 2;
        }
        work->count = 0;
        work->twoColumns = 0;
        break;
    case 2:
        targetX = -0x1f0;
        switch (MenuShopWork->state) {
        case 0x30:
        {
            int category = MenuShopWork->category;
            switch (category) {
            case 1:
            case 2:
            case 3:
                    switch (category) {
                    case 1: MenuShopParaSet(10); break;
                    case 2: MenuShopParaSet(11); break;
                    case 3: MenuShopParaSet(12); break;
                    }
                    work->twoColumns = 0;
                    work->count = 7;
                    for (i = 0; i < work->count; i++)
                        work->labels[i].text = msg00_9[i];
                    for (i = 0; i < work->count; i++) {
                        work->numbers[i].format = 5;
                        work->numbers[i].digits = characterDigits.digits[i];
                        work->numbers[i].comparisonColor = 0;
                        work->numbers[i].firstOnly = 1;
                        work->numbers[i].value.scalar = MenuShopParaV.value[i];
                    }
                break;
            case 4:
            case 5:
            case 6:
            case 7:
                    switch (category) {
                    case 4: MenuShopParaSet(13); break;
                    case 5: MenuShopParaSet(14); break;
                    case 6: MenuShopParaSet(15); break;
                    case 7: MenuShopParaSet(16); break;
                    }
                    work->count = 6;
                    work->twoColumns = 1;
                    for (i = 0; i < work->count; i++)
                        work->labels[i].text = msg01_10[i];
                    for (i = 0; i < work->count; i++) {
                        work->numbers[i].format = 5;
                        work->numbers[i].digits = agwsDigits.digits[i];
                        work->numbers[i].comparisonColor = 0;
                        work->numbers[i].firstOnly = 1;
                        work->numbers[i].value.scalar = MenuShopParaV.value[i];
                    }
                break;
            }
        }
            /* fall through */
        case 0xa0:
        case 0xa2:
        case 0xa4:
            if (MenuShopWork->mode == 0 && MenuShopWork->category != 0 && MenuShopWork->category != 8)
                targetX = 0x10;
            break;
        case 0xd0:
        case 0xd2:
        case 0xd4:
        case 0xd6:
            if (MenuShopWork->sort_mode_selector != 0)
                MenuShopParaSet(3);
            else
                MenuShopParaSet(4);
            work->count = 6;
            work->twoColumns = 1;
            for (i = 0; i < work->count; i++)
                work->labels[i].text = msg01_10[i];
            for (i = 0; i < work->count; i++) {
                work->numbers[i].format = 5;
                work->numbers[i].digits = agwsDigits.digits[i];
                work->numbers[i].value.scalar = (unsigned int)MenuShopParaV.value[i] +
                    ((unsigned int)MenuShopPara2V.value[i] << 16);
                work->numbers[i].comparisonColor = 0;
                work->numbers[i].firstOnly = 1;
                if (MenuShopParaV.value[i] < MenuShopPara2V.value[i]) {
                    work->numbers[i].comparisonColor = 2;
                    work->numbers[i].firstOnly = 0;
                } else if (MenuShopPara2V.value[i] < MenuShopParaV.value[i]) {
                    work->numbers[i].comparisonColor = 1;
                    work->numbers[i].firstOnly = 0;
                }
            }
            targetX = 0x10;
            break;
        case 0xc2:
        case 0xc4:
        case 0xc6:
            MenuShopParaSet(2);
            work->count = 6;
            work->twoColumns = 1;
            for (i = 0; i < work->count; i++)
                work->labels[i].text = msg01_10[i];
            for (i = 0; i < work->count; i++) {
                work->numbers[i].format = 5;
                work->numbers[i].comparisonColor = 0;
                work->numbers[i].digits = agwsDigits.digits[i];
                work->numbers[i].firstOnly = 1;
                work->numbers[i].value.scalar = (unsigned int)MenuShopParaV.value[i] +
                    ((unsigned int)MenuShopPara2V.value[i] << 16);
            }
            work->numbers[0].format = 3;
            work->numbers[0].comparisonColor = 0;
            work->numbers[0].firstOnly = 0;
            targetX = 0x10;
            break;
        }
        MoveSlide(&work->window.x, &targetX, 3.0f);
        if (work->twoColumns == 0) {
            work->window.y = 0x139;
            work->window.height = 0x36;
        } else {
            work->window.y = 0x149;
            work->window.height = 0x26;
        }
        WindowDXMain((MenuShopWindowDX *)(void *)(&work->window));
        for (i = 0; i < work->count; i++) {
            if (work->twoColumns == 0) {
                work->labels[i].x = (unsigned short)work->window.x + (i / 3) * 0xa0 + 0x30;
                work->labels[i].y = (unsigned short)work->window.y + (i % 3) * 0x10 + 3;
            } else {
                work->labels[i].x = (unsigned short)work->window.x + (i / 2) * 0xa0 + 0x30;
                work->labels[i].y = (unsigned short)work->window.y + (i % 2) * 0x10 + 3;
            }
            eTagFontMain((void *)(&work->labels[i]));
        }
        for (i = 0; i < work->count; i++) {
            if (work->twoColumns == 0) {
                work->numbers[i].x = (unsigned short)work->window.x + (i / 3) * 0xa0 + 0x60;
                work->numbers[i].y = (unsigned short)work->window.y + (i % 3) * 0x10 + 3;
            } else {
                work->numbers[i].x = (unsigned short)work->window.x + (i / 2) * 0xa0 + 0x60;
                work->numbers[i].y = (unsigned short)work->window.y + (i % 2) * 0x10 + 3;
            }
            eNumberMain((void *)(&work->numbers[i]));
        }
        return;
    }
}

static void MenuShopEx2(TskObject *task, MenuShopEx2Work *work)
{
    short x;
    MenuShopWeapon *weapon;

    switch (task->state) {
    case 0:
        work->color = 0xFFFF00;
        WindowDXSet(&work->window);
        work->window.x = -0xB6;
        work->window.color = work->color;
        work->window.y = 0x100;
        work->window.width = 0xA6;
        work->window.height = 0x1E;
        work->window.title = D_004DB230;
        work->window.state = 1;
        WindowDXMain(&work->window);
        work->window.state = 3;
        eMessageSet(&work->message, 0);
        work->message.mode = 0x20;
        work->message.color = work->color + 2;
        break;

    case 2:
        x = -0xB6;
        if (MenuShopWork->state == 0x30 && MenuShopWork->category == 4 && MenuShopWork->listIndex >= 0) {
            weapon = func_A1A3D8(MenuSortGet(0, MenuShopWork->listIndex));
            if (weapon != 0 && (weapon->flags & 0x100) && weapon->bullet != 0) {
                work->message.text = *func_A2C738(weapon->bullet);
                x = 0x10;
            }
        }
        MoveSlide(&work->window.x, &x, 3.0f);
        WindowDXMain(&work->window);
        work->message.x = work->window.x + 3;
        work->message.y = work->window.y + 3;
        eMessageMain(&work->message);
        break;
    }
}

extern MenuShopWorkData *MenuShopWork;

static void MenuShopLine(unsigned char *arg0, unsigned char *arg1) {
    short arr[4];
    short tgt[8];
    short *var_19;
    unsigned int var_5;
    int var_18;
    unsigned char temp_3;
    unsigned char cur;
    unsigned char *var_16;
    unsigned char *temp_2;
    int *temp_4;

    temp_3 = arg0[0x10];
    switch (temp_3) {
    case 0:
        {
            unsigned char c60 = 0x60;
            signed char cm80 = -0x80;
            unsigned short c30 = 0x30;
            *(int *)(arg1 + 0x4) = 0xFFFFF0;
            var_5 = 0;
            for (var_18 = 0; var_18 < 5; var_18++) {
                temp_2 = (unsigned char *)arg1;
                temp_2 = temp_2 + (var_5 + var_18 * 12);
                temp_4 = (int *)arg1;
                temp_4 = temp_4 + 3;
                temp_4 = (int *)((char *)temp_4 + (var_5 + var_18 * 12));
                temp_2[0x12] = c60;
                temp_2[0x11] = c60;
                temp_2[0x10] = c60;
                temp_2[0x13] = cm80;
                *(unsigned short *)(temp_2 + 0x8) = 0;
                *(unsigned short *)(temp_2 + 0xA) = c30;
                *temp_4 = *(int *)(arg1 + 0x4);
            }
        }
        *(int *)(arg1 + 0x48) = 0;
        return;
    case 2:
        tgt[0] = 0x30;
        arr[0] = (MenuShopWork->category * 0x15) + 0x126;
        arr[1] = (MenuShopWork->category * 0x15) + 0x12A;
        arr[2] = (MenuShopWork->category * 0x15) + 0x12E;
        arr[3] = 0x200;
        switch (MenuShopWork->state) {
        case 0xA4:
        case 0xA0:
        case 0xA2:
        case 0x30:
        case 0x90:
            if (arr[0] == *(short *)(arg1 + 0x14)) {
                tgt[0] = 0x28;
            }
            cur = arg1[0x3];
            if (cur != 0x78) {
                arg1[0x3] = cur + 8;
            }
            break;
        default:
            if (arg1[0x3] != 0) {
                arg1[0x3] = arg1[0x3] - 8;
            }
            break;
        }
        *(unsigned short *)(arg1 + 0x8) = 0;
        *(unsigned short *)(arg1 + 0x44) = 0x200;
        var_19 = &arr[0];
        var_16 = arg1 + 0xC;
        var_18 = 3;
        do {
            MoveSlide((short *)(var_16 + 8), var_19, 3.0f);
            var_19 += 1;
            var_18 -= 1;
            var_16[0x13] = arg1[0x3];
            var_16 += 0xC;
        } while (var_18 >= 0);
        arg1[0x13] = arg1[0x3];
        MoveSlide((short *)(arg1 + 0x22), &tgt[0], 8.0f);
        endPrintExtFunc(*(int *)(arg1 + 0x4), 0, arg1 + 8);
        break;
    }
}

void MenuShopInfoSet(void)
{
    MenuShopWork->categoryMask = 0;

    if (MenuShopWork->mode == 0) {
        if (MenuShopWork->noCategories != 0)
            return;
        if (ShopData[0].count != 0)
            MenuShopWork->categoryMask = 1;
        if (ShopData[1].count != 0)
            MenuShopWork->categoryMask |= 0x2;
        if (ShopData[2].count != 0)
            MenuShopWork->categoryMask |= 0x4;
        if (ShopData[3].count != 0)
            MenuShopWork->categoryMask |= 0x8;
        if (ShopData[4].count != 0)
            MenuShopWork->categoryMask |= 0x10;
        if (ShopData[5].count != 0)
            MenuShopWork->categoryMask |= 0x20;
        if (ShopData[6].count != 0)
            MenuShopWork->categoryMask |= 0x40;
        if (ShopData[9].count != 0)
            MenuShopWork->categoryMask |= 0x80;
        if (MenuScenarioNo >= 115)
            MenuShopWork->categoryMask |= 0x100;
    } else {
        MenuSortSet(0, 1, 0);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x1;
        MenuSortSet(0, 4, -1);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x2;
        MenuSortSet(0, 16, -1);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x4;
        MenuSortSet(0, 8, -1);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x8;
        MenuSortSet(0, 4, -2);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x10;
        MenuSortSet(0, 16, -2);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x20;
        MenuSortSet(0, 8, -2);
        if (MenuSortCheck(0) != 0)
            MenuShopWork->categoryMask |= 0x40;
        if (dataEvtBoxChk(53) != 0 || dataEvtBoxChk(54) != 0 || dataEvtBoxChk(55) != 0)
            MenuShopWork->categoryMask |= 0x100;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/menu_shop", subMenuShopNumerInc_11);

INCLUDE_ASM("asm/main/nonmatchings/menu_shop", subMenuShopNumerDec_12);

INCLUDE_ASM("asm/main/nonmatchings/menu_shop", MenuShopCore);

extern void MenuShopSelect(TskObject *task, void *data);
extern void MenuShopEx(TskObject *task, void *data);
extern void MenuShopSeisan(TskObject *task, void *data);
extern void MenuShopSeisan02(TskObject *task, void *data);
extern void MenuShopIcon(TskObject *task, void *data);
extern void MenuShopParameter(TskObject *task, void *data);
extern XglTaskScheduler *MenuShopTask;
extern int ShopDataBufTop;
extern unsigned int GameLoopState[];
typedef struct MenuShopRenderView { unsigned char pad[0x10]; unsigned short u10; unsigned short u12; unsigned short u14; } MenuShopRenderView;
extern MenuShopRenderView sRender;
extern const char D_004C7AA8[];
extern void endPrintDirectFrameCopy(XglPacket *, int, int);
extern void MenuGameDataPush(void);
extern void MenuGameDataPop(void);
extern void MenuCfTaikiPush(void);
extern void MenuCfTaikiPop(void);
extern void xglStudioChange(int);
extern void *MenuWorkEndGet(void);
extern void *MenuBgTaskInit(unsigned char *, int);
extern int MenuModelInit(int work_start);
extern void *GameResourceWorkAlloc(int);
extern void MenuModelMemoryInit(void *, int);
extern void MenuModelMemorySet(int);
extern void MenuModelMenuMotionLoad(void);
extern void MenuWorkEndCheck(int);
extern void MenuLoadInit(void);
extern void endBackTexDraw(void *);
extern void MenuFontLoad(int, int);
extern void WindowTexLoad(void *, unsigned int);
extern void MenuLoadFile(const char *name, void *buffer);
extern int MenuLoadSync(void);
extern void MenuBgTaskMain(void);
extern void MenuShopCore(void);
extern int MenuScenarioNoGet(void);
extern void GameSnapShotCheck(void);
extern void MenuLoadCancel(void);
extern void MenuBgTaskBreak(void);
extern void MenuModelAllBreak(void);
extern void GameResourceWorkReload(void);
extern void GameCfPlayerLoadResource(int);
extern void nmlModelFlush(void);
extern void MenuModelMain(void);
extern void MenuShopAgwsListChange(void);
extern void MenuKeepSelectReset(void);
extern int PartyAllPartyGet(int *out, int mode);
extern int MenuEquipStealMaskCheck(void);
typedef struct MenuShopBlk170 { unsigned char b[0x170]; } MenuShopBlk170;
void endPrintInit(void);
void *xglTaskInitial(void *, int, int);
void xglSoundEffectNormalID(int, int);
typedef struct MenuShopBlk32 { unsigned char b[0x20]; } MenuShopBlk32;
typedef struct MenuShopBlk16 { unsigned char b[0x10]; } MenuShopBlk16;
static inline XglTaskPrefix *MenuShopTaskTail(XglTaskScheduler *sched)
{
    XglTaskPrefix *tail = 0;
    if (sched != 0) {
        tail = sched->active_tail;
    }
    return tail;
}
static inline XglTaskPrefix *MenuShopTaskEntry(XglTaskScheduler *sched, int (*fn)(XglTaskPrefix *))
{
    XglTaskPrefix *tail = 0;
    if (sched != 0) {
        tail = sched->active_tail;
    }
    return xglTaskEntryNext(sched, fn, tail);
}
#define MenuShopEntry(fn, func, data) TskObjectSet3((TskObject *)xglTaskEntryNext(MenuShopTask, (int (*)(XglTaskPrefix *))(fn), MenuShopTask != 0 ? MenuShopTask->active_tail : 0), func, data)

INCLUDE_ASM("asm/main/nonmatchings/menu_shop", MenuShopMain);

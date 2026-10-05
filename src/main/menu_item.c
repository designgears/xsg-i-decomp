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

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemPasMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemInfoMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemLineMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemStatusMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemStatusAgwsMain);

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

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemListMake00_1);

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

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemListMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemSelectMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemIconMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItemExMain);

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

INCLUDE_ASM("asm/main/nonmatchings/menu_item", MenuItem);

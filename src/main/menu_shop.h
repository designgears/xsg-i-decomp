/*
 * TU-local declarations of main/tu173 (src/main/menu_shop.c).
 */

#ifndef SRC_MAIN_MENU_SHOP_H
#define SRC_MAIN_MENU_SHOP_H

typedef struct MenuShopWorkData MenuShopWorkData;

struct MenuShopWorkData {
    unsigned char state;
    unsigned char unmodeled_01[0]; /* +0x1: zero-length alias of the former padding member, kept for saved bodies */
    unsigned char stateFlags;
    unsigned char nextState;
    unsigned char waitFrames;
    unsigned char unmodeled_04[2];
    unsigned short categoryMask;
    unsigned char unmodeled_08[8];
    unsigned char mode;
    unsigned char category;
    unsigned char unmodeled_12[0]; /* +0x12: zero-length alias of the former padding member, kept for saved bodies */
    signed char quantity;
    unsigned char quantityAction;
    unsigned char quantityLimit;
    unsigned char canEquip;
    unsigned char sortOption;
    unsigned char noCategories;
    unsigned char unmodeled_18[8];
    int listIndex;
    unsigned char unmodeled_24[0]; /* +0x24: zero-length alias of the former padding member, kept for saved bodies */
    int itemId;
    int modelId;
    unsigned int modelPending;
    long long availableMoney;
    unsigned char unmodeled_38[8];
    short unitId;
    short equipmentId;
    signed char equipmentStatus;
    signed char unitCount;
    unsigned char unmodeled_46;
    signed char unitSelection;
    signed char sort_mode_selector;
    unsigned char unmodeled_49[7];
    /* PartyAgwsGet emits at most six IDs; the core stores their low halfwords. */
    short unitIds[6];
};

extern MenuShopWorkData *MenuShopWork;

/* MenuShopSelect work (ported from upstream): two command panes and one
 * yes/no pane, each a 0x194-byte WindowDX followed by its panel. */
typedef struct MenuShopSelectWindow MenuShopSelectWindow;
struct MenuShopSelectWindow {
    short x;
    short y;
    int style;
    short width;
    short height;
    const char *tag_id;
    unsigned char state;
    unsigned char unmodeled_11[3];
    void (*close_callback)(MenuShopSelectWindow *window, void *arg);
    void *close_callback_arg;
    unsigned char unmodeled_1c[0x194 - 0x1c];
};

typedef struct MenuShopSelectCommand {
    const char *text;
    unsigned char unmodeled_04[4];
    unsigned char locked;
    unsigned char unmodeled_09[3];
} MenuShopSelectCommand;

typedef struct MenuShopCommandPanel {
    unsigned char unmodeled_00[2];
    unsigned short displayMode;
    int cursor;
    unsigned char unmodeled_08[4];
    unsigned char unmodeled_0c[4];
    MenuShopSelectCommand *commands;
    unsigned char unmodeled_14[1508];
} MenuShopCommandPanel;

typedef enum MenuShopChoiceCursor {
    MENU_SHOP_CHOICE_FIRST = 0
} MenuShopChoiceCursor;

typedef struct MenuShopChoicePanel {
    unsigned short unmodeled_00;
    unsigned short displayMode : 16;
    MenuShopChoiceCursor cursor;
    const char *choices;
    const char *prompt;
    int unmodeled_10;
    void *unmodeled_14;
    unsigned char unmodeled_18[1504];
} MenuShopChoicePanel;

typedef struct MenuShopCommandPane {
    MenuShopSelectWindow window;
    MenuShopCommandPanel panel;
    MenuShopSelectCommand commands[8];
} MenuShopCommandPane;

typedef struct MenuShopChoicePane {
    MenuShopSelectWindow window;
    MenuShopChoicePanel panel;
} MenuShopChoicePane;

typedef struct MenuShopSelectWork {
    unsigned char unmodeled_00[3];
    unsigned char popupFlags;
    int depth;
    MenuShopCommandPane menus[2];
    MenuShopChoicePane choices[1];
} MenuShopSelectWork;

extern const char *msg00_1_0036DB30[8];
extern const char *msg01_2_0036DB50[4];
extern const char D_004DB168[];
extern const char D_004DB170[];
extern int MenuSelectWindow();

#endif /* SRC_MAIN_MENU_SHOP_H */

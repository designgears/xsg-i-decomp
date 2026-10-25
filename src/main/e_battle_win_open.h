/*
 * TU-local declarations of main/tu166 (src/main/e_battle_win_open.c).
 */

#ifndef SRC_MAIN_E_BATTLE_WIN_OPEN_H
#define SRC_MAIN_E_BATTLE_WIN_OPEN_H

#include "shared.h"

/*
 * WindowDX is the sub-window control block WindowDXSet/WindowDXMain/
 * WindowDXFlagChange share, and the type every "*Main" screen task that
 * calls WindowDXSet embeds one of (TopStatusWinMain, the Char/Agws/Menu*Main
 * family, eBattleWinOpen/2/3/4, ...). Only the members WindowDXSet itself
 * writes are modelled here:
 *   +0x0C tag_id             -- WindowDXMain passes it as eTagFontSet's
 *                                second argument (`lw $5,0xC($16)` in the
 *                                delay slot of `jal eTagFontSet`, $4 =
 *                                window+0x80).
 *   +0x10 state               -- WindowDXFlagChange stores its flag argument
 *                                (0..4) here; WindowDXMain switches on it.
 *   +0x11 ribbon_initialized  -- WindowDXMain skips its eRibbonMain re-init
 *                                block once this is set.
 *   +0x14 close_callback      -- WindowDXMain loads it and, once non-null,
 *                                calls close_callback(window,
 *                                close_callback_arg).
 *   +0x18 close_callback_arg  -- the callback's second argument (`lw
 *                                $5,0x18($16)` in the jalr delay slot).
 *   +0x2C..0x2F color         -- set by WindowDXSet itself to a translucent
 *                                gray (0x80,0x80,0x80 RGB, 0x60 alpha).
 * Everything else is unmodeled until a function that reads or writes it is
 * recovered.
 */
typedef struct WindowDX WindowDX;
/* fork type needed by WindowDX (derwetterfrosch) */
typedef struct WindowDXRect {
    short x;
    short y;
    int style;
    short width;
    short height;
} WindowDXRect;

typedef struct WindowDXFrame {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int style;
    short width;
    short height;
    signed char progress;
    unsigned char mode;
    unsigned char unmodeled_12[0x44 - 0x12];
} WindowDXFrame;

typedef struct BattleWinTagFont {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int style;
    unsigned char color_r;
    unsigned char color_g;
    unsigned char color_b;
    unsigned char color_a;
    unsigned char unmodeled_10[0x20 - 0x10];
} BattleWinTagFont;

typedef struct WindowDXRibbonCell {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    unsigned char unmodeled_04[8];
} WindowDXRibbonCell;

typedef struct WindowDXRibbon {
    short x;
    short y;
    int style;
    short width;
    short height;
    unsigned char unmodeled_0c[0x18 - 0x0c];
    WindowDXRibbonCell cells[6];
    unsigned char unmodeled_60[0x70 - 0x60];
} WindowDXRibbon;

typedef struct WindowDXSprite {
    unsigned char unmodeled_00[0x10];
    unsigned short kind;
    unsigned char unmodeled_12[0x14 - 0x12];
} WindowDXSprite;

struct WindowDX {
    unsigned char unmodeled_00[0]; /* +0x0: zero-length alias of the former padding member, kept for saved bodies */
    short x;
    short y;
    int style;
    short width;
    short height;
    int tag_id;                        /* +0x0C */
    unsigned char state;               /* +0x10 */
    unsigned char ribbon_initialized;  /* +0x11 */
    unsigned char unmodeled_12[2];
    void (*close_callback)(WindowDX *window, void *arg); /* +0x14 */
    void *close_callback_arg;          /* +0x18 */
    unsigned char unmodeled_1c[0x4];
    WindowDXRect rect;
    unsigned char color_r;             /* +0x2C */
    unsigned char color_g;             /* +0x2D */
    unsigned char color_b;             /* +0x2E */
    unsigned char alpha;               /* +0x2F */
};

void WindowDXSet(WindowDX *window);

/*
 * BW is the work area eBattleWinOpen/eBattleWinMain/eBattleWinClose share
 * for the item/ether "get" screens: menuCloseEth and menuCloseItm
 * (src/ov01/menu.c) each call eBattleWinClose on their own screen object,
 * but eBattleWinClose ignores that argument and always clears BW itself
 * ($a0 is overwritten before it is ever read), which is why the parameter
 * below is unused. eBattleWinOpen embeds a WindowDX at +0x1740
 * (`addiu $4,$4,0x1740` right before `jal WindowDXSet`, with $4 = BW), so
 * window.state lands at BW+0x1750.
 */
typedef struct BattleWindow BattleWindow;
/* fork type needed by BattleWindow (derwetterfrosch) */
typedef struct BattleWinCommand {
    const char *text;
    const char *description;
    unsigned char flags;
    signed char icon;
    unsigned char unmodeled_0a[2];
} BattleWinCommand;

typedef struct BattleWinMessage {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int extent;
    unsigned char unmodeled_0c[4];
    unsigned char color_r;
    unsigned char color_g;
    unsigned char color_b;
    unsigned char unmodeled_13[0x18 - 0x13];
    const char *text;
    unsigned char paged;
    unsigned char unmodeled_1d[0x44 - 0x1d];
} BattleWinMessage;

struct BattleWindow {
    unsigned char unmodeled_00[0x1];
    unsigned char mode;
    unsigned char unmodeled_2[0x2];
    BattleWinCommand *commands;
    unsigned char state;      /* +0x0008 */
    unsigned char unmodeled_09[0x5b];
    BattleWinMessage message;
    unsigned char unmodeled_a8[0x1698];
    WindowDX window;          /* +0x1740 */
    unsigned char unmodeled_1770[0x18D5 - (0x1740 + 0x30)];
    unsigned char active;     /* +0x18D5 */
};

extern BattleWindow *BW;

void eBattleWinClose(void *window);

/*
 * BW2 is BW's counterpart for the second "get" screen: eBattleWinOpen2
 * embeds a WindowDX at +0x8 (`addiu $4,$2,0x8` with $2 = BW2 right before
 * `jal WindowDXSet`), so window.state lands at BW2+0x18.
 */
typedef struct BattleWindow2 BattleWindow2;
struct BattleWindow2 {
    unsigned char unmodeled_00[0]; /* +0x0: zero-length alias of the former padding member, kept for saved bodies */
    int active;
    int text_offset;
    WindowDX window; /* +0x0008 */
};

extern BattleWindow2 *BW2;

void eBattleWinClose2(void);

/*
 * BW3 is BW's counterpart for the third "get" screen: eBattleWinOpen3
 * embeds a WindowDX at +0x4 (`addiu $4,$3,0x4` with $3 = BW3 right before
 * `jal WindowDXSet`), so window.state lands at BW3+0x14. secondary_state is
 * set to the same value as window.state whenever eBattleWinClose3 runs, but
 * nothing in this allocation reads it back.
 */
typedef struct BattleWindow3 BattleWindow3;
/* fork type needed by BattleWindow3 (derwetterfrosch) */
typedef struct BattleWinNumber {
    short x;
    short y;
    int style;
    unsigned char color_r;
    unsigned char color_g;
    unsigned char color_b;
    unsigned char unmodeled_0b;
    unsigned char font;
    unsigned char unmodeled_0d;
    unsigned char option;
    unsigned char places;
    unsigned char unmodeled_10[4];
    int value;
    unsigned char unmodeled_18[0x90 - 0x18];
} BattleWinNumber;

struct BattleWindow3 {
    unsigned char unmodeled_00[0]; /* +0x0: zero-length alias of the former padding member, kept for saved bodies */
    int active;
    WindowDX window;              /* +0x0004 */
    unsigned char unmodeled_34[0]; /* +0x34: zero-length alias of the former padding member, kept for saved bodies */
    BattleWinMessage messages[3];
    BattleWinTagFont tag_font;
    BattleWinNumber number;
    WindowDX window2;
    BattleWinTagFont tag_fonts[5];
    unsigned char unmodeled_280[0xa4];
    unsigned char secondary_state; /* +0x0324 */
};

extern BattleWindow3 *BW3;

void eBattleWinClose3(void);

/*
 * BW4 is BW's counterpart for the fourth "get" screen: eBattleWinOpen4
 * embeds a WindowDX at +0x8 (`addiu $4,$2,0x8` with $2 = BW4 right before
 * `jal WindowDXSet`), so window.state lands at BW4+0x18. page is the word
 * eBattleWinPageCheck4 returns.
 */
typedef struct BattleWindow4 BattleWindow4;
struct BattleWindow4 {
    unsigned char unmodeled_00[0]; /* +0x0: zero-length alias of the former padding member, kept for saved bodies */
    int active;
    int page;         /* +0x0004 */
    WindowDX window;  /* +0x0008 */
};

extern BattleWindow4 *BW4;

void eBattleWinClose4(void);

int eBattleWinPageCheck4(void);

void *eBattleWinInit2(void *arg);

/* Not yet recovered (src/main/window_tex_load.c); matches the prototype
 * already used at its other call sites (src/main/party.c). */
void *MenuWorkEndGet(void);

void eBattleWinInit(void);

/*
 * WindowSPCursor is the SP (scrollable list) cursor record every WindowSP*
 * function of this TU shares (WindowSPSelect, WindowSPSet,
 * WindowSPItemChange, WindowSPSelectJump, WindowSPSetSelect -- all still
 * assembler), and that WindowSPKeepSelect/WindowSPKeepSelectCheck save and
 * clear on behalf of the seven MenuXxx screens (MenuItem, MenuCharactor,
 * MenuAgws, MenuShopCore, MenuEther, MenuTec, MenuSkill -- outside this TU,
 * not yet recovered) that call them. Only the members this allocation and
 * its siblings touch are modeled:
 *   +0x20 visible_top     WindowSPItemChange (0x00283cf0) clamps this
 *                          against +0x22 (0x00283e18..0x00283e34), the shape
 *                          of a scrolled list's first visible row.
 *   +0x21 visible_bottom  the same function's symmetric clamp against +0x23
 *                          (0x00283e64..0x00283e80).
 *   +0x24 current_row     set from a row WindowSPItemChange computes by
 *                          dividing item_count by the row/column grid at
 *                          +0x14/+0x15 (0x00283d94..0x00283da8).
 *   +0x28 selected_index  a signed halfword; WindowSPSet (0x00283b88) resets
 *                          it to -1 ("nothing selected") and
 *                          WindowSPItemChange advances it while it stays
 *                          below item_count.
 *   +0x2A item_count      a halfword; WindowSPSet walks a linked list from
 *                          +0x1C to count it (0x00283bc4..0x00283bf4) and
 *                          stores the result here.
 * Everything else is unmodeled until a function that reads or writes it is
 * recovered.
 */
typedef struct BattleWinItem BattleWinItem;
/* fork type needed by WindowSPCursor (derwetterfrosch) */
typedef struct BattleWinCursor {
    unsigned char mode;
    unsigned char shown;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int style;
    unsigned char unmodeled_0c[0x24 - 0x0c];
} BattleWinCursor;

typedef struct WindowSPCursor {
    unsigned char unmodeled_00[0]; /* +0x0: zero-length alias of the former padding member, kept for saved bodies */
    unsigned char state;
    unsigned char draw_flags;
    unsigned char anim;
    unsigned char unmodeled_3[0x1];
    short x;
    short y;
    int style;
    short width;
    short height;
    const char *title;
    signed char columns;          /* +0x14 */
    signed char rows;             /* +0x15 */
    unsigned char unmodeled_16[2];
    int buttons;                  /* +0x18 */
    BattleWinItem *items;         /* +0x1C */
    signed char visible_top;      /* +0x20 */
    signed char visible_bottom;   /* +0x21 */
    union {
        unsigned char unmodeled_22[2];
        struct { signed char visible_cols; signed char visible_rows; };
    };
    signed char current_row;      /* +0x24 */
    union {
        unsigned char unmodeled_25[3];
        struct { signed char max_scroll; signed char mode; unsigned char unmodeled_27; };
    };
    short selected_index;         /* +0x28 */
    short item_count;             /* +0x2A */
} WindowSPCursor;

/*
 * A saved snapshot of one WindowSPCursor: item_count, selected_index,
 * visible_top, visible_bottom and current_row packed into 5 bytes in that
 * order (see WindowSPKeepSelect). WindowSPKeepSelectCheck clears it when
 * MenuCursorKeepCheck (main/menu_1.c, still assembler) reports nothing to
 * restore.
 */
typedef unsigned char WindowSPKeepBuffer[5];

#endif /* SRC_MAIN_E_BATTLE_WIN_OPEN_H */

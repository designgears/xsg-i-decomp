#include "common.h"
#include "shared.h"
#include "e_battle_win_open.h"

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", eBattleWinOpen);

void eBattleWinClose(void *window)
{
    BW->state = 4;
    BW->window.state = 4;
    BW->active = 0;
}

short WindowSPSelect();
void WindowSPMain();
void WindowDXMain();
void eMessageMain();
void endPrintExtFunc();
extern PadPrefix PadData;

void eBattleWinMain(int *arg0)
{
    int sel = -1;
    int mode = BW->unmodeled_00[1];
    PadPrefix *pad = &PadData;
    PadPrefix *pad2;
    int key = *(unsigned short *)((char *)pad + 0x34);

    if (mode != -1) {
        if (mode >= -1) {
            if (mode < 2) {
                sel = WindowSPSelect(&BW->state, key);
                if (sel >= 0) {
                    int off = sel * 12;
                    *(int *)&BW->unmodeled_1770[0x18EC - 0x1770] = *(int *)(*(int *)&BW->unmodeled_00[4] + off + 4);
                }
            }
        }
        pad2 = &PadData;
        arg0[8] = sel;
        arg0[7] = pad2->half_2a;
        endPrintExtFunc(0, 100, 0);
        WindowSPMain(&BW->state);
        BW->unmodeled_09[0x3C] = 1;
        WindowDXMain(&BW->window);
        eMessageMain(&BW->unmodeled_1770[0x164]);
    } else {
        arg0[8] = mode;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", eBattleWinOpen2);

int eBattleWinMain2(void)
{
    BattleWindow2 *w = BW2;

    if (*(int *)w == 0) {
        return 0;
    }
    endPrintExtFunc(0xFFFFFF, 0x64, 0);
    WindowDXMain(&BW2->window);
    switch ((signed char)BW2->window.state) {
    case 0:
        *(int *)BW2 = 0;
        return 0;
    case 3:
        eMessageMain((char *)BW2 + 0x19C);
        return 2;
    case 1:
    case 2:
    case 4:
    case 5:
        return 1;
    }
    return 0;
}

void eBattleWinClose2(void)
{
    BW2->window.state = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", eBattleWinOpen3);

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", eBattleWinMain3);

void eBattleWinClose3(void)
{
    BW3->window.state = 4;
    BW3->secondary_state = 4;
}

extern void eMessageSet(void *message, int text);
extern void eMessageTextChange(void *message, int text);
typedef struct { unsigned short x; unsigned short p0; unsigned short y; unsigned short p1; char *z; int w; int t; } OpenArg4;
typedef struct {
    int active;
    int page;
    unsigned short x;
    unsigned short y;
    char *z;
    short w;
    short h;
    int tag;
    unsigned char state;
    unsigned char ribbon;
    unsigned char pad[0x19C - 0x1A];
    char msg[1];
    unsigned char mc;
    unsigned char pad2[2];
    short mx;
    short my;
    int mz;
    unsigned char pad3[0x1B8 - 0x1A8];
    unsigned char mf;
} V4;
#define V ((V4 *)BW4)

void eBattleWinOpen4(OpenArg4 *a)
{
    V4 *w;
    int tg;
    char *z;
    unsigned short x, y;
    WindowDXSet(&BW4->window);
    w = V;
    tg = a->w;
    x = a->x;
    y = a->y;
    z = a->z;
    w->x = x;
    w->y = y;
    w->z = z;
    w->w = 0x1D4;
    w->h = 0x70;
    w->tag = tg;
    w->ribbon = 1;
    eMessageSet(V->msg, a->t);
    V->mf = 1;
    eMessageTextChange(V->msg, a->t);
    V->mz = (int)(V->z + 2);
    V->mx = V->x + 3;
    V->my = V->y + 3;
    V->mc = 0x22;
    V->state = 1;
    V->active = 1;
    V->page = 0;
}

extern int eMessageNextPage(void *message, int arg);
extern PadPrefix PadData;

int eBattleWinMain4(void)
{
    BattleWindow4 *w;
    int r;

    if (*(int *)BW4 == 0) {
        return 0;
    }
    endPrintExtFunc(0xFFFFFF, 0x64, 0);
    WindowDXMain(&BW4->window);
    switch ((signed char)BW4->window.state) {
    case 3:
        if (PadData.half_2a & 0x20) {
            r = eMessageNextPage((char *)BW4 + 0x19C, 0);
            w = BW4;
            if (r == 0) {
                w->page = 1;
            }
        }
        eMessageMain((char *)BW4 + 0x19C);
        return 2;
    case 1:
    case 2:
    case 4:
    case 5:
        return 1;
    }
    return 0;
}

void eBattleWinClose4(void)
{
    BW4->window.state = 4;
}

int eBattleWinPageCheck4(void)
{
    return BW4->page;
}

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", eBattleWinInit2);

void eBattleWinInit(void)
{
    eBattleWinInit2(MenuWorkEndGet());
}

void WindowDXFlagChange(WindowDX *window, int flag)
{
    unsigned char s;
    switch (flag) {
    case 1:
        s = window->state;
        if ((unsigned char)(s + 1) < 2 || (signed char)s == 4 || (signed char)s == 5) {
            window->state = 1;
        }
        break;
    case 3:
        window->state = flag;
        break;
    case 4:
        if ((signed char)window->state == 3) {
            window->state = flag;
        }
        break;
    default:
        window->state = flag;
        break;
    }
}

void WindowDXSet(WindowDX *window)
{
    /*
     * These nine stores are independent of each other, so -O2 schedules them
     * out of source order: it fills the jr delay slot with the third-to-last
     * store below and moves the last store to right after the two immediate
     * loads. color_r is written last here so it lands in that early slot,
     * matching the original's own first store to window.
     */
    window->alpha = 0x60;
    window->tag_id = 0;
    window->close_callback = 0;
    window->close_callback_arg = 0;
    window->state = 0;
    window->ribbon_initialized = 0;
    window->color_g = 0x80;
    window->color_b = 0x80;
    window->color_r = 0x80;
}

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", WindowDXMain);

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", subMWModeExSet);

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", subMWPosSet);

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", subMWDraw);

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", subMWControlType00);

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", subMWControlType01);

typedef struct SPSel {
    unsigned char kind;
    char pad1[0x13];
    signed char mode;
    char pad15[3];
    int arg;
    int active;
    char pad20[8];
    short result;
    short flag;
} SPSel;
void subMWControlType00(void *, int);
void subMWControlType01(void *, int);

short WindowSPSelect(SPSel *w, int arg1) {
    unsigned char k;
    if (w == 0) return -1;
    if (w->active == 0) return -1;
    if (w->flag == 0) return -1;
    k = w->kind;
    if (w->kind != 3) return -1;
    w->arg = arg1;
    switch (w->mode) {
    case 1: subMWControlType00(w, k); break;
    case 2: subMWControlType01(w, k); break;
    }
    return w->result;
}

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", WindowSPSet);

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", WindowSPItemChange);

typedef struct SPJ {
    char pad0[0x14];
    signed char mode;
    signed char n;
    char pad16[5];
    int active;
    char pad20[1];
    signed char v21;
    char pad22;
    unsigned char v23;
    signed char top;
    char pad25[3];
    short cur;
} SPJ;

void WindowSPSelectJump(SPJ *w, int sel) {
    int cur, n, top;
    if (w->active == 0) return;
    cur = w->cur;
    if (cur == sel) return;
    if (w->mode != 1) return;
    if (sel == 0) {
        w->top = 0;
        w->v21 = 0;
        w->cur = 0;
        return;
    }
    n = w->n;
    if (cur - sel >= n) goto reset;
    top = w->top;
    if (sel - top >= n) {
        int m = w->v23;
        w->cur = sel;
        w->top = sel - (unsigned char)w->n + 1;
        w->v21 = m - 1;
        return;
    }
    if (sel < top) {
reset:
        w->v21 = 0;
        w->top = sel;
        w->cur = sel;
        return;
    }
    w->cur = sel;
    w->v21 = sel - (unsigned char)w->top;
}

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", WindowSPMain);

void WindowSPKeepSelect(WindowSPCursor *cursor, WindowSPKeepBuffer keep)
{
    if (cursor->item_count != 0) {
        keep[0] = (unsigned char)cursor->item_count;
        keep[1] = (unsigned char)cursor->selected_index;
        keep[2] = cursor->visible_top;
        keep[3] = cursor->visible_bottom;
        keep[4] = cursor->current_row;
    }
}

/* main/menu_1.c, still assembler; called here with the same keep-select
 * buffer, matching the pass-through argument this call site proves. */
extern int MenuCursorKeepCheck(WindowSPKeepBuffer keep);

void WindowSPKeepSelectCheck(WindowSPKeepBuffer keep)
{
    if (MenuCursorKeepCheck(keep) == 0) {
        memset(keep, 0, 5);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/e_battle_win_open", WindowSPSetSelect);

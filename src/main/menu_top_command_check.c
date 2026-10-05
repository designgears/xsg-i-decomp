#include "common.h"

extern unsigned short MenuTopCommandLock;

int MenuTopCommandCheck(int commandId)
{
    unsigned int commandBit = 1 << (commandId - 1);
    return (commandBit & ~MenuTopCommandLock) != 0;
}

typedef struct {
    unsigned char pad0[3];
    unsigned char m3;
    unsigned char pad4;
    signed char m5;
} MenuWorkView;

extern MenuWorkView MenuWork[];

void WindowDXSet(void *window);

void WindowDXMain(void *window);

void MoveSlide(short *current, short *target, float rate);

INCLUDE_ASM("asm/main/nonmatchings/menu_top_command_check", TopStatusWinMain);

typedef struct {
    unsigned char state;
    unsigned char unk1;
    signed char unk2;
    unsigned char pad3;
    short x;
    short y;
    int col;
    short w;
    short h;
    unsigned char pad10[4];
    unsigned char mode;
    unsigned char pad15[3];
    void *func;
    void *work;
    unsigned char pad20[0x180];
    short w0;
    short w1;
    unsigned char w2;
    unsigned char pad1a5[7];
    char *msg;
} TopInfoWork;
extern TopInfoWork *TopInfo;
extern char *text[];
extern char D_004DA890[];
void MenuInfoWindow(void);
void TopInfoMain(void)
{
    TopInfoWork *t = TopInfo;
    void *win;
    short target[8];

    short *p;

    switch (t->state) {
    case 0:
        win = &t->x;
        WindowDXSet(win);
        t->x = -0x10;
        t->y = 0x1E0;
        t->w = 0x220;
        t->h = 0x36;
        t->col = 0xF000F0;
        p = &t->w0;
        *p = 0;
        t->func = MenuInfoWindow;
        t->work = p;
        t->w1 = 0;
        t->mode = 1;
        WindowDXMain(win);
        t->mode = 3;
        t->unk2 = -1;
        t->unk1 = 1;
        t->state = 2;
        t->w2 = 0;
        break;
    case 2:
        win = &t->x;
        break;
    default:
        return;
    }
    target[0] = 0x182;
    switch (MenuWork->m3) {
    case 0x20:
        t->unk2 = -1;
    case 0x21:
        t->w2 = 0;
        t->w1 = 0;
        break;
    default:
        target[0] = 0x1E0;
        break;
    }
    if (MenuWork->m5 < 9) {
        t->msg = text[MenuWork->m5];
    } else {
        t->msg = D_004DA890;
    }
    MoveSlide(&t->y, target, 5.0f);
    WindowDXMain(win);
}

typedef struct {
    int col0;
    unsigned char state;
    unsigned char unk1;
    unsigned char pad6[2];
    short x;
    short y;
    int col;
    short w;
    short h;
    char *msg;
    unsigned char mode;
    unsigned char pad19[3];
    void *func;
    void *work;
    unsigned char pad24[0x178];
    short w0;
    short w1;
    int sel;
    unsigned char pad1a4[8];
    void *cmd;
} TopMenuWinWork;
typedef struct {
    unsigned char pad[8];
    unsigned char lock;
    unsigned char pad9[3];
} TopMenuCmd;
extern TopMenuWinWork *TopMenuWin;
extern unsigned char MenuTopCommand[];
extern TopMenuCmd command_0[];
extern char D_004DA8B8[];
void MenuSelectWindow(void);

void TopMenuWinMain(void)
{
    TopMenuWinWork *t = TopMenuWin;
    void *win;
    short target[8];
    int i;

    switch (t->state) {
    case 0:
        t->col0 = 0xF000F0;
        for (i = 0; i < 9; i++) {
            if (MenuTopCommandCheck(MenuTopCommand[i]) == 0) {
                command_0[i].lock = 1;
            } else {
                command_0[i].lock = 0;
            }
        }
        win = &t->x;
        WindowDXSet(win);
        t->x = 0x220;
        t->func = MenuSelectWindow;
        t->y = 0x1C;
        t->col = t->col0;
        t->w = 0xC2;
        t->h = 0xDE;
        t->work = &t->w0;
        t->msg = D_004DA8B8;
        t->cmd = command_0;
        t->sel = 0;
        t->w1 = 2;
        t->mode = 1;
        WindowDXMain(win);
        t->mode = 3;
        t->unk1 = 1;
        t->state = 2;
        break;
    case 2:
        win = &t->x;
        break;
    default:
        return;
    }
    target[0] = 0x13C;
    switch (MenuWork->m3) {
    case 0x20:
    case 0x21:
        t->sel = MenuWork->m5;
        break;
    default:
        target[0] = 0x220;
        break;
    }
    MoveSlide(win, target, 5.0f);
    WindowDXMain(win);
}

INCLUDE_ASM("asm/main/nonmatchings/menu_top_command_check", TopFaceExWinMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_top_command_check", TopMenu);

INCLUDE_ASM("asm/main/nonmatchings/menu_top_command_check", tskMenuTai);

INCLUDE_ASM("asm/main/nonmatchings/menu_top_command_check", tskMenuTaiPointa);

INCLUDE_ASM("asm/main/nonmatchings/menu_top_command_check", MenuBackModelSet);

INCLUDE_ASM("asm/main/nonmatchings/menu_top_command_check", MenuStatusDisp);

INCLUDE_ASM("asm/main/nonmatchings/menu_top_command_check", MenuInfoWindow);

INCLUDE_ASM("asm/main/nonmatchings/menu_top_command_check", MenuSelectWindow);

void MenuPasWindow(void) {

}

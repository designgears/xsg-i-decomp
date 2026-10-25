/*
 * OV02 original TU 9: 0x00a05738..0x00a097d8 (10 functions)
 */
#include "common.h"

#include "shared.h"
typedef struct UmnMailPasTask {
    unsigned char unmodeled_00[0x10];
    int command;
} UmnMailPasTask;
typedef struct UmnMailPasWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    unsigned char unmodeled_0c[4];
    unsigned char state;
    unsigned char unmodeled_11[3];
    void (*callback)(void);
    void *callback_arg;
} UmnMailPasWindow;
typedef struct UmnMailPasMessage {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    unsigned char unmodeled_0c[0x44 - 0x0c];
} UmnMailPasMessage;
typedef struct UmnMailPasBox {
    short x;
    short y;
    int kind;
    short width;
    short height;
} UmnMailPasBox;
typedef struct UmnMailPasWork {
    unsigned char unmodeled_00;
    unsigned char state;
    unsigned char unmodeled_02[2];
    int kind;
    UmnMailPasWindow window;
    unsigned char unmodeled_24[0x19c - 0x24];
    UmnMailPasMessage messages[5];
    unsigned char unmodeled_2f0[0x3bc - 0x2f0];
    UmnMailPasBox box;
} UmnMailPasWork;
typedef struct UmnMailState {
    unsigned char unmodeled_00;
    unsigned char mail_mode;
    unsigned char unmodeled_02;
    unsigned char tab_state;
    unsigned char unmodeled_04[0x42 - 4];
    unsigned char visible_folder_index;
    unsigned char unmodeled_43;
    unsigned char scroll_offset;
    unsigned char unmodeled_45[0x0b];
    unsigned char selection_index;
    signed char folder_entry_count;
    signed char selected_mail_id;
    unsigned char unmodeled_53[9];
    signed char mail_selection_state;
    unsigned char saved_selection_index;
    unsigned char saved_scroll_offset;
    unsigned char saved_visible_folder_index;
} UmnMailState;
extern UmnMailState UmnWork;
extern const char *msg00_0_00A10618[5];
extern int MenuPasLengthGet(const char *text);
extern void WindowDXSet(void *window);
extern void WindowDXMain(void *window);
extern void MoveSlide(short *current, short *target, float rate);
extern void eMessageSet(void *message, const char *text);
extern void eMessageMain(void *message);
extern void endPrintExtFunc(int kind, int id, void *data);

void tskUmnMailPas(UmnMailPasTask *task, UmnMailPasWork *work)
{
    int caption_widths[5];
    int i;
    unsigned char mail_mode;

    for (i = 0; i < 5; i++) {
        caption_widths[i] = MenuPasLengthGet(msg00_0_00A10618[i]);
    }

    mail_mode = UmnWork.mail_mode;
    if (mail_mode != 1) {
        task->command = -1;
    } else {
        switch (task->command) {
        case 0:
            work->kind = 0x00fffff0;
            WindowDXSet(&work->window);
            work->window.x = -0x110;
            work->window.y = 8;
            work->window.color = work->kind;
            work->window.width = 0x110;
            work->window.height = 32;
            work->window.state = mail_mode;
            WindowDXMain(&work->window);
            work->window.state = 3;

            for (i = 0; i < 5; i++) {
                eMessageSet(&work->messages[i], msg00_0_00A10618[i]);
                work->messages[i].mode = 0x20;
                work->messages[i].x = 0x120;
                work->messages[i].y = 0x0b;
                work->messages[i].color = work->kind + 2;
            }
            work->state = 1;
            break;

        case 2: {
            short slide_targets[8];
            short window_target;

            window_target = -16;
            for (i = 0; i < 5; i++) {
                slide_targets[i] = 288;
            }

            switch (UmnWork.tab_state - 16) {
            case 0:
            case 1:
            case 16:
            case 17:
            case 32:
            case 33:
            case 37:
            case 80:
            case 81:
                slide_targets[0] = 16;
                slide_targets[UmnWork.mail_selection_state * 3 + 1] =
                    (short)(caption_widths[0] + 16);
                break;
            case 48:
            case 49:
                slide_targets[0] = 16;
                slide_targets[2] = (short)(caption_widths[0] + 16);
                break;
            case 64:
            case 65:
            case 66:
            case 67:
            case 68:
            case 69:
                slide_targets[0] = 16;
                slide_targets[3] = (short)(caption_widths[0] + 16);
                break;
            default:
                window_target = -272;
                break;
            }

            if (work->state != 0) {
                MoveSlide(&work->window.x, &window_target, 3.0f);
                WindowDXMain(&work->window);
                work->box.x = work->window.x + 3;
                work->box.y = work->window.y + 3;
                work->box.kind = work->kind;
                work->box.width = work->window.width - 6;
                work->box.height = work->window.height - 6;
                endPrintExtFunc(work->kind, 0x65, &work->box);

                for (i = 0; i < 5; i++) {
                    MoveSlide(&work->messages[i].x, &slide_targets[i], 3.0f);
                    if (work->messages[i].x < 256) {
                        eMessageMain(&work->messages[i]);
                    }
                }
                endPrintExtFunc(work->kind, 0x66, 0);
            }
            break;
        }
        }
    }
}

extern void *memset(void *dest, int value, unsigned int size);
extern void eMessageCpy(char *destination, char *source);
extern void eMessageCat(char *source);
extern char *MenuNumberTextGet(int value, int digits, int mode);
extern unsigned char *UmnMailDataGet(int box_id);
extern signed char *UmnHistoryTreeGet(unsigned int history_id);
extern char *UmnMailHeaderGet(int index);
extern void MenuInfoWindow(void *window, void *text);
extern char D_00A12548[];
extern char D_00A12550[];
extern char D_00A12558[];
typedef struct { char *p[6]; } UmnMailInfoCap;
typedef struct { char *p[4]; } UmnMailInfoTab;
typedef struct { char *p[7]; } UmnMailInfoMsg;
extern UmnMailInfoCap D_00A12408;
extern UmnMailInfoTab D_00A12478;
extern UmnMailInfoMsg D_00A12528;
typedef struct { unsigned char pad[0x60]; int v60; } UmnMailInfoState60;
typedef struct UmnMailInfoWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    unsigned char unmodeled_0c[4];
    unsigned char state;
    unsigned char unmodeled_11[3];
    void (*callback)(void *, void *);
    void *callback_arg;
    unsigned char unmodeled_1c[0x178];
} UmnMailInfoWindow;
typedef struct UmnMailInfoText {
    short x;
    short y;
    unsigned char unmodeled_04[8];
    char *text;
} UmnMailInfoText;
typedef struct UmnMailInfoWork {
    unsigned char state;
    unsigned char opened;
    unsigned char unmodeled_02[6];
    int color;
    UmnMailInfoWindow window;
    UmnMailInfoText info;
    unsigned char unmodeled_1b0[0x350 - 0x1b0];
    char msg[0x100];
} UmnMailInfoWork;

void tskUmnMailInfo(void *task_opaque, void *work_opaque)
{
    UmnMailPasTask *task = task_opaque;
    UmnMailInfoWork *work = work_opaque;
    UmnMailInfoCap cap;
    UmnMailInfoTab tab;
    UmnMailInfoMsg msg;
    unsigned char mail_mode;
    short target;

    cap = D_00A12408;
    tab = D_00A12478;
    msg = D_00A12528;
    mail_mode = UmnWork.mail_mode;
    if (mail_mode != 1) {
        task->command = -1;
        return;
    }
    switch (task->command) {
    case 0:
        work->opened = 0;
        work->color = 0xFF0000;
        work->state = 0;
        memset(work->msg, 0, 0x100);
        WindowDXSet(&work->window);
        work->window.x = -16;
        ((volatile UmnMailInfoWork *)work)->window.state = 0;
        work->window.color = work->color;
        work->window.y = 0x1E0;
        work->window.width = 0x220;
        work->window.height = 0x36;
        work->window.callback = MenuInfoWindow;
        work->window.callback_arg = &work->info;
        work->info.x = 0;
        work->info.y = 0;
        work->info.text = 0;
        ((volatile UmnMailInfoWork *)work)->window.state = mail_mode;
        WindowDXMain(&work->window);
        work->window.state = 3;
        break;
    case 2:
        target = 0x182;
        switch (work->state) {
        case 0:
            work->opened = mail_mode;
            work->state = 2;
        case 2:
            switch (UmnWork.tab_state - 16) {
            case 0:
            case 1:
            case 16:
            case 17: {
                unsigned char flags = *UmnMailDataGet(UmnWork.selected_mail_id);
                if (flags & 8) {
                    int i;
                    int j;
                    char *hdr;
                    for (i = 0; i < 0x51; i++) {
                        signed char *tree = UmnHistoryTreeGet(i);
                        for (j = 0; j < 3; j++) {
                            if (tree[j] == UmnWork.selected_mail_id) {
                                break;
                            }
                        }
                        if (j != 3) {
                            break;
                        }
                    }
                    hdr = UmnMailHeaderGet(i);
                    if (hdr != 0) {
                        eMessageCpy(work->msg, cap.p[4]);
                        eMessageCat(hdr);
                        eMessageCat(D_00A12548);
                        eMessageCat(cap.p[5]);
                        eMessageCat(hdr + 0x40);
                        eMessageCat(D_00A12548);
                    } else {
                        eMessageCpy(work->msg, D_00A12550);
                    }
                } else {
                    if (flags & 2) {
                        eMessageCpy(work->msg, cap.p[0]);
                    } else {
                        eMessageCpy(work->msg, cap.p[1]);
                    }
                    if ((signed char)UmnWork.unmodeled_53[8] != 0) {
                        eMessageCat(cap.p[2]);
                    } else {
                        eMessageCat(cap.p[3]);
                    }
                }
                break;
            }
            case 32:
            case 33:
                eMessageCpy(work->msg, tab.p[(signed char)UmnWork.unmodeled_45[1]]);
                break;
            case 64:
            case 65:
                if (((UmnMailInfoState60 *)&UmnWork)->v60 >= 0) {
                    char *hdr = UmnMailHeaderGet(UmnWork.selected_mail_id);
                    eMessageCpy(work->msg, msg.p[0]);
                    eMessageCat(hdr + 0x40);
                } else {
                    char *hdr = UmnMailHeaderGet(UmnWork.selected_mail_id);
                    int n = -((UmnMailInfoState60 *)&UmnWork)->v60;
                    int num = *(int *)(hdr + 0x8C);
                    eMessageCpy(work->msg, msg.p[4]);
                    eMessageCat(MenuNumberTextGet(num, 8, 1));
                    eMessageCat(msg.p[6]);
                    eMessageCat(D_00A12558);
                    eMessageCat(msg.p[5]);
                    eMessageCat(MenuNumberTextGet(n, 8, 1));
                    eMessageCat(msg.p[6]);
                }
                break;
            case 66:
            case 67:
                eMessageCpy(work->msg, msg.p[1]);
                break;
            case 68:
            case 69:
                eMessageCpy(work->msg, msg.p[2]);
                break;
            case 80:
            case 81:
                eMessageCpy(work->msg, msg.p[3]);
                break;
            default:
                eMessageCpy(work->msg, D_00A12550);
                target = 0x200;
                break;
            }
        }
        work->info.text = work->msg;
        MoveSlide(&work->window.y, &target, 3.0f);
        WindowDXMain(&work->window);
        break;
    }
}

extern void eCursolSet(void *cursol, int value);
extern void eCursolModeChange(void *cursol, int mode);
extern void eCursolMain(void *cursol);
extern void eMessageModeChange(void *message, int mode);
extern void eSpriteSet(void *sprite, short v);
extern void eSpriteMain(void *sprite);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern int UmnTexAddr;
extern unsigned char mail_folder[0x80];
extern unsigned char D_00A126B8[];
extern char D_00A126A8[];
extern char D_00A126C0[];
typedef struct UmnFolderSprite {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int color;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char unmodeled_0f[0x28 - 0x0f];
} UmnFolderSprite;
typedef struct UmnFolderCursor {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int color;
    unsigned char unmodeled_0c[0x24 - 0x0c];
} UmnFolderCursor;
typedef struct UmnFolderData {
    unsigned char flags;
    unsigned char flags2;
} UmnFolderData;
typedef struct UmnFolderItem {
    UmnFolderData data;
    unsigned char unmodeled_02[2];
    int id;
    short x;
    short y;
    struct {
        short x;
        short y;
        int color;
        short width;
        short height;
        unsigned char unmodeled_0c[4];
        unsigned char state;
        unsigned char unmodeled_11[0x194 - 0x11];
    } window;
    UmnFolderSprite spr[4];
    UmnMailPasMessage msg[2];
} UmnFolderItem;
typedef struct UmnFolderWork {
    unsigned char state;
    unsigned char active;
    unsigned char unmodeled_02;
    unsigned char count;
    short x;
    short y;
    int color;
    UmnFolderItem item[6];
    UmnFolderCursor cursor;
    int arrows;
    UmnFolderCursor up;
    UmnFolderCursor down;
    int frame;
    UmnMailPasBox box;
    int tex;
} UmnFolderWork;
static inline unsigned char *folderUmn(void)
{
    return (unsigned char *)&UmnWork;
}
static inline signed char *folderS(void)
{
    return (signed char *)&UmnWork;
}
static inline signed char *folderSL(void)
{
    signed char *m = (signed char *)&UmnWork;
    return m;
}

void tskUmnMailFolder(void *task_opaque, void *work_opaque)
{
    UmnMailPasTask *task = task_opaque;
    UmnFolderWork *work = work_opaque;
    short tgt0;
    short tgt1;
    short tgt2;
    short tgt3;
    short tgt4;
    short tgt5;
    short tgt6;
    short tgt7;
    int i;
    int k;
    int j;
    unsigned char *hdr;
    int extra;
    int idx;
    unsigned char st;

    if (folderUmn()[1] != 1) {
        task->command = -1;
        return;
    }
    switch (task->command) {
    case 0:
        work->x = 0x210;
        work->y = 0x60;
        work->color = 0xFF0000;
        for (i = 0; i < 6; i++) {
            work->item[i].x = 0;
            work->item[i].y = 0;
            WindowDXSet(&work->item[i].window);
            work->item[i].window.x = 0x210;
            work->item[i].window.y = 0;
            work->item[i].window.width = 0x1B8;
            work->item[i].window.height = 0x36;
            work->item[i].window.state = 1;
            WindowDXMain(&work->item[i].window);
            work->item[i].window.state = 3;
        }
        eCursolSet(&work->cursor, 0);
        work->cursor.color = work->color + 2;
        eCursolSet(&work->up, 1);
        eCursolModeChange(&work->up, 0x20);
        work->up.color = work->color + 8;
        eCursolSet(&work->down, 2);
        eCursolModeChange(&work->down, 0x20);
        work->down.color = work->color + 8;
        work->frame = 0;
        work->state = 0;
        work->active = 0;
        break;
    case 2:
        work->count = folderUmn()[0x51];
        if (work->count >= 5) {
            work->count = 4;
        }
        st = work->state;
        switch (st) {
        case 0:
            work->active = 0;
            work->state = 10;
        case 10:
            if (folderUmn()[3] == 0x21) {
                work->active = 1;
                work->state = 0x14;
                for (i = 0; i < work->count; i++) {
                    work->item[i].y = i * 0x40 + 0x60;
                    work->item[i].x = -0x1C0;
                }
                eCursolModeChange(&work->cursor, 0x20);
            } else if (folderUmn()[3] == 0x41) {
                work->active = 1;
                work->state = 0x32;
                work->item[0].x = -0x1C0;
                work->item[0].y = 0x30;
                for (i = 1; i < work->count; i++) {
                    work->item[i].y = i * 0x40 + 0x60;
                    work->item[i].x = -0x1C0;
                }
                eCursolModeChange(&work->cursor, 0x20);
            }
            break;
        case 20: {
            int cnt = 0;
            for (i = 0; i < work->count; i++) {
                tgt0 = 0x20;
                MoveSlide(&work->item[i].x, &tgt0, 3.0f);
                if (work->item[i].x == tgt0) {
                    if (i == folderS()[0x42]) {
                        tgt1 = i * 0x40 + 0x60;
                        MoveSlide(&work->item[i].y, &tgt1, 3.0f);
                        if (work->item[i].y == tgt1) {
                            cnt++;
                        }
                    } else {
                        cnt++;
                    }
                }
            }
            if (cnt == work->count) {
                work->state = 0x1E;
                work->frame = 1;
                folderUmn()[0x11] = 0;
                work->arrows = 1;
            }
            break;
        }
        case 30: {
            int brk = 1;
            unsigned char tab = folderUmn()[3];
            if (tab == 0x31) {
                work->frame = 0;
                work->state = 0x28;
                work->arrows = 0;
            } else if (tab == 0xF1) {
                work->frame = 0;
                work->state = 0x5A;
                work->arrows = 0;
            } else if (tab == 0x11) {
                work->frame = 0;
                work->state = 0x2A;
                work->arrows = 0;
            } else if (folderS()[0x12] != 0) {
                work->state = 0x20;
                folderUmn()[0x11] = 1;
                brk = 0;
            }
            if (brk) {
                break;
            }
        }
        case 32:
            if (folderS()[0x12] == 1) {
                work->item[4].x = work->item[0].x;
                work->item[4].y = work->item[0].y;
            } else {
                work->item[4].x = work->item[3].x;
                work->item[4].y = work->item[3].y;
            }
            for (i = 0; i < work->count; i++) {
                work->item[i].y += folderS()[0x12] * 0x40;
            }
            work->state = 0x22;
        case 34:
            for (i = 0; i < work->count; i++) {
                work->item[i].y -= folderS()[0x12] * 8;
            }
            if (folderS()[0x12] == 1) {
                work->item[4].y = work->item[0].y - 0x40;
                if (work->item[4].y == 0x20) {
                    work->state = 0x1E;
                    folderUmn()[0x12] = 0;
                    folderUmn()[0x11] = 0;
                }
            } else {
                work->item[4].y = work->item[3].y + 0x40;
                if (work->item[4].y == 0x160) {
                    work->state = 0x1E;
                    folderUmn()[0x12] = 0;
                    folderUmn()[0x11] = 0;
                }
            }
            break;
        case 40:
        case 42:
            switch (st) {
            case 40:
                tgt2 = 0x90;
                break;
            case 42:
                tgt2 = 0x210;
                break;
            }
            for (i = 0; i < work->count; i++) {
                MoveSlide(&work->item[i].x, &tgt2, 3.0f);
            }
            if (work->item[folderS()[0x42]].x == tgt2) {
                switch (work->state) {
                case 40:
                    work->state = 0x2D;
                    break;
                case 42:
                    work->state = 10;
                    folderUmn()[0x11] = 0;
                    break;
                }
            }
            break;
        case 45:
            switch (folderUmn()[3]) {
            case 0x11:
                work->state = 0x2A;
                break;
            case 0x21:
            case 0x61:
                work->state = 0x14;
                break;
            case 0x41:
                work->state = 0x32;
                break;
            default:
                folderUmn()[0x11] = 0;
                break;
            }
            break;
        case 50: {
            int cnt = 0;
            int v;
            for (i = 0; i < work->count; i++) {
                if (i == folderS()[0x42]) {
                    tgt3 = 0x30;
                    MoveSlide(&work->item[i].y, &tgt3, 3.0f);
                    if (work->item[i].y == tgt3) {
                        v = 0x10;
                        tgt4 = v;
                        MoveSlide(&work->item[i].x, &tgt4, 3.0f);
                        if (work->item[i].x == tgt4) {
                            cnt++;
                        }
                    }
                } else {
                    v = -0x1C0;
                    tgt5 = v;
                    ((int (*)(short *, short *, float))MoveSlide)(&work->item[i].x, &tgt5, 3.0f);
                    if (work->item[i].x == tgt5) {
                        cnt++;
                    }
                }
            }
            if (cnt == work->count) {
                work->state = 0x3C;
            }
            break;
        }
        case 60:
            switch (folderUmn()[3]) {
            case 0x21:
                work->state = 0x14;
                break;
            case 0x51:
            case 0x80:
                work->state = 0x46;
                break;
            default:
                folderUmn()[0x11] = 0;
                break;
            }
            break;
        case 70:
            tgt6 = -0x1C0;
            MoveSlide(&work->item[folderS()[0x42]].x, &tgt6, 3.0f);
            if (work->item[folderS()[0x42]].x == tgt6) {
                work->state = 0x50;
            }
            break;
        case 80:
            switch (folderUmn()[3]) {
            case 0x41:
                work->state = 0x32;
                break;
            case 0x21:
                work->state = 0x14;
                for (i = 0; i < 4; i++) {
                    work->item[i].y = i * 0x40 + 0x60;
                    work->item[i].x = -0x1C0;
                }
                break;
            }
            break;
        case 90: {
            int cnt = 0;
            for (i = 0; i < work->count; i++) {
                tgt7 = -0x1C0;
                MoveSlide(&work->item[i].x, &tgt7, 3.0f);
                if (work->item[i].x == tgt7) {
                    cnt++;
                }
            }
            if (cnt == work->count) {
                work->state = 0;
                folderUmn()[0x11] = 0;
            }
            break;
        }
        }

        if (work->active != 0) {
            extra = 0;
            if (folderS()[0x12] != 0) {
                extra = 1;
            }
            for (i = 0; i < work->count + extra; i++) {
                int id;
                idx = folderS()[0x50] - folderS()[0x42] + i;
                if (i == 4 && folderS()[0x12] == 1) {
                    idx = folderS()[0x50] - folderS()[0x42] - 1;
                }
                if (idx < 0) {
                    idx += folderS()[0x51];
                } else {
                    idx %= folderS()[0x51];
                }
                id = mail_folder[idx];
                work->item[i].data = *(UmnFolderData *)UmnMailDataGet(id);
                work->item[i].id = id;
            }
            if (work->frame != 0) {
                work->box.x = 0x20;
                work->box.y = 0x60;
                work->box.width = work->item[0].window.width + 0x10;
                work->box.height = work->item[0].window.height + 0xC0;
                work->box.kind = 0x60;
                endPrintExtFunc(work->color, 5, &work->box);
            }
            for (i = 0; i < work->count + extra; i++) {
                work->item[i].window.x = work->item[i].x;
                if (work->item[i].data.flags & 8) {
                    work->item[i].window.x = work->item[i].x + 0x10;
                }
                work->item[i].window.y = work->item[i].y;
                if (i == folderS()[0x50]) {
                    work->item[i].window.color = work->color;
                } else {
                    work->item[i].window.color = work->color - 0x10;
                }
                WindowDXMain(&work->item[i].window);
                xglFontDebugPrintf(work->item[i].x / 2, work->item[i].y / 2, D_00A126A8, work->item[i].id);
            }
            {
                int n = folderS()[0x42];
                work->cursor.x = work->item[n].window.x + 3;
                work->cursor.color = work->item[n].window.color;
                work->cursor.y = work->item[n].window.y + 0x13;
            }
            eCursolMain(&work->cursor);
            if (folderS()[0x51] >= 5 && work->arrows != 0) {
                work->up.x = 0x1DA;
                work->up.y = 0x60;
                work->up.color = work->color;
                eCursolMain(&work->up);
                work->down.x = 0x1DA;
                work->down.y = 0x146;
                work->down.color = work->color;
                eCursolMain(&work->down);
            }
            if (work->frame != 0) {
                endPrintExtFunc(work->color, 6, 0);
            }
            work->tex = UmnTexAddr;
            endPrintExtFunc(0, 0xE, &work->tex);
            if (work->frame != 0) {
                endPrintExtFunc(work->color, 5, &work->box);
                endPrintExtFunc(work->color, 0x65, &work->box);
            }
            for (i = 0; i < work->count + extra; i++) {
                short ids[2];
                int spr;
                hdr = (unsigned char *)UmnMailHeaderGet(work->item[i].id);
                spr = 0x1408;
                if (work->item[i].data.flags & 8) {
                    spr = 0x1409;
                } else if (work->item[i].data.flags & 2) {
                    spr = 0x140A;
                }
                eSpriteSet(&work->item[i].spr[0], spr);
                work->item[i].spr[0].x = work->item[i].window.x + 0x13;
                work->item[i].spr[0].y = work->item[i].window.y + 3;
                work->item[i].spr[0].color = work->item[i].window.color + 1;
                eSpriteMain(&work->item[i].spr[0]);
                __builtin_memcpy(ids, D_00A126B8, 4);
                for (k = 0; k < 2; k++) {
                    eSpriteSet(&work->item[i].spr[k + 1], ids[k]);
                    work->item[i].spr[k + 1].x = work->item[i].spr[0].x + 0x26;
                    work->item[i].spr[k + 1].color = work->item[i].spr[0].color;
                    work->item[i].spr[k + 1].y = work->item[i].spr[0].y + k * 0x18;
                    eSpriteMain(&work->item[i].spr[k + 1]);
                }
                eMessageSet(&work->item[i].msg[0], (char *)hdr + 0x40);
                eMessageSet(&work->item[i].msg[1], (char *)hdr);
                for (j = 0; j < 2; j++) {
                    eMessageModeChange(&work->item[i].msg[j], 0x20);
                    work->item[i].msg[j].x = work->item[i].spr[j + 1].x + 0x14;
                    work->item[i].msg[j].color = work->item[i].spr[j + 1].color + 2;
                    work->item[i].msg[j].y = work->item[i].spr[j + 1].y;
                    eMessageMain(&work->item[i].msg[j]);
                }
                if (hdr[0x86] != 0) {
                    eSpriteSet(&work->item[i].spr[3], 0x1406);
                    if (work->item[i].data.flags2 & 0x80) {
                        work->item[i].spr[3].b = 0x40;
                        work->item[i].spr[3].g = 0x40;
                        work->item[i].spr[3].r = 0x40;
                    }
                } else {
                    eSpriteSet(&work->item[i].spr[3], 0x1407);
                }
                work->item[i].spr[3].x = work->item[i].msg[0].x + 0x146;
                work->item[i].spr[3].y = work->item[i].msg[0].y + 0xE;
                work->item[i].spr[3].color = work->item[i].msg[0].color;
                eSpriteMain(&work->item[i].spr[3]);
            }
            if (work->frame != 0) {
                endPrintExtFunc(work->color, 6, 0);
                endPrintExtFunc(work->color, 0x66, 0);
            }
            endPrintExtFunc(0, 0xF, 0);
        }
        break;
    }
    xglFontDebugPrintf(0, 0x10, D_00A126C0, work->state);
}

extern void eCursolSet(void *cursor, int n);
extern void eCursolMain(void *cursor);
extern void eSpriteSet(void *sprite, short v);
extern void eSpriteMain(void *sprite);
extern int tyaUmlDispParamReset(void *p, int v);
extern int tyaUmlDispMain(void *p);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern void *UmnModelUkn;
extern int UmnTexAddr;
extern unsigned char PadData[0xD0];
extern unsigned char sikaku_1[16];
extern unsigned char D_00A12840[8];
extern const char D_00A12848[];
extern const char D_00A12860[];
static inline unsigned char *dispPad(void) {
    return PadData;
}
static inline unsigned char *dispUmn(void) {
    return (unsigned char *)&UmnWork;
}

void tskUmnMailDisp(void *arg0, void *arg1)
{
    unsigned char *w = arg1;
    int mode;

    mode = UmnWork.mail_mode;
    if (mode != 1) {
        *(int *)((char *)arg0 + 0x10) = -1;
        return;
    }
    switch (*(int *)((char *)arg0 + 0x10)) {
    case 0: {
        unsigned char *win = w + 0x14;
        int ci;
        int n;
        *(short *)(w + 0xA) = 0x70;
        *(int *)(w + 0xC) = 0xF00000;
        *(short *)(w + 0x8) = 0x210;
        WindowDXSet(win);
        *(short *)(w + 0x14) = 0x210;
        *(short *)(w + 0x1C) = 0x1E0;
        *(short *)(w + 0x1E) = 0x12C;
        w[0x24] = mode;
        *(short *)(w + 0x16) = 0;
        WindowDXMain(win);
        w[0x24] = 3;
        ci = 0;
        do {
            int t16 = ci * 0x24;
            int t16b = t16 + 0x1E0;
            n = ci + 1;
            eCursolSet(w + t16 + 0x1EC, n);
            ci = n;
            w[t16b + 0xC] = 0x20;
            *(int *)(win + t16b) = 0xF00010;
        } while (ci < 2);
        tyaUmlDispParamReset(w + 0x1A8, 0);
        *(short *)(w + 0x1B0) = *(short *)(w + 0x8) + 0x706;
        *(short *)(w + 0x1B2) = *(short *)(w + 0xA) + 0x726;
        *(short *)(w + 0x1C2) = 0x30;
        *(short *)(w + 0x1B4) = 0x1DA;
        *(short *)(w + 0x1B6) = 0x120;
        *(unsigned long long *)(w + 0x1B8) = (unsigned int)(*(int *)(w + 0xC) + 2);
        {
            short tmp[4];
            int i;
            ({ typedef struct __attribute__((packed)) { short w[4]; } Mc_; *(Mc_ *)(tmp) = *(Mc_ *)(D_00A12840); });
            for (i = 0; i < 4; i++) {
                eSpriteSet(w + 0x27C + i * 0x28, tmp[i]);
                *(int *)(win + 0x270 + i * 0x28) = 0xF00000;
                w[0x31C + i] = 0;
            }
        }
        eMessageSet(w + 0x234, sikaku_1);
        w[0x235] = 0x20;
        *(int *)(w + 0x23C) = 0xF00000;
        w[1] = 0;
        w[0] = 0;
        break;
    }
    case 2:
        switch (w[0]) {
        case 0x0:
            w[1] = 0;
            w[0] = 0xA;
            /* fallthrough */
        case 0xA:
            if (UmnWork.tab_state == 0x35) {
                w[0] = 0x14;
            }
            break;
        case 0x14: {
            int id;
            if (UmnModelUkn != 0 && ((unsigned char *)UmnModelUkn)[0x11] != 0) {
                *(short *)(w + 0x8) = 0x100;
                *(short *)(w + 0xA) = 0x103;
                *(short *)(w + 0x10) = 6;
                *(short *)(w + 0x12) = 6;
                *(short *)(w + 0x1C) = 6;
                *(short *)(w + 0x1E) = 6;
                tyaUmlDispParamReset(w + 0x1A8, 0);
                id = UmnWork.selected_mail_id;
                *(short *)(w + 0x1B4) = 6;
                *(int *)(w + 0x1A8) = id;
                *(short *)(w + 0x1C2) = 0x30;
                w[0] = 0x1E;
                *(short *)(w + 0x1B6) = 6;
                w[3] = 0;
                w[4] = 0;
                w[5] = 0;
                w[6] = 0;
                *(short *)(w + 0x1E8) = 0;
                w[7] = 0;
            }
            break;
        }
        case 0x1E: {
            short d;
            int t2;
            int t3;
            short sp10;
            short sp12;
            w[1] = 1;
            sp10 = 0xF0;
            sp12 = 0x93;
            MoveSlide((short *)(w + 0x10), &sp10, 3.0f);
            MoveSlide((short *)(w + 0x12), &sp12, 3.0f);
            *(short *)(w + 0x8) = (sp10 - *(short *)(w + 0x10)) + 0x10;
            *(short *)(w + 0xA) = (sp12 - *(short *)(w + 0x12)) + 0x70;
            t3 = *(short *)(w + 0x10) * 2;
            *(short *)(w + 0x1C) = t3;
            t2 = *(short *)(w + 0x12) * 2;
            *(short *)(w + 0x1E) = t2;
            d = sp10 - *(short *)(w + 0x10);
            *(short *)(w + 0x1B4) = t3 - 6;
            *(short *)(w + 0x1B6) = t2 - 6;
            *(short *)(w + 0x1C0) = d;
            if (*(short *)(w + 0x10) == sp10 && *(short *)(w + 0x12) == sp12) {
                w[0] = 0x28;
                w[6] = 1;
                w[7] = 1;
            }
            break;
        }
        case 0x28:
            switch (UmnWork.tab_state) {
            case 0x41: {
                int q;
                short sp14;
                short sp16;
                if (*(signed char *)(w + 5) == 0) {
                    q = (*(short *)(w + 0x1C6) - 0x30) / *(short *)(w + 0x1B6);
                    if (q != 0) {
                        if (*(unsigned short *)(dispPad() + 0x30) & 0x1000) {
                            w[5] = 1;
                        }
                        if (*(unsigned short *)(dispPad() + 0x30) & 0x4000) {
                            w[5] = 2;
                        }
                        if ((*(unsigned short *)(dispPad() + 0x30) & 4) && (signed char)w[3] > 0) {
                            w[5] = 3;
                            w[3] = (signed char)w[3] - 1;
                            w[0x31D] = -6;
                        }
                        if (*(unsigned short *)(dispPad() + 0x30) & 8) {
                            if ((*(short *)(w + 0x1C6) - 0x30) % *(short *)(w + 0x1B6) != 0) {
                                q++;
                            }
                            if ((signed char)w[3] < q - 1) {
                                w[5] = 4;
                                w[3] = w[3] + 1;
                                w[0x31E] = 6;
                            }
                        }
                        if ((*(unsigned short *)(dispPad() + 0x30) & 1) && *(short *)(w + 0x1C2) != 0x30) {
                            w[5] = 5;
                            w[0x31C] = -6;
                        }
                        if ((*(unsigned short *)(dispPad() + 0x30) & 2)
                            && *(short *)(w + 0x1C2) != *(short *)(w + 0x1C6) - *(short *)(w + 0x1B6)) {
                            w[5] = 6;
                            w[0x31F] = 6;
                        }
                        if ((*(unsigned short *)(dispPad() + 0x30) & 0x80) && w[0x1D0] != 0) {
                            w[4] = w[4] + 1;
                            if (w[0x1D0] - 1 < (signed char)w[4] - 1) {
                                w[4] = 1;
                            }
                            w[5] = 7;
                        }
                    }
                }
                switch ((signed char)w[5]) {
                case 1: {
                    int qq;
                    *(short *)(w + 0x1C2) -= 8;
                    if (*(short *)(w + 0x1C2) < 0x30) {
                        *(short *)(w + 0x1C2) = 0x30;
                    }
                    qq = (*(short *)(w + 0x1C2) - 0x30) / *(short *)(w + 0x1B6);
                    w[5] = 0;
                    w[3] = qq;
                    break;
                }
                case 2: {
                    int qq;
                    *(short *)(w + 0x1C2) += 8;
                    if (*(short *)(w + 0x1C2) > *(short *)(w + 0x1C6) - *(short *)(w + 0x1B6)) {
                        *(short *)(w + 0x1C2) = *(short *)(w + 0x1C6) - *(short *)(w + 0x1B6);
                    }
                    qq = (*(short *)(w + 0x1C2) - 0x30) / *(short *)(w + 0x1B6);
                    w[5] = 0;
                    w[3] = qq;
                    break;
                }
                case 3 ... 7:
                    sp14 = *(short *)(w + 0x1C2);
                    switch ((signed char)w[5]) {
                    case 3:
                    case 4:
                        sp16 = (signed char)w[3] * *(short *)(w + 0x1B6) + 0x30;
                        if (sp16 > *(short *)(w + 0x1C6) - *(short *)(w + 0x1B6)) {
                            sp16 = *(short *)(w + 0x1C6) - *(short *)(w + 0x1B6);
                        }
                        break;
                    case 5:
                        sp16 = 0x30;
                        break;
                    case 6:
                        sp16 = *(short *)(w + 0x1C6) - *(short *)(w + 0x1B6);
                        break;
                    case 7: {
                        signed char k = *(signed char *)(w + 4);
                        if (k != 0) {
                            sp16 = *(short *)(w + k * 2 + 0x1D2);
                        }
                        break;
                    }
                    }
                    MoveSlide(&sp14, &sp16, 3.0f);
                    *(short *)(w + 0x1C2) = sp14;
                    if (sp14 == sp16) {
                        if (*(signed char *)(w + 5) != 4) {
                            w[3] = (sp14 - 0x30) / *(short *)(w + 0x1B6);
                        }
                        w[5] = 0;
                    }
                    break;
                }
                break;
            }
            case 0x21:
            case 0x80:
                w[0] = 0x32;
                break;
            case 0x51:
                w[1] = 0;
                w[0] = 0x3C;
                break;
            default:
                dispUmn()[0x11] = 0;
                break;
            }
            break;
        case 0x32: {
            int t2;
            int t3;
            short sp18;
            short sp1A;
            w[6] = 0;
            w[7] = 0;
            sp18 = 3;
            sp1A = 3;
            MoveSlide((short *)(w + 0x10), &sp18, 3.0f);
            MoveSlide((short *)(w + 0x12), &sp1A, 3.0f);
            *(short *)(w + 0x8) = 0x100 - *(short *)(w + 0x10);
            *(short *)(w + 0xA) = 0x103 - *(short *)(w + 0x12);
            t3 = *(short *)(w + 0x10) * 2;
            *(short *)(w + 0x1C) = t3;
            t2 = *(short *)(w + 0x12) * 2;
            *(short *)(w + 0x1B4) = t3 - 6;
            *(short *)(w + 0x1E) = t2;
            *(short *)(w + 0x1B6) = t2 - 6;
            if (*(short *)(w + 0x10) == sp18 && *(short *)(w + 0x12) == sp1A) {
                w[0] = 0;
                w[1] = 0;
            }
            break;
        }
        case 0x3C: {
            int id;
            *(short *)(w + 0x10) = 6;
            *(short *)(w + 0x12) = 6;
            *(short *)(w + 0x1C) = 6;
            *(short *)(w + 0x1E) = 6;
            tyaUmlDispParamReset(w + 0x1A8, 0);
            id = (signed char)UmnWork.unmodeled_53[2];
            *(short *)(w + 0x1C2) = 0x30;
            *(int *)(w + 0x1A8) = id;
            *(short *)(w + 0x1B4) = 6;
            *(short *)(w + 0x1B6) = 6;
            w[2] = UmnWork.unmodeled_53[2];
            w[0] = 0x46;
            w[6] = 0;
            w[7] = 0;
            break;
        }
        case 0x46: {
            short d;
            int t2;
            int t3;
            short sp1C;
            short sp1E;
            w[1] = 1;
            sp1C = 0xF0;
            sp1E = 0x7B;
            MoveSlide((short *)(w + 0x10), &sp1C, 3.0f);
            MoveSlide((short *)(w + 0x12), &sp1E, 3.0f);
            *(short *)(w + 0x8) = (sp1C - *(short *)(w + 0x10)) + 0x10;
            *(short *)(w + 0xA) = (sp1E - *(short *)(w + 0x12)) + 0x80;
            t3 = *(short *)(w + 0x10) * 2;
            *(short *)(w + 0x1C) = t3;
            t2 = *(short *)(w + 0x12) * 2;
            *(short *)(w + 0x1E) = t2;
            d = sp1C - *(short *)(w + 0x10);
            *(short *)(w + 0x1B4) = t3 - 6;
            *(short *)(w + 0x1B6) = t2 - 6;
            *(short *)(w + 0x1C0) = d;
            if (*(short *)(w + 0x10) == sp1C && *(short *)(w + 0x12) == sp1E) {
                w[0] = 0x50;
            }
            if ((signed char)w[2] != (signed char)UmnWork.unmodeled_53[2]) {
                w[1] = 0;
                w[0] = 0x3C;
            }
            break;
        }
        case 0x50:
            switch (UmnWork.tab_state) {
            case 0x21:
                w[0] = 0x32;
                break;
            case 0x41:
                w[1] = 0;
                w[0] = 0x14;
                break;
            default:
                if ((signed char)w[2] != (signed char)UmnWork.unmodeled_53[2]) {
                    w[1] = 0;
                    w[0] = 0x3C;
                }
                break;
            }
            break;
        }
        if (w[1] != 0) {
            int x;
            short sp20;
            unsigned char *p;
            unsigned char *spr;
            unsigned char *pos;
            int kk;
            *(short *)(w + 0x14) = *(short *)(w + 0x8);
            *(short *)(w + 0x16) = *(short *)(w + 0xA);
            *(int *)(w + 0x18) = *(int *)(w + 0xC);
            WindowDXMain(w + 0x14);
            *(short *)(w + 0x1B0) = *(short *)(w + 0x8) + 0x703;
            *(short *)(w + 0x1B2) = *(short *)(w + 0xA) + 0x723;
            *(unsigned long long *)(w + 0x1B8) = (unsigned int)(*(int *)(w + 0xC) + 2);
            tyaUmlDispMain(w + 0x1A8);
            if (*(signed char *)(w + 6) != 0) {
                if (((*(short *)(w + 0x1C6) - 0x30) / *(short *)(w + 0x1B6)) != 0) {
                    if (*(short *)(w + 0x1C2) != 0x30) {
                        *(short *)(w + 0x1F0) = *(short *)(w + 0x8) + *(short *)(w + 0x1C) - 0x11;
                        *(short *)(w + 0x1F2) = *(short *)(w + 0xA) + 4;
                        eCursolMain(w + 0x1EC);
                    }
                    if (*(short *)(w + 0x1C2) < *(short *)(w + 0x1C6) - *(short *)(w + 0x1B6)) {
                        *(short *)(w + 0x214) = *(short *)(w + 0x8) + *(short *)(w + 0x1C) - 0x11;
                        *(short *)(w + 0x216) = *(short *)(w + 0xA) + *(short *)(w + 0x1E) - 0x13;
                        eCursolMain(w + 0x210);
                    }
                }
            }
            sp20 = 0;
            if (*(signed char *)(w + 7) != 0) {
                sp20 = 0x46;
            }
            MoveSlide((short *)(w + 0x1E8), &sp20, 3.0f);
            *(short *)(w + 0x238) = 0x40;
            kk = 0;
            p = w + 0x31C;
            spr = w + 0x27C;
            x = 0x100;
            pos = w + 0x270;
            *(short *)(w + 0x23A) = 0x1E0 - *(short *)(w + 0x1E8);
            eMessageMain(w + 0x234);
            *(int *)(w + 0x278) = UmnTexAddr;
            endPrintExtFunc(0, 0xE, w + 0x278);
            do {
                if (*(signed char *)p != 0) {
                    if (kk < 2) {
                        *p = *p + 1;
                    } else {
                        *p = (signed char)*p - 1;
                    }
                }
                *(short *)(pos + 0x10) = x;
                x += 0x20;
                *(short *)(pos + 0x12) = (signed char)*p - *(short *)(w + 0x1E8) + 0x1E0;
                p++;
                kk++;
                pos += 0x28;
                eSpriteMain(spr);
                spr += 0x28;
            } while (kk < 4);
            endPrintExtFunc(0, 0xF, 0);
        }
        break;
    }
    xglFontDebugPrintf(0x10, 0x80, D_00A12848, (signed char)w[5]);
    xglFontDebugPrintf(0x10, 0x88, D_00A12860, (signed char)w[3]);
}

extern void eCursolSet(void *cursol, int value);
extern void eCursolModeChange(void *cursol, int mode);
extern void eCursolMain(void *cursol);
extern void eMessageModeChange(void *message, int mode);
extern char *UmnMailHeaderGet(int index);
extern char text_2[];
static inline unsigned char *umnHensinLit(void) {
    unsigned char *m = (unsigned char *)&UmnWork;
    return m;
}

void tskUmnMailHensin(UmnMailPasTask *task, void *work)
{
    unsigned char *w = (unsigned char *)work;
    short sp[2];

    int idx;
    unsigned char *msg;
    int *colp;
    unsigned char mail_mode;
    int i;
    mail_mode = UmnWork.mail_mode;
    if (mail_mode != 1) {
        task->command = -1;
        return;
    }
    switch (task->command) {
    case 0:
        *(short *)(w + 0x4) = 0x210;
        *(short *)(w + 0x6) = 0x30;
        *(int *)(w + 0x8) = 0xFF0000;
        WindowDXSet(w + 0xC);
        *(short *)(w + 0xC) = *(short *)(w + 0x4);
        *(short *)(w + 0x14) = 0x12E;
        *(short *)(w + 0x16) = 0x4E;
        w[0x1C] = mail_mode;
        *(short *)(w + 0xE) = *(short *)(w + 0x6);
        WindowDXMain(w + 0xC);
        w[0x1C] = 3;
        eCursolSet(w + 0x2B0, 0);
        w[1] = 0;
        w[0] = 0;
        break;
    case 2:
        switch (w[0]) {
        case 0:
            w[0] = 10;
            w[1] = 0;
            eCursolModeChange(w + 0x2B0, 0x70);
            /* fallthrough */
        case 10:
            if (umnHensinLit()[3] == 0x51) {
                w[0] = 0x14;
                w[1] = 1;
                eCursolModeChange(w + 0x2B0, 0x20);
            }
            break;
        case 20:
            sp[0] = 0x10;
            MoveSlide((short *)(w + 4), &sp[0], 3.0f);
            if (*(short *)(w + 4) == sp[0]) {
                w[0] = 0x1E;
            }
            break;
        case 30:
            if ((umnHensinLit()[3] >> 4) != 5) {
                w[0] = 0x28;
            } else {
                umnHensinLit()[0x11] = 0;
            }
            break;
        case 40:
            sp[1] = 0x210;
            MoveSlide((short *)(w + 4), &sp[1], 3.0f);
            if (*(short *)(w + 4) == sp[1]) {
                w[0] = 0;
            }
            break;
        }
        break;
    }
    if (w[1]) {
        signed char *hdr;
        *(short *)(w + 0xC) = *(short *)(w + 0x4);
        *(short *)(w + 0xE) = *(short *)(w + 0x6);
        *(int *)(w + 0x10) = *(int *)(w + 0x8);
        WindowDXMain(w + 0xC);
        eMessageSet(w + 0x1A0, text_2);
        *(short *)(w + 0x1A4) = *(short *)(w + 0xC) + 0x13;
        *(short *)(w + 0x1A6) = *(short *)(w + 0xE) + 3;
        *(int *)(w + 0x1A8) = *(int *)(w + 0x10) + 2;
        eMessageModeChange(w + 0x1A0, 0x20);
        eMessageMain(w + 0x1A0);
        i = 0;
        if ((signed char)UmnWork.unmodeled_53[1] >= 1) {
            int off;
            int k;
            off = 0;
            k = 0;
            msg = w + 0x1E4;
            do {
                idx = (signed char)UmnWork.unmodeled_53[3 + i];
                eMessageSet(msg, 0x40 + UmnMailHeaderGet(idx));
                *(short *)(msg + 0x4) = *(short *)(w + 0xC) + 0x3B;
                *(short *)(msg + 0x6) = *(short *)(w + 0xE) + off + 3;
                *(int *)(w + 0x1EC + k * 0x44) = *(int *)(w + 0x10) + 2;
                eMessageModeChange(msg, 0x20);
                eMessageMain(msg);
                k++;
                i = i + 1;
                off += 0x18;
                msg += 0x44;
            } while (i < (signed char)UmnWork.unmodeled_53[1]);
        }
        *(short *)(w + 0x2B4) = *(short *)(w + 0xC) + 3;
        *(short *)(w + 0x2B6) =*(short *)(w + 0x1EA + (signed char)UmnWork.unmodeled_53[0] * 0x44) + 4;
        *(int *)(w + 0x2B8) = *(int *)(w + 0x10) + 2;
        eCursolMain(w + 0x2B0);
    }
}

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern int MenuSelectWindow;
extern const char *text2_4[4];
extern int text_3_00A10650[4];
static inline unsigned char *umnBase(void) {
    return (unsigned char *)&UmnWork;
}
static inline unsigned char *umnLit(void) {
    unsigned char *m = (unsigned char *)&UmnWork;
    return m;
}

void tskUmnMailMenu(UmnMailPasTask *arg0, unsigned char *arg1) {
    unsigned char *w = arg1;
    unsigned char *base;
    short sp[4];
    int off;
    int t;
    unsigned char *q;
    const char **p;
    int k;
    int *t3;
    int one;
    unsigned char *pf;

    if (umnBase()[1] != 1) {
        *(int *)((char *)arg0 + 0x10) = -1;
        return;
    }
    switch (*(int *)((char *)arg0 + 0x10)) {
    case 0:
        *(short *)(w + 0x4) = 0x210;
        *(short *)(w + 0x6) = 0x30;
        *(int *)(w + 0x8) = 0xFFFFF0;
        *(int *)(w + 0xC) = 0;
        WindowDXSet(w + 0x10);
        t = *(int *)(w + 0x8);
        *(volatile const char * volatile *)(w + 0x1C) = "Select";
        *(short *)(w + 0x1A) = 0x56;
        t3 = text_3_00A10650;
        *(volatile int *)(w + 0x1AC) = *(volatile int *)&t3[1];
        *(void **)(w + 0x28) = w + 0x1A4;
        *(short *)(w + 0x1A6) = 3;
        *(int *)(w + 0x1B0) = *t3;
        *(int *)(w + 0x14) = t;
        *(short *)(w + 0x18) = 0xA9;
        *(void **)(w + 0x24) = &MenuSelectWindow;
        *(int *)(w + 0x79C) = 0;
        WindowDXSet(w + 0x7A0);
        *(int *)(w + 0x7A4) = *(int *)(w + 0x8);
        *(short *)(w + 0x7A8) = 0x7D;
        *(short *)(w + 0x7AA) = 0x66;
        *(short *)(w + 0x936) = 2;
        *(const char **)(w + 0x7AC) = "Menu";
        *(void **)(w + 0x7B4) = &MenuSelectWindow;
        *(void **)(w + 0x7B8) = w + 0x934;
        w[0] = 0;
        w[1] = 0;
        break;
    case 2:
        switch (w[0]) {
        case 0x0:
            w[1] = 1;
            w[0] = 0xA;
            /* fallthrough */
        case 0xA:
            switch (umnLit()[3]) {
            case 0x31:
                base = w + 0xC;
                off = 0xF20;
                p = text2_4;
                k = 3;
                do {
                    *(const char **)(base + off) = *p;
                    q = w + off;
                    p++;
                    k--;
                    off += 0xC;
                    q[0x14] = 0;
                } while (k >= 0);
                if (UmnWork.mail_selection_state != 0) {
                    w[0xF40] = 1;
                }
                if (((signed char *)umnLit())[0x5B] != 0 && ((signed char *)umnLit())[0x5A] != 0) {
                    w[0xF4C] = 0;
                } else {
                    w[0xF4C] = 1;
                }
                one = 1;
                *(short *)(w + 0x7A0) = (short)(-0x7D);
                *(short *)(w + 0x7A2) = 0xB0;
                *(void **)(w + 0x944) = w + 0xF2C;
                *(void **)(w + 0x79C) = (void *)(unsigned int)one;
                *(unsigned char *)(w + 0x7B0) = one;
                *(int *)(w + 0x938) = 0;
                WindowDXMain(w + 0x7A0);
                w[0x7B0] = 3;
                w[0] = 20;
                break;
            case 0x53:
                *(short *)(w + 0x10) = 0x210;
                *(short *)(w + 0x12) = 0x28;
                {
                    int one53 = 1;
                    *(int *)(w + 0xC) = one53;
                    w[0x20] = one53;
                }
                *(int *)(w + 0x1A8) = 0;
                WindowDXMain(w + 0x10);
                w[0x20] = 3;
                w[0] = 50;
                break;
            }
            break;
        case 0x14:
            sp[0] = 0x10;
            MoveSlide((short *)(w + 0x7A0), sp, 3.0f);
            if (*(short *)(w + 0x7A0) == sp[0]) {
                w[0] = 0x1E;
            }
            break;
        case 0x1E:
            *(int *)(w + 0x938) = (int)(signed char)umnBase()[0x46];
            if (umnLit()[3] != 0x31) {
                w[0] = 0x28;
            } else {
                umnLit()[0x11] = 0;
            }
            break;
        case 0x28:
            sp[1] = -0x7D;
            MoveSlide((short *)(w + 0x7A0), &sp[1], 3.0f);
            if (*(short *)(w + 0x7A0) == sp[1]) {
                w[0] = 0;
                *(int *)(w + 0x79C) = 0;
            }
            break;
        case 0x32:
            sp[2] = (short)(0x1F0 - *(unsigned short *)(w + 0x18));
            MoveSlide((short *)(w + 0x10), &sp[2], 3.0f);
            if (*(short *)(w + 0x10) == sp[2]) {
                w[0] = 60;
            }
            break;
        case 0x3C:
            *(int *)(w + 0x1A8) = (int)(signed char)umnLit()[0x47];
            if (umnLit()[3] != 0x53) {
                w[0] = 0x46;
            } else {
                umnLit()[0x11] = 0;
            }
            break;
        case 0x46:
            sp[3] = 0x210;
            MoveSlide((short *)(w + 0x10), &sp[3], 3.0f);
            if (*(short *)(w + 0x10) == sp[3]) {
                w[0] = 0;
                *(int *)(w + 0xC) = 0;
            }
            break;
        }
        break;
    }
    if (w[1] != 0) {
        if (*(int *)(w + 0xC) != 0) {
            WindowDXMain(w + 0x10);
        }
        if (*(int *)(w + 0x79C) != 0) {
            WindowDXMain(w + 0x7A0);
        }
    }
    xglFontDebugPrintf(0, 0x28, "yn : %2d", w[0]);
}

extern void eRibbonSet(void *ribbon, int mode);
extern void eRibbonMain(void *ribbon);
extern void eTagFontSet(void *tag, void *text);
extern void eTagFontMain(void *tag);
extern void eNumberSet(void *number, int mode);
extern void eNumberMain(void *number);
extern unsigned char *UmnMailDataGet(int box_id);
extern char D_00A12C60[];
extern char *text_5[3];
struct UmnMailExWinMsg {
    short slide_x;
    short slide_y;
    UmnMailPasMessage msg;
};
struct UmnMailExWinItem {
    short slide_x;
    short slide_y;
    struct {
        unsigned char unmodeled_00[4];
        short x;
        short y;
        int value;
        unsigned char unmodeled_0c[0x14];
    } tag;
    struct {
        short x;
        short y;
        int color;
        short width;
        short height;
        unsigned char unmodeled_0c[0x64];
    } ribbon;
    struct {
        short x;
        short y;
        int value;
        unsigned char unmodeled_08[6];
        unsigned char mode;
        unsigned char digits;
        unsigned char unmodeled_10[4];
        int value2;
        unsigned char unmodeled_18[0x78];
    } number;
};
struct UmnMailExWinWork {
    unsigned char state;
    unsigned char opened;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    struct UmnMailExWinMsg msgs[3];
    struct UmnMailExWinItem item[2];
};

void tskUmnMailExWin(void *task_opaque, void *work_opaque)
{
    UmnMailPasTask *task = task_opaque;
    struct UmnMailExWinWork *work = work_opaque;
    int i;

    if (UmnWork.mail_mode != 1) {
        task->command = -1;
        return;
    }
    switch (task->command) {
    case 0:
        work->x = 0x210;
        work->y = 0x20;
        work->color = 0x00FFFFF0;
        for (i = 0; i < 3; i++) {
            work->msgs[i].slide_x = 0x210;
            work->msgs[i].slide_y = 0x10;
            eMessageSet(&work->msgs[i].msg, text_5[i]);
            eMessageModeChange(&work->msgs[i].msg, 0x20);
        }
        for (i = 0; i < 2; i++) {
            work->item[i].slide_x = -0x110;
            work->item[i].slide_y = 0x30;
            eRibbonSet(&work->item[i].ribbon, 3);
            work->item[i].ribbon.width = 0x100;
            work->item[i].ribbon.height = 0x18;
            eTagFontSet(&work->item[i].tag, D_00A12C60);
            eNumberSet(&work->item[i].number, 0);
        }
        work->state = 0;
        work->opened = 0;
        break;
    case 2: {
        short sl_a[8];
        short sl_b[8];
        unsigned char *p;
        unsigned char *q;

        for (i = 0; i < 3; i++) {
            sl_a[i] = 0x210;
            sl_b[i] = -0x110;
        }
        switch (UmnWork.tab_state) {
        case 0x20:
        case 0x21:
        case 0x30:
        case 0x31:
        case 0x60:
        case 0x61:
            sl_b[0] = 0;
            break;
        case 0x40:
        case 0x41:
            p = (unsigned char *)text_5[1] + 1;
            q = (unsigned char *)text_5[1] + 0xF;
            if ((signed char)UmnWork.unmodeled_53[6] != 0) {
                p[2] = 0x80;
                p[1] = 0x80;
                p[0] = 0x80;
            } else {
                p[2] = 0x40;
                p[1] = 0x40;
                p[0] = 0x40;
            }
            if (!(UmnWork.selected_mail_id != 0x1E || (UmnMailDataGet(0x1E)[0] & 0x30))) {
                q[2] = 0x40;
                q[1] = 0x40;
                q[0] = 0x40;
            } else {
                q[2] = 0x80;
                q[1] = 0x80;
                q[0] = 0x80;
            }
            sl_a[1] = 0x110;
            break;
        case 0x50:
        case 0x51: {
            unsigned char *r = (unsigned char *)text_5[2] + 1;
            if (((struct { unsigned char pad[0x60]; int v; } *)&UmnWork)->v >= 0) {
                r[2] = 0x80;
                r[1] = 0x80;
                r[0] = 0x80;
            } else {
                r[2] = 0x40;
                r[1] = 0x40;
                r[0] = 0x40;
            }
        }
            sl_a[2] = 0x110;
            break;
        }
        for (i = 0; i < 3; i++) {
            MoveSlide(&work->msgs[i].slide_x, &sl_a[i], 3.0f);
            work->msgs[i].msg.x = work->msgs[i].slide_x;
            work->msgs[i].msg.y = work->msgs[i].slide_y;
            work->msgs[i].msg.color = work->color;
            eMessageMain(&work->msgs[i].msg);
        }
        for (i = 0; i < 2; i++) {
            MoveSlide(&work->item[i].slide_x, &sl_b[i], 3.0f);
            work->item[i].ribbon.x = work->item[i].slide_x;
            work->item[i].ribbon.y = work->item[i].slide_y;
            work->item[i].ribbon.color = work->color;
            eRibbonMain(&work->item[i].ribbon);
            work->item[i].tag.x = work->item[i].slide_x + 0xBC;
            work->item[i].tag.y = work->item[i].slide_y + 4;
            work->item[i].tag.value = work->item[i].ribbon.color + 1;
            eTagFontMain(&work->item[i].tag);
            work->item[i].number.x = work->item[i].tag.x + 0x1E;
            work->item[i].number.y = work->item[i].tag.y;
            work->item[i].number.value = work->item[i].tag.value;
            work->item[i].number.mode = 3;
            work->item[i].number.value2 = (UmnWork.folder_entry_count << 16) + (signed char)UmnWork.selection_index + 1;
            work->item[i].number.digits = 2;
            eNumberMain(&work->item[i].number);
        }
        break;
    }
    }
}

extern signed char *UmnHistoryTreeGet(unsigned int history_id);
extern unsigned char *UmnMailDataGet(int box_id);
extern signed char D_4A1A0C;
extern unsigned char mail_folder[0x80];

void UmnMailFolderSet(void)
{
    int SisoSiso(int id)
    {
        int i = 0;
        signed char *tree = UmnHistoryTreeGet(id);
        UmnMailState *w = &UmnWork;
        if (tree == 0) return 0;
        do {
            int c = tree[i++];
            if (c < 0) return 0;
            if (c == w->selected_mail_id || SisoSiso(c)) return 1;
        } while (i < 3);
        return 0;
    }
    int id;
    int i;
    unsigned char *base;
    const int three = 3;
    signed char *tree;
    unsigned char *dst;

    if (UmnWork.mail_selection_state != 0) {
        dst = &mail_folder[0];
        id = (signed char)UmnWork.selected_mail_id;
        UmnWork.saved_selection_index = UmnWork.selection_index;
        UmnWork.saved_scroll_offset = UmnWork.scroll_offset;
        UmnWork.saved_visible_folder_index = UmnWork.visible_folder_index;
        i = 0;
        while (i < 0x51) {
            if (SisoSiso(i) != 0) {
                id = i;
                break;
            }
            i++;
        }
        *dst++ = id;
        UmnWork.folder_entry_count = 1;
        tree = UmnHistoryTreeGet(id);
        if (tree != 0) {
            signed char *p;
            do {
                for (i = 0; i < three; i++) {
                    p = tree + i;
                    if (*p > -1) {
                        if (*UmnMailDataGet(*p) & 1) {
                            UmnWork.folder_entry_count += 1;
                            *dst++ = *(unsigned char *)(tree + i);
                            tree = UmnHistoryTreeGet((unsigned int)*p);
                            break;
                        }
                    }
                }
                if (i == three) break;
            } while (tree != 0);
        }
        i = 0;
        if (UmnWork.folder_entry_count > 0) {
            base = mail_folder;
            if (base[0] != UmnWork.selected_mail_id) {
                do {
                    ++i;
                } while (i < UmnWork.folder_entry_count && *(unsigned char *)((unsigned)i + (unsigned)base) != UmnWork.selected_mail_id);
            }
        }
        UmnWork.selection_index = (unsigned char)i;
        if (3 < i) {
            UmnWork.visible_folder_index = 3;
            UmnWork.scroll_offset = i - three;
        } else {
            UmnWork.visible_folder_index = (unsigned char)i;
        }
        return;
    }
    {
        signed char *src;
        int k;
        src = (signed char *)&D_4A1A0C;
        UmnWork.folder_entry_count = 0;
        while (*src != -1) {
            src++;
            UmnWork.folder_entry_count++;
        }
        for (k = 0; k < UmnWork.folder_entry_count; k++) {
            mail_folder[k] = *--src;
        }
    }
    UmnWork.scroll_offset = UmnWork.saved_scroll_offset;
    UmnWork.visible_folder_index = UmnWork.saved_visible_folder_index;
    UmnWork.selection_index = UmnWork.saved_selection_index;
    UmnWork.selected_mail_id = mail_folder[(signed char)UmnWork.saved_selection_index];
}

extern int UmnWorkEnd;
extern unsigned char PadData[];
extern void UmnObjectTaskCreate(void (*task)(void *, void *), void *arg);
extern int UmnEventTextInit(int x);
extern void UmnChangeTopLevel(int level);
extern int UmnMailEventCheck(int id);
extern int UmnMailCompulsionDownLoadCheck(int id);
extern void UmnMailAttachSet(int kind);
extern void UmnKosmosSpecialGetCheck(int id);
extern int UmnManzaiControl(void);
extern int UmnManzaiInit(int arg);
extern int UmnEventEndMailCheck(int id);
extern void UmnMailBoxSet(int box);
extern int dataMoneyBoxChk(void);
extern int dataMoneyBoxDec(int amount);
extern int MenuScenarioNoGet(void);
extern signed char MenuSelectMove(int cursor, int count, int wrap);
extern void xglSoundEffectNormalID(int sound_id, int variant);
extern void tskUmnMailInfo(void *, void *);
extern void tskUmnMailFolder(void *, void *);
extern void tskUmnMailExWin(void *, void *);
extern void tskUmnMailDisp(void *, void *);
typedef struct { signed char v[2]; } UmnRange;
static inline unsigned char *padBase(void) {
    return PadData;
}
static inline signed char *umnS(void) {
    return (signed char *)&UmnWork;
}

void UmnMail(int menuId)
{
    UmnRange sa, t1, sbuf, t2;
    int c;
    signed char var;
    int t;
    int i;
    int j;
    int k;
    int retA, retB, retC;
    int ev;
    unsigned short pad;
    unsigned short padB;
    unsigned char *hdrA, *hdrB, *hdrC, *hdrD, *hdrE, *hdrG, *hdrJ;
    unsigned char *dataA, *dataB, *dataD, *dataE, *dataF, *dataH, *dataI, *dataJ;
    unsigned char *p7;
    unsigned char *dd;
    unsigned char vB;
    int vD;
    int money;
    int w;

    switch (umnBase()[3]) {
    case 0x00:
        w = UmnWorkEnd;
        w = (w + 0xF) & ~0xF;
        UmnObjectTaskCreate((void (*)(void *, void *))tskUmnMailPas, (void *)w);
        w = (w + 0x3D7) & ~0xF;
        UmnObjectTaskCreate(tskUmnMailInfo, (void *)w);
        w = (w + 0x45F) & ~0xF;
        UmnObjectTaskCreate(tskUmnMailFolder, (void *)w);
        w = (w + 0x114F) & ~0xF;
        UmnObjectTaskCreate((void (*)(void *, void *))tskUmnMailHensin, (void *)w);
        w = (w + 0x2E3) & ~0xF;
        UmnObjectTaskCreate((void (*)(void *, void *))tskUmnMailMenu, (void *)w);
        w = (w + 0xF77) & ~0xF;
        UmnObjectTaskCreate(tskUmnMailExWin, (void *)w);
        w = (w + 0x33B) & ~0xF;
        UmnObjectTaskCreate(tskUmnMailDisp, (void *)w);
        UmnEventTextInit(w + 0x320);
        if (menuId == 1) {
            umnBase()[3] = 0x35;
            umnBase()[0x10] = 2;
        } else {
            umnBase()[3] = 0x20;
        }
        UmnMailFolderSet();
        if (umnS()[0x51] == 0) {
            UmnChangeTopLevel(0);
            return;
        }
        break;

    case 0x10:
        umnBase()[0x11] = 1;
        umnBase()[3] = 0x11;
        /* fallthrough */
    case 0x11:
        if (umnS()[0x11] == 0) {
            umnBase()[3] = 0x20;
            umnBase()[0x5C] ^= 1;
        }
        break;

    case 0x20:
        umnBase()[3] = 0x21;
        umnBase()[0x11] = 1;
        umnBase()[0x10] = 0;
        umnBase()[0x12] = 0;
        UmnMailFolderSet();
        /* fallthrough */
    case 0x21:
        if (umnS()[0x11] == 0 && umnS()[0x10] == 0) {
            var = 0;
            pad = *(unsigned short *)(padBase() +0x30);
            if (pad & 0x1000) {
                if (umnS()[0x50] == 0) {
                    if (*(unsigned short *)(padBase() +0x32) & 0x1000) {
                        var = -1;
                    }
                } else if (umnS()[0x42] == 0) {
                    var = -1;
                } else if (*(unsigned short *)(padBase() +0x34) & 0x1000) {
                    var = -1;
                }
            } else if (pad & 0x4000) {
                if (umnS()[0x50] == umnS()[0x51] - 1) {
                    if (*(unsigned short *)(padBase() +0x32) & 0x4000) {
                        var = 1;
                    }
                } else if (umnS()[0x42] == 3) {
                    var = 1;
                } else if (*(unsigned short *)(padBase() +0x34) & 0x4000) {
                    var = 1;
                }
            }

            if (var != 0) {
                t = ((-var) >> 1) & 1;
                c = *(volatile unsigned char *)&umnBase()[0x51] - 1;
                *(volatile signed char *)&t1.v[0] = 0;
                t1.v[1] = c;
                sa = t1;
                t2.v[1] = c;
                t2.v[0] = 0;
                sbuf = t2;
                if (sbuf.v[1] >= 4) {
                    sbuf.v[1] = 3;
                }
                umnBase()[0x10] = 4;
                if (umnS()[0x50] == sa.v[t]) {
                    umnBase()[0x50] = sa.v[t ^ 1];
                } else {
                    umnS()[0x50] += var;
                }
                if (umnS()[0x42] == sbuf.v[t]) {
                    if (umnS()[0x51] >= 5) {
                        umnBase()[0x12] = var;
                        umnS()[0x44] += var;
                    } else {
                        umnBase()[0x42] = sa.v[t ^ 1];
                    }
                } else {
                    umnS()[0x42] += var;
                }
                xglSoundEffectNormalID(3, 0);
            } else {
                if (*(unsigned short *)(padBase() +0x2A) == 0x20) {
                    umnBase()[3] = 0x30;
                    umnBase()[0x46] = 0;
                    xglSoundEffectNormalID(1, 0);
                }
                if (*(unsigned short *)(padBase() +0x2A) == 0x40) {
                    if (umnS()[0x5C] != 0) {
                        umnBase()[3] = 0x10;
                        xglSoundEffectNormalID(1, 0);
                    } else {
                        umnBase()[3] = 0xF0;
                        xglSoundEffectNormalID(2, 0);
                    }
                }
            }
        }
        umnBase()[0x5A] = 0;
        umnBase()[0x5B] = 0;
        umnBase()[0x52] = mail_folder[umnS()[0x50]];
        hdrA = (unsigned char *)UmnMailHeaderGet(umnS()[0x52]);
        if (hdrA[0x86] != 0) {
            dataA = UmnMailDataGet(umnS()[0x52]);
            umnBase()[0x5B] = 1;
            if (!(dataA[1] & 0x80)) {
                umnBase()[0x5A] = 1;
            }
        }
        break;

    case 0x30:
        umnBase()[3] = 0x31;
        umnBase()[0x11] = 1;
        /* fallthrough */
    case 0x31:
        if (umnS()[0x11] == 0) {
            retA = MenuSelectMove(umnS()[0x46], 4, 0);
            umnBase()[0x46] = retA;
            if (*(unsigned short *)(padBase() +0x34) == 0x20) {
                switch ((signed char)retA) {
                case 0:
                    umnBase()[3] = 0x35;
                    umnBase()[0x10] = 2;
                    xglSoundEffectNormalID(1, 0);
                    break;
                case 1:
                    if (umnS()[0x5C] == 0) {
                        umnBase()[3] = 0x10;
                        xglSoundEffectNormalID(1, 0);
                    } else {
                        xglSoundEffectNormalID(5, 0);
                    }
                    break;
                case 3:
                    umnBase()[3] = 0x21;
                    umnBase()[0x10] = 8;
                    umnBase()[0x11] = 1;
                    xglSoundEffectNormalID(2, 0);
                    break;
                case 2:
                    if (umnS()[0x5B] != 0 && umnS()[0x5A] != 0) {
                        umnBase()[3] = 0x60;
                        xglSoundEffectNormalID(1, 0);
                    } else {
                        xglSoundEffectNormalID(5, 0);
                    }
                    break;
                }
            }
            if (*(unsigned short *)(padBase() +0x34) == 0x40) {
                umnBase()[3] = 0x21;
                umnBase()[0x10] = 8;
                umnBase()[0x11] = 1;
                xglSoundEffectNormalID(2, 0);
            }
        }
        break;

    case 0x35:
        if (umnS()[0x10] == 0) {
            umnBase()[3] = 0x40;
            if (menuId == 1) {
                umnBase()[0x10] = 8;
            }
            break;
        }
        goto dec;

    case 0x40:
        umnBase()[3] = 0x41;
        umnBase()[0x11] = 1;
        hdrB = (unsigned char *)UmnMailHeaderGet(umnS()[0x52]);
        umnBase()[0x54] = 0;
        for (i = 0; i < 3; i++) {
            vB = hdrB[0x80 + i];
            if (vB != 0) {
                umnBase()[0x56 + i] = vB;
                umnBase()[0x54]++;
            } else {
                umnBase()[0x56 + i] = -1;
            }
        }
        umnBase()[0x55] = umnBase()[0x56];
        umnBase()[0x53] = 0;
        if (UmnMailEventCheck(umnS()[0x52]) != 0) {
            umnBase()[0x54] = 0;
        }
        dataB = UmnMailDataGet(umnS()[0x52]);
        if (!(dataB[0] & 2)) {
            hdrC = (unsigned char *)UmnMailHeaderGet(umnS()[0x52]);
            dd = (unsigned char *)&D_4A1A0C;
            p7 = hdrC + 7;
            for (j = 0; j < 5; j++) {
                k = j + 0x80;
                if (p7[k] != 0) {
                    dd[k] += p7[k];
                }
            }
            UmnKosmosSpecialGetCheck(umnS()[0x52]);
        }
        dataD = UmnMailDataGet(umnS()[0x52]);
        hdrD = (unsigned char *)UmnMailHeaderGet(umnS()[0x52]);
        dataD[0] |= 2;
        umnBase()[0x59] = 0;
        vD = dataD[0];
        if (!(vD & 8) && umnS()[0x54] != 0 && !(vD & 0x30)) {
            vD = *(unsigned short *)(hdrD + 0x84);
            if (vD > MenuScenarioNoGet()) {
                umnBase()[0x59] = 1;
            }
        }
        hdrE = (unsigned char *)UmnMailHeaderGet(umnS()[0x52]);
        dataE = UmnMailDataGet(umnS()[0x52]);
        dataE[1] |= hdrE[0x86];
        if (UmnMailCompulsionDownLoadCheck(umnS()[0x52]) == 1) {
            dataE[1] |= 0x80;
            UmnMailAttachSet(hdrE[0x86]);
        }
        /* fallthrough */
    case 0x41:
        if (*(unsigned short *)(umnBase() + 0x10) == 0) {
            if (*(unsigned short *)(padBase() +0x2A) == 0x40) {
                ev = UmnMailEventCheck(umnS()[0x52]);
                if (ev != 0) {
                    dataF = UmnMailDataGet(umnS()[0x52]);
                    dataF[0] |= 4;
                    if (UmnManzaiInit(ev) < 0) {
                        UmnManzaiInit(0);
                    }
                    umnBase()[3] = 0x80;
                    xglSoundEffectNormalID(1, 0);
                } else if (umnS()[0x52] == 0x1E) {
                    if (UmnMailDataGet(0x1E)[0] & 0x30) {
                        umnBase()[3] = 0x21;
                        umnBase()[0x11] = 1;
                        xglSoundEffectNormalID(2, 0);
                    } else {
                        xglSoundEffectNormalID(5, 0);
                    }
                } else {
                    umnBase()[3] = 0x21;
                    umnBase()[0x11] = 1;
                    xglSoundEffectNormalID(2, 0);
                }
            }
            if (*(unsigned short *)(padBase() +0x2A) == 0x20) {
                if (umnS()[0x59] != 0) {
                    umnBase()[3] = 0x50;
                    xglSoundEffectNormalID(1, 0);
                } else {
                    xglSoundEffectNormalID(5, 0);
                }
            }
        }
        break;

    case 0x50:
        hdrG = (unsigned char *)UmnMailHeaderGet(umnS()[0x52]);
        *(int *)(umnBase() + 0x60) = *(int *)(hdrG + 0x8C);
        if (dataMoneyBoxChk() < *(int *)(hdrG + 0x8C)) {
            *(int *)(umnBase() + 0x60) = dataMoneyBoxChk() - *(int *)(hdrG + 0x8C);
        }
        umnBase()[0x11] = 1;
        umnBase()[0x10] = 0x10;
        umnBase()[0x53] = 0;
        umnBase()[3] = 0x51;
        /* fallthrough */
    case 0x51:
        if (*(unsigned short *)(umnBase() + 0x10) == 0) {
            retB = MenuSelectMove(umnS()[0x53], umnS()[0x54], 0);
            if (retB != umnS()[0x53]) {
                umnBase()[0x53] = retB;
                umnBase()[0x10] = 8;
            } else {
                padB = *(unsigned short *)(PadData + 0x2A);
                if (padB & 0x20) {
                    if (*(int *)(umnBase() + 0x60) >= 0) {
                        umnBase()[3] = 0x52;
                        xglSoundEffectNormalID(1, 0);
                    } else {
                        xglSoundEffectNormalID(5, 0);
                    }
                } else if (padB & 0x40) {
                    umnBase()[3] = 0x41;
                    xglSoundEffectNormalID(2, 0);
                }
            }
        }
        umnBase()[0x55] = umnBase()[0x56 + umnS()[0x53]];
        break;

    case 0x52:
        if (umnS()[0x5C] == 0) {
            umnBase()[0x5F] = 0;
            umnBase()[0x5E] = 0;
            umnBase()[0x5D] = 0;
        }
        umnBase()[0x47] = 0;
        umnBase()[3] = 0x53;
        umnBase()[0x11] = 1;
        /* fallthrough */
    case 0x53:
        if (umnS()[0x11] == 0) {
            retC = MenuSelectMove(umnS()[0x47], 2, 0);
            umnBase()[0x47] = retC;
            if (*(unsigned short *)(padBase() +0x2A) == 0x20) {
                if ((signed char)retC != 0) {
                    umnBase()[3] = 0x51;
                    xglSoundEffectNormalID(2, 0);
                } else {
                    umnBase()[3] = 0x54;
                    xglSoundEffectNormalID(1, 0);
                }
            }
            if (*(unsigned short *)(padBase() +0x2A) == 0x40) {
                umnBase()[3] = 0x51;
                xglSoundEffectNormalID(2, 0);
            }
        }
        break;

    case 0x54:
        dataH = UmnMailDataGet(umnS()[0x52]);
        dataH[0] |= (umnBase()[0x53] + 1) << 4;
        dataI = UmnMailDataGet(umnS()[0x55]);
        dataI[0] |= 9;
        UmnMailBoxSet(umnS()[0x55]);
        money = *(int *)(umnBase() + 0x60);
        if (money > 0) {
            dataMoneyBoxDec(money);
        }
        umnBase()[3] = 0x55;
        umnBase()[0x10] = 2;
        /* fallthrough */
    case 0x55:
        if (umnS()[0x10] != 0) {
            goto dec;
        }
        if (*(unsigned short *)(PadData + 0x2A) == 0x20) {
            umnBase()[3] = 0x20;
            xglSoundEffectNormalID(1, 0);
        }
        break;

    case 0x60:
        hdrJ = (unsigned char *)UmnMailHeaderGet(umnS()[0x52]);
        dataJ = UmnMailDataGet(umnS()[0x52]);
        dataJ[1] |= 0x80;
        dataJ[1] |= hdrJ[0x86];
        UmnMailAttachSet(hdrJ[0x86]);
        umnBase()[0x10] = 2;
        umnBase()[3] = 0x61;
        /* fallthrough */
    case 0x61:
        if (umnS()[0x10] != 0) {
            goto dec;
        }
        if (*(unsigned short *)(PadData + 0x2A) == 0x20) {
            umnBase()[3] = 0x21;
            xglSoundEffectNormalID(1, 0);
        }
        break;

    case 0x80:
        if (UmnManzaiControl() == 0) {
            umnBase()[3] = 0x35;
            if (UmnEventEndMailCheck(umnS()[0x52]) != 0) {
                UmnMailFolderSet();
            }
        }
        break;

    case 0xF0:
        umnBase()[3] = 0xF1;
        umnBase()[0x11] = 1;
        /* fallthrough */
    case 0xF1:
        if (umnS()[0x11] == 0) {
            UmnChangeTopLevel(0);
        }
        break;
    }
    if (umnS()[0x10] != 0) {
dec:
        umnS()[0x10]--;
    }
    xglFontDebugPrintf(0, 0x50, "%2x", umnBase()[3]);
    xglFontDebugPrintf(0, 0x58, "%2d/%2d", umnS()[0x50], umnS()[0x51]);
    xglFontDebugPrintf(0, 0x60, "%2d", umnS()[0x12]);
}

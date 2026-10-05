/*
 * OV02 original TU 11: 0x00a0baf8..0x00a0c488 (4 functions)
 */
#include "common.h"
#include "shared.h"

typedef struct UmnTaskCommand {
    unsigned char unmodeled_00[0x10];
    int command;
} UmnTaskCommand;
typedef struct UmnWindow {
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
} UmnWindow;
typedef struct UmnMessageControl {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    unsigned char unmodeled_0c[0x44 - 0x0c];
} UmnMessageControl;
typedef struct UmnMessage {
    unsigned char unmodeled_00[0x0c];
    UmnMessageControl control;
} UmnMessage;
typedef struct UmnBox {
    short x;
    short y;
    int kind;
    short width;
    short height;
} UmnBox;
typedef struct UmnSimulationPasWork {
    unsigned char unmodeled_00;
    unsigned char state;
    unsigned char unmodeled_02[2];
    int kind;
    UmnWindow window;
    unsigned char unmodeled_24[0x190 - 0x24];
    UmnMessage message;
    UmnBox box;
} UmnSimulationPasWork;
typedef struct UmnSlideTargets {
    short window;
    short message;
} UmnSlideTargets;
typedef struct UmnSharedWork {
    unsigned char unmodeled_00;
    unsigned char simulation_state;
    unsigned char unmodeled_02;
    unsigned char tab_state;
    unsigned char unmodeled_04[0x10 - 4];
    signed char countdown;
    unsigned char unmodeled_11[0x50 - 0x11];
    int list_index;
    int list_count;
    int pending_script;
} UmnSharedWork;
extern UmnSharedWork UmnWork;
extern const char *msg00_0_00A10888;
extern void WindowDXSet(void *window);
extern void WindowDXMain(void *window);
extern void MoveSlide(short *current, short *target, float rate);
extern void eMessageSet(void *message, const char *text);
extern void eMessageMain(void *message);
extern void endPrintExtFunc(int kind, int id, void *data);

void tskUmnSimulationPas(UmnTaskCommand *task, UmnSimulationPasWork *work)
{
    unsigned char simulation_state = UmnWork.simulation_state;

    if (simulation_state != 3) {
        task->command = -1;
    } else {
        switch (task->command) {
        case 0: {
            UmnMessage *message;
            int window_height = 32;
            unsigned char opening_state = 1;
            int message_x = 0x120;
            int message_y = 0x0b;

            work->kind = 0x00fffff0;
            WindowDXSet(&work->window);
            work->window.x = -0x110;
            work->window.y = 8;
            work->window.color = work->kind;
            work->window.width = 312;
            work->window.height = window_height;
            work->window.state = opening_state;
            WindowDXMain(&work->window);
            work->window.state = simulation_state;
            eMessageSet(&work->message.control, msg00_0_00A10888);
            message = &work->message;
            message->control.mode = 0x20;
            message->control.x = message_x;
            message->control.y = message_y;
            work->state = opening_state;
            work->message.control.color = work->kind + 2;
            break;
        }

        case 2: {
            UmnSlideTargets targets;
            short *message_target;
            int tab_state = UmnWork.tab_state;

            targets.window = -0x10;
            message_target = &targets.message;
            *message_target = 288;
            if (tab_state < 18) {
                if (tab_state >= 16) {
                    targets.message = 16;
                } else {
                    targets.window = -312;
                }
            } else {
                targets.window = -312;
            }

            if (work->state != 0) {
                MoveSlide(&work->window.x, &targets.window, 3.0f);
                WindowDXMain(&work->window);
                work->box.x = work->window.x + 3;
                work->box.y = work->window.y + 3;
                work->box.kind = work->kind;
                work->box.width = work->window.width - 6;
                work->box.height = work->window.height - 6;
                endPrintExtFunc(work->kind, 101, &work->box);

                MoveSlide(&work->message.control.x, message_target, 3.0f);
                if (work->message.control.x < 256) {
                    eMessageMain(&work->message.control);
                }
                endPrintExtFunc(work->kind, 102, 0);
            }
            break;
        }
        }
    }
}

typedef struct UmnSimInfoText {
    short x;
    short y;
    unsigned char unmodeled_04[0x0C - 0x04];
    char *text;
} UmnSimInfoText;
typedef struct UmnSimInfoWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    unsigned char unmodeled_0c[4];
    unsigned char state;
    unsigned char unmodeled_11[3];
    void *callback;
    void *callbackArg;
    unsigned char unmodeled_1c[0x194 - 0x1C];
} UmnSimInfoWindow;
typedef struct UmnSimInfoStr {
    long head;
    short tail;
} UmnSimInfoStr;
typedef struct UmnSimInfoWork {
    unsigned char state;
    unsigned char opened;
    unsigned char unmodeled_02[0x08 - 0x02];
    int color;
    UmnSimInfoWindow window;
    UmnSimInfoText info;
    unsigned char unmodeled_1b0[0x350 - 0x1B0];
    union {
        char buf[0x40];
        UmnSimInfoStr head;
    } str;
} UmnSimInfoWork;
extern void MenuInfoWindow(void);
extern char D_00A13218;
extern const char D_00A13220[] __attribute__((aligned(8)));
extern char D_00A13230[];
extern char *ListText[];
extern char *strcat(char *, const char *);

void tskUmnSimulationInfo(UmnTaskCommand *task, void *arg)
{
    UmnSimInfoWork *work = arg;
    unsigned char simulation_state = UmnWork.simulation_state;
    short slideTarget;
    UmnSimInfoText *info;
    int tab;
    char *buf;

    if (simulation_state != 3) {
        task->command = -1;
        return;
    }

    switch (task->command) {
    case 0:
        work->color = 0x00FF0000;
        work->str.buf[0] = D_00A13218;
        work->opened = 0;
        work->state = 0;
        WindowDXSet(&work->window);
        work->window.x = -16;
        work->window.state = 0;
        work->window.y = 480;
        work->window.width = 544;
        work->window.height = 54;
        work->window.callbackArg = &work->info;
        work->window.color = work->color;
        work->info.x = 0;
        work->window.callback = MenuInfoWindow;
        work->window.state = 1;
        work->info.y = 0;
        work->info.text = 0;
        WindowDXMain(&work->window);
        work->window.state = simulation_state;
        break;
    case 2:
        slideTarget = 386;
        if (work->state != 0) {
            if (work->state != 2) {
                goto slide;
            }
        } else {
            work->state = 2;
            work->opened = 1;
        }
        tab = UmnWork.tab_state;
        if (tab < 0x12) {
            if (tab >= 0x10) {
                if (UmnWork.list_index >= 0) {
                    __builtin_memcpy(&work->str.head, D_00A13220, 10);
                    strcat(work->str.buf, ListText[UmnWork.list_index]);
                    strcat(work->str.buf, D_00A13230);
                }
            } else {
                slideTarget = 512;
            }
        } else {
            slideTarget = 512;
        }
    slide:
        work->info.text = work->str.buf;
        MoveSlide(&work->window.y, &slideTarget, 3.0f);
        WindowDXMain(&work->window);
        break;
    }
}

typedef struct UmnGameLoopState {
    unsigned char unmodeled_00[0x10];
    unsigned int flags;
} UmnGameLoopState;

extern UmnGameLoopState GameLoopState;

extern PadPrefix PadData;

extern const char D_00A13238[];
extern void WindowSPSet(void *window);
extern void WindowSPMain(void *window);
extern int WindowSPSelect(void *window, int pad);
typedef struct UmnSimListItem {
    char *text;
    int value;
    unsigned char flag;
} UmnSimListItem;
typedef struct UmnSimListWindow {
    unsigned char type;
    unsigned char mode;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    short width;
    short height;
    const char *title;
    unsigned char rows;
    unsigned char cols;
    unsigned char unmodeled_22[6];
    UmnSimListItem *items;
} UmnSimListWindow;
typedef struct UmnSimListWork {
    unsigned char state;
    unsigned char opened;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    UmnSimListWindow window;
    unsigned char unmodeled_2c[0x1718];
    UmnSimListItem item[17];
} UmnSimListWork;

void tskUmnSimulationList(UmnTaskCommand *task, void *arg)
{
    UmnSimListWork *work = arg;
    short target;

    if (UmnWork.simulation_state != 3) {
        task->command = -1;
        return;
    }
    switch (task->command) {
    case 0:
        {
            int i;
            ((union { short s; int i; } *)&work->x)->s = 0x210;
            ((union { short s; int i; } *)&work->y)->s = 0x70;
            work->color = 0xFFFF00;
            for (i = 0; i < UmnWork.list_count; i++) {
                work->item[i].text = ListText[i];
                if (GameLoopState.flags & 0x400000) {
                    work->item[i].flag = 0;
                } else {
                    work->item[i].flag = 1;
                }
                work->item[i].value = 0;
            }
            work->item[i].text = &D_00A13218;
            work->item[i].value = 0;
            work->item[i].flag = 0;
            work->item[i + 1].text = 0;
            work->window.x = work->x;
            work->window.y = work->y;
            work->window.color = work->color;
            work->window.mode = 3;
            work->window.title = D_00A13238;
            work->window.rows = 1;
            work->window.cols = 7;
            work->window.width = 0x1A6;
            work->window.height = 0xAE;
            work->window.items = work->item;
            WindowSPSet(&work->window);
            work->window.type = 0x11;
            work->opened = 0;
            work->state = 0;
        }
        break;
    case 2:
        switch (work->state) {
        case 0:
            work->opened = 0;
            work->state = 10;
        case 10:
            if (UmnWork.tab_state == 0x11) {
                work->opened = 1;
                work->state = 20;
            }
            break;
        case 20:
        case 40:
            target = 0x20;
            if (work->state != 20) {
                target = 0x210;
            } else {
                target = 0x20;
            }
            MoveSlide(&work->x, &target, 3.0f);
            if (work->x == target) {
                { long s = work->state; if (s == 20) work->state = 30; else work->state = 0; }
            }
            break;
        case 30:
            UmnWork.list_index = WindowSPSelect(&work->window, ((unsigned short *)&PadData)[0x1A]);
            if (UmnWork.tab_state != 0x11) {
                work->state = 40;
            }
            break;
        }
        break;
    }
    if (work->opened) {
        work->window.x = work->x;
        work->window.y = work->y;
        WindowSPMain(&work->window);
    }
}

extern int UmnWorkEnd;
extern int UmnSimulationScriptNoTbl[];
extern int UmnSimulationNo;
extern const char D_00A132F8[];
extern int MenuScenarioNoGet(void);
extern void tskUmnSimulationInfo(UmnTaskCommand *task, void *work);
extern void tskUmnSimulationList(UmnTaskCommand *task, void *work);
extern void UmnObjectTaskCreate(void (*callback)(), int offset);
extern void UmnChangeTopLevel(int next_level);
extern void xglSoundEffectNormalID(int sound_id, int volume);
extern void xglFontDebugPrintf(int x, int y, const char *text);
extern int xglFontDebugHex(int x, int y, int value, int digits);

int UmnSimulation(void)
{
    unsigned char state = UmnWork.tab_state;

    switch (state) {
    case 0: {
        int task_offset = UmnWorkEnd;
        int scenario;
        int list_offset;

        task_offset = (task_offset + 15) & ~15;

        UmnObjectTaskCreate((void (*)())tskUmnSimulationPas, task_offset);
        task_offset = (task_offset + 507) & ~15;
        UmnObjectTaskCreate((void (*)())tskUmnSimulationInfo, task_offset);
        list_offset = (task_offset + 927) & ~15;
        UmnObjectTaskCreate((void (*)())tskUmnSimulationList, list_offset);

        scenario = MenuScenarioNoGet();
        UmnWork.list_count = 0;
        if (scenario >= 388) {
            UmnWork.list_count = 1;
        }
        if (scenario >= 345) {
            UmnWork.list_count++;
        }
        if (scenario >= 301) {
            UmnWork.list_count++;
        }
        if (scenario >= 162) {
            UmnWork.list_count++;
        }
        if (scenario >= 115) {
            UmnWork.list_count += 3;
        }
        UmnWork.list_index = -1;
        if (UmnWork.list_count == 0) {
            UmnWork.tab_state = -16;
        } else {
            UmnWork.tab_state = 16;
        }
        break;
    }

    case 16:
        UmnWork.tab_state = 17;
        UmnWork.countdown = 12;
        /* The newly armed countdown is handled by the active state. */

    case 17:
        if (UmnWork.countdown == 0) {
            if (UmnWork.list_index >= 0 && (PadData.half_2a & 0x20) != 0) {
                if ((GameLoopState.flags & 0x400000) != 0) {
                    UmnWork.tab_state = -16;
                    UmnWork.pending_script = UmnSimulationScriptNoTbl[UmnWork.list_index];
                    xglSoundEffectNormalID(1, 0);
                } else {
                    xglSoundEffectNormalID(5, 0);
                }
            }
            if ((PadData.half_2a & 0x40) != 0) {
                xglSoundEffectNormalID(2, 0);
                UmnWork.tab_state = -16;
            }
        }
        break;

    case 240:
        UmnWork.tab_state = -15;
        UmnWork.countdown = 16;

    case 241:
        if (UmnWork.countdown == 0) {
            UmnSimulationNo = UmnWork.pending_script;
            UmnChangeTopLevel(0);
        }
        break;
    }

    if (UmnWork.countdown != 0) {
        UmnWork.countdown--;
    }
    xglFontDebugPrintf(32, 208, D_00A132F8);
    return xglFontDebugHex(0, 64, UmnWork.tab_state, 2);
}

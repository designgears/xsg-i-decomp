/*
 * OV02 original TU 13: 0x00a0de20..0x00a0e938 (5 functions)
 */
#include "common.h"
#include "shared.h"

struct UmnPluginState {
    unsigned char unmodeled_00;
    unsigned char plugin_work_state;
    unsigned char unmodeled_02;
    unsigned char plugin_mode;
    unsigned char unmodeled_04[0x0c];
    signed char transition_timer;
    unsigned char unmodeled_11[0x3f];
    signed char selected_plugin_index;
    signed char unlocked_plugin_count;
    unsigned char selected_plugin_id;
    unsigned char unmodeled_53[0x2d];
};

extern struct UmnPluginState UmnWork;
extern unsigned char *UmnWorkEnd;
extern unsigned char plugin_folder[16];
extern const char D_00A13450[];
extern const char D_00A13540[];
extern unsigned short D_4A1A0C[];

/* PadData's +0x2a halfword records newly pressed controller buttons. */
extern PadPrefix PadData;

struct UmnPluginTask {
    XglTaskPrefix prefix;
    int state;
};

struct UmnPluginInfoWindow {
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
};

struct UmnPluginInfoText {
    short x;
    short y;
    unsigned char unmodeled_04[8];
    char *text;
};

struct UmnPluginInfoWork {
    unsigned char state;
    unsigned char opened;
    unsigned char unmodeled_02[6];
    int color;
    struct UmnPluginInfoWindow window;
    struct UmnPluginInfoText info;
};

struct UmnPluginPasWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    unsigned char unmodeled_0c[4];
    unsigned char state;
};

struct UmnPluginPasLabel {
    unsigned char unmodeled_00[12];
    unsigned char message[4];
    short slide_x;
    short slide_y;
    int accent_color;
};

struct UmnPluginPasRect {
    short x;
    short y;
    int color;
    short width;
    short height;
};

struct UmnPluginPasWork {
    unsigned char unmodeled_00;
    unsigned char initialized;
    unsigned char unmodeled_02[2];
    int color;
    struct UmnPluginPasWindow window;
    unsigned char unmodeled_1c[0x190 - 0x1c];
    struct UmnPluginPasLabel label;
    unsigned char unmodeled_1a8[0x3bc - 0x1a8];
    struct UmnPluginPasRect caption_rect;
};

extern void *msg00_0_00A10C40;
void eMessageSet(void *message, void *text);
void eMessageMain(void *message);
void endPrintExtFunc(int color, int mode, void *rectangle);

void tskUmnPluginPas(void *task, void *work);
void tskUmnPluginInfo(void *task, void *work);
void tskUmnPluginList(void *task, void *work);
void tskUmnPluginExWin(void *task, void *work);
void UmnObjectTaskCreate(void (*task)(void *, void *), void *work);
void UmnChangeTopLevel(int level);
void xglFontDebugPrintf(int x, int y, const char *format, ...);
void xglFontDebugHex(int x, int y, int value, int digits);
void xglSoundEffectNormalID(int sound, int channel);
void WindowDXSet(void *window);
void WindowDXMain(void *window);
void MenuInfoWindow(void *window, void *text);
char *UmnPluginTextGet(unsigned int plugin_id);
void MoveSlide(short *current, short *target, float rate);

void tskUmnPluginPas(void *task_opaque, void *work_opaque)
{
    struct UmnPluginTask *task = task_opaque;
    struct UmnPluginPasWork *work = work_opaque;

    if (UmnWork.plugin_work_state != 4) {
        task->state = -1;
    } else {
        switch (task->state) {
        case 0: {
            int label_x = 288;
            int label_y = 11;
            int label_font_size = 32;
            int initial_state = 1;
            int panel_color;
            work->color = 0x00FFFFF0;
            WindowDXSet(&work->window);
            work->window.x = -272;
            work->window.color = work->color;
            work->window.y = 8;
            work->window.width = 272;
            work->window.height = label_font_size;
            work->window.state = initial_state;
            WindowDXMain(&work->window);
            work->window.state = 3;
            eMessageSet(work->label.message, msg00_0_00A10C40);
            {
            struct UmnPluginPasLabel *label = &work->label;
            label->message[1] = label_font_size;
            panel_color = work->color;
            label->slide_x = label_x;
            label->slide_y = label_y;
            work->initialized = initial_state;
            }
            work->label.accent_color = panel_color + 2;
            break;
        }
        case 2:
            {
                short slide_targets[2];
                short *label_target;
                int plugin_mode = UmnWork.plugin_mode;
                int caption_mode;

                slide_targets[0] = -16;
                label_target = &slide_targets[1];
                *label_target = 288;
                if (plugin_mode < 18) {
                    if (plugin_mode >= 16) {
                        slide_targets[1] = 16;
                    } else {
                        slide_targets[0] = -272;
                    }
                } else {
                    slide_targets[0] = -272;
                }
                if (work->initialized != 0) {
                    MoveSlide(&work->window.x, &slide_targets[0], 3.0f);
                    WindowDXMain(&work->window);
                    caption_mode = 101;
                    work->caption_rect.x = (unsigned short)work->window.x + 3;
                    work->caption_rect.y = (unsigned short)work->window.y + 3;
                    work->caption_rect.width = work->window.width - 6;
                    work->caption_rect.height = work->window.height - 6;
                    work->caption_rect.color = work->color;
                    endPrintExtFunc(work->color, caption_mode, &work->caption_rect);
                    MoveSlide(&work->label.slide_x, label_target, 3.0f);
                    if (work->label.slide_x < 256) {
                        eMessageMain(work->label.message);
                    }
                    endPrintExtFunc(work->color, 102, 0);
                }
            }
            break;
        }
    }
}

void tskUmnPluginInfo(void *task_opaque, void *work_opaque)
{
    struct UmnPluginTask *task = task_opaque;
    struct UmnPluginInfoWork *work = work_opaque;

    if (UmnWork.plugin_work_state != 4) {
        task->state = -1;
    } else {
        switch (task->state) {
        case 0:
            work->color = 0x00FF0000;
            work->opened = 0;
            work->state = 0;
            WindowDXSet(&work->window);
            work->window.x = -16;
            ((volatile struct UmnPluginInfoWork *)work)->window.state = 0;
            work->window.y = 480;
            work->window.width = 544;
            work->window.height = 54;
            work->window.callback = MenuInfoWindow;
            work->window.color = work->color;
            work->window.callback_arg = &work->info;
            work->info.x = 0;
            work->info.y = 0;
            work->info.text = 0;
            ((volatile struct UmnPluginInfoWork *)work)->window.state = 1;
            WindowDXMain(&work->window);
            work->window.state = 3;
            break;
        case 2:
            {
                short slide_target = 480;
                int plugin_mode = UmnWork.plugin_mode;
                unsigned char plugin_id;

                if (plugin_mode < 18) {
                    if (plugin_mode >= 16) {
                        slide_target = 386;
                    }
                }
                plugin_id = UmnWork.selected_plugin_id;
                if (plugin_id < 6) {
                    work->info.text = UmnPluginTextGet((signed char)plugin_id) + 0x20;
                } else {
                    work->info.text = (char *)D_00A13450;
                }
                MoveSlide(&work->window.y, &slide_target, 3.0f);
                WindowDXMain(&work->window);
            }
            break;
        }
    }
}

extern const char D_00A13458[];
extern const char D_00A13460[];
void WindowSPSet(void *window);
void WindowSPMain(void *window);
void WindowSPItemChange(void *window);
int WindowSPSelect(void *window, int pad);
struct UmnPluginListItem {
    const char *text;
    int value;
    unsigned char flag;
};
struct UmnPluginListWindow {
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
    unsigned char unmodeled_16[6];
    struct UmnPluginListItem *items;
};
struct UmnPluginListWork {
    unsigned char state;
    unsigned char opened;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    struct UmnPluginListWindow window;
    unsigned char unmodeled_2c[0x1718];
    struct UmnPluginListItem item[17];
};

void tskUmnPluginList(void *task_opaque, void *work_opaque)
{
    struct UmnPluginTask *task = task_opaque;
    struct UmnPluginListWork *work = work_opaque;
    short target[2];

    if (UmnWork.plugin_work_state != 4) {
        task->state = -1;
        return;
    }
    switch (task->state) {
    case 0:
        work->state = 0;
        work->opened = 0;
        work->x = 0x210;
        work->y = 0x70;
        work->color = 0xFF0000;
        work->window.x = 0x210;
        work->window.y = 0x70;
        work->window.color = 0xFF0000;
        work->window.mode = 3;
        work->window.title = D_00A13458;
        work->window.cols = 10;
        work->window.rows = 1;
        work->window.width = 400;
        work->window.height = 0xF6;
        work->window.items = 0;
        WindowSPSet(&work->window);
        break;
    case 2:
        switch (work->state) {
        case 0:
            work->opened = 0;
            work->state = 10;
        case 10:
            if (UmnWork.plugin_mode == 0x11) {
                work->opened = 1;
                work->state = 20;
            }
            break;
        case 20:
            {
                int i = 0;
                for (; i < UmnWork.unlocked_plugin_count; i++) {
                    work->item[i].text = UmnPluginTextGet(plugin_folder[i]);
                    work->item[i].value = 0;
                    work->item[i].flag = 0;
                }
                work->item[i].text = D_00A13450;
                work->item[i + 1].text = 0;
                work->window.items = work->item;
                WindowSPItemChange(&work->window);
                work->window.type = 0x11;
                WindowSPMain(&work->window);
                work->state = 30;
            }
        case 30:
            target[0] = 0x38;
            MoveSlide(&work->x, &target[0], 3.0f);
            if (work->x == target[0]) {
                work->state = 40;
            }
            break;
        case 40:
            {
                int sel = WindowSPSelect(&work->window, ((unsigned short *)&PadData)[0x1A]);
                if (sel >= 0) {
                    UmnWork.selected_plugin_index = sel;
                    UmnWork.selected_plugin_id = plugin_folder[UmnWork.selected_plugin_index];
                }
                if (UmnWork.plugin_mode != 0x11) {
                    work->state = 50;
                }
            }
            break;
        case 50:
            target[1] = 0x210;
            MoveSlide(&work->x, &target[1], 3.0f);
            if (work->x == target[1]) {
                work->state = 0;
            }
            break;
        }
        break;
    }
    if (work->opened) {
        work->window.x = work->x;
        work->window.y = work->y;
        work->window.color = work->color;
        WindowSPMain(&work->window);
    }
    xglFontDebugPrintf(0, 0x10, D_00A13460, work->state);
}

extern char msg00_1_00A10C48[];
void eNumberSet(void *number, int mode);
void eNumberMain(void *number);
void eTagFontSet(void *tag, void *text);
void eTagFontMain(void *tag);
void eRibbonSet(void *ribbon, int mode);
void eRibbonMain(void *ribbon);
struct UmnPluginExWinItem {
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
struct UmnPluginExWinWork {
    unsigned char state;
    unsigned char opened;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    struct UmnPluginExWinItem item[1];
};

void tskUmnPluginExWin(void *task_opaque, void *work_opaque)
{
    struct UmnPluginTask *task = task_opaque;
    struct UmnPluginExWinWork *work = work_opaque;

    if (UmnWork.plugin_work_state != 4) {
        task->state = -1;
    } else {
        switch (task->state) {
        case 0:
            {
            int i;
            work->x = 0x210;
            work->y = 0x20;
            work->color = 0x00FFFFF0;
            for (i = 0; i < 1; i++) {
                work->item[i].slide_x = -0x110;
                work->item[i].slide_y = 0x30;
                eRibbonSet(&work->item[i].ribbon, 3);
                work->item[i].ribbon.width = 0x100;
                work->item[i].ribbon.height = 0x18;
                eTagFontSet(&work->item[i].tag, msg00_1_00A10C48);
                eNumberSet(&work->item[i].number, 0);
            }
            }
            work->state = 0;
            work->opened = 0;
            break;
        case 2:
            {
                short slide_target[2];
                int plugin_mode = UmnWork.plugin_mode;
                int i;

                slide_target[0] = -0x110;
                if (plugin_mode < 18) {
                    if (plugin_mode >= 16) {
                        slide_target[0] = 0;
                    }
                }
                i = 0;
                do {
                    MoveSlide(&work->item[i].slide_x, slide_target, 3.0f);
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
                    work->item[i].number.value2 = UmnWork.selected_plugin_index + (UmnWork.unlocked_plugin_count << 16) + 1;
                    work->item[i].number.digits = 2;
                    eNumberMain(&work->item[i].number);
                    i++;
                } while (i < 1);
            }
            break;
        }
    }
}

void UmnPlugin(void)
{
    switch (UmnWork.plugin_mode) {
    case 0:
        {
        unsigned short unlocked_plugins;
        int plugin_index;
        unsigned char *task_work = UmnWorkEnd;
        unsigned char *folder_cursor;
        unsigned short *plugin_flags;

        UmnObjectTaskCreate(tskUmnPluginPas, task_work);
        task_work += 968;
        UmnObjectTaskCreate(tskUmnPluginInfo, task_work);
        folder_cursor = plugin_folder;
        task_work += 848;
        UmnObjectTaskCreate(tskUmnPluginList, task_work);
        UmnObjectTaskCreate(tskUmnPluginExWin, task_work + 6340);
        plugin_flags = D_4A1A0C;
        UmnWork.unlocked_plugin_count = 0;
        UmnWork.selected_plugin_index = 0;
        memset(folder_cursor, 0, sizeof(plugin_folder));
        unlocked_plugins = plugin_flags[0x43];
        for (plugin_index = 0; plugin_index < 16; plugin_index++) {
            if (((int)unlocked_plugins >> plugin_index) & 1) {
                *folder_cursor++ = plugin_index;
                UmnWork.unlocked_plugin_count++;
            }
        }
        if (UmnWork.unlocked_plugin_count >= 7) {
            UmnWork.unlocked_plugin_count = 6;
        }
        UmnWork.plugin_mode = 16;
        UmnWork.selected_plugin_id = plugin_folder[UmnWork.selected_plugin_index];
        break;
        }
    case 16:
        UmnWork.plugin_mode = 17;
        UmnWork.transition_timer = 12;
        /* Continue through the entry timer check. */
    case 17:
        if (UmnWork.transition_timer == 0 &&
            (PadData.half_2a & 0x40) != 0) {
            UmnWork.plugin_mode = 240;
            xglSoundEffectNormalID(2, 0);
        }
        break;
    case 240:
        UmnWork.plugin_mode = 241;
        UmnWork.transition_timer = 16;
        /* Continue through the exit timer check. */
    case 241:
        if (UmnWork.transition_timer == 0) {
            UmnChangeTopLevel(0);
        }
        break;
    }

    if (UmnWork.transition_timer != 0) {
        UmnWork.transition_timer--;
    }
    xglFontDebugPrintf(32, 208, D_00A13540);
    xglFontDebugHex(0, 64, UmnWork.plugin_mode, 2);
}

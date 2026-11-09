/*
 * OV02 original TU 7: 0x00a03450..0x00a04170 (2 functions)
 */
#include "common.h"
#include "shared.h"

#define UMN_MODE_TOP_MENU 0
#define UMN_TASK_INIT 0
#define UMN_TASK_RUN 2
#define UMN_TASK_END (-1)
#define UMN_TOP_OPTION_COUNT 4
#define UMN_TOP_WIN_INIT 0
#define UMN_TOP_WIN_WAIT 20
#define UMN_TOP_WIN_SCROLL 22
#define UMN_TOP_OPTION_SELECTED_Y 128
#define UMN_TOP_OPTION_SCROLL_STEP 16
#define UMN_TOP_OPTION_WRAP_BOTTOM 464
#define UMN_TOP_OPTION_WRAP_TOP (-112)
#define UMN_TOP_PANEL_RED 56
#define UMN_TOP_PANEL_GREEN (-84)
#define UMN_TOP_PANEL_BLUE (-62)
#define UMN_TOP_PANEL_ALPHA 84
#define OV02_SCREEN_HEIGHT 448
#define EMESSAGE_MODE_LINE_START 0x24
#define E_MODE_SHOW_START 0x20
#define END_PRINT_CMD_LINE 0
#define END_PRINT_CMD_SPRITE_QUAD 8
#define END_PRINT_CMD_FONT_TEX_CHANGE 14
#define END_PRINT_CMD_FONT_TEX_RELOAD 15
typedef struct UmnTopWorkData {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char menu_selected;
    unsigned char tab_state;
    unsigned char unmodeled_04[0x0E];
    signed char direction;
    unsigned char unmodeled_13[0x0D];
    unsigned char option_ids[16];
    unsigned char menu_count;
    unsigned char menu_cursor;
} UmnTopWorkData;
extern unsigned char UmnWork[];
#define UmnTopW (*(UmnTopWorkData *)UmnWork)
typedef struct UmnObjectTask {
    int base[4];
    int state;
} UmnObjectTask;
typedef struct UmnTopESprite {
    signed char state;
    u8 unmodeled_001[3];
    short x;
    short y;
    int work;
    signed char color[4];
    u8 unmodeled_010[0x14];
    short spriteId;
} UmnTopESprite;
typedef struct UmnTopCursolSprite {
    short x; short y; int work; short width; short height;
    signed char color[4]; short style; signed char frame; signed char flags;
} UmnTopCursolSprite;
typedef struct UmnTopCursol {
    unsigned char state, active, timer, width;
    unsigned short x, y; int work; unsigned char color[4];
    UmnTopCursolSprite sprite;
} UmnTopCursol;
typedef struct UmnTopMessage {
    unsigned char flags; unsigned char mode; unsigned char visible; unsigned char wrap_width_half;
    short x; short y; int draw_parameter; short text_x_offset; short text_y_offset;
    unsigned char color_r, color_g, color_b, color_a;
    unsigned short text_timer; unsigned short frame; const char *text;
    unsigned char page_available, page_index, page_count;
    UmnTopCursol cursor;
} UmnTopMessage;
typedef struct UmnTopRibbonPoint { short x; short y; int work; signed char r, g, b, a; } UmnTopRibbonPoint;
typedef struct UmnTopRibbon {
    short x; short y; int work; short width; short height;
    unsigned char mode, progress, visible, style;
    UmnTopRibbonPoint points[6];
    u8 unmodeled_058[4]; int renderCount; u8 unmodeled_060[0x10];
} UmnTopRibbon;
typedef struct UmnTopSprite {
    signed char state;
    u8 unmodeled_001[3];
    short x;
    short y;
    int work;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
    u8 unmodeled_010[0x14];
    short spriteId;
} UmnTopSprite;
typedef struct UmnTopOption {
    short x;
    short y;
    UmnTopSprite sprite;
    UmnTopRibbon ribbon;
    float fade;
} UmnTopOption;
typedef struct UmnTopWindow {
    int x;
    int y;
    int graphics;
    int width;
    int height;
    unsigned char unmodeled_14[4];
    unsigned char mode;
    unsigned char unmodeled_19[0x57];
} UmnTopWindow;
typedef struct UmnTopVertex {
    short x;
    short y;
    int graphics;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
} UmnTopVertex;
typedef struct UmnTopEntry {
    UmnTopVertex vertex[6];
} UmnTopEntry;
typedef struct UmnTopQuad {
    UmnTopVertex vertex[3];
} UmnTopQuad;
typedef struct UmnTopBox {
    unsigned char unmodeled_00;
    unsigned char unmodeled_01;
    short unmodeled_02;
    unsigned char unmodeled_04[0x0c];
    int box_16;
    int box_20;
    int box_24;
} UmnTopBox;
typedef struct UmnTopColor {
    unsigned char c0;
    unsigned char c1;
    unsigned char c2;
    unsigned char c3;
} UmnTopColor;
typedef struct UmnTopWinWork {
    unsigned char state;
    unsigned char visible;
    short selection_span;
    short x;
    short y;
    int graphics;
    int selected_option;
    UmnTopOption option[UMN_TOP_OPTION_COUNT];
    UmnTopWindow window;
    UmnTopMessage top_message;
    UmnTopMessage bottom_message;
    UmnTopESprite group1[2];
    int texture_addr;
    UmnTopESprite group2[4];
    unsigned char unmodeled_45c[4];
    UmnTopBox box;
    unsigned char unmodeled_49c[0x34];
    UmnTopColor bar_color[4];
    UmnTopQuad quad0;
    UmnTopQuad quad1;
    UmnTopEntry entry[5];
} UmnTopWinWork;
typedef struct UmnTopShortSet {
    short value[4];
} UmnTopShortSet;
typedef struct UmnTopIntSet {
    int value[4];
} UmnTopIntSet;
extern const char menuCursorDebugFormat[];
void eWindowSet(void *window);
void eSpriteSet(UmnTopESprite *sprite, int texture);
void eSpriteMain(UmnTopESprite *sprite);
void eMessageMain(UmnTopMessage *message);
void eMessageSet(UmnTopMessage *message, const char *text);
void eMessageTextChange(UmnTopMessage *message, const char *text);
void eMessageModeChange(UmnTopMessage *message, unsigned char mode);
void eRibbonSet(UmnTopRibbon *ribbon, int style);
void endPrintExtFunc(int order, int command, void *param);
extern int UmnTexAddr;

void taskUmnTopWin(void *taskArg, void *data)
{
    static const char *moji00[] = {
        "Mail",
        "Database",
        "EVS (Environmental Simulator)",
        "Plug-in Module",
    };
    static const char *moji01[] = {
        "Read and reply to emails.\nDownload attachments.",
        "View database relating to the\ncharacters and the Gnosis.",
        "Simulate places that you have\nbeen to previously.",
        "View list of Plug-ins currently\nequipped.",
    };
    UmnObjectTask *task = taskArg;
    UmnTopWinWork *work;
    UmnTopOption *option;
    UmnTopESprite *sprite;
    UmnTopShortSet icons;
    UmnTopIntSet positions;
    UmnTopShortSet icons2;
    unsigned int k;
    int j;
    int m;
    int v;
    int i;
    int point;
    unsigned char cur;
    int id;
    int index;
    int nav;
    int waiting;
    int graphicsTemp;
    unsigned char *bar;
    float fade;

    if (UmnTopW.mode != UMN_MODE_TOP_MENU) {
        task->state = UMN_TASK_END;
        return;
    }
    work = data;
    switch (task->state) {
    case UMN_TASK_INIT:
        work->x = 48;
        work->y = 256;
        work->graphics = 0xfc000;
        work->visible = 0;
        eWindowSet(&work->window);
        work->window.mode = E_MODE_SHOW_START;
        work->window.graphics = work->graphics;
        work->window.width = 576;
        work->window.height = 102;
        index = UmnTopW.option_ids[UmnTopW.menu_cursor] - 1;
        eMessageSet(&work->top_message, moji00[index]);
        eMessageModeChange(&work->top_message, EMESSAGE_MODE_LINE_START);
        graphicsTemp = work->graphics;
        work->top_message.draw_parameter = graphicsTemp + 6;
        eMessageSet(&work->bottom_message, moji01[index]);
        eMessageModeChange(&work->bottom_message, EMESSAGE_MODE_LINE_START);
        graphicsTemp = work->graphics;
        work->bottom_message.draw_parameter = graphicsTemp + 6;
        icons = (UmnTopShortSet){ .value = { 5120, 5121, 5122, 5123 } };
        work->selection_span = 144;
        positions = (UmnTopIntSet){ .value = { 0, 1, 2, -1 } };
        for (k = 0; k < UmnTopW.menu_count; k++) {
            eRibbonSet(&work->option[k].ribbon, 3);
            work->option[k].ribbon.width = 102;
            work->option[k].ribbon.height = 128;
            if (UmnTopW.option_ids[k] != 0) {
                eSpriteSet((UmnTopESprite *)&work->option[k].sprite, icons.value[UmnTopW.option_ids[k] - 1]);
            }
            work->option[k].x = 48;
            work->option[k].y = work->selection_span * positions.value[(unsigned int)(k - UmnTopW.menu_cursor) % (unsigned int)UmnTopW.menu_count] + UMN_TOP_OPTION_SELECTED_Y;
            if (UmnTopW.menu_cursor == k) {
                work->option[k].fade = 1.0f;
            } else {
                work->option[k].fade = 0.5f;
            }
        }
        for (j = 0; j < 2; j++) {
            eSpriteSet(&work->group1[j], 266);
            work->group1[j].work = work->graphics + 8;
        }
        icons2 = (UmnTopShortSet){ .value = { 6144, 6145, 6146, 6147 } };
        for (j = 0; j < 4; j++) {
            eSpriteSet(&work->group2[j], icons2.value[j]);
            work->group2[j].work = work->graphics + 8;
        }

        work->box.unmodeled_01 = 0;
        work->box.unmodeled_02 = 5131;
        work->box.unmodeled_00 = 0;
        for (i = 0; i < 4; i++) {
            work->bar_color[i].c0 = work->bar_color[i].c1 = work->bar_color[i].c2 = work->bar_color[i].c3 = -128;
        }
        work->bar_color[1].c3 = 112;
        work->bar_color[0].c3 = 112;
        work->bar_color[3].c3 = 0;
        work->bar_color[2].c3 = 0;
        work->state = 0;
        work->visible = 1;
        break;
    case UMN_TASK_RUN:
        switch (work->state) {
        case UMN_TOP_WIN_INIT:
            work->state = UMN_TOP_WIN_WAIT;
        case UMN_TOP_WIN_WAIT:
            nav = UmnTopW.direction;
            if (nav != 0) {
                work->selected_option = UmnTopW.menu_cursor;
                if (UmnTopW.option_ids[UmnTopW.menu_cursor] != 0) {
                    int message = UmnTopW.option_ids[UmnTopW.menu_cursor] - 1;

                    eMessageTextChange(&work->top_message, moji00[message]);
                    eMessageModeChange(&work->top_message, EMESSAGE_MODE_LINE_START);
                    eMessageTextChange(&work->bottom_message, moji01[message]);
                    eMessageModeChange(&work->bottom_message, EMESSAGE_MODE_LINE_START);
                }
                waiting = 0;
                work->state = UMN_TOP_WIN_SCROLL;
            } else {
                waiting = 1;
            }
            if (waiting) {
                break;
            }
        case UMN_TOP_WIN_SCROLL:
            for (v = 0; v < UmnTopW.menu_count; v++) {
                work->option[v].y -= UmnTopW.direction * UMN_TOP_OPTION_SCROLL_STEP;
                if (work->option[v].y == UMN_TOP_OPTION_WRAP_BOTTOM) {
                    work->option[v].y = UMN_TOP_OPTION_WRAP_TOP;
                } else if (work->option[v].y == UMN_TOP_OPTION_WRAP_TOP) {
                    work->option[v].y = UMN_TOP_OPTION_WRAP_BOTTOM;
                }
            }
            cur = UmnTopW.menu_cursor;
            if (UMN_TOP_OPTION_SELECTED_Y == work->option[cur].y) {
                if (UmnTopW.option_ids[cur] == 0) {
                    UmnTopW.menu_cursor = cur + UmnTopW.direction;
                    UmnTopW.menu_cursor = UmnTopW.menu_cursor % UmnTopW.menu_count;
                    work->state = UMN_TOP_WIN_WAIT;
                } else {
                    work->state = UMN_TOP_WIN_WAIT;
                    UmnTopW.direction = 0;
                }
            }
        }
        if (work->visible != 0) {
            work->quad0.vertex[0].y = 0;
            work->quad0.vertex[1].x = 36;
            work->quad0.vertex[1].y = OV02_SCREEN_HEIGHT;
            work->quad0.vertex[1].graphics = work->graphics;
            work->quad0.vertex[0].x = 36;
            work->quad0.vertex[0].graphics = work->graphics;
            work->quad0.vertex[0].red = UMN_TOP_PANEL_RED;
            work->quad0.vertex[1].red = UMN_TOP_PANEL_RED;
            work->quad0.vertex[0].green = UMN_TOP_PANEL_GREEN;
            work->quad0.vertex[1].green = UMN_TOP_PANEL_GREEN;
            work->quad0.vertex[0].blue = UMN_TOP_PANEL_BLUE;
            work->quad0.vertex[1].blue = UMN_TOP_PANEL_BLUE;
            work->quad0.vertex[0].alpha = UMN_TOP_PANEL_ALPHA;
            work->quad0.vertex[1].alpha = UMN_TOP_PANEL_ALPHA;
            work->quad0.vertex[2].graphics = 0;
            endPrintExtFunc(0, END_PRINT_CMD_LINE, &work->quad0);
            work->quad1.vertex[1].x = 160;
            work->quad1.vertex[1].y = OV02_SCREEN_HEIGHT;
            work->quad1.vertex[1].graphics = work->graphics;
            work->quad1.vertex[0].x = 160;
            work->quad1.vertex[0].graphics = work->graphics;
            work->quad1.vertex[0].y = 0;
            work->quad1.vertex[0].red = UMN_TOP_PANEL_RED;
            work->quad1.vertex[1].red = UMN_TOP_PANEL_RED;
            work->quad1.vertex[0].green = UMN_TOP_PANEL_GREEN;
            work->quad1.vertex[1].green = UMN_TOP_PANEL_GREEN;
            work->quad1.vertex[0].blue = UMN_TOP_PANEL_BLUE;
            work->quad1.vertex[1].blue = UMN_TOP_PANEL_BLUE;
            work->quad1.vertex[0].alpha = UMN_TOP_PANEL_ALPHA;
            work->quad1.vertex[1].alpha = UMN_TOP_PANEL_ALPHA;
            work->quad1.vertex[2].graphics = 0;
            endPrintExtFunc(0, END_PRINT_CMD_LINE, &work->quad1);
            work->texture_addr = UmnTexAddr;
            endPrintExtFunc(0, END_PRINT_CMD_FONT_TEX_CHANGE, &work->texture_addr);
            for (k = 0; (int)k < UmnTopW.menu_count; k++) {
                id = UmnTopW.option_ids[k];
                if (id != 0) {
                    work->option[k].sprite.x = work->option[k].x;
                    work->option[k].sprite.y = work->option[k].y;
                    work->option[k].sprite.work = work->graphics - 2;
                    if (UmnTopW.menu_cursor == k) {
                        fade = work->option[k].fade + 0.1f;
                        work->option[k].fade = fade;
                        if (fade > 1.0f) {
                            work->option[k].fade = 1.0f;
                        }
                    } else {
                        fade = work->option[k].fade - 0.1f;
                        work->option[k].fade = fade;
                        if (0.5f > fade) {
                            work->option[k].fade = 0.5f;
                        }
                    }
                    work->option[k].sprite.red = (unsigned char)(work->option[k].fade * 128.0f);
                    work->option[k].sprite.green = (unsigned char)(work->option[k].fade * 128.0f);
                    work->option[k].sprite.blue = (unsigned char)(work->option[k].fade * 128.0f);
                    eSpriteMain((UmnTopESprite *)&work->option[k].sprite);
                } else {
                    int px[5] = { work->option[k].x, work->option[k].x + 100, work->option[k].x + 100, work->option[k].x, work->option[k].x };
                    int py[5] = { work->option[k].y, work->option[k].y, work->option[k].y + 128, work->option[k].y + 128, work->option[k].y };

                    for (point = 0; point < 5; point++) {
                        work->entry[k].vertex[point].x = px[point];
                        work->entry[k].vertex[point].y = py[point];
                        work->entry[k].vertex[point].graphics = work->graphics;
                        work->entry[k].vertex[point].red = UMN_TOP_PANEL_RED;
                        work->entry[k].vertex[point].green = UMN_TOP_PANEL_GREEN;
                        work->entry[k].vertex[point].blue = UMN_TOP_PANEL_BLUE;
                        work->entry[k].vertex[point].alpha = UMN_TOP_PANEL_ALPHA;
                    }
                    work->entry[k].vertex[5].graphics = 0;
                    endPrintExtFunc(0, END_PRINT_CMD_LINE, &work->entry[k].vertex[0]);
                }
            }
            do {
                for (m = 0; m < 4; m++) {
                    work->group2[m].x = 176 + 64 * m;
                    work->group2[m].y = 392;
                    eSpriteMain(&work->group2[m]);
                }
            } while (0);
            endPrintExtFunc(0, END_PRINT_CMD_FONT_TEX_RELOAD, 0);
            work->window.x = work->x - 96;
            work->window.y = work->y;
            endPrintExtFunc(0, END_PRINT_CMD_FONT_TEX_CHANGE, &work->texture_addr);
            work->box.box_16 = 64;
            work->box.box_20 = work->y + 7;
            work->box.box_24 = work->graphics + 1;
            endPrintExtFunc(0, END_PRINT_CMD_SPRITE_QUAD, &work->box);
            endPrintExtFunc(0, END_PRINT_CMD_FONT_TEX_RELOAD, 0);
            work->top_message.x = work->x + 108;
            work->top_message.y = work->y + 10;
            eMessageMain(&work->top_message);
            work->bottom_message.x = work->x + 108;
            work->bottom_message.y = work->y + 46;
            eMessageMain(&work->bottom_message);
        }
        break;
    }
}

extern unsigned char UmnWork[];
extern unsigned char D_4A1A0C[];
extern int UmnWorkEnd;
extern unsigned char PadData[];
void taskUmnTopWin(void *, void *);
void UmnObjectTaskCreate(void (*task)(void *, void *), void *work);
void UmnChangeTopLevel(int level);
void xglSoundEffectNormalID(int sound_id, int variant);
void xglFontDebugPrintf(int x, int y, const char *format, ...);

short *UmnManzaiText = 0;
unsigned int UmnManzaiFlag = 0;
unsigned char UmnManzaiWin[2][0xA4] = { { 0 } }; /* UmnManzaiWinTask[2], umn_procurator.h */

void UmnTopMenu(void)
{
    int i;
    unsigned char *p;
    unsigned char *d;
    unsigned short t, t2, t3;
    unsigned char v;

    switch (UmnWork[3]) {
    case 0:
        UmnObjectTaskCreate(taskUmnTopWin, (void *)((UmnWorkEnd + 0xF) & ~0xF));
        d = D_4A1A0C;
        p = UmnWork + 0x23;
        i = 3;
        do {
            i--;
            *p-- = 0;
        } while (i >= 0);
        i = 4;
        t = *(unsigned short *)(d + 0x86);
        if (t & 8) {
            UmnWork[0x20] = 3;
        }
        UmnWork[0x21] = 1;
        if (t & 2) {
            UmnWork[0x22] = 2;
        }
        UmnWork[0x23] = i;
        UmnWork[0x30] = i;
        if (UmnWork[2] == 0) {
            for (i = 0; i < 4 && UmnWork[0x20 + i] == 0; i++) {
            }
            UmnWork[2] = i;
        }
        UmnWork[0x31] = UmnWork[2];
        UmnWork[3] = 0x20;
        UmnWork[0x10] = 8;
        /* fallthrough */
    case 0x20:
        UmnWork[0x11] = 0;
        UmnWork[0x12] = 0;
        UmnWork[3] = 0x21;
        /* fallthrough */
    case 0x21:
        if ((signed char)UmnWork[0x10] == 0 && (signed char)UmnWork[0x12] == 0) {
            if (UmnWork[0x30] >= 2) {
                t2 = *(unsigned short *)(PadData + 0x30);
                if (t2 & 0x1000) {
                    UmnWork[0x12] = -1;
                }
                if (t2 & 0x4000) {
                    UmnWork[0x12] = 1;
                }
            }
            if ((signed char)UmnWork[0x12] != 0) {
                UmnWork[0x31] = (unsigned char)(UmnWork[0x31] + UmnWork[0x12]);
                v = UmnWork[0x31] % UmnWork[0x30];
                UmnWork[0x10] = 7;
                UmnWork[0x31] = v;
                xglSoundEffectNormalID(3, 0);
            } else {
                t3 = *(unsigned short *)(PadData + 0x2A);
                if (t3 & 0x20) {
                    UmnWork[2] = UmnWork[0x31];
                    if (UmnWork[0x20 + UmnWork[0x31]] != 0) {
                        UmnChangeTopLevel(UmnWork[0x20 + UmnWork[0x31]]);
                        xglSoundEffectNormalID(1, 0);
                    } else {
                        xglSoundEffectNormalID(5, 0);
                    }
                } else if (t3 & 0x40) {
                    UmnWork[3] = 0xF0;
                    xglSoundEffectNormalID(2, 0);
                }
            }
        }
        break;
    case 0xF0:
        UmnWork[1] = 0xFF;
        break;
    }

    if ((signed char)UmnWork[0x10] != 0) {
        UmnWork[0x10] = (signed char)UmnWork[0x10] - 1;
    }
    xglFontDebugPrintf(0, 0x40, "%d", UmnWork[0x31]);
}

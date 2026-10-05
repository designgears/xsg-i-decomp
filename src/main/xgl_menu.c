#include "common.h"
#include "shared.h"
#include "xgl_menu.h"

extern float I2F(int value);
typedef struct XglMenuOpenArg {
    unsigned char type;
    unsigned char sub;
    unsigned char pad02[2];
    short x;
    short y;
    short z;
    unsigned char pad0a[2];
    void *val0c;
    XglMenuNode *node;
    int val14;
} XglMenuOpenArg;
typedef struct XglMenuType0 {
    unsigned char a;
    unsigned char b;
    unsigned char pad02[0xE];
    float pos[3];
    float scale[5];
    short rot[4];
    unsigned char pad38[4];
} XglMenuType0;
typedef struct XglMenuOpenEntry {
    XglMenuNode *node;
    unsigned char type;
    unsigned char sub;
    unsigned char pad06;
    unsigned char active;
    void *val08;
    int pad0c;
    int val10;
    unsigned char pad14[0xC];
    union {
        XglMenuType0 t0;
        XglMenuListData list;
    } u;
    unsigned short colors[18];
} XglMenuOpenEntry;

int xglMenuOpen(int index, XglMenuOpenArg *arg) {
    XglMenuOpenEntry *entry;
    XglMenuType0 *t0;
    XglMenuListData *list;
    int i;

    if (index == -1) {
        for (index = 0; index < 16; index++) {
            if (menutbl[index].active == 0) {
                break;
            }
        }
        if (index == 16) {
            return -1;
        }
    }
    entry = (XglMenuOpenEntry *)&menutbl[index];
    entry->type = arg->type;
    entry->sub = arg->sub;
    entry->active = 1;
    entry->node = arg->node;
    entry->val08 = arg->val0c;
    entry->val10 = arg->val14;
    switch (arg->type) {
    case 0:
        t0 = &entry->u.t0;
        t0->a = 100;
        t0->b = 0;
        t0->pos[0] = I2F(arg->x);
        t0->pos[1] = I2F(arg->y);
        t0->pos[2] = I2F(arg->z);
        t0->scale[0] = 1.0f;
        t0->scale[1] = 1.0f;
        t0->scale[2] = 1.0f;
        t0->scale[3] = 1.0f;
        t0->scale[4] = 1.0f;
        t0->rot[0] = 0;
        t0->rot[1] = 0;
        t0->rot[2] = 0;
        t0->rot[3] = 0;
        for (i = 0; i < entry->node->item_count; i++) {
            entry->colors[i] = 0x3000;
        }
        break;
    case 1:
        list = &entry->u.list;
        list->current_index = 0;
        list->position_x = arg->x;
        list->position_y = arg->y;
        list->position_z = arg->z;
        list->flags = 0;
        list->items[0] = arg->node;
        break;
    }
    return index;
}

static void xglMenuDrawType0(XglMenuEntry *entry, int unused) {
    entry->active = 0;
}

static void xglMenuDrawType1Sub(XglMenuEntry *entry,
                                XglMenuDrawContext *context, unsigned int level)
{
    XglMenuListData *list;
    XglMenuNode *node;
    XglMenuRow *row;
    int row_index;

    row_index = 0;
    list = &entry->list;
    node = list->items[level];
    while (row_index < node->item_count) {
        row = &node->rows[row_index];
        if (row_index == node->selected_item) {
            xglFontDebugPrintf(context->x, context->y,
                               D_004DC2B8, row->text);
        } else if (row->next == (XglMenuNode *)-1) {
            xglFontDebugPrintf(context->x, context->y,
                               D_004DC2C0, row->text);
        } else {
            xglFontDebugPrintf(context->x, context->y,
                               D_004DC2C8, row->text);
        }

        context->y += list->row_offsets[level];

        if (level < (unsigned int)list->current_index &&
            list->items[level + 1] == row->next) {
            context->x += MENU_LIST_INDENT;
            xglMenuDrawType1Sub(entry, context, level + 1);
            context->x -= MENU_LIST_INDENT;
        }
        row_index++;
    }
}

typedef struct XglMenuPadView {
    unsigned char pad00[0x32];
    unsigned short trg;
    unsigned short rpt;
} XglMenuPadView;
extern XglMenuPadView PadData;
typedef struct XglMenuNodeV {
    unsigned char item_count;
    signed char init_sel;
    unsigned char flags;
    unsigned char max_offset;
    unsigned char selected_item;
    unsigned char pad05[3];
    int val08;
    int val0c;
    unsigned char pad10[8];
    XglMenuRow *rows;
} XglMenuNodeV;
typedef struct XglMenuEntV {
    unsigned char pad00[4];
    unsigned char type;
    unsigned char sub;
    unsigned char pad06;
    unsigned char active;
    int val08;
    int result;
    int *out;
    unsigned char pad14[0xC];
    XglMenuListData list;
} XglMenuEntV;

static void xglMenuDrawType1(XglMenuEntry *entry, XglMenuDrawContext *context)
{
    XglMenuEntV *e = (XglMenuEntV *)entry;
    XglMenuListData *list = &e->list;
    int cur = list->current_index;
    XglMenuNodeV *node = (XglMenuNodeV *)list->items[cur];
    XglMenuNodeV *next;
    int i;
    int busy;
    int j;

    switch (e->active) {
    case 1:
        if (list->row_offsets[cur] < node->max_offset) {
            list->row_offsets[cur]++;
        } else {
            e->active = 2;
        }
        break;
    case 2:
        if (PadData.rpt & 0x1000) {
            i = (signed char)node->selected_item;
            do {
                i--;
                if (i < 0) {
                    if (node->flags & 1) {
                        i = node->item_count - 1;
                    } else {
                        i = (signed char)node->selected_item;
                    }
                    break;
                }
            } while (node->rows[i].next == (XglMenuNode *)-1);
            node->selected_item = i;
        }
        if (PadData.rpt & 0x4000) {
            i = (signed char)node->selected_item;
            do {
                i++;
                if (i >= node->item_count) {
                    i = 0;
                    if (!(node->flags & 1)) {
                        i = (signed char)node->selected_item;
                    }
                    break;
                }
            } while (node->rows[i].next == (XglMenuNode *)-1);
            node->selected_item = i;
        }
        if (PadData.trg & 0x20) {
            next = (XglMenuNodeV *)node->rows[(signed char)node->selected_item].next;
            if ((int)next < 0x100000) {
                e->result = (int)next;
                if (e->sub & 1) {
                    if (e->out != 0) {
                        *e->out = (int)next;
                    }
                } else {
                    e->active = 4;
                }
            } else {
                list->current_index++;
                list->items[list->current_index] = (XglMenuNode *)next;
                if (next->init_sel >= 0) {
                    next->selected_item = next->init_sel;
                }
                if (e->out != 0) {
                    *e->out = next->val08;
                }
                e->active = 1;
            }
        }
        if (PadData.trg & 0x40) {
            if (e->out != 0) {
                *e->out = node->val0c;
            }
            if (list->current_index == 0) {
                e->active = 4;
                e->result = e->val08;
            } else {
                e->active = 3;
            }
        }
        break;
    case 3:
        if (list->row_offsets[cur] > 0) {
            list->row_offsets[cur]--;
        } else {
            list->current_index = cur - 1;
            e->active = 2;
        }
        break;
    case 4:
        busy = 0;
        for (j = 0; j < 8; j++) {
            if (list->row_offsets[j] > 0) {
                list->row_offsets[j]--;
                busy = 1;
            }
        }
        if (!busy) {
            if (e->out != 0) {
                *e->out = e->result;
            }
            e->active = 0;
        }
        break;
    }
    context->x = list->position_x;
    context->y = list->position_y;
    xglMenuDrawType1Sub(entry, context, 0);
}

void xglMenuDraw(void) {
    int i;
    for (i = 0; i < 16; i++) {
        if (menutbl[i].active != 0) {
            switch (menutbl[i].unmodeled_00[4]) {
            case 0:
                xglMenuDrawType0(&menutbl[i], 0x70000000);
                break;
            case 1:
                xglMenuDrawType1(&menutbl[i], 0x70000000);
                break;
            }
        }
    }
}

void xglMenuInitial(void)
{
    int i;

    for (i = MENU_TABLE_COUNT - 1; i >= 0; i--) {
        menutbl[i].active = 0;
    }
}

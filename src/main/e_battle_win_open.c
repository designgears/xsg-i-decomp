#include "common.h"
#include "shared.h"
#include "e_battle_win_open.h"

extern char text_0[1];
extern char D_004C3960[];
extern void *BW_item;
struct SPSetW;
extern void WindowSPSet(struct SPSetW *w);
extern void eMessageSet(void *message, int text);
typedef struct { char *a; int b; char c; signed char d; char pad[2]; } OpenItem;
typedef struct { char *a; int b; char c; char pad[3]; } OutItem;
typedef struct { int x; int y; int z; int w; int h; OpenItem *items; char *t; } OpenArg;
typedef struct {
    char p0[4];
    OpenItem *items;       /* 0x4 */
    unsigned char state;   /* 0x8 */
    unsigned char m9;      /* 0x9 */
    char pA[2];
    unsigned short x;      /* 0xC */
    unsigned short y;      /* 0xE */
    int z;                 /* 0x10 */
    short w;               /* 0x14 */
    short h;               /* 0x16 */
    char *t;               /* 0x18 */
    unsigned char m1C;     /* 0x1C */
    unsigned char m1D;     /* 0x1D */
    char p1E[6];
    void *item;            /* 0x24 */
    char p28[0x63 - 0x28];
    unsigned char m63;     /* 0x63 */
    char p64[0x1740 - 0x64];
    unsigned short wx;     /* 0x1740 */
    unsigned short wy;     /* 0x1742 */
    int wz;                /* 0x1744 */
    short ww;              /* 0x1748 */
    short wh;              /* 0x174A */
    char *tag;             /* 0x174C */
    unsigned char wstate;  /* 0x1750 */
    unsigned char wrib;    /* 0x1751 */
    char p1752[0x18D4 - 0x1752];
    char msg;              /* 0x18D4 */
    unsigned char active;  /* 0x18D5 */
    char p18D6[2];
    short mx;              /* 0x18D8 */
    short my;              /* 0x18DA */
    int mz;                /* 0x18DC */
} BWX;

void eBattleWinOpen(OpenArg *a)
{
  OpenItem *s;
  OutItem *d;
  char *v;
  BWX *w;
  BWX *w_52;
  void *it;
  unsigned short tx2;
  unsigned short ty2;
  int tz_t;
  w = (BWX *) BW;
  it = BW_item;
  w->items = a->items;
  s = a->items;
  v = s->a;
  d = it;
  if (v != 0)
  {
    do
    {
      d->a = s->a;
      d->b = s->d;
      d->c = s->c;
      s++;
      d++;
    }
    while (s->a != 0);
    w = (BWX *) BW;
    it = BW_item;
  }
  d[1].a = 0;
  d->a = text_0;
  w->x = a->x;
  w->y = a->y;
  w->z = a->z;
  w->w = 0x1D4;
  w->h = 0x70;
  w->t = a->t;
  w->item = it;
  w->m1C = 2;
  ((BWX *) BW)->m1D = 4;
  BW->unmodeled_09[0] = 7;
  WindowSPSet((struct SPSetW *) (&((BWX *) BW)->state));
  BW->state = 1;
  BW->unmodeled_09[0x5A] = 0x40;
  WindowDXSet(&BW->window);
  w_52 = (BWX *) BW;
  tx2 = a->x + a->w;
  ty2 = a->y + a->h;
  tz_t = a->z;
  w_52->wx = tx2;
  w_52->wy = ty2;
  w_52->wz = tz_t;
  w_52->ww = 0x138;
  w_52->wh = 0x40;
  w_52->tag = D_004C3960;
  BW->window.ribbon_initialized = 1;
  BW->window.state = 1;
  eMessageSet(&((BWX *) BW)->msg, 0);
  BW->active = 0x20;
  ((BWX *) BW)->mx = ((BWX *) BW)->wx + 3;
  ((BWX *) BW)->my = ((BWX *) BW)->wy + 3;
  ((BWX *) BW)->mz = ((BWX *) BW)->wz + 2;
}

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
    int mode = BW->mode;
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

extern int eMessageNextGyouMaxGet(char *s);
extern void eMessageSet(void *message, int text);
typedef struct { unsigned short x; unsigned short p0; unsigned short y; unsigned short p1; int z; int w; char *t; int tag; } OpenArg2;
typedef struct {
    int active;
    int ofs;
    unsigned short x;
    unsigned short y;
    int z;
    unsigned short w;
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
} V2;
#define V ((V2 *)BW2)

void eBattleWinOpen2(OpenArg2 *a)
{
  int w;
  int n;
  unsigned short x;
  unsigned short y;
  unsigned short wu;
  V2 *b;
  V2 *b_t;
  int new_var;
  WindowDXSet(&BW2->window);
  b = (V2 *) BW2;
  b->z = a->z;
  x = a->x;
  y = a->y;
  b->x = x;
  do { } while (0);
  wu = a->w;
  w = a->w;
  b->y = y;
  b->w = wu;
  if (w < 0)
  {
    char *p = a->t;
    int cur = 0;
    int max = 0;
    new_var = !BW2;
    while ((*p) != 0)
    {
      if ((*p) == '\n')
      {
        p++;
        max = (max < cur) ? (cur) : (max);
        cur = 0;
      }
      else
      {
        cur++;
        p++;
      }
    }

    if (new_var)
    {
    }
    max = (max < cur) ? (cur) : (max);
    b->ofs = ((unsigned int) (w - 6)) >> 1;
    b->ofs = b->ofs - ((max * 9) / 2);
  }
  else
  {
    b->ofs = 0;
  }
  n = eMessageNextGyouMaxGet(a->t);
  ((V2 *) BW2)->h = (24 * n) + 6;
  ((V2 *) BW2)->ribbon = 1;
  ((V2 *) BW2)->tag = a->tag;
  eMessageSet(((V2 *) BW2)->msg, (int) a->t);
  b_t = (V2 *) BW2;
  do
  {
  }
  while (0);
  b_t->mc = 0x20;
  b_t->my = b_t->y + 3;
  b_t->mz = b_t->z + 2;
  b_t->mx = (b_t->x + b_t->ofs) + 3;
  ((V2 *) BW2)->state = 1;
  WindowDXMain(&BW2->window);
  ((V2 *) BW2)->state = 3;
  ((V2 *) BW2)->active = 1;
  if (!a->t)
  {
  }
}

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

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct BattleWinPad {
    unsigned char unmodeled_00[0x2A];
    unsigned short half_2a;
    unsigned char unmodeled_2c[6];
    unsigned short half_32;
    unsigned short half_34;
} BattleWinPad;

typedef struct BattleWinTask {
    unsigned char unmodeled_00[0x1C];
    int buttons;
    int selection;
} BattleWinTask;

typedef struct BattleWinOpenParam4 {
    int x;
    int y;
    int style;
    const char *title;
    const char *text;
} BattleWinOpenParam4;

typedef struct BattleWinOpen2Params {
    int x;
    int y;
    int style;
    int width;
    const char *text;
    const char *title;
} BattleWinOpen2Params;

extern int BW3BattleOrDataBase;
extern const char D_004C39D0[];
extern const char D_004DA9D8[];
extern const char *msg00_1_0036D600[];
extern const char *msg01_2_0036D610[];
extern unsigned short msg03_3[3];
typedef struct BattleWinConditionLabelTable {
    const unsigned short *rows[10];
} BattleWinConditionLabelTable;
extern const BattleWinConditionLabelTable D_004C39A8;
const char **func_A2C5F8(int id);
const char **func_A2C698(int id);
const char **func_A2C6E8(int id);
const char **func_A2C738(int id);
void eTagFontSet(void *tag, int tag_word);
void eNumberSet(void *number, int value);
typedef struct BattleWinAnalysisParams {
    int x;
    int y;
    int style;
    const char *name;
    short display_value_low;
    short display_value_high;
    unsigned short condition_flags;
    unsigned char unmodeled_16[2];
    int tech_points;
    int ether_points;
    int skill_points;
    int experience;
    long money;
    unsigned short item;
    unsigned short rare_item;
    const char *item_text;
    const char *rare_item_text;
} BattleWinAnalysisParams;
typedef struct UpWindowDX {
    short x;
    short y;
    int style;
    short width;
    short height;
    const char *tag_id;
    unsigned char state;
    unsigned char ribbon_initialized;
    unsigned char unmodeled_12[2];
    void (*close_callback)(void *window, void *arg);
    void *close_callback_arg;
    unsigned char unmodeled_1c[4];
    WindowDXRect rect;
    unsigned char color_r;
    unsigned char color_g;
    unsigned char color_b;
    unsigned char alpha;
    unsigned char unmodeled_30[0x3C - 0x30];
    WindowDXFrame frame;
    BattleWinTagFont title_font;
    WindowDXRibbon ribbon_top;
    WindowDXRibbon ribbon_bottom;
    WindowDXSprite sprite;
} UpWindowDX;
typedef struct UpBattleWindow3 {
    int active;
    UpWindowDX window;
    BattleWinMessage messages[3];
    BattleWinTagFont tag_font;
    BattleWinNumber number;
    UpWindowDX window2;
    BattleWinTagFont tag_fonts[5];
    BattleWinNumber numbers[5];
    BattleWinMessage messages2[4];
} UpBattleWindow3;
extern UpBattleWindow3 *UBW3 __asm__("BW3");

void eBattleWinOpen3(void *data)
{
    const BattleWinAnalysisParams *param = data;
    BattleWinConditionLabelTable condition_labels = D_004C39A8;
    unsigned char code_ids[2];
    unsigned char code_kinds[2];
    unsigned char rare_category;
    unsigned char item_category;
    int experience_value;
    const char *text;
    int display_index;
    int points_digit_count;
    int entry_index;
    UpBattleWindow3 **analysis_window_slot;
    const unsigned int *database_mode = (const unsigned int *)&BW3BattleOrDataBase;

    WindowDXSet(&UBW3->window);
    analysis_window_slot = &UBW3;
    {
        UpBattleWindow3 *window = (*analysis_window_slot);
        unsigned int x = (unsigned short)param->x;
        const char *tag = D_004C39D0;
        unsigned int y = (unsigned short)param->y;
        int depth = param->style;
        unsigned int mode = *database_mode;

        window->window.x = x;
        window->window.y = y;
        window->window.style = depth;
        window->window.width = 326;
        window->window.height = 54;
        window->window.tag_id = tag;
        switch (mode) {
        case 0:
            window->window.ribbon_initialized = 1;
            window = (*analysis_window_slot);
            break;
        default:
            break;
        }
        eMessageSet(&window->messages[0], (int)(param->name));
    }
    {
        UpBattleWindow3 *message_window = (*analysis_window_slot);

        unsigned short x = message_window->window.x;
        unsigned short y = message_window->window.y;
        int depth = message_window->window.style;

        message_window->messages[0].mode = 32;
        message_window->messages[0].y = y + 3;
        message_window->messages[0].extent = depth + 2;
        message_window->messages[0].x = x + 3;
    }
    eMessageSet(&(*analysis_window_slot)->messages[1], (int)(msg00_1_0036D600[0]));
    {
        UpBattleWindow3 *message_window = (*analysis_window_slot);

        unsigned short x = message_window->window.x;
        unsigned short y = message_window->window.y;
        int depth = message_window->window.style;

        x += 23;
        y += 27;
        message_window->messages[1].x = x;
        message_window->messages[1].y = y;
        message_window->messages[1].extent = depth + 2;
        message_window->messages[1].mode = 32;
        display_index = 0;
    }
    {
        unsigned char *condition_text = (unsigned char *)msg03_3;
        int clear_index;

        for (clear_index = sizeof(msg03_3) - 1; clear_index >= 0; clear_index--) {
            condition_text[clear_index] = 0;
        }
    }
    {
        int condition_index;

        for (condition_index = 0; condition_index < 10; condition_index++) {
            if ((param->condition_flags >> condition_index) & 1) {
                msg03_3[display_index] = condition_labels.rows[condition_index][0];
                if (display_index == 1) {
                    break;
                }
                display_index++;
            }
        }
    }
    eMessageSet(&(*analysis_window_slot)->messages[2], (int)((const char *)msg03_3));
    {
        UpBattleWindow3 *message_window = (*analysis_window_slot);
        unsigned short x = message_window->window.x;
        unsigned short y = message_window->window.y;
        int depth = message_window->window.style;

        message_window->messages[2].mode = 32;
        message_window->messages[2].y = y + 27;
        message_window->messages[2].extent = depth + 2;
        message_window->messages[2].x = x + 123;
    }
    eTagFontSet(&(*analysis_window_slot)->tag_font, (int)D_004DA9D8);
    (*analysis_window_slot)->tag_font.x = (*analysis_window_slot)->window.x + 191;
    (*analysis_window_slot)->tag_font.y = (*analysis_window_slot)->window.y + 16;
    (*analysis_window_slot)->tag_font.style = (*analysis_window_slot)->window.style + 2;
    eNumberSet(&(*analysis_window_slot)->number, 0);
    {
        UpBattleWindow3 *number_window = (*analysis_window_slot);
        int depth = number_window->window.style;
        unsigned short x = number_window->window.x;
        unsigned short y = number_window->window.y;

        number_window->number.option = 3;
        number_window->number.style = depth + 2;
        number_window->number.y = y + 16;
        number_window->number.x = x + 250;
    }
    (*analysis_window_slot)->number.places = 5;
    (*analysis_window_slot)->number.value =
        ((unsigned int)param->display_value_high << 16) +
        param->display_value_low;
    WindowDXSet((WindowDX *)&(*analysis_window_slot)->window2);
    {
        UpBattleWindow3 *window = (*analysis_window_slot);
        unsigned short y = param->y;
        unsigned short x = param->x;
        int depth = param->style;
        unsigned int mode = *database_mode;

        y += 54;
        window->window2.x = x;
        window->window2.y = y;
        window->window2.style = depth;
        window->window2.width = 326;
        window->window2.height = 126;
        if (mode == 0) {
            window->window2.ribbon_initialized = 1;
            window = (*analysis_window_slot);
        }
        entry_index = 0;
        do {
            eTagFontSet(&window->tag_fonts[entry_index], (int)msg01_2_0036D610[entry_index]);
            window = (*analysis_window_slot);
            window->tag_fonts[entry_index].x = window->window2.x + (entry_index / 3) * 120 + 54;
            window->tag_fonts[entry_index].y = window->window2.y + (entry_index % 3) * 24 + 7;
            window->tag_fonts[entry_index].style = window->window2.style + 2;
            entry_index++;
        } while (entry_index < 5);
        entry_index = 0;
initialize_next_number:
        eNumberSet(&window->numbers[entry_index], 0);
        points_digit_count = 4;
        UBW3->numbers[entry_index].x = UBW3->window2.x + (entry_index / 3) * 120 + 58;
        UBW3->numbers[entry_index].y = UBW3->window2.y + (entry_index % 3) * 24 + 7;
        UBW3->numbers[entry_index].style = UBW3->window2.style + 2;
        UBW3->numbers[entry_index].option = 0;
        if (entry_index < 3) {
            UBW3->numbers[entry_index].places = points_digit_count;
        } else {
            UBW3->numbers[entry_index].places = 10;
        }
        entry_index++;
        window = UBW3;
        if (entry_index < 5) {
            goto initialize_next_number;
        }
        {
            int tech_value = param->tech_points;

            rare_category = param->rare_item >> 8;
            (*analysis_window_slot)->numbers[0].value = tech_value;
        }
        item_category = param->item >> 8;
        (*analysis_window_slot)->numbers[1].value = param->ether_points;
        (*analysis_window_slot)->numbers[2].value = param->skill_points;
        experience_value = param->experience;
        (*analysis_window_slot)->numbers[4].value = param->money;
        (*analysis_window_slot)->numbers[3].value = experience_value;
        code_ids[0] = param->rare_item & 0xff;
        code_ids[1] = param->item;
        code_kinds[0] = rare_category;
        code_kinds[1] = item_category;
        entry_index = 0;
        do {
            UpBattleWindow3 *message_window;

            text = msg00_1_0036D600[entry_index + 1];
            eMessageSet(&(*analysis_window_slot)->messages2[entry_index], (int)(text));
            message_window = (*analysis_window_slot);
            message_window->messages2[entry_index].x = message_window->window2.x + entry_index * 40 + 3;
            message_window->messages2[entry_index].y = message_window->window2.y + entry_index * 24 + 75;
            message_window->messages2[entry_index].extent = message_window->window2.style + 2;
            message_window->messages2[entry_index].mode = 32;
            entry_index++;
        } while (entry_index < 2);
    }
    entry_index = 0;
    do {
        if (*database_mode == 0) {
            unsigned char *catalog_id = &code_ids[entry_index];

            if (*catalog_id != 0) {
                switch (code_kinds[entry_index]) {
                case 0:
                    text = *func_A2C5F8(*catalog_id);
                    break;
                case 1:
                    text = *func_A2C6E8(*catalog_id);
                    break;
                case 2:
                    text = *func_A2C738(*catalog_id);
                    break;
                case 3:
                    text = *func_A2C698(*catalog_id);
                    break;
                default:
                    text = 0;
                    break;
                }
            } else {
                text = 0;
            }
        } else if (entry_index == 0) {
            text = param->rare_item_text;
        } else {
            text = param->item_text;
        }
        eMessageSet(&(*analysis_window_slot)->messages2[entry_index + 2], (int)(text));
        {
            UpBattleWindow3 *message_window = (*analysis_window_slot);

            message_window->messages2[entry_index + 2].x = message_window->window2.x + 143;
            message_window->messages2[entry_index + 2].y = message_window->window2.y + entry_index * 24 + 75;
            message_window->messages2[entry_index + 2].extent = message_window->window2.style + 2;
            message_window->messages2[entry_index + 2].mode = 32;
        }
        entry_index++;
    } while (entry_index < 2);
    (*analysis_window_slot)->window.state = 1;
    (*analysis_window_slot)->window2.state = 1;
    (*analysis_window_slot)->active = 1;
}

void eTagFontMain(void *tag);
void eNumberMain(void *number);

int eBattleWinMain3(void)
{
    int i;

    if (*(int *)BW3 == 0) {
        return 0;
    }
    endPrintExtFunc(0xFFFFFF, 0x64, 0);
    WindowDXMain(&BW3->window);
    WindowDXMain((char *)BW3 + 0x314);
    switch ((signed char)BW3->window.state) {
    case 3:
        for (i = 0; i < 3; i++) {
            eMessageMain((char *)BW3 + i * 0x44 + 0x198);
        }
        eTagFontMain((char *)BW3 + 0x264);
        eNumberMain((char *)BW3 + 0x284);
        for (i = 0; i < 4; i++) {
            eMessageMain((char *)BW3 + i * 0x44 + 0x818);
        }
        for (i = 0; i < 5; i++) {
            eTagFontMain((char *)BW3 + i * 0x20 + 0x4A8);
        }
        for (i = 0; i < 5; i++) {
            eNumberMain((char *)BW3 + i * 0x90 + 0x548);
        }
        return 2;
    case 1:
    case 2:
    case 4:
    case 5:
        return 1;
    }
    return 0;
}

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

extern void *BW_item;
extern int BW3BattleOrDataBase;
extern void endPrintInit(void);

void *eBattleWinInit2(void *arg)
{
    char *p;
    int *q2, *q3;
    int *r;

    BW = arg;
    *((char *)arg + 1) = 0;
    p = (char *)arg + 0x1918;
    BW_item = p;
    p = (char *)arg + 0x2518;
    ((struct { BattleWindow2 *v; } *)&BW2)->v = (BattleWindow2 *)p;
    q2 = (int *)p;
    p = (char *)arg + 0x26F8;
    *q2 = 0;
    ((struct { BattleWindow3 *v; } *)&BW3)->v = (BattleWindow3 *)p;
    q3 = (int *)p;
    p = (char *)arg + 0x3020;
    *q3 = 0;
    ((struct { BattleWindow4 *v; } *)&BW4)->v = (BattleWindow4 *)p;
    r = (int *)p;
    p = (char *)arg + 0x3200;
    BW3BattleOrDataBase = 0;
    *r = 0;
    endPrintInit();
    return p;
}

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

void endSpriteSet(void *sprite, int a);
void eTagFontSet(void *tag, int tag_word);
void eTagFontMain(void *tag);
void eRibbonSet(void *ribbon, int a);
void eRibbonMain(void *ribbon);
void OpenCloseMain(void *oc);
typedef struct { short x; short y; int z; short w; short h; } DXRect;
typedef struct DXWin DXWin;
struct DXWin {
    DXRect r;                     /* 0x00 */
    int tag;                      /* 0x0C */
    unsigned char state;          /* 0x10 */
    unsigned char ribbon;         /* 0x11 */
    unsigned char p12[2];
    void (*cb)(DXWin *, void *);  /* 0x14 */
    void *arg;                    /* 0x18 */
    unsigned char p1c[4];
    DXRect r20;                   /* 0x20 */
    unsigned char p2c[0x10];
    unsigned char oc[4];          /* 0x3C */
    DXRect r40;                   /* 0x40 */
    signed char oc4c;             /* 0x4C */
    unsigned char oc4d;           /* 0x4D */
    unsigned char p4e[0x32];
    unsigned char font[4];        /* 0x80 */
    short fx;                     /* 0x84 */
    short fy;                     /* 0x86 */
    int fz;                       /* 0x88 */
    unsigned char fc[4];          /* 0x8C */
    unsigned char p90[0x10];
    DXRect rb;                    /* 0xA0 */
    unsigned char pac[0x64];
    DXRect rb2;                   /* 0x110 */
    unsigned char p11c[0xC];
    struct { unsigned char c[4]; unsigned char pad[8]; } col[6]; /* 0x128 */
    unsigned char p170[0x10];
    unsigned char sprite[0x10];   /* 0x180 */
    short s190;                   /* 0x190 */
};

void WindowDXMain(DXWin *w)
{
    unsigned char st = w->state;
    int s;

    switch (st & 0xF) {
    case 0:
        break;
    case 1:
    case 0x11:
        s = (signed char)st;
        w->state = 0;
        if (w->cb != 0) {
            w->cb(w, w->arg);
        }
        w->r40.x = w->r.x;
        w->r40.y = w->r.y;
        w->r40.z = w->r.z;
        w->r40.w = w->r.w;
        w->r40.h = w->r.h;
        w->s190 = 0x609;
        w->state = s;
        w->oc4c = 0;
        w->oc4d = 0;
        endSpriteSet(w->sprite, 0xFF);
        eTagFontSet(w->font, w->tag);
        eRibbonSet(&w->rb, 1);
        eRibbonSet(&w->rb2, 1);
        w->col[3].c[0] = 0x20;
        w->col[2].c[0] = 0x20;
        w->col[1].c[0] = 0x20;
        w->col[0].c[0] = 0x20;
        w->col[5].c[0] = 0x20;
        w->col[4].c[0] = 0x20;
        w->col[3].c[1] = 0xF;
        w->col[2].c[1] = 0xF;
        w->col[1].c[1] = 0xF;
        w->col[0].c[1] = 0xF;
        w->col[5].c[1] = 0x34;
        w->col[4].c[1] = 0x34;
        w->col[3].c[2] = 0xA;
        w->col[2].c[2] = 0xA;
        w->col[1].c[2] = 0xA;
        w->col[0].c[2] = 0xA;
        w->col[5].c[2] = 0x40;
        w->col[4].c[2] = 0x40;
        w->col[2].c[3] = 0x30;
        w->col[0].c[3] = 0x30;
        w->col[3].c[3] = 0x0;
        w->col[1].c[3] = 0x0;
        w->col[5].c[3] = 0x60;
        w->col[4].c[3] = 0x60;
        if (w->cb != 0) {
            w->cb(w, w->arg);
        }
        w->state = 2;
    case 2:
        OpenCloseMain(w->oc);
        if (w->oc4c >= 0) {
            break;
        }
        w->state = 3;
    case 3:
        w->r20.x = w->r.x;
        w->r20.y = w->r.y;
        w->r20.w = w->r.w;
        w->r20.h = w->r.h;
        w->r20.z = w->r.z;
        endPrintExtFunc(w->r.z, 1, &w->r20);
        if (w->tag != 0) {
            w->rb.x = w->r.x;
            w->rb.y = w->r.y - 0xD;
            w->rb.z = w->r.z;
            w->rb.w = w->r.w - 2;
            w->rb.h = 0xE;
            eRibbonMain(&w->rb);
            w->fx = w->r.x + 8;
            w->fy = w->r.y - 0xE;
            w->fz = w->r.z + 2;
            w->fc[3] = 0x80;
            w->fc[1] = 0x80;
            w->fc[2] = 0x80;
            w->fc[0] = 0x80;
            eTagFontMain(w->font);
        }
        if (w->ribbon == 0) {
            w->rb2.x = w->r20.x + 3;
            w->rb2.y = w->r20.y + 3;
            w->rb2.z = w->r20.z;
            w->rb2.w = w->r20.w - 6;
            w->rb2.h = w->r20.h - 6;
            eRibbonMain(&w->rb2);
        }
        if (w->cb != 0) {
            w->cb(w, w->arg);
        }
        break;
    case 4:
        w->oc4d = 0x10;
        w->r40.x = w->r.x;
        w->r40.y = w->r.y;
        w->r40.z = w->r.z;
        w->r40.w = w->r.w;
        w->r40.h = w->r.h;
        w->oc4c = 0;
        if (w->cb != 0) {
            w->cb(w, w->arg);
        }
        w->state = 5;
    case 5:
        OpenCloseMain(w->oc);
        if (w->oc4c < 0) {
            w->state = 0xFF;
        }
        break;
    }
}

void eCursolModeChange(unsigned char *mode, unsigned int next);
typedef struct { int a; int b; signed char f; } MWEnt;
typedef struct {
    unsigned char p2[0x0C];
    unsigned char r, g, b;
    unsigned char p13[5];
    int text;
    unsigned char p1c[0x44 - 0x18];
} MWMsg;
typedef struct {
    unsigned char pre[0xC];
    unsigned char p0[8];
    unsigned char r, g, b;
    unsigned char pb;
    unsigned char mode;
    unsigned char pd;
    unsigned char e;
    unsigned char f;
    unsigned char p10[4];
    int value;
    unsigned char p18[0x90 - 0x24];
} MWNum;

void subMWModeExSet(char *w)
{
    int top = w[0x24] * w[0x22];
    MWEnt *e = (MWEnt *)(*(int *)(w + 0x1C)) + top;
    int i, n, rem;

    if (*(short *)(w + 0x2A) != 0) {
        rem = *(short *)(w + 0x2A) - top;
        n = w[0x23] * w[0x22];
        if (n > 0x18) n = 0x18;
        if (rem < n) n = rem;
        i = 0;
        if (n > 0) {
            int c20;
            int c3;
            int c80;
            int c30;
            int c40;
            MWMsg *m;
            int *vp;
            MWNum *p;
            int *tp;
            c20 = 0x20;
            c3 = 3;
            c80 = -0x80;
            c30 = 0x30;
            c40 = 0x40;
            m = (MWMsg *)(w + 0x230);
            vp = (int *)(w + 0x8A0);
            p = (MWNum *)(w + 0x880);
            i = n;
            tp = (int *)(w + 0x244);
            do {
                *tp = e->a;
                tp += 0x11;
                ((char *)m)[-3] = c20;
                p->e = 0;
                p->f = w[0x26];
                *vp = e->b;
                vp += 0x24;
                p->mode = c3;
                if (e->f == 0) {
                    m->b = c80;
                    m->g = c80;
                    m->r = c80;
                    p->b = c80;
                    p->g = c80;
                    p->r = c80;
                } else {
                    m->b = c30;
                    m->g = c30;
                    m->r = c30;
                    p->b = c40;
                    p->g = c40;
                    p->r = c40;
                }
                e++;
                i--;
                m++;
                p++;
            } while (i != 0);
            i = n;
        }
        for (; i < 0x18; i++) {
            w[0x22D + i * 0x44] = 0x70;
            ((MWNum *)(w + 0x880))[i].mode = 0;
        }
        *(int *)(w + 0x1620) = *(short *)(w + 0x28) + 1;
        *(int *)(w + 0x16B0) = *(short *)(w + 0x2A);
        w[0x161B] = 3;
        w[0x16AB] = 3;
        w[0x1C1] = 1;
        eCursolModeChange((unsigned char *)(w + 0x1C0), 0x50);
        if (w[0x25] != 0) {
            if (w[0x24] != 0) {
                eCursolModeChange((unsigned char *)(w + 0x1E4), 0x50);
            } else {
                w[0x1E4] = 0x70;
            }
            if (w[0x24] != w[0x25] && w[0x24] < w[0x25]) {
                eCursolModeChange((unsigned char *)(w + 0x208), 0x50);
                return;
            }
            w[0x208] = 0x70;
        } else {
            w[0x1E4] = 0x70;
            w[0x208] = 0x70;
        }
        return;
    }
    {
        int c;
        int v = 0x70;
        for (c = 2; c >= 0; c--) {
            w[0x1C0 + c * 0x24] = v;
        }
    }
    ((MWMsg *)(w + 0x230))[0].text = e->a;
    w[0x22D] = 0x20;
    {
        int j;
        for (j = 0; j < 0x17; j++) {
            w[0x271 + j * 0x44] = 0x70;
            w[0x928 + j * 0x90] = 0;
        }
    }
}

typedef struct SPPosWin {
    short x;
    short y;
    int style;
    unsigned short width;
    short height;
} SPPosWin;
typedef struct SPPosCur {
    unsigned char mode;
    unsigned char unmodeled_01[3];
    short x;
    short y;
    int style;
    unsigned char unmodeled_0c[0x24 - 0x0c];
} SPPosCur;
typedef struct SPPosMsg {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int extent;
    unsigned char unmodeled_0c[4];
    unsigned char color_b;
    unsigned char color_g;
    unsigned char color_r;
    unsigned char unmodeled_13[5];
    char *text;
    unsigned char unmodeled_1c[0x44 - 0x1c];
} SPPosMsg;
typedef struct SPPosNum {
    short x;
    short y;
    int style;
    unsigned char unmodeled_08[0x90 - 0x08];
} SPPosNum;
typedef struct SPPosRect {
    short x;
    short y;
    int style;
    unsigned short width;
    short height;
} SPPosRect;
typedef struct SPPosView {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int style;
    short width;
    short height;
    unsigned char unmodeled_10[4];
    signed char columns;
    signed char rows;
    unsigned char unmodeled_16[6];
    void *items;
    unsigned char visible_top;
    signed char visible_bottom;
    signed char visible_cols;
    signed char visible_rows;
    signed char current_row;
    signed char max_scroll;
    signed char mode;
    unsigned char unmodeled_27;
    short selected_index;
    short item_count;
    SPPosWin window;
    unsigned char unmodeled_38[0x1c0 - 0x38];
    SPPosCur cursors[3];
    SPPosMsg messages[24];
    SPPosNum numbers[26];
    SPPosRect rect;
} SPPosView;

void subMWPosSet(void *arg)
{
    /* v6 */
    SPPosView *list = (SPPosView *)arg;
    int row;
    int col;
    int i;
    int width;
    int text_width;
    const char *p;

    list->rect.x = list->x + 3;
    list->rect.y = list->y + 3;
    list->rect.width = list->width - 6;
    list->rect.height = list->height - 6;
    list->numbers[24].x = list->x + list->width - 142;
    list->numbers[25].x = list->x + list->width - 78;
    list->window.x = list->x;
    list->window.y = list->y;
    list->window.style = list->style;
    list->window.width = list->width;
    list->window.height = list->height;
    list->rect.style = list->style;
    list->numbers[24].y = list->y - 13;
    list->numbers[25].y = list->y - 13;
    list->numbers[24].style = list->style + 2;
    list->numbers[25].style = list->style + 2;
    if (list->item_count != 0) {
        list->cursors[0].x = list->x + (signed char)list->visible_top * (list->width / 2) + 3;
        list->cursors[1].y = list->y + 4;
        list->cursors[0].y = list->y + list->visible_bottom * 24 + 7;
        list->cursors[0].style = list->style + 2;
        list->cursors[1].x = list->x + list->width - 17;
        list->cursors[2].x = list->x + list->width - 17;
        list->cursors[2].y = list->y + list->height - 19;
        list->cursors[1].style = list->style + 2;
        list->cursors[2].style = list->style + 2;
        i = 0;
        for (row = 0; row < list->visible_rows; row++) {
            for (col = 0; col < list->visible_cols; col++) {
                list->messages[i].x = list->x + (list->width / 2) * col + 19;
                list->messages[i].y = list->y + row * 24 + 3;
                list->messages[i].extent = list->style + 2;
                i++;
                if (i >= 24) {
                    break;
                }
            }
        }
        if (list->columns == 2) {
            i = 0;
            for (row = 0; row < list->visible_rows; row++) {
                for (col = 0; col < list->visible_cols; col++) {
                    list->numbers[i].x = list->x + col * (list->width / 2) + 195;
                    list->numbers[i].y = list->y + row * 24 + 8;
                    list->numbers[i].style = list->style + 2;
                    i++;
                    if (i >= 24) {
                        break;
                    }
                }
            }
        } else {
            i = 0;
            for (row = 0; row < list->visible_rows; row++) {
                for (col = 0; col < list->visible_cols; col++) {
                    list->numbers[i].x = list->x + list->width - list->mode * 11 - 19;
                    list->numbers[i].y = list->y + row * 24 + 8;
                    list->numbers[i].style = list->style + 2;
                    i++;
                    if (i >= 24) {
                        break;
                    }
                }
            }
        }
    } else {
        text_width = 0;
        for (p = list->messages[0].text; *p != 0 && *p != '\n'; p++) {
            text_width += 10;
        }
        for (i = 23; i >= 0; i--) {
            list->numbers[i].style = 0;
        }
        width = list->width;
        list->messages[0].x = list->x + (width - text_width) / 2;
        list->messages[0].y = list->y + (list->height - 24) / 2;
        list->messages[0].extent = list->style + 2;
        list->messages[0].color_r = 0x80;
        list->messages[0].color_g = 0x80;
        list->messages[0].color_b = 0x80;
    }
}

void eCursolMain(void *cursol);
void eNumberMain(void *number);

void subMWDraw(char *w)
{
    unsigned char flags = w[1];
    int i;

    WindowDXMain(w + 0x2C);
    if (flags & 1) {
        for (i = 0; i < 3; i++) {
            eCursolMain(w + 0x1C0 + i * 0x24);
        }
    }
    endPrintExtFunc(*(int *)(w + 0x1730), 5, w + 0x172C);
    if (flags & 2) {
        for (i = 0; i < 0x18; i++) {
            eMessageMain(w + 0x22C + i * 0x44);
        }
    }
    if (*(short *)(w + 0x2A) != 0 && (flags & 4)) {
        for (i = 0; i < 0x18; i++) {
            eNumberMain(w + 0x88C + i * 0x90);
        }
    }
    endPrintExtFunc(*(int *)(w + 0x1730), 6, 0);
}

typedef struct MWCtl00 {
    char pad0[0x18];
    int key;
    char pad1c[5];
    signed char cur;
    char pad22;
    signed char rows;
    signed char top;
    unsigned char topmax;
    char pad26[2];
    unsigned short pos;
    short num;
} MWCtl00;
extern void xglSoundEffectNormalID(int, int);

void subMWControlType00(void *p, int k) {
    MWCtl00 *w = p;
    unsigned short old;

    if (w->num < 2) return;
    if (w->key & 0x1000) {
        old = w->pos;
        if ((short)(w->pos = (short)old - 1) < 0) {
            if (*(unsigned short *)((char *)&PadData + 0x32) & 0x1000) {
                w->pos = w->num - 1;
                w->cur = w->rows - 1;
                w->top = w->topmax;
                xglSoundEffectNormalID(3, 0);
            } else {
                w->pos = old;
            }
        } else {
            if (w->cur == 0) w->top = w->top - 1;
            else w->cur--;
            xglSoundEffectNormalID(3, 0);
        }
    }
    if (w->key & 0x4000) {
        old = w->pos;
        if ((short)++w->pos > w->num - 1) {
            if (*(unsigned short *)((char *)&PadData + 0x32) & 0x4000) {
                w->pos = 0;
                w->cur = 0;
                w->top = 0;
                xglSoundEffectNormalID(3, 0);
            } else {
                w->pos = old;
            }
        } else {
        if (w->cur == w->rows - 1) w->top++;
        else w->cur++;
        xglSoundEffectNormalID(3, 0);
        }
    }
}

void subMWControlType01(void *p, int k)
{
    WindowSPCursor *list = (WindowSPCursor *)p;
    if (list->item_count < 2) {
        return;
    }
    if (list->buttons & 0x8000) {
        list->selected_index--;
        if (list->selected_index < 0) {
            if (*(unsigned short *)((unsigned char *)&PadData + 0x32) & 0x8000) {
                list->selected_index = list->item_count - 1;
                list->current_row = list->max_scroll;
                list->visible_bottom = list->visible_rows - 1;
                xglSoundEffectNormalID(3, 0);
            } else {
                list->selected_index++;
            }
        } else {
            if (list->visible_bottom == 0) {
                if (list->visible_top == 0) {
                    list->current_row--;
                }
            } else {
                if (list->visible_top == 0) {
                    list->visible_bottom--;
                }
            }
            xglSoundEffectNormalID(3, 0);
        }
        list->visible_top = list->selected_index % list->visible_cols;
    }
    if (list->buttons & 0x2000) {
        list->selected_index++;
        if (list->item_count - 1 < list->selected_index) {
            if (*(unsigned short *)((unsigned char *)&PadData + 0x32) & 0x2000) {
                list->selected_index = 0;
                list->current_row = 0;
                list->visible_bottom = 0;
                xglSoundEffectNormalID(3, 0);
            } else {
                list->selected_index--;
            }
        } else {
            if (list->visible_bottom == list->visible_rows - 1) {
                if (list->visible_top == list->visible_cols - 1) {
                    list->current_row++;
                }
            } else {
                if (list->visible_top == list->visible_cols - 1) {
                    list->visible_bottom++;
                }
            }
            xglSoundEffectNormalID(3, 0);
        }
        list->visible_top = list->selected_index % list->visible_cols;
    }
    if (list->item_count == 2) {
        return;
    }
    if (list->buttons & 0x1000) {
        list->selected_index -= list->visible_cols;
        if (list->selected_index < 0) {
            if (*(unsigned short *)((unsigned char *)&PadData + 0x32) & 0x1000) {
                if ((list->item_count - 1) % 2 != 0) {
                    list->selected_index = list->item_count + list->selected_index;
                } else {
                    list->selected_index = list->item_count - 1;
                }
                list->visible_bottom = list->visible_rows - 1;
                list->current_row = list->max_scroll;
                xglSoundEffectNormalID(3, 0);
            } else {
                list->selected_index += list->visible_cols;
            }
        } else {
            if (list->visible_bottom == 0) {
                list->current_row--;
            } else {
                list->visible_bottom--;
            }
            xglSoundEffectNormalID(3, 0);
        }
        list->visible_top = list->selected_index % list->visible_cols;
    }
    if (list->buttons & 0x4000) {
        list->selected_index += list->visible_cols;
        if (list->item_count - 1 < list->selected_index) {
            if ((list->item_count - 1) % 2 != 0) {
                if (*(unsigned short *)((unsigned char *)&PadData + 0x32) & 0x4000) {
                    list->visible_bottom = 0;
                    list->selected_index -= list->item_count;
                    list->current_row = 0;
                    xglSoundEffectNormalID(3, 0);
                } else {
                    list->selected_index -= list->visible_cols;
                }
            } else if (list->item_count != list->selected_index) {
                if (*(unsigned short *)((unsigned char *)&PadData + 0x32) & 0x4000) {
                    list->selected_index = 0;
                    list->visible_bottom = 0;
                    list->current_row = 0;
                    xglSoundEffectNormalID(3, 0);
                } else {
                    list->selected_index -= list->visible_cols;
                }
            } else {
                list->selected_index = list->item_count - 1;
                list->visible_bottom = list->visible_rows - 1;
                list->current_row = list->max_scroll;
                xglSoundEffectNormalID(3, 0);
            }
        } else {
            if (list->visible_bottom == list->visible_rows - 1) {
                list->current_row++;
            } else {
                list->visible_bottom++;
            }
            xglSoundEffectNormalID(3, 0);
        }
        list->visible_top = list->selected_index % list->visible_cols;
    }
}

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

typedef struct SPSetItem { char *name; int a; int b; } SPSetItem;
typedef struct SPSetW {
    unsigned char state;
    char pad1[0x13];
    unsigned char cols;
    unsigned char rows;
    char pad16[2];
    int v18;
    struct SPSetItem *items;
    unsigned char b20;
    unsigned char b21;
    unsigned char b22;
    unsigned char b23;
    unsigned char b24;
    unsigned char b25;
    unsigned char b26;
    char pad27;
    short h28;
    short count;
} SPSetW;

void WindowSPSet(SPSetW *w)
{
    SPSetItem *p = w->items;
    int n = 0;
    int t;
    int q;

    if (p == 0) {
        w->b24 = 0;
        w->v18 = 0;
        w->b20 = 0;
        w->b21 = 0;
        w->h28 = 0;
        return;
    }
    w->state = 0;
    w->b26 = 2;
    w->h28 = -1;
    w->v18 = 0;
    while ((p++)->name != 0) {
        n++;
    }
    n--;
    if (n != 0) {
        w->count = n;
        w->h28 = 0;
        w->b24 = 0;
        q = ((short)n - (signed char)w->cols * (signed char)w->rows + (signed char)w->cols - 1) / (signed char)w->cols;
        w->b25 = q;
        if ((signed char)q < 0) w->b25 = 0;
        w->b20 = 0;
        w->b21 = 0;
        if (w->count < (signed char)w->cols) {
            w->b22 = w->count;
        } else {
            w->b22 = w->cols;
        }
        if ((w->count + (signed char)w->cols - 1) / (signed char)w->cols < (signed char)w->rows) {
            w->b23 = (w->count + (signed char)w->cols - 1) / (signed char)w->cols;
        } else {
            w->b23 = w->rows;
        }
        return;
    }
    w->b23 = 0;
    w->b21 = 0;
    w->b22 = 0;
    w->b20 = 0;
    w->b25 = 0;
    w->b24 = 0;
    w->count = 0;
}

struct BattleWinItem {
    char *text;
    int a;
    int b;
};

void WindowSPItemChange(WindowSPCursor *list)
{ /* run 3 */
    BattleWinItem *item;
    int count = 0;

    if (list->items == 0) {
        return;
    }
    list->mode = 2;
    item = list->items;
    while ((item++)->text != 0) {
        count++;
    }
    count--;
    if (count != 0) {
        list->item_count = count;
        if (list->selected_index >= list->item_count) {
            list->selected_index = list->item_count - 1;
        }
        list->max_scroll = (list->item_count - list->columns * list->rows + list->columns - 1) / list->columns;
        if (list->max_scroll < 0) {
            list->max_scroll = 0;
        }
        if (list->current_row > list->max_scroll) {
            list->current_row = list->max_scroll;
        }
        if (list->item_count < list->columns) {
            list->visible_cols = list->item_count;
        } else {
            list->visible_cols = list->columns;
        }
        if ((list->item_count + list->columns - 1) / list->columns < list->rows) {
            list->visible_rows = (list->item_count + list->columns - 1) / list->columns;
        } else {
            list->visible_rows = list->rows;
        }
        if (list->visible_top >= list->visible_cols) {
            list->visible_top = list->visible_cols - 1;
        } else if (list->selected_index == list->item_count - 1) {
            if (list->selected_index % 2 == 0) {
                list->visible_top = 0;
            }
        }
        if (list->visible_bottom >= list->visible_rows) {
            list->visible_bottom = list->visible_rows - 1;
        }
    } else {
        list->visible_rows = 0;
        list->visible_bottom = 0;
        list->visible_cols = 0;
        list->visible_top = 0;
        list->max_scroll = 0;
        list->current_row = 0;
        list->item_count = 0;
        list->selected_index = 0;
    }
}

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

void eCursolSet(void *cursor, int width);
void eNumberSet(void *number, int value);
typedef struct SPMainWin {
    short x;
    short y;
    int style;
    unsigned short width;
    short height;
    int tag_id;
    unsigned char state;
    unsigned char ribbon_initialized;
    unsigned char unmodeled_12[2];
    void (*close_callback)(void *window, void *arg);
    void *close_callback_arg;
} SPMainWin;
typedef struct SPMainCur {
    unsigned char mode;
    unsigned char unmodeled_01[3];
    short x;
    short y;
    int style;
    unsigned char unmodeled_0c[0x24 - 0x0c];
} SPMainCur;
typedef struct SPMainMsg {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int extent;
    unsigned char unmodeled_0c[4];
    unsigned char color_b;
    unsigned char color_g;
    unsigned char color_r;
    unsigned char unmodeled_13[5];
    char *text;
    unsigned char unmodeled_1c[0x44 - 0x1c];
} SPMainMsg;
typedef struct SPMainNum {
    short x;
    short y;
    int style;
    unsigned char unmodeled_08[4];
    unsigned char font;
    unsigned char unmodeled_0d[0x90 - 0x0d];
} SPMainNum;
typedef struct SPMainView {
    union {
        unsigned char state;
        struct {
            unsigned char unmodeled_00[2];
            unsigned char anim;
            unsigned char unmodeled_03;
        };
    };
    short x;
    short y;
    int style;
    unsigned short width;
    short height;
    int title;
    unsigned char unmodeled_14[8];
    void *items;
    unsigned char unmodeled_20[0x2c - 0x20];
    SPMainWin window;
    unsigned char unmodeled_48[0x1c0 - 0x48];
    SPMainCur cursors[3];
    SPMainMsg messages[24];
    SPMainNum numbers[26];
} SPMainView;

void WindowSPMain(void *arg)
{
    /* w1 */
    SPMainView *list = (SPMainView *)arg;
    int sub;
    int i;

    if (list->items == 0) {
        return;
    }
    switch (list->state & 0xf) {
    case 1:
        sub = list->state >> 4;
        switch (sub) {
        case 0:
            WindowDXSet(&list->window);
            list->window.x = list->x;
            list->window.y = list->y;
            list->window.style = list->style;
            list->window.width = list->width;
            list->window.height = list->height;
            list->window.state = 1;
            list->window.tag_id = list->title;
            for (i = 0; i < 3; i++) {
                eCursolSet(&list->cursors[i], i);
            }
            for (i = 0; i < 24; i++) {
                eMessageSet(&list->messages[i], 0);
            }
            for (i = 0; i < 26; i++) {
                eNumberSet(&list->numbers[i], 0);
            }
            subMWModeExSet(list);
            break;
        case 1:
            WindowDXSet(&list->window);
            list->window.x = list->x;
            list->window.y = list->y;
            list->window.style = list->style;
            list->window.width = list->width;
            list->window.height = list->height;
            list->window.tag_id = list->title;
            list->window.state = sub;
            WindowDXMain(&list->window);
            list->window.state = 3;
            for (i = 0; i < 3; i++) {
                eCursolSet(&list->cursors[i], i);
            }
            for (i = 0; i < 24; i++) {
                eMessageSet(&list->messages[i], 0);
            }
            for (i = 0; i < 26; i++) {
                eNumberSet(&list->numbers[i], 0);
            }
            subMWModeExSet(list);
            break;
        }
        list->state = 2;
    case 2:
        WindowDXMain(&list->window);
        if ((signed char)list->window.state != 3) {
            break;
        }
        list->state = 3;
        list->cursors[0].mode = 32;
        for (i = 0; i < 24; i++) {
            list->messages[i].mode = 32;
            list->numbers[i].font = 3;
        }
        list->anim = 0;
    case 3:
        subMWModeExSet(list);
        subMWPosSet(list);
        subMWDraw(list);
        break;
    case 4:
        list->window.x = list->x;
        list->window.y = list->y;
        list->window.style = list->style;
        list->window.width = list->width;
        list->window.height = list->height;
        list->window.state = 4;
        list->window.tag_id = list->title;
        list->state = 5;
        list->window.close_callback_arg = 0;
        list->window.close_callback = 0;
    case 5:
        WindowDXMain(&list->window);
        if ((signed char)list->window.state == 0) {
            list->state = 0;
        }
        break;
    }
}

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

void WindowSPSetSelect(WindowSPCursor *cursor, WindowSPKeepBuffer keep)
{
    unsigned char k4;

    if (WindowSPSelect((SPSel *)cursor, 0) < 0) return;
    if (cursor->item_count == 0) {
        memset(keep, 0, 5);
        return;
    }
    if (cursor->item_count - 1 < keep[1]) {
        keep[1] = cursor->item_count - 1;
        if ((signed char)cursor->unmodeled_22[1] - 1 < keep[3]) {
            keep[3] = (signed char)cursor->unmodeled_22[1] - 1;
        }
        if ((signed char)cursor->unmodeled_22[0] == 2) {
            keep[2] = keep[1] & 1;
        }
        k4 = keep[4];
        if ((signed char)cursor->unmodeled_25[0] < k4) {
            keep[4] = k4 - 1;
        }
    }
    cursor->selected_index = keep[1];
    cursor->visible_top = keep[2];
    cursor->visible_bottom = keep[3];
    cursor->current_row = keep[4];
}

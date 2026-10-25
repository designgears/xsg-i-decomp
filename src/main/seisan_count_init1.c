#include "common.h"

#include "shared.h"

#include "main/xgl_task.h"

#include "seisan_count_init1.h"

const SeisanRibbonData D_004C7350 = {
    { 0x00000000, 0x0000FFFF, 0x80000000, 0x01C00000,
      0x0000FFFF, 0x80000000, 0x00000200, 0x0000FFFF,
      0x80000000, 0x01C00200, 0x0000FFFF, 0x80000000,
      0x00000000, 0x00000000, 0x00000000 }
};

extern unsigned char *SeisanWork;


extern XglTaskScheduler *SeisanBgTask;


extern XglTaskScheduler *SeisanTask;


extern SeisanResultData *SeisanResult;


extern SeisanCountData SeisanCN;


/* The original eight-byte prompt-pointer table remains in data scaffolding. */

extern const char *msg00_1_0036DAC8[];

void SeisanCountInit1(void)
{
    int chrNo;

    memset(&SeisanCN, 0, sizeof(SeisanCN));
    for (chrNo = 1; chrNo < 13; chrNo++)
    {
        SeisanCN.nextExp[chrNo - 1] = func_A19210(chrNo)->nextExp;
    }
}

typedef struct PortCountTarget {
    int level;
    int hp;
    int ep;
    int nextExp;
    int experience;
    int techPoints;
    int skillPoints;
    int specialPoints;
} PortCountTarget;
typedef struct PortCountData {
    unsigned int state;
    PortCountTarget targets[3];
    unsigned char unmodeled_64[4];
    long countedMoney;
    int nextExp[12];
    unsigned char unmodeled_A0[0x10];
} PortCountData;
typedef struct PortUnit {
    short hp;
    short ep;
    unsigned char unmodeled_04[0x14 - 4];
    signed char level;
} PortUnit;
typedef struct PortChar {
    int experience;
    int nextExp;
    unsigned char unmodeled_08[4];
    int techPoints;
    int skillPoints;
    int specialPoints;
} PortChar;
typedef struct PortHis {
    int characterNo;
    int experience;
    int techPoints;
    int skillPoints;
    int specialPoints;
    int level;
    int hp;
    int ep;
    int learnedIds[6];
    int count;
} PortHis;
typedef struct PortResult {
    PortHis entries[3];
} PortResult;
extern PortUnit *PortGetUnit(int chrNo) __asm__("func_A191C0");
extern PortChar *PortGetChar(int chrNo) __asm__("func_A19210");
#define SeisanCN (*(PortCountData *)&SeisanCN)
#define SeisanResult ((PortResult *)SeisanResult)
static inline void SeisanRollbackStats(PortUnit *unit,
                                      PortChar *character,
                                      const PortHis *row)
{
    unit->hp -= row->hp;
    unit->ep -= row->ep;
    character->techPoints -= row->techPoints;
    character->skillPoints -= row->skillPoints;
    character->specialPoints -= row->specialPoints;
    unit->level -= row->level;
}

void SeisanCountInit2(void)
{
    int i;
    PortHis *result = SeisanResult->entries;

    SeisanCN.state = 0;
    for (i = 0; i < 3; i++) {
        int characterNo = result[i].characterNo;
        if ((unsigned int)(characterNo - 1) < 12) {
            PortUnit *unit = PortGetUnit(characterNo);
            PortChar *character = PortGetChar(characterNo);

            SeisanCN.targets[i].level = unit->level;
            SeisanCN.targets[i].hp = unit->hp;
            SeisanCN.targets[i].ep = unit->ep;
            {
                SeisanCN.targets[i].experience = character->experience;
                SeisanCN.targets[i].nextExp = character->nextExp;
                SeisanCN.targets[i].techPoints = character->techPoints;
                SeisanCN.targets[i].skillPoints = character->skillPoints;
                SeisanCN.targets[i].specialPoints = character->specialPoints;
                SeisanRollbackStats(unit, character, &SeisanResult->entries[i]);
                {
                    PortResult *freshResults = SeisanResult;
                    result = freshResults->entries;
                    character->experience -= (freshResults->entries + i)->experience;
                }
                character->nextExp = SeisanCN.nextExp[characterNo - 1];
            }
        }
    }
    SeisanCN.countedMoney = 0;
}

int SeisanNumberCount(int current, int target, int step, int *moving)
{
  int overshot;
  int next;
  next = current;
  if (next != target)
  {
    if (next < target)
    {
      next += step;
      overshot = target < next;
    }
    else
    {
      next -= step;
      overshot = next < target;
    }
    if (overshot)
    {
      next = target;
    }
    else
    {
      *moving = 1;
    }
  }
  return next;
}

typedef struct SeisanMoneyDataTE {
    unsigned char unmodeled_00[0x10];
    int money;
    int itemTypes[8];
    int itemIds[8];
    int itemCount;
} SeisanMoneyDataTE;

extern SeisanMoneyDataTE *SeisanMg;

typedef struct { int v; } __attribute__((packed)) SeisanExpEnt;
extern SeisanExpEnt *dataExpTblGet(int cid) __asm__("func_A19338");
extern void xglSoundEffectNormalID(int id, int arg);
extern void xglSoundEffectStopID(int id, int arg);

void SeisanCountMain(int mode)
{
    int moving;
    SeisanExpEnt *tbl;
    int i;
    int chrNo;
    PortUnit *unit;
    PortChar *character;

    tbl = dataExpTblGet(0);
    moving = 0;
    switch (SeisanCN.state) {
    case 0:
    case 3:
        xglSoundEffectStopID(0xE, 0);
        break;
    case 1:
        for (i = 0; i < 3; i++) {
            chrNo = ((PortHis *)SeisanResult)[i].characterNo;
            if ((unsigned int)(chrNo - 1) < 12) {
                unit = PortGetUnit(chrNo);
                character = PortGetChar(chrNo);
                unit->hp = SeisanNumberCount(unit->hp, SeisanCN.targets[i].hp, 1, &moving);
                unit->ep = SeisanNumberCount(unit->ep, SeisanCN.targets[i].ep, 1, &moving);
                character->techPoints = SeisanNumberCount(character->techPoints, SeisanCN.targets[i].techPoints, 1, &moving);
                character->skillPoints = SeisanNumberCount(character->skillPoints, SeisanCN.targets[i].skillPoints, 1, &moving);
                character->specialPoints = SeisanNumberCount(character->specialPoints, SeisanCN.targets[i].specialPoints, 1, &moving);
                character->experience = SeisanNumberCount(character->experience, SeisanCN.targets[i].experience, 2, &moving);
                if (unit->level != SeisanCN.targets[i].level) {
                    character->nextExp -= 2;
                    if (character->nextExp <= 0) {
                        unit->level = SeisanNumberCount(unit->level, SeisanCN.targets[i].level, 1, &moving);
                        character->nextExp = tbl[unit->level].v + character->nextExp;
                        xglSoundEffectNormalID(4, 0);
                    }
                } else {
                    character->nextExp = SeisanNumberCount(character->nextExp, SeisanCN.targets[i].nextExp, 2, &moving);
                }
            }
        }
        if (SeisanMg->money != SeisanCN.countedMoney) {
            SeisanCN.countedMoney += 5;
            if (SeisanCN.countedMoney >= SeisanMg->money) {
                SeisanCN.countedMoney = SeisanMg->money;
            } else {
                moving = 1;
            }
        }
        xglSoundEffectNormalID(0xE, 0);
        if (moving == 0) {
            SeisanCN.state = 3;
        }
        break;
    case 2:
        for (i = 0; i < 3; i++) {
            chrNo = ((PortHis *)SeisanResult)[i].characterNo;
            if ((unsigned int)(chrNo - 1) < 12 && ((PortHis *)SeisanResult)[i].level != 0) {
                xglSoundEffectNormalID(4, 0);
                break;
            }
        }
        for (i = 0; i < 3; i++) {
            chrNo = ((PortHis *)SeisanResult)[i].characterNo;
            if ((unsigned int)(chrNo - 1) < 12) {
                unit = PortGetUnit(chrNo);
                character = PortGetChar(chrNo);
                unit->hp = SeisanCN.targets[i].hp;
                unit->ep = SeisanCN.targets[i].ep;
                character->techPoints = SeisanCN.targets[i].techPoints;
                character->skillPoints = SeisanCN.targets[i].skillPoints;
                character->specialPoints = SeisanCN.targets[i].specialPoints;
                character->experience = SeisanCN.targets[i].experience;
                character->nextExp = SeisanCN.targets[i].nextExp;
                unit->level = SeisanCN.targets[i].level;
            }
        }
        SeisanCN.countedMoney = SeisanMg->money;
        xglSoundEffectNormalID(0xE, 0);
        SeisanCN.state = 3;
        break;
    }
}

void TskObjectSet2(TskObject *task, TskObjectWorker worker, void *data)
{
    task->data = data;
    task->worker = worker;
    task->state = 0;
}

void tskTskMain2(TskObject *task)
{
    TskObjectWorker worker = task->worker;

    if (SeisanWork[1] == 0xff) {
        xglTaskWaitRemove(&task->base);
        return;
    }

    if (task->state != 0) {
        if (task->state != 2)
            return;
    } else {
        worker(task, task->data);
        task->state = 2;
    }

    worker(task, task->data);
}

typedef struct SeisanControlStateTE {
    unsigned char unmodeled_00;
    unsigned char state;
    unsigned char flags;
    unsigned char requestedState;
    unsigned char selectedState;
    unsigned char closingTicks;
    unsigned char partySize;
    unsigned char unmodeled_07[64 - 7];
} SeisanControlStateTE;
typedef struct SeisanTimeWindowTE {
    short x;
    short y;
    int depth;
    short width;
    short height;
    const char *title;
    signed char state;
    unsigned char ribbonInitialized;
    unsigned char unmodeled_12[2];
    void (*callback)(struct SeisanTimeWindowTE *window, void *argument);
    void *callbackArgument;
    unsigned char unmodeled_1c[0x194 - 0x1c];
} SeisanTimeWindowTE;
typedef struct SeisanTimeTagTE {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int depth;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
    unsigned char unmodeled_10[0x20 - 0x10];
} SeisanTimeTagTE;
typedef struct SeisanTimeNumberTE {
    short x;
    short y;
    int depth;
    unsigned char unmodeled_08[6];
    unsigned char option;
    unsigned char digits;
    unsigned char unmodeled_10[4];
    int value;
    unsigned char unmodeled_18[0x90 - 0x18];
} SeisanTimeNumberTE;
typedef struct SeisanTimeSpriteTE {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int depth;
    unsigned char unmodeled_0c[0x28 - 0x0c];
} SeisanTimeSpriteTE;
typedef struct SeisanTimePanelTE {
    unsigned char unmodeled_00[4];
    int depth;
    SeisanTimeWindowTE window;
    SeisanTimeTagTE tags[2];
    SeisanTimeNumberTE numbers[3];
    SeisanTimeSpriteTE sprites[2];
} SeisanTimePanelTE;
extern const char *msg00_0_0036DAB8[3];
extern void WindowDXSet(void *window);
extern void WindowDXMain(void *window);
extern void eTagFontSet(void *tag, const char *title);
extern void eTagFontMain(void *tag);
extern void eNumberSet(void *number, int value);
extern void eNumberMain(void *number);
extern void eSpriteMain(void *sprite);
extern unsigned int func_A22908(void);
extern void xglClockUInt2DayTime(XglClock *clock, unsigned int seconds);
extern int PartyTimeLimitCheck(XglClock *clock);
extern void PartyTimeDispChange(XglClock *clock);
extern int dataMoneyBoxChk(void);

static void SeisanTimeEx(TskObject *task, SeisanTimePanelTE *panel)
{
    XglClock elapsed;
    short targetY;
    int i;

    switch (task->state) {
    case 0:
        panel->depth = 0xfffff0;
        WindowDXSet(&panel->window);
        panel->window.x = 16;
        panel->window.y = -64;
        panel->window.depth = panel->depth;
        panel->window.width = 480;
        panel->window.height = 32;
        panel->window.title = msg00_0_0036DAB8[0];
        panel->window.state = 1;
        WindowDXMain(&panel->window);
        panel->window.state = 3;
        for (i = 0; i < 2; i++) {
            eTagFontSet(&panel->tags[i], msg00_0_0036DAB8[i + 1]);
            panel->tags[i].depth = panel->depth + 2;
        }
        for (i = 0; i < 3; i++) {
            eNumberSet(&panel->numbers[i], 0);
            panel->numbers[i].depth = panel->depth + 2;
        }
        xglClockUInt2DayTime(&elapsed, func_A22908() / 30);
        PartyTimeLimitCheck(&elapsed);
        PartyTimeDispChange(&elapsed);
        panel->numbers[0].option = 2;
        /* The number receives the first four display-clock bytes as one word. */
        __builtin_memcpy(&panel->numbers[0].value, &elapsed, sizeof(int));
        {
            int money = dataMoneyBoxChk();
            panel->numbers[1].digits = 9;
            panel->numbers[1].option = 0;
            panel->numbers[1].value = money;
        }
        panel->numbers[2].value = SeisanMg->money;
        panel->numbers[2].digits = panel->numbers[1].digits;
        panel->numbers[2].option = 7;
        for (i = 0; i < 2; i++) {
            eSpriteSet(&panel->sprites[i], i + 0x707);
            panel->sprites[i].depth = panel->depth + 2;
        }
        break;
    case 2: {
        SeisanControlStateTE *control = (SeisanControlStateTE *)SeisanWork;
        targetY = 32;
        if (control->state == SEISAN_STATE_CLOSING)
            targetY = -64;
        MoveSlide(&panel->window.y, &targetY, 3.0f);
        WindowDXMain(&panel->window);
        panel->numbers[0].x = panel->window.x + 91;
        panel->numbers[0].y = panel->window.y + 8;
        eNumberMain(&panel->numbers[0]);
        panel->tags[0].x = panel->numbers[0].x - 8;
        panel->tags[0].y = panel->window.y + 8;
        eTagFontMain(&panel->tags[0]);
        panel->numbers[1].x = panel->window.x + 238;
        panel->numbers[1].y = panel->window.y + 8;
        panel->numbers[1].value = dataMoneyBoxChk() - SeisanMg->money + SeisanCN.countedMoney;
        eNumberMain(&panel->numbers[1]);
        panel->tags[1].x = panel->numbers[1].x - 8;
        panel->tags[1].y = panel->window.y + 8;
        eTagFontMain(&panel->tags[1]);
        panel->sprites[0].x = panel->window.x + 340;
        panel->sprites[0].y = panel->numbers[2].y = panel->window.y + 8;
        eSpriteMain(&panel->sprites[0]);
        panel->numbers[2].x = panel->window.x + 350;
        panel->numbers[2].value = SeisanCN.countedMoney;
        panel->numbers[2].y = panel->window.y + 8;
        eNumberMain(&panel->numbers[2]);
        panel->sprites[1].x = panel->window.x + 461;
        panel->sprites[1].y = panel->numbers[2].y = panel->window.y + 8;
        eSpriteMain(&panel->sprites[1]);
        break;
    }
    }
}

static void tskSeisanButton(TskObject *task, SeisanButtonWork *button)
{
    short targetY;

    switch (task->state) {
    case 0: {
        button->color = 0xfffff0;
        eSpriteSet(&button->sprite, 0x200);
        button->sprite.x = -0x70;
        button->sprite.y = 0x1a4;
        button->sprite.color = button->color;
        eMessageSet(&button->message, msg00_1_0036DAC8[0]);
        button->message.mode = 0x20;
        button->message.x = button->sprite.x + 0x14;
        button->message.y = button->sprite.y;
        button->message.color = button->color;
        break;
    }
    case 2: {
        unsigned char *work = SeisanWork;
        unsigned char flags = work[SEISAN_WORK_FLAGS];
        targetY = 0x180;
        if (flags & 4)
            button->message.text = msg00_1_0036DAC8[1];
        if (work[SEISAN_WORK_STATE] == SEISAN_STATE_CLOSING)
            targetY = 0x220;
        MoveSlide(&button->sprite.x, &targetY, 3.0f);
        eSpriteMain(&button->sprite);
        button->message.x = button->sprite.x + 0x14;
        button->message.y = button->sprite.y;
        eMessageMain(&button->message);
        break;
    }
    }
}

typedef struct SeisanTimeWindowSI {
    short x;
    short y;
    int depth;
    short width;
    short height;
    const char *title;
    signed char state;
    unsigned char ribbonInitialized;
    unsigned char unmodeled_12[2];
    void (*callback)(struct SeisanTimeWindowSI *window, void *argument);
    void *callbackArgument;
    unsigned char unmodeled_1c[0x194 - 0x1c];
} SeisanTimeWindowSI;
typedef struct SeisanItemPanelSI {
    unsigned char unmodeled_00;
    unsigned char active;
    unsigned char itemCount;
    unsigned char unmodeled_03;
    int depth;
    SeisanTimeWindowSI windows[2];
    SeisanButtonMessage title;
    SeisanButtonMessage items[8];
} SeisanItemPanelSI;
typedef struct SeisanItemEntrySI {
    int itemId;
    int count;
} SeisanItemEntrySI;
typedef struct SeisanItemTextEntrySI {
    const char *name;
} SeisanItemTextEntrySI;
typedef struct SeisanMoneyDataSI {
    unsigned char unmodeled_00[0x54];
    int itemCount;
} SeisanMoneyDataSI;
typedef struct SeisanLabelBytesSI {
    unsigned char bytes[5];
} SeisanLabelBytesSI;
extern SeisanItemEntrySI *SeisanItemBox;
extern const char *msg00_2_0036DAD0[];
extern const unsigned short *msg_number[10];
extern const char D_004DB090[];
extern const char D_004DB098[];
extern SeisanItemTextEntrySI *MenuTextGet(int index);
extern void xglFontPrint(int x, int y, int color, const char *text);

static void tskSeisanItem(TskObject *task, void *data)
{
    SeisanItemPanelSI *panel = data;
    unsigned short ordinalText[3];
    unsigned short quantityText[3];
    short targetX;
    int i;

    switch (task->state) {
    case 0:
        panel->active = 0;
        panel->itemCount = 0;
        panel->depth = 0xfffff0;
        for (i = 0; i < SeisanMg->itemCount; i++) {
            if (SeisanItemBox[i].count != 0)
                panel->itemCount++;
        }
        panel->windows[0].y = 128;
        panel->windows[0].height = 32;
        panel->windows[1].y = 160;
        panel->windows[1].height = panel->itemCount * 24 + 6;
        for (i = 0; i < 2; i++) {
            WindowDXSet(&panel->windows[i]);
            panel->windows[i].x = 832;
            panel->windows[i].width = 288;
            panel->windows[i].depth = panel->depth;
            panel->windows[i].state = 1;
            WindowDXMain(&panel->windows[i]);
            panel->windows[i].state = 3;
        }
        if (panel->itemCount)
            eMessageSet(&panel->title, msg00_2_0036DAD0[0]);
        else
            eMessageSet(&panel->title, msg00_2_0036DAD0[1]);
        panel->title.mode = 32;
        panel->title.color = panel->depth + 2;
        for (i = 0; i < panel->itemCount; i++) {
            eMessageSet(&panel->items[i], MenuTextGet(SeisanItemBox[i].itemId)->name);
            panel->items[i].mode = 32;
            panel->items[i].color = panel->depth + 2;
        }
        break;
    case 2:
        targetX = 112;
        switch (SeisanWork[SEISAN_WORK_STATE]) {
        case 0x20:
        case 0x30:
        case 0x50:
        case 0x58:
        case 0x60:
            targetX = 832;
            break;
        case 0x80:
            panel->active = 1;
            break;
        case SEISAN_STATE_CLOSING:
            targetX = -320;
            break;
        }
        if (panel->active) {
            MoveSlide(&panel->windows[0].x, &targetX, 3.0f);
            WindowDXMain(&panel->windows[0]);
            panel->title.x = panel->windows[0].x + 64;
            panel->title.y = panel->windows[0].y + 3;
            if (panel->itemCount == 0)
                panel->title.x += 20;
            eMessageMain(&panel->title);
            if (panel->itemCount) {
                panel->windows[1].x = panel->windows[0].x;
                WindowDXMain(&panel->windows[1]);
                for (i = 0; i < panel->itemCount; i++) {
                    const unsigned short *ordinalSource;
                    const unsigned short *quantitySource;
                    unsigned short ordinalGlyph;
                    unsigned short quantityGlyph;
                    int printX;
                    int printY;
                    panel->items[i].x = panel->windows[1].x + 43;
                    panel->items[i].y = panel->windows[1].y + i * 24 + 3;
                    eMessageMain(&panel->items[i]);
                    /* Two-byte glyph pairs followed by the original terminating byte. */
                    ordinalSource = msg_number[i + 1];
                    printY = panel->windows[1].y + i * 24 + 2;
                    printX = panel->windows[1].x + 3;
                    ordinalGlyph = ordinalSource[0];
                    __builtin_memcpy(ordinalText, D_004DB090, 5);
                    ordinalText[0] = ordinalGlyph;
                    xglFontPrint(printX, printY, 0xffffff, (const char *)ordinalText);
                    quantitySource = msg_number[SeisanItemBox[i].count];
                    printY = panel->windows[1].y + i * 24 + 2;
                    printX = panel->windows[1].x + 221;
                    *(SeisanLabelBytesSI *)quantityText = *(const SeisanLabelBytesSI *)D_004DB098;
                    quantityGlyph = quantitySource[0];
                    quantityText[1] = quantityGlyph;
                    xglFontPrint(printX, printY, 0xffffff, (const char *)quantityText);
                }
            }
        }
        break;
    }
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct SeisanControlState {
    unsigned char unmodeled_00;
    unsigned char state;
    unsigned char flags;
    unsigned char requestedState;
    unsigned char selectedState;
    unsigned char closingTicks;
    unsigned char partySize;
    unsigned char unmodeled_07[64 - 7];
} SeisanControlState;

typedef struct SeisanUnitRecord {
    short hp;
    short ep;
    unsigned char unmodeled_04[0x14 - 4];
    signed char level;
} SeisanUnitRecord;

typedef struct SeisanFadeVertex {
    short x;
    short y;
    int depth;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
} SeisanFadeVertex;

typedef struct SeisanFadeData {
    unsigned char state;
    unsigned char mode;
    unsigned char unmodeled_02[2];
    SeisanFadeVertex vertices[4];
    unsigned char unmodeled_34[4];
    int terminatorDepth;
    unsigned char unmodeled_3c[4];  /* SeisanInit advances 64 bytes after this block. */
} SeisanFadeData;

typedef struct SeisanTimeWindow {
    short x;
    short y;
    int depth;
    short width;
    short height;
    const char *title;
    signed char state;
    unsigned char ribbonInitialized;
    unsigned char unmodeled_12[2];
    void (*callback)(struct SeisanTimeWindow *window, void *argument);
    void *callbackArgument;
    unsigned char unmodeled_1c[0x194 - 0x1c];
} SeisanTimeWindow;

typedef struct SeisanTimeTag {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int depth;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
    unsigned char unmodeled_10[0x20 - 0x10];
} SeisanTimeTag;

typedef struct SeisanTimeNumber {
    short x;
    short y;
    int depth;
    unsigned char unmodeled_08[6];
    unsigned char option;
    unsigned char digits;
    unsigned char unmodeled_10[4];
    int value;
    unsigned char unmodeled_18[0x90 - 0x18];
} SeisanTimeNumber;

typedef struct SeisanTimeSprite {
    unsigned char unmodeled_00[4];
    short x;
    short y;
    int depth;
    unsigned char unmodeled_0c[0x28 - 0x0c];
} SeisanTimeSprite;

typedef struct SeisanTimePanel {
    unsigned char unmodeled_00[4];
    int depth;
    SeisanTimeWindow window;
    SeisanTimeTag tags[2];
    SeisanTimeNumber numbers[3];
    SeisanTimeSprite sprites[2];
} SeisanTimePanel;

typedef struct SeisanItemPanel {
    unsigned char unmodeled_00;
    unsigned char active;
    unsigned char itemCount;
    unsigned char unmodeled_03;
    int depth;
    SeisanTimeWindow windows[2];
    SeisanButtonMessage title;
    SeisanButtonMessage items[8];
} SeisanItemPanel;

typedef struct SeisanItemEntry {
    int itemId;
    int count;
} SeisanItemEntry;

typedef struct SeisanItemTextEntry {
    const char *name;
} SeisanItemTextEntry;

typedef struct SeisanMoneyData {
    unsigned char unmodeled_00[0x10];
    int money;
    int itemTypes[8];                /* +0x14, written by monsGainSet */
    int itemIds[8];                  /* +0x34, low halfword read by SeisanInit */
    int itemCount;
} SeisanMoneyData;

typedef struct SeisanLabelBytes {
    unsigned char bytes[5];
} SeisanLabelBytes;

typedef struct SeisanCharRecord_SS {
    int experience;
    int nextExp;                    /* +0x04 */
    unsigned char unmodeled_08[4];
    int techPoints;
    int skillPoints;
    int specialPoints;
} SeisanCharRecord_SS;
typedef struct SeisanHissatuEntry_SS {
    int characterNo;
    int experience;
    int techPoints;
    int skillPoints;
    int specialPoints;
    int level;
    int hp;
    int ep;
    int learnedIds[6];               /* +0x20: names fetched by tskSeisanStatus */
    int count;                      /* +0x38 in each 0x3c-byte result row */
} SeisanHissatuEntry_SS;
typedef struct SeisanResultData_SS {
    SeisanHissatuEntry_SS entries[3];  /* Three rows: SeisanInit clears 180 bytes. */
} SeisanResultData_SS;
typedef struct SeisanUnitRecord_SS {
    short hp;
    short ep;
    unsigned char unmodeled_04[0x14 - 4];
    signed char level;
} SeisanUnitRecord_SS;
#define SEISAN_WORK_STATE 1
#define SEISAN_STATE_CLOSING 0xf0
typedef struct SeisanStatusProfile {
    signed char mode;
    unsigned char flags;
    short characterNo;
    short x;
    short y;
    unsigned char unmodeled_08[0x840 - 8];
} SeisanStatusProfile;
typedef struct SeisanStatusSprite {
    short x;
    short y;
    int depth;
    short width;
    short height;
    unsigned char color[4];
    short uv;
    unsigned char column;
    unsigned char row;
} SeisanStatusSprite;
typedef struct SeisanStatusPanel {
    unsigned char state;
    unsigned char displayFlags;
    unsigned char enabled;
    unsigned char partySlot;
    unsigned char learnedVisible;
    unsigned char unmodeled_05[3];
    unsigned short previousLevel;
    unsigned short levelFlashTimer;
    int depth;
    SeisanTimeWindow window;
    SeisanStatusProfile profile;
    SeisanTimeNumber numbers[10];
    SeisanTimeTag tags[4];
    SeisanButtonMessage learnedTitle;
    SeisanButtonMessage learned[6];
    SeisanFadeVertex separator[3];
    SeisanStatusSprite sprites[14];
    SeisanFadeVertex highlight[5];
} SeisanStatusPanel;
extern void MenuStatusDisp(SeisanTimeWindow *window, void *argument);
extern void endSpriteSet(void *sprite, int flags);
extern SeisanItemTextEntry *func_A2C7D8(int abilityId);
extern const char D_004C7340[];
extern const char *msg00_3_0036DAD8[];
extern const char *msg01_4_0036DAE8[];
static inline int SeisanLearnedCount(int partySlot)
{
    const SeisanHissatuEntry_SS *results = ((SeisanResultData_SS *)SeisanResult)->entries;
    return results[partySlot].count;
}
static inline int SeisanLearnedId(const SeisanHissatuEntry_SS *results,
                                  int partySlot, int learnedIndex)
{
    return results[partySlot].learnedIds[learnedIndex];
}
typedef union SeisanNumberCoordinates {
    struct { short x, y; } axes;
    unsigned int packed;
} SeisanNumberCoordinates;

static void tskSeisanStatus(TskObject *task, void *data)
{
    extern unsigned char * SeisanWork_SS __asm__("SeisanWork");
    SeisanStatusPanel *panel = data;
    short targetX;
    int i;
    int columnX;
    int baselineY;
    SeisanHissatuEntry_SS *entries;

    switch (task->state) {
    case 0:
        panel->enabled = 0;
        panel->displayFlags = 1;
        panel->depth = 0xfffff0;
        {
            SeisanHissatuEntry_SS *initialEntries = ((SeisanResultData_SS *)SeisanResult)->entries;
            if ((unsigned int)(initialEntries[panel->partySlot].characterNo - 1) >= 12) {
                panel->state = 3;
                break;
            }
        }
        panel->enabled = 1;
        WindowDXSet(&panel->window);
        panel->window.x = 1024;
        panel->window.ribbonInitialized = 1;
        panel->window.depth = panel->depth;
        panel->window.width = 480;
        panel->window.height = 99;
        panel->window.y = panel->partySlot * 116 + 88;
        panel->window.title = D_004C7340;
        panel->window.callback = MenuStatusDisp;
        panel->window.callbackArgument = &panel->profile;
        panel->profile.mode = -1;
        panel->profile.x = 0;
        panel->profile.y = 0;
        {
            SeisanHissatuEntry_SS *profileEntries = ((SeisanResultData_SS *)SeisanResult)->entries;
            panel->profile.characterNo = profileEntries[panel->partySlot].characterNo;
        }
        panel->window.state = 1;
        panel->profile.flags = 0;
        WindowDXMain(&panel->window);
        panel->window.state = 3;
        for (i = 0; i < 10; i++) {
            eNumberSet(&panel->numbers[i], 0);
            panel->numbers[i].depth = 0xffffff;
        }
        for (i = 0; i < 4; i++) {
            eTagFontSet(&panel->tags[i], msg00_3_0036DAD8[i]);
            panel->tags[i].depth = 0xffffff;
            if (i == 3)
                continue;
            panel->tags[i].red = 112;
        }
        eMessageSet(&panel->learnedTitle, 0);
        panel->learnedTitle.color = 0xffffff;
        panel->learnedTitle.mode = 32;
        for (i = 0; i < 6; i++) {
            eMessageSet(&panel->learned[i], 0);
            panel->learned[i].color = 0xffffff;
            panel->learned[i].mode = 32;
        }
        for (i = 0; i < 14; i++) {
            panel->sprites[i].uv = (i & 1) + 0x707;
            endSpriteSet(&panel->sprites[i], 255);
            panel->sprites[i].depth = 0xffffff;
        }
        panel->learnedVisible = 0;
        panel->levelFlashTimer = 0;
        {
            SeisanHissatuEntry_SS *levelEntries = ((SeisanResultData_SS *)SeisanResult)->entries;
            panel->previousLevel = ((SeisanUnitRecord_SS *(*)(int))PortGetUnit)(levelEntries[panel->partySlot].characterNo)->level;
        }
        break;
    case 2: {
        SeisanUnitRecord_SS *unit;
        SeisanCharRecord_SS *character;
        SeisanTimeWindow *window;

        if (!panel->enabled)
            break;
        targetX = 16;
        {
            SeisanHissatuEntry_SS *unitEntries = ((SeisanResultData_SS *)SeisanResult)->entries;
            unit = ((SeisanUnitRecord_SS *(*)(int))PortGetUnit)(unitEntries[panel->partySlot].characterNo);
        }
        {
            unsigned int partySlot = panel->partySlot;
            SeisanHissatuEntry_SS *characterEntries = ((SeisanResultData_SS *)SeisanResult)->entries;
            character = ((SeisanCharRecord_SS *(*)(int))func_A19210)(characterEntries[partySlot].characterNo);
        }
        if (panel->levelFlashTimer)
            panel->levelFlashTimer--;
        switch (SeisanWork_SS[SEISAN_WORK_STATE]) {
        case 0x20:
            if (panel->previousLevel != unit->level) {
                panel->previousLevel = unit->level;
                panel->levelFlashTimer = 32;
            }
            break;
        case 0x50:
        case 0x58: {
            if (SeisanLearnedCount(panel->partySlot) != 0) {
                const char *learnedTitle = msg01_4_0036DAE8[0];
                panel->profile.mode = -2;
                panel->displayFlags = 16;
                panel->learnedTitle.text = learnedTitle;
                for (i = 0; i < SeisanLearnedCount(panel->partySlot); i++) {
                    panel->learned[i].text = func_A2C7D8(
                        SeisanLearnedId(((SeisanResultData_SS *)SeisanResult)->entries, panel->partySlot, i))->name;
                }
                for (; i < 6; i++)
                    panel->learned[i].text = 0;
                panel->learnedVisible = 1;
            } else {
                targetX = -512;
            }
            break;
        }
        case 0x60:
            panel->learnedTitle.text = msg01_4_0036DAE8[1];
            panel->displayFlags = 16;
            panel->profile.mode = 0;
            break;
        case 0x80:
        case SEISAN_STATE_CLOSING:
            targetX = -512;
            break;
        }
        window = &panel->window;
        MoveSlide(&window->x, &targetX, 3.0f);
        WindowDXMain(window);
        if (panel->displayFlags & 1) {
            {
                columnX = panel->window.x + 228;
                baselineY = panel->window.y + 31;

                panel->sprites[0].x = columnX - 11;
                panel->sprites[0].y = baselineY - 1;
                endPrintExtFunc(panel->depth, 2, &panel->sprites[0]);
                panel->numbers[0].x = columnX;
                ((SeisanNumberCoordinates *)(void *)&panel->numbers[0])->axes.y = baselineY + 1;
                entries = ((SeisanResultData_SS *)SeisanResult)->entries;
                panel->numbers[0].value = entries[panel->partySlot].hp -
                    (SeisanCN.targets[panel->partySlot].hp - unit->hp);
                if (panel->numbers[0].value != 0) {
                    panel->numbers[0].digits = 4;
                    panel->numbers[0].option = 8;
                } else {
                    panel->numbers[0].digits = 5;
                    panel->numbers[0].option = 9;
                }
                eNumberMain(&panel->numbers[0]);
                panel->sprites[1].x = columnX + 54;
                panel->sprites[1].y = baselineY - 1;
                endPrintExtFunc(panel->depth, 2, &panel->sprites[1]);
            }
            {
                columnX = panel->window.x + 228;
                baselineY = panel->window.y + 47;

                panel->sprites[2].x = columnX - 11;
                panel->sprites[2].y = baselineY - 1;
                endPrintExtFunc(panel->depth, 2, &panel->sprites[2]);
                panel->numbers[1].x = columnX;
                panel->numbers[1].y = baselineY + 1;
                entries = ((SeisanResultData_SS *)SeisanResult)->entries;
                panel->numbers[1].value = entries[panel->partySlot].ep -
                    (SeisanCN.targets[panel->partySlot].ep - unit->ep);
                if (panel->numbers[1].value != 0) {
                    panel->numbers[1].digits = 2;
                    panel->numbers[1].option = 8;
                } else {
                    panel->numbers[1].digits = 3;
                    panel->numbers[1].option = 9;
                }
                eNumberMain(&panel->numbers[1]);
                panel->sprites[3].x = columnX + 34;
                panel->sprites[3].y = baselineY - 1;
                endPrintExtFunc(panel->depth, 2, &panel->sprites[3]);
            }
            panel->tags[0].x = panel->window.x + 344;
            panel->tags[0].y = panel->window.y + 30;
            eTagFontMain(&panel->tags[0]);
            panel->numbers[2].x = panel->window.x + 352;
            panel->numbers[2].value = character->techPoints;
            panel->numbers[2].y = panel->window.y + 32;
            panel->numbers[2].option = 1;
            panel->numbers[2].digits = 4;
            eNumberMain(&panel->numbers[2]);
            panel->tags[1].x = panel->window.x + 344;
            panel->tags[1].y = panel->window.y + 46;
            eTagFontMain(&panel->tags[1]);
            panel->numbers[3].x = panel->window.x + 352;
            panel->numbers[3].value = character->skillPoints;
            panel->numbers[3].y = panel->window.y + 48;
            panel->numbers[3].option = 1;
            panel->numbers[3].digits = 4;
            eNumberMain(&panel->numbers[3]);
            panel->tags[2].x = panel->window.x + 344;
            panel->tags[2].y = panel->window.y + 62;
            eTagFontMain(&panel->tags[2]);
            panel->numbers[4].x = panel->window.x + 352;
            panel->numbers[4].y = panel->window.y + 64;
            panel->numbers[4].value = character->specialPoints;
            panel->numbers[4].option = 1;
            panel->numbers[4].digits = 4;
            eNumberMain(&panel->numbers[4]);
            {
                columnX = panel->window.x + 409;
                baselineY = panel->window.y + 31;

                panel->sprites[4].x = columnX - 11;
                panel->sprites[4].y = baselineY - 1;
                endPrintExtFunc(panel->depth, 2, &panel->sprites[4]);
                panel->numbers[5].x = columnX;
                panel->numbers[5].y = baselineY + 1;
                entries = ((SeisanResultData_SS *)SeisanResult)->entries;
                panel->numbers[5].value = entries[panel->partySlot].techPoints -
                    (SeisanCN.targets[panel->partySlot].techPoints - character->techPoints);
                if (panel->numbers[5].value != 0) {
                    panel->numbers[5].digits = 4;
                    panel->numbers[5].option = 8;
                } else {
                    panel->numbers[5].digits = 5;
                    panel->numbers[5].option = 9;
                }
                eNumberMain(&panel->numbers[5]);
                panel->sprites[5].x = columnX + 54;
                panel->sprites[5].y = baselineY - 1;
                endPrintExtFunc(panel->depth, 2, &panel->sprites[5]);
            }
            {
                columnX = panel->window.x + 409;
                baselineY = panel->window.y + 47;

                panel->sprites[6].x = columnX - 11;
                panel->sprites[6].y = baselineY - 1;
                endPrintExtFunc(panel->depth, 2, &panel->sprites[6]);
                panel->numbers[6].x = columnX;
                panel->numbers[6].y = baselineY + 1;
                entries = ((SeisanResultData_SS *)SeisanResult)->entries;
                panel->numbers[6].value = entries[panel->partySlot].skillPoints -
                    (SeisanCN.targets[panel->partySlot].skillPoints - character->skillPoints);
                if (panel->numbers[6].value != 0) {
                    panel->numbers[6].digits = 4;
                    panel->numbers[6].option = 8;
                } else {
                    panel->numbers[6].digits = 5;
                    panel->numbers[6].option = 9;
                }
                eNumberMain(&panel->numbers[6]);
                panel->sprites[7].x = columnX + 54;
                panel->sprites[7].y = baselineY - 1;
                endPrintExtFunc(panel->depth, 2, &panel->sprites[7]);
            }
            {
                columnX = panel->window.x + 409;
                baselineY = panel->window.y + 63;

                panel->sprites[8].x = columnX - 11;
                panel->sprites[8].y = baselineY - 1;
                endPrintExtFunc(panel->depth, 2, &panel->sprites[8]);
                panel->numbers[7].x = columnX;
                panel->numbers[7].y = baselineY + 1;
                entries = ((SeisanResultData_SS *)SeisanResult)->entries;
                panel->numbers[7].value = entries[panel->partySlot].specialPoints -
                    (SeisanCN.targets[panel->partySlot].specialPoints - character->specialPoints);
                if (panel->numbers[7].value != 0) {
                    panel->numbers[7].digits = 4;
                    panel->numbers[7].option = 8;
                } else {
                    panel->numbers[7].digits = 5;
                    panel->numbers[7].option = 9;
                }
                eNumberMain(&panel->numbers[7]);
                panel->sprites[9].x = columnX + 54;
                panel->sprites[9].y = baselineY - 1;
                endPrintExtFunc(panel->depth, 2, &panel->sprites[9]);
            }
            {
                columnX = panel->window.x + 228;
                baselineY = panel->window.y + 64;

                panel->sprites[10].x = columnX - 11;
                panel->sprites[10].y = baselineY - 1;
                endPrintExtFunc(panel->depth, 2, &panel->sprites[10]);
                panel->numbers[8].x = columnX;
                panel->numbers[8].y = baselineY + 1;
                if (unit->level == 99) {
                    panel->numbers[8].value = -1;
                    panel->numbers[8].option = 1;
                    panel->numbers[8].digits = 6;
                } else {
                    entries = ((SeisanResultData_SS *)SeisanResult)->entries;
                    panel->numbers[8].value = (SeisanCN.targets[panel->partySlot].experience -
                        character->experience) - entries[panel->partySlot].experience;
                    panel->numbers[8].option = 8;
                    panel->numbers[8].digits = 5;
                }
                eNumberMain(&panel->numbers[8]);
                panel->sprites[11].x = columnX + 64;
                panel->sprites[11].y = baselineY - 1;
                endPrintExtFunc(panel->depth, 2, &panel->sprites[11]);
            }
            columnX = panel->window.x + 228;
            baselineY = panel->window.y + 80;
            panel->sprites[12].x = columnX - 11;
            panel->sprites[12].y = baselineY - 1;
            endPrintExtFunc(panel->depth, 2, &panel->sprites[12]);
            panel->numbers[9].x = columnX;
            panel->numbers[9].y = baselineY + 1;
            entries = ((SeisanResultData_SS *)SeisanResult)->entries;
            panel->numbers[9].value = entries[panel->partySlot].experience -
                (SeisanCN.targets[panel->partySlot].experience - character->experience);
            panel->numbers[9].digits = 5;
            panel->numbers[9].option = 8;
            eNumberMain(&panel->numbers[9]);
            panel->sprites[13].x = columnX + 64;
            panel->sprites[13].y = baselineY - 1;
            endPrintExtFunc(panel->depth, 2, &panel->sprites[13]);
            if (panel->levelFlashTimer) {
                panel->tags[3].x = panel->window.x + 306;
                panel->tags[3].y = panel->window.y + 7;
                eTagFontMain(&panel->tags[3]);
            }
        }
        if (SeisanWork_SS[SEISAN_WORK_STATE] == 0x20) {
            panel->separator[0].y = panel->window.y + 28;
            panel->separator[0].x = panel->separator[1].x = panel->window.x + 304;
            panel->separator[1].y = panel->window.y + panel->window.height - 3;
        } else {
            panel->separator[0].y = panel->window.y + 3;
            panel->separator[0].x = panel->separator[1].x = panel->window.x + 238;
            panel->separator[1].y = panel->window.y + panel->window.height - 3;
        }
        panel->separator[0].depth = panel->separator[1].depth = 0xfffffe;
        panel->separator[1].alpha = panel->separator[0].alpha = 128;
        panel->separator[0].red = panel->separator[0].green = panel->separator[0].blue = 192;
        panel->separator[1].red = panel->separator[1].green = panel->separator[1].blue = 192;
        panel->separator[2].depth = 0;
        endPrintExtFunc(panel->depth, 0, panel->separator);
        if (panel->learnedVisible) {
            int topY;
            int rightX;
            int bottomY;
            for (i = 0; i < 4; i++) {
                panel->highlight[i].blue = 0;
                panel->highlight[i].green = 0;
                panel->highlight[i].red = 0;
                panel->highlight[i].alpha = i < 2 ? 64 : 0;
                panel->highlight[i].depth = panel->depth + 2;
            }
            panel->highlight[0].x = panel->highlight[1].x = window->x + 238;
            panel->highlight[i].depth = 0;
            topY = (unsigned short)panel->window.y + 3;
            rightX = (unsigned short)window->x + (unsigned short)panel->window.width - 3;
            bottomY = (unsigned short)panel->window.y + (unsigned short)panel->window.height - 3;
            panel->highlight[2].x = panel->highlight[3].x = rightX;
            panel->highlight[1].y = panel->highlight[3].y = bottomY;
            panel->highlight[0].y = panel->highlight[2].y = topY;
            endPrintExtFunc(panel->depth, 3, panel->highlight);
        }
        if (panel->displayFlags & 16) {
            unsigned short firstLabels[3];
            unsigned short lastLabels[3];

            panel->learnedTitle.x = panel->window.x + 88;
            panel->learnedTitle.y = panel->window.y + 32;
            eMessageMain(&panel->learnedTitle);
            if (SeisanWork_SS[SEISAN_WORK_STATE] == 0x50) {
                for (i = 0; i < 4; i++) {
                    panel->learned[i].x = panel->window.x + 288;
                    panel->learned[i].y = panel->window.y + i * 24 + 3;
                    eMessageMain(&panel->learned[i]);
                    if (panel->learned[i].text) {
                        const unsigned short *ordinalSource = msg_number[i + 1];
                        int printX = panel->learned[i].x - 40;
                        int printY = panel->learned[i].y - 2;
                        *(SeisanLabelBytes *)firstLabels = *(const SeisanLabelBytes *)D_004DB090;
                        firstLabels[0] = ordinalSource[0];
                        xglFontPrint(printX, printY, 0xffffff, (const char *)firstLabels);
                    }
                }
            } else {
                for (i = 0; i < 2; i++) {
                    panel->learned[i + 4].x = panel->window.x + 288;
                    panel->learned[i + 4].y = panel->window.y + i * 24 + 3;
                    eMessageMain(&panel->learned[i + 4]);
                    if (panel->learned[i + 4].text) {
                        const unsigned short *ordinalSource = msg_number[i + 5];
                        int printX = panel->learned[i + 4].x - 40;
                        int printY = panel->learned[i + 4].y - 2;
                        *(SeisanLabelBytes *)lastLabels = *(const SeisanLabelBytes *)D_004DB090;
                        lastLabels[0] = ordinalSource[0];
                        xglFontPrint(printX, printY, 0xffffff, (const char *)lastLabels);
                    }
                }
            }
        }
        break;
    }
    }
}

extern unsigned char *SeisanFade;

void SeisanFadeMain(void)
{
    unsigned char *pFade;
    unsigned char *temp_2;
    unsigned char *temp_3;
    u32 *temp_3_2;
    u32 temp_const;
    int var_4;
    int var_5;
    int temp_2_2;

    pFade = SeisanFade;
    switch (pFade[0]) {
    case 0:
        pFade[1] = 0;
        temp_3 = pFade + 8;
        temp_const = 0xFFFFFF;
        var_4 = 0;
        var_5 = 3;
        do {
            temp_2 = pFade + var_4;
            temp_3_2 = (u32 *)(temp_3 + var_4);
            var_5 -= 1;
            temp_2[0xF] = 0;
            temp_2[0xE] = 0;
            var_4 += 0xC;
            temp_2[0xD] = 0;
            temp_2[0xC] = 0;
            *temp_3_2 = temp_const;
        } while (var_5 >= 0);
        *(u16 *)(pFade + 4) = 0;
        *(u16 *)(pFade + 6) = 0;
        *(u16 *)(pFade + 0x10) = 0;
        *(u16 *)(pFade + 0x12) = 0x1C0;
        *(u16 *)(pFade + 0x1C) = 0x200;
        *(u16 *)(pFade + 0x1E) = 0;
        *(u16 *)(pFade + 0x28) = 0x200;
        *(u16 *)(pFade + 0x2A) = 0x1C0;
        pFade[0] = 2;
        *(u32 *)(pFade + 0x38) = 0;
        /* fallthrough */
    case 2:
        if (SeisanWork[1] == 0xF0) {
            temp_2_2 = (0x10 - SeisanWork[5]) * 8;
            pFade[0x33] = temp_2_2;
            pFade[0x27] = temp_2_2;
            pFade[0x1B] = temp_2_2;
            pFade[0xF] = temp_2_2;
        }
        if (((int (*)(unsigned int, int, void *))endPrintExtFunc)(0x02FFFFFF, 3, pFade + 4)) {}
        break;
    }
}

int subSeisanHissatuCheck00(unsigned char *seisan_work)
{
    int i;

    for (i = 0; i < SeisanWork[6]; i++)
    {
        if (SeisanResult->entries[i].count != 0)
        {
            return 0x50;
        }
    }
    return 0;
}

int subSeisanHissatuCheck01(void)
{
    int i;

    for (i = 0; i < SeisanWork[6]; i++)
    {
        if (SeisanResult->entries[i].count >= 5)
        {
            return 0x58;
        }
    }
    return 0;
}

int subSeisanEtherCheck(void)
{
    return 0;
}

int subSeisanItemCheck(void)
{
    return 0x80;
}

void subSeisanCheck(void)
{
    switch (SeisanWork[SEISAN_WORK_STATE]) {
    case 0x20:
        SeisanWork[SEISAN_WORK_NEXT_STATE] =
            subSeisanHissatuCheck00(SeisanWork);
        if (SeisanWork[SEISAN_WORK_NEXT_STATE] != 0)
            break;
        /* fallthrough */
    case 0x50:
        SeisanWork[SEISAN_WORK_NEXT_STATE] = subSeisanHissatuCheck01();
        if (SeisanWork[SEISAN_WORK_NEXT_STATE] != 0)
            break;
        /* fallthrough */
    case 0x58:
        SeisanWork[SEISAN_WORK_NEXT_STATE] = subSeisanEtherCheck();
        if (SeisanWork[SEISAN_WORK_NEXT_STATE] != 0)
            break;
        /* fallthrough */
    case 0x60:
        SeisanWork[SEISAN_WORK_NEXT_STATE] = subSeisanItemCheck();
        if (SeisanWork[SEISAN_WORK_NEXT_STATE] != 0)
            break;
        /* fallthrough */
    case 0x80:
        SeisanWork[SEISAN_WORK_NEXT_STATE] = SEISAN_STATE_CLOSING;
        SeisanWork[SEISAN_WORK_FLAGS] |= 4;
        break;
    }
}

void subSeisanMain(void)
{
    if (SeisanWork[1] != SeisanWork[3]) {
        SeisanWork[1] = SeisanWork[3];
        SeisanWork[2] |= 1;
    } else {
        SeisanWork[2] &= 0xfe;
    }
    if (SeisanWork[5] != 0)
        SeisanWork[5]--;

    switch (SeisanWork[1]) {
    case 0x20:
        if ((unsigned char)(SeisanWork[2] & 1)) {
            SeisanWork[5] = 0x20;
            subSeisanCheck();
        }
        if (SeisanWork[5] == 0 && SeisanCN.state == 0)
            SeisanCN.state = 1;
        if ((PadData.half_2a & 0x20) && SeisanWork[5] == 0) {
            if (SeisanCN.state == 3)
                SeisanWork[3] = SeisanWork[4];
            else
                SeisanCN.state = 2;
        }
        SeisanCountMain(0);
        break;
    case 0x30:
        break;
    case 0x50:
        if ((unsigned char)(SeisanWork[2] & 1)) {
            SeisanWork[5] = 16;
            subSeisanCheck();
        }
        if ((PadData.half_2a & 0x20) && SeisanWork[5] == 0)
            SeisanWork[3] = SeisanWork[4];
        break;
    case 0x58:
        if ((unsigned char)(SeisanWork[2] & 1)) {
            SeisanWork[5] = 16;
            subSeisanCheck();
        }
        if ((PadData.half_2a & 0x20) && SeisanWork[5] == 0)
            SeisanWork[3] = SeisanWork[4];
        break;
    case 0x60:
        if ((unsigned char)(SeisanWork[2] & 1)) {
            SeisanWork[5] = 16;
            subSeisanCheck();
        }
        if ((PadData.half_2a & 0x20) && SeisanWork[5] == 0)
            SeisanWork[3] = SeisanWork[4];
        break;
    case 0x80:
        if ((unsigned char)(SeisanWork[2] & 1)) {
            SeisanWork[5] = 16;
            subSeisanCheck();
        }
        if ((PadData.half_2a & 0x20) && SeisanWork[5] == 0)
            SeisanWork[3] = SeisanWork[4];
        break;
    case 0xf0:
        if ((unsigned char)(SeisanWork[2] & 1))
            SeisanWork[5] = 16;
        if (SeisanWork[5] == 0) {
            unsigned char *work;
            MenuBgTaskBreak();
            work = SeisanWork;
            work[3] = 0xff;
            work[1] = 0xff;
        } else {
            SeisanWork[5]--;
        }
        break;
    }
    SeisanWork[2] &= 0xfd;
}

int SeisanMain(void)
{
    subSeisanMain();
    return SeisanWork[SEISAN_WORK_STATE] != SEISAN_STATE_FINISHED;
}

void SeisanDisp(void)
{
    SeisanRibbonData ribbon;

    ribbon = D_004C7350;
    endPrintDirectRibbon(&ribbon);
    endPrintExtFunc(0, 100, 0);
    if (MenuLoadSync())
    {
        xglTaskExecute(SeisanBgTask);
    }
    else
    {
        MenuBgTaskMain();
        xglTaskExecute(SeisanBgTask);
        xglTaskExecute(SeisanTask);
        SeisanFadeMain();
    }
}

INCLUDE_ASM("asm/main/nonmatchings/seisan_count_init1", SeisanInit);

#include "common.h"
#include "shared.h"
#include "main/xgl_task.h"
#include "seisan_count_init1.h"

void SeisanCountInit1(void)
{
    int chrNo;

    memset(&SeisanCN, 0, sizeof(SeisanCN));
    for (chrNo = 1; chrNo < 13; chrNo++)
    {
        SeisanCN.nextExp[chrNo - 1] = func_A19210(chrNo)->nextExp;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/seisan_count_init1", SeisanCountInit2);

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

INCLUDE_ASM("asm/main/nonmatchings/seisan_count_init1", SeisanCountMain);

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

INCLUDE_ASM("asm/main/nonmatchings/seisan_count_init1", SeisanTimeEx);

extern char *msg00_1_0036DAC8[];
void MoveSlide(short *current, short *target, float rate);
void eSpriteSet(void *sprite, short spriteId);
void eSpriteMain(void *sprite);
void eMessageSet(void *message, const char *text);
void eMessageMain(void *message);
typedef struct SeisanButtonData {
    unsigned char unk00[4];
    int color;              /* +0x04 */
    unsigned char msg[1];   /* +0x08 */
    unsigned char msgAttr;  /* +0x09 */
    unsigned char unk0A[2];
    short msgX;             /* +0x0C */
    short msgY;             /* +0x0E */
    int msgColor;           /* +0x10 */
    unsigned char unk14[0xC];
    char *msgText;          /* +0x20 */
    unsigned char unk24[0x28];
    unsigned char sprite[4]; /* +0x4C */
    short x;                /* +0x50 */
    short y;                /* +0x52 */
    int sprColor;           /* +0x54 */
} SeisanButtonData;

static void tskSeisanButton(TskObject *task, SeisanButtonData *w)
{
    short target[8];
    unsigned char *sw;
    int flag;

    switch (task->state) {
    case 0:
        w->color = 0xFFFFF0;
        eSpriteSet(w->sprite, 0x200);
        w->x = -0x70;
        w->y = 0x1A4;
        w->sprColor = w->color;
        eMessageSet(w->msg, msg00_1_0036DAC8[0]);
        w->msgAttr = 0x20;
        w->msgX = w->x + 0x14;
        w->msgY = w->y;
        w->msgColor = w->color;
        break;
    case 2:
        sw = SeisanWork;
        flag = sw[2] & 4;
        target[0] = 0x180;
        if (flag) {
            w->msgText = msg00_1_0036DAC8[1];
        }
        if (sw[1] == 0xF0) {
            target[0] = 0x220;
        }
        MoveSlide(&w->x, target, 3.0f);
        eSpriteMain(w->sprite);
        w->msgX = w->x + 0x14;
        w->msgY = w->y;
        eMessageMain(w->msg);
        break;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/seisan_count_init1", tskSeisanItem);

INCLUDE_ASM("asm/main/nonmatchings/seisan_count_init1", tskSeisanStatus);

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

INCLUDE_ASM("asm/main/nonmatchings/seisan_count_init1", subSeisanMain);

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

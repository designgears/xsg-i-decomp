#include "common.h"
#include "shared.h"
#include "main/xgl_task.h"
#include "main/xgl_2.h"

#include "create_evt_item_get_task.h"

extern int dataBoxInc(int category, int id);
extern char *GetItemName(int category, int id);
extern XglTaskPrefix *xglTaskEntryNext(XglTaskScheduler *scheduler,
                                       int (*callback)(XglTaskPrefix *),
                                       XglTaskPrefix *entry);

/* One MapUnit record view shared by the initialization and item-update
 * functions below.  Its stride and all named members are supported by their
 * combined accesses; gaps remain unmodelled. */
typedef struct ItemUnitInfo {
    unsigned char unmodeled_00[2];
    unsigned short itemNo;
    signed char signal;
    signed char unitNo;
    unsigned char unmodeled_06[0x27];
    signed char rsrcIndex;
    unsigned char unmodeled_2e[6];
    int seId;
} ItemUnitInfo;

typedef struct ItemMapUnit {
    unsigned int flags;
    void (*update)(struct ItemMapUnit *);
    void (*draw)(struct ItemMapUnit *);
    unsigned char unmodeled_0c[4];
    Vector4 position;
    Vector4 rotation;
    unsigned char unmodeled_30[0x10];
    Matrix4 matrix;
    Matrix4 *matrixPtr;
    unsigned char unmodeled_84[0x1c];
    unsigned char seChannel;
    unsigned char actionNo;
    unsigned char actionSub;
    unsigned char unmodeled_a3;
    unsigned short serial;
    unsigned char unmodeled_a6[2];
    short sequenceNo;
    unsigned char unmodeled_aa[0x3e];
    int effectCf[3];
    int ownerNo;
    unsigned char unmodeled_f8[0xa8];
    ItemUnitInfo info;
} ItemMapUnit;

typedef struct MapDisplayRecord {
    unsigned char unmodeled_00[4];
    void *model;
} MapDisplayRecord;

extern void MAP_updateUnitSpecialSymbol(ItemMapUnit *unit);
extern void MAP_updateUnitItemSymbol(ItemMapUnit *unit);
extern void SetItemSymbolRsrc(ItemMapUnit *unit);
extern int xglFlagsGet1(int bitOffset);
extern int printf(const char *format, ...);
extern void sefDeleteEffectCf(int effectId);
extern const char D_004CA5D0[];

typedef struct ItemOwnerUnit {
    unsigned char unmodeled_00[0xa4];
    short serial;
    unsigned char unmodeled_a6[0x25a];
} ItemOwnerUnit;
extern ItemOwnerUnit MapUnit[64];
extern void nmlModelSetPartsVisible(void *model, short partsNo, int visible);
extern void DrawActiveCursol(ItemMapUnit *unit);
extern const float D_004D7F7C;
extern signed char printflg;
extern const char D_004CA650[];

typedef struct ItemBoxEntry {
    signed char kind;
    signed char pad1;
    signed char level;
    signed char pad3;
    int special;
} ItemBoxEntry;

extern ItemBoxEntry ItemBoxTbl[];

extern char *strno[];
extern const char D_004CA4D0[];
extern const char D_004CA508[];
extern const char D_004CA520[];
extern const char D_004CA530[];
extern const char D_004CA550[];
extern char D_004DB780[];
extern int xglFlagsSet1(int bit_offset, int value);
extern int dataMoneyBoxInc(int amount);
extern void CallMethod_I(const char *, int);
typedef struct ItemGetTaskView {
    XglTaskPrefix task;
    unsigned char unk10[8];
    char *name;
    unsigned char *owner;
    ItemGetWindow *window;
    signed char kind;
    unsigned char unk25;
    short itemNo;
    signed char level;
    unsigned char unk29;
    unsigned char unk2a[2];
    int special;
    signed char started;
} ItemGetTaskView;

static int taskItemGet(XglTaskPrefix *t)
{
    ItemGetTaskView *task;
    char message[64];
    int id;
    int no;
    ItemBoxEntry *e;
    unsigned char *b;
    int count;
    char *name;
    int *new_var2;
    unsigned char *o;
    int method;
    signed char unit;
    ItemGetWindow *w;
    signed char lvl;
    unsigned char *new_var;
  task = (ItemGetTaskView *) t;
  GameLoopState[0x1c / (sizeof(unsigned int))] = 0;
  if (0 == task->started)
  {
    memset(message, 0, 64);
    new_var = (unsigned char *) ItemBoxTbl;
    no = task->itemNo;
    method = no * 8;
    b = new_var + method;
    new_var2 = (int *) ((((unsigned char *) ItemBoxTbl) + method) + 4);
    task->kind = b[0];
    id = b[1];
    task->level = b[2];
    task->unk29 = b[3];
    task->special = *new_var2;
    xglFlagsSet1(no - -0x79EC7, 1);
    task->started = 1;
    if (task->special != 0)
    {
      sprintf(message, D_004CA4D0, task->special);
      dataMoneyBoxInc(task->special);
    }
    else
    {
      lvl = task->level;
      if (lvl == 1)
      {
        char *n1 = GetItemName(task->kind, id);
        task->name = n1;
        if (task->kind == 10)
        {
          sprintf(message, evt_item_format_special, n1);
        }
        else
        {
          sprintf(message, evt_item_format_normal, n1);
        }
        dataBoxInc(task->kind, id);
      }
      else
      {
        count = lvl;
        if (((!task->kind) && (!task->kind)) && (!task->kind))
        {
        }
        if (count >= 100)
        {
          count = 99;
        }
        name = GetItemName(task->kind, id);
        task->name = name;
        if (task->level < 10)
        {
          if (task->kind == 10)
          {
            sprintf(message, D_004CA508, strno[count], name);
          }
          else
          {
            sprintf(message, D_004CA520, strno[count], name);
          }
        }
        else
          if (task->kind == 10)
        {
          sprintf(message, D_004CA530, strno[count / 10], strno[count % 10], name);
        }
        else
        {
          sprintf(message, D_004CA550, strno[count / 10], strno[count % 10], name);
        }
        if (count > 0)
        {
          do
          {
            count--;
            dataBoxInc(task->kind, id);
          }
          while (count != 0);
        }
      }
    }
    task->window = createItemGetWin(message);
    GameLoopState[0x10 / (sizeof(unsigned int))] |= 0x20000;
  }
  if (2 != task->window->state)
  {
    return (int) task->window;
  }
  {
    o = task->owner + 0x1a0;
    method = *((short *) (o + 0x30));
    GameLoopState[0x10 / (sizeof(unsigned int))] &= 0xfffdffffu;
    if (method != (-1))
    {
      CallMethod_I(D_004DB780, method);
    }
    unit = o[5];
    if (unit != (-1))
    {
      ((unsigned char *) (&MapUnit[unit]))[0xa2] = 5;
    }
    return xglTaskWaitRemove(&task->task);
  }
}

static void taskEvtItemGet(EventItemTask *task)
{
    char message[64];
    GameLoopState[0x1c / sizeof(unsigned int)] = 0;
    if (task->window_created == 0) {
        memset(message, 0, 64);
        task->window_created = 1;
        if (task->category == 10) {
            sprintf(message, evt_item_format_special, task->item_name);
        } else {
            sprintf(message, evt_item_format_normal, task->item_name);
        }
        task->window = createItemGetWin(message);
    }
    if (task->window->state == 2) {
        GameLoopState[0x10 / sizeof(unsigned int)] &= 0xfffdffffu;
        xglTaskWaitRemove(&task->task);
    }
}

int CreateEvtItemGetTask(int item, int count)
{
    XglTaskScheduler *scheduler;
    EventItemTask *task;

    if (dataBoxInc(item, count) == 0) {
        return 0;
    }
    scheduler = (XglTaskScheduler *) GameLoopState[2];
    task = (EventItemTask *) xglTaskEntryNext(
        scheduler, (int (*)(XglTaskPrefix *)) taskEvtItemGet,
        scheduler != 0 ? scheduler->active_tail : 0);
    task->zero_word_10 = 0;
    task->zero_word_14 = 0;
    task->category = item;
    task->window_created = 0;
    task->item_name = GetItemName(item, count);
    GameLoopState[4] |= 0x20000;
    return 1;
}

extern char deb_0[8];
extern const char D_004CA568[];
extern char **func_A2C5F8(int id);
extern char **func_A2C6E8(int id);
extern char **func_A2C738(int id);
extern char **func_A2C698(int id);
extern char *dataEvtItmNameGet(int index);

char *GetItemName(int category, int id)
{
    char **p;
    char *s;

    if (id == 0) {
        return deb_0;
    }
    switch (category) {
    case 0:
        p = func_A2C5F8(id);
        goto tail;
    case 1:
        p = func_A2C6E8(id);
        goto tail;
    case 2:
        p = func_A2C738(id);
        goto tail;
    case 3:
        p = func_A2C698(id);
    tail:
        if (p != 0) {
            return *p;
        }
        return deb_0;
    case 10:
        s = dataEvtItmNameGet(id);
        if (s == 0) {
            s = deb_0;
        }
        return s;
    default:
        printf(D_004CA568, category);
        return 0;
    }
}

char *dataEvtItmNameGet(int index)
{
    return (char *)EvtItemTbl + index * 0x80 - 0x80;
}

static int Pow(int base, int exponent)
{
    int count = 1;
    int result = base;

    if (count < exponent) {
        count = exponent - 1;
        do {
            count--;
            result *= base;
        } while (count != 0);
    }

    return result;
}

extern char *strno[];
extern char *strcat(char *, const char *);

int HexToStr(int n, char *buf)
{
    int digits;
    int i;

    digits = 1;
    if (n < 0)
        return 1;
    for (i = 1; i < 16; i++) {
        if (n < Pow(10, i))
            break;
        digits++;
    }
    if (digits == 1) {
        strcat(buf, strno[n % 10]);
        return 1;
    }
    for (i = digits; i > 1; i--) {
        strcat(buf, strno[(n / Pow(10, i - 1)) % 10]);
    }
    strcat(buf, strno[n % 10]);
    return digits;
}

extern int uwares_tbl[];
extern int xtxres_tbl[];
extern const char D_004CA5B0[];
extern void MAP_updateUnitItemBox();
extern void MAP_drawUnitItemBox();
#define IIB_RSRC(n) \
    { int a = uwares_tbl[n]; int b = xtxres_tbl[n]; \
    *(int *)((char *)unit + 0xE0) = a; \
    *(int *)((char *)unit + 0xE4) = b; }

void InitItemBox(ItemMapUnit *unit)
{
    ItemUnitInfo *info = &unit->info;
    Matrix4 *mtx;
    int m;
    int i;
    int *p;

    switch ((short)unit->serial) {
    case 0x7005:
        IIB_RSRC(4);
        info->seId = 6;
        break;
    case 0x7008:
        IIB_RSRC(7);
        info->seId = 0x39;
        break;
    default:
        IIB_RSRC(4);
        break;
    }
    m = *(int *)((char *)unit + 0xE0);
    mtx = (Matrix4 *)(m + *(int *)(m + 0x50) + 0x40);
    *(Matrix4 **)((char *)unit + 0x230) = mtx;
    *(float *)((char *)unit + 0xC0) = (*mtx)[3][0];
    *(float *)((char *)unit + 0xC4) = (*mtx)[3][1];
    *(float *)((char *)unit + 0xC8) = (*mtx)[3][2];
    *(float *)((char *)unit + 0xCC) = 1.0f;
    xglMatrixStackUnit();
    xglMatrixStackTrans((float *)((char *)unit + 0xC0));
    xglMatrixStackSave(*(Matrix4 **)((char *)unit + 0x230));
    if (info->rsrcIndex == -1) {
        info->rsrcIndex = 2;
    }
    *(int *)((char *)info + 0x44) = 0;
    unit->flags |= 4;
    *(float *)((char *)unit + 0xB0) = 0.3f;
    *(float *)((char *)unit + 0xB4) = 0.5f;
    *(float *)((char *)unit + 0xB8) = 0.2f;
    xglMatrixStackUnit();
    xglMatrixStackTrans(&unit->position.x);
    xglMatrixStackRotX(unit->rotation.x);
    xglMatrixStackRotY(unit->rotation.y);
    xglMatrixStackRotZ(unit->rotation.z);
    xglMatrixStackSave(unit->matrix);
    unit->matrixPtr = &unit->matrix;
    unit->update = (void (*)(ItemMapUnit *))MAP_updateUnitItemBox;
    unit->draw = (void (*)(ItemMapUnit *))MAP_drawUnitItemBox;
    unit->flags |= 0x10000;
    unit->flags |= 0x8000000;
    unit->actionNo = 0;
    unit->sequenceNo = 0;
    *(short *)((char *)unit + 0xAA) = 0;
    unit->actionSub = 0;
    if (info->unitNo != -1) {
        ItemOwnerUnit *o = &MapUnit[info->unitNo];
        *(long *)((char *)o + 0x10) = *(long *)&unit->position;
        *(unsigned int *)o |= 4;
        *(long *)((char *)o + 0x18) = *(long *)((char *)&unit->position + 8);
    }
    if (info->itemNo >= 602) {
        unit->serial = (unsigned short)-1;
        unit->update = 0;
        unit->draw = 0;
        printf(D_004CA5B0, (short)info->itemNo);
        return;
    }
    if (xglFlagsGet1(0x79EC7 + (short)info->itemNo) == 1) {
        unit->sequenceNo = 0x10;
        unit->actionSub = 3;
        for (i = 0; i < 3; i++) {
            if (unit->effectCf[i] != 0) {
                sefDeleteEffectCf(unit->effectCf[i]);
            }
        }
    }
}

void InitItemSymbol(ItemMapUnit *unit)
{
    ItemUnitInfo *info = &unit->info;
    int i;

    xglMatrixStackUnit();
    xglMatrixStackTrans(&unit->position.x);
    xglMatrixStackRotX(unit->rotation.x);
    xglMatrixStackRotY(unit->rotation.y);
    xglMatrixStackRotZ(unit->rotation.z);
    xglMatrixStackSave(unit->matrix);
    unit->matrixPtr = &unit->matrix;
    if (info->rsrcIndex == -1) {
        info->rsrcIndex = 0;
    }
    unit->update = MAP_updateUnitItemSymbol;
    unit->flags |= 0x10000;
    unit->flags |= 0x10000000;
    if ((unsigned int) unit->serial - 0x7033 < 5) {
        unit->update = MAP_updateUnitSpecialSymbol;
        return;
    }
    if (info->itemNo >= 602) {
        unit->draw = 0;
        unit->update = 0;
        unit->serial = (unsigned short) -1;
        printf(D_004CA5D0, (short) info->itemNo);
        return;
    }
    if (xglFlagsGet1(0x79EC7 + (short) info->itemNo) == 1) {
        unit->serial = (unsigned short) -1;
        unit->update = 0;
        for (i = 0; i < 3; i++) {
            if (unit->effectCf[i] != 0) {
                sefDeleteEffectCf(unit->effectCf[i]);
            }
        }
        return;
    }
    SetItemSymbolRsrc(unit);
}

void InitSpecialSymbol(ItemMapUnit *unit)
{
    ItemUnitInfo *info = &unit->info;

    xglMatrixStackUnit();
    xglMatrixStackTrans(&unit->position.x);
    xglMatrixStackRotX(unit->rotation.x);
    xglMatrixStackRotY(unit->rotation.y);
    xglMatrixStackRotZ(unit->rotation.z);
    xglMatrixStackSave(unit->matrix);
    unit->matrixPtr = &unit->matrix;
    if (info->rsrcIndex == -1) {
        info->rsrcIndex = 0;
    }
    unit->update = MAP_updateUnitSpecialSymbol;
    unit->flags |= 0x10004;
    unit->flags |= 0x10000000;
    SetItemSymbolRsrc(unit);
}

extern int uwares_tbl[];
extern int xtxres_tbl[];
#define SET_RSRC(n) \
    { int a = uwares_tbl[n]; int b = xtxres_tbl[n]; \
    *(int *)((char *)unit + 0xE0) = a; \
    *(int *)((char *)unit + 0xE4) = b; }

void SetItemSymbolRsrc(ItemMapUnit *unit)
{
    int kind, level;
    ItemUnitInfo *info = &unit->info;

    if ((short)unit->serial != 0x7000) {
        switch ((short)unit->serial) {
        case 0x7009: SET_RSRC(8); break;
        case 0x700A: SET_RSRC(9); break;
        case 0x700B: SET_RSRC(10); break;
        case 0x700C: SET_RSRC(11); break;
        case 0x700D: SET_RSRC(12); break;
        case 0x700E: SET_RSRC(13); break;
        }
    } else {
        kind = ItemBoxTbl[(short)info->itemNo].kind;
        level = ItemBoxTbl[(short)info->itemNo].level;
        if (ItemBoxTbl[(short)info->itemNo].special != 0) {
            unit->serial = 0x7009;
            SET_RSRC(8);
            return;
        }
        switch (kind) {
        case 0:
            if (level < 0xF) { SET_RSRC(9); }
            else { SET_RSRC(10); }
            break;
        case 1:
            if (level < 0x46) { SET_RSRC(11); }
            else { SET_RSRC(12); }
            break;
        case 2:
            SET_RSRC(12);
            break;
        case 3:
            if (level < 0x80) { SET_RSRC(11); }
            else { SET_RSRC(12); }
            break;
        case 10:
            SET_RSRC(13);
            break;
        }
    }
}

typedef struct ItemGetTask {
    XglTaskPrefix task;
    int w10;
    int w14;
    unsigned char pad18[4];
    ItemMapUnit *unit;
    unsigned char pad20[6];
    unsigned short itemNo;
    unsigned char pad28[8];
    unsigned char b30;
} ItemGetTask;

extern void UwamonoCommonFunc(void *unit);

extern void xglSoundEffectNormalID(int sound_id, int variant);

extern int taskItemGet(XglTaskPrefix *task);

extern int ClearUwamonoEffect(void *unit);

void MAP_updateUnitItemBox(void *arg)
{
    XglTaskScheduler *scheduler;
    ItemGetTask *task;
    ItemOwnerUnit *owner;
    signed char signal;
    int itemNo;
    ItemMapUnit *unit = arg;
    ItemUnitInfo *info = &unit->info;

    unit->flags |= 4;
    UwamonoCommonFunc(unit);
    switch (unit->actionSub) {
    case 0:
        signal = info->signal;
        if (signal == 1) {
            unit->actionSub = signal;
            xglSoundEffectNormalID(info->seId, 0);
            ClearUwamonoEffect(unit);
            return;
        }
        break;
    case 1:
        unit->sequenceNo++;
        scheduler = (XglTaskScheduler *) GameLoopState[2];
        task = (ItemGetTask *) xglTaskEntryNext(scheduler, taskItemGet,
            scheduler != 0 ? scheduler->active_tail : 0);
        itemNo = info->itemNo;
        *(volatile unsigned char *) &task->b30 = 0;
        *(volatile unsigned short *) &task->itemNo = itemNo;
        task->w14 = 0;
        task->unit = (ItemMapUnit *) unit;
        task->w10 = 0;
        unit->actionSub = 2;
        if (info->unitNo != -1) {
            owner = &MapUnit[info->unitNo];
            owner->unmodeled_a6[0xFE] = 1;
            *(long long *) &owner->unmodeled_00[0x10] = *(long long *) ((char *) unit + 0x10);
            *(long long *) &owner->unmodeled_00[0x18] = *(long long *) ((char *) unit + 0x18);
            return;
        }
        break;
    case 2:
        if (++unit->sequenceNo > 16) {
            unit->actionSub = 3;
        }
        break;
    case 3:
        ClearUwamonoEffect(unit);
        break;
    }
}

extern void nmlModelEntry(int entry);
extern void nmlModelSetClip(int clip);
extern void nmlModelSetPlace(Matrix4 matrix);
extern void nmlModelSetTexture(const char *texture);

/*
 * Only the fields MAP_drawUnitItemBox reads/writes are evidenced: the
 * embedded placement matrix at +0x40 (addiu $4,$16,64 into nmlModelSetPlace),
 * the rotation-X angle at +0xA8 (lh 0xA8, divided by 9.0 before
 * xglMatrixStackRotX), the embedded translation vector at +0xC0 (addiu
 * $17,$16,192, passed to xglMatrixStackTrans), the model entry index at
 * +0xE0 (lw 0xE0 into nmlModelEntry), the texture pointer at +0xE4 (lw 0xE4
 * into nmlModelSetTexture) and the saved-matrix pointer at +0x230 (lw 0x230
 * into xglMatrixStackSave). The spans between them are left unmodelled.
 */
typedef struct UnitItemBox {
    unsigned char unmodeled_00[0x40];  /* +0x00 */
    Matrix4 place;                      /* +0x40 */
    unsigned char unmodeled_80[0x28];  /* +0x80 */
    short angle;                        /* +0xA8 */
    unsigned char unmodeled_aa[0x16];  /* +0xAA */
    Vector4 translation;                /* +0xC0 */
    unsigned char unmodeled_d0[0x10];  /* +0xD0 */
    int model_entry;                    /* +0xE0 */
    const char *texture;                /* +0xE4 */
    unsigned char unmodeled_e8[0x148]; /* +0xE8 */
    Matrix4 *saved_matrix;              /* +0x230 */
} UnitItemBox;

void MAP_drawUnitItemBox(UnitItemBox *box)
{
    xglMatrixStackUnit();
    xglMatrixStackTrans(&box->translation.x);
    xglMatrixStackRotX((float) box->angle / 9.0f);
    xglMatrixStackSave(*box->saved_matrix);
    nmlModelSetTexture(box->texture);
    nmlModelSetPlace(box->place);
    nmlModelSetClip(1);
    nmlModelEntry(box->model_entry);
    xglMatrixStackUnit();
    xglMatrixStackTrans(&box->translation.x);
    xglMatrixStackSave(*box->saved_matrix);
}

extern float D_004D7F60;
extern float D_004D7F64;
extern float CheckDist3D(Vector4 *a, Vector4 *b);
void MAP_updateUnitItemSymbol(ItemMapUnit *unit)
{
    XglTaskScheduler *scheduler;
    ItemGetTask *task;
    unsigned short itemNo;
    ItemUnitInfo *info = &unit->info;

    unit->rotation.y += D_004D7F60;
    UwamonoCommonFunc(unit);
    switch (unit->actionSub) {
    case 0:
        if (CheckDist3D((Vector4 *) (GameLoopState[1] + 0x10), &unit->position) < D_004D7F64
            && !(GameLoopState[4] & 0x20000)) {
            scheduler = (XglTaskScheduler *) GameLoopState[2];
            task = (ItemGetTask *) xglTaskEntryNext(scheduler, taskItemGet,
                scheduler != 0 ? scheduler->active_tail : 0);
            itemNo = info->itemNo;
            task->w10 = 0;
            task->b30 = 0;
            task->w14 = 0;
            task->itemNo = itemNo;
            task->unit = unit;
            unit->actionSub = 1;
            unit->flags |= 4;
            xglSoundEffectNormalID(6, 0);
            GameLoopState[4] |= 0x20000;
        }
        break;
    case 1:
        unit->update = 0;
        unit->serial = -1;
        break;
    }
}

typedef union ItemMapVectorBlockUp {
    Vector4 vector;
    unsigned long long words[2];
} ItemMapVectorBlockUp;
typedef struct ItemMapUnitUp {
    unsigned int flags;
    void (*update)(struct ItemMapUnitUp *);
    void (*draw)(struct ItemMapUnitUp *);
    unsigned char unmodeled_0c[4];
    ItemMapVectorBlockUp position;
    Vector4 rotation;
    Vector4 scale;
    Matrix4 matrix;
    Matrix4 *matrixPtr;
    unsigned char unmodeled_84[0x1c];
    unsigned char seChannel;
    unsigned char actionNo;
    unsigned char actionSub;
    unsigned char unmodeled_a3;
    unsigned short serial;
    unsigned char unmodeled_a6[2];
    short sequenceNo;
    short sequenceEnd;
    unsigned char unmodeled_ac[4];
    float extent[3];
    unsigned char unmodeled_bc[4];
    ItemMapVectorBlockUp translation;
    unsigned char unmodeled_d0[0x10];
    void *model;
    const char *texture;
    int effectCf[3];
    int ownerNo;
    unsigned char unmodeled_f8[0xa8];
    ItemUnitInfo info;
} ItemMapUnitUp;

void MAP_updateUnitSpecialSymbol(ItemMapUnit *arg)
{
    ItemMapUnitUp *unit = (ItemMapUnitUp *)arg;
    ItemUnitInfo *info = &arg->info;
    ItemMapVectorBlockUp targetBlock;
    Vector4 *target = &targetBlock.vector;

    switch (unit->actionSub) {
    case 0:
        if (info->signal == 1) {
            unit->actionSub = 1;
            unit->scale.x = unit->scale.y = unit->scale.z = 0.0f;
            unit->flags &= ~4;
            unit->sequenceNo = 0;
        }
        break;
    case 1:
        unit->rotation.y += 0.05f;
        unit->position.vector.y += 0.03f;
        unit->sequenceNo++;
        unit->scale.x = unit->scale.y = unit->scale.z = unit->sequenceNo * 0.06;
        unit->scale.w = 1.0f;
        if (unit->scale.x > 1.0f) {
            unit->scale.x = unit->scale.y = unit->scale.z = 1.0f;
            unit->actionSub = 2;
            unit->translation = unit->position;
            unit->sequenceEnd = unit->sequenceNo;
        }
        break;
    case 2:
        unit->rotation.y += 0.05f;
        break;
    case 5:
        targetBlock = ((ItemMapUnitUp *)GameLoopState[1])->position;
        target->y += 1.2f;
        unit->position.vector.x = target->x + (unit->translation.vector.x - target->x) * unit->sequenceNo / unit->sequenceEnd;
        unit->position.vector.y = target->y + (unit->translation.vector.y - target->y) * unit->sequenceNo / unit->sequenceEnd;
        unit->position.vector.z = target->z + (unit->translation.vector.z - target->z) * unit->sequenceNo / unit->sequenceEnd;
        unit->rotation.y += 0.05f;
        unit->scale.x = unit->scale.y = unit->scale.z = --unit->sequenceNo * 0.06;
        if (unit->scale.x < 0.0f) {
            unit->update = 0;
            unit->serial = (unsigned short) -1;
        }
        break;
    }
}

void MAP_updateUnitItem(ItemMapUnit *unit)
{
    unsigned short nextSequenceNo;

    if (MapUnit[unit->ownerNo].serial != -1) {
        return;
    }
    unit->flags &= ~4;
    DrawActiveCursol(unit);
    unit->rotation.y = unit->rotation.y + D_004D7F7C;
    switch (unit->actionSub) {
    case 0:
        if (unit->actionNo == 1) {
            unit->actionSub = 1;
        }
        break;
    case 1:
        unit->sequenceNo++;
        nextSequenceNo = unit->sequenceNo;
        if ((nextSequenceNo & 1) == 0) {
            nmlModelSetPartsVisible(((MapDisplayRecord *) GameLoopState[0x54 / sizeof(unsigned int)])->model, unit->serial, 0);
        } else {
            nmlModelSetPartsVisible(((MapDisplayRecord *) GameLoopState[0x54 / sizeof(unsigned int)])->model, unit->serial, 1);
        }
        if (unit->sequenceNo < 4) {
            return;
        }
        if (unit->sequenceNo < 17) {
            return;
        }
        nmlModelSetPartsVisible(((MapDisplayRecord *) GameLoopState[0x54 / sizeof(unsigned int)])->model, unit->serial, 0);
        if (printflg != 0) {
            printf(D_004CA650, (short) unit->serial, unit->seChannel);
        }
        unit->update = 0;
        unit->flags = 0;
        unit->serial = (unsigned short) -1;
        break;
    }
}

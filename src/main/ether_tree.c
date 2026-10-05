#include "common.h"
#include "ether_tree.h"

/*
 * EtherTreeObject points at a table of 80 tree-object records of 0x70 bytes
 * (EtherTreeObjectSet clears 0x2300 bytes of it; EtherTreeObjectGet matches a
 * 16-bit id at offset 0). EtherTreeObjectP is the fill cursor into that table:
 * EtherTreeObjectWorkGet hands out the current record and advances it by one.
 * No function claimed here reads a record, so the record type stays opaque.
 */
typedef struct EtherTreeObjectData EtherTreeObjectData;

extern EtherTreeObjectData *EtherTreeObject;
extern EtherTreeObjectData *EtherTreeObjectP;

void EtherTreeObjectGetClear(void)
{
    EtherTreeObjectP = EtherTreeObject;
}

EtherTreeObjectData *EtherTreeObjectWorkGet(void)
{
    EtherTreeObjectData *object = EtherTreeObjectP;

    EtherTreeObjectP++;
    return object;
}

/*
 * EtherTreeCenterSet, EtherTreeToCenterSet and EtherTreeCenterMove (this
 * unit, main/tu179) all read or write this struct through EtherTreeSystem.
 * flags bit 0 gates EtherTreeCenterSet, EtherTreeCenterMove and
 * EtherTreeDraw; EtherTreeCenterSet sets bit 1 and EtherTreeCenterMove
 * tests it before sliding centerX/centerY toward targetCenterX/
 * targetCenterY with subMoveSlide. Bytes no function claimed here writes or
 * reads stay unmodeled.
 */
typedef struct EtherTreeSystemData {
    unsigned char flags;
    unsigned char unmodeled_01;
    unsigned short firstDataIndex;
    unsigned char unmodeled_04[0x0c];
    float centerX;
    float centerY;
    unsigned char unmodeled_18[0x08];
    float targetCenterX;
    float targetCenterY;
    unsigned char unmodeled_28[0x08];
    float nodeX;
    float nodeY;
    float nodeZ;
    float scale;
    float targetNodeX;
    float targetNodeY;
    unsigned char unmodeled_48[0x08];
    EtherTreeObjectData *targetObject;
    unsigned char unmodeled_54[0x1c];
    int windowTexAddr;
} EtherTreeSystemData;

/*
 * EtherTreeDraw (this unit, main:0x002bc4f0) stores WindowTexAddrGet(2)'s
 * result at +0x70 (sw $2,112($6)) and hands its address to endPrintExtFunc;
 * nothing else in this allocation touches the span between it and
 * targetCenterY, so it stays unmodeled.
 */
/*
 * EtherTreeFirstDataGet (main:0x002b9f30) reads firstDataIndex (offset 0x02,
 * u16). EtherTreeTargetChange (main:0x002ba750) reads the EtherTreeObjectGet
 * result into targetObject (offset 0x50) and its negated x/y into
 * targetNodeX/targetNodeY (offsets 0x40/0x44, always) and, when its mode
 * argument is 1, also into nodeX/nodeY (offsets 0x30/0x34).
 */
extern EtherTreeSystemData *EtherTreeSystem;
extern int EtherTreeFirstData[];

void *EtherTreeFirstDataGet(void)
{
    return &EtherTreeFirstData[EtherTreeSystem->firstDataIndex - 1];
}

EtherTreeObjectData *EtherTreeObjectGet(int id)
{
    int i;

    for (i = 0; i < 80; i++) {
        if (EtherTreeObject[i].id == id)
            return &EtherTreeObject[i];
    }
    return 0;
}

struct EtherTreeColorTable {
    unsigned char colors[4][4];
};

extern const struct EtherTreeColorTable D_004C9AB0;

void EtherTreeLineColorGet(unsigned char *color, int colorIndex)
{
    struct EtherTreeColorTable colors = D_004C9AB0;
    int i;

    for (i = 0; i < 4; i++)
        color[i] = colors.colors[colorIndex][i];
}

extern EtherTreeObjectData *EtherTreeObjectWorkGet(void);
extern unsigned char *MenuEtherDataGet(unsigned short skillId);

void sub2ParentChildSet(EtherTreeObjectData *object)
{
    int i;

    object->childCount = 0;
    object->flags |= 4;

    for (i = 0; i < 3; i++) {
        unsigned char id = MenuEtherDataGet(object->id)[i];

        if (id != 0) {
            EtherTreeObjectData *child = EtherTreeObjectWorkGet();

            child->id = id;
            object->children[i] = child;
            child->parent = object;
            object->childCount++;
            sub2ParentChildSet(object->children[i]);
        } else {
            object->children[i] = 0;
        }
    }
}

void subParentChildSet(void)
{
    unsigned char *ids = EtherTreeFirstDataGet();
    EtherTreeObjectData *root = EtherTreeObjectWorkGet();
    int i;

    root->childCount = 0;
    root->flags = 0x20;

    for (i = 0; i < 3; i++) {
        unsigned char id = ids[i];

        if (id != 0) {
            EtherTreeObjectData *child = EtherTreeObjectWorkGet();

            child->id = id;
            root->children[i] = child;
            root->childCount++;
            sub2ParentChildSet(child);
        } else {
            root->children[i] = 0;
        }
    }
}

int subPosSet(EtherTreeObjectData *node)
{
    EtherTreeObjectData *child;
    int total = 0;
    int i;

    if (node->childCount == 0) {
        total = 1;
    } else {
        for (i = 0; i < node->childCount; i++) {
            child = node->children[i];
            child->x = 57.0f;
            *(float *)child->unmodeled_28 = *(float *)node->unmodeled_28 + 1.0f;
            child->y = 0.0f;
            switch (node->childCount) {
            case 2:
                if (i == 0) {
                    child->y = -36.0f;
                } else {
                    child->y = 36.0f;
                }
                break;
            case 3:
                if (i == 0) {
                    child->y = -72.0f;
                } else if (i == 2) {
                    child->y = 72.0f;
                }
                break;
            }
            total += subPosSet(child);
        }
    }
    if (node->y < 0.0f) {
        node->y -= (float)(total - 1) * 36.0f;
    } else if (node->y > 0.0f) {
        node->y += (float)(total - 1) * 36.0f;
    }
    if (EtherTreeSystem->firstDataIndex == 6 && (node->flags & 0x20)) {
        node->children[2]->y += 36.0f;
    }
    return total;
}

typedef struct PosSetPos {
    float x;
    float y;
    float unk08[2];
} __attribute__((aligned(8))) PosSetPos;

void subPosSet2(EtherTreeObjectData *node, PosSetPos *offset)
{
    PosSetPos pos;
    int i;

    if (node == 0) {
        return;
    }
    pos = *(PosSetPos *)&node->x;
    if (offset != 0) {
        pos.x += offset->x;
        pos.y += offset->y;
    }
    for (i = 0; i < node->childCount; i++) {
        subPosSet2(node->children[i], &pos);
    }
    *(PosSetPos *)&node->x = pos;
}

static inline void subPosSet3Mid(EtherTreeObjectData *p, EtherTreeObjectData *a, EtherTreeObjectData *b)
{
    p->y = (b->y - a->y) * 0.5f + a->y;
}

void subPosSet3(void)
{
    int i;
    EtherTreeObjectData *p;

    for (i = 0; i < 80; i++) {
        p = &EtherTreeObject[i];
        switch (p->childCount) {
        case 2:
            subPosSet3Mid(p, p->children[0], p->children[1]);
            break;
        case 3:
            subPosSet3Mid(p, p->children[0], p->children[2]);
            break;
        }
    }
}

void sub2JoutoYGet(EtherTreeObjectData *object, float *out)
{
    if (object->childCount != 0) {
        sub2JoutoYGet(object->children[object->childCount - 1], out);
        return;
    }
    *out = object->y;
}

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", subJoutoPosSet);

extern int func_A19578(unsigned short, unsigned short);
extern int MenuEtherSetCheck(unsigned short, unsigned short);
extern signed char MenuEtherTypeGet(unsigned short);

void EtherTreeParaSet(EtherTreeObjectData *object)
{
    unsigned short firstDataIndex;
    unsigned char flags2;
    int type;

    firstDataIndex = EtherTreeSystem->firstDataIndex;
    if (object->id == 0) {
        return;
    }
    if (object->flags & 4) {
        object->flags = object->flags | 8;
    }
    if (MenuEtherSetCheck(firstDataIndex, object->id) != 0) {
        object->unmodeled_28[0x1D] = 1;
        object->flags = object->flags | 2;
        *(unsigned short *)&object->unmodeled_28[0x20] = 8;
    } else {
        object->flags = object->flags & 0xFD;
    }
    if (func_A19578(firstDataIndex, object->id) != 0) {
        object->flags = (object->flags | 9) & 0xEF;
    } else {
        if (object->flags & 4) {
            object->flags = object->flags | 0x10;
        }
    }
    type = MenuEtherTypeGet(object->id);
    switch (type) {
    case 0:
        object->unmodeled_19[1] = 0;
        return;
    case 1:
    case 2:
        object->unmodeled_19[1] = 1;
        break;
    default:
        object->unmodeled_19[1] = 2;
        break;
    }
}

extern EtherTreeLineData *EtherTreeLine;

/* Collect the active non-root object records into the line table. */
void EtherTreeLineSet(void)
{
    EtherTreeLineData *line = EtherTreeLine;
    EtherTreeObjectData *object;
    int count;

    if (!(EtherTreeSystem->flags & 1))
        return;

    object = EtherTreeObject;
    count = 79;
    do {
        if ((object->flags & 4) && object->id != 0) {
            line->object = object;
            line++;
        }
        object++;
    } while (--count >= 0);
}

extern EtherTreeObjectData *EtherTreeObjectGet(int id);

void EtherTreeTargetChange(int id, int mode)
{
    EtherTreeObjectData *object = EtherTreeObjectGet(id);
    float nodeX;
    float nodeY;

    if (!(EtherTreeSystem->flags & 1))
        return;
    if (object == 0)
        return;

    EtherTreeSystem->targetObject = object;
    nodeX = -object->x;
    EtherTreeSystem->targetNodeX = nodeX;
    nodeY = -object->y;
    EtherTreeSystem->targetNodeY = nodeY;
    if (mode == 1) {
        EtherTreeSystem->nodeX = nodeX;
        EtherTreeSystem->nodeY = nodeY;
    }
}

extern void *memset(void *destination, int value, unsigned int count);
extern struct EtherTreeLine2Data *EtherTreeLine2;
extern struct EtherTreeRightData *EtherTreeRight;

void EtherTreeWorkClear(void)
{
    memset(EtherTreeSystem, 0, 0x80);
    memset(EtherTreeObject, 0, 0x2300);
    memset(EtherTreeLine, 0, 0x1900);
    memset(EtherTreeLine2, 0, 0x140);
    memset(EtherTreeRight, 0, 0x80);
}

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", EtherTreeObjectSet);

void EtherTreeCenterSet(int axisMask, float x, float y)
{
    if ((EtherTreeSystem->flags & 1) &&
        (!(axisMask & 1) || EtherTreeSystem->centerX != EtherTreeSystem->targetCenterX) &&
        (!(axisMask & 2) || EtherTreeSystem->centerY != EtherTreeSystem->targetCenterY)) {
        EtherTreeSystem->centerX = x;
        EtherTreeSystem->centerY = y;
        EtherTreeSystem->flags |= 2;
    }
}

void EtherTreeToCenterSet(float targetX, float targetY)
{
    EtherTreeSystem->targetCenterX = targetX;
    EtherTreeSystem->targetCenterY = targetY;
}

/* Move a floating-point position toward its target without crossing it. */
void subMoveSlide(float *position, float *target, float rate)
{
    float difference = *position - *target;

    if (difference == 0.0f)
        return;

    if (*target < *position) {
        *position = *position - (difference * rate + 1.0f);
        if (*position < *target)
            *position = *target;
    } else {
        *position = *position - (difference * rate - 1.0f);
        if (*target < *position)
            *position = *target;
    }
}

extern float D_004D7E80;

void EtherTreeCenterMove(void)
{
    EtherTreeSystemData *temp_16;
    float *temp_14;
    float *temp_15;
    float *temp_17;
    float *temp_18;
    float temp_f20;
    unsigned char temp_3;

    temp_16 = EtherTreeSystem;
    temp_14 = &temp_16->centerX;
    temp_15 = &temp_16->targetCenterX;
    temp_18 = &temp_16->nodeX;
    temp_17 = &temp_16->targetNodeX;
    temp_3 = temp_16->flags;
    if ((temp_3 & 1 & 0xFF) && __builtin_expect(temp_3 & 2, 0)) {
        temp_f20 = D_004D7E80;
        subMoveSlide(temp_14, temp_15, temp_f20);
        subMoveSlide(&temp_16->centerY, &temp_16->targetCenterY, temp_f20);
        subMoveSlide(temp_18, temp_17, temp_f20);
        subMoveSlide(&temp_16->nodeY, &temp_16->targetNodeY, temp_f20);
    }
}

typedef struct EtherTreeRightPos { long a, b; } EtherTreeRightPos;
typedef struct EtherTreeRightSetView {
    unsigned char flags;
    unsigned char mode;
    unsigned char pad02[0x0e];
    EtherTreeRightPos pos;
    EtherTreeObjectData *object;
    unsigned char pad24[0x08];
    unsigned char col0[3];
    signed char col0a;
    int counter;
    int size;
    unsigned char col1[3];
    unsigned char col1a;
    unsigned char pad3c[4];
} EtherTreeRightSetView;

void EtherTreeRightSet(int id, int index)
{
    EtherTreeRightSetView *record = (EtherTreeRightSetView *)EtherTreeRight + index;
    EtherTreeObjectData *object;

    if (!(unsigned char)(EtherTreeSystem->flags & 1))
        return;

    object = EtherTreeObjectGet(id);
    record->object = object;
    if (object == 0)
        return;

    record->mode = 0;
    record->flags |= 1;
    record->counter = 0;
    record->pos.a = ((EtherTreeRightPos *)((char *)object + 0x20))->a;
    *(volatile long *)&record->pos.b = ((EtherTreeRightPos *)((char *)object + 0x20))->b;
    record->size = record->col0a = 0x20;
    *(volatile unsigned char *)&record->col1[0] = 0x80;
    *(volatile unsigned char *)&record->col1a = 0;
    *(volatile unsigned char *)&record->col0[2] = 0x80;
    *(volatile unsigned char *)&record->col0[1] = 0x80;
    *(volatile unsigned char *)&record->col0[0] = 0x80;
    *(volatile unsigned char *)&record->col1[2] = 0x80;
    *(volatile unsigned char *)&record->col1[1] = 0x80;
}

/*
 * EtherTreeRight is a 2-entry array of "right panel" object-highlight slot
 * records (indexed by index*0x40 in EtherTreeRightSet, this unit, so each
 * record is 0x40 bytes). flags bit 0 marks the slot active; every function
 * here is itself gated by EtherTreeSystem->flags bit 0. mode drives a
 * 4-state open/close animation (subEtherTreeRightMain): 0 idle, 1 opening
 * (counter counts up by 8 to 48, then mode becomes 2), 2 held open (counter
 * pinned at 48), 3 closing (counter counts down by 8 to 0, then flags bit 0
 * is cleared and mode returns to 0). object is the EtherTreeObjectGet()
 * record the slot currently tracks (EtherTreeRightSet, this unit;
 * EtherTreeRightTargetChange). Bytes no function claimed here writes or
 * reads stay unmodeled.
 */
typedef struct EtherTreeRightData {
    unsigned char flags;
    unsigned char mode;
    unsigned char unmodeled_02[0x1e];
    EtherTreeObjectData *object;
    unsigned char unmodeled_24[0x0c];
    int counter;
    unsigned char unmodeled_34[0x0c];
} EtherTreeRightData;

void EtherTreeRightModeChange(int mode, int index)
{
    EtherTreeRightData *record = &EtherTreeRight[index];

    if (!(unsigned char)(EtherTreeSystem->flags & 1))
        return;

    if (!(unsigned char)(record->flags & 1))
        return;

    switch (record->mode) {
    case 1:
    case 2:
        if (mode != 1 && mode != 2)
            record->mode = mode;
        break;
    case 3:
        if (mode != 3 && mode != 0)
            record->mode = mode;
        break;
    case 0:
        record->mode = mode;
        break;
    }
}

extern EtherTreeRightData *EtherTreeRight;
extern EtherTreeObjectData *EtherTreeObjectGet(int id);

void EtherTreeRightTargetChange(int id, int index)
{
    EtherTreeRightData *record = &EtherTreeRight[index];

    if (!(EtherTreeSystem->flags & 1))
        return;
    if (record->flags & 1) {
        record->object = EtherTreeObjectGet(id);
    }
}

extern void subEtherTreeRightMain(EtherTreeRightData *record);

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", subEtherTreeRightMain);

void EtherTreeRightMain(void)
{
    int i;

    if (!(EtherTreeSystem->flags & 1))
        return;

    for (i = 0; i < 2; i++) {
        subEtherTreeRightMain(&EtherTreeRight[i]);
    }
}

extern float D_004D7E84;
extern void endPrintExtFunc(int kind, int id, void *data);
typedef struct EtherTreeRightDrawView {
    unsigned char unmodeled_00[0x10];
    float x;
    float y;
    unsigned char unmodeled_18[0x08];
    char *object;
    short screenX;
    short screenY;
    unsigned int depth;
} EtherTreeRightDrawView;

void subRightDraw(EtherTreeRightData *record)
{
    EtherTreeRightDrawView *view = (EtherTreeRightDrawView *)record;
    float rate = D_004D7E84;
    EtherTreeSystemData *sys = EtherTreeSystem;
    float *px = &view->x;
    float *py = &view->y;
    float *node;
    float *center;

    subMoveSlide(px, (float *)(view->object + 0x20), rate);
    node = &sys->nodeX;
    center = &sys->centerX;
    subMoveSlide(py, (float *)(view->object + 0x24), rate);
    view->screenX = *px + node[0] + center[0];
    view->screenY = *py + node[1] + center[1];
    view->depth = node[2] + 4.0f;
    endPrintExtFunc(0, 0x11, &view->screenX);
}

extern void subRightDraw(EtherTreeRightData *record);

void EtherTreeRightDraw(void)
{
    int i;
    EtherTreeRightData *record;

    if (!(unsigned char)(EtherTreeSystem->flags & 1))
        return;

    for (i = 0; i < 2; i++) {
        record = &EtherTreeRight[i];
        if ((unsigned char)(record->flags & 1)) {
            if (record->mode == 0)
                continue;
            subRightDraw(record);
        }
    }
}

/*
 * EtherTreeBlack is the fade-to-black slot: this unit provides the same
 * Set/ModeChange/Main/Draw family for it as for EtherTreeRight. mode drives
 * its animation and counter is the frame timer (EtherTreeBlackModeChange,
 * EtherTreeBlackMain); EtherTreeBlackSet resets both to idle. Bytes no
 * function claimed here writes or reads stay unmodeled.
 */
typedef struct EtherTreeBlackData {
    unsigned char unmodeled_00;
    unsigned char mode;
    short counter;
} EtherTreeBlackData;

extern EtherTreeBlackData *EtherTreeBlack;

void EtherTreeBlackSet(void)
{
    EtherTreeBlack->counter = (EtherTreeBlack->mode = 0);
}

void EtherTreeBlackModeChange(int mode)
{
    if (!(unsigned char)(EtherTreeSystem->flags & 1))
        return;

    switch (EtherTreeBlack->mode) {
    case 1:
    case 2:
        if (mode != 1 && mode != 2)
            EtherTreeBlack->mode = mode;
        break;
    case 3:
        if (mode != 3 && mode != 0)
            EtherTreeBlack->mode = mode;
        break;
    case 0:
        EtherTreeBlack->mode = mode;
        break;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", EtherTreeBlackDraw);

void EtherTreeBlackMain(void)
{
    EtherTreeBlackData *black = EtherTreeBlack;

    if (!(unsigned char)(EtherTreeSystem->flags & 1))
        return;

    switch (black->mode) {
    case 1:
        black->counter += 8;
        if (black->counter >= 0x50) {
            black->mode = 2;
        case 2:
            black->counter = 0x50;
        }
        break;
    case 0:
        return;
    case 3:
        black->counter -= 8;
        if (black->counter <= 0) {
            black->counter = 0;
            black->mode = 0;
        }
        break;
    }
}

/*
 * subLine2_DrawType_1 (this unit, main:0x002bb448) draws the block's three
 * lines: each one runs from a corner of from[] to the matching corner of
 * to[], both offset by the tree node and the screen centre and scaled by
 * EtherTreeSystem, and both vertices of the pair take line colour 3 before
 * the pair goes to endPrintExtFunc(0, 0x10, ...). The bytes it leaves alone
 * stay unmodeled.
 */
typedef struct EtherTreeLine2Corner {
    float x;
    float y;
    unsigned char unmodeled_08[0x08];
} EtherTreeLine2Corner;

/* EtherTreeLineColorGet (this unit) fills the four colour bytes at 0x10. */
typedef struct EtherTreeLine2Vertex {
    float x;
    float y;
    float z;
    unsigned char unmodeled_0c[0x04];
    unsigned char color[0x04];
    unsigned char unmodeled_14[0x0c];
} EtherTreeLine2Vertex;

typedef struct EtherTreeLine2Line {
    EtherTreeLine2Vertex from;
    EtherTreeLine2Vertex to;
} EtherTreeLine2Line;

/*
 * EtherTreeLine2 is the second lower-menu line control block, gated like
 * every function in this file by EtherTreeSystem->flags bit 0.
 * selectedIndex is the index EtherTreeLine2SelectChange last latched;
 * changing it replays EtherTreeLine2ModeChange(0) (close) before
 * EtherTreeLine2ModeChange(1) (open) on the new index. Bytes no function
 * claimed here writes or reads stay unmodeled.
 */
typedef struct EtherTreeLine2Data {
    unsigned char flags;
    unsigned char mode;
    unsigned char opened;
    unsigned char unmodeled_03[0x01];
    int selectedIndex;
    unsigned char unmodeled_08[0x08];
    EtherTreeLine2Corner from[3];
    EtherTreeLine2Corner to[3];
    EtherTreeObjectData *object;
    unsigned char unmodeled_74[0x0c];
    EtherTreeLine2Line line[3];
} EtherTreeLine2Data;

typedef union { EtherTreeLine2Corner c; } Line2CornerView;

void EtherTreeLine2Set(int id, int index)
{
    EtherTreeLine2Data *line2 = EtherTreeLine2;
    EtherTreeLine2Corner *corner;
    int i;

    if (EtherTreeSystem->flags & 1 & 0xFF) {
        line2->object = EtherTreeObjectGet(id);
        i = 0;
        if (line2->object->childCount != 0) {
            corner = line2->to;
            do {
                ((Line2CornerView *)corner)->c = *(EtherTreeLine2Corner *)((char *)line2->object + 0x20);
                corner[-3] = *corner;
                corner++;
                i++;
            } while (i < line2->object->childCount);
        }
        line2->unmodeled_03[0] = index;
        line2->mode = 0;
        line2->flags |= 1;
        line2->opened = 0;
    }
}

void EtherTreeLine2ModeChange(int mode)
{
    EtherTreeLine2Data *line2 = EtherTreeLine2;
    EtherTreeLine2Corner *corner;
    int i;

    if (!(unsigned char)(EtherTreeSystem->flags & 1))
        return;

    if (!(unsigned char)(line2->flags & 1))
        return;

    switch (mode) {
    case 1:
    case 2:
        if ((unsigned char)(line2->mode - 1) >= 2) {
            line2->mode = mode;
            if (line2->mode == 1) {
                line2->opened = 0;
                i = 0;
                corner = line2->to;
                for (; i < 3; i++) {
                    ((Line2CornerView *)corner)->c = *(EtherTreeLine2Corner *)((char *)line2->object + 0x20);
                    ((Line2CornerView *)&corner[-3])->c = *corner;
                    corner++;
                }
            }
        }
        break;
    case 3:
        if (line2->mode != 3 && line2->mode != 0)
            line2->mode = mode;
        break;
    case 0:
        line2->mode = mode;
        break;
    }
}

void EtherTreeLine2ModeChange(int mode);

void EtherTreeLine2SelectChange(int index)
{
    EtherTreeLine2Data *line2 = EtherTreeLine2;

    if ((unsigned char)(EtherTreeSystem->flags & 1)) {
        if ((unsigned char)(line2->flags & 1)) {
            if (line2->selectedIndex != index) {
                line2->selectedIndex = index;
                EtherTreeLine2ModeChange(0);
            }
            EtherTreeLine2ModeChange(1);
        }
    }
}

extern void EtherTreeLineColorGet(unsigned char *color, int colorIndex);
extern void endPrintExtFunc(int kind, int id, void *data);

/*
 * Build the three lines of the block: line[i] runs from corner from[i] to
 * corner to[i], each offset by the tree node and the screen centre and
 * scaled, and both of its vertices sit one unit in front of the node plane.
 * corner walks the four floats the pair needs (from[i].x, from[i].y,
 * to[i].x, to[i].y) and block walks the 0x40 bytes of one line record.
 */
void subLine2_DrawType_1(EtherTreeLine2Data *line2)
{
    float *node = &EtherTreeSystem->nodeX;
    float *center = &EtherTreeSystem->centerX;
    float *corner = &line2->to[0].y;
    EtherTreeLine2Data *block = line2;
    EtherTreeLine2Line *line;
    int remaining = 2;

    do {
        line = &block->line[0];
        remaining -= 1;
        block->line[0].from.x = ((corner[-13] + node[0]) + center[0]) * node[3];
        block->line[0].from.y = ((corner[-12] + node[1]) + center[1]) * node[3];
        block->line[0].from.z = node[2] - 1.0f;
        block->line[0].to.x = ((corner[-1] + node[0]) + center[0]) * node[3];
        block->line[0].to.y = ((corner[0] + node[1]) + center[1]) * node[3];
        block->line[0].to.z = node[2] - 1.0f;
        corner += 4;
        EtherTreeLineColorGet(block->line[0].from.color, 3);
        EtherTreeLineColorGet(block->line[0].to.color, 3);
        block = (EtherTreeLine2Data *) (((char *) block) + 0x40);
        endPrintExtFunc(0, 0x10, line);
    } while (remaining >= 0);
}

extern EtherTreeLine2Data *EtherTreeLine2;
extern void subLine2_DrawType_1(EtherTreeLine2Data *line2);

void EtherTreeLine2Draw(void)
{
    EtherTreeLine2Data *line2 = EtherTreeLine2;

    if (!(EtherTreeSystem->flags & 1))
        return;
    if (!(line2->flags & 1))
        return;

    subLine2_DrawType_1(line2);
}

typedef struct {
    unsigned long long w[2];
} Line2Quad;

int subLine2_OpenType_0(EtherTreeLine2Data *line2)
{
    char *object = (char *)line2->object;
    float limit;

    switch (line2->opened) {
    case 0:
        *(Line2Quad *)&line2->to[0] = *(Line2Quad *)(object + 0x20);
        ((Line2CornerView *)&line2->from[0])->c = line2->to[0];
        line2->opened = 1;
    case 1:
        line2->to[0].x += 4.0f;
        limit = *(float *)(*(char **)(object + 0xC) + 0x20);
        if (line2->to[0].x < limit) {
            return 0;
        }
        line2->to[0].x = limit;
        return 1;
    }
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", subLine2_OpenType_1);

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", EtherTreeLine2Main);

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", sub2ObjectLampDraw);

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", EtherTreeObjectDraw);

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", subTreeLineDraw);

void subTreeLineDraw_type_0(void)
{
}

typedef struct TLNode { char pad0[0xC]; struct TLNode *child; char pad10[8]; unsigned char flags; char pad19[7]; long long p0; long long p1; } TLNode;
typedef struct { TLNode *node; int pad[3]; long long a0; long long a1; int pad2[4]; long long b0; long long b1; } TLWork;
void subTreeLineDraw(TLWork *w, int *arg);

void subTreeLineDraw_type_1(TLWork *w)
{
    int arg[4];
    TLNode *n = w->node;
    TLNode *c = n->child;
    w->a0 = n->p0;
    w->a1 = n->p1;
    w->b0 = c->p0;
    w->b1 = c->p1;
    if ((unsigned char)(c->flags & 1)) {
        arg[0] = 2;
    } else {
        arg[0] = 1;
    }
    subTreeLineDraw(w, arg);
}

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", subTreeLineDraw_type_2);

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", subTreeLineDraw_type_3);

typedef struct { void (*f[4])(); } LineFnTbl;
extern const LineFnTbl D_004C9AD0;

void EtherTreeLineDraw(void)
{
    int line = (int)EtherTreeLine;
    LineFnTbl table = D_004C9AD0;
    EtherTreeLineData *p;
    int i = 15;
    p = (EtherTreeLineData *)line;
    do {
        if (p->object != 0)
            table.f[p->object->childCount](p);
        i--;
        p++;
    } while (i >= 0);
}

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", EtherTreeCursolDraw);

extern void EtherTreeObjectDraw(void);
extern void EtherTreeLineDraw(void);
extern void EtherTreeCursolDraw(void);
extern void EtherTreeRightDraw(void);
extern void EtherTreeBlackDraw(void);
extern int WindowTexAddrGet(int index);
extern void endPrintExtFunc(int kind, int id, void *data);

void EtherTreeDraw(void)
{
    if (!(EtherTreeSystem->flags & 1))
        return;

    EtherTreeSystem->windowTexAddr = WindowTexAddrGet(2);
    endPrintExtFunc(0, 0xE, &EtherTreeSystem->windowTexAddr);
    EtherTreeObjectDraw();
    EtherTreeLineDraw();
    EtherTreeLine2Draw();
    EtherTreeCursolDraw();
    EtherTreeRightDraw();
    EtherTreeBlackDraw();
    endPrintExtFunc(0, 0xF, 0);
}

/*
 * Carves the ether-tree work areas out of a 16-byte-aligned arena and
 * returns the address just past the last one (arenaBase + 0x3e80).
 */
int EtherTreeInit(int arenaBase)
{
    int addr = (arenaBase + 0xf) & ~0xf;

    EtherTreeSystem = (EtherTreeSystemData *) addr;
    addr += 0x80;
    EtherTreeObject = (EtherTreeObjectData *) addr;
    addr += 0x2300;
    EtherTreeLine = (EtherTreeLineData *) addr;
    addr += 0x1900;
    EtherTreeLine2 = (EtherTreeLine2Data *) addr;
    addr += 0x140;
    EtherTreeRight = (EtherTreeRightData *) addr;
    addr += 0x80;
    EtherTreeBlack = (EtherTreeBlackData *) addr;
    addr += 0x40;
    EtherTreeWorkClear();
    EtherTreeBlackSet();
    return addr;
}

/* True for a robot ether index (annotations/slus_204.69_annotations.csv: 13 through 16). */
static int RoboEtherCheck(int etherIndex)
{
    return (unsigned int) (etherIndex - 13) < 4U;
}

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", EtherTreeMain);

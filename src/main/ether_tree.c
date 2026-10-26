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


/*
 * EtherTreeCenterSet, EtherTreeToCenterSet and EtherTreeCenterMove (this
 * unit, main/tu179) all read or write this struct through EtherTreeSystem.
 * flags bit 0 gates EtherTreeCenterSet, EtherTreeCenterMove and
 * EtherTreeDraw; EtherTreeCenterSet sets bit 1 and EtherTreeCenterMove
 * tests it before sliding centerX/centerY toward targetCenterX/
 * targetCenterY with subMoveSlide. Bytes no function claimed here writes or
 * reads stay unmodeled.
 */

typedef struct EtherTreeCoordinates {
    float x;
    float y;
} EtherTreeCoordinates;

typedef struct EtherTreeNodeTransform {
    float x;
    float y;
    float z;
    float scale;
} EtherTreeNodeTransform;

typedef struct EtherTreeSystemData {
    unsigned char flags;
    unsigned char unmodeled_01;
    unsigned short firstDataIndex;
    unsigned char unmodeled_04[0x0c];
    EtherTreeCoordinates center;
    unsigned char unmodeled_18[0x08];
    EtherTreeCoordinates targetCenter;
    unsigned char unmodeled_28[0x08];
    EtherTreeNodeTransform node;
    EtherTreeCoordinates targetNode;
    unsigned char unmodeled_48[0x08];
    EtherTreeObjectData *targetObject;
    unsigned char unmodeled_54[0x1c];
    unsigned char *windowTexAddr;
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


typedef struct EtherTreeFirstDataEntry {
    unsigned char childIds[3];
    unsigned char unmodeled_03;
} EtherTreeFirstDataEntry;

extern EtherTreeFirstDataEntry EtherTreeFirstData[7];


struct EtherTreeColorTable {
    unsigned char colors[4][4];
};

const struct EtherTreeColorTable D_004C9AB0 = {
    { { 0x00, 0x00, 0x00, 0x00 },
      { 0x40, 0x40, 0x40, 0x60 },
      { 0x80, 0x80, 0x80, 0x80 },
      { 0xa0, 0xa0, 0x40, 0x80 } }
};

extern EtherTreeObjectData *EtherTreeObjectWorkGet(void);

extern unsigned char *MenuEtherDataGet(unsigned short skillId);

extern EtherTreeLineData *EtherTreeLine;


/* Collect the active non-root object records into the line table. */

extern EtherTreeObjectData *EtherTreeObjectGet(int id);

extern void *memset(void *destination, int value, unsigned int count);

extern struct EtherTreeLine2Data *EtherTreeLine2;


extern struct EtherTreeRightData *EtherTreeRight;


/* Move a floating-point position toward its target without crossing it. */

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
    unsigned char unmodeled_02[0xe];
    EtherTreePosition position;
    EtherTreeObjectData *object;
    unsigned char unmodeled_24[0]; /* +0x24: zero-length alias of the former padding member, kept for saved bodies */
    short screenX;
    short screenY;
    unsigned int depth;
    unsigned char color[4];
    int counter;
    unsigned char unmodeled_34[0]; /* +0x34: zero-length alias of the former padding member, kept for saved bodies */
    int height;
    unsigned char borderColor[4];
    unsigned char unmodeled_3c[0x4];
} EtherTreeRightData;

extern void subEtherTreeRightMain(EtherTreeRightData *record);

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


/*
 * subLine2_DrawType_1 (this unit, main:0x002bb448) draws the block's three
 * lines: each one runs from a corner of from[] to the matching corner of
 * to[], both offset by the tree node and the screen centre and scaled by
 * EtherTreeSystem, and both vertices of the pair take line colour 3 before
 * the pair goes to endPrintExtFunc(0, 0x10, ...). The bytes it leaves alone
 * stay unmodeled.
 */

typedef EtherTreePosition EtherTreeLine2Corner;

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
    unsigned char counter;
    unsigned char selectionType;
    int selectedIndex;
    unsigned char unmodeled_08[0x08];
    EtherTreeLine2Corner from[3];
    EtherTreePosition to[3];
    EtherTreeObjectData *object;
    unsigned char unmodeled_74[0x0c];
    EtherTreeLine2Line line[3];
} EtherTreeLine2Data;

extern void EtherTreeLineColorGet(unsigned char *color, int colorIndex);

extern void endPrintExtFunc(int kind, int id, void *data);

/*
 * Build the three lines of the block: line[i] runs from corner from[i] to
 * corner to[i], each offset by the tree node and the screen centre and
 * scaled, and both of its vertices sit one unit in front of the node plane.
 * corner walks the four floats the pair needs (from[i].x, from[i].y,
 * to[i].x, to[i].y) and block walks the 0x40 bytes of one line record.
 */

extern void subLine2_DrawType_1(EtherTreeLine2Data *line2);

extern void EtherTreeObjectDraw(void);

extern void EtherTreeLineDraw(void);

extern void EtherTreeCursolDraw(void);

extern void EtherTreeRightDraw(void);

extern void EtherTreeBlackDraw(void);

extern unsigned char *WindowTexAddrGet(int index);

#include "ov01/data_unit_org_get.h"

typedef struct EtherTreeCharacterRecord {
    unsigned char unmodeled_00[0x28];
    signed char learnedEther[16];
} EtherTreeCharacterRecord;

extern EtherTreeCharacterRecord *func_A19210(int characterId);

extern int MenuEtherSetCheck(unsigned short firstDataIndex, unsigned int objectId);

extern int func_A19578(unsigned short firstDataIndex, unsigned int objectId);

extern signed char MenuEtherTypeGet(int etherId);

/* Only a .lit4 literal in the original (0x004D7E7C); no storage object. */
#define D_004D7E7C 65535.0f

struct EtherTreeLine2Data;

void subPosSet3(void);

void subJoutoPosSet(void);

void subMoveSlide(float *position, float *target, float rate);

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

void EtherTreeLineColorGet(unsigned char *color, int colorIndex)
{
    struct EtherTreeColorTable colors = D_004C9AB0;
    int i;

    for (i = 0; i < 4; i++)
        color[i] = colors.colors[colorIndex][i];
}

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

int subPosSet(EtherTreeObjectData *object)
{
    int total;
    int i;

    total = 0;
    if (object->childCount == 0) {
        total = 1;
    } else {
        for (i = 0; i < object->childCount; i++) {
            float childX = 57.0f;
            float childDepthStep = 1.0f;
            float twoChildLeft = -36.0f;
            float twoChildRight = 36.0f;
            float threeChildLeft = -72.0f;
            float threeChildRight = 72.0f;
            float childDepth;
            unsigned short childCount;
            EtherTreeObjectData *child = object->children[i];

            childCount = object->childCount;
            child->position.vector.x = childX;
            childDepth = object->position.vector.z;
            child->position.vector.y = 0.0f;
            child->position.vector.z = childDepth + childDepthStep;
            switch (childCount) {
            case 2:
                if (i == 0)
                    child->position.vector.y = twoChildLeft;
                else
                    child->position.vector.y = twoChildRight;
                break;
            case 3:
                if (i == 0)
                    child->position.vector.y = threeChildLeft;
                else if (i == 2)
                    child->position.vector.y = threeChildRight;
                break;
            }

            total += subPosSet(child);
        }
    }

    if (object->position.vector.y < 0.0f)
        object->position.vector.y -= (float) (total - 1) * 36.0f;
    else if (object->position.vector.y > 0.0f)
        object->position.vector.y += (float) (total - 1) * 36.0f;

    if (EtherTreeSystem->firstDataIndex == 6 && (object->flags & 0x20))
        object->children[2]->position.vector.y += 36.0f;

    return total;
}

void subPosSet2(EtherTreeObjectData *object, Vector4 *parentPosition)
{
    EtherTreePosition position;
    int i;

    if (object == 0)
        return;

    position = object->position;
    if (parentPosition != 0) {
        position.vector.x += parentPosition->x;
        position.vector.y += parentPosition->y;
    }

    for (i = 0; i < object->childCount; i++)
        subPosSet2(object->children[i], &position.vector);

    object->position = position;
}

void subPosSet3(void)
{
    float midpointScale = 0.5f;
    EtherTreeObjectData *object = EtherTreeObject;
    int index;

    for (index = 0; index < 80; index++) {
        switch (object->childCount) {
        case 2: {
            EtherTreeObjectData *first = object->children[0];
            EtherTreeObjectData *last = object->children[1];

            object->position.vector.y = ((last->position.vector.y - first->position.vector.y) * midpointScale) + first->position.vector.y;
            break;
        }
        case 3: {
            EtherTreeObjectData *first = object->children[0];
            EtherTreeObjectData *last = object->children[2];

            object->position.vector.y = ((last->position.vector.y - first->position.vector.y) * midpointScale) + first->position.vector.y;
            break;
        }
        }
        object++;
    }
}

void sub2JoutoYGet(EtherTreeObjectData *object, float *y)
{
    if (object->childCount != 0)
        sub2JoutoYGet(object->children[object->childCount - 1], y);
    else
        *y = object->position.vector.y;
}

extern int MenuEtherWhoCheck(int id);

void subJoutoPosSet(void)
{
    float *node = &EtherTreeSystem->node.x;
    unsigned short firstDataIndex;
    EtherTreeObjectData *root;
    EtherTreeObjectData *object;
    EtherTreeObjectData *top;
    float pos[4];
    float baseX = 57.0f;
    float stepX = 57.0f;
    int count = 0;
    int id = 1;

    firstDataIndex = EtherTreeSystem->firstDataIndex;
    root = EtherTreeObjectWorkGet();
    top = EtherTreeObject;
    *(EtherTreeObjectData **)EtherTreeSystem->unmodeled_54 = root;
    object = root;
    sub2JoutoYGet(top, pos);
    pos[0] += 72.0f;
    do {
        if (firstDataIndex != (unsigned short)MenuEtherWhoCheck(id)
            && ((int (*)(int, int))func_A19578)(firstDataIndex, id) != 0) {
            object->flags |= 8;
            object->id = id;
            object->position.vector.x = baseX + (float)(count % 4) * stepX;
            object->position.vector.y = pos[0] + (float)(count / 4) * 72.0f;
            object->position.vector.z = node[2];
            count++;
            object++;
        }
        id++;
    } while (id < 0x50);
    root->childCount = count;
}

void EtherTreeParaSet(EtherTreeObjectData *object)
{
    unsigned short firstDataIndex = EtherTreeSystem->firstDataIndex;
    signed char type;

    if (object->id == 0)
        return;

    if (object->flags & 4)
        object->flags |= 8;

    if (MenuEtherSetCheck(firstDataIndex, object->id)) {
        object->isSet = 1;
        object->flags |= 2;
        object->setValue = 8;
    } else {
        object->flags &= ~2;
    }

    if (func_A19578(firstDataIndex, object->id) != 0) {
        object->flags = (object->flags | 9) & ~0x10;
    } else if (object->flags & 4) {
        object->flags |= 0x10;
    }

    type = MenuEtherTypeGet(object->id);
    switch (type) {
    case 0:
        object->displayType = 0;
        break;
    case 1:
    case 2:
        object->displayType = 1;
        break;
    default:
        object->displayType = 2;
        break;
    }
}

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

void EtherTreeTargetChange(int id, int mode)
{
    EtherTreeObjectData *object = EtherTreeObjectGet(id);
    float nodeX;
    float nodeY;

    if (!(EtherTreeSystem->flags & 1))
        return;
    if (object == 0)
        return;

    nodeX = -object->position.vector.x;
    EtherTreeSystem->targetObject = object;
    EtherTreeSystem->targetNode.x = nodeX;
    nodeY = -object->position.vector.y;
    EtherTreeSystem->targetNode.y = nodeY;
    if (mode == 1) {
        EtherTreeSystem->node.x = nodeX;
        EtherTreeSystem->node.y = nodeY;
    }
}

void EtherTreeWorkClear(void)
{
    memset(EtherTreeSystem, 0, 0x80);
    memset(EtherTreeObject, 0, 0x2300);
    memset(EtherTreeLine, 0, 0x1900);
    memset(EtherTreeLine2, 0, 0x140);
    memset(EtherTreeRight, 0, 0x80);
}

void EtherTreeObjectSet(int firstDataIndex)
{
    EtherTreeCharacterRecord *etherData;
    signed char *zeroRun;
    unsigned char *firstDataIds;
    int zeroCount;
    int i;

    memset(EtherTreeObject, 0, 0x2300);
    memset(EtherTreeLine, 0, 0x1900);
    memset(EtherTreeLine2, 0, 0x140);

    etherData = func_A19210(firstDataIndex);
    zeroRun = etherData->learnedEther;
    zeroCount = 0;
    if (zeroRun[0] == 0) {
        do {
            zeroCount++;
        } while ((++zeroRun, zeroCount < 0x10 && zeroRun[0] == 0));
    }

    if (zeroCount == 0x10)
        return;

    EtherTreeSystem->flags |= 1;
    EtherTreeSystem->firstDataIndex = (unsigned short) firstDataIndex;
    EtherTreeObjectGetClear();
    subParentChildSet();
    subPosSet(EtherTreeObject);
    subPosSet2(EtherTreeObject, 0);
    subPosSet3();
    subJoutoPosSet();
    for (i = 0; i < 80; i++)
        EtherTreeParaSet(&EtherTreeObject[i]);
    EtherTreeLineSet();
    firstDataIds = EtherTreeFirstDataGet();
    EtherTreeTargetChange(firstDataIds[0], 1);
    EtherTreeSystem->node.z = D_004D7E7C;
    EtherTreeSystem->node.scale = 1.0f;
}

void EtherTreeCenterSet(int axisMask, float x, float y)
{
    if ((EtherTreeSystem->flags & 1) &&
        (!(axisMask & 1) || EtherTreeSystem->center.x != EtherTreeSystem->targetCenter.x) &&
        (!(axisMask & 2) || EtherTreeSystem->center.y != EtherTreeSystem->targetCenter.y)) {
        EtherTreeSystem->center.x = x;
        EtherTreeSystem->center.y = y;
        EtherTreeSystem->flags |= 2;
    }
}

void EtherTreeToCenterSet(int axisMask, float targetX, float targetY)
{
    EtherTreeSystem->targetCenter.x = targetX;
    EtherTreeSystem->targetCenter.y = targetY;
}

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

void EtherTreeCenterMove(void)
{
    EtherTreeSystemData *system = EtherTreeSystem;
    float *centerX = &system->center.x;
    float *targetCenterX = &system->targetCenter.x;
    float *targetNodeX;
    float *nodeX;

    nodeX = &system->node.x;
    targetNodeX = &system->targetNode.x;
    if (system->flags & 1) {
        if (system->flags & 2) {
            float rate = 0.4f;

            subMoveSlide(centerX, targetCenterX, rate);
            subMoveSlide(&system->center.y, &system->targetCenter.y, rate);
            subMoveSlide(nodeX, targetNodeX, rate);
            subMoveSlide(&system->node.y, &system->targetNode.y, rate);
        }
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

void EtherTreeRightTargetChange(int id, int index)
{
    EtherTreeRightData *record = &EtherTreeRight[index];

    if (!(EtherTreeSystem->flags & 1))
        return;
    if (record->flags & 1) {
        record->object = EtherTreeObjectGet(id);
    }
}

void subEtherTreeRightMain(EtherTreeRightData *record)
{
    if (!(EtherTreeSystem->flags & 1))
        return;
    if (!(record->flags & 1))
        return;

    switch (record->mode) {
    case 0:
        break;
    case 1:
        record->counter += 8;
        if (record->counter < 0x30)
            break;
        record->mode = 2;
    case 2:
        record->counter = 0x30;
        break;
    case 3:
        record->counter -= 8;
        if (record->counter <= 0) {
            record->flags &= ~1;
            record->mode = 0;
            record->counter = 0;
        }
        break;
    }
}

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
    node = &sys->node.x;
    center = &sys->center.x;
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

typedef struct BlackVtx {
    short x;
    short y;
    unsigned int z;
    unsigned char r, g, b, a;
} BlackVtx;
typedef struct BlackPrim {
    BlackVtx v[4];
    int pad30;
    int tail;
} BlackPrim;
typedef struct BlackView {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char alpha;
    unsigned char pad03;
    BlackPrim prim;
} BlackView;
static inline void setVtxZ(BlackVtx *v, unsigned int z)
{
    v->z = z;
}

void EtherTreeBlackDraw(void)
{
    BlackView *blk = (BlackView *)EtherTreeBlack;
    BlackPrim *data = &blk->prim;
    BlackVtx *v1, *v2, *v3;
    BlackVtx *w1, *w2, *w3;
    unsigned int z;
    short hy;

    if (!(unsigned char)(EtherTreeSystem->flags & 1))
        return;
    if (!blk->mode)
        return;
    w1 = &blk->prim.v[1];
    w2 = &blk->prim.v[2];
    w3 = &blk->prim.v[3];
    z = EtherTreeSystem->node.z + 2.0f;
    hy = 0x1C0;
    *(volatile unsigned int *)&w3->z = z;
    *(volatile short *)&data->v[0].x = 0;
    *(volatile unsigned int *)&w2->z = z;
    *(volatile unsigned int *)&w1->z = z;
    *(volatile short *)&data->v[0].y = 0;
    *(volatile short *)&data->v[1].x = 0;
    *(volatile short *)&data->v[2].y = 0;
    *(volatile short *)&data->v[3].x = 0x200;
    *(volatile short *)&data->v[3].y = hy;
    *(volatile short *)&data->v[1].y = hy;
    *(volatile short *)&data->v[2].x = 0x200;
    *(volatile unsigned int *)&data->v[0].z = z;
    v1 = &data->v[1];
    v2 = &data->v[2];
    v3 = &data->v[3];
    data->v[0].b = v1->b = v2->b = v3->b = 0;
    data->v[0].g = v1->g = v2->g = v3->g = 0;
    data->v[0].r = v1->r = v2->r = v3->r = 0;
    data->v[0].a = v1->a = v2->a = v3->a = blk->alpha;
    data->tail = 0;
    endPrintExtFunc(0, 3, data);
}

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

typedef struct { unsigned char b[16]; } SetCorner;
typedef union { SetCorner c; } SetCornerView;

void EtherTreeLine2Set(int id, int index)
{
    EtherTreeLine2Data *line2 = EtherTreeLine2;
    SetCorner *corner;
    int i;

    if (EtherTreeSystem->flags & 1 & 0xFF) {
        line2->object = EtherTreeObjectGet(id);
        i = 0;
        if (line2->object->childCount != 0) {
            corner = (SetCorner *)line2->to;
            do {
                ((SetCornerView *)corner)->c = *(SetCorner *)((char *)line2->object + 0x20);
                corner[-3] = *corner;
                corner++;
                i++;
            } while (i < line2->object->childCount);
        }
        line2->counter = 0;
        line2->selectionType = index;
        line2->mode = 0;
        line2->flags = line2->flags | 1;
    }
}

typedef struct { unsigned char b[16]; } Line2Corner;
typedef union { Line2Corner c; } Line2CornerView;

void EtherTreeLine2ModeChange(int mode)
{
    EtherTreeLine2Data *line2 = EtherTreeLine2;
    Line2Corner *corner;
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
                line2->counter = 0;
                i = 0;
                corner = (Line2Corner *)line2->to;
                for (; i < 3; i++) {
                    ((Line2CornerView *)corner)->c = *(Line2Corner *)((char *)line2->object + 0x20);
                    corner[-3] = *corner;
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

static inline float *line2EndpointY(EtherTreeLine2Data *line2, unsigned int index)
{
    return &line2->to[index].vector.y;
}

void subLine2_DrawType_1(EtherTreeLine2Data *line2)
{
    float *node = &EtherTreeSystem->node.x;
    float *center = &EtherTreeSystem->center.x;
    float *corner = &line2->to[0].vector.y;
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

void EtherTreeLine2Draw(void)
{
    EtherTreeLine2Data *line2 = EtherTreeLine2;

    if (!(EtherTreeSystem->flags & 1))
        return;
    if (!(line2->flags & 1))
        return;

    subLine2_DrawType_1(line2);
}

int subLine2_OpenType_0(EtherTreeLine2Data *line2)
{
    unsigned char counter = line2->counter;
    EtherTreeObjectData *object = line2->object;
    EtherTreeObjectData *firstChild;

    switch (counter) {
    case 0:
        line2->to[0] = object->position;
        line2->from[0].vector = line2->to[0].vector;
        line2->counter = 1;
        /* fall through */
    case 1:
        firstChild = object->children[0];
        line2->to[0].vector.x += 4.0f;
        if (line2->to[0].vector.x < firstChild->position.vector.x)
            return 0;

        line2->to[0].vector.x = firstChild->position.vector.x;
        return 1;

    default:
        return 0;
    }
}

typedef struct { float x; float y; unsigned char unmodeled_08[0x08]; } Line2Pt;
typedef struct { Line2Pt c __attribute__((aligned(8))); } Line2PtA;
typedef union { Line2Pt c; } Line2PtView;

int subLine2_OpenType_1(EtherTreeLine2Data *line2)
{
    char *object = (char *)line2->object;
    char *child;
    float limit;

    switch (line2->counter) {
    case 0:
        line2->counter = 1;
        line2->to[0] = *(EtherTreePosition *)(object + 0x20);
        ((Line2PtView *)&line2->from[0])->c = *(Line2Pt *)&line2->to[0];
    case 1:
        ((Line2Pt *)&line2->to[0])->x += 4.0f;
        limit = *(float *)(object + 0x20) + 19.0f + 9.5f;
        if (((Line2Pt *)&line2->to[0])->x < limit) {
            return 0;
        }
        ((Line2Pt *)&line2->to[0])->x = limit;
        *(Line2PtA *)&line2->to[1] = *(Line2PtA *)(object + 0x20);
        *(Line2Pt *)&line2->from[1] = *(Line2Pt *)&line2->to[1];
        ((Line2Pt *)&line2->from[1])->x += 28.5f;
        ((Line2Pt *)&line2->to[1])->x += 28.5f;
        line2->counter = 2;
    case 2:
        if (line2->selectedIndex == 0) {
            char *c0 = object + 0xC;
            ((Line2Pt *)&line2->to[1])->y -= 8.0f;
            if (*(float *)(*(char **)(c0 + (line2->selectedIndex << 2)) + 0x24) < ((Line2Pt *)&line2->to[1])->y) {
                return 0;
            }
        } else {
            char *c1 = object + 0xC;
            ((Line2Pt *)&line2->to[1])->y += 8.0f;
            if (((Line2Pt *)&line2->to[1])->y < *(float *)(*(char **)(c1 + (line2->selectedIndex << 2)) + 0x24)) {
                return 0;
            }
        }
        {
            char *base = object + 0xC;
            char *sel = *(char **)(base + (line2->selectedIndex << 2));
            ((Line2Pt *)&line2->to[1])->y = *(float *)(sel + 0x24);
            *(Line2PtA *)&line2->to[2] = *(Line2PtA *)(sel + 0x20);
        }
        *(Line2Pt *)&line2->from[2] = *(Line2Pt *)&line2->to[2];
        ((Line2Pt *)&line2->from[2])->x -= 28.5f;
        ((Line2Pt *)&line2->to[2])->x -= 28.5f;
        line2->counter = 3;
    case 3: {
        char *list = object + 0xC;
        ((Line2Pt *)&line2->to[2])->x += 4.0f;
        child = *(char **)(list + (line2->selectedIndex << 2));
        if (((Line2Pt *)&line2->to[2])->x < *(float *)(child + 0x20)) {
            return 0;
        }
        ((Line2Pt *)&line2->to[2])->x = *(float *)(child + 0x20);
        return 1;
    }
    }
    return 0;
}

void EtherTreeLine2Main(void)
{
    EtherTreeLine2Data *line2 = EtherTreeLine2;
    unsigned char flags;
    int n;

    if (!(EtherTreeSystem->flags & 1))
        return;
    flags = line2->flags;
    if (!(flags & 1))
        return;

    switch (line2->mode) {
    case 0:
        break;
    case 1:
        n = line2->object->childCount;
        if (n != 1) {
            if (n < 2)
                goto open;
            if (n < 4)
                goto type1;
            goto open;
        }
        if (!subLine2_OpenType_0(line2))
            break;
    type1:
        if (!subLine2_OpenType_1(line2))
            break;
    open:
        line2->mode = 2;
        break;
    case 2:
        break;
    case 3:
        line2->mode = 0;
        line2->flags = flags & ~1;
        break;
    }
}

extern void endSpriteSet(unsigned char *data, int kind);
typedef struct LampView {
    unsigned char pad00[0x30];
    short x;
    short y;
    int z;
    unsigned char pad38[0x0e];
    unsigned short alpha;
    unsigned short speed;
    unsigned char pad4a[0x06];
    short sx;
    short sy;
    int depth;
    short w;
    short h;
    unsigned char pad5c[3];
    unsigned char a;
    unsigned short attr;
} LampView;

void sub2ObjectLampDraw(LampView *obj)
{
    EtherTreeSystemData *sys = EtherTreeSystem;
    unsigned char *data = (unsigned char *)&obj->sx;
    float *node = &sys->node.x;
    short v;
    float scale, scale2;
    short w;

    v = obj->alpha += obj->speed;
    if (v < 0) {
        obj->speed = -obj->speed;
        obj->alpha = 0;
    } else if (v > 0x80) {
        obj->alpha = 0x80;
        obj->speed = -obj->speed;
    }
    obj->attr = 0x1203;
    endSpriteSet(data, 0xFF);
    obj->w = obj->w * node[3];
    obj->h = obj->h * node[3];
    obj->a = obj->alpha;
    obj->depth = obj->z + 1;
    *(short *)data = obj->x + node[3] * 27.0f - 2.0f;
    obj->sy = obj->y + node[3] - 3.0f;
    endPrintExtFunc(0, 2, data);
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct PrintSpriteInfo EtherTreeSprite;

typedef struct PrintSpriteInfo {
    short x;
    short y;
    int depth;
    short width;
    short height;
    unsigned char color[4];
    short uv;
    unsigned char column;
    unsigned char row;
} EtherTreeSprite;

typedef struct EtherTreeRibbonPoint {
    short x;
    short y;
    unsigned int depth;
    unsigned char color[4];
} EtherTreeRibbonPoint;

typedef struct EtherTreeLineDrawMethods {
    void (*byChildCount[4])(EtherTreeLineData *line);
} EtherTreeLineDrawMethods;

typedef struct EtherTreeGeometrySegment {
    EtherTreePosition from;
    unsigned char fromColor[4];
    unsigned char unmodeled_14[12];
    EtherTreePosition to;
    unsigned char toColor[4];
    unsigned char unmodeled_34[12];
} EtherTreeGeometrySegment;

typedef struct EtherTreeLineGeometry {
    EtherTreeObjectData *object;
    unsigned char unmodeled_04[12];
    EtherTreeGeometrySegment segments[6];
} EtherTreeLineGeometry;

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", EtherTreeObjectDraw);

void subTreeLineDraw(EtherTreeLineData *line, const int *colorIndices)
{
    EtherTreeSystemData *system = EtherTreeSystem;
    EtherTreeCoordinates *center = &system->center;
    EtherTreeNodeTransform *node = &system->node;
    int segmentsByChildCount[4] = { 0, 1, 5, 6 };
    int segmentIndex = 0;

    if (segmentsByChildCount[line->object->childCount] > 0) {
        do {
            EtherTreeLineDrawSegment *segment = &line->segments[segmentIndex];

            segment->from.x = ((segment->from.x + node->x) + center->x) * node->scale;
            segment->to.x = ((segment->to.x + node->x) + center->x) * node->scale;
            segment->from.y = ((segment->from.y + node->y) + center->y) * node->scale;
            segment->to.y = ((segment->to.y + node->y) + center->y) * node->scale;
            segment->from.z = node->z - 2.0f;
            segment->to.z = node->z - 2.0f;
            EtherTreeLineColorGet(segment->fromColor, colorIndices[segmentIndex]);
            EtherTreeLineColorGet(segment->toColor, colorIndices[segmentIndex]);
            endPrintExtFunc(0, 0x10, &segment->from.x);

            segmentIndex++;
        } while (segmentIndex < segmentsByChildCount[line->object->childCount]);
    }
}

void subTreeLineDraw_type_0(void)
{
}

void subTreeLineDraw_type_1(EtherTreeLineData *line)
{
    int arg[4];
    EtherTreeObjectData *object = line->object;
    EtherTreeObjectData *child = object->children[0];

    ((EtherTreePosition *)&line->segments[0].from)->words[0] = object->position.words[0];
    ((EtherTreePosition *)&line->segments[0].from)->words[1] = object->position.words[1];
    ((EtherTreePosition *)&line->segments[0].to)->words[0] = child->position.words[0];
    ((EtherTreePosition *)&line->segments[0].to)->words[1] = child->position.words[1];
    if (child->flags & 1) {
        arg[0] = 2;
    } else {
        arg[0] = 1;
    }
    subTreeLineDraw(line, arg);
}

typedef union { struct { long long lo, hi; } q; float f[4]; } TLV;
typedef struct TLWork { EtherTreeObjectData *node; int pad[3]; TLV a0; int pad2[4]; TLV b0; } TLWork;

void subTreeLineDraw_type_2(TLWork *w)
{
  int arg[5];
  EtherTreeObjectData *n = w->node;
  int i;
  int new_var2;
  if (((unsigned char) ((&n->children[0])[0]->flags & 1)) || ((unsigned char) ((&n->children[0])[1]->flags & 1)))
  {
    arg[0] = 2;
  }
  else
  {
    arg[0] = 1;
  }
  for (i = 0; i < 2; i++)
  {
    if ((unsigned char) ((&n->children[0])[i]->flags & 1))
    {
      arg[(i * 2) + 2] = 2;
      arg[(i * 2) + 1] = 2;
    }
    else
    {
      arg[(i * 2) + 2] = 1;
      arg[(i * 2) + 1] = 1;
    }
  }

  ;
  *((TLV *) (&w->a0)) = (*((TLV *) (&w->b0)) = *((TLV *) (&n->position)));
  ((TLV *) (&w->b0))->f[0] += 28.5f;
  new_var2 = 1;
  for (i = 0; i < 2; i++)
  {
    EtherTreeObjectData *c = (&n->children[0])[i];
    TLV *v = (TLV *) (&w[(i * 2) + 2].b0);
    float x;
    v[-6] = (v[-4] = *((TLV *) (&w->b0)));
    x = v[-4].f[0];
    v[-4].f[1] = ((TLV *) (&c->position))->f[1];
    v[-2] = (v[0] = *((TLV *) (&c->position)));
    v[-2].f[0] = x;
  }

  subTreeLineDraw((EtherTreeLineData *)w, arg);
}

typedef union { long long d[2]; float f[4]; } TLVec;
typedef struct { EtherTreeObjectData *node; int pad[3]; TLVec a; int pad2[4]; TLVec b; } TLWork3;

void subTreeLineDraw_type_3(TLWork3 *w)
{
    int i, j;
    int arg[8];
    EtherTreeObjectData *n;
    EtherTreeObjectData *c;
    TLVec *v;
    long long t0, t1, t2, t3, t4, t5, t6, t7;

    arg[0] = 1;
    for (i = 0; i < 3; i++) {
        if ((unsigned char)(w->node->children[0]->flags & 1)) {
            arg[0] = 2;
            break;
        }
    }
    n = w->node;
    for (j = 0; j < 3; j++) {
        if ((unsigned char)(n->children[j]->flags & 1)) {
            arg[j * 2 + 2] = 2;
            arg[j * 2 + 1] = 2;
        } else {
            arg[j * 2 + 2] = 1;
            arg[j * 2 + 1] = 1;
        }
    }
    t0 = ((TLVec *)&n->position)->d[0];
    w->b.d[0] = t0;
    t1 = ((TLVec *)&n->position)->d[1];
    w->a.d[0] = t0;
    w->b.d[1] = t1;
    w->a.d[1] = t1;
    w->b.f[0] += 28.5f;
    n = w->node;
    for (i = 0; i < 2; i++) {
        EtherTreeObjectData *cc = n->children[i * 2];
        v = &w[i * 2 + 2].b;
        t4 = w->b.d[0];
        v[-4].d[0] = t4;
        t5 = w->b.d[1];
        v[-6].d[0] = t4;
        v[-4].d[1] = t5;
        v[-6].d[1] = t5;
        v[-4].f[1] = ((TLVec *)&cc->position)->f[1];
        t2 = ((TLVec *)&cc->position)->d[0];
        v[0].d[0] = t2;
        t3 = ((TLVec *)&cc->position)->d[1];
        v[-2].d[0] = t2;
        v[0].d[1] = t3;
        v[-2].d[1] = t3;
        v[-2].f[0] = v[-4].f[0];
    }
    c = n->children[1];
    t6 = ((TLVec *)&c->position)->d[0];
    w[5].b.d[0] = t6;
    t7 = ((TLVec *)&c->position)->d[1];
    w[5].a.d[0] = t6;
    w[5].b.d[1] = t7;
    w[5].a.d[1] = t7;
    w[5].a.f[0] = w->b.f[0];
    subTreeLineDraw((EtherTreeLineData *)w, arg);
}

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

typedef struct EtherTreeCursolSprite {
    short x;
    short y;
    unsigned int depth;
    short w;
    short h;
    unsigned char unmodeled_0c[0x04];
    short attr;
} EtherTreeCursolSprite;

void EtherTreeCursolDraw(void)
{
  EtherTreeSystemData *sys = EtherTreeSystem;
  EtherTreeCursolSprite *sprite = (EtherTreeCursolSprite *) (&sys->unmodeled_54[8]);
  float *center;
  EtherTreeObjectData *object;
  float *node;
  float scale;
  float depth;
  float *pos;
  float x;
  float y;
  long long new_var2;
  float *new_var;
  sprite->attr = 0x1204;
  object = sys->targetObject;
  center = &sys->center.x;
  endSpriteSet((unsigned char *) sprite, 0xFF);
  node = &sys->node.x;
  new_var = center;
  pos = &object->position.vector.x;
  new_var2 = 0;
  sprite->x = (pos[0] - 23.0f) + (node[new_var2] + new_var[new_var2]);
  sprite->y = (pos[1] - 28.0f) + (node[1] + center[1]);
  scale = node[3];
  depth = node[2] + 2.0f;
  sprite->w *= scale;
  sprite->h *= scale;
  sprite->x *= scale;
  sprite->y *= scale;
  sprite->depth = depth;
  endPrintExtFunc(0, 2, sprite);
}

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

static int RoboEtherCheck(int etherIndex)
{
    return (unsigned int) (etherIndex - 13) < 4U;
}

INCLUDE_ASM("asm/main/nonmatchings/ether_tree", EtherTreeMain);

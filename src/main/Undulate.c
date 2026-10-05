#include "common.h"
#include "Undulate.h"

extern unsigned char *CurrentColiHead;

INCLUDE_ASM("asm/main/nonmatchings/Undulate", UnduInit);

static UnduDataHeader *UnduDataGetHeaderSub(UnduDataHeader *headers,
                                            unsigned int count,
                                            unsigned int identifier)
{
    unsigned int index;
    UnduDataHeader *header;

    if (identifier & 0x8000) {
        identifier &= 0x7fff;
        if (identifier < count)
            return &headers[identifier];
    } else {
        header = headers;
        for (index = 0; index < count; index++, header++) {
            if (header->identifier == identifier)
                return header;
        }
    }
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/Undulate", UnduDataGetHeader);

void UnduParamInit(UnduQuery *query)
{
    /* UnduCheck consumes the Z count and scratchpad after initialization. */
    volatile UnduQuery *observableQuery = query;
    unsigned char crossCheckCount = 16;
    UnduQuery *scratchpad = (UnduQuery *)0x70000000;

    observableQuery->crossCheckCountZ = crossCheckCount;
    query->includedAttributes = 1;
    query->heightLimit = 1.0f;
    query->resultHeight = -1000.0f;
    observableQuery->scratchpad = scratchpad;
    query->queryFlags = 0;
    query->queryFilter = 0;
    query->attrMask = 0;
    query->secondaryVertexOffset = 0;
    do {
        query->crossCheckCountX = crossCheckCount;
        query->header = 0;
    } while (0);
    query->initializedState = 0;
    query->excludedAttributes = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/Undulate", UnduCheckSub);

extern float GameLoopState[];

static int UnduCheckSubHeightCheck(UnduWork *work, int subOffset, float height)
{
    unsigned short flags;
    unsigned int attr;
    int result;
    float low;
    float high;

    flags = work->wallAttrHeader->flags;
    if (work->wallAttributeTable != 0) {
        attr = work->wallAttributeTable[*(unsigned short *)subOffset].words[0];
    } else {
        attr = *(unsigned short *)subOffset;
    }
    if ((flags & 0x20) && ((attr & 0xe000) == 0x4000)) {
        height = *(float *)((char *)GameLoopState + 0x5c + ((attr & 0x1f00) >> 6));
    }
    result = 0;
    if (flags & 0x10) {
        low = work->heightCenter;
        high = work->heightHalf;
        if ((low - high) < height) {
            if (height < (low + high)) {
                result = ((low + high) < work->bestHeight) ? 2 : 1;
                goto done;
            }
        }
        if (flags & 0x800) {
            if (work->bestHeight == -1000.0f) {
                result = 1;
            } else if (height < low + high) {
                result = 1;
            } else if (height < work->bestHeight) {
                result = 1;
            }
        }
    } else if (flags & 0x800) {
        if (height <= (work->heightCenter + work->heightHalf)) {
            result = 1;
        }
    } else {
        if (work->bestHeight == -1000.0f) {
            result = 1;
        } else if (height < (work->heightCenter + work->heightHalf)) {
            result = 1;
        } else if (height < work->bestHeight) {
            result = 2;
        }
    }
done:
    if (result != 0) {
        if (result == 2 || work->bestHeight < height) {
            result = 2;
            work->bestHeight = height;
            work->bestSubOffset = subOffset;
        }
    }
    return result;
}

static float UnduCheckSub(UnduWork *work, int subOffset);
static int UnduCheckSubHeightCheck(UnduWork *work, int subOffset, float height);

static void UnduCheckSubCheckAll(UnduWork *work) {
    int subOffset;
    int entryOffset;
    int i;
    float height;

    subOffset = work->subListOffset;
    for (i = 0; i < work->subCount; i++) {
        height = UnduCheckSub(work, subOffset);
        entryOffset = subOffset;
        subOffset += 0x18;
        if (height != -1000.0f) {
            UnduCheckSubHeightCheck(work, entryOffset, height);
        }
    }
}

static int CheckWallAttr(UnduWork *work, const unsigned short *wallAttribute)
{
    unsigned short attributeIndex;
    unsigned long long attributes;
    int wallMode;
    unsigned int wallModeMask;
    UnduWallAttributes *attributeTable;
    UnduWallAttributes *tableEntry;
    UnduWallAttributes *selectedAttributes;

    attributeTable = work->wallAttributeTable;
    if (attributeTable != 0) {
        attributeIndex = *wallAttribute;
        selectedAttributes = &work->wallAttributes;
        tableEntry = &attributeTable[attributeIndex];
        selectedAttributes->words[0] = tableEntry->words[0];
        selectedAttributes->words[1] = tableEntry->words[1];
    } else {
        work->wallAttributes.bits = *wallAttribute;
    }
    attributes = work->wallAttributes.bits;
    if ((attributes & work->wallAttributeMask) == work->wallAttributeValue) {
        return 1;
    }
    if ((attributes & 7) != 0) {
        wallModeMask = 0;
        wallMode = work->wallAttrHeader->flags & 0x300;
        switch (wallMode) {
        case 0x100:
            wallModeMask = 1;
            break;
        case 0x200:
            wallModeMask = 2;
            break;
        case 0x300:
            wallModeMask = 4;
            break;
        default:
            break;
        }
        if ((attributes & wallModeMask) != 0) {
            return 1;
        }
    }
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/Undulate", UnduCheckSubCrossCheck);

INCLUDE_ASM("asm/main/nonmatchings/Undulate", UnduCheckResultCheck);

INCLUDE_ASM("asm/main/nonmatchings/Undulate", UnduCheck);

float UnduCheck(float *sample, void *output, UnduQuery *query);

float UnduGet2(int queryAddress, float x, float z)
{
    UnduQuery *query;
    UnduQuery *work;
    int type;
    int hasCollisionData;

    if (CurrentColiHead == 0)
        return 0.0f;
    if (queryAddress == 0) {
        UnduParamInit((UnduQuery *)0x70000000);
        queryAddress = 0x70000000;
    }
    query = (UnduQuery *)queryAddress;
    work = query->scratchpad;
    type = CurrentColiHead[0] & 0x7f;
    if (work == 0)
        work = (UnduQuery *)0x70000000;
    if (type < 3) {
        hasCollisionData = type != 0;
        if (hasCollisionData) {
            work->sample[0] = x;
            work->sample[2] = z;
            work->sample[1] = 0.0f;
            return UnduCheck(work->sample, 0, query);
        }
    }
    return query->resultHeight;
}

extern float UnduGet2(int flag, float x, float z);

float UnduGet(float x, float z)
{
    return UnduGet2(0, x, z);
}

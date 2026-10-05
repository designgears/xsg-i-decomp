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

static float UnduCheckSub(UnduWork *work, int subOffset)
{
    unsigned char *base;
    float *a, *b, *c, *p, *pos;
    float x, z, ax, az, bx, bz, cx, cz;
    unsigned short flags;
    float lo, hi, t, hc;
    int ok;
    float hh;
    float d1, d2, d3;
    float xaz, xbz, xcz, bxz, axz, cxz;

    pos = (float *)((char *)work + 0x80);
    base = *(unsigned char **)((char *)work + 0x58);
    a = (float *)(base + *(unsigned short *)(subOffset + 8));
    b = (float *)(base + *(unsigned short *)(subOffset + 10));
    x = pos[0];
    az = a[2];
    c = (float *)(base + *(unsigned short *)(subOffset + 12));
    ax = a[0];
    bz = b[2];
    bx = b[0];
    z = pos[2];
    cz = c[2];
    cx = c[0];
    xaz = x * az;
    xbz = x * bz;
    xcz = x * cz;
    bxz = bx * z;
    axz = ax * z;
    cxz = cx * z;
    d1 = (((xaz + ax * bz + bxz) - xbz) - axz) - bx * az;
    d2 = (((xbz + bx * cz + cxz) - xcz) - bxz) - cx * bz;
    d3 = (((xcz + cx * az + axz) - xaz) - cxz) - ax * cz;
    if (d1 <= 0.0f && d2 <= 0.0f && d3 <= 0.0f) {
        p = (float *)(*(unsigned char **)((char *)work + 0x5c) + *(unsigned short *)(subOffset + 14));
        flags = work->wallAttrHeader->flags;
        t = ((*(float *)(subOffset + 4) - p[0] * x) - p[2] * z) / p[1];
        if (flags & 0x400) {
            float by, cy, t2;
            ok = 0;
            lo = a[1]; hi = lo;
            by = b[1];
            if (by < lo) lo = by;
            cy = c[1];
            if (cy < lo) lo = cy;
            hh = work->heightHalf;
            lo = lo + hh;
            if (hi < by) hi = by;
            if (hi < cy) hi = cy;
            t2 = hi - hh;
            hc = pos[1];
            if (flags & 0x10) {
                if (t2 < hc) {
                    if (flags & 0x800) ok = 1;
                    else if (hc < lo) ok = 1;
                }
            } else if (flags & 0x800) {
                if (t2 < hc) ok = 1;
            } else {
                ok = 1;
            }
            if (!ok) return -1000.0f;
        }
        return t;
    }
    return -1000.0f;
}

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

static float UnduCheckResultCheck(UnduWork *work, UnduQuery *query, float *pos)
{
    unsigned int flags;
    int mode;
    UnduWallAttributes *table;

    flags = (unsigned short)query->attrMask & 0xEFFF;
    query->attrMask = flags;
    mode = flags & 7;
    query->secondaryVertexOffset = ((unsigned char *)work)[0xA2] & 3;
    query->resultHeight = work->bestHeight;
    if (work->bestSubOffset != 0) {
        table = work->wallAttributeTable;
        query->queryFilter = query->queryFlags;
        query->queryFlags = work->bestSubOffset;
        if (table != 0) {
            UnduWallAttributes *e = &table[*(unsigned short *)work->bestSubOffset];
            UnduWallAttributes *d = (UnduWallAttributes *)&query->initializedState;
            d->words[0] = e->words[0];
            d->words[1] = e->words[1];
        } else {
            query->initializedState = *(unsigned short *)work->bestSubOffset;
        }
    } else {
        query->queryFlags = 0;
        query->initializedState = 0;
        if (mode != 1) {
            goto end;
        }
    }
    if (*(int *)((char *)work + 0x90) != 0) {
        pos[0] = *(float *)((char *)work + 0x80);
        pos[1] = work->heightCenter;
        pos[2] = *(float *)((char *)work + 0x88);
    }
end:
    return work->bestHeight;
}

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

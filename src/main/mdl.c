#include "common.h"

#include "mdl.h"

typedef struct { int pad[12]; int name; int pad2[3]; } MdlGroup;
static inline void MDL_setVisible_inl(MdlHandle *mdl, int idx, int on)
{
    int bit = 1 << (idx & 0x1f);
    int i = idx >> 5;
    if (on) {
        mdl->visibleParts[i] |= bit;
    } else {
        mdl->visibleParts[i] &= ~bit;
    }
}

int MDL_setGroupVisible(MdlHandle *model, int name, int on)
{
    MdlResource *resource;
    int count;
    int i;
    int n;

    if (model->entry == 0) {
        return 0;
    }
    name = (((u32)name >> 24) | ((name << 8) & 0xFF0000)) | (((name >> 8) & 0xFF00) | (name << 24));
    resource = (MdlResource *)model->entry;
    count = resource->partCount;
    n = 0;
    for (i = 0; i < count; i++) {
        if (((MdlGroup *)model->parts)[i].name == name) {
            MDL_setVisible_inl(model, i, on);
            n++;
        }
    }
    return n;
}

typedef struct { int pad[14]; int name0; int name1; } MdlGroupN;
#define MDL_BSWAP(x) ((((u32)(x) >> 24) | (((x) << 8) & 0xFF0000)) | ((((x) >> 8) & 0xFF00) | ((x) << 24)))

int MDL_setNameVisible(MdlHandle *model, int *name, int on)
{
    MdlResource *resource;
    MdlGroupN *parts;
    int n0, n1, m0, m1;
    int count;
    int i;
    int n;

    resource = (MdlResource *)model->entry;
    if (resource == 0 || (parts = (MdlGroupN *)model->parts) == 0) {
        return 0;
    }
    n0 = name[0];
    n1 = name[1];
    i = 0;
    m0 = name[2];
    m1 = name[3];
    n0 = MDL_BSWAP(n0);
    n1 = MDL_BSWAP(n1);
    m0 = MDL_BSWAP(m0);
    m1 = MDL_BSWAP(m1);
    count = resource->partCount;
    n0 &= m0;
    n1 &= m1;
    n = 0;
    for (; i < count; i++) {
        if ((parts[i].name0 & m0) == n0 && (parts[i].name1 & m1) == n1) {
            n++;
            MDL_setVisible_inl(model, i, on);
        }
    }
    return n;
}

typedef struct { int pad; int visible[1]; } MdlVis;

void MDL_setVisible(MdlVis *mdl, int idx, int on)
{
    int bit = 1 << (idx & 0x1f);
    int i = idx >> 5;
    if (on) {
        mdl->visible[i] |= bit;
    } else {
        mdl->visible[i] &= ~bit;
    }
}

void MDL_create(MdlHandle *model, const MdlResource *resource)
{
    int wordIndex;

    if (resource != 0) {
        model->copiedData[1] = resource->copiedData[2];
        model->copiedData[2] = resource->copiedData[1];
        model->copiedData[0] = resource->copiedData[3];
    }
    model->entry = (int)resource;
    model->parts = 0;
    for (wordIndex = 3; wordIndex >= 0; wordIndex--) {
        model->visibleParts[wordIndex] = 0xffffffffu;
    }
}

void MDL_partsSetVisible(MdlHandle *model)
{
    MdlResource *resource = (MdlResource *)(u32)model->entry;
    u32 *visibleWords;
    int partCount;
    int bitIndexMask;
    int partIndex;

    if (resource == 0) {
        return;
    }

    partCount = resource->partCount;
    if (partCount > 128) {
        partCount = 128;
    }

    /* The low five index bits select a flag within each 32-part word. */
    visibleWords = model->visibleParts;
    bitIndexMask = 31;

    for (partIndex = 0; partIndex < partCount; partIndex++) {
        u32 bit = 1u << (partIndex & bitIndexMask);
        u32 visibilityMask = visibleWords[partIndex >> 5] & bit;

        nmlModelSetPartsVisible(resource, partIndex, visibilityMask);
    }
}

/*
 * Applies the model's texture and placement, marks its parts visible, then
 * tail-calls the model system to enter it for rendering.
 */
void MDL_draw(MdlHandle *model, const Vector4 *place)
{
    int entry = model->entry;

    nmlModelSetTexture(model->texture);
    nmlModelSetPlace(place);
    MDL_partsSetVisible(model);
    nmlModelEntry(entry);
}

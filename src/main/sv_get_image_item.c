#include "common.h"

#include "shared.h"

#include "main/xgl_packet.h"

typedef unsigned int Quadword __attribute__((mode(TI)));

typedef struct Matrix { float elements[16]; } Matrix;

extern int _draw3D;


extern int _nEffect2D;


extern int eft_5;


extern int charID_3;


extern int eftCate_4;


extern int _nowImage;


/*
 * One 0x24-byte entry of the working image list svImageListCreate clears
 * and svGetImageItem indexes; only its address is ever taken in this
 * allocation, so its fields stay unmodeled.
 */

typedef struct SvImageItem {
    unsigned char unmodeled_00[0x24];
} SvImageItem;

/*
 * The single working image list: a 0x1C-byte header (untouched by this
 * allocation) followed by 0x100 SvImageItem entries (0x1C + 0x100*0x24 =
 * 0x241C, svImageListCreate's own clear size).
 */

typedef struct SvImageList {
    unsigned char unmodeled_header[0x1C];
    SvImageItem items[0x100];
} SvImageList;

extern SvImageList _imageList;


/*
 * One 0x24-byte entry of an image mapper's item table: only the leading
 * dword svImageListDestroy clears is modeled.
 */

typedef struct SvImageMapperItem {
    int used; /* 0x00, cleared when non-zero */
    unsigned char unmodeled_04[0x24 - 4];
} SvImageMapperItem;

/*
 * The image-mapper table has a 0x1C-byte header followed by 0x100 entries
 * of 0x24 bytes. These entry fields are evidenced by the lookup routines:
 * the active word is at +0x08, the type byte at +0x0E, and the resource
 * name pointer at +0x20.
 */

typedef struct SvTypeListEntry {
    const void *data;
    unsigned char unmodeled_04[4];
    int used;
    short resourceKind;
    unsigned char type;
    unsigned char unmodeled_0F[3];
    unsigned short basePointer;
    unsigned short bitsPerPixel;
    unsigned short bufferWidthPixels;
    unsigned short width;
    unsigned short height;
    unsigned short textureWidthExponent;
    unsigned short textureHeightExponent;
    const char *name;
} SvTypeListEntry;

typedef struct SvTypeList {
    int total;
    int size;
    unsigned char unmodeled_08[4];
    int savedSize;
    short width;
    short height;
    short entryCount;
    unsigned char unmodeled_16[6];
    SvTypeListEntry entries[0x100];
} SvTypeList;

typedef struct SvReferenceImageSlot {
    unsigned int words[2];
} SvReferenceImageSlot;

/* _imageMapper's symbol-table size is 40 mapper records followed by these
 * forty two-word reference-image slots. */

typedef struct SvImageMapperStorage {
    SvTypeList typeLists[40];
    SvReferenceImageSlot referenceImages[40];
} SvImageMapperStorage;

/* Forty 0x241C-byte mapper records followed by forty reference-image slots. */

extern SvImageMapperStorage _imageMapper;


extern Matrix _defMatLcMod;


extern Matrix _defMatLn;


int strcmp(const char *, const char *);

/*
 * Only `data`, `destAddress` and `format` are evidenced (svLoadClut's three
 * field reads); the bytes between `data` and `destAddress` are an explicit
 * unmodeled span rather than an invented field.
 */

typedef struct ClutUploadRequest {
    void *data;                     /* 0x00, CLUT pixel data source address */
    unsigned char unmodeled_04[0x0E];
    unsigned short destAddress;     /* 0x12, GS local CLUT destination */
    unsigned short format;          /* 0x14, 8 => 256-entry CLUT, else 16-entry */
} ClutUploadRequest;

/*
 * Parameter roles as the SDK body (main 0x0020b208) packs them: GS
 * BITBLTBUF DBP/DPSM/DBW from basePointer/pixelFormat/bufferWidth, TRXPOS
 * DSAX/DSAY from x/y, TRXREG RRW/RRH from width/height, then `qwc`
 * quadwords of `image` sent by reference.
 */

extern void sceVif1PkRefLoadImage(XglPacket *packet,
                                  unsigned short basePointer,
                                  unsigned char pixelFormat,
                                  unsigned short bufferWidth,
                                  const void *image, unsigned int qwc,
                                  unsigned int x, unsigned int y,
                                  unsigned int width, unsigned int height);

int svLoadImageList(SvTypeList *typeList);

extern int svImageListAlloc(int typeListAddr);


/*
 * One svAddScript2 sub-record, at +0x1C of a 0x24-byte slot of the type
 * list's item array. Only the fields this function itself writes are
 * modeled, at their byte offset from this sub-record's own base: `codePtr`
 * (+0x00, the script's own +0xC0 address), `active` (+0x08, always 1),
 * `kind` (+0x0C, always 5, matching this function's own "script" name),
 * `id` (+0x0E, the script's own +0xA halfword), two cleared halfwords
 * (+0x18/+0x1A) and `dataPtr` (+0x20, the script's own +0x14 address).
 * +0x20 (slot +0x3C) lands past the slot's own 0x24-byte stride, into the
 * next slot's leading bytes -- evidenced by the original bytes, not
 * treated as a bug.
 */

typedef struct SvScriptSlot {
    void *codePtr;                  /* 0x00 */
    unsigned char unmodeled_04[4];
    int active;                      /* 0x08 */
    short kind;                       /* 0x0C */
    short id;                          /* 0x0E */
    unsigned char unmodeled_10[8];
    short clearedA;                     /* 0x18 */
    short clearedB;                      /* 0x1A */
    unsigned char unmodeled_1C[4];
    void *dataPtr;                        /* 0x20 */
} SvScriptSlot;

int svAnalyzeChunk(SvTypeList *typeList, void *chunk);

extern SvTypeList *svGetTypeList(int type);


void svImageListDestroy(SvTypeList *typeList);

/*
 * SGsSendPacket's own address precedes SGsPacket's definition (below, before
 * SGsInitGifPacket, outside this allocation): the same four fields, at the
 * same evidenced offsets that definition documents, restated here under a
 * local tag so this allocation adds text only next to its own function.
 */

typedef struct SGsPacketReset {
    int count;               /* 0x0 */
    int tagIndex;             /* 0x4 */
    int loopCount;             /* 0x8 */
    void *base;               /* 0xC, starts at the scratchpad (0x70000000) */
} SGsPacketReset;

extern void sceVif1PkCnt(XglPacket *packet, int count);

extern void sceVif1PkAddCode(XglPacket *packet, unsigned int code);

extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);

extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data,
                                    int count);

extern void sceVif1PkCloseDirectHLCode(XglPacket *packet);

extern void sceVif1PkTerminate(XglPacket *packet);

#define SEF_CLIP_VIEW_VOLUME_OFFSET 0x4F0

/*
 * sefDrawSchedulerEffect (still INCLUDE_ASM here) calls sefClipViewVolume and
 * branches on the returned v0, and sefClipViewVolume itself ends in a tail
 * jump to sefClipViewVolumeA (no jal, so its result is whatever
 * sefClipViewVolumeA returns): both are declared non-void here.
 */

extern int sefClipViewVolumeA(void *scheduler);


extern void sefInitClipViewVolume(void *view);


extern void sefDrawSchedulerEffect(int schedulerIndex);


extern void SGsSetZTestEnv(int enable);

extern char *srsAnalyzeEftNo(int effectId, int *charId,
                            int *effectCategory);

/*
 * One 0xab0-byte record of the scheduler table sefGetScheduler returns
 * (src/main/sef.h, "The original scheduler table..."). Only the three
 * fields this function reads are named; sefIsDeadSchduler (sef.c) reads the
 * same in-use word raw through its own TU's SchedulerState, which this TU
 * cannot include (TU-local elsewhere), so this is a separate, narrower view
 * of the same bytes.
 */

typedef struct SchedulerRecord {
    unsigned char unmodeled_000[0x6b0];
    int inUse;                            /* +0x6b0 */
    unsigned char unmodeled_6b4[0xa78 - 0x6b4];
    short effectId;                       /* +0xa78 */
    unsigned char unmodeled_a7a[0xa8c - 0xa7a];
    int flags;                            /* +0xa8c */
    unsigned char unmodeled_a90[0xab0 - 0xa90];
} SchedulerRecord;

/* sef.c (main/tu211) returns unsigned char *; this prototype
 * preserves that return type across the call. */

extern unsigned char *sefGetScheduler(void);

#define SCHEDULER_COUNT 0x80

/* Draws the alters whose group (+0x0E) equals layer. */

void sdvDrawAlters(int layer);

extern int _scMslCate;

void MEfObjExec2nd(void);

extern void svDrawSchedulerParticle(void);


/*
 * The SGs* family (SGsInitGifPacket, SGsOpenGifPacket, SGsCloseGifPacket,
 * SGsAddReg, SGsAddGifXYZ2, SGsSendPacket and others in this TU) shares one
 * accumulator: a growing array of 16-byte GIF records at `base`, `count`
 * records long. SGsOpenGifPacket/SGsCloseGifPacket/SGsAddReg (still
 * INCLUDE_ASM here, read for this evidence per docs/naming.md) show that
 * `tagIndex` is the record index SGsOpenGifPacket saves so
 * SGsCloseGifPacket can patch the GIFtag it opened, and `loopCount` is
 * OR-ed, unshifted, into that tag's low (NLOOP) bits by SGsCloseGifPacket;
 * both functions here leave it at 1.
 */

/*
 * One 128-bit GIF packet record. The SGs* writers fill it as a whole
 * quadword (sq), as two doublewords (sd at 0x0/0x8: a register value and
 * its A+D address) or as four words (sw, SGsAddGifXYZ2).
 */

typedef union SGsGifRecord {
    Quadword quad;
    unsigned long long dword[2];
    int word[4];
    unsigned short halfword[8];
    float floatValue[4];
} SGsGifRecord;

typedef struct SGsPacket {
    int count;              /* 0x0 */
    int tagIndex;           /* 0x4 */
    int loopCount;          /* 0x8 */
    SGsGifRecord *base;     /* 0xC, starts at the scratchpad (0x70000000) */
} SGsPacket;

typedef struct SvImageParameter {
    unsigned long long texture;
    float width;
    float height;
} SvImageParameter;

extern const char D_004DBA50[];

extern const char D_004DBA58[];

/*
 * GS XYZF2 vertex: X/Y are (value*16 + screen offset), Z and F are the
 * raw depth and fog values -- the PS2 SDK's own XYZ2/XYZF2 naming.
 */

#include "sv_get_image_item.h"

void *svGetImageItem(int index) {
    return &_imageList.items[index];
}

void svImageListCreate(void) {
    memset(&_imageList, 0, sizeof(SvImageList));
}

void svImageListDestroy(SvTypeList *typeList)
{
  SvTypeList *header = typeList;
  SvImageMapperItem *item = (SvImageMapperItem *) (((unsigned char *) typeList) + 0x24);
  int count = 0xFF;

  do
  {
    if (item->used != 0)
    {
      item->used = 0;
    }
    count -= 1;
    item += 1;
  }
  while (count >= 0);

  header->savedSize = header->size;
  header->total = 0;
  header->width = 0;
  header->height = 0;
  header->entryCount = 0;
}

static int svImageListAlloc(int typeListAddr)
{
    SvTypeList *typeList;
    int index;

    typeList = (SvTypeList *)typeListAddr;
    for (index = 0; index < 0x100; index++) {
        if (typeList->entries[index].used == 0) {
            return index;
        }
    }
    return -1;
}

static SvTypeList * svGetTypeList(int type) {
    return &_imageMapper.typeLists[type];
}

static void *svGetImageListItemSub(int type, unsigned int flags)
{
    SvTypeList *typeList;
    SvTypeList *storage;
    SvTypeListEntry *entry;
    int index;

    typeList = svGetTypeList(type);
    storage = typeList;
    entry = storage->entries;
    index = 0;
    while (index < 0x100) {
        if (entry->used != 0 && entry->type == (unsigned char)flags) {
            return entry;
        }
        entry++;
        index++;
    }
    return 0;
}

void *svGetImageListItem(int type, unsigned int flags)
{
    int typeIndex;

    if ((flags & 0x100) != 0) {
        typeIndex = 0;
    } else if ((flags & 0x200) != 0) {
        typeIndex = type;
    } else if ((flags & 0x400) != 0) {
        typeIndex = type;
    } else if ((flags & 0x800) != 0) {
        typeIndex = 15;
    } else if ((flags & 0x1000) != 0) {
        typeIndex = type;
    } else {
        typeIndex = 14;
    }
    return svGetImageListItemSub(typeIndex, flags);
}

static SvTypeListEntry *svGetResFromName(const char *name)
{
    SvTypeList *typeList;
    SvTypeList *storage;
    SvTypeListEntry *entry;
    int index;

    typeList = svGetTypeList(0);
    storage = typeList;
    entry = storage->entries;
    index = 0;
    while (index < 0x100) {
        if (entry->used != 0 && strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry++;
        index++;
    }
    return 0;
}

SvImageParameter *svGetPrmFromName(const char *baseName,
                                   SvImageParameter *parameter)
{
    char resourceName[128];
    SvTypeListEntry *bitmap;
    SvTypeListEntry *clut;

    sprintf(resourceName, D_004DBA50, baseName);
    bitmap = svGetResFromName(resourceName);
    sprintf(resourceName, D_004DBA58, baseName);
    clut = svGetResFromName(resourceName);

    if (bitmap != 0) {
        if (clut != 0) {
            unsigned long long baseBits;
            unsigned long long dimensionBits;
            unsigned long long formatBits;
            unsigned long long clutBits;
            int pixelFormat;
            unsigned short clutBasePointer;
            unsigned short bitsPerPixel;
            float scaledWidth;

            clutBasePointer = clut->basePointer;
            baseBits = (unsigned long long)bitmap->basePointer |
                       0x2000000000000000ULL;
            dimensionBits =
                ((unsigned long long)bitmap->textureWidthExponent << 26) |
                0x400000000ULL;
            pixelFormat = 20;
            bitsPerPixel = bitmap->bitsPerPixel;
            if (bitsPerPixel == 8) {
                pixelFormat = 19;
            }
            formatBits =
                ((unsigned long long)pixelFormat << 20) |
                ((unsigned long long)(bitmap->bufferWidthPixels >> 6) << 14);
            clutBits =
                ((unsigned long long)bitmap->textureHeightExponent << 30) |
                ((unsigned long long)clutBasePointer << 37);
            scaledWidth = bitmap->width * 0.01f;
            parameter->height = bitmap->height * 0.01f;
            parameter->width = scaledWidth;
            parameter->texture = (baseBits | formatBits) |
                                 (dimensionBits | clutBits);
            return parameter;
        }
    }

    memset(parameter, 0, 16);
    return 0;
}

void svLoadTexture(SvTypeListEntry *image)
{
    XglPacket *packet;
    unsigned short basePointer;
    unsigned short height;
    unsigned short width;
    unsigned short bitsPerPixel;

    basePointer = image->basePointer;
    height = image->height;
    width = image->width;
    packet = xglPacketGetCurrent();
    bitsPerPixel = image->bitsPerPixel;

    if (bitsPerPixel == 8) {
        unsigned short bufferWidth;
        int qwc;

        bufferWidth = image->bufferWidthPixels;
        qwc = bufferWidth * height;
        if (qwc < 0) {
            qwc += 15;
        }
        sceVif1PkRefLoadImage(packet, basePointer, 19,
                              bufferWidth >> 6, image->data,
                              (unsigned int)(qwc >> 4), 0, 0, width, height);
    } else {
        unsigned short bufferWidth;
        int qwc;

        bufferWidth = image->bufferWidthPixels;
        qwc = bufferWidth * height;
        if (qwc < 0) {
            qwc += 15;
        }
        sceVif1PkRefLoadImage(packet, basePointer, 20,
                              bufferWidth >> 6, image->data,
                              (unsigned int)(qwc >> 4), 0, 0, width, height);
    }
}

void svLoadClut(ClutUploadRequest *clut)
{
    XglPacket *packet = xglPacketGetCurrent();
    unsigned short dest = clut->destAddress;

    if (clut->format == 8) {
        sceVif1PkRefLoadImage(packet, dest, 0, 1, clut->data, 0x40, 0, 0,
                              0x10, 0x10);
    } else {
        sceVif1PkRefLoadImage(packet, dest, 0, 1, clut->data, 4, 0, 0, 8, 2);
    }
}

static inline int svLoadImageListCount(SvTypeList *typeList)
{
    int i;
    int cnt = 0;

    for (i = 0; i < 0x100; i++) {
        if (typeList->entries[i].used != 0) {
            if (typeList->entries[i].resourceKind == 4) {
                cnt++;
                svLoadTexture(&typeList->entries[i]);
            } else if (typeList->entries[i].resourceKind == 3) {
                svLoadClut((ClutUploadRequest *)&typeList->entries[i]);
            }
        }
    }
    return cnt;
}

int svLoadImageList(SvTypeList *typeList)
{
    return svLoadImageListCount(typeList);
}

s16 svLoadMapperList(void)
{
    s16 result;
    int type;
    SvTypeList *typeLists = _imageMapper.typeLists;

    for (type = 0; type < 40; type++) {
        /* Preserve the original loop's separate entry-count pointer. */
        const s16 *entryCount = &typeLists[type].entryCount;
        result = *entryCount;
        if (result > 0) {
            result = ((s16 (*)(SvTypeList *))svLoadImageList)(&_imageMapper.typeLists[type]);
        }
    }
    return result;
}

static int svGetSizeBit(unsigned int value)
{
    unsigned int mask;
    int bitIndex;

    mask = 2;
    bitIndex = 1;
    while (bitIndex < 32) {
        if ((value & mask) != 0) {
            return bitIndex;
        }
        mask <<= 1;
        bitIndex++;
    }
    return 0;
}

typedef struct SvImageSlot {
    void *codePtr;          /* 0x00 */
    unsigned char unmodeled_04[4];
    int active;             /* 0x08 */
    short kind;             /* 0x0C */
    short id;               /* 0x0E */
    unsigned char unmodeled_10[2];
    short vramAddr;         /* 0x12 */
    short format;           /* 0x14 */
    short height2;          /* 0x16 */
    unsigned short width;   /* 0x18 */
    unsigned short height;  /* 0x1A */
    short widthBit;         /* 0x1C */
    short heightBit;        /* 0x1E */
    void *dataPtr;          /* 0x20 */
} SvImageSlot;
typedef struct SvImageHeader {
    unsigned char unmodeled_00[4];
    unsigned int size;      /* 0x04 */
    unsigned char unmodeled_08[2];
    unsigned short id;      /* 0x0A */
    unsigned short width;   /* 0x0C */
    unsigned short height;  /* 0x0E */
    short format;           /* 0x10 */
} SvImageHeader;

static int svAddImage(int typeListAddr, SvImageHeader *image)
{
  int index;
  int ret;
  unsigned short width;
  unsigned int height;
  unsigned int size;
  unsigned int addr;
  void *slot;
  SvImageSlot *sub;
  index = svImageListAlloc(typeListAddr);
  ret = -1;
  slot = (void *) (typeListAddr + (index * 0x24));
  if (index >= 0)
  {
    sub = (SvImageSlot *) (((unsigned char *) slot) + 0x1C);
    sub->kind = 4;
    sub->codePtr = ((unsigned char *) image) + 0x40;
    sub->id = image->id;
    sub->active = 1;
    sub->width = image->width;
    width = sub->width;
    sub->height = image->height;
    sub->format = image->format;
    sub->widthBit = svGetSizeBit(width);
    height = sub->height;
    sub->heightBit = svGetSizeBit(height);
    sub->dataPtr = ((unsigned char *) image) + 0x14;
    size = image->size;
    if (image->format == 4)
    {
      sub->height2 = ((short) image->width) >> 1;
      if (((int) (height * 2)) < ((int) width))
      {
        size *= ((int) width) / ((int) (height * 2));
      }
    }
    else
      if (width < height)
    {
      sub->height2 = (((short) image->width) * ((int) height)) / ((int) width);
    }
    else
    {
      sub->height2 = image->width;
    }
    addr = *((unsigned int *) (typeListAddr + 0xC));
    sub->vramAddr = addr >> 6;
    *((unsigned int *) (typeListAddr + 0xC)) = addr + (size >> 2);
    return index;
  }
  return ret;
}

typedef struct SvResourceChunk {
    unsigned int byteSize;
    unsigned char unmodeled_04[4];
    short kind;
    unsigned char unmodeled_0A[54];
} SvResourceChunk;

typedef struct SvResourceChunkFile {
    SvResourceChunk *firstChunk;
} SvResourceChunkFile;

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", svAddClut);

static int svAddModel(int typeListAddr, void *script)
{
  int index;
  void *codePtr;
  void *dataPtr;
  void *slot;
  SvScriptSlot *sub;

  index = svImageListAlloc(typeListAddr);
  codePtr = (unsigned char *) script + 0x40;
  dataPtr = (unsigned char *) script + 0x14;
  slot = (void *) (typeListAddr + (index * 0x24));
  if (index < 0)
  {
    return -1;
  }
  sub = (SvScriptSlot *) ((unsigned char *) slot + 0x1C);
  sub->kind = 6;
  sub->dataPtr = dataPtr;
  sub->codePtr = codePtr;
  sub->id = (short) (*((u16 *) (((unsigned char *) script) + 0xA)) | 0x2000);
  sub->active = 1;
  sub->clearedB = 0;
  sub->clearedA = 0;
  return index;
}

static int svAddScript(int typeListAddr, void *script)
{
  int index;
  void *codePtr;
  void *dataPtr;
  void *slot;
  SvScriptSlot *sub;

  index = svImageListAlloc(typeListAddr);
  codePtr = (unsigned char *) script + 0x40;
  dataPtr = (unsigned char *) script + 0x14;
  slot = (void *) (typeListAddr + (index * 0x24));
  if (index < 0)
  {
    return -1;
  }
  sub = (SvScriptSlot *) ((unsigned char *) slot + 0x1C);
  sub->kind = 5;
  sub->dataPtr = dataPtr;
  sub->codePtr = codePtr;
  sub->id = (short) *((u16 *) (((unsigned char *) script) + 0xA));
  sub->active = 1;
  sub->clearedB = 0;
  sub->clearedA = 0;
  return index;
}

static int svAddScript2(int typeListAddr, void *script)
{
  int index;
  void *codePtr;
  void *dataPtr;
  void *slot;
  SvScriptSlot *sub;

  index = svImageListAlloc(typeListAddr);
  codePtr = (unsigned char *) script + 0xC0;
  dataPtr = (unsigned char *) script + 0x14;
  slot = (void *) (typeListAddr + (index * 0x24));
  if (index < 0)
  {
    return -1;
  }
  sub = (SvScriptSlot *) ((unsigned char *) slot + 0x1C);
  sub->kind = 5;
  sub->dataPtr = dataPtr;
  sub->codePtr = codePtr;
  sub->id = (short) *((u16 *) (((unsigned char *) script) + 0xA));
  sub->active = 1;
  sub->clearedB = 0;
  sub->clearedA = 0;
  return index;
}

int svAddClut(int typeListAddr, SvImageHeader *image);

static int svAnalyzeChunk(SvTypeList *typeList, void *chunk)
{
    unsigned char *p;

    p = *(unsigned char **) chunk;
    while (*(short *) (p + 8) != 0) {
        switch (*(short *) (p + 8)) {
        case 1:
        case 2:
            p += 0x40;
            break;
        case 4:
            svAddImage((int) typeList, (SvImageHeader *) p);
            p += *(int *) p;
            break;
        case 3:
            svAddClut((int) typeList, (SvImageHeader *) p);
            p += *(int *) p;
            break;
        case 7:
            svAddImage((int) typeList, (SvImageHeader *) p);
            p += *(int *) p;
            break;
        case 6:
        case 9:
            svAddModel((int) typeList, p);
            p += *(int *) p;
            break;
        case 5:
            svAddScript((int) typeList, p);
            p += *(int *) p;
            break;
        case 10:
            svAddScript2((int) typeList, p);
            p += *(int *) p;
            break;
        default:
            goto out;
        }
    }
out:
    ;
}

extern int top_1[];
extern int siz_0[];

void svInitImageMapper(void) {
    unsigned char *p = (unsigned char *)&_imageMapper;
    int i;
    memset(p, 0, 0x5A5A0);
    p += 0x10;
    for (i = 0; i < 0x28; i++) {
        int t = top_1[i];
        *(int *)(p - 0xC) = t;
        *(int *)(p - 4) = t;
        *(int *)(p - 8) = siz_0[i];
        *(short *)(p + 4) = 0;
        *(short *)p = 0;
        *(short *)(p + 2) = 0;
        p += 0x241C;
    }
}

static void svInitRefImage(void)
{
    SvImageMapperStorage *storage;
    int index;

    storage = &_imageMapper;
    for (index = 0; index < 40; index++) {
        storage->referenceImages[index].words[0] = 0;
        storage->referenceImages[index].words[1] = 0;
    }
}

void svAddImageMapper(int type, int width, void *chunk, int height) {
    SvTypeList *typeList;
    SvTypeList *header;

    typeList = svGetTypeList(type);
    header = typeList;
    if (header->entryCount != 0) {
        svImageListDestroy(typeList);
    }
    header->height = (short) height;
    header->width = (short) width;
    header->entryCount = (short) ((u16) header->entryCount + 1);
    svAnalyzeChunk(typeList, chunk);
}

void svDeleteImageMapper(int type)
{
    SvTypeList *typeList = svGetTypeList(type);

    if (typeList->entryCount != 0) {
        svImageListDestroy(typeList);
    }
}

void svDeleteImageMapperID(int id)
{
    int index;

    for (index = 2; index < 14; index++) {
        if (_imageMapper.typeLists[index].width == id) {
            svImageListDestroy(&_imageMapper.typeLists[index]);
            return;
        }
    }
}

void svDeleteImageMapperData(int data)
{
    int index;

    for (index = 2; index < 14; index++) {
        if (_imageMapper.typeLists[index].height == data) {
            svImageListDestroy(&_imageMapper.typeLists[index]);
            return;
        }
    }
}

typedef struct SrsMemResSlots {
    void *data[40];     /* +0x000 */
    short pending[40];  /* +0x0A0 */
    short no[40];       /* +0x0F0 */
} SrsMemResSlots;
extern unsigned char _srsMemRes[];
extern int srsFileLoadCf(int arg0, int effectId);
extern int sefIsFinishEffect2(int effectId);
extern int sefGetLoadQue(void);
extern void sefSetLoadQue(int effectId);
extern void *smAlloc(unsigned int size);
extern int srsLoadEffectData(void *buf, int effectId);
#define MEMRES ((SrsMemResSlots *) _srsMemRes)

int svFileLoadScript(int arg0, int effectId)
{
    unsigned char charId;
    int cate;
    SrsMemResSlots *res;

    srsAnalyzeEftNo(effectId, &charId, &cate);
    if (cate == 16) {
        return srsFileLoadCf(arg0, effectId);
    } else if (cate == 14) {
        if (MEMRES->data[14] != 0) {
            if (effectId == MEMRES->no[14]) {
                return 14;
            }
            if (sefIsFinishEffect2(MEMRES->no[14]) == 0) {
                sefGetLoadQue();
                sefSetLoadQue(effectId);
                return -1;
            }
            svDeleteImageMapper(14);
        }
        sefSetLoadQue(0);
        res = MEMRES;
        if (res->data[14] == 0) {
            res->data[14] = smAlloc(0x40000);
            if (res->data[14] == 0) {
                return -1;
            }
        }
        res->no[14] = effectId;
        res->pending[14] = 1;
        if (srsLoadEffectData(res->data[14], effectId) < 0) {
            res->pending[14] = 0;
            res->no[14] = 0;
            return -1;
        }
        return 14;
    }
    return -1;
}

extern int sefSearchMapperIndex(int effect_no);
extern char *srsGetEsdData(int index);
extern int strncmp(const char *, const char *, unsigned int);
extern unsigned int strlen(const char *);

int **svGetScript(int eftNo)
{
    char *name;
    int index;
    SvTypeList *typeList;
    unsigned char *base;
    char *rec;
    unsigned char *namep;
    unsigned int off;
    int i;
    int kind5;
    int none;

    name = srsGetEsdData(eftNo);
    if (name == 0) {
        return 0;
    }
    index = sefSearchMapperIndex(eftNo);
    typeList = &_imageMapper.typeLists[index];
    if (index == 0xE) {
        if (typeList->entries[0].used != 0 && typeList->entries[0].resourceKind != 0) {
            return (int **)&typeList->entries[0];
        }
    }
    i = 0;
    base = (unsigned char *)typeList + 0x14;
    kind5 = 5;
    none = -1;
    namep = (unsigned char *)&typeList->entries[0].name;
    off = 0;
    for (; i < 0x100; i++) {
        rec = (char *)(off + (unsigned int)base);
        if (*(int *)(rec + 0x10) != 0 && *(short *)(namep - 0x14) == kind5 &&
            *(int *)namep != none) {
            if (strncmp(name, *(char **)namep, strlen(name)) == 0) {
                return (int **)((unsigned char *)typeList + off + 0x1C);
            }
        }
        namep += 0x24;
        off += 0x24;
    }
    return 0;
}

void SGsSendPacket(SGsPacketReset *packet)
{
    XglPacket *vifPacket;

    if (packet->count != 0) {
        vifPacket = xglPacketGetCurrent();
        sceVif1PkCnt(vifPacket, 0);
        sceVif1PkAddCode(vifPacket, 0x11000000);
        sceVif1PkOpenDirectHLCode(vifPacket, 0);
        sceVif1PkAddDirectDataN(vifPacket, packet->base, packet->count);
        sceVif1PkCloseDirectHLCode(vifPacket);
        sceVif1PkTerminate(vifPacket);
        packet->count = 0;
        packet->tagIndex = 0;
        packet->loopCount = 1;
        packet->base = (void *)0x70000000;
    }
}

extern SGsPacket _packet;
static inline void SGsInitGifPacket_i(SGsPacket *packet)
{
    packet->tagIndex = 0;
    packet->loopCount = 1;
    packet->base = (SGsGifRecord *)0x70000000;
    packet->count = 0;
}
static inline void SGsAddReg_i(SGsPacket *packet, unsigned long long reg, unsigned long long value)
{
    packet->base[packet->count].dword[0] = value;
    packet->base[packet->count].dword[1] = reg;
    packet->count++;
}

void SGsTexFlush(void)
{
    SGsInitGifPacket_i(&_packet);
    SGsAddReg_i(&_packet, 0xE, 0x1000000000008001ULL);
    SGsAddReg_i(&_packet, 0x3F, 1);
    SGsSendPacket((SGsPacketReset *)&_packet);
}

extern void *xglStudioGetLight2(void);
extern void xglLightCalcMatrix(void *lightSet);
typedef struct SGsLightBackup {
    unsigned long long w[0x1e];
} SGsLightBackup;
extern SGsLightBackup _defLight;
extern Matrix _defMatLc;
static inline void SGsCopyMat4(void *dst, void *src)
{
    __asm__ __volatile__(
        "lq $8, 0(%1)\n\t"
        "lq $9, 16(%1)\n\t"
        "lq $10, 32(%1)\n\t"
        "lq $11, 48(%1)\n\t"
        "sq $8, 0(%0)\n\t"
        "sq $9, 16(%0)\n\t"
        "sq $10, 32(%0)\n\t"
        "sq $11, 48(%0)"
        :
        : "r"(dst), "r"(src)
        : "$8", "$9", "$10", "$11", "memory");
}

void SGsInitEnv(void)
{
    void *lightSet;
    int *row;
    int *p;
    int i;
    int j;

    SGsInitGifPacket_i(&_packet);
    SGsAddReg_i(&_packet, 0xE, 0x1000000000008001ULL);
    SGsAddReg_i(&_packet, 0x3F, 1);
    SGsAddReg_i(&_packet, 0xE, 0x1000000000008004ULL);
    SGsAddReg_i(&_packet, 0x47, 0x5000D);
    SGsAddReg_i(&_packet, 0x14, 0x60);
    SGsAddReg_i(&_packet, 8, 5);
    SGsAddReg_i(&_packet, 0x4E, 0x131000000);
    SGsSendPacket((SGsPacketReset *)&_packet);

    lightSet = xglStudioGetLight2();
    _defLight = *(SGsLightBackup *)lightSet;
    xglLightCalcMatrix(lightSet);

    SGsCopyMat4(&_defMatLcMod, (unsigned char *)lightSet + 0xB0);
    SGsCopyMat4(&_defMatLc, (unsigned char *)lightSet + 0xB0);
    SGsCopyMat4(&_defMatLn, (unsigned char *)lightSet + 0x70);

    row = (int *)&_defMatLc;
    i = 0;
    do {
        j = 2;
        p = row + 2;
        do {
            j--;
            *p = 0;
            p--;
        } while (j >= 0);
        i++;
        row += 4;
    } while (i < 3);
}

void SGsSetZTestEnv(int enable)
{
    SGsInitGifPacket_i(&_packet);
    SGsAddReg_i(&_packet, 0xE, 0x1000000000008001ULL);
    if (enable) {
        SGsAddReg_i(&_packet, 0x47, 0x5000D);
    } else {
        SGsAddReg_i(&_packet, 0x47, 0x2000D);
    }
    SGsSendPacket((SGsPacketReset *)&_packet);
}

void SGsRestoreEnv(void)
{
    SGsInitGifPacket_i(&_packet);
    SGsAddReg_i(&_packet, 0xE, 0x1000000000008002ULL);
    SGsAddReg_i(&_packet, 0x4E, 0x31000000);
    SGsAddReg_i(&_packet, 0x14, 0);
    SGsSendPacket((SGsPacketReset *)&_packet);
    nmlModelSetLight(_defMatLcMod, _defMatLn);
}

static void sefInitClipViewVolume(void *view) {
    __asm__ __volatile__("lqc2 vf12, 0(%0)\n\tlqc2 vf13, 16(%0)\n\tlqc2 vf14, 32(%0)\n\tlqc2 vf15, 48(%0)\n\tlqc2 vf16, 64(%0)\n\tlqc2 vf17, 80(%0)\n\tlqc2 vf18, 96(%0)\n\tlqc2 vf19, 112(%0)\n\tnop" :  : "r"(view) : "memory");
}

/*
 * Handwritten function (original is assembly): the same frustum test as
 * face_point's _ModelCalcClip (vf16..vf19 / vf12..vf15 hold the two clip
 * matrices) on the vector at scheduler, with every delay-slot/hazard nop
 * explicit under .set noreorder as in the original. Returns 0 if inside,
 * 1 if clipped.
 */
static int sefClipViewVolumeA(void *scheduler)
{
    register void *arg __asm__("$4") = scheduler;
    register int result __asm__("$2");

    __asm__ __volatile__(
        ".set push\n\t"
        ".set noreorder\n\t"
        "move $2,$4\n\t"
        "lqc2 vf20,0($2)\n\t"
        "vaddw.x vf2x,vf0x,vf20w\n\t"
        "qmfc2 $4,vf2\n\t"
        "mtc1 $4,$f1\n\t"
        "vmulax.xyzw ACC,vf16xyzw,vf20x\n\t"
        "vmadday.xyzw ACC,vf17xyzw,vf20y\n\t"
        "vmaddaz.xyzw ACC,vf18xyzw,vf20z\n\t"
        "vmaddw.xyzw vf9xyzw,vf19xyzw,vf0w\n\t"
        "qmfc2 $4,vf9\n\t"
        "mtc1 $4,$f0\n\t"
        "nop\n\t"
        "c.lt.s $f0,$f1\n\t"
        "nop\n\t"
        "bc1f 1f\n\t"
        "nop\n\t"
        "nop\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $4,vf9\n\t"
        "mtc1 $4,$f0\n\t"
        "nop\n\t"
        "c.lt.s $f0,$f1\n\t"
        "nop\n\t"
        "bc1f 1f\n\t"
        "nop\n\t"
        "nop\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $4,vf9\n\t"
        "mtc1 $4,$f0\n\t"
        "nop\n\t"
        "c.lt.s $f0,$f1\n\t"
        "nop\n\t"
        "bc1f 1f\n\t"
        "nop\n\t"
        "nop\n\t"
        "vmulax.xyzw ACC,vf12xyzw,vf20x\n\t"
        "vmadday.xyzw ACC,vf13xyzw,vf20y\n\t"
        "vmaddaz.xyzw ACC,vf14xyzw,vf20z\n\t"
        "vmaddw.xyzw vf9xyzw,vf15xyzw,vf0w\n\t"
        "qmfc2 $4,vf9\n\t"
        "mtc1 $4,$f0\n\t"
        "nop\n\t"
        "c.lt.s $f0,$f1\n\t"
        "nop\n\t"
        "bc1f 1f\n\t"
        "nop\n\t"
        "nop\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $4,vf9\n\t"
        "mtc1 $4,$f0\n\t"
        "nop\n\t"
        "c.lt.s $f0,$f1\n\t"
        "nop\n\t"
        "bc1f 1f\n\t"
        "nop\n\t"
        "nop\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $4,vf9\n\t"
        "mtc1 $4,$f0\n\t"
        "nop\n\t"
        "c.lt.s $f0,$f1\n\t"
        "nop\n\t"
        "bc1f 1f\n\t"
        "nop\n\t"
        "nop\n\t"
        "sub $4,$4,$4\n\t"
        "j 2f\n\t"
        "nop\n\t"
        "nop\n\t"
        "1:\n\t"
        "sub $4,$4,$4\n\t"
        "addi $4,$4,1\n\t"
        "2:\n\t"
        "move $2,$4\n\t"
        ".set pop"
        : "=r"(result), "+r"(arg)
        :
        : "$f0", "$f1", "memory");
    return result;
}

int sefClipViewVolume(void *scheduler, void *effect)
{
    sefInitClipViewVolume((unsigned char *)effect + SEF_CLIP_VIEW_VOLUME_OFFSET);
    return sefClipViewVolumeA(scheduler);
}

extern unsigned long long alphaTbl_004CBE78[4];
typedef struct SvSpriteParticle {
    unsigned char unmodeled_00[0xC0];
    float centerX;
    float centerY;
    unsigned char unmodeled_C8[0xF8 - 0xC8];
    float angle;
    unsigned char unmodeled_FC[4];
    float scaleX;
    float scaleY;
    unsigned char unmodeled_108[0x140 - 0x108];
    SGsGifRecord packedColor;
    unsigned char unmodeled_150[18];
    unsigned char blendMode;
    unsigned char unmodeled_163[0x178 - 0x163];
    unsigned long long texture;
    float halfWidth;
    float halfHeight;
    unsigned char unmodeled_188[0x240 - 0x188];
    float offsetX;
    float offsetY;
    unsigned char unmodeled_248[0x280 - 0x248];
} SvSpriteParticle;
typedef struct SvSpriteVertex {
    float x;
    float y;
} SvSpriteVertex;
static inline float svSpriteExtentPixels(float extent, float scale)
{
    float scaledExtent = extent * scale;
    return scaledExtent * 100.0f;
}
static inline void svAppendSpriteVertex(SGsPacket *packet,
                                         const Quadword *textureCoordinates,
                                         const SvSpriteVertex *vertex,
                                         float centerX, float centerY)
{
    int screenX;
    int screenY;

    packet->base[packet->count].quad = *textureCoordinates;
    packet->count++;
    screenX = (int)(centerX + vertex->x) * 16 + 0x7000;
    screenY = (int)(centerY + vertex->y) * 16 + 0x7200;
    packet->base[packet->count].word[0] = screenX;
    packet->base[packet->count].word[1] = screenY;
    packet->base[packet->count].word[2] = 0xFFFFFA;
    packet->base[packet->count].word[3] = 0;
    packet->count++;
}

void sefDrawParticle2D(SvSpriteParticle *particle, SGsPacket *packet,
                      const Quadword textureCoordinates[4])
{
    float scaleX;
    float scaleY;
    float halfWidth;
    float halfHeight;
    float centerX;
    float centerY;
    float offsetX;
    float offsetY;
    float angle;
    float sine;
    float cosine;
    float cornerX;
    float cornerY;
    SvSpriteVertex vertex;
    unsigned long long gifRegisters;
    unsigned long long gifTag;

    gifRegisters = 0x0525252521EEULL;
    packet->loopCount = 1;
    packet->tagIndex = packet->count;
    packet->base[packet->count].dword[1] = gifRegisters;
    gifTag = 0xB02A400000008000ULL;
    packet->base[packet->count].dword[0] = gifTag;
    packet->count++;
    packet->base[packet->count].dword[0] = particle->texture;
    packet->base[packet->count].dword[1] = 6;
    packet->count++;
    packet->base[packet->count].dword[0] = alphaTbl_004CBE78[particle->blendMode];
    packet->base[packet->count].dword[1] = 66;
    packet->count++;
    packet->base[packet->count].quad = particle->packedColor.quad;
    packet->count++;

    scaleX = particle->scaleX;
    scaleY = particle->scaleY;
    halfWidth = particle->halfWidth * scaleX;
    halfHeight = svSpriteExtentPixels(particle->halfHeight, scaleY);
    centerX = particle->centerX * 10.0f;
    centerY = particle->centerY * 10.0f;
    offsetX = particle->offsetX;
    offsetY = particle->offsetY;
    centerX += 256.0f;
    centerY += 224.0f;
    offsetX *= scaleX;
    offsetY *= scaleY;
    halfWidth *= 100.0f;
    angle = particle->angle;
    __asm__ __volatile__(
        "mfc1 $8, %1\n\tqmtc2 $8, vf4\n\tvcallms 0x20\n\t"
        "qmfc2.i $8, vf1\n\tmtc1 $8, %0"
        : "=f"(sine) : "f"(angle) : "$8", "memory");
    __asm__ __volatile__(
        "mfc1 $8, %1\n\tqmtc2 $8, vf4\n\tvcallms 0xE8\n\t"
        "qmfc2.i $8, vf1\n\tmtc1 $8, %0"
        : "=f"(cosine) : "f"(angle) : "$8", "memory");

    cornerX = offsetX - halfWidth;
    cornerY = offsetY - halfHeight;
    vertex.x = cornerX * cosine - cornerY * sine;
    vertex.y = cornerX * sine + cornerY * cosine;
    svAppendSpriteVertex(packet, &textureCoordinates[0], &vertex, centerX, centerY);
    cornerX = offsetX + halfWidth;
    cornerY = offsetY - halfHeight;
    vertex.x = cornerX * cosine - cornerY * sine;
    vertex.y = cornerX * sine + cornerY * cosine;
    svAppendSpriteVertex(packet, &textureCoordinates[1], &vertex, centerX, centerY);
    cornerX = offsetX - halfWidth;
    cornerY = offsetY + halfHeight;
    vertex.x = cornerX * cosine - cornerY * sine;
    vertex.y = cornerX * sine + cornerY * cosine;
    svAppendSpriteVertex(packet, &textureCoordinates[2], &vertex, centerX, centerY);
    cornerX = offsetX + halfWidth;
    cornerY = offsetY + halfHeight;
    vertex.x = cornerX * cosine - cornerY * sine;
    vertex.y = cornerX * sine + cornerY * cosine;
    svAppendSpriteVertex(packet, &textureCoordinates[3], &vertex, centerX, centerY);
    packet->loopCount = 1;
    packet->base[packet->tagIndex].dword[0] |= packet->loopCount;
}

static void svRotMatrixScale(void *dst, const void *src)
{
    const char *base = (const char *)src;
    const char *contrib = base + 0xC0;
    const char *scale = base + 0x100;

    __asm__ __volatile__(
        "lqc2 vf8, 0(%2)\n\t"
        "lqc2 vf1, 0(%1)\n\t"
        "lqc2 vf2, 16(%1)\n\t"
        "lqc2 vf3, 32(%1)\n\t"
        "lqc2 vf4, 48(%1)\n\t"
        "lqc2 vf5, 0(%3)\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf1x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf1y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf1z\n\t"
        "vmaddw.xyzw vf9xyzw, vf8xyzw, vf1w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf2x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf2y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf2z\n\t"
        "vmaddw.xyzw vf10xyzw, vf8xyzw, vf2w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf3x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf3y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf3z\n\t"
        "vmaddw.xyzw vf11xyzw, vf8xyzw, vf3w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf4x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf4y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf4z\n\t"
        "vmaddw.xyzw vf12xyzw, vf8xyzw, vf4w\n\t"
        "vmulx.xyzw vf9xyzw, vf9xyzw, vf5x\n\t"
        "vmuly.xyzw vf10xyzw, vf10xyzw, vf5y\n\t"
        "vmulz.xyzw vf11xyzw, vf11xyzw, vf5z\n\t"
        "vmulw.xyzw vf12xyzw, vf12xyzw, vf5w\n\t"
        "sqc2 vf9, 0(%0)\n\t"
        "sqc2 vf10, 16(%0)\n\t"
        "sqc2 vf11, 32(%0)\n\t"
        "sqc2 vf12, 48(%0)\n\t"
        "nop"
        : : "r"(dst), "r"(base), "r"(contrib), "r"(scale) : "memory");
}

typedef struct SvModelPosition {
    int x;
    int y;
    int z;
    int alpha;
} SvModelPosition;

typedef struct SvModelDrawItem {
    Matrix4 orientation;
    Matrix4 cfMatrix;
    unsigned char unmodeled_080[64];
    Vector4 origin;
    unsigned char unmodeled_0d0[48];
    Vector4 scale;
    unsigned char unmodeled_110[48];
    SvModelPosition position;
    unsigned char unmodeled_150[4];
    float scaleMultiplier;
    unsigned char unmodeled_158[10];
    unsigned char blendMode;
    unsigned char unmodeled_163[13];
    void *model;
    void *texture;
    unsigned char unmodeled_178[16];
    short renderType;
    unsigned char unmodeled_18a[2];
    unsigned int renderFlags;
    unsigned char unmodeled_190[0x280 - 0x190];
} SvModelDrawItem;

extern unsigned long long alphaTbl_2[];
extern void nmlModelSetSortOffset(float offset);
extern void nmlModelSetFogCancel(void);
extern void nmlModelSetLight(void *colorMod, void *normal);
extern void nmlModelSetSpecularOff(int disableSpecular);
extern void nmlModelSetRenderStatus(int status);
extern void nmlModelSetToumei(int enabled);
extern void nmlModelSetAlpha(unsigned long long alpha);
extern void nmlModelSetTexture(void *texture);
extern void nmlModelSetClip(int clip);
extern void nmlModelSetPlace(void *matrix);
extern void nmlModelEntryEffect(void *effect);

void svDrawModel(SvModelDrawItem *model, void *place)
{
    unsigned long long alpha;
    int a;

    if (*(unsigned char *)((unsigned char *)model + 0x162) == 0 && *(int *)((unsigned char *)model + 0x14C) == 0x80) {
        alpha = 0;
    } else {
        a = *(int *)((unsigned char *)model + 0x14C);
        alpha = alphaTbl_2[*(unsigned char *)((unsigned char *)model + 0x162)] | ((unsigned long long)a << 32);
        nmlModelSetSortOffset(-5.0f);
    }
    nmlModelSetFogCancel();
    {
        unsigned char *material;
        if (*(unsigned char *)((unsigned char *)model + 0x162) == 0) {
            material = (unsigned char *)&_defMatLcMod;
        } else {
            material = (unsigned char *)&_defMatLc;
        }
        {
            unsigned char light[4][16];
            __asm__ __volatile__(
                "lq $8, 0(%1)\n\t"
                "lq $9, 16(%1)\n\t"
                "lq $10, 32(%1)\n\t"
                "lq $11, 48(%1)\n\t"
                "sq $8, 0(%0)\n\t"
                "sq $9, 16(%0)\n\t"
                "sq $10, 32(%0)\n\t"
                "sq $11, 48(%0)"
                :
                : "r"(light), "r"(material)
                : "$8", "$9", "$10", "$11", "memory");
            {
                unsigned char *dst = light[3];
                unsigned char *src = (unsigned char *)model + 0x140;
                __asm__ __volatile__(
                    ".set noat\n\t"
                    "lui $1, 0x3C00\n\t"
                    "mtc1 $1, $f0\n\t"
                    "nop\n\t"
                    "mfc1 $8, $f0\n\t"
                    "qmtc2.ni $8, vf1\n\t"
                    "lqc2 vf2, 0(%1)\n\t"
                    "vitof0.xyzw vf2, vf2\n\t"
                    "vmulx.xyz vf2, vf2, vf1x\n\t"
                    "sqc2 vf2, 0(%0)\n\t"
                    ".set at\n\t"
                    :
                    : "r"(dst), "r"(src)
                    : "$1", "$f0", "$8", "memory");
            }
            nmlModelSetLight(light, &_defMatLn);
        }
    }
    nmlModelSetSpecularOff(1);
    if (*(int *)((unsigned char *)model + 0x18C) & 1) {
        nmlModelSetRenderStatus(0x100);
    }
    if (alpha != 0) {
        nmlModelSetToumei(1);
        nmlModelSetAlpha(alpha);
    }
    if (*(int *)((unsigned char *)model + 0x174) != 0) {
        nmlModelSetTexture(*(void **)((unsigned char *)model + 0x174));
    }
    nmlModelSetClip(1);
    nmlModelSetPlace(place);
    nmlModelEntryEffect(*(void **)((unsigned char *)model + 0x170));
}

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", sefDrawParticle2);

typedef struct SvParticleDrawContext {
    Matrix4 *place;
    Vector4 *scale;
    const Quadword *textureCoordinates;
    const Quadword *gifRegisters;
    float scaleMultiplier;
    int drawVariant;
} SvParticleDrawContext;

typedef struct SvQuadImageParameters {
    unsigned char unmodeled_00[8];
    unsigned long long texture;
    float halfWidth;
    float halfHeight;
} SvQuadImageParameters;
typedef struct SvQuadParticle {
    Matrix4 localTransform;
    unsigned char unmodeled_40[248 - 64];
    float angle;
    unsigned char unmodeled_FC[320 - 252];
    SGsGifRecord packedColor;
    unsigned char unmodeled_150[18];
    unsigned char blendMode;
    unsigned char unmodeled_163[368 - 355];
    SvQuadImageParameters imageParameters;
    unsigned char unmodeled_188[576 - 392];
    Vector4 planeOffset;
    unsigned char unmodeled_250[640 - 592];
} SvQuadParticle;
static inline const SvQuadImageParameters *svPrepareParticleImage(
    SGsGifRecord *header, SvQuadParticle *particle, Quadword gifRegisters,
    unsigned int textureRegister, unsigned int alphaRegister)
{
    const SvQuadImageParameters *image = &particle->imageParameters;
    unsigned long long textureValue;

    header->quad = gifRegisters;
    textureValue = image->texture;
    header[1].dword[1] = textureRegister;
    header[1].dword[0] = textureValue;
    header[2].dword[0] = alphaTbl_004CBE78[particle->blendMode];
    header[2].dword[1] = alphaRegister;
    header[3].quad = particle->packedColor.quad;
    return image;
}

void sefDrawParticle(SvQuadParticle *particle, const Vector4 *position,
                     SGsPacket *packet, SvParticleDrawContext *context)
{
    SGsGifRecord *header;
    SGsGifRecord *vertices;
    SGsGifRecord *xyzVertices;
    const SvQuadImageParameters *image;
    const Quadword *textureCoordinates;
    const Vector4 *scale;
    float angle;
    float halfWidth;
    float halfHeight;
    int clipFlags;

    scale = context->scale;
    header = &packet->base[packet->count];
    vertices = &header[4];
    angle = particle->angle;
    __asm__ __volatile__(
        "lqc2 vf8, 0(%0)\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf16x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf16y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf16z\n\t"
        "vmaddw.xyzw vf9xyzw, vf8xyzw, vf16w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf17x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf17y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf17z\n\t"
        "vmaddw.xyzw vf10xyzw, vf8xyzw, vf17w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf18x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf18y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf18z\n\t"
        "vmaddw.xyzw vf11xyzw, vf8xyzw, vf18w\n\t"
        "vmulax.xyzw ACCxyzw, vf13xyzw, vf19x\n\t"
        "vmadday.xyzw ACCxyzw, vf14xyzw, vf19y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf15xyzw, vf19z\n\t"
        "vmaddw.xyzw vf12xyzw, vf8xyzw, vf19w\n\t"
        "mfc1 $8, %2\n\tqmtc2 $8, vf4\n\tvcallms 0xe8\n\t"
        "vaddx.x vf7x, vf0x, vf1x\n\tqmtc2 $8, vf4\n\t"
        "vcallms 0x20\n\tvaddx.x vf8x, vf0x, vf1x\n\t"
        "lqc2 vf1, 0(%1)\n\t"
        "vmulax.xyzw ACCxyzw, vf9xyzw, vf7x\n\t"
        "vmaddx.xyzw vf5xyzw, vf10xyzw, vf8x\n\t"
        "vmulax.xyzw ACCxyzw, vf10xyzw, vf7x\n\t"
        "vmsubx.xyzw vf6xyzw, vf9xyzw, vf8x\n\t"
        "vmulz.xyzw vf11xyzw, vf11xyzw, vf1z\n\t"
        "vmulw.xyzw vf12xyzw, vf12xyzw, vf1w\n\t"
        "vmulx.xyzw vf9xyzw, vf5xyzw, vf1x\n\t"
        "vmuly.xyzw vf10xyzw, vf6xyzw, vf1y\n\t"
        "vmulax.xyzw ACCxyzw, vf21xyzw, vf9x\n\t"
        "vmadday.xyzw ACCxyzw, vf22xyzw, vf9y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf23xyzw, vf9z\n\t"
        "vmaddw.xyzw vf27xyzw, vf24xyzw, vf9w\n\t"
        "vmulax.xyzw ACCxyzw, vf21xyzw, vf10x\n\t"
        "vmadday.xyzw ACCxyzw, vf22xyzw, vf10y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf23xyzw, vf10z\n\t"
        "vmaddw.xyzw vf28xyzw, vf24xyzw, vf10w\n\t"
        "vmulax.xyzw ACCxyzw, vf21xyzw, vf11x\n\t"
        "vmadday.xyzw ACCxyzw, vf22xyzw, vf11y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf23xyzw, vf11z\n\t"
        "vmaddw.xyzw vf29xyzw, vf24xyzw, vf11w\n\t"
        "vmulax.xyzw ACCxyzw, vf21xyzw, vf12x\n\t"
        "vmadday.xyzw ACCxyzw, vf22xyzw, vf12y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf23xyzw, vf12z\n\t"
        "vmaddw.xyzw vf30xyzw, vf24xyzw, vf12w"
        : : "r"(position), "r"(scale), "f"(angle)
        : "$8", "memory");

    __asm__ __volatile__("" : : "r"(vertices));
    image = svPrepareParticleImage(header, particle, *context->gifRegisters,
                                   6, 66);
    halfWidth = image->halfWidth;
    halfHeight = image->halfHeight;
    __asm__ __volatile__(
        "lqc2 vf5, 0(%2)\n\tmfc1 $2, %0\n\tmfc1 $3, %1\n\t"
        "qmtc2 $2, vf6\n\tqmtc2 $3, vf7\n\t"
        "vmove.zw vf1zw, vf5zw\n\tvmove.zw vf2zw, vf5zw\n\t"
        "vmove.zw vf3zw, vf5zw\n\tvmove.zw vf4zw, vf5zw\n\t"
        "vsubx.x vf1x, vf5x, vf6x\n\tvaddx.x vf2x, vf5x, vf6x\n\t"
        "vsubx.x vf3x, vf5x, vf6x\n\tvaddx.x vf4x, vf5x, vf6x\n\t"
        "vaddx.y vf1y, vf5y, vf7x\n\tvaddx.y vf2y, vf5y, vf7x\n\t"
        "vsubx.y vf3y, vf5y, vf7x\n\tvsubx.y vf4y, vf5y, vf7x"
        : : "f"(halfWidth), "f"(halfHeight), "r"(&particle->planeOffset)
        : "$2", "$3", "memory");

    textureCoordinates = context->textureCoordinates;
    xyzVertices = &vertices[1];
    vertices[0].quad = textureCoordinates[0];
    vertices[2].quad = textureCoordinates[1];
    vertices[4].quad = textureCoordinates[2];
    vertices[6].quad = textureCoordinates[3];
    /* All XYZ2 stores finish before the clip flags are returned to C. */
    __asm__ __volatile__(
        "ctc2 $0, $vi18\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf1x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf1y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf1z\n\t"
        "vmaddw.xyzw vf9xyzw, vf30xyzw, vf0w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf2x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf2y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf2z\n\t"
        "vmaddw.xyzw vf10xyzw, vf30xyzw, vf0w\n\t"
        "vclipw.xyz vf9xyz, vf9w\n\tvdiv Q, vf0w, vf9w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf3x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf3y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf3z\n\t"
        "vmaddw.xyzw vf11xyzw, vf30xyzw, vf0w\n\t"
        "vclipw.xyz vf10xyz, vf10w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf4x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf4y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf4z\n\t"
        "vmaddw.xyzw vf12xyzw, vf30xyzw, vf0w\n\t"
        "vclipw.xyz vf11xyz, vf11w\n\t"
        "vmulq.xyz vf9xyz, vf9xyz, Q\n\tvmulq.xyz vf10xyz, vf10xyz, Q\n\t"
        "vmulq.xyz vf11xyz, vf11xyz, Q\n\tvclipw.xyz vf12xyz, vf12w\n\t"
        "vmulq.xyz vf12xyz, vf12xyz, Q\n\t"
        "vmulaw.xyzw ACCxyzw, vf26xyzw, vf0w\n\t"
        "vmadd.xyzw vf1xyzw, vf9xyzw, vf25xyzw\n\t"
        "vmulaw.xyzw ACCxyzw, vf26xyzw, vf0w\n\t"
        "vmadd.xyzw vf2xyzw, vf10xyzw, vf25xyzw\n\t"
        "vmulaw.xyzw ACCxyzw, vf26xyzw, vf0w\n\t"
        "vmadd.xyzw vf3xyzw, vf11xyzw, vf25xyzw\n\t"
        "vmulaw.xyzw ACCxyzw, vf26xyzw, vf0w\n\t"
        "vmadd.xyzw vf4xyzw, vf12xyzw, vf25xyzw\n\t"
        "vftoi4.xy vf9xy, vf1xy\n\tvftoi4.xy vf10xy, vf2xy\n\t"
        "vftoi4.xy vf11xy, vf3xy\n\tvftoi4.xy vf12xy, vf4xy\n\t"
        "vftoi0.zw vf9zw, vf1zw\n\tvftoi0.zw vf10zw, vf2zw\n\t"
        "vftoi0.zw vf11zw, vf3zw\n\tvftoi0.zw vf12zw, vf4zw\n\t"
        "vsub.w vf9w, vf0w, vf0w\n\tvsub.w vf10w, vf0w, vf0w\n\t"
        "vsub.w vf11w, vf0w, vf0w\n\tvsub.w vf12w, vf0w, vf0w\n\t"
        "sqc2 vf9, 0(%1)\n\tsqc2 vf10, 32(%1)\n\t"
        "sqc2 vf11, 64(%1)\n\tsqc2 vf12, 96(%1)\n\tcfc2.ni %0, $vi18"
        : "=r"(clipFlags) : "r"(xyzVertices) : "memory");
    if (clipFlags == 0) {
        int tagIndex = packet->count;

        packet->tagIndex = tagIndex;
        packet->count = tagIndex + 12;
        header->dword[0] |= 1;
    }
}

typedef struct SvParticleEffect {
    unsigned char unmodeled_00[4];
    const short *scriptWords;
    unsigned char unmodeled_08[40];
    Vector4 position;
    unsigned char unmodeled_40[464 - 64];
    const short *renderDomainA;
    unsigned char unmodeled_1D4[12];
    const short *renderDomainB;
    unsigned char unmodeled_1E4[0x600 - 484];
    short particleIndices[256];
    unsigned char unmodeled_800[0x812 - 0x800];
    unsigned short flags;
} SvParticleEffect;
extern Matrix4 _invView;
extern Matrix4 _svUnitMatrix;
extern const Quadword stqTbl_004CBEA0[4];
extern const Quadword _svRegData;
void svDrawModel(SvModelDrawItem *item, void *place);

void sefDrawParticleList(SvParticleEffect *effect)
{
    Matrix4 modelPlace;
    SvParticleDrawContext context;
    SGsPacket *packet;
    int is2D;
    int drawMode;
    int drawVariant;
    SvModelDrawItem *particles;
    short *particleIndex;
    short quadRenderType;
    int available;

    is2D = 0;
    drawMode = _draw3D;
    if (*effect->renderDomainA == 3 || *effect->renderDomainB == 3) {
        is2D = 1;
    }
    drawVariant = 0;
    if (effect->scriptWords != 0) {
        drawVariant = effect->scriptWords[-1];
    }
    packet = &_packet;
    particles = (SvModelDrawItem *)sevGetPtAllocator2(0);
    if (drawMode != 1 || is2D != 0) {
        if (drawMode != 1 && is2D == 1) {
            _nEffect2D++;
            _imageMapper.referenceImages[_nowImage].words[1]++;
        } else {
            if (drawMode == 0) {
                _imageMapper.referenceImages[_nowImage].words[0]++;
            }
            context.textureCoordinates = stqTbl_004CBEA0;
            context.gifRegisters = &_svRegData;
            context.drawVariant = drawVariant;
            __asm__ __volatile__(
                "lqc2 vf16, 0(%0)\n\tlqc2 vf17, 16(%0)\n\t"
                "lqc2 vf18, 32(%0)\n\tlqc2 vf19, 48(%0)"
                : : "r"(&_invView) : "memory");
            __asm__ __volatile__(
                "lqc2 vf13, 0(%0)\n\tlqc2 vf14, 16(%0)\n\tlqc2 vf15, 32(%0)"
                : : "r"(&_svUnitMatrix) : "memory");
            packet->loopCount = 1;
            packet->base = (SGsGifRecord *)0x70000000;
            packet->count = 0;
            available = 64;
            quadRenderType = 4;
            packet->tagIndex = 0;
            particleIndex = effect->particleIndices;
            {
                int remaining = 255;

                while (remaining >= 0) {
                    int index = *particleIndex;

                    if (index >= 0) {
                        SvModelDrawItem *item = &particles[index];

                        if (item->position.alpha != 0) {
                            switch (drawMode) {
                            case 2:
                                if (item->renderType == 6) {
                                    svRotMatrixScale(&modelPlace, item);
                                    svDrawModel(item, &modelPlace);
                                }
                                break;
                            case 1:
                                if (item->renderType == quadRenderType) {
                                    available--;
                                    sefDrawParticle2D(item, packet, context.textureCoordinates);
                                    if (available == 0) {
                                        available = 64;
                                        SGsSendPacket((SGsPacketReset *)packet);
                                    }
                                }
                                break;
                            case 0:
                                if (item->renderType == quadRenderType) {
                                    context.scale = &item->scale;
                                    if (context.drawVariant == 0) {
                                        sefDrawParticle(item, &item->origin, packet, &context);
                                    } else {
                                        sefDrawParticle2(item, &item->origin, packet, &context);
                                    }
                                    available--;
                                    if (available == 0) {
                                        available = 64;
                                        SGsSendPacket((SGsPacketReset *)packet);
                                    }
                                }
                                break;
                            }
                        }
                    }
                    remaining--;
                    particleIndex++;
                }
            }
            SGsSendPacket((SGsPacketReset *)packet);
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/sv_get_image_item", sefDrawParticleListCf);

typedef struct SvParticleListCfContext {
    Matrix4 *place;
    Vector4 *scaledScale;
    const Quadword *textureCoordinates;
    const Quadword *gifRegisters;
    float scaleMultiplier;
    int drawVariant;
    unsigned char unmodeled_18[8];
    int schedulerVariant;
} SvParticleListCfContext;
typedef struct SvSchedulerEffectGroup {
    signed char effectIds[0x20];
    unsigned char unmodeled_20[0x0C];
    short effectCount;
    unsigned char unmodeled_2e[2];
} SvSchedulerEffectGroup;
typedef struct SvSchedulerEffectRecord {
    Quadword transform[4];
    Quadword localTransform[4];
    Quadword packetInfo;
    unsigned char unmodeled_90[0x10];
    float localScale;
    unsigned char unmodeled_a4[0x0C];
    SvSchedulerEffectGroup groups[0x20];
    int inUse;
    unsigned char unmodeled_6b4[8];
    void *transformFlagA;
    unsigned char unmodeled_6c0[4];
    void *transformFlagB;
    unsigned char unmodeled_6c8[0x3C0];
    float scaleMultiplier;
    unsigned int flags;
    unsigned char unmodeled_a90[10];
    signed char active;
    unsigned char unmodeled_a9b[0x15];
} SvSchedulerEffectRecord;
typedef struct SvBattleEffectData {
    unsigned char unmodeled_00[0x30];
    Quadword packetInfo;
    unsigned char unmodeled_40[0x1DC];
    float scaleMultiplier;
    unsigned char unmodeled_220[4];
    unsigned int flags;
    unsigned char unmodeled_228[4];
    unsigned int partialFlags;
} SvBattleEffectData;
extern Vector4 *MMathApplyMatrix(Vector4 *out, const Matrix4 *matrix, const Vector4 *vector);
extern Matrix4 *MMathMulMatrix(Matrix4 *out, const Matrix4 *left, const Matrix4 *right);
extern unsigned char *sefGetBattleData(void);
extern unsigned char *sefGetLineAdr(int lineIndex);
extern float sefGetMatrixScale(float *matrix);
extern StudioCamera *xglStudioGetActiveCamera(void);
void sefDrawParticleListCf(SvParticleEffect *effect, SvParticleListCfContext *context, SvSchedulerEffectRecord *scheduler);

static void sefDrawSchedulerEffect(int schedulerIndex)
{
    SvSchedulerEffectRecord *scheduler =
        (SvSchedulerEffectRecord *)(sefGetScheduler() + schedulerIndex * 0xAB0);
    SvBattleEffectData *battleData = (SvBattleEffectData *)sefGetBattleData();
    StudioCamera *camera;
    int drawMode;
    int groupIndex;
    int effectIndex;

    if (scheduler->inUse == 0 || scheduler->active == 0) {
        return;
    }

    camera = xglStudioGetActiveCamera();
    __asm__ __volatile__(
        "lqc2 vf21, 0(%0)\n\t"
        "lqc2 vf22, 16(%0)\n\t"
        "lqc2 vf23, 32(%0)\n\t"
        "lqc2 vf24, 48(%0)"
        : : "r"(&camera->viewMatrix) : "memory");
    __asm__ __volatile__(
        "lqc2 vf25, 0(%0)\n\t"
        "lqc2 vf26, 0(%1)"
        : : "r"(&camera->screenScale), "r"(&camera->screenOffset) : "memory");

    battleData->flags = scheduler->flags;
    battleData->partialFlags = scheduler->flags & 2;
    if ((scheduler->flags & 2) == 0 || (drawMode = _draw3D) == 1) {
        __asm__ __volatile__(
            "lq $8, 0(%1)\n\t"
            "sq $8, 0(%0)"
            : : "r"(&battleData->packetInfo), "r"(&scheduler->packetInfo)
            : "$8", "memory");
        for (groupIndex = 0; groupIndex < 32; groupIndex++) {
            SvSchedulerEffectGroup *group = &scheduler->groups[groupIndex];

            if (group->effectCount == 0) {
                continue;
            }
            for (effectIndex = 0; effectIndex < 32; effectIndex++) {
                int lineIndex = group->effectIds[effectIndex];

                if (lineIndex >= 0) {
                    sefDrawParticleList((SvParticleEffect *)sefGetLineAdr(lineIndex));
                }
            }
        }
    } else {
        Quadword localTransform[4];
        Vector4 position;
        SvParticleListCfContext context;
        float matrixScale;
        const Quadword *gifRegisters;

        context.textureCoordinates = stqTbl_004CBEA0;
        gifRegisters = &_svRegData;
        context.schedulerVariant = (unsigned short)scheduler->flags & 0x10;
        context.gifRegisters = gifRegisters;
        context.place = (Matrix4 *)localTransform;
        if (scheduler->transformFlagA != 0 || scheduler->transformFlagB != 0) {
            MMathMulMatrix((Matrix4 *)localTransform,
                           (const Matrix4 *)scheduler->transform,
                           (const Matrix4 *)scheduler->localTransform);
            matrixScale = sefGetMatrixScale((float *)localTransform);
        } else {
            __asm__ __volatile__(
                "lq $8, 0(%1)\n\t"
                "lq $9, 16(%1)\n\t"
                "lq $10, 32(%1)\n\t"
                "lq $11, 48(%1)\n\t"
                "sq $8, 0(%0)\n\t"
                "sq $9, 16(%0)\n\t"
                "sq $10, 32(%0)\n\t"
                "sq $11, 48(%0)"
                : : "r"(localTransform), "r"(scheduler->localTransform)
                : "$8", "$9", "$10", "$11", "memory");
            matrixScale = scheduler->localScale;
        }
        battleData->scaleMultiplier = scheduler->scaleMultiplier * matrixScale;

        for (groupIndex = 0; groupIndex < 32; groupIndex++) {
            SvSchedulerEffectGroup *group = &scheduler->groups[groupIndex];

            if (group->effectCount == 0) {
                continue;
            }
            for (effectIndex = 0; effectIndex < 32; effectIndex++) {
                int lineIndex = group->effectIds[effectIndex];

                if (lineIndex >= 0) {
                    SvParticleEffect *effect =
                        (SvParticleEffect *)sefGetLineAdr(lineIndex);
                    Vector4 *effectPosition = &effect->position;

                    context.place = (Matrix4 *)localTransform;
                    context.scaleMultiplier = matrixScale;
                    if (*effect->renderDomainA == 3 || *effect->renderDomainB == 3) {
                        char *base = (char *)&_imageMapper.typeLists[0].size;
                        int offset = _nowImage * 8 + 369760;
                        SvReferenceImageSlot *slot = (SvReferenceImageSlot *)(base + offset);

                        _nEffect2D++;
                        slot->words[0]++;
                    } else if ((effect->flags & 0x400) != 0) {
                        sefDrawParticleListCf(effect, &context, scheduler);
                    } else {
                        MMathApplyMatrix(&position, (Matrix4 *)localTransform, effectPosition);
                        position.w = battleData->scaleMultiplier;
                        if (context.schedulerVariant != 0 &&
                            drawMode == 0 &&
                            sefClipViewVolume(&position, camera) != 0) {
                            continue;
                        }
                        sefDrawParticleListCf(effect, &context, scheduler);
                    }
                }
            }
        }
    }
}

static void svDrawSchedulerBlk(int eftCate, int eftNo)
{
    short effectId;
    int schedulerIndex;
    SchedulerRecord *record;
    SchedulerRecord *scheduler;

    scheduler = (SchedulerRecord *)sefGetScheduler();
    schedulerIndex = 0;
    record = scheduler;
    do {
        if (record->inUse != 0) {
            effectId = record->effectId;
            if (eft_5 != effectId) {
                eft_5 = effectId;
                srsAnalyzeEftNo(effectId, &charID_3, &eftCate_4);
            }
            if (eftCate_4 == eftCate &&
                (eftNo == 0 || (eftNo > 0 && eftNo == eft_5))) {
                SGsSetZTestEnv(((record->flags >> 2) ^ 1) & 1);
                sefDrawSchedulerEffect(schedulerIndex);
            }
        }
        schedulerIndex++;
        record++;
    } while (schedulerIndex < SCHEDULER_COUNT);
}

static void svDrawAlters(int layer)
{
    if (_draw3D == 0) {
        sdvDrawAlters(layer);
    }
}

static void svDrawMissile(int category)
{
    if (_draw3D == 0) {
        if (_scMslCate == category) {
            MEfObjExec2nd();
        }
    }
}

extern void SGsInitEnv(void);
extern void SGsRestoreEnv(void);

static void svDrawSchedulerParticle(void)
{
    int i;
    int total;
    int draw3D;

    draw3D = _draw3D;
    SGsInitEnv();
    if (_imageMapper.typeLists[0].entryCount > 0) {
        if (svLoadImageList(&_imageMapper.typeLists[0]) > 0) {
            SGsTexFlush();
        }
        svDrawSchedulerBlk(0, 0);
    }
    svDrawMissile(0);
    if (draw3D != 1 || (int)_imageMapper.referenceImages[14].words[1] > 0
        || (int)_imageMapper.referenceImages[2].words[1] > 0) {
        total = 0;
        for (i = 2; i < 11; i++) {
            if (_imageMapper.typeLists[i].entryCount > 0) {
                total += svLoadImageList(&_imageMapper.typeLists[i]);
            }
        }
        if (total > 0) {
            SGsTexFlush();
        }
        svDrawSchedulerBlk(2, 0);
        svDrawAlters(2);
    }
    svDrawMissile(2);
    if (draw3D != 1 || (int)_imageMapper.referenceImages[14].words[1] > 0
        || (int)_imageMapper.referenceImages[11].words[1] > 0) {
        total = 0;
        for (i = 11; i < 14; i++) {
            if (_imageMapper.typeLists[i].entryCount > 0) {
                total += svLoadImageList(&_imageMapper.typeLists[i]);
            }
        }
        if (total > 0) {
            SGsTexFlush();
        }
        svDrawSchedulerBlk(0xB, 0);
        svDrawAlters(0xB);
    }
    svDrawMissile(0xB);
    if ((draw3D != 1 || (int)_imageMapper.referenceImages[14].words[1] > 0)
        && _imageMapper.typeLists[14].entryCount > 0) {
        if (svLoadImageList(&_imageMapper.typeLists[14]) > 0) {
            SGsTexFlush();
        }
        svDrawSchedulerBlk(0xE, 0);
    }
    svDrawAlters(0xE);
    svDrawMissile(0xE);
    if (_imageMapper.typeLists[15].entryCount > 0) {
        if (svLoadImageList(&_imageMapper.typeLists[15]) > 0) {
            SGsTexFlush();
        }
        svDrawSchedulerBlk(0xF, 0);
    }
    for (i = 16; i < 40; i++) {
        if (_imageMapper.typeLists[i].entryCount > 0
            && (draw3D != 1 || (int)_imageMapper.referenceImages[i].words[1] > 0)) {
            if (svLoadImageList(&_imageMapper.typeLists[i]) > 0) {
                SGsTexFlush();
            }
            if (((SrsMemResSlots *)_srsMemRes)->no[i] > 0) {
                svDrawSchedulerBlk(0x10, ((SrsMemResSlots *)_srsMemRes)->no[i]);
            }
        }
    }
    SGsRestoreEnv();
}

void svDrawScheduler2D(void)
{
    if (_nEffect2D != 0) {
        _nowImage = 0;
        _draw3D = 1;
        svDrawSchedulerParticle();
    }
}

void svDrawScheduler(void)
{
    _draw3D = 0;
    _nEffect2D = 0;
    _nowImage = 0;
    svDrawSchedulerParticle();
}

extern void MEfObjExec1st(void);
extern void SGsInitEnv(void);
extern void SGsRestoreEnv(void);

void svDrawScheduler3D(void)
{
    int i;
    unsigned char *mem;
    short *cnt;
    short *no;

    _draw3D = 2;
    svInitRefImage();
    SGsInitEnv();
    if (_imageMapper.typeLists[0].entryCount > 0) {
        _nowImage = 0;
        svDrawSchedulerBlk(0, 0);
    }
    _nowImage = 2;
    svDrawSchedulerBlk(2, 0);
    _nowImage = 0xB;
    svDrawSchedulerBlk(0xB, 0);
    _nowImage = 0xE;
    svDrawSchedulerBlk(0xE, 0);
    _nowImage = 0xF;
    svDrawSchedulerBlk(0xF, 0);
    i = 0x10;
    mem = _srsMemRes;
    cnt = &_imageMapper.typeLists[16].entryCount;
    no = (short *)(mem + 0x110);
    do {
        short count = *cnt;
        cnt += 0x120E;
        if (count > 0) {
            short n = *no;
            if (n > 0) {
                _nowImage = i;
                svDrawSchedulerBlk(0x10, n);
            }
        }
        i++;
        no++;
    } while (i < 0x28);
    MEfObjExec1st();
    SGsRestoreEnv();
}

void SGsInitGifPacket(SGsPacket *packet)
{
    packet->loopCount = 1;
    packet->base = (SGsGifRecord *)0x70000000;
    packet->tagIndex = 0;
    packet->count = 0;
}

void SGsOpenGifPacket(SGsPacket *packet, int prim, unsigned long long registers,
                      unsigned int nreg)
{
    unsigned long long tag;

    packet->loopCount = 1;
    packet->tagIndex = packet->count;
    packet->base[packet->count].dword[1] = registers;
    tag = ((unsigned long long)prim << 47) |
          ((unsigned long long)nreg << 60) | 0x0000400000008000ULL;
    packet->base[packet->count].dword[0] = tag;
    packet->count++;
}

void SGsCloseGifPacket(SGsPacket *packet) {
    packet->base[packet->tagIndex].dword[0] |= packet->loopCount;
}

void SGsAddReg(SGsPacket *packet, unsigned long long address,
               unsigned long long value)
{
    packet->base[packet->count].dword[0] = value;
    packet->base[packet->count].dword[1] = address;
    packet->count++;
}

void SGsAddGifRGBA(SGsPacket *packet, Quadword rgba)
{
    packet->base[packet->count].quad = rgba;
    packet->count++;
}

void SGsAddGifXYZ2(SGsPacket *packet, int x, int y, int z, int fog)
{
    packet->base[packet->count].word[0] = x * 0x10 + 0x7000;
    packet->base[packet->count].word[1] = y * 0x10 + 0x7200;
    packet->base[packet->count].word[2] = z;
    packet->base[packet->count].word[3] = fog;
    packet->count++;
}

void SGsAddGifData(SGsPacket *packet, Quadword data)
{
    packet->base[packet->count].quad = data;
    packet->count++;
}

void SGsAddGifSTQ(SGsPacket *packet, float texture_s, float texture_t,
                  float texture_q)
{
    packet->base[packet->count].floatValue[0] = texture_s;
    packet->base[packet->count].floatValue[1] = texture_t;
    packet->base[packet->count].floatValue[2] = texture_q;
    packet->base[packet->count].word[3] = 0;
    packet->count++;
}

void SGsAddGifUV(SGsPacket *packet, int u_coordinate, int v_coordinate)
{
    packet->base[packet->count].halfword[0] =
        (unsigned short)((unsigned int)u_coordinate << 4);
    packet->base[packet->count].halfword[1] =
        (unsigned short)((unsigned int)v_coordinate << 4);
    packet->count++;
}

const char D_004DBA50[8] = {'%', 's', '.', 'b', 'm', 'p', '\0', '\0'};

const char D_004DBA58[8] = {'%', 's', '.', 'c', 'l', 't', '\0', '\0'};

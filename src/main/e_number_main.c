#include "common.h"
#include "shared.h"

extern void endSpriteSet(void *sprite, int mode);
extern void endPrintExtFunc(int work, int mode, void *descriptor);
typedef struct ENumberSpriteDescriptor {
    short x, y;
    int work;
    short width, height;
    unsigned char color[4];
    short spriteId;
    short unmodeled_012;
} ENumberSpriteDescriptor;
typedef struct ENumberRenderDescriptor {
    unsigned short x, y;
    int work;
    union {
        struct {
            signed char red, green, blue, alpha;
            int value;
        } number;
        struct {
            short width, height;
            unsigned char color[4];
        } sprite;
        struct {
            unsigned char color[4];
            const char *text;
        } text;
    } payload;
    /* Opcode 10: digit count and flags; opcode 2: sprite ID in argument1. */
    unsigned short argument1, argument2;
} ENumberRenderDescriptor;
typedef ENumberRenderDescriptor ENumberRenderArray[4];
typedef union ENumberValue {
    int integer;
    const char *text;
} ENumberValue;
typedef struct ENumberMainData {
    unsigned short x, y;
    int work;
    unsigned char color[4];
    unsigned char mode;
    unsigned char unmodeled_00d;
    unsigned char format;
    unsigned char digits;
    unsigned char colorMode[2];
    unsigned char alignment;
    unsigned char firstOnly;
    ENumberValue value;
    ENumberRenderArray render;
    ENumberSpriteDescriptor separators[2];
} ENumberMainData;
#define E_NUMBER_STORE_STRING(destination, string) (*(destination) = (string))

void eNumberMain(ENumberMainData *number)
{
    unsigned char byteValue[4];
    short pairValue[2];
    unsigned char colors[2];
    int component;
    unsigned short componentX;
    int byteX;
    int componentWork;
    unsigned char componentValue;
    int blankWork;
    int blankComponent;
    int blankValue;
    int part;
    int partX;
    int dash;
    int signX;
    int dashX;
    int spacing;
    int magnitude;
    ENumberRenderDescriptor *sprite;
    ENumberSpriteDescriptor *separatorSprite;
    ENumberRenderDescriptor *text;
    const unsigned char tintHigh = 144;
    const unsigned char opacity = 128;
    const unsigned char tintLow = 80;
    const unsigned char alertLow = 64;

    if ((number->mode & 15) != 3)
        return;

    switch (number->format) {
    case 0:
    case 1:
    case 14:
        number->render[0].x = number->x;
        number->render[0].y = number->y;
        number->render[0].work = number->work;
        number->render[0].payload.number.red = number->color[0];
        number->render[0].payload.number.green = number->color[1];
        number->render[0].payload.number.blue = number->color[2];
        number->render[0].payload.number.alpha = number->color[3];
        number->render[0].payload.number.value = number->value.integer;
        number->render[0].argument1 = number->digits;
        if (number->format == 1)
            number->render[0].argument2 = number->colorMode[0] * 16 + 1;
        else
            number->render[0].argument2 = number->colorMode[0] * 16;
        endPrintExtFunc(number->work, 10, &number->render[0]);
        if (number->format == 14 && number->value.integer >= 0) {
            number->separators[0].spriteId = 0x506;
            endSpriteSet(&number->separators[0], 255);
            number->separators[0].x = number->x + number->digits * 11;
            number->separators[0].work = number->work;
            number->separators[0].color[0] = number->separators[0].color[1] =
                number->separators[0].color[2] = number->separators[0].color[3] = 128;
            number->separators[0].y = number->y;
            endPrintExtFunc(number->work, 2, &number->separators[0]);
        }
        break;
    case 2:
        /* This format interprets the stored word as three byte components. */
        __builtin_memcpy(byteValue, &number->value.integer, sizeof byteValue);
        for (component = 0; component < 3; component++) {
            byteX = number->x;
            componentWork = number->work;
            byteX += component * 32;
            number->render[component].x = byteX;
            {
                unsigned short y = number->y;
                number->render[component].work = componentWork;
                number->render[component].y = y;
            }
            number->render[component].payload.number.red = number->color[0];
            number->render[component].payload.number.green = number->color[1];
            number->render[component].payload.number.blue = number->color[2];
            number->render[component].payload.number.alpha = number->color[3];
            componentValue = byteValue[3 - component];
            number->render[component].argument1 = 2;
            number->render[component].payload.number.value = componentValue;
            number->render[component].argument2 = 0;
            endPrintExtFunc(number->work, 10, &number->render[component]);
        }
        for (part = 0; part < 2; part++) {
            separatorSprite = &number->separators[part];
            number->separators[part].spriteId = 0x704;
            endSpriteSet(separatorSprite, 255);
            number->separators[part].x = number->x + part * 32 + 22;
            number->separators[part].work = number->work;
            number->separators[part].y = number->y + 2;
            number->separators[part].color[0] = number->color[0];
            number->separators[part].color[1] = number->color[1];
            number->separators[part].color[2] = number->color[2];
            number->separators[part].color[3] = number->color[3];
            endPrintExtFunc(number->work, 2, separatorSprite);
        }
        break;
    case 11:
        for (blankComponent = 0; blankComponent < 3; blankComponent++) {
            componentX = number->x;
            blankWork = number->work;
            componentX += blankComponent * 32;
            number->render[blankComponent].x = componentX;
            {
                unsigned short y = number->y;
                number->render[blankComponent].work = blankWork;
                number->render[blankComponent].y = y;
            }
            blankValue = -1;
            number->render[blankComponent].payload.number.red = number->color[0];
            number->render[blankComponent].payload.number.green = number->color[1];
            number->render[blankComponent].payload.number.blue = number->color[2];
            number->render[blankComponent].payload.number.alpha = number->color[3];
            number->render[blankComponent].argument1 = 2;
            number->render[blankComponent].payload.number.value = blankValue;
            number->render[blankComponent].argument2 = 0;
            endPrintExtFunc(number->work, 10, &number->render[blankComponent]);
        }
        for (part = 0; part < 2; part++) {
            separatorSprite = &number->separators[part];
            number->separators[part].spriteId = 0x704;
            endSpriteSet(separatorSprite, 255);
            number->separators[part].x = number->x + part * 32 + 22;
            number->separators[part].work = number->work;
            number->separators[part].y = number->y + 2;
            number->separators[part].color[0] = number->color[0];
            number->separators[part].color[1] = number->color[1];
            number->separators[part].color[2] = number->color[2];
            number->separators[part].color[3] = number->color[3];
            endPrintExtFunc(number->work, 2, separatorSprite);
        }
        break;
    case 3: case 4: case 5: case 6: case 12: case 13:
        /* Paired formats interpret the stored word as two signed values. */
        __builtin_memcpy(pairValue, &number->value.integer, sizeof pairValue);
        colors[0] = number->colorMode[0];
        colors[1] = number->colorMode[1];
        switch (number->format - 3) {
        case 0: case 9: number->separators[0].spriteId = 0x702; break;
        case 1: case 10: number->separators[0].spriteId = 0x703; break;
        case 2: case 3: number->separators[0].spriteId = 0x118; break;
        }
        endSpriteSet(&number->separators[0], 255);
        spacing = number->separators[0].width;
        if (number->firstOnly == 0) {
            number->separators[0].x = number->x;
            if (number->separators[0].spriteId == 0x118)
                number->separators[0].y = number->y + 3;
            else
                number->separators[0].y = number->y;
            number->separators[0].work = number->work;
            number->separators[0].color[0] = number->separators[0].color[1] =
                number->separators[0].color[2] = number->separators[0].color[3] = 128;
            endPrintExtFunc(number->work, 2, &number->separators[0]);
        }
        if ((unsigned char)(number->format - 12) < 2) {
            int maximum = pairValue[1];
            int current = pairValue[0];
            if (current < maximum / 5)
                colors[0] = 3;
        }
        for (part = 0; part < 2; part++) {
            if (part == 1 && number->firstOnly != 0)
                break;
            number->render[part].work = number->work;
            partX = number->x + part * spacing;
            number->render[part].x = partX;
            number->render[part].y = number->y;
            switch (colors[part]) {
            case 0:
                number->render[part].payload.number.red = number->color[0];
                number->render[part].payload.number.green = number->color[1];
                number->render[part].payload.number.blue = number->color[2];
                number->render[part].payload.number.alpha = number->color[3];
                break;
            case 1:
                number->render[part].payload.number.red = tintHigh;
                number->render[part].payload.number.blue = number->render[part].payload.number.green = tintLow;
                number->render[part].payload.number.alpha = opacity;
                break;
            case 2:
                number->render[part].payload.number.green = number->render[part].payload.number.red = tintLow;
                number->render[part].payload.number.blue = tintHigh;
                number->render[part].payload.number.alpha = opacity;
                break;
            case 3:
                number->render[part].payload.number.red = tintHigh;
                number->render[part].payload.number.blue = number->render[part].payload.number.green = alertLow;
                number->render[part].payload.number.alpha = opacity;
                break;
            }
            number->render[part].payload.number.value = pairValue[part];
            number->render[part].argument1 = number->digits;
            number->render[part].argument2 = number->alignment + (1 - part) * 16;
            endPrintExtFunc(number->work, 10, &number->render[part]);
        }
        break;
    case 7: case 8:
        signX = (short)number->x;
        if (number->value.integer < 0)
            number->separators[0].spriteId = 0x706;
        else
            number->separators[0].spriteId = 0x705;
        endSpriteSet(&number->separators[0], 255);
        number->separators[0].x = signX;
        number->separators[0].y = number->y - number->format * 2 + 15;
        number->separators[0].color[0] = number->color[0];
        number->separators[0].color[1] = number->color[1];
        number->separators[0].color[2] = number->color[2];
        number->separators[0].color[3] = number->color[3];
        number->separators[0].work = number->work;
        endPrintExtFunc(number->work, 2, &number->separators[0]);
        signX += number->separators[0].width;
        magnitude = number->value.integer;
        if (magnitude < 0)
            magnitude = -magnitude;
        number->render[0].x = signX;
        number->render[0].y = number->y;
        number->render[0].payload.number.red = number->color[0];
        number->render[0].payload.number.green = number->color[1];
        number->render[0].payload.number.blue = number->color[2];
        number->render[0].payload.number.alpha = number->color[3];
        number->render[0].payload.number.value = magnitude;
        number->render[0].argument1 = number->digits;
        number->render[0].argument2 = number->format - 7;
        number->render[0].work = number->work;
        endPrintExtFunc(number->work, 10, &number->render[0]);
        break;
    case 9:
        dashX = (short)number->x;
        sprite = &number->render[0];
        for (dash = 0; dash < number->digits; dash++, sprite++) {
            sprite->argument1 = 0x706;
            endSpriteSet(sprite, 255);
            sprite->x = dashX;
            sprite->work = number->work;
            sprite->y = (short)number->y - 2;
            endPrintExtFunc(number->work, 2, sprite);
            dashX += sprite->payload.sprite.width;
        }
        break;
    case 10:
        text = &number->render[0];
        text->x = number->x;
        text->work = number->work;
        text->y = number->y;
        text->payload.text.color[0] = number->color[0];
        text->payload.text.color[1] = number->color[1];
        text->payload.text.color[2] = number->color[2];
        text->payload.text.color[3] = number->color[3];
        E_NUMBER_STORE_STRING(&text->payload.text.text, number->value.text);
        endPrintExtFunc(number->work, 7, text);
        break;
    }
}

void eNumberNumberChange(void)
{
}

/*
 * Partial view of the object eNumberSet initialises. eNumberMain (still
 * INCLUDE_ASM in this TU) copies the four bytes at +0x08..+0x0b verbatim into
 * the same +0x20..+0x23 render colour slot that eSpriteMain fills from its
 * own colour quad (and again into +0x74..+0x77), so they are a colour quad
 * set to 0x80 per channel, not digits; the number itself is the word at
 * +0x14 that eNumberMain formats in base 10. It runs only when the low
 * nibble of mode is 3 and reads flag[0] to pick a display format; the other
 * unmodeled bytes are untouched by eNumberSet.
 */
typedef struct ENumber {
    u8 unmodeled_000[8];    /* +0x00..0x07 */
    signed char color[4];   /* +0x08..0x0b */
    signed char mode;       /* +0x0c */
    u8 unmodeled_00d[3];    /* +0x0d..0x0f */
    signed char flag[4];    /* +0x10..0x13 */
} ENumber;

void eNumberSet(ENumber *number)
{
    number->color[0] = number->color[1] = number->color[2] = number->color[3] = -128;
    number->mode = 3;
    number->flag[0] = number->flag[1] = number->flag[2] = number->flag[3] = 0;
}

/* The first 16 bytes are the fields initialized by eSpriteSet. The following
 * coordinates, work word, and color quad form the descriptor submitted by
 * eSpriteMain. */
typedef struct ESpriteMainData {
    signed char state;
    u8 unmodeled_001[3];
    short x;
    short y;
    int work;
    signed char color[4];
    u8 unmodeled_010[4];
    short renderX;
    short renderY;
    int renderWork;
    u8 unmodeled_01c[4];
    signed char renderColor[4];
} ESpriteMainData;

extern void endSpriteSet(void *sprite, int mode);
extern void endPrintExtFunc(int work, int mode, void *descriptor);

void eSpriteMain(ESpriteMainData *sprite)
{
    endSpriteSet(&sprite->renderX, 5);
    sprite->renderX = sprite->x;
    sprite->renderWork = sprite->work;
    sprite->renderY = sprite->y;
    sprite->renderColor[0] = sprite->color[0];
    sprite->renderColor[1] = sprite->color[1];
    sprite->renderColor[2] = sprite->color[2];
    sprite->renderColor[3] = sprite->color[3];
    endPrintExtFunc(sprite->work, 2, &sprite->renderX);
}

/*
 * Partial view of the object eSpriteSet initialises. eSpriteMain copies x,
 * y and work verbatim into its own render fields, and copies mode together
 * with the three following bytes as one contiguous quad into a render
 * buffer, so they are modelled here as one 4-byte color quad rather than a
 * scalar mode plus three unrelated bytes; eTagFontMain evidences the same
 * grouping at the same offsets.
 */
typedef struct ESprite {
    signed char state;      /* +0x00 */
    u8 unmodeled_001[3];    /* +0x01..0x03 */
    short x;                 /* +0x04 */
    short y;                 /* +0x06 */
    int work;                /* +0x08 */
    signed char color[4];    /* +0x0c..0x0f */
    u8 unmodeled_010[0x14]; /* +0x10..0x23 */
    short spriteId;          /* +0x24 */
} ESprite;

void eSpriteSet(ESprite *sprite, short spriteId)
{
    sprite->x = 0;
    sprite->y = 0;
    sprite->work = 0;
    sprite->color[3] = -128;
    sprite->color[2] = -128;
    sprite->color[1] = -128;
    sprite->state = 0;
    sprite->color[0] = -128;
    sprite->spriteId = spriteId;
}

typedef struct ERibbonPart {
    short x;
    short y;
    int work;
    signed char color[4];
} ERibbonPart;

typedef struct ERibbon {
    short x;
    short y;
    int work;
    short w;
    short h;
    u8 mode;
    u8 unk0d;
    u8 unk0e;
    u8 type;
    ERibbonPart part[6];
    u8 unmodeled_058[4];
    int unk5c;
} ERibbon;

void eRibbonMain(ERibbon *r)
{
    u16 x;
    int v;
    u16 y, w, px;
    int yh, xw, half;
    int work;
    u8 d;

    switch (r->mode) {
    case 0x20:
        x = r->x;
        r->part[0].x = x;
        r->part[1].x = x;
        r->part[0].y = r->y;
        r->part[2].y = r->y;
        r->part[1].y = r->y + r->h;
        r->part[0].work = r->work;
        r->part[2].work = r->work;
        r->part[3].y = r->y + r->h;
        r->part[3].work = r->work;
        r->unk0e = 1;
        r->mode = 0x21;
        r->unk0d = 0;
        goto anim;
    case 0x21:
        x = r->x;
    anim:
        d = r->unk0d;
        r->unk0d = d + 1;
        v = (short)x + r->w * (d * 0.125f);
        r->part[2].x = v;
        r->part[3].x = v;
        if ((u8)(d + 1) >= 9) {
            r->mode = 0xF0;
        }
        break;
    case 0xF:
    case 0xF0:
    case 0xFF:
        break;
    case 0x2E:
        r->mode = 0x2F;
    case 0:
    case 0x2F:
        r->mode = 0x50;
        break;
    case 0x50:
        r->mode = 0x51;
        r->unk0e = 1;
    case 0x51:
        if (r->type == 0) {
            r->part[0].x = r->x;
            r->part[3].x = r->x + r->w / 2;
            r->part[5].x = r->x + r->w;
            r->part[4].x = r->x + r->w;
            r->part[1].x = r->x;
            r->part[2].x = r->x + r->w / 2;
        } else {
            r->part[1].x = r->x;
            r->part[0].x = r->x;
            half = r->w / 10;
            r->part[3].x = r->x + half;
            r->part[5].x = r->x + r->w;
            r->part[4].x = r->x + r->w;
            r->part[2].x = r->x + half;
        }
        r->part[2].y = r->y;
        r->part[0].y = r->y;
        r->part[5].y = r->y + r->h;
        r->part[3].y = r->y + r->h;
        r->part[5].work = r->work;
        r->part[4].work = r->work;
        r->part[3].work = r->work;
        r->part[2].work = r->work;
        r->part[1].work = r->work;
        r->part[4].y = r->y;
        r->part[1].y = r->y + r->h;
        r->part[0].work = r->work;
        r->unk5c = 0;
        break;
    }
    if (r->unk0e != 0) {
        endPrintExtFunc(r->part[0].work, 3, r->part);
    }
}

/*
 * Partial view of the object eRibbonModeChange writes, limited to the mode
 * field it evidences.
 */
typedef struct ERibbonEntry {
    u8 unmodeled_000[0xc]; /* +0x00..0x0b */
    signed char mode;      /* +0x0c */
} ERibbonEntry;

void eRibbonModeChange(ERibbonEntry *entry, signed char mode)
{
    entry->mode = mode;
}

void eRibbonSet(ERibbon *r, int type)
{
    int i;
    ERibbonPart *p;

    r->type = type;
    r->mode = 0x50;
    r->x = 0;
    r->y = 0;
    r->work = 0;
    r->unk0d = 0;
    r->unk0e = 0;
    switch (r->type) {
    case 3:
        p = r->part;
        for (i = 0; i < 6; i++, p++) {
            if (i & 1) {
                p->color[0] = 0;
                p->color[1] = 0x20;
                p->color[2] = 0x34;
            } else {
                p->color[0] = 0x60;
                p->color[1] = 0x40;
                p->color[2] = -128;
            }
            p->color[3] = 0x30;
        }
        break;
    case 0:
        r->part[3].color[2] = 0;
        r->part[2].color[2] = 0;
        r->part[1].color[2] = 0;
        r->part[0].color[2] = 0;
        r->part[3].color[1] = 0;
        r->part[2].color[1] = 0;
        r->part[1].color[1] = 0;
        r->part[0].color[1] = 0;
        r->part[3].color[0] = 0;
        r->part[2].color[0] = 0;
        r->part[1].color[0] = 0;
        r->part[0].color[0] = 0;
        r->part[3].color[3] = -128;
        r->part[2].color[3] = -128;
        r->part[1].color[3] = -128;
        r->part[0].color[3] = -128;
        break;
    case 1:
    case 2:
        if (type == 1) {
            r->part[0].color[3] = 0;
            r->part[1].color[3] = 0;
            r->part[2].color[3] = -128;
            r->part[3].color[3] = -128;
            r->part[4].color[3] = 0;
            r->part[5].color[3] = 0;
        } else {
            r->part[3].color[3] = 0x40;
            r->part[2].color[3] = 0x70;
            r->part[0].color[3] = 0x78;
            r->part[1].color[3] = 0;
        }
        r->part[3].color[0] = 0x20;
        r->part[2].color[0] = 0x20;
        r->part[1].color[0] = 0x20;
        r->part[0].color[0] = 0x20;
        r->part[5].color[0] = 0;
        r->part[4].color[0] = 0;
        r->part[3].color[1] = 0x34;
        r->part[2].color[1] = 0x34;
        r->part[1].color[1] = 0x34;
        r->part[0].color[1] = 0x34;
        r->part[5].color[1] = 0x20;
        r->part[4].color[1] = 0x20;
        r->part[3].color[2] = 0x34;
        r->part[2].color[2] = 0x34;
        r->part[1].color[2] = 0x34;
        r->part[0].color[2] = 0x34;
        r->part[5].color[2] = 0x34;
        r->part[4].color[2] = 0x34;
        r->part[0].color[3] = 0;
        r->part[1].color[3] = 0;
        r->part[2].color[3] = -128;
        r->part[3].color[3] = -128;
        r->part[4].color[3] = -128;
        r->part[5].color[3] = -128;
        break;
    }
}

typedef struct ERibbonPoint {
    short x;
    short y;
    int work;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
} ERibbonPoint;
typedef struct ERibbonSetData {
    short x;
    short y;
    int work;
    u8 unmodeled_008[4];
    signed char mode;
    unsigned char progress;
    unsigned char visible;
    unsigned char style;
    ERibbonPoint points[6];
} ERibbonSetData;
typedef struct ELinePoint {
    int x;
    int y;
    int work;
    signed char red;
    signed char green;
    signed char blue;
    signed char alpha;
} ELinePoint;
typedef struct ELineEntry {
    u8 state;
    u8 unmodeled_001[2];
    u8 duration;
    u8 progress[5];
    u8 unmodeled_009[3];
    ELinePoint points[5];
    unsigned int count;
    ELinePoint render[5];
} ELineEntry;
typedef struct ELineSetData {
    u8 state;
    u8 unmodeled_001[2];
    u8 duration;
    u8 progress[5];
    u8 unmodeled_009[3];
    ELinePoint points[5];
} ELineSetData;
typedef struct ERibbonRenderPoint {
    short x;
    short y;
    int work;
    u8 unmodeled_008[4];
} ERibbonRenderPoint;
typedef struct ERibbonMainData {
    short x;
    short y;
    int work;
    short width;
    short height;
    signed char mode;
    unsigned char progress;
    unsigned char visible;
    unsigned char style;
    ERibbonRenderPoint points[6];
    u8 unmodeled_058[4];
    int renderCount;
} ERibbonMainData;

void eLineMain(ELineEntry *line)
{
    int point;
    unsigned int segment;
    unsigned int reset;
    int remaining;
    float progress;
    int *sourceX;
    int *sourceY;
    int *sourceWork;
    ELinePoint *render;

    switch (line->state) {
    case 0:
        line->state = 0x50;
        break;
    case 1:
        break;
    case 0x10:
        for (reset = 0; reset < line->count; reset++)
            line->progress[reset] = 0;
        line->state = 0x11;
    case 0x11:
        for (segment = 0; segment < line->count; segment++) {
            line->progress[segment]++;
            if (line->progress[segment] >= line->duration)
                line->progress[segment] = line->duration;
            else
                break;
        }
        remaining = 0;
        for (segment = 0; segment < line->count; segment++)
            remaining += line->progress[segment] - line->duration;
        if (remaining == 0)
            line->state = 0x50;
        break;
    case 0x20:
        for (reset = 0; reset < line->count; reset++)
            line->progress[reset] = 0;
        line->state = 0x21;
    case 0x21:
        for (segment = 0; segment < line->count; segment++) {
            line->progress[segment]++;
            if (line->progress[segment] >= line->duration)
                line->progress[segment] = line->duration;
        }
        remaining = 0;
        for (segment = 0; segment < line->count; segment++)
            remaining += line->progress[segment] - line->duration;
        if (remaining == 0)
            line->state = 0x50;
        break;
    case 0x50:
        line->state = 0x51;
        break;
    }
    sourceX = &line->points[0].x;
    sourceY = &line->points[0].y;
    sourceWork = &line->points[0].work;
    render = line->render;
    render->x = *sourceX;
    line->render[0].y = *sourceY;
    line->render[0].work = *sourceWork;
    render->red = line->points[0].red;
    render->green = line->points[0].green;
    render->blue = line->points[0].blue;
    render->alpha = line->points[0].alpha;
    for (point = 1; point < line->count; point++) {
        progress = (float)line->progress[point - 1] / (float)line->duration;
        line->render[point].x = (int)((float)(line->points[point].x - line->render[point - 1].x) * progress + (float)line->render[point - 1].x);
        line->render[point].y = (int)((float)(line->points[point].y - line->render[point - 1].y) * progress + (float)line->render[point - 1].y);
        line->render[point].work = line->points[point].work;
        line->render[point].red = line->points[point].red;
        line->render[point].green = line->points[point].green;
        line->render[point].blue = line->points[point].blue;
        line->render[point].alpha = line->points[point].alpha;
    }
}

void eLineSet(unsigned char *p)
{
    int i;
    unsigned char *q;

    p[3] = 7;
    i = 0;
    p[0] = 0;
    q = p + 4;
    for (; i < 5; i++) {
        q[i] = p[3];
        p[i * 16 + 0x1A] = 0xFF;
        p[i * 16 + 0x19] = 0xFF;
        p[i * 16 + 0x18] = 0xFF;
        p[i * 16 + 0x1B] = 0x80;
    }
}

/* The source color quad is copied into the descriptor with its two middle
 * channels exchanged; the value word controls whether the descriptor is sent. */
typedef struct ETagFontMainData {
    u8 unmodeled_000[4];
    short x;
    short y;
    int work;
    signed char color[4];
    struct {
        short x;
        short y;
        int work;
        signed char color[4];
    } render;
    int value;
} ETagFontMainData;

void eTagFontMain(ETagFontMainData *tag)
{
    if (tag->value != 0) {
        tag->render.x = tag->x;
        tag->render.y = tag->y;
        tag->render.color[0] = tag->color[0];
        tag->render.color[1] = tag->color[2];
        tag->render.color[2] = tag->color[1];
        tag->render.color[3] = tag->color[3];
        tag->render.work = tag->work;
        endPrintExtFunc(tag->work, 7, &tag->render);
    }
}

/*
 * Partial view of the object eTagFontSet initialises. eTagFontMain copies
 * the same 4-byte color quad at +0xc..+0xf into its own render buffer that
 * eSpriteMain copies from its object (same offsets, same grouping).
 */
typedef struct ETagFont {
    signed char state;      /* +0x00 */
    signed char subState;   /* +0x01 */
    u8 unmodeled_002[10];   /* +0x02..0x0b */
    signed char color[4];    /* +0x0c..0x0f */
    u8 unmodeled_010[0xc];  /* +0x10..0x1b */
    int value;               /* +0x1c */
} ETagFont;

void eTagFontSet(ETagFont *tag, int value)
{
    tag->state = 0;
    tag->subState = 0;
    tag->color[3] = -128;
    tag->color[1] = -128;
    tag->color[2] = -128;
    tag->color[0] = -128;
    tag->value = value;
}

#include "common.h"
#include "shared.h"

INCLUDE_ASM("asm/main/nonmatchings/e_number_main", eNumberMain);

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

INCLUDE_ASM("asm/main/nonmatchings/e_number_main", eLineMain);

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

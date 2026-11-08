#include "common.h"
#include "shared.h"

/* DispPillar's two angle steps: the same value twice in the original pool
   (a C literal would be merged into one), so they stay scaffold-owned */
extern float D_004D7F88;
extern float D_004D7F8C;
extern float xglSin(float angle);
extern float xglCos(float angle);

extern struct { char pad[0x2A]; unsigned short trg; unsigned short btn; } PadData;

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

static int debmode = 0;



extern void DebDispEvtItem(void);
extern void DebDispItemBox(void);
extern void UnlockMapUnit(void);
extern struct { char pad[0x20]; int flags; } GameLoopState;
extern char lineflg, dispflg, printflg, collflg;

void PauseMenuPageUwamono(void) {
    static int cursol_0 __asm__("cursol.0") = 0;
    int c;

    if (debmode == 1) {
        DebDispEvtItem();
        return;
    }
    if (debmode == 2) {
        DebDispItemBox();
        return;
    }
    xglFontDebugPrintf(0x10, 0x10, "\276\345\312\252\315\321\245\307\245\320\245\303\245\260\245\341\245\313\245\345\241\274");
    xglFontDebugPrintf(0x30, cursol_0 * 0x10 + 0x20, "\241\344");
    if (lineflg) xglFontDebugPrintf(0x40, 0x20, "\274\315\267\342\265\260\300\327\311\275\274\250\241\241\241\241\245\252\245\363");
    else xglFontDebugPrintf(0x40, 0x20, "\274\315\267\342\265\260\300\327\311\275\274\250\241\241\241\241\245\252\245\325");
    if (dispflg) xglFontDebugPrintf(0x40, 0x30, "\276\345\312\252\276\360\312\363\311\275\274\250\241\241\241\241\245\252\245\363");
    else xglFontDebugPrintf(0x40, 0x30, "\276\345\312\252\276\360\312\363\311\275\274\250\241\241\241\241\245\252\245\325");
    if (printflg) xglFontDebugPrintf(0x40, 0x40, "\276\345\312\252\276\360\312\363\275\320\316\317\241\241\241\241\245\252\245\363");
    else xglFontDebugPrintf(0x40, 0x40, "\276\345\312\252\276\360\312\363\275\320\316\317\241\241\241\241\245\252\245\325");
    if (collflg) xglFontDebugPrintf(0x40, 0x50, "\245\263\245\352\245\270\245\347\245\363\311\275\274\250\241\241\245\252\245\363");
    else xglFontDebugPrintf(0x40, 0x50, "\245\263\245\352\245\270\245\347\245\363\311\275\274\250\241\241\245\252\245\325");
    if (GameLoopState.flags & 1) xglFontDebugPrintf(0x40, 0x60, "\274\315\267\342\245\325\245\351\245\260\241\241\241\241\241\241\245\252\245\363");
    else xglFontDebugPrintf(0x40, 0x60, "\274\315\267\342\245\325\245\351\245\260\241\241\241\241\241\241\245\252\245\325");
    xglFontDebugPrintf(0x40, 0x70, "\245\244\245\331\245\363\245\310\245\242\245\244\245\306\245\340\260\354\315\367");
    xglFontDebugPrintf(0x40, 0x80, "\312\365\310\242\303\346\277\310\260\354\315\367");
    if (PadData.trg & 0x1000) {
        cursol_0--;
        if (cursol_0 < 0) cursol_0 = 6;
    }
    if (PadData.trg & 0x4000) {
        cursol_0++;
        cursol_0 = cursol_0 % 7;
    }
    c = cursol_0;
    if (c == 5 && (PadData.trg & 0x20)) debmode = 1;
    if (c == 6 && (PadData.trg & 0x20)) debmode = 2;
    if (PadData.trg & 0x2000) {
        switch (c) {
        case 0: lineflg = 1; break;
        case 1: dispflg = 1; break;
        case 2: printflg = 1; break;
        case 3: collflg = 1; break;
        case 4: GameLoopState.flags |= 1; break;
        }
    }
    if (PadData.trg & 0x8000) {
        switch (c) {
        case 0: lineflg = 0; break;
        case 1: dispflg = 0; break;
        case 2: printflg = 0; break;
        case 3: collflg = 0; break;
        case 4:
            UnlockMapUnit();
            GameLoopState.flags &= ~1;
            break;
        }
    }
}

extern char EvtItemTbl[][0x80];

void DebDispEvtItem(void)
{
    static int page_1 __asm__("page.1") = 0;
    int i;
    int n;
    int y1, y2;
    char *p;

    xglFontDebugPrintf(0x10, 0x10, "\245\244\245\331\245\363\245\310\245\242\245\244\245\306\245\340\260\354\315\367");
    xglFontDebugPrintf(0x10, 0x20, "\241\337\245\334\245\277\245\363\244\307\314\341\244\353");
    for (i = 0; i < 4; i++) {
        n = i + page_1 * 4;
        if (n >= 0xFF) break;
        p = EvtItemTbl[n];
        xglFontDebugPrintf(4, i * 0x22 + 0x3C, "%3d %s %c", n + 1, p, *p);
        while (*p++ != 0) {
        }
        xglFontDebugPrintf(0x10, i * 0x22 + 0x48, "%s", p);
    }
    if (PadData.btn & 0x1000) {
        if (--page_1 < 0) {
            page_1 = 0x3F;
        }
    }
    if (PadData.btn & 0x4000) {
        page_1 = (page_1 + 1) % 64;
    }
    if (PadData.trg & 0x40) {
        debmode = 0;
    }
}

extern void HexToStr(int v, char *buf);
extern char *GetItemName(int kind, int id);
extern int xglFlagsGet1(int bit_offset);
extern void *memset(void *, int, unsigned int);
typedef struct { signed char kind; unsigned char id; signed char c; signed char d; int e; } ItemBoxEnt;
extern ItemBoxEnt ItemBoxTbl[];

void DebDispItemBox(void)
{
    static int page_2 __asm__("page.2") = 0;
    char buf1[16];
    char buf2[16];
    int i;
    int n;
    int e;
    int c;
    int d;
    char *name;
    ItemBoxEnt *p;

    xglFontDebugPrintf(0x10, 0x10, "\312\365\310\242\303\346\277\310\260\354\315\367");
    xglFontDebugPrintf(0x10, 0x20, "\241\337\245\334\245\277\245\363\244\307\314\341\244\353");
    for (i = 0; i < 8; i++) {
        memset(buf1, 0, 0x10);
        memset(buf2, 0, 0x10);
        n = i + page_2 * 8;
        if (n >= 0x259) break;
        p = &ItemBoxTbl[n];
        e = ((int *)&ItemBoxTbl[n])[1];
        c = p->c;
        d = p->d;
        name = GetItemName(p->kind, p->id);
        if (name == 0) {
            name = "NULL";
        }
        if (e == 0) {
            HexToStr(c, buf1);
            xglFontDebugPrintf(4, i * 0x10 + 0x3C, "%3d %s", n, name);
            xglFontDebugPrintf(0x80, i * 0x10 + 0x3C, "%s\270\304", buf1);
        } else {
            HexToStr(e, buf1);
            xglFontDebugPrintf(4, i * 0x10 + 0x3C, "%3d \244\252\266\342 %s\243\307", n, buf1);
        }
        if (xglFlagsGet1(n + 0x79EC7) == 1) {
            xglFontDebugPrintf(0x98, i * 0x10 + 0x3C, "\274\350\306\300\272\321");
        }
        if (d != 0) {
            HexToStr(d, buf2);
            xglFontDebugPrintf(0xBA, i * 0x10 + 0x3C, "\263\316\316\250%s", buf2);
        }
    }
    if (PadData.btn & 0x1000) {
        if (--page_2 < 0) {
            page_2 = 0x4B;
        }
    }
    if (PadData.btn & 0x4000) {
        page_2 = (page_2 + 1) % 76;
    }
    if (PadData.trg & 0x40) {
        debmode = 0;
    }
}

typedef union UwamonoVector {
    Vector4 vector;
    u64 words[2];
} UwamonoVector;

typedef struct UwamonoInfo {
    short kind;                      /* +0x00 */
    short item;                      /* +0x02 */
    unsigned char unmodeled_04[0x06];
    signed char bounds_shown;        /* +0x0A */
    unsigned char unmodeled_0b;
    unsigned char unmodeled_0c[0x21];
    signed char shape;               /* +0x2D */
    unsigned char unmodeled_2e[0x16];
    int state_flags;                 /* +0x44 */
    int sound;                       /* +0x48 */
} UwamonoInfo;

typedef struct UwamonoDebugUnit {
    unsigned int flags;              /* +0x00 */
    unsigned char unmodeled_04[0x0C];
    UwamonoVector position;          /* +0x10 */
    unsigned char unmodeled_20[0x60];
    float (*matrix)[4];              /* +0x80 */
    unsigned char unmodeled_84[0x04];
    unsigned char unmodeled_88[0x1C];
    short serial;                    /* +0xA4 */
    unsigned char unmodeled_a6[0x0A];
    UwamonoVector size;              /* +0xB0 */
    unsigned char unmodeled_c0[0xE0];
    UwamonoInfo info;                /* +0x1A0 */
    unsigned char unmodeled_1ec[0x114];
} UwamonoDebugUnit;

extern UwamonoDebugUnit MapUnit[64];

extern float xglRotTransPers(int *result, float matrix[4][4], void *position, int camera);

void DispDebugUwamono(void)
{
    int screen[3];
    Vector4 offset;
    int i;

    offset.x = 0.0f;
    offset.y = 2.0f;
    offset.w = 1.0f;
    offset.z = 0.0f;
    for (i = 0; i < 64; i++) {
        UwamonoDebugUnit *unit = &MapUnit[i];
        UwamonoInfo *info = &unit->info;
        int screen_col;
        int x;
        int y;
        int money;
        int count;
        int category;
        int index;

        if (!(unit->flags & 0x10000))
            continue;
        if (unit->matrix == 0)
            continue;
        if (unit->serial == -1)
            continue;
        xglRotTransPers(screen, unit->matrix, &offset, 0);
        if (screen[0] <= 0x7000 || screen[0] >= 0x9000)
            continue;
        if (screen[1] < 0x6E01)
            continue;
        if (screen[1] > 0x8DFF)
            continue;
        if (screen[2] <= 0)
            continue;
        screen_col = ((screen[0] >> 4) - 0x700) >> 1;
        y = ((screen[1] >> 4) - 0x720) >> 1;
        y += 32;
        if (unit->serial < 0x1000) {
            x = screen_col - 32;
            xglFontDebugPrintf(x, y, "\015\000\245\321\241\274\245\304%4d", unit->serial);
        } else {
            x = screen_col - 32;
            xglFontDebugPrintf(x, y, "\015\000\245\342\245\307\245\3530x%4x", unit->serial);
        }
        switch (info->kind) {
        case 1:
            xglFontDebugPrintf(x, y + 12, "\015\000\245\311\245\242");
            break;
        case 2:
            xglFontDebugPrintf(x, y + 12, "\015\000\262\365\244\354\312\252");
            if (info->state_flags & 1)
                xglFontDebugPrintf(x, y + 24, "\015\000\274\253\262\365 \241\373");
            else
                xglFontDebugPrintf(x, y + 24, "\015\000\274\253\262\365 \241\337");
            if (info->state_flags & 2)
                xglFontDebugPrintf(x, y + 36, "\015\000\305\250\262\365 \241\373");
            else
                xglFontDebugPrintf(x, y + 36, "\015\000\305\250\262\365 \241\337");
            break;
        case 3:
            if (unit->serial == 0x7000) {
                xglFontDebugPrintf(x, y + 12, "\015\000\245\263\245\352\245\270\245\347\245\363");
            } else if (unit->serial == 0x7012) {
                xglFontDebugPrintf(x, y + 12, "\015\000\262\273 0x%8x", info->sound);
                break;
            } else {
                xglFontDebugPrintf(x, y + 12, "\015\000\245\310\245\351\245\303\245\327");
            }
            if (info->state_flags & 1)
                xglFontDebugPrintf(x, y + 24, "\015\000\274\253\262\365 \241\373");
            else
                xglFontDebugPrintf(x, y + 24, "\015\000\274\253\262\365 \241\337");
            if (info->state_flags & 2)
                xglFontDebugPrintf(x, y + 36, "\015\000\305\250\262\365 \241\373");
            else
                xglFontDebugPrintf(x, y + 36, "\015\000\305\250\262\365 \241\337");
            break;
        case 4:
            xglFontDebugPrintf(x, y + 12, "\015\000\312\365\310\242");
            {
                int item = info->item;

                money = ItemBoxTbl[item].e;
                count = ItemBoxTbl[item].c;
                index = ItemBoxTbl[item].id;
                category = ItemBoxTbl[item].kind;
            }
            if (money != 0)
                xglFontDebugPrintf(x, y + 24, "\015\000\244\252\266\342 %6d G", money);
            else
                xglFontDebugPrintf(x, y + 24, "\015\000%s %3d\270\304",
                                   GetItemName(category, index), count);
            break;
        case 5:
            xglFontDebugPrintf(x, y + 12, "\015\000\245\242\245\244\245\306\245\340");
            {
                int item = info->item;

                money = ItemBoxTbl[item].e;
                count = ItemBoxTbl[item].c;
                index = ItemBoxTbl[item].id;
                category = ItemBoxTbl[item].kind;
            }
            if (money != 0)
                xglFontDebugPrintf(x, y + 24, "\015\000\244\252\266\342 %6dG", money);
            else
                xglFontDebugPrintf(x, y + 24, "\015\000%s %3d\270\304",
                                   GetItemName(category, index), count);
            break;
        case 6:
            xglFontDebugPrintf(x, y + 12, "\015\000\245\273\241\274\245\326");
            break;
        case 7:
            xglFontDebugPrintf(x, y + 12, "\015\000\245\267\245\347\245\303\245\327");
            break;
        }
        if (unit->serial != 0x7012 && info->sound > 0)
            xglFontDebugPrintf(x, y + 48, "\015\000\262\273 0x%8x", info->sound);
    }
}


void DispCollUwamono(void)
{
    UwamonoVector bottom[4];
    UwamonoVector top[4];
    UwamonoVector position;
    UwamonoVector size;
    int i;
    int colors[8];
    float position_w;
    float corner_w;

    colors[0] = 0;
    colors[1] = 0;
    colors[2] = 255;
    colors[3] = 128;
    colors[4] = 255;
    colors[5] = 0;
    colors[6] = 0;
    colors[7] = 128;
    position_w = 0.3f;
    corner_w = 0.1f;
    for (i = 0; i < 64; i++) {
        UwamonoInfo *info = &MapUnit[i].info;
        UwamonoDebugUnit *unit;
        float (*rotation)[4];

        if (MapUnit[i].serial == -1)
            continue;
        if (MapUnit[i].flags & 0x100000)
            continue;
        if (MapUnit[i].serial == 0x7012) {
            unit = &MapUnit[i];
            position = unit->position;
            if (info->bounds_shown == 0) {
                position.vector.w = position_w;
            } else {
                rotation = unit->matrix;
                bottom[0].vector.x = position.vector.x - rotation[0][0];
                bottom[0].vector.y = position.vector.y - rotation[0][1];
                bottom[0].vector.z = position.vector.z - rotation[0][2];
                bottom[0].vector.w = 0.0f;
                bottom[1].vector.x = position.vector.x + rotation[0][0];
                bottom[1].vector.y = position.vector.y + rotation[0][1];
                bottom[1].vector.z = position.vector.z + rotation[0][2];
                bottom[1].vector.w = 0.0f;
            }
        } else if (info->shape != 0 && (MapUnit[i].flags & 0x10000)) {
            unit = &MapUnit[i];
            rotation = unit->matrix;
            position = unit->position;
            size = unit->size;
            if (info->shape == 2) {
                bottom[0].vector.x = position.vector.x + (size.vector.x * rotation[0][0] + size.vector.z * rotation[2][0]);
                bottom[0].vector.y = position.vector.y;
                bottom[0].vector.z = position.vector.z + (size.vector.x * rotation[0][2] + size.vector.z * rotation[2][2]);
                bottom[1].vector.x = position.vector.x - (size.vector.z * rotation[0][2] + size.vector.x * rotation[2][2]);
                bottom[1].vector.y = position.vector.y;
                bottom[1].vector.z = position.vector.z + (size.vector.z * rotation[0][0] + size.vector.x * rotation[2][0]);
                bottom[2].vector.x = position.vector.x - (size.vector.x * rotation[0][0] + size.vector.z * rotation[2][0]);
                bottom[2].vector.y = position.vector.y;
                bottom[2].vector.z = position.vector.z - (size.vector.x * rotation[0][2] + size.vector.z * rotation[2][2]);
                bottom[3].vector.x = position.vector.x + (size.vector.z * rotation[0][2] + size.vector.x * rotation[2][2]);
                bottom[3].vector.y = position.vector.y;
                bottom[3].vector.z = position.vector.z - (size.vector.z * rotation[0][0] + size.vector.x * rotation[2][0]);
                bottom[0].vector.w = corner_w;
                bottom[1].vector.w = corner_w;
                bottom[2].vector.w = corner_w;
                bottom[3].vector.w = corner_w;
                top[0] = bottom[0];
                top[1] = bottom[1];
                top[2] = bottom[2];
                top[3] = bottom[3];
                top[0].vector.y += size.vector.y;
                top[1].vector.y += size.vector.y;
                top[2].vector.y += size.vector.y;
                top[3].vector.y += size.vector.y;
            } else if (info->shape == 1) {
                DispPillar(&unit->position.vector, &unit->size.vector, colors);
            }
            if (info->sound > 0) {
                unit = &MapUnit[i];
                position = unit->position;
                position.vector.w = position_w;
            }
        }
    }
}

void DispPillar(Vector4 *base, Vector4 *step)
{
    union PillarVertex {
        Vector4 vector;
        u64 words[2];
    } corners[4];
    int segment = 0;
    float angle;
    float angle_increment;

    base->w = 1.0f;
    do {
        /* Compute four local corners for each angular segment. */
        corners[0].vector = *base;
        corners[1].vector = *base;
        angle_increment = D_004D7F88;
        angle = (float)segment * angle_increment;
        corners[0].vector.x += xglSin(angle) * step->x;
        angle_increment = D_004D7F8C;
        corners[0].vector.z += xglCos(angle) * step->x;
        angle = (float)(segment + 1) * angle_increment;
        corners[1].vector.x += xglSin(angle) * step->x;
        corners[1].vector.z += xglCos(angle) * step->x;
        corners[2] = corners[0];
        corners[3] = corners[1];
        corners[2].vector.y += step->y;
        corners[3].vector.y += step->y;
        segment++;
    } while (segment < 17);
}

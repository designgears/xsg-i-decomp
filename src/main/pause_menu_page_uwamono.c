#include "common.h"
#include "shared.h"

extern float D_004D7F88;
extern float D_004D7F8C;
extern float xglSin(float angle);
extern float xglCos(float angle);

extern struct { char pad[0x2A]; unsigned short trg; unsigned short btn; } PadData;

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

extern int debmode;

extern char D_004CA788[];

extern char D_004CA7A0[];

extern void DebDispEvtItem(void);
extern void DebDispItemBox(void);
extern void UnlockMapUnit(void);
extern struct { char pad[0x20]; int flags; } GameLoopState;
extern int cursol_0;
extern char lineflg, dispflg, printflg, collflg;
extern char D_004CA680[], D_004DB798[], D_004CA698[], D_004CA6B0[], D_004CA6C8[], D_004CA6E0[];
extern char D_004CA6F8[], D_004CA710[], D_004CA728[], D_004CA740[], D_004CA758[], D_004CA770[];

void PauseMenuPageUwamono(void) {
    int c;

    if (debmode == 1) {
        DebDispEvtItem();
        return;
    }
    if (debmode == 2) {
        DebDispItemBox();
        return;
    }
    xglFontDebugPrintf(0x10, 0x10, D_004CA680);
    xglFontDebugPrintf(0x30, cursol_0 * 0x10 + 0x20, D_004DB798);
    if (lineflg) xglFontDebugPrintf(0x40, 0x20, D_004CA698);
    else xglFontDebugPrintf(0x40, 0x20, D_004CA6B0);
    if (dispflg) xglFontDebugPrintf(0x40, 0x30, D_004CA6C8);
    else xglFontDebugPrintf(0x40, 0x30, D_004CA6E0);
    if (printflg) xglFontDebugPrintf(0x40, 0x40, D_004CA6F8);
    else xglFontDebugPrintf(0x40, 0x40, D_004CA710);
    if (collflg) xglFontDebugPrintf(0x40, 0x50, D_004CA728);
    else xglFontDebugPrintf(0x40, 0x50, D_004CA740);
    if (GameLoopState.flags & 1) xglFontDebugPrintf(0x40, 0x60, D_004CA758);
    else xglFontDebugPrintf(0x40, 0x60, D_004CA770);
    xglFontDebugPrintf(0x40, 0x70, D_004CA788);
    xglFontDebugPrintf(0x40, 0x80, D_004CA7A0);
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

extern char D_004CA7E8[];
extern char D_004CA7F8[];
extern char D_004DB7A0[];
extern char EvtItemTbl[][0x80];
extern int page_1;

void DebDispEvtItem(void)
{
    int i;
    int n;
    int y1, y2;
    char *p;

    xglFontDebugPrintf(0x10, 0x10, D_004CA788);
    xglFontDebugPrintf(0x10, 0x20, D_004CA7E8);
    for (i = 0; i < 4; i++) {
        n = i + page_1 * 4;
        if (n >= 0xFF) break;
        p = EvtItemTbl[n];
        xglFontDebugPrintf(4, i * 0x22 + 0x3C, D_004CA7F8, n + 1, p, *p);
        while (*p++ != 0) {
        }
        xglFontDebugPrintf(0x10, i * 0x22 + 0x48, D_004DB7A0, p);
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
extern int page_2;
extern char D_004CA808[];
extern char D_004DB7A8[];
extern char D_004DB7B0[];
extern char D_004DB7B8[];
extern char D_004DB7C0[];
extern char D_004DB7C8[];

void DebDispItemBox(void)
{
    char buf1[16];
    char buf2[16];
    int i;
    int n;
    int e;
    int c;
    int d;
    char *name;
    ItemBoxEnt *p;

    xglFontDebugPrintf(0x10, 0x10, D_004CA7A0);
    xglFontDebugPrintf(0x10, 0x20, D_004CA7E8);
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
            name = D_004DB7A8;
        }
        if (e == 0) {
            HexToStr(c, buf1);
            xglFontDebugPrintf(4, i * 0x10 + 0x3C, D_004DB7B0, n, name);
            xglFontDebugPrintf(0x80, i * 0x10 + 0x3C, D_004DB7B8, buf1);
        } else {
            HexToStr(e, buf1);
            xglFontDebugPrintf(4, i * 0x10 + 0x3C, D_004CA808, n, buf1);
        }
        if (xglFlagsGet1(n + 0x79EC7) == 1) {
            xglFontDebugPrintf(0x98, i * 0x10 + 0x3C, D_004DB7C0);
        }
        if (d != 0) {
            HexToStr(d, buf2);
            xglFontDebugPrintf(0xBA, i * 0x10 + 0x3C, D_004DB7C8, buf2);
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
extern char D_004CA818[];
extern char D_004CA828[];
extern char D_004DB7D0[];
extern char D_004DB7D8[];
extern char D_004CA838[];
extern char D_004CA848[];
extern char D_004CA858[];
extern char D_004CA868[];
extern char D_004CA878[];
extern char D_004CA888[];
extern char D_004CA898[];
extern char D_004CA8A8[];
extern char D_004CA8B8[];
extern char D_004CA8C8[];
extern char D_004CA8D8[];
extern char D_004CA8E8[];
extern char D_004CA8F8[];
extern char D_004CA908[];

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
            xglFontDebugPrintf(x, y, D_004CA818, unit->serial);
        } else {
            x = screen_col - 32;
            xglFontDebugPrintf(x, y, D_004CA828, unit->serial);
        }
        switch (info->kind) {
        case 1:
            xglFontDebugPrintf(x, y + 12, D_004DB7D0);
            break;
        case 2:
            xglFontDebugPrintf(x, y + 12, D_004CA838);
            if (info->state_flags & 1)
                xglFontDebugPrintf(x, y + 24, D_004CA848);
            else
                xglFontDebugPrintf(x, y + 24, D_004CA858);
            if (info->state_flags & 2)
                xglFontDebugPrintf(x, y + 36, D_004CA868);
            else
                xglFontDebugPrintf(x, y + 36, D_004CA878);
            break;
        case 3:
            if (unit->serial == 0x7000) {
                xglFontDebugPrintf(x, y + 12, D_004CA888);
            } else if (unit->serial == 0x7012) {
                xglFontDebugPrintf(x, y + 12, D_004CA898, info->sound);
                break;
            } else {
                xglFontDebugPrintf(x, y + 12, D_004CA8A8);
            }
            if (info->state_flags & 1)
                xglFontDebugPrintf(x, y + 24, D_004CA848);
            else
                xglFontDebugPrintf(x, y + 24, D_004CA858);
            if (info->state_flags & 2)
                xglFontDebugPrintf(x, y + 36, D_004CA868);
            else
                xglFontDebugPrintf(x, y + 36, D_004CA878);
            break;
        case 4:
            xglFontDebugPrintf(x, y + 12, D_004DB7D8);
            {
                int item = info->item;

                money = ItemBoxTbl[item].e;
                count = ItemBoxTbl[item].c;
                index = ItemBoxTbl[item].id;
                category = ItemBoxTbl[item].kind;
            }
            if (money != 0)
                xglFontDebugPrintf(x, y + 24, D_004CA8B8, money);
            else
                xglFontDebugPrintf(x, y + 24, D_004CA8C8,
                                   GetItemName(category, index), count);
            break;
        case 5:
            xglFontDebugPrintf(x, y + 12, D_004CA8D8);
            {
                int item = info->item;

                money = ItemBoxTbl[item].e;
                count = ItemBoxTbl[item].c;
                index = ItemBoxTbl[item].id;
                category = ItemBoxTbl[item].kind;
            }
            if (money != 0)
                xglFontDebugPrintf(x, y + 24, D_004CA8E8, money);
            else
                xglFontDebugPrintf(x, y + 24, D_004CA8C8,
                                   GetItemName(category, index), count);
            break;
        case 6:
            xglFontDebugPrintf(x, y + 12, D_004CA8F8);
            break;
        case 7:
            xglFontDebugPrintf(x, y + 12, D_004CA908);
            break;
        }
        if (unit->serial != 0x7012 && info->sound > 0)
            xglFontDebugPrintf(x, y + 48, D_004CA898, info->sound);
    }
}

extern float D_004D7F80;
extern float D_004D7F84;

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
    position_w = D_004D7F80;
    corner_w = D_004D7F84;
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

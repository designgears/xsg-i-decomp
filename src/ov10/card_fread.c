/*
 * OV10 original TU 2: 0x00a08670..0x00a12a90 (101 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov10/cgp.h"
#include "card_fread.h"

/* The card-model fade state in CardGameWork (+0x7100..+0x71A0, ov10/cgp.h):
 * four model slots, of whose four color channels the fades touch 0..2. */
#define CARD_MODEL_SLOT_COUNT 4
#define CARD_MODEL_COLOR_CHANNELS 3

extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
extern char *strcat(char *destination, const char *source);
extern char D_00A4D130[];

int CardFread(void *buffer, char *name)
{
    char path[256];
    *(long long *)&path[0] = *(long long *)&D_00A4D130[0];
    *(int *)&path[8] = *(int *)&D_00A4D130[8];
    *(short *)&path[12] = *(short *)&D_00A4D130[12];
    path[14] = D_00A4D130[14];
    strcat(path, name);
    return xglCdReadFile(path, buffer, 0, 0);
}

/*
 * Mirrors CardModelFadeIn's timer/step/color fields (same offsets in
 * CardGameWork, ov10/cgp.h) but subtracts modelFadeStep and clamps each
 * channel at zero instead of raising it toward modelColorTarget.
 */
void CardModelFadeOut(CardGameWork *work)
{
    int slot;
    int channel;
    float color;

    if (work->modelFadeInTimer == 0) {
        return;
    }
    work->modelFadeInTimer -= 1;
    for (slot = 0; slot < CARD_MODEL_SLOT_COUNT; slot++) {
        for (channel = 0; channel < CARD_MODEL_COLOR_CHANNELS; channel++) {
            color = work->modelColor[slot][channel] - work->modelFadeStep;
            work->modelColor[slot][channel] = color;
            if (color < 0.0f) {
                work->modelColor[slot][channel] = 0.0f;
            }
        }
    }
}

void CardModelFadeIn(CardGameWork *work)
{
    int slot;
    int channel;

    if (work->modelFadeInTimer == 0) {
        return;
    }
    work->modelFadeInTimer -= 1;
    for (slot = 0; slot < CARD_MODEL_SLOT_COUNT; slot++) {
        for (channel = 0; channel < CARD_MODEL_COLOR_CHANNELS; channel++) {
            work->modelColor[slot][channel] += work->modelFadeStep;
            if (work->modelColorTarget[slot][channel] < work->modelColor[slot][channel]) {
                work->modelColor[slot][channel] = work->modelColorTarget[slot][channel];
            }
        }
    }
}

void CardFadeOut(CardGameWork *work)
{
    int slot;
    int level;

    if (work->fadeOutTimer == 0) {
        return;
    }
    work->fadeOutTimer -= 1;
    for (slot = 0; slot < CARD_MODEL_SLOT_COUNT; slot++) {
        level = work->fadeOutColor[slot] - work->fadeOutStep;
        work->fadeOutColor[slot] = (level < 0) ? 0 : level;
    }
}

void CardFadeIn(CardGameWork *work)
{
    int slot;
    int level;
    int target;

    if (work->fadeOutTimer == 0) {
        return;
    }
    work->fadeOutTimer -= 1;
    for (slot = 0; slot < CARD_MODEL_SLOT_COUNT; slot++) {
        level = work->fadeOutColor[slot] + work->fadeOutStep;
        target = work->unmodeled_710A[slot];
        if (target < level) {
            level = target;
        }
        work->fadeOutColor[slot] = level;
    }
}

typedef struct { u8 type; u8 pad; u16 flag; u8 rest[0x20]; } CardMakeDispEnt;

int CardMakeDispList(s16 *out, u16 mask, CardMakeDispEnt *ent, int n)
{
    int i;
    int cnt = 0;
    int lo = mask & 0xFF;
    mask >>= 8;
    for (i = 0; i < n; i++, ent++) {
        if (ent->type & mask) {
            if (ent->type == 1 && !(ent->flag & lo)) continue;
            *out = i;
            out += 2;
            cnt++;
        }
    }
    return cnt;
}

typedef struct { s16 id; s16 cnt; } CdwEnt;
typedef struct { u8 pad[0x12]; s16 groupCount; u8 pad2[2]; CdwEnt g[1]; } CdwDeck;
typedef struct { u8 pad[4]; CdwEnt e[1]; } CdwIds;

void CardCopyDeckWork(u16 *out, CdwDeck *deck)
{
    int n = 0;
    int i;
    int j;
    for (i = 0; i < deck->groupCount; i++) {
        for (j = 0; j < ((CdwIds *)deck)->e[i + 4].cnt; j++) {
            out[n++] = ((CdwIds *)deck)->e[i + 4].id;
        }
    }
}

typedef int s32;

/* Compact deck-build list CardMakeDeckProc/CardLoadDeckProc/CardSaveDeckProc
 * edit: a card-type group count followed by one 4-byte group record per
 * group, whose selected-copy count is its first halfword. */
typedef struct CardDeckGroupEntry {
    s16 copyCount;              /* +0x00 */
    u8 unmodeled_02[2];
} CardDeckGroupEntry;

typedef struct CardDeckGroupList {
    u8 unmodeled_00[0x12];
    s16 groupCount;             /* +0x12 */
    u8 unmodeled_14[2];
    CardDeckGroupEntry groups[1]; /* +0x16: groupCount entries */
} CardDeckGroupList;

s32 CardGetDeckAllCnt(CardDeckGroupList *deck)
{
    s32 total;
    int i;

    total = 0;
    for (i = 0; i < deck->groupCount; i++) {
        total += deck->groups[i].copyCount;
    }
    return total;
}

typedef struct { s16 id; s16 cnt; } CardDeckIdCnt;

typedef struct { u8 pad[4]; CardDeckIdCnt e[1]; } CardDeckIdList;

s32 CardGetDeckCardCnt(s16 id, CardDeckGroupList *deck)
{
    s32 total;
    int i;

    total = 0;
    for (i = 0; i < deck->groupCount; i++) {
        if (((CardDeckIdList *)deck)->e[i + 4].id == id) {
            total += ((CardDeckIdList *)deck)->e[i + 4].cnt;
        }
    }
    return total;
}

typedef struct { u16 id; s16 cnt; } CardCleanEnt;

void CardCleanUpDeck(CardCleanEnt *deck, s16 n)
{
    int i;
    int j;
    int k;
    u16 min;
    s16 id;
    s16 cnt;

    for (i = 0; i < n; i++) {
        min = deck[i].id;
        j = i + 1;
        k = -1;
        for (; j < n; j++) {
            if (deck[j].id < min) {
                min = deck[j].id;
                k = j;
            }
        }
        if (k > 0) {
            id = deck[i].id;
            cnt = deck[i].cnt;
            deck[i].id = deck[k].id;
            deck[i].cnt = deck[k].cnt;
            deck[k].id = id;
            deck[k].cnt = cnt;
        }
    }
}

typedef struct { s16 id; s16 cnt; } CardDeckEnt;
typedef struct {
    u8 pad0[5];
    s8 unk5;
    u8 pad6[6];
    s16 unkC;
    s16 unkE;
    u8 pad10[2];
    s16 n;
    CardDeckEnt e[1];
} CardDeckT;

int CardAddDeck(CardDeckT *deck, s16 id)
{
    int found = 0;
    int i;

    for (i = 0; i < deck->n; i++) {
        if (deck->e[i].id == id) {
            found = 1;
            break;
        }
    }
    if (found) {
        if (deck->e[i].cnt < 3) {
            deck->e[i].cnt++;
        }
    } else {
        deck->e[deck->n].id = id;
        deck->e[deck->n].cnt = 1;
        deck->n++;
    }
    CardCleanUpDeck((CardCleanEnt *)deck->e, deck->n);
    for (i = 0; i < deck->n; i++) {
        if (deck->e[i].id == id) {
            break;
        }
    }
    return i;
}

int CardDelDeck(CardDeckT *deck, s16 id)
{
    int i;

    for (i = 0; i < deck->n; i++) {
        if (deck->e[i].id == id) {
            if (deck->e[i].cnt < 2) {
                deck->e[i].id = -1;
                deck->e[i].cnt = 0;
                CardCleanUpDeck((CardCleanEnt *)deck->e, deck->n);
                deck->n--;
                deck->unkE--;
                if (deck->unkC != 0) {
                    deck->unkC--;
                } else if (deck->unk5 != 0) {
                    deck->unk5--;
                } else {
                    return 0;
                }
            } else {
                deck->e[i].cnt--;
            }
            break;
        }
    }
    return 1;
}

int CDTSGetStrLen(unsigned char *s);

void CardNameConvert(unsigned char *text)
{
    int len;
    int i;
    unsigned char *p;
    unsigned char *q;
    unsigned char *r;
    unsigned char ch;

    len = CDTSGetStrLen(text);
    for (i = 0; i < len && text[i] != 0; i++) {
        p = &text[i];
        ch = *p;
        switch (ch) {
        case 0x40:
            *p = 0x2C;
            break;
        case 0x23:
            *p = 0x27;
            break;
        case 0x24:
            *p = 0x0A;
            break;
        case 0x5C:
            q = p + 1;
            if (*q == ch) {
                r = p + 2;
                if (*r == *q) {
                    *p = 0x23;
                    *q = 0x37;
                    *r = 0x33;
                } else if (*r == 0x25) {
                    *p = 0x23;
                    *q = 0x32;
                    *r = 0x33;
                }
            }
            break;
        }
    }
}

typedef struct {
    u16 flags;
    char unk2[3];
    s8 sel;
    s16 x;
    s16 y;
    u16 len;
    s16 top;
    char unkE[2];
    s16 count;
    s16 num;
    struct { s16 id; s16 pad; } ent[1];
} CardListWin;

extern void xglFontPrint(int x, int y, int color, const char *text);

extern int sprintf(char *buf, const char *fmt, ...);

extern char D_00A4D148[];

extern char D_00A4D150[];

extern char D_00A4D158[];

extern char D_00A4D160[];

extern char D_00A4D168[];

extern char D_00A4D170[];

typedef struct { u8 pad[0x18]; int name; u8 pad1C[8]; } CardListInfo;

extern char D_00A4D140[];

void CardDeckDisp(CardListWin *w, CardListInfo *info)
{
    char buf[32];
    char num[16];
    int i;
    int j;
    int id;
    int off;
    char *name;

    i = 0;
    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D140);
    for (; i < w->count; i++) {
        if (i < w->num) {
            id = w->ent[i + w->top].id;
            if (id >= 0) {
                off = id * sizeof(CardListInfo);
                name = (char *)(((CardListInfo *)((char *)info + off))->name + 0x01000000);
                if (i == w->sel) {
                    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D148);
                } else {
                    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D150);
                }
                for (j = 0; j < w->len * 2; j++) {
                    buf[j] = name[j];
                }
                buf[j] = 0;
                CardNameConvert((unsigned char *)buf);
                xglFontPrint(0, 0, 0, D_00A4D158);
                xglFontPrint(w->x, w->y + i * 25, 0xFFF0, buf);
                if (i == w->sel) {
                    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D148);
                } else {
                    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D160);
                }
                sprintf(num, D_00A4D168, w->ent[i + w->top].pad);
                xglFontPrint(w->x + w->len * 10 + 6, w->y + i * 25, 0xFFF0, num);
            }
        }
    }
    *(s16 *)w->unkE = w->num;
    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D170);
}

extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
extern char *strcat(char *destination, const char *source);
extern char D_00A4D130[];
int CDTSGetStrLen(unsigned char *s) {
    int n = 0;
    int k;
    while (*s != 0) {
        if (*s < 0x20) {
            k = xglFontGetSPcodeSize(*s, s);
            if (k >= 0) {
                do { k--; s++; } while (k >= 0);
            }
        } else {
            s++;
            n++;
        }
    }
    return n;
}

extern char D_00A4D180[];

void CardDispTxtSub(u16 x, u16 y, unsigned char *text, u16 color)
{
    char col[6] = "\x15\x03\x03";
    int len;
    int i;
    int c;
    unsigned char *p;
    unsigned char *q;
    unsigned char *r;
    unsigned char ch;

    xglFontPrint(0, 0, 0, D_00A4D180);
    c = color * 21;
    col[3] = c >> 8;
    col[4] = c;
    col[5] = 0;
    xglFontPrint(0, 0, 0, col);
    len = CDTSGetStrLen(text);
    for (i = 0; i < len && text[i] != 0; i++) {
        p = &text[i];
        ch = *p;
        switch (ch) {
        case 0x40:
            *p = 0x2C;
            break;
        case 0x23:
            *p = 0x27;
            break;
        case 0x24:
            *p = 0x0A;
            break;
        case 0x5C:
            q = p + 1;
            if (*q == ch) {
                r = p + 2;
                if (*r == *q) {
                    *p = 0x23;
                    *q = 0x37;
                    *r = 0x33;
                } else if (*r == 0x25) {
                    *p = 0x23;
                    *q = 0x32;
                    *r = 0x33;
                }
            }
            break;
        }
    }
    xglFontPrint(x, y, 0xFFF0, (char *)text);
}

typedef struct { char c[6]; } CardTxtCol;
extern const CardTxtCol D_00A4D190;

void CardDispTxtSub2(u16 x, u16 y, unsigned char *text)
{
    CardTxtCol col = D_00A4D190;
    int len;
    int i;
    unsigned char *p;
    unsigned char *q;
    unsigned char *r;
    unsigned char ch;

    xglFontPrint(0, 0, 0, D_00A4D180);
    xglFontPrint(0, 0, 0, col.c);
    len = CDTSGetStrLen(text);
    for (i = 0; i < len && text[i] != 0; i++) {
        p = &text[i];
        ch = *p;
        switch (ch) {
        case 0x40:
            *p = 0x2C;
            break;
        case 0x23:
            *p = 0x27;
            break;
        case 0x24:
            *p = 0x0A;
            break;
        case 0x5C:
            q = p + 1;
            if (*q == ch) {
                r = p + 2;
                if (*r == *q) {
                    *p = 0x23;
                    *q = 0x37;
                    *r = 0x33;
                } else if (*r == 0x25) {
                    *p = 0x23;
                    *q = 0x32;
                    *r = 0x33;
                }
            }
            break;
        }
    }
    xglFontPrint(x, y, 0xFFF0, (char *)text);
}

INCLUDE_ASM("asm/nonmatchings/ov10/card_fread", CardDispLight);

extern void nmlModelSetLight(void *colorMod, void *normal);
extern void nmlModelSetPlace(void *m);
extern void nmlModelSetTexture(const char *texture);
extern void nmlModelEntryCard(void *model);
extern void xglMatrixTrans(float (*destination)[4], float (*source)[4], const float translation[4]);
extern int printf(const char *format, ...);
extern char MiddleColor[];
extern char CNColor[];
extern char asDir[];

void CardDispCard(float (*place)[4], s16 no, s16 flag)
{
    float trans[4];
    float mat[4][4];
    char *tex;
    unsigned char *info;
    void *model;
    int n;

    n = (short)(unsigned short)no;
    if (no < 0) {
        return;
    }
    if ((u16)no >= 0x94) {
        printf("CardDispCallError No[%d]\n", no, n);
        return;
    }
    tex = (char *)(n * 0xBAB0 + 0x0100C000);
    info = (unsigned char *)(n * 0x24 + 0x01000010);
    if (flag <= 0) {
        return;
    }

    if (flag & 0x40) {
        nmlModelSetLight(MiddleColor, asDir);
    } else if (flag & 0x80) {
        nmlModelSetLight(CNColor, asDir);
    }
    nmlModelSetPlace(place);
    nmlModelSetTexture(tex);
    nmlModelEntryCard((void *)0x016F8500);
    trans[0] = -0.436f;
    trans[1] = 0.847f;
    trans[2] = 0.005f;
    trans[3] = 1.0f;
    xglMatrixTrans(mat, place, trans);
    if (flag & 0x40) {
        nmlModelSetLight(MiddleColor, asDir);
    } else if (flag & 0x80) {
        nmlModelSetLight(CNColor, asDir);
    }
    nmlModelSetPlace(mat);
    nmlModelSetTexture(tex);
    model = (void *)0x01701F80;
    nmlModelEntryCard((void *)0x016F8100);
    switch (*info) {
    case 1:
        model = (void *)0x016F8900;
        break;
    case 2:
        model = (void *)0x016FD440;
        break;
    }
    nmlModelSetPlace(place);
    nmlModelSetTexture((char *)0x016E0000);
    if (flag & 0x40) {
        nmlModelSetLight(MiddleColor, asDir);
    } else if (flag & 0x80) {
        nmlModelSetLight(CNColor, asDir);
    }
    nmlModelEntryCard(model);
    if (flag & 2) {
        nmlModelSetPlace(place);
        nmlModelSetTexture((char *)0x016E0000);
        nmlModelEntryCard((void *)0x01706AC0);
    }
}

extern void xglMatrixUnit(float (*matrix)[4]);
extern void xglMatrixScale(float (*destination)[4], float (*source)[4], const float scale[4]);
extern float CardHpPos01[3];
extern float CardHpPos10[3];
extern float CardPowPos01[3];
extern float CardPowPos10[3];
extern float CardStnbyPos[3];
extern float CardAttriPos[3];
extern float CardAttakPos[3];
extern float CardCondPos[4][3];
extern char D_00A4D340[];
extern char D_00A4D350[];
extern char D_00A4D368[];
extern char D_00A4D370[];
typedef struct {
    u8 type;
    u8 pad1;
    u16 attr;
    u8 cond[4];
    u8 pad8[8];
    u8 stnby;
    u8 pad11;
    u8 hp;
    u8 atk;
    u8 pow;
    u8 pad15;
    u8 kind;
    u8 pad17[5];
    int name;
    int text;
} CardDispInfo;
#define CARD_DISP_PART(pos, tex, m) \
    trans[0] = (pos)[0]; \
    trans[1] = (pos)[1]; \
    trans[2] = (pos)[2]; \
    xglMatrixTrans(mat, place, tp); \
    nmlModelSetPlace(mat); \
    nmlModelSetTexture(tex); \
    nmlModelEntryCard(m)

void CardDisp(float (*place)[4], s16 no, s16 flag, s16 disp)
{
    float mat[4][4] __attribute__((aligned(16)));
    float trans[4];
    float vec[4];
    CardDispInfo *info;
    char *tex;
    int front;
    int v;
    int i;
    int d;
    u8 *cond;
    float *tp;
    void *model;

    if (no < 0) {
        return;
    }
    info = (CardDispInfo *)(no * 0x24 + 0x01000010);
    tex = (char *)(no * 0xBAB0 + 0x0100C000);
    front = flag & 1;
    if (flag & 1) {
        nmlModelSetPlace(place);
        nmlModelSetTexture(tex);
        nmlModelEntryCard((void *)0x016F8500);
        tp = trans;
        trans[0] = -0.41599998f;
        trans[1] = 0.847f;
        trans[2] = 0.005f;
        trans[3] = 1.0f;
        xglMatrixTrans(mat, place, tp);
        nmlModelSetPlace(mat);
        nmlModelSetTexture(tex);
        nmlModelEntryCard((void *)0x016F8100);
        if (info->type == 1) {
            if (!(info->attr & 0xFFE0)) {
                v = (u8)(info->hp / 10);
                if (v) {
                    model = (void *)(0x01C80000 + (v << 10));
                    CARD_DISP_PART(CardHpPos10, (char *)0x01C91000, model);
                }
                v = info->hp % 10;
                model = (void *)(0x01C80000 + (v << 10));
                CARD_DISP_PART(CardHpPos01, (char *)0x01C91000, model);
            }
            v = (u8)(info->pow / 10);
            if (v) {
                model = (void *)(0x01C80000 + (v << 10));
                CARD_DISP_PART(CardPowPos10, (char *)0x01C91000, model);
            }
            v = info->pow % 10;
                model = (void *)(0x01C80000 + (v << 10));
                CARD_DISP_PART(CardPowPos01, (char *)0x01C91000, model);
        }
        v = info->stnby % 10;
                model = (void *)(0x01C80000 + (v << 10));
                CARD_DISP_PART(CardStnbyPos, (char *)0x01C91000, model);
        if (info->type == 1) {
            v = 0;
            switch (info->attr) {
            case 1:
                break;
            case 0x20:
            case 0x40:
                v = 1;
            case 0x10:
                v++;
            case 4:
                v++;
            case 8:
                v++;
            case 2:
                v++;
                break;
            }
            model = (void *)(0x01C87400 + (v << 10));
            CARD_DISP_PART(CardAttriPos, (char *)0x01C91000, model);
        }
        i = 0;
        cond = info->cond;
        for (; i < 4; i++) {
            v = 0;
            switch (cond[i]) {
            case 1:
                break;
            case 4:
                v = 1;
            case 8:
                v++;
            case 2:
                v++;
                break;
            case 31:
                v = 6;
                break;
            default:
                v = 8;
                break;
            }
            if (v != 8) {
                model = (void *)(0x01C85000 + (v << 10));
                CARD_DISP_PART(CardCondPos[i], (char *)0x01C91000, model);
            }
        }
        v = 0;
        switch (info->atk) {
        case 1:
            break;
        case 4:
            v = 1;
        case 3:
            v++;
        case 2:
            v++;
            break;
        default:
            v = -1;
            break;
        }
        if (v >= 0) {
            model = (void *)(0x01C89400 + (v << 10));
            CARD_DISP_PART(CardAttakPos, (char *)0x01C94050, model);
        }
        switch (info->kind) {
        case 0x50:
            model = (void *)0x01C90730;
            break;
        case 0x52:
            model = (void *)0x01C90220;
            break;
        case 0x55:
            model = (void *)0x01C8FD10;
            break;
        case 0x41:
        default:
            model = (void *)0x01C8F800;
            break;
        }
        nmlModelSetPlace(place);
        nmlModelSetTexture((char *)0x01C9BB70);
        nmlModelEntryCard(model);
    }
    __asm__ __volatile__(
        "lq $2, 0x0(%1)\n"
        "sq $2, 0x0(%0)\n"
        "lq $2, 0x10(%1)\n"
        "sq $2, 0x10(%0)\n"
        "lq $2, 0x20(%1)\n"
        "sq $2, 0x20(%0)\n"
        "lq $2, 0x30(%1)\n"
        "sq $2, 0x30(%0)\n"
        : : "r"(mat), "r"(place) : "$2", "memory");
    if (!(flag & 1)) {
        nmlModelSetPlace(mat);
        nmlModelSetTexture((char *)0x01C74000);
        nmlModelEntryCard((void *)0x01C73000);
    } else {
        switch (info->type) {
        case 2:
            model = (void *)0x016FD440;
            break;
        case 1:
            model = (void *)0x016F8900;
            break;
        case 3:
        default:
            model = (void *)0x01701F80;
            break;
        }
        nmlModelSetPlace(mat);
        nmlModelSetTexture((char *)0x016E0000);
        nmlModelEntryCard(model);
        if (flag & 2) {
            nmlModelSetPlace(place);
            nmlModelSetTexture((char *)0x016E0000);
            nmlModelEntryCard((void *)0x01706AC0);
        }
    }
    nmlModelSetPlace(mat);
    nmlModelSetTexture((char *)0x01717600);
    nmlModelEntryCard((void *)0x01713600);
    if (disp == 0) {
        return;
    }
    if (info->name) {
        xglFontPrint(0x78, 0x120, 0xFFF0, D_00A4D150);
        if (flag > 0) {
            CardDispTxtSub(0x18, 0x138, (unsigned char *)(info->name + 0x01000000), 0x17);
        } else {
            CardDispTxtSub(0x18, 0x138, (unsigned char *)D_00A4D340, 0x17);
        }
    }
    if (flag <= 0) {
        xglFontPrint(0x18, 0x19C, 0xFFF0, D_00A4D350);
    } else if (info->text) {
        xglFontPrint(0x18, 0x120, 0xFFF0, D_00A4D368);
        xglFontPrint(0, 0, 0, D_00A4D158);
        xglFontPrint(0x18, 0x19C, 0xFFF0, (char *)(info->text + 0x01000000));
    }
    xglFontPrint(0x78, 0x120, 0xFFF0, D_00A4D370);
    if (disp & 4) {
        vec[1] = -1.2f;
        vec[2] = 0.8f;
        vec[0] = 0.0f;
        vec[3] = 1.0f;
        xglMatrixUnit(mat);
        xglMatrixTrans(mat, mat, vec);
        vec[0] = 0.88f;
        vec[1] = 0.92f;
        vec[2] = 1.0f;
        xglMatrixScale(mat, mat, vec);
        nmlModelSetPlace(mat);
        nmlModelSetTexture((char *)0x01DB5800);
        nmlModelEntryCard((void *)0x01DA0000);
    }
}

extern unsigned char SampleDeck0[];

void CardSampleInitDeck0(s16 *deck)
{
    int i;

    for (i = 0; i < 40; i++) {
        deck[i] = SampleDeck0[i];
    }
}

extern unsigned char SampleDeck1[];

void CardSampleInitDeck1(s16 *deck)
{
    int i;

    for (i = 0; i < 40; i++) {
        deck[i] = SampleDeck1[i];
    }
}

extern unsigned char SampleDeck2[];

void CardSampleInitDeck2(s16 *deck)
{
    int i;

    for (i = 0; i < 40; i++) {
        deck[i] = SampleDeck2[i];
    }
}

extern unsigned char SampleDeck3[];

void CardSampleInitDeck3(s16 *deck)
{
    int i;

    for (i = 0; i < 40; i++) {
        deck[i] = SampleDeck3[i];
    }
}

extern unsigned char SampleDeck4[];

void CardSampleInitDeck4(s16 *deck)
{
    int i;

    for (i = 0; i < 40; i++) {
        deck[i] = SampleDeck4[i];
    }
}

extern unsigned char SampleDeck5[];

void CardSampleInitDeck5(s16 *deck)
{
    int i;

    for (i = 0; i < 40; i++) {
        deck[i] = SampleDeck5[i];
    }
}

extern unsigned char SampleDeck6[];

void CardSampleInitDeck6(s16 *deck)
{
    int i;

    for (i = 0; i < 40; i++) {
        deck[i] = SampleDeck6[i];
    }
}

extern unsigned char SampleDeck7[];

void CardSampleInitDeck7(s16 *deck)
{
    int i;

    for (i = 0; i < 40; i++) {
        deck[i] = SampleDeck7[i];
    }
}

extern unsigned char SampleDeck8[];

void CardSampleInitDeck8(s16 *deck)
{
    int i;

    for (i = 0; i < 40; i++) {
        deck[i] = SampleDeck8[i];
    }
}

extern unsigned char SampleDeck9[];

void CardSampleInitDeck9(s16 *deck)
{
    int i;

    for (i = 0; i < 40; i++) {
        deck[i] = SampleDeck9[i];
    }
}

/* Per-enemy ROM deck lists (each entry points at a 40-byte card-id array). */
extern unsigned char *EnemyDeckLst[];

void CardCopyRom2Deck(s16 *deck, s32 enemyId)
{
    unsigned char *romDeck;
    int i;

    romDeck = EnemyDeckLst[enemyId];
    for (i = 0; i < 40; i++) {
        deck[i] = romDeck[i];
    }
}

/* The stored decks repeat CardSaveData.deck as 42-byte records, so the record
 * of slot n starts n * 42 bytes into the save block; the address is formed as
 * n * 32 plus n * 10 (0x00a0ae48..0x00a0ae60). Only the forty card ids at the
 * start of a record are copied, one byte each into a halfword entry. */
void CardCopyRam2Deck(s16 *deck, CardGameWork *work, s32 slot)
{
    unsigned char *stored;
    int i;

    stored = ((CardSaveData *)((slot << 5) + (unsigned int)work->save + slot * 10))->deck;
    for (i = 39; i >= 0; i--) {
        *deck++ = *stored++;
    }
}

typedef struct { s16 id; s16 cnt; } CLDEntry;
typedef struct { char pad[0x12]; s16 count; CLDEntry entries[151]; } CLDList;

void CardCopyRam2CLD(CLDList *list, CardGameWork *work, s32 slot)
{
    CardSaveData *sd;
    int prev;
    int n;
    int i;

    sd = (CardSaveData *)((slot << 5) + (unsigned int)work->save + slot * 10);
    prev = -1;
    n = -1;
    for (i = 0; i < 40; i++) {
        if (prev != sd->deck[i]) {
            list->entries[++n].id = sd->deck[i];
            list->entries[n].cnt = 1;
        } else {
            list->entries[n].cnt++;
        }
        prev = sd->deck[i];
    }
    list->count = n + 1;
}

void CardCopyCLD2Ram(CLDList *list, CardGameWork *work, s32 slot)
{
    int i;
    int j;
    int k;

    k = 0;
    for (i = 0; i < list->count; i++) {
        for (j = 0; j < list->entries[i].cnt; j++) {
            work->save->deck[slot * 42 + k] = list->entries[i].id;
            k++;
        }
    }
}

/*
 * The one's-complement checksum of a stored deck's 40 card bytes
 * (CardCopyRam2Deck's comment documents the record's 42-byte stride and the
 * CardSaveData reuse for the deck bytes themselves); the stored checksum
 * sits 2 bytes before that same deck array, inside CardSaveData's own
 * unmodeled_02 span (+0x2A).
 */
s32 CardChkDeckRam(CardGameWork *work, int unused, s32 slot)
{
    u8 *deckByte;
    u16 storedChecksum;
    u8 sum;
    s32 remaining;

    deckByte = ((CardSaveData *) ((slot << 5) + (unsigned int) work->save + slot * 10))->deck;
    sum = 0;
    remaining = 0x27;
    do {
        sum += *deckByte;
        deckByte += 1;
        remaining -= 1;
    } while (remaining >= 0);
    storedChecksum = ((CardDeckRecord *) ((unsigned int) work->save + slot * 0x2A))->checksum;
    return (u8) ~sum == storedChecksum;
}

typedef struct { int pad[3]; unsigned char *p; } CardCntObj;

unsigned int CardCntList(CardCntObj *o, int idx)
{
    unsigned int r = 0;
    unsigned char b = o->p[4 + idx / 4];
    int s = idx % 4;
    switch (s) {
    case 1: r = (b & 0xC) >> 2; break;
    case 0: r = b & 3; break;
    case 3: r = b >> 6; break;
    case 2: r = (b & 0x30) >> 4; break;
    }
    return r;
}

int CardAddList(CardCntObj *o, int idx)
{
    int r = 0;
    unsigned char b = o->p[4 + idx / 4];
    unsigned int v;

    switch (idx % 4) {
    case 0:
        v = (b & 3) + 1;
        if (v > 3) {
            v = 3;
            r = 1;
        }
        b = (b & 0xFC) + v;
        break;
    case 1:
        v = (b & 0xC) + 4;
        if (v > 0xC) {
            v = 0xC;
            r = 1;
        }
        b = (b & 0xF3) + v;
        break;
    case 2:
        v = (b & 0x30) + 0x10;
        if (v > 0x30) {
            v = 0x30;
            r = 1;
        }
        b = (b & 0xCF) + v;
        break;
    case 3:
        v = (b & 0xC0) + 0x40;
        if (v > 0xC0) {
            v = 0xC0;
            r = 1;
        }
        b = (b & 0x3F) + v;
        break;
    }
    o->p[4 + idx / 4] = b;
    return r;
}

typedef struct { u8 b0; u8 pad1[2]; u8 b3; u8 b4; u8 b5; s16 card; u8 pad8[2]; } CPIWLayer;
typedef struct { u8 pad0[8]; CPIWLayer layers[40]; float pos[3]; u8 pad1A4[0x18]; } CPIWSlot;
typedef struct { s16 h; u8 state; u8 timer; float pos[3]; float vel[3]; } CPIWMove;
typedef struct {
    u8 pad0[4];
    u8 c4;
    u8 c5;
    u8 c6;
    u8 pad7;
    s16 cards[40];
    s16 deck[40];
    s16 sute[40];
    s16 junk[40];
    s16 x148[40];
    s16 x198;
    u8 pad19A[2];
    CPIWSlot battle[4];
    CPIWSlot disposal[4];
    s16 operations[40];
    s16 operationFlags[40];
    s16 operationDuration[40];
    u8 pad106C[0x14CC - 0x106C];
    CPIWMove opeMv[40];
    CPIWMove handMv[40];
    CPIWMove deckMv[40];
    CPIWMove suteMv[40];
} CPIWSide;
extern float BattlePos1P[4][3];
extern float DisposePos1P[4][3];
extern unsigned short xglSRand(void);
int CardPlayDrawCard(CardHand *hand);

void CardPlayInitWork(CPIWSide *side, s16 *deck)
{
    s16 order[40];
    int i;
    int j;
    s16 *p;

    side->x198 = -1;
    side->c4 = 0;
    side->c5 = 0;
    side->c6 = 0;
    for (i = 0; i < 40; i++) {
        side->cards[i] = -1;
        side->deck[i] = -1;
        side->sute[i] = -1;
        side->junk[i] = -1;
        side->x148[i] = -1;
        side->handMv[i].h = 0;
        side->handMv[i].state = 0;
        side->handMv[i].timer = 0;
        side->deckMv[i].h = 0;
        side->deckMv[i].state = 0;
        side->deckMv[i].timer = 0;
        side->suteMv[i].h = 0;
        side->suteMv[i].state = 0;
        side->suteMv[i].timer = 0;
        order[i] = -1;
        side->operations[i] = -1;
        side->operationFlags[i] = 0;
        side->operationDuration[i] = 0;
        side->opeMv[i].h = 0;
        side->opeMv[i].state = 0;
        side->opeMv[i].timer = 0;
        for (j = 0; j < 4; j++) {
            side->battle[j].layers[i].card = -1;
            side->battle[j].layers[i].b3 = 0;
            side->battle[j].layers[i].b4 = 0;
            side->battle[j].layers[i].b5 = 0;
            side->battle[j].layers[i].b0 = 0;
            side->disposal[j].layers[i].card = -1;
            side->battle[j].pos[0] = BattlePos1P[j][0];
            side->battle[j].pos[1] = BattlePos1P[j][1];
            side->battle[j].pos[2] = BattlePos1P[j][2];
            side->disposal[j].layers[i].b3 = 0;
            side->disposal[j].layers[i].b4 = 0;
            side->disposal[j].layers[i].b5 = 0;
            side->disposal[j].layers[i].b0 = 0;
            side->disposal[j].pos[0] = DisposePos1P[j][0];
            side->disposal[j].pos[1] = DisposePos1P[j][1];
            side->disposal[j].pos[2] = DisposePos1P[j][2];
        }
    }
    for (i = 0; i < 40;) {
        p = &order[xglSRand() % 40];
        if (*p < 0) {
            *p = i;
            i++;
        }
    }
    for (i = 0; i < 40; i++) {
        side->deck[i] = deck[order[i]];
    }
    for (i = 0; i < 6; i++) {
        CardPlayDrawCard((CardHand *)side);
    }
}

void CPIWsub(CPIWSide *side)
{
    int i;
    int j;

    side->x198 = -1;
    side->c4 = 0;
    side->c5 = 0;
    side->c6 = 0;
    for (i = 0; i < 40; i++) {
        side->cards[i] = -1;
        side->deck[i] = -1;
        side->sute[i] = -1;
        side->junk[i] = -1;
        side->x148[i] = -1;
        side->handMv[i].h = 0;
        side->handMv[i].state = 0;
        side->handMv[i].timer = 0;
        side->deckMv[i].h = 0;
        side->deckMv[i].state = 0;
        side->deckMv[i].timer = 0;
        side->suteMv[i].h = 0;
        side->suteMv[i].state = 0;
        side->suteMv[i].timer = 0;
        side->operations[i] = -1;
        side->operationFlags[i] = 0;
        side->operationDuration[i] = 0;
        side->opeMv[i].h = 0;
        side->opeMv[i].state = 0;
        side->opeMv[i].timer = 0;
        for (j = 0; j < 4; j++) {
            side->battle[j].layers[i].card = -1;
            side->battle[j].layers[i].b3 = 0;
            side->battle[j].layers[i].b4 = 0;
            side->battle[j].layers[i].b5 = 0;
            side->battle[j].layers[i].b0 = 0;
            side->disposal[j].layers[i].card = -1;
            side->battle[j].pos[0] = BattlePos1P[j][0];
            side->battle[j].pos[1] = BattlePos1P[j][1];
            side->battle[j].pos[2] = BattlePos1P[j][2];
            side->disposal[j].layers[i].b3 = 0;
            side->disposal[j].layers[i].b4 = 0;
            side->disposal[j].layers[i].b5 = 0;
            side->disposal[j].layers[i].b0 = 0;
            side->disposal[j].pos[0] = DisposePos1P[j][0];
            side->disposal[j].pos[1] = DisposePos1P[j][1];
            side->disposal[j].pos[2] = DisposePos1P[j][2];
        }
    }
}

void CardPlayNoInitWork(CardPlaySide *side, s16 *deck)
{
    int i;
    s16 *p;

    CPIWsub((CPIWSide *)side);
    for (i = 0; i < 34; i++) {
        side->deck[i] = deck[i];
    }
    p = side->hand.cards;
    for (; i < 40; i++) {
        *p++ = deck[i];
    }
}

void CardPlayShuffleYama(CardPlaySide *side);
s32 CardPlayYamaCnt(CardPlaySide *side);

void CardPlayInitWorkFake(CardPlaySide *side, s16 *deck, CardDefinition *defs)
{
    s16 cards[40];
    s16 picked[8];
    int i;
    int j;
    int k;
    int n;
    int best;
    int dup;
    int b;

    i = 0;
    n = 0;
    CPIWsub((CPIWSide *)side);
    for (; i < 40; i++) {
        cards[i] = deck[i];
    }
    for (i = 0; i < 40; i++) {
        j = i;
        best = -1;
        for (; j < 40; j++) {
            if (cards[j] < 0) {
                continue;
            }
            if (defs[cards[j]].kind != 1 || defs[cards[j]].flags != 1) {
                continue;
            }
            dup = 0;
            for (k = 0; k < n; k++) {
                if (picked[k] == cards[j]) {
                    dup = 1;
                    break;
                }
            }
            if (dup) {
                continue;
            }
            if (best < 0) {
                best = j;
                continue;
            }
            dup = 0;
            b = 0;
            for (k = 0; k < 4; k++) {
                if (defs[cards[best]].unmodeled_04[k]) {
                    dup++;
                }
                if (defs[cards[j]].unmodeled_04[k]) {
                    b++;
                }
            }
            if (b < dup) {
                best = j;
            }
        }
        if (best < 0 || n >= 3) {
            break;
        }
        picked[n++] = cards[best];
        cards[best] = -1;
    }
    j = 0;
    for (i = 0; i < 40; i++) {
        if (cards[i] >= 0) {
            side->deck[j++] = cards[i];
        }
    }
    i = 0;
    CardPlayShuffleYama(side);
    CardPlayShuffleYama(side);
    for (; i < n; i++) {
        side->hand.cards[i] = picked[i];
    }
    best = CardPlayYamaCnt(side);
    for (j = 0; j < 6 - n; j++) {
        side->hand.cards[i] = side->deck[best - 1];
        side->deck[best - 1] = -1;
        i++;
        best--;
    }
}

extern unsigned short xglSRand(void);
s32 CardPlayYamaCnt(CardPlaySide *side);

void CardPlayShuffleYama(CardPlaySide *side)
{
    s16 order[40];
    s16 copy[40];
    s32 n;
    s32 i;
    s16 *p;

    i = 0;
    n = CardPlayYamaCnt(side);
    for (; i < n; i++) {
        order[i] = -1;
        copy[i] = side->deck[i];
    }
    for (i = 0; i < n;) {
        p = &order[xglSRand() % n];
        if (*p < 0) {
            *p = i;
            i++;
        }
    }
    for (i = 0; i < n; i++) {
        side->deck[i] = copy[order[i]];
    }
}

extern void xglSoundEffectNormalID(int sound_id, int variant);
typedef struct { u8 unk0[2]; u8 state; u8 timer; float pos[3]; float vel[3]; } CardDrawMove;
typedef struct { u8 pad[0x192C]; CardDrawMove mv[40]; } CardDrawMoveView;

int CardPlayDrawCard(CardHand *hand)
{
    s32 i;
    s32 h;

    for (i = 0; i < 39; i++) {
        if (hand->cards[i + 41] < 0) {
            break;
        }
    }
    if (i == 0 && hand->cards[40] < 0) {
        return 1;
    }
    for (h = 0; h < 40; h++) {
        if (hand->cards[h] < 0) {
            break;
        }
    }
    hand->cards[h] = hand->cards[i + 40];
    ((CardDrawMoveView *)hand)->mv[h].timer = 10;
    ((CardDrawMoveView *)hand)->mv[h].state = 4;
    ((CardDrawMoveView *)hand)->mv[h].pos[0] = 0.0f;
    ((CardDrawMoveView *)hand)->mv[h].pos[1] = -1.81f;
    ((CardDrawMoveView *)hand)->mv[h].pos[2] = 0.1f;
    ((CardDrawMoveView *)hand)->mv[h].vel[0] = 0.0f;
    ((CardDrawMoveView *)hand)->mv[h].vel[1] = 0.15f;
    ((CardDrawMoveView *)hand)->mv[h].vel[2] = 0.025f;
    hand->cards[i + 40] = -1;
    xglSoundEffectNormalID(0x2000E, 0);
    return 0;
}

int CardPlayDrawSute(CardHand *hand)
{
    s32 i;
    s32 h;

    for (i = 0; i < 39; i++) {
        if (hand->cards[i + 81] < 0) {
            break;
        }
    }
    if (i == 0 && hand->cards[80] < 0) {
        return 1;
    }
    for (h = 0; h < 40; h++) {
        if (hand->cards[h] < 0) {
            break;
        }
    }
    hand->cards[h] = hand->cards[i + 80];
    ((CardDrawMoveView *)hand)->mv[h].timer = 10;
    ((CardDrawMoveView *)hand)->mv[h].state = 4;
    ((CardDrawMoveView *)hand)->mv[h].pos[0] = 1.265f;
    ((CardDrawMoveView *)hand)->mv[h].pos[1] = -1.81f;
    ((CardDrawMoveView *)hand)->mv[h].pos[2] = 0.1f;
    ((CardDrawMoveView *)hand)->mv[h].vel[0] = 0.0f;
    ((CardDrawMoveView *)hand)->mv[h].vel[1] = 0.15f;
    ((CardDrawMoveView *)hand)->mv[h].vel[2] = 0.025f;
    hand->cards[i + 80] = -1;
    return 0;
}

typedef struct { u8 unk0[2]; u8 state; u8 timer; float pos[3]; float vel[3]; } CardYamaMove;
typedef struct { u8 pad[0x1D8C]; CardYamaMove mv[40]; } CardYamaView;

int CardPlayRecavery(CardHand *hand)
{
    s32 i;
    s32 y;

    for (i = 0; i < 39; i++) {
        if (hand->cards[i + 81] < 0) {
            break;
        }
    }
    if (i == 0 && hand->cards[80] < 0) {
        return 1;
    }
    y = CardPlayYamaCnt((CardPlaySide *)hand);
    hand->cards[y + 40] = hand->cards[i + 80];
    ((CardYamaView *)hand)->mv[y].timer = 10;
    ((CardYamaView *)hand)->mv[y].state = 0;
    ((CardYamaView *)hand)->mv[y].pos[0] = 1.265f;
    ((CardYamaView *)hand)->mv[y].pos[1] = -1.81f;
    ((CardYamaView *)hand)->mv[y].pos[2] = 0.02f;
    ((CardYamaView *)hand)->mv[y].vel[0] = -0.1265f;
    ((CardYamaView *)hand)->mv[y].vel[1] = 0.0f;
    ((CardYamaView *)hand)->mv[y].vel[2] = 0.0f;
    hand->cards[i + 80] = -1;
    return 0;
}

void CardPlayCleanHand(CardHand *hand)
{
    int i;
    int j;

    j = 0;
    for (i = 0; i < 40; i++) {
        if (hand->cards[i] >= 0) {
            if (i != j) {
                hand->cards[j] = hand->cards[i];
                hand->cards[i] = -1;
            }
            j++;
        }
    }
}

void CardPlayCleanYama(CardHand *hand)
{
    int i;
    int j;

    j = 0;
    for (i = 0; i < 40; i++) {
        if (hand->cards[i + 40] >= 0) {
            if (i != j) {
                hand->cards[j + 40] = hand->cards[i + 40];
                hand->cards[i + 40] = -1;
            }
            j++;
        }
    }
}

void CardPlayCleanJunk(CardHand *hand)
{
    int i;
    int j;

    j = 0;
    for (i = 0; i < 40; i++) {
        if (hand->cards[i + 120] >= 0) {
            if (i != j) {
                hand->cards[j + 120] = hand->cards[i + 120];
                hand->cards[i + 120] = -1;
            }
            j++;
        }
    }
}

extern float JunkPos1P[];
typedef struct { u8 pad[0x192C]; CardYamaMove mv[40]; } CardHandMoveView;
typedef struct { u8 pad[0x264C]; CardYamaMove mv[40]; } CardJunkMoveViewH;

int CardPlayDesertCard(CardHand *hand, int index)
{
    s32 n;

    for (n = 0; n < 40; n++) {
        if (hand->cards[n + 120] < 0) {
            break;
        }
    }
    ((CardJunkMoveViewH *)hand)->mv[n].pos[0] = ((CardHandMoveView *)hand)->mv[index].pos[0];
    ((CardJunkMoveViewH *)hand)->mv[n].pos[1] = ((CardHandMoveView *)hand)->mv[index].pos[1];
    ((CardJunkMoveViewH *)hand)->mv[n].pos[2] = ((CardHandMoveView *)hand)->mv[index].pos[2];
    hand->cards[n + 120] = hand->cards[index];
    ((CardJunkMoveViewH *)hand)->mv[n].vel[0] = (JunkPos1P[3] - ((CardJunkMoveViewH *)hand)->mv[n].pos[0]) / 10.0f;
    ((CardJunkMoveViewH *)hand)->mv[n].vel[1] = (JunkPos1P[4] - ((CardJunkMoveViewH *)hand)->mv[n].pos[1]) / 10.0f;
    ((CardJunkMoveViewH *)hand)->mv[n].vel[2] = (JunkPos1P[5] - ((CardJunkMoveViewH *)hand)->mv[n].pos[2]) / 10.0f;
    ((CardJunkMoveViewH *)hand)->mv[n].timer = 10;
    hand->cards[index] = -1;
    CardPlayCleanHand(hand);
    return 0;
}

int CardPlayDesert2Card(CardHand *hand, s32 a, s32 b)
{
    s32 n;

    for (n = 0; n < 40; n++) {
        if (hand->cards[n + 120] < 0) {
            break;
        }
    }
    hand->cards[n + 120] = hand->cards[a];
    hand->cards[a] = -1;
    for (n = 0; n < 40; n++) {
        if (hand->cards[n + 120] < 0) {
            break;
        }
    }
    hand->cards[n + 120] = hand->cards[b];
    hand->cards[b] = -1;
    CardPlayCleanHand(hand);
    return 0;
}

extern float JunkPos1P[];

typedef struct CardJunkMove {
    u8 unmodeled_00[3];
    u8 timer;
    float x;
    float y;
    float z;
    float dx;
    float dy;
    float done;
} CardJunkMove;

typedef struct { u8 pad[0x264C]; CardJunkMove junk[40]; } CardJunkView;

typedef struct { u8 pad[0x14CC]; CardYamaMove mv[40]; } CardOpMoveView;
typedef struct { u8 pad[0x264C]; CardYamaMove mv[40]; } CardJunkMoveView;

int CardPlayDesertOperation(CardPlaySide *side, int index)
{
    s32 n;

    for (n = 0; n < 40; n++) {
        if (side->hand.cards[n + 120] < 0) {
            break;
        }
    }
    ((CardJunkMoveView *)side)->mv[n].pos[0] = ((CardOpMoveView *)side)->mv[index].pos[0];
    ((CardJunkMoveView *)side)->mv[n].pos[1] = ((CardOpMoveView *)side)->mv[index].pos[1];
    ((CardJunkMoveView *)side)->mv[n].pos[2] = 0.06f;
    ((CardJunkMoveView *)side)->mv[n].vel[0] = (JunkPos1P[3] - ((CardJunkMoveView *)side)->mv[n].pos[0]) / 10.0f;
    ((CardJunkMoveView *)side)->mv[n].vel[1] = (JunkPos1P[4] - ((CardJunkMoveView *)side)->mv[n].pos[1]) / 10.0f;
    ((CardJunkMoveView *)side)->mv[n].vel[2] = 0.0f;
    ((CardJunkMoveView *)side)->mv[n].timer = 10;
    side->hand.cards[n + 120] = side->operations[index];
    side->operations[index] = -1;
    return 0;
}

#define JM(h, n) (((CardJunkView *)(h))->junk[n])

int CardPlayDesertBattle(CardHand *hand, CardLayerStack *card)
{
    s32 i;
    s32 n;

    for (n = 0; n < 40; n++) {
        if (hand->cards[n + 120] < 0) {
            break;
        }
    }
    for (i = 0; i < 40; i++) {
        if (card->layers[i].cardId >= 0) {
            JM(hand, n).x = card->position[0];
            JM(hand, n).y = card->position[1];
            JM(hand, n).z = 0.06f;
            JM(hand, n).dx = (JunkPos1P[3] - JM(hand, n).x) / 10.0f;
            JM(hand, n).dy = (JunkPos1P[4] - JM(hand, n).y) / 10.0f;
            JM(hand, n).timer = 10;
            JM(hand, n).done = 0.0f;
            hand->cards[n + 120] = card->layers[i].cardId;
            card->layers[i].cardId = -1;
            n++;
        }
    }
    return 0;
}

typedef struct { u8 pad[0x21EC]; CardYamaMove mv[40]; } CardSuteMoveView;

int CardPlayCostCard(CardHand *hand)
{
    s32 i;
    s32 s;

    for (i = 0; i < 40; i++) {
        if (hand->cards[i + 41] < 0) {
            break;
        }
    }
    for (s = 0; s < 40; s++) {
        if (hand->cards[s + 80] < 0) {
            break;
        }
    }
    if (i == 0 && hand->cards[40] < 0) {
        return 1;
    }
    hand->cards[s + 80] = hand->cards[i + 40];
    ((CardSuteMoveView *)hand)->mv[s].timer = 10;
    ((CardSuteMoveView *)hand)->mv[s].state = 0;
    ((CardSuteMoveView *)hand)->mv[s].pos[0] = 0.0f;
    ((CardSuteMoveView *)hand)->mv[s].pos[1] = -1.81f;
    ((CardSuteMoveView *)hand)->mv[s].pos[2] = 0.02f;
    ((CardSuteMoveView *)hand)->mv[s].vel[0] = 0.1265f;
    ((CardSuteMoveView *)hand)->mv[s].vel[1] = 0.0f;
    ((CardSuteMoveView *)hand)->mv[s].vel[2] = 0.0f;
    hand->cards[i + 40] = -1;
    return 0;
}

int CardPlayReturnCard(CardHand *hand, CardLayerStack *battle, s16 index)
{
    s32 i;
    s32 n;

    for (n = 0; n < 40; n++) {
        if (hand->cards[n] < 0) {
            break;
        }
    }
    for (i = 0; i < 40; i++) {
        if (((CardLayerStack *)((char *)battle + index * 0x1BC))->layers[i].cardId >= 0) {
            hand->cards[n++] = ((CardLayerStack *)((char *)battle + index * 0x1BC))->layers[i].cardId;
            ((CardLayerStack *)((char *)battle + index * 0x1BC))->layers[i].life = -1;
            ((CardLayerStack *)((char *)battle + index * 0x1BC))->layers[i].cardId = -1;
        }
    }
    ((CardLayerStack *)((char *)battle + index * 0x1BC))->flags = 0;
    ((CardLayerStack *)((char *)battle + index * 0x1BC))->power = 0xFF;
    ((CardLayerStack *)((char *)battle + index * 0x1BC))->powerLimit = 0xFF;
    return 0;
}

s32 CardPlayYamaCnt(CardPlaySide *side)
{
    s16 *card;
    s16 id;
    s32 count;
    s32 remaining;

    card = side->deck;
    count = 0;
    remaining = 0x27;
    do {
        id = *card;
        card += 1;
        remaining -= 1;
        if (id >= 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

s32 CardPlaySuteCnt(CardPlaySide *side)
{
    s16 *card;
    s16 id;
    s32 count;
    s32 remaining;

    card = side->sute;
    count = 0;
    remaining = 0x27;
    do {
        id = *card;
        card += 1;
        remaining -= 1;
        if (id >= 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

s32 CardPlayJunkCnt(CardPlaySide *side)
{
    s16 *card;
    s16 id;
    s32 count;
    s32 remaining;

    card = side->junk;
    count = 0;
    remaining = 0x27;
    do {
        id = *card;
        card += 1;
        remaining -= 1;
        if (id >= 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

s32 CardPlayOperationCnt(CardPlaySide *side)
{
    s16 *op;
    s16 id;
    s32 count;
    s32 remaining;

    op = side->operations;
    count = 0;
    remaining = 0x27;
    do {
        id = *op;
        op += 1;
        remaining -= 1;
        if (id >= 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

s32 CardPlayDisposeCnt(CardPlaySide *side)
{
    s16 *cardId;
    s16 id;
    s32 count;
    s32 remaining;

    cardId = &side->disposal[0].battle.layers[0].cardId;
    count = 0;
    remaining = 3;
    do {
        id = *cardId;
        cardId += 0xDE;
        remaining -= 1;
        if (id >= 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

s32 CardPlayHandCnt(CardHand *hand)
{
    s16 *card;
    s16 id;
    s32 count;
    s32 remaining;

    card = hand->cards;
    count = 0;
    remaining = 0x27;
    do {
        id = *card;
        card += 1;
        remaining -= 1;
        if (id >= 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

s32 CardPlayBattleCnt(CardPlaySide *side)
{
    s16 *cardId;
    s16 id;
    s32 count;
    s32 remaining;

    cardId = &side->battle[0].battle.layers[0].cardId;
    count = 0;
    remaining = 3;
    do {
        id = *cardId;
        cardId += 0xDE;
        remaining -= 1;
        if (id >= 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

void CardPlayTypeSort(CardDefinition *defs, s16 *cards, int n, s8 *types)
{
    s16 sorted[40];
    s16 backup[40];
    s32 i;
    s32 j;
    s32 t;
    s32 k;
    s32 pos;
    s16 id;

    pos = 0;
    id = 0;
    for (i = 0; i < n; i++) {
        backup[i] = cards[i];
    }
    k = 0;
    for (t = 0; t < 3; t++) {
        for (i = 0; i < n; i++) {
            for (j = 0; j < n; j++) {
                if (cards[j] >= 0 && types[t] == defs[cards[j]].kind) {
                    pos = j;
                    id = cards[j];
                    break;
                }
            }
            for (j = 0; j < n; j++) {
                if (cards[j] >= 0 && types[t] == defs[cards[j]].kind && cards[j] < id) {
                    id = cards[j];
                    pos = j;
                }
            }
            if (cards[pos] >= 0) {
                sorted[k++] = cards[pos];
                cards[pos] = -1;
            }
        }
    }
    for (i = 0; i < n; i++) {
        cards[i] = sorted[i];
    }
}

s32 CardPlayChkCost(CardDefinition *definitions, CardPlaySide *side, s32 cardId, CardGameWork *work)
{

    s32 i;
    s32 ng;


    if (definitions[cardId].kind & 1) {
        ng = 1;
        if (!(definitions[cardId].flags & 0x64)) {
            ng = 0;
            for (i = 0; i < 4; i++) {
                if (side->disposal[i].battle.layers[0].cardId < 0) {
                    ng = 1;
                    break;
                }
            }
        }
    } else {
        ng = 0;
        for (i = 0; i < 4; i++) {
            if (side->operations[i] < 0) {
                ng = 1;
                break;
            }
        }
    }
    if (ng) {
        return 0;
    }
    return 1;
}

void CardCountGeneration(s8 *gen, CardDefinition *def, CardPlaySide *side)
{
    int i;
    int k;
    s16 id;
    CardBoardSlot *slot;
    s8 *p;
    s16 *idp;

    i = 4;
    p = &gen[4];
    for (; i >= 0; i--) {
        *p = 0;
        p--;
    }
    for (i = 0; i < 4; i++) {
        slot = &side->disposal[i];
        id = slot->battle.layers[0].cardId;
        if (id >= 0 && !(slot->battle.flags & 1)) {
            k = 0;
            switch (def[id].flags) {
            case 16:
                k++;
            case 4:
                k++;
            case 8:
                k++;
            case 2:
                k++;
            }
            gen[k]++;
        }
    }
    idp = &side->battle[0].battle.layers[0].cardId;
    for (i = 0; i < 4; i++, idp += 0xDE) {
        id = *idp;
        if (id >= 0) {
            k = 0;
            switch (def[id].flags) {
            case 16:
                k++;
            case 4:
                k++;
            case 8:
                k++;
            case 2:
                k++;
            }
            gen[k]++;
        }
    }
}

int CardPlayIdentityCheck(CardGameWork *work, CardHand *hand, u16 identity);

s32 CardPlayChkCondition(CardDefinition *def, CardPlaySide *side, s32 id, CardGameWork *work)
{
    u8 buf[40];
    int ok;
    int i;
    int j;
    u8 c;
    int n;
    int cnt;
    int mask;
    s16 *idp;
    s16 cid;
    CardBoardSlot *slot;
    u8 *p;
    u8 *q;

    ok = 0;
    for (i = 0; i < 40; i++) {
        buf[i] = 0;
    }
    mask = 0;
    cnt = 0;
    for (i = 0; i < 4; i++) {
        if (def[id].unmodeled_04[i] != 0) {
            cnt++;
            mask |= def[id].unmodeled_04[i];
        }
    }
    if (mask != 0) {
        n = 0;
        idp = &side->battle[0].battle.layers[0].cardId;
        for (i = 3; i >= 0; i--) {
            if (*idp >= 0) {
                buf[n++] = def[*idp].flags;
            }
            idp += 0xDE;
        }
        slot = side->disposal;
        for (i = 3; i >= 0; i--) {
            cid = slot->battle.layers[0].cardId;
            if (cid >= 0 && !(slot->battle.flags & 1)) {
                buf[n++] = def[cid].flags;
            }
            slot++;
        }
        mask = 0;
        for (i = 0; i < cnt; i++) {
            for (j = 0; j < n; j++) {
                if (def[id].unmodeled_04[i] & buf[j]) {
                    buf[j] = 0;
                    mask++;
                    break;
                }
            }
        }
        if (mask >= cnt) {
            ok = 1;
        }
    } else {
        ok = 1;
    }
    if (!ok) {
        return 2;
    }
    ok = 0;
    for (i = 0; i < 40; i++) {
        buf[i] = 0;
    }
    mask = 0;
    cnt = 0;
    for (i = 0; i < 4; i++) {
        if (def[id].unmodeled_04[i + 8] != 0) {
            cnt++;
            mask |= def[id].unmodeled_04[i + 8];
        }
    }
    if (mask != 0) {
        n = 0;
        idp = &side->battle[0].battle.layers[0].cardId;
        for (i = 3; i >= 0; i--) {
            if (*idp >= 0) {
                buf[n++] = def[*idp].flags;
            }
            idp += 0xDE;
        }
        idp = &side->disposal[0].battle.layers[0].cardId;
        for (i = 3; i >= 0; i--) {
            if (*idp >= 0) {
                buf[n++] = def[*idp].flags;
            }
            idp += 0xDE;
        }
        mask = 0;
        for (i = 0; i < cnt; i++) {
            for (j = 0; j < n; j++) {
                if (def[id].unmodeled_04[i + 8] & buf[j]) {
                    buf[j] = 0;
                    mask++;
                }
            }
        }
        if (mask >= cnt) {
            ok = 1;
        }
    } else {
        ok = 1;
    }
    if (!ok) {
        return 4;
    }
    if ((def[id].kind & 1) && CardPlayIdentityCheck(work, &side->hand, work->definitions[id].identity)) {
        return 5;
    }
    return 0;
}

s32 CardChkAttackType(CardDefinition *definitions, CardPowerCard *card)
{
    s16 *cardId;
    s16 id;
    s32 type;
    s32 remaining;

    cardId = &((CardLayerStack *)card)->layers[39].cardId;
    type = 0;
    remaining = 0x27;
    do {
        id = *cardId;
        cardId -= 5;
        if (id < 0) {
            continue;
        }
        if (id == 0x2F) {
            continue;
        }
        if (id == 0x2C) {
            continue;
        }
        type = definitions[id].unmodeled_13;
        break;
    } while (--remaining >= 0);
    return type;
}

typedef struct CPCMCursor {
    u8 unmodeled_00[0xC];
    s16 row;
    s16 col;
    float scale[4];
    float pos[3];
} CPCMCursor;

void CPCMSub(CPCMCursor *cursor, CardPlaySide *side, u16 pressed, u16 held, int lock)
{
    float mat[4][4];
    float trans[4];
    u16 row;
    s16 id;
    int neg;
    s16 r;

    if (cursor->row < 0) {
        cursor->row = 0;
    }
    if (cursor->row >= 4) {
        cursor->row = 3;
    }
    cursor->scale[3] = 1.0f;
    cursor->scale[2] = 1.0f;
    if (cursor->row == 0) {
        cursor->scale[1] = 0.175f;
        cursor->scale[0] = 0.175f;
        if (held & 0x10) {
            id = side->disposal[cursor->col].battle.layers[0].cardId & 0xF0FF;
            if (id >= 0) {
                trans[2] = 0.1f;
                trans[0] = 0.0f;
                trans[1] = 1.0f;
                trans[3] = 1.0f;
                xglMatrixUnit(mat);
                xglMatrixTrans(mat, mat, trans);
                CardDisp(mat, id, 1, 1);
            }
        }
    } else if (cursor->row == 3) {
        cursor->scale[1] = 0.175f;
        cursor->scale[0] = 0.175f;
    } else {
        cursor->scale[1] = 0.355f;
        cursor->scale[0] = 0.355f;
        if (lock == 0 && (pressed & 0x4000)) {
            cursor->col = 2;
        } else if (cursor->col == 1 || cursor->col == 3) {
            cursor->col = cursor->col & 0xFE;
        }
    }
}

extern float *CursorTbl_00A43420[];
extern float HandPos1P[];

void CardPlayCursorMove(CPCMCursor *cursor, CardHand *hand, u16 pressed, u16 held)
{
    s16 col;
    s32 n;
    float *pos;

    col = cursor->col;
    if (pressed & 0x1000) {
        cursor->col--;
    }
    if (pressed & 0x2000) {
        cursor->row++;
    }
    if (pressed & 0x4000) {
        cursor->col++;
    }
    if (pressed & 0x8000) {
        cursor->row--;
    }
    if (cursor->col < 0) {
        cursor->col = 0;
    }
    if (cursor->col >= 5) {
        cursor->col = 4;
    }
    if (cursor->col < 4) {
        CPCMSub(cursor, (CardPlaySide *)hand, pressed, held, col);
        pos = CursorTbl_00A43420[cursor->col * 4 + cursor->row];
        cursor->pos[0] = pos[0];
        cursor->pos[1] = pos[1];
        cursor->pos[2] = pos[2];
        return;
    }
    n = CardPlayHandCnt(hand);
    cursor->scale[1] = 0.175f;
    cursor->scale[3] = 1.0f;
    cursor->scale[0] = 0.175f;
    cursor->scale[2] = 1.0f;
    if (n == 0) {
        cursor->col = 3;
        CPCMSub(cursor, (CardPlaySide *)hand, pressed, held, col);
        pos = CursorTbl_00A43420[cursor->col * 4 + cursor->row];
        cursor->pos[0] = pos[0];
        cursor->pos[1] = pos[1];
        cursor->pos[2] = pos[2];
        return;
    }
    if (cursor->row < 0) {
        cursor->row = 0;
    }
    if (cursor->row >= n) {
        cursor->row = n - 1;
    }
    cursor->pos[0] = HandPos1P[0] - (HandPos1P[0] - HandPos1P[9]) / n * cursor->row;
    cursor->pos[1] = HandPos1P[1];
    cursor->pos[2] = HandPos1P[2];
}

extern char DeckListStr[][16];

typedef struct { u8 pad[8]; CardListInfo *info; u8 padC[4]; s16 cnt[1]; } CardListSrc;
extern char D_00A4D500[];
extern char D_00A4D508[];
void CardListDisp(CardListWin *w, CardListSrc *src)
{
    char buf[32];
    char num[16];
    CardListInfo *info;
    s16 *cnts;
    int i;
    int j;
    int id;
    int n;
    int off;
    char *name;

    i = 0;
    info = src->info;
    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D500);
    cnts = src->cnt;
    for (; i < w->count; i++) {
        if (i < w->num) {
            id = w->ent[i + w->top].id;
            if (src->cnt[id] > 0) {
                off = id * sizeof(CardListInfo);
                name = (char *)(((CardListInfo *)((char *)info + off))->name + 0x01000000);
                if (i == w->sel) {
                    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D148);
                } else {
                    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D150);
                }
                for (j = 0; j < w->len * 2; j++) {
                    buf[j] = name[j];
                }
                buf[j] = 0;
                CardNameConvert((unsigned char *)buf);
                xglFontPrint(0, 0, 0, D_00A4D158);
                xglFontPrint(w->x, w->y + i * 25, 0xFFF0, buf);
                if (w->flags & 1) {
                    n = cnts[id];
                    if (n >= 10) {
                        n = 9;
                    }
                    sprintf(num, D_00A4D168, n);
                    if (i == w->sel) {
                        xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D148);
                    } else {
                        xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D160);
                    }
                    xglFontPrint(w->x + w->len * 10 + 6, w->y + i * 25, 0xFFF0, num);
                }
            } else {
                if (i == w->sel) {
                    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D148);
                } else {
                    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D150);
                }
                xglFontPrint(w->x, w->y + i * 25, 0xFFF0, D_00A4D508);
            }
        }
    }
    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D170);
}

extern char D_00A4D518[];

void CardDeckListDisp(CardListWin *w)
{
    char buf[32];
    int j;
    int i;
    char *src;

    i = 0;
    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D518);
    buf[0] = 0x19;
    buf[1] = 3;
    for (; i < w->count; i++) {
        if (i < w->num) {
            src = DeckListStr[i];
            if (i == w->sel) {
                xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D148);
            } else {
                xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D150);
            }
            for (j = 0; j < w->len; j++) {
                buf[j + 2] = src[j];
            }
            buf[j + 2] = 0;
            xglFontPrint(w->x, w->y + i * 0x18, 0xFFF0, buf);
        }
    }
    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D170);
}

void CardStrListDisp(CardListWin *w, char **strs, int sel)
{
    char buf[32];
    int j;
    int i;
    char *src;

    i = 0;
    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D518);
    buf[0] = 0x19;
    buf[1] = 3;
    for (; i < w->count; i++) {
        if (i < w->num) {
            src = strs[i];
            if (i == sel) {
                xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D148);
            } else {
                xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D150);
            }
            for (j = 0; j < w->len; j++) {
                buf[j + 2] = src[j];
            }
            buf[j + 2] = 0;
            xglFontPrint(w->x, w->y + i * 0x19, 0xFFF0, buf);
        }
    }
    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D170);
}

extern char D_00A4D500[];

void CardEnemyListDisp(CardListWin *w, int *ids, int sel)
{
    char buf[32];
    int j;
    int i;
    char *src;

    i = 0;
    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D500);
    for (; i < w->count; i++) {
        if (i < w->num) {
            src = DeckListStr[ids[i]];
            if (i == sel) {
                xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D148);
            } else {
                xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D150);
            }
            for (j = 0; j < w->len; j++) {
                buf[j] = src[j];
            }
            buf[j] = 0;
            xglFontPrint(w->x, w->y + i * 0x18, 0xFFF0, buf);
        }
    }
    xglFontPrint(w->x, w->y, 0xFFF0, D_00A4D170);
}

/*
 * CGPCalcPowerPlus (ov10/tu008 cgp.c, already accepted C) takes CardPowerCard
 * *card, declared there a second time from the same memory CardLayerStack
 * covers because its own loads at +0x08..+0x0A are signed where
 * CardBattleLayer's accepted accesses of the same bytes are unsigned
 * (ov10/cgp.h's own comment on CardPowerCard). CardPlayCntPower always
 * passes a whole CardLayerStack's address, so the call below casts to the
 * accepted definition's own parameter type instead of redeclaring the
 * extern with a different one.
 */
extern s32 CGPCalcPowerPlus(CardGameWork *work, CardPowerCard *card, s32 power);

s32 CardPlayCntPower(CardGameWork *work, CardLayerStack *slot)
{
    CardDefinition *def;
    s16 topCardId;
    s32 power;

    power = 0;
    if (!(slot->flags & 0x80)) {
        def = &work->definitions[slot->layers[0].cardId];
        topCardId = slot->layers[1].cardId;
        if (topCardId >= 0 && topCardId != 0x35 && topCardId != 0x2F && topCardId != 0x2C) {
            def = &work->definitions[topCardId];
        }
        power = CGPCalcPowerPlus(work, (CardPowerCard *) slot, def->attackPower);
    }
    return power;
}

INCLUDE_ASM("asm/nonmatchings/ov10/card_fread", CardPlayDispBattle);

INCLUDE_ASM("asm/nonmatchings/ov10/card_fread", CardPlayDispHand);

void CardPlayDispJunk(int unused, CardPlaySide *hand, float (*place)[4])
{
    float mat[4][4];
    float trans[4];
    float scale[4];
    int n;
    int i;
    int v;
    int last;
    void *model;

    n = CardPlayJunkCnt(hand);
    scale[1] = 1.4f;
    scale[2] = 1.0f;
    scale[3] = 1.0f;
    scale[0] = 1.4f;
    i = n / 10;
    trans[2] = 0.01f;
    trans[3] = 1.0f;
    if (i != 0) {
        last = 0x01C80000 + (i << 10);
        trans[0] = 1.919f;
        trans[1] = -1.876f;
        xglMatrixTrans(mat, place, trans);
        xglMatrixScale(mat, mat, scale);
        nmlModelSetPlace(mat);
        nmlModelSetTexture((char *)0x01C91000);
        nmlModelEntryCard((void *)last);
    }
    v = n % 10;
    last = 0x01C80000 + (v << 10);
    trans[0] = 2.033f;
    trans[1] = -1.876f;
    xglMatrixTrans(mat, place, trans);
    xglMatrixScale(mat, mat, scale);
    nmlModelSetPlace(mat);
    nmlModelSetTexture((char *)0x01C91000);
    nmlModelEntryCard((void *)last);
    if (n == 0) {
        return;
    }
    last = -1;
    trans[3] = 1.0f;
    scale[1] = 0.175f;
    scale[0] = 0.175f;
    scale[3] = 1.0f;
    scale[2] = 1.0f;
    for (i = 0; i < n; i++) {
        if (((CardJunkMoveViewH *)hand)->mv[i].timer == 0) {
            last = i;
        }
    }
    if (last >= 0) {
        trans[0] = JunkPos1P[0];
        trans[1] = JunkPos1P[1];
        trans[2] = JunkPos1P[2] - 0.02f;
        xglMatrixTrans(mat, place, trans);
        xglMatrixScale(mat, mat, scale);
        CardDispCard(mat, hand->hand.cards[last + 120] & 0xF0FF, 1);
    }
    for (i = 0; i < n; i++) {
        if (((CardJunkMoveViewH *)hand)->mv[i].timer != 0) {
            ((CardJunkMoveViewH *)hand)->mv[i].timer--;
            if (((CardJunkMoveViewH *)hand)->mv[i].timer == 0) {
                xglSoundEffectNormalID(0x2000F, 0);
            }
            ((CardJunkMoveViewH *)hand)->mv[i].pos[0] += ((CardJunkMoveViewH *)hand)->mv[i].vel[0];
            ((CardJunkMoveViewH *)hand)->mv[i].pos[1] += ((CardJunkMoveViewH *)hand)->mv[i].vel[1];
            ((CardJunkMoveViewH *)hand)->mv[i].pos[2] += ((CardJunkMoveViewH *)hand)->mv[i].vel[2];
            trans[0] = ((CardJunkMoveViewH *)hand)->mv[i].pos[0];
            trans[1] = ((CardJunkMoveViewH *)hand)->mv[i].pos[1];
            trans[2] = ((CardJunkMoveViewH *)hand)->mv[i].pos[2];
            xglMatrixTrans(mat, place, trans);
            xglMatrixScale(mat, mat, scale);
            CardDispCard(mat, hand->hand.cards[i + 120] & 0xF0FF, 1);
        } else if (i == n - 1) {
            ((CardJunkMoveViewH *)hand)->mv[i].pos[0] = JunkPos1P[0];
            trans[0] = ((CardJunkMoveViewH *)hand)->mv[i].pos[0];
            ((CardJunkMoveViewH *)hand)->mv[i].pos[1] = JunkPos1P[1];
            trans[1] = ((CardJunkMoveViewH *)hand)->mv[i].pos[1];
            ((CardJunkMoveViewH *)hand)->mv[i].pos[2] = JunkPos1P[2];
            trans[2] = ((CardJunkMoveViewH *)hand)->mv[i].pos[2];
            xglMatrixTrans(mat, place, trans);
            xglMatrixScale(mat, mat, scale);
            CardDispCard(mat, hand->hand.cards[i + 120] & 0xF0FF, 1);
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ov10/card_fread", CardPlayDispDispose);

extern float OperationPos1P[][3];

void CardPlayDispOperation(int unused, CardPlaySide *side, float (*place)[4])
{
    float mat[4][4];
    float trans[4];
    float scale[4];
    int i;

    if (CardPlayOperationCnt(side) == 0) {
        return;
    }
    trans[3] = 1.0f;
    scale[1] = 0.175f;
    scale[0] = 0.175f;
    scale[3] = 1.0f;
    scale[2] = 1.0f;
    for (i = 0; i < 4; i++) {
        if (side->operations[i] < 0) {
            continue;
        }
        if (((CardOpMoveView *)side)->mv[i].timer != 0) {
            ((CardOpMoveView *)side)->mv[i].timer--;
            if (((CardOpMoveView *)side)->mv[i].timer == 0) {
                xglSoundEffectNormalID(0x2000F, 0);
            }
            ((CardOpMoveView *)side)->mv[i].pos[0] += ((CardOpMoveView *)side)->mv[i].vel[0];
            ((CardOpMoveView *)side)->mv[i].pos[1] += ((CardOpMoveView *)side)->mv[i].vel[1];
            ((CardOpMoveView *)side)->mv[i].pos[2] += ((CardOpMoveView *)side)->mv[i].vel[2];
        } else {
            ((CardOpMoveView *)side)->mv[i].pos[0] = OperationPos1P[i][0];
            ((CardOpMoveView *)side)->mv[i].pos[1] = OperationPos1P[i][1];
            ((CardOpMoveView *)side)->mv[i].pos[2] = OperationPos1P[i][2];
        }
        trans[0] = ((CardOpMoveView *)side)->mv[i].pos[0];
        trans[1] = ((CardOpMoveView *)side)->mv[i].pos[1];
        trans[2] = ((CardOpMoveView *)side)->mv[i].pos[2];
        xglMatrixTrans(mat, place, trans);
        xglMatrixScale(mat, mat, scale);
        CardDispCard(mat, side->operations[i] & 0xF0FF, 1);
        nmlModelSetPlace(mat);
        nmlModelSetTexture((char *)0x01717600);
        nmlModelEntryCard((void *)0x01716600);
        if (((CardOpMoveView *)side)->mv[i].timer == 0 && (side->operationFlags[i] & 1)) {
            trans[0] = OperationPos1P[i][0];
            trans[1] = OperationPos1P[i][1];
            trans[2] = 0.01f;
            xglMatrixTrans(mat, place, trans);
            nmlModelSetPlace(mat);
            nmlModelSetTexture((char *)0x01F70200);
            nmlModelEntryCard((void *)0x01900400);
        }
    }
}

extern void xglMatrixRotY(float (*destination)[4], float (*source)[4], float angle);
extern char CMColor[];

void CardPlayDispYama(int side, CardHand *hand, float (*place)[4])
{
    float mat[4][4];
    float trans[4];
    float scale[4];
    int cnt;
    int n;
    int i;
    int v;
    void *model;

    cnt = hand->unmodeled_04[0];
    n = cnt;
    for (i = 0; i < cnt; i++) {
        if (((CardYamaView *)hand)->mv[i].timer != 0) {
            n--;
        }
    }
    if (n < 0) {
        n = 0;
    }
    i = n / 10;
    trans[2] = 0.01f;
    trans[3] = 1.0f;
    scale[1] = 1.7f;
    scale[0] = 1.7f;
    scale[2] = 1.0f;
    scale[3] = 1.0f;
    if (i != 0) {
        model = (void *)(0x01C80000 + (i << 10));
        trans[0] = 0.401f;
        trans[1] = -1.855f;
        xglMatrixTrans(mat, place, trans);
        xglMatrixScale(mat, mat, scale);
        nmlModelSetPlace(mat);
        nmlModelSetTexture((char *)0x01C91000);
        nmlModelEntryCard(model);
    }
    v = n % 10;
    model = (void *)(0x01C80000 + (v << 10));
    trans[0] = 0.547f;
    trans[1] = -1.855f;
    xglMatrixTrans(mat, place, trans);
    xglMatrixScale(mat, mat, scale);
    nmlModelSetPlace(mat);
    nmlModelSetTexture((char *)0x01C91000);
    nmlModelEntryCard(model);
    if (n != 0) {
        trans[0] = 0.0f;
        trans[1] = -1.81f;
        xglMatrixTrans(mat, place, trans);
        nmlModelSetPlace(mat);
        nmlModelSetLight(CMColor, asDir);
        nmlModelSetTexture((char *)0x016E0000);
        if (side == 0) {
            nmlModelEntryCard((void *)0x01911800);
        } else {
            nmlModelEntryCard((void *)0x01915800);
        }
        trans[0] = -0.484f;
        trans[1] = -1.851f;
        xglMatrixTrans(mat, place, trans);
        nmlModelSetPlace(mat);
        nmlModelSetTexture((char *)0x01D80000);
        if (side == 0) {
            nmlModelEntryCard((void *)0x01D18000);
        } else {
            nmlModelEntryCard((void *)0x01D1C000);
        }
    }
    if (cnt >= 0) {
        for (i = 0; i < cnt; i++) {
            if (((CardYamaView *)hand)->mv[i].timer != 0) {
                ((CardYamaView *)hand)->mv[i].timer--;
                if (((CardYamaView *)hand)->mv[i].state == 0) {
                    if (((CardYamaView *)hand)->mv[i].timer == 0) {
                        xglSoundEffectNormalID(0x2000F, 0);
                    }
                    ((CardYamaView *)hand)->mv[i].pos[0] += ((CardYamaView *)hand)->mv[i].vel[0];
                    ((CardYamaView *)hand)->mv[i].pos[1] += ((CardYamaView *)hand)->mv[i].vel[1];
                    ((CardYamaView *)hand)->mv[i].pos[2] += ((CardYamaView *)hand)->mv[i].vel[2];
                    trans[3] = 1.0f;
                    scale[1] = 0.175f;
                    scale[0] = 0.175f;
                    trans[0] = ((CardYamaView *)hand)->mv[i].pos[0];
                    trans[1] = ((CardYamaView *)hand)->mv[i].pos[1];
                    trans[2] = ((CardYamaView *)hand)->mv[i].pos[2];
                    scale[3] = 1.0f;
                    scale[2] = 1.0f;
                    xglMatrixTrans(mat, place, trans);
                    xglMatrixScale(mat, mat, scale);
                    xglMatrixRotY(mat, mat, 3.1415927f);
                    CardDispCard(mat, hand->cards[i + 40], 1);
                }
            } else if (((CardYamaView *)hand)->mv[i].state != 0) {
                ((CardYamaView *)hand)->mv[i].state = 0;
                ((CardYamaView *)hand)->mv[i].timer = 10;
            }
        }
    }
}

extern void xglMatrixRotY(float (*destination)[4], float (*source)[4], float angle);

void CardPlayDispSute(int unused, CardHand *hand, float (*place)[4])
{
    float mat[4][4];
    float trans[4];
    float scale[4];
    int cnt;
    int n;
    int i;
    int v;
    void *model;

    cnt = hand->unmodeled_04[1];
    n = cnt;
    for (i = 0; i < cnt; i++) {
        if (((CardSuteMoveView *)hand)->mv[i].timer != 0 || ((CardSuteMoveView *)hand)->mv[i].state != 0) {
            n--;
        }
    }
    if (n < 0) {
        n = 0;
    }
    i = n / 10;
    trans[2] = 0.01f;
    trans[3] = 1.0f;
    scale[1] = 1.4f;
    scale[0] = 1.4f;
    scale[2] = 1.0f;
    scale[3] = 1.0f;
    if (i != 0) {
        model = (void *)(0x01C80000 + (i << 10));
        trans[0] = 0.889f;
        trans[1] = -1.876f;
        xglMatrixTrans(mat, place, trans);
        xglMatrixScale(mat, mat, scale);
        nmlModelSetPlace(mat);
        nmlModelSetTexture((char *)0x01C91000);
        nmlModelEntryCard(model);
    }
    v = n % 10;
    model = (void *)(0x01C80000 + (v << 10));
    trans[0] = 1.0f;
    trans[1] = -1.876f;
    xglMatrixTrans(mat, place, trans);
    xglMatrixScale(mat, mat, scale);
    nmlModelSetPlace(mat);
    nmlModelSetTexture((char *)0x01C91000);
    nmlModelEntryCard(model);
    if (n != 0 && (n != 1 || ((CardSuteMoveView *)hand)->mv[0].timer == 0 || ((CardSuteMoveView *)hand)->mv[0].state == 0)) {
        trans[0] = 1.265f;
        trans[1] = -1.81f;
        scale[3] = 1.0f;
        scale[1] = 1.0f;
        scale[0] = 1.0f;
        scale[2] = 1.0f;
        xglMatrixTrans(mat, place, trans);
        xglMatrixScale(mat, mat, scale);
        nmlModelSetPlace(mat);
        nmlModelSetTexture((char *)0x016E0000);
        nmlModelEntryCard((void *)0x01901800);
    }
    if (cnt != 0) {
        for (i = 0; i < cnt; i++) {
            if (((CardSuteMoveView *)hand)->mv[i].timer != 0) {
                ((CardSuteMoveView *)hand)->mv[i].timer--;
                if (((CardSuteMoveView *)hand)->mv[i].timer == 0) {
                    xglSoundEffectNormalID(0x2000F, 0);
                }
                if (((CardSuteMoveView *)hand)->mv[i].state == 0) {
                    ((CardSuteMoveView *)hand)->mv[i].pos[0] += ((CardSuteMoveView *)hand)->mv[i].vel[0];
                    ((CardSuteMoveView *)hand)->mv[i].pos[1] += ((CardSuteMoveView *)hand)->mv[i].vel[1];
                    ((CardSuteMoveView *)hand)->mv[i].pos[2] += ((CardSuteMoveView *)hand)->mv[i].vel[2];
                    trans[3] = 1.0f;
                    scale[1] = 0.175f;
                    scale[0] = 0.175f;
                    trans[0] = ((CardSuteMoveView *)hand)->mv[i].pos[0];
                    trans[1] = ((CardSuteMoveView *)hand)->mv[i].pos[1];
                    trans[2] = ((CardSuteMoveView *)hand)->mv[i].pos[2];
                    scale[3] = 1.0f;
                    scale[2] = 1.0f;
                    xglMatrixTrans(mat, place, trans);
                    xglMatrixScale(mat, mat, scale);
                    xglMatrixRotY(mat, mat, 3.1415927f);
                    CardDispCard(mat, hand->cards[i + 80], 1);
                }
            } else if (((CardSuteMoveView *)hand)->mv[i].state != 0) {
                ((CardSuteMoveView *)hand)->mv[i].state = 0;
                ((CardSuteMoveView *)hand)->mv[i].timer = 10;
            }
        }
    }
}

void CardCursorPassive(CardCursorControl *cursor)
{
    u8 *a;
    u8 *b;
    s32 i;

    b = (u8 *)cursor + 0x48;
    a = (u8 *)cursor + 0x30;
    for (i = 0; i < 0x18; i++) {
        *a++ = 0;
        *b++ = 0;
    }
}

typedef struct CardCursorPos {
    s16 zone;
    s16 index;
    s16 cardId;
    u8 unmodeled_06[2];
} CardCursorPos;
typedef struct { u8 pad[0x30]; u8 active[0x18]; } CardCursorView;

void CardCursorActive(CardCursorControl *cursor, CardCursorPos *position, int mode)
{
    int base;

    base = -1;
    switch (position->zone) {
    case 3:
        base = 0x14;
        break;
    case 4:
        base = 0x10;
        break;
    case 2:
        base = 0xC;
        break;
    case 7:
        base = 8;
        break;
    case 5:
        base = 4;
        break;
    case 6:
        base = 0;
        break;
    }
    if (base >= 0) {
        switch (mode) {
        case 0:
        default:
            ((CardCursorView *)cursor)->active[base + position->index] = 0;
            break;
        case 1:
            ((CardCursorView *)cursor)->active[base + position->index] = 1;
            break;
        case 2:
            ((CardCursorView *)cursor)->active[base + position->index] = 2;
            break;
        case 3:
            ((CardCursorView *)cursor)->active[base + position->index] = 3;
            break;
        case 4:
            ((CardCursorView *)cursor)->active[base + position->index] = 4;
            break;
        }
    }
}

/* Maps a cursor-area code to a fixed cursor model resource address;
 * codes other than 2, 3 and 4 (including 0 and 1) share the same address. */
int CAMASub(int areaCode) {
    int code;
    int addr;

    code = areaCode & 0xFF;
    addr = 0x01D0C000;
    if ((code != 2) &&
        ((code < 3) ||
         (((addr = 0x01D10000), (code != 3)) &&
          ((addr = 0x01D12000), (code != 4))))) {
        addr = 0x01D08000;
    }
    return addr;
}

INCLUDE_ASM("asm/nonmatchings/ov10/card_fread", CardPlayDispCursor);

/*
 * CardPlayIdentityCheck (ov10:0x00a10b60): `hand` is a side's CardHand,
 * CardPlaySide's first member (ov10/cgp.h), so it is also the address of the
 * enclosing side; this walks that side's four battle and four disposal board
 * slots, at every stacked layer, for a card whose CardDefinition.identity
 * matches. `identity` masks to 16 bits at entry (`andi $a2,$a2,0xffff`, the
 * function's first instruction) rather than to a byte, so the parameter
 * itself is 16 bits wide; ccu_038_exec_sub.c's extern still declares it
 * `u8` and needs updating to match this definition.
 */
int CardPlayIdentityCheck(CardGameWork *work, CardHand *hand, u16 identity)
{
    s16 *cardIdSlot;    /* walks disposal[3]'s cardId column, one layer per iteration */
    int layer;
    int found;
    s16 cardId;

    found = 0;
    if (identity < 0x80) {
        cardIdSlot = (s16 *)((u8 *)hand + 0xDCE); /* &((CardPlaySide *)hand)->disposal[3].battle.layers[0].cardId */

        /*
         * battle[0..3] and disposal[0..3] are 8 consecutive CardBoardSlot
         * records; each check below reads the same layer of a different
         * slot, at a fixed distance (a multiple of sizeof(CardBoardSlot))
         * behind cardIdSlot's current slot, disposal[3].
         */
        layer = 0;
        do {
            cardId = *(s16 *)((u8 *)cardIdSlot - 7 * sizeof(CardBoardSlot)); /* battle[0] */
            if (cardId >= 0 && work->definitions[cardId].identity == identity) {
                found = 1;
                break;
            }
            cardId = *(s16 *)((u8 *)cardIdSlot - 6 * sizeof(CardBoardSlot)); /* battle[1] */
            if (cardId >= 0 && work->definitions[cardId].identity == identity) {
                found = 1;
                break;
            }
            cardId = *(s16 *)((u8 *)cardIdSlot - 5 * sizeof(CardBoardSlot)); /* battle[2] */
            if (cardId >= 0 && work->definitions[cardId].identity == identity) {
                found = 1;
                break;
            }
            cardId = *(s16 *)((u8 *)cardIdSlot - 4 * sizeof(CardBoardSlot)); /* battle[3] */
            if (cardId >= 0 && work->definitions[cardId].identity == identity) {
                found = 1;
                break;
            }
            cardId = *(s16 *)((u8 *)cardIdSlot - 3 * sizeof(CardBoardSlot)); /* disposal[0] */
            if (cardId >= 0 && work->definitions[cardId].identity == identity) {
                found = 1;
                break;
            }
            cardId = *(s16 *)((u8 *)cardIdSlot - 2 * sizeof(CardBoardSlot)); /* disposal[1] */
            if (cardId >= 0 && work->definitions[cardId].identity == identity) {
                found = 1;
                break;
            }
            cardId = *(s16 *)((u8 *)cardIdSlot - sizeof(CardBoardSlot));    /* disposal[2] */
            if (cardId >= 0 && work->definitions[cardId].identity == identity) {
                found = 1;
                break;
            }
            cardId = *cardIdSlot;                                          /* disposal[3] */
            if (cardId >= 0 && work->definitions[cardId].identity == identity) {
                found = 1;
                break;
            }
            cardIdSlot = (s16 *)((u8 *)cardIdSlot + sizeof(CardBattleLayer));
            layer++;
        } while (layer < 40);
    }
    return found;
}

extern float win0pos[4];

void CGCPHandDispSubCore(CardHand *hand, int sel, float (*place)[4], int side)
{
    float mat[4][4];
    float trans[4];
    float scale1[4];
    float scale2[4];
    float step;
    float f;
    int n;
    int i;

    n = CardPlayHandCnt(hand);
    if (n < 2) {
        step = (win0pos[3] - win0pos[0]) * 0.5f;
    } else {
        step = (win0pos[3] - win0pos[0]) / (float)(n - 1);
    }
    i = 0;
    trans[3] = 1.0f;
    scale1[0] = 0.4f;
    scale1[2] = 1.0f;
    scale1[3] = 1.0f;
    scale2[0] = 0.5f;
    trans[1] = win0pos[1];
    trans[2] = win0pos[2];
    scale2[2] = 1.0f;
    scale1[1] = 0.4f;
    scale2[1] = 0.5f;
    scale2[3] = 1.0f;
    for (; i < n; i++) {
        f = (float)i;
        trans[0] = win0pos[0] + step * f;
        trans[2] = win0pos[2] + f * 0.015f;
        if (i == sel) {
            trans[2] = win0pos[2] + (float)(n + 1) * 0.015f;
        }
        xglMatrixTrans(mat, place, trans);
        if (i == sel) {
            xglMatrixScale(mat, mat, scale2);
            CardDisp(mat, hand->cards[i], 1, 0);
        } else {
            xglMatrixScale(mat, mat, scale1);
            CardDispCard(mat, hand->cards[i], 1);
        }
    }
    nmlModelSetTexture((char *)0x01DB5800);
    nmlModelSetPlace(place);
    nmlModelEntryCard((void *)0x01DA0000);
}

extern void nmlModelSetLight(void *colorMod, void *normal);
extern void nmlModelSetPlace(void *m);
extern void nmlModelSetTexture(const char *texture);
extern void nmlModelEntryCard(void *model);
extern void xglMatrixTrans(float (*destination)[4], float (*source)[4], const float translation[4]);
extern void xglMatrixScale(float (*destination)[4], float (*source)[4], const float scale[4]);
extern char HalfColor[];
extern char asDir[];

void CGCPNeedDispSub(int side, u8 *types, float (*place)[4], u8 *gen)
{
    float base[4][4];
    float mat[4][4];
    float trans[4];
    float scale[4];
    int i;
    int j;
    int k;
    int special;
    int found;
    void *model;

    if (side == 0) {
        trans[0] = -0.145f;
        trans[1] = 0.408f;
    } else {
        trans[0] = 0.212f;
        trans[1] = -0.686f;
    }
    trans[2] = 0.0f;
    trans[3] = 1.0f;
    scale[0] = 0.9f;
    scale[1] = 0.9f;
    scale[3] = 1.0f;
    scale[2] = 1.0f;
    xglMatrixTrans(base, place, trans);
    trans[0] = 0.155f;
    trans[1] = trans[2] = 0.0f;
    for (i = 0; i < 4; i++) {
        special = 0;
        k = 0;
        switch (types[i]) {
        case 1:
            break;
        case 0x1F:
            special = 1;
            k = 3;
        case 0x10:
            k++;
        case 4:
            k++;
        case 8:
            k++;
        case 2:
            k++;
            break;
        default:
            k = -1;
            break;
        }
        if (k >= 0) {
            model = (void *)(0x01C87400 + (k << 10));
            xglMatrixScale(mat, base, scale);
            nmlModelSetPlace(mat);
            if (special) {
                found = 0;
                for (j = 0; j < 5; j++) {
                    if (gen[j] != 0) {
                        gen[j]--;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    nmlModelSetLight(HalfColor, asDir);
                }
            } else if (gen[k] != 0) {
                gen[k]--;
            } else {
                nmlModelSetLight(HalfColor, asDir);
            }
            nmlModelSetTexture((char *)0x01C91000);
            nmlModelEntryCard(model);
            xglMatrixTrans(base, base, trans);
        }
    }
}

void CGCPGeneDispSub(int side, CardDefinition *def, CardPlaySide *play, float (*place)[4], u8 *gen)
{
    float base[4][4];
    float mat[4][4];
    float trans[4];
    float scale[4];
    int i;
    int j;
    int off;

    if (side == 0) {
        trans[0] = -0.145f;
        trans[1] = 0.675f;
    } else {
        trans[0] = 0.212f;
        trans[1] = -0.418f;
    }
    trans[2] = 0.0f;
    trans[3] = 1.0f;
    scale[0] = 0.9f;
    scale[1] = 0.9f;
    scale[3] = 1.0f;
    scale[2] = 1.0f;
    xglMatrixTrans(base, place, trans);
    trans[0] = 0.155f;
    trans[1] = trans[2] = 0.0f;
    CardCountGeneration((s8 *)gen, def, play);
    for (i = 0; i < 5; i++) {
        if (gen[i] != 0) {
            for (j = 0; j < gen[i]; j++) {
                off = i << 10;
                xglMatrixScale(mat, base, scale);
                nmlModelSetPlace(mat);
                nmlModelSetTexture((char *)0x01C91000);
                nmlModelEntryCard((void *)(0x01C87400 + off));
                xglMatrixTrans(base, base, trans);
            }
        }
    }
}

extern float NewWin0pos[];
extern char CLColor[];

void CGCPHandDispSub3(CardDefinition *definitions, CardHand *hand, int selected, int mode, int option)
{
    float base[4][4];
    float tmp[4][4];
    float mat[4][4];
    float trans[4];
    float scale1[4];
    float scale2[4];
    u8 gen[16];
    float step;
    float f;
    int n;
    int i;
    int flag;

    xglMatrixUnit(base);
    trans[0] = 0.0f;
    trans[1] = 1.5f;
    if (option) {
        trans[1] = 1.74f;
    }
    trans[2] = 0.15f;
    scale1[1] = 0.97f;
    trans[3] = 1.0f;
    scale1[0] = 1.0f;
    scale1[2] = 1.0f;
    scale1[3] = 1.0f;
    xglMatrixTrans(base, base, trans);
    xglMatrixScale(base, base, scale1);
    n = CardPlayHandCnt(hand);
    if (n < 2) {
        step = (NewWin0pos[9] - NewWin0pos[6]) * 0.5f;
    } else {
        step = (NewWin0pos[9] - NewWin0pos[6]) / (float)(n - 1);
    }
    i = 0;
    trans[1] = NewWin0pos[7];
    trans[2] = NewWin0pos[8];
    trans[3] = 1.0f;
    scale1[0] = 0.23f;
    scale1[2] = 1.0f;
    scale1[3] = 1.0f;
    scale2[0] = 0.3f;
    scale2[2] = 1.0f;
    scale1[1] = 0.23f;
    scale2[1] = 0.3f;
    scale2[3] = 1.0f;
    for (; i < n; i++) {
        flag = 3;
        if (i == n - 1) {
            flag = 1;
        }
        f = (float)i;
        trans[0] = NewWin0pos[6] + step * f;
        trans[2] = NewWin0pos[8] + f * 0.012f;
        if (i == selected) {
            trans[2] = NewWin0pos[2] + (float)(n + 1) * 0.012f;
        }
        if (mode != 0) {
            switch (definitions[hand->cards[i]].kind) {
            case 1:
                flag = !(mode & 1) ? (flag | 0x40) : (flag | 0x80);
                break;
            case 2:
                flag = !(mode & 2) ? (flag | 0x40) : (flag | 0x80);
                break;
            case 4:
                flag = !(mode & 4) ? (flag | 0x40) : (flag | 0x80);
                break;
            }
        }
        xglMatrixUnit(mat);
        xglMatrixTrans(tmp, base, trans);
        mat[3][0] = tmp[3][0];
        mat[3][1] = tmp[3][1];
        mat[3][2] = tmp[3][2];
        if (((CardHandMoveView *)hand)->mv[i].state == 0 && ((CardHandMoveView *)hand)->mv[i].timer == 0) {
            if (i == selected) {
                xglMatrixScale(mat, mat, scale2);
                CardDispCard(mat, hand->cards[selected], flag);
                nmlModelSetLight(CLColor, asDir);
                nmlModelSetPlace(mat);
                nmlModelSetTexture((char *)0x01D60000);
                nmlModelEntryCard((void *)0x01D12000);
            } else {
                xglMatrixScale(mat, mat, scale1);
                CardDispCard(mat, hand->cards[i], flag);
                if (flag & 0x40) {
                    nmlModelSetLight(MiddleColor, asDir);
                } else if (flag & 0x80) {
                    nmlModelSetLight(CLColor, asDir);
                }
                nmlModelSetPlace(mat);
                nmlModelSetTexture((char *)0x01717600);
                nmlModelEntryCard((void *)0x01716600);
            }
        }
    }
    nmlModelSetPlace(base);
    nmlModelSetLight(CNColor, asDir);
    nmlModelSetTexture((char *)0x019B0000);
    nmlModelEntryCard((void *)0x019AC000);
    CGCPGeneDispSub(1, definitions, (CardPlaySide *)hand, base, gen);
    CGCPNeedDispSub(1, (u8 *)&definitions[hand->cards[selected]].unmodeled_04, base, gen);
}

extern float NewWin0pos[4];
extern char CLColor[];

void CGCPHandDispSub2(CardDefinition *definitions, CardHand *hand, int selected, int mode)
{
    float base[4][4];
    float tmp[4][4];
    float mat[4][4];
    float trans[4];
    float scale1[4];
    float scale2[4];
    u8 gen[16];
    float step;
    float f;
    int n;
    int i;
    int flag;
    int v;

    xglMatrixUnit(base);
    trans[1] = -1.7f;
    trans[2] = 0.1f;
    scale1[1] = 0.97f;
    trans[0] = 0.0f;
    trans[3] = 1.0f;
    scale1[0] = 1.0f;
    scale1[2] = 1.0f;
    scale1[3] = 1.0f;
    xglMatrixTrans(base, base, trans);
    xglMatrixScale(base, base, scale1);
    n = CardPlayHandCnt(hand);
    if (n < 2) {
        step = (NewWin0pos[3] - NewWin0pos[0]) * 0.5f;
    } else {
        step = (NewWin0pos[3] - NewWin0pos[0]) / (float)(n - 1);
    }
    i = 0;
    trans[1] = NewWin0pos[1];
    trans[2] = NewWin0pos[2];
    trans[3] = 1.0f;
    scale1[0] = 0.23f;
    scale1[2] = 1.0f;
    scale1[3] = 1.0f;
    scale2[0] = 0.3f;
    scale2[2] = 1.0f;
    scale1[1] = 0.23f;
    scale2[1] = 0.3f;
    scale2[3] = 1.0f;
    for (; i < n; i++) {
        flag = 3;
        if (i == n - 1) {
            flag = 1;
        }
        f = (float)i;
        trans[0] = NewWin0pos[0] + step * f;
        trans[2] = NewWin0pos[2] + f * 0.012f;
        if (i == selected) {
            trans[2] = win0pos[2] + (float)(n + 1) * 0.012f;
        }
        if (mode != 0) {
            switch (definitions[hand->cards[i]].kind) {
            case 1:
                flag = !(mode & 1) ? (flag | 0x40) : (flag | 0x80);
                break;
            case 2:
                flag = !(mode & 2) ? (flag | 0x40) : (flag | 0x80);
                break;
            case 4:
                flag = !(mode & 4) ? (flag | 0x40) : (flag | 0x80);
                break;
            }
        }
        xglMatrixUnit(mat);
        xglMatrixTrans(tmp, base, trans);
        mat[3][0] = tmp[3][0];
        mat[3][1] = tmp[3][1];
        mat[3][2] = tmp[3][2];
        if (((CardHandMoveView *)hand)->mv[i].state == 0 && ((CardHandMoveView *)hand)->mv[i].timer == 0) {
            if (i == selected) {
                xglMatrixScale(mat, mat, scale2);
                CardDispCard(mat, hand->cards[selected], flag);
                nmlModelSetLight(CLColor, asDir);
                nmlModelSetPlace(mat);
                nmlModelSetTexture((char *)0x01D60000);
                nmlModelEntryCard((void *)0x01D12000);
            } else {
                xglMatrixScale(mat, mat, scale1);
                CardDispCard(mat, hand->cards[i], flag);
                nmlModelSetPlace(mat);
                if (flag & 0x40) {
                    nmlModelSetLight(MiddleColor, asDir);
                } else if (flag & 0x80) {
                    nmlModelSetLight(CLColor, asDir);
                }
                nmlModelSetTexture((char *)0x01717600);
                nmlModelEntryCard((void *)0x01716600);
            }
        }
    }
    v = n / 10;
    trans[1] = -0.005f;
    trans[2] = 0.0f;
    scale1[0] = 1.2f;
    scale1[1] = 1.2f;
    scale1[2] = 1.0f;
    if (v != 0) {
        v = 0x01C80000 + (v << 10);
        trans[0] = 1.116f;
        xglMatrixTrans(tmp, base, trans);
        xglMatrixScale(tmp, tmp, scale1);
        nmlModelSetPlace(tmp);
        nmlModelSetTexture((char *)0x01C91000);
        nmlModelEntryCard((void *)v);
    }
    v = 0x01C80000 + ((n % 10) << 10);
    trans[0] = 1.229f;
    xglMatrixTrans(tmp, base, trans);
    xglMatrixScale(tmp, tmp, scale1);
    nmlModelSetPlace(tmp);
    nmlModelSetTexture((char *)0x01C91000);
    nmlModelEntryCard((void *)v);
    nmlModelSetPlace(base);
    nmlModelSetLight(CNColor, asDir);
    nmlModelSetTexture((char *)0x019B0000);
    nmlModelEntryCard((void *)0x019A8000);
    CGCPGeneDispSub(0, definitions, (CardPlaySide *)hand, base, gen);
    CGCPNeedDispSub(0, (u8 *)&definitions[hand->cards[selected]].unmodeled_04, base, gen);
}

void CGCPHandDispSub(CardHand *hand, int sel, int side)
{
    float mat[4][4];
    float trans[4];

    xglMatrixUnit(mat);
    trans[0] = 0.0f;
    trans[1] = -0.95f;
    if (side) {
        trans[1] = 0.95f;
    }
    trans[2] = 0.1f;
    trans[3] = 1.0f;
    xglMatrixTrans(mat, mat, trans);
    CGCPHandDispSubCore(hand, sel, mat, side);
}

extern char D_00A4D710[];

void CGCPYamaDispSub(CardPlaySide *side, int index, int count, unsigned short buttons, int rotated)
{
    float base[4][4];
    float mat[4][4];
    float trans[4];
    float scale1[4];
    float scale2[4];
    float mat2[4][4];
    float step;
    float f;
    int n;
    int i;

    n = CardPlayYamaCnt(side);
    if (n < count) {
        count = n;
    }
    if (count == 1) {
        step = 0.0f;
    } else if (count < 2) {
        step = (win0pos[3] - win0pos[0]) * 0.5f;
    } else {
        step = (win0pos[3] - win0pos[0]) / (float)(count - 1);
    }
    xglMatrixUnit(base);
    trans[2] = 0.1f;
    i = n - count;
    trans[0] = 0.0f;
    trans[1] = 0.0f;
    trans[3] = 1.0f;
    xglMatrixTrans(base, base, trans);
    trans[1] = win0pos[1];
    trans[2] = win0pos[2];
    trans[3] = 1.0f;
    scale1[0] = 0.4f;
    scale1[2] = 1.0f;
    scale1[3] = 1.0f;
    scale2[0] = 0.5f;
    scale2[2] = 1.0f;
    scale1[1] = 0.4f;
    scale2[1] = 0.5f;
    scale2[3] = 1.0f;
    for (; i < n; i++) {
        f = (float)(i - (n - count));
        trans[1] = win0pos[1];
        trans[0] = win0pos[0] + step * f;
        trans[2] = win0pos[2] + f * 0.012f;
        if (i == n - count + index) {
            trans[2] = win0pos[2] + (float)(count + 1) * 0.012f;
        }
        xglMatrixTrans(mat, base, trans);
        if (i == n - count + index) {
            xglMatrixScale(mat, mat, scale2);
            { int d = 0; if (rotated) d = 5; CardDisp(mat, side->deck[i], 1, d); }
            if (!rotated && (buttons & 0x10)) {
                trans[0] = 0.0f;
                trans[1] = 0.369f;
                trans[2] = 0.3f;
                trans[3] = 1.0f;
                xglMatrixUnit(mat2);
                xglMatrixTrans(mat2, mat2, trans);
                CardDisp(mat2, side->deck[i], 1, 5);
            }
        } else {
            xglMatrixScale(mat, mat, scale1);
            CardDispCard(mat, side->deck[i], 1);
        }
    }
    __asm__ __volatile__(
        "lq $2, 0x0(%1)\n"
        "sq $2, 0x0(%0)\n"
        "lq $2, 0x10(%1)\n"
        "sq $2, 0x10(%0)\n"
        "lq $2, 0x20(%1)\n"
        "sq $2, 0x20(%0)\n"
        "lq $2, 0x30(%1)\n"
        "sq $2, 0x30(%0)\n"
        : : "r"(mat), "r"(base) : "$2", "memory");
    if (count == 1) {
        trans[0] = -2.0f;
        trans[1] = trans[2] = 0.0f;
        scale2[0] = 0.5f;
        trans[3] = 1.0f;
        scale2[1] = 1.0f;
        scale2[2] = 1.0f;
        scale2[3] = 1.0f;
        xglMatrixTrans(mat, mat, trans);
        xglMatrixScale(mat, mat, scale2);
    }
    nmlModelSetTexture((char *)0x01DB5800);
    nmlModelSetPlace(mat);
    nmlModelEntryCard((void *)0x01DA0000);
    trans[0] = -2.1f;
    trans[1] = 0.66f;
    trans[2] = 0.1f;
    scale2[0] = 0.5f;
    scale2[1] = 0.6f;
    trans[3] = 1.0f;
    scale2[2] = 1.0f;
    scale2[3] = 1.0f;
    xglMatrixTrans(mat, base, trans);
    xglMatrixScale(mat, mat, scale2);
    nmlModelSetTexture((char *)0x01DB5800);
    nmlModelSetPlace(mat);
    nmlModelEntryCard((void *)0x01DAAC00);
    xglFontPrint(0x16, 0x8C, 0xFFF8, D_00A4D710);
}

extern char D_00A4D720[];

void CGCPJunkDispSub(CardPlaySide *side, int sel)
{
    float base[4][4];
    float mat[4][4];
    float trans[4];
    float scale1[4];
    float scale2[4];
    float step;
    int n;
    int i;

    i = 0;
    n = CardPlayJunkCnt(side);
    step = (win0pos[3] - win0pos[0]) / (float)(n - 1);
    xglMatrixUnit(base);
    trans[0] = 0.0f;
    trans[1] = 0.0f;
    trans[2] = 0.1f;
    trans[3] = 1.0f;
    xglMatrixTrans(base, base, trans);
    trans[1] = win0pos[1];
    trans[2] = win0pos[2];
    trans[3] = 1.0f;
    scale1[0] = 0.4f;
    scale1[1] = 0.4f;
    scale1[2] = 1.0f;
    scale1[3] = 1.0f;
    scale2[0] = 0.5f;
    scale2[1] = 0.5f;
    scale2[2] = 1.0f;
    scale2[3] = 1.0f;
    for (; i < n; i++) {
        trans[0] = win0pos[0] + step * (float)i;
        trans[2] = win0pos[2] + (float)(n - i) * 0.012f;
        if (i == sel) {
            trans[2] = win0pos[2] + (float)(n + 1) * 0.012f;
        }
        xglMatrixTrans(mat, base, trans);
        if (i == sel) {
            xglMatrixScale(mat, mat, scale2);
            CardDisp(mat, side->hand.cards[i + 120], 1, 0);
        } else {
            xglMatrixScale(mat, mat, scale1);
            CardDispCard(mat, side->hand.cards[i + 120], 1);
        }
    }
    nmlModelSetTexture((char *)0x01DB5800);
    nmlModelSetPlace(base);
    nmlModelEntryCard((void *)0x01DA0000);
    trans[0] = -2.1f;
    trans[3] = 1.0f;
    trans[1] = 0.66f;
    trans[2] = 0.1f;
    scale2[0] = 0.5f;
    scale2[1] = 0.6f;
    scale2[2] = 1.0f;
    scale2[3] = 1.0f;
    xglMatrixTrans(mat, base, trans);
    xglMatrixScale(mat, mat, scale2);
    nmlModelSetTexture((char *)0x01DB5800);
    nmlModelSetPlace(mat);
    nmlModelEntryCard((void *)0x01DAAC00);
    xglFontPrint(0x16, 0x8C, 0xFFF8, D_00A4D720);
}

int CardPlayCntAllOpe(char *p, s16 v)
{
    s16 *a;
    s16 *b;
    s16 want;
    s32 n;
    s32 i;

    n = 0;
    want = v;
    b = (s16 *)(p + 0x3E84);
    a = (s16 *)(p + 0x13D8);
    i = 3;
    do {
        s16 y = *b;
        s16 x;
        b += 1;
        x = *a;
        a += 1;
        i -= 1;
        if ((want ^ x) == 0) n++;
        if ((want ^ y) == 0) n++;
    } while (i >= 0);
    return n;
}

s32 CardPlayCntOpe(CardPlaySide *side, s16 opId)
{
    s16 *op;
    s16 id;
    s16 want;
    s32 count;
    s32 remaining;

    want = opId;
    op = side->operations;
    count = 0;
    remaining = 3;
    do {
        id = *op;
        op += 1;
        remaining -= 1;
        if ((want ^ id) == 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

/*
 * hand is CardHand only by its call sites' declared type (CardPlayCntFld,
 * outside this TU's allocation, passes &work->player.hand/&work->enemy.hand);
 * CardHand is CardPlaySide's own first member (ov10/cgp.h, offset +0x000), so
 * that address is numerically CardPlaySide's own and this reaches
 * disposal[0]'s top card id, sibling to CardPlayCntBtl's battle[0].
 */
int CardPlayCntDsp(CardHand *hand, s16 cardId)
{
    CardPlaySide *side;
    s16 *cardIdField;
    s16 id;
    s16 want;
    s32 count;
    s32 remaining;

    want = cardId;
    side = (CardPlaySide *) hand;
    cardIdField = &side->disposal[0].battle.layers[0].cardId;
    count = 0;
    remaining = 3;
    do {
        id = *cardIdField;
        cardIdField += 0xDE;
        remaining -= 1;
        if ((want ^ id) == 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

/* Same base-address identity as CardPlayCntDsp, reaching battle[0] instead. */
int CardPlayCntBtl(CardHand *hand, s16 cardId)
{
    CardPlaySide *side;
    s16 *cardIdField;
    s16 id;
    s16 want;
    s32 count;
    s32 remaining;

    want = cardId;
    side = (CardPlaySide *) hand;
    cardIdField = &side->battle[0].battle.layers[0].cardId;
    count = 0;
    remaining = 3;
    do {
        id = *cardIdField;
        cardIdField += 0xDE;
        remaining -= 1;
        if ((want ^ id) == 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

s32 CardPlayCntLight(CardGameWork *work, CardPlaySide *side)
{
    s16 *cardIdField;
    s16 *p2;
    s16 id;
    s32 count;
    s32 remaining;

    count = 0;
    cardIdField = &side->battle[0].battle.layers[0].cardId;
    remaining = 3;
    do {
        id = *cardIdField;
        cardIdField += 0xDE;
        if (id >= 0 && ((work->definitions[id].flags & 1) || (work->definitions[id].flags & 2))) {
            count++;
        }
        remaining -= 1;
    } while (remaining >= 0);
    p2 = &side->disposal[0].battle.layers[0].cardId;
    remaining = 3;
    do {
        id = *p2;
        p2 += 0xDE;
        if (id >= 0 && ((work->definitions[id].flags & 1) || (work->definitions[id].flags & 2))) {
            count++;
        }
        remaining -= 1;
    } while (remaining >= 0);
    return count;
}

s32 CardPlayCntHeavy(CardGameWork *work, CardPlaySide *side)
{
    s16 *cardIdField;
    s16 id;
    s32 count;
    s32 remaining;

    count = 0;
    cardIdField = &side->battle[0].battle.layers[0].cardId;
    remaining = 3;
    do {
        id = *cardIdField;
        cardIdField += 0xDE;
        if (id >= 0 && (work->definitions[id].flags & 8)) {
            count++;
        }
        remaining -= 1;
    } while (remaining >= 0);
    cardIdField = &side->disposal[0].battle.layers[0].cardId;
    remaining = 3;
    do {
        id = *cardIdField;
        cardIdField += 0xDE;
        if (id >= 0 && (work->definitions[id].flags & 8)) {
            count++;
        }
        remaining -= 1;
    } while (remaining >= 0);
    return count;
}

extern s32 CGPWeaponEquipChk(CardGameWork *work, s32 weaponId, s32 equipId);

s32 CardEquipCntHeavy(CardGameWork *work, CardPlaySide *side, s32 weaponId)
{
    s16 *cardIdField;
    s16 *p2;
    s16 id;
    s32 count;
    s32 remaining;

    cardIdField = &side->battle[0].battle.layers[0].cardId;
    remaining = 3;
    count = 0;
    do {
        id = *cardIdField;
        cardIdField += 0xDE;
        if (id >= 0 && (work->definitions[id].flags & 8)) {
            if (CGPWeaponEquipChk(work, weaponId, id)) {
                count++;
            }
        }
        remaining -= 1;
    } while (remaining >= 0);
    p2 = &side->disposal[0].battle.layers[0].cardId;
    remaining = 3;
    do {
        id = *p2;
        p2 += 0xDE;
        if (id >= 0 && (work->definitions[id].flags & 8)) {
            if (CGPWeaponEquipChk(work, weaponId, id)) {
                count++;
            }
        }
        remaining -= 1;
    } while (remaining >= 0);
    return count;
}

/* still INCLUDE_ASM above in this TU; CardHand * matches CardPlayHandCnt's
 * argument (ov10/cgp.h), the same player.hand/enemy.hand records this
 * function indexes. */
extern int CardPlayCntDsp(CardHand *hand, s16 cardId);
extern int CardPlayCntBtl(CardHand *hand, s16 cardId);

int CardPlayCntFld(CardGameWork *work, s16 cardId) {
    CardHand *playerHand;
    CardHand *enemyHand;
    int count;

    playerHand = &work->player.hand;
    enemyHand = &work->enemy.hand;
    count = CardPlayCntDsp(playerHand, cardId);
    count = count + CardPlayCntBtl(playerHand, cardId);
    count = count + CardPlayCntDsp(enemyHand, cardId);
    return count + CardPlayCntBtl(enemyHand, cardId);
}

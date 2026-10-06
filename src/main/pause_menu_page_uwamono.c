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

INCLUDE_ASM("asm/main/nonmatchings/pause_menu_page_uwamono", DispDebugUwamono);

INCLUDE_ASM("asm/main/nonmatchings/pause_menu_page_uwamono", DispCollUwamono);

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

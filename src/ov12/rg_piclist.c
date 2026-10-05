/*
 * OV12 original TU 75: 0x00a40008..0x00a420e0 (46 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_piclist.h"
#include "ov12/rg_debug_flags.h"

/*
 * XrgPaint2DDrawXYWH's mode word is the same bit set XrgPaint2DRect.mode
 * documents in src/ov12/xrg_paint2d.h: bit 0x20 takes the drawn extent from
 * the bound picture instead of the width/height arguments, which is why
 * both draw helpers below always pass zero for width and height.
 */
#define XRG_PAINT2D_MODE_USE_PIC_SIZE 0x20
#define XRG_PAINT2D_ADD 1
#define XRG_PAINT2D_LINEAR 0

/* The fixed on-disk size of a serialized picture record. */
#define RG_PIC_BINARY_SIZE 0x100

extern int RgBxxGetPic(int group, int index);
/*
 * The second argument selects a blend mode (XrgPaint2DAlpha switches over
 * 0..5; RgPicDraw passes 0, 1 or 3 from the picture's alpha setting), not an
 * on/off flag.
 */
extern void XrgPaint2DAlpha(int paint, int blendMode);
extern void XrgPaint2DUseTexture(int paint, int texture);
extern void XrgPaint2DDrawXYWH(int paint, int mode, int x, int y, int width,
                               int height);

extern void assert_prog(const char *expression, const char *source_file,
                        int line);
extern RgDebugFlags *InstanceOfRgDebugFlags(void);
extern RgPic *CreateRgPicFromBinary(RgBxx *bxx, void *buffer);

/*
 * These are external file-backed witnesses, not candidate-emitted data.
 * Neither address has an entry in config/symbols/ov12.txt, so this
 * allocation keeps the splat default names rather than inventing new ones.
 */
extern const char D_00A57310[];
extern const char D_00A57320[];

extern RgHeap *InstanceOfRgHeap(void);
extern void *RgHeapAlloc(void *heap, unsigned int size, const char *source_file,
                         int line);
extern void RgHeapFree(void *heap, void *pointer, const char *source_file,
                       int line);

/* The fixed on-disk header size of a serialized picture list. */
#define RG_PIC_LIST_BINARY_SIZE 0x40

extern RgPic *CreateRgPic(RgBxx *bxx, const char *pszName);
extern void DisposeRgPicList(RgPicList *pList);

/*
 * Additional file-backed witnesses this allocation's assertions reference,
 * none registered with a name in config/symbols/ov12.txt.
 */
extern const char D_00A57338[]; /* "pBxx != NIL" */
extern const char D_00A57358[]; /* "pszName != NIL" */
extern const char D_00A57380[]; /* "pPaint != NIL" */
extern const char D_00A57390[]; /* "pList != NIL" */
extern const char D_00A573A0[]; /* "pList->m_uNum < PIC_MAX" */
extern const char D_00A573B8[]; /* "pDebug != NIL" */

static void _draw_add(int paintId, int picGroup, int picIndex, int x, int y)
{
    int texture;

    texture = RgBxxGetPic(picGroup, picIndex);
    if (texture != 0) {
        XrgPaint2DAlpha(paintId, XRG_PAINT2D_ADD);
        XrgPaint2DUseTexture(paintId, texture);
        XrgPaint2DDrawXYWH(paintId, XRG_PAINT2D_MODE_USE_PIC_SIZE, x, y, 0, 0);
    }
}

static void _draw_linear(int paintId, int picGroup, int picIndex, int x, int y)
{
    int texture;

    texture = RgBxxGetPic(picGroup, picIndex);
    if (texture != 0) {
        XrgPaint2DAlpha(paintId, XRG_PAINT2D_LINEAR);
        XrgPaint2DUseTexture(paintId, texture);
        XrgPaint2DDrawXYWH(paintId, XRG_PAINT2D_MODE_USE_PIC_SIZE, x, y, 0, 0);
    }
}

extern void XrgPaint2DColor(int paint, const RgPicColor *color);
extern int s_nColor_0;
typedef struct { RgPicColor c; } __attribute__((aligned(8))) RgPicColorA8;

static void _draw_nontex(int paint, int x, int y, int width, int height)
{
    RgPicColorA8 tmp;
    RgPicColorA8 color;

    memset(&color, 0, sizeof(color));
    color.c.r = s_nColor_0;
    color.c.g = s_nColor_0;
    color.c.b = s_nColor_0;
    tmp = color;
    s_nColor_0 += 8;
    if (s_nColor_0 > 0x20) {
        s_nColor_0 = 0;
    }
    XrgPaint2DColor(paint, &tmp.c);
    XrgPaint2DAlpha(paint, 1);
    XrgPaint2DDrawXYWH(paint, 0, x, y, width, height);
}

int RgPicGetWidth(RgPic *pic)
{
    int width;

    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 81);

    width = 0;
    if (pic->tex != 0)
        width = pic->tex->width;
    return width;
}

int RgPicGetHeight(RgPic *pic)
{
    int height;

    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 93);

    height = 0;
    if (pic->tex != 0)
        height = pic->tex->height;
    return height;
}

typedef struct { char b[0x20]; } RgPicName;
typedef struct { char b[4]; } RgPicWord;

void RgPicWrite(RgPic *pic, int handle)
{
    unsigned char buffer[0x400];
    unsigned char *p;
    int n;

    if (InstanceOfRgDebugFlags()->flags[3] == 0)
        return;
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 110);
    *(RgPicWord *)(buffer + 0x20) = *(RgPicWord *)&pic->x;
    *(RgPicWord *)(buffer + 0x24) = *(RgPicWord *)&pic->y;
    *(RgPicWord *)(buffer + 0x28) = *(RgPicWord *)&pic->alpha;
    *(RgPicWord *)(buffer + 0x2c) = *(RgPicWord *)&pic->dark;
    *(RgPicWord *)(buffer + 0x30) = *(RgPicWord *)&pic->notDraw;
    *(RgPicName *)buffer = *(RgPicName *)pic;
    p = buffer + 0x34;
    n = RG_PIC_BINARY_SIZE - 0x34 - 1;
    do {
        *p++ = 0;
    } while (n-- > 0);
    XrgHostWrite(handle, buffer, RG_PIC_BINARY_SIZE);
}

int RgPicBinarySize(void)
{
    return RG_PIC_BINARY_SIZE;
}

extern const char D_00A57348[];
typedef union { RgPicName n; RgPicWord w[16]; } RgPicU;

RgPic *CreateRgPicFromBinary(RgBxx *bxx, void *buffer)
{
    RgPic *pic;
    RgPicU *dst;
    RgPicWord *sp = (RgPicWord *)buffer;
    int tex;

    pic = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgPic), D_00A57320, 136);
    if (bxx == 0)
        assert_prog(D_00A57338, D_00A57320, 139);
    if (buffer == 0)
        assert_prog(D_00A57348, D_00A57320, 140);
    dst = (RgPicU *)pic;
    dst->n = ((RgPicU *)buffer)->n;
    sp += 8;
    *(RgPicWord *)&pic->x = *sp++;
    *(RgPicWord *)&pic->y = *sp++;
    *(RgPicWord *)&pic->alpha = *sp++;
    *(RgPicWord *)&pic->dark = *sp++;
    *(RgPicWord *)&pic->notDraw = *sp++;
    tex = RgBxxGetPic((int)bxx, (int)pic);
    pic->ofsY = 0;
    pic->tex = (RgPicTexture *)tex;
    pic->ofsX = 0;
    pic->color.b = 0x80;
    pic->color.g = 0x80;
    pic->color.r = 0x80;
    pic->color.a = 0x7f;
    if (tex == 0) {
        RgHeapFree(InstanceOfRgHeap(), pic, D_00A57320, 153);
        pic = 0;
    }
    return pic;
}

RgPic *RgPicRead(int handle, RgBxx *bxx)
{
    unsigned char buffer[0x400];
    RgDebugFlags *debugFlags;

    extern int XrgHostRead(int handle, void *buffer, int size);

    debugFlags = InstanceOfRgDebugFlags();
    if (debugFlags->flags[3] == 0)
        return 0;
    if (bxx == 0)
        assert_prog(D_00A57338, D_00A57320, 165);
    XrgHostRead(handle, buffer, RgPicBinarySize());
    return CreateRgPicFromBinary(bxx, buffer);
}

extern const char D_00A57368[];
extern char *strcpy(char *, const char *);

RgPic *CreateRgPic(RgBxx *bxx, const char *pszName)
{
    RgPic *pic;
    int tex;

    pic = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgPic), D_00A57320, 177);
    if (bxx == 0)
        assert_prog(D_00A57338, D_00A57320, 178);
    if (pszName == 0)
        assert_prog(D_00A57358, D_00A57320, 179);
    tex = RgBxxGetPic((int)bxx, (int)pszName);
    pic->tex = (RgPicTexture *)tex;
    if (tex == 0)
        assert_prog(D_00A57368, D_00A57320, 181);
    pic->x = 0;
    pic->y = 0;
    pic->alpha = 2;
    pic->dark = 0;
    pic->notDraw = 0;
    pic->ofsY = 0;
    pic->ofsX = 0;
    pic->color.b = 0x80;
    pic->color.g = 0x80;
    pic->color.r = 0x80;
    pic->color.a = 0x7f;
    strcpy(pic->name, pszName);
    return pic;
}

void DisposeRgPic(RgPic *pic)
{
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 197);
    RgHeapFree(InstanceOfRgHeap(), pic, D_00A57320, 198);
}

char *RgPicGetName(RgPic *pic)
{
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 204);
    return pic->name;
}

int RgPicGetAlpha(RgPic *pic)
{
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 211);
    return pic->alpha;
}

void RgPicSetAlpha(RgPic *pic, int alpha)
{
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 218);
    pic->alpha = alpha;
}

int RgPicGetX(RgPic *pic)
{
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 225);
    return pic->x;
}

int RgPicGetY(RgPic *pic)
{
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 230);
    return pic->y;
}

void RgPicSetOfsXY(RgPic *pic, int ofsX, int ofsY)
{
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 237);
    pic->ofsX = ofsX;
    pic->ofsY = ofsY;
}

void RgPicSetColor(RgPic *pic, const RgPicColor *color)
{
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 245);
    pic->color.r = color->r;
    pic->color.g = color->g;
    pic->color.b = color->b;
    pic->color.a = color->a;
}

RgPicTexture *RgPicGetTex(RgPic *pic)
{
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 255);
    return pic->tex;
}

void RgPicSetPos(RgPic *pic, int x, int y)
{
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 262);
    pic->x = x;
    pic->y = y;
}

void RgPicMove(RgPic *pic, int dx, int dy)
{
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 270);
    pic->x += dx;
    pic->y += dy;
}

extern void XrgPaint2DColor(int paint, const RgPicColor *color);

/*
 * The picture's alpha field selects a blend mode with the same values
 * XrgPaint2DAlpha documents above (1 -> add, 3 -> alpha, 2 -> linear); any
 * other alpha value leaves the paint object's current blend mode untouched.
 * A "dark" picture halves every color component before handing it to
 * XrgPaint2DColor instead of drawing with its stored color unchanged.
 */
void RgPicDraw(RgPic *pic, int paint)
{
    RgPicColor darkColor;

    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 280);
    if (paint == 0)
        assert_prog(D_00A57380, D_00A57320, 281);

    if (pic->notDraw == 0) {
        switch ((unsigned int) pic->alpha) {
        case 1:
            XrgPaint2DAlpha(paint, 1);
            break;
        case 3:
            XrgPaint2DAlpha(paint, 3);
            break;
        case 2:
            XrgPaint2DAlpha(paint, 0);
            break;
        }
        XrgPaint2DUseTexture(paint, (int) pic->tex);
        if (pic->dark != 0) {
            darkColor.r = pic->color.r / 2;
            darkColor.g = pic->color.g / 2;
            darkColor.b = pic->color.b / 2;
            darkColor.a = pic->color.a / 2;
            XrgPaint2DColor(paint, &darkColor);
        } else {
            XrgPaint2DColor(paint, &pic->color);
        }
        XrgPaint2DDrawXYWH(paint, XRG_PAINT2D_MODE_USE_PIC_SIZE,
                            pic->x + pic->ofsX, pic->y + pic->ofsY, 0, 0);
    }
}

void RgPicSetDark(RgPic *pic, int dark)
{
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 318);
    pic->dark = dark;
}

void RgPicNotDraw(RgPic *pic, int notDraw)
{
    if (pic == 0)
        assert_prog(D_00A57310, D_00A57320, 326);
    pic->notDraw = notDraw;
}

RgPicList *CreateRgPicList(RgBxx *pBxx)
{
    RgPicList *pList;

    pList = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgPicList), D_00A57320, 347);
    if (pBxx == 0)
        assert_prog(D_00A57338, D_00A57320, 348);
    pList->bxx = pBxx;
    pList->m_uNum = 0;
    pList->ofsX = 0;
    pList->ofsY = 0;
    pList->drawEnable = 1;
    return pList;
}

void DisposeRgPicList(RgPicList *pList)
{
    unsigned int i;

    if (pList == 0)
        assert_prog(D_00A57390, D_00A57320, 362);
    for (i = 0; i < pList->m_uNum; i++)
        DisposeRgPic(pList->pics[i]);
    RgHeapFree(InstanceOfRgHeap(), pList, D_00A57320, 365);
}

RgPic *RgPicListAddPic(RgPicList *pList, const char *pszName)
{
    RgPic *pic;

    if (pList == 0)
        assert_prog(D_00A57390, D_00A57320, 373);
    if (pszName == 0)
        assert_prog(D_00A57358, D_00A57320, 374);
    if (pList->m_uNum >= PIC_MAX)
        assert_prog(D_00A573A0, D_00A57320, 375);

    pic = CreateRgPic(pList->bxx, pszName);
    pList->pics[pList->m_uNum++] = pic;
    return pic;
}

void RgPicListDelPic(RgPicList *pList, int index)
{
    unsigned int i;

    if (pList == 0)
        assert_prog(D_00A57390, D_00A57320, 385);

    if (index >= 0 && index < pList->m_uNum) {
        DisposeRgPic(pList->pics[index]);
        for (i = (unsigned int) index; i < pList->m_uNum - 1; i++)
            pList->pics[i] = pList->pics[i + 1];
        pList->m_uNum--;
    }
}

RgPic *RgPicListGetPic(RgPicList *pList, int index)
{
    if (pList == 0)
        assert_prog(D_00A57390, D_00A57320, 399);

    if (index >= 0 && index < pList->m_uNum) {
        return pList->pics[index];
    }
    return 0;
}

unsigned int RgPicListGetSize(RgPicList *pList)
{
    if (pList == 0)
        assert_prog(D_00A57390, D_00A57320, 410);
    return pList->m_uNum;
}

extern void XrgPaint2DOffset2DDot(int paint, int offsetX, int offsetY);

void RgPicListDraw(RgPicList *pList, int paint)
{
    unsigned int i;

    if (pList == 0)
        assert_prog(D_00A57390, D_00A57320, 419);

    if (pList->drawEnable != 0) {
        XrgPaint2DOffset2DDot(paint, pList->ofsX, pList->ofsY);
        for (i = 0; i < pList->m_uNum; i++)
            RgPicDraw(pList->pics[i], paint);
    }
}

void RgPicListSetDrawEnable(RgPicList *pList, int enable)
{
    if (pList == 0)
        assert_prog(D_00A57390, D_00A57320, 431);
    pList->drawEnable = enable;
}

static int _BinarySizePicList(void)
{
    return RG_PIC_LIST_BINARY_SIZE;
}

typedef struct { char b[4]; } RgPicWLW;
typedef union { unsigned char b[0x408]; RgPicWLW w[0x102]; unsigned int n[0x102]; } RgPicWBuf;

void RgPicListWrite(RgPicList *pList, int handle)
{
    RgPicWBuf u;
    unsigned int i;
    unsigned char *p;
    int n;
    int sz;
    int t;

    if (InstanceOfRgDebugFlags()->flags[3] != 0) {
        u.n[0x100] = 0;
        if (pList == 0)
            assert_prog(D_00A57390, D_00A57320, 452);
        u.w[0] = *(RgPicWLW *)&pList->m_uNum;
        u.w[1] = *(RgPicWLW *)&pList->ofsX;
        u.w[2] = *(RgPicWLW *)&pList->ofsY;
        u.w[3] = u.w[0x100];
        p = u.b + 0x10;
        sz = _BinarySizePicList();
        n = sz - 0x11;
        if (sz - 0x10 > 0) {
            do {
                *p++ = 0;
            } while (n-- > 0);
        }
        i = 0;
        XrgHostWrite(handle, u.b, _BinarySizePicList());
        for (i = 0; i < pList->m_uNum; i++)
            RgPicWrite(pList->pics[i], handle);
    }
}

RgPicList *CreateRgPicListFromBinary(RgBxx *bxx, void *buffer)
{
    RgPicWord hdr[2];
    RgPicList *list;
    unsigned char *p = (unsigned char *)buffer;
    unsigned int i;
    RgPic *pic;
    int n;
    int sz;

    list = CreateRgPicList(bxx);
    hdr[0] = *(RgPicWord *)p;
    p += 4;
    *(RgPicWord *)&list->ofsX = *(RgPicWord *)p;
    p += 4;
    *(RgPicWord *)&list->ofsY = *(RgPicWord *)p;
    p += 4;
    hdr[1] = *(RgPicWord *)p;
    p += 4;
    sz = _BinarySizePicList();
    n = sz - 0x11;
    if (sz - 0x10 > 0) {
        do {
            p++;
        } while (n-- > 0);
    }
    for (i = 0; i < *(unsigned int *)&hdr[0]; i++) {
        pic = CreateRgPicFromBinary(bxx, p);
        if (pic != 0)
            list->pics[list->m_uNum++] = pic;
        p += RgPicBinarySize();
    }
    return list;
}

typedef struct { char b[4]; } RgPicLW;
typedef union { unsigned char b[0x408]; RgPicLW w[0x102]; unsigned int n[0x102]; } RgPicBuf;

RgPicList *RgPicListRead(int handle, RgBxx *bxx)
{
    RgPicBuf u;
    RgPicList *list;
    unsigned int i;
    RgPic *pic;

    extern int XrgHostRead(int handle, void *buffer, int size);

    if (InstanceOfRgDebugFlags()->flags[3] != 0) {
        i = 0;
        list = CreateRgPicList(bxx);
        XrgHostRead(handle, u.b, _BinarySizePicList());
        u.w[0x100] = u.w[0];
        *(RgPicLW *)&list->ofsX = u.w[1];
        *(RgPicLW *)&list->ofsY = u.w[2];
        u.w[0x101] = u.w[3];
        for (i = 0; i < u.n[0x100]; i++) {
            pic = RgPicRead(handle, bxx);
            if (pic != 0)
                list->pics[list->m_uNum++] = pic;
        }
        return list;
    }
    return 0;
}

int RgPicListGetOfsX(RgPicList *pList)
{
    if (pList == 0)
        assert_prog(D_00A57390, D_00A57320, 528);
    return pList->ofsX;
}

int RgPicListGetOfsY(RgPicList *pList)
{
    if (pList == 0)
        assert_prog(D_00A57390, D_00A57320, 534);
    return pList->ofsY;
}

void RgPicListSetOffset(RgPicList *pList, int ofsX, int ofsY)
{
    if (pList == 0)
        assert_prog(D_00A57390, D_00A57320, 541);
    pList->ofsX = ofsX;
    pList->ofsY = ofsY;
}

void RgPicListMoveAllPic(RgPicList *pList, int dx, int dy)
{
    unsigned int i;

    if (pList == 0)
        assert_prog(D_00A57390, D_00A57320, 550);
    for (i = 0; i < pList->m_uNum; i++)
        RgPicMove(pList->pics[i], dx, dy);
}

RgPicDebug *CreateRgPicDebug(RgBxx *pBxx)
{
    RgPicDebug *pDebug;

    if (pBxx == 0)
        assert_prog(D_00A57338, D_00A57320, 582);
    pDebug = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgPicDebug), D_00A57320, 583);
    pDebug->bxx = pBxx;
    pDebug->list = CreateRgPicList(pBxx);
    pDebug->mode = 1;
    pDebug->index = -1;
    pDebug->picId = 0;
    return pDebug;
}

void DisposeRgPicDebug(RgPicDebug *pDebug)
{
    if (pDebug == 0)
        assert_prog(D_00A573B8, D_00A57320, 596);
    DisposeRgPicList(pDebug->list);
    RgHeapFree(InstanceOfRgHeap(), pDebug, D_00A57320, 598);
}

typedef struct { unsigned char _p[0x28]; unsigned short held; unsigned short pressed; unsigned char _q[0xa0]; } PadD;
extern PadD PadData;

void RgPicDebugEditPosition(int *x, int *y)
{
    
    int step;
    int align;
    int input;

    input = PadData.held;
    step = 1;
    align = 0;
    if (input & 0x4) {
        step = 8;
        align = 1;
    }
    if (input & 0x1000) {
        *y -= step;
    }
    if (input & 0x4000) {
        *y += step;
    }
    if (input & 0x8000) {
        *x -= step;
    }
    if (input & 0x2000) {
        *x += step;
    }
    if (align != 0) {
        *x &= ~7;
        *y &= ~7;
    }
}

extern int RgBxxGetTexNum(RgBxx *pBxx);
extern int RgBxxGetPicID(RgBxx *pBxx, unsigned int id);
extern int XrgHostCreateOpen(const char *filename);
extern int XrgHostReadOpen(const char *filename);
extern void XrgHostClose(int handle);
extern void RgError(const char *message, const char *source_file, int line, ...);
extern const char D_00A573C8[];
extern const char D_00A573E0[];
extern const char D_00A573F8[];
typedef struct {
    unsigned char _p[0x28];
    unsigned short held;
    unsigned short pressed;
    unsigned short trig;
    unsigned char _r[0xa];
    signed char lx;
    signed char ly;
    signed char rx;
    signed char ry;
} PadDCtl;
static inline int RgClamp(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    return hi < v ? hi : v;
}
static inline int RgPicDebugClampIndex(RgPicDebug *pDebug)
{
    return RgClamp(pDebug->index, 0, RgPicListGetSize(pDebug->list) - 1);
}

void RgPicDebugControl(RgPicDebug *pDebug)
{
    int x;
    int y;
    int dx;
    int dy;
    int ofsX;
    int ofsY;
    int sy;
    int sx;
    int held;
    int trig;
    int num;
    int id;
    int idx;
    int max;
    int h;
    RgPic *prev;
    RgPic *pic;
    RgPic *npic;
    int mx;
    int mode;
    int my;
    PadDCtl *pad = (PadDCtl *)&PadData;

    {
        int rx = pad->rx;
        sy = pad->ly + pad->ry;
        sx = pad->lx + rx;
    }
    trig = pad->trig;
    held = pad->held;
    if (pDebug == 0)
        assert_prog(D_00A573B8, D_00A57320, 0x27C);
    mode = pDebug->mode;
    if (mode == 1) {
        pic = RgPicListGetPic(pDebug->list, pDebug->index);
        if (pic != 0) {
            x = RgPicGetX(pic);
            y = RgPicGetY(pic);
            RgPicDebugEditPosition(&x, &y);
            if (trig & 8) {
                unsigned int a = RgPicGetAlpha(pic) + 1;
                RgPicSetAlpha(pic, a < 4 ? a : 0);
            }
            RgPicSetPos(pic, x, y);
        }
        if (trig & 0x10)
            pDebug->index--;
        if (trig & 0x80)
            pDebug->index++;
        pDebug->index = RgClamp(pDebug->index, 0, RgPicListGetSize(pDebug->list) - 1);
        if (trig & 0x20)
            pDebug->mode = 0;
        if (trig & 0x40)
            pDebug->mode = 2;
        if ((held & 5) == 5)
            pDebug->mode = 3;
        if ((held & 0xA) == 0xA)
            pDebug->mode = 4;
        if (trig & 0x800)
            pDebug->mode = 5;
    } else if (mode == 0) {
        num = RgBxxGetTexNum(pDebug->bxx);
        if (num == 0)
            pDebug->mode = 1;
        if (trig & 0x10)
            pDebug->picId--;
        else if (trig & 0x80)
            pDebug->picId++;
        pDebug->picId = RgClamp(pDebug->picId, 0, num - 1);
        if (trig & 0x20) {
            id = RgBxxGetPicID(pDebug->bxx, pDebug->picId);
            if (id != 0) {
                prev = RgPicListGetPic(pDebug->list, pDebug->index);
                npic = RgPicListAddPic(pDebug->list, (const char *)id);
                if (npic != 0) {
                    if (prev == 0) {
                        RgPicSetPos(npic, 0x40, 0x80);
                    } else {
                        mx = 0x80 - RgPicGetX(prev);
                        my = 0x80 - RgPicGetY(prev);
                        if (sx < -0x50) {
                            my = 0;
                            mx = -RgPicGetWidth(npic);
                        } else if (sx > 0x50) {
                            my = 0;
                            mx = RgPicGetWidth(prev);
                        } else if (sy < -0x50) {
                            mx = 0;
                            my = -RgPicGetHeight(npic);
                        } else if (sy > 0x50) {
                            mx = 0;
                            my = RgPicGetHeight(prev);
                        }
                        RgPicSetPos(npic, RgPicGetX(prev) + mx, RgPicGetY(prev) + my);
                    }
                }
            }
            pDebug->mode = 1;
            pDebug->index = RgPicListGetSize(pDebug->list) - 1;
        }
        if (trig & 0x40)
            pDebug->mode = 1;
    } else if (mode == 2) {
        if (trig & 0x20) {
            RgPicListDelPic(pDebug->list, pDebug->index);
            if (RgPicListGetSize(pDebug->list) != 0)
                pDebug->index = RgPicDebugClampIndex(pDebug);
            else
                pDebug->index = -1;
            pDebug->mode = 1;
        }
        if (trig & 0x840)
            pDebug->mode = 1;
    } else if (mode == 3) {
        if (trig & 0x20) {
            if (InstanceOfRgDebugFlags()->flags[3] != 0) {
                h = XrgHostCreateOpen(D_00A573C8);
                if (h >= 0) {
                    RgPicListWrite(pDebug->list, h);
                    XrgHostClose(h);
                }
            }
            pDebug->mode = 1;
        } else if (trig & 0x40) {
            pDebug->mode = 1;
        }
    } else if (mode == 4) {
        if (trig & 0x20) {
            if (InstanceOfRgDebugFlags()->flags[3] != 0) {
                h = XrgHostReadOpen(D_00A573C8);
                if (h >= 0) {
                    DisposeRgPicList(pDebug->list);
                    pDebug->list = RgPicListRead(h, pDebug->bxx);
                    if (pDebug->list == 0)
                        assert_prog(D_00A573E0, D_00A57320, 0x302);
                    XrgHostClose(h);
                    pDebug->index = RgPicListGetSize(pDebug->list) - 1;
                }
            }
            pDebug->mode = 1;
        } else if (trig & 0x40) {
            pDebug->mode = 1;
        }
    } else if (mode == 5) {
        ofsX = RgPicListGetOfsX(pDebug->list);
        ofsY = RgPicListGetOfsY(pDebug->list);
        if (held & 8) {
            dx = 0;
            dy = 0;
            RgPicDebugEditPosition(&dx, &dy);
            RgPicListMoveAllPic(pDebug->list, dx, dy);
        } else {
            RgPicDebugEditPosition(&ofsX, &ofsY);
            RgPicListSetOffset(pDebug->list, ofsX, ofsY);
        }
        if (trig & 0x20)
            pDebug->mode = 1;
    } else {
        RgError(D_00A573F8, D_00A57320, 0x322, mode);
    }
}

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern int RgBxxGetPicID(RgBxx *pBxx, unsigned int id);
extern const char D_00A57418[];
extern const char D_00A57440[];
extern const char D_00A57458[];
extern const char D_00A57470[];
extern const char D_00A57490[];
extern const char D_00A574A0[];
extern const char D_00A574B8[];
extern const char D_00A574D0[];
extern const char D_00A574E8[];
extern const char D_00A57500[];
extern const char D_00A57520[];

void RgPicDebugDisp(RgPicDebug *pDebug, int paint)
{
    RgPic *pic;
    RgPicTexture *tex;
    int x;
    int y;
    unsigned int size;
    int id;
    char *name;
    int ofsX;
    int ofsY;

    if (pDebug == 0)
        assert_prog(D_00A573B8, D_00A57320, 812);
    switch (pDebug->mode) {
    case 1:
        pic = RgPicListGetPic(pDebug->list, pDebug->index);
        if (pic != 0) {
            tex = RgPicGetTex(pic);
            x = RgPicGetX(pic);
            y = RgPicGetY(pic);
            size = RgPicListGetSize(pDebug->list);
            RgPicListDraw(pDebug->list, paint);
            xglFontDebugPrintf(0, 8, D_00A57418);
            xglFontDebugPrintf(0, 0x18, D_00A57440, pDebug->index, size);
            name = RgPicGetName(pic);
            xglFontDebugPrintf(0, 0x24, D_00A57458, name, x, y, RgPicGetAlpha(pic));
            _draw_nontex(paint, x, y, tex->width, tex->height);
        }
        break;
    case 0:
        id = RgBxxGetPicID(pDebug->bxx, pDebug->picId);
        if (id != 0) {
            xglFontDebugPrintf(0, 8, D_00A57470);
            xglFontDebugPrintf(0, 0x20, D_00A57490, pDebug->picId, id);
            _draw_linear(paint, (int)pDebug->bxx, id, 0x40, 0x80);
        }
        break;
    case 2:
        pic = RgPicListGetPic(pDebug->list, pDebug->index);
        xglFontDebugPrintf(0, 8, D_00A574A0);
        if (pic != 0)
            RgPicDraw(pic, paint);
        break;
    case 3:
        xglFontDebugPrintf(0, 8, D_00A574B8);
        xglFontDebugPrintf(0, 0x18, D_00A574D0);
        break;
    case 4:
        xglFontDebugPrintf(0, 8, D_00A574E8);
        xglFontDebugPrintf(0, 0x18, D_00A574D0);
        break;
    case 5:
        do {
            RgPicListDraw(pDebug->list, paint);
            xglFontDebugPrintf(0, 8, D_00A57500);
            ofsX = RgPicListGetOfsX(pDebug->list);
            ofsY = RgPicListGetOfsY(pDebug->list);
            xglFontDebugPrintf(0, 0x18, D_00A57520, ofsX, ofsY);
        } while (0);
        break;
    }
}

void CreateRgPlacer2D(void)
{
}

#include "common.h"

/* The controller's first 16 bytes are the rectangle passed to the drawing
 * routines; the phase/subtype/frame bytes follow at +0x10..+0x12. */
typedef struct OpenCloseController {
    unsigned char unmodeled_00[16];
    signed char phase;
    unsigned char subtype;
    unsigned char frame;
} OpenCloseController;

typedef struct OpenCloseCallbacks {
    void (*entry[4])(OpenCloseController *);
} OpenCloseCallbacks;

/* Four original function pointers: open subtype 00/01, close 00/00. */
extern OpenCloseCallbacks D_004C2FD0;

extern void endPrintExtFunc(int work, int mode, void *descriptor);

typedef struct OpenSub01Packet {
    short x;
    short y;
    int work;
    short w;
    short h;
    unsigned char r, g, b, a;
} OpenSub01Packet;

typedef struct OpenSub01Ctrl {
    unsigned char pad0[4];
    short x;
    short y;
    int work;
    short w;
    short h;
    signed char phase;
    unsigned char subtype;
    unsigned char frame;
    unsigned char pad13;
    OpenSub01Packet packet[1];
} OpenSub01Ctrl;

extern const float D_004D7D40;
extern const float D_004D7D44;
extern const float D_004D7D48;
extern float xglCos(float angle);
extern float xglSin(float angle);

static void OpenSubType00(OpenSub01Ctrl *c)
{
    OpenSub01Packet *p;
    int i;
    int t;
    float ft;
    float s;
    float a;
    int x0, x1, y0, y1;
    int alpha;

    p = c->packet;
    t = 11 - c->frame;
    ft = t;
    alpha = t * 5 + 10;
    for (i = 0; i < 3; i++) {
        s = i;
        a = ft * D_004D7D40;
        x0 = (c->x + c->w / 2) - (c->w / 2) * xglCos(a);
        x1 = (c->x + c->w / 2) + (c->w / 2) * xglCos(a);
        a = ft * D_004D7D44;
        x0 = x0 - (c->w / 2) * (s * D_004D7D48) * xglSin(a);
        x1 = x1 + (c->w / 2) * (s * D_004D7D48) * xglSin(a);
        a = ft * D_004D7D40;
        y0 = (c->y + c->h / 2) - (c->h / 2) * xglCos(a);
        y1 = (c->y + c->h / 2) + (c->h / 2) * xglCos(a);
        a = ft * D_004D7D44;
        y0 = y0 - (c->h / 2) * (s * D_004D7D48) * xglSin(a);
        y1 = y1 + (c->h / 2) * (s * D_004D7D48) * xglSin(a);
        p->w = x1 - x0;
        p->work = c->work;
        p->x = x0;
        p->y = y0;
        p->r = 0x80;
        p->b = 0x80;
        p->g = 0x80;
        p->h = y1 - y0;
        if (i == 0) {
            p->a = 0x40;
        } else {
            p->a = alpha;
        }
        endPrintExtFunc(p->work, 9, p);
        p++;
    }
    if (t == 0) {
        c->phase = -1;
    } else {
        c->frame++;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/open_close_main", CloseSubType00);

static void OpenSubType01(OpenSub01Ctrl *c)
{
    OpenSub01Packet *p;
    int i;
    int half;
    int top;
    double mid;
    double ext;
    int bot;

    for (i = 0; i < 1; i++) {
        p = &c->packet[i];
        p->x = c->x;
        p->w = c->w;
        half = c->h / 2;
        mid = c->y + half;
        top = mid - half * (c->frame / 7.0);
        bot = mid + half * (c->frame / 7.0);
        p->y = top;
        p->h = bot - top;
        p->work = c->work;
        p->a = 0x40;
        p->r = 0x80;
        p->b = 0x80;
        p->g = 0x80;
        endPrintExtFunc(p->work, 9, p);
    }
    if (c->frame == 7) {
        c->phase = -1;
    } else {
        c->frame++;
    }
}

void OpenCloseMain(OpenCloseController *controller)
{
    OpenCloseCallbacks callbacks;

    if (controller->phase != 0) {
        if (controller->phase <= 0)
            return;
        if (controller->phase != 1)
            return;
    } else {
        controller->frame = 0;
        controller->phase = 1;
    }

    callbacks = D_004C2FD0;
    callbacks.entry[(controller->subtype >> 4) * 2 +
                    (controller->subtype & 15)](controller);
}

typedef struct EWinView {
    unsigned short x; unsigned char p02[2];
    unsigned short y; unsigned char p06[2];
    int work;
    unsigned short w; unsigned char p0e[2];
    unsigned short h; unsigned char p12[6];
    unsigned char mode;
    unsigned char flag;
    unsigned char visible;
    unsigned char p1b;
    OpenSub01Packet packet;
    OpenSub01Ctrl ctrl;
} EWinView;
static inline void eWindowPacketSet(EWinView *w)
{
    w->packet.x = w->x;
    w->packet.y = w->y;
    w->packet.work = w->work;
    w->packet.w = w->w;
    w->packet.h = w->h;
    w->packet.r = 0x80;
    w->packet.b = 0x80;
    w->packet.g = 0x80;
    w->packet.a = 0x40;
}

void eWindowMain(EWinView *w)
{
    switch (w->mode) {
    case 0:
        w->mode = 1;
        w->visible = 0;
        break;
    case 1:
        break;
    case 0x20:
        eWindowPacketSet(w);
        w->visible = 1;
        w->mode = 0x20;
    case 0x21:
        w->mode = 0x50;
        break;
    case 0x22:
        w->ctrl.x = w->x;
        w->ctrl.y = w->y;
        w->ctrl.work = w->work;
        w->ctrl.w = w->w;
        w->ctrl.h = w->h;
        w->mode = 0x23;
        w->ctrl.phase = 0;
        w->ctrl.subtype = 0;
    case 0x23:
        OpenCloseMain((OpenCloseController *)&w->ctrl);
        if (w->ctrl.phase < 0)
            w->mode = 0x20;
        break;
    case 0x50:
        w->mode = 0x51;
        w->visible = 1;
    case 0x51:
        eWindowPacketSet(w);
        break;
    case 0xA0:
        w->mode = 0xA1;
        break;
    case 0xA1:
        break;
    }
    if (w->visible != 0)
        endPrintExtFunc(w->packet.work, 1, &w->packet);
}

/*
 * Partial view of the object eWindowSet initialises. eWindowMain (still
 * INCLUDE_ASM in this TU) reads x/y/width/height back as the low 16 bits of
 * these words and work as a full word, copying all five into a render packet
 * at +0x1c..+0x27; it also dispatches on mode through a dozen-plus state
 * constants and gates that same packet's use on visible. flag is cleared here
 * but not read by any function in this TU.
 */
typedef struct EWindow {
    int x;               /* +0x00 */
    int y;                /* +0x04 */
    int work;             /* +0x08 */
    int width;             /* +0x0c */
    int height;             /* +0x10 */
    signed char color[4];   /* +0x14..0x17 */
    signed char mode;        /* +0x18 */
    signed char flag;         /* +0x19 */
    signed char visible;       /* +0x1a */
} EWindow;

void eWindowSet(EWindow *window)
{
    window->color[3] = -128;
    window->height = 0x10;
    window->flag = 0;
    window->x = 0;
    window->y = 0;
    window->work = 0;
    window->color[0] = -128;
    window->color[1] = -128;
    /* Kept together: without this grouping cc1 hoists color[2]/width/mode to
     * the front of the function instead of storing them here. */
    do {
        window->color[2] = -128;
        window->width = 0x10;
    } while (0);
    window->visible = 0;
    window->mode = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/open_close_main", eCursolMain);

void eCursolModeChange(unsigned char *mode, unsigned int next)
{
    unsigned char c;
    switch (next) {
    case 0 ... 0x1f:
        break;
    case 0x20:
    case 0x21:
        c = *mode;
        if (c == 0x50) return;
        if (c == 0x51) return;
        if (c == 0x20) return;
        if (c == 0x21) return;
        break;
    case 0x50:
    case 0x51:
        c = *mode;
        if (c == 0x50) return;
        if (c == 0x51) return;
        break;
    }
    *mode = next;
}

/*
 * Partial view of the object eCursolSet initialises. eCursolMain (still
 * INCLUDE_ASM in this TU) dispatches on state through a jump table, gates its
 * render call on active, advances timer once per call to drive a blink
 * pattern, and uses width in the cursor's box-position arithmetic; x, y and
 * work are copied into an embedded sprite record at +0x10 the same way
 * eSpriteSet's own x/y/work are used (src/main/e_number_main.c), and color is
 * the same four-channel quad convention as that file's other e* widgets.
 */
typedef struct ECursol {
    signed char state;    /* +0x00 */
    signed char active;    /* +0x01 */
    signed char timer;      /* +0x02 */
    signed char width;       /* +0x03 */
    short x;                  /* +0x04 */
    short y;                   /* +0x06 */
    int work;                   /* +0x08 */
    signed char color[4];        /* +0x0c..0x0f */
} ECursol;

void eCursolSet(ECursol *cursol, signed char width)
{
    cursol->width = width;
    cursol->color[0] = -128;
    cursol->x = 0;
    cursol->y = 0;
    cursol->work = 0;
    cursol->color[3] = -128;
    cursol->color[2] = -128;
    /* Kept together: matches eWindowSet's color[2]/width barrier (this file,
     * CP-0337); without the do/while(0) barrier around color[1]/state/timer
     * cc1 hoists this run to the front instead (measured:
     * attempt-95dd36f8157e build/form-03, first difference 0x00000e8c,
     * 55.357%; build/form-04, byte-identical, 100%). */
    do {
        cursol->color[1] = -128;
        cursol->state = 0;
        cursol->timer = 0;
    } while (0);
    cursol->active = 0;
}

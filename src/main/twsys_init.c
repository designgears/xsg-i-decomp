#include "common.h"

/*
 * One slot of the fixed-size `tcomponent` table TWSYS_createComponent hands
 * out (still assembler in this TU). Only the header fields every component
 * kind shares, evidenced from this allocation, are modeled:
 *  - +0x10 flags: TWIN_dispose/TMENU_dispose clear bits 0x11 on disposal;
 *    TWSYS_update tests bit 0x10 before dispatching the component's update.
 *  - +0x16 kindAndGroup: low nibble selects the update/draw handler
 *    (TMENU/TSLIDER/TWIN), bits 4-5 select the `groupStatus` slot.
 *  - +0x32 closeState: cleared with the flags when a slot is (re)claimed.
 *  - +0x54 ewHandle: TWIN_dispose passes it to EW_dispose (a window-kind
 *    slot's own layout; other kinds keep their own ewHandle elsewhere).
 *  - +0x9C/+0xA0 msgBuffer: TWIN_dispose passes each non-zero entry to
 *    MBUF_dispose.
 * The 0x5B0 stride between slots is TWSYS_init/TWSYS_update/TWSYS_draw's own
 * loop increment.
 */
typedef struct TComponent {
    unsigned char unmodeled_00[0xc];
    unsigned short pixelWidth; /* +0xc */
    unsigned char unmodeled_0e[2];
    unsigned int flags;                /* +0x10 */
    unsigned char unmodeled_14[0x16 - 0x14];
    unsigned short kindAndGroup;       /* +0x16 */
    unsigned char unmodeled_18[0x20 - 0x18];
    float width;                       /* +0x20: TMENU_create/TMENU_init */
    float height;                      /* +0x24: TMENU_create/TMENU_init */
    unsigned char unmodeled_28[0x32 - 0x28];
    unsigned short closeState;         /* +0x32 */
    unsigned char unmodeled_34[0x54 - 0x34];
    int ewHandle;                      /* +0x54 */
    unsigned char unmodeled_58[0x9C - 0x58];
    int *msgBuffer[2];                 /* +0x9C */
    unsigned char unmodeled_a4[0x5B0 - 0xA4];
} TComponent;

extern TComponent tcomponent[4];
extern unsigned char groupStatus[3];
extern unsigned char D_004DC593;

void EW_init(unsigned char *freeSpace);
void MBUF_init(void);

void TWSYS_init(void)
{
    TComponent *component;
    TComponent *end;
    unsigned char *groupByte;
    int count;

    component = tcomponent;
    end = tcomponent + 4;
    do {
        component->flags = 0;
        component->closeState = 0;
        component++;
    } while ((int)component < (int)end);
    count = 3;
    groupByte = &D_004DC593;
    do {
        count -= 1;
        *groupByte = 0;
        groupByte -= 1;
    } while (count >= 0);
    EW_init((unsigned char *)end);
    MBUF_init();
}

void TWSYS_setGRPStatus(int group, int status)
{
    groupStatus[group] = (unsigned char)status;
}

unsigned char TWSYS_getGRPStatus(int group)
{
    return groupStatus[group];
}

TComponent *TWSYS_createComponent(int slotIndex, int kindAndGroup)
{
    int i;
    TComponent *c;

    if (slotIndex < 0) {
        for (i = 0; i < 4; i++) {
            c = &tcomponent[i];
            if (!(c->flags & 0x10)) {
                slotIndex = i;
                break;
            }
        }
        if (slotIndex < 0) {
            return 0;
        }
    }
    c = &tcomponent[slotIndex];
    c->kindAndGroup = (slotIndex << 8) | kindAndGroup;
    return c;
}

void TMENU_updateDefault(TComponent *component);
void TSLIDER_updateDefault(TComponent *component);
void TWIN_update2(TComponent *component);

void TWSYS_update(void)
{
    TComponent *component;
    int slotIndex;
    unsigned int group;
    unsigned int kind;

    component = tcomponent;
    slotIndex = 0;
    do {
        group = (component->kindAndGroup >> 4) & 3;
        if (!(groupStatus[group] & 1)) {
            kind = component->kindAndGroup & 0xF;
            if (component->flags & 0x10) {
                switch (kind) {
                case 1:
                    TMENU_updateDefault(component);
                    break;
                case 2:
                    TSLIDER_updateDefault(component);
                    break;
                case 0:
                case 3:
                    TWIN_update2(component);
                    break;
                case 4:
                    TWIN_update2(component);
                    break;
                }
            }
        }
        slotIndex += 1;
        component++;
    } while (slotIndex < 4);
}

void EW_draw(void);
void TMENU_drawDefault(TComponent *component);
void TSLIDER_drawDefault(TComponent *component);
void TWIN_draw2(TComponent *component);
void TWIN_drawScene2(TComponent *component);

/* Defined by xgl_font.c; `text` is a font control-code string. */
extern void xglFontPrintDirectOT(int ot, const char *text);

extern const char D_004DA420[];

/*
 * The font control strings TWSYS_draw emits before a component's own drawing
 * code: the same setup for every window-like kind, and a variant for the
 * scene window, differing only in the two counts at offsets 1 and 4.
 */
#define TWSYS_DRAW_WINDOW_FONT "\x0d\x02\x0e\x02\x03\x18\x18\x18\x0f\x00\x00\x80\x0c\x80\x80\x80"
#define TWSYS_DRAW_SCENE_FONT  "\x0d\x03\x0e\x02\x02\x18\x18\x18\x0f\x00\x00\x80\x0c\x80\x80\x80"

void TWSYS_draw(void)
{
    TComponent *component;
    int slotIndex;
    unsigned int group;
    unsigned int kind;

    component = tcomponent;
    slotIndex = 0;
    do {
        group = (component->kindAndGroup >> 4) & 3;
        if (!(groupStatus[group] & 2)) {
            if (component->flags & 0x10) {
                xglFontPrintDirectOT(0xFFFFFF, D_004DA420);
                kind = component->kindAndGroup & 0xF;
                switch (kind) {
                case 1:
                    xglFontPrintDirectOT(0xFFFFFF, TWSYS_DRAW_WINDOW_FONT);
                    TMENU_drawDefault(component);
                    break;
                case 2:
                    xglFontPrintDirectOT(0xFFFFFF, TWSYS_DRAW_WINDOW_FONT);
                    TSLIDER_drawDefault(component);
                    break;
                case 0:
                case 3:
                    xglFontPrintDirectOT(0xFFFFFF, TWSYS_DRAW_WINDOW_FONT);
                    TWIN_draw2(component);
                    break;
                case 4:
                    xglFontPrintDirectOT(0xFFFFFF, TWSYS_DRAW_SCENE_FONT);
                    TWIN_drawScene2(component);
                    break;
                }
            }
        }
        slotIndex += 1;
        component++;
    } while (slotIndex < 4);
    EW_draw();
}

int PARSE_int(char *s, int len, int radix)
{
  int result = 0;
  int i = (float) 0;
  int neg = 0;
  int limit;
  int multmin;
  int d;
  int c;
  int new_var;
  int t;
  int u;
  int w;
  if (len < 0)
  {
    len = strlen(s);
  }
  if ((*s) == '-')
  {
    neg = 1;
    i = 1;
    limit = 0x80000000;
  }
  else
  {
    limit = -0x7FFFFFFF;
  }
  multmin = limit / radix;
  if (i < len)
  {
    c = s[i++];
    d = c;
    if (((unsigned) (t = c - '0')) < 10)
    {
      d = t;
    }
    else
      if (((unsigned) (t = c - 'a')) < 26)
    {
      d = t;
      d += 10;
    }
    else
      if (((unsigned) (t = c - 'A')) < 26)
    {
      d = t;
      d += 10;
    }
    result = -d;
    while (i < len)
    {
      c = s[i++];
      d = c;
      t = (new_var = c - '0');
      u = c + -'a';
      if (((unsigned) t) < 10)
      {
        d = t;
      }
      else
        if (((unsigned) u) < 26)
      {
        d = u;
        d += 10;
      }
      else
      {
        if (((unsigned) (w = c - 'A')) < 26)
        {
          d = w;
          d += 10;
        }
 do { } while (0);
      }
      if (result < multmin)
      {
        return 0;
      }
      result *= radix;
      if (result < (limit + d))
      {
        return 0;
      }
      result -= d;
    }

  }
  if (neg)
  {
    return result;
  }
  return -result;
}

extern void *memcpy(void *destination, const void *source, unsigned int count);
extern unsigned char radixTenUnits[100];
extern unsigned char radixTenTenths[100];

int STRING_int(char *dst, int val)
{
    char buf[12];
    int j;
    int i;
    int neg;

    neg = (unsigned)val >> 31;
    if (neg) {
        val = -val;
    }
    i = 12;
    j = 12;
    do {
        int r = val % 100;
        i -= 2;
        buf[--j] = radixTenUnits[r];
        buf[--j] = radixTenTenths[r];
        val /= 100;
    } while (val != 0);
    if ((unsigned char)buf[j] == '0') {
        i++;
    }
    if (neg) {
        i--;
        buf[i] = '-';
    }
    memcpy(dst, buf + i, 12 - i);
    return (int)dst - i + 12;
}

void STRING_h2zEUC(char *destination, const char *source)
{
    int character;

    character = *source++;
    while (character != 0) {
        if (character >= '0' && character <= '9') {
            *destination++ = (char)0xA3;
            *destination++ = character - 0x80;
        } else if (character >= 'a' && character <= 'z') {
            *destination++ = (char)0xA3;
            *destination++ = character - 0x80;
        } else if (character >= 'A' && character <= 'Z') {
            *destination++ = (char)0xA3;
            *destination++ = character + 0x60;
        } else if (character < 160) {
        } else {
            *destination++ = character;
            *destination++ = (unsigned char)*source++;
        }
        character = *source++;
    }
    *destination = 0;
}

extern char digits[];

void STRING_toUInt(char *dst, int val, int shift)
{
    char buf[32];
    int mask = (1 << shift) - 1;
    int i = 32;
    do {
        buf[--i] = digits[val & mask];
        val >>= shift;
    } while (val != 0);
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct MsgQueueInfo {
    short count;
    short lines;
    short width;
} MsgQueueInfo;

typedef struct Vector4_SP {
    float x;
    float y;
    float z;
    float w;
} Vector4_SP;
typedef float Matrix4_SP[4][4];
typedef struct StudioCamera_SP {
    unsigned int active;
    unsigned int state;
    unsigned char unmodeled_08[0x18];
    float nearClip;
    float farClip;
    unsigned char unmodeled_28[0x48];
    Vector4_SP screenOffset;
    Vector4_SP screenScale;
    unsigned char unmodeled_90[0x10];
    Vector4_SP rotation;
    Vector4_SP angleVelocity;
    unsigned char unmodeled_c0[0x10];
    Vector4_SP position;
    unsigned char unmodeled_e0[0x90];
    Matrix4_SP cameraToWorldMatrix;
    unsigned char unmodeled_1b0[0x2c0];
    Matrix4_SP viewMatrix;
    unsigned char unmodeled_4b0[0x140];
} StudioCamera_SP;
typedef struct TComponent_SP {
    unsigned char unmodeled_00[0xc];
    unsigned short pixelWidth;
    unsigned short pixelHeight;
    unsigned int flags;
    unsigned char unmodeled_14[0x16 - 0x14];
    unsigned short kindAndGroup;
    unsigned char unmodeled_18[0x20 - 0x18];
    float width;
    float height;
    unsigned char unmodeled_28[0x32 - 0x28];
    unsigned short closeState;
    unsigned char unmodeled_34[0x44 - 0x34];
    Vector4_SP *ownerPosition;
    unsigned char unmodeled_48[0x54 - 0x48];
    int ewHandle;
    unsigned char unmodeled_58[0x9C - 0x58];
    int *msgBuffer[2];
    unsigned char unmodeled_a4[0x5B0 - 0xA4];
} TComponent_SP;
typedef struct Matrix4Block_SP {
    Matrix4_SP value;
} Matrix4Block_SP;
typedef struct TransformParams_SP {
    Matrix4_SP matrix;
    Matrix4_SP world;
    Vector4_SP screenScale;
    Vector4_SP screenOffset;
    Matrix4_SP *local;
} TransformParams_SP;
typedef struct WindowOwnerPrefix_SP {
    unsigned char unmodeled_00[0x10];
    Vector4_SP position;
} WindowOwnerPrefix_SP;
int XTK_getWindowOwner(void);
extern float D_004D7C94;
extern void xglMatrixStackUnit(void);
void xglMatrixStackTrans(const float translation[4]);
extern void xglMatrixStackSave(float matrix[4][4]);
void xglStudioGetCamera(StudioCamera_SP **camera, int index);
void transformVector(Vector4_SP *destination, const Vector4_SP *vectors, int count,
                     TransformParams_SP *params);
extern Vector4_SP vSample;
extern void xglVectorLength(float *destination, const Vector4_SP *vector);

void TW_setPos(TComponent *component_)
{
    TComponent_SP *component = (TComponent_SP *)component_;
    Vector4_SP anchorPosition;
    Vector4_SP direction;
    Vector4_SP cameraDelta;
    Matrix4_SP savedMatrix;
    Vector4_SP offsets;
    StudioCamera_SP *cameras[1];
    StudioCamera_SP *sourceCamera;
    Vector4_SP *cameraPosition;
    WindowOwnerPrefix_SP *owner;
    Vector4_SP *ownerPosition;
    Vector4_SP *projected;
    TransformParams_SP *params = (TransformParams_SP *)0x70001800;
    int halfWidth;
    int halfHeight;
    float screenShiftX;
    float screenShiftY;
    float distanceOffset;
    float rightEdge;

    owner = (WindowOwnerPrefix_SP *)XTK_getWindowOwner();
    if (owner == 0 || !(component->flags & 0x40))
        return;

    projected = (Vector4_SP *)0x70001680;
    ownerPosition = &owner->position;
    anchorPosition.x = ownerPosition->x;
    anchorPosition.y = ownerPosition->y + D_004D7C94;
    anchorPosition.z = ownerPosition->z;
    component->ownerPosition = ownerPosition;
    anchorPosition.w = 1.0f;
    xglMatrixStackUnit();
    xglMatrixStackTrans(&anchorPosition.x);
    xglMatrixStackSave(savedMatrix);
    xglStudioGetCamera(&cameras[0], 0);
    sourceCamera = cameras[0];
    params->local = &savedMatrix;
    *(Matrix4Block_SP *)params->world = *(Matrix4Block_SP *)sourceCamera->viewMatrix;
    params->screenOffset = sourceCamera->screenOffset;
    params->screenScale = sourceCamera->screenScale;
    transformVector(projected, &vSample, 1, params);
    cameraPosition = &cameras[0]->position;
    projected->x -= 2048.0f;
    projected->y = projected->y - 2048.0f;

    /* Original VU0 operation: distance from the anchor to the camera.
     * vf2/vf3 are local VU scratch state; the stored vector feeds length. */
    __asm__ __volatile__(
        "lqc2 $vf3,0(%1)\n"
        "lqc2 $vf2,0(%2)\n"
        "vsub.xyz $vf2xyz,$vf2xyz,$vf3xyz\n"
        "sqc2 $vf2,0(%0)"
        : : "r"(&cameraDelta), "r"(cameraPosition), "r"(&anchorPosition)
        : "memory");
    xglVectorLength(&projected->z, &cameraDelta);
    if (projected->z > 2.5f)
        projected->z = 2.5f;
    projected->z = 2.5f - projected->z;
    direction.x = 0.0f < projected->x ? -1.0f : 1.0f;
    direction.y = -1.0f;
    halfWidth = (short)component->pixelWidth / 2;
    halfHeight = (short)component->pixelHeight / 2;
    offsets.x = (float)(halfWidth + 16) * direction.x;
    offsets.y = -(float)(halfHeight + 16);
    screenShiftY = (float)(224 - halfHeight);
    distanceOffset = 40.0f * projected->z;
    {
        float shiftX = offsets.x + distanceOffset * direction.x;

        projected->x += shiftX;
    }
    projected->y += offsets.y + distanceOffset * direction.y;
    screenShiftX = (float)(256 - halfWidth);
    projected->x += screenShiftX;
    projected->y += screenShiftY;
    if (8.0f > projected->x)
        projected->x = 8.0f;
    rightEdge = (float)(504 - (short)component->pixelWidth);
    if (projected->x > rightEdge)
        projected->x = rightEdge;
    if (8.0f > projected->y) {
        float clampDelta = projected->y - 8.0f;

        offsets.z = clampDelta;
        projected->y = 8.0f;
        projected->x -= offsets.z * direction.x * 0.5f;
    }
    if (projected->x < 8.0f)
        projected->x = 8.0f;
    if (rightEdge < projected->x)
        projected->x = rightEdge;
    if (projected->y > 224.0f)
        projected->y = 224.0f;
    component->width = projected->x;
    component->height = projected->y;
}

/*
 * The text-window record TWSYS_createComponent hands back (still assembler
 * in this TU). Only the fields this allocation and its evidenced siblings
 * touch are modeled:
 *  - +0xC/+0xE width/height: TW_setPos (0x0025d660) reads both as unsigned
 *    halfwords for its on-screen centering math.
 *  - +0xA6 line_pitch: TWIN_drawScene2 (0x0025ec24, still assembler)
 *    multiplies the current line index (+0x185) by this halfword to place
 *    each text line, so it is the per-line vertical advance.
 *  - +0x186 layout_offset: set here and by TWIN_create2's own default
 *    (still assembler); no function recovered so far in this TU reads it
 *    back, so only that it is a byte is proven.
 *  - +0x187 line_count: TWIN_update2 and TWIN_popCF/TWIN_popScene (still
 *    assembler) loop up to this byte over the +0x17C line-pointer array.
 */
/* Ported from the fork's include/main/msg_init.h for the TWindow/TMenu layouts. */
typedef struct MsgBuffer {
    int active;
    unsigned int capacity;
    unsigned char **entry_table;
    unsigned char *cursor;
    unsigned char workspace[0x800];
} MsgBuffer;

typedef struct MsgQueue {
    unsigned char *buffer;
    unsigned char unmodeled_04[4];
    short head;
    short tail;
    short size;
    short count;
    short width;
    short last;
    short limit;
    unsigned char unmodeled_22[6];
    int label;
} MsgQueue;

typedef struct EwContainerWidget EwContainerWidget;

typedef struct TWindow {
    unsigned char unmodeled_00[0xC];
    unsigned short width;          /* +0xC */
    unsigned short height;         /* +0xE */
    unsigned char unmodeled_10[0]; /* +0x10: zero-length alias of the former padding member, kept for saved bodies */
    unsigned int flags;            /* +0x10 */
    unsigned short state;          /* +0x14 */
    unsigned short kindAndGroup;   /* +0x16 */
    unsigned char unmodeled_018[0x20 - 0x18];
    float x; /* +0x20 */
    float y; /* +0x24 */
    unsigned char unmodeled_028[0x30 - 0x28];
    short waitFrames; /* +0x30 */
    unsigned short closeState;     /* +0x32 */
    unsigned char unmodeled_034[0x50 - 0x34];
    unsigned short textVerticalOffset; /* +0x50 */
    unsigned char unmodeled_052[0x54 - 0x52];
    EwContainerWidget *rootWidget; /* +0x54 */
    unsigned char unmodeled_058[0x9c - 0x58];
    MsgBuffer *messageBuffer[2]; /* +0x9c */
    short currentPage;             /* +0xA4 */
    short line_pitch;              /* +0xA6 */
    unsigned char unmodeled_a8[0]; /* +0xa8: zero-length alias of the former padding member, kept for saved bodies */
    char *textOutput;              /* +0xA8 */
    MsgQueue queue; /* +0xac */
    unsigned int frameCounter; /* +0xcc */
    void *widgets[8]; /* +0xd0 */
    unsigned int initializationWord0; /* +0xF0 */
    unsigned char unmodeled_f4[0x108 - 0xF4];
    unsigned int initializationWord1; /* +0x108 */
    unsigned char unmodeled_10c[0x120 - 0x10c];
    int pendingMessage; /* +0x120 */
    unsigned char unmodeled_124[0x14c - 0x124];
    unsigned short lineWidths[24]; /* +0x14c */
    char **lineBufferPointers;     /* +0x17C */
    char *textScratchBuffer;       /* +0x180 */
    unsigned char lineCharacterOffset; /* +0x184 */
    unsigned char currentLineIndex; /* +0x185 */
    unsigned char layout_offset;   /* +0x186 */
    unsigned char line_count;      /* +0x187 */
    char *lineData[1];             /* +0x188; inline pointer and character buffers follow */
} TWindow;

/* Configures a field-conversation text window's line pitch, layout offset,
 * line count and on-screen size. */
void WIN_initCF(TWindow *window)
{
    window->line_pitch = 0x18;
    window->layout_offset = 0x2A;
    window->line_count = 3;
    window->width = 0x1CC;
    window->height = 0x50;
}

/* Configures a scene-message text window's line pitch, layout offset, line
 * count and on-screen size. */
void WIN_initScene(TWindow *window)
{
    window->line_pitch = 0x1A;
    window->layout_offset = 0x30;
    window->line_count = 3;
    window->width = 0x1E0;
    window->height = 0x56;
}

typedef struct TWindowInit2 {
    unsigned char pad_00[0x10];
    int flags;                  /* +0x10 */
    short state;                /* +0x14 */
    unsigned short kindAndGroup;/* +0x16 */
    unsigned char pad_18[0x32 - 0x18];
    short closeState;           /* +0x32 */
    unsigned char pad_34[0xA4 - 0x34];
    short a4;                   /* +0xA4 */
    short line_pitch;           /* +0xA6 */
    char *a8;                   /* +0xA8 */
    unsigned char pad_ac[0xF0 - 0xAC];
    int f0;                     /* +0xF0 */
    unsigned char pad_f4[0x108 - 0xF4];
    int x108;                   /* +0x108 */
    unsigned char pad_10c[0x17C - 0x10C];
    char **lines;               /* +0x17C */
    char *x180;                 /* +0x180 */
    unsigned char x184;
    unsigned char x185;
    unsigned char layout_offset;
    unsigned char line_count;   /* +0x187 */
    char buf[1];                /* +0x188 */
} TWindowInit2;

void TWIN_init2(TWindowInit2 *window)
{
    int kind;
    int i;
    char *p;
    int fl;

    window->x108 = 0;
    window->x185 = 0;
    window->x184 = 0;
    window->a4 = 0;
    kind = window->kindAndGroup & 0xF;
    switch (kind) {
    case 3:
    default:
        WIN_initCF((TWindow *)window);
        break;
    case 4:
        WIN_initScene((TWindow *)window);
        break;
    }
    p = window->buf;
    window->lines = (char **)p;
    p += window->line_count * 4;
    for (i = 0; i < window->line_count; i++) {
        window->lines[i] = p;
        *p = 0;
        p += 0xC0;
    }
    window->a8 = p;
    *p = 0;
    p += 0x80;
    window->x180 = p;
    fl = 0x54;
    window->state = 1;
    window->flags = fl;
    window->closeState = 0;
    window->f0 = 0;
}

int WIN_checkActiveWindow(void)
{
    TComponent *component;
    int slotIndex;
    unsigned int group;

    component = tcomponent;
    for (slotIndex = 0; slotIndex < 4; slotIndex++, component++) {
        group = (component->kindAndGroup >> 4) & 3;
        if (groupStatus[group] & 1) {
            continue;
        }
        if (component->flags & 0x10) {
            return 1;
        }
    }
    return 0;
}

void *MBUF_create2(void);
unsigned char *EW_create(int parent, int kind);
int EW_addComponent(unsigned char *ew, int slot, unsigned char *child);

TComponent *TWIN_create2(int slotIndex)
{
    unsigned char *w;
    unsigned char *mb;
    unsigned char *ew;
    unsigned char *c;
    unsigned char *e;
    int i;
    int kind;

    w = (unsigned char *)TWSYS_createComponent(slotIndex, 0);
    if (w == 0) {
        return 0;
    }
    mb = MBUF_create2();
    if (mb == 0) {
        return 0;
    }
    w[0x186] = 0x2D;
    w[0x187] = 3;
    *(float *)(w + 0x20) = 16.0f;
    *(float *)(w + 0x24) = 305.0f;
    *(short *)(w + 0x50) = 0;
    TWIN_init2((TWindowInit2 *)w);
    for (i = 7; i >= 0; i--) {
        ((int *)(w + 0xD0))[i] = 0;
    }
    ew = EW_create(-1, 4);
    *(unsigned char **)(ew + 0x14) = w + 0xD0;
    *(short *)(ew + 0x10) = 8;
    *(unsigned short *)ew |= 0x4000;
    *(unsigned char **)(w + 0x54) = ew;
    c = EW_create(-1, 10);
    *(short *)(c + 0xC) = 0x10;
    *(short *)(c + 0xE) = 0x10;
    EW_addComponent(ew, 1, c);
    e = *(unsigned char **)(w + 0xD4);
    *(int *)(e + 0x14) = 0x808000;
    *(int *)(e + 0x10) |= 0x100;
    c = EW_create(-1, 9);
    *(short *)(c + 0xC) = 0x10;
    *(short *)(c + 0xE) = 0x10;
    EW_addComponent(ew, 0, c);
    c = EW_create(-1, 7);
    *(int *)(c + 0x24) = *(int *)(w + 0x17C);
    *(short *)(c + 0x14) = -1;
    EW_addComponent(ew, 2, c);
    c = EW_create(-1, 9);
    *(short *)(c + 0xC) = 0x200;
    *(short *)(c + 0xE) = 0x1C0;
    *(short *)(c + 0x4) = 0;
    *(short *)(c + 0x6) = 0;
    *(int *)(c + 0x8) = 0;
    EW_addComponent(ew, 3, c);
    *(unsigned short *)c |= 0x4000;
    c = EW_create(-1, 1);
    EW_addComponent(ew, 4, c);
    c = EW_create(-1, 5);
    *(short *)(c + 0xC) = 0x10;
    *(short *)(c + 0xE) = 0x10;
    EW_addComponent(ew, 5, c);
    *(unsigned char **)(w + 0x9C) = mb;
    kind = *(unsigned short *)(w + 0x16) & 0xF;
    *(int *)(w + 0xAC) = *(int *)(mb + 0xC);
    *(short *)(w + 0xB8) = *(unsigned short *)(mb + 4);
    *(int *)(w + 0xA0) = 0;
    *(short *)(w + 0xB4) = 0;
    *(short *)(w + 0xB6) = 0;
    *(short *)(w + 0xBA) = 0;
    *(short *)(w + 0xBC) = 0;
    switch (kind) {
    case 3:
    default:
        WIN_initCF((TWindow *)w);
        break;
    case 4:
        WIN_initScene((TWindow *)w);
        break;
    }
    return (TComponent *)w;
}

void EW_dispose(int handle);
void MBUF_dispose(int handle);

void TWIN_dispose(TComponent *window)
{
    window->flags &= ~0x11;
    EW_dispose(window->ewHandle);
    if (window->msgBuffer[0] != 0) {
        MBUF_dispose(window->msgBuffer[0]);
    }
    if (window->msgBuffer[1] != 0) {
        MBUF_dispose(window->msgBuffer[1]);
    }
}

int MSG_queuePop(void *q, char *src, int n);
void MSG_queueReset(void *q, int n);
char *strcpy(char *, const char *);

void TWIN_popCF(char *w)
{
    void *q;
    short c;
    int v;

    if ((*(unsigned int *)(w + 0x10) & 3) != 1) {
        return;
    }
    if (*(short *)(w + 0xB6) <= 0) {
        return;
    }
    q = w + 0xAC;
    *(short *)(w + 0xC0) = *(short *)(w + 0xC) - 0x18;
    *(unsigned char *)(w + 0x184) += MSG_queuePop(q, ((char **)*(int *)(w + 0x17C))[*(unsigned char *)(w + 0x185)] + *(unsigned char *)(w + 0x184), 2);
    *(short *)(w + *(unsigned char *)(w + 0x185) * 2 + 0x14C) = *(short *)(w + 0xBC);
    c = *(short *)(w + 0xBE);
    if (c >= 0x80) {
        switch (c) {
        case 0x83:
            *(short *)(w + 0x14) = 0xD;
            *(short *)(w + 0x30) = *(short *)(w + 0xC8);
            break;
        case 0x84:
            *(short *)(w + 0x14) = 2;
            break;
        case 0x81:
            *(short *)(w + 0xBA) = 0;
            *(short *)(w + 0x14) = 9;
            *(short *)(w + 0xBC) = 0;
            break;
        case 0x80:
            *(short *)(w + 0x14) = 8;
            *(int *)(w + 0x120) = *(int *)(w + 0xC8);
            break;
        case 0x85:
            strcpy(*(char **)(w + 0xA8), *(char **)(w + 0xC8));
            break;
        case 0x82:
            break;
        }
    } else if (*(short *)(w + 0xBC) >= *(short *)(w + 0xC0) || c == 10) {
        int n, i;
        MSG_queueReset(q, 10);
        i = *(unsigned char *)(w + 0x185);
        n = *(unsigned char *)(w + 0x187) - 1;
        if (i >= n && *(short *)(w + 0xA4) >= n) {
            *(short *)(w + 0x14) = 0x11;
        } else {
            *(short *)(w + 0x14) = 10;
        }
    }
    if (*(short *)(w + 0xB4) < *(short *)(w + 0xB6)) {
        return;
    }
    MSG_queueReset(q, 0);
}

void TWIN_popScene(char *w)
{
    void *q;
    short c;
    short *lw;
    int i;
    unsigned char n;

    if ((*(unsigned int *)(w + 0x10) & 3) != 1) {
        return;
    }
    if (*(short *)(w + 0xB6) <= 0) {
        return;
    }
    q = w + 0xAC;
    lw = (short *)(w + 0xC);
    do {
        *(short *)(w + 0xC0) = *(short *)(w + 0xC);
        *(unsigned char *)(w + 0x184) += MSG_queuePop(q, ((char **)*(int *)(w + 0x17C))[*(unsigned char *)(w + 0x185)] + *(unsigned char *)(w + 0x184), 0x18);
        lw[*(unsigned char *)(w + 0x185) + 0xA0] = *(short *)(w + 0xBC);
        c = *(short *)(w + 0xBE);
        if (c >= 0x80) {
            switch (c) {
            case 0x83:
                *(short *)(w + 0x14) = 0xD;
                *(short *)(w + 0x30) = *(short *)(w + 0xC8);
                break;
            case 0x84:
                *(short *)(w + 0x14) = 2;
                break;
            case 0x81:
                *(short *)(w + 0xBA) = 0;
                *(short *)(w + 0x14) = 9;
                *(short *)(w + 0xBC) = 0;
                break;
            case 0x80:
                *(short *)(w + 0x14) = 8;
                *(int *)(w + 0x120) = *(int *)(w + 0xC8);
                break;
            case 0x85:
                strcpy(*(char **)(w + 0xA8), *(char **)(w + 0xC8));
                break;
            case 0x82:
                break;
            }
            return;
        }
        if (*(short *)(w + 0xBC) >= *(short *)(w + 0xC) || c == 10) {
            MSG_queueReset(q, 0x10A);
            i = *(unsigned char *)(w + 0x185) + 1;
            n = *(unsigned char *)(w + 0x187);
            *(unsigned char *)(w + 0x184) = 0;
            *(unsigned char *)(w + 0x185) = i;
            if ((unsigned char)i >= n) {
                *(unsigned char *)(w + 0x185) = *(unsigned char *)(w + 0x187) - 1;
                *(short *)(w + 0x14) = 0xC;
                *(int *)(w + 0x120) = *(int *)(w + 0xC8);
                return;
            }
        }
    } while (*(short *)(w + 0xB4) < *(short *)(w + 0xB6));
    *(short *)(w + 0xB4) = 0;
    *(short *)(w + 0xB6) = 0;
}

typedef struct PadDataU {
    unsigned char pad_00[0x2A];
    unsigned short pressed;
} PadDataU;
extern PadDataU PadDataAlias[] __asm__("PadData");
typedef struct EwMaskStateU {
    int blend;
    int fix;
} EwMaskStateU;
typedef struct EwMaskWidget {
    unsigned short flags;
    unsigned short type;
    short x;
    short y;
    int z;
    short width;
    short height;
    EwMaskStateU mask;
    unsigned char unmodeled_18[0x10];
} EwMaskWidget;
typedef struct EwLabelWidget {
    unsigned short flags;
    unsigned short type;
    short x;
    short y;
    int z;
    short width;
    short height;
    unsigned char unmodeled_10[2];
    short linePitch;
    short lineCount;
    short firstLine;
    int colorScale;
    unsigned short *lineWidths;
    const unsigned char *textStyle;
    char **textLines;
} EwLabelWidget;
typedef struct TWindowU {
    unsigned char unmodeled_00[0xC];
    unsigned short width;          /* +0xC */
    unsigned short height;         /* +0xE */
    unsigned int flags;            /* +0x10 */
    unsigned short state;          /* +0x14 */
    unsigned short kindAndGroup;   /* +0x16 */
    unsigned char unmodeled_018[0x20 - 0x18];
    float x; /* +0x20 */
    float y; /* +0x24 */
    unsigned char unmodeled_028[0x30 - 0x28];
    short waitFrames; /* +0x30 */
    unsigned short closeState;     /* +0x32 */
    unsigned char unmodeled_034[0x50 - 0x34];
    unsigned short textVerticalOffset; /* +0x50 */
    unsigned char unmodeled_052[0x54 - 0x52];
    void *rootWidget; /* +0x54 */
    unsigned char unmodeled_058[0x9c - 0x58];
    void *messageBuffer[2]; /* +0x9c */
    short currentPage;             /* +0xA4 */
    short line_pitch;              /* +0xA6 */
    char *textOutput;              /* +0xA8 */
    unsigned char queue[0xCC - 0xAC]; /* +0xac */
    unsigned int frameCounter; /* +0xcc */
    void *widgets[8]; /* +0xd0 */
    unsigned int initializationWord0; /* +0xF0 */
    unsigned char unmodeled_f4[0x108 - 0xF4];
    unsigned int initializationWord1; /* +0x108 */
    unsigned char unmodeled_10c[0x120 - 0x10c];
    int pendingMessage; /* +0x120 */
    unsigned char unmodeled_124[0x14c - 0x124];
    unsigned short lineWidths[24]; /* +0x14c */
    char **lineBufferPointers;     /* +0x17C */
    char *textScratchBuffer;       /* +0x180 */
    unsigned char lineCharacterOffset; /* +0x184 */
    unsigned char currentLineIndex; /* +0x185 */
    unsigned char layout_offset;   /* +0x186 */
    unsigned char line_count;      /* +0x187 */
    char *lineData[1];             /* +0x188; inline pointer and character buffers follow */
} TWindowU;

void TWIN_update2(TComponent *component)
{
    unsigned int flags;
    TWindowU *window = (TWindowU *)component;
    EwMaskWidget *mask;
    EwLabelWidget *label;
    int alpha;
    int lineIndex;
    int index;
    char *firstLine;

    window->frameCounter++;
    flags = window->flags;
    if (flags & 1) {
        mask = (EwMaskWidget *)window->widgets[1];
        mask->mask.blend |= 0x100;
        alpha = (mask->mask.fix >> 8) & 0xFF;
        if (alpha != 1) {
            alpha -= 4;
            if (alpha <= 0) {
                label = (EwLabelWidget *)window->widgets[2];
                label->colorScale = 128;
                alpha = 1;
            }
            mask->mask.fix = (alpha << 8) | (alpha << 16);
        }
    }
    switch (window->kindAndGroup & 15) {
    case 3:
    default:
        TWIN_popCF((char *)window);
        break;
    case 4:
        TWIN_popScene((char *)window);
        break;
    }
    if (!(flags & 2)) {
        switch (window->state) {
        case 0:
            break;
        case 1:
        case 8:
        case 9:
        case 11:
        case 12:
        case 17:
            window->waitFrames = 0;
            window->flags |= 2;
            break;
        case 13:
            window->flags |= 2;
            break;
        case 2:
            window->waitFrames = 10;
            window->flags |= 2;
            break;
        case 10:
            window->flags |= 2;
            window->waitFrames = 0;
            break;
        }
        return;
    }
    if (!(flags & 4)) {
        return;
    }
    switch (window->state) {
    case 8:
        window->waitFrames = (window->waitFrames + 1) & 31;
        if (PadDataAlias[0].pressed & 0x20) {
            window->flags &= ~2;
            window->state = 0;
            window->closeState = window->pendingMessage;
        }
        break;
    case 12:
        window->waitFrames = (window->waitFrames + 1) & 31;
        if (PadDataAlias[0].pressed & 0x20) {
            window->flags &= ~2;
            window->state = 0;
            window->lineCharacterOffset = 0;
            window->currentLineIndex = 0;
            for (lineIndex = 0; lineIndex < window->line_count; lineIndex++) {
                window->lineBufferPointers[lineIndex][0] = '\0';
            }
        }
        break;
    case 13:
        window->waitFrames--;
        if ((short)window->waitFrames < 0) {
            window->flags &= ~2;
            window->state = 0;
        }
        break;
    case 9:
        window->flags &= ~2;
        window->state = 0;
        window->currentPage = 0;
        window->lineCharacterOffset = 0;
        window->currentLineIndex = 0;
        for (lineIndex = 0; lineIndex < window->line_count; lineIndex++) {
            window->lineBufferPointers[lineIndex][0] = '\0';
        }
        break;
    case 2:
        window->waitFrames--;
        if ((short)window->waitFrames < 0) {
            window->closeState = 255;
            TWIN_dispose(component);
        }
        break;
    case 11:
        if (PadDataAlias[0].pressed & 0x20) {
            window->flags &= ~2;
            window->state = 0;
            window->currentLineIndex = 0;
            window->currentPage = 0;
            for (lineIndex = 0; lineIndex < window->line_count; lineIndex++) {
                window->lineBufferPointers[lineIndex][0] = '\0';
            }
        }
        window->waitFrames++;
        break;
    case 17:
        window->waitFrames++;
        if (PadDataAlias[0].pressed & 0x20) {
            window->waitFrames = 0;
            window->state = 10;
        }
        break;
    case 10:
        if (window->currentLineIndex >= window->line_count - 1) {
            window->textVerticalOffset = (int)((float)(short)window->waitFrames * -6.0f);
            window->waitFrames++;
            if ((short)window->waitFrames >= 5) {
                firstLine = window->lineBufferPointers[0];
                for (index = 0; index < window->line_count - 1; index++) {
                    window->lineWidths[index] = window->lineWidths[index + 1];
                    window->lineBufferPointers[index] = window->lineBufferPointers[index + 1];
                }
                firstLine[0] = '\0';
                window->state = 0;
                window->lineBufferPointers[window->currentLineIndex] = firstLine;
                window->flags &= ~2;
                window->lineCharacterOffset = 0;
                window->textVerticalOffset = 0;
                if (window->currentPage >= window->line_count - 1) {
                    window->currentPage = 0;
                } else {
                    window->currentPage++;
                }
            }
        } else {
            window->currentLineIndex++;
            window->currentPage++;
            window->flags &= ~2;
            window->state = 0;
            window->lineCharacterOffset = 0;
        }
        break;
    case 1:
        window->waitFrames++;
        if ((short)window->waitFrames < 2) {
            window->textVerticalOffset = 0;
        }
        if ((short)window->waitFrames >= 11) {
            window->state = 0;
            window->flags |= 1;
            window->flags &= ~2;
        }
        break;
    }
}

extern char D_004C1C48[];
extern char D_004DA428[];
extern void xglFontPrint(int x, int y, int ot, char *text);
void EW_sprtSetCursorUV(unsigned char *widget, int kind, int index);

void TWIN_draw2(TComponent *component)
{
  char *w = (char *) component;
  char *a;
  char *b;
  char *p;
  int new_var;
  int y;
  int i;
  int x;
  short t;
  int t2;
  unsigned short s;
  if (((unsigned short) ((*((unsigned short *) (w + 0x14))) + 0xFFFF)) < 2)
  {
    return;
  }
  a = *((char **) (w + 0xD4));
  b = *((char **) (w + 0xE0));
  *((int *) (a + 8)) = 0xFFFFF1;
  x = component->width + 16.0f;
  *((int *) (b + 8)) = 0xFFFFF0;
  y = (component->height + 16.0f) + 32.0f;
  *((short *) (a + 4)) = x;
  *((short *) (a + 6)) = y;
  *((short *) (a + 0xC)) = *((short *) (w + 0xC));
  *((short *) (a + 0xE)) = *((short *) (w + 0xE));
  *((unsigned short *) a) |= 0x4000;
  *((short *) (b + 4)) = x;
  *((short *) (b + 6)) = y;
  *((short *) (b + 0xC)) = *((short *) (w + 0xC));
  *((short *) (b + 0xE)) = *((short *) (w + 0xE));
  *((unsigned short *) b) |= 0x4000;
  b = *((char **) (w + 0xD0));
  *((short *) (b + 4)) = x;
  *((short *) (b + 6)) = y;
  *((int *) (b + 8)) = 0xFFFFFF;
  *((short *) (b + 0xC)) = *((short *) (w + 0xC));
  *((short *) (b + 0xE)) = *((short *) (w + 0xE));
  *((unsigned short *) b) |= 0x4000;
  b = *((char **) (w + 0xD8));
  ((union { int i; float f; } *) (b + 0x18))->i = 0;
  *((short *) (b + 4)) = x;
  *((short *) (b + 4)) = (*((short *) (b + 4))) + 12;

  {
    char *tptr = *((char **) (w + 0x17C));
    unsigned short u50 = *((unsigned short *) (w + 0x50));
    *(int *)(b + 8) = 0xFFFFFF;
    *((short *) (b + 6)) = (u50 + y) + 4;
    ((union { short s; char *p; } *)(b + 0x24))->p = tptr;
  }
  *((short *) (b + 0x14)) = *((unsigned char *) (w + 0x187));
  *((unsigned short *) b) |= 0x4000;

  s = *((unsigned short *) (w + 0x14));
  b = *((char **) (w + 0xE4));
  if (((s == 0xB) || (s == 8)) || (s == 0x11))
  {
    i = *((unsigned char *) (w + 0x185));
    p = w + 0xC;
    t2 = *((short *) (w + 0xC0));
    x = *((short *) ((p + (i * 2)) + 0x140));
    y = i * 24;
    p = b + 6;
    if (t2 < (x + 16))
    {
      x = -8;
      y += 24;
    }
    else
    {
      p = w + 0xC;
      if ((x == 0) && (i != 0))
      {
        t = *((short *) ((p + (i * 2)) + 0x13E));
        if (!(t2 < (t + 16)))
        {
          x = t;
          y = y - 24;
          if ((!y) && (!y))
          {
          }
        }
        else
        {
          x = -8;
        }
      }
    }
    *((int *) (b + 8)) = 0xFFFFFF;
    x = ((float) x) + ((component->width + 20.0f) + 16.0f);
    y = ((float) y) + (((component->height + 12.0f) + 16.0f) + 32.0f);
    *((short *) (b + 4)) = x;
    *((short *) (b + 6)) = y;
    EW_sprtSetCursorUV((unsigned char *) b, 3, *((int *) (w + 0xCC)));
    *((unsigned short *) b) |= 0x4000;
  }
  else
  {
    *((unsigned short *) b) &= 0xBFFF;
  }
  x = component->width + 16.0f;
  y = component->height + 16.0f;
  new_var = 0xA8;
  xglFontPrintDirectOT(0xFFFFF0, D_004C1C48);
  xglFontPrint(x, y, 0xFFFFF0, *((char **) (w + new_var)));
  xglFontPrintDirectOT(0xFFFFF0, D_004DA428);
}

extern char D_004C1C60[];
void EW_sprtSetCursorUV(unsigned char *widget, int kind, int index);

void TWIN_drawScene2(TComponent *component)
{
    char *w = (char *)component;
    char *a;
    char *b;
    int y;
    int x;
    int i;
    unsigned short s;
    char *t;

    if ((unsigned short)(*(unsigned short *)(w + 0x14) + 0xFFFF) < 2) {
        return;
    }
    a = *(char **)(w + 0xD4);
    b = *(char **)(w + 0xD0);
    *(int *)(a + 8) = 0xFFFFF1;
    t = *(char **)(w + 0x17C);
    *(int *)(b + 8) = 0xFFFFFF;
    x = component->width + 16.0f;
    y = component->height + 16.0f;
    *(short *)(a + 6) = y;
    *(short *)(a + 4) = x;
    *(short *)(a + 0xC) = *(short *)(w + 0xC);
    *(short *)(a + 0xE) = *(short *)(w + 0xE);
    *(unsigned short *)a |= 0x4000;
    *(short *)(b + 4) = x;
    *(short *)(b + 6) = y;
    *(short *)(b + 0xC) = *(short *)(w + 0xC);
    *(short *)(b + 0xE) = *(short *)(w + 0xE);
    *(unsigned short *)b |= 0x4000;
    b = *(char **)(w + 0xD8);
    *(short *)(b + 4) = x;
    ((union { int i; float f; } *)(b + 8))->i = 0xFFFFFF;
    *(short *)(b + 6) = *(unsigned short *)(w + 0x50) + y + 4;
    *(short *)(b + 0xC) = *(short *)(w + 0xC);
    *(short *)(b + 0xE) = *(short *)(w + 0xE);
    ((union { short s; char *p; } *)(b + 0x24))->p = t;
    *(short *)(b + 0x14) = *(unsigned char *)(w + 0x187);
    ((union { short s; char *p; } *)(b + 0x1C))->p = w + 0x14C;
    *(short *)(b + 0x12) = 0x1A;
    *(volatile unsigned short *)b |= 0x4000;
    ((volatile union { short s; char *p; } *)(b + 0x20))->p = D_004C1C60;
    s = *(unsigned short *)(w + 0x14);
    b = *(char **)(w + 0xE4);
    if (s == 0xB || s == 8 || s == 0xC || s == 0x11) {
        i = *(unsigned char *)(w + 0x185);
        *(int *)(b + 8) = 0xFFFFFF;
        y = i * *(short *)(w + 0xA6);
        x = *(short *)(w + 0xC) / 2 + *(short *)(w + i * 2 + 0x14C) / 2;
        x = (float)x + (component->width + 4.0f + 16.0f);
        y = (float)y + (component->height + 12.0f + 16.0f);
        *(short *)(b + 6) = y;
        *(short *)(b + 4) = x;
        EW_sprtSetCursorUV((unsigned char *)b, 3, *(int *)(w + 0xCC));
        *(unsigned short *)b |= 0x4000;
    } else {
        *(unsigned short *)b &= 0xBFFF;
    }
}

typedef struct Vector4_TI {
    float x;
    float y;
    float z;
    float w;
} Vector4_TI;
typedef struct MsgBuffer_TI {
    int active;
    unsigned int capacity;
    unsigned char **entry_table;
    unsigned char *cursor;
    unsigned char workspace[0x800];
} MsgBuffer_TI;
typedef struct MsgQueue_TI {
    unsigned char *buffer;
    unsigned char unmodeled_04[4];
    short head;
    short tail;
    short size;
    short count;
    short width;
    short last;
    short limit;
    unsigned char unmodeled_22[6];
    int label;
} MsgQueue_TI;
typedef struct EwMaskState_TI {
    int blend;
    int fix;
} EwMaskState_TI;
typedef struct EwContainerState_TI {
    short childCapacity;
    void **children;
    int unusedByContainer[2];
} EwContainerState_TI;
typedef struct EwContainerWidget_TI {
    unsigned short flags;
    unsigned short type;
    unsigned char unmodeled_04[0x0C];
    EwContainerState_TI container;
    unsigned char unmodeled_20[0x08];
} EwContainerWidget_TI;
typedef struct EwMaskWidget_TI {
    unsigned short flags;
    unsigned short type;
    short x;
    short y;
    int z;
    short width;
    short height;
    EwMaskState_TI mask;
    unsigned char unmodeled_18[0x10];
} EwMaskWidget_TI;
typedef struct EwLabelWidget_TI {
    unsigned short flags;
    unsigned short type;
    short x;
    short y;
    int z;
    short width;
    short height;
    unsigned char unmodeled_10[2];
    short linePitch;
    short lineCount;
    short firstLine;
    int colorScale;
    unsigned short *lineWidths;
    const unsigned char *textStyle;
    char **textLines;
} EwLabelWidget_TI;
typedef struct EwWidget_TI {
    unsigned short flags;
    unsigned short type;
    short x;
    short y;
    int drawValue;
    short width;
    short height;
    unsigned char unmodeled_10[0x18];
} EwWidget_TI;
typedef struct TComponent_TI {
    unsigned char unmodeled_00[0xc];
    unsigned short pixelWidth;
    unsigned short pixelHeight;
    unsigned int flags;
    unsigned char unmodeled_14[0x16 - 0x14];
    unsigned short kindAndGroup;
    unsigned char unmodeled_18[0x20 - 0x18];
    float width;
    float height;
    unsigned char unmodeled_28[0x32 - 0x28];
    unsigned short closeState;
    unsigned char unmodeled_34[0x44 - 0x34];
    Vector4_TI *ownerPosition;
    unsigned char unmodeled_48[0x54 - 0x48];
    int ewHandle;
    unsigned char unmodeled_58[0x9C - 0x58];
    int *msgBuffer[2];
    unsigned char unmodeled_a4[0x5B0 - 0xA4];
} TComponent_TI;
typedef struct TMenu_TI {
    unsigned char unmodeled_00[0x0C];
    short width;
    short height;
    unsigned int flags;
    unsigned short state;
    unsigned char unmodeled_16[0x20 - 0x16];
    float x;
    float y;
    unsigned char unmodeled_28[0x30 - 0x28];
    short waitFrames;
    unsigned char unmodeled_32[0x50 - 0x32];
    unsigned int closePolicy;
    signed char selectedItem;
    signed char selectionResult;
    unsigned char itemCount;
    unsigned char unmodeled_57;
    short firstVisibleItem;
    short visibleLineCount;
    unsigned char unmodeled_5c[0x60 - 0x5C];
    MsgBuffer_TI *msgBuffer[2];
    unsigned char requestedSelection;
    unsigned char unmodeled_69[0xD8 - 0x69];
    MsgQueue_TI queryQueue;
    unsigned int frameCounter;
    int ewHandle;
    void *widgets[16];
    unsigned char itemCapacity;
    unsigned char queryLineCount;
    signed char currentQueryLine;
    signed char queryCharacterOffset;
    char **queryLines;
    char *activeQueryId;
    const char **itemTexts;
    unsigned char unmodeled_150[0x154 - 0x150];
    char *nextQueryId;
    const char *itemStorage[16];
    char *queryLineStorage[3];
    char queryText[1];
} TMenu_TI;
typedef union EwRootView_TI { EwContainerWidget_TI container; EwWidget_TI widget; } EwRootView_TI;
typedef union EwLabelBody_TI {
    EwLabelWidget_TI label;
    EwContainerWidget_TI container;
} EwLabelBody_TI;
typedef union EwLabelSlot_TI {
    unsigned short *lineWidths;
    const unsigned char *textStyle;
    char **textLines;
    const char **readOnlyTextLines;
    int bits;
} EwLabelSlot_TI;

void TMENU_init(TComponent *component)
{
    MsgBuffer_TI *MBUF_create(int byteSize, int messageCount);
    TMenu_TI *menu;
    const char **cursor;
    unsigned char **sourceWords;
    char **destinationWords;
    void *slots;
    int count;
    unsigned char capacity;
    MsgBuffer_TI *messageBuffer;
    EwContainerWidget_TI *root;
    EwMaskWidget_TI *mask;
    EwLabelWidget_TI *label;
    EwWidget_TI *widget;
    char **textLines;
    const char **itemTexts;

    menu = (TMenu_TI *)component;
    capacity = 0x10;
    slots = menu->itemStorage;
    menu->itemTexts = slots;
    slots = menu->queryLineStorage;
    menu->closePolicy = 1;
    messageBuffer = MBUF_create(0x80, 3);
    menu->queryQueue.buffer = messageBuffer->cursor;
    menu->queryQueue.size = 256;
    cursor = menu->itemTexts;
    menu->msgBuffer[0] = messageBuffer;
    menu->itemCapacity = capacity;
    menu->queryQueue.head = 0;
    menu->queryQueue.tail = 0;
    menu->queryQueue.count = 0;
    menu->queryQueue.width = 0;
    menu->msgBuffer[1] = 0;
    count = 15;
    do {
        cursor[count] = 0;
        count -= 1;
    } while (count >= 0);
    menu->queryLines = slots;
    slots = (char **)slots + 3;
    menu->currentQueryLine = 0;
    count = 0;
    menu->queryCharacterOffset = 0;
    menu->queryLineCount = 0;
    menu->activeQueryId = 0;
    sourceWords = menu->msgBuffer[0]->entry_table;
    destinationWords = menu->queryLines;
    do {
        destinationWords[count] = (char *)sourceWords[count];
        count += 1;
    } while (count < 3);
    menu->nextQueryId = (char *)slots;
    count = 15;
    menu->flags = 0x15;
    menu->state = 1;
    menu->x = 120.0f;
    menu->y = 100.0f;
    menu->height = 0x20;
    menu->visibleLineCount = 4;
    menu->itemCount = 0;
    menu->firstVisibleItem = 0;
    menu->selectionResult = 0;
    menu->selectedItem = 0;
    menu->width = 0;
    for (count = 15; count >= 0; count--) {
        menu->widgets[count] = 0;
    }
    count = 0x10;
    root = (EwContainerWidget_TI *)EW_create(-1, 4);
    root->container.children = menu->widgets;
    root->container.childCapacity = 16;
    ((EwRootView_TI *)root)->widget.flags |= 0x4000;
    menu->ewHandle = (int)root;

    widget = (EwWidget_TI *)EW_create(-1, 9);
    widget->width = 16;
    widget->height = 16;
    EW_addComponent((unsigned char *)root, 0, (unsigned char *)&widget->flags);

    widget = (EwWidget_TI *)EW_create(-1, 10);
    widget->width = 16;
    widget->height = 16;
    EW_addComponent((unsigned char *)root, 1, (unsigned char *)&widget->flags);
    mask = (EwMaskWidget_TI *)menu->widgets[1];
    mask->mask.fix = 0x00808000;
    mask->mask.blend |= 0x100;

    widget = (EwWidget_TI *)EW_create(-1, 7);
    label = (EwLabelWidget_TI *)widget;
    itemTexts = menu->itemTexts;
    label->colorScale = 0;
    ((EwLabelBody_TI *)label)->label.lineCount = -1;
    ((EwLabelSlot_TI *)&label->textLines)->readOnlyTextLines = itemTexts;
    EW_addComponent((unsigned char *)root, 2, (unsigned char *)&widget->flags);

    widget = (EwWidget_TI *)EW_create(-1, 7);
    label = (EwLabelWidget_TI *)widget;
    textLines = menu->queryLines;
    label->colorScale = 0;
    ((EwLabelBody_TI *)label)->label.lineCount = -1;
    label->textLines = textLines;
    EW_addComponent((unsigned char *)root, 3, (unsigned char *)&widget->flags);

    widget = (EwWidget_TI *)EW_create(-1, 9);
    widget->width = 512;
    widget->height = 448;
    widget->x = 0;
    widget->y = 0;
    widget->drawValue = 0;
    EW_addComponent((unsigned char *)root, 4, (unsigned char *)&widget->flags);
    widget->flags |= 0x4000;

    widget = (EwWidget_TI *)EW_create(-1, 1);
    EW_addComponent((unsigned char *)root, 5, (unsigned char *)&widget->flags);

    widget = (EwWidget_TI *)EW_create(-1, 5);
    widget->width = 16;
    widget->height = 16;
    EW_addComponent((unsigned char *)root, 6, (unsigned char *)&widget->flags);

    widget = (EwWidget_TI *)EW_create(-1, 5);
    widget->width = 16;
    widget->height = 16;
    EW_addComponent((unsigned char *)root, 7, (unsigned char *)&widget->flags);

    widget = (EwWidget_TI *)EW_create(-1, 5);
    widget->width = 16;
    widget->height = 16;
    EW_addComponent((unsigned char *)root, 8, (unsigned char *)&widget->flags);
    menu->flags = 0x55;
    menu->state = 1;
}

TComponent *TWSYS_createComponent(int slotIndex, int kindAndGroup);
void TMENU_init(TComponent *component);

TComponent *TMENU_create(int slotIndex)
{
    TComponent *component;

    component = TWSYS_createComponent(slotIndex, 1);
    if (component == 0) {
        return 0;
    }

    TMENU_init(component);
    component->width = 48.0f;
    component->height = 64.0f;
    return component;
}

/*
 * The menu-kind component TMENU_create hands back (still assembler in this
 * TU). Only the fields this allocation touches are modeled:
 *  - +0x10 flags: TMENU_dispose clears bits 0x11 when it releases the menu.
 *  - +0x60/+0x64 msgBuffer: TMENU_dispose passes each non-zero entry to
 *    MBUF_dispose.
 *  - +0xD8 queryQueue: TMENU_addQuery pushes a converted message onto it
 *    (an opaque handle: the address is passed on, never dereferenced here).
 *  - +0xFC ewHandle: TMENU_dispose passes it to EW_dispose.
 *  - +0x148 activeQueryId: TMENU_addQuery leaves it alone once it holds a
 *    query.
 *  - +0x154 nextQueryId: copied into activeQueryId when that one is empty.
 */
typedef struct TMenu {
    unsigned char unmodeled_00[0x0C];
    short width;                   /* +0x0C */
    short height;                  /* +0x0E */
    unsigned int flags;                /* +0x10 */
    unsigned char unmodeled_14[0]; /* +0x14: zero-length alias of the former padding member, kept for saved bodies */
    unsigned short state;        /* +0x14 */
    unsigned char unmodeled_16[0x20 - 0x16];
    float x;                     /* +0x20 */
    float y;                     /* +0x24 */
    unsigned char unmodeled_28[0x30 - 0x28];
    short waitFrames;            /* +0x30: signed opening and countdown frames */
    unsigned char unmodeled_32[0x50 - 0x32];
    unsigned int closePolicy;     /* +0x50: bit 0 selects terminal state */
    signed char selectedItem;     /* +0x54 */
    signed char selectionResult;  /* +0x55: -1 for cancelled selection */
    unsigned char itemCount;       /* +0x56 */
    unsigned char unmodeled_57;
    short firstVisibleItem;       /* +0x58 */
    short visibleLineCount;        /* +0x5A */
    unsigned char unmodeled_5c[0x60 - 0x5C];
    MsgBuffer *msgBuffer[2];            /* +0x60 */
    unsigned char unmodeled_68[0]; /* +0x68: zero-length alias of the former padding member, kept for saved bodies */
    unsigned char requestedSelection; /* +0x68: opening flag 0x200 copies this index */
    unsigned char unmodeled_69[0xD8 - 0x69];
    MsgQueue queryQueue;               /* +0xD8 */
    unsigned int frameCounter;         /* +0xF8: cursor animation frame */
    int ewHandle;                      /* +0xFC */
    unsigned char unmodeled_100[0]; /* +0x100: zero-length alias of the former padding member, kept for saved bodies */
    void *widgets[16];                 /* +0x100 */
    unsigned char itemCapacity;    /* +0x140 */
    unsigned char queryLineCount;  /* +0x141: MSG_queueGetInfo line count */
    signed char currentQueryLine;       /* +0x142: signed index read by text reveal */
    signed char queryCharacterOffset;   /* +0x143: signed byte offset within a query line */
    char **queryLines;                 /* +0x144 */
    char *activeQueryId;               /* +0x148 */
    unsigned char unmodeled_14c[0]; /* +0x14c: zero-length alias of the former padding member, kept for saved bodies */
    const char **itemTexts;       /* +0x14C */
    unsigned char unmodeled_150[0x154 - 0x150];
    char *nextQueryId;                 /* +0x154 */
    const char *itemStorage[16];       /* +0x158 */
    char *queryLineStorage[3];         /* +0x198 */
    char queryText[1];                /* +0x1A4: inline string workspace */
} TMenu;

void TMENU_dispose(TMenu *menu)
{
    menu->flags &= ~0x11;
    EW_dispose(menu->ewHandle);
    if (menu->msgBuffer[0] != 0) {
        MBUF_dispose(menu->msgBuffer[0]);
    }
    if (menu->msgBuffer[1] != 0) {
        MBUF_dispose(menu->msgBuffer[1]);
    }
}

void MSG_convert(int *destination, int destinationSize, int messageId, int encoding);
int MSG_queuePush(int queue, int *message, int messageSize, int encoding);

void TMENU_addQuery(TMenu *menu, int messageId)
{
    int message[0x100];

    MSG_convert(message, 0x400, messageId, -1);
    MSG_queuePush((int)&menu->queryQueue, message, 0x400, -1);
    if (menu->activeQueryId == 0) {
        menu->activeQueryId = menu->nextQueryId;
    }
}

int MSG_copyln(char **dst, char **src, int flag);

void TMENU_setItem(unsigned char *menu, int index, char *item)
{
    char buf[1024];
    char *src;
    char *p;
    int n;
    int i;
    int w;

    n = menu[0x140];
    if (index < 0) {
        for (i = 0; i < n; i++) {
            if ((*(int **)(menu + 0x14C))[i] == 0) {
                index = i;
                break;
            }
        }
        if (index < 0)
            return;
        menu[0x56]++;
    }
    src = (*(char ***)(menu + 0x14C))[index] = item;
    p = buf;
    w = MSG_copyln(&p, &src, 0) + 0x38;
    *(short *)(menu + 0xE) = *(short *)(menu + 0x5A) * 0x18 + 4;
    if (*(short *)(menu + 0xC) < w)
        *(short *)(menu + 0xC) = w;
}

void TMENU_addItem(unsigned char *menu, int text)
{
    char buf[1024];
    char *dst;
    char *src;
    char *cur;
    unsigned int n;
    int w;
    int i;
    int *items;
    int x, y;
    int t;

    MSG_convert((int *)buf, 0x400, text, -1);
    cur = *(char **)(menu + 0x154);
    n = menu[0x56];
    w = *(short *)(menu + 0xC);
    *(char * volatile *)&dst = cur;
    *(char * volatile *)&src = buf;
    do {
        (*(char ***)(menu + 0x14C))[n] = cur;
        n++;
        t = MSG_copyln(&dst, &src, 0) + 0x38;
        cur = dst + 1;
        if (w < t)
            w = t;
        dst = cur;
        *(char **)(menu + 0x154) = cur;
    } while (src != 0);
    n = 0;
    for (i = 0; i < menu[0x140]; i++) {
        if ((*(int **)(menu + 0x14C))[i] != 0)
            n++;
    }
    menu[0x56] = n;
    if (*(int *)(menu + 0x148) != 0) {
        y = *(short *)(menu + 0x5A);
        x = menu[0x141] * 3;
        x += y * 3;
        *(short *)(menu + 0xE) = x * 8 + 0x10;
    } else {
        x = *(short *)(menu + 0x5A) * 0x18 + 4;
        x += menu[0x141] * 0x18;
        *(short *)(menu + 0xE) = x;
    }
    if (*(short *)(menu + 0xC) < w)
        *(short *)(menu + 0xC) = w;
}

typedef struct MsgBufferTD {
    int active;
    unsigned int capacity;
    unsigned char **entry_table;
    unsigned char *cursor;
    unsigned char workspace[0x800];
} MsgBufferTD;
typedef struct MsgQueueTD {
    unsigned char *buffer;
    unsigned char unmodeled_04[4];
    short head;
    short tail;
    short size;
    short count;
    short width;
    short last;
    short limit;
    unsigned char unmodeled_22[6];
    int label;
} MsgQueueTD;
typedef struct MsgQueueInfoTD {
    short count;
    short lines;
    short width;
} MsgQueueInfoTD;
typedef struct EwMaskStateTD {
    int blend;
    int fix;
} EwMaskStateTD;
typedef struct EwContainerStateTD {
    short childCapacity;
    void **children;
    int unusedByContainer[2];
} EwContainerStateTD;
typedef struct EwContainerWidgetTD {
    unsigned short flags;
    unsigned short type;
    unsigned char unmodeled_04[0x0C];
    EwContainerStateTD container;
    unsigned char unmodeled_20[0x08];
} EwContainerWidgetTD;
typedef struct EwMaskWidgetTD {
    unsigned short flags;
    unsigned short type;
    short x;
    short y;
    int z;
    short width;
    short height;
    EwMaskStateTD mask;
    unsigned char unmodeled_18[0x10];
} EwMaskWidgetTD;
typedef struct EwLabelWidgetTD {
    unsigned short flags;
    unsigned short type;
    short x;
    short y;
    int z;
    short width;
    short height;
    unsigned char unmodeled_10[2];
    short linePitch;
    short lineCount;
    short firstLine;
    int colorScale;
    unsigned short *lineWidths;
    const unsigned char *textStyle;
    char **textLines;
} EwLabelWidgetTD;
typedef struct EwWidgetTD {
    unsigned short flags;
    unsigned short type;
    short x;
    short y;
    int drawValue;
    short width;
    short height;
    unsigned char unmodeled_10[0x18];
} EwWidgetTD;
typedef struct TMenuTD {
    unsigned char unmodeled_00[0x0C];
    short width;
    short height;
    unsigned int flags;
    unsigned short state;
    unsigned char unmodeled_16[0x20 - 0x16];
    float x;
    float y;
    unsigned char unmodeled_28[0x30 - 0x28];
    short waitFrames;
    unsigned char unmodeled_32[0x50 - 0x32];
    unsigned int closePolicy;
    signed char selectedItem;
    signed char selectionResult;
    unsigned char itemCount;
    unsigned char unmodeled_57;
    short firstVisibleItem;
    short visibleLineCount;
    unsigned char unmodeled_5c[0x60 - 0x5C];
    MsgBufferTD *msgBuffer[2];
    unsigned char requestedSelection;
    unsigned char unmodeled_69[0xD8 - 0x69];
    MsgQueueTD queryQueue;
    unsigned int frameCounter;
    int ewHandle;
    void *widgets[16];
    unsigned char itemCapacity;
    unsigned char queryLineCount;
    signed char currentQueryLine;
    signed char queryCharacterOffset;
    char **queryLines;
    char *activeQueryId;
    const char **itemTexts;
    unsigned char unmodeled_150[0x154 - 0x150];
    char *nextQueryId;
    const char *itemStorage[16];
    char *queryLineStorage[3];
    char queryText[1];
} TMenuTD;
typedef struct XglPadRecordTD {
    unsigned char unmodeled_00[0x2A];
    unsigned short pressed;
    unsigned char unmodeled_2c[0x32 - 0x2c];
    unsigned short mappedPressed;
    unsigned short mappedRepeat;
    unsigned char unmodeled_36[0x48 - 0x36];
    signed char repeat_delay;
    signed char repeat_interval;
    unsigned short repeat_mask;
    signed char repeat_wait;
    signed char repeat_count;
    unsigned char horizontal_dead_zone;
    unsigned char vertical_dead_zone;
    unsigned char actuator[6];
    unsigned char actuator_pending;
    unsigned char unmodeled_57;
    unsigned char button_map[8];
    unsigned char axis_dead_zone[4];
    signed char axis[4];
} XglPadRecordTD;
extern XglPadRecordTD PadData[2];
extern void xglSoundEffectNormalID(int soundEffectId, int pitch);
extern void TW_setPos(TComponent *component);
extern void MSG_queueGetInfo(MsgQueueTD *queue, MsgQueueInfoTD *info, int flags);

/* Port of the upstream matched body (menu state machine), with local types. */
void TMENU_updateDefault(TComponent *component)
{
    TMenuTD *menu = (TMenuTD *)component;
    unsigned int flags;
    MsgQueueInfoTD info;
    EwMaskWidgetTD *mask;
    EwWidgetTD *widget;
    int oldSelection;
    int alpha;
    int secondaryAlpha;
    int code;

    flags = menu->flags;
    if (!(flags & 0x10)) {
        return;
    }
    if (menu->itemCount < menu->visibleLineCount) {
        menu->visibleLineCount = menu->itemCount;
    }
    menu->frameCounter++;
    if (!(flags & 2)) {
        switch (menu->state) {
        case 16: {
            EwMaskWidgetTD *initialMask;
            menu->flags = flags | 2;
            initialMask = (EwMaskWidgetTD *)menu->widgets[1];
            menu->waitFrames = 0;
            initialMask->mask.fix = 0x00808000;
            initialMask->mask.blend |= 0x100;
            break;
        }
        case 1:
            menu->flags = flags | 2;
            menu->waitFrames = 0;
            menu->height = menu->visibleLineCount * 24 + 4;
            MSG_queueGetInfo(&menu->queryQueue, &info, 0);
            if (info.lines > 0) {
                menu->height += info.lines * 24 + 12;
            }
            if (menu->width < info.width + 36) {
                menu->width = info.width + 36;
            }
            menu->queryLineCount = (unsigned char)info.lines;
            break;
        case 14:
        case 15:
            menu->flags = flags | 2;
            menu->waitFrames = 0;
            /* The initial input states continue through the wait-state setup. */
        case 13:
            menu->flags |= 2;
            break;
        case 2:
            menu->waitFrames = 10;
            menu->flags = flags | 2;
            widget = (EwWidgetTD *)menu->ewHandle;
            widget->flags &= 0xBFFF;
            break;
        }
        return;
    }
    if (!(flags & 4)) {
        return;
    }
    switch (menu->state) {
    case 15:
        menu->flags = flags & ~2;
        if (menu->closePolicy & 1) {
            menu->state = 2;
        } else {
            menu->state = 0;
        }
        break;
    case 14:
        if (PadData[0].mappedPressed & 0x40) {
            xglSoundEffectNormalID(2, 0);
            menu->flags &= ~2;
            menu->state = 15;
            menu->selectionResult = -1;
            break;
        }
        oldSelection = menu->selectedItem;
        if (PadData[0].mappedPressed & 0x20) {
            xglSoundEffectNormalID(1, 0);
            menu->flags &= ~2;
            menu->state = 15;
            break;
        }
        menu->waitFrames = (menu->waitFrames + 1) & 0xFF;
        if ((PadData[0].mappedRepeat & 0x1000)
            && oldSelection > 0 && menu->selectionResult == 0) {
            xglSoundEffectNormalID(3, 0);
            menu->selectedItem--;
            menu->waitFrames = 0;
        }
        if ((PadData[0].mappedRepeat & 0x4000)
            && menu->selectedItem < menu->itemCount - 1) {
            xglSoundEffectNormalID(3, 0);
            menu->selectedItem++;
            menu->waitFrames = 0;
        }
        if (menu->selectedItem != oldSelection) {
            if (menu->selectedItem < 0) {
                menu->selectedItem = 0;
            }
            if (menu->selectedItem >= menu->firstVisibleItem + menu->visibleLineCount) {
                menu->firstVisibleItem = menu->firstVisibleItem + 1;
            }
            if (menu->selectedItem >= menu->itemCount) {
                menu->selectedItem = menu->itemCount - 1;
            }
            if (menu->firstVisibleItem > menu->selectedItem) {
                menu->firstVisibleItem = menu->firstVisibleItem - 1;
            }
            if (menu->firstVisibleItem < 0) {
                menu->firstVisibleItem = 0;
            }
            if (menu->firstVisibleItem >= menu->itemCount - menu->visibleLineCount) {
                menu->firstVisibleItem = menu->itemCount - menu->visibleLineCount;
            }
        }
        break;
    case 13:
        menu->waitFrames--;
        if (menu->waitFrames < 0) {
            menu->flags = flags & ~2;
            menu->state = 16;
        }
        break;
    case 2:
        menu->waitFrames--;
        if (menu->waitFrames < 0) {
            TMENU_dispose((TMenu *)menu);
        }
        break;
    case 1:
        menu->waitFrames++;
        TW_setPos(component);
        if (menu->waitFrames >= 11) {
            menu->flags |= 1;
            /* Both query modes enter the same waiting state. */
            if (menu->activeQueryId != 0) {
                menu->state = 16;
                menu->flags &= ~2;
            } else {
                menu->state = 16;
                menu->flags &= ~2;
            }
        }
        if (menu->flags & 0x200) {
            menu->flags &= ~0x200;
            menu->selectedItem = menu->requestedSelection;
        }
        break;
    case 16:
        if (menu->waitFrames < 8) {
            alpha = (int)(128.0f - ((float)menu->waitFrames * 0.125f) * 128.0f);
        } else {
            alpha = 1;
        }
        if (menu->waitFrames < 4) {
            secondaryAlpha = (int)(128.0f - ((float)menu->waitFrames * 0.25f) * 128.0f);
        } else {
            secondaryAlpha = 1;
        }
        menu->waitFrames++;
        mask = (EwMaskWidgetTD *)menu->widgets[1];
        mask->mask.fix = ((unsigned int)secondaryAlpha << 16)
            | ((unsigned int)alpha << 8);
        if (menu->waitFrames >= 8) {
            EwLabelWidgetTD *queryLabel = (EwLabelWidgetTD *)menu->widgets[2];
            EwLabelWidgetTD *itemLabel = (EwLabelWidgetTD *)menu->widgets[3];
            queryLabel->colorScale = 128;
            itemLabel->colorScale = 128;
        }
        widget = (EwWidgetTD *)mask;
        widget->drawValue = 0x00FFFFF1;
        widget->x = (int)(menu->x + 16.0f);
        widget->y = (int)(menu->y + 16.0f);
        widget->width = menu->width;
        widget->height = menu->height;
        if (menu->queryQueue.tail > 0) {
            menu->queryCharacterOffset += MSG_queuePop(&menu->queryQueue,
                (unsigned char *)menu->queryLines[menu->currentQueryLine]
                    + menu->queryCharacterOffset, 1);
            code = menu->queryQueue.last;
            if (code >= 128) {
                switch (code) {
                case 131:
                    menu->state = 13;
                    menu->waitFrames = menu->queryQueue.label;
                    break;
                case 128:
                case 129:
                case 130:
                case 132:
                case 133:
                default:
                    break;
                }
            } else if (menu->queryQueue.width >= menu->width || code == 10) {
                menu->queryCharacterOffset = 0;
                menu->currentQueryLine++;
                MSG_queueReset(&menu->queryQueue, 10);
            }
        }
        if (menu->queryQueue.head >= menu->queryQueue.tail && menu->waitFrames >= 9) {
            MSG_queueReset(&menu->queryQueue, 0);
            menu->flags &= ~2;
            menu->state = 14;
            mask = (EwMaskWidgetTD *)menu->widgets[1];
            mask->mask.fix = 0x00010100;
        }
        break;
    default:
        menu->waitFrames++;
        break;
    }
}

typedef struct MsgBuffer_TD2 {
    int active;
    unsigned int capacity;
    unsigned char **entry_table;
    unsigned char *cursor;
    unsigned char workspace[0x800];
} MsgBuffer_TD2;
typedef struct MsgQueue_TD2 {
    unsigned char *buffer;
    unsigned char unmodeled_04[4];
    short head;
    short tail;
    short size;
    short count;
    short width;
    short last;
    short limit;
    unsigned char unmodeled_22[6];
    int label;
} MsgQueue_TD2;
typedef struct EwContainerState_TD2 {
    short childCapacity;
    void **children;
    int unusedByContainer[2];
} EwContainerState_TD2;
typedef struct EwContainerWidget_TD2 {
    unsigned short flags;
    unsigned short type;
    unsigned char unmodeled_04[0x0C];
    EwContainerState_TD2 container;
    unsigned char unmodeled_20[0x08];
} EwContainerWidget_TD2;
typedef struct EwLabelWidget_TD2 {
    unsigned short flags;
    unsigned short type;
    short x;
    short y;
    int z;
    short width;
    short height;
    unsigned char unmodeled_10[2];
    short linePitch;
    short lineCount;
    short firstLine;
    int colorScale;
    unsigned short *lineWidths;
    const unsigned char *textStyle;
    char **textLines;
} EwLabelWidget_TD2;
typedef struct EwWidget_TD2 {
    unsigned short flags;
    unsigned short type;
    short x;
    short y;
    int drawValue;
    short width;
    short height;
    unsigned char unmodeled_10[0x18];
} EwWidget_TD2;
typedef struct TMenu_TD2 {
    unsigned char unmodeled_00[0x0C];
    short width;
    short height;
    unsigned int flags;
    unsigned short state;
    unsigned char unmodeled_16[0x20 - 0x16];
    float x;
    float y;
    unsigned char unmodeled_28[0x30 - 0x28];
    short waitFrames;
    unsigned char unmodeled_32[0x50 - 0x32];
    unsigned int closePolicy;
    signed char selectedItem;
    signed char selectionResult;
    unsigned char itemCount;
    unsigned char unmodeled_57;
    short firstVisibleItem;
    short visibleLineCount;
    unsigned char unmodeled_5c[0x60 - 0x5C];
    MsgBuffer_TD2 *msgBuffer[2];
    unsigned char requestedSelection;
    unsigned char unmodeled_69[0xD8 - 0x69];
    MsgQueue_TD2 queryQueue;
    unsigned int frameCounter;
    int ewHandle;
    void *widgets[16];
    unsigned char itemCapacity;
    unsigned char queryLineCount;
    signed char currentQueryLine;
    signed char queryCharacterOffset;
    char **queryLines;
    char *activeQueryId;
    const char **itemTexts;
    unsigned char unmodeled_150[0x154 - 0x150];
    char *nextQueryId;
    const char *itemStorage[16];
    char *queryLineStorage[3];
    char queryText[1];
} TMenu_TD2;
typedef union EwLabelBody_TD2 {
    EwLabelWidget_TD2 label;
    EwContainerWidget_TD2 container;
} EwLabelBody_TD2;
typedef struct EwSpriteState_TD2 {
    short flags;
    signed char alpha;
    unsigned short u;
    unsigned short v;
} EwSpriteState_TD2;
typedef struct EwSpriteWidget_TD2 {
    unsigned short flags;
    unsigned short type;
    short x;
    short y;
    int z;
    short width;
    short height;
    EwSpriteState_TD2 sprite;
    unsigned char unmodeled_18[0x10];
} EwSpriteWidget_TD2;
typedef union EwLabelSlot_TD2 {
    unsigned short *lineWidths;
    const unsigned char *textStyle;
    char **textLines;
    const char **readOnlyTextLines;
    int bits;
} EwLabelSlot_TD2;

void TMENU_drawDefault(TComponent *component)
{
    TMenu_TD2 *menu = (TMenu_TD2 *)component;
    EwWidget_TD2 *widget;
    EwLabelBody_TD2 *body;
    int x;
    int y;
    int queryHeight;

    if (!(menu->flags & 0x10)) {
        return;
    }
    if (menu->state == 1 || menu->state == 2) {
        return;
    }
    x = (int)menu->x + 16;
    y = (int)menu->y + 16;
    widget = (EwWidget_TD2 *)menu->widgets[1];
    widget->x = (int)(menu->x + 16.0f);
    widget->drawValue = 0xFFFFF1;
    widget->y = (int)(menu->y + 16.0f);
    widget->width = menu->width;
    widget->height = menu->height;
    widget->flags |= 0x4000;
    widget = (EwWidget_TD2 *)menu->widgets[5];
    widget->drawValue = 0x00FFFFF0;
    widget->x = x;
    widget->y = y;
    widget->width = menu->width;
    widget->height = menu->height;
    widget->flags |= 0x4000;
    if (menu->queryLineCount != 0) {
        queryHeight = menu->queryLineCount * 24 + 12;
    } else {
        queryHeight = 0x0;
    }
    widget = (EwWidget_TD2 *)menu->widgets[0];
    widget->drawValue = 0x00FFFFF1;
    widget->x = x;
    widget->y = y;
    widget->width = menu->width;
    widget->height = menu->height;
    widget->flags |= 0x4000;
    widget = (EwWidget_TD2 *)menu->widgets[3];
    body = (EwLabelBody_TD2 *)widget;
    body->label.textLines = menu->queryLines;
    widget->drawValue = 0xFFFFFF;
    widget->x = x;
    widget->y = y;
    body->label.lineCount = (short)menu->queryLineCount;
    body->label.firstLine = 0x0;
    widget->flags |= 0x4000;
    widget = (EwWidget_TD2 *)menu->widgets[2];
    body = (EwLabelBody_TD2 *)widget;
    widget->x = x + 20;
    widget->drawValue = 0x00FFFFFF;
    widget->y = y + queryHeight;
    body->label.firstLine = (short)menu->firstVisibleItem;
    ((EwLabelSlot_TD2 *)&body->label.textLines)->readOnlyTextLines = menu->itemTexts;
    body->label.lineCount = menu->visibleLineCount + 1;
    widget->flags |= 0x4000;

    widget = (EwWidget_TD2 *)menu->widgets[6];
    widget->x = x + menu->width - 16;
    widget->y = y;
    widget->drawValue = 0x00FFFFFF;
    EW_sprtSetCursorUV((unsigned char *)widget, 1, menu->frameCounter);
    if (menu->firstVisibleItem > 0) {
        widget->flags = widget->flags | 0x4000;
    } else {
        widget->flags = widget->flags & 0xBFFF;
    }
    widget = (EwWidget_TD2 *)menu->widgets[7];
    widget->drawValue = 0x00FFFFFF;
    widget->x = (unsigned short)menu->width + x - 16;
    widget->y = (unsigned short)menu->height + y - 16;
    EW_sprtSetCursorUV((unsigned char *)widget, 2, menu->frameCounter);
    if (menu->firstVisibleItem + menu->visibleLineCount < menu->itemCount) {
        widget->flags |= 0x4000;
    } else {
        widget->flags &= 0xBFFF;
    }
    y = 0xC;
    if (menu->selectedItem >= 0) {
        y = (menu->selectedItem - menu->firstVisibleItem) * 24 + 12;
    }
    if (menu->queryLineCount != 0) {
        queryHeight = 24 * menu->queryLineCount + 12;
    } else {
        queryHeight = 0;
    }
    widget = (EwWidget_TD2 *)menu->widgets[8];
    widget->drawValue = 0x00FFFFFF;
    /* Retain both floating zero-offset additions in the cursor layout. */
    x = (int)(menu->x + 20.0f + 0.0f + 0.0f);
    y = (int)((float)y + (menu->y + 16.0f - 6.0f + (float)queryHeight));
    widget->x = x;
    widget->y = y;
    EW_sprtSetCursorUV((unsigned char *)widget, 0, menu->frameCounter);
    widget->flags |= 0x4000;
}

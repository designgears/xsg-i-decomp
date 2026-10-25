/*
 * OV02 original TU 8: 0x00a04170..0x00a05738 (11 functions)
 */
#include "common.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "umn_procurator.h"

extern PadPrefix PadData;
extern int eMessageNextPage(void *message, int arg);
extern void xglSoundEffectNormalID(int sound_id, int variant);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern char D_00A12060[];

int UmnManzaiControl(void)
{
    unsigned int flag;
    unsigned int half;
    short *next;

    if (!(UmnManzaiFlag & 0x40)) {
        if ((UmnManzaiFlag & 0x30) == 0x30) {
            flag = UmnManzaiFlag;
            half = flag & ~1;
            UmnManzaiFlag = half & ~8;
            if ((flag & 0x206) == 0)
            if (PadData.half_2a == 0x20) {
                UmnManzaiFlag |= 1;
                if (eMessageNextPage(UmnWork + 0x7C + *UmnManzaiText * 0xA4, 0) == 0) {
                    next = UmnEventTextNextGet(1);
                    xglSoundEffectNormalID(1, 0);
                    if (next == 0) {
                        UmnManzaiFlag |= 0x40;
                    } else {
                        UmnManzaiText = next;
                        UmnManzaiFlag |= 8;
                    }
                }
            }
        }
    } else if (!(UmnManzaiFlag & 0x30)) {
        return 0;
    }
    xglFontDebugPrintf(0x80, 0x40, D_00A12060, UmnManzaiFlag);
    return 1;
}

int UmnManzaiInit(void)
{
    UmnManzaiWinTask *window;
    int index;

    if (UmnEventTextMake() < 0) {
        return -1;
    }
    UmnManzaiText = UmnEventTextNextGet(0);
    if (UmnModelSon == 0) {
        MenuModelCreate(&UmnModelSon, 3);
    }
    MenuModelExtFuncSet(UmnModelUkn, UmnProcuratorManzai, 0);
    MenuModelExtFuncSet(UmnModelSon, UmnProcuratorManzai2, 0);
    index = 0;
    window = UmnManzaiWin;
    do {
        UmnObjectTaskCreate(tskUmnManzaiWin, window);

        {
            int next_index = index + 1;
            window->task_index = next_index;
            index = next_index;
        }
        /* Advance this loop to the next task record. */
        window++;
    } while (index < 2);
    UmnManzaiFlag = 6;
    return 0;
}

typedef struct ManzaiSlideTarget {
    short v[2];
} ManzaiSlideTarget;
extern const ManzaiSlideTarget D_00A12070;
extern const ManzaiSlideTarget D_00A12078;
extern char D_00A12080[];
typedef struct ManzaiWinElem {
    short x;
    short y;
    int color;
    struct {
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;
    } c;
} ManzaiWinElem;
typedef struct ManzaiWinMessage {
    int head;
    short x;
    short y;
    int color;
    unsigned char pad_6c[0x10];
    unsigned char mode;
    unsigned char pad_7d[0x27];
} ManzaiWinMessage;
typedef struct ManzaiWinWork {
    unsigned char state;
    unsigned char index;
    unsigned char active;
    unsigned char pad_03;
    short x;
    short y;
    short *text;
    ManzaiWinElem elem[6];
    int unk54;
    int unk58;
    int unk5C;
    ManzaiWinMessage message;
} ManzaiWinWork;
typedef struct ManzaiTask {
    unsigned char pad[0x10];
    int state;
} ManzaiTask;
extern void MoveSlide(short *value, short *destination, float speed);
extern void endPrintExtFunc(int kind, int id, void *data);
extern void eMessageSet(void *message, const char *text);
extern void eMessageMain(void *message);
extern void eMessageModeChange(void *message, int mode);
extern void eMessageTextChange(void *message, int text);

void tskUmnManzaiWin(void *task_opaque, void *work_opaque)
{
    ManzaiTask *task = task_opaque;
    ManzaiWinWork *w = work_opaque;
    ManzaiSlideTarget t;
    ManzaiSlideTarget t2;
    unsigned int flag;
    short old;
    short old2;
    int y;
    short *p;
    int i;
    unsigned char mode;
    unsigned char state;
    short *text;

    mode = UmnWork[1];
    if (mode != 1) {
        task->state = -1;
        return;
    }
    if (task->state == 0) {
        w->state = 0;
        if (w->index == mode) {
            w->x = 0;
            w->y = 0x10;
        } else {
            w->x = 0x30;
            w->y = 0x160;
        }
        for (i = 0; i < 6; i++) {
            w->elem[i].color = 0xFFFFF0;
            w->elem[i].c.b = 0;
            w->elem[i].c.g = 0;
            w->elem[i].c.r = 0;
        }
        w->elem[1].c.a = 0x20;
        w->elem[3].c.a = 0x80;
        w->elem[0].c.a = 0x20;
        w->elem[2].c.a = 0x80;
        w->elem[5].c.a = 0xFF;
        w->elem[4].c.a = 0xFF;
        w->unk58 = 0;
        eMessageSet(&w->message, 0);
        w->message.mode = 2;
        w->active = 0;
        w->message.color = 0xFFFFF2;
        w->text = UmnManzaiText;
        return;
    }
    switch (w->state) {
    case 0:
        w->state = 1;
    case 1:
        if ((UmnManzaiFlag & 0x30) == 0x30) {
            if (*w->text == w->index) {
                w->state = 10;
            } else {
                w->state = 2;
            }
        }
    case 2:
        flag = UmnManzaiFlag;
        UmnManzaiFlag = flag & ~0x200;
        if (flag & 8) {
            text = UmnManzaiText;
            if (*text == w->index) {
                w->state = 10;
                w->text = text;
            } else {
                w->state = 0x14;
            }
            UmnManzaiFlag |= 0x200;
        }
        if (UmnManzaiFlag & 0x40) {
            w->state = 0x14;
        }
        break;
    case 10:
        if (w->active == 0) {
            for (i = 0; i < 6; i++) {
                if (w->index == 1) {
                    w->elem[i].x = 0;
                } else {
                    w->elem[i].x = 0x200;
                }
                if (i & 1) {
                    w->elem[i].y = w->y + 0x50;
                } else {
                    w->elem[i].y = w->y;
                }
            }
        }
        w->state = 11;
        w->active = 1;
    case 11:
        t = D_00A12070;
        old = w->elem[4].x;
        MoveSlide(&w->elem[4].x, &t.v[w->index - 1], 3.0f);
        w->elem[5].x = w->elem[4].x;
        w->elem[3].x = w->elem[2].x = w->elem[2].x + (w->elem[4].x - old);
        if (w->elem[4].x == t.v[w->index - 1]) {
            w->state = 2;
            eMessageTextChange(&w->message, ((int *)w->text)[1]);
            eMessageModeChange(&w->message, 0x22);
        }
        break;
    case 20:
        w->state = 0x15;
        eMessageModeChange(&w->message, 0x70);
    case 21:
        t2 = D_00A12078;
        old2 = w->elem[4].x;
        MoveSlide(&w->elem[4].x, &t2.v[w->index - 1], 3.0f);
        w->elem[5].x = w->elem[4].x;
        w->elem[3].x = w->elem[2].x = w->elem[2].x + (w->elem[4].x - old2);
        if (w->elem[4].x == t2.v[w->index - 1]) {
            if (UmnManzaiFlag & 0x40) {
                task->state = -1;
                return;
            }
            w->active = 0;
            w->state = 2;
        }
        break;
    }
    if (w->active != 0) {
        endPrintExtFunc(0, 3, &w->elem[0]);
        if (w->index == 1) {
            w->message.x = w->x + 0x16;
        } else {
            w->message.x = w->x + 2;
        }
        w->message.y = w->y + 4;
        eMessageMain(&w->message);
    }
    xglFontDebugPrintf(0x80, w->index * 8 + 0x20, D_00A12080, w->state);
}

extern void xglLightIntensityAmbient(void *light, const Vector4 *intensity);
extern void xglLightIntensityParallel(void *light, unsigned int index, const Vector4 *intensity);
extern void xglLightDirection(void *light, unsigned int index, const Vector4 *direction);
typedef union LocalLightVector {
    double alignment; /* 8-byte alignment of the original block copy */
    Vector4 value;
} LocalLightVector;
typedef struct LocalLightTable {
    LocalLightVector ambient;
    LocalLightVector color[3];
    LocalLightVector direction[3];
} LocalLightTable;
extern const LocalLightTable D_00A120F0;

static void LocalLightSet(unsigned int *model)
{
    LocalLightTable table;
    void *light;

    table = D_00A120F0;
    light = (char *)model + 0x510;
    xglLightIntensityAmbient(light, &table.ambient.value);
    xglLightIntensityParallel(light, 0, &table.color[0].value);
    xglLightDirection(light, 0, &table.direction[0].value);
    xglLightIntensityParallel(light, 1, &table.color[1].value);
    xglLightDirection(light, 1, &table.direction[1].value);
    xglLightIntensityParallel(light, 2, &table.color[2].value);
    xglLightDirection(light, 2, &table.direction[2].value);
    *model |= 0x8000;
}

extern void MenuModelSubWindowSet(void *unit, int studioId, int mode);
extern void MenuModelSubWindowBreak(void *unit);
extern void MenuModelMotionSet(void *unit, int motion);
extern LocalLightVector D_00A12160;
extern LocalLightVector D_00A12170;
extern LocalLightVector D_00A12180;
extern char D_00A12190[];
typedef struct Manzai2Win {
    int unk0;
    short cx;
    short cy;
    short rx;
    short ry;
    unsigned char *model;
} Manzai2Win;

void UmnProcuratorManzai2(void *task)
{
    unsigned char *t = (unsigned char *)task;
    unsigned int *model;
    float *a;
    float *b;
    float *c;
    unsigned int *um;
    float *ua;
    float *ub;
    Manzai2Win *win;
    float *r;
    float *rr;
    unsigned char st;
    float mtx[16];
    LocalLightVector v0;
    LocalLightVector v1;
    float mtx2[16];
    short dst[2];
    int x;
    int y;

    if (t[0x11] == 0) {
        return;
    }
    {
        model = *(unsigned int **)(t + 0x20);
        a = (float *)((char *)model + 0xC0);
        b = (float *)((char *)model + 0xD0);
        c = (float *)((char *)model + 0xE0);
        um = *(unsigned int **)((char *)UmnModelUkn + 0x20);
        ua = (float *)((char *)um + 0xC0);
        ub = (float *)((char *)um + 0xD0);
        if (((unsigned char *)UmnModelUkn)[0x13] < 2) {
            a[0x18] = 0.0f;
            return;
        }
        if (t[0x13] < 0xB && (UmnManzaiFlag & 0x40)) {
            t[0x13] = 0xA;
        }
        st = t[0x13];
        switch (st) {
        case 0:
            if (((unsigned char *)UmnModelUkn)[0x13] < 2) {
                break;
            }
            v0 = D_00A12160;
            v1 = D_00A12170;
            *(LocalLightVector *)c = v1;
            a[0] = ua[0];
            a[1] = ua[1];
            a[2] = ua[2];
            b[0] = ub[0];
            b[1] = 1.5707964f - ub[1];
            b[2] = ub[2];
            b[3] = ub[3];
            xglMatrixStackUnit();
            xglMatrixStackRotZ(ub[2]);
            xglMatrixStackRotY(ub[1]);
            xglMatrixStackRotX(ub[0]);
            xglMatrixStackTrans((float *)&v0);
            xglMatrixStackSave((float (*)[4])mtx);
            a[0] += mtx[12];
            a[1] += mtx[13];
            a[2] += mtx[14];
            ((void (*)(unsigned int *, int))LocalLightSet)(model, 1);
            a[0x18] = 0.0f;
            t[0x13] = 1;
        case 1:
            if (((unsigned char *)UmnModelUkn)[0x13] < 2) {
                st = t[0x13];
            } else {
                a[0x18] += 0.2f;
                if (a[0x18] >= 1.0f) {
                    a[0x18] = 1.0f;
                    t[0x13] = 2;
                    st = 2;
                    UmnManzaiFlag |= 0x100;
                } else {
                    st = t[0x13];
                }
            }
            break;
        case 2:
            v1 = D_00A12180;
            MenuModelSubWindowSet(task, 2, 2);
            win = *(Manzai2Win **)(t + 0x4C);
            r = (float *)(win->model + 0xA0);
            rr = (float *)(win->model + 0xD0);
            r[0] = ((float *)model)[0x14];
            r[1] = ((float *)model)[0x15];
            r[2] = ((float *)model)[0x16];
            rr[0] = ((float *)model)[4];
            rr[1] = ((float *)model)[5];
            rr[2] = ((float *)model)[6];
            xglMatrixStackUnit();
            xglMatrixStackRotZ(r[2]);
            xglMatrixStackRotY(r[1]);
            xglMatrixStackRotX(r[0]);
            xglMatrixStackTrans((float *)&v1);
            xglMatrixStackSave((float (*)[4])mtx2);
            rr[0] += mtx2[12];
            win->rx = 0x1B0;
            win->cx = 0x1B0;
            win->ry = 0x170;
            win->cy = 0x170;
            rr[1] += mtx2[13];
            rr[2] += mtx2[14];
            r[0] -= 0.3f;
            if (*UmnManzaiText == 2) {
                t[0x13] = 3;
                st = 3;
            } else {
                t[0x13] = 5;
                st = 5;
            }
        case 3:
        case 5:
        case 10:
            win = *(Manzai2Win **)(t + 0x4C);
            switch (st) {
            case 3:
                x = (unsigned short)win->rx - 0xE0;
                y = (unsigned short)win->ry - 0xF0;
                break;
            case 5:
                x = (unsigned short)win->rx - 0x80;
                y = (unsigned short)win->ry - 0x90;
                break;
            case 10:
                x = win->rx;
                y = win->ry;
                break;
            default:
                goto skip;
            }
            dst[0] = x;
            dst[1] = y;
        skip:
            if (win->cx == dst[0] && win->cy == dst[1]) {
                if (t[0x13] == 0xA) {
                    UmnManzaiFlag &= ~0x20;
                    t[0x13] = 0xB;
                } else {
                    UmnManzaiFlag |= 0x20;
                    t[0x13] = 4;
                }
                st = t[0x13];
                UmnManzaiFlag &= ~4;
            } else {
                MoveSlide(&win->cx, &dst[0], 3.0f);
                MoveSlide(&win->cy, &dst[1], 3.0f);
                st = t[0x13];
            }
            break;
        case 4:
            if (UmnManzaiFlag & 8) {
                if (*UmnManzaiText == 2) {
                    MenuModelMotionSet(task, ((signed char *)UmnManzaiText)[3]);
                    t[0x13] = 3;
                } else {
                    t[0x13] = 5;
                }
                st = t[0x13];
                UmnManzaiFlag |= 4;
            }
            break;
        case 11:
            MenuModelSubWindowBreak(task);
            t[0x10] = 0xFF;
            st = t[0x13];
            break;
        default:
            break;
        }
        xglFontDebugPrintf(0x80, 0x20, D_00A12190, st);
    }
}

static void Rotation(float *out, float *position, float *base, float *rotation)
{
    float matrix[16];

    xglMatrixStackUnit();
    xglMatrixStackRotZ(rotation[2]);
    xglMatrixStackRotY(rotation[1]);
    xglMatrixStackRotX(rotation[0]);
    xglMatrixStackTrans(position);
    xglMatrixStackSave((float (*)[4])matrix);

    out[0] = base[0] + matrix[12];
    out[1] = base[1] + matrix[13];
    out[2] = base[2] + matrix[14];
}

extern StudioCamera *xglStudioGetCamera2(int camera_id);
extern void MenuModelSubWindowSet(void *unit, int studioId, int mode);
extern void MenuModelSubWindowBreak(void *unit);
extern LocalLightVector D_00A121D0;
extern LocalLightVector D_00A121E0;
extern LocalLightVector D_00A121F0;
extern char D_00A12190[];
typedef struct ManzaiSubWindow {
    int unk0;
    short x;
    short y;
    short slideX;
    short slideY;
    unsigned char *model;
} ManzaiSubWindow;

void UmnProcuratorManzai(void *task)
{
    unsigned char *t = (unsigned char *)task;
    unsigned int *model;
    float *a;
    float *b;
    float *c;
    StudioCamera *cam;
    LocalLightVector rot;
    LocalLightVector pos;
    short dst[2];
    int sx;
    int sy;
    ManzaiSubWindow *win;
    float *r;
    int n;
    unsigned char st;

    if (t[0x11] != 0) {
        model = *(unsigned int **)(t + 0x20);
        a = (float *)((char *)model + 0xC0);
        b = (float *)((char *)model + 0xD0);
        c = (float *)((char *)model + 0xE0);
        if (t[0x13] != 0xB && (UmnManzaiFlag & 0x40)) {
            t[0x13] = 0xA;
        }
        st = t[0x13];
        switch (st) {
        case 0:
            t[0x13] = 1;
            ((void (*)(unsigned int *, int))LocalLightSet)(model, 0);
            cam = xglStudioGetCamera2(0);
            rot = D_00A121D0;
            pos = D_00A121E0;
            *(LocalLightVector *)&cam->position = pos;
            *(LocalLightVector *)&cam->rotation = rot;
        case 1:
            n = 0;
            a[0] += 0.02f;
            if (a[0] >= 1.5f) {
                a[0] = 1.5f;
                n = 1;
            }
            a[1] += 0.01f;
            if (a[1] >= 0.0f) {
                a[1] = 0.0f;
                n++;
            }
            b[0] += 0.01f;
            if (b[0] >= 0.0f) {
                b[0] = 0.0f;
                n++;
            }
            b[1] -= 0.01f;
            if (b[1] <= -0.7853982f) {
                b[1] = -0.7853982f;
                n++;
            }
            c[0] -= 0.01f;
            if (c[0] <= 0.5f) {
                c[0] = 0.5f;
                n++;
            }
            c[2] = c[0];
            c[1] = c[0];
            if (n == 5) {
                t[0x13] = 2;
                st = 2;
                break;
            }
            st = t[0x13];
            break;
        case 2:
            if (!(UmnManzaiFlag & 0x100)) {
                break;
            }
            pos = D_00A121F0;
            MenuModelSubWindowSet(task, 1, 2);
            win = *(ManzaiSubWindow **)(t + 0x4C);
            r = (float *)(win->model + 0xA0);
            r[0] = b[0];
            r[1] = b[1];
            r[2] = b[2];
            Rotation((float *)(win->model + 0xD0), (float *)&pos, a, r);
            win->slideX = 0x50;
            win->x = 0x50;
            win->slideY = 0x40;
            win->y = 0x40;
            if (*UmnManzaiText == 1) {
                t[0x13] = 3;
                st = 3;
            } else {
                t[0x13] = 5;
                st = 5;
            }
        case 3:
        case 5:
        case 10:
            win = *(ManzaiSubWindow **)(t + 0x4C);
            switch (st) {
            case 3:
                sx = (unsigned short)win->x + 0xE0;
                sy = (unsigned short)win->y + 0x110;
                goto set;
            case 5:
                sx = (unsigned short)win->x + 0x80;
                sy = (unsigned short)win->y + 0x90;
                goto set;
            case 10:
                sx = win->x;
                sy = win->y;
            set:
                dst[0] = sx;
                dst[1] = sy;
                break;
            }
            if (win->slideX == dst[0] && win->slideY == dst[1]) {
                if (t[0x13] == 0xA) {
                    UmnManzaiFlag &= ~0x10;
                    t[0x13] = 0xB;
                } else {
                    UmnManzaiFlag |= 0x10;
                    t[0x13] = 4;
                }
                st = t[0x13];
                UmnManzaiFlag &= ~2;
            } else {
                MoveSlide(&win->slideX, &dst[0], 3.0f);
                MoveSlide(&win->slideY, &dst[1], 3.0f);
                st = t[0x13];
            }
            break;
        case 4:
            if (UmnManzaiFlag & 8) {
                if (*UmnManzaiText == 1) {
                    t[0x13] = 3;
                } else {
                    t[0x13] = 5;
                }
                st = t[0x13];
                UmnManzaiFlag |= 2;
            }
            break;
        case 11:
            MenuModelSubWindowBreak(task);
            MenuModelExtFuncSet(UmnModelUkn, UmnProcuratorMail, 0);
            st = t[0x13];
            break;
        }
        xglFontDebugPrintf(0x80, 0x18, D_00A12190, st);
    }
}

extern StudioCamera *xglStudioGetCamera2(int camera_id);
extern LocalLightVector D_00A12230;

void UmnProcuratorMail(void *task)
{
    unsigned char *t = (unsigned char *)task;
    unsigned int *model;
    float *a;
    float *b;
    float *c;
    StudioCamera *cam;
    LocalLightVector rot;
    LocalLightVector pos;
    int n;

    if (t[0x11] != 0) {
        model = *(unsigned int **)(t + 0x20);
        a = (float *)((char *)model + 0xC0);
        b = (float *)((char *)model + 0xD0);
        c = (float *)((char *)model + 0xE0);
        switch (t[0x13]) {
        case 0:
            t[0x13] = 1;
            ((void (*)(unsigned int *, int))LocalLightSet)(model, 0);
            cam = xglStudioGetCamera2(0);
            memset(&rot, 0, 0x10);
            rot.value.w = 1.0f;
            pos = D_00A12230;
            *(LocalLightVector *)&cam->position = pos;
            *(LocalLightVector *)&cam->rotation = rot;
            MenuModelUnitOpen(task, 2);
        case 1:
            n = 0;
            c[0] += 0.01f;
            if (c[0] >= 1.0f) {
                c[0] = 1.0f;
                n = 1;
            }
            c[1] = c[0];
            c[2] = c[0];
            a[0] += 0.02f;
            if (a[0] >= 1.0f) {
                a[0] = 1.0f;
                n++;
            }
            a[1] -= 0.01f;
            if (a[1] <= -0.3f) {
                a[1] = -0.3f;
                n++;
            }
            b[0] += 0.01f;
            if (b[0] >= 0.0f) {
                b[0] = 0.0f;
                n++;
            }
            b[1] -= 0.01f;
            if (b[1] <= -0.7853982f) {
                b[1] = -0.7853982f;
                n++;
            }
            if (n == 5) {
                t[0x13] = 2;
            }
            break;
        case 2:
            break;
        }
    }
}

typedef void (*LLS2)(unsigned int *, int);

void UmnProcuratorTop(void *task)
{
    unsigned char *t = (unsigned char *)task;
    unsigned int *model;
    float *a;
    float *b;

    if (t[0x11] != 0) {
        model = *(unsigned int **)(t + 0x20);
        a = (float *)((char *)model + 0xC0);
        b = (float *)((char *)model + 0xD0);
        switch (t[0x13]) {
        case 0:
            a[0] = 1.0f;
            a[1] = -0.3f;
            b[0] = 0.0f;
            b[1] = -0.7853982f;
            ((LLS2)LocalLightSet)(model, 0);
            t[0x13] = 1;
        case 1:
            MenuModelUnitOpen(task, 2);
            t[0x13] = 2;
            break;
        case 2:
            break;
        }
    }
}

void UmnProcuratorGoodBy(void *task)
{
    unsigned char *t = (unsigned char *)task;
    unsigned int *model;
    float *a;
    float *b;

    if (t[0x11] != 0) {
        model = *(unsigned int **)(t + 0x20);
        a = (float *)((char *)model + 0xC0);
        b = (float *)((char *)model + 0xD0);
        switch (t[0x13]) {
        case 0:
            t[0x13] = 1;
            ((LLS2)LocalLightSet)(model, 0);
            MenuModelUnitOpen(task, 3);
        case 1:
            a[0] += 0.4f;
            b[1] += 0.4f;
            if (a[0x18] == 0.0f) {
                t[0x10] = -1;
            }
            break;
        }
    }
}

static inline void UmnProcuratorSetGoodBy(void)
{
    MenuModelExtFuncSet(UmnModelUkn, UmnProcuratorGoodBy, 0);
}

void UmnProcuratorSet(void)
{
    switch (UmnWork[1]) {
    case 0:
        if (UmnModelUkn == 0) {
            MenuModelCreate(&UmnModelUkn, 0x1D);
        }
        MenuModelExtFuncSet(UmnModelUkn, UmnProcuratorTop, 0);
        break;
    case 1:
        if (UmnModelUkn == 0) {
            MenuModelCreate(&UmnModelUkn, 0x1D);
        }
        MenuModelExtFuncSet(UmnModelUkn, UmnProcuratorMail, 0);
        break;
    default:
        UmnProcuratorSetGoodBy();
        break;
    }
}

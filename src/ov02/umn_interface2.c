/*
 * OV02 original TU 6: 0x00a02d68..0x00a03450 (5 functions)
 */
#include "common.h"

typedef struct { char pad[0x10]; unsigned char unk10, unk11, unk12; char pad2; int unk14; int unk18; int unk1C; } UmnBgCubeTask;

typedef struct { int unk0; int unk4; char pad[0x94-8]; float unk94; char pad2[0xA0-0x98]; float unkA0; float unkA4; float unkA8; char pad3[0xD0-0xAC]; float unkD0; float unkD4; float unkD8; } UmnCam;

extern UmnCam *xglStudioGetCamera2(int);

typedef struct { float x, y, z, w; } __attribute__((aligned(16))) UmnBgVec;
typedef struct {
    UmnBgVec ambient;
    UmnBgVec intensity[3];
    UmnBgVec direction[3];
} UmnBgLightParam;
typedef struct {
    UmnBgVec ambient;
    UmnBgVec lights[6];
    float matrix[4][4];
    float color[4][4];
} UmnBgLight;
typedef struct {
    UmnBgVec trans;
    UmnBgVec rot;
    UmnBgVec rot2;
    UmnBgVec scale;
    float place[4][4];
} UmnBgCubeData;
extern unsigned char UmnWork[];
extern UmnBgVec D_00A11E60;
extern UmnBgLightParam D_00A11E70;
extern void *memset(void *dest, int value, unsigned int size);
extern float xglFRand(void);
extern int xglTaskWaitRemove(void *task);
extern void xglMatrixStackUnit(void);
extern void xglMatrixStackScale(const float scale[4]);
extern void xglMatrixStackTrans(const float translation[4]);
extern void xglMatrixStackRotX(float angle);
extern void xglMatrixStackRotY(float angle);
extern void xglMatrixStackRotZ(float angle);
extern void xglMatrixStackSave(float matrix[4][4]);
extern void xglLightIntensityAmbient(UmnBgLight *light, UmnBgVec *ambient);
extern void xglLightIntensityParallel(UmnBgLight *light, int index, UmnBgVec *intensity);
extern void xglLightDirection(UmnBgLight *light, int index, UmnBgVec *direction);
extern void xglLightCalcMatrix(UmnBgLight *light);
extern void nmlModelSetLight(float color[4][4], float matrix[4][4]);
extern void nmlModelSetTexture(int tex);
extern void nmlModelSetPlace(float matrix[4][4]);
extern void nmlModelEntry(int model);

void tskUmnBgCubeMain(void *arg) {
    UmnBgCubeTask *t = arg;
    UmnBgCubeData *d;
    UmnBgVec *rot;
    UmnBgVec *rot2;
    UmnBgVec *scale;
    float (*place)[4];
    UmnBgVec v;
    UmnBgVec s;
    UmnBgLight light;
    UmnBgLightParam lp;
    int i;

    d = (UmnBgCubeData *)t->unk1C;
    rot = &d->rot;
    rot2 = &d->rot2;
    scale = &d->scale;
    place = d->place;
    if (UmnWork[1] == 0xFF) {
        t->unk10 = 0x63;
    }
    switch (t->unk10) {
    case 0:
        t->unk10 = 1;
        t->unk11 = 1;
        memset(&v, 0, 0x10);
        v.w = 1.0f;
        s = D_00A11E60;
        d->trans = v;
        *rot = v;
        *rot2 = v;
        *scale = s;
        rot->x = xglFRand();
        rot->y = xglFRand();
        rot->z = xglFRand();
        rot2->x = xglFRand();
        rot2->y = xglFRand();
        rot2->z = xglFRand();
        scale->z = 7.5f;
        scale->y = 7.5f;
        scale->x = 7.5f;
    case 1:
        for (i = 0; i < 3; i++) {
            (&rot->x)[i] += 0.001f;
        }
        break;
    case 0x63:
        xglTaskWaitRemove(t);
        return;
    }
    if (t->unk11 != 0) {
        xglStudioGetCamera2(t->unk12)->unk0 = 1;
        xglMatrixStackUnit();
        xglMatrixStackScale(&scale->x);
        xglMatrixStackTrans(&d->trans.x);
        xglMatrixStackRotZ(rot->z);
        xglMatrixStackRotY(rot->y);
        xglMatrixStackRotX(rot->x);
        xglMatrixStackSave(d->place);
        lp = D_00A11E70;
        xglLightIntensityAmbient(&light, &lp.ambient);
        xglLightIntensityParallel(&light, 0, &lp.intensity[0]);
        xglLightDirection(&light, 0, &lp.direction[0]);
        xglLightIntensityParallel(&light, 1, &lp.intensity[1]);
        xglLightDirection(&light, 1, &lp.direction[1]);
        xglLightIntensityParallel(&light, 2, &lp.intensity[2]);
        xglLightDirection(&light, 2, &lp.direction[2]);
        xglLightCalcMatrix(&light);
        nmlModelSetLight(light.color, light.matrix);
        nmlModelSetTexture(t->unk14);
        nmlModelSetPlace(place);
        nmlModelEntry(t->unk18);
    }
}

typedef struct { char pad[8]; void *unk8; } UmnTaskSched;
extern void tskUmnBgCubeMain(void *);
extern int UmnBgCubeLex;
extern int UmnBgCubeXtx;
extern UmnTaskSched *task_xmx;
extern UmnBgCubeTask *xglTaskEntryNext(UmnTaskSched *, void (*)(void *), void *);
extern void xglCameraInit(UmnCam *);

int UmnBgCubeInit(int arg0) {
    UmnBgCubeTask *t;
    UmnCam *cam;
    int xtx, lex;
    float z, fov;

    arg0 = (arg0 + 0xF) & ~0xF;
    t = xglTaskEntryNext(task_xmx, tskUmnBgCubeMain, task_xmx ? task_xmx->unk8 : 0);
    t->unk1C = arg0;
    arg0 += 0x80;
    t->unk10 = 0;
    t->unk11 = 0;
    cam = xglStudioGetCamera2(0);
    t->unk12 = 0;
    xglCameraInit(cam);
    cam->unkA0 = 0.0f;
    xtx = UmnBgCubeXtx;
    lex = UmnBgCubeLex;
    cam->unkA4 = 0.0f;
    cam->unkA8 = 0.0f;
    cam->unkD0 = 0.0f;
    cam->unkD4 = 0.0f;
    z = 4.0f;
    fov = 0.91593236f;
    cam->unk0 = 1;
    cam->unk4 = 1;
    cam->unk94 = fov;
    cam->unkD8 = z;
    t->unk14 = xtx;
    t->unk18 = lex;
    return arg0;
}

/*
 * State reached through the global UmnInterface2 pointer; only the two
 * frame counters UmnInterface2Main touches are evidenced.
 */
typedef struct UmnInterface2State {
    unsigned int base_color;
    unsigned char color_a[3];
    unsigned char intensity_a; /* decremented by 8 each frame until zero */
    unsigned char color_b[3];
    unsigned char intensity_b; /* decremented by 2 each frame until zero */
} UmnInterface2State;

typedef struct { char pad[0x14]; unsigned short unk14; char pad2[0x5C - 0x16]; } UmnIf2Render;
extern UmnIf2Render sRender;
extern void sceVif1PkAddDirectDataN(void *packet, const void *data, int count);
typedef struct {
    void *packet;
    char pad[0x2C];
    unsigned long long q[10];
} UmnIf2Context;

static void DrawUmnInterface2(void *context, void *arg) {
    UmnIf2Context *ctx = context;
    UmnInterface2State *st = arg;
    unsigned long long *q = ctx->q;
    unsigned int *w;
    int i;

    q[0] = 0x1000000000000004;
    q[1] = 0xE;
    q[2] = 0;
    q[3] = 0x3F;
    q[4] = 0x44;
    q[5] = 0x42;
    q[6] = (sRender.unk14 << 5) | 0x24120000 | 0x2000000640000000;
    q[7] = 6;
    q[8] = 0x5000D;
    q[9] = 0x47;
    sceVif1PkAddDirectDataN(ctx->packet, q, 5);
    {
    int xs[6] = { 0x100, 0, 0x200, 0x200, 0, 0 };
    int ys[6] = { 0xE0, 0, 0, 0x1C0, 0x1C0, 0 };
    ((unsigned int *)q)[0] = 0x8006;
    ((unsigned int *)q)[1] = 0x30AEC000;
    ((unsigned int *)q)[2] = 0x531;
    ((unsigned int *)q)[3] = 0;
    w = (unsigned int *)&ctx->q[2];
    for (i = 0; i < 6; i++) {
        if (i == 0) {
            w[0] = st->color_a[0];
            w[1] = st->color_a[1];
            w[2] = st->color_a[2];
            w[3] = st->intensity_a;
        } else {
            w[0] = st->color_b[0];
            w[1] = st->color_b[1];
            w[2] = st->color_b[2];
            w[3] = st->intensity_b;
        }
        w += 4;
        w[0] = xs[i] << 4;
        w[1] = ys[i] << 4;
        w[3] = 0;
        w[2] = 0;
        w += 4;
        w[0] = (xs[i] << 4) + 0x6FF8;
        w[1] = (ys[i] << 4) + 0x71F8;
        w[2] = st->base_color;
        w[3] = 0;
        w += 4;
    }
    }
    sceVif1PkAddDirectDataN(ctx->packet, q, 0x13);
}

/* The font queue calls draw(context, arg) with the arg registered below;
 * the context record (read at +0x00 and +0x30) is not yet recovered. */
static void DrawUmnInterface2(void *context, void *arg);
extern void xglFontPrintExtFunc(unsigned int flags, void (*draw)(void *context, void *arg), void *arg);
extern UmnInterface2State *UmnInterface2;

void UmnInterface2Main(void) {
    if (UmnInterface2->intensity_a != 0) {
        UmnInterface2->intensity_a -= 8;
    }
    if (UmnInterface2->intensity_b != 0) {
        UmnInterface2->intensity_b -= 2;
    }
    if (UmnInterface2->intensity_a != 0 || UmnInterface2->intensity_b != 0) {
        xglFontPrintExtFunc(0x03FFFFF0, DrawUmnInterface2, UmnInterface2);
    }
}

void *UmnInterface2Init(void *memory) {
    unsigned int aligned_address;
    UmnInterface2State *color_a_state;
    void *result;
    UmnInterface2State *color_b_state;

    result = 0;
    if (memory != 0) {
        aligned_address = ((unsigned int)memory + 15) & ~15u;
        UmnInterface2 = (UmnInterface2State *)aligned_address;
        result = (void *)(aligned_address + 12);
    }

    color_a_state = UmnInterface2;
    color_a_state->color_a[0] = 0x80;
    color_a_state->intensity_a = 0x80;
    color_a_state->color_a[2] = 0x80;
    color_a_state->color_a[1] = 0x80;
    color_a_state->base_color = 0x00fffffe;
    color_b_state = UmnInterface2;
    color_b_state->intensity_b = 0x80;
    color_b_state->color_b[2] = 0x80;
    color_b_state->color_b[1] = 0x80;
    color_b_state->color_b[0] = 0x80;
    return result;
}

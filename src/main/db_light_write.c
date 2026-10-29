#include "common.h"

extern int mode_004DC5A8;


#include "shared.h"

#include "db_light_write.h"

/*
 * The retail ELF has a GLOBAL OBJECT named cursor at 0x0099C7F0 with size
 * 0x60. The shared declaration and this TU's uses establish a 16-byte
 * HomogeneousVector slot at +0x10; the callback pair is accessed as raw
 * words at +0x50/+0x54. Six slots of the existing vector type express the
 * recorded object extent without assigning meanings to the other words.
 */

typedef struct CursorState {
    int mode;
    unsigned char unmodeled_04[0x0C];
    HomogeneousVector position;
    unsigned char unmodeled_20[0x30];
    CursorCallback callback;
    void *callback_argument;
    unsigned char unmodeled_58[8];
} CursorState;

extern HomogeneousVector cursor[6];


#include "main/xgl_studio.h"

/* Owner declarations used by these debug views; storage remains original scaffolding. */

typedef struct {
    unsigned char unmodeled_00[0x50];
    unsigned int matrix_data_offset;
} MapPartListView;

typedef struct {
    unsigned char unmodeled_00[4];
    MapPartListView *parts;
} MapStageModelView;

typedef struct {
    unsigned char unmodeled_00[0x54];
    MapStageModelView *model;
    unsigned char unmodeled_58[0x28];
    Vector4 stage_color;
} MapGameLoopStateView;

typedef struct {
    unsigned char unmodeled_00[0x28];
    unsigned short half_28;
    unsigned short half_2a;
    unsigned short half_2c;
    unsigned char unmodeled_2e[0x66 - 0x2e];
    signed char stick_x;
    signed char stick_y;
} DbLightWritePadSecondPrefix;

typedef struct {
    unsigned char unmodeled_00[0x28];
    unsigned short half_28;
    unsigned short half_2a;
    unsigned char unmodeled_2c[0x68 - 0x2c];
    DbLightWritePadSecondPrefix second;
} DbLightWritePadData;

typedef struct DebugCursorColour {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
} DebugCursorColour;

typedef struct {
    Matrix4 matrix;
    Matrix4 world;
    Vector4 screen_scale;
    Vector4 screen_offset;
    Matrix4 *local;
} DebugDrawTransform;

typedef struct {
    unsigned int count, primitive, registers, reserved;
    unsigned int payload[8][4];
} DebugRectPacket;

typedef struct {
    DebugRectPacket packet;
    unsigned char unmodeled_90[0x400 - 0x90];
    DebugDrawTransform transform;
    unsigned char unmodeled_4a4[0x4b0 - 0x4a4];
    Vector4 input[2];
    unsigned char unmodeled_4d0[0x10];
    Vector4 output[2];
} DebugLineScratch;

typedef struct {
    DebugRectPacket packet;
    unsigned char unmodeled_90[0x400 - 0x90];
    DebugDrawTransform transform;
    unsigned char unmodeled_4a4[0x4e0 - 0x4a4];
    Vector4 positions[5];
} DebugRectScratch;

extern MapGameLoopStateView GameLoopState;

extern DbLightWritePadData PadData;

extern int visible_0;

extern int partsIndex;

extern const char D_004C2158[];

extern const char D_004DA520[];

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

extern void nmlModelSetPartsVisible(MapPartListView *parts,
                                    int part_index, int visible);

extern Vector4 lightDir[6];

extern Vector4 lightCol[6];

extern void xglLightIntensityParallel(void *light, int index,
                                      const Vector4 *intensity);

extern void xglLightIntensityAmbient(void *light,
                                     const Vector4 *intensity);

extern void xglLightDirection(void *light, unsigned int index,
                              const Vector4 *direction);

extern float atan2f(float y, float x);

extern float xglSin(float angle);

extern float xglCos(float angle);

extern int sceWrite(int descriptor, const void *buffer, int size);
extern double fptodp(float value);
extern const char D_004C2020[];
extern const char D_004C2050[];
extern const char D_004C2088[];
typedef struct { float f[28]; } LightFloats;

void DB_lightWrite(int descriptor)
{
    char buffer[128];
    StudioLight *light;
    int lightNumber;
    int lightIndex;

    xglStudioGetLight(&light);
    sceWrite(descriptor, buffer,
             sprintf(buffer,
                     D_004C2020,
                     0, light->ambient_color.x, light->ambient_color.y,
                     light->ambient_color.z));

    lightIndex = 0;
    do {
        lightNumber = lightIndex + 1;
        sceWrite(descriptor, buffer,
                 sprintf(buffer, D_004C2020, lightNumber,
                         ((LightFloats *)light)->f[lightIndex * 8 + 4],
                         ((LightFloats *)light)->f[lightIndex * 8 + 5],
                         ((LightFloats *)light)->f[lightIndex * 8 + 6]));
        sceWrite(descriptor, buffer,
                 sprintf(buffer, D_004C2050, lightNumber,
                         ((LightFloats *)light)->f[lightIndex * 8 + 8],
                         ((LightFloats *)light)->f[lightIndex * 8 + 9],
                         ((LightFloats *)light)->f[lightIndex * 8 + 10]));
        lightIndex = lightNumber;
    } while (lightIndex < 3);

    {
        const char *stage_format = D_004C2088;
        double stage_red;
        double stage_green;
        double stage_blue;

        stage_red = fptodp(GameLoopState.stage_color.x);
        stage_green = fptodp(GameLoopState.stage_color.y);
        stage_blue = fptodp(GameLoopState.stage_color.z);
        sceWrite(descriptor, buffer,
                 sprintf(buffer, stage_format, stage_red, stage_green, stage_blue));
    }
}

typedef struct TransformParams {
    Matrix4 matrix;
    Matrix4 world;
    Vector4 screenScale;
    Vector4 screenOffset;
    Matrix4 *local;
} TransformParams;

typedef struct { unsigned char r, g, b, a; } DbColor;

extern void xglMatrixStackUnit(void);
extern void xglMatrixStackSave(float matrix[4][4]);
extern void transformVector(Vector4 *destination, const Vector4 *vectors, int count,
                            TransformParams *params);
extern void xglPrimAddGifTagDirect(void *packet, const void *data, int count);
extern Vector4 vRect[4];
extern DbColor col[14];

void drawRect2(Matrix4 *m, int mode)
{
    StudioCamera *camera;
    unsigned int *packet;
    Vector4 *v;
    int i;

    char *spr = (char *)0x70000000;

    mode = mode * 2;

    xglStudioGetCamera(&camera, 0);
    xglMatrixStackUnit();
    xglMatrixStackSave(((TransformParams *)0x70000400)->world);
    ((TransformParams *)0x70000400)->screenOffset = camera->screenOffset;
    ((TransformParams *)(spr + 0x400))->screenScale.x = 1.0f;
    ((TransformParams *)(spr + 0x400))->screenScale.y = -1.0f;
    ((TransformParams *)(spr + 0x400))->screenScale.z = 1.0f;
    ((TransformParams *)(spr + 0x400))->screenScale.w = 1.0f;
    ((TransformParams *)(spr + 0x400))->local = m;
    transformVector((Vector4 *)0x700004E0, vRect, 4, (TransformParams *)0x70000400);
    packet = (unsigned int *)0x70000000;
    packet[0] = 0x8004;
    packet[1] = 0x20064000;
    packet[2] = -0xAF;
    packet[3] = 0;
    packet += 4;
    v = (Vector4 *)0x700004E0;
    for (i = 0; i < 4; i++) {
        packet[0] = col[mode + 1].r;
        packet[1] = col[mode + 1].g;
        packet[2] = col[mode + 1].b;
        packet[3] = col[mode + 1].a;
        packet += 4;
        packet[0] = v->x * 16.0f;
        packet[1] = v->y * 16.0f;
        packet[2] = -1;
        packet[3] = 0;
        packet += 4;
        v++;
    }
    xglPrimAddGifTagDirect(0, (void *)0x70000000, 9);
}

extern void xglMatrixStackUnit(void);
extern void xglMatrixStackSave(float matrix[4][4]);
extern void transformVector(Vector4 *destination, const Vector4 *vectors, int count,
                            TransformParams *params);
extern void xglPrimAddGifTagDirect(void *packet, const void *data, int count);
extern DbColor col[14];
extern Vector4 vBox[5];
#define SPR_PARAMS ((TransformParams *)0x70000400)

void drawBox2(Matrix4 *m, int mode)
{
    StudioCamera *camera;
    unsigned int *packet;
    Vector4 *v;
    int i;

    char *spr = (char *)0x70000000;

    mode = mode * 2;

        xglStudioGetCamera(&camera, 0);
    xglMatrixStackUnit();
    xglMatrixStackSave(SPR_PARAMS->world);
    SPR_PARAMS->screenOffset = camera->screenOffset;
    ((TransformParams *)(spr + 0x400))->screenScale.x = 1.0f;
    ((TransformParams *)(spr + 0x400))->screenScale.y = -1.0f;
    ((TransformParams *)(spr + 0x400))->screenScale.z = 1.0f;
    ((TransformParams *)(spr + 0x400))->screenScale.w = 1.0f;
    ((TransformParams *)(spr + 0x400))->local = m;
    transformVector((Vector4 *)0x700004E0, vBox, 5, SPR_PARAMS);
    packet = (unsigned int *)0x70000000;
    packet[0] = 0x8005;
    packet[1] = 0x20054000;
    packet[2] = -0xAF;
    packet[3] = 0;
    packet += 4;
    v = (Vector4 *)0x700004E0;
    for (i = 0; i < 5; i++) {
        packet[0] = col[mode + 1].r;
        packet[1] = col[mode + 1].g;
        packet[2] = col[mode + 1].b;
        packet[3] = col[mode + 1].a;
        packet += 4;
        packet[0] = v->x * 16.0f;
        packet[1] = v->y * 16.0f;
        packet[2] = -1;
        packet[3] = 0;
        packet += 4;
        v++;
    }
    xglPrimAddGifTagDirect(0, (void *)0x70000000, 0xB);
}

void drawCircle2(Matrix4 *matrix, int color_index, float radius)
{
    int segment;
    int colour;
    const unsigned char *alpha;
    const unsigned char *blue;
    const unsigned char *green;
    const unsigned char *red_column;
    const unsigned char *green_column;
    const unsigned char *blue_column;
    const unsigned char *alpha_column;
    const unsigned char *red;
    StudioCamera *camera;
    DebugLineScratch *scratch = (DebugLineScratch *)0x70000000;
    unsigned int *write;
    float angle;
    float scale;

    color_index *= 2;
    xglStudioGetCamera(&camera, 0);
    xglMatrixStackUnit();
    xglMatrixStackSave(scratch->transform.world);
    scratch->transform.screen_offset = camera->screenOffset;
    scratch->transform.screen_scale.x = 1.0f;
    scratch->transform.screen_scale.y = -1.0f;
    scratch->transform.screen_scale.z = 1.0f;
    scratch->transform.screen_scale.w = 1.0f;
    scratch->transform.local = matrix;
    scratch->input[0].x = 0.0f;
    scratch->input[0].y = 0.0f;
    scratch->input[0].z = 0.0f;
    scratch->input[0].w = 1.0f;
    transformVector(scratch->output, scratch->input, 1, &scratch->transform);
    colour = color_index * 4 + 4;
    angle = 0.0f;
    scale = 16.0f;
    write = (unsigned int *)scratch;
    ((DebugRectPacket *)write)->count = 0x8011;
    ((DebugRectPacket *)write)->primitive = 0x20054000;
    ((DebugRectPacket *)write)->registers = 0xffffff51;
    ((DebugRectPacket *)write)->reserved = 0;
    write += 4;
    red_column = (const unsigned char *)col;
    green_column = (const unsigned char *)col + 1;
    blue_column = (const unsigned char *)col + 2;
    alpha_column = (const unsigned char *)col + 3;
    green = green_column + colour;
    blue = blue_column + colour;
    alpha = alpha_column + colour;
    red = red_column + colour;
    for (segment = 0; segment < 17; segment++) {
        write[0] = *red;
        write[1] = *green;
        write[2] = *blue;
        write[3] = *alpha;
        write += 4;
        write[0] = (unsigned int)((scratch->output[0].x +
                                   xglSin(angle) * radius) * scale);
        write[1] = (unsigned int)((scratch->output[0].y +
                                   xglCos(angle) * radius) * scale);
        angle += 0.39269908f;
        write[2] = 0xffffffffU;
        write[3] = 0;
        write += 4;
    }
    xglPrimAddGifTagDirect(0, (const void *)0x70000000, 35);
}

void drawVector2(Vector4 *arg0, Matrix4 *arg1, int arg2)
{
    StudioCamera *camera;
    unsigned int *packet;
    Vector4 *v;
    DbColor *c;
    int i;

    char *spr = (char *)0x70000000;

    arg2 = arg2 * 2;

    xglStudioGetCamera(&camera, 0);
    xglMatrixStackUnit();
    xglMatrixStackSave(((TransformParams *)(spr + 0x400))->world);
    ((TransformParams *)(spr + 0x400))->screenOffset = camera->screenOffset;
    ((TransformParams *)(spr + 0x400))->screenScale.x = 1.0f;
    ((TransformParams *)(spr + 0x400))->screenScale.z = 1.0f;
    ((TransformParams *)(spr + 0x400))->screenScale.w = 1.0f;
    ((Vector4 *)(spr + 0x4B0))->w = 1.0f;
    ((TransformParams *)(spr + 0x400))->screenScale.y = -1.0f;
    ((Vector4 *)(spr + 0x4B0))->x = 0.0f;
    ((Vector4 *)(spr + 0x4B0))->y = 0.0f;
    ((Vector4 *)(spr + 0x4B0))->z = 0.0f;
    ((TransformParams *)(spr + 0x400))->local = arg1;
    ((Vector4 *)(spr + 0x4C0))->x = arg0->x;
    ((Vector4 *)(spr + 0x4C0))->y = arg0->y;
    ((Vector4 *)(spr + 0x4C0))->z = arg0->z;
    ((Vector4 *)(spr + 0x4C0))->w = 1.0f;
    transformVector((Vector4 *)0x700004E0, (Vector4 *)0x700004B0, 2, (TransformParams *)0x70000400);
    packet = (unsigned int *)0x70000000;
    packet[0] = 0x8002;
    packet[1] = 0x2004C000;
    packet[2] = -0xAF;
    packet[3] = 0;
    packet += 4;
    v = (Vector4 *)0x700004E0;
    c = &col[arg2];
    for (i = 0; i < 2; i++) {
        packet[0] = c->r;
        packet[1] = c->g;
        packet[2] = c->b;
        packet[3] = c->a;
        packet += 4;
        packet[0] = v->x * 16.0f;
        packet[1] = v->y * 16.0f;
        packet[2] = 4294967296.0f - v->z;
        packet[3] = 0;
        packet += 4;
        v++;
        c++;
    }
    xglPrimAddGifTagDirect(0, (void *)0x70000000, 5);
}

typedef struct { Matrix4 m; } MatBlockDv;

void drawVector4S(Vector4 *arg0, Matrix4 *arg1, int arg2, void *arg3)
{
  StudioCamera *camera;
  unsigned int *packet;
  Vector4 *v;
  DbColor *c;
  StudioCamera **new_var;
  int i;
  int n;
  register Matrix4 *m1 = arg1;
  arg2 = arg2 * 2;
  n = arg2;
  xglStudioGetCamera(&camera, 0);
  c = &col[n];
  new_var = &camera;
  ((TransformParams *) (((char *) arg3) + 0x400))->local = m1;
  ((TransformParams *) (((char *) arg3) + 0x400))->screenOffset = (*new_var)->screenOffset;
  *((MatBlockDv *) ((TransformParams *) (((char *) arg3) + 0x400))->world) = *((MatBlockDv *) (*new_var)->viewMatrix);
  ((TransformParams *) (((char *) arg3) + 0x400))->screenScale = (*new_var)->screenScale;
  ((Vector4 *) (((char *) arg3) + 0x4B0))->w = 1.0f;
  ((Vector4 *) (((char *) arg3) + 0x4B0))->x = 0.0f;
  ((Vector4 *) (((char *) arg3) + 0x4B0))->y = 0.0f;
  ((Vector4 *) (((char *) arg3) + 0x4B0))->z = 0.0f;
  ((Vector4 *) (((char *) arg3) + 0x4C0))->x = arg0->x;
  ((Vector4 *) (((char *) arg3) + 0x4C0))->y = arg0->y;
  ((Vector4 *) (((char *) arg3) + 0x4C0))->z = arg0->z;
  ((Vector4 *) (((char *) arg3) + 0x4C0))->w = 1.0f;
  transformVector((Vector4 *) (((char *) arg3) + 0x4E0), (Vector4 *) (((char *) arg3) + 0x4B0), 2, (TransformParams *) (((char *) arg3) + 0x400));
  v = (Vector4 *) (((char *) arg3) + 0x4E0);
  packet = (unsigned int *) arg3;
  packet[0] = 0x8002;
  packet[1] = 0x2004C000;
  packet[2] = -0xAF;
  packet[3] = 0;
  packet += 4;
  for (i = 0; i <= 1; i++)
  {
    c = &(&col[n])[i];
    packet[0] = c->r;
    packet[1] = c->g;
    packet[2] = c->b;
    packet[3] = c->a;
    packet += 4;
    packet[0] = v->x * 16.0f;
    packet[1] = v->y * 16.0f;
    packet[2] = 4294967296.0f - v->z;
    packet[3] = 0;
    packet += 4;
    v++;
  }

  xglPrimAddGifTagDirect(0, arg3, 5);
}

extern void xglMatrixStackLoad(float matrix[4][4]);
extern unsigned char cCursor[24];
typedef struct { Matrix4 m; } MatBlockG;
extern Vector4 vGridXZ[5];

void drawGrid(Matrix4 matrix, float scale)
{
    StudioCamera *camera;
    StudioCamera *cam;
    char *spr = (char *)0x70000000;
    Vector4 *v;
    Vector4 s;
    float sp10[4][4];
    unsigned int *packet;
    int i;

    xglStudioGetCamera(&camera, 0);
    cam = camera;
    SPR_PARAMS->screenOffset = cam->screenOffset;
    *(MatBlockG *)SPR_PARAMS->world = *(MatBlockG *)cam->viewMatrix;
    SPR_PARAMS->screenScale = cam->screenScale;
    s.x = scale;
    s.y = scale;
    s.z = scale;
    s.w = 1.0f;
    SPR_PARAMS->local = sp10;
    xglMatrixStackLoad(matrix);
    xglMatrixStackScale(&s.x);
    xglMatrixStackSave(sp10);
    transformVector((Vector4 *)(spr + 0x4E0), vGridXZ, 5, (TransformParams *)0x70000400);
    v = (Vector4 *)(spr + 0x4E0);
    for (i = 0; i < 5; i++) {
        if (v->x > 2560.0f) {
            return;
        }
        if (v->x < 1536.0f) {
            return;
        }
        if (v->y > 2560.0f || v->y < 1536.0f) {
            return;
        }
        if (0.0f > v->z) {
            return;
        }
        v = v + 1;
    }
    *(unsigned int *)0x70000000 = 0x8005;
    *(unsigned int *)0x70000004 = 0x20054000;
    *(unsigned int *)0x70000008 = 0xFFFFFF51u;
    *(unsigned int *)0x7000000C = 0;
    v = (Vector4 *)0x700004E0;
    packet = (unsigned int *)0x70000010;
    for (i = 0; i < 5; i++) {
        packet[0] = cCursor[0];
        packet[1] = cCursor[1];
        packet[2] = cCursor[2];
        packet[3] = cCursor[3];
        packet += 4;
        packet[0] = v[0].x * 16.0f;
        packet[1] = v->y * 16.0f;
        packet[2] = v->z;
        v = v + 1;
        packet[3] = 0u;
        packet = packet + 4;
    }
    xglPrimAddGifTagDirect(0, spr, 11);
}

static StudioCamera *getCurrentCamera(void)
{
    int camera_index;
    StudioCamera *camera;

    camera_index = 0;
    for (;;) {
        xglStudioGetCamera(&camera, camera_index);
        camera_index++;
        if (camera->active != 0)
            return camera;
        if (camera_index >= 8)
            return 0;
    }
}

extern unsigned char cCursor[24];
typedef struct { Matrix4 m; } MatBlockAx;
extern Vector4 vCursor[6];

void drawAxis(Matrix4 matrix, float scale)
{
  char *spr = (char *)0x70000000;
  StudioCamera *camera;
  unsigned int *packet;
  Vector4 *v;
  Vector4 s;
  unsigned char *new_var;
  unsigned char *new_var2;
  Matrix4 sp10;
  int i;
  camera = getCurrentCamera();
  ((TransformParams *)(spr + 0x400))->screenOffset = camera->screenOffset;
  *((MatBlockAx *)((TransformParams *)(spr + 0x400))->world) = *((MatBlockAx *)camera->viewMatrix);
  ((TransformParams *)(spr + 0x400))->screenScale = camera->screenScale;
  *(volatile Matrix4 **)&((TransformParams *)(spr + 0x400))->local = &sp10;
  s.x = scale;
  s.y = scale;
  s.z = scale;
  s.w = 1.0f;
  xglMatrixStackLoad(matrix);
  xglMatrixStackScale(&s.x);
  xglMatrixStackSave(sp10);
  transformVector((Vector4 *)(spr + 0x4E0), vCursor, 6, (TransformParams *)(spr + 0x400));
  v = (Vector4 *)(spr + 0x4E0);
  if ((v->x > 2560.0f) || (v->x < 1536.0f))
  {
    return;
  }
  if ((v->y > 2560.0f) || (v->y < 1536.0f))
  {
    return;
  }
  if (v->z < 0.0f)
  {
    return;
  }
  *((unsigned int *) 0x70000000) = 0x8006;
  *((unsigned int *) 0x70000004) = 0x2004C000;
  *((unsigned int *) 0x70000008) = -0xAF;
  *((unsigned int *) 0x7000000C) = 0;
  packet = (unsigned int *) 0x70000010;
  for (i = 0; i < 6; i++)
  {
    packet[0] = cCursor[(i * 4) + 0];
    new_var = &cCursor[(i * 4) + 3];
    new_var2 = (unsigned char *)((unsigned int)cCursor + (i * 4) + 2);
    packet[1] = ((unsigned char *)cCursor)[(i * 4) + 1];
    packet[2] = *new_var2;
    packet[3] = *new_var;
    packet += 4;
    packet[0] = v->x * 16.0f;
    packet[1] = v->y * 16.0f;
    packet[2] = 0x7FFFFFFF;
    packet[3] = 0;
    packet += 4;
    v++;
  }

  xglPrimAddGifTagDirect(0, (void *) 0x70000000, 0xD);
}

typedef struct {
    Matrix4 m;
} Matrix4Block;

typedef struct {
    float elements[16];
} Matrix;
extern void MATRIX_transpose3(Matrix4 matrix);
extern void xglMatrixMul(Matrix4 destination, Matrix4 left, Matrix4 right);
extern float dptofp(double value);
extern const float D_004D7CB8;

void drawCircle3(const Vector4 *position, Matrix4 *matrix, int color_index)
{
    StudioCamera *camera_slot;
    StudioCamera **camera = &camera_slot;
    Matrix rotation;
    unsigned int *write;
    const unsigned char *red;
    const unsigned char *green;
    const unsigned char *blue;
    const unsigned char *alpha;
    const unsigned char *red_column;
    const unsigned char *green_column;
    const unsigned char *blue_column;
    const unsigned char *alpha_column;
    double magnitude;
    double zero;
    float offset_x;
    Vector4 *centre;
    float radius;
    float angle;
    int segment;
    int colour;

    color_index *= 2;
    xglStudioGetCamera(camera, 0);
    ((DebugLineScratch *)0x70000000)->transform.screen_offset = (*camera)->screenOffset;
    *(Matrix4Block *)((DebugLineScratch *)0x70000000)->transform.world = *(Matrix4Block *)(*camera)->viewMatrix;
    ((DebugLineScratch *)0x70000000)->transform.screen_scale = (*camera)->screenScale;
    ((DebugLineScratch *)0x70000000)->transform.local = matrix;
    ((DebugLineScratch *)0x70000000)->input[0].x = position->x;
    ((DebugLineScratch *)0x70000000)->input[0].y = position->y;
    ((DebugLineScratch *)0x70000000)->input[0].z = position->z;
    ((DebugLineScratch *)0x70000000)->input[0].w = 1.0f;
    transformVector(((DebugLineScratch *)0x70000000)->output, ((DebugLineScratch *)0x70000000)->input, 1, &((DebugLineScratch *)0x70000000)->transform);
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        "lq $2, 16(%1)\n"
        "sq $2, 16(%0)\n"
        "lq $2, 32(%1)\n"
        "sq $2, 32(%0)\n"
        "lq $2, 48(%1)\n"
        "sq $2, 48(%0)\n"
        :
        : "r"(&rotation), "r"((char *)*camera + 0x170)
        : "$2", "memory");
    rotation.elements[12] = 0.0f;
    rotation.elements[13] = 0.0f;
    rotation.elements[14] = 0.0f;
    MATRIX_transpose3((float (*)[4])rotation.elements);
    ((DebugLineScratch *)0x70000000)->transform.local = (Matrix4 *)rotation.elements;
    xglMatrixMul(*matrix, (float (*)[4])rotation.elements, *matrix);
    ((DebugLineScratch *)0x70000000)->input[0].x = position->w;
    ((DebugLineScratch *)0x70000000)->input[0].y = 0.0f;
    ((DebugLineScratch *)0x70000000)->input[0].z = 0.0f;
    ((DebugLineScratch *)0x70000000)->input[0].w = 1.0f;
    write = (unsigned int *)0x70000000;
    zero = 0.0;
    transformVector(&((DebugLineScratch *)0x70000000)->output[1], ((DebugLineScratch *)0x70000000)->input, 1, &((DebugLineScratch *)0x70000000)->transform);
    write[0] = 0x8011;
    write[1] = 0x20054000;
    write[2] = 0xffffff51;
    write[3] = 0;
    write += 4;
    magnitude = fptodp(((Vector4 *)0x700004f0)->x - 2048.0f);
    if (magnitude < zero)
        magnitude = zero - magnitude;
    radius = dptofp(magnitude);
    colour = color_index * 4 + 4;
    red_column = (const unsigned char *)col;
    green_column = (const unsigned char *)col + 1;
    blue_column = (const unsigned char *)col + 2;
    alpha_column = (const unsigned char *)col + 3;
    green = green_column + colour;
    blue = blue_column + colour;
    alpha = alpha_column + colour;
    red = red_column + colour;
    centre = (Vector4 *)0x700004e0;
    angle = 0.0f;
    for (segment = 16; segment >= 0; segment--) {
        write[0] = *red;
        write[1] = *green;
        write[2] = *blue;
        write[3] = *alpha;
        write += 4;
        write[0] = (centre->x + xglSin(angle) * radius) * 16.0f;
        write[1] = (centre->y + xglCos(angle) * radius) * 16.0f;
        angle += D_004D7CB8;
        write[2] = 0xffffffffU;
        write[3] = 0;
        write += 4;
    }
    xglPrimAddGifTagDirect(0, (const void *)0x70000000, 35);
}

typedef struct { Matrix4 m; } MatBlock64;

void drawTags(Vector4 *arg0, Matrix4 *arg1, const char *arg2)
{
    char *spr = (char *)0x70000000;
    StudioCamera *camera;
    StudioCamera *cam;
    Vector4 *v;

    xglStudioGetCamera(&camera, 0);
    cam = camera;
    ((TransformParams *)(spr + 0x400))->screenOffset = cam->screenOffset;
    *((MatBlock64 *)((TransformParams *)(spr + 0x400))->world) = *((MatBlock64 *)cam->viewMatrix);
    ((TransformParams *)(spr + 0x400))->screenScale = cam->screenScale;
    *(Matrix4 **)(spr + 0x4A0) = arg1;
    ((Vector4 *)(spr + 0x4B0))->x = arg0->x;
    ((Vector4 *)(spr + 0x4B0))->y = arg0->y;
    ((Vector4 *)(spr + 0x4B0))->z = arg0->z;
    ((Vector4 *)(spr + 0x4B0))->w = 1.0f;
    transformVector((Vector4 *)(spr + 0x4E0), (Vector4 *)(spr + 0x4B0), 1, (TransformParams *)(spr + 0x400));
    v = (Vector4 *)(spr + 0x4E0);
    if (2560.0f < v->x)
        return;
    if (v->x < 1536.0f)
        return;
    if (2560.0f < v->y)
        return;
    if (v->y < 1536.0f)
        return;
    if (v->z < 0.0f)
        return;
    xglFontDebugPrintf((int)(((v->x - 2048.0f) + 256.0f) * 0.5f),
                       (int)(((v->y - 2048.0f) + 224.0f) * 0.5f), arg2);
}

typedef struct DbLightVector3 { float x, y, z; } DbLightVector3;
extern StudioCamera *xglStudioGetActiveCamera(void);

void drawVector4(const DbLightVector3 *vector, Matrix4 *matrix, int color_index)
{
    StudioCamera *camera;
    DebugLineScratch *scratch = (DebugLineScratch *)0x70000000;
    Vector4 *position;
    unsigned int (*write)[4];
    int vertex_index;
    const unsigned char *colour;

    color_index *= 2;
    camera = xglStudioGetActiveCamera();
    *(Matrix4Block *)scratch->transform.world = *(Matrix4Block *)camera->viewMatrix;
    scratch->transform.screen_offset = camera->screenOffset;
    scratch->transform.screen_scale = camera->screenScale;
    scratch->input[0].w = 1.0f;
    scratch->input[0].x = 0.0f;
    scratch->input[0].y = 0.0f;
    scratch->input[0].z = 0.0f;
    scratch->transform.local = matrix;
    scratch->input[1].x = vector->x;
    scratch->input[1].y = vector->y;
    scratch->input[1].z = vector->z;
    scratch->input[1].w = 1.0f;
    transformVector(scratch->output, scratch->input, 2, &scratch->transform);
    position = scratch->output;
    write = (unsigned int (*)[4])scratch;
    (*write)[0] = 0x8002;
    (*write)[1] = 0x2004c000;
    (*write)[2] = 0xffffff51;
    (*write)[3] = 0;
    write++;
    colour = (const unsigned char *)col + color_index * 4;
    for (vertex_index = 0; vertex_index < 2; vertex_index++) {
        (*write)[0] = colour[0];
        (*write)[1] = colour[1];
        (*write)[2] = colour[2];
        (*write)[3] = colour[3];
        colour += 4;
        write++;
        (*write)[0] = (unsigned int)(position->x * 16.0f);
        (*write)[1] = (unsigned int)(position->y * 16.0f);
        (*write)[2] = (unsigned int)(4294967296.0f - position->z);
        (*write)[3] = 0;
        position++;
        write++;
    }
    xglPrimAddGifTagDirect(0, (const void *)0x70000000, 5);
}

float ball2point(const Vector4 *cursor_position,
                 const Vector4 *actor_position, float radius)
{
    Vector4 difference;
    float distance;

    difference.x = cursor_position->x - actor_position->x;
    difference.y = cursor_position->y - actor_position->y;
    difference.z = cursor_position->z - actor_position->z;
    xglVectorLength(&distance, &difference);
    if (distance < radius)
        return distance;
    return -1.0f;
}

void *prevActor(int startIndex)
{
    int i;

    for (i = startIndex; i >= 0; i--) {
        if (actor[i].inUseId != 0 && !(actor[i].flags & 8)) {
            return &actor[i];
        }
    }
    return 0;
}

ActorHead *nextActor(register int startIndex)
{
    register ActorHead *entry;
    ActorHead *end;

    if (startIndex < ACTOR_COUNT) {
        startIndex *= sizeof(ActorHead);
        entry = (ActorHead *)((char *)actor + startIndex);
        end = &actor[ACTOR_COUNT];
        for (;;) {
            if (entry->inUseId != 0 && !(entry->flags & 8)) {
                return entry;
            }
            entry++;
            if ((int)entry >= (int)end) {
                break;
            }
        }
    }
    return 0;
}

extern void xglVectorNormal(Vector4 *destination, const Vector4 *source);
extern void xglVectorMulMat(Vector4 *destination, Matrix4 matrix, const Vector4 *vector);
extern float D_004D7CBC;
typedef struct {
    unsigned short unk0;
    unsigned short buttons;
    unsigned char unmodeled_04[4];
    signed char unk8;
    signed char unk9;
} KeyDataView;
extern KeyDataView keyData;
typedef int TIq __attribute__((mode(TI)));
typedef union {
    Matrix4 m;
    Vector4 row[4];
} __attribute__((aligned(16))) MatQ;

void updateCursorMode1(void)
{
    Vector4 vel;
    Vector4 out;
    MatQ mat;
    StudioCamera *camera;
    float len;
    int mode;
    KeyDataView *kd;
    HomogeneousVector *pos;
    HomogeneousVector *c0;

    kd = &keyData;
    xglStudioGetCamera(&camera, 0);
    pos = &cursor[1];
    if (kd->buttons & 2) {
        int next;
        c0 = pos - 1;
        next = *(int *)&c0->y + 1;
        *(int *)&c0->y = next;
        if (next >= 3) {
            *(int *)&c0->y = 0;
        }
    }
    len = 0.0f;
    mode = *(int *)&cursor[0].y;
    switch (mode) {
    case 0:
        vel.x = (float)kd->unk8;
        vel.y = (float)-kd->unk9;
        vel.z = 0.0f;
        vel.w = 1.0f;
        break;
    case 1:
        vel.x = (float)kd->unk8;
        vel.y = 0.0f;
        vel.z = 0.0f;
        vel.w = 1.0f;
        break;
    case 2:
        vel.x = 0.0f;
        vel.y = (float)-kd->unk9;
        vel.z = 0.0f;
        vel.w = 1.0f;
        break;
    }
    xglVectorLength(&len, &vel);
    if (len > 40.0f) {
        {
            __asm__ __volatile__(
                "lq $2, 0(%1)\n\t"
                "sq $2, 0(%0)\n\t"
                "lq $2, 16(%1)\n\t"
                "sq $2, 16(%0)\n\t"
                "lq $2, 32(%1)\n\t"
                "sq $2, 32(%0)\n\t"
                "lq $2, 48(%1)\n\t"
                "sq $2, 48(%0)"
                : : "r"(&mat), "r"((char *)camera->unmodeled_e0 + 0x90) : "$2", "memory");
        }
        ((volatile int *)&mat)[12] = 0;
        ((volatile int *)&mat)[13] = 0;
        ((volatile int *)&mat)[14] = 0;
        xglVectorMulMat(&out, mat.m, &vel);
        xglVectorNormal(&out, &out);
        len = D_004D7CBC;
        pos->x += out.x * len;
        pos->y += out.y * len;
        pos->z += out.z * len;
    }
}

extern void ACT_updateSequence(void *actor);

void updateCursorMode2(void)
{
    StudioCamera *camera;
    DbLightWritePadSecondPrefix *pad;
    ActorHead *p;
    HomogeneousVector *pos;
    HomogeneousVector *c;
    HomogeneousVector *c3;
    HomogeneousVector *c4;
    HomogeneousVector *cl;
    HomogeneousVector *hv;
    HomogeneousVector *rot;
    unsigned short held;
    float thr;
    int i;

    pad = &PadData.second;
    p = 0;
    xglStudioGetCamera(&camera, 0);
    pos = &cursor[1];
    if (pad->half_2a & 8) {
        c = pos - 1;
        p = nextActor(*(int *)&c->z);
        if (p == 0) {
            *(int *)&c->z = 0;
        } else {
            *(int *)&c->z = p->unmodeled_04[0x7C] + 1;
        }
    }
    if (pad->half_2a & 2) {
        c = cursor;
        p = prevActor(*(int *)&c->z);
        if (p == 0) {
            *(int *)&c->z = 0x3F;
        } else {
            *(int *)&c->z = p->unmodeled_04[0x7C] - 1;
        }
    }
    if (p != 0) {
        hv = (HomogeneousVector *)&p->unmodeled_04[0x0C];
        pos->x = hv->x;
        pos->y = hv->y;
        pos->z = hv->z;
    }
    c3 = cursor;
    if (*(ActorHead **)&c3[3].y != 0 && *(int *)&c3[3].x == 0) {
        rot = (HomogeneousVector *)&(*(ActorHead **)&c3[3].y)->unmodeled_04[0x4C];
        held = pad->half_28;
        if (held & 0x1000) {
            c3[4].y += 0.033333335f;
        }
        if (held & 0x4000) {
            c3[4].y -= 0.033333335f;
        }
        if (held & 0x2000) {
            rot->y += 0.026179939f;
        }
        if (held & 0x8000) {
            rot->y -= 0.026179939f;
        }
    }
    c4 = cursor;
    pad = (DbLightWritePadSecondPrefix *)&PadData;
    if (*(ActorHead **)&c4[3].y != 0) {
        if (pad->half_2a & 0x10) {
            p = *(ActorHead **)&c4[3].y;
            *(ActorHead **)&c4[3].y = 0;
            *(int *)&p->unmodeled_04[0x2C] = 0;
            *(int *)&p->unmodeled_04[0x30] = 0;
            *(int *)&p->unmodeled_04[0x34] = 0;
        } else if (*(int *)&c4[3].x == 0) {
            p = *(ActorHead **)&c4[3].y;
            hv = (HomogeneousVector *)&p->unmodeled_04[0x0C];
            hv->x = pos->x + c4[4].x;
            hv->y = pos->y + c4[4].y;
            hv->z = pos->z + c4[4].z;
            if (mode_004DC5A8 == 0) {
                ACT_updateSequence(p);
            }
        }
    } else if (pad->half_2a & 0x10) {
        *(int *)&c4[4].x = 0;
        *(int *)&c4[4].y = 0;
        *(int *)&c4[4].z = 0;
        thr = c4[4].x;
        p = actor;
        for (i = 0x3F; i >= 0; i--) {
            if (p->inUseId != 0 && !(p->flags & 8) &&
                thr <= ball2point((Vector4 *)pos, (Vector4 *)&p->unmodeled_04[0x0C], 0.5f)) {
                cl = cursor;
                *(ActorHead **)&cl[3].y = p;
            }
            p++;
        }
    }
}

typedef struct {
    int pad0[3];
    int modeC;
    int mode10;
    int pad14[7];
    float x30, y30, z30;
    int pad3C;
    int flag40;
    HomogeneousVector *target44;
    int pad48[(0x4D0 - 0x48) / 4];
    float x4D0, y4D0, z4D0;
    int pad4DC;
    int flag4E0;
    HomogeneousVector *target4E4;
} TCameraView;

extern TCameraView tcamera;

typedef struct DbCursorView {
    int mode;
    int flag_04;
    int actor_index;
    int step;
    HomogeneousVector position;
    unsigned char unmodeled_20[0x10];
    int attach_state;
    int flag_34;
    unsigned char unmodeled_38[0x0c];
    float offset_44;
    unsigned char unmodeled_48[8];
    CursorCallback callback;
    void *callback_argument;
    unsigned char unmodeled_58[8];
} DbCursorView;
#define DB_CAMERA_TO_WORLD(camera) ((float *)((unsigned char *)(camera) + 0x170))
extern float cursorSpeed[];
extern const float D_004D7CD0;
extern const float D_004D7CD4;
extern const char D_004C20B8[];
extern const char D_004C20C8[];
extern float UnduGet(float x, float z);

void updateCursorModePlane(void)
{
    unsigned short *keys;
    Vector4 stick;
    Vector4 direction;
    MatQ rotation;
    StudioCamera *active_camera;
    StudioCamera *camera;
    float length;
    float zero;
    float height;
    const char *height_format;
    const char *missing_format;
    HomogeneousVector *position;

    keys = (unsigned short *)&keyData;
    xglStudioGetCamera(&camera, 0);
    position = &cursor[1];
    stick.x = (float)((signed char *)keys)[8];
    zero = 0.0f;
    stick.z = (float)((signed char *)keys)[9];
    stick.w = 1.0f;
    length = zero;
    stick.y = zero;
    xglVectorLength(&length, &stick);
    if (length > 40.0f) {
        /* Copy the origin-relative camera-to-world transform through one scratch
         * register; remove translation below to transform a movement direction. */
        __asm__ __volatile__(
            "lq $2, 0(%1)\n"
            "sq $2, 0(%0)\n"
            "lq $2, 16(%1)\n"
            "sq $2, 16(%0)\n"
            "lq $2, 32(%1)\n"
            "sq $2, 32(%0)\n"
            "lq $2, 48(%1)\n"
            "sq $2, 48(%0)\n"
            :
            : "r"(&rotation), "r"(DB_CAMERA_TO_WORLD(camera))
            : "$2", "memory");
        rotation.m[3][0] = zero;
        rotation.m[3][1] = zero;
        rotation.m[3][2] = zero;
        xglVectorMulMat(&direction, rotation.m, &stick);
        xglVectorNormal(&stick, &stick);
        xglVectorLength(&length, &stick);
        length *= cursorSpeed[((DbCursorView *)cursor)->step];
        xglVectorNormal(&direction, &direction);
        switch (((DbCursorView *)cursor)->flag_04) {
        case 0:
            position->x += direction.x * length;
            position->y += direction.y * length;
            position->z += direction.z * length;
            active_camera = camera;
            active_camera->position.x += direction.x * length;
            active_camera->position.y += direction.y * length;
            active_camera->position.z += direction.z * length;
            break;
        case 1:
            position->x += direction.x * length;
            position->y += direction.y * length;
            position->z += direction.z * length;
            break;
        case 2:
            if (((DbCursorView *)cursor)->mode == 0) {
                active_camera = camera;
                active_camera->position.x += direction.x * length;
                active_camera->position.y += direction.y * length;
                active_camera->position.z += direction.z * length;
            } else {
                (&tcamera.x30)[((DbCursorView *)cursor)->mode - 1] += direction.y * length;
            }
            break;
        }
    }
    stick.x = (float)((signed char *)keys)[10];
    stick.y = (float)-((signed char *)keys)[11];
    stick.w = 1.0f;
    length = 0.0f;
    stick.z = 0.0f;
    xglVectorLength(&length, &stick);
    if (length > 40.0f) {
        /* Copy the origin-relative camera-to-world transform through one scratch
         * register; remove translation below to transform a movement direction. */
        __asm__ __volatile__(
            "lq $2, 0(%1)\n"
            "sq $2, 0(%0)\n"
            "lq $2, 16(%1)\n"
            "sq $2, 16(%0)\n"
            "lq $2, 32(%1)\n"
            "sq $2, 32(%0)\n"
            "lq $2, 48(%1)\n"
            "sq $2, 48(%0)\n"
            :
            : "r"(&rotation), "r"(DB_CAMERA_TO_WORLD(camera))
            : "$2", "memory");
        rotation.m[3][0] = 0.0f;
        rotation.m[3][1] = 0.0f;
        rotation.m[3][2] = 0.0f;
        xglVectorMulMat(&direction, rotation.m, &stick);
        xglVectorNormal(&stick, &stick);
        xglVectorLength(&length, &stick);
        length *= cursorSpeed[((DbCursorView *)cursor)->step];
        xglVectorNormal(&direction, &direction);
        switch (((DbCursorView *)cursor)->flag_04) {
        case 0:
            position->x += direction.x * length;
            position->y += direction.y * length;
            position->z += direction.z * length;
            break;
        case 1:
            position->x += direction.x * length;
            position->y += direction.y * length;
            position->z += direction.z * length;
            active_camera = camera;
            active_camera->position.x += direction.x * length;
            active_camera->position.y += direction.y * length;
            active_camera->position.z += direction.z * length;
            break;
        case 2:
            if (((DbCursorView *)cursor)->mode == 0) {
                active_camera = camera;
                active_camera->position.x += direction.x * length;
                active_camera->position.y += direction.y * length;
                active_camera->position.z += direction.z * length;
            } else {
                (&tcamera.x30)[((DbCursorView *)cursor)->mode - 1] += direction.y * length;
            }
            break;
        }
    }
    if (keys[0] & 0x2000) {
        camera->rotation.z -= D_004D7CD0;
    }
    if (keys[0] & 0x8000) {
        camera->rotation.z += D_004D7CD4;
    }
    length = cursorSpeed[((DbCursorView *)cursor)->step];
    if (keys[0] & 4) {
        position->y += length;
        camera->position.y += length;
    }
    if ((unsigned short)(keys[0] & 1)) {
        position->y -= length;
        camera->position.y -= length;
    }
    if (keys[0] & 0x40) {
        height = UnduGet(position->x, position->z);
        if (height != -1000.0f) {
            height_format = D_004C20B8;
            position->y = height;
            xglFontDebugPrintf(4, 0xC4, height_format, fptodp(height));
        } else {
            missing_format = D_004C20C8;
            xglFontDebugPrintf(4, 0xC4, missing_format, fptodp(height));
        }
        if (((DbCursorView *)cursor)->flag_34 != 0) {
            position->y -= ((DbCursorView *)cursor)->offset_44;
        }
    }
}

extern const float D_004D7CD8;
extern const float D_004D7CDC;

void updateCursorMode0(void)
{
    unsigned short *keys;
    Vector4 stick;
    Vector4 direction;
    MatQ rotation;
    StudioCamera *active_camera;
    StudioCamera *camera;
    float length;
    float zero;
    float height;
    const char *height_format;
    const char *missing_format;
    HomogeneousVector *position;

    keys = (unsigned short *)&keyData;
    xglStudioGetCamera(&camera, 0);
    position = &cursor[1];
    stick.x = (float)((signed char *)keys)[10];
    zero = 0.0f;
    stick.z = (float)((signed char *)keys)[11];
    stick.w = 1.0f;
    length = zero;
    stick.y = zero;
    xglVectorLength(&length, &stick);
    if (length > 40.0f) {
        /* Copy the origin-relative camera-to-world transform through one scratch
         * register; remove translation below to transform a movement direction. */
        __asm__ __volatile__(
            "lq $2, 0(%1)\n"
            "sq $2, 0(%0)\n"
            "lq $2, 16(%1)\n"
            "sq $2, 16(%0)\n"
            "lq $2, 32(%1)\n"
            "sq $2, 32(%0)\n"
            "lq $2, 48(%1)\n"
            "sq $2, 48(%0)\n"
            :
            : "r"(&rotation), "r"(DB_CAMERA_TO_WORLD(camera))
            : "$2", "memory");
        rotation.m[3][0] = zero;
        rotation.m[3][1] = zero;
        rotation.m[3][2] = zero;
        xglVectorMulMat(&direction, rotation.m, &stick);
        xglVectorNormal(&stick, &stick);
        xglVectorLength(&length, &stick);
        length *= cursorSpeed[((DbCursorView *)cursor)->step];
        xglVectorNormal(&direction, &direction);
        switch (((DbCursorView *)cursor)->flag_04) {
        case 0:
            position->x += direction.x * length;
            position->y += direction.y * length;
            position->z += direction.z * length;
            active_camera = camera;
            active_camera->position.x += direction.x * length;
            active_camera->position.y += direction.y * length;
            active_camera->position.z += direction.z * length;
            break;
        case 1:
            position->x += direction.x * length;
            position->y += direction.y * length;
            position->z += direction.z * length;
            break;
        case 2:
            if (((DbCursorView *)cursor)->mode == 0) {
                active_camera = camera;
                active_camera->position.x += direction.x * length;
                active_camera->position.y += direction.y * length;
                active_camera->position.z += direction.z * length;
            } else {
                (&tcamera.x30)[((DbCursorView *)cursor)->mode - 1] += direction.y * length;
            }
            break;
        }
    }
    stick.x = (float)((signed char *)keys)[8];
    stick.y = (float)-((signed char *)keys)[9];
    stick.w = 1.0f;
    length = 0.0f;
    stick.z = 0.0f;
    xglVectorLength(&length, &stick);
    if (length > 40.0f) {
        /* Copy the origin-relative camera-to-world transform through one scratch
         * register; remove translation below to transform a movement direction. */
        __asm__ __volatile__(
            "lq $2, 0(%1)\n"
            "sq $2, 0(%0)\n"
            "lq $2, 16(%1)\n"
            "sq $2, 16(%0)\n"
            "lq $2, 32(%1)\n"
            "sq $2, 32(%0)\n"
            "lq $2, 48(%1)\n"
            "sq $2, 48(%0)\n"
            :
            : "r"(&rotation), "r"(DB_CAMERA_TO_WORLD(camera))
            : "$2", "memory");
        rotation.m[3][0] = 0.0f;
        rotation.m[3][1] = 0.0f;
        rotation.m[3][2] = 0.0f;
        xglVectorMulMat(&direction, rotation.m, &stick);
        xglVectorNormal(&stick, &stick);
        xglVectorLength(&length, &stick);
        length *= cursorSpeed[((DbCursorView *)cursor)->step];
        xglVectorNormal(&direction, &direction);
        switch (((DbCursorView *)cursor)->flag_04) {
        case 0:
            position->x += direction.x * length;
            position->y += direction.y * length;
            position->z += direction.z * length;
            break;
        case 1:
            if (((DbCursorView *)cursor)->mode == 0) {
                position->x += direction.x * length;
                position->y += direction.y * length;
                position->z += direction.z * length;
                break;
            }
            position->x += direction.x * length;
            position->y += direction.y * length;
            position->z += direction.z * length;
            active_camera = camera;
            active_camera->position.x += direction.x * length;
            active_camera->position.y += direction.y * length;
            active_camera->position.z += direction.z * length;
            break;
        case 2:
            if (((DbCursorView *)cursor)->mode == 0) {
                active_camera = camera;
                active_camera->position.x += direction.x * length;
                active_camera->position.y += direction.y * length;
                active_camera->position.z += direction.z * length;
            } else {
                (&tcamera.x30)[((DbCursorView *)cursor)->mode - 1] += direction.y * length;
            }
            break;
        }
    }
    if (keys[0] & 0x2000) {
        camera->rotation.z -= D_004D7CD8;
    }
    if (keys[0] & 0x8000) {
        camera->rotation.z += D_004D7CDC;
    }
    length = cursorSpeed[((DbCursorView *)cursor)->step];
    if (keys[0] & 4) {
        position->y += length;
        camera->position.y += length;
    }
    if ((unsigned short)(keys[0] & 1)) {
        position->y -= length;
        camera->position.y -= length;
    }
    if (keys[0] & 0x40) {
        height = UnduGet(position->x, position->z);
        if (height != -1000.0f) {
            height_format = D_004C20B8;
            position->y = height;
            xglFontDebugPrintf(4, 0xC4, height_format, fptodp(height));
        } else {
            missing_format = D_004C20C8;
            xglFontDebugPrintf(4, 0xC4, missing_format, fptodp(height));
        }
        if (((DbCursorView *)cursor)->flag_34 != 0) {
            position->y -= ((DbCursorView *)cursor)->offset_44;
        }
    }
}

void changeCameraMode(void)
{
    Vector4 diff;
    StudioCamera *camera;
    float dist;
    int mode;
    unsigned char *cb;
    HomogeneousVector *p;
    HomogeneousVector *c;
    Vector4 *cam;

    c = cursor;
    xglStudioGetCamera(&camera, 0);
    cam = &camera->position;
    diff.x = c[1].x - cam->x;
    diff.y = c[1].y - cam->y;
    diff.z = c[1].z - cam->z;
    xglVectorLength(&dist, &diff);
    if (dist < 1.0f) {
        dist = 1.0f;
    }
    camera->rotation.z = 0.0f;
    mode = *(int *)&c->x;
    *(int *)&c->y = 0;
    if (mode != 4) {
        tcamera.mode10 = 0x14;
        tcamera.flag4E0 = 1;
        tcamera.target4E4 = &cursor[1];
        tcamera.y4D0 = 0.0f;
        tcamera.z4D0 = 0.0f;
        tcamera.x4D0 = 0.0f;
    }
    switch (mode) {
    case 0:
        p = cursor;
        *(int *)&p->y = 2;
        tcamera.modeC = 0x10;
        break;
    case 2:
        tcamera.modeC = 0x14;
        tcamera.flag40 = 1;
        tcamera.target44 = &cursor[1];
        tcamera.y30 = dist;
        tcamera.z30 = 0.0f;
        tcamera.x30 = 0.0f;
        break;
    case 3:
        tcamera.x30 = 0.0f;
        tcamera.y30 = 0.0f;
        tcamera.modeC = 0x14;
        tcamera.target44 = &cursor[1];
        tcamera.z30 = -dist;
        tcamera.flag40 = 1;
        break;
    case 1:
        tcamera.y30 = 0.0f;
        tcamera.z30 = 0.0f;
        tcamera.modeC = 0x14;
        tcamera.target44 = &cursor[1];
        tcamera.x30 = -dist;
        tcamera.flag40 = 1;
    case 4:
        if (*(int *)((cb = (unsigned char *)cursor) + CURSOR_CALLBACK_OFFSET) != 0) {
            tcamera.modeC = 0x10;
            tcamera.mode10 = 0x10;
        } else {
            *(int *)cb = 0;
            changeCameraMode();
        }
        break;
    }
}

extern int toolFlags;
extern int cursorMode;
extern int cursorModeName[];
extern const char D_004C2108[];
extern const char D_004C2128[];
extern const char D_004C2148[];
extern void PAD_getKey(unsigned short *keys, int port, int mask);

void updateCursor(int mode)
{
    unsigned short *keys;
    int plane;
    int selected;
    StudioCamera *camera;
    HomogeneousVector *position;
    Vector4 *camera_position;
    StudioCamera *reset_camera;
    char text[0x100];
    const char *position_format;
    const char *camera_format;

    keys = (unsigned short *)&keyData;
    switch (mode) {
    case 0:
        toolFlags = 0;
        PAD_getKey(keys, 0, 0xF7FF);
        break;
    case 1:
        toolFlags = 1;
        PAD_getKey(keys, 0, 0x600);
        break;
    case 2:
        toolFlags = 1;
        PAD_getKey(keys, 0, 0x400);
        break;
    case 3:
        toolFlags = 0;
        PAD_getKey(keys, 0, 0x57BF);
        break;
    }
    if ((keys[0] & 0xA0) != 0) {
        if (cursorMode < 0) {
            cursorMode = ((DbCursorView *)cursor)->mode;
        }
        if (keys[1] & 0x1000) {
            cursorMode += 1;
        }
        if (keys[1] & 0x4000) {
            cursorMode -= 1;
        }
        if (cursorMode < 0) {
            cursorMode = 4;
        }
        if (cursorMode >= 5) {
            cursorMode = 0;
        }
    } else {
        if (cursorMode >= 0) {
            ((DbCursorView *)cursor)->mode = cursorMode;
            changeCameraMode();
            cursorMode = -1;
        }
        if (keys[1] & 0x1000) {
            ((DbCursorView *)cursor)->step += 1;
        }
        if (keys[1] & 0x4000) {
            ((DbCursorView *)cursor)->step -= 1;
        }
        ((DbCursorView *)cursor)->step = ((DbCursorView *)cursor)->step & 7;
    }
    position = &cursor[1];
    xglStudioGetCamera(&camera, 0);
    if ((keys[1] & 0x200) != 0) {
        position->x = 0.0f;
        position->y = 0.0f;
        position->z = 0.0f;
    }
    if (keys[1] & 0x400) {
        reset_camera = camera;
        reset_camera->position.y = 0.0f;
        reset_camera->position.z = 4.0f;
        reset_camera->position.x = 0.0f;
    }
    if (keys[1] & 8) {
        ((DbCursorView *)cursor)->flag_04 -= 1;
    }
    if (keys[1] & 2) {
        ((DbCursorView *)cursor)->flag_04 += 1;
    }
    plane = ((DbCursorView *)cursor)->flag_04;
    if (plane < 0) {
        ((DbCursorView *)cursor)->flag_04 = 2;
        plane = 2;
    }
    if (!(plane < 3)) {
        ((DbCursorView *)cursor)->flag_04 = 0;
    }
    if (((DbCursorView *)cursor)->mode != 4) {
        switch (((DbCursorView *)cursor)->mode) {
        case 0:
        case 1:
        case 3:
            updateCursorMode0();
            break;
        case 2:
            updateCursorModePlane();
            break;
        }
        updateCursorMode2();
        if (toolFlags == 0) {
            camera_position = &camera->position;
            position_format = D_004C2108;
            sprintf(text, position_format, fptodp(position->x), fptodp(position->y),
                    fptodp(position->z));
            xglFontDebugPrintf(0xAA, 0xAA, text);
            camera_format = D_004C2128;
            sprintf(text, camera_format, fptodp(camera_position->x), fptodp(camera_position->y),
                    fptodp(camera_position->z));
            xglFontDebugPrintf(0xAA, 0xC6, text);
        }
    } else {
        if (((DbCursorView *)cursor)->callback != 0) {
            ((DbCursorView *)cursor)->callback(((DbCursorView *)cursor)->callback_argument);
        }
    }
    selected = cursorMode;
    if (selected < 0) {
        selected = ((DbCursorView *)cursor)->mode;
    }
    if (toolFlags == 0) {
        xglFontDebugPrintf(4, 0xB4, D_004C2148, cursorModeName[selected], ((DbCursorView *)cursor)->flag_04,
                           ((DbCursorView *)cursor)->step + 1);
    }
}

void drawCursor(void)
{
    Matrix4 matrix;
    HomogeneousVector *position;

    position = &cursor[1];
    position->w = 1.0f;
    xglMatrixStackUnit();
    xglMatrixStackTrans(&position->x);
    xglMatrixStackSave(matrix);
    drawAxis(matrix, 1.0f);
}

extern int cursorMode;
static inline void initCursorSet(int *c, HomogeneousVector *pos, int m10, int flag, int c3, int mode, int c1)
{
    tcamera.flag4E0 = flag;
    c[1] = c1;
    tcamera.mode10 = m10;
    c[3] = c3;
    cursorMode = mode;
    tcamera.target4E4 = pos;
    tcamera.y4D0 = 0.0f;
    tcamera.z4D0 = 0.0f;
    tcamera.x4D0 = 0.0f;
    c[5] = 0;
    c[6] = 0;
    c[0] = 0;
    c[4] = 0;
}

unsigned int initCursor(void)
{
  int camera_index;
  StudioCamera *cam;
  HomogeneousVector *pos;
  int *c;
  camera_index = 0;
  do
  {
    StudioCamera *camera;
    if (!cam)
    {
    }
    xglStudioGetCamera(&camera, camera_index);
    cam = camera;
    if (camera_index == 0)
    {
      cam->active = 1;
      cam->state = 1;
    }
    else
    {
      cam->active = 0;
    }
    camera_index++;
  }
  while (camera_index < 8);
  pos = &cursor[1];
  c = ((int *) pos) - 4;
  initCursorSet(c, pos, 0x14, 1, 3, -1, 2);
  changeCameraMode();
}

static void printM(int x, int y, const float *matrix)
{
    char buffer[256];
    const float *rowEnd;
    int rowIndex;

    rowEnd = matrix + 3;
    rowIndex = 0;
    do {
        rowIndex++;
        sprintf(buffer, D_004C2158,
                rowEnd[-3], rowEnd[-2], rowEnd[-1], rowEnd[0]);
        xglFontDebugPrintf(x, y, buffer);
        if (rowIndex > 3)
            break;
        rowEnd += 4;
        y += 8;
    } while (1);
}

void MAP_serach(void)
{
    MapStageModelView *model;
    MapPartListView *parts;
    DbLightWritePadSecondPrefix *mapPad;
    unsigned char *matrix_base;
    void *matrix_data;
    char buffer[256];

    model = GameLoopState.model;
    mapPad = &PadData.second;
    visible_0++;
    if (model != 0) {
        parts = model->parts;
        if (parts != 0) {
            matrix_base = (unsigned char *)parts +
                          parts->matrix_data_offset;
            if (mapPad->half_2c & 0x4000) {
                nmlModelSetPartsVisible(parts, partsIndex, 1);
                visible_0 = 0;
                partsIndex--;
            }

            if (mapPad->half_2c & 0x1000) {
                nmlModelSetPartsVisible(model->parts,
                                        partsIndex, 1);
                visible_0 = 0;
                partsIndex++;
            }

            if (partsIndex < 0)
                partsIndex = 0;
            matrix_data = matrix_base + partsIndex * sizeof(Matrix4);
            printM(0x20, 0x40, matrix_data);
            sprintf(buffer, D_004DA520, partsIndex);
            xglFontDebugPrintf(8, 0x20, buffer);
            nmlModelSetPartsVisible(model->parts, partsIndex,
                                    (visible_0 >> 4) & 1);
        }
    }
}

void EvtTools(void)
{
    if ((PadData.half_2a & 0x10) && (PadData.half_28 & 0x100)) {
        mode_004DC5A8 = (mode_004DC5A8 + 1) & 1;
    }
    MAP_serach();
    if (mode_004DC5A8 == 1) {
        JTHREAD_cntl();
        PLAY_ctrl();
    }
    TCAMERA_update();
    if (mode_004DC5A8 == 1) {
        ACT_update();
        MAP_updateUnit();
    }
    updateCursor(0);
    drawCursor();
}

typedef struct {
    unsigned char pad_00[0x20];
    unsigned int color_r;
    unsigned int color_g;
    unsigned int color_b;
} ClearEnvColorView;

extern ClearEnvColorView ClearEnv;

void initLight3(void)
{
    StudioLight *light;
    StudioLight *l;
    StudioLight *source;
    Vector4 *slot;
    Vector4 *colour;
    Vector4 *direction;
    float sum;
    int i;
    int filled;

    xglStudioGetLight(&light);
    l = light;
    slot = &lightCol[3];
    slot->x = l->ambient_color.x;
    slot->y = l->ambient_color.y;
    slot->z = l->ambient_color.z;
    sum = slot->x + slot->y + slot->z;
    slot->w = sum / 3.0f;
    colour = lightCol;
    i = 0;
    do {
        filled = i + 1;
        source = l;
        colour->x = source->lights[0].color.x;
        colour->y = source->lights[0].color.y;
        colour->z = source->lights[0].color.z;
        colour->w = (colour->x + colour->y + colour->z) / 3.0f;
        colour++;
        i = filled;
    } while (filled < 3);
    slot = &lightCol[4];
    slot->x = (float)ClearEnv.color_r / 255.0f;
    slot->y = (float)ClearEnv.color_g / 255.0f;
    slot->z = (float)ClearEnv.color_b / 255.0f;
    slot->w = (slot->x + slot->y + slot->z) / 3.0f;
    slot = &lightCol[5];
    slot->w = (slot->x + slot->y + slot->z) / 3.0f;
    source = l;
    i = 0;
    do {
        direction = &lightDir[i];
        direction->x = source->lights[0].direction.x;
        direction->y = source->lights[0].direction.y;
        if (direction->y >= 0.0f) {
            direction->y = 1.0f;
        } else {
            direction->y = -1.0f;
        }
        l = source;
        direction->z = source->lights[0].direction.z;
    } while (++i < 3);
}

void initLight2(void)
{
    StudioLight *light;
    StudioLight *lp;
    StudioLightEntry *e;
    float *src;
    Vector4 *dst;
    Vector4 *ld;
    int i;
    float x;
    float y;

    xglStudioGetLight(&light);
    lp = light;

    dst = &lightCol[3];
    dst->x = lp->ambient_color.x;
    dst->y = lp->ambient_color.y;
    dst->z = lp->ambient_color.z;
    dst->w = (dst->x + dst->y + dst->z) / 3.0f;

    e = (StudioLightEntry *)((char *)lp->lights + 8);
    for (i = 0; i < 3; i++) {
        float lx = ((float *)(e + i))[-2];
        float ly;
        float lz;

        dst = lightCol + i;
        dst->x = lx;
        ly = ((float *)(e + i))[-1];
        dst->y = ly;
        lz = ((float *)(e + i))[0];
        dst->z = lz;
        dst->w = (lx + ly + lz) / 3.0f;
    }

    dst = &lightCol[4];
    dst->x = (float)ClearEnv.color_r / 255.0f;
    dst->y = (float)ClearEnv.color_g / 255.0f;
    dst->z = (float)ClearEnv.color_b / 255.0f;
    dst->w = (dst->x + dst->y + dst->z) / 3.0f;
    dst = &lightCol[5];
    dst->w = (dst->x + dst->y + dst->z) / 3.0f;

    src = &lp->lights[0].direction.x + 2;
    ld = lightDir;
    i = 2;
    do {
        x = src[-2];
        ld->x = x;
        y = src[-1];
        ld->y = y;
        if (y >= 0.0f) {
            ld->y = 1.0f;
        } else {
            ld->y = -1.0f;
        }
        ld->z = src[0];
        src += 8;
        ld++;
        i--;
    } while (i >= 0);
}

void initLight(void)
{
    StudioLight *light;
    Vector4 direction;
    int lightIndex;

    xglStudioGetLight(&light);
    lightIndex = 0;
    do {
        Vector4 *source = &lightDir[lightIndex];

        if (__builtin_sqrtf((source->x * source->x) +
                            (source->y * source->y)) > 1.0f) {
            float angle = atan2f(source->x, source->y);
            source->x = xglSin(angle);
            source->y = xglCos(angle);
        }

        direction.x = source->x;
        direction.y = source->y;
        direction.z = __builtin_sqrtf(
            1.0f - ((direction.x * direction.x) +
                    (direction.y * direction.y)));
        if (source->z < 0.0f)
            direction.z = -direction.z;

        xglLightIntensityParallel(light, lightIndex, &lightCol[lightIndex]);
        xglLightDirection(light, lightIndex, &direction);
    } while (++lightIndex < 3);

    xglLightIntensityAmbient(light, &lightCol[3]);
}

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateLight);

typedef struct WindRecord {
    signed char mode;
    unsigned char unmodeled_01[0x0F];
    float x;
    float y;
    float z;
    float w;
    float offset_a;
    float offset_b;
} WindRecord;
typedef union WindPoint {
    Vector4 vector;
    unsigned long long raw[2];
} WindPoint;
typedef struct WindMarker {
    WindPoint from;
    Vector4 to;
    int color[4];
} WindMarker;
extern void EXM_ResetWind(void);
extern int dispy_2;
extern int csr_1;
extern const char D_004DA558[];
extern const char D_004DA560[];
extern const char D_004DA568[];
extern const char D_004DA570[];
extern const char D_004DA578[];
extern const char D_004DA580[];
extern const char D_004DA588[];
extern const char D_004C21C8[];
extern const char D_004C21D8[];
extern const char D_004C21F0[];
extern const char D_004C2200[];
extern const char D_004C2210[];
extern const char D_004C2220[];
extern const char D_004C2230[];
extern const char D_004C2240[];
extern const char D_004C2250[];
extern const char D_004C2260[];
extern const char D_004C2270[];
extern const char D_004C2290[];
extern const char D_004C22C8[];
extern const char D_004C22F8[];

void updateWind(WindRecord *wind)
{
    StudioCamera *camera;
    DbLightWritePadSecondPrefix *pad;
    char text[256];
    char text_line[256];
    WindMarker marker;
    char text_wide[128];
    char *wide_text;
    unsigned short buttons;
    int stick;
    int selection;
    int mode;
    float step;
    float angle;
    float cos_angle;
    float sin_angle;
    float stick_x;
    float stick_y;
    const char *format1, *format2, *format3, *format4, *format5, *format6, *format7, *format8, *format9, *format10;
    char *line;

    pad = &PadData.second;
    camera = xglStudioGetActiveCamera();
    buttons = pad->half_2c;
    if (buttons & 0x1000) {
        selection = csr_1;
        selection += 4;
        selection %= 5;
        csr_1 = selection;
    }
    if (buttons & 0x4000) {
        selection = csr_1;
        selection += 1;
        selection %= 5;
        csr_1 = selection;
    }
    selection = csr_1;
    if ((pad->half_2a & 0x20) && selection < 3) {
        wind->mode = csr_1;
        selection = csr_1;
    }
    stick = pad->half_28;
    buttons = stick;
    step = (buttons & 0x80) ? 0.001f : 0.01f;

    if (selection == 3) {
        if (buttons & 0x8000) {
            wind->offset_a -= step;
        }
        if (buttons & 0x2000) {
            wind->offset_a += step;
        }
    }
    if (selection == 4) {
        float offset_clamp;

        if (buttons & 0x8000) {
            wind->offset_b -= step;
        }
        if (buttons & 0x2000) {
            wind->offset_b += step;
        }
        offset_clamp = wind->offset_b;
        if (offset_clamp > (3.14159265f * 2.0f)) {
            wind->offset_b = (3.14159265f * 2.0f);
            offset_clamp = (3.14159265f * 2.0f);
        }
        if (offset_clamp < (-3.14159265f * 2.0f)) {
            wind->offset_b = (-3.14159265f * 2.0f);
        }
    }
    if (buttons & 0x4) {
        wind->w = wind->w + step * 0.1f;
    }
    if (buttons & 0x1) {
        wind->w -= step * 0.1f;
    }
    if (wind->mode == 1) {
        step *= 0.1f;
    }
    if (buttons & 0x8) {
        wind->y += step * 5.0f;
    }
    if (buttons & 0x2) {
        wind->y -= step * 5.0f;
    }

    angle = ((Vector4 *)camera->unmodeled_b0)->y;
    cos_angle = xglCos(angle);
    sin_angle = xglSin(angle);

    stick_x = (float)pad->stick_x * step * 0.1f;
    stick_y = (float)pad->stick_y * step * 0.1f;
    wind->x = wind->x + (cos_angle * stick_x - sin_angle * stick_y);
    wind->z = wind->z + (cos_angle * stick_y + sin_angle * stick_x);

    if (pad->half_2a & 0x10) {
        mode = wind->mode;
        EXM_ResetWind();
        wind->mode = mode;
        if (mode == 1) {
            wind->w = 1.0f;
        }
    }

    xglFontDebugPrintf(0, 0, D_004DA558);
    xglFontDebugPrintf(150, dispy_2 - 12, D_004DA560);
    mode = wind->mode;
    if (mode == 0) {
        xglFontDebugPrintf(162, dispy_2, D_004C21C8);
    } else {
        xglFontDebugPrintf(162, dispy_2, D_004DA568);
    }
    if (mode == 1) {
        xglFontDebugPrintf(162, dispy_2 + 12, D_004C21D8);
    } else {
        xglFontDebugPrintf(162, dispy_2 + 12, D_004C21F0);
    }
    if (mode == 2) {
        xglFontDebugPrintf(162, dispy_2 + 24, D_004C2200);
    } else {
        xglFontDebugPrintf(162, dispy_2 + 24, D_004DA570);
    }
    xglFontDebugPrintf(150, dispy_2 + 36, D_004DA578);
    format1 = D_004C2210;
    sprintf(text, format1, fptodp(wind->offset_a));
    xglFontDebugPrintf(162, dispy_2 + 48, text);
    format2 = D_004C2220;
    sprintf(text, format2, fptodp(wind->offset_b));
    xglFontDebugPrintf(162, dispy_2 + 60, text);
    xglFontDebugPrintf(170, dispy_2 + 72, D_004DA580);
    format3 = D_004C2230;
    line = text_line;
    sprintf(line, format3, fptodp(wind->x));
    xglFontDebugPrintf(182, dispy_2 + 80, line);
    format4 = D_004C2240;
    sprintf(line, format4, fptodp(wind->y));
    xglFontDebugPrintf(182, dispy_2 + 88, line);
    format5 = D_004C2250;
    sprintf(line, format5, fptodp(wind->z));
    xglFontDebugPrintf(182, dispy_2 + 96, line);
    format6 = D_004C2260;
    sprintf(line, format6, fptodp(wind->w));
    xglFontDebugPrintf(182, dispy_2 + 104, line);
    if (wind->mode == 1) {
        marker.from.raw[0] = *(unsigned long long *)((char *)camera + 288);
        marker.from.raw[1] = *(unsigned long long *)((char *)camera + 296);
        marker.color[0] = 255;
        marker.color[1] = 60;
        marker.color[2] = 60;
        marker.color[3] = 255;
        marker.to.x = marker.from.vector.x + wind->x;
        marker.to.y = marker.from.vector.y + wind->y;
        marker.to.z = marker.from.vector.z + wind->z;
        marker.from.vector.w = 0.05f;
    }
    if (csr_1 < 3) {
        xglFontDebugPrintf(150, dispy_2 + csr_1 * 12, D_004DA588);
    } else {
        xglFontDebugPrintf(150, dispy_2 + csr_1 * 12 + 12, D_004DA588);
    }
    if (pad->half_2a & 0x100) {
        wide_text = text_wide;
        format7 = D_004C2270;
        sprintf(wide_text, format7, fptodp(wind->offset_a), fptodp(wind->offset_b));
        switch (wind->mode) {
        case 1:
            format8 = D_004C2290;
            sprintf(wide_text, format8, fptodp(wind->x), fptodp(wind->y), fptodp(wind->z), fptodp(wind->w));
            break;
        case 2:
            format9 = D_004C22C8;
            sprintf(wide_text, format9, fptodp(wind->x), fptodp(wind->y), fptodp(wind->z), fptodp(wind->w));
            break;
        case 0:
            format10 = D_004C22F8;
            sprintf(wide_text, format10);
            break;
        }
    }
}

void VW_setCursorMode(int mode)
{
    CursorState *state = (void *)cursor;

    state->mode = mode;
    changeCameraMode();
}

void VW_setCursorFunc(CursorCallback callback, void *argument)
{
    CursorState *state = (void *)cursor;

    state->callback = callback;
    state->callback_argument = argument;
}

void VW_setCursor(const Vector4 *position)
{
    cursor[1].x = position->x;
    cursor[1].y = position->y;
    cursor[1].z = position->z;
}

void VW_getCursor(HomogeneousVector *destination)
{
    destination->x = cursor[1].x;
    destination->y = cursor[1].y;
    destination->z = cursor[1].z;
    destination->w = 1.0f;
}

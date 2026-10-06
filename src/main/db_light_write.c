#include "common.h"
#include "shared.h"
#include "db_light_write.h"

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", DB_lightWrite);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawRect2);

typedef struct TransformParams {
    Matrix4 matrix;
    Matrix4 world;
    Vector4 screenScale;
    Vector4 screenOffset;
    Matrix4 *local;
} TransformParams;
extern void xglMatrixStackUnit(void);
extern void xglMatrixStackSave(float matrix[4][4]);
extern void transformVector(Vector4 *destination, const Vector4 *vectors, int count,
                            TransformParams *params);
extern void xglPrimAddGifTagDirect(void *packet, const void *data, int count);
typedef struct { unsigned char r, g, b, a; } DbColor;
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

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawCircle2);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawVector2);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawVector4S);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawGrid);

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

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawAxis);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawCircle3);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawTags);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", drawVector4);

/*
 * updateCursorMode2 supplies the cursor and an actor position here.  A
 * candidate is eligible only when its xyz distance is below radius; the
 * distance is returned for nearest-candidate selection, otherwise -1.0f.
 */
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

void *nextActor(int startIndex)
{
    typeof(&actor[0]) p;
    if (startIndex < 64) {
        startIndex *= sizeof(*p);
        p = (typeof(p))((char *)actor + startIndex);
        do {
            if (p->inUseId != 0 && !(p->flags & 8)) {
                return p;
            }
            p++;
        } while ((int)p < (int)&actor[64]);
    }
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateCursorMode1);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateCursorMode2);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateCursorModePlane);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateCursorMode0);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", changeCameraMode);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateCursor);

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

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", initCursor);

extern const char D_004C2158[];
extern double fptodp(float value);
extern int sprintf(char *str, const char *format, ...);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

static void printM(int px, int py, float *m)
{
    char buf[0x100];
    float *p;
    int yy;
    int i;
    double dx, dy, dz, dw;

    p = m + 3;
    yy = py;
    i = 0;
loop:
    i++;
    dx = fptodp(p[-3]);
    dy = fptodp(p[-2]);
    dz = fptodp(p[-1]);
    dw = fptodp(p[0]);
    p += 4;
    sprintf(buf, D_004C2158, dx, dy, dz, dw);
    xglFontDebugPrintf(px, yy, buf);
    yy += 8;
    if (i < 4)
        goto loop;
}

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", MAP_serach);

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

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", initLight3);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", initLight2);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", initLight);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateLight);

INCLUDE_ASM("asm/main/nonmatchings/db_light_write", updateWind);

void VW_setCursorMode(int mode)
{
    *(int *)((unsigned char *)cursor + CURSOR_MODE_OFFSET) = mode;
    changeCameraMode();
}

void VW_setCursorFunc(CursorCallback callback, void *argument)
{
    unsigned char *cursorBytes = (unsigned char *)cursor;
    *(CursorCallback *)(cursorBytes + CURSOR_CALLBACK_OFFSET) = callback;
    *(void **)(cursorBytes + CURSOR_CALLBACK_ARGUMENT_OFFSET) = argument;
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

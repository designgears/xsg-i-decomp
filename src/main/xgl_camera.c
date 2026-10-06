#include "common.h"
#include "shared.h"

static void xglCameraControlInit(StudioCamera *camera)
{
    camera->state = camera->active = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/xgl_camera", xglCameraScreenInit);

INCLUDE_ASM("asm/main/nonmatchings/xgl_camera", xglCameraTravelInit);

extern float sShootAngleA_10[4];
extern float sShootPlaceA_11[4];

static void xglCameraTravelReset(StudioCamera *camera)
{
    *(int *)((char *)camera + 0xC) = 0;
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)"
        :
        : "r"((char *)camera + 0x30), "r"(sShootAngleA_10)
        : "$2", "memory");
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)"
        :
        : "r"((char *)camera + 0x60), "r"(sShootPlaceA_11)
        : "$2", "memory");
}

typedef struct CameraPadView {
    u8 unmodeled_00[0x90];
    union { u16 v; } trigger;
    union { u16 v; } hold;
    u8 unmodeled_94[0x3C];
} CameraPadView;
extern CameraPadView PadData;

static void xglCameraTravelFocus(float *focus)
{
    if ((PadData.hold.v & 0x20) && focus[0] < 9.0f) {
        focus[0] += 1.0f;
    }
    if ((PadData.hold.v & 0x80) && focus[0] > 1.0f) {
        focus[0] -= 1.0f;
    }
    if (PadData.trigger.v & 0x8) {
        focus[3] = 0.0011170107f;
    }
    if (PadData.trigger.v & 0x2) {
        focus[3] = -0.0011170107f;
    }
}

extern float D_004D8868;
extern float D_004D886C;
void xglVectorScaleXYZ(Vector4 *destination, const Vector4 *source, float scale);

static void xglCameraTravelScale(float *focus)
{
	float scale;

	if (PadData.trigger.v & 0x40) {
		focus[3] *= 4.0f;
		xglVectorScaleXYZ((Vector4 *)((char *)focus + 0x30), (Vector4 *)((char *)focus + 0x30), 4.0f);
		xglVectorScaleXYZ((Vector4 *)((char *)focus + 0x60), (Vector4 *)((char *)focus + 0x60), 4.0f);
	}

	scale = (focus[0] - 5.0f) * D_004D8868 + 1.0f;
	focus[3] *= scale;
	xglVectorScaleXYZ((Vector4 *)((char *)focus + 0x30), (Vector4 *)((char *)focus + 0x30), scale);
	xglVectorScaleXYZ((Vector4 *)((char *)focus + 0x60), (Vector4 *)((char *)focus + 0x60), (focus[0] - 5.0f) * D_004D886C + 1.0f);
}

INCLUDE_ASM("asm/main/nonmatchings/xgl_camera", xglCameraTravelManual);

INCLUDE_ASM("asm/main/nonmatchings/xgl_camera", xglCameraTravelChase);

INCLUDE_ASM("asm/main/nonmatchings/xgl_camera", xglCameraTravelProc);

INCLUDE_ASM("asm/main/nonmatchings/xgl_camera", xglCameraViewScreen);

INCLUDE_ASM("asm/main/nonmatchings/xgl_camera", xglCameraViewTravel);

INCLUDE_ASM("asm/main/nonmatchings/xgl_camera", xglCameraViewVolume);

static void xglCameraScreenInit(StudioCamera *camera);
static void xglCameraTravelInit(StudioCamera *camera);

void xglCameraInit(StudioCamera *camera)
{
    xglCameraControlInit(camera);
    xglCameraScreenInit(camera);
    xglCameraTravelInit(camera);
}

/* Move passes an unused second register argument; the callee reads only a0. */
static void xglCameraViewScreen();
static void xglCameraViewTravel(StudioCamera *camera, Vector4 *offset);
static void xglCameraViewVolume(StudioCamera *camera, Vector4 *offset);

void xglCameraMove(StudioCamera *camera)
{
    Vector4 offset;

    memset(&offset, 0, sizeof(offset));
    xglCameraViewScreen(camera, &offset);
    xglCameraViewTravel(camera, &offset);
    xglCameraViewVolume(camera, &offset);
}

static void xglCameraViewScreen(StudioCamera *camera);
static void xglCameraViewTravel(StudioCamera *camera, Vector4 *offset);
static void xglCameraViewVolume(StudioCamera *camera, Vector4 *offset);

void xglCameraMoveOffset(StudioCamera *camera, Vector4 *offset)
{
    xglCameraViewScreen(camera);
    xglCameraViewTravel(camera, offset);
    xglCameraViewVolume(camera, offset);
}

/* The screen subobject begins at +0x10; its viewport occupies +0x60..+0x6f. */
typedef struct CameraScreen {
    u8 unmodeled_00[0x50];
    Vector4 window;
} CameraScreen;

typedef struct CameraWindowData {
    u8 unmodeled_00[0x10];
    CameraScreen screen;
} CameraWindowData;

/* sRender is 0x5c bytes; its signed screen dimensions are at +4 and +6. */
typedef struct RenderWindowLimits {
    u8 unmodeled_00[4];
    short width;
    short height;
    u8 unmodeled_08[0x54];
} RenderWindowLimits;

extern RenderWindowLimits sRender;

void xglCameraSetWindow(CameraWindowData *camera, int left, int top, int right, int bottom)
{
    int swapCoordinate;
    CameraScreen *screen;

    if (camera == 0) {
        return;
    }
    screen = &camera->screen;
    if (right < left) {
        swapCoordinate = right;
        right = left;
        left = swapCoordinate;
    }
    if (bottom < top) {
        swapCoordinate = bottom;
        bottom = top;
        top = swapCoordinate;
    }
    if (left < 0) left = 0;
    if (right < 0) right = 0;
    if (top < 0) top = 0;
    if (bottom < 0) bottom = 0;
    if (left >= sRender.width) left = sRender.width - 1;
    if (top >= sRender.height) top = sRender.height - 1;
    if (right >= sRender.width) right = sRender.width - 1;
    if (bottom >= sRender.height) bottom = sRender.height - 1;
    screen->window.x = (float)left;
    screen->window.y = (float)top;
    screen->window.z = (float)right;
    screen->window.w = (float)bottom;
}

void xglCameraClipRangeDefault(StudioCamera *camera)
{
    camera->nearClip = 0.01f;
    camera->farClip = 99000.0f;
}

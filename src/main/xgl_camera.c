#include "common.h"
#include "shared.h"

static void xglCameraControlInit(StudioCamera *camera)
{
    camera->state = camera->active = 0;
}

/* sRender is 0x5c bytes; its signed screen dimensions are at +4 and +6. */
typedef struct RenderWindowLimits {
    u8 unmodeled_00[4];
    short width;
    short height;
    u8 unmodeled_08[0x54];
} RenderWindowLimits;

extern RenderWindowLimits sRender;

extern float sClipRange_0[4];
extern float sPortEffect_1[4];
typedef union { float f; int i; } CamFloatU;

static void xglCameraScreenInit(StudioCamera *camera)
{
    unsigned char *s = (unsigned char *)camera + 0x10;

    *(int *)(s + 0x0) = 0xFFFFFF;
    *(int *)(s + 0x4) = 0;
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)"
        :
        : "r"(s + 0x10), "r"(sClipRange_0)
        : "$2", "memory");
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)"
        :
        : "r"(s + 0x20), "r"(sPortEffect_1)
        : "$2", "memory");
    ((CamFloatU *)(s + 0x30))->f = sRender.width;
    ((CamFloatU *)(s + 0x34))->f = sRender.height;
    ((CamFloatU *)(s + 0x38))->f = sRender.width;
    ((CamFloatU *)(s + 0x3C))->f = sRender.height;
    ((CamFloatU *)(s + 0x50))->f = 0.0f;
    ((CamFloatU *)(s + 0x54))->f = 0.0f;
    ((CamFloatU *)(s + 0x58))->f = sRender.width;
    ((CamFloatU *)(s + 0x5C))->f = sRender.height;
}

typedef struct XglCamera XglCamera;
typedef struct XglCameraScreen XglCameraScreen;
typedef union CameraVector {
    Vector4 vector;
    float element[4];
} CameraVector;
typedef struct CameraTravel {
    float focus;
    float fov;
    float fov_velocity;
    float scale;
    Vector4 rotation;
    Vector4 angle_velocity;
    Vector4 angle_delta;
    Vector4 position;
    Vector4 place_velocity;
    Vector4 place_delta;
    float chase_yaw;
    float chase_distance;
    unsigned char unmodeled_78[8];
    Vector4 chase_reference;
    Vector4 chase_position;
    Vector4 frustum[4];
} CameraTravel;
struct XglCameraScreen {
    unsigned int depthMax;
    unsigned int depthMin;
    unsigned char unmodeled_08[8];
    Vector4 clipRange;
    Vector4 portEffect;
    Vector4 resolution;
    Vector4 projection;
    Vector4 window;
    Vector4 screenOffset;
    Vector4 screenScale;
};
typedef struct CameraVolume {
    CameraVector vertex[16];
    CameraVector plane[12];
    Matrix4 frustum;
    Matrix4 viewMatrix;
    unsigned char unmodeled_240[0x40];
    float planeSet[4][4][4];
} CameraVolume;
struct XglCamera {
    unsigned int active;
    unsigned int state;
    unsigned char unmodeled_08[8];
    XglCameraScreen screen;
    CameraTravel travel;
    Matrix4 transform;
    Matrix4 inverseTransform;
    unsigned char unmodeled_1f0[0x80];
    CameraVolume volume[1];
};

typedef struct CameraDisplayEnv {
    u8 unmodeled_00[0x18];
    long display;
    u8 unmodeled_20[0x30];
} CameraDisplayEnv;
extern CameraDisplayEnv DispEnv;
extern signed char aaiVertexIndex[6][3];
extern Vector4 sShootAngleR_2;
extern Vector4 sShootAngleV_3;
extern Vector4 sShootAngleA_4;
extern Vector4 sShootPlaceR_5;
extern Vector4 sShootPlaceV_6;
extern Vector4 sShootPlaceA_7;
extern Vector4 sChaseRefPos_8;
extern Vector4 sChaseIntPos_9;
extern void xglVectorNormal(Vector4 *destination, const Vector4 *source);
extern float atan2f(float y, float x);
extern float tanf(float angle);
extern void xglMatrixStackInverse(void);
extern void xglMatrixStackMul(Matrix4 matrix);
extern void xglMatrixStackFrustum(float left, float right, float bottom,
                                  float top, float near, float far);
extern void xglVectorMulMat(Vector4 *destination, Matrix4 matrix,
                            const Vector4 *vector);
extern void xglPlaneParameter(Vector4 *destination, const Vector4 *p0,
                              const Vector4 *p1, const Vector4 *p2);
extern void xglVectorScale(Vector4 *destination, const Vector4 *source,
                           float scale);
extern float xglSin(float angle);
extern float xglCos(float angle);
extern void xglVectorScaleAddXYZ(Vector4 *destination, const Vector4 *source,
                                 const Vector4 *other, float scale);

static void xglCameraTravelInit(StudioCamera *studio)
{
    XglCamera *camera = (XglCamera *)studio;
    CameraTravel *travel = &camera->travel;

    travel->focus = 5.0f;
    travel->fov = 0.69813174f;
    travel->fov_velocity = 0.0f;
    travel->scale = 0.0f;
    __asm__ __volatile__("lq $2, 0(%1)\n\t"
                         "sq $2, 0(%0)"
                         : : "r"(&travel->rotation), "r"(&sShootAngleR_2)
                         : "$2", "memory");
    __asm__ __volatile__("lq $2, 0(%1)\n\t"
                         "sq $2, 0(%0)"
                         : : "r"(&travel->angle_velocity), "r"(&sShootAngleV_3)
                         : "$2", "memory");
    __asm__ __volatile__("lq $2, 0(%1)\n\t"
                         "sq $2, 0(%0)"
                         : : "r"(&travel->angle_delta), "r"(&sShootAngleA_4)
                         : "$2", "memory");
    __asm__ __volatile__("lq $2, 0(%1)\n\t"
                         "sq $2, 0(%0)"
                         : : "r"(&travel->position), "r"(&sShootPlaceR_5)
                         : "$2", "memory");
    __asm__ __volatile__("lq $2, 0(%1)\n\t"
                         "sq $2, 0(%0)"
                         : : "r"(&travel->place_velocity), "r"(&sShootPlaceV_6)
                         : "$2", "memory");
    __asm__ __volatile__("lq $2, 0(%1)\n\t"
                         "sq $2, 0(%0)"
                         : : "r"(&travel->place_delta), "r"(&sShootPlaceA_7)
                         : "$2", "memory");
    travel->chase_yaw = travel->rotation.y;
    travel->chase_distance = travel->position.z;
    __asm__ __volatile__("lq $2, 0(%1)\n\t"
                         "sq $2, 0(%0)"
                         : : "r"(&travel->chase_reference), "r"(&sChaseRefPos_8)
                         : "$2", "memory");
    __asm__ __volatile__("lq $2, 0(%1)\n\t"
                         "sq $2, 0(%0)"
                         : : "r"(&travel->chase_position), "r"(&sChaseIntPos_9)
                         : "$2", "memory");
}

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
    u8 unmodeled_94[0x34];
    u8 analog_magnitude[4];
    signed char analog_signed[4];
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

static void xglCameraTravelManual(XglCamera *camera)
{
    CameraTravel *travel = &camera->travel;
    Vector4 axes[4];
    float damping;

    xglCameraTravelReset((StudioCamera *)travel);
    xglCameraTravelFocus((float *)travel);
    if (PadData.analog_signed[1] < 0) {
        travel->angle_delta.x = (PadData.analog_signed[1] + PadData.analog_magnitude[1]) * -6.981317e-05f;
    }
    if (PadData.analog_signed[1] > 0) {
        travel->angle_delta.x = (PadData.analog_signed[1] - PadData.analog_magnitude[1]) * -6.981317e-05f;
    }
    if (PadData.analog_signed[0] < 0) {
        travel->angle_delta.y = (PadData.analog_signed[0] + PadData.analog_magnitude[0]) * -6.981317e-05f;
    }
    if (PadData.analog_signed[0] > 0) {
        travel->angle_delta.y = (PadData.analog_signed[0] - PadData.analog_magnitude[0]) * -6.981317e-05f;
    }
    if (PadData.trigger.v & 0x8000) {
        travel->angle_delta.z = 0.0022340214f;
    }
    if (PadData.trigger.v & 0x2000) {
        travel->angle_delta.z = -0.0022340214f;
    }
    if (PadData.analog_signed[2] < 0) {
        travel->place_delta.x = (PadData.analog_signed[2] + PadData.analog_magnitude[2]) * 0.0002f;
    }
    if (PadData.analog_signed[2] > 0) {
        travel->place_delta.x = (PadData.analog_signed[2] - PadData.analog_magnitude[2]) * 0.0002f;
    }
    if (PadData.trigger.v & 1) {
        travel->place_delta.y = -0.0064f;
    }
    if (PadData.trigger.v & 4) {
        travel->place_delta.y = 0.0064f;
    }
    if (PadData.analog_signed[3] < 0) {
        travel->place_delta.z = (PadData.analog_signed[3] + PadData.analog_magnitude[3]) * 0.0002f;
    }
    if (PadData.analog_signed[3] > 0) {
        travel->place_delta.z = (PadData.analog_signed[3] - PadData.analog_magnitude[3]) * 0.0002f;
    }
    damping = 0.8f;
    xglCameraTravelScale((float *)travel);
    travel->fov_velocity = travel->fov_velocity * damping + travel->scale;
    travel->fov += travel->fov_velocity;
    xglVectorScaleXYZ(&travel->angle_velocity, &travel->angle_velocity, damping);
    xglVectorScaleXYZ(&travel->place_velocity, &travel->place_velocity, damping);
    __asm__ __volatile__("lqc2 vf3, 0(%1)\n\t"
                         "lqc2 vf2, 0(%0)\n\t"
                         "vadd.xyz vf2, vf2, vf3\n\t"
                         "sqc2 vf2, 0(%0)\n\t"
                         "nop"
                         : : "r"(&travel->angle_velocity), "r"(&travel->angle_delta)
                         : "memory");
    xglMatrixStackUnit();
    xglMatrixStackRotY(travel->rotation.y);
    xglMatrixStackSave((float (*)[4])axes);
    xglVectorScaleAddXYZ(&travel->place_velocity, &axes[0], &travel->place_velocity, travel->place_delta.x);
    travel->place_velocity.y += travel->place_delta.y;
    xglVectorScaleAddXYZ(&travel->place_velocity, &axes[2], &travel->place_velocity, travel->place_delta.z);
    __asm__ __volatile__("lqc2 vf3, 0(%1)\n\t"
                         "lqc2 vf2, 0(%0)\n\t"
                         "vadd.xyz vf2, vf2, vf3\n\t"
                         "sqc2 vf2, 0(%0)"
                         : : "r"(&travel->rotation), "r"(&travel->angle_velocity) : "memory");
    __asm__ __volatile__("lqc2 vf3, 0(%1)\n\t"
                         "lqc2 vf2, 0(%0)\n\t"
                         "vadd.xyz vf2, vf2, vf3\n\t"
                         "sqc2 vf2, 0(%0)"
                         : : "r"(&travel->position), "r"(&travel->place_velocity) : "memory");
}

static void xglCameraTravelChase(XglCamera *camera)
{
    CameraTravel *travel = &camera->travel;
    Vector4 offset;
    Vector4 placement[4];
    float damping;
    float yaw_sine;

    xglCameraTravelReset((StudioCamera *)travel);
    xglCameraTravelFocus((float *)travel);
    if (PadData.analog_signed[2] < 0) {
        travel->angle_delta.y = (PadData.analog_signed[2] + PadData.analog_magnitude[2]) * 6.981317e-05f;
    }
    if (PadData.analog_signed[2] > 0) {
        travel->angle_delta.y = (PadData.analog_signed[2] - PadData.analog_magnitude[2]) * 6.981317e-05f;
    }
    if (PadData.trigger.v & 1) {
        travel->place_delta.y = -0.0064f;
    }
    if (PadData.trigger.v & 4) {
        travel->place_delta.y = 0.0064f;
    }
    if (PadData.analog_signed[3] < 0) {
        travel->place_delta.z = (PadData.analog_signed[3] + PadData.analog_magnitude[3]) * 0.0002f;
    }
    if (PadData.analog_signed[3] > 0) {
        travel->place_delta.z = (PadData.analog_signed[3] - PadData.analog_magnitude[3]) * 0.0002f;
    }
    damping = 0.8f;
    xglCameraTravelScale((float *)travel);
    travel->fov_velocity = travel->fov_velocity * damping + travel->scale;
    travel->fov += travel->fov_velocity;
    xglVectorScaleXYZ(&travel->angle_velocity, &travel->angle_velocity, damping);
    xglVectorScaleXYZ(&travel->place_velocity, &travel->place_velocity, damping);
    __asm__ __volatile__("lqc2 vf3, 0(%1)\n\t"
                         "lqc2 vf2, 0(%0)\n\t"
                         "vadd.xyz vf2, vf2, vf3\n\t"
                         "sqc2 vf2, 0(%0)"
                         : : "r"(&travel->angle_velocity), "r"(&travel->angle_delta) : "memory");
    __asm__ __volatile__("lqc2 vf3, 0(%1)\n\t"
                         "lqc2 vf2, 0(%0)\n\t"
                         "vadd.xyz vf2, vf2, vf3\n\t"
                         "sqc2 vf2, 0(%0)"
                         : : "r"(&travel->place_velocity), "r"(&travel->place_delta) : "memory");
    travel->chase_distance += travel->place_velocity.z;
    travel->chase_yaw += travel->angle_velocity.y;
    if (travel->chase_distance < 0.4f) {
        travel->chase_distance = 0.4f;
    }
    if (travel->chase_distance > 100.0f) {
        travel->chase_distance = 100.0f;
    }
    __asm__ __volatile__("lqc2 vf3, 0(%1)\n\t"
                         "lqc2 vf2, 0(%0)\n\t"
                         "vsub.xyz vf2, vf2, vf3\n\t"
                         "sqc2 vf2, 0(%2)"
                         : : "r"(&travel->chase_reference), "r"(&travel->chase_position), "r"(&offset)
                         : "memory");
    xglVectorLength(&offset.w, &offset);
    if (offset.w < 1.0f) {
        xglVectorScaleAddXYZ(&travel->chase_position, &offset, &travel->chase_position, 0.2f);
    } else {
        __asm__ __volatile__("lq $2, 0(%1)\n\t"
                             "sq $2, 0(%0)"
                             : : "r"(&travel->chase_position), "r"(&travel->chase_reference)
                             : "$2", "memory");
    }
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = travel->chase_distance;
    offset.w = 1.0f;
    xglMatrixStackUnit();
    xglMatrixStackTrans(&travel->chase_position.x);
    xglMatrixStackRotY(travel->chase_yaw);
    xglMatrixStackTrans(&offset.x);
    xglMatrixStackSave((float (*)[4])placement);
    travel->position.y += travel->place_velocity.y;
    travel->position.x = placement[3].x;
    travel->position.z = placement[3].z;
    if (travel->position.y < 0.4f) {
        travel->position.y = 0.4f;
    }
    if (travel->position.y > 100.0f) {
        travel->position.y = 100.0f;
    }
    __asm__ __volatile__("lqc2 vf3, 0(%1)\n\t"
                         "lqc2 vf2, 0(%0)\n\t"
                         "vsub.xyz vf2, vf2, vf3\n\t"
                         "sqc2 vf2, 0(%2)"
                         : : "r"(&travel->chase_position), "r"(&travel->position), "r"(&offset)
                         : "memory");
    xglVectorNormal(&offset, &offset);
    offset.z = -offset.z;
    travel->rotation.y = atan2f(offset.x, offset.z);
    yaw_sine = xglSin(travel->rotation.y);
    travel->rotation.x = atan2f(offset.y, offset.x * yaw_sine + offset.z * xglCos(travel->rotation.y));
    travel->rotation.y = -travel->rotation.y;
}

int xglCameraTravelProc(StudioCamera *studio)
{
    XglCamera *camera = (XglCamera *)studio;

    switch (camera->state) {
    case 0:
    case 1:
    case 4:
    default:
        break;
    case 2:
        if (!(PadData.trigger.v & 0x100)) {
            xglCameraTravelManual(camera);
            break;
        }
        if (PadData.hold.v & 0x10) {
            xglCameraTravelInit(studio);
        }
        break;
    case 3:
        if (PadData.trigger.v & 0x100) {
            if (PadData.hold.v & 0x10) {
                xglCameraTravelInit(studio);
            }
            break;
        }
        xglCameraTravelChase(camera);
        break;
    }
}

static void xglCameraViewScreen(StudioCamera *studio)
{
    XglCamera *camera = (XglCamera *)studio;
    XglCameraScreen *screen = &camera->screen;
    float depth[2];
    unsigned int near = screen->depthMin;
    unsigned int range;
    Vector4 scaled;
    float aspect;
    float magnify;
    float centerX;
    float centerY;

    centerX = (screen->window.z + screen->window.x) * 0.5f - screen->resolution.x * 0.5f;
    magnify = 54.0f / (float)(((int)(DispEnv.display >> 23) & 0xf) + 1);
    aspect = 12.272727f / magnify;
    centerY = (screen->window.w + screen->window.y) * 0.5f - screen->resolution.y * 0.5f;
    if ((int)near >= 0) {
        depth[0] = (int)near;
    } else {
        float half = (float)(int)((near & 1) | (near >> 1));
        depth[0] = half + half;
    }
    range = screen->depthMax - near;
    if ((int)range >= 0) {
        depth[1] = (int)range;
    } else {
        float half = (float)(int)((range & 1) | (range >> 1));
        depth[1] = half + half;
    }
    if (screen->resolution.z > screen->resolution.w) {
        xglVectorScale(&scaled, &screen->resolution,
                       (4096.0f - __builtin_fabsf(2.0f * centerX)) / screen->resolution.z);
    } else {
        xglVectorScale(&scaled, &screen->resolution,
                       (4096.0f - __builtin_fabsf(2.0f * centerY)) / screen->resolution.w);
    }
    screen->clipRange.z = screen->projection.x * (scaled.z / screen->resolution.z);
    screen->clipRange.w = screen->projection.y * (scaled.w / screen->resolution.w);
    __asm__ __volatile__("lq $2, 0(%1)\n\t"
                         "sq $2, 0(%0)"
                         : : "r"(&screen->projection), "r"(&screen->resolution)
                         : "$2", "memory");
    screen->projection.x *= aspect;
    screen->screenOffset.x = centerX + 2048.0f;
    screen->projection.z *= aspect;
    screen->screenOffset.y = centerY + 2048.0f;
    screen->screenOffset.z = depth[0] + depth[1] * 0.5f;
    screen->screenOffset.w = (1.0f - screen->portEffect.z) * 255.0f;
    screen->screenScale.x = scaled.x * screen->portEffect.x * 0.5f;
    screen->screenScale.y = -(scaled.y * screen->portEffect.y * 0.5f);
    screen->screenScale.z = -(depth[1] * 0.5f);
    screen->screenScale.w = screen->portEffect.z * screen->portEffect.w * 255.0f;
}

static void xglCameraViewTravel(StudioCamera *studio, Vector4 *offset)
{
    XglCamera *camera = (XglCamera *)studio;
    Vector4 *frustum = &camera->travel.frustum[3];
    XglCameraScreen *screen = &camera->screen;
    CameraTravel *travel = &camera->travel;
    Vector4 extent;
    Vector4 ratio;
    Vector4 shift;
    float tangent;
    float nearClip;

    tangent = tanf(travel->fov * 0.5f);
    extent.y = extent.x = tangent;
    extent.w = extent.z = tangent * (screen->clipRange.w / screen->projection.y);
    ratio.y = ratio.x = screen->projection.x / screen->projection.y;
    nearClip = screen->clipRange.x;
    ratio.w = ratio.z = screen->clipRange.z / screen->clipRange.w;
    travel->frustum[3].x = nearClip;
    travel->frustum[3].w = travel->frustum[3].y = screen->clipRange.y;
    travel->frustum[3].z = nearClip;
    __asm__ __volatile__("lqc2 vf20, 0(%0)" : : "r"(frustum) : "memory");
    __asm__ __volatile__("lqc2 vf21, 0(%0)" : : "r"(&extent) : "memory");
    __asm__ __volatile__("vmul.xyzw vf20, vf20, vf21" : : : "memory");
    __asm__ __volatile__("sqc2 vf20, 0(%0)" : : "r"(frustum) : "memory");
    xglVectorScale(&camera->travel.frustum[2], frustum, -1.0f);
    __asm__ __volatile__("lqc2 vf20, 0(%1)\n\t"
                         "lqc2 vf21, 0(%2)\n\t"
                         "vmul.xyzw vf20, vf20, vf21\n\t"
                         "sqc2 vf20, 0(%0)"
                         : : "r"(&camera->travel.frustum[1]), "r"(frustum), "r"(&ratio) : "memory");
    xglVectorScale(&camera->travel.frustum[0], &camera->travel.frustum[1], -1.0f);
    xglMatrixStackUnit();
    __asm__ __volatile__("lqc2 vf3, 0(%1)\n\t"
                         "lqc2 vf2, 0(%0)\n\t"
                         "vsub.xyz vf2, vf2, vf3\n\t"
                         "sqc2 vf2, 0(%2)\n\t"
                         "nop"
                         : : "r"(&travel->position), "r"(offset), "r"(&shift) : "memory");
    xglMatrixStackTrans(&shift.x);
    xglMatrixStackRotY(travel->rotation.y);
    xglMatrixStackRotX(travel->rotation.x);
    xglMatrixStackRotZ(travel->rotation.z);
    xglMatrixStackSave(camera->transform);
    xglMatrixStackInverse();
    xglMatrixStackSave(camera->inverseTransform);
}

static void xglCameraViewVolume(StudioCamera *studio, Vector4 *offset)
{
    XglCamera *camera = (XglCamera *)studio;
    XglCameraScreen *screen = &camera->screen;
    CameraVolume *volume;
    Vector4 corner;
    int i;
    int j;
    int k;
    int edge;
    float gx;
    float gy;
    float gz;
    float (*viewInverse)[4];
    float (*transform)[4];
    Vector4 *travelFrustum;

    i = 0;
    travelFrustum = camera->travel.frustum;
    viewInverse = camera->inverseTransform;
    transform = camera->transform;
    for (; i < 1; i++) {
        volume = &camera->volume[i];
        __asm__ __volatile__("lq $2, 0(%1)\n\t"
                             "sq $2, 0(%0)\n\t"
                             "lq $2, 16(%1)\n\t"
                             "sq $2, 16(%0)\n\t"
                             "lq $2, 32(%1)\n\t"
                             "sq $2, 32(%0)\n\t"
                             "lq $2, 48(%1)\n\t"
                             "sq $2, 48(%0)"
                             : : "r"(volume->frustum), "r"(travelFrustum)
                             : "$2", "memory");
        xglMatrixStackUnit();
        xglMatrixStackFrustum(volume->frustum[0][2], volume->frustum[1][2],
                              volume->frustum[2][2], volume->frustum[3][2],
                              screen->clipRange.x, screen->clipRange.y);
        xglMatrixStackMul(viewInverse);
        xglMatrixStackSave(volume->viewMatrix);
        for (j = 0; j < 2; j++) {
            for (k = 0; k < 8; k++) {
                edge = (((k & 4) >> 2) | ((j & 1) << 1));
                gx = volume->frustum[k & 1][edge];
                gy = volume->frustum[((k & 2) >> 1) | 2][edge];
                gz = (&screen->clipRange.x)[edge & 1];
                corner.x = gx;
                corner.y = gy;
                corner.z = -gz;
                corner.w = 1.0f;
                xglVectorMulMat(&volume->vertex[j * 8 + k].vector, transform, &corner);
            }
            for (k = 0; k < 6; k++) {
                xglPlaneParameter(&(volume->plane + j * 6)[k].vector,
                                  &(volume->vertex + j * 8)[aaiVertexIndex[k][0]].vector,
                                  &(volume->vertex + j * 8)[aaiVertexIndex[k][1]].vector,
                                  &(volume->vertex + j * 8)[aaiVertexIndex[k][2]].vector);
            }
        }
        for (j = 0; j < 4; j++) {
            for (k = 0; k < 3; k++) {
                volume->planeSet[0][j][k] = volume->plane[k].element[j];
                volume->planeSet[1][j][k] = volume->plane[k + 3].element[j];
                volume->planeSet[2][j][k] = volume->plane[k + 6].element[j];
                volume->planeSet[3][j][k] = volume->plane[k + 9].element[j];
            }
        }
    }
}

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
    ((void (*)())xglCameraViewScreen)(camera, &offset);
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

#include "common.h"
#include "shared.h"
#include "camera.h"

/*
 * getRotateX__/getRotateY__/getRotateZ__ each keep their own .lit4 copy of
 * the radian-to-degree divisor (bits 0x40490fdb, pi); the scaffold still
 * owns the pool and did not merge the three entries.
 */
extern const float D_004D83C4;

void Java_xeno_Camera_getRotateX__(JavaEnvironment *environment,
                                   CameraGetterArgs *arguments, float *result)
{
    (void)environment;
    *result = xglStudioGetCamera2(arguments->camera->camera_id)->rotation.x
              / D_004D83C4 * 180.0f;
}

extern const float D_004D83C8;

void Java_xeno_Camera_getRotateY__(JavaEnvironment *environment,
                                   CameraGetterArgs *arguments, float *result)
{
    (void)environment;
    *result = xglStudioGetCamera2(arguments->camera->camera_id)->rotation.y
              / D_004D83C8 * 180.0f;
}

extern const float D_004D83CC;

void Java_xeno_Camera_getRotateZ__(JavaEnvironment *environment,
                                   CameraGetterArgs *arguments, float *result)
{
    (void)environment;
    *result = xglStudioGetCamera2(arguments->camera->camera_id)->rotation.z
              / D_004D83CC * 180.0f;
}

void Java_xeno_Camera_getTranslateX__(JavaEnvironment *environment,
                                      CameraGetterArgs *arguments, float *result)
{
    (void)environment;
    *result = xglStudioGetCamera2(arguments->camera->camera_id)->position.x;
}

void Java_xeno_Camera_getTranslateY__(JavaEnvironment *environment,
                                      CameraGetterArgs *arguments, float *result)
{
    (void)environment;
    *result = xglStudioGetCamera2(arguments->camera->camera_id)->position.y;
}

void Java_xeno_Camera_getTranslateZ__(JavaEnvironment *environment,
                                      CameraGetterArgs *arguments, float *result)
{
    (void)environment;
    *result = xglStudioGetCamera2(arguments->camera->camera_id)->position.z;
}

void Java_xeno_Camera_fovSPL__aFI(JavaEnvironment *environment,
                                  CameraSplineRequest *request, void *result)
{
    CameraWork *camera = request->camera;
    unsigned int weight_mode = request->weight_mode;
    unsigned char *camera_bytes = (unsigned char *)camera;
    const JavaFloatArray *sample_array = request->samples;

    (void)environment;
    (void)result;
    (void)xglStudioGetCamera2(camera->camera_id);
    camera->mode[CAMERA_CHANNEL_FOV] = CAMERA_MODE_SPLINE;
    camera->frame[CAMERA_CHANNEL_FOV] = 0;
    SPL_init((CameraSpline *)(camera_bytes + 0xe10), weight_mode,
             sample_array->elements, sample_array->length >> 1, 1);
    ((CameraSpline *)(camera_bytes + 0xe10))->first_key = (unsigned short)-1;
}

/*
 * The game loop's own record, as far as this unit reads it: only the
 * halfword camera_change latches the matched studio camera index into,
 * at +0xc8 (sh at 0x002fb5e0). Nothing before it is read or written here.
 */
typedef struct {
    unsigned char unmodeled_00[0xc8];
    short activeCameraIndex; /* +0xc8 */
} CameraLoopState;

extern CameraLoopState GameLoopState;

static void camera_change(int camera_id)
{
    StudioCamera *studio_camera;
    int index;

    for (index = 0; index < 8; index++) {
        studio_camera = xglStudioGetCamera2(index);
        if (camera_id == index) {
            studio_camera->active = 1;
            GameLoopState.activeCameraIndex = (short)index;
        } else {
            studio_camera->active = 0;
        }
    }
}

void Java_xeno_Camera_change__(JavaEnvironment *environment, CameraGetterArgs *arguments)
{
    (void)environment;
    camera_change(arguments->camera->camera_id);
}

static void CAMERA_rotateSPL(int selection, JavaEnvironment *environment,
                             CameraSplineRequest *request)
{
    CameraWork *camera = request->camera;
    unsigned int weight_mode = request->weight_mode;
    unsigned char *camera_bytes = (unsigned char *)camera;
    const JavaFloatArray *sample_array;

    (void)environment;
    sample_array = request->samples;
    (void)xglStudioGetCamera2(camera->camera_id);
    camera->mode[CAMERA_CHANNEL_VIEW] = CAMERA_MODE_ROTATE_SPLINE;
    camera->frame[CAMERA_CHANNEL_VIEW] = 0;

    /* The view channel's spline payload is at camera + 0x4d0: the four channel
     * payloads are 0x4a0 apart (TCAMERA_update passes +0x30, +0x4d0, +0x970 and
     * +0xe10 to the channel handlers) and nothing evidences the internal extent
     * of a payload, so the offset cannot become a CameraWork member without
     * inventing the span in between.
     * A rotation key is four floats -- one weight plus x, y and z, SplinePoint
     * in src/main/spl.h -- so the array's float count >> 2 is its key count. */
    switch (selection) {
    case 0:
        SPL_init((CameraSpline *)(camera_bytes + 0x4d0), weight_mode,
                 sample_array->elements, sample_array->length >> 2, 3);
        ((CameraSpline *)(camera_bytes + 0x4d0))->first_key =
            (unsigned short)-1;
        break;
    case 1:
        SPL_init((CameraSpline *)(camera_bytes + 0x4d0), weight_mode,
                 sample_array->elements, sample_array->length >> 2, 3);
        ((CameraSpline *)(camera_bytes + 0x4d0))->first_key =
            request->first_key;
        ((CameraSpline *)(camera_bytes + 0x4d0))->last_key =
            request->last_key;
        break;
    }
}

void Java_xeno_Camera_rotateSPL__aFI(JavaEnvironment *environment,
                                     CameraSplineRequest *arguments,
                                     void *result)
{
    /* The VM supplies a result slot as the helper's fourth ABI argument, even
     * though CAMERA_rotateSPL does not read it. */
    ((void (*)(int, JavaEnvironment *, CameraSplineRequest *, void *))CAMERA_rotateSPL)(
        0, environment, arguments, result);
}

void Java_xeno_Camera_rotateSPL__aFIII(JavaEnvironment *environment,
                                       CameraSplineRequest *arguments,
                                       void *result)
{
    /* Keep forwarding the VM's unused result slot in the original ABI slot. */
    ((void (*)(int, JavaEnvironment *, CameraSplineRequest *, void *))CAMERA_rotateSPL)(
        1, environment, arguments, result);
}

void Java_xeno_Camera_setActive__Z(JavaEnvironment *environment,
                                   CameraSetActiveArgs *arguments, void *result)
{
    (void)environment;
    (void)result;
    xglStudioGetCamera2(arguments->camera->camera_id)->active = arguments->active;
}

/* This TU's own .lit4 copy of the fov-scale seed 1.2, stored at CameraWork
 * +0x12b4 (docs above CameraWork). */
extern const float D_004D83D0;
extern void *classJava_xeno_Camera;
/* The engine's camera table (main:0x00465e10, size 0x9600 = 8 * 0x12c0):
 * TCAMERA_get (still asm, src/main/tcamera.c) indexes it the same way. */
extern unsigned char tcamera[];

void Java_xeno_Camera_create__I(JavaEnvironment *environment,
                                CameraCreateArgs *arguments, CameraWork **result)
{
    int camera_id = arguments->camera_id;
    unsigned char *camera_bytes = tcamera + camera_id * 0x12c0;
    CameraWork *camera = (CameraWork *)camera_bytes;

    (void)environment;
    /* CameraWork stops at +0x2c (docs above CameraWork); +0x12b4 is beyond
     * that partial extent, so the fov-scale seed is stored through the byte
     * pointer rather than a struct member. */
    *(float *)(camera_bytes + 0x12b4) = D_004D83D0;
    /* CameraWork's own +0x00 word: filled from +0x18 of the loaded class
     * record (docs above CameraWork); its meaning is not recovered, so it
     * is stored through the byte pointer rather than the anonymous
     * bitfield, which no C expression can name. classJava_xeno_Camera's own
     * +0x18 is a JVM class-record field this TU does not model either. */
    *(unsigned int *)camera_bytes =
        *(unsigned int *)((unsigned char *)classJava_xeno_Camera + 0x18);
    camera->camera_id = camera_id;
    xglStudioGetCamera2(camera_id);
    *result = camera;
}

extern int JNI_isInstanceOf(SceneObject object, SceneClass *target_class);
extern SceneString *loadConstString(const char *bytes, int length);
extern JavaField *lookupClassField(void *class_object, void *name, int flags);
extern void *classJava_xeno_Chr;
extern const char D_004DC178[];

void Java_xeno_Camera_transCNS__Ljava_lang_Object_FFF(JavaEnvironment *environment,
                                                      union { int i; float f; void *p; } *args)
{
    CameraWork *camera;
    SceneObject object;
    unsigned char *camera_bytes;
    float v[3];

    (void)environment;
    camera = args[0].p;
    object = args[1].p;
    v[0] = args[2].f;
    v[1] = args[3].f;
    v[2] = args[4].f;
    camera_bytes = (unsigned char *)camera;
    xglStudioGetCamera2(camera->camera_id);
    camera->mode[CAMERA_CHANNEL_TRANSLATE] = CAMERA_MODE_CONSTANT;
    if (JNI_isInstanceOf(object, classJava_xeno_Chr) == 1) {
        *(int *)(camera_bytes + 0x44) =
            *(int *)(object + lookupClassField(classJava_xeno_Chr,
                         loadConstString(D_004DC178, -1), 0)->offset) + 0x10;
    } else if (JNI_isInstanceOf(object, classJava_xeno_Unit) == 1) {
        *(int *)(camera_bytes + 0x44) =
            *(int *)(object + lookupClassField(classJava_xeno_Unit,
                         loadConstString(D_004DC178, -1), 0)->offset) + 0x10;
    }
done:
    ((union { int i; float f; } *)(camera_bytes + 0x40))->i = 1;
    *(float *)(camera_bytes + 0x30) = v[0];
    *(float *)(camera_bytes + 0x34) = v[1];
    *(float *)(camera_bytes + 0x38) = v[2];
}

static void CAMERA_transSPL(int selection, JavaEnvironment *environment,
                            CameraSplineRequest *request, void *result)
{
    CameraWork *camera = request->camera;
    unsigned int weight_mode = request->weight_mode;
    unsigned char *camera_bytes = (unsigned char *)camera;
    const JavaFloatArray *sample_array;

    (void)environment;
    (void)result;
    sample_array = request->samples;
    (void)xglStudioGetCamera2(camera->camera_id);
    camera->mode[CAMERA_CHANNEL_TRANSLATE] = CAMERA_MODE_SPLINE;
    camera->frame[CAMERA_CHANNEL_TRANSLATE] = 0;

    switch (selection) {
    case 0:
        SPL_init((CameraSpline *)(camera_bytes + 0x30), weight_mode,
                 sample_array->elements, sample_array->length >> 2, 3);
        ((CameraSpline *)(camera_bytes + 0x30))->first_key =
            (unsigned short)-1;
        break;
    case 1:
        SPL_init((CameraSpline *)(camera_bytes + 0x30), weight_mode + 2,
                 sample_array->elements, sample_array->length >> 2, 3);
        ((CameraSpline *)(camera_bytes + 0x30))->first_key =
            request->first_key;
        ((CameraSpline *)(camera_bytes + 0x30))->last_key =
            request->last_key;
        break;
    }
}

void Java_xeno_Camera_transSPL__aFI(JavaEnvironment *environment,
                                    CameraSplineRequest *request,
                                    void *result)
{
    CAMERA_transSPL(0, environment, request, result);
}

void Java_xeno_Camera_transSPL__aFIII(JavaEnvironment *environment,
                                      CameraSplineRequest *request,
                                      void *result)
{
    CAMERA_transSPL(1, environment, request, result);
}

INCLUDE_ASM("asm/main/nonmatchings/camera", Java_xeno_Camera_viewCNS__Ljava_lang_Object_FFF);

static void CAMERA_viewSPL(int selection, JavaEnvironment *environment,
                           CameraSplineRequest *request, void *result)
{
    CameraWork *camera = request->camera;
    unsigned int weight_mode = request->weight_mode;
    unsigned char *camera_bytes = (unsigned char *)camera;
    const JavaFloatArray *sample_array;

    (void)environment;
    (void)result;
    sample_array = request->samples;
    (void)xglStudioGetCamera2(camera->camera_id);
    camera->mode[CAMERA_CHANNEL_VIEW] = CAMERA_MODE_SPLINE;
    camera->frame[CAMERA_CHANNEL_VIEW] = 0;

    switch (selection) {
    case 0:
        SPL_init((CameraSpline *)(camera_bytes + 0x4d0), weight_mode,
                 sample_array->elements, sample_array->length >> 2, 3);
        ((CameraSpline *)(camera_bytes + 0x4d0))->first_key =
            (unsigned short)-1;
        break;
    case 1:
        SPL_init((CameraSpline *)(camera_bytes + 0x4d0), weight_mode,
                 sample_array->elements, sample_array->length >> 2, 3);
        ((CameraSpline *)(camera_bytes + 0x4d0))->first_key =
            request->first_key;
        ((CameraSpline *)(camera_bytes + 0x4d0))->last_key =
            request->last_key;
        break;
    }
}

void Java_xeno_Camera_viewSPL__aFI(JavaEnvironment *environment,
                                   CameraSplineRequest *request,
                                   void *result)
{
    CAMERA_viewSPL(0, environment, request, result);
}

void Java_xeno_Camera_viewSPL__aFIII(JavaEnvironment *environment,
                                     CameraSplineRequest *request,
                                     void *result)
{
    CAMERA_viewSPL(1, environment, request, result);
}

void Java_xeno_Camera_rollSPL__aFI(JavaEnvironment *environment,
                                   CameraSplineRequest *request, void *result)
{
    CameraWork *camera = request->camera;
    unsigned int weight_mode = request->weight_mode;
    unsigned char *camera_bytes = (unsigned char *)camera;
    const JavaFloatArray *sample_array = request->samples;

    (void)environment;
    (void)result;
    (void)xglStudioGetCamera2(camera->camera_id);
    camera->mode[CAMERA_CHANNEL_ROLL] = CAMERA_MODE_SPLINE;
    camera->frame[CAMERA_CHANNEL_ROLL] = 0;
    SPL_init((CameraSpline *)(camera_bytes + 0x970), weight_mode,
             sample_array->elements, sample_array->length >> 1, 1);
    ((CameraSpline *)(camera_bytes + 0x970))->first_key = (unsigned short)-1;
}

extern void TCAMERA_setRotate(StudioCamera *camera, float pitchDegrees, float yawDegrees, float rollDegrees);

void Java_xeno_Camera_setRotate__FFF(JavaEnvironment *environment,
                                     union { int i; float f; void *p; } *args)
{
    CameraWork *camera;
    StudioCamera *studio_camera;

    (void)environment;
    camera = args[0].p;
    studio_camera = xglStudioGetCamera2(camera->camera_id);
    camera->mode[CAMERA_CHANNEL_VIEW] = CAMERA_MODE_IDLE;
    TCAMERA_setRotate(studio_camera, args[1].f, args[2].f, args[3].f);
}

extern void TCAMERA_setRoll(StudioCamera *camera, float degrees);

void Java_xeno_Camera_setRoll__F(JavaEnvironment *environment,
                                 union { int i; float f; void *p; } *args)
{
    CameraWork *camera;
    StudioCamera *studio_camera;

    (void)environment;
    camera = args[0].p;
    studio_camera = xglStudioGetCamera2(camera->camera_id);
    camera->mode[CAMERA_CHANNEL_ROLL] = CAMERA_MODE_IDLE;
    TCAMERA_setRoll(studio_camera, args[1].f);
}

extern void TCAMERA_setTranslate(StudioCamera *camera, float x, float y, float z);

void Java_xeno_Camera_setTranslate__FFF(JavaEnvironment *environment,
                                     union { int i; float f; void *p; } *args)
{
    CameraWork *camera;
    StudioCamera *studio_camera;

    (void)environment;
    camera = args[0].p;
    studio_camera = xglStudioGetCamera2(camera->camera_id);
    camera->mode[CAMERA_CHANNEL_TRANSLATE] = CAMERA_MODE_IDLE;
    TCAMERA_setTranslate(studio_camera, args[1].f, args[2].f, args[3].f);
}

extern void TCAMERA_setFov(StudioCamera *camera, float degrees);

void Java_xeno_Camera_setFov__F(JavaEnvironment *environment,
                                 union { int i; float f; void *p; } *args)
{
    CameraWork *camera;
    StudioCamera *studio_camera;

    (void)environment;
    camera = args[0].p;
    studio_camera = xglStudioGetCamera2(camera->camera_id);
    camera->mode[CAMERA_CHANNEL_FOV] = CAMERA_MODE_IDLE;
    TCAMERA_setFov(studio_camera, args[1].f);
}

extern const float D_004D83D4;

/*
 * The studio camera's field-of-view scalar, at +0x94 inside StudioCamera's
 * unmodeled_90 span (include/shared.h): that struct is shared with other
 * TUs and not owned here, so the offset is named rather than completing the
 * shared layout. TCAMERA_setFov (src/main/tcamera.c) evidences the same
 * offset on the same record through its own local view.
 */
#define CAMERA_STUDIO_FOV(studio_camera) (*(const float *)((const unsigned char *)(studio_camera) + 0x94))

void Java_xeno_Camera_getFov__(JavaEnvironment *environment,
                               CameraGetterArgs *arguments, float *result)
{
    (void)environment;
    *result = CAMERA_STUDIO_FOV(xglStudioGetCamera2(arguments->camera->camera_id))
              / D_004D83D4 * 180.0f;
}

extern void TCAMERA_setView(StudioCamera *camera, float x, float y, float z);

void Java_xeno_Camera_setView__FFF(JavaEnvironment *environment,
                                     union { int i; float f; void *p; } *args)
{
    CameraWork *camera;
    StudioCamera *studio_camera;

    (void)environment;
    camera = args[0].p;
    studio_camera = xglStudioGetCamera2(camera->camera_id);
    camera->mode[CAMERA_CHANNEL_VIEW] = CAMERA_MODE_IDLE;
    TCAMERA_setView(studio_camera, args[1].f, args[2].f, args[3].f);
}

extern int JNI_isInstanceOf(SceneObject object, SceneClass *target_class);
extern SceneString *loadConstString(const char *bytes, int length);
extern JavaField *lookupClassField(void *class_object, void *name, int flags);
extern void *classJava_xeno_Chr;
/* This TU's own copy of the field name string "peer" (the scaffold still
 * owns the literal pool, so it is a separate address from chr.c's
 * chr_peer_string). */
extern const char D_004DC178[];

void Java_xeno_Camera_start__ILjava_lang_Object_(JavaEnvironment *environment,
                                                 CameraStartArgs *arguments,
                                                 void *result)
{
    CameraWork *camera = arguments->camera;
    int state = arguments->state;
    SceneObject peer = arguments->peer;
    unsigned char *camera_bytes = (unsigned char *)camera;
    JavaField *peer_field;

    (void)environment;
    (void)result;
    xglStudioGetCamera2(camera->camera_id)->state = state;
    if (state == 3) {
        if (peer != 0) {
            if (JNI_isInstanceOf(peer, classJava_xeno_Chr) != 0) {
                peer_field = lookupClassField(classJava_xeno_Chr,
                                              loadConstString(D_004DC178, -1), 0);
                /* `peer` is a field of the Java class xeno.Chr, so its
                 * position inside the object is the offset the JVM hands
                 * back at run time, exactly as CHR_sclX (src/main/chr.c)
                 * reads its own peer field. CameraWork stops at +0x2c (docs
                 * above CameraWork); +0x12b0 is beyond that partial extent. */
                *(int *)(camera_bytes + 0x12b0) =
                    *(int *)(peer + peer_field->offset);
            }
        } else {
            *(int *)(camera_bytes + 0x12b0) = 0;
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/camera", Java_xeno_Camera_setCFAngle__IFFFF);

INCLUDE_ASM("asm/main/nonmatchings/camera", Java_xeno_Camera_setCFAnglePers__IFFFFF);

typedef struct {
    unsigned char unmodeled_00[0x50];
    float hokan0; /* +0x50 */
    float hokan1; /* +0x54 */
    unsigned char unmodeled_58[0x48];
} CfCameraDefineEntry;
extern CfCameraDefineEntry CfCameraDefine[];

void Java_xeno_Camera_setCFHokan__IFF(JavaEnvironment *environment,
                                      union { int i; float f; void *p; } *args)
{
    int id;
    int start;
    int end;
    float a;
    float b;
    unsigned char *state;
    CfCameraDefineEntry *p;

    (void)environment;
    id = args[1].i;
    if (id < 0) {
        start = 0;
        end = 0x21;
    } else {
        start = id;
        end = id + 1;
    }
    a = args[2].f;
    b = args[3].f;
    p = &CfCameraDefine[start];
    for (; start < end; start++) {
        p->hokan0 = a;
        p->hokan1 = b;
        p++;
    }
    state = (unsigned char *)&GameLoopState;
    state[0xc1] |= 0x40;
}

void Java_xeno_Camera_setCFLock__IIFFF(JavaEnvironment *environment,
                                       union { int i; float f; void *p; } *args)
{
    int id;
    int start;
    int end;
    int mode;
    float x;
    float y;
    float z;
    unsigned char *state;
    CfCameraDefineEntry *p;

    (void)environment;
    id = args[1].i;
    if (id < 0) {
        start = 0;
        end = 0x21;
    } else {
        start = id;
        end = id + 1;
    }
    mode = args[2].i;
    x = args[3].f;
    y = args[4].f;
    z = args[5].f;
    p = &CfCameraDefine[start];
    for (; start < end; start++) {
        *(int *)((unsigned char *)p + 0x4c) = mode;
        *(float *)((unsigned char *)p + 0x40) = x;
        *(float *)((unsigned char *)p + 0x44) = y;
        *(float *)((unsigned char *)p + 0x48) = z;
        p++;
    }
    state = (unsigned char *)&GameLoopState;
    state[0xc1] |= 0x40;
}

void Java_xeno_Camera_setCFOffset__IFFFIFIF(JavaEnvironment *environment,
                                            union { int i; float f; void *p; short s; } *args)
{
    int id;
    int start;
    int end;
    float v[3];
    short a;
    float b;
    short c;
    float d;
    unsigned char *state;
    CfCameraDefineEntry *p;

    (void)environment;
    id = args[1].i;
    if (id < 0) {
        start = 0;
        end = 0x21;
    } else {
        start = id;
        end = id + 1;
    }
    v[0] = args[2].f;
    v[1] = args[3].f;
    v[2] = args[4].f;
    p = &CfCameraDefine[start];
    a = args[5].s;
    b = args[6].f;
    c = args[7].s;
    d = args[8].f;
    for (; start < end; start++) {
        *(unsigned char *)p = 0;
        *(float *)((unsigned char *)p + 0x10) = v[0];
        *(float *)((unsigned char *)p + 0x14) = v[1];
        *(float *)((unsigned char *)p + 0x18) = v[2];
        *(int *)((unsigned char *)p + 0x20) = a;
        *(float *)((unsigned char *)p + 0x24) = b;
        *(int *)((unsigned char *)p + 0x28) = c;
        *(float *)((unsigned char *)p + 0x2c) = d;
        p++;
    }
    state = (unsigned char *)&GameLoopState;
    state[0xc1] |= 0x40;
}

typedef struct CameraSetModeArgs {
    unsigned char unmodeled_00[4];
    unsigned char mode;
} CameraSetModeArgs;

void Java_xeno_Camera_setMode__I(JavaEnvironment *environment,
                                 CameraSetModeArgs *arguments)
{
    unsigned char *state;

    (void)environment;
    state = (unsigned char *)&GameLoopState;
    state[0xc0] = arguments->mode;
    state[0xc1] |= 0x40;
    camera_change(0);
}

/*
 * The game loop mode byte, GameLoopState+0xc0. Java_xeno_Camera_getMode__
 * copies it to its caller unchanged (lbu/sw at main:0x002fc5d4/0x002fc5dc).
 * CameraLoopState (above, owned by camera_change's own allocation) stops
 * short of +0xc0, so the byte is reached through GameLoopState's own address
 * rather than growing that struct.
 */
#define GAME_LOOP_MODE_OFFSET 0xc0

#define GAME_LOOP_MODE ((unsigned char *)((unsigned char *)&GameLoopState + GAME_LOOP_MODE_OFFSET))

void Java_xeno_Camera_getMode__(JavaEnvironment *environment, void *arguments, int *result)
{
    (void)environment;
    (void)arguments;
    *result = *GAME_LOOP_MODE;
}

INCLUDE_ASM("asm/main/nonmatchings/camera", Java_xeno_Camera_setCFPedestal__IFFFFFFFF);

void Java_xeno_Camera_setCFPedestalHokan__II(JavaEnvironment *environment,
                                             union { int i; float f; void *p; } *args)
{
    int id;
    int start;
    int end;
    int on;
    CfCameraDefineEntry *p;

    (void)environment;
    id = args[1].i;
    if (id < 0) {
        start = 0;
        end = 0x21;
    } else {
        start = id;
        end = id + 1;
    }
    on = args[2].i;
    p = &CfCameraDefine[start];
    for (; start < end; start++) {
        if (on) {
            ((unsigned char *)p)[1] |= 1;
        } else {
            ((unsigned char *)p)[1] &= 0xfe;
        }
        p++;
    }
}

void Java_xeno_Camera_changeID__III(JavaEnvironment *environment, CameraChangeIDArgs *arguments)
{
    (void)environment;
    GameCameraChangeID(arguments->id, arguments->id2, arguments->id3);
}

typedef unsigned int CameraFogQuad __attribute__((mode(TI)));
extern float I2F(int value);
static inline void fog_cp(void *d, void *s)
{
    do {
        *(volatile CameraFogQuad *)d = *(volatile CameraFogQuad *)s;
    } while (0);
}

void Java_xeno_Camera_setFog__IFFFFIIII(JavaEnvironment *environment,
                                        union { int i; float f; void *p; } *args)
{
    int id;
    int start;
    int end;
    float v[8] __attribute__((aligned(16)));
    CfCameraDefineEntry *p;
    float *src;

    (void)environment;
    id = args[1].i;
    if (id < 0) {
        start = 0;
        end = 0x21;
    } else {
        start = id;
        end = id + 1;
    }
    v[0] = args[2].f;
    v[1] = args[3].f;
    v[2] = args[4].f;
    v[3] = args[5].f;
    v[4] = I2F(args[6].i);
    v[5] = I2F(args[7].i);
    v[6] = I2F(args[8].i);
    v[7] = I2F(args[9].i);
    p = &CfCameraDefine[start];
    if (start < end) {
        src = v;
        for (; start < end; start++) {
            fog_cp((char *)p + 0x60, src);
            fog_cp((char *)p + 0x70, src + 4);
            p++;
        }
    }
}

void Java_xeno_Camera_resetFog__I(JavaEnvironment *environment,
                                  union { int i; float f; void *p; } *args)
{
    int id;
    int start;
    int end;
    CfCameraDefineEntry *p;

    (void)environment;
    id = args[1].i;
    if (id < 0) {
        start = 0;
        end = 0x21;
    } else {
        start = id;
        end = id + 1;
    }
    p = &CfCameraDefine[start];
    for (; start < end; start++) {
        __asm__ volatile("sq $0, 0(%0)" : : "r"((char *)p + 0x60) : "memory");
        __asm__ volatile("sq $0, 0(%0)" : : "r"((char *)p + 0x70) : "memory");
        p++;
    }
}

void Java_xeno_Camera_setClipRange__FF(JavaEnvironment *environment,
                                       CameraSetClipRangeArgs *arguments,
                                       void *result)
{
    StudioCamera *studio_camera = xglStudioGetCamera2(arguments->camera->camera_id);

    (void)environment;
    (void)result;
    studio_camera->nearClip = arguments->nearClip;
    studio_camera->farClip = arguments->farClip;
}

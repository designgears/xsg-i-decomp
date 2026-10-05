#include "common.h"
#include "shared.h"
#include "spline.h"

void Java_xeno_util_Spline_create__(void *environment, void *arguments,
                                    void **result)
{
    SplineClassEntry *spline_class;
    void *class_pointer;
    Spline *spline;

    spline_class = classJava_xeno_util_Spline;
    spline = xmalloc(0x494, 0xE);
    class_pointer = spline_class->class_pointer;
    spline->class_pointer = class_pointer;
    *result = spline;
}

typedef struct SplineCtrlState {
    unsigned char unmodeled_00[6];
    unsigned short weight_mode;
    unsigned short first_key;
} SplineCtrlState;
typedef struct SplineCtrlWeights {
    unsigned char unmodeled_00[4];
    unsigned int count;
    void *data;
} SplineCtrlWeights;
typedef struct SplineCtrlVertex {
    SplineCtrlState *state;
    SplineCtrlWeights *weights;
    int flag;
    int weight_mode;
    unsigned short first_key;
} SplineCtrlVertex;
extern void SPL_init2(SplineCtrlState *spline, int flag, void *data, unsigned int count, int mode);

void Java_xeno_util_Spline_setCtrlVertex__aFIII(void *environment, void *arguments, void **result)
{
    union { int i; float f; void *p; } *args = arguments;
    SplineCtrlState *st = args[0].p;
    int wm = args[3].i;
    SplineCtrlWeights *w = args[1].p;
    int flag = args[2].i;
    st->weight_mode = wm;
    st->first_key = args[4].i;
    SPL_init2(st, flag, w->data, w->count >> 2, 3);
}

typedef struct SplineValueVector {
    SceneObjectClassRef *class_ref;
    float z;
    float y;
    float x;
    float w;
    int pad;
} SplineValueVector;
extern SplineValueVector value_0;
extern SceneClass *classJava_xeno_util_Vector4f;
extern void SPL_getValueXYZ(float *destination, void *spline, float frame);

void Java_xeno_util_Spline_getValue__I(void *environment, void *arguments, int *result)
{
    union { int i; float f; void *p; } *args = arguments;
    float point[3];

    SPL_getValueXYZ(point, args[0].p, (float)args[1].i);
    value_0.x = point[0];
    value_0.y = point[1];
    value_0.z = point[2];
    value_0.w = 1.0f;
    value_0.class_ref = classJava_xeno_util_Vector4f->instance_class_ref;
    ((union { int i; float f; void *p; } *)result)->p = &value_0;
}

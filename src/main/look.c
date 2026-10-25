#include "common.h"
#include "look.h"

                                              /* MARK provisional: .lit4 0x004d8564 */

float xglAsin(float value)
{
    /* MARK: explicit NaN-fallback call structure -- emits sqrt.s + c.eq.s/bc1t
     * plus jal sqrtf@0x003240f8 on the NaN path; a plain sqrtf() call cannot
     * reproduce this (S2 evidence). */
    float root = __builtin_sqrtf(1.0f - value);
    float result;
    float cubic = xglAsin_cubic;
    float quadratic = xglAsin_quadratic;
    float linear = xglAsin_linear;
    float pi_over_two_approx = xglAsin_pi_over_two_approx;

    result = value * cubic;
    result += quadratic;
    result *= value;
    result -= linear;
    result *= value;
    result += pi_over_two_approx;
    return xglAsin_pi_over_two - root * result;
}

void look_limit(float *angle, float limit)
{
    /* MARK: __builtin_fabsf required -- emits inline abs.s, no call (S1: plain
     * fabsf() adds jal fabsf and mismatches). */
    if (__builtin_fabsf(*angle) < limit) {
        *angle = 0.0f;
        return;
    }
    if (limit <= *angle) {
        *angle -= limit;
    }
    if (*angle <= -limit) {
        *angle += limit;
    }
}

void look_limit_add(float *angle, float target, float step)
{
    /* MARK: __builtin_fabsf required -- emits inline abs.s, no call (S1). */
    float delta = *angle - target;

    if (__builtin_fabsf(delta) < step) {
        *angle = target;
        return;
    }
    if (step <= delta) {
        *angle -= step;
    }
    delta = *angle - target;
    if (delta <= -step) {
        *angle += step;
    }
}

typedef struct LookVector {
    float x;
    float y;
    float z;
    float w;
} LookVector;
typedef struct LookPlaceSource {
    unsigned long long lo;
    unsigned long long hi;
} LookPlaceSource;
typedef struct LookEnemyLink {
    unsigned char unmodeled_00[0xC];
    LookPlaceSource *place;
} LookEnemyLink;
typedef struct LookEnemyWork {
    unsigned char unmodeled_0000[0x3860];
    LookEnemyLink link;
    unsigned char unmodeled_3870[0x38B0 - 0x3870];
} LookEnemyWork;
extern LookEnemyWork enepc[16];
typedef struct LookTargetInfo {
    unsigned char unmodeled_00[0x48];
    int kind;
} LookTargetInfo;
typedef struct LookActor {
    unsigned char unmodeled_000[0x70];
    LookVector upperPlace;
    unsigned char enemyNo;
    unsigned char unmodeled_081[0x90 - 0x81];
    LookVector childPlace;
    unsigned char unmodeled_0a0[0x620 - 0xA0];
    LookVector ownPlace;
    LookVector lowerPlace;
    unsigned char unmodeled_640[0x674 - 0x640];
    unsigned short lookMode;
    unsigned char unmodeled_676[0x67C - 0x676];
    struct LookActor *target;
    unsigned char unmodeled_680[0x8D0 - 0x680];
    LookTargetInfo *info;
} LookActor;
void *xglStudioGetActiveCamera(void);
static inline void look_copy_place(void *from, void *to)
{
    __asm__ __volatile__(
        "lq $2, 0(%0)\n\t"
        "sq $2, 0(%1)"
        :
        : "r"(from), "r"(to)
        : "$2", "memory");
}

static int look_get_place(LookActor *actor, LookVector *place)
{
    int result = 1;
    LookEnemyLink *link = 0;
    unsigned short mode = actor->lookMode;
    LookPlaceSource *source;
    LookActor *target;

    if (actor->enemyNo < 16) {
        link = &enepc[actor->enemyNo].link;
    }
    switch (mode) {
    case 1:
        if (link != 0) {
            source = link->place;
            if (source != 0) {
                result = 0;
                ((LookPlaceSource *)place)->lo = source->lo;
                ((LookPlaceSource *)place)->hi = source->hi;
            }
        }
        break;
    case 2:
        target = (LookActor *)((char *)xglStudioGetActiveCamera() + 0xD0);
        goto copy;
    case 3:
        target = (LookActor *)&actor->ownPlace;
        goto copy;
    case 4:
        target = actor->target;
        if (target != 0) {
            if (target->info == 0) {
                result = 0;
            } else if (target->info->kind >= 8) {
                look_copy_place(&target->lowerPlace, place);
                result = 0;
            } else {
                look_copy_place(&target->upperPlace, place);
                result = 0;
            }
        }
        break;
    case 5:
        target = actor->target;
        if (target != 0) {
            target = (LookActor *)&target->childPlace;
            goto copy;
        }
        break;
    copy:
        look_copy_place(target, place);
        result = 0;
        break;
    }
    return result;
}

void LOOK_target_init(void)
{
}

void LOOK_target_doit(void)
{
}

INCLUDE_ASM("asm/main/nonmatchings/look", LOOK_sebo_cont);

#include "shared.h"
typedef struct LookJointElement {
    float scale[3];
    unsigned char unmodeled_0c[4];
    float rotation[3];
    unsigned char unmodeled_1c[4];
    float translation[3];
    unsigned char unmodeled_2c[0x40 - 0x2c];
} LookJointElement;
typedef struct LookJointWork {
    unsigned char unmodeled_000[0x2c];
    int jointIndex;
    unsigned char unmodeled_030[0x720 - 0x30];
    Matrix4 rootMatrix;
    unsigned char unmodeled_760[0x7a0 - 0x760];
    Matrix4 *matrixBuffer;
    unsigned char unmodeled_7a4[0x818 - 0x7a4];
    LookJointElement *elements;
} LookJointWork;
typedef struct LookEyeState {
    int frame;
    int timer;
    float yaw;
    float pitch;
} LookEyeState;
typedef struct LookCharacter {
    unsigned char unmodeled_000[0x70];
    Vector4 nearPosition;
    unsigned char number;
    unsigned char unmodeled_081[2];
    unsigned char headPositionValid;
    unsigned char unmodeled_084[0x90 - 0x84];
    Vector4 eyePosition;
    unsigned char unmodeled_0a0[0x600 - 0xa0];
    Vector4 lookAngles[2];
    Vector4 worldPosition;
    Vector4 headPosition;
    LookEyeState eyes[2];
    float eyeYaw;
    float eyePitch;
    unsigned char unmodeled_668[4];
    float lookWeight;
    unsigned char unmodeled_670[4];
    unsigned short lookMode;
    unsigned char unmodeled_676[2];
    unsigned int eyeMode;
    struct LookCharacter *lookTarget;
    unsigned char unmodeled_680[0x8d0 - 0x680];
    struct LookModelRecord *model;
    unsigned char unmodeled_8d4[0x8fc - 0x8d4];
    struct LookCharacter *eyeOwner;
    unsigned char unmodeled_900[0x9a0 - 0x900];
    unsigned int headFlags;
} LookCharacter;
typedef struct LookModelRecord {
    unsigned char unmodeled_00[0x11];
    char name[0x48 - 0x11];
    int detail;
} LookModelRecord;
typedef union LookAlignedVector4 {
    double alignment;
    Vector4 v;
} LookAlignedVector4;
void xglMatrixMul(Matrix4 destination, Matrix4 left, Matrix4 right);
void xglMatrixInverse(Matrix4 destination, Matrix4 source);
void xglVectorMulMat(Vector4 *destination, Matrix4 matrix, const Vector4 *source);
float xglAtan2(float x, float y);
#define D_004D8578 0.0001f
#define D_004D857C 0.15f
#define D_004D8580 1.0471975803375244f
#define D_004D8584 (-1.0471975803375244f)
#define D_004D8588 1.483529806137085f
#define D_004D858C (-1.483529806137085f)
#define D_004D8590 (-0.7f)
#define D_004D8594 0.7f
#define D_004D8598 0.1745329350233078f
#define D_004D859C 0.0872664675116539f
#define D_004D85A0 0.1745329350233078f
#define D_004D85A4 0.0872664675116539f
void CUR_MATRIX_Set(Matrix4 matrix);
void CUR_MATRIX_Txyz4s(const float *displacement);
void CUR_MATRIX_Rzyx4s(const float *angles);
void CUR_MATRIX_Get(Matrix4 matrix);
void MATRIX_convert4(Matrix4 destination, Matrix4 source);
void xglMatrixRotX(Matrix4 destination, Matrix4 source, float angle);
void xglMatrixRotY(Matrix4 destination, Matrix4 source, float angle);
float xglAsin(float value);

void *LOOK_head_cont(LookJointWork *work, void *channels, LookCharacter *actor)
{
    Matrix4 matrix;
    Matrix4 rootMatrix;
    LookAlignedVector4 place;
    Matrix4 currentMatrix;
    Vector4 rotation;
    Vector4 localPlace;
    LookJointElement *element;
    float weight;
    float horizontalDistance;

    if (actor != 0) {
        weight = actor->lookWeight;
        actor->headFlags &= ~0x80U;
        element = &work->elements[work->jointIndex];
        if (!(__builtin_fabsf(weight) < D_004D8578)) {
            if (weight <= 0.0f)
                weight = 1.0f;
            CUR_MATRIX_Set(work->matrixBuffer[work->jointIndex - 1]);
            CUR_MATRIX_Txyz4s(element->translation);
            CUR_MATRIX_Rzyx4s(element->rotation);
            CUR_MATRIX_Get(currentMatrix);
            MATRIX_convert4(matrix, currentMatrix);
            MATRIX_convert4(rootMatrix, work->rootMatrix);
            xglMatrixMul(matrix, rootMatrix, matrix);
            /* The SDK copies the joint's translated row through one scratch register. */
            __asm__ __volatile__("lq $2, 0(%1)\n"
                                 "sq $2, 0(%0)\n"
                                 : : "r" (&actor->headPosition), "r" (matrix[3])
                                 : "$2", "memory");
            actor->headPositionValid = 1;
            actor->headPosition.y += D_004D857C;
            if (look_get_place((LookActor *)actor, (LookVector *)&place) == 0) {
                place.v.w = 1.0f;
                __asm__ __volatile__("sq $0, 0(%0)\n"
                                     : : "r" (&rotation) : "memory");
                xglMatrixInverse(matrix, matrix);
                xglVectorMulMat(&localPlace, matrix, &place.v);
                rotation.y = xglAtan2(localPlace.z, localPlace.x);
                horizontalDistance = __builtin_sqrtf(localPlace.z * localPlace.z +
                                                     localPlace.x * localPlace.x);
                rotation.z = xglAtan2(localPlace.y, horizontalDistance);
                if (rotation.z > 0.0f) {
                    if (rotation.z > D_004D8580)
                        rotation.z = D_004D8580;
                } else if (rotation.z < D_004D8584) {
                    rotation.z = D_004D8584;
                }
                if (rotation.y > 0.0f) {
                    if (rotation.y > D_004D8588)
                        rotation.y = D_004D8588;
                } else if (rotation.y < D_004D858C) {
                    rotation.y = D_004D858C;
                }
                rotation.y *= D_004D8590;
                rotation.z *= D_004D8594;
                look_limit_add(&actor->lookAngles[1].y, rotation.y, weight * D_004D8598);
                look_limit_add(&actor->lookAngles[1].z, rotation.z, weight * D_004D859C);
            } else {
                look_limit(&actor->lookAngles[1].y, weight * D_004D85A0);
                look_limit(&actor->lookAngles[1].z, weight * D_004D85A4);
            }
            xglMatrixUnit(matrix);
            xglMatrixRotX(matrix, matrix, actor->lookAngles[1].z);
            xglMatrixRotY(matrix, matrix, actor->lookAngles[1].y);
            element->rotation[0] += xglAtan2(matrix[1][2], matrix[2][2]);
            element->rotation[1] -= xglAsin(matrix[0][2]);
            element->rotation[2] += xglAtan2(matrix[0][1], matrix[0][0]);
        }
    }
    return channels;
}

int LOOK_eye_cont(char *model, int target, char *step, int side);
extern float s_fEyeOffset;

void LOOK_eyeL_cont(int model, int target, int step)
{
    s_fEyeOffset = D_004D85A8;
    LOOK_eye_cont(model, target, step, 0);
}

void LOOK_eyeR_cont(int model, int target, int step)
{
    s_fEyeOffset = D_004D85AC;
    LOOK_eye_cont(model, target, step, 1);
}

INCLUDE_ASM("asm/main/nonmatchings/look", LOOK_eye_cont);

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

INCLUDE_ASM("asm/main/nonmatchings/look", LOOK_head_cont);

void LOOK_eye_cont(int model, int target, int step, int side);
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

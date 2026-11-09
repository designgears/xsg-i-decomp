/*
 * OV01 original TU 21: 0x00a33648..0x00a337a0 (1 functions)
 */
#include "common.h"
#include "shared.h"
#include "m_ef_create.h"

extern int MEfCreate_MSP00();
extern int MEfCreate_BP00();
extern int MEfCreate_SMP01();
extern int MEfCreate_EAC00();
extern int MEfCreate_AMP02();
extern int MEfCreate_MSP02();
extern int MEfCreate_SOLB();
extern int MEfCreate_ECM01();
extern int MEfCreate_ECM02();
extern int MEfCreate_GAMERA();
extern int MEfCreate_EAD00();
extern int MEfCreate_SO14();
extern int MEfCreate_DORA();
extern int MEfCreate_KOSBW02();

int MEfCreate(MEfCreateParam *param)
{
    static MEfObjCtor func[MEF_TYPE_COUNT] = {
        MEfCreate_MSP00,
        MEfCreate_BP00,
        MEfCreate_SMP01,
        MEfCreate_EAC00,
        MEfCreate_AMP02,
        MEfCreate_MSP02,
        MEfCreate_SOLB,
        MEfCreate_ECM01,
        MEfCreate_ECM02,
        MEfCreate_GAMERA,
        MEfCreate_EAD00,
        MEfCreate_SO14,
        MEfCreate_DORA,
        MEfCreate_KOSBW02,
    };
    static short ntbl[MEF_TYPE_COUNT] = { 1, 2, 1, 1, 1, 1, 1, 1, 4, 6, 1, 1, 6, 1 };
    int i;
    MEfObjRecord *object;

    if ((u32)param->type >= MEF_TYPE_COUNT) {
        MOutputDebugStringWarn("MEfCreate: Invalid type (%d)", param->type);
        return 0;
    }

    for (i = 0; i < ntbl[param->type]; i++) {
        object = (MEfObjRecord *)MEfObjCreate();
        if (object == 0) {
            MOutputDebugStringWarn("MEfCreate: Couldn't create an Object");
            return 0;
        }

        object->work = *param;

        if (!func[param->type](object)) {
            MEfObjDestroy(object);
        }
    }

    return 1;
}

/* Unreferenced word following ntbl in the original .data. */
int D_00A4377C = 0;

/* GIF ST/Q texture coordinates of a quad's four corners (s, t, q, 0). */
const float McEfSTQtbl[4][4] __attribute__((aligned(16))) = {
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 1.0f, 1.0f, 0.0f },
    { 1.0f, 0.0f, 1.0f, 0.0f },
    { 1.0f, 1.0f, 1.0f, 0.0f },
};

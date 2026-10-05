/*
 * OV02 original TU 8: 0x00a04170..0x00a05738 (11 functions)
 */
#include "common.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "umn_procurator.h"

INCLUDE_ASM("asm/nonmatchings/ov02/umn_procurator", UmnManzaiControl);

int UmnManzaiInit(void)
{
    UmnManzaiWinTask *window;
    int index;

    if (UmnEventTextMake() < 0) {
        return -1;
    }
    UmnManzaiText = UmnEventTextNextGet(0);
    if (UmnModelSon == 0) {
        MenuModelCreate(&UmnModelSon, 3);
    }
    MenuModelExtFuncSet(UmnModelUkn, UmnProcuratorManzai, 0);
    MenuModelExtFuncSet(UmnModelSon, UmnProcuratorManzai2, 0);
    index = 0;
    window = UmnManzaiWin;
    do {
        UmnObjectTaskCreate(tskUmnManzaiWin, window);

        {
            int next_index = index + 1;
            window->task_index = next_index;
            index = next_index;
        }
        /* Advance this loop to the next task record. */
        window++;
    } while (index < 2);
    UmnManzaiFlag = 6;
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov02/umn_procurator", tskUmnManzaiWin);

extern void xglLightIntensityAmbient(void *light, const Vector4 *intensity);
extern void xglLightIntensityParallel(void *light, unsigned int index, const Vector4 *intensity);
extern void xglLightDirection(void *light, unsigned int index, const Vector4 *direction);
typedef union LocalLightVector {
    double alignment; /* 8-byte alignment of the original block copy */
    Vector4 value;
} LocalLightVector;
typedef struct LocalLightTable {
    LocalLightVector ambient;
    LocalLightVector color[3];
    LocalLightVector direction[3];
} LocalLightTable;
extern const LocalLightTable D_00A120F0;

static void LocalLightSet(unsigned int *model)
{
    LocalLightTable table;
    void *light;

    table = D_00A120F0;
    light = (char *)model + 0x510;
    xglLightIntensityAmbient(light, &table.ambient.value);
    xglLightIntensityParallel(light, 0, &table.color[0].value);
    xglLightDirection(light, 0, &table.direction[0].value);
    xglLightIntensityParallel(light, 1, &table.color[1].value);
    xglLightDirection(light, 1, &table.direction[1].value);
    xglLightIntensityParallel(light, 2, &table.color[2].value);
    xglLightDirection(light, 2, &table.direction[2].value);
    *model |= 0x8000;
}

INCLUDE_ASM("asm/nonmatchings/ov02/umn_procurator", UmnProcuratorManzai2);

static void Rotation(float *out, float *position, float *base, float *rotation)
{
    float matrix[16];

    xglMatrixStackUnit();
    xglMatrixStackRotZ(rotation[2]);
    xglMatrixStackRotY(rotation[1]);
    xglMatrixStackRotX(rotation[0]);
    xglMatrixStackTrans(position);
    xglMatrixStackSave((float (*)[4])matrix);

    out[0] = base[0] + matrix[12];
    out[1] = base[1] + matrix[13];
    out[2] = base[2] + matrix[14];
}

INCLUDE_ASM("asm/nonmatchings/ov02/umn_procurator", UmnProcuratorManzai);

INCLUDE_ASM("asm/nonmatchings/ov02/umn_procurator", UmnProcuratorMail);

typedef void (*LLS2)(unsigned int *, int);

void UmnProcuratorTop(void *task)
{
    unsigned char *t = (unsigned char *)task;
    unsigned int *model;
    float *a;
    float *b;

    if (t[0x11] != 0) {
        model = *(unsigned int **)(t + 0x20);
        a = (float *)((char *)model + 0xC0);
        b = (float *)((char *)model + 0xD0);
        switch (t[0x13]) {
        case 0:
            a[0] = 1.0f;
            a[1] = -0.3f;
            b[0] = 0.0f;
            b[1] = -0.7853982f;
            ((LLS2)LocalLightSet)(model, 0);
            t[0x13] = 1;
        case 1:
            MenuModelUnitOpen(task, 2);
            t[0x13] = 2;
            break;
        case 2:
            break;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ov02/umn_procurator", UmnProcuratorGoodBy);

INCLUDE_ASM("asm/nonmatchings/ov02/umn_procurator", UmnProcuratorSet);

/*
 * OV02 original TU 8: 0x00a04170..0x00a05738 (11 functions)
 */
#include "common.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "umn_procurator.h"

extern PadPrefix PadData;
extern int eMessageNextPage(void *message, int arg);
extern void xglSoundEffectNormalID(int sound_id, int variant);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern char D_00A12060[];

int UmnManzaiControl(void)
{
    unsigned int flag;
    unsigned int half;
    short *next;

    if (!(UmnManzaiFlag & 0x40)) {
        if ((UmnManzaiFlag & 0x30) == 0x30) {
            flag = UmnManzaiFlag;
            half = flag & ~1;
            UmnManzaiFlag = half & ~8;
            if ((flag & 0x206) == 0)
            if (PadData.half_2a == 0x20) {
                UmnManzaiFlag |= 1;
                if (eMessageNextPage(UmnWork + 0x7C + *UmnManzaiText * 0xA4, 0) == 0) {
                    next = UmnEventTextNextGet(1);
                    xglSoundEffectNormalID(1, 0);
                    if (next == 0) {
                        UmnManzaiFlag |= 0x40;
                    } else {
                        UmnManzaiText = next;
                        UmnManzaiFlag |= 8;
                    }
                }
            }
        }
    } else if (!(UmnManzaiFlag & 0x30)) {
        return 0;
    }
    xglFontDebugPrintf(0x80, 0x40, D_00A12060, UmnManzaiFlag);
    return 1;
}

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

void UmnProcuratorGoodBy(void *task)
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
            t[0x13] = 1;
            ((LLS2)LocalLightSet)(model, 0);
            MenuModelUnitOpen(task, 3);
        case 1:
            a[0] += 0.4f;
            b[1] += 0.4f;
            if (a[0x18] == 0.0f) {
                t[0x10] = -1;
            }
            break;
        }
    }
}

static inline void UmnProcuratorSetGoodBy(void)
{
    MenuModelExtFuncSet(UmnModelUkn, UmnProcuratorGoodBy, 0);
}

void UmnProcuratorSet(void)
{
    switch (UmnWork[1]) {
    case 0:
        if (UmnModelUkn == 0) {
            MenuModelCreate(&UmnModelUkn, 0x1D);
        }
        MenuModelExtFuncSet(UmnModelUkn, UmnProcuratorTop, 0);
        break;
    case 1:
        if (UmnModelUkn == 0) {
            MenuModelCreate(&UmnModelUkn, 0x1D);
        }
        MenuModelExtFuncSet(UmnModelUkn, UmnProcuratorMail, 0);
        break;
    default:
        UmnProcuratorSetGoodBy();
        break;
    }
}

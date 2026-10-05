/*
 * OV10 original TU 0: 0x00a00000..0x00a00410 (4 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov10/cgp.h"

extern int s_nDebMode;
extern int s_nMode;
extern CardGameWork D_00A4FA10;
extern char D_00A4BBA0[];
extern char D_00A4BBB0[];
extern void xglRenderClearFrame(void);
extern void xglRenderGlobalFadeInit(void);
extern void xglSleep(void);
extern void xglSoundLoadEffect(const char *bank_name, void *buffer, int mode);
extern int CardMainInit(CardGameWork *work);
extern int CardMainProc(CardGameWork *work);
extern int printf(const char *format, ...);

typedef struct {
    float pos[4];
    float nrm[4];
} CvVert;

void CalcVertOutVecEdge(CvVert *vtx, int num, unsigned short *edge)
{
    CvVert *v = vtx;
    int i;
    int n;
    int k;
    CvVert *o;
    float d[3];
    float len;

    if (num > 0) {
        i = num;
        do {
            k = *edge++;
            v->nrm[0] = v->nrm[1] = v->nrm[2] = 0.0f;
            if (k != 0) {
            n = k;
            do {
                o = (CvVert *)((*edge++ << 5) + (int)vtx);
                d[0] = v->pos[0] - o->pos[0];
                d[1] = v->pos[1] - o->pos[1];
                d[2] = v->pos[2] - o->pos[2];
                len = __builtin_sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                v->nrm[0] += d[0] / len;
                v->nrm[1] += d[1] / len;
                v->nrm[2] += d[2] / len;
            } while (--n != 0);
            }
            len = __builtin_sqrtf(v->nrm[0] * v->nrm[0] + v->nrm[1] * v->nrm[1] + v->nrm[2] * v->nrm[2]);
            v->nrm[0] /= len;
            v->nrm[1] /= len;
            v->nrm[2] /= len;
            v++;
        } while (--i != 0);
    }
}

typedef struct {
    int pad[8];
    int r, g, b, a;
} YClearEnv;
typedef struct {
    int unk0;
    int unk4;
    int pad[2];
    float mtx[4][4];
} YModel;
typedef struct StudioLight StudioLight;
extern YClearEnv ClearEnv;
extern YModel s_aModel[10];
extern float s_fvInt[4];
extern float s_fvPos[4];
extern float s_inMulCol[4];
extern float s_inMulColAdd[4];
extern int s_nPause_00A42038;
extern void xglStudioInit(void);
extern void xglStudioChange(int studio_index);
extern void xglStudioGetLight(StudioLight **light_out);
extern void xglLightSetDefault(StudioLight *light);
extern void xglStudioMainCameraInit(void);
extern void xglMatrixUnit(float matrix[4][4]);

void yNewRenderInit(void)
{
    StudioLight *light;
    int i;
    int off;
    char *base;
    char *mbase;
    YModel *p;

    ClearEnv.a = 0x80;
    ClearEnv.g = 0;
    ClearEnv.b = 0;
    ClearEnv.r = 0;
    xglStudioInit();
    xglStudioChange(0);
    xglStudioGetLight(&light);
    xglLightSetDefault(light);
    s_nPause_00A42038 = 0;
    s_nDebMode = 0;
    s_inMulCol[0] = 1.0f;
    s_inMulCol[1] = 1.0f;
    s_inMulCol[2] = 1.0f;
    s_inMulCol[3] = 1.0f;
    s_inMulColAdd[0] = 0.01f;
    s_inMulColAdd[1] = 0.015f;
    s_inMulColAdd[2] = 0.02f;
    s_inMulColAdd[3] = 0;
    s_fvPos[0] = 0;
    s_fvPos[1] = 0;
    s_fvPos[2] = 5.8f;
    s_fvPos[3] = 1.0f;
    s_fvInt[0] = 0;
    s_fvInt[1] = 0;
    s_fvInt[2] = 0;
    s_fvInt[3] = 1.0f;
    xglStudioMainCameraInit();
    base = (char *)s_aModel;
    mbase = base + 16;
    off = 0;
    i = 9;
    do {
        p = (YModel *)(off + (int)base);
        p->unk0 = 0;
        p->unk4 = 0;
        xglMatrixUnit((float (*)[4])(off + (int)mbase));
        off += 0x50;
    } while (--i >= 0);
}

extern void yNewRenderInit(void);

void CardGameRoot(void)
{
    xglRenderClearFrame();
    s_nMode = 1;
    yNewRenderInit();
    s_nMode = 1;
    s_nDebMode = 0;
    xglRenderGlobalFadeInit();
    xglSoundLoadEffect(D_00A4BBA0, (void *)0x01B00000, 2);
    if (CardMainInit(&D_00A4FA10) < 0)
        printf(D_00A4BBB0);

    while (CardMainProc(&D_00A4FA10))
        xglSleep();

    xglSoundLoadEffect(0, 0, 1);
    xglSoundLoadEffect(0, 0, 2);
    xglRenderGlobalFadeInit();
}

void CardGameRoot(void);

void YmineTest(void) {
    CardGameRoot();
}

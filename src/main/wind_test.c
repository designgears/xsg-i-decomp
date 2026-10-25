#include "common.h"
#include "shared.h"

typedef struct { float x, y, z, w; } __attribute__((aligned(16))) WtQ;
typedef struct { char pad[0x10]; WtQ m10; char pad20[0x30]; WtQ m50; WtQ m60; } WtAct;
typedef struct { char pad[0xD0]; float d0, d4, d8; } WtCam;
extern void GameResourceInit(unsigned int, int);
extern void xglStudioInit(void);
extern void xglStudioChange(int);
extern void xglStudioMainCameraInit(void);
extern void xglStudioGetCamera(WtCam **, int);
extern void ppInit(void *);
extern void ppSetPos(void *, float, float, float);
extern void ACT_init(void);
extern void ACT_resourceInit(void);
extern WtAct *ACT_create(int, int);
extern void ACT_initMotion(WtAct *);
extern void ACT_loadMotion(WtAct *, int, int);
extern void ACT_loadResource(WtAct *, int);
extern void ACT_allocMatrix(WtAct *, int);
extern void ACT_setModelWrapper(WtAct *, int);
extern void EXM_SetPointWind(void *);
extern void EXM_SetShakeTime(float);
extern WtCam *pCamera_004DC614;
extern float cpos_00585140[];
extern float wpos[];
extern float wvec[];
extern float crx_004DC618;
extern float cry_004DC61C;
extern float crvx;
extern float crvy;
extern float wpow;
extern float shake;
extern signed char wtype;
extern void *pAct_004DC62C;
extern float D_004D81F4;
extern float D_004D81F8;
extern const WtQ D_004CBB60;
extern const WtQ D_004CBB70;
extern void *memset(void *, int, unsigned int);

static int InitTest(void)
{
    float k;

    GameResourceInit(0x07000000U, 0x06000000);
    xglStudioInit();
    xglStudioChange(0);
    xglStudioMainCameraInit();
    xglStudioGetCamera(&pCamera_004DC614, 0);
    pCamera_004DC614->d4 = 2.0f;
    pCamera_004DC614->d8 = 2.0f;
    pCamera_004DC614->d0 = 0.0f;
    ppInit(cpos_00585140);
    ppSetPos(cpos_00585140, 0.0f, 1.0f, 3.0f);
    cpos_00585140[12] = 0.0f;
    crx_004DC618 = 0.0f;
    cry_004DC61C = 0.0f;
    crvx = 0.0f;
    crvy = 0.0f;
    ACT_init();
    ACT_resourceInit();
    pAct_004DC62C = ACT_create(-1, 2);
    k = 0.1f;
    ACT_initMotion(pAct_004DC62C);
    ACT_loadMotion(pAct_004DC62C, 2, 1);
    ACT_loadResource(pAct_004DC62C, 2);
    ACT_allocMatrix(pAct_004DC62C, -1);
    ACT_setModelWrapper(pAct_004DC62C, 0);
    {
    typedef struct { signed char value; } TypeByte;
    TypeByte t;
    WtQ v0;
    WtQ v1;
    WtQ v2;
    memset(&v0, 0, 16);
    v1 = *(const WtQ *)&D_004CBB60;
    t.value = 2;
    v2 = *(const WtQ *)&D_004CBB70;
    v0.w = 1.0f;
    *(TypeByte *)&wtype = t;
    *(WtQ *)((char *)pAct_004DC62C + 0x10) = v0;
    *(WtQ *)((char *)pAct_004DC62C + 0x50) = v1;
    *(WtQ *)((char *)pAct_004DC62C + 0x60) = v2;
    }
    ppInit(wpos);
    wpos[12] = 0.0f;
    ppSetPos(wpos, 0.5f, 1.0f, 0.0f);
    wpos[3] = k;
    shake = k;
    EXM_SetPointWind(wpos);
    ppInit(wvec);
    wvec[12] = 0.0f;
    ppSetPos(wvec, D_004D81F8, 0.0f, 0.0f);
    wpos[3] = 1.0f;
    wpow = k;
    EXM_SetShakeTime(0.5f);
    return 1;
}

typedef struct {
    char pad0[0x28];
    unsigned short trg;
    unsigned short lvl;
    char pad2c[0x64 - 0x2C];
    signed char rx;
    signed char ry;
    signed char lx;
    signed char ly;
    char pad68[0xD0 - 0x68];
} WindPad;

extern WindPad PadData;

extern float cry_004DC61C;

extern double cos(double);

extern double sin(double);

extern void ppNextStart(void *);

extern void ppNextEnd(void *);

typedef struct WindParticle {
    Vector4 position;
    Vector4 previous_position;
    Vector4 velocity;
    float gravity;
    float damping;
    unsigned char unmodeled_38[8];
} WindParticle;
typedef struct WindPadRecord {
    u8 unmodeled_00[0x28];
    union {
        u64 packed;
        struct {
            u16 held;
            u16 pressed;
            u16 repeat;
            u16 released;
        } input;
    } buttons;
    u8 unmodeled_30[0x64 - 0x30];
    signed char axis[4];
} WindPadRecord;
typedef union WindVectorBlock {
    Vector4 vector;
    u64 words[2];
} WindVectorBlock;
extern WindPadRecord PadDataRec[2] __asm__("PadData");
extern WindParticle cpos __asm__("cpos_00585140");
extern float crx __asm__("crx_004DC618");
extern float cry __asm__("cry_004DC61C");
extern StudioCamera *pCamera __asm__("pCamera_004DC614");
extern float crvx;
extern float crvy;
extern double fptodp(float value);
extern float atan2f(float y, float x);

static void MoveCamera(void)
{
    float lookX;
    float lookZ;
    float lookHeight;
    float moveX;
    float moveZ;
    double position;
    double horizontalMove;
    double verticalMove;
    double rotatedMove;

    if (PadDataRec[0].buttons.input.held & 2) {
        crvx -= 0.01f;
    }
    if (PadDataRec[0].buttons.input.held & 8) {
        crvx += 0.01f;
    }
    crvy -= (float)PadDataRec[0].axis[0] * 0.0001f;
    crvx += (float)PadDataRec[0].axis[1] * 0.0001f;
    cry += crvy;
    if (cry < 0.0f) {
        cry += 6.2831855f;
    } else if (cry > 6.2831855f) {
        cry -= 6.2831855f;
    }
    crx += crvx;
    if (crx < 0.0f) {
        crx += 6.2831855f;
    } else if (crx > 6.2831855f) {
        crx -= 6.2831855f;
    }
    crvy *= 0.8f;
    crvx *= 0.8f;
    if ((PadDataRec[0].buttons.input.pressed & 0x400) != 0 ||
        ((PadDataRec[0].buttons.input.held & 1) != 0 &&
         (PadDataRec[0].buttons.input.held & 2) != 0)) {
        crx = crvx = 0.0f;
    }
    if ((PadDataRec[0].buttons.input.held & 0x40) != 0) {
        lookX = -cpos.position.x;
        lookZ = -cpos.position.z;
        lookHeight = 1.0f - cpos.position.y;
        cry = -atan2f(lookX, cpos.position.z);
        crx = atan2f(lookHeight, __builtin_sqrtf(lookX * lookX + lookZ * lookZ));
        crvx = crvy = 0.0f;
    }
    ppNextStart(&cpos);
    if (PadDataRec[0].buttons.input.held & 1) {
        cpos.position.y -= 0.01f;
    }
    if (PadDataRec[0].buttons.input.held & 4) {
        cpos.position.y += 0.01f;
    }
    moveX = (float)PadDataRec[0].axis[2] * 0.0001f;
    moveZ = (float)PadDataRec[0].axis[3] * 0.0001f;
    position = fptodp(cpos.position.x);
    horizontalMove = moveX;
    rotatedMove = horizontalMove * cos(-cry);
    verticalMove = moveZ;
    cpos.position.x = position + (rotatedMove - verticalMove * sin(-cry));
    position = cpos.position.z;
    rotatedMove = verticalMove * cos(-cry);
    cpos.position.z = position + (rotatedMove + horizontalMove * sin(-cry));
    ppNextEnd(&cpos);
    *(WindVectorBlock *)&pCamera->position = *(WindVectorBlock *)&cpos.position;
    pCamera->rotation.x = crx;
    pCamera->rotation.y = cry;
    pCamera->rotation.z = 0.0f;
}

extern float wpos[];
extern float D_004D8228;
extern float D_004D822C;
extern volatile float D_004D8230;
static void MoveWind(void)
{
    float x;
    float y;
    float scale;

    ppNextStart(wpos);
    scale = D_004D8228;
    if (PadData.trg & 1) {
        wpos[1] -= D_004D822C;
    }
    if (PadData.trg & 4) {
        wpos[1] += D_004D8230;
    }
    x = PadData.lx * scale;
    y = PadData.ly * scale;
    {
        double p = wpos[0];
        double dx = x;
        double a = dx * cos(-cry_004DC61C);
        double dy = y;
        wpos[0] = p + (a - dy * sin(-cry_004DC61C));
        p = wpos[2];
        a = dy * cos(-cry_004DC61C);
        wpos[2] = p + (a + dx * sin(-cry_004DC61C));
    }
    ppNextEnd(wpos);
}

extern float wvec[];
extern float D_004D8234;
extern float D_004D8238;
extern volatile float D_004D823C;

static void MoveWind2(void)
{
    float x;
    float y;
    float scale;

    ppNextStart(wvec);
    scale = D_004D8234;
    if (PadData.trg & 1) {
        wvec[1] -= D_004D8238;
    }
    if (PadData.trg & 4) {
        wvec[1] += D_004D823C;
    }
    x = PadData.lx * scale;
    y = PadData.ly * scale;
    {
        double p = wvec[0];
        double dx = x;
        double a = dx * cos(-cry_004DC61C);
        double dy = y;
        wvec[0] = p + (a - dy * sin(-cry_004DC61C));
        p = wvec[2];
        a = dy * cos(-cry_004DC61C);
        wvec[2] = p + (a + dx * sin(-cry_004DC61C));
    }
    ppNextEnd(wvec);
}

extern signed char wtype;
extern float shake;
extern const char D_004CBB80[];
extern const char D_004CBB90[];
extern const char D_004CBBA0[];
extern const char D_004CBBB8[];
extern const char D_004DB9D0[];
extern float D_0058518C[];
extern double fptodp(float value);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

static void PrintDisp(void)
{
    const char *position_format;

    xglFontDebugPrintf(0, 0, D_004CBB80);
    if (wtype == 2) {
        xglFontDebugPrintf(8, 8, D_004DB9D0);
    } else {
        xglFontDebugPrintf(8, 8, D_004CBB90);
    }
    position_format = D_004CBBA0;
    xglFontDebugPrintf(0x64, 0xC8, position_format, fptodp(D_0058518C[0]));
    xglFontDebugPrintf(0x64, 0xD0, D_004CBBB8, fptodp(shake));
}

typedef struct WindParticleMc {
    Vector4 position;
    Vector4 previous_position;
    Vector4 velocity;
    float gravity;
    float damping;
    unsigned char unmodeled_38[8];
} WindParticleMc;
typedef struct WindPadRecordMc {
    unsigned char unmodeled_00[0x28];
    union {
        unsigned long long packed;
        struct {
            unsigned short held;
            unsigned short pressed;
            unsigned short repeat;
            unsigned short released;
        } input;
    } buttons;
    unsigned char unmodeled_30[0x64 - 0x30];
    signed char axis[4];
} WindPadRecordMc;
typedef struct WindTestDirectionalRecordMc {
    unsigned long long payload_words[2];
    unsigned int zero_10;
    float y_bias_14;
    unsigned int zero_18;
    float scalar_1c;
    float direction_20[3];
} WindTestDirectionalRecordMc;
extern void *pAct_004DC62C;
extern const unsigned long long D_004CBBD0[2];
extern int InitTest(void);
extern void EXM_StepShakeWind(void);
extern void EXM_SetPointWind(void *);
extern void EXM_SetDirectionalWind(void *);
extern void EXM_SetShakePower(float);
extern void EXM_InitWind(void);
extern void ACT_setMotion(void *, unsigned int);
extern void ACT_updateMotion(void *);
extern void ACT_modelDraw(void *);
extern void MoveCamera(void);
extern void MoveWind(void);
extern void MoveWind2(void);
extern void PrintDisp(void);
extern void xglRenderClearFrame(void);
extern void xglSleep(void);
#define PAD0 (*(WindPadRecordMc *)&PadData)
#define WPOS (*(WindParticleMc *)wpos)
#define WVEC (*(WindParticleMc *)wvec)
#define WindTestDirectionalRecord WindTestDirectionalRecordMc

void WindTest(void)
{
    const float minimum = 0.0f;
    const float step = 0.005f;
    const float wind_scale = 10.0f;
    const float directional_y_bias = 1.8f;

    xglRenderClearFrame();
    xglSleep();

    if (InitTest() != 0) {
        if ((PAD0.buttons.packed & 0x08000100ULL) != 0x08000100ULL) {
            do {
                WindTestDirectionalRecord directional_record;
                if ((PAD0.buttons.input.pressed & 0x0100) != 0) {
                    if (wtype == 2) {
                        wtype = 1;
                    } else {
                        wtype = 2;
                    }
                }

                EXM_StepShakeWind();

                if ((PAD0.buttons.input.held & 0x0080) != 0) {
                    if (wtype == 2) {
                        MoveWind();
                    } else {
                        MoveWind2();
                    }
                } else {
                    MoveCamera();
                }

                if ((PAD0.buttons.input.repeat & 0x1000) != 0) {
                    WPOS.position.w += step;
                } else if ((PAD0.buttons.input.repeat & 0x4000) != 0) {
                    WPOS.position.w -= step;
                    if (WPOS.position.w < minimum) {
                        WPOS.position.w = minimum;
                    }
                }

                if ((PAD0.buttons.input.repeat & 0x2000) != 0) {
                    shake += step;
                } else if ((PAD0.buttons.input.repeat & 0x8000) != 0) {
                    shake -= step;
                    if (shake < minimum) {
                        shake = minimum;
                    }
                }

                if (wtype == 2) {
                    EXM_SetPointWind(&WPOS.position);
                } else {
                    EXM_SetDirectionalWind(&WVEC.position);
                    directional_record.payload_words[0] = D_004CBBD0[0];
                    directional_record.payload_words[1] = D_004CBBD0[1];
                    directional_record.zero_10 = 0;
                    directional_record.y_bias_14 = directional_y_bias;
                    directional_record.zero_18 = 0;
                    directional_record.scalar_1c = 0.1f;
                    directional_record.direction_20[0] =
                        WVEC.position.x * wind_scale + minimum;
                    directional_record.direction_20[1] =
                        WVEC.position.y * wind_scale + directional_y_bias;
                    directional_record.direction_20[2] =
                        WVEC.position.z * wind_scale + minimum;
                }

                EXM_SetShakePower(shake);
                ACT_setMotion(pAct_004DC62C, 0);
                ACT_updateMotion(pAct_004DC62C);
                ACT_modelDraw(pAct_004DC62C);
                PrintDisp();
                xglSleep();
            } while ((PAD0.buttons.packed & 0x08000100ULL) != 0x08000100ULL);
        }

        EXM_InitWind();
    }
}

#include "common.h"
#include "shared.h"

INCLUDE_ASM("asm/main/nonmatchings/wind_test", InitTest_002D9430);

INCLUDE_ASM("asm/main/nonmatchings/wind_test", MoveCamera);

typedef struct {
    char pad0[0x28];
    unsigned short trg;
    char pad2a[0x66 - 0x2A];
    signed char lx;
    signed char ly;
    char pad68[0xD0 - 0x68];
} WindPad;
extern WindPad PadData;
extern float wpos[];
extern float D_004D8228;
extern float D_004D822C;
extern volatile float D_004D8230;
extern float cry_004DC61C;
extern double cos(double);
extern double sin(double);
extern void ppNextStart(void *);
extern void ppNextEnd(void *);

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

INCLUDE_ASM("asm/main/nonmatchings/wind_test", MoveWind2);

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

INCLUDE_ASM("asm/main/nonmatchings/wind_test", WindTest);

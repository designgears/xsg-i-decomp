/*
 * OV12 original TU 85: 0x00a49f70..0x00a4a620 (2 functions)
 */
#include "common.h"
#include "init_xrg_input.h"

void InitXrgInput(XrgInput *pInput, int padId)
{
    int i;

    pInput->padId = padId;
    for (i = 0; i < 2; i++) {
        pInput->angle[i] = 0.0f;
        pInput->magnitude[i] = 0.0f;
    }
    pInput->flags = 0;

    for (i = 0; i < 2; i++) {
        pInput->buttonTimer[i * 2] = 100000000.0f;
        pInput->buttonTimer[i * 2 + 1] = 100000000.0f;
    }

    pInput->pressFlags = 0;
    pInput->latchedFlags = 0;
    pInput->flags = 0;
}

typedef struct { u8 pad[8]; u16 held; u16 trig; u8 pad2[4]; } XrgPadBtn;
typedef struct { u8 pad[8]; signed char stick[4]; u8 pad2[4]; } XrgPadAna;
typedef struct XrgPadView {
    u8 pad_00[0x20];
    XrgPadBtn btn;
    XrgPadAna ana;
    u8 pad_40[0x28];
} XrgPadView;
extern XrgPadView PadData[];
extern float XrgNormalizeVector(RgVector destination, RgVector source);
extern float atan2f(float y, float x);

void XrgInputDeviceCheck(XrgInput *pInput, float frameTime)
{
    float x[2];
    float y[2];
    float mag[2];
    RgVector v;
    float mn[2];
    unsigned short trig;
    unsigned short held;
    float m;
    float c;
    float a, b;
    float t0, t1, t2, t3;
    int i, j;
    int id;
    int off;
    u8 *base;
    XrgPadBtn *btn;
    XrgPadAna *ana;

    id = pInput->padId;
    held = PadData[id].btn.held;
    trig = PadData[id].btn.trig;
    memset(x, 0, 8);
    memset(y, 0, 8);
    memset(mag, 0, 8);
    if (trig & 4) pInput->buttonTimer[0] = 0;
    if (trig & 1) pInput->buttonTimer[1] = 0;
    if (trig & 8) pInput->buttonTimer[2] = 0;
    if (trig & 2) pInput->buttonTimer[3] = 0;

    ana = (XrgPadAna *)((char *)(id * 0x68 + 0x30) + (int)PadData);
    x[0] = ana->stick[3] * 0.0078125f;
    y[0] = ana->stick[2] * 0.0078125f;
    x[1] = ana->stick[1] * 0.0078125f;
    y[1] = ana->stick[0] * 0.0078125f;
    if (held & 0x1000) {
        x[0] = -1.0f;
    } else if (held & 0x4000) {
        x[0] = 1.0f;
    }
    if (held & 0x8000) {
        y[0] = -1.0f;
    } else if (held & 0x2000) {
        y[0] = 1.0f;
    }

    for (i = 0; i < 2; i++) {
        v[0] = x[i];
        v[1] = y[i];
        v[2] = v[3] = 0.0f;
        m = XrgNormalizeVector(v, v);
        c = (1.0f < m) ? 1.0f : m;
        mag[i] = c;
        if (c < 0.3f) mag[i] = 0.0f;
        pInput->magnitude[i] = mag[i];
        pInput->angle[i] = atan2f(v[1], v[0]);
    }

    pInput->flags = 0;
    if (trig & 5) pInput->latchedFlags |= 1;
    if (trig & 0xA) pInput->latchedFlags |= 2;
    t0 = pInput->buttonTimer[0];
    t1 = pInput->buttonTimer[1];
    a = (t0 < t1) ? t0 : t1;
    mn[0] = a;
    t2 = pInput->buttonTimer[2];
    t3 = pInput->buttonTimer[3];
    b = (t2 < t3) ? t2 : t3;
    mn[1] = b;
    if (mn[0] < 0.2f && mn[1] < 0.2f) pInput->latchedFlags |= 4;

    if (held & 5) {
        if (pInput->latchedFlags & 1) pInput->flags |= 1;
    } else {
        pInput->latchedFlags &= ~1;
    }
    if (held & 0xA) {
        if (pInput->latchedFlags & 2) pInput->flags |= 2;
    } else {
        pInput->latchedFlags &= ~2;
    }
    if ((held & 0xA) && (held & 5)) {
        if (pInput->latchedFlags & 4) pInput->flags |= 4;
    } else {
        pInput->latchedFlags &= ~4;
    }
    if (pInput->flags & 4) {
        pInput->flags &= ~3;
        pInput->latchedFlags &= ~3;
    }
    if (0.7f < y[0] && y[1] < -0.7f) pInput->flags |= 0x20;
    if (y[0] < -0.8f && 0.8f < y[1]) pInput->flags |= 0x40;
    pInput->flags &= ~0x180;
    if (held & 0x600) pInput->flags |= 0x80;
    if ((held & 0x600) == 0x600) pInput->flags |= 0x100;
    pInput->flags &= ~0xE00;
    if (t0 < 0.1f && t1 < 0.1f) pInput->flags |= 0x200;
    if (t2 < 0.1f && t3 < 0.1f) pInput->flags |= 0x400;
    if ((pInput->flags & 0x600) == 0x600) pInput->flags = (pInput->flags & ~0x600) | 0x800;
    pInput->pressFlags |= pInput->flags & 0xE00;
    if (!(held & 5)) pInput->pressFlags &= ~0x200;
    if (!(held & 0xA)) pInput->pressFlags &= ~0x400;
    if (!(held & 0xF)) pInput->pressFlags &= ~0x800;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            pInput->buttonTimer[i * 2 + j] += frameTime;
        }
    }
}

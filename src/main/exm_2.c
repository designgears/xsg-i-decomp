#include "common.h"
#include "shared.h"

typedef struct EXM_WindState EXM_WindState;

extern EXM_WindState *wind;
extern float wave;
extern float waverad;
extern EXM_WindState _wind;

/* The +0x10 vector fields are named from the allocated reset and setter stores.
 * The u64 at +0x08 preserves the 8-byte alignment of the complete wind state. */

void EXM_ResetWind(void);

/*
 * EXM_GetWindPower/EXM_ResetWind/EXM_SetDirectionalWind/EXM_SetPointWind/
 * EXM_StepShakeWind (this unit, main/tu268) also read or write this struct:
 * +0x00 is the mode EXM_StopWind and EXM_ResetWind clear to 0,
 * EXM_SetDirectionalWind sets to 1 and EXM_SetPointWind sets to 2; +0x10 is a
 * direction/point vector (four floats: EXM_ResetWind zeroes +0x10/+0x14/+0x18/
 * +0x1c individually) that EXM_Set{Directional,Point}Wind load with a 16-byte
 * copy. None of those bytes are written by a function claimed here, so they
 * stay modeled only for their evidenced size and 8-byte alignment (`u64`,
 * not the attribute-qualified float type the ordinary-C admission refuses):
 * EXM_SetWindPara/EXM_GetWindPara move the whole 0x30-byte block in six ld/sd
 * pairs, which needs that alignment -- a struct whose largest member is only
 * 4-byte aligned instead compiles the same `*wind = *para;` assignment to
 * ldl/ldr/sdl/sdr.
 */
typedef struct EXM_WindVector {
    float x;
    float y;
    float z;
    float w;
} EXM_WindVector;

struct EXM_WindState {
    char mode;              /* +0x00, read signed (lb) by EXM_GetWindPower */
    u8 unmodeled_01[7];
    u64 unmodeled_08;
    EXM_WindVector direction; /* +0x10, four-component direction/position */
    float shake_power;      /* +0x20, EXM_SetShakePower */
    float shake_time;       /* +0x24, EXM_SetShakeTime */
    float shake_rad;        /* +0x28, EXM_SetShakeWind/EXM_GetShakeRad */
    u8 unmodeled_2c[4];
};

extern float D_004D8770;
extern float xglSin(float angle);
extern void xglVectorNormal(Vector4 *destination, const Vector4 *source);
extern void MATRIX_identity4s(float destination[4][4]);
extern void MATRIX_rotY4s(Matrix4 matrix, float angle);
extern void MATRIX_translate4s(Matrix4 matrix, float x, float y, float z);
typedef struct { u64 a, b; } EXM_Q;

void EXM_GetWindPower(float *out, char *obj, float *pos)
{
    union { Vector4 f; EXM_Q q; } u;
    Matrix4 m;
    float shake;
    float c;
    float s;
    float scale;
    char *p;
    Vector4 *vp;

    if (wind->mode == 0) goto plain;
    {
        wave = wave + D_004D8770;
        shake = xglSin(wind->shake_rad - wave) * wave;
        c = *(float *)(obj + 0x720);
        s = *(float *)(obj + 0x740);
        switch (wind->mode) {
        default:
        plain:
            out[0] = pos[0];
            return;
        case 1:
            scale = wind->direction.w * (shake * wind->shake_power + 1.0f);
            u.q = *(EXM_Q *)&wind->direction;
            vp = &u.f;
            u.f.x *= scale;
            u.f.y *= scale;
            u.f.z *= scale;
            break;
        case 2:
            u.f.x = *(float *)(obj + 0x750) + pos[0] * c + pos[2] * s - wind->direction.x;
            u.f.y = *(float *)(obj + 0x754) + pos[1] - wind->direction.y;
            u.f.z = *(float *)(obj + 0x758) + pos[2] * c - pos[0] * s - wind->direction.z;
            xglVectorNormal(&u.f, &u.f);
            vp = &u.f;
            scale = wind->direction.w * (shake * wind->shake_power + 1.0f);
            u.f.x *= scale;
            u.f.y *= scale;
            u.f.z *= scale;
            break;
        }
        p = *(char **)(obj + 0x820);
        {
            float x = vp->x * c - vp->z * s;
            float z = vp->z * c + vp->x * s;
            if (p != 0) {
                MATRIX_identity4s(m);
                MATRIX_translate4s(m, x, 0.0f, z);
                MATRIX_rotY4s(m, -*(float *)(p + 0x54));
                x = m[3][0];
                z = m[3][2];
            }
            out[0] = pos[0] + x;
            out[1] = pos[1] + vp->y;
            out[2] = pos[2] + z;
        }
    }
}

void EXM_ResetWind(void)
{
    float reset_value;
    wind->mode = 0;
    reset_value = 0.0f;
    wind->direction.w = reset_value;
    wind->direction.z = reset_value;
    wind->direction.y = reset_value;
    wind->direction.x = reset_value;
    wind->shake_power = reset_value;
    wind->shake_time = reset_value;
    wind->shake_rad = reset_value;
}

void EXM_SetWindPara(EXM_WindState *para)
{
    if (para != 0) {
        *wind = *para;
    }
}

void EXM_GetWindPara(EXM_WindState *para)
{
    if (para != 0) {
        *para = *wind;
    }
}

void EXM_InitWind(void)
{
    wind = &_wind;
    EXM_ResetWind();
}

void EXM_StopWind(void)
{
    wind->mode = 0;
    wind->shake_rad = 0.0f;
}

void EXM_SetShakePower(float power)
{
    wind->shake_power = power;
}

/* D_004D8774 = 6.2831855f (2*pi), the upper clamp for the shake phase. */
extern float D_004D8774;
/* D_004D8778 = -6.2831855f (-2*pi), the lower clamp for the shake phase. */
extern float D_004D8778;

void EXM_SetShakeTime(float time)
{
    if (time > D_004D8774) {
        time = D_004D8774;
    } else if (time < D_004D8778) {
        time = D_004D8778;
    }
    wind->shake_time = time;
}

void EXM_SetDirectionalWind(Vector4 *dir)
{
    EXM_WindState *w;

    w = wind;
    if (dir != 0) {
        *(Vector4 *)&w->direction = *dir;
    } else {
        __asm__ volatile("");
    }
    w->mode = 1;
}

void EXM_SetPointWind(Vector4 *pos)
{
    EXM_WindState *w;

    w = wind;
    if (pos != 0) {
        *(Vector4 *)&w->direction = *pos;
    } else {
        __asm__ volatile("");
    }
    w->mode = 2;
}

void EXM_SetWindStruct(EXM_WindState *state)
{
    wind = state;
}

void EXM_ClearWindStruct(void)
{
    wind = &_wind;
}

extern volatile float D_004D877C;
extern float D_004D8780;

void EXM_StepShakeWind(void) {
    float r;

    if (wind != 0) {
        float k;
        wind->shake_rad += wind->shake_time;
        k = D_004D877C;
        if (wind->shake_rad > k) {
            wind->shake_rad -= k;
        } else if (wind->shake_rad < 0.0f) {
            wind->shake_rad += k;
        }
    }
    r = waverad + 0.5f;
    waverad = r;
    if (r > D_004D8780) {
        waverad = r - D_004D8780;
    } else if (r < 0.0f) {
        waverad = r + D_004D8780;
    }
}

void EXM_SetShakeWind(float rad)
{
    wind->shake_rad = rad;
}

void EXM_ResetWave(void)
{
    wave = 0.0f;
}

float EXM_GetWaveRad(void)
{
    return waverad;
}

float EXM_GetShakeRad(void)
{
    return wind->shake_rad;
}

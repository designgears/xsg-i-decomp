#include "common.h"
#include "shared.h"
#include "xgl_light.h"

typedef unsigned int XglLightQuadword __attribute__((mode(TI)));

/* Light vectors occupy aligned quadwords consumed by the VU light matrix program. */
typedef struct {
    XglLightQuadword intensity;
    XglLightQuadword direction;
} XglPackedParallelLight;

typedef struct {
    XglLightQuadword ambientIntensity;
    XglPackedParallelLight parallel[3];
} XglPackedLightSet;

void xglLightIntensityAmbient(volatile XglPackedLightSet *lightSet, const XglLightQuadword *intensity);
void xglLightIntensityParallel(XglPackedLightSet *lightSet, unsigned int index, const XglLightQuadword *intensity);
void xglLightAngle(XglLightSet *lightSet, unsigned int index, const float *angle);

void xglLightSetDefault(XglPackedLightSet *lightSet)
{
    /* Ambient, then three parallel intensities (r, g, b, 1.0). */
    /* asIntensity.0 holds four float quadwords (ambient, then the three
     * parallel intensities); the code reads them as XglLightQuadword. */
    static float asIntensityData[4][4] __asm__("asIntensity.0") __attribute__((aligned(16))) = {
        { 0.25f, 0.25f, 0.25f, 1.0f },
        { 0.8f, 0.8f, 0.8f, 1.0f },
        { 0.5f, 0.5f, 0.5f, 1.0f },
        { 0.5f, 0.5f, 0.5f, 1.0f }
    };
    extern XglLightQuadword asIntensity_0[4] __asm__("asIntensity.0");
    static float asDirection_1[4][4] __asm__("asDirection.1") = {
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { -0.26195645f, 0.53120846f, -0.019844394f, 1.0f },
        { 2.3561945f, 1.2733574f, 3.1415927f, 1.0f },
        { 0.36123082f, 3.775898f, 0.028536133f, 1.0f }
    };
    int i;

    for (i = 0; i < 4; i++) {
        if (i == 0) {
            xglLightIntensityAmbient(lightSet, &asIntensity_0[0]);
        } else {
            xglLightIntensityParallel(lightSet, i - 1, &asIntensity_0[i]);
            xglLightAngle((XglLightSet *)lightSet, i - 1, asDirection_1[i]);
        }
    }
}

/* Volatile preserves the quadword update observed by the VU light calculation. */
void xglLightIntensityAmbient(volatile XglPackedLightSet *lightSet, const XglLightQuadword *intensity)
{
    lightSet->ambientIntensity = *intensity;
}

/* Volatile preserves the quadword update observed by the VU light calculation. */
void xglLightIntensityParallel(XglPackedLightSet *lightSet, unsigned int index, const XglLightQuadword *intensity)
{
    /* EE pointers are 32-bit addresses; the selected array entry stays aligned. */
    unsigned int lightSetAddress = (unsigned int)lightSet;
    if (index < 3U) {
        XglPackedParallelLight *parallelLight = (XglPackedParallelLight *)
            (index * sizeof(XglPackedParallelLight) + lightSetAddress +
             sizeof(lightSet->ambientIntensity));
        volatile XglLightQuadword *destination = &parallelLight->intensity;
        *destination = *intensity;
    }
}

extern void xglVectorNormal(Vector4 *destination, const Vector4 *source);

void xglLightDirection(XglLightSet *lightSet, unsigned int index, const Vector4 *source)
{
    if (index < 3U) {
        xglVectorNormal(&lightSet->parallel[index].direction, source);
    }
}

void xglLightAngle(XglLightSet *lightSet, unsigned int index, const float *angle)
{
    float m[4][4];

    if (index < 3U) {
        xglMatrixStackUnit();
        xglMatrixStackRotZ(angle[2]);
        xglMatrixStackRotY(angle[1]);
        xglMatrixStackRotX(angle[0]);
        xglMatrixStackSave(m);
        __asm__ __volatile__(
            "lq $2, 0(%1)\n"
            "sq $2, 0(%0)\n"
            : : "r"((index << 5) + (unsigned int)lightSet + 32), "r"(&m[2]) : "$2", "memory");
    }
}

/*
 * xglLightCalcMatrix (ee-vu-cop2): dispatches the VU0 macro-mode
 * Vu0CallLightCalcMatrix microprogram (src/main/vu0/Vu0MicroCode.dvp) on the
 * light set's seven quadwords (ambientIntensity and the three parallel
 * lights' intensity/direction pairs) and writes its two 4x4 output matrices
 * back at lightSet+0x70 and lightSet+0xB0: the first is the transpose of the
 * three light directions (each row padded with w=0, plus a trailing
 * (0,0,0,1) row) and the second has the three light intensities (w=0) and
 * ambientIntensity (w=1) as its columns -- see that file's own comment for
 * the per-lane derivation. Only the VU0 dispatch is inline assembly.
 */
extern char Vu0CallLightCalcMatrix[];

void xglLightCalcMatrix(XglLightSet *lightSet)
{
    __asm__ __volatile__(
        "ctc2.i %0,$vi27\n\t"
        "lqc2 vf4,0(%1)\n\t"
        "lqc2 vf5,16(%1)\n\t"
        "lqc2 vf6,32(%1)\n\t"
        "lqc2 vf7,48(%1)\n\t"
        "lqc2 vf8,64(%1)\n\t"
        "lqc2 vf9,80(%1)\n\t"
        "lqc2 vf10,96(%1)\n\t"
        "vcallmsr $vi27\n\t"
        "cfc2.i $0,$vi0\n\t"
        "sqc2 vf20,112(%1)\n\t"
        "sqc2 vf21,128(%1)\n\t"
        "sqc2 vf22,144(%1)\n\t"
        "sqc2 vf23,160(%1)\n\t"
        "sqc2 vf24,176(%1)\n\t"
        "sqc2 vf25,192(%1)\n\t"
        "sqc2 vf26,208(%1)\n\t"
        "sqc2 vf27,224(%1)\n\t"
        "nop"
        :
        : "r"((unsigned int)Vu0CallLightCalcMatrix / 8), "r"(lightSet)
        : "memory"
    );
}


void xglLightInit(XglLightSet *lightSet)
{
    static Vector4 sIntensity0_2 __asm__("sIntensity0.2") __attribute__((aligned(16))) = { 0.0f, 0.0f, 0.0f, 0.0f };
    static Vector4 sIntensity1_3 __asm__("sIntensity1.3") __attribute__((aligned(16))) = { 1.0f, 1.0f, 1.0f, 1.0f };
    int i;

    xglLightIntensityAmbient(lightSet, &sIntensity1_3);
    for (i = 0; i < 3; i++) {
        xglLightIntensityParallel(lightSet, i, &sIntensity0_2);
        xglLightDirection(lightSet, i, &sIntensity1_3);
    }
}

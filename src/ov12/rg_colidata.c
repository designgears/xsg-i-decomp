/*
 * OV12 original TU 62: 0x00a31970..0x00a32688 (14 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_colidata.h"

struct RgColiTriangle {
    RgVector m_vertices[3];
    RgVector m_plane;
    RgMatrix m_planeMatrix;
};

extern void XrgUnitMatrix(RgMatrix destination);
extern void XrgCopyVectorXYZ(RgVector destination, RgVector source);
extern void XrgCalcPlane(RgVector plane, RgVector p0, RgVector p1,
                         RgVector p2);
extern void XrgSubVector(RgVector destination, RgVector first,
                         RgVector second);
extern void XrgOuterVector(RgVector destination, RgVector first,
                           RgVector second);
extern float XrgNormalizeVector(RgVector destination, RgVector source);
extern float XrgInnerVector(RgVector first, RgVector second);
extern const char D_00A55548[];

static void _InitTriColi(void *pTri, void *pV0, void *pV1, void *pV2)
{
    RgVector edge;
    RgVector cross;
    RgVector facePlane;
    RgVector sidePlanes[3];
    int component;
    int plane;
    RgColiTriangle *triangle;

    triangle = pTri;
    XrgUnitMatrix(triangle->m_planeMatrix);
    XrgClearVector(triangle->m_vertices[0]);
    XrgClearVector(triangle->m_vertices[1]);
    XrgClearVector(triangle->m_vertices[2]);
    XrgCopyVectorXYZ(triangle->m_vertices[0], pV0);
    XrgCopyVectorXYZ(triangle->m_vertices[1], pV1);
    XrgCopyVectorXYZ(triangle->m_vertices[2], pV2);
    XrgCalcPlane(facePlane, pV0, pV1, pV2);
    XrgCopyVector(triangle->m_plane, facePlane);

    XrgSubVector(edge, pV1, pV0);
    XrgOuterVector(cross, facePlane, edge);
    XrgNormalizeVector(sidePlanes[0], cross);
    sidePlanes[0][3] = -XrgInnerVector(sidePlanes[0], pV0);

    XrgSubVector(edge, pV2, pV1);
    XrgOuterVector(cross, facePlane, edge);
    XrgNormalizeVector(sidePlanes[1], cross);
    sidePlanes[1][3] = -XrgInnerVector(sidePlanes[1], pV1);

    XrgSubVector(edge, pV0, pV2);
    XrgOuterVector(cross, facePlane, edge);
    XrgNormalizeVector(sidePlanes[2], cross);
    sidePlanes[2][3] = -XrgInnerVector(sidePlanes[2], pV2);

    for (plane = 0; plane < 3; plane++) {
        for (component = 0; component < 4; component++) {
            triangle->m_planeMatrix[component * 4 + plane] =
                sidePlanes[plane][component];
        }
    }
    for (plane = 0; plane < 4; plane++) {
        triangle->m_planeMatrix[plane * 4 + 3] = facePlane[plane];
    }
}

static void _AllocDataMemory(RgColiData *pData, int nCapa)
{
    if (pData == 0) {
        assert_prog(D_00A554F0, D_00A55500, 0xAC);
    }
    if (nCapa <= 0) {
        assert_prog(D_00A55518, D_00A55500, 0xAD);
    }
    pData->m_pTriangles = RgHeapAlloc(InstanceOfRgHeap(), nCapa << 7,
                                      D_00A55500, 0xAE);
    if (pData->m_pTriangles == 0) {
        assert_prog(D_00A55528, D_00A55500, 0xAF);
    }
    pData->m_nCapacity = nCapa;
    pData->m_nTriangles = 0;
}

extern void XrgAddVector(RgVector destination, RgVector first, RgVector second);
static inline void _ColiApply(float *dst, float *mtx, float *src)
{
    __asm__ __volatile__(
        "lqc2 $vf2,0(%2)\n\t"
        "lqc2 $vf3,0(%1)\n\t"
        "lqc2 $vf4,16(%1)\n\t"
        "lqc2 $vf5,32(%1)\n\t"
        "lqc2 $vf6,48(%1)\n\t"
        "vmulax.xyzw ACCxyzw,$vf3xyzw,$vf2x\n\t"
        "vmadday.xyzw ACCxyzw,$vf4xyzw,$vf2y\n\t"
        "vmaddaz.xyzw ACCxyzw,$vf5xyzw,$vf2z\n\t"
        "vmaddw.xyzw $vf2xyzw,$vf6xyzw,$vf2w\n\t"
        "sqc2 $vf2,0(%0)"
        : : "r"(dst), "r"(mtx), "r"(src) : "memory");
}

static int _CheckIntersectBall(RgColiData *pPoly, void *pArg, void *pResult)
{
    RgVector push;
    RgVector local;
    RgVector pos;
    float *sum;
    float *mtx;
    float radius;
    float d;
    int ret;
    int i;

    if (pPoly->m_pTriangles == 0 || pPoly->m_nTriangles <= 0) {
        return 0;
    }
    sum = (float *)pResult + 4;
    XrgClearVector(sum);
    *(int *)pResult = 0;
    for (i = 0; i < pPoly->m_nTriangles; i++) {
        mtx = pPoly->m_pTriangles[i].m_planeMatrix;
        radius = ((float *)pArg)[4];
        __asm__ __volatile__(
            "lqc2 $vf31, 0(%1)\n\t"
            "sqc2 $vf31, 0(%0)\n\t"
            : : "r"(pos), "r"(pArg) : "memory");
        pos[3] = 1.0f;
        _ColiApply(local, mtx, pos);
        if (local[3] < -radius || radius < local[3]) {
            ret = -1;
        } else if (local[0] >= 0.0f && local[1] >= 0.0f && local[2] >= 0.0f) {
            ret = 1;
            d = radius - local[3];
            push[0] = mtx[3] * d;
            push[1] = mtx[7] * d;
            push[2] = mtx[11] * d;
            push[3] = 0.0f;
        } else {
            ret = -2;
        }
        if (ret >= 0) {
            XrgAddVector(sum, sum, push);
            (*(int *)pResult)++;
        }
    }
    return *(int *)pResult != 0;
}

static void _InitColiData(RgColiData *pData)
{
    if (pData == 0) {
        assert_prog(D_00A554F0, D_00A55500, 0xEB);
    }
    pData->m_pTriangles = 0;
    pData->m_nTriangles = 0;
    pData->m_nCapacity = 0;
}

static void _DisposeColiData(RgColiData *pData)
{
    if (pData == 0) {
        assert_prog(D_00A554F0, D_00A55500, 243);
    }
    if (pData->m_pTriangles != 0) {
        RgHeapFree(InstanceOfRgHeap(), pData->m_pTriangles, D_00A55500, 245);
    }
    _InitColiData(pData);
}

static void _InitTriangles(RgColiData *pData, int nCapa)
{
    if (pData == 0) {
        assert_prog(D_00A554F0, D_00A55500, 252);
    }
    if (nCapa <= 0) {
        assert_prog(D_00A55548, D_00A55500, 253);
    }

    _DisposeColiData(pData);
    _AllocDataMemory(pData, nCapa);
}

static void _InitTriColi(void *pTri, void *pV0, void *pV1, void *pV2);

static void _AddTriangle(RgColiData *pData, void *pV0, void *pV1, void *pV2)
{
    /* separate load keeps the new triangle's index in its own register */
    int nIndex;
    int nTriangles;

    nIndex = pData->m_nTriangles;
    nTriangles = nIndex;

    if (pData->m_pTriangles == 0) {
        assert_prog(D_00A55528, D_00A55500, 264);
        nTriangles = pData->m_nTriangles;
    }

    if (nTriangles >= pData->m_nCapacity) {
        assert_prog(D_00A55560, D_00A55500, 265);
    }

    _InitTriColi((char *) pData->m_pTriangles + (nIndex << 7), pV0, pV1, pV2);
    pData->m_nTriangles++;
}

RgColiData *CreateRgColiData(void)
{
    RgColiData *pData;

    pData = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgColiData), D_00A55500, 275);
    _InitColiData(pData);
    return pData;
}

typedef struct { RgVector v[8]; } __attribute__((aligned(8))) QuadPrismVerts;
typedef struct { int i[6][4]; } __attribute__((aligned(8))) QuadPrismIdx;
extern const QuadPrismVerts D_00A55590;
extern const QuadPrismIdx D_00A55610;
void RgColiDataInitTriangles(RgColiData *pData, int nCapa);
void RgColiDataAddTriangle(RgColiData *pData, void *pV0, void *pV1, void *pV2);

RgColiData *CreateRgColiDataQuadPrism(float fWidth, float fHeight)
{
    RgColiData *pData;
    QuadPrismVerts verts;
    QuadPrismIdx idx;
    int i;

    pData = CreateRgColiData();
    verts = D_00A55590;
    idx = D_00A55610;
    RgColiDataInitTriangles(pData, 12);
    for (i = 0; i < 8; i++) {
        verts.v[i][0] *= fWidth;
        verts.v[i][1] *= fHeight;
        verts.v[i][2] *= fWidth;
    }
    for (i = 0; i < 6; i++) {
        RgColiDataAddTriangle(pData, verts.v[idx.i[i][0]], verts.v[idx.i[i][2]], verts.v[idx.i[i][1]]);
        RgColiDataAddTriangle(pData, verts.v[idx.i[i][1]], verts.v[idx.i[i][2]], verts.v[idx.i[i][3]]);
    }
    return pData;
}

void DisposeRgColiData(RgColiData *pData)
{
    if (pData == 0) {
        assert_prog(D_00A554F0, D_00A55500, 0x145);
    }
    _DisposeColiData(pData);
    RgHeapFree(InstanceOfRgHeap(), pData, D_00A55500, 0x147);
}

void RgColiDataInitTriangles(RgColiData *pData, int nCapa)
{
    if (pData == 0) {
        assert_prog(D_00A554F0, D_00A55500, 0x150);
    }
    _InitTriangles(pData, nCapa);
}

void RgColiDataAddTriangle(RgColiData *pData, void *pV0, void *pV1, void *pV2)
{
    if (pData == 0) {
        assert_prog(D_00A554F0, D_00A55500, 0x156);
    }
    _AddTriangle(pData, pV0, pV1, pV2);
}

void RgColiDataVsBall(RgColiData *pPoly, void *pArg, void *pResult)
{
    if (pPoly == 0 || pArg == 0 || pResult == 0) {
        assert_prog(D_00A55670, D_00A55500, 0x18D);
    }
    _CheckIntersectBall(pPoly, pArg, pResult);
}

INCLUDE_ASM("asm/nonmatchings/ov12/rg_colidata", RgColiDataVsRay);

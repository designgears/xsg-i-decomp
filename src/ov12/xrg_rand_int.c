/*
 * OV12 original TU 88: 0x00a4caf8..0x00a4e1e8 (60 functions)
 */
#include "common.h"
#include "shared.h"
#include "xrg_rand_int.h"

/* newlib rand(), out of recovery scope: this TU only calls it. */
extern int rand(void);

int XrgRandInt(void)
{
    return rand();
}

float XrgRand(float lower, float upper)
{
    return (float) rand() * 4.656613e-10f * (upper - lower) + lower;
}

int XrgRandIntRange(int lower, int upper)
{
    return (rand() % (upper - lower)) + lower;
}

extern RgVector s_aRes_0;

float *XrgVectorZero(void)
{
    return s_aRes_0;
}

void XrgUnitVector(RgVector vector)
{
    XrgClearVector(vector);
    vector[3] = 1.0f;
}

extern RgVector s_aRes_1;

float *XrgVectorX(void)
{
    return s_aRes_1;
}

extern RgVector s_aRes_2;

float *XrgVectorY(void)
{
    return s_aRes_2;
}

extern RgVector s_aRes_3;

float *XrgVectorZ(void)
{
    return s_aRes_3;
}

void XrgSetVector(RgVector destination, float x, float y, float z, float w)
{
    destination[0] = x;
    destination[1] = y;
    destination[2] = z;
    destination[3] = w;
}

void XrgSetIVector(int destination[4], int x, int y, int z, int w)
{
    destination[0] = x;
    destination[1] = y;
    destination[2] = z;
    destination[3] = w;
}

void XrgSetIVectorXYZ(int destination[4], int x, int y, int z)
{
    destination[0] = x;
    destination[1] = y;
    destination[2] = z;
    destination[3] = 0;
}

void XrgSetVectorXYZ(RgVector destination, float x, float y, float z)
{
    destination[0] = x;
    destination[1] = y;
    destination[2] = z;
    destination[3] = 1.0f;
}

void XrgClearVector(RgVector destination)
{
    destination[0] = destination[1] = destination[2] = destination[3] = 0.0f;
}

void XrgAddVector(RgVector destination, RgVector first, RgVector second)
{
    destination[0] = first[0] + second[0];
    destination[1] = first[1] + second[1];
    destination[2] = first[2] + second[2];
    destination[3] = first[3] + second[3];
}

void XrgAddVectorXYZ(RgVector destination, RgVector first, RgVector second)
{
    destination[0] = first[0] + second[0];
    destination[1] = first[1] + second[1];
    destination[2] = first[2] + second[2];
}

void XrgScaleVector(RgVector destination, RgVector source, float scale)
{
    destination[0] = source[0] * scale;
    destination[1] = source[1] * scale;
    destination[2] = source[2] * scale;
    destination[3] = source[3] * scale;
}

void XrgScaleVectorXYZ(RgVector destination, RgVector source, float scale)
{
    destination[0] = source[0] * scale;
    destination[1] = source[1] * scale;
    destination[2] = source[2] * scale;
    destination[3] = source[3];
}

void XrgCopyVectorXYZ(RgVector destination, RgVector source)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
}

void XrgLinearIntpVector(RgVector destination, RgVector first,
                         RgVector second, float weight)
{
    float complement = 1.0f - weight;

    destination[0] = first[0] * weight + second[0] * complement;
    destination[1] = first[1] * weight + second[1] * complement;
    destination[2] = first[2] * weight + second[2] * complement;
    destination[3] = 1.0f;
}

void XrgCopyVector(RgVector destination, RgVector source)
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    destination[3] = source[3];
}

float XrgLengthVector(const Vector4 *source)
{
    float z_squared = source->z * source->z;
    float y_squared = source->y * source->y;
    float x_squared = source->x * source->x;

    return __builtin_sqrtf(x_squared + y_squared + z_squared);
}

float XrgNormalizeVector(RgVector destination, RgVector source)
{
    float length = __builtin_sqrtf(source[0] * source[0]
                                   + source[1] * source[1]
                                   + source[2] * source[2]);

    if (length <= 0.0f) {
        destination[0] = 0.0f;
        destination[1] = 0.0f;
        destination[2] = 0.0f;
    } else {
        destination[0] = source[0] / length;
        destination[1] = source[1] / length;
        destination[2] = source[2] / length;
    }
    return length;
}

void XrgOuterVector(RgVector destination, RgVector first, RgVector second)
{
    destination[0] = first[1] * second[2] - first[2] * second[1];
    destination[1] = first[2] * second[0] - first[0] * second[2];
    destination[2] = first[0] * second[1] - first[1] * second[0];
}

float XrgInnerVector(RgVector first, RgVector second)
{
    return first[0] * second[0] + first[1] * second[1] + first[2] * second[2];
}

float XrgInnerVectorXYZW(RgVector first, RgVector second)
{
    return first[0] * second[0] + first[1] * second[1] +
           first[2] * second[2] + first[3] * second[3];
}

void clearRgMatrix(RgMatrix matrix)
{
    XrgClearVector(matrix);
    XrgClearVector(matrix + 4);
    XrgClearVector(matrix + 8);
    XrgClearVector(matrix + 12);
}

void XrgCopyMatrix(RgMatrix destination, const RgMatrix source)
{
    int row = 0;

    do {
        XrgCopyVector(&destination[row * 4], (float *)&source[row * 4]);
        row++;
    } while (row < 4);
}

void XrgUnitMatrix(RgMatrix destination)
{
    float *diagonal;
    int row;

    clearRgMatrix(destination);
    diagonal = &destination[15];
    row = 3;
    do {
        row -= 1;
        *diagonal = 1.0f;
        diagonal -= 5;
    } while (row >= 0);
}

void XrgMulMatrix(RgMatrix destination, RgMatrix left, RgMatrix right)
{
    register void *d __asm__("$4") = destination;
    register void *m __asm__("$5") = left;
    register void *s __asm__("$6") = right;

    __asm__ __volatile__(
        ".set noreorder\n\t"
        "lqc2 vf4, 0x0(%2)\n\t"
        "lqc2 vf5, 0x10(%2)\n\t"
        "lqc2 vf6, 0x20(%2)\n\t"
        "lqc2 vf7, 0x30(%2)\n\t"
        "addiu $7, $0, 4\n"
        "1:\n\t"
        "lqc2 vf8, 0x0(%1)\n\t"
        "vmulax.xyzw ACC, vf4, vf8x\n\t"
        "vmadday.xyzw ACC, vf5, vf8y\n\t"
        "vmaddaz.xyzw ACC, vf6, vf8z\n\t"
        "vmaddw.xyzw vf9, vf7, vf8w\n\t"
        "sqc2 vf9, 0x0(%0)\n\t"
        "addi $7, $7, -1\n\t"
        "addi %1, %1, 0x10\n\t"
        "addi %0, %0, 0x10\n\t"
        "bne $0, $7, 1b\n\t"
        "nop\n\t"
        ".set reorder"
        : "+r"(d), "+r"(s) : "r"(m) : "$7", "memory");
}

void XrgApplyVector(RgVector destination, RgMatrix matrix, RgVector vector);

void XrgTransMatrix(RgMatrix destination, RgMatrix source, RgVector translation)
{
    RgVector translated;

    XrgCopyVector(translated, translation);
    translated[3] = 0.0f;
    XrgApplyVector(translated, source, translated);
    XrgCopyMatrix(destination, source);
    XrgAddVectorXYZ(destination + 12, destination + 12, translated);
}

void XrgSubVector(RgVector destination, RgVector first, RgVector second)
{
    destination[0] = first[0] - second[0];
    destination[1] = first[1] - second[1];
    destination[2] = first[2] - second[2];
}

void XrgSubVectorXYZ(RgVector destination, RgVector first, RgVector second)
{
    destination[0] = first[0] - second[0];
    destination[1] = first[1] - second[1];
    destination[2] = first[2] - second[2];
}

void XrgNegateVector(RgVector destination, RgVector source)
{
    destination[0] = -source[0];
    destination[1] = -source[1];
    destination[2] = -source[2];
    destination[3] = source[3];
}

void XrgRotMatrixX(RgMatrix destination, RgMatrix source, float angle);
void XrgRotMatrixY(RgMatrix destination, RgMatrix source, float angle);

void XrgRandVector(RgVector destination)
{
    RgMatrix matrix;
    float x;
    float y;

    x = (float) rand() * 4.656613e-10f;
    y = (float) rand() * 4.656613e-10f;
    x = x * 3.1415927f - 1.5707964f;
    y = y * 6.2831855f - 3.1415927f;
    XrgUnitMatrix(matrix);
    XrgRotMatrixY(matrix, matrix, y);
    XrgRotMatrixX(matrix, matrix, x);
    XrgApplyVector(destination, matrix, XrgVectorZ());
}

void XrgRandVectorRange(RgVector destination, float xRange, float yRange)
{
    RgMatrix matrix;
    float x;
    float y;

    x = (float) rand() * 4.656613e-10f;
    y = (float) rand() * 4.656613e-10f;
    x = x * xRange - xRange * 0.5f;
    y = y * yRange - yRange * 0.5f;
    XrgUnitMatrix(matrix);
    XrgRotMatrixY(matrix, matrix, y);
    XrgRotMatrixX(matrix, matrix, x);
    XrgApplyVector(destination, matrix, XrgVectorZ());
}

/* ACC accumulates the transformed vector one row at a time: row 0 times x,
 * then rows 1-3 times y, z and the source vector's own w, the same shape as
 * xglVectorMulMat (src/main/xgl_2.c). */
void XrgApplyVector(RgVector destination, RgMatrix matrix, RgVector vector)
{
    __asm__ __volatile__(
        "lqc2 vf31, 0(%0)\n\t"
        "lqc2 vf27, 0(%1)\n\t"
        "lqc2 vf28, 16(%1)\n\t"
        "lqc2 vf29, 32(%1)\n\t"
        "lqc2 vf30, 48(%1)\n\t"
        "vmulax.xyzw ACC, vf27, vf31x\n\t"
        "vmadday.xyzw ACC, vf28, vf31y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf31z\n\t"
        "vmaddw.xyzw vf31, vf30, vf31w\n\t"
        "sqc2 vf31, 0(%2)\n\t"
        "nop"
        :
        : "r"(vector), "r"(matrix), "r"(destination)
        : "memory"
    );
}

/* Unlike XrgApplyVector, the final vmaddw writes only vf31's xyz lanes, so
 * the result keeps the source vector's own w component unchanged. */
void XrgRotVector(RgVector destination, RgMatrix matrix, RgVector vector)
{
    __asm__ __volatile__(
        "lqc2 vf31, 0(%0)\n\t"
        "lqc2 vf27, 0(%1)\n\t"
        "lqc2 vf28, 16(%1)\n\t"
        "lqc2 vf29, 32(%1)\n\t"
        "lqc2 vf30, 48(%1)\n\t"
        "vmulax.xyz ACC, vf27, vf31x\n\t"
        "vmadday.xyz ACC, vf28, vf31y\n\t"
        "vmaddaz.xyz ACC, vf29, vf31z\n\t"
        "vmaddw.xyz vf31, vf30, vf0w\n\t"
        "sqc2 vf31, 0(%2)\n\t"
        "nop"
        :
        : "r"(vector), "r"(matrix), "r"(destination)
        : "memory"
    );
}

extern float XrgNormalizeVector(RgVector destination, RgVector source);
extern void XrgOuterVector(RgVector destination, RgVector first, RgVector second);
extern void XrgUnitMatrix(RgMatrix destination);

/*
 * Each XrgCalcMatrix<From>to<To> builds an orthonormal frame (matrix rows 0/1/2
 * as its X/Y/Z axes, ov12:0x00a4d498..0x00a4d698): one axis is normalized
 * directly from an input vector, the second is its cross product with the
 * other input, and the third closes the frame as their cross product. The
 * two input vectors are named for the axis each represents, in the order the
 * original passes them (annotations/overlays/ov12_annotations.csv gives the
 * desired/reference role per function; which role lands in which parameter
 * position is this allocation's own evidence, from XrgNormalizeVector's and
 * XrgOuterVector's argument order at each call site).
 */
void XrgCalcMatrixYtoZ(RgMatrix matrix, RgVector zAxis, RgVector yAxis)
{
    float *rowZ;

    rowZ = matrix + 8;
    XrgUnitMatrix(matrix);
    XrgNormalizeVector(rowZ, zAxis);
    XrgOuterVector(matrix, yAxis, rowZ);
    XrgNormalizeVector(matrix, matrix);
    XrgOuterVector(matrix + 4, rowZ, matrix);
}

void XrgCalcMatrixZtoY(RgMatrix matrix, RgVector zAxis, RgVector yAxis)
{
    float *rowY;

    rowY = matrix + 4;
    XrgUnitMatrix(matrix);
    XrgNormalizeVector(rowY, yAxis);
    XrgOuterVector(matrix, rowY, zAxis);
    XrgNormalizeVector(matrix, matrix);
    XrgOuterVector(matrix + 8, matrix, rowY);
}

void XrgCalcMatrixXtoY(RgMatrix matrix, RgVector xAxis, RgVector yAxis)
{
    float *rowZ;

    XrgUnitMatrix(matrix);
    XrgNormalizeVector(matrix, xAxis);
    rowZ = matrix + 8;
    XrgOuterVector(rowZ, xAxis, yAxis);
    XrgNormalizeVector(rowZ, rowZ);
    XrgOuterVector(matrix + 4, rowZ, matrix);
}

void XrgCalcMatrixXtoZ(RgMatrix matrix, RgVector xAxis, RgVector zAxis)
{
    float *rowY;

    XrgUnitMatrix(matrix);
    XrgNormalizeVector(matrix, xAxis);
    rowY = matrix + 4;
    XrgOuterVector(rowY, zAxis, xAxis);
    XrgNormalizeVector(rowY, rowY);
    XrgOuterVector(matrix + 8, matrix, rowY);
}

float sinf(float);
float cosf(float);

void XrgRotMatrixX(RgMatrix destination, RgMatrix source, float angle)
{
    float m[16];
    float s = sinf(angle);
    float c = cosf(angle);
    XrgUnitMatrix(m);
    m[9] = -s;
    m[6] = s;
    m[5] = c;
    m[10] = c;
    XrgMulMatrix(destination, source, m);
}

void XrgRotMatrixY(RgMatrix destination, RgMatrix source, float angle)
{
    float m[16];
    float s = sinf(angle);
    float c = cosf(angle);
    XrgUnitMatrix(m);
    m[2] = -s;
    m[8] = s;
    m[10] = c;
    m[0] = c;
    XrgMulMatrix(destination, source, m);
}

void XrgRotMatrixZ(RgMatrix destination, RgMatrix source, float angle)
{
    float m[16];
    float s = sinf(angle);
    float c = cosf(angle);
    XrgUnitMatrix(m);
    m[4] = -s;
    m[1] = s;
    m[0] = c;
    m[5] = c;
    XrgMulMatrix(destination, source, m);
}

void XrgInvMatrix(RgMatrix destination, const RgMatrix source)
{
    float rot[16];
    float trans[16];
    int i, j;

    XrgUnitMatrix(rot);
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            rot[j * 4 + i] = source[i * 4 + j];
        }
    }
    XrgUnitMatrix(trans);
    XrgNegateVector(trans + 12, (float *)source + 12);
    XrgMulMatrix(destination, rot, trans);
}

extern double atan2(double y, double x);

float XrgCalcRotY(const Vector4 *vector)
{
    return (float) atan2((double) vector->x, (double) vector->z);
}

extern unsigned char s_aStack[];
extern float *s_paTop;

void XrgClearMatStack(void)
{
    s_paTop = (float *) s_aStack;
    XrgUnitMatrix((float *) s_aStack);
}

/* The current matrix-stack top. The data is still scaffold-owned
 * (docs/naming.md); the scaffold defines it under the original ELF symbol
 * name s_paTop, imported through config/symbols/ov12.txt. */
extern float *s_paTop;

void XrgPushMatStack(void)
{
    float *previous_top = s_paTop;

    s_paTop = previous_top + 16;
    XrgCopyMatrix(s_paTop, previous_top);
}

void XrgSetTopMatStack(const RgMatrix source)
{
    XrgCopyMatrix(s_paTop, source);
}

void XrgPopMatStack(void)
{
    s_paTop -= 16;
}

void XrgMulMatrix(RgMatrix destination, RgMatrix left, RgMatrix right);

void XrgMulMatStack(RgMatrix matrix)
{
    XrgMulMatrix(s_paTop, s_paTop, matrix);
}

void XrgTransMatStack(float x, float y, float z)
{
    RgVector translation;

    XrgSetVectorXYZ(translation, x, y, z);
    XrgApplyVector(translation, s_paTop, translation);
    XrgCopyVector(s_paTop + 12, translation);
}

void XrgRotMatrixX(RgMatrix destination, RgMatrix source, float angle);
void XrgRotMatrixY(RgMatrix destination, RgMatrix source, float angle);
void XrgRotMatrixZ(RgMatrix destination, RgMatrix source, float angle);

void XrgRotMatStack(float angle_x, float angle_y, float angle_z)
{
    XrgRotMatrixY(s_paTop, s_paTop, angle_y);
    XrgRotMatrixX(s_paTop, s_paTop, angle_x);
    XrgRotMatrixZ(s_paTop, s_paTop, angle_z);
}

float *XrgTopMatStack(void)
{
    return s_paTop;
}

void XrgCopyMatStack(RgMatrix destination)
{
    XrgCopyMatrix(destination, s_paTop);
}

void XrgCalcPlaneNorm(RgVector normal, RgVector p0, RgVector p1, RgVector p2)
{
    RgVector edge0;
    RgVector edge1;

    XrgSubVector(edge0, p1, p0);
    XrgSubVector(edge1, p2, p1);
    XrgOuterVector(normal, edge0, edge1);
    XrgNormalizeVector(normal, normal);
    normal[3] = 1.0f;
}

/* normal = (p1 - p0) x (p2 - p1), via XrgSubVector and XrgOuterVector. */
void XrgCalcPlaneNorm(RgVector normal, RgVector p0, RgVector p1, RgVector p2);
float XrgInnerVector(RgVector first, RgVector second);

/* The plane through three points is stored as a 4-component vector:
 * XrgCalcPlaneNorm fills the first three floats with the plane normal, and
 * this function stores the negated dot product of that normal with the first
 * point in the fourth float (offset 0xC), the plane distance. */
void XrgCalcPlane(RgVector plane, RgVector p0, RgVector p1, RgVector p2)
{
    XrgCalcPlaneNorm(plane, p0, p1, p2);
    plane[3] = -XrgInnerVector(plane, p0);
}

int XrgIsTriIntersectedByRay(RgVector origin, RgVector dir, RgVector tri, float *t)
{
    RgVector e0;
    RgVector e1;
    RgVector e2;
    RgVector n;
    RgVector d;
    float dn;
    float r;

    XrgSubVector(e0, tri + 4, tri);
    XrgSubVector(e1, tri + 8, tri + 4);
    XrgSubVector(e2, tri, tri + 8);
    XrgOuterVector(n, e0, dir);
    XrgSubVector(d, origin, tri);
    if (XrgInnerVector(d, n) <= -0.0001f) return 0;
    XrgOuterVector(n, e1, dir);
    XrgSubVector(d, origin, tri + 4);
    if (XrgInnerVector(d, n) <= -0.0001f) return 0;
    XrgOuterVector(n, e2, dir);
    XrgSubVector(d, origin, tri + 8);
    if (XrgInnerVector(d, n) <= -0.0001f) return 0;
    XrgOuterVector(n, e0, e1);
    n[3] = -XrgInnerVector(n, tri);
    dn = XrgInnerVector(n, dir);
    if (dn >= -0.0001f) return 0;
    r = -(XrgInnerVector(n, origin) + n[3]) / dn;
    *t = r;
    if (r >= 0.0f && r < 1.0f) return 1;
    return 0;
}

int XrgQuantAngle(float a)
{
    while (a > 3.1415927f) {
        a -= 6.2831855f;
    }
    while (a < -3.1415927f) {
        a += 6.2831855f;
    }
    if (a >= -0.3926991f && a < 0.3926991f) {
        return 0;
    }
    if (a >= 0.3926991f && a < 1.1780972f) {
        return 1;
    }
    if (a >= 1.1780972f && a < 1.96349549f) {
        return 2;
    }
    if (a >= 1.96349549f && a < 2.74889374f) {
        return 3;
    }
    if (a >= 2.74889374f || a < -2.74889374f) {
        return 4;
    }
    if (a >= -2.74889374f && a < -1.96349549f) {
        return 5;
    }
    if (a >= -1.96349549f && a < -1.1780972f) {
        return 6;
    }
    return 7;
}

int XrgQuantAngle4(float a)
{
    while (a > 3.1415927f) {
        a -= 6.2831855f;
    }
    while (a < -3.1415927f) {
        a += 6.2831855f;
    }
    if (a >= -0.7853982f && a < 0.7853982f) {
        return 0;
    }
    if (a >= 0.7853982f && a < 2.3561945f) {
        return 1;
    }
    if (a >= 2.3561945f || a < -2.3561945f) {
        return 2;
    }
    if (a >= -2.3561945f && a < -0.7853982f) {
        return 3;
    }
    return -1;
}

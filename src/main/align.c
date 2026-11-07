#include "port/ps2_asm.h"
#include "common.h"
#include "shared.h"
#include "align.h"

/* These matrix helpers are defined in the matrix_print translation unit.
 * Their angle arguments are pointers to the float bit patterns consumed by
 * the VU0 trigonometric path. */
extern void CUR_MATRIX_Unit(void);
extern void CUR_MATRIX_Get(Matrix4 matrix);
extern void CUR_MATRIX_Rx4s(const float *angle);
extern void CUR_MATRIX_Ry4s(const float *angle);
extern void CUR_MATRIX_Rz4s(const float *angle);

void alignXYAxis(void) {

}

/*
 * alignXAxisEuler: derive the yaw/pitch Euler pair that rotates an object's
 * local X axis onto "direction". The VU0 macro-mode block reads direction as
 * a quadword, forms x^2+y^2 and x^2+y^2+z^2, and uses two reciprocal-square-
 * root passes (Q) to rescale the components by 1/sqrt(x^2+y^2) and by
 * 1/length, packing the four ratios into components. components.y/components.x
 * hold sin/cos of the heading about Z (their ratio is atan2(y,x), unchanged
 * by the common horizontal scale); components.z/components.w hold sin/cos of
 * the elevation about Y. euler->x (no additional roll once X is aligned) and
 * euler->w (homogeneous term) are fixed.
 */
void alignXAxisEuler(const Vector4 *direction, Vector4 *euler) {
    Vector4 components;
    float pitch;

#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1, 0(%1)\n\t"
        "vsubw.y vf11, vf0, vf0w\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vmul.xyz vf2, vf1, vf1\n\t"
        "vmul.y vf1, vf1, vf11\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vaddy.x vf3, vf2, vf2y\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vrsqrt Q, vf0w, vf3x\n\t"
        "vaddz.x vf4, vf3, vf2z\n\t"
        "vwaitq\n\t"
        "vaddq.x vf5, vf0, Q\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vrsqrt Q, vf0w, vf4x\n\t"
        "vmulx.xy vf10, vf1, vf5x\n\t"
        "vwaitq\n\t"
        "vaddq.w vf5, vf0, Q\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vdiv Q, vf0w, vf5x\n\t"
        "vmulw.z vf10, vf1, vf5w\n\t"
        "vwaitq\n\t"
        "vmulq.w vf10, vf5, Q\n\t"
        "sqc2 vf10, 0(%0)"
        : : "r"(&components), "r"(direction)
        : "memory");
#else
    {
        XsgGpr r_1 = XSG_IN(direction);
        XsgGpr r_0 = XSG_IN(&components);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_arith(VU0_SUB, 4, 11, 0, 0, 3);
        vu0_arith(VU0_MUL, 14, 2, 1, 1, VU0_VEC);
        vu0_arith(VU0_MUL, 4, 1, 1, 11, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 3, 2, 2, 1);
        vu0_rsqrt(0, 3, 3, 0);
        vu0_arith(VU0_ADD, 8, 4, 3, 2, 2);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 5, 0, 0, VU0_BQ);
        vu0_rsqrt(0, 3, 4, 0);
        vu0_arith(VU0_MUL, 12, 10, 1, 5, 0);
        vu0_waitq();
        vu0_arith(VU0_ADD, 1, 5, 0, 0, VU0_BQ);
        vu0_div(0, 3, 5, 0);
        vu0_arith(VU0_MUL, 2, 10, 1, 5, 3);
        vu0_waitq();
        vu0_arith(VU0_MUL, 1, 10, 5, 0, VU0_BQ);
        vu0_sqc2(10, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif

    euler->w = 1.0f;
    euler->z = xglAtan2(components.y, components.x);
    pitch = xglAtan2(components.z, components.w);
    euler->x = 0.0f;
    euler->y = pitch;
}

/* The VU0 block builds the component ratios used for the two rotations that
 * place the current matrix's X axis along direction.  The scalar atan2 calls
 * and the persistent matrix operations remain ordinary C. */
void alignXAxis(const Vector4 *direction, Matrix4 matrix) {
    float rotation_angles[3];
    Vector4 components;

#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1, 0(%1)\n\t"
        "vsubw.y vf11, vf0, vf0w\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vmul.xyz vf2, vf1, vf1\n\t"
        "vmul.y vf1, vf1, vf11\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vaddy.x vf3, vf2, vf2y\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vrsqrt Q, vf0w, vf3x\n\t"
        "vaddz.x vf4, vf3, vf2z\n\t"
        "vwaitq\n\t"
        "vaddq.x vf5, vf0, Q\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vrsqrt Q, vf0w, vf4x\n\t"
        "vmulx.xy vf10, vf1, vf5x\n\t"
        "vwaitq\n\t"
        "vaddq.w vf5, vf0, Q\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vdiv Q, vf0w, vf5x\n\t"
        "vmulw.z vf10, vf1, vf5w\n\t"
        "vwaitq\n\t"
        "vmulq.w vf10, vf5, Q\n\t"
        "sqc2 vf10, 0(%0)"
        :
        : "r"(&components), "r"(direction)
        : "memory");
#else
    {
        XsgGpr r_1 = XSG_IN(direction);
        XsgGpr r_0 = XSG_IN(&components);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_arith(VU0_SUB, 4, 11, 0, 0, 3);
        vu0_arith(VU0_MUL, 14, 2, 1, 1, VU0_VEC);
        vu0_arith(VU0_MUL, 4, 1, 1, 11, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 3, 2, 2, 1);
        vu0_rsqrt(0, 3, 3, 0);
        vu0_arith(VU0_ADD, 8, 4, 3, 2, 2);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 5, 0, 0, VU0_BQ);
        vu0_rsqrt(0, 3, 4, 0);
        vu0_arith(VU0_MUL, 12, 10, 1, 5, 0);
        vu0_waitq();
        vu0_arith(VU0_ADD, 1, 5, 0, 0, VU0_BQ);
        vu0_div(0, 3, 5, 0);
        vu0_arith(VU0_MUL, 2, 10, 1, 5, 3);
        vu0_waitq();
        vu0_arith(VU0_MUL, 1, 10, 5, 0, VU0_BQ);
        vu0_sqc2(10, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif

    rotation_angles[0] = 0.0f;
    rotation_angles[1] = xglAtan2(components.z, components.w);
    rotation_angles[2] = xglAtan2(components.y, components.x);
    CUR_MATRIX_Unit();
    CUR_MATRIX_Ry4s(&rotation_angles[1]);
    CUR_MATRIX_Rz4s(&rotation_angles[2]);
    CUR_MATRIX_Get(matrix);
}

/* The VU0 block derives the two angles for the X-then-Z rotations that align
 * the current matrix's Y axis with direction. */
void alignYAxis(const Vector4 *direction, Matrix4 matrix) {
    float rotation_angles[3];
    Vector4 components;

#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1, 0(%1)\n\t"
        "vsubw.z vf11, vf0, vf0w\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vmul.xyz vf2, vf1, vf1\n\t"
        "vmul.z vf1, vf1, vf11\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vaddy.z vf3, vf2, vf2y\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vrsqrt Q, vf0w, vf3z\n\t"
        "vaddz.x vf4, vf2, vf3z\n\t"
        "vwaitq\n\t"
        "vaddq.x vf5, vf0, Q\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vrsqrt Q, vf0w, vf4x\n\t"
        "vmulx.yz vf10, vf1, vf5x\n\t"
        "vwaitq\n\t"
        "vaddq.w vf5, vf0, Q\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vdiv Q, vf0w, vf5x\n\t"
        "vmulw.x vf10, vf1, vf5w\n\t"
        "vwaitq\n\t"
        "vmulq.w vf10, vf5, Q\n\t"
        "sqc2 vf10, 0(%0)"
        :
        : "r"(&components), "r"(direction)
        : "memory");
#else
    {
        XsgGpr r_1 = XSG_IN(direction);
        XsgGpr r_0 = XSG_IN(&components);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_arith(VU0_SUB, 2, 11, 0, 0, 3);
        vu0_arith(VU0_MUL, 14, 2, 1, 1, VU0_VEC);
        vu0_arith(VU0_MUL, 2, 1, 1, 11, VU0_VEC);
        vu0_arith(VU0_ADD, 2, 3, 2, 2, 1);
        vu0_rsqrt(0, 3, 3, 2);
        vu0_arith(VU0_ADD, 8, 4, 2, 3, 2);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 5, 0, 0, VU0_BQ);
        vu0_rsqrt(0, 3, 4, 0);
        vu0_arith(VU0_MUL, 6, 10, 1, 5, 0);
        vu0_waitq();
        vu0_arith(VU0_ADD, 1, 5, 0, 0, VU0_BQ);
        vu0_div(0, 3, 5, 0);
        vu0_arith(VU0_MUL, 8, 10, 1, 5, 3);
        vu0_waitq();
        vu0_arith(VU0_MUL, 1, 10, 5, 0, VU0_BQ);
        vu0_sqc2(10, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif

    rotation_angles[0] = xglAtan2(components.z, components.y);
    rotation_angles[1] = 0.0f;
    rotation_angles[2] = xglAtan2(components.x, components.w);
    CUR_MATRIX_Unit();
    CUR_MATRIX_Rx4s(&rotation_angles[0]);
    CUR_MATRIX_Rz4s(&rotation_angles[2]);
    CUR_MATRIX_Get(matrix);
}

/* The VU0 block derives the two angles for the X-then-Y rotations that align
 * the current matrix's Z axis with direction. */
void alignZAxis(const Vector4 *direction, Matrix4 matrix) {
    float rotation_angles[3];
    Vector4 components;

#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1, 0(%1)\n\t"
        "vsubw.x vf11, vf0, vf0w\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vmul.xyz vf2, vf1, vf1\n\t"
        "vmul.x vf1, vf1, vf11\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vaddy.z vf3, vf2, vf2y\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vrsqrt Q, vf0w, vf3z\n\t"
        "vaddz.x vf4, vf2, vf3z\n\t"
        "vwaitq\n\t"
        "vaddq.x vf5, vf0, Q\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vrsqrt Q, vf0w, vf4x\n\t"
        "vmulx.yz vf10, vf1, vf5x\n\t"
        "vwaitq\n\t"
        "vaddq.w vf5, vf0, Q\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vdiv Q, vf0w, vf5x\n\t"
        "vmulw.x vf10, vf1, vf5w\n\t"
        "vwaitq\n\t"
        "vmulq.w vf10, vf5, Q\n\t"
        "sqc2 vf10, 0(%0)"
        :
        : "r"(&components), "r"(direction)
        : "memory");
#else
    {
        XsgGpr r_1 = XSG_IN(direction);
        XsgGpr r_0 = XSG_IN(&components);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_arith(VU0_SUB, 8, 11, 0, 0, 3);
        vu0_arith(VU0_MUL, 14, 2, 1, 1, VU0_VEC);
        vu0_arith(VU0_MUL, 8, 1, 1, 11, VU0_VEC);
        vu0_arith(VU0_ADD, 2, 3, 2, 2, 1);
        vu0_rsqrt(0, 3, 3, 2);
        vu0_arith(VU0_ADD, 8, 4, 2, 3, 2);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 5, 0, 0, VU0_BQ);
        vu0_rsqrt(0, 3, 4, 0);
        vu0_arith(VU0_MUL, 6, 10, 1, 5, 0);
        vu0_waitq();
        vu0_arith(VU0_ADD, 1, 5, 0, 0, VU0_BQ);
        vu0_div(0, 3, 5, 0);
        vu0_arith(VU0_MUL, 8, 10, 1, 5, 3);
        vu0_waitq();
        vu0_arith(VU0_MUL, 1, 10, 5, 0, VU0_BQ);
        vu0_sqc2(10, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif

    rotation_angles[0] = xglAtan2(components.y, components.z);
    rotation_angles[1] = xglAtan2(components.x, components.w);
    rotation_angles[2] = 0.0f;
    CUR_MATRIX_Unit();
    CUR_MATRIX_Rx4s(&rotation_angles[0]);
    CUR_MATRIX_Ry4s(&rotation_angles[1]);
    CUR_MATRIX_Get(matrix);
}

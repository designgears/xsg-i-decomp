#include "port/ps2_asm.h"
#include "common.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "outer_product.h"

typedef union {
    Vector4 vector;
    u64 words[2];
} OuterProductVector;

void OuterProduct(const OuterProductVector *left, const OuterProductVector *right,
                  OuterProductVector *destination)
{
    OuterProductVector result;

    result.vector.x = left->vector.y * right->vector.z
                    - left->vector.z * right->vector.y;
    result.vector.y = left->vector.z * right->vector.x
                    - left->vector.x * right->vector.z;
    result.vector.z = left->vector.x * right->vector.y
                    - left->vector.y * right->vector.x;
    __builtin_memcpy(destination, &result, sizeof(result));
}

void CalcVerticalVector(const Vector4 *first, const Vector4 *second,
                        const Vector4 *third, Vector4 *vertical)
{
    Vector4 first_edge;
    Vector4 second_edge;
    Vector4 normal;

#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf3, 0(%1)\n\t"
        "lqc2 vf2, 0(%2)\n\t"
        "vsub.xyz vf2, vf2, vf3\n\t"
        "sqc2 vf2, 0(%0)"
        :
        : "r"(&first_edge), "r"(first), "r"(second)
        : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(second);
        XsgGpr r_1 = XSG_IN(first);
        XsgGpr r_0 = XSG_IN(&first_edge);
        vu0_lqc2(3, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 14, 2, 2, 3, VU0_VEC);
        vu0_sqc2(2, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf3, 0(%1)\n\t"
        "lqc2 vf2, 0(%2)\n\t"
        "vsub.xyz vf2, vf2, vf3\n\t"
        "sqc2 vf2, 0(%0)"
        :
        : "r"(&second_edge), "r"(first), "r"(third)
        : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(third);
        XsgGpr r_1 = XSG_IN(first);
        XsgGpr r_0 = XSG_IN(&second_edge);
        vu0_lqc2(3, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 14, 2, 2, 3, VU0_VEC);
        vu0_sqc2(2, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
    xglVectorOuter(&normal, &first_edge, &second_edge);
    xglVectorOuter(vertical, &first_edge, &normal);
}

extern void CalcVerticalVector(const Vector4 *first, const Vector4 *second,
                               const Vector4 *third, Vector4 *vertical);
extern void xglVectorNormal(Vector4 *destination, const Vector4 *source);
extern void xglVectorInner(float *result, const Vector4 *left, const Vector4 *right);

float CalcLength(const Vector4 *first, const Vector4 *second, const Vector4 *third)
{
    Vector4 vertical;
    Vector4 delta;
    float length;

    CalcVerticalVector(first, second, third, &vertical);
    xglVectorNormal(&vertical, &vertical);
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf3, 0(%0)\n\t"
        "lqc2 vf2, 0(%1)\n\t"
        "vsub.xyz vf2, vf2, vf3\n\t"
        "sqc2 vf2, 0(%2)"
        :
        : "r"(third), "r"(first), "r"(&delta)
        : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(&delta);
        XsgGpr r_1 = XSG_IN(first);
        XsgGpr r_0 = XSG_IN(third);
        vu0_lqc2(3, XSG_EA(r_0, 0));
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_arith(VU0_SUB, 14, 2, 2, 3, VU0_VEC);
        vu0_sqc2(2, XSG_EA(r_2, 0));
        xsg_vu0_blocks++;
    }
#endif
    xglVectorInner(&length, &vertical, &delta);
    return length;
}

void CalcCrossPoint(const Vector4 *first, const Vector4 *second,
                    const Vector4 *third, const Vector4 *fourth,
                    Vector4 *cross_point)
{
    Vector4 normal;
    float intersection_values[2];

    intersection_values[1] = CalcLength(first, second, third);
    CalcVerticalVector(first, second, third, &normal);
    xglVectorNormal(&normal, &normal);
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf3, 0(%1)\n\t"
        "lqc2 vf2, 0(%2)\n\t"
        "vsub.xyz vf2, vf2, vf3\n\t"
        "sqc2 vf2, 0(%0)"
        :
        : "r"(cross_point), "r"(third), "r"(fourth)
        : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(fourth);
        XsgGpr r_1 = XSG_IN(third);
        XsgGpr r_0 = XSG_IN(cross_point);
        vu0_lqc2(3, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 14, 2, 2, 3, VU0_VEC);
        vu0_sqc2(2, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
    xglVectorInner(&intersection_values[0], cross_point, &normal);
    intersection_values[0] = intersection_values[1] / intersection_values[0];
    xglVectorLength(&intersection_values[1], cross_point);
    xglVectorNormal(cross_point, cross_point);
    cross_point->x *= intersection_values[0] * intersection_values[1];
    cross_point->y *= intersection_values[0] * intersection_values[1];
    cross_point->z *= intersection_values[0] * intersection_values[1];
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf20, 0(%0)\n\t"
        "lqc2 vf21, 0(%1)\n\t"
        "vadd.xyzw vf20, vf20, vf21\n\t"
        "sqc2 vf20, 0(%0)"
        :
        : "r"(cross_point), "r"(third)
        : "memory");
#else
    {
        XsgGpr r_1 = XSG_IN(third);
        XsgGpr r_0 = XSG_IN(cross_point);
        vu0_lqc2(20, XSG_EA(r_0, 0));
        vu0_lqc2(21, XSG_EA(r_1, 0));
        vu0_arith(VU0_ADD, 15, 20, 20, 21, VU0_VEC);
        vu0_sqc2(20, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
}

int CheckPointLine(const Vector4 *first, const Vector4 *second,
                   const Vector4 *point)
{
    Vector4 first_delta;
    Vector4 second_delta;
    float cross;
    float first_x = first->x;
    int result = 1;

    first_delta.x = second->x - first_x;
    second_delta.x = point->x - first_x;
    first_delta.z = second->z - first->z;
    second_delta.z = point->z - first->z;
    cross = first_delta.x * second_delta.z
          - first_delta.z * second_delta.x;

    if (!(0.0f < cross)) {
        if (cross < 0.0f) {
            result = 2;
        } else {
            result = 0;
        }
    }
    return result;
}

int CheckCrossLine(const Vector4 *first_start, const Vector4 *first_end,
                   const Vector4 *second_start, const Vector4 *second_end)
{
    int first_start_side = CheckPointLine(first_start, first_end, second_start);
    int first_end_side = CheckPointLine(first_start, first_end, second_end);

    if (first_start_side != first_end_side) {
        int second_start_side = CheckPointLine(second_start, second_end, first_start);
        int second_end_side = CheckPointLine(second_start, second_end, first_end);

        if (second_start_side != second_end_side) {
            return 1;
        }
    }
    return 0;
}

int CheckCrossCircle(float radius, const Vector4 *start, const Vector4 *end, const Vector4 *point)
{
    Vector4 center;
    Vector4 dir;
    float half;
    float dist;
    float dx;
    float dz;

    dir.x = end->x - start->x;
    dir.y = 0.0f;
    dir.z = end->z - start->z;
    dir.w = 1.0f;
    half = __builtin_sqrtf(dir.x * dir.x + dir.z * dir.z) * 0.5f;
    xglVectorNormal(&dir, &dir);
    center.x = start->x + (end->x - start->x) * 0.5f;
    center.z = start->z + (end->z - start->z) * 0.5f;
    dx = point->x - center.x;
    dz = point->z - center.z;
    dist = dx * dir.x + dz * dir.z;
    if (half + radius < __builtin_fabsf(dist)) {
        return 0;
    }
    dx = point->x - (dist * dir.x + center.x);
    dz = point->z - (dist * dir.z + center.z);
    if (dx * dx + dz * dz < radius * radius) {
        return 1;
    }
    return 0;
}

float CheckDist3D(const Vector4 *first, const Vector4 *second)
{
    return __builtin_sqrtf((first->x - second->x) * (first->x - second->x)
                           + (first->y - second->y) * (first->y - second->y)
                           + (first->z - second->z) * (first->z - second->z));
}

float CheckDist2D(const Vector4 *first, const Vector4 *second)
{
    return __builtin_sqrtf((first->x - second->x) * (first->x - second->x)
                           + (first->z - second->z) * (first->z - second->z));
}

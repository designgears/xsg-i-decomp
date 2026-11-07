#include "port/ps2_asm.h"
#include "common.h"

#include "shared.h"

#include "main/xgl_2.h"

#include "xgl_2.h"

extern unsigned int D_004ADE80[];

extern unsigned long long iRandSeed;


extern void xglDmaDirectSrcChain(unsigned int channel, unsigned int address);

extern StudioCamera *xglStudioGetCamera2(int cameraId);

extern char Vu0CallSin[];

extern char Vu0CallCos[];

extern const int D_004D27C8[3];

float xglAtan2(float y, float x)
{
    int reciprocal = 0;
    int quadrant = 0;
    const float coefficient1 = 0.0161657371f;
    const float coefficient2 = 0.0429096147f;
    const float coefficient3 = 0.0752896369f;
    const float coefficient4 = 0.106562637f;
    const float coefficient5 = 0.142088994f;
    const float coefficient6 = 0.199935511f;
    const float coefficient7 = 0.333331466f;
    const float unitCoefficient = 1.0f;
    float polynomial = 0.00286622578f;
    float square;

    if (y == 0.0f) {
        if (x >= 0.0f) return 0.0f;
        __builtin_memcpy(&reciprocal, &y, sizeof reciprocal);
        if (reciprocal < 0) return -3.14159274f;
        return 3.14159274f;
    }
    if (x == 0.0f) {
        if (y < 0.0f) return -1.57079637f;
        return 1.57079637f;
    }
    if (y >= 0.0f) {
        if (x >= 0.0f) quadrant = 0;
        else quadrant = 1;
    } else {
        quadrant = 2;
        if (x >= 0.0f) quadrant = 2;
        else quadrant = 3;
    }
    y = __builtin_fabsf(y);
    x = __builtin_fabsf(x);
    if (x < y) {
        y = x / y;
        reciprocal = 1;
    } else {
        y = y / x;
    }
    square = y * y;
    polynomial *= square;
    polynomial -= coefficient1;
    polynomial *= square;
    polynomial += coefficient2;
    polynomial *= square;
    polynomial -= coefficient3;
    polynomial *= square;
    polynomial += coefficient4;
    polynomial *= square;
    polynomial -= coefficient5;
    polynomial *= square;
    polynomial += coefficient6;
    polynomial *= square;
    polynomial -= coefficient7;
    polynomial *= square;
    polynomial += unitCoefficient;
    polynomial *= y;
    if (reciprocal) polynomial = 1.57079637f - polynomial;
    if (quadrant == 1) polynomial = 3.14159274f - polynomial;
    if (quadrant == 2) polynomial = -polynomial;
    if (quadrant == 3) polynomial = polynomial - 3.14159274f;
    return polynomial;
}

void xglMemCopy64(void *destination, const void *source, int count)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "1:\n\t"
        "lq $2,0(%1)\n\t"
        "sq $2,0(%0)\n\t"
        "lq $2,16(%1)\n\t"
        "sq $2,16(%0)\n\t"
        "lq $2,32(%1)\n\t"
        "sq $2,32(%0)\n\t"
        "lq $2,48(%1)\n\t"
        "sq $2,48(%0)\n\t"
        "addi %2,%2,-1\n\t"
        "addi %0,%0,64\n\t"
        "addi %1,%1,64\n\t"
        "bne $0,%2,1b\n\t"
        "nop"
        : "+r"(destination), "+r"(source), "+r"(count)
        :
        : "$2", "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(count);
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        xsg_Lxgl_2_93_0:;
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 0), 16);
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 16), 16);
        __builtin_memcpy(XSG_EA(r_0, 16), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 32), 16);
        __builtin_memcpy(XSG_EA(r_0, 32), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 48), 16);
        __builtin_memcpy(XSG_EA(r_0, 48), &xsg_ee_gpr[2], 16);
        xsg_ee_set32(&r_2, r_2.w[0] + (unsigned int)(-1));
        xsg_ee_set32(&r_0, r_0.w[0] + (unsigned int)(64));
        xsg_ee_set32(&r_1, r_1.w[0] + (unsigned int)(64));
        if (xsg_ee_zero.d[0] != r_2.d[0]) goto xsg_Lxgl_2_93_0;
        xsg_vu0_blocks++;
        XSG_OUT(destination, r_0);
        XSG_OUT(source, r_1);
        XSG_OUT(count, r_2);
    }
#endif
}

void xglMemCopy64b(void *destination, const void *source, int count)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "1:\n\t"
        "lq $2,0(%1)\n\t"
        "sq $2,0(%0)\n\t"
        "lq $2,-16(%1)\n\t"
        "sq $2,-16(%0)\n\t"
        "lq $2,-32(%1)\n\t"
        "sq $2,-32(%0)\n\t"
        "lq $2,-48(%1)\n\t"
        "sq $2,-48(%0)\n\t"
        "addi %2,%2,-1\n\t"
        "addi %0,%0,-64\n\t"
        "addi %1,%1,-64\n\t"
        "bne $0,%2,1b\n\t"
        "nop"
        : "+r"(destination), "+r"(source), "+r"(count)
        :
        : "$2", "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(count);
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        xsg_Lxgl_2_115_0:;
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 0), 16);
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, -16), 16);
        __builtin_memcpy(XSG_EA(r_0, -16), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, -32), 16);
        __builtin_memcpy(XSG_EA(r_0, -32), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, -48), 16);
        __builtin_memcpy(XSG_EA(r_0, -48), &xsg_ee_gpr[2], 16);
        xsg_ee_set32(&r_2, r_2.w[0] + (unsigned int)(-1));
        xsg_ee_set32(&r_0, r_0.w[0] + (unsigned int)(-64));
        xsg_ee_set32(&r_1, r_1.w[0] + (unsigned int)(-64));
        if (xsg_ee_zero.d[0] != r_2.d[0]) goto xsg_Lxgl_2_115_0;
        xsg_vu0_blocks++;
        XSG_OUT(destination, r_0);
        XSG_OUT(source, r_1);
        XSG_OUT(count, r_2);
    }
#endif
}

void xglMemCopy16(void *destination, const void *source, int count)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "1:\n\t"
        "lq $2,0(%1)\n\t"
        "sq $2,0(%0)\n\t"
        "addi %2,%2,-1\n\t"
        "addi %0,%0,16\n\t"
        "addi %1,%1,16\n\t"
        "bne $0,%2,1b\n\t"
        "nop"
        : "+r"(destination), "+r"(source), "+r"(count)
        :
        : "$2", "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(count);
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        xsg_Lxgl_2_137_0:;
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 0), 16);
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[2], 16);
        xsg_ee_set32(&r_2, r_2.w[0] + (unsigned int)(-1));
        xsg_ee_set32(&r_0, r_0.w[0] + (unsigned int)(16));
        xsg_ee_set32(&r_1, r_1.w[0] + (unsigned int)(16));
        if (xsg_ee_zero.d[0] != r_2.d[0]) goto xsg_Lxgl_2_137_0;
        xsg_vu0_blocks++;
        XSG_OUT(destination, r_0);
        XSG_OUT(source, r_1);
        XSG_OUT(count, r_2);
    }
#endif
}

void xglMemCopy16b(void *destination, const void *source, int count)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "1:\n\t"
        "lq $2,0(%1)\n\t"
        "sq $2,0(%0)\n\t"
        "addi %2,%2,-1\n\t"
        "addi %0,%0,-16\n\t"
        "addi %1,%1,-16\n\t"
        "bne $0,%2,1b\n\t"
        "nop"
        : "+r"(destination), "+r"(source), "+r"(count)
        :
        : "$2", "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(count);
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        xsg_Lxgl_2_153_0:;
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 0), 16);
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[2], 16);
        xsg_ee_set32(&r_2, r_2.w[0] + (unsigned int)(-1));
        xsg_ee_set32(&r_0, r_0.w[0] + (unsigned int)(-16));
        xsg_ee_set32(&r_1, r_1.w[0] + (unsigned int)(-16));
        if (xsg_ee_zero.d[0] != r_2.d[0]) goto xsg_Lxgl_2_153_0;
        xsg_vu0_blocks++;
        XSG_OUT(destination, r_0);
        XSG_OUT(source, r_1);
        XSG_OUT(count, r_2);
    }
#endif
}

void xglMemCopy8(void *destination, const void *source, int count)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "1:\n\t"
        "ld $2,0(%1)\n\t"
        "sd $2,0(%0)\n\t"
        "addi %2,%2,-1\n\t"
        "addi %0,%0,8\n\t"
        "addi %1,%1,8\n\t"
        "bne $0,%2,1b\n\t"
        "nop"
        : "+r"(destination), "+r"(source), "+r"(count)
        :
        : "$2", "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(count);
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        xsg_Lxgl_2_169_0:;
        { unsigned long long xsg_v_; __builtin_memcpy(&xsg_v_, XSG_EA(r_1, 0), sizeof xsg_v_); xsg_ee_gpr[2].d[0] = xsg_v_; }
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[2], 8);
        xsg_ee_set32(&r_2, r_2.w[0] + (unsigned int)(-1));
        xsg_ee_set32(&r_0, r_0.w[0] + (unsigned int)(8));
        xsg_ee_set32(&r_1, r_1.w[0] + (unsigned int)(8));
        if (xsg_ee_zero.d[0] != r_2.d[0]) goto xsg_Lxgl_2_169_0;
        xsg_vu0_blocks++;
        XSG_OUT(destination, r_0);
        XSG_OUT(source, r_1);
        XSG_OUT(count, r_2);
    }
#endif
}

void xglMemCopy8b(void *destination, const void *source, int count)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "1:\n\t"
        "ld $2,0(%1)\n\t"
        "sd $2,0(%0)\n\t"
        "addi %2,%2,-1\n\t"
        "addi %0,%0,-8\n\t"
        "addi %1,%1,-8\n\t"
        "bne $0,%2,1b\n\t"
        "nop"
        : "+r"(destination), "+r"(source), "+r"(count)
        :
        : "$2", "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(count);
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        xsg_Lxgl_2_185_0:;
        { unsigned long long xsg_v_; __builtin_memcpy(&xsg_v_, XSG_EA(r_1, 0), sizeof xsg_v_); xsg_ee_gpr[2].d[0] = xsg_v_; }
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[2], 8);
        xsg_ee_set32(&r_2, r_2.w[0] + (unsigned int)(-1));
        xsg_ee_set32(&r_0, r_0.w[0] + (unsigned int)(-8));
        xsg_ee_set32(&r_1, r_1.w[0] + (unsigned int)(-8));
        if (xsg_ee_zero.d[0] != r_2.d[0]) goto xsg_Lxgl_2_185_0;
        xsg_vu0_blocks++;
        XSG_OUT(destination, r_0);
        XSG_OUT(source, r_1);
        XSG_OUT(count, r_2);
    }
#endif
}

void xglMemCopy4(void *destination, const void *source, int count)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "1:\n\t"
        "lw $2,0(%1)\n\t"
        "sw $2,0(%0)\n\t"
        "addi %2,%2,-1\n\t"
        "addi %0,%0,4\n\t"
        "addi %1,%1,4\n\t"
        "bne $0,%2,1b\n\t"
        "nop"
        : "+r"(destination), "+r"(source), "+r"(count)
        :
        : "$2", "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(count);
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        xsg_Lxgl_2_201_0:;
        { int xsg_v_; __builtin_memcpy(&xsg_v_, XSG_EA(r_1, 0), sizeof xsg_v_); xsg_ee_gpr[2].sd[0] = xsg_v_; }
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[2], 4);
        xsg_ee_set32(&r_2, r_2.w[0] + (unsigned int)(-1));
        xsg_ee_set32(&r_0, r_0.w[0] + (unsigned int)(4));
        xsg_ee_set32(&r_1, r_1.w[0] + (unsigned int)(4));
        if (xsg_ee_zero.d[0] != r_2.d[0]) goto xsg_Lxgl_2_201_0;
        xsg_vu0_blocks++;
        XSG_OUT(destination, r_0);
        XSG_OUT(source, r_1);
        XSG_OUT(count, r_2);
    }
#endif
}

void xglMemCopy4b(void *destination, const void *source, int count)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "1:\n\t"
        "lw $2,0(%1)\n\t"
        "sw $2,0(%0)\n\t"
        "addi %2,%2,-1\n\t"
        "addi %0,%0,-4\n\t"
        "addi %1,%1,-4\n\t"
        "bne $0,%2,1b\n\t"
        "nop"
        : "+r"(destination), "+r"(source), "+r"(count)
        :
        : "$2", "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(count);
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        xsg_Lxgl_2_217_0:;
        { int xsg_v_; __builtin_memcpy(&xsg_v_, XSG_EA(r_1, 0), sizeof xsg_v_); xsg_ee_gpr[2].sd[0] = xsg_v_; }
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[2], 4);
        xsg_ee_set32(&r_2, r_2.w[0] + (unsigned int)(-1));
        xsg_ee_set32(&r_0, r_0.w[0] + (unsigned int)(-4));
        xsg_ee_set32(&r_1, r_1.w[0] + (unsigned int)(-4));
        if (xsg_ee_zero.d[0] != r_2.d[0]) goto xsg_Lxgl_2_217_0;
        xsg_vu0_blocks++;
        XSG_OUT(destination, r_0);
        XSG_OUT(source, r_1);
        XSG_OUT(count, r_2);
    }
#endif
}

void xglMemCopy2(void *destination, const void *source, int count)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "1:\n\t"
        "lh $2,0(%1)\n\t"
        "sh $2,0(%0)\n\t"
        "addi %2,%2,-1\n\t"
        "addi %0,%0,2\n\t"
        "addi %1,%1,2\n\t"
        "bne $0,%2,1b\n\t"
        "nop"
        : "+r"(destination), "+r"(source), "+r"(count)
        :
        : "$2", "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(count);
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        xsg_Lxgl_2_233_0:;
        { short xsg_v_; __builtin_memcpy(&xsg_v_, XSG_EA(r_1, 0), sizeof xsg_v_); xsg_ee_gpr[2].sd[0] = xsg_v_; }
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[2], 2);
        xsg_ee_set32(&r_2, r_2.w[0] + (unsigned int)(-1));
        xsg_ee_set32(&r_0, r_0.w[0] + (unsigned int)(2));
        xsg_ee_set32(&r_1, r_1.w[0] + (unsigned int)(2));
        if (xsg_ee_zero.d[0] != r_2.d[0]) goto xsg_Lxgl_2_233_0;
        xsg_vu0_blocks++;
        XSG_OUT(destination, r_0);
        XSG_OUT(source, r_1);
        XSG_OUT(count, r_2);
    }
#endif
}

void xglMemCopy2b(void *destination, const void *source, int count)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "1:\n\t"
        "lh $2,0(%1)\n\t"
        "sh $2,0(%0)\n\t"
        "addi %2,%2,-1\n\t"
        "addi %0,%0,-2\n\t"
        "addi %1,%1,-2\n\t"
        "bne $0,%2,1b\n\t"
        "nop"
        : "+r"(destination), "+r"(source), "+r"(count)
        :
        : "$2", "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(count);
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        xsg_Lxgl_2_249_0:;
        { short xsg_v_; __builtin_memcpy(&xsg_v_, XSG_EA(r_1, 0), sizeof xsg_v_); xsg_ee_gpr[2].sd[0] = xsg_v_; }
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[2], 2);
        xsg_ee_set32(&r_2, r_2.w[0] + (unsigned int)(-1));
        xsg_ee_set32(&r_0, r_0.w[0] + (unsigned int)(-2));
        xsg_ee_set32(&r_1, r_1.w[0] + (unsigned int)(-2));
        if (xsg_ee_zero.d[0] != r_2.d[0]) goto xsg_Lxgl_2_249_0;
        xsg_vu0_blocks++;
        XSG_OUT(destination, r_0);
        XSG_OUT(source, r_1);
        XSG_OUT(count, r_2);
    }
#endif
}

void xglVectorDiv(Vector4 *destination, const Vector4 *numerator,
                  const Vector4 *divisor)
{
    destination->x = numerator->x / divisor->x;
    destination->y = numerator->y / divisor->y;
    destination->z = numerator->z / divisor->z;
    destination->w = numerator->w / divisor->w;
}

void xglVectorDivXYZ(Vector4 *destination, const Vector4 *numerator,
                     const Vector4 *divisor)
{
    destination->x = numerator->x / divisor->x;
    destination->y = numerator->y / divisor->y;
    destination->z = numerator->z / divisor->z;
    destination->w = numerator->w;
}

void xglVectorMulAdd(Vector4 *destination, const Vector4 *left,
                     const Vector4 *right, const Vector4 *addend)
{
    destination->x = left->x * right->x + addend->x;
    destination->y = left->y * right->y + addend->y;
    destination->z = left->z * right->z + addend->z;
    destination->w = left->w * right->w + addend->w;
}

void xglVectorMulAddXYZ(Vector4 *destination, const Vector4 *left,
                        const Vector4 *right, const Vector4 *addend)
{
    destination->x = left->x * right->x + addend->x;
    destination->y = left->y * right->y + addend->y;
    destination->z = left->z * right->z + addend->z;
    destination->w = left->w;
}

void xglVectorMulSub(Vector4 *destination, const Vector4 *left,
                     const Vector4 *right, const Vector4 *subtrahend)
{
    destination->x = left->x * right->x - subtrahend->x;
    destination->y = left->y * right->y - subtrahend->y;
    destination->z = left->z * right->z - subtrahend->z;
    destination->w = left->w * right->w - subtrahend->w;
}

void xglVectorMulSubXYZ(Vector4 *destination, const Vector4 *left,
                        const Vector4 *right, const Vector4 *subtrahend)
{
    destination->x = left->x * right->x - subtrahend->x;
    destination->y = left->y * right->y - subtrahend->y;
    destination->z = left->z * right->z - subtrahend->z;
    destination->w = left->w;
}

void xglVectorDivAdd(Vector4 *destination, const Vector4 *numerator,
                     const Vector4 *divisor, const Vector4 *addend)
{
    destination->x = numerator->x / divisor->x + addend->x;
    destination->y = numerator->y / divisor->y + addend->y;
    destination->z = numerator->z / divisor->z + addend->z;
    destination->w = numerator->w / divisor->w + addend->w;
}

void xglVectorDivAddXYZ(Vector4 *destination, const Vector4 *numerator,
                        const Vector4 *divisor, const Vector4 *addend)
{
    destination->x = numerator->x / divisor->x + addend->x;
    destination->y = numerator->y / divisor->y + addend->y;
    destination->z = numerator->z / divisor->z + addend->z;
    destination->w = numerator->w;
}

void xglVectorDivSub(Vector4 *destination, const Vector4 *numerator,
                     const Vector4 *divisor, const Vector4 *subtrahend)
{
    destination->x = numerator->x / divisor->x - subtrahend->x;
    destination->y = numerator->y / divisor->y - subtrahend->y;
    destination->z = numerator->z / divisor->z - subtrahend->z;
    destination->w = numerator->w / divisor->w - subtrahend->w;
}

void xglVectorDivSubXYZ(Vector4 *destination, const Vector4 *numerator,
                        const Vector4 *divisor, const Vector4 *subtrahend)
{
    destination->x = numerator->x / divisor->x - subtrahend->x;
    destination->y = numerator->y / divisor->y - subtrahend->y;
    destination->z = numerator->z / divisor->z - subtrahend->z;
    destination->w = numerator->w;
}

void xglVectorScale(float scale, Vector4 *destination, const Vector4 *source)
{
    destination->x = source->x * scale;
    destination->y = source->y * scale;
    destination->z = source->z * scale;
    destination->w = source->w * scale;
}

void xglVectorScaleXYZ(float scale, Vector4 *destination, const Vector4 *source)
{
    destination->x = source->x * scale;
    destination->y = source->y * scale;
    destination->z = source->z * scale;
    destination->w = source->w;
}

void xglVectorScaleAdd(float scale, Vector4 *destination,
                       const Vector4 *source, const Vector4 *other)
{
    destination->x = source->x * scale + other->x;
    destination->y = source->y * scale + other->y;
    destination->z = source->z * scale + other->z;
    destination->w = source->w * scale + other->w;
}

void xglVectorScaleAddXYZ(float scale, Vector4 *destination,
                          const Vector4 *source, const Vector4 *other)
{
    destination->x = source->x * scale + other->x;
    destination->y = source->y * scale + other->y;
    destination->z = source->z * scale + other->z;
    destination->w = source->w;
}

void xglVectorScaleSub(float scale, Vector4 *destination,
                       const Vector4 *source, const Vector4 *other)
{
    destination->x = source->x * scale - other->x;
    destination->y = source->y * scale - other->y;
    destination->z = source->z * scale - other->z;
    destination->w = source->w * scale - other->w;
}

void xglVectorScaleSubXYZ(float scale, Vector4 *destination,
                          const Vector4 *source, const Vector4 *other)
{
    destination->x = source->x * scale - other->x;
    destination->y = source->y * scale - other->y;
    destination->z = source->z * scale - other->z;
    destination->w = source->w;
}

void xglVectorInter(float fraction, Vector4 *destination,
                    const Vector4 *start, const Vector4 *end)
{
    destination->x = (end->x - start->x) * fraction + start->x;
    destination->y = (end->y - start->y) * fraction + start->y;
    destination->z = (end->z - start->z) * fraction + start->z;
    destination->w = (end->w - start->w) * fraction + start->w;
}

void xglVectorInterXYZ(float fraction, Vector4 *destination,
                       const Vector4 *start, const Vector4 *end)
{
    destination->x = (end->x - start->x) * fraction + start->x;
    destination->y = (end->y - start->y) * fraction + start->y;
    destination->z = (end->z - start->z) * fraction + start->z;
    destination->w = end->w;
}

void xglVectorInner(float *result, const Vector4 *left, const Vector4 *right)
{
    float dot = left->x * right->x;
    *result = dot;
    dot += left->y * right->y;
    *result = dot;
    dot += left->z * right->z;
    *result = dot;
}

float xglVectorInner4(const Vector4 *left, const Vector4 *right)
{
    return left->x * right->x + left->y * right->y
         + left->z * right->z + left->w * right->w;
}

void xglVectorOuter(Vector4 *destination, const Vector4 *left,
                    const Vector4 *right)
{
    destination->x = left->y * right->z - left->z * right->y;
    destination->y = left->z * right->x - left->x * right->z;
    destination->z = left->x * right->y - left->y * right->x;
    destination->w = left->w;
}

void xglVectorLength(float *destination, const Vector4 *vector)
{
    register float length PS2_REG("$2");

#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf3, 0(%1)\n\t"
        "vmul.xyz vf3xyz, vf3xyz, vf3xyz\n\t"
        "vaddy.x vf3x, vf3x, vf3y\n\t"
        "vaddz.x vf3x, vf3x, vf3z\n\t"
        "vsqrt Q, vf3x\n\t"
        "vwaitq\n\t"
        "vaddq.x vf4x, vf0x, Q\n\t"
        "qmfc2 %0, vf4"
        : "=r"(length)
        : "r"(vector)
        : "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(vector);
        vu0_lqc2(3, XSG_EA(r_1, 0));
        vu0_arith(VU0_MUL, 14, 3, 3, 3, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 3, 3, 3, 1);
        vu0_arith(VU0_ADD, 8, 3, 3, 3, 2);
        vu0_sqrt(3, 0);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 4, 0, 0, VU0_BQ);
        vu0_qmfc2(&xsg_ee_gpr[2], 4);
        xsg_vu0_blocks++;
        XSG_OUT(length, xsg_ee_gpr[2]);
    }
#endif
    *destination = length;
#ifndef PORT_HOST
    PS2_ASMV("" : : "r"(destination));
#else
    {
        XsgGpr r_0 = XSG_IN(destination);
        xsg_vu0_blocks++;
    }
#endif
}

float xglPointLength(const Vector4 *point1, const Vector4 *point2)
{
    float length;

#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf2, 0(%1)\n\t"
        "lqc2 vf3, 0(%2)\n\t"
        "vsub.xyz vf2xyz, vf2xyz, vf3xyz\n\t"
        "vmul.xyz vf3xyz, vf2xyz, vf2xyz\n\t"
        "vaddy.x vf3x, vf3x, vf3y\n\t"
        "vaddz.x vf3x, vf3x, vf3z\n\t"
        "vsqrt Q, vf3x\n\t"
        "vwaitq\n\t"
        "vaddq.x vf4x, vf0x, Q\n\t"
        "qmfc2 %0, vf4"
        : "=r"(length)
        : "r"(point1), "r"(point2)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(point2);
        XsgGpr r_1 = XSG_IN(point1);
        XsgGpr r_0 = {{0, 0}};
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_lqc2(3, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 14, 2, 2, 3, VU0_VEC);
        vu0_arith(VU0_MUL, 14, 3, 2, 2, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 3, 3, 3, 1);
        vu0_arith(VU0_ADD, 8, 3, 3, 3, 2);
        vu0_sqrt(3, 0);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 4, 0, 0, VU0_BQ);
        vu0_qmfc2(&r_0, 4);
        xsg_vu0_blocks++;
        XSG_OUT(length, r_0);
    }
#endif
    return length;
}

float xglPointLengthXZ(const Vector4 *point1, const Vector4 *point2)
{
    float length;

#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf2, 0(%1)\n\t"
        "lqc2 vf3, 0(%2)\n\t"
        "vsub.xyz vf2xyz, vf2xyz, vf3xyz\n\t"
        "vmul.xyz vf3xyz, vf2xyz, vf2xyz\n\t"
        "vaddz.x vf3x, vf3x, vf3z\n\t"
        "vsqrt Q, vf3x\n\t"
        "vwaitq\n\t"
        "vaddq.x vf4x, vf0x, Q\n\t"
        "qmfc2 %0, vf4"
        : "=r"(length)
        : "r"(point1), "r"(point2)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(point2);
        XsgGpr r_1 = XSG_IN(point1);
        XsgGpr r_0 = {{0, 0}};
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_lqc2(3, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 14, 2, 2, 3, VU0_VEC);
        vu0_arith(VU0_MUL, 14, 3, 2, 2, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 3, 3, 3, 2);
        vu0_sqrt(3, 0);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 4, 0, 0, VU0_BQ);
        vu0_qmfc2(&r_0, 4);
        xsg_vu0_blocks++;
        XSG_OUT(length, r_0);
    }
#endif
    return length;
}

void xglVectorNormal(Vector4 *destination, const Vector4 *source)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf2, 0(%1)\n\t"
        "vmul.xyz vf3xyz, vf2xyz, vf2xyz\n\t"
        "vaddy.x vf3x, vf3x, vf3y\n\t"
        "vaddz.x vf3x, vf3x, vf3z\n\t"
        "vrsqrt Q, vf0w, vf3x\n\t"
        "vwaitq\n\t"
        "vmulq.xyz vf2xyz, vf2xyz, Q\n\t"
        "sqc2 vf2, 0(%0)\n\t"
        "nop\n\t"
        :
        : "r"(destination), "r"(source)
        : "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_arith(VU0_MUL, 14, 3, 2, 2, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 3, 3, 3, 1);
        vu0_arith(VU0_ADD, 8, 3, 3, 3, 2);
        vu0_rsqrt(0, 3, 3, 0);
        vu0_waitq();
        vu0_arith(VU0_MUL, 14, 2, 2, 0, VU0_BQ);
        vu0_sqc2(2, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
}

void xglVectorClamp(Vector4 *destination, const Vector4 *source,
                    float minimum, float maximum)
{
    destination->x = source->x < minimum ? minimum
                    : maximum < source->x ? maximum : source->x;
    destination->y = source->y < minimum ? minimum
                    : maximum < source->y ? maximum : source->y;
    destination->z = source->z < minimum ? minimum
                    : maximum < source->z ? maximum : source->z;
    destination->w = source->w < minimum ? minimum
                    : maximum < source->w ? maximum : source->w;
}

void xglVectorClampXYZ(Vector4 *destination, const Vector4 *source,
                       float minimum, float maximum)
{
    destination->x = source->x < minimum ? minimum
                    : maximum < source->x ? maximum : source->x;
    destination->y = source->y < minimum ? minimum
                    : maximum < source->y ? maximum : source->y;
    destination->z = source->z < minimum ? minimum
                    : maximum < source->z ? maximum : source->z;
    destination->w = source->w;
}

void xglPlaneParameter(Vector4 *destination, const Vector4 *p0,
                       const Vector4 *p1, const Vector4 *p2)
{
    Vector4 edge0;
    Vector4 edge1;

#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf3, 0(%1)\n\t"
        "lqc2 vf2, 0(%2)\n\t"
        "vsub.xyz vf2xyz, vf2xyz, vf3xyz\n\t"
        "sqc2 vf2, 0(%0)\n\t"
        :
        : "r"(&edge0), "r"(p0), "r"(p1)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(p1);
        XsgGpr r_1 = XSG_IN(p0);
        XsgGpr r_0 = XSG_IN(&edge0);
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
        "vsub.xyz vf2xyz, vf2xyz, vf3xyz\n\t"
        "sqc2 vf2, 0(%0)\n\t"
        :
        : "r"(&edge1), "r"(p1), "r"(p2)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(p2);
        XsgGpr r_1 = XSG_IN(p1);
        XsgGpr r_0 = XSG_IN(&edge1);
        vu0_lqc2(3, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 14, 2, 2, 3, VU0_VEC);
        vu0_sqc2(2, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif

    xglVectorOuter(destination, &edge0, &edge1);
    xglVectorNormal(destination, destination);
    xglVectorInner(&destination->w, destination, p0);
    destination->w = -destination->w;
}

void xglVectorMulMat(Vector4 *destination, const Matrix4 matrix,
                     const Vector4 *vector)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1, 0(%2)\n\t"
        "lqc2 vf27, 0(%1)\n\t"
        "lqc2 vf28, 16(%1)\n\t"
        "lqc2 vf29, 32(%1)\n\t"
        "lqc2 vf30, 48(%1)\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf1x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf1y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf1z\n\t"
        "vmaddw.xyzw vf31xyzw, vf30xyzw, vf1w\n\t"
        "sqc2 vf31, 0(%0)\n\t"
        "nop\n\t"
        :
        : "r"(destination), "r"(matrix), "r"(vector)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(vector);
        XsgGpr r_1 = XSG_IN(matrix);
        XsgGpr r_0 = XSG_IN(destination);
        vu0_lqc2(1, XSG_EA(r_2, 0));
        vu0_lqc2(27, XSG_EA(r_1, 0));
        vu0_lqc2(28, XSG_EA(r_1, 16));
        vu0_lqc2(29, XSG_EA(r_1, 32));
        vu0_lqc2(30, XSG_EA(r_1, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 27, 1, 0);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 28, 1, 1);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 29, 1, 2);
        vu0_arith(VU0_MADD, 15, 31, 30, 1, 3);
        vu0_sqc2(31, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
}

float xglRotTransPers(Vector4 *out, const Matrix4 matrix, Vector4 *point, int cameraId)
{
    StudioCamera *camera;

    xglMatrixStackPush();
    camera = xglStudioGetCamera2(cameraId);
    xglMatrixStackLoad(camera->viewMatrix);
    if (matrix != 0) {
        xglMatrixStackMul(matrix);
    }
    xglMatrixStackRTPS(out, point, &camera->screenScale, &camera->screenOffset);
    xglMatrixStackPop(1);
    return out->w;
}

void xglRotTransPersN(Vector4 *destination, const Matrix4 matrix,
                      const Vector4 *points, int count, int cameraId)
{
    StudioCamera *camera;

    xglMatrixStackPush();
    camera = xglStudioGetCamera2(cameraId);
    xglMatrixStackLoad(camera->viewMatrix);
    if (matrix != 0) {
        xglMatrixStackMul(matrix);
    }
    while (count > 0) {
        xglMatrixStackRTPS(destination, points,
                           &camera->screenScale, &camera->screenOffset);
        ++destination;
        ++points;
        --count;
    }
    xglMatrixStackPop(1);
}

void xglMatrixUnit(float matrix[4][4])
{
    /* Original local unit matrix: four quadword loads consume all 64 bytes. */
    static Matrix4 unit = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}
    };

#ifndef PORT_HOST
    PS2_ASMV(
        "lq $2,0(%1)\n\t"
        "sq $2,0(%0)\n\t"
        "lq $2,16(%1)\n\t"
        "sq $2,16(%0)\n\t"
        "lq $2,32(%1)\n\t"
        "sq $2,32(%0)\n\t"
        "lq $2,48(%1)\n\t"
        "sq $2,48(%0)"
        :
        : "r"(matrix), "r"(unit)
        : "$2", "memory");
#else
    {
        XsgGpr r_1 = XSG_IN(unit);
        XsgGpr r_0 = XSG_IN(matrix);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 0), 16);
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 16), 16);
        __builtin_memcpy(XSG_EA(r_0, 16), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 32), 16);
        __builtin_memcpy(XSG_EA(r_0, 32), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 48), 16);
        __builtin_memcpy(XSG_EA(r_0, 48), &xsg_ee_gpr[2], 16);
        xsg_vu0_blocks++;
    }
#endif
}

extern float unit_1[16];

void xglMatrixUnit4s(float matrix[4][4])
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lq $2, 0x0(%0)\n\t"
        "sq $2, 0x0(%1)\n\t"
        "lq $2, 0x10(%0)\n\t"
        "sq $2, 0x10(%1)\n\t"
        "lq $2, 0x20(%0)\n\t"
        "sq $2, 0x20(%1)\n\t"
        "lq $2, 0x30(%0)\n\t"
        "sq $2, 0x30(%1)\n\t"
        :
        : "r"(unit_1), "r"(matrix)
        : "$2", "memory");
#else
    {
        XsgGpr r_1 = XSG_IN(matrix);
        XsgGpr r_0 = XSG_IN(unit_1);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_0, 0), 16);
        __builtin_memcpy(XSG_EA(r_1, 0), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_0, 16), 16);
        __builtin_memcpy(XSG_EA(r_1, 16), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_0, 32), 16);
        __builtin_memcpy(XSG_EA(r_1, 32), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_0, 48), 16);
        __builtin_memcpy(XSG_EA(r_1, 48), &xsg_ee_gpr[2], 16);
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixMul(Matrix *destination, Matrix *left, Matrix *right)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf2, 0(%2)\n\t"
        "lqc2 vf3, 16(%2)\n\t"
        "lqc2 vf4, 32(%2)\n\t"
        "lqc2 vf5, 48(%2)\n\t"
        "lqc2 vf27, 0(%1)\n\t"
        "lqc2 vf28, 16(%1)\n\t"
        "lqc2 vf29, 32(%1)\n\t"
        "lqc2 vf30, 48(%1)\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf2x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf2y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf2z\n\t"
        "vmaddw.xyzw vf2xyzw, vf30xyzw, vf2w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf3x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf3y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf3z\n\t"
        "vmaddw.xyzw vf3xyzw, vf30xyzw, vf3w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf4x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf4y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf4z\n\t"
        "vmaddw.xyzw vf4xyzw, vf30xyzw, vf4w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf5x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf5y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf5z\n\t"
        "vmaddw.xyzw vf30xyzw, vf30xyzw, vf5w\n\t"
        "sqc2 vf2, 0(%0)\n\t"
        "sqc2 vf3, 16(%0)\n\t"
        "sqc2 vf4, 32(%0)\n\t"
        "sqc2 vf30, 48(%0)\n\t"
        "nop\n\t"
        :
        : "r"(destination), "r"(left), "r"(right)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(right);
        XsgGpr r_1 = XSG_IN(left);
        XsgGpr r_0 = XSG_IN(destination);
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_lqc2(3, XSG_EA(r_2, 16));
        vu0_lqc2(4, XSG_EA(r_2, 32));
        vu0_lqc2(5, XSG_EA(r_2, 48));
        vu0_lqc2(27, XSG_EA(r_1, 0));
        vu0_lqc2(28, XSG_EA(r_1, 16));
        vu0_lqc2(29, XSG_EA(r_1, 32));
        vu0_lqc2(30, XSG_EA(r_1, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 27, 2, 0);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 28, 2, 1);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 29, 2, 2);
        vu0_arith(VU0_MADD, 15, 2, 30, 2, 3);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 27, 3, 0);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 28, 3, 1);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 29, 3, 2);
        vu0_arith(VU0_MADD, 15, 3, 30, 3, 3);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 27, 4, 0);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 28, 4, 1);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 29, 4, 2);
        vu0_arith(VU0_MADD, 15, 4, 30, 4, 3);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 27, 5, 0);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 28, 5, 1);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 29, 5, 2);
        vu0_arith(VU0_MADD, 15, 30, 30, 5, 3);
        vu0_sqc2(2, XSG_EA(r_0, 0));
        vu0_sqc2(3, XSG_EA(r_0, 16));
        vu0_sqc2(4, XSG_EA(r_0, 32));
        vu0_sqc2(30, XSG_EA(r_0, 48));
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixReverse(Matrix *destination, const Matrix *source)
{
    Matrix transposed;
    int row;
    int column;

    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 4; ++column) {
            transposed.elements[row * 4 + column] =
                source->elements[column * 4 + row];
        }
    }
    /* The original SDK copy reuses scratch $2 for all four rows. */
#ifndef PORT_HOST
    PS2_ASMV(
        "lq $2,0(%1)\n\t"
        "sq $2,0(%0)\n\t"
        "lq $2,16(%1)\n\t"
        "sq $2,16(%0)\n\t"
        "lq $2,32(%1)\n\t"
        "sq $2,32(%0)\n\t"
        "lq $2,48(%1)\n\t"
        "sq $2,48(%0)"
        :
        : "r"(destination), "r"(&transposed)
        : "$2", "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(&transposed);
        XsgGpr r_0 = XSG_IN(destination);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 0), 16);
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 16), 16);
        __builtin_memcpy(XSG_EA(r_0, 16), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 32), 16);
        __builtin_memcpy(XSG_EA(r_0, 32), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 48), 16);
        __builtin_memcpy(XSG_EA(r_0, 48), &xsg_ee_gpr[2], 16);
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixInverse(Matrix4 d, const Matrix4 s)
{
    float a00 = s[0][0];
    float a11 = s[1][1];
    float a22 = s[2][2];
    float a12 = s[1][2];
    float a21 = s[2][1];
    float a20 = s[2][0];
    float a10 = s[1][0];
    float a01 = s[0][1];
    float a02 = s[0][2];
    float a30 = s[3][0];
    float a31 = s[3][1];
    float a32 = s[3][2];
    float pad[16];
    float c00, c01, c02, c10, c11, c12, c20, c21, c22, inv;

    d[3][3] = 1.0f;
    d[0][3] = 0.0f;
    d[1][3] = 0.0f;
    d[2][3] = 0.0f;
    c00 = a11 * a22 - a12 * a21;
    c01 = a21 * a02 - a22 * a01;
    c02 = a01 * a12 - a02 * a11;
    c10 = a12 * a20 - a10 * a22;
    c11 = a22 * a00 - a20 * a02;
    c12 = a02 * a10 - a00 * a12;
    c20 = a10 * a21 - a11 * a20;
    c21 = a20 * a01 - a21 * a00;
    c22 = a00 * a11 - a01 * a10;
    inv = a00 * c00 + a01 * c10;
    inv += a02 * c20;
    inv = 1.0f / inv;
    d[0][0] = c00 * inv;
    d[0][1] = c01 * inv;
    d[0][2] = c02 * inv;
    d[1][0] = c10 * inv;
    d[1][1] = c11 * inv;
    d[1][2] = c12 * inv;
    d[2][0] = c20 * inv;
    d[2][1] = c21 * inv;
    d[2][2] = c22 * inv;
    inv = -inv;
    d[3][0] = (c00 * a30 + c10 * a31 + c20 * a32) * inv;
    d[3][1] = (c01 * a30 + c11 * a31 + c21 * a32) * inv;
    d[3][2] = (c02 * a30 + c12 * a31 + c22 * a32) * inv;
}

void xglMatrixScale(Matrix4 destination, const Matrix4 source,
                    const float scale[4])
{
    int column;

    for (column = 0; column < 4; ++column) {
        destination[0][column] = source[0][column] * scale[0];
        destination[1][column] = source[1][column] * scale[1];
        destination[2][column] = source[2][column] * scale[2];
        destination[3][column] = source[3][column] * scale[3];
    }
}

void xglMatrixTrans(Matrix4 destination, const Matrix4 source,
                    const float translation[4])
{
    int column;

    for (column = 0; column < 4; ++column) {
        destination[0][column] = source[0][column];
        destination[1][column] = source[1][column];
        destination[2][column] = source[2][column];
        destination[3][column] = source[0][column] * translation[0]
                                + source[1][column] * translation[1]
                                + source[2][column] * translation[2]
                                + source[3][column] * translation[3];
    }
}

void xglMatrixRotV(Matrix *destination, Matrix *source, Vector4 *axis,
                   float angle)
{
    Vector4 oneMinusCosAxis;
    Vector4 sinAxis;
    Matrix rotation;
    float cosineForScale;
    float sine;
    float cosineForRow0;
    float cosineForRow1;
    float cosineForRow2;
    float ax;
    float ay;
    float az;
    float oneMinusCosX;
    float oneMinusCosY;
    float oneMinusCosZ;
    float sinX;
    float sinY;
    float sinZ;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %1,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %2,vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i %0,vf1"
        : "=r"(cosineForScale)
        : "r"((unsigned int)Vu0CallCos / 8), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN((unsigned int)Vu0CallCos / 8);
        XsgGpr r_0 = {{0, 0}};
        vu0_ctc2(27, &r_1);
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(cosineForScale, r_0);
    }
#endif
    xglVectorScaleXYZ(1.0f - cosineForScale, &oneMinusCosAxis, axis);

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %1,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %2,vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i %0,vf1"
        : "=r"(sine)
        : "r"((unsigned int)Vu0CallSin / 8), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN((unsigned int)Vu0CallSin / 8);
        XsgGpr r_0 = {{0, 0}};
        vu0_ctc2(27, &r_1);
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(sine, r_0);
    }
#endif
    xglVectorScaleXYZ(sine, &sinAxis, axis);

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %1,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %2,vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i %0,vf1"
        : "=r"(cosineForRow0)
        : "r"((unsigned int)Vu0CallCos / 8), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN((unsigned int)Vu0CallCos / 8);
        XsgGpr r_0 = {{0, 0}};
        vu0_ctc2(27, &r_1);
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(cosineForRow0, r_0);
    }
#endif
    ax = axis->x;
    oneMinusCosX = oneMinusCosAxis.x;
    oneMinusCosY = oneMinusCosAxis.y;
    oneMinusCosZ = oneMinusCosAxis.z;
    ay = axis->y;
    sinY = sinAxis.y;
    sinZ = sinAxis.z;
    rotation.elements[0] = oneMinusCosX * ax + cosineForRow0;
    rotation.elements[1] = oneMinusCosY * ax + sinZ;
    rotation.elements[2] = oneMinusCosZ * ax - sinY;
    rotation.elements[3] = 0.0f;
    rotation.elements[4] = oneMinusCosX * ay - sinZ;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %1,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %2,vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i %0,vf1"
        : "=r"(cosineForRow1)
        : "r"((unsigned int)Vu0CallCos / 8), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN((unsigned int)Vu0CallCos / 8);
        XsgGpr r_0 = {{0, 0}};
        vu0_ctc2(27, &r_1);
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(cosineForRow1, r_0);
    }
#endif
    sinX = sinAxis.x;
    az = axis->z;
    rotation.elements[5] = oneMinusCosY * ay + cosineForRow1;
    rotation.elements[6] = oneMinusCosZ * ay + sinX;
    rotation.elements[7] = 0.0f;
    rotation.elements[8] = oneMinusCosX * az + sinY;
    rotation.elements[9] = oneMinusCosY * az - sinX;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %1,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %2,vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i %0,vf1"
        : "=r"(cosineForRow2)
        : "r"((unsigned int)Vu0CallCos / 8), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN((unsigned int)Vu0CallCos / 8);
        XsgGpr r_0 = {{0, 0}};
        vu0_ctc2(27, &r_1);
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(cosineForRow2, r_0);
    }
#endif
    rotation.elements[10] = oneMinusCosZ * az + cosineForRow2;
    rotation.elements[11] = 0.0f;
    rotation.elements[12] = 0.0f;
    rotation.elements[13] = 0.0f;
    rotation.elements[14] = 0.0f;
    rotation.elements[15] = 1.0f;

    xglMatrixMul(destination, source, &rotation);
}

void xglMatrixRotX(Matrix4 destination, const Matrix4 source, float angle)
{
    float cosSin[4];
    float *pair = cosSin;
    float sine;
    float cosine;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %1,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %2,vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i %0,vf1"
        : "=r"(sine)
        : "r"((unsigned int)Vu0CallSin / 8), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN((unsigned int)Vu0CallSin / 8);
        XsgGpr r_0 = {{0, 0}};
        vu0_ctc2(27, &r_1);
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(sine, r_0);
    }
#endif
    cosSin[1] = sine;
#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %1,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %2,vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i %0,vf1"
        : "=r"(cosine)
        : "r"((unsigned int)Vu0CallCos / 8), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN((unsigned int)Vu0CallCos / 8);
        XsgGpr r_0 = {{0, 0}};
        vu0_ctc2(27, &r_1);
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(cosine, r_0);
    }
#endif
    cosSin[0] = cosine;

#ifndef PORT_HOST
    PS2_ASMV(
        "lq $2,0(%1)\n\t"
        "lqc2 vf28,16(%1)\n\t"
        "lqc2 vf29,32(%1)\n\t"
        "lqc2 vf1,0(%2)\n\t"
        "sq $2,0(%0)\n\t"
        "lq $2,48(%1)\n\t"
        "sq $2,48(%0)\n\t"
        "vmulax.xyzw ACCxyzw,vf28xyzw,vf1x\n\t"
        "vmaddy.xyzw vf27xyzw,vf29xyzw,vf1y\n\t"
        "vmulax.xyzw ACCxyzw,vf29xyzw,vf1x\n\t"
        "vmsuby.xyzw vf30xyzw,vf28xyzw,vf1y\n\t"
        "sqc2 vf27,16(%0)\n\t"
        "sqc2 vf30,32(%0)"
        :
        : "r"(destination), "r"(source), "r"(pair)
        : "$2", "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(pair);
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 0), 16);
        vu0_lqc2(28, XSG_EA(r_1, 16));
        vu0_lqc2(29, XSG_EA(r_1, 32));
        vu0_lqc2(1, XSG_EA(r_2, 0));
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 48), 16);
        __builtin_memcpy(XSG_EA(r_0, 48), &xsg_ee_gpr[2], 16);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 28, 1, 0);
        vu0_arith(VU0_MADD, 15, 27, 29, 1, 1);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 29, 1, 0);
        vu0_arith(VU0_MSUB, 15, 30, 28, 1, 1);
        vu0_sqc2(27, XSG_EA(r_0, 16));
        vu0_sqc2(30, XSG_EA(r_0, 32));
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixRotY(Matrix4 destination, const Matrix4 source, float angle)
{
    float cosSin[4];
    float *pair = cosSin;
    float sine;
    float cosine;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %1,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %2,vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i %0,vf1"
        : "=r"(sine)
        : "r"((unsigned int)Vu0CallSin / 8), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN((unsigned int)Vu0CallSin / 8);
        XsgGpr r_0 = {{0, 0}};
        vu0_ctc2(27, &r_1);
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(sine, r_0);
    }
#endif
    cosSin[1] = sine;
#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %1,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %2,vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i %0,vf1"
        : "=r"(cosine)
        : "r"((unsigned int)Vu0CallCos / 8), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN((unsigned int)Vu0CallCos / 8);
        XsgGpr r_0 = {{0, 0}};
        vu0_ctc2(27, &r_1);
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(cosine, r_0);
    }
#endif
    cosSin[0] = cosine;

#ifndef PORT_HOST
    PS2_ASMV(
        "lq $2,16(%1)\n\t"
        "lqc2 vf27,0(%1)\n\t"
        "lqc2 vf29,32(%1)\n\t"
        "lqc2 vf1,0(%2)\n\t"
        "sq $2,16(%0)\n\t"
        "lq $2,48(%1)\n\t"
        "sq $2,48(%0)\n\t"
        "vmulax.xyzw ACCxyzw,vf27xyzw,vf1x\n\t"
        "vmsuby.xyzw vf28xyzw,vf29xyzw,vf1y\n\t"
        "vmulax.xyzw ACCxyzw,vf29xyzw,vf1x\n\t"
        "vmaddy.xyzw vf30xyzw,vf27xyzw,vf1y\n\t"
        "sqc2 vf28,0(%0)\n\t"
        "sqc2 vf30,32(%0)"
        :
        : "r"(destination), "r"(source), "r"(pair)
        : "$2", "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(pair);
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 16), 16);
        vu0_lqc2(27, XSG_EA(r_1, 0));
        vu0_lqc2(29, XSG_EA(r_1, 32));
        vu0_lqc2(1, XSG_EA(r_2, 0));
        __builtin_memcpy(XSG_EA(r_0, 16), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 48), 16);
        __builtin_memcpy(XSG_EA(r_0, 48), &xsg_ee_gpr[2], 16);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 27, 1, 0);
        vu0_arith(VU0_MSUB, 15, 28, 29, 1, 1);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 29, 1, 0);
        vu0_arith(VU0_MADD, 15, 30, 27, 1, 1);
        vu0_sqc2(28, XSG_EA(r_0, 0));
        vu0_sqc2(30, XSG_EA(r_0, 32));
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixRotZ(Matrix4 destination, const Matrix4 source, float angle)
{
    float cosSin[4];
    float *pair = cosSin;
    float sine;
    float cosine;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %1,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %2,vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i %0,vf1"
        : "=r"(sine)
        : "r"((unsigned int)Vu0CallSin / 8), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN((unsigned int)Vu0CallSin / 8);
        XsgGpr r_0 = {{0, 0}};
        vu0_ctc2(27, &r_1);
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(sine, r_0);
    }
#endif
    cosSin[1] = sine;
#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %1,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %2,vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i %0,vf1"
        : "=r"(cosine)
        : "r"((unsigned int)Vu0CallCos / 8), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN((unsigned int)Vu0CallCos / 8);
        XsgGpr r_0 = {{0, 0}};
        vu0_ctc2(27, &r_1);
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(cosine, r_0);
    }
#endif
    cosSin[0] = cosine;

#ifndef PORT_HOST
    PS2_ASMV(
        "lq $2,32(%1)\n\t"
        "lqc2 vf27,0(%1)\n\t"
        "lqc2 vf28,16(%1)\n\t"
        "lqc2 vf1,0(%2)\n\t"
        "sq $2,32(%0)\n\t"
        "lq $2,48(%1)\n\t"
        "sq $2,48(%0)\n\t"
        "vmulax.xyzw ACCxyzw,vf27xyzw,vf1x\n\t"
        "vmaddy.xyzw vf29xyzw,vf28xyzw,vf1y\n\t"
        "vmulax.xyzw ACCxyzw,vf28xyzw,vf1x\n\t"
        "vmsuby.xyzw vf30xyzw,vf27xyzw,vf1y\n\t"
        "sqc2 vf29,0(%0)\n\t"
        "sqc2 vf30,16(%0)"
        :
        : "r"(destination), "r"(source), "r"(pair)
        : "$2", "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(pair);
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 32), 16);
        vu0_lqc2(27, XSG_EA(r_1, 0));
        vu0_lqc2(28, XSG_EA(r_1, 16));
        vu0_lqc2(1, XSG_EA(r_2, 0));
        __builtin_memcpy(XSG_EA(r_0, 32), &xsg_ee_gpr[2], 16);
        __builtin_memcpy(&xsg_ee_gpr[2], XSG_EA(r_1, 48), 16);
        __builtin_memcpy(XSG_EA(r_0, 48), &xsg_ee_gpr[2], 16);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 27, 1, 0);
        vu0_arith(VU0_MADD, 15, 29, 28, 1, 1);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 28, 1, 0);
        vu0_arith(VU0_MSUB, 15, 30, 27, 1, 1);
        vu0_sqc2(29, XSG_EA(r_0, 0));
        vu0_sqc2(30, XSG_EA(r_0, 16));
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixFrustum(Matrix *destination, Matrix *source,
                      float left, float right, float bottom, float top,
                      float nearPlane, float farPlane)
{
    Matrix projection;

    projection.elements[0] = (2.0f * nearPlane) / (right - left);
    projection.elements[1] = 0.0f;
    projection.elements[2] = 0.0f;
    projection.elements[3] = 0.0f;
    projection.elements[4] = 0.0f;
    projection.elements[5] = (2.0f * nearPlane) / (top - bottom);
    projection.elements[6] = 0.0f;
    projection.elements[7] = 0.0f;
    projection.elements[8] = (right + left) / (right - left);
    projection.elements[9] = (top + bottom) / (top - bottom);
    projection.elements[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    projection.elements[11] = -1.0f;
    projection.elements[12] = 0.0f;
    projection.elements[13] = 0.0f;
    projection.elements[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
    projection.elements[15] = 0.0f;
    xglMatrixMul(destination, source, &projection);
}

void xglMatrixStackUnit(void)
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackUnit >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vcallmsr $vi27\n\t"
        "nop"
        :
        : "r"(entry)
        : "memory"
    );
#else
    {
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackMul(const Matrix4 matrix)
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackMul >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "lqc2 $vf4,0(%1)\n\t"
        "lqc2 $vf5,16(%1)\n\t"
        "lqc2 $vf6,32(%1)\n\t"
        "lqc2 $vf7,48(%1)\n\t"
        "vcallmsr $vi27\n\t"
        "nop"
        :
        : "r"(entry), "r"(matrix)
        : "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(matrix);
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_lqc2(4, XSG_EA(r_1, 0));
        vu0_lqc2(5, XSG_EA(r_1, 16));
        vu0_lqc2(6, XSG_EA(r_1, 32));
        vu0_lqc2(7, XSG_EA(r_1, 48));
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackReverse(void)
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackReverse >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vcallmsr $vi27\n\t"
        "nop"
        :
        : "r"(entry)
        : "memory"
    );
#else
    {
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackInverse(void)
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackInverse >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vcallmsr $vi27\n\t"
        "nop"
        :
        : "r"(entry)
        : "memory"
    );
#else
    {
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackScale(const float scale[4])
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackScale >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "lqc2 $vf4,0(%1)\n\t"
        "vcallmsr $vi27\n\t"
        "nop"
        :
        : "r"(entry), "r"(scale)
        : "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(scale);
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_lqc2(4, XSG_EA(r_1, 0));
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackTrans(const float translation[4])
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackTrans >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "lqc2 $vf4,0(%1)\n\t"
        "vcallmsr $vi27\n\t"
        "nop"
        :
        : "r"(entry), "r"(translation)
        : "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(translation);
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_lqc2(4, XSG_EA(r_1, 0));
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackRotV(const float axis[4], float angle)
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackRotV >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "lqc2 $vf5,0(%1)\n\t"
        "qmtc2 %2,$vf4\n\t"
        "vcallmsr $vi27\n\t"
        "nop"
        :
        : "r"(entry), "r"(axis), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN(axis);
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_lqc2(5, XSG_EA(r_1, 0));
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackRotX(float angle)
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackRotX >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %1,$vf4\n\t"
        "vcallmsr $vi27\n\t"
        "nop"
        :
        : "r"(entry), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(angle);
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_qmtc2(4, &r_1);
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackRotY(float angle)
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackRotY >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %1,$vf4\n\t"
        "vcallmsr $vi27\n\t"
        "nop"
        :
        : "r"(entry), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(angle);
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_qmtc2(4, &r_1);
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackRotZ(float angle)
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackRotZ >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %1,$vf4\n\t"
        "vcallmsr $vi27\n\t"
        "nop"
        :
        : "r"(entry), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(angle);
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_qmtc2(4, &r_1);
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackFrustum(float left, float right, float bottom, float top,
                           float nearPlane, float farPlane)
{
    /* the quadword pair in VU0 data memory the microprogram reads */
    volatile float *window = (volatile float *)0x11004800;
    unsigned int entry = (unsigned int)Vu0CallMatrixStackFrustum >> 3;

#ifndef PORT_HOST
    PS2_ASMV("ctc2.i %0,$vi27" : : "r"(entry) : "memory");
#else
    {
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        xsg_vu0_blocks++;
    }
#endif
    window[0] = left;
    window[1] = bottom;
    window[2] = nearPlane;
    window[4] = right;
    window[5] = top;
    window[6] = farPlane;
    ((volatile int *)window)[7] = 0;
    ((volatile int *)window)[3] = 0;
#ifndef PORT_HOST
    PS2_ASMV("sync" : : : "memory");
#else
    {
        xsg_vu0_blocks++;
    }
#endif
#ifndef PORT_HOST
    PS2_ASMV("vcallmsr $vi27\n\t"
                         "nop"
                         : : : "memory");
#else
    {
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackSave(float matrix[4][4])
{
#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi0\n\t"
        "sqc2 $vf28,0(%0)\n\t"
        "sqc2 $vf29,16(%0)\n\t"
        "sqc2 $vf30,32(%0)\n\t"
        "sqc2 $vf31,48(%0)\n\t"
        "nop"
        :
        : "r"(matrix)
        : "memory"
    );
#else
    {
        XsgGpr r_0 = XSG_IN(matrix);
        vu0_ctc2(0, &r_0);
        vu0_sqc2(28, XSG_EA(r_0, 0));
        vu0_sqc2(29, XSG_EA(r_0, 16));
        vu0_sqc2(30, XSG_EA(r_0, 32));
        vu0_sqc2(31, XSG_EA(r_0, 48));
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackLoad(float matrix[4][4])
{
#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi0\n\t"
        "lqc2 $vf28,0(%0)\n\t"
        "lqc2 $vf29,16(%0)\n\t"
        "lqc2 $vf30,32(%0)\n\t"
        "lqc2 $vf31,48(%0)\n\t"
        "nop"
        :
        : "r"(matrix)
        : "memory"
    );
#else
    {
        XsgGpr r_0 = XSG_IN(matrix);
        vu0_ctc2(0, &r_0);
        vu0_lqc2(28, XSG_EA(r_0, 0));
        vu0_lqc2(29, XSG_EA(r_0, 16));
        vu0_lqc2(30, XSG_EA(r_0, 32));
        vu0_lqc2(31, XSG_EA(r_0, 48));
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackPushUnit(void)
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackPushUnit >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vcallmsr $vi27\n\t"
        "nop"
        :
        : "r"(entry)
        : "memory"
    );
#else
    {
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackPush(void)
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackPush >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vcallmsr $vi27\n\t"
        "nop"
        :
        : "r"(entry)
        : "memory"
    );
#else
    {
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackPop(int count)
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackPop >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "ctc2 %1,$vi1\n\t"
        "vcallmsr $vi27\n\t"
        "nop"
        :
        : "r"(entry), "r"(count)
        : "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(count);
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_ctc2(1, &r_1);
        vu0_callmsr();
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackMulVector(Vector4 *out, const Vector4 *vector)
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackMulVector >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "lqc2 $vf4,0(%2)\n\t"
        "vcallmsr $vi27\n\t"
        "cfc2.i $0,$vi0\n\t"
        "sqc2 $vf5,0(%1)\n\t"
        "nop"
        :
        : "r"(entry), "r"(out), "r"(vector)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(vector);
        XsgGpr r_1 = XSG_IN(out);
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_lqc2(4, XSG_EA(r_2, 0));
        vu0_callmsr();
        vu0_cfc2(&xsg_ee_sink, 0);
        vu0_sqc2(5, XSG_EA(r_1, 0));
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrixStackRTPS(Vector4 *out, const Vector4 *point,
                        const Vector4 *scale, const Vector4 *offset)
{
    unsigned int entry = (unsigned int)Vu0CallMatrixStackRTPS >> 3;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %0,$vi27\n\t"
        "lqc2 $vf4,0(%2)\n\t"
        "lqc2 $vf5,0(%3)\n\t"
        "lqc2 $vf6,0(%4)\n\t"
        "vcallmsr $vi27\n\t"
        "cfc2.i $0,$vi0\n\t"
        "sqc2 $vf7,0(%1)\n\t"
        "nop"
        :
        : "r"(entry), "r"(out), "r"(point), "r"(scale), "r"(offset)
        : "memory"
    );
#else
    {
        XsgGpr r_4 = XSG_IN(offset);
        XsgGpr r_3 = XSG_IN(scale);
        XsgGpr r_2 = XSG_IN(point);
        XsgGpr r_1 = XSG_IN(out);
        XsgGpr r_0 = XSG_IN(entry);
        vu0_ctc2(27, &r_0);
        vu0_lqc2(4, XSG_EA(r_2, 0));
        vu0_lqc2(5, XSG_EA(r_3, 0));
        vu0_lqc2(6, XSG_EA(r_4, 0));
        vu0_callmsr();
        vu0_cfc2(&xsg_ee_sink, 0);
        vu0_sqc2(7, XSG_EA(r_1, 0));
        xsg_vu0_blocks++;
    }
#endif
}

void xglMatrix2Quaternion(float quaternion[4], const Matrix *matrix)
{
    float diagonalX = matrix->elements[0];
    float diagonalY = matrix->elements[5];
    float diagonalZ = matrix->elements[10];
    float root;
    float scale;
    int largestAxis;
    int nextAxis;
    int lastAxis;
    int nextAxes[3];
    float *largestComponent;
    float *nextComponent;
    float *lastComponent;
    scale = (diagonalX + diagonalY) + diagonalZ;
    if (scale > 0.0f) {
        scale = __builtin_sqrtf(scale + 1.0f);
        root = 0.5f * scale;
        quaternion[3] = root;
        scale = 0.5f / (root + root);
        quaternion[0] = (matrix->elements[6] - matrix->elements[9]) * scale;
        quaternion[1] = (matrix->elements[8] - matrix->elements[2]) * scale;
        quaternion[2] = (matrix->elements[1] - matrix->elements[4]) * scale;
    } else {
        __builtin_memcpy(nextAxes, D_004D27C8, sizeof(nextAxes));
        largestAxis = 0;
        if (diagonalX < diagonalY) largestAxis = 1;
        if (matrix->elements[5 * largestAxis] < diagonalZ) largestAxis = 2;
        nextAxis = nextAxes[largestAxis];
        lastAxis = nextAxes[nextAxis];
        largestComponent = &quaternion[largestAxis];
        root = 0.5f * __builtin_sqrtf(matrix->elements[5 * largestAxis] - (matrix->elements[5 * nextAxis] + matrix->elements[5 * lastAxis]) + 1.0f);
        *largestComponent = root;
        nextComponent = &quaternion[nextAxis];
        lastComponent = &quaternion[lastAxis];
        scale = 0.5f / (root + root);
        quaternion[3] = (matrix->elements[4 * nextAxis + lastAxis] - matrix->elements[4 * lastAxis + nextAxis]) * scale;
        *nextComponent = (matrix->elements[4 * largestAxis + nextAxis] + matrix->elements[4 * nextAxis + largestAxis]) * scale;
        *lastComponent = (matrix->elements[4 * largestAxis + lastAxis] + matrix->elements[4 * lastAxis + largestAxis]) * scale;
    }
}

void xglQuaternion2Matrix(Matrix4 destination, const Vector4 *quaternion)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf31, 0(%1)\n\t"
        "vaddw.x vf27x, vf0x, vf0w\n\t"
        "vaddw.y vf28y, vf0y, vf0w\n\t"
        "vaddw.z vf29z, vf0z, vf0w\n\t"
        "vadd.xyzw vf14xyzw, vf31xyzw, vf31xyzw\n\t"
        "vsub.w vf27w, vf27w, vf27w\n\t"
        "vsub.w vf28w, vf28w, vf28w\n\t"
        "vsub.w vf29w, vf29w, vf29w\n\t"
        "vmulx.xyzw vf15xyzw, vf31xyzw, vf14x\n\t"
        "vmuly.xyzw vf16xyzw, vf31xyzw, vf14y\n\t"
        "vmulz.xyzw vf17xyzw, vf31xyzw, vf14z\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vaddy.x vf20x, vf15x, vf16y\n\t"
        "vaddz.y vf20y, vf16y, vf17z\n\t"
        "vaddx.z vf20z, vf17z, vf15x\n\t"
        "vnop\n\t"
        "vsubw.z vf27z, vf15z, vf16w\n\t"
        "vaddw.y vf27y, vf15y, vf17w\n\t"
        "vsuby.x vf27x, vf27x, vf20y\n\t"
        "vsubw.x vf28x, vf16x, vf17w\n\t"
        "vaddw.z vf28z, vf16z, vf15w\n\t"
        "vsubz.y vf28y, vf28y, vf20z\n\t"
        "vsubw.y vf29y, vf17y, vf15w\n\t"
        "vaddw.x vf29x, vf17x, vf16w\n\t"
        "vsubx.z vf29z, vf29z, vf20x\n\t"
        "sqc2 vf0, 48(%0)\n\t"
        "sqc2 vf27, 0(%0)\n\t"
        "sqc2 vf28, 16(%0)\n\t"
        "sqc2 vf29, 32(%0)\n\t"
        "nop"
        :
        : "r"(destination), "r"(quaternion)
        : "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(quaternion);
        XsgGpr r_0 = XSG_IN(destination);
        vu0_lqc2(31, XSG_EA(r_1, 0));
        vu0_arith(VU0_ADD, 8, 27, 0, 0, 3);
        vu0_arith(VU0_ADD, 4, 28, 0, 0, 3);
        vu0_arith(VU0_ADD, 2, 29, 0, 0, 3);
        vu0_arith(VU0_ADD, 15, 14, 31, 31, VU0_VEC);
        vu0_arith(VU0_SUB, 1, 27, 27, 27, VU0_VEC);
        vu0_arith(VU0_SUB, 1, 28, 28, 28, VU0_VEC);
        vu0_arith(VU0_SUB, 1, 29, 29, 29, VU0_VEC);
        vu0_arith(VU0_MUL, 15, 15, 31, 14, 0);
        vu0_arith(VU0_MUL, 15, 16, 31, 14, 1);
        vu0_arith(VU0_MUL, 15, 17, 31, 14, 2);
        vu0_arith(VU0_ADD, 8, 20, 15, 16, 1);
        vu0_arith(VU0_ADD, 4, 20, 16, 17, 2);
        vu0_arith(VU0_ADD, 2, 20, 17, 15, 0);
        vu0_arith(VU0_SUB, 2, 27, 15, 16, 3);
        vu0_arith(VU0_ADD, 4, 27, 15, 17, 3);
        vu0_arith(VU0_SUB, 8, 27, 27, 20, 1);
        vu0_arith(VU0_SUB, 8, 28, 16, 17, 3);
        vu0_arith(VU0_ADD, 2, 28, 16, 15, 3);
        vu0_arith(VU0_SUB, 4, 28, 28, 20, 2);
        vu0_arith(VU0_SUB, 4, 29, 17, 15, 3);
        vu0_arith(VU0_ADD, 8, 29, 17, 16, 3);
        vu0_arith(VU0_SUB, 2, 29, 29, 20, 0);
        vu0_sqc2(0, XSG_EA(r_0, 48));
        vu0_sqc2(27, XSG_EA(r_0, 0));
        vu0_sqc2(28, XSG_EA(r_0, 16));
        vu0_sqc2(29, XSG_EA(r_0, 32));
        xsg_vu0_blocks++;
    }
#endif
}

void xglQuaternionInterpolateLinear(Vector4 *destination, const Vector4 *left, const Vector4 *right,
                                    float left_weight, float right_weight)
{
    /* The blend takes the shorter arc: when the dot product of the two
     * quaternions is not positive, right is negated first. The sum is then
     * normalised with the VU0 reciprocal square root. */
#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $3, %3\n\t"
        "mfc1 $7, %4\n\t"
        "qmtc2 $3, vf10\n\t"
        "qmtc2 $7, vf11\n\t"
        "lqc2 vf20, 0(%1)\n\t"
        "lqc2 vf21, 0(%2)\n\t"
        "vmul.xyzw vf18, vf20, vf21\n\t"
        "li.s $f0, 0.0\n\t"
        "vsubw.x vf1, vf0, vf0w\n\t"
        "vaddy.x vf23, vf0, vf18y\n\t"
        "vaddz.x vf24, vf0, vf18z\n\t"
        "vaddw.x vf25, vf0, vf18w\n\t"
        "vmulaw.x ACC, vf18, vf0w\n\t"
        "vmaddaw.x ACC, vf23, vf0w\n\t"
        "vmaddaw.x ACC, vf24, vf0w\n\t"
        "vmaddw.x vf26, vf25, vf0w\n\t"
        "qmfc2 $2, vf26\n\t"
        "mtc1 $2, $f1\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1t 1f\n\t"
        "vmulx.xyzw vf21, vf21, vf1x\n"
        "1:\n\t"
        "vmulx.xyzw vf20, vf20, vf10x\n\t"
        "vmulx.xyzw vf21, vf21, vf11x\n\t"
        "vadd.xyzw vf22, vf20, vf21\n\t"
        "vmul.xyzw vf19, vf22, vf22\n\t"
        "vaddy.x vf23, vf0, vf19y\n\t"
        "vaddz.x vf24, vf0, vf19z\n\t"
        "vaddw.x vf25, vf0, vf19w\n\t"
        "vmulaw.x ACC, vf19, vf0w\n\t"
        "vmaddaw.x ACC, vf23, vf0w\n\t"
        "vmaddaw.x ACC, vf24, vf0w\n\t"
        "vmaddw.x vf26, vf25, vf0w\n\t"
        "vrsqrt Q, vf0w, vf26x\n\t"
        "vwaitq\n\t"
        "vmulq.xyzw vf22, vf22, Q\n\t"
        "sqc2 vf22, 0(%0)\n\t"
        "nop"
        :
        : "r"(destination), "r"(left), "r"(right), "f"(left_weight), "f"(right_weight)
        : "$2", "$3", "$7", "$f0", "$f1", "memory");
#else
    {
        XsgFpr f_4 = XSG_FIN(right_weight);
        XsgFpr f_3 = XSG_FIN(left_weight);
        XsgGpr r_2 = XSG_IN(right);
        XsgGpr r_1 = XSG_IN(left);
        XsgGpr r_0 = XSG_IN(destination);
        xsg_ee_set32(&xsg_ee_gpr[3], f_3.u);
        xsg_ee_set32(&xsg_ee_gpr[7], f_4.u);
        vu0_qmtc2(10, &xsg_ee_gpr[3]);
        vu0_qmtc2(11, &xsg_ee_gpr[7]);
        vu0_lqc2(20, XSG_EA(r_1, 0));
        vu0_lqc2(21, XSG_EA(r_2, 0));
        vu0_arith(VU0_MUL, 15, 18, 20, 21, VU0_VEC);
        xsg_ee_fpr[0].f = 0.0f;
        vu0_arith(VU0_SUB, 8, 1, 0, 0, 3);
        vu0_arith(VU0_ADD, 8, 23, 0, 18, 1);
        vu0_arith(VU0_ADD, 8, 24, 0, 18, 2);
        vu0_arith(VU0_ADD, 8, 25, 0, 18, 3);
        vu0_arith(VU0_MUL, 8, VU0_ACC, 18, 0, 3);
        vu0_arith(VU0_MADD, 8, VU0_ACC, 23, 0, 3);
        vu0_arith(VU0_MADD, 8, VU0_ACC, 24, 0, 3);
        vu0_arith(VU0_MADD, 8, 26, 25, 0, 3);
        vu0_qmfc2(&xsg_ee_gpr[2], 26);
        xsg_ee_fpr[1].u = xsg_ee_gpr[2].w[0];
        xsg_ee_fcc = xsg_ee_fpr[0].f < xsg_ee_fpr[1].f;
        if (xsg_ee_fcc) goto xsg_Lxgl_2_1498_0;
        vu0_arith(VU0_MUL, 15, 21, 21, 1, 0);
        xsg_Lxgl_2_1498_0:;
        vu0_arith(VU0_MUL, 15, 20, 20, 10, 0);
        vu0_arith(VU0_MUL, 15, 21, 21, 11, 0);
        vu0_arith(VU0_ADD, 15, 22, 20, 21, VU0_VEC);
        vu0_arith(VU0_MUL, 15, 19, 22, 22, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 23, 0, 19, 1);
        vu0_arith(VU0_ADD, 8, 24, 0, 19, 2);
        vu0_arith(VU0_ADD, 8, 25, 0, 19, 3);
        vu0_arith(VU0_MUL, 8, VU0_ACC, 19, 0, 3);
        vu0_arith(VU0_MADD, 8, VU0_ACC, 23, 0, 3);
        vu0_arith(VU0_MADD, 8, VU0_ACC, 24, 0, 3);
        vu0_arith(VU0_MADD, 8, 26, 25, 0, 3);
        vu0_rsqrt(0, 3, 26, 0);
        vu0_waitq();
        vu0_arith(VU0_MUL, 15, 22, 22, 0, VU0_BQ);
        vu0_sqc2(22, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
}

void xglRandSeedInit(void)
{
    iRandSeed = 0x12345678ULL;
}

void xglGeometryInit(void)
{
    float random_seed = 0.1f;

#ifndef PORT_HOST
    PS2_ASMV(
        "qmtc2 %0,vf1\n\t"
        "vrinit R,vf1x"
        :
        : "r"(random_seed)
        : "memory"
    );
#else
    {
        XsgGpr r_0 = XSG_IN(random_seed);
        vu0_qmtc2(1, &r_0);
        vu0_rinit(1, 0);
        xsg_vu0_blocks++;
    }
#endif
    iRandSeed = 0x12345678ULL;
    xglDmaDirectSrcChain(0, (unsigned int)D_004ADE80);
}

unsigned short xglSRand(void)
{
    iRandSeed = iRandSeed * 1103515245ULL + 12345ULL;
    return (unsigned short)((iRandSeed >> 16) & 0x7fff);
}

unsigned int xglLRand(void)
{
    int first = iRandSeed * 1103515245LL + 12345LL;
    unsigned long long next = (unsigned int)(first * 1103515245 + 12345);

    iRandSeed = next;
    return (unsigned int)(first << 16) + (unsigned int)(next >> 16);
}

float xglFRand(void)
{
    float value;

#ifndef PORT_HOST
    PS2_ASMV(
        "vrnext.x vf1, R\n\t"
        "qmfc2.ni %0, vf1"
        : "=r"(value)
        :
        : "memory"
    );
#else
    {
        XsgGpr r_0 = {{0, 0}};
        vu0_rnext(8, 1);
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(value, r_0);
    }
#endif
    return value;
}

void xglFSrand(float seed)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "qmtc2.ni %0, vf1\n\t"
        "vrinit R, vf1x\n\t"
        "nop"
        :
        : "r"(seed)
        : "memory"
    );
#else
    {
        XsgGpr r_0 = XSG_IN(seed);
        vu0_qmtc2(1, &r_0);
        vu0_rinit(1, 0);
        xsg_vu0_blocks++;
    }
#endif
}

int F2I(float value)
{
    int result;

#ifndef PORT_HOST
    PS2_ASMV(
        "mtc1 %1,$f0\n\t"
        "cvt.w.s $f0,$f0\n\t"
        "mfc1 %0,$f0"
        : "=r"(result)
        : "r"(value)
        : "$f0");
#else
    {
        XsgGpr r_1 = XSG_IN(value);
        XsgGpr r_0 = {{0, 0}};
        xsg_ee_fpr[0].u = r_1.w[0];
        xsg_ee_fpr[0].i = (int)xsg_ee_fpr[0].f;
        xsg_ee_set32(&r_0, xsg_ee_fpr[0].u);
        xsg_vu0_blocks++;
        XSG_OUT(result, r_0);
    }
#endif
    return result;
}

float I2F(int value)
{
    return (float) value;
}

float xglSin(float angle)
{
    unsigned int entry = (unsigned int)Vu0CallSin >> 3;
    float result;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %1,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %2,$vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i %0,$vf1"
        : "=r"(result)
        : "r"(entry), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN(entry);
        XsgGpr r_0 = {{0, 0}};
        vu0_ctc2(27, &r_1);
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(result, r_0);
    }
#endif
    return result;
}

float xglCos(float angle)
{
    unsigned int entry = (unsigned int)Vu0CallCos >> 3;
    float result;

#ifndef PORT_HOST
    PS2_ASMV(
        "ctc2.i %1,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %2,$vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i %0,$vf1"
        : "=r"(result)
        : "r"(entry), "r"(angle)
        : "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(angle);
        XsgGpr r_1 = XSG_IN(entry);
        XsgGpr r_0 = {{0, 0}};
        vu0_ctc2(27, &r_1);
        vu0_qmtc2(4, &r_2);
        vu0_callmsr();
        vu0_qmfc2(&r_0, 1);
        xsg_vu0_blocks++;
        XSG_OUT(result, r_0);
    }
#endif
    return result;
}

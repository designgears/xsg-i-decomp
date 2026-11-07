#include "port/ps2_asm.h"
#include "common.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "m_math.h"

float MMathMakeRandom(void)
{
    return (float)xglSRand() * 3.051850945e-05f;
}

float MMathMakeRandom2PI(void)
{
    return (float)xglSRand() * 0.0001917534391f;
}

float MMathCalcRotNear(float first, float second)
{
    float difference = fmodf(second, 6.283185005f) - fmodf(first, 6.283185005f);

    if (__builtin_fabsf(difference) > 3.141592741f) {
        if (difference < 0.0f)
            difference += 6.283185005f;
        else
            difference -= 6.283185005f;
    }
    return difference;
}

float MMathCalcRotFar(float first, float second)
{
    float difference = fmodf(second, 6.283185005f) - fmodf(first, 6.283185005f);

    if (__builtin_fabsf(difference) < 3.141592741f) {
        if (difference < 0.0f)
            difference += 6.283185005f;
        else
            difference -= 6.283185005f;
    }
    return difference;
}

/*
 * MMathCalcHermite: four-component cubic Hermite/Catmull-Rom evaluation at
 * parameter t. The VU0 macro-mode block builds the four blend weights from t
 * (packed two-per-register across vf2/vf3's x/y lanes), then accumulates
 * point_start*h1 + point_end*h2 + tangent_start*h3 + tangent_end*h4 into vf1
 * and stores it; vf1.w is forced to VF00's w (1.0) by the trailing vmove.w.
 * The prototype was corrected from the stale `void` shared.h declared for it
 * (no caller anywhere reads the return value, so the change is source-visible
 * only) to the `float *` the original object's `daddu v0,a0,zero` lead-in
 * requires, matching every other pointer-returning function of this TU.
 *
 * The original fixes the parameter's COP1->GPR transfer in $10/t2 (the
 * qmtc2 immediately after reads it from there), so `scratch` is declared on
 * that hard register and used as this block's own read/write operand: a
 * genuine "=r" output at %0 receives the mfc1, and the same variable is
 * re-supplied as an "r" input at %2 for the qmtc2 that consumes it. Nothing
 * in the template names a register directly; the compiler places the mfc1
 * and qmtc2 using ordinary operand substitution, and $10 is fixed only by
 * the explicit-register C declaration, the one register value the original
 * object pins.
 */
float *MMathCalcHermite(float *destination, float parameter,
                         HermiteVector *tangent_first, HermiteVector *tangent_second,
                         HermiteVector *endpoint_first, HermiteVector *endpoint_second)
{
    register int scratch PS2_REG("$10");

#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 %0,%1\n\t"
        "qmtc2 %2,$vf1\n\t"
        "vaddx.xyz $vf2xyz,$vf0xyz,$vf1x\n\t"
        "vmulx.xyz $vf2xyz,$vf2xyz,$vf1x\n\t"
        "vmulx.y $vf2y,$vf2y,$vf1x\n\t"
        "vaddw.xyzw $vf3xyzw,$vf0xyzw,$vf0w\n\t"
        "vaddw.xyzw $vf3xyzw,$vf3xyzw,$vf0w\n\t"
        "vmr32.xyzw $vf3xyzw,$vf3xyzw\n\t"
        "vmul.xyz $vf3xyz,$vf3xyz,$vf2xyz\n\t"
        "vsubz.y $vf5y,$vf3y,$vf3z\n\t"
        "vaddw.y $vf5y,$vf5y,$vf0w\n\t"
        "vsuby.z $vf6z,$vf3z,$vf3y\n\t"
        "vsubx.y $vf7y,$vf2y,$vf3x\n\t"
        "vaddx.y $vf7y,$vf7y,$vf1x\n\t"
        "vsubx.y $vf8y,$vf2y,$vf2x\n\t"
        "lqc2 $vf1,0(%4)\n\t"
        "lqc2 $vf2,0(%5)\n\t"
        "lqc2 $vf3,0(%6)\n\t"
        "lqc2 $vf4,0(%7)\n\t"
        "vmulay.xyz ACCxyz,$vf1xyz,$vf7y\n\t"
        "vmadday.xyz ACCxyz,$vf2xyz,$vf8y\n\t"
        "vmadday.xyz ACCxyz,$vf3xyz,$vf5y\n\t"
        "vmaddz.xyz $vf1xyz,$vf4xyz,$vf6z\n\t"
        "vmove.w $vf1w,$vf0w\n\t"
        "sqc2 $vf1,0(%3)\n\t"
        "nop"
        : "=r"(scratch)
        : "f"(parameter), "r"(scratch), "r"(destination), "r"(tangent_first),
          "r"(tangent_second), "r"(endpoint_first), "r"(endpoint_second)
        : "memory");
#else
    {
        XsgGpr r_7 = XSG_IN(endpoint_second);
        XsgGpr r_6 = XSG_IN(endpoint_first);
        XsgGpr r_5 = XSG_IN(tangent_second);
        XsgGpr r_4 = XSG_IN(tangent_first);
        XsgGpr r_3 = XSG_IN(destination);
        XsgFpr f_1 = XSG_FIN(parameter);
        xsg_ee_gpr[10] = XSG_IN(scratch);
        xsg_ee_set32(&xsg_ee_gpr[10], f_1.u);
        vu0_qmtc2(1, &xsg_ee_gpr[10]);
        vu0_arith(VU0_ADD, 14, 2, 0, 1, 0);
        vu0_arith(VU0_MUL, 14, 2, 2, 1, 0);
        vu0_arith(VU0_MUL, 4, 2, 2, 1, 0);
        vu0_arith(VU0_ADD, 15, 3, 0, 0, 3);
        vu0_arith(VU0_ADD, 15, 3, 3, 0, 3);
        vu0_mr32(15, 3, 3);
        vu0_arith(VU0_MUL, 14, 3, 3, 2, VU0_VEC);
        vu0_arith(VU0_SUB, 4, 5, 3, 3, 2);
        vu0_arith(VU0_ADD, 4, 5, 5, 0, 3);
        vu0_arith(VU0_SUB, 2, 6, 3, 3, 1);
        vu0_arith(VU0_SUB, 4, 7, 2, 3, 0);
        vu0_arith(VU0_ADD, 4, 7, 7, 1, 0);
        vu0_arith(VU0_SUB, 4, 8, 2, 2, 0);
        vu0_lqc2(1, XSG_EA(r_4, 0));
        vu0_lqc2(2, XSG_EA(r_5, 0));
        vu0_lqc2(3, XSG_EA(r_6, 0));
        vu0_lqc2(4, XSG_EA(r_7, 0));
        vu0_arith(VU0_MUL, 14, VU0_ACC, 1, 7, 1);
        vu0_arith(VU0_MADD, 14, VU0_ACC, 2, 8, 1);
        vu0_arith(VU0_MADD, 14, VU0_ACC, 3, 5, 1);
        vu0_arith(VU0_MADD, 14, 1, 4, 6, 2);
        vu0_move(1, 1, 0);
        vu0_sqc2(1, XSG_EA(r_3, 0));
        xsg_vu0_blocks++;
        XSG_OUT(scratch, xsg_ee_gpr[10]);
    }
#endif
    return destination;
}

/*
 * MMathCalcHermitePrm: central-difference tangent pair for a Hermite/Catmull-
 * Rom segment. tangent_start = (point_end - point_prev) * 0.5, tangent_end =
 * (point_next - point_start) * 0.5. The 0.5 factor comes from vitof4 (integer
 * 8 converted with a divide-by-16 shift), not a compiler literal.
 */
void MMathCalcHermitePrm(HermiteVector *tangent_start, HermiteVector *tangent_end,
                          const HermiteVector *point_prev, const HermiteVector *point_start,
                          const HermiteVector *point_end, const HermiteVector *point_next)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1,0(%2)\n\t"
        "lqc2 vf2,0(%3)\n\t"
        "lqc2 vf3,0(%4)\n\t"
        "lqc2 vf4,0(%5)\n\t"
        "viaddi vi5,vi0,8\n\t"
        "vmfir.x vf5x,vi5\n\t"
        "vitof4.x vf5x,vf5x\n\t"
        "vsub.xyz vf6xyz,vf3xyz,vf1xyz\n\t"
        "vsub.xyz vf7xyz,vf4xyz,vf2xyz\n\t"
        "vmulx.xyz vf6xyz,vf6xyz,vf5x\n\t"
        "vmulx.xyz vf7xyz,vf7xyz,vf5x\n\t"
        "sqc2 vf6,0(%0)\n\t"
        "sqc2 vf7,0(%1)\n\t"
        "nop\n\t"
        :
        : "r"(tangent_start), "r"(tangent_end), "r"(point_prev), "r"(point_start),
          "r"(point_end), "r"(point_next)
        : "memory"
    );
#else
    {
        XsgGpr r_5 = XSG_IN(point_next);
        XsgGpr r_4 = XSG_IN(point_end);
        XsgGpr r_3 = XSG_IN(point_start);
        XsgGpr r_2 = XSG_IN(point_prev);
        XsgGpr r_1 = XSG_IN(tangent_end);
        XsgGpr r_0 = XSG_IN(tangent_start);
        vu0_lqc2(1, XSG_EA(r_2, 0));
        vu0_lqc2(2, XSG_EA(r_3, 0));
        vu0_lqc2(3, XSG_EA(r_4, 0));
        vu0_lqc2(4, XSG_EA(r_5, 0));
        vu0_iaddi(5, 0, 8);
        vu0_mfir(8, 5, 5);
        vu0_itof(4, 8, 5, 5);
        vu0_arith(VU0_SUB, 14, 6, 3, 1, VU0_VEC);
        vu0_arith(VU0_SUB, 14, 7, 4, 2, VU0_VEC);
        vu0_arith(VU0_MUL, 14, 6, 6, 5, 0);
        vu0_arith(VU0_MUL, 14, 7, 7, 5, 0);
        vu0_sqc2(6, XSG_EA(r_0, 0));
        vu0_sqc2(7, XSG_EA(r_1, 0));
        xsg_vu0_blocks++;
    }
#endif
}

/*
 * MMathDeg2RadVector/I and MMathRad2DegVector/I: scale the XYZ lanes of an
 * aligned four-lane vector by the degrees<->radians conversion factor, W
 * untouched: the lane-selective VU0
 * macro-mode multiply is what the C says, not an approximation of it. The
 * conversion factor is scaffold-owned .lit4 data (config/tu/main/tu219.json
 * data_ownership; declared `extern const float` in m_math.h under its
 * existing splat/symbol-map name, config/symbols/main.txt) read into a local
 * pinned to $f8 -- the original object's own scratch register for this
 * COP1->GPR->VF conduit. The destination pointer is likewise pinned to $2,
 * the register the original object returns it in (tu-worker.md, "register T
 * x asm(...)"): with both locals declared in that order the compiler emits
 * them in the original's own order -- daddu/move into $2 first, then the
 * lit4 load into $f8 -- instead of the unpinned allocator's choice of $f0
 * scheduled ahead of the return copy.
 */
void *MMathDeg2RadVector(void *destination, const Vector4 *source)
{
    register void *result PS2_REG("$2") = destination;
    register float scale PS2_REG("$f8") = 0.01745329238f;
#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $8,%2\n\t"
        "qmtc2 $8,vf1\n\t"
        "lqc2 vf2,0(%1)\n\t"
        "vmulx.xyz vf2xyz,vf2xyz,vf1x\n\t"
        "sqc2 vf2,0(%0)\n\t"
        "nop\n\t"
        : "+r"(result)
        : "r"(source), "f"(scale)
        : "$8", "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(source);
        xsg_ee_gpr[2] = XSG_IN(result);
        xsg_ee_fpr[8] = XSG_FIN(scale);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_fpr[8].u);
        vu0_qmtc2(1, &xsg_ee_gpr[8]);
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_arith(VU0_MUL, 14, 2, 2, 1, 0);
        vu0_sqc2(2, XSG_EA(xsg_ee_gpr[2], 0));
        xsg_vu0_blocks++;
        XSG_OUT(result, xsg_ee_gpr[2]);
    }
#endif
    return result;
}

void *MMathDeg2RadVectorI(void *destination, const Vector4 *source)
{
    register void *result PS2_REG("$2") = destination;
    register float scale PS2_REG("$f8") = 0.01745329238f;
#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $8,%2\n\t"
        "qmtc2 $8,vf1\n\t"
        "lqc2 vf2,0(%1)\n\t"
        "vitof0.xyz vf2xyz,vf2xyz\n\t"
        "vmulx.xyz vf2xyz,vf2xyz,vf1x\n\t"
        "sqc2 vf2,0(%0)\n\t"
        "nop\n\t"
        : "+r"(result)
        : "r"(source), "f"(scale)
        : "$8", "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(source);
        xsg_ee_gpr[2] = XSG_IN(result);
        xsg_ee_fpr[8] = XSG_FIN(scale);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_fpr[8].u);
        vu0_qmtc2(1, &xsg_ee_gpr[8]);
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_itof(0, 14, 2, 2);
        vu0_arith(VU0_MUL, 14, 2, 2, 1, 0);
        vu0_sqc2(2, XSG_EA(xsg_ee_gpr[2], 0));
        xsg_vu0_blocks++;
        XSG_OUT(result, xsg_ee_gpr[2]);
    }
#endif
    return result;
}

void *MMathRad2DegVector(void *destination, const Vector4 *source)
{
    register void *result PS2_REG("$2") = destination;
    register float scale PS2_REG("$f8") = 57.29578018f;
#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $8,%2\n\t"
        "qmtc2 $8,vf1\n\t"
        "lqc2 vf2,0(%1)\n\t"
        "vmulx.xyz vf2xyz,vf2xyz,vf1x\n\t"
        "sqc2 vf2,0(%0)\n\t"
        "nop\n\t"
        : "+r"(result)
        : "r"(source), "f"(scale)
        : "$8", "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(source);
        xsg_ee_gpr[2] = XSG_IN(result);
        xsg_ee_fpr[8] = XSG_FIN(scale);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_fpr[8].u);
        vu0_qmtc2(1, &xsg_ee_gpr[8]);
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_arith(VU0_MUL, 14, 2, 2, 1, 0);
        vu0_sqc2(2, XSG_EA(xsg_ee_gpr[2], 0));
        xsg_vu0_blocks++;
        XSG_OUT(result, xsg_ee_gpr[2]);
    }
#endif
    return result;
}

void *MMathRad2DegVectorI(void *destination, const Vector4 *source)
{
    register void *result PS2_REG("$2") = destination;
    register float scale PS2_REG("$f8") = 57.29578018f;
#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $8,%2\n\t"
        "qmtc2 $8,vf1\n\t"
        "lqc2 vf2,0(%1)\n\t"
        "vmulx.xyz vf2xyz,vf2xyz,vf1x\n\t"
        "vftoi0.xyz vf2xyz,vf2xyz\n\t"
        "sqc2 vf2,0(%0)\n\t"
        "nop\n\t"
        : "+r"(result)
        : "r"(source), "f"(scale)
        : "$8", "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(source);
        xsg_ee_gpr[2] = XSG_IN(result);
        xsg_ee_fpr[8] = XSG_FIN(scale);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_fpr[8].u);
        vu0_qmtc2(1, &xsg_ee_gpr[8]);
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_arith(VU0_MUL, 14, 2, 2, 1, 0);
        vu0_ftoi(0, 14, 2, 2);
        vu0_sqc2(2, XSG_EA(xsg_ee_gpr[2], 0));
        xsg_vu0_blocks++;
        XSG_OUT(result, xsg_ee_gpr[2]);
    }
#endif
    return result;
}

/*
 * MMathNormalizeVector2: normalize the XYZ lanes of an aligned four-lane
 * vector with the VU0 macro-mode reciprocal-square-root pipeline (Q
 * register), forcing W to 1.0 (vf0w) on the way out.
 */
void *MMathNormalizeVector2(void *destination, const Vector4 *source)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1,0(%1)\n\t"
        "vmul.xyz vf2xyz,vf1xyz,vf1xyz\n\t"
        "vaddy.x vf2x,vf2x,vf2y\n\t"
        "vaddz.x vf2x,vf2x,vf2z\n\t"
        "vrsqrt Q,vf0w,vf2x\n\t"
        "vmove.w vf2w,vf0w\n\t"
        "vwaitq\n\t"
        "vmulq.xyz vf2xyz,vf1xyz,Q\n\t"
        "sqc2 vf2,0(%0)\n\t"
        "nop\n\t"
        : "+r"(destination)
        : "r"(source)
        : "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_arith(VU0_MUL, 14, 2, 1, 1, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 2, 2, 2, 1);
        vu0_arith(VU0_ADD, 8, 2, 2, 2, 2);
        vu0_rsqrt(0, 3, 2, 0);
        vu0_move(1, 2, 0);
        vu0_waitq();
        vu0_arith(VU0_MUL, 14, 2, 1, 0, VU0_BQ);
        vu0_sqc2(2, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
        XSG_OUT(destination, r_0);
    }
#endif
    return destination;
}

/*
 * The six length/distance functions below are recovered as C with
 * constrained ee-vu-cop2 inline assembly. Their C-visible shape comes from
 * the accepted record's own asm_contract: each takes aligned vectors through a0/a1 and returns the
 * reduced length in f0.
 */

float MMathCalcLength(const Vector4 *vector)
{
    float result;
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1,0(%1)\n\t"
        "vmul.xyz vf1xyz,vf1xyz,vf1xyz\n\t"
        "vaddy.x vf1x,vf1x,vf1y\n\t"
        "vaddz.x vf1x,vf1x,vf1z\n\t"
        "vsqrt Q,vf1x\n\t"
        "vwaitq\n\t"
        "vaddq.x vf1x,vf0x,Q\n\t"
        "qmfc2 $8,vf1\n\t"
        "mtc1 $8,%0\n\t"
        : "=f"(result)
        : "r"(vector)
        : "$8", "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(vector);
        XsgFpr f_0 = {0};
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_arith(VU0_MUL, 14, 1, 1, 1, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 1, 1, 1, 1);
        vu0_arith(VU0_ADD, 8, 1, 1, 1, 2);
        vu0_sqrt(1, 0);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 1, 0, 0, VU0_BQ);
        vu0_qmfc2(&xsg_ee_gpr[8], 1);
        f_0.u = xsg_ee_gpr[8].w[0];
        xsg_vu0_blocks++;
        result = f_0.f;
    }
#endif
    return result;
}

float MMathCalcLengthXY(const Vector4 *vector)
{
    float result;
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1,0(%1)\n\t"
        "vmul.xy vf1xy,vf1xy,vf1xy\n\t"
        "vaddy.x vf1x,vf1x,vf1y\n\t"
        "vsqrt Q,vf1x\n\t"
        "vwaitq\n\t"
        "vaddq.x vf1x,vf0x,Q\n\t"
        "qmfc2 $8,vf1\n\t"
        "mtc1 $8,%0\n\t"
        : "=f"(result)
        : "r"(vector)
        : "$8", "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(vector);
        XsgFpr f_0 = {0};
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_arith(VU0_MUL, 12, 1, 1, 1, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 1, 1, 1, 1);
        vu0_sqrt(1, 0);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 1, 0, 0, VU0_BQ);
        vu0_qmfc2(&xsg_ee_gpr[8], 1);
        f_0.u = xsg_ee_gpr[8].w[0];
        xsg_vu0_blocks++;
        result = f_0.f;
    }
#endif
    return result;
}

float MMathCalcLengthXZ(const Vector4 *vector)
{
    float result;
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1,0(%1)\n\t"
        "vmul.xz vf1xz,vf1xz,vf1xz\n\t"
        "vaddz.x vf1x,vf1x,vf1z\n\t"
        "vsqrt Q,vf1x\n\t"
        "vwaitq\n\t"
        "vaddq.x vf1x,vf0x,Q\n\t"
        "qmfc2 $8,vf1\n\t"
        "mtc1 $8,%0\n\t"
        : "=f"(result)
        : "r"(vector)
        : "$8", "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(vector);
        XsgFpr f_0 = {0};
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_arith(VU0_MUL, 10, 1, 1, 1, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 1, 1, 1, 2);
        vu0_sqrt(1, 0);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 1, 0, 0, VU0_BQ);
        vu0_qmfc2(&xsg_ee_gpr[8], 1);
        f_0.u = xsg_ee_gpr[8].w[0];
        xsg_vu0_blocks++;
        result = f_0.f;
    }
#endif
    return result;
}

float MMathCalcLengthYZ(const Vector4 *vector)
{
    float result;
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1,0(%1)\n\t"
        "vmul.yz vf1yz,vf1yz,vf1yz\n\t"
        "vaddz.y vf1y,vf1y,vf1z\n\t"
        "vsqrt Q,vf1y\n\t"
        "vwaitq\n\t"
        "vaddq.x vf1x,vf0x,Q\n\t"
        "qmfc2 $8,vf1\n\t"
        "mtc1 $8,%0\n\t"
        : "=f"(result)
        : "r"(vector)
        : "$8", "memory"
    );
#else
    {
        XsgGpr r_1 = XSG_IN(vector);
        XsgFpr f_0 = {0};
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_arith(VU0_MUL, 6, 1, 1, 1, VU0_VEC);
        vu0_arith(VU0_ADD, 4, 1, 1, 1, 2);
        vu0_sqrt(1, 1);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 1, 0, 0, VU0_BQ);
        vu0_qmfc2(&xsg_ee_gpr[8], 1);
        f_0.u = xsg_ee_gpr[8].w[0];
        xsg_vu0_blocks++;
        result = f_0.f;
    }
#endif
    return result;
}

float MMathCalcDist(const Vector4 *first, const Vector4 *second)
{
    float result;
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1,0(%1)\n\t"
        "lqc2 vf2,0(%2)\n\t"
        "vsub.xyz vf1xyz,vf2xyz,vf1xyz\n\t"
        "vmul.xyz vf1xyz,vf1xyz,vf1xyz\n\t"
        "vaddy.x vf1x,vf1x,vf1y\n\t"
        "vaddz.x vf1x,vf1x,vf1z\n\t"
        "vsqrt Q,vf1x\n\t"
        "vwaitq\n\t"
        "vaddq.x vf1x,vf0x,Q\n\t"
        "qmfc2 $8,vf1\n\t"
        "mtc1 $8,%0\n\t"
        : "=f"(result)
        : "r"(first), "r"(second)
        : "$8", "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(second);
        XsgGpr r_1 = XSG_IN(first);
        XsgFpr f_0 = {0};
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 14, 1, 2, 1, VU0_VEC);
        vu0_arith(VU0_MUL, 14, 1, 1, 1, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 1, 1, 1, 1);
        vu0_arith(VU0_ADD, 8, 1, 1, 1, 2);
        vu0_sqrt(1, 0);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 1, 0, 0, VU0_BQ);
        vu0_qmfc2(&xsg_ee_gpr[8], 1);
        f_0.u = xsg_ee_gpr[8].w[0];
        xsg_vu0_blocks++;
        result = f_0.f;
    }
#endif
    return result;
}

float MMathCalcDistXZ(const Vector4 *first, const Vector4 *second)
{
    float result;
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1,0(%1)\n\t"
        "lqc2 vf2,0(%2)\n\t"
        "vsub.xz vf1xz,vf2xz,vf1xz\n\t"
        "vmul.xz vf1xz,vf1xz,vf1xz\n\t"
        "vaddz.x vf1x,vf1x,vf1z\n\t"
        "vsqrt Q,vf1x\n\t"
        "vwaitq\n\t"
        "vaddq.x vf1x,vf0x,Q\n\t"
        "qmfc2 $8,vf1\n\t"
        "mtc1 $8,%0\n\t"
        : "=f"(result)
        : "r"(first), "r"(second)
        : "$8", "memory"
    );
#else
    {
        XsgGpr r_2 = XSG_IN(second);
        XsgGpr r_1 = XSG_IN(first);
        XsgFpr f_0 = {0};
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 10, 1, 2, 1, VU0_VEC);
        vu0_arith(VU0_MUL, 10, 1, 1, 1, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 1, 1, 1, 2);
        vu0_sqrt(1, 0);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 1, 0, 0, VU0_BQ);
        vu0_qmfc2(&xsg_ee_gpr[8], 1);
        f_0.u = xsg_ee_gpr[8].w[0];
        xsg_vu0_blocks++;
        result = f_0.f;
    }
#endif
    return result;
}

Vector4 *MMathCalcDirVector(Vector4 *destination, const Vector4 *source,
                            const Vector4 *target)
{
    register Vector4 *normalized_destination PS2_REG("$2") = destination;
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1,0(%1)\n\t"
        "lqc2 vf2,0(%2)\n\t"
        "vsub.xyz vf2,vf2,vf1\n\t"
        "sqc2 vf2,0(%0)"
        : "+r"(normalized_destination)
        : "r"(source), "r"(target)
        : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(target);
        XsgGpr r_1 = XSG_IN(source);
        xsg_ee_gpr[2] = XSG_IN(normalized_destination);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 14, 2, 2, 1, VU0_VEC);
        vu0_sqc2(2, XSG_EA(xsg_ee_gpr[2], 0));
        xsg_vu0_blocks++;
        XSG_OUT(normalized_destination, xsg_ee_gpr[2]);
    }
#endif
    MMathNormalizeVector2(destination, normalized_destination);
    return normalized_destination;
}

/* sef.c (main); no header is published for it yet. */
extern float srsAtan2(float deltaX, float deltaZ);

/*
 * The call to srsAtan2 is the last statement of this function, but the
 * original keeps a real jal for it followed by its own epilogue rather than
 * tail-jumping into srsAtan2; a value read after the call reproduces that
 * (this compiler otherwise folds a trailing call with nothing after it into
 * a sibling jump).
 */
#ifdef PORT_HOST
/* srsAtan2's result stays in $f0 on the EE; callers read it as the return. */
float MMathCalcDir(const Vector4 *from, const Vector4 *to)
{
    return srsAtan2(to->x - from->x, to->z - from->z);
}
#else
void MMathCalcDir(const Vector4 *from, const Vector4 *to)
{
    float toX;

    srsAtan2(to->x - from->x, to->z - from->z);
    toX = to->x;
}
#endif

Vector4 *MMathCalcAngle(Vector4 *out, const Vector4 *from, const Vector4 *to)
{
    Vector4 delta;
    float length = 0.0f;

#ifndef PORT_HOST
    PS2_ASMV(
        "lui $8,%%hi(McMathUnitVector)\n\t"
        "addiu $8,$8,%%lo(McMathUnitVector)\n\t"
        "movz %1,$8,%1\n\t"
        "movz %2,$8,%2\n\t"
        "lqc2 $vf1, 0(%1)\n\t"
        "lqc2 $vf2, 0(%2)\n\t"
        "vsub.xyz $vf3, $vf2, $vf1\n\t"
        "vmul.xz $vf4, $vf3, $vf3\n\t"
        "vaddz.x $vf4, $vf4, $vf4z\n\t"
        "vsqrt Q, $vf4x\n\t"
        "sqc2 $vf3, 0(%3)\n\t"
        "sqc2 $vf0, 0(%4)\n\t"
        "vwaitq\n\t"
        "vaddq.x $vf1, $vf0, Q\n\t"
        "qmfc2.ni $8, $vf1\n\t"
        "mtc1 $8, %0"
        : "+f"(length)
        : "r"(from), "r"(to), "r"(&delta), "r"(out)
        : "memory", "$8"
    );
#else
    {
        extern char xsg_sym_McMathUnitVector[] __asm__("McMathUnitVector");
        XsgGpr r_4 = XSG_IN(out);
        XsgGpr r_3 = XSG_IN(&delta);
        XsgGpr r_2 = XSG_IN(to);
        XsgGpr r_1 = XSG_IN(from);
        XsgFpr f_0 = XSG_FIN(length);
        xsg_ee_set32(&xsg_ee_gpr[8], (unsigned int)((((unsigned int)(unsigned long)xsg_sym_McMathUnitVector + 0x8000u) >> 16)) << 16);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_gpr[8].w[0] + (unsigned int)((int)(short)((unsigned int)(unsigned long)xsg_sym_McMathUnitVector & 0xffffu)));
        if (r_1.d[0] == 0) r_1.d[0] = xsg_ee_gpr[8].d[0];
        if (r_2.d[0] == 0) r_2.d[0] = xsg_ee_gpr[8].d[0];
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 14, 3, 2, 1, VU0_VEC);
        vu0_arith(VU0_MUL, 10, 4, 3, 3, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 4, 4, 4, 2);
        vu0_sqrt(4, 0);
        vu0_sqc2(3, XSG_EA(r_3, 0));
        vu0_sqc2(0, XSG_EA(r_4, 0));
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 1, 0, 0, VU0_BQ);
        vu0_qmfc2(&xsg_ee_gpr[8], 1);
        f_0.u = xsg_ee_gpr[8].w[0];
        xsg_vu0_blocks++;
        length = f_0.f;
    }
#endif
#ifdef PORT_HOST
    {
        /* Host stand-in for the VU0 block above (McMathUnitVector for null inputs). */
        extern const Vector4 McMathUnitVector;
        if (!from) from = &McMathUnitVector;
        if (!to) to = &McMathUnitVector;
        delta.x = to->x - from->x;
        delta.y = to->y - from->y;
        delta.z = to->z - from->z;
        length = __builtin_sqrtf(delta.x * delta.x + delta.z * delta.z);
        out->x = 0.0f; out->y = 0.0f; out->z = 0.0f; out->w = 1.0f;
    }
#endif
    out->x = srsAtan2(delta.y, length);
    out->y = srsAtan2(delta.x, delta.z);
    return out;
}

Vector4 *MMathCalcAngleCam(Vector4 *out, const Vector4 *from, const Vector4 *to)
{
    Vector4 delta;
    float length = 0.0f;

#ifndef PORT_HOST
    PS2_ASMV(
        "lui $8,%%hi(McMathUnitVector)\n\t"
        "addiu $8,$8,%%lo(McMathUnitVector)\n\t"
        "movz %1,$8,%1\n\t"
        "movz %2,$8,%2\n\t"
        "lqc2 $vf1, 0(%1)\n\t"
        "lqc2 $vf2, 0(%2)\n\t"
        "vsub.xyz $vf3, $vf1, $vf2\n\t"
        "vmul.xz $vf4, $vf3, $vf3\n\t"
        "vaddz.x $vf4, $vf4, $vf4z\n\t"
        "vsqrt Q, $vf4x\n\t"
        "vsub.y $vf3, $vf2, $vf1\n\t"
        "sqc2 $vf3, 0(%3)\n\t"
        "sqc2 $vf0, 0(%4)\n\t"
        "vwaitq\n\t"
        "vaddq.x $vf1, $vf0, Q\n\t"
        "qmfc2.ni $8, $vf1\n\t"
        "mtc1 $8, %0"
        : "+f"(length), "+r"(from), "+r"(to)
        : "r"(&delta), "r"(out)
        : "memory", "$8"
    );
#else
    {
        extern char xsg_sym_McMathUnitVector[] __asm__("McMathUnitVector");
        XsgGpr r_4 = XSG_IN(out);
        XsgGpr r_3 = XSG_IN(&delta);
        XsgGpr r_2 = XSG_IN(to);
        XsgGpr r_1 = XSG_IN(from);
        XsgFpr f_0 = XSG_FIN(length);
        xsg_ee_set32(&xsg_ee_gpr[8], (unsigned int)((((unsigned int)(unsigned long)xsg_sym_McMathUnitVector + 0x8000u) >> 16)) << 16);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_gpr[8].w[0] + (unsigned int)((int)(short)((unsigned int)(unsigned long)xsg_sym_McMathUnitVector & 0xffffu)));
        if (r_1.d[0] == 0) r_1.d[0] = xsg_ee_gpr[8].d[0];
        if (r_2.d[0] == 0) r_2.d[0] = xsg_ee_gpr[8].d[0];
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 14, 3, 1, 2, VU0_VEC);
        vu0_arith(VU0_MUL, 10, 4, 3, 3, VU0_VEC);
        vu0_arith(VU0_ADD, 8, 4, 4, 4, 2);
        vu0_sqrt(4, 0);
        vu0_arith(VU0_SUB, 4, 3, 2, 1, VU0_VEC);
        vu0_sqc2(3, XSG_EA(r_3, 0));
        vu0_sqc2(0, XSG_EA(r_4, 0));
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 1, 0, 0, VU0_BQ);
        vu0_qmfc2(&xsg_ee_gpr[8], 1);
        f_0.u = xsg_ee_gpr[8].w[0];
        xsg_vu0_blocks++;
        length = f_0.f;
        XSG_OUT(from, r_1);
        XSG_OUT(to, r_2);
    }
#endif
#ifdef PORT_HOST
    {
        /* Host stand-in for the VU0 block above (McMathUnitVector for null inputs). */
        extern const Vector4 McMathUnitVector;
        if (!from) from = &McMathUnitVector;
        if (!to) to = &McMathUnitVector;
        delta.x = from->x - to->x;
        delta.y = to->y - from->y;
        delta.z = from->z - to->z;
        length = __builtin_sqrtf(delta.x * delta.x + delta.z * delta.z);
        out->x = 0.0f; out->y = 0.0f; out->z = 0.0f; out->w = 1.0f;
    }
#endif
    out->x = srsAtan2(delta.y, length);
    out->y = srsAtan2(delta.x, delta.z);
    return out;
}

/*
 * MMathAddRotateVectorY: rotate "offset" about the Y axis by "angle" (VU0
 * macro-mode sin/cos at 0xe8/0x20) and add the result to "base", storing the
 * sum at "destination".
 */
Vector4 *MMathAddRotateVectorY(Vector4 *destination, float angle, const Vector4 *base, const Vector4 *offset)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $8, %0\n\t"
        "qmtc2.ni $8, vf6\n\t"
        "vmove.x vf4, vf6\n\t"
        "vcallms 0xE8\n\t"
        "vmove.x vf5, vf1\n\t"
        "vmove.x vf4, vf6\n\t"
        "vcallms 0x20\n\t"
        "vmove.x vf6, vf1\n\t"
        "lqc2 vf2, 0(%1)\n\t"
        "lqc2 vf3, 0(%2)\n\t"
        "vaddz.x vf4, vf0, vf3z\n\t"
        "vsubx.z vf4, vf0, vf3x\n\t"
        "vmulax.xz ACC, vf3, vf5x\n\t"
        "vmaddx.xz vf3, vf4, vf6x\n\t"
        "vadd.xyz vf2, vf2, vf3\n\t"
        "sqc2 vf2, 0(%3)\n\t"
        "nop"
        : : "f"(angle), "r"(base), "r"(offset), "r"(destination) : "$8", "memory");
#else
    {
        XsgGpr r_3 = XSG_IN(destination);
        XsgGpr r_2 = XSG_IN(offset);
        XsgGpr r_1 = XSG_IN(base);
        XsgFpr f_0 = XSG_FIN(angle);
        xsg_ee_set32(&xsg_ee_gpr[8], f_0.u);
        vu0_qmtc2(6, &xsg_ee_gpr[8]);
        vu0_move(8, 4, 6);
        vu0_callms(232);
        vu0_move(8, 5, 1);
        vu0_move(8, 4, 6);
        vu0_callms(32);
        vu0_move(8, 6, 1);
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_lqc2(3, XSG_EA(r_2, 0));
        vu0_arith(VU0_ADD, 8, 4, 0, 3, 2);
        vu0_arith(VU0_SUB, 2, 4, 0, 3, 0);
        vu0_arith(VU0_MUL, 10, VU0_ACC, 3, 5, 0);
        vu0_arith(VU0_MADD, 10, 3, 4, 6, 0);
        vu0_arith(VU0_ADD, 14, 2, 2, 3, VU0_VEC);
        vu0_sqc2(2, XSG_EA(r_3, 0));
        xsg_vu0_blocks++;
    }
#endif
    return destination;
}

/*
 * MMathCalcOffset: compose a position offset from two lane-packed angles
 * (yaw in "first"'s w and "second"'s y, pitch chained through "second"'s x)
 * with VU0 macro-mode sin/cos (0xe8/0x20), scale it by "second"'s w and add
 * it to "first", storing the sum at "destination".
 */
Vector4 *MMathCalcOffset(Vector4 *destination, const Vector4 *first, const Vector4 *second)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf5, 0(%0)\n\t"
        "lqc2 vf6, 0(%1)\n\t"
        "vaddy.w vf7, vf5, vf6y\n\t"
        "vaddw.x vf4, vf0, vf7w\n\t"
        "vcallms 0xE8\n\t"
        "vaddx.z vf7, vf0, vf1x\n\t"
        "vaddw.x vf4, vf0, vf7w\n\t"
        "vcallms 0x20\n\t"
        "vmove.x vf7, vf1\n\t"
        "vmove.x vf4, vf6\n\t"
        "vcallms 0x20\n\t"
        "vaddx.y vf7, vf0, vf1x\n\t"
        "vmove.x vf4, vf6\n\t"
        "vcallms 0xE8\n\t"
        "vmulx.xz vf7, vf7, vf1x\n\t"
        "vmulaw.xyz ACC, vf7, vf6w\n\t"
        "vmaddw.xyz vf5, vf5, vf0w\n\t"
        "sqc2 vf5, 0(%2)\n\t"
        "nop"
        : : "r"(first), "r"(second), "r"(destination) : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(destination);
        XsgGpr r_1 = XSG_IN(second);
        XsgGpr r_0 = XSG_IN(first);
        vu0_lqc2(5, XSG_EA(r_0, 0));
        vu0_lqc2(6, XSG_EA(r_1, 0));
        vu0_arith(VU0_ADD, 1, 7, 5, 6, 1);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 3);
        vu0_callms(232);
        vu0_arith(VU0_ADD, 2, 7, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 3);
        vu0_callms(32);
        vu0_move(8, 7, 1);
        vu0_move(8, 4, 6);
        vu0_callms(32);
        vu0_arith(VU0_ADD, 4, 7, 0, 1, 0);
        vu0_move(8, 4, 6);
        vu0_callms(232);
        vu0_arith(VU0_MUL, 10, 7, 7, 1, 0);
        vu0_arith(VU0_MUL, 14, VU0_ACC, 7, 6, 3);
        vu0_arith(VU0_MADD, 14, 5, 5, 0, 3);
        vu0_sqc2(5, XSG_EA(r_2, 0));
        xsg_vu0_blocks++;
    }
#endif
    return destination;
}

/*
 * MMathCalcOffsetXYZ: the single-angle-chain counterpart of MMathCalcOffset
 * (angle taken from "first"'s w only), same VU0 macro-mode sin/cos shape.
 */
Vector4 *MMathCalcOffsetXYZ(Vector4 *destination, const Vector4 *first, const Vector4 *second)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf5, 0(%0)\n\t"
        "lqc2 vf6, 0(%1)\n\t"
        "vaddw.x vf4, vf0, vf5w\n\t"
        "vcallms 0xE8\n\t"
        "vmove.x vf7, vf1\n\t"
        "vaddw.x vf4, vf0, vf5w\n\t"
        "vcallms 0x20\n\t"
        "vaddz.x vf2, vf0, vf6z\n\t"
        "vsubx.z vf2, vf0, vf6x\n\t"
        "vmulax.xz ACC, vf6, vf7x\n\t"
        "vmaddx.xz vf6, vf2, vf1x\n\t"
        "vadd.xyz vf5, vf5, vf6\n\t"
        "sqc2 vf5, 0(%2)\n\t"
        "nop"
        : : "r"(first), "r"(second), "r"(destination) : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(destination);
        XsgGpr r_1 = XSG_IN(second);
        XsgGpr r_0 = XSG_IN(first);
        vu0_lqc2(5, XSG_EA(r_0, 0));
        vu0_lqc2(6, XSG_EA(r_1, 0));
        vu0_arith(VU0_ADD, 8, 4, 0, 5, 3);
        vu0_callms(232);
        vu0_move(8, 7, 1);
        vu0_arith(VU0_ADD, 8, 4, 0, 5, 3);
        vu0_callms(32);
        vu0_arith(VU0_ADD, 8, 2, 0, 6, 2);
        vu0_arith(VU0_SUB, 2, 2, 0, 6, 0);
        vu0_arith(VU0_MUL, 10, VU0_ACC, 6, 7, 0);
        vu0_arith(VU0_MADD, 10, 6, 2, 1, 0);
        vu0_arith(VU0_ADD, 14, 5, 5, 6, VU0_VEC);
        vu0_sqc2(5, XSG_EA(r_2, 0));
        xsg_vu0_blocks++;
    }
#endif
    return destination;
}

/* MMathSubVectorMulS: destination = (first - second) * scale (xyz only). */
Vector4 *MMathSubVectorMulS(Vector4 *destination, const Vector4 *first, const Vector4 *second, float scale)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $8, %0\n\t"
        "qmtc2.ni $8, vf3\n\t"
        "lqc2 vf1, 0(%1)\n\t"
        "lqc2 vf2, 0(%2)\n\t"
        "vsub.xyz vf1, vf1, vf2\n\t"
        "vmulx.xyz vf1, vf1, vf3x\n\t"
        "sqc2 vf1, 0(%3)\n\t"
        "nop"
        : : "f"(scale), "r"(first), "r"(second), "r"(destination) : "$8", "memory");
#else
    {
        XsgGpr r_3 = XSG_IN(destination);
        XsgGpr r_2 = XSG_IN(second);
        XsgGpr r_1 = XSG_IN(first);
        XsgFpr f_0 = XSG_FIN(scale);
        xsg_ee_set32(&xsg_ee_gpr[8], f_0.u);
        vu0_qmtc2(3, &xsg_ee_gpr[8]);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 14, 1, 1, 2, VU0_VEC);
        vu0_arith(VU0_MUL, 14, 1, 1, 3, 0);
        vu0_sqc2(1, XSG_EA(r_3, 0));
        xsg_vu0_blocks++;
    }
#endif
    return destination;
}

/* MMathSubVectorDivS: destination = (first - second) / scale (xyz only). */
Vector4 *MMathSubVectorDivS(Vector4 *destination, const Vector4 *first, const Vector4 *second, float scale)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $8, %0\n\t"
        "qmtc2.ni $8, vf3\n\t"
        "lqc2 vf1, 0(%1)\n\t"
        "vdiv Q, vf0w, vf3x\n\t"
        "lqc2 vf2, 0(%2)\n\t"
        "vsub.xyz vf1, vf1, vf2\n\t"
        "vwaitq\n\t"
        "vmulq.xyz vf1, vf1, Q\n\t"
        "sqc2 vf1, 0(%3)\n\t"
        "nop"
        : : "f"(scale), "r"(first), "r"(second), "r"(destination) : "$8", "memory");
#else
    {
        XsgGpr r_3 = XSG_IN(destination);
        XsgGpr r_2 = XSG_IN(second);
        XsgGpr r_1 = XSG_IN(first);
        XsgFpr f_0 = XSG_FIN(scale);
        xsg_ee_set32(&xsg_ee_gpr[8], f_0.u);
        vu0_qmtc2(3, &xsg_ee_gpr[8]);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_div(0, 3, 3, 0);
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 14, 1, 1, 2, VU0_VEC);
        vu0_waitq();
        vu0_arith(VU0_MUL, 14, 1, 1, 0, VU0_BQ);
        vu0_sqc2(1, XSG_EA(r_3, 0));
        xsg_vu0_blocks++;
    }
#endif
    return destination;
}

/*
 * MMathDivVector: destination.xyz = first.xyz / second.xyz (lane by lane,
 * VU0 macro-mode divider unit); destination.w is passed through from first.
 */
Vector4 *MMathDivVector(Vector4 *destination, const Vector4 *first, const Vector4 *second)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1, 0(%0)\n\t"
        "lqc2 vf2, 0(%1)\n\t"
        "vdiv Q, vf1x, vf2x\n\t"
        "vmove.w vf3, vf1\n\t"
        "vwaitq\n\t"
        "vaddq.x vf3, vf0, Q\n\t"
        "vdiv Q, vf1y, vf2y\n\t"
        "vwaitq\n\t"
        "vaddq.y vf3, vf0, Q\n\t"
        "vdiv Q, vf1z, vf2z\n\t"
        "vwaitq\n\t"
        "vaddq.z vf3, vf0, Q\n\t"
        "sqc2 vf3, 0(%2)\n\t"
        "nop"
        : : "r"(first), "r"(second), "r"(destination) : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(destination);
        XsgGpr r_1 = XSG_IN(second);
        XsgGpr r_0 = XSG_IN(first);
        vu0_lqc2(1, XSG_EA(r_0, 0));
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_div(1, 0, 2, 0);
        vu0_move(1, 3, 1);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 3, 0, 0, VU0_BQ);
        vu0_div(1, 1, 2, 1);
        vu0_waitq();
        vu0_arith(VU0_ADD, 4, 3, 0, 0, VU0_BQ);
        vu0_div(1, 2, 2, 2);
        vu0_waitq();
        vu0_arith(VU0_ADD, 2, 3, 0, 0, VU0_BQ);
        vu0_sqc2(3, XSG_EA(r_2, 0));
        xsg_vu0_blocks++;
    }
#endif
    return destination;
}

/* MMathDivVectorS: destination.xyz = source.xyz / scale. */
Vector4 *MMathDivVectorS(Vector4 *destination, const Vector4 *source, float scale)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $8, %0\n\t"
        "qmtc2.ni $8, vf2\n\t"
        "lqc2 vf1, 0(%1)\n\t"
        "vdiv Q, vf0w, vf2x\n\t"
        "vwaitq\n\t"
        "vmulq.xyz vf1, vf1, Q\n\t"
        "sqc2 vf1, 0(%2)\n\t"
        "nop"
        : : "f"(scale), "r"(source), "r"(destination) : "$8", "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(destination);
        XsgGpr r_1 = XSG_IN(source);
        XsgFpr f_0 = XSG_FIN(scale);
        xsg_ee_set32(&xsg_ee_gpr[8], f_0.u);
        vu0_qmtc2(2, &xsg_ee_gpr[8]);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_div(0, 3, 2, 0);
        vu0_waitq();
        vu0_arith(VU0_MUL, 14, 1, 1, 0, VU0_BQ);
        vu0_sqc2(1, XSG_EA(r_2, 0));
        xsg_vu0_blocks++;
    }
#endif
    return destination;
}

/* MMathDivVector4: destination = first / second, all four lanes. */
Vector4 *MMathDivVector4(Vector4 *destination, const Vector4 *first, const Vector4 *second)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1, 0(%0)\n\t"
        "lqc2 vf2, 0(%1)\n\t"
        "vdiv Q, vf1x, vf2x\n\t"
        "vsub.w vf3, vf3, vf3\n\t"
        "vwaitq\n\t"
        "vaddq.x vf3, vf0, Q\n\t"
        "vdiv Q, vf1y, vf2y\n\t"
        "vwaitq\n\t"
        "vaddq.y vf3, vf0, Q\n\t"
        "vdiv Q, vf1z, vf2z\n\t"
        "vwaitq\n\t"
        "vaddq.z vf3, vf0, Q\n\t"
        "vdiv Q, vf1w, vf2w\n\t"
        "vwaitq\n\t"
        "vaddq.w vf3, vf3, Q\n\t"
        "sqc2 vf3, 0(%2)\n\t"
        "nop"
        : : "r"(first), "r"(second), "r"(destination) : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(destination);
        XsgGpr r_1 = XSG_IN(second);
        XsgGpr r_0 = XSG_IN(first);
        vu0_lqc2(1, XSG_EA(r_0, 0));
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_div(1, 0, 2, 0);
        vu0_arith(VU0_SUB, 1, 3, 3, 3, VU0_VEC);
        vu0_waitq();
        vu0_arith(VU0_ADD, 8, 3, 0, 0, VU0_BQ);
        vu0_div(1, 1, 2, 1);
        vu0_waitq();
        vu0_arith(VU0_ADD, 4, 3, 0, 0, VU0_BQ);
        vu0_div(1, 2, 2, 2);
        vu0_waitq();
        vu0_arith(VU0_ADD, 2, 3, 0, 0, VU0_BQ);
        vu0_div(1, 3, 2, 3);
        vu0_waitq();
        vu0_arith(VU0_ADD, 1, 3, 3, 0, VU0_BQ);
        vu0_sqc2(3, XSG_EA(r_2, 0));
        xsg_vu0_blocks++;
    }
#endif
    return destination;
}

/* MMathDivVectorS4: destination = source / scale, all four lanes. */
Vector4 *MMathDivVectorS4(Vector4 *destination, const Vector4 *source, float scale)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $8, %0\n\t"
        "qmtc2.ni $8, vf2\n\t"
        "lqc2 vf1, 0(%1)\n\t"
        "vdiv Q, vf0w, vf2x\n\t"
        "vwaitq\n\t"
        "vmulq.xyzw vf1, vf1, Q\n\t"
        "sqc2 vf1, 0(%2)\n\t"
        "nop"
        : : "f"(scale), "r"(source), "r"(destination) : "$8", "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(destination);
        XsgGpr r_1 = XSG_IN(source);
        XsgFpr f_0 = XSG_FIN(scale);
        xsg_ee_set32(&xsg_ee_gpr[8], f_0.u);
        vu0_qmtc2(2, &xsg_ee_gpr[8]);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_div(0, 3, 2, 0);
        vu0_waitq();
        vu0_arith(VU0_MUL, 15, 1, 1, 0, VU0_BQ);
        vu0_sqc2(1, XSG_EA(r_2, 0));
        xsg_vu0_blocks++;
    }
#endif
    return destination;
}

short MMathIsVectorEqual(const Vector4 *first, const Vector4 *second)
{
    int result;

#ifndef PORT_HOST
    PS2_ASMV(
        "lq $8, 0(%1)\n\t"
        "lq $9, 0(%2)\n\t"
        "pxor $10, $8, $9\n\t"
        "addi %0, $0, 1\n\t"
        "movn %0, $0, $10\n\t"
        "pexew $10, $10\n\t"
        "movn %0, $0, $10"
        : "=r"(result)
        : "r"(first), "r"(second)
        : "$8", "$9", "$10");
#else
    {
        XsgGpr r_2 = XSG_IN(second);
        XsgGpr r_1 = XSG_IN(first);
        XsgGpr r_0 = {{0, 0}};
        __builtin_memcpy(&xsg_ee_gpr[8], XSG_EA(r_1, 0), 16);
        __builtin_memcpy(&xsg_ee_gpr[9], XSG_EA(r_2, 0), 16);
        xsg_mmi_pxor(&xsg_ee_gpr[10], &xsg_ee_gpr[8], &xsg_ee_gpr[9]);
        xsg_ee_set32(&r_0, xsg_ee_zero.w[0] + (unsigned int)(1));
        if (xsg_ee_gpr[10].d[0] != 0) r_0.d[0] = xsg_ee_zero.d[0];
        xsg_mmi_pexew(&xsg_ee_gpr[10], &xsg_ee_zero, &xsg_ee_gpr[10]);
        if (xsg_ee_gpr[10].d[0] != 0) r_0.d[0] = xsg_ee_zero.d[0];
        xsg_vu0_blocks++;
        XSG_OUT(result, r_0);
    }
#endif
    return result;
}

short MMathIsVectorEqual4(const Vector4 *first, const Vector4 *second)
{
    int result;

#ifndef PORT_HOST
    PS2_ASMV(
        "lq $8, 0(%1)\n\t"
        "lq $9, 0(%2)\n\t"
        "pxor $10, $8, $9\n\t"
        "addi %0, $0, 1\n\t"
        "movn %0, $0, $10\n\t"
        "pcpyud $10, $10, $0\n\t"
        "movn %0, $0, $10"
        : "=r"(result)
        : "r"(first), "r"(second)
        : "$8", "$9", "$10");
#else
    {
        XsgGpr r_2 = XSG_IN(second);
        XsgGpr r_1 = XSG_IN(first);
        XsgGpr r_0 = {{0, 0}};
        __builtin_memcpy(&xsg_ee_gpr[8], XSG_EA(r_1, 0), 16);
        __builtin_memcpy(&xsg_ee_gpr[9], XSG_EA(r_2, 0), 16);
        xsg_mmi_pxor(&xsg_ee_gpr[10], &xsg_ee_gpr[8], &xsg_ee_gpr[9]);
        xsg_ee_set32(&r_0, xsg_ee_zero.w[0] + (unsigned int)(1));
        if (xsg_ee_gpr[10].d[0] != 0) r_0.d[0] = xsg_ee_zero.d[0];
        xsg_mmi_pcpyud(&xsg_ee_gpr[10], &xsg_ee_gpr[10], &xsg_ee_zero);
        if (xsg_ee_gpr[10].d[0] != 0) r_0.d[0] = xsg_ee_zero.d[0];
        xsg_vu0_blocks++;
        XSG_OUT(result, r_0);
    }
#endif
    return result;
}

/* MMathVectorDotProduct: xyz dot product, reduced through vf1's x lane. */
float MMathVectorDotProduct(const Vector4 *first, const Vector4 *second)
{
    float result;

#ifndef PORT_HOST
    PS2_ASMV("lqc2 vf1, 0(%0)" : : "r"(first) : "memory");
#else
    {
        XsgGpr r_0 = XSG_IN(first);
        vu0_lqc2(1, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
#ifndef PORT_HOST
    PS2_ASMV("lqc2 vf2, 0(%0)" : : "r"(second) : "memory");
#else
    {
        XsgGpr r_0 = XSG_IN(second);
        vu0_lqc2(2, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
#ifndef PORT_HOST
    PS2_ASMV("vmul.xyz vf1, vf1, vf2" : : : "memory");
#else
    {
        vu0_arith(VU0_MUL, 14, 1, 1, 2, VU0_VEC);
        xsg_vu0_blocks++;
    }
#endif
#ifndef PORT_HOST
    PS2_ASMV("vaddy.x vf1, vf1, vf1y" : : : "memory");
#else
    {
        vu0_arith(VU0_ADD, 8, 1, 1, 1, 1);
        xsg_vu0_blocks++;
    }
#endif
#ifndef PORT_HOST
    PS2_ASMV("vaddz.x vf1, vf1, vf1z" : : : "memory");
#else
    {
        vu0_arith(VU0_ADD, 8, 1, 1, 1, 2);
        xsg_vu0_blocks++;
    }
#endif
#ifndef PORT_HOST
    PS2_ASMV(
        "qmfc2.ni $8, vf1\n\t"
        "mtc1 $8, %0"
        : "=f"(result) :  : "$8", "memory");
#else
    {
        XsgFpr f_0 = {0};
        vu0_qmfc2(&xsg_ee_gpr[8], 1);
        f_0.u = xsg_ee_gpr[8].w[0];
        xsg_vu0_blocks++;
        result = f_0.f;
    }
#endif
    return result;
}

/* MMathVectorCrossProduct: destination.xyz = first x second, w forced to 1.0. */
Vector4 *MMathVectorCrossProduct(Vector4 *destination, const Vector4 *first, const Vector4 *second)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1, 0(%0)\n\t"
        "lqc2 vf2, 0(%1)\n\t"
        "vopmula.xyz ACC, vf1, vf2\n\t"
        "vopmsub.xyz vf1, vf2, vf1\n\t"
        "vmove.w vf1, vf0\n\t"
        "sqc2 vf1, 0(%2)\n\t"
        "nop"
        : : "r"(first), "r"(second), "r"(destination) : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(destination);
        XsgGpr r_1 = XSG_IN(second);
        XsgGpr r_0 = XSG_IN(first);
        vu0_lqc2(1, XSG_EA(r_0, 0));
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_opmula(14, 1, 2);
        vu0_opmsub(14, 1, 2, 1);
        vu0_move(1, 1, 0);
        vu0_sqc2(1, XSG_EA(r_2, 0));
        xsg_vu0_blocks++;
    }
#endif
    return destination;
}

/* MMathVectorInterpolation: destination = first * (1 - parameter) + second * parameter. */
Vector4 *MMathVectorInterpolation(Vector4 *destination, const Vector4 *first, const Vector4 *second, float parameter)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $8, %0\n\t"
        "qmtc2.ni $8, vf3\n\t"
        "vsubx.w vf3, vf0, vf3x\n\t"
        "lqc2 vf1, 0(%1)\n\t"
        "lqc2 vf2, 0(%2)\n\t"
        "vmulaw.xyz ACC, vf1, vf3w\n\t"
        "vmaddx.xyz vf1, vf2, vf3x\n\t"
        "sqc2 vf1, 0(%3)\n\t"
        "nop"
        : : "f"(parameter), "r"(first), "r"(second), "r"(destination) : "$8", "memory");
#else
    {
        XsgGpr r_3 = XSG_IN(destination);
        XsgGpr r_2 = XSG_IN(second);
        XsgGpr r_1 = XSG_IN(first);
        XsgFpr f_0 = XSG_FIN(parameter);
        xsg_ee_set32(&xsg_ee_gpr[8], f_0.u);
        vu0_qmtc2(3, &xsg_ee_gpr[8]);
        vu0_arith(VU0_SUB, 1, 3, 0, 3, 0);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_MUL, 14, VU0_ACC, 1, 3, 3);
        vu0_arith(VU0_MADD, 14, 1, 2, 3, 0);
        vu0_sqc2(1, XSG_EA(r_3, 0));
        xsg_vu0_blocks++;
    }
#endif
    return destination;
}

Vector4 *MMathSpecialVectorSub(Vector4 *destination, const Vector4 *first, const Vector4 *second, float parameter)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lui $8,%%hi(McMathUnitVector)\n\t"
        "addiu $8,$8,%%lo(McMathUnitVector)\n\t"
        "movz %0,$8,%0\n\t"
        "li.s $f8,0.001953125\n\t"
        "mul.s $f8,$f8,%3\n\t"
        "mfc1 $8,$f8\n\t"
        "qmtc2 $8,$vf4\n\t"
        "lqc2 $vf1,0(%2)\n\t"
        "lqc2 $vf2,0(%0)\n\t"
        "vmr32.xyzw $vf3xyzw,$vf0xyzw\n\t"
        "vsub.xyz $vf1xyz,$vf3xyz,$vf1xyz\n\t"
        "vmulax.xyz ACCxyz,$vf1xyz,$vf4x\n\t"
        "vmaddw.xyz $vf1xyz,$vf2xyz,$vf0w\n\t"
        "sqc2 $vf1,0(%1)\n\t"
        "nop"
        : "+r"(second) : "r"(destination), "r"(first), "f"(parameter) : "$8", "$f8", "memory");
#else
    {
        extern char xsg_sym_McMathUnitVector[] __asm__("McMathUnitVector");
        XsgFpr f_3 = XSG_FIN(parameter);
        XsgGpr r_2 = XSG_IN(first);
        XsgGpr r_1 = XSG_IN(destination);
        XsgGpr r_0 = XSG_IN(second);
        xsg_ee_set32(&xsg_ee_gpr[8], (unsigned int)((((unsigned int)(unsigned long)xsg_sym_McMathUnitVector + 0x8000u) >> 16)) << 16);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_gpr[8].w[0] + (unsigned int)((int)(short)((unsigned int)(unsigned long)xsg_sym_McMathUnitVector & 0xffffu)));
        if (r_0.d[0] == 0) r_0.d[0] = xsg_ee_gpr[8].d[0];
        xsg_ee_fpr[8].f = 0.001953125f;
        xsg_ee_fpr[8].f = xsg_ee_fpr[8].f * f_3.f;
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_fpr[8].u);
        vu0_qmtc2(4, &xsg_ee_gpr[8]);
        vu0_lqc2(1, XSG_EA(r_2, 0));
        vu0_lqc2(2, XSG_EA(r_0, 0));
        vu0_mr32(15, 3, 0);
        vu0_arith(VU0_SUB, 14, 1, 3, 1, VU0_VEC);
        vu0_arith(VU0_MUL, 14, VU0_ACC, 1, 4, 0);
        vu0_arith(VU0_MADD, 14, 1, 2, 0, 3);
        vu0_sqc2(1, XSG_EA(r_1, 0));
        xsg_vu0_blocks++;
        XSG_OUT(second, r_0);
    }
#endif
    return destination;
}

Matrix4 *MMathRotateMatrixX(Matrix4 *out, const Matrix4 *matrix, float angle)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lui $8,%%hi(McMathUnitMatrix)\n\t"
        "addiu $8,$8,%%lo(McMathUnitMatrix)\n\t"
        "movz %0,$8,%0\n\t"
        "mfc1 $8,%2\n\t"
        "qmtc2 $8,$vf4\n\t"
        "vcallms 0xe8\n\t"
        "vmove.x $vf5x,$vf1x\n\t"
        "qmtc2 $8,$vf4\n\t"
        "vcallms 0x20\n\t"
        "vmove.x $vf6x,$vf1x\n\t"
        "lqc2 $vf1,0(%0)\n\t"
        "lqc2 $vf2,16(%0)\n\t"
        "lqc2 $vf3,32(%0)\n\t"
        "lqc2 $vf4,48(%0)\n\t"
        "sqc2 $vf1,0(%1)\n\t"
        "sqc2 $vf4,48(%1)\n\t"
        "vmulax.xyzw ACCxyzw,$vf2xyzw,$vf5x\n\t"
        "vmaddx.xyzw $vf1xyzw,$vf3xyzw,$vf6x\n\t"
        "sqc2 $vf1,16(%1)\n\t"
        "vmulax.xyzw ACCxyzw,$vf3xyzw,$vf5x\n\t"
        "vmsubx.xyzw $vf1xyzw,$vf2xyzw,$vf6x\n\t"
        "sqc2 $vf1,32(%1)\n\t"
        "nop"
        : "+r"(matrix) : "r"(out), "f"(angle) : "$8", "memory");
#else
    {
        extern char xsg_sym_McMathUnitMatrix[] __asm__("McMathUnitMatrix");
        XsgFpr f_2 = XSG_FIN(angle);
        XsgGpr r_1 = XSG_IN(out);
        XsgGpr r_0 = XSG_IN(matrix);
        xsg_ee_set32(&xsg_ee_gpr[8], (unsigned int)((((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix + 0x8000u) >> 16)) << 16);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_gpr[8].w[0] + (unsigned int)((int)(short)((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix & 0xffffu)));
        if (r_0.d[0] == 0) r_0.d[0] = xsg_ee_gpr[8].d[0];
        xsg_ee_set32(&xsg_ee_gpr[8], f_2.u);
        vu0_qmtc2(4, &xsg_ee_gpr[8]);
        vu0_callms(232);
        vu0_move(8, 5, 1);
        vu0_qmtc2(4, &xsg_ee_gpr[8]);
        vu0_callms(32);
        vu0_move(8, 6, 1);
        vu0_lqc2(1, XSG_EA(r_0, 0));
        vu0_lqc2(2, XSG_EA(r_0, 16));
        vu0_lqc2(3, XSG_EA(r_0, 32));
        vu0_lqc2(4, XSG_EA(r_0, 48));
        vu0_sqc2(1, XSG_EA(r_1, 0));
        vu0_sqc2(4, XSG_EA(r_1, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 5, 0);
        vu0_arith(VU0_MADD, 15, 1, 3, 6, 0);
        vu0_sqc2(1, XSG_EA(r_1, 16));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 3, 5, 0);
        vu0_arith(VU0_MSUB, 15, 1, 2, 6, 0);
        vu0_sqc2(1, XSG_EA(r_1, 32));
        xsg_vu0_blocks++;
        XSG_OUT(matrix, r_0);
    }
#endif
    return out;
}

Matrix4 *MMathRotateMatrixY(Matrix4 *out, const Matrix4 *matrix, float angle)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lui $8,%%hi(McMathUnitMatrix)\n\t"
        "addiu $8,$8,%%lo(McMathUnitMatrix)\n\t"
        "movz %0,$8,%0\n\t"
        "mfc1 $8,%2\n\t"
        "qmtc2 $8,$vf4\n\t"
        "vcallms 0xe8\n\t"
        "vmove.x $vf5x,$vf1x\n\t"
        "qmtc2 $8,$vf4\n\t"
        "vcallms 0x20\n\t"
        "vmove.x $vf6x,$vf1x\n\t"
        "lqc2 $vf1,0(%0)\n\t"
        "lqc2 $vf2,16(%0)\n\t"
        "lqc2 $vf3,32(%0)\n\t"
        "lqc2 $vf4,48(%0)\n\t"
        "sqc2 $vf2,16(%1)\n\t"
        "sqc2 $vf4,48(%1)\n\t"
        "vmulax.xyzw ACCxyzw,$vf1xyzw,$vf5x\n\t"
        "vmsubx.xyzw $vf2xyzw,$vf3xyzw,$vf6x\n\t"
        "sqc2 $vf2,0(%1)\n\t"
        "vmulax.xyzw ACCxyzw,$vf3xyzw,$vf5x\n\t"
        "vmaddx.xyzw $vf2xyzw,$vf1xyzw,$vf6x\n\t"
        "sqc2 $vf2,32(%1)\n\t"
        "nop"
        : "+r"(matrix) : "r"(out), "f"(angle) : "$8", "memory");
#else
    {
        extern char xsg_sym_McMathUnitMatrix[] __asm__("McMathUnitMatrix");
        XsgFpr f_2 = XSG_FIN(angle);
        XsgGpr r_1 = XSG_IN(out);
        XsgGpr r_0 = XSG_IN(matrix);
        xsg_ee_set32(&xsg_ee_gpr[8], (unsigned int)((((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix + 0x8000u) >> 16)) << 16);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_gpr[8].w[0] + (unsigned int)((int)(short)((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix & 0xffffu)));
        if (r_0.d[0] == 0) r_0.d[0] = xsg_ee_gpr[8].d[0];
        xsg_ee_set32(&xsg_ee_gpr[8], f_2.u);
        vu0_qmtc2(4, &xsg_ee_gpr[8]);
        vu0_callms(232);
        vu0_move(8, 5, 1);
        vu0_qmtc2(4, &xsg_ee_gpr[8]);
        vu0_callms(32);
        vu0_move(8, 6, 1);
        vu0_lqc2(1, XSG_EA(r_0, 0));
        vu0_lqc2(2, XSG_EA(r_0, 16));
        vu0_lqc2(3, XSG_EA(r_0, 32));
        vu0_lqc2(4, XSG_EA(r_0, 48));
        vu0_sqc2(2, XSG_EA(r_1, 16));
        vu0_sqc2(4, XSG_EA(r_1, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 1, 5, 0);
        vu0_arith(VU0_MSUB, 15, 2, 3, 6, 0);
        vu0_sqc2(2, XSG_EA(r_1, 0));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 3, 5, 0);
        vu0_arith(VU0_MADD, 15, 2, 1, 6, 0);
        vu0_sqc2(2, XSG_EA(r_1, 32));
        xsg_vu0_blocks++;
        XSG_OUT(matrix, r_0);
    }
#endif
    return out;
}

Matrix4 *MMathRotateMatrixZ(Matrix4 *out, const Matrix4 *matrix, float angle)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lui $8,%%hi(McMathUnitMatrix)\n\t"
        "addiu $8,$8,%%lo(McMathUnitMatrix)\n\t"
        "movz %0,$8,%0\n\t"
        "mfc1 $8,%2\n\t"
        "qmtc2 $8,$vf4\n\t"
        "vcallms 0xe8\n\t"
        "vmove.x $vf5x,$vf1x\n\t"
        "qmtc2 $8,$vf4\n\t"
        "vcallms 0x20\n\t"
        "vmove.x $vf6x,$vf1x\n\t"
        "lqc2 $vf1,0(%0)\n\t"
        "lqc2 $vf2,16(%0)\n\t"
        "lqc2 $vf3,32(%0)\n\t"
        "lqc2 $vf4,48(%0)\n\t"
        "sqc2 $vf3,32(%1)\n\t"
        "sqc2 $vf4,48(%1)\n\t"
        "vmulax.xyzw ACCxyzw,$vf1xyzw,$vf5x\n\t"
        "vmaddx.xyzw $vf3xyzw,$vf2xyzw,$vf6x\n\t"
        "sqc2 $vf3,0(%1)\n\t"
        "vmulax.xyzw ACCxyzw,$vf2xyzw,$vf5x\n\t"
        "vmsubx.xyzw $vf3xyzw,$vf1xyzw,$vf6x\n\t"
        "sqc2 $vf3,16(%1)\n\t"
        "nop"
        : "+r"(matrix) : "r"(out), "f"(angle) : "$8", "memory");
#else
    {
        extern char xsg_sym_McMathUnitMatrix[] __asm__("McMathUnitMatrix");
        XsgFpr f_2 = XSG_FIN(angle);
        XsgGpr r_1 = XSG_IN(out);
        XsgGpr r_0 = XSG_IN(matrix);
        xsg_ee_set32(&xsg_ee_gpr[8], (unsigned int)((((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix + 0x8000u) >> 16)) << 16);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_gpr[8].w[0] + (unsigned int)((int)(short)((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix & 0xffffu)));
        if (r_0.d[0] == 0) r_0.d[0] = xsg_ee_gpr[8].d[0];
        xsg_ee_set32(&xsg_ee_gpr[8], f_2.u);
        vu0_qmtc2(4, &xsg_ee_gpr[8]);
        vu0_callms(232);
        vu0_move(8, 5, 1);
        vu0_qmtc2(4, &xsg_ee_gpr[8]);
        vu0_callms(32);
        vu0_move(8, 6, 1);
        vu0_lqc2(1, XSG_EA(r_0, 0));
        vu0_lqc2(2, XSG_EA(r_0, 16));
        vu0_lqc2(3, XSG_EA(r_0, 32));
        vu0_lqc2(4, XSG_EA(r_0, 48));
        vu0_sqc2(3, XSG_EA(r_1, 32));
        vu0_sqc2(4, XSG_EA(r_1, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 1, 5, 0);
        vu0_arith(VU0_MADD, 15, 3, 2, 6, 0);
        vu0_sqc2(3, XSG_EA(r_1, 0));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 5, 0);
        vu0_arith(VU0_MSUB, 15, 3, 1, 6, 0);
        vu0_sqc2(3, XSG_EA(r_1, 16));
        xsg_vu0_blocks++;
        XSG_OUT(matrix, r_0);
    }
#endif
    return out;
}

Matrix4 *MMathRotateMatrixXY(Matrix4 *out, const Matrix4 *matrix, const Vector4 *angles)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lui $8,%%hi(McMathUnitMatrix)\n\t"
        "addiu $8,$8,%%lo(McMathUnitMatrix)\n\t"
        "movz %0,$8,%0\n\t"
        "lqc2 $vf7,0(%2)\n\t"
        "vaddx.x $vf4x,$vf0x,$vf7x\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.x $vf5x,$vf0x,$vf1x\n\t"
        "vaddy.x $vf4x,$vf0x,$vf7y\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.y $vf5y,$vf0y,$vf1x\n\t"
        "vaddx.x $vf4x,$vf0x,$vf7x\n\t"
        "vcallms 0x20\n\t"
        "vaddx.x $vf6x,$vf0x,$vf1x\n\t"
        "vaddy.x $vf4x,$vf0x,$vf7y\n\t"
        "vcallms 0x20\n\t"
        "vaddx.y $vf6y,$vf0y,$vf1x\n\t"
        "lqc2 $vf1,0(%0)\n\t"
        "lqc2 $vf2,16(%0)\n\t"
        "lqc2 $vf3,32(%0)\n\t"
        "lqc2 $vf4,48(%0)\n\t"
        "vmulax.xyzw ACCxyzw,$vf2xyzw,$vf5x\n\t"
        "vmaddx.xyzw $vf7xyzw,$vf3xyzw,$vf6x\n\t"
        "vmulax.xyzw ACCxyzw,$vf3xyzw,$vf5x\n\t"
        "vmsubx.xyzw $vf3xyzw,$vf2xyzw,$vf6x\n\t"
        "vmulay.xyzw ACCxyzw,$vf1xyzw,$vf5y\n\t"
        "vmsuby.xyzw $vf8xyzw,$vf3xyzw,$vf6y\n\t"
        "vmulay.xyzw ACCxyzw,$vf3xyzw,$vf5y\n\t"
        "vmaddy.xyzw $vf3xyzw,$vf1xyzw,$vf6y\n\t"
        "sqc2 $vf8,0(%1)\n\t"
        "sqc2 $vf7,16(%1)\n\t"
        "sqc2 $vf3,32(%1)\n\t"
        "sqc2 $vf4,48(%1)\n\t"
        "nop"
        : "+r"(matrix) : "r"(out), "r"(angles) : "$8", "memory");
#else
    {
        extern char xsg_sym_McMathUnitMatrix[] __asm__("McMathUnitMatrix");
        XsgGpr r_2 = XSG_IN(angles);
        XsgGpr r_1 = XSG_IN(out);
        XsgGpr r_0 = XSG_IN(matrix);
        xsg_ee_set32(&xsg_ee_gpr[8], (unsigned int)((((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix + 0x8000u) >> 16)) << 16);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_gpr[8].w[0] + (unsigned int)((int)(short)((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix & 0xffffu)));
        if (r_0.d[0] == 0) r_0.d[0] = xsg_ee_gpr[8].d[0];
        vu0_lqc2(7, XSG_EA(r_2, 0));
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 0);
        vu0_callms(232);
        vu0_arith(VU0_ADD, 8, 5, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 1);
        vu0_callms(232);
        vu0_arith(VU0_ADD, 4, 5, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 0);
        vu0_callms(32);
        vu0_arith(VU0_ADD, 8, 6, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 1);
        vu0_callms(32);
        vu0_arith(VU0_ADD, 4, 6, 0, 1, 0);
        vu0_lqc2(1, XSG_EA(r_0, 0));
        vu0_lqc2(2, XSG_EA(r_0, 16));
        vu0_lqc2(3, XSG_EA(r_0, 32));
        vu0_lqc2(4, XSG_EA(r_0, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 5, 0);
        vu0_arith(VU0_MADD, 15, 7, 3, 6, 0);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 3, 5, 0);
        vu0_arith(VU0_MSUB, 15, 3, 2, 6, 0);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 1, 5, 1);
        vu0_arith(VU0_MSUB, 15, 8, 3, 6, 1);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 3, 5, 1);
        vu0_arith(VU0_MADD, 15, 3, 1, 6, 1);
        vu0_sqc2(8, XSG_EA(r_1, 0));
        vu0_sqc2(7, XSG_EA(r_1, 16));
        vu0_sqc2(3, XSG_EA(r_1, 32));
        vu0_sqc2(4, XSG_EA(r_1, 48));
        xsg_vu0_blocks++;
        XSG_OUT(matrix, r_0);
    }
#endif
    return out;
}

Matrix4 *MMathRotateMatrixYX(Matrix4 *out, const Matrix4 *matrix, const Vector4 *angles)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lui $8,%%hi(McMathUnitMatrix)\n\t"
        "addiu $8,$8,%%lo(McMathUnitMatrix)\n\t"
        "movz %0,$8,%0\n\t"
        "lqc2 $vf7,0(%2)\n\t"
        "vaddx.x $vf4x,$vf0x,$vf7x\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.x $vf5x,$vf0x,$vf1x\n\t"
        "vaddy.x $vf4x,$vf0x,$vf7y\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.y $vf5y,$vf0y,$vf1x\n\t"
        "vaddx.x $vf4x,$vf0x,$vf7x\n\t"
        "vcallms 0x20\n\t"
        "vaddx.x $vf6x,$vf0x,$vf1x\n\t"
        "vaddy.x $vf4x,$vf0x,$vf7y\n\t"
        "vcallms 0x20\n\t"
        "vaddx.y $vf6y,$vf0y,$vf1x\n\t"
        "lqc2 $vf1,0(%0)\n\t"
        "lqc2 $vf2,16(%0)\n\t"
        "lqc2 $vf3,32(%0)\n\t"
        "lqc2 $vf4,48(%0)\n\t"
        "vmulay.xyzw ACCxyzw,$vf1xyzw,$vf5y\n\t"
        "vmsuby.xyzw $vf7xyzw,$vf3xyzw,$vf6y\n\t"
        "vmulay.xyzw ACCxyzw,$vf3xyzw,$vf5y\n\t"
        "vmaddy.xyzw $vf3xyzw,$vf1xyzw,$vf6y\n\t"
        "vmulax.xyzw ACCxyzw,$vf2xyzw,$vf5x\n\t"
        "vmaddx.xyzw $vf8xyzw,$vf3xyzw,$vf6x\n\t"
        "vmulax.xyzw ACCxyzw,$vf3xyzw,$vf5x\n\t"
        "vmsubx.xyzw $vf3xyzw,$vf2xyzw,$vf6x\n\t"
        "sqc2 $vf7,0(%1)\n\t"
        "sqc2 $vf8,16(%1)\n\t"
        "sqc2 $vf3,32(%1)\n\t"
        "sqc2 $vf4,48(%1)\n\t"
        "nop"
        : "+r"(matrix) : "r"(out), "r"(angles) : "$8", "memory");
#else
    {
        extern char xsg_sym_McMathUnitMatrix[] __asm__("McMathUnitMatrix");
        XsgGpr r_2 = XSG_IN(angles);
        XsgGpr r_1 = XSG_IN(out);
        XsgGpr r_0 = XSG_IN(matrix);
        xsg_ee_set32(&xsg_ee_gpr[8], (unsigned int)((((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix + 0x8000u) >> 16)) << 16);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_gpr[8].w[0] + (unsigned int)((int)(short)((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix & 0xffffu)));
        if (r_0.d[0] == 0) r_0.d[0] = xsg_ee_gpr[8].d[0];
        vu0_lqc2(7, XSG_EA(r_2, 0));
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 0);
        vu0_callms(232);
        vu0_arith(VU0_ADD, 8, 5, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 1);
        vu0_callms(232);
        vu0_arith(VU0_ADD, 4, 5, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 0);
        vu0_callms(32);
        vu0_arith(VU0_ADD, 8, 6, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 1);
        vu0_callms(32);
        vu0_arith(VU0_ADD, 4, 6, 0, 1, 0);
        vu0_lqc2(1, XSG_EA(r_0, 0));
        vu0_lqc2(2, XSG_EA(r_0, 16));
        vu0_lqc2(3, XSG_EA(r_0, 32));
        vu0_lqc2(4, XSG_EA(r_0, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 1, 5, 1);
        vu0_arith(VU0_MSUB, 15, 7, 3, 6, 1);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 3, 5, 1);
        vu0_arith(VU0_MADD, 15, 3, 1, 6, 1);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 5, 0);
        vu0_arith(VU0_MADD, 15, 8, 3, 6, 0);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 3, 5, 0);
        vu0_arith(VU0_MSUB, 15, 3, 2, 6, 0);
        vu0_sqc2(7, XSG_EA(r_1, 0));
        vu0_sqc2(8, XSG_EA(r_1, 16));
        vu0_sqc2(3, XSG_EA(r_1, 32));
        vu0_sqc2(4, XSG_EA(r_1, 48));
        xsg_vu0_blocks++;
        XSG_OUT(matrix, r_0);
    }
#endif
    return out;
}

Matrix4 *MMathRotateMatrixXYZ(Matrix4 *out, const Matrix4 *matrix, const Vector4 *angles)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lui $8,%%hi(McMathUnitMatrix)\n\t"
        "addiu $8,$8,%%lo(McMathUnitMatrix)\n\t"
        "movz %0,$8,%0\n\t"
        "lqc2 $vf7,0(%2)\n\t"
        "vaddx.x $vf4x,$vf0x,$vf7x\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.x $vf5x,$vf0x,$vf1x\n\t"
        "vaddy.x $vf4x,$vf0x,$vf7y\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.y $vf5y,$vf0y,$vf1x\n\t"
        "vaddz.x $vf4x,$vf0x,$vf7z\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.z $vf5z,$vf0z,$vf1x\n\t"
        "vaddx.x $vf4x,$vf0x,$vf7x\n\t"
        "vcallms 0x20\n\t"
        "vaddx.x $vf6x,$vf0x,$vf1x\n\t"
        "vaddy.x $vf4x,$vf0x,$vf7y\n\t"
        "vcallms 0x20\n\t"
        "vaddx.y $vf6y,$vf0y,$vf1x\n\t"
        "vaddz.x $vf4x,$vf0x,$vf7z\n\t"
        "vcallms 0x20\n\t"
        "vaddx.z $vf6z,$vf0z,$vf1x\n\t"
        "lqc2 $vf1,0(%0)\n\t"
        "lqc2 $vf2,16(%0)\n\t"
        "lqc2 $vf3,32(%0)\n\t"
        "lqc2 $vf4,48(%0)\n\t"
        "vmulax.xyzw ACCxyzw,$vf2xyzw,$vf5x\n\t"
        "vmaddx.xyzw $vf7xyzw,$vf3xyzw,$vf6x\n\t"
        "vmulax.xyzw ACCxyzw,$vf3xyzw,$vf5x\n\t"
        "vmsubx.xyzw $vf3xyzw,$vf2xyzw,$vf6x\n\t"
        "vmulay.xyzw ACCxyzw,$vf1xyzw,$vf5y\n\t"
        "vmsuby.xyzw $vf8xyzw,$vf3xyzw,$vf6y\n\t"
        "vmulay.xyzw ACCxyzw,$vf3xyzw,$vf5y\n\t"
        "vmaddy.xyzw $vf3xyzw,$vf1xyzw,$vf6y\n\t"
        "vmulaz.xyzw ACCxyzw,$vf8xyzw,$vf5z\n\t"
        "vmaddz.xyzw $vf1xyzw,$vf7xyzw,$vf6z\n\t"
        "vmulaz.xyzw ACCxyzw,$vf7xyzw,$vf5z\n\t"
        "vmsubz.xyzw $vf2xyzw,$vf8xyzw,$vf6z\n\t"
        "sqc2 $vf1,0(%1)\n\t"
        "sqc2 $vf2,16(%1)\n\t"
        "sqc2 $vf3,32(%1)\n\t"
        "sqc2 $vf4,48(%1)\n\t"
        "nop"
        : "+r"(matrix) : "r"(out), "r"(angles) : "$8", "memory");
#else
    {
        extern char xsg_sym_McMathUnitMatrix[] __asm__("McMathUnitMatrix");
        XsgGpr r_2 = XSG_IN(angles);
        XsgGpr r_1 = XSG_IN(out);
        XsgGpr r_0 = XSG_IN(matrix);
        xsg_ee_set32(&xsg_ee_gpr[8], (unsigned int)((((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix + 0x8000u) >> 16)) << 16);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_gpr[8].w[0] + (unsigned int)((int)(short)((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix & 0xffffu)));
        if (r_0.d[0] == 0) r_0.d[0] = xsg_ee_gpr[8].d[0];
        vu0_lqc2(7, XSG_EA(r_2, 0));
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 0);
        vu0_callms(232);
        vu0_arith(VU0_ADD, 8, 5, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 1);
        vu0_callms(232);
        vu0_arith(VU0_ADD, 4, 5, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 2);
        vu0_callms(232);
        vu0_arith(VU0_ADD, 2, 5, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 0);
        vu0_callms(32);
        vu0_arith(VU0_ADD, 8, 6, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 1);
        vu0_callms(32);
        vu0_arith(VU0_ADD, 4, 6, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 2);
        vu0_callms(32);
        vu0_arith(VU0_ADD, 2, 6, 0, 1, 0);
        vu0_lqc2(1, XSG_EA(r_0, 0));
        vu0_lqc2(2, XSG_EA(r_0, 16));
        vu0_lqc2(3, XSG_EA(r_0, 32));
        vu0_lqc2(4, XSG_EA(r_0, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 5, 0);
        vu0_arith(VU0_MADD, 15, 7, 3, 6, 0);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 3, 5, 0);
        vu0_arith(VU0_MSUB, 15, 3, 2, 6, 0);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 1, 5, 1);
        vu0_arith(VU0_MSUB, 15, 8, 3, 6, 1);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 3, 5, 1);
        vu0_arith(VU0_MADD, 15, 3, 1, 6, 1);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 8, 5, 2);
        vu0_arith(VU0_MADD, 15, 1, 7, 6, 2);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 7, 5, 2);
        vu0_arith(VU0_MSUB, 15, 2, 8, 6, 2);
        vu0_sqc2(1, XSG_EA(r_1, 0));
        vu0_sqc2(2, XSG_EA(r_1, 16));
        vu0_sqc2(3, XSG_EA(r_1, 32));
        vu0_sqc2(4, XSG_EA(r_1, 48));
        xsg_vu0_blocks++;
        XSG_OUT(matrix, r_0);
    }
#endif
    return out;
}

Matrix4 *MMathRotateMatrixYXZ(Matrix4 *out, const Matrix4 *matrix, const Vector4 *angles)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lui $8,%%hi(McMathUnitMatrix)\n\t"
        "addiu $8,$8,%%lo(McMathUnitMatrix)\n\t"
        "movz %0,$8,%0\n\t"
        "lqc2 $vf7,0(%2)\n\t"
        "vaddx.x $vf4x,$vf0x,$vf7x\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.x $vf5x,$vf0x,$vf1x\n\t"
        "vaddy.x $vf4x,$vf0x,$vf7y\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.y $vf5y,$vf0y,$vf1x\n\t"
        "vaddz.x $vf4x,$vf0x,$vf7z\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.z $vf5z,$vf0z,$vf1x\n\t"
        "vaddx.x $vf4x,$vf0x,$vf7x\n\t"
        "vcallms 0x20\n\t"
        "vaddx.x $vf6x,$vf0x,$vf1x\n\t"
        "vaddy.x $vf4x,$vf0x,$vf7y\n\t"
        "vcallms 0x20\n\t"
        "vaddx.y $vf6y,$vf0y,$vf1x\n\t"
        "vaddz.x $vf4x,$vf0x,$vf7z\n\t"
        "vcallms 0x20\n\t"
        "vaddx.z $vf6z,$vf0z,$vf1x\n\t"
        "lqc2 $vf1,0(%0)\n\t"
        "lqc2 $vf2,16(%0)\n\t"
        "lqc2 $vf3,32(%0)\n\t"
        "lqc2 $vf4,48(%0)\n\t"
        "vmulay.xyzw ACCxyzw,$vf1xyzw,$vf5y\n\t"
        "vmsuby.xyzw $vf7xyzw,$vf3xyzw,$vf6y\n\t"
        "vmulay.xyzw ACCxyzw,$vf3xyzw,$vf5y\n\t"
        "vmaddy.xyzw $vf3xyzw,$vf1xyzw,$vf6y\n\t"
        "vmulax.xyzw ACCxyzw,$vf2xyzw,$vf5x\n\t"
        "vmaddx.xyzw $vf8xyzw,$vf3xyzw,$vf6x\n\t"
        "vmulax.xyzw ACCxyzw,$vf3xyzw,$vf5x\n\t"
        "vmsubx.xyzw $vf3xyzw,$vf2xyzw,$vf6x\n\t"
        "vmulaz.xyzw ACCxyzw,$vf7xyzw,$vf5z\n\t"
        "vmaddz.xyzw $vf1xyzw,$vf8xyzw,$vf6z\n\t"
        "vmulaz.xyzw ACCxyzw,$vf8xyzw,$vf5z\n\t"
        "vmsubz.xyzw $vf2xyzw,$vf7xyzw,$vf6z\n\t"
        "sqc2 $vf1,0(%1)\n\t"
        "sqc2 $vf2,16(%1)\n\t"
        "sqc2 $vf3,32(%1)\n\t"
        "sqc2 $vf4,48(%1)\n\t"
        "nop"
        : "+r"(matrix) : "r"(out), "r"(angles) : "$8", "memory");
#else
    {
        extern char xsg_sym_McMathUnitMatrix[] __asm__("McMathUnitMatrix");
        XsgGpr r_2 = XSG_IN(angles);
        XsgGpr r_1 = XSG_IN(out);
        XsgGpr r_0 = XSG_IN(matrix);
        xsg_ee_set32(&xsg_ee_gpr[8], (unsigned int)((((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix + 0x8000u) >> 16)) << 16);
        xsg_ee_set32(&xsg_ee_gpr[8], xsg_ee_gpr[8].w[0] + (unsigned int)((int)(short)((unsigned int)(unsigned long)xsg_sym_McMathUnitMatrix & 0xffffu)));
        if (r_0.d[0] == 0) r_0.d[0] = xsg_ee_gpr[8].d[0];
        vu0_lqc2(7, XSG_EA(r_2, 0));
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 0);
        vu0_callms(232);
        vu0_arith(VU0_ADD, 8, 5, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 1);
        vu0_callms(232);
        vu0_arith(VU0_ADD, 4, 5, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 2);
        vu0_callms(232);
        vu0_arith(VU0_ADD, 2, 5, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 0);
        vu0_callms(32);
        vu0_arith(VU0_ADD, 8, 6, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 1);
        vu0_callms(32);
        vu0_arith(VU0_ADD, 4, 6, 0, 1, 0);
        vu0_arith(VU0_ADD, 8, 4, 0, 7, 2);
        vu0_callms(32);
        vu0_arith(VU0_ADD, 2, 6, 0, 1, 0);
        vu0_lqc2(1, XSG_EA(r_0, 0));
        vu0_lqc2(2, XSG_EA(r_0, 16));
        vu0_lqc2(3, XSG_EA(r_0, 32));
        vu0_lqc2(4, XSG_EA(r_0, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 1, 5, 1);
        vu0_arith(VU0_MSUB, 15, 7, 3, 6, 1);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 3, 5, 1);
        vu0_arith(VU0_MADD, 15, 3, 1, 6, 1);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 5, 0);
        vu0_arith(VU0_MADD, 15, 8, 3, 6, 0);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 3, 5, 0);
        vu0_arith(VU0_MSUB, 15, 3, 2, 6, 0);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 7, 5, 2);
        vu0_arith(VU0_MADD, 15, 1, 8, 6, 2);
        vu0_arith(VU0_MUL, 15, VU0_ACC, 8, 5, 2);
        vu0_arith(VU0_MSUB, 15, 2, 7, 6, 2);
        vu0_sqc2(1, XSG_EA(r_1, 0));
        vu0_sqc2(2, XSG_EA(r_1, 16));
        vu0_sqc2(3, XSG_EA(r_1, 32));
        vu0_sqc2(4, XSG_EA(r_1, 48));
        xsg_vu0_blocks++;
        XSG_OUT(matrix, r_0);
    }
#endif
    return out;
}

Vector4 *MMathApplyMatrix(Vector4 *out, const Matrix4 *matrix, const Vector4 *vector)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 $vf1,0(%2)\n\t"
        "lqc2 $vf2,0(%1)\n\t"
        "lqc2 $vf3,16(%1)\n\t"
        "lqc2 $vf4,32(%1)\n\t"
        "lqc2 $vf5,48(%1)\n\t"
        "vmulax.xyzw ACCxyzw,$vf2xyzw,$vf1x\n\t"
        "vmadday.xyzw ACCxyzw,$vf3xyzw,$vf1y\n\t"
        "vmaddaz.xyzw ACCxyzw,$vf4xyzw,$vf1z\n\t"
        "vmaddw.xyzw $vf1xyzw,$vf5xyzw,$vf1w\n\t"
        "sqc2 $vf1,0(%0)\n\t"
        "nop"
        : : "r"(out), "r"(matrix), "r"(vector) : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(vector);
        XsgGpr r_1 = XSG_IN(matrix);
        XsgGpr r_0 = XSG_IN(out);
        vu0_lqc2(1, XSG_EA(r_2, 0));
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_lqc2(3, XSG_EA(r_1, 16));
        vu0_lqc2(4, XSG_EA(r_1, 32));
        vu0_lqc2(5, XSG_EA(r_1, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 1, 0);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 3, 1, 1);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 4, 1, 2);
        vu0_arith(VU0_MADD, 15, 1, 5, 1, 3);
        vu0_sqc2(1, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
    return out;
}

Matrix4 *MMathMulMatrix(Matrix4 *out, const Matrix4 *left, const Matrix4 *right)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 $vf2,0(%1)\n\t"
        "lqc2 $vf3,16(%1)\n\t"
        "lqc2 $vf4,32(%1)\n\t"
        "lqc2 $vf5,48(%1)\n\t"
        "lqc2 $vf1,0(%2)\n\t"
        "vmulax.xyzw ACCxyzw,$vf2xyzw,$vf1x\n\t"
        "vmadday.xyzw ACCxyzw,$vf3xyzw,$vf1y\n\t"
        "vmaddaz.xyzw ACCxyzw,$vf4xyzw,$vf1z\n\t"
        "vmaddw.xyzw $vf6xyzw,$vf5xyzw,$vf1w\n\t"
        "sqc2 $vf6,0(%0)\n\t"
        "lqc2 $vf1,16(%2)\n\t"
        "vmulax.xyzw ACCxyzw,$vf2xyzw,$vf1x\n\t"
        "vmadday.xyzw ACCxyzw,$vf3xyzw,$vf1y\n\t"
        "vmaddaz.xyzw ACCxyzw,$vf4xyzw,$vf1z\n\t"
        "vmaddw.xyzw $vf6xyzw,$vf5xyzw,$vf1w\n\t"
        "sqc2 $vf6,16(%0)\n\t"
        "lqc2 $vf1,32(%2)\n\t"
        "vmulax.xyzw ACCxyzw,$vf2xyzw,$vf1x\n\t"
        "vmadday.xyzw ACCxyzw,$vf3xyzw,$vf1y\n\t"
        "vmaddaz.xyzw ACCxyzw,$vf4xyzw,$vf1z\n\t"
        "vmaddw.xyzw $vf6xyzw,$vf5xyzw,$vf1w\n\t"
        "sqc2 $vf6,32(%0)\n\t"
        "lqc2 $vf1,48(%2)\n\t"
        "vmulax.xyzw ACCxyzw,$vf2xyzw,$vf1x\n\t"
        "vmadday.xyzw ACCxyzw,$vf3xyzw,$vf1y\n\t"
        "vmaddaz.xyzw ACCxyzw,$vf4xyzw,$vf1z\n\t"
        "vmaddw.xyzw $vf6xyzw,$vf5xyzw,$vf1w\n\t"
        "sqc2 $vf6,48(%0)\n\t"
        "nop"
        : : "r"(out), "r"(left), "r"(right) : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(right);
        XsgGpr r_1 = XSG_IN(left);
        XsgGpr r_0 = XSG_IN(out);
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_lqc2(3, XSG_EA(r_1, 16));
        vu0_lqc2(4, XSG_EA(r_1, 32));
        vu0_lqc2(5, XSG_EA(r_1, 48));
        vu0_lqc2(1, XSG_EA(r_2, 0));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 1, 0);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 3, 1, 1);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 4, 1, 2);
        vu0_arith(VU0_MADD, 15, 6, 5, 1, 3);
        vu0_sqc2(6, XSG_EA(r_0, 0));
        vu0_lqc2(1, XSG_EA(r_2, 16));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 1, 0);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 3, 1, 1);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 4, 1, 2);
        vu0_arith(VU0_MADD, 15, 6, 5, 1, 3);
        vu0_sqc2(6, XSG_EA(r_0, 16));
        vu0_lqc2(1, XSG_EA(r_2, 32));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 1, 0);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 3, 1, 1);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 4, 1, 2);
        vu0_arith(VU0_MADD, 15, 6, 5, 1, 3);
        vu0_sqc2(6, XSG_EA(r_0, 32));
        vu0_lqc2(1, XSG_EA(r_2, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 1, 0);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 3, 1, 1);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 4, 1, 2);
        vu0_arith(VU0_MADD, 15, 6, 5, 1, 3);
        vu0_sqc2(6, XSG_EA(r_0, 48));
        xsg_vu0_blocks++;
    }
#endif
    return out;
}

Matrix4 *MMathScaleMatrix(Matrix4 *out, const Matrix4 *matrix, const Vector4 *scale)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 $vf5,0(%2)\n\t"
        "lqc2 $vf1,0(%1)\n\t"
        "lqc2 $vf2,16(%1)\n\t"
        "lqc2 $vf3,32(%1)\n\t"
        "lqc2 $vf4,48(%1)\n\t"
        "vmulx.xyzw $vf1xyzw,$vf1xyzw,$vf5x\n\t"
        "vmuly.xyzw $vf2xyzw,$vf2xyzw,$vf5y\n\t"
        "vmulz.xyzw $vf3xyzw,$vf3xyzw,$vf5z\n\t"
        "vmulw.xyzw $vf4xyzw,$vf4xyzw,$vf5w\n\t"
        "sqc2 $vf1,0(%0)\n\t"
        "sqc2 $vf2,16(%0)\n\t"
        "sqc2 $vf3,32(%0)\n\t"
        "sqc2 $vf4,48(%0)\n\t"
        "nop"
        : : "r"(out), "r"(matrix), "r"(scale) : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(scale);
        XsgGpr r_1 = XSG_IN(matrix);
        XsgGpr r_0 = XSG_IN(out);
        vu0_lqc2(5, XSG_EA(r_2, 0));
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_1, 16));
        vu0_lqc2(3, XSG_EA(r_1, 32));
        vu0_lqc2(4, XSG_EA(r_1, 48));
        vu0_arith(VU0_MUL, 15, 1, 1, 5, 0);
        vu0_arith(VU0_MUL, 15, 2, 2, 5, 1);
        vu0_arith(VU0_MUL, 15, 3, 3, 5, 2);
        vu0_arith(VU0_MUL, 15, 4, 4, 5, 3);
        vu0_sqc2(1, XSG_EA(r_0, 0));
        vu0_sqc2(2, XSG_EA(r_0, 16));
        vu0_sqc2(3, XSG_EA(r_0, 32));
        vu0_sqc2(4, XSG_EA(r_0, 48));
        xsg_vu0_blocks++;
    }
#endif
    return out;
}

Matrix4 *MMathScaleMatrixS(Matrix4 *out, const Matrix4 *matrix, float scale)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $8,%2\n\t"
        "qmtc2 $8,$vf5\n\t"
        "lqc2 $vf1,0(%1)\n\t"
        "lqc2 $vf2,16(%1)\n\t"
        "lqc2 $vf3,32(%1)\n\t"
        "lqc2 $vf4,48(%1)\n\t"
        "vmulx.xyzw $vf1xyzw,$vf1xyzw,$vf5x\n\t"
        "vmulx.xyzw $vf2xyzw,$vf2xyzw,$vf5x\n\t"
        "vmulx.xyzw $vf3xyzw,$vf3xyzw,$vf5x\n\t"
        "vmulx.xyzw $vf4xyzw,$vf4xyzw,$vf5x\n\t"
        "sqc2 $vf1,0(%0)\n\t"
        "sqc2 $vf2,16(%0)\n\t"
        "sqc2 $vf3,32(%0)\n\t"
        "sqc2 $vf4,48(%0)\n\t"
        "nop"
        : : "r"(out), "r"(matrix), "f"(scale) : "$8", "memory");
#else
    {
        XsgFpr f_2 = XSG_FIN(scale);
        XsgGpr r_1 = XSG_IN(matrix);
        XsgGpr r_0 = XSG_IN(out);
        xsg_ee_set32(&xsg_ee_gpr[8], f_2.u);
        vu0_qmtc2(5, &xsg_ee_gpr[8]);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_1, 16));
        vu0_lqc2(3, XSG_EA(r_1, 32));
        vu0_lqc2(4, XSG_EA(r_1, 48));
        vu0_arith(VU0_MUL, 15, 1, 1, 5, 0);
        vu0_arith(VU0_MUL, 15, 2, 2, 5, 0);
        vu0_arith(VU0_MUL, 15, 3, 3, 5, 0);
        vu0_arith(VU0_MUL, 15, 4, 4, 5, 0);
        vu0_sqc2(1, XSG_EA(r_0, 0));
        vu0_sqc2(2, XSG_EA(r_0, 16));
        vu0_sqc2(3, XSG_EA(r_0, 32));
        vu0_sqc2(4, XSG_EA(r_0, 48));
        xsg_vu0_blocks++;
    }
#endif
    return out;
}

Matrix4 *MMathTranslateMatrix(Matrix4 *out, const Matrix4 *matrix, const Vector4 *translation)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 $vf5,0(%2)\n\t"
        "lqc2 $vf1,0(%1)\n\t"
        "lqc2 $vf2,16(%1)\n\t"
        "lqc2 $vf3,32(%1)\n\t"
        "lqc2 $vf4,48(%1)\n\t"
        "vmulax.xyzw ACCxyzw,$vf1xyzw,$vf5x\n\t"
        "vmadday.xyzw ACCxyzw,$vf2xyzw,$vf5y\n\t"
        "vmaddaz.xyzw ACCxyzw,$vf3xyzw,$vf5z\n\t"
        "vmaddw.xyzw $vf4xyzw,$vf4xyzw,$vf0w\n\t"
        "sqc2 $vf1,0(%0)\n\t"
        "sqc2 $vf2,16(%0)\n\t"
        "sqc2 $vf3,32(%0)\n\t"
        "sqc2 $vf4,48(%0)\n\t"
        "nop"
        : : "r"(out), "r"(matrix), "r"(translation) : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(translation);
        XsgGpr r_1 = XSG_IN(matrix);
        XsgGpr r_0 = XSG_IN(out);
        vu0_lqc2(5, XSG_EA(r_2, 0));
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_1, 16));
        vu0_lqc2(3, XSG_EA(r_1, 32));
        vu0_lqc2(4, XSG_EA(r_1, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 1, 5, 0);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 2, 5, 1);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 3, 5, 2);
        vu0_arith(VU0_MADD, 15, 4, 4, 0, 3);
        vu0_sqc2(1, XSG_EA(r_0, 0));
        vu0_sqc2(2, XSG_EA(r_0, 16));
        vu0_sqc2(3, XSG_EA(r_0, 32));
        vu0_sqc2(4, XSG_EA(r_0, 48));
        xsg_vu0_blocks++;
    }
#endif
    return out;
}

/*
 * MMathCalcMatrixVector: build an orientation matrix that faces "direction",
 * starting from the identity (built in VU0 registers via vmr32 rotations of
 * the hardwired vf0 = (0,0,0,1), instead of loading McMathUnitMatrix from
 * memory) and then applying a yaw rotation (atan2 of direction.x,
 * direction.z) followed by a pitch rotation (atan2 of direction.y, the
 * vector's XZ length). When direction has no XZ component the yaw rotation
 * is skipped (undefined for a vector pointing straight up/down).
 */
Matrix4 *MMathCalcMatrixVector(Matrix4 *out, const Vector4 *direction)
{
    float directionX;

#ifndef PORT_HOST
    PS2_ASMV("vmr32.xyzw vf1, vf0" : : : "memory");
#else
    {
        vu0_mr32(15, 1, 0);
        xsg_vu0_blocks++;
    }
#endif
#ifndef PORT_HOST
    PS2_ASMV("vmr32.xyzw vf2, vf1" : : : "memory");
#else
    {
        vu0_mr32(15, 2, 1);
        xsg_vu0_blocks++;
    }
#endif
#ifndef PORT_HOST
    PS2_ASMV("vmr32.xyzw vf3, vf2" : : : "memory");
#else
    {
        vu0_mr32(15, 3, 2);
        xsg_vu0_blocks++;
    }
#endif
#ifndef PORT_HOST
    PS2_ASMV("sqc2 vf0, 48(%0)" : : "r"(out) : "memory");
#else
    {
        XsgGpr r_0 = XSG_IN(out);
        vu0_sqc2(0, XSG_EA(r_0, 48));
        xsg_vu0_blocks++;
    }
#endif
#ifndef PORT_HOST
    PS2_ASMV("sqc2 vf1, 32(%0)" : : "r"(out) : "memory");
#else
    {
        XsgGpr r_0 = XSG_IN(out);
        vu0_sqc2(1, XSG_EA(r_0, 32));
        xsg_vu0_blocks++;
    }
#endif
#ifndef PORT_HOST
    PS2_ASMV("sqc2 vf2, 16(%0)" : : "r"(out) : "memory");
#else
    {
        XsgGpr r_0 = XSG_IN(out);
        vu0_sqc2(2, XSG_EA(r_0, 16));
        xsg_vu0_blocks++;
    }
#endif
#ifndef PORT_HOST
    PS2_ASMV("sqc2 vf3, 0(%0)" : : "r"(out) : "memory");
#else
    {
        XsgGpr r_0 = XSG_IN(out);
        vu0_sqc2(3, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif

    directionX = direction->x;
    if (directionX == 0.0f && direction->z == 0.0f) {
        MMathRotateMatrixX(out, (const Matrix4 *) out, srsAtan2(direction->y, 0.0f));
    } else {
        MMathRotateMatrixY(out, (const Matrix4 *) out, srsAtan2(directionX, direction->z));
        MMathRotateMatrixX(out, (const Matrix4 *) out, srsAtan2(direction->y, MMathCalcLengthXZ(direction)));
    }
    return out;
}

/*
 * MMathCalcVectorMatrix: loads the matrix row at offset 0x20 (the third row)
 * and writes VF00 - row to the output. VF00 is the VU0 hardwired constant
 * (0, 0, 0, 1), so this negates the row's x/y/z and produces w = 1 - row.w.
 */
Vector4 *MMathCalcVectorMatrix(Vector4 *out, const Matrix4 *matrix)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 $vf1,32(%1)\n\t"
        "vsub.xyzw $vf2xyzw,$vf0xyzw,$vf1xyzw\n\t"
        "sqc2 $vf2,0(%0)\n\t"
        "nop"
        : : "r"(out), "r"(matrix) : "memory");
#else
    {
        XsgGpr r_1 = XSG_IN(matrix);
        XsgGpr r_0 = XSG_IN(out);
        vu0_lqc2(1, XSG_EA(r_1, 32));
        vu0_arith(VU0_SUB, 15, 2, 0, 1, VU0_VEC);
        vu0_sqc2(2, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
    return out;
}

/*
 * MMathCalcAngleMatrix: recover the yaw/pitch pair of "matrix" from the
 * direction vector MMathCalcVectorMatrix reads out of its third row. out->z
 * and out->w come from the leading "sqc2 vf0" (VU0's hardwired (0,0,0,1))
 * and are never overwritten; out->x is yaw, out->y is pitch.
 */
Vector4 *MMathCalcAngleMatrix(Vector4 *out, const Matrix4 *matrix)
{
    Vector4 direction;

    MMathCalcVectorMatrix(&direction, matrix);
#ifndef PORT_HOST
    PS2_ASMV("sqc2 vf0, 0(%0)" : : "r"(out) : "memory");
#else
    {
        XsgGpr r_0 = XSG_IN(out);
        vu0_sqc2(0, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
    out->x = srsAtan2(direction.y, MMathCalcLengthXZ(&direction));
    out->y = srsAtan2(direction.x, direction.z);
    return out;
}

/*
 * MMathRotTransPers: transform "point" by "matrix", perspective-divide by
 * the resulting w, then scale/offset it into screen space with
 * camera->screenScale and camera->screenOffset (fields at 0x80/0x70,
 * StudioCamera) and store the fixed-point result at "destination".
 */
void MMathRotTransPers(Vector4 *destination, const StudioCamera *camera, const Matrix4 *matrix, const Vector4 *point)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1, 0(%0)\n\t"
        "lqc2 vf2, 0(%1)\n\t"
        "lqc2 vf3, 16(%1)\n\t"
        "lqc2 vf4, 32(%1)\n\t"
        "lqc2 vf5, 48(%1)\n\t"
        "vmulax.xyzw ACC, vf2, vf1x\n\t"
        "vmadday.xyzw ACC, vf3, vf1y\n\t"
        "vmaddaz.xyzw ACC, vf4, vf1z\n\t"
        "vmaddw.xyzw vf1, vf5, vf1w\n\t"
        "vdiv Q, vf0w, vf1w\n\t"
        "lqc2 vf2, 0(%2)\n\t"
        "lqc2 vf3, 0(%3)\n\t"
        "vsub.xyzw vf4, vf0, vf0\n\t"
        "vwaitq\n\t"
        "vmulq.xyz vf1, vf1, Q\n\t"
        "vmula.xyz ACC, vf1, vf2\n\t"
        "vmaddw.xyz vf4, vf3, vf0w\n\t"
        "vftoi4.xy vf1, vf4\n\t"
        "vftoi0.zw vf1, vf4\n\t"
        "sqc2 vf1, 0(%4)\n\t"
        "nop"
        : : "r"(point), "r"(matrix), "r"(&camera->screenScale), "r"(&camera->screenOffset), "r"(destination) : "memory");
#else
    {
        XsgGpr r_4 = XSG_IN(destination);
        XsgGpr r_3 = XSG_IN(&camera->screenOffset);
        XsgGpr r_2 = XSG_IN(&camera->screenScale);
        XsgGpr r_1 = XSG_IN(matrix);
        XsgGpr r_0 = XSG_IN(point);
        vu0_lqc2(1, XSG_EA(r_0, 0));
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_lqc2(3, XSG_EA(r_1, 16));
        vu0_lqc2(4, XSG_EA(r_1, 32));
        vu0_lqc2(5, XSG_EA(r_1, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 1, 0);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 3, 1, 1);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 4, 1, 2);
        vu0_arith(VU0_MADD, 15, 1, 5, 1, 3);
        vu0_div(0, 3, 1, 3);
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_lqc2(3, XSG_EA(r_3, 0));
        vu0_arith(VU0_SUB, 15, 4, 0, 0, VU0_VEC);
        vu0_waitq();
        vu0_arith(VU0_MUL, 14, 1, 1, 0, VU0_BQ);
        vu0_arith(VU0_MUL, 14, VU0_ACC, 1, 2, VU0_VEC);
        vu0_arith(VU0_MADD, 14, 4, 3, 0, 3);
        vu0_ftoi(4, 12, 1, 4);
        vu0_ftoi(0, 3, 1, 4);
        vu0_sqc2(1, XSG_EA(r_4, 0));
        xsg_vu0_blocks++;
    }
#endif
}

/*
 * MMathRotTransPersClip: MMathRotTransPers plus the VU0 near/far clip test.
 * The transform and the screen scale/offset are the same as MMathRotTransPers;
 * here the clip-flag register ($vi18) is cleared first, the perspective-
 * divided point is run through vclipw, and the resulting flag word is masked
 * to 0x8000 when any near/far bit is set and stored as the w component of the
 * fixed-point result at *destination (which is how the caller detects a
 * clipped point). There is no scalar code in the original at all, so the whole
 * body is one VU0 asm block, as in MMathRotTransPers.
 *
 * The two GPR operands of that block (the cfc2 flag word and the 0x8000 mask)
 * are `register` locals pinned to $2 and $9: the original object keeps the
 * clip word in $2 and materialises the mask in $9, and an unpinned local is
 * allocated to a different GPR ($4/$3 measured), which costs four words. The
 * pin is the only thing that reproduces the original's own choice.
 */
void MMathRotTransPersClip(Vector4 *destination, const StudioCamera *camera, const Matrix4 *matrix, const Vector4 *point)
{
    register int flag PS2_REG("$2");
    register int clipped PS2_REG("$9");

#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 vf1, 0(%2)\n\t"
        "lqc2 vf2, 0(%3)\n\t"
        "lqc2 vf3, 16(%3)\n\t"
        "lqc2 vf4, 32(%3)\n\t"
        "lqc2 vf5, 48(%3)\n\t"
        "vmulax.xyzw ACC, vf2, vf1x\n\t"
        "vmadday.xyzw ACC, vf3, vf1y\n\t"
        "vmaddaz.xyzw ACC, vf4, vf1z\n\t"
        "vmaddw.xyzw vf1, vf5, vf1w\n\t"
        "vdiv Q, vf0w, vf1w\n\t"
        "ctc2 $0, $vi18\n\t"
        "lqc2 vf2, 0(%4)\n\t"
        "lqc2 vf3, 0(%5)\n\t"
        "ori %1, $0, 0x8000\n\t"
        "vwaitq\n\t"
        "vmulq.xyz vf4, vf1, Q\n\t"
        "vclipw.xyz vf4xyz, vf0w\n\t"
        "vmula.xyz ACC, vf4, vf2\n\t"
        "vmaddw.xyz vf5, vf3, vf0w\n\t"
        "vftoi4.xy vf1, vf5\n\t"
        "vftoi0.z vf1, vf5\n\t"
        "cfc2 %0, $vi18\n\t"
        "nop\n\t"
        "movn %0, %1, %0\n\t"
        "qmtc2 %0, vf2\n\t"
        "vmr32.w vf1, vf2\n\t"
        "sqc2 vf1, 0(%6)\n\t"
        "nop"
        : "=r"(flag), "=&r"(clipped)
        : "r"(point), "r"(matrix), "r"(&camera->screenScale), "r"(&camera->screenOffset), "r"(destination)
        : "memory");
#else
    {
        XsgGpr r_6 = XSG_IN(destination);
        XsgGpr r_5 = XSG_IN(&camera->screenOffset);
        XsgGpr r_4 = XSG_IN(&camera->screenScale);
        XsgGpr r_3 = XSG_IN(matrix);
        XsgGpr r_2 = XSG_IN(point);
        vu0_lqc2(1, XSG_EA(r_2, 0));
        vu0_lqc2(2, XSG_EA(r_3, 0));
        vu0_lqc2(3, XSG_EA(r_3, 16));
        vu0_lqc2(4, XSG_EA(r_3, 32));
        vu0_lqc2(5, XSG_EA(r_3, 48));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 1, 0);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 3, 1, 1);
        vu0_arith(VU0_MADD, 15, VU0_ACC, 4, 1, 2);
        vu0_arith(VU0_MADD, 15, 1, 5, 1, 3);
        vu0_div(0, 3, 1, 3);
        vu0_ctc2(18, &xsg_ee_zero);
        vu0_lqc2(2, XSG_EA(r_4, 0));
        vu0_lqc2(3, XSG_EA(r_5, 0));
        xsg_ee_gpr[9].d[0] = xsg_ee_zero.d[0] | (unsigned long long)(unsigned short)(32768);
        vu0_waitq();
        vu0_arith(VU0_MUL, 14, 4, 1, 0, VU0_BQ);
        vu0_clip(4, 0);
        vu0_arith(VU0_MUL, 14, VU0_ACC, 4, 2, VU0_VEC);
        vu0_arith(VU0_MADD, 14, 5, 3, 0, 3);
        vu0_ftoi(4, 12, 1, 5);
        vu0_ftoi(0, 2, 1, 5);
        vu0_cfc2(&xsg_ee_gpr[2], 18);
        if (xsg_ee_gpr[2].d[0] != 0) xsg_ee_gpr[2].d[0] = xsg_ee_gpr[9].d[0];
        vu0_qmtc2(2, &xsg_ee_gpr[2]);
        vu0_mr32(1, 1, 2);
        vu0_sqc2(1, XSG_EA(r_6, 0));
        xsg_vu0_blocks++;
        XSG_OUT(flag, xsg_ee_gpr[2]);
        XSG_OUT(clipped, xsg_ee_gpr[9]);
    }
#endif
}

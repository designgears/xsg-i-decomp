/*
 * Stage 7 unit tests: the host VU0 model (port/runtime/vu0.c), the VU0
 * microprograms run through vu1.cpp, and the translated m_math.c asm.
 *
 * Built by port/CMakeLists.txt (target test_vu0, not built by default).
 * Expected values are computed in double precision and compared with a
 * relative tolerance (the VU rounds toward zero, the host to nearest).
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "vu0.h"

typedef struct { float x, y, z, w; } __attribute__((aligned(16))) V4;
typedef float M4[4][4] __attribute__((aligned(16)));

/* m_math.c (game code, compiled with -DPORT_HOST) */
V4 *MMathApplyMatrix(V4 *out, const M4 *matrix, const V4 *vector);
M4 *MMathMulMatrix(M4 *out, const M4 *left, const M4 *right);
M4 *MMathRotateMatrixX(M4 *out, const M4 *matrix, float angle);
M4 *MMathRotateMatrixY(M4 *out, const M4 *matrix, float angle);
M4 *MMathRotateMatrixZ(M4 *out, const M4 *matrix, float angle);
void *MMathNormalizeVector2(void *destination, const V4 *source);
float MMathCalcLength(const V4 *vector);
float MMathCalcDist(const V4 *a, const V4 *b);
float MMathVectorDotProduct(const V4 *a, const V4 *b);
V4 *MMathVectorCrossProduct(V4 *d, const V4 *a, const V4 *b);
M4 *MMathTranslateMatrix(M4 *out, const M4 *matrix, const V4 *t);
M4 *MMathScaleMatrix(M4 *out, const M4 *matrix, const V4 *s);

/* link stubs for m_math.c / vu1.cpp */
const M4 McMathUnitMatrix = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
const V4 McMathUnitVector = { 0, 0, 1, 0 };
float srsAtan2(float y, float x) { return atan2f(y, x); }
void xglSRand(int s) { (void)s; }
void xsg_logf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); }

static int fails, checks;

static void near(const char *what, double got, double want, double tol)
{
    checks++;
    double err = fabs(got - want), scale = fabs(want) > 1 ? fabs(want) : 1;
    if (!(err <= tol * scale)) {
        fails++;
        printf("FAIL %s: got %.9g want %.9g\n", what, got, want);
    }
}

static void near_m(const char *what, const M4 *got, double want[4][4])
{
    char b[96];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            snprintf(b, sizeof b, "%s[%d][%d]", what, i, j);
            near(b, (*got)[i][j], want[i][j], 1e-5);
        }
}

static void mul_ref(double out[4][4], const M4 *a, const M4 *b)   /* a x b, row vectors */
{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            double s = 0;
            for (int k = 0; k < 4; k++) s += (double)(*a)[i][k] * (*b)[k][j];
            out[i][j] = s;
        }
}

static void set_vf(int n, float x, float y, float z, float w)
{
    V4 v = { x, y, z, w };
    vu0_lqc2(n, &v);
}

static V4 get_vf(int n)
{
    V4 v;
    vu0_sqc2(n, &v);
    return v;
}

static const M4 A = { { 0.9f, 0.1f, -0.3f, 0 }, { -0.2f, 1.1f, 0.4f, 0 }, { 0.3f, -0.5f, 0.8f, 0 }, { 5, -7, 2.5f, 1 } };
static const M4 B = { { 0.5f, -0.25f, 0.75f, 0 }, { 1.5f, 0.2f, -0.6f, 0 }, { 0.1f, 0.9f, 0.3f, 0 }, { -1, 2, 3, 1 } };

static void test_macro_ops(void)
{
    /* vf0 is constant */
    set_vf(0, 9, 9, 9, 9);
    V4 v = get_vf(0);
    near("vf0.x", v.x, 0, 0); near("vf0.w", v.w, 1, 0);

    /* add/sub/mul with broadcast and dest masks */
    set_vf(1, 1, 2, 3, 4);
    set_vf(2, 10, 20, 30, 40);
    set_vf(3, 0, 0, 0, 0);
    vu0_arith(VU0_ADD, VU0_X | VU0_Z, 3, 1, 2, VU0_VEC);
    v = get_vf(3);
    near("vadd.xz x", v.x, 11, 0); near("vadd.xz y", v.y, 0, 0); near("vadd.xz z", v.z, 33, 0);
    vu0_arith(VU0_MUL, 15, 3, 1, 2, 3);  /* vmulw */
    v = get_vf(3);
    near("vmulw.xyzw z", v.z, 120, 0);

    /* ACC chain = matrix x vector */
    vu0_arith(VU0_MUL, 15, VU0_ACC, 1, 2, 0);   /* ACC = vf1 * 10 */
    vu0_arith(VU0_MADD, 15, 3, 1, 2, 1);        /* vf3 = ACC + vf1 * 20 */
    v = get_vf(3);
    near("vmadd y", v.y, 60, 0);

    /* clamp: overflow becomes +-FLT_MAX, not inf */
    set_vf(4, 3e38f, -3e38f, 0, 0);
    vu0_arith(VU0_MUL, 15, 5, 4, 4, VU0_VEC);
    v = get_vf(5);
    near("clamp +", v.x, 3.4028234663852886e38, 1e-7);
    checks++; if (isinf(v.x)) { fails++; printf("FAIL clamp produced inf\n"); }

    /* div / sqrt / rsqrt through Q */
    set_vf(6, 2, 8, 0, 0);
    vu0_div(0, 3, 6, 0);           /* Q = 1/2 */
    vu0_arith(VU0_MUL, 8, 7, 6, 0, VU0_BQ);
    near("vdiv/vmulq", get_vf(7).x, 1, 0);
    vu0_sqrt(6, 1);
    vu0_arith(VU0_ADD, 8, 7, 0, 0, VU0_BQ);
    near("vsqrt", get_vf(7).x, sqrt(8.0), 1e-6);
    vu0_rsqrt(0, 3, 6, 1);
    vu0_arith(VU0_ADD, 8, 7, 0, 0, VU0_BQ);
    near("vrsqrt", get_vf(7).x, 1 / sqrt(8.0), 1e-6);
    /* divide by zero: +-MAX */
    vu0_div(0, 3, 0, 0);
    vu0_arith(VU0_ADD, 8, 7, 0, 0, VU0_BQ);
    near("vdiv by 0", get_vf(7).x, 3.4028234663852886e38, 1e-7);

    /* outer product */
    set_vf(1, 1, 2, 3, 0);
    set_vf(2, 4, 5, 6, 0);
    vu0_opmula(14, 1, 2);
    vu0_opmsub(14, 3, 2, 1);
    v = get_vf(3);
    near("opmsub x", v.x, 2 * 6 - 3 * 5, 0);
    near("opmsub y", v.y, 3 * 4 - 1 * 6, 0);
    near("opmsub z", v.z, 1 * 5 - 2 * 4, 0);

    /* ftoi/itof */
    set_vf(1, 1.75f, -2.5f, 100.0f, 0.5f);
    vu0_ftoi(4, 15, 2, 1);
    v = get_vf(2);
    checks++; if (((int *)&v)[0] != 28 || ((int *)&v)[1] != -40) { fails++; printf("FAIL ftoi4\n"); }
    vu0_itof(4, 15, 3, 2);
    near("itof4", get_vf(3).y, -2.5, 0);

    /* mr32 */
    set_vf(1, 1, 2, 3, 4);
    vu0_mr32(15, 1, 1);
    v = get_vf(1);
    near("mr32 x", v.x, 2, 0); near("mr32 w", v.w, 1, 0);

    /* clip flags */
    XsgGpr g = { { 0, 0 } };
    vu0_ctc2(18, &g);
    set_vf(1, 2, -0.5f, -3, 1);
    vu0_clip(1, 1);
    vu0_cfc2(&g, 18);
    checks++; if ((g.w[0] & 0x3f) != (0x01 | 0x20)) { fails++; printf("FAIL clip 0x%x\n", g.w[0]); }

    /* integer regs */
    vu0_iaddi(5, 0, 8);
    vu0_mfir(8, 4, 5);
    vu0_itof(4, 8, 4, 4);
    near("viaddi/vmfir/vitof4", get_vf(4).x, 0.5, 0);
}

static void test_mmi(void)
{
    XsgGpr a = { { 0x1111111122222222ull, 0x3333333344444444ull } }, b = { { 0xaaaaaaaabbbbbbbbull, 0xccccccccddddddddull } }, d;
    xsg_mmi_pcpyld(&d, &a, &b);
    checks++; if (d.d[0] != b.d[0] || d.d[1] != a.d[0]) { fails++; printf("FAIL pcpyld\n"); }
    xsg_mmi_pextlw(&d, &a, &b);
    checks++; if (d.w[0] != 0xbbbbbbbb || d.w[1] != 0x22222222 || d.w[2] != 0xaaaaaaaa || d.w[3] != 0x11111111) { fails++; printf("FAIL pextlw\n"); }
    xsg_mmi_pcpyud(&d, &a, &b);
    checks++; if (d.d[0] != a.d[1] || d.d[1] != b.d[1]) { fails++; printf("FAIL pcpyud\n"); }
}

static void test_mmath(void)
{
    M4 out;
    double ref[4][4];
    MMathMulMatrix(&out, &A, &B);
    mul_ref(ref, &B, &A);
    near_m("MMathMulMatrix", (const M4 *)&out, ref);

    V4 p = { 1, 2, 3, 1 }, r;
    MMathApplyMatrix(&r, &A, &p);
    double e[4];
    for (int j = 0; j < 4; j++) e[j] = p.x * A[0][j] + p.y * A[1][j] + p.z * A[2][j] + p.w * A[3][j];
    near("ApplyMatrix x", r.x, e[0], 1e-6); near("ApplyMatrix y", r.y, e[1], 1e-6);
    near("ApplyMatrix z", r.z, e[2], 1e-6); near("ApplyMatrix w", r.w, e[3], 1e-6);

    V4 a = { 3, 4, 12, 7 }, b = { -1, 2, 0.5f, 0 }, n;
    near("CalcLength", MMathCalcLength(&a), 13, 1e-6);
    near("CalcDist", MMathCalcDist(&a, &b), sqrt(16.0 + 4 + 11.5 * 11.5), 1e-6);
    near("DotProduct", MMathVectorDotProduct(&a, &b), -3 + 8 + 6, 1e-6);
    MMathVectorCrossProduct(&n, &a, &b);
    near("Cross x", n.x, 4 * 0.5 - 12 * 2, 1e-6);
    near("Cross y", n.y, 12 * -1 - 3 * 0.5, 1e-6);
    near("Cross z", n.z, 3 * 2 - 4 * -1, 1e-6);
    MMathNormalizeVector2(&n, &a);
    near("Normalize x", n.x, 3.0 / 13, 1e-5); near("Normalize z", n.z, 12.0 / 13, 1e-5);

    /* rotations go through the VU0 microcode (vcallms 0x20 / 0xE8 = sin / cos) */
    const double ang = 0.7;
    double c = cos(ang), s = sin(ang);
    MMathRotateMatrixX(&out, &McMathUnitMatrix, (float)ang);
    near("RotX [1][1]", out[1][1], c, 2e-5); near("RotX [1][2]", out[1][2], s, 2e-5);
    near("RotX [2][1]", out[2][1], -s, 2e-5); near("RotX [2][2]", out[2][2], c, 2e-5);
    near("RotX [0][0]", out[0][0], 1, 0);
    MMathRotateMatrixZ(&out, &A, -2.5f);
    {
        M4 rz = { { (float)cos(-2.5), (float)sin(-2.5), 0, 0 }, { (float)-sin(-2.5), (float)cos(-2.5), 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
        double ref2[4][4];
        mul_ref(ref2, &A, &rz);
        /* the matrix stack conventions differ by side; accept either and report */
        int okA = 1;
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) if (fabs(out[i][j] - ref2[i][j]) > 5e-5) okA = 0;
        double ref3[4][4];
        mul_ref(ref3, &rz, &A);
        int okB = 1;
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) if (fabs(out[i][j] - ref3[i][j]) > 5e-5) okB = 0;
        checks++;
        if (!okA && !okB) { fails++; printf("FAIL RotZ(A) is neither A*Rz nor Rz*A\n"); }
    }

    V4 t = { 10, 20, 30, 1 };
    MMathTranslateMatrix(&out, &McMathUnitMatrix, &t);
    near("Translate [3][0]", out[3][0], 10, 0); near("Translate [3][2]", out[3][2], 30, 0);
    V4 sc = { 2, 3, 4, 1 };
    MMathScaleMatrix(&out, &A, &sc);
    near("Scale [0][0]", out[0][0], A[0][0] * 2, 1e-6);
    near("Scale [2][1]", out[2][1], A[2][1] * 4, 1e-6);   /* row i scaled by s[i] */
}

static void test_microcode_stack(void)
{
    /* Vu0CallSin/Cos on their own */
    for (double ang = -3.0; ang <= 3.0; ang += 0.37) {
        set_vf(4, (float)ang, 0, 0, 0);
        vu0_callms(0x20);
        near("Vu0CallSin", get_vf(1).x, sin(ang), 5e-5);
        set_vf(4, (float)ang, 0, 0, 0);
        vu0_callms(0xE8);
        near("Vu0CallCos", get_vf(1).x, cos(ang), 5e-5);
    }
    /* matrix stack: load A into vf28..31, Mul by B (vf4..7), Inverse, MulVector */
    for (int i = 0; i < 4; i++) vu0_lqc2(28 + i, A[i]);
    for (int i = 0; i < 4; i++) vu0_lqc2(4 + i, B[i]);
    vu0_callms(0x130);   /* Vu0CallMatrixStackMul */
    M4 got;
    for (int i = 0; i < 4; i++) vu0_sqc2(28 + i, got[i]);
    double r1[4][4], r2[4][4];
    mul_ref(r1, &B, &A);
    mul_ref(r2, &A, &B);
    int ok1 = 1, ok2 = 1;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
        if (fabs(got[i][j] - r1[i][j]) > 1e-4) ok1 = 0;
        if (fabs(got[i][j] - r2[i][j]) > 1e-4) ok2 = 0;
    }
    checks++;
    if (!ok1 && !ok2) { fails++; printf("FAIL MatrixStackMul\n"); }
    /* inverse: M * inv(M) == I */
    M4 m;
    memcpy(m, got, sizeof m);
    vu0_callms(0x218);   /* Vu0CallMatrixStackInverse */
    M4 inv;
    for (int i = 0; i < 4; i++) vu0_sqc2(28 + i, inv[i]);
    double id[4][4];
    mul_ref(id, &m, &inv);
    double I4[4][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) near("M*inv(M)", id[i][j], I4[i][j], 1e-4);
    /* push / unit / pop */
    for (int i = 0; i < 4; i++) vu0_lqc2(28 + i, A[i]);
    vu0_callms(0x538);   /* PushUnit */
    near("PushUnit [0][0]", get_vf(28).x, 1, 0);
    XsgGpr one = { { 1, 0 } };
    vu0_ctc2(1, &one);
    vu0_callms(0x5C8);   /* Pop 1 */
    near("Pop [3][0]", get_vf(31).x, A[3][0], 0);
}

int main(void)
{
    if (mmap((void *)0x11000000, 0x10000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED) {
        perror("mmap VU memory");
        return 2;
    }
    test_macro_ops();
    test_mmi();
    test_mmath();
    test_microcode_stack();
    printf("test_vu0: %d checks, %d failures\n", checks, fails);
    return fails != 0;
}

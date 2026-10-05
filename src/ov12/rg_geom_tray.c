/*
 * OV12 original TU 56: 0x00a2dd58..0x00a2e698 (8 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_geom_tray.h"
#include "ov12/xrg_rand_int.h"

#define RgGeomLocalMatricesAt(geom) \
    ((RgGeomLocalMatrices *)((unsigned char *)(geom) + 0x20))
#define RgGeomLocalMatricesConstAt(geom) \
    ((const RgGeomLocalMatrices *)((const unsigned char *)(geom) + 0x20))

/*
 * assert_prog: shared assertion helper, still assembly; this TU's own
 * RgGeomTraySetSize below declares it the same way.
 */
extern void assert_prog(const char *expression, const char *source_file,
                        int line);

/*
 * RgGeomInit / RgGeomSetType: original functions of the base geometry TU
 * (rg_geom.c, ov12/tu051), already recovered as C. Declared TU-locally, as
 * src/ov12/rg_geom_pillar.c also does, until that TU's own header is
 * published.
 */
extern void RgGeomInit(RgGeom *pGeom);
extern void RgGeomSetType(RgGeom *pGeom, int type);

/*
 * XrgUnitMatrix: original function of the polygon-geometry TU
 * (rg_geom_poly.c, ov12/tu055), still assembly there. Declared TU-locally,
 * as that TU also does, until it is recovered.
 */
extern void XrgUnitMatrix(RgMatrix destination);

/*
 * The type tag _InitRgGeomTray passes to RgGeomSetType; src/ov12/rg_geom_pillar.c's
 * RG_GEOM_TYPE_PILLAR (5) is the next value of the same sequence.
 */
#define RG_GEOM_TYPE_TRAY 4

extern void XrgSubVectorXYZ(RgVector destination, RgVector first,
                            RgVector second);
extern void XrgSetVectorXYZ(RgVector destination, float x, float y, float z);

static void _InitRgGeomTray(RgGeom *geom)
{
    RgGeomTray *tray = (RgGeomTray *)geom;

    if (geom == 0) {
        assert_prog(D_00A55218, D_00A55228, 19);
    }
    RgGeomInit(geom);
    RgGeomSetType(geom, RG_GEOM_TYPE_TRAY);
    tray->width = 100.0f;
    tray->height = 100.0f;
    XrgUnitMatrix(tray->local);
    XrgUnitMatrix(tray->inverse_local);
}

void InitRgGeomTray(RgGeom *geom)
{
    _InitRgGeomTray(geom);
}

extern RgHeap *InstanceOfRgHeap(void);
extern void *RgHeapAlloc(void *heap, unsigned int size,
                         const char *source_file, int line);

RgGeom *CreateRgGeomTray(void)
{
    RgGeomTray *tray;

    tray = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgGeomTray), D_00A55228, 37);
    _InitRgGeomTray((RgGeom *)tray);
    return (RgGeom *)tray;
}

extern void assert_prog(const char *expression, const char *source_file,
                        int line);

void RgGeomTraySetSize(void *tray, float width, float height)
{
    RgGeomTray *pTray = tray;

    if (pTray == 0) {
        assert_prog(D_00A55218, D_00A55228, 48);
    }
    if (width <= 0.0f) {
        width = 0.0f;
    }
    if (height <= 0.0f) {
        height = 0.0f;
    }
    pTray->width = width;
    pTray->height = height;
}

/*
 * The tray stores its local and inverse-local four-by-four matrices at byte
 * offsets 0x20 and 0x60.  The preceding geometry header is intentionally not
 * modelled here: these two routines only address the two evidenced matrix
 * objects and pass them to the established matrix helpers.
 */

void RgGeomTraySetLocal(void *tray, Matrix4 source)
{
    XrgCopyMatrix((float *)((char *)tray + 0x20), (const float *)source);
    XrgInvMatrix((float *)((char *)tray + 0x60), (const float *)source);
}

void RgGeomTrayGetLocal(void *tray, Matrix4 destination)
{
    XrgCopyMatrix((float *)destination, (const float *)((char *)tray + 0x20));
}

/*
 * Clip the old-to-current movement against the tray's two horizontal bounds
 * and its floor.  Each axis contributes at most one contact normal; when a
 * boundary crossing lies outside the movement segment, the endpoint is
 * clamped and the contact point is moved from the ball center to the surface.
 */
static int _CalcIntersect(RgVector clipped_position, RgVector last_contact,
                          RgVector start_position, RgVector end_position,
                          RgVector normals[3], float width, float height,
                          float radius)
{
    struct RgGeomTrayMovement {
        float x;
        float y;
        float z;
    } movement;
    RgVector current_position;
    float boundary;
    float left_boundary;
    float right_boundary;
    float time;
    unsigned int hit_axes;
    int hit_count;

    hit_axes = 0;
    hit_count = 0;
    __asm__ __volatile__(
        "lqc2 vf31, 0(%1)\n\t"
        "sqc2 vf31, 0(%0)\n\t"
        : : "r"(current_position), "r"(end_position) : "memory");
    __asm__ __volatile__(
        "lqc2 vf31, 0(%1)\n\t"
        "sqc2 vf31, 0(%0)\n\t"
        : : "r"(last_contact), "r"(current_position) : "memory");

    XrgSubVectorXYZ(&movement.x, current_position, start_position);
    if (movement.z < -0.001f) {
        boundary = radius - height;
        time = (boundary - start_position[2]) / movement.z;
        if (time >= 0.0f && time <= 1.0f) {
            current_position[2] = boundary;
            last_contact[0] = start_position[0] + time * movement.x;
            last_contact[1] = start_position[1] + time * movement.y;
            last_contact[2] = start_position[2] + time * movement.z;
            XrgSetVectorXYZ(normals[hit_count], 0.0f, 0.0f, 1.0f);
            hit_count++;
            hit_axes |= 1;
        }
    }
    if ((hit_axes & 1) == 0 && current_position[2] < radius - height) {
        current_position[2] = radius - height;
        __asm__ __volatile__(
            "lqc2 vf31, 0(%1)\n\t"
            "sqc2 vf31, 0(%0)\n\t"
            : : "r"(last_contact), "r"(current_position) : "memory");
        last_contact[2] -= radius;
        XrgSetVectorXYZ(normals[hit_count], 0.0f, 0.0f, 1.0f);
        hit_count++;
        hit_axes |= 1;
    }

    XrgSubVectorXYZ(&movement.x, current_position, start_position);
    if (movement.z > 0.001f) {
        boundary = height - radius;
        time = (boundary - start_position[2]) / movement.z;
        if (time >= 0.0f && time <= 1.0f) {
            current_position[2] = boundary;
            last_contact[0] = start_position[0] + time * movement.x;
            last_contact[1] = start_position[1] + time * movement.y;
            last_contact[2] = start_position[2] + time * movement.z;
            XrgSetVectorXYZ(normals[hit_count], 0.0f, 0.0f, -1.0f);
            hit_count++;
            hit_axes |= 2;
        }
    }
    if ((hit_axes & 2) == 0 && current_position[2] > height - radius) {
        current_position[2] = height - radius;
        __asm__ __volatile__(
            "lqc2 vf31, 0(%1)\n\t"
            "sqc2 vf31, 0(%0)\n\t"
            : : "r"(last_contact), "r"(current_position) : "memory");
        last_contact[2] += radius;
        XrgSetVectorXYZ(normals[hit_count], 0.0f, 0.0f, -1.0f);
        hit_count++;
        hit_axes |= 2;
    }

    XrgSubVectorXYZ(&movement.x, current_position, start_position);
    if (movement.x < -0.001f) {
        left_boundary = radius - width;
        time = (left_boundary - start_position[0]) / movement.x;
        if (time >= 0.0f && time <= 1.0f) {
            current_position[0] = left_boundary;
            last_contact[0] = start_position[0] + time * movement.x;
            last_contact[1] = start_position[1] + time * movement.y;
            last_contact[2] = start_position[2] + time * movement.z;
            XrgSetVectorXYZ(normals[hit_count], 1.0f, 0.0f, 0.0f);
            hit_count++;
            hit_axes |= 4;
        }
    }
    if ((hit_axes & 4) == 0 && current_position[0] < radius - width) {
        current_position[0] = radius - width;
        __asm__ __volatile__(
            "lqc2 vf31, 0(%1)\n\t"
            "sqc2 vf31, 0(%0)\n\t"
            : : "r"(last_contact), "r"(current_position) : "memory");
        last_contact[0] -= radius;
        XrgSetVectorXYZ(normals[hit_count], 1.0f, 0.0f, 0.0f);
        hit_count++;
        hit_axes |= 4;
    }

    XrgSubVectorXYZ(&movement.x, current_position, start_position);
    if (movement.x > 0.001f) {
        right_boundary = width - radius;
        time = (right_boundary - start_position[0]) / movement.x;
        if (time >= 0.0f && time <= 1.0f) {
            current_position[0] = right_boundary;
            last_contact[0] = start_position[0] + time * movement.x;
            last_contact[1] = start_position[1] + time * movement.y;
            last_contact[2] = start_position[2] + time * movement.z;
            XrgSetVectorXYZ(normals[hit_count], -1.0f, 0.0f, 0.0f);
            hit_count++;
            hit_axes |= 8;
        }
    }
    if ((hit_axes & 8) == 0 && current_position[0] > width - radius) {
        current_position[0] = width - radius;
        __asm__ __volatile__(
            "lqc2 vf31, 0(%1)\n\t"
            "sqc2 vf31, 0(%0)\n\t"
            : : "r"(last_contact), "r"(current_position) : "memory");
        last_contact[0] += radius;
        XrgSetVectorXYZ(normals[hit_count], -1.0f, 0.0f, 0.0f);
        hit_count++;
        hit_axes |= 8;
    }

    if ((hit_axes & 0x10) == 0 && current_position[1] < radius) {
        current_position[1] = radius;
        __asm__ __volatile__(
            "lqc2 vf31, 0(%1)\n\t"
            "sqc2 vf31, 0(%0)\n\t"
            : : "r"(last_contact), "r"(current_position) : "memory");
        last_contact[1] = 0.0f;
        XrgSetVectorXYZ(normals[hit_count], 0.0f, 1.0f, 0.0f);
        hit_count++;
    }

    __asm__ __volatile__(
        "lqc2 vf31, 0(%1)\n\t"
        "sqc2 vf31, 0(%0)\n\t"
        : : "r"(clipped_position), "r"(current_position) : "memory");
    return hit_count;
}

extern const char D_00A55240[];
extern const char D_00A55250[];
static inline void RgGeomTrayApply(float *dst, float *mtx, float *src)
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

int RgGeomTrayCheckBall(void *pTray, float *pCheck, float *pResult)
{
    RgGeomTray *tray = pTray;
    RgVector clipped;
    RgVector contact;
    RgVector start;
    RgVector end;
    float *normals;
    unsigned int i;
    RgVector pad;

    if (pTray == 0) {
        assert_prog(D_00A55218, D_00A55228, 0xE2);
    }
    if (pCheck == 0) {
        assert_prog(D_00A55240, D_00A55228, 0xE3);
    }
    if (pResult == 0) {
        assert_prog(D_00A55250, D_00A55228, 0xE4);
    }
    __asm__ __volatile__(
        "lqc2 vf31, 0(%1)\n\t"
        "sqc2 vf31, 0(%0)\n\t"
        : : "r"(start), "r"(pCheck + 12) : "memory");
    __asm__ __volatile__(
        "lqc2 vf31, 0(%1)\n\t"
        "sqc2 vf31, 0(%0)\n\t"
        : : "r"(end), "r"(pCheck + 8) : "memory");
    RgGeomTrayApply(start, (float *)tray->inverse_local, start);
    RgGeomTrayApply(end, (float *)tray->inverse_local, end);
    normals = pResult + 8;
    ((int *)pResult)[20] = _CalcIntersect(clipped, contact, start, end,
                                          (RgVector *)normals, tray->width,
                                          tray->height, pCheck[28]);
    RgGeomTrayApply(pResult, (float *)tray->local, contact);
    RgGeomTrayApply(pResult + 4, (float *)tray->local, clipped);
    for (i = 0; i < ((unsigned int *)pResult)[20]; i++) {
        normals[i * 4 + 3] = 0.0f;
        RgGeomTrayApply(normals + i * 4, (float *)tray->local, normals + i * 4);
    }
    return ((int *)pResult)[20] != 0;
}

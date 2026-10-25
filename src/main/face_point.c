#include "common.h"
#include "shared.h"

extern int s_nIgnoreCulling;

typedef int s32;

/*
 * One registered culling volume: the three vectors xglCullingMapSet and
 * xglCullingMapCreate fill in from a 9-float box record, followed by the
 * derived state culling_matrix builds from them and setup_occlusion and
 * check_occlusion read back. position.w and scale.w are set to 1.0f by both
 * writers; rotation.w is never written.
 */
typedef struct CullingVolume {
    Vector4 position;
    Vector4 rotation;
    Vector4 scale;
    u8 unmodeled_030[304];
} CullingVolume;

/*
 * s_inCulling is the culling map's own state: an active/ready byte at
 * offset 0, the ten registered volumes at offset 32 (the 352-byte stride
 * xglCullingCheck, xglCullingCheckSeparate, xglCullingCheckSeparateInit,
 * xglCullingMapSet and xglCullingMapCreate all index), an evidenced count
 * at 0xDE0 (compared against 10 and incremented once per registered
 * volume) and an evidenced source value at 0xDE4 (set from a caller
 * argument by xglCullingMapSet and read back by xglCullingMapLastCheck).
 */
typedef struct CullingMap {
    u8 active;
    u8 unmodeled_001[31];
    CullingVolume volumes[10];
    s32 count;
    s32 source;
    s32 model;
} CullingMap;

extern CullingMap s_inCulling;

typedef struct AVec4 { float v[4]; } __attribute__((aligned(16))) AVec4;
extern void xglMatrixStackUnit(void);
extern void xglMatrixStackTrans(const float translation[4]);
extern void xglMatrixStackRotX(float angle);
extern void xglMatrixStackRotY(float angle);
extern void xglMatrixStackRotZ(float angle);
extern void xglMatrixStackScale(const float scale[4]);
extern void xglMatrixStackSave(float matrix[4][4]);
extern void xglMatrixStackInverse(void);
extern void xglVectorNormal(Vector4 *destination, const Vector4 *source);
extern const AVec4 D_004D4A10;
extern const AVec4 D_004D4A20;
extern const AVec4 D_004D4A30;
extern const AVec4 D_004D4A40;
extern const AVec4 D_004D4A50;
typedef struct CullingVolumeView {
    float position[4];
    float rotation[4];
    float scale[4];
    Vector4 normal;
    float matrix[4][4];
    float inverse[4][4];
    Vector4 corners[4];
} CullingVolumeView;
static inline void apply_point(void *destination, float (*matrix)[4], const void *vector)
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
        :
        : "r"(vector), "r"(matrix), "r"(destination)
        : "memory"
    );
}

static void culling_matrix(CullingVolumeView *volume)
{
    AVec4 axis;
    AVec4 points[4];
    float (*matrix)[4] = volume->matrix;
    Vector4 *dst;

    xglMatrixStackUnit();
    xglMatrixStackTrans(volume->position);
    xglMatrixStackRotY(volume->rotation[1]);
    xglMatrixStackRotX(volume->rotation[0]);
    xglMatrixStackRotZ(volume->rotation[2]);
    xglMatrixStackScale(volume->scale);
    xglMatrixStackSave(matrix);
    xglMatrixStackInverse();
    xglMatrixStackSave(volume->inverse);
    axis = D_004D4A10;
    dst = &volume->normal;
    __asm__ __volatile__(
        "lqc2 vf31, 0(%0)\n\t"
        "lqc2 vf27, 0(%1)\n\t"
        "lqc2 vf28, 16(%1)\n\t"
        "lqc2 vf29, 32(%1)\n\t"
        "lqc2 vf30, 48(%1)\n\t"
        "vmulax.xyz ACC, vf27, vf31x\n\t"
        "vmadday.xyz ACC, vf28, vf31y\n\t"
        "vmaddz.xyz vf31, vf29, vf31z\n\t"
        "sqc2 vf31, 0(%2)\n\t"
        :
        : "r"(&axis), "r"(matrix), "r"(dst)
        : "memory"
    );
    xglVectorNormal(dst, dst);
    points[0] = D_004D4A20;
    points[1] = D_004D4A30;
    points[2] = D_004D4A40;
    points[3] = D_004D4A50;
    apply_point(&volume->corners[0], matrix, &points[0]);
    apply_point(&volume->corners[1], matrix, &points[1]);
    apply_point(&volume->corners[2], matrix, &points[2]);
    apply_point(&volume->corners[3], matrix, &points[3]);
}

/* Empty in this build: the original body is a bare return. */
static void culling_cell_disp(void)
{
}

/*
 * Line-plane intersection. lineStart/lineEnd are the two points of the
 * directed line; normal.xyz is the plane normal and normal.w scales
 * lineStart.w to offset planePoint along the normal before use. The
 * intersection point is destination = lineStart + t * (lineEnd - lineStart),
 * with t = (dot(normal, C) - dot(normal, lineStart)) / dot(normal, lineEnd -
 * lineStart) and C = planePoint + normal.xyz * (normal.w * lineStart.w).
 * Only destination's xyz lanes are computed by the vsub.xyz that produces
 * vf15; destination->w keeps whatever is in vf15's w lane going into that
 * sqc2, unrelated to lineStart/lineEnd/planePoint's own w.
 */
void _FacePoint(Vector4 *destination, const Vector4 *lineStart, const Vector4 *lineEnd,
                const Vector4 *normal, const Vector4 *planePoint) {
    __asm__ __volatile__(
        "lqc2 vf22, 0(%0)\n\t"
        "lqc2 vf20, 0(%1)\n\t"
        "lqc2 vf21, 0(%2)\n\t"
        "lqc2 vf23, 0(%3)\n\t"
        "vmulw.xyz vf14, vf22, vf22w\n\t"
        "vmulw.xyz vf14, vf14, vf20w\n\t"
        "vadd.xyz vf23, vf23, vf14\n\t"
        "vaddx.x vf10, vf0, vf22x\n\t"
        "vaddy.x vf11, vf0, vf22y\n\t"
        "vaddz.x vf12, vf0, vf22z\n\t"
        "vsub.xyz vf19, vf21, vf20\n\t"
        "vmulax.x ACC, vf10, vf21x\n\t"
        "vmadday.x ACC, vf11, vf21y\n\t"
        "vmaddz.x vf17, vf12, vf21z\n\t"
        "vmulax.x ACC, vf10, vf19x\n\t"
        "vmadday.x ACC, vf11, vf19y\n\t"
        "vmaddz.x vf18, vf12, vf19z\n\t"
        "vmulax.x ACC, vf10, vf23x\n\t"
        "vmadday.x ACC, vf11, vf23y\n\t"
        "vmaddz.x vf16, vf12, vf23z\n\t"
        "vdiv Q, vf0w, vf18x\n\t"
        "vsub.x vf16, vf17, vf16\n\t"
        "vwaitq\n\t"
        "vmulq.x vf16, vf16, Q\n\t"
        "vmulx.xyz vf15, vf19, vf16x\n\t"
        "vsub.xyz vf15, vf21, vf15\n\t"
        "sqc2 vf15, 0(%4)\n\t"
        "nop\n\t"
        :
        : "r"(normal), "r"(lineStart), "r"(lineEnd), "r"(planePoint), "r"(destination)
        : "memory"
    );
}

const char D_004D4598[16] = "MC_UTK05";
const char D_004D45A8[16] = "MC_UTK04";
const char D_004D45B8[16] = "MC_KAS31";
const char D_004D45C8[16] = "MC_KAS04";
const char D_004D45D8[16] = "MC_KAS12";
const char D_004D45E8[16] = "MC_KAS13";
const char D_004D45F8[16] = "MC_KAS18";
const char D_004D4608[16] = "MC_KAS19";
const char D_004D4618[16] = "MC_KAS20";
const char D_004D4628[16] = "MC_UTA03";
const char D_004D4638[16] = "MC_UTA04";
const char D_004D4648[16] = "MC_UTA05";
const char D_004D4658[16] = "MC_UTA06";
const char D_004D4668[16] = "MC_UTA07";
const char D_004D4678[16] = "MC_UTA08";
const char D_004D4688[16] = "MC_UTA13";
const char D_004D4698[16] = "MC_UTA17";
const char D_004D46A8[16] = "MC_GNK04";
const char D_004D46B8[16] = "MC_GNK09";
const char D_004D46C8[16] = "MC_GNK10";
const char D_004D46D8[16] = "MC_GNK11";
const char D_004D46E8[16] = "MC_GNK12";
const char D_004D46F8[16] = "MC_GNK13";
const char D_004D4708[16] = "MC_GNK14";
const char D_004D4718[16] = "MC_GNK15";
const char D_004D4728[16] = "MC_GNK18";
const char D_004D4738[16] = "MC_GNK20";
const char D_004D4748[16] = "MC_GNK21";
const char D_004D4758[16] = "MC_GNK02";
const char D_004D4768[16] = "MC_VOK13";
const char D_004D4778[16] = "MC_VOK13B";
const char D_004D4788[16] = "MC_GNU01";
const char D_004D4798[16] = "MC_GNU02";
const char D_004D47A8[16] = "MC_GNU03";
const char D_004D47B8[16] = "MC_GNU04";
const char D_004D47C8[16] = "MC_GNU05";
const char D_004D47D8[16] = "MC_GNU06";
const char D_004D47E8[16] = "MC_KUK16";
const char D_004D47F8[16] = "MC_KUK12";
const char D_004D4808[16] = "MC_KUK11";
const char D_004D4818[16] = "MC_KUK02";
const char D_004D4828[16] = "Mc_dyu04";
const char D_004D4838[16] = "MC_DYU07";
const char D_004D4848[16] = "MC_DYU10";
const char D_004D4858[16] = "MC_DYU12";
const char D_004D4868[16] = "MC_DYU13";
const char D_004D4878[16] = "MC_DYU15";
const char D_004D4888[16] = "MC_DYU16";
const char D_004D4898[16] = "MC_KOU05";
const char D_004D48A8[16] = "MC_KOU04";
const char D_004D48B8[16] = "MC_KOU01";
const char D_004D48C8[16] = "MC_DYU01";
const char D_004D48D8[16] = "MC_VOK24";
const char D_004D48E8[16] = "MC_ELS01";
const char D_004D48F8[16] = "MC_ELS04B";
const char D_004D4908[16] = "MC_ELS04";
const char D_004D4918[16] = "MC_ELS03";
const char D_004D4928[16] = "MC_ELS02B";
const char D_004D4938[16] = "mc_els02";
const char D_004D4948[16] = "MC_ELS09";
const char D_004D4958[16] = "MC_ELS10";
const char D_004D4968[16] = "MC_PRO02";
const char D_004D4978[16] = "MC_VOK12b";
const char D_004D4988[16] = "mc_vok12";
const char D_004D4998[16] = "MC_VOK11";
const char D_004D49A8[16] = "MC_VOK10";
const char D_004D49B8[16] = "MC_VOK07";
const char D_004D49C8[16] = "mc_vok04";
const char D_004D49D8[16] = "MC_VOK03";
extern void xglVectorScaleXYZ(float scale, Vector4 *destination,
                              const Vector4 *source);
typedef struct CullingMapNameSource {
    u8 unmodeled_000[0x10];
    char name_with_prefix[32];
} CullingMapNameSource;
typedef struct CullingCameraView {
    u8 unmodeled_000[0x1B0];
    Matrix4 occlusionTransform;
} CullingCameraView;

float _CheckLine(const Vector4 *offsetStart, const Vector4 *offsetEnd,
                 const Vector4 *endpointStart, const Vector4 *endpointEnd,
                 const Vector4 *planePoint, const Vector4 *planeNormal)
{
    Vector4 adjustedStart;
    Vector4 adjustedEnd;
    Vector4 endpointDelta;
    Vector4 pointFromStart;
    Vector4 normalOffset;

    xglVectorScaleXYZ(planeNormal->w * planePoint->w * 0.5f,
                      &normalOffset, planeNormal);
    xglVectorScaleXYZ(planePoint->w * 0.5f, &adjustedStart, offsetStart);
    xglVectorScaleXYZ(planePoint->w * 0.5f, &adjustedEnd, offsetEnd);

    __asm__ __volatile__(
        "lqc2 vf3, 0(%0)\n\t"
        "lqc2 vf2, 0(%1)\n\t"
        "vsub.xyz vf2, vf2, vf3\n\t"
        "sqc2 vf2, 0(%0)\n\t"
        :
        : "r"(&adjustedStart), "r"(endpointStart)
        : "memory");

    __asm__ __volatile__(
        "lqc2 vf3, 0(%0)\n\t"
        "lqc2 vf2, 0(%1)\n\t"
        "vsub.xyz vf2, vf2, vf3\n\t"
        "sqc2 vf2, 0(%0)\n\t"
        :
        : "r"(&adjustedEnd), "r"(endpointEnd)
        : "memory");

    __asm__ __volatile__(
        "lqc2 vf3, 0(%1)\n\t"
        "lqc2 vf2, 0(%0)\n\t"
        "vadd.xyz vf2, vf2, vf3\n\t"
        "sqc2 vf2, 0(%0)\n\t"
        :
        : "r"(&adjustedStart), "r"(&normalOffset)
        : "memory");

    __asm__ __volatile__(
        "lqc2 vf3, 0(%1)\n\t"
        "lqc2 vf2, 0(%0)\n\t"
        "vadd.xyz vf2, vf2, vf3\n\t"
        "sqc2 vf2, 0(%0)\n\t"
        :
        : "r"(&adjustedEnd), "r"(&normalOffset)
        : "memory");

    __asm__ __volatile__(
        "lqc2 vf3, 0(%0)\n\t"
        "lqc2 vf2, 0(%1)\n\t"
        "vsub.xyz vf2, vf2, vf3\n\t"
        "sqc2 vf2, 0(%2)\n\t"
        :
        : "r"(&adjustedStart), "r"(&adjustedEnd),
          "r"(&endpointDelta)
        : "memory");

    __asm__ __volatile__(
        "lqc2 vf3, 0(%0)\n\t"
        "lqc2 vf2, 0(%1)\n\t"
        "vsub.xyz vf2, vf2, vf3\n\t"
        "sqc2 vf2, 0(%2)\n\t"
        :
        : "r"(&adjustedStart), "r"(planePoint),
          "r"(&pointFromStart)
        : "memory");

    return endpointDelta.x * pointFromStart.x +
           endpointDelta.y * pointFromStart.y +
           endpointDelta.z * pointFromStart.z;
}

extern void xglVectorOuter(Vector4 *destination, const Vector4 *left, const Vector4 *right);
extern void xglVectorNormal(Vector4 *destination, const Vector4 *source);

static void plane_from_points(const Vector4 *p0, const Vector4 *p1, const Vector4 *p2, Vector4 *plane)
{
    Vector4 edge2;
    Vector4 edge1;
    Vector4 normal;

    edge2.x = p1->x - p0->x;
    edge2.y = p1->y - p0->y;
    edge2.z = p1->z - p0->z;

    edge1.x = p2->x - p0->x;
    edge1.y = p2->y - p0->y;
    edge1.z = p2->z - p0->z;

    xglVectorOuter(&normal, &edge2, &edge1);
    xglVectorNormal(&normal, &normal);

    plane->x = normal.x;
    plane->y = normal.y;
    plane->z = normal.z;
    plane->w = -(normal.x * p0->x + normal.y * p0->y + normal.z * p0->z);
}

typedef unsigned int so_u128 __attribute__((mode(TI)));
typedef struct SetupOcclusionView {
    float position[4];
    float rotation[4];
    float scale[4];
    Vector4 normal;
    float matrix[4][4];
    float inverse[4][4];
    Vector4 corners[4];
    Vector4 planes[5];
    float depth;
} SetupOcclusionView;

void setup_occlusion(CullingVolume *volume, s32 camera)
{
    SetupOcclusionView *v = (SetupOcclusionView *)volume;
    Vector4 a;
    Vector4 b;
    Vector4 c;
    Vector4 d;
    union { Vector4 v; so_u128 q; } o;
    Vector4 *po;

    apply_point(&a, (void *)(camera + 0x1B0), &v->corners[0]);
    apply_point(&b, (void *)(camera + 0x1B0), &v->corners[1]);
    apply_point(&c, (void *)(camera + 0x1B0), &v->corners[2]);
    apply_point(&d, (void *)(camera + 0x1B0), &v->corners[3]);
    po = &o.v;
    a.z = -a.z;
    b.z = -b.z;
    c.z = -c.z;
    d.z = -d.z;
    v->depth = a.z;
    if (b.z < v->depth) v->depth = b.z;
    if (c.z < v->depth) v->depth = c.z;
    if (d.z < v->depth) v->depth = d.z;
    __asm__ volatile("sq $0, 0(%0)" : : "r"(po) : "memory");
    plane_from_points(&a, &b, &c, &v->planes[0]);
    if (v->planes[0].w > 0.0f) {
        plane_from_points(po, &a, &b, &v->planes[1]);
        plane_from_points(po, &b, &c, &v->planes[2]);
        plane_from_points(po, &c, &d, &v->planes[3]);
        plane_from_points(po, &d, &a, &v->planes[4]);
    } else {
        plane_from_points(&c, &b, &a, &v->planes[0]);
        plane_from_points(po, &b, &a, &v->planes[1]);
        plane_from_points(po, &c, &b, &v->planes[2]);
        plane_from_points(po, &d, &c, &v->planes[3]);
        plane_from_points(po, &a, &d, &v->planes[4]);
    }
}
/* sq0 */

typedef struct CullingVolumePlanes {
    Vector4 position;
    Vector4 rotation;
    Vector4 scale;
    Vector4 normal;
    float matrix[4][4];
    float inverse[4][4];
    Vector4 corners[4];
    float planes[5][4];
    float depth;
} CullingVolumePlanes;

s32 check_occlusion(CullingVolume *volume, s32 camera, s32 model)
{
    CullingVolumePlanes *v = (CullingVolumePlanes *)volume;
    Vector4 p;
    s32 result;

    apply_point(&p, (const void *)(camera + 0x1B0), (const void *)model);
    result = 0;
    p.z = -p.z;
    if (!((p.z - p.w) < v->depth)) {
        if (!(-p.w < p.x * v->planes[0][0] + p.y * v->planes[0][1] + p.z * v->planes[0][2] + v->planes[0][3]) &&
            !(-p.w < p.x * v->planes[1][0] + p.y * v->planes[1][1] + p.z * v->planes[1][2] + v->planes[1][3]) &&
            !(-p.w < p.x * v->planes[2][0] + p.y * v->planes[2][1] + p.z * v->planes[2][2] + v->planes[2][3]) &&
            !(-p.w < p.x * v->planes[3][0] + p.y * v->planes[3][1] + p.z * v->planes[3][2] + v->planes[3][3]) &&
            !(-p.w < p.x * v->planes[4][0] + p.y * v->planes[4][1] + p.z * v->planes[4][2] + v->planes[4][3])) {
            result = 1;
        }
    }
    return result;
}

void setup_occlusion(CullingVolume *volume, s32 camera);
s32 check_occlusion(CullingVolume *volume, s32 camera, s32 model);

s32 xglCullingCheck(s32 camera, s32 model)
{
  s32 r = 0;
  s32 i;
  if (s_nIgnoreCulling == 0)
  {
    i = 0;
    for (i = 0; i < s_inCulling.count; i++)
    {
      setup_occlusion(&s_inCulling.volumes[i], camera);
      r = check_occlusion(&s_inCulling.volumes[i], camera, model);
      if (r != 0)
      {
        break;
      }
    }

  }
  return r;
}

void setup_occlusion(CullingVolume *volume, s32 camera);

/*
 * Rebuilds the occlusion planes of every registered volume for camera, so
 * that the xglCullingCheckSeparate calls that follow only have to test
 * model against them. xglCullingCheck does both steps per volume instead.
 */
void xglCullingCheckSeparateInit(s32 camera)
{
    s32 i;

    for (i = 0; i < s_inCulling.count; i++) {
        setup_occlusion(&s_inCulling.volumes[i], camera);
    }
}

s32 check_occlusion(CullingVolume *volume, s32 camera, s32 model);

s32 xglCullingCheckSeparate(s32 camera, s32 model)
{
    s32 r = (u8)camera >> 8;
    s32 i = r;

    while (i < s_inCulling.count) {
        s32 t = i * 0xB;
        i++;
        r = check_occlusion((CullingVolume *)((t << 5) + (int)s_inCulling.volumes), camera, model);
        if (r != 0)
            break;
        t = 0;
        goto next;
next:;
    }
    return r;
}

/*
 * Declared as an incomplete array, not a scalar: cc1 -G8 would otherwise
 * classify a plain scalar extern as small data and emit a single
 * gp-relative load, but the original site is two absolute hi/lo
 * instructions; an incomplete-extent declaration keeps the target's
 * unknown size ineligible for that optimization.
 */
extern s32 D_00969760[];

s32 xglCullingExist(void) {
    return D_00969760[0];
}

extern void nmlModelSetMapLastEntry(int model_id, int entry);

void xglCullingMapLastCheck(void)
{
    s32 i;
    s32 e;

    if (s_inCulling.source == 0) {
        return;
    }
    i = 0;
loop:
    if (i < 12) {
        e = ((s32 *)s_inCulling.source)[i++];
        if (e >= 0) {
            nmlModelSetMapLastEntry(s_inCulling.model, e);
            goto loop;
        }
    }
}

void xglCullingMapInit(void)
{
    s_nIgnoreCulling = 0;
    s_inCulling.count = 0;
    s_inCulling.active = 0;
    s_inCulling.source = 0;
}

/*
 * One row of the named culling-map table: entryCount and source mirror
 * CullingMap's own count/source pair (source is copied verbatim into
 * CullingMap.source by xglCullingMapSet once a row is matched) and are
 * both zero for an unused row; name is compared against the sought name.
 */
typedef struct CullingMapEntry {
    const char *name;
    s32 entryCount;
    s32 source;
    u8 unmodeled_00C[4];
} CullingMapEntry;

extern CullingMapEntry s_aCullingMap[70];

int strcmp(const char *, const char *);

/*
 * Finds a named culling-map entry in the current map table and returns its
 * index or minus one. The table ends at the first row with entryCount and
 * source both zero.
 */
s32 check_culling_map(const char *name) {
    s32 result = -1;
    s32 i;

    for (i = 0; s_aCullingMap[i].entryCount != 0 || s_aCullingMap[i].source != 0; i++) {
        if (strcmp(s_aCullingMap[i].name, name) == 0) {
            result = i;
            break;
        }
    }
    return result;
}

unsigned int strlen(const char *);
char *strcpy(char *, const char *);

void xglCullingMapSet(s32 model)
{
    s32 idx;
    s32 off;
    float *rec;
    CullingVolume *v;

    s_inCulling.count = 0;
    s_inCulling.source = 0;
    if (strlen((char *)model + 0x10) < 0x20) {
        strcpy((char *)&s_inCulling, (char *)model + 0x11);
    }
    idx = check_culling_map((char *)&s_inCulling);
    if (idx < 0) {
        return;
    }
    s_inCulling.model = model;
    s_inCulling.source = s_aCullingMap[idx].source;
    if (s_aCullingMap[idx].entryCount == 0) {
        return;
    }
    for (off = 0; ; off++) {
        rec = (float *)s_aCullingMap[idx].entryCount + off * 9;
        v = &s_inCulling.volumes[s_inCulling.count];
        if (s_inCulling.count >= 10) {
            return;
        }
        if (rec[6] < 0) {
            return;
        }
        v->position.x = rec[0];
        v->position.y = rec[1];
        v->position.z = rec[2];
        v->position.w = 1.0f;
        v->rotation.x = rec[3];
        v->rotation.y = rec[4];
        v->rotation.z = rec[5];
        v->scale.x = rec[6];
        v->scale.y = rec[7];
        v->scale.z = rec[8];
        v->scale.w = 1.0f;
        culling_matrix((CullingVolumeView *)v);
        s_inCulling.count++;
    }
}

extern float s_aCullingBase[9];

void xglCullingMapCreate(void) {
    float *b = s_aCullingBase;
    CullingVolume *v = &s_inCulling.volumes[s_inCulling.count];
    if (s_inCulling.count < 10) {
        v->position.x = b[0];
        v->position.y = b[1];
        v->position.z = b[2];
        v->position.w = 1.0f;
        v->rotation.x = b[3];
        v->rotation.y = b[4];
        v->rotation.z = b[5];
        v->scale.x = b[6];
        v->scale.y = b[7];
        v->scale.z = b[8];
        v->scale.w = 1.0f;
        culling_matrix((CullingVolumeView *)v);
        s_inCulling.count++;
    }
}

void xglCullingMapDisp(void)
{
    CullingMap *m = &s_inCulling;
    s32 n = m->count;
    s32 i;
    u8 *p;

    if (n > 0) {
        p = (u8 *)m + 0x20;
        i = n;
        do {
            ((void (*)(void *))culling_cell_disp)(p);
            p += 0x160;
        } while (--i != 0);
    }
}

void xglCullingIgnore(void)
{
    s_nIgnoreCulling = 1;
}

void xglCullingIgnoreOff(void)
{
    s_nIgnoreCulling = 0;
}

/* Empty in this build: the original body is a bare return. */
void xglCullingMapDebug(void)
{
}

/*
 * _ModelCalcClipInit: load the pair of 4x4 clip matrices into VU0 macro-mode
 * registers for the callees that read them without reloading.
 *
 * Eight lqc2 loads at decimal offsets 0,16,...,112 off `clip` establish the
 * parameter as two consecutive 16-byte-aligned 4x4 matrices (128 bytes):
 * clip[0]'s four rows land in vf12..vf15 and clip[1]'s four rows land in
 * vf16..vf19, in address order.
 *
 * The registers persist in VU0 state for _ModelCalcClip, _ModelCalcClipMat1
 * and _ModelCalcClipMat2 -- the only three callees that read vf12..vf19
 * without reloading them. This function's own callers are 8 of the 10
 * nmlModelCalcClip* wrappers, which tail-call _ModelCalcClip right after
 * calling this function (nmlModelCalcClip, nmlModelCalcClipNoCulling,
 * nmlModelCalcClipMat1, nmlModelCalcClipMat2, nmlModelCalcClipCam,
 * nmlModelCalcClipStudio, nmlModelCalcClipMat1AllCam,
 * nmlModelCalcClipMat2AllCam); only nmlModelCalcClipMat1Cam and
 * nmlModelCalcClipMat2Cam instead tail-call _ModelCalcClipMat1 and
 * _ModelCalcClipMat2 respectively. All three consumers read vf16..vf19
 * before vf12..vf15 (_ModelCalcClipMat1 at 0x0023f154 then 0x0023f1c0;
 * _ModelCalcClipMat2 at 0x0023f01c then 0x0023f088; _ModelCalcClip at
 * 0x0023f260 then 0x0023f2cc): the bank order does not follow either
 * callee's own "Mat1"/"Mat2" name, so there is no mat1/mat2 register
 * nickname to derive from the consumers -- clip[0] and clip[1] are just
 * the two matrices in address order. nmlModelCalcClipMat1 corroborates the
 * 128-byte extent by passing camera+0x4F0 as this function's argument
 * (0x0023f468).
 *
 * Eight lqc2 loads, nothing else: no branch, no stack frame, no other memory
 * access. The final statement appends a bare nop after the eighth lqc2, in
 * the same asm block, as the delay-slot filler this route's pinned assembler
 * needs: cc1 leaves a leaf function's bare `j $31` delay slot to the
 * assembler, and in reorder mode the assembler fills it by moving the
 * previous instruction there unless a real one already occupies it -- and
 * among the instruction kinds this route emits, only lqc2/sqc2 get moved.
 * The original toolchain did not do this (74 of 79 comparable sites in
 * SLUS_204.69 keep `jr $31; nop`), so the nop restores the original extent
 * and instruction order
 */
static void _ModelCalcClipInit(const Matrix4 clip[2])
{
    __asm__ __volatile__("lqc2 vf12, 0(%0)"   :: "r"(clip) : "memory");
    __asm__ __volatile__("lqc2 vf13, 16(%0)"  :: "r"(clip) : "memory");
    __asm__ __volatile__("lqc2 vf14, 32(%0)"  :: "r"(clip) : "memory");
    __asm__ __volatile__("lqc2 vf15, 48(%0)"  :: "r"(clip) : "memory");
    __asm__ __volatile__("lqc2 vf16, 64(%0)"  :: "r"(clip) : "memory");
    __asm__ __volatile__("lqc2 vf17, 80(%0)"  :: "r"(clip) : "memory");
    __asm__ __volatile__("lqc2 vf18, 96(%0)"  :: "r"(clip) : "memory");
    __asm__ __volatile__("lqc2 vf19, 112(%0)\n\t"
                         "nop"                 :: "r"(clip) : "memory");
}

static s32 _ModelCalcClipMat2(s32 model, s32 matrix1, s32 matrix2)
{
    s32 clipped;

    __asm__ __volatile__(
        "lqc2 vf10,0(%1)\n\t"
        "lqc2 vf27,0(%2)\n\t"
        "lqc2 vf28,16(%2)\n\t"
        "lqc2 vf29,32(%2)\n\t"
        "lqc2 vf30,48(%2)\n\t"
        "vsubw.w vf27w,vf27w,vf27w\n\t"
        "vsubw.w vf28w,vf28w,vf28w\n\t"
        "vsubw.w vf29w,vf29w,vf29w\n\t"
        "lqc2 vf20,0(%3)\n\t"
        "lqc2 vf21,16(%3)\n\t"
        "lqc2 vf22,32(%3)\n\t"
        "lqc2 vf23,48(%3)\n\t"
        "vsubw.w vf20w,vf20w,vf20w\n\t"
        "vsubw.w vf21w,vf21w,vf21w\n\t"
        "vsubw.w vf22w,vf22w,vf22w\n\t"
        "vmulax.xyzw ACC,vf27xyzw,vf20x\n\t"
        "vmadday.xyzw ACC,vf28xyzw,vf20y\n\t"
        "vmaddaz.xyzw ACC,vf29xyzw,vf20z\n\t"
        "vmaddw.xyzw vf20xyzw,vf30xyzw,vf20w\n\t"
        "vmulax.xyzw ACC,vf27xyzw,vf21x\n\t"
        "vmadday.xyzw ACC,vf28xyzw,vf21y\n\t"
        "vmaddaz.xyzw ACC,vf29xyzw,vf21z\n\t"
        "vmaddw.xyzw vf21xyzw,vf30xyzw,vf21w\n\t"
        "vmulax.xyzw ACC,vf27xyzw,vf22x\n\t"
        "vmadday.xyzw ACC,vf28xyzw,vf22y\n\t"
        "vmaddaz.xyzw ACC,vf29xyzw,vf22z\n\t"
        "vmaddw.xyzw vf22xyzw,vf30xyzw,vf22w\n\t"
        "vmulax.xyzw ACC,vf27xyzw,vf23x\n\t"
        "vmove.xyzw vf27xyzw,vf20xyzw\n\t"
        "vmadday.xyzw ACC,vf28xyzw,vf23y\n\t"
        "vmove.xyzw vf28xyzw,vf21xyzw\n\t"
        "vmaddaz.xyzw ACC,vf29xyzw,vf23z\n\t"
        "vmove.xyzw vf29xyzw,vf22xyzw\n\t"
        "vmaddw.xyzw vf30xyzw,vf30xyzw,vf23w\n\t"
        "vmulax.xyzw ACC,vf27xyzw,vf10x\n\t"
        "vmadday.xyzw ACC,vf28xyzw,vf10y\n\t"
        "vmaddaz.xyzw ACC,vf29xyzw,vf10z\n\t"
        "vmaddw.xyzw vf20xyzw,vf30xyzw,vf0w\n\t"
        "vaddw.x vf2x,vf0x,vf10w\n\t"
        "qmfc2 $2,vf2\n\t"
        "mtc1 $2,$f1\n\t"
        "vmulax.xyzw ACC,vf16xyzw,vf20x\n\t"
        "vmadday.xyzw ACC,vf17xyzw,vf20y\n\t"
        "vmaddaz.xyzw ACC,vf18xyzw,vf20z\n\t"
        "vmaddw.xyzw vf9xyzw,vf19xyzw,vf0w\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmulax.xyzw ACC,vf12xyzw,vf20x\n\t"
        "vmadday.xyzw ACC,vf13xyzw,vf20y\n\t"
        "vmaddaz.xyzw ACC,vf14xyzw,vf20z\n\t"
        "vmaddw.xyzw vf9xyzw,vf15xyzw,vf0w\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "sub $2,$2,$2\n\t"
        "j 2f\n\t"
        "1:\n\t"
        "sub $2,$2,$2\n\t"
        "addi $2,$2,1\n\t"
        "2:\n\t"
        "move %0,$2"
        : "=r"(clipped)
        : "r"(model), "r"(matrix1), "r"(matrix2)
        : "$2", "$f0", "$f1", "memory");
    return clipped;
}

/* Handwritten function: original is assembly. */
static s32 _ModelCalcClipMat1(s32 model, s32 matrix)
{
    s32 clipped;

    __asm__ __volatile__(
        "lqc2 vf10,0(%1)\n\t"
        "lqc2 vf27,0(%2)\n\t"
        "lqc2 vf28,16(%2)\n\t"
        "lqc2 vf29,32(%2)\n\t"
        "lqc2 vf30,48(%2)\n\t"
        "vsubw.w vf27w,vf27w,vf27w\n\t"
        "vsubw.w vf28w,vf28w,vf28w\n\t"
        "vsubw.w vf29w,vf29w,vf29w\n\t"
        "vmulax.xyzw ACC,vf27xyzw,vf10x\n\t"
        "vmadday.xyzw ACC,vf28xyzw,vf10y\n\t"
        "vmaddaz.xyzw ACC,vf29xyzw,vf10z\n\t"
        "vmaddw.xyzw vf20xyzw,vf30xyzw,vf0w\n\t"
        "vaddw.x vf2x,vf0x,vf10w\n\t"
        "qmfc2 $2,vf2\n\t"
        "mtc1 $2,$f1\n\t"
        "vmulax.xyzw ACC,vf16xyzw,vf20x\n\t"
        "vmadday.xyzw ACC,vf17xyzw,vf20y\n\t"
        "vmaddaz.xyzw ACC,vf18xyzw,vf20z\n\t"
        "vmaddw.xyzw vf9xyzw,vf19xyzw,vf0w\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmulax.xyzw ACC,vf12xyzw,vf20x\n\t"
        "vmadday.xyzw ACC,vf13xyzw,vf20y\n\t"
        "vmaddaz.xyzw ACC,vf14xyzw,vf20z\n\t"
        "vmaddw.xyzw vf9xyzw,vf15xyzw,vf0w\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "sub $2,$2,$2\n\t"
        "j 2f\n\t"
        "1:\n\t"
        "sub $2,$2,$2\n\t"
        "addi $2,$2,1\n\t"
        "2:\n\t"
        "move %0,$2"
        : "=r"(clipped)
        : "r"(model), "r"(matrix)
        : "$2", "$f0", "$f1", "memory");
    return clipped;
}

/* Handwritten function: original is assembly. */
static s32 _ModelCalcClip(s32 model)
{
    s32 clipped;

    __asm__ __volatile__(
        "lqc2 vf10,0(%1)\n\t"
        "vaddw.x vf2x,vf0x,vf10w\n\t"
        "qmfc2 $2,vf2\n\t"
        "mtc1 $2,$f1\n\t"
        "vmulax.xyzw ACC,vf16xyzw,vf10x\n\t"
        "vmadday.xyzw ACC,vf17xyzw,vf10y\n\t"
        "vmaddaz.xyzw ACC,vf18xyzw,vf10z\n\t"
        "vmaddw.xyzw vf9xyzw,vf19xyzw,vf0w\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmulax.xyzw ACC,vf12xyzw,vf10x\n\t"
        "vmadday.xyzw ACC,vf13xyzw,vf10y\n\t"
        "vmaddaz.xyzw ACC,vf14xyzw,vf10z\n\t"
        "vmaddw.xyzw vf9xyzw,vf15xyzw,vf0w\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9xyzw,vf9xyzw\n\t"
        "qmfc2 $2,vf9\n\t"
        "mtc1 $2,$f0\n\t"
        "c.lt.s $f0,$f1\n\t"
        "bc1f 1f\n\t"
        "sub $2,$2,$2\n\t"
        "j 2f\n\t"
        "1:\n\t"
        "sub $2,$2,$2\n\t"
        "addi $2,$2,1\n\t"
        "2:\n\t"
        "move %0,$2"
        : "=r"(clipped)
        : "r"(model)
        : "$2", "$f0", "$f1", "memory");
    return clipped;
}

s32 xglStudioGetActiveCamera(void);
s32 _ModelCalcClip(s32 model);
s32 xglCullingCheck(s32 camera, s32 model);

/*
 * Tests model bounds against the active camera's combined view volume and
 * returns its clipping mask: the frustum clip mask _ModelCalcClip computes,
 * combined with xglCullingCheck's registered-volume mask for the same
 * camera/model pair.
 */
s32 nmlModelCalcClip(s32 model) {
    s32 clip;
    s32 camera = xglStudioGetActiveCamera();

    if (camera != 0) {
        _ModelCalcClipInit((const Matrix4 *) (camera + 0x4F0));
        clip = _ModelCalcClip(model);
        clip |= xglCullingCheck(camera, model);
        return clip;
    }
    return camera;
}

s32 xglStudioGetActiveCamera(void);
s32 _ModelCalcClip(s32 model);

/*
 * Tests model against the active camera's clip state directly, without
 * consulting the registered culling-map volumes xglCullingCheck reads.
 */
s32 nmlModelCalcClipNoCulling(s32 model) {
    s32 camera = xglStudioGetActiveCamera();

    if (camera != 0) {
        _ModelCalcClipInit((const Matrix4 *) (camera + 0x4F0));
        return _ModelCalcClip(model);
    }
    return camera;
}

/*
 * Transforms model's bounds by matrix (the same row-by-row point transform
 * as _ApplyMatrix) into a stack-local Vector4, then passes its address in
 * place of a model handle to _ModelCalcClip and xglCullingCheck: model is
 * itself the address of a per-model bounding Vector4 (dereferenced here via
 * lqc2, the same handle-as-address convention nmlModelCalcClipCam's
 * camera + 0x4F0 cast already evidences), and the transformed copy is
 * tested against the active camera's combined view volume the same way
 * nmlModelCalcClip tests the untransformed one.
 */
s32 nmlModelCalcClipMat1(s32 model, s32 matrix) {
    Vector4 bounds;
    s32 clip;
    s32 camera = xglStudioGetActiveCamera();

    if (camera != 0) {
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
            :
            : "r"(model), "r"(matrix), "r"(&bounds)
            : "memory"
        );
        _ModelCalcClipInit((const Matrix4 *) (camera + 0x4F0));
        clip = _ModelCalcClip((s32) &bounds);
        clip |= xglCullingCheck(camera, (s32) &bounds);
        return clip;
    }
    return camera;
}

s32 xglCullingCheck(s32 camera, s32 model);

/*
 * Transforms model's bounds through matrix1 and then matrix2 (the same
 * two-pass point transform as _ApplyMatrix2Mat) into a stack-local Vector4,
 * then tests that transformed copy against the active camera's combined
 * view volume the way nmlModelCalcClip tests the untransformed one: model
 * is itself the address of a per-model bounding Vector4.
 */
s32 nmlModelCalcClipMat2(s32 model, s32 matrix1, s32 matrix2) {
    Vector4 bounds;
    s32 clip;
    s32 camera = xglStudioGetActiveCamera();

    if (camera != 0) {
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
            "lqc2 vf27, 0(%2)\n\t"
            "lqc2 vf28, 16(%2)\n\t"
            "lqc2 vf29, 32(%2)\n\t"
            "lqc2 vf30, 48(%2)\n\t"
            "vmulax.xyz ACC, vf27, vf31x\n\t"
            "vmadday.xyz ACC, vf28, vf31y\n\t"
            "vmaddaz.xyz ACC, vf29, vf31z\n\t"
            "vmaddw.xyz vf31, vf30, vf0w\n\t"
            "sqc2 vf31, 0(%3)\n\t"
            :
            : "r"(model), "r"(matrix1), "r"(matrix2), "r"(&bounds)
            : "memory"
        );
        _ModelCalcClipInit((const Matrix4 *) (camera + 0x4F0));
        clip = _ModelCalcClip((s32) &bounds);
        clip |= xglCullingCheck(camera, (s32) &bounds);
        return clip;
    }
    return camera;
}

s32 _ModelCalcClip(s32 model);

/*
 * camera + 0x4F0 is the pair of 4x4 clip matrices _ModelCalcClipInit reads
 * (see its own comment): each nmlModelCalcClip*Cam wrapper loads camera's
 * clip state, then tail-calls the matching _ModelCalcClip* worker on model.
 */
void nmlModelCalcClipCam(s32 model, s32 camera) {
    _ModelCalcClipInit((const Matrix4 *) (camera + 0x4F0));
    _ModelCalcClip(model);
}

s32 _ModelCalcClipMat1(s32 model, s32 matrix);

void nmlModelCalcClipMat1Cam(s32 model, s32 matrix, s32 camera) {
    _ModelCalcClipInit((const Matrix4 *) (camera + 0x4F0));
    _ModelCalcClipMat1(model, matrix);
}

s32 _ModelCalcClipMat2(s32 model, s32 matrix1, s32 matrix2);

void nmlModelCalcClipMat2Cam(s32 model, s32 matrix1, s32 matrix2, s32 camera) {
    _ModelCalcClipInit((const Matrix4 *) (camera + 0x4F0));
    _ModelCalcClipMat2(model, matrix1, matrix2);
}

s32 xglCullingCheck(s32 camera, s32 model);
s32 xglStudioSelectGetActiveCamera(s32 cameraIndex);

/*
 * Selects the studio camera identified by cameraIndex, then combines
 * _ModelCalcClip's frustum clip mask for model with xglCullingCheck's
 * registered-volume mask for the same camera/model pair.
 */
s32 nmlModelCalcClipStudio(s32 model, s32 cameraIndex) {
    s32 clip = 0;
    s32 camera = xglStudioSelectGetActiveCamera(cameraIndex);

    if (camera != 0) {
        _ModelCalcClipInit((const Matrix4 *) (camera + 0x4F0));
        clip = _ModelCalcClip(model);
        clip |= xglCullingCheck(camera, model);
    }
    return clip;
}

extern int g_aSubWindow[4];

/*
 * Transforms model's bounds by matrix and tests them in every enabled
 * sub-window's camera: the model is clipped away (1) only when every
 * enabled sub-window clips it, and the first sub-window that keeps it
 * answers 0. A sub-window without a selectable camera ends the sweep.
 */
s32 nmlModelCalcClipMat1AllCam(s32 model, s32 matrix)
{
    Vector4 bounds;
    s32 clipped = 1;
    s32 camera;
    s32 clip;
    s32 i;

    for (i = 0; i < 4; i++) {
        camera = xglStudioSelectGetActiveCamera(i);
        if (camera == 0) {
            return camera;
        }
        if (g_aSubWindow[i] != 0) {
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
                :
                : "r"(model), "r"(matrix), "r"(&bounds)
                : "memory"
            );
            _ModelCalcClipInit((const Matrix4 *) (camera + 0x4F0));
            clip = _ModelCalcClip((s32) &bounds);
            clip |= xglCullingCheck(camera, (s32) &bounds);
            if (clip == 0) {
                clipped = 0;
                break;
            }
        }
    }
    return clipped;
}

/*
 * Transforms model's bounds through matrix1 and matrix2 and collects the
 * frustum clip mask of every enabled sub-window: each tested sub-window
 * contributes its mask shifted to its own bit, and bit 7 is added when
 * every tested sub-window reported the same single clip bit. Only the
 * first three sub-windows are swept, and a sub-window without a selectable
 * camera answers 0.
 */
s32 nmlModelCalcClipMat2AllCam(s32 model, s32 matrix1, s32 matrix2)
{
    Vector4 bounds;
    s32 clipMask = 0;
    s32 tested = 0;
    s32 total = 0;
    s32 camera;
    s32 clip;
    s32 i;

    for (i = 0; i < 3; i++) {
        if (g_aSubWindow[i] != 0) {
            camera = xglStudioSelectGetActiveCamera(i);
            if (camera == 0) {
                return camera;
            }
            _ModelCalcClipInit((const Matrix4 *) (camera + 0x4F0));
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
                "lqc2 vf27, 0(%2)\n\t"
                "lqc2 vf28, 16(%2)\n\t"
                "lqc2 vf29, 32(%2)\n\t"
                "lqc2 vf30, 48(%2)\n\t"
                "vmulax.xyz ACC, vf27, vf31x\n\t"
                "vmadday.xyz ACC, vf28, vf31y\n\t"
                "vmaddaz.xyz ACC, vf29, vf31z\n\t"
                "vmaddw.xyz vf31, vf30, vf0w\n\t"
                "sqc2 vf31, 0(%3)\n\t"
                :
                : "r"(model), "r"(matrix1), "r"(matrix2), "r"(&bounds)
                : "memory"
            );
            clip = _ModelCalcClip((s32) &bounds);
            tested++;
            clipMask |= clip << i;
            total += clip;
        }
    }
    if (tested != 0) {
        if (total == tested) {
            clipMask |= 0x80;
        }
    }
    return clipMask;
}

/*
 * ACC accumulates matrix * vector row by row: row 0 times x, row 1 times y,
 * and vmaddz folds row 2 times z directly into vf31 without a separate ACC
 * step. Row 3 (the translation row) is loaded into vf30 but never used --
 * this applies only the 3x3 rotation part of matrix. vmaddz writes only
 * vf31's xyz lanes, so destination->w keeps whatever the first lqc2 loaded
 * from vector->w.
 */
void _ApplyMatrix33(Vector4 *destination, const Matrix4 matrix, const Vector4 *vector) {
    __asm__ __volatile__(
        "lqc2 vf31, 0(%0)\n\t"
        "lqc2 vf27, 0(%1)\n\t"
        "lqc2 vf28, 16(%1)\n\t"
        "lqc2 vf29, 32(%1)\n\t"
        "lqc2 vf30, 48(%1)\n\t"
        "vmulax.xyz ACC, vf27, vf31x\n\t"
        "vmadday.xyz ACC, vf28, vf31y\n\t"
        "vmaddz.xyz vf31, vf29, vf31z\n\t"
        "sqc2 vf31, 0(%2)\n\t"
        "nop\n\t"
        :
        : "r"(vector), "r"(matrix), "r"(destination)
        : "memory"
    );
}

/*
 * Same row-by-row accumulation as _ApplyMatrix33, extended with the
 * translation row: vmaddw folds row 3 (times vf0's constant w lane, 1.0)
 * into vf31's xyz lanes, so vector is transformed as a point. Only xyz
 * lanes are ever written to vf31, so destination->w again keeps whatever
 * the first lqc2 loaded from vector->w.
 */
void _ApplyMatrix(Vector4 *destination, const Matrix4 matrix, const Vector4 *vector) {
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
        "nop\n\t"
        :
        : "r"(vector), "r"(matrix), "r"(destination)
        : "memory"
    );
}

/*
 * Applies matrix1 then matrix2 to vector in sequence (matrix2 * (matrix1 *
 * vector)) instead of pre-combining the two matrices first: each pass is
 * the same row-by-row accumulation as _ApplyMatrix, translation row
 * included (vmaddw times vf0's constant w lane, 1.0). Only xyz lanes are
 * ever written to vf31, so destination->w keeps whatever the first lqc2
 * loaded from vector->w.
 */
void _ApplyMatrix2Mat(Vector4 *destination, const Matrix4 matrix1, const Matrix4 matrix2, const Vector4 *vector) {
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
        "lqc2 vf27, 0(%2)\n\t"
        "lqc2 vf28, 16(%2)\n\t"
        "lqc2 vf29, 32(%2)\n\t"
        "lqc2 vf30, 48(%2)\n\t"
        "vmulax.xyz ACC, vf27, vf31x\n\t"
        "vmadday.xyz ACC, vf28, vf31y\n\t"
        "vmaddaz.xyz ACC, vf29, vf31z\n\t"
        "vmaddw.xyz vf31, vf30, vf0w\n\t"
        "sqc2 vf31, 0(%3)\n\t"
        "nop\n\t"
        :
        : "r"(vector), "r"(matrix1), "r"(matrix2), "r"(destination)
        : "memory"
    );
}

/*
 * OV12 original TU 58: 0x00a2f650..0x00a303a8 (15 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov12/xrg_rand_int.h"

/* Referenced original data recovered from this translation unit. */
const char D_00A552F8[32] = "../rg_geom_collision.euc.c";
const char D_00A55328[16] = "pArg != NIL";

extern int RgGeomGetType(RgGeom *pGeom);
extern int RgGeomBallCheckBall(RgGeom *a, RgGeom *b, void *contact);
extern int RgGeomBallCheckBallDist(RgGeom *a, RgGeom *b, void *contact);
extern float RgGeomPointGetWeight(RgGeomPoint *point);
extern void RgGeomPointSetPos(RgGeomPoint *point, RgVector position);
extern void XrgAddVector(RgVector destination, RgVector first, RgVector second);
extern void XrgSubVector(RgVector destination, RgVector first, RgVector second);
extern void XrgScaleVector(RgVector destination, RgVector source, float scale);
extern void XrgCopyVector(RgVector destination, RgVector source);
extern void XrgNegateVector(RgVector destination, RgVector source);
typedef struct {
    RgVector position1;            /* +0x00 */
    RgVector position2;            /* +0x10 */
    RgVector penetration;          /* +0x20 */
    float distance;                /* +0x30 */
} RgBallVsBallContact;
typedef struct {
    RgVector contact;              /* +0x00 */
    RgVector normal;               /* +0x10 */
    void *otherObject;             /* +0x20 */
} RgBallVsBallHit;
typedef struct {
    unsigned char unmodeled_00[8];
    unsigned int flags;            /* +0x08 */
} RgBallVsBallBody;

static int _BallVsBall(unsigned int *pArg)
{
    RgBallVsBallContact contact;
    RgVector scaled;
    RgVector moved;
    RgBallVsBallHit hit;
    RgGeom *a;
    RgGeom *b;
    void (*callback)(void *, void *);
    int typeA;
    int typeB;
    int result;
    unsigned int flags;
    float weightA;
    float weightB;
    float total;

    a = (RgGeom *) pArg[0];
    b = (RgGeom *) pArg[2];
    typeA = RgGeomGetType(a);
    typeB = RgGeomGetType(b);
    if (typeA == 2) {
        if (typeB == typeA) {
            result = RgGeomBallCheckBallDist(a, b, &contact);
            goto dispatched;
        }
    }
    if (typeA != typeB && (typeA == 2 || typeB == 2)) {
        result = RgGeomBallCheckBall(a, b, &contact);
    } else {
        result = RgGeomBallCheckBallDist(a, b, &contact);
    }
dispatched:
    if (result != 0) {
        flags = ((RgBallVsBallBody *) a)->flags & ((RgBallVsBallBody *) b)->flags;
        weightA = RgGeomPointGetWeight((RgGeomPoint *) a);
        weightB = RgGeomPointGetWeight((RgGeomPoint *) b);
        if (flags & 0x10) {
            total = weightA + weightB;
            XrgScaleVector(scaled, contact.penetration, weightB / total);
            XrgAddVector(moved, contact.position1, scaled);
            RgGeomPointSetPos((RgGeomPoint *) a, moved);
            XrgScaleVector(scaled, contact.penetration, weightA / total);
            XrgSubVector(moved, contact.position2, scaled);
            RgGeomPointSetPos((RgGeomPoint *) b, moved);
        }
        __asm__ __volatile__(
            "lqc2 $vf30, 0(%1)\n\t"
            "lqc2 $vf31, 0(%2)\n\t"
            "vadd.xyzw $vf30, $vf30, $vf31\n\t"
            "sqc2 $vf30, 0(%0)\n\t"
            : : "r"(hit.contact), "r"(contact.position1), "r"(contact.position2) : "memory");
        hit.contact[0] *= 0.5f;
        hit.contact[1] *= 0.5f;
        hit.contact[2] *= 0.5f;
        hit.contact[3] *= 0.5f;
        XrgCopyVector(hit.normal, contact.penetration);
        callback = (void (*)(void *, void *)) pArg[1];
        if (callback != 0) {
            hit.otherObject = b;
            callback(a, &hit);
        }
        if (pArg[3] != 0) {
            XrgNegateVector(hit.normal, hit.normal);
            hit.otherObject = a;
            ((void (*)(void *, void *)) pArg[3])(b, &hit);
        }
        return 1;
    }
    return 0;
}

extern int RgGeomPolyCheckRay(RgGeom *pPoly, RgVector origin, RgVector out);
extern void XrgNegateVector(RgVector destination, RgVector source);
typedef struct {
    RgVector contact;              /* +0x00 */
    RgVector normal;               /* +0x10 */
    void *otherObject;             /* +0x20 */
    unsigned char unmodeled_24[0xC];
} RgBallVsPolyHit;
typedef struct {
    RgVector p0;                   /* +0x00 */
    RgVector dir;                  /* +0x10 */
    RgVector normal;               /* +0x20 ray out */
    float t;                       /* +0x30 ray out */
    unsigned char unmodeled_34[0xC];
    RgVector ptB;                  /* +0x40 */
    RgVector ptA;                  /* +0x50 */
    RgBallVsPolyHit hit;           /* +0x60 */
} RgBallVsPolyFrame;
static inline void BvpCopy(void *dst, const void *src)
{
    __asm__ __volatile__("lqc2 $vf31, 0(%0)\n\tsqc2 $vf31, 0(%1)" : : "r"(src), "r"(dst) : "memory");
}
static inline void BvpXform(void *dst, const void *src, const void *m)
{
    __asm__ __volatile__(
        "lqc2 $vf2, 0(%1)\n\t"
        "lqc2 $vf3, 0(%2)\n\t"
        "lqc2 $vf4, 0x10(%2)\n\t"
        "lqc2 $vf5, 0x20(%2)\n\t"
        "lqc2 $vf6, 0x30(%2)\n\t"
        "vmulax.xyzw ACC, $vf3, $vf2x\n\t"
        "vmadday.xyzw ACC, $vf4, $vf2y\n\t"
        "vmaddaz.xyzw ACC, $vf5, $vf2z\n\t"
        "vmaddw.xyzw $vf2, $vf6, $vf2w\n\t"
        "sqc2 $vf2, 0(%0)\n\t"
        : : "r"(dst), "r"(src), "r"(m) : "memory");
}
static inline void BvpSub(void *dst, const void *a, const void *b)
{
    __asm__ __volatile__(
        "lqc2 $vf30, 0(%1)\n\t"
        "lqc2 $vf31, 0(%2)\n\t"
        "vsub.xyzw $vf30, $vf30, $vf31\n\t"
        "sqc2 $vf30, 0(%0)\n\t"
        : : "r"(dst), "r"(a), "r"(b) : "memory");
}
static inline void BvpAdd(void *dst, const void *a, const void *b)
{
    __asm__ __volatile__(
        "lqc2 $vf30, 0(%1)\n\t"
        "lqc2 $vf31, 0(%2)\n\t"
        "vadd.xyzw $vf30, $vf30, $vf31\n\t"
        "sqc2 $vf30, 0(%0)\n\t"
        : : "r"(dst), "r"(a), "r"(b) : "memory");
}

static int _BallVsPoly(unsigned int *pArg)
{
    RgBallVsPolyFrame f;
    char *point;
    RgGeom *poly;
    void (*callback)(void *, void *);
    int result;

    point = (char *) pArg[0];
    poly = (RgGeom *) pArg[2];
    BvpCopy(f.ptA, point + 0x20);
    BvpCopy(f.ptB, point + 0x30);
    f.ptA[3] = 1.0f;
    f.ptB[3] = 1.0f;
    BvpXform(f.ptA, f.ptA, (char *) poly + 0x60);
    BvpXform(f.ptB, f.ptB, (char *) poly + 0x60);
    BvpCopy(f.p0, point + 0x30);
    BvpSub(f.dir, point + 0x20, point + 0x30);
    result = RgGeomPolyCheckRay(poly, f.p0, f.normal);
    if (result != 0) {
        float t = f.t;
        f.hit.contact[0] = f.dir[0] * t;
        f.hit.contact[1] = f.dir[1] * t;
        f.hit.contact[2] = f.dir[2] * t;
        f.hit.contact[3] = f.dir[3] * t;
        BvpAdd(f.hit.contact, f.hit.contact, f.p0);
        BvpCopy(f.hit.normal, f.normal);
        f.hit.normal[3] = 1.0f;
        callback = (void (*)(void *, void *)) pArg[1];
        if (callback != 0) {
            f.hit.otherObject = poly;
            callback(point, &f.hit);
        }
        if (pArg[3] != 0) {
            XrgNegateVector(f.hit.normal, f.hit.normal);
            f.hit.otherObject = point;
            ((void (*)(void *, void *)) pArg[3])(poly, &f.hit);
        }
        result = 1;
    }
    return result;
}

static int _BallVsPoly(unsigned int *pArg);

static void _PolyVsBall(unsigned int *pArg)
{
    unsigned int reordered[4];
    unsigned int first;
    unsigned int second;
    unsigned int third;
    unsigned int fourth;

    third = pArg[2];
    first = pArg[0];
    fourth = pArg[3];
    second = pArg[1];
    reordered[0] = third;
    reordered[2] = first;
    reordered[1] = fourth;
    reordered[3] = second;
    _BallVsPoly(reordered);
}

extern void assert_prog(const char *expression, const char *source_file, int line);
extern const char D_00A552E8[];
extern const char D_00A55318[];
extern void __RgGeomPointGetPos(RgGeomPoint *point, RgVector destination,
                                const char *file, int line);
extern float RgGeomBallGetRadius(RgGeomPoint *pBall);
extern int RgGeomPolyCheckBall(RgGeom *pPoly, float *pOrgArg, float *pResult);
extern void RgGeomPointSetPos(RgGeomPoint *point, RgVector position);
extern void RgGeomPointGetVel(RgGeomPoint *point, RgVector velocity);
extern float XrgNormalizeVector(RgVector destination, RgVector source);
extern void XrgAddVector(RgVector destination, RgVector first, RgVector second);
extern void XrgScaleVector(RgVector destination, RgVector source, float scale);
extern float XrgInnerVector(RgVector first, RgVector second);
typedef struct {
    unsigned char unmodeled_00[8];
    unsigned int flags;            /* +0x08 */
    unsigned char unmodeled_0c[0x14];
    RgVector position;             /* +0x20 */
    unsigned char unmodeled_30[0x10];
    RgVector velocity;             /* +0x40 */
} RgRobVsPolyPoint;
typedef struct {
    RgVector contact;              /* +0x00 */
    RgVector normal;               /* +0x10 */
    void *otherObject;             /* +0x20 */
    unsigned char unmodeled_24[0xC];
} RgRobVsPolyHit;
typedef struct {
    RgVector pos;                  /* +0x00 */
    float radius;                  /* +0x10 */
    unsigned char unmodeled_14[0xC];
    RgVector contact;              /* +0x20 */
    RgVector normal;               /* +0x30 */
    RgRobVsPolyHit hit;            /* +0x40 */
    RgVector moved;                /* +0x70 */
    RgVector dir;                  /* +0x80 */
    RgVector copy;                 /* +0x90 */
    RgVector vel;                  /* +0xA0 */
} RgRobVsPolyFrame;

static int _RobotVsPoly(unsigned int *pArg)
{
    RgRobVsPolyFrame f;
    RgRobVsPolyPoint *point;
    RgGeom *poly;
    RgRobVsPolyHit *hitp;
    float *copyp;
    void (*callback)(void *, void *);

    point = (RgRobVsPolyPoint *) pArg[0];
    poly = (RgGeom *) pArg[2];
    if (point == 0) {
        assert_prog(D_00A552E8, D_00A552F8, 0xA1);
    }
    if (poly == 0) {
        assert_prog(D_00A55318, D_00A552F8, 0xA2);
    }
    __RgGeomPointGetPos((RgGeomPoint *) point, f.pos, D_00A552F8, 0xA4);
    f.radius = RgGeomBallGetRadius((RgGeomPoint *) point);
    if (RgGeomPolyCheckBall(poly, f.pos, f.contact) == 0) {
        return 0;
    }
    hitp = &f.hit;
    XrgNormalizeVector(f.hit.normal, f.normal);
    callback = (void (*)(void *, void *)) pArg[1];
    if (callback != 0) {
        f.hit.otherObject = poly;
        callback(point, hitp);
    }
    callback = (void (*)(void *, void *)) pArg[3];
    if (callback != 0) {
        f.hit.otherObject = point;
        callback(poly, hitp);
    }
    if (point->flags & 0x10) {
        __RgGeomPointGetPos((RgGeomPoint *) point, f.moved, D_00A552F8, 0xB9);
        copyp = f.copy;
        __asm__ __volatile__("lqc2 vf31, 0(%0)" : : "r"(f.normal) : "memory");
        __asm__ __volatile__("sqc2 vf31, 0(%0)" : : "r"(copyp) : "memory");
        XrgAddVector(f.moved, f.moved, f.copy);
        RgGeomPointSetPos((RgGeomPoint *) point, f.moved);
    }
    XrgNormalizeVector(f.dir, f.normal);
    RgGeomPointGetVel((RgGeomPoint *) point, f.vel);
    XrgScaleVector(f.moved, f.dir, XrgInnerVector(f.dir, f.vel));
    XrgSubVector(point->velocity, point->velocity, f.moved);
    return 1;
}

static int _RobotVsPoly(unsigned int *pArg);

static void _PolyVsRobot(unsigned int *pArg)
{
    unsigned int reordered[4];
    unsigned int first;
    unsigned int second;
    unsigned int third;
    unsigned int fourth;

    third = pArg[2];
    first = pArg[0];
    fourth = pArg[3];
    second = pArg[1];
    reordered[0] = third;
    reordered[2] = first;
    reordered[1] = fourth;
    reordered[3] = second;
    _RobotVsPoly(reordered);
}

INCLUDE_ASM("asm/nonmatchings/ov12/rg_geom_collision", _BallVsTray);

static int _BallVsTray(unsigned int *pArg);

static void _TrayVsBall(void *pArg)
{
    unsigned int *argWords = pArg;
    unsigned int reordered[4];
    unsigned int first;
    unsigned int second;
    unsigned int third;
    unsigned int fourth;

    third = argWords[2];
    first = argWords[0];
    fourth = argWords[3];
    second = argWords[1];
    reordered[0] = third;
    reordered[2] = first;
    reordered[1] = fourth;
    reordered[3] = second;
    _BallVsTray(reordered);
}

/*
 * The pairwise collision-check argument this TU's _XxxVsYyy functions use
 * (still INCLUDE_ASM in this allocation, so only InitRgGeomColiArg's own
 * writes are evidenced here: it clears exactly these four words to a common
 * role, so they are modelled as one array rather than four distinct fields).
 */
typedef struct RgGeomColiArg {
    unsigned int data[4];
} RgGeomColiArg;

extern int RgGeomPillarCheckPoint(RgGeom *pillar, RgGeomPoint *point, RgVector contact);

/*
 * A pairwise collision-response callback, stored by RgGeomColiArg's own
 * words (data[1]/data[3] above): called with the OTHER side's object
 * pointer and the contact point _BallVsPillar computes.
 */
typedef void (*RgGeomColiCallback)(void *object, RgVector contact);

/*
 * A hit-normal scratch record _BallVsPillar builds beside the contact point:
 * a cleared RgVector whose Y lane is then set to 1.0 (main 0x00a2fe64/
 * 0x00a2fe6c), followed by the other collider's own pointer (main
 * 0x00a2fe78/0x00a2fe80). Neither of this function's own calls reads it
 * back, so its consumer is outside this allocation's evidence.
 */
typedef struct {
    RgVector normal;      /* +0x00 */
    void *otherObject;    /* +0x10 */
    unsigned char unmodeled_14[0xC];
} RgGeomColiHit;

static int _BallVsPillar(RgGeomColiArg *pArg)
{
    RgGeomPoint *point;
    RgGeom *pillar;
    RgVector contact;
    RgVector hitContact;
    float *hitContactPtr;
    RgGeomColiHit hit;
    RgGeomColiCallback callback;
    int result;

    point = (RgGeomPoint *) pArg->data[0];
    pillar = (RgGeom *) pArg->data[2];
    result = RgGeomPillarCheckPoint(pillar, point, contact);
    hitContactPtr = hitContact;
    if (result != 0) {
        __asm__ __volatile__("lqc2 vf31, 0(%0)" : : "r"(contact) : "memory");
        __asm__ __volatile__("sqc2 vf31, 0(%0)" : : "r"(hitContactPtr) : "memory");
        XrgClearVector(hit.normal);
        callback = (RgGeomColiCallback) pArg->data[1];
        hit.normal[1] = 1.0f;
        if (callback != 0) {
            hit.otherObject = pillar;
            callback(point, hitContactPtr);
        }
        callback = (RgGeomColiCallback) pArg->data[3];
        if (callback != 0) {
            hit.otherObject = point;
            callback(pillar, hitContactPtr);
        }
        result = 1;
    }
    return result;
}

static void _PillarVsBall(RgGeomColiArg *pArg)
{
    RgGeomColiArg reordered;
    unsigned int first;
    unsigned int second;
    unsigned int third;
    unsigned int fourth;

    third = pArg->data[2];
    first = pArg->data[0];
    fourth = pArg->data[3];
    second = pArg->data[1];
    reordered.data[0] = third;
    reordered.data[2] = first;
    reordered.data[1] = fourth;
    reordered.data[3] = second;
    _BallVsPillar(&reordered);
}

extern int RgGeomTrayCheckBall(void *tray, void *ball, void *result);
extern void XrgClearVector(RgVector destination);
extern void XrgSubVector(RgVector destination, RgVector first, RgVector second);
typedef struct {
    RgVector contact;              /* +0x00 */
    RgVector pos;                  /* +0x10 */
    RgVector normals[3];           /* +0x20 */
    unsigned int count;            /* +0x50 */
} RgRobTrayRes;

static int _RobVsTray(unsigned int *pArg)
{
    RgRobTrayRes res;
    RgRobVsPolyHit hit;
    RgRobVsPolyHit *hitp;
    void *tray;
    RgRobVsPolyPoint *point;
    RgVector sum;
    RgVector vel;
    void (*callback)(void *, void *);
    int i;
    float *normal;
    float *m;
    float *vp;
    float dot;
    int result;

    point = (RgRobVsPolyPoint *) pArg[0];
    tray = (void *) pArg[2];
    result = RgGeomTrayCheckBall(tray, point, &res);
    if (result != 0) {
        hitp = &hit;
        __asm__ __volatile__("lqc2 vf31, 0(%0)" : : "r"(res.contact) : "memory");
        __asm__ __volatile__("sqc2 vf31, 0(%0)" : : "r"(hitp) : "memory");
        if (res.count >= 2) {
            XrgClearVector(sum);
            for (i = 0; i < res.count; i++) {
                BvpAdd(sum, sum, res.normals[i]);
            }
            normal = hit.normal;
            XrgNormalizeVector(normal, sum);
        } else {
            normal = hit.normal;
            __asm__ __volatile__("lqc2 vf31, 0(%0)" : : "r"(res.normals[0]) : "memory");
            __asm__ __volatile__("sqc2 vf31, 0(%0)" : : "r"(normal) : "memory");
        }
        callback = (void (*)(void *, void *)) pArg[1];
        if (callback != 0) {
            hit.otherObject = tray;
            callback(point, hitp);
        }
        callback = (void (*)(void *, void *)) pArg[3];
        if (callback != 0) {
            hit.otherObject = point;
            callback(tray, hitp);
        }
        if (point->flags & 0x10) {
            RgGeomPointSetPos((RgGeomPoint *) point, res.pos);
        }
        i = 0;
        if (res.count != 0) {
            m = (float *) res.normals;
            do {
                __asm__ __volatile__("lqc2 vf31, 0(%0)\n\tsqc2 vf31, 0(%1)" : : "r"(point->velocity), "r"(vel) : "memory");
                dot = m[0] * vel[0] + m[1] * vel[1] + m[2] * vel[2];
                if (dot < 0) {
                    XrgScaleVector(sum, (float *) m, dot);
                    XrgSubVector(point->velocity, point->velocity, sum);
                }
                m += 4;
                i++;
            } while (i < res.count);
        }
        result = 1;
    }
    return result;
}

/* Still INCLUDE_ASM in this TU. hit: a four-word pair record whose halves
 * _TrayVsBall swaps before calling _BallVsTray (layout not yet recovered). */
static void _TrayVsBall(void *hit);

/* Reuses tray-versus-ball collision handling for a robot collider (elf_names
 * annotation); a plain tail call to _TrayVsBall with the caller's record. */
static void _TrayVsRob(void *hit)
{
    _TrayVsBall(hit);
}

extern int RgGeomPillarCheckBall(RgGeom *pillar, RgGeomPoint *ball, RgVector contact);
extern void XrgNegateVector(RgVector destination, RgVector source);
extern void XrgCopyVector(RgVector destination, RgVector source);
extern void XrgSubVector(RgVector destination, RgVector first, RgVector second);
typedef struct {
    RgVector c0;                   /* +0x00 */
    RgVector r10;                  /* +0x10 */
    RgVector c20;                  /* +0x20 */
    RgRobVsPolyHit hit;            /* +0x30 */
    RgVector v60;                  /* +0x60 */
    RgVector v70;                  /* +0x70 */
} RgRobVsPillarFrame;

static int _RobVsPillar(RgGeomColiArg *pArg)
{
    RgRobVsPillarFrame f;
    RgRobVsPolyPoint *point;
    RgGeom *pillar;
    RgRobVsPolyHit *hitp;
    float d;
    float *normalp;
    float *contactp;
    int result;

    point = (RgRobVsPolyPoint *) pArg->data[0];
    pillar = (RgGeom *) pArg->data[2];
    result = RgGeomPillarCheckBall(pillar, (RgGeomPoint *) point, f.c0);
    if (result != 0) {
        contactp = f.hit.contact;
        __asm__ __volatile__("lqc2 vf31, 0(%0)" : : "r"(f.c0) : "memory");
        __asm__ __volatile__("sqc2 vf31, 0(%0)" : : "r"(contactp) : "memory");
        normalp = f.hit.normal;
        __asm__ __volatile__("lqc2 vf31, 0(%0)" : : "r"(f.c20) : "memory");
        __asm__ __volatile__("sqc2 vf31, 0(%0)" : : "r"(normalp) : "memory");
        hitp = &f.hit;
        if (pArg->data[1] != 0) {
            f.hit.otherObject = pillar;
            ((void (*)(void *, void *)) pArg->data[1])(point, hitp);
        }
        if (pArg->data[3] != 0) {
            XrgNegateVector(f.hit.normal, f.hit.normal);
            f.hit.otherObject = point;
            ((void (*)(void *, void *)) pArg->data[3])(pillar, hitp);
        }
        if (point->flags & 0x10) {
            RgGeomPointSetPos((RgGeomPoint *) point, f.r10);
        }
        XrgCopyVector(f.v70, point->velocity);
        d = XrgInnerVector(f.c20, f.v70);
        if (d < 0.0f) {
            XrgScaleVector(f.v60, f.c20, d);
            XrgSubVector(point->velocity, point->velocity, f.v60);
        }
        result = 1;
    }
    return result;
}

static int _RobVsPillar(RgGeomColiArg *pArg);

static void _PillarVsRob(RgGeomColiArg *pArg)
{
    RgGeomColiArg reordered;
    unsigned int first;
    unsigned int second;
    unsigned int third;
    unsigned int fourth;

    third = pArg->data[2];
    first = pArg->data[0];
    fourth = pArg->data[3];
    second = pArg->data[1];
    reordered.data[0] = third;
    reordered.data[2] = first;
    reordered.data[1] = fourth;
    reordered.data[3] = second;
    _RobVsPillar(&reordered);
}

extern void assert_prog(const char *expression, const char *source_file, int line);

extern const char D_00A55328[]; /* "pArg != NIL" */
extern const char D_00A552F8[]; /* "../rg_geom_collision.euc.c" */

void InitRgGeomColiArg(RgGeomColiArg *pArg)
{
    if (pArg == 0) {
        assert_prog(D_00A55328, D_00A552F8, 465);
    }
    pArg->data[0] = 0;
    pArg->data[1] = 0;
    pArg->data[2] = 0;
    pArg->data[3] = 0;
}

extern int RgGeomGetType(RgGeom *pGeom);
typedef struct {
    int unk0;
    int unk4;
    int flags;
    int hit;
} RgGeomColiHdr;
typedef struct {
    RgGeomColiHdr *geomA;
    void *funcA;
    RgGeomColiHdr *geomB;
    void *funcB;
} RgGeomColiJobArg;
extern int (*s_apJobFuncTbl[])(RgGeomColiJobArg *);

int RgGeomCollisionJob(RgGeomColiJobArg *pArg)
{
    int result = 0;
    int typeA;
    int typeB;
    int (*func)(RgGeomColiJobArg *);

    if (pArg == 0) {
        assert_prog(D_00A55328, D_00A552F8, 480);
    }
    if (pArg->geomA == 0 || pArg->geomB == 0) {
        return 0;
    }
    if (!(pArg->geomA->flags & pArg->geomB->flags & 1)) {
        return 0;
    }
    typeA = RgGeomGetType((RgGeom *)pArg->geomA);
    typeB = RgGeomGetType((RgGeom *)pArg->geomB);
    if (typeA >= 0 && typeB >= 0) {
        func = s_apJobFuncTbl[typeA * 6 + typeB];
        if (func != 0) {
            result = func(pArg);
        }
        if (result != 0) {
            pArg->geomA->hit = 1;
            pArg->geomB->hit = 1;
        }
    }
    return result;
}

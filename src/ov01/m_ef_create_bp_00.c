#include "port/ps2_types.h"
#include "port/ps2_asm.h"
/*
 * OV01 original TU 24: 0x00a346c8..0x00a34e90 (5 functions)
 */
#include "common.h"
#include "shared.h"
#include "m_ef_create_bp_00.h"

extern void MEfGetActorMatrix(Matrix4 destination, unsigned int actor, unsigned int part);
extern void MEfGetActorCoord(Vector4 *destination, unsigned int actor, unsigned int coordinate);
extern Vector4 *MMathApplyMatrix(Vector4 *destination, const Matrix4 *matrix, const Vector4 *vector);
extern Vector4 *MMathCalcVectorMatrix(Vector4 *destination, const Matrix4 *matrix);
extern void MGsGPInit(void *packet, void *address, int size);

static void fnBP00_PR000(void *self, void *work);
static void fnBP00_DP000(void *self, void *work);
static void fnBP00_PO000(void *self, void *work);

int MEfCreate_BP00(BP00Object *object)
{
    BP00Work *work = &object->work;
    Vector4 direction;
    Matrix4 actorMatrix;

    work->frame = 0;
    work->counter = 0;
    MEfGetActorMatrix(actorMatrix, work->actorId, work->actorPart);
    /* GCC 2.96 does not implicitly add const through a pointer to a typedef'd
       array; the cast changes only the read-only view of the same matrix. */
    MMathApplyMatrix(&work->position, (const Matrix4 *)&actorMatrix, &work->creationOffset);
    work->position.w = 1.0f;
    MEfGetActorCoord(&work->actorCoordinates, work->coordinateActorId, -1);
    memset(work->visibility, 0, sizeof(work->visibility));
    memset(work->trail, 0, sizeof(work->trail));
    work->visibility[0].flag[0] = 1;
    /* Quadword copy: the first trail point starts at the current position. */
#ifndef PORT_HOST
    PS2_ASMV(
        "lq $8,0(%1)\n\t"
        "sq $8,0(%0)\n\t"
        :
        : "r"(work->trail[0].point), "r"(&work->position)
        : "$8", "memory");
#else
    {
        XsgGpr r_1 = XSG_IN(&work->position);
        XsgGpr r_0 = XSG_IN(work->trail[0].point);
        __builtin_memcpy(&xsg_ee_gpr[8], XSG_EA(r_1, 0), 16);
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[8], 16);
        xsg_vu0_blocks++;
    }
#endif
    MMathCalcVectorMatrix(&direction, (const Matrix4 *)&actorMatrix);
    /* vectorOrigin[0].xyz = direction.xyz * -0.65, the first strand's
       per-frame step back along the actor's facing. */
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 $vf1,0(%1)\n\t"
        "mfc1 $8,%2\n\t"
        "qmtc2 $8,$vf2\n\t"
        "vmulx.xyz $vf1xyz,$vf1xyz,$vf2x\n\t"
        "sqc2 $vf1,0(%0)\n\t"
        :
        : "r"(&work->vectorOrigin[0]), "r"(&direction), "f"(-0.65f)
        : "$8", "memory");
#else
    {
        XsgFpr f_2 = XSG_FIN(-0.65f);
        XsgGpr r_1 = XSG_IN(&direction);
        XsgGpr r_0 = XSG_IN(&work->vectorOrigin[0]);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        xsg_ee_set32(&xsg_ee_gpr[8], f_2.u);
        vu0_qmtc2(2, &xsg_ee_gpr[8]);
        vu0_arith(VU0_MUL, 14, 1, 1, 2, 0);
        vu0_sqc2(1, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
    MGsGPInit(work->packet, 0, 0);

    object->processCallback = fnBP00_PR000;
    object->drawCallback = fnBP00_DP000;
    object->postCallback = fnBP00_PO000;
    return 1;
}

extern float MMathMakeRandom2PI(void);
extern float MMathMakeRandom(void);
extern Vector4 *MMathDivVectorS(Vector4 *destination, const Vector4 *source, float scale);
static __inline__ void bp00AddVectorXYZ(Vector4 *destination, const Vector4 *left, const Vector4 *right)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 $vf1,0(%1)\n\t"
        "lqc2 $vf2,0(%2)\n\t"
        "vadd.xyz $vf1xyz,$vf1xyz,$vf2xyz\n\t"
        "sqc2 $vf1,0(%0)\n\t"
        :
        : "r"(destination), "r"(left), "r"(right)
        : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(right);
        XsgGpr r_1 = XSG_IN(left);
        XsgGpr r_0 = XSG_IN(destination);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_ADD, 14, 1, 1, 2, VU0_VEC);
        vu0_sqc2(1, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
}
static __inline__ void bp00SubVectorXYZ(Vector4 *destination, const Vector4 *left, const Vector4 *right)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 $vf1,0(%1)\n\t"
        "lqc2 $vf2,0(%2)\n\t"
        "vsub.xyz $vf1xyz,$vf1xyz,$vf2xyz\n\t"
        "sqc2 $vf1,0(%0)\n\t"
        :
        : "r"(destination), "r"(left), "r"(right)
        : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(right);
        XsgGpr r_1 = XSG_IN(left);
        XsgGpr r_0 = XSG_IN(destination);
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_lqc2(2, XSG_EA(r_2, 0));
        vu0_arith(VU0_SUB, 14, 1, 1, 2, VU0_VEC);
        vu0_sqc2(1, XSG_EA(r_0, 0));
        xsg_vu0_blocks++;
    }
#endif
}
static __inline__ void bp00CopyVector(Vector4 *destination, const Vector4 *source)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lq $8,0(%1)\n\t"
        "sq $8,0(%0)\n\t"
        :
        : "r"(destination), "r"(source)
        : "$8", "memory");
#else
    {
        XsgGpr r_1 = XSG_IN(source);
        XsgGpr r_0 = XSG_IN(destination);
        __builtin_memcpy(&xsg_ee_gpr[8], XSG_EA(r_1, 0), 16);
        __builtin_memcpy(XSG_EA(r_0, 0), &xsg_ee_gpr[8], 16);
        xsg_vu0_blocks++;
    }
#endif
}
static __inline__ float bp00Cos(float angle)
{
    float cosine;

#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $8,%1\n\t"
        "qmtc2.ni $8,$vf4\n\t"
        "vcallms 0xe8\n\t"
        "qmfc2.i $8,$vf1\n\t"
        "mtc1 $8,%0\n\t"
        : "=f"(cosine)
        : "f"(angle)
        : "$8", "memory");
#else
    {
        XsgFpr f_1 = XSG_FIN(angle);
        XsgFpr f_0 = {0};
        xsg_ee_set32(&xsg_ee_gpr[8], f_1.u);
        vu0_qmtc2(4, &xsg_ee_gpr[8]);
        vu0_callms(232);
        vu0_qmfc2(&xsg_ee_gpr[8], 1);
        f_0.u = xsg_ee_gpr[8].w[0];
        xsg_vu0_blocks++;
        cosine = f_0.f;
    }
#endif
    return cosine;
}
static __inline__ float bp00Sin(float angle)
{
    float sine;

#ifndef PORT_HOST
    PS2_ASMV(
        "mfc1 $8,%1\n\t"
        "qmtc2.ni $8,$vf4\n\t"
        "vcallms 0x20\n\t"
        "qmfc2.i $8,$vf1\n\t"
        "mtc1 $8,%0\n\t"
        : "=f"(sine)
        : "f"(angle)
        : "$8", "memory");
#else
    {
        XsgFpr f_1 = XSG_FIN(angle);
        XsgFpr f_0 = {0};
        xsg_ee_set32(&xsg_ee_gpr[8], f_1.u);
        vu0_qmtc2(4, &xsg_ee_gpr[8]);
        vu0_callms(32);
        vu0_qmfc2(&xsg_ee_gpr[8], 1);
        f_0.u = xsg_ee_gpr[8].w[0];
        xsg_vu0_blocks++;
        sine = f_0.f;
    }
#endif
    return sine;
}
static void fnBP00_PR010(void *self, BP00Work *work);

static void fnBP00_PR000(void *self, void *data)
{
    BP00Object *object = (BP00Object *)self;
    BP00Work *work = (BP00Work *)data;
    Vector4 origin;
    Vector4 target;
    Vector4 step;
    float angle;
    float spread;
    int i;

    /* Shift the first strand's history one point back. */
    for (i = 5; i >= 0; i--) {
        work->visibility[0].flag[i + 1] = work->visibility[0].flag[i];
        work->trail[0].point[i + 1] = work->trail[0].point[i];
    }
    work->visibility[0].flag[0] = 0;
    if (work->counter < 18) {
        work->visibility[0].flag[0] = 1;
        bp00AddVectorXYZ(work->trail[0].point, work->trail[0].point, &work->vectorOrigin[0]);
    }
    if (work->counter >= 24) {
        work->counter = 0;
        bp00CopyVector(&origin, work->trail[0].point);
        for (i = 0; i < 5; i++) {
            Vector4 *head;

            work->visibility[i].flag[0] = 1;
            head = work->trail[i].point;
            bp00CopyVector(head, &origin);
            angle = MMathMakeRandom2PI();
            head->x += bp00Cos(angle);
            head->z += bp00Sin(angle);
            bp00CopyVector(&target, &work->actorCoordinates);
            spread = MMathMakeRandom() * 5.0f + 1.0f;
            target.x += spread * bp00Cos(angle);
            target.z += spread * bp00Sin(angle);
            bp00SubVectorXYZ(&step, &target, head);
            MMathDivVectorS(&work->vectorOrigin[i], &step, 9.0f);
        }
        object->processCallback = fnBP00_PR010;
    }
}

static inline void fnBP00_vaddxyz(Vector4 *src, Vector4 *dst)
{
#ifndef PORT_HOST
    PS2_ASMV(
        "lqc2 $vf1, 0(%0)\n\t"
        "lqc2 $vf2, 0(%1)\n\t"
        "vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz\n\t"
        "sqc2 $vf1, 0(%2)"
        : : "r"(dst), "r"(src), "r"(dst) : "memory");
#else
    {
        XsgGpr r_2 = XSG_IN(dst);
        XsgGpr r_1 = XSG_IN(src);
        XsgGpr r_0 = XSG_IN(dst);
        vu0_lqc2(1, XSG_EA(r_0, 0));
        vu0_lqc2(2, XSG_EA(r_1, 0));
        vu0_arith(VU0_ADD, 14, 1, 1, 2, VU0_VEC);
        vu0_sqc2(1, XSG_EA(r_2, 0));
        xsg_vu0_blocks++;
    }
#endif
}

static void fnBP00_PR010(void *self, BP00Work *work)
{
    int i;
    int j;

    for (i = 0; i < 5; i++) {
        if (i < work->counter) {
            for (j = 5; j >= 0; j--) {
                work->visibility[i].flag[j + 1] = work->visibility[i].flag[j];
                work->trail[i].point[j + 1] = work->trail[i].point[j];
            }
            work->visibility[i].flag[0] = 0;
        }
    }
    if (work->counter < 16) {
        for (i = 0; i < 5; i++) {
            if (i < work->counter) {
                Vector4 *point;
                work->visibility[i].flag[0] = 1;
                point = (Vector4 *)((char *)(i * 7 * 16) + (int)work->trail);
                fnBP00_vaddxyz(&work->vectorOrigin[i], point);
            }
        }
    }
}

PS2_TI_TYPEDEF(unsigned int, BP00Quad);
typedef union BP00QW {
    BP00Quad q;
    unsigned PS2_LONG ul[2];
    unsigned short us[8];
    unsigned int ui[4];
    unsigned char uc[16];
    volatile unsigned PS2_LONG vul[2];
    volatile unsigned char vuc[16];
} BP00QW;
typedef struct BP00GPacket {
    void *data;
    int capacity;
    BP00QW *current;
    int count;
    int auxCount;
} BP00GPacket;
typedef struct BP00CamView {
    void *screen;
    void *matrix;
} BP00CamView;
extern BP00CamView mefCamParams;
extern BP00QW coltbl_0_00A51460;
extern int MMathRotTransPersClip(BP00QW *destination, const void *screen, const void *matrix, void *point);
extern void MGsGPTerm(void *packet);
static inline void bp00Flush(BP00GPacket *pk, int nreg)
{
    int m = pk->auxCount * nreg + 1;

    pk->current->us[0] |= nreg;
    pk->current += m;
    pk->count += m;
}
static inline BP00QW *bp00Begin(BP00GPacket *gp, unsigned PS2_LONG tag, unsigned PS2_LONG regs, int aux)
{
    BP00QW *t = gp->current;
    BP00QW *r = t + 1;

    t[0].ul[1] = regs;
    t[0].ul[0] = tag | ((unsigned PS2_LONG)aux << 60);
    gp->auxCount = aux;
    return r;
}

static void fnBP00_DP000(void *self, void *work)
{
  BP00Work *w = (BP00Work *) work;
  BP00QW out;
  BP00QW unused;
  unsigned int new_var;
  BP00QW col;
  void *screen;
  void *matrix;
  BP00GPacket *pk;
  BP00QW *q;
  BP00QW *a2;
  BP00QW *a3;
  BP00QW *dst;
  int k;
  int i;
  int n;
  int clip;
  int r;
  int g;
  int b;
  int t;
  screen = mefCamParams.screen;
  matrix = mefCamParams.matrix;
  pk = (BP00GPacket *) w->packet;
  MGsGPInit(pk, 0, 0);
  q = pk->current;
  q[0].vul[0] = 0x1000000000008000;
  q[0].vul[1] = 0xE;
  pk->auxCount = 1;
  dst = q + 1;
  a2 = q + 2;
  a3 = q + 3;
  dst->vuc[8] = 0x47;
  dst->vul[0] = 0x5000D;
  new_var = 0x4E;
  a2->vul[0] = 0x131000000;
  a2->uc[8] = new_var;
  a3->vuc[8] = 0x42;
  a3->vul[0] = 0x48;
  bp00Flush(pk, 3);
  for (k = 0; k < 5; k++)
  {
    col.q = coltbl_0_00A51460.q;
    n = 0;
    clip = 0;
    dst = bp00Begin((BP00GPacket *) w->packet, 0x0025400000008000, 0x51, 2);
    for (i = 0; i < 7; i++)
    {
      if (((short) w->visibility[k].flag[i]) != 0)
      {
        t = 0x30 - (i * 0x18);
        r = 0;
        if (t >= 0)
        {
          r = t;
        }
        t = 0x60 - (i * 0x14);
        g = 0;
        if (t >= 0)
        {
          g = t;
        }
        t = 0xFF - (i * 0x10);
        b = 0;
        if (t >= 0)
        {
          b = t;
        }
        col.ui[0] = r;
        col.ui[1] = g;
        col.ui[2] = b;
        if (i == 0)
        {
          col.ui[0] = ((unsigned int) r) >> 1;
          /* Shift count stored through col.ui[1] first: needed for the match (register allocation). */
          col.ui[1] = 1;
          col.ui[1] = ((unsigned int) g) >> col.ui[1];
          col.ui[2] = ((unsigned int) b) >> 1;
        }
        if (MMathRotTransPersClip(&out, screen, matrix, &w->trail[k].point[i]))
        {
          clip = 1;
        }
        else
          if (clip != 0)
        {
          clip--;
          out.ui[3] = 0x8000;
        }
        n++;
        dst[0].q = col.q;
        dst[1].q = out.q;
        dst += 2;
      }
    }

    if (n > 0)
    {
      bp00Flush(pk, n);
    }
  }

  MGsGPTerm(pk);
}

/* MEfObjDestroy (src/main/m_ef_obj.c) and sefHitEffect (src/main/sef.c) are
 * still asm in their defining TU; declared locally until published there. */
extern void MEfObjDestroy(void *self);
extern void sefHitEffect(void);

/* work's layout is unresolved beyond +0x70: the per-frame lifetime counter
 * this function advances, firing sefHitEffect at frame 0x21 and expiring
 * through MEfObjDestroy once it reaches 0x2D. counter, right after it, is
 * only ever incremented here in lockstep with frame; nothing recovered in
 * this TU reads it back, so its specific role (an "age" naming borrowed
 * from the unrelated SolbState.age, src/ov01/m_ef_create_solb.h) is not
 * evidenced here and is left neutral. */
#define BP00_HIT_FRAME 0x21
#define BP00_LIFETIME  0x2D

typedef struct BP00State {
    unsigned char unmodeled_00[0x70];
    int frame;   /* +0x70 */
    int counter; /* +0x74 */
} BP00State;

static void fnBP00_PO000(void *self, void *work)
{
    BP00State *state = (BP00State *)work;

    state->frame++;
    state->counter++;
    if (state->frame == BP00_HIT_FRAME) {
        sefHitEffect();
    }
    if (state->frame >= BP00_LIFETIME) {
        MEfObjDestroy(self);
    }
}

/*
 * OV01 original TU 25: 0x00a34e90..0x00a358a0 (7 functions)
 */
#include "common.h"
#include "shared.h"
#include "m_ef_create_smp_01.h"

typedef struct Smp01GsParameter {
    unsigned long primitive_tag;
    unsigned char rest[8];
} Smp01GsParameter;
typedef struct Smp01CreateWork {
    unsigned char unmodeled_00[8];
    unsigned int source_actor;          /* 0x08 */
    unsigned int source_part;           /* 0x0C */
    Vector4 source_offset;              /* 0x10 */
    unsigned char unmodeled_20[4];
    unsigned int coordinate_actor;      /* 0x24 */
    unsigned char unmodeled_28[0x70 - 0x28];
    int frame;                          /* 0x70 */
    int phase;                          /* 0x74 */
    int phaseCount;                     /* 0x78 */
    unsigned char unmodeled_7C[4];
    Vector4 spawnPoint;                 /* 0x80 */
    Vector4 phaseStart;                 /* 0x90 */
    Vector4 targetPosition;             /* 0xA0 */
    Vector4 position;                   /* 0xB0 */
    Vector4 previousPosition;           /* 0xC0 */
    Vector4 angles;                     /* 0xD0 */
    float phaseAngle;                   /* 0xE0 */
    unsigned char packet[0x14];         /* 0xE4 */
    unsigned long parameter_first;      /* 0xF8 */
    unsigned long parameter_second;     /* 0x100 */
    unsigned char active[0x14];         /* 0x108 */
    unsigned char unmodeled_11C[4];
    unsigned char points[0x1E0];        /* 0x120 */
    unsigned char twist[0x78];          /* 0x300 */
} Smp01CreateWork;
typedef struct Smp01CreateObject {
    unsigned char unmodeled_00[4];
    void *process;                      /* 0x04 */
    void *draw;                         /* 0x08 */
    void *draw_packet;                  /* 0x0C */
    void *lifetime;                     /* 0x10 */
    unsigned char unmodeled_14[0x0C];
    Smp01CreateWork work;               /* 0x20 */
} Smp01CreateObject;
extern void MEfGetActorMatrix(void *destination, u32 actor, u32 part);
extern void MEfGetActorCoord(void *destination, u32 actor, u32 part);
extern Vector4 *MEfCalcCircumXZ(void *destination, const void *center, float radius);
extern void *MMathApplyMatrix(Vector4 *destination, const Vector4 *matrix,
                              const Vector4 *point);
extern void *MEfCalcAngle(Vector4 *destination, const Vector4 *from, const Vector4 *to);
extern float MMathMakeRandom2PI(void);
extern void MGsGPInit(void *packet, void *address, int size);
extern Smp01GsParameter *svGetPrmFromName(const char *name, Smp01GsParameter *output);
extern void *memset(void *destination, int value, unsigned int size);
extern const char D_00A51480[];
extern const char D_00A51490[];
static void fnSMP01_PR000(Smp01Object *object, Smp01Work *work);
static void fnSMP01_DM000(Smp01Object *self, Smp01Work *work);
static void fnSMP01_PO000(void *object, void *work);
extern void fnSMP01_DP000();

int MEfCreate_SMP01(Smp01CreateObject *object)
{
    Smp01CreateWork *work = &object->work;
    Smp01GsParameter parameter;
    Vector4 actor_coordinate[1];
    Vector4 actor_matrix[4];

    work->frame = 0;
    work->phase = 0;
    work->phaseCount = 0;
    MEfGetActorMatrix(actor_matrix, work->source_actor, work->source_part);
    MMathApplyMatrix(&work->spawnPoint, actor_matrix, &work->source_offset);
    MEfGetActorCoord(actor_coordinate, work->coordinate_actor, -1);
    MEfCalcCircumXZ(&work->targetPosition, actor_coordinate, 1.0f);
    __asm__ __volatile__("lq $8,0(%1)\n\tsq $8,0(%0)"
                         : : "r"(&work->position), "r"(&work->spawnPoint)
                         : "$8", "memory");
    __asm__ __volatile__("lq $8,0(%1)\n\tsq $8,0(%0)"
                         : : "r"(&work->previousPosition), "r"(&work->spawnPoint)
                         : "$8", "memory");
    MEfCalcAngle(&work->angles, &work->targetPosition, &work->spawnPoint);
    work->phaseAngle = MMathMakeRandom2PI();
    MGsGPInit(work->packet, 0, 0);
    svGetPrmFromName(D_00A51480, &parameter);
    work->parameter_first = parameter.primitive_tag;
    svGetPrmFromName(D_00A51490, &parameter);
    work->parameter_second = parameter.primitive_tag;
    memset(work->active, 0, sizeof(work->active));
    memset(work->points, 0, sizeof(work->points));
    memset(work->twist, 0, sizeof(work->twist));

    object->process = fnSMP01_PR000;
    object->draw = fnSMP01_DM000;
    object->draw_packet = fnSMP01_DP000;
    object->lifetime = fnSMP01_PO000;
    return 1;
}

extern float MMathMakeRandom(void);
extern Vector4 *MMathVectorInterpolation(Vector4 *destination, const Vector4 *first,
                                         const Vector4 *second, float parameter);
extern void MMathRotateMatrixYX(Vector4 *destination, const Vector4 *source,
                               const Vector4 *angles);
typedef struct Smp01SmokeWork {
    unsigned char head[0x108];         /* 0x000 */
    short count[10];                   /* 0x108 */
    unsigned char unmodeled_11C[4];    /* 0x11C */
    Vector4 smoke[10][3];              /* 0x120 */
    float rotation[10][3];             /* 0x300 */
} Smp01SmokeWork;

static void updateSmoke(Smp01Work *work)
{
    Smp01SmokeWork *smoke = (Smp01SmokeWork *)work;
    Vector4 matrix[4];
    Vector4 offset;
    int i;
    int j;

    for (i = 8; i >= 0; i--) {
        smoke->count[i + 1] = smoke->count[i];
        for (j = 0; j < 3; j++) {
            __asm__ __volatile__("lq $8, 0(%1)\n\t"
                                 "sq $8, 0(%0)"
                                 :
                                 : "r"(&smoke->smoke[i + 1][j]), "r"(&smoke->smoke[i][j])
                                 : "$8", "memory");
            smoke->rotation[i + 1][j] = smoke->rotation[i][j];
        }
    }
    smoke->count[0] = 0;
    for (j = 0; j < 3; j++) {
        __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&smoke->smoke[0][j]) : "memory");
    }
    if (work->frame < 15) {
        smoke->count[0] = 1;
        for (j = 0; j < 3; j++) {
            MMathVectorInterpolation(&offset, &work->position, &work->previousPosition,
                                     (float)j / 3.0f);
            MMathRotateMatrixYX(matrix, 0, &work->angles);
            __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&offset) : "memory");
            __asm__ __volatile__("vmove.w vf1, vf0" : : : "memory");
            __asm__ __volatile__("sqc2 vf1, 48(%0)" : : "r"(matrix) : "memory");
            __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&offset) : "memory");
            offset.x = MMathMakeRandom() * 0.06f - 0.03f;
            offset.y = MMathMakeRandom() * 0.06f - 0.03f;
            MMathApplyMatrix(&smoke->smoke[0][j], matrix, &offset);
            smoke->rotation[0][j] = MMathMakeRandom2PI();
        }
    }
}

extern void *MEfCalcAngle(Vector4 *destination, const Vector4 *from,
                          const Vector4 *to);
extern void *MMathApplyMatrix(Vector4 *destination, const Vector4 *matrix,
                              const Vector4 *point);
extern void MMathRotateMatrixYX(Vector4 *destination, const Vector4 *source,
                               const Vector4 *angles);
static void updateSmoke(Smp01Work *work);

static void fnSMP01_PR010(Smp01Object *object, Smp01Work *work);

static void fnSMP01_PR000(Smp01Object *object, Smp01Work *work)
{
    Vector4 offset;
    Vector4 matrix[4];
    int phase;
    float sine;
    float cosine;

    phase = work->phase;
    work->phase = phase + 1;
    work->phaseCount = work->phaseCount - phase + 5;

    __asm__ __volatile__("lq $8, 0(%1)\n\t"
                         "sq $8, 0(%0)"
                         :
                         : "r"(&work->previousPosition), "r"(&work->position)
                         : "$8", "memory");
    __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&offset) : "memory");

    __asm__ __volatile__("mfc1 $8, %1\n\t"
                         "qmtc2.ni $8, vf4\n\t"
                         "vcallms 0x20\n\t"
                         "qmfc2.i $8, vf1\n\t"
                         "mtc1 $8, %0"
                         : "=f"(sine)
                         : "f"(work->phaseAngle)
                         : "$8", "memory");
    offset.x = (float)work->phase * 0.06f * sine;

    __asm__ __volatile__("mfc1 $8, %1\n\t"
                         "qmtc2.ni $8, vf4\n\t"
                         "vcallms 0xe8\n\t"
                         "qmfc2.i $8, vf1\n\t"
                         "mtc1 $8, %0"
                         : "=f"(cosine)
                         : "f"(work->phaseAngle)
                         : "$8", "memory");
    offset.y = (float)work->phase * 0.06f * cosine;
    offset.z = (float)work->phaseCount * -0.02f;

    MMathRotateMatrixYX(matrix, 0, &work->angles);
    {
        /* Keep the translation load's address in v1, as at 0x00a352ec. */
        register Vector4 *spawnPoint asm("$3") = &work->spawnPoint;

        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(spawnPoint) : "memory");
    }
    __asm__ __volatile__("vmove.w vf1, vf0" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 48(%0)" : : "r"(matrix) : "memory");
    MMathApplyMatrix(&work->position, matrix, &offset);

    if (work->phase >= 4) {
        updateSmoke(work);
    }

    if (work->phase >= 5) {
        work->phase = 0;
        __asm__ __volatile__("lq $8, 0(%1)\n\t"
                             "sq $8, 0(%0)"
                             :
                             : "r"(&work->phaseStart), "r"(&work->position)
                             : "$8", "memory");
        MEfCalcAngle(&work->angles, &work->targetPosition, &work->phaseStart);
        object->updateCallback = fnSMP01_PR010;
    }
}

enum { SMP01_DRAW_FRAME_LIMIT = 15 };

extern Vector4 *MMathVectorInterpolation(Vector4 *destination, const Vector4 *first,
                                         const Vector4 *second, float parameter);

static void fnSMP01_PR010(Smp01Object *object, Smp01Work *work)
{
    (void)object;
    if (work->frame < SMP01_DRAW_FRAME_LIMIT) {
        float phase;

        __asm__ __volatile__("lq $8, 0(%1)\n\t"
                             "sq $8, 0(%0)"
                             :
                             : "r"(&work->previousPosition), "r"(&work->position)
                             : "$8", "memory");
        phase = ((float)(work->phase + 1) / 10.0f) * 1.5707964f + 4.712389f;
        __asm__ __volatile__("mfc1 $8, %0\n\t"
                             "qmtc2.ni $8, vf4\n\t"
                             "vcallms 0x20\n\t"
                             "qmfc2.i $8, vf1\n\t"
                             "mtc1 $8, %0"
                             : "+f"(phase)
                             :
                             : "$8", "memory");
        MMathVectorInterpolation(&work->position, &work->phaseStart,
                                 &work->targetPosition, phase + 1.0f);
        work->angles.z += 0.5235988f;
    }
    updateSmoke(work);
    work->phase++;
}

extern Vector4 *MMathRotateMatrixYXZ(Vector4 *out, const Vector4 *matrix, const Vector4 *angles);
extern Vector4 *MMathScaleMatrix(Vector4 *out, const Vector4 *matrix, const Vector4 *scale);
extern void MEfDrawModel(const Vector4 *place, int entry, const char *texture);
extern Vector4 scale_0_00A514A0;

static void fnSMP01_DM000(Smp01Object *self, Smp01Work *work)
{
    Vector4 matrix[4];

    (void)self;
    if (work->frame < SMP01_DRAW_FRAME_LIMIT) {
        MMathRotateMatrixYXZ(matrix, 0, &work->angles);
        MMathScaleMatrix(matrix, matrix, &scale_0_00A514A0);
        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&work->position) : "memory");
        __asm__ __volatile__("vmove.w vf1, vf0" : : : "memory");
        __asm__ __volatile__("sqc2 vf1, 48(%0)" : : "r"(matrix) : "memory");
        MEfDrawModel(matrix, work->modelEntry, work->textureName);
    }
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct Smp01Packet {
    void *data;    /* 0x00 */
    int capacity;  /* 0x04 */
    void *current; /* 0x08 */
    int count;     /* 0x0C */
    int auxCount;  /* 0x10 */
} Smp01Packet;

typedef struct Smp01ImageParameter {
    u64 texture;
    float width;
    float height;
} Smp01ImageParameter;

typedef unsigned int Smp01xQuad __attribute__((mode(TI)));
typedef union Smp01xQW {
    Smp01xQuad q;
    unsigned long ul[2];
    unsigned short us[8];
    unsigned char uc[16];
    volatile unsigned long vul[2];
    volatile unsigned char vuc[16];
} Smp01xQW;
extern int MEfMake4Vertex2(Vector4 *dst, const Vector4 *source, float size, float angleBase);
extern void MGsGPTerm(void *packet);
extern Smp01xQuad McEfSTQtbl[];
extern Smp01xQuad coltbl_1_00A514B0[];
extern float sztbl_2_00A51550[];
typedef struct Smp01xPacket {
    void *data;
    int capacity;
    Smp01xQW *current;
    int count;
    int auxCount;
} Smp01xPacket;
#define SMP01X_AD(P, REG, VAL) \
    { Smp01xQW *_ad = (P); _ad->uc[8] = (REG); _ad->ul[0] = (VAL); }
typedef struct Smp01Sub {
    unsigned char unmodeled_00[0x100];
    unsigned short active[10];          /* 0x108 - 8 */
} Smp01Sub;
typedef struct Smp01Draw {
    unsigned char unmodeled_00[0xE4];
    unsigned char drawPacket[0x14];     /* 0xE4 */
    unsigned char unmodeled_f8[0x10];
    unsigned short active[10];          /* 0x108 */
    unsigned char unmodeled_11c[4];
    Vector4 points[10][3];              /* 0x120 */
    float twist[10][3];                 /* 0x300 */
} Smp01Draw;

static void fnSMP01_DP000(void *task, Smp01Draw *effect)
{
  int i;
  long regs;
  int nreg;
  float *szp;
  Vector4 verts[4];
  Smp01xPacket *pk;
  Smp01xPacket *gp;
  Smp01xQW *q;
  Smp01xQW *a1;
  Smp01xQW *a2;
  Smp01xQW *a3;
  Smp01xQW *a4;
  Smp01xQW *a5;
  Smp01xQuad color;
  unsigned long *tex;
  int j;
  int n;
  int off;
  int lo;
  int nloop;
  int texReg;
  Smp01Sub *sub;
  ;
  pk = (Smp01xPacket *) effect->drawPacket;
  MGsGPInit(pk, 0, 0);
  q = pk->current;
  q[0].vul[0] = 0x1000000000008000;
  q[0].vul[1] = 0xE;
  pk->auxCount = 1;
  a1 = q + 1;
  a2 = q + 2;
  a3 = q + 3;
  a4 = q + 4;
  a5 = q + 5;
  a1->vuc[8] = 0x47;
  a1->vul[0] = 0x5000D;
  a2->vuc[8] = 0x14;
  a2->vul[0] = 0x60;
  a3->vuc[8] = 0x8;
  a3->vul[0] = 5;
  a4->vuc[8] = 0x4E;
  a4->vul[0] = 0x131000000;
  a5->vuc[8] = 0x42;
  a5->vul[0] = 0x48;
  nreg = 5;
  n = (pk->auxCount * nreg) + 1;
  pk->current->us[0] |= nreg;
  pk->current += n;
  pk->count += n;
  for (i = 0; i < 10; i++)
  {
    sub = (Smp01Sub *)((char *)effect + 8);
    if (((short *)sub->active)[i] != 0)
    {
      color = coltbl_1_00A514B0[i];
      lo = i < 5;
      off = 0xF8;
      if (lo)
      {
        off = 0xF0;
      }
      tex = (unsigned long *) (((char *) sub) + off);
      for (j = 0; j < 3; j++)
      {
        if (MEfMake4Vertex2(verts, &effect->points[i][j], sztbl_2_00A51550[i], effect->twist[i][j]) == 0)
        {
          Smp01xQW *t;
          Smp01xQW *g;
          unsigned long texVal;
          int n1;
          int n2;
          gp = (Smp01xPacket *) effect->drawPacket;
          texReg = 6;
          regs = 0x512;
          nloop = 3;
          t = gp->current;
          t[0].ul[0] = 0x1000000000008000;
          t[0].ul[1] = 0xE;
          gp->auxCount = 1;
          a1 = t + 1;
          texVal = *tex;
          a1->uc[8] = texReg;
          a1->ul[0] = texVal;
          n1 = gp->auxCount + 1;
          gp->current->us[0] |= 1;
          gp->current += n1;
          gp->count += n1;
          g = gp->current;
          g[0].ul[0] = 0x302A400000008000;
          g[0].ul[1] = regs;
          gp->auxCount = nloop;
          g[1].q = McEfSTQtbl[0];
          g[2].q = color;
          g[3].q = *((Smp01xQuad *) (&verts[0]));
          g[4].q = McEfSTQtbl[1];
          g[5].q = color;
          g[6].q = *((Smp01xQuad *) (&verts[1]));
          g[7].q = McEfSTQtbl[2];
          g[8].q = color;
          g[9].q = *((Smp01xQuad *) (&verts[2]));
          g[10].q = McEfSTQtbl[3];
          g[11].q = color;
          g[12].q = *((Smp01xQuad *) (&verts[3]));
          n2 = ((gp->auxCount * 2) * 2) + 1;
          gp->current->us[0] |= 4;
          gp->current += n2;
          gp->count += n2;
        }
      }
__asm__("");__asm__("");__asm__("");__asm__("");__asm__("");__asm__("");__asm__("");__asm__("");__asm__("");__asm__("");__asm__("");
    }
  }

  MGsGPTerm(pk);
}

extern void MEfObjDestroy(void *object);
extern void sefHitEffect(void);

static void fnSMP01_PO000(void *object, void *work)
{
    Smp01State *state = (Smp01State *)work;

    state->frame++;
    if (state->frame == 15) {
        sefHitEffect();
    }
    if (state->frame >= 25) {
        MEfObjDestroy(object);
    }
}

#include "common.h"
#include "shared.h"

#define ID_LIGHT_COUNT 16

/* Scratchpad RAM base; the terrain query block is built there. */
#define SCRATCHPAD_BASE 0x70000000

/* UnduDataGetHeader group holding the light-id floor attributes, and the unit
 * asked for (the map default). */
#define UNDU_HEADER_ID_LIGHT 0x306
#define UNDU_UNIT_DEFAULT 0x8000

/* Scratchpad layout of the terrain query: the block, then its scratch area. */
#define ID_LIGHT_QUERY_SCRATCH_OFFSET 0x40

/* Ground attribute word: bits 13-15 are the attribute type (3 = light), bits
 * 8-12 the light id. */
#define ATTR_TYPE_MASK 0xE000
#define ATTR_TYPE_LIGHT 0x6000
#define ATTR_ID_MASK 0x1F00
#define ATTR_ID_SHIFT 8

/* IdLightActor.flags bit set whenever the actor's light changes. */
#define ID_LIGHT_FLAG_CHANGED 0x8000

/* Frames an actor's light blends towards a new light id. */
#define ID_LIGHT_BLEND_STEPS 4

/* 16-byte copy through the R5900 128-bit GPRs (sceVu0CopyVector idiom). */
#define QuadwordCopy(destination, source) \
    __asm__ __volatile__( \
        "lq $2, 0(%1)\n" \
        "sq $2, 0(%0)\n" \
        : \
        : "r" (destination), "r" (source) \
        : "$2", "memory")

typedef unsigned int IdLightQuadword __attribute__((mode(TI)));

typedef struct IdLightPair {
    IdLightQuadword color;
    IdLightQuadword direction;
} IdLightPair;

/* A studio light block (0xF0 bytes): ambient, then colour/direction pairs. */
typedef struct IdLightSet {
    IdLightQuadword ambientColor;
    IdLightPair lights[3];
    IdLightQuadword unmodeled_70[8];
} IdLightSet;

typedef struct LayoutHeader LayoutHeader;

/* The actor fields GameIdLightSet touches. */
typedef struct IdLightActor {
    unsigned int flags;                    /* +0x00, bit 0x8000 = light changed */
    unsigned char unmodeled_04[0x10 - 0x04];
    Vector4 position;                      /* +0x10 */
    unsigned char unmodeled_20[0x92 - 0x20];
    unsigned char lightId;                 /* +0x92, 0 = studio light */
    unsigned char blendSteps;              /* +0x93, steps left to the target */
    unsigned char unmodeled_94[0x510 - 0x94];
    IdLightSet light;                      /* +0x510 */
} IdLightActor;

/* Terrain query block built in the scratchpad; only the fields set here. */
typedef struct IdLightQuery {
    int queryFlags;                        /* +0x00 */
    unsigned char unmodeled_04[0x08 - 0x04];
    short attrMask;                        /* +0x08 */
    unsigned char unmodeled_0a[0x18 - 0x0a];
    LayoutHeader *header;                  /* +0x18 */
    void *scratch;                         /* +0x1c */
    long long attribute;                   /* +0x20 */
} IdLightQuery;

/* The background parameter block ends in the light preset table's first slot. */
typedef struct GameBgDrawType2 {
    unsigned char unmodeled_00[0x7F0];
    IdLightSet lights[ID_LIGHT_COUNT];
} GameBgDrawType2;

extern GameBgDrawType2 GameBgDrawType2Param;
extern IdLightSet GameIdLight[];

extern LayoutHeader *UnduDataGetHeader(int map_index, int unit_index);
extern void UnduParamInit(void *param);
extern void *xglStudioGetLight2(void);
extern void UnduCheck(void *position, void *exclude, void *param);
extern float I2F(int value);

extern void xglVectorInter(IdLightQuadword *destination, IdLightQuadword *start,
                           const IdLightQuadword *end, float fraction);

void GameIdLightSet(IdLightActor *actor, IdLightQuery *query) {
    int id;
    const IdLightSet *target;
    IdLightSet *studio;
    float fraction;

    if (query == 0) {
        query = (IdLightQuery *)SCRATCHPAD_BASE;
        UnduParamInit(query);
        query->header = UnduDataGetHeader(UNDU_HEADER_ID_LIGHT, UNDU_UNIT_DEFAULT);
        query->scratch = (void *)(SCRATCHPAD_BASE + ID_LIGHT_QUERY_SCRATCH_OFFSET);
        query->attrMask = 0x810;
        UnduCheck(&actor->position, 0, (void *)SCRATCHPAD_BASE);
    }

    if ((query->attribute & ATTR_TYPE_MASK) == ATTR_TYPE_LIGHT) {
        id = (query->attribute & ATTR_ID_MASK) >> ATTR_ID_SHIFT;
    } else {
        id = 0;
    }

    if (actor->lightId != id) {
        actor->blendSteps = ID_LIGHT_BLEND_STEPS;
        if (actor->lightId == 0) {
            studio = xglStudioGetLight2();
            actor->light = *studio;
        }
        actor->lightId = id;
    }

    if (actor->blendSteps == 0) {
        if (id != 0) {
            actor->flags |= ID_LIGHT_FLAG_CHANGED;
            actor->light = GameBgDrawType2Param.lights[id];
        }
    } else {
        actor->flags |= ID_LIGHT_FLAG_CHANGED;
        if (id == 0) {
            target = xglStudioGetLight2();
        } else {
            target = &GameIdLight[id] - 1;
        }
        actor->blendSteps--;
        fraction = 1.0f / I2F(actor->blendSteps + 1);
        xglVectorInter(&actor->light.ambientColor, &actor->light.ambientColor,
                       &target->ambientColor, fraction);
        xglVectorInter(&actor->light.lights[0].color, &actor->light.lights[0].color,
                       &target->lights[0].color, fraction);
        xglVectorInter(&actor->light.lights[1].color, &actor->light.lights[1].color,
                       &target->lights[1].color, fraction);
        xglVectorInter(&actor->light.lights[2].color, &actor->light.lights[2].color,
                       &target->lights[2].color, fraction);
        QuadwordCopy(&actor->light.lights[0].direction, &target->lights[0].direction);
        QuadwordCopy(&actor->light.lights[1].direction, &target->lights[1].direction);
        QuadwordCopy(&actor->light.lights[2].direction, &target->lights[2].direction);
    }
}

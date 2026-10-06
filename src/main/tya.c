#include "common.h"
#include "shared.h"

typedef struct {
    u8 unmodeled_00[0x38];
    u64 buffer_select;
} TyaDisplayEnvironment;
extern TyaDisplayEnvironment DispEnv;
typedef struct {
    u8 unmodeled_00[0x12];
    u16 framebuffer_base_page[2];
} TyaFramebufferState;
extern TyaFramebufferState sRender;

static int getfbp(void)
{
    long long shifted_mode;
    int selector;

    shifted_mode = (long long)(DispEnv.buffer_select << 7);
    selector = shifted_mode >> 32;
    if ((selector & 3) == 1) {
        return sRender.framebuffer_base_page[1] << 5;
    }
    return sRender.framebuffer_base_page[0] << 5;
}

INCLUDE_ASM("asm/main/nonmatchings/tya", tyaBmpOutput);

INCLUDE_ASM("asm/main/nonmatchings/tya", tyaSiPicOutput);

INCLUDE_ASM("asm/main/nonmatchings/tya", tyaCaptureMain);

static void tyaCaptureNull(void)
{
}

#define ACTOR_COUNT 64
#define ACTOR_IN_USE_ID_OFFSET 0x86

typedef struct {
    u32 flags;
    u8 unmodeled_04[ACTOR_IN_USE_ID_OFFSET - 0x04];
    short inUseId;
    u8 unmodeled_88[0xA70 - (ACTOR_IN_USE_ID_OFFSET + 2)];
} ActorHead;

extern ActorHead actor[ACTOR_COUNT];

/* The scene's active capture layer: compared against a per-array base
 * (0x200 for the actor array here, 0x300 for MapUnit below) plus the slot
 * index, so only the one actor or map unit the current capture pass wants
 * is drawn. */
extern u16 layer;

extern char ACT_filterGuno[];
extern int alpha;
extern int transparency;
int F2I(float f);
void ACT_modelDraw(ActorHead *unit);

static void tyaCaptureActor(void)
{
    int i;
    ActorHead *unit;
    u8 *p;
    float f;

    unit = actor;
    i = 0;
    for (; i < ACTOR_COUNT; i++, unit++) {
        if (unit->inUseId != 0 && !(unit->flags & 8) &&
            (layer == i + 0x200 ||
             ((p = *(u8 **)((u8 *)unit + 0x8FC)) != 0 && layer == p[0x80] + 0x200))) {
            u8 *q;
            if (*(void **)((u8 *)unit + 8) == ACT_filterGuno) {
                alpha = 1;
                f = *(float *)((u8 *)unit + 0x920);
                f *= 128.0f;
                goto conv;
            }
            q = (u8 *)unit + 0x920;
            if (*(int *)(q + 0x80) & 1) {
                f = *(float *)(q + 0xA0);
                alpha = *(int *)(q + 0xB8);
            conv:
                transparency = F2I(f);
            } else {
                alpha = 0;
            }
            ACT_modelDraw(unit);
        }
    }
}

/*
 * The engine's actor record (`actor`, 64-entry array at main 0x0043c1e0,
 * 0xa70-byte stride; fuller evidence in src/main/near_dir.h,
 * src/main/enemy_2.h, src/main/set_motion.h and src/main/db_light_write.h,
 * which names the same two fields for the same reason). tyaCaptureShadow
 * only reads +0x00 flags (bits 0x8 and 0x20) and +0x86, the in-use id
 * ACT_create writes and ACT_update skips a slot on when it is zero; the
 * offsets between them are not evidenced by this TU and stay unmodeled.
 */

/* Defined in src/main/act_3.c (main/tu265), not yet published in
 * include/main/act_3.h. */
void ACT_DrawShadowBegin(void);
void ACT_DrawShadow(ActorHead *unit);
void ACT_DrawShadowEnd(void);

static void tyaCaptureShadow(void)
{
    int i;
    ActorHead *unit;

    unit = actor;
    i = 0;
    ACT_DrawShadowBegin();
    for (; i < ACTOR_COUNT; i++, unit++) {
        if (unit->inUseId != 0 && !(unit->flags & 8) &&
            layer == i + 0x200 && (unit->flags & 0x20)) {
            ACT_DrawShadow(unit);
        }
    }
    ACT_DrawShadowEnd();
}

/*
 * MapUnit[] entries are game-wide unit records (src/main/init_drill.c,
 * src/main/init_uwamono_sys.c and src/main/map_create_unit_peer.h keep
 * their own views of the same array); only the field this TU touches is
 * named. tyaCaptureUnit reads +0xA4 serial, the same field
 * src/main/init_uwamono_sys.c's UwamonoCommonUnit and
 * src/main/init_drill.c's DrillMapUnit name.
 */
#define MAP_UNIT_COUNT 64
#define MAP_UNIT_SERIAL_OFFSET 0xA4

typedef struct {
    u8 unmodeled_00[MAP_UNIT_SERIAL_OFFSET];
    short serial;
    u8 unmodeled_a6[0x300 - (MAP_UNIT_SERIAL_OFFSET + 2)];
} MapUnitHead;

extern MapUnitHead MapUnit[MAP_UNIT_COUNT];

/* Defined in src/main/map_2.c (main/tu269), not yet published in
 * include/main/map_2.h. */
void MAP_drawUnitAt(MapUnitHead *unit);

static void tyaCaptureUnit(void)
{
    int i;
    MapUnitHead *unit;

    for (i = 0, unit = MapUnit; i < MAP_UNIT_COUNT; i++, unit++) {
        if (unit->serial >= 0 && layer == i + 0x300) {
            MAP_drawUnitAt(unit);
        }
    }
}

static void tyaCaptureUnit2(void)
{
    int i;
    ActorHead *unit;
    ActorHead *found;
    MapUnitHead *mu;

    found = (ActorHead *)-1;
    for (i = 0, unit = actor; i < ACTOR_COUNT; i++, unit++) {
        if (unit->inUseId != 0 && !(unit->flags & 8) && layer == i + 0x200) {
            found = unit;
            break;
        }
    }
    for (i = MAP_UNIT_COUNT - 1, mu = MapUnit; i >= 0; i--, mu++) {
        if (mu->serial >= 0 && *(ActorHead **)((u8 *)mu + 0xFC) == found) {
            MAP_drawUnitAt(mu);
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/tya", tyaCaptureStart);

extern struct { int pad[4]; int flags; } GameLoopState;
void tyaCaptureMain(int);
unsigned short xglFontGetFlags(void);
void xglFontSetFlags(int flags);

void tyaCaptureEnd(void)
{
    tyaCaptureMain(1);
    xglFontSetFlags((unsigned short)(xglFontGetFlags() | 3));
    GameLoopState.flags &= 0xFAFFFFFF;
}

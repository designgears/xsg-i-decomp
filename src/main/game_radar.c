#include "common.h"
#include "shared.h"
#include "main/xgl_cd.h"

INCLUDE_ASM("asm/main/nonmatchings/game_radar", GameRadarDraw);

INCLUDE_ASM("asm/main/nonmatchings/game_radar", DrawRadarPing);

struct RadarActor;
int CheckActorExist(struct RadarActor *actor);
typedef struct {
    u32 flags;
    u8 pad04[0xC];
    float x;
    u8 pad14[4];
    float z;
    u8 pad1C[0x38];
    float size;
    u8 pad58[0x28];
    u8 enemyIndex;
    u8 pad81[0x45B];
    float f4DC;
    u8 pad4E0[0xA70 - 0x4E0];
} RadarInfoActor;
typedef struct {
    u8 pad00[0x48];
    signed char kind;
    u8 pad49[0x37E0 - 0x49];
    u32 flags;
    u8 pad37E4[0x38B0 - 0x37E4];
} RadarInfoEnemy;
typedef struct {
    u8 pad00[0x10];
    float x;
    u8 pad14[4];
    float z;
    u8 pad1C[0x38];
    float size;
    u8 pad58[0x28];
    u8 index;
} RadarInfoPlayer;
extern RadarInfoActor actor[64];
extern RadarInfoEnemy enepc[16];
extern RadarInfoPlayer *GameLoopState[];
extern int cnt_2;
void DrawActorPoint(float size, int *pos, int *col);
static inline int radar_abs(int x)
{
    if (x < 0) {
        x = -x;
    }
    return x;
}

static void DrawRadarInfo(int *arg0)
{
    int col[4];
    int pos[4];
    RadarInfoActor *a;
    RadarInfoPlayer *pl;
    int i;
    int ox, oz;
    int dx, dz;
    int t;

    pl = GameLoopState[1];
    i = 0;
    ox = arg0[0] * 16 + 0x7498;
    oz = arg0[1] * 16 + 0x7680;
    do {
        a = &actor[i];
        if (CheckActorExist((struct RadarActor *)a) && i != pl->index) {
            dx = (a->x - pl->x) * 64.0f;
            dz = (a->z - pl->z) * 64.0f;
            if (!(a->flags & 0x80)) {
                t = __builtin_abs(dx);
                if (t <= 0x380) {
                    t = __builtin_abs(dz);
                    if (t <= 0x300) {
                        if (enepc[a->enemyIndex].flags & 1) {
                            if (a->f4DC == -1000.0f) goto next;
                            if (enepc[i].kind == 6 || enepc[i].kind == 10) {
                                col[1] = 0;
                                col[0] = cnt_2;
                                col[2] = 0;
                            } else {
                                col[0] = 0xFF;
                                col[1] = 0xFF;
                                col[2] = 0;
                            }
                        } else {
                            col[0] = 0;
                            col[1] = 0xFF;
                            col[2] = 0;
                        }
                        col[3] = 0x80;
                        pos[0] = ox + dx;
                        pos[1] = oz + dz;
                        pos[2] = arg0[2] + 2;
                        pos[3] = 0;
                        DrawActorPoint(a->size, pos, col);
                    }
                }
            }
        }
    next:
        i++;
    } while (i < 64);
    col[0] = 0xFF;
    col[3] = 0x80;
    col[1] = 0;
    col[2] = 0;
    pos[0] = ox;
    pos[1] = oz;
    pos[2] = arg0[2] + 3;
    pos[3] = 0;
    DrawActorPoint(pl->size, pos, col);
    cnt_2 = (cnt_2 + 0x10) & 0xFF;
}

INCLUDE_ASM("asm/main/nonmatchings/game_radar", DrawActorPoint);

typedef struct RadarActor {
    u32 flags;
    u8 unmodeled_04[0x82];
    short inUseId;
    u8 unmodeled_88[0x874];
    struct RadarActor *parent;
} RadarActor;

int CheckActorExist(RadarActor *actor)
{
    if (actor->inUseId <= 0) {
        return 0;
    }
    if (actor->flags & 8) {
        return 0;
    }
    return actor->parent == 0;
}

extern u8 *image_004DC554;
extern u8 rate;
extern const char D_004C0618[];

void GameRadarInit(void)
{
    image_004DC554 = (u8 *) (((int) WorkEnd + 0xF) & ~0xF);
    WorkEnd = image_004DC554 + xglCdReadFile(D_004C0618, image_004DC554, 0, 0);
    rate = 0;
}

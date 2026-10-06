#include "common.h"
#include "shared.h"

/* Partial view of the resource record RES_loadFile hands back for a map:
 * MapReset (0x00246598, still INCLUDE_ASM) dereferences it at +0x04
 * (xglCullingMapSet argument), +0x0c (UnduInit argument) and +0x10 (second
 * argument to the still-unrecovered func_A1CFB8). Only those three offsets
 * are evidenced; the rest stays unmodeled. */
typedef struct {
    u8 unmodeled_00[4];
    void *culling_map;
    u8 unmodeled_08[4];
    void *undu_state;
    int map_parameter;
} MapResource;

typedef struct {
    u8 unmodeled_00[0x10];
    int flags;
    u8 unmodeled_14[0x3c];
    short map_no;
    u8 unmodeled_52[2];
    MapResource *map_resource;
    u8 unmodeled_58[0x28];
    float scale_80;
    float scale_84;
    float scale_88;
    float scale_8c;
    u8 unmodeled_90[0x20];
    int clear_b0;
    int clear_b4;
    int fade_countdown;
    u8 unmodeled_bc[5];
    u8 control_flags;
} MapGameState;

typedef struct {
    u8 unmodeled_00[0x10];
    u8 state;
    u8 unmodeled_11[3];
} MapTaskState;

typedef void (*MapTaskCallback)(void *task);

typedef struct {
    u8 unmodeled_00[0x30];
    MapTaskCallback callback;
} MapRenderState;

extern MapGameState GameLoopState;
extern u8 UseTestPath;
extern void MapChangeFadeSet(void);
extern void MapChange2(int map_id);
extern MapResource *RES_loadFile(int file_id, int resource_type, int map_id, int flags);
extern void MapReset(void);
extern XglPacket *xglPacketGetCurrent(void);
extern void xglRenderDrawFlipPk(XglPacket *packet);
extern void nmlModelSetFadeInCancel(int model_id);
extern void nmlModelSetFadeOutCancel(int model_id);
const char D_004BE308[16] = "data\\map\\";
const char D_004BE318[16] = "data\\map\\test\\";
const char D_004BE328[16] = "data\\map\\check\\";
static void taskMapChange(void *task);
MapTaskState tsk = { { 0 }, 0, { 0, 0, 0 } };
extern MapRenderState sRender;

const char *MAP_getPath(void)
{
    int test_path = UseTestPath;

    if (test_path != 1) {
        if (test_path < 2) {
            return D_004BE308;
        }
        if (test_path != 2) {
            return D_004BE308;
        }
        return D_004BE328;
    }
    return D_004BE318;
}

extern void sceVif1PkCnt(void *packet, int count);
extern void sceVif1PkAddDataN(void *packet, const void *data, int count);
extern void nmlModelSendBackBufferSignal(void);
extern u64 TestPrim_0_00362790[];

static void taskMapChange(void *task)
{
    MapRenderState *r = &sRender;

    TestPrim_0_00362790[4] = ((u64)tsk.state << 32) | 100;
    TestPrim_0_00362790[10] = 0x24120000 | (*(u16 *)((u8 *)r + 0x14) << 5) | ((u64)0xC800 << 19);
    sceVif1PkCnt(task, 0);
    sceVif1PkAddDataN(task, TestPrim_0_00362790, 0x2C);
    nmlModelSendBackBufferSignal();
    tsk.state += 0xFC;
    if (tsk.state == 0) {
        r->callback = 0;
    }
}

void MapChangeFadeSet(void)
{
    if (GameLoopState.fade_countdown <= 0) {
        xglRenderDrawFlipPk(xglPacketGetCurrent());
        tsk.state = 0x80;
        sRender.callback = taskMapChange;
        nmlModelSetFadeInCancel(5);
        nmlModelSetFadeOutCancel(5);
    }
}

extern void func_A1CFB8(short, int);
extern void nmlModelSetMapLastInit(void);
extern void UnduInit(void *);
extern void xglCullingMapSet(void *);

void MapReset(void) {
    UnduInit(GameLoopState.map_resource->undu_state);
    GameLoopState.control_flags |= 0x80;
    ((struct { u8 pad[0x10]; unsigned lo : 4; unsigned b : 2; } *)&GameLoopState)->b = 3;
    func_A1CFB8(GameLoopState.map_no, GameLoopState.map_resource->map_parameter);
    nmlModelSetMapLastInit();
    xglCullingMapSet(GameLoopState.map_resource->culling_map);
    GameLoopState.clear_b0 = 0;
    GameLoopState.clear_b4 = 0;
    GameLoopState.scale_8c = 1.0f;
    GameLoopState.scale_88 = 1.0f;
    GameLoopState.scale_84 = 1.0f;
    GameLoopState.scale_80 = 1.0f;
}

typedef struct {
    MapResource *res;
    int pad4;
    int map_no;
    int valid;
} GameResourceEntry;
extern GameResourceEntry GameResource[];
extern float *UnduDataGetHeader(int type, int id);

void MapChangeResource(int idx, int sub)
{
    float *hdr;
    u8 *obj;

    if (GameResource[idx].valid == 1) {
        GameLoopState.map_no = GameResource[idx].map_no;
        GameLoopState.map_resource = GameResource[idx].res;
        if (idx < 1000) {
            MapChangeFadeSet();
        }
        MapReset();
        *(short *)((u8 *)&GameLoopState + 0x52) = sub;
        hdr = UnduDataGetHeader(0x201, sub + 0x8000);
        if (hdr != 0) {
            obj = *(u8 **)((u8 *)&GameLoopState + 4);
            *(float *)(obj + 0x10) = hdr[0];
            *(float *)(obj + 0x14) = hdr[1];
            *(float *)(obj + 0x18) = hdr[2];
            *(float *)(obj + 0x9E4) = *(float *)(obj + 0x54) = hdr[3];
        }
    }
}

void MapChange(int map_id)
{
    MapChangeFadeSet();
    MapChange2(map_id);
}

void MapChange2(int map_id)
{
    if ((unsigned int)map_id < 0x71e) {
        GameLoopState.map_resource = RES_loadFile(-1, 1, map_id, 0);
        GameLoopState.map_no = map_id;
        MapReset();
    }
}

typedef struct {
    char **names;
    int count;
} MapNameBlock;
extern MapNameBlock maptbl[];

char *MapGetName(int map_id)
{
    char *name;
    int id = map_id;
    int r;
    int c;

    if (id < 0) {
        id = GameLoopState.map_no;
    }
    name = 0;
    if (id < 0x71F) {
        c = maptbl[id / 100].count;
        r = id % 100;
        if (r < c) {
            name = maptbl[id / 100].names[r];
        }
    }
    return name;
}

extern void *xglStudioGetLight2(void);
extern void xglLightCalcMatrix(void *light);
extern void nmlModelSetLight(void *a, void *b);
extern void nmlModelSetTexture(void *tex);
extern void nmlModelSetMapEntry(void);
extern void nmlModelSetMulColor(float *col);
extern void nmlModelSetRenderLevel(int level);
extern void nmlModelSetPlace(void *place);
extern void nmlModelSetClip(int clip);
extern void nmlModelEntry(int entry);
typedef struct {
    void *place;
    int entry;
    void *texture;
} MapDrawResource;

void MapDraw(void)
{
    MapGameState *gs = &GameLoopState;
    MapDrawResource *res = (MapDrawResource *)gs->map_resource;
    u8 *light;

    if (*(u16 *)((u8 *)gs + 0xC) == 2 && (gs->flags & 0x20)) {
        if (gs->flags & 0x10) {
            gs->flags &= ~0x10;
        } else {
            gs->flags &= ~0x20;
        }
    }
    if (res != 0 && res->entry != 0) {
        light = xglStudioGetLight2();
        xglLightCalcMatrix(light);
        nmlModelSetLight(light + 0xB0, light + 0x70);
        if (res->texture != 0) {
            nmlModelSetTexture(res->texture);
        }
        nmlModelSetMapEntry();
        nmlModelSetMulColor(&GameLoopState.scale_80);
        nmlModelSetRenderLevel(GameLoopState.clear_b0);
        nmlModelSetPlace(res->place);
        if (GameLoopState.clear_b4 != 0) {
            nmlModelSetClip(1);
        } else {
            nmlModelSetClip(2);
        }
        nmlModelEntry(res->entry);
    }
}

int MapGetNo(void)
{
    return GameLoopState.map_no;
}

void MapInit(void)
{
    UseTestPath = 0;
}

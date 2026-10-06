#include "common.h"
#include "shared.h"

static char *command_enenpcse(int command, char *cursor, int mode);
extern void GameDiskChange(int disk_number);
extern void Intermission(int save_number);
extern int xglCdGetFileSize(const char *name);
extern int arcfilepreload;
extern char scene_txt_buffer[];
extern void SCRIPT_fade(int time);
static char *command_mpeg2_core(int command, char *cursor, int mode);
static int next_arc_size(void *address);
extern void Enemy_LoadPreset(void *address, const char *name);
extern u32 *arcfileaddr;
extern void *AdrsEnemyPreset;
#include "main/xgl_thread.h"

/* Types recovered upstream (origin/master); kept for reference. */
/* search_key walks this object in 12-byte records: it compares the first
 * string with the requested key and walks the string-pointer list at +4.
 * The third pointer is present in every record but is not read by the
 * recovered caller, so its role remains intentionally unnamed. */
typedef struct ResourceKeyEntry {
    const char *key;
    const void *aliases;
    const void *unmodeled_08;
} ResourceKeyEntry;


typedef struct {
    u16 id;
    u8 type;
    u8 foot;
    char name[16];
} EnemySeEntry;

extern EnemySeEntry EnemySeBank[8];
extern u8 ScenePath;
extern const char *tbl_0_00366420[20];

static void default_callback(void)
{
    xglSleep();
}

const char *RES_getPath(void)
{
    if (ScenePath >= 20)
        ScenePath = 0;
    return tbl_0_00366420[ScenePath];
}

static char *RES_getScenePath(char *path, int resource_id)
{
    const char *scene_path = RES_getPath();
    int scene_id;

    while ((*path = *scene_path) != 0) {
        path++;
        scene_path++;
    }

    if (resource_id <= 0x0fffffff) {
        path[0] = 'c';
        path[1] = 'f';
    } else {
        path[0] = 'e';
        path[1] = 'v';
    }

    scene_id = resource_id & 0x0fffffff;

    path[5] = (char)('0' + scene_id % 10);
    scene_id /= 10;
    path[4] = (char)('0' + scene_id % 10);
    scene_id /= 10;
    path[3] = (char)('0' + scene_id % 10);
    scene_id /= 10;
    path[2] = (char)('0' + scene_id % 10);
    path[6] = '.';
    path[7] = 't';
    path[8] = 'x';
    path[9] = 't';
    path[10] = 0;

    return path + 7;
}

int RES_GetEnemySeBank(int sound_id)
{
    int index = 0;
    int bank = 0x40000;

    for (; index < 8; index++) {
        if (EnemySeBank[index].id == sound_id)
            return bank;
        bank += 0x10000;
    }
    return -1;
}

int RES_GetEnemySeType(int sound_id)
{
    int index;

    for (index = 0; index < 8; index++) {
        if (EnemySeBank[index].id == sound_id)
            return EnemySeBank[index].type;
    }
    return -1;
}

int RES_GetEnemySeFoot(int sound_id)
{
    int index;

    for (index = 0; index < 8; index++) {
        if (EnemySeBank[index].id == sound_id)
            return EnemySeBank[index].foot;
    }
    return -1;
}

const char *RES_GetEnemySeName(int index)
{
    if (index < 8)
        return EnemySeBank[index].name;
    return 0;
}

typedef struct {
    char *name;
    u16 ids[14];
} CfMtnEntry;
extern CfMtnEntry cfMtnList[];

static int RES_GetMotBaseID(int id)
{
    CfMtnEntry *p;
    u16 *q;
    int base;
    int ret;
    int cur;

    id &= 0xffff;
    for (p = cfMtnList; p->name != 0; p++) {
        q = p->ids;
        base = *q;
        for (; *q != 0; q++) {
            cur = *q;
            ret = base;
            if (cur == id)
                return ret;
        }
    }
    return 1;
}

typedef struct { unsigned short *tbl; unsigned short en; unsigned short key; } FaceEnt;
extern FaceEnt faceTbl[];

static int convert_face(int id)
{
    FaceEnt *e;
    unsigned int idx = id;
    unsigned int key = idx & 0x7F00;
    unsigned short v;
    idx &= 0xFFFF;

    for (e = faceTbl; e->en != 0; e++) {
        if (e->key == key) {
            v = e->tbl[idx & 0xFF];
            if (v != 0)
                idx = v;
            break;
        }
    }
    return idx;
}

static int resource_typeid_translate(int arg0, int *arg1, int *arg2)
{
    int value;
    int upper;

    switch (*(unsigned short *)arg1) {
    case 0:
        value = *arg2;
        upper = value & 0xFFFF0000;
        if (upper == 0x10000) {
            *arg2 = convert_face(value & 0xFFFF) + upper;
            return 1;
        }
        break;
    case 2:
        if (*arg2 <= 0x02FFFFFF) {
            *arg2 = RES_GetMotBaseID(*arg2);
            return 1;
        }
        break;
    case 3:
        *arg1 = 2;
        *arg2 = arg0 | 0x03000000;
        return 1;
    case 5:
        *arg2 = arg0;
        return 1;
    }
    return 0;
}

/*
 * The runtime resource-block table: up to 128 blocks carved out of the
 * arc-file load heap GameResourceInit sizes. address/size describe the
 * block's extent; handle is a random token GameResourceAlloc
 * (main:0x0024a9d8) assigns with xglSRand() when it hands a block out, and
 * both it and GameResourceReset (main:0x0024d8e8) clear it back to 0; state
 * is -1 for a free block (this TU's readers) and 9 once GameResourceAlloc
 * marks one in use (original bytes; neither function is claimed here).
 */
typedef struct {
    u32 address;   /* +0x0 */
    int size;       /* +0x4 */
    int handle;     /* +0x8 */
    int state;      /* +0xC, -1 = free */
} GameResourceEntry;

extern GameResourceEntry GameResource[128];

int GameResourceSearch(int type, int id)
{
    int i;
    int a;
    int b;

    if (type == 5) {
        return -1;
    }
    for (i = 0; i < 0x80; i++) {
        a = type;
        b = id;
        resource_typeid_translate(i, &a, &b);
        if (type == -1 || GameResource[i].state == a) {
            if (id == -1 || GameResource[i].handle == b) {
                return i;
            }
        }
    }
    return -1;
}

static int resource_get_free(void)
{
    int index;

    for (index = 0; index < 0x80; index++) {
        if (GameResource[index].state == -1)
            return index;
    }
    return -1;
}

unsigned int GameResourceGetFreeAddr(void)
{
    unsigned int a = GameResource[resource_get_free()].address;
    if (a < 0x112A000) a = 0x112A000;
    return a;
}

extern u8 GameResourceWorkReloadLeader;

extern u8 GameResourceWorkReloadTable[];

int GameResourceWorkAlloc(int size)
{
    int *p;
    int i;

    GameResourceWorkReloadLeader = 0;
    GameResourceWorkReloadTable[0] = 0;
    p = &GameResource[0].address;
    for (i = 0; i < 0x80; i++, p += 4) {
        if (p[3] == -1) {
            if (size < p[1])
                return p[0];
            goto fail;
        }
    }
fail:
    GameResourceWorkReloadTable[0] = 1;
    return 0x112A000;
}

/*
 * The game loop's own record, as far as this TU reads it. Two spans are
 * evidenced: the runtime flags word command_player_lock ORs a bit into
 * (lw/ori/sw +0x10, main:0x0024c188) and the scene id GameResourceWorkReload
 * reloads by (lw +0x18, main:0x0024a998). Nothing else is modelled; the
 * object is still owned by the generated .sdata scaffolding.
 */
typedef struct {
    u8 unmodeled_00[0x10];
    u32 flags;      /* +0x10 */
    u8 unmodeled_14[4];
    int scene_id;   /* +0x18 */
} GameLoopStatePrefix;

extern GameLoopStatePrefix GameLoopState;
extern void ACT_init(void);
extern void MapChangeFadeSet(void);
extern void GameResourceLoad(int scene_id);

void GameResourceWorkReload(void)
{
    if (GameResourceWorkReloadTable[0] != 0) {
        ACT_init();
        GameResourceLoad(GameLoopState.scene_id);
        return;
    }
    MapChangeFadeSet();
}

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", GameResourceAlloc);

extern int GameResourceGetIndex(int id);

int GameResourceRealloc(int resource, int size)
{
    int i = GameResourceGetIndex(resource);
    int total;
    u32 addr;
    if (i < 0) {
        return 0;
    }
    if (GameResource[i + 1].state == -1) {
        total = GameResource[i].size;
        size = (size + 0x3f) & ~0x3f;
        total = total + GameResource[i + 1].size;
        addr = GameResource[i].address;
        GameResource[i].size = size;
        GameResource[i + 1].address = addr + size;
        GameResource[i + 1].size = total - size;
        return resource;
    }
    return 0;
}

extern u32 archeader[];
extern u8 arcno;
extern int xglArxExtract(void *destination, void *source);
extern void *memcpy(void *destination, const void *source, unsigned int count);

static int next_arc_size(void *address)
{
    u32 *src;
    int size;

    if (arcno == 0) {
        u32 *s = arcfileaddr;
        u32 *d = archeader;
        int i;

        for (i = *arcfileaddr >> 2; i >= 0; i--) {
            *d++ = *s++;
        }
        src = (u32 *)((char *)arcfileaddr + ((archeader[0] + 0x40) & ~0x3F));
        size = archeader[1];
    } else {
        size = archeader[arcno + 1] - archeader[arcno];
        src = (u32 *)((char *)arcfileaddr + ((archeader[0] + 0x40) & ~0x3F) + archeader[arcno]);
    }
    arcno++;
    if (size > 0) {
        if (*src == 0x585241) {
            size = xglArxExtract(address, src);
        } else {
            memcpy(address, src, size);
        }
    }
    if (arcno >= archeader[0] >> 2) {
        arcfileaddr = 0;
    }
    return size;
}

static u8 *next_line(u8 *cursor)
{
    do {
        while (*cursor != '\n' && *cursor != '\r')
            cursor++;
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r')
            cursor++;
    } while (*cursor == ';');
    return cursor;
}

static u8 *skip_space(u8 *cursor)
{
    while (*cursor == ' ' || *cursor == '\t')
        cursor++;
    return cursor;
}

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", search_key);

extern void GameResourceReset(int reset_mode);

static unsigned char *command_reset(int command, unsigned char *cursor, int mode)
{
    int n = 0;

    while ((unsigned char)(*cursor - '0') < 10) {
        int c = *cursor++;
        n *= 10;
        n = n + c - '0';
    }
    GameResourceReset(n);
    return cursor;
}

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", command_loadmap);

extern int RES_loadFile(int command, int callback, int resource_id, int flags);
static char *search_key(char *cursor, u8 *table, int *id);
extern u8 model[];

static char *command_loadact(int command, char *cursor, int mode)
{
    int resource_id = 0;
    char *next_cursor = search_key(cursor, model, &resource_id);

    RES_loadFile(command, 0, resource_id, 0);
    return next_cursor;
}

static char *command_loadface(int command, char *cursor, int mode)
{
    int resource_id = 0;
    char *next_cursor = search_key(cursor, model, &resource_id);

    RES_loadFile(command, 0, resource_id + 0x10000, 0);
    return next_cursor;
}

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", command_loadmot);

/*
 * The engine's actor record: main/tu194 (src/main/enemy_2.h) defines and
 * owns it. This TU only reads the resource pointer past that definition's
 * extent, so it names the same tag without completing it (no local header
 * can include another TU's TU-local one) rather than modelling a
 * competing layout.
 */
struct Actor;

/*
 * The resource record an actor's `resource` pointer refers to.
 * RES_GetFootStepNo is the only reader in this TU, and it reads only the
 * footstep number byte at +6; the bytes before it are an unread span here.
 */
typedef struct ActorResource {
    unsigned char unmodeled_000[6];
    u8 foot_step_no; /* +6 */
} ActorResource;

/*
 * +0x8dc: the actor's resource pointer, past the extent src/main/enemy_2.h
 * completes; RES_GetFootStepNo is the only reader of it in this TU.
 */
#define ACTOR_RESOURCE_OFFSET 0x8dc
#define ACTOR_RESOURCE(actor) \
    (*(ActorResource **)((unsigned char *)(actor) + ACTOR_RESOURCE_OFFSET))

u8 RES_GetFootStepNo(struct Actor *actor)
{
    ActorResource *resource = ACTOR_RESOURCE(actor);
    u8 foot_step_no = 0;

    if (resource != 0) {
        foot_step_no = resource->foot_step_no;
    }
    return foot_step_no;
}

static unsigned char *command_loadtex(int command, unsigned char *cursor, int mode)
{
    int n = 0;

    while ((unsigned char)(*cursor - '0') < 10) {
        int c = *cursor++;
        n *= 10;
        n = n + c - '0';
    }
    RES_loadFile(command, 11, n, 0);
    return cursor;
}

static unsigned char *command_loadmovie(int command, unsigned char *cursor, int mode)
{
    int n = 0;

    while ((unsigned char)(*cursor - '0') < 10) {
        int c = *cursor++;
        n *= 10;
        n = n + c - '0';
    }
    RES_loadFile(command, 10, n, 0);
    return cursor;
}

extern int sefLoadEffectCfName(int cf_id, int effect_name);
extern int sefLoadMemoryEffectCfName(int cf_id, int effect_name, int buffer);

static void command_loadeffect_sub(int type, int effect_name, void *address)
{
    if (arcfileaddr != 0) {
        sefLoadMemoryEffectCfName((int)address, effect_name, next_arc_size(address));
        return;
    }
    sefLoadEffectCfName((int)address, effect_name);
}

static unsigned char *command_loadeffect(int command, unsigned char *cursor, int mode)
{
    char name[256];
    char *p = name;

    while (*cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r') {
        *p++ = *cursor++;
    }
    *p = 0;
    RES_loadFile(command, 6, (int)name, (int)command_loadeffect_sub);
    return cursor;
}

/* RES_loadFile callback: (resource type, file name, resource address). */
static int command_loadene_sub(int type, const char *name, void *address)
{
    if (arcfileaddr != 0)
        next_arc_size(AdrsEnemyPreset);
    else
        Enemy_LoadPreset(AdrsEnemyPreset, name);

    return 0;
}

static unsigned char *command_loadene(int command, unsigned char *cursor, int mode)
{
    char name[256];
    char *p = name;

    while (*cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r') {
        *p++ = *cursor++;
    }
    *p = 0;
    RES_loadFile(command, 8, (int)name, (int)command_loadene_sub);
    return cursor;
}

extern int SCRIPT_load2(const char *path);
extern void SCRIPT_exec2(void);

static unsigned char *command_script(int command, unsigned char *cursor, int mode)
{
    char name[256];
    char *p = name;
    const char *src = RES_getPath();

    while ((*p = *src) != 0) {
        src++;
        p++;
    }
    while (*cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r') {
        *p++ = *cursor++;
    }
    p[0] = '.';
    p[1] = 'e';
    p[2] = 'v';
    p[3] = 't';
    p[4] = 0;
    if (!(GameLoopState.flags & 0x40)) {
        SCRIPT_load2(name);
        SCRIPT_exec2();
    }
    return cursor;
}

extern void GameCfPlayerLoadResource(int resource_set);

static char *command_player(int command, char *cursor, int mode)
{
    GameCfPlayerLoadResource(0);
    return cursor;
}

extern int GameResourceAlloc(int a, int b);

extern int GameResourceRealloc(int resource, int size);

static char *command_dummy(int command, char *cursor, int mode)
{
    if (GameResource[command + 1].address == 0) {
        GameResourceAlloc(0, 0);
    } else {
        ((int (*)())GameResourceRealloc)(GameResource[command].address);
    }
    return cursor;
}

extern unsigned char SaveData[];
extern char foot_name[];
extern char *tbl_1_00366470[];

void RES_GetLeaderSeName(char *dst)
{
    char *src = tbl_1_00366470[*(u16 *)(SaveData + 0x100A4)];
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = '_';
    dst += 3;
    src = foot_name;
    while ((*dst = *src) != 0) {
        src++;
        dst++;
    }
}

extern char env_name[];

void RES_GetMapEnvSeName(char *name)
{
    char *env = env_name;

    while ((*name = *env) != 0) {
        env++;
        name++;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", command_loadse);

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", command_loadsmd);

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", command_mpeg2_core);

static char *command_mpeg2(int command, char *cursor)
{
    return command_mpeg2_core(command, cursor, 2);
}

static char *command_mpeg2battle(int command, char *cursor, int mode)
{
    SCRIPT_fade(2);
    return command_mpeg2_core(command, cursor, 1);
}

static char *command_mpeg2nofade(int command, char *cursor)
{
    return command_mpeg2_core(command, cursor, 5);
}

extern void SCRIPT_frameLock2Battle(void);

static char *command_event2battle(int command, char *cursor, int mode)
{
    SCRIPT_frameLock2Battle();
    return cursor;
}

static char *command_player_lock(int command, char *cursor, int mode)
{
    GameLoopState.flags |= 0x8000;
    return cursor;
}

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", command_enenpcse);

static char *command_enese(int command, char *cursor, int mode)
{
    return command_enenpcse(command, cursor, 0);
}

static char *command_npcse(int command, char *cursor, int mode)
{
    return command_enenpcse(command, cursor, 1);
}

static char *command_disk(int command, char *cursor, int mode)
{
    GameDiskChange((unsigned char)cursor[0] - '0');
    return cursor;
}

static unsigned char *command_save(int command, unsigned char *cursor, int mode)
{
    int n = 0;

    while ((unsigned char)(*cursor - '0') < 10) {
        int c = *cursor++;
        n *= 10;
        n = n + c - '0';
    }
    Intermission(n);
    return cursor;
}

void GameResourcePreLoad(int scene_number)
{
    char path[256];
    char *archive_path;
    int archive_size;

    archive_path = RES_getScenePath(path, scene_number);
    arcfilepreload = 0;
    if (xglCdGetFileSize(path) > 0) {
        xglCdReadFile(path, scene_txt_buffer, 1, 1);

        archive_path[0] = 'a';
        archive_path[1] = '\0';
        archive_size = xglCdGetFileSize(path);
        if (archive_size > 0) {
            arcfilepreload = 0x02000000 - ((archive_size + 2047) & -2048);
            xglCdReadFile(path, (void *)arcfilepreload, 1, 1);
        }
    }
}

static void readreq(const char *name, void *buffer)
{
    xglCdReadFile(name, buffer, 0, 1);
}

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", GameResourceLoad);

extern char *MAP_getPath(void);
extern char dummy_3[];

static int RES_loadFileSubMapSub(char *dir, char *name, void *buffer)
{
    char path[256];
    char *d = path;
    char *s;
    char *n;
    char *r;
    char *q;
    int size;

    q = MAP_getPath();
    while (*q != 0) *d++ = *q++;
    q = dir;
    while (*q != 0) *d++ = *q++;
    q = name;
    if (*q != 0) {
        do {
            *d++ = *(unsigned char *)q++;
        } while (*(unsigned char *)q != 0);
    }
    *d = 0;
    size = xglCdGetFileSize(path);
    if (size <= 0) {
        char *e = path;
        s = dummy_3;
        while (*s != 0) *e++ = *s++;
        s = name;
        while (*s != 0) *e++ = *s++;
        *e = 0;
        size = xglCdGetFileSize(path);
    }
    readreq(path, buffer);
    return size;
}

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", RES_loadFileSubMap);

extern void GameResourceReset(int reset_mode);
char *RES_GetMotBaseName(u16 id)
{
  CfMtnEntry *p;
  u16 *q;
  char *ret;
  int cur;
  for (p = cfMtnList; p->name != 0; p++)
  {
    for (q = p->ids; (*q) != 0; q++)
    {
      cur = *q;
      ;
      if (cur == id)
      {
        return p->name;
      }
    }

  }

 do { } while (0);
  return cfMtnList->name;
}

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", RES_GetMdlFileNameSub);

extern u8 ModelPath;
void RES_GetMdlFileNameSub(char *name);

void RES_GetMdlFileName(char *name)
{
    char buf[256];
    char *src;
    char *dst;
    int done;

    if (ModelPath == 0) {
        RES_GetMdlFileNameSub(name);
        return;
    }
    RES_GetMdlFileNameSub(buf);
    src = buf;
    dst = name;
    done = 0;
    for (; (*dst = *src) != 0; dst++, src++) {
        if (*src == '\\') {
            if (done == 0) {
                done = 1;
                dst[5] = '\\';
                dst[1] = 't';
                dst[2] = 'e';
                dst[3] = 's';
                dst[4] = 't';
                dst += 5;
            }
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", RES_loadFileSub);

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", RES_loadFile);

int GameResourceGetIndex(int id)
{
    int i;
    int *p = (int *)&GameResource[0].address;
    for (i = 0; i < 128; i++) {
        int v = *p;
        p += 4;
        if (v == id) return i;
        if (v == 0) break;
    }
    return -1;
}

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern char D_004C01B0[];

void GameResourceDump(int dump)
{
    GameResourceEntry *p;
    GameResourceEntry *q;
    int i;
    int n;
    int y;

    if (dump != 0) {
        i = 0x7F;
        if (GameResource[i].address == 0) {
            do {
                i--;
                if (GameResource[i].address != 0) break;
            } while (i != 0);
        }
        i -= 0xE;

        if (i < 0) i = 0;

        for (n = 0; n < 16; n++) {
            xglFontDebugPrintf(0x40, 0x40 + n * 8, D_004C01B0, i, GameResource[i].address, GameResource[i].size, GameResource[i].state, GameResource[i].handle);
            i++;

        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/res_get_path", GameResourceReset);

extern void (*GameResourceReadFileCallback)(void);

void GameResourceInit(u32 addr, int size)
{
    int i;
    for (i = 0; i < 128; i++) {
        GameResource[i].address = 0;
        GameResource[i].size = 0;
        GameResource[i].handle = 0;
        GameResource[i].state = -1;
    }
    GameResource[0].address = addr;
    GameResource[0].size = size;
    GameResourceReset(0);
    *(int *)0x01000000 = 0;
    *(int *)0x01070800 = 0;
    GameResourceReadFileCallback = default_callback;
    *(int *)0x010B1000 = 0;
    *(int *)0x010B4000 = 0;
    *(int *)0x010E6000 = 0;
    *(int *)0x010E7000 = 0;
}

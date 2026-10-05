#include "common.h"
#include "shared.h"

/*
 * UwamonoMapUnit is this TU's local view of the same MapUnit[] record that
 * src/main/init_drill.c also touches; only the fields SendBrokenSignal,
 * GetUwamonoSignal and CreateUwamonoCommon read or write are named, and the
 * rest is unmodeled padding recovered as its exact byte span so the offsets
 * below stay exact.
 */
typedef struct UwamonoMapUnit {
    u32 flags;                        /* +0x00 */
    unsigned char unmodeled_04[0x20];
    /*
     * MAP_updateUnitSymbol (VA 0x002bfbe0) adds the scaffold-owned .lit4
     * constant D_004D7EBC (value 0.05) to this field unconditionally
     * (`lwc1`/`add.s`/`swc1`, no branch in the original); no other role is
     * evidenced here.
     */
    float symbolPhase;                /* +0x24 */
    unsigned char unmodeled_28[0x79];
    signed char actionNo;             /* +0xA1 */
    signed char actionSub;            /* +0xA2 */
    unsigned char unmodeled_a3[5];
    short sequenceNo;                 /* +0xA8 */
    short sequenceSub;                /* +0xAA */
    unsigned char unmodeled_ac[0xf8];
    signed char signal;               /* +0x1A4 */
} UwamonoMapUnit;

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", InitUwamonoSys);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", Unit_CreateUwamono);

extern unsigned char D_004CA028[];
extern int uwares_tbl[14];
extern int xtxres_tbl[14];
extern int printf(const char *format, ...);

/*
 * MAP_LoadUwamonoResource's own additive view of the same MapUnit[] record
 * UwamonoMapUnit above already partially names; only the fields
 * MAP_LoadUwamonoResource (VA 0x002be5f0) writes are named here: it stores
 * uwares_tbl[index] and xtxres_tbl[index], where index = resourceId -
 * 0x7001.
 */
typedef struct UwamonoResourceUnit {
    unsigned char unmodeled_00[0xe0];
    int uwaresId;                     /* +0xE0 */
    int xtxresId;                     /* +0xE4 */
} UwamonoResourceUnit;

/*
 * Valid resourceId values run 0x7001..0x700E (14 entries, the size of
 * uwares_tbl and xtxres_tbl); 0x7012 is excluded explicitly ahead of the
 * broader >= 0x1000 range check.
 */
void MAP_LoadUwamonoResource(UwamonoResourceUnit *unit, int resourceId)
{
    int index;
    int uwaresId;
    int xtxresId;

    printf((const char *) D_004CA028);
    if (resourceId != 0x7012 && resourceId >= 0x1000) {
        index = resourceId - 0x7001;
        uwaresId = uwares_tbl[index];
        xtxresId = xtxres_tbl[index];
        unit->xtxresId = xtxresId;
        unit->uwaresId = uwaresId;
    }
}

/*
 * GetPartsPos's own additive view of the global game-loop record (TU-local
 * by canon, config/header-canon.json): only the map-parts resource root
 * pointer at +0x54 is read here.
 */
typedef struct MapPartsResource {
    unsigned char unmodeled_00[0x50];
    int arrayOffset;              /* +0x50, added to this pointer to form the entry array base */
} MapPartsResource;

typedef struct MapPartsRoot {
    unsigned char unmodeled_00[4];
    MapPartsResource *resource;   /* +0x04 */
} MapPartsRoot;

typedef struct UwamonoGameLoopState {
    unsigned char unmodeled_00[0x54];
    MapPartsRoot *partsRoot;          /* +0x54 */
    unsigned char unmodeled_58[0xe4 - 0x58];
    short slot[64];                   /* +0xe4 */
} UwamonoGameLoopState;

extern UwamonoGameLoopState GameLoopState;

/*
 * GetPartsPos's own additive view of the same MapUnit[] record other
 * functions in this TU already partially name; only the fields it writes
 * are named here: position (+0x10, the full Vector4 copied from the
 * resource entry, w set to 1.0f) and rotation's three touched components
 * (+0x20/+0x24/+0x28, x and z always cleared, y set from atan2f); entry
 * (+0x80, the resource entry pointer stored for later use); serial (+0xA4,
 * this unit's index into the resource entry array).
 */
typedef struct GetPartsPosEntry {
    unsigned char unmodeled_00[0x20];
    float basisX;             /* +0x20 */
    unsigned char unmodeled_24[4];
    float basisZ;              /* +0x28 */
    unsigned char unmodeled_2c[4];
    float x;                    /* +0x30 */
    float y;                     /* +0x34 */
    float z;                      /* +0x38 */
    unsigned char unmodeled_3c[4];
} GetPartsPosEntry;

typedef struct GetPartsPosUnit {
    unsigned char unmodeled_00[0x10];
    Vector4 position;      /* +0x10, w set to 1.0f */
    float rotationX;         /* +0x20, always cleared */
    float rotationY;          /* +0x24, atan2f(entry->basisX, entry->basisZ) */
    float rotationZ;           /* +0x28, always cleared */
    unsigned char unmodeled_2c[0x54];
    GetPartsPosEntry *entry;      /* +0x80 */
    unsigned char unmodeled_84[0x20];
    short serial;                  /* +0xA4 */
} GetPartsPosUnit;

extern float atan2f(float y, float x);

void GetPartsPos(GetPartsPosUnit *unit)
{
    MapPartsRoot *root;
    MapPartsResource *resource;
    GetPartsPosEntry *table;
    GetPartsPosEntry *entry;

    root = GameLoopState.partsRoot;
    resource = root->resource;
    table = (GetPartsPosEntry *) ((char *) resource + resource->arrayOffset);
    entry = &table[unit->serial];

    unit->entry = entry;
    unit->position.x = entry->x;
    unit->position.y = entry->y;
    unit->position.z = entry->z;
    unit->position.w = 1.0f;
    unit->rotationX = 0.0f;
    unit->rotationY = atan2f(entry->basisX, entry->basisZ);
    unit->rotationZ = 0.0f;
}

void SendBrokenSignal(UwamonoMapUnit *unit)
{
    unit->signal = 2;
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", GetPartsSize);

extern void sefDeleteEffectCf(int effectId);
extern void xglSoundEffectStopID(int soundId, int channel);

/*
 * ClearUwamonoEffect's own additive view of the same MapUnit[] record
 * UwamonoMapUnit above already partially names; only the fields
 * ClearUwamonoEffect (VA 0x002be8c0) reads or writes are named here.
 */
typedef struct UwamonoEffectUnit {
    unsigned char unmodeled_00[0xa0];
    /* Read once (`lbu`), plus one, as the channel argument to
     * xglSoundEffectStopID; no other role is evidenced here. */
    unsigned char seChannel;           /* +0xA0 */
    unsigned char unmodeled_a1[0x47];
    /* Walked in order, deleting and zeroing each non-zero effect handle
     * with sefDeleteEffectCf. */
    int effectCf[3];                   /* +0xE8 */
    unsigned char unmodeled_f4[0xac];
    /*
     * The original computes this sub-pointer once, ahead of the effect
     * loop, and keeps it live (register evidence) across the whole
     * function; UwamonoTimers below names the same +0x1A0 block for
     * UwamonoCommonFunc's view.
     */
    struct UwamonoEffectTimers {
        unsigned char unmodeled_00[0x48];
        int bgmTimer;                   /* +0x48 (record +0x1E8) */
    } timers;                           /* +0x1A0 */
} UwamonoEffectUnit;

void ClearUwamonoEffect(UwamonoEffectUnit *unit)
{
    int i;
    int effectId;
    struct UwamonoEffectTimers *timers = &unit->timers;

    for (i = 0; i < 3; i++) {
        effectId = unit->effectCf[i];
        if (effectId != 0) {
            sefDeleteEffectCf(effectId);
            unit->effectCf[i] = 0;
        }
    }
    if (timers->bgmTimer > 0) {
        xglSoundEffectStopID(timers->bgmTimer, unit->seChannel + 1);
        timers->bgmTimer = -1;
    }
}

signed char GetUwamonoSignal(UwamonoMapUnit *unit)
{
    return unit->signal;
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", InitUwamono);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", InitMapParts);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", InitMapKoware);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", InitSaveSymbol);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", InitShopSymbol);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", InitEvsSymbol);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", InitRetSymbol);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", SetHideObject);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", GameStateRestoreKoware);

extern unsigned char MapUnit[];

void SetMapUwamono(void)
{
    struct { char pad[0xe4]; short slot[64]; } *g = (void *)&GameLoopState;
    short *p = g->slot;
    char *u = (char *)MapUnit;
    int i = 0x3F;
    do {
        *p++ = -1;
        if (*(int *)u & 0x10000) {
            void (*f)(void *) = *(void (**)(void *))(u + 4);
            if (f != 0) f(u);
        }
        u += 0x300;
    } while (--i >= 0);
    UnlockMapUnit();
}

void UnlockMapUnit(void);
struct ShootSysView { char pad[0xe4]; short slot[64]; };

void ResetShootSys(void)
{
    struct ShootSysView *g = (struct ShootSysView *)&GameLoopState;
    short v = -1;
    int i;
    for (i = 63; i >= 0; i--) {
        g->slot[i] = v;
    }
    UnlockMapUnit();
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", CreateUwamono);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", CreateKowaremono);

void CreateUwamonoCommon(UwamonoMapUnit *unit)
{
    unit->actionNo = 0;
    unit->flags |= 0x10000;
    unit->sequenceNo = 0;
    unit->sequenceSub = 0;
    unit->actionSub = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", MAP_updateUnitKoware);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", MAP_updateUnitEnemy);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", MAP_updateUnitHide);

extern const float D_004D7EB4;
extern int CheckDist3D(void *a, void *b);

long MAP_updateUnitSaveSymbol(UwamonoMapUnit *unit)
{
    struct { int w0; char *player; int pad[4]; } *g = (void *)&GameLoopState;
    unit->symbolPhase = unit->symbolPhase + D_004D7EB4;
    return CheckDist3D(g->player + 0x10, (char *)unit + 0x10);
}

extern const float D_004D7EB8;

long MAP_updateUnitShopSymbol(UwamonoMapUnit *unit)
{
    struct { int w0; char *player; int pad[4]; } *g = (void *)&GameLoopState;
    unit->symbolPhase = unit->symbolPhase + D_004D7EB8;
    return CheckDist3D(g->player + 0x10, (char *)unit + 0x10);
}

extern const float D_004D7EBC;

void MAP_updateUnitSymbol(UwamonoMapUnit *unit)
{
    unit->symbolPhase = unit->symbolPhase + D_004D7EBC;
}

typedef struct { float x, y, z, w; } __attribute__((aligned(16))) UwaWindVec;
typedef struct {
    unsigned char type;
    char pad[0xf];
    UwaWindVec pos;
    float f20;
    float f24;
    int i28;
} UwaWindState_;
typedef struct {
    char pad[0x10];
    UwaWindVec pos;
    char pad2[0xb4 - 0x20];
    float height;
} UwaWindSrc_;
extern UwaWindState_ uwaWind;
extern const float D_004D7EC0;
extern const float D_004D7EC4;
extern void EXM_SetWindStruct(void *state);

void SetUwaWind(UwaWindSrc_ *unit)
{
    uwaWind.type = 2;
    uwaWind.pos = unit->pos;
    uwaWind.pos.y = uwaWind.pos.y + unit->height * 0.5f;
    uwaWind.pos.w = D_004D7EC0;
    uwaWind.f20 = D_004D7EC4;
    uwaWind.f24 = 1.0f;
    uwaWind.i28 = 0;
    EXM_SetWindStruct(&uwaWind);
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", UwamonoBrokenSe);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", DrawActiveCursol);

extern int HitCheckMapUnitAt(void *pos, void *unit);
typedef struct HitMapUnitSub {
    unsigned char unmodeled_00[0x2D];
    signed char hit;
    unsigned char unmodeled_2e[0x132];
} HitMapUnitSub;
typedef struct HitMapUnit {
    u32 flags;
    unsigned char unmodeled_04[0xA0];
    short serial;
    unsigned char unmodeled_a6[0xFA];
    HitMapUnitSub sub;
} HitMapUnit;

int HitCheckMapUnit(void *pos)
{
    int i;
    HitMapUnitSub *sub;

    for (i = 0; i < 64; i++) {
        sub = &((HitMapUnit *)MapUnit)[i].sub;
        if (((HitMapUnit *)MapUnit)[i].serial != -1
            && !(((HitMapUnit *)MapUnit)[i].flags & 0x100000)
            && sub->hit != 0
            && (((HitMapUnit *)MapUnit)[i].flags & 0x10000)
            && HitCheckMapUnitAt(pos, &((HitMapUnit *)MapUnit)[i]) != 0) {
            return i;
        }
    }
    return -1;
}

int HitCheckNyuru(void *pos, void *unit);
void NyuruCircle(void *pos, void *unit);
void NyuruMatrix(void *pos, void *unit, int mode);
typedef struct NyuruVec {
    unsigned long long w[2];
} NyuruVec;
typedef struct NyuruQuery {
    unsigned char unmodeled_00[0x10];
    union {
        NyuruVec v;
        Vector4 f;
    } pos;
    unsigned char unmodeled_20[0x10];
    union {
        NyuruVec v;
        Vector4 f;
    } move;
} NyuruQuery;

int HitCheckMapUnitWithNyuru(NyuruQuery *q)
{
    int list[64];
    NyuruVec savePos;
    NyuruVec saveMove;
    int i;
    int n;
    HitMapUnitSub *sub;
    HitMapUnit *unit;

    for (i = 63; i >= 0; i--) {
        list[i] = -1;
    }
    n = 0;
    for (i = 0; i < 64; i++) {
        sub = &((HitMapUnit *)MapUnit)[i].sub;
        if (((HitMapUnit *)MapUnit)[i].serial != -1
            && !(((HitMapUnit *)MapUnit)[i].flags & 0x100000)
            && sub->hit != 0
            && (((HitMapUnit *)MapUnit)[i].flags & 0x10000)
            && HitCheckMapUnitAt(q, &((HitMapUnit *)MapUnit)[i]) != 0) {
            list[n++] = i;
        }
    }
    if (n == 0) {
        return 0;
    }
    if (n == 1) {
        return HitCheckNyuru(q, &((HitMapUnit *)MapUnit)[list[0]]);
    }
    for (i = 0; list[i] != -1; i++) {
        savePos = q->pos.v;
        saveMove = q->move.v;
        unit = &((HitMapUnit *)MapUnit)[list[i]];
        switch (unit->sub.hit) {
        case 1:
            NyuruCircle(q, unit);
            break;
        case 2:
            NyuruMatrix(q, unit, 1);
            break;
        }
        if (HitCheckMapUnit(q) == -1) {
            return 1;
        }
        q->pos.v = savePos;
        q->move.v = saveMove;
    }
    q->pos.f.x -= q->move.f.x;
    q->pos.f.z -= q->move.f.z;
    q->move.f.x = 0.0f;
    q->move.f.z = 0.0f;
    return 0;
}

int HitCheckMapUnitReverse(void *pos)
{
    int i;
    HitMapUnitSub *sub;

    for (i = 64; i > 0; i--) {
        sub = &((HitMapUnit *)MapUnit)[i].sub;
        if (((HitMapUnit *)MapUnit)[i].serial != -1
            && !(((HitMapUnit *)MapUnit)[i].flags & 0x100000)
            && sub->hit != 0
            && (((HitMapUnit *)MapUnit)[i].flags & 0x10000)
            && HitCheckMapUnitAt(pos, &((HitMapUnit *)MapUnit)[i]) != 0) {
            return i;
        }
    }
    return -1;
}

/*
 * HitCheckMapUnitPos (main:0x002c02f0): a thin tail call into
 * HitCheckMapUnitPosSize with its distance threshold fixed at 0.0f (a
 * true `j`, no `jal`, so this function's own return value is whatever
 * HitCheckMapUnitPosSize returns).
 */
extern int HitCheckMapUnitPosSize(Vector4 *pos, float threshold);

void HitCheckMapUnitPos(Vector4 *pos) {
    HitCheckMapUnitPosSize(pos, 0.0f);
}

typedef struct HitMapQuery {
    unsigned char unmodeled_00[0x10];
    Vector4 pos;
    unsigned char unmodeled_20[0x9C8];
    float flag;
    unsigned char unmodeled_9ec[0x84];
} HitMapQuery;

extern int HitCheckMapUnitPosAt(void *query, void *unit);

int HitCheckMapUnitPosSize(Vector4 *pos, float size)
{
    HitMapQuery query;
    int i;
    HitMapUnitSub *sub;

    query.pos = *pos;
    query.flag = size;
    for (i = 0; i < 64; i++) {
        sub = &((HitMapUnit *)MapUnit)[i].sub;
        if (((HitMapUnit *)MapUnit)[i].serial != -1
            && sub->hit != 0
            && (((HitMapUnit *)MapUnit)[i].flags & 0x10000)
            && HitCheckMapUnitPosAt(&query, &((HitMapUnit *)MapUnit)[i]) != 0) {
            return i;
        }
    }
    return -1;
}

int HitCheckMapUnitPosReverse(Vector4 *pos)
{
    HitMapQuery query;
    int i;
    HitMapUnitSub *sub;

    query.pos = *pos;
    query.flag = 0.0f;
    for (i = 64; i > 0; i--) {
        sub = &((HitMapUnit *)MapUnit)[i].sub;
        if (((HitMapUnit *)MapUnit)[i].serial != -1
            && sub->hit != 0
            && (((HitMapUnit *)MapUnit)[i].flags & 0x10000)
            && HitCheckMapUnitAt(&query, &((HitMapUnit *)MapUnit)[i]) != 0) {
            return i;
        }
    }
    return -1;
}

extern float CheckDist2D(const Vector4 *first, const Vector4 *second);
extern int HitCheckBox(void *query, void *unit);
typedef struct HitAtUnit {
    unsigned char unmodeled_00[0x10];
    Vector4 pos;
    unsigned char unmodeled_20[0x90];
    float radius;                     /* +0xB0 */
    float height;                     /* +0xB4 */
    unsigned char unmodeled_b8[0xE8];
    HitMapUnitSub sub;                /* +0x1A0 */
} HitAtUnit;

int HitCheckMapUnitAt(void *pos, void *unit)
{
    HitMapQuery *q = pos;
    HitAtUnit *u = unit;
    int ret = 0;
    HitMapUnitSub *sub = &u->sub;

    if (q->pos.y > u->pos.y + u->height * 0.9f) {
        return ret;
    }
    if (q->pos.y < u->pos.y - 1.8f) {
        return ret;
    }
    switch (sub->hit) {
    case 1:
        if (CheckDist2D(&q->pos, &u->pos) < q->flag + u->radius) {
            return 1;
        }
        return 0;
    case 2:
        return HitCheckBox(q, u);
    }
    return ret;
}

extern int HitCheckBoxCorner(void *query, void *unit);

int HitCheckMapUnitPosAt(void *pos, void *unit)
{
    HitMapQuery *q = pos;
    HitAtUnit *u = unit;
    int ret = 0;
    HitMapUnitSub *sub = &u->sub;

    if (q->pos.y > u->pos.y + u->height * 0.9f) {
        return ret;
    }
    if (q->pos.y < u->pos.y - 1.8f) {
        return ret;
    }
    switch (sub->hit) {
    case 1:
        if (CheckDist2D(&q->pos, &u->pos) < q->flag + u->radius) {
            return 1;
        }
        return 0;
    case 2:
        return HitCheckBoxCorner(q, u);
    }
    return ret;
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", HitCheckBox);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", HitCheckBoxCorner);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", HitCheckCorner);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", HitCheckBoxTest);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", HitCheckNyuru);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", NyuruCircle);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", NyuruMatrix);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", NyuruBox);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", NyuruCorner);

extern void AimMapUnit(void);
extern void DispDebugUwamono(void);
extern void DispCollUwamono(void);
extern float Get_Angle(void *first, void *second);
extern void *sefCreateEffectCf(int effect_id, void *position, int flags);
extern void ACT_setMotion(void *actor, int motion);
extern void xglSoundEffectNormalID(int sound_id, int variant);
extern void Vibration_Set_Weak(int level);
extern void Enemy_FindByEar(void *actor, void *pos);
extern signed char dispflg;
extern signed char collflg;
extern signed char printflg;
extern char D_004CA260[];
extern struct { unsigned char pad[0x2A]; unsigned short trig; } PadData;
typedef struct ShootActor {
    unsigned char unmodeled_00[0x10];
    Vector4 pos;
    unsigned char unmodeled_20[0x4C8];
    unsigned long long state;         /* +0x4E8 */
    unsigned char unmodeled_4f0[0x200];
    int actFlags;                     /* +0x6F0 */
    float motionTime;                 /* +0x6F4 */
    float motionSpeed;                /* +0x6F8 */
    unsigned char unmodeled_6fc[8];
    unsigned short motion;            /* +0x704 */
    unsigned char unmodeled_706[0x2DE];
    float angle;                      /* +0x9E4 */
} ShootActor;
typedef struct ShootEffect {
    unsigned char unmodeled_00[0x6BC];
    ShootActor *owner;                /* +0x6BC */
    unsigned char unmodeled_6c0[0x3CC];
    int flags;                        /* +0xA8C */
} ShootEffect;
typedef struct ShootLoopState {
    int w0;
    ShootActor *player;               /* +0x04 */
    int pad[2];
    int flags;                        /* +0x10 */
    unsigned char unmodeled_14[0x29F2D];
    signed char shootUnit;            /* +0x29F41 */
} ShootLoopState;
typedef struct ShootSub {
    unsigned char unmodeled_00[4];
    signed char found;
} ShootSub;
#define G (*(ShootLoopState *)&GameLoopState)

void ShootMapUnit(void)
{
    ShootActor *actor = G.player;
    ShootEffect *eff;
    ShootSub *sub;
    int no;
    float limit;

    if (actor->state & 0x0F000000) {
        UnlockMapUnit();
        return;
    }
    if (dispflg == 1) {
        DispDebugUwamono();
    }
    if (collflg == 1) {
        DispCollUwamono();
    }
    if (actor->motion != 14) {
        if (!(G.flags & 0x8000)) {
            AimMapUnit();
            if ((PadData.trig & 0x80) && G.shootUnit != -1) {
                actor->angle = Get_Angle(&actor->pos, MapUnit + 0x10 + G.shootUnit * 0x300);
                eff = sefCreateEffectCf(0x2DE, 0, 0);
                if (eff != 0) {
                    eff->owner = actor;
                    eff->flags &= ~0x400;
                }
                actor->motionSpeed = 0.033333335f;
                G.flags |= 0xC000;
                ACT_setMotion(actor, 14);
                actor->actFlags &= ~8;
                xglSoundEffectNormalID(0x10002, 0);
            }
        }
        if (actor->motion != 14) {
            return;
        }
    }
    no = G.shootUnit;
    sub = (ShootSub *)&((HitMapUnit *)MapUnit)[no].sub;
    if (actor->motionTime > 0.2f && actor->motionTime < 0.36f) {
        Vibration_Set_Weak(4);
    }
    if (actor->motionTime > 0.36f && sub->found == 0) {
        if (printflg) {
            printf(D_004CA260, G.shootUnit);
        }
        Enemy_FindByEar(actor, MapUnit + 0x10 + no * 0x300);
        sub->found = 1;
    }
    if (actor->motionTime > 0.5f) {
        G.shootUnit = -1;
        G.flags = (G.flags & ~0x4000) & 0xFFFF7FFF;
        actor->actFlags |= 8;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", CrossPointUwamono);

int CalcNearCrossPointCircle(void *a, void *b, char *c);
void CalcNearCrossPointBox(void *a, void *b, char *c);

int CrossPointUwamonoAt(void *a, void *b, char *c)
{
    switch (c[0x1CD]) {
    case 1:
        return CalcNearCrossPointCircle(a, b, c);
    case 2:
        CalcNearCrossPointBox(a, b, c);
        break;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", CalcNearCrossPointCircle);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", CalcNearCrossPointBox);

/*
 * CheckNearPoint (VA 0x002c29d8, main/tu182) -- re-treated 2026-09-12 from the
 * accepted ee-asm record config/units/math-spark-cont01-002c29d8.json as C
 * with constrained ee-vu-cop2 inline assembly.
 * The EE has no scalar instruction for the aligned XYZ vector subtract, so each
 * of the two blocks below is the hardware sequence the original executes
 * (lqc2/lqc2/vsub.xyz/sqc2 into a reused stack scratch vector); everything
 * around it -- the parameters, the two xglVectorLength calls, the comparison
 * and the selected-vector copy -- is ordinary C. See notes.md for the
 * per-instruction correspondence with CheckNearPoint.s.
 */
void CheckNearPoint(const Vector4 *origin, const Vector4 *candA, const Vector4 *candB, Vector4 *out)
{
    Vector4 delta;
    float lenA;
    float lenB;

    __asm__ __volatile__(
        "lqc2 vf3,0(%1)\n\t"
        "lqc2 vf2,0(%0)\n\t"
        "vsub.xyz vf2xyz,vf2xyz,vf3xyz\n\t"
        "sqc2 vf2,0(%2)"
        :
        : "r"(origin), "r"(candA), "r"(&delta)
        : "memory"
    );
    xglVectorLength(&lenA, &delta);

    __asm__ __volatile__(
        "lqc2 vf3,0(%1)\n\t"
        "lqc2 vf2,0(%0)\n\t"
        "vsub.xyz vf2xyz,vf2xyz,vf3xyz\n\t"
        "sqc2 vf2,0(%2)"
        :
        : "r"(origin), "r"(candB), "r"(&delta)
        : "memory"
    );
    xglVectorLength(&lenB, &delta);

    if (lenA < lenB)
        *out = *candA;
    else
        *out = *candB;
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", UwamonoAreaCheck);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", AreaCheckBox);

int CrossCheckMapUnitAt(float *a, float *b, char *unit);
typedef struct AimUnitId {
    unsigned char kind;
    unsigned char pad[3];
    short serial;
} AimUnitId;

int CrossCheckMapUnitAim(float *a, unsigned char *actor)
{
    int i;
    int off;
    AimUnitId *id;
    HitMapUnitSub *sub;
    char *unit;
    char *base;

    for (i = 0, off = 0; i < 64; i++, off += 0x300) {
        sub = &((HitMapUnit *)MapUnit)[i].sub;
        base = (char *)MapUnit;
        id = (AimUnitId *)((char *)&((HitMapUnit *)MapUnit)[i] + 0xA0);
        if (id->serial == -1) continue;
        if (id->serial == 0x7000) continue;
        unit = (char *)(off + (int)base);
        if (((HitMapUnit *)MapUnit)[i].flags & 0x100000) continue;
        if (id->kind != actor[0xA0]
            && sub->hit != 0
            && (((HitMapUnit *)MapUnit)[i].flags & 0x10000)
            && CrossCheckMapUnitAt(a, (float *)(actor + 0x10), unit) != 0) {
            return i;
        }
    }
    return -1;
}

int CrossCheckMapUnitAt(float *a, float *b, char *unit);

int CrossCheckMapUnit(float *a, float *b)
{
    int i;
    HitMapUnitSub *sub;

    for (i = 0; i < 64; i++) {
        sub = &((HitMapUnit *)MapUnit)[i].sub;
        if (((HitMapUnit *)MapUnit)[i].serial != -1
            && !(((HitMapUnit *)MapUnit)[i].flags & 0x100000)
            && sub->hit != 0
            && (((HitMapUnit *)MapUnit)[i].flags & 0x10000)
            && CrossCheckMapUnitAt(a, b, (char *)&((HitMapUnit *)MapUnit)[i]) != 0) {
            return i;
        }
    }
    return -1;
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", CrossCheckActor);

extern float D_004D7F1C;
int CrossCheckMapUnitCircle(float *a, float *b, char *unit);
int CrossCheckMapUnitBox(float *a, float *b, char *unit);

int CrossCheckMapUnitAt(float *a, float *b, char *unit)
{
    int ret = 0;
    char *ext = unit + 0x1A0;
    if (__builtin_fabsf(a[1] - *(float *)(unit + 0x14)) > D_004D7F1C) {
        return ret;
    }
    switch (ext[0x2D]) {
    case 1:
        return CrossCheckMapUnitCircle(a, b, unit);
    case 2:
        return CrossCheckMapUnitBox(a, b, unit);
    }
    return ret;
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", CrossCheckMapUnitCircle);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", CrossCheckMapUnitBox);

void UnlockMapUnit(void)
{
    unsigned char *p = MapUnit;
    int *u;
    int i = 0x3F;
    p += 0xE8;
    u = (int *)p;
    do {
        int a = u[1];
        u[-0x3A] &= 0xFFFBFFFF;
        if (a != 0) {
            sefDeleteEffectCf(a);
            u[1] = 0;
        }
        if (u[2] != 0) {
            sefDeleteEffectCf(u[2]);
            u[2] = 0;
        }
        u += 0xC0;
    } while (--i >= 0);
    *((signed char *)&GameLoopState + 0x29F41) = -1;
}

typedef struct BrokenTimers {
    unsigned char unmodeled_00[4];
    signed char state;                /* +0x04 (record +0x1A4) */
    unsigned char unmodeled_05[0x3f];
    int brokenId;                     /* +0x44 (record +0x1E4) */
} BrokenTimers;

typedef struct BrokenMapUnit {
    unsigned int flags;               /* +0x00 */
    unsigned char unmodeled_04[0xa0];
    short serial;                     /* +0xA4 */
    unsigned char unmodeled_a6[0xfa];
    BrokenTimers timers;              /* +0x1A0 */
} BrokenMapUnit;

int CheckBrokenMapUnit(BrokenMapUnit *unit);
extern float CheckCornerDist(Vector4 *pos, void *unit);
typedef struct NearLoopState {
    int w0;
    ShootActor *player;               /* +0x04 */
    unsigned char unmodeled_08[0x29F3C];
    float nearDist;                   /* +0x29F44 */
} NearLoopState;

int CheckNearMapUnit(void)
{
    float best;
    ShootActor *actor;
    int found;
    int i;

    found = -1;
    best = ((NearLoopState *)&GameLoopState)->nearDist + 1000.0f;
    actor = ((NearLoopState *)&GameLoopState)->player;
    for (i = 0; i < 64; i++) {
        if ((CheckBrokenMapUnit((BrokenMapUnit *)&((HitMapUnit *)MapUnit)[i]) & 1)
            && (((HitMapUnit *)MapUnit)[i].flags & 0x40000)) {
            float d = CheckCornerDist(&actor->pos, (BrokenMapUnit *)&((HitMapUnit *)MapUnit)[i]);
            if (d < best) {
                best = d;
                found = i;
            }
        }
    }
    if (best < ((NearLoopState *)&GameLoopState)->nearDist) {
        return found;
    }
    return -1;
}

int CheckBrokenMapUnit(BrokenMapUnit *unit)
{
    BrokenTimers *t = &unit->timers;

    if (unit->serial == -1) {
        return 0;
    }
    if (!(unit->flags & 0x10000)) {
        return 0;
    }
    if (unit->flags & 4) {
        return 0;
    }
    if (t->state == 1) {
        return 0;
    }
    if (t->brokenId == -1) {
        return 0;
    }
    return t->brokenId;
}

extern float D_004D7F24;

int AimHeightCheck(char *unit)
{
    struct { int w0; char *player; int pad[6]; int flags; } *g = (void *)&GameLoopState;
    char *p = g->player;
    if (!(g->flags & 2)) return 1;
    return __builtin_fabsf(*(float *)(p + 0x14) - *(float *)(unit + 0x14)) < D_004D7F24;
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", AimMapUnit);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", AimMapUnitLookCheck);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", CheckCornerDist);

/*
 * *(UwamonoLinkedUnit.position below) is a separate record UwamonoCommonFunc
 * keeps synchronized with this unit's position; only the field it writes is
 * named.
 */
typedef struct UwamonoLinkedUnit {
    unsigned char unmodeled_00[0x30];
    Vector4 position; /* +0x30, set from the owning unit's position */
} UwamonoLinkedUnit;

/*
 * UwamonoCommonFunc's own additive view of the same MapUnit[] record
 * UwamonoMapUnit above already partially names (src/main/init_drill.c keeps
 * a third view, DrillMapUnit); only the fields UwamonoCommonFunc reads or
 * writes that UwamonoMapUnit does not already name are named here: position
 * (+0x10, read to sync linkedUnit's position); linkedUnit (+0x80, the record
 * kept in sync); serial (+0xA4, an lh compared against 0x1000 like
 * DrillMapUnit's own +0xA4 serial); and the timers block (+0x1A0), whose
 * heightCheckFlag (+0x1CE) and bgmTimer (+0x1E8) fields the original reaches
 * through a second pointer computed once, ahead of the serial branch, and
 * kept live (register evidence) across the CheckUwamonoHeight call.
 */
typedef struct UwamonoTimers {
    unsigned char unmodeled_00[0x2e];
    signed char heightCheckFlag; /* +0x2E (record +0x1CE) */
    unsigned char unmodeled_2f[0x19];
    int bgmTimer;                /* +0x48 (record +0x1E8) */
} UwamonoTimers;

typedef struct UwamonoCommonUnit {
    unsigned char unmodeled_00[0x10];
    Vector4 position;                 /* +0x10 */
    unsigned char unmodeled_20[0x60];
    UwamonoLinkedUnit *linkedUnit;     /* +0x80 */
    unsigned char unmodeled_84[0x20];
    short serial;                     /* +0xA4 */
    unsigned char unmodeled_a6[0xfa];
    UwamonoTimers timers;              /* +0x1A0 */
} UwamonoCommonUnit;

/* Defined in src/main/map_create_unit_peer.c (main/tu270), still in asm. */
extern void MAP_updateUnitPartsSequence(void *unit);
extern void MAP_updateUnitSequence(void *unit);

/* Defined later in this TU (a local sibling still in asm). */
void CheckUwamonoHeight(UwamonoCommonUnit *unit);
/* Defined later in this TU (a local sibling still in asm). */
void UwamonoBgmFunc(UwamonoCommonUnit *unit);

void UwamonoCommonFunc(UwamonoCommonUnit *unit)
{
    UwamonoTimers *timers = &unit->timers;

    if (unit->serial < 0x1000) {
        MAP_updateUnitPartsSequence(unit);
    } else {
        MAP_updateUnitSequence(unit);
    }

    if (timers->heightCheckFlag != 0) {
        CheckUwamonoHeight(unit);
    }

    unit->linkedUnit->position.x = unit->position.x;
    unit->linkedUnit->position.y = unit->position.y;
    unit->linkedUnit->position.z = unit->position.z;

    if (timers->bgmTimer > 0) {
        UwamonoBgmFunc(unit);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", UwamonoBgmFunc);

/*
 * UwamonoBgmFadeOut's own additive view of the same MapUnit[] record the
 * structs above already partially name; only the fields it reads are named
 * here. flags (+0x00) is the same field UwamonoMapUnit and DrillMapUnit
 * name; bit 0x10000 is the created/active flag CreateUwamonoCommon sets.
 * seChannel (+0xA0) is the same field ClearUwamonoEffect's UwamonoEffectUnit
 * names, used the same way (+1 as a channel-shaped argument). bgmTimer
 * (+0x1E8) is the same field UwamonoTimers/UwamonoEffectTimers name; here
 * its high halfword is a SoundWork effect-bank index and its low halfword
 * a sub-id, combined below into the effect_id SsdFadeoutEffect takes.
 */
typedef struct UwamonoBgmFadeOutUnit {
    u32 flags;                        /* +0x00 */
    unsigned char unmodeled_04[0x9c];
    unsigned char seChannel;          /* +0xA0 */
    unsigned char unmodeled_a1[0x147];
    int bgmTimer;                     /* +0x1E8 */
    unsigned char unmodeled_1ec[0x114];
} UwamonoBgmFadeOutUnit;

extern unsigned char MapUnit[];

/*
 * UwamonoBgmFadeOut's own additive view of the global sound-engine
 * SoundWork record src/main/xgl_sound.c names in full (main:0x004a8140,
 * struct SoundWork); only the low halfword of each 4-byte effect-bank entry
 * at +0x20 is read here -- the same field xgl_sound.c's SoundEffectBankEntry
 * calls handle.
 */
typedef struct UwamonoSoundWorkView {
    unsigned char unmodeled_00[0x20];
    struct {
        unsigned short handle;        /* +0x20 + bank * 4 */
        unsigned char unmodeled_02[2];
    } effectBanks[16];
} UwamonoSoundWorkView;

extern UwamonoSoundWorkView SoundWork;

/* Defined in src/main/ssd_1.c (main/tu ssd_1), already C. */
extern void SsdFadeoutEffect(int effect_id, int source_id, int fade_time);

void UwamonoBgmFadeOut(void)
{
    UwamonoBgmFadeOutUnit *unit;
    int i;
    int bgmTimer;
    int bank;
    unsigned short handle;

    unit = (UwamonoBgmFadeOutUnit *) MapUnit;
    for (i = 0; i < 0x40; i++, unit++) {
        if (!(unit->flags & 0x10000)) {
            continue;
        }
        bgmTimer = unit->bgmTimer;
        if (bgmTimer < 0) {
            continue;
        }
        if (bgmTimer > 0xFFFFF) {
            continue;
        }
        bank = bgmTimer >> 16;
        handle = SoundWork.effectBanks[bank].handle;
        if (handle == 0xFFFF) {
            continue;
        }
        SsdFadeoutEffect((handle << 16) + (bgmTimer & 0xFFFF), 0x12C, unit->seChannel + 1);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", CheckUwamonoHeight);

int HitCheckUwamonoAt(unsigned char *actor, char *unit);

int HitCheckUwamono(unsigned char *actor)
{
    int i;
    int off;
    AimUnitId *id;
    HitMapUnitSub *sub;
    char *unit;
    char *base;

    for (i = 0, off = 0; i < 64; i++, off += 0x300) {
        sub = &((HitMapUnit *)MapUnit)[i].sub;
        base = (char *)MapUnit;
        id = (AimUnitId *)((char *)&((HitMapUnit *)MapUnit)[i] + 0xA0);
        if (id->serial == -1) continue;
        unit = (char *)(off + (int)base);
        if (id->kind == actor[0xA0]) continue;
        if (((HitMapUnit *)MapUnit)[i].flags & 0x20000) continue;
        if (!(((HitMapUnit *)MapUnit)[i].flags & 0x100000)
            && sub->hit != 0
            && (((HitMapUnit *)MapUnit)[i].flags & 0x10000)
            && HitCheckUwamonoAt(actor, unit) != 0) {
            return i;
        }
    }
    return -1;
}

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", HitCheckUwamonoAt);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", HitCheckBoxUwamono);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", DrawRadar);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", DrawSprite);

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", SortLine);

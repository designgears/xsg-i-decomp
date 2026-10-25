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
    unsigned char unmodeled_04[0x0c];
    Vector4 position;                 /* +0x10 */
    unsigned char unmodeled_20[4];
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
    unsigned char unmodeled_a3;
    short serial;                     /* +0xA4 */
    unsigned char unmodeled_a6[2];
    short sequenceNo;                 /* +0xA8 */
    short sequenceSub;                /* +0xAA */
    unsigned char unmodeled_ac[0xf8];
    signed char signal;               /* +0x1A4 */
    unsigned char unmodeled_1a5[0x28];
    signed char crossType;            /* +0x1CD */
} UwamonoMapUnit;

/* Runtime-loaded object message with a hexadecimal unit identifier. */

const char D_004CA028[24] = "\xBE\xEF\xC3\xF3\xBE\xE5\xCA\xAA\xA5\xED\xA1\xBC\xA5\xC9 %x\n";

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

typedef struct UwamonoSoundActor {
    unsigned char unmodeled_00[0x10];
    Vector4 position;
} UwamonoSoundActor;

typedef UwamonoSoundActor AimHeightReference;

typedef struct UwamonoGameLoopState {
    unsigned char unmodeled_00[4];
    UwamonoSoundActor *actor; /* +0x04 */
    unsigned char unmodeled_08[0x14];
    int enemyUpdateState;             /* +0x1C */
    unsigned int aimHeightFlags;      /* +0x20 */
    unsigned char unmodeled_24[0x30];
    MapPartsRoot *partsRoot;          /* +0x54 */
    unsigned char unmodeled_58[0x8c];
    short shootUnit[64];              /* +0xE4 */
    unsigned char unmodeled_164[0x29ddd];
    signed char aimMapUnitIndex;
    unsigned char unmodeled_29f42[2];
    float selectionDistance;      /* +0x29F41 */
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

typedef struct InitMapPartsUnit {
    unsigned int flags;
    void (*update)(struct InitMapPartsUnit *);
    unsigned char unmodeled_08[0xd8];
    int uwaresId;
    unsigned char unmodeled_e4[0xbc];
    short mapType;
} InitMapPartsUnit;

extern void InitMapDoor(InitMapPartsUnit *unit);

extern void InitMapKoware(InitMapPartsUnit *unit);

extern void InitDrill(InitMapPartsUnit *unit);



#define UWAMONO_SYMBOL_PHASE_STEP 0.05f

/*
 * HitCheckMapUnitPos (main:0x002c02f0): a thin tail call into
 * HitCheckMapUnitPosSize with its distance threshold fixed at 0.0f (a
 * true `j`, no `jal`, so this function's own return value is whatever
 * HitCheckMapUnitPosSize returns).
 */

extern int HitCheckMapUnitPosSize(Vector4 *pos, float size);

/*
 * *(UwamonoLinkedUnit.position below) is a separate record UwamonoCommonFunc
 * keeps synchronized with this unit's position; only the field it writes is
 * named.
 */

typedef struct UwamonoLinkedUnit {
    float direction[3];               /* +0x00 */
    unsigned char unmodeled_0c[0x24];
    Vector4 position; /* +0x30, set from the owning unit's position */
} UwamonoLinkedUnit;

typedef struct UwamonoTimers {
    unsigned char unmodeled_00[0xa];
    signed char projectSound;         /* +0x0A (record +0x1AA) */
    unsigned char unmodeled_0b[0x23];
    signed char heightCheckFlag; /* +0x2E (record +0x1CE) */
    unsigned char unmodeled_2f[0x19];
    int bgmTimer;                /* +0x48 (record +0x1E8) */
} UwamonoTimers;

typedef union UwamonoAlignedVector {
    Vector4 value;
    long long words[2];
} UwamonoAlignedVector;

typedef struct UwamonoCommonUnit {
    unsigned char unmodeled_00[0x10];
    UwamonoAlignedVector position;                 /* +0x10 */
    unsigned char unmodeled_20[0x60];
    UwamonoLinkedUnit *linkedUnit;     /* +0x80 */
    unsigned char unmodeled_84[0x1c];
    unsigned char seChannel;
    unsigned char unmodeled_a1[3];
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

typedef struct UwamonoMapSlot UwamonoMapSlot;


struct UwamonoMapSlot {
    unsigned int flags;
    void (*update)(UwamonoMapSlot *);
    unsigned char unmodeled_08[8];
    UwamonoAlignedVector position;
    unsigned char unmodeled_20[0x60];
    struct UwamonoBoxBasis *basis;
    unsigned char unmodeled_84[0x1e];
    unsigned char actionSub;
    unsigned char unmodeled_a3;
    short serial;
    unsigned char unmodeled_a6[0x0a];
    UwamonoAlignedVector dimensions;
    unsigned char unmodeled_c0[0x28];
    int effectCf[3];
    unsigned char unmodeled_f4[0xac];
    struct UwamonoMapCollisionState {
        unsigned char unmodeled_00[0x2d];
        signed char crossType;
        unsigned char unmodeled_2e[0x132];
    } state;
};

extern UwamonoMapSlot MapUnit[];

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


typedef struct UwamonoInitSymbolState {
    unsigned char unmodeled_00[0x2d];
    signed char state;                 /* +0x2d */
    unsigned char unmodeled_2e[0x16];
    int timer;                         /* +0x44 */
} UwamonoInitSymbolState;

typedef struct UwamonoInitSymbolUnit {
    int flags;                         /* +0x00 */
    void (*updateFunction)(UwamonoMapUnit *unit); /* +0x04 */
    unsigned char unmodeled_08[8];
    Vector4 position;                  /* +0x10 */
    float rotationX;                   /* +0x20 */
    float rotationY;                   /* +0x24 */
    float rotationZ;                   /* +0x28 */
    unsigned char unmodeled_2c[0x14];
    float matrix[4][4];                /* +0x40 */
    float (*savedMatrix)[4];           /* +0x80 */
    unsigned char unmodeled_84[0x5c];
    int uwaresId;                      /* +0xe0 */
    int xtxresId;                      /* +0xe4 */
    unsigned char unmodeled_e8[0xb8];
    UwamonoInitSymbolState timerState; /* +0x1a0 */
} UwamonoInitSymbolUnit;





extern void MAP_updateUnitSaveSymbol(UwamonoMapUnit *unit);

extern void MAP_updateUnitShopSymbol(UwamonoMapUnit *unit);

extern void MAP_updateUnitSymbol(UwamonoMapUnit *unit);


typedef struct UwamonoCreatedState {
    unsigned char unmodeled_00[5];
    signed char signal;
    unsigned char unmodeled_06[0x1e];
    float height;
    unsigned char unmodeled_28[4];
    signed char state;
    unsigned char unmodeled_2d[1];
    signed char heightCheckFlag;
    unsigned char unmodeled_2f[3];
    signed char target;
    unsigned char unmodeled_33[0x11];
    int timer;
    unsigned char unmodeled_48[0x38];
} UwamonoCreatedState;

typedef struct UwamonoCreatedUnit {
    unsigned char unmodeled_00[0x10];
    Vector4 position;
    float rotationX;
    float rotationY;
    float rotationZ;
    unsigned char unmodeled_2c[0x174];
    UwamonoCreatedState state;
} UwamonoCreatedUnit;

extern const char D_004CA1B0[32];

extern UwamonoCreatedUnit *MAP_createUnit(int index, int resourceId);

extern int MAP_loadUnitResource(UwamonoCreatedUnit *unit, int resourceId);

extern void InitMapTrap(UwamonoCreatedUnit *unit);

extern void *memset(void *destination, int value, unsigned int bytes);

typedef struct UwamonoMapUnitState {
    unsigned char unmodeled_00[0x2d];
    signed char state;                 /* +0x2d */
    unsigned char unmodeled_2e[4];
    signed char target;                /* +0x32 */
    unsigned char unmodeled_33[0x11];
    int timer;                         /* +0x44 */
} UwamonoMapUnitState;

typedef struct UwamonoKowareDimensions {
    unsigned long long lanes[2];
} UwamonoKowareDimensions;

typedef struct UwamonoKowareKind {
    signed char shape;
    unsigned char unmodeled_01[0xf];
    UwamonoKowareDimensions dimensions;
    signed char target;
    unsigned char unmodeled_21[7];
    unsigned long long unmodeled_28;
} UwamonoKowareKind;

typedef struct UwamonoKowareUnit {
    unsigned int flags;
    void (*updateFunction)(void *unit);
    unsigned char unmodeled_08[8];
    unsigned long long unmodeled_10;
    unsigned char unmodeled_18[0x98];
    UwamonoKowareDimensions dimensions;
    unsigned char unmodeled_c0[0xe0];
    UwamonoMapUnitState state;         /* +0x1a0 */
} UwamonoKowareUnit;

extern UwamonoKowareKind KowareKind_t[];

extern const char D_004CA1D0[];

extern void MAP_updateUnitKoware(void *unit);

void CreateUwamonoCommon(UwamonoMapUnit *unit);

typedef struct UwamonoCollisionUnit {
    unsigned char unmodeled_00[0x10];
    Vector4 position;
    unsigned char unmodeled_20[0x90];
    float radius;
    float height;
    unsigned char unmodeled_b8[0xe8];
    UwamonoMapUnitState state;
} UwamonoCollisionUnit;

typedef struct UwamonoCollisionActor {
    unsigned char unmodeled_00[0x10];
    Vector4 position;
    unsigned char unmodeled_20[0x9c8];
    float radius;
    unsigned char unmodeled_9ec[0x84];
} UwamonoCollisionActor;

extern float CheckDist2D(const Vector4 *first, const Vector4 *second);

extern int HitCheckBoxCorner(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit);

unsigned int HitCheckMapUnitPosAt(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit);

typedef struct UwamonoScanFields {
    unsigned char channel;            /* +0x00 */
    unsigned char unmodeled_01[3];
    short resourceId;                 /* +0x04 */
    unsigned char unmodeled_06[0xfa];
    UwamonoMapUnitState state;         /* +0x100; state byte at +0x12D */
} UwamonoScanFields;

typedef struct UwamonoScanUnit {
    unsigned int flags;
    unsigned char unmodeled_04[0x0c];
    Vector4 position;
    unsigned char unmodeled_20[0x80];
    UwamonoScanFields scan;            /* +0xA0 */
    unsigned char unmodeled_1e8[0x118];
} UwamonoScanUnit;

struct UwamonoBoxUnit;

int HitCheckBox(UwamonoCollisionActor *actor, struct UwamonoBoxUnit *unit);

extern const float D_004D7ED0;

extern const float D_004D7ED4;

extern const float D_004D7ED8;

extern const float D_004D7EDC;

typedef struct UwamonoBoxBasis {
    float x;
    unsigned char unmodeled_04[4];
    float z;
} UwamonoBoxBasis;

typedef struct UwamonoBoxCollisionState {
    unsigned char unmodeled_00[0x2d];
    signed char crossType;
} UwamonoBoxCollisionState;

typedef struct UwamonoBoxUnit {
    unsigned char unmodeled_00[0x10];
    Vector4 position;                 /* +0x10 */
    unsigned char unmodeled_20[0x60];
    UwamonoBoxBasis *basis;           /* +0x80 */
    unsigned char unmodeled_84[0x2c];
    Vector4 dimensions;               /* +0xB0 */
    unsigned char unmodeled_c0[0xe0];
    UwamonoBoxCollisionState state;   /* +0x1A0 */
} UwamonoBoxUnit;

extern int HitCheckCorner(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit);

typedef struct UwamonoCrossState {
    unsigned char unmodeled_00[0x2d];
    signed char crossType;
} UwamonoCrossState;

typedef struct UwamonoCrossUnit {
    unsigned char unmodeled_00[0x10];
    Vector4 position;
    unsigned char unmodeled_20[0x180];
    UwamonoCrossState state;
} UwamonoCrossUnit;

typedef Vector4 UwamonoNyuruVector;

typedef Vector4 UwamonoBoxVector;

typedef struct UwamonoNyuruActor {
    unsigned char unmodeled_00[0x10];
    UwamonoNyuruVector position;
    unsigned char unmodeled_20[0x10];
    UwamonoNyuruVector displacement;
} UwamonoNyuruActor;

extern int NyuruCircle(UwamonoNyuruActor *actor, UwamonoCollisionUnit *unit);

extern int NyuruMatrix(UwamonoNyuruActor *actor, UwamonoCollisionUnit *unit, int mode);

extern int HitCheckMapUnit(void *actor);

/*
 * Still assembler: it only reads `from` and `to`, and reads the unit's
 * position and radius (+0x10/+0x18, +0xB0), so it takes the collision view.
 */
extern int CrossCheckMapUnitCircle(const Vector4 *from, const Vector4 *to,
                                   UwamonoCollisionUnit *unit);

extern void xglVectorNormal(Vector4 *destination, const Vector4 *source);


extern int AreaCheckBox(const Vector4 *position, UwamonoMapSlot *unit, float radius);

extern int CrossCheckMapUnitAt(Vector4 *from, Vector4 *to, UwamonoCrossUnit *unit);

typedef struct UwamonoCrossActor {
    unsigned int flags;
    unsigned char unmodeled_04[0x0c];
    Vector4 position;
    unsigned char unmodeled_20[0x60];
    unsigned char actorIndex;
    unsigned char unmodeled_81[5];
    short resourceId;
    unsigned char unmodeled_88[0x848];
    void *collisionObject;
    unsigned char unmodeled_8d4[0x114];
    float radius;
    unsigned char unmodeled_9ec[0x84];
} UwamonoCrossActor;

extern UwamonoCrossActor actor[64];

extern const float D_004D7F18;

extern int CheckCrossCircle(float radius, const Vector4 *from, const Vector4 *to, const Vector4 *center);

extern float CheckCornerDist(const Vector4 *position, UwamonoCollisionUnit *unit);


typedef struct BrokenMapUnitState {
    unsigned char unmodeled_00[4];
    signed char signal;
    unsigned char unmodeled_05[0x3f];
    int brokenId;
} BrokenMapUnitState;

typedef struct BrokenMapUnit {
    unsigned int flags;
    unsigned char unmodeled_04[0xa0];
    short serial;
    unsigned char unmodeled_a6[0xfa];
    BrokenMapUnitState brokenState;
} BrokenMapUnit;

extern int CheckBrokenMapUnit(BrokenMapUnit *unit);

extern void xglSoundEffectPosID(int soundId, const float *position, int enabled, int channel);

/* Defined in src/main/ssd_1.c (main/tu ssd_1), already C. */

extern int HitCheckUwamonoAt(UwamonoBoxUnit *actor, UwamonoBoxUnit *unit);

typedef struct UwamonoScreenPoint {
    int x;
    int y;
    int z;
    int w;
} UwamonoScreenPoint;

typedef struct UwamonoSpritePacket {
    unsigned char unmodeled_00[0x30];
    UwamonoScreenPoint color0;
    UwamonoScreenPoint position0;
    UwamonoScreenPoint color1;
    UwamonoScreenPoint position1;
} UwamonoSpritePacket;

typedef struct UwamonoLinePacket {
    unsigned char unmodeled_00[0x20];
    UwamonoScreenPoint color0;
    UwamonoScreenPoint position0;
    UwamonoScreenPoint color1;
    UwamonoScreenPoint position1;
} UwamonoLinePacket;

typedef struct UwamonoVectorWords {
    long long lower;
    long long upper;
} UwamonoVectorWords;

typedef struct UwamonoEnemyLink {
    unsigned char unmodeled_00[0x4b];
    unsigned char unitId;
    unsigned char unmodeled_4c[0x3864];
} UwamonoEnemyLink;

typedef struct UwamonoEnemyUnit UwamonoEnemyUnit;

struct UwamonoEnemyUnit {
    unsigned int flags;
    void (*update)(UwamonoEnemyUnit *);
    unsigned char unmodeled_08[8];
    UwamonoAlignedVector position;
    unsigned char unmodeled_20[0x34];
    float heading;
    unsigned char unmodeled_58[0x28];
    unsigned char enemyIndex;
    unsigned char unmodeled_81[0x963];
    float actorHeading;
};

extern UwamonoEnemyLink enepc[16];

extern void ACT_updateEnemy(UwamonoEnemyUnit *unit);

typedef struct UwamonoWind {
    unsigned char mode;
    unsigned char unmodeled_01[7];
    long long unmodeled_08;
    Vector4 position;
    float decay;
    float strength;
    int timer;
    unsigned char unmodeled_2c[4];
} UwamonoWind;

typedef struct UwamonoWindUnit {
    long long unmodeled_00[2];
    Vector4 position;
    unsigned char unmodeled_20[0x94];
    float height;
} UwamonoWindUnit;

extern UwamonoWind uwaWind;


extern void EXM_SetWindStruct(UwamonoWind *wind);

#define D_004D7EC0 0.3f

#define D_004D7EC4 0.1f

typedef struct UwamonoBrokenSoundState {
    unsigned char unmodeled_00[0x32];
    signed char material;
    unsigned char unmodeled_33;
    int soundId;
} UwamonoBrokenSoundState;

typedef struct UwamonoBrokenSoundUnit {
    unsigned char unmodeled_00[0xa0];
    unsigned char seChannel;
    unsigned char unmodeled_a1[0xff];
    UwamonoBrokenSoundState state;
} UwamonoBrokenSoundUnit;

extern void xglSoundEffectNormalID(int soundId, int channel);

#define D_004D7F44 0.1f

int HitCheckBoxUwamono(UwamonoBoxUnit *actor, UwamonoBoxUnit *unit);

#define D_004D7F1C 0.6f

extern void nmlModelSetPartsVisible(void *resource, int partsId, int visible);

extern float Get_Angle(const Vector4 *first, const Vector4 *second);

extern float CheckDist3D(const Vector4 *first, const Vector4 *second);

#define D_004D7EB4 0.05f

#define D_004D7EB8 0.05f

void UnlockMapUnit(void);

/*
 * The hit and cross checks read a MapUnit[] slot through the collision view
 * (position, radius/height, shape byte); callers holding the slot as a
 * UwamonoMapSlot or UwamonoMapUnit pass it in that view, the same way
 * CheckNearMapUnit hands its scan slot to CheckCornerDist.
 */
extern int HitCheckMapUnitAt(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit);

void CalcNearCrossPointCircle(const Vector4 *first, const Vector4 *second,
                              UwamonoCollisionUnit *unit, Vector4 *result);

void CalcNearCrossPointBox(Vector4 *first, Vector4 *second,
                           UwamonoMapUnit *unit, void *result);

extern int CrossCheckMapUnitBox(Vector4 *first, Vector4 *second,
                                UwamonoCrossUnit *unit);

extern unsigned char uwamono_dat[];
extern unsigned char ItemBoxTbl[0x12C8];
extern unsigned char EvtItemTbl[0x7F80];
extern unsigned char *WorkEnd;
extern int xglCdReadFile(const char *name, void *buffer, int offset, int mode);
extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern char D_004DB6D8[];
extern char D_004DB6E0[];
typedef struct UwamonoNameTbl { unsigned char b[0x38]; } UwamonoNameTbl;
extern UwamonoNameTbl D_004C9F20;
extern const char D_004C9F58[];
extern const char D_004C9F88[];
extern const char D_004C9FB0[];
extern const char D_004C9FD0[];

void InitUwamonoSys(void)
{
    char sp[0x80];
    char *names[14];
    unsigned char *cur;
    int i;
    int used;
    unsigned char *work;

    *(UwamonoNameTbl *)names = D_004C9F20;
    cur = uwamono_dat;
    for (i = 0; i < 14; i++) {
        memset(sp, 0, 0x80);
        strcpy(sp, names[i]);
        strcat(sp, D_004DB6D8);
        uwares_tbl[i] = (int)cur;
        cur += xglCdReadFile(sp, cur, 0, 1);
        cur = (unsigned char *)(((int)cur + 0xF) & ~0xF);
        memset(sp, 0, 0x80);
        strcpy(sp, names[i]);
        strcat(sp, D_004DB6E0);
        xtxres_tbl[i] = (int)cur;
        cur += xglCdReadFile(sp, cur, 0, 1);
        cur = (unsigned char *)(((int)cur + 0xF) & ~0xF);
    }

    used = cur - uwamono_dat;
    if (used > 0x50000) {
        printf(D_004C9F58, used / 1024);
    } else {
        printf(D_004C9F88, used / 1024, i);
    }

    memset(ItemBoxTbl, 0, 0x12C8);
    memset(EvtItemTbl, 0, 0x7F80);
    work = WorkEnd;
    xglCdReadFile(D_004C9FB0, work, 0, 1);
    __builtin_memcpy(ItemBoxTbl, work, 0x12C8);
    xglCdReadFile(D_004C9FD0, work, 0, 1);
    __builtin_memcpy(EvtItemTbl, work, 0x7F80);

    GameLoopState.selectionDistance = 3.0f;
    GameLoopState.aimHeightFlags |= 3;
}

extern int unitSequence[];
extern signed char printflg;
extern const char D_004C9FF0[24];
extern const char D_004CA008[];
void InitUwamono(UwamonoCreatedUnit *arg0);

UwamonoCreatedUnit *Unit_CreateUwamono(int arg0, int arg1)
{
  char *sub;
  u8 *cand;
  u8 *base;
  UwamonoCreatedUnit *unit = 0;
  int new_var;
  char *new_var2;
  int i;
  int off;
  u8 *temp;
  if (arg1 <= (0x1000 - 1))
  {
    i = 0;
    base = (u8 *) MapUnit;
    cand = base + 0xA0;
    off = 0;
    do
    {
      new_var = 0xA0;
      if ((arg1 == (*((short *) ((cand + (i * 768)) + 4)))) && ((temp = (u8 *) (((unsigned int) (off + (i * 768))) + ((unsigned int) base)), ((*((unsigned int *) ((cand + (i * 768)) - new_var))) & 0x10000) != 0)))
      {
        if (printflg != 0)
        {
          printf(D_004C9FF0, arg1);
        }
        unit = (UwamonoCreatedUnit *) temp;
        break;
      }
      i++;
    }
    while (i < 0x40);
  }
  if (unit == 0)
  {
    unit = MAP_createUnit(arg0, arg1);
    if (unit == 0)
    {
      printf(D_004CA008);
      return 0;
    }
  }
  if ((((arg1 >= 0x1001) && (((u32) (arg1 - 0x7000)) >= 0x14U)) && (((u32) (arg1 - 0x7005)) >= 3U)) && (arg1 != 0x7013))
  {
    MAP_loadUnitResource(unit, arg1);
  }
  *((int *) (((char *) unit) + 0x0)) = 0x10004;
  *((int *) (((char *) unit) + 0x80)) = (int) (((char *) unit) + 0x40);
  unit->unmodeled_2c[0x75] = 0;
  *((unsigned short *) (((char *) unit) + 0xA8)) = 0;
  *((unsigned short *) (((char *) unit) + 0xAA)) = 0;
  unit->unmodeled_2c[0x76] = 0;
  unit->position.x = 0.0f;
  unit->position.y = 0.0f;
  unit->position.z = 0.0f;
  unit->position.w = 1.0f;
  sub = (char *) unit;
  sub += 0x1A0;
  memset(&unit->state, 0, 0x80U);
  sub[0x5] = -1;
  *((short *) (sub + 0x2)) = -1;
  sub[0x2C] = -1;
  sub[0x2D] = -1;
  sub[0x2E] = -1;
  sub[0x32] = -1;
  *((int *) (sub + 0x48)) = -1;
  *((int *) (sub + 0x44)) = -1;
  *((int *) (sub + 0x34)) = -1;
  *((int *) (sub + 0x38)) = -1;
  *((int *) (sub + 0x40)) = -1;
  *((short *) (sub + 0x30)) = -1;
  *((int *) (((char *) unit) + 0x4)) = (int) InitUwamono;
  new_var2 = ((char *) unitSequence) + (unit->unmodeled_2c[0x74] * 0x260);
  {
    char *seq = new_var2;
    *((float *) (seq + 0x25C)) = 1.0f;
    *((int *) (seq + 0x30)) = 0;
    *((int *) (seq + 0x0)) = 0;
    *((int *) (seq + 0x4)) = 0;
    *((int *) (seq + 0x240)) = 0;
    *((int *) (seq + 0x244)) = 0;
    *((int *) (seq + 0x248)) = 0;
    *((float *) (seq + 0x24C)) = 1.0f;
    *((float *) (seq + 0x250)) = 1.0f;
    *((int *) (seq + 0x254)) = 0;
    *((int *) (seq + 0x258)) = 0;
    *((int *) (seq + 0xC)) = 0;
    *((int *) (seq + 0x14)) = 0;
    *((int *) (seq + 0x18)) = 0;
    *((int *) (seq + 0x1C)) = 0;
    *((int *) (seq + 0x20)) = 0;
    *((int *) (seq + 0x24)) = 0;
    *((int *) (seq + 0x28)) = 0;
    *((int *) (seq + 0x2C)) = 0;
    *((float *) (seq + 0x25C)) = 1.0f;
  }
  return unit;
}

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

extern signed char printflg;
extern const char D_004CA040[];
extern const char D_004CA050[];

int GetPartsSize(void *arg0)
{
    char *u = (char *)arg0;
    char *sub = (char *)arg0 + 0x1A0;
    int kind = *(short *)(sub + 2);
    unsigned int r;
    if (*(signed char *)(sub + 0x2D) == (signed char)-1) {
        *(signed char *)(sub + 0x2D) = KowareKind_t[kind].shape;
    }
    if (*(float *)(sub + 0xC) > 0.0f) {
        signed char pf = printflg;
        *(float *)(u + 0xB0) = *(float *)(sub + 0xC) * 0.5f;
        *(float *)(u + 0xB4) = *(float *)(sub + 0x10);
        *(float *)(u + 0xB8) = *(float *)(sub + 0x14) * 0.5f;
        if (pf) {
            printf(D_004CA040);
            printf(D_004CA050, *(float *)(sub + 0xC), *(float *)(sub + 0x10), *(float *)(sub + 0x14));
        }
    } else {
        *(UwamonoKowareDimensions *)(u + 0xB0) = KowareKind_t[kind].dimensions;
    }
    *(float *)(u + 0xB0) = *(float *)(u + 0xB0) + *(float *)(sub + 0x18) * 0.5f;
    *(float *)(u + 0xB4) = *(float *)(u + 0xB4) + *(float *)(sub + 0x1C);
    *(float *)(u + 0xB8) = *(float *)(u + 0xB8) + *(float *)(sub + 0x20) * 0.5f;
    {
        signed char tg = *(signed char *)(sub + 0x32);
        if (tg == -1) {
            *(signed char *)(sub + 0x32) = KowareKind_t[kind].target;
        }
    }
    r = *(int *)(sub + 0x40);
    if (r == -1) {
        *(int *)(sub + 0x40) = *(int *)((char *)KowareKind_t + kind * 48 + 0x24);
    } else {
        return r;
    }
}

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

typedef struct SaveSymbolUnit {
    u32 flags;                        /* +0x00 */
    long (*update)(UwamonoMapUnit *); /* +0x04 */
    unsigned char unmodeled_08[8];
    float position[4];                /* +0x10 */
    float rotation[4];                /* +0x20 */
    unsigned char unmodeled_30[0x10];
    float matrix[4][4];               /* +0x40 */
    float (*matrixPtr)[4];            /* +0x80 */
    unsigned char unmodeled_84[0x5c];
    int uwaresId;                     /* +0xE0 */
    int xtxresId;                     /* +0xE4 */
    unsigned char unmodeled_e8[0xb8];
    struct SaveSymbolSub {
        unsigned char unmodeled_00[0x2d];
        signed char state;            /* +0x2D */
        unsigned char unmodeled_2e[0x16];
        int count;                    /* +0x44 */
    } sub;                            /* +0x1A0 */
} SaveSymbolUnit;

typedef struct InitUwamonoQuery {
    unsigned char unmodeled_00[8];
    unsigned short flags;
    unsigned char unmodeled_0a[0xe];
    void *header;
    unsigned char unmodeled_20[0x20];
} InitUwamonoQuery;
extern void UnduParamInit(void *param);
extern void *UnduDataGetHeader(int map_index, int unit_index);
extern float UnduCheck(float *sample, void *output, void *query);
extern void InitItemBox(UwamonoCreatedUnit *unit);
extern void InitItemSymbol(UwamonoCreatedUnit *unit);
extern void InitSpecialSymbol(UwamonoCreatedUnit *unit);
extern void InitShopSymbol(UwamonoCreatedUnit *unit);
extern void InitEvsSymbol(UwamonoCreatedUnit *unit);
extern void InitRetSymbol(UwamonoCreatedUnit *unit);
extern void InitSaveSymbol(SaveSymbolUnit *unit);
const char D_004CA060[40] = "Illegal UWA_NULL!  id=%d serial=%d\n";
const char D_004CA088[40] = "Illegal Uwamono Type %d\n";
extern void InitMapParts(InitMapPartsUnit *unit);

void InitUwamono(UwamonoCreatedUnit *arg0)
{
    InitUwamonoQuery q;
    char *sub = (char *)arg0 + 0x1A0;
    short type;

    ((UwamonoMapUnit *)arg0)->flags &= ~4;
    if (*(short *)sub == 0) {
        printf(D_004CA060, *(short *)((char *)arg0 + 0xA4), *((unsigned char *)arg0 + 0xA0));
        *(short *)((char *)arg0 + 0xA4) = -1;
        *(int *)((char *)arg0 + 0x4) = 0;
        return;
    }
    if (sub[0x2E] == -1) {
        sub[0x2E] = 0;
    }
    UnduParamInit(&q);
    q.header = UnduDataGetHeader(0, 0x8000);
    if (sub[0x2F] != 0) {
        q.flags |= 0x20;
    }
    if (UnduCheck((float *)((char *)arg0 + 0x10), 0, &q) == -1000.0f) {
        if (sub[0x2F] == 0) {
            sub[0x2E] = 0;
        }
    }
    if (*(short *)((char *)arg0 + 0xA4) < 0x1000) {
        InitMapParts((InitMapPartsUnit *)arg0);
        return;
    }
    type = *(short *)sub;
    switch (type) {
    case 4:
        InitItemBox(arg0);
        return;
    case 5:
        InitItemSymbol(arg0);
        return;
    case 3:
        InitMapTrap(arg0);
        return;
    case 6:
        InitSaveSymbol((SaveSymbolUnit *)arg0);
        return;
    case 7:
        InitShopSymbol(arg0);
        return;
    case 8:
        InitEvsSymbol(arg0);
        return;
    case 9:
        InitRetSymbol(arg0);
        return;
    case 10:
        InitSpecialSymbol(arg0);
        return;
    default:
        printf(D_004CA088, type);
        *(int *)((char *)arg0 + 0x4) = 0;
        return;
    }
}

const char D_004CA0D0[24] = "Illegal Parts Type %d\n";
const char D_004CA170[32] = "Create Save Symbol %f %f %f\n";
const char D_004CA190[32] = "Create Shop Symbol %f %f %f\n";

void InitMapParts(InitMapPartsUnit *unit)
{
    unit->uwaresId = 0;
    switch (unit->mapType) {
    case 2:
        InitMapKoware(unit);
        break;
    case 1:
        InitMapDoor(unit);
        break;
    case 20:
        InitDrill(unit);
        break;
    default:
        printf(D_004CA0D0, unit->mapType);
        unit->update = 0;
        break;
    }
}

extern signed char printflg;
int GetPartsSize(void *);
void SetHideObject(void *);
extern const char D_004CA0E8[];
extern const char D_004CA108[];
extern const char D_004CA138[];

void InitMapKoware(InitMapPartsUnit *unit)
{
    char *u = (char *)unit;
    char *sub = u + 0x1A0;
    short kind = *(short *)(sub + 2);
    MapPartsResource *res = GameLoopState.partsRoot->resource;
    short id;

    if (kind < 0) {
        printf(D_004CA0E8, kind);
    }
    GetPartsPos((GetPartsPosUnit *)u);
    GetPartsSize(u);
    if (*(int *)(sub + 0x44) == -1) {
        *(int *)(sub + 0x44) = 1;
    }
    *(int *)u = 0;
    *(void **)(u + 4) = MAP_updateUnitKoware;
    CreateUwamonoCommon((UwamonoMapUnit *)u);
    SetHideObject(u);
    id = *(short *)(u + 0xA4);
    if (id >= 0 && id < *(int *)((char *)res + 0x40)) {
        nmlModelSetPartsVisible(res, id, 1);
    } else {
        printf(D_004CA108, *(short *)(u + 0xA4));
        *(short *)(u + 0xA4) = -1;
        *(void **)(u + 4) = 0;
    }
    if (printflg) {
        printf(D_004CA138, *(short *)(u + 0xA4), kind,
               *(float *)(u + 0x10), *(float *)(u + 0x14), *(float *)(u + 0x18));
    }
}

extern void xglMatrixStackUnit(void);
extern void xglMatrixStackRotX(float angle);
extern void xglMatrixStackRotY(float angle);
extern void xglMatrixStackRotZ(float angle);
extern void xglMatrixStackSave(float matrix[4][4]);
void xglMatrixStackTrans(const float translation[4]);
extern signed char printflg;
void InitSaveSymbol(SaveSymbolUnit *unit)
{
  float new_var;
  float (*matrix)[4] = unit->matrix;
  struct SaveSymbolSub *sub = &unit->sub;
  float *position;
  {
    int u = uwares_tbl[5];
    unit->xtxresId = xtxres_tbl[5];
    unit->uwaresId = u;
  }
  position = unit->position;
  xglMatrixStackUnit();
  xglMatrixStackTrans(position);
  xglMatrixStackRotX(unit->rotation[0]);
  xglMatrixStackRotY(unit->rotation[1]);
  xglMatrixStackRotZ(unit->rotation[2]);
  xglMatrixStackSave(matrix);
  unit->matrixPtr = matrix;
  if (sub->state == (-1))
  {
    sub->state = 0;
  }
  sub->count = 0;
  unit->update = MAP_updateUnitSaveSymbol;
  if ((new_var = unit && unit) && unit)
  {
  }
  unit->flags |= 0x10000;
  unit->flags |= 0x20000000;
  if (printflg)
  {
    printf(D_004CA170, position[0], new_var = unit->position[1], unit->position[2]);
  }
}

void InitShopSymbol(UwamonoCreatedUnit *arg0)
{
  float new_var;
  SaveSymbolUnit *unit = (SaveSymbolUnit *)arg0;
  float (*matrix)[4] = unit->matrix;
  struct SaveSymbolSub *sub = &unit->sub;
  float *position;
  {
    int u = uwares_tbl[6];
    unit->xtxresId = xtxres_tbl[6];
    unit->uwaresId = u;
  }
  position = unit->position;
  xglMatrixStackUnit();
  xglMatrixStackTrans(position);
  xglMatrixStackRotX(unit->rotation[0]);
  xglMatrixStackRotY(unit->rotation[1]);
  xglMatrixStackRotZ(unit->rotation[2]);
  xglMatrixStackSave(matrix);
  unit->matrixPtr = matrix;
  if (sub->state == (-1))
  {
    sub->state = 0;
  }
  sub->count = 0;
  unit->update = MAP_updateUnitShopSymbol;
  if ((new_var = unit && unit) && unit)
  {
  }
  unit->flags |= 0x10000;
  unit->flags |= 0x40000000;
  if (printflg)
  {
    printf(D_004CA190, position[0], unit->position[1], unit->position[2]);
  }
}

extern double fptodp(float);

void InitEvsSymbol(UwamonoCreatedUnit *arg0)
{
  SaveSymbolUnit *unit = (SaveSymbolUnit *)arg0;
  float (*matrix)[4] = unit->matrix;
  struct SaveSymbolSub *sub = &unit->sub;
  float *position;
  position = unit->position;
  xglMatrixStackUnit();
  xglMatrixStackTrans(position);
  xglMatrixStackRotX(unit->rotation[0]);
  xglMatrixStackRotY(unit->rotation[1]);
  xglMatrixStackRotZ(unit->rotation[2]);
  xglMatrixStackSave(matrix);
  unit->matrixPtr = matrix;
  if (sub->state == (-1))
  {
    sub->state = 0;
  }
  unit->update = (long (*)(UwamonoMapUnit *)) MAP_updateUnitSymbol;
  { float _sb = arg0 != 0; if (_sb) {} }
  sub->count = 0;
  unit->flags |= 0x10000;
  if (printflg)
  {
    printf(D_004CA190, fptodp(position[0]), fptodp(unit->position[1]), fptodp(unit->position[2]));
  }
}

extern double fptodp(float f);

void InitRetSymbol(UwamonoCreatedUnit *arg0)
{
  SaveSymbolUnit *unit = (SaveSymbolUnit *)arg0;
  float (*matrix)[4] = unit->matrix;
  struct SaveSymbolSub *sub = &unit->sub;
  float *position;
  position = unit->position;
  xglMatrixStackUnit();
  xglMatrixStackTrans(position);
  xglMatrixStackRotX(unit->rotation[0]);
  xglMatrixStackRotY(unit->rotation[1]);
  xglMatrixStackRotZ(unit->rotation[2]);
  xglMatrixStackSave(matrix);
  unit->matrixPtr = matrix;
  if (sub->state == (-1))
  {
    sub->state = 0;
  }
  unit->update = (long (*)(UwamonoMapUnit *)) MAP_updateUnitSymbol;
  { float _sb = arg0 != 0; if (_sb) {} }
  sub->count = 0;
  unit->flags |= 0x10000;
  if (printflg)
  {
    printf(D_004CA190, fptodp(position[0]), fptodp(unit->position[1]), fptodp(unit->position[2]));
  }
}

extern void MAP_updateUnitEnemy(UwamonoEnemyUnit *unit);
extern void MAP_updateUnitHide(UwamonoMapSlot *unit);
union _pfv_act { struct { char _p[4]; void * v; } o_4; struct { char _p[16]; long long v; } o_10; struct { char _p[24]; long long v; } o_18; struct { char _p[80]; int v; } o_50; struct { char _p[84]; int v; } o_54; struct { char _p[88]; int v; } o_58; };

void SetHideObject(void *arg0)
{
  signed char idx;
  MapPartsResource *res;
  UwamonoCrossActor *act;
  unsigned char new_var;
  UwamonoMapSlot *slot;
  short serial;
  int effect;
  unsigned char *ep;
  unsigned char *sub;
  char *new_var2;
  signed char new_var3;
  sub = ((unsigned char *) arg0) + 0x1A0;
  idx = *((signed char *) (sub + 0x2C));
  if ((-1) != idx)
  {
    act = &actor[idx];
    new_var = *((unsigned char *) (((char *) arg0) + 0xA0));
    ((union _pfv_act *) act)->o_4.v = MAP_updateUnitEnemy;
    ep = ((unsigned char *) (&enepc->unmodeled_00[0x40])) + (act->actorIndex * 0x38B0);
    ep[0x8] = 0;
    ep[0xB] = new_var;
    {
      long long p10 = *((long long *) (((char *) arg0) + 0x10));
      long long p18 = *((long long *) (((char *) arg0) + 0x18));
      ((union _pfv_act *) act)->o_10.v = p10;
      ((union _pfv_act *) act)->o_18.v = *((long long *) (((char *) arg0) + 0x18));
    }
    ((union _pfv_act *) act)->o_50.v = 0;
    ((union _pfv_act *) act)->o_54.v = 0;
    ((union _pfv_act *) act)->o_58.v = 0;
    ACT_updateEnemy((UwamonoEnemyUnit *) act);
    act->flags |= 8;
  }
  new_var3 = *((signed char *) (sub + 0x5));
  idx = new_var3;
  if (idx != (-1))
  {
    slot = &MapUnit[idx];
    res = GameLoopState.partsRoot->resource;
    slot->unmodeled_a3 = *((unsigned char *) (((char *) arg0) + 0xA0));
    effect = slot->effectCf[0];
    slot->position.words[0] = *((long long *) (((char *) arg0) + 0x10));
    slot->update = MAP_updateUnitHide;
    slot->flags |= 0x100004;
    slot->position.words[1] = *((long long *) (((char *) arg0) + 0x18));
    new_var2 = ((char *) slot) + 0x1A0;
    *((int *) (((char *) slot) + 0x8)) = 0;
    if (effect != 0)
    {
      *((unsigned char *) (effect + 0xA9A)) = 0;
    }
    if ((*((short *) new_var2)) == 2)
    {
      serial = slot->serial;
      if ((serial >= 0) && (serial < (*((int *) (((char *) res) + 0x40)))))
      {
        nmlModelSetPartsVisible(res, (int) serial, 0);
        return;
      }
      printf(D_004CA108, (short) ((unsigned short) slot->serial), effect, res);
      slot->serial = -1;
      slot->update = 0;
    }
  }
}

void GameStateRestoreKoware(void)
{
    UwamonoGameLoopState *state;
    int index;
    index = 0;
    while (index < 64 && GameLoopState.shootUnit[index] != -1) {
        state = &GameLoopState;
        nmlModelSetPartsVisible(state->partsRoot->resource, state->shootUnit[index], 0);
        ++index;
    }
}

void SetMapUwamono(void)
{
    UwamonoGameLoopState *state = &GameLoopState;
    UwamonoMapSlot *unit = (UwamonoMapSlot *) MapUnit;
    short *shootUnit = state->shootUnit;
    int remaining = 63;

    do {
        *shootUnit++ = -1;
        if (unit->flags & 0x10000) {
            if (unit->update)
                unit->update(unit);
        }
        --remaining;
        ++unit;
    } while (remaining >= 0);
    UnlockMapUnit();
}

void ResetShootSys(void)
{
    UwamonoGameLoopState *state = &GameLoopState;
    short emptyShootUnitId = -1;
    int index = 0x3f;
    short *shootUnit = &state->shootUnit[0x3f];

    do {
        index--;
        *shootUnit = emptyShootUnitId;
        shootUnit--;
    } while (index >= 0);

    UnlockMapUnit();
}

UwamonoCreatedUnit *CreateUwamono(float x, float y, float z, float rotationY, int resourceId)
{
    UwamonoCreatedUnit *unit = MAP_createUnit(-1, resourceId);
    UwamonoCreatedState *state;
    int inactiveValue;

    if (unit == 0) {
        printf(D_004CA1B0);
        return 0;
    }
    MAP_loadUnitResource(unit, resourceId);
    state = &unit->state;
    memset(state, 0, sizeof(*state));
    inactiveValue = -1;
    state->height = 3.0f;
    state->heightCheckFlag = 1;
    state->signal = inactiveValue;
    state->state = inactiveValue;
    state->target = inactiveValue;
    state->timer = 1;
    unit->position.x = x;
    unit->position.y = y;
    unit->position.z = z;
    unit->position.w = 1.0f;
    unit->rotationY = rotationY;
    unit->rotationX = 0.0f;
    unit->rotationZ = 0.0f;
    InitMapTrap(unit);
    return unit;
}

UwamonoKowareUnit *CreateKowaremono(short resourceId, short kindIndex)
{
    UwamonoKowareUnit *unit = (UwamonoKowareUnit *)MAP_createUnit(-1, resourceId);
    UwamonoMapUnitState *state;
    if (unit == 0) {
        printf(D_004CA1D0);
        return 0;
    }
    GetPartsPos((GetPartsPosUnit *)unit);
    state = &unit->state;
    if (state->state == -1) {
        state->state = KowareKind_t[kindIndex].shape;
    }
    unit->dimensions = KowareKind_t[kindIndex].dimensions;
    if (state->target == -1) {
        state->target = KowareKind_t[kindIndex].target;
    }
    unit->flags = 0;
    CreateUwamonoCommon((UwamonoMapUnit *)unit);
    unit->updateFunction = MAP_updateUnitKoware;
    return unit;
}

void CreateUwamonoCommon(UwamonoMapUnit *unit)
{
    unit->actionNo = 0;
    unit->flags |= 0x10000;
    unit->sequenceNo = 0;
    unit->sequenceSub = 0;
    unit->actionSub = 0;
}

extern void DrawActiveCursol(void *unit);
extern void UwamonoCommonFunc(UwamonoCommonUnit *unit);
extern void Vibration_Set_Strong(int a, int b, int c);
extern void UwamonoBrokenSe(UwamonoBrokenSoundUnit *unit);
extern void *sefCreateEffectCf(int, int, int);
extern void SetUwaWind(UwamonoWindUnit *unit);
extern void CallMethod_I(const char *name, int arg);
extern void EXM_ClearWindStruct(void);
extern const char D_004DB6E8[32];
extern const char D_004CA1F8[32];

void MAP_updateUnitKoware(void *unit) {
    char *temp_17;
    int temp_2;
    short temp_5_2;
    int var_6;
    unsigned char temp_3;

    temp_17 = (char *)unit + 0x1A0;
    if (*(int *)(temp_17 + 0x44) != 0) {
        DrawActiveCursol(unit);
    }
    UwamonoCommonFunc((UwamonoCommonUnit *) unit);
    temp_3 = *(unsigned char *)((char *)unit + 0xA2);
    switch (temp_3) {
    case 0:
        if (*(signed char *)(temp_17 + 0x4) != 0) {
            *(unsigned char *)((char *)unit + 0xA2) = 1;
            for (var_6 = 0; var_6 < 0x40; var_6++) {
                if (GameLoopState.shootUnit[var_6] == -1) {
                    GameLoopState.shootUnit[var_6] = *(unsigned short *)((char *)unit + 0xA4);
                    break;
                }
            }
            Vibration_Set_Strong(0xFF, 0x20, 0x1E);
            UwamonoBrokenSe((UwamonoBrokenSoundUnit *) unit);
            sefCreateEffectCf(*(int *)(temp_17 + 0x40), (int)((char *)unit + 0x10), (int)((char *)unit + 0x20));
            ClearUwamonoEffect((UwamonoEffectUnit *) unit);
            SetUwaWind((UwamonoWindUnit *) unit);
            temp_5_2 = *(short *)(temp_17 + 0x30);
            if (temp_5_2 != -1) {
                GameLoopState.enemyUpdateState = 0;
                CallMethod_I(D_004DB6E8, (int) temp_5_2);
                return;
            }
        }
        return;
    case 1:
        temp_2 = *(unsigned short *)((char *)unit + 0xA8) + 1;
        *(short *)((char *)unit + 0xA8) = temp_2;
        if ((short)temp_2 >= 3) {
            nmlModelSetPartsVisible(GameLoopState.partsRoot->resource, *(short *)((char *)unit + 0xA4), 0);
            *(int *)unit = *(int *)unit | 0x20000;
        }
        if (*(int *)(temp_17 + 0x3C) < *(short *)((char *)unit + 0xA8)) {
            if (printflg != 0) {
                printf(D_004CA1F8, *(short *)((char *)unit + 0xA4), *(unsigned char *)((char *)unit + 0xA0));
            }
            *(int *)unit = 0;
            *(short *)((char *)unit + 0xA4) = -1;
            *(void **)((char *)unit + 0x4) = 0;
            do { EXM_ClearWindStruct(); } while (0);
        }
        return;
    }
}

void MAP_updateUnitEnemy(UwamonoEnemyUnit *unit)
{
    unsigned char linkedUnitId = enepc[unit->enemyIndex].unitId;
    UwamonoGameLoopState *state;

    if (!MapUnit[linkedUnitId].actionSub) {
        unit->flags |= 8;
    } else {
        unit->position = MapUnit[linkedUnitId].position;
        unit->flags &= ~8;
        unit->update = ACT_updateEnemy;
        state = &GameLoopState;
        state->enemyUpdateState = 0;
        unit->actorHeading = (unit->heading = Get_Angle(&unit->position.value, &state->actor->position));
    }
}

extern void MAP_updateUnitItemBox(void *unit);
extern void MAP_drawUnitItemBox(void *unit);
extern void MAP_updateUnitItemSymbol(void *unit);
extern void MAP_updateUnitTrap(void *unit);

void MAP_updateUnitHide(UwamonoMapSlot *unit)
{
  unsigned char idx = unit->unmodeled_a3;
  char *st = (char *) &unit->state;
  void *res;
  short serial;
  int one;
  if (MapUnit[idx].actionSub == 0)
  {
    unit->flags |= 0x100000;
    *((unsigned char *) (st + 0x2E)) = 0;
    if (unit->effectCf[0] != 0)
    {
      *((unsigned char *) (unit->effectCf[0] + 0xA9A)) = 0;
    }
    return;
  }
  unit->position = MapUnit[idx].position;
  unit->flags &= ~0x100000;
  one = 1;
  *((unsigned char *) (st + 0x2E)) = one;
  if (unit->effectCf[0] != 0)
  {
    *((unsigned char *) (unit->effectCf[0] + 0xA9A)) = one;
  }
  *((float *) (((char *) unit->basis) + 0x30)) = unit->position.value.x;
  *((float *) (((char *) unit->basis) + 0x34)) = unit->position.value.y;
  *((float *) (((char *) unit->basis) + 0x38)) = unit->position.value.z;
  switch (*((short *) st))
  {
    case 4:
      unit->update = (void (*)(UwamonoMapSlot *)) MAP_updateUnitItemBox;
      *((void **) (unit->unmodeled_08)) = MAP_drawUnitItemBox;
      break;

    case 5:
      unit->flags &= ~4;
      unit->update = (void (*)(UwamonoMapSlot *)) MAP_updateUnitItemSymbol;
      break;

    case 2:
      res = GameLoopState.partsRoot->resource;
      unit->flags &= ~4;
      unit->update = (void (*)(UwamonoMapSlot *)) MAP_updateUnitKoware;
      serial = unit->serial;
      if ((serial >= 0) && (serial < (*((int *) (((char *) res) + 0x40)))))
      {
        nmlModelSetPartsVisible(res, serial, 1);
      }
      else
      {
        printf(D_004CA108, (short) (unsigned short) unit->serial);
        unit->serial = -1;
        unit->update = 0;
      }
      break;

    case 3:
      unit->flags &= ~4;
      unit->update = (void (*)(UwamonoMapSlot *)) MAP_updateUnitTrap;
      break;
  }
}

void MAP_updateUnitSaveSymbol(UwamonoMapUnit *unit)
{
    unit->symbolPhase += D_004D7EB4;
    if (CheckDist3D(&GameLoopState.actor->position, &unit->position) < 0.0f)
        return;
}

void MAP_updateUnitShopSymbol(UwamonoMapUnit *unit)
{
    unit->symbolPhase += D_004D7EB8;
    if (CheckDist3D(&GameLoopState.actor->position, &unit->position) < 0.0f)
        return;
}

void MAP_updateUnitSymbol(UwamonoMapUnit *unit)
{
    unit->symbolPhase = unit->symbolPhase + UWAMONO_SYMBOL_PHASE_STEP;
}

void SetUwaWind(UwamonoWindUnit *unit)
{
    UwamonoWind *wind = &uwaWind;
    float heightDelta;

    wind->mode = 2;
    __builtin_memcpy((UwamonoVectorWords *) &wind->position,
                     (const UwamonoVectorWords *) &unit->position, sizeof(Vector4));
    heightDelta = unit->height * 0.5f;
    wind->position.w = D_004D7EC0;
    wind->decay = D_004D7EC4;
    wind->strength = 1.0f;
    wind->timer = 0;
    wind->position.y += heightDelta;
    EXM_SetWindStruct(wind);
}

void UwamonoBrokenSe(UwamonoBrokenSoundUnit *unit)
{
    UwamonoBrokenSoundState *state = &unit->state;
    int soundId = state->soundId;
    int channel = unit->seChannel + 1;

    if (soundId != -1) {
        xglSoundEffectNormalID(soundId, channel);
        return;
    }
    switch (state->material) {
    case 0:
        xglSoundEffectNormalID(0x10003, channel);
        break;
    case 1:
        xglSoundEffectNormalID(0x10006, channel);
        break;
    case 2:
        xglSoundEffectNormalID(0x10007, channel);
        break;
    default:
        xglSoundEffectNormalID(0x10003, channel);
        break;
    }
}

extern void *sefCreateEffectCf(int, int, int);
extern f32 D_004D7EC8;
extern f32 D_004D7ECC;

void DrawActiveCursol(void *arg0) {
    void *var_4;
    f32 var_f1;
    f32 scale;
    f32 avg;

    if (((*(signed char *)((char *)(arg0) + 0x1A4)) == 1) || !((*(int *)((char *)(arg0) + 0x0)) & 0x40000)) {
        if (*(int *)((char *)(arg0) + 0xEC) != 0) {
            sefDeleteEffectCf(*(int *)((char *)(arg0) + 0xEC));
            *(int *)((char *)(arg0) + 0xEC) = 0;
        }
        if (*(int *)((char *)(arg0) + 0xF0) != 0) {
            sefDeleteEffectCf(*(int *)((char *)(arg0) + 0xF0));
            *(int *)((char *)(arg0) + 0xF0) = 0;
        }
    } else if (GameLoopState.aimMapUnitIndex == *(unsigned char *)((char *)(arg0) + 0xA0)) {
        if (*(int *)((char *)(arg0) + 0xEC) != 0) {
            sefDeleteEffectCf(*(int *)((char *)(arg0) + 0xEC));
            *(int *)((char *)(arg0) + 0xEC) = 0;
        }
        if (*(int *)((char *)(arg0) + 0xF0) == 0) {
            var_4 = sefCreateEffectCf(0x2E3, 0, 0);
            if (var_4 != 0) {
                avg = (((*(f32 *)((char *)(arg0) + 0xB0)) + (*(f32 *)((char *)(arg0) + 0xB4))) + (*(f32 *)((char *)(arg0) + 0xB8)));
                scale = D_004D7EC8;
                var_f1 = avg / 3.0f;
                var_f1 = var_f1 * scale;
                *(void **)((char *)(var_4) + 0x6C4) = arg0;
                *(int *)((char *)(var_4) + 0xA8C) = (((*(int *)((char *)(var_4) + 0xA8C)) & ~0x400) | 4);
                if (var_f1 < scale) {
                    var_f1 = scale;
                }
                if (var_f1 > 2.0f) {
                    var_f1 = 2.0f;
                }
                *(void **)((char *)(arg0) + 0xF0) = var_4;
                goto block_28;
            }
        }
    } else {
        if (*(int *)((char *)(arg0) + 0xF0) != 0) {
            sefDeleteEffectCf(*(int *)((char *)(arg0) + 0xF0));
            *(int *)((char *)(arg0) + 0xF0) = 0;
        }
        if (*(int *)((char *)(arg0) + 0xEC) == 0) {
            var_4 = sefCreateEffectCf(0x2E2, 0, 0);
            if (var_4 != 0) {
                avg = (((*(f32 *)((char *)(arg0) + 0xB0)) + (*(f32 *)((char *)(arg0) + 0xB4))) + (*(f32 *)((char *)(arg0) + 0xB8)));
                scale = D_004D7ECC;
                var_f1 = avg / 3.0f;
                var_f1 = var_f1 * scale;
                *(void **)((char *)(var_4) + 0x6C4) = arg0;
                *(int *)((char *)(var_4) + 0xA8C) = (((*(int *)((char *)(var_4) + 0xA8C)) & ~0x400) | 4);
                if (var_f1 < scale) {
                    var_f1 = scale;
                }
                if (var_f1 > 2.0f) {
                    var_f1 = 2.0f;
                }
                *(void **)((char *)(arg0) + 0xEC) = var_4;
block_28:
                *(f32 *)((char *)(var_4) + 0xA8) = var_f1;
                *(f32 *)((char *)(var_4) + 0xA4) = var_f1;
                *(f32 *)((char *)(var_4) + 0xA0) = var_f1;
            }
        }
    }
}

int HitCheckMapUnit(void *actor)
{
    int index = 0;

    do {
        struct UwamonoMapCollisionState *state = &MapUnit[index].state;
        UwamonoMapSlot *unit = &MapUnit[index];
        if (unit->serial != -1) {
            unsigned int flags = unit->flags;
            if (!(flags & 0x100000) && state->crossType &&
                (flags & 0x10000) &&
                HitCheckMapUnitAt(actor, (UwamonoCollisionUnit *)unit)) {
                return index;
            }
        }
        ++index;
    } while (index < 64);
    return -1;
}

int HitCheckNyuru(void *pos, void *unit);
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
    struct UwamonoMapCollisionState *sub;
    UwamonoMapSlot *unit;

    for (i = 63; i >= 0; i--) {
        list[i] = -1;
    }
    n = 0;
    for (i = 0; i < 64; i++) {
        sub = &MapUnit[i].state;
        if (MapUnit[i].serial != -1
            && !(MapUnit[i].flags & 0x100000)
            && sub->crossType != 0
            && (MapUnit[i].flags & 0x10000)
            && HitCheckMapUnitAt((UwamonoCollisionActor *)q, (UwamonoCollisionUnit *)&MapUnit[i]) != 0) {
            list[n++] = i;
        }
    }
    if (n == 0) {
        return 0;
    }
    if (n == 1) {
        return HitCheckNyuru(q, &MapUnit[list[0]]);
    }
    for (i = 0; list[i] != -1; i++) {
        savePos = q->pos.v;
        saveMove = q->move.v;
        unit = &MapUnit[list[i]];
        switch (unit->state.crossType) {
        case 1:
            NyuruCircle((UwamonoNyuruActor *)q, (UwamonoCollisionUnit *)unit);
            break;
        case 2:
            NyuruMatrix((UwamonoNyuruActor *)q, (UwamonoCollisionUnit *)unit, 1);
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

int HitCheckMapUnitReverse(void *actor)
{
    int index = 64;

    do {
        struct UwamonoMapCollisionState *state = &MapUnit[index].state;
        UwamonoMapSlot *unit = &MapUnit[index];
        if (unit->serial != -1) {
            unsigned int flags = unit->flags;
            if (!(flags & 0x100000) && state->crossType &&
                (flags & 0x10000) &&
                HitCheckMapUnitAt(actor, (UwamonoCollisionUnit *)unit))
                return index;
        }
        --index;
    } while (index > 0);
    return -1;
}

void HitCheckMapUnitPos(int unit) {
    HitCheckMapUnitPosSize(unit, 0.0f);
}

int HitCheckMapUnitPosSize(Vector4 *pos, float size)
{
    UwamonoCollisionActor probe;
    int i;

    probe.position = *pos;
    probe.radius = size;
    for (i = 0; i < 64; i++) {
        struct UwamonoMapCollisionState *state = &MapUnit[i].state;
        UwamonoMapSlot *unit = &MapUnit[i];
        if (unit->serial != -1 && state->crossType &&
            (unit->flags & 0x10000) &&
            HitCheckMapUnitPosAt(&probe, (UwamonoCollisionUnit *)unit) != 0) {
            return i;
        }
    }
    return -1;
}

int HitCheckMapUnitPosReverse(Vector4 *position)
{
    UwamonoCollisionActor probe;
    int index = 64;

    probe.position = *position;
    probe.radius = 0.0f;
    do {
        struct UwamonoMapCollisionState *state = &MapUnit[index].state;
        UwamonoMapSlot *unit = &MapUnit[index];
        if (unit->serial != -1 && state->crossType &&
            (unit->flags & 0x10000) &&
            HitCheckMapUnitAt(&probe, (UwamonoCollisionUnit *)unit))
            return index;
        --index;
    } while (index > 0);
    return -1;
}

int HitCheckMapUnitAt(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit)
{
    UwamonoMapUnitState *state;

    if (unit->position.y + unit->height * D_004D7ED0 < actor->position.y) {
        return 0;
    }

    state = &unit->state;
    if (actor->position.y < unit->position.y - D_004D7ED4) {
        return 0;
    }

    switch (state->state) {
    case 1: {
        float distance = CheckDist2D(&actor->position, &unit->position);
        float collisionRadius = actor->radius + unit->radius;
        if (!(distance < collisionRadius)) {
            return 0;
        }
        return 1;
    }
    case 2:
        return HitCheckBox(actor, (struct UwamonoBoxUnit *)unit);
    }
    return 0;
}

unsigned int HitCheckMapUnitPosAt(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit)
{
    UwamonoMapUnitState *state = &unit->state;
    float unitY = unit->position.y;
    float actorY = actor->position.y;
    signed char shape;

    if (unitY + unit->height * D_004D7ED8 < actorY) {
        return 0;
    }
    if (actorY < unitY - D_004D7EDC) {
        return 0;
    }
    shape = state->state;
    switch (shape) {
    case 1:
        if (CheckDist2D(&actor->position, &unit->position) <
            actor->radius + unit->radius) {
            return 1;
        }
        return 0;
    case 2:
        return HitCheckBoxCorner(actor, unit);
    default:
        return 0;
    }
}

typedef struct { float x, y, z, w; } __attribute__((aligned(8))) BoxVec8;
typedef union {
    BoxVec8 v;
    long long q[2];
} BoxPair8;

int HitCheckBox(UwamonoCollisionActor *actor, UwamonoBoxUnit *unit)
{
    BoxVec8 size;
    BoxPair8 pos;
    float x;
    float z;
    float cx;
    float cz;
    float d;
    float r;
    float sz;
    float *dir;

    pos.q[1] = *(long long *)((char *)unit + 0x18);
    pos.q[0] = *(long long *)((char *)unit + 0x10);
    x = actor->position.x - pos.v.x;
    z = actor->position.z - pos.v.z;
    size = *(BoxVec8 *)&unit->dimensions;
    dir = (float *)unit->basis;
    r = actor->radius * 1.5f;
    d = x * dir[0] + z * dir[2];
    if (size.x + r < __builtin_fabsf(d)) {
        return 0;
    }
    x = d * dir[0] + pos.v.x;
    z = d * dir[2] + pos.v.z;
    cx = actor->position.x - x;
    cz = actor->position.z - z;
    d = cx * cx + cz * cz;
    sz = size.z + r;
    if (d < sz * sz) {
        return 1;
    }
    return 0;
}

typedef struct { float x, y, z, w; } __attribute__((aligned(8))) HbcVec8;
typedef union {
    HbcVec8 v;
    long long q[2];
} HbcPair8;
typedef struct {
    char pad0[0x10];
    HbcVec8 pos;
    char pad20[0x60];
    float *dir;
    char pad84[0x2c];
    HbcVec8 size;
} HbcBox;
typedef struct {
    char pad0[0x10];
    float x, y, z;
    char pad1c[0x9e8 - 0x1c];
    float r;
} HbcPos;

int HitCheckBoxCorner(UwamonoCollisionActor *query, UwamonoCollisionUnit *unit)
{
    HbcPos *p = (HbcPos *)query;
    HbcBox *b = (HbcBox *)unit;
    HbcVec8 size;
    HbcPair8 pos;
    float r;
    float d;
    float dist;
    float x;
    float z;
    float cx;
    float cz;
    float sz;
    int flag;
    pos.q[1] = *(long long *)((char *)b + 0x18);
    pos.q[0] = *(long long *)((char *)b + 0x10);
    x = p->x - pos.v.x;
    z = p->z - pos.v.z;
    size = b->size;
    r = p->r * 1.5f;
    d = x * b->dir[0] + z * b->dir[2];
    flag = 0;
    if ((size.x + r) < __builtin_fabsf(d))
    {
        return 0;
    }
    if (__builtin_fabsf(d) > size.x)
    {
        flag = 1;
    }
    x = d * b->dir[0] + pos.v.x;
    z = d * b->dir[2] + pos.v.z;
    cx = p->x - x;
    cz = p->z - z;
    dist = (cx * cx) + (cz * cz);
    sz = size.z + r;
    if (dist < sz * sz)
    {
        if (dist > (size.z * size.z))
        {
            if (flag == 1)
            {
                return HitCheckCorner(query, unit) != 0;
            }
        }
        return 1;
    }
    return 0;
}

int HitCheckCorner(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit) {
    Vector4 c1;
    Vector4 c2;
    Vector4 c3;
    Vector4 c4;
    float *m = *(float **)((char *)unit + 0x80);
    UwamonoAlignedVector pos = ((UwamonoCommonUnit *)unit)->position;
    UwamonoAlignedVector dim = *(UwamonoAlignedVector *)((char *)unit + 0xB0);

    c1.x = pos.value.x + (dim.value.x * m[0] + dim.value.z * m[8]);
    c1.y = pos.value.y;
    c1.z = pos.value.z + (dim.value.x * m[2] + dim.value.z * m[10]);
    if (CheckDist2D(&actor->position, &c1) < actor->radius * 1.5f) {
        return 1;
    }

    c2.x = pos.value.x - (dim.value.z * m[2] + dim.value.x * m[10]);
    c2.y = pos.value.y;
    c2.z = pos.value.z + (dim.value.z * m[0] + dim.value.x * m[8]);
    if (CheckDist2D(&actor->position, &c2) < actor->radius * 1.5f) {
        return 1;
    }

    c3.x = pos.value.x - (dim.value.x * m[0] + dim.value.z * m[8]);
    c3.y = pos.value.y;
    c3.z = pos.value.z - (dim.value.x * m[2] + dim.value.z * m[10]);
    if (CheckDist2D(&actor->position, &c3) < actor->radius * 1.5f) {
        return 1;
    }

    c4.x = pos.value.x + (dim.value.z * m[2] + dim.value.x * m[10]);
    c4.y = pos.value.y;
    c4.z = pos.value.z - (dim.value.z * m[0] + dim.value.x * m[8]);
    if (CheckDist2D(&actor->position, &c4) < actor->radius * 1.5f) {
        return 1;
    }
    return 0;
}

extern struct { unsigned char pad[0x2A]; unsigned short trig; } PadData;

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct UwamonoNyuruAlignedActor {
    unsigned char unmodeled_00[0x10];
    UwamonoAlignedVector position;
    unsigned char unmodeled_20[0x10];
    UwamonoAlignedVector displacement;
} UwamonoNyuruAlignedActor;

typedef union UwamonoAlignedScreenPoint {
    UwamonoScreenPoint value;
    int coordinates[4];
    long long words[2];
} UwamonoAlignedScreenPoint;

INCLUDE_ASM("asm/main/nonmatchings/init_uwamono_sys", HitCheckBoxTest);

int HitCheckNyuru(void *pos, void *unit)
{
    NyuruQuery *q = pos;
    NyuruVec savePos;
    NyuruVec saveMove;
    int r;

    savePos = q->pos.v;
    saveMove = q->move.v;
    r = -1;
    switch (((UwamonoMapSlot *)unit)->state.crossType) {
    case 1:
        if (((int (*)(void *, void *))NyuruCircle)(q, unit)) {
            r = HitCheckMapUnit(q);
            break;
        }
        return 0;
    case 2:
        if (((int (*)(void *, void *, int))NyuruMatrix)(q, unit, 0)) {
            r = HitCheckMapUnit(q);
            break;
        }
        return 0;
    }
    if (r != -1) {
        q->pos.v = savePos;
        q->move.v = saveMove;
        q->pos.f.x -= q->move.f.x;
        q->pos.f.z -= q->move.f.z;
        q->move.f.x = 0.0f;
        q->move.f.z = 0.0f;
        return 0;
    }
    return 1;
}

extern float xglSin(float angle);
extern float xglCos(float angle);
extern float D_004D7EE8;
static int NyuruCircle(UwamonoNyuruActor *actor, UwamonoCollisionUnit *unit) {
    Vector4 tmp;
    Vector4 nrm;
    float len;
    float ang;
    float ang2;
    float r;
    float h;

    len = __builtin_sqrtf(actor->displacement.x * actor->displacement.x + actor->displacement.z * actor->displacement.z);
    nrm.x = actor->displacement.x / len;
    nrm.z = actor->displacement.z / len;
    ang = Get_Angle(&actor->position, &unit->position);
    atan2f(actor->displacement.x, actor->displacement.z);
    r = ((UwamonoCollisionActor *)actor)->radius + unit->radius;
    actor->position.x -= actor->displacement.x;
    actor->position.z -= actor->displacement.z;
    ang2 = Get_Angle(&actor->position, &unit->position);
    tmp.x = unit->position.x - r * xglSin(ang);
    tmp.z = unit->position.z - r * xglCos(ang);
    {
        float dx = actor->position.x - tmp.x;
        float dz = actor->position.z - tmp.z;
        float d = __builtin_sqrtf(dx * dx + dz * dz);
        float t;
        len = (len - d) * 0.25f;
        t = len /(unit->radius + unit->radius);
        if (0.0f < ang2 - ang) {
            ang = ang - t;
        } else {
            ang = ang + t;
        }
    }
    r = ((UwamonoCollisionActor *)actor)->radius + unit->radius + D_004D7EE8;
    tmp.x = unit->position.x - r * xglSin(ang);
    tmp.z = unit->position.z - r * xglCos(ang);
    actor->displacement.x = tmp.x - actor->position.x;
    actor->displacement.z = tmp.z - actor->position.z;
    h = UnduCheck(&actor->position, &actor->displacement, (char *)actor + 0x4C8);
    if (h == -1000.0f) {
        actor->position.x -= actor->displacement.x;
        actor->position.z -= actor->displacement.z;
        return 0;
    }
    actor->position.y = h;
    return 1;
}

typedef struct NyuruMat44 {
    float m[4][4];
} NyuruMat44;

extern float D_004D7EEC;
extern float D_004D7EF0;

static int NyuruMatrix(UwamonoNyuruActor *actor, UwamonoCollisionUnit *unit, int mode) {
    UwamonoAlignedVector dim;
    Vector4 nrm;
    volatile NyuruMat44 mcopy;
    float *m;
    float dx;
    float dz;
    float sq;
    float dot;
    float len;
    float s;
    float r;
    float ddx;
    float ddz;
    float ad;
    float rad;
    int ret;

    dim = *(UwamonoAlignedVector *)((char *)unit + 0xB0);
    m = *(float **)((char *)unit + 0x80);
    mcopy = *(NyuruMat44 *)m;
    dx = actor->displacement.x;
    dz = actor->displacement.z;
    sq = dx * dx + dz * dz;
    actor->position.x = actor->position.x - dx;
    actor->position.z = actor->position.z - dz;
    ddx = actor->position.x - unit->position.x;
    ddz = actor->position.z - unit->position.z;
    dot = (ddx * m[0]) + (ddz * m[2]);
    len = __builtin_sqrtf(sq);
    if (len == 0.0f) {
        return 0;
    }
    {
        rad = *(float *)((char *)actor + 0x9E8) * 1.5f;
        ad = __builtin_fabsf(dot);
        nrm.x = actor->displacement.x / len;
        nrm.z = actor->displacement.z / len;
        nrm.y = 0.0f;
        if (ad < dim.value.x + rad) {
            s = nrm.x * m[0] + nrm.z * m[2];
            actor->displacement.x = s * m[0] * len;
            actor->displacement.z = s * m[2] * len;
            if (__builtin_fabsf(actor->displacement.x) < D_004D7EEC && __builtin_fabsf(actor->displacement.z) < D_004D7EEC && mode == 0) {
                if (dim.value.x < dot) {
                    actor->displacement.x = m[0] * len * 0.5f;
                    actor->displacement.z = m[2] * len * 0.5f;
                } else if (dot < -dim.value.x) {
                    actor->displacement.x = -m[0] * len * 0.5f;
                    actor->displacement.z = -m[2] * len * 0.5f;
                }
            }
        } else {
            s = nrm.x * m[8] + nrm.z * m[10];
            actor->displacement.x = s * m[8] * len;
            actor->displacement.z = s * m[10] * len;
            if (__builtin_fabsf(actor->displacement.x) < D_004D7EF0 && __builtin_fabsf(actor->displacement.z) < D_004D7EF0 && mode == 0) {
                dot = (ddx * m[8]) + (ddz * m[10]);
                if (dim.value.z < dot) {
                    actor->displacement.x = m[8] * len * 0.5f;
                    actor->displacement.z = m[10] * len * 0.5f;
                } else if (dot < -dim.value.z) {
                    actor->displacement.x = -m[8] * len * 0.5f;
                    actor->displacement.z = -m[10] * len * 0.5f;
                }
            }
        }
        *(unsigned short *)((char *)actor + 0x4D0) = *(unsigned short *)((char *)actor + 0x4D0) | 0x4000;
        r = UnduCheck((float *)&actor->position, &actor->displacement, (char *)actor + 0x4C8);
        if (r == -1000.0f) {
            actor->position.x = actor->position.x - actor->displacement.x;
            actor->position.z = actor->position.z - actor->displacement.z;
            ret = 0;
        } else {
            actor->position.y = r;
            ret = 1;
        }
        return ret;
    }
}

int NyuruCorner(UwamonoCollisionActor *, UwamonoCollisionUnit *);

int NyuruBox(UwamonoCollisionActor *actor, UwamonoCollisionUnit *unit, int arg2) {
    UwamonoAlignedVector dim;
    Vector4 nrm;
    volatile NyuruMat44 mcopy;
    UwamonoNyuruActor *a;
    float *m;
    float len;
    float dx;
    float dz;
    float sq;
    float dot;
    float s;
    float r;

    m = *(float **)((char *)unit + 0x80);
    mcopy = *(NyuruMat44 *)m;
    dim = *(UwamonoAlignedVector *)((char *)unit + 0xB0);
    if (arg2 == 0 && HitCheckCorner(actor, unit) != 0) {
        NyuruCorner(actor, unit);
        return 1;
    }
    a = (UwamonoNyuruActor *)actor;
    dx = a->displacement.x;
    dz = a->displacement.z;
    sq = dx * dx + dz * dz;
    a->position.x = a->position.x - dx;
    a->position.z = a->position.z - dz;
    dot = ((a->position.x - unit->position.x) * m[0]) + ((a->position.z - unit->position.z) * m[2]);
    len = __builtin_sqrtf(sq);
    if (len != 0.0f) {
        nrm.x = a->displacement.x / len;
        nrm.z = a->displacement.z / len;
        nrm.y = 0.0f;
        if (__builtin_fabsf(dot) < dim.value.x + actor->radius * 1.5f) {
            s = nrm.x * m[0] + nrm.z * m[2];
            a->displacement.x = s * m[0] * len;
            a->displacement.z = s * m[2] * len;
        } else {
            s = nrm.x * m[8] + nrm.z * m[10];
            a->displacement.x = s * m[8] * len;
            a->displacement.z = s * m[10] * len;
        }
        *(unsigned short *)((char *)actor + 0x4D0) = *(unsigned short *)((char *)actor + 0x4D0) | 0x4000;
        r = UnduCheck((float *)&a->position, &a->displacement, (char *)actor + 0x4C8);
        if (r != -1000.0f) {
            a->position.y = r;
            return 1;
        }
        a->position.x = a->position.x - a->displacement.x;
        a->position.z = a->position.z - a->displacement.z;
        return 0;
    }
    return 0;
}

extern float D_004D7EF4;
typedef struct NyuruUnitCopy {
    UwamonoAlignedVector a[0x30];
} NyuruUnitCopy;

static int NyuruCorner(UwamonoCollisionActor *arg0, UwamonoCollisionUnit *arg1) {
    UwamonoAlignedVector corner;
    UwamonoAlignedVector alt;
    UwamonoAlignedVector pos;
    UwamonoAlignedVector dim;
    NyuruUnitCopy local;
    float *m;
    float d0;
    float radius;
    float r15;
    int ret;

    pos = *(UwamonoAlignedVector *)&arg1->position;
    dim = *(UwamonoAlignedVector *)((char *)arg1 + 0xB0);
    m = *(float **)((char *)arg1 + 0x80);
    corner.value.x = pos.value.x + (dim.value.x * m[0] + dim.value.z * m[8]);
    corner.value.y = pos.value.y;
    corner.value.z = pos.value.z + (dim.value.x * m[2] + dim.value.z * m[10]);
    alt.value.x = pos.value.x - (dim.value.z * m[2] + dim.value.x * m[10]);
    alt.value.y = pos.value.y;
    alt.value.z = pos.value.z + (dim.value.z * m[0] + dim.value.x * m[8]);
    d0 = CheckDist2D(&arg0->position, &corner.value);
    if (CheckDist2D(&arg0->position, &alt.value) < d0) {
        corner = alt;
    }
    alt.value.x = pos.value.x - (dim.value.x * m[0] + dim.value.z * m[8]);
    alt.value.y = pos.value.y;
    alt.value.z = pos.value.z - (dim.value.x * m[2] + dim.value.z * m[10]);
    d0 = CheckDist2D(&arg0->position, &corner.value);
    if (CheckDist2D(&arg0->position, &alt.value) < d0) {
        corner = alt;
    }
    alt.value.x = pos.value.x + (dim.value.z * m[2] + dim.value.x * m[10]);
    alt.value.y = pos.value.y;
    alt.value.z = pos.value.z - (dim.value.z * m[0] + dim.value.x * m[8]);
    d0 = CheckDist2D(&arg0->position, &corner.value);
    if (CheckDist2D(&arg0->position, &alt.value) < d0) {
        corner = alt;
    }
    local = *(NyuruUnitCopy *)arg1;
    radius = arg0->radius;
    r15 = radius * 1.5f;
    arg0->radius = D_004D7EF4;
    local.a[1] = corner;
    local.a[11].value.z = r15;
    local.a[11].value.x = r15;
    ret = NyuruCircle((UwamonoNyuruActor *)arg0, (UwamonoCollisionUnit *)&local);
    arg0->radius = radius;
    return ret;
}

extern int AimMapUnit(void);
extern void DispDebugUwamono(void);
extern void DispCollUwamono(void);
extern void ACT_setMotion(void *actor, int motion);
extern void Vibration_Set_Weak(int level);
extern void Enemy_FindByEar(void *actor, void *pos);
extern signed char dispflg;
extern signed char collflg;
extern char D_004CA260[];
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
    float mt;

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
                actor->angle = Get_Angle(&actor->pos, (char *)MapUnit + 0x10 + G.shootUnit * 0x300);
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
    mt = actor->motionTime;
    no = G.shootUnit;
    sub = (ShootSub *)&MapUnit[no].state;
    if (mt > 0.2f && mt < 0.36f) {
        Vibration_Set_Weak(4);
    }
    if (actor->motionTime > 0.36f && sub->found == 0) {
        if (printflg) {
            printf(D_004CA260, G.shootUnit);
        }
        Enemy_FindByEar(actor, (char *)MapUnit + 0x10 + no * 0x300);
        sub->found = 1;
    }
    if (actor->motionTime > 0.5f) {
        G.shootUnit = -1;
        G.flags = (G.flags & ~0x4000) & 0xFFFF7FFF;
        actor->actFlags |= 8;
    }
}

int UwamonoAreaCheck(Vector4 *arg0, char *arg1, float r);
void CheckNearPoint(const Vector4 *origin, const Vector4 *candA, const Vector4 *candB, Vector4 *out);
void CrossPointUwamonoAt(Vector4 *first, Vector4 *second, UwamonoMapUnit *unit, void *result);

int CrossPointUwamono(const Vector4 *position, const Vector4 *previous,
                      float radius, Vector4 *result)
{
    int candidates[64];
    Vector4 nearStart;
    Vector4 nearEnd;
    Vector4 farStart;
    Vector4 farEnd;
    Vector4 normal;
    Vector4 candidate;
    int count;
    float scaledRadius;
    float reach;
    int empty;
    int i;

    normal.w = 1.0f;
    normal.x = position->x - previous->x;
    normal.z = position->z - previous->z;
    normal.y = 0.0f;
    scaledRadius = radius * 1.5f;
    reach = normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
    reach = __builtin_sqrtf(reach) + scaledRadius;
    empty = -1;
    for (i = 63; i >= 0; i--) {
        candidates[i] = empty;
    }
    count = 0;
    for (i = 0; i < 64; i++) {
        struct UwamonoMapCollisionState *state = &MapUnit[i].state;
        UwamonoMapSlot *unit = &MapUnit[i];
        if (unit->serial != -1 && !(unit->flags & 0x100000) &&
            !(0.3f < __builtin_fabsf(position->y - unit->position.value.y)) &&
            state->crossType != 0 && (unit->flags & 0x10000) &&
            UwamonoAreaCheck((Vector4 *)position, (char *)unit, reach)) {
            candidates[count] = i;
            count++;
        }
    }

    result->x = 65535.0f;
    result->z = 65535.0f;
    result->y = -1000.0f;
    if (count == 0) {
        *result = *previous;
        return 0;
    }

    xglVectorNormal(&normal, &normal);
    nearStart = *position;
    nearEnd = *previous;
    farStart = *position;
    __builtin_memcpy(&farEnd, previous, sizeof(farEnd));
    farEnd.w = farStart.w = nearEnd.w = nearStart.w = 1.0f;
    nearStart.x += scaledRadius * normal.z;
    nearStart.z -= scaledRadius * normal.x;
    nearEnd.x += scaledRadius * normal.z;
    nearEnd.z -= scaledRadius * normal.x;
    farStart.x -= scaledRadius * normal.z;
    farStart.z += scaledRadius * normal.x;
    farEnd.x -= scaledRadius * normal.z;
    farEnd.z += scaledRadius * normal.x;

    for (i = 0; i < 64; i++) {
        UwamonoMapSlot *unit;
        if (candidates[i] == -1) {
            break;
        }
        unit = &MapUnit[candidates[i]];
        CrossPointUwamonoAt(&nearStart, &nearEnd,
                            (UwamonoMapUnit *)unit, &candidate);
        if (candidate.y != -1000.0f) {
            CheckNearPoint(position, &candidate, result, result);
        }
        CrossPointUwamonoAt(&farStart, &farEnd,
                            (UwamonoMapUnit *)unit, &candidate);
        if (candidate.y != -1000.0f) {
            CheckNearPoint(position, &candidate, result, result);
        }
        candidate.x = position->x - unit->position.value.x;
        candidate.z = position->z - unit->position.value.z;
        if ((normal.x * candidate.x + normal.z * candidate.z) > 0.0f &&
            CheckPointLine(&nearStart, &nearEnd, &unit->position.value) !=
                CheckPointLine(&farStart, &farEnd, &unit->position.value)) {
            CrossPointUwamonoAt((Vector4 *)position, &unit->position.value,
                                (UwamonoMapUnit *)unit, &candidate);
            CheckNearPoint(position, &candidate, result, result);
        }
    }

    if (result->y == -1000.0f) {
        *result = *previous;
        return 0;
    }
    nearEnd.x = position->x - result->x;
    nearEnd.y = position->y;
    nearEnd.z = position->z - result->z;
    reach = normal.x * nearEnd.x + normal.z * nearEnd.z;
    reach -= scaledRadius;
    if (reach < 0.0f) {
        *result = *position;
        return 1;
    }
    result->x = position->x - reach * normal.x;
    result->y = position->y;
    result->z = position->z - reach * normal.z;
    return 1;
}

void CrossPointUwamonoAt(Vector4 *first, Vector4 *second, UwamonoMapUnit *unit, void *result)
{
    switch (unit->crossType) {
    case 1:
        CalcNearCrossPointCircle(first, second, (UwamonoCollisionUnit *)unit, result);
        return;
    case 2:
        CalcNearCrossPointBox(first, second, unit, result);
        break;
    }
}

void CalcNearCrossPointCircle(const Vector4 *from, const Vector4 *to,
                              UwamonoCollisionUnit *unit, Vector4 *out)
{
    Vector4 delta;
    Vector4 direction;
    float distance;
    if (CrossCheckMapUnitCircle(from, to, unit)) {
        delta = *from;
        delta.y = unit->position.y;
        __asm__ __volatile__(
            "lqc2 vf3,0(%0)\n\t"
            "lqc2 vf2,0(%1)\n\t"
            "vsub.xyz vf2xyz,vf2xyz,vf3xyz\n\t"
            "sqc2 vf2,0(%2)"
            :
            : "r"(from), "r"(&unit->position), "r"(&delta)
            : "memory");
        xglVectorLength(&distance, &delta);
        xglVectorNormal(&direction, &delta);
        distance -= unit->radius;
        if (distance < 0.0f) {
            *out = *from;
        } else {
            out->x = from->x + distance * direction.x;
            out->y = from->y;
            out->z = from->z + distance * direction.z;
        }
    } else {
        out->y = -1000.0f;
    }
}

extern void CalcCrossPoint(const Vector4 *first, const Vector4 *second,
                           const Vector4 *third, const Vector4 *fourth,
                           Vector4 *cross_point);
extern int CheckCrossLine(const Vector4 *first_start, const Vector4 *first_end,
                          const Vector4 *second_start, const Vector4 *second_end);
void CheckNearPoint(const Vector4 *origin, const Vector4 *candA, const Vector4 *candB, Vector4 *out);

void CalcNearCrossPointBox(Vector4 *first, Vector4 *second, UwamonoMapUnit *unit, void *result) {
    Vector4 c1;
    Vector4 c2;
    Vector4 c3;
    Vector4 c4;
    UwamonoAlignedVector pos;
    UwamonoAlignedVector dim;
    Vector4 out;
    Vector4 *res = (Vector4 *)result;
    float *m;
    pos = ((UwamonoMapSlot *)unit)->position;
    m = *(float **)((char *)unit + 0x80);
    dim = ((UwamonoMapSlot *)unit)->dimensions;
    res->y = -1000.0f;
    out.y = first->y;

    c1.x = pos.value.x + (dim.value.x * m[0] + dim.value.z * m[8]);
    c1.y = pos.value.y;
    c1.z = pos.value.z + (dim.value.x * m[2] + dim.value.z * m[10]);
    c2.x = pos.value.x - (dim.value.z * m[2] + dim.value.x * m[10]);
    c2.y = pos.value.y;
    c2.z = pos.value.z + (dim.value.z * m[0] + dim.value.x * m[8]);
    c3.x = pos.value.x - (dim.value.x * m[0] + dim.value.z * m[8]);
    c3.y = pos.value.y;
    c3.z = pos.value.z - (dim.value.x * m[2] + dim.value.z * m[10]);
    c4.x = pos.value.x + (dim.value.z * m[2] + dim.value.x * m[10]);
    c4.y = pos.value.y;
    c4.z = pos.value.z - (dim.value.z * m[0] + dim.value.x * m[8]);

    if (CheckCrossLine(first, second, &c1, &c2) != 0) {
        CalcCrossPoint(first, second, &c1, &c2, &out);
        *(UwamonoAlignedVector *)res = *(UwamonoAlignedVector *)&out;
    }
    if (CheckCrossLine(first, second, &c2, &c3) != 0) {
        CalcCrossPoint(first, second, &c2, &c3, &out);
        if (res->y != -1000.0f) {
            CheckNearPoint(first, &out, res, res);
        } else {
            *(UwamonoAlignedVector *)res = *(UwamonoAlignedVector *)&out;
        }
    }
    if (CheckCrossLine(first, second, &c3, &c4) != 0) {
        CalcCrossPoint(first, second, &c3, &c4, &out);
        if (res->y != -1000.0f) {
            CheckNearPoint(first, &out, res, res);
        } else {
            *(UwamonoAlignedVector *)res = *(UwamonoAlignedVector *)&out;
        }
    }
    if (CheckCrossLine(first, second, &c4, &c1) != 0) {
        CalcCrossPoint(first, second, &c4, &c1, &out);
        if (res->y != -1000.0f) {
            CheckNearPoint(first, &out, res, res);
        } else {
            *(UwamonoAlignedVector *)res = *(UwamonoAlignedVector *)&out;
        }
    }
}

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

extern float D_004D7F10;
extern float D_004D7F14;

int UwamonoAreaCheck(Vector4 *arg0, char *arg1, float r) {
    char *p;
    float y;
    float top;
    int ret;

    p = arg1 + 0x1A0;
    top = *(float *)(arg1 + 0x14);
    y = arg0->y;
    if (top + *(float *)(arg1 + 0xB4) * D_004D7F10 < y) {
        return 0;
    }
    if (y < top - D_004D7F14) {
        return 0;
    }
    switch (p[0x2D]) {
    case 1:
        if (CheckDist2D(arg0, arg1 + 0x10) < r + *(float *)(arg1 + 0xB0)) {
one:
            ret = 1;
            break;
        }
        goto zero;
    case 2:
        if (((int (*)(Vector4 *, char *))AreaCheckBox)(arg0, arg1) != 0) {
            goto one;
        }
zero:
        ret = 0;
        break;
    default:
        return 0;
    }
    return ret;
}

static inline float UwamonoProjectBoxAxisXZ(float x, float z,
                                           const Matrix4 *matrix)
{
    return x * (*matrix)[0][0] + z * (*matrix)[0][2];
}

int AreaCheckBox(const Vector4 *position, UwamonoMapSlot *unit, float radius)
{
    UwamonoMapSlot *box = unit;
    UwamonoAlignedVector dimensions;
    float pointX = position->x;
    float pointZ = position->z;
    float unitZ;
    float unitX;
    float offsetX;
    float offsetZ;
    float differenceZ;
    const Matrix4 *matrix;
    float basisZ;
    float basisX;
    float projection;
    float depth;

    unitX = unit->position.value.x;
    unitZ = unit->position.value.z;
    offsetX = pointX - unitX;
    offsetZ = pointZ - unitZ;
    dimensions = box->dimensions;
    matrix = (const Matrix4 *)box->basis;
    projection = UwamonoProjectBoxAxisXZ(offsetX, offsetZ, matrix);
    if (box->dimensions.value.x + radius < __builtin_fabsf(projection)) {
        return 0;
    }
    depth = box->dimensions.value.z + radius;
    basisZ = (*matrix)[0][2];
    basisX = (*matrix)[0][0];
    /* Measure the residual offset from the point projected onto the box axis. */
    offsetX = pointX - (projection * basisX + unitX);
    differenceZ = pointZ - (projection * basisZ + unitZ);
    /* Square the Z residual component for the radial-distance test. */
    offsetZ = differenceZ * differenceZ;
    if (offsetX * offsetX + offsetZ < depth * depth) {
        return 1;
    }
    return 0;
}

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
    struct UwamonoMapCollisionState *sub;
    char *unit;
    char *base;

    for (i = 0, off = 0; i < 64; i++, off += 0x300) {
        sub = &MapUnit[i].state;
        base = (char *)MapUnit;
        id = (AimUnitId *)((char *)&MapUnit[i] + 0xA0);
        if (id->serial == -1) continue;
        if (id->serial == 0x7000) continue;
        unit = (char *)(off + (int)base);
        if (MapUnit[i].flags & 0x100000) continue;
        if (id->kind != actor[0xA0]
            && sub->crossType != 0
            && (MapUnit[i].flags & 0x10000)
            && CrossCheckMapUnitAt((Vector4 *)a, (Vector4 *)(actor + 0x10), (UwamonoCrossUnit *)unit) != 0) {
            return i;
        }
    }
    return -1;
}

int CrossCheckMapUnit(float *a, float *b)
{
    int i;
    struct UwamonoMapCollisionState *sub;

    for (i = 0; i < 64; i++) {
        sub = &MapUnit[i].state;
        if (MapUnit[i].serial != -1
            && !(MapUnit[i].flags & 0x100000)
            && sub->crossType != 0
            && (MapUnit[i].flags & 0x10000)
            && CrossCheckMapUnitAt((Vector4 *)a, (Vector4 *)b, (UwamonoCrossUnit *)&MapUnit[i]) != 0) {
            return i;
        }
    }
    return -1;
}

int CrossCheckActor(UwamonoCrossActor *source, const Vector4 *to)
{
    int index;
    for (index = 0; index < 64; ++index) {
        if (!(D_004D7F18 < __builtin_fabsf(source->position.y - actor[index].position.y)) &&
            actor[index].resourceId >= 0 && !(actor[index].flags & 8) &&
            actor[index].collisionObject != 0 && index != source->actorIndex &&
            CheckCrossCircle(actor[index].radius, &source->position, to, &actor[index].position)) {
            return index;
        }
    }
    return -1;
}

int CrossCheckMapUnitAt(Vector4 *first, Vector4 *second, UwamonoCrossUnit *unit)
{
    UwamonoCrossState *state = &unit->state;

    if (D_004D7F1C < __builtin_fabsf(first->y - unit->position.y))
        return 0;
    switch (state->crossType) {
    case 1:
        return CrossCheckMapUnitCircle(first, second, (UwamonoCollisionUnit *)unit);
    case 2:
        return CrossCheckMapUnitBox(first, second, unit);
    default:
        return 0;
    }
}

int CrossCheckMapUnitCircle(const Vector4 *from, const Vector4 *to, UwamonoCollisionUnit *unit)
{
    Vector4 center;
    Vector4 dir;
    float radius;
    float half;
    float dist;
    float dx;
    float dz;

    radius = unit->radius;
    dir.x = to->x - from->x;
    dir.y = 0.0f;
    dir.z = to->z - from->z;
    half = __builtin_sqrtf(dir.x * dir.x + dir.z * dir.z) * 0.5f;
    xglVectorNormal(&dir, &dir);
    center.x = from->x + (to->x - from->x) * 0.5f;
    center.z = from->z + (to->z - from->z) * 0.5f;
    dx = unit->position.x - center.x;
    dz = unit->position.z - center.z;
    dist = dx * dir.x + dz * dir.z;
    if (half + radius < __builtin_fabsf(dist)) {
        return 0;
    }
    dx = unit->position.x - (dist * dir.x + center.x);
    dz = unit->position.z - (dist * dir.z + center.z);
    if (dx * dx + dz * dz < radius * radius) {
        return 1;
    }
    return 0;
}

extern int CheckCrossLine(const Vector4 *first_start, const Vector4 *first_end,
                          const Vector4 *second_start, const Vector4 *second_end);
extern f32 D_004D7F20;

int CrossCheckMapUnitBox(Vector4 *first, Vector4 *second, UwamonoCrossUnit *unit) {
    Vector4 c1;
    Vector4 c2;
    Vector4 c3;
    Vector4 c4;
    UwamonoAlignedVector pos = *(UwamonoAlignedVector *)&unit->position;
    long long zw = *(long long *)((char *)unit + 0xB8);
    UwamonoAlignedVector dim;
    float *m;
    dim.words[0] = *(long long *)((char *)unit + 0xB0);
    dim.words[1] = zw;
    m = *(float **)((char *)unit + 0x80);
    dim.value.x += D_004D7F20;
    dim.value.z += D_004D7F20;

    c1.x = pos.value.x + (dim.value.x * m[0] + dim.value.z * m[8]);
    c1.y = pos.value.y;
    c1.z = pos.value.z + (dim.value.x * m[2] + dim.value.z * m[10]);
    c2.x = pos.value.x - (dim.value.z * m[2] + dim.value.x * m[10]);
    c2.y = pos.value.y;
    c2.z = pos.value.z + (dim.value.z * m[0] + dim.value.x * m[8]);
    if (CheckCrossLine(first, second, &c1, &c2) != 0) {
        return 1;
    }
    c3.x = pos.value.x - (dim.value.x * m[0] + dim.value.z * m[8]);
    c3.y = pos.value.y;
    c3.z = pos.value.z - (dim.value.x * m[2] + dim.value.z * m[10]);
    if (CheckCrossLine(first, second, &c2, &c3) != 0) {
        return 1;
    }
    c4.x = pos.value.x + (dim.value.z * m[2] + dim.value.x * m[10]);
    c4.y = pos.value.y;
    c4.z = pos.value.z - (dim.value.z * m[0] + dim.value.x * m[8]);
    if (CheckCrossLine(first, second, &c3, &c4) != 0) {
        return 1;
    }
    return CheckCrossLine(first, second, &c4, &c1) != 0;
}

void UnlockMapUnit(void)
{
    int unitIndex = 63;
    int *effectCf = &MapUnit[0].effectCf[0];

    do {
        MapUnit[63 - unitIndex].flags &= ~0x40000u;
        if (effectCf[1] != 0) {
            sefDeleteEffectCf(effectCf[1]);
            effectCf[1] = 0;
        }
        if (effectCf[2] != 0) {
            sefDeleteEffectCf(effectCf[2]);
            effectCf[2] = 0;
        }
        unitIndex--;
        if (unitIndex >= 0)
            effectCf = &MapUnit[63 - unitIndex].effectCf[0];
    } while (unitIndex >= 0);

    GameLoopState.aimMapUnitIndex = -1;
}

int CheckNearMapUnit(void)
{
    UwamonoScanUnit *unit = (UwamonoScanUnit *)MapUnit;
    struct UwamonoSoundActor *actor = GameLoopState.actor;
    float nearest = GameLoopState.selectionDistance + 1000.0f;
    int selected = -1;
    int index = 0;

    do {
        if (CheckBrokenMapUnit((BrokenMapUnit *)unit) & 1) {
            if (unit->flags & 0x40000) {
                float distance = CheckCornerDist(&actor->position, (UwamonoCollisionUnit *)unit);
                if (distance < nearest) {
                    nearest = distance;
                    selected = index;
                }
            }
        }
        ++index;
        ++unit;
    } while (index < 64);
    if (nearest < GameLoopState.selectionDistance) {
        return selected;
    }
    return -1;
}

int CheckBrokenMapUnit(BrokenMapUnit *unit)
{
    BrokenMapUnitState *state = &unit->brokenState;
    int flags;
    int brokenId;

    if (unit->serial == -1)
        return 0;
    flags = unit->flags;
    if (!(flags & 0x10000))
        return 0;
    if (flags & 4)
        return 0;
    if (state->signal == 1)
        return 0;
    brokenId = state->brokenId;
    return brokenId == -1 ? 0 : brokenId;
}

int AimHeightCheck(UwamonoMapUnit *unit)
{
    UwamonoGameLoopState *state = &GameLoopState;
    AimHeightReference *heightReference = state->actor;
    if (!(state->aimHeightFlags & 2))
        return 1;
    return __builtin_fabsf(heightReference->position.y - unit->position.y) < 0.3f;
}

typedef union UwamonoAlignedScreenPointSL {
    UwamonoScreenPoint value;
    int coordinates[4];
    long long words[2];
} UwamonoAlignedScreenPointSL;

float xglRotTransPers(Vector4 *out, const Matrix4 matrix, Vector4 *point, int camera_id);
typedef struct UwamonoAimCandidate {
    int unitIndex;
    unsigned char unmodeled_04[0x0c];
    UwamonoAlignedScreenPoint screenPoint;
    float distance;
    unsigned char unmodeled_24[0x0c];
} UwamonoAimCandidate;

int AimMapUnit(void)
{
    UwamonoAlignedScreenPoint actorScreen;
    UwamonoAimCandidate candidates[64];
    UwamonoAimCandidate savedCandidate;
    UwamonoAlignedVector actorPosition;
    UwamonoAlignedVector unitPosition;
    int candidateCount;
    int selectedFound;
    UwamonoAimCandidate *candidate;
    UwamonoAimCandidate *list;
    UwamonoSoundActor *actor = GameLoopState.actor;
    int index;

    actorPosition = *(UwamonoAlignedVector *)&actor->position;
    actorPosition.value.y += 1.0f;
    xglRotTransPers((Vector4 *)&actorScreen, 0, &actorPosition.value, 0);
    candidateCount = 0;
    selectedFound = 0;
    candidate = candidates;
    list = candidates;

    for (index = 0; index < 64; ++index) {
        unsigned int oldFlags = MapUnit[index].flags;
        float distance;

        MapUnit[index].flags &= ~0x40000u;
        candidate->unitIndex = -1;
        if (!(CheckBrokenMapUnit((BrokenMapUnit *)&MapUnit[index]) & 1)) {
            continue;
        }
        if (!AimHeightCheck((UwamonoMapUnit *)&MapUnit[index])) {
            continue;
        }

        distance = CheckCornerDist(&actor->position,
                                   (UwamonoCollisionUnit *)&MapUnit[index]);
        if (GameLoopState.selectionDistance < distance) {
            continue;
        }
        if (!AimMapUnitLookCheck((UwamonoScanUnit *)&MapUnit[index])) {
            continue;
        }

        if ((oldFlags & 0x40000u) == 0) {
            xglSoundEffectNormalID(0x10001, 0);
        }
        candidate->distance = distance;
        unitPosition = MapUnit[index].position;
        ((Vector4 *)&unitPosition)->y += 0.5f * ((const Vector4 *)&MapUnit[index].dimensions)->y;
        MapUnit[index].flags = MapUnit[index].flags | 0x40000u;
        if (GameLoopState.aimMapUnitIndex == index) {
            selectedFound = 1;
        }
        candidate->unitIndex = index;
        xglRotTransPers((Vector4 *)&candidate->screenPoint, 0,
                        &unitPosition.value, 0);
        ++candidateCount;
        ++candidate;
    }

    if (candidateCount < 2) {
        GameLoopState.aimMapUnitIndex = candidates[0].unitIndex;
        if (candidates[0].unitIndex != -1) {
            SortLine(&actorScreen, &candidates[0].screenPoint, 1);
        }
        return candidates[0].unitIndex;
    }

    index = 1;
    if (candidates[1].unitIndex != -1) {
        for (;;) {
            if (candidates[index - 1].screenPoint.value.x > candidates[index].screenPoint.value.x) {
            savedCandidate = candidates[index];
            candidates[index] = candidates[index - 1];
            candidates[index - 1] = savedCandidate;
            index = 0;
            }
            ++index;
            if (!(index < 64 && candidates[index].unitIndex != -1)) {
                break;
            }
        }
    }

    for (index = 0; index < 63 && candidates[index].unitIndex != -1; ++index) {
        if (GameLoopState.aimMapUnitIndex == candidates[index].unitIndex) {
            int offset = index * 48 + 16;

            SortLine(&actorScreen, (UwamonoAlignedScreenPoint *)((char *)candidates + offset), 1);
        } else {
            int offset = index * 48 + 16;

            SortLine(&actorScreen, (UwamonoAlignedScreenPoint *)((char *)candidates + offset), 0);
        }
    }

    if (!selectedFound) {
        GameLoopState.aimMapUnitIndex = CheckNearMapUnit();
    }

    if (PadData.trig & 8) {
        xglSoundEffectNormalID(3, 0);
        for (index = 0; index <= candidateCount; ++index) {
            if (GameLoopState.aimMapUnitIndex == candidates[index].unitIndex) {
                if (candidates[index + 1].unitIndex == -1) {
                    GameLoopState.aimMapUnitIndex = candidates[0].unitIndex;
                } else {
                    GameLoopState.aimMapUnitIndex = candidates[index + 1].unitIndex;
                }
                return GameLoopState.aimMapUnitIndex;
            }
        }
    }

    if (PadData.trig & 4) {
        xglSoundEffectNormalID(3, 0);
        for (index = candidateCount - 1; index >= 0; --index) {
            if (GameLoopState.aimMapUnitIndex == candidates[index].unitIndex) {
                if (index == 0) {
                    GameLoopState.aimMapUnitIndex =
                        candidates[candidateCount - 1].unitIndex;
                } else {
                    GameLoopState.aimMapUnitIndex = candidates[index - 1].unitIndex;
                }
                return GameLoopState.aimMapUnitIndex;
            }
        }
    }

    return candidates[0].unitIndex;
}

extern f32 D_004D7F28;
typedef struct AimVec16 {
    long long a;
    long long b;
} AimVec16;

int AimMapUnitLookCheck(unsigned char *arg0)
{
    InitUwamonoQuery q;
    union {
        Vector4 v;
        AimVec16 q;
    } center;
    Vector4 disp;
    float r;
    UwamonoSoundActor *actor;
    float ax;
    float ay;
    float az;
    int ret;

    actor = GameLoopState.actor;
    if ((GameLoopState.aimHeightFlags & 8) != 0
        && CrossCheckMapUnitAim((float *)((char *)actor + 0x10), arg0) != -1) {
        return 0;
    }
    if ((GameLoopState.aimHeightFlags & 4) == 0) {
        return 1;
    }
    UnduParamInit(&q);
    q.header = UnduDataGetHeader(0x305, 0x8000);
    if (q.header == 0) {
        q.header = UnduDataGetHeader(0, 0x8000);
        if ((*(unsigned short *)((char *)actor + 0x4D0) & 0x20) != 0) {
            q.flags |= 0x20;
        }
    }
    center.q = *(AimVec16 *)((char *)actor + 0x10);
    q.flags = 0x1810;
    ax = *(float *)(arg0 + 0x10);
    ay = *(float *)(arg0 + 0x14);
    az = *(float *)(arg0 + 0x18);
    disp.x = ax - *(float *)((char *)actor + 0x10);
    disp.y = ay - *(float *)((char *)actor + 0x14);
    disp.z = az - *(float *)((char *)actor + 0x18);
    disp.w = 1.0f;
    r = UnduCheck((float *)&center, &disp, &q);
    if (r == -1000.0f) {
        return 0;
    }
    if (!(__builtin_fabsf(center.v.x - *(float *)(arg0 + 0x10)) < D_004D7F28)) {
        return 0;
    }
    if (__builtin_fabsf(center.v.z - *(float *)(arg0 + 0x18)) < D_004D7F28) {
        return 1;
    }
    return 0;
}

typedef struct KowareQ {
    unsigned long long w[2];
} KowareQ;

static float CheckCornerDist(const Vector4 *pos, UwamonoCollisionUnit *vunit)
{
    char *unit = (char *)vunit;
    union { KowareQ q; float f[4]; } box;
    float d;
    float ret;

    switch (unit[0x1CD]) {
    case 1:
        ret = CheckDist2D(pos, (Vector4 *)(unit + 0x10)) - *(float *)(unit + 0xB0);
        break;
    case 2:
        box.q = *(KowareQ *)(unit + 0xB0);
        d = __builtin_fabsf(pos->x - *(float *)(unit + 0x10));
        if (d < box.f[0]) box.f[0] = d;
        d = __builtin_fabsf(pos->z - *(float *)(unit + 0x18));
        if (d < box.f[2]) box.f[2] = d;
        ret = CheckDist2D(pos, (Vector4 *)(unit + 0x10));
        ret -= __builtin_sqrtf(box.f[0] * box.f[0] + box.f[2] * box.f[2]);
        break;
    default:
        ret = CheckDist2D(pos, (Vector4 *)(unit + 0x10));
        break;
    }
    return ret;
}

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

    unit->linkedUnit->position.x = unit->position.value.x;
    unit->linkedUnit->position.y = unit->position.value.y;
    unit->linkedUnit->position.z = unit->position.value.z;

    if (timers->bgmTimer > 0) {
        UwamonoBgmFunc(unit);
    }
}

void UwamonoBgmFunc(UwamonoCommonUnit *unit)
{
    UwamonoTimers *timers = &unit->timers;
    UwamonoAlignedVector soundPosition;

    if (timers->projectSound == 0) {
        soundPosition = unit->position;
    } else {
        float x = unit->position.value.x;
        float y = unit->position.value.y;
        float z = unit->position.value.z;
        UwamonoLinkedUnit *linkedUnit = unit->linkedUnit;
        float projection =
            (GameLoopState.actor->position.x - x) * linkedUnit->direction[0] +
            (GameLoopState.actor->position.y - y) * linkedUnit->direction[1] +
            (GameLoopState.actor->position.z - z) * linkedUnit->direction[2];

        soundPosition.value.x = x + projection * linkedUnit->direction[0];
        soundPosition.value.y = y + projection * linkedUnit->direction[1];
        soundPosition.value.z = z + projection * linkedUnit->direction[2];
        soundPosition.value.w = 1.0f;
    }
    xglSoundEffectPosID(timers->bgmTimer, &soundPosition.value.x, 1, unit->seChannel + 1);
}

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

typedef struct UwaUnduParam {
    unsigned char unmodeled_00[8];
    unsigned short flags;
    unsigned char unmodeled_0a[0xE];
    void *header;
    unsigned char unmodeled_1c[0x24];
} UwaUnduParam;
int HitCheckUwamono(unsigned char *actor);
extern float D_004D7F2C;
extern float D_004D7F30;
extern float D_004D7F34;
extern float D_004D7F38;
extern float D_004D7F3C;
extern float D_004D7F40;
#define UWA_VY(u) (*(float *)((char *)(u) + 0x224))
static inline UwamonoMapSlot *uwaSlot(int i) { return (UwamonoMapSlot *)((char *)MapUnit + i * 0x300); }
static inline float uwaSlotSum(UwamonoMapSlot *m) { return m->position.value.y + *(float *)((char *)m + 0xB4); }
static inline float uwaBounceVy(UwamonoCommonUnit *u) { return -UWA_VY(u) * 0.25f; }

void CheckUwamonoHeight(UwamonoCommonUnit *unit)
{
    UwaUnduParam param;
    UwamonoTimers *timers;
    UwamonoMapSlot *pa;
    float nv2;
    float y;
    float h;
    float vy;
    float lim;
    float nv;
    int idx;

    UnduParamInit(&param);
    timers = &unit->timers;
    param.header = UnduDataGetHeader(0, 0x8000);
    if ((signed char)timers->unmodeled_2f[0] != 0) {
        param.flags |= 0x20;
    } else {
        param.flags = 0x1010;
    }
    h = UnduCheck(&unit->position.value, 0, &param);
    vy = UWA_VY(unit);
    if (vy > -0.6f) {
        vy = UWA_VY(unit) = vy - 0.05f;
    }
    y = unit->position.value.y + vy;
    unit->position.value.y = y;
    if (y < h) {
        unit->position.value.y = h + 0.001f;
        lim = 0.05f;
        nv = -vy * 0.25f;
        UWA_VY(unit) = nv;
        if (nv < lim) {
            UWA_VY(unit) = 0.0f;
            *((u32 *)unit) = (*((u32 *)unit)) | 0x80000;
        }
    } else {
        idx = HitCheckUwamono((unsigned char *)unit);
        if (idx != -1) {
            nv2 = uwaBounceVy(unit);
            unit->position.value.y = MapUnit[idx].position.value.y + MapUnit[idx].dimensions.value.y + 0.001f;
            UWA_VY(unit) = nv2;
            if (nv2 < 0.1f) {
                UWA_VY(unit) = 0.0f;
            }
        }
    }
}

int HitCheckUwamono(unsigned char *actor)
{
    int i;
    int off;
    AimUnitId *id;
    struct UwamonoMapCollisionState *sub;
    char *unit;
    char *base;

    for (i = 0, off = 0; i < 64; i++, off += 0x300) {
        sub = &MapUnit[i].state;
        base = (char *)MapUnit;
        id = (AimUnitId *)((char *)&MapUnit[i] + 0xA0);
        if (id->serial == -1) continue;
        unit = (char *)(off + (int)base);
        if (id->kind == actor[0xA0]) continue;
        if (MapUnit[i].flags & 0x20000) continue;
        if (!(MapUnit[i].flags & 0x100000)
            && sub->crossType != 0
            && (MapUnit[i].flags & 0x10000)
            && HitCheckUwamonoAt((UwamonoBoxUnit *)actor, (UwamonoBoxUnit *)unit) != 0) {
            return i;
        }
    }
    return -1;
}

int HitCheckUwamonoAt(UwamonoBoxUnit *actor, UwamonoBoxUnit *unit)
{
    UwamonoBoxCollisionState *state = &unit->state;
    float actorHeight = actor->position.y;
    float baseHeight = unit->position.y;

    if (!(actorHeight < baseHeight) && !(baseHeight + unit->dimensions.y < actorHeight)) {
        switch (state->crossType) {
        case 1:
            if (CheckDist2D(&actor->position, &unit->position) < unit->dimensions.x + D_004D7F44)
                return 1;
            return 0;
        case 2:
            return HitCheckBoxUwamono(actor, unit);
        default:
            return 0;
        }
    }
    return 0;
}

typedef struct { float x, y, z, w; } __attribute__((aligned(8))) UwaBoxVec8;
typedef union { UwaBoxVec8 v; long long q[2]; } UwaBoxPair8;

int HitCheckBoxUwamono(UwamonoBoxUnit *actor, UwamonoBoxUnit *unit)
{
    UwaBoxVec8 size;
    UwaBoxPair8 pos;
    float x;
    float z;
    float cx;
    float cz;
    float d;
    float dist;
    float sz;
    float *dir;

    pos.q[0] = *(long long *)((char *)unit + 0x10);
    pos.q[1] = *(long long *)((char *)unit + 0x18);
    x = actor->position.x - pos.v.x;
    z = actor->position.z - pos.v.z;
    size = *(UwaBoxVec8 *)&unit->dimensions;
    dir = (float *)unit->basis;
    d = x * dir[0] + z * dir[2];
    if (__builtin_fabsf(d) > unit->dimensions.x + 0.15f) {
        return 0;
    }
    x = d * dir[0] + pos.v.x;
    z = d * dir[2] + pos.v.z;
    cx = actor->position.x - x;
    cz = actor->position.z - z;
    d = cx * cx + cz * cz;
    sz = unit->dimensions.z + 0.15f;
    if (d < sz * sz) {
        return 1;
    }
    return 0;
}

extern void DrawSprite(int *pos, int *spr, int w, int h);

void DrawRadar(void) {
    int spr[4];
    int pos[4];
    int i;
    int x;
    int z;
    int ax;
    int az;
    int inx;
    float dx;
    float dz;
    float fz;
    UwamonoSoundActor *player;
    UwamonoCrossActor *a;

    player = GameLoopState.actor;
    spr[0] = 0xFF;
    spr[1] = 0;
    spr[2] = 0;
    spr[3] = 0x80;
    pos[0] = 0x8B00;
    pos[1] = 0x8900;
    pos[2] = 0xFFFFFF;
    pos[3] = 0;
    DrawSprite(pos, spr, 0x20, 0x20);
    spr[0] = 0xFF;
    spr[1] = 0xFF;
    spr[2] = 0;
    spr[3] = 0x80;
    a = actor;
    i = 0;
    do {
        if (a->resourceId >= 0 && !(a->flags & 8) && a->collisionObject != 0 && i != *(unsigned char *)((char *)player + 0x80)) {
            fz = a->position.z;
            dx = a->position.x - player->position.x;
            *(volatile int *)&pos[0] = 0x8B00;
            dz = fz - player->position.z;
            x = (int)(dx * 64.0f);
            z = (int)(dz * 64.0f);
            ax = x < 0 ? -x : x;
            inx = ax < 0x3E1;
            az = z < 0 ? -z : z;
            pos[1] = 0x8900;
            if (inx) {
                if (az < 0x3E1) {
                    pos[0] = x + 0x8B00;
                    pos[1] = z + 0x8900;
                    DrawSprite(pos, spr, 0x20, 0x20);
                }
            }
        }
        i++;
        a++;
    } while (i < 0x40);
}

extern int sPoint_0[];

void DrawSprite(int *pos, int *spr, int w, int h) {
    union { long long d[2]; int w[4]; } v;
    union { long long d[2]; int w[4]; } *p = (void *)pos;

    v.d[0] = p->d[0];
    v.d[1] = p->d[1];
    sPoint_0[12] = spr[0];
    sPoint_0[13] = spr[1];
    sPoint_0[14] = spr[2];
    sPoint_0[15] = spr[3];
    sPoint_0[20] = spr[0];
    sPoint_0[21] = spr[1];
    sPoint_0[22] = spr[2];
    sPoint_0[23] = spr[3];
    sPoint_0[16] = v.w[0] - w;
    sPoint_0[17] = v.w[1] - h;
    sPoint_0[24] = v.w[0] + w;
    sPoint_0[25] = v.w[1] + h;
    sPoint_0[18] = v.w[2];
    sPoint_0[19] = v.w[3];
    sPoint_0[26] = v.w[2];
    sPoint_0[27] = v.w[3];
    xglPrimAddGifTagDirect(0, sPoint_0, 7);
}

extern UwamonoLinePacket sSortLine_1;
extern signed char lineflg;

void SortLine(const UwamonoAlignedScreenPointSL *from,
              const UwamonoAlignedScreenPointSL *to, int mode)
{
    UwamonoAlignedScreenPointSL first;
    UwamonoAlignedScreenPointSL second;

    first.words[0] = from->words[0];
    first.words[1] = from->words[1];
    from = &first;
    second.words[0] = to->words[0];
    second.words[1] = to->words[1];
    to = &second;

    if (from->coordinates[2] >= 0 && to->coordinates[2] >= 0 && lineflg) {
        if (mode == 1) {
            sSortLine_1.color0.x = 0;
            sSortLine_1.color0.y = 0;
            sSortLine_1.color0.z = 255;
            sSortLine_1.color0.w = 0;
            sSortLine_1.color1.x = 0;
            sSortLine_1.color1.y = 0;
            sSortLine_1.color1.z = 255;
            sSortLine_1.color1.w = 0;
        } else {
            sSortLine_1.color0.x = 255;
            sSortLine_1.color0.y = 0;
            sSortLine_1.color0.z = 0;
            sSortLine_1.color0.w = 0;
            sSortLine_1.color1.x = 255;
            sSortLine_1.color1.y = 0;
            sSortLine_1.color1.z = 0;
            sSortLine_1.color1.w = 0;
        }
        sSortLine_1.position0.x = from->coordinates[0];
        sSortLine_1.position0.y = from->coordinates[1];
        sSortLine_1.position0.z = from->coordinates[2];
        sSortLine_1.position0.w = 128;
        sSortLine_1.position1.x = to->coordinates[0];
        sSortLine_1.position1.y = to->coordinates[1];
        sSortLine_1.position1.z = to->coordinates[2];
        sSortLine_1.position1.w = 128;
        nmlModelDirectSend(5, (unsigned char *)&sSortLine_1, 6);
    }
}

#include "common.h"

typedef struct {
    unsigned long long xy;
    float z;
    float w;
} TrapPositionUp;
typedef struct {
    unsigned char unmodeled_00[2];
    short kindIndex;
    signed char state;
    unsigned char unmodeled_05[7];
    float sizeX;
    float sizeY;
    float sizeZ;
    float offsetX;
    float offsetY;
    float offsetZ;
    float damageRadius;
    unsigned char unmodeled_28[5];
    signed char windState;
    unsigned char unmodeled_2e[2];
    short methodId;
    signed char target;
    unsigned char unmodeled_33[9];
    int activeDuration;
    int effectId;
    unsigned int activeFlags;
} TrapDamagePropertiesUp;
typedef struct TrapMapUnitUp TrapMapUnitUp;
typedef struct {
    signed char windState;
    unsigned char unmodeled_01[0x0f];
    TrapPositionUp dimensions;
    unsigned int flags;
    int effectId;
    unsigned char unmodeled_28[8];
} TrapKindEntry;
typedef struct {
    signed char shape;
    unsigned char unmodeled_01[0x0f];
    TrapPositionUp dimensions;
    signed char target;
    unsigned char unmodeled_21[0x0f];
} KowareKindEntry;
typedef struct {
    unsigned char unmodeled_00[0xa9a];
    unsigned char trap_active;
} TrapLinkedObjectUp;
struct TrapMapUnitUp {
    unsigned int flags;
    void (*update)(TrapMapUnitUp *);
    unsigned char unmodeled_08[0x08];
    TrapPositionUp position;
    TrapPositionUp effectOrientation;
    unsigned char unmodeled_30[0x10];
    float matrix[4][4];
    float (*matrixRef)[4];
    unsigned char unmodeled_84[0x1c];
    unsigned char trapSerial;
    unsigned char unmodeled_a1;
    unsigned char trapState;
    unsigned char unmodeled_a3;
    short trapId;
    unsigned char unmodeled_a6[2];
    short trapTimer;
    unsigned char unmodeled_aa[6];
    TrapPositionUp dimensions;
    unsigned char unmodeled_c0[0x20];
    int uwaresId;
    int xtxresId;
    TrapLinkedObjectUp *linkedObject;
    unsigned char unmodeled_ec[4];
    unsigned char unmodeled_f0[0xb0];
    TrapDamagePropertiesUp trap;
};
typedef struct {
    float x;
    float y;
    float z;
    float w;
} TrapVectorUp;
typedef struct {
    unsigned char unmodeled_00[0x10];
    TrapVectorUp position;
    TrapVectorUp rotation;
    unsigned char unmodeled_30[0x80];
    TrapVectorUp dimensions;
} TrapMapUnitVectorsUp;
extern TrapKindEntry TrapKind_t_0[];
extern KowareKindEntry KowareKind_t[127];
extern int uwares_tbl[14];
extern int xtxres_tbl[14];
extern const char D_004CA3F8[16];
extern const char D_004CA408[16];
extern const char D_004CA418[0x30];
extern float D_004D7F54;
extern signed char printflg;
extern int printf(const char *, ...);
extern void MAP_updateUnitTrap();
extern void CreateUwamonoCommon(void *);
extern void SetContainerParam(void *);
extern void SetHideObject(void *);
extern void xglMatrixStackUnit(void);
extern void xglMatrixStackTrans(const float translation[4]);
extern void xglMatrixStackRotX(float angle);
extern void xglMatrixStackRotY(float angle);
extern void xglMatrixStackRotZ(float angle);
extern void xglMatrixStackSave(float matrix[4][4]);

void InitMapTrap(TrapMapUnitUp *unit)
{
    TrapMapUnitVectorsUp *view = (TrapMapUnitVectorsUp *)unit;
    TrapDamagePropertiesUp *trap = &unit->trap;
    int index;
    int xtxres;
    int uwares;

    unit->update = MAP_updateUnitTrap;
    if ((unsigned short)(unit->trapId - 0x7001) < 4) {
        index = unit->trapId - 0x7001;
        uwares = uwares_tbl[index];
        xtxres = xtxres_tbl[index];
        unit->uwaresId = uwares;
        unit->xtxresId = xtxres;
        if (trap->windState == -1) {
            trap->windState = TrapKind_t_0[index].windState;
        }
        unit->flags |= TrapKind_t_0[index].flags;
        unit->dimensions = TrapKind_t_0[index].dimensions;
        trap->damageRadius = 3.0f;
        if (trap->target == -1) {
            trap->target = 0;
        }
        if (trap->effectId == -1) {
            trap->effectId = TrapKind_t_0[index].effectId;
        }
    } else if ((unsigned short)(unit->trapId - 0x7014) < 31) {
        SetContainerParam(unit);
    } else {
        unit->flags |= TrapKind_t_0[4].flags;
        unit->dimensions = TrapKind_t_0[4].dimensions;
        trap->damageRadius = 3.0f;
        if (trap->kindIndex != -1) {
            if (trap->windState == -1) {
                trap->windState = KowareKind_t[trap->kindIndex].shape;
            } else {
                trap->windState = TrapKind_t_0[4].windState;
            }
            unit->dimensions = KowareKind_t[trap->kindIndex].dimensions;
            if (trap->effectId == -1) {
                trap->effectId = TrapKind_t_0[trap->kindIndex].effectId;
            }
        } else if (trap->windState == -1) {
            trap->windState = TrapKind_t_0[4].windState;
        }
        if (trap->target == -1) {
            trap->target = KowareKind_t[trap->kindIndex].target;
        }
        if (trap->effectId == -1) {
            trap->effectId = TrapKind_t_0[4].effectId;
        }
    }
    if (trap->sizeX > 0.0f) {
        view->dimensions.x = trap->sizeX * 0.5f;
        view->dimensions.y = trap->sizeY;
        view->dimensions.z = trap->sizeZ * 0.5f;
        if (printflg) {
            printf(D_004CA3F8);
            printf(D_004CA408, trap->sizeX, trap->sizeY, trap->sizeZ);
        }
    }
    view->dimensions.x += trap->offsetX * 0.5f;
    view->dimensions.y += trap->offsetY;
    view->dimensions.z += trap->offsetZ * 0.5f;
    if (trap->activeFlags == -1) {
        trap->activeFlags = 1;
    }
    if (unit->trapId == 0x7000) {
        unit->flags |= 4;
        trap->activeFlags = 0;
    }
    if (unit->trapId == 0x7012) {
        unit->flags |= 4;
        trap->activeFlags = 0;
        trap->windState = 0;
    }
    CreateUwamonoCommon(unit);
    xglMatrixStackUnit();
    xglMatrixStackTrans(&view->position.x);
    xglMatrixStackRotX(view->rotation.x);
    xglMatrixStackRotY(view->rotation.y);
    xglMatrixStackRotZ(view->rotation.z);
    xglMatrixStackSave(unit->matrix);
    unit->matrixRef = unit->matrix;
    SetHideObject(unit);
    if (printflg) {
        printf(D_004CA418, view->position.x, view->position.y, view->position.z,
               view->rotation.y / D_004D7F54 * 180.0f);
    }
}

typedef struct {
    unsigned char unmodeled_00[0xa9a];
    unsigned char activeFlag;
} TrapCursolTarget;
typedef struct {
    unsigned char unmodeled_00[4];
    signed char state;
    unsigned char unmodeled_05[0x28];
    unsigned char wind;
    unsigned char unmodeled_2e[2];
    short methodArg;
    unsigned char unmodeled_32[0xa];
    int life;
    int effectId;
    int cursol;
} TrapUnitTimers;
typedef struct {
    unsigned int flags;
    int active;
    unsigned char unmodeled_08[0x08];
    unsigned char position[0x10];
    unsigned char unmodeled_20[0x80];
    unsigned char serial;
    unsigned char unmodeled_a1;
    unsigned char mode;
    unsigned char unmodeled_a3;
    short id;
    unsigned char unmodeled_a6[2];
    short counter;
    unsigned char unmodeled_aa[0x3e];
    TrapCursolTarget *target;
    unsigned char unmodeled_ec[0xb4];
    TrapUnitTimers timers;
} TrapUpdateUnit;
typedef struct {
    unsigned char unmodeled_00[0x1c];
    int unk1C;
    int unk20;
} TrapGameLoopState;
extern TrapGameLoopState GameLoopState;
extern signed char printflg;
extern const char D_004DB728[];
extern const char D_004CA448[];
extern void DrawActiveCursol(void *);
extern void UwamonoCommonFunc(void *);
extern void UwamonoBrokenSe(void *);
extern void ClearUwamonoEffect(void *);
extern void SetUwaWind(void *);
extern void xglSoundEffectNormalID(int, int);
extern void Vibration_Set_Strong(int, int, int);
extern void *sefCreateEffectCf(int, void *, void *);
extern void CallMethod_I(const char *, int);
extern void EXM_ClearWindStruct(void);
extern int GivesEnemyDamage();
extern int printf(const char *, ...);

void MAP_updateUnitTrap(TrapUpdateUnit *unit)
{
    TrapUnitTimers *t = &unit->timers;

    if (t->cursol != 0) {
        DrawActiveCursol(unit);
    }
    UwamonoCommonFunc(unit);
    switch (unit->mode) {
    case 0:
        if (unit->target != 0 && !(unit->flags & 0x200000)) {
            if (t->cursol & 1) {
                unit->target->activeFlag = 1;
            } else {
                unit->target->activeFlag = 0;
            }
        }
        if (t->state == 1) {
            if (unit->id == 0x7012) {
                unit->active = 0;
                unit->id = -1;
                ClearUwamonoEffect(unit);
                return;
            }
            unit->mode = 3;
            if (unit->flags & 0x400000) {
                xglSoundEffectNormalID(0x10004, unit->serial + 1);
            } else if (unit->flags & 0x800000) {
                xglSoundEffectNormalID(0x10005, unit->serial + 1);
            } else if (unit->flags & 0x1000000) {
                xglSoundEffectNormalID(0x10003, unit->serial + 1);
            } else if (unit->flags & 0x2000000) {
                xglSoundEffectNormalID(0x10003, unit->serial + 1);
            } else if (unit->flags & 0x4000000) {
                xglSoundEffectNormalID(0x1000A, unit->serial + 1);
            } else if (unit->flags & 0x200000) {
                UwamonoBrokenSe(unit);
            }
            if (unit->flags & 0x200000) {
                Vibration_Set_Strong(0xFF, 0x20, 0x1E);
            } else {
                Vibration_Set_Strong(0xFF, 0x20, 0x5A);
            }
            sefCreateEffectCf(t->effectId, unit->position, unit->unmodeled_20);
            unit->flags |= 0x20004;
            ClearUwamonoEffect(unit);
            SetUwaWind(unit);
            if (t->methodArg != -1) {
                CallMethod_I(D_004DB728, t->methodArg);
            }
        }
        if (t->state == 2) {
            if (unit->id == 0x7012) {
                unit->active = 0;
                unit->id = -1;
                ClearUwamonoEffect(unit);
                return;
            }
            unit->mode = 1;
            UwamonoBrokenSe(unit);
            sefCreateEffectCf(0x259, unit->position, unit->unmodeled_20);
            unit->flags |= 0x20004;
            ClearUwamonoEffect(unit);
            SetUwaWind(unit);
            if (t->methodArg != -1) {
                GameLoopState.unk1C = 0;
                CallMethod_I(D_004DB728, t->methodArg);
            }
            Vibration_Set_Strong(0xFF, 0x20, 0x1E);
        }
        break;
    case 3:
        unit->counter++;
        if (unit->counter >= 5) {
            t->wind = 0;
            EXM_ClearWindStruct();
        }
        if (unit->counter < t->life) {
            GivesEnemyDamage(unit);
        } else if (unit->counter > t->life) {
            if (printflg) {
                printf(D_004CA448, unit->id, unit->serial);
            }
            unit->active = 0;
            unit->id = -1;
            EXM_ClearWindStruct();
        }
        break;
    case 1:
        unit->counter++;
        if (unit->counter >= 5) {
            t->wind = 0;
            if (printflg) {
                printf(D_004CA448, unit->id, unit->serial);
            }
            unit->active = 0;
            unit->id = -1;
            EXM_ClearWindStruct();
        }
        break;
    }
}

#include "shared.h"

typedef struct {
    unsigned long long xy;
    float z;
    float w;
} TrapPosition;

typedef struct {
    unsigned char unmodeled_00[0x24];
    float damageRadius;
} TrapDamageProperties;

typedef struct {
    unsigned int flags;
    unsigned char unmodeled_04[0x0c];
    TrapPosition position;
    unsigned long long unmodeled_20;
    unsigned char unmodeled_28[0x178];
    TrapDamageProperties trap;
} TrapMapUnit;

typedef struct {
    unsigned int flags;
    unsigned char unmodeled_04[0x0c];
    TrapPosition position;
    unsigned char unmodeled_20[0xa50];
} TrapDamageActor;

typedef struct {
    unsigned char unmodeled_00[0x20];
    TrapPosition trapPosition;
    unsigned long long unmodeled_30;
    unsigned char unmodeled_38[0x3878];
} TrapEnemyWorkEntry;

typedef struct {
    volatile unsigned int red;
    volatile unsigned int green;
    volatile unsigned int blue;
    volatile unsigned int mode;
    TrapPosition sphere;
} TrapCollisionRecord;

extern TrapDamageActor actor[64];
extern TrapEnemyWorkEntry enepc[16];
extern signed char collflg;
extern signed char printflg;
const char D_004CA470[24] = "act[%d] Electric\n";
const char D_004CA488[24] = "act[%d] Explosion\n";
const char D_004CA4A0[16] = "act[%d] Fall\n";
const char D_004CA4B0[16] = "act[%d] Slot\n";
const char D_004CA4C0[16] = "act[%d] Seal\n";
extern float CheckDist3D(const TrapPosition *, const TrapPosition *);
extern int printf(const char *, ...);

int GivesEnemyDamage(TrapMapUnit *unit)
{
    /* The collflg branch writes a color, mode and sphere to a local record;
     * its emitted stores remain present even though no reader is evidenced. */
    TrapCollisionRecord collisionRecord;
    volatile float *collisionRadius = &collisionRecord.sphere.w;
    int actorIndex;
    TrapDamageActor *target;
    TrapDamageProperties *trap = &unit->trap;

    if (unit->flags & 0x00200000u) {
        return 0;
    }
    if (collflg) {
        collisionRecord.blue = 255;
        collisionRecord.sphere = unit->position;
        *collisionRadius = trap->damageRadius;
        collisionRecord.mode = 128;
        collisionRecord.red = 255;
        collisionRecord.green = 255;
    }
    for (actorIndex = 0; actorIndex < 64; actorIndex++) {
        target = &actor[actorIndex];
        if (target->flags & 8) {
            continue;
        }
        if (!(target->flags & 0x00010000u)) {
            continue;
        }
        if (target->flags & 0x00800000u) {
            continue;
        }
        enepc[actorIndex].trapPosition = unit->position;
        if (!(CheckDist3D(&actor[actorIndex].position, &unit->position) < trap->damageRadius)) {
            continue;
        }
        target->flags |= 0x00800000u;
        if (unit->flags & 0x00400000u) {
            target->flags |= 0x02000000u;
            if (printflg) {
                printf(D_004CA470, actorIndex);
            }
        } else if (unit->flags & 0x00800000u) {
            target->flags |= 0x01000000u;
            if (printflg) {
                printf(D_004CA488, actorIndex);
            }
        } else if (unit->flags & 0x01000000u) {
            target->flags |= 0x04000000u;
            if (printflg) {
                printf(D_004CA4A0, actorIndex);
            }
        } else if (unit->flags & 0x02000000u) {
            if (printflg) {
                printf(D_004CA4B0, actorIndex);
            }
        } else if (unit->flags & 0x04000000u) {
            target->flags |= 0x08000000u;
            if (printflg) {
                printf(D_004CA4C0, actorIndex);
            }
        }
    }
    return 0;
}

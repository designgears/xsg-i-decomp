#include "common.h"

INCLUDE_ASM("asm/main/nonmatchings/init_map_trap", InitMapTrap);

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

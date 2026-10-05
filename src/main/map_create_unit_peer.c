#include "common.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "map_create_unit_peer.h"

INCLUDE_ASM("asm/main/nonmatchings/map_create_unit_peer", MAP_createUnitPeer);

INCLUDE_ASM("asm/main/nonmatchings/map_create_unit_peer", SEQ_rotYCNSUnitChr);

INCLUDE_ASM("asm/main/nonmatchings/map_create_unit_peer", SEQ_transCNSUnitChr);

INCLUDE_ASM("asm/main/nonmatchings/map_create_unit_peer", SEQ_rotateUnit);

INCLUDE_ASM("asm/main/nonmatchings/map_create_unit_peer", SEQ_scaleUnit);

INCLUDE_ASM("asm/main/nonmatchings/map_create_unit_peer", SEQ_rotateUnitSPL);

extern void SPL_getValueXYZ(float *destination, void *spline, float frame);
typedef struct {
    void *spline;
    short frame;
    short end;
} SeqSplTrack;

void SEQ_scaleUnitSPL(MapUnitMotionRecord *unit)
{
    MapUnitGroupSequenceEntry *seq = &unitSequence[((unsigned char *)unit)[0xA0]];
    SeqSplTrack *track = (SeqSplTrack *)((unsigned char *)seq + 0x1B8);
    void *spline;
    float v[4];

    spline = track->spline;
    if (!(seq->flags & 0xE0)) {
        seq->flags |= 0xE0;
        track->frame = 0;
    }
    SPL_getValueXYZ(v, spline, (float)track->frame);
    unit->scale.x = v[0];
    unit->scale.y = v[1];
    unit->scale.z = v[2];
    if (track->end < ++track->frame) {
        seq->state &= ~0xE0;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/map_create_unit_peer", SEQ_moveUnitSPL);

INCLUDE_ASM("asm/main/nonmatchings/map_create_unit_peer", SEQ_moveUnitChr);

INCLUDE_ASM("asm/main/nonmatchings/map_create_unit_peer", SEQ_moveUnitXZ);

INCLUDE_ASM("asm/main/nonmatchings/map_create_unit_peer", SEQ_motionUnit);

/* Still asm; declared here until its own TU is published. */
extern void ANM_resetDefault(MapUnitAnmState *anim, int param);

void MAP_setUnitMotion(MapUnitMotionRecord *unit, int motionId)
{
    unit->anim.motionId = (short)motionId;
    ANM_resetDefault(&unit->anim, unit->animResetParam);
}

void MAP_updateUnitMotion(void)
{
}

int MAP_callUnitGroup(int group, void (*callback)(int *))
{
    MapUnitGroupSequenceEntry *sequence_entry;
    int index;

    if (callback != 0) {
        for (index = 0; index < 64; index++) {
            sequence_entry = &unitSequence[index];
            if (sequence_entry->group == group) {
                callback(&MapUnit[index].flags);
            }
        }
    }
    return 0;
}

/* play.c (main); no header is published for it yet (also declared this way
 * in src/main/near_dir.c). */
extern void *PLAY_getCurrent(void);
/* Still asm; declared here until its own TU is published. */
extern void SEQ_motionUnit(MapUnitMotionRecord *unit);

void MAP_updateUnitMPack(MapUnitMotionRecord *unit)
{
    void *play = PLAY_getCurrent();

    xglMatrixStackUnit();
    xglMatrixStackTrans(&unit->position.x);
    xglMatrixStackRotX(unit->rotation.x);
    xglMatrixStackRotY(unit->rotation.y);
    xglMatrixStackRotZ(unit->rotation.z);
    xglMatrixStackScale(&unit->scale.x);
    xglMatrixStackSave(unit->matrix);
    SEQ_motionUnit(unit);

    /* PLAY_getCurrent's object (play.c) is not recovered; +0x44 is the one
     * float this TU reads from it, mirrored into the unit's own motionTime. */
    unit->anim.motionTime = *(float *)((unsigned char *)play + 0x44);
}

void MAP_updateUnitSequence(MapUnitRecord *unit)
{
    MapUnitGroupSequenceEntry *sequence_entry;
    int callback_index;

    sequence_entry = &unitSequence[unit->serial];
    for (callback_index = 0; callback_index < 4; callback_index++) {
        if (sequence_entry->update_callbacks[callback_index] != 0) {
            sequence_entry->update_callbacks[callback_index](unit);
        }
    }

    if (sequence_entry->state == 0) {
        unit->flags &= ~2u;
        sequence_entry->flags = 0;
        sequence_entry->update_callbacks[0] = 0;
        sequence_entry->update_callbacks[1] = 0;
        sequence_entry->update_callbacks[2] = 0;
        sequence_entry->update_callbacks[3] = 0;
    }

    if (unit->flags & MAP_UNIT_FOLLOW_GROUND) {
        float ground_height = UnduGet(unit->position.x, unit->position.z);

        if (ground_height != -1000.0f) {
            unit->position.y = ground_height;
        }
    }

    xglMatrixStackUnit();
    xglMatrixStackTrans(&unit->position.x);
    xglMatrixStackRotX(unit->rotation.x);
    xglMatrixStackRotY(unit->rotation.y);
    xglMatrixStackRotZ(unit->rotation.z);
    xglMatrixStackScale(&unit->scale.x);
    /* Preserve the matrix-save call before the shared register epilogue. */
    do {
        xglMatrixStackSave(unit->matrix);
    } while (0);
}

void MAP_updateUnitDefault(MapUnitRecord *unit)
{
    if (unit->flags & MAP_UNIT_FOLLOW_GROUND) {
        float undulation = UnduGet(unit->position.x, unit->position.z);

        if (undulation != -1000.0f)
            unit->position.y = undulation;
    }

    xglMatrixStackUnit();
    xglMatrixStackTrans(&unit->position.x);
    xglMatrixStackRotX(unit->rotation.x);
    xglMatrixStackRotY(unit->rotation.y);
    xglMatrixStackRotZ(unit->rotation.z);
    xglMatrixStackScale(&unit->scale.x);
    xglMatrixStackSave(unit->matrix);
}

typedef struct {
    int a0, a4, a8, aC, a10, a14, a18, a1C, a20, a24, a28, a2C, a30;
    char pad[0x260 - 0x34];
} MAPIUS_Seq;
typedef struct {
    int f0;
    void *f4;
    char pad8[0xA1 - 8];
    char fA1;
    char padA2[6];
    short fA8;
    char padAA[0xD0 - 0xAA];
    int fD0;
    char padD4[0x300 - 0xD4];
} MAPIUS_Unit;

void MAP_initUnitSequance(void)
{
    MAPIUS_Unit *unit = (MAPIUS_Unit *)MapUnit;
    MAPIUS_Seq *seq = (MAPIUS_Seq *)unitSequence;
    int i;

    for (i = 0; i < 64; i++) {
        seq->a0 = 0;
        seq->a4 = 0;
        seq->aC = 0;
        seq->a14 = 0;
        seq->a18 = 0;
        seq->a1C = 0;
        seq->a20 = 0;
        seq->a24 = 0;
        seq->a28 = 0;
        seq->a2C = 0;
        seq->a30 = 0;
        seq++;
    }
    for (i = 0; i < 64; i++) {
        unit->fD0 = 0;
        unit->f0 = 0;
        unit->fA1 = 0;
        unit->fA8 = 0;
        unit->f4 = MAP_updateUnitDefault;
        unit++;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/map_create_unit_peer", MAP_updateUnitPartsSequence);

INCLUDE_ASM("asm/main/nonmatchings/map_create_unit_peer", MAP_updateUnitPartsSequence2);

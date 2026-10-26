#include "common.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "map_create_unit_peer.h"

extern void *MAP_createUnit(int a0, int a1);
#define U8(p, o) (*(unsigned char *)((unsigned char *)(p) + (o)))
#define S16(p, o) (*(short *)((unsigned char *)(p) + (o)))
#define S32(p, o) (*(int *)((unsigned char *)(p) + (o)))
#define F32(p, o) (*(float *)((unsigned char *)(p) + (o)))

void *MAP_createUnitPeer(int a0, int type)
{
    MapUnitRecord *u;
    MapUnitGroupSequenceEntry *s;

    u = (MapUnitRecord *)MAP_createUnit(a0, type);
    if (u != 0) {
        F32(u, 0x108) = 0.033333335f;
        S16(u, 0x118) = 0x1FF;
        U8(u, 0xA1) = 0;
        F32(u, 0x3C) = 1.0f;
        F32(u, 0x38) = 1.0f;
        F32(u, 0x34) = 1.0f;
        s = (MapUnitGroupSequenceEntry *)((unsigned char *)unitSequence + u->serial * sizeof(MapUnitGroupSequenceEntry));
        F32(u, 0x30) = 1.0f;
        F32(u, 0xD0) = 0.0f;
        S16(u, 0xA8) = 0;
        if (type != 0x6025) {
            S32(u, 0x4) = 0;
        }
        S32(s, 0x30) = 0;
        S32(s, 0x0) = 0;
        S32(s, 0x4) = 0;
        S32(s, 0x240) = 0;
        S32(s, 0x244) = 0;
        S32(s, 0x248) = 0;
        F32(s, 0x24C) = 1.0f;
        F32(s, 0x250) = 1.0f;
        S32(s, 0x254) = 0;
        S32(s, 0x258) = 0;
        S32(s, 0xC) = 0;
        S16(s, 0x10) = -1;
        S32(s, 0x14) = 0;
        S32(s, 0x18) = 0;
        S32(s, 0x1C) = 0;
        S32(s, 0x20) = 0;
        S32(s, 0x24) = 0;
        S32(s, 0x28) = 0;
        S32(s, 0x2C) = 0;
        F32(s, 0x25C) = 1.0f;
    }
    return u;
}

extern float xglAtan2(float x, float y);

void SEQ_rotYCNSUnitChr(MapUnitMotionRecord *unit)
{
    MapUnitGroupSequenceEntry *seq = &unitSequence[((unsigned char *)unit)[0xA0]];
    unsigned char *p = (unsigned char *)seq + 0xB8;
    float *m;
    float *pos = (float *)((unsigned char *)unit + 0x10);
    float *rot = (float *)((unsigned char *)unit + 0x20);
    float v[4];
    float r[3];

    v[0] = F32(p, 0x30);
    v[1] = F32(p, 0x34);
    v[2] = F32(p, 0x38);
    v[3] = 1.0f;
    m = (float *)(S32(seq, 0xCC) * 64 + S32(*(void **)p, 0x824));
    seq->state = 1;
    r[0] = v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + m[12];
    r[1] = v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + m[13];
    r[2] = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + m[14];
    rot[1] = xglAtan2(r[0] - pos[0], r[2] - pos[2]);
}

void SEQ_transCNSUnitChr(MapUnitMotionRecord *unit)
{
    MapUnitGroupSequenceEntry *seq = &unitSequence[((unsigned char *)unit)[0xA0]];
    unsigned char *p = (unsigned char *)seq + 0x38;
    float *m;
    float *out = (float *)((unsigned char *)unit + 0x10);
    float v[4];
    float r[3];

    v[0] = F32(p, 0x10);
    v[1] = F32(p, 0x14);
    v[2] = F32(p, 0x18);
    v[3] = 1.0f;
    m = (float *)(S32(seq, 0x78) * 64 + S32(*(void **)p, 0x824));
    seq->state = 1;
    r[0] = v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + m[12];
    r[1] = v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + m[13];
    r[2] = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + m[14];
    out[0] = r[0];
    out[1] = r[1];
    out[2] = r[2];
    out[3] = v[3];
}

extern float nearDir(float a, float b);

void SEQ_rotateUnit(MapUnitMotionRecord *unit)
{
    float *pos = (float *)((unsigned char *)unit + 0x10);
    float *rot = (float *)((unsigned char *)unit + 0x20);
    MapUnitGroupSequenceEntry *seq = &unitSequence[((unsigned char *)unit)[0xA0]];
    unsigned char *track = (unsigned char *)seq + 0xB8;
    float *start = (float *)((unsigned char *)seq + 0xD8);
    float *target = (float *)((unsigned char *)seq + 0xE8);
    float *delta = (float *)((unsigned char *)seq + 0xF8);
    unsigned char *obj;
    float diff[4];
    float inv;
    int frames;
    unsigned int flags;
    unsigned int state;
    unsigned int k2, k4, k8;

    flags = seq->flags;
    obj = *(unsigned char **)track;
    if (!(flags & 2)) {
        state = seq->state;
        if (state & 2) {
            start[0] = rot[0];
            if (S32(track, 0x1C) & 1) {
                target[1] = xglAtan2(F32(obj, 0x14) - pos[1], F32(obj, 0x18) - pos[2]);
            }
            frames = S32(track, 0x10);
            if (frames > 0) {
                inv = 1.0f;
                inv = inv / (float)frames;
                delta[0] = nearDir(start[0], target[0]) * inv;
            }
            S32(seq, 0x34) = 0;
            k2 = seq->flags | 2;
            seq->flags = k2;
            flags = k2;
            state = seq->state;
        } else {
            delta[0] = 0.0f;
            k2 = flags | 2;
            seq->flags = k2;
            flags = k2;
        }
    } else {
        state = seq->state;
    }
    if (!(flags & 4)) {
        if (state & 4) {
            start[1] = rot[1];
            if (S32(track, 0x1C) & 2) {
                target[1] = xglAtan2(F32(obj, 0x10) - pos[0], F32(obj, 0x18) - pos[2]);
            }
            frames = S32(track, 0x14);
            if (frames > 0) {
                inv = 1.0f;
                inv = inv / (float)frames;
                delta[1] = nearDir(start[1], target[1]) * inv;
            }
            S32(seq, 0x34) = 0;
            k4 = seq->flags | 4;
            seq->flags = k4;
            flags = k4;
            state = seq->state;
        } else {
            delta[1] = 0.0f;
            k4 = flags | 4;
            seq->flags = k4;
            flags = k4;
        }
    }
    if (!(flags & 8)) {
        if (state & 8) {
            start[2] = rot[2];
            if (S32(track, 0x1C) & 4) {
                target[1] = xglAtan2(F32(unit, 0x10) - pos[0], F32(unit, 0x14) - pos[1]);
            }
            frames = S32(track, 0x18);
            if (frames > 0) {
                inv = 1.0f;
                inv = inv / (float)frames;
                delta[2] = nearDir(start[2], target[2]) * inv;
            }
            S32(seq, 0x34) = 0;
            k8 = seq->flags | 8;
            seq->flags = k8;
            flags = k8;
            state = seq->state;
        } else {
            delta[2] = 0.0f;
            k8 = flags | 8;
            seq->flags = k8;
            flags = k8;
        }
    }
    if (state & 2) {
        if (S32(track, 0x1C) & 1) {
            target[1] = xglAtan2(F32(unit, 0x14) - pos[1], F32(unit, 0x18) - pos[2]);
        }
        diff[0] = __builtin_fabsf(nearDir(target[0], rot[0]));
        if (diff[0] <= __builtin_fabsf(delta[0])) {
            rot[0] = target[0];
            state = seq->state = seq->state & ~2;
        } else {
            rot[0] = rot[0] + delta[0];
            state = seq->state;
        }
    }
    if (state & 4) {
        if (S32(track, 0x1C) & 2) {
            target[1] = xglAtan2(F32(unit, 0x10) - pos[0], F32(unit, 0x18) - pos[2]);
        }
        diff[1] = __builtin_fabsf(nearDir(target[1], rot[1]));
        if (diff[1] <= __builtin_fabsf(delta[1])) {
            rot[1] = target[1];
            state = seq->state = seq->state & ~4;
        } else {
            rot[1] = rot[1] + delta[1];
            state = seq->state;
        }
    }
    if (state & 8) {
        if (S32(track, 0x1C) & 4) {
            target[1] = xglAtan2(F32(unit, 0x10) - pos[0], F32(unit, 0x14) - pos[1]);
        }
        diff[2] = __builtin_fabsf(nearDir(target[2], rot[2]));
        if (diff[2] <= __builtin_fabsf(delta[2])) {
            rot[2] = target[2];
            seq->state = seq->state & ~8;
        } else {
            rot[2] = rot[2] + delta[2];
        }
    }
}

void SEQ_scaleUnit(MapUnitMotionRecord *unit)
{
    float *scale = (float *)((unsigned char *)unit + 0x30);
    MapUnitGroupSequenceEntry *seq = &unitSequence[((unsigned char *)unit)[0xA0]];
    unsigned char *track = (unsigned char *)seq + 0x1B8;
    float *start = (float *)((unsigned char *)seq + 0x1D8);
    float *target = (float *)((unsigned char *)seq + 0x1E8);
    float *delta = (float *)((unsigned char *)seq + 0x1F8);
    float diff[4];
    float p;
    int frames;
    unsigned int flags;
    unsigned int state;
    unsigned int keep;
    unsigned int *_ap;

    flags = seq->flags;
    if (!(flags & 0x20)) {
        state = seq->state;
        if (*(_ap = &state) & 0x20) {
            p = scale[0];
            frames = S32(track, 0x10);
            start[0] = p;
            if (frames > 0) {
                delta[0] = (target[0] - p) * (1.0f / (float)frames);
            }
            keep = flags | 0x20;
            S32(seq, 0x34) = 0;
            seq->flags = keep;
            flags = keep;
        } else {
            delta[0] = 0.0f;
            keep = flags | 0x20;
            seq->flags = keep;
            flags = keep;
        }
    } else {
        state = seq->state;
    }
    if (!(flags & 0x40)) {
        if (state & 0x40) {
            p = scale[1];
            frames = S32(track, 0x14);
            start[1] = p;
            if (frames > 0) {
                delta[1] = (target[1] - p) * (1.0f / (float)frames);
            }
            keep = flags | 0x40;
            S32(seq, 0x34) = 0;
            seq->flags = keep;
            flags = keep;
        } else {
            delta[1] = 0.0f;
            keep = flags | 0x40;
            seq->flags = keep;
            flags = keep;
        }
    }
    if (!(flags & 0x80)) {
        if (state & 0x80) {
            p = scale[2];
            frames = S32(track, 0x18);
            start[2] = p;
            if (frames > 0) {
                delta[2] = (target[2] - p) * (1.0f / (float)frames);
            }
            keep = flags | 0x80;
            S32(seq, 0x34) = 0;
            seq->flags = keep;
            flags = keep;
        } else {
            delta[2] = 0.0f;
            keep = flags | 0x80;
            seq->flags = keep;
            flags = keep;
        }
    }
    if (state & 0x20) {
        diff[0] = __builtin_fabsf(target[0] - scale[0]);
        if (diff[0] <= __builtin_fabsf(delta[0])) {
            scale[0] = target[0];
            keep = state & ~0x20;
            seq->state = keep;
            state = keep;
        } else {
            scale[0] = scale[0] + delta[0];
        }
    }
    if (state & 0x40) {
        diff[1] = __builtin_fabsf(target[1] - scale[1]);
        if (diff[1] <= __builtin_fabsf(delta[1])) {
            scale[1] = target[1];
            keep = state & ~0x40;
            seq->state = keep;
            state = keep;
        } else {
            scale[1] = scale[1] + delta[1];
        }
    }
    if (state & 0x80) {
        diff[2] = __builtin_fabsf(target[2] - scale[2]);
        if (diff[2] <= __builtin_fabsf(delta[2])) {
            scale[2] = target[2];
            seq->state = state & ~0x80;
        } else {
            scale[2] = scale[2] + delta[2];
        }
    }
}

extern void SPL_getValueXYZ(float *destination, void *spline, float frame);
extern float D_004D8788;
typedef struct {
    void *spline;
    short frame;
    short end;
} SeqRotSplTrack;

void SEQ_rotateUnitSPL(MapUnitMotionRecord *unit)
{
    MapUnitGroupSequenceEntry *seq = &unitSequence[((unsigned char *)unit)[0xA0]];
    SeqRotSplTrack *track = (SeqRotSplTrack *)((unsigned char *)seq + 0xB8);
    void *spline;
    float v[4];

    spline = track->spline;
    if (!(seq->flags & 0xE)) {
        seq->flags |= 0xE;
        track->frame = 0;
    }
    SPL_getValueXYZ(v, spline, (float)track->frame);
    unit->rotation.x = v[0] / 180.0f * D_004D8788;
    unit->rotation.y = v[1] / 180.0f * D_004D8788;
    unit->rotation.z = v[2] / 180.0f * D_004D8788;
    if (track->end < ++track->frame) {
        seq->state &= ~0xE;
    }
}

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

extern float xglAtan2(float x, float y);
typedef struct {
    void *spline;
    short frame;
    short end;
    unsigned short flags;
} SeqSplMoveTrack;

void SEQ_moveUnitSPL(MapUnitMotionRecord *unit)
{
    MapUnitGroupSequenceEntry *seq = &unitSequence[((unsigned char *)unit)[0xA0]];
    SeqSplMoveTrack *track = (SeqSplMoveTrack *)((unsigned char *)seq + 0x38);
    void *spline;
    float v[4];

    spline = track->spline;
    if (!(seq->flags & 1)) {
        seq->flags |= 1;
        track->frame = 0;
    }
    SPL_getValueXYZ(v, spline, (float)track->frame);
    if (!(seq->state & 4) && (track->flags & 0x10)) {
        unit->rotation.y = xglAtan2(v[0] - unit->position.x, v[2] - unit->position.z);
    }
    unit->position.x = v[0];
    unit->position.y = v[1];
    unit->position.z = v[2];
    if (track->end < ++track->frame) {
        seq->state &= ~1;
    }
}

float xglSin(float);
float xglCos(float);

void SEQ_moveUnitChr(MapUnitMotionRecord *unit)
{
    float *pos = (float *)((unsigned char *)unit + 0x10);
    float *rot = (float *)((unsigned char *)unit + 0x20);
    MapUnitGroupSequenceEntry *seq = &unitSequence[((unsigned char *)unit)[0xA0]];
    unsigned char *track = (unsigned char *)seq + 0x38;
    float *start = (float *)((unsigned char *)seq + 0x48);
    float *target = (float *)(*(unsigned char **)track + 0x10);
    float *dist = (float *)((unsigned char *)seq + 0x64);
    float delta[4];
    float disp[4];
    float angle;
    float px;
    float pz;
    int frames;
    unsigned int keep;

    if (!(seq->flags & 1)) {
        px = pos[0];
        seq->flags |= 1;
        start[0] = px;
        frames = S32(track, 0x4);
        start[1] = pos[1];
        pz = pos[2];
        start[2] = pz;
        if (frames > 0) {
            float step = 1.0f / (float)frames;
            delta[0] = (target[0] - px) * step;
            delta[2] = (target[2] - pz) * step;
            delta[1] = 0.0f;
            xglVectorLength(dist, (Vector4 *)delta);
        }
        xglAtan2(target[0] - pos[0], target[2] - pos[2]);
        S32(seq, 0x34) = 0;
    }
    angle = xglAtan2(target[0] - pos[0], target[2] - pos[2]);
    if (!(seq->state & 4)) {
        rot[1] = angle;
    }
    disp[0] = target[0] - pos[0];
    disp[2] = target[2] - pos[2];
    disp[3] = disp[0] * disp[0] + disp[2] * disp[2];
    disp[3] = __builtin_sqrtf(disp[3]);
    if (disp[3] < __builtin_sqrtf((*dist + 0.5f) * (*dist + 0.5f))) {
        keep = seq->state & ~1;
        seq->state = keep;
        if (!(keep & ~0x10)) {
            seq->state = 0;
        }
        return;
    }
    pos[0] = pos[0] + xglSin(angle) * *dist;
    pos[2] = pos[2] + xglCos(angle) * *dist;
    S32(seq, 0x34) += 1;
}

extern float xglSin(float angle);
extern float xglCos(float angle);

void SEQ_moveUnitXZ(MapUnitMotionRecord *unit)
{
    float *pos = (float *)((unsigned char *)unit + 0x10);
    float *rot = (float *)((unsigned char *)unit + 0x20);
    MapUnitGroupSequenceEntry *seq = &unitSequence[((unsigned char *)unit)[0xA0]];
    unsigned char *track = (unsigned char *)seq + 0x38;
    float *start = (float *)((unsigned char *)seq + 0x48);
    float *target = (float *)((unsigned char *)seq + 0x58);
    float *dist = (float *)((unsigned char *)seq + 0x64);
    float delta[4];
    float disp[4];
    float angle;
    float px;
    float pz;
    int frames;
    unsigned int keep;

    if (!(seq->flags & 1)) {
        px = pos[0];
        seq->flags |= 1;
        start[0] = px;
        frames = S32(track, 0x4);
        start[1] = pos[1];
        pz = pos[2];
        start[2] = pz;
        if (frames > 0) {
            float step = 1.0f / (float)frames;
            delta[0] = (target[0] - px) * step;
            delta[1] = 0.0f;
            delta[2] = (target[2] - pz) * step;
            xglVectorLength(dist, (Vector4 *)delta);
            xglAtan2(target[0] - pos[0], target[2] - pos[2]);
        }
        S32(seq, 0x34) = 0;
    }
    angle = xglAtan2(target[0] - pos[0], target[2] - pos[2]);
    if (!(seq->state & 4)) {
        rot[1] = angle;
    }
    disp[0] = target[0] - pos[0];
    disp[2] = target[2] - pos[2];
    disp[3] = disp[0] * disp[0] + disp[2] * disp[2];
    disp[3] = __builtin_sqrtf(disp[3]);
    if (disp[3] <= __builtin_sqrtf(*dist * *dist)) {
        keep = seq->state & ~1;
        seq->state = keep;
        if (!(keep & ~0x10)) {
            seq->state = 0;
        }
    }
    pos[0] = pos[0] + xglSin(angle) * *dist;
    pos[2] = pos[2] + xglCos(angle) * *dist;
    S32(seq, 0x34) += 1;
}

extern void MAP_setUnitMotion(MapUnitMotionRecord *unit, int motionId);
typedef struct {
    int pad0[2];
    int motion;
    int pad1;
    short start;
    short end;
    int pad2;
    int flags;
    float speed;
} SeqMotionTrack;
typedef struct {
    int flags;
    float time;
    float speed;
    float start;
    float end;
} SeqMotionUnit;

void SEQ_motionUnit(MapUnitMotionRecord *unit)
{
  MapUnitGroupSequenceEntry *seq = &unitSequence[((unsigned char *) unit)[0xA0]];
  SeqMotionUnit *u = (SeqMotionUnit *) (((unsigned char *) unit) + 0x100);
  SeqMotionTrack *track = (SeqMotionTrack *) (((unsigned char *) seq) + 0x138);
  if (!(seq->flags & 0x10))
  {
    seq->flags |= 0x10;
    MAP_setUnitMotion(unit, track->motion);
    u->flags = track->flags;
    if (track->start >= 0)
    {
      float st = track->start * 0.033333335f;
      float en = track->end * 0.033333335f;
      u->start = st;
      u->time = st;
      u->end = en;
    }
    u->speed = track->speed * 0.033333335f;
  }
  if (!(seq->state & (~0x10)))
  {
    if (u->flags & 0x1000)
    {
      seq->state = 0;
    }
  }
}

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

typedef signed int s32;
#define NULL 0 /* draft_fix */
extern char GameLoopState[];

void MAP_updateUnitPartsSequence(MapUnitRecord *arg0)
{
  MapUnitGroupSequenceEntry *temp_18;
  int new_var;
  char *new_var3;
  s16 temp_4_2;
  s32 var_19;
  void (**var_16)(MapUnitRecord *);
  void (*var_2)(MapUnitRecord *);
  void *temp_16;
  void *temp_18_2;
  void *temp_4;
  short new_var2;
  var_19 = 3;
  temp_18 = &unitSequence[arg0->serial];
  var_16 = temp_18->update_callbacks;
  var_2 = temp_18->update_callbacks[0];

  loop_1:
  var_16++;

  new_var = var_2 != 0;
  do
  {
    if (new_var)
    {
      var_2(arg0);
    }
  }
  while (0);
  var_19 -= 1;
  if (var_19 >= 0)
  {
    var_2 = *var_16;
    goto loop_1;
  }
  if (temp_18->state == 0)
  {
    temp_18->update_callbacks[3] = 0;
    temp_18->update_callbacks[0] = 0;
    temp_18->update_callbacks[1] = 0;
    temp_18->update_callbacks[2] = 0;
    arg0->flags &= ~2;
    temp_18->flags = 0;
  }
  if (arg0->flags & 0x100)
  {
    arg0->position.y = UnduGet(arg0->position.x, arg0->position.z);
  }
  if (0 == (*((int *) (((char *) arg0) + 0xD4))))
  {
    temp_16 = *((int *) (GameLoopState + 0x54));
    if (temp_16 == 0)
    {
      return;
    }
    temp_4 = *((int *) (((char *) temp_16) + 0x4));
    do
    {
      if (temp_4 == 0)
      {
        return;
      }
      temp_18_2 = temp_4 + (*((int *) (((char *) temp_4) + 0x50)));
      xglMatrixStackUnit();
      xglMatrixStackTrans(&(*arg0).position.x);
      xglMatrixStackRotX(arg0->rotation.x);
      xglMatrixStackRotY(arg0->rotation.y);
    }
    while (0);
    xglMatrixStackRotZ(arg0->rotation.z);
    xglMatrixStackScale(&arg0->scale.x);
    new_var = *((int *) (((char *) (*((int *) (((char *) temp_16) + 0x4)))) + 0x40));
    new_var2 = *((short *) (((char *) arg0) + 0xA4));
    temp_4_2 = new_var2;
    if (temp_4_2 < new_var)
    {
      xglMatrixStackSave(temp_18_2 + (temp_4_2 << 6));
    }
  }
  return;
}

void MAP_updateUnitPartsSequence2(MapUnitRecord *unit)
{
    MapUnitGroupSequenceEntry *sequence_entry;
    unsigned char *display;
    unsigned char *model;
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
        unit->position.y = UnduGet(unit->position.x, unit->position.z);
    }

    if (*(int *)((unsigned char *)unit + 0xD4) == 0) {
        display = *(unsigned char **)(GameLoopState + 0x54);
        if (display != 0 && *(unsigned char **)(display + 4) != 0) {
            Matrix4 *matrices;
            Matrix4 local;
            Matrix4 aligned;
            Matrix4 inverse;
            Vector4 delta;
            Vector4 angles;
            Vector4 centre;
            Vector4 negated;
            float rot_x;
            float rot_y;
            float rz;

            model = *(unsigned char **)(display + 4);
            matrices = (Matrix4 *)(model + *(int *)(model + 0x50));

            /* Aligned matrix copies use the SDK's scratch-register quartet. */
            __asm__ __volatile__(
                "lq $2, 0(%1)\n"
                "sq $2, 0(%0)\n"
                "lq $2, 16(%1)\n"
                "sq $2, 16(%0)\n"
                "lq $2, 32(%1)\n"
                "sq $2, 32(%0)\n"
                "lq $2, 48(%1)\n"
                "sq $2, 48(%0)\n"
                : : "r" (local), "r" (unit->matrix)
                : "$2", "memory");
            local[3][0] = 0.0f;
            local[3][3] = 1.0f;
            local[3][1] = 0.0f;
            local[3][2] = 0.0f;
            switch ((int)*(float *)((unsigned char *)sequence_entry + 0x25C)) {
            case 1:
                alignXAxis((float *)((unsigned char *)sequence_entry + 0x250), aligned);
                break;
            case 2:
                alignYAxis((float *)((unsigned char *)sequence_entry + 0x250), aligned);
                break;
            case 3:
                alignZAxis((float *)((unsigned char *)sequence_entry + 0x250), aligned);
                break;
            }
            xglMatrixStackLoad(aligned);
            xglMatrixStackReverse();
            xglMatrixStackSave(inverse);
            xglMatrixStackLoad(aligned);
            xglMatrixStackMul((const float (*)[4])local);
            __asm__ __volatile__(
                "lqc2 $vf20, 0(%1)\n"
                "lqc2 $vf21, 0(%2)\n"
                "vsub.xyzw $vf20xyzw, $vf20xyzw, $vf21xyzw\n"
                "sqc2 $vf20, 0(%0)\n"
                : : "r" (&delta), "r" ((unsigned char *)sequence_entry + 0x240), "r" (unit->matrix[3])
                : "memory");
            rot_x = unit->rotation.x;
            rot_y = unit->rotation.y;
            rz = unit->rotation.z;
            angles.x = (rot_x * local[1][0] + rot_x * local[1][1] + rot_x * local[1][2]) * local[2][0]
                     + (rot_x * local[1][0] + rot_x * local[1][1] + rot_x * local[1][2]) * local[2][1]
                     + (rot_x * local[1][0] + rot_x * local[1][1] + rot_x * local[1][2]) * local[2][2];
            angles.y = (rot_y * local[0][0] + rot_y * local[0][1] + rot_y * local[0][2]) * local[2][0]
                     + (rot_y * local[0][0] + rot_y * local[0][1] + rot_y * local[0][2]) * local[2][1]
                     + (rot_y * local[0][0] + rot_y * local[0][1] + rot_y * local[0][2]) * local[2][2];
            angles.z = (rz * local[0][0] + rz * local[0][1] + rz * local[0][2]) * local[1][0]
                     + (rz * local[0][0] + rz * local[0][1] + rz * local[0][2]) * local[1][1]
                     + (rz * local[0][0] + rz * local[0][1] + rz * local[0][2]) * local[1][2];
            centre.x = local[0][0] * delta.x + local[0][1] * delta.y + local[0][2] * delta.z;
            negated.x = -centre.x;
            centre.y = local[1][0] * delta.x + local[1][1] * delta.y + local[1][2] * delta.z;
            negated.y = -centre.y;
            centre.z = local[2][0] * delta.x + local[2][1] * delta.y + local[2][2] * delta.z;
            negated.z = -centre.z;
            centre.w = 1.0f;
            delta.x = centre.x;
            delta.y = centre.y;
            delta.z = centre.z;
            negated.w = 1.0f;
            xglMatrixStackUnit();
            xglMatrixStackTrans(&centre.x);
            xglMatrixStackMul((const float (*)[4])inverse);
            xglMatrixStackRotX(angles.x);
            xglMatrixStackRotY(angles.y);
            xglMatrixStackRotZ(angles.z);
            xglMatrixStackScale(&unit->scale.x);
            xglMatrixStackMul((const float (*)[4])local);
            xglMatrixStackMul((const float (*)[4])aligned);
            xglMatrixStackTrans(&negated.x);
            xglMatrixStackSave(local);
            angles.x = unit->position.x * inverse[0][0] + unit->position.y * inverse[1][0] + unit->position.z * inverse[2][0];
            angles.y = unit->position.x * inverse[0][1] + unit->position.y * inverse[1][1] + unit->position.z * inverse[2][1];
            angles.z = unit->position.x * inverse[0][2] + unit->position.y * inverse[1][2] + unit->position.z * inverse[2][2];
            local[3][0] = local[3][0] + angles.x + unit->matrix[3][0];
            local[3][1] = local[3][1] + angles.y + unit->matrix[3][1];
            local[3][2] = local[3][2] + angles.z + unit->matrix[3][2];
            if (*(short *)((unsigned char *)unit + 0xA4) < *(int *)(*(unsigned char **)(display + 4) + 0x40)) {
                __asm__ __volatile__(
                    "lq $2, 0(%1)\n"
                    "sq $2, 0(%0)\n"
                    "lq $2, 16(%1)\n"
                    "sq $2, 16(%0)\n"
                    "lq $2, 32(%1)\n"
                    "sq $2, 32(%0)\n"
                    "lq $2, 48(%1)\n"
                    "sq $2, 48(%0)\n"
                    : : "r" (*(short *)((unsigned char *)unit + 0xA4) * sizeof(Matrix4) + (unsigned int)matrices), "r" (local)
                    : "$2", "memory");
            }
        }
    }
}

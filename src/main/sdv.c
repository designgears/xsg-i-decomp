#include "common.h"

#include "shared.h"

#include "sdv.h"

#include "m_ef_obj.h"

extern float _sdvMapRgb[4];


extern float _sdvAmbient[4];


extern const float McMathUnitMatrix[4][4];


extern short _sdvSpecialBuf[8];


extern SdvAlter _sdvAlter[16];


extern int _sdvAmbFrame;


extern int _sdvAmbState;


extern unsigned char charID_0;


extern int eftCate_1;


extern unsigned short itmBox[255];


extern unsigned short wpnBox[255];


extern unsigned short bltBox[255];


extern unsigned short accBox[255];


extern unsigned short evtBox[255];


extern long long moneyBox;


extern unsigned char orgData[33][0x180];


extern unsigned char batDatBuf[0x10000];


extern unsigned char thinkBuf[0x4000];


extern MEfObjCamParams mefCamParams;


extern void *memset(void *, int, unsigned int);

extern Vector4 *MMathVectorInterpolation(Vector4 *destination,
                                         const Vector4 *first,
                                         const Vector4 *second,
                                         float parameter);

extern unsigned char *sefGetBattleData(void);

extern short _hitFlag;

extern short _hitSignal;

typedef struct SdvSequence {
    unsigned int event_address;
    unsigned char unmodeled_04[4];
    int elapsed;
    int position;
    int stride;
    int last_sound;
} SdvSequence;

extern void sefMemZero(void *data, unsigned int size);

extern const Vector4 McMathUnitVector;


extern int serial_3;


extern Vector4 *func_A2C3F8(void);

extern int sefGetDmgNull(void);

extern void sefGetPosition(Vector4 *position, void *actor, int part);

extern int _nowEftNo;

extern void func_A31500(float *params, int kind);

extern void func_A31AE0(int kind, void *params);

extern void func_A31BB8(int kind);

extern int func_A31C30(int kind);

extern void func_A31CA0(int value0, int value1, int value2, int value3,
                        int value4);



typedef struct SdvCameraParam {
    int kind;
    unsigned char unmodeled_04[12];
    Vector4 position;
    int actor;
    int actor_number;
    int part;
    int source_actor;
    int source_number;
    int source_part;
    int target_actor;
    int target_number;
    int target_part;
    unsigned char unmodeled_44[40];
    float weight;
} SdvCameraParam;

typedef struct SdvBattleCameraActors {
    int source_actor;
    int target_actor;
    unsigned char unmodeled_08[8];
    short source_number;
    short target_number;
} SdvBattleCameraActors;

void sdvInitAmbient(void)
{
    _sdvAmbFrame = 0;
    _sdvAmbState = 0;
}

void sdvSaveAmbient(void)
{
    void *ambient = xglStudioGetLight2();
    void *map_rgb = func_A2C3F8();

    __asm__ __volatile__(
        "lq $8, 0(%1)\n"
        "sq $8, 0(%0)\n"
        :
        : "r" (_sdvAmbient), "r" (ambient)
        : "$8", "memory");
    __asm__ __volatile__(
        "lq $8, 0(%1)\n"
        "sq $8, 0(%0)\n"
        :
        : "r" (_sdvMapRgb), "r" (map_rgb)
        : "$8", "memory");
    _sdvAmbFrame = 0;
    _sdvAmbState = 0;
}

static void sdvSetAmbStateSub(int state, int effect_no, int force)
{
    if (force != 0) {
        _sdvAmbState = state;
        return;
    }

    srsAnalyzeEftNo(effect_no, &charID_0, &eftCate_1);
    if (eftCate_1 == 0xE || effect_no == 0xB25) {
        if ((unsigned int)(effect_no - 0x8FC) >= 0xB6) {
            _sdvAmbState = state;
        }
    }
}

void sdvSetAmbState(int state, int effect_no)
{
    sdvSetAmbStateSub(state, effect_no, 0);
}

void sdvSetAmbState2(int state, int effect_no)
{
    sdvSetAmbStateSub(state, effect_no, 1);
}

void sdvSetAmbient(void *map_rgb, void *ambient)
{
    xglLightIntensityAmbient(xglStudioGetLight2(), ambient);
    func_A2C3D8(map_rgb);
}

void sdvRestoreAmbient(void)
{
    sdvSetAmbient(_sdvMapRgb, _sdvAmbient);
}

void sdvExecAmbient(void)
{
    Vector4 map_rgb;
    Vector4 ambient;
    float parameter;
    int state = _sdvAmbState;

    if (state == 0) {
        return;
    }

    if (state == 1) {
        if (_sdvAmbFrame >= 20) {
            _sdvAmbState = 0;
            _sdvAmbFrame = 20;
            state = 0;
        } else {
            _sdvAmbFrame++;
        }
    } else if (state == 2) {
        if (_sdvAmbFrame <= 0) {
            _sdvAmbFrame = 0;
            state = 0;
            _sdvAmbState = 0;
        } else {
            _sdvAmbFrame--;
        }
    }

    if (state != 0) {
        float half = 0.5f;
        float divisor = 20.0f;

        parameter = (float)_sdvAmbFrame / divisor;
        /* These constrained operations implement the eight evidenced COP2 instructions. */
        __asm__ __volatile__(
            "lqc2 vf1, 0(%1)\n\t"
            "mfc1 $8, %0\n\t"
            "qmtc2 $8, vf2\n\t"
            "vmulx.xyz vf1, vf1, vf2x\n\t"
            "sqc2 vf1, 0(%2)"
            :
            : "f"(half), "r"(&_sdvMapRgb[0]), "r"(&map_rgb)
            : "$8", "memory");
        __asm__ __volatile__(
            "lqc2 vf1, 0(%1)\n\t"
            "mfc1 $8, %0\n\t"
            "qmtc2 $8, vf2\n\t"
            "vmulx.xyz vf1, vf1, vf2x\n\t"
            "sqc2 vf1, 0(%2)"
            :
            : "f"(half), "r"(&_sdvAmbient[0]), "r"(&ambient)
            : "$8", "memory");
        MMathVectorInterpolation(&map_rgb, (const Vector4 *)_sdvMapRgb,
                                 &map_rgb, parameter);
        MMathVectorInterpolation(&ambient, (const Vector4 *)_sdvAmbient,
                                 &ambient, parameter);
        sdvSetAmbient(&map_rgb, &ambient);
    }
}

int sdvExecSeqTbl(SdvSequence *sequence)
{
    int result = -1;
    const short *events = (const short *)sequence->event_address;
    short opcode = events[sequence->position * sequence->stride];
    unsigned char *battle = sefGetBattleData();

    if (opcode == 1024) {
        return 1024;
    }
    if (opcode == 4096) {
        if (battle[22] == 0) {
            sequence->position++;
        } else {
            int *position = &sequence->position;
            if (_hitFlag != 0 || _hitSignal != 0) {
                result = *position;
                *position = result + 1;
            }
        }
    } else if (sequence->elapsed >= opcode) {
        int *position = &sequence->position;
        result = *position;
        *position = result + 1;
    }

    sequence->elapsed++;
    if (sequence->elapsed >= 1024) {
        sequence->elapsed = 1023;
    }
    return result;
}

int sdvProgressKey(const short *sequence, SdvKeyCursor *cursor, int stride)
{
    int result = -1;
    short opcode = sequence[cursor->position * stride];
    unsigned char *battle = sefGetBattleData();

    if (opcode == 1024) {
        return 1024;
    }
    if (opcode == 4096) {
        if (battle[22] == 0) {
            cursor->position++;
        } else if (_hitFlag != 0 || _hitSignal != 0) {
            result = cursor->position;
            cursor->position++;
        }
    } else if (cursor->elapsed >= opcode) {
        result = cursor->position;
        cursor->position++;
    }

    cursor->elapsed++;
    if (cursor->elapsed >= 1024) {
        cursor->elapsed = 1023;
    }
    return result;
}

void sdvPlaySound(int sound_id, int unused, int flags)
{
    xglSoundEffectNormalID(sound_id, 0);
}

void sdvScheduleSound(SdvSequence *sequence)
{
    struct SdvSoundEvent {
        short command;
        short latch;
        int sound_id;
    };
    short latch;
    int sound_id;
    const struct SdvSoundEvent *event;
    int event_index;
    int word_index;
    unsigned int event_address;

    if (sequence == 0) {
        return;
    }
    if (sequence->event_address == 0) {
        return;
    }
    event_index = sdvExecSeqTbl(sequence);
    if ((unsigned int)event_index >= 1024) {
        return;
    }

    word_index = event_index * sequence->stride;
    event_address = word_index * sizeof(short) + sequence->event_address;
    event = (const struct SdvSoundEvent *)event_address;
    sound_id = event->sound_id;
    latch = event->latch;
    if (sound_id > 0) {
        sdvPlaySound(sound_id, 0, 0);
        if (latch > 0) {
            sequence->last_sound = sound_id;
        }
    }
}

int sdvSelectCamera(SdvCameraChoice *choice, int script_base,
                    unsigned short *table)
{
    int weights[8];
    int candidates[8];
    int chosen = 0;
    int roll = rand() % 100;
    int i;
    int weight_sum;

    if (table == 0 || script_base == 0) {
        return 0;
    }
    for (i = 0; i < 5; i++) {
        candidates[i] = scGetImmAdrImmIdx2(script_base, table, i * 2);
        weights[i] = scGetImmNumIdx((short *)table, i * 2 + 1);
    }

    if (sefIsEntryBoss() != 0 && candidates[4] != 0) {
        chosen = candidates[4];
    } else {
        weight_sum = 0;
        for (i = 0; i < 4; i++) {
            weight_sum += weights[i];
            if (roll < weight_sum && candidates[i] != 0) {
                chosen = candidates[i];
                break;
            }
        }
    }

    if (chosen != 0) {
        choice->camera_commands[0][0] =
            scGetImmAdrImmIdx2(script_base, (unsigned short *)chosen, 0);
        choice->camera_commands[1][0] =
            scGetImmAdrImmIdx2(script_base, (unsigned short *)chosen, 1);
        choice->camera_commands[2][0] =
            scGetImmAdrImmIdx2(script_base, (unsigned short *)chosen, 2);
        choice->camera_commands[3][0] =
            scGetImmAdrImmIdx2(script_base, (unsigned short *)chosen, 3);
    }
    choice->script_base = script_base;
    choice->selected = chosen;
    choice->animation = 0;
    return chosen;
}

static void sdvSetCameraParam(void *camera_data, int selection, const short *position)
{
    SdvCameraParam *camera = camera_data;
    Vector4 origin;
    int part = -1;
    int source = 0;
    int kind;
    void *battle_data = sefGetBattleData();
    SdvBattleCameraActors *battle = battle_data;

    if (selection == 256 || selection == 512) {
        kind = 1;
        part = 0;
        if (selection == 256)
            source = 1;
    } else if ((unsigned int)(selection - 257) < 16) {
        kind = 1;
        part = selection - 256;
        source = 1;
    } else if (selection == 529) {
        kind = 1;
        part = sefGetDmgNull() - 512;
    } else if ((unsigned int)(selection - 513) < 16) {
        kind = 1;
        part = selection - 512;
    } else {
        kind = 2;
        if (selection != 32)
            kind = selection == 33 ? 3 : 0;
    }
    memset(camera, 0, 112);
    camera->kind = kind;
    switch (kind) {
    case 0: {
        float x = position[0] * 0.01f;
        float y = position[1] * 0.01f;
        float z = position[2] * 0.01f;
        camera->position.z = z;
        camera->position.x = x;
        camera->position.y = y;
        if (selection == 5) {
            sefGetPosition(&origin, 0, 5);
            __asm__ __volatile__(
                "lqc2 vf1, 0(%0)\n\t"
                "lqc2 vf2, 0(%1)\n\t"
                "vadd.xyz vf1, vf1, vf2\n\t"
                "sqc2 vf1, 0(%0)"
                : : "r"(&camera->position), "r"(&origin) : "memory");
        }
        return;
    }
    case 1:
        camera->actor = source ? battle->source_actor : battle->target_actor;
        {
            short number = source ? battle->source_number : battle->target_number;
            camera->actor_number = number;
            camera->part = part;
        }
        return;
    case 2:
    case 3: {
        int actor = battle->source_actor;
        short target_number = battle->target_number;
        camera->source_actor = actor;
        {
            short source_number = battle->source_number;
            int target_actor = battle->target_actor;
            camera->source_part = 15;
            camera->target_actor = target_actor;
            camera->source_number = source_number;
            camera->target_number = target_number;
            camera->target_part = 15;
        }
        break;
    }
    }
}

static void sdvSetCameraOffset(SdvCamOffset *cam, int kind,
                               const int16_t *vec)
{
    int cam_kind = kind;
    const int16_t *cam_vec = vec;

    memset(cam, 0, sizeof(*cam));
    cam->kind = cam_kind;

    if (cam_kind == 1) {
        cam->pos[0] = (float)cam_vec[0] * 0.01f;
        cam->pos[1] = (float)cam_vec[1] * 0.01f;
        cam->pos[2] = (float)cam_vec[2] * 0.01f;
    } else if (cam_kind == 2) {
        cam->ang[0] = (float)cam_vec[0];
        cam->ang[1] = (float)cam_vec[1];
        cam->ang[2] = (float)cam_vec[2];
    }
}

int sdvProgressPrm(int kind, void *data, int size)
{
    typedef struct SdvCameraParameters {
        float before_offset[28];
        SdvCamOffset offset;
        float after_offset[4];
    } SdvCameraParameters;

    typedef struct SdvCameraInterpolation {
        SdvCameraParameters params;
        int source_byte_3;
        unsigned int source_byte_2;
        int frame_delta;
        short transition_type;
        short unmodeled_be;
    } SdvCameraInterpolation;

    SdvCameraPhase *phase = data;
    const short *keyframes = phase->active;
    const short *current;
    const short *next;
    SdvCameraParameters params;
    SdvCameraInterpolation transition;
    int frame;
    int time_delta;
    int now_effect_no;
    int has_camera_params;
    int in_reset_window;

    frame = sdvProgressKey(keyframes, &phase->cursor, size);
    if ((unsigned int)frame < 0x400U) {
        current = keyframes + frame * size;
        next = current + size;
        time_delta = next[0] - current[0];
        now_effect_no = _nowEftNo;
        in_reset_window =
            (unsigned int)now_effect_no - 2000U < 100U;
        if (now_effect_no == 0x9A1 || now_effect_no == 0x99D) {
            in_reset_window = 1;
        }

        if (kind == 0x41) {
            if (current[1] >= 0) {
                func_A31CA0(current[1], current[2], current[3], current[4],
                            current[5]);
            }
            return frame;
        }

        if (time_delta <= 0 || frame == 0 || current[0] == 0x1000 ||
            next[0] == 0x400) {
            has_camera_params = 0;
            if (func_A31C30(kind) != 0) {
                func_A31BB8(kind);
            }
            memset(&params, 0, 0xB0);
            if ((unsigned int)kind < 2U) {
                has_camera_params = 1;
                sdvSetCameraParam(params.before_offset, current[7], current + 4);
                sdvSetCameraOffset(&params.offset, current[11], current + 8);
                params.before_offset[27] = 0.5f;
            } else if (kind == 3) {
                params.after_offset[1] = (float)current[2];
                params.after_offset[0] = (float)current[3] * 0.017453292f;
                func_A31920(2, params.before_offset);
            }
            func_A31920(kind, params.before_offset);
            if (in_reset_window && has_camera_params) {
                memset(&params, 0, 0xB0);
                func_A31500(&params.before_offset[4], kind);
                params.before_offset[0] = 0.0f;
                params.offset.kind = 0;
                func_A31920(kind, params.before_offset);
            }
        }

        if (time_delta > 0 && next[0] < 0x400) {
            memset(&transition, 0, 0xC0);
            transition.source_byte_3 = (signed char)((unsigned short)current[1] >> 8);
            transition.source_byte_2 = (unsigned short)current[1] & 0xFFU;
            transition.frame_delta = time_delta;
            if ((unsigned int)kind < 2U) {
                sdvSetCameraParam(transition.params.before_offset, next[7], next + 4);
                sdvSetCameraOffset(&transition.params.offset, next[11], next + 8);
                transition.params.before_offset[27] = 0.5f;
                transition.transition_type = 1;
            } else if (kind == 3) {
                if (next[3] != current[3]) {
                    transition.params.after_offset[0] =
                        (float)next[3] * 0.017453292f;
                    func_A31AE0(2, &transition);
                }
                if (next[2] == current[2]) {
                    return frame;
                }
                transition.params.after_offset[1] = (float)next[2];
            }
            func_A31AE0(kind, &transition);
            if (next[size] == 0x400) {
                phase->cursor.position++;
            }
        }
    }

    return frame;
}

void sdvScheduleCamera(SdvCameraTask *task)
{
    float params[44];

    if (_sefBattleMode != 0 && task != 0 && task->active != 0) {
        if (task->pos.active != 0) {
            sdvProgressPrm(0, &task->pos, 0xC);
        }
        if (task->angle.active != 0) {
            sdvProgressPrm(1, &task->angle, 0xC);
        }
        if (task->scale.active != 0) {
            sdvProgressPrm(3, &task->scale, 4);
        } else {
            memset(params, 0, sizeof(params));
            func_A31920(2, params);
            params[41] = 40.0f;
            func_A31920(3, params);
        }
        if (task->offset.active != 0) {
            sdvProgressPrm(0x41, &task->offset, 6);
        }
    }
}

extern float D_004D836C;
extern void *sefGetNowScheduler(void);
extern void MMathMulMatrix(void *, void *, void *);
extern void MMathApplyMatrix(void *, void *, void *);
extern void *xglStudioGetActiveCamera(void);
extern int MMathRotTransPersClip(void *, void *, void *, void *);

static int sdvTransOffset(int *out, void *src, void *mtx, unsigned short *arg3)
{
    float pos[4];
    float tmp[4];
    float m[16];
    float m2[16];
    short type;
    char *sched;
    char *cam;

    type = *(short *)(*(int *)((char *)mtx + 0x54) + 2);
    if (type == 3) {
        __asm__ __volatile__("lq $8, 0(%1)\nsq $8, 0(%0)\n" : : "r"(out), "r"(src) : "$8", "memory");
        out[0] += 0x100;
        out[1] += 0xE0;
        out[2] = arg3[1] << 16;
        return 1;
    }
    sched = sefGetNowScheduler();
    __asm__ __volatile__("lqc2 vf1, 0(%0)\n\tvitof0.xyzw vf1, vf1\n\tsqc2 vf1, 0(%1)" : : "r"(src), "r"(tmp) : "memory");
    tmp[2] *= D_004D836C;
    tmp[3] = 1.0f;
    sefGetPosition(pos, tmp, type);
    if (*(int *)(sched + 0xA8C) & 2) {
        MMathMulMatrix(m, mtx, sched + 0x40);
        MMathApplyMatrix(pos, m, pos);
    }
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n\t"
        "vmove.w vf1, vf0\n\t"
        "vmr32.xyzw vf2, vf0\n\t"
        "vmr32.xyzw vf3, vf2\n\t"
        "vmr32.xyzw vf4, vf3\n\t"
        "sqc2 vf1, 0x30(%1)\n\t"
        "sqc2 vf2, 0x20(%1)\n\t"
        "sqc2 vf3, 0x10(%1)\n\t"
        "sqc2 vf4, 0(%1)\n\tnop" : : "r"(pos), "r"(m) : "memory");
    cam = xglStudioGetActiveCamera();
    MMathMulMatrix(m2, cam + 0x470, m);
    __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(pos) : "memory");
    if (MMathRotTransPersClip(out, cam, m2, pos) != 0) {
        return 0;
    }
    out[0] = (out[0] >> 4) - 0x700;
    out[1] = (out[1] >> 4) - 0x720;
    if (arg3[1] != 0) {
        out[2] += arg3[1];
        if (out[2] > 0xF000) {
            out[2] = 0xF000;
        }
    }
    return 1;
}

void sdvInitSpecialWork(void)
{
    memset(_sdvSpecialBuf, 0, sizeof(_sdvSpecialBuf));
}

void sdvClearSpecialWork(void)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (_sdvSpecialBuf[i] != 0) {
            GameDefocusSet(i, 0, 0);
            _sdvSpecialBuf[i] = 0;
        }
    }
}

static int sdvAllocSpecialWork(int kind)
{
    int slot;

    if (kind == 0) {
        if (_sdvSpecialBuf[0] == 0) {
            _sdvSpecialBuf[0] = 1;
            return 0;
        }
        return -1;
    }

    for (slot = 1; slot < 8; slot++) {
        if (_sdvSpecialBuf[slot] == 0) {
            _sdvSpecialBuf[slot] = 1;
            return slot;
        }
    }
    return -1;
}

extern const short blksiz_2[9];
typedef struct SdvScreenPoint {
    int x;
    int y;
    int z;
    int w;
} SdvScreenPoint;
typedef struct SdvSpecialKey {
    unsigned short time;            /* +0x00, 1024 ends the table */
    unsigned short depth_step;      /* +0x02 */
    unsigned char color[4];         /* +0x04 */
    short x;                        /* +0x08 */
    short y;                        /* +0x0a */
    unsigned short width;           /* +0x0c */
    unsigned short height;          /* +0x0e */
    unsigned short intensity;       /* +0x10 */
    short z;                        /* +0x12 */
} SdvSpecialKey;
typedef struct SdvSpecialTable {
    short kind;                     /* +0x00 */
    short part;                     /* +0x02, the actor part an offset follows */
    SdvSpecialKey keys[1];          /* +0x04 */
} SdvSpecialTable;
typedef struct SdvSpecialSlot {
    Matrix4 matrix;                 /* +0x00 */
    SdvSpecialKey key;              /* +0x40 */
    SdvSpecialTable *table;         /* +0x54 */
    int frame;                      /* +0x58 */
    int index;                      /* +0x5c */
} SdvSpecialSlot;
typedef union SdvColor {
    struct {
        unsigned int red : 8;
        unsigned int green : 8;
        unsigned int blue : 8;
        unsigned int alpha : 8;
    } channel;
    int word;
} SdvColor;
typedef struct SdvSpecial {
    SdvSequence sequence;
    unsigned char unmodeled_18[8];
    SdvSpecialSlot slots[7];
} SdvSpecial;

typedef struct SdvScheduler {
    Matrix4 base_matrix;
    Matrix4 matrix;
    unsigned char unmodeled_80[0xa8c - 0x80];
    int flags;
} SdvScheduler;
#define SDV_BLEND(dest, field) \
    (dest) = (key1->field == key0->field ? key0->field : (key0->field + (((key1->field - key0->field) * blend + 128) >> 8)) & 0xffff)
typedef union SdvKeyBytes {
    SdvSpecialKey key;
    char byte[sizeof(SdvSpecialKey)];
} SdvKeyBytes;
static inline void sdvCopyKey(SdvSpecialKey *destination, const SdvSpecialKey *source)
{
    ((SdvKeyBytes *)destination)->key = ((const SdvKeyBytes *)source)->key;
}

void sdvExecSpecial(SdvSpecial *special)
{
    int index;
    int n;
    SdvScheduler *scheduler;
    SdvSpecialSlot *slot;

    if (special == 0) {
        return;
    }
    if (special->sequence.event_address == 0) {
        return;
    }
    index = sdvExecSeqTbl(&special->sequence);
    if ((unsigned int)index < 1024) {
        scheduler = sefGetNowScheduler();
        for (n = 0; n < 7; n++) {
            slot = &special->slots[n];
            if (slot->table == 0) {
                slot->table = (SdvSpecialTable *)scGetImmAdrImmIdx2(
                    *(int *)special->sequence.unmodeled_04,
                    (unsigned short *)(special->sequence.event_address +
                                       index * special->sequence.stride * sizeof(short)),
                    1);
                slot->frame = 0;
                slot->index = 0;
                __asm__ __volatile__(
                    "lq $8, 0(%1)\n\t"
                    "lq $9, 16(%1)\n\t"
                    "lq $10, 32(%1)\n\t"
                    "lq $11, 48(%1)\n\t"
                    "sq $8, 0(%0)\n\t"
                    "sq $9, 16(%0)\n\t"
                    "sq $10, 32(%0)\n\t"
                    "sq $11, 48(%0)"
                    :
                    : "r"(slot->matrix), "r"(scheduler->base_matrix)
                    : "$8", "$9", "$10", "$11", "memory");
                break;
            }
        }
    }

    for (n = 0; n < special->sequence.position && n < 7; n++) {
        const SdvSpecialKey *keys;
        const SdvSpecialKey *key0;
        const SdvSpecialKey *key1;
        int blend;

        slot = &special->slots[n];
        if (slot->table == 0) {
            continue;
        }
        keys = slot->table->keys;
        key0 = &keys[slot->index];
        key1 = key0 + 1;
        if (key0->time != 1024) {
            if (key0->time < slot->frame && key1->time > 1024) {
                slot->table = 0;
            } else if (key1->time >= 1024) {
                sdvCopyKey(&slot->key, key0);
            } else if (key0->time != key1->time) {
                blend = ((slot->frame - key0->time) << 8) / (key1->time - key0->time);
                SDV_BLEND(slot->key.depth_step, depth_step);
                SDV_BLEND(slot->key.color[0], color[0]);
                SDV_BLEND(slot->key.color[1], color[1]);
                SDV_BLEND(slot->key.color[2], color[2]);
                SDV_BLEND(slot->key.color[3], color[3]);
                SDV_BLEND(slot->key.x, x);
                SDV_BLEND(slot->key.y, y);
                SDV_BLEND(slot->key.width, width);
                SDV_BLEND(slot->key.height, height);
                SDV_BLEND(slot->key.intensity, intensity);
                slot->key.z = key1->z == key0->z
                    ? key0->z
                    : (key0->z + (((key1->intensity - key0->z) * blend + 128) >> 8)) & 0xffff;
            }
        }
        if (slot->frame < 1024) {
            slot->frame++;
            if (slot->frame >= key1->time) {
                slot->index++;
            }
        }
    }
}

void sdvDrawSpecial(SdvSpecial *special)
{
    int params[16];
    SdvScreenPoint offset;
    SdvScreenPoint screen;
    SdvColor color;
    int n;

    for (n = 0; n < special->sequence.position && n < 7; n++) {
        SdvSpecialSlot *slot = &special->slots[n];
        SdvSpecialKey *key;
        int work;
        int size;
        unsigned int i;

        if (slot->table == 0) {
            continue;
        }
        key = &slot->key;
        params[0] = 0;
        switch (slot->table->kind) {
        case 7:
            if (key->color[3] == 0) {
                break;
            }
            offset.x = key->x;
            offset.y = key->y;
            offset.z = key->z != -1 ? key->z : 0;
            if (sdvTransOffset(&screen, &offset, slot, key) == 0) {
                break;
            }
            params[1] = 1;
            params[0] = 0;
            if (sdvAllocSpecialWork(0) >= 0) {
                GameDefocusSet(0, 2, (int)params);
            }
            {
                unsigned char t1 = (key)->color[1];
                (color).channel.red = (key)->color[0];
                (color).channel.green = t1;
                (color).channel.blue = (key)->color[2];
                (color).channel.alpha = (key)->color[3];
            };
            params[0] = screen.z;
            params[2] = screen.x;
            params[1] = color.word;
            params[3] = screen.y;
            params[4] = key->width;
            params[5] = key->height;
            params[6] = key->intensity;
            work = sdvAllocSpecialWork(1);
            if (work > 0) {
                GameDefocusSet(work, 7, (int)params);
            }
            break;
        case 6:
            if (key->color[3] == 0) {
                break;
            }
            params[1] = 1;
            params[0] = 0;
            if (sdvAllocSpecialWork(0) >= 0) {
                GameDefocusSet(0, 2, (int)params);
            }
            i = key->depth_step != 0 ? key->depth_step : 0xfffff8;
            {
                unsigned char t1 = (key)->color[1];
                (color).channel.red = (key)->color[0];
                (color).channel.green = t1;
                (color).channel.blue = (key)->color[2];
                (color).channel.alpha = (key)->color[3];
            };
            params[0] = i;
            params[2] = key->x;
            params[3] = key->y;
            params[4] = key->width;
            params[5] = key->height;
            params[1] = color.word;
            work = sdvAllocSpecialWork(1);
            if (work > 0) {
                GameDefocusSet(work, 6, (int)params);
            }
            break;
        case 5:
            if (key->color[3] == 0) {
                break;
            }
            {
                unsigned char t1 = (key)->color[1];
                (color).channel.red = (key)->color[0];
                (color).channel.green = t1;
                (color).channel.blue = (key)->color[2];
                (color).channel.alpha = (key)->color[3];
            };
            size = 1;
            params[0] = key->depth_step;
            params[1] = color.word;
            params[2] = key->x;
            params[3] = key->y;
            params[4] = key->width;
            params[5] = key->height;
            for (i = 0; i < 9; i++) {
                if (blksiz_2[i] >= key->intensity) {
                    size = blksiz_2[i];
                    break;
                }
            }
            params[6] = size;
            params[7] = size;
            work = sdvAllocSpecialWork(1);
            if (work > 0) {
                GameDefocusSet(work, 5, (int)params);
            }
            break;
        case 3:
            if (key->color[3] == 0) {
                break;
            }
            GameDefocusSet(0, 2, (int)params);
            params[0] = key->intensity;
            {
                unsigned char t1 = (key)->color[1];
                (color).channel.red = (key)->color[0];
                (color).channel.green = t1;
                (color).channel.blue = (key)->color[2];
                (color).channel.alpha = (key)->color[3];
            };
            params[2] = key->x;
            params[1] = color.word;
            params[3] = key->y;
            params[4] = key->width;
            params[5] = key->height;
            work = sdvAllocSpecialWork(1);
            if (work > 0) {
                GameDefocusSet(work, 3, (int)params);
            }
            break;
        case 1:
            if (key->color[3] == 0) {
                break;
            }
            {
                unsigned char t1 = (key)->color[1];
                (color).channel.red = (key)->color[0];
                (color).channel.green = t1;
                (color).channel.blue = (key)->color[2];
                (color).channel.alpha = (key)->color[3];
            };
            params[1] = key->intensity;
            params[2] = key->depth_step;
            params[4] = key->x;
            params[5] = key->y;
            params[6] = key->width;
            params[7] = key->height;
            params[0] = 0;
            params[3] = color.word;
            work = sdvAllocSpecialWork(1);
            if (work > 0) {
                GameDefocusSet(work, 1, (int)params);
            }
            break;
        case 4:
            if (key->color[3] == 0) {
                break;
            }
            {
                unsigned char t1 = (key)->color[1];
                (color).channel.red = (key)->color[0];
                (color).channel.green = t1;
                (color).channel.blue = (key)->color[2];
                (color).channel.alpha = (key)->color[3];
            };
            params[0] = 1;
            params[3] = color.word;
            params[4] = 0;
            params[5] = 0;
            params[6] = 0;
            params[7] = 0;
            params[1] = 1;
            params[2] = key->depth_step;
            work = sdvAllocSpecialWork(1);
            if (work > 0) {
                GameDefocusSet(work, 1, (int)params);
            }
            break;
        case 9:
            work = sdvAllocSpecialWork(1);
            if (work > 0) {
                GameDefocusSet(work, 9, (int)params);
            }
            break;
        }
    }
}

#define SCE_GS_SET_TEX0(tbp, tbw, psm, tw, th, tcc, tfx, cbp, cpsm, csm, csa, cld) \
    ((unsigned long long)(tbp) | ((unsigned long long)(tbw) << 14) | \
     ((unsigned long long)(psm) << 20) | ((unsigned long long)(tw) << 26) | \
     ((unsigned long long)(th) << 30) | ((unsigned long long)(tcc) << 34) | \
     ((unsigned long long)(tfx) << 35) | ((unsigned long long)(cbp) << 37) | \
     ((unsigned long long)(cpsm) << 51) | ((unsigned long long)(csm) << 55) | \
     ((unsigned long long)(csa) << 56) | ((unsigned long long)(cld) << 61))
typedef struct {
    void *source_actor;
    void *target_actor;
    void *weapon;
    unsigned char unmodeled_0c[8];
    short effect_no;
    unsigned char unmodeled_16[14];
} SdvAlterReq;
typedef union {
    unsigned int word[4];
    unsigned long long dword[2];
} SdvAlterQuad;
typedef struct {
    SdvAlterReq request;
    unsigned char unmodeled_24[12];
    SdvAlterQuad rgba;
    unsigned long long tex0[2];
    unsigned char unmodeled_50[4];
    float length;
    short selector[2];
    short life;
    short max_samples;
    unsigned short shrink;
    short blend;
    short image_count;
    short mode;
    unsigned short image_index;
    short image_frames;
    unsigned short image_timer;
    short category;
} SdvAlterParam;
typedef struct {
    unsigned char pad[0x12];
    unsigned short h12;
    unsigned short h14;
    unsigned short h16;
    unsigned char pad18[4];
    unsigned short h1C;
    unsigned short h1E;
} SdvImageItem;
extern void *sefSearchMapperIndex(int);
extern SdvImageItem *svGetImageListItem(void *, int);
extern int sdvCreateAlter();

void sdvScheduleAlter(SdvSequence *sequence, SdvAlterReq *request)
{
    int psm;
    unsigned long long *slot;
    unsigned long long fmt, clut, base, dim;
    SdvAlterParam param;
    unsigned char chr;
    int cate;
    unsigned int event_index;
    short *ev;
    short *p;
    void *map;
    int i;
    short a, b;
    SdvImageItem *i1, *i2;

    if (sequence == 0) {
        return;
    }
    if (sequence->event_address == 0) {
        return;
    }
    event_index = sdvExecSeqTbl(sequence);
    if (event_index >= 1024) {
        return;
    }
    ev = (short *)(sequence->event_address + event_index * sequence->stride * sizeof(short));
    memset(&param, 0, sizeof(param));
    param.request = *request;
    param.life = ev[1];
    param.selector[0] = ev[8];
    param.selector[1] = ev[9];
    param.max_samples = ev[10] > 128 ? 128 : ev[10];
    param.shrink = ev[11];
    param.mode = ev[2];
    param.blend = ev[3];
    param.rgba.word[0] = ev[4];
    param.rgba.word[1] = ev[5];
    param.rgba.word[2] = ev[6];
    param.rgba.word[3] = ev[7];
    param.image_frames = ev[12];
    param.length = ev[13];
    param.category = 14;
    srsAnalyzeEftNo(request->effect_no, &chr, &cate);
    if (cate == 0) {
        cate = 14;
    }
    map = sefSearchMapperIndex(request->effect_no);
    for (i = 0; i < 2; i++) {
        p = &ev[i * 2 + 15];
        a = p[-1];
        b = p[0];
        if (a < 0 || b < 0) {
            continue;
        }
        i1 = svGetImageListItem(map, a);
        i2 = svGetImageListItem(map, b);
        if (i1 != 0 && i2 != 0) {
            psm = i1->h14 == 8 ? 19 : 20;
            base = (unsigned long long)i1->h12 | 0x2000000000000000ULL;
            dim = ((unsigned long long)i1->h1C << 26) | 0x400000000ULL;
            fmt = ((unsigned long long)psm << 20) | ((unsigned long long)(i1->h16 >> 6) << 14);
            clut = ((unsigned long long)i1->h1E << 30) | ((unsigned long long)i2->h12 << 37);
            slot = (unsigned long long *)((char *)(param.image_count * 8) + (int)param.tex0);
            param.image_count++;
            param.category = cate;
            *slot = (base | fmt) | (dim | clut);
        }
    }
    sdvCreateAlter(&param);
}

void sdvInitAlter(SdvAlter *alter)
{
    sefMemZero(alter, sizeof(*alter));
}

int sdvCreateAlter(SdvAlterParameters *parameters)
{
    int index;
    int unavailable = -1;

    index = 0;
    do {
        if (_sdvAlter[index].active == 0) {
            int serial = (serial_3 + 1) & 255;
            serial_3 = serial;
            _sdvAlter[index].parameters = *parameters;
            _sdvAlter[index].serial = serial_3;
            _sdvAlter[index].active = 1;
            _sdvAlter[index].samples = 0;
            _sdvAlter[index].phase = 0.0f;
            _sdvAlter[index].cursor = 0;
            return (serial << 16) | index;
        }
        ++index;
    } while (index < 16);
    return unavailable;
}

void sdvDestroyAlter(SdvAlter *alter)
{
    sefMemZero(alter, sizeof(*alter));
}

void sdvKillAlter(int alter_id)
{
    int index;
    int serial;

    if (alter_id < 0)
        return;
    index = alter_id & 0xFFFF;
    serial = alter_id >> 16;
    if (_sdvAlter[index].active == 0)
        return;
    if (_sdvAlter[index].serial != serial)
        return;
    sdvDestroyAlter(&_sdvAlter[index]);
}

extern void sefGetNullPosition(Vector4 *position, void *actor, int part, int flag);
extern void sefGetWeaponPosition(Vector4 *position, void *weapon, int part);
typedef struct {
    SdvAlterParam p;
    Vector4 trail[2][128];
    float level[128];
    float phase;
    unsigned short cursor;
    unsigned short samples;
    unsigned short active;
    unsigned short wrapped;
    short serial;
} SdvAlterView;

void sdvExecAlter(SdvAlter *alter)
{
    SdvAlterView *a = (SdvAlterView *)alter;
    int i;

    a->p.life--;
    if (a->p.life > 0) {
        if ((short)a->cursor < 128) {
            if ((short)a->cursor < a->p.max_samples) {
                a->cursor++;
            }
        }
    } else {
        a->cursor -= a->p.shrink;
    }
    if ((short)a->cursor > 0) {
        for (i = 0; i < 2; i++) {
            int selector = a->p.selector[i];
            void *actor;
            Vector4 *slot = &a->trail[i][a->samples];

            __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(slot) : "memory");
            if (selector == 529)
                selector = sefGetDmgNull();
            if (selector == 256) {
                actor = a->p.request.source_actor;
                goto copy;
            } else if (selector == 512) {
                actor = a->p.request.target_actor;
            copy:
                if (actor != 0) {
                    __asm__ __volatile__(
                        "lq $8, 0(%1)\n\t"
                        "sq $8, 0(%0)"
                        :
                        : "r"(slot), "r"((char *)actor + 0x10)
                        : "$8", "memory");
                }
            } else if (selector & 0x100) {
                sefGetNullPosition(slot, a->p.request.source_actor,
                                   selector - 257, 1);
            } else if (selector & 0x200) {
                sefGetNullPosition(slot, a->p.request.target_actor,
                                   selector - 513, 1);
            } else if (selector & 0x400) {
                sefGetWeaponPosition(slot, a->p.request.weapon,
                                     selector - 1024);
            } else {
                sefGetPosition(&a->trail[i][a->samples], 0,
                               a->p.selector[i]);
            }
        }
        if (a->p.mode >= 3) {
            float phase = a->phase;

            a->phase = phase + a->p.length / (float)a->p.max_samples;
            if (a->p.mode == 3) {
                if (a->wrapped == 0)
                    a->level[a->samples] = phase;
                else
                    a->level[a->samples] = 4.0f;
            }
            if (a->p.mode == 4) {
                a->level[a->samples] = phase;
                if (a->phase > 20.0f)
                    a->phase -= 20.0f;
            }
        }
        a->samples++;
        if (a->samples >= 128) {
            a->wrapped = 1;
            a->samples &= 0x7f;
        }
    } else if (a->p.life < 0) {
        sdvDestroyAlter(alter);
    }
}

typedef unsigned int Quadword __attribute__((mode(TI)));
typedef union SdvQuadColor {
    Quadword quad;
    unsigned int word[4];
} SdvQuadColor;
typedef union SdvScreenQuad {
    SdvScreenPoint point;
    Quadword quad;
} SdvScreenQuad;
typedef union SdvDrawQuad {
    Quadword quad;
    unsigned int word[4];
    unsigned long long dword[2];
} SdvDrawQuad;
typedef struct SdvAlterDrawParameters {
    SdvAlterReq request;
    unsigned char unmodeled_24[12];
    SdvDrawQuad rgba;               /* +0x30 */
    unsigned long long tex0[2];     /* +0x40 */
    unsigned char unmodeled_50[4];
    float length;                   /* +0x54 */
    short selector[2];              /* +0x58 */
    short life;                     /* +0x5c */
    short max_samples;              /* +0x5e */
    unsigned short shrink;          /* +0x60 */
    short blend;                    /* +0x62 */
    short image_count;              /* +0x64 */
    short mode;                     /* +0x66 */
    unsigned short image_index;     /* +0x68 */
    short image_frames;             /* +0x6a */
    unsigned short image_timer;     /* +0x6c */
    short category;                 /* +0x6e */
} SdvAlterDrawParameters;
typedef struct SdvAlterDraw {
    SdvAlterDrawParameters parameters;
    Vector4 trail[2][128];          /* +0x70 */
    float level[128];               /* +0x1070 */
    float phase;
    unsigned short cursor;
    unsigned short samples;
    unsigned short active;
    unsigned short wrapped;
    short serial;
    unsigned char unmodeled_127e[2];
} SdvAlterDraw;
typedef struct SdvGifPacket {
    int count;
    int tag_index;
    int loop_count;
    void *base;
} SdvGifPacket;
extern const unsigned long long alphaTbl_004CC8D8[4];
extern void SGsInitGifPacket(SdvGifPacket *packet);
extern void SGsOpenGifPacket(SdvGifPacket *packet, int prim,
                             unsigned long long registers, unsigned int nreg);
extern void SGsCloseGifPacket(SdvGifPacket *packet);
extern void SGsAddReg(SdvGifPacket *packet, unsigned long long address,
                      unsigned long long value);
extern void SGsAddGifSTQ(SdvGifPacket *packet, float texture_s, float texture_t,
                         float texture_q);
extern void SGsAddGifRGBA(SdvGifPacket *packet, Quadword rgba);
extern void SGsAddGifData(SdvGifPacket *packet, Quadword data);
extern void SGsSendPacket(SdvGifPacket *packet);
extern void MMathCalcHermitePrm(HermiteVector *tangent_start, HermiteVector *tangent_end,
                                const HermiteVector *point_prev, const HermiteVector *point_start,
                                const HermiteVector *point_end, const HermiteVector *point_next);
float *MMathCalcHermite(float *destination, float parameter,
                         HermiteVector *tangent_first, HermiteVector *tangent_second,
                         HermiteVector *endpoint_first, HermiteVector *endpoint_second);

void sdvDrawAlter(SdvAlter *alter_in, void *packet_in)
{
    SdvAlterDraw *alter = (SdvAlterDraw *)alter_in;
    SdvGifPacket *packet = (SdvGifPacket *)packet_in;
    HermiteVector tangent[2];
    HermiteVector points[2][8];
    SdvScreenQuad screen;
    SdvQuadColor color;
    Matrix4 view;
    int i;
    int j;
    int k;
    int head;
    StudioCamera *camera;
    int count;
    int counter; /* Hermite sample position, then the strand fade countdown */
    int previous;
    int current;
    int next;
    int after;
    int previous_offset;
    int after_offset;
    float alpha;
    float alpha_step = 0.0f;
    float s_step = 0.0f;
    float textureS;

    count = (short)alter->cursor;
    head = alter->samples - 1;
    camera = xglStudioGetActiveCamera();
    textureS = alter->phase;
    SGsInitGifPacket(packet);
    if (alter->parameters.mode == 2) {
        alter->phase -= alter->parameters.length /
                        ((float)alter->parameters.max_samples * 2.0f);
        if (alter->phase < -20.0f) {
            alter->phase += 20.0f;
        }
    }
    color.quad = alter->parameters.rgba.quad;
    alpha = (float)alter->parameters.rgba.word[3];
    if (count >= 2) {
        /* s_step holds the per-segment alpha quotient before its own value. */
        s_step = alpha / (count - 1);
        alpha_step = s_step * 0.25f;
        s_step = alter->parameters.length / (count - 1);
        s_step *= 0.25f;
    }
    if (alter->parameters.image_count != 0) {
        SGsOpenGifPacket(packet, 92, 3822, 3);
        alter->parameters.image_timer++;
        if ((short)alter->parameters.image_timer >= alter->parameters.image_frames) {
            alter->parameters.image_timer = 0;
            alter->parameters.image_index++;
            if ((short)alter->parameters.image_index >= alter->parameters.image_count) {
                alter->parameters.image_index = 0;
            }
        }
        SGsAddReg(packet, 6,
                  alter->parameters.tex0[(short)alter->parameters.image_index]);
        if (alter->parameters.mode == 3) {
            SGsAddReg(packet, 8, 5);
        } else {
            SGsAddReg(packet, 8, 4);
        }
    } else {
        SGsOpenGifPacket(packet, 76, 14, 1);
    }
    SGsAddReg(packet, 66, alphaTbl_004CC8D8[alter->parameters.blend]);
    packet->loop_count = 1;
    SGsCloseGifPacket(packet);
    __asm__ __volatile__(
        "lq $8, 0(%1)\n\t"
        "lq $9, 16(%1)\n\t"
        "lq $10, 32(%1)\n\t"
        "lq $11, 48(%1)\n\t"
        "sq $8, 0(%0)\n\t"
        "sq $9, 16(%0)\n\t"
        "sq $10, 32(%0)\n\t"
        "sq $11, 48(%0)"
        :
        : "r"(view), "r"(camera->viewMatrix)
        : "$8", "$9", "$10", "$11", "memory");
    for (i = 0; i < count - 1; i++) {
        current = (head - i) & 0x7f;
        if (i == 0)
            previous = current;
        else
            previous = (current + 1) & 0x7f;
        if (i >= count - 2) {
            next = (current - 1) & 0x7f;
            after = next;
        } else {
            next = (current - 1) & 0x7f;
            after = (current - 2) & 0x7f;
        }
        previous_offset = previous * sizeof(HermiteVector);
        after_offset = after * sizeof(HermiteVector);
        for (j = 0; j < 2; j++) {
            HermiteVector *trail = (HermiteVector *)alter->trail[j];
            HermiteVector *out;

            MMathCalcHermitePrm(&tangent[0], &tangent[1],
                                (HermiteVector *)((char *)trail + previous_offset), &trail[current],
                                &trail[next], (HermiteVector *)((char *)trail + after_offset));
            /* The row address is built on integers, as in the original. */
            out = (HermiteVector *)(j * sizeof(points[0]) + (unsigned int)points);
            for (counter = 0; counter <= 100; counter += 25) {
                MMathCalcHermite((float *)out, (float)counter * 0.01f, &tangent[0],
                                 &tangent[1], &trail[current], &trail[next]);
                out++;
            }
        }
        if (alter->parameters.image_count != 0) {
            SGsOpenGifPacket(packet, 92, 1298, 3);
        } else {
            SGsOpenGifPacket(packet, 76, 1298, 3);
        }
        if (alter->parameters.mode >= 3) {
            textureS = alter->level[current];
            s_step = (alter->level[next] - textureS) * 0.25f;
        }
        counter = 0;
        for (k = 0; k < 5; k++) {
            color.word[3] = (unsigned int)alpha;
            for (j = 0; j < 2; j++) {
                MMathRotTransPersClip(&screen.point, camera, view, &points[j][k]);
                if (screen.point.w != 0) {
                    counter = 2;
                } else {
                    counter--;
                    if (counter >= 0) {
                        screen.point.w = 0x8000;
                    }
                }
                if (j == 0) {
                    SGsAddGifSTQ(packet, textureS, 0.0f, 1.0f);
                } else {
                    SGsAddGifSTQ(packet, textureS, 1.0f, 1.0f);
                }
                SGsAddGifRGBA(packet, color.quad);
                SGsAddGifData(packet, screen.quad);
            }
            if (k < 4) {
                textureS += s_step;
                alpha -= alpha_step;
            }
        }
        packet->loop_count = 10;
        SGsCloseGifPacket(packet);
        if ((i & 7) == 7) {
            SGsSendPacket(packet);
        }
    }
    SGsSendPacket(packet);
}

void sdvInitAlters(void)
{
    int i;

    for (i = 0; i < 16; i++) {
        sdvInitAlter(&_sdvAlter[i]);
    }
}

void sdvDestroyAlters(void)
{
    int i;

    for (i = 0; i < 16; i++) {
        sdvDestroyAlter(&_sdvAlter[i]);
    }
}

void sdvExecAlters(void)
{
    int i;

    for (i = 0; i < 16; i++) {
        if (_sdvAlter[i].active != 0) {
            sdvExecAlter(&_sdvAlter[i]);
        }
    }
}

extern char _packet[];
void SGsInitEnv(void);
void sdvDrawAlter(SdvAlter *a, void *pkt);
typedef struct { char pad[0xE]; short kind; } SdvAlterSub;

void sdvDrawAlters(int arg0)
{
    int n;
    int i;

    n = 0;
    for (i = 0; i < 16; i++) {
        if (_sdvAlter[i].active != 0 && ((SdvAlterSub *)&_sdvAlter[i].parameters.unmodeled_00[12])->kind == arg0) {
            n++;
            if (n == 1) {
                SGsInitEnv();
            }
            sdvDrawAlter(&_sdvAlter[i], _packet);
        }
    }
}

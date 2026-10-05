#include "common.h"
#include "shared.h"

/* Only the command words touched before ACT_DrawShadowBegin sends the buffer. */
typedef struct {
    u8 unmodeled_00[0x50];
    u64 shadow_command_word_10;
    u8 unmodeled_58[0x38];
    u64 shadow_command_word_18;
    u8 unmodeled_98[8];
} ActShadowCommand;

/* The render-state halfword read by the shadow command builder is at +0x20. */
typedef struct {
    u8 unmodeled_00[0x20];
    u16 shadow_state;
} ActShadowRenderState;

/* Each source point contributes three words to the rear-shadow packet. */
typedef struct {
    u32 x;
    u32 y;
    u32 z;
} DropShadowSourcePoint;

typedef struct {
    DropShadowSourcePoint *points[4];
} DropShadowBackEntry;

typedef struct {
    u32 x;
    u32 y;
    u32 z;
    u32 w;
} DropShadowPacketPoint;

/* Only the packet words and four 16-byte output points written here are modeled. */
typedef struct {
    u8 unmodeled_00[0x0c];
    u32 packet_tag_0c;
    u8 unmodeled_10[4];
    u32 packet_header_14;
    u32 packet_header_18;
    u8 unmodeled_1c[4];
    u32 packet_header_20;
    u8 unmodeled_24[0x10];
    u32 packet_header_34;
    u8 unmodeled_38[8];
    u32 packet_state[3];
    u32 draw_mode;
    DropShadowPacketPoint points[4];
} DropShadowBackPacket;

/* Status bytes and 16-byte source records immediately precede the sent packet. */
typedef struct {
    u8 unmodeled_00[0x11a0];
    u8 back_shadow_status[16];
    DropShadowBackEntry back_shadow_entries[17];
    DropShadowBackPacket back_shadow_packet;
} ActDropShadowState;

extern ActShadowCommand Head_9;
extern ActShadowRenderState sRender;
extern void nmlModelDirectSend(int mode, u8 *data, int count);

typedef struct {
    unsigned char pad00[0x10];
    float x, y, z;
    unsigned char pad1c[0x80 - 0x1c];
    unsigned char enemy_index;
    unsigned char pad81[5];
    unsigned short kind;
    unsigned char pad88[0xb0 - 0x88];
    short step_frame;
    unsigned char padb2[0x6f0 - 0xb2];
    unsigned int flags;
    float frame;
    unsigned char pad6f8[0x704 - 0x6f8];
    unsigned short motion;
} FootstepActor;
typedef struct {
    unsigned char pad00[8];
    short mode;
    unsigned char pad0a[0x18 - 0xa];
    void *header;
    unsigned char pad1c[4];
    unsigned long long surface_flags;
    unsigned char pad28[0x40 - 0x28];
} FootstepUndu;
extern struct { int pad0; FootstepActor *player; int pad8[4]; } GameLoopState;
extern signed char enepc[][0x38B0];
extern unsigned char SaveData[];
extern unsigned char *D_00478CCC[];
extern float D_004D85C8;
extern int F2I(float);
extern void UnduParamInit(void *);
extern void *UnduDataGetHeader(int, int);
extern void UnduCheck(float *, void *, void *);
extern int RES_GetEnemySeBank(int);
extern void xglSoundEffectNormalID(int, int);

static void Footstep(FootstepActor *a)
{
    int old = a->step_frame;
    int now;
    unsigned char *tbl;
    int id;
    unsigned short m;
    int b, t;

    if ((unsigned int)(a->kind - 1) > 5) {
        return;
    }
    if (a != GameLoopState.player && enepc[a->enemy_index][0x37A2] == 0) {
        return;
    }
    m = a->motion;
    a->step_frame = 99;
    if (m != 1 && m != 3) {
        return;
    }
    if (a->flags & 2) {
        return;
    }
    id = 0;
    tbl = D_00478CCC[*(unsigned short *)(SaveData + 0x100A4)];
    if (m == 3) {
        tbl += 2;
    }
    now = F2I(a->frame / D_004D85C8);
    b = tbl[0];
    t = old < b;
    if ((t && now >= b) || (now < old && t)) {
        id = 0x20001;
        if (a->motion == 3) id = 0x20003;
    }
    b = tbl[1];
    t = old < b;
    if ((t && now >= b) || (now < old && t)) {
        id = 0x20002;
        if (a->motion == 3) id = 0x20004;
    }
    a->step_frame = now;
    if (id != 0) {
        if (a == GameLoopState.player) {
            FootstepUndu u;
            float pos[4];
            unsigned int s;
            UnduParamInit(&u);
            u.header = UnduDataGetHeader(0x303, 0x8000);
            u.mode = 0x810;
            pos[0] = a->x;
            pos[1] = a->y;
            pos[2] = a->z;
            UnduCheck(pos, 0, &u);
            s = (int)(u.surface_flags & 0xFF00) - 0x2300;
            if (s < 0x301) {
                id += s >> 6;
            }
        } else {
            int bank = RES_GetEnemySeBank((short)a->kind);
            if (bank != -1) {
                id = bank + (id & 0xFFFF);
            } else {
                id = 0;
            }
        }
        if (id != 0) {
            xglSoundEffectNormalID(id, 0);
        }
    }
}

INCLUDE_ASM("asm/main/nonmatchings/act_3", DrawCircleShadow);

INCLUDE_ASM("asm/main/nonmatchings/act_3", DrawZeldaShadowSub);

extern float D_004D85EC;
extern volatile float D_004D85F0;
extern float D_004D85F4;
extern StudioCamera *xglStudioGetCamera2(int camera_id);
extern float atan2f(float y, float x);
extern float sqrtf(float x);
void DrawZeldaShadowSub(void *a, void *b);

static void DrawZeldaShadow(char *obj) {
    int i;
    float *dir;

    float h = *(float *)(obj + 0x4DC);
    if (!(h != -1000.0f)) {
        *(float *)0x70000130 = *(float *)(obj + 0x14) + D_004D85EC;
    } else {
        *(float *)0x70000130 = h + D_004D85F0;
    }
    for (i = 0; i < 8; i++) {
        if (xglStudioGetCamera2(i)->active == 1) {
            break;
        }
    }
    *(int *)0x7000013C = i;
    __asm__ __volatile__(
        "lq $2, 0(%1)\n\tsq $2, 0(%0)\n\t"
        "lq $2, 16(%1)\n\tsq $2, 16(%0)\n\t"
        "lq $2, 32(%1)\n\tsq $2, 32(%0)\n\t"
        "lq $2, 48(%1)\n\tsq $2, 48(%0)"
        : : "r"((void *)0x70000010), "r"(*(void **)(obj + 0x824)) : "$2", "memory");
    dir = (float *)(obj + 0xA0);
    *(float *)0x70000138 = __builtin_sqrtf(1.0f - dir[1] * dir[1]) * 0.5f;
    if (*(float *)0x70000138 < D_004D85F4) {
        *(float *)0x70000138 = D_004D85F4;
    }
    *(float *)0x70000134 = atan2f(-dir[0], -dir[2]);
    DrawZeldaShadowSub((void *)0x70000000, *(char **)(obj + 0x824) + 0x780);
    DrawZeldaShadowSub((void *)0x70000000, *(char **)(obj + 0x824) + 0x500);
}

typedef struct {
    int x;
    int y;
    int z;
    int valid;
} DropShadowChkPoint;

static int DrawDropShadowSubChk(ActDropShadowState *shadow, u8 *index, int base,
                                int count)
{
    int i;
    int sum;
    u8 *status;
    DropShadowBackEntry *entry;
    DropShadowChkPoint *pts;
    DropShadowChkPoint *p0;
    DropShadowChkPoint *p1;
    DropShadowChkPoint *p2;
    DropShadowChkPoint *p3;

    status = shadow->back_shadow_status;
    entry = shadow->back_shadow_entries;
    i = 0;
    sum = 0;
    if (count > 0) {
        pts = (DropShadowChkPoint *)((char *)shadow + base * 16 + 0x9A0);
        do {
            p0 = &pts[index[0]];
            p1 = &pts[index[1]];
            p2 = &pts[index[2]];
            p3 = &pts[index[3]];
            index += 4;
            if (p0->valid == 0 || p1->valid == 0 || p2->valid == 0 || p3->valid == 0) {
                return 0;
            }
            entry->points[0] = (DropShadowSourcePoint *)p0;
            entry->points[1] = (DropShadowSourcePoint *)p1;
            entry->points[2] = (DropShadowSourcePoint *)p2;
            entry->points[3] = (DropShadowSourcePoint *)p3;
            entry++;
            if ((float)(p2->x * p0->y + p0->x * p1->y + p1->x * p2->y
                        - p2->x * p1->y - p0->x * p2->y - p1->x * p0->y) >= 0.0f) {
                *status = 1;
            } else {
                *status = 2;
            }
            sum += *status++;
            i++;
        } while (i < count);
    }
    return sum;
}

static void DrawDropShadowSubBack(ActDropShadowState *shadow, int vertex_count,
                                  int draw_mode)
{
    int remaining;
    u8 *status;
    DropShadowBackEntry *entry;
    int active_status;

    entry = shadow->back_shadow_entries;
    shadow->back_shadow_packet.packet_tag_0c = 0x51000008;
    shadow->back_shadow_packet.packet_header_14 = 0x70024000;
    shadow->back_shadow_packet.packet_header_18 = 0x055551ee;
    shadow->back_shadow_packet.packet_header_20 = 0x00071001;
    shadow->back_shadow_packet.packet_header_34 = 0x7fffffff;
    shadow->back_shadow_packet.draw_mode = draw_mode;
    shadow->back_shadow_packet.packet_state[0] = 0;
    shadow->back_shadow_packet.packet_state[1] = 0;
    shadow->back_shadow_packet.packet_state[2] = 0;
    shadow->back_shadow_packet.points[0].w = 0;
    shadow->back_shadow_packet.points[1].w = 0;
    shadow->back_shadow_packet.points[2].w = 0;
    shadow->back_shadow_packet.points[3].w = 0;

    if (vertex_count <= 0) {
        return;
    }

    remaining = vertex_count;
    active_status = 1;
    status = shadow->back_shadow_status;

    /* The original advances the status bytes and vertex records in lockstep. */
    do {
        if (*status == active_status) {
            shadow->back_shadow_packet.points[0].x = entry->points[0]->x;
            shadow->back_shadow_packet.points[0].y = entry->points[0]->y;
            shadow->back_shadow_packet.points[0].z = entry->points[0]->z;
            shadow->back_shadow_packet.points[1].x = entry->points[1]->x;
            shadow->back_shadow_packet.points[1].y = entry->points[1]->y;
            shadow->back_shadow_packet.points[1].z = entry->points[1]->z;
            shadow->back_shadow_packet.points[2].x = entry->points[2]->x;
            shadow->back_shadow_packet.points[2].y = entry->points[2]->y;
            shadow->back_shadow_packet.points[2].z = entry->points[2]->z;
            shadow->back_shadow_packet.points[3].x = entry->points[3]->x;
            shadow->back_shadow_packet.points[3].y = entry->points[3]->y;
            shadow->back_shadow_packet.points[3].z = entry->points[3]->z;
            nmlModelDirectSend(1, (u8 *)&shadow->back_shadow_packet, 9);
        }
        remaining--;
        status++;
        entry++;
    } while (remaining != 0);
}

extern float I2F(int value);
typedef struct {
    float u;
    float v;
    u32 unmodeled_08[2];
    u32 x;
    u32 y;
    u32 z;
    u32 w;
} DropShadowFrontPoint;

static void DrawDropShadowSubFront(ActDropShadowState *shadow, int vertex_count)
{
    int remaining;
    u8 *status;
    DropShadowBackEntry *entry;
    DropShadowFrontPoint *pt;
    float scale;

    pt = (DropShadowFrontPoint *)shadow->back_shadow_packet.points;
    shadow->back_shadow_packet.packet_tag_0c = 0x5100000C;
    entry = shadow->back_shadow_entries;
    shadow->back_shadow_packet.packet_header_14 = 0xB02A4000;
    shadow->back_shadow_packet.packet_header_18 = 0x252521EE;
    *(u32 *)shadow->back_shadow_packet.unmodeled_1c = 0x525;
    shadow->back_shadow_packet.packet_header_20 =
        (u64)((u8 *)shadow)[0x12BB] << 4 | 0x70009;
    shadow->back_shadow_packet.draw_mode = ((u8 *)shadow)[0x12BB];
    shadow->back_shadow_packet.packet_header_34 = 0;
    shadow->back_shadow_packet.packet_state[0] = 0;
    shadow->back_shadow_packet.packet_state[1] = 0;
    shadow->back_shadow_packet.packet_state[2] = 0;
    scale = 0.00012207031f;
    pt[0].w = 0;
    pt[1].w = 0;
    pt[2].w = 0;
    pt[3].w = 0;

    if (vertex_count <= 0) {
        return;
    }

    remaining = vertex_count;
    status = shadow->back_shadow_status;
    do {
        if (*status++ == 2) {
            pt[0].u = I2F(entry->points[0]->x - 0x6FF8) * scale;
            pt[0].v = I2F(entry->points[0]->y - 0x71F7) * scale;
            pt[0].x = entry->points[0]->x;
            pt[0].y = entry->points[0]->y;
            pt[0].z = entry->points[0]->z;
            pt[1].u = I2F(entry->points[1]->x - 0x6FF8) * scale;
            pt[1].v = I2F(entry->points[1]->y - 0x71F7) * scale;
            pt[1].x = entry->points[1]->x;
            pt[1].y = entry->points[1]->y;
            pt[1].z = entry->points[1]->z;
            pt[2].u = I2F(entry->points[2]->x - 0x6FF8) * scale;
            pt[2].v = I2F(entry->points[2]->y - 0x71F7) * scale;
            pt[2].x = entry->points[2]->x;
            pt[2].y = entry->points[2]->y;
            pt[2].z = entry->points[2]->z;
            pt[3].u = I2F(entry->points[3]->x - 0x6FF8) * scale;
            pt[3].v = I2F(entry->points[3]->y - 0x71F7) * scale;
            pt[3].x = entry->points[3]->x;
            pt[3].y = entry->points[3]->y;
            pt[3].z = entry->points[3]->z;
            nmlModelDirectSend(1, (u8 *)&shadow->back_shadow_packet, 13);
        }
        remaining--;
        entry++;
    } while (remaining != 0);
}

INCLUDE_ASM("asm/main/nonmatchings/act_3", DrawDropShadowSub);

INCLUDE_ASM("asm/main/nonmatchings/act_3", DrawDropShadow);

INCLUDE_ASM("asm/main/nonmatchings/act_3", DrawDropCircle);

void ACT_DrawShadowBegin(void)
{
    u16 shadow_state = sRender.shadow_state;

    Head_9.shadow_command_word_10 = shadow_state | 0xFFFFFF00080000ULL;
    Head_9.shadow_command_word_18 = shadow_state;
    /* Set the command-enable bit after initializing the state bits. */
    Head_9.shadow_command_word_18 |= 0x80000;
    nmlModelDirectSend(1, (u8 *)&Head_9, 10);
}

extern void nmlModelDirectSend(int mode, u8 *data, int count);
extern u8 Tail_10[];

void ACT_DrawShadowEnd(void)
{
    nmlModelDirectSend(1, Tail_10, 3);
}

INCLUDE_ASM("asm/main/nonmatchings/act_3", ACT_DrawShadow);

void ACT_DrawShadowInit(void)
{
}

#include "common.h"
#include "shared.h"

/* Types recovered upstream (origin/master); kept for reference. */
/* Partial model of the fixed packet template; unknown words stay explicit. */
typedef struct DefocusSolidPacketHeader {
    u8 unmodeled_00[0x0c];
    u32 packet_tag_0c;
    u32 vu_memory_address_10;
    u32 packet_header_14;
    u32 packet_header_18;
    u32 unmodeled_1c;
    u32 packet_header_20;
    u32 unmodeled_24;
    u32 packet_header_28;
    u32 unmodeled_2c;
    u32 packet_header_30;
    u32 unmodeled_34;
    u32 packet_header_38;
    u32 unmodeled_3c;
} DefocusSolidPacketHeader;


#define NULL ((void *)0)

typedef struct {
    u8 unmodeled_00[0x14];
    u16 tex0_state_14;
    u8 unmodeled_16[0x0a];
    u16 tex0_state_20;
    u16 tex0_state_22;
    u8 unmodeled_24[0x10];
    void (*callback)(XglPacket *packet);
} DefocusRenderState;

extern DefocusRenderState sRender;

/* Partial view of one GameDefocusParam entry: 16 entries, stride 0x44.
 * The first three bytes are used by the recovered state handlers and the
 * following sixteen words hold the settings copied by GameDefocusSet. */
typedef struct {
    u8 type;
    u8 state;
    u8 index;
    u8 unmodeled_03;
    int parameters[16];
} DefocusLayer;

extern DefocusLayer GameDefocusParam[16];
extern void DefocusMain(XglPacket *packet);
extern void DefocusMainType09Final(DefocusLayer *layer);

static u64 GetTex0(int type, int tex_cc)
{
    switch (type) {
    case 0:
        return ((u64)0x24020000 | (u64)(sRender.tex0_state_20 << 5) |
                ((u64)0x9000 << 18) | ((u64)(u32)tex_cc << 34));
    case 1:
        return ((u64)0x24020000 | (u64)(sRender.tex0_state_22 << 5) |
                ((u64)0x9000 << 18) | ((u64)(u32)tex_cc << 34));
    case 2:
        return ((u64)0x24020000 | (u64)(sRender.tex0_state_14 << 5) |
                ((u64)0x9000 << 18) | ((u64)(u32)tex_cc << 34));
    default:
        return 0;
    }
}

extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkAddDataN(XglPacket *packet, const void *data, int count);
extern u64 Head_0[12];
typedef struct {
    u32 header[3];
    u32 gif_tag;
    u32 vif_command;
    u32 gs_registers;
    u32 register_count;
    u32 header_padding;
    u32 red;
    u32 green;
    u32 blue;
    u32 alpha;
    u32 u0;
    u32 v0;
    u32 pad38[2];
    u32 left;
    u32 top;
    u32 z0;
    u32 flags0;
    u32 u1;
    u32 v1;
    u32 pad58[2];
    u32 right;
    u32 bottom;
    u32 z1;
    u32 flags1;
} DefocusType01Packet;

static void DefocusMainType01(DefocusLayer *layer, XglPacket *packet)
{
    int i;
    int z;
    u8 *color;
    DefocusType01Packet *draw;

    i = 0;
    draw = (DefocusType01Packet *)0x70000000;
    Head_0[10] = GetTex0(layer->parameters[0], 0);
    draw->gif_tag = 0x51000006;
    draw->vif_command = 0x8001;
    draw->gs_registers = 0x50AB4000;
    draw->register_count = 0x53531;
    draw->header[0] = 0;
    draw->header[1] = 0;
    draw->header[2] = 0;
    draw->header_padding = 0;
    color = (u8 *)&layer->parameters[3];
    draw->red = color[0];
    draw->green = color[1];
    draw->blue = color[2];
    draw->alpha = color[3];
    draw->u1 = 0x2000;
    draw->u0 = 0;
    draw->left = (layer->parameters[4] << 4) + 0x6FF8;
    draw->right = ((layer->parameters[4] + layer->parameters[6]) << 4) + 0x8FF8;
    draw->v1 = 0x1C00;
    draw->v0 = 0;
    draw->top = (layer->parameters[5] << 4) + 0x71F7;
    draw->bottom = ((layer->parameters[5] + layer->parameters[7]) << 4) + 0x8DF7;
    z = layer->parameters[2];
    draw->z1 = z;
    draw->z0 = z;
    draw->flags1 = 0;
    draw->flags0 = 0;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, Head_0, 0x18);
    if (layer->parameters[1] != 0) {
        do {
            i++;
            sceVif1PkAddDataN(packet, (void *)0x70000000, 0x1C);
        } while ((u32)i < (u32)layer->parameters[1]);
    }
}

extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkAddDataN(XglPacket *packet, const void *data, int count);
typedef struct {
    u64 unmodeled_00[6];
    u64 tex0_a;
    u8 unmodeled_38[0x20];
    int width_a;
    int unmodeled_5c[3];
    int width_b;
    int unmodeled_6c[5];
    u64 tex0_b;
} DefocusPrimBuf;
extern u64 TestPrim_1[];
extern DefocusPrimBuf TestPrim_2;
extern u64 TestPrim_3[];

static void DefocusMainType02(DefocusLayer *layer, XglPacket *packet)
{
    TestPrim_1[12] = (u64)sRender.tex0_state_14 | 0x80000;
    TestPrim_1[14] = (u64)0x24020000 | (u64)(sRender.tex0_state_20 << 5) |
                     ((u64)0xC800 << 19);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, TestPrim_1, 0x3C);
    if (layer->parameters[1] != 0) {
        TestPrim_2.width_a = layer->parameters[1];
        TestPrim_2.tex0_a = (u64)sRender.tex0_state_14 | 0x80000;
        TestPrim_2.width_b = layer->parameters[1];
        TestPrim_2.tex0_b = (u64)sRender.tex0_state_20 | 0x80000;
        sceVif1PkCnt(packet, 0);
        sceVif1PkAddDataN(packet, &TestPrim_2, 0x24);
        return;
    }
    TestPrim_3[4] = (u64)sRender.tex0_state_20 | 0x80000;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, TestPrim_3, 0xC);
}

typedef struct {
    u32 header[3];
    u32 gif_tag;
    u32 vif_command;
    u32 gs_registers;
    u32 register_count;
    u32 header_padding;
    u32 red;
    u32 green;
    u32 blue;
    u32 alpha;
    u32 u0;
    u32 v0;
    u32 pad38[2];
    u32 left;
    u32 top;
    u32 z0;
    u32 flags0;
    u32 u1;
    u32 v1;
    u32 pad58[2];
    u32 right;
    u32 bottom;
    u32 z1;
    u32 flags1;
} DefocusType03Packet;
extern u64 Head_4_00367170[];

static void DefocusMainType03(DefocusLayer *layer, XglPacket *packet)
{
    int i;
    u8 *color;
    DefocusType03Packet *draw;

    i = 0;
    Head_4_00367170[10] = GetTex0(2, 1);
    draw = (DefocusType03Packet *)0x70000000;
    draw->gif_tag = 0x51000006;
    draw->vif_command = 0x8001;
    draw->gs_registers = 0x50AB4000;
    draw->register_count = 0x53531;
    draw->header[0] = 0;
    draw->header[1] = 0;
    draw->header[2] = 0;
    draw->header_padding = 0;
    color = (u8 *)&layer->parameters[1];
    draw->red = color[0];
    draw->green = color[1];
    draw->blue = color[2];
    draw->alpha = color[3];
    draw->u1 = 0x2000;
    draw->u0 = 0;
    draw->left = (layer->parameters[2] << 4) + 0x6FF8;
    draw->right = ((layer->parameters[2] + layer->parameters[4]) << 4) + 0x8FF8;
    draw->v1 = 0x1C00;
    draw->v0 = 0;
    draw->top = (layer->parameters[3] << 4) + 0x71F7;
    draw->bottom = ((layer->parameters[3] + layer->parameters[5]) << 4) + 0x8DF7;
    draw->z1 = -1;
    draw->z0 = -1;
    draw->flags1 = 0;
    draw->flags0 = 0;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, Head_4_00367170, 0x18);
    if (layer->parameters[0] != 0) {
        do {
            i++;
            sceVif1PkAddDataN(packet, (void *)0x70000000, 0x1C);
        } while ((u32)i < (u32)layer->parameters[0]);
    }
}

typedef struct {
    u32 header[3];
    u32 gif_tag;
    u32 vif_command;
    u32 gs_registers;
    u32 register_count;
    u32 header_padding;
    u32 red;
    u32 green;
    u32 blue;
    u32 alpha;
    u32 x0;
    u32 y0;
    u32 pad38[2];
    u32 left;
    u32 top;
    u32 color;
    u32 flags;
    u32 x1;
    u32 y1;
    u32 pad58[2];
    u32 right;
    u32 bottom;
    u32 second_color;
    u32 second_flags;
} DefocusType05Packet;
extern u64 Head_5[];

static void DefocusMainType05(DefocusLayer *layer, XglPacket *packet)
{
    DefocusType05Packet *draw;
    u8 *color;
    u32 v;
    u64 tex0;
    u64 clut;

    Head_5[10] = GetTex0(0, 0);
    clut = ((u64)(layer->parameters[6] ^ 0x3FF) << 4) | 0xF;
    Head_5[12] = clut | ((u64)(layer->parameters[7] ^ 0x3FF) << 24);
    draw = (DefocusType05Packet *)0x70000000;
    draw->gif_tag = 0x51000006;
    draw->vif_command = 0x8001;
    draw->gs_registers = 0x50AB4000;
    draw->register_count = 0x53531;
    draw->header[0] = 0;
    draw->header[1] = 0;
    draw->header[2] = 0;
    draw->header_padding = 0;
    color = (u8 *)&layer->parameters[1];
    draw->red = color[0];
    draw->green = color[1];
    draw->blue = color[2];
    draw->alpha = color[3];
    v = (u32)layer->parameters[2] << 4;
    draw->x0 = v;
    draw->left = v + 0x6FF8;
    v = (u32)layer->parameters[3] << 4;
    draw->y0 = v;
    draw->top = v + 0x71F7;
    v = (u32)(layer->parameters[2] + layer->parameters[4]) << 4;
    draw->x1 = v;
    draw->right = v + 0x6FF8;
    v = (u32)(layer->parameters[3] + layer->parameters[5]) << 4;
    draw->y1 = v;
    draw->bottom = v + 0x71F7;
    v = layer->parameters[0];
    draw->second_flags = 0;
    draw->second_color = v;
    draw->color = v;
    draw->flags = 0;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, Head_5, 0x1C);
    sceVif1PkAddDataN(packet, (void *)0x70000000, 0x1C);
}

typedef struct {
    u8 red;
    u8 green;
    u8 blue;
    u8 alpha;
} DefocusSolidColor;
typedef struct {
    u8 unmodeled_00[4];
    u32 draw_color;
    DefocusSolidColor color_bytes;
    int x;
    int y;
    int width;
    int height;
} DefocusSolidRect;
typedef struct {
    u32 header[3];
    u32 gif_tag;
    u32 vif_command;
    u32 gs_registers;
    u32 register_count;
    u32 header_padding;
    u32 red;
    u32 green;
    u32 blue;
    u32 alpha;
    u32 left;
    u32 top;
    u32 color;
    u32 flags;
    u32 right;
    u32 bottom;
    u32 second_color;
    u32 second_flags;
} DefocusSolidPacket;
#define DEFOCUS_SCRATCH ((DefocusSolidPacket *)0x70000000)
#define DEFOCUS_SOLID_GIF_TAG 0x51000004
#define DEFOCUS_SOLID_VIF_COMMAND 0x8001
#define DEFOCUS_SOLID_GS_MODE 0x30234000
#define DEFOCUS_SOLID_GS_REGISTERS 0x551
#define DEFOCUS_SCREEN_X_BIAS 28664
#define DEFOCUS_SCREEN_Y_BIAS 29175
#define DEFOCUS_HEAD_WORDS 16
#define DEFOCUS_SOLID_WORDS 20
extern const u32 Head_6_00367240[];
extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkAddDataN(XglPacket *packet, const void *data, int count);

static void DefocusMainType06(DefocusSolidRect *rect, XglPacket *packet)
{
    DefocusSolidPacket *draw;
    DefocusSolidColor *color;
    u32 left;
    u32 top;
    u32 draw_color;

    draw = DEFOCUS_SCRATCH;
    color = &rect->color_bytes;
    draw->gif_tag = DEFOCUS_SOLID_GIF_TAG;
    draw->vif_command = DEFOCUS_SOLID_VIF_COMMAND;
    draw->gs_registers = DEFOCUS_SOLID_GS_MODE;
    draw->register_count = DEFOCUS_SOLID_GS_REGISTERS;
    draw->header[0] = 0;
    draw->header[1] = 0;
    draw->header[2] = 0;
    draw->header_padding = 0;
    draw->red = color->red;
    draw->green = color->green;
    draw->blue = color->blue;
    draw->alpha = color->alpha;
    left = ((u32)rect->x << 4) + DEFOCUS_SCREEN_X_BIAS;
    draw->left = left;
    draw->right = left + ((u32)rect->width << 4);
    top = ((u32)rect->y << 4) + DEFOCUS_SCREEN_Y_BIAS;
    draw->top = top;
    draw->bottom = top + ((u32)rect->height << 4);
    draw_color = rect->draw_color;
    draw->color = draw_color;
    draw->second_color = draw_color;
    draw->flags = 0;
    draw->second_flags = 0;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, Head_6_00367240, DEFOCUS_HEAD_WORDS);
    sceVif1PkAddDataN(packet, DEFOCUS_SCRATCH, DEFOCUS_SOLID_WORDS);
}

extern float I2F(int value);
extern int F2I(float value);
extern float xglSin(float angle);
extern float xglCos(float angle);
typedef struct {
    u8 unmodeled_00[0x50];
    u64 texture;
    u8 unmodeled_58[8];
} DefocusTexturedHeader;
extern DefocusTexturedHeader Head_7;
typedef struct {
    u32 header[3];
    u32 gif_tag;
    u32 vif_command;
    u32 gs_mode;
    u32 gs_registers;
    u32 header_padding;
    u32 red;
    u32 green;
    u32 blue;
    u32 alpha;
    u32 sprite_tag_low;
    u32 sprite_tag_high;
    u32 sprite_registers;
    u32 sprite_padding;
} DefocusWaveHeader;
#define DEFOCUS_WAVE_HEADER ((DefocusWaveHeader *)0x70000000)
typedef struct {
    int u;
    int v;
    u32 unmodeled_08[2];
    int x;
    int y;
    u32 color;
    u32 flags;
} DefocusRadialVertex;
#define DEFOCUS_RADIAL_VERTICES ((DefocusRadialVertex *)0x70000000)

static void DefocusMainType07(DefocusLayer *layer, XglPacket *packet)
{
    const u8 *color;
    float angle;
    float next_angle;
    float sin_angle;
    float cos_angle;
    float sin_next;
    float cos_next;
    float uv_radius_x;
    float uv_radius_y;
    float screen_radius_x;
    float screen_radius_y;
    int ring;
    int step;
    int offset_u;
    int offset_v;
    int offset_x;
    int offset_y;
    u32 flags;
    u32 skip_flag = 0x8000;
    DefocusWaveHeader *header = DEFOCUS_WAVE_HEADER;

    Head_7.texture = GetTex0(2, 0);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, &Head_7, 24);
    header->gif_tag = 0x51000443;
    header->gs_mode = 0x10aa4000;
    header->vif_command = 1;
    header->gs_registers = 1;
    header->header[0] = 0;
    header->header[1] = 0;
    header->header[2] = 0;
    header->header_padding = 0;
    color = (const u8 *)&layer->parameters[1];
    header->red = color[0];
    header->green = color[1];
    header->blue = color[2];
    header->alpha = color[3];
    header->sprite_tag_low = 0x8220;
    header->sprite_tag_high = 0x20000000;
    header->sprite_registers = 0x53;
    header->sprite_padding = 0;
    sceVif1PkAddDataN(packet, DEFOCUS_WAVE_HEADER, 16);
    for (ring = 0; ring < 32; ring++) {
        angle = I2F(ring) * 0.03125f * 2.0f * 3.1415927f;
        next_angle = I2F(ring + 1) * 0.03125f * 2.0f * 3.1415927f;
        sin_angle = xglSin(angle);
        cos_angle = xglCos(angle);
        sin_next = xglSin(next_angle);
        cos_next = xglCos(next_angle);
        for (step = 0; step < 8; step++) {
            flags = 0;
            if (step == 0) {
                flags = skip_flag;
            }
            screen_radius_x = I2F(layer->parameters[4] * (8 - step)) * 0.125f;
            screen_radius_y = I2F(layer->parameters[5] * (8 - step)) * 0.125f;
            uv_radius_x = I2F(layer->parameters[4]) -
                          I2F(layer->parameters[4] * step * layer->parameters[6]) *
                              0.00048828125f;
            uv_radius_y = I2F(layer->parameters[5]) -
                          I2F(layer->parameters[5] * step * layer->parameters[6]) *
                              0.00048828125f;
            offset_u = F2I(cos_angle * uv_radius_x);
            offset_v = F2I(sin_angle * uv_radius_y);
            offset_x = F2I(cos_angle * screen_radius_x);
            offset_y = F2I(sin_angle * screen_radius_y);
            ((int *)0x70000000)[0] = (layer->parameters[2] + offset_u) << 4;
            ((int *)0x70000000)[1] = (layer->parameters[3] + offset_v) << 4;
            ((int *)0x70000000)[4] =
                ((layer->parameters[2] + offset_x) << 4) + 28664;
            ((int *)0x70000000)[5] =
                ((layer->parameters[3] + offset_y) << 4) + 29175;
            ((int *)0x70000000)[6] = layer->parameters[0];
            ((int *)0x70000000)[7] = flags;
            offset_u = F2I(cos_next * uv_radius_x);
            offset_v = F2I(sin_next * uv_radius_y);
            offset_x = F2I(cos_next * screen_radius_x);
            offset_y = F2I(sin_next * screen_radius_y);
            ((int *)0x70000000)[8] = (layer->parameters[2] + offset_u) << 4;
            ((int *)0x70000000)[9] = (layer->parameters[3] + offset_v) << 4;
            ((int *)0x70000000)[12] =
                ((layer->parameters[2] + offset_x) << 4) + 28664;
            ((int *)0x70000000)[13] =
                ((layer->parameters[3] + offset_y) << 4) + 29175;
            ((int *)0x70000000)[14] = layer->parameters[0];
            ((int *)0x70000000)[15] = flags;
            sceVif1PkAddDataN(packet, DEFOCUS_RADIAL_VERTICES, 16);
        }
        ((int *)0x70000000)[0] = layer->parameters[2] << 4;
        ((int *)0x70000000)[1] = layer->parameters[3] << 4;
        ((int *)0x70000000)[4] = (layer->parameters[2] << 4) + 28664;
        ((int *)0x70000000)[5] = (layer->parameters[3] << 4) + 29175;
        ((int *)0x70000000)[6] = layer->parameters[0];
        ((int *)0x70000000)[7] = 0;
        sceVif1PkAddDataN(packet, DEFOCUS_RADIAL_VERTICES, 8);
    }
}

extern unsigned short xglSRand(void);
extern float xglFRand(void);
extern u8 rate_9[37];
extern u64 Head_8[12];
typedef struct {
    u32 header[3];
    u32 gif_tag;
    u32 vif_command;
    u32 gs_mode;
    u32 gs_registers;
    u32 header_padding;
} DefocusStarHeader;
typedef struct {
    int x;
    int y;
} DefocusRingPoint;
typedef struct {
    DefocusRingPoint inner[33];
    DefocusRingPoint middle[33];
    DefocusRingPoint outer[33];
} DefocusRingTable;
#define DEFOCUS_STAR_HEADER ((DefocusStarHeader *)0x70000318)
typedef struct {
    int red;
    int green;
    int blue;
    int alpha;
    int x;
    int y;
    int z;
    int flags;
} DefocusStarVertex;
#define DEFOCUS_STAR_STRIP ((DefocusStarVertex *)0x70000338)
#define DEFOCUS_STAR_WORD(index) (((int *)0x70000318)[index])
#define DEFOCUS_RING_TABLE ((DefocusRingTable *)0x70000000)

static void DefocusMainType08(DefocusLayer *layer, XglPacket *packet)
{
    DefocusStarHeader *header = DEFOCUS_STAR_HEADER;
    float base_angle;
    float spread;
    float angle;
    float sine;
    float cosine;
    u32 index = 0;
    int x;
    int y;
    int spark_depth;
    int spark_x;
    int spark_y;
    int along_x;
    int along_y;
    int across_x;
    int across_y;
    int segment;
    int depth;
    int ring;
    u32 row;
    u32 spawn_x;
    u32 spawn_y;
    u32 lifetime;
    u32 *slot;
    int *shared;
    DefocusStarVertex *strip;
    u32 phase;
    int shade0;
    int shade1;
    int shade2;
    int shade3;
    int shade4;
    int shade5;
    int level2;
    int level3;

    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, &Head_8, 16);
    header->gif_tag = 0x51000009;
    header->vif_command = 0x8001;
    header->gs_mode = 0x8026c000;
    header->gs_registers = 0x55555151;
    header->header[0] = 0;
    header->header[1] = 0;
    header->header[2] = 0;
    header->header_padding = 0;
    base_angle = I2F(layer->parameters[3]) / 180.0f * 3.1415927f;
    spread = I2F(layer->parameters[4]) / 180.0f * 3.1415927f;
    spark_depth = layer->parameters[0];
    if (layer->parameters[2] != 0) {
        do {
            index++;
            spark_x = xglSRand() & 0x1fff;
            spark_y = xglSRand() & 0x1fff;
            angle = base_angle + (xglFRand() - 1.5f) * spread;
            spark_x += 28672;
            spark_y += 29184;
            along_x = F2I(xglCos(angle) * 4.0f * 16.0f);
            along_y = F2I(xglSin(angle) * 4.0f * 16.0f);
            across_x = F2I(xglSin(angle) * 128.0f * 16.0f);
            across_y = F2I(-xglCos(angle) * 128.0f * 16.0f);
            DEFOCUS_STAR_WORD(8) = 255;
            DEFOCUS_STAR_WORD(9) = 255;
            DEFOCUS_STAR_WORD(10) = 255;
            DEFOCUS_STAR_WORD(11) = 64;
            DEFOCUS_STAR_WORD(12) = spark_x;
            DEFOCUS_STAR_WORD(13) = spark_y;
            DEFOCUS_STAR_WORD(14) = spark_depth;
            DEFOCUS_STAR_WORD(15) = 0;
            DEFOCUS_STAR_WORD(16) = 255;
            DEFOCUS_STAR_WORD(17) = 255;
            DEFOCUS_STAR_WORD(18) = 255;
            DEFOCUS_STAR_WORD(19) = 0;
            DEFOCUS_STAR_WORD(20) = spark_x + along_x;
            DEFOCUS_STAR_WORD(21) = spark_y + along_y;
            DEFOCUS_STAR_WORD(36) = spark_x + along_x;
            DEFOCUS_STAR_WORD(37) = spark_y + along_y;
            DEFOCUS_STAR_WORD(22) = spark_depth;
            DEFOCUS_STAR_WORD(38) = spark_depth;
            DEFOCUS_STAR_WORD(23) = 0;
            DEFOCUS_STAR_WORD(39) = 0;
            DEFOCUS_STAR_WORD(24) = spark_x + across_x;
            DEFOCUS_STAR_WORD(25) = spark_y + across_y;
            DEFOCUS_STAR_WORD(26) = spark_depth;
            DEFOCUS_STAR_WORD(27) = 0;
            DEFOCUS_STAR_WORD(28) = spark_x - along_x;
            DEFOCUS_STAR_WORD(29) = spark_y - along_y;
            DEFOCUS_STAR_WORD(30) = spark_depth;
            DEFOCUS_STAR_WORD(31) = 0;
            DEFOCUS_STAR_WORD(32) = spark_x - across_x;
            DEFOCUS_STAR_WORD(33) = spark_y - across_y;
            DEFOCUS_STAR_WORD(34) = spark_depth;
            DEFOCUS_STAR_WORD(35) = 0;
            sceVif1PkAddDataN(packet, DEFOCUS_STAR_HEADER, 40);
        } while (index < (u32)layer->parameters[2]);
    }
    for (ring = 0; ring <= 32; ring++) {
        sine = xglSin(I2F(ring) * 2.0f * 3.1415927f * (1.0f / 32.0f));
        cosine = xglCos(I2F(ring) * 2.0f * 3.1415927f * (1.0f / 32.0f));
        DEFOCUS_RING_TABLE->inner[ring].x = F2I(cosine * 8.0f * 16.0f);
        DEFOCUS_RING_TABLE->inner[ring].y = F2I(sine * 8.0f * 16.0f);
        DEFOCUS_RING_TABLE->middle[ring].x = F2I(cosine * 28.0f * 16.0f);
        DEFOCUS_RING_TABLE->middle[ring].y = F2I(sine * 28.0f * 16.0f);
        DEFOCUS_RING_TABLE->outer[ring].x = F2I(cosine * 48.0f * 16.0f);
        DEFOCUS_RING_TABLE->outer[ring].y = F2I(sine * 48.0f * 16.0f);
    }
    DEFOCUS_STAR_WORD(3) = 0x51000181;
    DEFOCUS_STAR_WORD(4) = 0x80c0;
    DEFOCUS_STAR_WORD(5) = 0x20264000;
    DEFOCUS_STAR_WORD(6) = 0x51;
    DEFOCUS_STAR_WORD(0) = 0;
    DEFOCUS_STAR_WORD(1) = 0;
    DEFOCUS_STAR_WORD(2) = 0;
    DEFOCUS_STAR_WORD(7) = 0;
    for (row = 0; row < (u32)layer->parameters[5]; row++) {
        slot = (u32 *)&layer->parameters[8 + row];
        strip = DEFOCUS_STAR_STRIP;
        if ((*slot >> 24) == 0) {
            spawn_x = xglSRand();
            spawn_y = xglSRand();
            lifetime = xglSRand();
            *slot = ((((spawn_x & 0x1fff) + 28672) & 0xfff0) << 8) + (((spawn_y & 0x1fff) + 29184) >> 4) +
                    ((lifetime % (u32)layer->parameters[6] + 1) << 24);
        }
        *slot -= 0x1000000;
        x = (*slot >> 8) & 0xfff0;
        y = (*slot << 4) & 0xfff0;
        shared = layer->parameters;
        depth = *shared;
        phase = xglSRand() & 3;
        for (segment = 0; segment < 32; segment++) {
            shade0 = 0;
            if (!(rate_9[phase + segment] & 1)) {
                shade0 = 255;
            }
            strip[0].red = shade0;
            strip[0].green = shade0;
            strip[0].blue = shade0;
            strip[0].alpha = 0;
            strip[0].x = x + DEFOCUS_RING_TABLE->inner[segment].x;
            strip[0].y = y + DEFOCUS_RING_TABLE->inner[segment].y;
            strip[0].z = depth;
            strip[0].flags = 0x8000;
            shade1 = 0;
            if (!(rate_9[phase + segment + 1] & 1)) {
                shade1 = 255;
            }
            strip[1].alpha = 0;
            strip[1].red = shade1;
            strip[1].green = shade1;
            strip[1].blue = shade1;
            strip[1].x = x + DEFOCUS_RING_TABLE->inner[segment + 1].x;
            strip[1].y = y + DEFOCUS_RING_TABLE->inner[segment + 1].y;
            strip[1].z = depth;
            strip[1].flags = 0x8000;
            level2 = rate_9[phase + segment];
            shade2 = 0;
            if (!(level2 & 1)) {
                shade2 = 255;
            }
            strip[2].alpha = level2;
            strip[2].red = shade2;
            strip[2].green = shade2;
            strip[2].blue = shade2;
            strip[2].x = x + DEFOCUS_RING_TABLE->middle[segment].x;
            strip[2].y = y + DEFOCUS_RING_TABLE->middle[segment].y;
            strip[2].z = depth;
            strip[2].flags = 0;
            level3 = rate_9[phase + segment + 1];
            shade3 = 0;
            if (!(level3 & 1)) {
                shade3 = 255;
            }
            strip[3].red = shade3;
            strip[3].green = shade3;
            strip[3].blue = shade3;
            strip[3].alpha = level3;
            strip[3].x = x + DEFOCUS_RING_TABLE->middle[segment + 1].x;
            strip[3].y = y + DEFOCUS_RING_TABLE->middle[segment + 1].y;
            strip[3].z = depth;
            strip[3].flags = 0;
            shade4 = 0;
            if (!(rate_9[phase + segment] & 1)) {
                shade4 = 255;
            }
            strip[4].red = shade4;
            strip[4].green = shade4;
            strip[4].blue = shade4;
            strip[4].alpha = 0;
            strip[4].x = x + DEFOCUS_RING_TABLE->outer[segment].x;
            strip[4].y = y + DEFOCUS_RING_TABLE->outer[segment].y;
            strip[4].z = depth;
            strip[4].flags = 0;
            shade5 = 0;
            if (!(rate_9[phase + segment + 1] & 1)) {
                shade5 = 255;
            }
            strip[5].red = shade5;
            strip[5].green = shade5;
            strip[5].blue = shade5;
            strip[5].alpha = 0;
            strip[5].x = x + DEFOCUS_RING_TABLE->outer[segment + 1].x;
            strip[5].y = y + DEFOCUS_RING_TABLE->outer[segment + 1].y;
            strip[5].z = depth;
            strip[5].flags = 0;
            strip += 6;
        }
        sceVif1PkAddDataN(packet, DEFOCUS_STAR_HEADER, 1544);
    }
}

typedef struct {
    u64 flags;
    u8 unmodeled_08[0x40];
    u64 unk48;
} DefocusDispEnv;

extern DefocusDispEnv DispEnv;

extern int ScanLineInterpolate;

typedef struct {
    unsigned int en1 : 1;
    unsigned int en2 : 1;
    unsigned int crtmd : 3;
    unsigned int mmod : 1;
    unsigned int amod : 1;
    unsigned int slbg : 1;
    unsigned int alp : 8;
    unsigned int unmodeled_16 : 16;
    unsigned int unmodeled_32 : 32;
} D9PmodeT9;
typedef struct {
    u64 fbp : 9;
    u64 unmodeled_09 : 55;
} D9DispfbT9;
typedef struct {
    unsigned int dx : 12;
    unsigned int dy : 11;
    unsigned int magh : 4;
    unsigned int magv : 2;
    unsigned int unmodeled_29 : 3;
    unsigned int dw : 12;
    unsigned int dh : 11;
    unsigned int unmodeled_55 : 9;
} D9DisplayT9;
typedef struct {
    unsigned int sx : 12;
    unsigned int sy : 11;
    unsigned int smph : 4;
    unsigned int smpv : 2;
    unsigned int unmodeled_29 : 3;
    unsigned int ww : 12;
    unsigned int wh : 11;
    unsigned int unmodeled_55 : 9;
} D9ExtdataT9;
typedef struct {
    union {
        u64 flags;
        D9PmodeT9 pmode;
    };
    u8 unmodeled_08[8];
    u8 unmodeled_10[8];
    D9DisplayT9 display1;
    u8 unmodeled_20[8];
    D9DispfbT9 dispfb2;
    D9DisplayT9 display2;
    u64 extbuf;
    D9ExtdataT9 extdata;
    u64 extwrite;
} D9DispEnvT9;
#define D9Env (*(D9DispEnvT9 *)&DispEnv)

static void DefocusMainType09(DefocusLayer *layer)
{
    u64 *extbuf;

    if (layer->state == 0) {
        layer->parameters[0] = ScanLineInterpolate;
        layer->parameters[1] = (u32)D9Env.flags & 1;
        if (layer->parameters[2] == 0) {
            ScanLineInterpolate = 2;
        }
        layer->state = 1;
    } else {
        ScanLineInterpolate = 2;
        D9Env.dispfb2.fbp = sRender.tex0_state_14;
        D9Env.pmode.en1 = 1;
        D9Env.pmode.alp = 255;
        D9Env.display2.dy = D9Env.display1.dy;
    }
    extbuf = &D9Env.extbuf;
    *extbuf = 0x2120000 | (sRender.tex0_state_14 << 5);
    D9Env.extdata.sx = D9Env.display1.dx + 7;
    D9Env.extdata.sy = D9Env.display1.dy / 2;
    D9Env.extdata.smph = D9Env.display1.magh;
    D9Env.extdata.smpv = D9Env.display1.magv;
    D9Env.extdata.ww = 511;
    D9Env.extdata.wh = 223;
    D9Env.extwrite |= 1;
}

static void DefocusMainType09Final(DefocusLayer *layer)
{
    u64 unk48;
    u64 flags = DispEnv.flags;
    u64 bit = (u32)layer->parameters[1];
    int unk4;

    unk48 = DispEnv.unk48;
    unk4 = layer->parameters[0];

    flags = flags & ~1;
    unk48 = unk48 & ~1;
    bit = bit & 1;
    flags = flags | bit;

    ScanLineInterpolate = unk4;
    DispEnv.flags = flags;
    DispEnv.unk48 = unk48;
}

typedef struct {
    u8 type;
    u8 state;
    u8 index;
    u8 unmodeled_03;
    u8 step[4];
    int dx;
    int dy;
    int dw;
    int dh;
    u8 color[4];
    int x;
    int y;
    int w;
    int h;
} DefocusType10Layer;
#define D(i) (*(u32 *)(0x70000000 + (i) * 4))
extern u64 Copy_10[14];
extern u64 Head_11[12];
extern void GameDefocusSet(int index, int type, const int *settings);

static void DefocusMainType10(DefocusType10Layer *layer, XglPacket *packet)
{
  u8 s0;
  int s1;
  int new_var2;
  u8 new_var3;
  u8 s2;
  u8 s3;
  u8 c0;
  u8 c1;
  u8 c2;
  u8 c3;
  u8 r;
  u8 g;
  u8 b;
  int new_var;
  u8 a;
  u8 new_var4;
  switch (layer->state)
  {
    case 0:
      Copy_10[4] = ((0x80000 | (sRender.tex0_state_20 << 5)) | (((u64) sRender.tex0_state_14) << 37)) | (((u64) 0x8000) << 36);
      sceVif1PkCnt(packet, 0);
      sceVif1PkAddDataN(packet, Copy_10, 0x1C);
      *((u32 *) layer->color) = 0x80808080;
      layer->state = 1;
      layer->x = 0;
      layer->y = 0;
      layer->w = 0;
      layer->h = 0;

    case 1:
      new_var4 = layer->step[0];
      c0 = layer->color[0];
      new_var3 = layer->step[1];
      s0 = new_var4;
      s1 = new_var3;
      s2 = layer->step[2];
      s3 = layer->step[3];
      c1 = layer->color[1];
      c2 = layer->color[2];
      c3 = layer->color[3];
      if (c0 >= s0)
    {
      r = c0 - s0;
    }
    else
    {
      r = 0;
    }
      if (!(c1 < ((unsigned long long) s1)))
    {
      g = c1 - s1;
    }
    else
    {
      g = 0;
    }
      if (!(c2 < s2))
    {
      b = c2 - s2;
    }
    else
    {
      b = 0;
    }
      ;
      if (!(c3 < s3))
    {
      a = c3 - s3;
    }
    else
    {
      a = 0;
    }
      if (a == 0)
    {
      GameDefocusSet(layer->index, 0, (void *) 0);
      return;
    }
      layer->color[0] = r;
      *((u32 *) (0x70000000 + (8 * 4))) = r;
      layer->color[1] = g;
      *((u32 *) (0x70000000 + (9 * 4))) = g;
      layer->color[2] = b;
      *((u32 *) (0x70000000 + (10 * 4))) = b;
      layer->color[3] = a;
      *((u32 *) (0x70000000 + (((short) 11) * 4))) = a;
      Head_11[10] = GetTex0(2, 0);
      *((u32 *) (0x70000000 + (4 * 4))) = 0x8001;
      *((u32 *) (0x70000000 + (3 * 4))) = 0x51000006;
      *((u32 *) (0x70000000 + (5 * 4))) = 0x50AB4000;
      *((u32 *) (0x70000000 + (6 * 4))) = 0x53531;
      s0 = new_var4;
      *((u32 *) (0x70000000 + (0 * 4))) = 0;
      *((u32 *) (0x70000000 + (1 * 4))) = 0;
      *((u32 *) (0x70000000 + (2 * 4))) = 0;
      *((u32 *) (0x70000000 + (7 * 4))) = 0;
      layer->x += layer->dx;
      layer->y += layer->dy;
      layer->w += layer->dw;
      new_var2 = layer->dh;
      layer->h += new_var2;
      *((u32 *) (0x70000000 + (20 * 4))) = 0x2000;
      *((u32 *) (0x70000000 + (12 * 4))) = 0;
      *((u32 *) (0x70000000 + (16 * 4))) = (layer->x << 4) + 0x6FF8;
      s1 = layer->x;
      new_var = layer->w;
      *((u32 *) (0x70000000 + (21 * 4))) = 0x1C00;
      *((u32 *) (0x70000000 + (13 * 4))) = 0;
      *((u32 *) (0x70000000 + (24 * 4))) = ((s1 + new_var) << 4) + 0x8FF8;
      *((u32 *) (0x70000000 + (17 * 4))) = (layer->y << 4) + 0x71F7;
      *((u32 *) (0x70000000 + (25 * 4))) = ((layer->y + layer->h) << 4) + 0x8DF7;
      *((u32 *) (0x70000000 + (26 * 4))) = -1;
      *((u32 *) (0x70000000 + (18 * 4))) = -1;
      *((u32 *) (0x70000000 + (27 * 4))) = 0;
      *((u32 *) (0x70000000 + (19 * 4))) = 0;
      sceVif1PkCnt(packet, 0);
      sceVif1PkAddDataN(packet, Head_11, 0x18);
      sceVif1PkAddDataN(packet, (void *) 0x70000000, 0x1C);

  }

}

extern DefocusTexturedHeader Head_12;
typedef struct {
    int x[17];
    int y[17];
} DefocusWaveTableT11;
typedef struct {
    u32 first_u;
    u32 first_v;
    u32 unmodeled_08[2];
    int left;
    int top;
    u32 color;
    u32 flags;
    u32 second_u;
    u32 second_v;
    u32 unmodeled_28[2];
    int right;
    int bottom;
    u32 second_color;
    u32 second_flags;
} DefocusWaveSpriteT11;
#define DEFOCUS_WAVE_TABLE_T11 ((DefocusWaveTableT11 *)0x70000000)
#define DEFOCUS_WAVE_SPRITE_T11 ((DefocusWaveSpriteT11 *)0x70000080)

static void DefocusMainType11(DefocusLayer *layer, XglPacket *packet)
{
    const u8 *color;
    float phase_x;
    float phase_y;
    int index;
    int row;
    int column;
    int *wave_x;
    int *wave_y;
    int x;
    int y;
    int next_x;
    int next_y;
    int sprite_y;
    int sprite_next_y;
    DefocusWaveHeader *header = DEFOCUS_WAVE_HEADER;

    Head_12.texture = GetTex0(2, 0);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, &Head_12, 24);
    header->gif_tag = 0x51000383;
    header->gs_mode = 0x10ab4000;
    header->header[0] = 0;
    header->header[1] = 0;
    header->header[2] = 0;
    header->vif_command = 1;
    header->header_padding = 0;
    header->gs_registers = 1;
    color = (const u8 *)&layer->parameters[1];
    header->red = color[0];
    header->green = color[1];
    header->blue = color[2];
    header->alpha = color[3];
    header->sprite_tag_low = 0x80e0;
    header->sprite_tag_high = 0x40000000;
    header->sprite_registers = 0x5353;
    header->sprite_padding = 0;
    sceVif1PkAddDataN(packet, DEFOCUS_WAVE_HEADER, 16);
    layer->parameters[8] += layer->parameters[4];
    layer->parameters[9] += layer->parameters[7];
    phase_x = I2F(layer->parameters[8] & 0xfff) * 0.0015339808f;
    phase_y = I2F(layer->parameters[9] & 0xfff) * 0.0015339808f;
    for (index = 0; index <= 16; index++) {
        DEFOCUS_WAVE_TABLE_T11->x[index] =
            F2I(xglSin(phase_x) * I2F(layer->parameters[2]));
        phase_x += I2F(layer->parameters[3]) * (1.0f / 4096.0f);
        DEFOCUS_WAVE_TABLE_T11->y[index] =
            F2I(xglSin(phase_y) * I2F(layer->parameters[5]));
        phase_y += I2F(layer->parameters[6]) * (1.0f / 4096.0f);
    }
    ((int *)0x70000000)[16] = 0;
    ((int *)0x70000000)[0] = 0;
    ((int *)0x70000000)[31] = 0;
    ((int *)0x70000000)[17] = 0;
    for (row = 0, wave_y = DEFOCUS_WAVE_TABLE_T11->y, y = 0; row < 14; row++, y = row * 512) {
        column = 0;
        sprite_y = y + 29175;
        next_y = y + 512;
        sprite_next_y = y + 29687;
        wave_x = DEFOCUS_WAVE_TABLE_T11->x;
        for (; column < 16; column++) {
            x = column * 512;
            next_x = x + 512;
            ((int *)0x70000000)[32] = x + wave_x[0];
            ((int *)0x70000000)[33] = y + wave_y[0];
            ((int *)0x70000000)[36] = x + 28664;
            ((int *)0x70000000)[37] = sprite_y;
            ((int *)0x70000000)[38] = layer->parameters[0];
            ((int *)0x70000000)[39] = 0;
            ((int *)0x70000000)[40] = next_x + wave_x[1];
            ((int *)0x70000000)[41] = next_y + wave_y[1];
            ((int *)0x70000000)[44] = x + 29176;
            ((int *)0x70000000)[45] = sprite_next_y;
            ((int *)0x70000000)[46] = layer->parameters[0];
            ((int *)0x70000000)[47] = 0;
            sceVif1PkAddDataN(packet, DEFOCUS_WAVE_SPRITE_T11, 16);
            wave_x++;
        }
        wave_y++;
    }
}

extern const u32 Head_13[];
extern const u32 Tail_15[];
extern void (*func_14[])(DefocusLayer *layer, XglPacket *packet);

static void DefocusMain(XglPacket *packet)
{
    DefocusLayer *layer;
    int i;
    int type;

    layer = GameDefocusParam;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, Head_13, 20);
    for (i = 15; i >= 0; i--, layer++) {
        type = layer->type;
        if (type != 0) {
            if (type >= 0) {
                if (type < 12) {
                    func_14[type - 1](layer, packet);
                }
            }
        }
    }
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, Tail_15, 16);
}

void GameDefocusCheck(void)
{
    int layer_index;
    int active;

    active = 0;
    layer_index = 0;
    if (GameDefocusParam[0].type == 0) {
next_layer:
        layer_index++;
        if (layer_index < 16) {
            if (GameDefocusParam[layer_index].type != 0) {
                goto layer_active;
            }
            goto next_layer;
        }
    } else {
layer_active:
        active = 1;
    }
    if (active == 0) {
        sRender.callback = NULL;
        return;
    }
    sRender.callback = DefocusMain;
}

static void GameDefocusFinalize(int index)
{
    DefocusLayer *layer;

    layer = &GameDefocusParam[index];
    if (layer->type == 9) {
        DefocusMainType09Final(layer);
    }
    layer->index = index;
    layer->state = 0;
    layer->type = 0;
}

extern int printf(const char *format, ...);
extern const char D_004C0200[];

void GameDefocusSet(int index, int type, const int *settings)
{
    int clear_index;
    int parameter_index;
    DefocusLayer *layer;

    if (index >= 16) {
        printf(D_004C0200, index);
        return;
    }
    if (index < 0) {
        for (clear_index = 0; clear_index < 16; clear_index++) {
            GameDefocusFinalize(clear_index);
        }
    } else {
        layer = &GameDefocusParam[index];
        GameDefocusFinalize(index);
        layer->type = type;
        if (type > 0) {
            for (parameter_index = 0; parameter_index < 16; parameter_index++) {
                if (settings == NULL) {
                    layer->parameters[parameter_index] = 0;
                } else {
                    layer->parameters[parameter_index] = settings[parameter_index];
                }
            }
        }
    }
    GameDefocusCheck();
}

extern const char D_004C0228[];

void GameDefocusQuickSet(int group_index, int preset, int color, int intensity)
{
    DefocusLayer *layers;
    u32 first_index;
    u32 row_address;

    row_address = (u32)GameDefocusParam + (u32)group_index * 0x110u;
    if (group_index >= 4) {
        printf(D_004C0228, group_index);
        return;
    }

    layers = (DefocusLayer *)row_address;
    first_index = (u32)group_index * 4u;
    GameDefocusFinalize(first_index);
    GameDefocusFinalize(first_index + 1);
    GameDefocusFinalize(first_index + 2);
    GameDefocusFinalize(first_index + 3);

    switch (preset) {
    case 0:
        break;
    case 1:
        layers[0].type = 1;
        layers[0].parameters[0] = 0;
        layers[0].parameters[1] = 1;
        layers[0].parameters[2] = color;
        layers[0].parameters[3] = 0x40808080;
        layers[0].parameters[4] = intensity;
        layers[0].parameters[5] = 0;
        layers[0].parameters[6] = 0;
        layers[0].parameters[7] = 0;
        layers[1].type = 1;
        layers[1].parameters[0] = 0;
        layers[1].parameters[1] = 1;
        layers[1].parameters[2] = color;
        layers[1].parameters[3] = 0x40808080;
        layers[1].parameters[4] = 0;
        layers[1].parameters[5] = intensity;
        layers[1].parameters[6] = 0;
        layers[1].parameters[7] = 0;
        break;
    case 2:
        layers[0].type = 2;
        layers[0].parameters[0] = 0;
        layers[0].parameters[1] = color;
        layers[1].type = 3;
        layers[1].parameters[0] = 1;
        layers[1].parameters[1] = 0x40808080;
        layers[1].parameters[2] = intensity;
        layers[1].parameters[3] = 0;
        layers[1].parameters[4] = 0;
        layers[1].parameters[5] = 0;
        layers[2].type = 3;
        layers[2].parameters[0] = 1;
        layers[2].parameters[1] = 0x40808080;
        layers[2].parameters[2] = 0;
        layers[2].parameters[3] = intensity;
        layers[2].parameters[4] = 0;
        layers[2].parameters[5] = 0;
        layers[3].type = 3;
        layers[3].parameters[0] = 1;
        layers[3].parameters[1] = 0x40808080;
        layers[3].parameters[2] = intensity;
        layers[3].parameters[3] = intensity;
        layers[3].parameters[4] = 0;
        layers[3].parameters[5] = 0;
        break;
    case 4:
        layers[0].type = 1;
        layers[0].parameters[0] = 1;
        layers[0].parameters[1] = 1;
        layers[0].parameters[2] = color;
        layers[0].parameters[3] = 0x00808080u + ((u32)intensity << 24);
        layers[0].parameters[4] = 0;
        layers[0].parameters[5] = 0;
        layers[0].parameters[6] = 0;
        layers[0].parameters[7] = 0;
        break;
    case 5:
        layers[0].type = 5;
        layers[0].parameters[0] = color;
        layers[0].parameters[1] = 0x80808080;
        layers[0].parameters[4] = 512;
        layers[0].parameters[5] = 448;
        layers[0].parameters[2] = 0;
        layers[0].parameters[3] = 0;
        layers[0].parameters[6] = intensity;
        layers[0].parameters[7] = intensity;
        break;
    case 6:
        layers[0].type = 6;
        layers[0].parameters[0] = color;
        layers[0].parameters[1] = 0x80ffffff;
        layers[0].parameters[2] = 0;
        layers[0].parameters[3] = 0;
        layers[0].parameters[4] = 512;
        layers[0].parameters[5] = 448;
        break;
    case 9:
        layers[0].type = 9;
        break;
    case 10:
        layers[0].type = 10;
        layers[0].parameters[0] = (u32)intensity << 24;
        layers[0].parameters[1] = 0;
        layers[0].parameters[2] = 0;
        layers[0].parameters[3] = 0;
        layers[0].parameters[4] = 0;
        break;
    }
    GameDefocusCheck();
}

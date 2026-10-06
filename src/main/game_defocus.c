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

INCLUDE_ASM("asm/main/nonmatchings/game_defocus", DefocusMainType07);

INCLUDE_ASM("asm/main/nonmatchings/game_defocus", DefocusMainType08);

typedef struct {
    u64 flags;
    u8 unmodeled_08[0x40];
    u64 unk48;
} DefocusDispEnv;

extern DefocusDispEnv DispEnv;

extern int ScanLineInterpolate;

INCLUDE_ASM("asm/main/nonmatchings/game_defocus", DefocusMainType09);

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

INCLUDE_ASM("asm/main/nonmatchings/game_defocus", DefocusMainType10);

INCLUDE_ASM("asm/main/nonmatchings/game_defocus", DefocusMainType11);

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

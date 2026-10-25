#include "common.h"
#include "shared.h"

extern unsigned char TestEnv_0_00367AA0[];

static void clear(void)
{
    sceVif1PkRef(xglPacketGetCurrent(), TestEnv_0_00367AA0, 6, 0, 0, 0);
}

/* Background draw type 0 draws nothing; Game calls it directly. */
void GameBgDrawType0(void)
{
}

/*
 * The do-nothing hook GameBgDrawType1Entry and GameBgDrawType2Entry install
 * at GameLoopState+0x30; GameDrawSync calls it with no arguments.
 */
static void nullfunc(void)
{
}

typedef struct GameBgDrawType1Particle {
    u16 x;
    u16 y;
    s16 velocity_x;
    s16 velocity_y;
} GameBgDrawType1Particle;

typedef struct GameBgDrawType1Parameters {
    u8 initialized;
    u8 unmodeled_01[2];
    u8 particle_count;
    s16 spawn_x;
    s16 spawn_y;
    u16 entry_clear[2];
    GameBgDrawType1Particle particles[256];
} GameBgDrawType1Parameters;

typedef struct GameLoopBackgroundCallbacks {
    u8 unmodeled_00[0x2c];
    void (*background_draw)(void);
    void (*background_sync)(void);
    u8 unmodeled_34[0x2a030 - 0x34];
} GameLoopBackgroundCallbacks;

extern GameBgDrawType1Parameters GameBgDrawType1Param;
extern GameLoopBackgroundCallbacks GameLoopState;
void GameBgDrawType1(void);

typedef struct GameBgDrawType1PacketHeader {
    u32 header_pad_00[3];
    u32 gif_tag;
    u32 vif_command;
    u32 packet_header_14;
    u32 packet_header_18;
    u32 header_pad_1c;
    u64 register_pair_20;
    u64 register_pair_28;
    u64 register_pair_30;
    u64 register_pair_38;
    u32 vu_memory_address;
    u32 gs_mode;
    u32 gs_registers;
    u32 header_pad_4c;
} GameBgDrawType1PacketHeader;
typedef struct GameBgDrawType1Vertex {
    u32 start_st[4];
    u32 x;
    u32 y;
    u32 start_zw[2];
    u32 red;
    u32 green;
    u32 blue;
    u32 alpha;
    u32 end_x;
    u32 end_y;
    u32 end_zw[2];
} GameBgDrawType1Vertex;
typedef struct GameBgDrawType1Packet {
    GameBgDrawType1PacketHeader header;
    GameBgDrawType1Vertex vertices[1];
} GameBgDrawType1Packet;
#define GAME_BG_DRAW_TYPE1_SCRATCH ((GameBgDrawType1Packet *)0x70000000)
extern void sceVif1PkCnt(XglPacket *packet, int count);
extern int sceVif1PkAddDataN(XglPacket *packet, const void *data, int count);

void GameBgDrawType1(void)
{
    GameBgDrawType1Packet *scratch;
    XglPacket *packet;
    int i;
    int j;
    int k;
    GameBgDrawType1Vertex *vertex;
    GameBgDrawType1Parameters *param = &GameBgDrawType1Param;

    switch (param->initialized) {
    case 0:
        for (i = 0; i < 256; i++) {
            param->particles[i].x = 0;
            param->particles[i].y = 0;
        }
        param->initialized = 1;
        /* fallthrough */
    case 1:
        for (j = 0; j < param->particle_count; j++) {
            param->particles[j].x += param->particles[j].velocity_x;
            param->particles[j].y += param->particles[j].velocity_y;
            if ((u16)(param->particles[j].x + 0xC000) > 0x8000 || param->particles[j].y < 0x4000 || param->particles[j].y > 0xC000) {
                do {
                    param->particles[j].x = param->spawn_x;
                    param->particles[j].y = param->spawn_y;
                    param->particles[j].velocity_x = (xglSRand() & 0xFF) - 128;
                    param->particles[j].velocity_y = (xglSRand() & 0xFF) - 128;
                } while (param->particles[j].velocity_x == 0 && param->particles[j].velocity_y == 0);
            }
        }
        clear();
        packet = xglPacketGetCurrent();
        scratch = GAME_BG_DRAW_TYPE1_SCRATCH;
        scratch->header.header_pad_00[0] = 0;
        scratch->header.header_pad_00[1] = 0;
        scratch->header.header_pad_00[2] = 0;
        scratch->header.gif_tag = 0x51000004 + param->particle_count * 4;
        scratch->header.vif_command = 0x8001;
        scratch->header.packet_header_14 = 0x20000000;
        scratch->header.packet_header_18 = 0xEE;
        scratch->header.header_pad_1c = 0;
        scratch->header.register_pair_20 = 0x53001;
        scratch->header.register_pair_28 = 0x47;
        scratch->header.register_pair_30 = 0x44;
        scratch->header.register_pair_38 = 0x42;
        scratch->header.vu_memory_address = 0x8000 + param->particle_count;
        scratch->header.gs_mode = 0x4044C000;
        scratch->header.gs_registers = 0x5151;
        scratch->header.header_pad_4c = 0;
        vertex = scratch->vertices;
        for (k = 0; k < param->particle_count; k++) {
            vertex->start_st[0] = 0;
            vertex->start_st[1] = 0;
            vertex->start_st[2] = 0;
            vertex->start_st[3] = 0;
            vertex->x = param->particles[k].x;
            vertex->y = param->particles[k].y;
            vertex->start_zw[0] = 0;
            vertex->start_zw[1] = 0;
            vertex->red = 255;
            vertex->green = 255;
            vertex->blue = 255;
            vertex->alpha = 128;
            vertex->end_zw[0] = 0;
            vertex->end_zw[1] = 0;
            vertex->end_x = param->particles[k].x + param->particles[k].velocity_x;
            vertex->end_y = param->particles[k].y + param->particles[k].velocity_y;
            vertex++;
        }
        sceVif1PkCnt(packet, 0);
        sceVif1PkAddDataN(packet, scratch, param->particle_count * 16 + 20);
        packet = 0;
        break;
    }
}

void GameBgDrawType1Entry(void)
{
    GameBgDrawType1Param.particle_count = -16;
    GameBgDrawType1Param.entry_clear[1] = 0;
    GameBgDrawType1Param.spawn_x = -32768;
    GameBgDrawType1Param.entry_clear[0] = 0;
    GameLoopState.background_draw = GameBgDrawType1;
    GameLoopState.background_sync = nullfunc;
    GameBgDrawType1Param.spawn_y = -32768;
    GameBgDrawType1Param.initialized = 0;
}

typedef struct GameBgDrawType2Layer {
    float a;
    float b;
    float c;
    u8 unmodeled_0c[0x210 - 0xc];
} GameBgDrawType2Layer;

typedef struct GameBgDrawType2Parameters {
    u8 initialized;
    u8 unmodeled_01[2];
    u8 count;
    float f4;
    float f8;
    float fC;
    GameBgDrawType2Layer layers[4];
    float g850;
    float g854;
    float g858;
    float g85c;
    float g860;
    int g864;
    int g868;
    float g86c;
    float g870;
    float g874;
    float g878;
    float g87c;
    float g880;
    u8 unmodeled_884[0x8d4 - 0x884];
} GameBgDrawType2Parameters;

extern GameBgDrawType2Parameters GameBgDrawType2Param;

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct GameBgDepth {
    float z;
    unsigned int unmodeled_04;
} GameBgDepth;

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct GameBgBandTexture GameBgBandTexture;

struct GameBgBandTexture {
    unsigned int header[8];
    unsigned int pixels[2048];
};

extern float I2F(int value);
extern float xglCos(float angle);
extern float xglSin(float angle);
typedef struct GameBgScreenPosition {
    int x;
    int y;
    int z;
} GameBgScreenPosition;
typedef struct GameBgProjectedPoint {
    GameBgScreenPosition position;
    float q;
} GameBgProjectedPoint;
typedef struct GameBgGsPosition {
    unsigned int x;
    unsigned int y;
    unsigned int z;
    unsigned int control;
} GameBgGsPosition;
typedef struct GameBgTextureCoordinates {
    float s;
    float t;
    float q;
    unsigned int unmodeled_0c;
} GameBgTextureCoordinates;
typedef struct GameBgGsColorComponent {
    unsigned int value : 8;
    unsigned int reserved : 24;
} GameBgGsColorComponent;
typedef struct GameBgBandVertex {
    GameBgTextureCoordinates texture;
    GameBgGsColorComponent red;
    GameBgGsColorComponent green;
    GameBgGsColorComponent blue;
    GameBgGsColorComponent alpha;
    GameBgGsPosition position;
} GameBgBandVertex;
typedef struct GameBgGifTagWord {
    unsigned long loop_count : 15;
    unsigned long end_of_packet : 1;
    unsigned long reserved_10 : 16;
    unsigned long reserved_20 : 14;
    unsigned long primitive_enable : 1;
    unsigned long primitive : 11;
    unsigned long format : 2;
    unsigned long register_count : 4;
} GameBgGifTagWord;
typedef struct GameBgGifRegisterList {
    unsigned long front_start_texture : 4;
    unsigned long front_start_color : 4;
    unsigned long front_start_position : 4;
    unsigned long back_start_texture : 4;
    unsigned long back_start_color : 4;
    unsigned long back_start_position : 4;
    unsigned long front_end_texture : 4;
    unsigned long front_end_color : 4;
    unsigned long front_end_position : 4;
    unsigned long back_end_texture : 4;
    unsigned long back_end_color : 4;
    unsigned long back_end_position : 4;
    unsigned long unused_registers : 16;
} GameBgGifRegisterList;
typedef struct GameBgGifTag {
    GameBgGifTagWord tag;
    GameBgGifRegisterList registers;
} GameBgGifTag;
typedef struct GameBgBandPacketHeader {
    unsigned long long vif_nops;
    unsigned long vif_command;
    GameBgGifTag gif;
} GameBgBandPacketHeader;
typedef struct GameBgBandPacket {
    GameBgBandPacketHeader header;
    GameBgBandVertex vertices[4];
} GameBgBandPacket;
typedef struct GameBgBandScratch {
    Vector4 points[4];
    unsigned char unmodeled_40[64];
    GameBgProjectedPoint projected[4];
    unsigned char unmodeled_c0[64];
    GameBgBandPacketHeader header;
    GameBgBandVertex vertices[4];
} GameBgBandScratch;
extern void xglRenderClearColor(unsigned int color);
extern unsigned int xglLRand(void);
extern unsigned char TestEnv_1_00367B00[];
extern void xglRotTransPersN(GameBgProjectedPoint *destination, Matrix4 *matrix,
                           const Vector4 *source, int count, int mode);
typedef struct GameBgBand {
    float angular_velocity;
    float texture_velocity;
    float angle;
    float texture_phase;
    /* Interleaved x, y coordinates for the circular point ring. */
    float coordinates[128];
} GameBgBand;
typedef struct GameBgBandParameters {
    unsigned char initialized;
    unsigned char unmodeled_01[2];
    unsigned char point_count;
    float radius;
    float angular_step;
    float scale;
    GameBgBand bands[4];
    GameBgDepth depths[7];
    unsigned char unmodeled_888[72];
    GameBgBandTexture *texture_packet;
} GameBgBandParameters;

void GameBgDrawType2(void)
{
    float point_component;
    unsigned long vif_command;
    XglPacket *packet = xglPacketGetCurrent();
    GameBgBandParameters *param = (GameBgBandParameters *)&GameBgDrawType2Param;
    GameBgBandTexture *texture;
    GameBgBandScratch *scratch;
    unsigned int *row;
    unsigned int *source_pixel;
    unsigned int *destination_pixel;
    int band_index;
    int draw_band_index;
    int depth_index;
    int point_index;
    int animation_point_index;
    int texture_row;
    int texture_pixel_index;
    int clip_index;
    int j;
    int star;
    int initial_star;
    unsigned int color;
    unsigned int clear_color;
    float phase;
    float band_angle;
    float full_turn;
    float half_turn;
    float negative_full_turn;
    float angle;
    float s_start;
    float s_end;
    float t_front;
    float t_back;
    float point_x;
    float point_y;
    float front_z;
    int clipped;

    switch (param->initialized) {
    case 0:
        xglRenderClearColor(0x80000000);
        param->texture_packet = (GameBgBandTexture *)(((unsigned int)WorkEnd + 15) & ~15);
        WorkEnd = (unsigned char *)(param->texture_packet + 1);
        param->texture_packet->header[3] = 0x51000201;
        param->texture_packet->header[4] = 0x8200;
        param->texture_packet->header[5] = 0x08000000;
        param->texture_packet->header[0] = 0;
        param->texture_packet->header[1] = 0;
        param->texture_packet->header[2] = 0;
        param->texture_packet->header[6] = 0;
        param->texture_packet->header[7] = 0;
        row = param->texture_packet->pixels;
        for (texture_row = 0; texture_row < 64; texture_row++) {
            for (j = 31; j >= 0; j--) {
                row[j] = 0;
            }
            initial_star = xglSRand();
            color = xglLRand();
            initial_star &= 31;
            row[initial_star] = (color & 0xBFBFBF) + 0x80404040;
            row += 32;
        }
        param->initialized = 1;
        /* fallthrough */
    case 1:
        band_index = 0;
        do {
            band_angle = param->bands[band_index].angle;
            phase = param->bands[band_index].texture_phase +
                    param->bands[band_index].texture_velocity;
            while (phase > 1.0f) {
                phase -= 1.0f;
            }
            while (phase < -1.0f) {
                phase += 1.0f;
            }
            param->bands[band_index].texture_phase = phase;
            animation_point_index = 0;
            if (param->point_count != 0) {
                full_turn = 6.2831855f;
                half_turn = 3.1415927f;
                negative_full_turn = -6.2831855f;
                do {
                    angle = band_angle + (I2F(animation_point_index) * 2.0f * half_turn) /
                            I2F(param->point_count);
                    while (angle <= negative_full_turn) {
                        angle += full_turn;
                    }
                    while (full_turn <= angle) {
                        angle -= full_turn;
                    }
                    param->bands[band_index].coordinates[animation_point_index * 2] =
                        xglCos(angle) * param->radius;
                    param->bands[band_index].coordinates[animation_point_index * 2 + 1] =
                        xglSin(angle) * param->radius;
                    animation_point_index++;
                } while (animation_point_index < param->point_count);
            }
            band_index++;
        } while (band_index < 4);
        clear();
        texture = param->texture_packet;
        source_pixel = &texture->pixels[32];
        destination_pixel = texture->pixels;
        for (texture_pixel_index = 2015; texture_pixel_index >= 0; texture_pixel_index--) {
            *destination_pixel++ = *source_pixel++;
        }
        clear_color = 0x80000000;
        for (texture_pixel_index = 31; texture_pixel_index >= 0; texture_pixel_index--) {
            destination_pixel[texture_pixel_index] = clear_color;
        }
        star = xglSRand();
        color = xglLRand();
        star &= 31;
        destination_pixel[star] = (color & 0xBFBFBF) + 0x80404040;
        sceVif1PkRef(packet, TestEnv_1_00367B00, 11, 0, 0, 0);
        sceVif1PkCnt(packet, 0);
        ((void (*)(XglPacket *, const void *, int))sceVif1PkAddDataN)(packet, param->texture_packet, 2056);
        scratch = (GameBgBandScratch *)0x70000000;
        {
            vif_command = 0x5100000d00000000UL;
            {
                GameBgGifTagWord gif_tag = {1, 1, 0, 0, 1, 92, 0, 12};
                GameBgGifRegisterList gif_registers = {
                    2, 1, 5, 2, 1, 5, 2, 1, 5, 2, 1, 5, 0
                };
                scratch->header.gif.registers = gif_registers;
                scratch->header.vif_command = vif_command;
                scratch->header.gif.tag = gif_tag;
                scratch->header.vif_nops = 0;
            }
        }
        for (draw_band_index = 0; draw_band_index < 4; draw_band_index++) {
            for (depth_index = 0; depth_index < 6; depth_index++) {
                t_front = param->depths[depth_index].z * 0.00025f + 0.5f +
                          param->bands[draw_band_index].texture_phase;
                t_back = param->depths[depth_index + 1].z * 0.00025f + 0.5f +
                         param->bands[draw_band_index].texture_phase;
                for (point_index = 0; point_index < param->point_count; point_index++) {
                    s_start = I2F(point_index) * 7.0f / I2F(param->point_count);
                    s_end = I2F(point_index + 1) * 7.0f / I2F(param->point_count);
                    if (point_index == 0) {
                        point_x = param->bands[draw_band_index].coordinates[(param->point_count - 1) * 2];
                        scratch->points[1].x = point_x;
                    } else {
                        point_x = param->bands[draw_band_index].coordinates[(point_index - 1) * 2];
                        scratch->points[1].x = point_x;
                    }
                    point_component = scratch->points[1].x;
                    scratch->points[0].x = point_component;
                    point_x = param->bands[draw_band_index].coordinates[point_index * 2];
                    scratch->points[3].x = point_x;
                    scratch->points[2].x = point_x;
                    if (point_index == 0) {
                        scratch->points[1].y = param->bands[draw_band_index].coordinates[(param->point_count - 1) * 2 + 1];
                    } else {
                        scratch->points[1].y = param->bands[draw_band_index].coordinates[(point_index - 1) * 2 + 1];
                    }
                    scratch->points[0].y = scratch->points[1].y;
                    point_y = param->bands[draw_band_index].coordinates[point_index * 2 + 1];
                    scratch->points[3].y = point_y;
                    scratch->points[2].y = point_y;
                    front_z = -param->depths[depth_index].z;
                    scratch->points[2].z = front_z;
                    scratch->points[0].z = front_z;
                    point_component = -param->depths[depth_index + 1].z;
                    scratch->points[3].z = point_component;
                    scratch->points[1].z = point_component;
                    scratch->points[0].w = scratch->points[1].w =
                        scratch->points[2].w = scratch->points[3].w = 1.0f;
                    xglRotTransPersN(scratch->projected, 0, scratch->points, 4, 0);
                    clipped = 0;
                    clip_index = 0;
                    do {
                        if (clip_index >= 4) {
                            break;
                        }
                        if ((scratch->projected[clip_index].position.x & 0xFFFF0000) != 0 ||
                            (scratch->projected[clip_index].position.y & 0xFFFF0000) != 0 ||
                            scratch->projected[clip_index].position.z < 0) {
                            clipped = 1;
                            break;
                        }
                        clip_index++;
                    } while (!clipped);
                    if (!clipped) {
                        float projected_q;
                        float front_start_s;
                        float front_start_t;
                        float back_start_s;
                        float back_start_t;
                        float front_end_s;
                        float back_end_s;
                        float back_end_t;
                        unsigned int front_position_x;
                        unsigned int front_position_y;
                        unsigned int front_position_z;
                        int back_position_x;
                        projected_q = scratch->projected[0].q;
                        scratch->vertices[0].texture.q = projected_q;
                        front_start_t = t_front * projected_q;
                        front_start_s = s_start * projected_q;
                        projected_q = scratch->projected[1].q;
                        scratch->vertices[1].texture.q = projected_q;
                        back_start_s = s_start * projected_q;
                        back_start_t = t_back * projected_q;
                        projected_q = scratch->projected[2].q;
                        front_end_s = s_end * projected_q;
                        front_position_x = scratch->projected[0].position.x;
                        front_position_y = scratch->projected[0].position.y;
                        front_position_z = scratch->projected[0].position.z;
                        scratch->vertices[0].position.x = front_position_x;
                        scratch->vertices[0].position.y = front_position_y;
                        back_position_x = scratch->projected[1].position.x;
                        scratch->vertices[0].texture.s = front_start_s;
                        scratch->vertices[0].texture.t = front_start_t;
                        scratch->vertices[0].position.z = front_position_z;
                        scratch->vertices[1].texture.s = back_start_s;
                        scratch->vertices[1].texture.t = back_start_t;
                        scratch->vertices[1].position.x = back_position_x;
                        scratch->vertices[1].position.y = scratch->projected[1].position.y;
                        scratch->vertices[1].position.z = scratch->projected[1].position.z;
                        scratch->vertices[2].texture.t = t_front * projected_q;
                        scratch->vertices[2].texture.s = front_end_s;
                        scratch->vertices[0].red = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[0].green = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[0].blue = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[0].alpha = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[0].position.control = 0;
                        scratch->vertices[1].red = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[1].green = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[1].blue = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[1].alpha = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[1].position.control = 0;
                        scratch->vertices[2].texture.q = projected_q;
                        projected_q = scratch->projected[3].q;
                        back_end_s = s_end * projected_q;
                        back_end_t = t_back * projected_q;
                        scratch->vertices[2].position.x = scratch->projected[2].position.x;
                        scratch->vertices[2].position.y = scratch->projected[2].position.y;
                        scratch->vertices[2].position.z = scratch->projected[2].position.z;
                        scratch->vertices[3].texture.s = back_end_s;
                        scratch->vertices[3].texture.t = back_end_t;
                        scratch->vertices[3].texture.q = projected_q;
                        scratch->vertices[3].position.x = scratch->projected[3].position.x;
                        scratch->vertices[3].position.y = scratch->projected[3].position.y;
                        scratch->vertices[3].position.z = scratch->projected[3].position.z;
                        scratch->vertices[2].red = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[2].green = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[2].blue = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[2].alpha = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[2].position.control = 0;
                        scratch->vertices[3].red = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[3].green = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[3].blue = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[3].alpha = (GameBgGsColorComponent){128, 0};
                        scratch->vertices[3].position.control = 0;
                        sceVif1PkAddDataN(packet, &scratch->header,
                                         sizeof(GameBgBandPacket) / sizeof(unsigned int));
                    }
                }
            }
        }
        return;
    }
}

extern float xglFRand(void);
void GameBgDrawType2(void);
void GameBgDrawType2Entry(void)
{
  int new_var;
  float f20;
  float f21;
  float f0;
  f21 = 100.0f;
  GameBgDrawType2Param.count = 0x10;
  GameBgDrawType2Param.f8 = 0.3926991f;
  f20 = 0.01f;
  GameBgDrawType2Param.f4 = f21;
  GameBgDrawType2Param.layers[0].a = f20;
  GameBgDrawType2Param.fC = 1.0f;
  new_var = 0;
  GameBgDrawType2Param.initialized = new_var;
  GameBgDrawType2Param.layers[new_var].c = xglFRand();
  GameBgDrawType2Param.layers[1].a = 0.017f;
  GameBgDrawType2Param.layers[1].c = xglFRand();
  GameBgDrawType2Param.layers[2].a = -0.011f;
  GameBgDrawType2Param.layers[2].c = xglFRand();
  GameBgDrawType2Param.layers[3].a = -0.018f;
  f0 = xglFRand();
  GameBgDrawType2Param.g868 = (*((int *) (&GameBgDrawType2Param.layers[new_var].b)) = new_var);
  GameBgDrawType2Param.layers[3].c = f0;
  GameBgDrawType2Param.layers[1].b = 0.001f;
  GameBgDrawType2Param.layers[2].b = -0.01f;
  GameBgDrawType2Param.layers[3].b = f20;
  GameBgDrawType2Param.g850 = -2000.0f;
  GameBgDrawType2Param.g858 = -500.0f;
  GameBgDrawType2Param.g860 = -100.0f;
  GameBgDrawType2Param.g870 = f21;
  GameBgDrawType2Param.g878 = 500.0f;
  GameBgDrawType2Param.g880 = 2000.0f;
  GameLoopState.background_draw = GameBgDrawType2;
  GameLoopState.background_sync = nullfunc;
}

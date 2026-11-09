#include "common.h"
#include "shared.h"
#include "tya_draw_gauge.h"

enum TyaGaugeVertexWord {
    TYA_GAUGE_VERTEX_X,
    TYA_GAUGE_VERTEX_Y,
    TYA_GAUGE_VERTEX_ATTRIBUTE,
    TYA_GAUGE_VERTEX_UNMODELED,
    TYA_GAUGE_VERTEX_WORD_COUNT
};

enum {
    TYA_GAUGE_COORDINATE_SCALE = 0x10,
    TYA_GAUGE_X_ORIGIN = 0x6FF8,
    TYA_GAUGE_Y_ORIGIN = 0x71F7
};

typedef struct TyaGaugePacket {
    /* Three packet records precede the six vertex records written here. */
    u32 unmodeled_00[12];
    u32 filled_color[4];
    u32 filled_vertices[2][TYA_GAUGE_VERTEX_WORD_COUNT];
    u32 unfilled_color[4];
    u32 unfilled_vertices[2][TYA_GAUGE_VERTEX_WORD_COUNT];
} TyaGaugePacket;

extern void nmlModelDirectSend(int mode, u8 *data, int count);

void tyaDrawGauge(TyaGaugeArgs *gauge)
{
    static TyaGaugePacket TestEnv = {
        { 0, 0, 0, 0x51000008, 0x00008001, 0x70034000, 0x0551551E, 0,
          0x00070000, 0, 0x47, 0 },
        { 0x80, 0xC0, 0x80, 0x80 },
        { { 0, 0, 0xFFFFFFFF, 0 }, { 0, 0, 0xFFFFFFFF, 0 } },
        { 0x40, 0x40, 0x40, 0x80 },
        { { 0, 0, 0xFFFFFFFF, 0 }, { 0, 0, 0xFFFFFFFF, 0 } },
    };
    int left_x;
    int top_y;
    int right_x;
    int bottom_y;
    int fill_width;
    int fill_x;

    left_x = (gauge->x * TYA_GAUGE_COORDINATE_SCALE) + TYA_GAUGE_X_ORIGIN;
    top_y = (gauge->y * TYA_GAUGE_COORDINATE_SCALE) + TYA_GAUGE_Y_ORIGIN;
    right_x = left_x + (gauge->width * TYA_GAUGE_COORDINATE_SCALE);
    bottom_y = top_y + (gauge->height * TYA_GAUGE_COORDINATE_SCALE);
    fill_width = (gauge->current * gauge->width) / gauge->maximum;
    if (fill_width < 0) {
        fill_width = 0;
    }
    if (gauge->width < fill_width) {
        fill_width = gauge->width;
    }
    fill_x = left_x + (fill_width * TYA_GAUGE_COORDINATE_SCALE);

    TestEnv.filled_color[0] = gauge->filled_color[0];
    TestEnv.filled_color[1] = gauge->filled_color[1];
    TestEnv.filled_color[2] = gauge->filled_color[2];
    TestEnv.filled_color[3] = gauge->filled_color[3];
    TestEnv.filled_vertices[0][TYA_GAUGE_VERTEX_X] = left_x;
    TestEnv.filled_vertices[0][TYA_GAUGE_VERTEX_Y] = top_y;
    TestEnv.filled_vertices[0][TYA_GAUGE_VERTEX_ATTRIBUTE] =
        gauge->vertex_attribute;
    TestEnv.filled_vertices[1][TYA_GAUGE_VERTEX_X] = fill_x;
    TestEnv.filled_vertices[1][TYA_GAUGE_VERTEX_Y] = bottom_y;
    TestEnv.filled_vertices[1][TYA_GAUGE_VERTEX_ATTRIBUTE] =
        gauge->vertex_attribute;

    TestEnv.unfilled_color[0] = gauge->unfilled_color[0];
    TestEnv.unfilled_color[1] = gauge->unfilled_color[1];
    TestEnv.unfilled_color[2] = gauge->unfilled_color[2];
    TestEnv.unfilled_color[3] = gauge->unfilled_color[3];
    TestEnv.unfilled_vertices[0][TYA_GAUGE_VERTEX_X] = fill_x;
    TestEnv.unfilled_vertices[0][TYA_GAUGE_VERTEX_Y] = top_y;
    TestEnv.unfilled_vertices[0][TYA_GAUGE_VERTEX_ATTRIBUTE] =
        gauge->vertex_attribute;
    TestEnv.unfilled_vertices[1][TYA_GAUGE_VERTEX_X] = right_x;
    TestEnv.unfilled_vertices[1][TYA_GAUGE_VERTEX_Y] = bottom_y;
    TestEnv.unfilled_vertices[1][TYA_GAUGE_VERTEX_ATTRIBUTE] =
        gauge->vertex_attribute;

    nmlModelDirectSend(1, (u8 *)&TestEnv, 9);
}

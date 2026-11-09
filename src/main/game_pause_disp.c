#include "common.h"
#include "shared.h"
#include "game_pause_disp.h"
#include "main/xgl_packet.h"

/*
 * Seven-quadword VIF1 direct packet (GIF tag + register writes) that draws
 * the pause screen's darkening rectangle.
 */
static unsigned int ShadowEnv[28] __attribute__((aligned(16))) = {
    0x0, 0x0, 0x0, 0x51000006,
    0x8001, 0x50234000, 0x551EE, 0x0,
    0x71001, 0x0, 0x47, 0x0,
    0x44, 0x0, 0x42, 0x0,
    0x0, 0x0, 0x0, 0x40,
    0x6FF8, 0x71F7, 0x40000000, 0x0,
    0x8FF8, 0x8DF7, 0x40000000, 0x0
};

static void DrawShadow(PauseDrawContext *context)
{
    sceVif1PkAddDirectDataN(context->packet, ShadowEnv, 7);
}

static void DrawCredit(PauseDrawContext *context)
{
    PauseCreditData *data = &context->credit_data;
    int first_vertex_x = 30072;
    int first_vertex_y = 0x8C37;
    int vertex_depth = -1;

    data->initial_words[0] = 0x8001;
    data->initial_words[1] = 0x70AB4000;
    data->initial_words[2] = 0x0535316E;
    data->initial_words[3] = 0;
    data->wide_words[0] = 0x0007000D;
    data->wide_words[1] = 71;
    data->wide_words[2] = 0x2007ED8629343C00ULL;
    data->body_words[0] = 128;
    data->body_words[1] = 128;
    data->body_words[2] = 128;
    data->body_words[3] = 128;
    data->body_words[12] = 10752;
    data->body_words[13] = 752;
    data->body_words[16] = 0x8F78;
    data->body_words[17] = 0x8D77;
    data->body_words[11] = 0;
    data->body_words[19] = 0;
    data->body_words[4] = 0x1000;
    data->body_words[5] = 432;
    data->body_words[10] = vertex_depth;
    data->body_words[8] = first_vertex_x;
    data->body_words[9] = first_vertex_y;
    data->body_words[18] = vertex_depth;

    sceVif1PkAddDirectDataN(context->packet, &context->credit_data, 8);
}

void PauseMenu(void)
{
    int pause_mode = GameLoopState.pause_mode;
    unsigned int snapshot_work;
    int snapshot_size;
    int scanline_value;

    switch (pause_mode) {
    case 0:
        xglFontPrintExtFunc(0x00FFFFFF, DrawShadow, 0);
        GamePauseDispCf();
        return;
    case 100:
    case 101:
        if (SnapDrawCreditFlag != 0) {
            xglFontPrintExtFunc(0xFFFFFFFF, DrawCredit, 0);
        }
        GameLoopState.pause_mode = GameLoopState.pause_mode + 1;
        return;
    case 102:
        break;
    default:
        return;
    }
    snapshot_work = ((unsigned int)WorkEnd + 63) & ~63u;
    snapshot_size = GameSnapShotSave(GameLoopState.snapshot_slot, snapshot_work);
    GameSnapShotSaveFile(-1, snapshot_work, snapshot_size);
    xglFontSetFlags(GameLoopState.snapshot_flags);
    scanline_value = GameLoopState.scanline_value;
    GameLoopState.pause_mode = 0;
    ScanLineInterpolate = scanline_value;
}

void GamePauseDispBG(void)
{
    sceVif1PkRef(xglPacketGetCurrent(), ShadowEnv, 7, 0, 0, 0);
}

void GamePauseDispCf(void)
{
    static char pause[] = "\013\015\003\031\003PAUSE\031\002";

    xglFontPrint(256 - xglFontGetStringWidth(pause) / 2,
                 200, 0x00ffffff, pause);
    GamePauseDispBG();
}

void GamePauseDispEvent(void)
{
    static char msg1[] = "\013\015\003\014 \200 \242\244\014\200\200\200\031\003 Button : Skip and proceed\031\002";
    static char msg2[] = "\013\015\003\031\003START Button : Cancel PAUSE\031\002";

    GamePauseDispCf();
    xglFontPrint(256 - xglFontGetStringWidth(msg1) / 2,
                 256, 0x00ffffff, msg1);
    xglFontPrint(256 - xglFontGetStringWidth(msg2) / 2,
                 288, 0x00ffffff, msg2);
}

/*
 * Labels for the alignment fill after the first two messages (splat pad
 * items), so the data carve owns the whole section.
 */
/* The zero fill that ends the original .data (to its 16-byte end). */

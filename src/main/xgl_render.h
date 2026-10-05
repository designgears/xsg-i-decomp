#ifndef SRC_MAIN_XGL_RENDER_H
#define SRC_MAIN_XGL_RENDER_H

#include "shared.h"

/*
 * GS clear-environment block (0x004A9080, size 0x60). xglRenderClearDepth
 * and xglRenderClearFrame each write only the 8-byte tag at +0x10 (a
 * GS-register-shaped value selecting what a queued clear targets);
 * xglRenderClearColor splits a packed 0xAABBGGRR colour into the four
 * one-byte-per-word fields at +0x20..+0x2c. Bytes no claimed function
 * touches stay unmodeled.
 */
typedef struct GsClearEnv {
    u32 tag0;
    u32 tag1;
    u32 tag2;
    u32 tag3;
    u64 target;
    u64 prim;
    u32 color_r;
    u32 color_g;
    u32 color_b;
    u32 color_a;
    u32 xy0;
    u32 xy1;
    u32 pad38;
    u32 pad3c;
    u32 xy2;
    u32 xy3;
    u32 pad48;
    u32 pad4c;
    u64 giftag;
    u64 prim2;
} GsClearEnv;

/*
 * GS display-environment block (0x004A9000, size 0x50). xglRenderDispOff/On
 * only touch the 8-byte flag word at +0x00 (bit 0x2 gates display output).
 */
typedef struct GsDispEnv {
    u64 flags;
    unsigned char unmodeled_08[0x48];
} GsDispEnv;

extern GsClearEnv ClearEnv;
extern GsDispEnv DispEnv;

typedef void (*XglRenderFadeCallback)(XglPacket *packet);

typedef struct XglRenderState {
    u16 unk0;                           /* +0x00 */
    u16 psm;                            /* +0x02 */
    u16 screen_w;                       /* +0x04 */
    u16 screen_h;                       /* +0x06 */
    u16 unk8;                          /* +0x08 */
    unsigned char unmodeled_0a[6];
    u16 width;                         /* +0x10 */
    u16 height;                        /* +0x12 */
    unsigned char unmodeled_14[0x0c];
    u16 display_buffer_base;            /* +0x20 */
    u16 draw_buffer_base;               /* +0x22 */
    unsigned char unmodeled_24[0x20];
    XglRenderFadeCallback fade_callback; /* +0x44 */
    u32 render_status;                  /* +0x48 */
    u32 frame_delta;                    /* +0x4c */
    u32 frame_count;                    /* +0x50 */
    u32 frame_status;                   /* +0x54 */
    unsigned char unmodeled_58[4];
} XglRenderState;

extern XglRenderState sRender;

#endif

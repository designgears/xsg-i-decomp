/*
 * OV02 original TU 2: 0x00a00858..0x00a017a0 (4 functions)
 */
#include "common.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "title.h"

#define TITLE_PARTICLE_STRIDE 16
#define TITLE_PARTICLE_AT(storage, index) \
    ((TitleParticleView *)((u8 *)(storage) + ((index) * TITLE_PARTICLE_STRIDE)))

typedef struct {
    u8 unmodeled_00[0x14];
    u16 framebuffer_page;
    u8 unmodeled_16[0x0a];
    u16 buffer_select;
} DrawImageRenderState;
extern DrawImageRenderState sRender;
extern void sceVif1PkCnt(XglPacket *packet, int count);
extern int sceVif1PkCloseDirectHLCode(XglPacket *packet);
extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);
extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int count);
typedef struct {
    u8 unmodeled_00[0x10];
    u16 width;
    u16 page_a;
    u16 page_b;
} CopyFrameRenderView;

static int copyframe(void)
{
    XglPacket *packet;
    u64 *spr;
    u64 base;
    int w, pa, pb;

    packet = xglPacketGetCurrent();
    spr = (u64 *)0x70000000;
    w = ((CopyFrameRenderView *)&sRender)->width;
    pa = ((CopyFrameRenderView *)&sRender)->page_a;
    pb = ((CopyFrameRenderView *)&sRender)->page_b;
    base = 0x80000 | (w << 5);
    spr[0] = 0;
    spr[1] = 0x51000009;
    spr[2] = ((u64)0x10000000 << 32) | 0x8008;
    spr[3] = 0xE;
    spr[4] = base | ((u64)pa << 37) | ((u64)0x8000 << 36);
    spr[5] = 0x50;
    spr[6] = 0;
    spr[7] = 0x51;
    spr[8] = ((u64)0x1C0 << 32) | 0x200;
    spr[9] = 0x52;
    spr[10] = 2;
    spr[11] = 0x53;
    spr[12] = 0;
    spr[13] = 0x3F;
    spr[14] = base | ((u64)pb << 37) | ((u64)0x8000 << 36);
    spr[15] = 0x50;
    spr[16] = 2;
    spr[17] = 0x53;
    spr[18] = 0;
    spr[19] = 0x3F;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, (void *)0x70000000, 10);
    return sceVif1PkCloseDirectHLCode(packet);
}

INCLUDE_ASM("asm/nonmatchings/ov02/title", build_mipmap);

static void particle_reset(TitleParticleView *particles, int index)
{
    int random_value;
    int y;

    if (index & 1) {
        TITLE_PARTICLE_AT(particles, index)->x =
            (s16)((xglSRand() & 0x7ff) + 6656);
        TITLE_PARTICLE_AT(particles, index)->y =
            (s16)((xglSRand() & 0x7f) + 8);
    } else {
        u16 random_x = (u16)(xglSRand() & 0x7ff);
        TITLE_PARTICLE_AT(particles, index)->x =
            (u16)(random_x + (u16)0xfe00);
        TITLE_PARTICLE_AT(particles, index)->y =
            (s16)-((xglSRand() & 0x7f) + 8);
    }

    TITLE_PARTICLE_AT(particles, index)->z = (s16)(xglSRand() & 0x1fff);
    y = TITLE_PARTICLE_AT(particles, index)->y;
    random_value = TITLE_PARTICLE_AT(particles, index)->z - 3584;
    if (random_value < 0)
        random_value = TITLE_PARTICLE_AT(particles, index)->z - 3073;
    if (y < 0)
        y = -y;
    random_value >>= 9;
    random_value *= y;
    if (random_value < 0)
        random_value += 15;
    TITLE_PARTICLE_AT(particles, index)->depth = (s16)(random_value >> 4);
    TITLE_PARTICLE_AT(particles, index)->speed =
        (s16)((xglSRand() & 0x3f) + 16);
}

INCLUDE_ASM("asm/nonmatchings/ov02/title", Title);

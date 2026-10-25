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
    u8 unmodeled_16[0x4];
    u16 field_1a;
    u8 unmodeled_1c[0x20 - 0x1c];
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

#define SPD(o) (*(u64 *)(spr + (o)))
#define SPW(o) (*(u32 *)(spr + (o)))
#define GS_SET_FRAME(fbp, fbw, psm, fbmsk) \
    ((u64)(fbp) | ((u64)(fbw) << 16) | ((u64)(psm) << 24) | ((u64)(fbmsk) << 32))
#define GS_SET_TEX0(tbp, tbw, psm, tw, th, tcc, tfx, cbp, cpsm, csm, csa, cld) \
    ((u64)(tbp) | ((u64)(tbw) << 14) | ((u64)(psm) << 20) | ((u64)(tw) << 26) | \
     ((u64)(th) << 30) | ((u64)(tcc) << 34) | ((u64)(tfx) << 35) | ((u64)(cbp) << 37) | \
     ((u64)(cpsm) << 51) | ((u64)(csm) << 55) | ((u64)(csa) << 56) | ((u64)(cld) << 61))
#define GS_SET_MIPTBP1(tbp1, tbw1, tbp2, tbw2, tbp3, tbw3) \
    ((u64)(tbp1) | ((u64)(tbw1) << 14) | ((u64)(tbp2) << 20) | ((u64)(tbw2) << 34) | \
     ((u64)(tbp3) << 40) | ((u64)(tbw3) << 54))
#define GS_SET_MIPTBP2(tbp4, tbw4, tbp5, tbw5, tbp6, tbw6) \
    ((u64)(tbp4) | ((u64)(tbw4) << 14) | ((u64)(tbp5) << 20) | ((u64)(tbw5) << 34) | \
     ((u64)(tbp6) << 40) | ((u64)(tbw6) << 54))
typedef struct {
    u8 pad00[0x1A];
    u16 f1a;
    u8 pad1c[4];
    u16 f20;
} BodyRenderView;

static void build_mipmap(XglPacket *pkt)
{
    u64 tex0;
    int fbp;
    u64 mip1, mip2, x2, xa, x3;
    u32 u1 = 0x2000;
    u32 c90;
    u32 c94;
    u32 ca4;
    u32 cb0;
    u32 cb4;
    u32 c100;
    u32 c104;
    u32 c110;
    u32 c114;
    u32 c120;
    u32 c124;
    u32 c140;
    u32 c150;
    u32 c154;
    u32 c160;
    u32 c164;
    u32 c170;
    u32 c174;
    u32 c190;
    u32 c1a0;
    u32 c1a4;
    u32 c1b0;
    u32 c1b4;
    u32 c1c0;
    u32 c1c4;
    u8 *spr = (u8 *)0x70000000;
    tex0 = GS_SET_TEX0(((BodyRenderView *)&sRender)->f20 << 5, 8, 0, 9, 9, 1, 0, 0, 0, 0, 0, 0);
    SPD(0x00) = 0;
    SPD(0x08) = 0x51000023;
    SPD(0x10) = ((u64)0x10000000 << 0x20) | 4;
    SPD(0x18) = 0xE;
    SPD(0x20) = GS_SET_FRAME(((BodyRenderView *)&sRender)->f1a, 8, 0, 0);
    SPD(0x28) = 0x4C;
    SPD(0x30) = tex0;
    SPD(0x38) = 6;
    ca4 = 0x1C00;
    c100 = 0x7FF8;
    SPD(0x40) = 0x60;
    c1b4 = 0x380;
    SPD(0x48) = 0x14;
    c104 = 0x71F7;
    c110 = 0x1000;
    SPD(0x50) = 0x30000;
    SPD(0x58) = 0x47;
    SPD(0x60) = ((u64)0x508B4000 << 0x20) | 1;
    SPD(0x68) = 0x53531;
    SPW(0x70) = 0x80;
    SPW(0x74) = 0x80;
    SPW(0x78) = 0x80;
    c90 = 0x6FF8;
    c1a0 = 0x8BF8;
    SPW(0x7C) = 0x80;
    SPW(0x80) = 0;
    c94 = 0x71F7;
    cb0 = 0x7FF8;
    SPW(0x84) = 0;
    cb4 = 0x7FF7;
    c114 = 0xE00;
    SPW(0x90) = c90;
    SPW(0x94) = c94;
    c120 = 0x87F8;
    SPW(0x98) = 0;
    SPW(0x9C) = 0;
    SPW(0xA0) = u1;
    SPW(0xA4) = ca4;
    c150 = 0x87F8;
    SPW(0xB0) = cb0;
    SPW(0xB4) = cb4;
    SPW(0xB8) = 0;
    SPW(0xBC) = 0;
    c1b0 = 0x1C00;
    c1c0 = 0x8DF8;
    SPD(0xC0) = ((u64)0x8B4000 << 0x20) | 1;
    SPD(0xC8) = 0x535315353153531EULL;
    c124 = 0x78F7;
    fbp = ((volatile BodyRenderView *)&sRender)->f1a << 5;
    x2 = ((u64)(fbp + 0x80) << 20) | ((u64)8 << 34);
    xa = (u64)fbp | ((u64)8 << 14) | ((u64)8 << 54);
    x3 = (u64)(fbp + 0xC0) << 40;
    mip1 = (xa | x2) | x3;
    mip2 = GS_SET_MIPTBP2(fbp + 0xE0, 8, 0, 0, 0, 0);
    SPD(0xD0) = GS_SET_TEX0(fbp, 8, 0, 9, 9, 1, 0, 0, 0, 0, 0, 0);
    SPD(0xD8) = 6;
    SPW(0xE0) = 0x80;
    SPW(0xE4) = 0x80;
    SPW(0xE8) = 0x80;
    c140 = 0x1000;
    SPW(0xEC) = 0x80;
    SPW(0xF0) = 0;
    SPW(0xF4) = 0;
    SPW(0x100) = c100;
    SPW(0x104) = c104;
    SPW(0x108) = 0;
    c174 = 0x7577;
    c1c4 = 0x73B7;
    c160 = 0x1800;
    SPW(0x10C) = 0;
    c190 = 0x1800;
    c154 = 0x71F7;
    SPW(0x110) = c110;
    SPW(0x114) = c114;
    SPW(0x120) = c120;
    SPW(0x124) = c124;
    c1a4 = 0x71F7;
    SPW(0x128) = 0;
    SPW(0x12C) = 0;
    SPW(0x130) = 0x80;
    c170 = 0x8BF8;
    SPW(0x134) = 0x80;
    SPW(0x138) = 0x80;
    SPW(0x13C) = 0x80;
    SPW(0x140) = c140;
    SPW(0x144) = 0;
    SPW(0x150) = c150;
    SPW(0x154) = c154;
    SPW(0x158) = 0;
    SPW(0x15C) = 0;
    c164 = 0x700;
    SPW(0x160) = c160;
    SPW(0x164) = c164;
    SPW(0x170) = c170;
    SPW(0x174) = c174;
    SPW(0x178) = 0;
    SPW(0x17C) = 0;
    SPW(0x180) = 0x80;
    SPW(0x184) = 0x80;
    SPW(0x188) = 0x80;
    SPW(0x18C) = 0x80;
    SPW(0x190) = c190;
    SPW(0x194) = 0;
    SPW(0x1A0) = c1a0;
    SPW(0x1A4) = c1a4;
    SPW(0x1A8) = 0;
    SPW(0x1AC) = 0;
    SPW(0x1B0) = c1b0;
    SPW(0x1B4) = c1b4;
    SPW(0x1C0) = c1c0;
    SPW(0x1C4) = c1c4;
    SPW(0x1C8) = 0;
    SPW(0x1CC) = 0;
    SPD(0x1D0) = ((u64)0x10000000 << 0x20) | 0x8006;
    SPD(0x1D8) = 0xE;
    SPD(0x1E0) = GS_SET_FRAME(((volatile BodyRenderView *)&sRender)->f20, 8, 0, 0);
    SPD(0x1E8) = 0x4C;
    SPD(0x1F0) = mip1;
    SPD(0x1F8) = 0x34;
    SPD(0x200) = mip2;
    SPD(0x208) = 0x36;
    SPD(0x210) = 0x170;
    SPD(0x218) = 0x14;
    SPD(0x220) = tex0;
    SPD(0x228) = 6;
    SPD(0x230) = 0;
    SPD(0x238) = 0x3F;
    sceVif1PkCnt(pkt, 0);
    sceVif1PkOpenDirectHLCode(pkt, 0);
    sceVif1PkAddDirectDataN(pkt, spr, 0x24);
    sceVif1PkCloseDirectHLCode(pkt);
}

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

typedef signed int s32;
typedef signed long long s64;
extern int D_1245C58;
extern int D_2A007F8;
extern int GameLoopState;
extern int PadData;
extern u64 TestEnv_0_00A0F780[];
extern unsigned char TransEnv_1_00A0F7E0[];
extern unsigned char FlushEnv_2_00A0F800[];
extern unsigned char TestEnv_3[];
extern unsigned char image_4[];
extern unsigned char FlushEnv_5[];
extern unsigned char TestEnvOrg_6[];
extern u64 TestEnv_10[];
extern int TestEnv_11[];
extern u64 TestEnv_12[];
extern int TitleHddInstall(int);
extern void TitleGameInit(void);
extern int MenuFileMain(int);
extern int xglMcRequest(int, int, void *, void *);
extern int xglHddMcExist(void *);
extern int xglMcMain(void);
extern void xglFontLoad(int, int);
extern int xglJpegDecode(void *);
extern void xglSoundEffectNormalID(int, int);
int xglCdSync(void);
void sceVif1PkAddDataN(void *, void *, int);
void FlushCache(int);
extern const char D_00A115D0[];
extern const char D_00A115F0[];
typedef struct {
    u8 unk00[0x2A];
    u16 trigger;
    u8 unk2C[8];
    u16 repeat;
} TitlePadView;
typedef struct {
    u8 unk00[0xE];
    u16 skip;
    u8 unk10[8];
    int mode;
} TitleLoopView;

int *Title(void)
{
    struct {
        s32 sp0;
        s32 sp4;
        s32 sp8;
        s32 pad0c;
        struct {
            s32 a10;
            s32 p14;
            u8 *w18;
            s32 p1c;
            s32 a20;
            u8 *w24;
        } mc;
        s32 pad28;
        s32 pad2c;
        union { s32 w; s16 h[2]; } sp30;
        s32 pad34;
        s32 pad38;
        s32 pad3c;
        void *sp40[8];
    } F;
    u8 *buf;
    s32 timer;
    s32 wait;
    s32 hdd;
    s32 sel;
    s32 fade;
    s32 dir;
    s32 lim;
    s32 i;
    s32 r;
    s32 size;
    s32 jpg;
    XglPacket *packet;
    TitleParticleView *pp;
    u8 *rec;
    s32 k;
    s32 row;
    s32 j, n;
    u8 *hdr;
    u32 wh, hi, lo;
    s32 idx;
    s32 u, x, y;
    s32 x0, w0, y0, h0;
    u32 rnd;
    u16 nx, nz;
    s32 px, pz, spd;

    copyframe();
    *(s32 *)&F.mc.w18 = *(volatile s32 *)&WorkEnd;
    timer = 600;
    F.mc.a10 = -1;
    xglMcRequest(0, 2, &F.mc.a10, &F.sp0);
    xglMcRequest(1, 2, &F.mc.a10, &F.sp4);
    F.mc.w24 = WorkEnd;
    F.mc.a20 = -1;
    F.sp8 = xglHddMcExist(&F.mc.a20);
    xglFontLoad(1, 1);
    do {
        F.sp30.w = xglMcMain();
        xglSleep();
    } while (xglCdSync() != 0 || F.sp30.h[0] == 0xFF || F.sp30.h[0] == 0x80 || F.sp30.h[1] == 0xFF || F.sp30.h[1] == 0x80);
    if (F.sp0 > 0) {
        goto sel_one;
    }
    if (F.sp4 > 0) {
        goto sel_one;
    }
    if (F.sp8 > 0) {
        goto sel_one;
    }
    sel = 0;
    goto sel_done;
sel_one:
    sel = 1;
sel_done:
    switch (xglHddMount()) {
    case 0:
        hdd = 0;
        break;
    case -2:
        hdd = 2;
        break;
    case -3:
        hdd = 3;
        break;
    case -4:
        hdd = 4;
        break;
    case -5:
    case 1:
        hdd = 1;
        break;
    default:
        hdd = -1;
        break;
    }
    size = xglCdReadFile(D_00A115D0, (void *)0x01001000, 0, 1);
    jpg = size + 0x010E1000;
    buf = (u8 *)size + 0x01001000;
    xglCdReadFile(D_00A115F0, (void *)jpg, 0, 1);
    memset(F.sp40, 0, 0x20U);
    F.sp40[0] = (void *)jpg;
    F.sp40[1] = buf;
    xglJpegDecode(F.sp40);
    for (i = 0; i < 0x100; i++) {
        particle_reset((TitleParticleView *)0x01000000, i);
    }
    fade = 0x80;
    copyframe();
    wait = 4;
    for (;;) {
        packet = xglPacketGetCurrent();
        if (--timer <= 0) {
            return &D_2A007F8;
        }
        if (wait > 0) {
            wait -= 1;
        } else if (fade == 0 && (((TitlePadView *)&PadData)->trigger & 0x820)) {
            if (sel == 2 && hdd < 0) {
                xglSoundEffectNormalID(5, 0);
            } else {
                break;
            }
        }
        lim = (hdd < 0) ? 1 : 2;
        dir = 0;
        if (((TitlePadView *)&PadData)->repeat & 0x1000) {
            dir = (sel > 0) ? -1 : 0;
        }
        if (((TitlePadView *)&PadData)->repeat & 0x4000) {
            dir = (sel < lim) ? 1 : dir;
        }
        if (dir != 0) {
            while (dir != 0) {
                sel += dir;
                if (sel != 2 || hdd != -1) {
                    break;
                }
            }
            xglSoundEffectNormalID(3, 0);
            timer = 600;
            wait = (wait < 4) ? 4 : wait;
        }
        if (fade > 0) {
            fade -= 4;
            if (fade < 0) {
                fade = 0;
            }
        }
        TestEnv_0_00A0F780[4] = ((u64)sRender.buffer_select << 0x25) | ((u64)0x8000 << 0x24);
        sceVif1PkCnt(packet, 0);
        sceVif1PkAddDataN(packet, TestEnv_0_00A0F780, 0x18);
        sceVif1PkRef(packet, TransEnv_1_00A0F7E0, 2, 0, 0, 0);
        sceVif1PkRef(packet, buf, 0x7000, 0, 0x51007000, 0);
        sceVif1PkRef(packet, TransEnv_1_00A0F7E0, 2, 0, 0, 0);
        sceVif1PkRef(packet, buf + 0x70000, 0x7000, 0, 0x51007000, 0);
        sceVif1PkRef(packet, FlushEnv_2_00A0F800, 3, 0, 0, 0);
        sceVif1PkRef(packet, TestEnv_3, 0xB, 0, 0, 0);
        sceVif1PkRef(packet, image_4, 0x12, 0, 0, 0);
        __builtin_memcpy((void *)0x70000000, TestEnvOrg_6, 0x70);
        sceVif1PkRef(packet, FlushEnv_5, 3, 0, 0, 0);
        sceVif1PkCnt(packet, 0);
        for (j = 0; j < 0x100; j++) {
            pp = TITLE_PARTICLE_AT(0x01000000, j);
            rnd = xglSRand();
            px = *(s16 *)&pp->x + 0x6FF8;
            pz = pp->z + 0x71F8;
            spd = (u16)pp->speed;
            *(s32 *)0x7000002C = (rnd & 0xF) + 1;
            *(s32 *)0x70000060 = px + spd;
            *(s32 *)0x70000064 = pz + spd;
            *(s32 *)0x70000040 = px;
            *(s32 *)0x70000044 = pz;
            pp->x += pp->y;
            pp->z += pp->depth;
            if (pp->x > 0x2000 || pp->z < 0 || pp->z > 0x1C00) {
                particle_reset((TitleParticleView *)0x01000000, j);
            }
            sceVif1PkAddDataN(packet, (void *)0x70000000, 0x1C);
        }
        hdr = (u8 *)0x01001000;
        rec = hdr + *(s32 *)(hdr + 0xC);
        for (n = 0; n < *(s32 *)(hdr + 8); n++) {
            wh = *(u32 *)rec;
            hi = wh >> 16;
            lo = wh & 0xFFFF;
            TestEnv_10[4] = ((u64)((u32)(*(s32 *)(rec + 8) + 0xE0000) >> 6) << 32) | ((u64)hi << 48);
            TestEnv_10[8] = (u64)lo | ((u64)*(u32 *)(rec + 4) << 32);
            sceVif1PkCnt(packet, 0);
            sceVif1PkAddDataN(packet, TestEnv_10, 0x18);
            sceVif1PkRef(packet, (void *)(*(s32 *)(rec + 0x10) + 0x01001000), *(s32 *)(rec + 0xC), 0, 0, 0);
            rec += 0x14;
        }
        for (k = 0; k < 4; k++) {
            x0 = 0xA6;
            y0 = k * 0x12 + 0x120;
            w0 = 0xB4;
            h0 = 0x12;
            idx = k;
            if (k == 2) {
                if (hdd == -1) {
                    continue;
                }
                if (hdd == 1) {
                    idx = 3;
                }
            }
            if (sel != k) {
                x0 = 0xBA;
                w0 = 0x8C;
                y0 = k * 0x12 + 0x123;
                h0 = 0xC;
            }
            if (k < 3) {
                u = idx * 0x120;
                x = x0 * 16 + 0x6FF8;
                y = y0 * 16 + 0x71F7;
                TestEnv_11[37] = u;
                TestEnv_11[40] = x;
                TestEnv_11[41] = y;
                TestEnv_11[45] = u + 0x120;
                TestEnv_11[48] = x + w0 * 16;
                TestEnv_11[49] = y + h0 * 16;
                sceVif1PkCnt(packet, 0);
                sceVif1PkAddDataN(packet, TestEnv_11, 0x34);
            }
        }
        {
            u64 gs_frame_address_bits = 0xc800;
            TestEnv_12[6] = (0x24120000 | (sRender.framebuffer_page << 5)) | (gs_frame_address_bits << 0x13);
            TestEnv_12[8] = ((u64)fade << 32) | 100;
        }
        FlushCache(0);
        sceVif1PkRef(packet, TestEnv_12, 0xA, 0, 0, 0);
        xglSleep();
    }
    xglSoundEffectNormalID(1, 0);
    copyframe();
    xglSleep();
    switch (sel) {
    case 0:
        TitleGameInit();
        ((TitleLoopView *)&GameLoopState)->mode = 0x100003E8;
        ((TitleLoopView *)&GameLoopState)->skip = 0;
        return &D_1245C58;
    case 1:
        TitleGameInit();
        r = MenuFileMain(1);
        if (r != sel) {
            break;
        }
        ((TitleLoopView *)&GameLoopState)->skip = r;
        return &D_1245C58;
    case 2:
        if (hdd < 0) {
            return 0;
        }
        TitleHddInstall(hdd);
        return 0;
    case 3:
        TitleGameInit();
        ((TitleLoopView *)&GameLoopState)->mode = 0;
        ((TitleLoopView *)&GameLoopState)->skip = 0;
        return &D_1245C58;
    }
    return 0;
}

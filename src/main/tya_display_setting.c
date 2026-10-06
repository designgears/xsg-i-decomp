#include "common.h"
#include "shared.h"
#include "main/xgl_packet.h"

/*
 * sRender's destination-buffer selector: folded (with a fixed high bit)
 * into the register DrawImage primes at TestEnv0[4] before the two-part
 * image transfer below. Only this halfword is evidenced here (see
 * src/main/game_over.c's DrawImage for the same global read through its
 * own local view).
 */
typedef struct {
    u8 unmodeled_00[0x20];
    u16 buffer_select;
} DrawImageRenderState;

extern DrawImageRenderState sRender;

extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkAddDataN(XglPacket *packet, const void *data, int count);
extern u64 TestEnv_0_0036ABC0[12];
extern unsigned char TransEnv_1_0036AC20[];
extern unsigned char FlushEnv_2_0036AC40[];

static void DrawImage(u8 *framebuffer)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    TestEnv_0_0036ABC0[4] = ((u64)sRender.buffer_select << 0x25) | ((u64)0x8000 << 0x24);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, TestEnv_0_0036ABC0, 0x18);
    sceVif1PkRef(packet, TransEnv_1_0036AC20, 2, 0, 0, 0);
    sceVif1PkRef(packet, framebuffer, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, TransEnv_1_0036AC20, 2, 0, 0, 0);
    sceVif1PkRef(packet, framebuffer + 0x70000, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, FlushEnv_2_0036AC40, 3, 0, 0, 0);
}

typedef struct {
    unsigned long DX : 12;
    unsigned long DY : 11;
    unsigned long MAGH : 4;
    unsigned long MAGV : 2;
    unsigned long p0 : 3;
    unsigned long DW : 12;
    unsigned long DH : 11;
    unsigned long p1 : 9;
} TyaGsDisplay;
typedef struct {
    u64 pmode;
    u64 smode2;
    u64 dispfb;
    union { TyaGsDisplay f; u64 raw; } display;
    u64 bgcolor;
    u8 pad[0x28];
} TyaGsDispEnv;
typedef struct {
    u8 unmodeled_00[0x30];
    s16 disp_x;
    s16 disp_y;
} TyaSaveDataView;
typedef struct {
    u32 source;
    u32 destination;
    u8 unmodeled_08[0x18];
} TyaJpegDecodeRequest;
typedef struct {
    u8 pad_00[0x2a];
    u16 pressed;
    u8 pad_2c[8];
    u16 repeat;
} TyaPadView;
extern TyaGsDispEnv DispEnv;
extern TyaPadView PadData;
extern const char D_004C15E8[];
extern const char D_004C1600[];
extern void sceGsSetDefDispEnv(TyaGsDispEnv *env, int psm, int w, int h, int dx, int dy);
extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
extern int xglJpegDecode(void *request);
extern void xglSoundEffectNormalDirect(int effect_id);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern void xglSleep(void);

void tyaDisplaySetting(u8 *buf)
{
    TyaGsDispEnv env;
    TyaJpegDecodeRequest req;
    s16 x;
    s16 y;
    s16 save_x;
    s16 save_y;

    sceGsSetDefDispEnv(&env, 0, 0x200, 0x1C0, 0, 0);
    save_x = ((TyaSaveDataView *)SaveData)->disp_x;
    save_y = ((TyaSaveDataView *)SaveData)->disp_y;
    if (buf == 0) {
        DispEnv.display.f.DX = ((int)env.display.raw & 0xFFF) + save_x * 5;
        DispEnv.display.f.DY = ((int)(env.display.raw >> 12) & 0x7FF) + save_y;
        return;
    }
    if (buf == (u8 *)1) {
        DispEnv.display.f.DX = ((int)env.display.raw & 0xFFF);
        DispEnv.display.f.DY = ((int)(env.display.raw >> 12) & 0x7FF);
        ((TyaSaveDataView *)SaveData)->disp_x = 0;
        ((TyaSaveDataView *)SaveData)->disp_y = 0;
        return;
    }
    xglCdReadFile(D_004C15E8, buf + 0x100000, 0, 1);
    memset(&req, 0, 0x20);
    req.source = (u32)(buf + 0x100000);
    req.destination = (u32)buf;
    xglJpegDecode(&req);
    while (1) {
        x = ((TyaSaveDataView *)SaveData)->disp_x;
        y = ((TyaSaveDataView *)SaveData)->disp_y;
        if (PadData.repeat & 0xF000) {
            xglSoundEffectNormalDirect(3);
        }
        if (PadData.repeat & 0x2000) {
            x++;
        }
        if (PadData.repeat & 0x8000) {
            x--;
        }
        if (PadData.repeat & 0x4000) {
            y++;
        }
        if (PadData.repeat & 0x1000) {
            y--;
        }
        if (PadData.pressed & 0x80) {
            x = 0;
            y = 0;
            xglSoundEffectNormalDirect(1);
        }
        if (PadData.pressed & 0x40) {
            x = save_x;
            y = save_y;
        }
        if (x < -20) x = -20;
        if (x > 20) x = 20;
        if (y < -20) y = -20;
        if (y > 20) y = 20;
        ((TyaSaveDataView *)SaveData)->disp_x = x;
        DispEnv.display.f.DX = ((int)*(int *)&env.display & 0xFFF) + x * 5;
        DispEnv.display.f.DY = ((int)(env.display.raw >> 12) & 0x7FF) + y;
        ((TyaSaveDataView *)SaveData)->disp_y = y;
        if (PadData.pressed & 0x20) {
            xglSoundEffectNormalDirect(1);
            break;
        }
        if (PadData.pressed & 0x40) {
            xglSoundEffectNormalDirect(2);
            break;
        }
        xglFontDebugPrintf(8, 8, D_004C1600, (int)DispEnv.display.raw & 0xFFF, (int)(DispEnv.display.raw >> 12) & 0x7FF, ((TyaSaveDataView *)SaveData)->disp_x, ((TyaSaveDataView *)SaveData)->disp_y);
        DrawImage(buf);
        xglSleep();
    }
    xglSleep();
}

/* Types recovered upstream (origin/master); kept for reference. */
typedef union {
    u64 value;
    struct {
        u32 low;
        u32 high;
    } words;
} GifRegisterData;

typedef struct {
    u32 control_low;
    u32 control_high;
    u32 registers_low;
    u32 registers_high;
} GifTag;

typedef struct {
    GifRegisterData data;
    u32 register_address;
    u32 unused;
} GifAdCommand;

typedef struct {
    u64 dma_tag;
    u32 vif_nop;
    u32 vif_direct;
    GifTag gif_tag;
    GifAdCommand set_pmode;
    GifAdCommand set_smode2;
    GifAdCommand set_dispfb2;
    GifAdCommand set_display2;
} TestEnvironmentPacket;

typedef struct {
    u64 dma_tag;
    u32 vif_nop;
    u32 vif_direct;
    GifTag gif_tag;
} TransferEnvironmentPacket;

typedef struct {
    u64 dma_tag;
    u32 vif_nop;
    u32 vif_direct;
    GifTag gif_tag;
    GifAdCommand finish;
} FlushEnvironmentPacket;


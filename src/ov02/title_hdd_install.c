/*
 * OV02 original TU 4: 0x00a017f0..0x00a023a8 (5 functions)
 */
#include "common.h"
#include "shared.h"
#include "main/xgl_packet.h"

struct HddInstallParams {
    u8 unmodeled_00[8];
    u8 *transfer_buffer;
};

struct HddInstallRenderState {
    u8 unmodeled_00[0x20];
    u16 buffer_select;
};

extern struct HddInstallParams *HddInstallParam;
extern struct HddInstallRenderState sRender;
extern u64 TestEnv_0_00A0FE10[];
extern u8 TransEnv_1_00A0FE70[];
extern u8 FlushEnv_2_00A0FE90[];
extern const char D_00A11620[];

extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkAddDataN(XglPacket *packet, const void *data, int count);
extern void xglFontPrintDirectOT(int color, const char *text);

static void HddInstallSync(void)
{
    XglPacket *packet;
    u8 *transfer_buffer;

    packet = xglPacketGetCurrent();
    transfer_buffer = HddInstallParam->transfer_buffer;
    TestEnv_0_00A0FE10[4] = ((u64)sRender.buffer_select << 37) | ((u64)0x8000 << 36);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, TestEnv_0_00A0FE10, 0x18);
    sceVif1PkRef(packet, TransEnv_1_00A0FE70, 2, 0, 0, 0);
    sceVif1PkRef(packet, transfer_buffer, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, TransEnv_1_00A0FE70, 2, 0, 0, 0);
    sceVif1PkRef(packet, transfer_buffer + 0x70000, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, FlushEnv_2_00A0FE90, 3, 0, 0, 0);
    xglSleep();
    xglFontPrintDirectOT(-1, D_00A11620);
}

struct TitlePadData {
    u8 unk00[0x2A];
    u16 trigger;
};

extern struct TitlePadData PadData;

extern const char D_00A11628[];
extern const char D_00A11688[];
extern const char D_00A11698[];
extern const char D_00A116B0[];
extern const char D_00A116C0[];
extern const char D_00A116E0[];
extern const char D_00A116F8[];
extern const char D_00A11708[];
extern const char D_00A11718[];
extern const char D_00A11738[];
extern const char D_00A117A0[];
extern const char D_00A117B8[];
extern const char D_00A117C8[];
extern const char D_00A117D8[];
extern const char D_00A117F0[];
extern const char D_00A11800[];
extern const char D_00A11818[];
extern const char D_00A11828[];
extern char sztmp_5[];
extern const char *sztmp_6[];
extern char anim_4[];
extern u8 D_00A13654;
extern void tyaDrawGauge(void *g);
extern void xglFontPrintf(int x, int y, unsigned int color, void *arg);
extern int xglJpegDecode(void *request_address);
extern unsigned long long __udivdi3(unsigned long long a, unsigned long long b);
extern void xglSoundEffectNormalDirect(int effect_id);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern void xglFontPrint(int x, int y, int color, const char *text);
extern int xglHddCheck(void);
extern int printf(const char *format, ...);

static int HddInstallCB(int arg0, int arg1, int *arg2) {
    struct {
        short w0;
        short w2;
        int w4;
        short w8;
        short wA;
        short wC;
        short wE;
        u8 b[8];
    } g;
    u64 args[2];
    int jp[8];
    int t16;
    int t18;

    if (*arg2 != 0) {
        HddInstallSync();
        return *arg2;
    }
    xglFontPrint(0x20, 0x10, -1, D_00A11628);
    switch (arg0) {
    case 0:
        xglFontDebugPrintf(0x54, 0xD0, D_00A11688);
        goto done;
    case 1:
        xglFontDebugPrintf(0x54, 0xD0, D_00A11698);
        goto done;
    case 2:
        xglFontDebugPrintf(0x54, 0xD0, D_00A116B0);
        goto done;
    case 3:
        xglFontDebugPrintf(0x54, 0xD0, D_00A116C0, arg1);
        goto done;
    case 4:
        xglFontDebugPrintf(0x54, 0xD0, D_00A116E0);
        goto done;
    case 5:
        xglFontDebugPrintf(0x54, 0xD0, D_00A116F8);
        goto done;
    case 6:
        if (arg1 == 0) {
            xglFontDebugPrintf(0x54, 0xD0, D_00A11708);
            goto done;
        }
        {
            g.w0 = 0xA8;
            g.w2 = 0x1A0;
            g.w4 = -1;
            g.w8 = 0x140;
            g.wC = arg1;
            g.b[0] = 0x80;
            g.b[2] = 0x80;
            g.b[3] = 0x80;
            g.b[4] = 0x40;
            g.b[5] = 0x40;
            g.wA = 0xC;
            g.b[1] = 0xC0;
            g.wE = arg1 >> 16;
            g.b[6] = 0x40;
            g.b[7] = 0x80;
            tyaDrawGauge(&g);
            xglFontPrint(0x20, 0x50, -1, D_00A11718);
            args[0] = (u64)(u32)sztmp_5;
            args[1] = (u64)(u32)(int)(signed char)anim_4[(D_00A13654 >> 3) & 3];
            xglFontPrintf(0x98, 0x19E, -1, args);
            D_00A13654++;
            args[0] = (u64)(u32)sztmp_6;
            args[1] = (u64)(long long)(((arg1 & 0xFFFF) * 100) / (arg1 >> 16));
            xglFontPrintf(0xA8, 0x190, -1, args);
            t18 = (int)__udivdi3(args[1], 0x14);
            if (t18 != HddInstallParam->unmodeled_00[4] && t18 < 5) {
                t16 = *(int *)HddInstallParam->unmodeled_00;
                memset(jp, 0, 0x20);
                HddInstallParam->unmodeled_00[4] = (u8)t18;
                jp[0] = t16 + *(int *)(t18 * 4 + t16) + 0x40;
                jp[1] = *(volatile int *)&HddInstallParam->transfer_buffer;
                xglJpegDecode(jp);
            }
            if (PadData.trigger & 0x40) {
                xglSoundEffectNormalDirect(2);
                HddInstallSync();
                for (;;) {
                    int trig;
                    xglFontPrint(0x20, 0x10, -1, D_00A11738);
                    trig = PadData.trigger;
                    if (trig & 0x20) {
                        xglSoundEffectNormalDirect(1);
                        *arg2 = 1;
                        return 1;
                    }
                    if (trig & 0x40) {
                        xglSoundEffectNormalDirect(2);
                        if (xglHddCheck() < 0) {
                            *arg2 = 2;
                            return 2;
                        }
                        goto done;
                    }
                    HddInstallSync();
                }
            }
            goto done;
        }
    case 7:
        switch (arg1) {
        case 0:
            xglFontDebugPrintf(0x54, 0xD0, D_00A117A0);
            goto done;
        case 1:
            xglFontDebugPrintf(0x54, 0xD0, D_00A117B8);
            goto done;
        case 2:
            xglFontDebugPrintf(0x54, 0xD0, D_00A117C8);
            goto done;
        case 3:
            xglFontDebugPrintf(0x54, 0xD0, D_00A117D8);
            goto done;
        case 4:
            xglFontDebugPrintf(0x54, 0xD0, D_00A117F0);
            goto done;
        default:
            goto done;
        }
    case 8:
        xglFontDebugPrintf(0x54, 0xD0, D_00A11800);
        goto done;
    case -15:
    case -14:
    case -13:
    case -12:
    case -11:
    case -10:
    case -9:
    case -8:
    case -7:
    case -6:
    case -5:
    case -4:
    case -3:
    case -2:
    case -1:
        printf(D_00A11818, arg0, arg1, arg1);
        xglFontDebugPrintf(0x54, 0xD0, D_00A11828, arg0, arg1);
        goto done;
    default:
        goto done;
    }
done:
    HddInstallSync();
    return 0;
}

extern const char D_00A118B8[];
extern const char D_00A11928[];
extern const char D_00A11970[];
extern void xglSoundEffectNormalDirect(int id);
extern int xglHddCheck(void);
extern int xglHddInstall(void *cb, void *param);
extern int HddInstallCB();

static void TitleHddInstallSubInstall(void)
{
  int param[4];
  struct TitlePadData **new_var;
  int ret;
  int wait;
  struct TitlePadData *pad;
  for (;;)
  {
    wait = 4;
    param[0] = 0;
    for (;;)
    {
      xglFontPrint(0x20, 0x10, -1, D_00A118B8);
      HddInstallSync();
      if (wait > 0)
      {
        wait--;
        continue;
      }
      if (PadData.trigger & 0x20)
      {
        break;
      }
      if (PadData.trigger & 0x40)
      {
        xglSoundEffectNormalDirect(2);
        return;
      }
    }

    xglSoundEffectNormalDirect(1);
    ret = xglHddCheck();
    if (ret == 0)
    {
      ret = xglHddInstall(HddInstallCB, param);
    }
    if (ret == 1)
    {
      HddInstallSync();
      return;
    }
    if (ret == 0)
    {
      xglHddActivate(1);
      ret = xglHddActivate(-1);
    }
    else
    {
      if (!PadData.trigger)
      {
      }
      ret = 0;
    }
    wait = 4;
    HddInstallSync();
    if (ret == 1)
    {
      pad = &PadData;
      for (;;)
      {
        xglFontPrint(0x20, 0x10, -1, D_00A11928);
        pad = &PadData;
        HddInstallSync();
        new_var = &pad;
        if (wait > 0)
        {
          wait--;
          continue;
        }
        if ((*new_var)->trigger & 0x20)
        {
          break;
        }
      }

      xglSoundEffectNormalDirect(1);
      return;
    }
    for (;;)
    {
      xglFontPrint(0x20, 0x10, -1, D_00A11970);
      HddInstallSync();
      if (wait > 0)
      {
        wait--;
        continue;
      }
      if (PadData.trigger & 0x20)
      {
        xglSoundEffectNormalDirect(1);
        return;
      }
      if (PadData.trigger & 0x40)
      {
        break;
      }
    }

    xglSoundEffectNormalDirect(2);
    HddInstallSync();
  }

}

extern const char D_00A119E0[];
extern const char D_00A11A58[];
extern const char D_00A11AA8[];
extern void xglFontPrint(int x, int y, int color, const char *text);
extern int xglHddCheck2(void);
extern int xglHddUninstall(void);

static void TitleHddInstallSubUninstall(void)
{
  int result;
  char *new_var;
  int wait;
  struct TitlePadData *pad;
  pad = &PadData;
  for (;;)
  {
    wait = 4;
    for (;;)
    {
      xglFontPrint(0x20, 0x10, -1, D_00A119E0);
      HddInstallSync();
      if (wait > 0)
      {
        wait--;
        continue;
      }
      if (pad->trigger & 0x20)
      {
        break;
      }
      if (pad->trigger & 0x40)
      {
        xglSoundEffectNormalDirect(2);
        return;
      }
    }

    xglSoundEffectNormalDirect(1);
    result = xglHddCheck2();
    if (result == 0)
    {
      result = xglHddUninstall();
    }
    wait = 4;
    HddInstallSync();
    if (result == 0)
    {
      struct TitlePadData *p2 = &PadData;
      for (;;)
      {
        xglFontPrint(0x20, 0x10, -1, D_00A11A58);
        if ((p2 && (&PadData)) && (&PadData))
        {
        }
        HddInstallSync();
        if (wait > 0)
        {
          wait--;
          continue;
        }
        if ((&PadData)->trigger & 0x20)
        {
          break;
        }
      }

      xglSoundEffectNormalDirect(1);
      return;
    }
    else
    {
      for (;;)
      {
        xglFontPrint(0x20, 0x10, -1, D_00A11AA8);
        new_var = (char *) (&PadData);
        HddInstallSync();
        if (wait > 0)
        {
          wait--;
          continue;
        }
        if ((*((u16 *) (new_var + 0x2A))) & 0x20)
        {
          xglSoundEffectNormalDirect(1);
          xglHddCheck();
          return;
        }
        if ((*((u16 *) (new_var + 0x2A))) & 0x40)
        {
          xglSoundEffectNormalDirect(2);
          break;
        }
      }

    }
    wait = 4;
    HddInstallSync();
  }

}

extern const char *sztmp_6[];
extern int WorkEnd_w[] __asm__("WorkEnd");
extern int xglJpegDecode(void *request_address);
extern const char D_00A11C60[];
extern const char D_00A11C90[];
extern const char D_00A11CD8[];

void TitleHddInstall(u32 arg0)
{
    struct HddInstallParams hp;
    struct { u8 *in; u8 *out; u8 pad[0x18]; } jpeg;
    u8 *base;
    int out;
    int saved;
    int mode;
    u8 **pw;
    int n;

    mode = arg0;
    saved = WorkEnd_w[0];
    if (mode == 0) {
        if (xglHddCheck() != 0) {
            mode = 2;
        }
    } else if ((mode == 1) && (xglHddCheck2() != 0)) {
        mode = 5;
        xglHddActivate(0);
    }
    pw = (u8 **)WorkEnd_w;
    base = *pw;
    n = xglCdReadFile("data\\yajima\\hddi.bin", base, 0, 1);
    out = ((int)(base + n) + 0x3F) & ~0x3F;
    *pw = (u8 *)(out + 0xE0000);
    *(u8 **)hp.unmodeled_00 = base;
    hp.unmodeled_00[4] = 0;
    hp.transfer_buffer = (u8 *)out;
    memset(&jpeg, 0, 0x20);
    jpeg.in = base + 0x40;
    jpeg.out = (u8 *)out;
    xglJpegDecode(&jpeg);
    HddInstallParam = &hp;
    HddInstallSync();
    xglFontPrintDirectOT(-1, D_00A11620);
    switch (mode) {
    case 0:
        TitleHddInstallSubInstall();
        break;
    case 1:
        TitleHddInstallSubUninstall();
        break;
    case 2:
    case 3:
    case 4:
        n = 4;
        for (;;) {
            xglFontPrint(0x20, 0x10, -1, sztmp_6[mode]);
            xglFontPrintDirectOT(-1, D_00A11C90);
            HddInstallSync();
            if (n > 0) {
                n--;
                continue;
            }
            if (PadData.trigger & 0x20) {
                break;
            }
        }
        xglSoundEffectNormalDirect(1);
        break;
    case 5:
        n = 4;
        for (;;) {
            xglFontPrint(0x20, 0x10, -1, D_00A11C60);
            xglFontPrintDirectOT(-1, D_00A11CD8);
            HddInstallSync();
            if (n > 0) {
                n--;
                continue;
            }
            if (PadData.trigger & 0x20) {
                break;
            }
        }
        xglSoundEffectNormalDirect(1);
        xglHddActivate(0x103);
        xglHddCheck();
        break;
    }
    HddInstallSync();
    WorkEnd_w[0] = saved;
}

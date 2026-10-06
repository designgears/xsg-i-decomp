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

INCLUDE_ASM("asm/nonmatchings/ov02/title_hdd_install", HddInstallCB);

struct TitlePadData {
    u8 unk00[0x2A];
    u16 trigger;
};
extern struct TitlePadData PadData;
extern const char D_00A118B8[];
extern const char D_00A11928[];
extern const char D_00A11970[];
extern void xglSoundEffectNormalDirect(int id);
extern int xglHddCheck(void);
extern int xglHddInstall(void *cb, void *param);
extern void HddInstallCB();

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

INCLUDE_ASM("asm/nonmatchings/ov02/title_hdd_install", TitleHddInstallSubUninstall);

INCLUDE_ASM("asm/nonmatchings/ov02/title_hdd_install", TitleHddInstall);

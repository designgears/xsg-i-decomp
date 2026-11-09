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

static struct HddInstallParams *HddInstallParam;
extern struct HddInstallRenderState sRender;

extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkAddDataN(XglPacket *packet, const void *data, int count);
extern void xglFontPrintDirectOT(int color, const char *text);

static void HddInstallSync(void)
{
    /* GS register packets (GIF A+D tags); TestEnv's word 4 is the FRAME
     * register, rewritten every call with the current draw buffer. */
    static u64 TestEnv[12] __attribute__((aligned(16))) = {
        0, 0x5100000500000000ULL,
        0x1000000000000004ULL, 0xE,
        0, 0x50,
        0, 0x51,
        0x000001C000000200ULL, 0x52,
        0, 0x53,
    };
    static u64 TransEnv[4] __attribute__((aligned(16))) = {
        0, 0x5100000100000000ULL,
        0x0800000000007000ULL, 0,
    };
    static u64 FlushEnv[6] __attribute__((aligned(16))) = {
        0, 0x5100000200000000ULL,
        0x1000000000008001ULL, 0xE,
        0, 0x3F,
    };
    XglPacket *packet;
    u8 *transfer_buffer;

    packet = xglPacketGetCurrent();
    transfer_buffer = HddInstallParam->transfer_buffer;
    TestEnv[4] = ((u64)sRender.buffer_select << 37) | ((u64)0x8000 << 36);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, TestEnv, 0x18);
    sceVif1PkRef(packet, TransEnv, 2, 0, 0, 0);
    sceVif1PkRef(packet, transfer_buffer, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, TransEnv, 2, 0, 0, 0);
    sceVif1PkRef(packet, transfer_buffer + 0x70000, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, FlushEnv, 3, 0, 0, 0);
    xglSleep();
    xglFontPrintDirectOT(-1, "\017D\000\200\015\003");
}

struct TitlePadData {
    u8 unk00[0x2A];
    u16 trigger;
};

extern struct TitlePadData PadData;

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
    xglFontPrint(0x20, 0x10, -1, "\031\001\245\244\245\363\245\271\245\310\241\274\245\353\303\346\244\307\244\271\241\243\241\310\243\320\243\354\243\341\243\371\243\323\243\364\243\341\243\364\243\351\243\357\243\356\010\210\000\010\243\262\241\311\313\334\302\316\244\316\n\n\305\305\270\273\244\362\300\332\244\351\244\312\244\244\244\307\244\257\244\300\244\265\244\244\241\243");
    switch (arg0) {
    case 0:
        xglFontDebugPrintf(0x54, 0xD0, "Install Start");
        goto done;
    case 1:
        xglFontDebugPrintf(0x54, 0xD0, "Create Partition");
        goto done;
    case 2:
        xglFontDebugPrintf(0x54, 0xD0, "Write Resource");
        goto done;
    case 3:
        xglFontDebugPrintf(0x54, 0xD0, "Create Sub Partition %1d", arg1);
        goto done;
    case 4:
        xglFontDebugPrintf(0x54, 0xD0, "Format Partition");
        goto done;
    case 5:
        xglFontDebugPrintf(0x54, 0xD0, "Mount Partition");
        goto done;
    case 6:
        if (arg1 == 0) {
            xglFontDebugPrintf(0x54, 0xD0, "Start Install");
            goto done;
        }
        {
            static u8 frame;
            static char anim[4] = { '-', 0x7F, '|', '/' };
            static char sztmp[] = "%c";

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
            xglFontPrint(0x20, 0x50, -1, "\031\000\014  \200\241\337\014\200\200\200\245\334\245\277\245\363\241\247\303\346\303\307");
            args[0] = (u64)(u32)sztmp;
            args[1] = (u64)(u32)(int)(signed char)anim[(frame >> 3) & 3];
            xglFontPrintf(0x98, 0x19E, -1, args);
            frame++;
            {
                static char sztmp[] = "%3d\033\022";

                args[0] = (u64)(u32)sztmp;
                args[1] = (u64)(long long)(((arg1 & 0xFFFF) * 100) / (arg1 >> 16));
                xglFontPrintf(0xA8, 0x190, -1, args);
            }
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
                    xglFontPrint(0x20, 0x10, -1, "\245\244\245\363\245\271\245\310\241\274\245\353\244\317\244\336\244\300\264\260\316\273\244\267\244\306\244\244\244\336\244\273\244\363\241\243\n\n\303\346\303\307\244\267\244\336\244\271\244\253\241\251\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\303\346\303\307\241\277\014  \200\241\337\014\200\200\200\245\334\245\277\245\363\241\247\267\321\302\263");
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
            xglFontDebugPrintf(0x54, 0xD0, "CheckFile Create");
            goto done;
        case 1:
            xglFontDebugPrintf(0x54, 0xD0, "CheckFile Write");
            goto done;
        case 2:
            xglFontDebugPrintf(0x54, 0xD0, "CheckFile Close");
            goto done;
        case 3:
            xglFontDebugPrintf(0x54, 0xD0, "CheckFile Unmount");
            goto done;
        case 4:
            xglFontDebugPrintf(0x54, 0xD0, "CheckFile Mount");
            goto done;
        default:
            goto done;
        }
    case 8:
        xglFontDebugPrintf(0x54, 0xD0, "Install Complete");
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
        printf("%d:%d(%08x)\n", arg0, arg1, arg1);
        xglFontDebugPrintf(0x54, 0xD0, "Error %2d/%8x", arg0, arg1);
        goto done;
    default:
        goto done;
    }
done:
    HddInstallSync();
    return 0;
}

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
      xglFontPrint(0x20, 0x10, -1, "\245\274\245\316\245\265\241\274\245\254\244\316\245\244\245\363\245\271\245\310\241\274\245\353\244\362\263\253\273\317\244\267\244\336\244\271\241\243\n\n\244\350\244\355\244\267\244\244\244\307\244\271\244\253\241\251\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\263\253\273\317\241\277\014  \200\241\337\014\200\200\200\245\334\245\277\245\363\241\247\245\255\245\343\245\363\245\273\245\353");
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
        xglFontPrint(0x20, 0x10, -1, "\245\244\245\363\245\271\245\310\241\274\245\353\244\254\300\265\276\357\244\313\275\252\316\273\244\267\244\336\244\267\244\277\241\243\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\245\277\245\244\245\310\245\353\244\313\314\341\244\353");
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
      xglFontPrint(0x20, 0x10, -1, "\245\244\245\363\245\271\245\310\241\274\245\353\244\313\274\272\307\324\244\267\244\336\244\267\244\277\241\243\n\n\245\244\245\363\245\271\245\310\241\274\245\353\244\362\303\346\273\337\244\267\244\336\244\271\244\253\241\251\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\244\317\244\244\241\277\014  \200\241\337\014\200\200\200\245\334\245\277\245\363\241\247\244\244\244\244\244\250");
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
      xglFontPrint(0x20, 0x10, -1, "\245\274\245\316\245\265\241\274\245\254\244\316\245\242\245\363\245\244\245\363\245\271\245\310\241\274\245\353\244\362\263\253\273\317\244\267\244\336\244\271\241\243\n\n\244\350\244\355\244\267\244\244\244\307\244\271\244\253\241\251\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\263\253\273\317\241\277\014  \200\241\337\014\200\200\200\245\334\245\277\245\363\241\247\245\255\245\343\245\363\245\273\245\353");
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
        xglFontPrint(0x20, 0x10, -1, "\245\242\245\363\245\244\245\363\245\271\245\310\241\274\245\353\244\254\300\265\276\357\244\313\275\252\316\273\244\267\244\336\244\267\244\277\241\243\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\245\277\245\244\245\310\245\353\244\313\314\341\244\353");
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
        xglFontPrint(0x20, 0x10, -1, "\245\242\245\363\245\244\245\363\245\271\245\310\241\274\245\353\244\313\274\272\307\324\244\267\244\336\244\267\244\277\241\243\n\n\245\242\245\363\245\244\245\363\245\271\245\310\241\274\245\353\244\362\303\346\273\337\244\267\244\336\244\271\244\253\241\251\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\244\317\244\244\241\277\014  \200\241\337\014\200\200\200\245\334\245\277\245\363\241\247\244\244\244\244\244\250");
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

extern int WorkEnd_w[] __asm__("WorkEnd");
extern int xglJpegDecode(void *request_address);

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
    xglFontPrintDirectOT(-1, "\017D\000\200\015\003");
    switch (mode) {
    case 0:
        TitleHddInstallSubInstall();
        break;
    case 1:
        TitleHddInstallSubUninstall();
        break;
    case 2:
    case 3:
    case 4: {
        static char *mes[] = {
            "\245\317\241\274\245\311\245\307\245\243\245\271\245\257\245\311\245\351\245\244\245\326\244\313\245\242\245\257\245\273\245\271\244\307\244\255\244\336\244\273\244\363\241\243",
            "\245\317\241\274\245\311\245\307\245\243\245\271\245\257\245\311\245\351\245\244\245\326\244\254\245\325\245\251\241\274\245\336\245\303\245\310\244\265\244\354\244\306\n\n\244\244\244\336\244\273\244\363\241\243",
            "\245\317\241\274\245\311\245\307\245\243\245\271\245\257\245\311\245\351\245\244\245\326\244\313\241\242\245\274\245\316\245\265\241\274\245\254\244\316\n\n\245\244\245\363\245\271\245\310\241\274\245\353\244\313\311\254\315\327\244\312\266\365\244\255\315\306\316\314\244\254\302\255\244\352\244\336\244\273\244\363\241\243\n\n\010\210\377\370\241\312\266\365\244\255\315\306\316\314\244\310\244\267\244\306\031\001\243\261\243\267\243\271\243\262\243\315\243\302\031\000\260\312\276\345\311\254\315\327\244\307\244\271\241\243\010\210\377\370\241\313\n\n\n\n\302\276\244\316\245\242\245\327\245\352\245\261\241\274\245\267\245\347\245\363\244\362\245\242\245\363\245\244\245\363\245\271\245\310\241\274\245\353\244\271\244\353\244\310\241\242\n\n\245\244\245\363\245\271\245\310\241\274\245\353\244\307\244\255\244\353\244\350\244\246\244\313\244\312\244\353\262\304\307\275\300\255\244\254\244\242\244\352\244\336\244\271\241\243",
        };

        n = 4;
        for (;;) {
            xglFontPrint(0x20, 0x10, -1, mes[mode - 2]);
            xglFontPrintDirectOT(-1, "\n\n\n\n\245\244\245\363\245\271\245\310\241\274\245\353\244\362\303\346\273\337\244\267\244\336\244\271\241\243\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\245\277\245\244\245\310\245\353\244\313\314\341\244\353");
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
    }
    case 5:
        n = 4;
        for (;;) {
            xglFontPrint(0x20, 0x10, -1, "\245\317\241\274\245\311\245\307\245\243\245\271\245\257\245\311\245\351\245\244\245\326\244\313\245\242\245\257\245\273\245\271\244\307\244\255\244\336\244\273\244\363\241\243");
            xglFontPrintDirectOT(-1, "\n\n\n\n\245\242\245\363\245\244\245\363\245\271\245\310\241\274\245\353\244\362\303\346\273\337\244\267\244\336\244\271\241\243\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\245\277\245\244\245\310\245\353\244\313\314\341\244\353");
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

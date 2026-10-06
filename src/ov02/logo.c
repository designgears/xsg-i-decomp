/*
 * OV02 original TU 0: 0x00a00000..0x00a00850 (6 functions)
 */
#include "common.h"
#include "shared.h"
#include "main/xgl_thread.h"

/* Types recovered upstream (origin/master); kept for reference. */
typedef struct LogoGifTag {
    u32 control_low;
    u32 control_high;
    u32 registers_low;
    u32 registers_high;
} LogoGifTag;

typedef struct LogoGifAdCommand {
    u64 value;
    u32 register_address;
    u32 unused;
} LogoGifAdCommand;

typedef struct LogoGifRgbaq {
    u32 red;
    u32 green;
    u32 blue;
    u32 alpha;
} LogoGifRgbaq;

typedef struct LogoGifUv {
    u32 u;
    u32 v;
    u32 unused_08;
    u32 unused_0c;
} LogoGifUv;

typedef struct LogoGifXyz2 {
    u16 x;
    u16 unused_x;
    u16 y;
    u16 unused_y;
    u32 z;
    u32 unused;
} LogoGifXyz2;


extern int xglMcMain(void);
extern void xglFontPrintDirectOT(int color, void *param);
extern int D_00A10D28;

static int device_check_sync(void)
{
    int status;

    status = xglMcMain();
    xglSleep();
    xglFontPrintDirectOT(-1, &D_00A10D28);
    return status;
}

extern int xglHddMount(void);
extern int xglHddActivate(int state);
typedef struct HddChkReq {
    int card;
    void *work;
    int size;
} HddChkReq;
typedef struct HddMsgTbl {
    char *p[3];
} HddMsgTbl;
extern int xglHddMcExist(HddChkReq *req);
extern int xglHddMcCheck(HddChkReq *req);
extern void xglFontPrint(int x, int y, int color, const char *text);
extern void xglSoundEffectNormalDirect(int effect_id);
extern PadPrefix PadData;
extern char D_00A10D30[];
extern char D_00A10E38[];
extern char D_00A10E98[];
extern HddMsgTbl D_00A11048;
extern HddMsgTbl D_00A11148;
extern char D_00A11158[];
extern char D_00A11250[];
extern char D_00A112F0[];
extern char D_00A11338[];
extern char carddata_0[];

static int hdd_check(void)
{
    HddMsgTbl msgs;
    HddMsgTbl infos;
    HddChkReq req;
    char *msg;
    char *info;
    int ret;
    void *work;

    ret = xglHddMount();
    if (ret != -1) {
        if (ret == -99)
            return 1;
        goto check5;
    loop5:
        device_check_sync();
    check5:
        if (ret == -5) {
            xglFontPrint(0x10, 0x40, -1, D_00A10D30);
            if (!(PadData.half_2a & 0x20))
                goto loop5;
            xglSoundEffectNormalDirect(1);
        }
        device_check_sync();
        if (ret == -3) {
            msg = D_00A10E38;
            info = D_00A10E98;
        } else {
            work = WorkEnd;
            msgs = D_00A11048;
            infos = D_00A11148;
            req.card = -1;
            req.work = work;
            if (xglHddMcExist(&req) > 0)
                return 0;
            req.size = 0x163B0;
            req.work = carddata_0;
            ret = xglHddMcCheck(&req);
            if (ret == -1) {
                for (;;) {
                    xglFontPrint(0x10, 0x40, -1, D_00A11158);
                    if (PadData.half_2a & 0x20)
                        break;
                    device_check_sync();
                }
                xglSoundEffectNormalDirect(1);
                device_check_sync();
                xglHddActivate(2);
                return 1;
            }
            if (ret < -1) {
                msg = msgs.p[-ret - 2];
                info = infos.p[-ret - 2];
            } else
                return 0;
        }
        for (;;) {
            xglFontPrint(0x10, 0x40, -1, msg);
            xglFontPrintDirectOT(-1, D_00A11250);
            if (PadData.half_2a & 0x20) {
                xglSoundEffectNormalDirect(1);
                break;
            }
            if (PadData.half_2a & 0x40) {
                xglSoundEffectNormalDirect(2);
                device_check_sync();
                while (xglFontPrint(0x10, 0x40, -1, D_00A112F0),
                       xglFontPrintDirectOT(-1, info),
                       xglFontPrintDirectOT(-1, D_00A11338),
                       !(PadData.half_2a & 0x20))
                    device_check_sync();
                xglSoundEffectNormalDirect(1);
                break;
            }
            device_check_sync();
        }
        device_check_sync();
    }
    return 1;
}

extern int xglMcRequest(int port, int type, void *param, int *result);
extern void xglFontPrint(int x, int y, int color, const char *text);
extern void xglSoundEffectNormalDirect(int effect_id);
extern PadPrefix PadData;
extern char D_00A11358[];
extern char D_00A113C8[];
extern char D_00A113E8[];
extern char D_00A11430[];
extern char D_00A11478[];
extern char D_00A114C8[];
extern char D_00A11520[];
typedef union McCheckStat {
    int i;
    s16 s[2];
} McCheckStat;
typedef struct McCheckReq {
    int dir;
    int pad;
    int work;
} McCheckReq;

static void mc_check(void)
{
    s16 st[2];
    int free[2];
    McCheckReq req;
    int files[2];
    s16 st2[2];
    char *msg;
    char *info;
    int err;
    int i;

    i = 7;
    do {
        i--;
        ((McCheckStat *)st)->i = device_check_sync();
    } while (i >= 0);
    free[0] = 0;
    msg = 0;
    free[1] = 0;
    files[0] = 0;
    files[1] = 0;
    do {
        xglMcRequest(0, 1, 0, &free[0]);
        xglMcRequest(1, 1, 0, &free[1]);
        req.dir = -1;
        req.work = (int)WorkEnd;
        xglMcRequest(0, 2, &req, &files[0]);
        xglMcRequest(1, 2, &req, &files[1]);
        do {
            ((McCheckStat *)st)->i = device_check_sync();
        } while (st[0] == 0xFF || st[0] == 0x80 || st[1] == 0xFF || st[1] == 0x80);
        i = 7;
        do {
            i--;
            ((McCheckStat *)st)->i = device_check_sync();
        } while (i >= 0);
        info = D_00A11358;
        if (st[0] == 1 || st[0] == 2 || st[1] == 1 || st[1] == 2) {
            goto ok;
        }
        msg = D_00A113C8;
        goto print;
    slot2:
        if (st[1] == 1) {
            if (files[1] > 0) {
                goto none;
            }
            err |= 2;
            if (free[1] >= 0xA5) {
                goto none;
            }
        }
        goto check;
    ok:
        err = 0;
        if (st[0] != 2 && st[1] != 2) {
            if (st[0] == 1) {
                if (files[0] > 0 || free[0] >= 0xA5) {
                    goto check;
                }
                err = 1;
            }
            goto slot2;
        }
    none:
        err = 0;
        check:
            switch (err) {
            case 0:
                msg = 0;
                break;
            case 1:
                msg = D_00A113E8;
                break;
            case 2:
                msg = D_00A11430;
                break;
            case 3:
                msg = D_00A11478;
                break;
            }
    print:
        while (msg != 0) {
            xglFontPrint(0x10, 0x40, -1, msg);
            if (info != 0) {
                xglFontPrintDirectOT(-1, info);
            }
            xglFontPrintDirectOT(-1, D_00A114C8);
            if (PadData.half_2a & 0x20) {
                msg = 0;
                xglSoundEffectNormalDirect(1);
                break;
            }
            if (PadData.half_2a & 0x40) {
                xglSoundEffectNormalDirect(2);
                do {
                    *(int *)st2 = device_check_sync();
                    xglFontPrint(0x10, 0x40, -1, D_00A11520);
                    if (PadData.half_2a & 0x40) {
                        xglSoundEffectNormalDirect(1);
                        break;
                    }
                } while (*(int *)st == *(int *)st2);
                break;
            }
            *(int *)st2 = device_check_sync();
            if (*(int *)st != *(int *)st2) {
                break;
            }
        }
    } while (msg != 0);
    device_check_sync();
}

extern void xglRenderClearColor(u32 color);
extern void xglRenderClearFrame(void);
extern void xglRenderClearOff(void);
extern int D_00A115A0;
static int hdd_check(void);
void Logo(void);

void LogoFirst(void)
{
    xglRenderClearFrame();
    xglRenderClearColor(0x80000000);
    xglSleep();
    xglRenderClearOff();
    if (hdd_check())
    {
        mc_check();
    }
    xglFontPrintDirectOT(-1, &D_00A115A0);
    Logo();
}

typedef struct LogoMovieInfo {
    u8 unmodeled_00[0x48];
    u32 frame_width;
    u8 unmodeled_4c[0x44];
    u32 stream_base;
    u32 stream_size;
    u8 unmodeled_98[0x18];
    u32 display_base;
    u32 display_end;
    u8 unmodeled_b8[0x11];
    u8 stopped;
    u8 unmodeled_ca[6];
} LogoMovieInfo;
typedef struct LogoTestEnvironment {
    u8 unmodeled_00[0x30];
    u64 command;
} LogoTestEnvironment;
typedef struct LogoRenderState {
    u8 unmodeled_00[0x14];
    u16 display_buffer;
} LogoRenderState;
extern LogoTestEnvironment TestEnv_1_00A0F690;
extern LogoRenderState sRender;
extern void xglSoundEffectNormalDirect(int effect_id);
extern PadPrefix PadData;
extern void xglMovieInfoInit(LogoMovieInfo *movie);
extern int xglMovieOpen(LogoMovieInfo *movie, const char *path);
extern int xglMoviePlay(LogoMovieInfo *movie);
extern int xglMovieClose(LogoMovieInfo *movie);
extern float xglFRand(void);
extern void FlushCache(int mode);
extern void xglDmaDirectNormal(u32 channel, u32 address, u32 count);

static int ipuplay(char *path)
{
    LogoMovieInfo movie;
    int skipped;
    u32 frame_buffer_base;

    skipped = 0;
    xglMovieInfoInit(&movie);
    frame_buffer_base = 0x01000000;
    movie.stream_base = frame_buffer_base;
    movie.stream_size = 0x80000;
    movie.stopped = 0;
    frame_buffer_base = 0x01100000;
    xglMovieOpen(&movie, path);
    movie.display_base = frame_buffer_base;
    frame_buffer_base += movie.frame_width * 16;
    movie.display_end = frame_buffer_base;

    for (;;)
    {
        xglSRand();
        xglFRand();

        if (PadData.half_2a & 0x800)
        {
            skipped = 1;
            xglSoundEffectNormalDirect(1);
            break;
        }

        if (xglMoviePlay(&movie) != 0)
        {
            break;
        }

        /* The display-buffer value occupies bits 5 and above of the low command word. */
        TestEnv_1_00A0F690.command = 0x2000000640000000ULL |
                                     (u64)~(~0x24020000U & ~((u32)sRender.display_buffer << 5));
        FlushCache(0);
        xglDmaDirectNormal(2, (u32)&TestEnv_1_00A0F690, 9);
        xglSleep();
    }

    xglMovieClose(&movie);
    xglSleep();
    return skipped;
}

static int ipuplay(char *path);
extern void xglRenderClearColor(u32 color);
extern void xglRenderClearDepth(void);
extern void xglRenderClearFrame(void);
extern char D_00A115A8[];
extern char D_00A115B8[];

void Logo(void)
{
    xglRenderClearFrame();
    xglRenderClearColor(0x80000000);
    xglSleep();
    xglRenderClearDepth();
    if (ipuplay(D_00A115A8) == 0)
    {
        /*
         * The original calls ipuplay with jal and returns through the
         * shared epilogue instead of a sibling jump. Under this TU's
         * compiler the call stays out of tail position only inside a
         * loop construct, which is the shape a do/while (0) statement
         * gives it; the call alone as the block's last statement
         * compiles to `j ipuplay`.
         */
        do {
            ipuplay(D_00A115B8);
        } while (0);
    }
}

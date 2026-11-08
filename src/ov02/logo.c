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

static int device_check_sync(void)
{
    int status;

    status = xglMcMain();
    xglSleep();
    xglFontPrintDirectOT(-1, "\x19\x03");
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

static int hdd_check(void)
{
    static int carddata[4] = { 8, 0x400, 0x11AC0, 0 };
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
            xglFontPrint(0x10, 0x40, -1, "\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xA4\xCB\xA4\xA2\xA4\xEB\xA5\xBC\xA5\xCE\xA5\xB5\xA1\xBC\xA5\xAC\xA4\xCE\n\n\xA5\xA2\xA5\xD7\xA5\xEA\xA5\xB1\xA1\xBC\xA5\xB7\xA5\xE7\xA5\xF3\xA5\xC7\xA1\xBC\xA5\xBF\xA4\xCF\xC7\xCB\xC2\xBB\xA4\xB7\xA4\xC6\xA4\xA4\xA4\xDE\xA4\xB9\xA1\xA3\n\n\n\n\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xA4\xF2\xBB\xC8\xCD\xD1\xA4\xB7\xA4\xC6\xA5\xD7\xA5\xEC\xA5\xA4\xA4\xB9\xA4\xEB\xBE\xEC\xB9\xE7\xA1\xA2\n\n\xB0\xEC\xC5\xD9\xA5\xBC\xA5\xCE\xA5\xB5\xA1\xBC\xA5\xAC\xA4\xCE\xA5\xA2\xA5\xD7\xA5\xEA\xA5\xB1\xA1\xBC\xA5\xB7\xA5\xE7\xA5\xF3\xA5\xC7\xA1\xBC\xA5\xBF\xA4\xF2\n\n\xA5\xA2\xA5\xF3\xA5\xA4\xA5\xF3\xA5\xB9\xA5\xC8\xA1\xBC\xA5\xEB\xA4\xB7\xA4\xC6\xA4\xAB\xA4\xE9\xA5\xA4\xA5\xF3\xA5\xB9\xA5\xC8\xA1\xBC\xA5\xEB\xA4\xB7\xC4\xBE\xA4\xB7\xA4\xC6\n\n\xB2\xBC\xA4\xB5\xA4\xA4\xA1\xA3\n\n\n\n\x0C\x80  \xA1\xFB\x0C\x80\x80\x80\xA5\xDC\xA5\xBF\xA5\xF3\xA1\xA7\xC2\xB3\xB9\xD4");
            if (!(PadData.half_2a & 0x20))
                goto loop5;
            xglSoundEffectNormalDirect(1);
        }
        device_check_sync();
        if (ret == -3) {
            msg = "\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xA4\xAC\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8\xA4\xB5\xA4\xEC\xA4\xC6\x0A\x0A\xA4\xA4\xA4\xCA\xA4\xA4\xA4\xCE\xA4\xC7\xA5\xB2\xA1\xBC\xA5\xE0\xA4\xCE\xC5\xD3\xC3\xE6\xB7\xD0\xB2\xE1\xA4\xF2\xA5\xBB\xA1\xBC\xA5\xD6\xA4\xC7\xA4\xAD\xA4\xDE\xA4\xBB\xA4\xF3\xA1\xA3";
            info = "\xB0\xEC\xC5\xD9\xA5\xB2\xA1\xBC\xA5\xE0\xA4\xF2\xC8\xB4\xA4\xB1\xA4\xC6\x0A\x0A\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xA4\xCE\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8\xA4\xF2\xB9\xD4\xA4\xC3\xA4\xC6\x0A\x0A\xB2\xBC\xA4\xB5\xA4\xA4\xA1\xA3";
        } else {
            work = WorkEnd;
            msgs = (HddMsgTbl){ {
                    "\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xA4\xCB\xBA\xEE\xC0\xAE\xB2\xC4\xC7\xBD\xA4\xCA\xA5\xD5\xA5\xA9\xA5\xEB\xA5\xC0\xBF\xF4\n\n\xA4\xCE\xBE\xE5\xB8\xC2\xA4\xF2\xC4\xB6\xA4\xA8\xA4\xC6\xA4\xA4\xA4\xDE\xA4\xB9\xA1\xA3\xA4\xB3\xA4\xEC\xB0\xCA\xBE\xE5\xA4\xCE\xA5\xD5\xA5\xA9\xA5\xEB\xA5\xC0\xA4\xF2\n\n\xBA\xEE\xC0\xAE\xA4\xC7\xA4\xAD\xA4\xDE\xA4\xBB\xA4\xF3\xA1\xA3",
                    "\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xA4\xCB\xA5\xBB\xA1\xBC\xA5\xD6\xB2\xC4\xC7\xBD\xA4\xCA\xB8\xC4\xBF\xF4\xA4\xCE\n\n\xBE\xE5\xB8\xC2\xA4\xF2\xC4\xB6\xA4\xA8\xA4\xC6\xA4\xA4\xA4\xDE\xA4\xB9\xA1\xA3\xA4\xB3\xA4\xEC\xB0\xCA\xBE\xE5\xA4\xCE\xA5\xBB\xA1\xBC\xA5\xD6\xA5\xC7\xA1\xBC\xA5\xBF\xA4\xF2\n\n\xBA\xEE\xC0\xAE\xA4\xC7\xA4\xAD\xA4\xDE\xA4\xBB\xA4\xF3\xA1\xA3",
                    "\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xA4\xCB\xA5\xBC\xA5\xCE\xA5\xB5\xA1\xBC\xA5\xAC\xA4\xCE\xA5\xBB\xA1\xBC\xA5\xD6\xA4\xCB\n\n\xC9\xAC\xCD\xD7\xA4\xCA\xB6\xF5\xA4\xAD\xCE\xCE\xB0\xE8\xA4\xAC\xC9\xD4\xC2\xAD\xA4\xB7\xA4\xC6\xA4\xA4\xA4\xDE\xA4\xB9\xA1\xA3\n\n\xA1\xCA\xB6\xF5\xA4\xAD\xCD\xC6\xCE\xCC\xA4\xC8\xA4\xB7\xA4\xC6\xA3\xB2\xA3\xB2\xA3\xB5\xA3\xCB\xA3\xC2\xB0\xCA\xBE\xE5\xC9\xAC\xCD\xD7\xA4\xC7\xA4\xB9\xA1\xA3\xA1\xCB",
            } };
            infos = (HddMsgTbl){ {
                    "\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\n\n\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xC6\xE2\xA4\xCE\xC9\xD4\xCD\xD7\xA4\xCA\xA5\xD5\xA5\xA9\xA5\xEB\xA5\xC0\xA4\xF2\xBA\xEF\xBD\xFC\xA4\xB7\xA4\xC6\xB2\xBC\xA4\xB5\xA4\xA4\xA1\xA3",
                    "\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\n\n\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xC6\xE2\xA4\xCE\xC9\xD4\xCD\xD7\xA4\xCA\xA5\xBB\xA1\xBC\xA5\xD6\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB\xA4\xF2\xBA\xEF\xBD\xFC\xA4\xB7\xA4\xC6\n\n\xB2\xBC\xA4\xB5\xA4\xA4\xA1\xA3",
                    "\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\n\n\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xC6\xE2\xA4\xCE\xC9\xD4\xCD\xD7\xA4\xCA\xA5\xC7\xA1\xBC\xA5\xBF\xA4\xF2\xBA\xEF\xBD\xFC\xA4\xB7\xA4\xC6\xA1\xA2\xA3\xB2\xA3\xB2\xA3\xB5\xA3\xCB\xA3\xC2\n\n\xB0\xCA\xBE\xE5\xA4\xCE\xB6\xF5\xA4\xAD\xCD\xC6\xCE\xCC\xA4\xF2\xB3\xCE\xCA\xDD\xA4\xB7\xA4\xC6\xB2\xBC\xA4\xB5\xA4\xA4\xA1\xA3",
            } };
            req.card = -1;
            req.work = work;
            if (xglHddMcExist(&req) > 0)
                return 0;
            req.size = 0x163B0;
            req.work = carddata;
            ret = xglHddMcCheck(&req);
            if (ret == -1) {
                for (;;) {
                    xglFontPrint(0x10, 0x40, -1, "\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xA4\xCB\xA5\xA2\xA5\xAF\xA5\xBB\xA5\xB9\xA4\xC7\xA4\xAD\xA4\xDE\xA4\xBB\xA4\xF3\xA1\xA3\n\n\n\n\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xA4\xCB\xC9\xD5\xC2\xB0\xA4\xCE\xBC\xE8\xA4\xEA\xB0\xB7\xA4\xA4\xC0\xE2\xCC\xC0\xBD\xF1\xA4\xCE\n\n\xBB\xD8\xBC\xA8\xA4\xCB\xBD\xBE\xA4\xC3\xA4\xC6\xA1\xA2\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xA4\xCE\xBD\xA4\xC9\xFC\xA4\xF2\n\n\xB9\xD4\xA4\xC3\xA4\xC6\xB2\xBC\xA4\xB5\xA4\xA4\xA1\xA3\n\n\n\n\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xA4\xF2\xBB\xC8\xCD\xD1\xA4\xBB\xA4\xBA\xA4\xCB\xA5\xB2\xA1\xBC\xA5\xE0\xA4\xF2\n\n\xBB\xCF\xA4\xE1\xA4\xDE\xA4\xB9\xA1\xA3\n\n\n\n\x0C\x80  \xA1\xFB\x0C\x80\x80\x80\xA5\xDC\xA5\xBF\xA5\xF3\xA1\xA7\xC2\xB3\xB9\xD4");
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
            xglFontPrintDirectOT(-1, "\n\n\n\n\xA5\xB2\xA1\xBC\xA5\xE0\xA4\xCE\xC5\xD3\xC3\xE6\xB7\xD0\xB2\xE1\xA4\xCE\xA5\xBB\xA1\xBC\xA5\xD6\xA4\xCB\xA1\xA2\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\n\n\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xA4\xF2\xBB\xC8\xCD\xD1\xA4\xB7\xA4\xCA\xA4\xA4\xA4\xC7\xA5\xB2\xA1\xBC\xA5\xE0\xA4\xF2\xB3\xAB\xBB\xCF\xA4\xB7\xA4\xDE\xA4\xB9\xA4\xAB\xA1\xA9\n\n\n\n\x0C\x80  \xA1\xFB\x0C\x80\x80\x80\xA5\xDC\xA5\xBF\xA5\xF3\xA1\xA7\xBB\xC8\xCD\xD1\xA4\xB7\xA4\xCA\xA4\xA4\xA1\xBF\x0C  \x80\xA1\xDF\x0C\x80\x80\x80\xA5\xDC\xA5\xBF\xA5\xF3\xA1\xA7\xBB\xC8\xCD\xD1\xA4\xB9\xA4\xEB");
            if (PadData.half_2a & 0x20) {
                xglSoundEffectNormalDirect(1);
                break;
            }
            if (PadData.half_2a & 0x40) {
                xglSoundEffectNormalDirect(2);
                device_check_sync();
                while (xglFontPrint(0x10, 0x40, -1, "\xA5\xB2\xA1\xBC\xA5\xE0\xA4\xCE\xC5\xD3\xC3\xE6\xB7\xD0\xB2\xE1\xA4\xCE\xA5\xBB\xA1\xBC\xA5\xD6\xA4\xCB\xA5\xCF\xA1\xBC\xA5\xC9\xA5\xC7\xA5\xA3\xA5\xB9\xA5\xAF\x0A\x0A\xA5\xC9\xA5\xE9\xA5\xA4\xA5\xD6\xA4\xF2\xBB\xC8\xCD\xD1\xA4\xB9\xA4\xEB\xBE\xEC\xB9\xE7\xA4\xCF\xA1\xA2"),
                       xglFontPrintDirectOT(-1, info),
                       xglFontPrintDirectOT(-1, "\n\n\n\n\x0C\x80  \xA1\xFB\x0C\x80\x80\x80\xA5\xDC\xA5\xBF\xA5\xF3\xA1\xA7\xBF\xCA\xA4\xE0"),
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
        info = "\n\nThis game requires a Memory Card (PS2) with at least\n\n165KB of free space in order to save your progress.";
        if (st[0] == 1 || st[0] == 2 || st[1] == 1 || st[1] == 2) {
            goto ok;
        }
        msg = "No Memory Card (PS2) detected.";
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
                msg = "Insufficient space on the Memory Card (PS2)\n\nin MEMORY CARD slot 1.";
                break;
            case 2:
                msg = "Insufficient space on the Memory Card (PS2)\n\nin MEMORY CARD slot 2.";
                break;
            case 3:
                msg = "Insufficient space on the Memory Card (PS2)\n\nin MEMORY CARD slot 1 and slot 2.";
                break;
            }
    print:
        while (msg != 0) {
            xglFontPrint(0x10, 0x40, -1, msg);
            if (info != 0) {
                xglFontPrintDirectOT(-1, info);
            }
            xglFontPrintDirectOT(-1, "\n\nDo you wish to continue without saving?\n\n\x0C\x80  \xA1\xFB\x0C\x80\x80\x80 Button:YES / \x0C  \x80\xA1\xDF\x0C\x80\x80\x80 Button:NO");
            if (PadData.half_2a & 0x20) {
                msg = 0;
                xglSoundEffectNormalDirect(1);
                break;
            }
            if (PadData.half_2a & 0x40) {
                xglSoundEffectNormalDirect(2);
                do {
                    *(int *)st2 = device_check_sync();
                    xglFontPrint(0x10, 0x40, -1, "Please insert a Memory Card (PS2) with at least\n\n165KB available into MEMORY CARD slot 1 or slot 2.\n\n\n\n\x0C  \x80\xA1\xDF\x0C\x80\x80\x80""Button\xA1\xA7""Cancel");
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
    xglFontPrintDirectOT(-1, "\x19");
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
    u64 setup[6];      /* GIFtag + A+D register writes */
    u64 command;       /* +0x30 */
    u64 registers[11];
} LogoTestEnvironment;
typedef struct LogoRenderState {
    u8 unmodeled_00[0x14];
    u16 display_buffer;
} LogoRenderState;
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
    static LogoTestEnvironment TestEnv = {
        { 0x808B400000008001ULL, 0x53531EEE, 0x0, 0x3F, 0x7FFFF0, 0x8 },
        0x0,
        {
            0x6, 0x8000000080ULL, 0x8000000080ULL, 0x0, 0x0,
            0x71F700006FF8ULL, 0x40000000, 0x1C0000002000ULL, 0x0, 0x8DF700008FF8ULL, 0x40000000,
        },
    };
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
        TestEnv.command = 0x2000000640000000ULL |
                                     (u64)~(~0x24020000U & ~((u32)sRender.display_buffer << 5));
        FlushCache(0);
        xglDmaDirectNormal(2, (u32)&TestEnv, 9);
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

void Logo(void)
{
    xglRenderClearFrame();
    xglRenderClearColor(0x80000000);
    xglSleep();
    xglRenderClearDepth();
    if (ipuplay("data\\namco.ipu") == 0)
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
            ipuplay("data\\logo_msi.ipu");
        } while (0);
    }
}

/*
 * OV02 original TU 7: 0x00a03450..0x00a04170 (2 functions)
 */
#include "common.h"
#include "shared.h"

INCLUDE_ASM("asm/nonmatchings/ov02/task_umn_top_win", taskUmnTopWin);

extern unsigned char UmnWork[];
extern unsigned char D_4A1A0C[];
extern const char D_00A12050[];
extern int UmnWorkEnd;
extern unsigned char PadData[];
void taskUmnTopWin(void *, void *);
void UmnObjectTaskCreate(void (*task)(void *, void *), void *work);
void UmnChangeTopLevel(int level);
void xglSoundEffectNormalID(int sound_id, int variant);
void xglFontDebugPrintf(int x, int y, const char *format, ...);

void UmnTopMenu(void)
{
    int i;
    unsigned char *p;
    unsigned char *d;
    unsigned short t, t2, t3;
    unsigned char v;

    switch (UmnWork[3]) {
    case 0:
        UmnObjectTaskCreate(taskUmnTopWin, (void *)((UmnWorkEnd + 0xF) & ~0xF));
        d = D_4A1A0C;
        p = UmnWork + 0x23;
        i = 3;
        do {
            i--;
            *p-- = 0;
        } while (i >= 0);
        i = 4;
        t = *(unsigned short *)(d + 0x86);
        if (t & 8) {
            UmnWork[0x20] = 3;
        }
        UmnWork[0x21] = 1;
        if (t & 2) {
            UmnWork[0x22] = 2;
        }
        UmnWork[0x23] = i;
        UmnWork[0x30] = i;
        if (UmnWork[2] == 0) {
            for (i = 0; i < 4 && UmnWork[0x20 + i] == 0; i++) {
            }
            UmnWork[2] = i;
        }
        UmnWork[0x31] = UmnWork[2];
        UmnWork[3] = 0x20;
        UmnWork[0x10] = 8;
        /* fallthrough */
    case 0x20:
        UmnWork[0x11] = 0;
        UmnWork[0x12] = 0;
        UmnWork[3] = 0x21;
        /* fallthrough */
    case 0x21:
        if ((signed char)UmnWork[0x10] == 0 && (signed char)UmnWork[0x12] == 0) {
            if (UmnWork[0x30] >= 2) {
                t2 = *(unsigned short *)(PadData + 0x30);
                if (t2 & 0x1000) {
                    UmnWork[0x12] = -1;
                }
                if (t2 & 0x4000) {
                    UmnWork[0x12] = 1;
                }
            }
            if ((signed char)UmnWork[0x12] != 0) {
                UmnWork[0x31] = (unsigned char)(UmnWork[0x31] + UmnWork[0x12]);
                v = UmnWork[0x31] % UmnWork[0x30];
                UmnWork[0x10] = 7;
                UmnWork[0x31] = v;
                xglSoundEffectNormalID(3, 0);
            } else {
                t3 = *(unsigned short *)(PadData + 0x2A);
                if (t3 & 0x20) {
                    UmnWork[2] = UmnWork[0x31];
                    if (UmnWork[0x20 + UmnWork[0x31]] != 0) {
                        UmnChangeTopLevel(UmnWork[0x20 + UmnWork[0x31]]);
                        xglSoundEffectNormalID(1, 0);
                    } else {
                        xglSoundEffectNormalID(5, 0);
                    }
                } else if (t3 & 0x40) {
                    UmnWork[3] = 0xF0;
                    xglSoundEffectNormalID(2, 0);
                }
            }
        }
        break;
    case 0xF0:
        UmnWork[1] = 0xFF;
        break;
    }

    if ((signed char)UmnWork[0x10] != 0) {
        UmnWork[0x10] = (signed char)UmnWork[0x10] - 1;
    }
    xglFontDebugPrintf(0, 0x40, D_00A12050, UmnWork[0x31]);
}

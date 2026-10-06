/*
 * OV12 original TU 81: 0x00a48548..0x00a487c0 (2 functions)
 */
#include "common.h"
#include "shared.h"

const char D_00A58D10[16] = "log2 error %d";
const char D_00A58D20[24] = "../xrg_bxx_ps2.euc.c";

extern void RgError(const char *message, const char *source_file, int line,
                    ...);


static int _Log2(u32 bucket_count)
{
    if (bucket_count < 3U) {
        return 1;
    }
    if (bucket_count < 5U) {
        return 2;
    }
    if (bucket_count < 9U) {
        return 3;
    }
    if (bucket_count < 17U) {
        return 4;
    }
    if (bucket_count < 33U) {
        return 5;
    }
    if (bucket_count < 65U) {
        return 6;
    }
    if (bucket_count < 129U) {
        return 7;
    }
    if (bucket_count < 257U) {
        return 8;
    }
    if (bucket_count < 513U) {
        return 9;
    }
    if (bucket_count < 1025U) {
        return 10;
    }
    if (bucket_count < 2049U) {
        return 11;
    }
    if (bucket_count < 4097U) {
        return 12;
    }

    RgError(D_00A58D10, D_00A58D20, 50, bucket_count);
    return 0;
}

typedef struct XrgBxxHead {
    char pad00[0x10];
    u32 tbw4;   /* 0x10 */
    u32 tbw8;   /* 0x14 */
    u32 tbw16;  /* 0x18 */
    u32 tbw32;  /* 0x1C */
    u32 height; /* 0x20 */
} XrgBxxHead;
typedef struct XrgBxxTexPic {
    char pad00[0x24];
    int bpp;    /* 0x24 */
    char pad28[0x14];
    int cbp;    /* 0x3C */
    char pad40[0x18];
    int bxx;    /* 0x58 */
} XrgBxxTexPic;
extern void assert_prog(const char *expression, const char *source_file, int line);
extern XrgBxxHead *RgBxxGetHeader(int bxx);
extern const char D_00A58D38[];
extern const char D_00A58D48[];
extern const char D_00A58D58[];
extern const char D_00A58D60[];

u64 XrgBxxPs2Tex0(XrgBxxTexPic *pPic)
{
    XrgBxxHead *pHead;
    u32 tbw = 0;
    u64 psm = 0;
    u32 height;
    u32 tw, th;
    u64 tbp = 0x3800;
    u64 tcc = 1;

    if (pPic == 0) {
        assert_prog(D_00A58D38, D_00A58D20, 0x45);
    }
    pHead = RgBxxGetHeader(pPic->bxx);
    if (pHead == 0) {
        assert_prog(D_00A58D48, D_00A58D20, 0x47);
    }
    height = pHead->height;
    switch (pPic->bpp) {
    case 4:
        tbw = pHead->tbw4;
        psm = 0x14;
        break;
    case 8:
        tbw = pHead->tbw8;
        psm = 0x13;
        break;
    case 16:
        tbw = pHead->tbw16;
        psm = 2;
        break;
    case 24:
        tbw = pHead->tbw32;
        psm = 1;
        break;
    case 32:
        tbw = pHead->tbw32;
        psm = 0;
        break;
    case -8:
        RgError(D_00A58D58, D_00A58D20, 0x5F, pPic->bpp);
        break;
    default:
        RgError(D_00A58D60, D_00A58D20, 0x62, pPic->bpp);
        break;
    }
    tw = _Log2(tbw << 6);
    th = _Log2(height);
    return (u64)tbp | ((u64)psm << 20) | ((u64)tbw << 14)
         | ((u64)tw << 26) | ((u64)th << 30)
         | ((u64)tcc << 34) | ((u64)0 << 35) | ((u64)(pPic->cbp + 0x3800) << 37)
         | ((u64)0 << 51) | ((u64)0 << 55) | ((u64)0 << 56) | ((u64)1 << 61);
}

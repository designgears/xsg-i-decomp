/*
 * OV10 original TU 6: 0x00a1e5d8..0x00a21348 (27 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov10/cgp.h"

typedef struct CardHelpPackedVector {
    u64 words[2];
} CardHelpPackedVector;

void xglMatrixScale(Matrix4 destination, const Matrix4 source,
                            const float scale[4]);
void xglMatrixTrans(Matrix4 destination, const Matrix4 source,
                            const float translation[4]);
void CardDispLight(Matrix4 matrix, int card, int enabled, int color);
extern void nmlModelEntry(u32 model);
extern void nmlModelSetPlace(const Vector4 *matrix);
extern void nmlModelSetRenderLevel(int level);
extern void nmlModelSetTexture(const char *texture);

extern const CardHelpPackedVector D_00A4E0A0;
extern const CardHelpPackedVector D_00A4E0D0;
extern const CardHelpPackedVector D_00A4E0E0;
extern const CardHelpPackedVector D_00A4E170;
extern const CardHelpPackedVector D_00A4E180;
extern const CardHelpPackedVector D_00A4E190;
extern const CardHelpPackedVector D_00A4E1A0;
extern const CardHelpPackedVector D_00A4E1C0;
extern const CardHelpPackedVector D_00A4E1D0;
extern const char *cardxtx_tbl[];
extern u32 cardlex_tbl[];

extern const CardHelpPackedVector D_00A4E0B0;
extern const CardHelpPackedVector D_00A4E0C0;
extern void nmlModelSetLight(Matrix4 color, Matrix4 direction);
extern float xglSin(float angle);
int ChangeCnt = 0;
extern Matrix4 asDir;

/*
 * Help-title list state CHPTitleInitSub resets when CardHelpProc
 * (0x00a20908) opens a help category; CHPMenuInitSub (0x00a1e640, not part
 * of this TU's claimed source) runs immediately after it. TitleMaxList
 * (config/symbols/ov10.txt, 7 bytes) holds one entry-count byte per
 * category, read here through a pointer captured early and dereferenced
 * once per field (count and limit each get their own read). Only the
 * members CHPTitleInitSub writes are named; +0x04 stays unmodeled because
 * this function does not touch it.
 */
typedef struct CHPTitleList {
    s16 cursor;      /* +0x00: selected row, reset to the top */
    s8 flagA;        /* +0x02 */
    s8 flagB;        /* +0x03 */
    u8 unmodeled_04;
    s8 flagC;        /* +0x05 */
    s16 x;           /* +0x06: screen column */
    s16 y;           /* +0x08: screen row */
    s16 rowHeight;   /* +0x0A */
    s16 scroll;      /* +0x0C: top row offset, reset */
    s16 step;        /* +0x0E: row step */
    s16 count;       /* +0x10: TitleMaxList[category], sign-extended */
    s16 limit;       /* +0x12: TitleMaxList[category], sign-extended */
} CHPTitleList;

extern const u8 TitleMaxList[7];

void CHPTitleInitSub(CHPTitleList *list, int category)
{
    const u8 *entry;
    u8 raw;

    list->x = 266;
    list->y = 50;
    entry = &TitleMaxList[category];
    list->rowHeight = 24;
    list->cursor = 0;
    list->flagA = 0;
    list->flagB = 0;
    list->flagC = 0;
    list->scroll = 0;
    raw = *entry;
    list->step = 1;
    list->count = (s8)raw;
    raw = *entry;
    list->limit = (s8)raw;
}

void CHPMenuInitSub(CHPTitleList *menu, CHPTitleList *title)
{
    menu->x = 6; menu->y = 50; menu->rowHeight = 24; menu->cursor = 0; menu->flagA = 0; menu->flagB = 0; menu->flagC = 0; menu->scroll = 0; menu->count = 9; menu->step = 1; menu->limit = 7;
    CHPTitleInitSub(title, 0);
}

extern const char D_00A4DDB0[];
extern char *const D_00A4DF60[];
extern const char D_00A4DFF0[];
extern const char D_00A4DFF8[];
typedef struct CardHelpNameList {
    char *name[35];
} CardHelpNameList;

void CardHelpFileLoad(void)
{
    char path[64];
    char file[128];
    CardHelpNameList names;
    u32 addr;
    int size;
    int i;

    __builtin_memcpy(path, D_00A4DDB0, 20);
    memset(path + 20, 0, 0x2C);
    names = *(CardHelpNameList *)D_00A4DF60;
    addr = 0x1720000;
    for (i = 0; i < 64 && names.name[i] != 0; i++) {
        memset(file, 0, 0x80);
        strcpy(file, path);
        strcat(file, names.name[i]);
        strcat(file, D_00A4DFF0);
        ((u32 *)cardlex_tbl)[i] = addr;
        size = xglCdReadFile(file, (void *)addr, 0, 1);
        if (size > 0) {
            addr += size; addr = (addr + 15) & ~15;
        }
        memset(file, 0, 0x80);
        strcpy(file, path);
        strcat(file, names.name[i]);
        strcat(file, D_00A4DFF8);
        ((u32 *)cardxtx_tbl)[i] = addr;
        size = xglCdReadFile(file, (void *)addr, 0, 1);
        if (size > 0) {
            addr += size; addr = (addr + 15) & ~15;
        }
    }
}

/* CardHelpFileLoad: help directory, file stems, extensions. */
const char D_00A4DDB0[] = "data\\carddata\\help\\";
const char D_00A4DDC8[] = "h_tittle";
const char D_00A4DDD8[] = "h_ph1";
const char D_00A4DDE0[] = "h_ph2";
const char D_00A4DDE8[] = "h_text";
const char D_00A4DDF0[] = "h_event";
const char D_00A4DDF8[] = "h_battle1";
const char D_00A4DE08[] = "h_battle2";
const char D_00A4DE18[] = "h_down";
const char D_00A4DE20[] = "h_Emark";
const char D_00A4DE28[] = "h_Eblake";
const char D_00A4DE38[] = "h_open";
const char D_00A4DE40[] = "h_viewer";
const char D_00A4DE50[] = "h_decksave";
const char D_00A4DE60[] = "help5";
const char D_00A4DE68[] = "help6";
const char D_00A4DE70[] = "h_1P2P";
const char D_00A4DE78[] = "h_base";
const char D_00A4DE80[] = "h_battl";
const char D_00A4DE88[] = "h_field";
const char D_00A4DE90[] = "h_hand";
const char D_00A4DE98[] = "h_hand2";
const char D_00A4DEA0[] = "h_junk";
const char D_00A4DEA8[] = "h_lost";
const char D_00A4DEB0[] = "h_situation";
const char D_00A4DEC0[] = "h_standby";
const char D_00A4DED0[] = "h_deckmake";
const char D_00A4DEE0[] = "h_decklist";
const char D_00A4DEF0[] = "h_Phase1";
const char D_00A4DF00[] = "h_Phase2";
const char D_00A4DF10[] = "h_Phase3";
const char D_00A4DF20[] = "h_Phase4";
const char D_00A4DF30[] = "h_Phase5";
const char D_00A4DF40[] = "h_Phase6";
const char D_00A4DF50[] = "h_Phase7";
char *const D_00A4DF60[35] = {
    (char *)D_00A4DDC8,
    (char *)D_00A4DDD8,
    (char *)D_00A4DDE0,
    (char *)D_00A4DDE8,
    (char *)D_00A4DDF0,
    (char *)D_00A4DDF8,
    (char *)D_00A4DE08,
    (char *)D_00A4DE18,
    (char *)D_00A4DE20,
    (char *)D_00A4DE28,
    (char *)D_00A4DE38,
    (char *)D_00A4DE40,
    (char *)D_00A4DE50,
    (char *)D_00A4DE60,
    (char *)D_00A4DE68,
    (char *)D_00A4DE70,
    (char *)D_00A4DE78,
    (char *)D_00A4DE80,
    (char *)D_00A4DE88,
    (char *)D_00A4DE90,
    (char *)D_00A4DE98,
    (char *)D_00A4DEA0,
    (char *)D_00A4DEA8,
    (char *)D_00A4DEB0,
    (char *)D_00A4DEC0,
    (char *)D_00A4DED0,
    (char *)D_00A4DEE0,
    (char *)D_00A4DEF0,
    (char *)D_00A4DF00,
    (char *)D_00A4DF10,
    (char *)D_00A4DF20,
    (char *)D_00A4DF30,
    (char *)D_00A4DF40,
    (char *)D_00A4DF50,
    0
};
const char D_00A4DFF0[] = ".lex";
const char D_00A4DFF8[] = ".xtx";

extern void nmlModelEntryCard(void *model);

static void CHPMenuBackDispSub(Matrix4 matrix)
{
    Matrix4 transformed;
    Vector4 translation;
    float left = -1.4184f;
    float zero = 0.0f;

    translation.x = left;
    translation.w = 1.0f;
    translation.z = zero;
    translation.y = zero;
    xglMatrixTrans(transformed, matrix, &translation.x);
    nmlModelSetTexture((const char *)0x01CB0000);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetRenderLevel(2);
    nmlModelEntryCard((void *)0x01CA2B00);
    translation.x = 1.4184f;
    translation.z = zero;
    translation.y = zero;
    xglMatrixTrans(transformed, matrix, &translation.x);
    nmlModelSetTexture((const char *)0x01CB0000);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetRenderLevel(2);
    nmlModelEntryCard((void *)0x01CA1000);
    translation.x = left;
    translation.y = -1.832f;
    xglMatrixTrans(transformed, matrix, &translation.x);
    nmlModelSetTexture((const char *)0x01CC0050);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetRenderLevel(2);
    nmlModelEntryCard((void *)0x01CA4000);
}

void CardDispHelpCurrySet(Matrix4 matrix)
{
    Matrix4 transformed;
    Vector4 translation;
    Vector4 scale;

    translation.x = 1.5143f;
    translation.y = 1.186f;
    translation.z = 0.0f;
    translation.w = 1.0f;
    scale.x = 0.46f;
    scale.y = 0.46f;
    scale.z = 1.0f;
    scale.w = 1.0f;
    xglMatrixTrans(transformed, matrix, &translation.x);
    xglMatrixScale(transformed, transformed, &scale.x);
    CardDisp(transformed, 0, 1, 0);
    translation.y = 0.215f;
    xglMatrixTrans(transformed, matrix, &translation.x);
    xglMatrixScale(transformed, transformed, &scale.x);
    CardDisp(transformed, 0x54, 1, 0);
    translation.y = -0.757f;
    xglMatrixTrans(transformed, matrix, &translation.x);
    xglMatrixScale(transformed, transformed, &scale.x);
    CardDisp(transformed, 0x78, 1, 0);
}


void CardDispHelpSlide1(char mode, Matrix4 matrix)
{
    static short list16[4] __asm__("list16.0") = {0, 42, 2, 29};
    static short list17[3] __asm__("list17.1") = {84, 85, 86};
    static short list18[3] __asm__("list18.2") = {120, 121, 122};
    static float trans[4] __asm__("trans.3") __attribute__((aligned(16))) = {0.55715f, 1.246f, 0.0f, 1.0f};
    Matrix4 transformed;
    Vector4 scale;
    int idx;

    scale.x = 0.368f;
    scale.y = 0.368f;
    scale.z = 1.0f;
    scale.w = 1.0f;
    switch (mode) {
    case 16:
        idx = (ChangeCnt = ChangeCnt % 256) / 64;
        xglMatrixTrans(transformed, matrix, trans);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list16[idx], 1, 0);
        break;
    case 17:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, trans);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list17[idx], 1, 0);
        break;
    case 18:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, trans);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list18[idx], 1, 0);
        break;
    }
}

typedef struct { float x, y, z, w; } __attribute__((aligned(16))) Slide2Vec;
/* {1.397f, 0.179f, 0.0f, 1.0f} */
const CardHelpPackedVector D_00A4E000 __attribute__((aligned(16))) = {{0x3E374BC73FB2D0E5ULL, 0x3F80000000000000ULL}};

void CardDispHelpSlide2(char mode, Matrix4 matrix)
{
    static short list19[3] __asm__("list19.4") = {0, 84, 120};
    static short list20[3] __asm__("list20.5") = {0, 84, 120};
    static short list21[3] __asm__("list21.6") = {0, 84, 120};
    static short list22[6] __asm__("list22.7") = {0, 39, 67, 3, 49, 29};
    static short list25[6] __asm__("list25.8") = {0, 84, 120, 0, 0, 0};
    static short list26[6] __asm__("list26.9") = {0, 3, 18, 33, 25, 66};
    static short list27[6] __asm__("list27.10") = {0, 3, 18, 33, 43, 66};
    static short list28[4] __asm__("list28.11") = {2, 0, 40, 41};
    static short list45[6] __asm__("list45.12") = {49, 50, 51, 42, 43, 44};
    static short list46[6] __asm__("list46.13") = {49, 50, 51, 42, 43, 44};
    static short list47[4] __asm__("list47.14") = {67, 69, 74, 75};
    Matrix4 transformed;
    Slide2Vec translation;
    Vector4 scale;
    int idx;

    translation = *(Slide2Vec *)&D_00A4E000;
    scale.x = 0.9f;
    scale.z = 1.0f;
    scale.y = 0.9f;
    scale.w = 1.0f;
    switch (mode) {
    case 19:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list19[idx], 1, 0);
        break;
    case 20:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list20[idx], 1, 0);
        break;
    case 21:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list21[idx], 1, 0);
        break;
    case 22:
        idx = (ChangeCnt = ChangeCnt % 384) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDispLight(transformed, list22[idx], 1, 0x10);
        break;
    case 25:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDispLight(transformed, list25[idx], 1, 8);
        break;
    case 26:
        idx = (ChangeCnt = ChangeCnt % 384) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDispLight(transformed, list26[idx], 1, 2);
        break;
    case 27:
        idx = (ChangeCnt = ChangeCnt % 384) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDispLight(transformed, list27[idx], 1, 4);
        break;
    case 28:
        idx = (ChangeCnt = ChangeCnt % 256) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDispLight(transformed, list28[idx], 1, 0x40);
        break;
    case 45:
        idx = (ChangeCnt = ChangeCnt % 384) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list45[idx], 1, 0);
        break;
    case 46:
        idx = (ChangeCnt = ChangeCnt % 384) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list46[idx], 1, 0);
        break;
    case 47:
        idx = (ChangeCnt = ChangeCnt % 256) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list47[idx], 1, 0);
        break;
    }
}

typedef struct { float x, y, z, w; } __attribute__((aligned(16))) Slide3Vec;
/* {1.733f, 0.671f, 0.0f, 1.0f} */
const CardHelpPackedVector D_00A4E090 __attribute__((aligned(16))) = {{0x3F2BC6A83FDDD2F2ULL, 0x3F80000000000000ULL}};

void CardDispHelpSlide3(char mode, Matrix4 matrix)
{
    static short list23[3] __asm__("list23.15") = {0, 84, 120};
    static short list24[3] __asm__("list24.16") = {0, 84, 120};
    Matrix4 transformed;
    Slide3Vec translation;
    Vector4 scale;
    int idx;

    translation = *(Slide3Vec *)&D_00A4E090;
    scale.x = 0.7f;
    scale.y = 0.7f;
    scale.z = 1.0f;
    scale.w = 1.0f;
    switch (mode) {
    case 23:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list23[idx], 1, 0);
        break;
    case 24:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list24[idx], 1, 0);
        break;
    }
}

/* {1.48f, 0.16f, 0.0f, 1.0f} */
const CardHelpPackedVector D_00A4E0A0 __attribute__((aligned(16))) = {{0x3E23D70A3FBD70A4ULL, 0x3F80000000000000ULL}};

void CardDispCtr(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpPackedVector translation;

    translation = D_00A4E0A0;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   (const float *)translation.words);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[0x34 / 4]);
    nmlModelEntry(cardlex_tbl[0x34 / 4]);
}

void CardDispField(Matrix4 matrix, const float translation[4])
{
    Matrix4 transformed;

    xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[0x48 / 4]);
    nmlModelEntry(cardlex_tbl[0x48 / 4]);
}

/* {1.37f, 0.2f, 0.0f, 1.0f} */
const CardHelpPackedVector D_00A4E0B0 __attribute__((aligned(16))) = {{0x3E4CCCCD3FAF5C29ULL, 0x3F80000000000000ULL}};
/* {0.9f, 0.9f, 1.0f, 1.0f} */
const CardHelpPackedVector D_00A4E0C0 __attribute__((aligned(16))) = {{0x3F6666663F666666ULL, 0x3F8000003F800000ULL}};

void CardDispDeckMake(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpPackedVector translation;
    CardHelpPackedVector scale;

    translation = D_00A4E0B0;
    scale = D_00A4E0C0;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   (const float *)translation.words);
    xglMatrixScale(transformed, (const float (*)[4])transformed,
                   (const float *)scale.words);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[0x64 / 4]);
    nmlModelEntry(cardlex_tbl[0x64 / 4]);
}

void CardDispH1P2P(Matrix4 matrix, const float translation[4])
{
    Matrix4 transformed;

    xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[0x3C / 4]);
    nmlModelEntry(cardlex_tbl[0x3C / 4]);
}

/* {1.5f, 0.18f, 0.0f, 1.0f} */
const CardHelpPackedVector D_00A4E0D0 __attribute__((aligned(16))) = {{0x3E3851EC3FC00000ULL, 0x3F80000000000000ULL}};
/* {0.9f, 0.9f, 1.0f, 1.0f} */
const CardHelpPackedVector D_00A4E0E0 __attribute__((aligned(16))) = {{0x3F6666663F666666ULL, 0x3F8000003F800000ULL}};

void CardDispTurn(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpPackedVector translation;
    CardHelpPackedVector scale;

    translation = D_00A4E0D0;
    scale = D_00A4E0E0;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   (const float *)translation.words);
    xglMatrixScale(transformed, (const float (*)[4])transformed,
                   (const float *)scale.words);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[0x38 / 4]);
    nmlModelEntry(cardlex_tbl[0x38 / 4]);
}

void CardDispFieldPointScale(Matrix4 matrix, const float translation[4],
                              const float scale[4], int kind)
{
    Matrix4 transformed;
    Matrix4 color;
    float light;

    light = (xglSin((((float)ChangeCnt * 11.25f) / 180.0f) * 3.1415927f) * 0.5f) + 1.0f;
    color[3][0] = color[3][1] = color[3][2] = light;
    xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
    xglMatrixScale(transformed, (const float (*)[4])transformed, scale);
    nmlModelSetLight(color, asDir);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[kind]);
    nmlModelEntry(cardlex_tbl[kind]);
}

typedef struct { float x, y, z, w; } __attribute__((aligned(16))) TejyunVec;
/* {1.397f, -0.317f, 0.0f, 1.0f} */
const CardHelpPackedVector D_00A4E0F0 __attribute__((aligned(16))) = {{0xBEA24DD33FB2D0E5ULL, 0x3F80000000000000ULL}};
/* {1.397f, 1.457f, 0.0f, 1.0f} */
const CardHelpPackedVector D_00A4E100 __attribute__((aligned(16))) = {{0x3FBA7EFA3FB2D0E5ULL, 0x3F80000000000000ULL}};
/* {0.9f, 0.9f, 1.0f, 1.0f} */
const CardHelpPackedVector D_00A4E110 __attribute__((aligned(16))) = {{0x3F6666663F666666ULL, 0x3F8000003F800000ULL}};

void CardDispTejyun(Matrix4 matrix, int mode)
{
    Matrix4 transformed;
    TejyunVec trans1;
    TejyunVec trans2;
    TejyunVec scale;
    Matrix4 color;
    float light;

    trans1 = *(TejyunVec *)&D_00A4E0F0;
    trans2 = *(TejyunVec *)&D_00A4E100;
    scale = *(TejyunVec *)&D_00A4E110;
    light = (xglSin((((float)ChangeCnt * 11.25f) / 180.0f) * 3.1415927f) * 0.5f) + 1.0f;
    color[3][0] = color[3][1] = color[3][2] = light;
    xglMatrixTrans(transformed, matrix, &trans1.x);
    xglMatrixScale(transformed, transformed, &scale.x);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[0x12]);
    nmlModelEntry(cardlex_tbl[0x12]);
    xglMatrixTrans(transformed, matrix, &trans2.x);
    switch (mode) {
    case 27:
        CardDispFieldPointScale(matrix, &trans1.x, &scale.x, 0x10);
        CardDispFieldPointScale(matrix, &trans1.x, &scale.x, 0x13);
        nmlModelSetLight(color, asDir);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x1B]);
        nmlModelEntry(cardlex_tbl[0x1B]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x1C]);
        nmlModelEntry(cardlex_tbl[0x1C]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x1D]);
        nmlModelEntry(cardlex_tbl[0x1D]);
        break;
    case 28:
        CardDispFieldPointScale(matrix, &trans1.x, &scale.x, 0x11);
        CardDispFieldPointScale(matrix, &trans1.x, &scale.x, 0x18);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x1B]);
        nmlModelEntry(cardlex_tbl[0x1B]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetLight(color, asDir);
        nmlModelSetTexture(cardxtx_tbl[0x1C]);
        nmlModelEntry(cardlex_tbl[0x1C]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x1D]);
        nmlModelEntry(cardlex_tbl[0x1D]);
        break;
    case 29:
        CardDispFieldPointScale(matrix, &trans1.x, &scale.x, 0x13);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x1C]);
        nmlModelEntry(cardlex_tbl[0x1C]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetLight(color, asDir);
        nmlModelSetTexture(cardxtx_tbl[0x1D]);
        nmlModelEntry(cardlex_tbl[0x1D]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x1E]);
        nmlModelEntry(cardlex_tbl[0x1E]);
        break;
    case 30:
        CardDispFieldPointScale(matrix, &trans1.x, &scale.x, 0x13);
        CardDispFieldPointScale(matrix, &trans1.x, &scale.x, 0x18);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x1D]);
        nmlModelEntry(cardlex_tbl[0x1D]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetLight(color, asDir);
        nmlModelSetTexture(cardxtx_tbl[0x1E]);
        nmlModelEntry(cardlex_tbl[0x1E]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x1F]);
        nmlModelEntry(cardlex_tbl[0x1F]);
        break;
    case 31:
        CardDispFieldPointScale(matrix, &trans1.x, &scale.x, 0x14);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x1E]);
        nmlModelEntry(cardlex_tbl[0x1E]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetLight(color, asDir);
        nmlModelSetTexture(cardxtx_tbl[0x1F]);
        nmlModelEntry(cardlex_tbl[0x1F]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x20]);
        nmlModelEntry(cardlex_tbl[0x20]);
        break;
    case 32:
        CardDispFieldPointScale(matrix, &trans1.x, &scale.x, 0x11);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x1F]);
        nmlModelEntry(cardlex_tbl[0x1F]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetLight(color, asDir);
        nmlModelSetTexture(cardxtx_tbl[0x20]);
        nmlModelEntry(cardlex_tbl[0x20]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x21]);
        nmlModelEntry(cardlex_tbl[0x21]);
        break;
    case 33:
        CardDispFieldPointScale(matrix, &trans1.x, &scale.x, 0x13);
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x1F]);
        nmlModelEntry(cardlex_tbl[0x1F]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x20]);
        nmlModelEntry(cardlex_tbl[0x20]);
        transformed[3][1] -= 0.3f;
        nmlModelSetPlace((const Vector4 *)transformed);
        nmlModelSetTexture(cardxtx_tbl[0x21]);
        nmlModelSetLight(color, asDir);
        nmlModelEntry(cardlex_tbl[0x21]);
        break;
    }
}

void CardDispFieldPoint(Matrix4 matrix, const float translation[4], int kind)
{
    Matrix4 transformed;
    Matrix4 color;
    float light;

    light = (xglSin((((float)ChangeCnt * 11.25f) / 180.0f) * 3.1415927f) * 0.5f) + 1.0f;
    color[3][0] = color[3][1] = color[3][2] = light;
    xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
    nmlModelSetLight(color, asDir);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[kind]);
    nmlModelEntry(cardlex_tbl[kind]);
}

/* {1.397f, 0.863f, 0.0f, 1.0f} */
const CardHelpPackedVector D_00A4E140 __attribute__((aligned(16))) = {{0x3F5CED913FB2D0E5ULL, 0x3F80000000000000ULL}};
/* {0.6f, 0.6f, 1.0f, 1.0f} */
const CardHelpPackedVector D_00A4E150 __attribute__((aligned(16))) = {{0x3F19999A3F19999AULL, 0x3F8000003F800000ULL}};

void CardDispGun(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpPackedVector translation;
    CardHelpPackedVector scale;

    translation = D_00A4E140;
    scale = D_00A4E150;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   (const float *)translation.words);
    xglMatrixScale(transformed, (const float (*)[4])transformed,
                   (const float *)scale.words);
    CardDisp(transformed, 0x31, 1, 0);
    transformed[3][1] -= 1.33;
    CardDisp(transformed, 0x44, 1, 0);
}

/* {0.17f, 0.17f, 1.0f, 1.0f} */
const CardHelpPackedVector D_00A4E170 __attribute__((aligned(16))) = {{0x3E2E147B3E2E147BULL, 0x3F8000003F800000ULL}};

void CardDispShineCurry(Matrix4 matrix, const float translation[4])
{
    Matrix4 transformed;
    CardHelpPackedVector scale;

    scale = D_00A4E170;
    xglMatrixTrans(transformed, (const float (*)[4])matrix, translation);
    xglMatrixScale(transformed, (const float (*)[4])transformed,
                   (const float *)scale.words);
    CardDispLight(transformed, 84, 1, 255);
}

/* {-2.64f, 1.16f, 0.02f, 1.0f} */
const CardHelpPackedVector D_00A4E180 __attribute__((aligned(16))) = {{0x3F947AE1C028F5C3ULL, 0x3F8000003CA3D70AULL}};

void CardDispAttr(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpPackedVector translation;
    const float step = -0.38f;

    translation = D_00A4E180;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   (const float *)translation.words);
    nmlModelSetPlace((const Vector4 *)transformed);

    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C87400);
    transformed[3][1] += step;

    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C87C00);
    transformed[3][1] += step;

    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C88000);
    transformed[3][1] += step;

    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C87800);
    transformed[3][1] += step;

    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C88800);
    transformed[3][1] += -0.57f;

    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C88400);
}

/* {-2.2f, 0.58f, 0.02f, 1.0f} */
const CardHelpPackedVector D_00A4E190 __attribute__((aligned(16))) = {{0x3F147AE1C00CCCCDULL, 0x3F8000003CA3D70AULL}};

void CardDispNazo(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpPackedVector translation;

    translation = D_00A4E190;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   (const float *)translation.words);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C89000);
}

/* {-1.1f, 1.53f, 0.02f, 1.0f} */
const CardHelpPackedVector D_00A4E1A0 __attribute__((aligned(16))) = {{0x3FC3D70ABF8CCCCDULL, 0x3F8000003CA3D70AULL}};

void CardDispGuno(Matrix4 matrix)
{
    Matrix4 transformed;
    CardHelpPackedVector translation;

    translation = D_00A4E1A0;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   (const float *)translation.words);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture((const char *)0x01C91000);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(0x01C88000);
}

/* {1.397f, 0.179f, 0.0f, 1.0f} */
const CardHelpPackedVector D_00A4E1B0 __attribute__((aligned(16))) = {{0x3E374BC73FB2D0E5ULL, 0x3F80000000000000ULL}};

void CardDispOnce(Matrix4 matrix, int card)
{
    Matrix4 transformed;
    CardHelpPackedVector translation;
    Vector4 scale;

    translation = ((const union { CardHelpPackedVector v; } *)&D_00A4E1B0)->v;
    scale.x = 0.9f;
    scale.y = 0.9f;
    scale.z = 1.0f;
    scale.w = 1.0f;
    xglMatrixTrans(transformed, matrix, (const float *)&translation);
    xglMatrixScale(transformed, transformed, &scale.x);
    CardDispLight(transformed, (s16)card, 1, 0x20);
}

/* {1.36f, 0.15f, 0.0f, 1.0f} */
const CardHelpPackedVector D_00A4E1C0 __attribute__((aligned(16))) = {{0x3E19999A3FAE147BULL, 0x3F80000000000000ULL}};

void CardDispComm(Matrix4 matrix, int kind)
{
    Matrix4 transformed;
    CardHelpPackedVector translation;

    translation = D_00A4E1C0;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   (const float *)translation.words);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[kind]);
    nmlModelEntry(cardlex_tbl[kind]);
}

/* {1.36f, 0.15f, 0.0f, 1.0f} */
const CardHelpPackedVector D_00A4E1D0 __attribute__((aligned(16))) = {{0x3E19999A3FAE147BULL, 0x3F80000000000000ULL}};

void CardDispPhase(Matrix4 matrix, int kind)
{
    Matrix4 transformed;
    Matrix4 color;
    CardHelpPackedVector translation;
    float light;

    translation = D_00A4E1D0;
    light = (xglSin((((float)ChangeCnt * 11.25f) / 180.0f) * 3.1415927f) * 0.5f) + 1.0f;
    color[3][0] = color[3][1] = color[3][2] = light;
    xglMatrixTrans(transformed, (const float (*)[4])matrix,
                   (const float *)translation.words);
    nmlModelSetLight(color, asDir);
    nmlModelSetPlace((const Vector4 *)transformed);
    nmlModelSetTexture(cardxtx_tbl[kind]);
    nmlModelEntry(cardlex_tbl[kind]);
}

typedef struct { float x, y, z, w; } __attribute__((aligned(16))) HelpCardVec;
/* {1.397f, 0.215f, 0.0f, 1.0f} */
const CardHelpPackedVector D_00A4E1E0 __attribute__((aligned(16))) = {{0x3E5C28F63FB2D0E5ULL, 0x3F80000000000000ULL}};
/* {1.397f, -0.188f, 0.0f, 1.0f} */
const CardHelpPackedVector D_00A4E1F0 __attribute__((aligned(16))) = {{0xBE4083123FB2D0E5ULL, 0x3F80000000000000ULL}};

void CardDispHelpCard(char mode, Matrix4 matrix)
{
    HelpCardVec trans1;
    HelpCardVec trans2;

    ChangeCnt++;
    trans1 = *(HelpCardVec *)&D_00A4E1E0;
    trans2 = *(HelpCardVec *)&D_00A4E1F0;
    switch (mode) {
    case 0:
        CardDispComm(matrix, 0);
        break;
    case 1:
        CardDispCtr(matrix);
        break;
    case 3:
        CardDispComm(matrix, 1);
        break;
    case 4:
        CardDispComm(matrix, 1);
        CardDispPhase(matrix, 2);
        break;
    case 6:
        CardDispField(matrix, &trans1.x);
        CardDispH1P2P(matrix, &trans1.x);
        break;
    case 7:
        CardDispField(matrix, &trans1.x);
        CardDispFieldPoint(matrix, &trans1.x, 0x10);
        break;
    case 8:
        CardDispField(matrix, &trans1.x);
        CardDispFieldPoint(matrix, &trans1.x, 0x16);
        break;
    case 9:
        CardDispField(matrix, &trans1.x);
        CardDispFieldPoint(matrix, &trans1.x, 0x13);
        break;
    case 10:
        CardDispField(matrix, &trans1.x);
        CardDispFieldPoint(matrix, &trans1.x, 0x15);
        break;
    case 11:
        CardDispField(matrix, &trans1.x);
        CardDispFieldPoint(matrix, &trans1.x, 0x18);
        break;
    case 12:
        CardDispField(matrix, &trans1.x);
        CardDispFieldPoint(matrix, &trans1.x, 0x17);
        break;
    case 13:
        CardDispField(matrix, &trans1.x);
        CardDispFieldPoint(matrix, &trans1.x, 0x11);
        break;
    case 14:
        CardDispField(matrix, &trans1.x);
        break;
    case 2:
    case 15:
        CardDispHelpCurrySet(matrix);
        break;
    case 16:
        CardDispHelpSlide1(mode, matrix);
        CardDispField(matrix, &trans2.x);
        CardDispFieldPoint(matrix, &trans2.x, 0x11);
        CardDispFieldPoint(matrix, &trans2.x, 0x18);
        break;
    case 17:
        CardDispHelpSlide1(mode, matrix);
        CardDispField(matrix, &trans2.x);
        CardDispShineCurry(matrix, &trans2.x);
        break;
    case 18:
        CardDispHelpSlide1(mode, matrix);
        CardDispField(matrix, &trans2.x);
        CardDispFieldPoint(matrix, &trans2.x, 0x17);
        break;
    case 22:
        CardDispHelpSlide2(mode, matrix);
        CardDispAttr(matrix);
        break;
    case 23:
        CardDispOnce(matrix, 7);
        break;
    case 24:
        CardDispNazo(matrix);
        CardDispOnce(matrix, 0x1D);
        break;
    case 29:
        CardDispComm(matrix, 3);
        break;
    case 5:
    case 30:
    case 31:
    case 32:
        CardDispTurn(matrix);
        break;
    case 33:
        CardDispTejyun(matrix, 0x1B);
        break;
    case 34:
        CardDispTejyun(matrix, 0x1C);
        break;
    case 35:
    case 36:
    case 37:
        CardDispTejyun(matrix, 0x1D);
        break;
    case 38:
    case 39:
        CardDispTejyun(matrix, 0x1E);
        break;
    case 40:
        CardDispTejyun(matrix, 0x1F);
        break;
    case 41:
    case 42:
        CardDispTejyun(matrix, 0x20);
        break;
    case 43:
        CardDispTejyun(matrix, 0x21);
        break;
    case 44:
        CardDispGun(matrix);
        break;
    case 19:
    case 20:
    case 21:
    case 25:
    case 26:
    case 27:
    case 28:
    case 45:
    case 46:
        CardDispHelpSlide2(mode, matrix);
        break;
    case 47:
        CardDispHelpSlide2(mode, matrix);
        CardDispGuno(matrix);
        break;
    case 49:
        CardDispComm(matrix, 6);
        break;
    case 50:
        CardDispComm(matrix, 7);
        break;
    case 51:
        CardDispComm(matrix, 8);
        break;
    case 52:
        CardDispComm(matrix, 9);
        break;
    case 53:
        CardDispComm(matrix, 10);
        break;
    case 54:
        CardDispComm(matrix, 11);
        break;
    case 55:
        CardDispComm(matrix, 0x19);
        break;
    case 56:
        CardDispComm(matrix, 12);
        break;
    case 58:
        CardDispComm(matrix, 0x1A);
        break;
    }
}

extern CHPTitleList D_00A578C8;
extern CHPTitleList D_00A57B38;
extern s8 SkipStartList[7];
extern u8 SkipPageLst[7][14];
extern s8 PageStatusLst[][3];
extern u8 *CardHelpTxtLst[];
extern u8 *CardTitleTxtLst[];
extern char *CardSkipTitleTxtLst[];
extern char *CardBookMarkTxtLst[];
const char D_00A4E2F0[] = "\x19\x03";
const char D_00A4E2F8[] = "\r";
const char D_00A4E300[] = "Help Index";
const char D_00A4E310[] = "Topics";
const char D_00A4E318[] = "";
extern void xglFontPrint(int x, int y, int color, const char *text);
extern void xglSoundEffectNormalID(int sound_id, int variant);
extern void CardStrListDisp(void *w, char **strs, int sel);
extern void CardDispTxtSub(u16 x, u16 y, unsigned char *text, u16 color);
extern void CardDispTxtSub2(u16 x, u16 y, unsigned char *text);
#define HELP_CUR(w) ((w)->helpTopic)

void CardHelpProc(CardGameWork *work)
{
    Matrix4 m;
    Matrix4 m2;
    float trans[4];
    float scale[4];
    u16 pressed;
    u16 dirs;
    int card;
    int q;
    u8 page;

    xglMatrixUnit(m);
    scale[0] = scale[1] = 0.97f;
    scale[3] = 1.0f;
    scale[2] = 1.0f;
    trans[0] = trans[1] = trans[2] = 0.0f;
    trans[3] = 1.0f;
    xglMatrixScale(m, m, scale);

    pressed = work->command.input[0].pressed;
    dirs = work->command.input[0].directions;
    if (work->menuPlayer != 0) {
        pressed = work->command.input[1].pressed;
        dirs = work->command.input[1].directions;
    }

    switch (work->helpState & 0xF0) {
    case 0x30:
        CHPMenuInitSub(&D_00A578C8, &D_00A57B38);
        work->helpState = 0x10;
    case 0x10:
        if (pressed & 0x40) {
            xglSoundEffectNormalID(2, 0);
            work->helpState = 1;
        }
        if (dirs & 0x1000) {
            if (D_00A578C8.flagC != 0) {
                xglSoundEffectNormalID(3, 0);
                D_00A578C8.flagC--;
                CHPTitleInitSub(&D_00A57B38, D_00A578C8.flagC);
            }
        } else if (dirs & 0x4000) {
            if (D_00A578C8.flagC < 6) {
                xglSoundEffectNormalID(3, 0);
                D_00A578C8.flagC++;
                CHPTitleInitSub(&D_00A57B38, D_00A578C8.flagC);
            }
        } else if (pressed & 0x2020) {
            xglSoundEffectNormalID(1, 0);
            work->helpState = 0x20;
        }
        xglFontPrint(0, 0, 0, D_00A4E2F0);
        xglFontPrint(6, 0x19, 0xFFF0, D_00A4E2F8);
        xglFontPrint(6, 0x19, 0xFFF0, D_00A4E300);
        xglFontPrint(0x10A, 0x19, 0xFFF0, D_00A4E310);
        CardStrListDisp(&D_00A578C8, CardBookMarkTxtLst, D_00A578C8.flagC);
        CardStrListDisp(&D_00A57B38, &CardSkipTitleTxtLst[SkipStartList[D_00A578C8.flagC]], -1);
        CHPMenuBackDispSub(m);
        break;
    case 0x20:
        if (pressed & 0x8040) {
            xglSoundEffectNormalID(2, 0);
            work->helpState = 0x10;
        }
        if (dirs & 0x1000) {
            if (D_00A57B38.flagC != 0) {
                xglSoundEffectNormalID(3, 0);
                D_00A57B38.flagC--;
            }
        } else if (dirs & 0x4000) {
            if (D_00A57B38.flagC < D_00A57B38.count - 1) {
                xglSoundEffectNormalID(3, 0);
                D_00A57B38.flagC++;
            }
        } else if (pressed & 0x20) {
            xglSoundEffectNormalID(1, 0);
            page = SkipPageLst[D_00A578C8.flagC][D_00A57B38.flagC];
            work->helpState = 1;
            HELP_CUR(work) = page;
            if (work->activeHelpPage < 100) {
                work->activeHelpPage++;
            }
            work->helpPage[work->activeHelpPage] = HELP_CUR(work);
        }
        CardStrListDisp(&D_00A578C8, CardBookMarkTxtLst, D_00A578C8.flagC);
        CardStrListDisp(&D_00A57B38, &CardSkipTitleTxtLst[SkipStartList[D_00A578C8.flagC]], D_00A57B38.flagC);
        xglFontPrint(0, 0, 0, D_00A4E2F0);
        xglFontPrint(6, 0x19, 0xFFF0, D_00A4E2F8);
        xglFontPrint(6, 0x19, 0xFFF0, D_00A4E300);
        xglFontPrint(0x10A, 0x19, 0xFFF0, D_00A4E310);
        CHPMenuBackDispSub(m);
        break;
    default:
        if (pressed & 8) {
            xglSoundEffectNormalID(3, 0);
            HELP_CUR(work)++;
            if (HELP_CUR(work) > 0x3A) {
                HELP_CUR(work) = 0;
            }
        } else if (pressed & 4) {
            xglSoundEffectNormalID(3, 0);
            HELP_CUR(work)--;
            if (HELP_CUR(work) < 0) {
                HELP_CUR(work) = 0x3A;
            }
        } else {
            goto other;
        }
        if (work->activeHelpPage < 100) {
            work->activeHelpPage++;
        }
        work->helpPage[work->activeHelpPage] = HELP_CUR(work);
        goto disp;
    other:
        if (pressed & 0x80) {
            CHPMenuInitSub(&D_00A578C8, &D_00A57B38);
            xglSoundEffectNormalID(1, 0);
            work->helpState = 0x10;
        } else if (pressed & 0x40) {
            xglSoundEffectNormalID(2, 0);
            if (work->activeHelpPage == 0) {
                work->helpState = 0;
            } else {
                work->activeHelpPage--;
                HELP_CUR(work) = work->helpPage[work->activeHelpPage];
            }
        }
    disp:
        xglFontPrint(6, 0x19, 0xFFF0, D_00A4E318);
        CardDispTxtSub(6, 0x19, CardTitleTxtLst[HELP_CUR(work)], 0xC);
        CardDispTxtSub2(6, 0x32, CardHelpTxtLst[HELP_CUR(work)]);
        CardDispTxtSub(0x11E, 0x19, (unsigned char *)CardBookMarkTxtLst[PageStatusLst[HELP_CUR(work)][0]], 0xC);
        CardDispHelpCard(HELP_CUR(work), m);

        trans[1] = -1.496f;
        card = 0x1C8A800 + PageStatusLst[HELP_CUR(work)][0] * 0x400;
        trans[2] = 0.0f;
        trans[3] = 1.0f;
        scale[1] = 0.9f;
        scale[0] = 0.9f;
        trans[0] = 1.0064001f;
        scale[3] = 1.0f;
        scale[2] = 1.0f;
        xglMatrixTrans(m2, m, trans);
        xglMatrixScale(m2, m2, scale);
        nmlModelSetPlace((Vector4 *)m2);
        nmlModelSetTexture((const char *)0x1C990A0);
        nmlModelSetRenderLevel(2);
        nmlModelEntryCard((void *)card);

        q = (PageStatusLst[HELP_CUR(work)][1] + 1) / 10;
        if (q != 0) {
            trans[0] = 1.3564f;
            card = 0x1C8A400 + q * 0x400;
            xglMatrixTrans(m2, m, trans);
            xglMatrixScale(m2, m2, scale);
            nmlModelSetPlace((Vector4 *)m2);
            nmlModelSetTexture((const char *)0x1C990A0);
            nmlModelSetRenderLevel(2);
            nmlModelEntryCard((void *)card);
        }
        card = 0x1C8A400 + ((PageStatusLst[HELP_CUR(work)][1] + 1) % 10) * 0x400;
        trans[0] = 1.4824001f;
        xglMatrixTrans(m2, m, trans);
        xglMatrixScale(m2, m2, scale);
        nmlModelSetPlace((Vector4 *)m2);
        nmlModelSetTexture((const char *)0x1C990A0);
        nmlModelSetRenderLevel(2);
        nmlModelEntryCard((void *)card);

        q = (HELP_CUR(work) + 1) / 10;
        if (q != 0) {
            trans[0] = 1.8304f;
            card = 0x1C8A400 + q * 0x400;
            xglMatrixTrans(m2, m, trans);
            xglMatrixScale(m2, m2, scale);
            nmlModelSetPlace((Vector4 *)m2);
            nmlModelSetTexture((const char *)0x1C990A0);
            nmlModelSetRenderLevel(2);
            nmlModelEntryCard((void *)card);
        }
        card = 0x1C8A400 + ((HELP_CUR(work) + 1) % 10) * 0x400;
        trans[0] = 1.9564f;
        xglMatrixTrans(m2, m, trans);
        xglMatrixScale(m2, m2, scale);
        nmlModelSetPlace((Vector4 *)m2);
        nmlModelSetTexture((const char *)0x1C990A0);
        nmlModelSetRenderLevel(2);
        nmlModelEntryCard((void *)card);

        trans[0] = -1.4184f;
        trans[1] = trans[2] = 0.0f;
        trans[3] = 1.0f;
        xglMatrixTrans(m2, m, trans);
        nmlModelSetTexture((const char *)0x1CB0000);
        nmlModelSetPlace((Vector4 *)m2);
        nmlModelSetRenderLevel(2);
        nmlModelEntryCard((void *)0x1CA1000);

        trans[1] = 1.832f;
        trans[0] = 1.4184f;
        xglMatrixTrans(m2, m, trans);
        nmlModelSetTexture((const char *)0x1CC0050);
        nmlModelSetPlace((Vector4 *)m2);
        nmlModelSetRenderLevel(2);
        nmlModelEntryCard((void *)0x1CA1C00);

        trans[0] = 1.4184f;
        trans[1] = -1.832f;
        xglMatrixTrans(m2, m, trans);
        nmlModelSetTexture((const char *)0x1CC0050);
        nmlModelSetPlace((Vector4 *)m2);
        nmlModelSetRenderLevel(2);
        nmlModelEntryCard((void *)0x1CA6000);
        break;
    }
}

/*
 * Clears the 256-byte help page-state buffer at +0x59c0..+0x5abf, then the
 * active-page byte at +0x59be. Both fall inside CardGameWork's unmodeled
 * +0x0002..+0x5af0 span (include/ov10/cgp.h, ov10/tu008); naming them there
 * needs that TU's own allocation, so this TU uses the named-offset fallback
 * instead of indexing the unmodeled byte array with raw literals.
 */
#define CGP_HELP_PAGE_ACTIVE(w)  (((u8 *)(w)) + 0x59BE)
#define CGP_HELP_PAGE_STATE(w)   (((u8 *)(w)) + 0x59C0)
#define CGP_HELP_PAGE_STATE_SIZE 256

void CardHelpPageInit(CardGameWork *work)
{
    int i;

    for (i = CGP_HELP_PAGE_STATE_SIZE - 1; i >= 0; i--) {
        CGP_HELP_PAGE_STATE(work)[i] = 0;
    }
    *CGP_HELP_PAGE_ACTIVE(work) = 0;
}

/* Help viewer text (help topics, titles, bookmarks) and tables. */
char Help1_01_txt[] = "This is a trading card game\nbased on the Xenosaga world.\n\nThere are more than 140 cards\nfrom which to create your\nown 40-card set (referred to\nas a \"deck\"), and you and an\nopponent will use these decks\nto battle.\n\nBe sure to play this card game\nto explore the Xenosaga world\nfurther!";
char Help1_02_txt[] = "\x0C\xC8\x08\x08\xA1\xFB \x0B\x19\x03\x0D\x00""Button: Select, hold card,\n                  place card, use\n                  card's power.\n\x0C(\x08\xFF\xA1\xDF \x0B\x19\x03\x0D\x00""Button: Cancel.\n\x0C@\xFF@\xA2\xA4 \x0B\x19\x03\x0D\x00""Button: View details of the\n                  selected card.\n\x0C\xC0(h\xA2\xA2 \x0B\x19\x03\x0D\x00""Button: End phase.\n\n\x0CXX\xC0START \x0B\x19\x03\x0D\x00""Button: Open menu.\n\x0CXX\xC0SELECT \x0B\x19\x03\x0D\x00""Button: View help.";
char Help1_03_txt[] = "The cards are divided up into\nthree major groups.\n\n\x0CXX\xC0""Battle card\x0B\x19\x03\x0D\x00: This card is used in\n                   the actual battles.\n\x0CXX\xC0""Event card\x0B\x19\x03\x0D\x00: Recover your cards\n                   or interfere with\n                   your opponent.\n\x0CXX\xC0Situation card\x0B\x19\x03\x0D\x00: Same as the\n                   event card, except\n                   the effect stays\n                   on the field.";
char Help1_04_txt[] = "The game progresses in\nphases and turns.\n\nMoreover, the type of card\ndictates where the card can\nbe used. The first player to\nrun out of cards in their\ndeck loses.\n\n\n\n\n\n\n\n\n\n                                   1/3";
char Help1_05_txt[] = "Xeno Card is played in terms\nof phases.\n\nA phase refers to a step in\nthe game, such as draw a card,\nmove a card, play a card, and\nbattle.\n\nThe current phase is displayed\nduring the game. Blue-colored\nphases are for Player 1\n(P1 phase), and red-colored\nphases are for Player 2\n(P2 phase).\n\n\n\n                                   2/3";
char Help1_06_txt[] = "A turn refers to one full cycle\nof phases.\n\nYour turn and the opponent's\nturn make up one turn.\n\n\n\n\n\n\n\n\n\n\n\n\n                                   3/3";
char Help2_01_txt[] = "There are many places where a\ncard can be placed on the play\nmat, but each card is required\nto be placed in a specific\narea.\n\nBoth sides of the play mat are\nidentical.\n\nFrom here on, directions will be\ngiven based on the P1 side. The\ngame progresses by placing\ncards onto the field according\nto each phase.";
char Help2_02_txt[] = "The deck is shuffled and\nplaced faced down where it\nsays DECK.\n\nNormally, one card is drawn\nfrom the deck per turn. If the\ndeck takes a direct hit, the\nsame number of cards as the\ndamage points must be taken\nfrom the deck and moved to\nthe lost pile.\n\nThe object of the game is to\nmake the opponent use up all\nof the cards in their deck.";
char Help2_03_txt[] = "The discard area is marked as\nLOST. Cards are discarded\nfrom the deck as a result of\nattacks by opponents or to\npay the cost of an action.\nThose cards are placed face\ndown in this area.\n\nWhen using a card's effect\nto recover the deck, cards\nfrom the lost pile are returned\nto the deck.";
char Help2_04_txt[] = "The player's cards in hand are\nplaced where it says HAND.\nThe hand starts with six cards.\nCards are played from this\narea. Normally, one card is\ndrawn from the deck and\nadded to the hand every turn.\nThen, the game is played with\nthe cards in hand.\n\nAt the end of a turn, the\nplayer can have a maximum\nof six cards in hand.\n\nIf a player has more than six\ncards, they must discard the\nextra cards.";
char Help2_05_txt[] = "The discarded cards are\nplaced where it says JUNK.\n\nCards destroyed by the\nopponent, used event cards,\nand extra cards in hand are\nplaced face up in this area.";
char Help2_06_txt[] = "The standby area is where the\nbattle cards are first moved\nto from the hand.\n\nA regular battle card can only\nbe placed in the standby area.\n\nYou can only put up to four\ncards in this area, so choose\nyour placement wisely.";
char Help2_07_txt[] = "The situation area is where\nthe situation cards are placed.\n\nYou can only put up to four\ncards in this area, so choose\nyour placement wisely.";
char Help2_08_txt[] = "The battlefield is where the\nbattle cards go into action.\nCards that are placed here will\nautomatically go into battle.\n\nStatus information of the\nbattle cards such as HP and\nATK, as well as counters for\ncards that attack once every\nother turn, are displayed here.";
char Help2_09_txt[] = "Except for the hand, junk pile,\ndeck, and lost area, the\nentire play mat is called the\nfield. Placing a card down from\nthe hand is referred to as,\n\"putting the card on the field.\"";
char Help3_01_txt[] = "There are three major types of\ncards. Each type of card is\nused in different ways.\n\nLet's look at the details.";
char Help3_02_txt[] = "The battle cards are the cards\nused in the actual battles.\n\nThe number of battle cards on\nthe field affect what cards\ncan be used.\n\n\x0C\xFF@@(See: Card: Requirements)\n\n\x0B\x19\x03\x0D\x00They are used by placing them\non the standby area and the\nbattlefield. You are able to\nmove these cards around.\n\nXeno Card cannot be played\nwithout the battle cards.";
char Help3_03_txt[] = "The event card is a vital card\nthat lets you do various things\nsuch as recover your cards'\nHP and interfere with your\nopponent.\n\nThere are many different ways\nof using this card. Once this\ncard is used, the card will be\nplaced in the junk pile.";
char Help3_04_txt[] = "Like the event card, the\nsituation card has many uses\nsuch as recovering your cards'\nHP and interfering with your\nopponent. When this card is\nused, it is placed on the\nsituation area. While the event\ncard gets moved to the junk\npile immediately after use, the\nsituation card stays on the\nfield.";
char Help3_05_txt[] = "Let's go over the information\nwritten on the cards.\n\nAll cards display the following:\n\x0CXX\xC0Type\nName\nRequirement\nCost\nText\n\n\x0B\x19\x03\x0D\x00In addition, the battle cards\ndisplay:\n\x0CXX\xC0""Attribute\nHP\nATK\nAttack Type";
char Help3_06_txt[] = "This shows the card type.\nThere are three types of cards:\nBattle, event, and situation.\n\n\x0C\xFF@@(See: Card: Battle card)\n(See: Card: Event card)\n(See: Card: Situation card)";
char Help3_07_txt[] = "This is the name of the card.\nOnly three cards with the\nsame name can be put in the\ndeck.\n\nAlso, when the battle cards\nhave proper nouns such as\nZiggy and Shion, you can only\nplace one of those cards on\nthe field at one time.\n\nIf the card is destroyed or\nmoved off the field, you are\nable to put another card down\nagain.";
char Help3_08_txt[] = "Each battle card has an\nattribute, marked by an icon:\n   \xA1\xE1\x0CXX\xC0\"H\" \x0B\x19\x03\x0D\x00Human\n    Shion and other people.\n   \xA1\xE1\x0CXX\xC0\"M\" \x0B\x19\x03\x0D\x00Mech\n    A.G.W.S. and battleships.\n   \xA1\xE1\x0CXX\xC0\"G\" \x0B\x19\x03\x0D\x00Gnosis\n    The Gnosis.\n   \xA1\xE1\x0CXX\xC0\"R\" \x0B\x19\x03\x0D\x00Realian\n    MOMO and other Realians.\n   \xA1\xE1\x0CXX\xC0\"W\" \x0B\x19\x03\x0D\x00Weapon\n    You can use these cards\n    together with other cards.\n   \xA1\xE1\x0CXX\xC0\"U\" \x0B\x19\x03\x0D\x00Unknown\n    Something unknown.";
char Help3_09_txt[] = "There are certain conditions\nthat need to be met prior to\nplaying a card.\n\nThose conditions are\nindicated with attribute marks.\nYou need to have cards with\nthe required attributes on your\nown battlefield or standby\narea.\n\nFor example, when putting\ndown a Lv 10 Shion card on the\nfield, three \"H\" cards must be\non the battlefield or the\nstandby area.\n\n                                   1/2";
char Help3_10_txt[] = "There are some special\nrequirements. For example,\nthe Bunnie card on the right\nshows an attribute condition\nthat looks like this:\n\nThis means that any attribute\nwill meet the condition. In this\ncase, there must be two\nbattle cards on the field,\nregardless of the attribute.\nSome cards have the\nconditions written in the\ntext, so read carefully.\n\n\n\n                                   2/2";
char Help3_11_txt[] = "You need to pay a cost to\nplay your cards on the field.\n\nThe number next to \"cost\"\nindicates the number of cards\nto be moved from the deck to\nthe lost pile. You need to be\ncareful here, because if you\nrun out of cards in the deck,\nyou lose.";
char Help3_12_txt[] = "The strength of the battle\ncards is shown next to \"HP.\"\n\nThe number decreases after\neach attack. Once it runs out,\nthe card is destroyed and\nmoved to the junk pile.\n\nIt is a good idea to maintain\nthe cards' HP by using cards\nthat recover HP. The HP number\ndisplayed on the card is the\nmaximum value.";
char Help3_13_txt[] = "ATK denotes attack power.\n\nThe number next to \"ATK\" is\nthe damage point the card can\ninflict.\n\nDamages decrease the\nopponent's battle card HP or\nthe number of cards in the\nopponent's deck.";
char Help3_14_txt[] = "The battle cards come with\ndifferent types of attack.\n\nThere are four types:\n\n\x0CXX\xC0Hand\n\x0B\x19\x03\x0D\x00""Attack enemy directly in front.\n\x0CXX\xC0""Ballistic\n\x0B\x19\x03\x0D\x00""Attack enemy directly in the\nline of fire. If no enemies are\nin the line of fire, attack\nopponent's deck.\n\x0CXX\xC0Spread\n\x0B\x19\x03\x0D\x00Inflict damage over opponent's\nentire battlefield.\n\x0CXX\xC0Homing\n\x0B\x19\x03\x0D\x00""Attack the deck directly.";
char Help3_15_txt[] = "Each card comes with some\ntext. You can view this text\nanytime during the game by\npressing the \x0C\x08@\x08\xA2\xA4 \x0B\x19\x03\x0D\x00""Button.\n\nThe text describes the special\npowers of each card.\n\nThe effects of the text have\npriority over the game rules.";
char Help4_01_txt[] = "This game progresses in\nphases and turns.\n\nIn the beginning, you and your\nopponent's decks are shuffled\nand placed face down on the\ndeck.\n\nWhen playing alone, you are\nPLAYER 1. When playing with\ntwo people, the player going\nfirst is \x0CXX\xC0PLAYER 1\x0B\x19\x03\x0D\x00, and the\nplayer following is \x0CXX\xC0PLAYER 2\x0B\x19\x03\x0D\x00.";
char Help4_02_txt[] = "As shown on the right, seven\nphases make up a turn.\n\nThe first player will see\n\x0CXX\xC0\xA3\xB1P TURN\x0B\x19\x03\x0D\x00, and the second player\nwill see \x0CXX\xC0\xA3\xB2P TURN\x0B\x19\x03\x0D\x00.\n\nSometimes both players' turns\nare combined and referred to\nas a \"turn.\"\n\nPlayers alternately go through\ntheir turns in the game.\n\nThe actions taken in each\nphase are identical for both\nplayers.\n                                   1/2";
char Help4_03_txt[] = "A player can differentiate\nbetween the player's own\nturn and the opponent's by\nreferring to them as \"my\nturn\" and \"opponent's turn.\"\n\nPhases can also be called\n\"my phase\" or \"opponent's\nphase.\"\n\n\n\n\n\n\n\n\n                                   2/2";
char Help4_04_txt[] = "The draw phase will run\nautomatically. One card is\ndrawn from the deck and\nadded to your hand. If there\nare no cards in the deck after\nthe card is drawn, you lose.\nEach player's draw phases\nare specified as \"my draw\nphase\" and \"opponent's draw\nphase.\"\n\nWhen it is not specified, the\nterm refers to both draw\nphases. The same applies for\nthe other phases.";
char Help4_05_txt[] = "In this phase, you can move\nthe battle cards in the\nstandby and battle areas.\nMoving a card to the\nbattlefield is called\ndeployment, and sending a\ncard back to the standby\narea is called withdrawing.\n\nPick a card by using the\nDirectional Buttons and\npressing the \x0C\xC8\x08\x08\xA1\xFB \x0B\x19\x03\x0D\x00""Button.\n\nUse the Directional Buttons to\nmove the card to the desired\nspot, then press the \x0C\xC8\x08\x08\xA1\xFB \x0B\x19\x03\x0D\x00""Button\nto finish the move.";
char Help4_06_txt[] = "You can use any number of\nevent cards from your hand\nduring this phase. Of course,\nyou can choose not to use\nany cards.\n\nPick an event card from the\nhand by using the Directional\nButtons, and pressing the\n\x0C\xC8\x08\x08\xA1\xFB \x0B\x19\x03\x0D\x00""Button.\n\nSpecific conditions must be\nmet in order to use the event\ncards.\n\n\x0C\xFF@@(See: Card: Condition)\n\n                                   \x0B\x19\x03\x0D\x00 1/3";
char Help4_07_txt[] = "Also, there are instances when\nthe opponent uses an event\ncard that disables your event\ncard and vice versa.\n\nWhen an event card is disabled,\nit is called \"event block.\"\nIt is also possible to block an\nevent block card.\n\nEvent cards can be blocked as\nlong as there are cards to\nblock them.\n\n\n\n\n                                   2/3";
char Help4_08_txt[] = "In addition to using an event\ncard, you can use the special\ncards on the field during the\nevent phase. These are the\ncards with text such as,\n\"Discard this card during the\nevent phase to...\"\n\nPick a special card by using\nthe Directional Buttons, and\npress the \x0C\xC8\x08\x08\xA1\xFB \x0B\x19\x03\x0D\x00""Button to use\nits special power. Then, follow\nthe directions on the screen.\n\n\n\n\n                                   3/3";
char Help4_09_txt[] = "You can put down a battle or\nsituation card in this phase.\nEach card's requirements\nmust be met in order to put\nthe card on the field.\n\n\x0C\xFF@@(See: Card: Condition)\n\n\x0B\x19\x03\x0D\x00You can only use one situation\ncard and one battle card per\nturn (phase).\n\n\n\n\n\n\n                                   1/2";
char Help4_10_txt[] = "To take a card out of your\nhand, choose the card by using\nthe Directional Buttons and\npress the \x0C\xC8\x08\x08\xA1\xFB \x0B\x19\x03\x0D\x00""Button.\n\nThe battle cards are\nautomatically placed on the\nstandby area, and the\nsituation cards are placed\non the situation area.\n\nThe exceptions are the cards\nwith G and W attributes. You\ncan select where to place\nthese cards.\n\n\n                                   2/2";
char Help4_11_txt[] = "In this phase, a player can use\nan event card during the\nopponent's turn. If the\nopponent has an event card\nthat can block that card and\nthe requirements are met,\nthe opponent can block that\nevent.\n\n\x0C\xFF@@(See: Card: Condition) \x0B\x19\x03\x0D";
char Help4_12_txt[] = "The battle between the battle\ncards run automatically.\n\nThe order of attack depends\non the attack type.\n\nThe order is Hand, Ballistic,\nSpread, and Homing. If the\nattack types are the same,\nthey go in the order displayed\non the battlefield from 1 to 4.\n\n\x0C\xFF@@(See: Card: Type of Attack)\n\n\n\n\n                                   \x0B\x19\x03\x0D\x00 1/2";
char Help4_13_txt[] = "If the HP of a battle card\nreaches 0 after a battle, it is\nconsidered destroyed and is\nplaced in the junk pile.\n\nIf the deck receives damage,\nthe same number of cards as\nthe damage points is taken\nfrom the deck. If there are\nno more cards in the deck,\nyou lose.\n\nThe key to the game is trying\nto keep the deck from being\nattacked and the battle cards\nfrom being destroyed.\n\n                                   2/2";
char Help4_14_txt[] = "If there are more than six\ncards in your hand, you must\ndiscard the extra cards.\n\nIf the game lets you draw a\ncard at the end of a turn, the\ncards in hand are adjusted\nafter drawing the card.\n\nThis marks the end of the turn.\nThe opponent's turn will run\nthe same way.";
char Help5_01_txt[] = "We have explained how to use\nmost of the cards.\n\n\x0C\xFF@@(See: Card)\n(See: Play Order)\n\n\x0B\x19\x03\x0D\x00We will now explain how to use\ntwo types of special cards:\nW and G battle cards.";
char Help5_02_txt[] = "Unlike other battle cards, the\nweapon card cannot be used\nby itself.\n\nWhen a compatible battle card\nis on the field, you can combine\nit with that battle card\n(equip). You can put the card\non either the standby area or\nthe battlefield.\n\nOnce the weapon is equipped,\nit cannot be taken off unless\nit is replaced by another\nweapon.\n\n\n                                   1/2";
char Help5_03_txt[] = "To use the weapon card, pick\nthe card from the hand using\nthe Directional Buttons and\npress the \x0C\xC8\x08\x08\xA1\xFB \x0B\x19\x03\x0D\x00""Button. Then, pick\nthe cards you want to equip\nfrom the battlefield or standby\narea and press the \x0C\xC8\x08\x08\xA1\xFB \x0B\x19\x03\x0D\x00""Button.\n\nWhen the battle card is\nequipped with a weapon card,\nthe attack power of the battle\ncard is lost, and only the\nweapon's attack power is used.\n\n\n\n\n                                   2/2";
char Help5_04_txt[] = "Unlike other cards,   \nG battle cards (Gnosis) can\nbe placed in areas other than\nthe standby area.\n\nThe drawback is that a\nnumber of battle cards must\nbe discarded in order to set it.\n\nThe number of cards to be\ndiscarded is displayed in the\ntext. Follow the directions on\nthe screen and select the\ncards to be discarded. Then,\nselect the area to place\nthe card.";
char Help6_01_txt[] = "We will now explain some\nspecial terms:\n\nPenetrate\n\nDown\n\nE mark\n\nEvent block";
char Help6_02_txt[] = "The text on a battle card may\ninclude the word \"penetrate.\"\n\nNormally during a battle, an\nattack cannot go beyond\nthe opponent being hit. If it\nsays, \"penetrate,\" however,\nany extra damage points will\ngo through the opponent to\nthe cards or the deck behind it.";
char Help6_03_txt[] = "The text on a battle card may\ninclude the words \"down card\"\nor \"down opponent's card.\"\n\nA card in a downed state will\ndisplay a down mark.\n\nA downed card cannot attack\nand its text effect is\nneutralized. A downed card\nreturns to normal at the end\nof your battle phase.";
char Help6_04_txt[] = "A battle card or situation card\nthat was just placed on the\nbattlefield will display an\nE mark.\n\nThe E mark disappears right\nbefore your battle phase.\nThese cards are at risk\nbecause there are attacks\nthat specifically target cards\nwith this mark.\n\nCards with an E mark cannot\nmove, and their text are\nineffective.";
char Help6_05_txt[] = "Neutralizing an event card is\ncalled \"event block.\"\n\nThe two cards that can block\nevents are \"Miyuki's Email\"\nand \"Song of the Mysterious\nGirl.\"\n\nIt is also possible to block an\nevent block. When this occurs,\nthe card that was blocked\nfirst will be enabled. Players\ncan keep blocking events as\nlong as these cards are\navailable.";
char Help7_01_txt[] = "The cards used in this game\ncan be obtained at certain\nshops.\n\nObtained cards must be\nopened before they can be\nused. Select \"Open Card\"\nfrom the card game menu to\nopen new card packs. \n\nOne thing to keep in mind is\nthat you can only have three\nof each card -- any\nextras will be sold back.";
char Help7_02_txt[] = "You can see all of your cards\nby selecting \"View Cards.\"\n\nThere are a total of 148 cards,\nso try to collect them all!";
char Help7_03_txt[] = "Creating a good deck is\ncrucial to winning this game.\nThe window on the left shows\nthe cards you possess, and\nthe window on the right\ndisplays the deck being\ncreated.\n\nSelect cards by using the\nDirectional Buttons, and press\nthe \x0C\xC8\x08\x08\xA1\xFB \x0B\x19\x03\x0D\x00""Button to add cards\nor the \x0C(\x08\xFF\xA1\xDF \x0B\x19\x03\x0D\x00""Button to\nsubtract cards.";
char Help7_04_txt[] = "You can save up to five decks\ncreated during the game.\nThese decks are not saved on\nthe Memory Card (PS2) during\nthe card game, so the data\nwill be lost if the power is\nturned off or the RESET\nButton is pressed.\n\nTo save the deck information\non the Memory Card (PS2), you\nmust go to a save point and\nsave the game.";
char Help7_05_txt[] = "When playing 2-player battle,\nthe second player's moves\ncan be controlled using the 2P\ncontroller.\n\nWhen a player is looking\nat their cards, the other player\nshould try to look away.";
char Help7_06_txt[] = "Before starting a card\ngame, you must select a\ndeck.\n\nUse the Directional Buttons\nto choose a deck. A deck is\nchosen from the five decks\nthat are already registered.\n\n\x0C\xFF@@(See: Others: Create Deck)";
char Title1_01_txt[] = "WHAT IS XENO CARD?";
char Title1_02_txt[] = "CONTROLS";
char Title1_03_txt[] = "CARD OUTLINE";
char Title1_04_txt[] = "GAME PROGRESSION";
char Title1_05_txt[] = "GAME PROGRESSION: PHASE";
char Title1_06_txt[] = "GAME PROGRESSION: TURN";
char Title2_01_txt[] = "CARD POSITIONING";
char Title2_02_txt[] = "DECK";
char Title2_03_txt[] = "LOST PILE";
char Title2_04_txt[] = "HAND";
char Title2_05_txt[] = "JUNK PILE";
char Title2_06_txt[] = "STANDBY AREA";
char Title2_07_txt[] = "SITUATION AREA";
char Title2_08_txt[] = "BATTLEFIELD";
char Title2_09_txt[] = "FIELD";
char Title3_01_txt[] = "CARD";
char Title3_02_txt[] = "BATTLE CARD";
char Title3_03_txt[] = "EVENT CARD";
char Title3_04_txt[] = "SITUATION CARD";
char Title3_05_txt[] = "DETAILS";
char Title3_06_txt[] = "TYPES";
char Title3_07_txt[] = "NAMES";
char Title3_08_txt[] = "ATTRIBUTES";
char Title3_09_txt[] = "REQUIREMENTS";
char Title3_10_txt[] = "REQUIREMENTS";
char Title3_11_txt[] = "COST";
char Title3_12_txt[] = "HP";
char Title3_13_txt[] = "ATK";
char Title3_14_txt[] = "ATTACK TYPE";
char Title3_15_txt[] = "TEXT";
char Title4_01_txt[] = "HOW TO PLAY";
char Title4_02_txt[] = "1P AND 2P TURNS";
char Title4_03_txt[] = "1P AND 2P TURNS";
char Title4_04_txt[] = "DRAW PHASE";
char Title4_05_txt[] = "MOVE PHASE";
char Title4_06_txt[] = "EVENT PHASE";
char Title4_07_txt[] = "EVENT PHASE";
char Title4_08_txt[] = "EVENT PHASE";
char Title4_09_txt[] = "SET PHASE";
char Title4_10_txt[] = "SET PHASE";
char Title4_11_txt[] = "BLOCK PHASE";
char Title4_12_txt[] = "BATTLE PHASE";
char Title4_13_txt[] = "BATTLE PHASE";
char Title4_14_txt[] = "ADJUST PHASE";
char Title5_01_txt[] = "SPECIAL CARD";
char Title5_02_txt[] = "BATTLE CARD: WEAPONS";
char Title5_03_txt[] = "BATTLE CARD: WEAPONS";
char Title5_04_txt[] = "BATTLE CARD: GNOSIS";
char Title6_01_txt[] = "TERMINOLOGY";
char Title6_02_txt[] = "PENETRATE";
char Title6_03_txt[] = "DOWN";
char Title6_04_txt[] = "E MARK";
char Title6_05_txt[] = "EVENT BLOCK";
char Title7_01_txt[] = "OPENING CARDS";
char Title7_02_txt[] = "VIEWING CARDS";
char Title7_03_txt[] = "CREATING DECKS";
char Title7_04_txt[] = "SAVING DECKS";
char Title7_05_txt[] = "2-PLAYER BATTLE";
char Title7_06_txt[] = "SELECTING DECKS";
char BookMark1_txt[] = "XENO CARD OUTLINE";
char BookMark2_txt[] = "CARD POSITIONING";
char BookMark3_txt[] = "CARDS";
char BookMark4_txt[] = "HOW TO PLAY";
char BookMark5_txt[] = "SPECIAL CARDS";
char BookMark6_txt[] = "TERMINOLOGY";
char BookMark7_txt[] = "OTHERS";
u8 *CardHelpTxtLst[59] = {
    (u8 *)Help1_01_txt,
    (u8 *)Help1_02_txt,
    (u8 *)Help1_03_txt,
    (u8 *)Help1_04_txt,
    (u8 *)Help1_05_txt,
    (u8 *)Help1_06_txt,
    (u8 *)Help2_01_txt,
    (u8 *)Help2_02_txt,
    (u8 *)Help2_03_txt,
    (u8 *)Help2_04_txt,
    (u8 *)Help2_05_txt,
    (u8 *)Help2_06_txt,
    (u8 *)Help2_07_txt,
    (u8 *)Help2_08_txt,
    (u8 *)Help2_09_txt,
    (u8 *)Help3_01_txt,
    (u8 *)Help3_02_txt,
    (u8 *)Help3_03_txt,
    (u8 *)Help3_04_txt,
    (u8 *)Help3_05_txt,
    (u8 *)Help3_06_txt,
    (u8 *)Help3_07_txt,
    (u8 *)Help3_08_txt,
    (u8 *)Help3_09_txt,
    (u8 *)Help3_10_txt,
    (u8 *)Help3_11_txt,
    (u8 *)Help3_12_txt,
    (u8 *)Help3_13_txt,
    (u8 *)Help3_14_txt,
    (u8 *)Help3_15_txt,
    (u8 *)Help4_01_txt,
    (u8 *)Help4_02_txt,
    (u8 *)Help4_03_txt,
    (u8 *)Help4_04_txt,
    (u8 *)Help4_05_txt,
    (u8 *)Help4_06_txt,
    (u8 *)Help4_07_txt,
    (u8 *)Help4_08_txt,
    (u8 *)Help4_09_txt,
    (u8 *)Help4_10_txt,
    (u8 *)Help4_11_txt,
    (u8 *)Help4_12_txt,
    (u8 *)Help4_13_txt,
    (u8 *)Help4_14_txt,
    (u8 *)Help5_01_txt,
    (u8 *)Help5_02_txt,
    (u8 *)Help5_03_txt,
    (u8 *)Help5_04_txt,
    (u8 *)Help6_01_txt,
    (u8 *)Help6_02_txt,
    (u8 *)Help6_03_txt,
    (u8 *)Help6_04_txt,
    (u8 *)Help6_05_txt,
    (u8 *)Help7_01_txt,
    (u8 *)Help7_02_txt,
    (u8 *)Help7_03_txt,
    (u8 *)Help7_04_txt,
    (u8 *)Help7_05_txt,
    (u8 *)Help7_06_txt
};
u8 *CardTitleTxtLst[59] = {
    (u8 *)Title1_01_txt,
    (u8 *)Title1_02_txt,
    (u8 *)Title1_03_txt,
    (u8 *)Title1_04_txt,
    (u8 *)Title1_05_txt,
    (u8 *)Title1_06_txt,
    (u8 *)Title2_01_txt,
    (u8 *)Title2_02_txt,
    (u8 *)Title2_03_txt,
    (u8 *)Title2_04_txt,
    (u8 *)Title2_05_txt,
    (u8 *)Title2_06_txt,
    (u8 *)Title2_07_txt,
    (u8 *)Title2_08_txt,
    (u8 *)Title2_09_txt,
    (u8 *)Title3_01_txt,
    (u8 *)Title3_02_txt,
    (u8 *)Title3_03_txt,
    (u8 *)Title3_04_txt,
    (u8 *)Title3_05_txt,
    (u8 *)Title3_06_txt,
    (u8 *)Title3_07_txt,
    (u8 *)Title3_08_txt,
    (u8 *)Title3_09_txt,
    (u8 *)Title3_10_txt,
    (u8 *)Title3_11_txt,
    (u8 *)Title3_12_txt,
    (u8 *)Title3_13_txt,
    (u8 *)Title3_14_txt,
    (u8 *)Title3_15_txt,
    (u8 *)Title4_01_txt,
    (u8 *)Title4_02_txt,
    (u8 *)Title4_03_txt,
    (u8 *)Title4_04_txt,
    (u8 *)Title4_05_txt,
    (u8 *)Title4_06_txt,
    (u8 *)Title4_07_txt,
    (u8 *)Title4_08_txt,
    (u8 *)Title4_09_txt,
    (u8 *)Title4_10_txt,
    (u8 *)Title4_11_txt,
    (u8 *)Title4_12_txt,
    (u8 *)Title4_13_txt,
    (u8 *)Title4_14_txt,
    (u8 *)Title5_01_txt,
    (u8 *)Title5_02_txt,
    (u8 *)Title5_03_txt,
    (u8 *)Title5_04_txt,
    (u8 *)Title6_01_txt,
    (u8 *)Title6_02_txt,
    (u8 *)Title6_03_txt,
    (u8 *)Title6_04_txt,
    (u8 *)Title6_05_txt,
    (u8 *)Title7_01_txt,
    (u8 *)Title7_02_txt,
    (u8 *)Title7_03_txt,
    (u8 *)Title7_04_txt,
    (u8 *)Title7_05_txt,
    (u8 *)Title7_06_txt
};
char *CardSkipTitleTxtLst[52] = {
    Title1_01_txt,
    Title1_02_txt,
    Title1_03_txt,
    Title1_04_txt,
    Title1_05_txt,
    Title1_06_txt,
    Title2_01_txt,
    Title2_02_txt,
    Title2_03_txt,
    Title2_04_txt,
    Title2_05_txt,
    Title2_06_txt,
    Title2_07_txt,
    Title2_08_txt,
    Title2_09_txt,
    Title3_01_txt,
    Title3_02_txt,
    Title3_03_txt,
    Title3_04_txt,
    Title3_05_txt,
    Title3_06_txt,
    Title3_07_txt,
    Title3_08_txt,
    Title3_09_txt,
    Title3_11_txt,
    Title3_12_txt,
    Title3_13_txt,
    Title3_14_txt,
    Title3_15_txt,
    Title4_01_txt,
    Title4_02_txt,
    Title4_04_txt,
    Title4_05_txt,
    Title4_06_txt,
    Title4_09_txt,
    Title4_11_txt,
    Title4_12_txt,
    Title4_14_txt,
    Title5_01_txt,
    Title5_02_txt,
    Title5_04_txt,
    Title6_01_txt,
    Title6_02_txt,
    Title6_03_txt,
    Title6_04_txt,
    Title6_05_txt,
    Title7_01_txt,
    Title7_02_txt,
    Title7_03_txt,
    Title7_04_txt,
    Title7_05_txt,
    Title7_06_txt
};
char *CardBookMarkTxtLst[7] = {
    BookMark1_txt,
    BookMark2_txt,
    BookMark3_txt,
    BookMark4_txt,
    BookMark5_txt,
    BookMark6_txt,
    BookMark7_txt
};
u32 cardlex_tbl[64] = {0};
const char *cardxtx_tbl[64] = {0};

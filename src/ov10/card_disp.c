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
extern const u32 cardlex_tbl[];

extern const CardHelpPackedVector D_00A4E0B0;
extern const CardHelpPackedVector D_00A4E0C0;
extern void nmlModelSetLight(Matrix4 color, Matrix4 direction);
extern float xglSin(float angle);
extern int ChangeCnt;
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

extern char D_00A4DDB0[];
extern char *D_00A4DF60[];
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

extern short list16_0[];
extern short list17_1[];
extern short list18_2[];
extern float trans_3[];

void CardDispHelpSlide1(char mode, Matrix4 matrix)
{
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
        xglMatrixTrans(transformed, matrix, trans_3);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list16_0[idx], 1, 0);
        break;
    case 17:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, trans_3);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list17_1[idx], 1, 0);
        break;
    case 18:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, trans_3);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list18_2[idx], 1, 0);
        break;
    }
}

typedef struct { float x, y, z, w; } __attribute__((aligned(16))) Slide2Vec;
extern const CardHelpPackedVector D_00A4E000;
extern short list19_4[];
extern short list20_5[];
extern short list21_6[];
extern short list22_7[];
extern short list25_8[];
extern short list26_9[];
extern short list27_10[];
extern short list28_11[];
extern short list45_12[];
extern short list46_13[];
extern short list47_14[];

void CardDispHelpSlide2(char mode, Matrix4 matrix)
{
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
        CardDisp(transformed, list19_4[idx], 1, 0);
        break;
    case 20:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list20_5[idx], 1, 0);
        break;
    case 21:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list21_6[idx], 1, 0);
        break;
    case 22:
        idx = (ChangeCnt = ChangeCnt % 384) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDispLight(transformed, list22_7[idx], 1, 0x10);
        break;
    case 25:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDispLight(transformed, list25_8[idx], 1, 8);
        break;
    case 26:
        idx = (ChangeCnt = ChangeCnt % 384) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDispLight(transformed, list26_9[idx], 1, 2);
        break;
    case 27:
        idx = (ChangeCnt = ChangeCnt % 384) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDispLight(transformed, list27_10[idx], 1, 4);
        break;
    case 28:
        idx = (ChangeCnt = ChangeCnt % 256) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDispLight(transformed, list28_11[idx], 1, 0x40);
        break;
    case 45:
        idx = (ChangeCnt = ChangeCnt % 384) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list45_12[idx], 1, 0);
        break;
    case 46:
        idx = (ChangeCnt = ChangeCnt % 384) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list46_13[idx], 1, 0);
        break;
    case 47:
        idx = (ChangeCnt = ChangeCnt % 256) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list47_14[idx], 1, 0);
        break;
    }
}

typedef struct { float x, y, z, w; } __attribute__((aligned(16))) Slide3Vec;
extern const CardHelpPackedVector D_00A4E090;
extern short list23_15[];
extern short list24_16[];

void CardDispHelpSlide3(char mode, Matrix4 matrix)
{
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
        CardDisp(transformed, list23_15[idx], 1, 0);
        break;
    case 24:
        idx = (ChangeCnt = ChangeCnt % 192) / 64;
        xglMatrixTrans(transformed, matrix, &translation.x);
        xglMatrixScale(transformed, transformed, &scale.x);
        CardDisp(transformed, list24_16[idx], 1, 0);
        break;
    }
}

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
extern const CardHelpPackedVector D_00A4E0F0;
extern const CardHelpPackedVector D_00A4E100;
extern const CardHelpPackedVector D_00A4E110;

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

extern const CardHelpPackedVector D_00A4E140;
extern const CardHelpPackedVector D_00A4E150;

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

extern const CardHelpPackedVector D_00A4E1B0;

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
extern const CardHelpPackedVector D_00A4E1E0;
extern const CardHelpPackedVector D_00A4E1F0;

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
extern const char D_00A4E2F0[];
extern const char D_00A4E2F8[];
extern const char D_00A4E300[];
extern const char D_00A4E310[];
extern const char D_00A4E318[];
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

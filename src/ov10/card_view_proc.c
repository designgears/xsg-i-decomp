/*
 * OV10 original TU 5: 0x00a1c1b8..0x00a1e5d8 (5 functions)
 */
#include "common.h"

INCLUDE_ASM("asm/nonmatchings/ov10/card_view_proc", CardViewProc);

extern void xglMatrixScale(float (*dst)[4], float (*src)[4], float *v);
extern void xglMatrixTrans(float (*dst)[4], float (*src)[4], float *v);
extern void CardDispCard(float (*place)[4], short no, short flag);
extern void CardDisp(float (*place)[4], short no, short flag, short disp);
extern void nmlModelSetLight(void *color, void *direction);
extern void nmlModelSetPlace(float (*matrix)[4]);
extern void nmlModelSetTexture(void *texture);
extern void nmlModelSetRenderLevel(int level);
extern void nmlModelEntryCard(void *model);
extern float CCColor[4][4];
extern float asDir[4][4];

void COPCardDispSub(int flag, int unused, float (*src)[4], short *cards, unsigned char count, int sel)
{
    float mat[4][4];
    float trans[4];
    float scale[4];
    int n = count;
    int i;
    float step = 3.9757f / (float)(n - 1);

    trans[1] = trans[2] = 0.0f;
    if (n != 0) {
        for (i = 0; i < n; i++) {
            trans[0] = step * (float)i + -1.98785f;
            trans[2] = (float)(i + 1) * 0.015f;
            if (i == sel) {
                trans[2] = (float)(n + 1) * 0.02f;
            }
            trans[3] = 1.0f;
            scale[1] = 0.4f;
            scale[0] = 0.4f;
            scale[3] = 1.0f;
            scale[2] = 1.0f;
            xglMatrixTrans(mat, src, trans);
            xglMatrixScale(mat, mat, scale);
            if (i == sel) {
                CardDispCard(mat, cards[sel], 1);
                nmlModelSetLight(CCColor, asDir);
                nmlModelSetPlace(mat);
                nmlModelSetTexture((void *)0x01D60000);
                nmlModelEntryCard((void *)0x01D12000);
            } else {
                CardDispCard(mat, cards[i], 1);
            }
        }
        if (flag != 0) {
            trans[1] = 1.4f;
            trans[2] = 0.01f;
            trans[0] = 0.0f;
            scale[1] = 0.85f;
            scale[3] = 1.0f;
            scale[0] = 0.85f;
            scale[2] = 1.0f;
            xglMatrixTrans(mat, src, trans);
            xglMatrixScale(mat, mat, scale);
            CardDisp(mat, cards[sel], 1, 1);
        }
    }
    scale[3] = 1.0f;
    scale[1] = 0.4f;
    scale[2] = 1.0f;
    scale[0] = 1.0f;
    xglMatrixScale(mat, src, scale);
    nmlModelSetPlace(mat);
    nmlModelSetTexture((void *)0x01DB5800);
    nmlModelSetRenderLevel(2);
    nmlModelEntryCard((void *)0x01DA5600);
}

int COPSerachPackR(unsigned char *pack_occupied, int current_position)
{
    int found_position = -1;
    int scan_position;

    for (scan_position = current_position + 1; scan_position < 20; scan_position += 1) {
        if (pack_occupied[scan_position] != 0) {
            found_position = scan_position;
            break;
        }
    }

    if (found_position < 0) {
        for (scan_position = 0; scan_position < current_position - 1; scan_position += 1) {
            if (pack_occupied[scan_position] != 0) {
                found_position = scan_position;
                break;
            }
        }
        if (found_position < 0) {
            found_position = current_position;
        }
    }

    return found_position;
}

int COPSerachPackL(unsigned char *pack_occupied, int current_position)
{
    int found_position = -1;
    int scan_position;

    for (scan_position = current_position - 1; scan_position >= 0; scan_position -= 1) {
        if (pack_occupied[scan_position] != 0) {
            found_position = scan_position;
            break;
        }
    }

    if (found_position < 0) {
        for (scan_position = 19; scan_position > current_position; scan_position -= 1) {
            if (pack_occupied[scan_position] != 0) {
                found_position = scan_position;
                break;
            }
        }
        if (found_position < 0) {
            found_position = current_position;
        }
    }

    return found_position;
}

INCLUDE_ASM("asm/nonmatchings/ov10/card_view_proc", CardOpenProc);

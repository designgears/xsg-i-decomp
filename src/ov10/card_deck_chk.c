/*
 * OV10 original TU 4: 0x00a19a20..0x00a1c1b8 (5 functions)
 */
#include "common.h"
#include "shared.h"

int CardDeckChk(s16 *cards) {
    enum { CARD_COUNT = 40, MAX_COPIES = 3 };
    int first_index;
    int second_index;
    int matching_copies;

    for (first_index = 0; first_index < CARD_COUNT; first_index++) {
        matching_copies = 0;

        for (second_index = first_index + 1; second_index < CARD_COUNT; second_index++) {
            if (cards[second_index] < 0) {
                return 0;
            }

            if (cards[first_index] == cards[second_index]) {
                matching_copies++;
                if (matching_copies >= MAX_COPIES) {
                    return 0;
                }
            }
        }
    }

    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov10/card_deck_chk", CardMakeDeckProc);

typedef struct {
    s16 id;
    s16 n;
} CsdEntry;

typedef struct {
    u8 pad0[4];
    signed char mode;
    signed char cursor;
    u8 pad6[6];
    s16 top;
    s16 columns;
    s16 rows;
    s16 count;
    CsdEntry entries[40];
} CsdList;

typedef struct {
    u8 pad0[0x2C];
    u8 deck[1][0x2A];
} CsdSave;

typedef struct {
    u8 pad0[8];
    void *definitions;
    CsdSave *save;
    s16 owned[1];
} CsdWork;

typedef struct {
    u8 pad0[0x28];
    u16 held;
    u16 pad2a;
    u16 trig;
} CsdPad;

extern CsdPad PadData;

extern char D_00A4DA68[];

extern char D_00A4DA70[];

extern char D_00A4DAA0[];

extern char D_00A4DAD0[];

extern char D_00A4DAD8[];

extern void xglSoundEffectNormalID(int sound_id, int variant);

extern void xglFontPrint(int x, int y, int color, const char *text);

extern void xglMatrixUnit(float matrix[4][4]);

extern void xglMatrixTrans(float (*destination)[4], float (*source)[4], const float translation[4]);

extern int CardGetDeckAllCnt(CsdList *list);

extern void CardDisp(float (*place)[4], short no, short flag, short disp);

extern void CardDeckListDisp(CsdList *list);

extern void CardDeckDisp(CsdList *list, void *info);

extern CsdList *D_00A5783C;
extern int ActiveCursol_2;
extern char D_00A4DA78[];
extern void xglMatrixScale(float (*destination)[4], float (*source)[4], const float scale[4]);

int CardLoadDeckProc(int exiting, CsdList *cursor, CsdList *list, CsdWork *work, char *title)
{
    float trans[4];
    float mat[4][4];
    float trans2[4];
    float scale[4];
    int ret;
    int disp;

    ((u8 *)work)[0x59BF] = 0x3A;
    if (ActiveCursol_2 != 0) {
        D_00A5783C = list;
        cursor->cursor |= 0x80;
        list->cursor &= 0x7F;
    } else {
        D_00A5783C = cursor;
        cursor->cursor &= 0x7F;
        list->cursor |= 0x80;
    }
    ret = 0;
    switch (*(s16 *)((u8 *)work + 0x5AC6)) {
    case 0:
        disp = 0;
        if ((PadData.held & 0x10) && exiting == 0 && ActiveCursol_2 != 0) {
            short no;
            disp = 1;
            D_00A5783C = list;
            trans[0] = -1.15f;
            trans[1] = 0.45f;
            trans[2] = 0.2f;
            trans[3] = 1.0f;
            xglMatrixUnit(mat);
            xglMatrixTrans(mat, mat, trans);
            no = D_00A5783C->entries[D_00A5783C->top + D_00A5783C->cursor].id;
            CardDisp(mat, no, work->owned[no] != 0, 1);
        }
        if (disp == 0 || ActiveCursol_2 == 0) {
            CardDeckListDisp(cursor);
        }
        CardDeckDisp(list, work->definitions);
        if (ActiveCursol_2 == 0) {
            if (exiting == 0 && disp == 0) {
                if (title != 0) {
                    xglFontPrint(0x18, 0x138, 0xFFF0, title);
                }
                xglFontPrint(0, 0, 0, D_00A4DA68);
                xglFontPrint(0x78, 0x120, 0xFFF0, D_00A4DA70);
                xglFontPrint(0x18, 0x150, 0xFFF0, D_00A4DA78);
                xglFontPrint(0x18, 0x168, 0xFFF0, D_00A4DAA0);
                xglFontPrint(0x78, 0x120, 0xFFF0, D_00A4DAD0);
                if (*(u16 *)((u8 *)work + 0x5AD2) & 0x20) {
                    xglSoundEffectNormalID(1, 0);
                    (*(s16 *)((u8 *)work + 0x5AC6))++;
                } else if (*(u16 *)((u8 *)work + 0x5AD2) & 0x40) {
                    xglSoundEffectNormalID(2, 0);
                    ret = -1;
                }
            }
        } else {
            int no;
            trans2[0] = 0.0f;
            trans2[1] = -0.3932f;
            trans2[2] = 0.1f;
            trans2[3] = 1.0f;
            scale[1] = 0.335f;
            scale[0] = 0.335f;
            scale[3] = 1.0f;
            scale[2] = 1.0f;
            xglMatrixUnit(mat);
            xglMatrixTrans(mat, mat, trans2);
            xglMatrixScale(mat, mat, scale);
            no = D_00A5783C->entries[D_00A5783C->top + D_00A5783C->cursor].id;
            CardDisp(mat, no, work->owned[no] != 0, exiting == 0);
        }
        if (exiting == 0) {
            if (PadData.trig & 0x1000) {
                xglSoundEffectNormalID(3, 0);
                D_00A5783C->mode = -1;
            } else if (PadData.trig & 0x4000) {
                xglSoundEffectNormalID(3, 0);
                D_00A5783C->mode = 1;
            }
            if (PadData.trig & 0x2000) {
                xglSoundEffectNormalID(3, 0);
                if (CardGetDeckAllCnt(list) != 0) {
                    ActiveCursol_2 = 1;
                    cursor->cursor |= 0x80;
                    list->cursor &= 0x7F;
                }
            } else if (PadData.trig & 0x8000) {
                xglSoundEffectNormalID(3, 0);
                ActiveCursol_2 = 0;
                cursor->cursor &= 0x7F;
                list->cursor |= 0x80;
            }
        }
        if (list->mode != 0) {
          if (list->mode == -1 || list->mode == 1) {
            if (list->mode < 0) {
                if (list->top > 0) {
                    if (list->cursor < 2) {
                        list->top--;
                    } else {
                        list->cursor--;
                        if (list->cursor < 0) {
                            list->cursor = 0;
                        }
                    }
                } else if (list->cursor > 0) {
                    list->cursor--;
                }
            } else {
                if (list->top + list->rows < list->count) {
                    if (list->cursor >= list->rows - 2) { list->top++; } else {
                        list->cursor++;
                    }
                } else if (list->cursor < list->rows - 1 && list->cursor < list->count - 1
                           && list->cursor < list->columns) {
                    list->cursor++;
                }
            }
          }
            list->mode = 0;
        }
        if (cursor->mode != 0) {
            cursor->count = 5;
            if (cursor->mode == -1 || cursor->mode == 1) {
                int i;
                int j;
                int k;
                int prev;

                if (cursor->mode < 0) {
                    if (cursor->top > 0) {
                        if (cursor->cursor < 2) {
                            cursor->top--;
                        } else {
                            cursor->cursor--;
                            if (cursor->cursor < 0) {
                                cursor->cursor = 0;
                            }
                        }
                    } else if (cursor->cursor > 0) {
                        cursor->cursor--;
                    }
                } else {
                    if (cursor->top + cursor->rows < 5) {
                        if (cursor->cursor >= cursor->rows - 2) { cursor->top++; } else {
                            cursor->cursor++;
                        }
                    } else if (cursor->cursor < cursor->rows - 1 && cursor->cursor < 4) {
                        cursor->cursor++;
                    }
                }
                prev = -1;
                j = -1;
                k = cursor->top + (cursor->cursor & 0x7F);
                for (i = 0; i < 40; i++) {
                    if (prev != work->save->deck[k][i]) {
                        j++;
                        list->entries[j].id = work->save->deck[k][i];
                        list->entries[j].n = 1;
                    } else {
                        list->entries[j].n++;
                    }
                    prev = work->save->deck[k][i];
                }
                list->cursor = 0x80;
                list->count = j + 1;
                list->top = 0;
            }
            cursor->mode = 0;
        }
        if (list->count - 1 < list->cursor) {
            list->cursor = list->count - 1;
        }
        break;
    case 1:
        CardDeckListDisp(cursor);
        CardDeckDisp(list, work->definitions);
        if (exiting == 0) {
            xglFontPrint(0, 0, 0, D_00A4DA68);
            xglFontPrint(0x78, 0x120, 0xFFF0, D_00A4DA70);
            xglFontPrint(0x18, 0x150, 0xFFF0, D_00A4DAD8);
            xglFontPrint(0x18, 0x168, 0xFFF0, D_00A4DAA0);
            xglFontPrint(0x78, 0x120, 0xFFF0, D_00A4DAD0);
            if (*(u16 *)((u8 *)work + 0x5AD2) & 0x20) {
                xglSoundEffectNormalID(1, 0);
                ret = (cursor->cursor & 0x7F) + 1;
            } else if (*(u16 *)((u8 *)work + 0x5AD2) & 0x40) {
                xglSoundEffectNormalID(2, 0);
                (*(s16 *)((u8 *)work + 0x5AC6))--;
            }
        }
        break;
    }
    return ret;
}

extern CsdList *D_00A57840;
extern int ActiveCursol_4;
extern char D_00A4DAF0[];
int CardSaveDeckProc(int exiting, CsdList *cursor, CsdList *list, CsdWork *work, char *title)
{
    float trans[4];
    float mat[4][4];
    int ret;
    int disp;

    ((u8 *)work)[0x59BF] = 0x38;
    if (ActiveCursol_4 != 0) {
        D_00A57840 = list;
        cursor->cursor |= 0x80;
        list->cursor &= 0x7F;
    } else {
        D_00A57840 = cursor;
        cursor->cursor &= 0x7F;
        list->cursor |= 0x80;
    }
    ret = 0;
    switch (*(s16 *)((u8 *)work + 0x5AC6)) {
    case 0:
        if (exiting == 0) {
            if (PadData.trig & 0x1000) {
                xglSoundEffectNormalID(3, 0);
                D_00A57840->mode = -1;
            }
            if (PadData.trig & 0x4000) {
                xglSoundEffectNormalID(3, 0);
                D_00A57840->mode = 1;
            }
            if (PadData.trig & 0x2000) {
                xglSoundEffectNormalID(3, 0);
                if (CardGetDeckAllCnt(list) != 0) {
                    ActiveCursol_4 = 1;
                    cursor->cursor |= 0x80;
                    list->cursor &= 0x7F;
                }
            }
            if (PadData.trig & 0x8000) {
                xglSoundEffectNormalID(3, 0);
                ActiveCursol_4 = 0;
                cursor->cursor &= 0x7F;
                list->cursor |= 0x80;
            }
        }
        if (list->mode != 0) {
          if (list->mode == -1 || list->mode == 1) {
            if (list->mode < 0) {
                if (list->top > 0) {
                    if (list->cursor < 2) {
                        list->top--;
                    } else {
                        list->cursor--;
                        if (list->cursor < 0) {
                            list->cursor = 0;
                        }
                    }
                } else if (list->cursor > 0) {
                    list->cursor--;
                }
            } else {
                if (list->top + list->rows < list->count) {
                    if (list->cursor >= list->rows - 2) { list->top++; } else {
                        list->cursor++;
                    }
                } else if (list->cursor < list->rows - 1 && list->cursor < list->count - 1
                           && list->cursor < list->columns) {
                    list->cursor++;
                }
            }
          }
            list->mode = 0;
        }
        if (cursor->mode != 0) {
            cursor->count = 5;
            if (cursor->mode == -1 || cursor->mode == 1) {
                u8 *deck;
                int i;
                int j;
                int k;
                int prev;

                if (cursor->mode < 0) {
                    if (cursor->top > 0) {
                        if (cursor->cursor < 2) {
                            cursor->top--;
                        } else {
                            cursor->cursor--;
                            if (cursor->cursor < 0) {
                                cursor->cursor = 0;
                            }
                        }
                    } else if (cursor->cursor > 0) {
                        cursor->cursor--;
                    }
                } else {
                    if (cursor->top + cursor->rows < 5) {
                        if (cursor->cursor >= cursor->rows - 2) { cursor->top++; } else {
                            cursor->cursor++;
                        }
                    } else if (cursor->cursor < cursor->rows - 1 && cursor->cursor < 4) {
                        cursor->cursor++;
                    }
                }
                prev = -1;
                j = -1;
                k = cursor->top + (cursor->cursor & 0x7F);
                for (i = 0; i < 40; i++) {
                    if (prev != work->save->deck[k][i]) {
                        j++;
                        list->entries[j].id = work->save->deck[k][i];
                        list->entries[j].n = 1;
                    } else {
                        list->entries[j].n++;
                    }
                    prev = work->save->deck[k][i];
                }
                list->cursor = 0x80;
                list->count = j + 1;
                list->top = 0;
            }
            cursor->mode = 0;
        }
        if (list->count - 1 < list->cursor) {
            list->cursor = list->count - 1;
        }
        disp = 0;
        if ((PadData.held & 0x10) && exiting == 0) {
            disp = 1;
            if (ActiveCursol_4 != 0) {
                short no;
                D_00A57840 = list;
                trans[0] = -1.15f;
                trans[1] = 0.45f;
                trans[2] = 0.2f;
                trans[3] = 1.0f;
                xglMatrixUnit(mat);
                xglMatrixTrans(mat, mat, trans);
                no = D_00A57840->entries[D_00A57840->top + D_00A57840->cursor].id;
                CardDisp(mat, no, work->owned[no] != 0, 1);
            }
        }
        if (disp == 0 || ActiveCursol_4 == 0) {
            CardDeckListDisp(cursor);
        }
        CardDeckDisp(list, work->definitions);
        if (ActiveCursol_4 == 0 && exiting == 0) {
            xglFontPrint(0, 0, 0, D_00A4DA68);
            xglFontPrint(0x78, 0x120, 0xFFF0, D_00A4DA70);
            if (title != 0) {
                xglFontPrint(0x18, 0x138, 0xFFF0, title);
            }
            xglFontPrint(0x18, 0x150, 0xFFF0, D_00A4DAF0);
            xglFontPrint(0x18, 0x168, 0xFFF0, D_00A4DAA0);
            xglFontPrint(0x78, 0x120, 0xFFF0, D_00A4DAD0);
            if (*(u16 *)((u8 *)work + 0x5AD2) & 0x20) {
                xglSoundEffectNormalID(1, 0);
                (*(s16 *)((u8 *)work + 0x5AC6))++;
            } else if (*(u16 *)((u8 *)work + 0x5AD2) & 0x40) {
                xglSoundEffectNormalID(2, 0);
                ret = -1;
            }
        }
        break;
    case 1:
        CardDeckListDisp(cursor);
        CardDeckDisp(list, work->definitions);
        if (exiting == 0) {
            xglFontPrint(0, 0, 0, D_00A4DA68);
            xglFontPrint(0x78, 0x120, 0xFFF0, D_00A4DA70);
            xglFontPrint(0x18, 0x150, 0xFFF0, D_00A4DAD8);
            xglFontPrint(0x18, 0x168, 0xFFF0, D_00A4DAA0);
            xglFontPrint(0x78, 0x120, 0xFFF0, D_00A4DAD0);
            if (*(u16 *)((u8 *)work + 0x5AD2) & 0x20) {
                xglSoundEffectNormalID(1, 0);
                ret = (cursor->cursor & 0x7F) + 1;
            } else if (*(u16 *)((u8 *)work + 0x5AD2) & 0x40) {
                xglSoundEffectNormalID(2, 0);
                (*(s16 *)((u8 *)work + 0x5AC6))--;
            }
        }
        break;
    }
    return ret;
}

INCLUDE_ASM("asm/nonmatchings/ov10/card_deck_chk", CardLoadDeckProc2);

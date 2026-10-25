/*
 * OV10 original TU 4: 0x00a19a20..0x00a1c1b8 (5 functions)
 */
#include "common.h"

char ZenkakuListA[20] = "\xA1\xA1" "1 2 3 4 5 6 7 8 9 ";
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

typedef struct {
    s16 id;
    s16 n;
} CsdEntry;

typedef struct {
    u8 pad0[2];
    signed char typeFilter;        /* +0x02: index into TypeMaskLst (list only) */
    signed char attrFilter;        /* +0x03: index into AttriMaskLst (list only) */
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
    u8 pad0[2];
    u16 cardCount;                 /* +0x02: entries in the definitions table */
    u8 pad4[4];
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

#define CARD_PAD_ATTR_PREV 0x0001      /* L2 */
#define CARD_PAD_ATTR_NEXT 0x0002      /* R2 */
#define CARD_PAD_TYPE_PREV 0x0004      /* L1 */
#define CARD_PAD_TYPE_NEXT 0x0008      /* R1 */
#define CARD_PAD_PREVIEW 0x0010        /* triangle, held: show the selected card large */
#define CARD_PAD_ACCEPT 0x0020         /* circle */
#define CARD_PAD_CANCEL 0x0040         /* cross */
#define CARD_PAD_UP 0x1000
#define CARD_PAD_RIGHT 0x2000
#define CARD_PAD_DOWN 0x4000
#define CARD_PAD_LEFT 0x8000
#define CARD_SE_ACCEPT 1
#define CARD_SE_CANCEL 2
#define CARD_SE_CURSOR 3
#define CARD_PROMPT_ACCEPT_CANCEL \
    "\r\x04\x0c\x80\x40\x40\xa1\xfb \x0b\x19\x03\r\0Accept\xa1\xa1\r\x04\x0c\x40\x40\x80\xa1\xdf \x0b\x19\x03\r\0Cancel"
#define CARD_HELP_TOPIC_MAKE_DECK 55
#define CARD_HELP_TOPIC_SAVE_DECK 56
#define CARD_HELP_TOPIC_LOAD_DECK 58
#define CARD_SCROLL_MARGIN 2
#define CARD_MODEL_RENDER_LEVEL 2
#define CARD_TYPE_MASK_B 0x01          /* icon type1_B */
#define CARD_TYPE_MASK_E 0x02          /* icon type1_E */
#define CARD_TYPE_MASK_S 0x04          /* icon type1_S */
#define CARD_ATTR_MASK_1 0x01
#define CARD_ATTR_MASK_2 0x02
#define CARD_ATTR_MASK_3 0x08
#define CARD_ATTR_MASK_4 0x04
#define CARD_ATTR_MASK_5 0x10
#define CARD_ATTR_MASK_6 0x60          /* two bits: the icon is chosen only on the combined mask */
#define CARD_PANE_INACTIVE 0x80
#define CARD_CURSOR_INDEX_MASK 0x7f
#define CARD_DECK_SIZE 40
#define CARD_MAX_COPIES 3
#define CARD_DECK_SLOT_COUNT 5
#define CARD_TYPE_FILTER_COUNT 4
#define CARD_ATTR_FILTER_COUNT 7
enum {
    CARD_ACTION_NONE = 0,
    CARD_ACTION_SCROLL_DOWN = 1,
    CARD_ACTION_REFRESH = 2,       /* list: rebuild the rows, cursor and scroll to the top */
    CARD_ACTION_ADD = 3,           /* add the selected card to the deck */
    CARD_ACTION_REMOVE = 4,        /* remove the selected card from the deck */
    CARD_ACTION_SCROLL_UP = -1
};
enum {
    CARD_DECK_MSG_NONE = 0,
    CARD_DECK_MSG_COPIES_USED = 1, /* every owned copy is already in the deck */
    CARD_DECK_MSG_DECK_FULL = 3,
    CARD_DECK_MSG_NOT_OWNED = 4
};
#define CARD_MODEL_TYPE1_B 0x01900000
#define CARD_MODEL_TYPE1_E 0x01900400
#define CARD_MODEL_TYPE1_S 0x01900800
#define CARD_MODEL_TYPE2_0 0x01900C00
#define CARD_MODEL_TYPE2_1 0x01901000
#define CARD_MODEL_TYPE2_2 0x01901400
#define CARD_MODEL_TYPE2_3 0x01901800
#define CARD_MODEL_TYPE2_4 0x01901C00
#define CARD_MODEL_TYPE2_5 0x01902000
#define CARD_MODEL_TYPE2_6 0x01902400
#define CARD_TEXTURE_TYPE1_B 0x01908000
#define CARD_TEXTURE_TYPE1_E 0x01918100
#define CARD_TEXTURE_TYPE1_S 0x01928200
#define CARD_TEXTURE_TYPE2_0 0x01938300
#define CARD_DIGIT_MODEL_BASE 0x01C8A400
#define CARD_DIGIT_MODEL_SHIFT 10
#define CARD_DIGIT_TEXTURE 0x01C990A0
extern int ActiveCursol_0;
extern CsdList *D_00A57838;
extern u8 TypeMaskLst[];
extern u8 AttriMaskLst[];
extern void CardListDisp(void *list, CsdWork *work);
extern int CardGetDeckCardCnt(int cardId, void *deck);
extern int CardAddDeck(void *deck, int cardId);
extern void CardDelDeck(void *deck, int cardId);
extern s16 CardMakeDispList(void *rows, int mask, void *definitions, u16 cardCount);
extern void nmlModelSetPlace(float matrix[4][4]);
extern void nmlModelSetTexture(int texture);
extern void nmlModelSetRenderLevel(int level);
extern void nmlModelEntryCard(int entry);

int CardMakeDeckProc(int exiting, CsdList *list, CsdList *deck, CsdWork *work)
{
    float matrix[4][4];
    float offset[4];
    float scale[4];
    float pinMatrix[4][4];
    float filterMatrix[4][4];
    float digitMatrix[4][4];
    float countMatrix[4][4];
    s16 *ownedCount;
    int result;
    int hovered;
    int total;
    int digit;
    int typeMask;
    int attr;
    int texture;
    int entry;
    int model;

    /* Signed view of the owned-copy table: the original loads it with lh. */
    ownedCount = (s16 *) work->owned;
    ((u8 *) work)[0x59BF] = CARD_HELP_TOPIC_MAKE_DECK;

    if (ActiveCursol_0 != 0) {
        D_00A57838 = deck;
        list->cursor |= CARD_PANE_INACTIVE;
        deck->cursor &= CARD_CURSOR_INDEX_MASK;
    } else {
        D_00A57838 = list;
        list->cursor &= CARD_CURSOR_INDEX_MASK;
        deck->cursor |= CARD_PANE_INACTIVE;
    }

    result = 0;
    offset[0] = 0.0f;
    offset[1] = -0.3932f;
    offset[2] = 0.1f;
    offset[3] = 1.0f;
    scale[1] = 0.335f;
    scale[0] = 0.335f;
    scale[3] = 1.0f;
    scale[2] = 1.0f;
    xglMatrixUnit(matrix);
    xglMatrixTrans(matrix, matrix, offset);
    xglMatrixScale(matrix, matrix, scale);

    if (exiting == 0) {
        if (PadData.pad2a & CARD_PAD_TYPE_NEXT) {
            xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
            list->typeFilter++;
            list->mode = CARD_ACTION_REFRESH;
        }
        if (PadData.pad2a & CARD_PAD_TYPE_PREV) {
            xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
            list->typeFilter--;
            list->mode = CARD_ACTION_REFRESH;
        }
        if (PadData.pad2a & CARD_PAD_ATTR_NEXT) {
            xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
            list->attrFilter++;
            list->mode = CARD_ACTION_REFRESH;
        }
        if (PadData.pad2a & CARD_PAD_ATTR_PREV) {
            xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
            list->attrFilter--;
            list->mode = CARD_ACTION_REFRESH;
        }
        if (PadData.pad2a & CARD_PAD_ACCEPT) {
            xglSoundEffectNormalID(CARD_SE_ACCEPT, 0);
            D_00A57838->mode = CARD_ACTION_ADD;
        }
        if (PadData.pad2a & CARD_PAD_CANCEL) {
            xglSoundEffectNormalID(CARD_SE_CANCEL, 0);
            D_00A57838->mode = CARD_ACTION_REMOVE;
        }
        if (PadData.trig & CARD_PAD_UP) {
            xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
            D_00A57838->mode = CARD_ACTION_SCROLL_UP;
        }
        if (PadData.trig & CARD_PAD_DOWN) {
            xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
            D_00A57838->mode = CARD_ACTION_SCROLL_DOWN;
        }
        if (PadData.trig & CARD_PAD_RIGHT) {
            xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
            if (CardGetDeckAllCnt(deck) != 0) {
                ActiveCursol_0 = 1;
                list->cursor |= CARD_PANE_INACTIVE;
                deck->cursor &= CARD_CURSOR_INDEX_MASK;
            }
        }
        if (PadData.trig & CARD_PAD_LEFT) {
            xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
            ActiveCursol_0 = 0;
            list->cursor &= CARD_CURSOR_INDEX_MASK;
            deck->cursor |= CARD_PANE_INACTIVE;
        }
    }

    if (list->typeFilter < 0) {
        list->typeFilter = CARD_TYPE_FILTER_COUNT - 1;
    }
    if (list->typeFilter >= CARD_TYPE_FILTER_COUNT) {
        list->typeFilter = 0;
    }
    if (list->attrFilter < 0) {
        list->attrFilter = CARD_ATTR_FILTER_COUNT - 1;
    }
    if (list->attrFilter >= CARD_ATTR_FILTER_COUNT) {
        list->attrFilter = 0;
    }

    if (deck->mode != CARD_ACTION_NONE) {
        switch (deck->mode) {
        case CARD_ACTION_SCROLL_DOWN:
        case CARD_ACTION_SCROLL_UP:
            if (deck->mode < 0) {
                if (deck->top > 0) {
                    if (deck->cursor < CARD_SCROLL_MARGIN) {
                        deck->top = deck->top - 1;
                    } else {
                        deck->cursor = deck->cursor - 1;
                        if (deck->cursor < 0) {
                            deck->cursor = 0;
                        }
                    }
                } else if (deck->cursor > 0) {
                    deck->cursor = deck->cursor - 1;
                }
            } else if (deck->count > deck->top + deck->rows) {
                if (deck->cursor >= deck->rows - CARD_SCROLL_MARGIN) {
                    deck->top = deck->top + 1;
                } else {
                    deck->cursor = deck->cursor + 1;
                }
            } else {
                if (deck->cursor < deck->rows - 1
                    && deck->cursor < deck->count - 1
                    && deck->cursor < deck->columns) {
                    deck->cursor = deck->cursor + 1;
                }
            }
            break;
        case CARD_ACTION_ADD:
            if (CardGetDeckAllCnt(deck) < CARD_DECK_SIZE) {
                int id = deck->entries[deck->top + deck->cursor].id;

                if (CardGetDeckCardCnt(id, deck) < ownedCount[id]) {
                    CardAddDeck(deck, id);
                } else {
                    result = (ownedCount[id] == 0) ? CARD_DECK_MSG_NOT_OWNED : CARD_DECK_MSG_COPIES_USED;
                }
            } else {
                result = CARD_DECK_MSG_DECK_FULL;
            }
            break;
        case CARD_ACTION_REMOVE:
            CardDelDeck(deck, deck->entries[deck->top + deck->cursor].id);
            if (CardGetDeckAllCnt(deck) == 0) {
                ActiveCursol_0 = 0;
                list->cursor &= CARD_CURSOR_INDEX_MASK;
                deck->cursor |= CARD_PANE_INACTIVE;
            }
            break;
        }
        deck->mode = CARD_ACTION_NONE;
    } else if (list->mode != CARD_ACTION_NONE) {
        list->count = CardMakeDispList(list->entries,
                                          AttriMaskLst[list->attrFilter]
                                              | (TypeMaskLst[list->typeFilter] << 8),
                                          work->definitions, work->cardCount);
        switch (list->mode) {
        case 0:
            break;
        case CARD_ACTION_SCROLL_DOWN:
        case CARD_ACTION_SCROLL_UP:
            if (list->mode < 0) {
                if (list->top > 0) {
                    if (list->cursor < CARD_SCROLL_MARGIN) {
                        list->top = list->top - 1;
                    } else {
                        list->cursor = list->cursor - 1;
                        if (list->cursor < 0) {
                            list->cursor = 0;
                        }
                    }
                } else if (list->cursor > 0) {
                    list->cursor = list->cursor - 1;
                }
            } else if (list->top + list->rows < list->count) {
                if (list->cursor >= list->rows - CARD_SCROLL_MARGIN) {
                    list->top = list->top + 1;
                } else {
                    list->cursor = list->cursor + 1;
                }
            } else if (list->cursor < list->rows - 1
                       && list->cursor < list->count - 1) {
                list->cursor = list->cursor + 1;
            }
            break;
        case CARD_ACTION_REFRESH:
            list->cursor = 0;
            list->top = 0;
            break;
        case CARD_ACTION_ADD:
            result = CARD_DECK_MSG_DECK_FULL;
            if (CardGetDeckAllCnt(deck) < CARD_DECK_SIZE) {
                int id = list->entries[list->top + list->cursor].id;
                total = CardGetDeckCardCnt(id, deck);

                if (total < CARD_MAX_COPIES) {
                    if (total < ownedCount[id]) {
                        int newIdx = CardAddDeck(deck, id);
                        if (newIdx - deck->rows + 1 < 0) {
                            deck->top = 0;
                        } else {
                            deck->top = newIdx - deck->rows + 1;
                        }
                        /* Row within the window, with CARD_PANE_INACTIVE set (the list has focus). */
                        deck->cursor = (newIdx % deck->rows) - CARD_PANE_INACTIVE;
                        result = 0;
                    } else {
                        result = (ownedCount[id] == 0) ? CARD_DECK_MSG_NOT_OWNED : CARD_DECK_MSG_COPIES_USED;
                    }
                }
            }
            break;
        case CARD_ACTION_REMOVE:
            CardDelDeck(deck, list->entries[list->top + list->cursor].id);
            break;
        }
        list->mode = CARD_ACTION_NONE;
    }

    if (deck->count - 1 < deck->cursor) {
        deck->cursor = deck->count - 1;
    }
    if (deck->columns - 1 < deck->cursor) {
        deck->cursor = deck->columns - 1;
    }

    hovered = 0;
    if ((PadData.held & CARD_PAD_PREVIEW) && exiting == 0) {
        hovered = 1;
        if (ActiveCursol_0 != 0) {
            offset[0] = -1.15f;
            D_00A57838 = deck;
        } else {
            offset[0] = 1.15f;
            D_00A57838 = list;
        }
        offset[1] = 0.45f;
        offset[2] = 0.2f;
        offset[3] = 1.0f;
        xglMatrixUnit(pinMatrix);
        xglMatrixTrans(pinMatrix, pinMatrix, offset);
        {
            CsdList *sel = D_00A57838;
            int id = sel->entries[sel->top + sel->cursor].id;
            CardDisp(pinMatrix, id, (s16) work->owned[id] != 0, exiting == 0);
        }
    }
    if (hovered == 0 || ActiveCursol_0 == 0) {
        CardListDisp(list, work);
    }
    if (hovered == 0 || ActiveCursol_0 != 0) {
        CardDeckDisp(deck, work->definitions);
    }

    offset[0] = offset[2] = 0.0f;
    offset[1] = 1.292f;
    offset[3] = 1.0f;
    xglMatrixUnit(filterMatrix);
    xglMatrixTrans(filterMatrix, filterMatrix, offset);
    nmlModelSetPlace(filterMatrix);
    typeMask = TypeMaskLst[list->typeFilter];
    if (typeMask == CARD_TYPE_MASK_B) {
        texture = CARD_TEXTURE_TYPE1_B;
        entry = CARD_MODEL_TYPE1_B;
    } else if (typeMask == CARD_TYPE_MASK_E) {
        texture = CARD_TEXTURE_TYPE1_E;
        entry = CARD_MODEL_TYPE1_E;
    } else if (typeMask == CARD_TYPE_MASK_S) {
        texture = CARD_TEXTURE_TYPE1_S;
        entry = CARD_MODEL_TYPE1_S;
    } else {
        texture = CARD_TEXTURE_TYPE2_0;
        entry = CARD_MODEL_TYPE2_0;
    }
    nmlModelSetTexture(texture);
    nmlModelSetRenderLevel(CARD_MODEL_RENDER_LEVEL);
    nmlModelEntryCard(entry);

    offset[1] = 1.047f;
    entry = CARD_MODEL_TYPE2_1;
    xglMatrixUnit(filterMatrix);
    xglMatrixTrans(filterMatrix, filterMatrix, offset);
    nmlModelSetPlace(filterMatrix);
    /*
     * Attribute masks CARD_ATTR_MASK_1..6 (1, 2, 8, 4, 0x10, 0x60) select type2_1..type2_6; any
     * other value (no filter) shows type2_0. The texture assignments in the
     * last two arms are never read: with a single assignment per arm the
     * compiler turns the final test into a conditional move (the original
     * branches), so both arms keep their second statement.
     */
    attr = AttriMaskLst[list->attrFilter];
    if (attr != CARD_ATTR_MASK_1) {
        entry = CARD_MODEL_TYPE2_2;
        if (attr != CARD_ATTR_MASK_2) {
            entry = CARD_MODEL_TYPE2_3;
            if (attr != CARD_ATTR_MASK_3) {
                entry = CARD_MODEL_TYPE2_4;
                if (attr != CARD_ATTR_MASK_4) {
                    entry = CARD_MODEL_TYPE2_5;
                    if (attr != CARD_ATTR_MASK_5) {
                        if (attr == CARD_ATTR_MASK_6) {
                            texture = CARD_TEXTURE_TYPE2_0;
                            entry = CARD_MODEL_TYPE2_6;
                        } else {
                            texture = CARD_TEXTURE_TYPE2_0;
                            entry = CARD_MODEL_TYPE2_0;
                        }
                    }
                }
            }
        }
    }
    nmlModelSetTexture(CARD_TEXTURE_TYPE2_0);
    nmlModelSetRenderLevel(CARD_MODEL_RENDER_LEVEL);
    nmlModelEntryCard(entry);

    if (ActiveCursol_0 != 0) {
        D_00A57838 = deck;
    } else {
        D_00A57838 = list;
    }
    {
        CsdList *sel = D_00A57838;
        int id = sel->entries[sel->top + sel->cursor].id;
        int flag;

        /* The flag is materialised with a branch; `exiting == 0` inline changes the code. */
        if (exiting) {
            flag = 0;
        } else {
            flag = 1;
        }
        CardDisp(matrix, id, (s16) work->owned[id] != 0, flag);
    }

    total = list->entries[list->top + (list->cursor & CARD_CURSOR_INDEX_MASK)].id + 1;
    digit = total / 100;
    offset[1] = 1.764f;
    offset[2] = 0.01f;
    scale[0] = 1.13f;
    offset[3] = 1.0f;
    scale[3] = 1.0f;
    scale[2] = 1.0f;
    scale[1] = 1.0f;
    xglMatrixUnit(digitMatrix);
    xglMatrixScale(digitMatrix, digitMatrix, scale);
    if (digit != 0) {
        model = (digit << CARD_DIGIT_MODEL_SHIFT) + CARD_DIGIT_MODEL_BASE;
        offset[0] = -0.895f;
        xglMatrixTrans(matrix, digitMatrix, offset);
        nmlModelSetPlace(matrix);
        nmlModelSetTexture(CARD_DIGIT_TEXTURE);
        nmlModelSetRenderLevel(CARD_MODEL_RENDER_LEVEL);
        nmlModelEntryCard(model);
    }
    digit = total % 100 / 10;
    if (total / 100 != 0 || digit != 0) {
        model = (digit << CARD_DIGIT_MODEL_SHIFT) + CARD_DIGIT_MODEL_BASE;
        offset[0] = -0.748f;
        xglMatrixTrans(matrix, digitMatrix, offset);
        nmlModelSetPlace(matrix);
        nmlModelSetTexture(CARD_DIGIT_TEXTURE);
        nmlModelSetRenderLevel(CARD_MODEL_RENDER_LEVEL);
        nmlModelEntryCard(model);
    }
    digit = total % 100 % 10;
    model = (digit << CARD_DIGIT_MODEL_SHIFT) + CARD_DIGIT_MODEL_BASE;
    offset[0] = -0.588f;
    xglMatrixTrans(matrix, digitMatrix, offset);
    nmlModelSetPlace(matrix);
    nmlModelSetTexture(CARD_DIGIT_TEXTURE);
    nmlModelSetRenderLevel(CARD_MODEL_RENDER_LEVEL);
    nmlModelEntryCard(model);

    total = CardGetDeckAllCnt(deck);
    digit = total / 100;
    offset[1] = 1.764f;
    offset[2] = 0.01f;
    scale[0] = 1.13f;
    offset[3] = 1.0f;
    scale[3] = 1.0f;
    scale[2] = 1.0f;
    scale[1] = 1.0f;
    xglMatrixUnit(countMatrix);
    xglMatrixScale(countMatrix, countMatrix, scale);
    if (digit != 0) {
        entry = (digit << CARD_DIGIT_MODEL_SHIFT) + CARD_DIGIT_MODEL_BASE;
        offset[0] = 1.948f;
        xglMatrixTrans(matrix, countMatrix, offset);
        nmlModelSetPlace(matrix);
        nmlModelSetTexture(CARD_DIGIT_TEXTURE);
        nmlModelSetRenderLevel(CARD_MODEL_RENDER_LEVEL);
        nmlModelEntryCard(entry);
    }
    digit = total % 100 / 10;
    if (total / 100 != 0 || digit != 0) {
        entry = (digit << CARD_DIGIT_MODEL_SHIFT) + CARD_DIGIT_MODEL_BASE;
        offset[0] = 2.095f;
        xglMatrixTrans(matrix, countMatrix, offset);
        nmlModelSetPlace(matrix);
        nmlModelSetTexture(CARD_DIGIT_TEXTURE);
        nmlModelSetRenderLevel(CARD_MODEL_RENDER_LEVEL);
        nmlModelEntryCard(entry);
    }
    digit = total % 100 % 10;
    entry = (digit << CARD_DIGIT_MODEL_SHIFT) + CARD_DIGIT_MODEL_BASE;
    offset[0] = 2.255f;
    xglMatrixTrans(matrix, countMatrix, offset);
    nmlModelSetPlace(matrix);
    nmlModelSetTexture(CARD_DIGIT_TEXTURE);
    nmlModelSetRenderLevel(CARD_MODEL_RENDER_LEVEL);
    nmlModelEntryCard(entry);

    return result;
}

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

extern CsdList *D_00A57844;
extern int ActiveCursol_6;
extern char D_00A4DB10[];
extern u8 *EnemyDeckLst[48];

int CardLoadDeckProc2(int exiting, CsdList *cursor, CsdList *list, CsdWork *work, char *title)
{
    float trans[4];
    float mat[4][4];
    float trans2[4];
    float scale[4];
    int ret;
    int disp;

    ((u8 *)work)[0x59BF] = 0x3A;
    if (ActiveCursol_6 != 0) {
        D_00A57844 = list;
        cursor->cursor |= 0x80;
        list->cursor &= 0x7F;
    } else {
        D_00A57844 = cursor;
        cursor->cursor &= 0x7F;
        list->cursor |= 0x80;
    }
    ret = 0;
    switch (*(s16 *)((u8 *)work + 0x5AC6)) {
    case 0:
        disp = 0;
        if ((PadData.held & 0x10) && exiting == 0 && ActiveCursol_6 != 0) {
            short no;
            disp = 1;
            D_00A57844 = list;
            trans[0] = -1.15f;
            trans[1] = 0.45f;
            trans[2] = 0.2f;
            trans[3] = 1.0f;
            xglMatrixUnit(mat);
            xglMatrixTrans(mat, mat, trans);
            no = D_00A57844->entries[D_00A57844->top + D_00A57844->cursor].id;
            CardDisp(mat, no, work->owned[no] != 0, 1);
        }
        if (disp == 0 || ActiveCursol_6 == 0) {
            CardDeckListDisp(cursor);
        }
        CardDeckDisp(list, work->definitions);
        if (ActiveCursol_6 == 0) {
            if (exiting == 0 && disp == 0) {
                if (title != 0) {
                    xglFontPrint(0, 0, 0, D_00A4DA68);
                    xglFontPrint(0x18, 0x138, 0xFFF0, title);
                }
                xglFontPrint(0x78, 0x120, 0xFFF0, D_00A4DA70);
                xglFontPrint(0x18, 0x150, 0xFFF0, D_00A4DB10);
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
            no = D_00A57844->entries[D_00A57844->top + D_00A57844->cursor].id;
            CardDisp(mat, no, work->owned[no] != 0, exiting == 0);
        }
        if (exiting == 0) {
            if (PadData.trig & 0x1000) {
                xglSoundEffectNormalID(3, 0);
                D_00A57844->mode = -1;
            } else if (PadData.trig & 0x4000) {
                xglSoundEffectNormalID(3, 0);
                D_00A57844->mode = 1;
            }
            if (PadData.trig & 0x2000) {
                xglSoundEffectNormalID(3, 0);
                if (CardGetDeckAllCnt(list) != 0) {
                    ActiveCursol_6 = 1;
                    cursor->cursor |= 0x80;
                    list->cursor &= 0x7F;
                }
            } else if (PadData.trig & 0x8000) {
                xglSoundEffectNormalID(3, 0);
                ActiveCursol_6 = 0;
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
                    if (prev != EnemyDeckLst[k][i]) {
                        j++;
                        list->entries[j].id = EnemyDeckLst[k][i];
                        list->entries[j].n = 1;
                    } else {
                        list->entries[j].n++;
                    }
                    prev = EnemyDeckLst[k][i];
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

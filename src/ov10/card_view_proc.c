/*
 * OV10 original TU 5: 0x00a1c1b8..0x00a1e5d8 (5 functions)
 */
#include "common.h"

unsigned char TitleMaxList[7] = {6, 9, 14, 9, 3, 5, 6};

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern void xglFontPrint(int x, int y, int color, const char *text);
extern void xglSoundEffectNormalID(int sound_id, int variant);
extern void xglMatrixUnit(float (*m)[4]);
extern void xglMatrixScale(float (*dst)[4], float (*src)[4], float *v);
extern void xglMatrixTrans(float (*dst)[4], float (*src)[4], float *v);
extern void CardDisp(float (*place)[4], short no, short flag, short disp);
extern void nmlModelSetPlace(float (*matrix)[4]);
extern void nmlModelSetTexture(void *texture);
extern void nmlModelSetRenderLevel(int level);
extern void nmlModelEntryCard(void *model);
extern int CardMakeDispList(short *out, unsigned short mask, void *ent, int n);
extern void CardListDisp(void *w, void *src);
extern unsigned int CardCntList(void *o, int idx);
extern char ModeEndMess0[];
extern char ModeEndMess1[];
extern char D_00A4DB30[];
typedef struct { unsigned char pad[0x2A]; unsigned short trg; unsigned short rpt; unsigned char pad2[0xA2]; } CVPad;
extern CVPad PadData[];
typedef struct {
    unsigned char pad0[4];
    signed char move;
    signed char cur;
    unsigned char pad6[6];
    short top;
    unsigned char padE[2];
    short rows;
    short total;
    short list[1][2];
} CVList;
#define W16(o) (*(short *)((char *)work + (o)))
#define WU16(o) (*(unsigned short *)((char *)work + (o)))
#define W8(o) (*(unsigned char *)((char *)work + (o)))
#define CVDIGIT(d, x) \
    { \
        trans[0] = x; \
        m = ((d) << 10) + 0x01C8A400; \
        xglMatrixTrans(tmat, smat, trans); \
        nmlModelSetPlace(tmat); \
        nmlModelSetTexture((void *)0x01C990A0); \
        nmlModelSetRenderLevel(2); \
        nmlModelEntryCard((void *)m); \
    }

void CardViewProc(void *work, CVList *list, short *exiting)
{
  int btn;
  float mat[4][4];
  float pos[4];
  float trans[4];
  float scale[4];
  float smat[4][4];
  float tmat[4][4];
  int num;
  int d;
  int hund;
  int rest;
  int m;
  int i;
  int all;
  xglFontDebugPrintf(0x54, 0, D_00A4DB30);
  *((unsigned char *) (((char *) work) + 0x59BF)) = 0x36;
  pos[0] = -1.0f;
  pos[1] = 0.6f;
  pos[2] = 0.1f;
  pos[3] = 1.0f;
  xglMatrixUnit(mat);
  xglMatrixTrans(mat, mat, pos);
  if ((*exiting) != 0)
  {
    unsigned short btn;
    xglFontPrint(0x18, 0x138, 0xFFF0, ModeEndMess0);
    xglFontPrint(0xE0, 0x168, 0xFFF0, ModeEndMess1);
    if (PadData[0].trg & 0x20)
    {
      *((short *) (((char *) work) + 0x7102)) = 0xA;
      *((unsigned char *) (((char *) work) + 0x7104)) = 0x10;
      *((short *) (((char *) work) + 0x59B4)) = 0;
      xglSoundEffectNormalID(1, 0);
    }
    if (((&PadData[0])->trg & 0x40) || ((*((unsigned short *) (((char *) work) + 0x5AD2))) & 0x800))
    {
      *exiting = 0;
      xglSoundEffectNormalID(2, 0);
    }
  }
  else
  {
    btn = *((unsigned short *) 0x490DBC);
    rest = btn & 0x4000;
    if (btn & 0x1000)
    {
      list->move = -1;
      if (list->cur)
      {
      }
      else
      {
      }
      list->move = -1;
    }
    else
      if (rest)
    {
      list->move = 1;
    }
    else
      if ((*((unsigned short *) (((char *) work) + 0x5AD2))) & 0x800)
    {
      *exiting = 1;
      xglSoundEffectNormalID(1, 0);
    }
  }
  if (list->move != 0)
  {
    xglSoundEffectNormalID(3, 0);
    if (list->move < 0)
    {
      if (list->top > 0)
      {
        if (list->cur < 2)
        {
          list->top--;
        }
        else
        {
          list->cur--;
          if (list->cur < 0)
          {
            list->cur = 0;
          }
        }
      }
      else
        if (list->cur > 0)
      {
        list->cur--;
      }
    }
    else
      if ((list->top + list->rows) < list->total)
    {
      if (list->cur >= (list->rows - 2))
      {
        list->top++;
      }
      else
      {
        list->cur++;
      }
    }
    else
      if ((list->cur < (list->rows - 1)) && (list->cur < (list->total - 1)))
    {
      list->cur++;
    }
    list->move = 0;
    (*list).total = CardMakeDispList(list->list[0], 0xFFFF, *((void **) (((char *) work) + 8)), *((unsigned short *) (((char *) work) + 2)));
  }
  CardListDisp(list, work);
  num = (list->top + list->cur) + 1;
  hund = num / 100;
  d = hund;
  trans[1] = 1.764f;
  trans[2] = 0.01f;
  scale[0] = 1.13f;
  trans[3] = 1.0f;
  scale[3] = 1.0f;
  scale[2] = 1.0f;
  scale[1] = 1.0f;
  xglMatrixUnit(smat);
  xglMatrixScale(smat, smat, scale);
  if (0 != d)
  {
    trans[0] = 1.948f;
    m = (d << 10) + 0x01C8A400;
    xglMatrixTrans(tmat, smat, trans);
    nmlModelSetPlace(tmat);
    nmlModelSetTexture((void *) 0x01C990A0);
    nmlModelSetRenderLevel(2);
    nmlModelEntryCard((void *) m);
  }
  ;
  rest = num % 100;
  d = rest / 10;
  if ((num / 100) || d)
  {
    trans[0] = 2.095f;
    m = (d << 10) + 0x01C8A400;
    xglMatrixTrans(tmat, smat, trans);
    nmlModelSetPlace(tmat);
    nmlModelSetTexture((void *) 0x01C990A0);
    nmlModelSetRenderLevel(2);
    nmlModelEntryCard((void *) m);
  }
  ;
  d = (num % 100) % 10;
  i = 0;
  all = 1;
  {
    trans[0] = 2.255f;
    m = (d << 10) + 0x01C8A400;
    xglMatrixTrans(tmat, smat, trans);
    nmlModelSetPlace(tmat);
    nmlModelSetTexture((void *) 0x01C990A0);
    nmlModelSetRenderLevel(2);
    nmlModelEntryCard((void *) m);
  }
  ;
  {
    short no = list->list[list->top + list->cur][0];
    CardDisp(mat, no, (*((short *) (((char *) work) + (0x10 + (no * 2))))) != 0, (*exiting) == 0);
  }
  for (; i < (*((unsigned short *) (((char *) work) + 2))); i++)
  {
    if (CardCntList(work, i) == 0)
    {
      all = 0;
      break;
    }
  }

  if (all)
  {
    pos[0] = -0.194f;
    pos[1] = 1.6f;
    pos[2] = 0.5f;
    pos[3] = 1.0f;
    xglMatrixUnit(mat);
    xglMatrixTrans(mat, mat, pos);
    nmlModelSetPlace(mat);
    nmlModelSetTexture((void *) 0x01DDFC00);
    nmlModelSetRenderLevel(2);
    nmlModelEntryCard((void *) 0x01DDF800);
  }
}

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

#include "shared.h"
#include "ov10/cgp.h"
#define CARD_PAD_OK 0x0020            /* confirm: andi at 0x00a1c2a8 (CardViewProc), 0x00a1cd1c (CardOpenProc) */
#define CARD_PAD_CANCEL 0x0040        /* cancel: andi at 0x00a1c2d4, 0x00a1cd28 */
#define CARD_PAD_START 0x0800         /* opens or closes the exit prompt: andi at 0x00a1c2e4, 0x00a1cc94 */
#define CARD_PAD_ANY_RESPONSE (CARD_PAD_OK | CARD_PAD_CANCEL) /* andi 0x60 at 0x00a1de44 */
#define CARD_PAD_ANY_RESPONSE_WORD 0x00600000
#define CARD_VIEW_PAD_PREV_ENTRY 0x1000 /* d-pad: scroll the list back, andi at 0x00a1c310 */
#define CARD_VIEW_PAD_NEXT_ENTRY 0x4000 /* d-pad: scroll the list forward, andi at 0x00a1c318 */
#define CARD_OPEN_PAD_NEXT 0x2000       /* d-pad: next pack or card, andi at 0x00a1d18c, 0x00a1dd64 */
#define CARD_OPEN_PAD_PREV 0x8000       /* d-pad: previous pack or card, andi at 0x00a1d1b8, 0x00a1dd8c */
#define CARD_TEXT_COLOR 0xFFF0        /* colour argument of every xglFontPrint in these screens (ori at 0x00a1c280) */
#define CARD_MODEL_COUNT 16           /* CardGameWork.models entries */
#define CARD_FADE_TICKS 10            /* fadeOutTimer value that starts a fade */
#define CARD_FADE_STEP 16             /* fadeOutStep: amount CardFadeOut subtracts per tick */
#define CARD_HELP_TOPIC_VIEW 54       /* sb at 0x00a1c210 */
#define CARD_HELP_TOPIC_OPEN 0x35     /* sb at 0x00a1cc3c */
#define CARD_SE_OK 1
#define CARD_SE_CANCEL 2
#define CARD_SE_CURSOR 3
#define CARD_LIST_FILTER_ALL 0xFFFF   /* CardMakeDispList (0x00a08880) masks the type byte with the high byte and the
                                       * subtype halfword with the low byte: every card passes */
#define CARD_LIST_SCROLL_MARGIN 2     /* cursor rows kept between the cursor and the window edge: slti at 0x00a1c38c */
#define CARD_DIGIT_MODEL_BASE 0x01c8a400 /* models of the digits 0..9, ori at 0x00a1c528 */
#define CARD_DIGIT_MODEL_SHIFT 10     /* one digit model every 0x400 bytes */
#define CARD_DIGIT_TEXTURE 0x01c990a0 /* ori at 0x00a1c548 */
#define CARD_COMPLETE_MODEL 0x01ddf800 /* shown when CardCntList is nonzero for every card, ori at 0x00a1c750 */
#define CARD_COMPLETE_TEXTURE 0x01ddfc00 /* ori at 0x00a1c73c */
#define CARD_FAN_SELECTED_MODEL 0x01d12000 /* ori at 0x00a1c928 */
#define CARD_FAN_SELECTED_TEXTURE 0x01d60000
#define CARD_FAN_BACKDROP_MODEL 0x01da5600 /* ori at 0x00a1ca48 */
#define CARD_FAN_BACKDROP_TEXTURE 0x01db5800 /* ori at 0x00a1ca34 */
#define CARD_PACK1_MODEL 0x01900000   /* deckmake\pack1.lex */
#define CARD_PACK2_MODEL 0x01901000   /* deckmake\pack2.lex, ori at 0x00a1ce58 */
#define CARD_PACK3_MODEL 0x01903000   /* deckmake\pack3.lex */
#define CARD_PACK4_MODEL 0x01905000   /* deckmake\pack4.lex */
#define CARD_CURSOR_L_MODEL 0x01908000 /* deckmake\cur_L.lex */
#define CARD_CURSOR_R_MODEL 0x01908800 /* deckmake\cur_R.lex */
#define CARD_PACK_TEXTURE 0x01A00000  /* deckmake\pack.bin */
#define CARD_CURSOR_TEXTURE 0x01A40050 /* deckmake\cur_LR.bin, ori at 0x00a1cecc */
#define CARD_PACK_COUNT 20            /* slti at 0x00a1ceec, 0x00a1ca94 */
#define CARD_PACK_EVENT_BASE 0x24     /* addiu at 0x00a1cedc (dataEvtBoxChk), 0x00a1dc5c (dataEvtBoxDec) */
#define CARD_OPEN_LIST_MAX 0x28       /* CardList capacity: a starter pack lists 1 + 39 cards */
#define CARD_ID_COUNT 150             /* ownedCount entries, slti at 0x00a1dfc8 */
#define CARD_COUNTS_PER_BYTE 4
#define CARD_COUNT_BITS 2
#define CARD_COUNT_MASK 3
#define CARD_SELL_PRICE 5             /* dataMoneyBoxInc argument for a card sold instead of kept (0x00a1deb8) */
#define ZENKAKU_GLYPH_SIZE 2          /* the full-width digit tables hold two bytes per glyph */
#define CARD_PACK_SPECIAL_CARDS 3     /* li 3 at 0x00a1d6b8, stored to CardCnt at 0x00a1d6c0 */
enum {
    CARD_PACK_ST1 = 17,
    CARD_PACK_BS1,
    CARD_PACK_BS2
};
#define CARD_PACK_DRAW_SIZE 10        /* cards drawn from BS1 / BS2: slti at 0x00a1d858 */
#define CARD_PACK_RARE_DRAWS 1        /* BS1: li 1 at 0x00a1d6dc, bgtz at 0x00a1d78c; BS2: 0x00a1d944, 0x00a1da0c */
#define CARD_PACK_UNCOMMON_DRAWS 2    /* BS1: li 2 at 0x00a1d6f8, bgtz at 0x00a1d84c; BS2: 0x00a1d960, 0x00a1dacc */
#define BS1_RARE_COUNT 0x10
#define BS1_UNCOMMON_COUNT 0x17
#define BS1_COMMON_COUNT 0x2C
#define BS2_RARE_COUNT 0x14
#define BS2_UNCOMMON_COUNT 9
#define BS2_COMMON_COUNT 0x11
#define ST1_RARE_COUNT 6
#define ST1_CARD_COUNT 0x27           /* slti at 0x00a1dc2c */
#define CARD_CURSOR_FRAMES 0x10       /* CursorTimer wraps here: sltiu at 0x00a1d1f0 */
#define CARD_CURSOR_LAST_FRAME 15.0f
#define CARD_CURSOR_TRAVEL 0.31260002f
#define CARD_CURSOR_REST_X 1.5924f
#define CARD_PACK_SPIN_STEP 0.08726647f /* 5 degrees */
#define CARD_PACK_OPEN_SPIN_STEP 0.17453294f /* 10 degrees */
enum {
    CARD_OPEN_STEP_INIT,          /* clear the pack counts */
    CARD_OPEN_STEP_FADE_OUT,
    CARD_OPEN_STEP_LOAD,          /* load the pack models and read the pack counts */
    CARD_OPEN_STEP_FADE_IN,
    CARD_OPEN_STEP_COUNT,         /* find the owned pack kinds */
    CARD_OPEN_STEP_SELECT,        /* choose a pack to open */
    CARD_OPEN_STEP_OPENING,       /* fade out while the pack spins */
    CARD_OPEN_STEP_DRAW,          /* use up a pack and fill CardList */
    CARD_OPEN_STEP_REVEAL,        /* fade in the drawn cards */
    CARD_OPEN_STEP_BROWSE,        /* look through the drawn cards */
    CARD_OPEN_STEP_STORE,         /* add them to the collection, selling the extra copies */
    CARD_OPEN_STEP_REVEAL_KEPT,   /* fade in the cards that were kept */
    CARD_OPEN_STEP_SOLD,          /* report the cards sold */
    CARD_OPEN_STEP_BROWSE_KEPT,
    CARD_OPEN_STEP_LEAVE          /* fade out and return to the pack selection */
};
typedef struct CardPadEntry {
    u8 unmodeled_00[0x28];
    union {
        u64 all;                   /* +0x28: `ld` at 0x00a1cfa8 */
        struct {
            u16 half_28;
            u16 half_2a;
            u16 directions;        /* +0x2c */
        };
    };
    u8 unmodeled_30[0x68 - 0x30];
} CardPadEntry;
typedef struct CardOpenSaveView {
    u8 unmodeled_00[4];
    u8 cardCounts[38];
} CardOpenSaveView;
extern void xglMatrixRotY(Matrix4 destination, Matrix4 source, float angle);
extern void nmlModelEntry(unsigned int model);
extern void CardFadeOut(CardGameWork *work);
extern void CardFadeIn(CardGameWork *work);
extern int CardFread(u32 address, char *filename);
extern int dataEvtBoxChk(int index);
extern void dataEvtBoxDec(int index);
extern int dataMoneyBoxInc(int amount);
extern int CardAddList(CardGameWork *work, s16 cardId);
extern unsigned char BS1RariLst[];
extern unsigned char BS1UncoLst[];
extern unsigned char BS1ComoLst[];
extern unsigned char BS2RariLst[];
extern unsigned char BS2UncoLst[];
extern unsigned char BS2ComoLst[];
extern unsigned char ST1RariLst[];
extern unsigned char ST1CardLst[];
extern char ZenkakuListA[];
extern char ZenkakuListB[];
extern char CardConvertNameLst[CARD_PACK_COUNT][24];
extern u8 D_00A57848[CARD_PACK_COUNT];
extern s16 D_00A57860[CARD_OPEN_LIST_MAX];
extern int D_00A578B0;
extern int D_00A578B4;
extern int D_00A578B8;
extern int D_00A578BC;
extern int D_00A578C0;
extern int D_00A578C4;
extern float rotval_7;
extern u8 CursorTimer_9;

void CardOpenProc(CardGameWork *work, s16 *exiting)
{
    Matrix4 cardMatrix;
    Matrix4 placement;
    float cardOffset[4];
    float cardScale[4]; /* only ever stored: the original writes it too (0x00a1cc40..0x00a1cc60) */
    int i;
    int j;
    int packModel;
    u8 packDigits[4];
    u8 digits[5];
    int rareLeft;
    int uncommonLeft;
    unsigned short pick;
    int duplicate;

    ((u8 *) work)[0x59BF] = CARD_HELP_TOPIC_OPEN;
    cardOffset[0] = cardOffset[1] = cardOffset[2] = 0.0f;
    cardScale[0] = cardScale[1] = cardScale[2] = cardScale[3] = cardOffset[3] = 1.0f;
    xglMatrixUnit(cardMatrix);

    /* START toggles the exit prompt only in the pack selection and the last two steps (CARD_OPEN_STEP_SELECT,
     * _BROWSE_KEPT, _LEAVE, and any step past them); an if-chain on the same condition compiles to a different size. */
    switch (work->faseStep) {
    case CARD_OPEN_STEP_INIT: case CARD_OPEN_STEP_FADE_OUT: case CARD_OPEN_STEP_LOAD:
    case CARD_OPEN_STEP_FADE_IN: case CARD_OPEN_STEP_COUNT:
        break;
    case CARD_OPEN_STEP_OPENING: case CARD_OPEN_STEP_DRAW: case CARD_OPEN_STEP_REVEAL:
    case CARD_OPEN_STEP_BROWSE: case CARD_OPEN_STEP_STORE: case CARD_OPEN_STEP_REVEAL_KEPT:
    case CARD_OPEN_STEP_SOLD:
        break;
    default:
        if (work->command.input[0].pressed & CARD_PAD_START) {
            *exiting = (*exiting + 1) & 1;
            if (*exiting != 0) {
                xglSoundEffectNormalID(CARD_SE_OK, 0);
            } else {
                xglSoundEffectNormalID(CARD_SE_CANCEL, 0);
            }
        }
    }

    if (*exiting != 0) {
        xglFontPrint(24, 312, CARD_TEXT_COLOR, "\x19\x03\r\0Finished opening cards?\0");
        xglFontPrint(224, 360, CARD_TEXT_COLOR, ModeEndMess1);
        if (((CardPadEntry *) PadData)[0].half_2a & CARD_PAD_OK) {
            work->fadeOutTimer = CARD_FADE_TICKS;
            work->fadeOutStep = CARD_FADE_STEP;
            work->fase = 0;
            *exiting = 0;
            xglSoundEffectNormalID(CARD_SE_OK, 0);
        } else if (((CardPadEntry *) PadData)[0].half_2a & CARD_PAD_CANCEL) {
            *exiting = 0;
            xglSoundEffectNormalID(CARD_SE_CANCEL, 0);
        }
        return;
    }

    switch (work->faseStep) {
    case CARD_OPEN_STEP_INIT:
    default:
        work->command.step = 0;
        work->faseStep += 1;
        work->fadeOutTimer = CARD_FADE_TICKS;
        work->fadeOutStep = CARD_FADE_STEP;
        for (i = 0; i < CARD_PACK_COUNT; i += 1) {
            D_00A57848[i] = 0;
        }
        D_00A578B0 = D_00A578B4 = 0;
        D_00A578B8 = 0;
        break;
    case CARD_OPEN_STEP_FADE_OUT:
        CardFadeOut(work);
        if (work->fadeOutTimer == 0) {
            work->faseStep += 1;
        }
        break;
    case CARD_OPEN_STEP_LOAD:
        /* Hide every model entry, then show the last two. */
        for (i = CARD_MODEL_COUNT - 1; i >= 0; i -= 1) {
            work->models[i].visible = 0;
        }
        work->fadeOutTimer = CARD_FADE_TICKS;
        work->models[CARD_MODEL_COUNT - 1].visible = 1;
        work->models[CARD_MODEL_COUNT - 2].visible = 1;
        work->faseStep += 1;
        CardFread(CARD_PACK1_MODEL, "deckmake\\pack1.lex");
        CardFread(CARD_PACK2_MODEL, "deckmake\\pack2.lex");
        CardFread(CARD_PACK3_MODEL, "deckmake\\pack3.lex");
        CardFread(CARD_PACK4_MODEL, "deckmake\\pack4.lex");
        CardFread(CARD_CURSOR_L_MODEL, "deckmake\\cur_L.lex");
        CardFread(CARD_CURSOR_R_MODEL, "deckmake\\cur_R.lex");
        CardFread(CARD_PACK_TEXTURE, "deckmake\\pack.bin");
        CardFread(CARD_CURSOR_TEXTURE, "deckmake\\cur_LR.bin");
        for (i = 0; i < CARD_PACK_COUNT; i += 1) {
            D_00A57848[i] = dataEvtBoxChk(i + CARD_PACK_EVENT_BASE);
        }
        break;
    case CARD_OPEN_STEP_FADE_IN:
        CardFadeIn(work);
        if (work->fadeOutTimer == 0) {
            work->faseStep += 1;
        }
        break;
    case CARD_OPEN_STEP_COUNT:
        rotval_7 = 0.0f;
        D_00A578B0 = -1;
        D_00A578B4 = 0;
        for (i = 0; i < CARD_PACK_COUNT; i += 1) {
            D_00A578B4 += D_00A57848[i];
            if (D_00A57848[i] != 0) {
                if (D_00A578B0 < 0) {
                    D_00A578B0 = i;
                }
                D_00A578B8 += 1;
            }
        }
        if (D_00A578B4 == 0) {
            xglFontPrint(24, 312, CARD_TEXT_COLOR, "\v\x19\x03\r\0You don't have any card packs to open.");
            if (((CardPadEntry *) PadData)[0].all & CARD_PAD_ANY_RESPONSE_WORD) {
                work->fadeOutTimer = CARD_FADE_TICKS;
                work->fadeOutStep = CARD_FADE_STEP;
                work->fase = 0;
                *exiting = 0;
                xglSoundEffectNormalID(CARD_SE_OK, 0);
            }
        } else {
            work->faseStep += 1;
        }
        break;
    case CARD_OPEN_STEP_SELECT:
        cardOffset[0] = cardOffset[1] = cardOffset[2] = 0.0f;
        cardScale[0] = cardScale[1] = cardScale[2] = cardScale[3] = cardOffset[3] = 1.0f;
        xglMatrixUnit(cardMatrix);
        xglFontPrint(10, 312, CARD_TEXT_COLOR, "\x19\x03\r\0Open");
        xglFontPrint(58, 312, CARD_TEXT_COLOR, CardConvertNameLst[D_00A578B0]);
        xglFontPrint(162, 312, CARD_TEXT_COLOR, "\x19\x03\r\0?");
        xglFontPrint(10, 384, CARD_TEXT_COLOR, "\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\x0C\x80@@\xA1\xFB \v\x19\x03\r\0Okay");
        xglFontPrint(10, 336, CARD_TEXT_COLOR, "\v\x19\x03\r\0Number of packs: ");
        packDigits[0] = ZenkakuListA[(D_00A57848[D_00A578B0] / 10) * ZENKAKU_GLYPH_SIZE];
        packDigits[1] = ZenkakuListA[(D_00A57848[D_00A578B0] / 10) * ZENKAKU_GLYPH_SIZE + 1];
        packDigits[2] = ZenkakuListB[(D_00A57848[D_00A578B0] % 10) * ZENKAKU_GLYPH_SIZE];
        packDigits[3] = ZenkakuListB[(D_00A57848[D_00A578B0] % 10) * ZENKAKU_GLYPH_SIZE + 1];
        xglFontPrint(146, 336, CARD_TEXT_COLOR, packDigits);
        xglFontPrint(130, 336, CARD_TEXT_COLOR, ""); /* the original prints an empty string here too (0x00a1d154, string at 0x00a4dca8) */
        xglFontPrint(120, 288, CARD_TEXT_COLOR, "\v");
        if (D_00A578B8 != 1) {
            if (work->command.input[0].pressed & CARD_OPEN_PAD_NEXT) {
                xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
                D_00A578B0 = COPSerachPackR(D_00A57848, D_00A578B0);
            } else if (work->command.input[0].pressed & CARD_OPEN_PAD_PREV) {
                xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
                D_00A578B0 = COPSerachPackL(D_00A57848, D_00A578B0);
            }
            CursorTimer_9 += 1;
            if (CursorTimer_9 >= CARD_CURSOR_FRAMES) {
                CursorTimer_9 = 0;
            }
            cardOffset[1] = 0.6271f;
            cardOffset[2] = 0.6271f;
            cardOffset[0] = (((float) CursorTimer_9 * CARD_CURSOR_TRAVEL) / CARD_CURSOR_LAST_FRAME) + CARD_CURSOR_REST_X;
            xglMatrixTrans(placement, cardMatrix, cardOffset);
            nmlModelSetPlace(placement);
            nmlModelSetTexture(CARD_CURSOR_TEXTURE);
            nmlModelEntryCard(CARD_CURSOR_R_MODEL);
            cardOffset[1] = 0.6271f;
            cardOffset[2] = 0.6271f;
            cardOffset[0] = -CARD_CURSOR_REST_X - (((float) CursorTimer_9 * CARD_CURSOR_TRAVEL) / CARD_CURSOR_LAST_FRAME);
            xglMatrixTrans(placement, cardMatrix, cardOffset);
            nmlModelSetPlace(placement);
            nmlModelSetTexture(CARD_CURSOR_TEXTURE);
            nmlModelEntryCard(CARD_CURSOR_L_MODEL);
        }
        if (work->command.input[0].pressed & CARD_PAD_OK) {
            xglSoundEffectNormalID(CARD_SE_OK, 0);
            work->faseStep += 1;
            work->fadeOutTimer = CARD_FADE_TICKS;
        }
        cardOffset[1] = 0.6271f;
        cardOffset[2] = 0.3881f;
        cardOffset[0] = 0.0f;
        rotval_7 += CARD_PACK_SPIN_STEP;
        xglMatrixTrans(placement, cardMatrix, cardOffset);
        switch (D_00A578B0) {
        case CARD_PACK_ST1: packModel = CARD_PACK1_MODEL; break;
        case CARD_PACK_BS1: packModel = CARD_PACK2_MODEL; break;
        case CARD_PACK_BS2: packModel = CARD_PACK3_MODEL; break;
        default: packModel = CARD_PACK4_MODEL; break;
        }
        xglMatrixRotY(placement, placement, rotval_7);
        nmlModelSetPlace(placement);
        nmlModelSetTexture(CARD_PACK_TEXTURE);
        nmlModelEntry(packModel);
        break;
    case CARD_OPEN_STEP_OPENING:
        CardFadeOut(work);
        if (work->fadeOutTimer == 0) {
            work->faseStep += 1;
        }
        xglFontPrint(10, 312, CARD_TEXT_COLOR, "\x19\x03\r\0Open ");
        xglFontPrint(138, 312, CARD_TEXT_COLOR, CardConvertNameLst[D_00A578B0]);
        xglFontPrint(234, 312, CARD_TEXT_COLOR, "\x19\x03\r\0?");
        xglFontPrint(10, 384, CARD_TEXT_COLOR, "\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\xA1\x0C\x80@@\xA1\xFB \v\x19\x03\r\0Okay");
        xglFontPrint(120, 288, CARD_TEXT_COLOR, "\v");
        rotval_7 += CARD_PACK_OPEN_SPIN_STEP;
        cardOffset[1] = 0.6271f;
        cardOffset[2] = 0.3881f;
        cardOffset[0] = 0.0f;
        xglMatrixTrans(placement, cardMatrix, cardOffset);
        switch (D_00A578B0) {
        case CARD_PACK_ST1: packModel = CARD_PACK1_MODEL; break;
        case CARD_PACK_BS1: packModel = CARD_PACK2_MODEL; break;
        case CARD_PACK_BS2: packModel = CARD_PACK3_MODEL; break;
        default: packModel = CARD_PACK4_MODEL; break;
        }
        xglMatrixRotY(placement, placement, rotval_7);
        nmlModelSetPlace(placement);
        nmlModelSetTexture(CARD_PACK_TEXTURE);
        nmlModelEntryCard(packModel);
        break;
    case CARD_OPEN_STEP_DRAW:
        switch (D_00A578B0) {
        /* Kinds 0..16: three copies of one card, the card id is the case's value.
         * The ids stay literal: the original binary evidences no card names for these fixed pack cards. */
        case 0: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x8D; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 1: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x90; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 2: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x1D; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 3: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x0B; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 4: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x0D; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 5: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x23; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 6: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x34; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 7: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x3A; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 8: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x1E; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 9: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x66; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 10: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x71; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 11: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x76; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 12: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x80; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 13: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x8B; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 14: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x8E; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 15: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x8F; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case 16: D_00A57860[2] = D_00A57860[1] = D_00A57860[0] = 0x22; D_00A578BC = CARD_PACK_SPECIAL_CARDS; break;
        case CARD_PACK_BS1:
            D_00A578BC = 0;
            rareLeft = CARD_PACK_RARE_DRAWS;
            uncommonLeft = CARD_PACK_UNCOMMON_DRAWS;
            while (rareLeft > 0) {
                pick = xglSRand() & (BS1_RARE_COUNT - 1);
                duplicate = 0;
                for (j = 0; j < D_00A578BC; j += 1) {
                    if (BS1RariLst[pick] == D_00A57860[j]) {
                        duplicate = 1;
                        break;
                    }
                }
                if (duplicate == 0) {
                    D_00A57860[D_00A578BC] = BS1RariLst[pick];
                    D_00A578BC += 1;
                    rareLeft -= 1;
                }
            }
            while (uncommonLeft > 0) {
                pick = xglSRand() % BS1_UNCOMMON_COUNT;
                duplicate = 0;
                for (j = 0; j < D_00A578BC; j += 1) {
                    if (BS1UncoLst[pick] == D_00A57860[j]) {
                        duplicate = 1;
                        break;
                    }
                }
                if (duplicate == 0) {
                    D_00A57860[D_00A578BC] = BS1UncoLst[pick];
                    D_00A578BC += 1;
                    uncommonLeft -= 1;
                }
            }
            while (D_00A578BC < CARD_PACK_DRAW_SIZE) {
                pick = xglSRand() % BS1_COMMON_COUNT;
                duplicate = 0;
                for (j = 0; j < D_00A578BC; j += 1) {
                    if (BS1ComoLst[pick] == D_00A57860[j]) {
                        duplicate = 1;
                        break;
                    }
                }
                if (duplicate == 0) {
                    D_00A57860[D_00A578BC] = BS1ComoLst[pick];
                    D_00A578BC += 1;
                }
            }
            break;
        case CARD_PACK_BS2:
            D_00A578BC = 0;
            rareLeft = CARD_PACK_RARE_DRAWS;
            uncommonLeft = CARD_PACK_UNCOMMON_DRAWS;
            while (rareLeft > 0) {
                pick = xglSRand() % BS2_RARE_COUNT;
                duplicate = 0;
                for (j = 0; j < D_00A578BC; j += 1) {
                    if (BS2RariLst[pick] == D_00A57860[j]) {
                        duplicate = 1;
                        break;
                    }
                }
                if (duplicate == 0) {
                    D_00A57860[D_00A578BC] = BS2RariLst[pick];
                    D_00A578BC += 1;
                    rareLeft -= 1;
                }
            }
            while (uncommonLeft > 0) {
                pick = xglSRand() % BS2_UNCOMMON_COUNT;
                duplicate = 0;
                for (j = 0; j < D_00A578BC; j += 1) {
                    if (BS2UncoLst[pick] == D_00A57860[j]) {
                        duplicate = 1;
                        break;
                    }
                }
                if (duplicate == 0) {
                    D_00A57860[D_00A578BC] = BS2UncoLst[pick];
                    D_00A578BC += 1;
                    uncommonLeft -= 1;
                }
            }
            while (D_00A578BC < CARD_PACK_DRAW_SIZE) {
                pick = xglSRand() % BS2_COMMON_COUNT;
                duplicate = 0;
                for (j = 0; j < D_00A578BC; j += 1) {
                    if (BS2ComoLst[pick] == D_00A57860[j]) {
                        duplicate = 1;
                        break;
                    }
                }
                if (duplicate == 0) {
                    D_00A57860[D_00A578BC] = BS2ComoLst[pick];
                    D_00A578BC += 1;
                }
            }
            break;
        default:
            /* CARD_PACK_ST1: one random rare, then the fixed starter cards. */
            D_00A578BC = 0;
            pick = xglSRand() % ST1_RARE_COUNT;
            D_00A57860[D_00A578BC] = ST1RariLst[pick];
            D_00A578BC += 1;
            for (i = 0; i < ST1_CARD_COUNT; i += 1) {
                D_00A57860[D_00A578BC++] = ST1CardLst[i];
            }
            break;
        }
        /* i starts the search below; the original clears it before dataEvtBoxDec (0x00a1dc4c), and starting the
         * search from the loop header instead changes the code size. */
        i = 0;
        dataEvtBoxDec(D_00A578B0 + CARD_PACK_EVENT_BASE);
        D_00A57848[D_00A578B0]--;
        D_00A578B0 = -1;
        D_00A578B4 -= 1;
        if (D_00A578B4 > 0) {
            for (; i < D_00A578B4; i += 1) {
                if (D_00A57848[i] != 0) {
                    D_00A578B0 = i;
                    break;
                }
            }
        }
        work->fadeOutTimer = CARD_FADE_TICKS;
        work->faseStep += 1;
        break;
    case CARD_OPEN_STEP_REVEAL:
        cardOffset[0] = cardOffset[2] = 0.0f;
        cardOffset[1] = -0.2f;
        cardOffset[3] = 1.0f;
        D_00A578C4 = 0;
        xglMatrixTrans(placement, cardMatrix, cardOffset);
        COPCardDispSub(0, (int) work, placement, D_00A57860, D_00A578BC, 0);
        CardFadeIn(work);
        if (work->fadeOutTimer == 0) {
            work->faseStep += 1;
        }
        break;
    case CARD_OPEN_STEP_BROWSE:
        if (work->command.input[0].directions & CARD_OPEN_PAD_NEXT) {
            if (D_00A578BC - 1 > D_00A578C4) {
                xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
                D_00A578C4 += 1;
            }
        }
        if ((work->command.input[0].directions & CARD_OPEN_PAD_PREV) && D_00A578C4 > 0) {
            xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
            D_00A578C4 -= 1;
        }
        cardOffset[0] = cardOffset[2] = 0.0f;
        cardOffset[1] = -0.2f;
        cardOffset[3] = 1.0f;
        xglMatrixTrans(placement, cardMatrix, cardOffset);
        COPCardDispSub(1, (int) work, placement, D_00A57860, D_00A578BC, D_00A578C4);
        if (work->command.input[0].pressed & CARD_PAD_ANY_RESPONSE) {
            work->fadeOutTimer = CARD_FADE_TICKS;
            work->faseStep += 1;
            xglSoundEffectNormalID(CARD_SE_CANCEL, 0);
        }
        break;
    case CARD_OPEN_STEP_STORE:
        CardFadeOut(work);
        if (work->fadeOutTimer == 0) {
            int added = 0;
            int k; /* a separate variable: reusing j changes the code */
            D_00A578C0 = 0;
            for (i = 0; i < D_00A578BC; i += 1) {
                /* A nonzero result means the collection cannot take another copy: it is sold instead. */
                if (CardAddList(work, D_00A57860[i]) != 0) {
                    dataMoneyBoxInc(CARD_SELL_PRICE);
                    D_00A578C0 += 1;
                    D_00A57860[i] = -1;
                } else {
                    added += 1;
                }
            }
            /* Close the gaps left by the sold copies: shift the tail down one slot until slot i is a kept card. */
            for (i = 0; i < added; i += 1) {
                while (D_00A57860[i] < 0) {
                    if (i < D_00A578BC) {
                        for (k = i; k < D_00A578BC; k += 1) {
                            D_00A57860[k] = D_00A57860[k + 1];
                        }
                    }
                }
            }
            D_00A578BC = added;
            /* Unpack the saved counts into ownedCount. */
            for (i = 0; i < CARD_ID_COUNT; i += 1) {
                ((s16 *) work->ownedCount)[i] = (((CardOpenSaveView *) work->save)->cardCounts[i / CARD_COUNTS_PER_BYTE] >>
                                   ((i % CARD_COUNTS_PER_BYTE) * CARD_COUNT_BITS)) & CARD_COUNT_MASK;
            }
            if (D_00A578C0 != 0) {
                work->fadeOutTimer = CARD_FADE_TICKS;
                work->faseStep += 1;
            } else {
                work->faseStep = CARD_OPEN_STEP_FADE_IN;
                work->fadeOutTimer = CARD_FADE_TICKS;
            }
        }
        break;
    case CARD_OPEN_STEP_REVEAL_KEPT:
        D_00A578C4 = 0;
        cardOffset[0] = cardOffset[2] = 0.0f;
        cardOffset[1] = -0.2f;
        cardOffset[3] = 1.0f;
        xglMatrixTrans(placement, cardMatrix, cardOffset);
        if (D_00A578BC != 0) {
            COPCardDispSub(0, (int) work, placement, D_00A57860, D_00A578BC, 0);
        }
        CardFadeIn(work);
        if (work->fadeOutTimer == 0) {
            work->faseStep += 1;
        }
        break;
    case CARD_OPEN_STEP_SOLD:
        if (D_00A578C0 != 0) {
            xglFontPrint(24, 312, CARD_TEXT_COLOR, "\v\x19\x03\r\0You can only hold up to 3 identical cards.");
            /* D_00A578C0 as two full-width digits, then the proceeds D_00A578C0 * CARD_SELL_PRICE as up to three. */
            digits[0] = ZenkakuListA[(D_00A578C0 / 10) * ZENKAKU_GLYPH_SIZE];
            digits[1] = ZenkakuListA[(D_00A578C0 / 10) * ZENKAKU_GLYPH_SIZE + 1];
            digits[2] = ZenkakuListB[(D_00A578C0 % 10) * ZENKAKU_GLYPH_SIZE];
            digits[3] = ZenkakuListB[(D_00A578C0 % 10) * ZENKAKU_GLYPH_SIZE + 1];
            digits[4] = 0;
            xglFontPrint(24, 336, CARD_TEXT_COLOR, digits);
            xglFontPrint(48, 336, CARD_TEXT_COLOR, "\v\x19\x03\r\0 cards sold at ");
            digits[0] = ZenkakuListA[(D_00A578C0 / (100 / CARD_SELL_PRICE)) * ZENKAKU_GLYPH_SIZE];
            digits[1] = ZenkakuListA[(D_00A578C0 / (100 / CARD_SELL_PRICE)) * ZENKAKU_GLYPH_SIZE + 1];
            digits[2] = 0;
            xglFontPrint(160, 336, CARD_TEXT_COLOR, digits);
            if (D_00A578C0 / (100 / CARD_SELL_PRICE) != 0) {
                digits[0] = ZenkakuListB[((D_00A578C0 * CARD_SELL_PRICE) % 100 / 10) * ZENKAKU_GLYPH_SIZE];
                digits[1] = ZenkakuListB[((D_00A578C0 * CARD_SELL_PRICE) % 100 / 10) * ZENKAKU_GLYPH_SIZE + 1];
                digits[2] = 0;
                xglFontPrint(168, 336, CARD_TEXT_COLOR, digits);
            } else {
                digits[0] = ZenkakuListA[((D_00A578C0 * CARD_SELL_PRICE) % 100 / 10) * ZENKAKU_GLYPH_SIZE];
                digits[1] = ZenkakuListA[((D_00A578C0 * CARD_SELL_PRICE) % 100 / 10) * ZENKAKU_GLYPH_SIZE + 1];
                digits[2] = 0;
                xglFontPrint(168, 336, CARD_TEXT_COLOR, digits);
            }
            /* The two arms below are identical in the original: the units digit is printed
             * from ZenkakuListB whether or not the tens digit is zero. */
            if ((D_00A578C0 * CARD_SELL_PRICE) % 100 / 10 != 0) {
                digits[0] = ZenkakuListB[((D_00A578C0 * CARD_SELL_PRICE) % 100 % 10) * ZENKAKU_GLYPH_SIZE];
                digits[1] = ZenkakuListB[((D_00A578C0 * CARD_SELL_PRICE) % 100 % 10) * ZENKAKU_GLYPH_SIZE + 1];
                digits[2] = 0;
                xglFontPrint(176, 336, CARD_TEXT_COLOR, digits);
            } else {
                digits[0] = ZenkakuListB[((D_00A578C0 * CARD_SELL_PRICE) % 100 % 10) * ZENKAKU_GLYPH_SIZE];
                digits[1] = ZenkakuListB[((D_00A578C0 * CARD_SELL_PRICE) % 100 % 10) * ZENKAKU_GLYPH_SIZE + 1];
                digits[2] = 0;
                xglFontPrint(176, 336, CARD_TEXT_COLOR, digits);
            }
            xglFontPrint(192, 336, CARD_TEXT_COLOR, "\x19\x03G.");
            if (*(u64 *) &work->command.input[0] & CARD_PAD_ANY_RESPONSE_WORD) {
                xglSoundEffectNormalID(CARD_SE_OK, 0);
                if (D_00A578BC != 0) {
                    work->faseStep += 1;
                } else {
                    work->faseStep = CARD_OPEN_STEP_LEAVE;
                    work->fadeOutTimer = CARD_FADE_TICKS;
                }
            }
        } else {
            work->faseStep += 1;
        }
        if (D_00A578BC != 0) {
            D_00A578C4 = 0;
            cardOffset[0] = cardOffset[2] = 0.0f;
            cardOffset[1] = -0.2f;
            cardOffset[3] = 1.0f;
            xglMatrixTrans(placement, cardMatrix, cardOffset);
            COPCardDispSub(0, (int) work, placement, D_00A57860, D_00A578BC, 0);
        }
        break;
    case CARD_OPEN_STEP_BROWSE_KEPT:
        if (work->command.input[0].directions & CARD_OPEN_PAD_NEXT) {
            if (D_00A578BC - 1 > D_00A578C4) {
                xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
                D_00A578C4 += 1;
            }
        }
        if ((work->command.input[0].directions & CARD_OPEN_PAD_PREV) && D_00A578C4 > 0) {
            xglSoundEffectNormalID(CARD_SE_CURSOR, 0);
            D_00A578C4 -= 1;
        }
        cardOffset[0] = cardOffset[2] = 0.0f;
        cardOffset[1] = -0.2f;
        cardOffset[3] = 1.0f;
        xglMatrixTrans(placement, cardMatrix, cardOffset);
        COPCardDispSub(1, (int) work, placement, D_00A57860, D_00A578BC, D_00A578C4);
        if (*(u64 *) &work->command.input[0] & CARD_PAD_ANY_RESPONSE_WORD) {
            work->fadeOutTimer = CARD_FADE_TICKS;
            work->faseStep += 1;
            xglSoundEffectNormalID(CARD_SE_CANCEL, 0);
        }
        break;
    case CARD_OPEN_STEP_LEAVE:
        CardFadeOut(work);
        if (work->fadeOutTimer == 0) {
            work->faseStep = CARD_OPEN_STEP_FADE_IN;
            work->fadeOutTimer = CARD_FADE_TICKS;
        }
        break;
    }
}

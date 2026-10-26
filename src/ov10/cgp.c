/*
 * OV10 original TU 8: 0x00a21768..0x00a324b8 (68 functions)
 */
#include "common.h"
#include "cgp.h"

typedef struct CGPCursorPosition {
    s16 zone;
    s16 index;
    s16 cardId;
    s16 stackedCard;
} CGPCursorPosition;

extern s16 CGPCPTbl[9];

/* Movement record fields used by the slot swap at 0x00a25b70. */
typedef struct CardMoveLayer {
    u8 state;
    u8 preservedByte01;
    u8 boostCount;
    u8 life;
    u8 preservedByte04;
    u8 damageTimer;
    s16 cardId;
    u8 unmodeled_08[2];
} CardMoveLayer;

typedef struct CardMoveWork {
    u8 flags;
    u8 moveStep;
    u8 power;
    u8 powerLimit;
    u8 unmodeled_04[4];
    CardMoveLayer layers[40];
    float position[3];
    float velocity[3];
    float shuntPosition[3];
} CardMoveWork;

/* The same hand bytes have two evidenced owner views in this TU. */
typedef union CardPlayHandViews {
    CardHand costView;
    CardPlayHand maintenanceView;
} CardPlayHandViews;

typedef struct CardPlayDeploymentStack {
    u8 flags;
    u8 moveTimer;
    u8 power;
    u8 powerLimit;
    u8 unmodeled_04[4];
    CardBattleLayer layers[40];
    float position[3];
    float step[3];
} CardPlayDeploymentStack;

typedef struct CardPlayDeploymentSlot {
    u8 unmodeled_00[0x0C];
    CardPlayDeploymentStack stack;
} CardPlayDeploymentSlot;

typedef struct CardMoveRecord {
    u8 unmodeled_00[3];
    u8 moveTimer;
    float position[3];
    float step[3];
} CardMoveRecord;

typedef struct CardPlayField {
    CardPlayHandViews hand;
    u8 unmodeled_019A[0x0880 - 0x019A];
    CardPlayDeploymentSlot disposal[4];
    u8 unmodeled_0F70[0x0F7C - 0x0F70];
    s16 operations[40];
    s16 operationFlags[40];
    s16 operationDuration[40];
    u8 unmodeled_106C[0x14CC - 0x106C];
    CardMoveRecord operationMove[4];
    u8 unmodeled_153C[0x192C - 0x153C];
    CardMoveRecord handMove[40];
} CardPlayField;

extern float HandPos1P[];
extern float HandPos2P[];

extern void CGPCursor2Pos(CGPCursorPosition *position, CardGameWork *work);

extern int CardPlayIdentityCheck(CardGameWork *work, CardHand *hand, u16 identity);
extern int CardPlayRecavery(CardHand *hand);
extern s16 *CPCSearchCommWk(u16 side, CardGameWork *work);
extern s32 CardPlayCommandPlay(u16 side, u16 command, CardGameWork *work,
                               s16 operand, s16 target);
extern int CardPlayCntOpe(CardPlaySide *side, int opeId);
extern float DisposePos1P[][3];
extern float OperationPos1P[][3];

extern void CGPDispErrorMessPlus(s32 side, u16 reason, s32 value);
extern s32 CGPCursor2Area(CardGameWork *work);
extern void xglMatrixUnit(float matrix[4][4]);
extern void xglMatrixTrans(Matrix4 destination, const Matrix4 source,
                           const float position[4]);
extern void xglMatrixScale(Matrix4 destination, const Matrix4 source,
                           const float scale[4]);
extern void nmlModelSetLight(float color[4][4], float direction[4][4]);
extern void nmlModelSetPlace(float matrix[4][4]);
extern void nmlModelSetTexture(void *texture);
extern void nmlModelSetToumei(s32 enabled);
extern void nmlModelSetTransparency(float transparency);
extern void nmlModelEntry(s32 model);
extern void nmlModelEntryCard(void *model);
extern void *CPISMdlLst[];
extern void *CPIPhaseMdlLst[];
extern float asColor[4][4];
extern float asDir[4][4];
extern float CLP1Color[4][4];
extern float CLP2Color[4][4];
extern char D_00A4E530[];
extern char D_00A4E538[];

/*
 * Shunt one side's cards aside before the computer opponent moves
 * (CardEnemyMove, ov10 0x00a32570, passes it the player's or the enemy's
 * CardPlaySide). Every occupied slot hands its effect position to the shunt
 * position one slot record further on, the disposal board first and the
 * battle board after it; the last disposal slot writes CardPlaySide's
 * lastShuntPosition, the record that follows the disposal board.
 */
void CGPShuntPosSub(CardPlaySide *side)
{
    s32 i;

    for (i = 0; i < 4; i++) {
        if (side->disposal[i].battle.layers[0].cardId >= 0) {
            side->disposal[i + 1].shuntPosition[0] = side->disposal[i].battle.position[0];
            side->disposal[i + 1].shuntPosition[1] = side->disposal[i].battle.position[1];
            side->disposal[i + 1].shuntPosition[2] = side->disposal[i].battle.position[2];
        }
    }

    for (i = 0; i < 4; i++) {
        if (side->battle[i].battle.layers[0].cardId >= 0) {
            side->battle[i + 1].shuntPosition[0] = side->battle[i].battle.position[0];
            side->battle[i + 1].shuntPosition[1] = side->battle[i].battle.position[1];
            side->battle[i + 1].shuntPosition[2] = side->battle[i].battle.position[2];
        }
    }
}

extern s32 CardPlayHandCnt(CardHand *hand);
extern s32 CardPlayCntBtl(CardHand *hand, s16 cardId);

/*
 * Where the cards in play are moving to after the board shifted. Each occupied
 * slot keeps the point it was heading for, takes the shunt position of the slot
 * after it as its new effect position, and covers what is left of the way in ten
 * steps.
 */
void CGPRecalcPosSub(CardPlaySide *side)
{
    CardLayerStack *stack;
    float dest[3];
    s32 i;

    for (i = 0; i < 4; i++) {
        stack = &side->disposal[i].battle;
        if (stack->layers[0].cardId >= 0) {
            dest[0] = stack->position[0] + stack->step[0] * stack->moveFrames;
            dest[1] = stack->position[1] + stack->step[1] * stack->moveFrames;
            dest[2] = stack->position[2] + stack->step[2] * stack->moveFrames;
            stack->position[0] = side->disposal[i + 1].shuntPosition[0];
            stack->position[1] = side->disposal[i + 1].shuntPosition[1];
            stack->position[2] = side->disposal[i + 1].shuntPosition[2];
            stack->step[0] = (dest[0] - stack->position[0]) / 10.0f;
            stack->step[1] = (dest[1] - stack->position[1]) / 10.0f;
            stack->step[2] = (dest[2] - stack->position[2]) / 10.0f;
        }
    }
    for (i = 0; i < 4; i++) {
        stack = &side->battle[i].battle;
        if (stack->layers[0].cardId >= 0) {
            dest[0] = stack->position[0] + stack->step[0] * stack->moveFrames;
            dest[1] = stack->position[1] + stack->step[1] * stack->moveFrames;
            dest[2] = stack->position[2] + stack->step[2] * stack->moveFrames;
            stack->position[0] = side->battle[i + 1].shuntPosition[0];
            stack->position[1] = side->battle[i + 1].shuntPosition[1];
            stack->position[2] = side->battle[i + 1].shuntPosition[2];
            stack->step[0] = (dest[0] - stack->position[0]) / 10.0f;
            stack->step[1] = (dest[1] - stack->position[1]) / 10.0f;
            stack->step[2] = (dest[2] - stack->position[2]) / 10.0f;
        }
    }
}

extern void CardSetEffect(s32 side, CardGameWork *work, u8 code, s8 value, s32 option,
                          float *position, CardLayerStack *stack);
extern void xglSoundEffectNormalID(int sound_id, int variant);

/*
 * Command 143 takes two life off every card standing on a battle or disposal
 * slot of either side, clamped at zero, and starts effects 5 and 13 at each of
 * those stacks, the card's own side for its own cards and the other side for
 * the opponent's.
 */
void CCO143ExecSub(s32 side, CardGameWork *work, CardPlaySide *ownSide,
                   CardPlaySide *foeSide)
{
    s32 otherSide;
    s32 i;

    xglSoundEffectNormalID(0x20008, 0);
    otherSide = (side == 0);
    for (i = 0; i < 4; i++) {
        if (ownSide->battle[i].battle.layers[0].cardId >= 0) {
            ownSide->battle[i].battle.layers[0].life -= 2;
            if (ownSide->battle[i].battle.layers[0].life < 0) {
                ownSide->battle[i].battle.layers[0].life = 0;
            }
            CardSetEffect(side, work, 5, 0, 0, ownSide->battle[i].battle.position, 0);
            CardSetEffect(side, work, 13, 2, 0, ownSide->battle[i].battle.position,
                          &ownSide->battle[i].battle);
        }
        if (foeSide->battle[i].battle.layers[0].cardId >= 0) {
            foeSide->battle[i].battle.layers[0].life -= 2;
            if (foeSide->battle[i].battle.layers[0].life < 0) {
                foeSide->battle[i].battle.layers[0].life = 0;
            }
            CardSetEffect(otherSide, work, 5, 0, 0, foeSide->battle[i].battle.position, 0);
            CardSetEffect(otherSide, work, 13, 2, 0, foeSide->battle[i].battle.position,
                          &foeSide->battle[i].battle);
        }
    }
    for (i = 0; i < 4; i++) {
        if (ownSide->disposal[i].battle.layers[0].cardId >= 0) {
            ownSide->disposal[i].battle.layers[0].life -= 2;
            if (ownSide->disposal[i].battle.layers[0].life < 0) {
                ownSide->disposal[i].battle.layers[0].life = 0;
            }
            CardSetEffect(side, work, 5, 0, 0, ownSide->disposal[i].battle.position, 0);
            CardSetEffect(side, work, 13, 2, 0, ownSide->disposal[i].battle.position,
                          &ownSide->disposal[i].battle);
        }
        if (foeSide->disposal[i].battle.layers[0].cardId >= 0) {
            foeSide->disposal[i].battle.layers[0].life -= 2;
            if (foeSide->disposal[i].battle.layers[0].life < 0) {
                foeSide->disposal[i].battle.layers[0].life = 0;
            }
            CardSetEffect(otherSide, work, 5, 0, 0, foeSide->disposal[i].battle.position, 0);
            CardSetEffect(otherSide, work, 13, 2, 0, foeSide->disposal[i].battle.position,
                          &foeSide->disposal[i].battle);
        }
    }
}

extern int printf(const char *format, ...);
extern char D_00A4E4F0[]; /* "Yama[%2d]:" */
extern char D_00A4E500[]; /* "%3d," */
extern char D_00A4E508[]; /* "\n" */
extern char D_00A4E510[]; /* "Junk[%2d]:" */

void CGPPrintYama(CardPlaySide *side)
{
    s32 col;
    s32 i;

    col = 0;
    for (i = 0; i < 40; i++) {
        if (col == 0) {
            printf(D_00A4E4F0, i);
        }
        col++;
        printf(D_00A4E500, side->deck[i]);
        if (col == 10) {
            col = 0;
            printf(D_00A4E508);
        }
    }
    printf(D_00A4E508);
}

void CGPPrintJunk(CardPlaySide *side)
{
    s32 col;
    s32 i;

    col = 0;
    for (i = 0; i < 40; i++) {
        if (col == 0) {
            printf(D_00A4E510, i);
        }
        col++;
        printf(D_00A4E500, side->junk[i]);
        if (col == 10) {
            col = 0;
            printf(D_00A4E508);
        }
    }
    printf(D_00A4E508);
}

extern int printf(const char *format, ...);
extern char D_00A4E500[]; /* "%3d," */
extern char D_00A4E508[]; /* "\n" */
extern char D_00A4E520[]; /* "Sute[%2d]:" */

void CGPPrintSute(CardPlaySide *side)
{
    s32 col;
    s32 i;

    col = 0;
    for (i = 0; i < 40; i++) {
        if (col == 0) {
            printf(D_00A4E520, i);
        }
        col++;
        printf(D_00A4E500, side->sute[i]);
        if (col == 10) {
            col = 0;
            printf(D_00A4E508);
        }
    }
    printf(D_00A4E508);
}

void CGPSetMessage(CardGameWork *work, s32 index)
{
    work->mess = OLMessTbl[index];
}

void CGPSetInterruptMess(CardGameWork *work, s32 index)
{
    work->interruptMess = CardPlayTMessList[index];
}

void CGPSetPermanentMess(CardGameWork *work, s32 index)
{
    if (index == 0) {
        work->permanentMess = 0;
        return;
    }
    work->permanentMess = PermMessTbl[index];
}

void CGPSetErrorMess(s32 code, CardGameWork *work, s32 reason)
{
    work->errorMessCode = code;
    work->errorMessKind = 1;
    work->errorMessReason = reason;
    work->errorMessValue = -1;
}

void CGPSetErrorMessPlus(s8 code, CardGameWork *work, u8 reason, s16 value)
{
    work->errorMessCode = code;
    work->flags |= CGP_FLAG_ERROR_PLUS;
    work->errorMessKind = 3;
    work->errorMessReason = reason;
    work->errorMessValue = value;
}

/*
 * Screen position and scale CGPDispErrorMessCore (still assembler in this TU)
 * places the rendered message with; declared locally next to the callers
 * that build one on their own stack.
 */
typedef struct CGPErrorLayout {
    float x;
    float y;
    float scaleX;
    float scaleY;
} CGPErrorLayout;

typedef struct ErrorMessEntry {
    char *unk0;
    char *unk4;
    char *unk8;
    char *unkC;
    char *unk10;
} ErrorMessEntry;
extern ErrorMessEntry ErrorMessTbl[];
extern float Player1Color[4][4];
extern float Player2Color[4][4];
static inline char *EMsgAt(s32 off, s32 k)
{
    return *(char **)((char *)(off + k) + (int)ErrorMessTbl);
}

void CGPDispErrorMessCore(CardGameWork *work, u16 reason, s32 x, s32 y, CGPErrorLayout *layout)
{
    float sp[4][4];
    float sc[4];
    u16 px;
    u16 py;

    px = x & 0xFFFF;
    py = y & 0xFFFF;
    xglFontPrint(px, py, 0xFFF0, D_00A4E530);
    xglFontPrint(0, 0, 0, D_00A4E538);
    xglFontPrint(px, py, 0xFFF0, ErrorMessTbl[reason].unk0);
    if (EMsgAt(reason * 20, 4) != 0) {
        xglFontPrint(px, py + 0x18, 0xFFF0, D_00A4E530);
        xglFontPrint(0, 0, 0, D_00A4E538);
        xglFontPrint(px, py + 0x18, 0xFFF0, EMsgAt(reason * 20, 4));
    }
    if (EMsgAt(reason * 20, 8) != 0) {
        xglFontPrint(px, py + 0x30, 0xFFF0, D_00A4E530);
        xglFontPrint(0, 0, 0, D_00A4E538);
        xglFontPrint(px, py + 0x30, 0xFFF0, EMsgAt(reason * 20, 8));
    }
    if (EMsgAt(reason * 20, 12) != 0) {
        xglFontPrint(px, py + 0x48, 0xFFF0, D_00A4E530);
        xglFontPrint(0, 0, 0, D_00A4E538);
        xglFontPrint(px, py + 0x48, 0xFFF0, EMsgAt(reason * 20, 12));
    }
    if (EMsgAt(reason * 20, 16) != 0) {
        xglFontPrint(px, py + 0x60, 0xFFF0, D_00A4E530);
        xglFontPrint(0, 0, 0, D_00A4E538);
        xglFontPrint(px, py + 0x60, 0xFFF0, EMsgAt(reason * 20, 16));
    }
    xglMatrixUnit(sp);
    xglMatrixTrans(sp, (const float (*)[4]) sp, &layout->x);
    sc[0] = 1.35f;
    sc[3] = 1.0f;
    sc[2] = 1.0f;
    sc[1] = 1.0f;
    xglMatrixScale(sp, (const float (*)[4]) sp, sc);
    nmlModelSetPlace(sp);
    if ((s32) work >= 0) {
        if (work == 0) {
            nmlModelSetLight((f32 (*)[4]) Player1Color, asDir);
        } else {
            nmlModelSetLight((f32 (*)[4]) Player2Color, asDir);
        }
    }
    nmlModelSetTexture((void *)0x01DB5800);
    nmlModelEntryCard((void *)0x01DA2B00);
}

extern void CGPDispErrorMessCore(CardGameWork *work, u16 reason, s32 x, s32 y,
                                  CGPErrorLayout *layout);

void CGPDispErrorMess3(CardGameWork *work, s32 reason)
{
    CGPErrorLayout layout;

    layout.x = 1.1f;
    layout.y = 0.0f;
    layout.scaleX = 0.5f;
    layout.scaleY = 1.0f;
    CGPDispErrorMessCore(work, reason, 230, 164, &layout);
}

extern void CardDisp(Matrix4 matrix, s16 card, s32 mode, s32 layer);

void CGPDispErrorMessPlus(s32 side, u16 reason, s32 value)
{
    Matrix4 place;
    float vec[4];

    CGPDispErrorMess3((CardGameWork *) side, reason);
    xglMatrixUnit(place);
    vec[3] = 1.0f;
    vec[0] = -1.5f;
    vec[1] = 0.2f;
    vec[2] = 0.3f;
    xglMatrixTrans(place, (const float (*)[4]) place, vec);
    vec[1] = 0.8f;
    vec[0] = 0.8f;
    vec[2] = 1.0f;
    xglMatrixScale(place, (const float (*)[4]) place, vec);
    CardDisp(place, (s16) value, 1, 5);
}

void CGPDispErrorMess(CardGameWork *work, s32 reason)
{
    CGPErrorLayout layout;

    layout.x = 0.0f;
    layout.y = 0.0f;
    layout.scaleX = 0.5f;
    layout.scaleY = 1.0f;
    CGPDispErrorMessCore(work, reason, 124, 164, &layout);
}

void CGPDispErrorMess2(CardGameWork *work, s32 reason)
{
    CGPErrorLayout layout;

    layout.x = 0.0f;
    layout.y = 0.48f;
    layout.scaleX = 0.5f;
    layout.scaleY = 1.0f;
    CGPDispErrorMessCore(work, reason, 124, 116, &layout);
}

/*
 * Draws everything the card game shows beside the board: the message line, the
 * error message of the side that caused it, the information panel of the phase
 * being played with the turn counter on it, and the seven phase-guide models of
 * the side whose phase is running. Phases 0..6 belong to the player and 7..13
 * to the opponent, and a negative phase means no side is playing.
 */
void CardPlayDispInfo(CardGameWork *workAddress)
{
    CardGameWork *work = workAddress;
    float place[4][4];
    float guidePlace[4][4];
    float shadowPos[4];
    float pos[4];
    float guideScale[4];
    float color1P[4][4];
    float color2P[4][4];
    float turnColor[4][4];
    char *text;
    s32 flag;
    void *model;
    s32 area;
    float transparency;
    s32 tens;
    s32 i;

    text = work->mess;
    if (work->interruptMess != 0) {
        text = work->interruptMess;
    }
    if (work->permanentMess != 0) {
        text = work->permanentMess;
    }

    if (work->errorMessKind != 0) {
        s32 bothSides;

        bothSides = 0;
        if (work->errorMessCode == 0) {
            if (work->flags & 0x40) {
                bothSides = 1;
            }
        } else {
            if (work->flags & 0x20) {
                bothSides = 1;
            }
        }
        if (bothSides == 0) {
            if (work->errorMessKind & CGP_FLAG_ERROR_PLUS) {
                CGPDispErrorMessPlus(work->errorMessCode, work->errorMessReason,
                                     work->errorMessValue);
            } else {
                CGPDispErrorMess((CardGameWork *) (s32) work->errorMessCode,
                                 work->errorMessReason);
            }
        } else if (work->errorMessKind & CGP_FLAG_ERROR_PLUS) {
            CGPDispErrorMessPlus(-1, work->errorMessReason, work->errorMessValue);
        } else {
            CGPDispErrorMess((CardGameWork *) -1, work->errorMessReason);
        }
    }

    flag = 1;
    if (work->infoPanelPhase == 1) {
        flag = work->resultWait < 4;
    }
    switch ((s16) work->command.phase) {
    case 2:
    case 4:
    case 9:
    case 11:
        if ((u32) (work->command.phaseTimer - 20) < 10) {
            flag = 0;
        }
        break;
    }
    if (flag && text != 0) {
        xglFontPrint(2, 2, 0xFFF0, D_00A4E530);
        xglFontPrint(0, 0, 0, D_00A4E538);
        xglFontPrint(2, 2, 0xFFF0, text);
    }

    model = CPISMdlLst[(s16) work->command.phase];
    flag = 1;
    if ((s16) work->command.phase < 7) {
        if ((s16) work->command.phase >= 0) {
            flag = 0;
        }
    }
    switch (work->infoPanelPhase) {
    case 0:
    default:
        work->infoPanelPos[0] = 0.0f;
        work->infoPanelPos[1] = 0.0f;
        work->infoPanelPos[2] = 0.5f;
        work->infoPanelPos[3] = 1.0f;
        work->infoPanelPosStep[0] = 0.0f;
        work->infoPanelPosStep[1] = 0.0f;
        work->infoPanelPosStep[2] = 0.0f;
        work->infoPanelPosStep[3] = 1.0f;
        work->infoPanelScale[0] = 20.0f;
        work->infoPanelScale[1] = 20.0f;
        work->infoPanelScale[2] = 1.0f;
        work->infoPanelScale[3] = 1.0f;
        work->infoPanelScaleStep[0] = -1.9f;
        work->infoPanelScaleStep[1] = -1.9f;
        work->infoPanelScaleStep[2] = 0.0f;
        work->infoPanelScaleStep[3] = 1.0f;
        work->resultWait = 10;
        work->infoPanelPhase = 1;
        break;
    case 1:
        if (work->resultWait <= 0) {
            work->resultWait = 15;
            work->infoPanelPhase++;
        } else {
            work->resultWait--;
            work->infoPanelScale[0] += work->infoPanelScaleStep[0];
            work->infoPanelScale[1] += work->infoPanelScaleStep[1];
            work->infoPanelScale[2] += work->infoPanelScaleStep[2];
            work->infoPanelPos[0] += work->infoPanelPosStep[0];
            work->infoPanelPos[1] += work->infoPanelPosStep[1];
            work->infoPanelPos[2] += work->infoPanelPosStep[2];
        }
        break;
    case 2:
        if (work->resultWait <= 0) {
            work->infoPanelPhase++;
            work->resultWait = 6;
            if ((s16) work->command.phase < 7) {
                if ((s16) work->command.phase >= 0) {
                    work->infoPanelPosStep[0] = -0.25833334f;
                    work->infoPanelPosStep[1] = 0.27166668f;
                } else {
                    work->infoPanelPosStep[0] = 0.25833334f;
                    work->infoPanelPosStep[1] = -0.305f;
                }
            } else {
                work->infoPanelPosStep[0] = 0.25833334f;
                work->infoPanelPosStep[1] = -0.305f;
            }
            work->infoPanelScaleStep[0] = -0.083333336f;
            work->infoPanelPosStep[2] = -0.033333333f;
            work->infoPanelScaleStep[1] = -0.083333336f;
        } else {
            work->resultWait--;
        }
        break;
    case 3:
        if (work->resultWait <= 0) {
            work->infoPanelPhase++;
        } else {
            work->resultWait--;
            work->infoPanelPos[0] += work->infoPanelPosStep[0];
            work->infoPanelPos[1] += work->infoPanelPosStep[1];
            work->infoPanelPos[2] += work->infoPanelPosStep[2];
            work->infoPanelScale[0] += work->infoPanelScaleStep[0];
            work->infoPanelScale[1] += work->infoPanelScaleStep[1];
            work->infoPanelScale[2] += work->infoPanelScaleStep[2];
        }
        break;
    case 4:
        if (work->resultWait > 0) {
            work->resultWait--;
            work->infoPanelPos[0] += work->infoPanelPosStep[0];
            work->infoPanelPos[1] += work->infoPanelPosStep[1];
        }
        break;
    }

    if (((s16) work->command.phase == 5 || (s16) work->command.phase == 12) &&
        work->infoPanelPhase >= 4 && (s16) work->command.phaseTimer >= 10) {
        return;
    }
    if (work->command.flowRequest == 3) {
        return;
    }
    if (work->command.flowRequest == 4) {
        return;
    }
    if (work->command.flowRequest == 5) {
        return;
    }

    xglMatrixUnit(place);
    xglMatrixTrans(place, (const float (*)[4]) place, work->infoPanelPos);
    xglMatrixScale(place, (const float (*)[4]) place, work->infoPanelScale);
    nmlModelSetLight(asColor, asDir);
    nmlModelSetPlace(place);
    nmlModelSetTexture((void *) 0x01F70200);
    if (work->infoPanelPhase == 1) {
        transparency = 1.0f;
        transparency -= (work->resultWait * 0.3f) / 10.0f;
        nmlModelSetToumei(1);
        nmlModelSetTransparency(transparency);
    }
    nmlModelEntryCard(model);

    memset(color1P, 0, sizeof(color1P));
    color1P[0][3] = 1.0f;
    color1P[1][3] = 1.0f;
    color1P[2][3] = 1.0f;
    color1P[3][0] = 0.5f;
    color1P[3][1] = 0.5f;
    color1P[3][2] = 1.0f;
    color1P[3][3] = 1.0f;
    memset(color2P, 0, sizeof(color2P));
    color2P[0][3] = 1.0f;
    color2P[1][3] = 1.0f;
    color2P[2][3] = 1.0f;
    color2P[3][0] = 1.0f;
    color2P[3][1] = 0.4f;
    color2P[3][2] = 0.4f;
    color2P[3][3] = 1.0f;
    memset(turnColor, 0, sizeof(turnColor));
    turnColor[0][3] = 1.0f;
    turnColor[1][3] = 1.0f;
    turnColor[2][3] = 1.0f;
    turnColor[3][0] = 1.0f;
    turnColor[3][1] = 0.9f;
    turnColor[3][2] = 0.4f;
    turnColor[3][3] = 1.0f;

    xglMatrixUnit(place);
    shadowPos[0] = work->infoPanelPos[0];
    shadowPos[1] = work->infoPanelPos[1];
    shadowPos[2] = work->infoPanelPos[2] - 0.01f;
    shadowPos[3] = 1.0f;
    xglMatrixTrans(place, (const float (*)[4]) place, shadowPos);
    xglMatrixScale(place, (const float (*)[4]) place, work->infoPanelScale);
    if (flag == 0) {
        nmlModelSetLight(color1P, asDir);
    } else {
        nmlModelSetLight(color2P, asDir);
    }
    nmlModelSetPlace(place);
    nmlModelSetTexture((void *) 0x01DB5800);
    if (work->infoPanelPhase == 1) {
        transparency = 1.0f;
        transparency -= (work->resultWait * 0.3f) / 10.0f;
        nmlModelSetToumei(1);
        nmlModelSetTransparency(transparency);
    }
    nmlModelEntry(0x01DAD700);

    pos[0] = 0.0f;
    pos[1] = 1.72f;
    pos[2] = 0.0f;
    pos[3] = 1.0f;
    xglMatrixUnit(place);
    xglMatrixTrans(place, (const float (*)[4]) place, pos);
    nmlModelSetLight(turnColor, asDir);
    nmlModelSetPlace(place);
    nmlModelSetTexture((void *) 0x01DB5800);
    nmlModelEntryCard((void *) 0x01DB2D00);

    pos[0] = 0.0f;
    pos[1] = -0.02f;
    pos[2] = 0.05f;
    pos[3] = 1.0f;
    xglMatrixTrans(place, (const float (*)[4]) place, pos);
    nmlModelSetPlace(place);
    nmlModelSetTexture((void *) 0x01F70200);
    nmlModelEntryCard((void *) 0x01921800);

    tens = (work->command.turn + 1) / 10;
    if (tens != 0) {
        model = (void *) (0x01C80000 + (tens << 10));
        pos[0] = 0.213f;
        pos[1] = -0.003f;
        pos[2] = 0.0f;
        xglMatrixTrans(guidePlace, (const float (*)[4]) place, pos);
        nmlModelSetPlace(guidePlace);
        nmlModelSetTexture((void *) 0x01C91000);
        nmlModelEntryCard(model);
    }
    model = (void *) (0x01C80000 + (((work->command.turn + 1) % 10) << 10));
    pos[0] = 0.311f;
    pos[1] = -0.003f;
    pos[2] = 0.0f;
    xglMatrixTrans(guidePlace, (const float (*)[4]) place, pos);
    nmlModelSetPlace(guidePlace);
    nmlModelSetTexture((void *) 0x01C91000);
    nmlModelEntryCard(model);

    pos[0] = -2.3f;
    pos[2] = 0.0f;
    pos[3] = 1.0f;
    guideScale[0] = 0.5f;
    guideScale[1] = 0.5f;
    guideScale[2] = 1.0f;
    guideScale[3] = 1.0f;
    if (work->save->flags & 0x02) {
        return;
    }

    if ((s16) work->command.phase < 7) {
        area = CGPCursor2Area(workAddress);
        pos[1] = 1.53f;
        if (work->rotStageId == 0) {
            if (area == 3 || work->cursorMoveStep[1] != 0) {
                pos[1] = -0.31000006f;
            }
        } else {
            pos[1] = -0.31000006f;
            if (area == 2) {
                pos[1] = 1.53f;
            }
        }
        xglMatrixUnit(place);
        for (i = 0; i < 7; i++) {
            model = CPIPhaseMdlLst[i];
            xglMatrixTrans(guidePlace, (const float (*)[4]) place, pos);
            xglMatrixScale(guidePlace, (const float (*)[4]) guidePlace, guideScale);
            nmlModelSetPlace(guidePlace);
            nmlModelSetLight(asColor, asDir);
            nmlModelSetTexture((void *) 0x01F70200);
            nmlModelEntryCard(model);
            xglMatrixTrans(guidePlace, (const float (*)[4]) place, pos);
            xglMatrixScale(guidePlace, (const float (*)[4]) guidePlace, guideScale);
            if (i == 4) {
                if ((s16) work->command.phase == i) {
                    nmlModelSetLight(CLP2Color, asDir);
                } else {
                    nmlModelSetLight(color2P, asDir);
                }
            } else if ((s16) work->command.phase == i) {
                nmlModelSetLight(CLP1Color, asDir);
            } else {
                nmlModelSetLight(color1P, asDir);
            }
            nmlModelSetPlace(guidePlace);
            nmlModelSetTexture((void *) 0x01DB5800);
            nmlModelEntryCard((void *) 0x01DB0200);
            pos[1] -= 0.23f;
        }
    } else {
        pos[0] = 2.3f;
        area = CGPCursor2Area(workAddress);
        pos[1] = -0.31000006f;
        if (work->rotStageId == 0) {
            if (area == 7 || work->cursorMoveStep[0] != 0) {
                pos[1] = 1.53f;
            }
        } else {
            pos[1] = 1.53f;
            if (area == 6) {
                pos[1] = -0.31000006f;
            }
        }
        xglMatrixUnit(place);
        for (i = 0; i < 7; i++) {
            model = CPIPhaseMdlLst[i];
            xglMatrixTrans(guidePlace, (const float (*)[4]) place, pos);
            xglMatrixScale(guidePlace, (const float (*)[4]) guidePlace, guideScale);
            nmlModelSetPlace(guidePlace);
            nmlModelSetLight(asColor, asDir);
            nmlModelSetTexture((void *) 0x01F70200);
            nmlModelEntryCard(model);
            xglMatrixTrans(guidePlace, (const float (*)[4]) place, pos);
            xglMatrixScale(guidePlace, (const float (*)[4]) guidePlace, guideScale);
            if (i == 4) {
                if ((s16) work->command.phase - 7 == i) {
                    nmlModelSetLight(CLP1Color, asDir);
                } else {
                    nmlModelSetLight(color1P, asDir);
                }
            } else if ((s16) work->command.phase - 7 == i) {
                nmlModelSetLight(CLP2Color, asDir);
            } else {
                nmlModelSetLight(color2P, asDir);
            }
            nmlModelSetPlace(guidePlace);
            nmlModelSetTexture((void *) 0x01DB5800);
            nmlModelEntryCard((void *) 0x01DB0200);
            pos[1] -= 0.23f;
        }
    }
}

int CGPChkFaseLock(CardGameWork *work)
{
    int ret = 1;

    switch ((s16)work->command.phase) {
    case 1:
        if (work->flags & 0x40) {
            ret = 0;
        }
        break;
    case 8:
        if (work->flags & 0x20) {
            ret = 0;
        }
        break;
    case 2:
    case 3:
    case 11:
        if ((work->flags & 0x40) || (s16)work->command.phaseTimer >= 3) {
            ret = 0;
        }
        break;
    case 4:
    case 9:
    case 10:
        if ((work->flags & 0x20) || (s16)work->command.phaseTimer >= 3) {
            ret = 0;
        }
        break;
    case 0:
    case 5:
    case 6:
    case 7:
    case 12:
    case 13:
        ret = 0;
        break;
    }
    return ret;
}

/*
 * Register one effect of the card game. The first entry of the work area's
 * effect table whose code is zero is taken, so nothing happens once all 64
 * are busy. position is where the effect starts (NULL leaves the start each
 * code picks for itself), stack the board slot it travels to.
 */
void CardSetEffect(s32 side, CardGameWork *workAddress, u8 code, s8 value, s32 soundId,
                   float *position, CardLayerStack *stack)
{
    CardGameWork *work = workAddress;
    CardEffect *effect;
    s32 i;

    effect = 0;
    for (i = 0; i < 64; i++) {
        if (work->effects[i].code == 0) {
            effect = &work->effects[i];
            break;
        }
    }
    if (effect == 0) {
        return;
    }

    effect->side = side;
    effect->timer = 15;
    effect->soundId = soundId;
    effect->phase = 0;
    effect->code = code;
    effect->stack = 0;
    if (position != 0) {
        effect->position[0] = position[0];
        effect->position[1] = position[1];
        effect->position[2] = position[2] + 0.1f;
    }
    effect->scale[0] = effect->scale[1] = effect->scale[2] = 1.0f;
    effect->velocity[0] = effect->velocity[1] = effect->velocity[2] = 0.0f;
    effect->scale[3] = 1.0f;
    effect->velocity[3] = 1.0f;
    effect->position[3] = 1.0f;

    switch (code) {
    case 10:
        xglSoundEffectNormalID(0x2000D, 0);
        if (position == 0) {
            if (value == 0) {
                effect->position[1] = -1.81f;
            } else {
                effect->position[1] = 1.81f;
            }
            effect->position[2] = 0.1f;
        }
        effect->timer = 30;
        if (stack != 0) {
            effect->stack = stack;
            effect->velocity[0] = (-stack->position[0] - position[0]) / 15.0f;
            effect->velocity[1] = (-stack->position[1] - position[1]) / 15.0f;
        }
        effect->scale[1] = 1.5f;
        effect->scale[0] = 1.5f;
        effect->scale[2] = 1.0f;
        effect->scale[3] = 1.0f;
        effect->velocity[3] = 1.0f;
        effect->position[3] = 1.0f;
        break;
    case 11:
        effect->stack = stack;
        effect->position[2] = 0.1f;
        effect->position[0] = 0.0f;
        effect->position[1] = 0.0f;
        effect->scale[3] = 1.0f;
        effect->velocity[3] = 1.0f;
        effect->position[3] = 1.0f;
        if (value != 0) {
            if (value >= 100) {
                value = 99;
            }
            effect->value = value;
            effect->velocity[1] = 0.0053333333f;
        } else {
            effect->code = 0;
        }
        break;
    case 15:
        effect->timer = 24;
        effect->stack = stack;
        effect->position[2] = 0.1f;
        effect->position[0] = 0.0f;
        effect->position[1] = 0.0f;
        effect->scale[3] = 1.0f;
        effect->velocity[3] = 1.0f;
        effect->position[3] = 1.0f;
        if (value != 0) {
            if (value >= 100) {
                value = 99;
            }
            effect->value = value;
            effect->velocity[1] = 0.0053333333f;
        } else {
            effect->code = 0;
        }
        break;
    case 16:
        effect->timer = 33;
        effect->stack = stack;
        effect->position[2] = 0.1f;
        effect->position[0] = 0.0f;
        effect->position[1] = 0.0f;
        effect->scale[3] = 1.0f;
        effect->velocity[3] = 1.0f;
        effect->position[3] = 1.0f;
        if (value != 0) {
            if (value >= 100) {
                value = 99;
            }
            effect->value = value;
            effect->velocity[1] = 0.0053333333f;
        } else {
            effect->code = 0;
        }
        break;
    case 12:
        effect->stack = stack;
        effect->code = 12;
        effect->position[2] = 0.1f;
        effect->phase = 0;
        effect->position[0] = 0.0f;
        effect->position[1] = 0.0f;
        effect->scale[3] = 1.0f;
        effect->velocity[3] = 1.0f;
        effect->position[3] = 1.0f;
        if (value >= 0) {
            if (value == 0) {
                effect->code = 0;
            } else {
                effect->value = value;
                effect->velocity[1] = 0.0053333333f;
            }
        } else {
            effect->value = -value;
            effect->velocity[1] = -0.0033333334f;
        }
        break;
    case 13:
        effect->stack = stack;
        effect->phase = 1;
        effect->code = 12;
        effect->position[2] = 0.1f;
        effect->position[0] = 0.0f;
        effect->position[1] = 0.0f;
        effect->scale[3] = 1.0f;
        effect->velocity[3] = 1.0f;
        effect->position[3] = 1.0f;
        if (value >= 0) {
            if (value == 0) {
                effect->code = 0;
            } else {
                effect->value = value;
                effect->velocity[1] = 0.0053333333f;
            }
        } else {
            effect->value = -value;
            effect->velocity[1] = -0.0033333334f;
        }
        break;
    case 14:
        effect->stack = stack;
        effect->position[2] = 0.1f;
        effect->position[0] = 0.0f;
        effect->position[1] = 0.0f;
        if (value == 0) {
            effect->code = 0;
        } else {
            effect->value = value;
            effect->velocity[1] = 0.0053333333f;
        }
        break;
    case 5:
        if (effect->soundId != 0) {
            xglSoundEffectNormalID(effect->soundId, 0);
        }
        if (position == 0) {
            effect->position[1] = 1.81f;
            effect->position[2] = 0.15f;
            effect->position[0] = 0.0f;
        }
        effect->scale[1] = 1.5f;
        effect->scale[0] = 1.5f;
        effect->scale[2] = 1.0f;
        effect->scale[3] = 1.0f;
        effect->velocity[3] = 1.0f;
        effect->position[3] = 1.0f;
        break;
    case 25:
        if (effect->soundId != 0) {
            xglSoundEffectNormalID(effect->soundId, 0);
        }
        if (position == 0) {
            effect->position[1] = 1.81f;
            effect->position[2] = 0.15f;
            effect->position[0] = 0.0f;
        }
        effect->position[0] -= 0.125f;
        effect->position[1] -= 0.125f;
        effect->scale[1] = 1.5f;
        effect->scale[0] = 1.5f;
        effect->scale[2] = 1.0f;
        effect->scale[3] = 1.0f;
        effect->velocity[3] = 1.0f;
        effect->position[3] = 1.0f;
        break;
    default:
        if (effect->soundId != 0) {
            xglSoundEffectNormalID(effect->soundId, 0);
        }
        if (stack != 0) {
            if (code == 23) {
                effect->code = 3;
                effect->velocity[2] = 0.006666667f;
                effect->velocity[0] = -effect->position[0] / 15.0f;
                effect->velocity[1] = (0.7885f - effect->position[1]) / 15.0f;
            } else if (code == 33) {
                effect->velocity[2] = 0.006666667f;
                effect->velocity[0] = -effect->position[0] / 15.0f;
                effect->velocity[1] = (0.7885f - effect->position[1]) / 15.0f;
            }
            effect->stack = stack;
            effect->velocity[0] = (-stack->position[0] - position[0]) / 15.0f;
            effect->velocity[1] = (-stack->position[1] - position[1]) / 15.0f;
        } else {
            switch (code) {
            case 2:
            case 4:
            case 22:
            case 24:
            case 32:
            case 34:
                effect->velocity[2] = 0.006666667f;
                effect->velocity[0] = -effect->position[0] / 15.0f;
                effect->velocity[1] = (1.81f - effect->position[1]) / 15.0f;
                break;
            case 23:
                effect->code = 3;
                /* fallthrough */
            case 33:
                effect->velocity[2] = 0.006666667f;
                effect->velocity[0] = -effect->position[0] / 15.0f;
                effect->velocity[1] = (0.7885f - effect->position[1]) / 15.0f;
                break;
            }
        }
        effect->scale[1] = 1.5f;
        effect->scale[0] = 1.5f;
        effect->scale[2] = 1.0f;
        effect->scale[3] = 1.0f;
        effect->velocity[3] = 1.0f;
        effect->position[3] = 1.0f;
        break;
    }
    work->effectsSet++;
}

/*
 * Resets an effect's transform matrix to identity while preserving its
 * fourth row: CardDispEffect and CardSetEffect keep four color or scale
 * values there, aliased with the row xglMatrixUnit would otherwise clear.
 */
void CDEMatrixSub(float matrix[4][4])
{
    float row3[4];

    row3[0] = matrix[3][0];
    row3[1] = matrix[3][1];
    row3[2] = matrix[3][2];
    row3[3] = matrix[3][3];
    xglMatrixUnit(matrix);
    matrix[3][0] = row3[0];
    matrix[3][1] = row3[1];
    matrix[3][2] = row3[2];
    matrix[3][3] = row3[3];
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct CpuDeckRecord {
    char name[0x20];
    char gene[0x10];
    char deck[0x28];
    char data[0x28];
} CpuDeckRecord;

INCLUDE_ASM("asm/nonmatchings/ov10/cgp", CardDispEffect);

typedef struct {
    s16 id;
    s16 unmodeled_02;
} CardTeMaxItem;
typedef struct {
    s16 state;
    u8 kind;
    u8 maxSelect;
    u8 unmodeled_04;
    u8 selected;
    s16 x;
    s16 y;
    s16 lines;
    s16 cursor;
    u8 unmodeled_0E[2];
    s16 messId;
    s16 count;
    CardTeMaxItem items[40];
} CardTeMaxList;

int CGPCheckTeMax(CardTeMaxList *list, CardHand *hand)
{
    int n;
    int i;

    n = CardPlayHandCnt(hand);
    if (n >= 7) {
    list->kind = 7;
    list->maxSelect = 0xF;
    list->x = 0x150;
    list->y = 0x30;
    list->lines = 0xE;
    list->messId = 0xA;
    list->state = 0;
    list->selected = 0;
    list->cursor = 0;
    list->count = n;
    for (i = 0; i < n; i++) {
        list->items[i].id = hand->cards[i];
    }
    return -1;
    }
    return 0;
}

void CGPCommFinishSub(CGPCardDefSource *src, CardPlayHand *work, s32 index)
{
    s32 cost;
    s32 j;
    s32 junkCnt;

    if (index >= 0) {
        work->lastCardValue = work->cards[index];
        work->cards[index] = -1;
        CardPlayCleanHand(work);
    }

    cost = src->defs[work->lastCardValue].cost;
    for (j = 0; j < cost; j++) {
        CardPlayCostCard(work);
    }

    junkCnt = CardPlayJunkCnt(work);
    work->junkPile[junkCnt] = work->lastCardValue;
    work->lastCardValue = -1;
}

extern void CardCursorPassive(void *cursor);
extern int CardFread(unsigned int mode, char *filename);
extern char D_00A4E758[], D_00A4E768[], D_00A4E778[], D_00A4E788[], D_00A4E798[], D_00A4E7A8[];
extern char D_00A4E7B8[], D_00A4E7D0[], D_00A4E7E0[], D_00A4E7F0[], D_00A4E800[], D_00A4E810[];
extern char D_00A4E820[], D_00A4E830[], D_00A4E840[], D_00A4E850[], D_00A4E860[], D_00A4E870[];
extern char D_00A4E880[], D_00A4E890[], D_00A4E8A0[], D_00A4E8B0[], D_00A4E8C0[], D_00A4E8D8[];
extern char D_00A4E8F0[], D_00A4E900[], D_00A4E910[], D_00A4E920[], D_00A4E930[], D_00A4E940[];
extern char D_00A4E958[], D_00A4E970[], D_00A4E980[], D_00A4E990[], D_00A4E9A0[], D_00A4E9B0[];
extern char D_00A4E9C0[], D_00A4E9D0[], D_00A4E9E0[], D_00A4E9F0[], D_00A4EA00[], D_00A4EA10[];
extern char D_00A4EA20[], D_00A4EA30[], D_00A4EA40[], D_00A4EA50[], D_00A4EA68[], D_00A4EA80[];
extern char D_00A4EA98[], D_00A4EAB0[], D_00A4EAC8[], D_00A4EAE0[], D_00A4EAF8[], D_00A4EB10[];
extern char D_00A4EB20[], D_00A4EB30[], D_00A4EB40[], D_00A4EB58[], D_00A4EB70[], D_00A4EB80[];
extern char D_00A4EB98[], D_00A4EBB0[], D_00A4EBC8[], D_00A4EBE0[], D_00A4EBF8[], D_00A4EC10[];
extern char D_00A4EC28[], D_00A4EC40[], D_00A4EC58[], D_00A4EC70[], D_00A4EC88[], D_00A4ECA0[];
extern char D_00A4ECB8[], D_00A4ECD0[], D_00A4ECE8[], D_00A4ED00[], D_00A4ED18[], D_00A4ED28[];
extern char D_00A4ED38[], D_00A4ED48[], D_00A4ED58[], D_00A4ED68[], D_00A4ED78[], D_00A4ED90[];

void CardGameInit(CardGameWork *arg0)
{
  s32 i;
  float new_var;
  arg0->flags = 0;
  if (arg0->command.input[0].held & 0x10)
  {
    arg0->flags = 0x80;
  }
  *((unsigned short *) (((char *) arg0) + 0x5AE0)) = 1;
  *((unsigned short *) (((char *) arg0) + 0x5AE2)) = 0x2D;
  arg0->command.turn = 0;
  arg0->command.phase = 0;
  arg0->command.phaseTimer = 0;
  arg0->command.step = 0;
  arg0->command.flowRequest = 0;
  arg0->helpTopic = 0x21;
  arg0->errorMessValue = -1;
  (&arg0->command)->unmodeled_0A = 0;
  arg0->command.recoveryApplied = 0;
  arg0->command.commandIndex = 0;
  *((unsigned short *) (((char *) arg0) + 0x5AE4)) = 0;
  arg0->unmodeled_59B8[1] = 0;
  *((unsigned short *) (((char *) arg0) + 0x5ACE)) = 0;
  arg0->errorMessCode = 0;
  arg0->errorMessKind = 0;
  arg0->infoPanelPhase = 0;
  arg0->resultWait = 0;
  arg0->errorMessReason = 0;
  arg0->mess = 0;
  new_var = -0.9f;
  arg0->interruptMess = 0;
  arg0->permanentMess = 0;
  arg0->cursorMoveStep[0] = 0;
  arg0->cursorMoveStep[1] = 0;
  *((unsigned short *) (((char *) arg0) + 0x5BE0)) = 0;
  arg0->effectsSet = 0;
  {
    char *e = (char *) arg0->effects;
    i = 0x3F;
    do
    {
      i -= 1;
      e[0] = 0;
      e[1] = 0;
      e[2] = 0;
      e[3] = 0;
      e += 0x40;
    }
    while (i >= 0);
  }
  {
    s16 neg = -1;
    s16 *q = (s16 *) (((char *) arg0) + 0x31E);
    s16 *p = (s16 *) (((char *) arg0) + 0x31C);
    i = 0x27;
    do
    {
      i -= 1;
      p[-160] = neg;
      q[-160] = 0;
      p[0] = neg;
      p += 4;
      q[0] = 0;
      q += 4;
    }
    while (i >= 0);
  }
  CardCursorPassive(&arg0->cursor);
  CGPSetMessage(arg0, 0);
  arg0->command.slideDirection = 0;
  *((float *) (((char *) arg0) + 0x70F8)) = -20.0f;
  *((float *) (((char *) arg0) + 0x70F4)) = -0.9f;
  for (i = 15; i >= 0; i--)
  {
    arg0->models[i].visible = 0;
  }

  arg0->models[4].visible = 1;
  arg0->models[5].visible = 1;
  arg0->models[6].visible = 1;
  arg0->models[7].visible = 1;
  arg0->models[0xF].visible = 1;
  arg0->rotStageCooldown = 0;
  arg0->unmodeled_70F0 = 0;
  arg0->rotStageId = 0;
  arg0->unmodeled_70F2[0] = 1;
  *((int *) (((char *) arg0) + 0x70FC)) = 0;
  *((float *) (((char *) arg0) + 0x70F8)) = -20.0f;
  *((float *) (((char *) arg0) + 0x70F4)) = new_var;
  CardFread(0x01900000, D_00A4E758);
  CardFread(0x01900400, D_00A4E768);
  CardFread(0x01900800, D_00A4E778);
  CardFread(0x01900C00, D_00A4E788);
  CardFread(0x01901000, D_00A4E798);
  CardFread(0x01901400, D_00A4E7A8);
  CardFread(0x01901800, D_00A4E7B8);
  CardFread(0x01902800, D_00A4E7D0);
  CardFread(0x01903800, D_00A4E7E0);
  CardFread(0x01904800, D_00A4E7F0);
  CardFread(0x01905800, D_00A4E800);
  CardFread(0x01906800, D_00A4E810);
  CardFread(0x01907800, D_00A4E820);
  CardFread(0x01908800, D_00A4E830);
  CardFread(0x01909800, D_00A4E840);
  CardFread(0x0190A800, D_00A4E850);
  CardFread(0x0190B800, D_00A4E860);
  CardFread(0x0190C800, D_00A4E870);
  CardFread(0x0190D800, D_00A4E880);
  CardFread(0x0190E800, D_00A4E890);
  CardFread(0x0190F800, D_00A4E8A0);
  CardFread(0x01910800, D_00A4E8B0);
  CardFread(0x01911800, D_00A4E8C0);
  CardFread(0x01915800, D_00A4E8D8);
  CardFread(0x01919800, D_00A4E8F0);
  CardFread(0x0191A800, D_00A4E900);
  CardFread(0x0191B800, D_00A4E910);
  CardFread(0x0191C800, D_00A4E920);
  CardFread(0x0191D800, D_00A4E930);
  CardFread(0x0191E800, D_00A4E940);
  CardFread(0x0191F800, D_00A4E958);
  CardFread(0x01920800, D_00A4E970);
  CardFread(0x01921800, D_00A4E980);
  CardFread(0x01922800, D_00A4E990);
  CardFread(0x01923800, D_00A4E9A0);
  CardFread(0x01924800, D_00A4E9B0);
  CardFread(0x01925800, D_00A4E9C0);
  CardFread(0x01926800, D_00A4E9D0);
  CardFread(0x01927800, D_00A4E9E0);
  CardFread(0x01928800, D_00A4E9F0);
  CardFread(0x01929800, D_00A4EA00);
  CardFread(0x0192B000, D_00A4EA10);
  CardFread(0x01939800, D_00A4EA20);
  CardFread(0x01983400, D_00A4EA30);
  CardFread(0x01985400, D_00A4EA40);
  CardFread(0x01996000, D_00A4EA50);
  CardFread(0x0199A000, D_00A4EA68);
  CardFread(0x01930000, D_00A4EA80);
  CardFread(0x01931800, D_00A4EA98);
  CardFread(0x01933000, D_00A4EAB0);
  CardFread(0x01934800, D_00A4EAC8);
  CardFread(0x01936000, D_00A4EAE0);
  CardFread(0x01937800, D_00A4EAF8);
  CardFread(0x01940000, D_00A4EB10);
  CardFread(0x01944000, D_00A4EB20);
  CardFread(0x01949000, D_00A4EB30);
  CardFread(0x019A8000, D_00A4EB40);
  CardFread(0x019AC000, D_00A4EB58);
  CardFread(0x019B0000, D_00A4EB70);
  CardFread(0x01950000, D_00A4EB80);
  CardFread(0x01950510, D_00A4EB98);
  CardFread(0x01950A20, D_00A4EBB0);
  CardFread(0x01950F30, D_00A4EBC8);
  CardFread(0x01951440, D_00A4EBE0);
  CardFread(0x01951950, D_00A4EBF8);
  CardFread(0x01951E60, D_00A4EC10);
  CardFread(0x01952370, D_00A4EC28);
  CardFread(0x01952880, D_00A4EC40);
  CardFread(0x01952D90, D_00A4EC58);
  CardFread(0x01960000, D_00A4EC70);
  CardFread(0x01963680, D_00A4EC88);
  CardFread(0x01966D00, D_00A4ECA0);
  CardFread(0x01A00000, D_00A4ECB8);
  CardFread(0x01A00800, D_00A4ECD0);
  CardFread(0x01A01000, D_00A4ECE8);
  CardFread(0x01A14000, D_00A4ED00);
  CardFread(0x01A28000, D_00A4ED18);
  CardFread(0x01A40000, D_00A4ED28);
  CardFread(0x01A67000, D_00A4ED38);
  CardFread(0x01A85000, D_00A4ED48);
  CardFread(0x01AA0000, D_00A4ED58);
  CardFread(0x01BD8000, D_00A4ED68);
  CardFread(0x01BF0000, D_00A4ED78);
  CardFread(0x01C04000, D_00A4ED90);
}

extern u8 HelpPageList[];
extern void CardCursorPassive(void *cursor);

void CGPNextTurnSub(CardGameWork *work)
{
    work->infoPanelPhase = 0;
    work->command.phase++;
    if ((s16)work->command.phase >= 14) {
        work->command.phase = 0;
        work->command.turn++;
        xglSoundEffectNormalID(0x20001, 0);
    } else if ((s16)work->command.phase == 7) {
        xglSoundEffectNormalID(0x20001, 0);
    } else {
        xglSoundEffectNormalID(6, 0);
    }
    *(u16 *)&work->command.unmodeled_20[2] = 45;
    work->command.phaseTimer = 0;
    *(u16 *)&work->command.unmodeled_20[0] = work->command.phase + 1;
    work->command.step = 0;
    work->unmodeled_59B8[1] = 0;
    *(u16 *)work->command.unmodeled_0E = 0;
    CardCursorPassive(&work->cursor);
    work->cursor.controllingSide = -1;
    work->cursorMoveStep[0] = 0;
    work->cursorMoveStep[1] = 0;
    CGPSetMessage(work, (s16)work->command.phase);
    CGPSetInterruptMess(work, (s16)work->command.phase + 1);
    work->helpTopic = HelpPageList[(s16)work->command.phase];
    {
        float b = -20.0f;
        float a = -0.9f;
        *(float *)&work->unmodeled_70F4[0] = a;
        *(float *)&work->unmodeled_70F4[4] = b;
    }
    work->command.slideDirection = 0;
}

/*
 * Swap a disposal slot's stack with a battle slot's and start the ten-frame
 * slide that carries each of them to the other's position.
 */
void CGPMoveDSBATTLEwork(CardMoveWork *disposal, CardMoveWork *battle)
{
    CardMoveWork moved;
    s32 i;

    if (disposal->moveStep != 0) {
        float steps = disposal->moveStep;

        disposal->position[0] += disposal->velocity[0] * steps;
        disposal->position[1] += disposal->velocity[1] * steps;
        disposal->position[2] += disposal->velocity[2] * steps;
    }
    if (battle->moveStep != 0) {
        float steps = battle->moveStep;

        battle->position[0] += battle->velocity[0] * steps;
        battle->position[1] += battle->velocity[1] * steps;
        battle->position[2] += battle->velocity[2] * steps;
    }

    moved.flags = disposal->flags;
    moved.power = disposal->power;
    moved.powerLimit = disposal->powerLimit;
    moved.position[0] = disposal->position[0];
    moved.position[1] = disposal->position[1];
    moved.position[2] = disposal->position[2];
    moved.shuntPosition[0] = disposal->shuntPosition[0];
    moved.shuntPosition[1] = disposal->shuntPosition[1];
    moved.shuntPosition[2] = disposal->shuntPosition[2];
    for (i = 0; i < 40; i++) {
        moved.layers[i].state = disposal->layers[i].state;
        moved.layers[i].preservedByte01 = disposal->layers[i].preservedByte01;
        moved.layers[i].boostCount = disposal->layers[i].boostCount;
        moved.layers[i].life = disposal->layers[i].life;
        moved.layers[i].preservedByte04 = disposal->layers[i].preservedByte04;
        moved.layers[i].damageTimer = disposal->layers[i].damageTimer;
        moved.layers[i].cardId = disposal->layers[i].cardId;
    }

    disposal->flags = battle->flags;
    disposal->power = battle->power;
    disposal->powerLimit = battle->powerLimit;
    disposal->moveStep = 10;
    disposal->velocity[0] = (disposal->position[0] - battle->position[0]) / 10.0f;
    disposal->velocity[1] = (disposal->position[1] - battle->position[1]) / 10.0f;
    disposal->velocity[2] = 0.0f;
    disposal->position[0] = battle->position[0];
    disposal->position[1] = battle->position[1];
    disposal->position[2] = 0.12f;
    disposal->shuntPosition[0] = battle->shuntPosition[0];
    disposal->shuntPosition[1] = battle->shuntPosition[1];
    disposal->shuntPosition[2] = battle->shuntPosition[2];
    for (i = 0; i < 40; i++) {
        disposal->layers[i].life = battle->layers[i].life;
        disposal->layers[i].cardId = battle->layers[i].cardId;
    }

    battle->flags = moved.flags;
    battle->power = moved.power;
    battle->powerLimit = moved.powerLimit;
    battle->moveStep = 10;
    battle->velocity[0] = (battle->position[0] - moved.position[0]) / 10.0f;
    battle->velocity[1] = (battle->position[1] - moved.position[1]) / 10.0f;
    battle->velocity[2] = 0.0f;
    battle->position[0] = moved.position[0];
    battle->position[1] = moved.position[1];
    battle->position[2] = 0.12f;
    for (i = 0; i < 40; i++) {
        battle->layers[i].state = moved.layers[i].state;
        battle->layers[i].preservedByte01 = moved.layers[i].preservedByte01;
        battle->layers[i].boostCount = moved.layers[i].boostCount;
        battle->layers[i].life = moved.layers[i].life;
        battle->layers[i].preservedByte04 = moved.layers[i].preservedByte04;
        battle->layers[i].damageTimer = moved.layers[i].damageTimer;
        battle->layers[i].cardId = moved.layers[i].cardId;
        battle->shuntPosition[0] = moved.shuntPosition[0];
        battle->shuntPosition[1] = moved.shuntPosition[1];
        battle->shuntPosition[2] = moved.shuntPosition[2];
    }
}

void CGPequipSub(CardGameWork *work, CardPlayHand *hand, CardLayerStack *stack, s32 index)
{
    s32 i;
    s32 n;

    if (stack->layers[1].cardId >= 0) {
        n = CardPlayJunkCnt(hand);
        stack->power = work->definitions[stack->layers[0].cardId].basePower;
        stack->powerLimit = work->definitions[stack->layers[0].cardId].basePower;
        hand->junkPile[n] = stack->layers[1].cardId;
    }
    stack->layers[1].life = work->definitions[hand->cards[index]].operationDuration;
    if (hand->cards[index] != 0x2C && hand->cards[index] != 0x2F && hand->cards[index] != 0x35) {
        stack->power = work->definitions[hand->cards[index]].basePower;
        stack->powerLimit = work->definitions[hand->cards[index]].basePower;
    }
    stack->layers[1].state = 0;
    stack->layers[1].unmodeled_01 = 0;
    stack->layers[1].boostCount = 0;
    stack->layers[1].cardId = hand->cards[index];
    for (i = 0; i < work->definitions[hand->cards[index]].reserveCost; i++) {
        CardPlayCostCard(hand);
    }
    hand->unmodeled_00[0] -= work->definitions[hand->cards[index]].playCost;
    hand->cards[index] = -1;
    CardPlayCleanHand(hand);
}

/*
 * Whether one equipment card may be attached to one weapon card. Both cards
 * must carry kind bit 0. A weapon whose flags are 32 takes an equipment card
 * with flag 8, except cards 4, 80 and 65, and card 11 only on weapon 52; a
 * weapon whose flags are 64 takes an equipment card with flag 1 or flag 2,
 * except cards 2, 9 and 14. Every other weapon takes none.
 */
s32 CGPWeaponEquipChk(CardGameWork *work, s32 weaponId, s32 equipId)
{
    u8 weaponKind;
    u8 equipKind;
    u16 weaponFlags;
    u16 equipFlags;
    s32 allowed;

    weaponKind = work->definitions[weaponId].kind;
    equipKind = work->definitions[equipId].kind;
    weaponFlags = work->definitions[weaponId].flags;
    equipFlags = work->definitions[equipId].flags;
    allowed = 0;
    if ((weaponKind & 1) && (equipKind & 1)) {
        switch (weaponFlags) {
        case 32:
            if ((equipFlags & 8) && equipId != 4 && equipId != 80 &&
                equipId != 65)
            {
                if (equipId != 11) {
                    allowed = 1;
                } else if (weaponId == 52) {
                    allowed = 1;
                }
            }
            break;
        case 64:
            if (((equipFlags & 1) || (equipFlags & 2)) &&
                equipId != 2 && equipId != 9 && equipId != 14)
            {
                allowed = 1;
            }
            break;
        }
    }
    return allowed;
}

void CGPDisposeWeaponSub(s32 side, CardGameWork *work, CGPCursorPosition weapon, CGPCursorPosition target)
{
    s32 sel5;
    s32 sel6;
    CardPlaySide *hand;
    CardLayerStack *stack;
    s32 e;
    s32 ban;
    u16 eflags;
    CardDefinition *defs;

    sel5 = 5;
    sel6 = 6;
    hand = &work->player;
    if (side != 0) {
        hand = &work->enemy;
        sel6 = 2;
        sel5 = 4;
    }

    if (sel6 == target.zone || sel5 == target.zone) {
        if (target.cardId >= 0) {
            if (sel6 == target.zone) {
                stack = &hand->disposal[target.index].battle;
            } else {
                stack = &hand->battle[target.index].battle;
            }

            if (weapon.cardId == 0x34) {
                if (target.cardId == 0xB) {
                    goto success;
                }
                goto fail_mid;
            }

            defs = work->definitions;
            if ((defs[weapon.cardId].flags & 0x20) != 0) {
                e = target.cardId;
                if ((defs[e].flags & 8) != 0) {
                    if (e == 4 || e == 0xB || e == 0x50) {
                        goto fail_inner;
                    }
                    ban = 0x41;
                    goto check_ban;
                }
            }

            if ((defs[weapon.cardId].flags & 0x40) != 0) {
                e = target.cardId;
                eflags = defs[e].flags;
                if (((eflags & 1) & 0xFFFF) != 0 || (eflags & 2) != 0) {
                    if (e == 2 || e == 9) {
                        goto fail_inner;
                    }
                    ban = 0xE;
check_ban:
                    if (e != ban) {
success:
                        CGPequipSub(work, (CardPlayHand *)hand, stack, weapon.index);
                        xglSoundEffectNormalID(1, 0);
                        work->command.phaseTimer = 1;
                        return;
                    }
                }
            }
fail_inner:
fail_mid:
            xglSoundEffectNormalID(5, 0);
            return;
        }
        goto fail_early;
    }
fail_early:
    xglSoundEffectNormalID(5, 0);
}

/*
 * Board area the cursor currently points at. The ten areas follow the cursor's
 * zone: the enemy hand is zone 0 (area 0 while the cursor is on one of its
 * cards, 1 once it is past them), zones 1 to 4 are the enemy board row (area 2
 * for its last column, 3 for its first, 4 otherwise), zones 5 to 8 the player
 * board row (areas 7, 6 and 5 the same way round) and zone 9 the player hand
 * (area 9 on a card, 8 past them).
 */
s32 CGPCursor2Area(CardGameWork *work)
{
    s32 area;
    s32 count;

    if (work->cursorZone < 5) {
        area = 4;
        if (work->cursorZone == 0) {
            count = CardPlayHandCnt(&work->enemy.hand);
            area = 1;
            if (work->cursorIndex < count) {
                area = 0;
            }
        } else {
            if (work->cursorIndex == 0) {
                area = 3;
            }
            if (work->cursorIndex == 3) {
                area = 2;
            }
        }
    } else {
        area = 5;
        if (work->cursorZone == 9) {
            count = CardPlayHandCnt(&work->player.hand);
            area = 8;
            if (work->cursorIndex < count) {
                area = 9;
            }
        } else {
            if (work->cursorIndex == 0) {
                area = 6;
            }
            if (work->cursorIndex == 3) {
                area = 7;
            }
        }
    }
    return area;
}

void CGPCursor2Pos(CGPCursorPosition *position, CardGameWork *work)
{
    position->cardId = -1;
    position->stackedCard = -1;
    position->zone = CGPCursor2Area(work);
    switch (position->zone) {
    case 0:
        position->index = work->cursorIndex;
        position->cardId = work->enemy.hand.cards[position->index];
        break;
    case 9:
        position->index = work->cursorIndex;
        position->cardId = work->player.hand.cards[position->index];
        break;
    case 2:
        position->index = 4 - work->cursorZone;
        position->cardId = work->enemy.disposal[position->index].battle.layers[0].cardId;
        if (position->cardId >= 0 &&
            work->enemy.disposal[position->index].battle.layers[1].cardId >= 0)
        {
            position->stackedCard = work->enemy.disposal[position->index].battle.layers[1].cardId;
        }
        break;
    case 3:
        position->index = 4 - work->cursorZone;
        position->cardId = work->enemy.operations[position->index];
        break;
    case 4:
        position->index = 5 - work->cursorIndex - work->cursorZone;
        position->cardId = work->enemy.battle[position->index].battle.layers[0].cardId;
        if (position->cardId >= 0 &&
            work->enemy.battle[position->index].battle.layers[1].cardId >= 0)
        {
            position->stackedCard = work->enemy.battle[position->index].battle.layers[1].cardId;
        }
        break;
    case 6:
        position->index = work->cursorZone - 5;
        position->cardId = work->player.disposal[position->index].battle.layers[0].cardId;
        if (position->cardId >= 0 &&
            work->player.disposal[position->index].battle.layers[1].cardId != 0)
        {
            position->stackedCard = work->player.disposal[position->index].battle.layers[1].cardId;
        }
        break;
    case 7:
        position->index = work->cursorZone - 5;
        position->cardId = work->player.operations[position->index];
        break;
    case 5:
        position->index = work->cursorIndex + work->cursorZone - 6;
        position->cardId = work->player.battle[position->index].battle.layers[0].cardId;
        if (position->cardId >= 0 &&
            work->player.battle[position->index].battle.layers[1].cardId != 0)
        {
            position->stackedCard = work->player.battle[position->index].battle.layers[1].cardId;
        }
        break;
    case 8:
        position->index = work->cursorIndex - CardPlayHandCnt(&work->player.hand);
        position->cardId = work->player.junk[position->index];
        break;
    case 1:
        position->index = work->cursorIndex - CardPlayHandCnt(&work->enemy.hand);
        position->cardId = work->enemy.junk[position->index];
        break;
    }
}

extern float HandPos1P[];
extern float HandPos2P[];

extern void CGPCursor2Pos(CGPCursorPosition *position, CardGameWork *work);

/*
 * Clamps the cursor index to 0..3, resolves the cursor's zone/card (
 * CGPCursor2Pos), then for zones 2, 3, 6 and 7 sets the highlight position
 * to 0.175; for zones 4 and 5 sets it to 0.355 and rounds the cursor zone
 * down to the nearest odd value. Returns the resolved card id.
 */
s16 CGPCMSub(CardGameWork *work)
{
    CGPCursorPosition position;

    work->highlightScaleX = work->highlightScaleY = 1.0f;
    if (work->cursorIndex < 0) {
        work->cursorIndex = 3;
    }
    if (work->cursorIndex >= 4) {
        work->cursorIndex = 0;
    }
    CGPCursor2Pos(&position, work);
    switch (position.zone) {
    case 2:
    case 3:
    case 6:
    case 7:
        work->highlightX = work->highlightY = 0.175f;
        break;
    case 4:
    case 5:
        work->cursorZone = ((work->cursorZone - 1) & 0xFE) + 1;
        work->highlightX = work->highlightY = 0.355f;
        break;
    }
    return position.cardId;
}

typedef struct CGPCursorState {
    u8 moveStep[2];
    u8 junkSel[2];
    s16 index;
    s16 zone;
} __attribute__((aligned(8))) CGPCursorState;
extern float JunkPos1P[];
extern float JunkPos2P[];
extern void CGCPJunkDispSub(CardPlaySide *side, s32 index);
extern void CardPlayCleanJunk(CardHand *hand);
static inline void CursorTablePos(CardGameWork *work)
{
    float *base;

    CGPCMSub(work);
    base = ((float **)HelpPageList)[work->cursorZone * 4 + work->cursorIndex];
    work->cursorPos[0] = base[0];
    work->cursorPos[1] = base[1];
    work->cursorPos[2] = base[2];
}
static inline float CursorStep(float *base, s32 num)
{
    return (base[0] - base[9]) / (float)(num - 1);
}

s32 CGPCursorMove2(s32 flag, CardGameWork *work)
{
    CGPCursorPosition pos;
    float matrix[4][4];
    float trans[4];
    float scale[4];
    CardPlaySide *side;
    CardPlaySide *player;
    s32 disp;
    u16 held;
    u16 pressed;
    u16 repeat;
    s32 n;
    s32 n2;
    CardPlaySide *enemy;
    u8 step;
    s16 oldZone;
    s32 nearSide;
    s32 cnt;
    s32 rot;
    s16 zone;
    s32 hand;
    s32 junk;
    s32 hand2;
    float q;
    float step2;
    s32 layer;

    side = &work->player;
    disp = 1;
    player = side;
    held = work->command.input[0].held;
    pressed = work->command.input[0].pressed;
    repeat = work->command.input[0].repeat;
    if (flag != 0) {
        held = work->command.input[1].held;
        side = &work->enemy;
        pressed = work->command.input[1].pressed;
        repeat = work->command.input[1].repeat;
    }

    if (work->cursorMoveStep[0] != 0) {
        n = CardPlayJunkCnt((CardPlayHand *)player);
        step = work->cursorMoveStep[0];
        work->cursorPos[0] = JunkPos1P[0];
        work->cursorPos[1] = JunkPos1P[1];
        work->cursorPos[2] = JunkPos1P[2];
        switch (step) {
        case 1:
            if (pressed & 0x1000) {
                work->cursorMoveStep[0] = 0;
                work->cursorZone--;
                xglSoundEffectNormalID(3, 0);
            } else if (pressed & 0x20) {
                work->unmodeled_5B5A[0] = 0;
                work->cursorMoveStep[0] = 2;
                xglSoundEffectNormalID(1, 0);
                CardPlayCleanJunk(&player->hand);
            }
            pos.stackedCard = -1;
            pos.cardId = work->player.junk[n - 1];
            break;
        case 2:
            if (repeat & 0x8000) {
                if (work->unmodeled_5B5A[0] != 0) {
                    work->unmodeled_5B5A[0]--;
                    xglSoundEffectNormalID(3, 0);
                }
            } else if (repeat & 0x2000) {
                if (work->unmodeled_5B5A[0] < n - 1) {
                    work->unmodeled_5B5A[0]++;
                    xglSoundEffectNormalID(3, 0);
                }
            }
            if (pressed & 0x40) {
                work->cursorMoveStep[0] = 1;
                xglSoundEffectNormalID(2, 0);
            }
            CGCPJunkDispSub(player, work->unmodeled_5B5A[0]);
            pos.cardId = work->player.junk[work->unmodeled_5B5A[0]];
            pos.stackedCard = -1;
            break;
        }
    } else if (work->cursorMoveStep[1] != 0) {
        enemy = &work->enemy;
        n2 = CardPlayJunkCnt((CardPlayHand *)enemy);
        step = work->cursorMoveStep[1];
        work->cursorPos[0] = JunkPos2P[0];
        work->cursorPos[1] = JunkPos2P[1];
        work->cursorPos[2] = JunkPos2P[2];
        switch (step) {
        case 1:
            if (pressed & 0x4000) {
                work->cursorMoveStep[1] = 0;
                work->cursorZone++;
                xglSoundEffectNormalID(3, 0);
            } else if (pressed & 0x20) {
                work->unmodeled_5B5A[1] = 0;
                work->cursorMoveStep[1] = 2;
                xglSoundEffectNormalID(1, 0);
                CardPlayCleanJunk(&enemy->hand);
            }
            pos.stackedCard = -1;
            pos.cardId = work->enemy.junk[n2 - 1];
            break;
        case 2:
            if (repeat & 0x8000) {
                if (work->unmodeled_5B5A[1] != 0) {
                    work->unmodeled_5B5A[1]--;
                    xglSoundEffectNormalID(3, 0);
                }
            } else if (repeat & 0x2000) {
                if (work->unmodeled_5B5A[1] < n2 - 1) {
                    work->unmodeled_5B5A[1]++;
                    xglSoundEffectNormalID(3, 0);
                }
            }
            if (pressed & 0x40) {
                work->cursorMoveStep[1] = 1;
                xglSoundEffectNormalID(2, 0);
            }
            CGCPJunkDispSub(enemy, work->unmodeled_5B5A[1]);
            pos.cardId = work->enemy.junk[work->unmodeled_5B5A[1]];
            pos.stackedCard = -1;
            break;
        }
    } else {
        if (pressed & 0xF000) {
            xglSoundEffectNormalID(3, 0);
        }
        oldZone = work->cursorZone;
        nearSide = (u32)((s16)CGPCursor2Area(work) - 4) < 2;
        if (pressed & 0x1000) {
            work->cursorZone--;
            if (nearSide && work->cursorZone != 0) {
                work->cursorZone--;
            }
        }
        if (pressed & 0x4000) {
            work->cursorZone++;
            if (nearSide && work->cursorZone != 9) {
                work->cursorZone++;
            }
        }
        if (work->cursorZone < 0) {
            work->cursorZone = 9;
        }
        if (work->cursorZone >= 10) {
            work->cursorZone = 0;
        }
        cnt = 0;
        if (oldZone != 9 && ((CGPCursorState *)work->cursorMoveStep)->index == 3 && ((CGPCursorState *)work->cursorMoveStep)->zone == 9) {
            s32 c = CardPlayJunkCnt((CardPlayHand *)player);
            if (c != 0) {
                cnt = 1;
                work->cursorMoveStep[0] = 1;
                pos.cardId = work->player.junk[c - 1];
            }
        }
        if (oldZone != 0 && ((CGPCursorState *)work->cursorMoveStep)->index == 0 && ((CGPCursorState *)work->cursorMoveStep)->zone == 0) {
            s32 c = CardPlayJunkCnt((CardPlayHand *)&work->enemy);
            if (c != 0) {
                work->cursorMoveStep[1] = 1;
                cnt++;
                pos.cardId = work->enemy.junk[c - 1];
            }
        }
        if (cnt == 0) {
            CGPCursor2Area(work);
            rot = 0;
            switch ((s16)work->command.phase) {
            case 1:
            case 2:
            case 3:
            case 11:
                if (work->cursorZone == 9) {
                    if (work->rotStageId != 0) {
                        rot = 1;
                    }
                }
                break;
            case 4:
            case 8:
            case 9:
            case 10:
                if (work->cursorZone == 0) {
                    rot = work->rotStageId != 0;
                }
                break;
            }
            if (rot) {
                if (pressed & 0x2000) {
                    work->cursorIndex--;
                }
                if (pressed & 0x8000) {
                    work->cursorIndex++;
                }
            } else {
                if (pressed & 0x2000) {
                    work->cursorIndex++;
                }
                if (pressed & 0x8000) {
                    work->cursorIndex--;
                }
            }
            zone = work->cursorZone;
            if (zone == 0) {
                hand = CardPlayHandCnt(&work->enemy.hand);
                junk = CardPlayJunkCnt((CardPlayHand *)&work->enemy);
                work->highlightY = 0.175f;
                work->highlightScaleY = 1.0f;
                work->highlightX = 0.175f;
                work->highlightScaleX = 1.0f;
                if (hand == 0 && junk == 0) {
                    work->cursorZone = 1;
                    CursorTablePos(work);
                    goto done;
                }
                if (work->cursorIndex < 0) {
                    work->cursorIndex = hand - 1;
                }
                if (work->cursorIndex >= hand) {
                    work->cursorIndex = 0;
                }
                step2 = CursorStep(HandPos2P, hand);
                work->cursorPos[0] = HandPos2P[0] - step2 * (float)(hand - work->cursorIndex - 1);
                work->cursorPos[1] = HandPos2P[1];
                work->cursorPos[2] = HandPos2P[2];
            } else if (zone == 9) {
                hand2 = CardPlayHandCnt(&player->hand);
                junk = CardPlayJunkCnt((CardPlayHand *)player);
                work->highlightY = 0.175f;
                work->highlightScaleY = 1.0f;
                work->highlightX = 0.175f;
                work->highlightScaleX = 1.0f;
                if (hand2 == 0 && junk == 0) {
                    work->cursorZone = 1;
                    CursorTablePos(work);
                    goto done;
                }
                if (oldZone != work->cursorZone) {
                    if (work->cursorIndex < 3) {
                        work->cursorIndex = 0;
                    } else {
                        work->cursorIndex = hand2;
                    }
                }
                if (work->cursorIndex < 0) {
                    work->cursorIndex = hand2 - 1;
                }
                if (work->cursorIndex >= hand2) {
                    work->cursorIndex = 0;
                }
                if (hand2 != 0 && work->cursorIndex < hand2) {
                    q = CursorStep(HandPos1P, hand2);
                    work->cursorPos[0] = HandPos1P[0] - q * (float)work->cursorIndex;
                    work->cursorPos[1] = HandPos1P[1];
                    work->cursorPos[2] = HandPos1P[2];
                } else {
                    q = CursorStep(JunkPos1P, 0);
                    work->cursorPos[0] = JunkPos1P[0] - q * (float)(work->cursorIndex - hand2);
                    work->cursorPos[1] = JunkPos1P[1];
                    work->cursorPos[2] = JunkPos1P[2];
                }
            } else {
                CursorTablePos(work);
            }
        }
    done:
        if (work->cursorMoveStep[0] == 0 && work->cursorMoveStep[1] == 0) {
            CGPCursor2Pos(&pos, work);
        }
        switch ((s16)work->command.phase) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 11:
            if (pos.zone == 0) {
                disp = 0;
            }
            break;
        default:
            disp = pos.zone != 9;
            break;
        }
    }

    if ((held & 0x10) && disp && pos.cardId >= 0) {
    pos.cardId &= 0xF0FF;
    layer = 5;
    trans[0] = 0.0f;
    trans[1] = 0.369f;
    trans[2] = 0.5f;
    trans[3] = 1.0f;
    scale[0] = scale[1] = 1.1f;
    scale[2] = scale[3] = 1.0f;
    if ((pos.stackedCard >= 0 && (held & 8)) || (*(s16 *)((char *)side + 0x198) >= 0 && (held & 4))) {
        layer = 0;
        trans[1] = 0.096f;
        trans[2] = 0.48f;
        scale[0] = scale[1] = 0.55f;
    }
    xglMatrixUnit(matrix);
    xglMatrixTrans(matrix, matrix, trans);
    xglMatrixScale(matrix, matrix, scale);
    CardDisp(matrix, pos.cardId, 1, layer);
    if (pos.stackedCard >= 0) {
        layer = 0;
        trans[0] = 1.904f;
        trans[1] = 0.096f;
        trans[2] = 0.48f;
        scale[0] = scale[1] = 0.55f;
        scale[2] = scale[3] = 1.0f;
        if (held & 8) {
            layer = 5;
            trans[0] = 0.9f;
            trans[1] = 0.369f;
            trans[2] = 0.5f;
            scale[0] = scale[1] = 1.1f;
        }
        xglMatrixUnit(matrix);
        xglMatrixTrans(matrix, matrix, trans);
        xglMatrixScale(matrix, matrix, scale);
        CardDisp(matrix, pos.stackedCard, 1, layer);
        if (!(held & 8)) {
            trans[0] = 1.904f;
            trans[1] = 0.096f;
            trans[2] = 0.29f;
            xglMatrixUnit(matrix);
            xglMatrixTrans(matrix, matrix, trans);
            nmlModelSetPlace(matrix);
            nmlModelSetTexture((void *)0x1939800);
            nmlModelEntryCard((void *)0x1929800);
        }
    }
    if (*(s16 *)((char *)side + 0x198) >= 0) {
        layer = 0;
        trans[0] = -1.904f;
        trans[1] = 0.096f;
        trans[2] = 0.48f;
        scale[0] = scale[1] = 0.55f;
        scale[2] = scale[3] = 1.0f;
        if (held & 4) {
            layer = 5;
            trans[0] = -0.9f;
            trans[1] = 0.369f;
            trans[2] = 0.5f;
            scale[0] = scale[1] = 1.1f;
        }
        xglMatrixUnit(matrix);
        xglMatrixTrans(matrix, matrix, trans);
        xglMatrixScale(matrix, matrix, scale);
        CardDisp(matrix, *(s16 *)((char *)side + 0x198), 1, layer);
        if (!(held & 4)) {
            trans[0] = -1.904f;
            trans[1] = 0.096f;
            trans[2] = 0.29f;
            xglMatrixUnit(matrix);
            xglMatrixTrans(matrix, matrix, trans);
            nmlModelSetPlace(matrix);
            nmlModelSetTexture((void *)0x1939800);
            nmlModelEntryCard((void *)0x192B000);
        }
    }
    return 1;
    }
    return 0;
}

/* CGPCursorMove2, which does the actual cursor movement, is still
 * assembler-scaffolded in this TU. */
extern s32 CGPCursorMove2(s32 flag, CardGameWork *work);

void CGPCursorMove(s32 flag, CardGameWork *work)
{
    CGPCursorMove2(flag, work);
}

/*
 * Put the cursor on one board area, undoing any movement in progress. The area
 * codes are the ones CGPCursor2Area returns, and slot picks the column inside
 * the area: areas 0 and 1 are the enemy hand, 2 and 3 the last and the first
 * column of the enemy board row, 4 its two middle columns, 5 the two middle
 * columns of the player board row, 6 and 7 its first and last column, and 8
 * and 9 the player hand. The cursor then takes the screen position of the hand
 * it stands in.
 */
void CGPSetCursor(CardGameWork *work, u16 area, u16 slot)
{
    work->cursorMoveStep[0] = 0;
    work->cursorMoveStep[1] = 0;
    switch (area) {
    case 0:
    case 1:
        work->cursorZone = 0;
        work->cursorIndex = 0;
        break;
    case 2:
        work->cursorIndex = 3;
        work->cursorZone = 4 - slot;
        break;
    case 3:
        work->cursorIndex = 0;
        work->cursorZone = 4 - slot;
        break;
    case 4:
        if (slot < 2) {
            work->cursorZone = 3;
            work->cursorIndex = 2 - (slot & 1);
        } else {
            work->cursorZone = 2;
            work->cursorIndex = 2 - (slot & 1);
        }
        break;
    case 5:
        if (slot < 2) {
            work->cursorZone = 5;
            work->cursorIndex = (slot & 1) + 1;
        } else {
            work->cursorZone = 6;
            work->cursorIndex = (slot & 1) + 1;
        }
        break;
    case 6:
        work->cursorIndex = 0;
        work->cursorZone = slot + 5;
        break;
    case 7:
        work->cursorIndex = 3;
        work->cursorZone = slot + 5;
        break;
    case 8:
    case 9:
        work->cursorIndex = 0;
        work->cursorZone = 9;
        break;
    }
    CGPCMSub(work);
    if (work->cursorZone == 9) {
        work->cursorPos[0] = HandPos1P[0];
        work->cursorPos[1] = HandPos1P[1];
        work->cursorPos[2] = HandPos1P[2];
    } else {
        work->cursorPos[0] = HandPos2P[0];
        work->cursorPos[1] = HandPos2P[1];
        work->cursorPos[2] = HandPos2P[2];
    }
}

s32 CGPChkPlayable(s16 command)
{
    s32 i;

    for (i = 0; i < 9; i++) {
        if (command == CGPCPTbl[i]) {
            return 1;
        }
    }
    return 0;
}

s32 CGPChkOpePlayable(s32 side, CardGameWork *work, s16 command)
{
    CardPlaySide *enemy = (CardPlaySide *)&work->enemy;
    CardPlaySide *player;
    s32 found;
    s32 i;
    s16 op;
    s32 r;

    work = (CardGameWork *)&work->player;
    player = (CardPlaySide *)work;
    r = CGPChkPlayable(command);
    if (r == 0) {
        goto fail;
    }
    switch (command) {
    case 0x8B:
        found = 0;
        i = 0;
        if (player->battle[i].battle.layers[0].cardId >= 0) {
            goto hit;
        }
    battle0:
        if (enemy->battle[i].battle.layers[0].cardId >= 0) {
            goto hit;
        }
        i++;
        if (i >= 4) {
            break;
        }
        if (player->battle[i].battle.layers[0].cardId < 0) {
            goto battle0;
        }
        found = 1;
        break;
    case 0x8D:
        op = enemy->operations[0];
        found = 0;
        i = 0;
        goto ops_test;
    ops_next:
        i++;
        if (i >= 4) {
            goto done;
        }
        op = enemy->operations[i];
    ops_test:
        if (op < 0) {
            goto ops_next;
        }
        found = 1;
        break;
    default:
        goto ok;
    case 0x8F:
        found = 0;
        i = 0;
        if (player->disposal[i].battle.layers[0].cardId < 0) {
        disposal:
            if (enemy->disposal[i].battle.layers[0].cardId < 0) {
                i++;
                if (i >= 4) {
                    goto battle;
                }
                if (player->disposal[i].battle.layers[0].cardId < 0) {
                    goto disposal;
                }
            }
        }
        found = 1;
    battle:
        i = 0;
        if (player->battle[i].battle.layers[0].cardId < 0) {
            for (;;) {
                if (enemy->battle[i].battle.layers[0].cardId >= 0) {
                    break;
                }
                i++;
                if (i >= 4) {
                    goto done;
                }
                if (player->battle[i].battle.layers[0].cardId >= 0) {
                    break;
                }
            }
        }
    hit:
        found = 1;
        break;
    }
done:
    if (found) {
        goto ok;
    }
    r = 0;
fail:
    return r;
ok:
    return 1;
}

void CardPlayClearEndflg(CardPlaySide *side)
{
    s32 i;

    for (i = 0; i < 4; i++) {
        side->disposal[i].battle.flags &= ~1;
    }
    for (i = 0; i < 4; i++) {
        side->operationFlags[i] &= ~1;
    }
}

/*
 * Cards a command reserves from the draw pile, zero once the command's own
 * card is already in play on the side that would play it: command 37 is free
 * while card 5 or card 12 stands on the board, commands 4 and 39 while card 0,
 * card 1 or card 7 does.
 */
s32 CGPChkCardPlayCost(CardGameWork *work, CardHand *hand, s32 command)
{
    s32 cost;

    cost = work->definitions[command].reserveCost;
    switch (command) {
    case 4:
    case 39:
        if (CardPlayCntBtl(hand, 0) || CardPlayCntBtl(hand, 1) ||
            CardPlayCntBtl(hand, 7)) {
            cost = 0;
        }
        break;
    case 37:
        if (CardPlayCntBtl(hand, 5) || CardPlayCntBtl(hand, 12)) {
            cost = 0;
        }
        break;
    }
    return cost;
}

void CardPlayDisployment(u16 side, CardGameWork *workAddress, CardPlayField *field, s32 index)
{
    CardGameWork *work = workAddress;
    CardDefinition *defs;
    s32 cost;
    s32 i;
    s32 slot;

    defs = work->definitions;
    cost = 0;
    switch (defs[field->hand.maintenanceView.cards[index]].kind) {
    case 1:
        for (i = 0, slot = -1; i < 4; i++) {
            if (field->disposal[i].stack.layers[0].cardId < 0) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            return;
        }
        CardPlayCommandPlay(side, work->commandIds[field->hand.maintenanceView.cards[index]],
                            workAddress, 0, 0);
        field->disposal[slot].stack.flags = 1;
        field->disposal[slot].stack.power =
            defs[field->hand.maintenanceView.cards[index]].basePower;
        field->disposal[slot].stack.powerLimit =
            defs[field->hand.maintenanceView.cards[index]].basePower;
        field->disposal[slot].stack.layers[0].cardId = field->hand.maintenanceView.cards[index];
        field->disposal[slot].stack.layers[0].life =
            defs[field->hand.maintenanceView.cards[index]].operationDuration;
        cost = CGPChkCardPlayCost(workAddress, &field->hand.costView,
                                  field->hand.maintenanceView.cards[index]);
        field->disposal[slot].stack.position[0] = field->handMove[index].position[0];
        field->disposal[slot].stack.position[1] = field->handMove[index].position[1];
        field->disposal[slot].stack.position[2] = field->handMove[index].position[2];
        field->disposal[slot].stack.step[0] =
            (DisposePos1P[slot][0] - field->handMove[index].position[0]) / 10.0f;
        field->disposal[slot].stack.step[1] =
            (DisposePos1P[slot][1] - field->handMove[index].position[1]) / 10.0f;
        field->disposal[slot].stack.step[2] =
            (DisposePos1P[slot][2] - field->handMove[index].position[2]) / 10.0f;
        field->disposal[slot].stack.moveTimer = 10;
        field->hand.maintenanceView.cards[index] = -1;
        break;
    case 4:
        for (i = 0, slot = -1; i < 4; i++) {
            if (field->operations[i] < 0) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            return;
        }
        field->operationFlags[slot] |= 1;
        field->operations[slot] = field->hand.maintenanceView.cards[index];
        field->operationDuration[slot] =
            defs[field->hand.maintenanceView.cards[index]].operationDuration;
        cost = CGPChkCardPlayCost(workAddress, &field->hand.costView,
                                  field->hand.maintenanceView.cards[index]);
        field->operationMove[slot].position[0] = field->handMove[index].position[0];
        field->operationMove[slot].position[1] = field->handMove[index].position[1];
        field->operationMove[slot].position[2] = field->handMove[index].position[2];
        field->operationMove[slot].step[0] =
            (OperationPos1P[slot][0] - field->handMove[index].position[0]) / 10.0f;
        field->operationMove[slot].step[1] =
            (OperationPos1P[slot][1] - field->handMove[index].position[1]) / 10.0f;
        field->operationMove[slot].step[2] =
            (OperationPos1P[slot][2] - field->handMove[index].position[2]) / 10.0f;
        field->operationMove[slot].moveTimer = 10;
        field->hand.maintenanceView.cards[index] = -1;
        break;
    }

    for (i = 0; i < cost; i++) {
        CardPlayCostCard(&field->hand.maintenanceView);
    }
    CardPlayCleanHand(&field->hand.maintenanceView);
}

extern s32 CardPlayCommandPlay(u16 side, u16 command, CardGameWork *work,
                               s16 operand, s16 target);
extern s32 CardPlayChkCost(CardDefinition *definitions, CardPlaySide *side,
                           s32 cardId, CardGameWork *work);
extern s32 CardPlayChkCondition(CardDefinition *definitions, CardPlaySide *side,
                                s32 cardId, CardGameWork *work);
extern s32 CardPlayCntLight(CardGameWork *work, CardPlaySide *side);
extern s32 CardEquipCntHeavy(CardGameWork *work, CardPlaySide *side, s32 cardId);

/*
 * Why one side may not play the card at the given hand position, or zero when
 * it may. Kind 2 is never playable (11) and kinds 1 and 4 need enough play
 * points (12); a card whose definition carries any of the flags 0x64 also has
 * to pass the equipment checks, where flag 4 has no free slot (15), flag 0x40
 * needs a light slot (13) and every other such card a heavy one (14), and 16
 * means the card is equipment the slot search accepted.
 */
s32 CGPDispoSub0(s32 side, CardGameWork *work, s32 index)
{
    s32 error;
    CardPlaySide *ownSide;
    ownSide = &work->player;
    if (side != 0) {
        ownSide = &work->enemy;
    }
    error = 0;
    if (work->definitions[ownSide->hand.cards[index]].kind == 2) {
        error = 11;
    } else {
        if (work->definitions[ownSide->hand.cards[index]].kind == 1) {
            if (ownSide->hand.playPoints[0] < work->definitions[ownSide->hand.cards[index]].playCost) {
                error = 12;
            }
        } else if (work->definitions[ownSide->hand.cards[index]].kind == 4) {
            if (ownSide->hand.playPoints[1] < work->definitions[ownSide->hand.cards[index]].playCost) {
                error = 12;
            }
        }
        if (error == 0) {
            if (work->definitions[ownSide->hand.cards[index]].flags & 0x64) {
                error = CardPlayChkCost(work->definitions, ownSide, ownSide->hand.cards[index], work);
                if (error == 0) {
                    error = CardPlayChkCondition(work->definitions, ownSide,
                                                 ownSide->hand.cards[index], work);
                    if (error == 0) {
                        error = 16;
                        if (work->definitions[ownSide->hand.cards[index]].flags & 4) {
                            error = 15;
                        } else if (work->definitions[ownSide->hand.cards[index]].flags & 0x40) {
                            if (CardPlayCntLight(work, ownSide) != 0) {
                                error = 13;
                            }
                        } else if (CardEquipCntHeavy(work, ownSide, ownSide->hand.cards[index]) != 0) {
                            error = 14;
                        }
                    }
                }
            } else {
                error = CardPlayChkCost(work->definitions, ownSide, ownSide->hand.cards[index], work);
                if (error == 0) {
                    error = CardPlayChkCondition(work->definitions, ownSide,
                                                 ownSide->hand.cards[index], work);
                }
            }
        }
    }
    return error;
}

s32 CGPDisposeSub(s32 side, CardGameWork *work, s32 index)
{
    s32 error;
    s32 done;
    CardPlaySide *field;
    u16 pressed;
    s32 i;
    CardDefinition *def;

    error = 0;
    pressed = work->command.input[0].pressed;
    field = &work->player;
    if (side != 0) {
        pressed = work->command.input[1].pressed;
        field = &work->enemy;
    }

    if (index < CardPlayHandCnt(&field->hand)) {
        if (pressed & 0x20) {
            done = 0;
            switch (field->hand.cards[index]) {
            case 10:
            case 11:
            case 12:
            case 13:
                error = CGPDispoSub0(side, work, index);
                if (error == 5 || error == 1) {
                    error = 0;
                }
            case 7:
            case 9:
                if (error != 0) {
                    break;
                }
                if (field->hand.playPoints[0] < work->definitions[field->hand.cards[index]].playCost) {
                    error = 12;
                    break;
                }
                i = 0;
                if ((field->hand.cards[index] - 7) != field->disposal[0].battle.layers[0].cardId) {
                    goto loop1;
                }
                error = 9;
                goto loop2f;
err5x:
                error = 5;
                goto loop2f;
loop1:
                if (field->hand.cards[index] != field->disposal[i].battle.layers[0].cardId) {
                    i++;
                    if (i < 4) {
                        if ((field->hand.cards[index] - 7) == field->disposal[i].battle.layers[0].cardId) {
                            error = 9;
                        } else {
                            goto loop1;
                        }
                    }
                } else {
                    goto err5x;
                }

loop2f:
                i = 0;
                if ((field->hand.cards[index] - 7) == field->battle[0].battle.layers[0].cardId) {
                    done = 1;
                    error = 15;
                } else {
loop2:
                    i++;
                    if (i < 4) {
                        if ((field->hand.cards[index] - 7) == field->battle[i].battle.layers[0].cardId) {
                            done = 1;
                            error = 15;
                        } else {
                            goto loop2;
                        }
                    }
                }
                break;
            }

            if (done == 0) {
                if (error != 0 || (error = CGPDispoSub0(side, work, index)) != 0) {
                    if (error >= 0) {
                        if (error < 16) {
                            if (error >= 13) {
                                goto ok;
                            }
                        }
                    }
                    goto fail;
                }
                def = (CardDefinition *)((char *)(field->hand.cards[index] * sizeof(CardDefinition)) + (int)work->definitions);
                if (def->kind == 1) {
                    field->hand.playPoints[0] -= def->playCost;
                } else {
                    field->hand.playPoints[1] -= def->playCost;
                }
                CardPlayDisployment(side, work, (CardPlayField *)field, index);
ok:
                xglSoundEffectNormalID(1, 0);
                goto end2;
fail:
                if (error == 12) {
                    if (((CardDefinition *)(field->hand.cards[index] * sizeof(CardDefinition) + (char *)work->definitions))->kind == 1) {
                        CGPSetErrorMess(side, work, 12);
                    } else {
                        CGPSetErrorMess(side, work, 10);
                    }
                } else {
                    CGPSetErrorMess(side, work, error);
                }
                xglSoundEffectNormalID(5, 0);
                error = 0;
end2:
                ;
            }
        }
    }
    return error;
}

/*
 * Moves the cursor (CGPCursorMove2) and resolves the card at its new
 * position (CGPCursor2Pos), then clears the resolved card id to -1 unless
 * the zone is 2 or 4 (nonzero flag) or 5 or 6 (zero flag). `unused` (the
 * second parameter) is forwarded by every caller but never read here.
 */
s16 CGPMoveSub(s32 flag, void *unused, CardGameWork *work)
{
    CGPCursorPosition position;

    position.index = -1;
    CGPCursorMove2(flag, work);
    CGPCursor2Pos(&position, work);
    if (flag == 0) {
        if (position.zone != 5 && position.zone != 6) {
            position.cardId = -1;
        }
    } else {
        if (position.zone != 4 && position.zone != 2) {
            position.cardId = -1;
        }
    }
    return position.cardId;
}

/*
 * Raise or lower an attack's power before the damage is worked out. Operation
 * cards in play on either side act through the attacking card's flags, and
 * three attacking cards act on their own: 15 counts the cards in play on its
 * own board that carry flag 8, 14 looks for card 79 there and 76 plays command
 * 73. A power that fell below zero counts as none.
 */
s32 CGPBattleBeforeEffect(s32 side, CardGameWork *work, s32 power, CardLayerStack *stack,
                          s32 slot)
{
    CardPlaySide *own;
    CardPlaySide *foe;
    s32 cardId;
    s32 i;

    cardId = stack->layers[0].cardId;
    if (side == 0) {
        own = &work->player;
        foe = &work->enemy;
    } else {
        own = &work->enemy;
        foe = &work->player;
    }

    if (CardPlayCntOpe(own, 120) != 0 || CardPlayCntOpe(foe, 120) != 0) {
        if (work->definitions[cardId].flags & 4) {
            power--;
        }
    }

    i = CardPlayCntOpe(own, 123);
    if (i != 0) {
        if (work->definitions[cardId].flags & 1) {
            while (i > 0) {
                i--;
                power++;
            }
        }
    }

    i = CardPlayCntOpe(own, 134) + CardPlayCntOpe(foe, 134);
    if (i != 0) {
        if (work->definitions[cardId].flags & 4) {
            while (i > 0) {
                i--;
                power++;
            }
        }
    }

    i = CardPlayCntOpe(own, 144) + CardPlayCntOpe(foe, 144);
    if (i != 0) {
        while (i > 0) {
            i--;
            power++;
        }
    }

    switch (cardId) {
    case 15:
        if (stack->layers[1].cardId != 53 && stack->layers[1].cardId != 48) {
            for (i = 0; i < 4; i++) {
                if (own->battle[i].battle.layers[0].cardId >= 0) {
                    if (work->definitions[own->battle[i].battle.layers[0].cardId].flags & 8) {
                        power++;
                    }
                }
            }
        }
        break;
    case 14:
        for (i = 0; i < 4; i++) {
            if (own->battle[i].battle.layers[0].cardId == 79) {
                power++;
                break;
            }
        }
        break;
    case 76:
        CardPlayCommandPlay(side, 73, work, (s16) slot, 0);
        break;
    }

    return (power < 0) ? 0 : power;
}

extern int CardPlayCntOpe(CardPlaySide *side, int opeId);

/*
 * Extra battle effects the attacking card scores, added to the effect count
 * CGPBattleCalcuration keeps. A card whose stack carries flag 0x80 scores
 * nothing at all. Cards 3, 8 and 10 score one for card 79 standing anywhere on
 * their own battle row; they and card 26 score one more when the defending
 * card's definition carries flag 4, and any attacker stacking card 42 scores
 * three for the same definition flag.
 */
s32 CGPBattleAttackEffect(s32 side, CardGameWork *work, s32 effect,
                          CardLayerStack *attacker, CardLayerStack *defender)
{
    CardPlaySide *own;
    s16 attackCard;
    s16 i;

    own = &work->player;
    if (side != 0) {
        own = &work->enemy;
    }
    attackCard = attacker->layers[0].cardId;
    if (attacker->flags & 0x80) {
        effect = 0;
    } else if (defender != 0) {
        switch (attackCard) {
        case 3:
        case 8:
        case 10:
            for (i = 0; i < 4; i++) {
                if (own->battle[i].battle.layers[0].cardId == 79) {
                    effect++;
                    break;
                }
            }
            if (defender->layers[0].cardId >= 0 &&
                (work->definitions[defender->layers[0].cardId].flags & 4)) {
                effect++;
            }
            break;
        case 26:
            if (defender->layers[0].cardId >= 0 &&
                (work->definitions[defender->layers[0].cardId].flags & 4)) {
                effect++;
            }
            break;
        }
        if (attacker->layers[1].cardId == 42 &&
            defender->layers[0].cardId >= 0 &&
            (work->definitions[defender->layers[0].cardId].flags & 4)) {
            effect += 3;
        }
    }
    return effect;
}

s32 CGPBattleDiffenceEffect(s32 side, CardGameWork *work, s32 power, CardLayerStackView *defender, CardLayerStackView *attacker)
{
  CardDefinition *defs;
  s16 *p;
  s16 cnt;
  s16 i;
  s16 id;
  s16 j;
  id = CardPlayCntOpe((side == 0) ? (&work->enemy) : (&work->player), 0x7A);
  cnt = id;
  j = defender->layers[0].cardId;
  defs = work->definitions;
  if (defs[j].flags & 8)
  {
    for (i = 0; i < cnt; i++)
    {
      power--;
    }

  }
  p = &defender->layers[0].cardId;
  for (i = 0; i < 40; i++, p += 5)
  {
    id = *p;
    if (id < 0)
    {
      continue;
    }
    switch (id)
    {
      case 7:
        if (attacker->layers[0].cardId < 0)
      {
        break;
      }
        if (!(defs[attacker->layers[0].cardId].flags & 2))
      {
        break;
      }

      case 0x2C:

      case 0x2F:
        power--;
        break;

    }

  }

  return (power >= 0) ? (power) : (0);
}

/*
 * What a destroyed card leaves behind. The card survives with one life left
 * while the other side has card 81 in play; cards 22, 38, 59 and 60 each play a
 * command of their own, and 60 queues the last card of the other side's junk
 * pile again when a copy of 60 is destroyed there. Once the card is really
 * gone, operation card 126 recovers as many cards as the card reserved and
 * operation card 129 plays command 75 for every side that has one in play.
 */
s32 CGPBAEsub(s32 side, CardGameWork *work, s32 power, CardLayerStack *attacker,
              CardLayerStack *defender)
{
    CardPlaySide *own;
    CardPlaySide *foe;
    s16 *command;
    s16 foeSide;
    s16 ownSlot;
    s16 i;
    s32 card;
    s32 cardId;
    s32 found;
    s32 junkIndex;

    cardId = defender->layers[0].cardId;
    if (side == 0) {
        foeSide = 1;
        own = &work->player;
        foe = &work->enemy;
    } else {
        foeSide = 0;
        own = &work->enemy;
        foe = &work->player;
    }
    if (defender->layers[0].life <= 0) {
        defender->layers[0].life = 0;

        ownSlot = -1;
        for (i = 0; i < 4; i++) {
            if (attacker == &own->battle[i].battle) {
                ownSlot = i;
                break;
            }
        }
        for (i = 0; i < 4; i++) {
            if (defender == &foe->battle[i].battle) {
                break;
            }
        }

        switch (cardId) {
        case 3:
        case 8:
        case 10:
            if (CardPlayCntBtl((CardHand *) foe, 81) != 0) {
                defender->layers[0].life = 1;
                CardPlayCommandPlay(side, 71, work, foeSide, 0);
            }
            break;
        case 22:
            CardPlayCommandPlay(side, 66, work, foeSide, 0);
            break;
        case 38:
            CardPlayCommandPlay(side, 68, work, 0, 0);
            break;
        case 59:
            CardPlayCommandPlay(side, 69, work, ownSlot, 0);
            break;
        case 60:
            found = 0;
            for (i = 0; i < 4; i++) {
                if (foe->battle[i].battle.layers[0].cardId == 60 &&
                    foe->battle[i].battle.layers[0].life <= 0) {
                    found = 1;
                    break;
                }
            }
            if (found != 0) {
                /* the head of a side record is the play record of its hand */
                junkIndex = CardPlayJunkCnt((CardPlayHand *) foe) - 1;
                if (junkIndex >= 0) {
                    card = foe->junk[junkIndex];
                    if (work->definitions[card].kind == 1) {
                        if ((work->definitions[card].flags & 0xB) != 0) {
                            if (CardPlayIdentityCheck(work, (CardHand *) foe,
                                                      work->definitions[card].identity) == 0) {
                                command = CPCSearchCommWk(side, work);
                                if (command != 0) {
                                    command[1] = 10;
                                    command[0] = 6;
                                    if (side != 0) {
                                        command[0] = 13;
                                    }
                                    command[2] = i;
                                    command[3] = junkIndex;
                                }
                            }
                        }
                    }
                }
            }
            break;
        }

        if (defender->layers[0].life <= 0) {
            if (CardPlayCntOpe(foe, 126) != 0) {
                for (i = 0; i < work->definitions[defender->layers[0].cardId].reserveCost;
                     i++) {
                    CardPlayRecavery((CardHand *) foe);
                }
            }
            if (work->definitions[defender->layers[0].cardId].flags & 2) {
                if (CardPlayCntOpe(own, 129) != 0) {
                    CardPlayCommandPlay(side, 75, work, (s16) side, 0);
                }
                if (CardPlayCntOpe(foe, 129) != 0) {
                    CardPlayCommandPlay(side, 75, work, foeSide, 0);
                }
            }
        }
    }
    return 0;
}



/*
 * Effects that resolve once a battle is over. While the battle produced an
 * effect, the defending cards 2 and 9 mark the attacking stack, cards 30, 31,
 * 67 and 72 mark it as well, and card 0 queues command 3 for the side once the
 * attacking stack has run out of life. Both the attacking and the supporting
 * stack then run their own after-effects.
 */
void CGPBattleAfterEffect(s32 side, CardGameWork *work, s32 effect, s32 operand,
                          CardLayerStack *defender, CardLayerStack *attacker,
                          CardLayerStack *support)
{
    s16 card;

    card = defender->layers[0].cardId;
    if (card == 2 || card == 9) {
        if (effect != 0 && attacker != 0) {
            attacker->flags |= 2;
        }
    }
    if (effect != 0) {
        switch (card) {
        case 0:
            if (attacker->layers[0].life <= 0) {
                CardPlayCommandPlay(side, 3, work, operand, 0);
            }
            break;
        case 30:
        case 31:
            attacker->flags |= 2;
            break;
        case 67:
        case 72:
            attacker->flags |= 2;
            break;
        }
    }
    if (attacker != 0) {
        CGPBAEsub(side, work, effect, defender, attacker);
    }
    if (support != 0) {
        CGPBAEsub(side, work, effect, defender, support);
    }
}

extern s32 CGPBattleAttackEffect(s32 side, CardGameWork *work, s32 power,
                                 CardLayerStack *attacker, CardLayerStack *defender);
extern s32 CGPBattleDiffenceEffect(s32 side, CardGameWork *work, s32 power,
                                   CardLayerStack *defender, CardLayerStack *attacker);

/*
 * Damage one attack deals. The attacker's effects raise the power, the
 * defender's lower it, and what is left is taken off the defending card. A
 * card stacked on the attacker that pierces (34, 35, 37 or 43) lets the damage
 * that destroys the defending card pass to the card behind it, which sets the
 * destroyed code to 1 for the defending card alone and to 2 for both. The
 * damage still left over is returned.
 */
s32 CGPBattleCalcuration(s32 side, CardGameWork *work, s32 power, s32 *battleDone,
                         CardLayerStack *attacker, CardLayerStack *defender,
                         CardLayerStack *behind, s32 *destroyed)
{
    s32 damage;
    s32 taken;
    s32 pierces;
    s32 i;

    power = CGPBattleAttackEffect(side, work, power, attacker, defender);
    damage = CGPBattleDiffenceEffect(side, work, power, defender, attacker);
    *battleDone = 1;
    pierces = 0;
    if (damage != 0) {
        for (i = 39; i >= 0; i--) {
            if (attacker->layers[i].cardId >= 0) {
                if (attacker->layers[i].cardId == 34 || attacker->layers[i].cardId == 35 ||
                    attacker->layers[i].cardId == 37 || attacker->layers[i].cardId == 43) {
                    pierces = 1;
                    break;
                }
            }
        }
        if (pierces != 0 && defender->layers[0].life < damage) {
            *destroyed = 1;
            taken = defender->layers[0].life;
            defender->layers[0].life = 0;
            CardSetEffect(side, work, 11, taken, 0, defender->position, defender);
            damage -= taken;
            if (behind != 0) {
                if (behind->layers[0].life < damage) {
                    taken = behind->layers[0].life;
                    behind->layers[0].life = 0;
                    damage -= taken;
                    *destroyed = 2;
                } else {
                    taken = damage;
                    damage = 0;
                    behind->layers[0].life -= taken;
                }
                CardSetEffect(side, work, 22, 0, 0, attacker->position, behind);
                CardSetEffect(side, work, 15, taken, 0, behind->position, behind);
            }
        } else {
            CardSetEffect(side, work, 11, damage, 0, defender->position, defender);
            defender->layers[0].life -= damage;
            damage = 0;
        }
    }
    if (defender != 0) {
        if (defender->layers[0].life < 0) {
            defender->layers[0].life = 0;
        }
    }
    if (behind != 0) {
        if (behind->layers[0].life < 0) {
            behind->layers[0].life = 0;
        }
    }
    return damage;
}

/*
 * Base attack power of the topmost card on a stack, scanning down from the
 * last layer for the highest occupied one; ids 44, 47 and 53 are placeholder
 * equipment cards, so the base layer (index 0) is used instead when the
 * topmost occupied layer holds one of them.
 */
s32 CGPCalcPower(CardGameWork *work, CardLayerStack *stack)
{
    s32 i;
    s32 cardId;

    for (i = 39; i >= 0; i--) {
        if (stack->layers[i].cardId >= 0) {
            break;
        }
    }
    cardId = stack->layers[i].cardId;
    if (cardId == 44 || cardId == 47 || cardId == 53) {
        i = 0;
    }
    return work->definitions[stack->layers[i].cardId].attackPower;
}

/*
 * Power a card in play attacks with: its base power plus the two bonuses it
 * carries, doubled once per boost. A card whose attack type is 4 spends its
 * boosts instead of doubling.
 */
s32 CGPCalcPowerPlus(CardGameWork *work, CardPowerCard *card, s32 power)
{
    s32 boosts;

    if (card->cardId < 0) {
        power = 0;
    } else {
        power += card->powerPlus[0];
        power += card->powerPlus[1];
        if (CardChkAttackType(work->definitions, card) == 4) {
            card->boostCount = 0;
        }
        boosts = card->boostCount;
        if (boosts > 0) {
            do {
                boosts--;
                power *= 2;
            } while (boosts != 0);
        }
    }
    return power;
}

/*
 * Power a card gains or loses from the operation cards in play. Operation 120
 * on either side costs a card carrying flag 4 one point; every copy of 123 on
 * this side adds one to a card carrying flag 1, every copy of 134 on either
 * side one to a card carrying flag 4, and every copy of 144 one to any card.
 * Card 14 also gains a point while card 79 stands on one of this side's battle
 * slots, and card 15 one for every battle slot holding a card with flag 8.
 */
s32 CGPCalcPowerEnv(s32 side, CardGameWork *work, s32 power, s32 cardId)
{
    CardPlaySide *ownSide;
    CardPlaySide *foeSide;
    s32 i;

    if (side == 0) {
        ownSide = &work->player;
        foeSide = &work->enemy;
    } else {
        ownSide = &work->enemy;
        foeSide = &work->player;
    }
    if (CardPlayCntOpe(ownSide, 120) != 0 || CardPlayCntOpe(foeSide, 120) != 0) {
        if (work->definitions[cardId].flags & 4) {
            power--;
        }
    }
    i = CardPlayCntOpe(ownSide, 123);
    if (i != 0) {
        if (work->definitions[cardId].flags & 1) {
            while (i > 0) {
                i--;
                power++;
            }
        }
    }
    i = CardPlayCntOpe(ownSide, 134) + CardPlayCntOpe(foeSide, 134);
    if (i != 0) {
        if (work->definitions[cardId].flags & 4) {
            while (i > 0) {
                i--;
                power++;
            }
        }
    }
    i = CardPlayCntOpe(ownSide, 144) + CardPlayCntOpe(foeSide, 144);
    if (i != 0) {
        while (i > 0) {
            i--;
            power++;
        }
    }
    switch (cardId) {
    case 15:
        for (i = 0; i < 4; i++) {
            if (ownSide->battle[i].battle.layers[0].cardId >= 0 &&
                (work->definitions[ownSide->battle[i].battle.layers[0].cardId].flags & 8)) {
                power++;
            }
        }
        break;
    case 14:
        for (i = 0; i < 4; i++) {
            if (ownSide->battle[i].battle.layers[0].cardId == 79) {
                power++;
                break;
            }
        }
        break;
    }
    return power;
}

typedef struct CGPBoardArea {
    s16 area;
    s16 slot;
} CGPBoardArea;

extern s32 CardPlayYamaCnt(CardPlaySideView *side);
extern s32 CardPlaySuteCnt(CardPlaySideView *side);
typedef struct CgpMoveRec {
    u8 unk0[2];
    u8 state;
    u8 timer;
    float pos[3];
    float vel[3];
} CgpMoveRec;
typedef struct CgpMoveView {
    u8 pad[0x14CC];
    CgpMoveRec mv[5][40];
} CgpMoveView;
#define CGP_MV(r, n) (((CgpMoveView *)side)->mv[r][n])

void CGPHPCheckSub(CardDefinition *definitions, CardPlaySideView *side, CardLayerStackView *stack, CGPBoardArea *place)
{
    s32 i;
    s32 n;

    side->hand.unmodeled_04[0] = CardPlayYamaCnt(side);
    side->hand.unmodeled_04[1] = CardPlaySuteCnt(side);
    stack->unmodeled_04[0] = stack->flags;
    if (stack->layers[0].cardId < 0) {
        return;
    }
    if (stack->layers[0].damageTimer != 0) {
        stack->layers[0].damageTimer--;
    } else {
        stack->layers[0].unmodeled_04 = stack->layers[0].life;
    }
    if (stack->layers[1].cardId < 0) {
        if ((s8)stack->powerLimit != definitions[stack->layers[0].cardId].basePower) {
            stack->power = definitions[stack->layers[0].cardId].basePower;
            stack->powerLimit = definitions[stack->layers[0].cardId].basePower;
        }
    }
    if (stack->layers[0].damageTimer == 0 && stack->layers[0].life <= 0) {
        if (CardPlayCntOpe(side, 0x82) != 0 && (definitions[stack->layers[0].cardId].flags & 4)) {
            n = CardPlaySuteCnt(side);
            for (i = 0; i < 40; i++) {
                if (stack->layers[i].cardId >= 0) {
                    CGP_MV(3, n).timer = 10;
                    CGP_MV(3, n).state = 0;
                    CGP_MV(3, n).pos[0] = stack->position[0];
                    CGP_MV(3, n).pos[1] = stack->position[1];
                    CGP_MV(3, n).pos[2] = 0.12f;
                    CGP_MV(3, n).vel[0] = (1.265f - stack->position[0]) / 10.0f;
                    CGP_MV(3, n).vel[1] = (-1.81f - stack->position[1]) / 10.0f;
                    CGP_MV(3, n).vel[2] = 0.0f;
                    side->sute[n] = stack->layers[i].cardId;
                    stack->layers[i].cardId = -1;
                    n++;
                }
            }
        } else {
            n = CardPlayJunkCnt((CardPlayHand *)side);
            for (i = 0; i < 40; i++) {
                if (stack->layers[i].cardId >= 0) {
                    if (place->area == 2 || place->area == 6) {
                        CGP_MV(4, n).pos[0] = side->disposal[place->slot].battle.position[0];
                        CGP_MV(4, n).pos[1] = side->disposal[place->slot].battle.position[1];
                        CGP_MV(4, n).pos[2] = 0.12f;
                    } else {
                        CGP_MV(4, n).pos[0] = side->battle[place->slot].battle.position[0];
                        CGP_MV(4, n).pos[1] = side->battle[place->slot].battle.position[1];
                        CGP_MV(4, n).pos[2] = 0.12f;
                    }
                    CGP_MV(4, n).vel[0] = (JunkPos1P[3] - CGP_MV(4, n).pos[0]) / 10.0f;
                    CGP_MV(4, n).vel[1] = (JunkPos1P[4] - CGP_MV(4, n).pos[1]) / 10.0f;
                    CGP_MV(4, n).vel[2] = 0.0f;
                    CGP_MV(4, n).timer = 10;
                    side->junk[n] = stack->layers[i].cardId;
                    stack->layers[i].cardId = -1;
                    n++;
                }
            }
        }
    }
    if (stack->layers[1].cardId >= 0 && stack->layers[1].life <= 0) {
        stack->power = definitions[stack->layers[0].cardId].basePower;
        stack->powerLimit = definitions[stack->layers[0].cardId].basePower;
        n = CardPlayJunkCnt((CardPlayHand *)side);
        if (place->area == 2 || place->area == 6) {
            CGP_MV(4, n).pos[0] = side->disposal[place->slot].battle.position[0];
            CGP_MV(4, n).pos[1] = side->disposal[place->slot].battle.position[1];
            CGP_MV(4, n).pos[2] = 0.12f;
        } else {
            CGP_MV(4, n).pos[0] = side->battle[place->slot].battle.position[0];
            CGP_MV(4, n).pos[1] = side->battle[place->slot].battle.position[1];
            CGP_MV(4, n).pos[2] = 0.12f;
        }
        CGP_MV(4, n).vel[0] = (JunkPos1P[3] - CGP_MV(4, n).pos[0]) / 10.0f;
        CGP_MV(4, n).vel[1] = (JunkPos1P[4] - CGP_MV(4, n).pos[1]) / 10.0f;
        CGP_MV(4, n).vel[2] = 0.0f;
        CGP_MV(4, n).timer = 10;
        side->junk[n] = stack->layers[1].cardId;
        stack->layers[1].cardId = -1;
    }
}

extern void CGPHPCheckSub(CardDefinition *definitions, CardPlaySide *side,
                          CardLayerStack *stack, CGPBoardArea *place);

/*
 * Recheck the hit points of every card standing on either board: first the
 * four disposal slots of both sides, then their four battle slots.
 */
void CGPRefreshFieldHP(CardGameWork *work)
{
    CGPBoardArea playerPlace;
    CGPBoardArea enemyPlace;
    CardPlaySide *player;
    CardPlaySide *enemy;
    s32 i;

    player = &work->player;
    enemy = &work->enemy;
    playerPlace.area = 6;
    enemyPlace.area = 2;
    for (i = 0; i < 4; i++) {
        playerPlace.slot = i;
        enemyPlace.slot = i;
        CGPHPCheckSub(work->definitions, player,
                      &player->disposal[i].battle, &playerPlace);
        CGPHPCheckSub(work->definitions, enemy,
                      &enemy->disposal[i].battle, &enemyPlace);
    }
    playerPlace.area = 5;
    enemyPlace.area = 4;
    for (i = 0; i < 4; i++) {
        playerPlace.slot = i;
        enemyPlace.slot = i;
        CGPHPCheckSub(work->definitions, player,
                      &player->battle[i].battle, &playerPlace);
        CGPHPCheckSub(work->definitions, enemy,
                      &enemy->battle[i].battle, &enemyPlace);
    }
}

extern int CardPlayCommandCheck(s16 command, s16 *queue);
extern void CardPlayCommandBattleExecute(s16 side, s16 commandId, CardGameWork *work);
extern void CardCursorActive(CardCursorControl *cursor, s16 *position, int mode);
extern void xglSoundEffectParamDirect(int sound, int volume, int pan);
typedef struct CgpSlotS8 {
    float shuntPosition[3];
    u8 flags;
    u8 unmodeled_0D;
    s8 power;
    u8 powerLimit;
    u8 unmodeled_10[0x1BC - 0x10];
} CgpSlotS8;
typedef struct CgpSideS8 {
    u8 unmodeled_000[0x190];
    CgpSlotS8 battle[4];
} CgpSideS8;

s32 CGPBattleSub(s32 side, u16 type, s32 slot, CardGameWork *work, CardPlaySide *own,
                 CardPlaySide *opp, CardLayerStack *target)
{
    s16 pos[2];
    s32 battleDone;
    s32 attackDone;
    s32 result;
    s32 isPlayer;
    s16 *queue;
    CardLayerStack *def;
    CardLayerStack *stack;
    CardDefinition *defs;
    s32 i;
    s32 cardId;
    s32 multi;
    s32 atype;
    s32 power;
    s32 dmg;
    s32 k;
    s32 effect;
    u8 code;

    attackDone = 0;
    result = 0;
    isPlayer = side == 0;
    queue = work->commandQueue[0][0];
    if (side != 0) {
        queue = work->commandQueue[1][0];
    }
    if (CardPlayCommandCheck(work->command.phase, queue) != 0) {
        CardPlayCommandBattleExecute(side, work->command.phase, work);
        result = 1;
        CGPRefreshFieldHP(work);
    } else if (own->battle[slot].battle.layers[0].cardId >= 0) {
        for (i = 39; i >= 0; i--) {
            if (own->battle[slot].battle.layers[i].cardId >= 0) {
                break;
            }
        }
        cardId = own->battle[slot].battle.layers[i].cardId;
        multi = i != 0;
        if (cardId == 0x2C || cardId == 0x2F || cardId == 0x35) {
            i = 0;
        }
        defs = work->definitions;
        stack = &own->battle[slot].battle;
        dmg = defs[own->battle[slot].battle.layers[i].cardId].attackPower;
        atype = CardChkAttackType(defs, (CardPowerCard *)stack);
        dmg = CGPCalcPowerPlus(work, (CardPowerCard *)stack, dmg);
        if (type == atype) {
            pos[0] = 5;
            if (side != 0) {
                pos[0] = 4;
            }
            pos[1] = slot;
            CardCursorActive(&work->cursor, pos, 2);
            if (atype != 3 && (s8)own->battle[slot].battle.power > 0 &&
                !(own->battle[slot].battle.flags & 2)) {
                ((CgpSideS8 *)own)->battle[slot].power--;
            }
            if ((s8)own->battle[slot].battle.power <= 0 && !(own->battle[slot].battle.flags & 2)) {
                if (type != 3) {
                    own->battle[slot].battle.power = own->battle[slot].battle.powerLimit;
                }
                if (multi != 0 &&
                    work->definitions[own->battle[slot].battle.layers[1].cardId].unmodeled_13 != 3 &&
                    own->battle[slot].battle.layers[1].life < 0x40) {
                    own->battle[slot].battle.layers[1].life--;
                }
                dmg = CGPBattleBeforeEffect(side, work, dmg, &own->battle[slot].battle, slot);
                if (atype == 4 &&
                    (CardPlayCntBtl(&own->hand, 0x27) != 0 || CardPlayCntBtl(&opp->hand, 0x27) != 0)) {
                    dmg = 0;
                }
                if (own->battle[slot].battle.flags & 0x80) {
                    dmg = 0;
                }
                if (dmg != 0) {
                    CardLayerStack *behind;
                    battleDone = 0;
                    def = 0;
                    behind = 0;
                    if (type == 3) {
                        if (target != 0 && target->layers[0].cardId >= 0) {
                            def = target;
                            if (CardPlayCntOpe(opp, 0x7C) != 0 &&
                                (work->definitions[def->layers[0].cardId].flags & 1)) {
                                dmg = 0;
                            }
                            if (dmg != 0) {
                                CGPBattleCalcuration(side, work, dmg, &battleDone,
                                                     &own->battle[slot].battle, def, 0, &attackDone);
                                ((s32 (*)(s32, CardGameWork *, u8, s8, s32, float *, CardLayerStack *))CardSetEffect)(
                                    isPlayer, work, 5, 0, 0x20008, def->position, def);
                            }
                        }
                        goto zero;
                    } else {
                        k = slot & 1;
                        if (opp->battle[1 - k].battle.layers[0].cardId >= 0) {
                            def = &opp->battle[1 - k].battle;
                            if (opp->battle[3 - k].battle.layers[0].cardId >= 0) {
                                behind = &opp->battle[3 - k].battle;
                            }
                        } else if (!(atype & 1)) {
                            if (opp->battle[3 - k].battle.layers[0].cardId >= 0) {
                                def = &opp->battle[3 - k].battle;
                            }
                        }
                        if (CardPlayCntOpe(opp, 0x7C) != 0 && def != 0) {
                            if ((u16)(work->definitions[def->layers[0].cardId].flags & 1)) {
                                if (behind == 0 ||
                                    (def = behind, behind = 0,
                                     (u16)(work->definitions[def->layers[0].cardId].flags & 1))) {
                                    def = 0;
                                }
                            }
                        }
                        if (slot < 2) {
                            switch (atype) {
                            case 1:
                                if (def != 0) {
                                    CGPBattleCalcuration(side, work, dmg, &battleDone,
                                                         &own->battle[slot].battle, def, 0, &attackDone);
                                    if (work->definitions[own->battle[slot].battle.layers[0].cardId].flags & 8) {
                                        CardSetEffect(side, work, 1, 0, 0x20005,
                                                      own->battle[slot].battle.position, def);
                                    } else {
                                        CardSetEffect(side, work, 1, 0, 0x20002,
                                                      own->battle[slot].battle.position, def);
                                    }
                                }
                                dmg = 0;
                                break;
                            case 2:
                                if (def != 0) {
                                    effect = 0x20004;
                                    if (work->definitions[own->battle[slot].battle.layers[0].cardId].flags & 8) {
                                        effect = 0x20009;
                                    }
                                    dmg = CGPBattleCalcuration(side, work, dmg, &battleDone,
                                                                 &own->battle[slot].battle, def, behind,
                                                                 &attackDone);
                                    CardSetEffect(side, work, 2, 0, effect, own->battle[slot].battle.position, def);
                                }
                                break;
                            }
                        } else {
                            switch (atype) {
                            case 1:
                            zero:
                                dmg = 0;
                                break;
                            case 2:
                                if (def != 0) {
                                    effect = 0x20004;
                                    if (work->definitions[own->battle[slot].battle.layers[0].cardId].flags & 8) {
                                        effect = 0x20009;
                                    }
                                    dmg = CGPBattleCalcuration(side, work, dmg, &battleDone,
                                                                 &own->battle[slot].battle, def, behind,
                                                                 &attackDone);
                                    CardSetEffect(side, work, 2, 0, effect, own->battle[slot].battle.position, def);
                                }
                                break;
                            }
                        }
                    }
                    CGPBattleAfterEffect(side, work, battleDone, slot, &own->battle[slot].battle, def, behind);
                }
                if (dmg >= 100) {
                    dmg = 99;
                }
                if (dmg != 0) {
                    s32 effect2 = 0x20004;
                    if (work->definitions[own->battle[slot].battle.layers[0].cardId].flags & 8) {
                        effect2 = 0x20009;
                    }
                    if (atype == 2) {
                        if (attackDone == 0) {
                            CardSetEffect(side, work, 2, 0, effect2, own->battle[slot].battle.position, 0);
                        } else if (attackDone == 1) {
                            CardSetEffect(side, work, 0x16, 0, effect2, own->battle[slot].battle.position, 0);
                        } else {
                            CardSetEffect(side, work, 0x20, 0, effect2, own->battle[slot].battle.position, 0);
                        }
                    } else if (atype == 4) {
                        xglSoundEffectParamDirect(0x20007, 0x3C, 0x40);
                        CardSetEffect(side, work, 4, 0, 0, own->battle[slot].battle.position, 0);
                    }
                    if (!(opp->hand.commandFlags & 1)) {
                        i = CardPlayCntOpe(own, 0x87) + CardPlayCntOpe(opp, 0x87);
                        if (i != 0 &&
                            (work->definitions[own->battle[slot].battle.layers[0].cardId].flags & 4) &&
                            (s8)own->hand.unmodeled_02 > 0) {
                            (*(s8 *)&own->hand.unmodeled_02)--;
                            CardPlayCommandPlay(side, 0x11, work, 0, 0);
                        }
                        if (attackDone == 0) {
                            CardSetEffect(side, work, 0xB, dmg, 0x20008, 0, 0);
                        } else if (attackDone == 1) {
                            CardSetEffect(side, work, 0xF, dmg, 0x20008, 0, 0);
                        } else {
                            CardSetEffect(side, work, 0x10, dmg, 0x20008, 0, 0);
                        }
                        for (i = 0; i < dmg; i++) {
                            CardPlayCostCard((CardPlayHand *)opp);
                        }
                    } else {
                        CardPlayCommandPlay(side, 0x4F, work, 0, 0);
                    }
                }
            }
        }
    }
    return result;
}

void CGPRotStageSub(CardGameWork *work, u32 stage)
{
    if ((work->save->flags & CARD_SAVE_ROTATE_STAGE) && work->fase == 9 &&
        work->rotStageId != stage && work->rotStageCooldown == 0)
    {
        work->rotStageId = stage;
        work->rotStageCooldown = 20;
    }
}

extern void CGCPHandDispSub2(CardDefinition *defs, CardHand *hand, s32 index, s32 mode);
extern void CGCPHandDispSub3(CardDefinition *defs, CardHand *hand, s32 index, s32 mode, s32 flag);
extern void CGPSetErrorMessPlusI(s32 code, CardGameWork *work, u8 reason, s16 value) __asm__("CGPSetErrorMessPlus");
extern void CGCPYamaDispSub(CardPlaySide *side, s32 a, s32 b, s32 c, s32 d);
extern void CardPlayDrawSute(CardPlaySide *side);
extern void CardPlayShuffleYama(CardPlaySide *side);
extern s32 CardPlayCntDsp(CardPlaySide *side, s16 cardId);
extern int CardPlayDrawCard(CardPlaySide *side);
extern s32 CardPlayCntAllOpe(CardGameWork *work, s32 opeId);
extern s32 D_00A57DC0;
extern u8 D_00A57DC4;
extern u8 D_00A57DC5;
extern s8 D_00A57DC6;

void CGPDrawFaseProc(s32 side, CardGameWork *work, CardPlaySide *own, CardPlaySide *opp)
{
    CardPlaySide *cur;
    u16 held;
    u16 pressed;
    s32 auto_;

    auto_ = 0;
    if (side == 0) {
        cur = &work->player;
        held = work->command.input[0].held;
        pressed = work->command.input[0].pressed;
        if (work->flags & 0x40) {
            auto_ = 1;
        }
    } else {
        cur = &work->enemy;
        held = work->command.input[1].held;
        pressed = work->command.input[1].pressed;
        if (work->flags & 0x20) {
            auto_ = 1;
        }
    }

    switch ((s16)work->command.phaseTimer) {
    case 0:
        D_00A57DC0 = 0;
        D_00A57DC5 = 3;
        D_00A57DC6 = 0;
        *(s16 *)&work->command.unmodeled_20[0] = 0x16;
        *(s16 *)&work->command.unmodeled_20[2] = 0x2D;
        own->hand.commandFlags = 0;
        own->hand.playPoints[0] = 1;
        own->hand.playPoints[1] = 1;
        own->hand.unmodeled_02 = 1;
        *(s16 *)&work->cursor.unmodeled_02[0] = 0;
        work->cursor.selectionMode = 0;
        CGPRotStageSub(work, side);
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
    case 1:
        if (work->command.turn != 0) {
            goto ten;
        }
        if (side == 0) {
            if (work->flags & 0x40) {
                goto ten;
            }
            auto_ = 0;
        } else {
            if (work->flags & 0x20) {
                work->command.phaseTimer = 10;
                return;
            }
        }
        if (D_00A57DC5 != 0) {
            work->command.phaseTimer = work->command.phaseTimer + 1;
            return;
        }
    ten:
        work->command.phaseTimer = 10;
        return;
    case 2:
        {
        s32 cnt;
        s32 reason;
        cnt = CardPlayHandCnt(&cur->hand);
        reason = 0;
        switch (D_00A57DC5) {
        case 3:
            reason = 0x21;
            break;
        case 2:
            reason = 0x22;
            break;
        case 1:
            reason = 0x23;
            break;
        }
        if ((work->rotStageId == 0 && (pressed & 0x2000)) || (work->rotStageId != 0 && (pressed & 0x8000))) {
            if (D_00A57DC6 < cnt - 1) {
                xglSoundEffectNormalID(3, 0);
                D_00A57DC6++;
            }
        } else if ((work->rotStageId == 0 && (pressed & 0x8000)) || (work->rotStageId != 0 && (pressed & 0x2000))) {
            if (D_00A57DC6 > 0) {
                xglSoundEffectNormalID(3, 0);
                D_00A57DC6--;
            }
        }
        if (pressed & 0x40) {
            xglSoundEffectNormalID(1, 0);
            work->command.phaseTimer = work->command.phaseTimer + 1;
        } else if (pressed & 0x20) {
            xglSoundEffectNormalID(2, 0);
            work->command.phaseTimer = 10;
        }
        if (held & 0x10) {
            CGPDispErrorMessPlus(side, reason, cur->hand.cards[D_00A57DC6]);
        } else {
            CGPDispErrorMess((CardGameWork *)side, reason);
        }
        if ((side != 0 && work->rotStageId == 0) || (side == 0 && work->rotStageId != 0)) {
            CGCPHandDispSub3(work->definitions, &cur->hand, D_00A57DC6, 0, 0);
        } else {
            CGCPHandDispSub2(work->definitions, &cur->hand, D_00A57DC6, 0);
        }
        return;
        }
    case 3:
        {
        s32 n;
        s32 y;
        s32 i;
        n = CardPlayHandCnt(&cur->hand);
        y = CardPlayYamaCnt(cur);
        for (i = 0; i < n; i++) {
            cur->deck[y + i] = cur->hand.cards[i];
            cur->hand.cards[i] = -1;
        }
        CardPlayShuffleYama(cur);
        for (i = 0; i < 6; i++) {
            CardPlayDrawCard(cur);
        }
        work->command.phaseTimer = 1;
        D_00A57DC5--;
        return;
        }
    case 10:
        work->command.phaseTimer = work->command.phaseTimer + 1;
    case 11:
        {
        s32 i;
        s16 pos[2];
        D_00A57DC0 = 0;
        if (CardPlayCntDsp(own, 0x38) != 0) {
            D_00A57DC0 += 1;
        }
        if (CardPlayCntBtl(&own->hand, 0x38) != 0) {
            D_00A57DC0 += 2;
        }
        if (CardPlayCntDsp(opp, 0x38) != 0) {
            D_00A57DC0 += 1;
        }
        if (CardPlayCntBtl(&opp->hand, 0x38) != 0) {
            D_00A57DC0 += 2;
        }
        if (D_00A57DC0 == 0) {
            work->command.phaseTimer = 20;
            return;
        }
        if (auto_ == 0) {
            CGPSetErrorMessPlusI(side, work, 0x24, 0x38);
        }
        for (i = 0; i < 4; i++) {
            if (work->player.battle[i].battle.layers[0].cardId == 0x38) {
                pos[0] = 5;
                pos[1] = i;
                CardCursorActive(&work->cursor, pos, 2);
            }
            if (work->enemy.battle[i].battle.layers[0].cardId == 0x38) {
                pos[0] = 4;
                pos[1] = i;
                CardCursorActive(&work->cursor, pos, 2);
            }
        }
        for (i = 0; i < 4; i++) {
            if (work->player.disposal[i].battle.layers[0].cardId == 0x38) {
                pos[0] = 6;
                pos[1] = i;
                CardCursorActive(&work->cursor, pos, 2);
            }
            if (work->enemy.disposal[i].battle.layers[0].cardId == 0x38) {
                pos[0] = 2;
                pos[1] = i;
                CardCursorActive(&work->cursor, pos, 2);
            }
        }
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
        }
    case 12:
        CardPlayCostCard((CardPlayHand *)own);
        D_00A57DC0--;
        work->command.phaseTimer++;
        return;
    case 13:
        if (D_00A57DC0 <= 0) {
            work->command.phaseTimer = 20;
        } else {
            work->command.phaseTimer = (s16)work->command.phaseTimer - 1;
        }
        return;
    case 20:
        {
        s32 i;
        s16 pos2[2];
        CardCursorPassive(&work->cursor);
        D_00A57DC0 = 0;
        if (auto_ == 0) {
            if (CardPlayCntOpe(own, 0x84) != 0) {
                for (i = 0; i < 4; i++) {
                    if (own->operations[i] == 0x84) {
                        pos2[0] = 7;
                        if (side != 0) {
                            pos2[0] = 3;
                        }
                        pos2[1] = i;
                        CardCursorActive(&work->cursor, pos2, 2);
                    }
                }
                work->command.phaseTimer = work->command.phaseTimer + 1;
            } else {
                work->command.phaseTimer = 30;
            }
        } else {
            work->command.phaseTimer = 30;
        }
        return;
        }
    case 21:
        {
        s32 y;
        s32 i;
        s16 last;
        s32 flag;
        CGPDispErrorMess3((CardGameWork *)side, 0x1A);
        if (pressed & 0x40) {
            xglSoundEffectNormalID(1, 0);
            y = CardPlayYamaCnt(own);
            last = own->deck[y - 1];
            for (i = y - 1; i > 0; i--) {
                own->deck[i] = own->deck[i - 1];
            }
            own->deck[0] = last;
            work->command.phaseTimer = 30;
        } else if (pressed & 0x20) {
            xglSoundEffectNormalID(1, 0);
            work->command.phaseTimer = 30;
        }
        flag = 1;
        if (!(held & 0x10)) {
            if ((side != 0 && work->rotStageId == 0) || (side == 0 && work->rotStageId != 0)) {
                CGCPHandDispSub3(work->definitions, &cur->hand, -1, 0, 0);
            } else {
                flag = 0;
                CGCPHandDispSub2(work->definitions, &cur->hand, -1, 0);
            }
        }
        CGCPYamaDispSub(own, 0, 1, held, flag);
        return;
        }
    case 30:
        CardCursorPassive(&work->cursor);
        if ((D_00A57DC0 = CardPlayCntAllOpe(work, 0x8C)) != 0) {
            CGPSetErrorMessPlusI(side, work, 0x24, 0x8C);
        }
        if (CardPlayCntBtl(&own->hand, 0x4E) != 0) {
            D_00A57DC0++;
        }
        D_00A57DC0++;
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
    case 31:
        if (auto_ == 0) {
            if ((side != 0 && work->rotStageId == 0) || (side == 0 && work->rotStageId != 0)) {
                CGCPHandDispSub3(work->definitions, &cur->hand, D_00A57DC6, 0, 0);
            } else {
                CGCPHandDispSub2(work->definitions, &cur->hand, D_00A57DC6, 0);
            }
        }
        if (CardPlayCntOpe(own, 0x89) != 0 || CardPlayCntBtl(&own->hand, 0x4E) != 0) {
            CardPlayDrawSute(own);
            D_00A57DC4 = 0x32;
        } else {
            CardPlayDrawCard(own);
            D_00A57DC4 = 0x32;
        }
        work->command.phaseTimer = work->command.phaseTimer + 1;
        D_00A57DC0--;
        return;
    case 32:
        if (auto_ == 0) {
            if ((side != 0 && work->rotStageId == 0) || (side == 0 && work->rotStageId != 0)) {
                CGCPHandDispSub3(work->definitions, &cur->hand, D_00A57DC6, 0, 0);
            } else {
                CGCPHandDispSub2(work->definitions, &cur->hand, D_00A57DC6, 0);
            }
        }
        if (D_00A57DC4 != 0) {
            D_00A57DC4--;
        } else if (D_00A57DC0 == 0) {
            CGPNextTurnSub(work);
        } else {
            work->command.phaseTimer = (s16)work->command.phaseTimer - 1;
        }
        break;
    }
}

extern s16 D_00A57DC8;
extern CGPCursorPosition D_00A57DD0;
extern s32 CardPlayDisposeCnt(CardHand *hand);
extern s32 CardPlayBattleCnt(CardPlaySide *side);
#define CGP_ERRPLUS ((void (*)(s32, CardGameWork *, u8, s16))CGPSetErrorMessPlus)

void CGPMoveFaseProc(s32 side, CardGameWork *work, CardPlaySide *own)
{
    CGPCursorPosition pos;
    CGPCursorPosition pos1;
    CGPCursorPosition pos2;
    u16 pressed;
    s32 handZone;
    s32 dispZone;
    s32 btlZone;
    s32 i;
    CardMoveWork *src;
    CardMoveWork *dst;

    if (side == 0) {
        pressed = work->command.input[0].pressed;
        dispZone = 6;
        btlZone = 5;
        handZone = 9;
    } else {
        pressed = work->command.input[1].pressed;
        dispZone = 2;
        btlZone = 4;
        handZone = 0;
    }
    CGPCursor2Pos(&pos, work);
    if ((s16)work->command.phaseTimer != 0 && handZone == pos.zone &&
        *(u16 *)&work->cursorMoveStep[0] == 0) {
        work->cursor.selectionMode = 0;
        if ((side != 0 && work->rotStageId == 0) || (side == 0 && work->rotStageId != 0)) {
            CGCPHandDispSub3(work->definitions, &own->hand, pos.index, 0, 0);
        } else {
            CGCPHandDispSub2(work->definitions, &own->hand, pos.index, 0);
        }
    } else {
        work->cursor.selectionMode = 1;
    }

    switch ((s16)work->command.phaseTimer) {
    case 0:
        {
        s32 dispCnt;
        s32 btlCnt;
        D_00A57DC8 = -1;
        dispCnt = CardPlayDisposeCnt(&own->hand);
        btlCnt = CardPlayBattleCnt(own);
        if (dispCnt == 0 && btlCnt == 0) {
            work->command.phaseTimer = 0x14;
            if (side == 0 && (work->flags & 0x40)) {
                return;
            }
            if (side != 0 && (work->flags & 0x20)) {
                return;
            }
            if (side == 0 && (work->save->flags & 8)) {
                return;
            }
            if (side != 0 && (work->save->flags & 4)) {
                return;
            }
            CGPSetErrorMess(side, work, 6);
            return;
        }
        if (side == 0) {
            if (dispCnt != 0) {
                for (i = 0; i < 4; i++) {
                    if (own->disposal[i].battle.layers[0].cardId >= 0) {
                        work->cursorIndex = 0;
                        work->cursorZone = i + 5;
                        break;
                    }
                }
            } else {
                for (i = 0; i < 4; i++) {
                    if (own->battle[i].battle.layers[0].cardId >= 0) {
                        work->cursorIndex = (i & 1) + 1;
                        work->cursorZone = (i & 2) + 5;
                        break;
                    }
                }
            }
        } else {
            if (dispCnt != 0) {
                for (i = 0; i < 4; i++) {
                    if (own->disposal[i].battle.layers[0].cardId >= 0) {
                        work->cursorIndex = 3;
                        work->cursorZone = 4 - i;
                        break;
                    }
                }
            } else {
                for (i = 0; i < 4; i++) {
                    if (own->battle[i].battle.layers[0].cardId >= 0) {
                        work->cursorIndex = 2 - (i & 1);
                        work->cursorZone = 4 - (i & 2);
                        break;
                    }
                }
            }
        }
        CGPRotStageSub(work, side);
        *(s16 *)&work->cursor.unmodeled_02[0] = 1;
        work->cursor.selectionMode = 1;
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
        }
    case 1:
        {
        s32 mode;
        CardCursorPassive(&work->cursor);
        for (i = 0; i < 4; i++) {
            if (own->disposal[i].battle.layers[0].cardId >= 0) {
                D_00A57DD0.zone = dispZone;
                D_00A57DD0.index = i;
                CardCursorActive(&work->cursor, &D_00A57DD0.zone, 4);
            }
        }
        for (i = 0; i < 4; i++) {
            if (own->battle[i].battle.layers[0].cardId >= 0) {
                D_00A57DD0.zone = btlZone;
                D_00A57DD0.index = i;
                mode = 4;
                if (work->definitions[own->battle[i].battle.layers[0].cardId].flags & 4) {
                    if (CardPlayCntOpe(&work->player, 0x78) + CardPlayCntOpe(&work->enemy, 0x78) != 0) {
                        mode = 3;
                    }
                }
                CardCursorActive(&work->cursor, &D_00A57DD0.zone, mode);
            }
        }
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
        }
    case 2:
        {
        s16 *no = &D_00A57DC8;
        *no = CGPMoveSub(side, own, work);
        if (!(pressed & 0x20)) {
            return;
        }
        if (*(u16 *)&work->cursorMoveStep[0] != 0) {
            return;
        }
        if (*no >= 0) {
            s32 z;
            CGPCursor2Pos(&D_00A57DD0, work);
            z = D_00A57DD0.zone;
            if (dispZone == z) {
                xglSoundEffectNormalID(1, 0);
                CardCursorPassive(&work->cursor);
                CardCursorActive(&work->cursor, &D_00A57DD0.zone, 2);
                for (i = 0; i < 4; i++) {
                    pos1.index = i;
                    pos1.zone = btlZone;
                    CardCursorActive(&work->cursor, &pos1.zone, 1);
                }
            } else if (btlZone == z) {
                if (work->definitions[*no].flags & 4) {
                    if (CardPlayCntBtl(&work->player.hand, 10) != 0 ||
                        CardPlayCntBtl(&work->enemy.hand, 10) != 0) {
                        CGP_ERRPLUS(side, work, 0xF, 10);
                        xglSoundEffectNormalID(5, 0);
                        return;
                    }
                    if ((work->definitions[*no].flags & 4) &&
                        (CardPlayCntOpe(&work->player, 0x78) != 0 ||
                         CardPlayCntOpe(&work->enemy, 0x78) != 0)) {
                        CGP_ERRPLUS(side, work, 0xF, 0x78);
                        xglSoundEffectNormalID(5, 0);
                        return;
                    }
                }
                xglSoundEffectNormalID(1, 0);
                CardCursorPassive(&work->cursor);
                CardCursorActive(&work->cursor, &D_00A57DD0.zone, 2);
                for (i = 0; i < 4; i++) {
                    if (btlZone != D_00A57DD0.zone || i != D_00A57DD0.index) {
                        pos2.zone = btlZone;
                        pos2.index = i;
                        CardCursorActive(&work->cursor, &pos2.zone, 1);
                    }
                }
                for (i = 0; i < 4; i++) {
                    pos2.index = i;
                    pos2.zone = dispZone;
                    CardCursorActive(&work->cursor, &pos2.zone, 1);
                }
            } else {
                return;
            }
            work->command.phaseTimer = work->command.phaseTimer + 1;
            return;
        }
        CGPCursor2Pos(&pos2, work);
        if (handZone == pos2.zone) {
            CGPSetErrorMess(side, work, 0xB);
        }
        xglSoundEffectNormalID(5, 0);
        CGPCursor2Pos(&D_00A57DD0, work);
        return;
        }
    case 3:
        CGPMoveSub(side, own, work);
        if (pressed & 0x40) {
            work->command.phaseTimer = 1;
            xglSoundEffectNormalID(2, 0);
            CardCursorPassive(&work->cursor);
        }
        if (!(pressed & 0x20)) {
            return;
        }
        CGPCursor2Pos(&pos2, work);
        if (btlZone == pos2.zone) {
            if (dispZone == D_00A57DD0.zone) {
                if (pos2.cardId >= 0 &&
                    (CardPlayCntOpe(&work->player, 0x8A) != 0 ||
                     CardPlayCntOpe(&work->enemy, 0x8A) != 0)) {
                    CGP_ERRPLUS(side, work, 0xF, 0x8A);
                    xglSoundEffectNormalID(5, 0);
                    return;
                }
                if (work->definitions[pos2.cardId].flags & 4) {
                    if (CardPlayCntBtl(&work->player.hand, 10) != 0 ||
                        CardPlayCntBtl(&work->enemy.hand, 10) != 0) {
                        CGP_ERRPLUS(side, work, 0xF, 10);
                        xglSoundEffectNormalID(5, 0);
                        return;
                    }
                    if ((work->definitions[pos2.cardId].flags & 4) &&
                        (CardPlayCntOpe(&work->player, 0x78) != 0 ||
                         CardPlayCntOpe(&work->enemy, 0x78) != 0)) {
                        CGP_ERRPLUS(side, work, 0xF, 0x78);
                        xglSoundEffectNormalID(5, 0);
                        return;
                    }
                }
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[pos2.index].battle,
                                    (CardMoveWork *)&own->disposal[D_00A57DD0.index].battle);
                work->command.phaseTimer = work->command.phaseTimer + 1;
                return;
            } else {
                if (D_00A57DD0.index == pos2.index) {
                    break;
                }
                if (work->definitions[pos2.cardId].flags & 4) {
                    if (CardPlayCntBtl(&work->player.hand, 10) != 0 ||
                        CardPlayCntBtl(&work->enemy.hand, 10) != 0) {
                        CGP_ERRPLUS(side, work, 0xF, 10);
                        xglSoundEffectNormalID(5, 0);
                        return;
                    }
                    if ((work->definitions[pos2.cardId].flags & 4) &&
                        (CardPlayCntOpe(&work->player, 0x78) != 0 ||
                         CardPlayCntOpe(&work->enemy, 0x78) != 0)) {
                        CGP_ERRPLUS(side, work, 0xF, 0x78);
                        xglSoundEffectNormalID(5, 0);
                        return;
                    }
                }
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[pos2.index].battle,
                                    (CardMoveWork *)&own->battle[D_00A57DD0.index].battle);
                work->command.phaseTimer = work->command.phaseTimer + 1;
                return;
            }
        } else if (dispZone == pos2.zone) {
            if (CardPlayCntOpe(&work->player, 0x8A) != 0 ||
                CardPlayCntOpe(&work->enemy, 0x8A) != 0) {
                CGP_ERRPLUS(side, work, 0xF, 0x8A);
                xglSoundEffectNormalID(5, 0);
                return;
            }
            if (work->definitions[pos2.cardId].flags & 4) {
                if (CardPlayCntBtl(&work->player.hand, 10) != 0 ||
                    CardPlayCntBtl(&work->enemy.hand, 10) != 0) {
                    CGP_ERRPLUS(side, work, 0xF, 10);
                    xglSoundEffectNormalID(5, 0);
                    return;
                }
                if ((work->definitions[pos2.cardId].flags & 4) &&
                    (CardPlayCntOpe(&work->player, 0x78) != 0 ||
                     CardPlayCntOpe(&work->enemy, 0x78) != 0)) {
                    CGP_ERRPLUS(side, work, 0xF, 0x78);
                    xglSoundEffectNormalID(5, 0);
                    return;
                }
            }
            if (btlZone != D_00A57DD0.zone) {
                xglSoundEffectNormalID(5, 0);
                return;
            }
            CGPMoveDSBATTLEwork((CardMoveWork *)&own->disposal[pos2.index].battle,
                                (CardMoveWork *)&own->battle[D_00A57DD0.index].battle);
            work->command.phaseTimer = work->command.phaseTimer + 1;
            return;
        } else {
            xglSoundEffectNormalID(5, 0);
            return;
        }
    case 4:
        xglSoundEffectNormalID(1, 0);
        CardCursorPassive(&work->cursor);
        work->command.phaseTimer = 1;
        return;
    case 0x14:
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
    case 0x15:
        CGPNextTurnSub(work);
        break;
    }
}

extern s32 CardPlayBattleCnt(CardPlaySide *side);
extern void CardPlayTypeSort(CardDefinition *defs, s16 *cards, s32 count, s8 *order);
extern void CardPlayDesertBattle(CardHand *hand, CardLayerStack *stack);
extern void CGCPHandDispSub2(CardDefinition *defs, CardHand *hand, s32 index, s32 mode);
extern void CGCPHandDispSub3(CardDefinition *defs, CardHand *hand, s32 index, s32 mode, s32 flag);
extern void CGPSetErrorMessPlusI(s32 code, CardGameWork *work, u8 reason, s16 value) __asm__("CGPSetErrorMessPlus");
extern float BattlePos1P[][3];
extern CGPCursorPosition D_00A57DD8;
extern s32 D_00A57DE0;
extern char ZenkakuListB[];
extern const char D_00A4F068[];
extern const char D_00A4F088[];
#define HANDPOS(own, idx) (((CardPlayField *)(own))->handMove[idx].position)
#define DEF_OF_HAND(work, own, idx) (work->definitions[own->hand.cards[idx]])

void CGPSetFaseProc(s32 side, CardGameWork *work, CardPlaySide *own)
{
    s32 handZone;
    s32 dispZone;
    s32 btlZone;
    u16 pad;
    u8 buf[3];
    CGPCursorPosition pos;
    CGPCursorPosition pos2;
    CGPCursorPosition pos3;
    char tmp[3];
    float place[4][4];
    float tr[4];
    float vec[4];

    pad = work->command.input[0].pressed;
    handZone = 9;
    dispZone = 6;
    btlZone = 5;
    if (side != 0) {
        pad = work->command.input[1].pressed;
        handZone = 0;
        dispZone = 2;
        btlZone = 4;
    }
    switch ((s16)work->command.phaseTimer) {
    case 0:
        {
        s32 handCnt;
        handCnt = CardPlayHandCnt(&own->hand);
        if (side == 0) {
            work->cursorIndex = 0;
            work->cursorZone = 8;
            if (handCnt != 0) {
                work->cursorZone = 9;
            }
        } else {
            work->cursorIndex = 0;
            work->cursorZone = -1;
            if (handCnt != 0) {
                work->cursorZone = 0;
            }
        }
        CGPRotStageSub(work, side);
        *(s16 *)&work->cursor.unmodeled_02[0] = 1;
        work->cursor.selectionMode = 1;
        work->command.phaseTimer = work->command.phaseTimer + 1;
        buf[0] = 1;
        buf[1] = 4;
        buf[2] = 2;
        CardPlayTypeSort(work->definitions, own->hand.cards, handCnt, (s8 *)buf);
        return;
        }
    case 1:
        {
        s32 i;
        s32 j;
        s32 r;
        if (CardPlayBattleCnt(own) != 1) {
            for (i = 0; i < 4; i++) {
                if (own->battle[i].battle.layers[0].cardId == 0xe &&
                    *(s8 *)&own->battle[i].battle.layers[0].unmodeled_01 != 0) {
                    CGPSetErrorMessPlusI(side, work, 0x25, 0xe);
                    own->battle[i].battle.layers[0].unmodeled_01 = 0;
                }
            }
        }
        CardCursorPassive(&work->cursor);
        CGPCursorMove2(side, work);
        CGPCursor2Pos(&D_00A57DD8, work);
        work->cursor.selectionMode = 1;
        if (*(u16 *)&work->cursorMoveStep[0] != 0) {
            return;
        }
        if (handZone == D_00A57DD8.zone) {
            work->cursor.selectionMode = 0;
            r = CGPDisposeSub(side, work, D_00A57DD8.index);
            if (r == 0) {
            } else if (r == 0xf) {
                switch (D_00A57DD8.cardId) {
                case 7:
                case 9 ... 13:
                    work->command.phaseTimer = 0x14;
                    break;
                default:
                    D_00A57DE0 = 0;
                    for (j = 0; j < 4; j++) {
                        if (work->definitions[D_00A57DD8.cardId].unmodeled_04[j + 8] != 0) {
                            D_00A57DE0 = D_00A57DE0 + 1;
                        }
                    }
                    work->command.phaseTimer = 10;
                    break;
                }
            } else {
                if (D_00A57DD8.cardId == 0x34 &&
                    !(CardPlayCntBtl(&own->hand, 0xb) != 0 &&
                      (CardPlayCntBtl(&own->hand, 0) != 0 ||
                       CardPlayCntBtl(&own->hand, 1) != 0 ||
                       CardPlayCntBtl(&own->hand, 7) != 0))) {
                    CGPSetErrorMess(side, work, 2);
                    xglSoundEffectNormalID(5, 0);
                } else {
                    work->command.phaseTimer = work->command.phaseTimer + 1;
                }
            }
            if (side != 0) {
                if (work->rotStageId != 0) {
                    CGCPHandDispSub2(work->definitions, &own->hand, D_00A57DD8.index, 5);
                } else {
                    CGCPHandDispSub3(work->definitions, &own->hand, D_00A57DD8.index, 5, 0);
                }
            } else {
                if (work->rotStageId != 0) {
                    CGCPHandDispSub3(work->definitions, &own->hand, D_00A57DD8.index, 5, 0);
                } else {
                    CGCPHandDispSub2(work->definitions, &own->hand, D_00A57DD8.index, 5);
                }
            }
        } else if (pad & 0x20) {
            xglSoundEffectNormalID(5, 0);
        }
        return;
        }
    case 2:
        {
        s32 i;
        CardCursorPassive(&work->cursor);
        work->cursor.selectionMode = 1;
        if (D_00A57DD8.cardId == 0x34) {
            for (i = 0; i < 4; i++) {
                if (own->battle[i].battle.layers[0].cardId == 0xb) {
                    pos.zone = btlZone;
                    pos.index = i;
                    CardCursorActive(&work->cursor, &pos.zone, 1);
                }
            }
            for (i = 0; i < 4; i++) {
                if (own->disposal[i].battle.layers[0].cardId == 0xb) {
                    pos.zone = dispZone;
                    pos.index = i;
                    CardCursorActive(&work->cursor, &pos.zone, 1);
                }
            }
        } else {
            for (i = 0; i < 4; i++) {
                if (own->battle[i].battle.layers[0].cardId >= 0 &&
                    CGPWeaponEquipChk(work, D_00A57DD8.cardId,
                                      own->battle[i].battle.layers[0].cardId) != 0) {
                    pos.zone = btlZone;
                    pos.index = i;
                    CardCursorActive(&work->cursor, &pos.zone, 1);
                }
            }
            for (i = 0; i < 4; i++) {
                if (own->disposal[i].battle.layers[0].cardId >= 0 &&
                    CGPWeaponEquipChk(work, D_00A57DD8.cardId,
                                      own->disposal[i].battle.layers[0].cardId) != 0) {
                    pos.zone = dispZone;
                    pos.index = i;
                    CardCursorActive(&work->cursor, &pos.zone, 1);
                }
            }
        }
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
        }
    case 3:
        CGPCursorMove2(side, work);
        CGPCursor2Pos(&pos, work);
        if (pad & 0x20) {
            CGPDisposeWeaponSub(side, work, D_00A57DD8, pos);
            return;
        }
        if (pad & 0x40) {
            xglSoundEffectNormalID(2, 0);
            work->command.phaseTimer = 1;
        }
        return;
    case 10:
        work->cursor.selectionMode = 1;
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
    case 19:
        return;
    case 11:
        {
        s32 i;
        handZone = 6;
        dispZone = 5;
        if (side != 0) {
            handZone = 2;
            dispZone = 4;
        }
        if (own->hand.cards[D_00A57DD8.index] == 0x4e) {
            CardCursorPassive(&work->cursor);
            pos3.zone = handZone;
            for (i = 0; i < 4; i++) {
                pos3.index = i;
                CardCursorActive(&work->cursor, &pos3.zone, 1);
                if (own->disposal[i].battle.layers[0].cardId >= 0) {
                    own->disposal[i].battle.layers[0].life = 0;
                }
            }
            pos3.zone = dispZone;
            for (i = 0; i < 4; i++) {
                pos3.index = i;
                CardCursorActive(&work->cursor, &pos3.zone, 1);
                if (own->battle[i].battle.layers[0].cardId >= 0) {
                    own->battle[i].battle.layers[0].life = 0;
                }
            }
            work->command.phaseTimer = 0xd;
            return;
        }
        CGPCursorMove2(side, work);
        CGPCursor2Pos(&pos2, work);
        CardCursorPassive(&work->cursor);
        for (i = 0; i < 4; i++) {
            if (own->battle[i].battle.layers[0].cardId >= 0 &&
                (work->definitions[own->battle[i].battle.layers[0].cardId].flags & 1) != 0) {
                pos3.zone = dispZone;
                pos3.index = i;
                CardCursorActive(&work->cursor, &pos3.zone, 1);
            }
        }
        for (i = 0; i < 4; i++) {
            if (own->disposal[i].battle.layers[0].cardId >= 0 &&
                (own->disposal[i].battle.flags & 1) == 0 &&
                (work->definitions[own->disposal[i].battle.layers[0].cardId].flags & 1) != 0) {
                pos3.zone = handZone;
                pos3.index = i;
                CardCursorActive(&work->cursor, &pos3.zone, 1);
            }
        }
        tmp[0] = ZenkakuListB[D_00A57DE0 * 2];
        tmp[1] = ZenkakuListB[D_00A57DE0 * 2 + 1];
        tmp[2] = 0;
        xglFontPrint(0x8f, 0xd5, 0xfff8, D_00A4F068);
        xglFontPrint(0x16b, 0xd5, 0xfff8, tmp);
        vec[0] = 1.5f;
        vec[1] = vec[2] = vec[3] = 1.0f;
        tr[0] = tr[1] = 0.0f;
        tr[2] = 0.91f;
        tr[3] = 1.0f;
        xglMatrixUnit(place);
        xglMatrixTrans(place, (const float (*)[4])place, tr);
        xglMatrixScale(place, (const float (*)[4])place, vec);
        nmlModelSetPlace(place);
        nmlModelSetTexture((void *)0x01DB5800);
        nmlModelEntryCard((void *)0x01DAAC00);
        if (pad & 0x20) {
            if (pos2.cardId >= 0 && (work->definitions[pos2.cardId].flags & 1) != 0) {
                if (handZone == pos2.zone || (dispZone == pos2.zone && handZone == dispZone)) {
                    if ((own->disposal[pos2.index].battle.flags & 1) == 0) {
                        CardPlayDesertBattle(&own->hand, &own->disposal[pos2.index].battle);
                        xglSoundEffectNormalID(1, 0);
                        work->command.phaseTimer = work->command.phaseTimer + 1;
                    } else {
                        xglSoundEffectNormalID(5, 0);
                    }
                } else if (dispZone == pos2.zone) {
                    CardPlayDesertBattle(&own->hand, &own->battle[pos2.index].battle);
                    xglSoundEffectNormalID(1, 0);
                    work->command.phaseTimer = work->command.phaseTimer + 1;
                } else {
                    xglSoundEffectNormalID(5, 0);
                }
            } else {
                xglSoundEffectNormalID(5, 0);
            }
        } else if (pad & 0x40) {
            xglSoundEffectNormalID(5, 0);
        }
        return;
        }
    case 12:
        {
        s32 i;
        if (--D_00A57DE0 > 0) {
            work->command.phaseTimer = (s16)work->command.phaseTimer - 1;
            return;
        }
        handZone = 6;
        dispZone = 5;
        if (side != 0) {
            handZone = 2;
            dispZone = 4;
        }
        CardCursorPassive(&work->cursor);
        pos3.zone = handZone;
        for (i = 0; i < 4; i++) {
            if (own->disposal[i].battle.layers[0].cardId < 0) {
                pos3.index = i;
                CardCursorActive(&work->cursor, &pos3.zone, 1);
            }
        }
        pos3.zone = dispZone;
        for (i = 0; i < 4; i++) {
            if (own->battle[i].battle.layers[0].cardId < 0) {
                pos3.index = i;
                CardCursorActive(&work->cursor, &pos3.zone, 1);
            }
        }
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
        }
    case 13:
        {
        s32 n;
        s32 i;
        handZone = 6;
        dispZone = 5;
        if (side != 0) {
            handZone = 2;
            dispZone = 4;
        }
        CGPCursorMove2(side, work);
        CGPCursor2Pos(&pos2, work);
        xglFontPrint(0xa0, 0xd5, 0xfff8, D_00A4F088);
        vec[0] = 1.5f;
        vec[1] = vec[2] = vec[3] = 1.0f;
        xglMatrixUnit(place);
        xglMatrixScale(place, (const float (*)[4])place, vec);
        nmlModelSetPlace(place);
        nmlModelSetTexture((void *)0x01DB5800);
        nmlModelEntryCard((void *)0x01DAAC00);
        if ((pad & 0x20) == 0) {
            return;
        }
        if (pos2.cardId >= 0) {
            return;
        }
        if (handZone != pos2.zone && dispZone != pos2.zone) {
            return;
        }
        if (dispZone == pos2.zone) {
#define STP own->battle[pos2.index].battle
            STP.flags = 0;
            STP.power = DEF_OF_HAND(work, own, D_00A57DD8.index).basePower;
            STP.powerLimit = DEF_OF_HAND(work, own, D_00A57DD8.index).basePower;
            STP.layers[0].cardId = own->hand.cards[D_00A57DD8.index];
            STP.layers[0].life = DEF_OF_HAND(work, own, D_00A57DD8.index).operationDuration;
            STP.position[0] = HANDPOS(own, D_00A57DD8.index)[0];
            STP.position[1] = HANDPOS(own, D_00A57DD8.index)[1];
            STP.position[2] = 0.12f;
            STP.step[0] = (BattlePos1P[pos2.index][0] - HANDPOS(own, D_00A57DD8.index)[0]) / 10.0f;
            STP.step[1] = (BattlePos1P[pos2.index][1] - HANDPOS(own, D_00A57DD8.index)[1]) / 10.0f;
            STP.step[2] = 0.0f;
            STP.moveFrames = 10;
#undef STP
        } else {
#define STP own->disposal[pos2.index].battle
            STP.power = DEF_OF_HAND(work, own, D_00A57DD8.index).basePower;
            STP.powerLimit = DEF_OF_HAND(work, own, D_00A57DD8.index).basePower;
            STP.layers[0].cardId = own->hand.cards[D_00A57DD8.index];
            STP.layers[0].life = DEF_OF_HAND(work, own, D_00A57DD8.index).operationDuration;
            STP.position[0] = HANDPOS(own, D_00A57DD8.index)[0];
            STP.position[1] = HANDPOS(own, D_00A57DD8.index)[1];
            STP.position[2] = 0.12f;
            STP.step[0] = (DisposePos1P[pos2.index][0] - HANDPOS(own, D_00A57DD8.index)[0]) / 10.0f;
            STP.step[1] = (DisposePos1P[pos2.index][1] - HANDPOS(own, D_00A57DD8.index)[1]) / 10.0f;
            STP.step[2] = 0.0f;
            STP.moveFrames = 10;
#undef STP
        }
        n = DEF_OF_HAND(work, own, D_00A57DD8.index).reserveCost;
        own->hand.playPoints[0] -= DEF_OF_HAND(work, own, D_00A57DD8.index).playCost;
        own->hand.cards[D_00A57DD8.index] = -1;
        for (i = 0; i < n; i++) {
            CardPlayCostCard((CardPlayHand *)own);
        }
        CardPlayCleanHand((CardPlayHand *)own);
        CardCursorPassive(&work->cursor);
        xglSoundEffectNormalID(1, 0);
        work->command.phaseTimer = 1;
        return;
        }
    case 20:
        {
        s32 slot = 0;
        CardLayerStack *card = 0;
        s32 i;
        for (i = 0; i < 4; i++) {
            if (D_00A57DD8.cardId - 7 == own->battle[i].battle.layers[0].cardId) {
                slot = i;
                card = &own->battle[i].battle;
                break;
            }
        }
        if (card != 0) {
            s32 diff = card->layers[0].life -
                       work->definitions[card->layers[0].cardId].operationDuration;
            CardPlayDesertBattle(&own->hand, card);
            card->power = work->definitions[D_00A57DD8.cardId].basePower;
            card->powerLimit = work->definitions[D_00A57DD8.cardId].basePower;
            card->layers[0].cardId = D_00A57DD8.cardId;
            card->layers[0].life = work->definitions[D_00A57DD8.cardId].operationDuration + diff;
            card->position[0] = HANDPOS(own, D_00A57DD8.index)[0];
            card->position[1] = HANDPOS(own, D_00A57DD8.index)[1];
            card->position[2] = 0.12f;
            card->step[0] = (BattlePos1P[slot][0] - HANDPOS(own, D_00A57DD8.index)[0]) / 10.0f;
            card->step[1] = (BattlePos1P[slot][1] - HANDPOS(own, D_00A57DD8.index)[1]) / 10.0f;
            card->step[2] = 0.0f;
            card->moveFrames = 10;
            own->hand.playPoints[0] -= DEF_OF_HAND(work, own, D_00A57DD8.index).playCost;
            own->hand.cards[D_00A57DD8.index] = -1;
            CardPlayCleanHand((CardPlayHand *)own);
            xglSoundEffectNormalID(1, 0);
        }
        work->command.phaseTimer = 1;
        return;
        }
    }
}

extern void CCU063ExecSub(CardGameWork *work, CardPlaySide *side);
extern void CCU064ExecSub(CardGameWork *work, CardPlaySide *side);
extern void CCU065ExecSub(CardGameWork *work, CardPlaySide *side);
extern void CGCPHandDispSub(CardHand *hand, int sel, int side);
extern void CGCPYamaDispSub(CardPlaySide *side, s32 a, s32 b, s32 c, s32 d);
extern void CardPlayDesert2Card(CardPlaySide *side, s32 a, s32 b);
extern void CardPlayDrawSute(CardPlaySide *side);
extern int CardPlayDesertCard(CardHand *hand, int index);
extern void CardPlayDesertOperation(CardPlaySide *side, s32 a);
extern void CardPlayCleanYama(CardPlaySide *side);
extern void CardPlayShuffleYama(CardPlaySide *side);
extern s32 CardPlayYamaCnt(CardPlaySide *side);
extern void xglFontPrint(int x, int y, int color, const char *text);
extern CGPCursorPosition D_00A57DE8;
typedef struct CGPCFPair {
    s16 a;
    s16 b;
} CGPCFPair;
typedef struct CGPCFStackSlot {
    CardLayerStackView battle;
    float nextShunt[3];
} CGPCFStackSlot;
extern s16 D_00A57DF0[1];
extern s16 D_00A57DF2[1] __asm__("D_00A57DF0+2");
extern CGPCFPair D_00A57DF8;
extern s32 D_00A57E00;
extern s32 D_00A57E04;
extern s32 D_00A57E08;
extern char D_00A4F108[];
extern char D_00A4F128[];

s32 CGPCFPlayCommOperation(s16 side, CardGameWork *work, s16 comm)
{
    s32 ret = 0;
    CardPlaySide *own;
    CardPlaySide *foe;
    u16 held;
    u16 pressed;
    u16 repeat;
    float sA[4];
    float mA[4][4];
    float sB[4];
    float mB[4][4];
    CGPCursorPosition c80[2];
    CGPCursorPosition c70[2];
    CGPCursorPosition c60[2];

    if (side == 0) {
        own = &work->player;
        foe = &work->enemy;
        held = work->command.input[0].held;
        pressed = work->command.input[0].pressed;
        repeat = work->command.input[0].repeat;
    } else {
        own = &work->enemy;
        foe = &work->player;
        held = work->command.input[1].held;
        pressed = work->command.input[1].pressed;
        repeat = work->command.input[1].repeat;
    }

    switch (comm) {
    case 0x3F:
        switch (work->command.step) {
        case 0:
            work->command.step++;
            break;
        case 1:
            CCU063ExecSub(work, own);
            work->command.step++;
            break;
        case 2:
            ret = -1;
            break;
        }
        break;
    case 0x40:
        switch (work->command.step) {
        case 0:
            work->command.step++;
            break;
        case 1:
            CCU064ExecSub(work, own);
            work->command.step++;
            break;
        case 2:
            ret = -1;
            break;
        }
        break;
    case 0x41:
        switch (work->command.step) {
        case 0:
            work->command.step++;
            break;
        case 1:
            CCU065ExecSub(work, own);
            work->command.step++;
            break;
        case 2:
            ret = -1;
            break;
        }
        break;
    case 0x83: {
        s16 cnt = CardPlayHandCnt(&own->hand);

        switch (work->command.step) {
        case 0:
            D_00A57DF0[0] = 0;
            D_00A57DF2[0] = 0;
            if (cnt < 2) {
                CGPSetErrorMess(side, work, 0xD);
                ret = 1;
                xglSoundEffectNormalID(5, 0);
            } else {
                work->command.step++;
            }
            break;
        case 1:
            xglFontPrint(0x8F, 0xD5, 0xFFF8, D_00A4F108);
            sA[0] = 1.5f;
            sA[3] = 1.0f;
            sA[2] = 1.0f;
            sA[1] = 1.0f;
            xglMatrixUnit(mA);
            xglMatrixScale(mA, mA, sA);
            nmlModelSetPlace(mA);
            nmlModelSetTexture((void *)0x01DB5800);
            nmlModelEntryCard((void *)0x01DAAC00);
            if ((side != 0 && work->rotStageId == 0) || (side == 0 && work->rotStageId != 0)) {
                CGCPHandDispSub3(work->definitions, own, D_00A57DF0[0], 0, 0);
            } else {
                CGCPHandDispSub2(work->definitions, own, D_00A57DF0[0], 0);
            }
            if (repeat & 0x8000) {
                if (D_00A57DF0[0] > 0) {
                    xglSoundEffectNormalID(1, 0);
                    D_00A57DF0[0]--;
                }
            } else if (repeat & 0x2000) {
                if (D_00A57DF0[0] < cnt - 1) {
                    xglSoundEffectNormalID(1, 0);
                    D_00A57DF0[0]++;
                }
            }
            if (pressed & 0x20) {
                xglSoundEffectNormalID(1, 0);
                work->command.step++;
            }
            break;
        case 2:
            xglFontPrint(0x8F, 0xD5, 0xFFF8, D_00A4F128);
            sB[0] = 1.5f;
            sB[3] = 1.0f;
            sB[2] = 1.0f;
            sB[1] = 1.0f;
            xglMatrixUnit(mB);
            xglMatrixScale(mB, mB, sB);
            nmlModelSetPlace(mB);
            nmlModelSetTexture((void *)0x01DB5800);
            nmlModelEntryCard((void *)0x01DAAC00);
            if ((side != 0 && work->rotStageId == 0) || (side == 0 && work->rotStageId != 0)) {
                CGCPHandDispSub3(work->definitions, own, D_00A57DF2[0], 0, 0);
            } else {
                CGCPHandDispSub2(work->definitions, own, D_00A57DF2[0], 0);
            }
            if (repeat & 0x8000) {
                if (D_00A57DF2[0] > 0) {
                    xglSoundEffectNormalID(1, 0);
                    D_00A57DF2[0]--;
                }
            } else if (repeat & 0x2000) {
                if (D_00A57DF2[0] < cnt - 1) {
                    xglSoundEffectNormalID(1, 0);
                    D_00A57DF2[0]++;
                }
            }
            if (pressed & 0x20) {
                if (D_00A57DF0[0] == D_00A57DF2[0]) {
                    xglSoundEffectNormalID(5, 0);
                } else {
                    xglSoundEffectNormalID(1, 0);
                    work->command.step++;
                }
            }
            break;
        case 3:
            CGPDispErrorMess((CardGameWork *)(int)side, 0x18);
            if (pressed & 0x20) {
                xglSoundEffectNormalID(1, 0);
                work->command.step++;
            } else if (pressed & 0x40) {
                xglSoundEffectNormalID(2, 0);
                work->command.step = 1;
            }
            break;
        case 4:
            CardPlayDesert2Card(own, D_00A57DF0[0], D_00A57DF2[0]);
            ret = 1;
            CardPlayDrawSute(own);
            break;
        }
        break;
    }
    case 0x85:
        switch (work->command.step) {
        case 0: {
            s32 cnt = CardPlayHandCnt(&own->hand);
            s32 found = 0;
            s32 i;

            for (i = 0; i < cnt; i++) {
                if (work->definitions[own->hand.cards[i]].kind == 1 &&
                    (work->definitions[own->hand.cards[i]].flags & 4)) {
                    found = 1;
                    break;
                }
            }
            if (found == 0) {
                xglSoundEffectNormalID(2, 0);
                CGPSetErrorMess(side, work, 0xD);
                ret = 1;
            } else {
                D_00A57DF8.b = 0;
                work->command.step++;
            }
            break;
        }
        case 1: {
            s32 cnt;
            s32 flag;

            if (side == 0) {
                flag = work->rotStageId != 0;
            } else {
                flag = work->rotStageId == 0;
            }
            CGCPHandDispSub(&own->hand, D_00A57DF8.b, flag);
            cnt = CardPlayHandCnt(&own->hand);
            if (repeat & 0x2000) {
                if (D_00A57DF8.b < cnt - 1) {
                    xglSoundEffectNormalID(3, 0);
                    D_00A57DF8.b++;
                }
            } else if (repeat & 0x8000) {
                if (D_00A57DF8.b > 0) {
                    xglSoundEffectNormalID(3, 0);
                    D_00A57DF8.b--;
                }
            }
            if (pressed & 0x20) {
                if (work->definitions[own->hand.cards[D_00A57DF8.b]].flags & 4) {
                    D_00A57DF8.a = 9;
                    if (side != 0) {
                        D_00A57DF8.a = 0;
                    }
                    work->command.step++;
                    xglSoundEffectNormalID(1, 0);
                } else {
                    xglSoundEffectNormalID(5, 0);
                }
            }
            break;
        }
        case 2:
            CGPDispErrorMessPlus(side, 0x19, own->hand.cards[D_00A57DF8.b]);
            if (pressed & 0x20) {
                D_00A57E00 = 0;
                work->command.step++;
                CardPlayDesertCard(&own->hand, D_00A57DF8.b);
            }
            if (pressed & 0x40) {
                work->command.phaseTimer = (s16)work->command.phaseTimer - 1;
            }
            break;
        case 3: {
            s32 cnt = CardPlayYamaCnt(own);

            if (pressed & 0x20) {
                if (work->definitions[own->deck[D_00A57E00]].flags & 4) {
                    xglSoundEffectNormalID(1, 0);
                    work->command.step++;
                } else {
                    xglSoundEffectNormalID(5, 0);
                }
            }
            if (pressed & 0x40) {
                xglSoundEffectNormalID(2, 0);
                work->command.step = 0;
                ret = 1;
            }
            if (repeat & 0x2000) {
                D_00A57E00++;
                xglSoundEffectNormalID(3, 0);
                if (D_00A57E00 >= cnt) {
                    D_00A57E00 = cnt - 1;
                }
            }
            if (repeat & 0x8000) {
                D_00A57E00--;
                xglSoundEffectNormalID(3, 0);
                if (D_00A57E00 < 0) {
                    D_00A57E00 = 0;
                }
            }
            CGCPYamaDispSub(own, D_00A57E00, cnt, held, 0);
            break;
        }
        case 4:
            work->command.step++;
            break;
        case 5: {
            s32 cnt = CardPlayHandCnt(&own->hand);

            own->hand.cards[cnt] = own->deck[D_00A57E00];
            own->deck[D_00A57E00] = -1;
            CardPlayCleanYama(own);
            CardPlayShuffleYama(own);
            work->command.step++;
            break;
        }
        case 6:
            work->command.step = 10;
            break;
        case 10:
            ret = 1;
            break;
        }
        break;
    case 0x8B:
        switch (work->command.step) {
        case 0: {
            s32 i;

            c80[0].zone = 5;
            c70[0].zone = 4;
            if (side != 0) {
                c80[0].zone = 4;
                c70[0].zone = 5;
            }
            CardCursorPassive(&work->cursor);
            for (i = 0; i < 4; i++) {
                if (own->battle[i].battle.layers[0].cardId >= 0) {
                    c80[0].index = i;
                    CardCursorActive(&work->cursor, &c80[0].zone, 1);
                }
                if (foe->battle[i].battle.layers[0].cardId >= 0) {
                    c70[0].index = i;
                    CardCursorActive(&work->cursor, &c70[0].zone, 1);
                }
            }
            work->command.step++;
            break;
        }
        case 1:
            CGPCursorMove2(side, work);
            CGPCursor2Pos(&D_00A57DE8, work);
            if (pressed & 0x20) {
                if (D_00A57DE8.cardId < 0 || (D_00A57DE8.zone != 4 && D_00A57DE8.zone != 5)) {
                    xglSoundEffectNormalID(5, 0);
                } else {
                    xglSoundEffectNormalID(1, 0);
                    work->command.step++;
                }
            }
            break;
        case 2:
            CGPDispErrorMess((CardGameWork *)(int)side, 0x18);
            if (pressed & 0x20) {
                xglSoundEffectNormalID(1, 0);
                work->command.step++;
            } else if (pressed & 0x40) {
                xglSoundEffectNormalID(2, 0);
                work->command.step = 1;
            }
            break;
        case 3: {
            CGPCFStackSlot *tgt;
            s32 sel;

            if (D_00A57DE8.zone == 4) {
                tgt = (CGPCFStackSlot *)&work->enemy.battle[0].battle;
                sel = 1;
            } else if (D_00A57DE8.zone == 5) {
                tgt = (CGPCFStackSlot *)&work->player.battle[0].battle;
                sel = 0;
            } else {
                tgt = (CGPCFStackSlot *)&work->enemy.battle[0].battle;
                sel = 1;
            }
            tgt[D_00A57DE8.index].battle.layers[0].life -= 3;
            if (tgt[D_00A57DE8.index].battle.layers[0].life < 0) {
                tgt[D_00A57DE8.index].battle.layers[0].life = 0;
            }
            ret = -1;
            CardSetEffect(sel, work, 5, 0, 0x20008, tgt[D_00A57DE8.index].battle.position,
                          &tgt[D_00A57DE8.index].battle);
            CardSetEffect(sel, work, 0xD, 3, 0, tgt[D_00A57DE8.index].battle.position,
                          &tgt[D_00A57DE8.index].battle);
            break;
        }
        }
        break;
    case 0x8D: {
        s16 zone = 3;

        if (side != 0) {
            zone = 7;
        }
        switch (work->command.step) {
        case 0: {
            s32 i;

            CardCursorPassive(&work->cursor);
            c60[0].zone = zone;
            for (i = 0; i < 4; i++) {
                if (foe->operations[i] >= 0) {
                    c60[0].index = i;
                    CardCursorActive(&work->cursor, &c60[0].zone, 1);
                }
            }
            work->command.step++;
            break;
        }
        case 1:
            CGPCursorMove2(side, work);
            CGPCursor2Pos(&D_00A57DE8, work);
            if (pressed & 0x20) {
                if (D_00A57DE8.cardId < 0 || zone != D_00A57DE8.zone) {
                    xglSoundEffectNormalID(5, 0);
                } else {
                    work->command.step++;
                }
            }
            break;
        case 2:
            CGPDispErrorMess((CardGameWork *)(int)side, 0x18);
            if (pressed & 0x20) {
                xglSoundEffectNormalID(1, 0);
                work->command.step++;
            } else if (pressed & 0x40) {
                xglSoundEffectNormalID(2, 0);
                work->command.step = 1;
            }
            break;
        case 3:
            ret = -1;
            CardPlayDesertOperation(foe, D_00A57DE8.index);
            break;
        }
        break;
    }
    case 0x8E:
        switch (work->command.step) {
        case 0:
            work->command.step++;
            D_00A57E04 = 4;
            CardSetEffect(side, work, 5, 0, 0x20008, 0, 0);
            CardSetEffect(side, work, 0xD, 4, 0, 0, 0);
            break;
        case 1:
            CardPlayCostCard((CardPlayHand *)foe);
            work->command.step++;
            break;
        case 2:
            if (--D_00A57E04 != 0) {
                work->command.step--;
            } else {
                work->command.step++;
            }
            break;
        case 3:
            ret = -1;
            break;
        }
        break;
    case 0x8F:
        switch (work->command.step) {
        case 0:
            work->command.step++;
            D_00A57E08 = 10;
            break;
        case 1:
            if (D_00A57E08 != 0) {
                D_00A57E08--;
            } else {
                CCO143ExecSub(side, work, (void *)own, (void *)foe);
                D_00A57E08 = 15;
                work->command.step++;
            }
            break;
        case 2:
            if (D_00A57E08 != 0) {
                D_00A57E08--;
            } else {
                ret = -1;
            }
            break;
        }
        break;
    }
    return ret;
}

extern void xglMatrixRotZ(Matrix4 destination, const Matrix4 source, float angle);
extern void CardDispCard(float (*place)[4], s16 no, s16 flag);
extern void xglFontPrint(int x, int y, int color, const char *text);
extern char D_00A4F2F0[];
extern char D_00A4F308[];
extern char D_00A4F330[];
typedef struct CFSSide {
    u8 unmodeled_00[8];
    s16 pile[5][40];
} CFSSide;

void CGPCFSDispSub(s32 side, s32 turn, CardGameWork *work, s16 card, s16 unused)
{
    float sc[4];
    float vec[4];
    float base[4][4];
    float place[4][4];
    CFSSide *player;
    CFSSide *enemy;
    s32 found;
    s32 cnt;
    s32 i;
    float ofs;
    float f;
    float step;

    player = (CFSSide *)&work->player;
    enemy = (CFSSide *)&work->enemy;
    xglMatrixUnit(base);
    vec[0] = 0.0f;
    vec[1] = 0.0f;
    vec[2] = 0.1f;
    vec[3] = 1.0f;
    sc[1] = 0.75f;
    sc[0] = 1.0f;
    sc[2] = (sc[3] = 1.0f);
    step = 0.5663f;
    xglMatrixTrans(base, (const float (*)[4])base, vec);
    xglMatrixScale(place, (const float (*)[4])base, sc);
    if (turn == 0) {
        nmlModelSetLight(Player1Color, asDir);
    } else {
        nmlModelSetLight(Player2Color, asDir);
    }
    found = 0;
    nmlModelSetTexture((void *)0x01DB5800);
    nmlModelSetPlace(place);
    nmlModelEntryCard((void *)0x01DA5600);
    vec[0] = -1.9785f;
    vec[1] = 0.0f;
    vec[2] = 0.01f;
    vec[3] = 1.0f;
    sc[1] = 0.4f;
    sc[0] = 0.4f;
    sc[2] = (sc[3] = 1.0f);
    xglMatrixTrans(place, (const float (*)[4])base, vec);
    xglMatrixScale(place, (const float (*)[4])place, sc);
    CardDispCard(place, card, 1);
    if (turn == 0) {
        for (i = 0; i < 40; i++) {
            if (enemy->pile[4][i] >= 0) {
                found = 1;
                break;
            }
        }
    } else {
        for (i = 0; i < 40; i++) {
            if (player->pile[4][i] >= 0) {
                found = 1;
                break;
            }
        }
    }
    if (found == 0) {
        if (side == 0) {
            nmlModelSetLight(CLP1Color, asDir);
        } else {
            nmlModelSetLight(CLP2Color, asDir);
        }
        nmlModelSetPlace(place);
        nmlModelSetTexture(*(void **)((char *)work + 0x5BD4));
        nmlModelEntryCard((void *)0x01D12000);
        xglFontPrint(0x7C, 0x82, 0xFFF0, D_00A4F2F0);
    } else {
        xglFontPrint(0x7C, 0x82, 0xFFF0, D_00A4F308);
    }
    xglFontPrint(0x18, 0x127, 0xFFF0, D_00A4F330);

    sc[1] = 0.35f;
    sc[0] = 0.35f;
    sc[2] = (sc[3] = 1.0f);
    ofs = 0.0f;
    if (side == 0) {
        ofs = 0.28315f;
    }
    cnt = 0;
    for (i = 0; i < 40; i++) {
        if (player->pile[4][i] >= 0) {
            cnt++;
        }
    }
    vec[1] = -0.25f;
    if (work->rotStageId != 0) {
        vec[1] = -vec[1];
    }
    for (i = 0; i < cnt; i++) {
        f = (float)i;
        vec[0] = step * f + -1.4193f + ofs;
        vec[2] = f * 0.02f + 0.02f;
        xglMatrixTrans(place, (const float (*)[4])base, vec);
        xglMatrixScale(place, (const float (*)[4])place, sc);
        if (work->rotStageId != 0) {
            xglMatrixRotZ(place, (const float (*)[4])place, 3.1415927f);
        }
        CardDispCard(place, player->pile[4][i], 1);
    }
    if (cnt != 0 && turn != 0) {
        nmlModelSetLight(CLP1Color, asDir);
        nmlModelSetPlace(place);
        nmlModelSetTexture(*(void **)((char *)work + 0x5BD4));
        nmlModelEntryCard((void *)0x01D12000);
    }

    cnt = 0;
    for (i = 0; i < 40; i++) {
        if (enemy->pile[4][i] >= 0) {
            cnt++;
        }
    }
    ofs = 0.0f;
    if (side != 0) {
        ofs = 0.28315f;
    }
    vec[1] = 0.25f;
    if (work->rotStageId != 0) {
        vec[1] = -vec[1];
    }
    sc[1] = 0.35f;
    sc[0] = 0.35f;
    sc[2] = (sc[3] = 1.0f);
    for (i = 0; i < cnt; i++) {
        f = (float)i;
        vec[0] = step * f + -1.4193f + ofs;
        vec[2] = f * 0.02f + 0.02f;
        xglMatrixTrans(place, (const float (*)[4])base, vec);
        xglMatrixScale(place, (const float (*)[4])place, sc);
        if (work->rotStageId == 0) {
            xglMatrixRotZ(place, (const float (*)[4])place, 3.1415927f);
        }
        CardDispCard(place, enemy->pile[4][i], 1);
    }
    if (cnt != 0 && turn == 0) {
        nmlModelSetLight(CLP2Color, asDir);
        nmlModelSetPlace(place);
        nmlModelSetTexture(*(void **)((char *)work + 0x5BD4));
        nmlModelEntryCard((void *)0x01D12000);
    }
}

/*
 * Records the answered card (hand->cards[cardIndex]) in the first empty slot
 * of hand->answerCards, pays its cost by calling CardPlayCostCard that many
 * times, removes it from hand->cards and tail-calls CardPlayCleanHand.
 */
void CGPAnswerSub(CGPCardDefSource *src, CardPlayHand *hand, s32 cardIndex)
{
    s32 i;
    s32 cost;

    for (i = 0; i < 40; i++) {
        if (hand->answerCards[i] < 0) {
            break;
        }
    }
    hand->answerCards[i] = hand->cards[cardIndex];
    cost = src->defs[hand->cards[cardIndex]].cost;
    for (i = 0; i < cost; i++) {
        CardPlayCostCard(hand);
    }
    hand->cards[cardIndex] = -1;
    CardPlayCleanHand(hand);
}

extern s32 CardPlayCntDsp(CardPlaySide *side, s16 cardId);

/*
 * Commands one side's disposal slots play once its command phase ends. A slot
 * holding one of these cards whose stack carries flag 1 plays the command that
 * card ends the phase with: card 29 command 62, card 58 command 13, card 62
 * command 15, card 61 command 60, card 57 command 64 and card 55 command 11.
 */
void CGPCommFaseEndSub(s32 side, CardPlaySide *ownSide, CardGameWork *work)
{
    s32 i;

    if (CardPlayCntDsp(ownSide, 29) != 0) {
        for (i = 0; i < 4; i++) {
            if (ownSide->disposal[i].battle.layers[0].cardId == 29) {
                if (ownSide->disposal[i].battle.flags & 1) {
                    CardPlayCommandPlay(side, 62, work, 0, 0);
                }
            }
        }
    }
    if (CardPlayCntDsp(ownSide, 58) != 0) {
        for (i = 0; i < 4; i++) {
            if (ownSide->disposal[i].battle.layers[0].cardId == 58) {
                if (ownSide->disposal[i].battle.flags & 1) {
                    CardPlayCommandPlay(side, 13, work, 0, 0);
                }
            }
        }
    }
    if (CardPlayCntDsp(ownSide, 62) != 0) {
        for (i = 0; i < 4; i++) {
            if (ownSide->disposal[i].battle.layers[0].cardId == 62) {
                if (ownSide->disposal[i].battle.flags & 1) {
                    CardPlayCommandPlay(side, 15, work, 0, 0);
                }
            }
        }
    }
    if (CardPlayCntDsp(ownSide, 61) != 0) {
        for (i = 0; i < 4; i++) {
            if (ownSide->disposal[i].battle.layers[0].cardId == 61) {
                if (ownSide->disposal[i].battle.flags & 1) {
                    CardPlayCommandPlay(side, 60, work, 0, 0);
                }
            }
        }
    }
    if (CardPlayCntDsp(ownSide, 57) != 0) {
        for (i = 0; i < 4; i++) {
            if (ownSide->disposal[i].battle.layers[0].cardId == 57) {
                if (ownSide->disposal[i].battle.flags & 1) {
                    CardPlayCommandPlay(side, 64, work, 0, 0);
                }
            }
        }
    }
    if (CardPlayCntDsp(ownSide, 55) != 0) {
        for (i = 0; i < 4; i++) {
            if (ownSide->disposal[i].battle.layers[0].cardId == 55) {
                if (ownSide->disposal[i].battle.flags & 1) {
                    CardPlayCommandPlay(side, 11, work, 0, 0);
                }
            }
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ov10/cgp", CGPCommFaseSub);

static inline s32 CGPEndFaseChkGuno(CardPlaySide *s, CardGameWork *work)
{
    s32 found = 0;
    s32 j;

    j = 0;
    do {
        s16 id = (&s->disposal[0].battle.layers[0].cardId)[j * 0xDE];
        if (id >= 0 && (work->definitions[id].flags & 4)) {
            found = 1;
            break;
        }
        j++;
    } while (j < 4);
    j = 0;
    do {
        s16 id = (&s->battle[0].battle.layers[0].cardId)[j * 0xDE];
        if (id >= 0 && (work->definitions[id].flags & 4)) {
            found = 1;
            break;
        }
        j++;
    } while (j < 4);
    return found;
}

int CGPEndFaseSub(s32 side, CardHand *own, CardHand *other, CardGameWork *work)
{
    CardPlaySide *a = (CardPlaySide *)own;
    CardPlaySide *b = (CardPlaySide *)other;
    s32 i;
    s32 enemy = side == 0;

    for (i = 0; i < 4; i++) {
        a->battle[i].battle.layers[0].state = 0;
        a->battle[i].battle.layers[0].unmodeled_01 = 0;
        a->battle[i].battle.flags &= 0xFD;
        a->battle[i].battle.layers[0].boostCount = 0;
    }
    for (i = 0; i < 4; i++) {
        a->disposal[i].battle.layers[0].state = 0;
        a->disposal[i].battle.layers[0].unmodeled_01 = 0;
        a->disposal[i].battle.flags &= 0xFD;
        a->disposal[i].battle.layers[0].boostCount = 0;
    }
    if (work->command.unmodeled_0A) {
        work->command.unmodeled_0A--;
    }
    if (work->command.recoveryApplied) {
        work->command.recoveryApplied--;
    }

    for (i = 0; i < 4; i++) {
        if (a->operations[i] >= 0) {
            if (a->operationDuration[i] < 0x40) {
                if (a->operations[i] == 0x84) {
                    if (CGPEndFaseChkGuno(a, work)) {
                        a->operationDuration[i] = work->definitions[0x84].operationDuration;
                    } else {
                        a->operationDuration[i]--;
                    }
                } else {
                    a->operationDuration[i]--;
                }
            }
            if (a->operationDuration[i] <= 0) {
                a->junk[CardPlayJunkCnt((CardPlayHand *)a)] = a->operations[i];
                a->operations[i] = -1;
            }
        }
        if (b->operations[i] >= 0) {
            if (b->operationDuration[i] < 0x40) {
                if (a->operations[i] == 0x84) {
                    if (CGPEndFaseChkGuno(b, work)) {
                        b->operationDuration[i] = work->definitions[0x84].operationDuration;
                    } else {
                        b->operationDuration[i]--;
                    }
                } else {
                    b->operationDuration[i]--;
                }
            }
            if (b->operationDuration[i] <= 0) {
                b->junk[CardPlayJunkCnt((CardPlayHand *)b)] = b->operations[i];
                b->operations[i] = -1;
            }
        }
    }

    i = CardPlayCntOpe(a, 0x80);
    if (i != 0) {
        for (; i > 0; i--) {
            CardPlayRecavery(own);
        }
    }

    if (CardPlayCntOpe(a, 0x88) + CardPlayCntOpe(b, 0x88) != 0) {
        for (i = 0; i < 4; i++) {
            s16 id = a->disposal[i].battle.layers[0].cardId;
            if (id >= 0 && (work->definitions[id].flags & 4)) {
                a->disposal[i].battle.layers[0].life += 1;
                if (work->definitions[id].operationDuration < a->disposal[i].battle.layers[0].life) {
                    a->disposal[i].battle.layers[0].life = work->definitions[id].operationDuration;
                }
                CardSetEffect(side, work, 0xA, 0, 0, a->disposal[i].battle.position, 0);
                CardSetEffect(side, work, 0xC, 1, 0x2000D, a->disposal[i].battle.position,
                              &a->disposal[i].battle);
            }
        }
    }

    for (i = 0; i < 4; i++) {
        s32 id = a->disposal[i].battle.layers[0].cardId;
        if (id >= 0) {
            switch ((s16)id) {
            case 6:
                a->disposal[i].battle.layers[0].life += 2;
                if (work->definitions[6].operationDuration < a->disposal[i].battle.layers[0].life) {
                    a->disposal[i].battle.layers[0].life = work->definitions[6].operationDuration;
                }
                CardSetEffect(side, work, 0xA, 0, 0, a->disposal[i].battle.position, 0);
                CardSetEffect(side, work, 0xC, 2, 0x2000D, a->disposal[i].battle.position,
                              &a->disposal[i].battle);
                break;
            case 13:
                a->disposal[i].battle.layers[0].life += 3;
                if (work->definitions[13].operationDuration < a->disposal[i].battle.layers[0].life) {
                    a->disposal[i].battle.layers[0].life = work->definitions[13].operationDuration;
                }
                CardSetEffect(side, work, 0xA, 0, 0, a->disposal[i].battle.position, 0);
                CardSetEffect(side, work, 0xC, 3, 0x2000D, a->disposal[i].battle.position,
                              &a->disposal[i].battle);
                break;
            }
            if (CardPlayCntOpe(a, 0x7F) && !(work->definitions[id].flags & 4)) {
                a->disposal[i].battle.layers[0].life += 1;
                if (work->definitions[id].operationDuration < a->disposal[i].battle.layers[0].life) {
                    a->disposal[i].battle.layers[0].life = work->definitions[id].operationDuration;
                }
                CardSetEffect(side, work, 0xA, 0, 0, a->disposal[i].battle.position, 0);
                CardSetEffect(side, work, 0xC, 1, 0x2000D, a->disposal[i].battle.position,
                              &a->disposal[i].battle);
            }
        }
    }

    for (i = 0; i < 4; i++) {
        s32 id = a->battle[i].battle.layers[0].cardId;
        if (id >= 0) {
            switch ((s16)id) {
            case 6:
                a->battle[i].battle.layers[0].life += 2;
                if (work->definitions[6].operationDuration < a->battle[i].battle.layers[0].life) {
                    a->battle[i].battle.layers[0].life = work->definitions[6].operationDuration;
                }
                CardSetEffect(side, work, 0xA, 0, 0, a->battle[i].battle.position, 0);
                CardSetEffect(side, work, 0xC, 2, 0x2000D, a->battle[i].battle.position,
                              &a->battle[i].battle);
                break;
            case 13:
            case 0x4D:
                a->battle[i].battle.layers[0].life += 3;
                if (work->definitions[id].operationDuration < a->battle[i].battle.layers[0].life) {
                    a->battle[i].battle.layers[0].life = work->definitions[id].operationDuration;
                }
                CardSetEffect(side, work, 0xA, 0, 0, a->battle[i].battle.position, 0);
                CardSetEffect(side, work, 0xC, 3, 0x2000D, a->battle[i].battle.position,
                              &a->battle[i].battle);
                break;
            }
        }
        {
            s16 id2 = b->battle[i].battle.layers[0].cardId;
            if (id2 >= 0) {
                if (id2 == 0x36) {
                    CardPlayCommandPlay(enemy, 0x4D, work, i, 0);
                }
            }
        }
    }
}

extern s32 D_00A57E20;
extern s32 D_00A57E24;
extern u8 D_00A57E28;
extern s8 D_00A57E29;
extern s32 CardPlayBattleCnt(CardPlaySide *side);
extern void CGPSetErrorMessPlusI(s32 code, CardGameWork *work, u8 reason, s16 value) __asm__("CGPSetErrorMessPlus");

void CGPBattleFaseProc(s32 side, CardGameWork *work)
{
    CardPlaySide *other;
    CardPlaySide *own;
    CardPlaySide *tmp;
    s32 cnt;
    s32 btl;
    s32 found;
    s32 i;
    s32 type;
    s16 *queue;

    own = &work->player;
    other = &work->enemy;
    tmp = own;
    if (side != 0) {
        own = other;
        other = tmp;
    }
    switch ((s16) work->command.phaseTimer) {
    case 0:
        CardPlayClearEndflg(own);
        cnt = CardPlayBattleCnt(own);
        btl = CardPlayCntBtl(&other->hand, 5);
        btl += CardPlayCntBtl(&other->hand, 0xC);
        if (cnt == 0 && btl == 0) {
            work->command.phaseTimer = 5;
            if (side == 0 && (work->flags & 0x40)) return;
            if (side != 0 && (work->flags & 0x20)) return;
            if (side == 0 && (work->save->flags & 8)) return;
            if (side != 0 && (work->save->flags & 4)) return;
            CGPSetErrorMess(side, work, 6);
            return;
        }
        if (work->command.recoveryApplied != 0) {
            work->command.phaseTimer = 5;
            if ((side == 0) ? (work->flags & 0x40) : (work->flags & 0x20)) return;
            CGPSetErrorMessPlusI(side, work, 0x14, 0x5C);
            return;
        }
        if (work->command.unmodeled_0A != 0 || CardPlayCntOpe(own, 0x7D) != 0 ||
            CardPlayCntOpe(other, 0x7D) != 0) {
            work->command.phaseTimer = 5;
            CGPSetErrorMessPlusI(side, work, 0x14, 0x7D);
            return;
        }
        CGPRotStageSub(work, side);
        D_00A57E20 = 0;
        work->command.phaseTimer = 1;
        return;
    case 1:
        queue = work->commandQueue[0][0];
        if (side != 0) {
            queue = work->commandQueue[1][0];
        }
        if (CardPlayCommandCheck(work->command.phase, queue) != 0) {
            work->cursor.selectionMode = 1;
            CardPlayCommandBattleExecute(side, work->command.phase, work);
            work->cursor.selectionMode = 1;
        } else {
            work->command.slideDirection = 1;
            work->cursor.selectionMode = 0;
            work->command.phaseTimer = 0xA;
            work->command.slideTimer = 0xA;
            work->command.step = 0;
        }
        CGPRefreshFieldHP(work);
        return;
    case 5:
        queue = work->commandQueue[0][0];
        if (side != 0) {
            queue = work->commandQueue[1][0];
        }
        if (CardPlayCommandCheck(work->command.phase, queue) != 0) {
            work->cursor.selectionMode = 1;
            CardPlayCommandBattleExecute(side, work->command.phase, work);
        } else {
            CGPNextTurnSub(work);
        }
        CGPRefreshFieldHP(work);
        return;
    case 0xA:
        if (CGPBattleSub(side, 1, D_00A57E20, work, own, other, 0) != 0) {
            return;
        }
        type = CardChkAttackType(work->definitions, (CardPowerCard *) &own->battle[D_00A57E20].battle);
        D_00A57E28 = 0;
        if (type == 1) {
            D_00A57E28 = 0x1E;
        }
        D_00A57E20 = D_00A57E20 + 1;
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
    case 0xB:
        if (D_00A57E28 != 0) {
            D_00A57E28 = D_00A57E28 - 1;
            return;
        }
        CGPRefreshFieldHP(work);
        CardCursorPassive(&work->cursor);
        work->command.step = 0;
        if (D_00A57E20 >= 4) {
            work->command.phaseTimer = 0x14;
            D_00A57E20 = 0;
        } else {
            work->command.phaseTimer = (s16) work->command.phaseTimer - 1;
        }
        return;
    case 0x14:
        if (CGPBattleSub(side, 2, D_00A57E20, work, own, other, 0) != 0) {
            return;
        }
        type = CardChkAttackType(work->definitions, (CardPowerCard *) &own->battle[D_00A57E20].battle);
        D_00A57E28 = 0;
        if (type == 2) {
            D_00A57E28 = 0x1E;
        }
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
    case 0x15:
        if (D_00A57E28 != 0) {
            D_00A57E28 = D_00A57E28 - 1;
            return;
        }
        CGPRefreshFieldHP(work);
        CardCursorPassive(&work->cursor);
        work->command.step = 0;
        if (own->battle[D_00A57E20].battle.layers[0].cardId == 0x4C) {
            queue = work->commandQueue[0][0];
            if (side != 0) {
                queue = work->commandQueue[1][0];
            }
            if (CardPlayCommandCheck(work->command.phase, queue) != 0) {
                CardPlayCommandBattleExecute(side, work->command.phase, work);
            }
            work->command.phaseTimer = work->command.phaseTimer + 1;
            D_00A57E28 = 0x1E;
            D_00A57E20 = D_00A57E20 + 1;
            return;
        }
        D_00A57E20 = D_00A57E20 + 1;
        if (D_00A57E20 >= 4) {
            work->command.phaseTimer = 0x1E;
            D_00A57E20 = 0;
        } else {
            work->command.phaseTimer = (s16) work->command.phaseTimer - 1;
        }
        return;
    case 0x16:
        if (D_00A57E28 != 0) {
            D_00A57E28 = D_00A57E28 - 1;
        } else {
            CardCursorPassive(&work->cursor);
            work->command.step = 0;
            if (D_00A57E20 >= 4) {
                work->command.phaseTimer = 0x1E;
                D_00A57E20 = 0;
            } else {
                work->command.phaseTimer = 0x14;
            }
        }
        CGPRefreshFieldHP(work);
        return;
    case 0x1E:
        found = 0;
        if (CardPlayCntBtl(&other->hand, 5) != 0 || CardPlayCntBtl(&other->hand, 0xC) != 0) {
            for (i = 0; i < 4; i++) {
                if ((other->battle[i].battle.layers[0].cardId == 5 ||
                     other->battle[i].battle.layers[0].cardId == 0xC) &&
                    other->battle[i].battle.layers[1].cardId < 0) {
                    found = 1;
                    D_00A57E20 = i;
                    work->command.phaseTimer = work->command.phaseTimer + 1;
                }
            }
        }
        if (found) {
            return;
        }
        work->command.phaseTimer = 0x28;
        return;
    case 0x1F:
        if (CGPBattleSub(side == 0, 2, D_00A57E20, work, other, own, 0) != 0) {
            return;
        }
        D_00A57E28 = 0x1E;
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
    case 0x20:
        if (D_00A57E28 != 0) {
            D_00A57E28 = D_00A57E28 - 1;
            return;
        }
        CGPRefreshFieldHP(work);
        CardCursorPassive(&work->cursor);
        D_00A57E20 = 0;
        D_00A57E24 = 0;
        work->command.step = 0;
        work->command.phaseTimer = 0x28;
        return;
    case 0x28:
        D_00A57E29 = 0;
        if (own->battle[D_00A57E20].battle.layers[0].cardId >= 0 &&
            CardChkAttackType(work->definitions, (CardPowerCard *) &own->battle[D_00A57E20].battle) == 3 &&
            !(own->battle[D_00A57E20].battle.flags & 2)) {
            own->battle[D_00A57E20].battle.power = (s8) own->battle[D_00A57E20].battle.power - 1;
            found = CardPlayBattleCnt(other);
            if ((s8) own->battle[D_00A57E20].battle.power == 0 && found != 0) {
                found = CGPCalcPower(work, (CardLayerStack *) &own->battle[D_00A57E20].battle);
                found += CGPCalcPowerPlus(work, (CardPowerCard *) &own->battle[D_00A57E20].battle, found);
                if (found >= 3) {
                    D_00A57E28 = 0x2D;
                    CardSetEffect(side, work, 0x21, 0, 0x20006, own->battle[D_00A57E20].battle.position, 0);
                } else {
                    D_00A57E28 = 0x1E;
                    CardSetEffect(side, work, 0x17, 0, 0x20006, own->battle[D_00A57E20].battle.position, 0);
                }
            }
        }
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
    case 0x29:
        if (D_00A57E28 != 0) {
            D_00A57E28 = D_00A57E28 - 1;
            return;
        }
        if (CGPBattleSub(side, 3, D_00A57E20, work, own, other,
                         (CardLayerStack *) &other->battle[D_00A57E24].battle) != 0) {
            return;
        }
        type = CardChkAttackType(work->definitions, (CardPowerCard *) &own->battle[D_00A57E20].battle);
        D_00A57E28 = 0;
        if (type == 3 &&
            other->battle[D_00A57E24].battle.layers[0].cardId >= 0 &&
            (s8) own->battle[D_00A57E20].battle.power == 0) {
            D_00A57E28 = 0x1E;
        }
        work->command.phaseTimer = work->command.phaseTimer + 1;
        D_00A57E24 = D_00A57E24 + 1;
        return;
    case 0x2A:
        if (D_00A57E28 != 0) {
            D_00A57E28 = D_00A57E28 - 1;
            return;
        }
        work->command.step = 0;
        if (D_00A57E24 >= 4) {
            CardCursorPassive(&work->cursor);
            if (own->battle[D_00A57E20].battle.layers[0].cardId >= 0 &&
                CardChkAttackType(work->definitions, (CardPowerCard *) &own->battle[D_00A57E20].battle) == 3) {
                if (own->battle[D_00A57E20].battle.layers[1].cardId >= 0 &&
                    own->battle[D_00A57E20].battle.layers[1].life < 0x50 &&
                    !(own->battle[D_00A57E20].battle.flags & 2)) {
                    own->battle[D_00A57E20].battle.layers[1].life--;
                }
                if ((s8) own->battle[D_00A57E20].battle.power <= 0) {
                    own->battle[D_00A57E20].battle.power = own->battle[D_00A57E20].battle.powerLimit;
                }
            }
            D_00A57E24 = 0;
            if (own->battle[D_00A57E20].battle.layers[0].cardId == 0x20 &&
                !(own->battle[D_00A57E20].battle.flags & 2)) {
                CardPlayCommandPlay(side, 0x49, work, D_00A57E20, 0);
                work->command.phaseTimer = work->command.phaseTimer + 1;
                D_00A57E28 = 0x1E;
            } else {
                D_00A57E20 = D_00A57E20 + 1;
                if (D_00A57E20 >= 4) {
                    work->command.phaseTimer = 0x32;
                    D_00A57E20 = 0;
                } else {
                    work->command.phaseTimer = 0x28;
                }
            }
        } else {
            work->command.phaseTimer = (s16) work->command.phaseTimer - 1;
        }
        CGPRefreshFieldHP(work);
        return;
    case 0x2B:
        queue = work->commandQueue[0][0];
        if (side != 0) {
            queue = work->commandQueue[1][0];
        }
        if (CardPlayCommandCheck(work->command.phase, queue) != 0) {
            CardPlayCommandBattleExecute(side, work->command.phase, work);
            return;
        }
        if (D_00A57E28 != 0) {
            D_00A57E28 = D_00A57E28 - 1;
        } else {
            D_00A57E20 = D_00A57E20 + 1;
            if (D_00A57E20 >= 4) {
                work->command.phaseTimer = 0x32;
                D_00A57E20 = 0;
            } else {
                work->command.phaseTimer = 0x28;
            }
        }
        CGPRefreshFieldHP(work);
        return;
    case 0x32:
        if (CGPBattleSub(side, 4, D_00A57E20, work, own, other, 0) != 0) {
            return;
        }
        type = CardChkAttackType(work->definitions, (CardPowerCard *) &own->battle[D_00A57E20].battle);
        D_00A57E28 = 0;
        if (type == 4) {
            D_00A57E28 = 0x1E;
        }
        D_00A57E20 = D_00A57E20 + 1;
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
    case 0x33:
        if (D_00A57E28 != 0) {
            D_00A57E28 = D_00A57E28 - 1;
            return;
        }
        CardCursorPassive(&work->cursor);
        work->command.step = 0;
        if (D_00A57E20 >= 4) {
            D_00A57E20 = 0;
            work->command.phaseTimer = work->command.phaseTimer + 1;
        } else {
            work->command.phaseTimer = (s16) work->command.phaseTimer - 1;
        }
        CGPRefreshFieldHP(work);
        return;
    case 0x34:
        work->command.slideTimer = 0xA;
        work->command.phaseTimer = 0x3C;
        work->command.slideDirection = 0;
        if (side == 0 && (work->flags & 0x40)) return;
        if (side != 0 && (work->flags & 0x20)) return;
        if (side == 0 && (work->save->flags & 8)) return;
        if (side != 0 && (work->save->flags & 4)) return;
        CGPSetErrorMess(side, work, 0x15);
        return;
    case 0x3C:
        CGPRefreshFieldHP(work);
        D_00A57E28 = 0xA;
        work->command.phaseTimer = work->command.phaseTimer + 1;
        return;
    case 0x3D:
        CGPRefreshFieldHP(work);
        if (D_00A57E28 != 0) {
            D_00A57E28 = D_00A57E28 - 1;
        } else {
            work->command.phaseTimer = 5;
        }
        return;
    }
}

extern int CGPEndFaseSub(s32 side, CardHand *own, CardHand *other, CardGameWork *work);
extern int CardPlayCommandCheck(s16 command, s16 *queue);
extern int CardPlayCommandExecute(s16 command, CardGameWork *work);
extern void CGCPHandDispSub(CardHand *hand, int sel, int side);
extern int CardPlayDesertCard(CardHand *hand, int index);
extern void CardEnemyEnd(int side, CardGameWork *work);
extern s32 D_00A57E2C;
extern s32 D_00A57E30;
static inline s32 CGPEndFaseLastIndex(CardHand *hand)
{
    return CardPlayHandCnt(hand) - 1;
}

void CGPEndFaseProc(s32 arg0, CardGameWork *arg1, CardTeMaxList *arg2)
{
    float vec[4]; /* matrix trans vector */
    float mat[4][4];
    CardHand *own;
    CardHand *other;
    u16 held;
    u16 repeat;
    s32 timer;
    u16 raw;
    s32 temp_3;
    s32 temp_2_2;
    s32 temp_16;
    s32 temp_0;
    s32 n;
    s32 rotOk;
    s32 flag;
    u8 rot;

    own = &arg1->player.hand;
    other = &arg1->enemy.hand;
    held = arg1->command.input[0].held;
    repeat = arg1->command.input[0].repeat;
    if (arg0) {
        held = arg1->command.input[1].held;
        own = &arg1->enemy.hand;
        repeat = arg1->command.input[1].repeat;
        other = &arg1->player.hand;
    }
    temp_16 = CardPlayCommandCheck(arg1->command.phase, (s16 *) arg1->commandQueue[0]);
    if ((temp_16 + CardPlayCommandCheck(arg1->command.phase, (s16 *) arg1->commandQueue[1])) != 0) {
        CardPlayCommandExecute((s16) arg1->command.phase, arg1);
    } else {
        raw = arg1->command.phaseTimer;
        timer = raw;
        switch ((s16) raw) {
        case 0:
            temp_0 = timer + 1;
            arg1->cursor.selectionMode = 0;
            arg1->command.phaseTimer = temp_0;
            timer = temp_0;
            /* fallthrough */
        case 1:
            arg1->command.phaseTimer = timer + 1;
            CGPEndFaseSub(arg0, own, other, arg1);
            break;
        case 2:
            if (CGPCheckTeMax(arg2, own) == 0) {
                arg1->command.phaseTimer = 0xA;
            } else {
                if ((arg0 == 0) ? (arg1->flags & 0x40) : (arg1->flags & 0x20)) {
                    CardEnemyEnd(arg0, arg1);
                } else {
                    xglSoundEffectNormalID(5, 0);
                    CGPSetInterruptMess(arg1, 0x2B);
                    CGPRotStageSub(arg1, (u32) arg0);
                    arg1->command.phaseTimer += 1;
                    D_00A57E2C = 0;
                    D_00A57E30 = 0x19;
                }
            }
            break;
        case 3:
            if (D_00A57E30 != 0) {
                D_00A57E30 -= 1;
            } else {
                s32 rotOk;

                if (arg0 == 0) {
                    rotOk = arg1->rotStageId != 0;
                } else {
                    rotOk = arg1->rotStageId == 0;
                }
                CGCPHandDispSub(own, D_00A57E2C, rotOk);
                temp_2_2 = CardPlayHandCnt(own);
                flag = repeat & 0x2000;
                if (flag == 0) {
                    goto noRight;
                }
                if (temp_2_2 - 1 > D_00A57E2C) {
                    xglSoundEffectNormalID(3, 0);
                    ++D_00A57E2C;
                }
                goto afterRepeat;
noRight:
                if ((repeat & 0x8000) && D_00A57E2C > 0) {
                    xglSoundEffectNormalID(3, 0);
                    D_00A57E2C = D_00A57E2C - 1;
                }
afterRepeat:
                if ((held & 0x10) != 0) {
                    vec[0] = 0.0f;
                    vec[1] = 0.369f;
                    vec[2] = 0.3f;
                    vec[3] = 1.0f;
                    xglMatrixUnit(mat);
                    xglMatrixTrans(mat, mat, vec);
                    CardDisp(mat, own->cards[D_00A57E2C], 1, 5);
                }
                if (repeat & 0x20) {
                    xglSoundEffectNormalID(1, 0);
                    arg1->command.phaseTimer += 1;
                }
            }
            break;
        case 4:
            if (arg0 == 0) {
                rotOk = arg1->rotStageId != 0;
            } else {
                rotOk = arg1->rotStageId == 0;
            }
            CGPDispErrorMess2((CardGameWork *) arg0, 0x19);
            CGCPHandDispSub(own, D_00A57E2C, rotOk);
            if (repeat & 0x20) {
                arg1->command.phaseTimer = 2;
                CardPlayDesertCard(own, D_00A57E2C);
                xglSoundEffectNormalID(1, 0);
            } else if (repeat & 0x40) {
                arg1->command.phaseTimer = (s16) arg1->command.phaseTimer - 1;
                xglSoundEffectNormalID(2, 0);
            }
            break;
        case 10:
            CGPNextTurnSub(arg1);
            break;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ov10/cgp", CardGameProc);

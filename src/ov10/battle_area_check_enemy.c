/*
 * OV10 original TU 10: 0x00a34190..0x00a42034 (89 functions)
 */
#include "common.h"
#include "ov10/cgp.h"

typedef signed int s32;

/* Occupied battle slots on the side opposite `side` (four slots, 0xDE
 * shorts/slot, `side->battle[0].battle.layers[0].cardId` and on). */
s32 BattleAreaCheckEnemy(s32 side, CardGameWork *work)
{
    CardPlaySide *otherSide = &work->enemy;
    s16 *cardId;
    s32 count = 0;
    s32 remaining = 3;

    if (side) {
        otherSide = &work->player;
    }
    cardId = &otherSide->battle[0].battle.layers[0].cardId;
    do {
        s16 id = *cardId;
        cardId += 0xDE;
        remaining -= 1;
        if (id >= 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

/* Occupied disposal slots on the side opposite `side`. */
s32 DisposeAreaCheckEnemy(s32 side, CardGameWork *work)
{
    CardPlaySide *otherSide = &work->enemy;
    s16 *cardId;
    s32 count = 0;
    s32 remaining = 3;

    if (side) {
        otherSide = &work->player;
    }
    cardId = &otherSide->disposal[0].battle.layers[0].cardId;
    do {
        s16 id = *cardId;
        cardId += 0xDE;
        remaining -= 1;
        if (id >= 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

/* Occupied battle slots on the `side` side itself. */
s32 BattleAreaCheckFriend(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide = &work->player;
    s16 *cardId;
    s32 count = 0;
    s32 remaining = 3;

    if (side) {
        ownSide = &work->enemy;
    }
    cardId = &ownSide->battle[0].battle.layers[0].cardId;
    do {
        s16 id = *cardId;
        cardId += 0xDE;
        remaining -= 1;
        if (id >= 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

/* Occupied disposal slots on the `side` side itself. */
s32 DisposeAreaCheckFriend(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide = &work->player;
    s16 *cardId;
    s32 count = 0;
    s32 remaining = 3;

    if (side) {
        ownSide = &work->enemy;
    }
    cardId = &ownSide->disposal[0].battle.layers[0].cardId;
    do {
        s16 id = *cardId;
        cardId += 0xDE;
        remaining -= 1;
        if (id >= 0) {
            count = count + 1;
        }
    } while (remaining >= 0);
    return count;
}

s32 GetDeckData(s32 side, char *base, s32 cardId);

s32 CheckFriendType4(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide = &work->player;
    s16 *cardId;
    s32 count = 0;
    s32 i;

    if (side) {
        ownSide = &work->enemy;
    }
    cardId = &ownSide->disposal[0].battle.layers[0].cardId;
    for (i = 0; i < 4; i++) {
        s16 id = *cardId;
        cardId += 0xDE;
        if (id >= 0 && GetDeckData(side, (char *)work, id) == 4) {
            count++;
        }
    }
    cardId = &ownSide->battle[0].battle.layers[0].cardId;
    for (i = 0; i < 4; i++) {
        s16 id = *cardId;
        cardId += 0xDE;
        if (id >= 0 && GetDeckData(side, (char *)work, id) == 4) {
            count++;
        }
    }
    return count;
}

s32 CheckFriendKakutouCnt(s32 side, CardGameWork *work)
{
    CardPlaySide *friendSide;
    CardLayerStack *stack;
    s16 *cardId;
    s32 count;
    s32 remaining;

    friendSide = &work->player;
    if (side) {
        friendSide = &work->enemy;
    }
    stack = &friendSide->battle[0].battle;
    cardId = &stack->layers[0].cardId;
    count = 0;
    remaining = 3;
    do {
        if (*cardId >= 0 && CardChkAttackType(work->definitions, stack) == 1) {
            count++;
        }
        cardId += sizeof(CardBoardSlot) / sizeof(*cardId);
        stack = (CardLayerStack *)((char *)stack + sizeof(CardBoardSlot));
        remaining--;
    } while (remaining >= 0);
    return count;
}

s32 CheckNowPower(s32 side, CardGameWork *work, s32 slot, s32 cardId);

s32 AlbedoCheck(s32 side, CardGameWork *work)
{
    CardPlaySide *sideData = &work->player;
    CardBoardSlot *slot;
    s32 i;

    if (side != 0) {
        sideData = &work->enemy;
    }
    if (BattleAreaCheckFriend(side, work) == 1) {
        slot = sideData->battle;
        for (i = 0; i < 4; i++) {
            if (slot->battle.layers[0].cardId >= 0) {
                if (slot->battle.layers[0].cardId == 0xE) {
                    if (CheckNowPower(side, work, i | 0xF0000, 0xE) !=
                        work->definitions[slot->battle.layers[0].cardId].attackPower) {
                        goto found;
                    }
                }
            }
            slot++;
        }
    }
    return -1;
found:
    return i;
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", GainanCheck);

static inline CardPlaySide *gunoOwnSide(s32 side, CardGameWork *work)
{
    if (side == 0) {
        return &work->player;
    }
    return &work->enemy;
}

s32 CheckGuno(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    s16 *cardIdPtr;
    s32 i;

    ownSide = gunoOwnSide(side, work);
    i = 0;
    cardIdPtr = &ownSide->disposal[0].battle.layers[0].cardId;
    for (; i < 4; i++) {
        s16 cardId = *cardIdPtr;

        cardIdPtr += 0xDE;
        if (cardId >= 0 && (work->definitions[cardId].flags & 4)) {
            return 1;
        }
    }
    cardIdPtr = &ownSide->battle[0].battle.layers[0].cardId;
    for (i = 0; i < 4; i++) {
        s16 cardId = *cardIdPtr;

        cardIdPtr += 0xDE;
        if (cardId >= 0 && (work->definitions[cardId].flags & 4)) {
            return 1;
        }
    }
    return 0;
}

s32 CheckREAL(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    s16 *cardIdPtr;
    s32 i;

    ownSide = gunoOwnSide(side, work);
    i = 0;
    cardIdPtr = &ownSide->disposal[0].battle.layers[0].cardId;
    for (; i < 4; i++) {
        s16 cardId = *cardIdPtr;

        cardIdPtr += 0xDE;
        if (cardId >= 0 && (work->definitions[cardId].flags & 2)) {
            return 1;
        }
    }
    cardIdPtr = &ownSide->battle[0].battle.layers[0].cardId;
    for (i = 0; i < 4; i++) {
        s16 cardId = *cardIdPtr;

        cardIdPtr += 0xDE;
        if (cardId >= 0 && (work->definitions[cardId].flags & 2)) {
            return 1;
        }
    }
    return 0;
}

extern s32 CardChkAttackType(CardDefinition *definitions, CardLayerStack *card);
extern s32 CardPlayCntPower(CardGameWork *work, CardLayerStack *card);
extern s32 CGPCalcPowerEnv(s32 isOwnSide, CardGameWork *work, s32 count, s32 cardId);
s32 VX10000Check(s32 side, CardGameWork *work);

s32 CheckNowPower(s32 side, CardGameWork *work, s32 slot, s32 cardId)
{
    CardPlaySide *sideData = &work->player;
    CardLayerStack *card;
    CardLayerStack *card2;
    s32 power;
    s32 idx;

    if (side != 0) {
        sideData = &work->enemy;
    }
    if (cardId < 0) {
        return 0;
    }
    idx = slot & 0xFFFF;
    if (slot < 0) {
        return 0;
    }
    if (idx >= 4) {
        return 0;
    }
    if (!(slot & 0xF0000)) {
        card = &sideData->disposal[slot].battle;
        power = CardPlayCntPower(work, card);
        if (sideData->disposal[slot].battle.layers[0].cardId < 0) {
            return 0;
        }
        if (VX10000Check(side, work) == 1 && CardChkAttackType(work->definitions, card) == 4) {
            return 0;
        }
        power = CGPCalcPowerEnv(side, work, power, cardId);
    } else {
        slot = idx;
        card2 = &sideData->battle[slot].battle;
        power = CardPlayCntPower(work, card2);
        if (sideData->battle[slot].battle.layers[0].cardId < 0) {
            return 0;
        }
        if (VX10000Check(side, work) == 1 && CardChkAttackType(work->definitions, card2) == 4) {
            return 0;
        }
        power = CGPCalcPowerEnv(side, work, power, cardId);
    }
    return power;
}

s32 CheckEnemyAttack(s32 side, CardGameWork *work)
{
    s32 power[8];
    s32 top[4];
    CardPlaySide *otherSide;
    CardLayerStack *card;
    s32 attackType;
    s32 i;
    s32 j;
    s32 best;
    s32 sum;

    otherSide = (side == 0) ? &work->enemy : &work->player;
    for (i = 0; i < 4; i++) {
        power[i] = 0;
        card = &otherSide->battle[i].battle;
        if (card->layers[0].cardId >= 0) {
            attackType = CardChkAttackType(work->definitions, card);
            if (*(s8 *)&card->power < 2 && (attackType == 2 || attackType == 4)) {
                power[i] = CGPCalcPowerEnv(side == 0, work, CardPlayCntPower(work, card),
                                           card->layers[0].cardId);
            }
        }
    }
    for (i = 0; i < 4; i++) {
        power[i + 4] = 0;
        card = &otherSide->disposal[i].battle;
        if (card->layers[0].cardId >= 0) {
            attackType = CardChkAttackType(work->definitions, card);
            if (*(s8 *)&card->power < 2 && (attackType == 2 || attackType == 4)) {
                power[i + 4] = CGPCalcPowerEnv(side == 0, work, CardPlayCntPower(work, card),
                                               card->layers[0].cardId);
            }
        }
    }
    for (i = 0; i < 4; i++) {
        best = 0;
        for (j = 0; j < 8; j++) {
            if (power[best] < power[j]) {
                best = j;
            }
        }
        top[i] = power[best];
        power[best] = 0;
    }
    sum = 0;
    for (i = 0; i < 4; i++) {
        sum += top[i];
    }
    return sum;
}

extern s32 CardChkAttackType(CardDefinition *definitions, CardLayerStack *card);
extern s32 CardPlayCntPower(CardGameWork *work, CardLayerStack *card);
extern s32 CGPCalcPowerEnv(s32 isOwnSide, CardGameWork *work, s32 count, s32 cardId);

/*
 * CardLayerStack.power (ov10/cgp.h) reads unsigned everywhere it is stored so
 * far, but CheckEnemy12Attack and CheckEnemyFight both compare it with `lb`
 * (0x00a35168, 0x00a35204, 0x00a3530c, 0x00a35384): it is signed here.
 */
static inline s8 CardLayerPower(CardLayerStack *card) {
    return *(s8 *) &card->power;
}

/*
 * Highest and second-highest CGPCalcPowerEnv value among the other side's
 * category-one/two attack cards (power 0 or 1 in the disposal pile with
 * attack type 2 or 3, power 0 or 1 in the battle pile with attack type 2),
 * summed.
 */
s32 CheckEnemy12Attack(s32 side, CardGameWork *work)
{
    struct {
        s32 highest;
        s32 second;
    } best;
    CardPlaySide *otherSide;
    s32 slotIndex;

    memset(&best, 0, sizeof(best));
    otherSide = (side == 0) ? &work->enemy : &work->player;
    for (slotIndex = 0; slotIndex < 4; slotIndex++) {
        CardLayerStack *card = &otherSide->disposal[slotIndex].battle;

        if (card->layers[0].cardId >= 0) {
            s32 attackType = CardChkAttackType(work->definitions, card);

            if (CardLayerPower(card) == 1 && (u32) (attackType - 2) < 2) {
                s32 power = CGPCalcPowerEnv(side == 0, work,
                                             CardPlayCntPower(work, card),
                                             card->layers[0].cardId);
                if (best.highest < power) {
                    best.second = best.highest;
                    best.highest = power;
                } else if (best.second < power) {
                    best.second = power;
                }
            }
        }
    }
    for (slotIndex = 0; slotIndex < 4; slotIndex++) {
        CardLayerStack *card = &otherSide->battle[slotIndex].battle;

        if (card->layers[0].cardId >= 0) {
            s32 attackType = CardChkAttackType(work->definitions, card);

            if (CardLayerPower(card) < 2 && attackType == 2) {
                s32 power = CGPCalcPowerEnv(side == 0, work,
                                             CardPlayCntPower(work, card),
                                             card->layers[0].cardId);
                if (best.highest < power) {
                    best.second = best.highest;
                    best.highest = power;
                } else if (best.second < power) {
                    best.second = power;
                }
            }
        }
    }
    return best.highest + best.second;
}

/*
 * Highest CGPCalcPowerEnv value among the other side's category-one attack
 * cards (power 0 or 1, attack type 1), checking the battle pile then the
 * disposal pile.
 */
s32 CheckEnemyFight(s32 side, CardGameWork *work)
{
    CardPlaySide *otherSide = (side == 0) ? &work->enemy : &work->player;
    s32 best = 0;
    s32 power;
    s32 slotIndex;

    for (slotIndex = 0; slotIndex < 4; slotIndex++) {
        CardLayerStack *card = &otherSide->battle[slotIndex].battle;

        if (card->layers[0].cardId >= 0) {
            s32 attackType = CardChkAttackType(work->definitions, card);

            if (CardLayerPower(card) < 2 && attackType == 1) {
                power = CGPCalcPowerEnv(side == 0, work,
                                         CardPlayCntPower(work, card),
                                         card->layers[0].cardId);
                if (power > best) {
                    best = power;
                }
            }
        }
    }
    for (slotIndex = 0; slotIndex < 4; slotIndex++) {
        CardLayerStack *card = &otherSide->disposal[slotIndex].battle;

        if (card->layers[0].cardId >= 0) {
            s32 attackType = CardChkAttackType(work->definitions, card);

            if (CardLayerPower(card) < 2 && attackType == 1) {
                power = CGPCalcPowerEnv(side == 0, work,
                                         CardPlayCntPower(work, card),
                                         card->layers[0].cardId);
                if (power > best) {
                    best = power;
                }
            }
        }
    }
    return best;
}

extern void CardCountGeneration(u8 *counts, CardDefinition *definitions, CardPlaySide *side);
extern u8 *DeckGeneLst[];

s32 CheckGeneration(s32 side, CardGameWork *work)
{
    u8 counts[8];
    CardPlaySide *sideData;
    u8 *gene;
    s32 total = 0;
    s32 i;
    u8 deckId;

    if (side == 0) {
        sideData = &work->player;
        deckId = ((u8 *)work)[0x59BA];
    } else {
        deckId = ((u8 *)work)[0x59BB];
        sideData = &work->enemy;
    }
    gene = DeckGeneLst[deckId];
    CardCountGeneration(counts, work->definitions, sideData);
    for (i = 0; i < 5; i++) {
        total += counts[i];
    }
    if (total >= gene[5]) {
        return 1;
    }
    for (i = 0; i < 5; i++) {
        if (gene[i] > counts[i]) {
            return 0;
        }
    }
    return 1;
}

extern unsigned char *EnemyDeckLst[];
extern unsigned char *DeckDataLst[];

s32 GetDeckData(s32 side, char *base, s32 cardId)
{
    s32 idx;
    s32 i;
    unsigned char *data;
    unsigned char *deck;

    if (cardId < 0) {
        return 0;
    }
    if (side == 0) {
        idx = (unsigned char)base[0x59BA];
    } else {
        idx = (unsigned char)base[0x59BB];
    }
    deck = EnemyDeckLst[idx];
    data = DeckDataLst[idx];
    for (i = 0; i < 40; i++) {
        if (deck[i] == cardId) {
            return data[i];
        }
    }
    return 0;
}

u32 GetDeckDataAdjust(s32 side, CardGameWork *work, s32 cardId)
{
    u8 idx;
    unsigned char *data;
    unsigned char *deck;
    s32 i;

    if (cardId < 0) {
        return 0;
    }
    if (side == 0) {
        idx = ((u8 *)work)[0x59BA];
    } else {
        idx = work->enemyDeckId;
    }
    deck = EnemyDeckLst[idx];
    data = DeckDataLst[idx];
    if (BattleAreaCheckEnemy(side, work) + DisposeAreaCheckEnemy(side, work) == 0) {
        return 3;
    }
    for (i = 0; i < 40; i++) {
        if (deck[i] == cardId) {
            if (CheckGeneration(side, work) != 0) {
                if (data[i] == 0) {
                    return 2;
                }
                if (data[i] == 1) {
                    return 3;
                }
            }
            return data[i];
        }
    }
    return 0;
}

extern s32 CardPlayYamaCnt(CardPlaySide *side);
extern s32 CardPlayChkCondition(CardDefinition *definitions, CardPlaySide *side, s32 cardId, CardGameWork *work);
extern s32 CardPlayCommandCondition(u16 side, u16 cardId, CardGameWork *work);

/*
 * Whether the given side may play command card `cardId` now: a card that
 * reserves cards from the draw pile has to leave at least three of them
 * there, and neither the card's own condition nor the command condition may
 * refuse it.
 */
s32 CheckCommandPlay(s32 side, CardGameWork *work, s32 cardId)
{
    CardPlaySide *ownSide = (side == 0) ? &work->player : &work->enemy;

    if (work->definitions[cardId].reserveCost != 0 &&
        CardPlayYamaCnt(ownSide) - work->definitions[cardId].reserveCost < 3) {
        return 0;
    }
    if (CardPlayChkCondition(work->definitions, ownSide, cardId, work) == 0 &&
        CardPlayCommandCondition(side, cardId, work) == 0) {
        return 1;
    }
    return 0;
}

extern s32 CardPlayHandCnt(CardHand *hand);
extern s32 CheckCommandPlay(s32 side, CardGameWork *work, s32 cardId);

/* Card ids 0x53/0x75: the two Down/Kaifuku recovery command cards. */
s32 CheckDownKaifukuCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *sideData = &work->player;
    s32 cardIndex = 0;
    s32 handCount;

    if (side != 0) {
        sideData = &work->enemy;
    }
    handCount = CardPlayHandCnt(&sideData->hand);
    if (handCount <= 0) {
        return 0;
    }
    do {
        s16 cardId = sideData->hand.cards[cardIndex];

        if (cardId == 0x53 || cardId == 0x75) {
            s32 result = CheckCommandPlay(side, work, cardId);
            if (result == 1) {
                return result;
            }
        }
        cardIndex++;
    } while (cardIndex < handCount);
    return 0;
}

static inline CardPlaySide *gunoOtherSide(s32 side, CardGameWork *work)
{
    if (side == 0) {
        return &work->enemy;
    }
    return &work->player;
}

s32 CheckEnemyGuno(s32 side, CardGameWork *work)
{
    CardPlaySide *otherSide;
    s16 *cardIdPtr;
    s32 i;

    otherSide = gunoOtherSide(side, work);
    i = 0;
    cardIdPtr = &otherSide->disposal[0].battle.layers[0].cardId;
    for (; i < 4; i++) {
        s16 cardId = *cardIdPtr;

        cardIdPtr += 0xDE;
        if (cardId >= 0 && (work->definitions[cardId].flags & 4)) {
            return 1;
        }
    }
    cardIdPtr = &otherSide->battle[0].battle.layers[0].cardId;
    for (i = 0; i < 4; i++) {
        s16 cardId = *cardIdPtr;

        cardIdPtr += 0xDE;
        if (cardId >= 0 && (work->definitions[cardId].flags & 4)) {
            return 1;
        }
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_SetOpe);

static inline CardPlaySide *tamasiiSide(s32 side, CardGameWork *work)
{
    if (side == 0) {
        return &work->player;
    }
    return &work->enemy;
}

s32 CheckTamasii(s32 side, CardGameWork *work)
{
    CardPlaySide *sideData;
    s32 cardIndex;
    s32 handCount;
    s16 *cards;
    s16 cardId;

    sideData = tamasiiSide(side, work);
    if (*(s16 *)&work->command.phase == 1 || *(s16 *)&work->command.phase == 8) {
        handCount = CardPlayHandCnt(&sideData->hand);
        for (cardIndex = 0; cardIndex < handCount; cardIndex++) {
            cardId = sideData->hand.cards[cardIndex];
            if (work->definitions[cardId].kind == 4 && cardId == 0x7B &&
                CardPlayChkCondition(work->definitions, sideData, 0x7B, work) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_SetWeapon);

extern void CGPSetFaseProc(s32 side, CardGameWork *work, CardPlaySide *own);

s32 CC_Kara_SetLv10(s32 side, CardGameWork *work, s32 index)
{
    CardPlaySide *own;
    s16 *cardId;
    s32 i;

    own = tamasiiSide(side, work);
    if (own->hand.cards[index] == 7) {
        i = 0;
        cardId = &own->battle[0].battle.layers[0].cardId;
        for (; i < 4; i++) {
            s16 id = *cardId;
            cardId += 0xDE;
            if (id == 0) {
                work->command.phaseTimer = 0x14;
                CGPSetFaseProc(side, work, own);
                return 1;
            }
        }
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", SetGunoFinish);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_SetGuno);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_SetFase_1);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_CommFase);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CheckDamageCommand);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CheckTaikyakuCommand);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CheckTotugekiCommand);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CheckKonranCommand);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CheckCurryCommand);

s32 TodomeHyouka(s32 side, CardGameWork *work, s32 a2, s32 a3, s32 *life, s32 *attack);
s32 GetFriendAttack(s32 side, CardGameWork *work, s32 *life, s32 *attack);

s32 CheckTentCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide = &work->player;
    s32 life[4];
    s32 attack[4];
    s32 i;
    u8 c = side;
    s32 before = (c << 8) & 0xff;
    s32 after;
    s16 cardId;

    if (side) {
        ownSide = &work->enemy;
    }
    if (*(s16 *)&work->command.phase == 2 || *(s16 *)&work->command.phase == 9) {
        return 0;
    }
    GetFriendAttack(side == 0, work, life, attack);
    for (i = before; i < 4; i++) {
        attack[i] &= ~0x10000;
    }
    before = TodomeHyouka(side == 0, work, 0, 0, life, attack);
    for (i = 0; i < 4; i++) {
        if (ownSide->battle[i].battle.layers[0].cardId >= 0 &&
            work->definitions[cardId = ownSide->battle[i].battle.layers[0].cardId].flags != 4) {
            life[i] += 3;
            if (work->definitions[cardId].operationDuration < life[i]) {
                life[i] = work->definitions[cardId].operationDuration;
            }
        }
    }
    after = TodomeHyouka(side == 0, work, 0, 0, life, attack);
    for (i = 0; i < 4; i++) {
        attack[i] &= 0xF0FFFF;
    }
    if (after + 15 <= before) {
        return 1;
    }
    return 0;
}

s32 GetShoukin(CardLayerStack *stack);

s32 CheckSouteniHouCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    CardPlaySide *otherSide;
    s32 i;
    s32 otherTotal = 0;
    s32 ownTotal = 0;
    s16 cardId;

    if (side == 0) {
        ownSide = &work->player;
        otherSide = &work->enemy;
    } else {
        ownSide = &work->enemy;
        otherSide = &work->player;
    }
    for (i = 0; i < 4; i++) {
        cardId = ownSide->battle[i].battle.layers[0].cardId;
        if (cardId >= 0 &&
            (work->definitions[cardId].flags == 4 || work->definitions[cardId].flags == 8)) {
            ownTotal += GetShoukin(&ownSide->battle[i].battle);
        }
        cardId = otherSide->battle[i].battle.layers[0].cardId;
        if (cardId >= 0 &&
            (work->definitions[cardId].flags == 4 || work->definitions[cardId].flags == 8)) {
            otherTotal += GetShoukin(&otherSide->battle[i].battle);
        }
    }
    return ownTotal < otherTotal;
}

s32 GetShoukin(CardLayerStack *stack);
s32 GetPrice(CardLayerStack *stack);

s32 CheckHatudouCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    CardPlaySide *otherSide;
    s32 i;
    s32 otherTotal = 0;
    s32 ownTotal = 0;

    if (side == 0) {
        ownSide = &work->player;
        otherSide = &work->enemy;
    } else {
        ownSide = &work->enemy;
        otherSide = &work->player;
    }
    for (i = 0; i < 4; i++) {
        if (ownSide->battle[i].battle.layers[0].cardId >= 0) {
            ownTotal += GetShoukin(&ownSide->battle[i].battle);
        }
        if (ownSide->disposal[i].battle.layers[0].cardId >= 0) {
            ownTotal += GetPrice(&ownSide->disposal[i].battle);
        }
        if (otherSide->battle[i].battle.layers[0].cardId >= 0) {
            otherTotal += GetShoukin(&otherSide->battle[i].battle);
        }
        if (otherSide->disposal[i].battle.layers[0].cardId >= 0) {
            otherTotal += GetPrice(&otherSide->disposal[i].battle);
        }
    }
    return ownTotal < otherTotal;
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CheckXDamageCommand);

s32 GetShoukin(CardLayerStack *stack);

s32 CheckMilCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *otherSide = &work->enemy;
    s32 total = 0;

    if (side) {
        otherSide = &work->player;
    }
    if (otherSide->battle[2].battle.layers[0].cardId >= 0) {
        total += GetShoukin(&otherSide->battle[2].battle) * 0.9f;
    }
    if (otherSide->battle[3].battle.layers[0].cardId >= 0) {
        total += GetShoukin(&otherSide->battle[3].battle) * 0.9f;
    }
    if (total <= 80) {
        return 0;
    }
    return 1;
}

s32 CheckNazoCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    CardPlaySide *otherSide;
    s32 total = 0;
    s32 i;

    if (side == 0) {
        ownSide = &work->player;
        otherSide = &work->enemy;
    } else {
        ownSide = &work->enemy;
        otherSide = &work->player;
    }
    if (otherSide->battle[0].battle.layers[0].cardId < 0 &&
        otherSide->battle[1].battle.layers[0].cardId < 0) {
        for (i = 0; i < 4; i++) {
            if (ownSide->battle[i].battle.layers[0].cardId >= 0 &&
                CardChkAttackType(work->definitions, &ownSide->battle[i].battle) == 1) {
                total += CheckNowPower(side, work, i | 0xF0000, ownSide->battle[i].battle.layers[0].cardId);
            }
        }
    }
    if (total < 4) {
        return 0;
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CheckSeimitsuCommand);

s32 GetPrice(CardLayerStack *stack);

s32 CheckGunoYokanCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *otherSide = &work->enemy;
    CardLayerStack *stack;
    s32 i;
    s32 best = 0;
    s32 found = -1;

    if (side) {
        otherSide = &work->player;
    }
    stack = &otherSide->disposal[0].battle;
    for (i = 0; i < 4; i++) {
        if (stack->layers[0].cardId >= 0 && stack->flags == 1 && best < GetPrice(stack)) {
            found = i;
            best = GetPrice(stack);
        }
        stack = (CardLayerStack *)((char *)stack + sizeof(CardBoardSlot));
    }
    return found;
}

s32 CheckExtCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide = &work->player;
    s32 i;
    s32 value;
    s32 best;
    s32 found;

    if (side) {
        ownSide = &work->enemy;
    }
    best = 0;
    found = -1;
    for (i = 0; i < 4; i++) {
        if (ownSide->battle[i].battle.layers[0].cardId >= 0 && (ownSide->battle[i].battle.flags & 2)) {
            value = GetShoukin(&ownSide->battle[i].battle);
            if (best < value) {
                best = value;
                found = i | 0xF0000;
            }
        }
    }
    for (i = 0; i < 4; i++) {
        if (ownSide->disposal[i].battle.layers[0].cardId >= 0 && (ownSide->disposal[i].battle.flags & 2)) {
            value = GetPrice(&ownSide->disposal[i].battle);
            if (best < value) {
                best = value;
                found = i;
            }
        }
    }
    return found;
}

s32 CheckDownGunoCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *otherSide = &work->enemy;
    s32 i;
    s32 best = 0;
    s32 found = -1;
    s32 value;

    if (side) {
        otherSide = &work->player;
    }
    for (i = 0; i < 4; i++) {
        if (otherSide->battle[i].battle.layers[0].cardId >= 0 &&
            work->definitions[otherSide->battle[i].battle.layers[0].cardId].flags == 4 &&
            (otherSide->battle[i].battle.flags & 2)) {
            value = GetShoukin(&otherSide->battle[i].battle);
            if (best < value) {
                best = value;
                found = i | 0xF0000;
            }
        }
    }
    for (i = 0; i < 4; i++) {
        if (otherSide->disposal[i].battle.layers[0].cardId >= 0 &&
            work->definitions[otherSide->disposal[i].battle.layers[0].cardId].flags == 4 &&
            (otherSide->disposal[i].battle.flags & 2)) {
            value = GetPrice(&otherSide->disposal[i].battle);
            if (best < value) {
                best = value;
                found = i;
            }
        }
    }
    return found;
}

s32 GetPrice(CardLayerStack *stack);

/* Highest-price category-one card among the flagged slots of the other side's
 * disposal board, encoded as a disposal slot index (-1 if none qualifies). */
s32 CheckChudanCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *otherSide = (side == 0) ? &work->enemy : &work->player;
    s32 bestPrice = 0;
    s32 bestSlot = -1;
    s32 slotIndex;

    for (slotIndex = 0; slotIndex < 4; slotIndex++) {
        s16 cardId = otherSide->disposal[slotIndex].battle.layers[0].cardId;

        if (cardId >= 0 && work->definitions[cardId].flags == 1 &&
            (otherSide->disposal[slotIndex].battle.flags & 1)) {
            s32 price = GetPrice(&otherSide->disposal[slotIndex].battle);
            if (bestPrice < price) {
                bestPrice = price;
                bestSlot = slotIndex;
            }
        }
    }
    return bestSlot;
}

extern s32 play413;

s32 Check413Command(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    CardPlaySide *otherSide;
    s16 *cardId;
    s32 count = 0;
    s32 i;

    if (side == 0) {
        ownSide = &work->player;
        otherSide = &work->enemy;
    } else {
        ownSide = &work->enemy;
        otherSide = &work->player;
    }
    if (play413 == 1) {
        return 0;
    }
    cardId = &ownSide->battle[0].battle.layers[0].cardId;
    for (i = 0; i < 4; i++) {
        s16 id = *cardId;
        cardId += 0xDE;
        if (id >= 0 && work->definitions[id].flags == 2) {
            count++;
        }
    }
    if (count >= 2) {
        return 1;
    }
    if (count == 1 && CardPlayYamaCnt(otherSide) < 13) {
        return 1;
    }
    return 0;
}

s32 GetPrice(CardLayerStack *stack);

/* Highest-price flagged card on the other side's disposal board, encoded as
 * a disposal slot index (-1 if none qualifies). */
s32 CheckKyoukouCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *otherSide = (side == 0) ? &work->enemy : &work->player;
    s32 bestPrice = 0;
    s32 bestSlot = -1;
    s32 slotIndex;

    for (slotIndex = 0; slotIndex < 4; slotIndex++) {
        if (otherSide->disposal[slotIndex].battle.layers[0].cardId >= 0 &&
            (otherSide->disposal[slotIndex].battle.flags & 2)) {
            s32 price = GetPrice(&otherSide->disposal[slotIndex].battle);
            if (bestPrice < price) {
                bestPrice = price;
                bestSlot = slotIndex;
            }
        }
    }
    return bestSlot;
}

s32 GetShoukin(CardLayerStack *stack);

/* Lowest-reward active category-one card on the caller's own battle board,
 * encoded as a battle slot index (0xF0000 tags it as a battle, not disposal,
 * slot for the caller). */
s32 CheckShukumeiCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    CardPlaySide *opponent;
    s32 bestReward = 0x32;
    s32 bestSlot = -1;
    s32 slotIndex;

    if (side == 0) {
        ownSide = &work->player;
        opponent = &work->enemy;
    } else {
        ownSide = &work->enemy;
        opponent = &work->player;
    }
    for (slotIndex = 0; slotIndex < 4; slotIndex++) {
        s16 *cardIdSlot = &ownSide->battle[slotIndex].battle.layers[0].cardId;
        s16 cardId = *cardIdSlot;

        if (cardId >= 0 && work->definitions[cardId].flags == 1) {
            s32 reward = GetShoukin(&opponent->battle[slotIndex].battle);
            if (reward < bestReward) {
                bestReward = reward;
                bestSlot = slotIndex | 0xF0000;
            }
        }
    }
    return bestSlot;
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CheckBujinCommand);

extern u8 CardDat[0x100];

s32 CheckTuukokuCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *otherSide = &work->enemy;
    s32 best;
    s32 found;
    s32 i;

    if (side) {
        otherSide = &work->player;
    }
    found = -1;
    best = 0;
    for (i = 0; i < 4; i++) {
        if (otherSide->battle[i].battle.layers[0].cardId >= 0 &&
            otherSide->battle[i].battle.layers[1].cardId >= 0 &&
            best < CardDat[otherSide->battle[i].battle.layers[1].cardId]) {
            best = CardDat[otherSide->battle[i].battle.layers[1].cardId];
            found = i | 0xF0000;
        }
    }
    for (i = 0; i < 4; i++) {
        if (otherSide->disposal[i].battle.layers[0].cardId >= 0 &&
            otherSide->disposal[i].battle.layers[1].cardId >= 0 &&
            best < CardDat[otherSide->disposal[i].battle.layers[1].cardId]) {
            best = CardDat[otherSide->disposal[i].battle.layers[1].cardId];
            found = i;
        }
    }
    return found;
}

s32 CheckKetugiCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    CardPlaySide *otherSide;
    s32 otherTotal = 0;
    s32 ownTotal = 0;
    s32 i;

    if (side == 0) {
        ownSide = &work->player;
        otherSide = &work->enemy;
    } else {
        ownSide = &work->enemy;
        otherSide = &work->player;
    }
    for (i = 0; i < 4; i++) {
        if (ownSide->battle[i].battle.layers[0].cardId >= 0 &&
            ownSide->battle[i].battle.layers[1].cardId >= 0) {
            ownTotal += CardDat[ownSide->battle[i].battle.layers[1].cardId];
        }
        if (otherSide->battle[i].battle.layers[0].cardId >= 0 &&
            otherSide->battle[i].battle.layers[1].cardId >= 0) {
            otherTotal += CardDat[otherSide->battle[i].battle.layers[1].cardId];
        }
    }
    for (i = 0; i < 4; i++) {
        if (ownSide->disposal[i].battle.layers[0].cardId >= 0 &&
            ownSide->disposal[i].battle.layers[1].cardId >= 0) {
            ownTotal += CardDat[ownSide->disposal[i].battle.layers[1].cardId];
        }
        if (otherSide->disposal[i].battle.layers[0].cardId >= 0 &&
            otherSide->disposal[i].battle.layers[1].cardId >= 0) {
            otherTotal += CardDat[otherSide->disposal[i].battle.layers[1].cardId];
        }
    }
    return ownTotal < otherTotal;
}

s32 TodomeHyouka(s32 side, CardGameWork *work, s32 a2, s32 a3, s32 *life, s32 *attack);
s32 GetFriendAttack(s32 side, CardGameWork *work, s32 *life, s32 *attack);
extern s32 playteppeki;

s32 CheckTeppekiCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    CardPlaySide *otherSide;
    s32 life[4];
    s32 attack[4];
    s32 enemyAttack;
    s32 i;

    if (side == 0) {
        ownSide = &work->player;
        otherSide = &work->enemy;
    } else {
        ownSide = &work->enemy;
        otherSide = &work->player;
    }
    if (playteppeki == 1) {
        return 0;
    }
    {
        enemyAttack = CheckEnemyAttack(side, work);
        if (VX10000Check(side, work) == 0) {
            for (i = 0; i < 4; i++) {
                if (otherSide->battle[i].battle.layers[0].cardId >= 0 &&
                    CardChkAttackType(work->definitions, &otherSide->battle[i].battle) == 4 &&
                    (s8)otherSide->battle[i].battle.power < 2) {
                    return 1;
                }
            }
        }
        GetFriendAttack(side == 0, work, life, attack);
        if (TodomeHyouka(side == 0, work, 0, 0, life, attack) > 1000) {
            return 1;
        }
        return CardPlayYamaCnt(ownSide) < enemyAttack;
    }
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CheckSensiCommand);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", SensiEvent);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_DisposeEvent);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_CommRequest);

extern s32 commplay;
extern s32 playteppeki;
extern void SensiEvent(s32 side, CardGameWork *work);
extern void CGPEnemyExecCommand(s32 side, CardGameWork *work, s16 *command, s32 flag);

void CC_Kara_CommPlay(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    s16 command[2];
    s16 a;
    s16 b;
    s16 c;
    s16 d;

    if (side == 0) {
        ownSide = &work->player;
        a = 5;
        b = 4;
        c = 6;
        d = 2;
    } else {
        ownSide = &work->enemy;
        a = 4;
        b = 5;
        c = 2;
        d = 6;
    }
    if (!(commplay & 0xF00000)) {
        if (!(commplay & 0xF0000)) {
            command[0] = c;
            command[1] = commplay;
        } else {
            command[0] = a;
            command[1] = commplay;
        }
    } else {
        if (!(commplay & 0xF0000)) {
            command[0] = d;
            command[1] = commplay;
        } else {
            command[0] = b;
            command[1] = commplay;
        }
    }
    switch (*(s16 *)((char *)ownSide + 0x198)) {
    case 0x53:
    case 0x77:
    default:
        break;
    case 0x64:
        playteppeki = 1;
        break;
    case 0x65:
        play413 = 1;
        break;
    case 0x6D:
        SensiEvent(side, work);
        return;
    }
    CGPEnemyExecCommand(side, work, command, 0);
}

extern int timer_2[];
int CC_Kara_SetFase_1(int a0, u8 *a1);

int CC_Kara_SetFase(int a0, u8 *a1)
{
    if (a1[0x59B9] == 0) {
        a1[0x59B9] = 1;
        timer_2[0] = 15;
    }
    if (timer_2[0] == 0) {
        a1[0x59B9]++;
        if (a1[0x59B9] > 0x28) {
            return 0;
        }
        timer_2[0] = 15;
        return CC_Kara_SetFase_1(a0, a1);
    }
    timer_2[0]--;
    return 1;
}

extern u8 CardDat[0x100];

/* Reward of the base card in the stack plus, if present, the card stacked on
 * it (CardDat[cardId], indexed directly by card id). Zero if the base layer
 * is empty. */
s32 GetShoukin(CardLayerStack *stack)
{
    s16 cardId0 = stack->layers[0].cardId;
    s32 reward;
    s16 cardId1;

    if (cardId0 < 0) {
        return 0;
    }
    reward = CardDat[cardId0];
    cardId1 = stack->layers[1].cardId;
    if (cardId1 >= 0) {
        reward += CardDat[cardId1];
    }
    return reward;
}

/* Price of the base card in the stack plus, if present, the card stacked on
 * it (CardDat[cardId], indexed directly by card id). Zero if the base layer
 * is empty. */
s32 GetPrice(CardLayerStack *stack)
{
    s16 cardId0 = stack->layers[0].cardId;
    s32 price;
    s16 cardId1;

    if (cardId0 < 0) {
        return 0;
    }
    price = CardDat[cardId0];
    cardId1 = stack->layers[1].cardId;
    if (cardId1 >= 0) {
        price += CardDat[cardId1];
    }
    return price;
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", TodomeHyouka);

s32 LvUpCheck(s32 side, CardGameWork *work, s32 slot);
extern s32 CardPlayJunkCnt(CardPlaySide *side);

s32 GetFriendAttack(s32 side, CardGameWork *work, s32 *life, s32 *attack)
{
    CardPlaySide *ownSide;
    CardPlaySide *otherSide;
    s32 albedoSlot = -1;
    s32 i;
    s16 cardId;
    s32 type;
    s16 *cardIdPtr;
    s32 *atk;

    if (side == 0) {
        ownSide = &work->player;
        otherSide = &work->enemy;
    } else {
        ownSide = &work->enemy;
        otherSide = &work->player;
    }
    for (i = 0; i < 4; i++) {
        life[i] = 0;
        attack[i] = 0;
        if (otherSide->battle[i].battle.layers[0].cardId >= 0) {
            life[i] = otherSide->battle[i].battle.layers[0].life;
        }
        cardId = ownSide->battle[i].battle.layers[0].cardId;
        if (cardId >= 0) {
            type = CardChkAttackType(work->definitions, &ownSide->battle[i].battle);
            if ((s8)ownSide->battle[i].battle.power < 2) {
                if (type == 2) {
                    attack[i] = CheckNowPower(side, work, i | 0xF0000, ownSide->battle[i].battle.layers[0].cardId);
                    if (LvUpCheck(side, work, i | 0xF0000) != -1) {
                        attack[i] = LvUpCheck(side, work, i | 0xF0000);
                    }
                    if (cardId == 0x1F || cardId == 9) {
                        attack[i] |= 0x10000;
                    }
                    if (cardId == 0xE) {
                        albedoSlot = i;
                    }
                } else if (type == 1) {
                    attack[i] = CheckNowPower(side, work, i | 0xF0000, ownSide->battle[i].battle.layers[0].cardId);
                    if (LvUpCheck(side, work, i | 0xF0000) != -1) {
                        attack[i] = LvUpCheck(side, work, i | 0xF0000);
                    }
                    attack[i] |= 0x20000;
                    if (cardId == 2 || cardId == 9 || cardId == 0x1E || cardId == 0x43) {
                        attack[i] |= 0x10000;
                    }
                } else if (type == 3) {
                    attack[i] = CheckNowPower(side, work, i | 0xF0000, ownSide->battle[i].battle.layers[0].cardId) | 0x40000;
                } else if (type == 4) {
                    attack[i] = CheckNowPower(side, work, i | 0xF0000, ownSide->battle[i].battle.layers[0].cardId) | 0x80000;
                }
            }
            if (type == 3) {
                attack[i] |= 0x40000;
            }
            if (type == 4) {
                attack[i] |= 0x80000;
            }
            if (cardId == 0x20 || cardId == 0x4C) {
                attack[i] |= 0x100000;
            }
            if (ownSide->battle[i].battle.flags & 2) {
                s32 v = attack[i];
                v &= 0xFFFF0000;
                v &= ~0x10000;
                v &= ~0x100000;
                attack[i] = v;
            }
        }
    }
    if (albedoSlot != -1 && BattleAreaCheckFriend(side, work) == 1 && attack[albedoSlot] == 2) {
        attack[albedoSlot] = CardPlayJunkCnt(ownSide) + 2;
    }
    if (CheckTamasii(side, work) == 1) {
        cardIdPtr = &ownSide->battle[0].battle.layers[0].cardId;
        atk = attack;
        for (i = 3; i >= 0; i--) {
            s16 id = *cardIdPtr;

            cardIdPtr += 0xDE;
            if (id >= 0 && (u16)(work->definitions[id].flags & 1)) {
                (*atk)++;
            }
            atk++;
        }
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_TodomeMove);

s32 CC_Kara_TodomeMove(s32 side, CardGameWork *work, s32 slot);

s32 CC_Kara_AttackMove(s32 side, CardGameWork *work, s32 slot)
{
    CardPlaySide *sideData = &work->player;
    s32 attackType;

    if (side != 0) {
        sideData = &work->enemy;
    }
    slot &= 0xFFFF;
    attackType = CardChkAttackType(work->definitions, &sideData->battle[slot].battle);
    if (attackType == 2) {
        return CC_Kara_TodomeMove(side, work, slot);
    }
    if (attackType == 1) {
        return CC_Kara_TodomeMove(side, work, slot);
    }
    return CC_Kara_TodomeMove(side, work, slot);
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_BackTodomeMove);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_FrontTodomeMove);

s32 CC_Kara_BackTodomeMove(s32 side, CardGameWork *work, s32 slot);

s32 CC_Kara_BackAttackMove(s32 side, CardGameWork *work, s32 slot)
{
    CardPlaySide *sideData = &work->player;
    CardLayerStack *battle;
    s32 attackType;

    if (side != 0) {
        sideData = &work->enemy;
    }
    slot &= 0xFFFF;
    battle = &sideData->battle[slot].battle;
    attackType = CardChkAttackType(work->definitions, battle);
    if (attackType == 2) {
        if (BattleAreaCheckEnemy(side, work) == 0) {
            if ((u32)slot < 2) {
                if (sideData->battle[2].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&sideData->battle[2].battle, (CardMoveWork *)battle);
                    return 1;
                }
                if (sideData->battle[3].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&sideData->battle[3].battle, (CardMoveWork *)battle);
                    return 1;
                }
            }
            return 0;
        }
        return CC_Kara_BackTodomeMove(side, work, slot);
    }
    if (attackType == 1) {
        return CC_Kara_BackTodomeMove(side, work, slot);
    }
    if (BattleAreaCheckEnemy(side, work)) {
        return 0;
    }
    if ((u32)slot < 2) {
        if (sideData->battle[2].battle.layers[0].cardId < 0) {
            CGPMoveDSBATTLEwork((CardMoveWork *)&sideData->battle[2].battle, (CardMoveWork *)battle);
            return 1;
        }
        if (sideData->battle[3].battle.layers[0].cardId < 0) {
            CGPMoveDSBATTLEwork((CardMoveWork *)&sideData->battle[3].battle, (CardMoveWork *)battle);
            return 1;
        }
    }
    return 0;
}

/* Card id 0x27 (VX10000) on either side's battle board, checking both sides'
 * slot i together for i in 0..3. */
s32 VX10000Check(s32 side, CardGameWork *work)
{
    CardPlaySide *sideA;
    CardPlaySide *sideB;
    s32 slotIndex;

    if (!side) {
        sideA = &work->player;
        sideB = &work->enemy;
    } else {
        sideA = &work->enemy;
        sideB = &work->player;
    }
    for (slotIndex = 0; slotIndex < 4; slotIndex++) {
        if (sideA->battle[slotIndex].battle.layers[0].cardId == 0x27) {
            return 1;
        }
        if (sideB->battle[slotIndex].battle.layers[0].cardId == 0x27) {
            return 1;
        }
    }
    return 0;
}

/* Whether `side`'s battle board has card id 0x51 (Yoakimu) in some slot and
 * card id 3, 8 or 10 (MoMo) in some slot. */
s32 YoakimuMoMoCheck(s32 side, CardGameWork *work)
{
    CardPlaySide *sideData = &work->player;
    s16 *cardIdPtr;
    s32 yoakimuSlot = -1;
    s32 moMoSlot = -1;
    s32 slotIndex;

    if (side) {
        sideData = &work->enemy;
    }
    cardIdPtr = &sideData->battle[0].battle.layers[0].cardId;
    for (slotIndex = 0; slotIndex < 4; slotIndex++) {
        s16 cardId = *cardIdPtr;

        cardIdPtr += 0xDE;
        if (cardId == 0x51) {
            yoakimuSlot = slotIndex;
        }
        if (cardId == 3 || cardId == 8 || cardId == 10) {
            moMoSlot = slotIndex;
        }
    }
    if (yoakimuSlot != -1) {
        if (moMoSlot != -1) {
            return 1;
        }
    }
    return 0;
}

s32 MoMoCheck(s32 side, CardGameWork *work)
{
    CardPlaySide *sideData = &work->player;
    s16 *cardIdPtr;
    s16 *battlePtr;
    s32 slotIndex;

    if (side != 0) {
        sideData = &work->enemy;
    }
    for (slotIndex = 0; slotIndex < 4; slotIndex++) {
        s16 cardId = *(s16 *)((char *)sideData + slotIndex * 0x1BC + 0x89A);

        if (cardId == 3 || cardId == 8 || cardId == 10) {
            return slotIndex;
        }
    }
    battlePtr = &sideData->battle[0].battle.layers[0].cardId;
    for (slotIndex = 0; slotIndex < 4; slotIndex++) {
        s16 cardId = *battlePtr;

        battlePtr += 0xDE;
        if (cardId == 3 || cardId == 8 || cardId == 10) {
            return slotIndex | 0xF0000;
        }
    }
    return -1;
}

static inline s32 YoakimuMoveTo(CardMoveWork *src, CardMoveWork *base)
{
    CGPMoveDSBATTLEwork(src, (CardMoveWork *)((char *)base + 0x88C));
    return 1;
}

s32 YoakimuMove(s32 side, CardGameWork *work, s32 slot)
{
    CardPlaySide *ps;
    CardMoveWork *mid;
    s32 mo;
    s32 idx;
    s32 cnt;
    s32 i;
    s32 off;

    ps = side == 0 ? &work->player : &work->enemy;

    if (!(slot & 0xF0000)) {
        off = slot * 0x1BC;
        if (ps->disposal[slot].battle.layers[0].cardId == 0x51) {
            mo = MoMoCheck(side, work);
            if (mo != -1 && (mo & 0xF0000) != 0) {
                switch (mo & 0xFFFF) {
                case 0:
                    return YoakimuMoveTo((CardMoveWork *)((char *)ps + 0x514),
                                         (CardMoveWork *)((char *)ps + off));
                case 1:
                    return YoakimuMoveTo((CardMoveWork *)((char *)ps + 0x6D0),
                                         (CardMoveWork *)((char *)ps + off));
                case 2:
                    mid = (CardMoveWork *)((char *)ps + 0x514);
                    CGPMoveDSBATTLEwork((CardMoveWork *)((char *)ps + 0x19C), mid);
                    return YoakimuMoveTo(mid, (CardMoveWork *)((char *)ps + off));
                case 3:
                    mid = (CardMoveWork *)((char *)ps + 0x6D0);
                    CGPMoveDSBATTLEwork((CardMoveWork *)((char *)ps + 0x358), mid);
                    return YoakimuMoveTo(mid, (CardMoveWork *)((char *)ps + off));
                }
            }
            cnt = CardPlayHandCnt((CardHand *)ps);
            for (i = 0; i < cnt; i++) {
                if (ps->hand.cards[i] == 0x32 || ps->hand.cards[i] == 0x33) {
                    return 0;
                }
            }
            return CheckNowPower(side, work, slot,
                                 ps->disposal[slot].battle.layers[0].cardId) == 0;
        }
        return 0;
    } else {
        idx = slot & 0xFFFF;
        off = idx * 0x1BC;
        if (ps->battle[idx].battle.layers[0].cardId == 0x51) {
            mo = MoMoCheck(side, work);
            if (mo != -1 && (mo & 0xF0000) != 0) {
                switch (mo & 0xFFFF) {
                case 0:
                    if (idx != 2) {
                        CGPMoveDSBATTLEwork((CardMoveWork *)((char *)ps + 0x514),
                                            (CardMoveWork *)((char *)ps + off + 0x19C));
                    }
                    return 1;
                case 1:
                    if (idx != 3) {
                        CGPMoveDSBATTLEwork((CardMoveWork *)((char *)ps + 0x6D0),
                                            (CardMoveWork *)((char *)ps + off + 0x19C));
                    }
                    return 1;
                }
            }
            return CheckNowPower(side, work, slot | 0xF0000,
                                 ps->battle[idx].battle.layers[0].cardId) != 0;
        }
        return 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", MoMoMove);

u32 GetDeckDataAdjust(s32, CardGameWork *, s32);
static inline CardPlaySide *kabeSide(s32 side, CardGameWork *work)
{
    if (side == 0) {
        return &work->player;
    }
    return &work->enemy;
}

s32 CheckFriendKabe(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    s32 i;

    ownSide = kabeSide(side, work);
    for (i = 0; i < 4; i++) {
        if (ownSide->disposal[i].battle.layers[0].cardId >= 0 &&
            GetDeckDataAdjust(side, work, ownSide->disposal[i].battle.layers[0].cardId) == 2) {
            return 1;
        }
    }
    for (i = 0; i < 4; i++) {
        if (ownSide->battle[i].battle.layers[0].cardId >= 0 &&
            GetDeckDataAdjust(side, work, ownSide->battle[i].battle.layers[0].cardId) == 2) {
            return 1;
        }
    }
    return 0;
}

u32 GetDeckDataAdjust(s32, CardGameWork *, s32);
static inline CardPlaySide *pickSide(s32 side, CardGameWork *work)
{
    if (side == 0) {
        return &work->player;
    }
    return &work->enemy;
}

s32 CheckFriendSensi(s32 side, CardGameWork *work)
{
    s16 *cardIdPtr;
    s32 i;

    cardIdPtr = &pickSide(side, work)->disposal[0].battle.layers[0].cardId;
    for (i = 0; i < 4; i++) {
        s16 cardId = *cardIdPtr;

        cardIdPtr += 0xDE;
        if (cardId >= 0) {
            if (GetDeckDataAdjust(side, work, cardId) > 2) {
                return 1;
            }
        }
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_SensiMove);

static inline s32 haibiMove(CardPlaySide *sideData, s32 k, s32 slot)
{
    CGPMoveDSBATTLEwork((CardMoveWork *)&sideData->battle[k].battle,
                        (CardMoveWork *)&sideData->disposal[slot].battle);
    return 1;
}

s32 CC_Kara_HaibiKabeMove(s32 side, CardGameWork *work, s32 slot)
{
    CardPlaySide *sideData = &work->player;
    s16 cardId;

    if (side) {
        sideData = &work->enemy;
    }
    if (slot & 0xF0000) {
        goto done;
    }
    slot &= 0xFFFF;
    if (sideData->battle[3].battle.layers[0].cardId >= 0) {
        if (sideData->battle[1].battle.layers[0].cardId < 0) {
            return haibiMove(sideData, 1, slot);
        }
        if (sideData->battle[0].battle.layers[0].cardId < 0) {
            return haibiMove(sideData, 0, slot);
        }
        if (YoakimuMoMoCheck(side, work)) {
            goto done;
        }
        cardId = sideData->battle[1].battle.layers[0].cardId;
    } else {
        if (sideData->battle[2].battle.layers[0].cardId < 0) {
            goto done;
        }
        if (sideData->battle[0].battle.layers[0].cardId < 0) {
            return haibiMove(sideData, 0, slot);
        }
        if (sideData->battle[1].battle.layers[0].cardId < 0) {
            return haibiMove(sideData, 1, slot);
        }
        if (YoakimuMoMoCheck(side, work)) {
            goto done;
        }
        if (GetDeckDataAdjust(side, work, sideData->battle[0].battle.layers[0].cardId) == 2 &&
            sideData->battle[0].battle.layers[0].life < sideData->disposal[slot].battle.layers[0].life) {
            return haibiMove(sideData, 0, slot);
        }
        cardId = sideData->battle[1].battle.layers[0].cardId;
    }
    if (GetDeckDataAdjust(side, work, cardId) == 2 &&
        sideData->battle[1].battle.layers[0].life < sideData->disposal[slot].battle.layers[0].life) {
        return haibiMove(sideData, 1, slot);
    }
done:
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_KabeMove);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", EscapeMove);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", LvUpCheck);

s32 CC_Kara_MoveFase_Type2(s32 side, CardGameWork *work, s32 slot);

s32 CC_Kara_MoveFase_Type0(s32 side, CardGameWork *work, s32 slot)
{
    CardPlaySide *sideData = &work->player;
    s32 i;

    if (side) {
        sideData = &work->enemy;
    }
    if (LvUpCheck(side, work, slot) != -1) {
        return CC_Kara_MoveFase_Type2(side, work, slot);
    }
    CardPlayHandCnt(&sideData->hand);
    if (CardPlayYamaCnt(sideData) < 10 || CardPlayYamaCnt(sideData) < CheckEnemyAttack(side, work)) {
        return CC_Kara_MoveFase_Type2(side, work, slot);
    }
    if (CheckGeneration(side, work)) {
        if ((slot & 0xF0000) == 0) {
            if (CardChkAttackType(work->definitions, &sideData->disposal[slot].battle) != 1) {
                return CC_Kara_MoveFase_Type2(side, work, slot);
            }
            if (DisposeAreaCheckFriend(side, work) >= 2 && CheckFriendKakutouCnt(side, work) < 2) {
                return CC_Kara_MoveFase_Type2(side, work, slot);
            }
        } else {
            return CC_Kara_MoveFase_Type2(side, work, slot);
        }
    }
    if (slot & 0xF0000) {
        slot &= 0xFFFF;
        if (CheckGeneration(side, work) == 0) {
            for (i = 0; i < 4; i++) {
                if (sideData->disposal[i].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&sideData->disposal[i].battle,
                                        (CardMoveWork *)&sideData->battle[slot].battle);
                    return 1;
                }
            }
        }
    }
    return 0;
}

s32 CC_Kara_MoveFase_Type2(s32 side, CardGameWork *work, s32 slot);
s32 CC_Kara_MoveFase_Type3(s32 side, CardGameWork *work, s32 slot);

s32 CC_Kara_MoveFase_Type1(s32 side, CardGameWork *work, s32 slot)
{
    CardPlaySide *sideData = &work->player;
    s32 type;
    s32 i;

    if (side) {
        sideData = &work->enemy;
    }
    if (LvUpCheck(side, work, slot) != -1) {
        return CC_Kara_MoveFase_Type3(side, work, slot);
    }
    CardPlayHandCnt(&sideData->hand);
    if (CardPlayYamaCnt(sideData) < 10 || CardPlayYamaCnt(sideData) < CheckEnemyAttack(side, work)) {
        if ((slot & 0xF0000) == 0) {
            type = CardChkAttackType(work->definitions, &sideData->disposal[slot].battle);
        } else {
            type = CardChkAttackType(work->definitions, &sideData->battle[slot & 0xFFFF].battle);
        }
        if (type != 1) {
            return CC_Kara_MoveFase_Type3(side, work, slot);
        }
        return CC_Kara_MoveFase_Type2(side, work, slot);
    }
    if (CheckGeneration(side, work)) {
        return CC_Kara_MoveFase_Type3(side, work, slot);
    }
    if (slot & 0xF0000) {
        slot &= 0xFFFF;
        if (CheckGeneration(side, work) == 0) {
            for (i = 0; i < 4; i++) {
                if (sideData->disposal[i].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&sideData->disposal[i].battle,
                                        (CardMoveWork *)&sideData->battle[slot].battle);
                    return 1;
                }
            }
        }
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_MoveFase_Type2);

s32 MoMoMove(s32 side, CardGameWork *work, s32 slot);
s32 EscapeMove(s32 side, CardGameWork *work, s32 slot);
s32 CC_Kara_SensiMove(s32 side, CardGameWork *work, s32 slot);
s32 CC_Kara_KabeMove(s32 side, CardGameWork *work, s32 slot);

s32 CC_Kara_MoveFase_Type3(s32 side, CardGameWork *work, s32 slot)
{
    CardPlaySide *sideData = &work->player;
    s32 type;
    s32 idx;
    s16 cardId;

    if (side) {
        sideData = &work->enemy;
    }
    if ((slot & 0xF0000) == 0) {
        cardId = sideData->disposal[slot].battle.layers[0].cardId;
        if (cardId == 3 || cardId == 8 || cardId == 10) {
            if (MoMoMove(side, work, slot) == 1) {
                return 1;
            }
        }
    } else {
        idx = slot & 0xFFFF;
        cardId = sideData->battle[idx].battle.layers[0].cardId;
        if (cardId == 3 || cardId == 8 || cardId == 10) {
            if (MoMoMove(side, work, idx | 0xF0000) == 1) {
                return 1;
            }
        }
    }
    if (CardPlayYamaCnt(sideData) < 10 || CardPlayYamaCnt(sideData) < CheckEnemyAttack(side, work)) {
        if ((slot & 0xF0000) == 0) {
            type = CardChkAttackType(work->definitions, &sideData->disposal[slot].battle);
        } else {
            type = CardChkAttackType(work->definitions, &sideData->battle[slot & 0xFFFF].battle);
        }
        if (type == 1) {
            if ((slot & 0xF0000) == 0) {
                return CC_Kara_HaibiKabeMove(side, work, slot);
            }
            return CC_Kara_KabeMove(side, work, slot);
        }
    } else if (EscapeMove(side, work, slot) == 1) {
        return 1;
    }
    CC_Kara_SensiMove(side, work, slot);
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_MoveFase_Type4);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_MoveFase_2);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_MoveFase_1);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CheckComboMove);

INCLUDE_ASM("asm/nonmatchings/ov10/battle_area_check_enemy", CC_Kara_MoveFase);

s32 CC_Kara_1stAnswer(s32 side, CardGameWork *work)
{
    CardPlaySide *sideData;
    s32 cardIndex;
    s32 handCount;
    s16 cardId;

    sideData = tamasiiSide(side, work);
    cardIndex = 0;
    handCount = CardPlayHandCnt(&sideData->hand);
    for (; cardIndex < handCount; cardIndex++) {
        cardId = sideData->hand.cards[cardIndex];
        if ((cardId == 0x52 || cardId == 0x74) &&
            CardPlayChkCondition(work->definitions, sideData, cardId, work) == 0) {
            return 1;
        }
    }
    return 0;
}

extern int CardPlayDesertCard(CardHand *hand, int index);

s32 CC_Kara_EndFase(s32 side, CardGameWork *work)
{
    CardPlaySide *sideData;
    s32 cardIndex;
    s32 handCount;
    s16 cardId;

    sideData = tamasiiSide(side, work);
    cardIndex = 0;
    handCount = CardPlayHandCnt(&sideData->hand);
    for (; cardIndex < handCount; cardIndex++) {
        cardId = sideData->hand.cards[cardIndex];
        if (GetDeckData(side, (char *)work, cardId) == 0) {
            CardPlayDesertCard(&sideData->hand, cardIndex);
            return 1;
        }
        if (GetDeckData(side, (char *)work, cardId) == 1) {
            CardPlayDesertCard(&sideData->hand, cardIndex);
            return 1;
        }
    }
    CardPlayDesertCard(&sideData->hand, 0);
    return 1;
}

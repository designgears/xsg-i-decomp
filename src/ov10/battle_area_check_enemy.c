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

extern s32 CardPlayJunkCnt(CardPlaySide *side);
extern s32 CardPlayHandCnt(CardHand *hand);
extern s32 CardPlayYamaCnt(CardPlaySide *side);
extern s32 CheckCommandPlay(s32 side, CardGameWork *work, s32 cardId);
s32 CC_Kara_TodomeMove(s32 side, CardGameWork *work, s32 slot);
s32 GetFriendAttack(s32 side, CardGameWork *work, s32 *life, s32 *attack);
s32 CheckEnemy12Attack(s32 side, CardGameWork *work);
s32 TodomeHyouka(s32 side, CardGameWork *work, s32 a2, s32 a3, s32 *life, s32 *attack);

s32 GainanCheck(s32 side, CardGameWork *work)
{
    CardPlaySide *own;
    CardPlaySide *other;
    s32 slot;
    s32 count;
    s32 junk;
    s32 i;
    s32 j;
    s32 k;
    s32 n;
    s32 empty;
    s32 handCnt;
    u16 card;
    s32 val;
    s32 life[4];
    s32 attack[4];

    slot = -1;
    count = 0;
    if (side == 0) {
        own = &work->player;
        other = &work->enemy;
    } else {
        own = &work->enemy;
        other = &work->player;
    }
    for (i = 0; i < 4; i++) {
        if (own->disposal[i].battle.layers[0].cardId >= 0) {
            count++;
            if (own->disposal[i].battle.layers[0].cardId == 0xE && !(own->disposal[i].battle.flags & 2)) {
                slot = i;
                break;
            }
        }
    }
    for (i = 0; i < 4; i++) {
        if (own->battle[i].battle.layers[0].cardId >= 0) {
            count++;
            if (own->battle[i].battle.layers[0].cardId == 0xE && !(own->battle[i].battle.flags & 2)) {
                slot = i | 0xF0000;
                break;
            }
        }
    }
    junk = CardPlayJunkCnt(own);
    if (slot == -1) {
        return 0;
    }
    if (junk < 4 || count >= 6) {
        return 0;
    }
    if (!(slot & 0xF0000)) {
        for (i = 0; i < 4; i++) {
            if (own->battle[i].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[i].battle, (CardMoveWork *)&own->disposal[slot].battle);
                goto moved;
            }
        }
        count = 0;
    } else {
        count = 1;
    }
moved:
    for (i = 0; i < 4; i++) {
        if (own->battle[i].battle.layers[0].cardId >= 0 && own->battle[i].battle.layers[0].cardId != 0xE) {
            for (j = 0; j < 4; j++) {
                if (own->disposal[j].battle.layers[0].cardId < 0 || own->disposal[j].battle.layers[0].cardId == 0xE) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->disposal[j].battle, (CardMoveWork *)&own->battle[i].battle);
                }
            }
        }
    }
    for (i = 0; i < 3; i++) {
        for (k = 0; k < 4; k++) {
            if (own->battle[k].battle.layers[0].cardId >= 0) {
                if (own->battle[k].battle.layers[0].cardId == 0xE) {
                    slot = k;
                    break;
                }
            }
        }
        CC_Kara_TodomeMove(side, work, slot);
    }
    for (i = 0; i < 4; i++) {
        if (own->battle[i].battle.layers[0].cardId >= 0) {
            if (own->battle[i].battle.layers[0].cardId == 0xE) {
                slot = i;
                break;
            }
        }
    }
    if (junk >= 7) {
        n = 0;
        handCnt = CardPlayHandCnt(&own->hand);
        empty = 0;
        for (i = 0; i < handCnt; i++) {
            card = own->hand.cards[i];
            if ((u32)(card - 0x58) < 2) {
                if (CheckCommandPlay(side, work, (s16)card) == 1) {
                    n++;
                }
            }
        }
        for (i = 0; i < 4; i++) {
            if (other->disposal[i].battle.layers[0].cardId < 0) {
                empty++;
            }
        }
        switch (n) {
        case 0:
            if (other->battle[0].battle.layers[0].cardId < 0 && other->battle[2].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[3].battle, (CardMoveWork *)&own->battle[slot].battle);
                return 1;
            }
            if (other->battle[1].battle.layers[0].cardId < 0 && other->battle[3].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[2].battle, (CardMoveWork *)&own->battle[slot].battle);
                return 1;
            }
            break;
        case 1:
            if (other->battle[0].battle.layers[0].cardId < 0 && other->battle[2].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[3].battle, (CardMoveWork *)&own->battle[slot].battle);
                return 1;
            }
            if (other->battle[1].battle.layers[0].cardId < 0 && other->battle[3].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[2].battle, (CardMoveWork *)&own->battle[slot].battle);
                return 1;
            }
            if (empty > 0) {
                if (other->battle[0].battle.layers[0].cardId < 0 || other->battle[2].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[3].battle, (CardMoveWork *)&own->battle[slot].battle);
                    return 1;
                }
                if (other->battle[1].battle.layers[0].cardId < 0 || other->battle[3].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[2].battle, (CardMoveWork *)&own->battle[slot].battle);
                    return 1;
                }
            }
            break;
        default:
            if (other->battle[0].battle.layers[0].cardId < 0 && other->battle[2].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[3].battle, (CardMoveWork *)&own->battle[slot].battle);
                return 1;
            }
            if (other->battle[1].battle.layers[0].cardId < 0 && other->battle[3].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[2].battle, (CardMoveWork *)&own->battle[slot].battle);
                return 1;
            }
            if (empty > 0) {
                if (other->battle[0].battle.layers[0].cardId < 0 || other->battle[2].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[3].battle, (CardMoveWork *)&own->battle[slot].battle);
                    return 1;
                }
                if (other->battle[1].battle.layers[0].cardId < 0 || other->battle[3].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[2].battle, (CardMoveWork *)&own->battle[slot].battle);
                    return 1;
                }
            }
            if (empty >= 2 && CardPlayYamaCnt(own) >= 3) {
                if (other->battle[0].battle.layers[0].cardId < 0 || other->battle[2].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[3].battle, (CardMoveWork *)&own->battle[slot].battle);
                    return 1;
                }
                if (other->battle[1].battle.layers[0].cardId < 0 || other->battle[3].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[2].battle, (CardMoveWork *)&own->battle[slot].battle);
                    return 1;
                }
                return 1;
            }
            break;
        }
    }
    for (i = 0; i < 3; i++) {
        for (k = 0; k < 4; k++) {
            if (own->battle[k].battle.layers[0].cardId >= 0) {
                if (own->battle[k].battle.layers[0].cardId == 0xE) {
                    slot = k;
                    break;
                }
            }
        }
        CC_Kara_TodomeMove(side, work, slot);
    }
    GetFriendAttack(side, work, life, attack);
    if (CheckEnemy12Attack(side == 0, work) >= junk + 2) {
        return 0;
    }
    val = TodomeHyouka(side, work, 0, 0, life, attack);
    if (junk >= 7) {
        if (junk * 10 + 20 >= val) {
            return 0;
        }
    } else {
        if (val < junk * 10 + 40) {
            return 0;
        }
    }
    return 1;
}

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

extern s32 play413;
extern s32 CardPlayOperationCnt(CardPlaySide *side);
extern s32 CGPDispoSub0(s32 side, CardGameWork *work, s32 index);
extern void CardPlayDisployment(u16 side, CardGameWork *work, void *field, s32 index);
extern s32 BattleAreaCheckEnemy(s32 side, CardGameWork *work);
extern s32 DisposeAreaCheckEnemy(s32 side, CardGameWork *work);
extern s32 BattleAreaCheckFriend(s32 side, CardGameWork *work);
extern s32 DisposeAreaCheckFriend(s32 side, CardGameWork *work);
extern s32 CheckEnemyAttack(s32 side, CardGameWork *work);
static inline void setOpePlay(s32 side, CardGameWork *work, CardPlaySide *own, s32 i)
{
    own->hand.playPoints[1] -= work->definitions[own->hand.cards[i]].playCost;
    CardPlayDisployment(side, work, own, i);
}

s32 CC_Kara_SetOpe(s32 side, CardGameWork *work)
{
    CardPlaySide *own;
    s32 cnt;
    s32 flag;
    s32 dispo;
    s32 i;
    s32 cardId;
    s32 life[4];
    s32 attack[4];

    own = &work->player;
    if (side != 0) {
        own = &work->enemy;
    }
    if (CardPlayOperationCnt(own) >= 4) {
        return 0;
    }
    cnt = CardPlayHandCnt(&own->hand);
    if (play413 == 1) {
        for (i = 0; i < cnt; i++) {
            cardId = own->hand.cards[i];
            if (work->definitions[cardId].kind == 4 && cardId == 0x7D && CGPDispoSub0(side, work, i) == 0) {
                if (CardPlayYamaCnt(own) - work->definitions[0x7D].reserveCost <= 0) {
                    return 0;
                }
                setOpePlay(side, work, own, i);
                return 1;
            }
        }
    }
    flag = 0;
    i = 0;
    do {
        s16 id = *(s16 *)((char *)own + 0x1AA + i * 0x1BC);
        if (id >= 0 && (work->definitions[id].flags & 1)) {
            flag = 1;
            break;
        }
        i++;
    } while (i < 4);
    if (flag == 1) {
        for (i = 0; i < cnt; i++) {
            cardId = own->hand.cards[i];
            if (work->definitions[cardId].kind == 4 && cardId == 0x7B && CGPDispoSub0(side, work, i) == 0) {
                setOpePlay(side, work, own, i);
                return 1;
            }
        }
    }
    for (i = 0; i < cnt; i++) {
        cardId = own->hand.cards[i];
        if (work->definitions[cardId].kind != 4) {
            continue;
        }
        dispo = CGPDispoSub0(side, work, i);
        if ((cardId == 0x7B || cardId == 0x7C) && flag == 0) {
            continue;
        }
        if (cardId == 0x78 || cardId == 0x91) {
            if (CheckEnemyGuno(side, work) == 0) {
                continue;
            }
        }
        if (cardId == 0x7D) {
            s32 j;
            s32 cnt2 = 0;
            s32 has36 = 0;
            s32 enemyPow, friendPow, yama;
            s32 hyouka;

            for (j = 0; j < 4; j++) {
                s16 id = *(s16 *)((char *)own + 0x1AA + j * 0x1BC);
                if (id >= 0) {
                    if (id == 0x36) {
                        has36 = 1;
                    }
                    if (work->definitions[id].flags & 2) {
                        cnt2++;
                    }
                }
            }
            if (dispo != 0) {
                continue;
            }
            if (play413 != 0 || (has36 == 1 && cnt2 > 0)) {
                if (CardPlayYamaCnt(own) - work->definitions[cardId].reserveCost <= 0) {
                    return 0;
                }
                setOpePlay(side, work, own, i);
                return 1;
            }
            GetFriendAttack(side, work, life, attack);
            hyouka = TodomeHyouka(side, work, 0, 0, life, attack);
            enemyPow = BattleAreaCheckEnemy(side, work) + DisposeAreaCheckEnemy(side, work);
            friendPow = BattleAreaCheckFriend(side, work) + DisposeAreaCheckFriend(side, work);
            if (hyouka >= 0x32) {
                continue;
            }
            yama = CardPlayYamaCnt(own) - 1;
            if (CheckEnemyAttack(side, work) >= yama) {
                if (CardPlayYamaCnt(own) - work->definitions[cardId].reserveCost <= 0) {
                    return 0;
                }
                setOpePlay(side, work, own, i);
                return 1;
            }
            if (enemyPow < friendPow) {
                continue;
            }
            GetFriendAttack(side == 0, work, life, attack);
            if (hyouka >= TodomeHyouka(side == 0, work, 0, 0, life, attack)) {
                continue;
            }
        }
        if (dispo != 0) {
            continue;
        }
        if (CardPlayYamaCnt(own) - work->definitions[cardId].reserveCost <= 0) {
            return 0;
        }
        setOpePlay(side, work, own, i);
        return 1;
    }
    return 0;
}

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

typedef struct CGPCursorPosition {
    s16 zone;
    s16 index;
    s16 cardId;
    s16 stackedCard;
} CGPCursorPosition;
extern void CGPDisposeWeaponSub(s32 side, CardGameWork *work, CGPCursorPosition weapon, CGPCursorPosition target);
extern s32 CGPWeaponEquipChk(CardGameWork *work, s32 weaponId, s32 equipId);

s32 CC_Kara_SetWeapon(s32 side, CardGameWork *work, s32 index)
{
    CGPCursorPosition weapon;
    CGPCursorPosition dispPos;
    CGPCursorPosition battlePos;
    s32 cand[4];
    s32 life[4];
    s32 attack[4];
    CardPlaySide *other;
    CardDefinition *defs;
    CardPlaySide *own;
    s32 i;
    s32 idx;
    s32 hyouka;
    s32 kind;

    defs = work->definitions;
    if (side == 0) {
        weapon.zone = 9;
        dispPos.zone = 6;
        own = &work->player;
        battlePos.zone = 5;
        other = &work->enemy;
    } else {
        dispPos.zone = 2;
        battlePos.zone = 4;
        own = &work->enemy;
        other = &work->player;
        weapon.zone = 0;
    }
    if (CardPlayChkCondition(defs, own, own->hand.cards[index], work) != 0) {
        return 0;
    }
    weapon.index = index;
    weapon.cardId = own->hand.cards[index];
    if (weapon.cardId == 0x30 || weapon.cardId == 0x35) {
        if (BattleAreaCheckEnemy(side, work) < 2) {
            return 0;
        }
    }
    if (weapon.cardId == 0x2C) {
        for (i = 0; i < 4; i++) {
            if (own->battle[i].battle.layers[0].cardId == 0x1F) {
                if (own->battle[i].battle.layers[1].cardId < 0) {
                    battlePos.index = i;
                    battlePos.cardId = own->battle[i].battle.layers[0].cardId;
                    own->hand.playPoints[0] -= work->definitions[own->hand.cards[index]].playCost;
                    CGPDisposeWeaponSub(side, work, weapon, battlePos);
                    return 1;
                }
                if (own->battle[i].battle.layers[1].cardId == 0x2A &&
                    other->battle[0].battle.layers[0].cardId < 0 &&
                    other->battle[1].battle.layers[0].cardId < 0) {
                    battlePos.index = i;
                    battlePos.cardId = own->battle[i].battle.layers[0].cardId;
                    own->hand.playPoints[0] -= work->definitions[own->hand.cards[index]].playCost;
                    CGPDisposeWeaponSub(side, work, weapon, battlePos);
                    return 1;
                }
            }
        }
        for (i = 0; i < 4; i++) {
            if (own->disposal[i].battle.layers[0].cardId == 0x1F) {
                if (own->disposal[i].battle.layers[1].cardId < 0) {
                    dispPos.index = i;
                    dispPos.cardId = own->disposal[i].battle.layers[0].cardId;
                    own->hand.playPoints[0] -= work->definitions[own->hand.cards[index]].playCost;
                    CGPDisposeWeaponSub(side, work, weapon, dispPos);
                    return 1;
                }
                if (own->disposal[i].battle.layers[1].cardId == 0x2A) {
                    dispPos.index = i;
                    dispPos.cardId = own->disposal[i].battle.layers[0].cardId;
                    own->hand.playPoints[0] -= work->definitions[own->hand.cards[index]].playCost;
                    CGPDisposeWeaponSub(side, work, weapon, dispPos);
                    return 1;
                }
            }
        }
    }
    for (i = 0; i < 4; i++) {
        cand[i] = -1;
    }
    for (i = 0; i < 4; i++) {
        if (own->battle[i].battle.layers[0].cardId >= 0 && own->battle[i].battle.layers[1].cardId < 0) {
            battlePos.index = i;
            battlePos.cardId = own->battle[i].battle.layers[0].cardId;
            if (battlePos.cardId != 5 && battlePos.cardId != 0xC &&
                CGPWeaponEquipChk(work, weapon.cardId, battlePos.cardId) != 0 &&
                (weapon.cardId == 0x2C || weapon.cardId == 0x2F || weapon.cardId == 0x35 ||
                 defs[weapon.cardId].unmodeled_13 != defs[battlePos.cardId].unmodeled_13 ||
                 defs[weapon.cardId].attackPower > defs[battlePos.cardId].attackPower) &&
                (weapon.cardId != 0x2D || defs[battlePos.cardId].unmodeled_13 != 2)) {
                cand[i] = battlePos.cardId;
            }
        }
    }
    idx = -1;
    for (i = 3; i >= 0; i--) {
        if (cand[i] == -1) {
            continue;
        }
        if (weapon.cardId == 0x2A) {
            if (i >= 2) {
                continue;
            }
            if (i == 0 && other->battle[1].battle.layers[0].cardId < 0) {
                continue;
            }
            if (i == 1 && other->battle[0].battle.layers[0].cardId < 0) {
                continue;
            }
        }
        if (idx == -1) {
            idx = i;
        } else if (weapon.cardId == 0x35) {
            if (CheckNowPower(side, work, i | 0xF0000, cand[i]) > CheckNowPower(side, work, idx | 0xF0000, cand[idx])) {
                idx = i;
            }
        } else if ((u32)GetDeckData(side, (char *)work, cand[idx]) < 3U &&
                   (u32)GetDeckData(side, (char *)work, cand[i]) >= 3U) {
            idx = i;
        } else if (own->battle[idx].battle.layers[0].life < own->battle[i].battle.layers[0].life) {
            if ((u32)GetDeckData(side, (char *)work, cand[idx]) < 3U) {
                idx = i;
            }
        }
    }
    if (weapon.cardId == 0x35 && idx != -1) {
        if (CheckNowPower(side, work, idx | 0xF0000, cand[idx]) < 2 && BattleAreaCheckEnemy(side, work) < 3) {
            idx = -1;
        }
    }
    if (idx != -1) {
        GetFriendAttack(side, work, life, attack);
        hyouka = TodomeHyouka(side, work, 0, 0, life, attack);
        kind = work->definitions[weapon.cardId].unmodeled_13;
        if (kind == 2) {
            attack[idx] = work->definitions[weapon.cardId].attackPower;
        } else if (kind == 1) {
            attack[idx] = work->definitions[weapon.cardId].attackPower | 0x20000;
        } else if (kind == 3) {
            attack[idx] = work->definitions[weapon.cardId].attackPower | 0x40000;
        } else if (kind == 4) {
            attack[idx] = work->definitions[weapon.cardId].attackPower | 0x80000;
        }
        if (weapon.cardId == 0x35) {
            attack[idx] = CheckNowPower(side, work, idx | 0xF0000, cand[idx]) | 0x40000;
        }
        if (weapon.cardId == 0x2B || weapon.cardId == 0x2D || weapon.cardId == 0x2E) {
            if (cand[idx] == 0x24) {
                attack[idx] = 0x20000;
            }
            attack[idx] &= 0xFFFF0000;
        }
        if (own->battle[idx].battle.layers[0].cardId == 0x20 || own->battle[idx].battle.layers[0].cardId == 0x4C) {
            attack[idx] |= 0x100000;
        }
        if (TodomeHyouka(side, work, 0, 0, life, attack) >= hyouka && !(own->battle[idx].battle.flags & 2)) {
            battlePos.index = idx;
            battlePos.cardId = own->battle[idx].battle.layers[0].cardId;
            own->hand.playPoints[0] -= work->definitions[own->hand.cards[index]].playCost;
            CGPDisposeWeaponSub(side, work, weapon, battlePos);
            return 1;
        }
    }
    for (i = 0; i < 4; i++) {
        cand[i] = -1;
    }
    for (i = 0; i < 4; i++) {
        if (own->disposal[i].battle.layers[0].cardId >= 0 && own->disposal[i].battle.layers[1].cardId < 0) {
            dispPos.index = i;
            dispPos.cardId = own->disposal[i].battle.layers[0].cardId;
            if (dispPos.cardId != 5 && dispPos.cardId != 0xC &&
                CGPWeaponEquipChk(work, weapon.cardId, dispPos.cardId) != 0 &&
                (weapon.cardId == 0x2C || weapon.cardId == 0x2F || weapon.cardId == 0x35 ||
                 defs[weapon.cardId].unmodeled_13 != defs[dispPos.cardId].unmodeled_13 ||
                 defs[weapon.cardId].attackPower > defs[dispPos.cardId].attackPower) &&
                weapon.cardId != 0x2A && weapon.cardId != 0x2D) {
                cand[i] = dispPos.cardId;
            }
        }
    }
    idx = -1;
    for (i = 0; i < 4; i++) {
        if (cand[i] == -1) {
            continue;
        }
        if (idx == -1) {
            idx = i;
        } else if (weapon.cardId == 0x35) {
            if (work->definitions[cand[idx]].attackPower < work->definitions[cand[i]].attackPower) {
                idx = i;
            }
        } else if ((u32)GetDeckData(side, (char *)work, cand[idx]) < 3U &&
                   (u32)GetDeckData(side, (char *)work, cand[i]) >= 3U) {
            idx = i;
        } else if (own->disposal[idx].battle.layers[0].life < own->disposal[i].battle.layers[0].life) {
            if ((u32)GetDeckData(side, (char *)work, cand[idx]) < 3U) {
                idx = i;
            }
        }
    }
    if (idx != -1) {
        if (weapon.cardId == 0x35 && work->definitions[cand[idx]].attackPower < 2) {
            return 0;
        }
        dispPos.index = idx;
        dispPos.cardId = own->disposal[idx].battle.layers[0].cardId;
        own->hand.playPoints[0] -= work->definitions[own->hand.cards[index]].playCost;
        CGPDisposeWeaponSub(side, work, weapon, dispPos);
        return 1;
    }
    return 0;
}

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

typedef struct GunoMove {
    u8 unmodeled_00[4];
    float position[3];
    float step[3];
} GunoMove;
typedef struct GunoSlot {
    float shuntPosition[3];
    CardLayerStackView battle;
} GunoSlot;
typedef struct GunoField {
    CardHand hand;
    u8 unmodeled_058[0x190 - 0x58];
    GunoSlot battle[4];
    GunoSlot disposal[4];
    u8 unmodeled_F70[0x192C - 0xF70];
    GunoMove handMove[40];
} GunoField;
extern float BattlePos1P[][3];
extern int CardPlayCostCard(CardHand *hand);
extern void CardPlayCleanHand(CardHand *hand);

int SetGunoFinish(s32 side, CardGameWork *work, s32 index)
{
    s32 life[4];
    s32 attack[4];
    GunoField *own;
    CardDefinition *defs;
    s32 id;
    s32 i;
    s32 best;
    s32 bestIdx;
    s32 score;
    s32 power;
    s32 n;

    own = (GunoField *)&work->player;
    if (side != 0) {
        own = (GunoField *)&work->enemy;
    }
    id = own->hand.cards[index];
    best = -1;
    bestIdx = -1;
    defs = work->definitions;
    power = defs[id].attackPower;
    if (defs[id].unmodeled_13 == 1) {
        for (i = 0; i < 4; i++) {
            if (own->battle[i].battle.layers[0].cardId < 0) {
                GetFriendAttack(side, work, life, attack);
                attack[i] = power + 0x20000;
                if (id == 0x43) {
                    attack[i] = (power + 0x20000) | 0x10000;
                }
                score = TodomeHyouka(side, work, i, i, life, attack);
                if (best < score) {
                    bestIdx = i;
                    best = score;
                }
            }
        }
    } else if (defs[id].unmodeled_13 == 2) {
        for (i = 3; i >= 0; i--) {
            if (own->battle[i].battle.layers[0].cardId < 0) {
                GetFriendAttack(side, work, life, attack);
                attack[i] = power;
                score = TodomeHyouka(side, work, i, i, life, attack);
                if (best < score) {
                    bestIdx = i;
                    best = score;
                }
            }
        }
    } else {
        for (i = 3; i >= 0; i--) {
            if (own->battle[i].battle.layers[0].cardId < 0) {
                bestIdx = i;
                break;
            }
        }
    }
    if (bestIdx != -1) {
        own->battle[bestIdx].battle.flags = 0;
        own->battle[bestIdx].battle.power = work->definitions[id].basePower;
        own->battle[bestIdx].battle.powerLimit = work->definitions[id].basePower;
        own->battle[bestIdx].battle.layers[0].cardId = own->hand.cards[index];
        own->battle[bestIdx].battle.layers[0].life = work->definitions[id].operationDuration;
        own->battle[bestIdx].battle.position[0] = own->handMove[index].position[0];
        own->battle[bestIdx].battle.position[1] = own->handMove[index].position[1];
        own->battle[bestIdx].battle.position[2] = own->handMove[index].position[2];
        own->battle[bestIdx].battle.step[0] = (BattlePos1P[bestIdx][0] - own->handMove[index].position[0]) / 10.0f;
        own->battle[bestIdx].battle.step[1] = (BattlePos1P[bestIdx][1] - own->handMove[index].position[1]) / 10.0f;
        own->battle[bestIdx].battle.step[2] = (BattlePos1P[bestIdx][2] - own->handMove[index].position[2]) / 10.0f;
        own->battle[bestIdx].battle.moveFrames = 10;
    } else {
        for (i = 0; i < 4; i++) {
            if (own->disposal[i].battle.layers[0].cardId < 0) {
                own->disposal[i].battle.flags = 0;
                own->disposal[i].battle.power = work->definitions[id].basePower;
                own->disposal[i].battle.powerLimit = work->definitions[id].basePower;
                own->disposal[i].battle.layers[0].cardId = own->hand.cards[index];
                own->disposal[i].battle.layers[0].life = work->definitions[id].operationDuration;
                own->disposal[i].battle.position[0] = own->handMove[index].position[0];
                own->disposal[i].battle.position[1] = own->handMove[index].position[1];
                own->disposal[i].battle.position[2] = own->handMove[index].position[2];
                own->disposal[i].battle.step[0] = (BattlePos1P[bestIdx][0] - own->handMove[index].position[0]) / 10.0f;
                own->disposal[i].battle.step[1] = (BattlePos1P[bestIdx][1] - own->handMove[index].position[1]) / 10.0f;
                own->disposal[i].battle.step[2] = (BattlePos1P[bestIdx][2] - own->handMove[index].position[2]) / 10.0f;
                own->disposal[i].battle.moveFrames = 10;
                break;
            }
        }
    }
    n = work->definitions[id].reserveCost;
    own->hand.cards[index] = -1;
    own->hand.playPoints[0] -= work->definitions[id].playCost;
    for (i = 0; i < n; i++) {
        CardPlayCostCard(&own->hand);
    }
    CardPlayCleanHand(&own->hand);
    return 1;
}

extern u8 CardDat[0x100];
s32 GetPrice(CardLayerStack *stack);
s32 GetShoukin(CardLayerStack *stack);
int CardPlayDesertBattle(CardHand *hand, CardLayerStack *card);
int SetGunoFinish(s32, CardGameWork *, s32);

s32 CC_Kara_SetGuno(s32 side, CardGameWork *work, s32 index)
{
    s32 i;
    s32 nonzero;
    CardPlaySide *own;
    CardLayerStack *st;
    s32 disposalIds[4];
    s32 battleIds[4];
    s16 cardId;
    u8 price;
    s32 count;
    u8 *p;

    nonzero = 0;
    own = &work->player;
    if (side != 0) {
        own = &work->enemy;
    }
    cardId = own->hand.cards[index];
    price = CardDat[cardId];
    p = (u8 *)((cardId * 0x24) + (int)work->definitions) + 0xC;
    i = 3;
    do {
        if (*p != 0) {
            nonzero++;
        }
        p++;
        i--;
    } while (i >= 0);

    count = 0;
    for (i = 0; i < 4; i++) {
        disposalIds[i] = -1;
        if (own->disposal[i].battle.layers[0].cardId >= 0 &&
            (work->definitions[own->disposal[i].battle.layers[0].cardId].flags & 1) &&
            !(own->disposal[i].battle.flags & 1) && GetPrice(&own->disposal[i].battle) < price) {
            count++;
            disposalIds[i] = own->disposal[i].battle.layers[0].cardId;
        }
    }
    for (i = 0; i < 4; i++) {
        battleIds[i] = -1;
        if (own->battle[i].battle.layers[0].cardId >= 0 &&
            (work->definitions[own->battle[i].battle.layers[0].cardId].flags & 1) &&
            GetShoukin(&own->battle[i].battle) < price) {
            count++;
            battleIds[i] = own->battle[i].battle.layers[0].cardId;
        }
    }
    if (count < nonzero) {
        return 0;
    }

    if (BattleAreaCheckFriend(side, work) == 4) {
        for (i = 0; i < 4; i++) {
            if (battleIds[i] != -1) {
                CardPlayDesertBattle(&own->hand, &own->battle[i].battle);
                nonzero--;
            }
            if (nonzero <= 0) {
                SetGunoFinish(side, work, index);
                return 1;
            }
        }
        for (i = 0; i < 4; i++) {
            if (disposalIds[i] != -1) {
                CardPlayDesertBattle(&own->hand, &own->disposal[i].battle);
                nonzero--;
            }
            if (nonzero <= 0) {
                SetGunoFinish(side, work, index);
                return 1;
            }
        }
    } else {
        for (i = 0; i < 4; i++) {
            if (disposalIds[i] != -1) {
                CardPlayDesertBattle(&own->hand, &own->disposal[i].battle);
                nonzero--;
            }
            if (nonzero <= 0) {
                SetGunoFinish(side, work, index);
                return 1;
            }
        }
        for (i = 0; i < 4; i++) {
            if (battleIds[i] != -1) {
                CardPlayDesertBattle(&own->hand, &own->battle[i].battle);
                nonzero--;
            }
            if (nonzero <= 0) {
                SetGunoFinish(side, work, index);
                return 1;
            }
        }
    }
    return 0;
}

void CC_Kara_CommPlay(s32 side, CardGameWork *work);
extern s32 CGPEnemyLv10SetSub(s32 side, CardGameWork *work, s32 index);
extern s32 CGPChkCardPlayCost(CardGameWork *work, CardHand *hand, s32 cardId);

int CC_Kara_SetFase_1(int side, u8 *a1)
{
    s32 ids[40];
    CGPCursorPosition weapon;
    CGPCursorPosition dispPos;
    CGPCursorPosition battlePos;
    CardGameWork *work;
    CardDefinition *defs;
    CardPlaySide *own;
    CardPlaySide *other;
    s32 i;
    s32 n;
    s32 yama;
    s32 cost;
    s32 flag;
    s32 opeIdx;
    s32 gunoIdx;
    s32 a;
    s32 b;
    s32 id;

    work = (CardGameWork *)a1;
    CC_Kara_SetOpe(side, work);
    defs = work->definitions;
    if (side == 0) {
        weapon.zone = 9;
        dispPos.zone = 6;
        own = &work->player;
        battlePos.zone = 5;
        other = &work->enemy;
    } else {
        dispPos.zone = 2;
        battlePos.zone = 4;
        own = &work->enemy;
        other = &work->player;
        weapon.zone = 0;
    }
    for (i = 0; i < 40; i++) {
        ids[i] = -1;
    }
    n = CardPlayHandCnt(&own->hand);
    for (i = 0; i < n; i++) {
        if (defs[own->hand.cards[i]].kind == 1) {
            ids[i] = CGPDispoSub0(side, work, i);
        }
    }
    yama = CardPlayYamaCnt(own);
    if (yama >= 5) {
        for (i = 0; i < n; i++) {
            id = own->hand.cards[i];
            if (id == 7 || id == 9 || id == 10 || id == 11 || id == 12 || id == 13) {
                if (CGPEnemyLv10SetSub(side, work, i) == 0) {
                    return 1;
                }
            }
        }
    }
    if (yama < 10) {
        for (i = 0; i < n; i++) {
            if (ids[i] != -1) {
                cost = CGPChkCardPlayCost(work, &own->hand, own->hand.cards[i]);
                if (cost == 1) {
                    if (yama - 1 < 4) {
                        ids[i] = -1;
                    }
                } else if (cost != 0 && yama - cost < 6) {
                    ids[i] = -1;
                }
            }
        }
    }
    flag = 0;
    for (i = 0; i < 4; i++) {
        if (own->disposal[i].battle.layers[0].cardId < 0) {
            flag = 1;
        }
    }
    if (flag == 0) {
        for (i = 0; i < n; i++) {
            if ((u32)(ids[i] - 13) >= 3) {
                ids[i] = -1;
            }
        }
    }
    opeIdx = -1;
    gunoIdx = -1;
    for (i = 0; i < n; i++) {
        if (ids[i] == -1) {
            continue;
        }
        switch (ids[i]) {
        case 0:
            if (opeIdx == -1) {
                opeIdx = i;
            } else {
                a = own->hand.cards[i];
                b = own->hand.cards[opeIdx];
                if (CardDat[b] < CardDat[a]) {
                    opeIdx = i;
                }
            }
            break;
        case 15:
            if (gunoIdx == -1) {
                gunoIdx = i;
                if (VX10000Check(side, work) == 1 &&
                    work->definitions[own->hand.cards[i]].unmodeled_13 == 4) {
                    gunoIdx = -1;
                    continue;
                }
            }
            a = own->hand.cards[i];
            b = own->hand.cards[gunoIdx];
            if (CardDat[b] < CardDat[a]) {
                gunoIdx = i;
            }
            if (own->hand.cards[gunoIdx] == 0x43 && n < 8 &&
                other->battle[0].battle.layers[0].cardId < 0 &&
                other->battle[1].battle.layers[0].cardId < 0) {
                gunoIdx = -1;
            }
            break;
        }
    }
    b = 0;
    a = 0;
    if (opeIdx != -1) {
        b = CardDat[own->hand.cards[opeIdx]];
    }
    if (gunoIdx != -1) {
        a = CardDat[own->hand.cards[gunoIdx]];
    }
    for (i = 0; i < n; i++) {
        if (ids[i] != -1 && ids[i] < 15 && ids[i] >= 13 && own->hand.cards[i] == 0x35) {
            if (CC_Kara_SetWeapon(side, work, i) == 1) {
                return 1;
            }
        }
    }
    if (a < b) {
        if (opeIdx != -1) {
            own->hand.playPoints[0] -= work->definitions[own->hand.cards[opeIdx]].playCost;
            CardPlayDisployment(side, work, own, opeIdx);
            return 1;
        }
        if (gunoIdx != -1 && AlbedoCheck(side, work) == -1 && CC_Kara_SetGuno(side, work, gunoIdx) == 1) {
            return 1;
        }
    } else {
        if (gunoIdx != -1 && AlbedoCheck(side, work) == -1 && CC_Kara_SetGuno(side, work, gunoIdx) == 1) {
            return 1;
        }
        if (opeIdx != -1) {
            own->hand.playPoints[0] -= work->definitions[own->hand.cards[opeIdx]].playCost;
            CardPlayDisployment(side, work, own, opeIdx);
            return 1;
        }
    }
    for (i = 0; i < n; i++) {
        if (ids[i] != -1 && ids[i] < 15 && ids[i] >= 13) {
            if (own->hand.cards[i] == 0x2D && VX10000Check(side, work) == 1) {
                continue;
            }
            if (CC_Kara_SetWeapon(side, work, i) == 1) {
                return 1;
            }
        }
    }
    for (i = 0; i < n; i++) {
        id = own->hand.cards[i];
        if (id == 7 || id == 9 || id == 10 || id == 11 || id == 12 || id == 13) {
            if (CGPEnemyLv10SetSub(side, work, i) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

extern s32 cardno_0;
extern s32 no_1;
extern s32 CardPlaySuteCnt(CardPlaySide *side);
extern s32 CardPlayCommandPlay(u16 side, u16 command, CardGameWork *work, s16 operand, s16 target);
extern void CGPCommFinishSub(CardGameWork *work, CardPlaySide *side, s32 index);

s32 CC_Kara_CommFase(s32 side, CardGameWork *work)
{
  CardPlaySide *sideData_t;
  s16 **new_var;
  s32 i;
  s32 new_var2;
  s32 count;
  s16 *card;
  sideData_t = &work->player;
  i = 0;
  if (!side)
  {
  }
  else
  {
    sideData_t = &work->enemy;
    i = 0;
  }
  count = CardPlayHandCnt(&sideData_t->hand);
  switch (work->unmodeled_59B8[1])
  {
    case 0:
      i = 0;
      if (i < count)
      do
    {
      new_var = &card;
      card = &sideData_t->hand.cards[i];
      switch (*card)
      {
        case 0x56:

        case 0x57:
          if (CheckCommandPlay(side, work, *card) && (CardPlaySuteCnt(sideData_t) >= 5))
        {
          no_1 = i;
          cardno_0 = *(*new_var);
          work->unmodeled_59B8[1] = 1;
          return 0;
          work->unmodeled_59B8[1] = 0;
        }
          break;

        case 0x5D:
          if ((CheckCommandPlay(side, work, 0x5D) && (CardPlayYamaCnt(sideData_t) >= 11)) && (7 > count))
        {
          new_var2 = i;
          no_1 = new_var2;
          cardno_0 = *card;
          work->unmodeled_59B8[1] = 1;
          return 0;
        }
          break;

        case 0x5E:
          new_var2 = count;
          if ((CheckCommandPlay(side, work, 0x5E) && (CardPlaySuteCnt(sideData_t) >= 2)) && (new_var2 < 7))
        {
          cardno_0 = *card;
          no_1 = new_var2;
          no_1 = i;
          work->unmodeled_59B8[1] = 1;
          return 0;
        }
          break;

      }

      i++;
    }
    while (i < count);
      return 1;

    case 1:
      if (CardPlayCommandPlay(side, work->commandIds[cardno_0], work, 0, 0) == 0)
    {
      return 0;
    }
      CGPCommFinishSub(work, sideData_t, no_1);
      work->unmodeled_59B8[1] = 0;
      return 0;

    default:
      return 1;

  }

}

s32 CheckDamageCommand(s32 side, CardGameWork *work, s32 kind)
{
  CardPlaySide *ownSide;
  CardHand *hand;
  s32 life[4];
  s32 attack[4];
  s32 i;
  s32 count;
  unsigned long new_var2;
  s32 cnt;
  s32 before;
  s32 best;
  s32 found;
  s32 v;
  s32 t;
  s16 *extra;
  s32 k3;
  s32 new_var;
  s32 id139;
  CardGameWork *w2;
  s32 sd2;
  s32 z0;
  found = -1;
  if (side == 0)
  {
    hand = &work->player.hand;
    ownSide = &work->enemy;
  }
  else
  {
    hand = &work->enemy.hand;
    ownSide = &work->player;
  }
  count = 0;
  cnt = CardPlayHandCnt(hand);
  if (cnt > 0)
  {
    s16 *cp = hand->cards;
    i = cnt;
    do
    {
      u16 id = (u16) (*cp);
      cp++;
      if (((u32) (id - 0x5A)) < 2U)
      {
        if (CheckCommandPlay(side, work, (s32) ((s16) id)) == 1)
        {
          count = count + 2;
        }
      }
      i--;
    }
    while (i != 0);
  }
  extra = (s16 *) (((char *) hand) + 0xF7C);
  for (i = 0; i < 4; i++)
  {
    s16 id = *extra;
    extra += 1;
    if (id == 0x8B)
    {
      count = count + 3;
    }
  }

  kind = (kind < count) ? (count) : (kind);
  if ((((s16) work->command.phase) == 2) || (((s16) work->command.phase) == 9))
  {
    s32 tt;
    CardGameWork *w;
    s32 sd;
    GetFriendAttack(side, work, life, attack);
    best = TodomeHyouka(side, work, 0, 0, life, attack);
    before = best;
    for (i = 0; i <= 3; i++)
    {
      if (ownSide->battle[i].battle.layers[0].cardId >= 0)
      {
        sd = side;
        GetFriendAttack(sd, work, life, attack);
        tt = life[i] - kind;
        if (tt > 0)
        {
          v = (s32) ((((f32) GetShoukin(&ownSide->battle[i].battle)) / 10.0f) * ((f32) kind));
          life[i] -= kind;
        }
        else
        {
          v = GetShoukin(&ownSide->battle[i].battle) - tt;
          life[i] = 0;
        }
        v = v + TodomeHyouka(sd, work, 0, 0, life, attack);
        if (v > best)
        {
          best = v;
          found = i | 0xF0000;
        }
      }
    }

    if ((before + (kind * 13)) < best)
    {
      return found;
    }
    return -1;
  }
  best = 0;
  new_var2 = 1;
  z0 = best;
  for (i = 0; i <= 3; i++)
  {
    if (0 <= ownSide->battle[i].battle.layers[0].cardId)
    {
      s32 vb;
      sd2 = side;
      new_var = best;
      GetFriendAttack(sd2, work, life, attack);
      t = life[i] - kind;
      if (0 < t)
      {
        vb = (s32) ((((f32) GetShoukin(&ownSide->battle[i].battle)) / 10.0f) * ((f32) kind));
        life[i] = life[i] - kind;
      }
      else
      {
        vb = GetShoukin(&ownSide->battle[i].battle) - t;
        life[i] = 0;
      }
      if (vb > new_var)
      {
        best = vb;
        found = i | 0xF0000;
      }
    }
  }

  {
    s32 lim = 10 * kind;
    k3 = z0 != (best > lim);
    return (k3) ? (found) : (-new_var2);
  }
}

s32 TodomeHyouka(s32 side, CardGameWork *work, s32 a2, s32 a3, s32 *life, s32 *attack);
s32 GetFriendAttack(s32 side, CardGameWork *work, s32 *life, s32 *attack);

s32 CheckTaikyakuCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    CardHand *hand;
    s32 life[4];
    s32 attack[4];
    s32 found = -1;
    s32 best;
    s32 before;
    s32 i;
    s32 n;
    s32 cnt;
    s32 albedo;
    s32 t;

    if (side == 0) {
        hand = &work->player.hand;
        ownSide = &work->enemy;
    } else {
        hand = &work->enemy.hand;
        ownSide = &work->player;
    }
    albedo = AlbedoCheck(side, work);
    if (-1 != albedo) {
        n = CardPlayHandCnt(hand);
        cnt = 0;
        for (i = 0; i < n; i++) {
            u16 id = hand->cards[i];
            if ((u32)(id - 0x58) < 2U) {
                s32 r = CheckCommandPlay(side, work, (s16)id);
                s32 nxt = cnt + 1;
                cnt = (r == 1) ? nxt : cnt;
            }
        }
        if (2 <= cnt) {
            switch (albedo) {
            case 0:
            case 2:
                if (ownSide->battle[1].battle.layers[0].cardId >= 0) {
                    found = 1;
                } else {
                    found = (ownSide->battle[3].battle.layers[0].cardId >= 0) ? 3 : found;
                }
                return found;
            case 1:
            case 3:
                if (ownSide->battle[0].battle.layers[0].cardId >= 0) {
                    found = 0;
                } else {
                    found = (ownSide->battle[2].battle.layers[0].cardId >= 0) ? 2 : found;
                }
                return found;
            }
        }
    }
    if ((s16)work->command.phase != 0xB && (s16)work->command.phase != 4) {
        GetFriendAttack(side, work, life, attack);
        before = TodomeHyouka(side, work, 0, 0, life, attack);
        best = before;
        i = 0;
        goto phase_a_test;
    }
    GetFriendAttack(side == 0, work, life, attack);
    before = TodomeHyouka(side == 0, work, 0, 0, life, attack);
    best = before;
    i = 0;
    do {
        if (ownSide->battle[i].battle.layers[0].cardId >= 0) {
            GetFriendAttack(side == 0, work, life, attack);
            attack[i] = 0;
            t = TodomeHyouka(side == 0, work, 0, 0, life, attack);
            if (t < best) {
                best = t;
                found = i | 0xF0000;
            }
        }
        i++;
    } while (i < 4);
    if (before < best + 0x1E) {
        found = -1;
    }
    goto ret;

phase_a_body:
    if (ownSide->battle[i].battle.layers[0].cardId >= 0) {
        GetFriendAttack(side, work, life, attack);
        life[i] = 0;
        t = TodomeHyouka(side, work, 0, 0, life, attack);
        if (best < t) {
            best = t;
            found = i | 0xF0000;
        }
    }
    i++;
phase_a_test:
    if (i < 4) {
        goto phase_a_body;
    }
    if (best < before + 0x1E) {
        found = -1;
    }
ret:
    return found;
}

s32 TodomeHyouka(s32 side, CardGameWork *work, s32 a2, s32 a3, s32 *life, s32 *attack);
s32 GetFriendAttack(s32 side, CardGameWork *work, s32 *life, s32 *attack);

s32 CheckTotugekiCommand(s32 side, CardGameWork *work)
{
  CardPlaySide *otherSide;
  s32 life[4];
  s32 attack[4];
  s32 i;
  s32 best;
  s32 found;
  s32 base;
  s32 *new_var;
  s32 cur;
  i = 0;
  found = -1;
  otherSide = &work->enemy;
  if (side)
  {
    otherSide = &work->player;
  }
  if (((*((s16 *) (&work->command.phase))) == 2) || ((*((s16 *) (&work->command.phase))) == 9))
  {
    return -1;
  }
  GetFriendAttack(side == 0, work, life, attack);
  best = TodomeHyouka(side == 0, work, 0, 0, life, attack);
  base = best;
  i = 0;
  for (; i < 4; i++)
  {
    new_var = &i;
    if (otherSide->battle[*new_var].battle.layers[0].cardId >= 0)
    {
      GetFriendAttack(side == 0, work, life, attack);
      if ((cur = TodomeHyouka(side == 0, work, 0, 0, life, attack)) < best)
      {
        best = cur;
        found = *new_var;
      }
    }
  }

  if ((best + 30) < base)
  {
    found = -1;
  }
  return found;
}

typedef struct { s32 w[0x6F]; } KonranSlot;
typedef struct {
    u8 unmodeled_000[0x19C];
    KonranSlot slots[4];
} KonranSide;

s32 CheckKonranCommand(s32 side, CardGameWork *work)
{
    KonranSide *own;
    s32 life[4];
    s32 attack[4];
    KonranSlot tmp[4];
    s32 i;
    s32 h;
    s32 t;

    i = 0;
    own = (KonranSide *)&work->enemy;
    if (side) {
        own = (KonranSide *)&work->player;
    }
    if ((s16)work->command.phase != 2 && (s16)work->command.phase != 9) {
        GetFriendAttack(side == 0, work, life, attack);
        h = TodomeHyouka(side == 0, work, 0, 0, life, attack);
        goto b_test;
    }
    GetFriendAttack(side, work, life, attack);
    h = TodomeHyouka(side, work, 0, 0, life, attack);
    for (i = 0; i < 4; i++) {
        tmp[i] = own->slots[i];
    }
    own->slots[0] = tmp[2];
    own->slots[1] = tmp[3];
    own->slots[2] = tmp[0];
    own->slots[3] = tmp[1];
    GetFriendAttack(side, work, life, attack);
    t = TodomeHyouka(side, work, 0, 0, life, attack);
    for (i = 0; i < 4; i++) {
        own->slots[i] = tmp[i];
    }
    if (t < h + 0x1E) {
        return 0;
    }
    return 1;

b_body:
    tmp[i] = own->slots[i];
    i++;
b_test:
    if (i < 4) {
        goto b_body;
    }
    own->slots[0] = tmp[2];
    own->slots[1] = tmp[3];
    own->slots[2] = tmp[0];
    own->slots[3] = tmp[1];
    GetFriendAttack(side == 0, work, life, attack);
    t = TodomeHyouka(side == 0, work, 0, 0, life, attack);
    for (i = 0; i < 4; i++) {
        own->slots[i] = tmp[i];
    }
    if (h < t + 0x1E) {
        return 0;
    }
    return 1;
}

s32 TodomeHyouka(s32 side, CardGameWork *work, s32 a2, s32 a3, s32 *life, s32 *attack);
s32 GetFriendAttack(s32 side, CardGameWork *work, s32 *life, s32 *attack);

s32 CheckCurryCommand(s32 side, CardGameWork *work)
{
  CardPlaySide *ownSide = &work->player;
  s32 life[4];
  s32 attack[4];
  s32 i;
  s32 j;
  s32 before = 0;
  s32 best;
  s32 cur;
  s32 found = -1;
  i = 0;
  if (side)
  {
    ownSide = &work->enemy;
  }
  if (((*((s16 *) (&work->command.phase))) == 2) || ((*((s16 *) (&work->command.phase))) == 9))
  {
    return -1;
  }
  GetFriendAttack(side == 0, work, life, attack);
  for (i = before; i < 4; i++)
  {
    attack[i] &= ~0x10000;
  }

  before = TodomeHyouka(side == 0, work, 0, 0, life, attack);
  best = before;
  for (i = 0; i < 4; i++)
  {
    if (((ownSide->battle[i].battle.layers[0].cardId >= 0) && ((work->definitions[ownSide->battle[i].battle.layers[0].cardId].operationDuration - ownSide->battle[i].battle.layers[0].life) >= 2)) && (work->definitions[ownSide->battle[i].battle.layers[0].cardId].flags != 4))
    {
      GetFriendAttack(side == 0, work, life, attack);
      for (j = 0; j < 4; j++)
      {
        attack[j] &= 0xF0FFFF;
      }

      life[i] = work->definitions[ownSide->battle[i].battle.layers[0].cardId].operationDuration;
      cur = TodomeHyouka(side == 0, work, 0, 0, life, attack);
      if (cur < best)
      {
        best = cur;
        found = i | 0xF0000;
      }
    }
  }

  if ((best + 15) <= before)
  {
    return found;
  }
  return -1;
}

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

double litodp(s32 value);
s32 dptoli(double value);
double dpadd(double, double);
double dpmul(double, double);
double dpdiv(double, double);

s32 CheckXDamageCommand(s32 side, CardGameWork *work, s32 threshold)
{
    s32 sp0;
    char *sp4;
    char *var_30;
    char *var_23;
    char *var_22;
    s32 var_19;
    s32 var_20;
    s32 var_21;
    s32 temp_2;
    s32 price;
    double a;
    double b;
    double c;

    var_20 = 0;
    var_19 = 0;
    if (side == 0) {
        var_30 = (char *)work + 0x45C;
        sp4 = (char *)work + 0x2F08;
    } else {
        var_30 = (char *)work + 0x2F08;
        sp4 = (char *)work + 0x45C;
    }
    var_21 = 0;
    var_23 = sp4 + 0x190;
    var_22 = var_30 + 0x190;
    sp0 = 3;
    do {
        if (*(s16 *)(var_22 + 0x70A) >= 0) {
            if (*(s8 *)(var_22 + 0x707) - threshold <= 0) {
                var_19 += GetPrice((CardLayerStack *)(var_30 + var_21 + 0x88C));
            } else {
                price = GetPrice((CardLayerStack *)(var_30 + var_21 + 0x88C));
                a = litodp(var_19);
                c = litodp(price);
                b = dpdiv(c, 10.0);
                var_19 = dptoli(dpadd(a, dpmul(b, litodp(threshold))));
            }
        }
        if (*(s16 *)(var_23 + 0x70A) >= 0) {
            if (*(s8 *)(var_23 + 0x707) - threshold <= 0) {
                var_20 += GetPrice((CardLayerStack *)(sp4 + var_21 + 0x88C));
            } else {
                price = GetPrice((CardLayerStack *)(sp4 + var_21 + 0x88C));
                a = litodp(var_20);
                c = litodp(price);
                b = dpdiv(c, 10.0);
                var_20 = dptoli(dpadd(a, dpmul(b, litodp(threshold))));
            }
        }
        if (*(s16 *)(var_22 + 0x1A) >= 0) {
            if (*(s8 *)(var_22 + 0x17) - threshold <= 0) {
                var_19 += GetShoukin((CardLayerStack *)(var_30 + var_21 + 0x19C));
            } else {
                price = GetShoukin((CardLayerStack *)(var_30 + var_21 + 0x19C));
                a = litodp(var_19);
                c = litodp(price);
                b = dpdiv(c, 10.0);
                var_19 = dptoli(dpadd(a, dpmul(b, litodp(threshold))));
            }
        }
        if (*(s16 *)(var_23 + 0x1A) >= 0) {
            if (*(s8 *)(var_23 + 0x17) - threshold <= 0) {
                var_20 += GetShoukin((CardLayerStack *)(sp4 + var_21 + 0x19C));
            } else {
                price = GetShoukin((CardLayerStack *)(sp4 + var_21 + 0x19C));
                a = litodp(var_20);
                c = litodp(price);
                b = dpdiv(c, 10.0);
                var_20 = dptoli(dpadd(a, dpmul(b, litodp(threshold))));
            }
        }
        var_23 += 0x1BC;
        var_21 += 0x1BC;
        var_22 += 0x1BC;
    } while (--sp0 >= 0);
    return var_19 < var_20;
}

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

s32 CheckSeimitsuCommand(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    s32 life[4];
    s32 attack[4];
    s32 i;
    s32 before;
    s32 best;
    s32 found;
    s32 cur;
    s32 v;
    s32 *lp;
    s32 t;
    f32 f;

    found = -1;
    ownSide = (side == 0) ? &work->enemy : &work->player;
    GetFriendAttack(side, work, life, attack);
    before = TodomeHyouka(side, work, 0, 0, life, attack);
    best = before;
    for (i = 0; i < 4; i++) {
        if (ownSide->battle[i].battle.layers[0].cardId < 0) {
            continue;
        }
        GetFriendAttack(side, work, life, attack);
        t = life[i] - 1;
        if (t > 0) {
            f = (f32)GetShoukin(&ownSide->battle[i].battle);
            life[i] -= 1;
            v = (s32)(f / 10.0f);
        } else {
            v = GetShoukin(&ownSide->battle[i].battle) - t;
            life[i] = 0;
        }
        v += TodomeHyouka(side, work, 0, 0, life, attack);
        if (best < v) {
            best = v;
            found = i | 0xF0000;
        }
    }
    i = 0;
    lp = life;
    for (; i < 4; i++, lp++) {
        if (ownSide->battle[i + 4].battle.layers[0].cardId < 0) {
            continue;
        }
        GetFriendAttack(side, work, life, attack);
        t = (*lp) - 1;
        if (t > 0) {
            f = (f32)GetPrice(&ownSide->battle[i + 4].battle);
            (*lp) -= 1;
            v = (s32)(f / 10.0f);
        } else {
            v = GetPrice(&ownSide->battle[i + 4].battle) - t;
            (*lp) = 0;
        }
        v += TodomeHyouka(side, work, 0, 0, life, attack);
        if (best < v) {
            best = v;
            found = i;
        }
    }
    if (best >= before + 0xF) {
        return found;
    }
    return -1;
}

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

s32 CheckBujinCommand(s32 side, CardGameWork *work)
{
  CardPlaySide *ownSide;
  s32 life[4];
  s32 attack[4];
  s32 *new_var2;
  s32 i;
  s32 before;
  s32 threshold;
  s32 best;
  s32 found;
  s32 cur;
  s32 new_var;
  found = -1;
  ownSide = gunoOwnSide(side, work);
  new_var = side;
  GetFriendAttack(new_var, work, life, attack);
  best = (before = TodomeHyouka(side, work, 0, 0, life, attack));
  threshold = before;
  for (i = 0; i < 4; i++)
  {
    if (ownSide->battle[i].battle.layers[0].cardId < 0)
    {
      if (work)
      {
      }
      if (!side)
      {
      }
      continue;
    }
    if (ownSide->battle[i].battle.flags & 2)
    {
      continue;
    }
    if (ownSide->battle[i].battle.layers[1].cardId != 0x35)
    {
      if (1)
      {
        continue;
      }
    }
    GetFriendAttack(side, work, life, attack);
    attack[i] = (attack[i] & 0xFFFF0000) + ((attack[i] & 0xFFFF) * 2);
    cur = TodomeHyouka(side, work, 0, 0, life, attack);
    if ((before + 0x1E) < cur)
    {
      return i | 0xF0000;
      ;
    }
  }

  for (i = 0; i < 4; i++)
  {
    s16 id = ownSide->battle[i].battle.layers[0].cardId;
    if ((id >= 0) && (!(ownSide->battle[i].battle.flags & 2)))
    {
      s32 type = CardChkAttackType(work->definitions, &ownSide->battle[i].battle);
      if ((((((u32) (id - 0x22)) >= 2U) && (id != 0x25)) && (id != 0x2B)) && (type != 4))
      {
        new_var2 = attack;
        GetFriendAttack(side, work, life, attack);
        if (work)
        {
        }
        attack[i] = (new_var2[i] & 0xFFFF0000) + ((attack[i] & 0xFFFF) * 2);
        cur = TodomeHyouka(side, work, 0, 0, life, attack);
        if (best < cur)
        {
          best = cur;
          found = i | 0xF0000;
        }
      }
    }
  }

  if (best >= (threshold + 0x1E))
  {
    return found;
  }
  return -1;
}

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

extern s32 CardPlaySuteCnt(CardPlaySide *side);

s32 CheckSensiCommand(s32 side, CardGameWork *work)
{
  CardPlaySide *sideData = &work->player;
  s32 i;
  s32 found;
  s32 best;
  s32 value;
  if (side)
  {
    sideData = &work->enemy;
  }
  if (side)
  {
    if (value)
    {
      i = 0;
      best = 80;
    }
    else
    {
      i = 0;
      best = 80;
    }
  }
  else
  {
    i = 0;
  }
  found = -1;
  best = 80;
  if (CardPlaySuteCnt(sideData) < 8)
  {
    return -1;
  }
  if (CardPlayYamaCnt(sideData) >= 11)
  {
    return -1;
  }
  switch (*((s16 *) (&work->command.phase)))
  {
    case 2:

    case 9:
      return -1;

  }

  goto test;
  loop:
  if (sideData->disposal[i].battle.layers[0].cardId >= 0)
  {
    value = GetPrice(&sideData->disposal[i].battle);
    if (value < best)
    {
      best = value;
      found = i;
    }
  }

  i++;
  test:
  if (i < 4)
  {
    goto loop;
  }

  for (i = 0; i < 4; i++)
  {
    if (sideData->battle[i].battle.layers[0].cardId >= 0)
    {
      value = GetShoukin(&sideData->battle[i].battle);
      if (value < best)
      {
        best = value;
        found = i;
        found = found | 0xF0000;
      }
    }
  }

  return found;
}

extern void CGPEnemyExecCommand(s32 side, CardGameWork *work, s16 *command, s32 flag);
typedef struct SensiCmd {
    s16 kind;
    s16 slot;
    s16 pad[2];
} SensiCmd;

void SensiEvent(s32 side, CardGameWork *work)
{
    s32 slotA[4];
    s32 slotB[4];
    SensiCmd cmds[8];
    s32 rounds;
    CardPlaySide *sideData;
    s32 yama;
    s32 kindA;
    s32 kindB;
    s32 cnt;
    s32 sute;
    s32 bestA;
    s32 bestB;
    s32 i;

    if (side == 0) {
        sideData = &work->player;
        kindA = 5;
        kindB = 6;
    } else {
        sideData = &work->enemy;
        kindA = 4;
        kindB = 2;
    }
    for (i = 0; i < 4; i++) {
        slotA[i] = -1;
        slotB[i] = -1;
    }
    i = 0;
    yama = CardPlayYamaCnt(sideData);
    sute = CardPlaySuteCnt(sideData);
    do {
        CardBoardSlot *cur = &sideData->battle[i];
        if (cur->battle.layers[0].cardId >= 0) {
            if (cur->battle.layers[0].life < 5) {
                slotA[i] = 1;
            } else {
                slotB[i] = -1;
            }
        } else {
            slotA[i] = -1;
        }
        if (*(s16 *)((char *)cur + 0x70A) >= 0) {
            if (cur->battle.layers[0].life < 5) {
                slotB[i] = 1;
            } else {
                bestB = i;
                slotB[i] = -1;
            }
        } else {
            slotB[i] = -1;
        }
        i++;
    } while (i < 4);
    cnt = 0;
    {
        s32 *cntp = &cnt;
        *cntp = 0;
    }
    rounds = 0;
    do {
        bestA = -1;
        bestB = -1;
        for (i = 0; i < 4; i++) {
            if (slotA[i] != -1 && (bestA == -1 || GetShoukin(&sideData->battle[i].battle) < GetShoukin(&sideData->battle[bestA].battle))) {
                bestA = i;
            }
            if (slotB[i] != -1 && (bestB == -1 || GetPrice(&sideData->disposal[i].battle) < GetPrice(&sideData->disposal[bestB].battle))) {
                bestB = i;
            }
        }
        if (yama >= 20 || sute < 4) {
            break;
        }
        if (bestB != -1) {
            cmds[cnt].kind = kindB;
            cmds[cnt].slot = bestB;
            sute -= 4;
            slotB[bestB] = -1;
            yama += 4;
            cnt++;
        }
        if (yama >= 20 || sute < 4) {
            break;
        }
        if (bestA != -1) {
            cmds[cnt].kind = kindA;
            cmds[cnt].slot = bestA;
            sute -= 4;
            slotA[bestA] = -1;
            yama += 4;
            cnt++;
        }
        if (yama >= 20 || sute < 4) {
            break;
        }
        rounds++;
    } while (rounds < 4);
    if (cnt > 0) {
        i = cnt;
        do {
            i--;
        } while (i != 0);
    }
    CGPEnemyExecCommand(side, work, &cmds[0].kind, cnt);
}

void CCU063ExecSub(CardGameWork *work, s32 context);
void CCU064ExecSub(CardGameWork *work, s32 context);
void CCU065ExecSub(CardGameWork *work, s32 context);

s32 CC_Kara_DisposeEvent(s32 side, CardGameWork *work)
{
    CardPlaySide *sideData = &work->player;
    s32 life[4];
    s32 attack[4];
    s32 slot3F = -1;
    s32 slot40 = -1;
    s32 slot41 = -1;
    s32 before;
    s32 after;
    s32 i;
    s32 id;
    s16 *cbase;
    s32 cnt;
    s32 *ap;

    if (side) {
        sideData = &work->enemy;
    }
    cbase = &sideData->battle[0].battle.layers[0].cardId;
    for (i = 0; i < 4; i++) {
        id = *(s16 *)((char *)cbase + i * 0x1BC);
        if (id == 0x3F) {
            slot3F = i;
        } else if (id == 0x40) {
            slot40 = i;
        } else if (id == 0x41) {
            slot41 = i;
        }
    }
    GetFriendAttack(side, work, life, attack);
    before = TodomeHyouka(side, work, 0, 0, life, attack);
    if (slot3F != -1) {
        for (i = 0; i < 4; i++) {
            id = *(s16 *)((char *)sideData + 0x1AA + i * 0x1BC);
            if (id >= 0 && (work->definitions[id].flags & 1)) {
                attack[i] += 1;
            }
            attack[slot3F] = 0;
        }
        after = TodomeHyouka(side, work, 0, 0, life, attack);
        if (before + 0x78 < after) {
            *((unsigned char *)sideData + slot3F * 0x1BC + 0x1A7) = 0;
            CCU063ExecSub(work, (s32)sideData);
        }
    }
    if (slot40 != -1) {
        for (i = 0; i < 4; i++) {
            id = *(s16 *)((char *)sideData + 0x1AA + i * 0x1BC);
            if (id >= 0 && (work->definitions[id].flags & 8)) {
                attack[i] += 1;
            }
            attack[slot40] = 0;
        }
        after = TodomeHyouka(side, work, 0, 0, life, attack);
        if (before + 0x78 < after) {
            *((unsigned char *)sideData + slot40 * 0x1BC + 0x1A7) = 0;
            CCU064ExecSub(work, (s32)sideData);
        }
    }
    if (slot41 != -1) {
        for (i = 0; i < 4; i++) {
            id = *(s16 *)((char *)sideData + 0x1AA + i * 0x1BC);
            if (id >= 0 && (work->definitions[id].flags & 4)) {
                attack[i] += 2;
            }
            attack[slot41] = 0;
        }
        after = TodomeHyouka(side, work, 0, 0, life, attack);
        if (before + 0x78 < after) {
            *((unsigned char *)sideData + slot41 * 0x1BC + 0x1A7) = 0;
            CCU065ExecSub(work, (s32)sideData);
        }
    }
    return 0;
}

extern s32 commplay;
s32 CheckExtCommand(s32 side, CardGameWork *work);
s32 CheckCurryCommand(s32 side, CardGameWork *work);
s32 CheckTentCommand(s32 side, CardGameWork *work);
s32 CheckNazoCommand(s32 side, CardGameWork *work);
s32 CheckTeppekiCommand(s32 side, CardGameWork *work);
s32 Check413Command(s32 side, CardGameWork *work);
s32 CheckSouteniHouCommand(s32 side, CardGameWork *work);
s32 CheckHatudouCommand(s32 side, CardGameWork *work);
s32 CheckMilCommand(s32 side, CardGameWork *work);
s32 CheckXDamageCommand(s32 side, CardGameWork *work, s32 threshold);
s32 CheckKonranCommand(s32 side, CardGameWork *work);
s32 CheckSensiCommand(s32 side, CardGameWork *work);
s32 CheckShukumeiCommand(s32 side, CardGameWork *work);
s32 CheckDamageCommand(s32 side, CardGameWork *work, s32 threshold);
s32 CheckTaikyakuCommand(s32 side, CardGameWork *work);
s32 CheckGunoYokanCommand(s32 side, CardGameWork *work);
s32 CheckDownGunoCommand(s32 side, CardGameWork *work);
s32 CheckChudanCommand(s32 side, CardGameWork *work);
s32 CheckTuukokuCommand(s32 side, CardGameWork *work);
s32 CheckSeimitsuCommand(s32 side, CardGameWork *work);
s32 CheckKyoukouCommand(s32 side, CardGameWork *work);
s32 CheckBujinCommand(s32 side, CardGameWork *work);
s32 CheckTotugekiCommand(s32 side, CardGameWork *work);
s32 CheckKetugiCommand(s32 side, CardGameWork *work);
s32 DisposeAreaCheckFriend(s32 side, CardGameWork *work);
s32 BattleAreaCheckFriend(s32 side, CardGameWork *work);
s32 CC_Kara_DisposeEvent(s32 side, CardGameWork *work);
static inline CardPlaySide *CommReqSide(CardGameWork *work, s32 side)
{
    if (side == 0) {
        return &work->player;
    }
    return &work->enemy;
}

s32 CC_Kara_CommRequest(s32 arg0, CardGameWork *arg1)
{
    CardPlaySide *own;
    s32 i;
    s32 count;
    s32 r;
    s32 t;
    s32 dflag;
    s16 phase;
    s16 *tp;
    s16 *cards;

    own = CommReqSide(arg1, arg0);
    if ((s16)arg1->command.phase == 2 || (s16)arg1->command.phase == 9) {
        CC_Kara_DisposeEvent(arg0, arg1);
    }
    count = CardPlayHandCnt(&own->hand);
    i = 0;
    if (count > 0) {
    cards = own->hand.cards;
    for (; i < count; i++) {
        tp = &cards[i];
        if (arg1->definitions[*tp].kind != 2) {
            continue;
        }
        if (CheckCommandPlay(arg0, arg1, *tp) != 1) {
            continue;
        }
        if (arg1->definitions[*tp].reserveCost != 0 &&
            CardPlayYamaCnt(own) - arg1->definitions[*tp].reserveCost < 3) {
            continue;
        }
        switch ((s16)(u16)*tp) {
        case 0x53:
            r = CheckExtCommand(arg0, arg1);
            if (r != -1) {
                commplay = r;
                return i;
            }
            break;
        case 0x54:
            r = CheckCurryCommand(arg0, arg1);
            if (r != -1) {
                commplay = r;
                return i;
            }
            break;
        case 0x55:
            r = CheckTentCommand(arg0, arg1);
            if (r == 1) {
                commplay = 0;
                return i;
            }
            break;
        case 0x56:
            phase = arg1->command.phase;
            if (phase != 2 && phase != 9 && CardPlaySuteCnt(own) >= 5) {
                commplay = 0;
                return i;
            }
            break;
        case 0x57:
            phase = arg1->command.phase;
            if (phase != 2 && phase != 9) {
                t = DisposeAreaCheckFriend(arg0, arg1);
                t = t + BattleAreaCheckFriend(arg0, arg1);
                if (t < 4) {
                    if (CardPlayYamaCnt(own) < 10) {
                        return i;
                    }
                    break;
                }
                if (CardPlaySuteCnt(own) >= t) {
                    return i;
                }
            }
            break;
        case 0x58:
        case 0x59:
            r = CheckTaikyakuCommand(arg0, arg1);
            if (r != -1) {
                commplay = r | 0xFF0000;
                return i;
            }
            break;
        case 0x5A:
        case 0x5B:
            r = CheckDamageCommand(arg0, arg1, 2);
            if (r != -1) {
                commplay = r | 0xFF0000;
                return i;
            }
            break;
        case 0x5C:
            r = CheckNazoCommand(arg0, arg1);
            if (r == 1) {
                commplay = 0;
                return i;
            }
            break;
        case 0x5D:
            phase = arg1->command.phase;
            if (phase == 2 || phase == 9) {
                if (CardPlayYamaCnt(own) >= 0xB && 7 > count) {
                    commplay = 0;
                    return i;
                }
            } else if (CardPlayYamaCnt(own) >= 0xB) {
                commplay = 0;
                return i;
            }
            break;
        case 0x5E:
            phase = arg1->command.phase;
            if (phase == 2 || phase == 9) {
                if (CardPlaySuteCnt(own) >= 2 && count < 7) {
                    commplay = 0;
                    return i;
                }
            } else if (CardPlaySuteCnt(own) >= 2) {
                commplay = 0;
                return i;
            }
            break;
        case 0x5F:
        case 0x60:
        case 0x69:
        case 0x6C:
            return i;
        case 0x61:
            r = CheckGunoYokanCommand(arg0, arg1);
            if (r != -1) {
                commplay = r | 0xF00000;
                return i;
            }
            break;
        case 0x62:
            r = CheckDownGunoCommand(arg0, arg1);
            if (r != -1) {
                commplay = r | 0xF00000;
                return i;
            }
            break;
        case 0x63:
            r = CheckChudanCommand(arg0, arg1);
            if (r != -1) {
                commplay = r | 0xF00000;
                return i;
            }
            break;
        case 0x64:
            r = CheckTeppekiCommand(arg0, arg1);
            if (r == 1) {
                commplay = 0;
                return i;
            }
            break;
        case 0x65:
            r = Check413Command(arg0, arg1);
            if (r == 1) {
                commplay = 0;
                return i;
            }
            break;
        case 0x66:
            r = CheckSouteniHouCommand(arg0, arg1);
            if (r == 1) {
                commplay = 0;
                return i;
            }
            break;
        case 0x67:
            r = CheckTuukokuCommand(arg0, arg1);
            if (r != -1) {
                commplay = r | 0xF00000;
                return i;
            }
            break;
        case 0x68:
            if (CheckKetugiCommand(arg0, arg1) != 0) {
                commplay = 0;
                return i;
            }
            break;
        case 0x6A:
            if (count < 7) {
                return i;
            }
            break;
        case 0x6B:
            r = CheckBujinCommand(arg0, arg1);
            if (r != -1) {
                commplay = r | 0xF0000;
                return i;
            }
            break;
        case 0x6D:
            r = CheckSensiCommand(arg0, arg1);
            if (r != -1) {
                commplay = r;
                return i;
            }
            break;
        case 0x6E:
            r = CheckXDamageCommand(arg0, arg1, 1);
            if (r == 1) {
                commplay = 0;
                return i;
            }
            break;
        case 0x6F:
            r = CheckTotugekiCommand(arg0, arg1);
            if (r != -1) {
                commplay = r | 0xFF0000;
                return i;
            }
            break;
        case 0x70:
            r = CheckSeimitsuCommand(arg0, arg1);
            if (r != -1) {
                commplay = r | 0xF00000;
                return i;
            }
            break;
        case 0x71:
            r = CheckHatudouCommand(arg0, arg1);
            if (r == 1) {
                commplay = 0;
                return i;
            }
            break;
        case 0x72:
            r = CheckKonranCommand(arg0, arg1);
            if (r == 1) {
                commplay = 0;
                return i;
            }
            break;
        case 0x73:
            r = CheckMilCommand(arg0, arg1);
            if (r == 1) {
                commplay = 0;
                return i;
            }
            break;
        case 0x75:
            r = CheckExtCommand(arg0, arg1);
            if (r != -1) {
                commplay = r;
                return i;
            }
            break;
        case 0x76:
            r = CheckShukumeiCommand(arg0, arg1);
            if (r != -1) {
                commplay = r;
                return i;
            }
            break;
        case 0x77:
            r = CheckKyoukouCommand(arg0, arg1);
            if (r != -1) {
                commplay = r | 0xF00000;
                return i;
            }
            break;
        }
    }
    }
    return -1;
}

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

s32 YoakimuMoMoCheck(s32 side, CardGameWork *work);
s32 MoMoCheck(s32 side, CardGameWork *work);

s32 TodomeHyouka(s32 side, CardGameWork *work, s32 a2, s32 a3, s32 *life, s32 *attack)
{
  s32 lp[4];
  s32 ap[4];
  s32 fl[4];
  s32 bonus[4];
  CardPlaySide *me;
  CardPlaySide *opp;
  s32 sc;
  s32 f;
  s32 i;
  s32 r;
  s32 g;
  s32 cnt7a;
  s32 hi0;
  s32 hi1;
  s32 total;
  s32 momo;
  s32 cnt;
  s32 sum4;
  s32 sum8;
  s32 *p;
  sc = 0;
  hi0 = 0;
  hi1 = 0;
  total = 0;
  momo = -1;
  cnt = 0;
  sum4 = 0;
  sum8 = 0;
  if (side == 0)
  {
    me = &work->player;
    opp = &work->enemy;
  }
  else
  {
    me = &work->enemy;
    opp = &work->player;
  }
  if (YoakimuMoMoCheck(side == 0, work) == 1)
  {
    momo = (u16) MoMoCheck(side == 0, work);
  }
  for (i = 0; i < 4; i++)
  {
    lp[i] = life[i];
    ap[i] = attack[i];
  }

  cnt7a = 0;
  for (i = 0; i < 4; i++)
  {
    if (opp->operations[i] == 0x7A)
    {
      cnt7a++;
    }
  }

  for (i = 0; i < 4; i++)
  {
    s16 id = opp->battle[i].battle.layers[0].cardId;
    bonus[i] = 0;
    if (id >= 0)
    {
      if ((opp->battle[i].battle.layers[1].cardId == 0x2C) || (opp->battle[i].battle.layers[1].cardId == 0x2F))
      {
        bonus[i] = 1;
      }
      if (work->definitions[id].flags == 8)
      {
        bonus[i] += cnt7a;
      }
    }
  }

  if (a2 != a3)
  {
    r = ap[a2];
    ap[a2] = ap[a3];
    ap[a3] = r;
    if (((me->battle[a3].battle.layers[0].cardId < 0) || (me->battle[a2].battle.layers[0].cardId < 0)) && (!(attack[a3] & 0x20000)))
    {
      f = CheckEnemyFight(side, work);
      if (f != 0)
      {
        if ((((u32) a3) < 2) && (((u32) (a2 - 2)) < 2))
        {
          sc = f * 10;
        }
        if ((((u32) (a3 - 2)) < 2) && (((u32) a2) < 2))
        {
          sc = -(f * 10);
        }
      }
      else
      {
        if ((((u32) a3) < 2) && (((u32) (a2 - 2)) < 2))
        {
          sc = 8;
        }
        if ((((u32) (a3 - 2)) < 2) && (((u32) a2) < 2))
        {
          sc = -8;
        }
      }
    }
  }
  if (!(ap[0] & 0x20000) && !(ap[2] & 0x20000) && ap[0] != 0 && ap[2] != 0)
  {
    if ((ap[0] & 0xFFFF) < (ap[2] & 0xFFFF))
    {
      sc++;
    }
    if ((ap[2] & 0xFFFF) < (ap[0] & 0xFFFF))
    {
      sc--;
    }
  }
  if (!(ap[1] & 0x20000) && !(ap[3] & 0x20000) && ap[1] != 0 && ap[3] != 0)
  {
    if ((ap[1] & 0xFFFF) < (ap[3] & 0xFFFF))
    {
      sc++;
    }
    if ((ap[3] & 0xFFFF) < (ap[1] & 0xFFFF))
    {
      sc--;
    }
  }
  if (ap[0] & 0x20000)
  {
    if ((!(ap[2] & 0x20000)) && (ap[2] & 0xFFFF))
    {
      sc += 10;
    }
  }
  if (ap[1] & 0x20000)
  {
    if ((!(ap[3] & 0x20000)) && (ap[3] & 0xFFFF))
    {
      sc += 10;
    }
  }
  if (((ap[0] != 0) && (ap[2] & 0x20000)) && (ap[2] & 0xFFFF))
  {
    sc -= 10;
  }
  if (((ap[1] != 0) && (ap[3] & 0x20000)) && (ap[3] & 0xFFFF))
  {
    sc -= 10;
  }
  p = &ap[2];
  if (*p & 0xC0000)
  {
    sc += 2;
    if ((a2 != 0) && (a3 != 0))
    {
      if (me->battle[0].battle.layers[0].cardId >= 0)
      {
        sc += me->battle[0].battle.layers[0].life;
      }
    }
    else
      if (a2 == 0)
    {
      if (me->battle[a3].battle.layers[0].cardId >= 0)
      {
        sc += me->battle[a3].battle.layers[0].life;
      }
    }
    else
      if (me->battle[a2].battle.layers[0].cardId >= 0)
    {
      sc += me->battle[a2].battle.layers[0].life;
    }
  }
  p = &ap[3];
  if (*p & 0xC0000)
  {
    sc += 2;
    if ((a2 != 1) && (a3 != 1))
    {
      if (me->battle[1].battle.layers[0].cardId >= 0)
      {
        sc += me->battle[1].battle.layers[0].life;
      }
    }
    else
      if (a2 == 1)
    {
      if (me->battle[a3].battle.layers[0].cardId >= 0)
      {
        sc += me->battle[a3].battle.layers[0].life;
      }
    }
    else
      if (me->battle[a2].battle.layers[0].cardId >= 0)
    {
      sc += me->battle[a2].battle.layers[0].life;
    }
  }
  for (i = 0; i < 4; i++)
  {
    if (ap[i] & 0x100000)
    {
      cnt++;
    }
    if (ap[i] & 0x40000)
    {
      sum4 += ap[i] & 0xFFFF;
      ap[i] = 0;
    }
    if (ap[i] & 0x80000)
    {
      sum8 += ap[i] & 0xFFFF;
      ap[i] = 0;
    }
  }

  if (ap[0] & 0x20000)
  {
    hi0 = ap[0] & 0xFFFF;
    ap[0] &= 0xFFFF0000;
  }
  if (ap[1] & 0x20000)
  {
    hi1 = ap[1] & 0xFFFF;
    ap[1] &= 0xFFFF0000;
  }
  if (ap[2] & 0x20000)
  {
    ap[2] = 0;
  }
  if (ap[3] & 0x20000)
  {
    ap[3] = 0;
  }
  for (i = 0; i < 4; i++)
  {
    if (ap[i] & 0x10000)
    {
      fl[i] = 1;
    }
    else
    {
      fl[i] = 0;
    }
    ap[i] = (u16) ap[i];
  }

  if (lp[0] != 0)
  {
    r = lp[0] - ap[1];
    if (hi1 != 0)
    {
      r = lp[0] - hi1;
    }
    if ((bonus[0] != 0) && ((ap[1] != 0) || (hi1 != 0)))
    {
      r += bonus[0];
    }
    if (r > 0)
    {
      r -= ap[3];
      if ((bonus[0] != 0) && (ap[3] != 0))
      {
        r += bonus[0];
      }
      if (r > 0)
      {
        if ((fl[1] == 1) || (fl[3] == 1))
        {
          g = GetShoukin(&opp->battle[0].battle);
          sc = ((float) sc) + (((float) g) * 0.5f);
          g = GetShoukin(&opp->battle[0].battle);
          sc = ((float) sc) + ((((float) g) / 20.0f) * ((float) (lp[0] - r)));
        }
        else
        {
          g = GetShoukin(&opp->battle[0].battle);
          sc = ((float) sc) + ((((float) g) / 10.0f) * ((float) (lp[0] - r)));
        }
        lp[0] = r;
      }
      else
        if (momo != 0)
      {
        sc += GetShoukin(&opp->battle[0].battle) + r;
        lp[0] = 0;
      }
    }
    else
    {
      lp[0] = 0;
      if (momo != 0)
      {
        sc += GetShoukin(&opp->battle[0].battle) + r;
        if (lp[2] != 0)
        {
          r = lp[2] - ap[3];
          if ((bonus[2] != 0) && (ap[3] != 0))
          {
            r += bonus[2];
          }
          if (r > 0)
          {
            if (fl[3] == 1)
            {
              g = GetShoukin(&opp->battle[2].battle);
              sc = ((float) sc) + (((float) g) * 0.5f);
              g = GetShoukin(&opp->battle[2].battle);
              sc = ((float) sc) + ((((float) g) / 20.0f) * ((float) (lp[2] - r)));
            }
            else
            {
              g = GetShoukin(&opp->battle[2].battle);
              sc = ((float) sc) + ((((float) g) / 10.0f) * ((float) (lp[2] - r)));
            }
            lp[2] = r;
          }
          else
            if (momo != 2)
          {
            sc += GetShoukin(&opp->battle[2].battle) + r;
            lp[2] = 0;
          }
        }
        else
        {
          sc += ap[3];
          total += ap[3];
        }
      }
    }
  }
  else
    if (lp[2] != 0)
  {
    r = lp[2] - ap[1];
    if ((bonus[2] != 0) && (ap[1] != 0))
    {
      r += bonus[2];
    }
    if (r > 0)
    {
      r -= ap[3];
      if ((bonus[2] != 0) && (ap[3] != 0))
      {
        r += bonus[2];
      }
      if (r > 0)
      {
        if ((hi1 == 0) && ((fl[1] == 1) || (fl[3] == 1)))
        {
          g = GetShoukin(&opp->battle[2].battle);
          sc = ((float) sc) + (((float) g) * 0.5f);
          g = GetShoukin(&opp->battle[2].battle);
          sc = ((float) sc) + ((((float) g) / 20.0f) * ((float) (lp[2] - r)));
        }
        else
        {
          g = GetShoukin(&opp->battle[2].battle);
          sc = ((float) sc) + ((((float) g) / 10.0f) * ((float) (lp[2] - r)));
        }
        lp[2] = r;
      }
      else
        if (momo != 2)
      {
        sc += GetShoukin(&opp->battle[2].battle) + r;
        lp[2] = 0;
      }
    }
    else
      if (momo != 2)
    {
      lp[2] = 0;
      sc += GetShoukin(&opp->battle[2].battle) + r;
      sc += ap[3];
      total += ap[3];
    }
  }
  else
  {
    sc += ap[1] + ap[3];
    total += ap[1] + ap[3];
  }
  if (lp[1] != 0)
  {
    r = lp[1] - ap[0];
    if (hi0 != 0)
    {
      r = lp[1] - hi0;
    }
    if ((bonus[1] != 0) && ((ap[0] != 0) || (hi0 != 0)))
    {
      r += bonus[1];
    }
    if (r > 0)
    {
      r -= ap[2];
      if ((bonus[1] != 0) && (ap[2] != 0))
      {
        r += bonus[1];
      }
      if (r > 0)
      {
        if ((fl[0] == 1) || (fl[2] == 1))
        {
          g = GetShoukin(&opp->battle[1].battle);
          sc = ((float) sc) + (((float) g) * 0.5f);
          g = GetShoukin(&opp->battle[1].battle);
          sc = ((float) sc) + ((((float) g) / 20.0f) * ((float) (lp[1] - r)));
        }
        else
        {
          g = GetShoukin(&opp->battle[1].battle);
          sc = ((float) sc) + ((((float) g) / 10.0f) * ((float) (lp[1] - r)));
        }
        lp[1] = r;
      }
      else
        if (momo != 1)
      {
        sc += GetShoukin(&opp->battle[1].battle) + r;
        lp[1] = 0;
      }
    }
    else
      if (momo != 1)
    {
      sc += GetShoukin(&opp->battle[1].battle) + r;
      if (lp[3] != 0)
      {
        r = lp[3] - ap[2];
        if ((bonus[3] != 0) && (ap[2] != 0))
        {
          r += bonus[3];
        }
        if (r > 0)
        {
          if (fl[2] == 1)
          {
            g = GetShoukin(&opp->battle[3].battle);
            sc = ((float) sc) + (((float) g) * 0.5f);
            g = GetShoukin(&opp->battle[3].battle);
            sc = ((float) sc) + ((((float) g) / 20.0f) * ((float) (lp[3] - r)));
          }
          else
          {
            g = GetShoukin(&opp->battle[3].battle);
            sc = ((float) sc) + ((((float) g) / 10.0f) * ((float) (lp[3] - r)));
          }
          lp[3] = r;
        }
        else
          if (momo != 3)
        {
          sc += GetShoukin(&opp->battle[3].battle) + r;
          lp[3] = 0;
        }
      }
      else
      {
        sc += ap[2];
        total += ap[2];
      }
    }
  }
  else
    if (lp[3] != 0)
  {
    r = lp[3] - ap[0];
    if ((bonus[3] != 0) && (ap[0] != 0))
    {
      r += bonus[3];
    }
    if (r > 0)
    {
      r -= ap[2];
      if ((bonus[3] != 0) && (ap[2] != 0))
      {
        r += bonus[3];
      }
      if (r > 0)
      {
        if ((hi0 == 0) && ((fl[0] == 1) || (fl[2] == 1)))
        {
          g = GetShoukin(&opp->battle[3].battle);
          sc = ((float) sc) + (((float) g) * 0.5f);
          g = GetShoukin(&opp->battle[3].battle);
          sc = ((float) sc) + ((((float) g) / 20.0f) * ((float) (lp[3] - r)));
        }
        else
        {
          g = GetShoukin(&opp->battle[3].battle);
          sc = ((float) sc) + ((((float) g) / 10.0f) * ((float) (lp[3] - r)));
        }
        lp[3] = r;
      }
      else
        if (momo != 3)
      {
        sc += GetShoukin(&opp->battle[3].battle) + r;
        lp[3] = 0;
      }
    }
    else
      if (momo != 3)
    {
      lp[3] = 0;
      sc += GetShoukin(&opp->battle[3].battle) + r;
      sc += ap[2];
      total += ap[2];
    }
  }
  else
  {
    total += ap[0] + ap[2];
    sc += ap[0] + ap[2];
  }
  if (cnt != 0)
  {
    for (i = 0; i < 4; i++)
    {
      if (lp[i] > 0)
      {
        lp[i] -= cnt;
      }
    }

  }
  if (sum4 != 0)
  {
    for (i = 0; i < 4; i++)
    {
      if (lp[i] > 0)
      {
        lp[i] -= sum4;
        sc = ((float) sc) + ((((float) GetShoukin(&opp->battle[i].battle)) / 10.0f) * ((float) sum4));
      }
    }

  }
  if (sum8 != 0)
  {
    sc += sum8;
    total += sum8;
  }
  if ((CardPlayYamaCnt(opp) - total) < 2)
  {
    sc += 1000;
  }
  if (AlbedoCheck(side, work) != (-1))
  {
    sc += total * 10;
  }
  return sc;
}

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

static inline s32 TodomeSearch(s32 side, CardGameWork *work, CardPlaySide *own, s32 slot, s32 best, s32 *results)
{
    s16 *cardId;
    s32 *res;
    s32 i;
    s32 bestIdx;

    bestIdx = -1;
    for (i = 0, cardId = &own->battle[0].battle.layers[0].cardId, res = results; i < 4; i++, cardId += 0xDE, res++) {
        if (i != slot) {
            if (best < *res) {
                if (*res > 1000) {
                    best = *res;
                    bestIdx = i;
                } else if (*cardId < 0 || GetDeckDataAdjust(side, work, *cardId) < 4) {
                    best = *res;
                    bestIdx = i;
                }
            }
        }
    }
    return bestIdx;
}

s32 CC_Kara_TodomeMove(s32 side, CardGameWork *work, s32 slot)
{
    s32 life[4];
    s32 attack[4];
    s32 results[4];
    CardPlaySide *own;
    s32 best;
    s32 bestIdx;

    own = &work->player;
    if (side != 0) {
        own = &work->enemy;
    }
    GetFriendAttack(side, work, life, attack);
    slot &= 0xFFFF;
    best = TodomeHyouka(side, work, slot, slot, life, attack);
    results[0] = TodomeHyouka(side, work, 0, slot, life, attack);
    results[1] = TodomeHyouka(side, work, 1, slot, life, attack);
    results[2] = TodomeHyouka(side, work, 2, slot, life, attack);
    results[3] = TodomeHyouka(side, work, 3, slot, life, attack);
    bestIdx = TodomeSearch(side, work, own, slot, best, results);
    if (bestIdx == -1) {
        return 1;
    }
    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[bestIdx].battle, (CardMoveWork *)&own->battle[slot].battle);
    return 1;
}

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

s32 CC_Kara_BackTodomeMove(s32 side, CardGameWork *work, s32 slot)
{
    s32 life[4];
    s32 attack[4];
    CardPlaySide *own;
    s32 t;

    own = &work->player;
    if (side != 0) {
        own = &work->enemy;
    }
    GetFriendAttack(side, work, life, attack);
    switch (slot) {
    case 0:
        t = TodomeHyouka(side, work, 0, 0, life, attack);
        if (t < TodomeHyouka(side, work, 2, 0, life, attack)) {
            if (own->battle[2].battle.layers[0].cardId < 0
                || GetDeckDataAdjust(side, work, own->battle[2].battle.layers[0].cardId) < 4U) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[2].battle, (CardMoveWork *)&own->battle[0].battle);
                return 1;
            }
            if (TodomeHyouka(side, work, 2, 0, life, attack) > 1000) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[2].battle, (CardMoveWork *)&own->battle[0].battle);
                return 1;
            }
        }
        if (t < TodomeHyouka(side, work, 3, 0, life, attack)) {
            if (own->battle[3].battle.layers[0].cardId < 0
                || GetDeckDataAdjust(side, work, own->battle[3].battle.layers[0].cardId) < 4U) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[3].battle, (CardMoveWork *)&own->battle[slot].battle);
                return 1;
            }
            if (TodomeHyouka(side, work, 3, 0, life, attack) > 1000) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[3].battle, (CardMoveWork *)&own->battle[slot].battle);
                return 1;
            }
        }
        return 1;
    case 1:
        t = TodomeHyouka(side, work, 1, 1, life, attack);
        if (t < TodomeHyouka(side, work, 2, 1, life, attack)) {
            if (own->battle[2].battle.layers[0].cardId < 0
                || GetDeckDataAdjust(side, work, own->battle[2].battle.layers[0].cardId) < 4U) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[2].battle, (CardMoveWork *)&own->battle[1].battle);
                return 1;
            }
            if (TodomeHyouka(side, work, 2, 1, life, attack) > 1000) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[2].battle, (CardMoveWork *)&own->battle[1].battle);
                return 1;
            }
        }
        if (t < TodomeHyouka(side, work, 3, 1, life, attack)) {
            if (own->battle[3].battle.layers[0].cardId < 0
                || GetDeckDataAdjust(side, work, own->battle[3].battle.layers[0].cardId) < 4U) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[3].battle, (CardMoveWork *)&own->battle[slot].battle);
                return 1;
            }
            if (TodomeHyouka(side, work, 3, 1, life, attack) > 1000) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[3].battle, (CardMoveWork *)&own->battle[slot].battle);
                return 1;
            }
        }
        return 1;
    case 2:
        t = TodomeHyouka(side, work, 2, 2, life, attack);
        if (t < TodomeHyouka(side, work, 3, 2, life, attack)) {
            if (own->battle[3].battle.layers[0].cardId < 0
                || GetDeckDataAdjust(side, work, own->battle[3].battle.layers[0].cardId) < 4U) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[3].battle, (CardMoveWork *)&own->battle[2].battle);
                return 1;
            }
            if (TodomeHyouka(side, work, 3, 2, life, attack) > 1000) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[3].battle, (CardMoveWork *)&own->battle[2].battle);
                return 1;
            }
        }
        return 1;
    case 3:
        t = TodomeHyouka(side, work, 3, 3, life, attack);
        if (t < TodomeHyouka(side, work, 2, 3, life, attack)) {
            if (own->battle[2].battle.layers[0].cardId < 0
                || GetDeckDataAdjust(side, work, own->battle[2].battle.layers[0].cardId) < 4U) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[2].battle, (CardMoveWork *)&own->battle[3].battle);
                return 1;
            }
            if (TodomeHyouka(side, work, 2, 3, life, attack) > 1000) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[2].battle, (CardMoveWork *)&own->battle[3].battle);
                return 1;
            }
        }
        break;
    }
    return 1;
}

s32 CC_Kara_KabeMove(s32 side, CardGameWork *work, s32 slot);

s32 CC_Kara_FrontTodomeMove(s32 side, CardGameWork *work, s32 slot)
{
    CardPlaySide *own;
    CardPlaySide *other;
    s32 life[4];
    s32 attack[4];
    s32 t2;
    s32 t18;

    if (side == 0) {
        own = &work->player;
        other = &work->enemy;
    } else {
        own = &work->enemy;
        other = &work->player;
    }
    slot = (u16)slot;
    GetFriendAttack(side, work, life, attack);
    switch (slot) {
    case 0:
        if (attack[0] != 0) {
            t18 = TodomeHyouka(side, work, 0, 0, life, attack);
            t2 = TodomeHyouka(side, work, 1, 0, life, attack);
            if (t18 + 10 < t2) {
                if (own->battle[1].battle.layers[0].cardId < 0
                    || GetDeckDataAdjust(side, work, own->battle[1].battle.layers[0].cardId) < 3U) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[1].battle, (CardMoveWork *)&own->battle[0].battle);
                    return 1;
                }
                if (t2 > 1000) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[1].battle, (CardMoveWork *)&own->battle[0].battle);
                    return 1;
                }
            } else if (t2 < t18 - 10) {
                if (CardChkAttackType(work->definitions, &own->battle[0].battle) != 1
                    || other->battle[1].battle.layers[0].cardId >= 0) {
                    return 1;
                }
                CC_Kara_KabeMove(side, work, 0);
                return 1;
            } else if (t18 < t2) {
                if (own->battle[1].battle.layers[0].cardId < 0
                    && own->battle[3].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[1].battle, (CardMoveWork *)&own->battle[0].battle);
                    return 1;
                }
            }
        }
    case 2:
    case 3:
        ((void (*)(s32, CardGameWork *, s32))CC_Kara_KabeMove)(side, work, slot);
        return 1;
    case 1:
        if (attack[1] != 0) {
            t18 = TodomeHyouka(side, work, 1, 1, life, attack);
            t2 = TodomeHyouka(side, work, 0, 1, life, attack);
            if (t18 + 10 < t2) {
                if (own->battle[0].battle.layers[0].cardId < 0
                    || GetDeckDataAdjust(side, work, own->battle[0].battle.layers[0].cardId) < 3U) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[0].battle, (CardMoveWork *)&own->battle[1].battle);
                    return 1;
                }
                if (t2 > 1000) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[0].battle, (CardMoveWork *)&own->battle[1].battle);
                    return 1;
                }
            } else if (t2 < t18 - 10) {
                if (CardChkAttackType(work->definitions, &own->battle[1].battle) != 1
                    || other->battle[0].battle.layers[0].cardId >= 0) {
                    return 1;
                }
                CC_Kara_KabeMove(side, work, 1);
                return 1;
            } else if (t18 < t2) {
                if (own->battle[0].battle.layers[0].cardId < 0
                    && own->battle[2].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[0].battle, (CardMoveWork *)&own->battle[1].battle);
                    return 1;
                }
            }
        }
        CC_Kara_KabeMove(side, work, slot);
        return 1;
    }
    return 1;
}

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

static inline void MoMoMMMove(CardLayerStack *dstp, CardLayerStack *srcp)
{
    CGPMoveDSBATTLEwork((CardMoveWork *)dstp, (CardMoveWork *)srcp);
}
static inline CardLayerStack *MoMoMMAt(CardPlaySide *own, s32 off, s32 idx)
{
    return (CardLayerStack *)((char *)own + idx * 0x1BC + off);
}

s32 MoMoMove(s32 side, CardGameWork *work, s32 slot)
{
  s32 i;
  CardPlaySide *own;
  s32 found = -1;
  s32 life[4];
  s32 attack[4];
  s32 r1;
  s32 r2;
  s32 swapA;
  s32 cmp;
  CardLayerStack *dstp;
  CardLayerStack *srcp;
  s32 fcid;
  own = &work->player;
  if (side)
  {
    own = &work->enemy;
  }
  for (i = 0; i < 4; i++)
  {
    if (own->battle[i].battle.layers[0].cardId == 0x51)
    {
      found = i;
      break;
    }
  }

  if (found == (-1))
  {
    return 0;
  }
  if ((slot & 0xF0000) == 0)
  {
    if (CheckNowPower(side, work, found | 0xF0000, own->battle[found].battle.layers[0].cardId) == 0)
    {
      if (own->disposal[slot].battle.flags & 2)
      {
        if (CardPlayYamaCnt(own) >= 11)
        {
          if (CheckDownKaifukuCommand(side, work) == 0)
          {
            return 1;
          }
        }
      }
    }
    switch (found)
    {
      case 0:
        MoMoMMMove(&own->battle[2].battle, &own->battle[0].battle);
        MoMoMMMove(&own->battle[0].battle, MoMoMMAt(own, 0x19C, slot));
        break;

      case 1:
        MoMoMMMove(&own->battle[3].battle, &own->battle[1].battle);
        MoMoMMMove(&own->battle[1].battle, MoMoMMAt(own, 0x19C, slot));
        break;

      case 2:
        MoMoMMMove(&own->battle[0].battle, MoMoMMAt(own, 0x88C, slot));
        break;

      case 3:
        MoMoMMMove(&own->battle[1].battle, MoMoMMAt(own, 0x88C, slot));
        break;

      default:
      end:
        return 1;

    }

    return 1;
  }
  slot &= 0xFFFF;
  fcid = own->battle[found].battle.layers[0].cardId;
  if (CheckNowPower(side, work, found | 0xF0000, fcid) == 0)
  {
    if (((GetDeckDataAdjust(side, work, own->battle[slot].battle.layers[0].cardId) != 2) && (own->battle[slot].battle.flags & 2)) && (CardPlayYamaCnt(own) >= 11))
    {
      for (i = 0; i < 4; i++)
      {
        if (own->disposal[i].battle.layers[0].cardId < 0)
        {
          if (CheckDownKaifukuCommand(side, work) == 0)
          {
            MoMoMMMove(MoMoMMAt(own, 0x88C, i), MoMoMMAt(own, 0x19C, slot));
            return 1;
          }
        }
      }

    }
  }
  switch (found)
  {
    case 0:
      dstp = &own->battle[2].battle;
      srcp = &own->battle[0].battle;
      break;

    case 1:
      dstp = &own->battle[3].battle;
      srcp = &own->battle[1].battle;
      break;

    case 2:
      if (slot != 0)
    {
      MoMoMMMove(&own->battle[0].battle, MoMoMMAt(own, 0x19C, slot));
    }
      GetFriendAttack(side, work, life, attack);
      r1 = TodomeHyouka(side, work, 0, 0, life, attack);
      swapA = attack[3];
      attack[3] = attack[2];
      attack[2] = swapA;
      r2 = TodomeHyouka(side, work, 1, 0, life, attack);
      if (r2 > r1)
    {
      s16 id = own->battle[1].battle.layers[0].cardId;
      if ((id >= 0) && (!(GetDeckDataAdjust(side, work, id) < 4)))
      {
        goto c2ool;
      }
      c2move:
      MoMoMMMove(&own->battle[1].battle, &own->battle[0].battle);

      dstp = &own->battle[3].battle;
      srcp = &own->battle[2].battle;
      break;
      c2ool:
      if (r2 < 1001)
      {
        goto end;
      }

      i = 0;
      goto c2move;
    }
      goto end;

    case 3:
      if (slot != 1)
    {
      MoMoMMMove(&own->battle[1].battle, MoMoMMAt(own, 0x19C, slot));
    }
      GetFriendAttack(side, work, life, attack);
      r1 = TodomeHyouka(side, work, 1, 1, life, attack);
      swapA = attack[3];
      attack[3] = attack[2];
      attack[2] = swapA;
      r2 = TodomeHyouka(side, work, 0, 1, life, attack);
      if (r1 < r2)
    {
      s16 id3 = own->battle[0].battle.layers[0].cardId;
      if ((id3 >= 0) && (!(GetDeckDataAdjust(side, work, id3) < 4)))
      {
        goto c3ool;
      }
      c3move:
      MoMoMMMove(&own->battle[0].battle, &own->battle[1].battle);

      dstp = &own->battle[2].battle;
      srcp = &own->battle[3].battle;
      break;
      c3ool:
      if (r2 < 1001)
      {
        goto end;
      }

      i = 0;
      goto c3move;
    }
      goto end;

    default:
      return 1;

  }

  MoMoMMMove(dstp, srcp);
  return 1;
}

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

s32 CC_Kara_HaibiKabeMove(s32 side, CardGameWork *work, s32 slot);
s32 CC_Kara_KabeMove(s32 side, CardGameWork *work, s32 slot);

s32 CC_Kara_SensiMove(s32 side, CardGameWork *work, s32 slot)
{
    CardPlaySide *own;
    CardPlaySide *opp;
    s32 k;
    s32 type;
    s32 id;
    s32 p;
    s32 p2;
    s32 e;
    s32 t;
    s32 f;
    s32 d;
    s32 y;

    if (side == 0) {
        own = &work->player;
        opp = &work->enemy;
    } else {
        own = &work->enemy;
        opp = &work->player;
    }
    if (!(slot & 0xF0000)) {
        if ((own->disposal[slot].battle.flags & 2) && CardPlayYamaCnt(own) >= 11 &&
            CheckDownKaifukuCommand(side, work) == 0) {
            return 1;
        }
        type = CardChkAttackType(work->definitions, &own->disposal[slot].battle);
        if (type == 1) {
            if (opp->battle[1].battle.layers[0].cardId >= 0) {
                id = own->battle[0].battle.layers[0].cardId;
                if (id < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[0].battle, (CardMoveWork *)&own->disposal[slot].battle);
                    return 1;
                }
                p = CheckNowPower(side, work, 0xF0000, id);
                if (p < CheckNowPower(side, work, slot, own->disposal[slot].battle.layers[0].cardId) &&
                    !YoakimuMoMoCheck(side, work)) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[0].battle, (CardMoveWork *)&own->disposal[slot].battle);
                    return 1;
                }
            }
            if (opp->battle[0].battle.layers[0].cardId >= 0) {
                id = own->battle[1].battle.layers[0].cardId;
                if (id < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[1].battle, (CardMoveWork *)&own->disposal[slot].battle);
                    return 1;
                }
                p = CheckNowPower(side, work, 0xF0001, id);
                if (p < CheckNowPower(side, work, slot, own->disposal[slot].battle.layers[0].cardId) &&
                    !YoakimuMoMoCheck(side, work)) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[1].battle, (CardMoveWork *)&own->disposal[slot].battle);
                    return 1;
                }
            }
            if (BattleAreaCheckFriend(side, work) > 0) {
                CC_Kara_HaibiKabeMove(side, work, slot);
                return 1;
            }
            return 0;
        }
        for (k = 3; k >= 0; k--) {
            if (own->battle[k].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[k].battle, (CardMoveWork *)&own->disposal[slot].battle);
                return 1;
            }
            if (CardChkAttackType(work->definitions, &own->battle[k].battle) == 1) {
                if (k == 0 && opp->battle[1].battle.layers[0].cardId >= 0) {
                    p = CheckNowPower(side, work, 0xF0000, own->battle[0].battle.layers[0].cardId);
                    if (p >= CheckNowPower(side, work, slot, own->disposal[slot].battle.layers[0].cardId) &&
                        !YoakimuMoMoCheck(side, work)) {
                        continue;
                    }
                }
                if (k == 1 && opp->battle[0].battle.layers[0].cardId >= 0) {
                    p = CheckNowPower(side, work, 0xF0001, own->battle[1].battle.layers[0].cardId);
                    if (p >= CheckNowPower(side, work, slot, own->disposal[slot].battle.layers[0].cardId) &&
                        !YoakimuMoMoCheck(side, work)) {
                        continue;
                    }
                }
                if (GetDeckDataAdjust(side, work, own->battle[k].battle.layers[0].cardId) != 4) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[k].battle, (CardMoveWork *)&own->disposal[slot].battle);
                    return 1;
                }
            }
        }
        for (k = 3; k >= 0; k--) {
            if (GetDeckDataAdjust(side, work, own->battle[k].battle.layers[0].cardId) != 4) {
                p = CheckNowPower(side, work, k | 0xF0000, own->battle[k].battle.layers[0].cardId);
                p2 = CheckNowPower(side, work, slot, own->disposal[slot].battle.layers[0].cardId);
                if (own->battle[k].battle.layers[0].cardId >= 0) {
                    e = BattleAreaCheckEnemy(side, work);
                    if (CardChkAttackType(work->definitions, &own->battle[k].battle) == 3) {
                        p *= e;
                    }
                    if (*(s16 *)((char *)own + k * 0x1BC + 0x1AA) == 0x20) {
                        p += 3;
                    }
                    if (type == 3) {
                        p2 *= e;
                    }
                    if (own->disposal[slot].battle.layers[0].cardId == 0x20) {
                        p += 3;
                    }
                }
                if (p < p2 && !YoakimuMoMoCheck(side, work)) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[k].battle, (CardMoveWork *)&own->disposal[slot].battle);
                    return 1;
                }
            }
        }
        return 0;
    }
    slot &= 0xFFFF;
    if ((own->battle[slot].battle.flags & 2) && CardPlayYamaCnt(own) >= 11) {
        for (k = 0; k < 4; k++) {
            if (own->disposal[k].battle.layers[0].cardId < 0 && CheckDownKaifukuCommand(side, work) == 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&own->disposal[k].battle, (CardMoveWork *)&own->battle[slot].battle);
                return 1;
            }
        }
    }
    if ((u32)slot < 2) {
        t = CardChkAttackType(work->definitions, &own->battle[slot].battle);
        if (t == 1 && opp->battle[0].battle.layers[0].cardId < 0 && opp->battle[1].battle.layers[0].cardId < 0) {
            if (CardPlayYamaCnt(own) >= 11) {
                y = CardPlayYamaCnt(own);
                if (CheckEnemyAttack(side, work) < y) {
                    f = BattleAreaCheckFriend(side, work);
                    d = DisposeAreaCheckFriend(side, work);
                    if (f == t && d == 0) {
                        CGPMoveDSBATTLEwork((CardMoveWork *)&own->disposal[0].battle, (CardMoveWork *)&own->battle[slot].battle);
                        return 1;
                    }
                }
            }
            return CC_Kara_KabeMove(side, work, slot);
        }
    }
    CC_Kara_AttackMove(side, work, slot);
    return 0;
}

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

s32 CC_Kara_KabeMove(s32 side, CardGameWork *work, s32 slot)
{
    CardPlaySide *sd = &work->player;
    s32 p2;
    s32 p3;
    s32 friendCnt;
    s32 atk;
    s32 t;   /* attack type of the slot-2/3 card */
    CardLayerStack *cur;

    if (side) {
        sd = &work->enemy;
    }
    p2 = GetShoukin(&sd->battle[2].battle);
    slot &= 0xFFFF;
    p3 = GetShoukin(&sd->battle[3].battle);
    p2 += GetDeckData(side, (char *)work, sd->battle[2].battle.layers[0].cardId) * 10;
    p3 += GetDeckData(side, (char *)work, sd->battle[3].battle.layers[0].cardId) * 10;
    friendCnt = DisposeAreaCheckFriend(side, work);
    if (GetDeckData(side, (char *)work, sd->battle[2].battle.layers[0].cardId) == 4) {
        p2 += 100;
    }
    if (GetDeckData(side, (char *)work, sd->battle[3].battle.layers[0].cardId) == 4) {
        p3 += 100;
    }
    cur = &sd->battle[slot].battle;
    atk = CardChkAttackType(work->definitions, cur);

    switch (slot) {
    case 0:
        if (p2 < p3) {
            if (sd->battle[1].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[1].battle, (CardMoveWork *)&sd->battle[0].battle);
                return 1;
            }
            if (GetDeckDataAdjust(side, work, sd->battle[1].battle.layers[0].cardId) == 2 &&
                sd->battle[0].battle.layers[0].life > sd->battle[1].battle.layers[0].life &&
                !YoakimuMoMoCheck(side, work)) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[1].battle, (CardMoveWork *)&sd->battle[0].battle);
                return 1;
            }
        }
        if (sd->battle[2].battle.layers[0].cardId >= 0 &&
            GetDeckDataAdjust(side, work, sd->battle[2].battle.layers[0].cardId) == 2) {
            t = CardChkAttackType(work->definitions, &sd->battle[2].battle);
            if (CardChkAttackType(work->definitions, &sd->battle[0].battle) != 1 && t == 1) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[2].battle, (CardMoveWork *)&sd->battle[0].battle);
                return 1;
            }
        }
        if (atk != 1 && !CheckFriendSensi(side, work) &&
            sd->battle[2].battle.layers[0].cardId < 0 &&
            sd->battle[1].battle.layers[0].cardId >= 0 &&
            sd->battle[3].battle.layers[0].cardId >= 0) {
            CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[2].battle, (CardMoveWork *)&sd->battle[0].battle);
            return 1;
        }
        break;
    case 1:
        if (p3 < p2) {
            if (sd->battle[0].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[0].battle, (CardMoveWork *)&sd->battle[1].battle);
                return 1;
            }
            if (GetDeckDataAdjust(side, work, sd->battle[0].battle.layers[0].cardId) == 2 &&
                sd->battle[1].battle.layers[0].life > sd->battle[0].battle.layers[0].life &&
                !YoakimuMoMoCheck(side, work)) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[0].battle, (CardMoveWork *)cur);
                return 1;
            }
        }
        if (sd->battle[3].battle.layers[0].cardId >= 0 &&
            GetDeckDataAdjust(side, work, sd->battle[3].battle.layers[0].cardId) == 2) {
            t = CardChkAttackType(work->definitions, &sd->battle[3].battle);
            if (CardChkAttackType(work->definitions, &sd->battle[1].battle) != 1 && t == 1) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[2].battle, (CardMoveWork *)&sd->battle[0].battle);
                return 1;
            }
        }
        if (atk != 1 && !CheckFriendSensi(side, work) &&
            sd->battle[3].battle.layers[0].cardId < 0 &&
            sd->battle[0].battle.layers[0].cardId >= 0 &&
            sd->battle[2].battle.layers[0].cardId >= 0) {
            CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[3].battle, (CardMoveWork *)&sd->battle[1].battle);
            return 1;
        }
        break;
    case 2:
    case 3:
        if (friendCnt > 0 && CheckFriendSensi(side, work) == 1) {
            if (sd->battle[0].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[0].battle, (CardMoveWork *)&sd->battle[slot].battle);
                return 1;
            }
            if (sd->battle[1].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[1].battle, (CardMoveWork *)&sd->battle[slot].battle);
                return 1;
            }
        }
        if (sd->battle[0].battle.layers[0].cardId >= 0 &&
            GetDeckDataAdjust(side, work, sd->battle[0].battle.layers[0].cardId) == 2 &&
            sd->battle[0].battle.layers[0].life < sd->battle[slot].battle.layers[0].life &&
            CardChkAttackType(work->definitions, &sd->battle[0].battle) != 1 &&
            !YoakimuMoMoCheck(side, work)) {
            CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[0].battle, (CardMoveWork *)&sd->battle[slot].battle);
            return 1;
        }
        if (sd->battle[1].battle.layers[0].cardId >= 0 &&
            GetDeckDataAdjust(side, work, sd->battle[1].battle.layers[0].cardId) == 2 &&
            sd->battle[1].battle.layers[0].life < sd->battle[slot].battle.layers[0].life &&
            CardChkAttackType(work->definitions, &sd->battle[1].battle) != 1 &&
            !YoakimuMoMoCheck(side, work)) {
            CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[1].battle, (CardMoveWork *)&sd->battle[slot].battle);
            return 1;
        }
        if (slot == 3 && sd->battle[0].battle.layers[0].cardId < 0 &&
            sd->battle[2].battle.layers[0].cardId >= 0) {
            CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[0].battle, (CardMoveWork *)&sd->battle[3].battle);
            return 1;
        }
        if (slot == 2 && sd->battle[1].battle.layers[0].cardId < 0 &&
            sd->battle[3].battle.layers[0].cardId >= 0) {
            CGPMoveDSBATTLEwork((CardMoveWork *)&sd->battle[1].battle, (CardMoveWork *)&sd->battle[2].battle);
            return 1;
        }
        return 0;
    }
    return 0;
}

extern s32 CC_Kara_MoveFase_Type4(s32, CardGameWork *, s32);

s32 EscapeMove(s32 side, CardGameWork *work, s32 slot)
{
  /* diag run 2 */
  CardPlaySide *temp_20;
  s16 *var_4;
  s32 temp_2_2;
  s32 temp_16;
  s32 temp_16_2;
  s32 temp_17;
  s32 temp_23;
  s32 temp_2;
  s32 temp_3;
  s32 temp_3_2;
  s32 var_5;
  s32 var_6_2;
  s32 var_6;
  s32 var_7;
  temp_20 = (side == 0) ? (&work->player) : (&work->enemy);
  temp_16 = DisposeAreaCheckFriend(side, work);
  temp_2 = BattleAreaCheckFriend(side, work);
  if ((temp_16 + temp_2) >= 3)
  {
    return 0;
  }
  {
    s32 temp_12 = CheckEnemy12Attack(side == 0, work);
    if (temp_12 >= 3)
    {
      return 0;
    }
    temp_16_2 = BattleAreaCheckEnemy(side, work);
    if ((temp_16_2 + DisposeAreaCheckEnemy(side, work)) >= 2)
    {
      if (CheckFriendKabe(side, work) != 1)
      {
        temp_23 = CheckEnemy12Attack(side, work);
        if (!(slot & 0xF0000))
        {
          temp_3_2 = CardChkAttackType(work->definitions, (CardPowerCard *) (&temp_20->disposal[slot].battle));
          if (((s8) temp_20->disposal[slot].battle.power) < 2)
          {
            if ((temp_3_2 != 3) || (BattleAreaCheckEnemy(side, work) < 2))
            {
              goto block_11;
            }
            return 0;
          }
          block_11:
          if (temp_23 >= ((s8) temp_20->disposal[slot].battle.layers[0].life))
          {
            CC_Kara_MoveFase_Type4(side, work, slot);
            return 1;
          }

          return 0;
        }
        temp_17 = slot & 0xFFFF;
        temp_3 = CardChkAttackType(work->definitions, (CardPowerCard *) (&temp_20->battle[temp_17].battle));
        if (((s8) temp_20->battle[temp_17].battle.power) < 2)
        {
          if ((temp_3 != 3) || (BattleAreaCheckEnemy(side, work) < 2))
          {
            goto block_18;
          }
          return 0;
        }
        block_18:
        if (temp_2 == 1)
        {
          temp_3 = temp_20->battle[temp_17].battle.layers[0].life;
          if (temp_23 >= temp_3)
          {
            for (var_5 = 0; var_5 < 4; var_5 += 1)
            {
              if (temp_20->disposal[var_5].battle.layers[0].cardId < 0)
              {
                CGPMoveDSBATTLEwork((CardMoveWork *) (&temp_20->disposal[var_5].battle), (CardMoveWork *) (&temp_20->battle[temp_17].battle));
                return 1;
              }
            }
            return 0;
          }
          return 0;
        }

        var_6_2 = temp_17 | 0xF0000;
        CC_Kara_MoveFase_Type4(side, work, var_6_2);
        return 1;
      }
      return 0;
    }
  }
  return 0;
}

s32 LvUpCheck(s32 side, CardGameWork *work, s32 slot)
{
    CardPlaySide *ps;
    s32 cnt;
    s32 i;

    ps = (side == 0) ? &work->player : &work->enemy;
    cnt = CardPlayHandCnt(&ps->hand);
    if (!(slot & 0xF0000)) {
        switch (ps->disposal[slot].battle.layers[0].cardId) {
        case 0:
            for (i = 0; i < cnt; i++) {
                if (ps->hand.cards[i] == 7) {
                    return 3;
                }
            }
            break;
        case 2:
            for (i = 0; i < cnt; i++) {
                if (ps->hand.cards[i] == 9) {
                    return 2;
                }
            }
            break;
        case 3:
            for (i = 0; i < cnt; i++) {
                if (ps->hand.cards[i] == 10) {
                    return 3;
                }
            }
            break;
        case 4:
            for (i = 0; i < cnt; i++) {
                if (ps->hand.cards[i] == 11) {
                    return -1;
                }
            }
            break;
        case 5:
            for (i = 0; i < cnt; i++) {
                if (ps->hand.cards[i] == 12) {
                    return 2;
                }
            }
            break;
        case 6:
            for (i = 0; i < cnt; i++) {
                if (ps->hand.cards[i] == 13) {
                    return 3;
                }
            }
            break;
        }
    } else {
        switch (ps->battle[(u16)slot].battle.layers[0].cardId) {
        case 0:
            for (i = 0; i < cnt; i++) {
                if (ps->hand.cards[i] == 7) {
                    return 3;
                }
            }
            break;
        case 2:
            for (i = 0; i < cnt; i++) {
                if (ps->hand.cards[i] == 9) {
                    return 2;
                }
            }
            break;
        case 3:
            for (i = 0; i < cnt; i++) {
                if (ps->hand.cards[i] == 10) {
                    return 3;
                }
            }
            break;
        case 4:
            for (i = 0; i < cnt; i++) {
                if (ps->hand.cards[i] == 11) {
                    return -1;
                }
            }
            break;
        case 5:
            for (i = 0; i < cnt; i++) {
                if (ps->hand.cards[i] == 12) {
                    return 2;
                }
            }
            break;
        case 6:
            for (i = 0; i < cnt; i++) {
                if (ps->hand.cards[i] == 13) {
                    return 3;
                }
            }
            break;
        }
    }
    return -1;
}

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

s32 MoMoMove(s32 side, CardGameWork *work, s32 slot);
s32 CC_Kara_SensiMove(s32 side, CardGameWork *work, s32 slot);
s32 CC_Kara_FrontTodomeMove(s32 side, CardGameWork *work, s32 slot);
#define MOVE(a, b) { CGPMoveDSBATTLEwork((CardMoveWork *)(a), (CardMoveWork *)(b)); return 1; }

s32 CC_Kara_MoveFase_Type2(s32 side, CardGameWork *work, s32 slot)
{
    CardPlaySide *own;
    CardPlaySide *other;
    s32 idx;
    s32 k;
    s32 bf;
    s32 df;
    s32 c;

    if (side == 0) {
        other = &work->enemy;
        own = &work->player;
    } else {
        other = &work->player;
        own = &work->enemy;
    }
    if ((slot & 0xF0000) == 0) {
        c = own->disposal[slot].battle.layers[0].cardId;
        if (c == 3 || c == 8 || c == 10) {
            if (MoMoMove(side, work, slot) == 1) {
                return 1;
            }
        }
        if (own->disposal[slot].battle.layers[0].cardId >= 0
            && own->disposal[slot].battle.layers[1].cardId != 0x2C
            && own->disposal[slot].battle.layers[1].cardId != 0x2F) {
            CC_Kara_SensiMove(side, work, slot);
            return 1;
        }
    } else {
        idx = slot & 0xFFFF;
        c = own->battle[idx].battle.layers[0].cardId;
        if (c == 3 || c == 8 || c == 10) {
            if (MoMoMove(side, work, idx | 0xF0000) == 1) {
                return 1;
            }
        }
        if (own->battle[idx].battle.layers[0].cardId >= 0
            && own->battle[idx].battle.layers[1].cardId != 0x2C
            && own->battle[idx].battle.layers[1].cardId != 0x2F) {
            CC_Kara_SensiMove(side, work, slot);
            return 1;
        }
    }
    if ((slot & 0xF0000) == 0) {
        if (own->battle[3].battle.layers[0].cardId >= 0) {
            if (own->battle[1].battle.layers[0].cardId < 0) {
                MOVE(&own->battle[1].battle, &own->disposal[slot].battle);
            }
            if (own->battle[0].battle.layers[0].cardId < 0) {
                MOVE(&own->battle[0].battle, &own->disposal[slot].battle);
            }
            if (YoakimuMoMoCheck(side, work) == 0
                && GetDeckDataAdjust(side, work, own->battle[1].battle.layers[0].cardId) == 2
                && own->battle[1].battle.layers[0].life < own->disposal[slot].battle.layers[0].life) {
                MOVE(&own->battle[1].battle, &own->disposal[slot].battle);
            }
        } else if (own->battle[2].battle.layers[0].cardId >= 0) {
            if (own->battle[0].battle.layers[0].cardId < 0) {
                MOVE(&own->battle[0].battle, &own->disposal[slot].battle);
            }
            if (own->battle[1].battle.layers[0].cardId < 0) {
                MOVE(&own->battle[1].battle, &own->disposal[slot].battle);
            }
            if (YoakimuMoMoCheck(side, work) == 0) {
                if (GetDeckDataAdjust(side, work, own->battle[0].battle.layers[0].cardId) == 2
                    && own->battle[0].battle.layers[0].life < own->disposal[slot].battle.layers[0].life) {
                    MOVE(&own->battle[0].battle, &own->disposal[slot].battle);
                }
                if (GetDeckDataAdjust(side, work, own->battle[1].battle.layers[0].cardId) == 2
                    && own->battle[1].battle.layers[0].life < own->disposal[slot].battle.layers[0].life) {
                    MOVE(&own->battle[1].battle, &own->disposal[slot].battle);
                }
            }
        }
        if (CardChkAttackType(work->definitions, &own->disposal[slot].battle) != 1) {
            for (k = 3; k >= 0; k--) {
                if (own->battle[k].battle.layers[0].cardId < 0) {
                    MOVE(&own->battle[k].battle, &own->disposal[slot].battle);
                }
            }
        } else {
        if (other->battle[0].battle.layers[0].cardId >= 0 && own->battle[1].battle.layers[0].cardId < 0) {
            MOVE(&own->battle[1].battle, &own->disposal[slot].battle);
        }
        if (other->battle[1].battle.layers[0].cardId >= 0 && own->battle[0].battle.layers[0].cardId < 0) {
            MOVE(&own->battle[0].battle, &own->disposal[slot].battle);
        }
        for (k = 0; k < 4; k++) {
            if (own->battle[k].battle.layers[0].cardId < 0) {
                MOVE(&own->battle[k].battle, &own->disposal[slot].battle);
            }
        }
        }
        return 1;
    }
    bf = BattleAreaCheckFriend(side, work);
    df = DisposeAreaCheckFriend(side, work);
    slot &= 0xFFFF;
    if (bf == 1 && df == 0) {
        CC_Kara_AttackMove(side, work, slot);
        return 1;
    }
    if ((u32)slot < 2 && own->battle[0].battle.layers[0].cardId >= 0 && own->battle[1].battle.layers[0].cardId >= 0) {
        for (k = 0; k < 4; k++) {
            if (own->disposal[k].battle.layers[0].cardId >= 0 && GetDeckDataAdjust(side, work, own->disposal[k].battle.layers[0].cardId) == 2
                && own->battle[slot].battle.layers[0].life < own->disposal[k].battle.layers[0].life) {
                MOVE(&own->disposal[k].battle, &own->battle[slot].battle);
            }
        }
    }
    if (other->battle[0].battle.layers[0].cardId >= 0 || other->battle[1].battle.layers[0].cardId >= 0) {
        if (YoakimuMoMoCheck(side, work) == 0) {
            CC_Kara_FrontTodomeMove(side, work, slot);
            return 1;
        }
    }
    CC_Kara_KabeMove(side, work, slot);
    return 0;
}

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

s32 CC_Kara_MoveFase_Type4(s32 side, CardGameWork *work, s32 slot)
{
    CardPlaySide *sideData = &work->player;
    s32 i;

    if (side) {
        sideData = &work->enemy;
    }
    if (YoakimuMove(side, work, slot) == 1) {
        return 1;
    }
    if (CardPlayYamaCnt(sideData) < CheckEnemyAttack(side, work)) {
        return CC_Kara_MoveFase_Type3(side, work, slot);
    }
    if (CheckFriendType4(side, work) >= 4) {
        return CC_Kara_SensiMove(side, work, slot);
    }
    if ((slot & 0xF0000) == 0) {
        if (sideData->battle[0].battle.layers[0].cardId >= 0) {
            if (sideData->battle[2].battle.layers[0].cardId < 0
                || GetDeckData(side, (s8 *)work, sideData->battle[2].battle.layers[0].cardId) < 4U) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&sideData->battle[2].battle,
                                    (CardMoveWork *)&sideData->disposal[slot].battle);
                return 1;
            }
        }
        if (sideData->battle[1].battle.layers[0].cardId >= 0) {
            if (sideData->battle[3].battle.layers[0].cardId < 0
                || GetDeckData(side, (s8 *)work, sideData->battle[3].battle.layers[0].cardId) < 4U) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&sideData->battle[3].battle,
                                    (CardMoveWork *)&sideData->disposal[slot].battle);
                return 1;
            }
        }
        return 0;
    }
    slot &= 0xFFFF;
    if (slot == 0 || slot == 1) {
        for (i = 0; i < 4; i++) {
            if (sideData->disposal[i].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&sideData->disposal[i].battle,
                                    (CardMoveWork *)&sideData->battle[slot].battle);
                return 1;
            }
        }
    } else if (slot == 2 && sideData->battle[0].battle.layers[0].cardId < 0) {
        for (i = 0; i < 4; i++) {
            if (sideData->disposal[i].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&sideData->disposal[i].battle,
                                    (CardMoveWork *)&sideData->battle[slot].battle);
                return 1;
            }
        }
    } else if (slot == 3 && sideData->battle[1].battle.layers[0].cardId < 0) {
        for (i = 0; i < 4; i++) {
            if (sideData->disposal[i].battle.layers[0].cardId < 0) {
                CGPMoveDSBATTLEwork((CardMoveWork *)&sideData->disposal[i].battle,
                                    (CardMoveWork *)&sideData->battle[slot].battle);
                return 1;
            }
        }
    }
    if (sideData->battle[0].battle.layers[0].cardId >= 0) {
        if (sideData->battle[1].battle.layers[0].cardId >= 0) {
            CC_Kara_BackAttackMove(side, work, slot);
            return 1;
        }
    }
    return 0;
}

s32 CC_Kara_MoveFase_2(s32 arg0, CardGameWork *arg1)
{
    CardPlaySide *side;
    s32 sp0;
    s32 sp4;
    s32 i;
    s32 k;
    s32 t;

    side = &arg1->player;
    if (arg0 != 0) {
        side = &arg1->enemy;
    }
    sp0 = BattleAreaCheckEnemy(arg0, arg1);
    sp4 = DisposeAreaCheckEnemy(arg0, arg1);
    for (i = 0; i < 4; i++) {
        if (side->disposal[i].battle.layers[0].cardId < 0) {
            continue;
        }
        if (sp0 < 2) {
            if (CardPlayYamaCnt(side) >= 11) {
                t = side->disposal[i].battle.layers[1].cardId;
                if ((s8)side->disposal[i].battle.power < 2) {
                    if (t == 0x35 || t == 0x30 || t == 0x2E) {
                        continue;
                    }
                }
            }
        }
        if (sp0 + sp4 == 0) {
            CC_Kara_MoveFase_Type3(arg0, arg1, i);
            continue;
        }
        switch (GetDeckData(arg0, (char *)arg1, side->disposal[i].battle.layers[0].cardId)) {
        case 0:
            CC_Kara_MoveFase_Type0(arg0, arg1, i);
            break;
        case 1:
            CC_Kara_MoveFase_Type1(arg0, arg1, i);
            break;
        case 2:
            CC_Kara_MoveFase_Type2(arg0, arg1, i);
            break;
        case 3:
            CC_Kara_MoveFase_Type3(arg0, arg1, i);
            break;
        case 4:
            CC_Kara_MoveFase_Type4(arg0, arg1, i);
            break;
        }
    }
    for (i = 0; i < 4; i++) {
        if (side->battle[i].battle.layers[0].cardId < 0) {
            continue;
        }
        if (sp0 + sp4 == 0) {
            CC_Kara_MoveFase_Type3(arg0, arg1, i | 0xF0000);
            continue;
        }
        if (sp0 < 2) {
            if (CardPlayYamaCnt(side) >= 11) {
                t = side->battle[i].battle.layers[1].cardId;
                if ((s8)side->battle[i].battle.power < 2) {
                    if (t == 0x35 || t == 0x30 || t == 0x2E) {
                        for (k = 0; k < 4; k++) {
                            if (side->disposal[k].battle.layers[0].cardId < 0) {
                                CGPMoveDSBATTLEwork((CardMoveWork *)&side->disposal[k].battle,
                                                    (CardMoveWork *)&side->battle[i].battle);
                            }
                        }
                    }
                }
            }
        }
        switch (GetDeckData(arg0, (char *)arg1, side->battle[i].battle.layers[0].cardId)) {
        case 0:
            CC_Kara_MoveFase_Type0(arg0, arg1, i | 0xF0000);
            break;
        case 1:
            CC_Kara_MoveFase_Type1(arg0, arg1, i | 0xF0000);
            break;
        case 2:
            CC_Kara_MoveFase_Type2(arg0, arg1, i | 0xF0000);
            break;
        case 3:
            CC_Kara_MoveFase_Type3(arg0, arg1, i | 0xF0000);
            break;
        case 4:
            CC_Kara_MoveFase_Type4(arg0, arg1, i | 0xF0000);
            break;
        }
    }
    return 0;
}

s32 CC_Kara_MoveFase_1(s32 side, CardGameWork *work)
{
    CardPlaySide *ownSide;
    CardPlaySide *otherSide;
    s32 i;
    s32 j;

    if (side == 0) {
        otherSide = &work->enemy;
        ownSide = &work->player;
    } else {
        otherSide = &work->player;
        ownSide = &work->enemy;
    }
    for (i = 0; i < 4; i++) {
        if ((ownSide->battle[i].battle.layers[0].cardId >= 0) && (ownSide->battle[i].battle.flags & 2)) {
            for (j = 0; j < 4; j++) {
                if (ownSide->disposal[j].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&ownSide->disposal[j].battle,
                                        (CardMoveWork *)&ownSide->battle[i].battle);
                }
            }
        }
    }
    for (i = 0; i < 4; i++) {
        if ((ownSide->disposal[i].battle.layers[0].cardId >= 0) && (!(ownSide->disposal[i].battle.flags & 2))) {
            if (CardChkAttackType(work->definitions, &ownSide->disposal[i].battle) == 1) {
                if (ownSide->battle[0].battle.layers[0].cardId < 0) {
                    if ((ownSide->battle[1].battle.layers[0].cardId < 0) &&
                        (otherSide->battle[0].battle.layers[0].cardId >= 0) &&
                        (otherSide->battle[1].battle.layers[0].cardId < 0)) {
                        CGPMoveDSBATTLEwork((CardMoveWork *)&ownSide->battle[1].battle,
                                            (CardMoveWork *)&ownSide->disposal[i].battle);
                    } else {
                        CGPMoveDSBATTLEwork((CardMoveWork *)&ownSide->battle[0].battle,
                                            (CardMoveWork *)&ownSide->disposal[i].battle);
                    }
                    return 1;
                }
                if (ownSide->battle[1].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&ownSide->battle[1].battle,
                                        (CardMoveWork *)&ownSide->disposal[i].battle);
                    return 1;
                }
            } else {
                if (ownSide->battle[2].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&ownSide->battle[2].battle,
                                        (CardMoveWork *)&ownSide->disposal[i].battle);
                    return 1;
                }
                j = ownSide->battle[3].battle.layers[0].cardId < 0;
                if (j) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&ownSide->battle[3].battle,
                                        (CardMoveWork *)&ownSide->disposal[i].battle);
                    return 1;
                }
                if (ownSide->battle[0].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&ownSide->battle[0].battle,
                                        (CardMoveWork *)&ownSide->disposal[i].battle);
                    return 1;
                }
                if (ownSide->battle[1].battle.layers[0].cardId < 0) {
                    CGPMoveDSBATTLEwork((CardMoveWork *)&ownSide->battle[1].battle,
                                        (CardMoveWork *)&ownSide->disposal[i].battle);
                    return 1;
                }
            }
        }
    }
    return 0;
}

extern s32 CGPDispoSub0(s32 side, CardGameWork *work, s32 index);

s32 CheckComboMove(s32 side, CardGameWork *work)
{
    s32 life[4];
    s32 attack[4];
    CardPlaySide *other;
    CardPlaySide *own;
    s32 handCount;
    CardDefinition *defs;
    s32 i, j, k;
    s32 cid;
    s32 id;
    s16 *p;
    s32 yama;
    s32 flagA, flagB, flagC, found, cnt, num;
    s32 four, eight, two;

    if (side == 0) {
        other = &work->enemy;
        own = &work->player;
    } else {
        other = &work->player;
        own = &work->enemy;
    }
    play413 = 0;
    defs = work->definitions;
    playteppeki = 0;
    handCount = CardPlayHandCnt(&own->hand);
    if (BattleAreaCheckFriend(side, work) + DisposeAreaCheckFriend(side, work) == 0) {
        return 0;
    }
    GetFriendAttack(side, work, life, attack);
    if (TodomeHyouka(side, work, 0, 0, life, attack) > 1000) {
        return 0;
    }
    for (i = 0; i < handCount; i++) {
        if (own->hand.cards[i] == 0x66 && CheckCommandPlay(side, work, 0x66) == 1 &&
            BattleAreaCheckEnemy(side, work) > 0) {
            for (k = 0, four = 4, eight = 8; k < 4; k++) {
                cid = own->battle[k].battle.layers[0].cardId;
                if (cid >= 0 && (defs[cid].flags == four || defs[cid].flags == eight)) {
                    for (j = 0; j < 4; j++) {
                        id = own->disposal[j].battle.layers[0].cardId;
                        if (id < 0) {
                            CGPMoveDSBATTLEwork((CardMoveWork *)&own->disposal[j].battle,
                                                (CardMoveWork *)&own->battle[k].battle);
                            break;
                        }
                        if (defs[id].flags != four && defs[id].flags != eight) {
                            CGPMoveDSBATTLEwork((CardMoveWork *)&own->disposal[j].battle,
                                                (CardMoveWork *)&own->battle[k].battle);
                            break;
                        }
                    }
                }
            }
            if (CheckSouteniHouCommand(side, work) == 1) {
                return 1;
            }
        }
    }

    yama = CardPlayYamaCnt(own);
    flagA = 0;
    flagB = 0;
    flagC = 0;
    for (i = 0; i < handCount; i++) {
        if (yama >= 2) {
            if (own->hand.cards[i] == 0x65 && CheckCommandPlay(side, work, 0x65) == 1) {
                flagA = 1;
            }
            if (own->hand.cards[i] == 0x7D && CGPDispoSub0(side, work, i) == 0) {
                flagB = 1;
            }
        }
    }

    if (flagA == 1 && flagB == 1) {
        cnt = 0;
        for (i = 0, two = 2; i < 4; i++) {
            cid = own->disposal[i].battle.layers[0].cardId;
            if (cid >= 0 && defs[cid].flags == two) {
                for (j = 0; j < 4; j++) {
                    if (own->battle[j].battle.layers[0].cardId < 0) {
                        cnt++;
                        CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[j].battle,
                                            (CardMoveWork *)&own->disposal[i].battle);
                        break;
                    }
                    if (defs[own->battle[j].battle.layers[0].cardId].flags != two) {
                        CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[j].battle,
                                            (CardMoveWork *)&own->disposal[i].battle);
                        cnt++;
                        break;
                    }
                }
            }
        }
        if (cnt >= 2) {
            return 1;
        }
        if (cnt == 1 && CardPlayYamaCnt(other) < 13) {
            return 1;
        }
    }

    for (i = 0; i < 4; i++) {
        s32 dcard = own->disposal[i].battle.layers[0].cardId;

        if (dcard == 0x36) {
            flagC = 1;
            break;
        }
        if (own->battle[i].battle.layers[0].cardId == 0x36) {
            flagC = 1;
            break;
        }
    }

    if (flagB == 1 && flagC == 1) {
        found = 0;
        for (i = 0; i < 4; i++) {
            cid = own->disposal[i].battle.layers[0].cardId;
            if (cid >= 0 && defs[cid].flags == 2) {
                for (j = 0; j < 4; j++) {
                    if (own->battle[j].battle.layers[0].cardId < 0) {
                        CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[j].battle,
                                            (CardMoveWork *)&own->disposal[i].battle);
                        break;
                    }
                    if (defs[own->battle[j].battle.layers[0].cardId].flags != 2) {
                        CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[j].battle,
                                            (CardMoveWork *)&own->disposal[i].battle);
                        break;
                    }
                }
            }
        }
        for (i = 0; i < 4; i++) {
            if (own->disposal[i].battle.layers[0].cardId == 0x36) {
                for (j = 0; j < 4; j++) {
                    cid = own->battle[j].battle.layers[0].cardId;
                    if (cid < 0) {
                        CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[j].battle,
                                            (CardMoveWork *)&own->disposal[i].battle);
                        break;
                    }
                    if (defs[cid].flags != 2) {
                        CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[j].battle,
                                            (CardMoveWork *)&own->disposal[i].battle);
                        break;
                    }
                    if (j == 3) {
                        CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[j].battle,
                                            (CardMoveWork *)&own->disposal[i].battle);
                        break;
                    }
                }
                break;
            }
        }
        for (i = 0; i < 4; i++) {
            if (own->battle[i].battle.layers[0].cardId == 0x36) {
                found = 1;
                break;
            }
        }
        num = 0;
        for (i = 0; i < 4; i++) {
            if (own->battle[i].battle.layers[0].cardId >= 0 && defs[own->battle[i].battle.layers[0].cardId].flags == 2) {
                num++;
            }
        }
        if (num > 0 && found == 1) {
            return 1;
        }
    }
    return 0;
}

extern int CardPlayCntOpe(CardPlaySide *side, int opeId);
s32 CheckGuno(s32 side, CardGameWork *work);
s32 CheckREAL(s32 side, CardGameWork *work);
s32 GainanCheck(s32 side, CardGameWork *work);
s32 CheckComboMove(s32 side, CardGameWork *work);
s32 CC_Kara_MoveFase_2(s32 arg0, CardGameWork *arg1);

s32 CC_Kara_MoveFase(s32 side, CardGameWork *work)
{
    CardPlaySide *own;
    CardPlaySide *opp;
    CardPlaySide *pl;
    CardPlaySide *en;
    s32 i;
    s32 j;
    s32 k;
    s32 n;

    if (side == 0) {
        own = &work->player;
        opp = &work->enemy;
        pl = own;
        en = opp;
    } else {
        own = &work->enemy;
        opp = &work->player;
        en = own;
        pl = opp;
    }
    if (CheckComboMove(side, work) == 1) {
        return 0;
    }
    if (CardPlayCntOpe(pl, 0x91) || CardPlayCntOpe(en, 0x91)) {
        if (CheckGuno(side, work)) {
            for (i = 0; i < 4; i++) {
                if (own->disposal[i].battle.layers[0].cardId >= 0 &&
                    (work->definitions[own->disposal[i].battle.layers[0].cardId].flags & 4)) {
                    for (j = 0; j < 4; j++) {
                        if (own->battle[j].battle.layers[0].cardId < 0) {
                            CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[j].battle,
                                                (CardMoveWork *)&own->disposal[i].battle);
                            break;
                        }
                    }
                }
            }
            return 0;
        }
    }
    if (CardPlayCntOpe(pl, 0x92) || CardPlayCntOpe(en, 0x92)) {
        for (i = 0; i < 4; i++) {
            if (own->disposal[i].battle.layers[0].cardId >= 0) {
                for (j = 0; j < 4; j++) {
                    if (own->battle[j].battle.layers[0].cardId < 0) {
                        CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[j].battle,
                                            (CardMoveWork *)&own->disposal[i].battle);
                        break;
                    }
                }
            }
        }
        return 0;
    }
    if (CardPlayCntOpe(pl, 0x93) || CardPlayCntOpe(en, 0x93)) {
        if (CheckREAL(side, work)) {
            for (i = 0; i < 4; i++) {
                if (own->disposal[i].battle.layers[0].cardId >= 0 &&
                    (work->definitions[own->disposal[i].battle.layers[0].cardId].flags & 2)) {
                    for (j = 0; j < 4; j++) {
                        if (own->battle[j].battle.layers[0].cardId < 0) {
                            CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[j].battle,
                                                (CardMoveWork *)&own->disposal[i].battle);
                            break;
                        }
                    }
                }
            }
            return 0;
        }
    }
    if (CardPlayCntOpe(pl, 0x78) || CardPlayCntOpe(en, 0x78)) {
        if (CheckGuno(side, work)) {
            return 0;
        }
    }
    if (CardPlayCntOpe(pl, 0x8A)) {
        return 0;
    }
    if (CardPlayCntOpe(en, 0x8A)) {
        return 0;
    }
    if (CheckGuno(side, work)) {
        for (i = 0; i < 4; i++) {
            if (own->battle[i].battle.layers[0].cardId == 10 ||
                opp->battle[i].battle.layers[0].cardId == 10) {
                for (n = 0; n < 4; n++) {
                    if (own->disposal[n].battle.layers[0].cardId >= 0) {
                        for (j = 0; j < 4; j++) {
                            if (own->battle[j].battle.layers[0].cardId < 0) {
                                CGPMoveDSBATTLEwork((CardMoveWork *)&own->battle[j].battle,
                                                    (CardMoveWork *)&own->disposal[n].battle);
                                break;
                            }
                        }
                    }
                }
                return 0;
            }
        }
    }
    if (GainanCheck(side, work) == 1) {
        return 0;
    }
    for (i = 9; i >= 0; i--) {
        CC_Kara_MoveFase_2(side, work);
    }
    return 0;
}

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

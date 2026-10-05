#include "common.h"
#include "shared.h"
#include "main/party.h"
#include "menu_skill.h"

int SkillSetLvGet(int skill_id)
{
    int skill_index = (unsigned short)skill_id;
    MenuSkillData *skill_data = (MenuSkillData *)SkillDataBuf;

    if (skill_index <= 0)
        return 0;
    return skill_data->level_cap_by_skill_id[skill_index - 1];
}

int SkillGetPtGet(int skill_id)
{
    int skill_index = (unsigned short)skill_id;
    MenuSkillData *skill_data = (MenuSkillData *)SkillDataBuf;

    if (skill_index <= 0)
        return 0;
    return skill_data->point_cost_by_skill_id[skill_index];
}

/*
 * Offset of the per-level "points required for next skill level" table inside
 * the loaded skill data block (SkillDataBuf). Evidenced by the original
 * SkillNextLvGet (`lhu $2, 382($4)`, 382 = 0x17e);
 */
#define SKILL_NEXT_LV_TABLE_OFFSET 0x17e

unsigned short SkillNextLvGet(int level)
{
    return ((unsigned short *)(SkillDataBuf + SKILL_NEXT_LV_TABLE_OFFSET))[level];
}

int SkillCharSkillLvGet(int character_id)
{
    unsigned char *party = PartyDataGet();
    PartySkillLevelArray *skill_levels;
    int character_index = (unsigned short)character_id;

    skill_levels = (PartySkillLevelArray *)(party + 0x140);
    if (character_index == 0)
        return 0;
    if (character_index >= 9)
        return 0;
    return skill_levels->level_by_character[character_id - 1];
}

int SkillCharPointPlus(int character_id, int point_delta)
{
    int character_index = (unsigned short)character_id;
    PlayerCharacter *character;
    unsigned char *party_bytes;
    unsigned char *level_by_character;
    PartySkillPoints *points_to_next_level;
    int level;
    int character_points;
    int remaining_points;
    PartySkillPoints next_level_points;

    if (character_index == 0 || character_index >= 9)
        return 0;

    character = (PlayerCharacter *)dataPlChaGet(character_index);
    party_bytes = PartyDataGet();
    character_points = character->skill_points - point_delta;
    if (character_points < 0)
        character_points = 0;
    character->skill_points = character_points;

    level_by_character = party_bytes + character_id + 0x13f;
    if (*level_by_character >= 5)
        return 1;

    points_to_next_level = (PartySkillPoints *)(character_id * 8 + (unsigned int)party_bytes + 0x140);
    remaining_points = *points_to_next_level;
    remaining_points -= point_delta;
    if (remaining_points <= 0) {
        xglSoundEffectNormalID(1, 0);
        level = *level_by_character + 1;
        *level_by_character = level;
        if ((unsigned char)level < 5) {
            next_level_points = SkillNextLvGet(*level_by_character);
            *points_to_next_level = next_level_points;
            if (remaining_points < 0)
                *points_to_next_level = next_level_points - -remaining_points;
        } else {
            *points_to_next_level = 0;
        }
    } else {
        *points_to_next_level = remaining_points;
    }
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/menu_skill", MenuSkillPasMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_skill", MenuSkillInfoMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_skill", MenuSkillStatusMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_skill", MenuSkillMenuMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_skill", MenuSkillExMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_skill", MenuSkillL1R1Main);

extern struct { char pad[0x31]; signed char unk31; char pad32[0xE]; signed char unk40; } MenuWork;

INCLUDE_ASM("asm/main/nonmatchings/menu_skill", MenuSkillSetListMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_skill", MenuSkillCategoryMain);

typedef struct {
    unsigned char pad0;
    unsigned char unk1;
    unsigned char pad2[10];
    short unkC;
    short unkE;
    char *unk10;
    unsigned char unk14;
    unsigned char unk15;
    unsigned char pad16[6];
    void *unk1C;
} MSLWindow;

typedef struct {
    int unk0;
    int unk4;
    unsigned char pad8;
    unsigned char unk9;
    unsigned char padA[2];
} MSLRow;

extern unsigned char *MenuSkillList;

extern unsigned char MenuKeepSelect[];

extern int *MenuSortAddrGet(int);

extern void *MenuListGet(int);

extern void MenuSortSet(int, int, int);

extern void *MenuListMake(int, int);

extern int MenuSortCheck(int);

extern void WindowSPItemChange(void *);

extern void WindowSPSetSelect(void *, unsigned char *);

extern int WindowSPSelect(void *, int);

int SkillSetLvGet(int skill_id);

typedef struct {
    int unk0;
    int unk4;
    signed char unk8;
    signed char unk9;
    unsigned char padA[2];
} MSL0Row;
extern char D_004C9A18[];
extern int *func_A19210(int);
extern unsigned char *func_A1A548(int);
extern int func_A19698(int, unsigned short);
int SkillGetPtGet(int skill_id);
int SkillCharSkillLvGet(int character_id);

static void MenuSkillListChange00(void)
{
    int *sort = MenuSortAddrGet(0);
    MSL0Row *row = MenuListGet(0);
    MSLWindow *win = (MSLWindow *)(MenuSkillList + 0xC);
    int *chr = func_A19210(MenuWork.unk40);
    int i;
    int count;

    MenuSortSet(0, 0x10, MenuWork.unk31 + 0x100);
    MenuListMake(0, 0);
    count = MenuSortCheck(0);
    for (i = 0; i < count; i++) {
        short id = sort[i];
        unsigned char *sk = func_A1A548(id);
        int pt = SkillGetPtGet(id);
        int lv = SkillCharSkillLvGet(MenuWork.unk40);
        int slv;
        row->unk4 = pt;
        slv = SkillSetLvGet(id);
        row->unk8 = 0;
        row->unk9 = slv;
        if (*(unsigned short *)(sk + 0xE) == 0)
            row->unk8 = 1;
        else if (func_A19698(MenuWork.unk40, *(unsigned short *)(sk + 0xE)) != 0)
            row->unk8 = 1;
        if (lv < row->unk9)
            row->unk8 = 1;
        if (chr[5] < pt)
            row->unk8 = 1;
        row++;
    }
    win->unk10 = D_004C9A18;
    win->unk1 = 7;
    win->unk14 = 1;
    win->unk15 = 8;
    win->unkC = 0x114;
    win->unkE = 0xC6;
    win->unk1C = MenuListGet(0);
    WindowSPItemChange(win);
    WindowSPSetSelect(win, &MenuKeepSelect[MenuWork.unk31 * 5]);
    ((unsigned char *)win)[0x26] = 4;
    WindowSPSelect(win, 0);
}

extern char D_004DB640[];
extern int MenuSkillEquipCheck(int, int);
static void MenuSkillListChange01(void)
{
    int *sort = MenuSortAddrGet(0);
    MSLRow *row = MenuListGet(0);
    MSLWindow *win = (MSLWindow *)(MenuSkillList + 0xC);
    int count;
    int i;

    MenuSortSet(0, 0x40, MenuWork.unk40);
    MenuListMake(0, 0);
    count = MenuSortCheck(0);
    for (i = 0; i < count; i++) {
        row[i].unk4 = SkillSetLvGet(sort[i]);
        if (MenuSkillEquipCheck(MenuWork.unk40, sort[i]))
            row[i].unk9 = 1;
        else
            row[i].unk9 = 0;
    }
    win->unk10 = D_004DB640;
    win->unk1 = 3;
    win->unk14 = 1;
    win->unk15 = 8;
    win->unkC = 0x100;
    win->unkE = 0xC6;
    win->unk1C = MenuListGet(0);
    WindowSPItemChange(win);
    WindowSPSetSelect(win, &MenuKeepSelect[0x14]);
    WindowSPSelect(win, 0);
}

INCLUDE_ASM("asm/main/nonmatchings/menu_skill", MenuSkillListMain);

INCLUDE_ASM("asm/main/nonmatchings/menu_skill", MenuSkill);

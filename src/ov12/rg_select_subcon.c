/*
 * OV12 original TU 70: 0x00a3c6a0..0x00a3cd10 (7 functions)
 */
#include "common.h"
#include "rg_select_subcon.h"

extern int s_bIgnoreEventFlag;

extern int XrgEventIsUsablePlayer(unsigned int index);
extern int XrgEventIsUsableEnemy(unsigned int index);
extern int XrgRandIntRange(int lower, int upper);

void RgSelectIgnoreEventFlag(void)
{
    s_bIgnoreEventFlag = 1;
}

int RgSelectGetPlayerChars(void)
{
    unsigned int i;
    int mask;

    mask = 1;
    for (i = 0; i < 6; i++)
    {
        if (s_bIgnoreEventFlag || XrgEventIsUsablePlayer(i))
        {
            mask |= 1 << i;
        }
    }
    return mask;
}

int RgSelectGetEnemyChars(void)
{
    unsigned int i;
    int mask;

    mask = 1;
    for (i = 0; i < 6; i++)
    {
        if (s_bIgnoreEventFlag || XrgEventIsUsableEnemy(i))
        {
            mask |= 1 << i;
        }
    }
    return mask;
}

extern void *InstanceOfRgWeaponDB(void);
extern int XrgEventGetLevel(void);
extern unsigned int RgWeaponDBGetSize(void *db);
extern char *RgWeaponDBGetIndex(void *db, unsigned int index);
extern int RgEquipIsEquipable(void *equip, int type);
extern int XrgEventIsUsableWeapon(void *weapon);

static unsigned int _get_all_weapons(int *list, int characterID,
                                     int weaponType, int mode)
{
    void *db;
    unsigned int size;
    unsigned int i;
    unsigned int count;
    int level;
    int ok;
    int mask;
    char *w;
    int *p;

    i = 0;
    count = 0;
    db = InstanceOfRgWeaponDB();
    level = XrgEventGetLevel();
    size = RgWeaponDBGetSize(db);
    for (; i < size; i++)
    {
        w = RgWeaponDBGetIndex(db, i);
        mask = 1 << characterID;
        if ((*(int *)(w + 0xA8) & mask) && RgEquipIsEquipable(w + 0x70, weaponType))
        {
            ok = 1;
            if (mode == 0)
            {
            }
            else if (mode == 1)
            {
                ok = *(int *)(w + 0x3A0) >= level;
            }
            else if (mode == 2)
            {
                ok = 0;
                if (XrgEventIsUsableWeapon(w) || *(int *)(w + 0x3A4))
                {
                    ok = 1;
                }
            }
            if (ok)
            {
                if (list)
                {
                    list[count] = (int)w;
                }
                count++;
            }
        }
    }
    return count;
}

static unsigned int _get_all_weapons(int *list, int characterID,
                                     int weaponType, int mode);

/*
 * Fills `list` with the weapons matching characterID/weaponType and returns
 * their count; the filter mode forwarded to _get_all_weapons switches with
 * s_bIgnoreEventFlag (0 while events are ignored, 2 otherwise).
 */
unsigned int RgSelectWeapons(int *list, int characterID, int weaponType)
{
    if (s_bIgnoreEventFlag != 0)
    {
        return _get_all_weapons(list, characterID, weaponType, 0);
    }
    return _get_all_weapons(list, characterID, weaponType, 2);
}

int RgSelectGetEnemyChar(void)
{
    int usableEnemies[6];
    int enemy;
    int usableCount;
    int maximumEnemy;
    int result;

    usableCount = 0;
    for (enemy = 0; enemy < 6; enemy++)
    {
        if (s_bIgnoreEventFlag || XrgEventIsUsableEnemy(enemy))
        {
            usableEnemies[usableCount] = enemy;
            usableCount++;
        }
    }

    if (usableCount == 0)
    {
        usableEnemies[0] = 0;
        usableCount = 1;
    }

    enemy = usableEnemies[XrgRandIntRange(0, usableCount)];
    maximumEnemy = 5;
    if (enemy < 0)
    {
        result = 0;
    }
    else if (enemy > maximumEnemy)
    {
        result = maximumEnemy;
    }
    else
    {
        result = enemy;
    }
    return result;
}

INCLUDE_ASM("asm/nonmatchings/ov12/rg_select_subcon", RgSelectGetEnemyWeapons);

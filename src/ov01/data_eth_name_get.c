/*
 * OV01 original TU 11: 0x00a2c5a8..0x00a2c9b8 (13 functions)
 */
#include "common.h"

extern int printf(const char *format, ...);

/*
 * Sibling of the dataItmNameGet/dataWepNameGet/dataBltNameGet/dataAccNameGet
 * family this TU also defines (forward-declared, with this same
 * `const char **` return, in src/ov01/battle_init.c): each entry's first
 * member is the display name string this family returns the address of.
 * ethNameTbl is 0x3B4 bytes for a stride of 0xC, i.e. 79 entries.
 */
typedef struct EthName {
    const char *name;
    unsigned char unmodeled_04[8];
} EthName;

extern EthName ethNameTbl[79];

const char **dataEthNameGet(int ethId)
{
    if (ethId <= 0)
    {
        printf("** dataEthnameGet: err %d\n", ethId);
        return 0;
    }
    return &ethNameTbl[ethId - 1].name;
}

extern EthName itmNameTbl[72];

const char **dataItmNameGet(int index)
{
    if (index <= 0)
    {
        printf("** dataItmNameGet: err %d\n", index);
        return 0;
    }
    return &itmNameTbl[index - 1].name;
}

extern EthName sklNameTbl[67];

const char **dataSklNameGet(int index)
{
    if (index <= 0)
    {
        printf("** dataSklNameGet: err %d\n", index);
        return 0;
    }
    return &sklNameTbl[index - 1].name;
}

extern EthName accNameTbl[180];

const char **dataAccNameGet(int index)
{
    if (index <= 0)
    {
        printf("** dataAccNameGet: err %d\n", index);
        return 0;
    }
    return &accNameTbl[index - 1].name;
}

extern EthName wepNameTbl[114];

const char **dataWepNameGet(int index)
{
    if (index <= 0)
    {
        printf("** dataWepNameGet: err %d\n", index);
        return 0;
    }
    return &wepNameTbl[index - 1].name;
}

extern EthName bltNameTbl[178];

const char **dataBltNameGet(int index)
{
    if (index <= 0)
    {
        printf("** dataBltNameGet: err %d\n", index);
        return 0;
    }
    return &bltNameTbl[index - 1].name;
}

extern EthName nrmNameTbl[44];

const char **dataNrmNameGet(int index)
{
    if (index <= 0)
    {
        printf("** dataNrmNameGet: err %d\n", index);
        return 0;
    }
    return &nrmNameTbl[index - 1].name;
}

/*
 * spcNameTbl (0x27C bytes, 53 entries) is laid out right after nrmNameTbl
 * (0x210 bytes, 44 entries): special item ids continue the normal item id
 * space, so the table index here is (index - 45).
 */
extern EthName spcNameTbl[53];

const char **dataSpcNameGet(int index)
{
    if (index <= 0)
    {
        printf("** dataSpcNameGet: err %d\n", index);
        return 0;
    }
    return &spcNameTbl[index - 45].name;
}

/* enemyNameTbl holds 154 name pointers (0x268 bytes, stride 4); enemy ids start at 33. */
extern const char *enemyNameTbl[154];

const char **dataEnemyNameGet(int index)
{
    if (index < 33)
    {
        printf("** dataEnemyNameGet: err %d\n", index);
        return 0;
    }
    return &enemyNameTbl[index - 33];
}

/* statNameTbl holds one name-table pointer per stat type; each is indexed by `index`. */
extern const char **statNameTbl[];

const char **dataStatNameGet(int statType, int index)
{
    const char **name;

    name = 0;
    if (statType < 9)
    {
        name = statNameTbl[statType] + index;
    }
    return name;
}

extern int MenuScenarioNoGet(void);
extern const char *ziggyName;
extern const char *playerNameTbl[32];

const char **dataPlayerNameGet(int id)
{
    int no;

    if (id >= 33)
    {
        printf("** dataPlayerNameGet: err %d\n", id);
        return 0;
    }
    if (id == 5)
    {
        no = MenuScenarioNoGet();
        if (no >= 108)
        {
            return &ziggyName;
        }
    }
    return &playerNameTbl[id - 1];
}

extern EthName engNameTbl[40];

const char **dataEngNameGet(int index)
{
    if (index <= 0)
    {
        printf("** dataEngNameGet: err %d\n", index);
        return 0;
    }
    return &engNameTbl[index - 1].name;
}

extern EthName frmNameTbl[40];

const char **dataFrmNameGet(int index)
{
    if (index <= 0)
    {
        printf("** dataFrmNameGet: err %d\n", index);
        return 0;
    }
    return &frmNameTbl[index - 1].name;
}

#include "common.h"
#include "main/xgl_cd.h"
#include "menu_1.h"

/* OptMntToPosEquip2 maps the mount code through this asm sibling first. */
int OptMntToPosEquip(int mountCode);

extern unsigned char SaveData[];

unsigned char *MenuSaveDataGet(void)
{
    return SaveData + 0x10254;
}

int MenuScenarioNoGet(void);

/*
 * dataEvtBoxChk is defined in src/main/data.c and xglFlagsGet in
 * src/main/xgl_flags.c; neither is published in include/main/ yet (only
 * xglFlagsInitial is), so both are forward-declared here like
 * MenuScenarioNoGet above and OptMntToPosEquip at the top of this file.
 */
int dataEvtBoxChk(int eventId);
int xglFlagsGet(int bitOffset, int bitCount);

int MenuShionMwsCheck(void)
{
    if (MenuScenarioNoGet() < 0xA) {
        return 1;
    }
    if (xglFlagsGet(0x3FF, 1) != 0) {
        return 1;
    }
    return dataEvtBoxChk(0xA) != 0;
}

int MenuEquipStealMaskCheck(void)
{
    return xglFlagsGet(0x3F9, 1) != 0;
}

typedef struct {
    int no;
    int start;
    int end;
} ScenarioFlagRange;

int MenuScenarioNoGet(void)
{
    ScenarioFlagRange tbl[5] = {
        { 500, 500, 500 },
        { 400, 400, 399 },
        { 399, 399, 300 },
        { 197, 197, 100 },
        { 56, 56, 1 },
    };
    int i;
    int flag;
    int no;

    for (i = 0; i < 5; i++) {
        flag = tbl[i].start;
        no = tbl[i].no;
        for (; flag >= tbl[i].end; flag--, no--) {
            if (xglFlagsGet(flag, 1) != 0) {
                return no;
            }
        }
    }
    return no;
}

int MenuCursorKeepCheck(void)
{
    return SaveData[0x44] == 0;
}

extern int map_ex_text;

char *MenuSegmentInfoTextGet(int n)
{
    char *p = (char *)map_ex_text;
    while (n >= 2) {
        while (*p++);
        n--;
    }
    return p;
}

char *MenuSegmentMapNameGet(int n)
{
    char *p = (char *)map_ex_text + 0x540;
    while (n >= 2) {
        while (*p++);
        n--;
    }
    return p;
}

char *MenuSegmentItemNameGet(int n)
{
    char *p = (char *)map_ex_text + 0x3C0;
    while (n >= 2) {
        while (*p++);
        n--;
    }
    return p;
}

extern char f_name_0_0036D6C8[];
extern int map_ex_text;

/*
 * MenuLoadFile is defined in src/main/window_tex_load.c; not yet published
 * in its own header, so forward-declared here like MenuScenarioNoGet above
 * (and again below for MenuSaveMapTextLoad, which needs it after this
 * point in the file).
 */
extern int MenuLoadFile(const char *name, void *buffer);

int MenuMapExTextLoad(int work, int mode)
{
    int aligned = (work + 0xF) & ~0xF;
    int next = aligned + 0x2000;

    map_ex_text = aligned;
    if (mode == 0) {
        xglCdReadFile(f_name_0_0036D6C8, (void *)aligned, 0, 1);
    } else {
        MenuLoadFile(f_name_0_0036D6C8, (void *)aligned);
    }
    return next;
}

typedef struct { char pad[0x42]; short id; char pad2[2]; } SaveMapText;
extern int save_map_text;

SaveMapText *MenuSaveMapNameGet(int id)
{
  SaveMapText *p = (SaveMapText *) save_map_text;
  SaveMapText **new_var;
  SaveMapText *top = p;
  int i;
  for (i = 0; i < 37; i++, p++)
  {
    if (p->id == id)
    {
      return *(new_var = &p);
    }
  }

  return top;
}

/*
 * MenuLoadFile is defined in src/main/window_tex_load.c; not yet published
 * in its own header, so forward-declared here like MenuScenarioNoGet above.
 */
extern int MenuLoadFile(const char *name, void *buffer);

extern char f_name_1[];
extern int save_map_text;

int MenuSaveMapTextLoad(int work, int mode)
{
    int aligned = (work + 0xF) & ~0xF;
    int next = aligned + 0x1000;

    save_map_text = aligned;
    if (mode == 0) {
        xglCdReadFile(f_name_1, (void *)aligned, 0, 1);
    } else {
        MenuLoadFile(f_name_1, (void *)aligned);
    }
    return next;
}

int PartyAllPartyGet(int *out, int mode);
short *func_00A11108(int id, void *a, void *b);

int MenuCharHpCheck(int id)
{
    int a[4];
    int b[4];
    int party[16];
    short *st;
    int i;
    int n;
    int ret = 0;

    if (id == 0) {
        return 0;
    }
    {
        if (id < 0) {
            n = PartyAllPartyGet(party, 0);
            for (i = 0; i < n; i++) {
                st = func_00A11108(party[i], a, b);
                if (st[0x34 / 2] - st[0]) {
                    break;
                }
            }
            if (i != n) {
                ret = 0;
            } else {
                return 1;
            }
        } else {
            ret = 1;
            func_A191C0(id);
            st = func_00A11108(id, a, b);
            if (st[0x34 / 2] == st[0]) {
                return 1;
            }
            ret = 0;
        }
    }
    return ret;
}

int MenuCharEpCheck(int id)
{
    int a[4];
    int b[4];
    int party[16];
    short *st;
    int i;
    int n;
    int ret = 0;

    if (id == 0) {
        return 0;
    }
    {
        if (id < 0) {
            n = PartyAllPartyGet(party, 0);
            for (i = 0; i < n; i++) {
                st = func_00A11108(party[i], a, b);
                if (st[0x36 / 2] - st[1]) {
                    break;
                }
            }
            if (i != n) {
                ret = 0;
            } else {
                return 1;
            }
        } else {
            ret = 1;
            st = func_00A11108(id, a, b);
            if (st[0x36 / 2] == st[1]) {
                return 1;
            }
            ret = 0;
        }
    }
    return ret;
}

int MenuMaryIdChange(int characterId)
{
    if (characterId == 20) {
        return 11;
    }
    if (characterId == 19) {
        return 12;
    }
    if (characterId < 17) {
        return characterId;
    }
    return 3;
}

int MenuMainCharCheck(int charId)
{
    return (unsigned int)(charId - 1) < 7U;
}

int MenuMainAgwsCheck(int charId)
{
    unsigned int isMainAgws;

    isMainAgws = (unsigned int)(charId - 0x13) < 2U;
    if (isMainAgws) {
        return 0;
    }
    return 1;
}

extern char *strcat(char *destination, const char *source);
extern long long __divdi3(long long, long long);
extern long long __moddi3(long long, long long);
extern char msg_2_00532A78[];
extern unsigned char D_004DAB88[];
extern char D_004C5180[];

char *MenuNumberTextGet(long long value, int width, int mode)
{
    char *tbl[10];
    long long p;
    long long rem;
    int n;
    int i;
    int w;

    __builtin_memcpy(tbl, D_004C5180, 40);
    msg_2_00532A78[0] = D_004DAB88[0];
    if (width < 0) {
        p = 10;
        w = 1;
        while (__divdi3(value, p) != 0) {
            p *= 10;
            w++;
        }
        width = w;
    }
    mode = (mode == 1);
    if (width < 11) {
        n = width;
    } else {
        n = 10;
    }
    rem = value;
    while (n > 0) {
        p = 10;
        for (i = 1; i < n; i++) {
            p *= 10;
        }
        if (mode || __divdi3(rem, p) != 0) {
            mode = 1;
            strcat(msg_2_00532A78, tbl[(int)__divdi3(rem, p)]);
        }
        n--;
        rem = __moddi3(rem, p);
    }
    strcat(msg_2_00532A78, tbl[(int)__moddi3(rem, 10)]);
    return msg_2_00532A78;
}

extern char *tag_name_3[];

char *MenuTagTextGet(int index)
{
    return tag_name_3[index];
}

typedef struct { unsigned short v[12]; } FaceEpidTbl12;
typedef struct { unsigned short v[16]; } FaceEpidTbl16;
extern FaceEpidTbl12 D_004C51E8;
extern FaceEpidTbl12 D_004C5200;
extern FaceEpidTbl16 D_004C5218;
extern FaceEpidTbl12 D_004C5238;

int MenuFaceEpidGet(int id, int kind)
{
    FaceEpidTbl12 t0;
    FaceEpidTbl12 t1;
    FaceEpidTbl16 t2;
    FaceEpidTbl12 t3;
    FaceEpidTbl12 *p;

    if (id == 2) {
        return (kind == 0) ? 0xC01 : 0x401;
    }
    if (kind == 0) {
        if ((unsigned)(id - 1) < 12) {
            t0 = D_004C51E8;
            return t0.v[id - 1];
        }
        if ((unsigned)(id - 0x11) < 12) {
            p = &t1;
            *p = D_004C5200;
        } else {
            goto zero;
        }
    } else {
        p = &t3;
        if ((unsigned)(id - 1) < 12) {
            t2 = D_004C5218;
            return t2.v[id - 1];
        }
        *p = D_004C5238;
    }
    return p->v[id - 0x11];
zero:
    return 0;
}

extern char *para_name_4[];

char *MenuParaNameGet(int index)
{
    return para_name_4[index];
}

extern char *para_name2_5[];

char *MenuParaNameGet2(int index)
{
    return para_name2_5[index];
}

extern char *char_name_6[];
extern char *char_name2_7[];

char *MenuCharNameGet(int no)
{
    char *name = char_name_6[no];

    if (no == 5 && MenuScenarioNoGet() < 0x6C) {
        name = char_name2_7[0];
    }
    return name;
}

typedef struct {
    const char *name;
    const unsigned char *auxiliary;
    const unsigned char *sortName;
} UpTextEntry;
extern MenuTextEntry dumm_msg_8;
extern MenuTextEntry dumm_9;
extern UpTextEntry dumm2_10;
extern MenuTextEntry *func_A2C5F8(int);
extern MenuTextEntry *func_A2C648(int);
extern MenuTextEntry *func_A2C698(int);
extern MenuTextEntry *func_A2C6E8(int);
extern MenuTextEntry *func_A2C738(int);
extern MenuTextEntry *func_A2C7D8(int);
extern MenuTextEntry *func_A2C918(int);
extern MenuTextEntry *func_A2C968(int);
extern MenuTextEntry *func_A2C5A8(int);
extern char *dataEvtItmNameGet(int index);
extern int MenuEquipStealMaskCheck(void);
extern int MenuShionMwsCheck(void);

MenuTextEntry *MenuTextGet(int index)
{
    unsigned int encodedIndex = (unsigned int)index;
    unsigned int itemId = encodedIndex & 0xFFFF;
    unsigned int highIndex = encodedIndex >> 16;
    MenuTextEntry *result = 0;

    if (itemId == 0) {
        return &dumm_msg_8;
    } else {
        switch (highIndex - 1) {
        case 0:
            result = func_A2C5F8(itemId);
            break;
        case 1: {
            const char *itemName = dataEvtItmNameGet(itemId);
            const unsigned char *end = (const unsigned char *)itemName;

            dumm2_10.name = itemName;
            dumm2_10.sortName = (const unsigned char *)itemName;
            while (*end++ != '\0') {
            }
            dumm2_10.auxiliary = end;
            result = (MenuTextEntry *)&dumm2_10;
            break;
        }
        case 2:
            result = &dumm_msg_8;
            if (MenuEquipStealMaskCheck() == 0) {
                if (itemId == 0x45) {
                    if (MenuShionMwsCheck() != 0) {
                        result = func_A2C6E8(0x45);
                    }
                } else {
                    result = func_A2C6E8(itemId);
                }
            }
            break;
        case 3:
            if (MenuEquipStealMaskCheck() != 0) {
                result = &dumm_msg_8;
            } else {
                result = func_A2C738(itemId);
            }
            break;
        case 4:
            if (MenuEquipStealMaskCheck() == 0) {
                result = func_A2C698(itemId);
            } else {
                result = &dumm_msg_8;
            }
            break;
        case 5:
        case 6:
            dumm_9.name = MenuCharNameGet(itemId);
            result = &dumm_9;
            break;
        case 7:
            result = func_A2C648(itemId);
            break;
        case 8:
            result = func_A2C5A8(itemId);
            break;
        case 9:
            result = func_A2C7D8(itemId);
            break;
        case 10:
            result = func_A2C968(itemId);
            break;
        case 11:
            result = func_A2C918(itemId);
            break;
        default:
            break;
        }
    }
    return result;
}

int dataItmBoxChk(int id);
int dataWpnBoxChk(int weapon_id);
int dataBltBoxChk(int bullet_id);
int dataAccBoxChk(int accessory_id);

int MenuBoxChk(int index)
{
    int ret = 0;
    int id = index & 0xFFFF;

    switch ((unsigned int)index >> 16) {
    case 1:
        ret = dataItmBoxChk(id);
        break;
    case 2:
        ret = dataEvtBoxChk(id);
        break;
    case 3:
        ret = dataWpnBoxChk(id);
        break;
    case 4:
        ret = dataBltBoxChk(id);
        break;
    case 5:
        ret = dataAccBoxChk(id);
        break;
    case 6:
        ret = 0;
        break;
    }
    return ret;
}

void dataItmBoxInc(int id);
void dataEvtBoxInc(int id);
void dataWpnBoxInc(int id);
void dataBltBoxInc(int id);
void dataAccBoxInc(int id);
void PartyTakeAgwsOn(int id);

int MenuBoxInc(int index)
{
    int ret = 0;
    int id = index & 0xFFFF;

    switch ((unsigned int)index >> 16) {
    case 1:
        dataItmBoxInc(id);
        break;
    case 2:
        dataEvtBoxInc(id);
        break;
    case 3:
        dataWpnBoxInc(id);
        break;
    case 4:
        dataBltBoxInc(id);
        break;
    case 5:
        dataAccBoxInc(id);
        break;
    case 6:
        ret = -1;
        break;
    case 7:
        PartyTakeAgwsOn(id);
        break;
    }
    return ret;
}

void dataItmBoxDec(int id);
void dataEvtBoxDec(int id);
void dataWpnBoxDec(int id);
void dataBltBoxDec(int id);
void dataAccBoxDec(int id);

int MenuBoxDec(int index)
{
    int ret = 0;
    int id = index & 0xFFFF;

    switch ((unsigned int)index >> 16) {
    case 1:
        dataItmBoxDec(id);
        break;
    case 2:
        dataEvtBoxDec(id);
        break;
    case 3:
        dataWpnBoxDec(id);
        break;
    case 4:
        dataBltBoxDec(id);
        break;
    case 5:
        dataAccBoxDec(id);
        break;
    case 6:
        ret = -1;
        break;
    }
    return ret;
}

/*
 * dataWpnGet's own record (ov01 VA 0x00a1a3d8). MenuBulletCheck above models
 * its +0x6 flags halfword as WeaponStatus, already accepted and unable to
 * grow, so this call site keeps its own scaffold label func_A1A3D8 (matching
 * src/main/menu_shop.c) for the different +0x12 halfword summed here.
 */
typedef struct {
    unsigned char unmodeled_00[0x8];
    unsigned short characterEquipMask;
    unsigned char unmodeled_a[0x2];
    unsigned short categoryMask;
    unsigned char mountMask;
    unsigned char unmodeled_f[0x2];
    unsigned char equipWeight;
    short wagl; /* +0x12 */
} WeaponWaglData;

extern WeaponWaglData *func_A1A3D8(int weaponId);

/* Types ported from the derwetterfrosch fork (2026-10-19). */
struct MenuCharParameters {
    short maxHp;
    short maxEp;
    unsigned char unmodeled_04[0x34 - 0x04];
    short hp;
    short ep;
};

typedef struct MenuNumberDigits {
    const char *text[10];
} MenuNumberDigits;

struct MenuScenarioRange {
    int scenario;
    int flag;
    int minimumFlag;
};

struct MenuScenarioRangeSet {
    struct MenuScenarioRange ranges[5];
};

struct MenuScenarioTable {
    struct MenuScenarioRangeSet rangeSet;
    unsigned char unmodeled_3c[4];
};

typedef struct { unsigned short low; unsigned short high; } MbmS32Parts;
typedef union { MbmS32Parts parts; int value __attribute__((packed)); } MbmS32;
typedef struct { MbmS32 price; } MbmItemPrice;
typedef struct { unsigned char unmodeled_00[2]; MbmS32 price; } MbmWeaponPrice;
typedef struct { unsigned char unmodeled_00[6]; MbmS32 price; unsigned char unmodeled_0a[2]; short weaponId; } MbmBulletPrice;
extern void *func_A1A4E8(int itemId);
extern MbmBulletPrice *func_A1A428(int bulletId);
extern void *func_A1A598(int frameId);
extern void *func_A1A5E8(int engineId);
extern const int D_004C5388[3];

int MenuBoxMoneyGet(int index, int priceMode)
{
    float rate[5];
    int priceValues[3];
    int agwsPrices[12];
    int itemId = index & 0xffff;
    int price = 0;
    int category = (unsigned int)index >> 16;
    int rateIndex;
    short weaponId;

    if (itemId == 0) {
        return 0;
    }
    if (priceMode == 0) {
        const unsigned char *discountLevel = &SaveData[0x102dc];
        for (rateIndex = 0; rateIndex < 5; rateIndex++) {
            rate[rateIndex] = 1.0f - discountLevel[rateIndex] * 0.1f;
            if (rate[rateIndex] < 0.5f) {
                rate[rateIndex] = 0.5f;
            }
        }
    } else {
        for (rateIndex = 4; rateIndex >= 0; rateIndex--) {
            rate[rateIndex] = 1.0f;
        }
    }
    if ((unsigned int)(category - 1) < 12U) {
    switch (category) {
    case 1:
        {
            int rawPrice = ((MbmItemPrice *)func_A1A4E8(itemId))->price.value;
            price = (int)(rawPrice * rate[0]);
        }
        break;
    case 2:
        if ((unsigned int)(itemId - 53) < 3U) {
            __builtin_memcpy(priceValues, D_004C5388, sizeof(priceValues));
            price = priceValues[itemId - 53];
        }
        break;
    case 3:
        price = ((MbmWeaponPrice *)func_A1A3D8(itemId))->price.value;
        if (func_A1A3D8(itemId)->categoryMask & 0x4000) {
            price = (int)(price * rate[4]);
        } else {
            price = (int)(price * rate[2]);
        }
        break;
    case 4:
        price = ((MbmBulletPrice *)func_A1A428(itemId))->price.value;
        weaponId = func_A1A428(itemId)->weaponId;
        if (weaponId != 0) {
            if (func_A1A3D8(weaponId)->categoryMask & 0x4000) {
                price = (int)(price * rate[4]);
            } else {
                price = (int)(price * rate[2]);
            }
        }
        break;
    case 5:
        price = ((MbmItemPrice *)func_A1A548(itemId))->price.value;
        if (itemId < 129) {
            price = (int)(price * rate[4]);
        } else {
            price = (int)(price * rate[3]);
        }
        break;
    case 11:
        price = ((MbmItemPrice *)func_A1A598(itemId))->price.value;
        break;
    case 12:
        price = ((MbmItemPrice *)func_A1A5E8(itemId))->price.value;
        break;
    case 7:
        memset(agwsPrices, 0, sizeof(agwsPrices));
        agwsPrices[5] = 50000;
        agwsPrices[9] = 80000;
        agwsPrices[10] = 300000;
        price = (int)(agwsPrices[itemId - 17] * rate[1]);
        break;
    }
    }
    if (priceMode == 1) {
        price = (int)(price * 0.5f);
    }
    return price;
}

int MenuSkillEquipCheck(short chrNo, short skillId)
{
    short *p = (short *)((char *)func_A191C0(chrNo) + 0xA6);
    int i;
    int id = skillId;

    for (i = 0; i < 3; i++) {
        int v = *p++;
        if (v == id)
            return i + 1;
    }
    return 0;
}

void MenuSkillEquip(short chrNo, short skillId, int slot)
{
    char *b = (char *)func_A191C0(chrNo);
    short *p;
    int i;

    if (slot >= 0) {
        *(short *)(b + slot * 2 + 0xA6) = skillId;
        return;
    }
    for (i = 0; i < 3; i++) {
        p = (short *)((char *)(i * 2) + (int)b + 0xA6);
        if (*p == 0) {
            *p = skillId;
            break;
        }
    }
}

int MenuRWeaponCheck(int id, int chr)
{
    if (chr < 0x24) {
        if (chr >= 0x20) {
            switch (id) {
            case 0x5B: id = 0x70; break;
            case 0x5C: id = 0x71; break;
            case 0x5E: id = 0x72; break;
            }
        }
    }
    return id;
}

int MenuRWeaponCheck2(int id)
{
    switch (id) {
    case 0x70: id = 0x5B; break;
    case 0x71: id = 0x5C; break;
    case 0x72: id = 0x5E; break;
    }
    return id;
}

extern unsigned char opt_mnt_tbl[8][2];

unsigned char PosEquipToOptMnt(int posEquip)
{
    int bit;

    for (bit = 0; bit < 8; bit++) {
        if (((posEquip & 0xFF) >> bit) & 1) {
            return opt_mnt_tbl[bit][0];
        }
    }
    return 0;
}

int OptMntToPosEquip(int mountCode)
{
    int code = mountCode & 0xFF;
    int i = 0;

    if (((unsigned short *)opt_mnt_tbl)[i] == code) {
        return 1 << i;
    }
    goto next;
    for (;;) {
        if (((unsigned short *)opt_mnt_tbl)[i] == code) {
            break;
        }
next:
        i++;
        if (i >= 8) {
            goto none;
        }
    }
    return (1 << i) & 0xFF;
none:
    return 0;
}

int OptMntToPosEquip2(int mountCode)
{
    int posEquip = OptMntToPosEquip(mountCode & 0xFF);

    switch (posEquip) {
    case 1:
        posEquip = 3;
        break;
    case 2:
        posEquip = 3;
        break;
    case 16:
    case 32:
        posEquip = 0x30;
        break;
    }

    return posEquip;
}

int OptMntToWpnPos(int mountCode)
{
    int weaponPos = 0;

    switch (mountCode) {
    case 32:
    case 33:
        weaponPos = 1;
        break;
    case 24:
    case 25:
        weaponPos = 2;
        break;
    case 34:
        weaponPos = 3;
        break;
    case 26:
        weaponPos = 4;
        break;
    case 35:
        weaponPos = 5;
        break;
    case 27:
        weaponPos = 6;
        break;
    }
    return weaponPos;
}

int WpnPosToOptMnt(int arg0, int arg1)
{
    char *var_6;
    int res;

    var_6 = 0;
    res = 0;
    if (arg1 != 0) {
        var_6 = (char *)func_A1A3D8(arg1);
    }
    switch (arg0) {
    case 1:
        if (var_6 != 0) {
            res = (*(unsigned char *)(var_6 + 0xE) & 1) ? 0x20 : 0x21;
        }
        break;
    case 2:
        if (var_6 != 0) {
            res = (*(unsigned char *)(var_6 + 0xE) & 0x10) ? 0x18 : 0x19;
        }
        break;
    case 3:
        res = 0x22;
        break;
    case 4:
        res = 0x1A;
        break;
    case 5:
        res = 0x23;
        break;
    case 6:
        res = 0x1B;
        break;
    }
    return res;
}

int MenuWpnMountIdChange(int arg0, int arg1)
{
    int tbl[8] = { 0x20, 0x18, 0x22, 0x1A, 0x23, 0x1B, 0x21, 0x19 };

    if (arg1 < 2) {
        return tbl[arg1 + 5];
    }
    return tbl[arg1 - 1];
}

int PosEquipToMntPos(void)
{
    return 0;
}

static unsigned short chrEquipGet(int charId)
{
    if (charId < 0x11) {
        return 0x8000 >> (charId - 1);
    }
    return 0x8000 >> (charId - 0x11);
}

static int wpnInfoGet(int weaponType)
{
    return (weaponType < 0x11) ? 0x4000 : 0x2000;
}

int MenuWeaponEquipCheck(int chr, int weaponId)
{
    unsigned short eq;
    int info;
    char *w;

    if (chr == 0)
        return 0;
    eq = chrEquipGet(chr);
    info = wpnInfoGet(chr);
    if (weaponId == 0)
        return 0;
    w = (char *)func_A1A3D8(weaponId);
    if ((*(short *)(w + 0xC) & info) == 0)
        return 0;
    return (*(unsigned short *)(w + 8) & eq) != 0;
}

/*
 * dataUnitOrgGet's own 0x180-byte per-character record (ov01 VA 0x00a191c0,
 * cross-overlay call under the scaffold label func_A191C0, matching
 * src/main/window_tex_load.c and src/main/menu_para_pt_rate_get.c; not in
 * this image). The three equipped-weapon id halfwords at +0x5E immediately
 * precede the accessory id halfwords src/main/menu_para_pt_rate_get.c models
 * at +0x64 as CharParaData's accessory[3]; only that weapon array is read
 * here.
 */
typedef struct {
    unsigned char unmodeled_00[0x5E];
    short weapon[3]; /* +0x5E */
} UnitOrgWeapons;

extern UnitOrgWeapons *func_A191C0(int chrNo);

typedef struct {
    unsigned char unmodeled_000[0x8 - 0x0];
    unsigned short characterEquipMask; /* +0x8 */
    unsigned char unmodeled_00a[0xc - 0xa];
    unsigned short categoryMask; /* +0xc */
    unsigned char mountMask; /* +0xe */
    unsigned char unmodeled_00f[0x11 - 0xf];
    unsigned char equipWeight; /* +0x11 */
    short wagl; /* +0x12 */
} WeaponWaglDataWP;
typedef struct {
    unsigned char unmodeled_00[0x54];
    short agwsId; /* +0x54 */
    unsigned char unmodeled_056[0x5a - 0x56];
    signed char weaponMount[3]; /* +0x5a */
    unsigned char unmodeled_05d[0x5e - 0x5d];
    short weapon[3]; /* +0x5E */
    short accessoryId[3]; /* +0x64 */
    unsigned char unmodeled_06a[0x8e - 0x6a];
    short etherId[12]; /* +0x8e */
    short skillId[3]; /* +0xa6 */
} UnitOrgWeaponsWP;

int MenuWeaponEquipPosCheck(int charId, int weaponId, int weaponPosition, int replacementSlot)
{
    UnitOrgWeaponsWP *character;
    WeaponWaglDataWP *weapon;
    unsigned char occupiedPositions = 0;
    unsigned short blockedPositions = 0;
    int weight;
    int slot;
    int equipmentIndex;
    int requestedPosition;
    int supportedPositions;
    int result = 0;
    int capacity;
    int mountCode;
    int bitIndex;
    int bit;
    unsigned char alternatives;
    unsigned char mountMask;

    character = (UnitOrgWeaponsWP *)func_A191C0(charId);
    if ((unsigned int)(weaponId - 1) >= 115U) {
        return 0;
    }
    weapon = (WeaponWaglDataWP *)func_A1A3D8(weaponId);
    supportedPositions = weapon->mountMask;
    if (!(weapon->characterEquipMask & chrEquipGet(charId))) {
        return 0;
    }
    capacity = 3;
    if (charId == 0x1b) {
        capacity = 2;
    }
    weight = 0;
    for (equipmentIndex = 0; equipmentIndex < 3; equipmentIndex++) {
        if (character->weapon[equipmentIndex] != 0 && equipmentIndex != replacementSlot) {
            weight += ((WeaponWaglDataWP *)func_A1A3D8(character->weapon[equipmentIndex]))->equipWeight;
        }
    }
    if (capacity < weight + ((WeaponWaglDataWP *)func_A1A3D8(weaponId))->equipWeight) {
        return 0;
    }
    requestedPosition = OptMntToPosEquip(WpnPosToOptMnt(weaponPosition, weaponId) & 0xff);
    OptMntToPosEquip2(WpnPosToOptMnt(weaponPosition, weaponId) & 0xff);
    for (equipmentIndex = 0; equipmentIndex < 3; equipmentIndex++) {
        if (equipmentIndex != replacementSlot) {
            occupiedPositions = (occupiedPositions | OptMntToPosEquip(character->weaponMount[equipmentIndex] & 0xff)) & 0xff;
        }
    }
    for (equipmentIndex = 0; equipmentIndex < 8; equipmentIndex++) {
        if ((occupiedPositions >> equipmentIndex) & 1) {
            mountCode = PosEquipToOptMnt((1 << equipmentIndex) & 0xff);
            for (slot = 0; slot < 3; slot++) {
                if (character->weaponMount[slot] == mountCode && character->weapon[slot] != 0) {
                    int existingBit;

                    weapon = (WeaponWaglDataWP *)func_A1A3D8(character->weapon[slot]);
                    mountMask = weapon->mountMask;
                    for (bitIndex = 0; bitIndex < 2; bitIndex++) {
                        unsigned short alternatives;
                        existingBit = 0x80 >> bitIndex;
                        alternatives = mountMask & ~existingBit & 0xf0;
                        if (weapon->mountMask & existingBit) {
                            if (alternatives & 0x10) {
                                alternatives |= 0x20;
                            }
                            if (alternatives & 0x20) {
                                alternatives |= 0x10;
                            }
                            blockedPositions |= alternatives;
                        }
                    }
                    for (bitIndex = 0; bitIndex < 2; bitIndex++) {
                        unsigned char lowMountMask = weapon->mountMask;
                        existingBit = 8 >> bitIndex;
                        alternatives = mountMask & ~existingBit & 0x0f;
                        if (lowMountMask & existingBit) {
                            if (alternatives & 1) {
                                alternatives |= 2;
                            }
                            if (alternatives & 2) {
                                alternatives |= 1;
                            }
                            blockedPositions |= alternatives;
                        }
                    }
                }
            }
            for (slot = 0; slot < 3; slot++) {
                if (character->weapon[slot] == 0x4f || character->weapon[slot] == 0x54) {
                    blockedPositions |= 0x30;
                }
            }
        }
    }
    weapon = (WeaponWaglDataWP *)func_A1A3D8(weaponId);
    mountMask = weapon->mountMask;
    for (bitIndex = 0; bitIndex < 2; bitIndex++) {
        unsigned short alternatives;
        bit = 0x80 >> bitIndex;
        alternatives = mountMask & ~bit & 0xf0;
        if (weapon->mountMask & bit) {
            if (alternatives & 0x10) {
                alternatives |= 0x20;
            }
            if (alternatives & 0x20) {
                alternatives |= 0x10;
            }
            if (occupiedPositions & alternatives) {
                blockedPositions = (blockedPositions | bit) & 0xff;
            }
        }
    }
    for (bitIndex = 0; bitIndex < 2; bitIndex++) {
        unsigned char lowMountMask = weapon->mountMask;
        bit = 8 >> bitIndex;
        alternatives = mountMask & ~bit & 0x0f;
        if (lowMountMask & bit) {
            if (alternatives & 1) {
                alternatives |= 2;
            }
            if (alternatives & 2) {
                alternatives |= 1;
            }
            if (occupiedPositions & alternatives) {
                blockedPositions = (blockedPositions | bit) & 0xff;
            }
        }
    }
    if ((weaponId == 0x4f || weaponId == 0x54) && (occupiedPositions & 0x30)) {
        blockedPositions |= 3;
    }
    if ((requestedPosition & supportedPositions) && (requestedPosition & ~blockedPositions)) {
        result = WpnPosToOptMnt(weaponPosition, weaponId) & 0xff;
    }
    return result;
}

typedef struct {
    unsigned char unmodeled_00[0x64];
    short accessoryId[3]; /* +0x64 */
} UpAccWeaponOrg;
typedef struct {
    unsigned char unmodeled_00[6];
    unsigned short equipMask; /* +0x6 */
    unsigned char unmodeled_08[2];
    unsigned short category; /* +0xA */
} UpAccEquipData;

int MenuAccessoryEquipCheck(int charId, int accessoryId, int slotIndex)
{
    unsigned short characterMask;
    UpAccWeaponOrg *characterWeapons;
    UpAccEquipData *accessory;
    unsigned short categoryToScan;
    int index;

    if (charId == 0) {
        return 0;
    }

    characterMask = chrEquipGet(charId);
    characterWeapons = (UpAccWeaponOrg *)func_A191C0(charId);

    if (accessoryId == 0) {
        return 0;
    }

    accessory = (UpAccEquipData *)func_A1A548(accessoryId);
    if ((accessory->equipMask & characterMask) == 0) {
        return 0;
    }

    if (slotIndex < 0) {
        return 1;
    }

    if (characterWeapons->accessoryId[slotIndex] != 0) {
        UpAccEquipData *equipped =
            (UpAccEquipData *)func_A1A548(characterWeapons->accessoryId[slotIndex]);

        if (equipped->category == accessory->category) {
            if (equipped->category != 2) {
                return 1;
            }
            if (characterWeapons->accessoryId[slotIndex] == accessoryId) {
                return 1;
            }
        }
    }

    categoryToScan =
        charId < 0x11 ? accessory->category : 2;

    if (categoryToScan == 2) {
        for (index = 0; index < 3; index++) {
            if (characterWeapons->accessoryId[index] == accessoryId) {
                return 0;
            }
        }
    } else if (accessory->category != 2) {
        for (index = 0; index < 3; index++) {
            short equippedId = characterWeapons->accessoryId[index];

            if (equippedId != 0) {
                UpAccEquipData *equipped = (UpAccEquipData *)func_A1A548(equippedId);

                if (equipped->category == accessory->category) {
                    return 0;
                }
            }
        }
    }

    return 1;
}

typedef struct WeaponStatus WeaponStatus;

/*
 * dataWpnGet (ov01/data_unit_org_get.c, still asm) returns weapon weaponId's
 * status record (it rejects ids <= 0); MenuBulletCheck only reads the flag
 * halfword at +0x6, whose bit 0x100 marks a weapon that needs bullets.
 */
struct WeaponStatus {
    unsigned char unmodeled_00[6];
    unsigned short flags; /* +0x6: bit 0x100 - weapon needs bullets */
};

extern WeaponStatus *dataWpnGet(int weaponId);

typedef struct BulletData BulletData;

/*
 * dataAttGet (same TU, still asm) indexes the bullet/attachment table by
 * ammoId; only the owning weapon id short at +0xC is read here.
 */
struct BulletData {
    unsigned char unmodeled_00[0xC];
    short weaponId; /* +0xC: the weapon this ammunition belongs to */
};

extern BulletData *dataAttGet(int ammoId);

int MenuBulletCheck(int weaponId, int ammoId)
{
    if (weaponId == 0) {
        return 0;
    }
    if ((dataWpnGet(weaponId)->flags & 0x100) == 0) {
        return 0;
    }
    if (ammoId < 0) {
        return 1;
    }
    if (ammoId == 0) {
        return 0;
    }
    return (dataAttGet(ammoId)->weaponId ^ weaponId) == 0 ? ammoId : 0;
}

extern int func_A197E8(int, int);

int MenuBulletCheck2(int weaponId, int kind)
{
    switch (kind) {
    case 1:
    case 6:
        return 0;
    case 3:
        return func_A197E8(3, 0x3E) != 0;
    default:
        return MenuBulletCheck(weaponId, -1);
    }
}

typedef struct PartEquipData PartEquipData;

/*
 * dataFrmGet and dataEngGet (ov01/data_unit_org_get.c, still asm) return
 * per-entry AGWS part records that share this layout as far as
 * MenuFrameEquipCheck/MenuEngineEquipCheck read it: a halfword at +0x6
 * bitmask of the characters that can equip the part, tested against
 * chrEquipGet's per-character bit.
 */
struct PartEquipData {
    unsigned char unmodeled_00[6];
    unsigned short equipMask; /* +0x6 */
};

extern PartEquipData *dataFrmGet(int frameId);
extern PartEquipData *dataEngGet(int engineId);

int MenuFrameEquipCheck(int charId, int frameId)
{
    if (frameId == 0) {
        return 0;
    }
    return (dataFrmGet(frameId)->equipMask & chrEquipGet(charId)) != 0;
}

int MenuEngineEquipCheck(int charId, int engineId)
{
    if (engineId == 0) {
        return 0;
    }
    return (dataEngGet(engineId)->equipMask & chrEquipGet(charId)) != 0;
}

int MenuAgwsPilotCheck(int id)
{
    int i;
    for (i = 1; i < 13; i++) {
        if (*(short *)((char *)func_A191C0(i) + 0x54) == id)
            return i;
    }
    return 0;
}

int MenuAgwsWaglGet(int chrNo)
{
    UnitOrgWeapons *org;
    short weaponId;
    int result;
    int total;
    int slot;

    result = 0;
    total = 0;
    if (chrNo != 0) {
        org = func_A191C0(chrNo);
        for (slot = 0; slot < 3; slot++) {
            weaponId = org->weapon[slot];
            if (weaponId != 0) {
                total += func_A1A3D8(weaponId)->wagl;
            }
        }
        result = total;
    }
    return result;
}

static void MenuSortSubType05(int *first, int *second)
{
    const unsigned char *firstName = MenuTextGet(*first)->sortName;
    const unsigned char *secondName = MenuTextGet(*second)->sortName;
    unsigned char firstChar;
    unsigned char secondChar;

    for (;;) {
        secondChar = *secondName++;
        firstChar = *firstName++;
        if (firstChar < secondChar) {
            int savedIndex = *first;
            *first = *second;
            *second = savedIndex;
            return;
        }
        if (firstChar != secondChar || firstChar == 0) {
            return;
        }
    }
}

static void MenuSortSubType04(int *first, int *second)
{
    const unsigned char *firstName = MenuTextGet(*first)->sortName;
    const unsigned char *secondName = MenuTextGet(*second)->sortName;
    unsigned char firstChar;
    unsigned char secondChar;

    for (;;) {
        secondChar = *secondName++;
        firstChar = *firstName++;
        if (secondChar < firstChar) {
            int savedIndex = *first;
            *first = *second;
            *second = savedIndex;
            return;
        }
        if (firstChar != secondChar || firstChar == 0) {
            return;
        }
    }
}

static void MenuSortSubType03(int *first, int *second)
{
    int firstCount = MenuBoxChk(*first);

    if (MenuBoxChk(*second) < firstCount) {
        int savedIndex = *first;
        *first = *second;
        *second = savedIndex;
    }
}

static void MenuSortSubType02(int *first, int *second)
{
    int firstCount = MenuBoxChk(*first);

    if (firstCount < MenuBoxChk(*second)) {
        int savedIndex = *first;
        *first = *second;
        *second = savedIndex;
    }
}

static void MenuSortSubType01(unsigned int *first, unsigned int *second)
{
    unsigned int firstValue = *first;
    unsigned int secondValue = *second;

    if (firstValue < secondValue) {
        *first = secondValue;
        *second = firstValue;
    }
}

static void MenuSortSubType00(unsigned int *first, unsigned int *second)
{
    unsigned int firstValue = *first;
    unsigned int secondValue = *second;

    if (secondValue < firstValue) {
        *first = secondValue;
        *second = firstValue;
    }
}

typedef signed int s32;
extern char f_11[24];

void MenuSortChange(s32 arg0, s32 arg1)
{
  extern int sort[2][0x100];
  s32 *temp_20;
  s32 *var_17;
  s32 *var_3;
  s32 *var_5;
  s32 temp_22;
  s32 temp_2;
  s32 var_21;
  s32 var_3_2;
  void (*temp_23)(s32 *, s32 *);
  s32 var_16;
  s32 var_18;
  temp_20 = sort[arg0];
  var_21 = 0;
  var_3 = f_11;
  temp_23 = ((void (**)(s32 *, s32 *))var_3)[arg1];
  var_3 = &temp_20[1];
  if ((*temp_20) != 0)
  {
    do
    {
      temp_2 = *var_3;
      var_3++;
      var_21 += 1;
    }
    while (0 != temp_2);
  }
  if (var_21)
  {
    var_3_2 = 0;
    if (0 < (var_21 - 1))
    {
      var_18 = 1;
      do
      {
        var_18 = var_3_2 + 1;
        if (var_18 < var_21)
        {
          var_17 = (s32 *)((char *)(var_18 * 4) + (int)temp_20);
          var_16 = var_18;
          var_16 = var_21 - var_16;
          do
          {
            var_5 = var_17;
            var_17++;
            var_16 -= 1;
            temp_23(&temp_20[var_3_2], var_5);
            var_5 = var_17;
          }
          while (var_16);
        }
        var_3_2 = var_18;
      }
      while (var_3_2 < (var_21 - 1));
    }
  }
}

extern int sort[2][0x100];

int MenuSortCheck(int listIndex)
{
    int *row = sort[listIndex];
    int *entry = row + 1;
    int value;
    int count = 0;

    if (*row != 0) {
        do {
            value = *entry;
            entry++;
            count++;
        } while (value != 0);
    }
    return count;
}

extern unsigned char D_004C5438[];
int MenuIdChange(short id, int highPart);
typedef struct { unsigned char unmodeled_00[0xC]; unsigned short categoryMask; unsigned char mountMask; } WeaponViewSS;
typedef struct { unsigned char unmodeled_00[0xA]; unsigned short category; unsigned char unmodeled_0C[0x2]; unsigned short associatedId; } AccessoryViewSS;
typedef struct { unsigned char unmodeled_00[0x8E]; short etherId[12]; } UnitOrgViewSS;

void MenuSortSet(int bufferIndex, int categoryMask, int filter)
{
    struct MenuWeaponRangePair {
        unsigned char first;
        unsigned char last;
    } weaponRanges[9];
    int party[8];
    int *entry = sort[bufferIndex];
    int id;
    int first;
    int last;
    int range;
    int weaponCategory;
    int weaponPosition;
    int bitIndex;
    unsigned int mountBits;
    int availableMounts;
    int accessoryCategory;
    int requiresAssociatedId;
    int partyCount;
    int *partyMember;
    int currentMember;
    WeaponViewSS *weapon;
    AccessoryViewSS *accessory;
    UnitOrgViewSS *character;
    short selectedCharId;
    short *etherId;
    int currentEtherId;

    memset(entry, 0, sizeof(sort[bufferIndex]));
    if (categoryMask & 1) {
        if (filter == -10) {
            for (id = 1; id < 73; id++) {
                *entry++ = MenuIdChange((short)id, 1);
            }
        } else {
            id = 1;
            goto inventory_bound;
            do {
                if (dataItmBoxChk(id) > 0) {
                    *entry++ = MenuIdChange((short)id, 1);
                }
                id++;
inventory_bound:
                ;
            } while (id < 73);
        }
    }
    if (categoryMask & 2) {
        first = 1;
        last = 254;
        if (filter == -1) {
            first = 53;
            last = 55;
        }
        for (id = first; id <= last; id++) {
            if (dataEvtBoxChk(id) > 0) {
                if (id != 10 || MenuEquipStealMaskCheck() != 1) {
                    *entry++ = (id & 0xffff) + 0x20000;
                }
            }
        }
    }
    if ((categoryMask & 4) && MenuEquipStealMaskCheck() == 0) {
        __builtin_memcpy(weaponRanges, D_004C5438, sizeof(weaponRanges));
        weaponPosition = 0;
        switch (filter) {
        case -2:
            range = 7;
            weaponCategory = 0x2000;
            break;
        case -1:
            weaponCategory = 0x4000;
            range = 8;
            break;
        default:
            if (filter & 0x2000) {
                weaponCategory = 0x2000;
                weaponPosition = filter & ~0x2000;
                range = 7;
            } else {
                weaponCategory = 0x4000;
                range = filter - 1;
            }
            break;
        }
        for (id = weaponRanges[range].first; id <= weaponRanges[range].last; id++) {
            if (dataWpnBoxChk(id) > 0) {
                weapon = (WeaponViewSS *)func_A1A3D8(id);
                if (!(weapon->categoryMask & weaponCategory)) {
                    continue;
                }
                if (weaponPosition != 0) {
                    availableMounts = weapon->mountMask;
                    mountBits = 0x88;
                    for (bitIndex = 0; bitIndex < 2; bitIndex++) {
                        if (availableMounts & mountBits) {
                            availableMounts &= mountBits;
                            break;
                        }
                        mountBits >>= 1;
                    }
                    if (!(availableMounts & OptMntToPosEquip2(WpnPosToOptMnt(weaponPosition, id) & 0xff))) {
                        continue;
                    }
                }
                *entry++ = MenuIdChange((short)id, 3);
            }
        }
    }
    if (categoryMask & 8) {
        id = 0;
        last = 0;
        if (MenuEquipStealMaskCheck() == 0) {
            switch (filter) {
            case 3: id = 1; last = 15; break;
            case 2: id = 16; last = 30; break;
            case 5: id = 31; last = 45; break;
            case 7: id = 46; last = 63; break;
            case -1: id = 1; last = 63; break;
            case -2: id = 64; last = 178; break;
            case -3: id = 1; last = 178; break;
            }
            for (; id <= last; id++) {
                if (dataBltBoxChk(id) > 0) {
                    *entry++ = MenuIdChange((short)id, 4);
                }
            }
        }
    }
    if ((categoryMask & 0x10) && MenuEquipStealMaskCheck() == 0) {
        accessoryCategory = filter;
        requiresAssociatedId = 0;
        if (filter > 0) {
            accessoryCategory = filter & 0xff;
            requiresAssociatedId = filter >> 8;
        }
        switch (accessoryCategory) {
        case -1:
            for (id = 1; id < 129; id++) {
                if (dataAccBoxChk(id) > 0) {
                    *entry++ = MenuIdChange((short)id, 5);
                }
            }
            break;
        case -2:
            for (id = 129; id < 181; id++) {
                if (dataAccBoxChk(id) > 0) {
                    *entry++ = MenuIdChange((short)id, 5);
                }
            }
            break;
        case 0:
        case 1:
        case 2:
            for (id = 1; id < 129; id++) {
                if (dataAccBoxChk(id) <= 0) {
                    continue;
                }
                accessory = (AccessoryViewSS *)func_A1A548(id);
                if (requiresAssociatedId != 0 && accessory->associatedId == 0) {
                    continue;
                }
                switch (accessoryCategory) {
                case 0:
                    if (accessory->category == 1) {
                        *entry++ = MenuIdChange((short)id, 5);
                    }
                    break;
                case 1:
                    if (accessory->category == 0) {
                        *entry++ = MenuIdChange((short)id, 5);
                    }
                    break;
                case 2:
                    if (accessory->category == 2) {
                        *entry++ = MenuIdChange((short)id, 5);
                    }
                    break;
                }
            }
            break;
        }
    }
    if ((categoryMask & 0x20) && filter == 1) {
        partyMember = party;
        partyCount = PartyAllPartyGet(partyMember, 1);
        id = 0;
        if (partyCount > 0) {
            do {
                currentMember = *partyMember;
                if (currentMember == 0) {
                    break;
                }
                if (currentMember != 2 && currentMember != 5) {
                    *entry++ = MenuIdChange((short)*partyMember, 6);
                }
                id++;
                partyMember++;
            } while (id < partyCount);
        }
    }
    if (categoryMask & 0x40) {
        selectedCharId = filter;
        for (id = 1; id < 91; id++) {
            if (func_A19698(selectedCharId, id) != 0) {
                *entry++ = (id & 0xffff) + 0x80000;
            }
        }
    }
    if (categoryMask & 0x80) {
        selectedCharId = filter;
        if (((unsigned int)filter >> 16) == 0) {
            for (id = 1; id < 80; id++) {
                if (func_A19578(selectedCharId, id) != 0) {
                    *entry++ = (id & 0xffff) + 0x90000;
                }
            }
        } else {
            id = 0;
            character = (UnitOrgViewSS *)func_A191C0(selectedCharId);
            etherId = character->etherId;
            currentEtherId = *etherId;
            if (currentEtherId != 0) {
                currentEtherId = (currentEtherId & 0xffff) + 0x90000;
                do {
                    do {
                        *entry++ = currentEtherId;
                        ++id;
                        ++etherId;
                    } while (0);
                    if (id < 12) {
                        currentEtherId = *etherId;
                        if (currentEtherId == 0) {
                            break;
                        }
                        currentEtherId = (currentEtherId & 0xffff) + 0x90000;
                        continue;
                    }
                    break;
                } while (1);
            }
        }
    }
    *entry = 0;
}

void *MenuSortAddrGet(int listIndex)
{
    return sort[listIndex];
}

int MenuSortGet(int listIndex, int entryIndex)
{
    return sort[listIndex][entryIndex];
}

int MenuIdChange(short id, int highPart)
{
    return id + (highPart << 16);
}

void subListMake00(MenuListEntry *entry, int index)
{
    entry->name = MenuTextGet(index)->name;
    entry->amount = MenuBoxChk(index);
    entry->flag = 0;
}

void subListMake01(MenuListEntry *entry, int index)
{
    entry->name = MenuTextGet(index)->name;
    entry->amount = MenuBoxMoneyGet(index, 0);
    entry->flag = 0;
}

extern const char msg_12[];

/*
 * mw_list is defined below, right before MenuListGet, which is the only
 * other function reading it directly; forward-declared again here since
 * MenuListMake needs it earlier in the file.
 */
extern MenuListEntry mw_list[2][256];

/*
 * mode == -10 selects the shop price fill (subListMake01, which fills
 * amount from MenuBoxMoneyGet); every other value uses the plain owned-
 * count fill (subListMake00). Callers include MenuShopListChange00/01
 * (still asm).
 */
MenuListEntry *MenuListMake(int listIndex, int mode)
{
    MenuListEntry *list = mw_list[listIndex];
    MenuListEntry *entry = list;
    int *row = sort[listIndex];
    void (*fill)(MenuListEntry *, int);
    int index;
    int next;

    fill = (mode == -0xA) ? subListMake01 : subListMake00;

    index = *row;
    if (index != 0) {
        do {
            fill(entry, index);
            entry++;
            row++;
            next = *row;
            index = next;
        } while (next != 0);
    }
    entry->amount = -1;
    entry->name = msg_12;
    entry->flag = 0;
    entry[1].name = 0;
    return list;
}

extern MenuListEntry mw_list[2][256];

MenuListEntry *MenuListGet(int listIndex)
{
    return mw_list[listIndex];
}

extern char fname_13[];
extern char *f_base_14[3];
extern char *f_exten_15[5];
extern char *f_ukn_name_16[4];
extern char *f_char_mot_17[3];
extern char *f_mons_mot_18[3];
void RES_GetMdlFileName(char *name, int charaId);

char *MenuFileNameGet(short arg0, unsigned int arg1)
{
    char *var_5;
    int temp_5;
    int temp_16;
    int temp_lo;
    char *temp_17;

    switch (arg1) {
    case 0:
    case 1:
    case 2:
        if (arg0 == 0x2201) {
            strcpy(fname_13, f_base_14[2]);
            strcat(fname_13, f_ukn_name_16[0]);
        } else {
            RES_GetMdlFileName(fname_13, arg0);
        }
        var_5 = f_exten_15[arg1];
        goto tail;
    case 3:
        temp_5 = (arg0 >> 0xC) & 0xF;
        switch (temp_5) {
        case 0:
            strcpy(fname_13, f_base_14[0]);
            strcat(fname_13, f_char_mot_17[0]);
            var_5 = f_exten_15[3];
            strcat(fname_13, var_5);
            goto done;
        case 2:
            if (arg0 == 0x2201) {
                strcpy(fname_13, f_base_14[0]);
                strcat(fname_13, f_char_mot_17[1]);
                var_5 = f_exten_15[3];
                strcat(fname_13, var_5);
                goto done;
            } else {
                strcpy(fname_13, f_base_14[0]);
                strcat(fname_13, f_char_mot_17[2]);
                var_5 = f_exten_15[3];
                strcat(fname_13, var_5);
                goto done;
            }
            break;
        case 4:
            temp_17 = f_mons_mot_18[0];
            temp_16 = (arg0 != 0x4037) ? (arg0 - 0x4000) : 0x72;
            strcpy(fname_13, f_base_14[1]);
            temp_17[7] = (char)((temp_16 % 10) + 0x30);
            temp_lo = temp_16 / 10;
            temp_17[6] = (char)((temp_lo % 10) + 0x30);
            temp_17[5] = (char)(((temp_lo / 10) % 10) + 0x30);
            strcat(fname_13, temp_17);
            var_5 = f_exten_15[4];
            strcat(fname_13, var_5);
            goto done;
        default:
            goto done;
        }
    tail:
        strcat(fname_13, var_5);
        goto done;
    case 4:
    case 5:
        RES_GetMdlFileName(fname_13, arg0 + 0x10000);
        if (arg1 == 4) {
            strcat(fname_13, f_exten_15[0]);
        } else {
            strcat(fname_13, f_exten_15[2]);
        }
        goto done;
    default:
        goto done;
    }
done:
    return fname_13;
}

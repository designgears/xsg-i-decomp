/*
 * OV12 original TU 27: 0x00a1c7d8..0x00a1e618 (20 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov12/rg_simple_db.h"
#include "ov12/rg_singleton_id.h"
#include "rg_weapon_db.h"

static void _EntryTemporaries(RgSimpleDB *database) {

}

extern void *memcpy(void *destination, const void *source, unsigned int count);

/*
 * Copies at most nSize bytes from pszSrc into pszDest, then always writes a
 * NUL at pszDest[nSize - 1] (memcpy, main:0x0032eccc, followed by a single
 * sb $0,-1(dest)): the destination is terminated even when the source is
 * not, unlike a plain memcpy.
 */
static void _copy_str_n(char *pszDest, const char *pszSrc, u32 nSize)
{
    memcpy(pszDest, pszSrc, nSize);
    pszDest[nSize - 1] = 0;
}

static void _WrapperDestruct(RgSimpleDB *database)
{
    DisposeRgSimpleDB(database);
}

RgSimpleDB *InstanceOfRgWeaponDB(void)
{
    RgSimpleDB *database = RgSingletonIDGet(2);

    if (database == 0) {
        database = CreateRgSimpleDB(48, 16);
        _EntryTemporaries(database);
        RgSingletonIDEntry(2, database, _WrapperDestruct);
    }
    return database;
}

extern void assert_prog(const char *expression, const char *source_file,
                        int line);
extern unsigned int strlen(const char *string);

/*
 * Scaffold-owned (.rodata still owner: asm, config/tu-build.json
 * data_ownership window 0x00a53816..0x00a53cf0). ov12:0x00a53818 holds
 * "pDB != NIL", ov12:0x00a53828 holds "../rg_weapon_db.euc.c".
 */
const char D_00A53818[] = "pDB != NIL";
const char D_00A53828[] = "../rg_weapon_db.euc.c";
const char D_00A53840[] = "left";
const char D_00A53848[] = "right";
const char D_00A53850[] = "back";
const char D_00A53858[] = "unknown equip type '%s'";
const char D_00A53870[] = "'%c' is not 'o' or 'x'";
const char D_00A53888[] = "'%s' is not '???-??-???'";
const char D_00A538A8[] = "'%s' is not '???-??-???' format";
const char D_00A538C8[] = "cont";
const char D_00A538D0[] = ".";
const char D_00A538D8[] = "notlock";
const char D_00A538E0[] = "full-mot";
const char D_00A538F0[] = "'full-mot' cannot spec to AttackWeapon";
const char D_00A53918[] = "'full-mot' cannot spec to ShieldWeapon";
const char D_00A53940[] = "'full-mot' cannot spec to UnArmedWeapon";
const char D_00A53968[] = "not-adv";
const char D_00A53970[] = "adv";
const char D_00A53978[] = "no-robmot";
const char D_00A53988[] = "target";
const char D_00A53990[] = "not";
const char D_00A53998[] = "aft";
const char D_00A539A0[] = "wth";
const char D_00A539A8[] = "unknown keyword '%s-target'\ncollect not,aft,wth";
const char D_00A539D8[] = "'%s' is error in 'cont'";
const char D_00A539F0[] = "num";
const char D_00A539F8[] = "energy";
const char D_00A53A00[] = "hardness";
const char D_00A53A10[] = "inf";
const char D_00A53A18[] = "lock-range";
const char D_00A53A28[] = "ignored 'lock-range'";
const char D_00A53A40[] = "lock";
const char D_00A53A48[] = "shot";
const char D_00A53A50[] = "unknown shot '%s'";
const char D_00A53A68[] = "mot";
const char D_00A53A70[] = "mot-all";
const char D_00A53A78[] = "szStr[0] == '('";
const char D_00A53A88[] = "def";
const char D_00A53A90[] = "selfmot";
const char D_00A53A98[] = "selfmot-all";
const char D_00A53AA8[] = "basemot";
const char D_00A53AB0[] = "basemot-all";
const char D_00A53AC0[] = "model";

extern void RgSimpleDBClear(RgSimpleDB *pDB);
extern int RgSimpleDBFind(RgSimpleDB *pDB, const char *pszName);
extern int RgSimpleDBSize(RgSimpleDB *pDB);
extern void *RgSimpleDBGet(RgSimpleDB *pDB, int nDataID);
extern void RgSimpleDBDump(RgSimpleDB *pDB);

void RgWeaponDBClear(RgSimpleDB *pDB)
{
    if (pDB == 0) {
        assert_prog(D_00A53818, D_00A53828, 85);
    }
    RgSimpleDBClear(pDB);
    _EntryTemporaries(pDB);
}

void *RgWeaponDBGetEssence(RgSimpleDB *pDB, const char *pszName)
{
    int nDataID;

    if ((pszName == 0) || (strlen(pszName) == 0)) {
        return 0;
    }
    nDataID = RgSimpleDBFind(pDB, pszName);
    if (nDataID >= 0) {
        return RgSimpleDBGet(pDB, nDataID);
    }
    return 0;
}

int RgWeaponDBGetSize(RgSimpleDB *pDB)
{
    if (pDB == 0) {
        assert_prog(D_00A53818, D_00A53828, 113);
    }
    return RgSimpleDBSize(pDB);
}

void *RgWeaponDBGetIndex(RgSimpleDB *pDB, int nDataID)
{
    if (pDB == 0) {
        assert_prog(D_00A53818, D_00A53828, 121);
    }
    return RgSimpleDBGet(pDB, nDataID);
}

/*
 * Opaque handle owned by ov12/tu036 (src/ov12/rg_read_text.c,
 * src/ov12/rg_read_text.h): this TU only forwards the pointer between
 * RgReadTextGetString/-GetFloat/-IsEOF/-Unget and RgError, it never reads or
 * writes a member of it.
 */
typedef struct RgReadText RgReadText;

extern void RgError(const char *message, const char *source_file, int line,
                    ...);
extern void RgReadTextGetString(RgReadText *pReader, char *pszOut);
extern int strcasecmp(const char *s1, const char *s2);

/*
 * Scaffold-owned data (same window as above). ov12:0x00a53840 holds "left",
 * 0x00a53848 "right", 0x00a53850 "back": the three equip-slot keywords
 * _ReadEquipType recognizes. 0x00a53858 holds "unknown equip type '%s'", the
 * RgError message it reports for anything else.
 */
/*
 * The equip-slot indices the strings above are keyed to; "left" (index 0) is
 * the value _ReadEquipType falls through to below without a named constant.
 */
#define RG_EQUIP_RIGHT 1
#define RG_EQUIP_BACK  2

static int _ReadEquipType(RgReadText *pReader)
{
    char szToken[0x80];
    int result;

    RgReadTextGetString(pReader, szToken);
    result = strcasecmp(szToken, D_00A53840);
    if (result != 0) {
        if (strcasecmp(szToken, D_00A53848) == 0) {
            return RG_EQUIP_RIGHT;
        }
        if (strcasecmp(szToken, D_00A53850) == 0) {
            return RG_EQUIP_BACK;
        }
        RgError(D_00A53858, D_00A53828, 0x8C, szToken);
        result = -1;
        return result;
    }
    return result;
}

static int _IsOnChar(char marker)
{
    if (marker == 'o') {
        return 1;
    }
    if (marker == 'm') {
        return 2;
    }
    if (marker == 'x') {
        return 0;
    }
    RgError(D_00A53870, D_00A53828, 157, marker);
    return 0;
}

static int _ReadEquipBitMask(RgReadText *pReader, unsigned int *pMask)
{
    char szToken[0x80];
    unsigned int mask;
    int equipmentType = -1;

    RgReadTextGetString(pReader, szToken);
    if (strlen(szToken) != 10) {
        RgError(D_00A53888, D_00A53828, 169, szToken);
    }
    if (szToken[3] != '-') {
        RgError(D_00A538A8, D_00A53828, 170, szToken);
    }
    if (szToken[6] != '-') {
        RgError(D_00A538A8, D_00A53828, 171, szToken);
    }

    mask = 0;
    if (_IsOnChar(szToken[0]) != 0) mask |= 0x01;
    if (_IsOnChar(szToken[1]) != 0) mask |= 0x02;
    if (_IsOnChar(szToken[2]) != 0) mask |= 0x04;
    if (_IsOnChar(szToken[9]) != 0) mask |= 0x08;
    if (_IsOnChar(szToken[8]) != 0) mask |= 0x10;
    if (_IsOnChar(szToken[7]) != 0) mask |= 0x20;
    if (_IsOnChar(szToken[4]) != 0) mask |= 0x40;
    if (_IsOnChar(szToken[5]) != 0) mask |= 0x80;

    if (_IsOnChar(szToken[0]) == 2) equipmentType = 0;
    if (_IsOnChar(szToken[1]) == 2) equipmentType = 1;
    if (_IsOnChar(szToken[2]) == 2) equipmentType = 2;
    if (_IsOnChar(szToken[9]) == 2) equipmentType = 3;
    if (_IsOnChar(szToken[8]) == 2) equipmentType = 4;
    if (_IsOnChar(szToken[7]) == 2) equipmentType = 5;
    if (_IsOnChar(szToken[4]) == 2) equipmentType = 6;
    if (_IsOnChar(szToken[5]) == 2) equipmentType = 7;

    if (equipmentType != -1) {
        mask |= 1u << equipmentType;
    }
    *pMask = mask;
    return equipmentType;
}

/*
 * RG_ACTOR_CHAR_ROBNUM (6): the same evidenced constant as ov12/tu026's
 * RgWeapon.shotMotionTable[RG_ACTOR_CHAR_ROBNUM][3] (src/ov12/rg_weapon.h),
 * the number of party characters who can pilot an AGWS robot. The loop below
 * fills the equivalent per-character motion table one row at a time.
 */
#define RG_ACTOR_CHAR_ROBNUM 6

static void _SetAllCharMotion(int motionTable[][3], int motion0, int motion1,
                               int motion2)
{
    unsigned int i;

    for (i = 0; i < RG_ACTOR_CHAR_ROBNUM; i++) {
        motionTable[i][0] = motion0;
        motionTable[i][1] = motion1;
        motionTable[i][2] = motion2;
    }
}

/* The volume clamp _ReadCommon applies to the "v*snd" keys (inlined). */
static inline int _Clamp(int value, int low, int high)
{
    if (value < low) {
        return low;
    }
    return high < value ? high : value;
}

#define weaponDbContinueKeyword D_00A538C8
#define weaponDbEndMarkerKeyword D_00A538D0
#define weaponDbNotLockKeyword D_00A538D8
#define weaponDbFullMotionKeyword D_00A538E0
#define weaponDbFullMotionAttackErrorText D_00A538F0
#define weaponDbFullMotionShieldErrorText D_00A53918
#define weaponDbFullMotionUnarmedErrorText D_00A53940
#define weaponDbNotAdvanceKeyword D_00A53968
#define weaponDbAdvanceKeyword D_00A53970
#define weaponDbNoRobotMotionKeyword D_00A53978
#define weaponDbTargetSuffixKeyword D_00A53988
#define weaponDbNotPrefixKeyword D_00A53990
#define weaponDbAfterKeyword D_00A53998
#define weaponDbWithKeyword D_00A539A0
#define weaponDbUnknownTargetKeywordFormat D_00A539A8
#define weaponDbContinueErrorFormat D_00A539D8
#define weaponDbNumKeyword D_00A539F0
#define weaponDbEnergyKeyword D_00A539F8
#define weaponDbHardnessKeyword D_00A53A00
#define weaponDbInfiniteKeyword D_00A53A10
#define weaponDbLockRangeKeyword D_00A53A18
#define weaponDbIgnoredLockRangeText D_00A53A28
#define weaponDbLockKeyword D_00A53A40
#define weaponDbShotKeyword D_00A53A48
#define weaponDbUnknownShotFormat D_00A53A50
#define weaponDbMotionKeyword D_00A53A68
#define weaponDbMotionAllKeyword D_00A53A70
#define weaponDbAssertOpenParen D_00A53A78
#define weaponDbDefaultKeyword D_00A53A88
#define weaponDbSelfMotionKeyword D_00A53A90
#define weaponDbSelfMotionAllKeyword D_00A53A98
#define weaponDbBaseMotionKeyword D_00A53AA8
#define weaponDbBaseMotionAllKeyword D_00A53AB0
#define weaponDbModelKeyword D_00A53AC0
extern const short D_00A53AC8[];
#define weaponDbRobotModelDirectory D_00A53AC8
const char D_00A53AE0[] = "modelall";
const char D_00A53AF0[] = "use";
const char D_00A53AF8[] = "unknown char name '%s'";
const char D_00A53B10[] = "equip";
const char D_00A53B18[] = "occupy";
const char D_00A53B20[] = "fake";
const char D_00A53B28[] = "mass";
const char D_00A53B30[] = "weight";
const char D_00A53B38[] = "name";
const char D_00A53B40[] = "v-dir";
const char D_00A53B48[] = "h-dir";
const char D_00A53B50[] = "hitsnd";
const char D_00A53B58[] = "shotsnd";
const char D_00A53B60[] = "bgsnd";
const char D_00A53B68[] = "lifesnd";
const char D_00A53B70[] = "vhitsnd";
const char D_00A53B78[] = "vshotsnd";
const char D_00A53B88[] = "vbgsnd";
const char D_00A53B90[] = "vlifesnd";
const char D_00A53BA0[] = "1wa";
const char D_00A53BA8[] = "2wa";
const char D_00A53BB0[] = "3wa";
const char D_00A53BB8[] = "default";
const char D_00A53BC0[] = "shot-pnt";
const char D_00A53BD0[] = "pEss->m_uShotPntNum <= RG_WEAPON_SHOTPNT_MAX";
const char D_00A53C00[] = "0 <= uDatIdx && uDatIdx < 8";
#define weaponDbModelAllKeyword D_00A53AE0
#define weaponDbUseKeyword D_00A53AF0
#define weaponDbUnknownCharNameFormat D_00A53AF8
#define weaponDbEquipKeyword D_00A53B10
#define weaponDbOccupyKeyword D_00A53B18
#define weaponDbFakeKeyword D_00A53B20
#define weaponDbMassKeyword D_00A53B28
#define weaponDbWeightKeyword D_00A53B30
#define weaponDbNameKeyword D_00A53B38
#define weaponDbVerticalDirectionKeyword D_00A53B40
#define weaponDbHorizontalDirectionKeyword D_00A53B48
#define weaponDbHitSoundKeyword D_00A53B50
#define weaponDbShotSoundKeyword D_00A53B58
#define weaponDbBackgroundSoundKeyword D_00A53B60
#define weaponDbLifeSoundKeyword D_00A53B68
#define weaponDbVoiceHitSoundKeyword D_00A53B70
#define weaponDbVoiceShotSoundKeyword D_00A53B78
#define weaponDbVoiceBackgroundSoundKeyword D_00A53B88
#define weaponDbVoiceLifeSoundKeyword D_00A53B90
#define weaponDbFirstWeaponArmKeyword D_00A53BA0
#define weaponDbSecondWeaponArmKeyword D_00A53BA8
#define weaponDbThirdWeaponArmKeyword D_00A53BB0
#define weaponDbDefaultArmKeyword D_00A53BB8
#define weaponDbShotPointKeyword D_00A53BC0
#define weaponDbAssertShotPointCountInRange D_00A53BD0
#define weaponDbAssertDataIndexInRange D_00A53C00
#define weaponDbSourcePath D_00A53828
#define XRG_PI 3.1415927f
#define RG_WEAPON_VOLUME_SCALE 128.0f
#define RG_WEAPON_VOLUME_MAX 128
#define RG_WEAPON_CONTROL_KIND_MASK 0x07
#define RG_WEAPON_CONTROL_KIND_FULL_MOT 1
#define RG_WEAPON_CONTROL_KIND_ATTACK 2
#define RG_WEAPON_CONTROL_KIND_SHIELD 3
#define RG_WEAPON_CONTROL_KIND_UNARMED 4
#define RG_WEAPON_CONTROL_TARGET_MASK 0x18
#define RG_WEAPON_CONTROL_TARGET_WTH 0x08
#define RG_WEAPON_CONTROL_TARGET_AFT 0x10
#define RG_WEAPON_CONTROL_LOCKON 0x20
#define RG_WEAPON_CONTROL_ADV 0x40
#define RG_WEAPON_CONTROL_NO_ROBMOT 0x80
static int s_aeJntTbl_0[8] = {8, 9, 10, 11, 12, 13, 14, 15};
extern float dptofp(double value);
extern double atof(const char *s);
extern char *strcat(char *d, const char *s);
extern void RgWarn(const char *message, const char *source_file, int line, ...);
extern int RgReadTextGetInt(RgReadText *pReader);
extern float RgReadTextGetFloat(RgReadText *pReader);
extern int RgActorMotIDGetFromName(const char *name);
extern int RgActorNameToCharID(const char *name);
extern struct RgShotEssence *RgShotDBGetEssence(void *db, const char *name);
extern void *InstanceOfRgShotDB(void);
typedef struct RgWeEquip { int mounts[12]; int mountCount; int activeMount; } RgWeEquip;
extern void RgEquipAddMount(RgWeEquip *pEquip, unsigned int eType, int ePos, unsigned int posMask);
extern void RgEquipSetDefault(RgWeEquip *pEquip, unsigned int posMask);
typedef struct RgWeActorSpec { int actorID; char fileName[0x80]; int state; } RgWeActorSpec;
typedef struct RgWeaponEssenceFull {
    void *m_pCreateMethod; unsigned int controlFlags; float lockOnAngleMin; float lockOnAngleMax;
    struct RgShotEssence *shotEssence; float capacity; float shotPitch; float shotYaw;
    int aimAxis; int shotPntNum; int shotPnt[16]; int unmodeled_68; unsigned char unmodeled_6c[4];
    RgWeEquip equip; int useMask; float mass;
    int shotMotionTable[6][3]; int shotStartMotionTable[6][3]; int shotEndMotionTable[6][3];
    int actorFlag; unsigned char unmodeled_18c[4]; RgWeActorSpec actorSpec[3];
    unsigned char unmodeled_328[8]; char databaseName[0x20]; char displayName[0x30];
    int shotSound, robotHitSound, bgHitSound, lifeSound, shotVolume, robotHitVolume, bgHitVolume, lifeVolume;
    int eventLevel; int isDefault; unsigned char unmodeled_3a8[8];
} RgWeaponEssenceFull;

static int _ReadCommon(void *pEss, RgReadText *pReader, char *pszToken)
{
    RgWeaponEssenceFull *pWeapon;
    char szToken[0x80];
    char szName[0x40];
    unsigned int equipMask;
    unsigned int occupyMask;
    int equipmentType;
    int mountType;
    unsigned int i;
    int volume;
    char firstChar;
    int level;
    pWeapon = (RgWeaponEssenceFull *)pEss;

    if (strcasecmp(pszToken, weaponDbContinueKeyword) == 0) {
        RgReadTextGetString(pReader, szToken);
        while (strcasecmp(szToken, weaponDbEndMarkerKeyword) != 0) {
            if (strcasecmp(szToken, weaponDbNotLockKeyword) == 0) {
                pWeapon->controlFlags &= ~RG_WEAPON_CONTROL_LOCKON;
                pWeapon->controlFlags &= ~RG_WEAPON_CONTROL_TARGET_MASK;
                pWeapon->controlFlags |= RG_WEAPON_CONTROL_TARGET_WTH;
            } else if (strcasecmp(szToken, weaponDbFullMotionKeyword) == 0) {
                if ((pWeapon->controlFlags & RG_WEAPON_CONTROL_KIND_MASK) == RG_WEAPON_CONTROL_KIND_ATTACK) {
                    RgError(weaponDbFullMotionAttackErrorText, weaponDbSourcePath, 224);
                } else if ((pWeapon->controlFlags & RG_WEAPON_CONTROL_KIND_MASK) == RG_WEAPON_CONTROL_KIND_SHIELD) {
                    RgError(weaponDbFullMotionShieldErrorText, weaponDbSourcePath, 226);
                } else if ((pWeapon->controlFlags & RG_WEAPON_CONTROL_KIND_MASK) == RG_WEAPON_CONTROL_KIND_UNARMED) {
                    RgError(weaponDbFullMotionUnarmedErrorText, weaponDbSourcePath, 228);
                } else {
                    pWeapon->controlFlags = (pWeapon->controlFlags & ~RG_WEAPON_CONTROL_KIND_MASK) | RG_WEAPON_CONTROL_KIND_FULL_MOT;
                }
            } else if (strcasecmp(szToken, weaponDbNotAdvanceKeyword) == 0) {
                pWeapon->controlFlags &= ~RG_WEAPON_CONTROL_ADV;
            } else if (strcasecmp(szToken, weaponDbAdvanceKeyword) == 0) {
                pWeapon->controlFlags |= RG_WEAPON_CONTROL_ADV;
            } else if (strcasecmp(szToken, weaponDbNoRobotMotionKeyword) == 0) {
                pWeapon->controlFlags |= RG_WEAPON_CONTROL_NO_ROBMOT;
            } else if (strcasecmp(szToken + 4, weaponDbTargetSuffixKeyword) == 0) {
                pWeapon->controlFlags &= ~RG_WEAPON_CONTROL_TARGET_MASK;
                szToken[3] = 0;
                if (strcasecmp(szToken, weaponDbNotPrefixKeyword) != 0) {
                    if (strcasecmp(szToken, weaponDbAfterKeyword) == 0) {
                        pWeapon->controlFlags |= RG_WEAPON_CONTROL_TARGET_AFT;
                    } else if (strcasecmp(szToken, weaponDbWithKeyword) == 0) {
                        pWeapon->controlFlags |= RG_WEAPON_CONTROL_TARGET_WTH;
                    } else {
                        RgError(weaponDbUnknownTargetKeywordFormat, weaponDbSourcePath, 256);
                    }
                }
            } else {
                RgError(weaponDbContinueErrorFormat, weaponDbSourcePath, 260, szToken);
            }
            RgReadTextGetString(pReader, szToken);
        }
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbNumKeyword) == 0 || strcasecmp(pszToken, weaponDbEnergyKeyword) == 0 ||
        strcasecmp(pszToken, weaponDbHardnessKeyword) == 0) {
        RgReadTextGetString(pReader, szName);
        if (strcasecmp(szName, weaponDbInfiniteKeyword) == 0) {
            pWeapon->capacity = 1.0e8f;
        } else {
            pWeapon->capacity = dptofp(atof(szName));
        }
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbLockRangeKeyword) == 0) {
        RgReadTextGetFloat(pReader);
        RgWarn(weaponDbIgnoredLockRangeText, weaponDbSourcePath, 282);
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbLockKeyword) == 0) {
        pWeapon->lockOnAngleMin = RgReadTextGetFloat(pReader) * XRG_PI / 180.0f;
        pWeapon->lockOnAngleMax = RgReadTextGetFloat(pReader) * XRG_PI / 180.0f;
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbShotKeyword) == 0) {
        RgReadTextGetString(pReader, szToken);
        pWeapon->shotEssence = RgShotDBGetEssence(InstanceOfRgShotDB(), szToken);
        if (pWeapon->shotEssence == 0) {
            RgError(weaponDbUnknownShotFormat, weaponDbSourcePath, 294, szToken);
        }
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbMotionKeyword) == 0) {
        int motion0;
        int motion1;
        int motion2;

        RgReadTextGetString(pReader, szToken);
        motion0 = RgActorMotIDGetFromName(szToken);
        RgReadTextGetString(pReader, szToken);
        motion1 = RgActorMotIDGetFromName(szToken);
        RgReadTextGetString(pReader, szToken);
        motion2 = RgActorMotIDGetFromName(szToken);
        _SetAllCharMotion(pWeapon->shotMotionTable, motion0, motion1, motion2);
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbMotionAllKeyword) == 0) {
        RgReadTextGetString(pReader, szToken);
        if (szToken[0] != '(') {
            assert_prog(weaponDbAssertOpenParen, weaponDbSourcePath, 307);
        }
        RgReadTextGetString(pReader, szToken);
        while (szToken[0] != ')') {
            int motion0;
            int motion1;
            int motion2;
            int charID;

            if (strcasecmp(szToken, weaponDbDefaultKeyword) == 0) {
                RgReadTextGetString(pReader, szToken);
                motion0 = RgActorMotIDGetFromName(szToken);
                RgReadTextGetString(pReader, szToken);
                motion1 = RgActorMotIDGetFromName(szToken);
                RgReadTextGetString(pReader, szToken);
                motion2 = RgActorMotIDGetFromName(szToken);
                _SetAllCharMotion(pWeapon->shotMotionTable, motion0, motion1, motion2);
            } else {
                charID = RgActorNameToCharID(szToken);
                RgReadTextGetString(pReader, szToken);
                motion0 = RgActorMotIDGetFromName(szToken);
                RgReadTextGetString(pReader, szToken);
                motion1 = RgActorMotIDGetFromName(szToken);
                RgReadTextGetString(pReader, szToken);
                motion2 = RgActorMotIDGetFromName(szToken);
                pWeapon->shotMotionTable[charID][0] = motion0;
                pWeapon->shotMotionTable[charID][1] = motion1;
                pWeapon->shotMotionTable[charID][2] = motion2;
            }
            RgReadTextGetString(pReader, szToken);
        }
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbSelfMotionKeyword) == 0) {
        int motion0;
        int motion1;
        int motion2;

        RgReadTextGetString(pReader, szToken);
        motion0 = RgActorMotIDGetFromName(szToken);
        RgReadTextGetString(pReader, szToken);
        motion1 = RgActorMotIDGetFromName(szToken);
        RgReadTextGetString(pReader, szToken);
        motion2 = RgActorMotIDGetFromName(szToken);
        _SetAllCharMotion(pWeapon->shotStartMotionTable, motion0, motion1, motion2);
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbSelfMotionAllKeyword) == 0) {
        RgReadTextGetString(pReader, szToken);
        if (szToken[0] != '(') {
            assert_prog(weaponDbAssertOpenParen, weaponDbSourcePath, 339);
        }
        RgReadTextGetString(pReader, szToken);
        while (szToken[0] != ')') {
            int motion0;
            int motion1;
            int motion2;
            int charID;

            if (strcasecmp(szToken, weaponDbDefaultKeyword) == 0) {
                RgReadTextGetString(pReader, szToken);
                motion0 = RgActorMotIDGetFromName(szToken);
                RgReadTextGetString(pReader, szToken);
                motion1 = RgActorMotIDGetFromName(szToken);
                RgReadTextGetString(pReader, szToken);
                motion2 = RgActorMotIDGetFromName(szToken);
                _SetAllCharMotion(pWeapon->shotStartMotionTable, motion0, motion1, motion2);
            } else {
                charID = RgActorNameToCharID(szToken);
                RgReadTextGetString(pReader, szToken);
                motion0 = RgActorMotIDGetFromName(szToken);
                RgReadTextGetString(pReader, szToken);
                motion1 = RgActorMotIDGetFromName(szToken);
                RgReadTextGetString(pReader, szToken);
                motion2 = RgActorMotIDGetFromName(szToken);
                pWeapon->shotStartMotionTable[charID][0] = motion0;
                pWeapon->shotStartMotionTable[charID][1] = motion1;
                pWeapon->shotStartMotionTable[charID][2] = motion2;
            }
            RgReadTextGetString(pReader, szToken);
        }
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbBaseMotionKeyword) == 0) {
        int motion0;
        int motion1;
        int motion2;

        RgReadTextGetString(pReader, szToken);
        motion0 = RgActorMotIDGetFromName(szToken);
        RgReadTextGetString(pReader, szToken);
        motion1 = RgActorMotIDGetFromName(szToken);
        RgReadTextGetString(pReader, szToken);
        motion2 = RgActorMotIDGetFromName(szToken);
        _SetAllCharMotion(pWeapon->shotEndMotionTable, motion0, motion1, motion2);
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbBaseMotionAllKeyword) == 0) {
        RgReadTextGetString(pReader, szToken);
        if (szToken[0] != '(') {
            assert_prog(weaponDbAssertOpenParen, weaponDbSourcePath, 371);
        }
        RgReadTextGetString(pReader, szToken);
        while (szToken[0] != ')') {
            int motion0;
            int motion1;
            int motion2;
            int charID;

            if (strcasecmp(szToken, weaponDbDefaultKeyword) == 0) {
                RgReadTextGetString(pReader, szToken);
                motion0 = RgActorMotIDGetFromName(szToken);
                RgReadTextGetString(pReader, szToken);
                motion1 = RgActorMotIDGetFromName(szToken);
                RgReadTextGetString(pReader, szToken);
                motion2 = RgActorMotIDGetFromName(szToken);
                _SetAllCharMotion(pWeapon->shotEndMotionTable, motion0, motion1, motion2);
            } else {
                charID = RgActorNameToCharID(szToken);
                RgReadTextGetString(pReader, szToken);
                motion0 = RgActorMotIDGetFromName(szToken);
                RgReadTextGetString(pReader, szToken);
                motion1 = RgActorMotIDGetFromName(szToken);
                RgReadTextGetString(pReader, szToken);
                motion2 = RgActorMotIDGetFromName(szToken);
                pWeapon->shotEndMotionTable[charID][0] = motion0;
                pWeapon->shotEndMotionTable[charID][1] = motion1;
                pWeapon->shotEndMotionTable[charID][2] = motion2;
            }
            RgReadTextGetString(pReader, szToken);
        }
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbModelKeyword) == 0) {
        RgReadTextGetString(pReader, szToken);
        for (i = 0; i < 3; i++) {
            pWeapon->actorSpec[i].actorID = RG_ACTOR_CHAR_ROBNUM;
            __builtin_memcpy(pWeapon->actorSpec[i].fileName, weaponDbRobotModelDirectory, 18);
            strcat(pWeapon->actorSpec[i].fileName, szToken);
        }
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbModelAllKeyword) == 0) {
        for (i = 0; i < 3; i++) {
            RgReadTextGetString(pReader, szToken);
            pWeapon->actorSpec[i].actorID = RG_ACTOR_CHAR_ROBNUM;
            __builtin_memcpy(pWeapon->actorSpec[i].fileName, weaponDbRobotModelDirectory, 18);
            strcat(pWeapon->actorSpec[i].fileName, szToken);
        }
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbUseKeyword) == 0) {
        int charID;

        pWeapon->useMask = 0;
        RgReadTextGetString(pReader, szToken);
        while (strcasecmp(szToken, weaponDbEndMarkerKeyword) != 0) {
            charID = RgActorNameToCharID(szToken);
            if (charID == -1) {
                RgError(weaponDbUnknownCharNameFormat, weaponDbSourcePath, 426, szToken);
            }
            pWeapon->useMask |= 1 << charID;
            RgReadTextGetString(pReader, szToken);
        }
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbEquipKeyword) == 0) {
        _ReadEquipBitMask(pReader, &equipMask);
        RgEquipSetDefault(&pWeapon->equip, equipMask);
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbOccupyKeyword) == 0) {
        equipmentType = _ReadEquipType(pReader);
        occupyMask = 0;
        mountType = _ReadEquipBitMask(pReader, &occupyMask);
        RgEquipAddMount(&pWeapon->equip, equipmentType, mountType, occupyMask);
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbFakeKeyword) == 0) {
        pWeapon->equip.activeMount = 1;
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbMassKeyword) == 0 || strcasecmp(pszToken, weaponDbWeightKeyword) == 0) {
        pWeapon->mass = RgReadTextGetFloat(pReader);
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbNameKeyword) == 0) {
        RgReadTextGetString(pReader, szToken);
        _copy_str_n(pWeapon->displayName, szToken, 48);
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbVerticalDirectionKeyword) == 0) {
        pWeapon->shotPitch = -RgReadTextGetFloat(pReader) * XRG_PI / 180.0f;
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbHorizontalDirectionKeyword) == 0) {
        pWeapon->shotYaw = -RgReadTextGetFloat(pReader) * XRG_PI / 180.0f;
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbHitSoundKeyword) == 0) {
        pWeapon->robotHitSound = RgReadTextGetInt(pReader);
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbShotSoundKeyword) == 0) {
        pWeapon->shotSound = RgReadTextGetInt(pReader);
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbBackgroundSoundKeyword) == 0) {
        pWeapon->bgHitSound = RgReadTextGetInt(pReader);
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbLifeSoundKeyword) == 0) {
        pWeapon->lifeSound = RgReadTextGetInt(pReader);
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbVoiceHitSoundKeyword) == 0) {
        pWeapon->robotHitSound = RgReadTextGetInt(pReader);
        volume = (int)(RgReadTextGetFloat(pReader) * RG_WEAPON_VOLUME_SCALE);
        level = _Clamp(volume, 0, RG_WEAPON_VOLUME_MAX);
        pWeapon->robotHitVolume = level;
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbVoiceShotSoundKeyword) == 0) {
        pWeapon->shotSound = RgReadTextGetInt(pReader);
        volume = (int)(RgReadTextGetFloat(pReader) * RG_WEAPON_VOLUME_SCALE);
        level = _Clamp(volume, 0, RG_WEAPON_VOLUME_MAX);
        pWeapon->shotVolume = level;
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbVoiceBackgroundSoundKeyword) == 0) {
        pWeapon->bgHitSound = RgReadTextGetInt(pReader);
        volume = (int)(RgReadTextGetFloat(pReader) * RG_WEAPON_VOLUME_SCALE);
        level = _Clamp(volume, 0, RG_WEAPON_VOLUME_MAX);
        pWeapon->bgHitVolume = level;
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbVoiceLifeSoundKeyword) == 0) {
        pWeapon->lifeSound = RgReadTextGetInt(pReader);
        volume = (int)(RgReadTextGetFloat(pReader) * RG_WEAPON_VOLUME_SCALE);
        level = _Clamp(volume, 0, RG_WEAPON_VOLUME_MAX);
        pWeapon->lifeVolume = level;
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbFirstWeaponArmKeyword) == 0) {
        pWeapon->eventLevel = 0;
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbSecondWeaponArmKeyword) == 0) {
        pWeapon->eventLevel = 1;
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbThirdWeaponArmKeyword) == 0) {
        pWeapon->eventLevel = 2;
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbDefaultArmKeyword) == 0) {
        pWeapon->isDefault = 1;
        return 1;
    }

    if (strcasecmp(pszToken, weaponDbShotPointKeyword) == 0) {
        unsigned char *pChar;
        unsigned int nextChar;
        unsigned int shotChar;
        int *pShotPntNum;
        unsigned int shotPnt;
        int tableIndex;

        pWeapon->shotPntNum = 0;
        RgReadTextGetString(pReader, szToken);
        firstChar = szToken[0];
        pChar = (unsigned char *)szToken;
        shotChar = *pChar;
        if (firstChar != 0) {
            do {
                pShotPntNum = &pWeapon->shotPntNum;
                if ((unsigned int)(shotChar - '1') < 8) {
                    shotPnt = *pShotPntNum;
                    *pShotPntNum = shotPnt + 1;
                    tableIndex = (char)shotChar - '1';
                    if (!(shotPnt + 1 < 17)) {
                        assert_prog(weaponDbAssertShotPointCountInRange, weaponDbSourcePath, 552);
                    }
                    if (!((unsigned int)tableIndex < 8)) {
                        assert_prog(weaponDbAssertDataIndexInRange, weaponDbSourcePath, 553);
                    }
                    pWeapon->shotPnt[shotPnt] = s_aeJntTbl_0[tableIndex];
                }
                pChar++;
                nextChar = *pChar;
                shotChar = nextChar;
            } while (nextChar != 0);
        }
        return 1;
    }

    return 0;
}

extern void *RgHeapAlloc(void *heap, unsigned int size, const char *source_file,
                         int line);
extern RgHeap *InstanceOfRgHeap(void);
extern float RgReadTextGetFloat(RgReadText *pReader);
extern int RgReadTextIsEOF(RgReadText *pReader);
extern void RgReadTextUnget(RgReadText *pReader, char *pszToken);

/*
 * _ReadCommon is one of two file-local statics of that name in this overlay
 * (the other, ov12:0x00a18730, belongs to a different translation unit).
 * This one, ov12:0x00a1cd08, is still INCLUDE_ASM above and shared by every
 * Read*Type function below: it consumes the token they already read and
 * reports (zero) whether it recognized a field common to every essence type.
 */
static int _ReadCommon(void *pEss, RgReadText *pReader, char *pszToken);

/*
 * The prefix every weapon essence the weapon database builds shares: each
 * _Read*Type function of rg_weapon_db.c hands its own essence to _ReadCommon
 * (ov12:0x00a1cd08), which fills these members from the common keys.
 *
 * shotEssence (+0x10): the "shot" key (ov12:0x00a53a48) names an entry of
 * the shot database. _ReadCommon stores RgShotDBGetEssence's result here
 * (sw $2,0x10 at ov12:0x00a1d130) and reports "unknown shot '%s'" when there
 * is none. That entry is whichever shot kind RgShotDBRead built (normal,
 * homing, grenade, beam or fire), so it is typed by the prefix all of them
 * share. rg_weapon.c's _InitWeaponCommon copies it onto the weapon
 * (ov12:0x00a198ec/0x00a19904), and _ShotTypeShot and _EnergyPassTime pass
 * that copy to CreateRgShotFromEssence (ov12:0x00a1a0ec/0x00a1b0dc).
 *
 * capacity (+0x14): the "num", "energy" and "hardness" keys all store one
 * float here, and "inf" stores 1e8 (swc1 0x14 at ov12:0x00a1d02c/0x00a1d048).
 * _InitWeaponCommon copies it to the weapon's shot count (ov12:0x00a198c0/
 * 0x00a198d0).
 *
 * _ReadCommon also writes parts of the span before +0x10 (the "lock" key
 * stores two angles at +0x08/+0x0c); that span stays unmodeled here.
 *
 * shotEssence is spelled with the struct tag so that this type needs no
 * typedef from rg_shot.c's header; RgShotEssence (src/ov12/rg_shot.h)
 * completes the same tag.
 */
typedef struct RgWeaponEssence RgWeaponEssence;
struct RgWeaponEssence {
    unsigned char unmodeled_00[0x10];
    struct RgShotEssence *shotEssence; /* +0x10 */
    float capacity;                    /* +0x14 */
};

/*
 * Scaffold-owned data. ov12:0x00a53c20 holds "pReader != NIL", the argument
 * check every Read*Type function below reports; 0x00a53c30 holds "busy", the
 * key _ReadShotType recognizes for a shot type's cooldown.
 */
const char D_00A53C20[] = "pReader != NIL";
const char D_00A53C30[] = "busy";

/*
 * The per-instance data _ReadShotType allocates. ov12/tu026's rg_weapon.h
 * only forward-declares this tag (its own evidence, _CreateWeaponShotType's
 * null checks, does not reach the interior); this allocation's own field
 * write below completes it.
 */
typedef struct RgWeaponShotEssence RgWeaponShotEssence;
struct RgWeaponShotEssence {
    unsigned char unmodeled_000[0x3b0];
    float busyTime;             /* +0x3b0, the "busy" key above */
    unsigned char unmodeled_3b4[0x0c];
};

extern void InitRgWeaponShotEssence(RgWeaponShotEssence *pEss);

static RgWeaponShotEssence *_ReadShotType(RgReadText *pReader)
{
    char szToken[0x80];
    RgWeaponShotEssence *pEss;

    if (pReader == 0) {
        assert_prog(D_00A53C20, D_00A53828, 0x238);
    }
    pEss = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgWeaponShotEssence),
                       D_00A53828, 0x23A);
    InitRgWeaponShotEssence(pEss);
    for (;;) {
        if (RgReadTextIsEOF(pReader) == 0) {
            RgReadTextGetString(pReader, szToken);
            if (_ReadCommon(pEss, pReader, szToken) == 0) {
                if (strcasecmp(szToken, D_00A53C30) == 0) {
                    pEss->busyTime = RgReadTextGetFloat(pReader);
                    continue;
                } else {
                    RgReadTextUnget(pReader, szToken);
                }
            } else {
                continue;
            }
        }
        break;
    }

    return pEss;
}

/*
 * Scaffold-owned data. ov12:0x00a53c38 holds "damage", 0x00a53c40 "hiteff":
 * the two keys _ReadAttackType and _ReadUnArmedType both recognize.
 */
const char D_00A53C38[] = "damage";
const char D_00A53C40[] = "hiteff";

/*
 * The per-instance data _ReadAttackType allocates. ov12/tu026's rg_weapon.h
 * only forward-declares this tag; this allocation's own field writes below
 * complete it.
 */
typedef struct RgWeaponAttackEssence RgWeaponAttackEssence;
struct RgWeaponAttackEssence {
    unsigned char unmodeled_000[0x3b8];
    float damage;                /* +0x3b8, the "damage" key above */
    char hitEffectName[0x24];    /* +0x3bc, the "hiteff" key above */
};

extern void InitRgWeaponAttackEssence(RgWeaponAttackEssence *pEss);

static RgWeaponAttackEssence *_ReadAttackType(RgReadText *pReader)
{
    char szToken[0x80];
    RgWeaponAttackEssence *pEss;

    if (pReader == 0) {
        assert_prog(D_00A53C20, D_00A53828, 0x252);
    }
    pEss = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgWeaponAttackEssence),
                       D_00A53828, 0x254);
    InitRgWeaponAttackEssence(pEss);
    while (RgReadTextIsEOF(pReader) == 0) {
        RgReadTextGetString(pReader, szToken);
        if (_ReadCommon(pEss, pReader, szToken) == 0) {
            if (strcasecmp(szToken, D_00A53C38) == 0) {
                pEss->damage = RgReadTextGetFloat(pReader);
                continue;
            } else {
                if (strcasecmp(szToken, D_00A53C40) == 0) {
                    RgReadTextGetString(pReader, pEss->hitEffectName);
                    continue;
                }
                RgReadTextUnget(pReader, szToken);
            }
            break;
        }
    }

    return pEss;
}

/*
 * The per-instance data _ReadUnArmedType allocates. ov12/tu026's rg_weapon.h
 * only forward-declares this tag. Its layout matches RgWeaponAttackEssence:
 * the same "damage"/"hiteff" keys at the same offsets.
 */
typedef struct RgWeaponUnArmedEssence RgWeaponUnArmedEssence;
struct RgWeaponUnArmedEssence {
    unsigned char unmodeled_000[0x3b8];
    float damage;                /* +0x3b8, the "damage" key above */
    char hitEffectName[0x24];    /* +0x3bc, the "hiteff" key above */
};

extern void InitRgWeaponUnArmedEssence(RgWeaponUnArmedEssence *pEss);

static RgWeaponUnArmedEssence *_ReadUnArmedType(RgReadText *pReader)
{
    char szToken[0x80];
    RgWeaponUnArmedEssence *pEss;

    if (pReader == 0) {
        assert_prog(D_00A53C20, D_00A53828, 0x26F);
    }
    pEss = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgWeaponUnArmedEssence),
                       D_00A53828, 0x271);
    InitRgWeaponUnArmedEssence(pEss);
    while (RgReadTextIsEOF(pReader) == 0) {
        RgReadTextGetString(pReader, szToken);
        if (_ReadCommon(pEss, pReader, szToken) == 0) {
            if (strcasecmp(szToken, D_00A53C38) == 0) {
                pEss->damage = RgReadTextGetFloat(pReader);
                continue;
            } else {
                if (strcasecmp(szToken, D_00A53C40) == 0) {
                    RgReadTextGetString(pReader, pEss->hitEffectName);
                    continue;
                }
                RgReadTextUnget(pReader, szToken);
            }
            break;
        }
    }

    return pEss;
}

/*
 * RgWeaponShieldEssence is fully evidenced and completed by ov12/tu026
 * (src/ov12/rg_weapon.h, InitRgWeaponShieldEssence's own field writes); this
 * TU only allocates and forwards the pointer, so it stays opaque here.
 */
typedef struct RgWeaponShieldEssence RgWeaponShieldEssence;

extern void InitRgWeaponShieldEssence(RgWeaponShieldEssence *pEss);

static RgWeaponShieldEssence *_ReadShieldType(RgReadText *pReader)
{
    char szToken[0x80];
    RgWeaponShieldEssence *pEss;

    if (pReader == 0) {
        assert_prog(D_00A53C20, D_00A53828, 0x28C);
    }
    pEss = RgHeapAlloc(InstanceOfRgHeap(), 0x3B0, D_00A53828, 0x28E);
    InitRgWeaponShieldEssence(pEss);
    for (;;) {
        if (RgReadTextIsEOF(pReader) == 0) {
            RgReadTextGetString(pReader, szToken);
            if (_ReadCommon(pEss, pReader, szToken) == 0) {
                RgReadTextUnget(pReader, szToken);
            } else {
                continue;
            }
        }
        break;
    }

    return pEss;
}

typedef struct RgWeaponEnergyEssence RgWeaponEnergyEssence;
struct RgWeaponEnergyEssence {
    unsigned char unmodeled_000[0x3b0];
    float shotEnergy;
    float upTemp;
    float limitTemp;
    float downTemp;
};

extern void InitRgWeaponEnergyEssence(RgWeaponEnergyEssence *pEss);

static RgWeaponEnergyEssence *_ReadEnergyType(RgReadText *pReader)
{
    char szToken[0x80];
    RgWeaponEnergyEssence *pEss;

    if (pReader == 0) {
        assert_prog(D_00A53C20, D_00A53828, 676);
    }
    pEss = RgHeapAlloc(InstanceOfRgHeap(), sizeof(RgWeaponEnergyEssence),
                       D_00A53828, 678);
    InitRgWeaponEnergyEssence(pEss);
    while (RgReadTextIsEOF(pReader) == 0) {
        RgReadTextGetString(pReader, szToken);
        if (_ReadCommon(pEss, pReader, szToken) == 0) {
            if (strcasecmp(szToken, "shot-energy") == 0) {
                pEss->shotEnergy = RgReadTextGetFloat(pReader);
                continue;
            } else {
                if (strcasecmp(szToken, "up-temp") == 0) {
                    pEss->upTemp = RgReadTextGetFloat(pReader);
                    continue;
                } else {
                    if (strcasecmp(szToken, "down-temp") == 0) {
                        pEss->downTemp = RgReadTextGetFloat(pReader);
                        continue;
                    } else {
                        if (strcasecmp(szToken, "limit-temp") == 0) {
                            pEss->limitTemp = RgReadTextGetFloat(pReader);
                            continue;
                        }
                        RgReadTextUnget(pReader, szToken);
                        break;
                    }
                }
            }
        }
    }

    return pEss;
}

/*
 * Database names occupy two fields shared by every type-specific essence.
 * The parser stores the record key at +0x330 and uses +0x350 as its optional
 * display name, defaulting it to the key when _ReadCommon left it empty.
 */
typedef struct RgWeaponDBRecord RgWeaponDBRecord;
struct RgWeaponDBRecord {
    RgWeaponEssence common;
    unsigned char unmodeled_18[0x318];
    char databaseName[0x20];
    char displayName[0x30];
};

const char D_00A53C80[] = "pszDataBase != NIL";
const char D_00A53C98[] = "shottype";
const char D_00A53CA8[] = "atktype";
const char D_00A53CB0[] = "sldtype";
const char D_00A53CB8[] = "enetype";
const char D_00A53CC0[] = "sudetype";
const char D_00A53CD0[] = "%s : unknown keyword : %s";

extern char *strcpy(char *destination, const char *source);
extern void RgSimpleDBEntry(RgSimpleDB *pDB, void *pDat,
                            const char *pszName);
extern RgReadText *CreateRgReadText(const char *pszDataBase);
extern void DisposeRgReadText(RgReadText *pReader);

void RgWeaponDBRead(RgSimpleDB *pDB, const char *pszDataBase)
{
    char szToken[0x100];
    RgReadText *pReader;
    RgWeaponDBRecord *pEntry;

    if (pDB == 0) {
        assert_prog(D_00A53818, D_00A53828, 709);
    }
    if (pszDataBase == 0) {
        assert_prog(D_00A53C80, D_00A53828, 710);
    }

    pReader = CreateRgReadText(pszDataBase);
    if (pReader != 0) {
        while (RgReadTextIsEOF(pReader) == 0) {
            pEntry = 0;
            RgReadTextGetString(pReader, szToken);

            if (strcasecmp(szToken, D_00A53C98) == 0) {
                RgReadTextGetString(pReader, szToken);
                pEntry = (RgWeaponDBRecord *)_ReadShotType(pReader);
            } else if (strcasecmp(szToken, D_00A53CA8) == 0) {
                RgReadTextGetString(pReader, szToken);
                pEntry = (RgWeaponDBRecord *)_ReadAttackType(pReader);
            } else if (strcasecmp(szToken, D_00A53CB0) == 0) {
                RgReadTextGetString(pReader, szToken);
                pEntry = (RgWeaponDBRecord *)_ReadShieldType(pReader);
            } else if (strcasecmp(szToken, D_00A53CB8) == 0) {
                RgReadTextGetString(pReader, szToken);
                pEntry = (RgWeaponDBRecord *)_ReadEnergyType(pReader);
            } else if (strcasecmp(szToken, D_00A53CC0) == 0) {
                RgReadTextGetString(pReader, szToken);
                pEntry = (RgWeaponDBRecord *)_ReadUnArmedType(pReader);
            } else {
                RgError(D_00A53CD0, D_00A53828, 746, pszDataBase, szToken);
            }

            if (pEntry != 0) {
                RgSimpleDBEntry(pDB, pEntry, szToken);
                strcpy(pEntry->databaseName, szToken);
                if (strlen(pEntry->displayName) == 0) {
                    _copy_str_n(pEntry->displayName, szToken, 48);
                }
            }
        }
        DisposeRgReadText(pReader);
    }
}

void RgWeaponDBDump(RgSimpleDB *pDB)
{
    RgSimpleDBDump(pDB);
}

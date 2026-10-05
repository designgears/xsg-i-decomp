/*
 * OV01 original TU 13: 0x00a2c9f8..0x00a2e4b0 (34 functions)
 */
#include "common.h"
#include "shared.h"
#include "snd.h"

#define SND_MU_VOLUME_MAX 0x7F
#define SND_MU_FADE_TIME  2000

extern int sndBankGet();

typedef struct {
    SndSeLoadWork work;             /* +0x00 */
    unsigned char unmodeled_0c[8]; /* +0x0c */
} SndSeRegBank;

typedef struct {
    int id;               /* +0x00 */
    SndSeRegBank bank[5]; /* +0x04 */
} SndSeRegDat;

extern SndSeRegDat sndSeRegDat[6];

/* ov01:0x00a2d1f8. Returns the address of sndSeDat (ov01:0x00a42d18, size
 * 0x68), the base of loaded sound-effect bank metadata; only its address is
 * evidenced by this allocation, not its internal layout. */
extern unsigned char sndSeDat[0x68];

void sndInit(void)
{
    int i;

    for (i = 0; i < 6; i++) {
        memset(&sndSeRegDat[i], 0, sizeof(SndSeRegDat));
        if (i < 3) {
            sndSeRegDat[i].bank[0].work.dest = (void *)(0x1914000 + i * 0x50000);
        } else {
            sndSeRegDat[i].bank[0].work.dest = (void *)(0x1905000 + i * 0x55000);
        }
    }
    memset(sndSeDat, 0, 0x68);
    *(int *)(sndSeDat + 8) = 0x1B4E000;
    memset(&sndMuDat, 0, sizeof(SndMuData));
    sndMuDat.smdNormal = (void *)0x1904000;
}

extern int dmgBankOffs[3];
extern int dmgBankStat[3];
extern int ctrlId;

void sndInit2(void)
{
    int i;

    for (i = 0; i < 3; i++) {
        dmgBankOffs[i] = 0;
        dmgBankStat[i] = 0;
    }
    ctrlId = 0;
}

typedef struct SndStEdData {
    int unitId;
    const char *name;
    int condition[8];
} SndStEdData;

extern int dataSndStEdNumSub(ObjectTask *unit, int condition);
extern int rnd(int maximum);

/* The caller selects btst/bted records; every live record starts with the
 * unconditional condition 1, so at least one candidate is always written. */
int dataSndStEdNum(SndStEdData *data, ObjectTask *unit)
{
    int candidate[8];
    int index;
    int count;
    int *conditions;
    int *condition;

    conditions = data->condition;
    count = 0;
    for (index = 0; index < 8; index++) {
        condition = &conditions[index];
        if (dataSndStEdNumSub(unit, *condition)) {
            if (*condition >= 7) {
                candidate[0] = index + 1;
                count = 1;
                break;
            }
            candidate[count] = index + 1;
            count++;
        }
    }
    return candidate[rnd(count - 1)];
}

INCLUDE_ASM("asm/nonmatchings/ov01/snd", dataSndStEdNumSub);

extern SndStEdData btst[];
extern SndStEdData bted[];

char *dataSndStEdNameGet(char *name, ObjectTask *unit, int type)
{
    SndStEdData *data;
    SndStEdData *table;
    char num[2];
    SndStEdData **pdata;
    short charaId;
    int n;

    charaId = calcUPGet(unit)->charaId;
    num[0] = '0';
    num[1] = '\0';
    if (type == 0x18) {
        table = btst;
    } else {
        table = bted;
    }
    if (table->unitId != 0) {
        for (data = table; data->unitId != 0; data++) {
            pdata = &data;
            if ((*pdata)->unitId == charaId) {
                n = ((int (*)(SndStEdData *, ObjectTask *, int))dataSndStEdNum)(*pdata, unit, type);
                num[0] += n;
                strcpy(name, (*pdata)->name);
                strcat(name, num);
            }
        }
    }
    return name;
}

typedef struct {
    unsigned char unmodeled_00[0x11]; /* +0x00 */
    signed char category;             /* +0x11 */
    unsigned char unmodeled_12[4];    /* +0x12 */
    short spid;                       /* +0x16 */
} TecData;

extern TecData *dataTecGet(int tecId);

extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern char *strlwr(char *s);
extern char *sefGetEffectName(int effectNo);
extern unsigned char *dataWpnGet(int index);
extern char *D_00A424EC[];
extern char *plChaMidSeName[];
extern char *plAgwsMidSeName[];
extern char *enemyMidSeName[];

int dataSndSeNameGet(char *name, ObjectTask *unit, int seType)
{
    short charaId;
    char *base;
    short id;
    short wpn;

    charaId = calcUPGet(unit)->charaId;
    *name = 0;
    if (seType < 0x1A) {
        if (!(*(unsigned int *)unit->work & 0x40) && !(calcUPGet(unit)->flags & 0x40)
            && (calcUPGet(unit)->charaId < 0xBB || calcUPGet(unit)->charaId >= 0xC3)) {
            if ((unsigned int)(seType - 0x18) < 2) {
                dataSndStEdNameGet(name, unit, seType);
            } else {
                base = D_00A424EC[charaId];
                if (base != 0) {
                    strcpy(name, base);
                    strcat(name, plChaMidSeName[seType]);
                }
            }
        } else if (calcUPGet(unit)->flags & 0x40) {
            base = D_00A424EC[charaId];
            if (base != 0) {
                if (seType < 3) {
                    strcpy(name, base);
                    strcat(name, plAgwsMidSeName[seType]);
                } else {
                    wpn = calcUPGet(unit)->sefSetupParams[seType - 3];
                    if (wpn != 0) {
                        strcpy(name, sefGetEffectName(dataTecGet(*(short *)(dataWpnGet(wpn) + 0x14))->spid));
                    }
                }
            }
        } else {
            base = D_00A424EC[charaId];
            if (base != 0) {
                if (seType < 3) {
                    strcpy(name, base);
                    strcat(name, enemyMidSeName[seType]);
                } else {
                    id = ((short *)((char *)calcUPGet(unit) + 0x70))[seType];
                    if (id != 0) {
                        strcpy(name, base);
                        strcpy(name, sefGetEffectName(dataTecGet(id)->spid));
                    }
                }
            }
        }
    } else {
        strcpy(name, sefGetEffectName(seType - 0x1A));
    }
    strlwr(name);
    return (int)name;
}

void *sndSeAdrGet(void)
{
    return sndSeDat;
}

SndSeRegDat *sndSeRegAdrChk(int id)
{
    int i;

    for (i = 0; i < 6; i++) {
        if (sndSeRegDat[i].id == id) {
            return &sndSeRegDat[i];
        }
    }
    return 0;
}

SndSeRegDat *sndSeRegAdrChk(int id);

/*
 * ov01:0x00a2d248. Reuses an existing sound registration if one already
 * matches id; otherwise, for a character id below 0xBB, claims the first
 * free slot in the id's half of sndSeRegDat (0..2 below 0x21, 3..5
 * otherwise) and returns its address, or 0 if none is free or id is 0xBB or
 * above.
 */
SndSeRegDat *sndSeRegAdrGet(int id)
{
    SndSeRegDat *found;
    int i;
    int start;
    int end;

    found = sndSeRegAdrChk(id);
    if (found != 0) {
        return found;
    }
    if (id >= 0xBB) {
        return 0;
    }
    if (id < 0x21) {
        start = 0;
        end = 3;
    } else {
        start = 3;
        end = 6;
    }
    for (i = start; i < end; i++) {
        if (sndSeRegDat[i].id == 0) {
            sndSeRegDat[i].id = id;
            return &sndSeRegDat[i];
        }
    }
    return 0;
}

int sndSeRegRemove(int id)
{
    int i;
    int j;

    for (i = 0; i < 6; i++) {
        if (sndSeRegDat[i].id == id) {
            sndSeRegDat[i].id = 0;
            for (j = 4; j >= 0; j--) {
                sndSeRegDat[i].bank[j].work.seType = 0;
            }
            return 1;
        }
    }
    return 0;
}

typedef struct SndSeActorFlags {
    unsigned int flags;
} SndSeActorFlags;
#define SND_SE_ACTOR_FLAG_ENEMY 0x40u

/*
 * ov01:0x00a2d388. Reports whether the sound-effect index seType has a
 * registered bank for this unit: a player-side unit outside the boss charaId
 * range (0xBB..0xC2) registers 1..4 and 23, a boss or enemy unit in the
 * 0x97..0xBA charaId range registers 1 and 2, and every other enemy
 * registers 1..5.
 */
int sndSeRegChk(ObjectTask *unit, int seType)
{
    int registered = 0;

    if (!(((SndSeActorFlags *)unit->work)->flags & SND_SE_ACTOR_FLAG_ENEMY)
        && !(calcUPGet(unit)->flags & SND_SE_ACTOR_FLAG_ENEMY)
        && (calcUPGet(unit)->charaId < 0xBB || calcUPGet(unit)->charaId >= 0xC3)) {
        if (seType > 0 && (seType < 5 || seType == 23)) {
            registered = 1;
        }
    } else if (calcUPGet(unit)->charaId >= 0x97 && calcUPGet(unit)->charaId < 0xBB) {
        if (seType < 3) {
            if (seType > 0) {
                registered = 1;
            }
        }
    } else if (seType < 6) {
        registered = seType > 0;
    }
    return registered;
}

#include "ov01/calc.h"

/* calcUPGet's return type comes from its definer, ov01/tu004 calc.c
 * (published include/ov01/calc.h); calcUPGet itself is still asm there, so
 * this TU declares it the same way ov01/tu002 unit_cmd.c does. */
extern CalcUnitParam *calcUPGet(ObjectTask *unit);

/*
 * dataSndSeLoadSub's output descriptor. Only the three words the function
 * itself touches are evidenced: seType (sw zero,0(s2) ov01:0x00a2d504; sw
 * s3,0(s2) ov01:0x00a2d574), dest (lw a1,4(s2) ov01:0x00a2d540, read only)
 * and size (sw a1,8(s2) ov01:0x00a2d570).
 */


extern int dataSndSeNameGet(char *name, ObjectTask *unit, int seType);
extern void dataFileLoadNB(void *buffer, void *address);
extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern int xglCdGetFileSize(const char *name);
extern int printf(const char *format, ...);
extern char fileName[];
extern char *sndSeNameBase;
extern char *sndSeNameExt;
extern const char D_00A4E6A8[];
extern const char D_00A4E6D0[];

int dataSndSeLoadSub(ObjectTask *unit, int seType, SndSeLoadWork *work)
{
    char name;
    int fileSize;
    int withSlack;
    int clamped;
    int roundedSize;

    dataSndSeNameGet(&name, unit, seType);
    if (name == 0) {
        printf(D_00A4E6A8, calcUPGet(unit)->charaId, seType);
        work->seType = 0;
        return 0;
    }

    strcpy(fileName, sndSeNameBase);
    strcat(fileName, &name);
    strcat(fileName, sndSeNameExt);
    dataFileLoadNB(fileName, work->dest);

    fileSize = xglCdGetFileSize(fileName);
    withSlack = fileSize + 0x7FF;
    clamped = (withSlack < 0) ? (fileSize + 0xFFE) : withSlack;
    roundedSize = (clamped >> 0xB) << 0xB;
    work->size = roundedSize;
    printf(D_00A4E6D0, roundedSize);
    work->seType = seType;
    return 1;
}

extern const char D_00A4E6E8[];
typedef struct {
    int seType;
    int pad4[2];
    int a;
    int b;
} SndSeLoadDatWork;
typedef struct {
    int id;
    SndSeLoadDatWork work;
} SndSeLoadDat;

int dataSndSeLoad(ObjectTask *unit, int seType)
{
    short charaId;
    SndSeLoadDat *dat;
    SndSeLoadDatWork *w;

    charaId = calcUPGet(unit)->charaId;
    dat = ((SndSeLoadDat *(*)(int))sndSeAdrGet)(charaId);
    w = &dat->work;
    if (dat->id == charaId && w->seType == seType) {
        printf(D_00A4E6E8, charaId, seType);
        return 1;
    }
    w->b = 0;
    w->a = 0;
    if (dataSndSeLoadSub(unit, seType, (SndSeLoadWork *)w) == 0) {
        return 0;
    }
    dat->id = charaId;
    return 1;
}

typedef struct {
    int pad0;
    char *base;
    int pad8;
    char *a;
    char *b;
} SndSeLoad2Work;

void dataSndSeLoad2(ObjectTask *unit)
{
    SndSeLoad2Work *w;
    w = (SndSeLoad2Work *)((char *)((void *(*)(int))sndSeAdrGet)(*(short *)((char *)calcUPGet(unit) + 0x38)) + 4);
    if (w->a == 0 && w->b == 0) {
        w->a = w->base + ((int *)w->base)[2];
        w->b = w->base + ((int *)w->base)[3];
    }
}

extern const char D_00A4E708[];

/*
 * ov01:0x00a2d6a0. Claims a unit sound registration, loads its base bank,
 * then loads three AGWS-specific banks when applicable.
 */
int dataSndSeRegLoad(ObjectTask *unit)
{
    short charaId;
    SndSeRegDat *record;
    SndSeRegBank *bank;
    void *end;
    int seType;

    charaId = calcUPGet(unit)->charaId;
    if (sndSeRegAdrChk(charaId) != 0) {
        printf(D_00A4E708, charaId);
        return 1;
    }
    record = sndSeRegAdrGet(charaId);
    if (record == 0) {
        return 0;
    }
    bank = &record->bank[0];
    if (dataSndSeLoadSub(unit, 0, &bank->work) == 0) {
        return 0;
    }
    if (calcUPGet(unit)->flags & 0x40) {
        end = (unsigned char *)bank->work.dest + bank->work.size;
        for (seType = 3; seType < 6; seType++) {
            bank = &record->bank[seType - 1];
            bank->work.dest = end;
            if (dataSndSeLoadSub(unit, seType, &bank->work) == 0) {
                bank->work.size = 0;
                bank->work.seType = 0;
            }
            end = (unsigned char *)bank->work.dest + bank->work.size;
        }
    }
    record->id = charaId;
    return 1;
}

void dataSndSeRegLoad2(ObjectTask *unit)
{
    char name[16];
    SndSeRegDat *record;
    SndSeRegBank *bank;
    char *dest;
    int n;
    int i;

    dataSndSeNameGet(name, unit, 0);
    if (name[0] == 0) {
        return;
    }
    record = sndSeRegAdrGet(calcUPGet(unit)->charaId);
    bank = &record->bank[0];
    if (record == 0) {
        return;
    }
    dest = bank->work.dest;
    n = *(long *)dest / 2;
    for (i = 0; i < n; i++) {
        *(char **)&bank->unmodeled_0c[0] = dest + (int)((void **)dest)[i * 2 + 2];
        *(char **)&bank->unmodeled_0c[4] = dest + (int)((void **)dest)[i * 2 + 3];
        bank->work.seType = i + 1;
        bank++;
    }
    if (calcUPGet(unit)->flags & 0x40) {
        for (i = 2; i >= 0; i--) {
            if (bank->work.seType != 0) {
                dest = bank->work.dest;
                *(char **)&bank->unmodeled_0c[0] = dest + ((int *)dest)[2];
                *(char **)&bank->unmodeled_0c[4] = dest + ((int *)dest)[3];
            }
            bank++;
        }
    }
}

extern int sndConvRegNo(int id);
extern int xglSoundSendSed(void *sed, int bank);
extern void SsdCheckSpuMemory(int mode);
extern const char D_00A4E728[];
extern const char D_00A4E740[];
extern const char D_00A4E768[];
extern const char D_00A4E788[];
extern const char D_00A4E7B0[];
extern const char D_00A4E7C8[];

int sndSeTrans(int seId, int volume)
{
    short charaId;
    SndSeRegBank *bank;
    int *sed;
    int *swd;
    int b;
    int r1;
    int r2;

    if (volume == 0) {
        return 0;
    }
    charaId = calcUPGet((ObjectTask *)seId)->charaId;
    if (sndSeRegChk((ObjectTask *)seId, volume) != 0) {
        bank = &sndSeRegAdrGet(charaId)->bank[sndConvRegNo(volume)];
    } else {
        bank = (SndSeRegBank *)(sndSeDat + 4);
    }
    if (bank->work.seType != 0) {
        b = sndBankGet(seId, volume);
        if (volume == 2 && dmgBankOffs[b] != 0) {
            printf(D_00A4E728);
            return 0;
        }
        sed = *(int **)&bank->unmodeled_0c[0];
        if (sed[0] != 0x73646573) {
            printf(D_00A4E740, sed[0], sed);
            return 0;
        }
        if (sed[2] > 0x10000) {
            printf(D_00A4E768, sed[2]);
            return 0;
        }
        swd = *(int **)&bank->unmodeled_0c[4];
        if (swd[0] != 0x6D647773) {
            printf(D_00A4E788, swd[0], swd);
            return 0;
        }
        r1 = xglSoundSendSed(sed, b);
        r2 = xglSoundSendSwd(*(int **)&bank->unmodeled_0c[4], b);
        printf(D_00A4E7B0, r1, r2);
        if (r1 < 0 || r2 < 0) {
            SsdCheckSpuMemory(1);
        }
        return 1;
    }
    printf(D_00A4E7C8, charaId, volume);
    return 0;
}

extern void xglSoundEffectNormalID(int sound_id, int variant);

/* ov01:0x00a2da80. Plays system sound identifiers below 0x26 on variant 0
 * and reports whether the identifier was valid. */
int sndSysSePlay(int soundId)
{
    if (soundId >= 0x26) {
        return 0;
    }
    xglSoundEffectNormalID(soundId, 0);
    return 1;
}

extern int ctrlId;
extern int dmgBankOffs[3];
extern void xglSoundEffectNormalID(int soundId, int ctrlId);
extern const char D_00A4E7E8[];

/*
 * ov01:0x00a2dab0. Starts sound-effect id on the unit's bank. sndBankGet
 * resolves that bank from the unit the sound belongs to, which reaches
 * sndSePlay as the word sndSeTransPlay stores in the deferred task
 * (lw a0,28(s0) ov01:0x00a2e128) and is handed straight on (no argument setup
 * at the jal, ov01:0x00a2dac8). A damage sound
 * (id 2) plays once per bank: the first one marks the bank in dmgBankOffs and
 * every later one is dropped, and it does not advance the control id the other
 * sounds count with.
 */
int sndSePlay(int unit, int id, int no)
{
    int bank;

    if (id == 0) {
        return 0;
    }
    bank = sndBankGet(unit, id);
    if (id == 2) {
        if (dmgBankOffs[bank] != 0) {
            return 0;
        }
        dmgBankOffs[bank] = 1;
    }
    xglSoundEffectNormalID((bank << 16) | no, ctrlId);
    printf(D_00A4E7E8, bank, id, no, ctrlId);
    if (id != 2) {
        ctrlId++;
    }
    return 1;
}

extern int sndBankGet();
extern void xglSoundEffectStopID(int sound_id, int flags);
extern const char D_00A4E810[];

int sndSeStop(int unused, int pack)
{
    int stopped = 0;

    if (pack != 0) {
        xglSoundEffectStopID(sndBankGet() << 0x10, 0);
        printf(D_00A4E810, pack);
        stopped = 1;
    }
    return stopped;
}

typedef struct {
    int key;   /* +0x00 */
    int value; /* +0x04 */
} SndConvRegPair;
typedef struct {
    SndConvRegPair pair[7];
} SndConvRegTable;
extern const SndConvRegTable D_00A4E828;

/*
 * ov01:0x00a2dbc8. Searches a sentinel-terminated sound conversion pair
 * table and returns its mapped register number, or minus one.
 */
int sndConvRegNo(int id)
{
    SndConvRegTable table;
    int i;

    table = D_00A4E828;
    for (i = 0; table.pair[i].key != 0; i++) {
        if (table.pair[i].key == id) {
            return table.pair[i].value;
        }
    }
    return -1;
}

int sndBankGet(ObjectTask *unit, int seType)
{
    int bank;
    int i;

    if ((unsigned int)(seType - 5) < 0x12
        && !(*(unsigned int *)unit->work & 0x40)
        && !(calcUPGet(unit)->flags & 0x40)
        && (calcUPGet(unit)->charaId < 0xBB || calcUPGet(unit)->charaId >= 0xC3)) {
        bank = 2;
    } else if (seType >= 0x1A || seType == 0x18 || seType == 0x19
               || ((unsigned int)(seType - 3) < 5 && (*(unsigned int *)unit->work & 0x40))) {
        bank = 2;
    } else if (seType == 1) {
        bank = 3;
    } else if (seType == 2) {
        bank = 4;
        for (i = 0; i < 3; i++) {
            if (dmgBankOffs[i] == calcUPGet(unit)->charaId) {
                bank = i + 4;
                break;
            }
        }
        if (i < 3) {
        } else for (i = 0; i < 3; i++) {
            if (dmgBankOffs[i] == 0) {
                bank += i;
                dmgBankOffs[i] = calcUPGet(unit)->charaId;
                break;
            }
        }
    } else {
        bank = 1;
    }
    return bank;
}

typedef struct {
    unsigned char unmodeled_00[0x10]; /* +0x00 */
    short spid;                       /* +0x10 */
} ItmData;
extern ItmData *dataItmGet(int itmId);
extern int dataSpecBaseGet(int charaId);
extern int dataBakpBaseGet(int charaId);
#define SND_SPID_NORMAL  3
#define SND_SPID_SPECIAL 9
#define SND_SPID_BREAK   17
#define SND_SPID_EVENT   26

/*
 * ov01:0x00a2de30. Resolves the sound-effect index a battle command plays:
 * a technique (mode 1) by its category, the technique's or the item's own
 * recorded index (modes 2 and 3) and a fixed one for mode 9; every other mode
 * has none.
 */
int sndConvSpid(ObjectTask *unit, int mode, int tecId, int itmId)
{
    short charaId;
    int spid = 0;
    int category;

    charaId = calcUPGet(unit)->charaId;
    switch (mode) {
    case 1:
        category = dataTecGet(tecId)->category;
        switch (category) {
        case 6:
            spid = (tecId - dataBakpBaseGet(charaId)) + SND_SPID_BREAK;
            break;
        case 2:
            spid = (tecId - dataSpecBaseGet(charaId)) + SND_SPID_SPECIAL;
            break;
        case 4:
        case 5:
            spid = itmId + SND_SPID_NORMAL;
            break;
        default:
            spid = dataNormIdxGet(unit, tecId) + SND_SPID_NORMAL;
            break;
        }
        break;
    case 9:
        spid = 2;
        break;
    case 2:
        spid = dataTecGet(tecId)->spid + SND_SPID_EVENT;
        break;
    case 3:
        spid = dataItmGet(itmId)->spid + SND_SPID_EVENT;
        break;
    }
    return spid;
}

extern int dataNormIdxGet(ObjectTask *unit, int seType);

/*
 * The battle actor object unit->work points at once loaded is the same
 * Actor record ov01/tu002 unit_cmd.c defines. A TU-local header cannot be
 * included from another TU, so this TU restates the same partial view under
 * the same name, matching src/main/chr.h, src/main/near_dir.h,
 * src/main/set_motion.h and src/ov01/unit_cmd.h. Only the flags header word
 * this function tests is evidenced here (lw straight off unit->work, & 0x40,
 * the same ACTOR_FLAG_ENEMY bit ov01/unit_cmd.h documents).
 */
typedef struct Actor {
    unsigned int flags;
} Actor;

#define ACTOR_FLAG_ENEMY 0x40u

/* ov01:0x00a2df80. Picks the sound-effect normal-hit-reaction index for a
 * non-enemy unit outside the boss charaId range (0xBB..0xC2), returning
 * 0x21 when that index selects one of the two heavier reaction categories
 * (3 or 5); every other case passes seType through unchanged (result 1). */
int sndConvSe(ObjectTask *unit, int mode, int seType)
{
    int idx;
    int result = 1;

    if (mode == 1
        && !(((Actor *)unit->work)->flags & ACTOR_FLAG_ENEMY)
        && !(calcUPGet(unit)->flags & ACTOR_FLAG_ENEMY)
        && (calcUPGet(unit)->charaId < 0xBB || calcUPGet(unit)->charaId >= 0xC3)) {
        idx = dataNormIdxGet(unit, seType);
        if (idx != -1) {
            if (idx == 3 || idx == 5) {
                result = 0x21;
            }
        }
    }
    return result;
}

/* ov01:0x00a2e050. Returns hit sound 0x22 for normal indices three or five
 * under mode one; otherwise returns two. */
int sndConvHitSe(ObjectTask *unit, int mode, int seType)
{
    int idx;
    int result = 2;

    if (mode == 1) {
        idx = dataNormIdxGet(unit, seType);
        if (idx != -1) {
            if (idx == 3 || idx == 5) {
                result = 0x22;
            }
        }
    }
    return result;
}

extern void *objEntryPure();
extern int sndSeTrans(int seId, int volume);
void sndSeTransPlayObj(SndSePlayTask *task);

void sndSeTransPlay(int seId, int volume, int pan)
{
    SndSePlayTask *task;

    if (sndSeTrans(seId, volume) != 0) {
        task = objEntryPure(sndSeTransPlayObj);
        task->seId = seId;
        task->volume = volume;
        task->pan = pan;
    }
}

void sndSeTransPlayObj(SndSePlayTask *task)
{
    if (SsdSpuDmaCompleted(0) != 0) {
        return;
    }
    sndSePlay(task->seId, task->volume, task->pan);
    objRemovePure(&task->base);
}

extern char *sndMuNameBase;
extern char *sndMuNameExt1;
extern char *sndMuNameExt2;
extern char *muName[3];
extern char *mu2Name[3];

/*
 * ov01:0x00a2e160. Loads the three sound files of music set mode into
 * sndMuDat: the normal sequence, the alternate one the set may not have, and
 * the wave table, each laid out right behind the previous one and its size
 * rounded up to a 2048-byte CD sector. Returns whether the set exists.
 */
int dataSndMuLoad(int mode)
{
    SndMuData *mu;
    int smdFileSize;
    int altFileSize;
    int swdFileSize;
    int smdSectorSize;
    int altSectorSize;
    int swdSectorSize;

    if (mode >= 3) {
        return 0;
    }
    mu = &sndMuDat;

    strcpy(fileName, sndMuNameBase);
    strcat(fileName, muName[mode]);
    strcat(fileName, sndMuNameExt1);
    dataFileLoadNB(fileName, mu->smdNormal);
    smdFileSize = xglCdGetFileSize(fileName);
    smdSectorSize = smdFileSize + 0x7FF;
    if (smdSectorSize < 0) {
        smdSectorSize = smdFileSize + 0xFFE;
    }
    smdSectorSize >>= 0xB;
    smdSectorSize <<= 0xB;
    mu->smdSize = smdSectorSize;
    mu->smdAlt = (unsigned char *)mu->smdNormal + smdSectorSize;

    if (mu2Name[mode] != 0) {
        strcpy(fileName, sndMuNameBase);
        strcat(fileName, mu2Name[mode]);
        strcat(fileName, sndMuNameExt1);
        dataFileLoadNB(fileName, mu->smdAlt);
        altFileSize = xglCdGetFileSize(fileName);
        altSectorSize = altFileSize + 0x7FF;
        if (altSectorSize < 0) {
            altSectorSize = altFileSize + 0xFFE;
        }
        altSectorSize >>= 0xB;
        altSectorSize <<= 0xB;
        mu->smdAltSize = altSectorSize;
    } else {
        mu->smdAltSize = 0;
    }
    mu->swd = (unsigned char *)mu->smdAlt + mu->smdAltSize;

    strcpy(fileName, sndMuNameBase);
    strcat(fileName, muName[mode]);
    strcat(fileName, sndMuNameExt2);
    dataFileLoadNB(fileName, mu->swd);
    swdFileSize = xglCdGetFileSize(fileName);
    mu->mode = mode;
    swdSectorSize = swdFileSize + 0x7FF;
    if (swdSectorSize < 0) {
        swdSectorSize = swdFileSize + 0xFFE;
    }
    swdSectorSize >>= 0xB;
    swdSectorSize <<= 0xB;
    mu->swdSize = swdSectorSize;
    return 1;
}

int sndMuTrans(int mode)
{
    if (mode == 0) {
        xglSoundSendSmd(sndMuDat.smdNormal);
        xglSoundSendSwd(sndMuDat.swd, -1);
    } else {
        xglSoundSendSmd(sndMuDat.smdAlt);
    }
    return 1;
}

int sndMuPlay(void)
{
    xglSoundSequenceNormal(SND_MU_VOLUME_MAX);
    return 1;
}

int sndMuStop(void)
{
    xglSoundSequenceFadeOut(SND_MU_FADE_TIME);
    return 1;
}

void sndMuFadeIn(void)
{
    xglSoundSequenceNormal3(0, SND_MU_VOLUME_MAX, SND_MU_FADE_TIME);
}

extern const char D_00A4E860[];
void sndMuTransPlayObj(ObjectTask *task);

void sndMuTransPlay(int mode)
{
    if (sndMuTrans(mode) != 0) {
        printf(D_00A4E860, mode);
        objEntryPure(sndMuTransPlayObj);
    }
}

void sndMuTransPlayObj(ObjectTask *task)
{
    if (SsdSpuDmaCompleted(0) != 0) {
        return;
    }
    sndMuPlay();
    objRemovePure(task);
}

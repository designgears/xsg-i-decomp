#include "common.h"
#include "shared.h"
#include "srs.h"

extern char _loadEsdData[]; /* 0xC800-byte effect-name table. */ /* 0xC800-byte table (config/symbols/main.txt) */

extern signed char _srsEffect2IndexTbl[];

int srsGetEffect2Idx(int effectNo)
{
    int r;

    if ((u32) (effectNo - 0x259) < 0x75U) {
        r = _srsEffect2IndexTbl[effectNo - 0x259];
    } else if ((u32) (effectNo - 0xA28) < 0xCU) {
        r = effectNo - 0xA28;
    } else if ((u32) (effectNo - 0xA34) < 9U) {
        r = effectNo - 0xA34;
    } else if ((u32) (effectNo - 0xA5B) < 0xAU) {
        r = effectNo - 0xA5B;
    } else if ((u32) (effectNo - 0xA42) < 4U) {
        r = effectNo - 0xA42;
    } else if ((u32) (effectNo - 0xA46) < 7U) {
        r = effectNo - 0xA46;
    } else {
        r = ((u32) (effectNo - 0xB54) < 0x63U) ? 0x64 : 0;
    }
    return r;
}

extern int srsGetEffect2Idx(int effectNo);

char *srsGetEsdData(int index)
{
    int idx;

    if ((unsigned int) index >= 0xc00) {
        return 0;
    }
    idx = srsGetEffect2Idx(index);
    return _loadEsdData + (index - idx) * 0x10;
}

char *srsGetEsdData2(int effectNo)
{
    if ((unsigned int) effectNo >= 0xc00) {
        return 0;
    }
    return _loadEsdData + effectNo * 0x10;
}

char *sefGetEffectName(int effectNo)
{
    if ((unsigned int) effectNo >= 0xc00) {
        return 0;
    }
    return _loadEsdData + effectNo * 0x10;
}

typedef struct SrsChrEfEntry {
    short chrNo;
    short min0;
    short max0;
    short min1;
    short max1;
    short min2;
    short max2;
    short min3;
    short max3;
} SrsChrEfEntry;
typedef struct SrsChrEf2Entry {
    short chrNo;
    short min;
    short max;
} SrsChrEf2Entry;
extern SrsChrEfEntry _srsChrEfTbl[];
extern SrsChrEf2Entry _srsChrEfTbl2[];
extern char *_loadCfCommonData;
extern char *_loadComboData[];

char *srsAnalyzeEftNo(int effectNo, int *chrEftNo, int *effectCategory)
{
    int i;
    SrsChrEf2Entry *p;
    short *q;
    char *ret;

    *chrEftNo = 0;
    *effectCategory = 0xE;
    if ((u32) (effectNo - 1) >= 0xBFF) {
        return 0;
    }
    if (effectNo >= 0xBE2) {
        *effectCategory = 0xE;
        return 0;
    }
    if ((u32) (effectNo - 0xA28) < 0x3E) {
        *chrEftNo = 0;
        *effectCategory = 0;
        return 0;
    }
    if ((u32) (effectNo - 0xAF0) < 0xC8) {
        *effectCategory = 2;
        *chrEftNo = 0x11;
        return 0;
    }
    if ((u32) (effectNo - 0x258) < 0x190) {
        ret = _loadCfCommonData;
        *effectCategory = 0xF;
        return ret;
    }
    if ((u32) (effectNo - 0x898) < 0xA) {
        *chrEftNo = 2;
        *effectCategory = 0xE;
        return 0;
    }
    if ((u32) (effectNo - 0x8A2) < 8) {
        *chrEftNo = 5;
        *effectCategory = 0xE;
        return 0;
    }
    if ((u32) (effectNo - 0x3E8) < 0x3E8) {
        *chrEftNo = 0;
        *effectCategory = 0x10;
        return 0;
    }
    if ((u32) (effectNo - 0x7D0) < 0xDB) {
        *chrEftNo = 0;
        *effectCategory = 0xE;
        return 0;
    }
    for (i = 0; i < 12; i++) {
        if (_srsChrEfTbl[i].min0 <= effectNo && effectNo <= _srsChrEfTbl[i].max0) {
            *chrEftNo = _srsChrEfTbl[i].chrNo;
            *effectCategory = 2;
            return _loadComboData[*chrEftNo];
        }
        if (_srsChrEfTbl[i].min1 <= effectNo && effectNo <= _srsChrEfTbl[i].max1) {
            *chrEftNo = _srsChrEfTbl[i].chrNo;
            *effectCategory = 0xE;
            return 0;
        }
        if (_srsChrEfTbl[i].min2 <= effectNo && effectNo <= _srsChrEfTbl[i].max2) {
            *chrEftNo = _srsChrEfTbl[i].chrNo;
            *effectCategory = 0xE;
            return 0;
        }
        if (_srsChrEfTbl[i].min3 <= effectNo && effectNo <= _srsChrEfTbl[i].max3) {
            *chrEftNo = _srsChrEfTbl[i].chrNo;
            *effectCategory = 2;
            return _loadComboData[*chrEftNo];
        }
    }
    p = _srsChrEfTbl2;
    for (i = 0; i < 0xA1; i++) {
        q = &p[i].max;
        if (q[-1] <= effectNo && effectNo <= q[0]) {
            *chrEftNo = q[-2];
            if (q[-2] < 0x97) {
                *effectCategory = 0xB;
                return _loadComboData[*chrEftNo];
            }
            if (effectNo >= 0xA78) {
                *effectCategory = 0xB;
                return _loadComboData[*chrEftNo];
            }
            *effectCategory = 0xE;
            return 0;
        }
    }
    return 0;
}

/*
 * One _srsChrEfTbl3 row (6 bytes; the 0xD8-byte symbol holds 36 rows).
 * srsGetBossWaitEftNo scans effectNo and returns the matching bossWaitEftNo.
 */
typedef struct SrsChrEfTbl3Entry {
    short effectNo;
    short bossWaitEftNo;
    unsigned char unmodeled_04[2];
} SrsChrEfTbl3Entry;

extern SrsChrEfTbl3Entry _srsChrEfTbl3[];

short srsGetBossWaitEftNo(int effectNo)
{
    int i;

    if ((unsigned int) (effectNo - 151) < 36) {
        for (i = 0; i < 36; i++) {
            if (effectNo == _srsChrEfTbl3[i].effectNo) {
                return _srsChrEfTbl3[i].bossWaitEftNo;
            }
        }
    }
    return 0;
}

/*
 * The original srs prefix is retained; its historical expansion is not
 * established by the available evidence.  This partial translation unit
 * owns only the two GLOBAL accessors.  srsLoadMode remains the original
 * external .sdata word rather than a new definition here.
 */
void srsSetLoadMode(int load_mode)
{
    srsLoadMode = load_mode;
}

int srsGetLoadMode(void)
{
    return srsLoadMode;
}

extern unsigned char srsViewPath[];
extern unsigned char *strcpy(unsigned char *destination, const unsigned char *source);

void srsSetViewPath(unsigned char *path)
{
    strcpy(srsViewPath, path);
}

extern char *strcat(char *destination, const char *source);
typedef struct { char s[15]; } SrsPath15;
typedef struct { char s[23]; } SrsPath23;
typedef struct { char s[7]; } SrsPath7;
extern SrsPath15 D_004CC680;
extern SrsPath23 D_004CC690;
extern char D_004DBB18[];

static int srsMakeFileName(char *path, char *name)
{
    if (name == 0) {
        path[0] = 0;
        return 0;
    }
    if (srsLoadMode == 0) {
        *(SrsPath15 *)path = D_004CC680;
        strcat(path, name);
    } else if (srsLoadMode == 1) {
        *(SrsPath23 *)path = D_004CC690;
        strcat(path, (char *)srsViewPath);
        strcat(path, name);
    } else if (srsLoadMode == 2) {
        *(SrsPath7 *)path = *(SrsPath7 *)D_004DBB18;
        strcat(path, (char *)srsViewPath);
        strcat(path, name);
    }
    return 1;
}

void srsChangeSeparator(char *path)
{
    int index;

    if (path != 0) {
        for (index = 0; path[index] != '\0'; index++) {
            if (path[index] == '/')
                path[index] = '\\';
        }
    }
}

extern int xglCdGetFileSize(const char *name);
extern int sceOpen(const char *path, int flags, ...);
extern int sceLseek(int descriptor, int offset, int whence);
extern int sceClose(int descriptor);

static int srsGetFileLen(char *name)
{
    char path[256];
    int descriptor;
    int size;

    if (srsMakeFileName(path, name) == 0) {
        return -1;
    }
    if (srsLoadMode == 0) {
        srsChangeSeparator(path);
        size = xglCdGetFileSize(path);
        if (size <= 0) {
            return -1;
        }
    } else if (srsLoadMode == 1) {
        descriptor = sceOpen(path, 1);
        if (descriptor < 0) {
            return -1;
        }
        size = sceLseek(descriptor, 0, 2);
        sceLseek(descriptor, 0, 0);
        sceClose(descriptor);
    } else {
        return -1;
    }
    return size;
}

extern int xglCdGetFileSize(const char *name);
extern int sceOpen(const char *path, int flags, ...);
extern int sceLseek(int fd, int offset, int whence);
extern int sceRead(int fd, void *buf, int size);
extern int sceClose(int fd);
extern void sresDataMapping(void);
extern void srsCdReadCallback(int code);
extern int func_00A103C8(char *archive, void *buffer);

static int fileLoad(void *buffer, const char *name, int mode)
{
  char path[256];
  int ret = 0;
  int nread;
  int fd;
  if (srsMakeFileName(path, name) == 0)
  {
    return -1;
  }
  if (srsLoadMode == 0)
  {
    srsChangeSeparator(path);
    ret = xglCdGetFileSize(path);
    if (ret <= 0)
    {
      return -1;
    }
    _nRead++;
    nread = xglCdReadFile(path, buffer, mode, (int) srsCdReadCallback);
    if (nread > 0)
    {
      if (mode == 0)
      {
        ret = nread;
        _nRead--;
        if (_nRead < 0)
        {
          _nRead = 0;
        }
        else
        {
        }
      }
    }
  }
  else
    if (srsLoadMode == 1)
  {
    fd = sceOpen(path, 1);
    if (fd < 0)
    {
      return -1;
    }
    ret = sceLseek(fd, 0, 2);
    sceLseek(fd, 0, 0);
    ret = sceRead(fd, buffer, ret);
    sceClose(fd);
    sresDataMapping();
  }
  else
    if (srsLoadMode == 2)
  {
    ret = func_00A103C8(&path[5], buffer);
    if (ret > 0)
    {
      sresDataMapping();
    }
  }
  return ret;
}

char *srsGetEffectData(int index)
{
    return srsGetEsdData(index);
}

/*
 * 0x30C-byte table of 195 pointer-sized slots (config/symbols/main.txt).
 */
extern char *_loadComboData[];

char **srsGetComboData(int index)
{
    if (index >= 195) {
        return 0;
    }
    return (_loadComboData[index] != 0) ? &_loadComboData[index] : 0;
}

extern char msg_0_00795120[];
extern char D_004CC6A8[]; /* "esd/%s%s" */
extern char D_004DBB20[]; /* ".esd" */

char *srsGetEffectName(int effectNo)
{
    char *name;

    name = srsGetEffectData(effectNo);
    if (name == 0) {
        return name;
    }
    if (*name == 0) {
        return 0;
    }
    sprintf(msg_0_00795120, D_004CC6A8, name, D_004DBB20);
    return msg_0_00795120;
}

/*
 * One _weaponTbl row (8 bytes; the 0x168-byte symbol holds 45 rows).
 * srsEftNo2WeaponID reads weaponID at the row start; srsEftNoWeaponEffectID
 * reads weaponEffectID right after it.
 */
typedef struct WeaponTblEntry {
    short weaponID;
    short weaponEffectID;
    unsigned char unmodeled_04[4];
} WeaponTblEntry;

extern WeaponTblEntry _weaponTbl[];

int srsGetWeaponEffectIdx(int id)
{
    int i;

    if ((unsigned)(id - 0xB54) < 0x63) {
        id -= 0x64;
    }
    if (id > 0) {
        for (i = 0; i < 45; i++) {
            if (id == ((short *)&_weaponTbl[i])[1] || id == ((short *)&_weaponTbl[i])[2] || id == ((short *)&_weaponTbl[i])[3]) {
                return i;
            }
        }
    }
    return -1;
}

extern int srsGetWeaponEffectIdx();

short srsEftNoWeaponEffectID(void)
{
    int index;
    short weaponEffectID;

    index = ((int (*)())srsGetWeaponEffectIdx)();
    weaponEffectID = 0;
    if (index >= 0) {
        weaponEffectID = _weaponTbl[index].weaponEffectID;
    }
    return weaponEffectID;
}

short srsWeapon2EffectID(int weaponID)
{
    int i;

    if (weaponID > 0) {
        for (i = 0; i < 45; i++) {
            if (weaponID == _weaponTbl[i].weaponID) {
                return _weaponTbl[i].weaponEffectID;
            }
        }
    }
    return -1;
}

short srsEftNo2WeaponID(void)
{
    int index;
    short weaponID;

    index = ((int (*)())srsGetWeaponEffectIdx)();
    weaponID = 0;
    if (index >= 0) {
        weaponID = _weaponTbl[index].weaponID;
    }
    return weaponID;
}

extern char msg_1_007951A0[];
extern char D_004CC6B8[]; /* "esp/%s%s" */
extern char D_004DBB28[]; /* ".esp" */

char *srsGetEffectName2(int effectNo)
{
    char *name;

    name = srsGetEffectData(effectNo);
    if (name == 0) {
        return name;
    }
    sprintf(msg_1_007951A0, D_004CC6B8, name, D_004DBB28);
    return msg_1_007951A0;
}

extern char msg_2_00795220[];

static char *srsGetComboName(int index)
{
    char **entry;
    char *value;

    entry = srsGetComboData(index);
    if (entry == 0) {
        return 0;
    }
    value = *entry;
    if (value == 0) {
        return 0;
    }
    sprintf(msg_2_00795220, D_004CC6B8, value, D_004DBB28);
    return msg_2_00795220;
}

/*
 * The ranges are disjoint, evidenced value windows over effectNo; no source
 * for their meaning is available beyond the compiled comparisons.
 */
int srsGetEffectType(int effectNo)
{
    int effectType;

    effectType = 0;
    if ((u32) (effectNo - 0xA28) >= 0x3E) {
        effectType = 2;
        if ((u32) (effectNo - 0x9C4) >= 0x64 && effectNo >= 0x64) {
            effectType = 0xE;
            if ((u32) (effectNo - 0x7D0) >= 0xDB) {
                effectType = 0xB;
                if ((u32) (effectNo - 0xF0) >= 0x9D) {
                    effectType = 0xE;
                    if ((u32) (effectNo - 0x8FC) >= 0xB6) {
                        effectType = 2;
                        if ((u32) (effectNo - 0xAF0) >= 0xC8) {
                            effectType = ((u32) (effectNo - 0x258) < 0x190) ? 0xF : 0xE;
                        }
                    }
                }
            }
        }
    }
    return effectType;
}

void srsInitCdRead(void)
{
    _nRead = 0;
}

int srsLeaveCdRead(void)
{
    return _nRead;
}

extern void sresDataMapping(void);

/*
 * The CD reader reports each finished request to this callback with the
 * request's own code; only three of the seven codes the dispatch covers
 * touch the outstanding-read count srsInitCdRead resets, and the count
 * reaching zero on code 4 maps the data that was just read.
 */
void srsCdReadCallback(int code)
{
    switch (code) {
    case 4:
        _nRead--;
        if (_nRead < 0) {
            _nRead = 0;
        }
        if (_nRead == 0) {
            sresDataMapping();
        }
        break;
    case -1:
        _nRead--;
        if (_nRead < 0) {
            _nRead = 0;
        }
        break;
    case -2: {
        int remaining;

        remaining = _nRead - 1;
        if (remaining < 0) {
            _nRead = 0;
        } else {
            _nRead = remaining;
        }
        break;
    }
    case 0:
    case 1:
    case 2:
    case 3:
        break;
    }
}

int srsLoadEffectData(void *buffer, int effectNo)
{
    char *name;

    name = srsGetEffectName(effectNo);
    if (name == 0) {
        return -1;
    }
    return fileLoad(buffer, name, 1);
}

extern unsigned char _srsMemRes[];

void sresInitMemoryRes(void)
{
    memset(_srsMemRes, 0, 0x1A0);
}

/*
 * _srsMemRes's leading view needed by the two loaders below: the same +0x00
 * image pointer sresFreeMemoryRes owns, plus the +0x3C reloader-buffer
 * pointer sresLoadCfMemory allocates and svAddImageMapper maps.
 */
typedef struct SrsMemResEarly {
    void *image; /* +0x00 */
    unsigned char unmodeled_04[0x38];
    void *cfImage; /* +0x3C */
} SrsMemResEarly;

extern void *smAlloc(int size);

/*
 * svAddImageMapper takes the address of a chunk list; svAnalyzeChunk walks it
 * from its first entry, which is the only one these two loaders fill.
 */
extern void svAddImageMapper(int type, int width, void *chunk, int height);
extern char D_004CC6F0[]; /* "esp/default.esp" */
extern char D_004CC700[]; /* "seffect.esp" */
extern char D_004CC710[]; /* "esp/cf_def.esp" */

void sresLoadCommonMemory(void)
{
    SrsMemResEarly *memRes;
    int size;
    void *chunk[8];

    memRes = (SrsMemResEarly *) _srsMemRes;
    if (memRes->image == 0) {
        memRes->image = smAlloc(0x25800);
        if (memRes->image != 0) {
            size = fileLoad(memRes->image, D_004CC6F0, 0);
            if (size >= 0) {
                if (size <= 0x25800) {
                    chunk[0] = memRes->image;
                    svAddImageMapper(0, 0, chunk, 3000);
                }
            }
        }
        memset(_loadEsdData, 0, 0xC800);
        fileLoad(_loadEsdData, D_004CC700, 0);
    }
}

void sresLoadCfMemory(void)
{
    SrsMemResEarly *memRes;
    int size;
    void *chunk[8];

    memRes = (SrsMemResEarly *) _srsMemRes;
    if (memRes->cfImage == 0) {
        memRes->cfImage = smAlloc(0x3E000);
        if (memRes->cfImage != 0) {
            size = fileLoad(memRes->cfImage, D_004CC710, 0);
            if (size >= 0) {
                chunk[0] = memRes->cfImage;
                svAddImageMapper(15, 0, chunk, 3500);
            }
        }
    }
}

extern void smFree(void *block);
extern void svDeleteImageMapper(int type);

/*
 * The reloader view of the same _srsMemRes record.  Its 0x1A0 bytes hold four
 * parallel blocks: one buffer pointer per resource, then a halfword per
 * resource that is set while the buffer still has to be handed to the image
 * mapper, then the effect number the buffer was loaded for, which the release
 * path sets back to -1, and last a second pointer per effect slot.  The mapper
 * type is fixed per block: the three weapon rows at +0x08 own three types each
 * from 2 up, the combo pointers at +0x2C own 11 to 13, the battle image 14,
 * the cf image 15 and the effect slots at +0x40 own 16 up.  A reloader slot
 * number below 3 names a weapon row and 3 or more a combo pointer.
 */
typedef struct SrsMemResMap {
    void *image; /* +0x000 */
    void *effectImage; /* +0x004 */
    void *weaponData[3][3]; /* +0x008 */
    void *comboData[3]; /* +0x02C */
    void *battleImage; /* +0x038 */
    void *cfImage; /* +0x03C */
    void *effectData[24]; /* +0x040 */
    short imagePending; /* +0x0A0 */
    short effectImagePending; /* +0x0A2 */
    short weaponPending[3][3]; /* +0x0A4 */
    short comboPending[3]; /* +0x0B6 */
    short battleImagePending; /* +0x0BC */
    short cfImagePending; /* +0x0BE */
    short effectPending[24]; /* +0x0C0 */
    short imageNo; /* +0x0F0 */
    short effectImageNo; /* +0x0F2 */
    short weaponNo[3][3]; /* +0x0F4 */
    short comboNo[3]; /* +0x106 */
    short battleImageNo; /* +0x10C */
    short cfImageNo; /* +0x10E */
    short effectNo[24]; /* +0x110 */
    void *effectExtra[24]; /* +0x140 */
} SrsMemResMap;

void sresFreeReloaderMemoryNo(int no)
{
    if (no < 3) {
        if (((SrsMemResMap *) _srsMemRes)->weaponData[no][0] != 0) {
            svDeleteImageMapper(no * 3 + 2);
            smFree(((SrsMemResMap *) _srsMemRes)->weaponData[no][0]);
            ((SrsMemResMap *) _srsMemRes)->weaponData[no][0] = 0;
            ((SrsMemResMap *) _srsMemRes)->weaponData[no][1] = 0;
            ((SrsMemResMap *) _srsMemRes)->weaponData[no][2] = 0;
        }
    } else {
        if (((SrsMemResMap *) _srsMemRes)->comboData[no - 3] != 0) {
            svDeleteImageMapper(no + 8);
            smFree(((SrsMemResMap *) _srsMemRes)->comboData[no - 3]);
            ((SrsMemResMap *) _srsMemRes)->comboData[no - 3] = 0;
        }
    }
}

extern void smFree(void *block);
extern void svDeleteImageMapper(int type);

/*
 * The reloader view of the same _srsMemRes record.  Its 0x1A0 bytes hold four
 * parallel blocks: one buffer pointer per resource, then a halfword per
 * resource that is set when the buffer still has to be handed to the image
 * mapper, then the effect number the buffer was loaded for, which the release
 * path sets back to -1, and last a second pointer per effect slot that is
 * released together with the first.  The mapper type is fixed per block: the
 * three weapon rows at +0x08 own three types each from 2 up, the combo
 * pointers at +0x2C own 11 to 13, the battle image 14, the cf image 15 and
 * the effect slots at +0x40 own 16 up.
 */
void sresFreeReloaderMemory(int freeCf)
{
    int i;
    int type;

    if (((SrsMemResMap *) _srsMemRes)->battleImage != 0) {
        svDeleteImageMapper(14);
        smFree(((SrsMemResMap *) _srsMemRes)->battleImage);
        ((SrsMemResMap *) _srsMemRes)->battleImage = 0;
    }
    for (i = 0; i < 3; i++) {
        if (((SrsMemResMap *) _srsMemRes)->comboData[i] != 0) {
            svDeleteImageMapper(i + 11);
            smFree(((SrsMemResMap *) _srsMemRes)->comboData[i]);
            ((SrsMemResMap *) _srsMemRes)->comboData[i] = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        if (((SrsMemResMap *) _srsMemRes)->weaponData[i][0] != 0) {
            svDeleteImageMapper(i * 3 + 2);
            svDeleteImageMapper(i * 3 + 3);
            svDeleteImageMapper(i * 3 + 4);
            smFree(((SrsMemResMap *) _srsMemRes)->weaponData[i][0]);
            ((SrsMemResMap *) _srsMemRes)->weaponData[i][0] = 0;
            ((SrsMemResMap *) _srsMemRes)->weaponData[i][1] = 0;
            ((SrsMemResMap *) _srsMemRes)->weaponData[i][2] = 0;
            ((SrsMemResMap *) _srsMemRes)->weaponNo[i][0] = -1;
            ((SrsMemResMap *) _srsMemRes)->weaponNo[i][1] = -1;
            ((SrsMemResMap *) _srsMemRes)->weaponNo[i][2] = -1;
        }
    }
    for (i = 0; i < 24; i++) {
        type = i + 16;
        if (((SrsMemResMap *) _srsMemRes)->effectData[i] != 0) {
            svDeleteImageMapper(type);
            smFree(((SrsMemResMap *) _srsMemRes)->effectData[i]);
            ((SrsMemResMap *) _srsMemRes)->effectData[i] = 0;
            ((SrsMemResMap *) _srsMemRes)->effectExtra[i] = 0;
            ((SrsMemResMap *) _srsMemRes)->effectNo[i] = -1;
        }
    }
    if (freeCf != 0) {
        if (((SrsMemResMap *) _srsMemRes)->cfImage != 0) {
            svDeleteImageMapper(15);
            smFree(((SrsMemResMap *) _srsMemRes)->cfImage);
            ((SrsMemResMap *) _srsMemRes)->cfImage = 0;
        }
    }
}

/*
 * _srsMemRes's own additive view: only the leading pointer sresFreeMemoryRes
 * tests and clears is modeled, the rest of the 0x1A0-byte record (zeroed
 * whole by sresInitMemoryRes) stays an explicit unmodeled span.
 */
typedef struct SrsMemRes {
    void *image; /* +0x00 */
    unsigned char unmodeled_04[0x19c];
} SrsMemRes;

extern void smFree(void *block);
extern void sresFreeReloaderMemory(int reload_bgm);
extern void svDeleteImageMapper(int type);

void sresFreeMemoryRes(void)
{
    SrsMemRes *memRes;

    sresFreeReloaderMemory(1);
    memRes = (SrsMemRes *) _srsMemRes;
    if (memRes->image != 0) {
        svDeleteImageMapper(0);
        smFree(memRes->image);
        memRes->image = 0;
    }
}

/*
 * The reloader view of the same _srsMemRes record.  Its 0x1A0 bytes hold four
 * parallel blocks: one buffer pointer per resource, then a halfword per
 * resource that is set while the buffer still has to be handed to the image
 * mapper, then the effect number the buffer was loaded for, which the release
 * path sets back to -1, and last a second pointer per effect slot.  The mapper
 * type is fixed per block: the three weapon rows at +0x08 own three types each
 * from 2 up, the combo pointers at +0x2C own 11 to 13, the battle image 14,
 * the cf image 15 and the effect slots at +0x40 own 16 up.  A reloader slot
 * number below 3 names a weapon row and 3 or more a combo pointer.
 */
extern void svAddImageMapper(int type, int width, void *chunk, int height);

void sresDataMapping(void)
{
    void *chunk[8];
    int i;
    int j;
    int pending;
    int type;

    if (((SrsMemResMap *) _srsMemRes)->image != 0) {
        if (((SrsMemResMap *) _srsMemRes)->imagePending != 0) {
            chunk[0] = ((SrsMemResMap *) _srsMemRes)->image;
            svAddImageMapper(0, 0, chunk, 3000);
            ((SrsMemResMap *) _srsMemRes)->imagePending = 0;
        }
    }
    if (((SrsMemResMap *) _srsMemRes)->effectImage != 0) {
        if (((SrsMemResMap *) _srsMemRes)->effectImagePending != 0) {
            chunk[0] = ((SrsMemResMap *) _srsMemRes)->effectImage;
            svAddImageMapper(1, 0, chunk, 3100);
            ((SrsMemResMap *) _srsMemRes)->effectImagePending = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            if (((SrsMemResMap *) _srsMemRes)->weaponData[i][j] != 0) {
                pending = ((SrsMemResMap *) _srsMemRes)->weaponPending[i][j];
                if (pending != 0) {
                    chunk[0] = ((SrsMemResMap *) _srsMemRes)->weaponData[i][j];
                    svAddImageMapper(i * 3 + j + 2, pending, chunk, 2000);
                    ((SrsMemResMap *) _srsMemRes)->weaponPending[i][j] = 0;
                }
            }
        }
    }
    for (i = 0; i < 3; i++) {
        type = i + 11;
        if (((SrsMemResMap *) _srsMemRes)->comboData[i] != 0) {
            pending = ((SrsMemResMap *) _srsMemRes)->comboPending[i];
            if (pending != 0) {
                chunk[0] = ((SrsMemResMap *) _srsMemRes)->comboData[i];
                svAddImageMapper(type, pending, chunk, 2100);
                ((SrsMemResMap *) _srsMemRes)->comboPending[i] = 0;
            }
        }
    }
    if (((SrsMemResMap *) _srsMemRes)->battleImage != 0) {
        if (((SrsMemResMap *) _srsMemRes)->battleImagePending != 0) {
            chunk[0] = ((SrsMemResMap *) _srsMemRes)->battleImage;
            svAddImageMapper(14, 0, chunk, 3400);
            ((SrsMemResMap *) _srsMemRes)->battleImagePending = 0;
        }
    }
    if (((SrsMemResMap *) _srsMemRes)->cfImage != 0) {
        if (((SrsMemResMap *) _srsMemRes)->cfImagePending != 0) {
            chunk[0] = ((SrsMemResMap *) _srsMemRes)->cfImage;
            svAddImageMapper(15, 0, chunk, 3500);
            ((SrsMemResMap *) _srsMemRes)->cfImagePending = 0;
        }
    }
    for (i = 0; i < 24; i++) {
        type = i + 16;
        if (((SrsMemResMap *) _srsMemRes)->effectData[i] != 0) {
            pending = ((SrsMemResMap *) _srsMemRes)->effectPending[i];
            if (pending != 0) {
                chunk[0] = ((SrsMemResMap *) _srsMemRes)->effectData[i];
                svAddImageMapper(type, pending, chunk, 3600);
                ((SrsMemResMap *) _srsMemRes)->effectPending[i] = 0;
            }
        }
    }
}

typedef struct SrsBattlePrm {
    short charNo[3]; /* +0x00 */
    short enemyNo[3]; /* +0x06 */
    unsigned char unmodeled_0C[6];
    short weaponEffect[3][3]; /* +0x12 */
    short charReload[3]; /* +0x24 */
    short enemyReload[3]; /* +0x2A */
} SrsBattlePrm;
typedef struct SrsResFlat {
    void *image; /* +0x000 */
    void *effectImage; /* +0x004 */
    void *weaponData[3][3]; /* +0x008 */
    void *comboData[3]; /* +0x02C */
    void *battleImage; /* +0x038 */
    void *cfImage; /* +0x03C */
    void *effectData[24]; /* +0x040 */
    short pending[40]; /* +0x0A0: image 0, effect image 1, weapon 2..10, combo 11..13 */
    short no[40]; /* +0x0F0: same indexing */
    void *effectExtra[24]; /* +0x140 */
} SrsResFlat;

void sresLoadBattleData(SrsBattlePrm *battlePrm)
{
    int i;
    int j;
    char *name;
    int effectNo;

    for (i = 0; i < 24; i++) {
        if (((SrsResFlat *) _srsMemRes)->effectData[i] != 0) {
            smFree(((SrsResFlat *) _srsMemRes)->effectData[i]);
            ((SrsResFlat *) _srsMemRes)->effectData[i] = 0;
            ((SrsResFlat *) _srsMemRes)->effectExtra[i] = 0;
        }
    }
    if (((SrsResFlat *) _srsMemRes)->cfImage != 0) {
        smFree(((SrsResFlat *) _srsMemRes)->cfImage);
        ((SrsResFlat *) _srsMemRes)->cfImage = 0;
    }
    for (i = 0; i < 3; i++) {
        if (battlePrm->charNo[i] > 0 && battlePrm->charReload[i] != 0) {
            battlePrm->charReload[i] = 0;
            if (battlePrm->charNo[i] < 151) {
                if (battlePrm->charNo[i] >= 17 && battlePrm->charNo[i] < 33) {
                    if (((SrsResFlat *) _srsMemRes)->weaponData[i][0] == 0) {
                        ((SrsResFlat *) _srsMemRes)->weaponData[i][0] = smAlloc(0x12800);
                    }
                    if (((SrsResFlat *) _srsMemRes)->weaponData[i][0] != 0) {
                        ((SrsResFlat *) _srsMemRes)->weaponData[i][1] = (char *) ((SrsResFlat *) _srsMemRes)->weaponData[i][0] + 0x6000;
                        ((SrsResFlat *) _srsMemRes)->weaponData[i][2] = (char *) ((SrsResFlat *) _srsMemRes)->weaponData[i][0] + 0xC000;
                    }
                    ((SrsResFlat *) _srsMemRes)->no[2 + i * 3 + 0] = -1;
                    ((SrsResFlat *) _srsMemRes)->no[2 + i * 3 + 1] = -1;
                    ((SrsResFlat *) _srsMemRes)->no[2 + i * 3 + 2] = -1;
                    for (j = 0; j < 3; j++) {
                        if (battlePrm->weaponEffect[i][j] > 0) {
                            effectNo = srsWeapon2EffectID(battlePrm->weaponEffect[i][j]);
                            name = srsGetEffectName2(effectNo);
                            if (name != 0) {
                                ((SrsResFlat *) _srsMemRes)->pending[2 + i * 3 + j] = battlePrm->charNo[i];
                                ((SrsResFlat *) _srsMemRes)->no[2 + i * 3 + j] = effectNo;
                                if (fileLoad(((SrsResFlat *) _srsMemRes)->weaponData[i][j], name, 1) < 0) {
                                    ((SrsResFlat *) _srsMemRes)->pending[2 + i * 3 + j] = 0;
                                }
                            }
                        }
                    }
                } else {
                    name = srsGetComboName(battlePrm->charNo[i]);
                    if (((SrsResFlat *) _srsMemRes)->weaponData[i][0] == 0) {
                        ((SrsResFlat *) _srsMemRes)->weaponData[i][0] = smAlloc(0x12800);
                    }
                    ((SrsResFlat *) _srsMemRes)->weaponData[i][1] = 0;
                    ((SrsResFlat *) _srsMemRes)->weaponData[i][2] = 0;
                    ((SrsResFlat *) _srsMemRes)->pending[2 + i * 3] = battlePrm->charNo[i];
                    if (fileLoad(((SrsResFlat *) _srsMemRes)->weaponData[i][0], name, 1) < 0) {
                        ((SrsResFlat *) _srsMemRes)->pending[2 + i * 3] = 0;
                    }
                }
            }
        }
    }
    for (i = 0; i < 3; i++) {
        if (battlePrm->enemyNo[i] > 0 && battlePrm->enemyReload[i] != 0) {
            battlePrm->enemyReload[i] = 0;
            if (battlePrm->enemyNo[i] < 17 || battlePrm->enemyNo[i] >= 33) {
                name = srsGetComboName(battlePrm->enemyNo[i]);
                if (((SrsResFlat *) _srsMemRes)->comboData[i] == 0) {
                    ((SrsResFlat *) _srsMemRes)->comboData[i] = smAlloc(0x12000);
                }
                ((SrsResFlat *) _srsMemRes)->pending[11 + i] = battlePrm->enemyNo[i];
                if (fileLoad(((SrsResFlat *) _srsMemRes)->comboData[i], name, 1) <= 0) {
                    ((SrsResFlat *) _srsMemRes)->pending[11 + i] = 0;
                }
            }
        }
    }
}

int srsFileLoad(void *buffer, const char *name, int mode)
{
    return fileLoad(buffer, name, mode);
}

extern char D_004CC720[];
extern char D_004CC748[];
extern char D_004CC768[];
extern void tracePrint();

int srsFileLoadCf(int cfId, int effectNo)
{
    int charNo;
    int effectType;
    char *name;
    void *buffer;
    int i;
    int size;

    srsAnalyzeEftNo(effectNo, &charNo, &effectType);
    if (effectType == 15) {
        return 15;
    }
    if ((unsigned int) (effectType - 16) < 24) {
        name = srsGetEffectName(effectNo);
        if (name == 0) {
            return 0;
        }
        for (i = 0; i < 24; i++) {
            if (((SrsResFlat *) _srsMemRes)->effectData[i] != 0 && ((SrsResFlat *) _srsMemRes)->no[16 + i] == effectNo) {
                return 0;
            }
        }
        for (i = 0; i < 24; i++) {
            if (((SrsResFlat *) _srsMemRes)->effectData[i] == 0) {
                size = srsGetFileLen(name);
                if (size <= 0) {
                    return -1;
                }
                buffer = smAlloc(size);
                ((SrsResFlat *) _srsMemRes)->effectData[i] = buffer;
                if (buffer == 0) {
                    tracePrint(D_004CC720);
                    return -1;
                }
                size = srsFileLoad(buffer, name, 0);
                if (size > 0) {
                    ((SrsResFlat *) _srsMemRes)->pending[16 + i] = 1;
                    ((SrsResFlat *) _srsMemRes)->no[16 + i] = effectNo;
                    sresDataMapping();
                    return size;
                }
                return -1;
            }
        }
    } else {
        tracePrint(D_004CC748, effectType, effectNo);
    }
    tracePrint(D_004CC768);
    return -1;
}

extern void *memcpy(void *dst, const void *src, unsigned int n);

int srsMemoryLoadCf(void *data, int effectNo, int size)
{
    int charNo;
    int effectType;
    void *buffer;
    int i;

    srsAnalyzeEftNo(effectNo, &charNo, &effectType);
    if (effectType == 15) {
        return 15;
    }
    if ((unsigned int) (effectType - 16) < 24) {
        for (i = 0; i < 24; i++) {
            if (((SrsResFlat *) _srsMemRes)->effectData[i] != 0 && ((SrsResFlat *) _srsMemRes)->no[16 + i] == effectNo) {
                return 0;
            }
        }
        for (i = 0; i < 24; i++) {
            if (((SrsResFlat *) _srsMemRes)->effectData[i] == 0) {
                if (size <= 0) {
                    return -1;
                }
                buffer = smAlloc(size);
                ((SrsResFlat *) _srsMemRes)->effectData[i] = buffer;
                if (buffer == 0) {
                    tracePrint(D_004CC720);
                    return -1;
                }
                memcpy(buffer, data, size);
                ((SrsResFlat *) _srsMemRes)->pending[16 + i] = 1;
                ((SrsResFlat *) _srsMemRes)->no[16 + i] = effectNo;
                sresDataMapping();
                return size;
            }
        }
    } else {
        tracePrint(D_004CC748, effectType, effectNo);
    }
    tracePrint(D_004CC768);
    return -1;
}

extern int strcmp(const char *s1, const char *s2);

int srsEffectNameToID(char *effectName)
{
    int index;
    char *name;

    if (effectName == 0) {
        return 0;
    }
    for (index = 1; index < 3072; index++) {
        name = srsGetEffectData(index);
        if (name != 0 && strcmp(effectName, name) == 0) {
            return index;
        }
    }
    return 0;
}

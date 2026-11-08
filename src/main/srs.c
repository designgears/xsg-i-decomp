#include "common.h"
#include "shared.h"
#include "srs.h"

static int srsLoadMode = 0;
char *_loadCommonData[1] = { "default" };
char *_loadCfCommonData[1] = { "cf_def" };

extern char D_004CBFC0[];
extern char D_004CBFD0[];
extern char D_004CBFE0[];
extern char D_004CBFF0[];
extern char D_004CC000[];
extern char D_004CC010[];
extern char D_004CC020[];
extern char D_004CC030[];
extern char D_004CC040[];
extern char D_004CC050[];
extern char D_004CC060[];
extern char D_004CC070[];
extern char D_004CC080[];
extern char D_004CC090[];
extern char D_004CC0A0[];
extern char D_004CC0B0[];
extern char D_004CC0C0[];
extern char D_004CC0D0[];
extern char D_004CC0E0[];
extern char D_004CC0F0[];
extern char D_004CC100[];
extern char D_004CC110[];
extern char D_004CC120[];
extern char D_004CC130[];
extern char D_004CC140[];
extern char D_004CC150[];
extern char D_004CC160[];
extern char D_004CC170[];
extern char D_004CC180[];
extern char D_004CC190[];
extern char D_004CC1A0[];
extern char D_004CC1B0[];
extern char D_004CC1C0[];
extern char D_004CC1D0[];
extern char D_004CC1E0[];
extern char D_004CC1F0[];
extern char D_004CC200[];
extern char D_004CC210[];
extern char D_004CC220[];
extern char D_004CC230[];
extern char D_004CC240[];
extern char D_004CC250[];
extern char D_004CC260[];
extern char D_004CC270[];
extern char D_004CC280[];
extern char D_004CC290[];
extern char D_004CC2A0[];
extern char D_004CC2B0[];
extern char D_004CC2C0[];
extern char D_004CC2D0[];
extern char D_004CC2E0[];
extern char D_004CC2F0[];
extern char D_004CC300[];
extern char D_004CC310[];
extern char D_004CC320[];
extern char D_004CC330[];
extern char D_004CC340[];
extern char D_004CC350[];
extern char D_004CC360[];
extern char D_004CC370[];
extern char D_004CC380[];
extern char D_004CC390[];
extern char D_004CC3A0[];
extern char D_004CC3B0[];
extern char D_004CC3C0[];
extern char D_004CC3D0[];
extern char D_004CC3E0[];
extern char D_004CC3F0[];
extern char D_004CC400[];
extern char D_004CC410[];
extern char D_004CC420[];
extern char D_004CC430[];
extern char D_004CC440[];
extern char D_004CC450[];
extern char D_004CC460[];
extern char D_004CC470[];
extern char D_004CC480[];
extern char D_004CC490[];
extern char D_004CC4A0[];
extern char D_004CC4B0[];
extern char D_004CC4C0[];
extern char D_004CC4D0[];
extern char D_004CC4E0[];
extern char D_004CC4F0[];
extern char D_004CC500[];
extern char D_004CC510[];
extern char D_004CC520[];
extern char D_004CC530[];
extern char D_004CC540[];
extern char D_004CC550[];
extern char D_004CC560[];
extern char D_004CC570[];
extern char D_004CC580[];
extern char D_004CC590[];
extern char D_004CC5A0[];
extern char D_004CC5B0[];
extern char D_004CC5C0[];
extern char D_004CC5D0[];
extern char D_004CC5E0[];
extern char D_004CC5F0[];
extern char D_004CC600[];
extern char D_004CC610[];
extern char D_004CC620[];
extern char D_004CC630[];
extern char D_004CC640[];
extern char D_004CC650[];
extern char D_004CC660[];
extern char D_004CC670[];

static unsigned char srsViewPath[256] = { 0 };

char *_loadComboData[195] = {
    "default", "chaos", "kosmos", "shion",
    "shitan", "ziggy", "momo", "jr",
    "cecilia", D_004CC670, "virgil", "mary",
    "shelley", 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, D_004CC660, D_004CC660, D_004CC650,
    D_004CC640, D_004CC630, D_004CC620, D_004CC610,
    D_004CC600, D_004CC5F0, D_004CC5E0, D_004CC5D0,
    D_004CC5C0, D_004CC5B0, D_004CC5A0, D_004CC590,
    D_004CC580, D_004CC570, D_004CC560, D_004CC550,
    D_004CC540, D_004CC530, D_004CC520, D_004CC510,
    D_004CC500, D_004CC4F0, D_004CC4E0, D_004CC660,
    D_004CC650, D_004CC640, D_004CC4D0, D_004CC4C0,
    D_004CC4B0, D_004CC4A0, D_004CC490, D_004CC480,
    D_004CC470, D_004CC460, D_004CC450, D_004CC440,
    D_004CC430, D_004CC420, D_004CC410, D_004CC400,
    D_004CC3F0, D_004CC3E0, D_004CC3D0, D_004CC3C0,
    D_004CC3B0, D_004CC3A0, D_004CC390, D_004CC380,
    D_004CC370, D_004CC360, D_004CC350, D_004CC340,
    D_004CC330, D_004CC320, D_004CC310, D_004CC300,
    D_004CC2F0, D_004CC2E0, D_004CC2D0, D_004CC2C0,
    D_004CC2B0, D_004CC2A0, D_004CC290, D_004CC280,
    D_004CC270, D_004CC260, D_004CC250, D_004CC240,
    D_004CC230, D_004CC220, D_004CC210, D_004CC200,
    D_004CC1F0, D_004CC1E0, D_004CC1D0, D_004CC3D0,
    D_004CC3D0, D_004CC390, D_004CC610, D_004CC4C0,
    D_004CC420, D_004CC410, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, D_004CC420, D_004CC420,
    D_004CC420, D_004CC420, D_004CC420, D_004CC1C0,
    D_004CC1B0, D_004CC1A0, D_004CC190, D_004CC180,
    D_004CC170, D_004CC160, D_004CC150, D_004CC140,
    D_004CC130, D_004CC120, D_004CC110, D_004CC100,
    D_004CC0F0, D_004CC0E0, D_004CC0D0, D_004CC0C0,
    D_004CC0B0, D_004CC0A0, D_004CC090, D_004CC080,
    D_004CC070, D_004CC060, D_004CC050, D_004CC040,
    D_004CC030, D_004CC020, D_004CC010, D_004CC000,
    D_004CBFF0, D_004CBFE0, D_004CBFD0, D_004CBFC0,
    D_004CC1A0, D_004CC190, D_004CC090, "",
    "", "", "", "",
    "", "", "",
};

extern char _loadEsdData[]; /* 0xC800-byte effect-name table. */ /* 0xC800-byte table (config/symbols/main.txt) */

static signed char _srsEffect2IndexTbl[117];

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
static SrsChrEfEntry _srsChrEfTbl[12];
static SrsChrEf2Entry _srsChrEfTbl2[161];
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
        ret = _loadCfCommonData[0];
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

static SrsChrEfTbl3Entry _srsChrEfTbl3[36];

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

extern unsigned char *strcpy(unsigned char *destination, const unsigned char *source);

void srsSetViewPath(unsigned char *path)
{
    strcpy(srsViewPath, path);
}

extern char *strcat(char *destination, const char *source);
typedef struct { char s[15]; } SrsPath15;
typedef struct { char s[23]; } SrsPath23;
typedef struct { char s[7]; } SrsPath7;

static int srsMakeFileName(char *path, char *name)
{
    if (name == 0) {
        path[0] = 0;
        return 0;
    }
    if (srsLoadMode == 0) {
        *(SrsPath15 *)path = *(SrsPath15 *)"data/simajiri/";
        strcat(path, name);
    } else if (srsLoadMode == 1) {
        *(SrsPath23 *)path = *(SrsPath23 *)"host0:/home/xeno/work/";
        strcat(path, (char *)srsViewPath);
        strcat(path, name);
    } else if (srsLoadMode == 2) {
        *(SrsPath7 *)path = *(SrsPath7 *)"usb0:/";
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
    sprintf(msg_0_00795120, "esd/%s%s", name, ".esd");
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

static WeaponTblEntry _weaponTbl[45];

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

char *srsGetEffectName2(int effectNo)
{
    char *name;

    name = srsGetEffectData(effectNo);
    if (name == 0) {
        return name;
    }
    sprintf(msg_1_007951A0, "esp/%s%s", name, ".esp");
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
    sprintf(msg_2_00795220, "esp/%s%s", value, ".esp");
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

void sresLoadCommonMemory(void)
{
    SrsMemResEarly *memRes;
    int size;
    void *chunk[8];

    memRes = (SrsMemResEarly *) _srsMemRes;
    if (memRes->image == 0) {
        memRes->image = smAlloc(0x25800);
        if (memRes->image != 0) {
            size = fileLoad(memRes->image, "esp/default.esp", 0);
            if (size >= 0) {
                if (size <= 0x25800) {
                    chunk[0] = memRes->image;
                    svAddImageMapper(0, 0, chunk, 3000);
                }
            }
        }
        memset(_loadEsdData, 0, 0xC800);
        fileLoad(_loadEsdData, "seffect.esp", 0);
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
            size = fileLoad(memRes->cfImage, "esp/cf_def.esp", 0);
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
                    tracePrint("#### \xA3\xC3\xA3\xC6\xA5\xA8\xA5\xD5\xA5\xA7\xA5\xAF\xA5\xC8\xA5\xE1\xA5\xE2\xA5\xEA\xA4\xAC\xC2\xAD\xA4\xEA\xA4\xDE\xA4\xBB\xA4\xF3!!");
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
        tracePrint("-- err category(%d) eft(%d)", effectType, effectNo);
    }
    tracePrint("-- err : eftNo is not cf load");
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
                    tracePrint("#### \xA3\xC3\xA3\xC6\xA5\xA8\xA5\xD5\xA5\xA7\xA5\xAF\xA5\xC8\xA5\xE1\xA5\xE2\xA5\xEA\xA4\xAC\xC2\xAD\xA4\xEA\xA4\xDE\xA4\xBB\xA4\xF3!!");
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
        tracePrint("-- err category(%d) eft(%d)", effectType, effectNo);
    }
    tracePrint("-- err : eftNo is not cf load");
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

static signed char _srsEffect2IndexTbl[117] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 5, 6,
    7, 8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4,
    5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6,
    7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2,
    3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8,
    9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4,
    5, 6, 7, 8, 9,
};

static SrsChrEfEntry _srsChrEfTbl[12] = {
    { 0x0001, 0x001C, 0x0021, 0x0022, 0x0029, 0x07EE, 0x07F9, 0x09CD, 0x09D0 },
    { 0x0002, 0x000F, 0x0014, 0x0015, 0x0017, 0x07E1, 0x07ED, 0x09C9, 0x09CC },
    { 0x0003, 0x0001, 0x0006, 0x0007, 0x000E, 0x07D1, 0x07E0, 0x09C5, 0x09C8 },
    { 0x0004, 0x0052, 0x0057, 0x0058, 0x005F, 0x0818, 0x0824, 0x09DD, 0x09E0 },
    { 0x0005, 0x0038, 0x003D, 0x003E, 0x0044, 0x0806, 0x080D, 0x09D5, 0x09D8 },
    { 0x0006, 0x002A, 0x002F, 0x0030, 0x0037, 0x07FA, 0x0805, 0x09D1, 0x09D4 },
    { 0x0007, 0x0045, 0x004A, 0x004B, 0x0051, 0x080E, 0x0817, 0x09D9, 0x09DC },
    { 0x0008, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
    { 0x0009, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
    { 0x000A, 0x0060, 0x0062, 0x0000, 0x0000, 0x0000, 0x0000, 0x09E1, 0x09E4 },
    { 0x000B, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
    { 0x000C, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
};

static SrsChrEf2Entry _srsChrEfTbl2[161] = {
    { 0x0022, 0x010B, 0x010B },
    { 0x0023, 0x010C, 0x010D },
    { 0x0024, 0x010E, 0x010F },
    { 0x0025, 0x0110, 0x0112 },
    { 0x0026, 0x0113, 0x0115 },
    { 0x0027, 0x0116, 0x0118 },
    { 0x0028, 0x0119, 0x0119 },
    { 0x0029, 0x011A, 0x011B },
    { 0x002A, 0x011C, 0x011D },
    { 0x002B, 0x011E, 0x0120 },
    { 0x002C, 0x0121, 0x0122 },
    { 0x002D, 0x0123, 0x0125 },
    { 0x002E, 0x0126, 0x0127 },
    { 0x002F, 0x0128, 0x0129 },
    { 0x0030, 0x012A, 0x012C },
    { 0x0031, 0x012D, 0x012F },
    { 0x0032, 0x0130, 0x0131 },
    { 0x0033, 0x0132, 0x0133 },
    { 0x0034, 0x0134, 0x0136 },
    { 0x0035, 0x0137, 0x0138 },
    { 0x0036, 0x0139, 0x013A },
    { 0x0037, 0x013B, 0x013C },
    { 0x0038, 0x013D, 0x013D },
    { 0x0039, 0x013E, 0x013E },
    { 0x003A, 0x013F, 0x013F },
    { 0x003B, 0x010B, 0x010B },
    { 0x003C, 0x010C, 0x010D },
    { 0x003D, 0x010E, 0x010F },
    { 0x003E, 0x0145, 0x0146 },
    { 0x003F, 0x00F1, 0x00F2 },
    { 0x0040, 0x00F3, 0x00F5 },
    { 0x0041, 0x00F6, 0x00F6 },
    { 0x0042, 0x00F8, 0x00F9 },
    { 0x0043, 0x00FA, 0x00FB },
    { 0x0044, 0x00FC, 0x00FE },
    { 0x0045, 0x00FF, 0x00FF },
    { 0x0046, 0x0100, 0x0102 },
    { 0x0047, 0x0103, 0x0105 },
    { 0x0048, 0x0106, 0x0106 },
    { 0x0049, 0x0107, 0x0107 },
    { 0x004A, 0x0108, 0x0108 },
    { 0x004B, 0x0109, 0x0109 },
    { 0x004C, 0x010A, 0x010A },
    { 0x004D, 0x0147, 0x0147 },
    { 0x004E, 0x0148, 0x0148 },
    { 0x004F, 0x0149, 0x014A },
    { 0x0050, 0x014B, 0x014C },
    { 0x0051, 0x014D, 0x014E },
    { 0x0052, 0x014F, 0x0151 },
    { 0x0053, 0x0152, 0x0153 },
    { 0x0054, 0x0154, 0x0156 },
    { 0x0055, 0x0157, 0x0159 },
    { 0x0056, 0x015A, 0x015B },
    { 0x0057, 0x015C, 0x015D },
    { 0x0058, 0x015E, 0x015F },
    { 0x0059, 0x0160, 0x0161 },
    { 0x005A, 0x0162, 0x0164 },
    { 0x005B, 0x0165, 0x0165 },
    { 0x005C, 0x0166, 0x0168 },
    { 0x005D, 0x0169, 0x016B },
    { 0x005E, 0x016C, 0x016E },
    { 0x005F, 0x016F, 0x0171 },
    { 0x0060, 0x0172, 0x0174 },
    { 0x0061, 0x0175, 0x0175 },
    { 0x0062, 0x0176, 0x0176 },
    { 0x0063, 0x0177, 0x0177 },
    { 0x0064, 0x0178, 0x0178 },
    { 0x0065, 0x0179, 0x017B },
    { 0x0066, 0x017C, 0x017E },
    { 0x0067, 0x017F, 0x0180 },
    { 0x0068, 0x0181, 0x0181 },
    { 0x0069, 0x0182, 0x0182 },
    { 0x006A, 0x0183, 0x0184 },
    { 0x006B, 0x0185, 0x0185 },
    { 0x006C, 0x0186, 0x0187 },
    { 0x006D, 0x0188, 0x0189 },
    { 0x006E, 0x018A, 0x018B },
    { 0x0097, 0x0916, 0x091A },
    { 0x0098, 0x091B, 0x091F },
    { 0x0099, 0x0907, 0x090B },
    { 0x009A, 0x0902, 0x0906 },
    { 0x009B, 0x0925, 0x0929 },
    { 0x009C, 0x0920, 0x0924 },
    { 0x009D, 0x08FD, 0x0901 },
    { 0x009E, 0x092A, 0x092E },
    { 0x009F, 0x092F, 0x0933 },
    { 0x00A0, 0x0934, 0x0938 },
    { 0x00A1, 0x0939, 0x093D },
    { 0x00A2, 0x093E, 0x0942 },
    { 0x00A3, 0x0943, 0x0947 },
    { 0x00A4, 0x0948, 0x094C },
    { 0x00A5, 0x094D, 0x0951 },
    { 0x00A6, 0x0952, 0x0956 },
    { 0x00A7, 0x0957, 0x095B },
    { 0x00A8, 0x095C, 0x0960 },
    { 0x00A9, 0x0961, 0x0965 },
    { 0x00AA, 0x0966, 0x096A },
    { 0x00AB, 0x096B, 0x096F },
    { 0x00AC, 0x0970, 0x0974 },
    { 0x00AD, 0x097A, 0x097E },
    { 0x00AE, 0x097F, 0x0983 },
    { 0x00AF, 0x0984, 0x0988 },
    { 0x00B0, 0x0989, 0x098D },
    { 0x00B1, 0x098E, 0x0992 },
    { 0x00B2, 0x0993, 0x0997 },
    { 0x00B3, 0x0998, 0x099C },
    { 0x00B4, 0x099D, 0x09A1 },
    { 0x00B5, 0x09A2, 0x09A6 },
    { 0x00B6, 0x09AC, 0x09B0 },
    { 0x00B7, 0x0975, 0x0979 },
    { 0x00B8, 0x0907, 0x090B },
    { 0x00B9, 0x0902, 0x0906 },
    { 0x00BA, 0x0966, 0x096A },
    { 0x0097, 0x0A79, 0x0A7B },
    { 0x0098, 0x0A7C, 0x0A7E },
    { 0x0099, 0x0A7F, 0x0A81 },
    { 0x009A, 0x0A82, 0x0A84 },
    { 0x009B, 0x0A85, 0x0A87 },
    { 0x009C, 0x0A88, 0x0A8A },
    { 0x009D, 0x0A8B, 0x0A8D },
    { 0x009E, 0x0A8E, 0x0A90 },
    { 0x009F, 0x0A91, 0x0A93 },
    { 0x00A0, 0x0A94, 0x0A96 },
    { 0x00A1, 0x0A97, 0x0A99 },
    { 0x00A2, 0x0A9A, 0x0A9C },
    { 0x00A3, 0x0A9D, 0x0A9F },
    { 0x00A4, 0x0AA0, 0x0AA2 },
    { 0x00A5, 0x0AA3, 0x0AA5 },
    { 0x00A6, 0x0AA6, 0x0AA8 },
    { 0x00A7, 0x0AA9, 0x0AAB },
    { 0x00A8, 0x0AAC, 0x0AAE },
    { 0x00A9, 0x0AAF, 0x0AB1 },
    { 0x00AA, 0x0AB2, 0x0AB4 },
    { 0x00AB, 0x0AB5, 0x0AB7 },
    { 0x00AC, 0x0AB8, 0x0ABA },
    { 0x00AD, 0x0ABB, 0x0ABD },
    { 0x00AE, 0x0ABE, 0x0AC0 },
    { 0x00AF, 0x0AC1, 0x0AC3 },
    { 0x00B0, 0x0AC4, 0x0AC6 },
    { 0x00B1, 0x0AC7, 0x0AC9 },
    { 0x00B2, 0x0ACA, 0x0ACC },
    { 0x00B3, 0x0ACD, 0x0ACF },
    { 0x00B4, 0x0AD0, 0x0AD2 },
    { 0x00B5, 0x0AD3, 0x0AD5 },
    { 0x00B6, 0x0AD6, 0x0AD8 },
    { 0x00B7, 0x0AD9, 0x0ADB },
    { 0x00B8, 0x0A7F, 0x0A81 },
    { 0x00B9, 0x0A82, 0x0A84 },
    { 0x00BA, 0x0AB2, 0x0AB4 },
    { 0x006F, 0x0148, 0x0148 },
    { 0x0070, 0x0148, 0x0148 },
    { 0x0071, 0x014F, 0x0151 },
    { 0x0072, 0x0116, 0x0118 },
    { 0x003F, 0x00F1, 0x00F2 },
    { 0x0049, 0x0107, 0x0107 },
    { 0x004A, 0x0108, 0x0108 },
    { 0x0092, 0x0107, 0x0107 },
    { 0x0093, 0x0107, 0x0107 },
    { 0x0094, 0x0107, 0x0107 },
    { 0x0095, 0x0107, 0x0107 },
    { 0x0096, 0x0107, 0x0107 },
};

static SrsChrEfTbl3Entry _srsChrEfTbl3[36] = {
    { 0x0097, 0x0A79, { 0x7B, 0x0A } },
    { 0x0098, 0x0A7C, { 0x7E, 0x0A } },
    { 0x0099, 0x0A7F, { 0x81, 0x0A } },
    { 0x009A, 0x0A82, { 0x84, 0x0A } },
    { 0x009B, 0x0A85, { 0x87, 0x0A } },
    { 0x009C, 0x0A88, { 0x8A, 0x0A } },
    { 0x009D, 0x0A8B, { 0x8D, 0x0A } },
    { 0x009E, 0x0A8E, { 0x90, 0x0A } },
    { 0x009F, 0x0A91, { 0x93, 0x0A } },
    { 0x00A0, 0x0A94, { 0x96, 0x0A } },
    { 0x00A1, 0x0A97, { 0x99, 0x0A } },
    { 0x00A2, 0x0A9A, { 0x9C, 0x0A } },
    { 0x00A3, 0x0A9D, { 0x9F, 0x0A } },
    { 0x00A4, 0x0AA0, { 0xA2, 0x0A } },
    { 0x00A5, 0x0AA3, { 0xA5, 0x0A } },
    { 0x00A6, 0x0AA6, { 0xA8, 0x0A } },
    { 0x00A7, 0x0AA9, { 0xAB, 0x0A } },
    { 0x00A8, 0x0AAC, { 0xAE, 0x0A } },
    { 0x00A9, 0x0AAF, { 0xB1, 0x0A } },
    { 0x00AA, 0x0AB2, { 0xB4, 0x0A } },
    { 0x00AB, 0x0AB5, { 0xB7, 0x0A } },
    { 0x00AC, 0x0AB8, { 0xBA, 0x0A } },
    { 0x00AD, 0x0ABB, { 0xBD, 0x0A } },
    { 0x00AE, 0x0ABE, { 0xC0, 0x0A } },
    { 0x00AF, 0x0AC1, { 0xC3, 0x0A } },
    { 0x00B0, 0x0AC4, { 0xC6, 0x0A } },
    { 0x00B1, 0x0AC7, { 0xC9, 0x0A } },
    { 0x00B2, 0x0ACA, { 0xCC, 0x0A } },
    { 0x00B3, 0x0ACD, { 0xCF, 0x0A } },
    { 0x00B4, 0x0AD0, { 0xD2, 0x0A } },
    { 0x00B5, 0x0AD3, { 0xD5, 0x0A } },
    { 0x00B6, 0x0AD6, { 0xD8, 0x0A } },
    { 0x00B7, 0x0AD9, { 0xDB, 0x0A } },
    { 0x00B8, 0x0A7F, { 0x81, 0x0A } },
    { 0x00B9, 0x0A82, { 0x84, 0x0A } },
    { 0x00BA, 0x0AB2, { 0xB4, 0x0A } },
};

static WeaponTblEntry _weaponTbl[45] = {
    { 0x0046, 0x0AF1, { 0xF2, 0x0A, 0xF1, 0x0A } },
    { 0x0047, 0x0AF3, { 0xF3, 0x0A, 0xF4, 0x0A } },
    { 0x0048, 0x0AF5, { 0xF6, 0x0A, 0xF7, 0x0A } },
    { 0x0049, 0x0AF8, { 0x2E, 0x0B, 0x2F, 0x0B } },
    { 0x004A, 0x0AF9, { 0xF9, 0x0A, 0xFA, 0x0A } },
    { 0x004B, 0x0AFB, { 0xFC, 0x0A, 0xFD, 0x0A } },
    { 0x004C, 0x0AFE, { 0x30, 0x0B, 0x31, 0x0B } },
    { 0x004D, 0x0AFF, { 0xFF, 0x0A, 0x00, 0x0B } },
    { 0x004E, 0x0B01, { 0x01, 0x0B, 0x33, 0x0B } },
    { 0x004F, 0x0B02, { 0x02, 0x0B, 0x02, 0x0B } },
    { 0x0050, 0x0B03, { 0x03, 0x0B, 0x04, 0x0B } },
    { 0x0051, 0x0B05, { 0x05, 0x0B, 0x32, 0x0B } },
    { 0x0052, 0x0B06, { 0x06, 0x0B, 0x07, 0x0B } },
    { 0x0053, 0x0B08, { 0x08, 0x0B, 0x2D, 0x0B } },
    { 0x0054, 0x0B09, { 0x09, 0x0B, 0x09, 0x0B } },
    { 0x0055, 0x0B0A, { 0x0A, 0x0B, 0x0B, 0x0B } },
    { 0x0056, 0x0B0C, { 0x0C, 0x0B, 0x0D, 0x0B } },
    { 0x0057, 0x0B0E, { 0x0E, 0x0B, 0x0F, 0x0B } },
    { 0x0058, 0x0B10, { 0x10, 0x0B, 0x10, 0x0B } },
    { 0x0059, 0x0B11, { 0x11, 0x0B, 0x12, 0x0B } },
    { 0x005A, 0x0B13, { 0x13, 0x0B, 0x14, 0x0B } },
    { 0x005B, 0x0B15, { 0x15, 0x0B, 0x16, 0x0B } },
    { 0x0070, 0x0B15, { 0x15, 0x0B, 0x16, 0x0B } },
    { 0x005C, 0x0B17, { 0x17, 0x0B, 0x18, 0x0B } },
    { 0x0071, 0x0B17, { 0x17, 0x0B, 0x18, 0x0B } },
    { 0x005D, 0x0B19, { 0x19, 0x0B, 0x19, 0x0B } },
    { 0x005E, 0x0B1A, { 0x1A, 0x0B, 0x1A, 0x0B } },
    { 0x0072, 0x0B1A, { 0x1A, 0x0B, 0x1A, 0x0B } },
    { 0x005F, 0x0B1B, { 0x1B, 0x0B, 0x1B, 0x0B } },
    { 0x0060, 0x0B1C, { 0x1C, 0x0B, 0x1C, 0x0B } },
    { 0x0061, 0x0B1D, { 0x1D, 0x0B, 0x1D, 0x0B } },
    { 0x0062, 0x0B1E, { 0x1E, 0x0B, 0x1E, 0x0B } },
    { 0x0063, 0x0B1F, { 0x1F, 0x0B, 0x1F, 0x0B } },
    { 0x0064, 0x0B20, { 0x20, 0x0B, 0x20, 0x0B } },
    { 0x0065, 0x0B21, { 0x21, 0x0B, 0x21, 0x0B } },
    { 0x0066, 0x0B22, { 0x22, 0x0B, 0x22, 0x0B } },
    { 0x0067, 0x0B23, { 0x23, 0x0B, 0x23, 0x0B } },
    { 0x0068, 0x0B24, { 0x24, 0x0B, 0x24, 0x0B } },
    { 0x0069, 0x0B25, { 0x25, 0x0B, 0x25, 0x0B } },
    { 0x006C, 0x0B26, { 0x26, 0x0B, 0x26, 0x0B } },
    { 0x006E, 0x0B27, { 0x27, 0x0B, 0x27, 0x0B } },
    { 0x006A, 0x0B28, { 0x28, 0x0B, 0x28, 0x0B } },
    { 0x006B, 0x0B29, { 0x29, 0x0B, 0x29, 0x0B } },
    { 0x006F, 0x0B2A, { 0x2A, 0x0B, 0x2A, 0x0B } },
    { 0x006D, 0x0B2B, { 0x2B, 0x0B, 0x2C, 0x0B } },
};

unsigned char _srsMemRes[0x1A0] = { 0 };

char _loadEsdData[0xC800] = { 0 };

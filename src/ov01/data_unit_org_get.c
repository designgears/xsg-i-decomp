/*
 * OV01 original TU 5: 0x00a191c0..0x00a1d348 (93 functions)
 */
#include "common.h"
#include "data_unit_org_get.h"
#include "ov01/calc.h"

/* Types recovered upstream (origin/master); kept for reference. */
typedef struct BattleDataHeader {
    unsigned char unmodeled_00[8];
    int unitInitOffset; /* +0x08 */
    int techniqueOffset;
    int normalInitOffset; /* +0x10 */
    int specialInitOffset; /* +0x14 */
    int etherInitOffset; /* +0x18 */
    int weaponOffset;
    int ethOffset;
    int itmOffset;
    int attOffset;
    int accessoryOffset;
    int skillOffset;
    int gainOffset;
    int experienceOffset; /* +0x38 */
    int parameterOffset; /* +0x3C */
    int specialTableOffset; /* +0x40 */
    int defaultEquipmentOffset; /* +0x44 */
} BattleDataHeader;

/* Fixed-size records the data*Get accessors index (upstream recovery). */
typedef struct NormalInitRecord { unsigned char opaque[0x0C]; } NormalInitRecord;
typedef struct SpecialInitRecord { unsigned char opaque[0x0C]; } SpecialInitRecord;
typedef struct EtherInitRecord { unsigned char opaque[0x18]; } EtherInitRecord;
typedef struct ParameterRecord { unsigned char opaque[0x20]; } ParameterRecord;
typedef struct DefaultEquipmentRecord { unsigned char opaque[0x20]; } DefaultEquipmentRecord;
typedef struct SpecialTableRecord { unsigned char opaque[0x20]; } SpecialTableRecord;


/*
 * orgData is a 33-entry table of 0x180-byte unit-origin records; callers
 * pass a one-based index, so dataUnitOrgGet returns orgData[id - 1].
 */
typedef struct UnitOrgRecord {
    unsigned char unmodeled_0[0x180];
} UnitOrgRecord;

extern const char D_00A456A8[];
extern UnitOrgRecord orgData[];

void *dataUnitOrgGet(int unitOrgId)
{
    if (unitOrgId >= 0x21) {
        printf(D_00A456A8, unitOrgId);
        return 0;
    }
    return &orgData[unitOrgId - 1];
}

extern const char D_00A456C8[];
extern unsigned char plChaData[];

PlCharacter *dataPlChaGet(int cid)
{
    unsigned char *base;

    if (cid >= 0x21) {
        printf(D_00A456C8, cid);
        return 0;
    }
    base = plChaData - 0xA8;
    return (PlCharacter *)(base + cid * 0xA8);
}

/*
 * D_426F48, D_426F50, D_426F54 and D_426F58 are byte offsets, from their own
 * loaded battle-data table's start (8, 0x10, 0x14 and 0x18 bytes before each
 * field respectively), the same idiom D_426F78 uses below for
 * dataExpTblGet; each points to a table of fixed-size records that
 * dataUnitInitGet, dataNormInitGet, dataSpecInitGet and dataEtherInitGet
 * index with a one-based id, as dataUnitOrgGet does orgData.
 */
extern int D_426F48;
extern int D_426F50;
extern int D_426F54;
extern int D_426F58;

void *dataUnitInitGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F48 - 8;
    value = D_426F48;
    return value + base + id * 0x34 - 0x34;
}

void *dataNormInitGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F50 - 0x10;
    value = D_426F50;
    return value + base + id * 0xC - 0xC;
}

void *dataSpecInitGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F54 - 0x14;
    value = D_426F54;
    return value + base + id * 0xC - 0xC;
}

void *dataEtherInitGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F58 - 0x18;
    value = D_426F58;
    return value + base + id * 0x18 - 0x18;
}

/*
 * D_426F78 is a selector field 0x38 bytes into a loaded battle-data table;
 * dataExpTblGet returns the table entry that selector picks out. Splat has
 * not named the table's own base, only this field.
 */
extern int D_426F78;

unsigned char *dataExpTblGet(void)
{
    return (unsigned char *)&D_426F78 - 0x38 + D_426F78;
}

/*
 * D_426F7C, D_426F80 and D_426F84 are the same kind of loaded-table byte
 * offset as D_426F48 above (0x3C, 0x40 and 0x44 bytes before their own table
 * start); dataParaTblGet, dataSpecTblGet and dataDefEquipGet each index a
 * table of 0x20-byte records with a one-based id.
 */
extern int D_426F7C;
extern int D_426F80;
extern int D_426F84;

void *dataParaTblGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F7C - 0x3C;
    value = D_426F7C;
    return value + base + id * 0x20 - 0x20;
}

void *dataDefEquipGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F84 - 0x44;
    value = D_426F84;
    return value + base + id * 0x20 - 0x20;
}

void *dataSpecTblGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F80 - 0x40;
    value = D_426F80;
    return value + base + id * 0x20 - 0x20;
}

extern void dataEtherLearnSet(int etherType, int techniqueId, int selector);
extern const char D_00A456E8[];
extern CalcUnitParam *calcUPGet(ObjectTask *unit);
extern void *dataUnitFileGet(ObjectTask *unit, int charaId);
extern int dataUnitFileLoadMot(ObjectTask *unit, int motionId, int slot, void *fileInfo);

extern void dataSpecLearnSet(int cid, int specialId);

void dataEtherTecSet(int selector)
{
    int etherType = 0;
    int techniqueId = 0;
    int specialId = 0;

    switch (selector) {
    case 1:
        etherType = 3;
        techniqueId = 13;
        break;
    case 2:
        etherType = 3;
        techniqueId = 14;
        break;
    case 3:
        etherType = 3;
        techniqueId = 15;
        break;
    case 4:
        etherType = 3;
        techniqueId = 16;
        break;
    case 5:
        etherType = 6;
        techniqueId = 43;
        specialId = 90;
        break;
    case 6:
        etherType = 6;
        techniqueId = 46;
        specialId = 89;
        break;
    case 7:
        etherType = 7;
        techniqueId = 57;
        specialId = 97;
        break;
    case 8:
        etherType = 6;
        specialId = 88;
        break;
    default:
        printf(D_00A456E8, selector);
        return;
    }

    if (techniqueId != 0) {
        dataEtherLearnSet(etherType, techniqueId, selector);
    }
    if (specialId != 0) {
        dataSpecLearnSet(etherType, specialId);
    }
}

extern const char D_00A45730[];

void dataEtherLearnSet(int cid, int id, int selector)
{
    PlCharacter *pl;
    int b;

    if (cid > 16) {
        printf(D_00A45730, cid);
        return;
    }
    if (id != 0) {
        pl = dataPlChaGet(cid);
        b = id - 1;
        *(unsigned char *)((b / 8) + (int)pl + 0x28) |= (1 << (b % 8));
    }
}

extern const char D_00A45750[];

int dataEtherLearnGet(int cid, int id)
{
    PlCharacter *pl;
    int b;

    if (cid > 16) {
        printf(D_00A45750, cid);
        return 0;
    }
    pl = dataPlChaGet(cid);
    b = id - 1;
    return *(signed char *)((b / 8) + (int)pl + 0x28) & (1 << (b % 8));
}

extern const char D_00A45770[];

void dataSkillLearnSet(int cid, int id)
{
    PlCharacter *pl;
    int b;

    if (cid > 16) {
        printf(D_00A45770, cid);
        return;
    }
    if (id != 0) {
        pl = dataPlChaGet(cid);
        b = id - 1;
        *(unsigned char *)((b / 8) + (int)pl + 0x38) |= (1 << (b % 8));
    }
}

extern const char D_00A45790[];
extern PlCharacter *dataPlChaGet(int cid);

int dataSkillLearnGet(int cid, int id)
{
    PlCharacter *pl;
    int b;

    if (cid > 16) {
        printf(D_00A45790, cid);
        return 0;
    }
    pl = dataPlChaGet(cid);
    b = id - 1;
    return *(signed char *)((b / 8) + (int)pl + 0x38) & (1 << (b % 8));
}

typedef struct ThinkMapTable {
    unsigned char unmodeled_000[0x18C];
    int specBase[0x11]; /* +0x18C */
} ThinkMapTable;

extern ThinkMapTable thinkMapTbl;

int dataSpecBaseGet(int cid)
{
    return thinkMapTbl.specBase[cid];
}

/*
 * specTbl's tail is the character-id-indexed backup special-technique base
 * table dataBakpBaseGet reads.
 */
typedef struct SpecTable {
    int specialBase[0x3C / sizeof(int)];
    int bakpBase[1]; /* +0x3C, indexed dynamically */
} SpecTable;

extern SpecTable specTbl;

int dataBakpBaseGet(int cid)
{
    return specTbl.bakpBase[cid];
}

void dataSpecLearnSet(int cid, int specialId)
{
    PlCharacter *pl;

    if (cid >= 0x11) {
        printf(D_00A457B0, cid);
        return;
    }
    if (specialId != 0) {
        pl = dataPlChaGet(cid);
        pl->special[specialId - dataSpecBaseGet(cid)].id = specialId;
    }
}

/*
 * thinkMapTbl's tail is a per-character-id table of special-technique base
 * ids; dataSpecLearnGet subtracts the caller's entry from a learned special
 * id before indexing PlCharacter.special, the same subtraction
 * dataSpecLearnSet performs through dataSpecBaseGet (still asm).
 */
extern const char D_00A457D0[];

short dataSpecLearnGet(int cid, int specialId)
{
    short learned;

    if (cid >= 0x11) {
        printf(D_00A457D0, cid);
        return 0;
    }
    learned = 0;
    if (specialId != 0) {
        learned = dataPlChaGet(cid)->special[specialId - thinkMapTbl.specBase[cid]].id;
    }
    return learned;
}

void *dataSpecDataGet(int cid, int id)
{
    PlCharacter *pl;
    int i;

    if (cid >= 0x11) {
        return 0;
    }
    if (id == 0) {
        return 0;
    }
    pl = dataPlChaGet(cid);
    for (i = 0; i < 8; i++) {
        if (pl->special[i].id == id) {
            return &pl->special[i];
        }
    }
    return 0;
}

extern const char D_00A456C8[];
extern unsigned char plChaData[];
extern const char D_00A45790[];
extern PlCharacter *dataPlChaGet(int cid);

int dataNormIdxGet(ObjectTask *unit, int id)
{
    int i;
    int off = 0x70;
    int ret = -1;
    for (i = 0; i < 6; i++) {
        if (*(short *)((char *)calcUPGet(unit) + off + 6) == id) {
            ret = i;
            break;
        }
        off += 2;
    }
    return ret;
}

typedef struct PlInitCopy12 { unsigned char b[0xC]; } PlInitCopy12;
typedef struct PlInitCopy24 { unsigned char b[0x18]; } PlInitCopy24;
typedef struct PlInitCopy34 { unsigned char b[0x34]; } PlInitCopy34;
typedef struct PlPackedInt { int v; } __attribute__((packed)) PlPackedInt;
typedef struct PlUnitSub {
    unsigned char pad0[4];
    unsigned short f4;
    unsigned short f6;
    unsigned short f8;
    unsigned char b[3];
    unsigned char padD;
    unsigned short s[3][3];
} PlUnitSub;
typedef struct PlUnitInit {
    unsigned short w0;
    unsigned short w2;
    unsigned char pad4[0x10];
    signed char level;
    unsigned char pad15[0x34 - 0x15];
} PlUnitInit;
typedef struct PlUnitRec {
    unsigned short w0;
    unsigned short w2;
    unsigned char pad4[0x10];
    signed char level;
    unsigned char pad15[0x30 - 0x15];
    PlUnitSub a;
    PlUnitSub b;
    unsigned char pad70[0x180 - 0x70];
} PlUnitRec;
typedef struct PlEquipSlot {
    unsigned short s0;
    unsigned char b;
    unsigned char pad;
    unsigned short s1;
} PlEquipSlot;
typedef struct PlChaSpec {
    short id;
    short pad2[2];
    short lv;
    unsigned char pad8[4];
} PlChaSpec;
typedef struct PlChaRec {
    unsigned char pad0[4];
    int nextExp;
    unsigned char pad8[0x10];
    unsigned short growth[8];
    unsigned char pad28[0x20];
    PlChaSpec spec[8];
} PlChaRec;
typedef struct PlShortTbl {
    short v[12];
} PlShortTbl;
typedef struct PlNormSlot { PlInitCopy12 norm; unsigned char pad[0x180 - 0xC]; } PlNormSlot;
extern PlNormSlot D_4229B6[];
extern int D_4229C2;
extern int D_4229CE;

void dataPlUnitInit(void)
{
  PlInitCopy12 *nb;
  unsigned char *cha;
  int i;
  int id;
  int k;
  int ofs;
  int ofs2;
  int j;
  PlUnitRec *rec;
  PlUnitSub *p;
  PlUnitSub *p2;
  unsigned char *q4;
  PlEquipSlot *eq;
  unsigned char *q;
  unsigned char *exp;
  unsigned short *para;
  PlInitCopy12 *sb;
  int *new_var;
  unsigned char *new_var2;
  unsigned short *d2;
  new_var = &D_4229CE;
  cha = plChaData;
  for (i = 0; i < 0x20; i++)
  {
    rec = &((PlUnitRec *) orgData)[i];
    nb = &D_4229B6[i].norm;
    memset(rec, 0, 0x180);
    id = i + 1;
    *((PlUnitInit *) rec) = *((PlUnitInit *) dataUnitInitGet(id));
    ofs = (i * 0x180) + 0x30;
    p = (PlUnitSub *) (((unsigned char *) orgData) + ofs);
    p->f4 = ((PlUnitInit *) rec)->w0;
    p->f6 = ((PlUnitInit *) rec)->w2;
    p->f8 = id;
    *nb = *((PlInitCopy12 *) dataNormInitGet(id));
    eq = dataDefEquipGet(id);
    d2 = rec->b.s[2];
    for (k = 0; k < 3; k++)
    {
      j = k * 6;
      q = ((unsigned char *) eq) + 2;
      q4 = ((unsigned char *) eq) + 4;
      rec->b.b[k] = q[j];
      d2[-6] = *((unsigned short *) (((unsigned char *) eq) + j));
      *d2 = *((unsigned short *) (q4 + j));
      d2++;
    }

    for (k = 0; k < 3; k++)
    {
      ((PlUnitRec *) (((unsigned char *) orgData) + (0x180 * i)))->b.s[1][k] = ((unsigned short *) (q + 0x10))[k];
    }

    new_var2 = (unsigned char *) new_var;
    ofs2 = (i * 0x180) + 0x50;
    p2 = ((unsigned char *) orgData) + ofs2;
    p2->f4 = *((unsigned short *) (((unsigned char *) eq) + 0x18));
    p2->f8 = *((unsigned short *) (((unsigned char *) eq) + 0x1A));
    p2->f6 = *((unsigned short *) (((unsigned char *) eq) + 0x1C));
    if (!eq)
    {
    }
    if (i < 0x10)
    {
      memset((void *) ((i * 0xA8) + ((int) cha)), 0, 0xA8);
      for (k = 7; k >= 0; k--)
      {
        ((short *) (cha + (i * 0xA8)))[(k * 6) + 0x27] = 2;
      }

      exp = ((unsigned char *(*)(int)) dataExpTblGet)(id);
      ((PlChaRec *) dataPlChaGet(id))->nextExp = ((PlPackedInt *) (exp + (((PlUnitRec *) (((unsigned char *) orgData) + (i * 0x180)))->level * 4)))->v;
      para = dataParaTblGet(id);
      for (k = 0; k < 8; k++)
      {
        ((PlChaRec *) dataPlChaGet(id))->growth[k] = ((PlShortTbl *) para)->v[k];
      }

      if (((!k) && (!k)) && (!k))
      {
      }
      if (!rec)
      {
      }
      for (k = 0; k < 6; k++)
      {
        dataSpecLearnSet(i + 1, ((PlShortTbl *) dataSpecInitGet(i + 1))->v[k]);
      }

      for (k = 0; ((k - 1) + 1) <= (12 - 1); k++)
      {
        ((void (*)(int, int)) dataEtherLearnSet)(i + 1, ((PlShortTbl *) dataEtherInitGet(i + 1))->v[k]);
      }

      sb = (PlInitCopy12 *) (((unsigned char *) (&D_4229C2)) + (i * 0x180));
      *sb = *((PlInitCopy12 *) dataSpecInitGet(id));
      *((PlInitCopy24 *) (new_var2 + (i * 0x180))) = *((PlInitCopy24 *) dataEtherInitGet(id));
      nb = &D_4229B6[i].norm;
    }
  }

}

void dataDummySet(void)
{
}

/*
 * playerData (7 entries) and enemyData (5 entries) are 0x180-byte battle
 * unit records; dataBattleInit is this TU's only writer and only clears the
 * calc-record index at +0x38.
 */
typedef struct BattleUnitInfo {
    unsigned char unmodeled_00[0x38];
    short calcIdx; /* +0x38 */
    unsigned char unmodeled_3A[0x146];
} BattleUnitInfo;

extern void dataFileInfoInit(void);
extern BattleUnitInfo playerData[7];
extern BattleUnitInfo enemyData[5];

void dataBattleInit(void)
{
    int i;

    dataFileInfoInit();
    for (i = 6; i >= 0; i--) {
        playerData[i].calcIdx = 0;
    }
    for (i = 4; i >= 0; i--) {
        enemyData[i].calcIdx = 0;
    }
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct BattleModelActor {
    BattleActor state;
    unsigned char unmodeled_88[0x8D0 - 0x88];
    void *modelAdr; /* +0x8D0 */
    void *animationAdr; /* +0x8D4 */
    void *textureAdr; /* +0x8D8 */
    /* dataUnitFileLoadMotSp2 indexes from +0x8DC; the extent is not yet
     * recovered. The overlapping motionAdr word is entry 1 at +0x8E0. */
    union {
        int slot[2];
        struct {
            int unmodeled_8dc;
            int motionAdr;
        } weapon;
    } motion;
} BattleModelActor;

typedef struct TecRecord {
    unsigned char unmodeled_00[0x11];
    signed char kind; /* +0x11 */
    unsigned char unmodeled_12[0x14 - 0x12];
    short motionId; /* +0x14 */
    unsigned char unmodeled_16[0x18 - 0x16];
} TecRecord;

typedef struct WpnRecord {
    unsigned char unmodeled_00[0x14];
    short tecId; /* +0x14 */
    unsigned char unmodeled_16[0x20 - 0x16];
} WpnRecord;

typedef struct AttRecord { unsigned char opaque[0x0E]; } AttRecord;

typedef struct EthRecord { unsigned char opaque[0x0C]; } EthRecord;

typedef struct ItmRecord { unsigned char opaque[0x14]; } ItmRecord;

typedef struct AccRecord { unsigned char opaque[0x10]; } AccRecord;

typedef struct FrmRecord { unsigned char opaque[0x10]; } FrmRecord;

typedef struct EngRecord { unsigned char opaque[0x0C]; } EngRecord;

typedef struct SklRecord { unsigned char opaque[0x08]; } SklRecord;

typedef struct GainRecord { unsigned char opaque[0x14]; } GainRecord;

typedef struct PartyLeaderView {
    unsigned char unmodeled_00[0x2C];
    unsigned short leader_battle_id; /* +0x2C */
} PartyLeaderView;

typedef struct PcPackEntry {
    int weaponId;
    char *name;
} PcPackEntry;

typedef struct CmdEntry {
    int selector; /* +0x00 */
    int argument; /* +0x04 */
    int extra;    /* +0x08: unwritten by this call */
    unsigned char unmodeled_0c[8]; /* +0x0C: unwritten by this call */
} CmdEntry;

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataUnitInitSet);

/*
 * posTbl is a table of 0x20-byte position records; dataPosTblGet and
 * dataPosLineGet both index it with the absolute value of the caller's
 * position id (negative ids alias the same record as their positive
 * counterpart). dataPosLineGet reads the halfword at +0x10 of the record.
 */
typedef struct PosTblEntry {
    float position[4];
    short line; /* +0x10 */
    unsigned char unmodeled_12[0x20 - 0x12];
} PosTblEntry;

extern PosTblEntry posTbl[];

void *dataPosTblGet(int id)
{
    if (id < 0) {
        id = -id;
    }
    return &posTbl[id];
}

short dataPosLineGet(int id)
{
    if (id < 0) {
        id = -id;
    }
    return posTbl[id].line;
}

extern unsigned char D_00A45890[]; /* "** dataCidGet: err %d\n" */
extern short cidChgTbl[];

short dataCidGet(unsigned int cid)
{
    if (cid >= 0xC4) {
        printf(D_00A45890, cid);
        return 0;
    }
    return cidChgTbl[cid];
}

extern unsigned char D_00A458A8[]; /* "** dataWidGet: err %d" */
extern short widChgTbl[];

short dataWidGet(unsigned int wid)
{
    if (wid >= 0x75) {
        printf(D_00A458A8, wid);
        return 0;
    }
    return widChgTbl[wid];
}

extern int D_426F4C;
extern const char D_00A458C0[];

unsigned char *dataTecGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A458C0, index);
        return 0;
    }
    base = (unsigned char *)&D_426F4C - 0xC;
    return base + D_426F4C + ((index * 3) << 3) - 0x18;
}

extern const char D_00A458D8[];
extern int D_426F5C;

unsigned char *dataWpnGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A458D8, index);
        return 0;
    }
    base = (unsigned char *)&D_426F5C - 0x1C;
    return base + D_426F5C + (index << 5) - 0x20;
}

extern const char D_00A458F0[];
extern int D_426F68;

unsigned char *dataAttGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A458F0, index);
        return 0;
    }
    base = (unsigned char *)&D_426F68 - 0x28;
    return base + D_426F68 + index * 14 - 0xE;
}

extern const char D_00A45908[];
extern int D_426F60;

unsigned char *dataEthGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45908, index);
        return 0;
    }
    base = (unsigned char *)&D_426F60 - 0x20;
    return base + D_426F60 + index * 12 - 0xC;
}

extern const char D_00A45920[];
extern int D_426F64;

unsigned char *dataItmGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45920, index);
        return 0;
    }
    base = (unsigned char *)&D_426F64 - 0x24;
    return base + D_426F64 + index * 20 - 0x14;
}

extern int D_426F6C;
extern const char D_00A45938[];

unsigned char *dataAccGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45938, index);
        return 0;
    }
    base = (unsigned char *)&D_426F6C - 0x2C;
    return base + D_426F6C + (index << 4) - 0x10;
}

extern const char D_00A45950[];
extern int D_426F88;

unsigned char *dataFrmGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45950, index);
        return 0;
    }
    base = (unsigned char *)&D_426F88 - 0x48;
    return base + D_426F88 + (index << 4) - 0x10;
}

extern char D_00A45968[];
extern int D_426F8C;

unsigned char *dataEngGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45968, index);
        return 0;
    }
    base = (unsigned char *)&D_426F8C - 0x4C;
    return base + D_426F8C + index * 12 - 0xC;
}

extern const char D_00A45980[];
extern int D_426F70;

unsigned char *dataSklGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45980, index);
        return 0;
    }
    base = (unsigned char *)&D_426F70 - 0x30;
    return base + D_426F70 + (index << 3) - 0x8;
}

extern const char D_00A45998[];
extern int D_426F74;

unsigned char *dataGainGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45998, index);
        return 0;
    }
    base = (unsigned char *)&D_426F74 - 0x34;
    return base + D_426F74 + ((((index << 2) + index)) << 2) - 0x294;
}

extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
extern char D_00A459B0[];
extern char D_00A459C8[];

int dataFileLoad(const char *name, void *dst)
{
    long size = xglCdReadFile(name, dst, 0, 0);
    if (size < 0) {
        printf(D_00A459B0, name);
        return 0;
    }
    printf(D_00A459C8, name, (int)size);
    return 1;
}

int dataCdSync(void)
{
    return cdReqNum;
}

void dataCdSyncClear(void)
{
    cdReqNum = 0;
}

extern int printf(const char *format, ...);
extern const char D_00A459E0[];
extern const char D_00A459F8[];

void dataNBreadCB(int result) {
    if (result < 0) {
        printf(D_00A459E0, result);
        return;
    }
    if (result == 4) {
        if (--cdReqNum == 0) {
            printf(D_00A459F8, 0);
        }
    }
}

extern const char D_00A45A10[];
extern const char D_00A45A28[];

int dataFileLoadNB(const char *name, void *dst)
{
    long size = xglCdReadFile(name, dst, 1, (int)dataNBreadCB);
    if (size < 0) {
        printf(D_00A45A10, name);
        return 0;
    }
    cdReqNum++;
    printf(D_00A45A28, cdReqNum, name, dst);
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataFileInfoInit);

extern const char D_00A45A48[];

void dataFileInfoInitChg(ObjectTask *unit, int charaId)
{
    char *file = dataUnitFileGet(unit, charaId);
    int i, j;
    char *p4, *p8, *pc, *p10;

    if (file == 0) {
        printf(D_00A45A48);
        return;
    }
    i = 0;
    p8 = file + 8;
    pc = file + 12;
    p10 = file + 16;
    *(int *)(file + 4) = 0;
    p4 = file + 4;
    for (; i < 3; i++) {
        int off = 0x38 + i * 24;
        *(int *)(p8 + off) = 0;
        *(float *)(pc + off) = 0.0f;
        *(int *)(p10 + off) = 0;
        for (j = 0; j < 3; j++) {
            int k = 0xa0 + i * 36 + j * 12;
            *(int *)(file + k) = 0;
            *(int *)(p4 + k) = 0;
            *(int *)(p8 + k) = 0;
        }
    }
}

typedef struct EnemyMdlHdr {
    unsigned int top;
    unsigned char *mid;
    unsigned char *end;
} EnemyMdlHdr;
typedef struct EnemyMdlRes {
    EnemyMdlHdr *address;
    int size;
    int pad[2];
} EnemyMdlRes;
extern EnemyMdlRes GameResource[];
extern const char D_00A45A68[];
extern const char D_00A45A80[];
extern const char D_00A45A98[];
extern int GameResourceSearch(int type, int id);
extern void xglMemCopy16(unsigned int dst, unsigned int src, int n);
extern void xglMemCopy16b(unsigned int dst, unsigned int src, int n);
typedef struct EnemyMdlEnt {
    short id;
    short pad[3];
} EnemyMdlEnt;
typedef struct EnemyMdlFile {
    int charaId;
    int cid;
    unsigned char *base;
    int pad0c[2];
    unsigned char *mid;
    int pad18[2];
    unsigned char *end;
    unsigned char pad24[0x130 - 0x24];
} EnemyMdlFile;
extern EnemyMdlFile D_00A59420[];

void dataEnemyMdlChk(EnemyMdlEnt *tbl)
{
  EnemyMdlFile *dst;
  int i;
  int cid;
  int idx;
  EnemyMdlHdr *h;
  int sz;
  unsigned char *n;
  unsigned char **p;
  int id;
  for (i = 0; (tbl[i].id != 0) && (i < 3); i++)
  {
    id = dataCidGet(tbl[i].id);
    cid = id;
    idx = GameResourceSearch(0, cid);
    if (idx != (-1))
    {
      h = GameResource[idx].address;
      sz = GameResource[idx].size;
      printf(D_00A45A68, h->top, h->mid, h->end, sz);
      if (((((h->top != 0) && (h->mid != 0)) && (h->end != 0)) && (h->top < ((unsigned int) h->mid))) && (h->mid < h->end))
      {
        id = tbl[i].id;
        dst = &D_00A59420[i];
        do
        {
          n = dst->base;
        }
        while (0);
        dst->charaId = id;
        do
        {
          p = &h->mid;
          n += ((unsigned int) (*p)) - h->top;
          dst->cid = cid;
          dst->mid = n;
          p = &h->end;
          n += (*p) - h->mid;
          dst->end = n;
        }
        while (0);
        if (((unsigned int) dst->base) < h->top)
        {
          xglMemCopy16((unsigned int) dst->base, h->top, sz / 16);
        }
        else
        {
          xglMemCopy16b((((unsigned int) dst->base) + sz) - 0x10, (h->top + sz) - 0x10, sz / 16);
        }
        printf(D_00A45A80, dst->base, dst->mid, dst->end);
        printf(D_00A45A98, tbl[i].id);
        return;
      }
    }
  }

}

extern int leaderId;
extern int calcAccEquChk(ObjectTask *unit, int id);
typedef struct LeaderChkWork {
    unsigned char unmodeled_0[0x84];
    void *org; /* +0x84 */
    unsigned char unmodeled_88[0x88];
} LeaderChkWork;

void dataLeaderChk(short *ids)
{
  union 
  {
    ObjectTask task;
    unsigned char pad[0x80];
  } t;
  LeaderChkWork work;
  unsigned char *party;
  UnitFileInfo *base;
  int *p0;
  int i;
  party = PartyDataGet();
  dataLeaderCidReset();
  base = unitFileInfo;
  for (i = 0; i < 3; i++)
  {
    unsigned short cid;
    p0 = (int *) (base[0].unmodeled_4 + 4);
    p0 = (int *) ((*(unitFileInfo + 0)).unmodeled_4 + 4);
    t.task.work = &work;
    work.org = dataUnitOrgGet(ids[i * 4]);
    cid = *((unsigned short *) (party + 0x2C));
    if ((cid == dataCidGet(ids[i * 4])) && (calcAccEquChk(&t.task, 0x2D) == 0))
    {
      int tmp = *((int *) (base[i].unmodeled_4 + 4));
      int tmp0 = ((int *) base)[2];
      *((void **) p0) = (void *) tmp;
      *((unsigned int *) (&leaderCid)) = *((unsigned short *) (party + 0x2C));
      *((int *) (base[i].unmodeled_4 + 4)) = tmp0;
      leaderId = ids[(i * 2) * 2];
      *((int *) (base[i].unmodeled_4 + 4)) = tmp0;
      return;
    }
  }

}

int dataLeaderCidGet(void) {
    return leaderCid;
}

void dataLeaderCidReset(void) {
    leaderCid = 0;
}

extern unsigned char *PartyDataGet(void);
extern const char D_00A45AB0[];

void dataLeaderReload(void)
{
    if (*(unsigned short *)(PartyDataGet() + 0x2C) != leaderCid) {
        printf(D_00A45AB0);
    }
}

extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern int dataMotNameGetSub(char *name, CalcUnitParam *up, int motionId, int weaponId);
extern char *motNameBase;
extern char *motNameExt;

int dataMotNameGetMenu(char *name, int unitOrgId, int weaponId)
{
    CalcUnitParam *up;
    int found;

    *name = 0;
    up = (CalcUnitParam *)dataUnitOrgGet(unitOrgId);
    if (up->sefSetupParams[weaponId] == 0) {
        return 0;
    }
    found = *(short *)(dataTecGet(*(short *)(dataWpnGet(up->sefSetupParams[weaponId]) + 0x14)) + 0x14);
    if (found == 0) {
        return 0;
    }
    strcpy(name, motNameBase);
    found = dataMotNameGetSub(name, up, found, weaponId);
    strcat(name, motNameExt);
    if (found == 0) {
        *name = 0;
    }
    return found;
}

extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern int dataMotNameGetSub(char *name, CalcUnitParam *up, int motionId, int weaponId);
extern char *motNameBase;
extern char *motNameExt;

int dataMotNameGet(char *name, ObjectTask *unit, int motionId, int weaponId)
{
    int found;

    strcpy(name, motNameBase);
    found = dataMotNameGetSub(name, calcUPGet(unit), motionId, weaponId);
    strcat(name, motNameExt);
    if (found == 0) {
        *name = 0;
    }
    return found;
}

extern char *motFile[];
extern char *agwsName[];
extern char *agwsLRName[];

int dataMotNameGetSub(char *name, CalcUnitParam *up, int motionId, int weaponId)
{
    char *mot;
    int lr;
    int i;
    int c;

    if (motionId < 0x157) {
        i = motionId;
        if (motionId >= 0x12E) {
            i = motionId - 0x2A;
        }
        mot = motFile[i - 1];
        if (mot != 0) {
            if (motionId >= 0x103 && (up->flags & 0x40)) {
                c = up->charaId - 0x11;
                if (motionId >= 0x12E) {
                    lr = 2;
                } else {
                    lr = (signed char)up->unmodeled_54[weaponId + 6] < 0x20;
                }
                strcat(name, agwsName[c]);
                strcat(name, mot);
                strcat(name, agwsLRName[lr]);
            } else {
                strcat(name, mot);
            }
            return 1;
        }
    }
    return 0;
}

extern int dataUnitFileLoadWep(ObjectTask *unit, int weaponId, int slot, void *fileInfo);
extern int dataUnitFileLoadMdl();
extern int leaderId;

int dataUnitFileLoad(ObjectTask *unit, int mode)
{
  void *file;
  int i;
  int off;
  unsigned char *base;
  short wpn;
  file = dataUnitFileGet(unit, mode);
  if (file == 0)
  {
    return 0;
  }
  if (leaderId == mode)
  {
    if (leaderId != calcUPGet(unit)->charaId)
    {
      dataLeaderCidReset();
    }
  }
  i = 0;
  mode = calcUPGet(unit)->charaId;
  dataUnitFileLoadMdl(unit, unitCidGet(unit), file);
  off = 0x50;
  do
  {
    base = ((unsigned char *) calcUPGet(unit)) + (off + (i * 2));
    wpn = *((short *) (base + 0xE));
    if ((wpn != 0) && ((*((signed char *) (dataTecGet(*((short *) (dataWpnGet(wpn) + 0x14))) + 0x11))) != 6))
    {
      dataUnitFileLoadWep(unit, wpn, i, file);
    }
    i += 1;
  }
  while (i < 3);
  dataUnitFileLoadMot(unit, mode, 0, file);
  i = 0;
  do
  {
    wpn = calcUPGet(unit)->sefSetupParams[i];
    if (((wpn != 0) && ((*((short *) (dataWpnGet(wpn) + 0xA))) == 1)) && ((*((signed char *) (dataTecGet(*((short *) (dataWpnGet(wpn) + 0x14))) + 0x11))) == 4))
    {
      dataUnitFileLoadMot(unit, *((short *) (dataTecGet(*((short *) (dataWpnGet(wpn) + 0x14))) + 0x14)), i + 1, file);
    }
    i += 1;
  }
  while (i < 3);
  return 1;
}

extern const char D_00A45AC8[];

void *dataUnitFileGet(ObjectTask *unit, int charaId)
{
    UnitFileInfo *slot;
    int lo;
    int hi;
    int i;

    slot = 0;
    for (i = 0; i < 6; i++) {
        if (unitFileInfo[i].charaId == charaId) {
            slot = &unitFileInfo[i];
            slot->charaId = calcUPGet(unit)->charaId;
            break;
        }
    }
    if (slot == 0) {
        if (charaId < 0x21 || charaId >= 0xBB) {
            lo = 0;
            hi = 3;
        } else {
            lo = 3;
            hi = 6;
        }
        for (i = lo; i < hi; i++) {
            if (unitFileInfo[i].charaId == 0) {
                slot = &unitFileInfo[i];
                slot->charaId = calcUPGet(unit)->charaId;
                break;
            }
        }
        if (slot == 0) {
            printf(D_00A45AC8);
        }
    }
    return slot;
}

typedef struct { int id; char *name; } PackMdlEnt;
extern PackMdlEnt pcPack_0[];
extern int calcAccEquChk(ObjectTask *unit, int id);
extern int calcStatGet(ObjectTask *unit, int a, int b);
extern int MenuScenarioNoGet(void);

char *dataPackPcMdlNameGet(ObjectTask *unit)
{
    short temp_16;
    int temp_2;
    int var_5;
    char *var_18;

    if (*(unsigned int *)unit->work & 0x40) {
        goto block_8;
    }
    if (calcUPGet(unit)->flags & 0x40) {
        goto block_8;
    }
    if (calcUPGet(unit)->charaId < 0xBB) {
        goto block_16;
    }
    if (calcUPGet(unit)->charaId >= 0xC3) {
        goto block_16;
    }
block_8:
    if (calcUPGet(unit)->charaId >= 0xBB && calcUPGet(unit)->charaId < 0xC3) {
        goto block_16;
    }
    return 0;
block_16:
    var_18 = 0;
    temp_16 = calcUPGet(unit)->charaId;
    for (var_5 = 0; pcPack_0[var_5].id != 0; var_5++) {
        if (temp_16 == pcPack_0[var_5].id) {
            var_18 = pcPack_0[var_5].name;
            break;
        }
    }
    if (calcUPGet(unit)->charaId < 0xBB || calcUPGet(unit)->charaId >= 0xC3) {
        if (calcAccEquChk(unit, 0x2D) != 0) {
            if (temp_16 == 3) {
                var_18 = "shionm";
            } else if (temp_16 == 6) {
                var_18 = "momom";
            } else if (temp_16 == 1) {
                var_18 = "chaosm";
            } else if (temp_16 == 7) {
                var_18 = "jrm";
            }
        }
        if (temp_16 == 2 && MenuScenarioNoGet() < 0xA) {
            var_18 = "kosmos1";
        }
        if (calcStatGet(unit, 7, 2) != 0) {
            var_18 = "momoa";
        } else if (calcStatGet(unit, 7, 1) != 0) {
            var_18 = "momob";
        }
    }
    return var_18;
}

typedef struct { int id; char *name; } PackWpnEnt;
extern PackWpnEnt pcPack_1[];

char *dataPackWpnMdlNameGet(int id)
{
    char *name = 0;
    int i;
    for (i = 0; pcPack_1[i].id != 0; i++) {
        if (id == pcPack_1[i].id) {
            name = pcPack_1[i].name;
            break;
        }
    }
    return name;
}

/* Ported from upstream; local views keep this TU's existing types intact. */
typedef struct MdlLoadFile {
    int charaId;
    int modelCharaId; /* +0x04 */
    void *modelAdr; /* +0x08 */
    int modelSize; /* +0x0C */
    unsigned char unmodeled_10[4];
    void *animationAdr; /* +0x14 */
    int animationSize; /* +0x18 */
    unsigned char unmodeled_1c[4];
    void *textureAdr; /* +0x20 */
    int textureSize; /* +0x24 */
    unsigned char unmodeled_28[0x10C - 0x28];
    int faceCharaId; /* +0x10C */
} MdlLoadFile;
typedef struct MdlLoadActor {
    unsigned char unmodeled_00[0x8D0];
    void *modelAdr; /* +0x8D0 */
    void *animationAdr; /* +0x8D4 */
    void *textureAdr; /* +0x8D8 */
} MdlLoadActor;
typedef struct MdlLoadUnit {
    unsigned char unmodeled_00[0x10];
    int *work; /* +0x10: flags word first */
    MdlLoadActor *motionActor; /* +0x14 */
} MdlLoadUnit;
#define MDL_UP(unit) calcUPGet((ObjectTask *)(unit))

extern char mdlFileName[];
extern char mdlFileName2[];
extern char *pcNameBase;
extern char D_00A44898[];
extern const char D_00A45DD0[];
extern const char D_00A45DF8[];
extern const char D_00A45E00[];
extern const char D_00A45E08[];
extern const char D_00A45E10[];
extern const char D_00A45E38[];
extern char *dataPackPcMdlNameGet(ObjectTask *unit);
extern void RES_GetMdlFileName(char *name, int charaId);
extern int xglCdGetFileSize(const char *name);
extern int dataUnitFileLoadFaceMdl();
extern void ACT_initMotion();

int dataUnitFileLoadMdl(MdlLoadUnit *unit, int charaId, MdlLoadFile *file)
{
    MdlLoadActor *actor;
    char *packedName;

    actor = unit->motionActor;
    if (charaId != file->modelCharaId) {
        file->modelCharaId = charaId;
        if (((*unit->work & 0x40) == 0
             && (MDL_UP(unit)->flags & 0x40) == 0
             && (MDL_UP(unit)->charaId < 0xBB
                 || MDL_UP(unit)->charaId >= 0xC3))
            || (MDL_UP(unit)->charaId >= 0xBB
                && MDL_UP(unit)->charaId < 0xC3)) {
            if (dataLeaderCidGet() == charaId) {
                file->animationAdr = (void *)0x01070800;
                file->textureAdr = (void *)0x010B1000;
                printf(D_00A45DD0, charaId);
            } else {
                packedName = dataPackPcMdlNameGet((ObjectTask *)unit);
                strcpy(mdlFileName2, pcNameBase);
                strcat(mdlFileName2, packedName);
                strcat(mdlFileName2, D_00A44898);
                dataFileLoadNB(mdlFileName2, file->modelAdr);
                file->modelSize = ((xglCdGetFileSize(mdlFileName2) + 0x7FF) / 0x800) * 0x800;
                file->animationAdr = 0;
                file->textureAdr = 0;
            }
        } else {
            RES_GetMdlFileName(mdlFileName, charaId);
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45DF8);
            dataFileLoadNB(mdlFileName2, file->modelAdr);
            file->modelSize = ((xglCdGetFileSize(mdlFileName2) + 0x7FF) / 0x800) * 0x800;
            file->animationAdr = (unsigned char *)file->modelAdr + file->modelSize;
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45E00);
            dataFileLoadNB(mdlFileName2, file->animationAdr);
            file->animationSize = ((xglCdGetFileSize(mdlFileName2) + 0x7FF) / 0x800) * 0x800;
            file->textureAdr = (unsigned char *)file->animationAdr + file->animationSize;
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45E08);
            dataFileLoadNB(mdlFileName2, file->textureAdr);
            file->textureSize = ((xglCdGetFileSize(mdlFileName2) + 0x7FF) / 0x800) * 0x800;
        }
    } else {
        printf(D_00A45E10, charaId, file->modelAdr);
    }
    actor->modelAdr = file->modelAdr;
    actor->animationAdr = file->animationAdr;
    actor->textureAdr = file->textureAdr;
    ACT_initMotion(actor);
    printf(D_00A45E38, file->modelSize + file->animationSize + file->textureSize);
    if ((*unit->work & 0x40) == 0
        && (MDL_UP(unit)->flags & 0x40) == 0
        && (MDL_UP(unit)->charaId < 0xBB
            || MDL_UP(unit)->charaId >= 0xC3)) {
        dataUnitFileLoadFaceMdl(unit, charaId, file);
    } else {
        file->faceCharaId = 0;
    }
    return 1;
}

extern const char D_00A45E50[];

int dataUnitFileLoadFaceMdl(void *task, int cid, void *info) {
    char *p = (char *)info;
    char *a;

    if (cid != *(int *)(p + 0x10C)) {
        *(int *)(p + 0x10C) = cid;
        if (dataLeaderCidGet() == cid) {
            *(int *)(p + 0x110) = 0x010B4000;
            *(int *)(p + 0x128) = 0x010E6000;
        }
    } else {
        printf(D_00A45E50, cid, *(int *)(p + 0x110));
    }
    a = dataChildActorCreate(cid);
    *(int *)(a + 0x8D0) = *(int *)(p + 0x110);
    *(void **)((char *)task + 0x2C) = a;
    *(int *)(a + 0x8D4) = *(int *)(p + 0x14);
    *(int *)(a + 0x8D8) = *(int *)(p + 0x128);
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataUnitFileLoadMot);

INCLUDE_ASM("asm/nonmatchings/ov01/data_unit_org_get", dataUnitFileLoadWep);

/*
 * dataWpnLRChk shares dataUnitFileLoadWep's own (ObjectTask *, weaponId,
 * slot, fileInfo) argument list, passed through unchanged; it never reads
 * the unit argument and only reaches the loaded file's weapon-id field 0xA0
 * bytes into fileInfo.
 */
typedef struct WpnFileInfo {
    unsigned char unmodeled_00[0xA0];
    int weaponId; /* +0xA0 */
} WpnFileInfo;

int dataWpnLRChk(ObjectTask *unit, int weaponId, int slot, WpnFileInfo *fileInfo)
{
    /*
     * The original guards both checks with a bnel to a shared fail exit
     * (0x00a1c1b4, 0x00a1c1c0); a structured cascade of "if (...) return 0;"
     * statements folds the second guard into a branchless compare instead.
     */
    if (slot != 1) {
        goto fail;
    }
    if (weaponId != fileInfo->weaponId) {
        goto fail;
    }
    return 1;
fail:
    return 0;
}

/*
 * The engine's actor record ACT_create hands out (main/near_dir.h and
 * main/set_motion.h model the same 0xa70-byte record as their own TU-local
 * copy: +0x00 flags, +0x04 an update callback, +0x08 a draw callback).
 * dataChildActorCreate is this TU's only writer of that head.
 */
typedef struct ChildActor {
    int flags;                               /* +0x00 */
    void (*update)(struct ChildActor *self); /* +0x04 */
    void (*draw)(struct ChildActor *self);   /* +0x08 */
} ChildActor;

extern ChildActor *ACT_create(int parent, int callerId);
extern void ACT_initMotion(ChildActor *actor);
extern const char D_00A45F38[];

void *dataChildActorCreate(int callerId)
{
    ChildActor *actor;

    actor = ACT_create(-1, callerId);
    if (actor == 0) {
        printf(D_00A45F38);
        return 0;
    }
    ACT_initMotion(actor);
    actor->update = 0;
    actor->draw = 0;
    actor->flags = 0x208;
    return actor;
}

int dataUnitFileLoadMotSp(ObjectTask *unit, int motionId, int slot) {
    dataUnitFileLoadMot(unit, motionId, slot, dataUnitFileGet(unit, calcUPGet(unit)->charaId));
    return 1;
}

int dataUnitFileLoadMotSp2(UnitEquipInfo *unit, int motionId, int slot, int p3, int p4)
{
    EquipActor *actor;

    dataMotAdrSet(unit->motionTable, slot);
    if (p4 == 5) {
        actor = unit->equipActor[0];
        if (actor != 0) {
            actor->motionAdr = unit->motionTable->slot[slot];
        }
        actor = unit->equipActor[1];
        if (actor != 0) {
            actor->motionAdr = unit->motionTable->slot[slot];
        }
    } else if (p3 >= 0) {
        actor = unit->equipActor[p3 & 3];
        if (actor != 0) {
            actor->motionAdr = unit->motionTable->slot[slot];
        }
    }
    return 1;
}

extern char D_00A45F58[];
extern void dataUnitFileLoad2Sub(void *unit);
extern int rnd(int max);
extern void unitCmdSet(void *unit, int *cmd);

void dataUnitFileLoad2(void *unit)
{
    int cmd[8];

    printf(D_00A45F58, unit);
    dataUnitFileLoad2Sub(unit);
    cmd[0] = 0x25;
    if (!(**(int **)((char *)unit + 0x10) & 0x40)) {
        cmd[1] = 0;
    } else {
        cmd[1] = rnd(10);
    }
    unitCmdSet(unit, cmd);
}

extern void ACT_setHand(void *actor, int hand);
extern void ACT_setFace(void *actor, void *other, int flags);
extern void unitFaceMotSet(void *unit, int motionId);
extern void unitWpnMotSet(void *unit, int slot, int a, int b);
extern void *sefCreateEffect2(void *param);
extern void dataSndSeRegLoad2(void *unit);
extern char D_00A45F78[];
extern char D_00A45FA0[];
void dataPackWpnMdl2(EquipActor *actor);
void ACT_setArms(void *actor, void *other, int acc_id, int flags);
void ACT_allocMatrix(void *actor, int count);
void ACT_setModelWrapper(void *actor, int flags);
void ACT_updateDefault(void *actor);

void dataUnitFileLoad2Sub(void *unit)
{
  unsigned char sp[0x24];
  EquipActor *mot;
  EquipActor *face;
  EquipActor *actor;
  unsigned char *pack;
  void *fx;
  int i;
  int off;
  int tec;
  short wid;
  mot = *((EquipActor **) (((char *) unit) + 0x14));
  face = *((EquipActor **) (((char *) unit) + 0x2C));
  if ((*((EquipActor **) (((char *) unit) + 0x14)))->wpnMdl[1] == 0)
  {
    int *hdr;
    int t;
    pack = mot->wpnMdl[0];
    hdr = (int *) pack;
    t = hdr[2];
    mot->wpnMdl[0] = pack + t;
    t = hdr[3];
    mot->wpnMdl[1] = pack + t;
    t = hdr[4];
    mot->wpnMdl[2] = pack + t;
    if (face != 0)
    {
      face->wpnMdl[1] = pack + hdr[3];
      t = hdr[5];
      face->wpnMdl[0] = pack + t;
      t = hdr[6];
      do
      {
        face->wpnMdl[2] = pack + t;
      }
      while (0);
    }
    printf(D_00A45F78);
  }
  i = 0;
  off = 0x50;
  do
  {
    if (sp)
    {
    }
    wid = *((short *) ((((char *) calcUPGet(unit)) + (off + (i * 2))) + 0xE));
    actor = ((EquipActor **) (((char *) unit) + 0x1C))[i & 3];
    i += 1;
    if (wid != 0x3000)
    {
      if (actor != 0)
      {
        if (actor->wpnMdl[1] == 0)
        {
          dataPackWpnMdl2(actor);
          printf(D_00A45FA0);
        }
      }
    }
  }
  while (i < 3);
  ACT_allocMatrix(mot, -1);
  ACT_setModelWrapper(mot, 0);
  if (((!((*(*((int **) (((char *) unit) + 0x10)))) & 0x40)) && (!(calcUPGet(unit)->flags & 0x40))) && ((calcUPGet(unit)->charaId < 0xBB) || (calcUPGet(unit)->charaId >= 0xC3)))
  {
    short hand;
    int sel;
    hand = *((short *) (((char *) mot) + 0x86));
    if (((hand == 4) || (hand == 0x14)) || ((sel = 0, hand == 0x15)))
    {
      sel = 1;
    }
    ACT_setHand(mot, sel);
  }
  dataMotAdrSet((MotionAdrTable *) mot, 0);
  if (face != 0)
  {
    ACT_setFace(face, mot, 0);
    ACT_allocMatrix(face, -1);
    ACT_setModelWrapper(face, 0);
    *((void **) (((char *) face) + 0x4)) = (void *) ACT_updateDefault;
    *((int *) (((char *) face) + 0x8DC)) = ((MotionAdrTable *) mot)->slot[0];
    *((int *) face) = (*((int *) face)) & (~8);
    unitFaceMotSet(unit, 0x35);
  }
  for (i = 0; i < 3; i++)
  {
    wid = calcUPGet(unit)->sefSetupParams[i];
    actor = ((EquipActor **) (((char *) unit) + 0x1C))[i & 3];
    if (wid != 0)
    {
      tec = ((signed char *) dataTecGet(*((short *) (dataWpnGet(wid) + 0x14))))[0x11];
      if ((*((short *) (dataWpnGet(wid) + 0xA))) == 1)
      {
        if (tec == 4)
        {
          dataMotAdrSet((MotionAdrTable *) mot, i + 1);
        }
      }
      off = 0x5A;
      if ((dataWidGet(wid) != 0x3000) && (0 != actor))
      {
        ACT_setArms(actor, mot, ((signed char *) calcUPGet(unit))[off + i], 0);
        ACT_allocMatrix(actor, -1);
        ACT_setModelWrapper(actor, 0);
        *((void **) (((char *) actor) + 0x4)) = (void *) ACT_updateDefault;
        if ((tec != 6) && (wid != 0xC))
        {
          *((int *) actor) = (*((int *) actor)) & (~8);
        }
        if (!((*(*((int **) (((char *) unit) + 0x10)))) & 0x40))
        {
          if (calcUPGet(unit)->flags & 0x40)
          {
            *((int *) (((char *) actor) + 0x8DC)) = ((int *) (((char *) mot) + 0x8E0))[i];
            unitWpnMotSet(unit, i, 0, 5);
          }
          else
          {
            *((int *) (((char *) actor) + 0x8DC)) = ((MotionAdrTable *) mot)->slot[0];
            unitWpnMotSet(unit, i, 0, 0xFF);
          }
        }
      }
    }
  }

  if ((((!((*(*((int **) (((char *) unit) + 0x10)))) & 0x40)) && (!(calcUPGet(unit)->flags & 0x40))) && ((calcUPGet(unit)->charaId < 0xBB) || (calcUPGet(unit)->charaId >= 0xC3))) || ((calcUPGet(unit)->charaId >= 0x97) && (calcUPGet(unit)->charaId < 0xBB)))
  {
    if ((calcStatGet(unit, 7, 2) == 0) && (calcStatGet(unit, 7, 1) == 0))
    {
      memset(sp, 0, 0x24);
      *((short *) (sp + 0x14)) = 0x4001;
      *((void **) sp) = *((void **) (((char *) unit) + 0x14));
      *((short *) (sp + 0x10)) = calcUPGet(unit)->charaId;
      fx = sefCreateEffect2(sp);
      *((void **) (((char *) calcUPGet(unit)) + 0x158)) = fx;
      if (fx != 0)
      {
        *((int *) (((char *) fx) + 0xA8C)) = (*((int *) (((char *) fx) + 0xA8C))) | 0x100;
      }
    }
  }
  else
  {
    *((void **) (((char *) calcUPGet(unit)) + 0x158)) = 0;
  }
  dataSndSeRegLoad2(unit);
}

/*
 * A packed model or animation file: 8 bytes not recovered, then one byte
 * offset per packed entry, counted from the start of the header, the same
 * idiom dataFpkAdrGet below resolves for a single entry.
 */
typedef struct PackedFile {
    unsigned char unmodeled_00[8];
    int entryOffset[3]; /* +0x08 */
} PackedFile;

void dataPackWpnMdl2(EquipActor *actor)
{
    PackedFile *pack;
    int offset;

    pack = (PackedFile *)actor->wpnMdl[0];
    offset = pack->entryOffset[0];
    actor->wpnMdl[0] = (unsigned char *)pack + offset;
    offset = pack->entryOffset[1];
    actor->wpnMdl[1] = (unsigned char *)pack + offset;
    offset = pack->entryOffset[2];
    actor->wpnMdl[2] = (unsigned char *)pack + offset;
}

typedef struct MotionAdrEntry {
    unsigned char unmodeled_00[8];
    int data_offset;
    int extra_offset;
} MotionAdrEntry;

void dataMotAdrSet(MotionAdrTable *table, int motionId)
{
    MotionAdrEntry *e;
    int d;
    int x;
    int *slot;
    int off = motionId << 2;

    off += 0x8dc;
    slot = (int *)((int)table + off);
    e = (MotionAdrEntry *)*slot;
    d = e->data_offset;
    x = e->extra_offset;
    *slot = (int)e + d;
    ((int *)((motionId << 2) + (int)table))[0x40] = (int)e + x;
}

/*
 * dataFpkAdrGet resolves a packed file's data pointer: 8 bytes not
 * recovered, then the offset of the packed data from the start of the
 * header.
 */
typedef struct FpkHeader {
    unsigned char unmodeled_00[8];
    int data_offset;
} FpkHeader;

void *dataFpkAdrGet(FpkHeader *fpk)
{
    return (unsigned char *)fpk + fpk->data_offset;
}

typedef struct WepSpEntry {
    int tid;
    int unk4;
    int unk8;
} WepSpEntry;

extern WepSpEntry wepSpTbl[];

WepSpEntry *dataTid2WepSp(int tid)
{
    int i;

    for (i = 0; wepSpTbl[i].tid != 0; i++) {
        if (wepSpTbl[i].tid == tid) {
            return &wepSpTbl[i];
        }
    }
    return 0;
}

int dataUnitFileLoadWep(ObjectTask *, int, int, void *);

int dataFileLoadWepSp(ObjectTask *unit, int tid)
{
    WepSpEntry *ent;
    void *fileInfo;
    int wid;

    fileInfo = dataUnitFileGet(unit, calcUPGet(unit)->charaId);
    ent = dataTid2WepSp(tid);
    wid = 2;
    if (ent == 0) {
        if (((signed char *)dataTecGet(tid))[0x11] == 6) {
            wid = 1;
        } else {
            return 0;
        }
    }
    if (wid == 2) {
        calcUPGet(unit)->sefSetupParams[2] = ent->unk4;
        calcUPGet(unit)->unmodeled_54[8] = ent->unk8;
        dataUnitFileLoadWep(unit, ent->unk4, 2, fileInfo);
        return 1;
    } else if (wid == 1) {
        dataUnitFileLoadWep(unit, calcUPGet(unit)->sefSetupParams[1], 1, fileInfo);
        return 1;
    }
    return 0;
}

extern void ACT_setArms(void *actor, void *other, int acc_id, int flags);
extern void ACT_allocMatrix(void *actor, int count);
extern void ACT_setModelWrapper(void *actor, int flags);
extern void ACT_updateDefault(void *actor);

int dataFileLoadWepSp2(ObjectTask *unit, int tid)
{
    int slot;
    void *parent;
    short wid;
    EquipActor *actor;

    slot = 2;
    if (dataTid2WepSp(tid) == 0) {
        slot = 1;
        if (((signed char *)dataTecGet(tid))[0x11] != 6) {
            return 0;
        }
    }
    parent = ((UnitEquipInfo *)unit)->motionTable;
    wid = calcUPGet(unit)->sefSetupParams[slot];
    actor = ((UnitEquipInfo *)unit)->equipActor[slot];
    if (wid != 0) {
        if (dataWidGet(wid) == 0x3000) {
            return 1;
        }
        if (actor == 0) {
            return 1;
        }
        if (actor->wpnMdl[1] == 0) {
            dataPackWpnMdl2(actor);
        }
        if ((unsigned)(wid - 0xD) >= 4) {
            ACT_setArms(actor, parent, *(signed char *)(slot + (int)calcUPGet(unit) + 0x5A), 0);
        }
        ACT_allocMatrix(actor, -1);
        ACT_setModelWrapper(actor, 0);
        *(void **)((char *)actor + 4) = (void *)ACT_updateDefault;
    }
    return 1;
}

extern void ACT_dispose(void *);

int dataFileLoadWepSpEnd(ObjectTask *unit, int tid) {
    int slot;
    void **pp;
    int off;

    slot = 2;
    if (dataTid2WepSp(tid) == 0) {
        slot = 1;
        if (((signed char *)dataTecGet(tid))[0x11] != 6) {
            return 0;
        }
    }
    off = slot * 4 + 0x1C;
    pp = (void **)((char *)unit + off);
    if (*pp != 0) {
        ACT_dispose(*pp);
        *pp = 0;
    }
    if (slot == 2) {
        calcUPGet(unit)->sefSetupParams[2] = 0;
        calcUPGet(unit)->unmodeled_54[8] = 0;
    }
    return 1;
}

/* Callers pass the unit whose default weapon they want; the function
 * always returns 0 and ignores it. */
int dataDefWpnGet(void *unit)
{
    return 0;
}

extern const char D_00A45FC8[];

int dataMtdRead(void *unit, short trigger)
{
    unsigned char *act;
    unsigned char *base;
    unsigned char *rec;
    unsigned char *r;
    short *p;
    unsigned int off;
    int idx;
    int id;
    int i;
    unsigned int v;
    short cnt;
    int sk;
    short n;
    float ft;
    int frame;
    int key;
    int k;

    act = *(unsigned char **)((unsigned char *)unit + 0x14);
    idx = (*(unsigned short *)(act + 0x704) & 0x700) >> 8;
    id = (*(unsigned short *)(act + 0x704) & 0xFF) + 1;
    if (idx >= 5) {
        return 0;
    }
    base = *(unsigned char **)(act + 0x100 + idx * 4);
    if (base == 0) {
        return 0;
    }
    off = *(short *)base;
    p = (short *)base + (off >> 1);
    rec = 0;
    for (i = 0; (sk = p[i]) != 0; i += 2) {
        if (sk == id) {
            v = p[i + 1];
            rec = base + (((unsigned int)v >> 1) << 1);
            break;
        }
    }
    if (rec == 0) {
        return 0;
    }
    r = rec;
    key = *(unsigned short *)r;
    ft = *(float *)(act + 0x6F4) / 0.033333335f + 0.5f;
    frame = (int)ft;
    k = key;
    while (k != 0) {
        {
            if ((short)k == trigger) {

                for (i = 0; i < *(short *)(r + 2); i++) {
                    idx = *(short *)(r + 4 + (i << 1));
                    if (idx == frame) {
                        printf(D_00A45FC8, (short)key);
                        return 1;
                    }
                }
                return 2;
            }
            n = *(short *)(r + 2);
            rec = (unsigned char *)((short *)rec + n + 2);
            r = rec;
            key = k = *(unsigned short *)r;
        }
    }
    return 0;
}

typedef struct XtxFileEntry {
    const char *name;
    unsigned int address;
} XtxFileEntry;

extern XtxFileEntry xtxFile[];

void *dataXtxFileGet(int index)
{
    return &xtxFile[index];
}

extern const char D_00A45FE8[];
extern unsigned short xtxAdr[0x13];

unsigned short dataXtxAdrGet(int index)
{
    unsigned short value;

    value = 0;
    if (index < 0x13) {
        if (index >= 0) {
            value = xtxAdr[index];
        } else {
            goto fail;
        }
    } else {
    fail:
        printf(D_00A45FE8, index);
    }
    return value;
}

typedef struct { char *name; void *adr; } XtxLoadEnt;

extern int dataFileLoadNB(const char *name, void *dst);

void dataXtxLoad(void)
{
    int i = 0;

    while (((XtxLoadEnt *)xtxFile)[i].name != 0) {
        ((int (*)(void *, void *))dataFileLoadNB)(((XtxLoadEnt *)xtxFile)[i].name, ((XtxLoadEnt *)xtxFile)[i].adr);
        i++;
    }
}

extern int dataFileLoad(const char *name, void *dst);

extern const char D_00A46008[];
extern unsigned char batDatBuf[0x10000];

void dataBatDatLoad(void)
{
    dataFileLoad(D_00A46008, batDatBuf);
}

extern unsigned char thinkBuf[0x4000];

void *dataThinkAdrGet(void)
{
    return thinkBuf;
}

int thinkMapGet(int id)
{
    int i;
    for (i = 0; i < 100; i++) {
        if (id == ((int *)&thinkMapTbl)[i]) {
            return i;
        }
    }
    return -1;
}

extern int thinkMapGet(int mapNo);
extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern int sprintf(char *buffer, const char *format, ...);
extern char *thinkName;
extern char *thinkNameBase;
extern char *thinkNameExt;
extern const char D_00A46028[];

int dataThinkNameGet(char *name, int thinkNo)
{
    char num[16];
    int mappedNo;

    mappedNo = thinkMapGet(thinkNo);
    if (mappedNo != -1) {
        strcpy(name, thinkNameBase);
        strcat(name, thinkName);
        sprintf(num, D_00A46028, mappedNo);
        strcat(name, num);
        strcat(name, thinkNameExt);
        return 1;
    }
    name[0] = '\0';
    return 0;
}

extern void thinkNoSet(int);

int dataThinkDataSet(int thinkNo, const void *data)
{
    int mappedNo;

    mappedNo = thinkMapGet(thinkNo);
    if (mappedNo != -1 && data != 0) {
        __builtin_memcpy(thinkBuf, data, sizeof(thinkBuf));
        thinkNoSet(mappedNo);
        return 1;
    }
    return 0;
}

extern void thinkNoSet(int);

int dataThinkFileLoad(int thinkNo)
{
    int no = thinkMapGet(thinkNo);
    if (no == -1)
        return 0;
    dataThinkLoad(no);
    thinkNoSet(no);
    return 1;
}

extern char *thinkName;
extern char *thinkNameBase;
extern char *thinkNameExt;
extern const char D_00A46028[];

void dataThinkLoad(int thinkNo)
{
    char path[256];
    char num[16];

    strcpy(path, thinkNameBase);
    strcat(path, thinkName);
    sprintf(num, D_00A46028, thinkNo);
    strcat(path, num);
    strcat(path, thinkNameExt);
    dataFileLoad(path, thinkBuf);
}

/* dataMapLoad passes the map number it is loading; every map loads into
 * the same fixed EE RAM region, so the number is ignored. */
void *dataMapLoadAdrGet(int mapNo)
{
    return (void *)0x01b9e000;
}

typedef struct MapHeader {
    unsigned char unmodeled_00[0x10];
    int cameraOffset; /* +0x10 */
} MapHeader;

void *dataMapCameraAdrGet(int mapNo)
{
    MapHeader *map = dataMapLoadAdrGet(mapNo);
    return (unsigned char *)map + map->cameraOffset;
}

extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern int sprintf(char *buffer, const char *format, ...);
extern char *mapNameBase;
extern char *mapName;
extern char *mapNameExt;
extern const char D_00A46028[];
extern const char D_00A46030[];
void dataMapLoad(int mapNo)
{
    char path[256];
    char num[16];

    if (mapNo < 100) {
        strcpy(path, mapNameBase);
        strcat(path, mapName);
        sprintf(num, D_00A46028, mapNo);
        strcat(path, num);
        strcat(path, mapNameExt);
        dataFileLoadNB(path, dataMapLoadAdrGet(mapNo));
        return;
    }
    printf(D_00A46030, mapNo);
}

void dataVPadModeSet(int mode)
{
    padMode = mode;
    padData = 0;
}

extern u16 padConvTbl[];

void dataVPadSet(u16 pad)
{
    int i;
    u16 r = 0;
    for (i = 0; i < 14; i++) {
        if ((pad >> i) & 1) {
            r |= padConvTbl[i];
        }
    }
    padData = r;
}

extern int D_426F4C;
extern const char D_00A458C0[];
extern const char D_00A458D8[];
extern int D_426F5C;
extern const char D_00A45980[];
extern int D_426F70;
typedef struct WpnLRFileInfo {
    unsigned char unmodeled_00[0xA0];
    int wpnId; /* +0xA0 */
} WpnLRFileInfo;
extern char D_00A45F58[];
extern void dataUnitFileLoad2Sub(void *unit);
extern int rnd(int max);
extern void unitCmdSet(void *unit, int *cmd);
extern void thinkNoSet(int);
extern u16 padConvTbl[];
extern unsigned short PadData[];

unsigned short dataPadRead(int arg0) {
    unsigned short r = 0;
    if (padMode == 0) {
        switch (arg0) {
        case 0: r = PadData[0x18]; break;
        case 1: r = PadData[0x19]; break;
        case 2: r = PadData[0x1a]; break;
        }
    } else {
        r = padData;
    }
    return r;
}
